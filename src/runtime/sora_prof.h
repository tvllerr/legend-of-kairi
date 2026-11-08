// (s81 fps hunt) Host sampling profiler, in the DLL (hot reload, no debugger, no hw watchpoints). Live command
// "prof <ms> <tag>": a thread suspends every other thread of the game ~1000x a second (SuspendThread +
// GetThreadContext + a short RtlVirtualUnwind walk + ResumeThread), keeps samples with a frame in swgame.dll or
// savagewind.exe, and buckets each by g_prof_cc (enemies on the linkpos_enemy_vel controller this frame).
// The report (D:/SoraBotW/play/logs/prof_<tag>.txt) lists per thread the % of ticks per function, leaf and
// inclusive, for cc 0 vs cc > 0, sorted by the difference. swgame code = f_<guest addr> (gamelib address tables),
// the exe = exe+<rva> (objdump -p exports), others = module+rva.
#ifdef _WIN32
#include <tlhelp32.h>
#include "gamelib.h"
#endif

static volatile int g_prof_cc;  // (set every driver call)
#ifndef _WIN32  // (Deck Linux port: the sampler is Windows only; perf on the Deck instead)
static void prof_start(const char *args) { (void)args; LOG("[sora] prof: Windows only (use perf on Linux)\n"); }
#else

#define PROF_DEPTH 16
#define PROF_MAX 400000
typedef struct { uint32_t tid; uint8_t cc, n; uint64_t pc[PROF_DEPTH]; } ProfSample;
typedef struct { uint64_t host, guest; } ProfFn;
static ProfSample *g_ps;
static int g_psn;
static ProfFn *g_pf;
static size_t g_pfn;
static uint64_t g_sw_lo, g_sw_hi, g_exe_lo, g_exe_hi;
static long g_ticks[2];
static volatile LONG g_prof_busy;
static struct { int ms; char tag[48]; } g_prof_arg;

static int prof_fn_cmp(const void *a, const void *b) {
    uint64_t x = ((const ProfFn *)a)->host, y = ((const ProfFn *)b)->host;
    return x < y ? -1 : x > y;
}
static void prof_mod_range(const char *name, uint64_t *lo, uint64_t *hi) {
    HMODULE m = GetModuleHandleA(name);
    MODULEINFO mi;
    if (m && GetModuleInformation(GetCurrentProcess(), m, &mi, sizeof mi))
        *lo = (uint64_t)mi.lpBaseOfDll, *hi = *lo + mi.SizeOfImage;
}
static bool prof_table(void) {
    if (g_pf) return true;
    const GameLibTables *t = gamelib_tables();
    if (!t) return false;
    size_t n = 0;
    for (uint32_t i = 0; i < *t->nmodules; i++) n += *t->modules[i].count;
    g_pf = malloc(n * sizeof *g_pf);
    if (!g_pf) return false;
    for (uint32_t i = 0; i < *t->nmodules; i++)
        for (uint64_t k = 0; k < *t->modules[i].count; k++)
            g_pf[g_pfn++] = (ProfFn){(uint64_t)t->modules[i].fns[k], t->modules[i].addrs[k]};
    qsort(g_pf, g_pfn, sizeof *g_pf, prof_fn_cmp);
    prof_mod_range("swgame.dll", &g_sw_lo, &g_sw_hi);
    prof_mod_range(NULL, &g_exe_lo, &g_exe_hi);
    return true;
}
static int prof_in(uint64_t pc) { return (pc >= g_sw_lo && pc < g_sw_hi) || (pc >= g_exe_lo && pc < g_exe_hi); }
// A pc -> a key for counting: swgame = the guest address with bit 63 set; else the pc's module base + rva.
static uint64_t prof_key(uint64_t pc) {
    if (pc >= g_sw_lo && pc < g_sw_hi) {
        size_t lo = 0, hi = g_pfn;
        while (hi - lo > 1) { size_t m = (lo + hi) / 2; if (g_pf[m].host <= pc) lo = m; else hi = m; }
        if (g_pfn && g_pf[lo].host <= pc) return g_pf[lo].guest | 1ull << 63;
    }
    return pc;
}
static void prof_name(uint64_t key, char *o, size_t on) {
    if (key >> 63) { snprintf(o, on, "f_%llx", (unsigned long long)(key & ~(1ull << 63))); return; }
    if (key >= g_exe_lo && key < g_exe_hi) { snprintf(o, on, "exe+%llx", (unsigned long long)(key - g_exe_lo)); return; }
    HMODULE m = NULL;
    char nm[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)key, &m) && m) {
        GetModuleBaseNameA(GetCurrentProcess(), m, nm, sizeof nm);
        snprintf(o, on, "%s+%llx", nm, (unsigned long long)(key - (uint64_t)m));
    } else snprintf(o, on, "%llx", (unsigned long long)key);
}

static void prof_sample(HANDLE th, DWORD tid) {
    CONTEXT c;
    c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (SuspendThread(th) == (DWORD)-1) return;
    ProfSample s = {tid, g_prof_cc > 0, 0, {0}};
    if (GetThreadContext(th, &c)) {
        // (walks only through code with unwind info; a pc outside any (JIT/thunk) ends the walk)
        for (int d = 0; d < PROF_DEPTH && c.Rip; d++) {
            s.pc[s.n++] = c.Rip;
            DWORD64 base;
            PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(c.Rip, &base, NULL);
            if (!rf) break;
            void *hd;
            DWORD64 ef;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c.Rip, rf, &c, &hd, &ef, NULL);
        }
    }
    ResumeThread(th);
    bool keep = false;
    for (int i = 0; i < s.n && !keep; i++) keep = prof_in(s.pc[i]);
    if (keep && g_psn < PROF_MAX) g_ps[g_psn++] = s;
}

typedef struct { uint32_t tid; uint64_t key; long n[2]; } ProfCount;
static int prof_cnt_cmp_tk(const void *a, const void *b) {
    const ProfCount *x = a, *y = b;
    if (x->tid != y->tid) return x->tid < y->tid ? -1 : 1;
    return x->key < y->key ? -1 : x->key > y->key;
}
static double g_prof_sort_t[2];
static int prof_cnt_cmp_diff(const void *a, const void *b) {
    const ProfCount *x = a, *y = b;
    double dx = x->n[1] / g_prof_sort_t[1] - x->n[0] / g_prof_sort_t[0];
    double dy = y->n[1] / g_prof_sort_t[1] - y->n[0] / g_prof_sort_t[0];
    return dx < dy ? 1 : dx > dy ? -1 : 0;
}
// Counts (tid, key) over the samples; incl: every distinct key on the stack, else the leaf.
static size_t prof_count(ProfCount *o, size_t cap, bool incl) {
    size_t n = 0;
    for (int i = 0; i < g_psn; i++) {
        ProfSample *s = &g_ps[i];
        uint64_t seen[PROF_DEPTH];
        int ns = 0;
        for (int d = 0; d < (incl ? s->n : 1); d++) {
            uint64_t k = prof_key(s->pc[d]);
            bool dup = false;
            for (int j = 0; j < ns; j++) dup |= seen[j] == k;
            if (dup) continue;
            seen[ns++] = k;
            if (n < cap) o[n++] = (ProfCount){s->tid, k, {s->cc == 0, s->cc != 0}};
        }
    }
    qsort(o, n, sizeof *o, prof_cnt_cmp_tk);
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        if (m && o[m - 1].tid == o[i].tid && o[m - 1].key == o[i].key) o[m - 1].n[0] += o[i].n[0], o[m - 1].n[1] += o[i].n[1];
        else o[m++] = o[i];
    }
    return m;
}
static void prof_report(FILE *f, bool incl) {
    size_t cap = (size_t)g_psn * (incl ? PROF_DEPTH : 1) + 1;
    ProfCount *c = malloc(cap * sizeof *c);
    if (!c) return;
    size_t n = prof_count(c, cap, incl);
    // threads: total samples per tid (the leaf count = one per sample)
    for (size_t i = 0; i < n;) {
        size_t j = i;
        long tot[2] = {0, 0};
        while (j < n && c[j].tid == c[i].tid) {
            if (!incl) tot[0] += c[j].n[0], tot[1] += c[j].n[1];
            j++;
        }
        g_prof_sort_t[0] = g_ticks[0] ? g_ticks[0] : 1, g_prof_sort_t[1] = g_ticks[1] ? g_ticks[1] : 1;
        qsort(c + i, j - i, sizeof *c, prof_cnt_cmp_diff);
        fprintf(f, "\n== thread %u %s", c[i].tid, incl ? "INCLUSIVE" : "LEAF");
        if (!incl) fprintf(f, " (busy: cc0 %.1f%%  cc1 %.1f%% of ticks)", 100.0 * tot[0] / g_prof_sort_t[0], 100.0 * tot[1] / g_prof_sort_t[1]);
        fprintf(f, "\n   cc0%%   cc1%%   diff  function\n");
        for (size_t k = i; k < j && k < i + 40; k++) {
            char nm[96];
            prof_name(c[k].key, nm, sizeof nm);
            double a = 100.0 * c[k].n[0] / g_prof_sort_t[0], b = 100.0 * c[k].n[1] / g_prof_sort_t[1];
            if (b - a < 0.5 && k > i + 10) break;
            fprintf(f, "  %5.1f  %5.1f  %+5.1f  %s\n", a, b, b - a, nm);
        }
        i = j;
    }
    free(c);
}

static DWORD WINAPI prof_thread(void *arg) {
    (void)arg;
    DWORD me = GetCurrentThreadId(), pid = GetCurrentProcessId();
    DWORD tids[256];
    HANDLE ths[256];
    int nt = 0;
    ULONGLONG t0 = GetTickCount64(), tl = 0;
    g_psn = 0, g_ticks[0] = g_ticks[1] = 0;
    while (GetTickCount64() - t0 < (ULONGLONG)g_prof_arg.ms) {
        ULONGLONG now = GetTickCount64();
        if (now - tl > 500) {  // (re-list twice a second: only threads that used >= 15% of a core since the last list)
            for (int i = 0; i < nt; i++) CloseHandle(ths[i]);
            nt = 0;
            static DWORD ptid[1024];
            static ULONGLONG pcpu[1024];
            static int np;
            DWORD ntid[1024];
            ULONGLONG ncpu[1024];
            int nn = 0;
            HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            THREADENTRY32 te = {sizeof te};
            for (BOOL ok = Thread32First(sn, &te); ok && nn < 1024; ok = Thread32Next(sn, &te)) {
                if (te.th32OwnerProcessID != pid || te.th32ThreadID == me) continue;
                HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
                if (!h) continue;
                FILETIME c0, e0, k0, u0;
                ULONGLONG cpu = 0;
                if (GetThreadTimes(h, &c0, &e0, &k0, &u0))
                    cpu = (((ULONGLONG)k0.dwHighDateTime << 32 | k0.dwLowDateTime) + ((ULONGLONG)u0.dwHighDateTime << 32 | u0.dwLowDateTime)) / 10000;
                ntid[nn] = te.th32ThreadID, ncpu[nn++] = cpu;
                bool busy = false;
                for (int j = 0; j < np; j++)
                    if (ptid[j] == te.th32ThreadID) busy = tl && cpu - pcpu[j] >= (now - tl) * 15 / 100;
                if (busy && nt < 256) tids[nt] = te.th32ThreadID, ths[nt++] = h;
                else CloseHandle(h);
            }
            CloseHandle(sn);
            memcpy(ptid, ntid, nn * sizeof *ntid), memcpy(pcpu, ncpu, nn * sizeof *ncpu), np = nn;
            if (!tl) LOG("[sora] prof: %d threads\n", nn);
            tl = now;
        }
        g_ticks[g_prof_cc > 0]++;
        for (int i = 0; i < nt; i++) prof_sample(ths[i], tids[i]);
        Sleep(1);
    }
    for (int i = 0; i < nt; i++) CloseHandle(ths[i]);
    char path[700];
    snprintf(path, sizeof path, "%s/play/logs/prof_%s.txt", sora_root(), g_prof_arg.tag);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "prof %s: %d ms, %d samples, ticks cc0 %ld cc1 %ld (%%s = %% of ticks in that bucket)\n",
                g_prof_arg.tag, g_prof_arg.ms, g_psn, g_ticks[0], g_ticks[1]);
        prof_report(f, false);
        prof_report(f, true);
        fclose(f);
    }
    LOG("[sora] prof %s done: %d samples, ticks cc0 %ld cc1 %ld -> %s\n", g_prof_arg.tag, g_psn, g_ticks[0], g_ticks[1], path);
    InterlockedExchange(&g_prof_busy, 0);
    return 0;
}
static void prof_start(const char *args) {
    int ms = 0;
    char tag[48] = "x";
    if (sscanf(args, "%d %47s", &ms, tag) < 1 || ms <= 0) { LOG("[sora] prof: usage prof <ms> <tag>\n"); return; }
    if (InterlockedCompareExchange(&g_prof_busy, 1, 0)) { LOG("[sora] prof: already running\n"); return; }
    if (!g_ps) g_ps = malloc(PROF_MAX * sizeof *g_ps);
    if (!g_ps || !prof_table()) { LOG("[sora] prof: no memory / no gamelib tables\n"); g_prof_busy = 0; return; }
    g_prof_arg.ms = ms;
    snprintf(g_prof_arg.tag, sizeof g_prof_arg.tag, "%s", tag);
    LOG("[sora] prof %s: %d ms, %zu guest fns, swgame %llx-%llx\n", tag, ms, g_pfn, (unsigned long long)g_sw_lo,
        (unsigned long long)g_sw_hi);
    HANDLE h = CreateThread(NULL, 0, prof_thread, NULL, 0, NULL);
    if (h) CloseHandle(h); else g_prof_busy = 0;
}
#endif  // _WIN32

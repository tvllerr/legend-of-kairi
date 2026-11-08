// s182 sampling profiler for the R2 puppet attack runner (included by sora_esa.h, before EsaCtx). DLL only, read only: while a timed
// calc_ / enter_ runs, ONE sampler thread (started lazily, `esynclog` on and `set esaprof` not 0) suspends that guest thread every ~1 ms
// (SuspendThread, GetThreadContext, ResumeThread at once; nothing allocated or logged while it is suspended), then walks <= 8 frames of a
// COPY of the context (RtlLookupFunctionEntry + RtlVirtualUnwind). When the call turns out slow (> ESF_SLOW_MS) its samples go into the
// 10 s aggregate, else they are dropped. Report (from the sampler thread, every 10 s with slow calls):
//   [esa] prof 10 s: N slow calls (> 20 ms) of T timed, M samples
//   [esa] prof top rips: 31% f_7100aa79f0+1a4 | 12% ntdll+0x... | ...
//   [esa] prof callers (first frame in swgame.dll / savagewind.exe): ...
//   [esa] prof modules: swgame 70% | exe 5% | ntdll/kernel32/kernelbase 20% | mod dll 0% | other 5%
// Names: swgame.dll code = f_<guest addr>+<host offset> (gamelib tables, like sora_prof.h), the exe = exe+<rva>, others = module+0xrva.
// Stop: esf_stop() (called first thing in esa_unload, i.e. sora_mod_unload) sets the stop flag and JOINS the thread before the DLL goes.
#pragma once
#if defined(_WIN32) && !defined(ESA_NO_EXE)
#include <tlhelp32.h>
#include <psapi.h>
#include "gamelib.h"
#define ESF_ON 1
#else
#define ESF_ON 0
#endif

#define ESF_SLOW_MS 20.0
#define ESF_SLOTS 8
#define ESF_MAXS 1024    // samples kept per call (1 s at 1 ms)
#define ESF_DEPTH 8
#define ESF_HT 2048      // aggregate hash size
static volatile int g_esf_on = 1;  // `set esaprof 0|1` (also needs esynclog)

#if ESF_ON
enum { ESF_OTHER, ESF_EXE, ESF_SWGAME, ESF_MOD, ESF_NT };
typedef struct { uint64_t rip, caller, c2, c3; uint8_t cat; } EsfS;  // (s182b: c2 / c3 = the next 2 game frames, return addresses)
typedef struct { volatile LONG tid; HANDLE volatile h; SRWLOCK lk; volatile LONG active; uint32_t n; uint64_t stk_lo, stk_hi; EsfS s[ESF_MAXS]; } EsfSlot;
typedef struct { uint64_t key; uint32_t n; } EsfH;
static struct {
    EsfSlot slot[ESF_SLOTS];
    SRWLOCK agg_lk;
    EsfH leaf[ESF_HT], call[ESF_HT], call2[ESF_HT], call3[ESF_HT];
    uint32_t cat[5], samples, slow_calls, timed_calls;
    SRWLOCK start_lk;
    HANDLE th;
    volatile LONG stop, dead;
    uint64_t sw_lo, sw_hi, exe_lo, exe_hi, mod_lo, mod_hi, nt_lo[3], nt_hi[3];
    struct { uint64_t host, guest; } *fn;
    size_t nfn;
} g_esf;

static void esf_range(const char *name, uint64_t *lo, uint64_t *hi) {
    HMODULE m = GetModuleHandleA(name);
    MODULEINFO mi;
    if (m && GetModuleInformation(GetCurrentProcess(), m, &mi, sizeof mi)) *lo = (uint64_t)(uintptr_t)mi.lpBaseOfDll, *hi = *lo + mi.SizeOfImage;
}
static int esf_cat(uint64_t pc) {
    if (pc >= g_esf.sw_lo && pc < g_esf.sw_hi) return ESF_SWGAME;
    if (pc >= g_esf.exe_lo && pc < g_esf.exe_hi) return ESF_EXE;
    if (pc >= g_esf.mod_lo && pc < g_esf.mod_hi) return ESF_MOD;
    for (int i = 0; i < 3; i++) if (pc >= g_esf.nt_lo[i] && pc < g_esf.nt_hi[i]) return ESF_NT;
    return ESF_OTHER;
}
static void esf_hadd(EsfH *t, uint64_t key, uint32_t n) {
    uint32_t i = (uint32_t)(((key * 0x9E3779B97F4A7C15ull) >> 40) % ESF_HT);
    for (int k = 0; k < ESF_HT; k++, i = (i + 1) % ESF_HT)
        if (t[i].key == key || !t[i].key) { t[i].key = key, t[i].n += n; return; }
}
// one sample of a suspended-then-resumed thread (slot lock held by the sampler)
static void esf_sample(EsfSlot *sl) {
    CONTEXT c;
    memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    HANDLE h = sl->h;
    if (!h || SuspendThread(h) == (DWORD)-1) return;
    BOOL ok = GetThreadContext(h, &c);
    ResumeThread(h);  // (from here on nothing of the target is held)
    if (!ok || sl->n >= ESF_MAXS) return;
    EsfS s = {c.Rip, 0, 0, 0, (uint8_t)esf_cat(c.Rip)};
    uint64_t first = 0;
    for (int d = 0; d < ESF_DEPTH + 4 && c.Rip; d++) {
        DWORD64 base = 0;
        PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(c.Rip, &base, NULL);
        int ct = esf_cat(c.Rip);
        if (ct == ESF_SWGAME || ct == ESF_EXE) {  // (s182b: keep walking 2 more game frames: who called readable()?)
            if (!first) first = rf ? base + rf->BeginAddress : c.Rip;
            else if (!s.c2) s.c2 = c.Rip;
            else { s.c3 = c.Rip; break; }
            if (!rf) break;
        }
        if (d && (c.Rsp < sl->stk_lo || c.Rsp + 0x400 > sl->stk_hi)) break;  // (s182b: the target runs again: a stale frame must never send the unwind off its own stack)
        if (!rf) {  // (leaf without unwind data: return address at [rsp]; only on the very first frame)
            if (d || !c.Rsp || IsBadReadPtr((void *)(uintptr_t)c.Rsp, 8)) break;
            c.Rip = *(uint64_t *)(uintptr_t)c.Rsp, c.Rsp += 8;
            continue;
        }
        void *hd;
        DWORD64 ef;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c.Rip, rf, &c, &hd, &ef, NULL);  // (a COPY of the context; racy stack reads are only wrong frames)
    }
    s.caller = first;
    sl->s[sl->n++] = s;
}
static int esf_fn_cmp(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}
static void esf_table(void) {
    if (g_esf.fn) return;
    const GameLibTables *t = gamelib_tables();
    if (!t) return;
    size_t n = 0;
    for (uint32_t i = 0; i < *t->nmodules; i++) n += *t->modules[i].count;
    g_esf.fn = malloc(n * sizeof *g_esf.fn);
    if (!g_esf.fn) return;
    for (uint32_t i = 0; i < *t->nmodules; i++)
        for (uint64_t k = 0; k < *t->modules[i].count; k++) {
            g_esf.fn[g_esf.nfn].host = (uint64_t)(uintptr_t)t->modules[i].fns[k];
            g_esf.fn[g_esf.nfn++].guest = t->modules[i].addrs[k];
        }
    qsort(g_esf.fn, g_esf.nfn, sizeof *g_esf.fn, esf_fn_cmp);  // (sorted by host)
}
static void esf_name(uint64_t pc, char *o, size_t on) {
    if (pc >= g_esf.sw_lo && pc < g_esf.sw_hi && g_esf.nfn) {
        size_t lo = 0, hi = g_esf.nfn;
        while (hi - lo > 1) { size_t m = (lo + hi) / 2; if (g_esf.fn[m].host <= pc) lo = m; else hi = m; }
        if (g_esf.fn[lo].host <= pc) { snprintf(o, on, "f_%llx+%llx", (unsigned long long)g_esf.fn[lo].guest, (unsigned long long)(pc - g_esf.fn[lo].host)); return; }
    }
    if (pc >= g_esf.exe_lo && pc < g_esf.exe_hi) { snprintf(o, on, "exe+%llx", (unsigned long long)(pc - g_esf.exe_lo)); return; }
    HMODULE m = NULL;
    char nm[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)(uintptr_t)pc, &m) && m) {
        GetModuleBaseNameA(GetCurrentProcess(), m, nm, sizeof nm);
        snprintf(o, on, "%s+0x%llx", nm, (unsigned long long)(pc - (uint64_t)(uintptr_t)m));
    } else snprintf(o, on, "%llx", (unsigned long long)pc);
}
static int esf_h_cmp(const void *a, const void *b) {
    uint32_t x = ((const EsfH *)a)->n, y = ((const EsfH *)b)->n;
    return x < y ? 1 : x > y ? -1 : 0;
}
static void esf_top(const char *what, EsfH *t, uint32_t total, char *line, size_t ln) {
    qsort(t, ESF_HT, sizeof *t, esf_h_cmp);
    size_t o = (size_t)snprintf(line, ln, "[esa] prof %s:", what);
    for (int i = 0; i < 10 && t[i].n && o + 200 < ln; i++) {
        char nm[160];
        esf_name(t[i].key, nm, sizeof nm);
        o += (size_t)snprintf(line + o, ln - o, "%s %.0f%% %s", i ? " |" : "", 100.0 * t[i].n / (total ? total : 1), nm);
    }
    LOG("%s\n", line);
}
static void esf_report(void) {
    static EsfH leaf[ESF_HT], call[ESF_HT], call2[ESF_HT], call3[ESF_HT];
    static char line[2400];
    uint32_t cat[5], samples, slow, timed;
    while (!TryAcquireSRWLockExclusive(&g_esf.agg_lk)) Sleep(0);
    memcpy(leaf, g_esf.leaf, sizeof leaf), memcpy(call, g_esf.call, sizeof call), memcpy(call2, g_esf.call2, sizeof call2), memcpy(call3, g_esf.call3, sizeof call3), memcpy(cat, g_esf.cat, sizeof cat);
    samples = g_esf.samples, slow = g_esf.slow_calls, timed = g_esf.timed_calls;
    memset(g_esf.leaf, 0, sizeof g_esf.leaf), memset(g_esf.call, 0, sizeof g_esf.call), memset(g_esf.call2, 0, sizeof g_esf.call2), memset(g_esf.call3, 0, sizeof g_esf.call3), memset(g_esf.cat, 0, sizeof g_esf.cat);
    g_esf.samples = g_esf.slow_calls = g_esf.timed_calls = 0;
    ReleaseSRWLockExclusive(&g_esf.agg_lk);
    if (!slow) return;
    esf_table();
    LOG("[esa] prof 10 s: %u slow calls (> %.0f ms) of %u timed, %u samples\n", slow, ESF_SLOW_MS, timed, samples);
    esf_top("top rips", leaf, samples, line, sizeof line);
    esf_top("callers (first frame in swgame.dll / savagewind.exe)", call, samples, line, sizeof line);
    esf_top("caller 2 (next game frame, return address)", call2, samples, line, sizeof line);
    esf_top("caller 3", call3, samples, line, sizeof line);
    double d = samples ? 100.0 / samples : 0;
    LOG("[esa] prof modules: swgame %.0f%% | exe %.0f%% | ntdll/kernel32/kernelbase %.0f%% | mod dll %.0f%% | other %.0f%%\n", cat[ESF_SWGAME] * d, cat[ESF_EXE] * d,
        cat[ESF_NT] * d, cat[ESF_MOD] * d, cat[ESF_OTHER] * d);
}
static DWORD WINAPI esf_main(LPVOID arg) {
    (void)arg;
    ULONGLONG next = GetTickCount64() + 10000;
    while (!g_esf.stop && g_esf_on) {
        Sleep(1);
        for (int i = 0; i < ESF_SLOTS; i++) {
            EsfSlot *sl = &g_esf.slot[i];
            if (!sl->active || !TryAcquireSRWLockExclusive(&sl->lk)) continue;  // (never wait on a lock: the target may be the one holding it)
            if (sl->active) esf_sample(sl);
            ReleaseSRWLockExclusive(&sl->lk);
        }
        if (GetTickCount64() >= next) next += 10000, esf_report();
    }
    return 0;
}
static void esf_start(void) {
    AcquireSRWLockExclusive(&g_esf.start_lk);
    if (g_esf.th && WaitForSingleObject(g_esf.th, 0) == WAIT_OBJECT_0) CloseHandle(g_esf.th), g_esf.th = NULL;  // (finished by `set esaprof 0`)
    if (!g_esf.th && !g_esf.dead) {
        esf_range("swgame.dll", &g_esf.sw_lo, &g_esf.sw_hi);
        esf_range(NULL, &g_esf.exe_lo, &g_esf.exe_hi);
        HMODULE me = NULL;
        MODULEINFO mi;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)(void *)esf_start, &me) &&
            GetModuleInformation(GetCurrentProcess(), me, &mi, sizeof mi))
            g_esf.mod_lo = (uint64_t)(uintptr_t)mi.lpBaseOfDll, g_esf.mod_hi = g_esf.mod_lo + mi.SizeOfImage;
        esf_range("ntdll.dll", &g_esf.nt_lo[0], &g_esf.nt_hi[0]);
        esf_range("kernel32.dll", &g_esf.nt_lo[1], &g_esf.nt_hi[1]);
        esf_range("kernelbase.dll", &g_esf.nt_lo[2], &g_esf.nt_hi[2]);
        g_esf.stop = 0;
        g_esf.th = CreateThread(NULL, 0, esf_main, NULL, 0, NULL);
        if (g_esf.th) LOG("[esa] prof: sampler thread started (slow = calc_/enter_ over %.0f ms; report every 10 s)\n", ESF_SLOW_MS);
    }
    ReleaseSRWLockExclusive(&g_esf.start_lk);
}
// the calling thread's slot (claimed on first use; the handle is a real DuplicateHandle of the pseudo handle, kept until esf_stop)
static EsfSlot *esf_slot(void) {
    LONG me = (LONG)GetCurrentThreadId();
    for (int i = 0; i < ESF_SLOTS; i++) if (g_esf.slot[i].tid == me && g_esf.slot[i].h) return &g_esf.slot[i];
    for (int i = 0; i < ESF_SLOTS; i++) {
        EsfSlot *sl = &g_esf.slot[i];
        if (InterlockedCompareExchange(&sl->tid, me, 0) != 0) continue;
        HANDLE h = NULL;
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &h, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0)) {
            InterlockedExchange(&sl->tid, 0);
            return NULL;
        }
        sl->h = h;
        return sl;
    }
    return NULL;
}
// NULL = this call is not sampled
static EsfSlot *esf_begin(bool log_on) {
    if (!log_on || !g_esf_on || g_esf.dead || g_esf.stop) return NULL;
    if (!g_esf.th || WaitForSingleObject(g_esf.th, 0) == WAIT_OBJECT_0) esf_start();
    if (!g_esf.th) return NULL;
    EsfSlot *sl = esf_slot();
    if (!sl) return NULL;
    while (!TryAcquireSRWLockExclusive(&sl->lk)) Sleep(0);
    ULONG_PTR slo = 0, shi = 0;
    GetCurrentThreadStackLimits(&slo, &shi);  // (this IS the sampled thread; per call: fibers / guest stacks may differ)
    sl->stk_lo = slo, sl->stk_hi = shi;
    sl->n = 0, sl->active = 1;
    ReleaseSRWLockExclusive(&sl->lk);
    return sl;
}
static void esf_end(EsfSlot *sl, double ms) {
    if (!sl) return;
    while (!TryAcquireSRWLockExclusive(&sl->lk)) Sleep(0);
    sl->active = 0;
    while (!TryAcquireSRWLockExclusive(&g_esf.agg_lk)) Sleep(0);
    g_esf.timed_calls++;
    if (ms > ESF_SLOW_MS && sl->n) {
        g_esf.slow_calls++;
        for (uint32_t i = 0; i < sl->n; i++) {
            esf_hadd(g_esf.leaf, sl->s[i].rip, 1);
            if (sl->s[i].caller) esf_hadd(g_esf.call, sl->s[i].caller, 1);
            if (sl->s[i].c2) esf_hadd(g_esf.call2, sl->s[i].c2, 1);
            if (sl->s[i].c3) esf_hadd(g_esf.call3, sl->s[i].c3, 1);
            g_esf.cat[sl->s[i].cat]++;
        }
        g_esf.samples += sl->n;
    }
    ReleaseSRWLockExclusive(&g_esf.agg_lk);
    sl->n = 0;
    ReleaseSRWLockExclusive(&sl->lk);
}
// DLL unload / hot reload: stop and JOIN the sampler thread (it runs code of this DLL copy), then close the handles. Idempotent.
static void esf_stop(void) {
    g_esf.dead = 1;
    InterlockedExchange(&g_esf.stop, 1);
    AcquireSRWLockExclusive(&g_esf.start_lk);
    if (g_esf.th) {
        if (WaitForSingleObject(g_esf.th, 5000) != WAIT_OBJECT_0) LOG("[esa] prof: sampler thread did not stop in 5 s\n");
        else CloseHandle(g_esf.th), g_esf.th = NULL;
    }
    ReleaseSRWLockExclusive(&g_esf.start_lk);
    if (g_esf.th) return;  // (still running: leave the handles alone)
    for (int i = 0; i < ESF_SLOTS; i++) {
        EsfSlot *sl = &g_esf.slot[i];
        if (sl->h) CloseHandle(sl->h), sl->h = NULL;
        sl->active = 0, sl->tid = 0;
    }
}
#else
typedef struct EsfSlot EsfSlot;
static EsfSlot *esf_begin(bool log_on) { (void)log_on; return NULL; }
static void esf_end(EsfSlot *sl, double ms) { (void)sl; (void)ms; }
static void esf_stop(void) {}
#endif

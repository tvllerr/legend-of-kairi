// (Deck Linux port) The bigger Win32 shims of runtime/sora_win.h, Linux only (an empty file on Windows, left out of the
// Windows build by the Makefile). In the exe: sora_mod.so calls these same copies (-rdynamic).
#ifndef _WIN32
#include "sora_win.h"
#include <stdio.h>
#include <sys/mman.h>
#include "rt.h"  // (GUEST_LO, GUEST_HI)

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#define SW_PAGE 4096ull
#define SW_USER_END 0x800000000000ull
#define SW_GUEST_STEP (64ull << 20)  // (VirtualQuery in the guest window answers at most this much: 16 K mincore bytes)

void *sw_self_module(void) {
    static void *self;
    if (!self) self = dlopen(NULL, RTLD_LAZY);
    return self;
}

static DWORD prot_of(const char *perm) {  // "rwxp" -> PAGE_*
    bool r = perm[0] == 'r', w = perm[1] == 'w', x = perm[2] == 'x';
    if (!r && !w && !x) return PAGE_NOACCESS;
    if (x) return w ? PAGE_EXECUTE_READWRITE : r ? PAGE_EXECUTE_READ : PAGE_EXECUTE;
    return w ? PAGE_READWRITE : PAGE_READONLY;
}

// The /proc/self/maps line holding a, or the gap before the next one (*found false, hi = the next start).
static bool maps_find(uint64_t a, uint64_t *lo, uint64_t *hi, DWORD *prot, DWORD *type, bool *found) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return false;
    char line[512];
    uint64_t prev_end = 0;
    *found = false, *lo = 0, *hi = SW_USER_END;
    while (fgets(line, sizeof line, f)) {
        unsigned long long s, e, off, ino;
        char perm[8] = {0};
        unsigned dmaj, dmin;
        if (sscanf(line, "%llx-%llx %7s %llx %x:%x %llu", &s, &e, perm, &off, &dmaj, &dmin, &ino) < 7) continue;
        if (a < s) { *lo = prev_end, *hi = s; break; }  // (in the gap before this one)
        if (a < e) {
            *found = true, *lo = s, *hi = e, *prot = prot_of(perm);
            *type = !ino ? MEM_PRIVATE : perm[2] == 'x' ? MEM_IMAGE : MEM_MAPPED;
            break;
        }
        prev_end = e;
    }
    if (!*found && *hi == SW_USER_END) *lo = prev_end;
    fclose(f);
    return true;
}

// (Deck s9) Shared (file-backed) mappings inside the guest window. On RADV the Linux build maps the guest HEAP as an
// exported dma-buf (exec.c heap_export, HEAP_BASE, ~3.1 GB, MAP_SHARED|MAP_FIXED over the window's anon mapping).
// mincore knows nothing of dma-buf pages (no page cache: all "not resident") -> the heap looked MEM_RESERVE, every
// readable() on it failed and re-ran a 64 MB mincore (~1 ms, 3x a frame: servo_body never succeeded). All of a dma-buf
// is real memory from the start, so these ranges are MEM_COMMIT (and all "resident" for QueryWorkingSetEx). Found from
// /proc/self/maps at most once a second until one shows up (the heap is mapped once at Vulkan init and kept), giving
// up after SW_SH_TRIES (import mode: none).
#define SW_SH_MAX 8
#define SW_SH_TRIES 600
static pthread_mutex_t g_sh_mu = PTHREAD_MUTEX_INITIALIZER;
static uint64_t g_sh_lo[SW_SH_MAX], g_sh_hi[SW_SH_MAX];
static int g_sh_n, g_sh_tries;
static bool g_sh_done;
static uint64_t g_sh_next_ns;

static void shared_scan(void) {  // (g_sh_mu held)
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return;
    char line[512];
    int n = 0;
    while (n < SW_SH_MAX && fgets(line, sizeof line, f)) {
        unsigned long long s, e, off, ino;
        char perm[8] = {0};
        unsigned dmaj, dmin;
        if (sscanf(line, "%llx-%llx %7s %llx %x:%x %llu", &s, &e, perm, &off, &dmaj, &dmin, &ino) < 7) continue;
        if (e <= GUEST_LO || s >= GUEST_HI || (!ino && !strstr(line, "dmabuf"))) continue;
        if (s < GUEST_LO) s = GUEST_LO;
        if (e > GUEST_HI) e = GUEST_HI;
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        LOG("[sora] guest window: shared mapping %llx-%llx (%llu MB) = committed for VirtualQuery: %s\n", s, e,
            (e - s) >> 20, line);
        if (n && g_sh_hi[n - 1] == s) { g_sh_hi[n - 1] = e; continue; }  // (one dma-buf can show as several lines)
        g_sh_lo[n] = s, g_sh_hi[n] = e, n++;
    }
    fclose(f);
    g_sh_n = n;
    if (n) g_sh_done = true;
    else if (++g_sh_tries >= SW_SH_TRIES) {
        g_sh_done = true;
        LOG("[sora] guest window: no shared mapping after %d looks (heap imported?): mincore only\n", g_sh_tries);
    }
}

// The shared range holding a (true, [lo, hi)), else false and *next = the start of the next one above a (or GUEST_HI).
static bool shared_find(uint64_t a, uint64_t *lo, uint64_t *hi, uint64_t *next) {
    pthread_mutex_lock(&g_sh_mu);
    if (!g_sh_done) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t now = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
        if (now >= g_sh_next_ns) g_sh_next_ns = now + 1000000000ull, shared_scan();
    }
    bool in = false;
    *next = GUEST_HI;
    for (int i = 0; i < g_sh_n; i++) {
        if (a >= g_sh_lo[i] && a < g_sh_hi[i]) { *lo = g_sh_lo[i], *hi = g_sh_hi[i], in = true; break; }
        if (g_sh_lo[i] > a && g_sh_lo[i] < *next) *next = g_sh_lo[i];
    }
    pthread_mutex_unlock(&g_sh_mu);
    return in;
}

SIZE_T sw_VirtualQuery(const void *p, MEMORY_BASIC_INFORMATION *m, SIZE_T sz) {
    if (!m || sz < sizeof *m) return 0;
    uint64_t a = (uint64_t)(uintptr_t)p & ~(SW_PAGE - 1);
    if (a >= SW_USER_END) return 0;
    memset(m, 0, sizeof *m);
    if (a >= GUEST_LO && a < GUEST_HI) {
        uint64_t slo, shi, next;
        if (shared_find(a, &slo, &shi, &next)) {  // (the dma-buf heap: all committed)
            m->BaseAddress = (PVOID)(uintptr_t)a, m->AllocationBase = (PVOID)(uintptr_t)slo;
            m->AllocationProtect = PAGE_READWRITE;
            m->RegionSize = (SIZE_T)(shi - a);
            m->State = MEM_COMMIT, m->Protect = PAGE_READWRITE, m->Type = MEM_PRIVATE;
            return sizeof *m;
        }
        // One RW mapping from the start (platform/linux.c plat_reserve_guest): resident runs = committed. (Stops at
        // the next shared mapping: mincore says "not resident" for all of one.)
        uint64_t end = a + SW_GUEST_STEP < GUEST_HI ? a + SW_GUEST_STEP : GUEST_HI;
        if (end > next) end = next;
        static _Thread_local unsigned char vec[SW_GUEST_STEP / SW_PAGE];
        if (!mincore((void *)(uintptr_t)a, (size_t)(end - a), vec)) {
            size_t n = (size_t)((end - a) / SW_PAGE), k = 1;
            int in = vec[0] & 1;
            while (k < n && (vec[k] & 1) == in) k++;
            m->BaseAddress = (PVOID)(uintptr_t)a, m->AllocationBase = (PVOID)(uintptr_t)GUEST_LO;
            m->AllocationProtect = PAGE_READWRITE;
            m->RegionSize = (SIZE_T)(k * SW_PAGE);
            m->State = in ? MEM_COMMIT : MEM_RESERVE;
            m->Protect = in ? PAGE_READWRITE : 0;
            m->Type = MEM_PRIVATE;
            return sizeof *m;
        }
        // (ENOMEM: part of the step is not mapped, e.g. a hole after a reset: the maps answer below)
    }
    uint64_t lo, hi;
    DWORD prot = 0, type = 0;
    bool found;
    if (!maps_find(a, &lo, &hi, &prot, &type, &found)) return 0;
    m->BaseAddress = (PVOID)(uintptr_t)a;
    m->RegionSize = (SIZE_T)(hi - a);
    if (found) {
        m->AllocationBase = (PVOID)(uintptr_t)lo, m->AllocationProtect = prot;
        m->State = MEM_COMMIT, m->Protect = prot, m->Type = type;
    } else
        m->State = MEM_FREE, m->Protect = PAGE_NOACCESS;
    return sizeof *m;
}

static int posix_prot(DWORD prot) {
    switch (prot & 0xff) {
    case PAGE_READONLY: return PROT_READ;
    case PAGE_READWRITE: case PAGE_WRITECOPY: return PROT_READ | PROT_WRITE;
    case PAGE_EXECUTE: return PROT_EXEC;
    case PAGE_EXECUTE_READ: return PROT_READ | PROT_EXEC;
    case PAGE_EXECUTE_READWRITE: case PAGE_EXECUTE_WRITECOPY: return PROT_READ | PROT_WRITE | PROT_EXEC;
    default: return PROT_NONE;
    }
}

BOOL sw_VirtualProtect(void *p, SIZE_T n, DWORD prot, DWORD *old) {
    uint64_t a = (uint64_t)(uintptr_t)p, lo = a & ~(SW_PAGE - 1), hi = (a + n + SW_PAGE - 1) & ~(SW_PAGE - 1);
    uint64_t mlo, mhi;
    DWORD cur = 0, type;
    bool found;
    if (!n || !maps_find(a, &mlo, &mhi, &cur, &type, &found) || !found) { errno = ENOMEM; return FALSE; }
    if (mprotect((void *)(uintptr_t)lo, (size_t)(hi - lo), posix_prot(prot))) return FALSE;
    if (old) *old = cur;
    return TRUE;
}

// Exactly at `at` or nothing (as Windows with an address): the thunk page has to be within rel32 reach.
void *sw_VirtualAlloc(void *at, SIZE_T n, DWORD type, DWORD prot) {
    (void)type;
    int fl = MAP_PRIVATE | MAP_ANONYMOUS | (at ? MAP_FIXED_NOREPLACE : 0);
    void *r = mmap(at, n, posix_prot(prot), fl, -1, 0);
    if (r == MAP_FAILED) return NULL;
    if (at && r != at) { munmap(r, n); return NULL; }  // (old kernels: only a hint)
    return r;
}

// Valid = resident (mincore), one call per run of consecutive pages. In a shared (dma-buf) range: all valid. All of a
// dma-buf is real memory, so reading it never makes the system back anything new (the Windows reason for "resident
// only"); and nothing can tell the touched pages apart: mincore has no page cache to look at and /proc/self/pagemap
// skips VM_PFNMAP mappings (s9 Deck: 0 of 798720 heap pages "present" with the game in the world).
BOOL sw_QueryWorkingSetEx(HANDLE proc, PVOID ws, DWORD cb) {
    (void)proc;
    PSAPI_WORKING_SET_EX_INFORMATION *w = ws;
    size_t n = cb / sizeof *w;
    unsigned char vec[1024];
    for (size_t i = 0; i < n;) {
        uint64_t a = (uint64_t)(uintptr_t)w[i].VirtualAddress & ~(SW_PAGE - 1);
        size_t k = 1;
        while (i + k < n && k < sizeof vec &&
               ((uint64_t)(uintptr_t)w[i + k].VirtualAddress & ~(SW_PAGE - 1)) == a + k * SW_PAGE) k++;
        uint64_t slo = 0, shi = 0, next = GUEST_HI;
        bool guest = a >= GUEST_LO && a < GUEST_HI, sh = guest && shared_find(a, &slo, &shi, &next);
        if (guest && !sh && a + k * SW_PAGE > next) k = (size_t)((next - a) / SW_PAGE);  // (the run stops at a shared range)
        if (sh) {
            if (a + k * SW_PAGE > shi) k = (size_t)((shi - a) / SW_PAGE);
            for (size_t q = 0; q < k; q++) w[i + q].VirtualAttributes.Flags = 1;
        } else {
            bool ok = !mincore((void *)(uintptr_t)a, (size_t)(k * SW_PAGE), vec);
            for (size_t q = 0; q < k; q++) w[i + q].VirtualAttributes.Flags = ok && (vec[q] & 1);
        }
        i += k;
    }
    return TRUE;
}

// ---- threads: HANDLE = a SwThread (the start routine's own copy of fn/arg is freed by the thread itself)
typedef struct { pthread_t t; bool joined; } SwThread;
typedef struct { LPTHREAD_START_ROUTINE fn; LPVOID arg; } SwStart;

static void *sw_trampoline(void *p) {
    SwStart s = *(SwStart *)p;
    free(p);
    return (void *)(uintptr_t)s.fn(s.arg);
}

HANDLE sw_CreateThread(LPTHREAD_START_ROUTINE fn, LPVOID arg) {
    SwThread *h = calloc(1, sizeof *h);
    SwStart *s = malloc(sizeof *s);
    if (!h || !s) { free(h), free(s); return NULL; }
    s->fn = fn, s->arg = arg;
    if (pthread_create(&h->t, NULL, sw_trampoline, s)) { free(h), free(s); return NULL; }
    return h;
}

DWORD sw_WaitForSingleObject(HANDLE h, DWORD ms) {  // (threads, INFINITE only: our one caller)
    SwThread *t = h;
    if (!t || ms != INFINITE) return 0xFFFFFFFFu;  // (WAIT_FAILED)
    if (!t->joined) pthread_join(t->t, NULL), t->joined = true;
    return WAIT_OBJECT_0;
}

BOOL sw_CloseHandle(HANDLE h) {  // (thread handles only on Linux)
    SwThread *t = h;
    if (!t) return FALSE;
    if (!t->joined) pthread_detach(t->t);
    free(t);
    return TRUE;
}
#endif

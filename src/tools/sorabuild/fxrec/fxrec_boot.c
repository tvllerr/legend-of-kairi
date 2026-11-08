/* fxrec_boot.c - slice 1 driver (s156): map the KH2 FM exe headless and run the effect system init.
 *   fxrec_boot.exe [path to "KINGDOM HEARTS II FINAL MIX.exe"] [-v]
 * Steps: map + bind imports (fxrec_ldr.c) -> fake allocator (bump arena) -> fake device block (getter 0x11C150
 * hooked) -> allocator global 0xB63620 (what 0x2C4330 sets) -> 0x2BDB50(alloc, 0x80000, 0x20000), the call the game
 * makes from 0x1E13B0 via 0x2C4330 -> report faults, imports called, allocations, effect globals.
 * RVAs confirmed in our Ghidra project (recon/ghidra/kh2proj, out/s156_boot*.c). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fxrec_ldr.h"

#define RVA_DEV_GET     0x11C150   /* returns &device (thread-safe static 0x8A0970, ctor 0x10A270) */
#define RVA_FX_INIT     0x2BDB50   /* (allocator*, pool 0x80000, sort 0x20000) */
#define RVA_ALLOC_G     0xB63620   /* allocator global (0x2C4330 writes it) */
#define RVA_FX_ALLOC    0xB1DA20   /* 0x2BDB50: allocator, +8 pool, +0x10 sort buffer */
#define RVA_PPP_MGR     0x2B0EC08  /* 0x2C2480: = &0xB60808 after init */

/* ---------------------------------------------------------------- bump allocator in one arena */
static uint8_t *g_arena; static uint64_t g_arena_size, g_arena_used, g_nalloc, g_nfree;
static void *al_alloc(void *self, int64_t size, int64_t flags)
{
    (void)self; (void)flags;
    uint64_t a = (g_arena_used + 63) & ~63ull;
    if (size < 0 || a + (uint64_t)size > g_arena_size) return NULL;
    g_arena_used = a + (uint64_t)size;
    g_nalloc++;
    memset(g_arena + a, 0, (size_t)size);
    return g_arena + a;
}
static uint64_t al_other(uint32_t id, LdrRegs *r, void *user) { (void)id; (void)r; (void)user; g_nfree++; return 0; }

typedef struct FakeAlloc { void **vt; uint8_t pad[0x100]; } FakeAlloc;
static FakeAlloc g_alloc;
static void *g_alloc_vt[32];

/* ---------------------------------------------------------------- fake device */
static uint8_t *g_dev;
static const char *g_tex_path, *g_fx_path;
static int g_elem = 1, g_frames = 60;
int fxrec_run_fx(uint8_t *dev, void *(*alloc)(void *, int64_t, int64_t), const char *path, int elem, int frames, int verbose);
int fxrec_s2_opt(const char *k, const char *v);
int fxrec_hd_file(const void *base, long size, int which);          /* s160 HD tables (fxrec_s2.c) */
uint64_t fxrec_h_hd(uint32_t id, LdrRegs *r, void *user);   /* --dump / --cam / --owner (fxrec_s2.c) */
static void *dev_get(void) { return g_dev; }

int main(int argc, char **argv)
{
    const char *exe = "D:/SteamLibrary/steamapps/common/KINGDOM HEARTS -HD 1.5+2.5 ReMIX-/KINGDOM HEARTS II FINAL MIX.exe";
    int verbose = 0;
    for (int i = 1; i < argc; i++) { if (strcmp(argv[i], "-v") == 0) verbose = 1; else if (strcmp(argv[i], "--tex") == 0 && i + 1 < argc) g_tex_path = argv[++i]; else if (strcmp(argv[i], "--fx") == 0 && i + 1 < argc) g_fx_path = argv[++i]; else if (strcmp(argv[i], "--elem") == 0 && i + 1 < argc) g_elem = atoi(argv[++i]); else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) g_frames = atoi(argv[++i]); else if (i + 1 < argc && fxrec_s2_opt(argv[i], argv[i + 1])) i++; else exe = argv[i]; }
    setvbuf(stdout, NULL, _IONBF, 0);
    ldr_set_verbose(verbose);

    char err[512];
    if (ldr_map(exe, 1, err, sizeof err) != 0) { printf("map FAILED: %s\n", err); return 2; }
    printf("mapped KH2 at 0x%llX, 0x%llX bytes (sha1 ok)\n", (unsigned long long)KH2_BASE, (unsigned long long)ldr_image_size());

    g_arena_size = 0x30000000ull;
    g_arena = (uint8_t *)VirtualAlloc(NULL, g_arena_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    g_dev = (uint8_t *)VirtualAlloc(NULL, 0x100000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!g_arena || !g_dev) { printf("arena/device alloc failed\n"); return 2; }

    for (int i = 0; i < 32; i++) {
        char nm[32]; snprintf(nm, sizeof nm, "alloc.vt%d", i);
        g_alloc_vt[i] = i == 1 ? (void *)al_alloc : ldr_make_thunk(nm, al_other, NULL);
    }
    g_alloc.vt = g_alloc_vt;

    ldr_hook(RVA_DEV_GET, (void *)dev_get);
    KHU64(RVA_ALLOC_G) = (uint64_t)(uintptr_t)&g_alloc;

    uint64_t args[3] = { (uint64_t)(uintptr_t)&g_alloc, 0x80000, 0x20000 }, ret = 0;
    printf("calling fx init 0x%X(alloc, 0x80000, 0x20000)...\n", RVA_FX_INIT);
    int rc = ldr_call_rva(RVA_FX_INIT, 3, args, &ret);
    if (rc != 0) ldr_print_fault(ldr_last_fault());
    else printf("fx init returned OK\n");

    printf("allocations: %llu (%llu bytes used), other allocator slots called: %llu\n",
           (unsigned long long)g_nalloc, (unsigned long long)g_arena_used, (unsigned long long)g_nfree);
    printf("globals: fx alloc %llX pool %llX sort %llX | ppp mgr %llX (want %llX)\n",
           (unsigned long long)KHU64(RVA_FX_ALLOC), (unsigned long long)KHU64(RVA_FX_ALLOC + 8),
           (unsigned long long)KHU64(RVA_FX_ALLOC + 0x10), (unsigned long long)KHU64(RVA_PPP_MGR),
           (unsigned long long)(KH2_BASE + 0xB60808));
    ldr_report_imports(1);
    ldr_report_hooks();
    int ok = rc == 0 && KHU64(RVA_FX_ALLOC) == (uint64_t)(uintptr_t)&g_alloc && KHU64(RVA_FX_ALLOC + 8) &&
             KHU64(RVA_PPP_MGR) == KH2_BASE + 0xB60808;
    printf("SLICE1 %s\n", ok ? "PASS" : "FAIL");
    if (!ok || !g_tex_path) return ok ? 0 : 1;

    /* slice 2 step 1: shared effect textures. 0x2BEC60(dpd) -> 0x2C2620 -> 0x2C21E0 (texture records via 0x4AECD0
     * -> HD lookup 0x12AA20 / VRAM register 0x12C620), then sets the "drawing on" flag 0xB1D984 = 1. */
    FILE *f = fopen(g_tex_path, "rb");
    if (!f) { printf("cannot open %s\n", g_tex_path); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *dpd = (uint8_t *)al_alloc(NULL, n, 0);
    fread(dpd, 1, (size_t)n, f); fclose(f);
    fxrec_hd_file(dpd, n, 0);
    ldr_hook_rec(0x12AA20, "hd_lookup", fxrec_h_hd, NULL);  /* HD asset answers (--texhd/--fxhd), else 0 = PS2 page */
    ldr_hook_rec(0x12C620, "vram_register", NULL, NULL);
    ldr_hook_rec(0x12C0F0, "vram_lookup", NULL, NULL);
    uint64_t a2[1] = { (uint64_t)(uintptr_t)dpd };
    printf("calling texcommon 0x2BEC60(%ld bytes)...\n", n);
    rc = ldr_call_rva(0x2BEC60, 1, a2, &ret);
    if (rc != 0) ldr_print_fault(ldr_last_fault());
    else printf("texcommon returned OK, draw flag 0xB1D984 = %u\n", KHU32(0xB1D984));
    ldr_report_imports(1);
    ldr_report_hooks();
    if (rc != 0 || !g_fx_path) return rc == 0 ? 0 : 1;
    /* slice 2: one effect element, frame by frame (fxrec_s2.c) */
    return fxrec_run_fx(g_dev, (void *(*)(void *, int64_t, int64_t))al_alloc, g_fx_path, g_elem, g_frames, verbose);
}

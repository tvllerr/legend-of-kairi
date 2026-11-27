/* fxrec_mot.c - kh2mot.exe: KH2's OWN motion evaluation, headless, on the fxrec loader. Written from scratch (2026-11).
 *   kh2mot <KH2 exe> <model .mdlx> <motions .mset> <out.skkm> [--all]
 * Its own tool (not a fxrec_boot mode): the effect recorder's sources (so its re-record check and the kit's prebuilt
 * fxrec_boot.exe) stay untouched; the addresses are its own table (MOT_RVA, paired like fxrec_ldr.h's Epic column).
 *
 * Why: OpenKH's Kh2AnimEmu (sorabake's pose / hit bakes) ignores an interpolated motion's ConstraintActivations (the per
 * constraint on/off switches over time): e.g. P_EX100 A322 (Guard, slot 692) switches its left-hand IK target (C70:
 * joint 274 <- grip 282) and the hand turn (C82: 85 <- 282) OFF at GFR 4, so in KH2 the left hand lets go of the Keyblade
 * and the arm swings behind him; the emulator keeps it glued to the grip. KH2 applies the switches in MotionPrototype0's
 * evaluate (Steam 0x1D2C80): per constraint, flag = byte +1; with switches (u16 +8 count, u16 +10 start; 8-byte
 * {f32 time, i32 active}): time before the first switch -> !first.active, after the last -> last.active, else the switch
 * the time falls in (time = GFR, the motion's start included).
 *
 * What runs here (all KH2 code, the exe mapped by fxrec_ldr.c; Steam rvas named, Epic in MOT_RVA):
 *   model entry (mdlx BAR type 4) -> MOT_MODEL_FIX 0x1A7E60 = kn::ModelSKL built in place (the whole-file fix-up would
 *     also register the file's textures, which needs a device: only this entry);
 *   mset -> MOT_MSET_FIX 0x3A6840 = every slot's motion object built in place (kn::MotionPrototype0 / kn::MotionRAW);
 *   per motion: instance = ModelSKL vtable slot 8 (skl, allocator) (0x1C21E0: per-bone scale / rotation / translation from
 *     the rest pose, three 0x40-per-bone matrix buffers +0x48 / +0x50 / +0x58); trigger list = 16 x 0x20 zeroed entries,
 *     the first marked as the end (|= 0x200000 at +0x14, what 0x1A8D90 does);
 *   per GFR frame f = 0 .. Frames: MOT_POSE 0x1A4D40(instance, identity root (copied unconditionally: not NULL), motion,
 *     (float)f, no look-at target (NULL, as in game without one), triggers): it swaps +0x50 / +0x58, then the motion's
 *     evaluate; the pose = instance +0x50, model space (row vectors: rows 0..2 = axes, row 3 = translation; the root move
 *     NOT taken out - like Kh2AnimEmu's ProvideMatrices, so sorabake strips it the same way).
 *
 * Which motions are written: only those whose switches turn a constraint away from its flag during 0 .. Frames (KH2's
 * rule above, each GFR frame) - every other motion is the same in KH2 and the emulator (P_EX100 idle: all 228 bones within
 * 0.03 cm over 61 frames; all 89: within 0.06 cm), so sorabake keeps the emulator's bytes for them. --all writes every
 * motion (checks). Not covered: P_EX100 slot 680 (A170, no switches) differs at ONE frame (GFR 21, a fast foot: KH2 is
 * smooth, the emulator a frame early) - a sampling difference, left to the emulator.
 *
 * Output (little-endian) 'SKKM' v1: u32 magic 0x4D4B4B53, u32 version 1, u32 n_motions, u32 bones;
 *   dir[n] {u16 slot, u16 frames (= Frames + 1), u32 offset}; data: frames x bones x 12 f32 (r0 r1 r2 r3, 3 floats each).
 * Frames = round((FrameEnd - FrameStart) * 60 / FramesPerSecond), as sorabake's Slot (s91 fps fix). */
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fxrec_ldr.h"

/* Steam / Epic (paired like fxrec_ldr.h's KH2_RVAS: unique instruction shapes in headless Ghidra 12.1.2; the vftables from
 * each exe's RTTI) */
enum { M_MODEL_FIX, M_MSET_FIX, M_POSE, M_VT_SKL, M_VT_MP0, M_VT_RAW, M_N };
static const uint32_t MOT_RVA[2][M_N] = {
    /* Steam */ {0x1A7E60, 0x3A6840, 0x1A4D40, 0x5B3E00, 0x5B4958, 0x5B4A38},
    /* Epic  */ {0x1A7510, 0x3A5BF0, 0x1A43F0, 0x5B3FC0, 0x5B4B18, 0x5B4BF8},
};
static const uint32_t *g_mr;
#define MR(n) (g_mr[M_##n])

typedef void *(*MotAlloc)(void *self, int64_t size, int64_t flags);
typedef void (*PoseFn)(void *inst, void *root, void *motion, float t, void *lookat, void *trig);

static void *g_mi, *g_mm, *g_mt;
static float g_mtime;
static float g_mroot[16] __attribute__((aligned(16))) = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
static uint64_t pose_tramp(void)   /* run through ldr_call (fault guard + KH2's TLS); the float time goes in xmm3 */
{
    ((PoseFn)KH(MR(POSE)))(g_mi, g_mroot, g_mm, g_mtime, NULL, g_mt);
    return 0;
}

static uint8_t *mot_load(MotAlloc al, void *ao, const char *p, long *n)
{
    FILE *f = fopen(p, "rb");
    if (!f) { printf("mot: cannot open %s\n", p); return NULL; }
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = al(ao, *n + 64, 0);
    if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); printf("mot: short read %s\n", p); return NULL; }
    fclose(f);
    return b;
}
/* raw BAR (before a fix-up turns offsets into handles): entry i at 0x10 + 0x10 i = {u16 type, u16 dup, char name[4],
 * u32 offset, u32 size} */
static uint8_t *bar_find(uint8_t *bar, uint32_t size, int type)
{
    if (memcmp(bar, "BAR\x01", 4)) return NULL;
    uint32_t n = *(uint32_t *)(bar + 4);
    for (uint32_t i = 0; i < n && 0x10 + 0x10 * (i + 1) <= size; i++) {
        uint8_t *e = bar + 0x10 + 0x10 * i;
        uint32_t off = *(uint32_t *)(e + 8), sz = *(uint32_t *)(e + 12);
        if (*(uint16_t *)e == type && sz && off + sz <= size) return bar + off;
    }
    return NULL;
}

/* KH2's switch rule (0x1D2C80) for one constraint at GFR time t */
static int act_at(const uint8_t *data, const uint8_t *c, float t)
{
    int flag = c[1];
    uint16_t cnt = *(const uint16_t *)(c + 8), st = *(const uint16_t *)(c + 10);
    if (!cnt) return flag;
    const uint8_t *a = data + *(const uint32_t *)(data + 0x58);
    float t0 = *(const float *)(a + 8 * st), tl = *(const float *)(a + 8 * (st + cnt - 1));
    if (t0 > t) return *(const int32_t *)(a + 8 * st + 4) == 0;   /* before the first: the first entry inverted */
    if (t >= tl) return *(const int32_t *)(a + 8 * (st + cnt - 1) + 4) != 0;
    int k = st;
    for (int i = st; i < st + cnt && *(const float *)(a + 8 * i) <= t; i++) k = i;
    return *(const int32_t *)(a + 8 * k + 4) != 0;
}
/* data = the motion's data block (object +8): +0x50 constraint offset, +0x54 count (12-byte records), +0x58 switches */
static int needs_kh2(const uint8_t *data, int start, int frames)
{
    int n = *(const int32_t *)(data + 0x54);
    const uint8_t *cs = data + *(const uint32_t *)(data + 0x50);
    for (int i = 0; i < n; i++) {
        const uint8_t *c = cs + 12 * i;
        if (!*(const uint16_t *)(c + 8)) continue;
        for (int f = 0; f <= frames; f++)
            if (act_at(data, c, (float)(start + f)) != c[1]) return 1;
    }
    return 0;
}

static int run_mot(void *alloc_obj, MotAlloc al, const char *mdlx_path, const char *mset_path, const char *out_path, int all)
{
    long nm, ns;
    uint8_t *mdlx = mot_load(al, alloc_obj, mdlx_path, &nm), *mset = mot_load(al, alloc_obj, mset_path, &ns);
    if (!mdlx || !mset) return 2;
    uint8_t *mdl = bar_find(mdlx, (uint32_t)nm, 4);
    if (!mdl || memcmp(mset, "BAR\x01", 4)) { printf("mot: no model entry in %s / %s is not a BAR\n", mdlx_path, mset_path); return 2; }
    uint32_t nslots = *(uint32_t *)(mset + 4);
    /* each slot's motion entry, found before the fix-up (it turns the offsets into handles) */
    uint8_t **mot = al(alloc_obj, (int64_t)nslots * 8, 0);
    for (uint32_t s = 0; s < nslots; s++) {
        uint8_t *e = mset + 0x10 + 0x10 * s;
        uint32_t off = *(uint32_t *)(e + 8), sz = *(uint32_t *)(e + 12);
        mot[s] = sz && off + sz <= (uint32_t)ns ? bar_find(mset + off, sz, 9) : NULL;
    }
    uint64_t a[2], ret;
    a[0] = (uint64_t)(uintptr_t)mdl;
    if (ldr_call_rva(MR(MODEL_FIX), 1, a, &ret)) { printf("mot: model fix-up "); ldr_print_fault(ldr_last_fault()); return 1; }
    a[0] = (uint64_t)(uintptr_t)mset;
    if (ldr_call_rva(MR(MSET_FIX), 1, a, &ret)) { printf("mot: mset fix-up "); ldr_print_fault(ldr_last_fault()); return 1; }
    if (ldr_rva(*(uint64_t *)mdl) != MR(VT_SKL)) { printf("mot: the model is not a kn::ModelSKL (vtable rva %X)\n", ldr_rva(*(uint64_t *)mdl)); return 1; }
    uint64_t *svt = (uint64_t *)*(uint64_t *)mdl;
    int nb = *(uint16_t *)(*(uint8_t **)(mdl + 8) + 0x10);

    FILE *o = fopen(out_path, "wb");
    if (!o) { printf("mot: cannot write %s\n", out_path); return 2; }
    uint32_t hdr[4] = {0x4D4B4B53, 1, 0, (uint32_t)nb};
    fwrite(hdr, 4, 4, o);
    long dir_at = ftell(o);
    uint8_t *dir = calloc(nslots, 8);
    fwrite(dir, 8, nslots, o);   /* placeholder: rewritten with the real count */
    uint32_t nout = 0, nmot = 0, nkeep = 0, nraw = 0;
    float *rows = malloc((size_t)nb * 12 * sizeof(float));
    for (uint32_t s = 0; s < nslots; s++) {
        if (!mot[s]) continue;
        uint32_t vt = ldr_rva(*(uint64_t *)mot[s]);
        if (vt == MR(VT_RAW)) { nraw++; continue; }      /* raw motions have no constraints: the emulator is exact */
        if (vt != MR(VT_MP0)) { printf("mot: slot %u is not a motion object (vtable rva %X)\n", s, vt); return 1; }
        nmot++;
        const uint8_t *data = *(const uint8_t **)(mot[s] + 8);
        float fs = *(const float *)(data + 0x90), fe = *(const float *)(data + 0x94), fps = *(const float *)(data + 0x98);
        double k = fps > 0 ? 60.0 / fps : 2;
        int start = (int)lround(fs * k), frames = (int)lround((fe - fs) * k);
        if (!all && !needs_kh2(data, start, frames)) { nkeep++; continue; }
        a[0] = (uint64_t)(uintptr_t)mdl; a[1] = (uint64_t)(uintptr_t)alloc_obj;
        if (ldr_call(svt[8], 2, a, &ret) || !ret) { printf("mot: slot %u: create instance ", s); ldr_print_fault(ldr_last_fault()); return 1; }
        g_mi = (void *)(uintptr_t)ret; g_mm = mot[s];
        g_mt = al(alloc_obj, 16 * 0x20, 0);
        *(uint32_t *)((uint8_t *)g_mt + 0x14) |= 0x200000;   /* the trigger list's end (0x1A8D90) */
        uint32_t ent[2] = {s | ((uint32_t)(frames + 1) << 16), (uint32_t)ftell(o)};
        memcpy(dir + 8 * nout, ent, 8);
        for (int f = 0; f <= frames; f++) {
            g_mtime = (float)f;
            if (ldr_call((uint64_t)(uintptr_t)pose_tramp, 0, NULL, &ret)) {
                printf("mot: slot %u frame %d: ", s, f); ldr_print_fault(ldr_last_fault()); return 1;
            }
            const float *m = *(float **)((uint8_t *)g_mi + 0x50);
            for (int b = 0; b < nb; b++)
                for (int r = 0; r < 4; r++)
                    for (int c = 0; c < 3; c++) rows[b * 12 + r * 3 + c] = m[b * 16 + r * 4 + c];
            fwrite(rows, sizeof(float), (size_t)nb * 12, o);
        }
        printf("MOT slot %u: %d frames by KH2's own code\n", s, frames + 1);
        nout++;
    }
    hdr[2] = nout;
    fseek(o, 0, SEEK_SET); fwrite(hdr, 4, 4, o);
    fseek(o, dir_at, SEEK_SET); fwrite(dir, 8, nout, o);
    fclose(o);
    free(dir); free(rows);
    printf("MOT DONE: %u bones, %u interpolated motions (%u raw skipped): %u written, %u the same in the emulator (%s)\n",
           nb, nmot, nraw, nout, nkeep, all ? "--all" : "only motions with constraint switches");
    return 0;
}

/* ---------------------------------------------------------------- the tool: bump allocator in one arena (as fxrec_boot) */
static uint8_t *g_arena; static uint64_t g_arena_size, g_arena_used;
static void *al_alloc(void *self, int64_t size, int64_t flags)
{
    (void)self; (void)flags;
    uint64_t a = (g_arena_used + 63) & ~63ull;
    if (size < 0 || a + (uint64_t)size > g_arena_size) return NULL;
    g_arena_used = a + (uint64_t)size;
    memset(g_arena + a, 0, (size_t)size);
    return g_arena + a;
}
static uint64_t al_other(uint32_t id, LdrRegs *r, void *user) { (void)id; (void)r; (void)user; return 0; }
typedef struct FakeAlloc { void **vt; uint8_t pad[0x100]; } FakeAlloc;
static FakeAlloc g_alloc;
static void *g_alloc_vt[32];

int main(int argc, char **argv)
{
    int all = 0, n = 0;
    const char *a[4] = {0};
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--all")) all = 1;
        else if (n < 4) a[n++] = argv[i];
    }
    if (n != 4) { printf("usage: kh2mot <KH2 exe> <model .mdlx> <motions .mset> <out.skkm> [--all]\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    char err[512];
    if (ldr_map(a[0], 1, err, sizeof err) != 0) { printf("map FAILED: %s\n", err); return 2; }
    g_mr = MOT_RVA[strcmp(g_kh2_build, "Epic") == 0];
    printf("mapped KH2 (%s)\n", g_kh2_build);
    g_arena_size = 0x20000000ull;
    g_arena = (uint8_t *)VirtualAlloc(NULL, g_arena_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!g_arena) { printf("arena alloc failed\n"); return 2; }
    for (int i = 0; i < 32; i++) {
        char nm[32]; snprintf(nm, sizeof nm, "alloc.vt%d", i);
        g_alloc_vt[i] = i == 1 ? (void *)al_alloc : ldr_make_thunk(nm, al_other, NULL);
    }
    g_alloc.vt = g_alloc_vt;
    return run_mot(&g_alloc, al_alloc, a[1], a[2], a[3], all);
}

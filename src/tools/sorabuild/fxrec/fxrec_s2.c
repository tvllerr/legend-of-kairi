/* fxrec_s2.c - slice 2 (s157): load one effect file, start one element, run frames, snapshot every draw.
 * Called from fxrec_boot.c after fx init + texcommon. RVAs: recon/S156_FXREC.md, recon/S156_HELPER_DEVCALLS.md,
 * recon/ghidra/out/s156_boot.c + s157_a.c (our own decompiles). Oracle: recon/s156_work/ORACLE.md.
 *
 * Device: the renderer wrappers run UNCHANGED on our fake device block; only their GPU work is cut:
 *   0x119BA0 (queue a named D3D lambda) + 0x123700 (its wrapper) = record hooks that never run the lambda,
 *   0x11BB30 (current command context) = a fake object whose vtable slots are record thunks,
 *   0x11A330 (draw core) = our snapshot stub (count, prim, flags, stride, vertex bytes, program, texture),
 *   0x121310 (mode query) = 0, 0x127240 (bind texture) = record (texobj).
 * Vertex ring entry 0 (dev+0x5790) is prefilled so 0x111970 / 0x122FB0 hand out scratch memory unchanged. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "fxrec_ldr.h"

#define RVA_Q_LAMBDA   0x119BA0
#define RVA_Q_LAMBDA2  0x123700
#define RVA_CTX_GET    0x11BB30
#define RVA_DRAW_CORE  0x11A330
#define RVA_MODE_Q     0x121310
#define RVA_BIND_TEX   0x127240
#define RVA_PREVTEX    0x126C30
#define RVA_FRAMETEX   0x126890
#define RVA_PAX_FIX    0x2C4520
#define RVA_FX_START   0x2C6B30
#define RVA_FX_UPDATE  0x2BD190
#define RVA_FX_DRAW    0x2BD830
#define RVA_FRAME_TIME 0x717480
#define RVA_B63628     0xB63628   /* == 1 -> 0x2C6B30 calls 0x3C8000(owner + 0x158): keep it 0 (no owner) */

#define DEV_RING_IDX   0x578C
#define DEV_RING0      0x5790
#define DEV_PROGRAM    0x60

typedef struct Draw { uint32_t count, prim, flags, stride, program; uint64_t tex; uint32_t vbytes; } Draw;
#define MAX_DRAWS 4096
static Draw     g_draws[MAX_DRAWS];
static uint32_t g_ndraw;
static uint64_t g_last_tex;
static uint64_t g_texids[256]; static int g_ntex;
static int tex_id(uint64_t t)   /* 1-based, in order of first use (what the oracle numbers look like) */
{
    if (!t) return 0;
    if (t == (uint64_t)-2) return -2;              /* previous-frame texture (0x126C30 / 0x126890) */
    for (int i = 0; i < g_ntex; i++) if (g_texids[i] == t) return i + 1;
    if (g_ntex < 256) g_texids[g_ntex++] = t;
    return g_ntex;
}
static uint64_t g_vtx_total;

/* s158 slice 3: per-draw snapshot file (--dump), camera runs (--cam yaw,pitch) and owner probes (--owner x|z).
 * Dump (little endian): "FXD1"; per draw "DRW" record: u32 frame, count, prim, flags, stride, program, i32 tex,
 * u32 vbytes, vertex bytes, then device state +0x60..+0x2B0, CB2 +0x59E0 (0x90), CB3 +0x5BA8 (0x200 = regs 0..31),
 * CB4 +0x76AC (0x40), CB6 +0x5B04 (0x50), effect camera block KH 0x2B0EC20 (0x280); per frame "FRM" u32 frame,
 * u32 draws; at the end "END" u32 frames run, u32 ended. Vertex pointer = ring entry +0x48 (0x121FF0 writes it,
 * +0x50 count, +0x08 stride; recon/ghidra/out/s158_a.c). */
static const char *g_hd_tab[2];   /* s160: --texhd / --fxhd tables (hdtab.py) */
static int      g_vram_fake;      /* s160: --vramfake 1 = old s157 rule: unregistered VRAM key -> one fake object per key */
static FILE    *g_dump;
static const char *g_dump_path;   /* --dump (one element) */
static const char *g_all;         /* --all pattern: whole-file run */
static int      g_tails;          /* --tails N: loop-end tails per still-alive element (PHH TAIL_FRAMES 120) */
static int      g_pool_eid = -1;  /* --pool id: element fed one pool particle per frame (PRIZE 0) */
static int      g_isotails;       /* s161 --isotails 1: an alive element prints NEEDTAILS <id> <frames>, no tails here */
static int      g_tailsof = -1, g_tailsn;   /* s161 --tailsof id,n: fresh process, only that element's tails (n = its frames) */
static uint32_t g_vb_from = 0xFFFFFFFFu; static uint64_t g_vb_tail;   /* tail budget: vertex bytes from frame at on */
static int      g_frame_no = -1;   /* -1 = outside the frame loop: draws are not dumped */
static uint32_t g_frame_draws;
static int      g_cam_on; static float g_cam_yaw, g_cam_pitch;
static int      g_owner_axis = -1;
static char     g_pre[4096];   /* --pre: element ids run first (comma list) */
/* s166 fx_keep "used": --only a,b,c = the element ids to record + dump (every earlier id still RUNS, undumped, like --pre: the
 * device state it leaves behind is part of the next element's frames; ids after the largest are not run at all);
 * --tailat id:at,id:at = only these loop-end tails (default: all of 0..--tails) */
static int      g_only[512], g_only_n;
static int      g_tailat[1024][2], g_tailat_n;
#define DEV_STATE0 0x60
#define DEV_STATE1 0x2B0
#define DEV_CB2    0x59E0
#define DEV_CB3    0x5BA8
#define DEV_CB4    0x76AC
#define DEV_CB6    0x5B04
#define RVA_CAMBLK 0x2B0EC20
int fxrec_s2_opt(const char *k, const char *v)
{
    if (strcmp(k, "--vramfake") == 0) { g_vram_fake = atoi(v); return 1; }
    if (strcmp(k, "--texhd") == 0) { g_hd_tab[0] = v; return 1; }
    if (strcmp(k, "--fxhd") == 0) { g_hd_tab[1] = v; return 1; }
    if (strcmp(k, "--dump") == 0) { g_dump_path = v; return 1; }
    if (strcmp(k, "--all") == 0) { g_all = v; return 1; }          /* s160 whole file: dump path pattern with one %s */
    if (strcmp(k, "--tails") == 0) { g_tails = atoi(v); return 1; }
    if (strcmp(k, "--pool") == 0) { g_pool_eid = atoi(v); return 1; }
    if (strcmp(k, "--isotails") == 0) { g_isotails = atoi(v); return 1; }
    if (strcmp(k, "--tailsof") == 0) { if (sscanf(v, "%d,%d", &g_tailsof, &g_tailsn) != 2) { printf("--tailsof wants id,frames\n"); exit(2); } return 1; }
    if (strcmp(k, "--cam") == 0) { g_cam_on = sscanf(v, "%f,%f", &g_cam_yaw, &g_cam_pitch) == 2; return 1; }
    if (strcmp(k, "--pre") == 0) { if (strlen(v) >= sizeof g_pre) { printf("--pre list too long\n"); exit(2); } snprintf(g_pre, sizeof g_pre, "%s", v); return 1; }
    if (strcmp(k, "--only") == 0) {
        for (const char *q = v; *q; ) { if (g_only_n >= 512) { printf("--only list too long\n"); exit(2); } g_only[g_only_n++] = atoi(q); while (*q && *q != ',') q++; if (*q == ',') q++; }
        return 1; }
    if (strcmp(k, "--tailat") == 0) {
        for (const char *q = v; *q; ) { int a, b; if (g_tailat_n >= 1024 || sscanf(q, "%d:%d", &a, &b) != 2) { printf("--tailat wants id:at,id:at\n"); exit(2); }
            g_tailat[g_tailat_n][0] = a; g_tailat[g_tailat_n++][1] = b; while (*q && *q != ',') q++; if (*q == ',') q++; }
        return 1; }
    if (strcmp(k, "--owner") == 0) { g_owner_axis = (v[0] == 'x' || v[0] == 'X') ? 0 : (v[0] == 'z' || v[0] == 'Z') ? 2 : -1;
        if (g_owner_axis < 0) { printf("--owner wants x or z\n"); exit(2); }
        return 1; }
    return 0;
}
static void dump_u32(uint32_t v) { fwrite(&v, 4, 1, g_dump); }

/* s159: depth-sorted draws. 0x4AF7D0(entry, type, key, 0, callback) files a draw into one of 256 view-depth buckets
 * (recon/ghidra/out/s159_sort.c); the flush 0x4AFAD0 later calls callback(entry + 0x10) (entry word = (type-1) |
 * 0x20000000 -> its default case). The key depends on the camera, so we draw the entry AT ONCE (submission order)
 * and tag its draws with what the key is made of (0x4AE220, sprite type 0x11): flag 0x20 (u16 at entry+0x38) ->
 * key = the shape's priority (the key argument), else the shape's local anchor point: shape = entry+0x30,
 * hdr & 8 ? shape + n*0x18 + 0x14 : shape + (n+1)*0x10 + 4 (n = i16 at shape+2), two i16 x KH2's const 0x5A9C9C. */
static int      g_sort_kind;        /* 0 none, 1 key, 2 anchor */
static int32_t  g_sort_key;
static float    g_sort_ax, g_sort_ay;
static uint64_t h_sort(uint32_t id, LdrRegs *r, void *user)
{
    (void)id; (void)user;
    uint8_t *entry = (uint8_t *)(uintptr_t)r->rcx;
    uint32_t typ = (uint32_t)r->rdx;
    uint64_t cb = r->stk[4];
    int pk = g_sort_kind; int32_t pkey = g_sort_key; float pax = g_sort_ax, pay = g_sort_ay;
    g_sort_kind = 1; g_sort_key = (int32_t)(uint32_t)r->r8; g_sort_ax = g_sort_ay = 0.0f;
    if (typ == 0x11 && !(*(uint32_t *)(entry + 0x38) & 0x20)) {
        uint8_t *sh = (uint8_t *)(uintptr_t)*(uint64_t *)(entry + 0x30);
        int64_t n = *(int16_t *)(sh + 2);
        int16_t *xy = (int16_t *)(sh + ((sh[0] & 8) ? n * 0x18 + 0x14 : (n + 1) * 0x10 + 4));
        g_sort_kind = 2; g_sort_ax = (float)xy[0] * KHF32(0x5A9C9C); g_sort_ay = (float)xy[1] * KHF32(0x5A9C9C);
    }
    if (cb) ((void (*)(void *))(uintptr_t)cb)(entry + 0x10);
    g_sort_kind = pk; g_sort_key = pkey; g_sort_ax = pax; g_sort_ay = pay;
    return 0;
}

static uint64_t h_draw(uint32_t id, LdrRegs *r, void *user)
{
    (void)id; (void)user;
    uint8_t *dev = (uint8_t *)(uintptr_t)r->rcx;
    uint32_t idx = *(uint32_t *)(dev + DEV_RING_IDX);
    uint8_t *E = dev + DEV_RING0 + (size_t)idx * 0x58;
    if (g_dump && g_frame_no >= 0) {
        g_frame_draws++;
        uint32_t cnt = (uint32_t)r->rdx, stride = *(uint32_t *)(E + 0x08);
        uint8_t *vp = (uint8_t *)(uintptr_t)*(uint64_t *)(E + 0x48);
        uint32_t vb = vp ? cnt * stride : 0;
        if (g_sort_kind) {                          /* s159: optional tag of the next DRW (sorted draw) */
            fwrite("SRT", 1, 4, g_dump); dump_u32((uint32_t)g_sort_kind); dump_u32((uint32_t)g_sort_key);
            fwrite(&g_sort_ax, 4, 1, g_dump); fwrite(&g_sort_ay, 4, 1, g_dump);
        }
        fwrite("DRW", 1, 4, g_dump);
        dump_u32((uint32_t)g_frame_no); dump_u32(cnt); dump_u32((uint32_t)r->r8); dump_u32((uint32_t)r->r9);
        dump_u32(stride); dump_u32(*(uint32_t *)(dev + DEV_PROGRAM)); dump_u32((uint32_t)tex_id(g_last_tex)); dump_u32(vb);
        if (vb) fwrite(vp, 1, vb, g_dump);
        fwrite(dev + DEV_STATE0, 1, DEV_STATE1 - DEV_STATE0, g_dump);
        fwrite(dev + DEV_CB2, 1, 0x90, g_dump);
        fwrite(dev + DEV_CB3, 1, 0x200, g_dump);
        fwrite(dev + DEV_CB4, 1, 0x40, g_dump);
        fwrite(dev + DEV_CB6, 1, 0x50, g_dump);
        fwrite(KH(RVA_CAMBLK), 1, 0x280, g_dump);
    }
    if (g_ndraw < MAX_DRAWS) {
        Draw *d = &g_draws[g_ndraw++];
        d->count = (uint32_t)r->rdx; d->prim = (uint32_t)r->r8; d->flags = (uint32_t)r->r9;
        d->stride = *(uint32_t *)(E + 0x08);
        d->program = *(uint32_t *)(dev + DEV_PROGRAM);
        d->tex = g_last_tex;
        d->vbytes = d->count * d->stride;
    }
    g_vtx_total += (uint32_t)r->rdx;
    /* tail budget = PHH's: raw draw vertex bytes (count x stride, 0 without a vertex pointer) from frame `at` on
     * (s161 fix: was count x r9 = draw flags; no file reached 150 MB before King Mickey's element 2) */
    if (g_frame_no >= 0 && (uint32_t)g_frame_no >= g_vb_from && *(uint64_t *)(E + 0x48))
        g_vb_tail += (uint64_t)(uint32_t)r->rdx * *(uint32_t *)(E + 0x08);
    *(uint32_t *)(E + 0x14) = 0;                    /* what the real draw leaves: used count reset */
    return 0;
}
static uint64_t h_bind(uint32_t id, LdrRegs *r, void *user) { (void)id; (void)user; g_last_tex = r->rcx; return 0; }
static uint64_t h_prev(uint32_t id, LdrRegs *r, void *user) { (void)id; (void)r; (void)user; g_last_tex = (uint64_t)-2; return 0; }

/* VRAM lookup 0x12C0F0(TEX0 *): KH2 turns the PS2 TEX0 register into a key and finds the texture object in a std::map
 * (0x8AEF10) that the skipped CRT init would build. We compute the SAME exact key (decompile s156_boot.c) and hand out
 * one fake texture object per distinct key, so draws name their texture. */
#define MAX_FTEX 512
static uint64_t g_ftex_key[MAX_FTEX];
static uint8_t  g_ftex_obj[MAX_FTEX][0x100];
static int      g_nftex;
static uint64_t tex0_key(uint64_t t)
{
    uint32_t psm = (uint32_t)(t >> 0x14) & 0x3F;
    uint64_t p = (psm > 0x2C || ((0x101008000002ull >> psm) & 1) == 0) ? 0 : psm;
    uint64_t hi = (((uint64_t)((uint32_t)t & 0x3FFF) << 0x10) | p) << 0x20;
    return ((t >> 0x17 & 0x3E00000000ull) | (t & 0x7FFE000000000ull)) >> 0x21 | hi;
}
/* s160 VRAM register 0x12C620(tbp, ?, texobj, psm, cbp) (decompile s156_boot.c): key = ((psm masked) | tbp << 16) << 32
 * | cbp; an old object under the key gets +0xC0 = 0 and the key is erased; the new object is kept only if it carries the
 * texture magic and +0xB4 != 1 (then +0xC0 = 1). Lookup 0x12C0F0: exact key, then the key with cbp 0, else 0.
 * Keys never registered keep the s157 rule (one fake object per key) so PS2-page draws still name their texture. */
#define MAX_VREG 1024
static uint64_t g_vreg_key[MAX_VREG], g_vreg_obj[MAX_VREG];
static int      g_nvreg;
static uint32_t g_vreg_calls, g_vreg_nomagic, g_vlk_exact, g_vlk_hi, g_vlk_fake;
static int vreg_find(uint64_t k) { for (int i = 0; i < g_nvreg; i++) if (g_vreg_key[i] == k) return i; return -1; }
static uint64_t h_vram_register(uint32_t id, LdrRegs *r, void *user)
{
    (void)id; (void)user;
    g_vreg_calls++;
    uint32_t psm = (uint32_t)r->r9;
    if (psm > 0x2C || ((0x101008000002ull >> (psm & 0x3F)) & 1) == 0) psm = 0;
    int64_t tbp = (int32_t)(uint32_t)r->rcx, cbp = (int32_t)(uint32_t)r->stk[4];
    uint64_t k = (((uint64_t)(psm & 0xFFFF) | ((uint64_t)tbp << 16)) << 32) | (uint64_t)cbp;
    uint8_t *obj = (uint8_t *)(uintptr_t)r->r8;
    int i = vreg_find(k);
    if (i >= 0) {
        uint8_t *old = (uint8_t *)(uintptr_t)g_vreg_obj[i];
        if (old && old != obj) *(uint32_t *)(old + 0xC0) = 0;
        g_vreg_key[i] = g_vreg_key[g_nvreg - 1]; g_vreg_obj[i] = g_vreg_obj[g_nvreg - 1]; g_nvreg--;
    }
    if (obj && *(uint32_t *)(obj + 0x10) == 0x2F02EB5) {
        if (*(uint32_t *)(obj + 0xB4) == 1) *(uint32_t *)(obj + 0xC0) = 0;
        else {
            *(uint32_t *)(obj + 0xC0) = 1;
            if (g_nvreg >= MAX_VREG) { printf("VRAM table full\n"); exit(2); }
            g_vreg_key[g_nvreg] = k; g_vreg_obj[g_nvreg++] = (uint64_t)(uintptr_t)obj;
        }
    } else g_vreg_nomagic++;
    return 0;
}
static uint64_t h_vram_lookup(uint32_t id, LdrRegs *r, void *user)
{
    (void)id; (void)user;
    if (!r->rcx) return 0;
    uint64_t k = tex0_key(*(uint64_t *)(uintptr_t)r->rcx);
    int vi = vreg_find(k);
    if (vi >= 0) { g_vlk_exact++; return g_vreg_obj[vi]; }
    vi = vreg_find(k & 0xFFFFFFFF00000000ull);
    if (vi >= 0) { g_vlk_hi++; return g_vreg_obj[vi]; }
    g_vlk_fake++;
    if (!g_vram_fake) return 0;                    /* KH2's own rule (and PHH's): nothing under the key = no texture */
    for (int i = 0; i < g_nftex; i++) if (g_ftex_key[i] == k) return (uint64_t)(uintptr_t)g_ftex_obj[i];
    if (g_nftex >= MAX_FTEX) return 0;
    g_ftex_key[g_nftex] = k;
    *(uint32_t *)(g_ftex_obj[g_nftex] + 0x10) = 0x2F02EB5;   /* texture object magic 0x127240 checks */
    return (uint64_t)(uintptr_t)g_ftex_obj[g_nftex++];
}

/* s160 HD texture answers. 0x4AECD0 (PAX / dpd texture records) asks 0x12AA20(ctx, rec + 0x20 [+ i], .., .., w, h)
 * for a remastered asset: non-zero = KH2 stores that object's handle (0x4AD240) at rec+8 with type 0xFFFD and later
 * binds it instead of the PS2 page (0xFFFE). The real game finds the asset by the address' offset in the loaded file;
 * we answer from the package's remastered headers (hdtab.py: original-file offset & 0x1FFFFFFF -> asset name).
 * Tables: --texhd <tab> (texcommon.dpd), --fxhd <tab> (the effect file). No table = every lookup misses (old runs). */
#define MAX_HDROW 128
typedef struct HdFile { const uint8_t *base; long size; int n; uint32_t off[MAX_HDROW]; char name[MAX_HDROW][32]; char tag[16]; } HdFile;
static HdFile   g_hdf[4]; static int g_nhdf;
typedef struct HdTex { uint8_t obj[0x200]; int file, row, w, h; } HdTex;
#define MAX_HDTEX 128
static HdTex   *g_hdtex; static int g_nhdtex;
static uint32_t g_hd_calls, g_hd_miss_file;

int fxrec_hd_file(const void *base, long size, int which)
{
    if (which < 0 || which > 1 || !g_hd_tab[which] || g_nhdf >= 4) return 0;
    FILE *f = fopen(g_hd_tab[which], "r");
    if (!f) { printf("cannot open HD table %s\n", g_hd_tab[which]); exit(2); }
    HdFile *h = &g_hdf[g_nhdf];
    memset(h, 0, sizeof *h);
    h->base = (const uint8_t *)base; h->size = size;
    char line[256], pkg[32]; long sz = -1;
    if (!fgets(line, sizeof line, f) || sscanf(line, "pkg %31s size %ld", pkg, &sz) != 2 || sz != size) {
        printf("HD table %s: header wrong or size %ld != file %ld\n", g_hd_tab[which], sz, size); exit(2);
    }
    snprintf(h->tag, sizeof h->tag, "%s", which ? "fx" : "texcommon");
    while (fgets(line, sizeof line, f)) {
        unsigned o; char nm[64];
        if (sscanf(line, "%x %63s", &o, nm) != 2) continue;
        if (h->n >= MAX_HDROW) { printf("HD table %s: more than %d rows\n", g_hd_tab[which], MAX_HDROW); exit(2); }
        h->off[h->n] = o; snprintf(h->name[h->n], 32, "%s", nm); h->n++;
    }
    fclose(f);
    printf("HD table %s: %d assets (%s)\n", h->tag, h->n, pkg);
    g_nhdf++;
    return h->n;
}

uint64_t fxrec_h_hd(uint32_t id, LdrRegs *r, void *user)
{
    (void)id; (void)user;
    g_hd_calls++;
    const uint8_t *a = (const uint8_t *)(uintptr_t)r->rdx;
    for (int i = 0; i < g_nhdf; i++) {
        HdFile *h = &g_hdf[i];
        if (a < h->base || a >= h->base + h->size) continue;
        uint32_t off = (uint32_t)(a - h->base) & 0x1FFFFFFF;
        for (int k = 0; k < h->n; k++) {
            if (h->off[k] != off) continue;
            if (!g_hdtex) g_hdtex = (HdTex *)calloc(MAX_HDTEX, sizeof(HdTex));
            if (g_nhdtex >= MAX_HDTEX) { printf("too many HD textures\n"); exit(2); }
            HdTex *t = &g_hdtex[g_nhdtex++];
            t->file = i; t->row = k; t->w = (int)(r->stk[4] & 0xFFFF); t->h = (int)(r->stk[5] & 0xFFFF);
            *(uint32_t *)(t->obj + 0x10) = 0x2F02EB5;   /* texture object magic (0x127240) */
            return (uint64_t)(uintptr_t)t->obj;
        }
        return 0;                                      /* no asset at this offset: KH2 uses the PS2 page */
    }
    g_hd_miss_file++;
    return 0;
}

/* end of run: what each texture id is (writer input later): HD asset or PS2 page (VRAM key) */
static void report_textures(void)
{
    printf("hd_lookup: %u calls, %d HD objects, %u outside the loaded files\n", g_hd_calls, g_nhdtex, g_hd_miss_file);
    printf("vram: %u registers (%u without texture magic), lookups %u exact / %u cbp-0 / %u unregistered (fake per key)\n",
           g_vreg_calls, g_vreg_nomagic, g_vlk_exact, g_vlk_hi, g_vlk_fake);
    for (int i = 0; i < g_ntex; i++) {
        uint64_t t = g_texids[i];
        int done = 0;
        for (int k = 0; k < g_nhdtex && !done; k++)
            if (t == (uint64_t)(uintptr_t)g_hdtex[k].obj) {
                HdTex *x = &g_hdtex[k];
                printf("TEXID %d hd %s %s %dx%d\n", i + 1, g_hdf[x->file].tag, g_hdf[x->file].name[x->row], x->w, x->h);
                done = 1;
            }
        for (int k = 0; k < g_nftex && !done; k++)
            if (t == (uint64_t)(uintptr_t)g_ftex_obj[k]) { printf("TEXID %d ps2 key %016llX\n", i + 1, (unsigned long long)g_ftex_key[k]); done = 1; }
        /* s162: a KH2 texture record inside a loaded file = PS2-data page (PHH: Texture(file, off, None) -> one EMPTY
         * texture per (file, None) in the bin, numbered at first use like HD ones) */
        for (int k = 0; k < g_nhdf && !done; k++)
            if (t >= (uint64_t)(uintptr_t)g_hdf[k].base && t < (uint64_t)(uintptr_t)g_hdf[k].base + (uint64_t)g_hdf[k].size) {
                printf("TEXID %d ps2file %s\n", i + 1, g_hdf[k].tag); done = 1;
            }
        if (!done) printf("TEXID %d unknown %llX\n", i + 1, (unsigned long long)t);
    }
}

static void *g_ctx_obj[2];
static void *g_ctx_vt[128];
static uint64_t h_ctx(uint32_t id, LdrRegs *r, void *user) { (void)id; (void)r; (void)user; return (uint64_t)(uintptr_t)g_ctx_obj; }

static void ident(float *m) { memset(m, 0, 64); m[0] = m[5] = m[10] = m[15] = 1.0f; }

static uint8_t *load_file(const char *p, long *n_out, void *(*alloc)(void *, int64_t, int64_t))
{
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = (uint8_t *)alloc(NULL, n + 64, 0);
    if (b) fread(b, 1, (size_t)n, f);
    fclose(f);
    *n_out = n;
    return b;
}

/* s160: one recording = PHH's record(): srand(1) (KH2's rand is this ucrtbase rand), start 0x2C6B30(set, id, 0, 1, 0, 0),
 * per frame: effect_loop_end 0x2BE8E0 at frame loop_end_at (if alive), owner probe (manager +0 translation += 1.0 x
 * frame on x / z), pool hand-off (pool: one 16-byte particle {0,0,0, 7,0x80,0xFF,0xFF}, effect +0xC |= 0x40000, +0x48
 * records, +0x50 count = 1; re:0x412450 / 0x2BEBE0 per PHH), update 0x2BD190 + draw 0x2BD830; stops when the dead flag
 * (+0xC bit 16) is set, else after `frames` it is killed (0x4B01C0 + 0x2C1F50 on the manager, +0x30 = 0, dead flag).
 * dump != NULL: own FXD file (FRM per frame, END frames run + ended). Returns 1 ended, 0 killed, -1 fault, -2 no element. */
static int g_print_frames;
static int record_element(uint64_t *set, uint64_t *up, int eid, int frames, int loop_end_at, const char *dump, int pool, int *nframes)
{
    static uint8_t *pool_buf;
    uint64_t a6[6] = { (uint64_t)(uintptr_t)set, (uint64_t)eid, 0, 1, 0, 0 }, fx = 0;
    *nframes = 0;
    srand(1);
    if (ldr_call_rva(RVA_FX_START, 6, a6, &fx)) { printf("start %d FAULTED\n", eid); ldr_print_fault(ldr_last_fault()); return -1; }
    if (!fx) { printf("start: no element %d\n", eid); return -2; }
    if (dump) {
        g_dump = fopen(dump, "wb");
        if (!g_dump) { printf("cannot write %s\n", dump); return -1; }
        fwrite("FXD1", 1, 4, g_dump);
    }
    uint8_t *mgr0 = (uint8_t *)(uintptr_t)*(uint64_t *)(uintptr_t)(fx + 0x30);
    float trs0[3] = { 0, 0, 0 };
    if (mgr0) memcpy(trs0, mgr0, 12);
    else if (g_owner_axis >= 0 && dump) { printf("owner probe: element %d has no ppp manager (+0x30 = 0)\n", eid); return -1; }
    if (pool && !pool_buf) pool_buf = (uint8_t *)calloc(1, 256);
    int ended = 0, f = 0;
    for (f = 0; f < frames; f++) {
        g_ndraw = 0;
        /* a tail (loop_end_at >= 0) is stored like PHH's: frames before the loop end stay empty (FRM with 0 draws) */
        g_frame_no = (dump && f >= loop_end_at) ? f : -1; g_frame_draws = 0;
        uint64_t v0 = g_vtx_total;
        if (f == loop_end_at && !(*(uint32_t *)(uintptr_t)(fx + 0xC) & 0x10000)) {
            uint64_t le[1] = { fx };
            if (ldr_call_rva(0x2BE8E0, 1, le, NULL)) { printf("loop end FAULTED\n"); ldr_print_fault(ldr_last_fault()); return -1; }
        }
        uint8_t *mgr = (uint8_t *)(uintptr_t)*(uint64_t *)(uintptr_t)(fx + 0x30);
        if (g_owner_axis >= 0 && mgr) { float t[3]; memcpy(t, trs0, 12); t[g_owner_axis] += 1.0f * (float)f; memcpy(mgr, t, 12); }
        if (pool) {
            float xyz[3] = { 0, 0, 0 }; uint8_t b4[4] = { 7, 0x80, 0xFF, 0xFF };
            memset(pool_buf, 0, 16); memcpy(pool_buf, xyz, 12); memcpy(pool_buf + 12, b4, 4);
            *(uint32_t *)(uintptr_t)(fx + 0xC) |= 0x40000;
            *(uint64_t *)(uintptr_t)(fx + 0x48) = (uint64_t)(uintptr_t)pool_buf;
            *(uint32_t *)(uintptr_t)(fx + 0x50) = 1;
        }
        if (ldr_call_rva(RVA_FX_UPDATE, 3, up, NULL)) { printf("element %d frame %d: update FAULTED\n", eid, f); ldr_print_fault(ldr_last_fault()); return -1; }
        if (ldr_call_rva(RVA_FX_DRAW, 0, NULL, NULL)) { printf("element %d frame %d: draw FAULTED\n", eid, f); ldr_print_fault(ldr_last_fault()); return -1; }
        if (g_print_frames) {
            printf("%5d | %5u | %5llu |", f, g_ndraw, (unsigned long long)(g_vtx_total - v0));
            for (uint32_t i = 0; i < g_ndraw && i < 64; i++)
                printf(" %u/%u/%u/%u/%d", g_draws[i].count, g_draws[i].prim, g_draws[i].stride, g_draws[i].program, tex_id(g_draws[i].tex));
            printf("\n");
        }
        if (g_dump) { fwrite("FRM", 1, 4, g_dump); dump_u32((uint32_t)f); dump_u32(g_frame_no >= 0 ? g_frame_draws : 0); }
        if (*(uint32_t *)(uintptr_t)(fx + 0xC) & 0x10000) { if (g_print_frames) printf("element ended after frame %d\n", f); ended = 1; f++; break; }
    }
    g_frame_no = -1;
    if (!ended) {
        uint64_t pm[1] = { *(uint64_t *)(uintptr_t)(fx + 0x30) };
        if (pm[0]) {
            if (ldr_call_rva(0x4B01C0, 1, pm, NULL) || ldr_call_rva(0x2C1F50, 1, pm, NULL)) {
                printf("element %d: kill FAULTED\n", eid); ldr_print_fault(ldr_last_fault()); return -1;
            }
            *(uint64_t *)(uintptr_t)(fx + 0x30) = 0;
        }
        *(uint32_t *)(uintptr_t)(fx + 0xC) |= 0x10000;
    }
    if (g_dump) { fwrite("END", 1, 4, g_dump); dump_u32((uint32_t)f); dump_u32((uint32_t)ended); fclose(g_dump); g_dump = NULL; }
    *nframes = f;
    return ended;
}

/* returns 0 = ran all frames (or the element ended) */
int fxrec_run_fx(uint8_t *dev, void *(*alloc)(void *, int64_t, int64_t), const char *path, int elem, int frames, int verbose)
{
    /* device: screen sizes + viewport the wrappers read (fake 1920x1080) */
    *(int32_t *)(dev + 0x10) = 1920; *(int32_t *)(dev + 0x14) = 1080;
    *(float *)(dev + 0x18) = 1920.0f; *(float *)(dev + 0x1C) = 1080.0f;
    *(float *)(dev + 0x20) = 1.0f; *(float *)(dev + 0x24) = 1.0f; *(float *)(dev + 0x28) = 1.0f;
    { float vp[4] = { 0, 0, 1920.0f, 1080.0f }; memcpy(dev + 0x90, vp, 16); memcpy(dev + 0xA0, vp, 16); }
    /* vertex ring entry 0 */
    uint8_t *scratch = (uint8_t *)alloc(NULL, 0x4000000, 0);
    *(uint32_t *)(dev + DEV_RING_IDX) = 0;
    *(uint64_t *)(dev + 0x59B8) = 0;
    uint8_t *E = dev + DEV_RING0;
    *(uint64_t *)(E + 0x30) = 0x4000000; *(uint64_t *)(E + 0x38) = 0; *(uint64_t *)(E + 0x40) = (uint64_t)(uintptr_t)scratch;

    for (int i = 0; i < 128; i++) { char nm[32]; snprintf(nm, sizeof nm, "ctx.vt%d", i); g_ctx_vt[i] = ldr_make_thunk(nm, NULL, NULL); }
    g_ctx_obj[0] = g_ctx_vt;
    ldr_hook_rec(RVA_Q_LAMBDA, "queue_lambda", NULL, NULL);
    ldr_hook_rec(RVA_Q_LAMBDA2, "queue_lambda2", NULL, NULL);
    ldr_hook_rec(RVA_CTX_GET, "ctx_get", h_ctx, NULL);
    ldr_hook_rec(RVA_DRAW_CORE, "draw_core", h_draw, NULL);
    ldr_patch_ret(RVA_MODE_Q, 0);
    ldr_hook_rec(RVA_BIND_TEX, "bind_tex", h_bind, NULL);
    ldr_hook_rec(RVA_PREVTEX, "prev_frame_tex", h_prev, NULL);
    ldr_hook_rec(RVA_FRAMETEX, "frame_tex", h_prev, NULL);
    ldr_hook_rec(0x12C0F0, "vram_lookup_key", h_vram_lookup, NULL);
    ldr_hook_rec(0x12C620, "vram_register_map", h_vram_register, NULL);
    ldr_hook_rec(0x4AF7D0, "sort_insert", h_sort, NULL);

    KHF32(RVA_FRAME_TIME) = 1.0f;
    KHU32(RVA_B63628) = 0;

    /* camera: identity proj (+0, +0x84) and view (+8), eye 0, target -z */
    float *proj = (float *)alloc(NULL, 0x200, 0), *cam = (float *)alloc(NULL, 0x200, 0);
    ident(proj); ident((float *)((uint8_t *)proj + 0x84));
    /* view (row vectors, 4x4 row-major at cam+8): R = Ry(yaw) x Rx(pitch), the generic-angle turns of the camera trick
     * (S155_HELPER_FXHARNESS.md). Eye at the origin, target = eye + the view's column 2 (identity: +z). */
    float *V = (float *)((uint8_t *)cam + 8);
    ident(V);
    if (g_cam_on) {
        double y = g_cam_yaw * 3.14159265358979323846 / 180.0, p = g_cam_pitch * 3.14159265358979323846 / 180.0;
        double cy = cos(y), sy = sin(y), cp = cos(p), sp = sin(p);
        double ry[3][3] = { { cy, 0, -sy }, { 0, 1, 0 }, { sy, 0, cy } }, rx[3][3] = { { 1, 0, 0 }, { 0, cp, sp }, { 0, -sp, cp } };
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
            double s = 0; for (int k = 0; k < 3; k++) s += ry[i][k] * rx[k][j];
            V[i * 4 + j] = (float)s;
        }
    }
    { float eye[4] = { 0, 0, 0, 1 }, tgt[4] = { V[2], V[6], V[10], 1 }; memcpy((uint8_t *)cam + 0x48, eye, 16); memcpy((uint8_t *)cam + 0x58, tgt, 16); }

    long n = 0;
    uint8_t *file = load_file(path, &n, alloc);
    if (!file) { printf("cannot open %s\n", path); return 1; }
    fxrec_hd_file(file, n, 1);
    uint8_t *pax = file;
    if (memcmp(file, "BAR\x01", 4) == 0) {
        uint32_t cnt = *(uint32_t *)(file + 4); pax = NULL;
        for (uint32_t i = 0; i < cnt; i++) {
            uint8_t *e = file + 0x10 + i * 0x10;
            if (*(uint16_t *)e == 0x12) { pax = file + *(uint32_t *)(e + 8); break; }
        }
        if (!pax) { printf("no PAX entry (type 0x12) in %s\n", path); return 1; }
    }
    if (memcmp(pax, "PAX_", 4) != 0) { printf("not a PAX: %.4s\n", (char *)pax); return 1; }
    printf("PAX at +0x%llX: %u elements, dpx +0x%X\n", (unsigned long long)(pax - file), *(uint32_t *)(pax + 8), *(uint32_t *)(pax + 0xC));

    uint64_t ret = 0, a1[1] = { (uint64_t)(uintptr_t)pax };
    int rc = ldr_call_rva(RVA_PAX_FIX, 1, a1, &ret);
    if (rc) { printf("PAX fix-up FAULTED\n"); ldr_print_fault(ldr_last_fault()); return 1; }
    printf("PAX fix-up OK\n");

    uint64_t *set = (uint64_t *)alloc(NULL, 0x40, 0);
    set[0] = (uint64_t)(uintptr_t)pax;
    set[1] = (uint64_t)(uintptr_t)(pax + 0x10);
    set[2] = (uint64_t)(uintptr_t)(pax + *(uint32_t *)(pax + 0xC));
    uint64_t up[3] = { (uint64_t)(uintptr_t)proj, (uint64_t)(uintptr_t)cam, 0 };
    uint32_t nel = *(uint32_t *)(pax + 8);

    if (g_all) {
        /* s160 whole-file run = PHH's worker: every element id in SORTED order (unique), one process, each recorded
         * up to 600 frames (pool element: 60, counted as ended); an element still alive then gets its loop-end tails
         * (--tails N: one recording per start frame at < min(N, frames recorded), effect_loop_end at `at`, cap at+300,
         * stop after 150 MB of vertex bytes), recorded right after it. Dumps: printf(g_all, "<id>") / "<id>~<at>". */
        int ids[512], nid = 0;
        for (uint32_t i = 0; i < nel && nid < 512; i++) {
            int id = *(uint16_t *)(pax + 0x10 + 0x50 * i + 2), dup = 0;
            for (int k = 0; k < nid; k++) dup |= ids[k] == id;
            if (!dup) ids[nid++] = id;
        }
        for (int i = 1; i < nid; i++) for (int k = i; k > 0 && ids[k - 1] > ids[k]; k--) { int t = ids[k]; ids[k] = ids[k - 1]; ids[k - 1] = t; }
        /* s161 --tailsof id,n = PHH's isolated tails (King Mickey P_EX200 / P_EX220: KH2's depth sort allocates
         * count x 12 bytes per frame from the never-freed arena, count = effects left in KH2's list, so tails after
         * hundreds of recordings would fill it): a FRESH process loads + fixes up the file, then records only element
         * id's tails (n = frames its main recording ran); no other element runs first. A dead effect draws nothing. */
        if (g_tailsof >= 0) { nid = 1; ids[0] = g_tailsof; }
        int only_max = -1;
        for (int k = 0; k < g_only_n; k++) if (g_only[k] > only_max) only_max = g_only[k];
        for (int i = 0; i < nid; i++) {
            char nm[64], path[1024];
            int pool = ids[i] == g_pool_eid, nrec = 0, e = 0;
            if (g_tailsof < 0 && g_only_n) {
                int kept = 0;
                for (int k = 0; k < g_only_n; k++) kept |= g_only[k] == ids[i];
                if (ids[i] > only_max) break;          /* nothing kept after this id: not run */
                if (!kept) {                           /* earlier id: runs (state carries over), nothing stored */
                    e = record_element(set, up, ids[i], pool ? (frames < 60 ? frames : 60) : frames, -1, NULL, pool, &nrec);
                    if (e < 0) return 1;
                    printf("PRE %d %s after %d frames\n", ids[i], e ? "ended" : "killed", nrec);
                    continue;
                }
            }
            if (g_tailsof < 0) {
                snprintf(nm, sizeof nm, "%d", ids[i]); snprintf(path, sizeof path, g_all, nm);
                e = record_element(set, up, ids[i], pool ? (frames < 60 ? frames : 60) : frames, -1, path, pool, &nrec);
                if (e < 0) return 1;
                if (pool) e = 1;
                printf("ELEMENT %d %s after %d frames\n", ids[i], e ? "ended" : "killed", nrec);
                if (e || g_tails <= 0) continue;
                if (g_isotails) { printf("NEEDTAILS %d %d\n", ids[i], nrec); continue; }
            } else nrec = g_tailsn;
            uint64_t spent = 0;
            for (int at = 0; at < g_tails && at < nrec; at++) {
                if (g_tailat_n) {
                    int want = 0;
                    for (int k = 0; k < g_tailat_n; k++) want |= g_tailat[k][0] == ids[i] && g_tailat[k][1] == at;
                    if (!want) continue;
                }
                if (spent > (150ull << 20)) { printf("TAILS %d: budget hit before at %d\n", ids[i], at); break; }
                int tn = 0;
                snprintf(nm, sizeof nm, "%d~%d", ids[i], at); snprintf(path, sizeof path, g_all, nm);
                g_vb_from = (uint32_t)at; g_vb_tail = 0;
                int te = record_element(set, up, ids[i], at + 300, at, path, 0, &tn);
                if (te < 0) return 1;
                spent += g_vb_tail; g_vb_from = 0xFFFFFFFFu;
                printf("TAIL %d~%d %s after %d frames\n", ids[i], at, te ? "ended" : "killed", tn);
            }
        }
        printf("FILE DONE %d elements\n", nid);
        report_textures();
        if (verbose) { ldr_report_imports(1); ldr_report_hooks(); }
        return 0;
    }

    /* --pre a,b,c: run these elements first in this process, as a whole-file recording does (device state such as cull
     * carries over from one element to the next); each until it ends or 600 frames, then killed. Nothing is dumped. */
    if (g_pre[0]) {
        for (const char *s = g_pre; *s; ) {
            int pe = atoi(s), pn = 0;
            int e = record_element(set, up, pe, 600, -1, NULL, 0, &pn);
            if (e == -2) printf("pre %d: no element / fault\n", pe);
            else if (e < 0) return 1;
            else printf("pre %d: %s after %d frames\n", pe, e ? "ended" : "killed", pn);
            while (*s && *s != ',') s++;
            if (*s == ',') s++;
        }
    }
    int nrec = 0;
    g_print_frames = 1;
    printf("frame | draws | verts | detail (count/prim/stride/prog/tex)\n");
    int e = record_element(set, up, elem, frames, -1, g_dump_path, 0, &nrec);
    if (e < 0) return 1;
    report_textures();
    if (verbose) { ldr_report_imports(1); ldr_report_hooks(); }
    return 0;
}

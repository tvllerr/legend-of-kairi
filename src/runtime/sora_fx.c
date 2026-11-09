// Sora-in-BotW: KH2's own visual effects (hit sparks, spell casts, Fire / Blizzard / Thunder / Cure, the Keyblade
// swing ribbon) drawn in Sora's draw pass. Recon + design: D:/SoraBotW/recon/S35_HELPER_X_FX.md. API: sora_fx.h.
//
// What is drawn: D:/SoraBotW/data/fx/fx.bin (tools/fxbake.py) = KH2's effect elements exactly as Project Heart
// Hunters recorded them from KH2's own effect runtime (PHH tools/bake_kh2_fx2.py: three camera runs per element give
// each vertex's world point and camera-space offset, so billboards replay under any camera), KH2's HD effect
// textures (DXT5 decoded to BGRA8 offline), KH2's blend state per batch (additive = D3D SRCALPHA / ONE, alpha =
// SRCALPHA / INVSRCALPHA, alpha test > 0.002). Per element and 60 Hz KH frame the recording holds the triangles of
// that frame; a "player" only places them: owner pose (position + yaw, kept per frame so a particle born on frame
// f keeps the pose of frame f, PHF2's `age`), KH cm -> u, the camera-space offset turned into the world with the
// camera's rotation (so the effect is a plain world-space triangle list for sora_draw's mvp).
//
// Threads: everything but sora_fx_draw runs on the game thread (sora.c's frame, like sora_draw_pose); sora_fx_tick
// builds the frame's triangle list and publishes it under a mutex; sora_fx_draw (render thread, inside
// sora_draw.c draw_pass's dynamic rendering: the window image, or with the s57 ui order the game's final image at
// BotW's scene composite, + Sora's own cleared depth, after his body) copies it out,
// uploads it transiently and draws it with its own pipelines (no depth write, depth test LEQUAL against Sora's body,
// the same scene-depth occlusion as his body with its own bias).
//
// Spaces (UNVERIFIED in game, see the report "chirality"): a recorded point p is in KH2 effect space (x, y DOWN, z,
// cm). PHH draws an unbound element with KH2's world axes and a bound one (PAX flag 1, not 4) in the owner's bind
// frame, which is the world frame turned by pi (fxplay.bind_rows / WORLD_ROWS). sorabake's model space (the body's)
// is +x left, +y up, +z forward; KH2's world +x is the actor's right at yaw 0, so W(p) = (-p.x, -p.y, p.z) in model
// space, a bound element gets RotY(pi) of that, and the model point goes into the world exactly like Sora's
// vertices (sora_draw.c skin_publish: RotY(yaw) * scale + the owner's position). SAVAGEWIND_SORA_FX_FLIPZ=1 mirrors
// z if the Blizzard shard turns out to fly backwards.
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>  // (s136) map_all / fx_prefetch
#endif

#include "nvn/vulkan/vk.h"
#include "nvn/vulkan/record.h"
// (Alpha 4.1 port) Every command goes through upstream's recorder thread (record.h: rec_Cmd* = rec_Cmd* in call order),
// like exec.c's own and the helpers this file calls (vk_full_barrier, vk_image_init_layout), so they stay in order.
// The recorder carries one descriptor per write: a write of n descriptors becomes n writes (bindings dstBinding + i).
static inline void mod_update_set(const VkWriteDescriptorSet *w) {
    VkWriteDescriptorSet s[4];
    uint32_t n = w->descriptorCount < 4 ? w->descriptorCount : 4;
    for (uint32_t i = 0; i < n; i++) {
        s[i] = *w;
        s[i].dstBinding = w->dstBinding + i, s[i].dstArrayElement = 0, s[i].descriptorCount = 1;
        if (w->pImageInfo) s[i].pImageInfo = w->pImageInfo + i;
        if (w->pBufferInfo) s[i].pBufferInfo = w->pBufferInfo + i;
    }
    rec_UpdateDescriptorSets(n, s);
}
#include "sora_fx.h"
#include "sora_path.h"  // (Deck port: D:/SoraBotW -> SORA_ROOT)
unsigned sora_push_limit(void);  // (s183 amd, sora_draw.c: the GPU's maxPushConstantsSize, or SAVAGEWIND_SORA_PUSH_LIMIT's)

bool linkpos_camera_view(float m[12]);

#define FX_SLOTS 32
#define FX_RING 64          // owner poses kept per element (PHF2 age: frames since the pose a vertex follows)
#define FX_MAX_DRAWS 512
#define FX_MAX_VERTS 65536  // per frame, all elements (fire_1.2's biggest frame is 840 verts; cure_1.3's 1038)
#define TRAIL_SAMPLES 24
#define TRAIL_LIFE 9.0f     // KH frames a ribbon sample lives (GUESS: KH2's trail is short, ~1/6 s)
#define FX_ALPHA_REF 0.002f // KH2's alpha test (PHF2 sampler.alphaRef of every chosen batch)

// ------------------------------------------------------------------ the pack
typedef struct { uint32_t w, h, off, pad; } FTex;
typedef struct { char name[32]; uint32_t first_frame, frames, loops, pax; int32_t bone; uint32_t fade, max_verts, pad; } FEff;
typedef struct { uint32_t first_batch, n; } FFrame;
typedef struct { uint16_t tex; uint8_t flags, blend, clamp, pad[3]; uint32_t first, n; float upright[9]; } FBatch;
typedef struct { float p[3], off[3], uv[2]; uint16_t age, pad; uint8_t rgba[4]; } FVert;
typedef struct { uint32_t slot, start, end; } FTrail;

static uint8_t *g_pack;
static bool g_mapped;  // (s136) g_pack is a file view (map_all), not a malloc
static uint32_t g_ntex, g_neff, g_nframes, g_nbatch, g_nvert, g_ntrail;
static const FTex *g_tex;
static const FEff *g_eff;
static const FFrame *g_frame;
static const FBatch *g_batch;
static const FVert *g_vert;
static const FTrail *g_trail;
static const uint8_t *g_pix;
static int g_state;  // 0 not loaded, 1 ready, -1 off
static bool g_log, g_flipz, g_opaque, g_trail_on = true;
static int g_trail_smooth = 1;  // (s97 b11-trail) ribbon smoothing: 0 straight quads, 1 adaptive spline, 2..8 fixed sub-steps
static float g_bias = 0.05f;

// (s136, user chose "option 1") fx.bin is MAPPED, not read: the old malloc + fread of the whole file held all of it in RAM
// (921 MB with Chicken Little) and its 32-bit long size stopped at 2 GB. A copy-on-write view of the file: Windows pages in only
// what is drawn (the recorded data is unchanged, so every effect looks exactly as before). On spawn the element's vertex range
// gets a PrefetchVirtualMemory hint (Win8+, looked up at run time; no hint = plain page faults). Env SAVAGEWIND_SORA_FX_MAP=0 =
// the old full read. The format's own uint32 section offsets still cap fx.bin at 4 GB (writers + header v2 when it gets close).
static uint8_t *map_all(const char *path, size_t *n) {
#ifdef _WIN32
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    LARGE_INTEGER sz;
    uint8_t *b = NULL;
    if (GetFileSizeEx(f, &sz) && sz.QuadPart > 64) {
        HANDLE m = CreateFileMappingA(f, NULL, PAGE_WRITECOPY, 0, 0, NULL);
        if (m) {
            b = (uint8_t *)MapViewOfFile(m, FILE_MAP_COPY, 0, 0, 0);
            CloseHandle(m);  // (the view keeps the mapping alive)
        }
        if (b) *n = (size_t)sz.QuadPart;
    }
    CloseHandle(f);
    return b;
#else
    (void)path, (void)n;
    return NULL;
#endif
}

static void fx_prefetch(int id) {
#ifdef _WIN32
    typedef struct { void *va; size_t n; } Range;  // = WIN32_MEMORY_RANGE_ENTRY
    typedef BOOL(WINAPI * PrefetchFn)(HANDLE, ULONG_PTR, Range *, ULONG);
    static PrefetchFn fn;
    static int looked;
    if (!looked) looked = 1, fn = (PrefetchFn)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "PrefetchVirtualMemory");
    if (!fn || !g_mapped) return;
    const FEff *e = &g_eff[id];
    if (!e->frames || e->first_frame + e->frames > g_nframes) return;
    const FFrame *f0 = &g_frame[e->first_frame], *f1 = &g_frame[e->first_frame + e->frames - 1];
    if (!f0->n || !f1->n || f0->first_batch >= g_nbatch || f1->first_batch + f1->n > g_nbatch) return;
    uint32_t v0 = g_batch[f0->first_batch].first;
    const FBatch *bl = &g_batch[f1->first_batch + f1->n - 1];
    uint64_t v1 = (uint64_t)bl->first + bl->n;
    if (v1 <= v0 || v1 > g_nvert) return;  // (not laid out in order: no hint)
    Range r = {(void *)(g_vert + v0), (size_t)(v1 - v0) * sizeof(FVert)};
    fn(GetCurrentProcess(), 1, &r, 0);
#else
    (void)id;
#endif
}

static uint8_t *read_all(const char *path, long *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = *n > 64 ? malloc((size_t)*n) : NULL;
    if (b && fread(b, 1, (size_t)*n, f) != (size_t)*n) free(b), b = NULL;
    fclose(f);
    return b;
}

static bool pack_load(void) {
    const char *e = getenv("SAVAGEWIND_SORA_FX");
    if (e && *e && !strcmp(e, "0")) { LOG("[sora-fx] off (SAVAGEWIND_SORA_FX=0)\n"); return false; }
    const char *path = sora_path(e && *e ? e : "D:/SoraBotW/data/fx/fx.bin");
    size_t n = 0;
    const char *em = getenv("SAVAGEWIND_SORA_FX_MAP");
    if (!(em && !strcmp(em, "0"))) g_pack = map_all(path, &n), g_mapped = g_pack != NULL;
    if (!g_pack) {
        long ln = 0;
        g_pack = read_all(path, &ln);
        n = ln > 0 ? (size_t)ln : 0;
    }
    if (g_pack) LOG("[sora-fx] %s: %.0f MB, %s\n", path, n / 1048576.0, g_mapped ? "mapped (paged in on use)" : "read into RAM");
    if (!g_pack || memcmp(g_pack, "SKFX", 4)) { LOG("[sora-fx] no effect pack at %s: no KH2 effects\n", path); return false; }
    const uint32_t *h = (const uint32_t *)g_pack;
    if (h[1] != 1) { LOG("[sora-fx] %s: version %u, want 1\n", path, h[1]); return false; }
    g_ntex = h[2], g_neff = h[3], g_nframes = h[4], g_nbatch = h[5], g_nvert = h[6], g_ntrail = h[7];
    uint32_t off_tex = h[8], off_eff = h[9], off_frame = h[10], off_batch = h[11], off_vert = h[12], off_trail = h[13], off_pix = h[14], pix_bytes = h[15];
    if ((size_t)off_pix + pix_bytes > (size_t)n || off_vert + (size_t)g_nvert * sizeof(FVert) > (size_t)n) {
        LOG("[sora-fx] %s: truncated\n", path);
        return false;
    }
    g_tex = (const FTex *)(g_pack + off_tex), g_eff = (const FEff *)(g_pack + off_eff), g_frame = (const FFrame *)(g_pack + off_frame);
    g_batch = (const FBatch *)(g_pack + off_batch), g_vert = (const FVert *)(g_pack + off_vert), g_trail = (const FTrail *)(g_pack + off_trail);
    g_pix = g_pack + off_pix;
    g_log = getenv("SAVAGEWIND_SORA_FX_LOG") != NULL;
    e = getenv("SAVAGEWIND_SORA_FX_FLIPZ");
    g_flipz = e && atoi(e);
    e = getenv("SAVAGEWIND_SORA_FX_OPAQUE");
    g_opaque = e && atoi(e);
    e = getenv("SAVAGEWIND_SORA_FX_BIAS");
    if (e && *e) g_bias = strtof(e, NULL);
    e = getenv("SAVAGEWIND_SORA_FX_TRAIL");
    if (e && *e && !atoi(e)) g_trail_on = false;
    e = getenv("SAVAGEWIND_SORA_FX_TRAIL_SMOOTH");  // (s97 b11-trail)
    if (e && *e) sora_fx_trail_smooth(atoi(e));
    LOG("[sora-fx] %s: %u elements, %u frames, %u batches, %u verts, %u textures, %u trail windows%s%s\n", path, g_neff, g_nframes,
        g_nbatch, g_nvert, g_ntex, g_ntrail, g_flipz ? ", z mirrored" : "", g_trail_on ? "" : ", no ribbon");
    return true;
}

static bool ready(void) {
    if (!g_state) g_state = pack_load() ? 1 : -1;
    return g_state == 1;
}

bool sora_fx_ready(void) { return ready(); }

int sora_fx_id(const char *name) {
    if (!ready() || !name) return -1;
    for (uint32_t i = 0; i < g_neff; i++)
        if (!strncmp(g_eff[i].name, name, 32)) return (int)i;
    static char warned[8][32];
    static int nwarn;
    for (int i = 0; i < nwarn; i++) if (!strncmp(warned[i], name, 32)) return -1;
    if (nwarn < 8) strncpy(warned[nwarn++], name, 31);
    LOG("[sora-fx] unknown element \"%s\"\n", name);
    return -1;
}

// ------------------------------------------------------------------ live elements (game thread)
typedef struct {
    bool on, loops, fading;
    int eff;
    float frame, alpha, fade, rate, scale;
    uint32_t serial;
    float ring[FX_RING][4];  // owner pose per frame: x y z yaw; ring[head] = this frame's
    int head, filled;
} Inst;
static Inst g_inst[FX_SLOTS];
static uint32_t g_serial;

static void inst_push_pose(Inst *in, const float pos[3], float yaw) {
    in->head = (in->head + 1) % FX_RING;
    in->ring[in->head][0] = pos[0], in->ring[in->head][1] = pos[1], in->ring[in->head][2] = pos[2], in->ring[in->head][3] = yaw;
    if (in->filled < FX_RING) in->filled++;
}

int sora_fx_spawn(int id, const float pos[3], float yaw, float scale) {
    if (!ready() || id < 0 || id >= (int)g_neff || !pos) return -1;
    int i = -1;
    for (int k = 0; k < FX_SLOTS && i < 0; k++) if (!g_inst[k].on) i = k;
    if (i < 0) {  // all busy: the oldest-running one-shot gives way (a loop only if nothing else)
        float best = -1;
        for (int k = 0; k < FX_SLOTS; k++)
            if (!g_inst[k].loops && g_inst[k].frame > best) best = g_inst[k].frame, i = k;
        if (i < 0) i = 0;
        if (g_log) LOG("[sora-fx] evict slot %d (%s) for %s\n", i, g_eff[g_inst[i].eff].name, g_eff[id].name);
    }
    Inst *in = &g_inst[i];
    memset(in, 0, sizeof *in);
    in->on = true, in->eff = id, in->loops = g_eff[id].loops != 0, in->alpha = 1, in->fade = 1, in->scale = scale;
    in->serial = ++g_serial;
    in->head = -1;
    inst_push_pose(in, pos, yaw);
    fx_prefetch(id);  // (s136 map) start paging its frames in now
    if (g_log) LOG("[sora-fx] spawn %s at %.2f %.2f %.2f yaw %.0f (slot %d, %u frames%s)\n", g_eff[id].name, pos[0], pos[1], pos[2],
                   yaw * 57.29578f, i, g_eff[id].frames, in->loops ? ", loop" : "");
    return i;
}

int sora_fx_event(const char *name, const float pos[3], float yaw, float scale) { return sora_fx_spawn(sora_fx_id(name), pos, yaw, scale); }

bool sora_fx_alive(int h) { return h >= 0 && h < FX_SLOTS && g_inst[h].on; }

void sora_fx_move(int h, const float pos[3], float yaw) {
    if (!sora_fx_alive(h) || !pos) return;
    Inst *in = &g_inst[h];
    in->ring[in->head][0] = pos[0], in->ring[in->head][1] = pos[1], in->ring[in->head][2] = pos[2], in->ring[in->head][3] = yaw;
}

void sora_fx_kill(int h) {
    if (!sora_fx_alive(h)) return;
    if (g_log) LOG("[sora-fx] kill %s (slot %d)\n", g_eff[g_inst[h].eff].name, h);
    g_inst[h].on = false;
}

void sora_fx_fadeout(int h, float frames) {
    if (!sora_fx_alive(h)) return;
    if (frames <= 0) { sora_fx_kill(h); return; }
    Inst *in = &g_inst[h];
    in->fading = true, in->rate = -in->fade / frames;  // KH2 re:0x2BE630: fade += rate per frame, alpha *= fade while > 0
}

void sora_fx_clear(void) {
    for (int k = 0; k < FX_SLOTS; k++) g_inst[k].on = false;
}

int sora_fx_count(void) {
    int n = 0;
    for (int k = 0; k < FX_SLOTS; k++) n += g_inst[k].on;
    return n;
}

// ------------------------------------------------------------------ the ribbon (game thread)
// (s89 kbfx) TRAIL_N ribbons (0 = the main blade, 1 = Valor's left blade), each with its own style (sora_fx_trail_ex)
// (mp2-perf, multiplayer slice 2) 4: 2 / 3 = the remote player's blades 0 / 1 (the DLL asks sora_fx_trail_slots)
#define TRAIL_N 8  // (mp4: 4 players x 2 blades; slot k uses ribbons 2 + 2k, 3 + 2k)
typedef struct { float grip[3], tip[3], age; bool used; } TSample;
static TSample g_ts[TRAIL_N][TRAIL_SAMPLES];
static int g_ts_head[TRAIL_N] = {-1, -1, -1, -1, -1, -1, -1, -1}, g_ts_n[TRAIL_N];
static bool g_ts_fresh[TRAIL_N];  // a sample arrived this frame
static int g_ts_style[TRAIL_N] = {-1, -1, -1, -1, -1, -1, -1, -1}, g_ts_flags[TRAIL_N];

int sora_fx_trail_slots(void) { return TRAIL_N; }  // (DLL, by name: how many ribbons sora_fx_trail_ex takes; an older exe has 2 / no export)
void sora_fx_trail_ex(int r, const float grip[3], const float tip[3], bool on, int style, int flags) {
    if (!ready() || !g_trail_on || r < 0 || r >= TRAIL_N) return;
    g_ts_style[r] = style >= 0 && (uint32_t)style < g_neff ? style : -1, g_ts_flags[r] = flags;
    if (!on || !grip || !tip) return;
    g_ts_head[r] = (g_ts_head[r] + 1) % TRAIL_SAMPLES;
    TSample *s = &g_ts[r][g_ts_head[r]];
    memcpy(s->grip, grip, sizeof s->grip), memcpy(s->tip, tip, sizeof s->tip);
    s->age = 0, s->used = true;
    if (g_ts_n[r] < TRAIL_SAMPLES) g_ts_n[r]++;
    g_ts_fresh[r] = true;
}

// (s97 b11-trail) n: 0 = the old straight quads, 1 = adaptive spline (default), 2..8 = that many sub-steps per segment
void sora_fx_trail_smooth(int n) { g_trail_smooth = n < 0 ? 0 : n > 8 ? 8 : n; }

void sora_fx_trail(const float grip[3], const float tip[3], bool on) { sora_fx_trail_ex(0, grip, tip, on, -1, 0); }

bool sora_fx_trail_window(int slot, float clock) {
    if (!ready()) return false;
    for (uint32_t i = 0; i < g_ntrail; i++)
        if ((int)g_trail[i].slot == slot && clock >= (float)g_trail[i].start && (g_trail[i].end == 0xFFFFFFFFu || clock <= (float)g_trail[i].end))
            return true;
    return false;
}

// ------------------------------------------------------------------ the frame's triangles (game thread -> render thread)
typedef struct { float x, y, z, u, v; uint8_t rgba[4]; } DVert;  // 24 bytes, sora_fx's vertex layout
typedef struct { uint16_t tex; uint8_t blend, clamp; uint32_t first, n; } Draw;
typedef struct { DVert *v; Draw *d; uint32_t nv, nd; } DList;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static DList g_local, g_shared;  // the game thread's build buffer, the published one

static bool lists_alloc(void) {
    if (g_local.v) return true;
    g_local.v = malloc(sizeof(DVert) * FX_MAX_VERTS), g_local.d = malloc(sizeof(Draw) * FX_MAX_DRAWS);
    g_shared.v = malloc(sizeof(DVert) * FX_MAX_VERTS), g_shared.d = malloc(sizeof(Draw) * FX_MAX_DRAWS);
    return g_local.v && g_local.d && g_shared.v && g_shared.d;
}

static Draw *draw_begin(DList *l, uint16_t tex, uint8_t blend, uint8_t clamp) {
    if (l->nd && l->d[l->nd - 1].tex == tex && l->d[l->nd - 1].blend == blend && l->d[l->nd - 1].clamp == clamp) return &l->d[l->nd - 1];
    if (l->nd >= FX_MAX_DRAWS) return NULL;
    Draw *d = &l->d[l->nd++];
    d->tex = tex, d->blend = blend, d->clamp = clamp, d->first = l->nv, d->n = 0;
    return d;
}

// One element's frame into the list. view = the camera's 3x4 view rows (world -> camera, GL axes: x right, y up,
// z back); a recorded camera-space offset (KH: x right, y down, z forward) becomes the world vector R^T (ox, -oy, -oz).
static void emit_inst(DList *l, const Inst *in, const float view[12]) {
    const FEff *e = &g_eff[in->eff];
    int f = (int)in->frame;
    if (f < 0) f = 0;
    if (f >= (int)e->frames) f = (int)e->frames - 1;
    const FFrame *fr = &g_frame[e->first_frame + f];
    bool bound = (e->pax & 1) && !(e->pax & 4);
    float a_mul = in->alpha;
    for (uint32_t b = 0; b < fr->n; b++) {
        const FBatch *bt = &g_batch[fr->first_batch + b];
        if (bt->first + bt->n > g_nvert || l->nv + bt->n > FX_MAX_VERTS) break;
        // (s40) blend 2 (KH2 blending off / dst ZERO): only cure_flower / cure_sparkle's two 378-vert shells on tex 12
        // (dark green, alpha ~254). Drawn they cover the screen around him in opaque dark green (s40_cf_burst.png);
        // in KH2 they presumably go to another target. Skipped unless SAVAGEWIND_SORA_FX_OPAQUE=1.
        if (bt->blend == 2 && !g_opaque) continue;
        Draw *d = draw_begin(l, bt->tex, bt->blend, bt->clamp);
        if (!d) break;
        for (uint32_t k = 0; k < bt->n; k++) {
            const FVert *p = &g_vert[bt->first + k];
            int age = p->age;
            if (age >= in->filled) age = in->filled - 1;
            const float *o = in->ring[(in->head - age + FX_RING) % FX_RING];
            float s = sinf(o[3]), c = cosf(o[3]);
            // KH2 effect space -> sorabake model space (x left, y up, z fwd), bound elements turned by pi
            float mx = -p->p[0], my = -p->p[1], mz = g_flipz ? -p->p[2] : p->p[2];
            if (bound) mx = -mx, mz = -mz;
            float wx = o[0] + (c * mx + s * mz) * in->scale, wy = o[1] + my * in->scale, wz = o[2] + (-s * mx + c * mz) * in->scale;
            if (bt->flags & 1) {  // camera-space offset (flags 4 / 8: approximated as camera space, see the report)
                float ox = p->off[0] * in->scale, oy = -p->off[1] * in->scale, oz = -p->off[2] * in->scale;
                wx += view[0] * ox + view[4] * oy + view[8] * oz;
                wy += view[1] * ox + view[5] * oy + view[9] * oz;
                wz += view[2] * ox + view[6] * oy + view[10] * oz;
            }
            DVert *v = &l->v[l->nv++];
            v->x = wx, v->y = wy, v->z = wz, v->u = p->uv[0], v->v = p->uv[1];
            v->rgba[0] = p->rgba[0], v->rgba[1] = p->rgba[1], v->rgba[2] = p->rgba[2];
            v->rgba[3] = (uint8_t)(p->rgba[3] * a_mul + 0.5f);
            d->n++;
        }
    }
}

// (s97 b11-trail) The samples come one per game frame (~30 fps), so a blade turning ~60 deg a frame drew a hexagon.
// catmull_cr: centripetal (alpha 0.5) Catmull-Rom through c[1]..c[2], s 0..1 between them (Barry-Goldman pyramid).
// A knot distance ~0 (coincident points) falls back to uniform knots for that curve.
static void catmull_cr(const float c[4][3], float s, float out[3]) {
    float t[4] = {0, 0, 0, 0};
    bool deg = false;
    for (int k = 1; k < 4; k++) {
        float dx = c[k][0] - c[k - 1][0], dy = c[k][1] - c[k - 1][1], dz = c[k][2] - c[k - 1][2];
        float d = sqrtf(sqrtf(dx * dx + dy * dy + dz * dz));  // |d|^0.5
        if (d < 1e-3f) deg = true;  // |d| < 1e-6 u
        t[k] = t[k - 1] + d;
    }
    if (deg) for (int k = 1; k < 4; k++) t[k] = (float)k;
    float tt = t[1] + (t[2] - t[1]) * s;
    float a[3][3], b[2][3];
    for (int k = 0; k < 3; k++) {
        float w = (tt - t[k]) / (t[k + 1] - t[k]);
        for (int m = 0; m < 3; m++) a[k][m] = c[k][m] + (c[k + 1][m] - c[k][m]) * w;
    }
    for (int k = 0; k < 2; k++) {
        float w = (tt - t[k]) / (t[k + 2] - t[k]);
        for (int m = 0; m < 3; m++) b[k][m] = a[k][m] + (a[k + 1][m] - a[k][m]) * w;
    }
    float w = (tt - t[1]) / (t[2] - t[1]);
    for (int m = 0; m < 3; m++) out[m] = b[0][m] + (b[1][m] - b[0][m]) * w;
}

// Sub-steps for the segment c[1]..c[2] (tip points): fixed n when g_trail_smooth >= 2, else adaptive: one step per
// 0.06 u of tip chord (a Keyblade is ~1 u; GUESS: ~3.4 deg of arc at the tip), capped at 8, and 1 when the path
// is nearly straight (turn at both ends < 0.1 rad) so a straight swing stays two triangles.
static int trail_substeps(const float c[4][3]) {
    if (g_trail_smooth <= 0) return 1;
    if (g_trail_smooth >= 2) return g_trail_smooth;
    float d01[3], d12[3], d23[3];
    for (int m = 0; m < 3; m++) d01[m] = c[1][m] - c[0][m], d12[m] = c[2][m] - c[1][m], d23[m] = c[3][m] - c[2][m];
    float l12 = sqrtf(d12[0] * d12[0] + d12[1] * d12[1] + d12[2] * d12[2]);
    float l01 = sqrtf(d01[0] * d01[0] + d01[1] * d01[1] + d01[2] * d01[2]), l23 = sqrtf(d23[0] * d23[0] + d23[1] * d23[1] + d23[2] * d23[2]);
    float c1 = l01 * l12 > 1e-9f ? (d01[0] * d12[0] + d01[1] * d12[1] + d01[2] * d12[2]) / (l01 * l12) : 1;
    float c2 = l12 * l23 > 1e-9f ? (d12[0] * d23[0] + d12[1] * d23[1] + d12[2] * d23[2]) / (l12 * l23) : 1;
    if (c1 > 0.995f && c2 > 0.995f) return 1;  // cos 0.1 rad
    int n = (int)ceilf(l12 / 0.06f);
    return n < 1 ? 1 : n > 8 ? 8 : n;
}

// The ribbon: quads between consecutive samples (newest first), u = the sample's age / TRAIL_LIFE (the texture fades
// along u), v 0 at the grip .. 1 at the tip, additive white.
// (s89 kbfx) style >= 0 = a Keyblade's "kb<xx>_trail" element (KH2 W_EX010_<xx>.a.us element 10, the weapon's own
// ribbon): its first batch's texture + blend and its first vertex's colour (KH2's, x 1.99) instead of the gradient and
// white. Those textures are bright at the top (KH2 v 0) and the left (u 0): v 0 = the tip (GUESS from the texture
// shapes: Bond of Flame's flames run along the top edge), flags bit 0 = v 0 at the grip instead.
static void emit_trail(DList *l, float kh, int r) {
    if (g_ts_n[r] < 2) return;
    uint16_t tex = (uint16_t)(g_ntex - 1);  // the bake's last texture = the ribbon gradient
    uint8_t blend = 0, cr = 255, cg = 255, cb = 255;
    float vg = 0, vt = 1;
    int st = g_ts_style[r];
    if (st >= 0 && g_eff[st].frames > 0 && g_eff[st].first_frame < g_nframes) {  // (s89 review: bounds, a bad pack)
        const FFrame *fr = &g_frame[g_eff[st].first_frame];
        if (fr->n > 0 && fr->first_batch < g_nbatch) {
            const FBatch *bt = &g_batch[fr->first_batch];
            if (bt->tex != 0xFFFF && bt->tex < g_ntex && bt->n > 0 && bt->first < g_nvert) {
                const FVert *p = &g_vert[bt->first];
                tex = bt->tex, blend = bt->blend == 1 ? 1 : 0, cr = p->rgba[0], cg = p->rgba[1], cb = p->rgba[2];
                if (!(g_ts_flags[r] & 1)) vg = 1, vt = 0;
            }
        }
    }
    Draw *d = draw_begin(l, tex, blend, 1);
    if (!d) return;
    for (int i = 0; i + 1 < g_ts_n[r]; i++) {
        const TSample *a = &g_ts[r][(g_ts_head[r] - i + TRAIL_SAMPLES) % TRAIL_SAMPLES], *b = &g_ts[r][(g_ts_head[r] - i - 1 + TRAIL_SAMPLES) % TRAIL_SAMPLES];
        if (!a->used || !b->used || b->age >= TRAIL_LIFE || l->nv + 6 > FX_MAX_VERTS) break;
        // (s97 b11-trail) spline neighbours: i-1 (newer) and i+2 (older); a phantom 2*end - neighbour at the ends
        const TSample *n0 = i > 0 ? &g_ts[r][(g_ts_head[r] - i + 1 + TRAIL_SAMPLES) % TRAIL_SAMPLES] : NULL;
        const TSample *n3 = i + 2 < g_ts_n[r] ? &g_ts[r][(g_ts_head[r] - i - 2 + TRAIL_SAMPLES) % TRAIL_SAMPLES] : NULL;
        if (n0 && !n0->used) n0 = NULL;
        if (n3 && !n3->used) n3 = NULL;
        if (n3 && n3->age - b->age > 6.0f) n3 = NULL;  // (s97 review) a stale sample of an earlier swing, not b's neighbour
        float gc[4][3], tc[4][3];  // grip / tip control points: newer neighbour, a, b, older neighbour
        for (int k = 0; k < 3; k++) {
            gc[1][k] = a->grip[k], gc[2][k] = b->grip[k], tc[1][k] = a->tip[k], tc[2][k] = b->tip[k];
            gc[0][k] = n0 ? n0->grip[k] : 2 * a->grip[k] - b->grip[k], tc[0][k] = n0 ? n0->tip[k] : 2 * a->tip[k] - b->tip[k];
            gc[3][k] = n3 ? n3->grip[k] : 2 * b->grip[k] - a->grip[k], tc[3][k] = n3 ? n3->tip[k] : 2 * b->tip[k] - a->tip[k];
        }
        int ns = trail_substeps(tc);
        float ua = a->age / TRAIL_LIFE, ub = b->age / TRAIL_LIFE;
        float g0[3] = {a->grip[0], a->grip[1], a->grip[2]}, t0[3] = {a->tip[0], a->tip[1], a->tip[2]}, u0 = ua;
        uint8_t al0 = (uint8_t)(255 * (1 - ua));
        for (int j = 1; j <= ns; j++) {
            if (l->nv + 6 > FX_MAX_VERTS) break;
            float g1[3], t1[3], u1;
            if (j == ns) memcpy(g1, b->grip, sizeof g1), memcpy(t1, b->tip, sizeof t1), u1 = ub;  // ends exactly on b
            else {
                float s = (float)j / (float)ns;
                catmull_cr(gc, s, g1), catmull_cr(tc, s, t1);
                u1 = ua + (ub - ua) * s;  // age (u and alpha) linear across the sub-steps
            }
            uint8_t al1 = (uint8_t)(255 * (1 - u1));
            DVert q[4] = {{g0[0], g0[1], g0[2], u0, vg, {cr, cg, cb, al0}}, {t0[0], t0[1], t0[2], u0, vt, {cr, cg, cb, al0}},
                          {g1[0], g1[1], g1[2], u1, vg, {cr, cg, cb, al1}}, {t1[0], t1[1], t1[2], u1, vt, {cr, cg, cb, al1}}};
            l->v[l->nv++] = q[0], l->v[l->nv++] = q[1], l->v[l->nv++] = q[2];
            l->v[l->nv++] = q[1], l->v[l->nv++] = q[3], l->v[l->nv++] = q[2];
            d->n += 6;
            memcpy(g0, g1, sizeof g0), memcpy(t0, t1, sizeof t0), u0 = u1, al0 = al1;
        }
    }
    (void)kh;
}

void sora_fx_tick(float kh) {
    if (!ready() || !lists_alloc()) return;
    if (kh < 0) kh = 0;
    // KH2 re:0x2BD190: once per game frame the fade rate applies, then the frame advances; a loop wraps
    for (int k = 0; k < FX_SLOTS; k++) {
        Inst *in = &g_inst[k];
        if (!in->on) continue;
        const FEff *e = &g_eff[in->eff];
        int steps = (int)(in->frame + kh) - (int)in->frame;
        for (int s = 0; s < steps; s++) {
            if (in->rate != 0) {
                in->fade += in->rate;
                if (in->fade < 0) in->fade = 0;
                if (in->fade > 0) in->alpha *= in->fade;
            }
            // this frame's owner pose carries on to the next frame (sora_fx_move overwrites the newest entry)
            inst_push_pose(in, in->ring[in->head], in->ring[in->head][3]);
        }
        in->frame += kh;
        if (in->fading && in->fade <= 0) { in->on = false; continue; }
        if (in->frame >= (float)e->frames) {
            if (in->loops && e->frames > 0) in->frame = fmodf(in->frame, (float)e->frames);
            else {
                if (g_log) LOG("[sora-fx] %s ended (slot %d)\n", e->name, k);
                in->on = false;
                continue;
            }
        }
    }
    for (int r = 0; r < TRAIL_N; r++) {
        for (int i = 0; i < g_ts_n[r]; i++) {
            TSample *s = &g_ts[r][i];
            if (s->used && !(g_ts_fresh[r] && i == g_ts_head[r])) s->age += kh;  // (the sample pushed this frame starts at 0)
        }
        g_ts_fresh[r] = false;
    }
    float view[12];
    bool have_view = linkpos_camera_view(view);
    g_local.nv = g_local.nd = 0;
    if (have_view) {
        for (int k = 0; k < FX_SLOTS; k++) if (g_inst[k].on) emit_inst(&g_local, &g_inst[k], view);
        if (g_trail_on) for (int r = 0; r < TRAIL_N; r++) emit_trail(&g_local, kh, r);
    }
    pthread_mutex_lock(&g_lock);
    memcpy(g_shared.v, g_local.v, sizeof(DVert) * g_local.nv);
    memcpy(g_shared.d, g_local.d, sizeof(Draw) * g_local.nd);
    g_shared.nv = g_local.nv, g_shared.nd = g_local.nd;
    pthread_mutex_unlock(&g_lock);
}

// ------------------------------------------------------------------ the GPU side (render thread)
// The same push constants as sora_draw.c (mvp; k = (alpha ref, blended, scene depth mode, debug); vp; duv; z = (near,
// far, bias, -)), the same scene-depth test. The vertex carries its colour: KH2's vertex colour x the texture.
#define PC_DECL "layout(push_constant) uniform P { mat4 mvp; vec4 k; vec4 vp; vec4 duv; vec4 z; } pc;\n"
static const char *VERT_SRC =
    "#version 450\n"
    "layout(location = 0) in vec3 pos;\n"
    "layout(location = 1) in vec2 uv;\n"
    "layout(location = 2) in vec4 col;\n"
    PC_DECL
    "layout(location = 0) out vec2 v_uv;\n"
    "layout(location = 1) out vec4 v_col;\n"
    "void main() { v_uv = uv; v_col = col; gl_Position = pc.mvp * vec4(pos, 1.0); }\n";
static const char *FRAG_SRC =
    "#version 450\n"
    "layout(set = 0, binding = 0) uniform sampler2D tex;\n"
    "layout(set = 0, binding = 1) uniform sampler2D sdepth;\n"
    PC_DECL
    "layout(location = 0) in vec2 v_uv;\n"
    "layout(location = 1) in vec4 v_col;\n"
    "layout(location = 0) out vec4 o;\n"
    "void main() {\n"
    "    vec4 c = texture(tex, v_uv) * v_col;\n"
    "    if (c.a <= pc.k.x) discard;\n"
    "    if (pc.k.z > 0.5) {\n"
    "        float ze = 1.0 / gl_FragCoord.w;\n"
    "        vec2 s = (gl_FragCoord.xy - pc.vp.xy) / pc.vp.zw;\n"
    "        float d = texture(sdepth, pc.duv.xy + s * pc.duv.zw).r, n = pc.z.x, f = pc.z.y;\n"
    "        float zs = pc.k.z < 1.5 ? 2.0 * n * f / (f + n - (2.0 * d - 1.0) * (f - n)) : n * f / (f - d * (f - n));\n"
    "        if (ze > zs + pc.z.z + ze * 0.002) discard;\n"
    "    }\n"
    "    o = c;\n"
    "}\n";

static struct {
    bool init, failed;
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout layout;
    // per colour format (s57: the window image's, the game image's): 0 additive (src alpha / one), 1 alpha (src alpha /
    // inv src alpha)
    struct { VkFormat fmt; VkPipeline p[2]; } pipes[2];
    VkSampler wrap, clamp;
    VkImage *tex;
    VmaAllocation *tex_mem;
    VkImageView *tex_view;
    uint32_t ntex;  // the pack's + 1 (a white 2x2 for untextured batches)
    DList list;
    uint64_t drawn;
} G;

static VkPipeline make_pipeline(VkFormat fmt, int blend) {
    static VkShaderModule vs, fs;
    if (!vs) vs = vk_compile_glsl(VERT_SRC, STAGE_VERTEX, "sora_fx.vert");
    if (!fs) fs = vk_compile_glsl(FRAG_SRC, STAGE_FRAGMENT, "sora_fx.frag");
    if (!vs || !fs) return VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main"}};
    VkVertexInputBindingDescription bind = {0, sizeof(DVert), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attr[3] = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, {1, 0, VK_FORMAT_R32G32_SFLOAT, 12},
                                                 {2, 0, VK_FORMAT_R8G8B8A8_UNORM, 20}};
    VkPipelineVertexInputStateCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                               .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &bind,
                                               .vertexAttributeDescriptionCount = 3, .pVertexAttributeDescriptions = attr};
    VkPipelineInputAssemblyStateCreateInfo ia = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                 .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo vps = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                 .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .lineWidth = 1};
    VkPipelineMultisampleStateCreateInfo ms = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                               .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo ds = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_FALSE,
                                                .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL};
    VkPipelineColorBlendAttachmentState cba = {.blendEnable = VK_TRUE, .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                                               .dstColorBlendFactor = blend ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ONE,
                                               .colorBlendOp = VK_BLEND_OP_ADD, .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                                               .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .alphaBlendOp = VK_BLEND_OP_ADD, .colorWriteMask = 0xF};
    VkPipelineColorBlendStateCreateInfo cb = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &cba};
    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dyn};
    VkFormat df = VK_FORMAT_D32_SFLOAT;  // sora_draw.c DEPTH_FORMAT: we draw inside its rendering
    VkPipelineRenderingCreateInfo ri = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1,
                                        .pColorAttachmentFormats = &fmt, .depthAttachmentFormat = df};
    VkGraphicsPipelineCreateInfo pi = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri, .stageCount = 2,
                                       .pStages = stages, .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vps,
                                       .pRasterizationState = &rs, .pMultisampleState = &ms, .pDepthStencilState = &ds,
                                       .pColorBlendState = &cb, .pDynamicState = &dy, .layout = G.layout};
    VkPipeline p = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(g_vk.dev, g_vk.pcache, 1, &pi, NULL, &p) != VK_SUCCESS) return VK_NULL_HANDLE;
    return p;
}

static bool upload_tex(VkCommandBuffer ucb, uint32_t i, uint32_t w, uint32_t h, const void *bgra) {
    VkImageCreateInfo ic = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_B8G8R8A8_UNORM,
                            .extent = {w, h, 1}, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
                            .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};
    VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    if (vmaCreateImage(g_vk.vma, &ic, &ai, &G.tex[i], &G.tex_mem[i], NULL) != VK_SUCCESS) return false;
    vk_image_init_layout(ucb, G.tex[i], VK_IMAGE_ASPECT_COLOR_BIT);
    VkSpan s = vk_transient_copy(bgra, (uint64_t)w * h * 4, 16);
    VkBufferImageCopy rg = {.bufferOffset = s.off, .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {w, h, 1}};
    rec_CmdCopyBufferToImage(ucb, s.buf, G.tex[i], VK_IMAGE_LAYOUT_GENERAL, 1, &rg);
    VkImageViewCreateInfo vc = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = G.tex[i], .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                .format = VK_FORMAT_B8G8R8A8_UNORM, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    return vkCreateImageView(g_vk.dev, &vc, NULL, &G.tex_view[i]) == VK_SUCCESS;
}

static bool gpu_init(void) {
    if (sora_push_limit() < 128) {  // (s183 amd) mvp k vp duv z = sora_draw.c's push block, exactly Vulkan's minimum
        LOG("[sora-fx] push constants: %u bytes, need 128\n", sora_push_limit());
        return false;
    }
    VkDescriptorSetLayoutBinding b[2] = {{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
                                         {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL}};
    VkDescriptorSetLayoutCreateInfo li = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 2, .pBindings = b};
    if (vkCreateDescriptorSetLayout(g_vk.dev, &li, NULL, &G.set_layout) != VK_SUCCESS) return false;
    VkPushConstantRange r = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128};
    VkPipelineLayoutCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1,
                                     .pSetLayouts = &G.set_layout, .pushConstantRangeCount = 1, .pPushConstantRanges = &r};
    if (vkCreatePipelineLayout(g_vk.dev, &ci, NULL, &G.layout) != VK_SUCCESS) return false;
    VkSamplerCreateInfo si = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR,
                              .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST, .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                              .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT, .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT, .maxLod = 0};
    if (vkCreateSampler(g_vk.dev, &si, NULL, &G.wrap) != VK_SUCCESS) return false;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(g_vk.dev, &si, NULL, &G.clamp) != VK_SUCCESS) return false;
    G.ntex = g_ntex + 1;
    G.tex = calloc(G.ntex, sizeof *G.tex), G.tex_mem = calloc(G.ntex, sizeof *G.tex_mem), G.tex_view = calloc(G.ntex, sizeof *G.tex_view);
    G.list.v = malloc(sizeof(DVert) * FX_MAX_VERTS), G.list.d = malloc(sizeof(Draw) * FX_MAX_DRAWS);
    if (!G.tex || !G.tex_mem || !G.tex_view || !G.list.v || !G.list.d) return false;
    VkCommandBuffer ucb = vk_upload_cmd();
    for (uint32_t i = 0; i < g_ntex; i++)
        if (!upload_tex(ucb, i, g_tex[i].w, g_tex[i].h, g_pix + g_tex[i].off)) return false;
    static const uint8_t white[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    if (!upload_tex(ucb, g_ntex, 2, 2, white)) return false;
    vk_full_barrier(ucb);
    return true;
}

void sora_fx_draw(VkCommandBuffer cb, const float pc_in[32], VkImageView sdepth, VkSampler nearest, VkFormat fmt) {
    if (g_state != 1 || G.failed || !pc_in) return;
    pthread_mutex_lock(&g_lock);
    uint32_t nv = g_shared.nv, nd = g_shared.nd;
    if (G.list.v && nv) memcpy(G.list.v, g_shared.v, sizeof(DVert) * nv);
    if (G.list.d && nd) memcpy(G.list.d, g_shared.d, sizeof(Draw) * nd);
    pthread_mutex_unlock(&g_lock);
    if (!G.init) {
        G.init = true;
        if (!gpu_init()) { G.failed = true; LOG("[sora-fx] GPU setup failed\n"); }
        return;  // (the textures upload with this submission; draw from the next frame on)
    }
    if (!nd || !nv) return;
    const VkPipeline *P = NULL;  // (s57) two formats kept, as sora_draw.c pipes_for (a third replaces the second)
    for (int i = 0; i < 2 && !P; i++)
        if (G.pipes[i].p[0] && G.pipes[i].fmt == fmt) P = G.pipes[i].p;
    if (!P) {
        int i = G.pipes[0].p[0] ? 1 : 0;
        G.pipes[i].fmt = fmt;
        G.pipes[i].p[0] = make_pipeline(fmt, 0), G.pipes[i].p[1] = make_pipeline(fmt, 1);
        if (!G.pipes[i].p[0] || !G.pipes[i].p[1]) { G.failed = true; LOG("[sora-fx] pipelines failed (format %d)\n", fmt); return; }
        P = G.pipes[i].p;
    }
    float pc[32];
    memcpy(pc, pc_in, sizeof pc);
    pc[16] = FX_ALPHA_REF, pc[17] = 1, pc[19] = 0;  // k: alpha ref, blended, (mode kept), no debug
    pc[30] = g_bias, pc[31] = 0;                    // z: (near, far kept), the effects' bias, no shadow mode
    VkSpan vb = vk_transient_copy(G.list.v, (uint64_t)nv * sizeof(DVert), 16);
    VkDeviceSize vo = vb.off;
    rec_CmdBindVertexBuffers(cb, 0, 1, &vb.buf, &vo);
    int cur_pipe = -1;
    uint32_t cur_tex = 0xFFFFFFFFu, cur_clamp = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < nd; i++) {
        const Draw *d = &G.list.d[i];
        if (!d->n) continue;
        int pipe = d->blend == 0 ? 0 : 1;
        if (pipe != cur_pipe) {
            rec_CmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, P[pipe]);
            rec_CmdPushConstants(cb, G.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof pc, pc);
            cur_pipe = pipe;
        }
        uint32_t tex = d->tex < g_ntex ? d->tex : g_ntex;  // 0xFFFF (untextured) -> the white texture
        if (tex != cur_tex || d->clamp != cur_clamp) {
            VkDescriptorSet set = vk_alloc_set(G.set_layout);
            VkDescriptorImageInfo ii[2] = {{d->clamp ? G.clamp : G.wrap, G.tex_view[tex], VK_IMAGE_LAYOUT_GENERAL},
                                           {nearest, sdepth ? sdepth : G.tex_view[tex], VK_IMAGE_LAYOUT_GENERAL}};
            VkWriteDescriptorSet w = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set, .dstBinding = 0, .descriptorCount = 2,
                                      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = ii};
            mod_update_set(&w);
            rec_CmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, G.layout, 0, 1, &set, 0, NULL);
            cur_tex = tex, cur_clamp = d->clamp;
        }
        rec_CmdDraw(cb, d->n, 1, d->first, 0);
    }
    if (G.drawn++ % 300 == 0)
        LOG("[sora-fx] frame %llu: %u draws, %u verts, %d live elements\n", (unsigned long long)G.drawn - 1, nd, nv, sora_fx_count());
}

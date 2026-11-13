// Sora-in-BotW, step 4: Sora's own draw pass. KH2 Sora (P_EX100) from the offline bake (D:/SoraBotW/tools/sorabake:
// sora_model.bin = mesh + textures, sora_pose.bin = bone matrices per motion frame, root move taken out; formats in its
// Program.cs "draw data"), skinned on the CPU at the motion and clock sora.c plays, placed at Link's feet with Sora's
// facing, drawn over the game's final image with a depth buffer of his own (PHH's DRAW_MODE 3: after the post-process,
// over BotW's HUD) from BotW's camera: sead::LookAtCamera's view matrix + sead::PerspectiveProjection's fovy / aspect /
// near / far (both found by linkpos.c's camera scan).
// (s54, party slice 1: S35_HELPER_P_PARTY.md 2.2, S54_HELPER_PARTY_DRAW.md) Draw ACTORS: one bake + skin buffer +
// snapshot + textures each, Sora = actor 0 (the env bake below), Donald / Goofy = sora_draw_actor_load (data/party,
// partybake: the same formats), each placed at its own feet. Shared: the camera, pipelines, samplers, the depth image.
//
//   SAVAGEWIND_SORA_DRAW=0          no draw pass (Sora mode still drives Link; no party either)
//   SAVAGEWIND_SORA_MODEL=<path>    default D:/SoraBotW/data/sora_model.bin
//   SAVAGEWIND_SORA_POSE=<path>     default D:/SoraBotW/data/sora_pose.bin
//   SAVAGEWIND_SORA_FOVY=<deg>      without a projection found: this fovy (default 50), the game image's aspect
//   SAVAGEWIND_SORA_OCCLUDE=0       no test against BotW's scene depth (exec.c vk_scene_depth): Sora over everything
//   SAVAGEWIND_SORA_DEPTH_DEBUG=1   Sora painted grey = scene depth / his depth * 0.5 (0.5: the world at his depth)
//   SAVAGEWIND_SORA_SHADOW=0        no blob shadow (the KH2-style dark disc on the floor under him, see "the shadow")
//   SAVAGEWIND_SORA_SHADOW_DROP=0   no scene-depth decal for the disc (s39 option B): a flat disc at sora.c's floor
//   SAVAGEWIND_SORA_UI_ORDER=0      (s57) the old order: actors over the window image at present, over BotW's HUD and
//                                   popups (default 1: into the game image at BotW's scene composite, under its 2D UI)
//
// Threads: sora_draw_pose runs on the game thread at the end of sora_frame (rt_vfr_update, before the frame's physics:
// Link's matrix is the last physics step's, the camera the last frame's), skins into a local buffer and copies it with
// the camera under g_lock; sora_draw_present (device.c present_locked, the render thread, inside the window image's
// rendering right after the game image is drawn) copies the latest one out and draws it.
// (s57) With the ui order on, the draw happens earlier: exec.c's command-list translation (under gpu_lock, like the
// present) calls sora_draw_scene_pass right after the draw that composites the 3D scene into the final image;
// sora_draw_present then only maps that pass's camera to the window (sora_draw_project).
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <time.h>
#include "sora_draw.h"
#include "sora_path.h"  // (Deck port: D:/SoraBotW -> SORA_ROOT)
#include "sora_fx.h"
#include "game_sites.h"
#include "nvn/output_scale.h"  // (s183 amd FIX 2) output_world_proj: the world camera's projection as built
#include "sora_proj_core.h"

bool linkpos_link_mtx(float m[12]);
bool linkpos_weapon_shown(void);  // (s128 wpn) Link's BotW weapon is in Sora's hand now: hide the Keyblade groups
bool linkpos_camera_view(float m[12]);
bool linkpos_camera_proj(float p[4]);
VkImageView vk_frame_view(void);
VkExtent2D vk_frame_extent(void);
bool vk_scene_depth(VkImageView *view, float uv[4], uint32_t *w, uint32_t *h, uint32_t *draws);
bool vk_sun_cascade(VkImageView *view, float m[16], uint32_t *w, uint32_t *h);  // (s124 b, exec.c)
bool renderer_origin_lower_left(void);
uint32_t nvn_depth_mode(void);
void vk_frame_suspend(void);
void vk_frame_resume(void);

#define DEPTH_FORMAT VK_FORMAT_D32_SFLOAT
#define MAX_TEX 80     // (s85: room for every KH2 Keyblade baked in as alt blades, 1 texture each, both Valor chains)
#define MAX_GROUPS 160
#define SORA_DRAW_TEX_SETS 4096  // (mp2-perf C4) the persistent texture sets' pool (actors x textures; 48 x 80 at most)
#define KB_CHAIN_BONES 12  // (s85) one Keyblade chain's bones (W_EX010 and every W_EX010_xx: sorabake checks)
#define ATTR_WEAPON 0x100  // KH2 group attr: the weapon's groups (Sora's Keyblade 4-5, Donald's staff 5, Goofy's shield 3)
#define WEAPON_BONES_MAX 24  // (s58) sora_draw_actor_weapon_at: the weapon's bones (Sora's Keyblade 12, staff / shield 4)
// (s121 light) floats per vertex: x y z u v + the normal nx ny nz (CPU smooth normals, skin_publish). Every vertex buffer
// (local, snap, ring, gpu, the shadow disc) uses this stride; the shadow disc's normal is 0.
#define VB_F 8
#define LREC_F 48  // (s124 b) floats per instance record: light 16 + sun shadow 12 + cascade matrix 16 + shade colour 4
#define ATTR_NOLIGHT (1u << 30)  // KH2 group attr NoLight (OpenKH): drawn unlit in every light style
// (s215) a weapon group's alt-blade id = attr bits 24..31 WITHOUT the NoLight bit (bit 30 sits inside that byte: our own
// Keyblades' glow groups carry both, and were skipped as "another blade"). Alt ids are 1..63 (sorabake checks it).
#define ATTR_ALT(a) ((a) >> 24 & 0xBFu)

// ------------------------------------------------------------------ the shadow (state; drawn by the render thread)
// A KH2-style blob, a soft dark disc flat on the floor under each actor. KH2 itself has no shadow texture: its mdlx
// carries a low-poly "shadow model" (OpenKh ModelShadow / ShadowGroup, a mesh projected onto the floor), so the disc is
// procedural (sora_draw.frag, radial falloff). The floor height comes from sora.c (sora_draw_shadow: S.ground_z, the
// last grounded height); without a call the floor = the feet, so the disc rides with him in the air until the mod sets
// it. Game thread writes, copied into the snapshot by skin_publish.
typedef struct {
    bool on;           // env SAVAGEWIND_SORA_SHADOW != 0 (g_shadow_on, read once with Sora's bake; set at publish)
    bool set;          // sora_draw_shadow was called at least once (else floor = feet)
    float floor_y;     // world y of the floor under the actor (BotW units)
    float strength;    // alpha at the disc's centre on the ground, 0..1
    float radius;      // disc radius on the ground, BotW units
    float fade_h;      // height above the floor at which the disc reaches min_scale / min_alpha
    float min_scale, min_alpha;
} Shadow;
#define SHADOW_STRENGTH 0.55f  // GUESSES (no KH2 numbers found): see recon/S35_HELPER_S_SHADOW.md
#define SHADOW_RADIUS 0.40f    // ~Sora's standing footprint (idle x -45..34 cm, z -25..44 cm at 0.01 cm -> u)
#define SHADOW_FADE_H 4.0f     // u above the floor: 4 m up the disc is min_scale big and min_alpha * strength dark
#define SHADOW_MIN_SCALE 0.35f
#define SHADOW_MIN_ALPHA 0.30f
#define SHADOW_LIFT 0.015f     // u the disc floats above the floor height (z-fight with flat ground)
#define SHADOW_BIAS 0.10f      // u the scene may be in front of the disc before it hides it (Sora's body: 0.03)
#define SHADOW_SEG 24          // disc segments (a fan: 1 + SHADOW_SEG verts, SHADOW_SEG tris)
#define SHADOW_DROP_MAX 40.0f  // u below his feet the decal still shades the scene (option B, FRAG_SRC z.w 2)
#define SHADOW_DEFAULT {true, false, 0, SHADOW_STRENGTH, SHADOW_RADIUS, SHADOW_FADE_H, SHADOW_MIN_SCALE, SHADOW_MIN_ALPHA}
static bool g_shadow_on = true;

// ------------------------------------------------------------------ the draw actors (s54, party slice 1)
typedef struct { uint32_t w, h, off, addr; } MTex;
typedef struct { uint32_t tex, attr, first, n; } MGroup;
typedef struct { uint32_t first, n; float u, v; } MVert;
typedef struct { uint32_t bone; float p[4]; } MInfl;
typedef struct { uint16_t slot, poses; uint32_t off; } PDir;

typedef struct { bool have_proj; float view[12], proj[4]; bool screen; float clear[4], mask[4], shift[2]; bool co; int proj_src; } CamSnap;
// (s114 drawsync) Roxas looked below 60 fps although his clock, bakes and feet advance every frame (s113 anim60 probe). The scene
// pass runs on the queue thread ~30 ms after the game submitted the frame (recon/S114_HELPER_DRAWLAT.md) and used the NEWEST
// snapshot: depending on the race, a displayed frame got two new poses or none. Now each publish is kept in a ring tagged
// with the game frame (framepace_game_frame), and the scene pass takes the one published for its own list's frame
// (nvn/objects.c nvn_exec_frame_tag + g_sync_off). Modes (DLL "set drawsync N", env SAVAGEWIND_SORA_DRAW_SYNC):
//   0 = old (newest), 1 = tag match (default), 2 = pacing only (each scene pass one snapshot further, never two).
// g_sync_off (DLL "set drawoff N", default 0: s114b user "looks like 60 now", [drawlat] drawn step 1:600 while newest step was 0:118 2:117).
// "[drawlat]" line every 600 scene passes with DLL "set latdiag 1" (default off).
#define DRAW_RING 6
static int g_sync = -1, g_sync_off = 0, g_latdiag = 0;  // (s114b: off 0 + diag off VERIFIED by the user; off 1 missed 5-16% of frames: K+1 not published yet)
uint64_t framepace_game_frame(void);
uint64_t nvn_exec_frame_tag(void);
uint64_t plat_now_ns(void);
void sora_draw_set_sync(int mode, int off, int diag) {  // (DLL, looked up by name; mode / diag < 0, off -99 keep that setting)
    if (mode >= 0 && mode != g_sync) LOG("[sora-draw] drawsync %d (%s)\n", mode, mode == 0 ? "old: newest snapshot" : mode == 1 ? "tag match" : "pacing");
    if (mode >= 0) g_sync = mode > 2 ? 2 : mode;
    if (off != -99 && off >= -4 && off <= 4 && off != g_sync_off) g_sync_off = off, LOG("[sora-draw] drawoff %d\n", off);
    if (diag >= 0) g_latdiag = diag != 0;
}
// (mp2-perf) Remote avatar perf cuts (DLL "set drawperf N", sora_draw_set_perf): bit 1 = C1 (bones blended once per publish),
// 2 = C2 (static index buffer per actor), 4 = C3 (one vertex-buffer copy fewer per side), 8 = C4 (cached descriptor sets),
// 16 = verify (one-shot, not kept: the old and the new path on the same input, memcmp logged). A cleared cut bit = the old
// path. Default all four cuts on. Timers (smoothed 0.98/0.02 + max, ms) go into the 300-frame "[sora-draw]   perf" line:
// game thread (skin_publish): skin = bone blend + vertex skinning, norm = skin_normals, pub = the copies under g_lock
// (lock wait included); render thread (draw_pass): snap = snapshot copy under g_lock, up = vertex / index buffer uploads,
// rec = draw recording. [0] = the body actor, [1] = the other shown actors (game thread: per call, avg and max of one call;
// render thread: summed over the other actors per pass).
#define PERF_C1 1
#define PERF_C2 2
#define PERF_C3 4
#define PERF_C4 8
#define PERF_VERIFY 16
#define PERF_DEFAULT 15
typedef struct { double avg, mx; } PerfT;
static volatile int g_perf = PERF_DEFAULT, g_perf_verify, g_perf_verify_r;  // (_r: the render thread's one-shot check, set by the game side)
static struct { PerfT skin[2], norm[2], pub[2], snap[2], up[2], rec[2]; unsigned others; } g_pf;
static inline void pf_add(PerfT *p, double ms) {
    p->avg = p->avg * 0.98 + ms * 0.02;
    if (ms > p->mx) p->mx = ms;
}
static inline double pf_ms(uint64_t t0) { return (double)(plat_now_ns() - t0) / 1e6; }
void sora_draw_set_perf(int bits) {  // (DLL "set drawperf N", looked up by name; bit 16 arms the one-shot verify; bits < 0: log only)
    if (bits >= 0) {
        const int nb = bits & 15;
        g_perf_verify = (bits & PERF_VERIFY) != 0;
        if (nb != g_perf || g_perf_verify)
            LOG("[sora-draw] drawperf %d: C1 bones-once %s, C2 static index buffer %s, C3 one vb copy fewer %s, C4 cached descriptor sets %s%s\n",
                bits, nb & PERF_C1 ? "on" : "off", nb & PERF_C2 ? "on" : "off", nb & PERF_C3 ? "on" : "off", nb & PERF_C4 ? "on" : "off",
                g_perf_verify ? ", verify armed (next publish)" : "");
        g_perf = nb;
    }
}
int sora_draw_perf(void) { return g_perf; }

// One baked character. The bake + local + last_* + sh / ghost / hide_weapon are the game thread's (the bake read-only
// once loaded: the render thread reads groups / idx / tex too), snap is under g_lock, gpu is the render thread's.
typedef struct {
    uint8_t *model, *pose;
    char *mpath, *ppath;               // the files (sora_draw_actor_load: the same pair again = the same id)
    uint32_t nbones, ngroups, ntex, nverts, ninfl, nidx, nslots;
    const MTex *tex;
    const MGroup *groups;
    const MVert *verts;
    const MInfl *infl;
    const uint32_t *idx;
    const PDir *dir;
    float *local;                      // nverts x {x, y, z (world), u, v, nx, ny, nz}: the game thread's skinning buffer
    float *bl;                         // (mp2-perf C1) nbones x 12: the bones blended once per publish (game thread, lazily made)
    uint32_t *weld, nweld;             // (s121 light) per vertex its weld id (same influence list = same skinned spot)
    float *nacc;                       // (s121 light) nweld x 3: the normal accumulators (game thread)
    float light[16];                   // (s121 light) this actor's own light (sora_draw_actor_light), used while own_light
    bool own_light;
    int last_slot;                     // the unknown-slot fallback (sora_draw_pose's "the last pose stays")
    float last_clock;
    Shadow sh;
    float ghost;                       // alpha of the body where the scene hides it (0 = off; s48 water)
    float tint[4];                     // glow rgb + amount (0 = off; s54 the map warp's blue)
    bool hide_weapon;                  // skip the ATTR_WEAPON groups (the no-weapon motion variants)
    uint8_t gchain[MAX_GROUPS];        // (s85) a weapon group's chain: 0 = the 178 blade, 1 = Valor's left (bones >= nbones - 12)
    uint8_t blade[2];                  // (s85) the alt id drawn per chain (group attr bits 24..31; 0 = the default 1 / 2)
    int wbase;                         // (s58) the weapon groups' first bone (0 = not looked up yet, -1 = none / mixed)
    struct {                           // (s58) the weapon drawn detached (sora_draw_actor_weapon_at)
        bool on, own;                  // own: bones given (else the hand pose made relative to the weapon's first bone)
        float bones[WEAPON_BONES_MAX * 12], pos[3], yaw, scale;
    } wat;
    struct {
        bool valid;
        float *vb;                     // nverts x VB_F as local
        int rs;                        // (mp2-perf C3) 0 = the vertices are in vb, else ring[rs - 1].vb (the newest published slot; g_lock)
        float feet[3];                 // world (the placement's translation)
        Shadow sh;                     // the shadow as set when this frame was skinned
        float ghost, tint[4], light[16];  // (s121 light: the light block as published with this frame)
        bool hide_weapon, weapon_at;   // (s58 weapon_at: drawn detached this frame)
        uint8_t blade[2];
    } snap;
    struct {                           // (s114 drawsync) the last DRAW_RING snapshots, tagged with the game frame (g_lock)
        uint64_t tag;                  // framepace_game_frame() at the publish (0 = empty / cleared by an off)
        uint64_t t_ns;                 // plat_now_ns() at the publish (diag)
        float *vb, feet[3];
        Shadow sh;
        float ghost, tint[4], light[16];
        bool hide_weapon, weapon_at;
        uint8_t blade[2];
        CamSnap cam;                   // g_cam as this publish left it
    } ring[DRAW_RING];
    uint64_t rn;                       // publishes into the ring
    struct {
        bool init, failed;
        VkImage tex[MAX_TEX];
        VmaAllocation tex_mem[MAX_TEX];
        VkImageView tex_view[MAX_TEX];
        float *vb;                     // the snap copied out (render thread)
        VkDescriptorSet tex_set[MAX_TEX];  // (mp2-perf C4) set 0 for texture i, made once from G.pool (NULL = none: a per-draw set)
        VkBuffer ib;                   // (mp2-perf C2) the static index buffer (a->idx, uploaded once by actor_gpu_init); NULL = none
        VmaAllocation ib_mem;
    } gpu;
} DrawActor;
// (g_lock) the newest published vertices: C3 off = snap.vb, on = the newest ring slot's vb (snap.rs)
static inline const float *snap_vb(const DrawActor *a) {
    return a->snap.rs > 0 && a->ring[a->snap.rs - 1].vb ? a->ring[a->snap.rs - 1].vb : a->snap.vb;
}
static DrawActor g_act[SORA_DRAW_ACTORS] = {[0] = {.last_slot = -1, .sh = SHADOW_DEFAULT}};
static int g_nact;   // loaded actors (0 until Sora's bake is in; written under g_lock, after the actor is filled)
static int g_state;  // Sora's bake (actor 0): 0 not loaded, 1 ready, -1 off / failed - nothing draws without it

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
// the camera of the last publish (any actor), g_lock; (s59) screen = it was sora_draw_screen's (the KH2 Game Over),
// clear = that mode's black rect (fractions of the game image)
static CamSnap g_cam;
static struct { bool on; float view[12], proj[4], clear[4], mask[4]; } g_scr;  // (s59) sora_draw_screen's camera (game thread)
// (s106 invsora 3) a camera for the NORMAL pass (ui boundary, scene depth, under the UI) instead of BotW's scanned world camera:
// the inventory's menu Link is its own deferred scene (F9 dump s106: G-buffer, light, bloom, then composited into the final
// image right before the UI), so Sora drawn in that pass needs the menu scene's camera. view = row-major 3x4 like
// linkpos_camera_view, proj = {near, far, fovy rad, aspect (0 = the image's)} like linkpos_camera_proj. DLL: sora_draw_camera
// (on, view, proj) each frame; it lapses by itself 30 calls of skin_publish after the last call (DLL stopped / unloaded).
static struct { bool on; float view[12], proj[4], shift[2]; unsigned age; } g_camo;
void sora_draw_camera(bool on, const float view[12], const float proj[4]) {
    on = on && view && proj;
    if (on != g_camo.on) LOG("[sora-draw] camera override %s\n", on ? "on (the DLL's camera in the normal pass)" : "off (BotW's camera)");
    g_camo.on = on, g_camo.age = 0;
    if (on) memcpy(g_camo.view, view, sizeof g_camo.view), memcpy(g_camo.proj, proj, sizeof g_camo.proj);
}
// (s107 invsora 3) the override camera's off-centre shift, in NDC (x right, y up, as the game's own projection: the
// inventory's menu camera is a sead PerspectiveProjection with an offset, m02 = -0.59 -> sx = +0.59: Link stands right of
// centre). Kept until set again; used only while the override is on. DLL: sora_draw_camera_shift (each frame, before
// sora_draw_camera).
void sora_draw_camera_shift(float sx, float sy) {
    g_camo.shift[0] = isfinite(sx) ? sx : 0, g_camo.shift[1] = isfinite(sy) ? sy : 0;
}

// ------------------------------------------------------------------ (s183 kairi-diag) why the body is not on screen
// A tester had the KH2 HUD and the title logo but no model at all and no log. Every way the body draw can fail or never
// start records a short reason here (first failure wins, never cleared; code + text); the DLL (sora_diag.h) asks
// sora_draw_diag once a frame, and when it posed the body for > 5 s without one drawn frame it puts the reason on screen.
// Cheap: a latch writes once, a skip is one pointer store, a draw one counter store. Any thread.
uint64_t vk_frame_counter(void);
static char g_dg_why[200];
static volatile LONG g_dg_has;               // 0 none, 2 one thread is writing g_dg_why, 1 complete (readers use only 1)
static const char *volatile g_dg_skip = "";  // the latest reason a pass drew nothing (cleared by a draw)
static volatile uint64_t g_dg_drawn;         // vk_frame_counter() of the last pass that drew a body
static bool g_bake_ioerr;  // (s183) the last Sora bake failure was a read failure (retried), not a bad file
static const char *dg_bake_why = "see the [sora-draw] lines above in last.log";
static void dg_latch(const char *fmt, ...) {
    if (g_dg_has) return;
    char t[sizeof g_dg_why];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(t, sizeof t, fmt, ap);
    va_end(ap);
    t[sizeof t - 1] = 0;
    if (InterlockedCompareExchange(&g_dg_has, 2, 0) != 0) return;  // (first failure wins, even across threads)
    memcpy(g_dg_why, t, strlen(t) + 1);  // (only the text and its NUL, not the stack bytes after it)
    MemoryBarrier();
    InterlockedExchange(&g_dg_has, 1);
    LOG("[kairi-diag] body draw: %s\n", t);
}
static const char *dg_vkr(int r) {
    switch (r) {
    case 0: return "VK_SUCCESS";
    case -1: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case -2: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case -3: return "VK_ERROR_INITIALIZATION_FAILED";
    case -4: return "VK_ERROR_DEVICE_LOST";
    case -5: return "VK_ERROR_MEMORY_MAP_FAILED";
    case -7: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case -8: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case -11: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case -1000012000: return "VK_ERROR_INVALID_SHADER_NV";
    default: return "VkResult";
    }
}
// -> 1 and the first latched failure in why, else 0 and the latest skip reason (may be ""); *drawn_frame = the last frame drawn (0 = never).
int sora_draw_diag(char *why, int cap, uint64_t *drawn_frame) {
    if (drawn_frame) *drawn_frame = g_dg_drawn;
    int has = g_dg_has == 1;  // (2 = being written: not yet)
    const char *t = has ? g_dg_why : g_dg_skip;
    if (why && cap > 0) snprintf(why, (size_t)cap, "%s", t ? t : "");
    return has;
}

// (s183) why the last read_all failed: g_rd_gone = the file does not exist (else it exists but could not be read: antivirus /
// OneDrive lock, no memory, short read), g_rd_note = errno / GetLastError text for the log.
static bool g_rd_gone;
static char g_rd_note[160];
static uint8_t *read_all(const char *path, long *n) {
    path = sora_path(path);  // (every draw actor's model + pose)
    g_rd_gone = false, g_rd_note[0] = 0;
    errno = 0;
    FILE *f = fopen(path, "rb");
    if (!f) {
        int e = errno;
        g_rd_gone = e == ENOENT;
#ifdef _WIN32
        snprintf(g_rd_note, sizeof g_rd_note, "fopen failed: errno %d (%s), GetLastError %lu", e, strerror(e), (unsigned long)GetLastError());
#else
        snprintf(g_rd_note, sizeof g_rd_note, "fopen failed: errno %d (%s)", e, strerror(e));
#endif
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = *n > 32 ? malloc((size_t)*n) : NULL;
    if (!b) snprintf(g_rd_note, sizeof g_rd_note, *n > 32 ? "malloc of %ld bytes failed" : "file is only %ld bytes", *n);
    else if (fread(b, 1, (size_t)*n, f) != (size_t)*n)
        snprintf(g_rd_note, sizeof g_rd_note, "read failed after open: errno %d (%s)", errno, strerror(errno)), free(b), b = NULL;
    fclose(f);
    return b;
}

// (s121 light) The weld table: KH2 splits a vertex where its uv / group changes, so face normals summed per vertex would
// show the seams. Two vertices with the same influence list (bones + weighted bone-space positions, MInfl) always skin to
// the same spot, so they share a weld id and get one smooth normal. Hash of the list -> open-addressed table, the list
// compared on a hash hit. Game thread, at load.
static uint64_t infl_hash(const DrawActor *a, const MVert *mv) {
    uint64_t h = 1469598103934665603ull ^ mv->n;
    for (uint32_t k = mv->first; k < mv->first + mv->n && k < a->ninfl; k++) {
        const uint32_t *w = (const uint32_t *)&a->infl[k];
        for (int j = 0; j < 5; j++) h = (h ^ w[j]) * 1099511628211ull;
    }
    return h;
}
static bool infl_same(const DrawActor *a, const MVert *x, const MVert *y) {
    if (x->n != y->n) return false;
    if (x->first + x->n > a->ninfl || y->first + y->n > a->ninfl) return x->first == y->first;
    return !memcmp(&a->infl[x->first], &a->infl[y->first], x->n * sizeof(MInfl));
}
static bool weld_build(DrawActor *a) {
    uint32_t cap = 1;
    while (cap < a->nverts * 2u) cap <<= 1;
    uint32_t *slot = malloc(cap * sizeof(uint32_t));  // vertex index + 1 of the slot's first vertex (0 = empty)
    a->weld = malloc(a->nverts * sizeof(uint32_t));
    if (!slot || !a->weld) { free(slot); return false; }
    memset(slot, 0, cap * sizeof(uint32_t));
    uint32_t nw = 0;
    for (uint32_t v = 0; v < a->nverts; v++) {
        uint32_t i = (uint32_t)infl_hash(a, &a->verts[v]) & (cap - 1);
        for (;; i = (i + 1) & (cap - 1)) {
            if (!slot[i]) { slot[i] = v + 1, a->weld[v] = nw++; break; }
            uint32_t o = slot[i] - 1;
            if (infl_same(a, &a->verts[o], &a->verts[v])) { a->weld[v] = a->weld[o]; break; }
        }
    }
    free(slot);
    a->nweld = nw;
    a->nacc = calloc((size_t)nw * 4, sizeof(float));  // (x y z sum + the sum of the face normals' lengths)
    if (!a->nacc) return false;
    LOG("[sora-draw] light: %u verts welded to %u normals\n", a->nverts, nw);
    return true;
}
// (s121 light) Per frame after the skin: area-weighted face normals summed per weld id, normalized, written to each
// vertex (local[v * VB_F + 5..7]). The sign follows the bake's winding (unknown): the fragment shader turns each normal
// toward the camera's side of its triangle, so only the smoothness matters here. body: time it for the log.
static double g_norm_ms, g_norm_max;  // (body actor, for the 300-frame log)
static void skin_normals(DrawActor *a, float *buf, bool body) {  // (mp2-perf: buf = the vertex buffer to write: a->local, or a ring slot's vb)
    if (!a->weld || !a->nacc) return;
    uint64_t t0 = plat_now_ns();
    memset(a->nacc, 0, (size_t)a->nweld * 4 * sizeof(float));
    const float *L = buf;
    for (uint32_t k = 0; k + 2 < a->nidx; k += 3) {
        uint32_t i0 = a->idx[k], i1 = a->idx[k + 1], i2 = a->idx[k + 2];
        if (i0 >= a->nverts || i1 >= a->nverts || i2 >= a->nverts) continue;
        const float *p0 = L + i0 * VB_F, *p1 = L + i1 * VB_F, *p2 = L + i2 * VB_F;
        float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]}, e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
        float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        float *w0 = a->nacc + a->weld[i0] * 4, *w1 = a->nacc + a->weld[i1] * 4, *w2 = a->nacc + a->weld[i2] * 4;
        const float nl = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        for (int j = 0; j < 3; j++) w0[j] += n[j], w1[j] += n[j], w2[j] += n[j];
        w0[3] += nl, w1[3] += nl, w2[3] += nl;
    }
    // (s121 review) a sum much shorter than its parts = faces that cancel (two-sided cards: cape / hair with opposite
    // windings on one weld): its direction is noise, so normal 0 = drawn unlit. Threshold 0.15 GUESS.
    for (uint32_t v = 0; v < a->nverts; v++) {
        const float *n = a->nacc + a->weld[v] * 4;
        float l = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]), *o = buf + v * VB_F + 5;
        if (l > 1e-12f && l > 0.15f * n[3]) o[0] = n[0] / l, o[1] = n[1] / l, o[2] = n[2] / l;
        else o[0] = o[1] = o[2] = 0;  // (no triangle / cancelled: the shader draws it unlit)
    }
    if (body) {
        double ms = (double)(plat_now_ns() - t0) / 1e6;
        g_norm_ms = g_norm_ms * 0.98 + ms * 0.02;
        if (ms > g_norm_max) g_norm_max = ms;
    }
}

// The bake pair <mp> / <pp> into <a> (+ its skin buffers); false (and <a>'s bake fields cleared, memory freed) on any
// problem. Leaves sh / ghost / last_* alone (actor 0's may be set before its first use).
static bool bake_load(DrawActor *a, int id, const char *mp, const char *pp) {
    long nm = 0, np = 0;
    static char rbuf[240];
    a->model = read_all(mp, &nm);
    bool mgone = g_rd_gone;
    char mnote[160];
    memcpy(mnote, g_rd_note, sizeof mnote);
    a->pose = read_all(pp, &np);
    const char *why = NULL;
    if (!id) {
        g_bake_ioerr = !a->model || !a->pose;
        if (!a->model) snprintf(rbuf, sizeof rbuf, mgone ? "sora_model.bin is missing" : "sora_model.bin exists but could not be read (%s)", mnote);
        else if (!a->pose) snprintf(rbuf, sizeof rbuf, g_rd_gone ? "sora_pose.bin is missing" : "sora_pose.bin exists but could not be read (%s)", g_rd_note);
        else snprintf(rbuf, sizeof rbuf, "wrong file type");
        dg_bake_why = rbuf;
        if (g_bake_ioerr) LOG("[sora-draw] bake read: %s\n", rbuf);
    }
    if (!a->model || !a->pose || memcmp(a->model, "SKRG", 4) || memcmp(a->pose, "SKRP", 4)) {
        LOG("[sora-draw] no draw bake (%s %s, %s %s): %s not drawn\n", mp, a->model ? "ok" : "missing", pp,
            a->pose ? "ok" : "missing", id ? "actor" : "Sora");
        goto fail;
    }
    const uint32_t *h = (const uint32_t *)a->model;
    a->nbones = h[2], a->ngroups = h[3], a->ntex = h[4], a->nverts = h[5], a->ninfl = h[6], a->nidx = h[7];
    size_t t = 32, gr = t + 16 * (size_t)a->ntex, v = gr + 16 * (size_t)a->ngroups, in = v + 16 * (size_t)a->nverts,
           ix = in + 20 * (size_t)a->ninfl;
    if (a->ntex > MAX_TEX || a->ngroups > MAX_GROUPS || !a->nverts || ix + 4 * (size_t)a->nidx > (size_t)nm) {
        why = "bad model";
        if (!id) dg_bake_why = "sora_model.bin is damaged";
        goto bad;
    }
    a->tex = (const MTex *)(a->model + t), a->groups = (const MGroup *)(a->model + gr);
    a->verts = (const MVert *)(a->model + v), a->infl = (const MInfl *)(a->model + in), a->idx = (const uint32_t *)(a->model + ix);
    {  // (s85) each weapon group's chain, by its lowest bone: a second chain = the last KB_CHAIN_BONES bones above the first
        uint32_t lo0 = UINT32_MAX, glo[MAX_GROUPS];
        for (uint32_t g = 0; g < a->ngroups; g++) {
            glo[g] = UINT32_MAX;
            const MGroup *gr = &a->groups[g];
            for (uint32_t k = gr->first; (gr->attr & ATTR_WEAPON) && k < gr->first + gr->n && k < a->nidx; k++) {
                const MVert *mv = a->idx[k] < a->nverts ? &a->verts[a->idx[k]] : NULL;
                if (mv && mv->n && mv->first < a->ninfl && a->infl[mv->first].bone < glo[g]) glo[g] = a->infl[mv->first].bone;
            }
            if (glo[g] < lo0) lo0 = glo[g];
        }
        uint32_t b1 = a->nbones >= KB_CHAIN_BONES ? a->nbones - KB_CHAIN_BONES : 0;
        for (uint32_t g = 0; g < a->ngroups; g++) a->gchain[g] = b1 > lo0 && glo[g] != UINT32_MAX && glo[g] >= b1;
    }
    for (uint32_t i = 0; i < a->ntex; i++)  // (s54: a texture past the file's end would upload garbage / fault)
        if ((size_t)a->tex[i].off + (size_t)a->tex[i].w * a->tex[i].h * 4 > (size_t)nm) { why = "texture past the end"; goto bad; }
    const uint32_t *ph = (const uint32_t *)a->pose;
    a->nslots = ph[2];
    if (ph[3] != a->nbones) {
        LOG("[sora-draw] pose bake %s has %u bones, the model %u\n", pp, ph[3], a->nbones);
        if (!id) dg_bake_why = "sora_pose.bin does not match sora_model.bin";
        goto fail;
    }
    if (16 + 8 * (size_t)a->nslots > (size_t)np) { why = "bad pose directory"; goto bad; }
    a->dir = (const PDir *)(a->pose + 16);
    for (uint32_t i = 0; i < a->nslots; i++)  // (s54: every slot's frames inside the file)
        if ((size_t)a->dir[i].off + (size_t)a->dir[i].poses * a->nbones * 48 > (size_t)np) {
            why = "pose slot past the end";
            goto bad;
        }
    a->local = calloc(a->nverts, VB_F * sizeof(float));
    a->snap.vb = calloc(a->nverts, VB_F * sizeof(float));
    if (!a->local || !a->snap.vb) { why = "out of memory"; goto bad; }
    if (!weld_build(a)) { why = "out of memory (weld table)"; goto bad; }
    a->mpath = strdup(mp), a->ppath = strdup(pp);
    // (s203) the log names the file that was really read (read_all maps D:/SoraBotW -> SORA_ROOT; the s202 Melty test log
    // said "D:/SoraBotW/data/sora_model.bin" for a copy that read its own folder). mpath / ppath keep the caller's strings.
    LOG("[sora-draw] model %s (actor %d): %u bones, %u groups, %u verts, %u tris, %u textures; poses %s: %u slots\n", sora_path(mp),
        id, a->nbones, a->ngroups, a->nverts, a->nidx / 3, a->ntex, sora_path(pp), a->nslots);
    return true;
bad:
    if (!id) dg_bake_why = why;
    LOG("[sora-draw] %s: %s (actor %d not drawn)\n", mp, why, id);
fail:
    free(a->model), free(a->pose), free(a->local), free(a->snap.vb), free(a->weld), free(a->nacc), free(a->bl), a->bl = NULL;
    a->model = a->pose = NULL, a->local = a->snap.vb = NULL, a->weld = NULL, a->nacc = NULL, a->nweld = 0;
    for (int r = 0; r < DRAW_RING; r++) free(a->ring[r].vb), a->ring[r].vb = NULL, a->ring[r].tag = 0;
    a->nbones = a->ngroups = a->ntex = a->nverts = a->ninfl = a->nidx = a->nslots = 0;
    return false;
}

static const PDir *pose_slot(const DrawActor *a, int slot) {
    for (uint32_t i = 0; i < a->nslots; i++)
        if (a->dir[i].slot == slot && a->dir[i].poses) return &a->dir[i];  // (s54: an empty slot = unknown)
    return NULL;
}

// Sora's bake (actor 0), loaded on the game thread's first call (any of the entry points below), never again: true
// once ready. Every other actor needs it (the render thread's gate).
static bool ready(void) {
    if (!g_state) {
        const char *e = getenv("SAVAGEWIND_SORA_DRAW");
        if (e && !atoi(e)) { LOG("[sora-draw] off (SAVAGEWIND_SORA_DRAW=0)\n"); dg_latch("D0 drawing is switched off (SAVAGEWIND_SORA_DRAW=0)"); g_state = -1; return false; }
        const char *mp = getenv("SAVAGEWIND_SORA_MODEL"), *pp = getenv("SAVAGEWIND_SORA_POSE");
        if (!mp || !*mp) mp = "D:/SoraBotW/data/sora_model.bin";
        if (!pp || !*pp) pp = "D:/SoraBotW/data/sora_pose.bin";
        // (s183) a read failure (antivirus / OneDrive holding the freshly built 83 MB sora_pose.bin) is retried every ~2 s for
        // ~20 s before the D1 latch; a bad file (wrong type / damaged / mismatch) is final at once
        static struct timespec first, next;
        struct timespec now;
        timespec_get(&now, TIME_UTC);
        if (!first.tv_sec) first = now;
        if (now.tv_sec < next.tv_sec) return false;
        if (bake_load(&g_act[0], 0, mp, pp)) g_state = 1;
        else if (g_bake_ioerr && now.tv_sec - first.tv_sec < 20) {
            next.tv_sec = now.tv_sec + 2;
            return false;
        } else g_state = -1;
        if (g_state < 0) {
            bool io = g_bake_ioerr;
            dg_latch("D1 Sora's model data (sora_model.bin / sora_pose.bin) %s: %s", io ? "could not be read (or is missing)" : "is bad", dg_bake_why);
        }
        if (g_state == 1) {
            e = getenv("SAVAGEWIND_SORA_SHADOW");
            if (e && *e && !atoi(e)) g_shadow_on = false, LOG("[sora-draw] no blob shadow (SAVAGEWIND_SORA_SHADOW=0)\n");
            pthread_mutex_lock(&g_lock);
            g_nact = 1;
            pthread_mutex_unlock(&g_lock);
        }
    }
    return g_state == 1;
}

// The loaded actor <id> (game thread: it alone writes g_nact), else NULL.
static DrawActor *actor(int id) {
    return ready() && id >= 0 && id < g_nact ? &g_act[id] : NULL;
}
// For the settings that may come before the bake loads (sora_draw_shadow & co. never loaded it): actor 0 always.
static DrawActor *actor_set(int id) {
    return id == 0 ? &g_act[0] : id > 0 && id < g_nact ? &g_act[id] : NULL;
}

// (s68 drive) The actor Sora's own calls (sora_draw_pose / world / slot_pose / bone_count / shadow / ghost / tint) drive:
// 0 = base Sora, a Drive Form's loaded actor while he is in it. The switch carries the shadow / ghost / tint / weapon
// settings over and hides the old body; the new one shows from its next pose. Game thread.
static int g_body;
// (s102 leaves, glitch #12) BotW fades the foliage right in front of the camera (dithered: it looks see-through on screen)
// but those leaves still write the scene depth, so under a canopy with the camera inside the leaves Sora + party were
// hidden (GUESS, fits the report). s102 diag VERIFIED the s99 guess wrong: the 2 blended depth writers under trees go to a
// separate 800x450 target, not the scene depth. Now a scene surface nearer than g_near_cut metres to the camera (and nearer
// than 0.6 x the body's own distance) does not hide the bodies. DLL: sora_draw_near_cut(cm) (live key leafcut); 0 = old.
// (s107 slateocc, recon/S107_HELPER_SLATE.md, NEEDS EXE RELINK) The held Sheikah Slate may look see-through (Sora's hand drawn over
// it) when BotW draws it where the chosen scene depth does not see it. Analytic occluder: the exe knows the Slate model's world
// matrix (linkpos_slate_model_world); a box of half sizes h (mm, in the model's local axes) there hides every body fragment
// whose ray from the camera passes through it before reaching the fragment. DLL: sora_draw_slate_occ (live key slatocc).
// Model bbox (Item_ConductorDemo.sbfres, half floats): x +-0.132, y +-0.024 (the thin axis), z +-0.079 m.
bool linkpos_slate_model_world(float out[12]);
static volatile int g_socc_on, g_socc_h[3] = {140, 40, 90};
void sora_draw_slate_occ(int on, int hx_mm, int hy_mm, int hz_mm) {
    g_socc_on = on != 0, g_socc_h[0] = hx_mm, g_socc_h[1] = hy_mm, g_socc_h[2] = hz_mm;
}
static float g_near_cut = 2.0f;
void sora_draw_near_cut(int cm) { g_near_cut = cm > 0 ? cm / 100.0f : 0.0f; }
// (s105 leaves v2, NEEDS EXE RELINK) leafmode 1 replaces the flat near cut by a NEIGHBOURHOOD test: BotW fades near foliage
// by dithering (GUESS), so a faded leaf is a stipple in the scene depth while a solid leaf / branch is near in every
// neighbour. A pixel whose scene depth hides the body (and is nearer than leafrange) only hides it when at least leafthr
// percent of a 4x4 block of depth taps (spread leafk tenths of a texel) also hide it. DLL keys leafmode / leafk / leafthr /
// leafrange via sora_draw_leaf_cfg(which, v); depthdbg via sora_draw_depth_debug (the env SAVAGEWIND_SORA_DEPTH_DEBUG view).
static int g_leaf_mode = 1 /* s183: default ON (user OK, was dev-live-file only) */, g_leaf_k = 10, g_leaf_thr = 90, g_leaf_range = 1000, g_depth_dbg = 0;
void sora_draw_leaf_cfg(int which, int v) {
    if (which == 0) g_leaf_mode = v != 0;
    else if (which == 1) g_leaf_k = v < 1 ? 1 : v > 100 ? 100 : v;
    else if (which == 2) g_leaf_thr = v < 1 ? 1 : v > 100 ? 100 : v;
    else if (which == 3) g_leaf_range = v < 0 ? 0 : v;
}
void sora_draw_depth_debug(int on) { g_depth_dbg = on != 0; }
void sora_draw_set_body(int id) {
    DrawActor *to = actor_set(id), *from = actor_set(g_body);
    if (id == g_body) return;
    if (!to || !from) { LOG("[sora-draw] body: actor %d not loaded (stays %d)\n", id, g_body); return; }
    to->sh = from->sh, to->ghost = from->ghost, to->hide_weapon = from->hide_weapon;
    to->blade[0] = from->blade[0], to->blade[1] = from->blade[1];  // (s85)
    memcpy(to->tint, from->tint, sizeof to->tint);
    sora_draw_actor_off(g_body);
    LOG("[sora-draw] body: actor %d -> %d (%u bones)\n", g_body, id, to->nbones);
    g_body = id;
}
int sora_draw_body(void) { return g_body; }

// (s110 skate) the table size, for a mod that must not take the LAST free actor of an older exe (the skateboard would starve Mickey's load)
int sora_draw_actor_capacity(void) { return SORA_DRAW_ACTORS; }

int sora_draw_actor_load(const char *model_path, const char *pose_path) {
    if (!ready() || !model_path || !pose_path || !*model_path || !*pose_path) return -1;
    for (int i = 0; i < g_nact; i++)
        if (g_act[i].mpath && !strcmp(g_act[i].mpath, model_path) && !strcmp(g_act[i].ppath, pose_path)) return i;
    if (g_nact >= SORA_DRAW_ACTORS) {
        LOG("[sora-draw] %s: all %d draw actors in use\n", model_path, SORA_DRAW_ACTORS);
        return -1;
    }
    int id = g_nact;
    DrawActor *a = &g_act[id];
    memset(a, 0, sizeof *a);
    a->last_slot = -1, a->sh = (Shadow)SHADOW_DEFAULT;
    if (!bake_load(a, id, model_path, pose_path)) return -1;
    pthread_mutex_lock(&g_lock);  // (the render thread reads g_nact under it: the actor is complete before it counts)
    g_nact = id + 1;
    pthread_mutex_unlock(&g_lock);
    return id;
}

// ------------------------------------------------------------------ the snapshot (game thread -> render thread)
void sora_draw_actor_off(int id) {
    DrawActor *a = g_state == 1 ? actor_set(id) : NULL;
    if (!a || !a->snap.vb) return;
    pthread_mutex_lock(&g_lock);
    a->snap.valid = false;
    for (int r = 0; r < DRAW_RING; r++) a->ring[r].tag = 0;  // (s114: no pre-off pose drawn after the next show)
    pthread_mutex_unlock(&g_lock);
}

void sora_draw_off(void) {  // (s54: every actor, see sora_draw.h)
    if (g_state != 1) return;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_nact; i++) {
        g_act[i].snap.valid = false;
        for (int r = 0; r < DRAW_RING; r++) g_act[i].ring[r].tag = 0;
    }
    pthread_mutex_unlock(&g_lock);
}

// The two baked poses <clock> falls between for slot <d> (<clock> clamped to the slot's frames), and the blend weight
// of the second (0 at a baked frame): the pose = m0 + (m1 - m0) * u per matrix element.
static void pose_sample(const DrawActor *a, const PDir *d, float clock, const float **m0, const float **m1, float *u) {
    if (clock < 0) clock = 0;
    float maxf = (float)(d->poses - 1);
    if (clock > maxf) clock = maxf;
    int f0 = (int)clock, f1 = f0 + 1 < d->poses ? f0 + 1 : f0;
    *u = clock - (float)f0;
    *m0 = (const float *)(a->pose + d->off + (size_t)f0 * a->nbones * 48);
    *m1 = (const float *)(a->pose + d->off + (size_t)f1 * a->nbones * 48);
}

// (s58) 4x3 row-vector affine matrices (the bake's layout: rows r0 r1 r2 = the 3x3, r3 = the translation): out = a
// then b (v' = (v a) b), and the inverse of an invertible one (the identity for a singular one).
static void mtx43_mul(const float a[12], const float b[12], float out[12]) {
    for (int r = 0; r < 4; r++)
        for (int j = 0; j < 3; j++)
            out[r * 3 + j] = a[r * 3] * b[j] + a[r * 3 + 1] * b[3 + j] + a[r * 3 + 2] * b[6 + j] + (r == 3 ? b[9 + j] : 0);
}
static void mtx43_inv(const float m[12], float out[12]) {
    float d = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (!(fabsf(d) > 1e-12f)) {
        memset(out, 0, 12 * sizeof(float));
        out[0] = out[4] = out[8] = 1;
        return;
    }
    float id = 1 / d;
    out[0] = (m[4] * m[8] - m[5] * m[7]) * id, out[1] = (m[2] * m[7] - m[1] * m[8]) * id, out[2] = (m[1] * m[5] - m[2] * m[4]) * id;
    out[3] = (m[5] * m[6] - m[3] * m[8]) * id, out[4] = (m[0] * m[8] - m[2] * m[6]) * id, out[5] = (m[2] * m[3] - m[0] * m[5]) * id;
    out[6] = (m[3] * m[7] - m[4] * m[6]) * id, out[7] = (m[1] * m[6] - m[0] * m[7]) * id, out[8] = (m[0] * m[4] - m[1] * m[3]) * id;
    for (int j = 0; j < 3; j++) out[9 + j] = -(m[9] * out[j] + m[10] * out[3 + j] + m[11] * out[6 + j]);
}

// ------------------------------------------------------------------ (s121 light) the light block (game thread)
// 16 floats, every actor's unless it has its own (sora_draw_actor_light): L0 = sun direction (world, toward the sun) +
// style (0 = off = the old unlit look, 1 KH2 soft, 2 toon, 3 BotW soft), L1 = sun rgb + amount 0..1, L2 = ambient top rgb +
// ambient floor 0..1, L3 = ambient bottom rgb + shadow 0..1 (later). Rides with the next skinned frame like the tint;
// the render thread hands it to the shaders as a per-instance vertex attribute (binding 1). The DLL (runtime/sora_light.h)
// calls sora_draw_light each frame; n < 16 is ignored.
static float g_light[16];  // (style 0 until the DLL sends one)
static unsigned g_light_calls;
void sora_draw_light(const float *p, int n) {
    if (!p || n < 16) return;
    for (int i = 0; i < 16; i++) if (!isfinite(p[i])) return;
    if (!g_light_calls++ || (p[3] > 0.5f) != (g_light[3] > 0.5f))
        LOG("[sora-draw] light: style %.0f sun %.2f %.2f %.2f rgb %.2f %.2f %.2f amount %.2f\n", p[3], p[0], p[1], p[2], p[4], p[5], p[6], p[7]);
    memcpy(g_light, p, sizeof g_light);
}
// (s124 b) BotW's sun shadow on the bodies, from the DLL (sora_light.h) each frame: p = {mode (0 off, 1 one lookup at the
// chest, 2 per pixel, 3 / 4 = the same with debug colours: red shade, green sun, blue outside the cascade), strength 0..1,
// chest radius m, bias m, chest height m above the feet, offset toward the sun m (per pixel), per-pixel radius m, flags
// (-1 = auto: bit 0 flip uv y unless the window origin is lower-left, bit 1 depth 0..1 unless NVN's -1..1; bit 2 =
// greater-is-nearer)}. n < 8 ignored. The per-instance record (LREC_F floats): light 0..15, s0 16..19, s1 20..23,
// s2 24..27, cascade 0 matrix rows 28..43 (sun_shadow_rec).
// (s125 c, sora_draw_cast below) mode (0 off, 1 every actor, 2 Sora only, 3 every actor + they shade themselves), layer
// mask (bits 0..2), push m. GC.r*: cascade layer 0 as it was before the actors went in (copied each cast), what the
// bodies' own receive lookup reads (draw_pass) unless mode 3, so the verified s124 look stays (no self-shadow).
static float g_cast[3] = {1, 7, 0};
static struct {
    VkPipelineLayout layout;
    struct { VkFormat fmt; bool ge; VkPipeline p; } pipe[4];
    bool failed;
    unsigned n;
    VkImage rimg; VmaAllocation rmem; VkImageView rview; VkFormat rfmt; uint32_t rw, rh; uint64_t rframe; bool rfail;
} GC;
// (s126 lflat: [13] = 1 -> a vertex whose normal cancelled (two-sided hair cards / keychain) is lit by its triangle's own
// facing instead of drawn unlit: Roxas' hair stayed day-bright at night, mark_184618_4; 0 = the old unlit look)
static float g_sdw[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0.79f, 0.84f, 1.0f, 1.0f, 0.0f, 1.0f};  // (s124b: + sky rgb, darkness, cool)
void sora_draw_sun_shadow(const float *p, int n) {
    if (!p || n < 8) return;
    for (int i = 0; i < 8; i++) if (!isfinite(p[i])) return;
    if ((p[0] > 0.5f) != (g_sdw[0] > 0.5f) || p[0] != g_sdw[0] || p[7] != g_sdw[7])
        LOG("[sora-draw] sun shadow: mode %.0f strength %.2f radius %.2f bias %.3f chest %.2f off %.3f prad %.3f flags %.0f\n",
            p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
    memcpy(g_sdw, p, 8 * sizeof(float));
    if (n >= 13) {  // (s124b) the shade colour (older DLLs send 8: shade = the s124 look, darkness 1, cool 0)
        for (int i = 8; i < 13; i++) if (!isfinite(p[i])) return;
        memcpy(g_sdw + 8, p + 8, 5 * sizeof(float));
    } else g_sdw[11] = 1, g_sdw[12] = 0;
    if (n >= 16 && isfinite(p[13]) && isfinite(p[14]) && isfinite(p[15])) {  // (s125 c) the cast shadow: mode, layer mask, push m
        extern void sora_draw_cast_cfg(float mode, float mask, float push);
        sora_draw_cast_cfg(p[13], p[14], p[15]);
    }
    if (n >= 17 && isfinite(p[16]) && p[16] != g_sdw[13]) {  // (s126) lflat
        g_sdw[13] = p[16];
        LOG("[sora-draw] flat light for cancelled normals: %s\n", p[16] > 0.5f ? "on" : "off (old: unlit)");
    }
}
static int sun_shadow_flags(void) {
    if (g_sdw[7] >= 0) return (int)(g_sdw[7] + 0.5f);
    return (renderer_origin_lower_left() ? 0 : 1) | (nvn_depth_mode() != 0 ? 2 : 0);
}
static void sun_shadow_rec(float *r, const float feet[3], const float m[16]) {
    r[16] = g_sdw[0], r[17] = g_sdw[1], r[18] = g_sdw[2], r[19] = g_sdw[3];
    r[20] = feet[0], r[21] = feet[1] + g_sdw[4], r[22] = feet[2], r[23] = (float)sun_shadow_flags();
    r[24] = g_sdw[5], r[25] = g_sdw[6], r[26] = g_sdw[11], r[27] = g_sdw[12];
    r[44] = g_sdw[8], r[45] = g_sdw[9], r[46] = g_sdw[10], r[47] = g_sdw[13];  // (v_sk.w = lflat)
    memcpy(r + 28, m, 16 * sizeof(float));
}
// every 600 passes (and on a change of on / off): the chest's cascade-0 clip point, so the log shows it lands inside
static void sun_shadow_log(bool on, const float *feet, const float m[16], uint32_t w, uint32_t h) {
    static unsigned n;
    static int was = -1;
    if (g_sdw[0] < 0.5f) { was = -1; return; }
    if (n++ % 600 && (int)on == was) return;
    was = on;
    if (!on || !feet) { LOG("[sora-draw] sun shadow: no cascade this frame (mode %.0f)\n", g_sdw[0]); return; }
    float p[4] = {feet[0], feet[1] + g_sdw[4], feet[2], 1}, c[4];
    for (int i = 0; i < 4; i++) c[i] = m[i * 4] * p[0] + m[i * 4 + 1] * p[1] + m[i * 4 + 2] * p[2] + m[i * 4 + 3];
    float iw = fabsf(c[3]) > 1e-6f ? 1 / c[3] : 0;
    LOG("[sora-draw] sun shadow: cascade %ux%u, chest %.2f %.2f %.2f -> ndc %.3f %.3f %.3f (w %.3f), flags %d, last row %.3g %.3g %.3g %.3g\n",
        w, h, p[0], p[1], p[2], c[0] * iw, c[1] * iw, c[2] * iw, c[3], sun_shadow_flags(), m[12], m[13], m[14], m[15]);
}
// One actor's own light (p NULL / n < 16: back to the shared one).
void sora_draw_actor_light(int id, const float *p, int n) {
    DrawActor *a = actor_set(id);
    if (!a) return;
    for (int i = 0; p && n >= 16 && i < 16; i++) if (!isfinite(p[i])) return;  // (s121 review: like sora_draw_light)
    a->own_light = p && n >= 16;
    if (a->own_light) memcpy(a->light, p, sizeof a->light);
}

// (s121 light) BotW's world state for the DLL's light: out = {time of day 0..360 (= 24 h; TimeMgr +0xb8, the read of
// hle/framepace.c clock_probe, VERIFIED), stage type (u32 world::Manager +0x6b8, 2 = Indoor: GUESS offset, decomp 1.5.0 vs
// game 1.9.0), weather (u8 +0x7d1 if != 0xff else WeatherMgr +0x28, GUESS), climate (u32 +0x780, GUESS)}. False without the world manager. A "[light] env"
// line every 600 calls so normal play confirms the GUESS offsets.
bool sora_world_env(float out[4]) {
    if (!out || !g_site.world_mgr) return false;
    uint64_t wm = *(uint64_t *)(uintptr_t)g_site.world_mgr;
    if (!wm) return false;
    float tod = -1;
    if (*(uint32_t *)(uintptr_t)(wm + 0x5c0)) {
        uint64_t mgrs = *(uint64_t *)(uintptr_t)(wm + 0x5c8), tm = mgrs ? *(uint64_t *)(uintptr_t)mgrs : 0;
        if (tm) tod = *(float *)(uintptr_t)(tm + 0xb8);
    }
    uint32_t stage = *(uint32_t *)(uintptr_t)(wm + 0x6b8), climate = *(uint32_t *)(uintptr_t)(wm + 0x780);
    // (s123 a2) weather: Manager +0x7d1 is a FORCED request (0xff = none: seen in the S123 probe run); when it is 0xff the
    // live weather is WeatherMgr (= mMgrs[3], same chain as TimeMgr = mMgrs[0]) +0x28 u8. GUESS: +0x28 is the weather type; only
    // seen = 0 (clear) so far, rain not read yet.
    uint8_t weather = *(uint8_t *)(uintptr_t)(wm + 0x7d1);
    if (weather == 0xff && *(uint32_t *)(uintptr_t)(wm + 0x5c0) > 3) {
        uint64_t mgrs = *(uint64_t *)(uintptr_t)(wm + 0x5c8), wmgr = mgrs ? *(uint64_t *)(uintptr_t)(mgrs + 3 * 8) : 0;
        if (wmgr) weather = *(uint8_t *)(uintptr_t)(wmgr + 0x28);
    }
    out[0] = isfinite(tod) ? tod : -1, out[1] = (float)stage, out[2] = (float)weather, out[3] = (float)climate;
    static unsigned n;
    if (n++ % 600 == 0) LOG("[light] env: tod %.2f (hour %.2f) stage %u weather %u climate %u\n", out[0], out[0] / 15, stage, weather, climate);
    return tod >= 0;
}

// Skins <a> with the bone matrices m0 + (m1 - m0) * u (bones x 12 f32, model space, the bake's layout: rows r0 r1 r2
// r3 of a row-vector matrix, v' = x r0 + y r1 + z r2 + w r3) placed at <feet> (world; NULL = Link's matrix
// translation) facing <face> (rad, direction (sin, cos) in BotW x, z), KH cm -> BotW units * <scale>, and hands the
// frame (with the camera) to the render thread. False (and the actor switched off) without its feet or the camera.
// (s100 slatehip2) one bone of the body actor, blended like the skin and put in the world: sora_draw_bone_world
static int g_tb_req = -1;
static struct { bool ok; int bone; float w[12], face, scale; double tick; } g_tb;
// (s128 wpn, recon/S128_HELPER_EQUIP.md Q3) Sora's Keyblade grip = bone 228 of his 240-bone bake (origin = the hit-bake grip,
// blade along local -Y, 78 cm): tracked on every body draw for Link's BotW weapon in Sora's hand (linkpos.c weapon section).
// (s137 roxwpn) Roxas' bake has 241 bones (229 body + 12 Keyblade), so his Keyblade grip is bone 229 (VERIFIED: same frame, recon/S137_HELPER_ROXWPN.md)
#define GRIP_BONE_OF(a) ((a)->nbones == 241u ? 229u : 228u)
#define GRIP_FENCE() __atomic_thread_fence(__ATOMIC_SEQ_CST)
static struct { volatile uint32_t seq; float w[12], scale, vel[3]; double tick; uint32_t nbones; } g_grip;  // (s129 vel = Sora's feet m/s, for the weapon lead)
// (mp2-perf C1) The vertex skin of skin_publish. bl = the bones already blended (nbones x 12), else m0 + (m1 - m0) * u per
// influence (the old path); both give the same floats (the same expression per element). out = nverts x VB_F, x y z u v written.
typedef struct {
    const float *m0, *m1, *bl, *wm;
    float u, ws, wc, tx, ty, tz, c, s, scale;
    bool wat;
    float *out;
} SkinIn;
static void skin_verts(const DrawActor *a, const SkinIn *k0) {
    const float *m0 = k0->m0, *m1 = k0->m1, *bl = k0->bl, *wm = k0->wm;
    const float u = k0->u, ws = k0->ws, wc = k0->wc, tx = k0->tx, ty = k0->ty, tz = k0->tz, c = k0->c, s = k0->s, scale = k0->scale;
    const bool wat = k0->wat;
    for (uint32_t v = 0; v < a->nverts; v++) {
        float q[3] = {0, 0, 0};
        const MVert *mv = &a->verts[v];
        if (wat && mv->n && mv->first < a->ninfl && a->infl[mv->first].bone >= (uint32_t)a->wbase) {
            for (uint32_t k = mv->first; k < mv->first + mv->n && k < a->ninfl; k++) {
                const MInfl *in = &a->infl[k];
                if (in->bone < (uint32_t)a->wbase || in->bone >= a->nbones) continue;
                const float *mw = wm + (in->bone - (uint32_t)a->wbase) * 12;
                for (int r = 0; r < 4; r++)
                    for (int j = 0; j < 3; j++) q[j] += in->p[r] * mw[r * 3 + j];
            }
            float *o = k0->out + v * VB_F, sc = a->wat.scale;
            o[0] = a->wat.pos[0] + (wc * q[0] + ws * q[2]) * sc;
            o[1] = a->wat.pos[1] + q[1] * sc;
            o[2] = a->wat.pos[2] + (-ws * q[0] + wc * q[2]) * sc;
            o[3] = mv->u, o[4] = mv->v;
            continue;
        }
        for (uint32_t k = mv->first; k < mv->first + mv->n && k < a->ninfl; k++) {
            const MInfl *in = &a->infl[k];
            if (in->bone >= a->nbones) continue;
            if (bl) {
                const float *mb = bl + in->bone * 12;
                for (int r = 0; r < 4; r++)  // row vectors: q += p[r] * row r
                    for (int j = 0; j < 3; j++) q[j] += in->p[r] * mb[r * 3 + j];
            } else {
                const float *ma = m0 + in->bone * 12, *mb = m1 + in->bone * 12;
                for (int r = 0; r < 4; r++)
                    for (int j = 0; j < 3; j++) q[j] += in->p[r] * (ma[r * 3 + j] + (mb[r * 3 + j] - ma[r * 3 + j]) * u);
            }
        }
        float *o = k0->out + v * VB_F;
        o[0] = tx + (c * q[0] + s * q[2]) * scale;
        o[1] = ty + q[1] * scale;
        o[2] = tz + (-s * q[0] + c * q[2]) * scale;
        o[3] = mv->u, o[4] = mv->v;
    }
}
// (s183 amd FIX 2, game thread) BotW's world camera projection for the body pass: {near, far, fovy rad, aspect} and its
// source (sora_proj_core.h: the camera scan's, else the exe's own, else none = no scene-depth occlusion that frame).
// SAVAGEWIND_SORA_PROJ_SRC = auto (default) / exe / scan. One log line per change of source (the first 20, then every 100th).
static const char *proj_src_name(int src) {
    return src == PROJ_SRC_SCAN ? "scan" : src == PROJ_SRC_EXE ? "exe" : src == 3 ? "screen" : src == 4 ? "override" : "none";
}
static int world_proj(float proj[4]) {
    static int mode = -1, last = -1;
    static unsigned changes;
    static bool told_disagree;
    if (mode < 0) {
        const char *e = getenv("SAVAGEWIND_SORA_PROJ_SRC");
        mode = !e || !*e ? PROJ_AUTO : (!strcmp(e, "exe") || !strcmp(e, "1")) ? PROJ_EXE : (!strcmp(e, "scan") || !strcmp(e, "2")) ? PROJ_SCAN : PROJ_AUTO;
        LOG("[sora-draw] projection source: %s (SAVAGEWIND_SORA_PROJ_SRC=%s)\n", mode == PROJ_EXE ? "exe first" : mode == PROJ_SCAN ? "scan only" : "auto (scan, else exe)",
            e && *e ? e : "-");
    }
    float s[4] = {0, 0, 0, 0}, x[4];
    WorldProj wp;
    const bool sok = linkpos_camera_proj(s), eok = output_world_proj(&wp);
    x[0] = wp.tan_half_fovy, x[1] = wp.aspect, x[2] = wp.near_, x[3] = wp.far_;
    const int src = proj_pick(mode, sok, s, eok, x, proj);
    if (src != last && (++changes <= 20 || changes % 100 == 0)) {
        if (src == PROJ_SRC_NONE)
            LOG("[sora-draw] projection: NONE (no scanned gameplay projection, exe's %s): guessed one, scene-depth occlusion off (change %u)\n",
                eok ? "not sane" : "not built yet", changes);
        else
            LOG("[sora-draw] projection: %s near %.3f far %.0f fovy %.2f aspect %.4f (scan %s, exe %s; change %u)\n", proj_src_name(src),
                proj[0], proj[1], proj[2] * 57.29578f, proj[3], sok ? "found" : "none", eok && proj_exe_sane(x) ? "built" : "none", changes);
    }
    last = src;
    if (sok && eok && proj_exe_sane(x) && !proj_agree(s, x) && !told_disagree) {
        told_disagree = true;  // (log only, once: auto keeps the scan's unless the aspect differs)
        LOG("[sora-draw] projection: scan and exe differ: scan near %.3f far %.0f fovy %.2f aspect %.4f, exe near %.3f far %.0f fovy %.2f aspect %.4f\n",
            s[0], s[1], s[2] * 57.29578f, s[3], x[2], x[3], 2 * atanf(x[0]) * 57.29578f, x[1]);
    }
    return src;
}

static bool skin_publish(DrawActor *a, const float *m0, const float *m1, float u, const float feet[3], float face,
                         float scale) {
    float lm[12], view[12], proj[4], t[3];
    const int pfi = a == &g_act[g_body] ? 0 : 1;  // (mp2-perf timers: 0 = the body, 1 = the others)
    const uint64_t pf_t0 = plat_now_ns();
    if (feet) t[0] = feet[0], t[1] = feet[1], t[2] = feet[2];
    else if (linkpos_link_mtx(lm)) t[0] = lm[3], t[1] = lm[7], t[2] = lm[11];
    else { g_dg_skip = "DC Link's position was not found (the body has nowhere to stand)"; sora_draw_actor_off((int)(a - g_act)); return false; }
    const bool scr = g_scr.on;  // (s59: the KH2 Game Over's own camera, not BotW's)
    if (g_camo.on && ++g_camo.age > 30) g_camo.on = false, LOG("[sora-draw] camera override lapsed (no call for 30 poses)\n");
    const bool co = !scr && g_camo.on;  // (s106 invsora 3: the menu scene's camera in the normal pass)
    if (scr) memcpy(view, g_scr.view, sizeof view);
    else if (co) memcpy(view, g_camo.view, sizeof view);
    if (!(scr || co || linkpos_camera_view(view)) || !(t[0] == t[0] && t[1] == t[1] && t[2] == t[2])) {  // (a NaN foot: not drawn)
        g_dg_skip = "DD BotW's camera was not found (the body cannot be placed on screen)";
        sora_draw_actor_off((int)(a - g_act));
        return false;
    }
    const int psrc = scr ? 3 : co ? 4 : world_proj(proj);  // (s183 amd FIX 2: 3 / 4 = the screen / override camera's own)
    bool have_proj = scr ? (memcpy(proj, g_scr.proj, sizeof proj), true) : co ? (memcpy(proj, g_camo.proj, sizeof proj), true) : psrc != PROJ_SRC_NONE;
    // model (KH: +x left, +y up, +z forward, cm) -> world: RotY(face) * scale, then the feet
    float s = sinf(face), c = cosf(face), tx = t[0], ty = t[1], tz = t[2];
    // (s58) a detached weapon: its vertices (first influence on a bone >= wbase) skinned with the weapon's own
    // matrices (given, or the hand pose made relative to its first bone: W_b x W_base^-1, rigid) and placed at wat.pos
    // turned by wat.yaw instead of riding the hand
    const bool wat = a->wat.on && a->wbase > 0;
    float wm[WEAPON_BONES_MAX * 12], ws = sinf(a->wat.yaw), wc = cosf(a->wat.yaw);
    uint32_t nwb = wat ? a->nbones - (uint32_t)a->wbase : 0;
    if (wat && a->wat.own) memcpy(wm, a->wat.bones, nwb * 12 * sizeof(float));
    else if (wat) {
        float base[12], inv[12];
        const uint32_t b0 = (uint32_t)a->wbase;
        for (int j = 0; j < 12; j++) base[j] = m0[b0 * 12 + j] + (m1[b0 * 12 + j] - m0[b0 * 12 + j]) * u;
        mtx43_inv(base, inv);
        for (uint32_t b = 0; b < nwb; b++) {
            float mb[12];
            for (int j = 0; j < 12; j++) mb[j] = m0[(b0 + b) * 12 + j] + (m1[(b0 + b) * 12 + j] - m0[(b0 + b) * 12 + j]) * u;
            mtx43_mul(mb, inv, wm + b * 12);
        }
    }
    if (a == &g_act[g_body] && g_tb_req >= 0 && (uint32_t)g_tb_req < a->nbones) {  // (s100) the tracked bone, in the world
        float b[12], w[12];
        for (int j = 0; j < 12; j++) b[j] = m0[g_tb_req * 12 + j] + (m1[g_tb_req * 12 + j] - m0[g_tb_req * 12 + j]) * u;
        for (int r = 0; r < 3; r++) {  // axis r (row r of the bake) -> world (RotY(face)), unit length; column r of the 3x4
            float x = b[r * 3], y = b[r * 3 + 1], z = b[r * 3 + 2];
            float wx = c * x + s * z, wy = y, wz = -s * x + c * z, l = sqrtf(wx * wx + wy * wy + wz * wz);
            if (l < 1e-6f) l = 1, wx = (r == 0), wy = (r == 1), wz = (r == 2);
            w[0 * 4 + r] = wx / l, w[1 * 4 + r] = wy / l, w[2 * 4 + r] = wz / l;
        }
        w[3] = tx + (c * b[9] + s * b[11]) * scale, w[7] = ty + b[10] * scale, w[11] = tz + (-s * b[9] + c * b[11]) * scale;
        memcpy(g_tb.w, w, sizeof w);
        g_tb.bone = g_tb_req, g_tb.face = face, g_tb.scale = scale, g_tb.tick = (double)clock() / CLOCKS_PER_SEC, g_tb.ok = true;
    }
    if (a == &g_act[g_body] && GRIP_BONE_OF(a) < a->nbones) {  // (s128 wpn) the Keyblade grip bone, always (its own slot: g_tb is one bone)
        float b[12], w[12];
        for (int j = 0; j < 12; j++) b[j] = m0[GRIP_BONE_OF(a) * 12 + j] + (m1[GRIP_BONE_OF(a) * 12 + j] - m0[GRIP_BONE_OF(a) * 12 + j]) * u;
        for (int r = 0; r < 3; r++) {
            float x = b[r * 3], y = b[r * 3 + 1], z = b[r * 3 + 2];
            float wx = c * x + s * z, wy = y, wz = -s * x + c * z, l = sqrtf(wx * wx + wy * wy + wz * wz);
            if (l < 1e-6f) l = 1, wx = (r == 0), wy = (r == 1), wz = (r == 2);
            w[0 * 4 + r] = wx / l, w[1 * 4 + r] = wy / l, w[2 * 4 + r] = wz / l;
        }
        w[3] = tx + (c * b[9] + s * b[11]) * scale, w[7] = ty + b[10] * scale, w[11] = tz + (-s * b[9] + c * b[11]) * scale;
        // (s129) Sora's feet velocity (u/s, smoothed): the weapon's calcWorld runs before this draw, so linkpos.c leads it
        static float r_prev[3], vel[3];
        static double r_t;
        double now = (double)clock() / CLOCKS_PER_SEC, dt = now - r_t;
        if (dt > 0.004) {
            float d[3] = {tx - r_prev[0], ty - r_prev[1], tz - r_prev[2]};
            bool ok = r_t > 0 && dt < 0.25 && d[0] * d[0] + d[1] * d[1] + d[2] * d[2] < 4.0f;  // (a warp / load: no lead)
            for (int i = 0; i < 3; i++) vel[i] = ok ? 0.5f * vel[i] + 0.5f * d[i] / (float)dt : 0;
            r_prev[0] = tx, r_prev[1] = ty, r_prev[2] = tz, r_t = now;
        }
        g_grip.seq++;
        GRIP_FENCE();
        memcpy(g_grip.vel, vel, sizeof vel);
        memcpy(g_grip.w, w, sizeof w);
        g_grip.scale = scale, g_grip.tick = (double)clock() / CLOCKS_PER_SEC, g_grip.nbones = a->nbones;
        GRIP_FENCE();
        g_grip.seq++;
    }
    const int rk = (int)(a->rn % DRAW_RING);
    bool direct = false, ver3 = false;
    float *dst = a->local;
    {
        SkinIn si = {.m0 = m0, .m1 = m1, .u = u, .wat = wat, .wm = wm, .ws = ws, .wc = wc, .tx = tx, .ty = ty, .tz = tz, .c = c, .s = s,
                     .scale = scale};
        const bool c1 = (g_perf & PERF_C1) != 0, ver = g_perf_verify != 0;
        if (c1 || ver) {  // (mp2-perf C1) every bone blended once: m0 + (m1 - m0) * u, the skin's own expression per element
            if (!a->bl) a->bl = malloc((size_t)a->nbones * 12 * sizeof(float));
            if (a->bl) {
                for (uint32_t b = 0; b < a->nbones; b++)
                    for (int j = 0; j < 12; j++) a->bl[b * 12 + j] = m0[b * 12 + j] + (m1[b * 12 + j] - m0[b * 12 + j]) * u;
            }
        }
        if (ver) {  // one-shot: the old and the new skin of this very input into two buffers, memcmp logged
            g_perf_verify = 0;
            const size_t nb = (size_t)a->nverts * VB_F * sizeof(float);
            float *o0 = calloc(1, nb), *o1 = calloc(1, nb);
            if (o0 && o1 && a->bl) {
                si.bl = NULL, si.out = o0, skin_verts(a, &si);
                si.bl = a->bl, si.out = o1, skin_verts(a, &si);
                float md = 0;
                for (size_t k = 0; k < (size_t)a->nverts * VB_F; k++) {
                    float d = fabsf(o0[k] - o1[k]);
                    if (d > md || d != d) md = d;
                }
                LOG("[sora-draw] drawperf verify C1 (actor %d, %u verts, %u bones): memcmp %d, max abs diff %g\n", (int)(a - g_act), a->nverts,
                    a->nbones, memcmp(o0, o1, nb), md);
            } else LOG("[sora-draw] drawperf verify C1: out of memory\n");
            free(o0), free(o1);
        }
        // (mp2-perf C3) skin straight into the ring slot this publish will fill (the oldest one; its tag is cleared first, under
        // g_lock, so the render thread / cast shadow never pick a slot being written), no a->local -> snap -> ring double copy
        if (g_sync < 0) {
            const char *e = getenv("SAVAGEWIND_SORA_DRAW_SYNC");
            g_sync = e && *e ? atoi(e) : 1;
            LOG("[sora-draw] drawsync %d, drawoff %d (s114: 0 = old newest snapshot, 1 = tag match, 2 = pacing)\n", g_sync, g_sync_off);
        }
        if (!a->ring[rk].vb) a->ring[rk].vb = calloc(a->nverts, VB_F * sizeof(float));  // (game thread: the only writer of ring vb pointers)
        direct = (g_perf & PERF_C3) != 0 && a->ring[rk].vb;
        if (direct) {
            pthread_mutex_lock(&g_lock);
            a->ring[rk].tag = 0;
            pthread_mutex_unlock(&g_lock);
        }
        dst = direct ? a->ring[rk].vb : a->local;
        si.bl = c1 ? a->bl : NULL, si.out = dst;
        skin_verts(a, &si);
        ver3 = ver && direct;
    }
    pf_add(&g_pf.skin[pfi], pf_ms(pf_t0));
    const uint64_t pf_t1 = plat_now_ns();
    skin_normals(a, dst, a == &g_act[g_body]);  // (s121 light)
    pf_add(&g_pf.norm[pfi], pf_ms(pf_t1));
    if (ver3) {  // (mp2-perf verify C3, game side) the old path (skin into a->local + normals) vs what went into the ring slot
        const size_t nb = (size_t)a->nverts * VB_F * sizeof(float);
        SkinIn si = {.m0 = m0, .m1 = m1, .u = u, .wat = wat, .wm = wm, .ws = ws, .wc = wc, .tx = tx, .ty = ty, .tz = tz, .c = c, .s = s,
                     .scale = scale, .out = a->local};
        skin_verts(a, &si);
        skin_normals(a, a->local, false);
        LOG("[sora-draw] drawperf verify C3 (game side, actor %d): ring-slot vb vs old a->local path: memcmp %d\n", (int)(a - g_act),
            memcmp(a->local, dst, nb));
        g_perf_verify_r = 1;  // the render thread checks its upload next
    }
    Shadow sh = a->sh;
    sh.on = g_shadow_on;
    if (!sh.set || !(sh.floor_y < ty)) sh.floor_y = ty;  // (no floor from the mod, or one above the feet: at the feet)
    const uint64_t pf_t2 = plat_now_ns();
    pthread_mutex_lock(&g_lock);
    if (!direct) memcpy(a->snap.vb, a->local, (size_t)a->nverts * VB_F * sizeof(float));
    a->snap.rs = direct ? rk + 1 : 0;  // (mp2-perf C3: the render thread / cast read the newest published slot's vb instead)
    memcpy(g_cam.view, view, sizeof view);
    memcpy(g_cam.proj, proj, sizeof proj);
    g_cam.have_proj = have_proj;
    g_cam.proj_src = psrc;
    g_cam.screen = scr;
    g_cam.shift[0] = co ? g_camo.shift[0] : 0, g_cam.shift[1] = co ? g_camo.shift[1] : 0, g_cam.co = co;  // (s107)
    memcpy(g_cam.clear, g_scr.clear, sizeof g_cam.clear);
    memcpy(g_cam.mask, g_scr.mask, sizeof g_cam.mask);
    a->snap.feet[0] = tx, a->snap.feet[1] = ty, a->snap.feet[2] = tz;
    a->snap.sh = sh;
    a->snap.ghost = a->ghost;
    memcpy(a->snap.tint, a->tint, sizeof a->tint);
    memcpy(a->snap.light, a->own_light ? a->light : g_light, sizeof a->snap.light);  // (s121 light)
    if (scr) a->snap.light[3] = 0;  // (the KH2 Game Over's own camera: unlit, as before)
    a->snap.hide_weapon = (a->hide_weapon || (a == &g_act[g_body] && linkpos_weapon_shown())) && !wat, a->snap.weapon_at = wat;  // (s58: the detached one always shows)
    a->snap.blade[0] = a->blade[0], a->snap.blade[1] = a->blade[1];
    a->snap.valid = true;
    if (a->ring[rk].vb) {  // (s114 drawsync)
        typeof(a->ring[0]) *R = &a->ring[rk];
        if (!direct) memcpy(R->vb, a->local, (size_t)a->nverts * VB_F * sizeof(float));
        memcpy(R->feet, a->snap.feet, sizeof R->feet);
        R->sh = a->snap.sh, R->ghost = a->snap.ghost, R->hide_weapon = a->snap.hide_weapon, R->weapon_at = a->snap.weapon_at;
        memcpy(R->tint, a->snap.tint, sizeof R->tint);
        memcpy(R->light, a->snap.light, sizeof R->light);
        R->blade[0] = a->snap.blade[0], R->blade[1] = a->snap.blade[1];
        R->cam = g_cam;
        R->tag = framepace_game_frame(), R->t_ns = plat_now_ns();
        a->rn++;
    }
    pthread_mutex_unlock(&g_lock);
    pf_add(&g_pf.pub[pfi], pf_ms(pf_t2));
    return true;
}

// ------------------------------------------------------------------ (s59) the KH2 Game Over's screen mode (game thread)
// recon/S59_HELPER_GAMEOVER.md: the actors posed after this call use <view> / <proj> (not BotW's camera) and are drawn
// over the window image at present (over BotW's 2D UI, whatever the ui order), with no scene depth, shadow or KH2
// effect, after <clear> (x0 y0 x1 y1, fractions of the game image) is filled black. Off: BotW's camera again from the
// next pose; the render thread drops the mode (and the snapshots taken with that camera) at once.
void sora_draw_screen(bool on, const float view[12], const float proj[4], const float clear[4]) {
    on = on && view && proj;
    if (on != g_scr.on) LOG("[sora-draw] screen mode %s\n", on ? "on (own camera, over BotW's UI, no scene depth)" : "off");
    bool was = g_scr.on;
    g_scr.on = on;
    if (!on) memset(g_scr.mask, 0, sizeof g_scr.mask);
    if (on) {
        memcpy(g_scr.view, view, sizeof g_scr.view);
        memcpy(g_scr.proj, proj, sizeof g_scr.proj);
        if (clear) memcpy(g_scr.clear, clear, sizeof g_scr.clear);
        else memset(g_scr.clear, 0, sizeof g_scr.clear);
        return;
    }
    if (!was) return;
    pthread_mutex_lock(&g_lock);
    g_cam.screen = false;  // (the snapshots were skinned for the screen camera: none drawn with BotW's)
    for (int i = 0; i < g_nact; i++) g_act[i].snap.valid = false;
    pthread_mutex_unlock(&g_lock);
}

// (s105 islayer) screen mode only: a rect (x0 y0 x1 y1, fractions of the game image) Sora is NOT drawn in (the inventory's
// description panel: he then looks to stand behind it). Takes effect with the next pose; all zero = no hole. Persists
// until changed; sora_draw_screen(false) clears it.
void sora_draw_screen_mask(float x0, float y0, float x1, float y1) {
    g_scr.mask[0] = x0, g_scr.mask[1] = y0, g_scr.mask[2] = x1, g_scr.mask[3] = y1;
}

// ------------------------------------------------------------------ the shadow + ghost settings (game thread)
// The floor under the actor this frame, world y (BotW units, the space of Link's matrix), and how dark the disc is on
// the ground (0..1; <= 0 = no shadow this frame). Call every frame BEFORE the frame's pose / world draw (the values
// ride with that frame's snapshot; they stay until the next call). A floor above the feet is clamped to the feet.
// Height above the floor = feet - floor_y shrinks and fades the disc (sora_draw_actor_shadow_shape).
void sora_draw_actor_shadow(int id, float floor_y, float strength) {
    DrawActor *a = actor_set(id);
    if (!a) return;
    a->sh.set = true;
    a->sh.floor_y = floor_y;
    a->sh.strength = strength > 1 ? 1 : strength;
}
void sora_draw_shadow(float floor_y, float strength) { sora_draw_actor_shadow(g_body, floor_y, strength); }

// The body's alpha where the scene depth hides it (0..1, 0 = off): a faint silhouette, e.g. under the water surface
// while swimming. Rides with the next skinned frame like the shadow.
void sora_draw_actor_ghost(int id, float alpha) {
    DrawActor *a = actor_set(id);
    if (a) a->ghost = alpha > 1 ? 1 : alpha < 0 ? 0 : alpha;
}
void sora_draw_ghost(float alpha) { sora_draw_actor_ghost(g_body, alpha); }

// A glow over the whole body (s54: Link's blue during the map warp): rgb 0..1, amount 0..1 (0 = off). Rides with the
// next skinned frame like the ghost.
void sora_draw_actor_tint(int id, float r, float g, float b, float amount) {
    DrawActor *a = actor_set(id);
    if (a) a->tint[0] = r, a->tint[1] = g, a->tint[2] = b, a->tint[3] = amount > 1 ? 1 : amount < 0 ? 0 : amount;
}
void sora_draw_tint(float r, float g, float b, float amount) { sora_draw_actor_tint(g_body, r, g, b, amount); }

// The disc's shape, BotW units: radius on the ground; fade_h = the height above the floor at which it is min_scale
// of the radius and min_alpha of the strength (linear in between, clamped above). <= 0 keeps the current value.
void sora_draw_actor_shadow_shape(int id, float radius, float fade_h, float min_scale, float min_alpha) {
    DrawActor *a = actor_set(id);
    if (!a) return;
    if (radius > 0) a->sh.radius = radius;
    if (fade_h > 0) a->sh.fade_h = fade_h;
    if (min_scale > 0) a->sh.min_scale = min_scale > 1 ? 1 : min_scale;
    if (min_alpha > 0) a->sh.min_alpha = min_alpha > 1 ? 1 : min_alpha;
}
void sora_draw_shadow_shape(float radius, float fade_h, float min_scale, float min_alpha) {
    sora_draw_actor_shadow_shape(g_body, radius, fade_h, min_scale, min_alpha);
}

void sora_draw_actor_weapon(int id, bool shown) {
    DrawActor *a = actor_set(id);
    if (a) a->hide_weapon = !shown;
}

void sora_draw_actor_blade(int id, int chain, int alt) {
    DrawActor *a = actor_set(id);
    if (a && (chain == 0 || chain == 1) && alt >= 0 && alt <= 255) a->blade[chain] = (uint8_t)alt;
}

// (s58) The weapon groups' first bone: the lowest first-influence bone of their vertices, checked so no body vertex
// sits on it or above (the bake hangs the weapon's bones last: Goofy 155.., Donald 158.., Sora 228..); -1 when there
// is none or a body vertex shares them.
static int weapon_base(const DrawActor *a) {
    uint32_t lo = UINT32_MAX;
    for (uint32_t g = 0; g < a->ngroups; g++) {
        const MGroup *gr = &a->groups[g];
        if (!(gr->attr & ATTR_WEAPON)) continue;
        for (uint32_t k = gr->first; k < gr->first + gr->n && k < a->nidx; k++) {
            const MVert *mv = a->idx[k] < a->nverts ? &a->verts[a->idx[k]] : NULL;
            if (mv && mv->n && mv->first < a->ninfl && a->infl[mv->first].bone < lo) lo = a->infl[mv->first].bone;
        }
    }
    if (lo == UINT32_MAX || lo == 0 || lo >= a->nbones) return -1;
    for (uint32_t g = 0; g < a->ngroups; g++) {  // (a body vertex on the weapon's bones would fly with it)
        const MGroup *gr = &a->groups[g];
        if (gr->attr & ATTR_WEAPON) continue;
        for (uint32_t k = gr->first; k < gr->first + gr->n && k < a->nidx; k++) {
            const MVert *mv = a->idx[k] < a->nverts ? &a->verts[a->idx[k]] : NULL;
            if (mv && mv->n && mv->first < a->ninfl && a->infl[mv->first].bone >= lo) return -1;
        }
    }
    return (int)lo;
}

// (s58, Goofy Bash) The weapon drawn detached: sora_draw.h has the rules.
void sora_draw_actor_weapon_at(int id, const float *bones, int n, const float pos[3], float yaw, float scale) {
    DrawActor *a = actor(id);
    if (!a) return;
    if (!pos || !(pos[0] == pos[0] && pos[1] == pos[1] && pos[2] == pos[2])) { a->wat.on = false; return; }
    if (!a->wbase) {
        a->wbase = weapon_base(a);
        LOG("[sora-draw] actor %d: weapon bones %d..%u%s\n", id, a->wbase, a->nbones - 1,
            a->wbase > 0 ? "" : " (none: never drawn detached)");
    }
    int nb = a->wbase > 0 ? (int)a->nbones - a->wbase : 0;
    // (s58 live: Goofy's 159 bones = 155 body + W_EX030's 4, every shield vertex on the last one -> wbase 158, nb 1,
    // while the flight motion has all 4 weapon bones) more bones given = the weapon skeleton from its root: the mesh's
    // bones are its last nb
    if (bones && n > nb && nb > 0) bones += (size_t)(n - nb) * 12, n = nb;
    if (nb <= 0 || nb > WEAPON_BONES_MAX || (bones && n != nb)) {
        static int said;
        if (!said++) LOG("[sora-draw] actor %d: weapon_at with %d bones, its weapon has %d: not drawn detached\n", id, n, nb);
        a->wat.on = false;
        return;
    }
    a->wat.on = true, a->wat.own = bones != NULL;
    if (bones) memcpy(a->wat.bones, bones, (size_t)nb * 12 * sizeof(float));
    memcpy(a->wat.pos, pos, sizeof a->wat.pos);
    a->wat.yaw = yaw, a->wat.scale = scale;
}

// Pose of mset slot <slot> at <clock> KH frames (between baked frames: matrices blended), the actor at <feet> (NULL:
// Link's) facing <face> (rad, direction (sin, cos) in BotW x, z), KH cm -> BotW units * <scale>. Unknown slot: its
// last pose stays.
void sora_draw_actor_pose(int id, int slot, float clock, const float feet[3], float face, float scale) {
    DrawActor *a = actor(id);
    if (!a) return;
    const PDir *d = pose_slot(a, slot);
    if (!d) d = pose_slot(a, slot = a->last_slot), clock = a->last_clock;
    if (!d) { sora_draw_actor_off(id); return; }
    a->last_slot = slot, a->last_clock = clock;
    const float *m0, *m1;
    float u;
    pose_sample(a, d, clock, &m0, &m1, &u);
    skin_publish(a, m0, m1, u, feet, face, scale);
}
void sora_draw_pose(int slot, float clock, float face, float scale) { sora_draw_actor_pose(g_body, slot, clock, NULL, face, scale); }

// ------------------------------------------------------------------ the pose in the mod's hands (game thread)
// For a retarget living in sora.c (hot reload; SAVAGE_WIND_RECON "Session 22"): the mod takes the baked pose of a
// motion, edits bones, and draws with the result. All three run on the game thread like sora_draw_pose (the bake is
// read-only once loaded; the actor's local buffer is the game thread's). Exported like every other function of the exe
// (the Makefile's --export-all-symbols + build/savagewind.dll.a: the import library learns the new names at the exe's
// next link).

// Bones in the actor's bake (Sora's 240: the Keyblade's hang on from 228 when baked with it; Donald 162 = 158 + the
// staff's 4 on bone 131, Goofy 159 = 155 + the shield's 4 on bone 101), 0 without it. The bake has no parent indices
// (sorabake 'SKRG' / 'SKRP' carry none): the mod needs its own table of the KH2 skeleton.
int sora_draw_actor_bone_count(int id) {
    DrawActor *a = actor(id);
    return a ? (int)a->nbones : 0;
}
int sora_draw_bone_count(void) { return sora_draw_actor_bone_count(g_body); }

// The baked model-space bone matrices of mset slot <slot> at <clock> KH frames (clamped to the slot's frames; between
// baked frames blended, exactly as the pose draw samples them) into <out>: bone count x 12 f32, the bake's layout
// (rows r0 r1 r2 r3 of a row-vector matrix, model space: KH +x left, +y up, +z forward, cm, root move taken out, feet
// at y = 0). False (nothing written) without the actor or the slot; the last-pose fallback is not applied here.
bool sora_draw_actor_slot_pose(int id, int slot, float clock, float *out) {
    DrawActor *a = actor(id);
    if (!a || !out) return false;
    const PDir *d = pose_slot(a, slot);
    if (!d) return false;
    const float *m0, *m1;
    float u;
    pose_sample(a, d, clock, &m0, &m1, &u);
    size_t n = (size_t)a->nbones * 12;
    if (u == 0)
        memcpy(out, m0, n * sizeof(float));
    else
        for (size_t i = 0; i < n; i++) out[i] = m0[i] + (m1[i] - m0[i]) * u;
    return true;
}
bool sora_draw_slot_pose(int slot, float clock, float *out) { return sora_draw_actor_slot_pose(g_body, slot, clock, out); }

// Draws with <bones> (bone count x 12 f32, the space and layout sora_draw_actor_slot_pose writes: model space, so a
// pose read with it, edited, comes straight back) placed like the pose draw: at <feet> (NULL: Link's matrix) facing
// <face>, times <scale>. Model space rather than world because the skinning (skin_publish) places each vertex once; a
// world target (Link's joint at world w) goes into this space the opposite way: with feet t, s = sin(face), c =
// cos(face), d = (w - t) / scale: x = c d.x - s d.z, y = d.y, z = s d.x + c d.z. Nothing drawn (the actor switched off)
// without its feet or the camera; the matrices are read during the call only.
void sora_draw_actor_world(int id, const float *bones, const float feet[3], float face, float scale) {
    DrawActor *a = actor(id);
    if (!a || !bones) return;
    skin_publish(a, bones, bones, 0, feet, face, scale);
}
// (s100 slatehip2, game thread) Where bone <bone> of the body actor was drawn last (the skin of the last pose / world call):
// out = sead Matrix34f row-major 3x4 (columns = the bone's axes in the world, unit length; translation = the bone's joint,
// BotW world), fs = {facing rad, KH cm -> BotW scale} of that draw. False: the first call (it starts tracking the bone;
// the next draw fills it), a different bone than last asked, or no draw for 0.3 s.
bool sora_draw_bone_world(int bone, float out[12], float fs[2]) {
    g_tb_req = bone;
    if (!g_tb.ok || g_tb.bone != bone || (double)clock() / CLOCKS_PER_SEC - g_tb.tick > 0.3) return false;
    memcpy(out, g_tb.w, sizeof g_tb.w);
    if (fs) fs[0] = g_tb.face, fs[1] = g_tb.scale;
    return true;
}
// (s128 wpn, any thread) Sora's Keyblade grip (bone 228) as last drawn: out = sead Matrix34f row-major (columns = the bone's
// axes in the world, unit; translation = the grip), *scale = KH cm -> BotW units. False: no body draw for 0.1 s / a bake
// without bone 228 / a torn read.
bool sora_draw_grip_world(float out[12], float *scale, float vel_out[3]) {
    for (int n = 0; n < 8; n++) {
        uint32_t q0 = g_grip.seq;
        GRIP_FENCE();
        float w[12], vel[3];
        memcpy(w, g_grip.w, sizeof w);
        memcpy(vel, g_grip.vel, sizeof vel);
        float sc = g_grip.scale;
        double t = g_grip.tick;
        GRIP_FENCE();
        if ((q0 & 1) || q0 != g_grip.seq) continue;
        if (!q0 || (double)clock() / CLOCKS_PER_SEC - t > 0.1) return false;
        memcpy(out, w, sizeof w);
        if (scale) *scale = sc;
        if (vel_out) memcpy(vel_out, vel, sizeof vel);
        return true;
    }
    return false;
}
void sora_draw_world(const float *bones, float face, float scale) { sora_draw_actor_world(g_body, bones, NULL, face, scale); }

// ------------------------------------------------------------------ the GPU side (render thread)
// Push constants: mvp; k = (alpha test, blended, scene depth mode, debug); vp = the game image in the window (x, y, w,
// h); duv = the scene viewport in the scene depth (uv origin, size); z = (game near, far, bias, shadow). Scene depth
// modes: 0 none, 1 NVN depth mode "minus one" (GL: d = (z_ndc + 1) / 2), 2 zero-to-one. Sora's eye depth = clip w.
// z.w > 0.5 = the shadow disc: uv = the point on the unit disc, k.x = its centre alpha, black with a radial falloff
// (full inside 0.55, soft to the rim), the same scene-depth test (with the bigger bias z.z) hides it under things.
// z.w 1 = the old flat disc at sora.c's floor (s0.x; no scene depth, or SHADOW_DROP=0). z.w 2 = the glide shadow
// (s39 option B, a decal): a full-viewport quad (pos.xy = NDC from the CPU), the fragment shader rebuilds the world point
// of the scene depth under each pixel (mvp holds the inverse here: mat3 = inverse of the mvp's x / y / w rows, col 3 =
// the camera) and shades it when it lies within the disc around his feet (s2.xyz) and up to s1.z u below them; its
// depth below the feet shrinks the disc and fades it as sora_draw_shadow_shape describes, steep surfaces (walls) fade
// out. s38 did the floor search in the vertex shader: a VS-visible scene depth loses the GPU on NVIDIA ("device fault:
// address write invalid"), so binding 1 is fragment-only. s0 = (sora.c floor, radius, fade_h, min_scale), s1 =
// (min_alpha, -, search depth, lift), s2 = (feet, -).
// (s183 amd) Only mvp k vp duv z (128 bytes) are push constants: AMD's Windows driver and Mesa Intel allow 128 bytes
// (Vulkan's minimum; NVIDIA / RADV 256), and the old 240-byte block (all of the below in it) failed gpu_init there = no
// body and no KH2 effects, the HUD fine. s0 s1 s2 t oa ob oc (112 bytes) are a per-draw uniform block instead (set 2,
// one dynamic-offset slot per push in a per-pass transient span, PdRing), the same values the push had: pd.* = pc.* was.
// (s217 depth) org c3: the bodies (z.w 0) are drawn relative to their actor: clip = mvp3 * (pos - org) + c3, org = the
// actor's feet, c3 = mvp * (org, 1) made in double on the CPU (pd_set_org). mvp * (world pos, 1) in float32 cancels
// terms of ~2000 to a clip z of ~3: with BotW's near 0.1 (shrines) our depth was off by up to ~1 cm at 2.5 m and inner
// layers (Roxas' jacket lining, 0.3-10 mm behind) won the depth test. The same projection, only without that rounding.
#define PC_DECL "layout(push_constant) uniform P { mat4 mvp; vec4 k; vec4 vp; vec4 duv; vec4 z; } pc;\n" \
                "layout(set = 2, binding = 0) uniform D { vec4 s0; vec4 s1; vec4 s2; vec4 t; vec4 oa; vec4 ob; vec4 oc; vec4 org; vec4 c3; } pd;\n"
typedef struct { float mvp[16], k[4], vp[4], duv[4], z[4]; } BodyPC;                   // = P (sora_fx_draw reads its 32 floats)
typedef struct { float s0[4], s1[4], s2[4], t[4], oa[4], ob[4], oc[4], org[4], c3[4]; } BodyPD;  // = D (std140: 9 vec4, no padding)
#define PC_BYTES 128
#define PD_BYTES 144
#define PC_BYTES_S182 240  // (the old all-push block, for the log line only)
_Static_assert(sizeof(BodyPC) == PC_BYTES, "BodyPC must match the GLSL push block");
_Static_assert(PC_BYTES <= 128, "Sora's body pass must fit Vulkan's minimum maxPushConstantsSize (128): AMD Windows / Intel");
_Static_assert(sizeof(BodyPD) == PD_BYTES, "BodyPD must match the GLSL uniform block D");
_Static_assert(offsetof(BodyPD, oc) == 96 && offsetof(BodyPD, org) == 112 && offsetof(BodyPD, c3) == 128,
               "BodyPD members must sit at the GLSL block D's std140 offsets (vec4 = 16 bytes each, in order)");
static const char *VERT_SRC =
    "#version 450\n"
    "layout(location = 0) in vec3 pos;\n"
    "layout(location = 1) in vec2 uv;\n"
    PC_DECL
    "layout(location = 0) out vec2 v_uv;\n"
    "layout(location = 1) out float v_a;\n"
    "layout(location = 2) out vec3 v_w;\n"
    // (s121 light) the CPU smooth normal (0 = none) + the light block per instance (sun dir + style, sun rgb + amount,
    // ambient top + floor, ambient bottom + shadow), passed flat to the fragment shader
    "layout(location = 2) in vec3 nrm;\n"
    "layout(location = 3) in vec4 l0;\n"
    "layout(location = 4) in vec4 l1;\n"
    "layout(location = 5) in vec4 l2;\n"
    "layout(location = 6) in vec4 l3;\n"
    "layout(location = 3) out vec3 v_n;\n"
    "layout(location = 4) flat out vec4 v_l0;\n"
    "layout(location = 5) flat out vec4 v_l1;\n"
    "layout(location = 6) flat out vec4 v_l2;\n"
    "layout(location = 7) flat out vec4 v_l3;\n"
    // (s124 b) BotW's sun shadow: s0 = (mode, strength, chest radius m, bias m), s1 = (chest world xyz, flags),
    // s2 = (offset toward the sun m, per-pixel radius m, -, -), m0..m3 = the rows of BotW's world -> cascade 0 clip matrix.
    // Out: v_sc = this vertex's cascade clip (per pixel), v_scc = the chest's (flat), v_sp = s0, v_sq = (radius uv x, y,
    // bias in depth units, flags) for the mode's radius.
    "layout(location = 7) in vec4 s0;\n"
    "layout(location = 8) in vec4 s1;\n"
    "layout(location = 9) in vec4 s2;\n"
    "layout(location = 10) in vec4 m0;\n"
    "layout(location = 11) in vec4 m1;\n"
    "layout(location = 12) in vec4 m2;\n"
    "layout(location = 13) in vec4 m3;\n"
    "layout(location = 14) in vec4 s3;\n"  // (s124b) the shade colour: sky hue rgb, - (s2.z = darkness, s2.w = cool)
    "layout(location = 8) out vec4 v_sc;\n"
    "layout(location = 9) flat out vec4 v_scc;\n"
    "layout(location = 10) flat out vec4 v_sp;\n"
    "layout(location = 11) flat out vec4 v_sq;\n"
    "layout(location = 12) flat out vec4 v_ss;\n"
    "layout(location = 13) flat out vec4 v_sk;\n"
    "vec4 sclip(vec3 p) { vec4 q = vec4(p, 1.0); return vec4(dot(m0, q), dot(m1, q), dot(m2, q), dot(m3, q)); }\n"
    "void main() {\n"
    "    v_uv = uv; v_a = 1.0; v_w = pos;\n"
    "    v_n = nrm; v_l0 = l0; v_l1 = l1; v_l2 = l2; v_l3 = l3;\n"
    "    v_sp = s0; v_sc = vec4(0.0, 0.0, 0.0, 1.0); v_scc = v_sc; v_sq = vec4(0.0); v_ss = s2; v_sk = s3;\n"
    "    if (s0.x > 0.5) {\n"
    "        v_sc = sclip(pos + normalize(l0.xyz + vec3(0.0, 1e-6, 0.0)) * s2.x);\n"
    "        v_scc = sclip(s1.xyz);\n"
    "        int fl = int(s1.w + 0.5);\n"
    "        float r = (int(s0.x + 0.5) == 2 || int(s0.x + 0.5) == 4) ? s2.y : s0.z;\n"
    "        v_sq = vec4(r * 0.5 * length(m0.xyz), r * 0.5 * length(m1.xyz), s0.w * length(m2.xyz) * ((fl & 2) != 0 ? 1.0 : 0.5), s1.w);\n"
    "    }\n"
    "    if (pc.z.w < 0.5) {\n"  // (s217 depth) the bodies: relative to the actor (see PC_DECL)
    "        vec3 d = pos - pd.org.xyz;\n"
    "        gl_Position = pc.mvp[0] * d.x + pc.mvp[1] * d.y + pc.mvp[2] * d.z + pd.c3;\n"
    "        return;\n"
    "    }\n"
    "    if (pc.z.w > 1.5) { gl_Position = vec4(pos.xy, 0.5, 1.0); return; }\n"
    "    float fy = pd.s0.x;\n"
    "    float t = pd.s0.z > 0.0 ? clamp((pos.y - fy) / pd.s0.z, 0.0, 1.0) : 0.0;\n"
    "    float r = pd.s0.y * (1.0 - (1.0 - pd.s0.w) * t);\n"
    "    v_a = 1.0 - (1.0 - pd.s1.x) * t;\n"
    "    gl_Position = pc.mvp * vec4(pos.x + uv.x * r, fy + pd.s1.w, pos.z + uv.y * r, 1.0);\n"
    "}\n";
static const char *FRAG_SRC =
    "#version 450\n"
    "layout(set = 0, binding = 0) uniform sampler2D tex;\n"
    "layout(set = 1, binding = 0) uniform sampler2D sdepth;\n"
    "layout(set = 1, binding = 1) uniform sampler2D sdw;\n"  // (s124 b) BotW's sun cascade layer 0 (depth)
    PC_DECL
    "layout(location = 0) in vec2 v_uv;\n"
    "layout(location = 1) in float v_a;\n"
    "layout(location = 2) in vec3 v_w;\n"
    "layout(location = 3) in vec3 v_n;\n"
    "layout(location = 4) flat in vec4 v_l0;\n"
    "layout(location = 5) flat in vec4 v_l1;\n"
    "layout(location = 6) flat in vec4 v_l2;\n"
    "layout(location = 7) flat in vec4 v_l3;\n"
    "layout(location = 8) in vec4 v_sc;\n"
    "layout(location = 9) flat in vec4 v_scc;\n"
    "layout(location = 10) flat in vec4 v_sp;\n"
    "layout(location = 11) flat in vec4 v_sq;\n"
    "layout(location = 12) flat in vec4 v_ss;\n"
    "layout(location = 13) flat in vec4 v_sk;\n"
    "layout(location = 0) out vec4 o;\n"
    // (s124 b) how much sun reaches cascade clip point c: 16 taps on a disc (radius v_sq.xy in uv), each lit when its depth
    // is not behind BotW's (flags bit 0 = flip uv y, bit 1 = depth 0..1 not -1..1, bit 2 = greater-is-nearer), faded to
    // full sun near the cascade's edge (e = 0 outside). All conventions GUESS until the debug view (mode 3/4) confirms.
    "float sun_vis(vec4 c, out float e) {\n"
    "    e = 0.0;\n"
    "    if (c.w <= 1e-6) return 1.0;\n"
    "    vec3 n = c.xyz / c.w;\n"
    "    int fl = int(v_sq.w + 0.5);\n"
    "    vec2 uv = n.xy * 0.5 + 0.5;\n"
    "    if ((fl & 1) != 0) uv.y = 1.0 - uv.y;\n"
    "    float d = (fl & 2) != 0 ? n.z : n.z * 0.5 + 0.5;\n"
    "    e = 1.0 - smoothstep(0.85, 1.0, max(abs(n.x), abs(n.y)));\n"
    "    if (e <= 0.0) return 1.0;\n"
    "    float lit = 0.0;\n"
    "    for (int i = 0; i < 16; i++) {\n"
    "        float a = float(i) * 2.39996, r = sqrt((float(i) + 0.5) / 16.0);\n"
    "        float m = textureLod(sdw, uv + vec2(cos(a), sin(a)) * r * v_sq.xy, 0.0).r;\n"
    "        lit += ((fl & 4) != 0 ? d + v_sq.z >= m : d - v_sq.z <= m) ? 1.0 : 0.0;\n"
    "    }\n"
    "    return mix(1.0, lit / 16.0, e);\n"
    "}\n"
    // (s121 light) styles (all shapes GUESS): 1 KH2 soft = the painted shading stays the main one, the shade side is
    // floor x the lit side's brightness in the ambient top's hue; 2 toon = the same two colours with a hard edge at
    // N.L 0.2; 3 BotW soft = half-lambert sun + sky / ground ambient. amount: 0 = no change .. 1 = the full light.
    "vec3 light_of(vec3 N, float sv) {\n"  // (s124 b: sv = sun reaching this pixel, 1 = no shadow)
    "    vec3 L = normalize(v_l0.xyz + vec3(0.0, 1e-6, 0.0));\n"
    "    float d = dot(N, L), st = v_l0.w;\n"
    "    vec3 sun = v_l1.rgb, amb = mix(v_l3.rgb, v_l2.rgb, N.y * 0.5 + 0.5);\n"
    "    float smax = max(max(sun.r, sun.g), max(sun.b, 1e-3)), amax = max(max(v_l2.r, v_l2.g), max(v_l2.b, 1e-3));\n"
    "    vec3 shade = v_l2.rgb / amax * v_l2.w * smax, lit;\n"  // (shade keeps the sun's brightness: in shadow = the shade side)
    "    if (st < 1.5) lit = mix(shade, sun, max(d, 0.0));\n"
    "    else if (st < 2.5) lit = mix(shade, sun, smoothstep(0.15, 0.25, d));\n"
    "    else { float h = d * 0.5 + 0.5; lit = sun * h * h + amb; }\n"
    "    if (sv < 1.0) {\n"
    "        vec3 b = st < 2.5 ? shade : amb, w = vec3(0.3, 0.59, 0.11);\n"
    "        vec3 sk = v_sk.rgb * (dot(b, w) / max(dot(v_sk.rgb, w), 1e-3));\n"
    "        vec3 shd = mix(b, sk, clamp(v_ss.w, 0.0, 1.0)) * max(v_ss.z, 0.0);\n"
    "        lit = mix(shd, lit, sv);\n"
    "    }\n"
    "    return mix(vec3(1.0), lit, clamp(v_l1.w, 0.0, 1.0));\n"
    "}\n"
    "float lin_z(float d) {\n"
    "    float n = pc.z.x, f = pc.z.y;\n"
    "    return pc.k.z < 1.5 ? 2.0 * n * f / (f + n - (2.0 * d - 1.0) * (f - n)) : n * f / (f - d * (f - n));\n"
    "}\n"
    "float scene_z(vec2 s) {\n"  // the scene's eye depth at viewport point s (0..1)
    "    return lin_z(texture(sdepth, pc.duv.xy + s * pc.duv.zw).r);\n"
    "}\n"
    "void main() {\n"
    "    vec2 s = (gl_FragCoord.xy - pc.vp.xy) / pc.vp.zw;\n"
    // (s121 light) the triangle's side that faces the camera (screen x right / y down: any winding), before any discard
    "    vec3 ngt = -cross(dFdx(v_w), dFdy(v_w));\n"
    "    if (pc.z.w > 1.5) {\n"
    "        float zs = scene_z(s);\n"
    "        vec3 p = mat3(pc.mvp) * (vec3(s * 2.0 - 1.0, 1.0) * zs) + pc.mvp[3].xyz;\n"
    "        vec3 nr = cross(dFdx(p), dFdy(p));\n"
    "        float dy = pd.s2.y - p.y, lnr = length(nr);\n"
    "        if (dy < -0.3 || dy > pd.s1.z) discard;\n"
    "        float t = pd.s0.z > 0.0 ? clamp(max(dy, 0.0) / pd.s0.z, 0.0, 1.0) : 0.0;\n"
    "        float r = pd.s0.y * (1.0 - (1.0 - pd.s0.w) * t), q = length(p.xz - pd.s2.xz) / max(r, 0.01);\n"
    "        float a = pc.k.x * (1.0 - (1.0 - pd.s1.x) * t) * (1.0 - smoothstep(0.55, 1.0, q));\n"
    "        if (lnr > 0.0) a *= smoothstep(0.3, 0.6, abs(nr.y) / lnr);\n"
    "        if (q >= 1.0 || a < 0.004) discard;\n"
    "        o = vec4(0.0, 0.0, 0.0, a);\n"
    "        return;\n"
    "    }\n"
    "    vec4 c = texture(tex, v_uv);\n"
    "    if (pc.z.w > 0.5) {\n"
    "        c = vec4(0.0, 0.0, 0.0, pc.k.x * v_a * (1.0 - smoothstep(0.55, 1.0, length(v_uv))));\n"
    "        if (c.a < 0.004) discard;\n"
    "    } else if (c.a < pc.k.x) discard;\n"
    "    if (pd.oa.w > 0.5 && pc.z.w < 0.5 && pc.k.w < 0.5) {\n"  // (s107 slateocc) the Slate box: oa/ob = local axes 0/1, oc = centre, s0 = (camera, half2), s1.zw = half0/1
    "        vec3 a0 = pd.oa.xyz, a1 = pd.ob.xyz, a2 = cross(a0, a1), o0 = pd.s0.xyz - pd.oc.xyz, d0 = v_w - pd.s0.xyz;\n"
    "        vec3 ro = vec3(dot(o0, a0), dot(o0, a1), dot(o0, a2)), rd = vec3(dot(d0, a0), dot(d0, a1), dot(d0, a2));\n"
    "        vec3 hb = vec3(pd.s1.z, pd.s1.w, pd.s0.w);\n"
    "        vec3 inv = 1.0 / (rd + vec3(equal(rd, vec3(0.0))) * 1e-9), t1 = (-hb - ro) * inv, t2 = (hb - ro) * inv;\n"
    "        float tn = max(max(min(t1.x, t2.x), min(t1.y, t2.y)), min(t1.z, t2.z)), tf = min(min(max(t1.x, t2.x), max(t1.y, t2.y)), max(t1.z, t2.z));\n"
    "        if (tn <= tf && tf > 0.0 && tn < 1.0) discard;\n"
    "    }\n"
    "    float ze = 1.0 / gl_FragCoord.w, zs = 1e30;\n"
    "    if (pc.k.z > 0.5) {\n"
    "        zs = scene_z(s);\n"
    "        float lim = pc.z.z + ze * 0.002;\n"
    "        bool hid = ze > zs + lim && !(zs < pd.s2.w && zs < ze * 0.6);\n"
    "        if (pd.s2.x > 0.0 && pc.z.w < 0.5 && hid && zs < pd.s2.z) {\n"  // (s105 leafmode 1: 4x4 neighbourhood)
    "            vec2 base = pc.duv.xy + s * pc.duv.zw, ts = pd.s2.x / vec2(textureSize(sdepth, 0));\n"
    "            vec2 lo = min(pc.duv.xy, pc.duv.xy + pc.duv.zw), hi = max(pc.duv.xy, pc.duv.xy + pc.duv.zw);\n"
    "            int cnt = 0;\n"
    "            for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) {\n"
    "                vec2 q = clamp(base + (vec2(float(i), float(j)) - 1.5) * ts, lo, hi);\n"
    "                if (ze > lin_z(texture(sdepth, q).r) + lim) cnt++;\n"
    "            }\n"
    "            hid = float(cnt) >= pd.s1.x * 16.0 - 0.001;\n"  // s1.x = thr fraction on the bodies' pass
    "        }\n"
    "        if (pc.k.w < 0.5 && hid != (pd.s1.y > 0.0)) discard;\n"
    "    }\n"
    "    if (pc.k.w > 0.5) { o = vec4(vec3(clamp(zs / ze * 0.5, 0.0, 1.0)), 1.0); return; }\n"
    "    vec3 rgb = c.rgb;\n"  // (s54 tint: toward the glow colour by its luminance, plus a little added light)
    // (s121 light) the bodies only (not the disc / decal / depth view); style 0 or no normal = the old look
    // (s126 lflat, v_sk.w) a cancelled normal (0: two-sided cards) = this triangle's camera-facing side, flat
    "    bool nok = dot(v_n, v_n) > 1e-8, nflat = !nok && v_sk.w > 0.5 && dot(ngt, ngt) > 1e-30;\n"
    "    if (pc.z.w < 0.5 && v_l0.w > 0.5 && (nok || nflat)) {\n"
    "        vec3 N = nok ? normalize(v_n) : normalize(ngt);\n"
    "        if (dot(N, ngt) < 0.0) N = -N;\n"
    "        float sv = 1.0, se = 0.0;\n"  // (s124 b) mode 1 / 3 = one lookup at the chest, 2 / 4 = per pixel; 3 / 4 = debug colours
    "        int sm = int(v_sp.x + 0.5);\n"
    "        if (sm > 0) sv = mix(1.0, sun_vis((sm == 2 || sm == 4) ? v_sc : v_scc, se), clamp(v_sp.y, 0.0, 1.0));\n"
    "        rgb *= light_of(N, sv);\n"
    "        if (sm >= 3) rgb = mix(rgb, se < 0.01 ? vec3(0.1, 0.2, 1.0) : vec3(1.0 - sv, sv, 0.0), 0.75);\n"  // red shade, green sun, blue outside
    "    }\n"
    "    if (pd.t.w > 0.0) rgb = mix(rgb, pd.t.rgb * (0.35 + 0.65 * dot(rgb, vec3(0.3, 0.59, 0.11))), pd.t.w * 0.85)\n"
    "                            + pd.t.rgb * pd.t.w * 0.45;\n"
    "    o = vec4(rgb, pd.s1.y > 0.0 ? pd.s1.y : pc.k.y > 0.0 ? c.a : 1.0);\n"
    "}\n";

static struct {  // (s54: shared by every actor; the textures + vb copy are per actor, DrawActor.gpu)
    bool init, failed;
    VkDescriptorSetLayout set_layout;   // (mp2-perf C4) set 0: binding 0 = the group's texture (persistent per (actor, texture) set)
    VkDescriptorSetLayout set_layout1;  // set 1: binding 0 = BotW's scene depth, 1 = the sun cascade (one set per draw pass)
    VkDescriptorSetLayout set_layout2;  // (s183 amd) set 2: binding 0 = the per-draw block D (dynamic uniform buffer, one set per draw pass)
    VkDescriptorPool pool;              // our own pool for the persistent texture sets (never reset: the per-submission pools are)
    VkPipelineLayout layout;
    // per colour format (s57: the window image's and the game's final image's): opaque (alpha test, depth write),
    // blended (depth test only), depth only (ghost prepass)
    struct { VkFormat fmt; VkPipeline p[3]; } pipes[2];
    VkSampler sampler, nearest;
    VkImage depth;
    VmaAllocation depth_mem;
    VkImageView depth_view;
    VkExtent2D depth_ext;
    uint64_t drawn;
} G;

static VkPipeline make_pipeline(VkFormat fmt, bool blend, bool depth_only) {
    static VkShaderModule vs, fs;
    if (!vs) vs = vk_compile_glsl(VERT_SRC, STAGE_VERTEX, "sora_draw.vert");
    if (!fs) fs = vk_compile_glsl(FRAG_SRC, STAGE_FRAGMENT, "sora_draw.frag");
    if (!vs || !fs) {
        dg_latch("D3 Sora's shader did not compile (%s shader, GPU driver refused it)", !vs ? "vertex" : "fragment");
        return VK_NULL_HANDLE;
    }
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main"}};
    // (s121 light) binding 0 = the vertices (pos, uv, normal), binding 1 = the light block per instance (4 x vec4)
    VkVertexInputBindingDescription bind[2] = {{0, VB_F * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX},
                                               {1, LREC_F * sizeof(float), VK_VERTEX_INPUT_RATE_INSTANCE}};
    VkVertexInputAttributeDescription attr[15] = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, {1, 0, VK_FORMAT_R32G32_SFLOAT, 12},
                                                 {2, 0, VK_FORMAT_R32G32B32_SFLOAT, 20},
                                                 {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0}, {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 16},
                                                 {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 32}, {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 48},
                                                 // (s124 b) the sun shadow: s0 s1 s2 + the cascade 0 matrix rows m0..m3
                                                 {7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 64}, {8, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 80},
                                                 {9, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 96}, {10, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 112},
                                                 {11, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 128}, {12, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 144},
                                                 {13, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 160}, {14, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 176}};
    VkPipelineVertexInputStateCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                               .vertexBindingDescriptionCount = 2, .pVertexBindingDescriptions = bind,
                                               .vertexAttributeDescriptionCount = 15, .pVertexAttributeDescriptions = attr};
    VkPipelineInputAssemblyStateCreateInfo ia = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                 .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo vps = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                 .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .lineWidth = 1};
    VkPipelineMultisampleStateCreateInfo ms = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                               .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo ds = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                .depthTestEnable = VK_TRUE, .depthWriteEnable = blend ? VK_FALSE : VK_TRUE,
                                                .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL};
    VkPipelineColorBlendAttachmentState cba = {.colorWriteMask = depth_only ? 0 : 0xF};
    if (blend)
        cba = (VkPipelineColorBlendAttachmentState){.blendEnable = VK_TRUE, .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                                                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                    .colorBlendOp = VK_BLEND_OP_ADD, .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                                                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .alphaBlendOp = VK_BLEND_OP_ADD,
                                                    .colorWriteMask = 0xF};
    VkPipelineColorBlendStateCreateInfo cb = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1,
                                              .pAttachments = &cba};
    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dyn};
    VkFormat df = DEPTH_FORMAT;
    VkPipelineRenderingCreateInfo ri = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1,
                                        .pColorAttachmentFormats = &fmt, .depthAttachmentFormat = df};
    VkGraphicsPipelineCreateInfo pi = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri, .stageCount = 2,
                                       .pStages = stages, .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vps,
                                       .pRasterizationState = &rs, .pMultisampleState = &ms, .pDepthStencilState = &ds,
                                       .pColorBlendState = &cb, .pDynamicState = &dy, .layout = G.layout};
    VkPipeline p = VK_NULL_HANDLE;
    VkResult pr = vkCreateGraphicsPipelines(g_vk.dev, g_vk.pcache, 1, &pi, NULL, &p);
    if (pr != VK_SUCCESS) {
        dg_latch("D4 Vulkan pipeline failed: %s (%d), colour format %d", dg_vkr((int)pr), (int)pr, (int)fmt);
        return VK_NULL_HANDLE;
    }
    return p;
}

// (s57) The three pipelines for colour format fmt, made the first time it is asked for; two formats are kept (the
// window image's, the game image's), a third replaces the second (its pipelines are leaked: may still be in flight).
// NULL when they cannot be made (G.failed set).
static const VkPipeline *pipes_for(VkFormat fmt) {
    for (int i = 0; i < 2; i++)
        if (G.pipes[i].p[0] && G.pipes[i].fmt == fmt) return G.pipes[i].p;
    int i = G.pipes[0].p[0] ? 1 : 0;
    if (G.pipes[i].p[0]) LOG("[sora-draw] a third colour format %d: pipelines of format %d dropped\n", fmt, G.pipes[i].fmt);
    G.pipes[i].fmt = fmt;
    G.pipes[i].p[0] = make_pipeline(fmt, false, false), G.pipes[i].p[1] = make_pipeline(fmt, true, false);
    G.pipes[i].p[2] = make_pipeline(fmt, false, true);
    if (!G.pipes[i].p[0] || !G.pipes[i].p[1] || !G.pipes[i].p[2]) {
        G.failed = true, G.pipes[i].p[0] = VK_NULL_HANDLE;
        LOG("[sora-draw] pipelines failed (format %d)\n", fmt);
        dg_latch("D5 Vulkan pipelines could not be made for colour format %d", (int)fmt);
        return NULL;
    }
    LOG("[sora-draw] pipelines for colour format %d\n", fmt);
    return G.pipes[i].p;
}

// (s183 amd) maxPushConstantsSize as Sora's passes see it: the GPU's, or less with SAVAGEWIND_SORA_PUSH_LIMIT=<n> (=128 on
// NVIDIA = an AMD-Windows / Intel-like GPU, to prove the body still draws there). Every Sora pipeline layout with push
// constants (the body pass here, the cast shadow below, sora_fx.c's effects) checks its range against this.
unsigned sora_push_limit(void) {
    static long long env = -2;
    const uint32_t real = g_vk.props.limits.maxPushConstantsSize;
    if (env == -2) {
        const char *e = getenv("SAVAGEWIND_SORA_PUSH_LIMIT");
        env = e && *e ? strtoll(e, NULL, 0) : -1;
        if (env >= 0) LOG("[sora-draw] SAVAGEWIND_SORA_PUSH_LIMIT=%lld: Sora's passes treat this GPU's %u bytes of push constants as %lld\n",
                          env, real, env < (long long)real ? env : (long long)real);
    }
    return env >= 0 && env < (long long)real ? (uint32_t)env : real;
}

static bool gpu_init(void) {  // (s54: the shared part; textures per actor in actor_gpu_init)
    const uint32_t lim = sora_push_limit();
    LOG("[sora-draw] push constants: %u bytes allowed (GPU %u), body pass needs %d + a %d-byte uniform block (the s182 layout "
        "needed %d: %s)\n", lim, g_vk.props.limits.maxPushConstantsSize, PC_BYTES, PD_BYTES, PC_BYTES_S182,
        lim < PC_BYTES_S182 ? "it would have FAILED here, D6" : "it fit");
    if (lim < PC_BYTES) {
        LOG("[sora-draw] push constants: %u bytes, need %d\n", lim, PC_BYTES);
        dg_latch("D6 this GPU allows %u bytes of push constants, Sora needs %d", lim, PC_BYTES);
        return false;
    }
    VkDescriptorSetLayoutBinding b[2] = {{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
                                         {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL}};  // (s124 b sun cascade)
    VkDescriptorSetLayoutCreateInfo li = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = b};
    VK_CHECK(vkCreateDescriptorSetLayout(g_vk.dev, &li, NULL, &G.set_layout));
    li.bindingCount = 2;
    VK_CHECK(vkCreateDescriptorSetLayout(g_vk.dev, &li, NULL, &G.set_layout1));
    VkDescriptorSetLayoutBinding bd = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, NULL};
    VkDescriptorSetLayoutCreateInfo ld = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &bd};
    VK_CHECK(vkCreateDescriptorSetLayout(g_vk.dev, &ld, NULL, &G.set_layout2));
    VkDescriptorPoolSize psz = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SORA_DRAW_TEX_SETS};
    VkDescriptorPoolCreateInfo dpi = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = SORA_DRAW_TEX_SETS,
                                      .poolSizeCount = 1, .pPoolSizes = &psz};
    if (vkCreateDescriptorPool(g_vk.dev, &dpi, NULL, &G.pool) != VK_SUCCESS) G.pool = VK_NULL_HANDLE;  // (no persistent sets: per-draw ones)
    VkDescriptorSetLayout sls[3] = {G.set_layout, G.set_layout1, G.set_layout2};
    VkPushConstantRange r = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, PC_BYTES};
    VkPipelineLayoutCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 3,
                                     .pSetLayouts = sls, .pushConstantRangeCount = 1, .pPushConstantRanges = &r};
    VK_CHECK(vkCreatePipelineLayout(g_vk.dev, &ci, NULL, &G.layout));
    VkSamplerCreateInfo si = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR,
                              .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST, .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                              .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT, .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                              .maxLod = 0};
    VK_CHECK(vkCreateSampler(g_vk.dev, &si, NULL, &G.sampler));
    VkSamplerCreateInfo sn = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = VK_FILTER_NEAREST, .minFilter = VK_FILTER_NEAREST,
                              .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST, .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
    VK_CHECK(vkCreateSampler(g_vk.dev, &sn, NULL, &G.nearest));
    return true;
}

// An actor's textures: BGRA8 as baked (sorabake ToBgra32), uploaded once by the submission's upload command buffer (the
// actor draws from the next present on). (s54) Not VK_CHECK: an allocation failure here (GPU memory) drops this actor
// with a log line instead of FATAL; what was made is handed to the deferred destroy.
static bool actor_gpu_init(DrawActor *a) {
    VkCommandBuffer ucb = vk_upload_cmd();
    uint32_t i = 0;
    for (; i < a->ntex; i++) {
        const MTex *t = &a->tex[i];
        VkImageCreateInfo ic = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_B8G8R8A8_UNORM,
                                .extent = {t->w, t->h, 1}, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
                                .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};
        VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
        VkResult vr = vmaCreateImage(g_vk.vma, &ic, &ai, &a->gpu.tex[i], &a->gpu.tex_mem[i], NULL);
        if (vr != VK_SUCCESS) {
            LOG("[sora-draw] actor %d texture %u (%ux%u): vmaCreateImage %d\n", (int)(a - g_act), i, t->w, t->h, vr);
            if (a == &g_act[0]) dg_latch("D7 Sora's texture %u (%ux%u) could not be created: %s (%d)", i, t->w, t->h, dg_vkr((int)vr), (int)vr);
            break;
        }
        vk_image_init_layout(ucb, a->gpu.tex[i], VK_IMAGE_ASPECT_COLOR_BIT);
        VkSpan s = vk_transient_copy(a->model + t->off, (uint64_t)t->w * t->h * 4, 16);
        VkBufferImageCopy rg = {.bufferOffset = s.off, .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {t->w, t->h, 1}};
        rec_CmdCopyBufferToImage(ucb, s.buf, a->gpu.tex[i], VK_IMAGE_LAYOUT_GENERAL, 1, &rg);
        VkImageViewCreateInfo vc = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = a->gpu.tex[i], .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = VK_FORMAT_B8G8R8A8_UNORM, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        vr = vkCreateImageView(g_vk.dev, &vc, NULL, &a->gpu.tex_view[i]);
        if (vr != VK_SUCCESS) {
            LOG("[sora-draw] actor %d texture %u: vkCreateImageView %d\n", (int)(a - g_act), i, vr);
            if (a == &g_act[0]) dg_latch("D7 Sora's texture %u view failed: %s (%d)", i, dg_vkr((int)vr), (int)vr);
            vk_defer_image(a->gpu.tex[i], a->gpu.tex_mem[i]), a->gpu.tex[i] = VK_NULL_HANDLE;
            break;
        }
    }
    if (i < a->ntex || !a->ntex) {  // (no texture at all: nothing to bind for its groups either)
        vk_full_barrier(ucb);
        while (i-- > 0) vk_defer_view(a->gpu.tex_view[i]), vk_defer_image(a->gpu.tex[i], a->gpu.tex_mem[i]);
        memset(a->gpu.tex, 0, sizeof a->gpu.tex), memset(a->gpu.tex_view, 0, sizeof a->gpu.tex_view);
        return false;
    }
    if (a->nidx) {  // (mp2-perf C2) the index buffer never changes: one device-local copy for the actor's life (no unload path exists
                    // for actors, like their textures; made whatever the bit says so it can be switched on live; failure = per-frame copy)
        const uint64_t ibn = (uint64_t)a->nidx * 4;
        VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = ibn,
                                 .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
        VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
        VkResult vr = vmaCreateBuffer(g_vk.vma, &bi, &ai, &a->gpu.ib, &a->gpu.ib_mem, NULL);
        if (vr != VK_SUCCESS) {
            LOG("[sora-draw] actor %d index buffer (%llu bytes): vmaCreateBuffer %d (per-frame copy instead)\n", (int)(a - g_act),
                (unsigned long long)ibn, vr);
            a->gpu.ib = VK_NULL_HANDLE;
        } else {
            VkSpan sp = vk_transient_copy(a->idx, ibn, 16);
            VkBufferCopy bc = {.srcOffset = sp.off, .dstOffset = 0, .size = ibn};
            rec_CmdCopyBuffer(ucb, sp.buf, a->gpu.ib, 1, &bc);
        }
    }
    if (G.pool) {  // (mp2-perf C4) one persistent set per texture: binding 0 = its view (the sampler is fixed), updated in order with the commands
        VkDescriptorSetLayout lay[MAX_TEX];
        VkDescriptorSet sets[MAX_TEX];
        for (uint32_t k = 0; k < a->ntex; k++) lay[k] = G.set_layout;
        VkDescriptorSetAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = G.pool,
                                          .descriptorSetCount = a->ntex, .pSetLayouts = lay};
        VkResult vr = vkAllocateDescriptorSets(g_vk.dev, &ai, sets);
        if (vr != VK_SUCCESS) LOG("[sora-draw] actor %d: %u persistent texture sets: vkAllocateDescriptorSets %d (per-draw sets instead)\n", (int)(a - g_act), a->ntex, vr);
        else {
            for (uint32_t k = 0; k < a->ntex; k += 8) {
                VkDescriptorImageInfo ii[8];
                VkWriteDescriptorSet w[8];
                uint32_t n = a->ntex - k < 8 ? a->ntex - k : 8;
                for (uint32_t m = 0; m < n; m++) {
                    ii[m] = (VkDescriptorImageInfo){G.sampler, a->gpu.tex_view[k + m], VK_IMAGE_LAYOUT_GENERAL};
                    w[m] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = sets[k + m], .dstBinding = 0,
                                                  .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                  .pImageInfo = &ii[m]};
                }
                rec_UpdateDescriptorSets(n, w);
            }
            for (uint32_t k = 0; k < a->ntex; k++) a->gpu.tex_set[k] = sets[k];
        }
    }
    vk_full_barrier(ucb);
    return true;
}

// (s57) at least e: the pass renders only its colour target's area of it, so the window image and the game image
// (different sizes with bars or a scaled output) share one depth image instead of remaking it at each switch
static bool depth_for(VkExtent2D e) {
    if (G.depth && G.depth_ext.width >= e.width && G.depth_ext.height >= e.height) return true;
    if (G.depth) {
        if (G.depth_ext.width > e.width) e.width = G.depth_ext.width;
        if (G.depth_ext.height > e.height) e.height = G.depth_ext.height;
        vk_defer_view(G.depth_view), vk_defer_image(G.depth, G.depth_mem), G.depth = VK_NULL_HANDLE;
    }
    VkImageCreateInfo ic = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = DEPTH_FORMAT,
                            .extent = {e.width, e.height, 1}, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
                            .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};
    VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    VkResult dr = vmaCreateImage(g_vk.vma, &ic, &ai, &G.depth, &G.depth_mem, NULL);
    if (dr != VK_SUCCESS) {
        G.depth = VK_NULL_HANDLE;
        dg_latch("D8 Sora's depth image (%ux%u) could not be created: %s (%d)", e.width, e.height, dg_vkr((int)dr), (int)dr);
        return false;
    }
    vk_image_init_layout(vk_upload_cmd(), G.depth, VK_IMAGE_ASPECT_DEPTH_BIT);
    VkImageViewCreateInfo vc = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = G.depth, .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                .format = DEPTH_FORMAT, .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}};
    VK_CHECK(vkCreateImageView(g_vk.dev, &vc, NULL, &G.depth_view));
    G.depth_ext = e;
    return true;
}

// mvp (column-major for GLSL) = Vulkan projection (y down, depth 0..1) * the camera's view
static void build_mvp(const float v[12], const float p[4], float out[16]) {
    float n = p[0], f = p[1], t = 1.0f / tanf(p[2] * 0.5f), a = p[3];
    float P[4][4] = {{t / a, 0, 0, 0}, {0, -t, 0, 0}, {0, 0, f / (n - f), n * f / (n - f)}, {0, 0, -1, 0}};
    float V[4][4] = {{v[0], v[1], v[2], v[3]}, {v[4], v[5], v[6], v[7]}, {v[8], v[9], v[10], v[11]}, {0, 0, 0, 1}};
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += P[r][k] * V[k][c];
            out[c * 4 + r] = s;
        }
}

// The camera of the frame sora_draw_present drew last (render thread only: sora_hud_present runs right after it).
static float g_last_mvp[16], g_last_vp[4];
static bool g_last_ok;

// A world point (BotW units) -> window px of this frame's camera; false behind the camera or with no Sora frame.
bool sora_draw_project(const float w[3], float *sx, float *sy) {
    if (!g_last_ok) return false;
    const float *m = g_last_mvp;
    float c[4];
    for (int r = 0; r < 4; r++) c[r] = m[r] * w[0] + m[4 + r] * w[1] + m[8 + r] * w[2] + m[12 + r];
    if (c[3] < 0.05f) return false;
    *sx = g_last_vp[0] + (c[0] / c[3] * 0.5f + 0.5f) * g_last_vp[2];
    *sy = g_last_vp[1] + (c[1] / c[3] * 0.5f + 0.5f) * g_last_vp[3];
    return true;
}


// ------------------------------------------------------------------ s57: where the pass goes (S57_HELPER_UIORDER.md)
// SAVAGEWIND_SORA_UI_ORDER=1 (default): the actors, their shadows and the KH2 effects go into the game's final image
// right after BotW's scene composite (exec.c ui_boundary_check calls sora_draw_scene_pass there), so BotW's 2D UI
// (HUD, item-get popups, menus, fades) is drawn over them as over Link; our KH2 HUD (sora_hud_present) stays on top.
// A frame whose composite was not seen: with the 3D world drawn that frame (scene depth) the old order as a fallback
// (counted in the log), without it (menu over a still, a load, a re-present) nothing new (the image keeps what the
// composite pass drew). =0: the old order (over the window image at present, over BotW's HUD).
static int g_ui_order = -1;
static struct {
    uint64_t frame;     // vk_frame_counter() of the last scene pass (~0: none)
    bool ok;            // it drew (g_last_ok of that pass)
    float vp[4];        // its viewport in the game image (px) ...
    uint32_t w, h;      // ... of this size: mapped to the window at present for sora_draw_project
    uint64_t passes, fallbacks, skips;
} g_ub = {.frame = ~0ull};
uint64_t vk_frame_counter(void);
int sora_draw_ui_order(void) {
    if (g_ui_order < 0) {
        const char *e = getenv("SAVAGEWIND_SORA_UI_ORDER");
        g_ui_order = e && *e ? atoi(e) != 0 : 1;
        LOG("[sora-draw] ui order %d: %s\n", g_ui_order,
            g_ui_order ? "actors at BotW's scene composite (under its 2D UI)" : "actors over the window image (over BotW's HUD)");
    }
    return g_ui_order;
}
void sora_draw_set_ui_order(int on) {  // (live A/B; sora.c may look it up by name)
    sora_draw_ui_order();
    if (g_ui_order != (on != 0)) LOG("[sora-draw] ui order -> %d\n", on != 0);
    g_ui_order = on != 0;
}

// (s114 drawsync, g_lock held) the ring entry to draw: mode 1 = the newest with tag <= want (none: the oldest there),
// mode 2 = one past the last drawn (the same one when nothing new; the newest after a gap of more than 2)
static int ring_pick(const DrawActor *a, int newest, uint64_t want, uint64_t last) {
    int best = -1, oldest = -1;
    const uint64_t nt = a->ring[newest].tag;
    for (int r = 0; r < DRAW_RING; r++) {
        uint64_t t = a->ring[r].tag;
        if (!t || !a->ring[r].vb) continue;
        if (oldest < 0 || t < a->ring[oldest].tag) oldest = r;
        if (g_sync == 1 ? (t <= want && (best < 0 || t > a->ring[best].tag))
                        : (t > last && (best < 0 || t < a->ring[best].tag))) best = r;
    }
    if (g_sync == 1) return best >= 0 ? best : oldest;
    if (!last || nt < last || nt - last > 2) return newest;
    if (nt == last) return newest;  // (nothing new: the same pose again)
    return best >= 0 ? best : newest;
}
// (s114) one "[drawlat]" line per 600 scene passes (body actor): how far the newest snapshot moved between two passes
// (what the old mode drew: 0 and 2 = judder), how far the drawn one moved, newest - list tag, and the drawn one's age
static struct { unsigned n, nw[4], st[4], d[6], miss; double age, amax; uint64_t prev_newest; } g_ld;
static void lat_diag(const DrawActor *a, int newest, int pick, uint64_t xtag, uint64_t want, uint64_t last, uint64_t drawn) {
    if (!g_latdiag) return;
    uint64_t nt = a->ring[newest].tag;
    if (g_ld.prev_newest && nt >= g_ld.prev_newest) g_ld.nw[nt - g_ld.prev_newest > 3 ? 3 : nt - g_ld.prev_newest]++;
    g_ld.prev_newest = nt;
    if (last && drawn >= last) g_ld.st[drawn - last > 3 ? 3 : drawn - last]++;
    int64_t d = (int64_t)(nt - xtag);
    g_ld.d[d < -1 ? 0 : d > 3 ? 5 : d + 1 + (d >= -1)]++;
    if (g_sync == 1 && pick >= 0 && a->ring[pick].tag != want) g_ld.miss++;
    if (pick >= 0) {
        double age = ((double)plat_now_ns() - (double)a->ring[pick].t_ns) / 1e6;
        g_ld.age += age;
        if (age > g_ld.amax) g_ld.amax = age;
    }
    if (++g_ld.n < 600) return;
    LOG("[drawlat] %u passes, drawsync %d off %d | newest step 0:%u 1:%u 2:%u 3+:%u | drawn step 0:%u 1:%u 2:%u 3+:%u | "
        "newest-list <-1:%u -1:%u 0:%u 1:%u 2:%u 3+:%u | no exact tag %u | drawn age %.1f ms avg %.1f max\n",
        g_ld.n, g_sync, g_sync_off, g_ld.nw[0], g_ld.nw[1], g_ld.nw[2], g_ld.nw[3], g_ld.st[0], g_ld.st[1], g_ld.st[2], g_ld.st[3],
        g_ld.d[0], g_ld.d[1], g_ld.d[2], g_ld.d[3], g_ld.d[4], g_ld.d[5], g_ld.miss, g_ld.age / g_ld.n, g_ld.amax);
    uint64_t keep = g_ld.prev_newest;
    memset(&g_ld, 0, sizeof g_ld);
    g_ld.prev_newest = keep;
}

// (mp2-perf C4) The descriptor sets of one draw: set 0 = texture <tex> of <a> (C4: the persistent set, else a per-draw one), set 1 = BotW's
// scene depth + the sun cascade (<fset>, one per draw pass; NULL = C4 off: a per-draw set, dv / sv with <fb> where a view is missing).
static void bind_draw_sets(VkCommandBuffer cb, const DrawActor *a, uint32_t tex, VkDescriptorSet fset, VkImageView dv, VkImageView sv) {
    VkDescriptorSet s0 = fset ? a->gpu.tex_set[tex] : VK_NULL_HANDLE;
    if (!s0) {
        s0 = vk_alloc_set(G.set_layout);
        VkDescriptorImageInfo ii = {G.sampler, a->gpu.tex_view[tex], VK_IMAGE_LAYOUT_GENERAL};
        VkWriteDescriptorSet w = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = s0, .dstBinding = 0, .descriptorCount = 1,
                                  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &ii};
        mod_update_set(&w);
    }
    rec_CmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, G.layout, 0, 1, &s0, 0, NULL);
    if (!fset) {  // (the old cost: a set of its own per draw)
        VkDescriptorSet s1 = vk_alloc_set(G.set_layout1);
        const VkImageView fb = a->gpu.tex_view[0];
        VkDescriptorImageInfo ii[2] = {{G.nearest, dv ? dv : fb, VK_IMAGE_LAYOUT_GENERAL}, {G.nearest, sv ? sv : fb, VK_IMAGE_LAYOUT_GENERAL}};
        VkWriteDescriptorSet w = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = s1, .dstBinding = 0, .descriptorCount = 2,
                                  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = ii};
        mod_update_set(&w);
        rec_CmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, G.layout, 1, 1, &s1, 0, NULL);
    }
}

// (s183 amd) The per-draw block D (BodyPD) of one draw pass: one transient span of cap slots (each slot rounded up to
// minUniformBufferOffsetAlignment) behind one set-2 descriptor (dynamic uniform buffer), so a push is the 128-byte push
// constants + (when D changed since the last push) the next slot written and set 2 bound at its offset. One span = one
// VkBuffer for the whole pass (a second span only if cap ran out: a new set, never a slot an earlier draw still reads).
typedef struct { VkSpan span; VkDescriptorSet set; uint32_t stride, cap, n; BodyPD last; uint32_t slots, spans; } PdRing;
static void pd_begin(PdRing *r, uint32_t cap) {
    uint64_t al = g_vk.props.limits.minUniformBufferOffsetAlignment;
    if (al < 16) al = 16;
    r->stride = (uint32_t)((PD_BYTES + al - 1) / al * al);
    r->cap = cap ? cap : 1, r->n = 0;
    r->span = vk_transient((uint64_t)r->stride * r->cap, al);
    r->set = vk_alloc_set(G.set_layout2);
    VkDescriptorBufferInfo bi = {r->span.buf, r->span.off, PD_BYTES};
    VkWriteDescriptorSet w = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = r->set, .dstBinding = 0, .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .pBufferInfo = &bi};
    mod_update_set(&w);
    r->spans++;
}
// (s217 depth) a body draw's origin: org = its actor's feet (world), c3 = mvp * (org, 1) in double (exact products)
static void pd_set_org(BodyPD *pd, const float m[16], const float o[3]) {
    for (int k = 0; k < 3; k++) pd->org[k] = o[k];
    pd->org[3] = 0;
    for (int r = 0; r < 4; r++)
        pd->c3[r] = (float)((double)m[r] * o[0] + (double)m[4 + r] * o[1] + (double)m[8 + r] * o[2] + (double)m[12 + r]);
}
static void pd_push(VkCommandBuffer cb, PdRing *r, const BodyPC *pc, const BodyPD *pd) {
    rec_CmdPushConstants(cb, G.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof *pc, pc);
    if (r->slots && !memcmp(&r->last, pd, sizeof *pd)) return;  // (set 2 still bound at that block: binds of sets 0 / 1 and pipelines keep it)
    if (r->n >= r->cap) {
        static bool said;
        if (!said) said = true, LOG("[sora-draw] per-draw blocks: %u slots ran out, a second span (not expected)\n", r->cap);
        pd_begin(r, r->cap);
    }
    const uint32_t off = r->n++ * r->stride;
    memcpy((uint8_t *)r->span.ptr + off, pd, sizeof *pd);
    r->last = *pd, r->slots++;
    rec_CmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, G.layout, 2, 1, &r->set, 1, &off);
}

// Inside a rendering of its own into cv (format fmt, layout lay, size ext), the actors in (vx, vy, vw, vh) of it.
// window: cv = the window image, inside its rendering (suspended around ours), after the game image went into
// (vx, vy, vw, vh); else (s57) cv = the game's final image, with no rendering open (exec.c ended the game's pass).
// (s54) Every posed actor: one rendering, our depth cleared once; all shadows first, then pass-major (opaque of all,
// alpha phase of all, the ghost passes of those with one), so depth sorts the bodies against each other and the
// blended ones go last. True when something was drawn.
static bool draw_pass(VkImageView cv, VkFormat fmt, VkImageLayout lay, VkExtent2D ext, float vx, float vy, float vw, float vh,
                      bool window) {
    g_last_ok = false;
    if (g_state != 1 || G.failed) return false;
    typedef struct { bool valid; float feet[3]; Shadow sh; float ghost, tint[4], light[16]; bool hide_weapon, weapon_at; uint8_t blade[2]; } ASnap;
    ASnap s[SORA_DRAW_ACTORS];
    memset(s, 0, sizeof s);
    int nact;
    pthread_mutex_lock(&g_lock);
    nact = g_nact;
    pthread_mutex_unlock(&g_lock);
    for (int i = 0; i < nact; i++)  // (the copy target, made here: nverts is fixed once the actor counts)
        if (!g_act[i].gpu.vb && !g_act[i].gpu.failed && !(g_act[i].gpu.vb = calloc(g_act[i].nverts, VB_F * sizeof(float))))
            g_act[i].gpu.failed = true, LOG("[sora-draw] actor %d: out of memory\n", i);
    CamSnap cam;
    int nvalid = 0;
    // (s114 drawsync) the scene pass (not the window fallback) takes the ring snapshot of its list's game frame
    static uint64_t last_pick[SORA_DRAW_ACTORS];  // (render thread) the tag drawn last per actor
    const bool sync = g_sync > 0 && !window;
    const uint64_t xtag = nvn_exec_frame_tag(), want = xtag + (uint64_t)(int64_t)g_sync_off;
    bool cam_ring = false;
    VkSpan vb[SORA_DRAW_ACTORS], ib[SORA_DRAW_ACTORS];
    bool vb_done[SORA_DRAW_ACTORS] = {false};  // (mp2-perf C3) vb[i] already holds the picked vertices (copied under g_lock)
    const bool c3 = (g_perf & PERF_C3) != 0;
    bool ver_r = g_perf_verify_r != 0;         // (mp2-perf verify C3, render side: the first actor drawn after the game side armed it)
    g_perf_verify_r = 0;
    double p_snap[2] = {0, 0}, p_up[2] = {0, 0}, p_rec[2] = {0, 0};  // (mp2-perf, ms per group: 0 = the body, 1 = the others summed)
    bool p_has[2] = {false, false};
    pthread_mutex_lock(&g_lock);
    memcpy(&cam, &g_cam, sizeof cam);
    for (int i = 0; i < nact; i++) {
        DrawActor *a = &g_act[i];
        s[i].valid = a->snap.valid && !a->gpu.failed;
        if (!s[i].valid) continue;
        const uint64_t pf_ts = plat_now_ns();
        int newest = -1, pick = -1;
        const float *vsrc;
        for (int r = 0; r < DRAW_RING; r++)
            if (a->ring[r].tag && a->ring[r].vb && (newest < 0 || a->ring[r].tag > a->ring[newest].tag)) newest = r;
        if (sync && newest >= 0) pick = ring_pick(a, newest, want, last_pick[i]);
        if (pick >= 0) {
            const typeof(a->ring[0]) *R = &a->ring[pick];
            memcpy(s[i].feet, R->feet, sizeof s[i].feet);
            s[i].sh = R->sh, s[i].ghost = R->ghost, s[i].hide_weapon = R->hide_weapon, s[i].weapon_at = R->weapon_at;
            s[i].blade[0] = R->blade[0], s[i].blade[1] = R->blade[1];
            memcpy(s[i].tint, R->tint, sizeof s[i].tint);
            memcpy(s[i].light, R->light, sizeof s[i].light);
            vsrc = R->vb;
            if (i == g_body || !cam_ring) cam = R->cam, cam_ring = true;
        } else {
            memcpy(s[i].feet, a->snap.feet, sizeof s[i].feet);
            s[i].sh = a->snap.sh, s[i].ghost = a->snap.ghost, s[i].hide_weapon = a->snap.hide_weapon;
            s[i].weapon_at = a->snap.weapon_at;
            s[i].blade[0] = a->snap.blade[0], s[i].blade[1] = a->snap.blade[1];
            memcpy(s[i].tint, a->snap.tint, sizeof s[i].tint);
            memcpy(s[i].light, a->snap.light, sizeof s[i].light);
            vsrc = snap_vb(a);
        }
        const size_t vbn = (size_t)a->nverts * VB_F * sizeof(float);
        if (c3) {  // (mp2-perf C3) the picked slot / snapshot straight into the transient span: no a->gpu.vb step, no second copy later
            vb[i] = vk_transient(vbn, 16);
            memcpy(vb[i].ptr, vsrc, vbn);
            vb_done[i] = true;
            if (ver_r) {
                ver_r = false;
                memcpy(a->gpu.vb, vsrc, vbn);  // the old path's copy
                LOG("[sora-draw] drawperf verify C3 (render side, actor %d): transient span vs the old a->gpu.vb copy: memcmp %d\n", i,
                    memcmp(vb[i].ptr, a->gpu.vb, vbn));
            }
        } else memcpy(a->gpu.vb, vsrc, vbn);
        p_snap[i == g_body ? 0 : 1] += pf_ms(pf_ts), p_has[i == g_body ? 0 : 1] = true;
        uint64_t drawn = pick >= 0 ? a->ring[pick].tag : newest >= 0 ? a->ring[newest].tag : 0;
        if (i == g_body && !window && newest >= 0) lat_diag(a, newest, pick, xtag, want, last_pick[i], drawn);
        last_pick[i] = drawn;
        nvalid++;
    }
    pthread_mutex_unlock(&g_lock);
    for (int k = 0; k < 2; k++)
        if (p_has[k]) pf_add(&g_pf.snap[k], p_snap[k]);
    if (!nvalid) return false;
    if (!G.init) {
        G.init = true;
        if (!gpu_init()) { G.failed = true; LOG("[sora-draw] GPU setup failed\n"); dg_latch("D6 Sora's GPU setup failed"); return false; }
    }
    // an actor's first posed present: its textures go up with this submission, it draws from the next one on (one
    // actor per present: Sora first, the party a present apart)
    for (int i = 0, up = 0; i < nact; i++) {
        DrawActor *a = &g_act[i];
        if (!s[i].valid || a->gpu.init) continue;
        s[i].valid = false, nvalid--;
        if (up++) continue;
        a->gpu.init = true;
        if (!actor_gpu_init(a)) {
            a->gpu.failed = true, LOG("[sora-draw] actor %d: GPU setup failed (not drawn)\n", i);
            if (i == 0) dg_latch("D7 Sora's textures could not be uploaded (GPU memory?)");
        }
        else if (i) LOG("[sora-draw] actor %d: %u textures uploaded\n", i, a->ntex);
    }
    if (!nvalid) { g_dg_skip = "D9 the game has not handed the body a pose to draw"; return false; }
    if (!cv) { g_dg_skip = "DA no image to draw into"; return false; }
    if (!depth_for(ext)) return false;
    const VkPipeline *P = pipes_for(fmt);
    if (!P) return false;
    float proj[4] = {0.1f, 25000, 50 * 3.14159265f / 180, vh > 0 ? vw / vh : 16.0f / 9};
    const bool scr = cam.screen;  // (s59 KH2 Game Over: own camera, no scene depth / shadow / fx, the black rect first)
    if (cam.have_proj) memcpy(proj, cam.proj, sizeof proj);
    if (scr && !(proj[3] > 0)) proj[3] = vh > 0 ? vw / vh : 16.0f / 9;
    else {
        const char *e = getenv("SAVAGEWIND_SORA_FOVY");
        if (e && *e) proj[2] = strtof(e, NULL) * 3.14159265f / 180;
    }
    BodyPC pc;  // (s183 amd: the push constants, 128 bytes)
    BodyPD pd;  // (s183 amd: the per-draw uniform block, pd_push)
    memset(&pd, 0, sizeof pd);  // (s0 s1 s2, s107 slateocc oa ob oc: 0)
    memcpy(pd.t, s[0].tint, sizeof pd.t);  // (set again per actor before each of its draws)
    build_mvp(cam.view, proj, pc.mvp);
    if (!scr && (cam.shift[0] != 0 || cam.shift[1] != 0))  // (s107 invsora 3: an off-centre camera, clip += shift * w; y flipped here)
        for (int c = 0; c < 4; c++) pc.mvp[c * 4] += cam.shift[0] * pc.mvp[c * 4 + 3], pc.mvp[c * 4 + 1] -= cam.shift[1] * pc.mvp[c * 4 + 3];
    memcpy(g_last_mvp, pc.mvp, sizeof g_last_mvp);
    g_last_vp[0] = vx, g_last_vp[1] = vy, g_last_vp[2] = vw, g_last_vp[3] = vh, g_last_ok = true;
    // the world in front of Sora: BotW's scene depth (exec.c vk_scene_depth), linearised with the game's near / far
    static int occl = -1, dbg_env;
    if (occl < 0) {
        const char *e = getenv("SAVAGEWIND_SORA_OCCLUDE");
        occl = e && *e ? atoi(e) : 1;
        dbg_env = getenv("SAVAGEWIND_SORA_DEPTH_DEBUG") != NULL;
    }
    const int dbg = dbg_env || g_depth_dbg;  // (s105: also the live key depthdbg)
    VkImageView dview = VK_NULL_HANDLE;
    uint32_t dw = 0, dh = 0, ddraws = 0;
    float mode = 0;
    if (occl && !scr && vk_scene_depth(&dview, pc.duv, &dw, &dh, &ddraws)) mode = nvn_depth_mode() == 0 ? 1 : 2;
    if (mode > 0 && !cam.have_proj) {  // (s183 amd FIX 2) no known near / far: linearising BotW's depth with a guess hid the
        static uint64_t n_off;         // whole body (4:3, 5:4, 32:9 windows) -> no occlusion (and no decal shadow / ghost) this frame
        if (n_off++ % 600 == 0) LOG("[sora-draw] no world projection known: scene-depth occlusion off (%llu frames)\n", (unsigned long long)n_off);
        mode = 0;
    }
    VkImageView sdw_view = VK_NULL_HANDLE;  // (s124 b) BotW's sun cascade 0 + its world -> clip matrix
    float sdw_m[16];
    uint32_t sdw_w = 0, sdw_h = 0;
    bool sdw_on = !scr && !dbg && g_sdw[0] > 0.5f && vk_sun_cascade(&sdw_view, sdw_m, &sdw_w, &sdw_h);
    if (!sdw_on) sdw_view = VK_NULL_HANDLE;
    else if ((int)(g_cast[0] + 0.5f) != 3 && GC.rview && GC.rframe == vk_frame_counter() + 1 && GC.rw == sdw_w && GC.rh == sdw_h)
        sdw_view = GC.rview;  // (s125 c) layer 0 without the actors in it: no self-shadow
    sun_shadow_log(sdw_on, s[0].valid ? s[0].feet : NULL, sdw_m, sdw_w, sdw_h);
    pc.vp[0] = vx, pc.vp[1] = vy, pc.vp[2] = vw, pc.vp[3] = vh;
    pc.z[0] = proj[0], pc.z[1] = proj[1], pc.z[2] = 0.03f, pc.z[3] = 0;

    VkCommandBuffer cb = vk_cmd();
    PdRing pdr = {0};  // (s183 amd) at most one push per shadow + one per (scissor rect <= 4, pass <= 4) per drawn actor
    pd_begin(&pdr, (uint32_t)nvalid * 17 + 1);
    if (window) vk_frame_suspend();
    vk_full_barrier(cb);  // (the game image's draw into the window image / BotW's composite before our load of it)
    VkRenderingAttachmentInfo ca = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = cv,
                                    .imageLayout = lay,
                                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
    VkRenderingAttachmentInfo da = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = G.depth_view,
                                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .clearValue.depthStencil = {1.0f, 0}};
    VkRenderingInfo ri = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, ext}, .layerCount = 1,
                          .colorAttachmentCount = 1, .pColorAttachments = &ca, .pDepthAttachment = &da};
    rec_CmdBeginRendering(cb, &ri);
    VkViewport vp = {vx, vy, vw, vh, 0, 1};
    VkRect2D sc = {{(int32_t)vx > 0 ? (int32_t)vx : 0, (int32_t)vy > 0 ? (int32_t)vy : 0}, {(uint32_t)vw, (uint32_t)vh}};
    rec_CmdSetViewport(cb, 0, 1, &vp);
    rec_CmdSetScissor(cb, 0, 1, &sc);
    VkDescriptorSet fset = VK_NULL_HANDLE;  // (mp2-perf C4) set 1 for the whole pass: scene depth + sun cascade (views missing: an actor's texture 0, never sampled then)
    if (g_perf & PERF_C4) {
        VkImageView fb = VK_NULL_HANDLE;
        for (int i = 0; i < nact && !fb; i++)
            if (s[i].valid) fb = g_act[i].gpu.tex_view[0];
        fset = vk_alloc_set(G.set_layout1);
        VkDescriptorImageInfo ii[2] = {{G.nearest, dview ? dview : fb, VK_IMAGE_LAYOUT_GENERAL}, {G.nearest, sdw_view ? sdw_view : fb, VK_IMAGE_LAYOUT_GENERAL}};
        VkWriteDescriptorSet w = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = fset, .dstBinding = 0, .descriptorCount = 2,
                                  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = ii};
        mod_update_set(&w);
        rec_CmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, G.layout, 1, 1, &fset, 0, NULL);
    }
    {  // (s121 light) binding 1, per instance: record 2 * actor = its light, 2 * actor + 1 = the same unlit (style 0: shadow
       // discs, KH2 NoLight groups); a draw picks its record with firstInstance
        static float lrec[SORA_DRAW_ACTORS * 2][LREC_F];
        memset(lrec, 0, sizeof lrec);
        for (int i = 0; i < nact; i++) {
            if (s[i].valid) memcpy(lrec[2 * i], s[i].light, sizeof s[i].light);
            if (s[i].valid && sdw_on) sun_shadow_rec(lrec[2 * i], s[i].feet, sdw_m);  // (s124 b)
            if (s[i].valid) lrec[2 * i][47] = g_sdw[13];  // (s126 lflat: also with no sun shadow map)
            if (dbg) lrec[2 * i][3] = 0;
            memcpy(lrec[2 * i + 1], lrec[2 * i], sizeof lrec[0]), lrec[2 * i + 1][3] = 0, lrec[2 * i + 1][16] = 0;
        }
        VkSpan lb = vk_transient_copy(lrec, (uint64_t)(nact > 0 ? nact : 1) * 2 * sizeof lrec[0], 16);
        VkDeviceSize lo = lb.off;
        rec_CmdBindVertexBuffers(cb, 1, 1, &lb.buf, &lo);
    }
    if (scr && cam.clear[2] > cam.clear[0] && cam.clear[3] > cam.clear[1]) {  // (s59) the black behind him
        float x0 = vx + cam.clear[0] * vw, y0 = vy + cam.clear[1] * vh, x1 = vx + cam.clear[2] * vw, y1 = vy + cam.clear[3] * vh;
        x0 = x0 < 0 ? 0 : x0, y0 = y0 < 0 ? 0 : y0;
        x1 = x1 > (float)ext.width ? (float)ext.width : x1, y1 = y1 > (float)ext.height ? (float)ext.height : y1;
        if (x1 > x0 + 0.5f && y1 > y0 + 0.5f) {
            VkClearAttachment cl = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .colorAttachment = 0,
                                    .clearValue.color = {.float32 = {0, 0, 0, 1}}};
            VkClearRect cr = {.rect = {{(int32_t)x0, (int32_t)y0}, {(uint32_t)(x1 - x0), (uint32_t)(y1 - y0)}},
                              .baseArrayLayer = 0, .layerCount = 1};
            rec_CmdClearAttachments(cb, 1, &cl, 1, &cr);
        }
    }
    // the shadows first (under the feet; the blended pipeline: depth test only, our depth is still clear, so no body
    // drawn after them ever loses to one): per actor a fan of SHADOW_SEG triangles flat in XZ at floor + SHADOW_LIFT,
    // radius and alpha shrunk linearly with the height above the floor up to fade_h. The vertex shader places it (all
    // vertices at the feet, uv = the rim direction). With the scene depth (and SHADOW_DROP on) it is a decal instead:
    // one quad over the viewport, the fragment shader finds the floor under each pixel (see FRAG_SRC, z.w 2); the
    // decal's inverse is the camera's, made once for all actors. sh_h / sh_r / sh_a: actor 0's, for the log
    static int drop = -1;
    if (drop < 0) {
        const char *e = getenv("SAVAGEWIND_SORA_SHADOW_DROP");
        drop = !(e && *e && !atoi(e));
        if (!drop) LOG("[sora-draw] blob shadow: no scene-depth floor search (SAVAGEWIND_SORA_SHADOW_DROP=0)\n");
    }
    float sh_h = 0, sh_r = 0, sh_a = 0;
    float mvp_keep[16], mvp_inv[16];
    bool decal = false;
    memcpy(mvp_keep, pc.mvp, sizeof mvp_keep);
    if (!dbg && drop && mode > 0) {
        // the decal's world point from the viewport: p = zs * M3^-1 (ndc.x, ndc.y, 1) + cam, M3 = the mvp's x / y / w
        // rows (clip w = eye depth), cam = -M3^-1 (their column 3); GLSL mat4 = column-major: m[col * 4 + row]
        const float *m = pc.mvp;
        double a[3][3] = {{m[0], m[4], m[8]}, {m[1], m[5], m[9]}, {m[3], m[7], m[11]}}, t[3] = {m[12], m[13], m[15]};
        double det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
                     a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
        if (fabs(det) > 1e-12) {
            double inv[3][3];
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++) {  // inv[i][j] = cofactor(j, i) / det
                    int r0 = (j + 1) % 3, r1 = (j + 2) % 3, c0 = (i + 1) % 3, c1 = (i + 2) % 3;
                    inv[i][j] = (a[r0][c0] * a[r1][c1] - a[r0][c1] * a[r1][c0]) / det;
                }
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) mvp_inv[j * 4 + i] = (float)inv[i][j];
                mvp_inv[i * 4 + 3] = 0;
                mvp_inv[12 + i] = (float)-(inv[i][0] * t[0] + inv[i][1] * t[1] + inv[i][2] * t[2]);
            }
            mvp_inv[15] = 1;
            decal = true;
        }
    }
    static uint32_t si[SHADOW_SEG * 3];
    if (!si[2])
        for (int i = 0; i < SHADOW_SEG; i++) si[i * 3] = 0, si[i * 3 + 1] = 1 + (uint32_t)i, si[i * 3 + 2] = 1 + (uint32_t)((i + 1) % SHADOW_SEG);
    for (int ai = 0; ai < nact; ai++) {
        const ASnap *as = &s[ai];
        if (!as->valid) continue;
        const Shadow *sh = &as->sh;
        float h = as->feet[1] - sh->floor_y, t = sh->fade_h > 0 ? h / sh->fade_h : 0;
        if (t > 1) t = 1;
        if (t < 0) t = 0;
        if (ai == 0) sh_h = h, sh_r = sh->radius * (1 - (1 - sh->min_scale) * t), sh_a = sh->strength * (1 - (1 - sh->min_alpha) * t);
        if (!sh->on || dbg || scr || !(sh->strength > 0.004f) || !(sh->radius > 0.01f)) continue;
        float sv[(1 + SHADOW_SEG) * VB_F];
        float cx = as->feet[0], cy = as->feet[1], cz = as->feet[2];
        memset(sv, 0, sizeof sv);  // (s121: normals 0)
        sv[0] = cx, sv[1] = cy, sv[2] = cz, sv[3] = 0, sv[4] = 0;
        for (int i = 0; i < SHADOW_SEG; i++) {
            float an = (float)i * (2 * 3.14159265f / SHADOW_SEG), *o = sv + (1 + i) * VB_F;
            o[0] = cx, o[1] = cy, o[2] = cz, o[3] = cosf(an), o[4] = sinf(an);
        }
        if (decal) {  // a fan of radius 2 in NDC covers the viewport (inradius 2 cos(7.5 deg) > sqrt 2)
            for (int i = 0; i <= SHADOW_SEG; i++) sv[i * VB_F] = sv[i * VB_F + 3] * 2, sv[i * VB_F + 1] = sv[i * VB_F + 4] * 2, sv[i * VB_F + 2] = 0;
            pd.s2[0] = cx, pd.s2[1] = cy, pd.s2[2] = cz, pd.s2[3] = 0;
            memcpy(pc.mvp, mvp_inv, sizeof mvp_inv);
        }
        VkSpan svb = vk_transient_copy(sv, sizeof sv, 16), sib = vk_transient_copy(si, sizeof si, 16);
        VkDeviceSize svo = svb.off;
        rec_CmdBindVertexBuffers(cb, 0, 1, &svb.buf, &svo);
        rec_CmdBindIndexBuffer(cb, sib.buf, sib.off, VK_INDEX_TYPE_UINT32);
        rec_CmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, P[1]);
        pc.k[0] = sh->strength, pc.k[1] = 1, pc.k[2] = mode, pc.k[3] = 0, pc.z[2] = SHADOW_BIAS, pc.z[3] = decal ? 2.0f : 1.0f;
        memcpy(pd.t, as->tint, sizeof pd.t);  // (as before s54-party: the flat disc's colour goes through the tint too)
        pd.s0[0] = sh->floor_y, pd.s0[1] = sh->radius, pd.s0[2] = sh->fade_h, pd.s0[3] = sh->min_scale;
        pd.s1[0] = sh->min_alpha, pd.s1[1] = 0, pd.s1[2] = SHADOW_DROP_MAX, pd.s1[3] = SHADOW_LIFT;
        pd_push(cb, &pdr, &pc, &pd);
        bind_draw_sets(cb, &g_act[ai], 0, fset, dview, sdw_view);
        rec_CmdDrawIndexed(cb, SHADOW_SEG * 3, 1, 0, 0, (uint32_t)(2 * ai + 1));
    }
    memcpy(pc.mvp, mvp_keep, sizeof mvp_keep);
    pc.z[2] = 0.03f, pc.z[3] = 0;  // (the bodies: their bias, no shadow mode)
    pd.s2[0] = pd.s2[1] = pd.s2[2] = 0, pd.s2[3] = g_near_cut;  // (s102 leaves: occluders this close to the camera don't hide the bodies)
    pd.s1[0] = 0;
    memset(pd.s0, 0, sizeof pd.s0), pd.s1[2] = pd.s1[3] = 0, pd.oa[3] = 0;
    float sw[12];
    if (g_socc_on && !scr && linkpos_slate_model_world(sw)) {  // (s107 slateocc) world -> pc: axes 0/1 unit, the third is their cross product
        float a0[3] = {sw[0], sw[4], sw[8]}, a1[3] = {sw[1], sw[5], sw[9]}, a2[3] = {sw[2], sw[6], sw[10]};
        float l0 = sqrtf(a0[0] * a0[0] + a0[1] * a0[1] + a0[2] * a0[2]), l1 = sqrtf(a1[0] * a1[0] + a1[1] * a1[1] + a1[2] * a1[2]),
              l2 = sqrtf(a2[0] * a2[0] + a2[1] * a2[1] + a2[2] * a2[2]);
        const float *v = cam.view;  // camera world position = -R^T t
        if (l0 > 1e-4f && l1 > 1e-4f && l2 > 1e-4f) {
            for (int j = 0; j < 3; j++) {
                pd.oa[j] = a0[j] / l0, pd.ob[j] = a1[j] / l1, pd.oc[j] = sw[j * 4 + 3];
                pd.s0[j] = -(v[j] * v[3] + v[4 + j] * v[7] + v[8 + j] * v[11]);
            }
            pd.oa[3] = 1, pd.s1[2] = g_socc_h[0] * 0.001f * l0, pd.s1[3] = g_socc_h[1] * 0.001f * l1, pd.s0[3] = g_socc_h[2] * 0.001f * l2;
        }
    }
    if (g_leaf_mode) pd.s2[0] = g_leaf_k / 10.0f, pd.s2[2] = g_leaf_range / 100.0f, pd.s2[3] = 0, pd.s1[0] = g_leaf_thr / 100.0f;  // (s105 leafmode 1: near cut off)
    // the bodies' vertices (world, skinned on the game thread) + indices, one transient copy each per present
    bool any_ghost = false;
    for (int ai = 0; ai < nact; ai++) {
        if (!s[ai].valid) continue;
        const DrawActor *a = &g_act[ai];
        const uint64_t pf_tu = plat_now_ns();
        if (!vb_done[ai]) vb[ai] = vk_transient_copy(a->gpu.vb, (uint64_t)a->nverts * VB_F * sizeof(float), 16);
        if ((g_perf & PERF_C2) && a->gpu.ib) ib[ai] = (VkSpan){a->gpu.ib, 0, NULL};  // (mp2-perf C2: the static index buffer)
        else ib[ai] = vk_transient_copy(a->idx, (uint64_t)a->nidx * 4, 16);
        p_up[ai == g_body ? 0 : 1] += pf_ms(pf_tu);
        any_ghost |= s[ai].ghost > 0.004f && mode > 0 && !dbg;
    }
    // opaque groups, then the alpha phase (KH2 group attribute bit 0); with a ghost (s48 water) every group again where
    // the scene hides the body: 2 = depth only, 3 = colour at its ghost alpha (only the nearest hidden surface: no X-ray)
    // (s105) screen mode with a hole: the viewport rect minus the mask rect = up to 4 scissor rects, the whole body once per rect
    VkRect2D srect[4];
    int nsr = 1;
    srect[0] = sc;
    if (scr && cam.mask[2] > cam.mask[0] && cam.mask[3] > cam.mask[1]) {
        int32_t sx0 = sc.offset.x, sy0 = sc.offset.y, sx1 = sx0 + (int32_t)sc.extent.width, sy1 = sy0 + (int32_t)sc.extent.height;
        int32_t mx0 = (int32_t)(vx + cam.mask[0] * vw), my0 = (int32_t)(vy + cam.mask[1] * vh);
        int32_t mx1 = (int32_t)(vx + cam.mask[2] * vw), my1 = (int32_t)(vy + cam.mask[3] * vh);
        mx0 = mx0 < sx0 ? sx0 : mx0, my0 = my0 < sy0 ? sy0 : my0, mx1 = mx1 > sx1 ? sx1 : mx1, my1 = my1 > sy1 ? sy1 : my1;
        if (mx1 > mx0 && my1 > my0) {
            nsr = 0;
            if (my0 > sy0) srect[nsr++] = (VkRect2D){{sx0, sy0}, {(uint32_t)(sx1 - sx0), (uint32_t)(my0 - sy0)}};  // above
            if (my1 < sy1) srect[nsr++] = (VkRect2D){{sx0, my1}, {(uint32_t)(sx1 - sx0), (uint32_t)(sy1 - my1)}};  // below
            if (mx0 > sx0) srect[nsr++] = (VkRect2D){{sx0, my0}, {(uint32_t)(mx0 - sx0), (uint32_t)(my1 - my0)}};  // left
            if (mx1 < sx1) srect[nsr++] = (VkRect2D){{mx1, my0}, {(uint32_t)(sx1 - mx1), (uint32_t)(my1 - my0)}};  // right
        }
    }
    for (int ri = 0; ri < nsr; ri++) {
    rec_CmdSetScissor(cb, 0, 1, &srect[ri]);
    if (fset) rec_CmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, G.layout, 1, 1, &fset, 0, NULL);
    for (int pass = 0; pass < (any_ghost ? 4 : 2); pass++) {
        rec_CmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, P[pass == 2 ? 2 : pass == 3 ? 1 : pass]);
        for (int ai = 0; ai < nact; ai++) {
            const DrawActor *a = &g_act[ai];
            if (!s[ai].valid || (pass >= 2 && !(s[ai].ghost > 0.004f && mode > 0 && !dbg))) continue;
            const uint64_t pf_tr = plat_now_ns();
            pc.k[0] = pass == 1 ? 0.02f : 0.5f, pc.k[1] = (float)(pass == 1), pc.k[2] = mode, pc.k[3] = dbg ? 1.0f : 0.0f;
            pd.s1[1] = pass >= 2 ? s[ai].ghost : 0;
            memcpy(pd.t, s[ai].tint, sizeof pd.t);
            pd_set_org(&pd, pc.mvp, s[ai].feet);  // (s217 depth; pc.mvp = mvp_keep here, after the cam.shift edit)
            pd_push(cb, &pdr, &pc, &pd);
            VkDeviceSize vo = vb[ai].off;
            rec_CmdBindVertexBuffers(cb, 0, 1, &vb[ai].buf, &vo);
            rec_CmdBindIndexBuffer(cb, ib[ai].buf, ib[ai].off, VK_INDEX_TYPE_UINT32);
            for (uint32_t g = 0; g < a->ngroups; g++) {
                const MGroup *gr = &a->groups[g];
                if ((pass < 2 && (int)(gr->attr & 1) != pass) || gr->tex >= a->ntex || gr->first + gr->n > a->nidx) continue;
                if (s[ai].hide_weapon && (gr->attr & ATTR_WEAPON)) continue;
                if ((gr->attr & ATTR_WEAPON) && ATTR_ALT(gr->attr)) {  // (s85) an alt-blade bake: only the picked blade per chain
                    uint32_t c = a->gchain[g], want = s[ai].blade[c] ? s[ai].blade[c] : c + 1;
                    if (ATTR_ALT(gr->attr) != want) continue;
                }
                bind_draw_sets(cb, a, gr->tex, fset, dview, sdw_view);
                rec_CmdDrawIndexed(cb, gr->n, 1, gr->first, 0, (uint32_t)(2 * ai + ((gr->attr & ATTR_NOLIGHT) ? 1 : 0)));
            }
            p_rec[ai == g_body ? 0 : 1] += pf_ms(pf_tr);
        }
    }
    }
    rec_CmdSetScissor(cb, 0, 1, &sc);
    for (int k = 0; k < 2; k++)
        if (p_has[k]) pf_add(&g_pf.up[k], p_up[k]), pf_add(&g_pf.rec[k], p_rec[k]);
    pd.s1[1] = 0;
    if (!scr) sora_fx_draw(cb, (const float *)&pc, dview, G.nearest, fmt);  // KH2 effects over / behind his body (runtime/sora_fx.c)
    if (scr) g_last_ok = false;  // (s59: no world camera this frame for sora_draw_project)
    rec_CmdEndRendering(cb);
    vk_full_barrier(cb);
    if (window) vk_frame_resume();  // (the overlay menu draws after this)
    if (s[g_body].valid) g_dg_drawn = vk_frame_counter(), g_dg_skip = "";  // (s183 kairi-diag)
    if (G.drawn++ % 300 == 0) {
        LOG("[sora-draw]   ui order %d: drawn into the %s %ux%u (format %d); scene passes %llu, fallbacks %llu, skipped %llu\n",
            g_ui_order, window ? "window image" : "game image", ext.width, ext.height, fmt, (unsigned long long)g_ub.passes,
            (unsigned long long)g_ub.fallbacks, (unsigned long long)g_ub.skips);
        int a0 = 0;  // (the log's body: Sora, else the first drawn)
        while (a0 < nact - 1 && !s[a0].valid) a0++;
        const float *v0 = vb_done[a0] ? (const float *)vb[a0].ptr : g_act[a0].gpu.vb;
        LOG("[sora-draw] frame %llu: at %.2f %.2f %.2f, proj %s near %.3f far %.0f fovy %.1f aspect %.3f, viewport %.0f %.0f %.0fx%.0f, "
            "actors %d drawn of %d\n",
            (unsigned long long)G.drawn - 1, v0[0], v0[1], v0[2], cam.have_proj ? proj_src_name(cam.proj_src) : "default", proj[0], proj[1],
            proj[2] * 57.29578f, proj[3], vx, vy, vw, vh, nvalid, nact),
        LOG("[sora-draw]   scene depth %s %ux%u (%u draws) uv %.3f %.3f %.3f %.3f, depth mode %.0f\n", dview ? "yes" : "no", dw, dh,
            ddraws, pc.duv[0], pc.duv[1], pc.duv[2], pc.duv[3], mode),
        LOG("[sora-draw]   shadow %s: floor y %.3f (%s) feet y %.3f height %.2f -> radius %.2f alpha %.2f, ghost %.2f\n",
            s[0].sh.on ? "on" : "off", s[0].sh.floor_y, s[0].sh.set ? "from sora.c" : "= feet, no sora_draw_shadow call",
            s[0].feet[1], sh_h, sh_r, sh_a, s[0].ghost);
        LOG("[sora-draw]   light style %.0f amount %.2f, normals %.3f ms avg %.3f max (body)\n", s[a0].light[3], s[a0].light[7],
            g_norm_ms, g_norm_max);  // (s121 light)
        LOG("[sora-draw]   perf %d (ms avg/max; body | others): game skin %.3f/%.3f | %.3f/%.3f, norm %.3f/%.3f | %.3f/%.3f, pub %.3f/%.3f | %.3f/%.3f"
            "; render snap %.3f/%.3f | %.3f/%.3f, up %.3f/%.3f | %.3f/%.3f, rec %.3f/%.3f | %.3f/%.3f (game = per call; render = per pass, others summed)\n",
            g_perf, g_pf.skin[0].avg, g_pf.skin[0].mx, g_pf.skin[1].avg, g_pf.skin[1].mx, g_pf.norm[0].avg, g_pf.norm[0].mx,
            g_pf.norm[1].avg, g_pf.norm[1].mx, g_pf.pub[0].avg, g_pf.pub[0].mx, g_pf.pub[1].avg, g_pf.pub[1].mx,
            g_pf.snap[0].avg, g_pf.snap[0].mx, g_pf.snap[1].avg, g_pf.snap[1].mx, g_pf.up[0].avg, g_pf.up[0].mx,
            g_pf.up[1].avg, g_pf.up[1].mx, g_pf.rec[0].avg, g_pf.rec[0].mx, g_pf.rec[1].avg, g_pf.rec[1].mx);
        for (int k = 0; k < 2; k++)
            g_pf.skin[k].mx = g_pf.norm[k].mx = g_pf.pub[k].mx = g_pf.snap[k].mx = g_pf.up[k].mx = g_pf.rec[k].mx = 0;
        for (int i = 1; i < nact; i++)
            if (s[i].valid) LOG("[sora-draw]   actor %d at %.2f %.2f %.2f, floor y %.3f%s\n", i, s[i].feet[0], s[i].feet[1], s[i].feet[2],
                                s[i].sh.floor_y, s[i].weapon_at ? ", weapon detached" : s[i].hide_weapon ? ", weapon hidden" : "");
    }
    return true;
}

// ================================================================== (s125 c) Sora's own shadow in BotW's sun cascades
// exec.c (casc_cast_check), before the frame's first draw that samples BotW's cascade array (its sun pass / the half-size
// copy), ends the game's pass and calls sora_draw_cast with one depth view per cascade layer drawn this frame + that layer's
// world -> clip matrix (row-major, as casc_note kept it). Every posed actor's body goes in depth-only, so BotW's own light
// pass shades the ground / water / grass with it. The clip point is written exactly as the receive lookup (FRAG_SRC
// sun_vis, verified s124) reads it back: flags bit 0 = y flipped, bit 1 = depth 0..1 (else -1..1 -> stored z * 0.5 + 0.5,
// like exec.c's ShaderKey depth_minus_one), bit 2 = greater-is-nearer (compare GEQUAL). Depth clamp on, as BotW's cascade
// draws (S123 probe P2: clamp 1). push = the casters moved this far away from the sun (m, GUESS 0: Sora's self-shadow acne
// is handled by the receive side's shoff / shbias). Modes (DLL shcast): 0 off, 1 every actor (default), 2 Sora only.
void sora_draw_cast_cfg(float mode, float mask, float push) {
    if (mode != g_cast[0] || mask != g_cast[1] || push != g_cast[2])
        LOG("[sora-draw] cast shadow: mode %.0f layers %.0f push %.3f m\n", mode, mask, push);
    g_cast[0] = mode, g_cast[1] = mask, g_cast[2] = push;
}
bool sora_draw_cast_want(void) { return g_cast[0] > 0.5f && ((int)(g_cast[1] + 0.5f) & 7) && g_state == 1 && !G.failed && G.init; }

static const char *CAST_VS =
    "#version 450\n"
    "layout(location = 0) in vec3 pos;\n"
    "layout(push_constant) uniform P { vec4 m0; vec4 m1; vec4 m2; vec4 m3; vec4 push; } pc;\n"
    "void main() {\n"
    "    vec4 q = vec4(pos + pc.push.xyz, 1.0);\n"
    "    vec4 c = vec4(dot(pc.m0, q), dot(pc.m1, q), dot(pc.m2, q), dot(pc.m3, q));\n"
    "    int f = int(pc.push.w + 0.5);\n"
    "    if ((f & 1) != 0) c.y = -c.y;\n"
    "    if ((f & 2) == 0) c.z = 0.5 * (c.z + c.w);\n"
    "    gl_Position = c;\n"
    "}\n";

static bool fmt_has_stencil(VkFormat f) {
    return f == VK_FORMAT_D16_UNORM_S8_UINT || f == VK_FORMAT_D24_UNORM_S8_UINT || f == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

static VkPipeline cast_pipeline(VkFormat fmt, bool ge) {
    for (int i = 0; i < 4; i++)
        if (GC.pipe[i].p && GC.pipe[i].fmt == fmt && GC.pipe[i].ge == ge) return GC.pipe[i].p;
    static VkShaderModule vs;
    if (!vs) vs = vk_compile_glsl(CAST_VS, STAGE_VERTEX, "sora_cast.vert");
    if (!vs) return VK_NULL_HANDLE;
    if (sora_push_limit() < 20 * 4) {  // (s183 amd: never on a real GPU, Vulkan's minimum is 128; SAVAGEWIND_SORA_PUSH_LIMIT below 80)
        static bool said;
        if (!said) said = true, LOG("[sora-draw] cast shadow: %u bytes of push constants, needs %d (off)\n", sora_push_limit(), 20 * 4);
        return VK_NULL_HANDLE;
    }
    if (!GC.layout) {
        VkPushConstantRange r = {VK_SHADER_STAGE_VERTEX_BIT, 0, 20 * 4};
        VkPipelineLayoutCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pushConstantRangeCount = 1,
                                         .pPushConstantRanges = &r};
        if (vkCreatePipelineLayout(g_vk.dev, &ci, NULL, &GC.layout) != VK_SUCCESS) { GC.layout = VK_NULL_HANDLE; return VK_NULL_HANDLE; }
    }
    VkPipelineShaderStageCreateInfo stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                             .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main"};
    VkVertexInputBindingDescription bind = {0, VB_F * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attr = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
    VkPipelineVertexInputStateCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                               .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &bind,
                                               .vertexAttributeDescriptionCount = 1, .pVertexAttributeDescriptions = &attr};
    VkPipelineInputAssemblyStateCreateInfo ia = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                 .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo vps = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                 .depthClampEnable = g_vk.depth_clamp, .polygonMode = VK_POLYGON_MODE_FILL,
                                                 .cullMode = VK_CULL_MODE_NONE, .lineWidth = 1};
    VkPipelineMultisampleStateCreateInfo ms = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                               .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo ds = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_TRUE,
                                                .depthCompareOp = ge ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL};
    VkPipelineColorBlendStateCreateInfo cb = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dyn};
    VkPipelineRenderingCreateInfo ri = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .depthAttachmentFormat = fmt,
                                        .stencilAttachmentFormat = fmt_has_stencil(fmt) ? fmt : VK_FORMAT_UNDEFINED};
    VkGraphicsPipelineCreateInfo pi = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri, .stageCount = 1,
                                       .pStages = &stage, .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vps,
                                       .pRasterizationState = &rs, .pMultisampleState = &ms, .pDepthStencilState = &ds,
                                       .pColorBlendState = &cb, .pDynamicState = &dy, .layout = GC.layout};
    VkPipeline p = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(g_vk.dev, g_vk.pcache, 1, &pi, NULL, &p) != VK_SUCCESS) return VK_NULL_HANDLE;
    int slot = 0;
    while (slot < 3 && GC.pipe[slot].p) slot++;  // (a 5th format / compare replaces the last: leaked, may be in flight)
    GC.pipe[slot].fmt = fmt, GC.pipe[slot].ge = ge, GC.pipe[slot].p = p;
    LOG("[sora-draw] cast shadow: pipeline for depth format %d (%s, depth clamp %d)\n", fmt, ge ? "GEQUAL" : "LEQUAL", (int)g_vk.depth_clamp);
    return p;
}

// views[l] = layer l's depth view (VK_NULL_HANDLE: not drawn this frame), m[l] its matrix; no rendering is open.
// (s125 c) GC.r* = an image like layer 0 of the cascades (w x h, fmt), made / remade on a size or format change
static bool cast_copy_for(VkFormat fmt, uint32_t w, uint32_t h) {
    if (GC.rfail) return false;
    if (GC.rimg && GC.rfmt == fmt && GC.rw == w && GC.rh == h) return true;
    if (GC.rimg) vk_defer_view(GC.rview), vk_defer_image(GC.rimg, GC.rmem), GC.rimg = VK_NULL_HANDLE, GC.rview = VK_NULL_HANDLE;
    VkImageCreateInfo ic = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = fmt,
                            .extent = {w, h, 1}, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
                            .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};
    VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    if (vmaCreateImage(g_vk.vma, &ic, &ai, &GC.rimg, &GC.rmem, NULL) != VK_SUCCESS) {
        GC.rimg = VK_NULL_HANDLE, GC.rfail = true;
        LOG("[sora-draw] cast shadow: no copy image (%ux%u format %d): the bodies shade themselves\n", w, h, fmt);
        return false;
    }
    vk_image_init_layout(vk_upload_cmd(), GC.rimg, VK_IMAGE_ASPECT_DEPTH_BIT);
    VkImageViewCreateInfo vc = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = GC.rimg, .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                .format = fmt, .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}};
    if (vkCreateImageView(g_vk.dev, &vc, NULL, &GC.rview) != VK_SUCCESS) {
        vk_defer_image(GC.rimg, GC.rmem), GC.rimg = VK_NULL_HANDLE, GC.rview = VK_NULL_HANDLE, GC.rfail = true;
        return false;
    }
    GC.rfmt = fmt, GC.rw = w, GC.rh = h;
    LOG("[sora-draw] cast shadow: receive copy %ux%u format %d\n", w, h, fmt);
    return true;
}

void sora_draw_cast(VkImage img, const VkImageView views[3], VkFormat fmt, uint32_t w, uint32_t h, const float m[3][16]) {
    if (!sora_draw_cast_want() || GC.failed || !w || !h) return;
    const int mode = (int)(g_cast[0] + 0.5f), mask = (int)(g_cast[1] + 0.5f), fl = sun_shadow_flags();
    VkPipeline P = cast_pipeline(fmt, (fl & 4) != 0);
    if (!P) { GC.failed = true; LOG("[sora-draw] cast shadow: pipeline failed (format %d): off\n", fmt); return; }
    // the bodies as the scene pass will draw them this frame: the ring entry of this command list's game frame (drawsync 1),
    // else the newest snapshot; copied under g_lock
    VkSpan vb[SORA_DRAW_ACTORS], ib[SORA_DRAW_ACTORS];
    bool ok[SORA_DRAW_ACTORS] = {0}, hidew[SORA_DRAW_ACTORS] = {0};
    uint8_t blade[SORA_DRAW_ACTORS][2];
    int nact, nok = 0;
    uint64_t want = nvn_exec_frame_tag() + (uint64_t)(int64_t)g_sync_off;
    pthread_mutex_lock(&g_lock);
    nact = g_nact;
    for (int i = 0; i < nact && !g_cam.screen; i++) {
        DrawActor *a = &g_act[i];
        if (!a->snap.valid || a->gpu.failed || !a->idx || !a->nidx || (mode == 2 && i != g_body)) continue;
        int newest = -1, pick = -1;
        for (int r = 0; r < DRAW_RING; r++)
            if (a->ring[r].tag && a->ring[r].vb && (newest < 0 || a->ring[r].tag > a->ring[newest].tag)) newest = r;
        if (g_sync == 1 && newest >= 0) pick = ring_pick(a, newest, want, 0);
        const float *v = pick >= 0 ? a->ring[pick].vb : snap_vb(a);
        if (!v) continue;
        hidew[i] = pick >= 0 ? a->ring[pick].hide_weapon : a->snap.hide_weapon;
        memcpy(blade[i], pick >= 0 ? a->ring[pick].blade : a->snap.blade, 2);
        vb[i] = vk_transient_copy(v, (uint64_t)a->nverts * VB_F * sizeof(float), 16);
        if ((g_perf & PERF_C2) && a->gpu.ib) ib[i] = (VkSpan){a->gpu.ib, 0, NULL};  // (mp2-perf C2)
        else ib[i] = vk_transient_copy(a->idx, (uint64_t)a->nidx * 4, 16);
        ok[i] = true, nok++;
    }
    pthread_mutex_unlock(&g_lock);
    if (!nok) return;
    // push: away from the sun = along layer 0's depth row (+ when less-is-nearer), the same world vector for every layer
    struct { float m[16], push[4]; } pc;
    float push[3] = {0, 0, 0};
    {
        const float *r2 = m[0] + 8;
        float l = sqrtf(r2[0] * r2[0] + r2[1] * r2[1] + r2[2] * r2[2]), k = l > 1e-8f ? g_cast[2] / l * ((fl & 4) ? -1.0f : 1.0f) : 0;
        for (int j = 0; j < 3; j++) push[j] = r2[j] * k;
    }
    VkCommandBuffer cb = vk_cmd();
    vk_full_barrier(cb);  // (BotW's cascade draws before ours)
    if (mode != 3 && views[0] && img && cast_copy_for(fmt, w, h)) {  // layer 0 as BotW drew it, for the bodies' own lookup
        VkImageCopy rg = {.srcSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, .dstSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1},
                          .extent = {w, h, 1}};
        rec_CmdCopyImage(cb, img, VK_IMAGE_LAYOUT_GENERAL, GC.rimg, VK_IMAGE_LAYOUT_GENERAL, 1, &rg);
        GC.rframe = vk_frame_counter() + 1;
        vk_full_barrier(cb);  // (the copy reads layer 0 before we draw into it)
    }
    unsigned tris = 0, layers = 0;
    const bool stencil = fmt_has_stencil(fmt);
    for (int l = 0; l < 3; l++) {
        if (!views[l] || !((mask >> l) & 1)) continue;
        layers |= 1u << l;
        VkRenderingAttachmentInfo da = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = views[l],
                                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL, .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
        VkRenderingAttachmentInfo sa = da;
        VkRenderingInfo ri = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, {w, h}}, .layerCount = 1,
                              .pDepthAttachment = &da, .pStencilAttachment = stencil ? &sa : NULL};
        rec_CmdBeginRendering(cb, &ri);
        VkViewport vp = {0, 0, (float)w, (float)h, 0, 1};
        VkRect2D sc = {{0, 0}, {w, h}};
        rec_CmdSetViewport(cb, 0, 1, &vp);
        rec_CmdSetScissor(cb, 0, 1, &sc);
        rec_CmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, P);
        memcpy(pc.m, m[l], sizeof pc.m);
        pc.push[0] = push[0], pc.push[1] = push[1], pc.push[2] = push[2], pc.push[3] = (float)fl;
        rec_CmdPushConstants(cb, GC.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof pc, &pc);
        for (int ai = 0; ai < nact; ai++) {
            if (!ok[ai]) continue;
            const DrawActor *a = &g_act[ai];
            VkDeviceSize vo = vb[ai].off;
            rec_CmdBindVertexBuffers(cb, 0, 1, &vb[ai].buf, &vo);
            rec_CmdBindIndexBuffer(cb, ib[ai].buf, ib[ai].off, VK_INDEX_TYPE_UINT32);
            for (uint32_t g = 0; g < a->ngroups; g++) {  // (the groups the scene pass draws: weapon hidden / alt blades as there)
                const MGroup *gr = &a->groups[g];
                if (gr->first + gr->n > a->nidx) continue;
                if (hidew[ai] && (gr->attr & ATTR_WEAPON)) continue;
                if ((gr->attr & ATTR_WEAPON) && ATTR_ALT(gr->attr)) {
                    uint32_t c = a->gchain[g], wb = blade[ai][c] ? blade[ai][c] : c + 1;
                    if (ATTR_ALT(gr->attr) != wb) continue;
                }
                rec_CmdDrawIndexed(cb, gr->n, 1, gr->first, 0, 0);
                tris += gr->n / 3;
            }
        }
        rec_CmdEndRendering(cb);
    }
    vk_full_barrier(cb);  // (ours before BotW's readers)
    if (GC.n++ < 3 || GC.n % 3600 == 0)
        LOG("[sora-draw] cast shadow #%u: layers %#x of mask %d, %d actors, %u triangles per layer (%u in all), %ux%u format %d, flags %d, push %.3f m\n",
            GC.n, layers, mask, nok, layers ? tris / (unsigned)__builtin_popcount(layers) : 0, tris, w, h, fmt, fl, g_cast[2]);
}

// (s108, exec.c ui_boundary_check, once a frame) the inventory's menu camera (invsora 3) is the one in use
bool sora_draw_cam_override(void) {
    pthread_mutex_lock(&g_lock);
    bool co = g_cam.co && !g_cam.screen;
    pthread_mutex_unlock(&g_lock);
    return co;
}

// (s57) exec.c, before it ends the game's pass for sora_draw_scene_pass: the ui order is on, this frame has no scene
// pass yet, and an actor has a pose to draw (else the game's pass is left alone).
bool sora_draw_scene_want(void) {
    if (!sora_draw_ui_order() || g_state != 1 || G.failed || g_ub.frame == vk_frame_counter()) return false;
    bool any = false;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_nact && !any && !g_cam.screen; i++) any = g_act[i].snap.valid && !g_act[i].gpu.failed;
    pthread_mutex_unlock(&g_lock);
    return any;
}

// (s57) exec.c ui_boundary_run: right after BotW's scene composite into its final image (view: a raw UNORM view of it,
// layout GENERAL, w x h px; vp = the composite's viewport in it, px), no rendering open. Once a frame.
void sora_draw_scene_pass(VkImageView view, VkFormat fmt, uint32_t w, uint32_t h, const float vp[4]) {
    uint64_t f = vk_frame_counter();
    if (g_ub.frame == f) return;
    g_ub.frame = f;
    memcpy(g_ub.vp, vp, sizeof g_ub.vp);
    g_ub.w = w, g_ub.h = h;
    VkExtent2D ext = {w, h};
    g_ub.ok = draw_pass(view, fmt, VK_IMAGE_LAYOUT_GENERAL, ext, vp[0], vp[1], vp[2], vp[3], false) && g_last_ok;
    if (!g_ub.passes++)
        LOG("[sora-draw] ui order: first scene pass, frame %llu: game image %ux%u format %d, viewport %.0f %.0f %.0fx%.0f, %s\n",
            (unsigned long long)f, w, h, fmt, vp[0], vp[1], vp[2], vp[3], g_ub.ok ? "drawn" : "nothing drawn");
}

// device.c present_locked: inside the window image's rendering, right after the game image went into (vx, vy, vw, vh).
// (s57) With the ui order on, the actors went into the game image at its composite (sora_draw_scene_pass): here only
// the camera's window mapping for sora_draw_project; see "where the pass goes" for the frames without a composite.
void sora_draw_present(float vx, float vy, float vw, float vh) {
    pthread_mutex_lock(&g_lock);
    const bool scr = g_cam.screen;  // (s59 KH2 Game Over: always drawn here, over BotW's UI)
    pthread_mutex_unlock(&g_lock);
    if (scr) {
        static uint64_t n;
        VkExtent2D ext = vk_frame_extent();
        bool drew = draw_pass(vk_frame_view(), g_vk.sc_format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, ext, vx, vy, vw, vh, true);
        if (n++ % 300 == 0) LOG("[sora-draw] screen mode: present %llu %s (game image %.0f %.0f %.0fx%.0f)\n",
                                (unsigned long long)vk_frame_counter(), drew ? "drawn" : "nothing drawn", vx, vy, vw, vh);
        return;
    }
    if (sora_draw_ui_order()) {
        if (g_ub.frame == vk_frame_counter()) {  // drawn at the composite: its game-image viewport -> window px
            float kx = g_ub.w ? vw / (float)g_ub.w : 1, ky = g_ub.h ? vh / (float)g_ub.h : 1;
            g_last_vp[0] = vx + g_ub.vp[0] * kx, g_last_vp[1] = vy + g_ub.vp[1] * ky;
            g_last_vp[2] = g_ub.vp[2] * kx, g_last_vp[3] = g_ub.vp[3] * ky;
            g_last_ok = g_ub.ok;
            return;
        }
        VkImageView dv;
        float duv[4];
        uint32_t dw, dh, dn;
        pthread_mutex_lock(&g_lock);
        const bool cof = g_cam.co;  // (s107 invsora 3: the override camera's frame had no scene composite: drawn here, over the UI)
        pthread_mutex_unlock(&g_lock);
        static uint64_t ncof;
        if (cof && !ncof++) LOG("[sora-draw] camera override: no scene composite in frame %llu: drawn over the window image instead\n",
                                (unsigned long long)vk_frame_counter());
        if (!cof && !vk_scene_depth(&dv, duv, &dw, &dh, &dn)) {  // no 3D world this frame: no new actors either
            g_last_ok = false;
            g_dg_skip = "DB BotW's 3D scene was not found in this frame (menu / loading, or this GPU's scene pass was not recognised)";
            if (g_state == 1 && !g_ub.skips++) LOG("[sora-draw] ui order: frame %llu has no 3D scene: actors not drawn (menu / load / re-present)\n",
                                                   (unsigned long long)vk_frame_counter());
            return;
        }
    }
    VkExtent2D ext = vk_frame_extent();
    bool drew = draw_pass(vk_frame_view(), g_vk.sc_format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, ext, vx, vy, vw, vh, true);
    if (drew && g_ui_order && g_ub.fallbacks++ % 300 == 0)
        LOG("[sora-draw] ui order: no scene composite seen in frame %llu: drawn over the window image (old order), %llu such frames\n",
            (unsigned long long)vk_frame_counter(), (unsigned long long)g_ub.fallbacks);
}

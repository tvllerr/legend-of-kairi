// Sora-in-BotW: KH2's HP / MP gauge drawn over the game, KH2's own art. Recon: D:/SoraBotW/recon/SORA_HUD_RECON.md
//
// Art: D:/SoraBotW/data/hud/hud.bin (tools/hudbake `hud`): image 0 = the KH2 PC HD gauge sheet (field2d/us/es0field.2dd
// 'gaug', 1024x512; the sprite rectangles below are in the PS2 sheet's 512x256 pixels, scaled by the sheet's size),
// image 1 = Sora's portrait (menu/shopface/p_ex100 HD, 384x384). The pieces and their placement are the 'gaug'
// sequence's sprite groups 91-94 (HP ring + bar), 95/96 (the dark red "lost HP" ghost), 110-112 + sprite 122 (MP bar),
// 117 (portrait frame), as KH2's code draws them (the game sets the bar lengths and the screen position itself).
// Geometry (HUD units, origin = the portrait centre, y down): the HP gauge is a bar along the bottom (y 37..45) coming
// from the left, that wraps 3/4 around the portrait (radius 37..45: up the right side, over the top, ending at 9
// o'clock); it fills from the 9 o'clock end, so the straight part drains first. The MP bar (10 units tall, "mp" label at
// its right end) sits in the ring's open bottom-left quadrant, to the left of the portrait.
//
//   SAVAGEWIND_SORA_HUD=0              no HUD
//   SAVAGEWIND_SORA_HUD=<path>         hud.bin (default D:/SoraBotW/data/hud/hud.bin)
//   SAVAGEWIND_SORA_HUD_SCALE=<f>      HUD unit = game image height / 448 * f (default 1)   -- calibration knobs
//   SAVAGEWIND_SORA_HUD_POS=<x>,<y>    ring centre, HUD units from the game image's bottom-right (default 60,177 since
//                                      s57: just above BotW's minimap; 60,66 = the old place, on it; live: sora_hud_place)
//   SAVAGEWIND_SORA_HUD_MP_POS=<x>,<y> MP bar origin (right end of the bar, left of its label) from the ring centre
//                                      (default -40,31 = KH2's ag 116, since s67; was -42,22, on the Drive gauge)
//   SAVAGEWIND_SORA_HUD_HP_PX=<f>      straight bar length per max HP (default 2.0; 20 HP -> 40, 120 -> 240)
//   SAVAGEWIND_SORA_HUD_MP_PX=<f>      MP bar length per max MP (default 0.5; 100 -> 50)
//   SAVAGEWIND_SORA_HUD_EN_POS=<x>,<y> enemy gauge's right end, HUD units from the game image's top-right (default 16,31)
//   SAVAGEWIND_SORA_HUD_EN_HP=<f>      enemy life per full bar (default 300; length = 199 u per bar, at least 24)
//
// The command menu (sora_hud_menu, recon S32_HELPER_L_CMDMENU.md 1.5 / S34_HELPER_M_MENU_DRAW.md): art from
// D:/SoraBotW/data/hud/cmd.bin (tools/hudbake `cmd`, magic 'SKCM'): image 0 = the KH2 PC HD 'comw' sheet of one world
// (tt0 = Twilight Town), image 1 = the label atlas (Attack .. Cure in the evt font since s40, hudbake SORA_CMD_FONT=sys = the old one), plus KH2's menu *animation
// groups* (6 background, 18 title, 9 / 16 / 11 selected / plain / last slot, 45 icon frame, 37 41 39 44 the icons,
// 20 cursor, 60 57 59 58 the Magic page, 65 + 66..69 the shortcut page) pre-composed at their steady frame into
// quads (uv in PS2 px of the 512x256 sheet, 4 corner colours, local rect). Layout = KH2's (hud_kh2 COMMAND_DRAW):
// a 512 x 416 PS2 frame at the gauge's unit, background at (28, 322), level lv at x = 28 + 16 lv, with n rows the
// title at y = 388 - 22 n and row i at y = 410 - 22 n + 22 i, the selected row 8 px right, labels at the row + (10, -8).
//   SAVAGEWIND_SORA_CMD=<path>|0       cmd.bin (default D:/SoraBotW/data/hud/cmd.bin); missing = no menu (logged once)
//   SAVAGEWIND_SORA_MENU_POS=<dx>,<dy> shift the menu, PS2 px right / down (default 0,0: the frame's x centred in the
//                                      image then -85 = KH2 PC's widescreen menu offset, its bottom on the image's)
//   SAVAGEWIND_SORA_MENU_FONT=<h>      label height, PS2 px (default 21 = the vanilla screenshot (s40b), cell 24 PS2 px)
//
// (s54 dream) The Dream Weapon picker (sora_hud_dream, recon/S54_HELPER_DREAM.md): KH2's Station of Awakening lines
// from D:/SoraBotW/data/hud/dream.bin (tools/hudbake `dream`, magic 'SKDM': a label atlas in the sys font + rim, like
// cmd.bin's labels) on a dark box, over the command menu's own slots (cmd.bin groups 6 / 9 / 16 / 11 / 20, no title:
// every comw title sprite says COMMANDS / MAGIC / SHORTCUTS); drawn instead of the command menu while on.
//   SAVAGEWIND_SORA_DREAM=<path>|0     dream.bin (default D:/SoraBotW/data/hud/dream.bin); missing = no picker (logged)
//   SAVAGEWIND_SORA_DREAM_POS=<x>,<y>  the message block's centre, PS2 px of KH2's 512 x 416 frame centred in the image
//                                      (default 256,280 - GUESS; KH2's question carries {13} position 256,320)
//   SAVAGEWIND_SORA_DREAM_FONT=<h>     message line height, PS2 px (default 21 = the menu's)
//
// (s55 HUD) KH2's level-up panel (sora_hud_levelup, recon/S55_HELPER_HUD.md): dk::LEVEL_UP as PHH documents it
// (re:0x17D440 / 0x17D630 / 0x17E050): es0field 'gaug' animation group 104 (one line) / 105 (two lines) pre-composed per
// frame (0-200, 60 Hz, played once) in D:/SoraBotW/data/hud/lvup.bin (tools/hudbake `lvup`, magic 'SKLV': its parts on
// the gauge sheet = texture 0, plus a text atlas = texture 6: KH2's sys.bar lines in the system font at size 14, name and
// level with KH2's drop shadow, the colour 0x80E5E5E5 baked in). KH2's frame with the widescreen +0x55 of the right-side
// HUD, its x 597 on the image's right edge, y 0 on its top, at the gauge's unit. Queue = LEVELUP_INFO: 16 waiting, a
// batch of up to 4 starts when none shows, stacked by the group's UiPadding (44 / 64), the next batch after it ends.
//   SAVAGEWIND_SORA_LVUP=<path>|0      lvup.bin (default D:/SoraBotW/data/hud/lvup.bin); missing = no panel (logged)
//   SAVAGEWIND_SORA_LVUP_POS=<dx>,<dy> shift the panels, KH2 frame px right / down (default 0,0)
//
// (s57 party) KH2's party gauges (sora_hud_party, recon/S57_HELPER_HUDPARTY.md): dk::GAUGE_FRIEND as PHH documents it
// (re:0x181D20 / 0x182730 / 0x182180): per member a plate (es0field 'gaug' anim group 72 normal / 67 low HP / 70 hit /
// 71 intro / 73 / 74 member 2 sliding into / out of member 1's place) at KH2's place relative to Sora's orb (ag 137
// (455,355) -> ag 72 (476,288): 21 right, 67 up; member 2 another 48 up = ag 71's TextPositionX), its children at the
// plate's anchor: ag 64 the lost-HP track, 65 HP (left half), 66 MP (right half), each cut by KH2's CIRCLE_MASK = here
// an angle clip from 6 o'clock (shader mode 5: keep atan2(|x|, y) <= ratio x pi), then the member's face (his 'face'
// sequence: 3 normal, 0 low, 2 KO, 1 hurt) on top. Pre-composed per frame in D:/SoraBotW/data/hud/party.bin (hudbake
// `party`, magic 'SKPT': parts on the gauge sheet = texture 0, the faces = textures 7 / 8). Drawn under Sora's ring
// (KH2's creation order re:0x16C3A0: party 2, party 1, then the player gauge); clocks = wall time x 60, held while the
// gauge is hidden.
//   SAVAGEWIND_SORA_PARTY=<path>|0     party.bin (default D:/SoraBotW/data/hud/party.bin); missing = no plates (logged)
//
// (s67 drive) KH2's Drive gauge (sora_hud_drive, recon/S64_HELPER_DRIVE_RECON.md; dk::GAUGE's Drive part, PHH hud_kh2
// setup re:0x17F9B0 / tick re:0x1816C0): the plate (es0field 'gaug' anim group 21 intro, then 23; 26 during MP Charge),
// a child of the orb; at its anchor the fill (ag 25) cut by KH2's SECTOR_MASK (shader mode 6: a 50 x 120 rectangle
// with its corner at the anchor + (50, 100), turned by 0.004014257 x points, the fill shows outside it), the digit
// (ag 28 + bars at the anchor + (52, -2), restarted on a change), the bar-up effect (ag 22, once on a gain) and the
// full effect (ag 20, looping while bars = max). Pre-composed per frame in D:/SoraBotW/data/hud/drive.bin (hudbake
// `drive`, magic 'SKDR', party.bin's group / frame / part layout, no faces). Drawn over Sora's gauge (KH2 creates the
// plate after the face and the MP bar); clock = the party plates' (wall time x 60).
//   SAVAGEWIND_SORA_DRIVE=<path>|0     drive.bin (default D:/SoraBotW/data/hud/drive.bin); missing = no gauge (logged)
//
// (s78 camp) The KH2 pause menu's quad layer (sora_hud_quads, plan docs/plans/2026-10-30-kh2-pause-menu-slice1.md): the
// DLL sends this frame's quads (KH2's 512 x 416 virtual frame, centred in the image at its height); while they show
// nothing else of the HUD draws. Own shader pair (UV repeat regions for OpenKH's scroll, solid colour quads) and three
// pipelines (normal / add / subtract). Art = D:/SoraBotW/data/hud/camp.bin (hudbake `camp`, magic 'SKCP').
//   SAVAGEWIND_SORA_CAMP=<path>|0      camp.bin (default D:/SoraBotW/data/hud/camp.bin); missing = quads ignored (logged)
// Threads: sora_hud_set / sora_hud_menu on the game thread (values under a mutex); sora_hud_present on the render
// thread draws inside the window image's rendering already begun by device.c (colour only: no suspend, a pipeline of
// its own with alpha blending, one transient vertex buffer per frame). The textures upload once, on the first present.
#include <math.h>
#include <pthread.h>
#include <stdio.h>
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
#include "sora_hud.h"
#include "sora_hud_label.h"  // (mp Task 7: hud_label_origin)
#include "sora_path.h"  // (Deck port: D:/SoraBotW -> SORA_ROOT)

VkImageView vk_frame_view(void);
VkExtent2D vk_frame_extent(void);

// ------------------------------------------------------------------ values (game thread -> render thread)
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
typedef struct { bool on; float hp, hp_max, mp, mp_max; } Vals;
static Vals g_in;
static SoraHudMenu g_menu;
static struct { int kind; float w[3]; } g_lk_in;  // the lock-on icon (sora_hud_target): 0 none, 1 auto, 2 lock

// (s78 camp) the pause menu's quads: g_cq_at = vk_frame_counter() of the last sora_hud_quads (stale after 3 presents)
#define CQ_MAX 2048
static SoraHudQuad g_cq_in[CQ_MAX];
static int g_cq_n;
static uint64_t g_cq_at, g_cq_drawn;  // g_cq_drawn: vk_frame_counter() of the last present that drew them
static int g_cq_split;  // (s94) sora_hud_quads2: how many of them go under the actors
// (s94 review) render thread only: the quads sora_hud_present_under drew from at this present (g_cq_under_fr =
// vk_frame_counter() + 1, 0 = none), so sora_hud_present draws the rest of the SAME set even when the game thread sent
// a new one in between (else a page change could draw quads twice / not at all for a frame)
static SoraHudQuad g_cq_snap[CQ_MAX];
static int g_cq_snap_n, g_cq_snap_sp;
static uint64_t g_cq_under_fr;

static void cq_set_(const SoraHudQuad *q, int n, int split) {
    static bool warned;
    if (!q || n < 0) n = 0;
    if (n > CQ_MAX) {
        if (!warned) warned = true, LOG("[sora-hud] camp: %d quads, clipped to %d\n", n, CQ_MAX);
        n = CQ_MAX;
    }
    pthread_mutex_lock(&g_lock);
    if (n) memcpy(g_cq_in, q, sizeof g_cq_in[0] * (size_t)n);
    g_cq_n = n, g_cq_split = split < 0 ? 0 : split > n ? n : split, g_cq_at = vk_frame_counter();  // (one lock: quads + split together)
    pthread_mutex_unlock(&g_lock);
}

void sora_hud_quads(const SoraHudQuad *q, int n) { cq_set_(q, n, 0); }

// (s94 menuwalk) the same, with quads [0, split) drawn UNDER the party's screen-mode actors (sora_hud_present_under runs
// before sora_draw_present) and the rest over them (user: "infront of the menu but behind their name icons"); split 0 = all over
void sora_hud_quads2(const SoraHudQuad *q, int n, int split) { cq_set_(q, n, split); }

bool sora_hud_quads_shown(void) {
    pthread_mutex_lock(&g_lock);
    bool s = g_cq_drawn && vk_frame_counter() - g_cq_drawn <= 1;
    pthread_mutex_unlock(&g_lock);
    return s;
}

// (mp Task 7) sora_hud_label: one label (the remote player's name plate), stale after 3 presents like the camp quads
// (mp4) sora_hud_label_slot: LB_SLOTS labels (4 players), each its own quads + anchor + staleness; sora_hud_label = slot 0
#define LB_MAX 64
#define LB_SLOTS 4
static SoraHudQuad g_lb_in[LB_SLOTS][LB_MAX];
static int g_lb_n[LB_SLOTS];
static float g_lb_w[LB_SLOTS][3];
static uint64_t g_lb_at[LB_SLOTS];
void sora_hud_label_slot(int slot, const float world[3], const SoraHudQuad *q, int n) {
    if (slot < 0 || slot >= LB_SLOTS) return;
    if (!q || !world || n < 0) n = 0;
    if (n > LB_MAX) n = LB_MAX;
    pthread_mutex_lock(&g_lock);
    if (n) memcpy(g_lb_in[slot], q, sizeof g_lb_in[slot][0] * (size_t)n), memcpy(g_lb_w[slot], world, sizeof g_lb_w[slot]);
    g_lb_n[slot] = n, g_lb_at[slot] = vk_frame_counter();
    pthread_mutex_unlock(&g_lock);
}
void sora_hud_label(const float world[3], const SoraHudQuad *q, int n) { sora_hud_label_slot(0, world, q, n); }

// (s93 meals) text over the command menu's sub rows: the DLL composes the quads (camp.bin's sys font glyphs, tex 1) in the
// menu's own frame (KH2 PS2 px of the 512 x 416 frame, same space as the row sprites). menu_draw adds them after the labels.
// 3 presents without a call = gone. The DLL looks it up by name (GetProcAddress): exes before s93 lack it.
#define MT_MAX 256
static SoraHudQuad g_mt_in[MT_MAX];
static int g_mt_n;
static uint64_t g_mt_at;
void sora_hud_menu_text(const SoraHudQuad *q, int n) {
    if (!q || n < 0) n = 0;
    if (n > MT_MAX) n = MT_MAX;
    pthread_mutex_lock(&g_lock);
    if (n) memcpy(g_mt_in, q, sizeof g_mt_in[0] * (size_t)n);
    g_mt_n = n, g_mt_at = vk_frame_counter();
    pthread_mutex_unlock(&g_lock);
}

static SoraHudDream g_dream_in;  // (s54 dream) the picker panel
static uint64_t g_dream_at;      // vk_frame_counter() of its last sora_hud_dream (stale after 3 presents = hidden)

void sora_hud_dream(const SoraHudDream *d) {
    pthread_mutex_lock(&g_lock);
    if (d) g_dream_in = *d, g_dream_at = vk_frame_counter();
    else g_dream_in.on = false;
    pthread_mutex_unlock(&g_lock);
}

// (s55 HUD) the level-up requests (game thread -> render thread): KH2's ring of 16, taken while fewer than 15 wait
// (re:0x17DF70); g_lv_drop = sora_hud_levelup(NULL) / sora_hud_off: the render thread also clears the panels it shows
#define LV_QUEUE 16
static SoraHudLevelUp g_lv_q[LV_QUEUE];
static int g_lv_nq;
static bool g_lv_drop;

void sora_hud_levelup(const SoraHudLevelUp *u) {
    pthread_mutex_lock(&g_lock);
    if (!u) g_lv_nq = 0, g_lv_drop = true;
    else if (g_lv_nq < LV_QUEUE - 1) g_lv_q[g_lv_nq++] = *u;
    pthread_mutex_unlock(&g_lock);
}

void sora_hud_menu(const SoraHudMenu *m) {
    pthread_mutex_lock(&g_lock);
    if (m) g_menu = *m;
    else g_menu.on = false;
    pthread_mutex_unlock(&g_lock);
}

void sora_hud_set(float hp, float hp_max, float mp, float mp_max) {
    pthread_mutex_lock(&g_lock);
    g_in.on = hp_max > 0;
    g_in.hp = hp < 0 ? 0 : hp > hp_max ? hp_max : hp;
    g_in.hp_max = hp_max;
    g_in.mp_max = mp_max > 0 ? mp_max : 0;
    g_in.mp = mp < -g_in.mp_max ? -g_in.mp_max : mp > g_in.mp_max ? g_in.mp_max : mp;  // (< 0: MP Charge, -mp = progress)
    pthread_mutex_unlock(&g_lock);
}

// (s57 party) the members' values (game thread -> render thread, under g_lock) and the live ring placement (0 = env /
// default). at = vk_frame_counter() of the member's last call (30 presents without one = not shown).
typedef struct { bool shown; float hp, hp_max, mp, mp_max; uint64_t at; } PtIn;
static PtIn g_pt_in[2];
static float g_place_x, g_place_y;

void sora_hud_party(int member, float hp, float hp_max, float mp, float mp_max, bool shown) {
    if (member < 0 || member > 1) return;
    pthread_mutex_lock(&g_lock);
    PtIn *o = &g_pt_in[member];
    o->shown = shown && hp_max > 0;
    o->hp_max = hp_max > 0 ? hp_max : 0;
    o->hp = hp < 0 ? 0 : hp > o->hp_max ? o->hp_max : hp;
    o->mp_max = mp_max > 0 ? mp_max : 0;
    o->mp = mp < -o->mp_max ? -o->mp_max : mp > o->mp_max ? o->mp_max : mp;  // (< 0: MP Charge, -mp = progress)
    o->at = vk_frame_counter();
    pthread_mutex_unlock(&g_lock);
}

void sora_hud_place(float x, float y) {
    pthread_mutex_lock(&g_lock);
    g_place_x = x > 0 ? x : 0, g_place_y = y > 0 ? y : 0;
    pthread_mutex_unlock(&g_lock);
    LOG("[sora-hud] place: ring centre %s%.0f, %s%.0f units from the bottom-right\n", x > 0 ? "" : "default ", x > 0 ? x : 0, y > 0 ? "" : "default ", y > 0 ? y : 0);
}

// (s67 drive) the Drive gauge's values (game thread -> render thread, under g_lock); at = vk_frame_counter() of the last
// call (30 presents without one = hidden)
typedef struct { int state, bars, fill, max; uint64_t at; } DrIn;
static DrIn g_dr_in;

void sora_hud_drive(int state, int bars, int fill, int max_bars) {
    pthread_mutex_lock(&g_lock);
    g_dr_in.max = max_bars < 0 ? 0 : max_bars > 9 ? 9 : max_bars;
    g_dr_in.bars = bars < 0 ? 0 : bars > g_dr_in.max ? g_dr_in.max : bars;
    g_dr_in.fill = fill < 0 ? 0 : fill > 100 ? 100 : fill;
    g_dr_in.state = state;
    g_dr_in.at = vk_frame_counter();
    pthread_mutex_unlock(&g_lock);
}

static struct { float life, max; } g_en_in;  // the enemy gauge (sora_hud_enemy): max <= 0 = hidden

void sora_hud_enemy(float life, float life_max) {
    pthread_mutex_lock(&g_lock);
    g_en_in.max = life_max > 0 ? life_max : 0;
    g_en_in.life = life < 0 ? 0 : life > g_en_in.max ? g_en_in.max : life;
    pthread_mutex_unlock(&g_lock);
}

// (s92 hud) the enemy gauge's top-left place: the bar's LEFT end (its 2 u cap) x HUD units from the game image's left edge,
// its centre line y units from the top; on = false = the old place (right end from the top-right corner). Any thread
// (live "set enemyhud / enx / eny"); the DLL looks it up by name (exes before s92 lack it).
static struct { bool on; float x, y; } g_en_place;

void sora_hud_enemy_place(float x, float y, bool on) {
    pthread_mutex_lock(&g_lock);
    g_en_place.on = on, g_en_place.x = x, g_en_place.y = y;
    pthread_mutex_unlock(&g_lock);
    LOG("[sora-hud] enemy gauge place: %s\n", on ? "top-left" : "top-right (old)");
}

// (s71 drive) KH2's Drive screen dim (re:0x40D000): a black full-screen quad under the HUD, alpha 0..1 from the DLL (its
// timing lives there, hot reloadable); 3 presents without a call = off
static struct { float a; uint64_t at; } g_dim_in;

void sora_hud_dim(float a) {
    pthread_mutex_lock(&g_lock);
    g_dim_in.a = a < 0 ? 0 : a > 1 ? 1 : a;
    g_dim_in.at = vk_frame_counter();
    pthread_mutex_unlock(&g_lock);
}

void sora_hud_off(void) {
    pthread_mutex_lock(&g_lock);
    g_in.on = false;
    g_en_in.max = 0;
    g_menu.on = false;
    g_lk_in.kind = 0;
    g_dream_in.on = false;  // (s54 dream)
    // (s55 HUD: NOT the level-up panels: sora.c calls this every frame while BotW is paused / Link is hidden; the panels
    // just hold - their clock runs only while the gauge is up - and go on afterwards. sora_hud_levelup(NULL) drops them.)
    pthread_mutex_unlock(&g_lock);
}

// ------------------------------------------------------------------ the bake
typedef struct { uint32_t w, h, off, reserved; } HImg;
static uint8_t *g_bake;
static const HImg *g_img;
static uint32_t g_nimg;
static int g_state;  // 0 not loaded, 1 ready, -1 off / failed

static bool bake_load(void) {
    const char *p = getenv("SAVAGEWIND_SORA_HUD");
    if (p && !strcmp(p, "0")) { LOG("[sora-hud] off (SAVAGEWIND_SORA_HUD=0)\n"); return false; }
    if (!p || !*p || !strcmp(p, "1")) p = "D:/SoraBotW/data/hud/hud.bin";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[sora-hud] no %s: no HUD\n", p); return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_bake = n > 48 ? malloc((size_t)n) : NULL;
    bool ok = g_bake && fread(g_bake, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    if (!ok || memcmp(g_bake, "SKHD", 4)) { LOG("[sora-hud] bad %s\n", p); return false; }
    g_nimg = ((const uint32_t *)g_bake)[2];
    g_img = (const HImg *)(g_bake + 16);
    if (g_nimg < 2) { LOG("[sora-hud] %s: %u images, need 2\n", p, g_nimg); return false; }
    for (uint32_t i = 0; i < g_nimg; i++)
        if ((uint64_t)g_img[i].off + (uint64_t)g_img[i].w * g_img[i].h * 4 > (uint64_t)n) { LOG("[sora-hud] %s truncated\n", p); return false; }
    LOG("[sora-hud] %s: sheet %ux%u, face %ux%u\n", p, g_img[0].w, g_img[0].h, g_img[1].w, g_img[1].h);
    return true;
}

// ------------------------------------------------------------------ guest portraits (s111 roxhud, s116 mkface)
// roxface.bin / mkface.bin 'SKRF' v1 {magic, ver, w, h} + RGBA8: KH2's menu/shopface/p_ex110 (Roxas, tools_s111/roxface_bake.py)
// and KH2's Jiminy journal menu/jmface_ex18 (King Mickey, king outfit = P_EX200, tools_s116/mkface_bake.py). Drawn in place of
// Sora's face (texture 1) as textures 11 / 12 while sora_hud_face_id(1 / 2) is set; a missing file = Sora's face stays.
typedef struct { uint8_t *buf; uint32_t w, h; int state; } HFace;  // state 0 not tried, 1 ready, -1 missing / bad (logged once)
static HFace g_rf, g_mf;
static volatile int g_face_who;  // 0 Sora, 1 Roxas, 2 Mickey
static bool face_load(HFace *fc, const char *env, const char *def, const char *who) {
    const char *p = getenv(env);
    if (!p || !*p) p = def;
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[sora-hud] no %s: %s keeps Sora's face on the HUD\n", p, who); return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = n > 16 ? malloc((size_t)n) : NULL;
    bool ok = b && fread(b, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    uint32_t w = ok ? ((uint32_t *)b)[2] : 0, h = ok ? ((uint32_t *)b)[3] : 0;
    if (!ok || memcmp(b, "SKRF", 4) || !w || !h || w > 2048 || h > 2048 || 16 + 4ull * w * h > (uint64_t)n) {
        LOG("[sora-hud] bad %s: %s keeps Sora's face on the HUD\n", p, who);
        free(b);
        return false;
    }
    fc->buf = b;  // (the texture upload reads buf + 16)
    fc->w = w, fc->h = h;
    LOG("[sora-hud] %s: %s' face %ux%u\n", p, who, w, h);
    return true;
}
// called on change by the mod (sora.c hud_face_sync): 0 Sora, 1 Roxas, 2 Mickey (s116)
void sora_hud_face_id(int who) {
    static const char *const nm[3] = {"Sora", "Roxas", "Mickey"};
    if (who < 0 || who > 2) who = 0;
    if (g_face_who != who) LOG("[sora-hud] portrait: %s\n", nm[who]);
    g_face_who = who;
}
// (s111 roxhud: the old entry, kept for a DLL from before s116) 1 = Roxas, 0 = Sora
void sora_hud_face(int rox) { sora_hud_face_id(rox ? 1 : 0); }

// ------------------------------------------------------------------ (s139 sumplate) the summon portraits (sumface.bin, 'SKSF' v1)
// {magic, ver, n, tile px, atlas w, h, cols} (28 B), n x {i32 l, t, r, b} (the face part's rect in PS2 px around the orb plate's
// anchor: KH2's face Seqd, anim group 3), then the RGBA8 atlas (tiles in a grid; tools_s139/sumface_bake.py). Faces: 1 Chicken
// Little, 2 Stitch, 3 Genie, 4 Peter Pan, 5 Tinker Bell's P_EX340 image (a spare: it is a sheet of small Sora heads, not her portrait).
// Texture 14. A missing / bad file = no orb plate at all (an empty plate would look wrong). The orb plate is drawn by drive_draw.
typedef struct { uint8_t *buf; uint32_t n, tile, w, h, cols, off; int32_t rect[8][4]; int state; } SFace;
static SFace g_sf;
static volatile int g_sf_id;  // 0 none, 1.. = the face to show on the orb plate
static bool sface_load(void) {
    const char *p = getenv("SAVAGEWIND_SORA_HUD_SUMFACE");
    if (!p || !*p) p = "D:/SoraBotW/data/hud/sumface.bin";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[sora-hud] no %s: no summon orb plate / portrait\n", p); return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = n > 28 ? malloc((size_t)n) : NULL;
    bool ok = b && fread(b, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    const uint32_t *h = (const uint32_t *)b;
    uint64_t off = ok ? 28ull + 16ull * h[2] : 0;
    if (!ok || memcmp(b, "SKSF", 4) || h[1] != 1 || !h[2] || h[2] > 8 || !h[3] || !h[4] || !h[5] || h[4] > 4096 || h[5] > 4096 || !h[6] ||
        off + 4ull * h[4] * h[5] > (uint64_t)n) {
        LOG("[sora-hud] bad %s: no summon orb plate / portrait\n", p);
        free(b);
        return false;
    }
    g_sf.buf = b, g_sf.n = h[2], g_sf.tile = h[3], g_sf.w = h[4], g_sf.h = h[5], g_sf.cols = h[6], g_sf.off = (uint32_t)off;
    memcpy(g_sf.rect, b + 28, 16 * h[2]);
    LOG("[sora-hud] %s: %u summon faces, atlas %ux%u\n", p, g_sf.n, g_sf.w, g_sf.h);
    return true;
}
// called on change by the mod (sora_summon_menu.h smn_face_sync): 0 none (the plate fades out), 1 Chicken Little, 2 Stitch, 3 Genie, 4 Peter Pan
void sora_hud_summon_face(int id) {
    static const char *const nm[6] = {"none", "Chicken Little", "Stitch", "Genie", "Peter Pan", "Tinker Bell (spare)"};
    if (id < 0 || id > 5) id = 0;
    if (g_sf_id != id) LOG("[sora-hud] summon portrait: %s\n", nm[id]);
    g_sf_id = id;
}

// ------------------------------------------------------------------ the command menu bake (cmd.bin, 'SKCM' v1)
// header {u32 magic, version, n_img, n_groups}; img[n_img] HImg; label[8]; group[n_groups]; the parts (group off =
// relative to the parts block); the images (RGBA8). Written by tools/hudbake CmdBake(), same layout as read here.
typedef struct { float l, t, r, b; uint32_t col[4]; float x0, y0, x1, y1; uint32_t blend; } CPart;  // 52 bytes
typedef struct { uint16_t x, y, w, h; } CLabel;
typedef struct { uint16_t id, n; uint32_t off; } CGrp;
#define N_LABEL 8
#define SORA_CMD_MAX_LABELS 128  // (s137 review M4: was 64; Stitch 51..56 + the sumdata summons 57..74 need more. 128 = the int8_t top-label / pad-label limit)
static uint8_t *g_cmd;
static const HImg *g_cimg;
static const CLabel *g_clabel;
static const CGrp *g_cgrp;
static const uint8_t *g_cparts;
static uint32_t g_ncimg, g_ncgrp, g_cparts_n;
static int g_nclabel = N_LABEL;  // labels in g_clabel (the SKLX trailer's n when present)
static int g_cmd_state;  // 0 not tried, 1 ready, -1 off / missing / bad (logged once; the gauge is unaffected)

static bool cmd_load(void) {
    const char *p = getenv("SAVAGEWIND_SORA_CMD");
    if (p && !strcmp(p, "0")) { LOG("[sora-hud] command menu off (SAVAGEWIND_SORA_CMD=0)\n"); return false; }
    if (!p || !*p || !strcmp(p, "1")) p = "D:/SoraBotW/data/hud/cmd.bin";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[sora-hud] no %s: no command menu\n", p); return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_cmd = n > 16 + 2 * 16 + N_LABEL * 8 ? malloc((size_t)n) : NULL;
    bool ok = g_cmd && fread(g_cmd, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    if (!ok || memcmp(g_cmd, "SKCM", 4) || ((const uint32_t *)g_cmd)[1] != 1) { LOG("[sora-hud] bad %s: no command menu\n", p); return false; }
    g_ncimg = ((const uint32_t *)g_cmd)[2], g_ncgrp = ((const uint32_t *)g_cmd)[3];
    uint64_t hdr = 16 + (uint64_t)g_ncimg * 16 + N_LABEL * 8 + (uint64_t)g_ncgrp * 8;
    if (g_ncimg < 2 || g_ncgrp > 256 || hdr > (uint64_t)n) { LOG("[sora-hud] %s: %u images, %u groups: no command menu\n", p, g_ncimg, g_ncgrp); return false; }
    g_cimg = (const HImg *)(g_cmd + 16);
    g_clabel = (const CLabel *)(g_cmd + 16 + g_ncimg * 16);
    g_cgrp = (const CGrp *)(g_clabel + N_LABEL);
    g_cparts = (const uint8_t *)(g_cgrp + g_ncgrp);
    uint64_t parts_end = (uint64_t)n;  // the parts run up to the first image
    for (uint32_t i = 0; i < g_ncimg; i++) {
        if ((uint64_t)g_cimg[i].off + (uint64_t)g_cimg[i].w * g_cimg[i].h * 4 > (uint64_t)n || g_cimg[i].off < hdr || !g_cimg[i].w || !g_cimg[i].h) { LOG("[sora-hud] %s truncated: no command menu\n", p); return false; }
        if (g_cimg[i].off < parts_end) parts_end = g_cimg[i].off;
    }
    g_cparts_n = (uint32_t)((parts_end - hdr) / sizeof(CPart));
    for (uint32_t i = 0; i < g_ncgrp; i++)
        if (g_cgrp[i].off % sizeof(CPart) || g_cgrp[i].off / sizeof(CPart) + g_cgrp[i].n > g_cparts_n) { LOG("[sora-hud] %s: group %u out of range: no command menu\n", p, g_cgrp[i].id); return false; }
    // (s68) HImg[1].reserved = file offset of {'SKLX', u32 n, label[n]} = all labels (8 Valor Form, 9 Revert, ..); else 8
    g_nclabel = N_LABEL;
    uint32_t lx = g_cimg[1].reserved;
    if (lx && (uint64_t)lx + 8 <= (uint64_t)n && !memcmp(g_cmd + lx, "SKLX", 4)) {
        uint32_t ln = *(const uint32_t *)(g_cmd + lx + 4);
        if (ln >= N_LABEL && ln <= SORA_CMD_MAX_LABELS && (uint64_t)lx + 8 + ln * 8ull <= (uint64_t)n) g_clabel = (const CLabel *)(g_cmd + lx + 8), g_nclabel = (int)ln;
        else LOG("[sora-hud] %s: bad SKLX trailer (n %u): 8 labels\n", p, ln);
    }
    for (int i = 0; i < g_nclabel; i++)
        if ((uint32_t)g_clabel[i].x + g_clabel[i].w > g_cimg[1].w || (uint32_t)g_clabel[i].y + g_clabel[i].h > g_cimg[1].h) { LOG("[sora-hud] %s: label %d out of range: no command menu\n", p, i); return false; }
    LOG("[sora-hud] %s: sheet %ux%u, %d labels %ux%u, %u groups, %u parts\n", p, g_cimg[0].w, g_cimg[0].h, g_nclabel, g_cimg[1].w, g_cimg[1].h, g_ncgrp, g_cparts_n);
    return true;
}

// ------------------------------------------------------------------ (s54 dream) the picker's labels (dream.bin, 'SKDM' v1)
// header {u32 magic, version 1, w, h, n_label}; label[n] {u16 x, y, w, h}; the atlas RGBA8 w x h (tools/hudbake DreamBake)
static uint8_t *g_dm;
static const CLabel *g_dlabel;
static uint32_t g_dm_w, g_dm_h, g_dm_n, g_dm_off;
static int g_dm_state;  // 0 not tried, 1 ready, -1 off / missing / bad (logged once; nothing else is affected)

static bool dream_load(void) {
    const char *p = getenv("SAVAGEWIND_SORA_DREAM");
    if (p && !strcmp(p, "0")) { LOG("[sora-hud] dream picker off (SAVAGEWIND_SORA_DREAM=0)\n"); return false; }
    if (!p || !*p || !strcmp(p, "1")) p = "D:/SoraBotW/data/hud/dream.bin";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[sora-hud] no %s: no dream picker\n", p); return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_dm = n > 20 ? malloc((size_t)n) : NULL;
    bool ok = g_dm && fread(g_dm, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    if (!ok || memcmp(g_dm, "SKDM", 4) || ((const uint32_t *)g_dm)[1] != 1) { LOG("[sora-hud] bad %s: no dream picker\n", p); return false; }
    g_dm_w = ((const uint32_t *)g_dm)[2], g_dm_h = ((const uint32_t *)g_dm)[3], g_dm_n = ((const uint32_t *)g_dm)[4];
    g_dm_off = 20 + g_dm_n * 8;
    if (!g_dm_w || !g_dm_h || g_dm_n > 1024 || (uint64_t)g_dm_off + (uint64_t)g_dm_w * g_dm_h * 4 > (uint64_t)n) { LOG("[sora-hud] %s truncated: no dream picker\n", p); return false; }
    g_dlabel = (const CLabel *)(g_dm + 20);
    for (uint32_t i = 0; i < g_dm_n; i++)
        if ((uint32_t)g_dlabel[i].x + g_dlabel[i].w > g_dm_w || (uint32_t)g_dlabel[i].y + g_dlabel[i].h > g_dm_h) { LOG("[sora-hud] %s: label %u out of range: no dream picker\n", p, i); return false; }
    LOG("[sora-hud] %s: atlas %ux%u, %u labels\n", p, g_dm_w, g_dm_h, g_dm_n);
    return true;
}

// ------------------------------------------------------------------ (s78 camp) the pause menu's art (camp.bin, 'SKCP' v1)
// header {u32 magic, version 1, n = 2}; n x {w, h, logical_w, logical_h, off (from the file start)}; RGBA8 images
// (tools/hudbake `camp`): image 0 = the camp sheet (logical 256 x 256), image 1 = the sys font (512 x 256)
typedef struct { uint32_t w, h, lw, lh, off; } CpImg;
static uint8_t *g_cp;
static const CpImg *g_cpimg;
static int g_cp_state;  // 0 not tried, 1 ready, -1 off / missing / bad (logged once; the quads are then ignored)

static bool camp_load(void) {
    const char *p = getenv("SAVAGEWIND_SORA_CAMP");
    if (p && !strcmp(p, "0")) { LOG("[sora-hud] camp off (SAVAGEWIND_SORA_CAMP=0)\n"); return false; }
    if (!p || !*p || !strcmp(p, "1")) p = "D:/SoraBotW/data/hud/camp.bin";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[sora-hud] no %s: no camp\n", p); return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_cp = n > 12 + 2 * (long)sizeof(CpImg) ? malloc((size_t)n) : NULL;
    bool ok = g_cp && fread(g_cp, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    if (!ok || memcmp(g_cp, "SKCP", 4) || ((const uint32_t *)g_cp)[1] != 1 || ((const uint32_t *)g_cp)[2] != 2) { LOG("[sora-hud] bad %s: no camp\n", p); return false; }
    g_cpimg = (const CpImg *)(g_cp + 12);
    for (int i = 0; i < 2; i++) {
        const CpImg *c = &g_cpimg[i];
        if (!c->w || !c->h || !c->lw || !c->lh || (uint64_t)c->off + (uint64_t)c->w * c->h * 4 > (uint64_t)n) { LOG("[sora-hud] %s: image %d truncated: no camp\n", p, i); return false; }
    }
    LOG("[sora-hud] %s: sheet %ux%u (%ux%u), font %ux%u (%ux%u)\n", p, g_cpimg[0].w, g_cpimg[0].h, g_cpimg[0].lw, g_cpimg[0].lh,
        g_cpimg[1].w, g_cpimg[1].h, g_cpimg[1].lw, g_cpimg[1].lh);
    return true;
}

// ------------------------------------------------------------------ the Keyblade pictures (s120 itempic, itempic.bin 'SKIP' v1)
// KH2's Equipment page item pictures (menu right panel), one RGBA atlas: header {magic, ver 1, w, h, n, pix_off}, n x {u16 x, y,
// w, h, lw, lh} (the DLL's sora_camp_pic.h knows the same grid), RGBA8 at pix_off. Drawn as SoraHudQuad.tex == 3 (u / v in atlas
// texels) through camp_draw = texture 13. Written by tools_s120/itempic_bake.py.
//   SAVAGEWIND_SORA_ITEMPIC=<path>|0   itempic.bin (default D:/SoraBotW/data/hud/itempic.bin); missing = tex 3 quads skipped (logged once)
static uint8_t *g_ip;
static uint32_t g_ip_w, g_ip_h, g_ip_off;
static int g_ip_state;  // 0 not tried, 1 ready, -1 off / missing / bad
// (s120) the DLL asks before it emits tex 3 quads (an old exe has no such export = no pictures; the camp sheet would show instead)
bool sora_hud_itempic_ready(void) { return g_ip_state == 1; }
static bool itempic_load(void) {
    const char *p = getenv("SAVAGEWIND_SORA_ITEMPIC");
    if (p && !strcmp(p, "0")) { LOG("[sora-hud] itempic off (SAVAGEWIND_SORA_ITEMPIC=0)\n"); return false; }
    if (!p || !*p || !strcmp(p, "1")) p = "D:/SoraBotW/data/hud/itempic.bin";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[sora-hud] no %s: Keyblade pictures skipped\n", p); return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_ip = n > 24 ? malloc((size_t)n) : NULL;
    bool ok = g_ip && fread(g_ip, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    const uint32_t *h = (const uint32_t *)g_ip;
    if (!ok || memcmp(g_ip, "SKIP", 4) || h[1] != 1 || !h[2] || !h[3] || h[2] > 4096 || h[3] > 4096 || h[4] > 256 ||
        h[5] < 24 + (uint64_t)h[4] * 12 || (uint64_t)h[5] + 4ull * h[2] * h[3] > (uint64_t)n) {
        LOG("[sora-hud] bad %s: Keyblade pictures skipped\n", p);
        free(g_ip), g_ip = NULL;
        return false;
    }
    g_ip_w = h[2], g_ip_h = h[3], g_ip_off = h[5];
    LOG("[sora-hud] %s: Keyblade pictures, atlas %ux%u, %u cells\n", p, g_ip_w, g_ip_h, h[4]);
    return true;
}

// ------------------------------------------------------------------ the lock-on icons bake (lock.bin, 'SKLK' v2)
// header {magic, version 2, w, h, ps2_w, n_frames} + 2 icons {show_first, loop_start, release_first, end} (absolute
// frames: show_first..release_first-1 looping from loop_start, then the hide release_first..end-1); n_frames x
// {u32 off (in parts), u32 n}; the parts (LPart: 4 corners, PS2 px around the target, already rotated / scaled); the HD
// sheet RGBA8 (w x h) at the end. tools/hudbake LockBake(): icon 0 = KH2's tt0field 'fiel' group 5 / 4 (the yellow
// AUTO target), icon 1 = group 27 / 26 (the blue-white R1 LOCK, inner arcs turning). blend 1 = additive (sg 48's glow).
typedef struct { uint32_t magic, ver, w, h, ps2_w, n_frames; struct { uint32_t show, loop, rel, end; } icon[2]; } LHdr;
typedef struct { float l, t, r, b; uint32_t col[4]; float x[4], y[4]; uint32_t blend; } LPart;  // 68 bytes, corners TL TR BR BL
static uint8_t *g_lk;
static const LHdr *g_lkh;
static const uint32_t *g_lkfr;  // n_frames x {off, n}
static const LPart *g_lkparts;
static uint32_t g_lk_sheet;     // byte offset of the sheet
static int g_lk_state;          // 0 not tried, 1 ready, -1 off / missing / bad

static bool lock_load(void) {
    const char *p = getenv("SAVAGEWIND_SORA_LOCK_ICON");
    if (p && !strcmp(p, "0")) { LOG("[sora-hud] lock-on icon off (SAVAGEWIND_SORA_LOCK_ICON=0)\n"); return false; }
    if (!p || !*p || !strcmp(p, "1")) p = "D:/SoraBotW/data/hud/lock.bin";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[sora-hud] no %s: no lock-on icon\n", p); return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_lk = n > (long)sizeof(LHdr) ? malloc((size_t)n) : NULL;
    bool ok = g_lk && fread(g_lk, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    g_lkh = (const LHdr *)g_lk;
    if (!ok || memcmp(g_lk, "SKLK", 4) || g_lkh->ver != 2 || !g_lkh->w || !g_lkh->h || !g_lkh->ps2_w || !g_lkh->n_frames) { LOG("[sora-hud] bad %s (need v2): no lock-on icon\n", p); return false; }
    for (int i = 0; i < 2; i++) {
        uint32_t s = g_lkh->icon[i].show, l = g_lkh->icon[i].loop, r = g_lkh->icon[i].rel, e = g_lkh->icon[i].end;
        if (!(s <= l && l < r && r < e && e <= g_lkh->n_frames)) { LOG("[sora-hud] bad %s: icon %d frames: no lock-on icon\n", p, i); return false; }
    }
    uint64_t sheet = (uint64_t)g_lkh->w * g_lkh->h * 4, fr = sizeof(LHdr) + 8ull * g_lkh->n_frames;
    if (fr + sheet > (uint64_t)n) { LOG("[sora-hud] %s truncated: no lock-on icon\n", p); return false; }
    g_lk_sheet = (uint32_t)((uint64_t)n - sheet);
    g_lkfr = (const uint32_t *)(g_lk + sizeof(LHdr));
    g_lkparts = (const LPart *)(g_lk + fr);
    uint64_t nparts = (g_lk_sheet - fr) / sizeof(LPart);
    for (uint32_t i = 0; i < g_lkh->n_frames; i++)
        if ((uint64_t)g_lkfr[2 * i] + g_lkfr[2 * i + 1] > nparts) { LOG("[sora-hud] %s: frame %u out of range: no lock-on icon\n", p, i); return false; }
    LOG("[sora-hud] %s: sheet %ux%u (PS2 %u), %u frames, %llu parts; auto %u/%u/%u..%u, lock %u/%u/%u..%u\n", p, g_lkh->w, g_lkh->h,
        g_lkh->ps2_w, g_lkh->n_frames, (unsigned long long)nparts, g_lkh->icon[0].show, g_lkh->icon[0].loop, g_lkh->icon[0].rel,
        g_lkh->icon[0].end, g_lkh->icon[1].show, g_lkh->icon[1].loop, g_lkh->icon[1].rel, g_lkh->icon[1].end);
    return true;
}

// ------------------------------------------------------------------ (s55 HUD) the level-up panel bake (lvup.bin, 'SKLV' v1)
// header {magic, version 1, n_grp, n_frames, atlas w, h, n_label, atlas px per frame px}; grp[n_grp] (16 B); frame[n]
// {part off, n parts, anchor x, y, anchor colour} (20 B); label[n] (24 B); the parts (LPart, KH2 frame px); the atlas.
// tools/hudbake LvupBake() writes it (recon/wip_s55_hud/hudbake until merged).
typedef struct { uint32_t magic, ver, n_grp, n_frames, w, h, n_label, as; } VHdr;
typedef struct { uint16_t id, pad; int16_t light_x, text_x, text_y; uint16_t n; uint32_t first; } VGrp;
typedef struct { uint32_t off, n; int32_t ax, ay; uint32_t col; } VFrame;
typedef struct { uint16_t x, y, w, h; float ox, oy, adv; uint32_t key; } VLabel;
static uint8_t *g_lv;
static const VHdr *g_lvh;
static const VGrp *g_lvg;
static const VFrame *g_lvf;
static const VLabel *g_lvl;
static const LPart *g_lvp;
static uint32_t g_lv_atlas;  // byte offset of the atlas
static int g_lv_state;       // 0 not tried, 1 ready, -1 off / missing / bad (logged once; nothing else is affected)

static bool lvup_load(void) {
    const char *p = getenv("SAVAGEWIND_SORA_LVUP");
    if (p && !strcmp(p, "0")) { LOG("[sora-hud] level-up panel off (SAVAGEWIND_SORA_LVUP=0)\n"); return false; }
    if (!p || !*p || !strcmp(p, "1")) p = "D:/SoraBotW/data/hud/lvup.bin";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[sora-hud] no %s: no level-up panel\n", p); return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_lv = n > (long)sizeof(VHdr) ? malloc((size_t)n) : NULL;
    bool ok = g_lv && fread(g_lv, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    g_lvh = (const VHdr *)g_lv;
    if (!ok || memcmp(g_lv, "SKLV", 4) || g_lvh->ver != 1 || g_lvh->n_grp != 2 || !g_lvh->n_frames || !g_lvh->w || !g_lvh->h || !g_lvh->as || g_lvh->n_label > 4096) {
        LOG("[sora-hud] bad %s (need SKLV v1, 2 groups): no level-up panel\n", p);
        return false;
    }
    uint64_t parts0 = sizeof(VHdr) + 16ull * g_lvh->n_grp + 20ull * g_lvh->n_frames + 24ull * g_lvh->n_label, atlas = 4ull * g_lvh->w * g_lvh->h;
    if (parts0 + atlas > (uint64_t)n) { LOG("[sora-hud] %s truncated: no level-up panel\n", p); return false; }
    g_lv_atlas = (uint32_t)((uint64_t)n - atlas);
    g_lvg = (const VGrp *)(g_lv + sizeof(VHdr));
    g_lvf = (const VFrame *)(g_lvg + g_lvh->n_grp);
    g_lvl = (const VLabel *)(g_lvf + g_lvh->n_frames);
    g_lvp = (const LPart *)(g_lv + parts0);
    uint64_t nparts = (g_lv_atlas - parts0) / sizeof(LPart);
    for (uint32_t i = 0; i < g_lvh->n_grp; i++)
        if (!g_lvg[i].n || (uint64_t)g_lvg[i].first + g_lvg[i].n > g_lvh->n_frames) { LOG("[sora-hud] %s: group %u frames out of range: no level-up panel\n", p, i); return false; }
    for (uint32_t i = 0; i < g_lvh->n_frames; i++)
        if ((uint64_t)g_lvf[i].off + g_lvf[i].n > nparts) { LOG("[sora-hud] %s: frame %u out of range: no level-up panel\n", p, i); return false; }
    for (uint32_t i = 0; i < g_lvh->n_label; i++)
        if ((uint32_t)g_lvl[i].x + g_lvl[i].w > g_lvh->w || (uint32_t)g_lvl[i].y + g_lvl[i].h > g_lvh->h) { LOG("[sora-hud] %s: label %u out of range: no level-up panel\n", p, i); return false; }
    LOG("[sora-hud] %s: groups %u (%u frames) / %u (%u frames), %llu parts, %u labels, atlas %ux%u\n", p, g_lvg[0].id, g_lvg[0].n,
        g_lvg[1].id, g_lvg[1].n, (unsigned long long)nparts, g_lvh->n_label, g_lvh->w, g_lvh->h);
    return true;
}

// ------------------------------------------------------------------ (s57 party) the party gauges bake (party.bin, 'SKPT' v1)
// header {magic, version 1, n_grp, n_frames, n_face, face w, h, face PS2 w; orb x, y, plate dx, dy, pitch, 0} (56 B);
// grp[n] {u16 id (KH2 anim group; 0x100 + n = face group n), u16 tex (0 gaug, 1 face), u16 n_frames, u16 loop (0xFFFF =
// one-shot), u32 first, u32 0}; frame[n] {u32 off, n, f32 anchor x, y, u32 colour}; the parts (LPart: plate groups
// relative to Sora's orb, children and faces to the plate's anchor); the faces RGBA8 (0 Donald, 1 Goofy). tools/hudbake
// PartyBake() writes it (recon/wip_s57_hudparty/hudbake until merged).
typedef struct { uint32_t magic, ver, n_grp, n_frames, n_face, face_w, face_h, face_ps2; int32_t orb_x, orb_y, slot_dx, slot_dy, pitch, reserved; } PtHdr;
typedef struct { uint16_t id, tex, n, loop; uint32_t first, reserved; } PtGrp;
typedef struct { uint32_t off, n; float ax, ay; uint32_t col; } PtFrame;
static uint8_t *g_pt;
static const PtHdr *g_pth;
static const PtGrp *g_ptg;
static const PtFrame *g_ptf;
static const LPart *g_ptp;
static uint32_t g_pt_face;  // byte offset of the first face
static int g_pt_state;      // 0 not tried, 1 ready, -1 off / missing / bad (logged once; nothing else is affected)
// the groups the draw uses (indices into g_ptg, found once by id): KH2 ids 64-74 and the face groups
enum { PG_TRACK, PG_HP, PG_MP, PG_LOW, PG_HIT, PG_INTRO, PG_NORMAL, PG_SLIDE_UP, PG_SLIDE_BACK, PG_F_LOW, PG_F_HURT, PG_F_KO, PG_F_NORMAL, PG_N };
static const uint16_t PG_ID[PG_N] = {64, 65, 66, 67, 70, 71, 72, 73, 74, 0x100, 0x101, 0x102, 0x103};
static int g_pg[PG_N];

static bool party_load(void) {
    const char *p = getenv("SAVAGEWIND_SORA_PARTY");
    if (p && !strcmp(p, "0")) { LOG("[sora-hud] party gauges off (SAVAGEWIND_SORA_PARTY=0)\n"); return false; }
    if (!p || !*p || !strcmp(p, "1")) p = "D:/SoraBotW/data/hud/party.bin";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[sora-hud] no %s: no party gauges\n", p); return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_pt = n > (long)sizeof(PtHdr) ? malloc((size_t)n) : NULL;
    bool ok = g_pt && fread(g_pt, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    g_pth = (const PtHdr *)g_pt;
    if (!ok || memcmp(g_pt, "SKPT", 4) || g_pth->ver != 1 || !g_pth->n_grp || g_pth->n_grp > 64 || !g_pth->n_frames || g_pth->n_face < 2 ||
        !g_pth->face_w || !g_pth->face_h || !g_pth->face_ps2) {
        LOG("[sora-hud] bad %s (need SKPT v1, 2 faces): no party gauges\n", p);
        return false;
    }
    uint64_t parts0 = sizeof(PtHdr) + 16ull * g_pth->n_grp + 20ull * g_pth->n_frames, faces = 4ull * g_pth->face_w * g_pth->face_h * g_pth->n_face;
    if (parts0 + faces > (uint64_t)n) { LOG("[sora-hud] %s truncated: no party gauges\n", p); return false; }
    g_pt_face = (uint32_t)((uint64_t)n - faces);
    g_ptg = (const PtGrp *)(g_pt + sizeof(PtHdr));
    g_ptf = (const PtFrame *)(g_ptg + g_pth->n_grp);
    g_ptp = (const LPart *)(g_pt + parts0);
    uint64_t nparts = (g_pt_face - parts0) / sizeof(LPart);
    for (uint32_t i = 0; i < g_pth->n_grp; i++)
        if (!g_ptg[i].n || (uint64_t)g_ptg[i].first + g_ptg[i].n > g_pth->n_frames || (g_ptg[i].loop != 0xFFFF && g_ptg[i].loop >= g_ptg[i].n)) {
            LOG("[sora-hud] %s: group %u frames out of range: no party gauges\n", p, g_ptg[i].id);
            return false;
        }
    for (uint32_t i = 0; i < g_pth->n_frames; i++)
        if ((uint64_t)g_ptf[i].off + g_ptf[i].n > nparts) { LOG("[sora-hud] %s: frame %u out of range: no party gauges\n", p, i); return false; }
    for (int k = 0; k < PG_N; k++) {
        g_pg[k] = -1;
        for (uint32_t i = 0; i < g_pth->n_grp; i++)
            if (g_ptg[i].id == PG_ID[k]) g_pg[k] = (int)i;
        if (g_pg[k] < 0) { LOG("[sora-hud] %s: no group %#x: no party gauges\n", p, PG_ID[k]); return false; }
    }
    LOG("[sora-hud] %s: %u groups, %u frames, %llu parts, faces %ux%u; plate at orb + %d,%d, member 2 + %d\n", p, g_pth->n_grp,
        g_pth->n_frames, (unsigned long long)nparts, g_pth->face_w, g_pth->face_h, g_pth->slot_dx, g_pth->slot_dy, g_pth->pitch);
    return true;
}

// ------------------------------------------------------------------ (s67 drive) the Drive gauge bake (drive.bin, 'SKDR' v1)
// header {magic, version 1, n_grp, n_frames, i32 digit x, y (the plate's anchor to the digit: ag 21 LightPositionX,
// TextPositionY), 0, 0} (32 B); grp / frame / parts = party.bin's (PtGrp, PtFrame, LPart; plate groups relative to Sora's
// orb, the children to the plate's anchor), all on the gauge sheet. tools/hudbake DriveBake() writes it.
typedef struct { uint32_t magic, ver, n_grp, n_frames; int32_t digit_x, digit_y; uint32_t reserved[2]; } DrHdr;
static uint8_t *g_dr;
static const DrHdr *g_drh;
static const PtGrp *g_drg;
static const PtFrame *g_drf;
static const LPart *g_drp;
static int g_dr_state;  // 0 not tried, 1 ready, -1 off / missing / bad (logged once; nothing else is affected)
// the groups the draw uses (indices into g_drg, found once by id): KH2 ids 20-26, the digits 28-37 (0-9)
// (s137 smenu) KH2's summon state (drive state 3): plate 0x8D (141, the "SUMMON" tag), fill 0x8B (139), digits 144 + bars (table 0x71B0E0); these are optional
// (an s67 drive.bin has none: DG_BASE_N groups only, the summon then shows on the normal plate)
// (s139 sumplate) + KH2's orb plate (FUN_140180e00: 0x9A = 154 fade in -> 0x9B = 155 loop, 0x9C = 156 fade out), also optional
enum { DG_FULL, DG_INTRO, DG_BARUP, DG_PLATE, DG_FILL, DG_CHARGE, DG_DIGIT0, DG_BASE_N = DG_DIGIT0 + 10, DG_SFILL = DG_BASE_N, DG_SPLATE, DG_SDIGIT0, DG_SUM_N = DG_SDIGIT0 + 10, DG_ORB_IN = DG_SUM_N, DG_ORB, DG_ORB_OUT, DG_N };
static const uint16_t DG_ID[DG_N] = {20, 21, 22, 23, 25, 26, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 139, 141, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156};
static int g_dg[DG_N];
static bool g_dg_sum;  // the summon groups are in drive.bin
static bool g_dg_orb;  // (s139 sumplate) the orb plate groups 154 / 155 / 156 are in drive.bin

static bool drive_load(void) {
    const char *p = getenv("SAVAGEWIND_SORA_DRIVE");
    if (p && !strcmp(p, "0")) { LOG("[sora-hud] drive gauge off (SAVAGEWIND_SORA_DRIVE=0)\n"); return false; }
    if (!p || !*p || !strcmp(p, "1")) p = "D:/SoraBotW/data/hud/drive.bin";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[sora-hud] no %s: no drive gauge\n", p); return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_dr = n > (long)sizeof(DrHdr) ? malloc((size_t)n) : NULL;
    bool ok = g_dr && fread(g_dr, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    g_drh = (const DrHdr *)g_dr;
    if (!ok || memcmp(g_dr, "SKDR", 4) || g_drh->ver != 1 || !g_drh->n_grp || g_drh->n_grp > 64 || !g_drh->n_frames) {
        LOG("[sora-hud] bad %s (need SKDR v1): no drive gauge\n", p);
        return false;
    }
    uint64_t parts0 = sizeof(DrHdr) + 16ull * g_drh->n_grp + 20ull * g_drh->n_frames;
    if (parts0 > (uint64_t)n) { LOG("[sora-hud] %s truncated: no drive gauge\n", p); return false; }
    g_drg = (const PtGrp *)(g_dr + sizeof(DrHdr));
    g_drf = (const PtFrame *)(g_drg + g_drh->n_grp);
    g_drp = (const LPart *)(g_dr + parts0);
    uint64_t nparts = ((uint64_t)n - parts0) / sizeof(LPart);
    for (uint32_t i = 0; i < g_drh->n_grp; i++)
        if (!g_drg[i].n || (uint64_t)g_drg[i].first + g_drg[i].n > g_drh->n_frames || (g_drg[i].loop != 0xFFFF && g_drg[i].loop >= g_drg[i].n)) {
            LOG("[sora-hud] %s: group %u frames out of range: no drive gauge\n", p, g_drg[i].id);
            return false;
        }
    for (uint32_t i = 0; i < g_drh->n_frames; i++)
        if ((uint64_t)g_drf[i].off + g_drf[i].n > nparts) { LOG("[sora-hud] %s: frame %u out of range: no drive gauge\n", p, i); return false; }
    for (int k = 0; k < DG_N; k++) {
        g_dg[k] = -1;
        for (uint32_t i = 0; i < g_drh->n_grp; i++)
            if (g_drg[i].id == DG_ID[k]) g_dg[k] = (int)i;
        if (g_dg[k] < 0 && k < DG_BASE_N) { LOG("[sora-hud] %s: no group %u: no drive gauge\n", p, DG_ID[k]); return false; }
    }
    g_dg_sum = true;
    for (int k = DG_BASE_N; k < DG_SUM_N; k++) g_dg_sum &= g_dg[k] >= 0;
    g_dg_orb = g_dg[DG_ORB_IN] >= 0 && g_dg[DG_ORB] >= 0 && g_dg[DG_ORB_OUT] >= 0;
    LOG("[sora-hud] drive.bin: KH2 summon plate groups %s\n", g_dg_sum ? "present (state 2 draws them)" : "missing (a summon shows on the normal plate)");
    LOG("[sora-hud] %s: %u groups, %u frames, %llu parts; digit at the plate's anchor + %d,%d\n", p, g_drh->n_grp, g_drh->n_frames,
        (unsigned long long)nparts, g_drh->digit_x, g_drh->digit_y);
    return true;
}

static int lvup_label(uint32_t key) {  // the label of a key, -1 none (153 labels: a linear search is fine)
    for (uint32_t i = 0; i < g_lvh->n_label; i++)
        if (g_lvl[i].key == key) return (int)i;
    return -1;
}

// game thread -> render thread (under g_lock): which icon (0 none, 1 auto, 2 lock) and its target's centre (world)
void sora_hud_target(int kind, const float world[3]) {
    pthread_mutex_lock(&g_lock);
    g_lk_in.kind = world && kind >= 1 && kind <= 2 ? kind : 0;
    if (g_lk_in.kind) memcpy(g_lk_in.w, world, sizeof g_lk_in.w);
    pthread_mutex_unlock(&g_lock);
}
void sora_hud_lock(bool on, const float world[3]) { sora_hud_target(on ? 2 : 0, world); }

// ------------------------------------------------------------------ layout parameters (env, read once)
static struct {
    float scale, pos_x, pos_y, mp_x, mp_y, hp_px, mp_px;
    float menu_dx, menu_dy, menu_font;
    float dream_x, dream_y, dream_font;  // (s54 dream)
    float lv_dx, lv_dy;                  // (s55 HUD: the level-up panels' shift)
    float en_x, en_y, en_hp, en_min;
} P;

static void params_init(void) {
    static bool done;
    if (done) return;
    done = true;
    // (s57 party: pos_y 66 -> 177 = BotW's minimap top (124 u: its disc centre 75 u up, radius 49 u, measured at 720p and
    // 1080p) + the bar's 45 u below the ring centre + an 8 u gap; the user's wish: Sora's gauge above the minimap)
    P.scale = 1, P.pos_x = 60, P.pos_y = 177, P.mp_x = -40, P.mp_y = 31, P.hp_px = 2.0f, P.mp_px = 0.5f;
    // (s111 user: "move the command menu more in the bottom left corner") dx 0 -> -56 u (~180 px at 1440p: the frame's left edge
    // was 220 px in, now ~40 px like its bottom margin); live "set cmdx N" / "set cmdy N" via sora_hud_menu_pos (no restart)
    P.menu_dx = -56, P.menu_dy = -12, P.menu_font = 21;  // (s111: dy -12 = raised ~40 px, its bottom clipped at 0)  // (s40b: the user's vanilla screenshot; was 14)
    const char *e;
    if ((e = getenv("SAVAGEWIND_SORA_MENU_POS")) && *e) sscanf(e, "%f,%f", &P.menu_dx, &P.menu_dy);
    if ((e = getenv("SAVAGEWIND_SORA_MENU_FONT")) && *e) P.menu_font = strtof(e, NULL);
    if (P.menu_font <= 0) P.menu_font = 21;
    P.dream_x = 256, P.dream_y = 280, P.dream_font = 21;  // (s54 dream: GUESS placement, hudbake dreamtest = the preview)
    if ((e = getenv("SAVAGEWIND_SORA_DREAM_POS")) && *e) sscanf(e, "%f,%f", &P.dream_x, &P.dream_y);
    if ((e = getenv("SAVAGEWIND_SORA_DREAM_FONT")) && *e) P.dream_font = strtof(e, NULL);
    if (P.dream_font <= 0) P.dream_font = 21;
    P.lv_dx = 0, P.lv_dy = 0;  // (s55 HUD)
    if ((e = getenv("SAVAGEWIND_SORA_LVUP_POS")) && *e) sscanf(e, "%f,%f", &P.lv_dx, &P.lv_dy);
    if ((e = getenv("SAVAGEWIND_SORA_HUD_SCALE")) && *e) P.scale = strtof(e, NULL);
    if ((e = getenv("SAVAGEWIND_SORA_HUD_POS")) && *e) sscanf(e, "%f,%f", &P.pos_x, &P.pos_y);
    if ((e = getenv("SAVAGEWIND_SORA_HUD_MP_POS")) && *e) sscanf(e, "%f,%f", &P.mp_x, &P.mp_y);
    if ((e = getenv("SAVAGEWIND_SORA_HUD_HP_PX")) && *e) P.hp_px = strtof(e, NULL);
    if ((e = getenv("SAVAGEWIND_SORA_HUD_MP_PX")) && *e) P.mp_px = strtof(e, NULL);
    if (P.scale <= 0) P.scale = 1;
    P.en_x = 16, P.en_y = 31, P.en_hp = 300, P.en_min = 24;  // (KH2: anim group 38 at (498, 31) of the 512 frame)
    if ((e = getenv("SAVAGEWIND_SORA_HUD_EN_POS")) && *e) sscanf(e, "%f,%f", &P.en_x, &P.en_y);
    if ((e = getenv("SAVAGEWIND_SORA_HUD_EN_HP")) && *e) P.en_hp = strtof(e, NULL);
    if (P.en_hp <= 0) P.en_hp = 300;
}

// ------------------------------------------------------------------ the sprites (gaug sheet, PS2 pixel coordinates)
// uv rect l, t, r, b (r == l: a single texel column, stretched) and the sprite's tint (KH2 0xAABBGGRR, 0x80 = 1.0).
typedef struct { float l, t, r, b; uint32_t tint; } Spr;
static const Spr SP_RING_BLACK = {1, 60, 46, 105, 0x80808080};     // sprite 106
static const Spr SP_RING_GREEN = {50, 60, 95, 105, 0x80808080};    // 107
static const Spr SP_RING_RED = {99, 60, 144, 105, 0x80180880};     // 108 (grey arc tinted dark red: the lost-HP ghost)
static const Spr SP_BAR_BLACK = {45, 60, 46, 105, 0x80808080};     // 111 (the ring's cross section, 1 column)
static const Spr SP_BAR_GREEN = {95, 60, 95, 105, 0x80808080};     // 110
static const Spr SP_BAR_RED = {144, 60, 144, 105, 0x80180480};     // 112
static const Spr SP_END_LINE = {189, 49, 234, 52, 0x80808080};     // 109 (the 9 o'clock end cap, sg 91)
static const Spr SP_MP_BG = {183, 49, 185, 59, 0x80540B04};        // 120 (dark navy bar)
static const Spr SP_MP_CAP = {154, 37, 157, 47, 0x80808080};       // 121 (3 px between the bar and the label)
static const Spr SP_MP_LABEL = {157, 37, 181, 47, 0x80C8814D};     // 119 ("mp")
static const Spr SP_MP_FILL = {216, 40, 217, 46, 0x80808080};      // 122 (blue, 6 tall)
static const Spr SP_MP_BG_CHG = {183, 49, 185, 59, 0x80040454};    // 118 (MP Charge: dark red bar)
static const Spr SP_MP_LABEL_CHG = {157, 37, 181, 47, 0x80614CC8}; // 117
static const Spr SP_MP_FILL_CHG = {219, 40, 220, 46, 0x808714A8};  // 123 (pink)
static const Spr SP_FRAME_DISC = {119, 146, 156, 183, 0x40808080}; // 127 (quadrant, faint)
static const Spr SP_FRAME_RIM = {217, 0, 254, 37, 0x80808080};     // 128 (quadrant)
// the enemy gauge (sg 63-71): the fill and the green pip carry a corner gradient (L/T 8007E794, R/B 8001BD23, en_quad)
static const Spr SP_EN_FRAME = {183, 49, 185, 59, 0x80212121};     // 80 (dark bar, stretched)
static const Spr SP_EN_CAP = {155, 49, 157, 59, 0x80808080};       // 81 / 73 (2 px end caps)
static const Spr SP_EN_TAB = {155, 49, 180, 59, 0x80808080};       // 79 (the black housing of the pip row, sg 63)
static const Spr SP_EN_FILL = {2, 3, 2, 4, 0x80808080};            // 82 (green, 6 tall)
static const Spr SP_EN_GHOST = {1, 3, 2, 4, 0x80180880};           // 85 (dark red: the life just lost, sg 70)
static const Spr SP_EN_PIP_OFF = {209, 159, 222, 169, 0x80212121}; // 83 (an emptied bar, sg 67)
static const Spr SP_EN_PIP_ON = {209, 148, 222, 158, 0x80808080};  // 84 (a bar left, sg 68)
#define EN_LEN 199.0f  // one full bar (sg 69)

#define RING_OUT 45.0f
#define RING_IN 37.0f
#define RING_MID 41.0f
#define RING_ARC (0.75f * 6.2831853f * RING_MID)  // the 3/4 ring's length along its middle
#define FRAME_R 37.0f
#define FACE_R 34.0f

// ------------------------------------------------------------------ quads
typedef struct { float x, y, u, v, r, g, b, a, lx, ly; } Vtx;  // window NDC, uv 0..1, colour, HUD-local position
#define MAX_VTX (6 * 640)  // the gauge (~40 quads) + the menu (~150 quads with the Magic page open) + (s55) 4 level-up panels (~4 x 20) + (s57) 2 party plates (~2 x 18) + (s67) the Drive gauge (~30)
static Vtx g_vtx[MAX_VTX];
static int g_nvtx;
typedef struct { int first, n, tex; float pc[4]; } Batch;  // pc = (cut, L, rmid, mode)
#define MAX_BATCH 96  // (s55: was 32; the level-up panels add 2-3; s57: was 48, the party plates add 5 each; s67: 64, the Drive gauge
                      // adds ~5 + one per additive / alpha change)
static Batch g_batch[MAX_BATCH];
static int g_nbatch;

static struct { float ox, oy, unit, W, H; } T;  // ring centre in window px, HUD unit in px, window size

static void batch_begin(int tex, float cut, float L, float mode) {
    if (g_nbatch >= MAX_BATCH) return;
    Batch *b = &g_batch[g_nbatch++];
    b->first = g_nvtx, b->n = 0, b->tex = tex;
    b->pc[0] = cut, b->pc[1] = L, b->pc[2] = RING_MID, b->pc[3] = mode;
}

static void vtx(float lx, float ly, float u, float v, const float c[4]) {
    if (g_nvtx >= MAX_VTX) return;
    Vtx *o = &g_vtx[g_nvtx++];
    float px = T.ox + lx * T.unit, py = T.oy + ly * T.unit;
    o->x = px / T.W * 2 - 1, o->y = py / T.H * 2 - 1;
    o->u = u, o->v = v, o->r = c[0], o->g = c[1], o->b = c[2], o->a = c[3], o->lx = lx, o->ly = ly;
    if (g_nbatch) g_batch[g_nbatch - 1].n++;
}

// A quad at HUD-local (x0, y0)-(x1, y1) (x0 > x1 or y0 > y1 mirrors the sprite, as KH2's sprite parts do) showing the
// sheet sprite s (tinted). Texel coordinates: PS2 pixels * the sheet's scale; a zero-width rect samples its column.
static void quad(const Spr *s, float x0, float y0, float x1, float y1) {
    float sc = (float)g_img[0].w / 512.0f, W = (float)g_img[0].w, H = (float)g_img[0].h;
    float u0 = s->l * sc / W, u1 = s->r * sc / W, v0 = s->t * sc / H, v1 = s->b * sc / H;
    if (s->r == s->l) u0 = u1 = (s->l * sc + 0.5f) / W;
    if (s->b == s->t) v0 = v1 = (s->t * sc + 0.5f) / H;
    float c[4] = {(s->tint & 0xFF) / 128.0f, ((s->tint >> 8) & 0xFF) / 128.0f, ((s->tint >> 16) & 0xFF) / 128.0f, ((s->tint >> 24) & 0xFF) / 128.0f};
    vtx(x0, y0, u0, v0, c), vtx(x1, y0, u1, v0, c), vtx(x1, y1, u1, v1, c);
    vtx(x0, y0, u0, v0, c), vtx(x1, y1, u1, v1, c), vtx(x0, y1, u0, v1, c);
}

// ------------------------------------------------------------------ the menu's quads (textures 2 = comw sheet, 3 = labels)
// Menu-local coordinates (KH2's 512 x 416 frame, PS2 px, y down) go through vtx() with T.ox / T.oy pointed at the
// frame's origin for the menu's duration (menu_draw). Sprite parts carry KH2's four corner colours: a corner = the
// mean of its side and its top / bottom colour, as KH2's bilinear sprite colouring (hudbake Render) does it.
static void col4(uint32_t c, float o[4]) {
    o[0] = (c & 0xFF) / 128.0f, o[1] = ((c >> 8) & 0xFF) / 128.0f, o[2] = ((c >> 16) & 0xFF) / 128.0f, o[3] = ((c >> 24) & 0xFF) / 128.0f;
}

static void pquad(const CPart *p, float x, float y, float sc, float W, float H);
static void cquad(const CPart *p, float x, float y) {  // one baked part at menu-local (x, y)
    pquad(p, x, y, (float)g_cimg[0].w / 512.0f, (float)g_cimg[0].w, (float)g_cimg[0].h);
}
// a part on a sheet of W x H texels = sc x its PS2 pixels
static void pquad(const CPart *p, float x, float y, float sc, float W, float H) {
    float u0 = p->l * sc / W, u1 = p->r * sc / W, v0 = p->t * sc / H, v1 = p->b * sc / H;
    if (p->r == p->l) u0 = u1 = (p->l * sc + 0.5f) / W;
    if (p->b == p->t) v0 = v1 = (p->t * sc + 0.5f) / H;
    float cl[4], ct[4], cr[4], cb[4], c00[4], c10[4], c11[4], c01[4];
    col4(p->col[0], cl), col4(p->col[1], ct), col4(p->col[2], cr), col4(p->col[3], cb);
    for (int k = 0; k < 4; k++)
        c00[k] = (cl[k] + ct[k]) * 0.5f, c10[k] = (cr[k] + ct[k]) * 0.5f, c11[k] = (cr[k] + cb[k]) * 0.5f, c01[k] = (cl[k] + cb[k]) * 0.5f;
    float x0 = x + p->x0, y0 = y + p->y0, x1 = x + p->x1, y1 = y + p->y1;
    vtx(x0, y0, u0, v0, c00), vtx(x1, y0, u1, v0, c10), vtx(x1, y1, u1, v1, c11);
    vtx(x0, y0, u0, v0, c00), vtx(x1, y1, u1, v1, c11), vtx(x0, y1, u0, v1, c01);
}

static void sgroup(int id, float x, float y) {  // a baked animation group (its steady frame) at menu-local (x, y)
    for (uint32_t g = 0; g < g_ncgrp; g++) {
        if (g_cgrp[g].id != id) continue;
        const CPart *p = (const CPart *)(g_cparts + g_cgrp[g].off);
        for (uint32_t i = 0; i < g_cgrp[g].n; i++) cquad(&p[i], x, y);
        return;
    }
}

// Labels are queued while the sprites are emitted and drawn after them (one texture switch): a label's atlas strip
// at the row + (10, -8), P.menu_font PS2 px tall (the whole atlas cell: evt 32 / sys 24 PS2 px, baseline at 7/8), tinted by the KH2 text colour.
static struct { int id; float x, y; uint32_t col; } g_lab[24];
static int g_nlab;

static void label(int id, float x, float y, uint32_t col) {
    if (id < 0 || id >= g_nclabel || g_nlab >= (int)(sizeof g_lab / sizeof g_lab[0])) return;
    g_lab[g_nlab].id = id, g_lab[g_nlab].x = x, g_lab[g_nlab].y = y, g_lab[g_nlab].col = col, g_nlab++;
}

static void labels_flush(void) {
    if (!g_nlab) return;
    batch_begin(3, 0, 0, 0);
    float W = (float)g_cimg[1].w, H = (float)g_cimg[1].h;
    for (int i = 0; i < g_nlab; i++) {
        const CLabel *l = &g_clabel[g_lab[i].id];
        if (!l->w || !l->h) continue;
        float k = P.menu_font / (float)l->h, x0 = g_lab[i].x + 10, y0 = g_lab[i].y - P.menu_font * 0.583f,  // (caps centred on the row as at 14)
              x1 = x0 + l->w * k, y1 = y0 + l->h * k;
        float u0 = l->x / W, v0 = l->y / H, u1 = (l->x + l->w) / W, v1 = (l->y + l->h) / H, c[4];
        col4(g_lab[i].col, c);
        vtx(x0, y0, u0, v0, c), vtx(x1, y0, u1, v0, c), vtx(x1, y1, u1, v1, c);
        vtx(x0, y0, u0, v0, c), vtx(x1, y1, u1, v1, c), vtx(x0, y1, u0, v1, c);
    }
    g_nlab = 0;
}

// KH2's text colours by entry state (hud_kh2: 0 normal, 2 can't afford, 5 disabled), plain / selected row
static const uint32_t COL_ST[6] = {0x80B4B4B4, 0x80B4B4B4, 0x800079B7, 0x80B4B4B4, 0x80B4B4B4, 0x80646464};
static const uint32_t SEL_ST[6] = {0x80E6E6E6, 0x80E6E6E6, 0x8000A7FF, 0x80E6E6E6, 0x80E6E6E6, 0x80969696};
#define ST(tab, s) ((tab)[(s) < 6 ? (s) : 0])

// (s68 drive) the top page's labels (cmd.bin label ids; default Attack Magic Items Drive; in a Drive Form slot 3 = 9
// Revert): set by the DLL (by name) - a struct field would break older DLLs' SoraHudMenu
static volatile int8_t g_top_lab[4] = {0, 1, 2, 3};
// (s111 roxcmd) which of the 4 top rows are drawn (bit i = slot i; default all): Roxas has no Magic / Drive, so the DLL sets 0b0101
// (Attack, Items): the visible rows are packed (frame, last-row cap and cursor follow). Set by the DLL by name (GetProcAddress).
static volatile int g_top_mask = 0xF;
void sora_hud_menu_rows(int mask) { g_top_mask = (mask & 0xF) ? (mask & 0xF) : 0xF; }
void sora_hud_menu_top(int slot, int label) {
    if (slot >= 0 && slot < 4) g_top_lab[slot] = (int8_t)(label >= 0 && label < SORA_CMD_MAX_LABELS ? label : slot);
}

// (s119 mkmenu) KH2's Mickey command pane (recon/S119_HELPER_MKMENU.md: FUN_1404060e0 case 1 = Jump / Pearl / Attack, each row
// carries its face button's icon; FUN_14015df90 draws pane 14 rows 22 px apart under the title). Set by the DLL by name
// (sora_hud_menu_pad, GetProcAddress; a struct field would break older DLLs' SoraHudMenu), resent every frame while Mickey is out:
// n rows (0 = off), top to bottom: label ids + button-icon label ids (cmd.bin 28 Jump 29 Pearl 30..32 PlayStation Circle / Square /
// Cross, 33..35 Xbox B / X / A) + entry states (the same 0 / 2 / 5 as the top page). Without cmd.bin's labels 28..35 (an old bake) the
// pane stays off and the normal menu draws (logged once).
#define PAD_MAX 4
#define PAD_LABELS 36
#define PAD_GX (-8.0f)  // GUESS: the icon's x / the label's x in the row (menu-local PS2 px, before label()'s +10), tuned by eye on hudbake cmdtest
#define PAD_TX 14.0f
// (review: one g_lock for the rows + their timestamp, as sora_hud_dream / the meal quads: no torn row mixes, and the state
// is read once per row)
typedef struct { int n; int8_t lab[PAD_MAX], btn[PAD_MAX]; uint8_t st[PAD_MAX]; uint64_t at; } PadIn;
static PadIn g_pad_in;
// returns 1 when the pane draws (cmd.bin loaded with labels 28..35), 0 when the normal menu stays (an old bake / not loaded yet):
// the DLL keeps Sora's menu input then
int sora_hud_menu_pad(int n, const int8_t *lab, const int8_t *btn, const uint8_t *st) {
    if (n < 0 || n > PAD_MAX || (n && (!lab || !btn || !st))) n = 0;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < n; i++) g_pad_in.lab[i] = lab[i], g_pad_in.btn[i] = btn[i], g_pad_in.st[i] = st[i];
    g_pad_in.n = n, g_pad_in.at = vk_frame_counter();
    pthread_mutex_unlock(&g_lock);
    return n > 0 && g_cmd_state == 1 && g_nclabel >= PAD_LABELS;
}
static int pad_rows(PadIn *o) {  // rows to draw now (0 = the normal menu), a snapshot in *o
    pthread_mutex_lock(&g_lock);
    *o = g_pad_in;
    pthread_mutex_unlock(&g_lock);
    if (o->n <= 0 || vk_frame_counter() - o->at > 3) return 0;
    if (g_nclabel < PAD_LABELS) {
        static int warned;
        if (!warned) warned = 1, LOG("[sora-hud] Mickey command pane needs cmd.bin labels 28..35 (this bake has %d): the normal menu stays\n", g_nclabel);
        return 0;
    }
    return o->n;
}
static void pad_draw(const PadIn *p) {
    const float x = 28;
    const int n = p->n;
    sgroup(6, x, 322);
    sgroup(18, x, 388 - 22 * n);  // GUESS: the same COMMANDS title as the top page (pane 14's title style is not traced)
    for (int i = 0; i < n; i++) {
        float y = 410 - 22 * n + 22 * i;
        sgroup(i == n - 1 ? 11 : 16, x, y);  // no cursor: KH2's button panes fire on the button itself (no selected-row art traced)
        label(p->btn[i], x + PAD_GX, y, 0x80808080);  // the icon keeps its own colours
        label(p->lab[i], x + PAD_TX, y, ST(COL_ST, p->st[i]));
    }
    labels_flush();
}

// the DLL's text quads as tex 10 triangles (corners TL TR BL BR)
static void mt_quads(const SoraHudQuad *mt, int mn) {
    if (mn <= 0) return;
    batch_begin(10, 0, 0, 0);
    float iu = 1.0f / (float)g_cpimg[1].lw, iv = 1.0f / (float)g_cpimg[1].lh;
    for (int i = 0; i < mn; i++) {
        const SoraHudQuad *q = &mt[i];
        float c[4][4];
        for (int k = 0; k < 4; k++) col4(q->col[k], c[k]);
        // corners TL TR BL BR: (TL TR BR) (TL BR BL)
        vtx(q->x[0], q->y[0], q->u[0] * iu, q->v[0] * iv, c[0]), vtx(q->x[1], q->y[1], q->u[1] * iu, q->v[1] * iv, c[1]);
        vtx(q->x[3], q->y[3], q->u[3] * iu, q->v[3] * iv, c[3]);
        vtx(q->x[0], q->y[0], q->u[0] * iu, q->v[0] * iv, c[0]), vtx(q->x[3], q->y[3], q->u[3] * iu, q->v[3] * iv, c[3]);
        vtx(q->x[2], q->y[2], q->u[2] * iu, q->v[2] * iv, c[2]);
    }
}
// (s93 meals) the DLL's text quads (sys font = texture 10), menu-local PS2 px; the caller has set the menu origin
static void mt_draw(void) {
    if (g_cp_state != 1) return;
    static SoraHudQuad mt[MT_MAX];
    int mn = 0;
    pthread_mutex_lock(&g_lock);
    if (vk_frame_counter() - g_mt_at <= 3 && g_mt_n > 0) mn = g_mt_n, memcpy(mt, g_mt_in, sizeof mt[0] * (size_t)mn);
    pthread_mutex_unlock(&g_lock);
    mt_quads(mt, mn);
}

// (s134 rcslot) KH2's Reaction Command slot (comw anim group 2 offered / 4 flash) over the command menu, at menu-local (36, 377 - 22 n)
// (hud_kh2.py RC_POS), the button icon (cmd.bin label 42 Triangle / 43 Y) inside the slot's circle, the DLL's name text (sys font quads,
// menu-local PS2 px, composed by the DLL at the slot's text origin). Set by the DLL by name (sora_hud_menu_rc), resent every frame, 3 presents
// without a call = gone. Returns 1 when cmd.bin has group 2 and the icon label (else the DLL keeps its own box).
#define RC_ICON_X 15.0f  // GUESS: the icon label's x (+10 in labels_flush = 25, the circle's left edge; hud_kh2 prompt_codes: icon at pen -21 from the text origin 46)
static struct { int state, icon; int n; SoraHudQuad q[MT_MAX]; uint64_t at; } g_rc_in;
static bool grp_has(int id) {
    for (uint32_t g = 0; g < g_ncgrp; g++) if (g_cgrp[g].id == id) return true;
    return false;
}
int sora_hud_menu_rc(int state, int icon_label, const SoraHudQuad *q, int n) {
    if (!q || n < 0) n = 0;
    if (n > MT_MAX) n = MT_MAX;
    pthread_mutex_lock(&g_lock);
    g_rc_in.state = state, g_rc_in.icon = icon_label, g_rc_in.n = n, g_rc_in.at = vk_frame_counter();
    if (n) memcpy(g_rc_in.q, q, sizeof g_rc_in.q[0] * (size_t)n);
    pthread_mutex_unlock(&g_lock);
    if (g_cmd_state != 1 || g_cp_state != 1) return -1;  // (s134 review: not loaded yet / no camp font for the name: -1 = ask again, no warning)
    return state > 0 && grp_has(2) && icon_label >= 0 && icon_label < g_nclabel;
}
static void rc_draw(int nv) {  // nv = the visible top rows (the slot sits right above the title, 388 - 22 nv)
    static struct { int state, icon, n; SoraHudQuad q[MT_MAX]; } r;
    bool on;
    pthread_mutex_lock(&g_lock);
    on = g_rc_in.state > 0 && vk_frame_counter() - g_rc_in.at <= 3;
    if (on) r.state = g_rc_in.state, r.icon = g_rc_in.icon, r.n = g_rc_in.n, memcpy(r.q, g_rc_in.q, sizeof r.q[0] * (size_t)g_rc_in.n);
    pthread_mutex_unlock(&g_lock);
    if (!on || g_cmd_state != 1 || !grp_has(2) || r.icon < 0 || r.icon >= g_nclabel) return;
    float y = 377.0f - 22.0f * (float)nv;
    batch_begin(2, 0, 0, 0);
    sgroup(2, 36, y);
    if (r.state == 2) sgroup(4, 36, y);  // flash = group 4 over the slot (GUESS: it is an additive burst)
    label(r.icon, RC_ICON_X, y, 0x80808080);
    labels_flush();
    if (g_cp_state == 1) {  // the DLL composes the name for the 4-row page's slot: move it with the real one
        float dy = 22.0f * (float)(4 - nv);
        for (int i = 0; i < r.n; i++) for (int k = 0; k < 4; k++) r.q[i].y[k] += dy;
        mt_quads(r.q, r.n);
    }
}

// The pages (plan 4.4): the top page (title 18, rows 9 / 16 / 11; the selected row + icon frame 45, icon, cursor 20),
// the Magic page one level right (60 / 57 / 59 / 58), or the shortcut page (65, rows 67 69 68 66) instead of the top.
static void menu_draw(const SoraHudMenu *m, float vx, float vy, float vw, float vh) {
    static const int ICON[4] = {37, 41, 39, 44};  // Attack, Magic, Items, Drive (KH2 CmdIcon 0x25 0x29 0x27 0x2C)
    static const int SC_G[4] = {67, 69, 68, 66};
    float sox = T.ox, soy = T.oy;
    // the frame: 512 units wide centred in the image, then KH2 PC's widescreen menu shift of -85; 416 tall, bottom on the image's
    T.ox = vx + ((vw / T.unit - 512.0f) * 0.5f - 85.0f + P.menu_dx) * T.unit;
    T.oy = vy + vh - (416.0f - P.menu_dy) * T.unit;
    batch_begin(2, 0, 0, 0);
    const float x = 28;
    const int n = 4;
    int nv_rc = 4;  // (s134) rows of the page the RC slot sits over
    PadIn pin;
    if (pad_rows(&pin)) {  // (s119 mkmenu)
        pad_draw(&pin);
        rc_draw(pin.n);
        T.ox = sox, T.oy = soy;
        return;
    }
    sgroup(6, x, 322);
    if (!m->shortcut) {
        // KH2 stacks the pages: the Magic page (16 right) is drawn whole over the top page, its slots hiding the
        // top page's labels except the left edge, so each page's labels flush before the next page's sprites.
        // The cursor sits on the open page only.
        bool sub = (m->sub == 1 || m->sub == 2) && m->n_sub > 0;  // (s68: 2 = the Drive page, same look)
        const int mask = g_top_mask;  // (s111 roxcmd)
        int nv = 0, pos[4];
        for (int i = 0; i < n; i++) pos[i] = (mask >> i) & 1 ? nv++ : -1;
        nv_rc = nv;
        sgroup(18, x, 388 - 22 * nv);
        for (int i = 0; i < n; i++) {
            if (pos[i] < 0) continue;
            float y = 410 - 22 * nv + 22 * pos[i];
            if (i == m->cursor) {
                sgroup(9, x + 8, y), sgroup(45, x + 8, y), sgroup(ICON[i], x + 8, y);
                if (!sub) sgroup(20, x + 8, y);
                label(g_top_lab[i], x + 8, y, ST(SEL_ST, m->top_state[i]));
            } else {
                sgroup(pos[i] == nv - 1 ? 11 : 16, x, y);
                label(g_top_lab[i], x, y, ST(COL_ST, m->top_state[i]));
            }
        }
        if (sub) {
            labels_flush();
            batch_begin(2, 0, 0, 0);
            int ns = m->n_sub > 8 ? 8 : m->n_sub;
            float xs = x + 16;
            // (s68) group 60 is the "MAGIC" title; KH2's Drive title group is not found yet: the Drive page has none
            // unless SAVAGEWIND_SORA_MENU_DRIVE_TITLE=<group>
            static int dtitle = -2;
            if (dtitle == -2) { const char *e = getenv("SAVAGEWIND_SORA_MENU_DRIVE_TITLE"); dtitle = e && *e ? atoi(e) : -1; }
            int title = m->sub == 2 ? dtitle : 60;
            if (title >= 0) sgroup(title, xs, 388 - 22 * ns);
            for (int i = 0; i < ns; i++) {
                float y = 410 - 22 * ns + 22 * i;
                bool sel = i == m->sub_cursor;
                sgroup(sel ? 57 : i == ns - 1 ? 58 : 59, xs + (sel ? 8 : 0), y);
                if (sel) sgroup(20, xs + 8, y);
                label(m->sub_label[i], xs + (sel ? 8 : 0), y, ST(sel ? SEL_ST : COL_ST, m->sub_state[i]));
            }
        }
    } else {
        sgroup(65, x, 388 - 22 * 4);
        for (int i = 0; i < 4; i++) {
            float y = 410 - 22 * 4 + 22 * i;
            sgroup(SC_G[i], x, y);
            if (m->sc_label[i] >= 0) label(m->sc_label[i], x, y, ST(COL_ST, m->sc_state[i]));
        }
    }
    labels_flush();
    if (!m->shortcut) mt_draw();  // (s93 meals) the DLL's text rows
    rc_draw(nv_rc);
    T.ox = sox, T.oy = soy;
}

// (s54 dream) the picker: the message lines centred on (P.dream_x, P.dream_y) of a 512 x 416 frame centred in the image,
// P.dream_font tall, 1.15 apart, on a dark box (one texel of the gauge sheet's dark bar, sprite 80's, ~70% alpha: GUESS
// look); the choices as the command menu's slots in its frame (the selected one 8 px right with the cursor), labels as
// the menu's. Layout = tools/hudbake DreamTest (keep in step).
static const Spr SP_DREAM_BOX = {184, 54, 184, 54, 0x5A0C0C0F};
static void dream_label(int id, float x0, float y0, float h, uint32_t col) {  // one atlas strip at (x0, y0), h tall
    if (id < 0 || (uint32_t)id >= g_dm_n) return;
    const CLabel *l = &g_dlabel[id];
    if (!l->w || !l->h) return;
    float k = h / (float)l->h, x1 = x0 + l->w * k, y1 = y0 + l->h * k, W = (float)g_dm_w, H = (float)g_dm_h, c[4];
    float u0 = l->x / W, v0 = l->y / H, u1 = (l->x + l->w) / W, v1 = (l->y + l->h) / H;
    col4(col, c);
    vtx(x0, y0, u0, v0, c), vtx(x1, y0, u1, v0, c), vtx(x1, y1, u1, v1, c);
    vtx(x0, y0, u0, v0, c), vtx(x1, y1, u1, v1, c), vtx(x0, y1, u0, v1, c);
}
static float dream_w(int id, float h) {
    return id < 0 || (uint32_t)id >= g_dm_n || !g_dlabel[id].h ? 0 : g_dlabel[id].w * h / g_dlabel[id].h;
}

static void dream_draw(const SoraHudDream *d, float vx, float vy, float vw, float vh) {
    float sox = T.ox, soy = T.oy;
    int nt = d->n_text < 0 ? 0 : d->n_text > 4 ? 4 : d->n_text, nr = d->n_rows < 0 ? 0 : d->n_rows > 4 ? 4 : d->n_rows;
    if (nt) {  // the message: its own frame, centred in the image (no widescreen shift)
        T.ox = vx + (vw / T.unit - 512.0f) * 0.5f * T.unit, T.oy = vy + vh - 416.0f * T.unit;
        float h = P.dream_font, pitch = h * 1.15f, top = P.dream_y - nt * pitch * 0.5f, mw = 0;
        for (int j = 0; j < nt; j++)
            if (dream_w(d->text[j], h) > mw) mw = dream_w(d->text[j], h);
        batch_begin(0, 0, 0, 0);
        quad(&SP_DREAM_BOX, P.dream_x - mw * 0.5f - 10, top - 5, P.dream_x + mw * 0.5f + 10, top + nt * pitch + 5);
        batch_begin(5, 0, 0, 0);
        for (int j = 0; j < nt; j++)
            dream_label(d->text[j], P.dream_x - dream_w(d->text[j], h) * 0.5f, top + j * pitch + (pitch - h) * 0.5f, h, 0x80E6E6E6);
    }
    if (nr) {  // the choices: the command menu's frame and slots (menu_draw's origin and row spacing)
        T.ox = vx + ((vw / T.unit - 512.0f) * 0.5f - 85.0f + P.menu_dx) * T.unit;
        T.oy = vy + vh - (416.0f - P.menu_dy) * T.unit;
        const float x = 28;
        batch_begin(2, 0, 0, 0);
        sgroup(6, x, 322);
        for (int i = 0; i < nr; i++) {
            float y = 410 - 22 * nr + 22 * i;
            if (i == d->cursor) sgroup(9, x + 8, y), sgroup(20, x + 8, y);
            else sgroup(i == nr - 1 ? 11 : 16, x, y);
        }
        batch_begin(5, 0, 0, 0);
        for (int i = 0; i < nr; i++) {
            float y = 410 - 22 * nr + 22 * i, xr = x + (i == d->cursor ? 8 : 0);
            dream_label(d->row[i], xr + 10, y - P.menu_font * 0.583f, P.menu_font, i == d->cursor ? SEL_ST[0] : COL_ST[0]);
        }
    }
    T.ox = sox, T.oy = soy;
}

// (s55 HUD) the level-up panels on screen (render thread): one batch of up to 4 (LEVELUP_INFO re:0x17E050), stacked by
// their group's UiPadding, all started together; t = the seconds the batch has been shown (its clock stops while the
// gauge is hidden: BotW paused, Link hidden). Text placement = dk::LEVEL_UP's draw re:0x17D630 (PHH hud_kh2 LevelUp):
// line 1 at the anchor + (0x55 + LightPositionX, TextPositionY); the name right-aligned at the anchor + 0x55; the level
// at the anchor + TextPositionX + 0x55; line 2 at the anchor + (LightPositionX + 0x55, 0x11 + TextPositionY) (the +0x55
// when that x >= 0x100, else -0x55: KH2's side test); alpha = the anchor animation's. Layout = hudbake LvupTest.
#define LV_BATCH 4
#define LV_XOFF 0x55
static struct {
    int n;                                 // panels in the batch (0 = idle)
    int grp[LV_BATCH], y[LV_BATCH];        // group index (0 = 104 one line, 1 = 105 two lines), stack y (frame px)
    int lab[LV_BATCH][2], nl[LV_BATCH];    // the lines' labels
    int lab_lv[LV_BATCH], lab_name[LV_BATCH];  // the level's and the name's labels (s76: per panel, Sora or a form)
    double t, last;                        // seconds shown, wall clock of the last present
} LVB;

static double lv_now(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void lvup_start(const SoraHudLevelUp *u, int n) {  // a new batch from the queue's first n requests
    static uint32_t said_missing;
    int y = 0;
    LVB.n = 0, LVB.t = 0;
    for (int i = 0; i < n && i < LV_BATCH; i++) {
        int k = LVB.n, nl = 0, want = u[i].n_lines < 0 ? 0 : u[i].n_lines > 2 ? 2 : u[i].n_lines;
        if ((LVB.lab_name[k] = lvup_label(u[i].name)) < 0) {  // (s76: a form's name missing from an older lvup.bin = Sora)
            if (said_missing != u[i].name) said_missing = u[i].name, LOG("[sora-hud] level-up: no label for name key %#x (Sora's used)\n", u[i].name);
            LVB.lab_name[k] = lvup_label(0);
        }
        for (int j = 0; j < want; j++) {
            int id = lvup_label(u[i].line[j]);
            if (id >= 0) LVB.lab[k][nl++] = id;
            else if (said_missing != u[i].line[j]) said_missing = u[i].line[j], LOG("[sora-hud] level-up: no label for line key %#x (left out)\n", u[i].line[j]);
        }
        int lv = u[i].level < 1 ? 1 : u[i].level > 99 ? 99 : u[i].level;
        LVB.nl[k] = nl, LVB.grp[k] = nl >= 2 ? 1 : 0, LVB.lab_lv[k] = lvup_label((uint32_t)lv), LVB.y[k] = y;
        y += g_lvg[LVB.grp[k]].pad;  // (re:0x192DE0: 44 one line, 64 two)
        LVB.n++;
        LOG("[sora-hud] level-up panel: LV %d, name key %#x, %d line%s (group %u), stack y %d\n", lv, u[i].name, nl, nl == 1 ? "" : "s", g_lvg[LVB.grp[k]].id, LVB.y[k]);
    }
}

static void lvup_text(int id, float x, float y, float alpha, bool right) {  // one baked text at its KH2 top-left (x, y)
    if (id < 0) return;
    const VLabel *l = &g_lvl[id];
    float k = 1.0f / (float)g_lvh->as, W = (float)g_lvh->w, H = (float)g_lvh->h, c[4] = {1, 1, 1, alpha};
    if (right) x -= l->adv;  // (KH2 align 1: x - the text's width)
    float x0 = x + l->ox, y0 = y + l->oy, x1 = x0 + l->w * k, y1 = y0 + l->h * k;
    float u0 = l->x / W, v0 = l->y / H, u1 = (l->x + l->w) / W, v1 = (l->y + l->h) / H;
    vtx(x0, y0, u0, v0, c), vtx(x1, y0, u1, v0, c), vtx(x1, y1, u1, v1, c);
    vtx(x0, y0, u0, v0, c), vtx(x1, y1, u1, v1, c), vtx(x0, y1, u0, v1, c);
}

static void lvup_draw(int f, float vx, float vy, float vw, float vh) {
    (void)vh;
    float sox = T.ox, soy = T.oy;
    // KH2's frame + the right-side HUD's widescreen 0x55: x 512 + 0x55 = 597 on the image's right edge, y 0 on its top
    T.ox = vx + vw - (512.0f + LV_XOFF - P.lv_dx) * T.unit, T.oy = vy + P.lv_dy * T.unit;
    float sc = (float)g_img[0].w / 512.0f, W = (float)g_img[0].w, H = (float)g_img[0].h;
    int mode = -1;
    for (int k = 0; k < LVB.n; k++) {  // the panels' sprites (the gauge sheet, texture 0), the bake's order
        const VGrp *g = &g_lvg[LVB.grp[k]];
        const VFrame *fr = &g_lvf[g->first + (f < (int)g->n ? f : (int)g->n - 1)];
        const LPart *p = g_lvp + fr->off;
        for (uint32_t i = 0; i < fr->n; i++, p++) {
            int pm = p->blend ? 4 : 0;
            if (pm != mode) batch_begin(0, 0, 0, (float)(mode = pm));
            float u0 = p->l * sc / W, u1 = p->r * sc / W, v0 = p->t * sc / H, v1 = p->b * sc / H;
            if (p->r == p->l) u0 = u1 = (p->l * sc + 0.5f) / W;
            if (p->b == p->t) v0 = v1 = (p->t * sc + 0.5f) / H;
            float cl[4], ct[4], cr[4], cb[4], c[4][4], X[4], Y[4];
            col4(p->col[0], cl), col4(p->col[1], ct), col4(p->col[2], cr), col4(p->col[3], cb);
            for (int j = 0; j < 4; j++)  // (corner = the mean of its side and its top / bottom colour, as pquad)
                c[0][j] = (cl[j] + ct[j]) * 0.5f, c[1][j] = (cr[j] + ct[j]) * 0.5f, c[2][j] = (cr[j] + cb[j]) * 0.5f, c[3][j] = (cl[j] + cb[j]) * 0.5f;
            for (int j = 0; j < 4; j++) X[j] = LV_XOFF + p->x[j], Y[j] = (float)LVB.y[k] + p->y[j];
            vtx(X[0], Y[0], u0, v0, c[0]), vtx(X[1], Y[1], u1, v0, c[1]), vtx(X[2], Y[2], u1, v1, c[2]);
            vtx(X[0], Y[0], u0, v0, c[0]), vtx(X[2], Y[2], u1, v1, c[2]), vtx(X[3], Y[3], u0, v1, c[3]);
        }
    }
    batch_begin(6, 0, 0, 0);  // the text (atlas, texture 6; colour baked in, alpha = the anchor's)
    for (int k = 0; k < LVB.n; k++) {
        const VGrp *g = &g_lvg[LVB.grp[k]];
        const VFrame *fr = &g_lvf[g->first + (f < (int)g->n ? f : (int)g->n - 1)];
        float a = (float)((fr->col >> 24) & 0xFF) / 128.0f;
        if (a <= 0.004f) continue;
        if (a > 1) a = 1;
        int ax = fr->ax, ay = fr->ay + LVB.y[k], lx = ax + g->text_x, l2 = ax + g->light_x;
#define LV_SIDE(x) ((x) >= 0x100 ? LV_XOFF : -LV_XOFF)
        if (LVB.nl[k] >= 1) lvup_text(LVB.lab[k][0], (float)(LV_XOFF + ax + g->light_x), (float)(ay + g->text_y), a, false);
        lvup_text(LVB.lab_name[k], (float)(ax + LV_SIDE(ax)), (float)ay, a, true);
        lvup_text(LVB.lab_lv[k], (float)(lx + LV_SIDE(lx)), (float)ay, a, false);
        if (LVB.nl[k] >= 2) lvup_text(LVB.lab[k][1], (float)(l2 + LV_SIDE(l2)), (float)(ay + 0x11 + g->text_y), a, false);
#undef LV_SIDE
    }
    T.ox = sox, T.oy = soy;
}

// The ring's three quadrants (sprite groups 91 / 93 / 95): top-left as drawn, top-right X-mirrored, bottom-right
// XY-mirrored; the bottom-left quadrant is open (the bar enters there).
static void ring(const Spr *arc) {
    quad(arc, -RING_OUT, -RING_OUT, 0, 0);
    quad(arc, RING_OUT, -RING_OUT, 0, 0);
    quad(arc, RING_OUT, RING_OUT, 0, 0);
}
// The straight bar (sprite groups 92 / 94 / 96): the cross-section column from x = -L to 0, Y-mirrored so the ring's
// top-edge cross section lands at the bottom (y 37..45).
static void bar(const Spr *col, float L) { quad(col, -L, RING_OUT, 0, 0); }

// ------------------------------------------------------------------ the enemy gauge (KH2 'gaug' sg 63-71)
// HUD-local origin = the bar's right end (KH2: anim group 38's (498, 31)), y down: the dark bar (sg 64 / 69) from x = -L
// to 0 at y -5..5 with 2 px caps, the green fill (sg 66) at y -3..3 draining from the left (mode 2 clip), the pip row
// (sg 67 / 68 = an emptied / a remaining extra bar, 13 x 10 every 11 px leftwards from x -33) at y 3..13 on its black
// housing (sg 63). The ghost of lost life (sg 70, anim group 41): flashes white f0-10, holds to f60, fades out f61-90.
static void quad_c(const Spr *s, float x0, float y0, float x1, float y1, const float c[4][4]) {  // corners TL TR BR BL
    float sc = (float)g_img[0].w / 512.0f, W = (float)g_img[0].w, H = (float)g_img[0].h;
    float u0 = s->l * sc / W, u1 = s->r * sc / W, v0 = s->t * sc / H, v1 = s->b * sc / H;
    if (s->r == s->l) u0 = u1 = (s->l * sc + 0.5f) / W;
    if (s->b == s->t) v0 = v1 = (s->t * sc + 0.5f) / H;
    vtx(x0, y0, u0, v0, c[0]), vtx(x1, y0, u1, v0, c[1]), vtx(x1, y1, u1, v1, c[2]);
    vtx(x0, y0, u0, v0, c[0]), vtx(x1, y1, u1, v1, c[2]), vtx(x0, y1, u0, v1, c[3]);
}

static void en_green(const Spr *s, float x0, float y0, float x1, float y1) {  // L/T 8007E794, R/B 8001BD23 (pquad's rule)
    float g1[4], g2[4], c[4][4];
    col4(0x8007E794, g1), col4(0x8001BD23, g2);
    for (int j = 0; j < 4; j++) c[0][j] = g1[j], c[2][j] = g2[j], c[1][j] = c[3][j] = (g1[j] + g2[j]) * 0.5f;
    quad_c(s, x0, y0, x1, y1, c);
}

static struct { float ghost, last, max; int t; bool valid; } E;  // render thread

static void en_draw(float life, float max, bool left) {  // left: T.ox = the image's left edge + x; the bar is placed by its left end
    float per = P.en_hp;
    int nb = (int)ceilf(max / per), b = life > 0 ? (int)ceilf(life / per) : 1;
    if (nb < 1) nb = 1;
    if (b > nb) b = nb;
    float base = per * (float)(b - 1), cap = b == nb ? max - base : per;
    float L = nb > 1 ? EN_LEN : fminf(EN_LEN, fmaxf(P.en_min, max * EN_LEN / per));
    if (left) T.ox += (L + 2.0f) * T.unit;  // (s92: the bar spans -L-2 .. +2 around its origin, the pips and tab inside)
    float frac = fminf(fmaxf((life - base) / cap, 0), 1);
    // the ghost starts at the life before the first hit of a run; a hit while it shows restarts its clock
    if (!E.valid || E.max != max || life > E.last) E.ghost = life, E.t = 91, E.max = max, E.valid = true;
    else if (life < E.last) {
        if (E.t > 90) E.ghost = E.last;
        E.t = 0;
    }
    E.last = life;
    if (E.t <= 90) E.t++;
    float gfrac = fminf(fmaxf((E.ghost - base) / cap, frac), 1);  // (a ghost from the bar above shows this bar full)

    batch_begin(0, 0, 0, 0);
    if (nb > 1) {
        quad(&SP_EN_TAB, -23, 3, 2, 13);
        for (int k = 1; k < nb && k <= 16; k++) {
            float x1 = -20 - 11.0f * (float)(k - 1), x0 = x1 - 13;
            if (k < b) en_green(&SP_EN_PIP_ON, x0, 3, x1, 13);
            else quad(&SP_EN_PIP_OFF, x0, 3, x1, 13);
        }
    }
    quad(&SP_EN_FRAME, -L, -5, 0, 5);
    quad(&SP_EN_CAP, -L - 2, -5, -L, 5), quad(&SP_EN_CAP, 2, -5, 0, 5);
    if (E.t <= 90 && gfrac > frac) {
        float k = E.t <= 10 ? 2.0f - (float)E.t / 10.0f : 1.0f, a = E.t <= 60 ? 1.0f : 1.0f - (float)(E.t - 60) / 30.0f, c[4][4];
        col4(SP_EN_GHOST.tint, c[0]);
        c[0][0] *= k, c[0][1] *= k, c[0][2] *= k, c[0][3] *= a;
        for (int j = 1; j < 4; j++) memcpy(c[j], c[0], sizeof c[0]);
        batch_begin(0, -L * gfrac, 0, 2);
        quad_c(&SP_EN_GHOST, -L, -3, 0, 3, c);
    }
    if (frac > 0) {
        batch_begin(0, -L * frac, 0, 2);
        en_green(&SP_EN_FILL, -L, -3, 0, 3);
    }
}

// ------------------------------------------------------------------ (s57 party) the plates (render thread)
// KH2's GaugeFriend tick (re:0x182730, PHH's port): state 0 normal / 1 low (HP <= max HP / 4 in integers, re:0x3C1D30) /
// 2 KO (HP 0). First shown = the intro (71), then the state's plate (72 / 67); a loss (re:0x182530) = the hit (70) + the
// track restarted at the HP before it (64) + the hurt face (1); a gain (re:0x182360) = the state's plate and face, no
// track; member 2 slides down (73) when member 1 is not shown and back up (74) when he is again. The HP / MP / track
// arcs = their group's steady frame clipped by shader mode 5 at ratio x pi (re:0x182180's CIRCLE_MASK angles).
typedef struct { int g, next; double t; } PtAn;  // PG_* now and after a one-shot (-1 none); t = KH2 frames (60 Hz)
static struct {
    bool shown, slid;
    int state;
    float hp, hurt;  // ratios: HP now, and the track's (the HP before the last loss)
    PtAn slot, face;
    double track_t;  // KH2 frames into ag 64 (< 0: no track)
} PT[2];

static int pt_plate(int st) { return st == 1 ? PG_LOW : PG_NORMAL; }
static int pt_face(int st) { return st == 2 ? PG_F_KO : st == 1 ? PG_F_LOW : PG_F_NORMAL; }
static bool pt_oneshot(int pg) { return g_ptg[g_pg[pg]].loop == 0xFFFF; }
static void pt_set(PtAn *a, int g, int next) { a->g = g, a->next = next, a->t = 0; }
static void pt_want(PtAn *a, int g) {  // the state's group: now, or after the one-shot playing
    if (pt_oneshot(a->g)) a->next = g;
    else if (a->g != g) pt_set(a, g, -1);
}

// the frame an animation shows at its clock: a one-shot that ended goes on to its next group, a looping group wraps
// to its loop frame, a one-shot with no next holds its last frame
static const PtFrame *pt_frame(PtAn *a) {
    const PtGrp *g = &g_ptg[g_pg[a->g]];
    if (a->t >= g->n && g->loop == 0xFFFF && a->next >= 0) {
        a->t -= g->n, a->g = a->next, a->next = -1;
        g = &g_ptg[g_pg[a->g]];
    }
    if (a->t >= g->n && g->loop != 0xFFFF) a->t = g->loop + fmod(a->t - g->loop, (double)(g->n - g->loop));
    int f = (int)a->t;
    return &g_ptf[g->first + (f < g->n ? f : g->n - 1)];
}
static const PtFrame *pt_steady(int pg) {  // a looping child's steady frame (ag 65 / 66: one colour all along)
    const PtGrp *g = &g_ptg[g_pg[pg]];
    return &g_ptf[g->first + (g->loop != 0xFFFF ? g->loop : g->n - 1)];
}

// a baked frame's parts around T.ox / T.oy: tex 0 = the gauge sheet (PS2 px of 512 x 256), 7 / 8 = a face (PS2 px of
// face_ps2); mode 5 = the arc clip at cut (radians from 6 o'clock). Corner colours as lvup_draw / pquad.
// (s67 drive: parts0 = the bake's parts, so drive.bin's frames go through here too; addit: a part with blend 1 draws
// additive (mode 4) in a batch of its own, as the lock-on icon's, where mode is 0)
static void lp_parts(const LPart *parts0, const PtFrame *fr, int tex, int mode, float cut, bool addit) {
    bool face = tex >= 7;
    float W = face ? (float)g_pth->face_w : (float)g_img[0].w, H = face ? (float)g_pth->face_h : (float)g_img[0].h;
    float sc = face ? W / (float)g_pth->face_ps2 : W / 512.0f;
    batch_begin(tex, cut, 0, (float)mode);
    float cur = (float)mode;
    const LPart *p = parts0 + fr->off;
    for (uint32_t i = 0; i < fr->n; i++, p++) {
        float want = addit && mode == 0 && p->blend ? 4.0f : (float)mode;
        if (want != cur) batch_begin(tex, cut, 0, cur = want);
        float u0 = p->l * sc / W, u1 = p->r * sc / W, v0 = p->t * sc / H, v1 = p->b * sc / H;
        if (p->r == p->l) u0 = u1 = (p->l * sc + 0.5f) / W;
        if (p->b == p->t) v0 = v1 = (p->t * sc + 0.5f) / H;
        float cl[4], ct[4], cr[4], cb[4], c[4][4];
        col4(p->col[0], cl), col4(p->col[1], ct), col4(p->col[2], cr), col4(p->col[3], cb);
        for (int j = 0; j < 4; j++)
            c[0][j] = (cl[j] + ct[j]) * 0.5f, c[1][j] = (cr[j] + ct[j]) * 0.5f, c[2][j] = (cr[j] + cb[j]) * 0.5f, c[3][j] = (cl[j] + cb[j]) * 0.5f;
        vtx(p->x[0], p->y[0], u0, v0, c[0]), vtx(p->x[1], p->y[1], u1, v0, c[1]), vtx(p->x[2], p->y[2], u1, v1, c[2]);
        vtx(p->x[0], p->y[0], u0, v0, c[0]), vtx(p->x[2], p->y[2], u1, v1, c[2]), vtx(p->x[3], p->y[3], u0, v1, c[3]);
    }
}
static void pt_parts(const PtFrame *fr, int tex, int mode, float cut) { lp_parts(g_ptp, fr, tex, mode, cut, false); }

// both plates around the ring centre (T.ox, T.oy) = KH2's orb; dt = seconds since the last present (clocks x 60)
static void party_draw(const PtIn in[2], double dt) {
    static const char *NAME[2] = {"Donald", "Goofy"};
    float sox = T.ox, soy = T.oy;
    for (int k = 1; k >= 0; k--) {  // (KH2's creation order: member 2's plate, member 1's, then the player gauge)
        const PtIn *v = &in[k];
        if (!v->shown) {
            if (PT[k].shown) LOG("[sora-hud] party %d (%s): hidden\n", k, NAME[k]);
            PT[k].shown = false;
            continue;
        }
        float hp = v->hp / v->hp_max, mp = v->mp_max > 0 ? fabsf(v->mp) / v->mp_max : 0;
        int st = v->hp <= 0 ? 2 : (int)v->hp <= (int)v->hp_max / 4 ? 1 : 0;
        if (!PT[k].shown) {
            PT[k].shown = true, PT[k].state = st, PT[k].hp = PT[k].hurt = hp, PT[k].track_t = -1;
            PT[k].slid = k == 1 && !in[0].shown;
            pt_set(&PT[k].slot, PG_INTRO, pt_plate(st)), pt_set(&PT[k].face, pt_face(st), -1);
            LOG("[sora-hud] party %d (%s): shown (intro), hp %.0f/%.0f mp %.0f/%.0f%s\n", k, NAME[k], v->hp, v->hp_max, v->mp, v->mp_max,
                PT[k].slid ? ", in member 1's place" : "");
        } else {
            int prev = PT[k].state;
            PT[k].state = st;
            if (hp < PT[k].hp - 1e-4f) {
                PT[k].hurt = PT[k].hp, PT[k].hp = hp, PT[k].track_t = 0;
                pt_set(&PT[k].slot, PG_HIT, pt_plate(st)), pt_set(&PT[k].face, PG_F_HURT, pt_face(st));
                LOG("[sora-hud] party %d (%s): hit, hp %.0f/%.0f (track from %.0f%%), state %d\n", k, NAME[k], v->hp, v->hp_max, PT[k].hurt * 100, st);
            } else if (hp > PT[k].hp + 1e-4f) {
                PT[k].hp = PT[k].hurt = hp, PT[k].track_t = -1;
                pt_want(&PT[k].slot, pt_plate(st)), pt_set(&PT[k].face, pt_face(st), -1);
            } else if (prev != st) pt_want(&PT[k].slot, pt_plate(st)), pt_want(&PT[k].face, pt_face(st));
            if (k == 1 && !in[0].shown && !PT[k].slid) {
                PT[k].slid = true, pt_set(&PT[k].slot, PG_SLIDE_UP, pt_plate(st));
                LOG("[sora-hud] party 1 (Goofy): slides into member 1's place\n");
            } else if (k == 1 && in[0].shown && PT[k].slid) {
                PT[k].slid = false, pt_set(&PT[k].slot, PG_SLIDE_BACK, pt_plate(st));
                LOG("[sora-hud] party 1 (Goofy): slides back up\n");
            }
        }
        double df = dt * 60.0;
        PT[k].slot.t += df, PT[k].face.t += df;
        if (PT[k].track_t >= 0 && (PT[k].track_t += df) >= g_ptg[g_pg[PG_TRACK]].n) PT[k].track_t = -1;  // (faded out at its end)
        // the plate at the ring centre + its base (member 2: pitch up unless slid), the children at the plate's anchor
        float base = k == 1 && !PT[k].slid ? (float)g_pth->pitch : 0;
        T.ox = sox, T.oy = soy + base * T.unit;
        const PtFrame *sf = pt_frame(&PT[k].slot);
        pt_parts(sf, 0, 0, 0);
        T.ox = sox + sf->ax * T.unit, T.oy = soy + (base + sf->ay) * T.unit;
#define PT_CUT(r) ((r) >= 1 ? 4.0f : (r) * 3.14159265f)
        if (PT[k].track_t >= 0 && PT[k].hurt > PT[k].hp) {
            const PtGrp *g = &g_ptg[g_pg[PG_TRACK]];
            int f = (int)PT[k].track_t;
            pt_parts(&g_ptf[g->first + (f < g->n ? f : g->n - 1)], 0, 5, PT_CUT(PT[k].hurt));
        }
        if (hp > 0) pt_parts(pt_steady(PG_HP), 0, 5, PT_CUT(hp));
        if (mp > 0) pt_parts(pt_steady(PG_MP), 0, 5, PT_CUT(mp));
#undef PT_CUT
        pt_parts(pt_frame(&PT[k].face), 7 + k, 0, 0);
    }
    T.ox = sox, T.oy = soy;
}

// ------------------------------------------------------------------ (s67 drive) the gauge (render thread)
// KH2's Drive tick (re:0x1816C0, PHH's port): first shown = the plate's intro (21) then 23 (26 while MP Charge runs); a
// change of bars restarts the digit (28 + bars) and a gain plays the bar-up effect (22) once; the full effect (20) loops
// while bars = max. The fill (25, its steady frame) is cut by mode 6 at 0.004014257 x points (full: 200 points).
typedef struct { int g, next; double t; } DrAn;  // DG_* now and after a one-shot (-1 none); t = KH2 frames (60 Hz)
static struct {
    bool shown, full, sum;
    int bars;
    DrAn plate, digit;
    double barup_t, full_t;  // KH2 frames into ag 22 (< 0: not playing) / ag 20
} DR;

static void dr_set(DrAn *a, int g, int next) { a->g = g, a->next = next, a->t = 0; }
static const PtFrame *dr_frame(DrAn *a) {  // as pt_frame: a one-shot goes on to its next group, a loop wraps
    const PtGrp *g = &g_drg[g_dg[a->g]];
    if (a->t >= g->n && g->loop == 0xFFFF && a->next >= 0) {
        a->t -= g->n, a->g = a->next, a->next = -1;
        g = &g_drg[g_dg[a->g]];
    }
    if (a->t >= g->n && g->loop != 0xFFFF) a->t = g->loop + fmod(a->t - g->loop, (double)(g->n - g->loop));
    int f = (int)a->t;
    return &g_drf[g->first + (f < g->n ? f : g->n - 1)];
}
static const PtFrame *dr_at(int dg, double t) {  // a group's frame at clock t (looping groups wrap, one-shots hold)
    DrAn a = {dg, -1, t};
    return dr_frame(&a);
}

// (s139 sumplate) KH2's summon orb plate (FUN_140180e00 / FUN_14017f210): group 154 (fade in) -> 155 (loop) while a summon is out,
// 156 (fade out) when it ends; the summon's own face (its .a.us 'face' sprite, priority 13000 over the plate's 7000) sits at the
// plate's anchor, drawn plain (no circle clip: KH2's face is a normal sprite). Plate at the orb, anchor = orb + (ax, ay).
static struct { bool on, out; int id; DrAn an; } ORB;
static void orb_draw(double dt, bool gauge) {
    if (!gauge) { ORB.on = false; return; }  // (the gauge is hidden: the plate goes at once)
    int want = gauge && g_dg_orb && g_sf.state == 1 && g_sf_id > 0 && (uint32_t)g_sf_id <= g_sf.n ? g_sf_id : 0;
    if (want && (!ORB.on || ORB.out)) {
        ORB.on = true, ORB.out = false, ORB.id = want, dr_set(&ORB.an, DG_ORB_IN, DG_ORB);
        LOG("[sora-hud] summon orb plate: in (face %d)\n", want);
    } else if (want && ORB.on) ORB.id = want;
    else if (!want && ORB.on && !ORB.out) {
        ORB.out = true, dr_set(&ORB.an, DG_ORB_OUT, -1);
        LOG("[sora-hud] summon orb plate: out\n");
    }
    if (!ORB.on) return;
    ORB.an.t += dt * 60.0;
    if (ORB.out && ORB.an.t >= g_drg[g_dg[DG_ORB_OUT]].n) { ORB.on = false; return; }
    const PtFrame *pf = dr_frame(&ORB.an);
    lp_parts(g_drp, pf, 0, 0, 0, true);
    // the face: the plate's anchor colour alpha (0x80 = opaque) fades it with the plate (GUESS: not traced in KH2's exe)
    float a = (float)(pf->col >> 24) / 128.0f, sox = T.ox, soy = T.oy;
    if (a > 1) a = 1;
    int i = ORB.id - 1;
    const int32_t *r = g_sf.rect[i];
    T.ox = sox + pf->ax * T.unit, T.oy = soy + pf->ay * T.unit;
    float u0 = (float)((uint32_t)i % g_sf.cols * g_sf.tile) / g_sf.w, u1 = u0 + (float)g_sf.tile / g_sf.w;
    float v0 = (float)((uint32_t)i / g_sf.cols * g_sf.tile) / g_sf.h, v1 = v0 + (float)g_sf.tile / g_sf.h;
    float c[4] = {1, 1, 1, a};
    batch_begin(14, 0, 0, 0);
    vtx((float)r[0], (float)r[1], u0, v0, c), vtx((float)r[2], (float)r[1], u1, v0, c), vtx((float)r[2], (float)r[3], u1, v1, c);
    vtx((float)r[0], (float)r[1], u0, v0, c), vtx((float)r[2], (float)r[3], u1, v1, c), vtx((float)r[0], (float)r[3], u0, v1, c);
    T.ox = sox, T.oy = soy;
}

// around the ring centre (T.ox, T.oy) = KH2's orb; charge = MP Charge runs; dt = seconds since the last present
static void drive_draw(const DrIn *v, bool charge, double dt) {
    if (!v->state || v->max <= 0) {
        if (DR.shown) LOG("[sora-hud] drive: hidden\n");
        DR.shown = false;
        orb_draw(dt, false);  // (s139: the orb plate goes with the gauge)
        return;
    }
    bool sum = v->state == 2 && g_dg_sum;  // (s137) a summon is out: KH2's drive state 3 art
    bool full = v->bars >= v->max && !sum;  // (s137: no full fx in the summon state: GUESS, not traced)
    int plate = sum ? DG_SPLATE : charge ? DG_CHARGE : DG_PLATE;
    const int dig0 = sum ? DG_SDIGIT0 : DG_DIGIT0;
    if (DR.shown && sum != DR.sum) {  // KH2 sets the plate / fill / digit groups at once when the state changes (FUN_140180b00 / FUN_140181270)
        DR.sum = sum, DR.barup_t = -1, DR.full = full, DR.full_t = 0, DR.bars = v->bars;
        dr_set(&DR.plate, plate, -1), dr_set(&DR.digit, dig0 + v->bars, -1);
        LOG("[sora-hud] drive: %s plate\n", sum ? "summon (KH2 drive state 3)" : "normal");
    }
    if (!DR.shown) {
        DR.shown = true, DR.sum = sum, DR.bars = v->bars, DR.full = full, DR.barup_t = -1, DR.full_t = 0;
        dr_set(&DR.plate, sum ? plate : DG_INTRO, plate), dr_set(&DR.digit, dig0 + v->bars, -1);
        LOG("[sora-hud] drive: shown (intro), %d bars + %d of %d%s\n", v->bars, v->fill, v->max, full ? " FULL" : "");
    } else {
        if (DR.plate.g == DG_INTRO) DR.plate.next = plate;
        else if (DR.plate.g != plate) dr_set(&DR.plate, plate, -1);
        if (v->bars != DR.bars) {
            if (v->bars > DR.bars && !sum) DR.barup_t = 0;
            LOG("[sora-hud] drive: bars %d -> %d%s\n", DR.bars, v->bars, v->bars > DR.bars ? " (bar-up fx)" : "");
            DR.bars = v->bars, dr_set(&DR.digit, dig0 + v->bars, -1);
        }
        if (full != DR.full) LOG("[sora-hud] drive: %s\n", full ? "FULL (fx 20 on)" : "not full"), DR.full = full, DR.full_t = 0;
    }
    double df = dt * 60.0;
    DR.plate.t += df, DR.digit.t += df, DR.full_t += df;
    if (DR.barup_t >= 0 && (DR.barup_t += df) >= g_drg[g_dg[DG_BARUP]].n) DR.barup_t = -1;
    // the plate at the orb, its children at the plate's anchor
    float sox = T.ox, soy = T.oy;
    const PtFrame *pf = dr_frame(&DR.plate);
    lp_parts(g_drp, pf, 0, 0, 0, true);
    T.ox = sox + pf->ax * T.unit, T.oy = soy + pf->ay * T.unit;
    {
        const PtGrp *g = &g_drg[g_dg[sum ? DG_SFILL : DG_FILL]];
        float ang = 0.004014257f * (float)(full ? 200 : v->fill);
        lp_parts(g_drp, &g_drf[g->first + (g->loop != 0xFFFF ? g->loop : g->n - 1)], 0, 6, ang, false);
        if (g_nbatch) g_batch[g_nbatch - 1].pc[1] = 50, g_batch[g_nbatch - 1].pc[2] = 100;  // (the mask's pivot from the anchor)
    }
    float ax = T.ox, ay = T.oy;
    T.ox = ax + (float)g_drh->digit_x * T.unit, T.oy = ay + (float)g_drh->digit_y * T.unit;
    lp_parts(g_drp, dr_frame(&DR.digit), 0, 0, 0, true);
    T.ox = ax, T.oy = ay;
    if (DR.barup_t >= 0) lp_parts(g_drp, dr_at(DG_BARUP, DR.barup_t), 0, 0, 0, true);
    if (full) lp_parts(g_drp, dr_at(DG_FULL, DR.full_t), 0, 0, 0, true);
    T.ox = sox, T.oy = soy;
    orb_draw(dt, true);  // (s139 sumplate: the summon's orb plate + face, over the digits as KH2's 7000 / 13000 priorities)
}

// ------------------------------------------------------------------ the GPU side
static const char *VERT_SRC =
    "#version 450\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 1) in vec2 uv;\n"
    "layout(location = 2) in vec4 col;\n"
    "layout(location = 3) in vec2 loc;\n"
    "layout(location = 0) out vec2 v_uv;\n"
    "layout(location = 1) out vec4 v_col;\n"
    "layout(location = 2) out vec2 v_loc;\n"
    "void main() { v_uv = uv; v_col = col; v_loc = loc; gl_Position = vec4(pos, 0.0, 1.0); }\n";
// pc.a = (cut, L, rmid, mode). mode 1: HP path clip - path coordinate s = x + L along the bar (x < 0, y > 0), then
// L + angle * rmid around the ring from 6 o'clock counter-clockwise on screen (6 -> 3 -> 12 -> 9); keep s >= cut.
// mode 2: MP bar, keep x >= cut. mode 3: the face, keep inside the circle of radius cut. mode 4: additive (no clip).
// (s57 party) mode 5: KH2's CIRCLE_MASK on a party plate's arc: keep the angle from 6 o'clock atan2(|x|, y) <= cut.
// (s67 drive) mode 6: KH2's SECTOR_MASK on the Drive fill, pc.a = (angle, pivot x, pivot y): drop what falls inside the
// 50 x 120 rectangle [-50, 0] x [-120, 0] around the pivot turned by angle (hudbake DriveKeep is the same test).
static const char *FRAG_SRC =
    "#version 450\n"
    "layout(set = 0, binding = 0) uniform sampler2D tex;\n"
    "layout(push_constant) uniform P { vec4 a; } pc;\n"
    "layout(location = 0) in vec2 v_uv;\n"
    "layout(location = 1) in vec4 v_col;\n"
    "layout(location = 2) in vec2 v_loc;\n"
    "layout(location = 0) out vec4 o;\n"
    "void main() {\n"
    "    vec4 c = texture(tex, v_uv) * v_col;\n"
    "    float mode = pc.a.w;\n"
    "    if (mode > 0.5 && mode < 1.5) {\n"
    "        float s;\n"
    "        if (v_loc.x < 0.0 && v_loc.y > 0.0) s = v_loc.x + pc.a.y;\n"
    "        else { float th = atan(v_loc.x, v_loc.y); if (th < 0.0) th += 6.2831853; s = pc.a.y + th * pc.a.z; }\n"
    "        if (s < pc.a.x) discard;\n"
    "    } else if (mode > 1.5 && mode < 2.5) {\n"
    "        if (v_loc.x < pc.a.x) discard;\n"
    "    } else if (mode > 2.5 && mode < 3.5) {\n"
    "        if (dot(v_loc, v_loc) > pc.a.x * pc.a.x) discard;\n"
    "    } else if (mode > 4.5 && mode < 5.5) {\n"
    "        if (atan(abs(v_loc.x), v_loc.y) > pc.a.x) discard;\n"
    "    } else if (mode > 5.5 && mode < 6.5) {\n"
    "        vec2 d = v_loc - pc.a.yz; float cs = cos(pc.a.x), sn = sin(pc.a.x);\n"
    "        vec2 l = vec2(cs * d.x + sn * d.y, -sn * d.x + cs * d.y);\n"
    "        if (l.x >= -50.0 && l.x <= 0.0 && l.y >= -120.0 && l.y <= 0.0) discard;\n"
    "    }\n"
    "    if (c.a < 0.004) discard;\n"
    "    float a = min(c.a, 1.0);\n"  // premultiplied (src ONE): mode 4 = additive (dst kept, alpha 0)
    "    o = vec4(min(c.rgb, vec3(1.0)) * a, mode > 3.5 && mode < 4.5 ? 0.0 : a);\n"
    "}\n";

// (s78 camp) the pause menu's shader pair: CVtx (NDC pos, uv / wrap in 0..1 of the logical sheet, colour = byte / 128).
// wrap = (l, r, t, b): u = l + mod(u - l, r - l) when r > l (OpenKH's UV scroll), same for v. pc.a.x = 1: solid colour
// (tex 2, no texture). The output is premultiplied like the HUD's: the three pipelines differ in the blend only.
static const char *CAMP_VERT_SRC =
    "#version 450\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 1) in vec2 uv;\n"
    "layout(location = 2) in vec4 col;\n"
    "layout(location = 3) in vec4 wrap;\n"
    "layout(location = 0) out vec2 v_uv;\n"
    "layout(location = 1) out vec4 v_col;\n"
    "layout(location = 2) flat out vec4 v_wrap;\n"
    "void main() { v_uv = uv; v_col = col; v_wrap = wrap; gl_Position = vec4(pos, 0.0, 1.0); }\n";
static const char *CAMP_FRAG_SRC =
    "#version 450\n"
    "layout(set = 0, binding = 0) uniform sampler2D tex;\n"
    "layout(push_constant) uniform P { vec4 a; } pc;\n"
    "layout(location = 0) in vec2 v_uv;\n"
    "layout(location = 1) in vec4 v_col;\n"
    "layout(location = 2) flat in vec4 v_wrap;\n"
    "layout(location = 0) out vec4 o;\n"
    "void main() {\n"
    "    vec2 uv = v_uv;\n"
    "    if (v_wrap.y > v_wrap.x) uv.x = v_wrap.x + mod(uv.x - v_wrap.x, v_wrap.y - v_wrap.x);\n"
    "    if (v_wrap.w > v_wrap.z) uv.y = v_wrap.z + mod(uv.y - v_wrap.z, v_wrap.w - v_wrap.z);\n"
    "    vec4 c = pc.a.x > 0.5 ? v_col : texture(tex, uv) * v_col;\n"
    "    if (c.a < 0.004) discard;\n"
    "    float a = min(c.a, 1.0);\n"
    "    o = vec4(min(c.rgb, vec3(1.0)) * a, a);\n"
    "}\n";
typedef struct { float x, y, u, v, r, g, b, a, wl, wr, wt, wb; } CVtx;  // 48 B

static struct {
    bool init, failed;
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout layout;
    VkPipeline pipe;
    VkFormat pipe_fmt;
    VkPipeline cpipe[3];  // (s78 camp) normal / add / subtract
    VkSampler sampler;
    VkImage tex[15];  // 0-1 gauge, 2-3 menu, 4 lock-on reticle, 5 the dream picker's labels (s54), 6 the level-up text (s55),
                      // 7-8 Donald's / Goofy's faces (s57 party), 9-10 the camp sheet / sys font (s78 camp), 11 Roxas' face (s111),
                      // 12 Mickey's face (s116), 13 the Keyblade picture atlas (s120), 14 the summon faces (s139)
    VmaAllocation tex_mem[15];
    VkImageView tex_view[15];
    uint32_t ntex;
    uint64_t frames;
} G;

// blend 0 = the HUD's premultiplied (src ONE, dst 1 - SRC_A), 1 = add (ONE / ONE), 2 = subtract (REVERSE_SUBTRACT
// ONE / ONE: dst - src); the window's alpha is kept in all three
static VkPipeline make_pipe(VkFormat fmt, VkShaderModule vs, VkShaderModule fs, uint32_t stride,
                            const VkVertexInputAttributeDescription *attr, uint32_t nattr, int blend) {
    if (!vs || !fs) return VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main"}};
    VkVertexInputBindingDescription bind = {0, stride, VK_VERTEX_INPUT_RATE_VERTEX};
    VkPipelineVertexInputStateCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                               .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &bind,
                                               .vertexAttributeDescriptionCount = nattr, .pVertexAttributeDescriptions = attr};
    VkPipelineInputAssemblyStateCreateInfo ia = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                 .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo vps = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                 .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .lineWidth = 1};
    VkPipelineMultisampleStateCreateInfo ms = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                               .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineColorBlendAttachmentState cba = {.blendEnable = VK_TRUE, .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,  // (premultiplied)
                                               .dstColorBlendFactor = blend ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                               .colorBlendOp = blend == 2 ? VK_BLEND_OP_REVERSE_SUBTRACT : VK_BLEND_OP_ADD,
                                               .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                                               .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .alphaBlendOp = VK_BLEND_OP_ADD,
                                               .colorWriteMask = 0xF};
    VkPipelineColorBlendStateCreateInfo cb = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1,
                                              .pAttachments = &cba};
    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dyn};
    // colour only: drawn inside device.c's window image rendering (no depth attachment there)
    VkPipelineRenderingCreateInfo ri = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1,
                                        .pColorAttachmentFormats = &fmt};
    VkGraphicsPipelineCreateInfo pi = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri, .stageCount = 2,
                                       .pStages = stages, .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vps,
                                       .pRasterizationState = &rs, .pMultisampleState = &ms, .pColorBlendState = &cb,
                                       .pDynamicState = &dy, .layout = G.layout};
    VkPipeline p = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(g_vk.dev, g_vk.pcache, 1, &pi, NULL, &p) != VK_SUCCESS) return VK_NULL_HANDLE;
    return p;
}

static VkPipeline make_pipeline(VkFormat fmt) {
    static VkShaderModule vs, fs;
    if (!vs) vs = vk_compile_glsl(VERT_SRC, STAGE_VERTEX, "sora_hud.vert");
    if (!fs) fs = vk_compile_glsl(FRAG_SRC, STAGE_FRAGMENT, "sora_hud.frag");
    static const VkVertexInputAttributeDescription attr[4] = {{0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
                                                              {1, 0, VK_FORMAT_R32G32_SFLOAT, 8},
                                                              {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16},
                                                              {3, 0, VK_FORMAT_R32G32_SFLOAT, 32}};
    return make_pipe(fmt, vs, fs, sizeof(Vtx), attr, 4, 0);
}

static VkPipeline make_camp_pipeline(VkFormat fmt, int blend) {  // (s78 camp)
    static VkShaderModule vs, fs;
    if (!vs) vs = vk_compile_glsl(CAMP_VERT_SRC, STAGE_VERTEX, "sora_camp.vert");
    if (!fs) fs = vk_compile_glsl(CAMP_FRAG_SRC, STAGE_FRAGMENT, "sora_camp.frag");
    static const VkVertexInputAttributeDescription attr[4] = {{0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
                                                              {1, 0, VK_FORMAT_R32G32_SFLOAT, 8},
                                                              {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16},
                                                              {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 32}};
    return make_pipe(fmt, vs, fs, sizeof(CVtx), attr, 4, blend);
}

static bool gpu_init(void) {
    VkDescriptorSetLayoutBinding b = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL};
    VkDescriptorSetLayoutCreateInfo li = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &b};
    VK_CHECK(vkCreateDescriptorSetLayout(g_vk.dev, &li, NULL, &G.set_layout));
    VkPushConstantRange r = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16};
    VkPipelineLayoutCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1,
                                     .pSetLayouts = &G.set_layout, .pushConstantRangeCount = 1, .pPushConstantRanges = &r};
    VK_CHECK(vkCreatePipelineLayout(g_vk.dev, &ci, NULL, &G.layout));
    VkSamplerCreateInfo si = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR,
                              .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST, .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .maxLod = 0};
    VK_CHECK(vkCreateSampler(g_vk.dev, &si, NULL, &G.sampler));
    // the textures (RGBA8 as baked): the gauge's two, plus the menu's two when cmd.bin loaded; uploaded by the
    // submission's upload command buffer
    VkCommandBuffer ucb = vk_upload_cmd();
    G.ntex = 15;
    for (uint32_t i = 0; i < G.ntex; i++) {
        if ((i == 2 || i == 3) && g_cmd_state != 1) continue;
        if (i == 4 && g_lk_state != 1) continue;  // (texture 4 = the lock-on sheet, when lock.bin loaded)
        if (i == 5 && g_dm_state != 1) continue;  // (s54 dream: texture 5 = dream.bin's atlas)
        if (i == 6 && g_lv_state != 1) continue;  // (s55 HUD: texture 6 = lvup.bin's text atlas)
        if ((i == 7 || i == 8) && g_pt_state != 1) continue;  // (s57 party: textures 7 / 8 = party.bin's faces)
        if (i >= 9 && i <= 10 && g_cp_state != 1) continue;
        if (i == 11 && g_rf.state != 1) continue;  // (s111 roxhud: texture 11 = roxface.bin)
        if (i == 12 && g_mf.state != 1) continue;  // (s116 mkface: texture 12 = mkface.bin)  // (s78 camp: textures 9 / 10 = camp.bin's sheet / font)
        if (i == 14 && g_sf.state != 1) continue;  // (s139 sumplate: texture 14 = sumface.bin's atlas)
        if (i == 13 && g_ip_state != 1) continue;  // (s120 itempic: texture 13 = itempic.bin's atlas)
        HImg lk = {0};
        if (i == 4) lk.w = g_lkh->w, lk.h = g_lkh->h, lk.off = g_lk_sheet;
        if (i == 5) lk.w = g_dm_w, lk.h = g_dm_h, lk.off = g_dm_off;
        if (i == 6) lk.w = g_lvh->w, lk.h = g_lvh->h, lk.off = g_lv_atlas;
        if (i == 7 || i == 8) lk.w = g_pth->face_w, lk.h = g_pth->face_h, lk.off = g_pt_face + (i - 7) * 4 * g_pth->face_w * g_pth->face_h;
        if (i == 11) lk.w = g_rf.w, lk.h = g_rf.h, lk.off = 16;
        if (i == 12) lk.w = g_mf.w, lk.h = g_mf.h, lk.off = 16;
        if (i == 14) lk.w = g_sf.w, lk.h = g_sf.h, lk.off = g_sf.off;
        if (i == 13) lk.w = g_ip_w, lk.h = g_ip_h, lk.off = g_ip_off;
        if (i >= 9 && i <= 10) lk.w = g_cpimg[i - 9].w, lk.h = g_cpimg[i - 9].h, lk.off = g_cpimg[i - 9].off;
        const HImg *t = i < 2 ? &g_img[i] : i < 4 ? &g_cimg[i - 2] : &lk;
        const uint8_t *src = (i == 14 ? g_sf.buf : i == 11 ? g_rf.buf : i == 12 ? g_mf.buf : i == 13 ? g_ip : i < 2 ? g_bake : i < 4 ? g_cmd : i == 4 ? g_lk : i == 5 ? g_dm : i == 6 ? g_lv : i < 9 ? g_pt : g_cp) + t->off;
        VkImageCreateInfo ic = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM,
                                .extent = {t->w, t->h, 1}, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
                                .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};
        VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
        VK_CHECK(vmaCreateImage(g_vk.vma, &ic, &ai, &G.tex[i], &G.tex_mem[i], NULL));
        vk_image_init_layout(ucb, G.tex[i], VK_IMAGE_ASPECT_COLOR_BIT);
        VkSpan s = vk_transient_copy(src, (uint64_t)t->w * t->h * 4, 16);
        VkBufferImageCopy rg = {.bufferOffset = s.off, .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {t->w, t->h, 1}};
        rec_CmdCopyBufferToImage(ucb, s.buf, G.tex[i], VK_IMAGE_LAYOUT_GENERAL, 1, &rg);
        VkImageViewCreateInfo vc = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = G.tex[i], .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = VK_FORMAT_R8G8B8A8_UNORM, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        VK_CHECK(vkCreateImageView(g_vk.dev, &vc, NULL, &G.tex_view[i]));
    }
    vk_full_barrier(ucb);
    return true;
}

// ------------------------------------------------------------------ the frame
// Display state (render thread): the dark red ghost of lost HP eases down to the current HP after a short hold, the
// low-HP flash alternates the green fill with the red pieces (KH2: "the gauge will flash red" at critical HP).
static struct { float ghost_hp; int hold; bool valid; } D;
// The two icons' frames (render thread, one per present; [0] auto, [1] lock): show.. when its kind comes on, looping
// loop..rel-1 while it stays; when it goes off (another kind or none), rel..end-1 once at the last place, then nothing.
// Auto -> lock = the yellow mark hides while the lock flies in, as KH2 does it.
static struct { bool active, releasing; uint32_t f; float w[3]; } LK[2];

bool sora_draw_project(const float w[3], float *sx, float *sy);  // (sora_draw.c: this frame's camera)

// (s78 camp) the pause menu: KH2's 512 x 416 virtual frame centred in the game image (s_y = vh / 416, s_x = s_y * 416 /
// 384: KH2's pixel aspect, 16:9 = x -85..597), its own vertex array, a batch per texture / blend change
static CVtx g_cvtx[6 * CQ_MAX];
// menu = the pause menu's quads (sora_hud_quads_shown + the log); false = sora_hud_label's
static void camp_draw(const SoraHudQuad *q, int n, VkExtent2D ext, float vx, float vy, float vw, float vh, bool menu) {
    static VkFormat cfmt;
    if (!G.cpipe[0] || cfmt != g_vk.sc_format) {
        cfmt = g_vk.sc_format;
        for (int b = 0; b < 3; b++) G.cpipe[b] = make_camp_pipeline(cfmt, b);
        if (!G.cpipe[0] || !G.cpipe[1] || !G.cpipe[2]) { G.failed = true; LOG("[sora-hud] camp pipeline failed\n"); return; }
    }
    if (vw <= 0 || vh <= 0) vx = vy = 0, vw = T.W, vh = T.H;
    float sy = vh / 416.0f, sx = sy * 416.0f / 384.0f, cx = vx + vw * 0.5f;
    static struct { int first, n, tex, solid, blend; } bt[CQ_MAX];
    int nb = 0, nv = 0;
    for (int i = 0; i < n; i++, q++) {
        if (q->tex == 3 && g_ip_state != 1) continue;  // (s120 itempic: no atlas = the picture quad is skipped)
        int solid = q->tex == 2, tex = q->tex == 1 ? 10 : q->tex == 3 ? 13 : 9, blend = q->blend <= 2 ? q->blend : 0;
        if (!nb || bt[nb - 1].tex != tex || bt[nb - 1].solid != solid || bt[nb - 1].blend != blend)
            bt[nb].first = nv, bt[nb].n = 0, bt[nb].tex = tex, bt[nb].solid = solid, bt[nb].blend = blend, nb++;
        float iu, iv;
        if (tex == 13) iu = 1.0f / (float)g_ip_w, iv = 1.0f / (float)g_ip_h;  // (s120 itempic: u / v in atlas texels)
        else { const CpImg *im = &g_cpimg[tex - 9]; iu = 1.0f / (float)im->lw, iv = 1.0f / (float)im->lh; }
        CVtx c[4];
        for (int k = 0; k < 4; k++) {
            uint32_t col = q->col[k];
            c[k] = (CVtx){(cx + (q->x[k] - 256.0f) * sx) / T.W * 2 - 1, (vy + q->y[k] * sy) / T.H * 2 - 1, q->u[k] * iu, q->v[k] * iv,
                          (float)(col & 0xFF) / 128.0f, (float)(col >> 8 & 0xFF) / 128.0f, (float)(col >> 16 & 0xFF) / 128.0f,
                          (float)(col >> 24) / 128.0f, q->wrap[0] * iu, q->wrap[1] * iu, q->wrap[2] * iv, q->wrap[3] * iv};
        }
        // corners TL TR BL BR: (TL TR BL) (TR BR BL)
        g_cvtx[nv++] = c[0], g_cvtx[nv++] = c[1], g_cvtx[nv++] = c[2];
        g_cvtx[nv++] = c[1], g_cvtx[nv++] = c[3], g_cvtx[nv++] = c[2];
        bt[nb - 1].n += 6;
    }
    VkCommandBuffer cb = vk_cmd();
    VkViewport vp = {0, 0, T.W, T.H, 0, 1};
    VkRect2D sc = {{0, 0}, ext};
    rec_CmdSetViewport(cb, 0, 1, &vp);
    rec_CmdSetScissor(cb, 0, 1, &sc);
    VkSpan vb = vk_transient_copy(g_cvtx, (uint64_t)nv * sizeof(CVtx), 16);
    VkDeviceSize vo = vb.off;
    rec_CmdBindVertexBuffers(cb, 0, 1, &vb.buf, &vo);
    int last_tex = -1, last_blend = -1;
    for (int i = 0; i < nb; i++) {
        if (bt[i].blend != last_blend) rec_CmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, G.cpipe[last_blend = bt[i].blend]);
        if (bt[i].tex != last_tex) {
            VkDescriptorSet set = vk_alloc_set(G.set_layout);
            VkDescriptorImageInfo ii = {G.sampler, G.tex_view[bt[i].tex], VK_IMAGE_LAYOUT_GENERAL};
            VkWriteDescriptorSet w = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set, .dstBinding = 0, .descriptorCount = 1,
                                      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &ii};
            mod_update_set(&w);
            rec_CmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, G.layout, 0, 1, &set, 0, NULL);
            last_tex = bt[i].tex;
        }
        float pc[4] = {(float)bt[i].solid, 0, 0, 0};
        rec_CmdPushConstants(cb, G.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16, pc);
        rec_CmdDraw(cb, (uint32_t)bt[i].n, 1, (uint32_t)bt[i].first, 0);
    }
    vk_dyn_forget();  // (exec.c / the overlay menu: pipeline and dynamic state changed)
    if (!menu) return;
    pthread_mutex_lock(&g_lock);
    g_cq_drawn = vk_frame_counter();
    pthread_mutex_unlock(&g_lock);
    if (G.frames % 600 == 1) LOG("[sora-hud] frame %llu: camp %d quads, %d batches\n", (unsigned long long)G.frames, n, nb);
}

// (mp Task 7) the label: its quads moved from around 0, 0 onto the anchor's projection (render thread, after the HUD)
static void label_draw(SoraHudQuad *q, int n, const float w[3], VkExtent2D ext, float vx, float vy, float vw, float vh) {
    float px, py, ox, oy;
    if (n <= 0 || g_cp_state != 1 || !sora_draw_project(w, &px, &py)) return;
    hud_label_origin(px, py, vx, vy, vw, vh, T.W, T.H, &ox, &oy);
    for (int i = 0; i < n; i++)
        for (int k = 0; k < 4; k++) q[i].x[k] += ox, q[i].y[k] += oy;
    camp_draw(q, n, ext, vx, vy, vw, vh, false);
    if (G.frames % 600 == 1) LOG("[sora-hud] frame %llu: label %d quads at %.0f %.0f px\n", (unsigned long long)G.frames, n, px, py);
}

void sora_hud_present(float vx, float vy, float vw, float vh) {
    if (g_state < 0 || G.failed) return;
    Vals in;
    SoraHudMenu m;
    SoraHudDream dm;  // (s54 dream)
    int lk_kind;
    float lk_w[3];
    pthread_mutex_lock(&g_lock);
    in = g_in;
    m = g_menu;
    dm = g_dream_in;
    if (dm.on && vk_frame_counter() - g_dream_at > 3) dm.on = false;  // (s54 dream: the DLL stopped sending it)
    // (s55 HUD) level-up: a drop request; a new batch from the queue when none shows (only while the gauge is up)
    bool lv_drop = g_lv_drop;
    g_lv_drop = false;
    SoraHudLevelUp lv_take[LV_BATCH];
    int lv_ntake = 0;
    if (g_lv_state < 0) g_lv_nq = 0;  // (no lvup.bin: nothing to wait for)
    else if (g_lv_state == 1 && in.on && (lv_drop || !LVB.n) && g_lv_nq) {
        lv_ntake = g_lv_nq < LV_BATCH ? g_lv_nq : LV_BATCH;
        memcpy(lv_take, g_lv_q, sizeof lv_take[0] * (size_t)lv_ntake);
        memmove(g_lv_q, g_lv_q + lv_ntake, sizeof g_lv_q[0] * (size_t)(g_lv_nq - lv_ntake));
        g_lv_nq -= lv_ntake;
    }
    lk_kind = g_lk_in.kind;
    typeof(g_en_in) en = g_en_in;
    typeof(g_en_place) enp = g_en_place;  // (s92 hud)
    PtIn pt[2];  // (s57 party) the members, and the live ring placement
    memcpy(pt, g_pt_in, sizeof pt);
    DrIn dr = g_dr_in;  // (s67 drive)
    float dim_a = vk_frame_counter() - g_dim_in.at > 3 ? 0 : g_dim_in.a;  // (s71 dim)
    float place_x = g_place_x, place_y = g_place_y;
    memcpy(lk_w, g_lk_in.w, sizeof lk_w);
    static SoraHudQuad cq[CQ_MAX];  // (s78 camp) this present's quads (render thread only)
    int cq_n, cq_sp = 0;
    if (g_cq_under_fr == vk_frame_counter() + 1) {  // (s94) sora_hud_present_under drew [0, sp) of its snapshot: the rest of it
        cq_sp = g_cq_snap_sp, cq_n = g_cq_snap_n - cq_sp;
        if (cq_n > 0) memcpy(cq, g_cq_snap + cq_sp, sizeof cq[0] * (size_t)cq_n);
        else cq_n = 0;
    } else {  // (no under draw this present: all of them over the actors, the old order; the split is ignored)
        cq_n = vk_frame_counter() - g_cq_at > 3 || g_cp_state < 0 ? 0 : g_cq_n;
        if (cq_n) memcpy(cq, g_cq_in, sizeof cq[0] * (size_t)cq_n);
    }
    bool cq_any = cq_n || cq_sp;
    static SoraHudQuad lb[LB_SLOTS][LB_MAX];  // (mp Task 7, mp4) this present's labels (render thread only)
    float lb_w[LB_SLOTS][3] = {{0}};
    int lb_n[LB_SLOTS], lb_any = 0;
    for (int k = 0; k < LB_SLOTS; k++) {
        lb_n[k] = vk_frame_counter() - g_lb_at[k] > 3 ? 0 : g_lb_n[k];
        if (lb_n[k]) memcpy(lb[k], g_lb_in[k], sizeof lb[k][0] * (size_t)lb_n[k]), memcpy(lb_w[k], g_lb_w[k], sizeof lb_w[k]), lb_any = 1;
    }
    pthread_mutex_unlock(&g_lock);
    if (lv_drop) LVB.n = 0;                        // (s55 HUD: before any early return, so no taken request is lost)
    if (lv_ntake) lvup_start(lv_take, lv_ntake);
    if (!in.on) D.valid = false;
    if (en.max <= 0) E.valid = false;
    if (!in.on && !m.on && !dm.on && !lk_kind && !LK[0].active && !LK[1].active && en.max <= 0 && dim_a <= 0 && !cq_any && !lb_any) return;
    if (!g_state) {
        g_state = bake_load() ? 1 : -1;
        if (g_state == 1) g_cmd_state = cmd_load() ? 1 : -1;  // (the menu rides on the gauge's bake: one pipeline, one sampler)
        if (g_state == 1) g_lk_state = lock_load() ? 1 : -1;  // (the reticle too)
        if (g_state == 1 && g_cmd_state == 1) g_dm_state = dream_load() ? 1 : -1;  // (s54 dream: needs cmd.bin's slots)
        if (g_state == 1) g_lv_state = lvup_load() ? 1 : -1;  // (s55 HUD: its sprites are on the gauge sheet)
        if (g_state == 1) g_pt_state = party_load() ? 1 : -1;  // (s57 party: the plates are on the gauge sheet too)
        if (g_state == 1) g_dr_state = drive_load() ? 1 : -1;  // (s67 drive: the same sheet)
        if (g_state == 1) g_rf.state = face_load(&g_rf, "SAVAGEWIND_SORA_HUD_ROXFACE", "D:/SoraBotW/data/hud/roxface.bin", "Roxas") ? 1 : -1;  // (s111 roxhud)
        if (g_state == 1) g_sf.state = sface_load() ? 1 : -1;  // (s139 sumplate)
        if (g_state == 1) g_mf.state = face_load(&g_mf, "SAVAGEWIND_SORA_HUD_MKFACE", "D:/SoraBotW/data/hud/mkface.bin", "Mickey") ? 1 : -1;  // (s116 mkface)
        if (g_state == 1) g_cp_state = camp_load() ? 1 : -1;   // (s78 camp: its own art, needs only the HUD's GPU setup)
        if (g_state == 1) g_ip_state = itempic_load() ? 1 : -1;  // (s120 itempic: the Equipment page's Keyblade pictures)
    }
    if (g_cp_state != 1) cq_n = 0, cq_any = false;
    if (g_state != 1) return;
    if (g_lk_state == 1)
        for (int i = 0; i < 2; i++) {
            typeof(g_lkh->icon[0]) ic = g_lkh->icon[i];
            if (lk_kind == i + 1) {
                if (!LK[i].active || LK[i].releasing) LK[i].active = true, LK[i].releasing = false, LK[i].f = ic.show;
                else if (++LK[i].f >= ic.rel) LK[i].f = ic.loop;
                memcpy(LK[i].w, lk_w, sizeof LK[i].w);
            } else if (LK[i].active) {
                if (!LK[i].releasing) LK[i].releasing = true, LK[i].f = ic.rel;
                else if (++LK[i].f >= ic.end) LK[i].active = false;
            }
        }
    if (m.on && g_cmd_state != 1) m.on = false;  // no cmd.bin: the menu draws nothing (logged once in cmd_load)
    if (dm.on && g_dm_state != 1) dm.on = false;  // (s54 dream: no dream.bin = no picker, logged once in dream_load)
    if (!G.init) {
        G.init = true;
        if (!gpu_init()) { G.failed = true; LOG("[sora-hud] GPU setup failed\n"); return; }
        return;  // (the textures upload with this submission; draw from the next frame on)
    }
    VkExtent2D ext = vk_frame_extent();
    if (!vk_frame_view() || !ext.width || !ext.height) return;
    if (!G.pipe || G.pipe_fmt != g_vk.sc_format) {
        G.pipe_fmt = g_vk.sc_format;
        G.pipe = make_pipeline(G.pipe_fmt);
        if (!G.pipe) { G.failed = true; LOG("[sora-hud] pipeline failed\n"); return; }
    }
    params_init();
    G.frames++;
    if (cq_any) {  // (s78 camp) the pause menu owns the screen: nothing else of the HUD draws
        T.W = (float)ext.width, T.H = (float)ext.height;
        if (cq_n) camp_draw(cq, cq_n, ext, vx, vy, vw, vh, true);
        return;
    }

    // placement: HUD unit from the game image's height, ring centre from its bottom-right corner
    T.W = (float)ext.width, T.H = (float)ext.height;
    T.unit = (vh > 0 ? vh : T.H) / 448.0f * P.scale;
    T.ox = vx + vw - (place_x > 0 ? place_x : P.pos_x) * T.unit, T.oy = vy + vh - (place_y > 0 ? place_y : P.pos_y) * T.unit;  // (s57: + sora_hud_place)
    // (s57 party) the plates' clock: wall time (KH2's 60 Hz), a hitch or a pause counts at most 0.1 s; a member silent for
    // 30 presents is not shown
    static double pt_last;
    double pt_now = lv_now(), pt_dt = pt_last > 0 ? pt_now - pt_last : 0;
    pt_last = pt_now;
    pt_dt = pt_dt < 0 ? 0 : pt_dt > 0.1 ? 0.1 : pt_dt;
    for (int k = 0; k < 2; k++)
        if (vk_frame_counter() - pt[k].at > 30) pt[k].shown = false;
    if (vk_frame_counter() - dr.at > 30) dr.state = 0;  // (s67 drive)
    g_nvtx = g_nbatch = 0;
    if (dim_a > 0) {  // (s71 dim) first = under everything else: one opaque texel of the MP bar, tinted black
        static const Spr SP_DIM = {184, 54, 184, 54, 0x00000000};
        Spr d = SP_DIM;
        d.tint = (uint32_t)(dim_a * 128.0f + 0.5f) << 24;
        float sox = T.ox, soy = T.oy, su = T.unit;
        T.ox = vx, T.oy = vy, T.unit = 1;
        batch_begin(0, 0, 0, 0);
        quad(&d, 0, 0, vw, vh);
        T.ox = sox, T.oy = soy, T.unit = su;
    }
    // the lock-on icons first (under the gauge and the menu): this frame's parts around the target's projection, in
    // the bake's order (KH2's draw order: the additive glow first), a new batch at each alpha / additive change
    for (int k = 0; k < 2; k++) {
        float lk_x, lk_y;
        if (!LK[k].active || g_lk_state != 1 || !sora_draw_project(LK[k].w, &lk_x, &lk_y)) continue;
        float sox = T.ox, soy = T.oy, sc = (float)g_lkh->w / (float)g_lkh->ps2_w, W = (float)g_lkh->w, H = (float)g_lkh->h;
        T.ox = lk_x, T.oy = lk_y;
        const LPart *p = g_lkparts + g_lkfr[2 * LK[k].f];
        int mode = -1;
        for (uint32_t i = 0; i < g_lkfr[2 * LK[k].f + 1]; i++, p++) {
            int pm = p->blend ? 4 : 0;
            if (pm != mode) batch_begin(4, 0, 0, (float)(mode = pm));
            float u0 = p->l * sc / W, u1 = p->r * sc / W, v0 = p->t * sc / H, v1 = p->b * sc / H;
            if (p->r == p->l) u0 = u1 = (p->l * sc + 0.5f) / W;
            if (p->b == p->t) v0 = v1 = (p->t * sc + 0.5f) / H;
            float cl[4], ct[4], cr[4], cb[4], c[4][4];
            col4(p->col[0], cl), col4(p->col[1], ct), col4(p->col[2], cr), col4(p->col[3], cb);
            for (int j = 0; j < 4; j++)  // (corner = the mean of its side and its top / bottom colour, as pquad)
                c[0][j] = (cl[j] + ct[j]) * 0.5f, c[1][j] = (cr[j] + ct[j]) * 0.5f, c[2][j] = (cr[j] + cb[j]) * 0.5f, c[3][j] = (cl[j] + cb[j]) * 0.5f;
            vtx(p->x[0], p->y[0], u0, v0, c[0]), vtx(p->x[1], p->y[1], u1, v0, c[1]), vtx(p->x[2], p->y[2], u1, v1, c[2]);
            vtx(p->x[0], p->y[0], u0, v0, c[0]), vtx(p->x[2], p->y[2], u1, v1, c[2]), vtx(p->x[3], p->y[3], u0, v1, c[3]);
        }
        T.ox = sox, T.oy = soy;
    }
    float L = 0;
    bool low = false;
    if (in.on) {
        // the ghost: holds 20 frames after a loss, then drops at hp_max per 1.5 s (60 fps); jumps up with healing
        if (!D.valid || D.ghost_hp < in.hp) D.ghost_hp = in.hp, D.hold = 0, D.valid = true;
        else if (D.ghost_hp > in.hp) {
            if (D.hold < 20) D.hold++;
            else { D.ghost_hp -= in.hp_max / 90.0f; if (D.ghost_hp < in.hp) D.ghost_hp = in.hp; }
        }
        low = in.hp > 0 && in.hp <= in.hp_max * 0.25f;
        bool flash = low && (G.frames / 15) & 1;
        L = in.hp_max * P.hp_px;
        float total = L + RING_ARC;
        float cut_fill = total * (1 - in.hp / in.hp_max), cut_ghost = total * (1 - D.ghost_hp / in.hp_max);
        float Lm = in.mp_max * P.mp_px;
        bool charge = in.mp_max > 0 && in.mp < 0;  // KH2 MP Charge look (pink, flashing): mp < 0, -mp = the charge so far

        if (g_pt_state == 1) party_draw(pt, pt_dt);  // (s57 party: under Sora's ring, as KH2 draws them)
        // portrait: faint disc (sprite 127 x4), the face inside a circle, the rim (sprite 128 x4)
        batch_begin(0, 0, L, 0);
        quad(&SP_FRAME_DISC, -FRAME_R, -FRAME_R, 0, 0), quad(&SP_FRAME_DISC, FRAME_R, -FRAME_R, 0, 0);
        quad(&SP_FRAME_DISC, FRAME_R, FRAME_R, 0, 0), quad(&SP_FRAME_DISC, -FRAME_R, FRAME_R, 0, 0);
        batch_begin(g_face_who == 1 && g_rf.state == 1 ? 11 : g_face_who == 2 && g_mf.state == 1 ? 12 : 1, FACE_R, L, 3);
        {
            float c[4] = {1, 1, 1, 1}, r = FACE_R + 1;
            vtx(-r, -r, 0, 0, c), vtx(r, -r, 1, 0, c), vtx(r, r, 1, 1, c);
            vtx(-r, -r, 0, 0, c), vtx(r, r, 1, 1, c), vtx(-r, r, 0, 1, c);
        }
        batch_begin(0, 0, L, 0);
        quad(&SP_FRAME_RIM, -FRAME_R, -FRAME_R, 0, 0), quad(&SP_FRAME_RIM, FRAME_R, -FRAME_R, 0, 0);
        quad(&SP_FRAME_RIM, FRAME_R, FRAME_R, 0, 0), quad(&SP_FRAME_RIM, -FRAME_R, FRAME_R, 0, 0);
        // HP: black ring + bar + end cap (sg 91/92), the red ghost (sg 95/96), the green fill (sg 93/94)
        ring(&SP_RING_BLACK), bar(&SP_BAR_BLACK, L);
        quad(&SP_END_LINE, -RING_OUT, 0, 0, 2);
        if (D.ghost_hp > in.hp) {
            batch_begin(0, cut_ghost, L, 1);
            ring(&SP_RING_RED), bar(&SP_BAR_RED, L);
        }
        if (in.hp > 0) {
            batch_begin(0, cut_fill, L, 1);
            if (flash) ring(&SP_RING_RED), bar(&SP_BAR_RED, L);
            else ring(&SP_RING_GREEN), bar(&SP_BAR_GREEN, L);
        }
        // MP: bar background, cap, label (sg 111, 112, 110), the fill (sprite 122, 6 tall) clipped from its left end
        if (in.mp_max > 0) {
            float mx = P.mp_x, my = P.mp_y;
            batch_begin(0, 0, L, 0);
            quad(charge ? &SP_MP_BG_CHG : &SP_MP_BG, mx - Lm, my - 5, mx, my + 5);
            quad(&SP_MP_CAP, mx - 3, my - 5, mx, my + 5);
            quad(charge ? &SP_MP_LABEL_CHG : &SP_MP_LABEL, mx, my - 5, mx + 24, my + 5);
            float fill = charge ? -in.mp : in.mp;  // (charging: the pink gauge fills up with the charge's progress)
            if (fill > 0 && (!charge || (G.frames / 8) & 1)) {  // (KH2: the Charge gauge flashes)
                batch_begin(0, mx - Lm * (fill / in.mp_max), L, 2);
                quad(charge ? &SP_MP_FILL_CHG : &SP_MP_FILL, mx - Lm, my - 3, mx, my + 3);
            }
        }
        if (g_dr_state == 1) drive_draw(&dr, charge, pt_dt);  // (s67 drive: over the gauge, KH2 creates the plate last)
    }
    // the locked enemy's gauge, from the game image's top-right corner
    if (en.max > 0) {
        float sox = T.ox, soy = T.oy;
        if (enp.on) T.ox = vx + enp.x * T.unit, T.oy = vy + enp.y * T.unit;  // (s92 hud: top-left, where BotW's hearts were)
        else T.ox = vx + vw - P.en_x * T.unit, T.oy = vy + P.en_y * T.unit;
        en_draw(en.life, en.max, enp.on);
        T.ox = sox, T.oy = soy;
    }
    // (s55 HUD) the level-up panels (KH2 priority 9000: over the gauges), top right; their clock = wall time at KH2's
    // 60 Hz, only while the gauge shows (a pause holds them); the batch ends with its first panel's last frame
    {
        double now = lv_now(), dt = LVB.last > 0 ? now - LVB.last : 0;
        LVB.last = now;
        if (dt < 0) dt = 0;
        if (dt > 0.1) dt = 0.1;  // (a hitch or a pause: the panels do not jump)
        if (LVB.n && g_lv_state == 1 && in.on) {
            LVB.t += dt;
            int f = (int)(LVB.t * 60.0);
            if (f >= (int)g_lvg[LVB.grp[0]].n) LVB.n = 0;
            else lvup_draw(f, vx, vy, vw, vh);
        }
    }
    // the command menu, over the gauge (its own origin: KH2's 512 x 416 frame)
    if (dm.on) dream_draw(&dm, vx, vy, vw, vh);  // (s54 dream: the picker replaces the command menu while it is up)
    else if (m.on) menu_draw(&m, vx, vy, vw, vh);
    if (!g_nvtx) { for (int k = 0; k < LB_SLOTS; k++) label_draw(lb[k], lb_n[k], lb_w[k], ext, vx, vy, vw, vh); return; }

    VkCommandBuffer cb = vk_cmd();
    VkViewport vp = {0, 0, T.W, T.H, 0, 1};
    VkRect2D sc = {{0, 0}, ext};
    rec_CmdSetViewport(cb, 0, 1, &vp);
    rec_CmdSetScissor(cb, 0, 1, &sc);
    rec_CmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, G.pipe);
    VkSpan vb = vk_transient_copy(g_vtx, (uint64_t)g_nvtx * sizeof(Vtx), 16);
    VkDeviceSize vo = vb.off;
    rec_CmdBindVertexBuffers(cb, 0, 1, &vb.buf, &vo);
    int last_tex = -1;
    for (int i = 0; i < g_nbatch; i++) {
        const Batch *b = &g_batch[i];
        if (!b->n) continue;
        if (b->tex != last_tex) {
            VkDescriptorSet set = vk_alloc_set(G.set_layout);
            VkDescriptorImageInfo ii = {G.sampler, G.tex_view[b->tex], VK_IMAGE_LAYOUT_GENERAL};
            VkWriteDescriptorSet w = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set, .dstBinding = 0, .descriptorCount = 1,
                                      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &ii};
            mod_update_set(&w);
            rec_CmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, G.layout, 0, 1, &set, 0, NULL);
            last_tex = b->tex;
        }
        rec_CmdPushConstants(cb, G.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16, b->pc);
        rec_CmdDraw(cb, (uint32_t)b->n, 1, (uint32_t)b->first, 0);
    }
    vk_dyn_forget();  // (exec.c / the overlay menu: pipeline and dynamic state changed)
    for (int k = 0; k < LB_SLOTS; k++) label_draw(lb[k], lb_n[k], lb_w[k], ext, vx, vy, vw, vh);  // (mp Task 7, mp4: over the HUD)
    if (G.frames % 600 == 1)
        LOG("[sora-hud] frame %llu: hp %.0f/%.0f (ghost %.0f%s) mp %.0f/%.0f, ring at %.0f %.0f unit %.2f px, bar %.0f u, menu %s cursor %d sub %d/%d, enemy %.0f/%.0f, %d verts\n",
            (unsigned long long)G.frames, in.hp, in.hp_max, D.ghost_hp, low ? ", LOW" : "", in.mp, in.mp_max, T.ox, T.oy, T.unit, L,
            !m.on ? "off" : m.shortcut ? "shortcuts" : "on", m.cursor, m.sub, m.sub_cursor, en.life, en.max, g_nvtx);
    if (G.frames % 600 == 1 && g_pt_state == 1)  // (s57 party)
        LOG("[sora-hud] frame %llu: party Donald %s hp %.0f/%.0f mp %.0f/%.0f, Goofy %s hp %.0f/%.0f mp %.0f/%.0f%s\n", (unsigned long long)G.frames,
            pt[0].shown ? "on" : "off", pt[0].hp, pt[0].hp_max, pt[0].mp, pt[0].mp_max, pt[1].shown ? "on" : "off", pt[1].hp, pt[1].hp_max,
            pt[1].mp, pt[1].mp_max, PT[1].slid ? " (slid)" : "");
}

// (s94 menuwalk) device.c calls this BEFORE sora_draw_present: the camp quads [0, split) of sora_hud_quads2 (backdrop,
// layout, menu rows), so the party's screen-mode actors land over them and the rest (the name plates) over the actors.
// Draws only once the HUD's GPU setup + camp art are up (sora_hud_present does those); nothing while split is 0.
void sora_hud_present_under(float vx, float vy, float vw, float vh) {
    if (g_state != 1 || G.failed || !G.init || !G.pipe || g_cp_state != 1 || G.pipe_fmt != g_vk.sc_format) return;
    VkExtent2D ext = vk_frame_extent();
    if (!vk_frame_view() || !ext.width || !ext.height) return;
    pthread_mutex_lock(&g_lock);
    int n = vk_frame_counter() - g_cq_at > 3 ? 0 : g_cq_n, sp = g_cq_split < n ? g_cq_split : n;
    if (sp > 0) memcpy(g_cq_snap, g_cq_in, sizeof g_cq_snap[0] * (size_t)n);  // (all of them: sora_hud_present draws [sp, n))
    pthread_mutex_unlock(&g_lock);
    if (sp <= 0) return;
    g_cq_snap_n = n, g_cq_snap_sp = sp, g_cq_under_fr = vk_frame_counter() + 1;
    T.W = (float)ext.width, T.H = (float)ext.height;
    camp_draw(g_cq_snap, sp, ext, vx, vy, vw, vh, true);
}

// (s111) live command-menu position from sora_mod.dll ("set cmdx N" / "set cmdy N", KH2 HUD units; default -56 / 0)
void sora_hud_menu_pos(float dx, float dy) { P.menu_dx = dx, P.menu_dy = dy; }

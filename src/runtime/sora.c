// Sora-in-BotW, step 3: the Sora-side driver. KH2 Sora's ACTION locomotion (idle / walk / run / jump / fall / land)
// from the pad, out through Link's velocity servo (runtime/linkpos.c), with Link hidden and his own input blocked.
// The KH2 numbers and their sources: D:/SoraBotW/recon/SORA_DRIVER_RECON.md (section 3: prty row 0, the ACTION_JUMP
// curve, air control, the stick); the per-frame order follows its section 5.2 (motions and attacks come with the bake).
//
//   SAVAGEWIND_SORA=1              Sora mode from the moment Link stands in the world (linkpos_world_frame)
//   SAVAGEWIND_SORA_SHOW_LINK=1    keep Link visible (tests: the screenshots show where the servo puts him)
//   SAVAGEWIND_SORA_SCALE=<k>      KH cm -> BotW units = 0.01 * k (default 1: KH2's own metres)
//   SAVAGEWIND_SORA_INPUT=<from>:<NAME>[@<pct>]:<frames>,...
//                                  a test player's pad, frames counted from world entry (like SAVAGEWIND_LINK_VEL's
//                                  "+" windows): NAME = A B X Y L R ZL ZR PLUS MINUS LEFT UP RIGHT DOWN, STICK_UP /
//                                  DOWN / LEFT / RIGHT, STICK_<deg> (0 = right, 90 = up), RSTICK_* (camera),
//                                  STICK_ENEMY (walk to the nearest enemy within 4 u, stop 1.2 u off it: _ENEMY_STOP=<u>); @pct =
//                                  stick tilt in percent (default 100). It reaches Sora when Sora mode is on and the
//                                  game (Link) when it is off, as a real pad would. A window may end early:
//                                  "~x<-1052" (x y z, < >) once Link's body passes that coordinate.
//   SAVAGEWIND_SORA_WORLD_STICK=1  tests: the stick's up is world +z (right -x) whatever the camera does
//   log: <SAVAGEWIND_LINK_PROBE>.sora.txt, one line per game frame (columns in its header)
//
// Built as its own DLL (build/sora_mod.dll, `make sora`), reloaded by runtime/sora_host.c when it changes.
//
// Buttons by position, as KH2 on a PS2 pad (Switch names): bottom B = jump (KH2 Cross), right A = attack (Circle),
// left Y = dodge roll / guard (Square), top X = reaction (Triangle). Jump, the ground combo and the roll exist.
//
// Motions (step 3 item 3): the offline bake D:/SoraBotW/tools/sorabake writes sora_motion.bin (every mset slot: frames,
// loop, the root curve sampled per frame, range-trigger bits, frame triggers) and sora_params.h (prty / sstm / the
// ptya set). SAVAGEWIND_SORA_MOTION=<path> (default D:/SoraBotW/data/sora_motion.bin). Attacks and the roll move by
// their root curve only (base Sora's ptya has no lunge speeds): the per-frame root delta, rotated by Sora's facing
// (KH entity +z forward, +x left), goes out through the same velocity servo, so collision still stops him.
//   SAVAGEWIND_SORA_COMBO_MASTER=1  KH2's combo gate: a press continues the combo only if the last swing connected or
//                                  Combo Master is equipped (default off since s50 = base Sora: with it on, swings at
//                                  a wall chained at allow_combo, twice as fast as KH2's - recon/WALL_RECON.md 4).
//
// Hits (step 5 part 2): sorabake hitbake writes sora_hits.bin (W_EX010.mset attack_hitbox windows per slot with their
// atkp Power, and the blade's grip / tip per frame in the root-stripped entity frame). While a window of the playing
// attack is live, the blade's sweep since the last frame (5 x 4 points over the quad, as PHH's combat._sweep) is tested
// against each enemy of linkpos's list as an upright cylinder at its feet; one hit per (swing, window, enemy) ->
// linkpos_damage_enemy (BotW's own damage path) with max(1, round(base x Power / 100)), and the combo gate's hit flag.
// The damage type f54 (DAMAGE_ARGS_RECON.md) is 15 = BotW's small weapon hit, and the finisher's for a window whose atkp
// Kind is ComboFinisher / AirComboFinisher (A303, A304, the air finishers), so BotW plays a big reaction.
//   SAVAGEWIND_SORA_HITS=<path>     (default D:/SoraBotW/data/sora_hits.bin; missing: no hits)
//   SAVAGEWIND_SORA_DMG=<base>      BotW damage of a Power 100 swing (A300, default 4: a red Bokoblin has 13)
//   SAVAGEWIND_SORA_FIN_F54=<n>     the finishers' f54 (default 0x14 = large damage; 0x17, 0x16, 0x1b to try)
//   SAVAGEWIND_SORA_HIT_R / _H / _PAD  enemy cylinder radius, height, pad (u; default 0.5, 1.6, 0.3)
//
// Time: one BotW frame at 30 fps = 2 KH2 frames = one KH2 ACTION physics step (PHYS_STEP 2). The game's frames are
// often longer (sora1: ~25 fps, each frame 1.2 console frames of game time, Havok steps that long), so every
// rt_vfr_update runs one ACTION step of kh = 2 * the game time ratio KH frames (framepace_game_ratio: the last
// frame's, it changes at most 3% a frame) and the vertical request becomes u/s over that game time.
//
// Air and ground are Sora's own, from collision, as KH2 does it: Link's state word stays at its ground values
// (0x18000001 / 0x18000002) all through a servo jump (sora1), so a landing is a requested drop the body did not make
// (got > LAND_GOT of the request) and a walk-off is the state's air bit or the body dropping faster than a slope would
// explain while the game owns y.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sora_win.h"  // (<windows.h> on Windows; Linux shims: Deck port)
#include "sora_path.h"  // (Deck port: D:/SoraBotW -> SORA_ROOT)
#include "sora_sites.h"  // (1.8.2 / 1.9.0 addresses: sora_sites() in the exe)
#ifdef _WIN32
#include <psapi.h>
#endif

#include "hle.h"

bool linkpos_link_pos(float p[3]);
int linkpos_link_state(void);
int linkpos_link_life(void);
int linkpos_link_hits(void);
int linkpos_link_ragdoll(void);
uint32_t linkpos_link_state_word(void);
bool linkpos_camera_fwd(float f[2]);
uint64_t linkpos_world_frame(void);
void linkpos_set_velocity(bool on, float vx, float vz);
void linkpos_set_velocity_y(bool on, float vy);
void linkpos_move(float dx, float dy, float dz);
void linkpos_block_link_input(bool on);
void linkpos_hide_link(bool on);
void sora_draw_pose(int slot, float clock, float face, float scale);  // (runtime/sora_draw.c)
void sora_draw_off(void);
void sora_hud_set(float hp, float hp_max, float mp, float mp_max);    // (runtime/sora_hud.c)
void sora_hud_off(void);
int linkpos_enemies(uint64_t *actor, float (*pos)[3], int32_t *life, int max);
void linkpos_damage_enemy(uint64_t actor, int n, int f54);
void linkpos_damage_enemy_ex(uint64_t actor, int n, int f54, int f40);  // f40: 0 none, 1 fire, 2 ice, 3 electric
int linkpos_link_heal(int q);  // quarter hearts (Cure)
static int link_heal(int q);   // (s85f: Cure up to BotW's max hearts, not linkpos's "highest life seen")
static int g_cure_q = 8;       // (s85f user: Cure = 2 hearts) live "set cureq <quarter hearts>", 0 = g_cure_pct
bool linkpos_link_mtx(float m[12]);
// (s87c link face) optional exe export (a newer exe): Link's yaw = Sora's facing while she drives. Missing = harmless.
typedef void (*LinkposSetYawFn)(float yaw);
static int g_linkface = 1;  // live "set linkface 0" = Link keeps his own facing (the old behaviour)
bool linkpos_camera_view(float m[12]);  // (sora_draw.c's camera: view rows, proj near / far / fovy / aspect)
bool linkpos_camera_proj(float p[4]);
void linkpos_set_pad_tap(void (*fn)(uint64_t *b, int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry));
extern uint64_t vk_frame_counter(void);
float framepace_game_ratio(void);
enum { LINK_NONE, LINK_GROUND, LINK_AIR, LINK_OTHER };  // (linkpos.c)
#define PAUSE_STATE (sora_sites()->pause_state)  // (u32: 0 playing, 2 in the pause menu or the map; framepace.c)

// KH2 Sora's numbers (prty, sstm, ACTION_JUMP, the stick, the ptya set; SORA_DRIVER_RECON.md 3.1-3.5): generated by
// the bake from the game files. KH cm, KH frames (60 Hz), radians.
#include "sora_params.h"
#include "sora_params_valor.h"  // (s69 drive: Valor Form's prty / ptya / combo limits, sorabake SORABAKE_FORM=valor)
#include "sora_params_wisdom.h" // (s72 drive: Wisdom Form's, SORABAKE_FORM=wisdom)
#include "sora_params_limit.h"  // (s89 b4-limit: Limit Form's, SORABAKE_FORM=limit; prty / ptya 66)
#include "sora_params_master.h" // (s89 b4-master: Master Form's, SORABAKE_FORM=master)
#include "sora_params_final.h"  // (s89 b4-final: Final Form's, SORABAKE_FORM=final PRTY/PTYA 5 PLRP 0x85)
#include "sora_params_roxas.h"  // (s109 roxas: KH2 Roxas P_EX110, SORABAKE_FORM=roxas PTYA 9; prty row = base Sora GUESS, see sora_roxas.h)
#include "sora_params_mickey.h"  // (s110 b22-mickey: KH2 Mickey P_EX200, SORABAKE_FORM=mickey PRTY/PTYA 28)
#include "sora_params_anti.h"   // (s89 anti: Anti Form's, tools_s89/anti sorabake SORABAKE_FORM=anti, S89_HELPER_ANTI.md)
#include "sora_hud.h"  // (sora_hud_menu: the command menu, helper M)
#include "sora_sfx.h"  // (KH2 sound effects, helper A: S35_HELPER_A_SFX.md)
#include "sora_fx.h"   // (KH2 visual effects, helper X: S35_HELPER_X_FX.md)
// (s131 orbs) a per-slot generation: every DLL spawn bumps it, so a holder of an old handle (orbs, save rings) can tell the slot is now another effect's
static uint32_t g_fx_gen[32];
#include "sora_net.h"  // (multiplayer slice 1: UDP snapshots + events to the other player, docs/plans/2026-11-05-multiplayer-slice1.md)
// (mp Task 8) which of our fx handles the remote has a copy of (its event seq): their moves / kills go to it too
// (final review I1: a kill / fade not yet sent moves to the spill, net_fxo_reuse)
static void net_fxo_spawned(int h, int ev) { net_fxo_reuse(NET.fxo, h, ev); }
static int fx_spawn_g(int id, const float pos[3], float yaw, float scale) {
    int h = sora_fx_spawn(id, pos, yaw, scale);
    if (h >= 0 && h < 32) g_fx_gen[h]++;
    net_fxo_spawned(h, -1);  // (not sent: a reused slot stops sending)
    return h;
}
// (slice 3, defined after net_fill) the fx anchors: the nearest of Sora / Donald / Goofy within NET_ANCHOR_CM
static int net_anchor_event(const float pos[3], float yaw, float apos[3], float *ayaw);  // -> NANC_*, apos / ayaw = the anchored place
static int net_anchor_move(int anchor, const float pos[3], float yaw, float apos[3], float *ayaw);  // -> the anchor to use (NANC_WORLD: body gone / fx far)
static float net_anchor_eps_u(void);
static int fx_event_g(const char *name, const float pos[3], float yaw, float scale) {
    int h = sora_fx_event(name, pos, yaw, scale);
    if (h >= 0 && h < 32) g_fx_gen[h]++;
    int ev = -1, anc = NANC_WORLD;
    if (NET.on) {  // (mp: the remote plays it too)
        float ap[3], ay = yaw;
        if (g_net_anchor && pos) anc = net_anchor_event(pos, yaw, ap, &ay);
        NET.st.anc_ev[anc]++;
        ev = net_note_fx(sora_fx_id(name), anc ? ap : pos, anc ? ay : yaw, scale, anc);
        net_fxo_spawned(h, ev);
        if (ev >= 0 && h >= 0 && h < 32 && (anc || pos)) net_fxo_base(&NET.fxo[h], anc, anc ? ap : pos, anc ? ay : yaw);
    } else net_fxo_spawned(h, ev);
    return h;
}
static NetFxOut *net_fxo_live(int h) { return h >= 0 && h < 32 && NET.on && NET.fxo[h].on && !NET.fxo[h].rep ? &NET.fxo[h] : NULL; }
static void fx_move_g(int h, const float pos[3], float yaw) {
    sora_fx_move(h, pos, yaw);
    NetFxOut *o = net_fxo_live(h);
    if (!o) return;
    // (slice 3) a moving fx keeps the anchor its event chose (netanchor 0: world, every move re-sent as before); a move is
    // queued only when its anchored place changed (net_fxo_place), then rides NET_EVENT_REPEAT SNAPs
    float ap[3], ay = yaw;
    int anc = g_net_anchor ? net_anchor_move(o->anchor, pos, yaw, ap, &ay) : NANC_WORLD;
    if (!net_fxo_place(o, anc, anc ? ap : pos, anc ? ay : yaw, net_anchor_eps_u(), 0.0175f, !g_net_anchor)) NET.st.mv_skip++;
}
static void fx_kill_g(int h) {
    sora_fx_kill(h);
    NetFxOut *o = net_fxo_live(h);
    if (o) o->rep = NET_EVENT_REPEAT, o->flags = NMV_KILL;
}
static void fx_fadeout_g(int h, float frames) {
    sora_fx_fadeout(h, frames);
    NetFxOut *o = net_fxo_live(h);
    if (o) o->rep = NET_EVENT_REPEAT, o->flags = NMV_FADE, o->fade = frames;
}
// (mp) Sora's world sounds go to the remote too; menu / HUD sounds (net_ui_sfx) stay local
static int sfx_event_g(const char *name, float gain) {
    int h = sora_sfx_event(name, gain);
    if (NET.on && !net_ui_sfx(name)) net_note_sfx(sora_sfx_id(name), gain, NULL);
    return h;
}
static int sfx_play_at_g(int id, float gain, const float pos[3]) { int h = sora_sfx_play_at(id, gain, pos); net_note_sfx(id, gain, pos); return h; }
#define sora_sfx_event sfx_event_g
#define sora_sfx_play_at sfx_play_at_g
#define sora_fx_spawn fx_spawn_g
#define sora_fx_event fx_event_g
#define sora_fx_move fx_move_g
#define sora_fx_kill fx_kill_g
#define sora_fx_fadeout fx_fadeout_g
static inline bool fx_mine(int h, uint32_t gen) { return h >= 0 && h < 32 && g_fx_gen[h] == gen; }
void sora_draw_shadow(float floor_y, float strength);  // (sora_draw.c: the blob shadow, helper S)
void sora_draw_ghost(float alpha);  // (sora_draw.c, s48: his body where the scene hides it, e.g. under water)
#include "sora_dream.h"   // (s54 dream: KH2's Dream Weapon picker = dream_weapon / dream_giveup; S54_HELPER_DREAM.md)
static int g_kbpop = 1;  // (s92 b6-formfx) live "set kbpop": the "Keyblade obtained" panel (sora_unlock.h)
static void skt_slot_load(int slot);   // (s110 skate, runtime/sora_skate.h: the skateboard's place per save slot)
static void skt_slot_saved(int slot);
static bool skt_eat_x(void);           // (the input thread: X is the board's near it / riding)
#include "sora_levels.h"  // (s54 levels: KH2 EXP / levels / KH-relative damage, slice 1; S54_HELPER_LV_SLICE1.md)
#include "sora_unlock.h"  // (s89 kb: BotW milestones -> Keyblades owned + the Drive Form gate; S89_HELPER_KB.md)
static bool sm_out(void);  // (s136 summon, sora_summon.h: Chicken Little is out; read by sora_drive.h / sora_form.h)
static void sm_hit_refill(float drain);  // (s148 sumlv, sora_summon.h: a hit Sora lands while a summon is out refills its gauge)
#include "sora_sumlv.h"   // (s148 sumlv: KH2 Summon EXP + Summon Level)
#include "sora_drive.h"   // (s67 drive: KH2's Drive gauge, Drive Forms slice 1; S64_HELPER_DRIVE_RECON.md 6)
#include "sora_valor_lv.h" // (s70: Valor EXP + form levels)
#include "sora_light.h"  // (s121 a1 light: time of day / weather light for Sora + party; S121_HELPER_LIGHTDLL.md)
// s54: the map warp's blue glow (sora_draw.c sora_draw_tint). Looked up in the exe by name, not imported: an exe linked
// before s54 lacks it, and an import would make the hot reload fail; then there is no glow, nothing else changes.
// Live "set wtint 0/1", colour "set wtr/wtg/wtb <x100>", warp-out ramp "set wto0/wto1 <presents since 0x1>", arrival
// fade "set wti0/wti1".
static int g_wtint = 1, g_wt_rgb[3] = {35, 75, 100}, g_wt_out0 = 30, g_wt_out1 = 120, g_wt_in0 = 20, g_wt_in1 = 100;
static void warp_tint(float amount) {
    static void (*fn)(float, float, float, float);
    static int tried;
    if (!tried) {
        tried = 1, fn = (void (*)(float, float, float, float))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_tint");
        LOG("[sora] warp tint: sora_draw_tint %s\n", fn ? "found" : "not in this exe (glow off until the next exe link)");
    }
    if (fn) fn(g_wt_rgb[0] / 100.0f, g_wt_rgb[1] / 100.0f, g_wt_rgb[2] / 100.0f, g_wtint ? amount : 0);
}
static float smooth01(float a, float b, float x) {
    float u = b > a ? (x - a) / (b - a) : x >= b;
    u = u < 0 ? 0 : u > 1 ? 1 : u;
    return u * u * (3 - 2 * u);
}
#define KH_SHORT_HOP (KH_G * 0.5f * 5.0f * 5.0f)  // the short hop's arc: 5.1 cm, top 5 frames later
#define KH_FPS 60.0f
#define LAND_GOT 0.5f         // a fall that moved less than this part of its request was stopped: landed
#define LAND_MIN_REQ 0.5f     // (cm: near the apex the requests are too small to tell)
#define WALKOFF_DROP 3.0f     // cm a frame the body may drop beyond 1.2 x the horizontal move (a slope) on the ground
#define PI 3.14159265358979f

// KH2 motion ids (mset slot = id * 4 + variant; SORA_DRIVER_RECON.md 1.2) - what the renderer will play
enum { MOT_IDLE = 0, MOT_WALK = 1, MOT_RUN = 2, MOT_JUMP = 3, MOT_FALL = 4, MOT_LAND = 5, MOT_ROLL = 209 };
// S32 glide port (recon/S32_glide_port_draft.c): KH2's ACTION 17 hover / 18 glide in Sora's own servo
enum { ACT_IDLE, ACT_JUMP, ACT_GLIDE_HOVER = 17, ACT_GLIDE = 18 };  // KH2 registry ids (re:0x750D30), as in PHH
#define ACT_AIR(a) ((a) == ACT_JUMP || (a) == ACT_GLIDE_HOVER || (a) == ACT_GLIDE)  // ACT_FLAGS bit 0

// fmab Glide records, row = Glide level - 1 (re:0x3E92D0; 03system.bin pref/fmab @0x17A70, stride 0x44, via PHH's
// mined sora_movement.kh.json: recon/S32_HELPER_D_GLIDE_PARAMS.md). KH cm, 60 fps frames, radians.
typedef struct {
    float speed;         // GlideSpeed, cm/frame
    float accel;         // GlideAcceleration: sp = (1-acc)*GlideSpeed + |vel|*acc per physics step
    float fall_ratio;    // GlideFallRatio: sink = (h - FallHeight) * ratio, cm/frame
    float fall_height;   // GlideFallHeight: no sink until he is higher than this above the floor
    float fall_max;      // GlideFallMax: sink cap, cm/frame
    float start_height;  // GlideStartHeight: floor further than this below him -> the glide may start
    float end_height;    // GlideEndHeight: floor nearer than this -> Fall
    float turn;          // GlideTurnSpeed, rad/frame (replaces prty TurnSpeed while gliding, re:0x423B90)
} GlideRow;
static const GlideRow KH_GLIDE_ROWS[5] = {
    /* LV1 */ {16, 0.91f, 0.015f, 0, 3.0f, 60, 10, 0.05235988f},
    /* LV2 */ {20, 0.93f, 0.015f, 30, 2.5f, 60, 10, 0.05235988f},
    /* LV3 */ {24, 0.95f, 0.015f, 30, 2.0f, 60, 10, 0.05235988f},
    /* LV4 */ {32, 0.97f, 0.010f, 30, 1.0f, 60, 10, 0.05235988f},
    /* LV5 */ {64, 0.98f, 0.0f, 30, 0.0f, 20, 10, 0.05235988f},  // (Glide MAX: no sink)
};
// Sora's Glide level (the user: LV3 for now, later synced to Sora's own level as he levels up through the game).
// SAVAGEWIND_SORA_GLIDE_LV=<1-5> at start, live "set glidelv <1-5>".
static int g_glide_lv = 3;
static int g_air_magic = 1;  // (s68) live "set airmagic 0": casts on the ground only (the s67 rule)
static int g_air_once = 1;  // SAVAGEWIND_SORA_AIR_ONCE / live "set aironce": PHH's one-air-combo-per-jump rule
static int glide_lv_now(void);  // (s89 b4-final: g_glide_lv, or Final Form's own Glide level; after sora_final_lv.h)
#define GLIDE_ROW (KH_GLIDE_ROWS[glide_lv_now() - 1])
#define KH_GLIDE_START_HEIGHT GLIDE_ROW.start_height
#define KH_GLIDE_END_HEIGHT GLIDE_ROW.end_height
#define KH_GLIDE_FALL_HEIGHT GLIDE_ROW.fall_height
#define KH_GLIDE_FALL_RATIO GLIDE_ROW.fall_ratio
#define KH_GLIDE_FALL_MAX GLIDE_ROW.fall_max
#define KH_GLIDE_SPEED GLIDE_ROW.speed
#define KH_GLIDE_ACCEL GLIDE_ROW.accel
#define KH_GLIDE_TURN GLIDE_ROW.turn
#define KH_GLIDE_HOVER_BRAKE 0.9f    // re:0x5AAA40 re:0x423650 (known)
#define GLIDE_FLOOR_FAR 1000.0f      // cm: "height above the floor" when the floor is unknown (below the last ground)
#define GLIDE_LAND_FRAMES 3          // frames in a row his sink was stopped: he glided onto something -> landed
enum { SP_NONE, SP_ATTACK, SP_ROLL, SP_DAMAGE, SP_MAGIC, SP_REPEL, SP_DRIVE, SP_GUARD, SP_LIMIT, SP_ITEM };  // (s98 potion: SP_ITEM = KH2's ITEM motion A110, sora_meals.h)  // (s89: SP_LIMIT = a Limit Form command step, sora_limit.h)  // (s68 SP_DRIVE: CHANGEFORM / form entry)  // a motion that owns Sora until it ends or frees him
                                                   // (attack, dodge roll, a hit reaction, a spell's cast)
// hit reactions (PHH hit_reaction_kh2.py, KH2 re:0x3D2FE0): flinch front / back (HIGH), air flinch, blow + its landing
enum { MOT_FLINCH_FRONT = 10, MOT_FLINCH_BACK = 12, MOT_AIR_FLINCH_FRONT = 14, MOT_AIR_FLINCH_BACK = 15,
       MOT_BLOW_FRONT = 16, MOT_BLOW_BACK = 18 };
#define BTN_ATTACK (1ull << 0)  // Switch A
#define BTN_JUMP (1ull << 1)    // Switch B
#define BTN_ROLL (1ull << 3)    // Switch Y
#define BTN_SHORTCUT (1ull << 6)  // Switch L: KH2's L1, the shortcut menu while held
#define BTN_LEFT (1ull << 12)   // d-pad (script_parse names[] order): KH2's command menu
#define BTN_UP (1ull << 13)
#define BTN_RIGHT (1ull << 14)
#define BTN_DOWN (1ull << 15)
#define BTN_MENU (BTN_LEFT | BTN_UP | BTN_RIGHT | BTN_DOWN | BTN_SHORTCUT)
#define BTN_LOCK (1ull << 8)    // Switch ZL: KH2's R1 lock-on toggle (BotW never sees it unless live "set zl 1")
#define BTN_TEST_ENEMY (1ull << 63)  // (SAVAGEWIND_SORA_INPUT STICK_ENEMY: walk to the nearest enemy; not a real button)

static struct {
    int act, motion;
    float clock;            // KH frames in the current motion (for the renderer; no motion data yet)
    float face;             // facing, rad: direction (sin, cos) in BotW (x, z)
    float vel[2];           // horizontal velocity, cm/frame (the air / walk-off carry)
    float hvel[2];          // the jump's take-off velocity (dir * speed at the press)
    float takeoff;          // KH frames until the jump trigger (> 0: the jump motion is winding up on the ground)
    float air, jh, apex, base;  // the arc: time in it, height, apex time, base height (cm, absolute)
    float req, prev_z;      // last step's requested rise and the height before it (cm), for collision results
    bool req_set, ceiling, jump_held, jump_released;  // (+0x120 bits 17 / 18, re:0x3CB680)
    int air_frames;         // frames in a row Link's body was airborne while ACT_IDLE (walk-off check)
    float ground_z, ground_move;  // ACT_IDLE: last frame's height and horizontal move (cm), for the walk-off drop
    float body_xz[2];       // last frame's body x, z (cm): BotW's own pushes count as horizontal move too
    float safe[3][3];       // Link's body (u) on the last 3 grounded, not dropping frames (oldest first): the ledge hold
    int nsafe;
    int edge_swing;         // the swing a ledge hold stopped (its root move is off until the next play): range 29
    float peak, base0;      // highest point of the current jump and its first base (log: a hop moves the base)
    const struct Mot *mot;  // the playing motion's baked data (NULL: not in the bake)
    int special, next;      // SP_*; the motion an attack queues (ptya NextMotionId; 0 = idle)
    int d74, d75;           // combo hit index, finisher index (KH2 actor +0xD74 / +0xD75)
    bool hit;               // the last swing connected (no hits yet: Combo Master stands in)
    float root_cm;          // root move this frame (log)
    int swing;              // counts play()s: a new swing's windows hit again
    int hit_swing, hit_win, nhit;  // the window the hit list belongs to
    uint64_t hit_actor[16]; // enemies this window already hit
    float seg[6];           // last frame's blade (grip, tip) in the world (u), valid while seg_swing == swing
    int seg_swing;
    bool fresh;             // first driven frame: buttons down now are not presses
    bool puppet;            // drawn in Link's pose last frame (his own state: climbing, swimming...)
    bool glide_armed;       // B pressed again in the air and still held: the glide starts once he falls
    bool air_done;          // an air combo already started in this jump: no new one until he lands (PHH kh2ctl.py:2068)
    float wish;             // (s65) KH2's wish yaw: the stick's direction on the last free frame (TSR 7.4)
    bool ng;                // (s68) last ACTION tick ran in a state_no_gravity range (air casts)
    bool press_late;        // (s65) an attack press the gate dropped only because this present's hit came after it
} S;

// ------------------------------------------------------------------ the bake (sora_motion.bin, tools/sorabake)
typedef struct Mot {
    uint16_t slot, frames, loop, flags, nft, start;  // flags: 1 loop, 2 root, 4 root yaw
    const float *root;      // (frames + 1) x {x, y, z, yaw}: root position at frame f relative to frame 0
    const uint32_t *bits;   // frames x range-trigger bits (RB_*)
    const uint8_t *ft;      // nft x {u16 frame, u8 id, u8 n, i16 p1, i16 p2}, frames motion-relative
} Mot;
static Mot g_mots[1024];
static int g_nmots;
static float g_jump_trigger = 4;  // A003's frame trigger 0 ("jump", re:0x40DD70); from the bake when it loads
static bool g_combo_master = false;  // (base Sora; live "set combo 1")

// (s68 drive) a Drive Form's motion bake (sora_form.h: data/forms/valor_motion.bin), laid over g_mots while on
// (s72) one table per form (sora_form.h FM_FORMS); g_fform = the form Sora is in / driving to (read while g_fmots_on)
#define FM_NFORMS 8
enum { FORM_VALOR, FORM_WISDOM, FORM_LIMIT, FORM_MASTER, FORM_FINAL, FORM_ANTI, FORM_ROXAS, FORM_MICKEY };  // (s109 roxas: not a Drive Form: the permanent body until the paraglider, sora_roxas.h)  // (s89 trial: Valor, Wisdom, Limit, Master, Final, Anti = KH2's form ids 1..6 - 1; "set form N")
static Mot g_fmots_tab[FM_NFORMS][128];
static int g_nfmots_tab[FM_NFORMS];
static int g_fform;
#define g_fmots g_fmots_tab[g_fform]
#define g_nfmots g_nfmots_tab[g_fform]
static bool g_fmots_on;
#define IN_VALOR (g_fmots_on && g_fform == FORM_VALOR)
#define IN_WISDOM (g_fmots_on && g_fform == FORM_WISDOM)
#define IN_LIMIT (g_fmots_on && g_fform == FORM_LIMIT)  // (s89 b4-limit)
#define IN_MASTER (g_fmots_on && g_fform == FORM_MASTER)  // (s89 b4-master)
#define IN_ANTI (g_fmots_on && g_fform == FORM_ANTI)  // (s89 anti)
#define IN_ROXAS (g_fmots_on && g_fform == FORM_ROXAS)  // (s109 roxas)
#define IN_MICKEY (g_fmots_on && g_fform == FORM_MICKEY)  // (s110 b22-mickey: KH2 Mickey during the rescue, sora_mickey.h)
static int g_roxcmd = 1;  // (s111 roxcmd) 1 (default) = as Roxas: no MP bar, no Drive gauge, no Magic / Drive rows, no L1 shortcut page; live "set roxcmd 0" = old
#define ROXCMD (g_roxcmd && IN_ROXAS)
static int g_mkmenu = 1;  // (s119 mkmenu) 1 (default) = while Mickey is out the command menu is KH2's Mickey pane (Jump / Pearl / Attack with their button icons); live "set mkmenu 0" = the old Sora menu
static int g_mkpad = 1;   // (s119 mkmenu) the button icons: 1 = PlayStation (Cross / Square / Circle, KH2's own art), 2 = Xbox (A / X / B); live "set mkpad"
#define MKCMD (g_mkmenu && IN_MICKEY)
static bool g_mkpane;  // (s119 review) the exe drew the pane last frame (sora_hud_menu_pad returned 1): only then is Sora's menu input off for Mickey
static int mk_pearl_cost(void);  // (s119: MK_PEARL_MP, sora_mickey.h)
static bool roxcmd_on(void) { return ROXCMD; }
#define IN_GUEST (IN_ROXAS || IN_MICKEY)  // (s110: a guest body: never falls back to Sora's motion slots, no ragdoll retarget, no Drive / magic / glide)
static bool mk_se_ok(void);  // (s110m mickeyfx: his sfx.bin rows / fx.bin elements are installed)
static bool mk_fx_ok(void);
static const char *mk_kbfx_name(const char *kind);  // his Keyblade's hit sparks, NULL = not Mickey / no pack
static bool rc_live_set(const char *key, int v);
static bool rc_eat_x(void);  // (s131 react: the Triangle reaction prompt, sora_react.h)
static bool pl_eat_x(void);  // (s133 limits: the Triangle prompt of a party Limit, sora_plimit.h)
static bool mk_eat_x(void);  // Triangle is D-Charge / Healing Light while Mickey is out: no climb try, no pick-up A
static bool mk_hud(float *hp, float *hpm, float *mp, float *mpm);  // his 60 HP / 100 MP on the ring
static void mk_fx_tick(const float feet[3], float to_u);
static void mk_ball_tick(float kh, float to_u);  // (s117 pearl: Mickey's Pearl ball)
static void mk_hit(int atkp, uint64_t actor);  // a hit of his: Drive gauge
static void mk_dchg_loop(void);  // (s110m review: the held D-Charge's loop, before the motion end check)
#include "sora_wisdom_lv.h" // (s75: Wisdom EXP per kill + form levels; needs IN_WISDOM)
#include "sora_kbfx.h"      // (s89 kbfx: each Keyblade's own swing ribbon + hit sparks; S89_HELPER_KBFX.md)
#include "sora_wpn.h"       // (s128 wpn: Link's equipped BotW melee weapon in Sora's hand; recon/S128_PLAN.md)
#include "sora_limit_lv.h"  // (s89 b4-limit: Limit EXP per Limit finisher + form levels)
#include "sora_master_lv.h" // (s89 b4-master: Master EXP per Drive orb + form levels + Aerial Dodge growth)
#define IN_FINAL (g_fmots_on && g_fform == FORM_FINAL)  // (s89 b4-final)
#include "sora_final_lv.h"  // (s89 b4-final: Final EXP per kill + form levels + Glide growth; needs IN_FINAL)
// (s89 b4-final) the Glide row: base Sora's g_glide_lv; in Final its fmlv growth level (Glide 1,1,2,2,3,3,4 by Final
// LV), never below base Sora's (the user's LV3; design: KH2's base Sora would only have growth - 1). Live "set
// fglidelv <1-5>" pins Final's (0 = auto).
static int g_fglide_lv = 0;
static int glide_lv_now(void) {
    if (!IN_FINAL) return g_glide_lv;
    int lv = g_fglide_lv > 0 ? g_fglide_lv : fl_glide_growth() > g_glide_lv ? fl_glide_growth() : g_glide_lv;
    return lv < 1 ? 1 : lv > 5 ? 5 : lv;
}

// an SKRM bake into out[max]: the slot count, 0 when missing / bad (logged); the file stays loaded (Mot points into it)
static int mot_file_load(const char *path, Mot *out, int max) {
    path = sora_path(path);
    FILE *f = fopen(path, "rb");
    if (!f) { LOG("[sora] no motion bake at %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = n > 16 ? malloc((size_t)n) : NULL;
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n || memcmp(b, "SKRM", 4)) {
        LOG("[sora] bad motion bake %s\n", path);
        fclose(f), free(b);
        return 0;
    }
    fclose(f);
    uint32_t count;
    memcpy(&count, b + 8, 4);
    int k = 0;
    for (uint32_t i = 0; i < count && k < max && 16 + (i + 1) * 24 <= (uint32_t)n; i++) {
        const uint8_t *d = b + 16 + i * 24;
        Mot *m = &out[k++];
        uint32_t off[3];
        memcpy(&m->slot, d, 8), memcpy(off, d + 8, 12), memcpy(&m->nft, d + 20, 4);
        m->root = off[0] ? (const float *)(b + off[0]) : NULL;
        m->bits = (const uint32_t *)(b + off[1]);
        m->ft = b + off[2];
    }
    LOG("[sora] motion bake %s: %d slots\n", path, k);
    return k;
}

static void bake_load(void) {
    const char *path = getenv("SAVAGEWIND_SORA_MOTION");
    if (!path || !*path) path = "D:/SoraBotW/data/sora_motion.bin";
    path = sora_path(path);
    g_nmots = mot_file_load(path, g_mots, 1024);
    if (!g_nmots) LOG("[sora] no motion bake: locomotion only\n");
}

static const Mot *mot_find(const Mot *t, int n, int slot) {
    for (int i = 0; i < n; i++)
        if (t[i].slot == slot) return &t[i];
    return NULL;
}

static const Mot *mot_slot(int slot) {
    const Mot *m = g_fmots_on ? mot_find(g_fmots, g_nfmots, slot) : NULL;  // (s68: the form's slot first)
    return m || IN_GUEST ? m : mot_find(g_mots, g_nmots, slot);  // (s109 roxas: never Sora's slot)
}

// KH2's variant fallback, field first: slot = id * 4 + 3, 2, 0, 1. In a form every variant of the form's bake comes
// before the base table (s68c: base Sora's field walk 7 / run 11 beat Valor's battle 4 / 8 = a pose Valor lacks)
static const Mot *mot_of(int id) {
    static const int var[] = {3, 2, 0, 1};
    const Mot *m;
    if (g_fmots_on)
        for (int i = 0; i < 4; i++)
            if ((m = mot_find(g_fmots, g_nfmots, id * 4 + var[i]))) return m;
    if (IN_GUEST) return NULL;  // (s109 roxas: only Roxas' own motions; a slot he lacks (roll, glide, magic, ...) = that action is off, never Sora's)
    for (int i = 0; i < 4; i++)
        if ((m = mot_find(g_mots, g_nmots, id * 4 + var[i]))) return m;
    return NULL;
}

static uint32_t mot_bits(const Mot *m, float t) {
    if (!m || !m->frames) return 0;
    int f = (int)t;
    return m->bits[f < 0 ? 0 : f >= m->frames ? m->frames - 1 : f];
}

static void mot_root(const Mot *m, float t, float r[3]) {  // linear between the baked frames
    r[0] = r[1] = r[2] = 0;
    if (!m || !m->root || !m->frames) return;
    if (t < 0) t = 0;
    if (t > m->frames) t = m->frames;
    int i = (int)t;
    if (i >= m->frames) i = m->frames - 1;
    float u = t - (float)i;
    for (int c = 0; c < 3; c++) r[c] = m->root[i * 4 + c] + (m->root[(i + 1) * 4 + c] - m->root[i * 4 + c]) * u;
}

static float mot_frame_trigger(const Mot *m, int id, float dflt) {
    for (int i = 0; m && i < m->nft; i++) {
        uint16_t fr;
        memcpy(&fr, m->ft + i * 8, 2);
        if (m->ft[i * 8 + 2] == id) return fr;
    }
    return dflt;
}

// ------------------------------------------------------------------ hits (sora_hits.bin, sorabake hitbake)
typedef struct { uint16_t start, end, atkp, group, power, kind; uint8_t reaction, effect, pad[2]; } HitWin;
typedef struct { uint16_t slot, nwin, nseg; const HitWin *win; const float *seg; } HitSlot;
static HitSlot g_hslots[64];
static int g_nhslots;
// (s69 drive slice 3) Valor Form's two blades: right (Kingdom Key W_EX010_BTLF_R on bone 178) and left (Star Seeker
// W_EX010_BTLF_L on bone 86), sorabake hitbake with the VALOR BAKE env -> data/forms/valor_hits_r.bin / _l.bin
static HitSlot g_vhslots[2][64];
static int g_nvhslots[2];
static HitSlot g_lhslots[64];  // (s89 b4-limit: Limit Form, W_EX010_KH1F on 178 + the body's own windows: data/forms/limit_hits.bin)
static int g_nlhslots;
static HitSlot g_mhslots[2][64];  // (s89 b4-master: Master's right / left blade, data/forms/master_hits_r / _l.bin)
static int g_nmhslots[2];
// (s89 b4-final) Final Form's two floating blades: W_EX010 + W_EX010_ULTF_R on bone 178 / W_EX010_30 + W_EX010_ULTF_L on
// bone 86 (KH2 floats both off the hand bones: PHH weapon_slots.py), sorabake hitbake -> data/forms/final_hits_r / _l.bin
static HitSlot g_fhslots[2][64];
static int g_nfhslots[2];
// (s89 anti) Anti Form's claws / kicks: KH2's body-collision windows (no weapon), tools_s89/anti sorabake bodyhits ->
// data/forms/anti_hits_r.bin (track 0) / _l.bin (track 1, windows that overlap track 0's in time), run as two "blades"
static HitSlot g_ahslots[2][64];
static int g_nahslots[2];
static HitSlot g_rhslots[64];  // (s109 roxas) Roxas' Keyblade (W_EX010 + W_EX010_RX.mset on his bone 178): data/forms/roxas_hits.bin
static int g_nrhslots;
static HitSlot g_mkhslots[64];  // (s110 b22-mickey) Mickey's Keyblade (W_EX200 on his bone 178): data/forms/mickey_hits.bin
static int g_nmkhslots;
// (s110 mkhit) Mickey's swings did not connect / chain (user live test): live "set mkhit 0" = the old rules; "set mkhitlog 1" logs his sweep;
// "set mkreach <u*100>" extra sweep radius (default 35 = 0.35 u), "set mkstop <cm>" the hop's stop gap (default 20; KH2's Sora body is 55)
static int g_mkhit = 1, g_mkhitlog = 0, g_mk_gcombo = 0;
static float g_mk_reach = 0.35f, g_mk_stop = 20.0f;
static int mk_str_delta(void);  // (s110: Mickey's KH2 Strength, sora_mickey.h)
static float g_mp_charge_time = 3000;  // prty MpChargeTime, KH frames (03system.bin@0x17CFC); env / live "set mpcharge"
static float g_cure_pct = 30;          // Cure: % of Link's max life (UNVERIFIED, L 2.2); env SAVAGEWIND_SORA_CURE_PCT
static float g_dmg_base = 4, g_hit_r = 0.5f, g_hit_h = 1.6f, g_hit_pad = 0.3f;
static int g_fin_f54 = 0x14;
static int g_fin_big = 0;  // (s65) live "set finbig 1": finishers fling big enemies too (blade_hits)
#define F54_SMALL 15
#define ATKP_KIND_NORMAL 0  // (1 ComboFinisher, 2 AirComboFinisher)
static uint64_t g_fin_hit_actor, g_fin_hit_at;  // (s138 review) the enemy Sora's last finisher (atkp Kind != 0) hit + vk_frame_counter() then: Peter Pan's attack_blow (sora_summon_pp.h)
static int ab_finisher_dmg(int kind, int dmg);  // (s82, after the F_COMBO macros)
static bool berserk_on(void);  // (s106, after the F_COMBO macros)

static float env_f(const char *name, float dflt) {
    const char *v = getenv(name);
    return v && *v ? strtof(v, NULL) : dflt;
}

static int hits_file(const char *path, HitSlot *tab, int *ntab) {  // (s69: one SKRH file -> a slot table; 0 = none)
    path = sora_path(path);
    FILE *f = fopen(path, "rb");
    if (!f) { LOG("[sora] no hit bake at %s: swings don't hit\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = n > 16 ? malloc((size_t)n) : NULL;
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n || memcmp(b, "SKRH", 4)) {
        LOG("[sora] bad hit bake %s\n", path);
        fclose(f), free(b);
        return 0;
    }
    fclose(f);
    uint32_t count;
    memcpy(&count, b + 8, 4);
    for (uint32_t i = 0; i < count && *ntab < 64 && 16 + (i + 1) * 16 <= (uint32_t)n; i++) {
        const uint8_t *d = b + 16 + i * 16;
        HitSlot *h = &tab[(*ntab)++];
        uint32_t off[2];
        memcpy(&h->slot, d, 6), memcpy(off, d + 8, 8);
        h->win = (const HitWin *)(b + off[0]), h->seg = (const float *)(b + off[1]);
    }
    return *ntab;
}

// (s91) Final Form's 60 / 120 fps motions: until s91 sorabake turned every motion's length into GFR with a fixed x2 (right
// for 30 fps only), so Final's E301 / E302 / E303 / run / jump / casts (24 slots) ran 2x / 4x too long: the pose stopped at
// the real end and HELD (E303 = 560 frames, 7 s frozen, no hit window, no free frame: the "freeze" / "jump locked" bug).
// The fixed bakes are final_*_s91.bin (tools_s91/final, x 60 / fps). Live: set ffps 0 = the old files (refused in a Drive
// Form; it reloads Final's motions, hits and draw actor). Kept over a hot reload by the env SORA_FFPS.
static int g_ffps = -1;
// (s92 ffin) Final's ground finishers (E302 / E303) did not connect: final_hits_{r,l}_s92.bin = the _s91 bakes + KH2's atkp
// Interval re-hits (pad[1]), the blade's attack-sphere radius (pad[0]) and E303's body windows (atkp 1456 / 1737, track 0).
// Used when present; live "set ffin 0" = the _s91 hits, "set ffin 1" = _s92 (reloads Final's hits). Env SORA_FFIN=0 = off.
static int g_ffin = -1;
static const char *ffps_path(const char *p) {
    p = sora_path(p);
    if (g_ffps < 0) { const char *e = getenv("SORA_FFPS"); g_ffps = !(e && *e == '0'); }
    if (!g_ffps || !p || !strstr(p, "/final_")) return p;
    static char buf[4][300]; static int i;
    char *b = buf[i++ & 3];
    size_t n = strlen(p);
    if (n < 4 || n + 5 >= sizeof buf[0] || strcmp(p + n - 4, ".bin")) return p;
    if (g_ffin < 0) { const char *e = getenv("SORA_FFIN"); g_ffin = !(e && *e == '0'); }
    if (g_ffin && strstr(p, "/final_hits_")) {
        memcpy(b, p, n - 4), memcpy(b + n - 4, "_s92.bin", 9);
        if (GetFileAttributesA(b) != INVALID_FILE_ATTRIBUTES) return b;
        b = buf[i++ & 3];
    }
    memcpy(b, p, n - 4), memcpy(b + n - 4, "_s91.bin", 9);
    if (GetFileAttributesA(b) == INVALID_FILE_ATTRIBUTES) { LOG("[sora] ffps: %s missing, old %s\n", b, p); return p; }
    return b;
}
static void ffps_set(int on);  // (after sora_form.h)
static void ffin_set(int on);

static void hits_load(void) {
    const char *path = getenv("SAVAGEWIND_SORA_HITS");
    if (!path || !*path) path = "D:/SoraBotW/data/sora_hits.bin";
    path = sora_path(path);
    hits_file(path, g_hslots, &g_nhslots);
    hits_file("D:/SoraBotW/data/forms/valor_hits_r.bin", g_vhslots[0], &g_nvhslots[0]);
    hits_file("D:/SoraBotW/data/forms/valor_hits_l.bin", g_vhslots[1], &g_nvhslots[1]);
    hits_file("D:/SoraBotW/data/forms/roxas_hits.bin", g_rhslots, &g_nrhslots);  // (s109 roxas)
    hits_file("D:/SoraBotW/data/forms/mickey_hits.bin", g_mkhslots, &g_nmkhslots);  // (s110 b22-mickey)
    LOG("[sora] Valor hit bakes: right %d slots, left %d slots\n", g_nvhslots[0], g_nvhslots[1]);
    hits_file("D:/SoraBotW/data/forms/limit_hits.bin", g_lhslots, &g_nlhslots);  // (s89 b4-limit)
    LOG("[sora] Limit hit bake: %d slots\n", g_nlhslots);
    hits_file("D:/SoraBotW/data/forms/master_hits_r.bin", g_mhslots[0], &g_nmhslots[0]);  // (s89 b4-master)
    hits_file("D:/SoraBotW/data/forms/master_hits_l.bin", g_mhslots[1], &g_nmhslots[1]);
    LOG("[sora] Master hit bakes: right %d slots, left %d slots\n", g_nmhslots[0], g_nmhslots[1]);
    hits_file(ffps_path("D:/SoraBotW/data/forms/final_hits_r.bin"), g_fhslots[0], &g_nfhslots[0]);  // (s89 b4-final)
    hits_file(ffps_path("D:/SoraBotW/data/forms/final_hits_l.bin"), g_fhslots[1], &g_nfhslots[1]);
    LOG("[sora] Final hit bakes: right %d slots, left %d slots (ffps %d: %s, ffin %d)\n", g_nfhslots[0], g_nfhslots[1], g_ffps,
        g_ffps ? "60/120 fps fixed _s91 bakes" : "old x2 bakes", g_ffin);
    hits_file("D:/SoraBotW/data/forms/anti_hits_r.bin", g_ahslots[0], &g_nahslots[0]);  // (s89 anti)
    hits_file("D:/SoraBotW/data/forms/anti_hits_l.bin", g_ahslots[1], &g_nahslots[1]);
    LOG("[sora] Anti hit bakes: track 0 %d slots, track 1 %d slots\n", g_nahslots[0], g_nahslots[1]);
    g_dmg_base = env_f("SAVAGEWIND_SORA_DMG", g_dmg_base);
    g_mp_charge_time = env_f("SAVAGEWIND_SORA_MP_CHARGE", g_mp_charge_time);
    g_cure_pct = env_f("SAVAGEWIND_SORA_CURE_PCT", g_cure_pct);
    g_hit_r = env_f("SAVAGEWIND_SORA_HIT_R", g_hit_r), g_hit_h = env_f("SAVAGEWIND_SORA_HIT_H", g_hit_h);
    g_hit_pad = env_f("SAVAGEWIND_SORA_HIT_PAD", g_hit_pad);
    const char *fin = getenv("SAVAGEWIND_SORA_FIN_F54");
    if (fin && *fin) g_fin_f54 = (int)strtol(fin, NULL, 0);
    LOG("[sora] hit bake %s: %d slots (damage base %.1f, finisher f54 %#x, enemy cylinder r %.2f h %.2f pad %.2f)\n",
        path, g_nhslots, g_dmg_base, g_fin_f54, g_hit_r, g_hit_h, g_hit_pad);
}

// (mp2-rib) form = -1 base Sora (g_fmots_on off), else the FORM_* row; hit_slot below = the local Sora's form
static const HitSlot *hit_slot_form(int form, int slot, int blade) {  // (s69: blade 1 = Valor's left; in Valor only its own windows)
    bool fm = form >= 0;
    const HitSlot *tab = fm ? (form == FORM_VALOR ? g_vhslots[blade] : form == FORM_LIMIT && !blade ? g_lhslots : form == FORM_MASTER ? g_mhslots[blade] : form == FORM_FINAL ? g_fhslots[blade] : form == FORM_ANTI ? g_ahslots[blade] : form == FORM_ROXAS && !blade ? g_rhslots : form == FORM_MICKEY && !blade ? g_mkhslots : NULL) : blade ? NULL : g_hslots;  // (s72: Wisdom = its shot; s89 Limit, Master, Final, Anti)
    int n = fm ? (form == FORM_VALOR ? g_nvhslots[blade] : form == FORM_LIMIT && !blade ? g_nlhslots : form == FORM_MASTER ? g_nmhslots[blade] : form == FORM_FINAL ? g_nfhslots[blade] : form == FORM_ANTI ? g_nahslots[blade] : form == FORM_ROXAS && !blade ? g_nrhslots : form == FORM_MICKEY && !blade ? g_nmkhslots : 0) : g_nhslots;
    for (int i = 0; tab && i < n; i++)
        if (tab[i].slot == slot) return &tab[i];
    return NULL;
}
static const HitSlot *hit_slot(int slot, int blade) { return hit_slot_form(g_fmots_on ? (int)g_fform : -1, slot, blade); }

// the blade at motion time t (linear between baked frames) -> world (u): Sora's feet + RotY(face) * to_u, as sora_draw
static void blade_world(const HitSlot *h, float t, const float feet[3], float face, float to_u, float out[6]) {
    if (t < 0) t = 0;
    if (t > h->nseg - 1) t = (float)(h->nseg - 1);
    int i = (int)t;
    if (i >= h->nseg - 1) i = h->nseg > 1 ? h->nseg - 2 : 0;
    float u = h->nseg > 1 ? t - (float)i : 0, s = sinf(face), c = cosf(face);
    for (int k = 0; k < 2; k++) {
        const float *a = h->seg + i * 6 + k * 3, *b = h->nseg > 1 ? a + 6 : a;
        float q[3] = {a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u, a[2] + (b[2] - a[2]) * u};
        out[k * 3 + 0] = feet[0] + (c * q[0] + s * q[2]) * to_u;
        out[k * 3 + 1] = feet[1] + q[1] * to_u;
        out[k * 3 + 2] = feet[2] + (-s * q[0] + c * q[2]) * to_u;
    }
}

static float g_sweep_min;  // (SAVAGEWIND_SORA_HIT_LOG: the closest sample's horizontal distance, last sweep)
// (s65 audit #15) the blade sample that hit, pulled back to the enemy cylinder's surface: KH2's EffectOnHit is at the
// contact, not at the tip (often inside / past the enemy). Live "set sparkat 0" = the tip again.
static float g_sweep_hit[3];
static int g_spark_at = 1;

static bool sweep_hits_rh(const float a[6], const float b[6], const float e[3], float hit_r, float hit_h) {
    float r = hit_r + g_hit_pad + (IN_MICKEY && g_mkhit ? g_mk_reach : 0);  // (s110 mkhit: his blade is short, KH2 hit with a sphere around the blade)
    g_sweep_min = 1e9f;
    for (int ti = 0; ti < 4; ti++)
        for (int ui = 0; ui < 5; ui++) {
            float t = (float)ti / 3, u = (float)ui / 4, p[3];
            for (int c = 0; c < 3; c++) {
                float pa = a[c] + (a[3 + c] - a[c]) * u, pb = b[c] + (b[3 + c] - b[c]) * u;
                p[c] = pa + (pb - pa) * t;
            }
            float dx = p[0] - e[0], dz = p[2] - e[2];
            if (sqrtf(dx * dx + dz * dz) < g_sweep_min) g_sweep_min = sqrtf(dx * dx + dz * dz);
            if (dx * dx + dz * dz <= r * r && p[1] >= e[1] - g_hit_pad && p[1] <= e[1] + hit_h + g_hit_pad) {
                float d = sqrtf(dx * dx + dz * dz), k = d > hit_r ? 1 : d > 1e-4f ? hit_r / d : 0;
                g_sweep_hit[0] = e[0] + dx * k, g_sweep_hit[1] = p[1], g_sweep_hit[2] = e[2] + dz * k;
                if (k == 0) memcpy(g_sweep_hit, p, sizeof g_sweep_hit);  // (on the axis: the sample itself)
                return true;
            }
        }
    return false;
}
static bool sweep_hits(const float a[6], const float b[6], const float e[3]) {  // quad a -> b vs e's cylinder
    return sweep_hits_rh(a, b, e, g_hit_r, g_hit_h);
}

// (s87 glitch "Guardians = wall") one 0.5 u cylinder for every enemy: a Guardian's centre sits ~2.5 u inside its shell,
// the blade never reached it and the world ray hit the shell -> wall repel (prev.log: 20+ repels at
// Enemy_Guardian_A_Fixed_Moss, 0 blade hits). Big enemies get their own body cylinder (radius / height u, by actor
// name prefix; sizes = GUESS from BotW's models), used by the sweep and by repel_try (a ray contact inside an enemy's
// body is that enemy, not a wall). Live "set bigbody 0" = the old single cylinder.
static int g_big_body = 1;
static int body_actor_name(uint64_t actor, char *buf, int n) {  // (sora_levels.h's read: the exe copy, else the DLL's)
    sl_exe_bind();
    if (SLX.name && SLX.name(actor, buf, n) > 0) return (int)strlen(buf);
    const char *s = SL_ACTOR_NAME(actor);
    int k = 0;
    for (; s && k < n - 1 && s[k] > ' ' && s[k] < 127; k++) buf[k] = s[k];
    buf[k] = 0;
    return k;
}
static void enemy_body(uint64_t actor, float *r, float *h) {
    *r = g_hit_r, *h = g_hit_h;
    if (!g_big_body) return;
    static struct { uint64_t actor; float r, h; } cache[32];
    static int next;
    for (int i = 0; i < 32; i++)
        if (cache[i].actor == actor) { *r = cache[i].r, *h = cache[i].h; return; }
    char nm[48] = {0};
    if (body_actor_name(actor, nm, (int)sizeof nm) <= 0) return;  // (no name: default, not cached - retry next time)
    static const struct { const char *pre; float r, h; } BODY[] = {
        {"Enemy_Guardian_Mini", 0.9f, 1.8f},  // (Guardian Scouts: shrine-sized)
        {"Enemy_Guardian", 2.2f, 3.2f},       // (Decayed / Stalker / Skywatcher; Fixed_Moss = the plateau's decayed ones)
        {"Enemy_Golem", 1.8f, 4.0f},          // (Stone Talus)
        {"Enemy_Giant", 1.4f, 5.5f},          // (Hinox)
        {"Enemy_Lynel", 0.9f, 2.6f},
        {"Enemy_Moblin", 0.7f, 2.4f},
    };
    for (size_t k = 0; k < sizeof BODY / sizeof *BODY; k++)
        if (!strncmp(nm, BODY[k].pre, strlen(BODY[k].pre))) { *r = BODY[k].r, *h = BODY[k].h; break; }
    cache[next].actor = actor, cache[next].r = *r, cache[next].h = *h, next = (next + 1) % 32;
    if (*r != g_hit_r) LOG("[sora] body %llx %s: hit cylinder r %.2f h %.2f u\n", (unsigned long long)actor, nm, *r, *h);
}

// (s87) the enemies of this blade check (blade_hits_one fills it before its windows; repel_try reads it)
static struct { int n; uint64_t actor[64]; float pos[64][3]; int32_t life[64]; } g_bh;
static bool contact_in_enemy(const float c[3]) {  // a BG ray contact inside a live enemy's body (+0.5 u: its shell)
    for (int i = 0; i < g_bh.n; i++) {
        if (g_bh.life[i] <= 0) continue;
        float r, h, dx = c[0] - g_bh.pos[i][0], dz = c[2] - g_bh.pos[i][2];
        enemy_body(g_bh.actor[i], &r, &h);
        if (r <= g_hit_r) continue;  // (small enemies: the old rule; a wall behind a Bokoblin still bounces)
        if (dx * dx + dz * dz <= (r + 0.5f) * (r + 0.5f) && c[1] >= g_bh.pos[i][1] - 0.5f && c[1] <= g_bh.pos[i][1] + h + 0.5f)
            return true;
    }
    return false;
}

// ------------------------------------------------------------------ breakables (s55, recon/S55_HELPER_BREAK.md)
// BotW boxes, barrels, pots, ore, boards, cracked walls: actors whose DamageParam says Breakable. The exe keeps the list
// (linkpos_breakables) and lowers the life (linkpos_break_hit); BotW's own damage step breaks one whose life is below
// 1 (effect, vanish, drops). Both are looked up by name: an exe linked before s55 has neither, then nothing breaks.
// Hits to break (KH2 feel, BotW's damage categories): n = ceil(max life x min(1, rate x k)), rate by Sora's attack:
// swing = Sword, finisher = max(Sword, LargeSword), Blizzard = Arrow, Fire = all at once if the actor burns (wood:
// GUESS for BotW's chemical fire), Thunder = all at once if LightningAffect (BotW lightning); rate 0 = no effect.
// Live "set brk 0/1" (off / on), "set brkk <x100>" (k), "set brkltn 0/1" (Thunder by the lightning rule / not at all).
#include "linkpos_break.h"
enum { BRKHIT_BLADE, BRKHIT_FINISH, BRKHIT_FIRE, BRKHIT_ICE, BRKHIT_THUNDER };
static int g_brk_on = 1, g_brk_ltn = 1;
static float g_brk_k = 1.0f, g_brk_r = 0.45f, g_brk_h = 1.0f;  // (k; the box cylinder: radius / height, u)
static LinkposBreakablesFn g_brk_list_fn;
static LinkposBreakHitFn g_brk_hit_fn;
static int brk_list(LinkposBreakable *b, int max) {
    static int tried;
    if (!tried) {
        tried = 1;
        HMODULE exe = GetModuleHandleA(NULL);
        g_brk_list_fn = (LinkposBreakablesFn)(void *)GetProcAddress(exe, "linkpos_breakables");
        g_brk_hit_fn = (LinkposBreakHitFn)(void *)GetProcAddress(exe, "linkpos_break_hit");
        LOG("[sora] breakables: %s\n", g_brk_list_fn && g_brk_hit_fn ? "on" : "not in this exe (off until the next exe link)");
    }
    return g_brk_on && g_brk_list_fn && g_brk_hit_fn ? g_brk_list_fn(b, max) : 0;
}
static bool brk_burns(const char *name) {  // [GUESS] chem "wood*" / ivy / cloth / nests (breakables.json)
    static const char *const W[] = {"Kibako", "Barrel", "Wood", "Board", "Fence", "Ivy", "Balloon", "Nest", "Logs", "FlyShield"};
    for (size_t i = 0; i < sizeof W / sizeof *W; i++) if (strstr(name, W[i])) return true;
    return false;
}
static int brk_strike(const LinkposBreakable *b, int how) {  // life after the hit, -1 = no effect / refused
    float rate = 0;
    if (how == BRKHIT_BLADE) rate = b->rate[BRK_SWORD];
    else if (how == BRKHIT_FINISH) rate = fmaxf(b->rate[BRK_SWORD], b->rate[BRK_LARGESWORD]);
    else if (how == BRKHIT_ICE) rate = b->rate[BRK_ARROW];
    else if (how == BRKHIT_FIRE) rate = brk_burns(b->name) ? 100 : 0;
    else if (how == BRKHIT_THUNDER) rate = g_brk_ltn && b->lightning ? 100 : 0;
    float f = rate * g_brk_k;
    if (f <= 0) return -1;
    int n = (int)ceilf((float)(b->life_max > 0 ? b->life_max : b->life) * (f > 1 ? 1 : f));
    int after = g_brk_hit_fn(b->actor, n < 1 ? 1 : n);
    static const char *const HOW[] = {"swing", "finisher", "fire", "blizzard", "thunder"};
    LOG("[sora] BREAK hit %llx %s by %s: rate %.2f -> %d of %d life: %d left%s\n", (unsigned long long)b->actor, b->name,
        HOW[how], rate, n, b->life_max, after, after == 0 ? " (breaks)" : after < 0 ? " (refused)" : "");
    return after;
}
static bool st_frozen(uint64_t actor);  // (s122 world, below)
// The Keyblade sweep vs the breakables (blade_hits, before the wall repel: a box in front of him is not a wall).
static int brk_blade(const HitWin *win, const float prev[6], const float now[6], float to_u) {
    LinkposBreakable b[32];
    int n = brk_list(b, 32), hits = 0;
    for (int i = 0; i < n; i++) {
        int k = 0;
        while (k < S.nhit && S.hit_actor[k] != b[i].actor) k++;
        if (k < S.nhit || !sweep_hits_rh(prev, now, b[i].pos, g_brk_r, g_brk_h) || st_frozen(b[i].actor)) continue;  // (s122: Stasis'd = stores the hit instead)
        if (S.nhit < 16) S.hit_actor[S.nhit++] = b[i].actor;
        if (brk_strike(&b[i], win->kind != ATKP_KIND_NORMAL ? BRKHIT_FINISH : BRKHIT_BLADE) < 0) continue;
        sora_fx_event(kbfx_name(win->kind != ATKP_KIND_NORMAL ? "hit_finish" : "hit"), now + 3, S.face, to_u);
        sora_sfx_hit(win->atkp, 1.0f, b[i].pos);
        S.hit = true, hits++;  // [GUESS] KH2 counts an object hit for the combo
    }
    return hits;
}
// A spell's area vs the breakables: a cylinder (r, from y0 - below to y0 + above) at c.
static int brk_spell(int how, const float c[3], float r, float below, float above) {
    LinkposBreakable b[32];
    int n = brk_list(b, 32), hits = 0;
    for (int i = 0; i < n; i++) {
        float dx = b[i].pos[0] - c[0], dz = b[i].pos[2] - c[2], dy = b[i].pos[1] - c[1];
        if (dx * dx + dz * dz < r * r && dy > -below && dy < above && brk_strike(&b[i], how) >= 0) hits++;
    }
    return hits;
}

// ------------------------------------------------------------------ fire world (s59, recon/S59_HELPER_FIRE.md)
// Sora's / Donald's Fire as BotW fire, through BotW's own chemical system (linkpos.c "fire world"). Each Fire tick
// (0 / 18 / 36): the fire objects within g_fire_obj_r u of the caster (campfires, cooking pots, torches, lamps,
// burnable ivy: the exe's list) get their chemical set to burning, as BotW's ignite message does; the living enemies
// in the ring catch BotW fire (fireburn: BotW's burn reaction + its BurnDamage, on top of KH2's hit); g_fire_spk
// flint sparks go round the ring (BotW's spark element: GUESS it lights fuel only - wood_fuel / cook_pot, not grass
// nor Link). fireworld 2 adds lightning-strike fire bursts on a ring of g_fire_burst_r u on the LAST tick: the grass
// burns (BotW: so does Link standing in it, and his life is Sora's - hence opt-in). The exe functions are found by
// GetProcAddress: an exe linked before s59 -> nothing ("fire world: not in this exe").
// Live "set fireworld 0/1/2", "set fireburn 0/1", "set firespk <n>", "set firespkr <cm>", "set firespkl <frames>",
// "set firer <cm>", "set firebr <cm>", "set firebn <n>".
#include "linkpos_fire.h"
// (s60b) "set firespkat 0/1": one spark AT each burnable in reach (fire objects, enemies in the cylinder) - the sparks
// did the lighting (live_s60), the ring round Sora looked un-KH2; "set firespkoy / firespkey <cm>" = their lift.
static int g_fire_world = 1, g_fire_burn = 1, g_fire_spk = 0, g_fire_spk_at = 1, g_fire_burst_n = 6;
static float g_fire_spk_r = 1.0f, g_fire_spk_life = 5.0f, g_fire_obj_r = 2.6f, g_fire_burst_r = 2.5f;  // (u, frames)
static float g_fire_spk_y[2] = {0.3f, 0.6f};  // spark lift above a fire object's / an enemy's feet, u
static struct {
    LinkposFireObjsFn objs;
    LinkposFireIgniteFn ignite;
    LinkposFireSparkFn spark;
    LinkposFireBurstFn burst;
    int tried;
} FW;
static bool fire_fns(void) {
    if (!FW.tried) {
        HMODULE exe = GetModuleHandleA(NULL);
        FW.tried = 1;
        FW.objs = (LinkposFireObjsFn)(void *)GetProcAddress(exe, "linkpos_fire_objects");
        FW.ignite = (LinkposFireIgniteFn)(void *)GetProcAddress(exe, "linkpos_fire_ignite");
        FW.spark = (LinkposFireSparkFn)(void *)GetProcAddress(exe, "linkpos_fire_spark");
        FW.burst = (LinkposFireBurstFn)(void *)GetProcAddress(exe, "linkpos_fire_burst");
        LOG("[sora] fire world: %s\n", FW.objs && FW.ignite && FW.spark && FW.burst ? "on" : "not in this exe (off until the next exe link)");
    }
    return FW.objs && FW.ignite && FW.spark && FW.burst;
}
// c = the caster's feet; r, h = the Fire cylinder (u); who = "sora" / "donald" (the log)
// (s176 fire sync) remote = 1: the Fire was cast on ANOTHER game (NET_FIRE): the world only - fire objects ignite, sparks at every
// burnable (enemies included), the last-tick bursts; enemies are ignited only with `set netfireen 1` (their HP is shared by NET_EDMG,
// burning them here too would double the burn). res (remote only) = lit, sparks, bursts. No log line from the remote half (sora_net_fire.h logs).
static void fire_world_body(const float c[3], bool last, bool burst, float r, float h, const char *who, bool remote, int res[3]) {
    int lit = 0, burn = 0, spk = 0, bst = 0, nat = 0;
    char names[128] = "";
    float at[16][3];  // (s60b) where the sparks go: each burnable in reach (live_s60: ignite alone lit nothing)
    LinkposFireObj fo[48];
    int n = FW.objs(fo, 48);
    for (int i = 0; i < n; i++) {
        float dx = fo[i].pos[0] - c[0], dz = fo[i].pos[2] - c[2], dy = fo[i].pos[1] - c[1];
        if (dx * dx + dz * dz > g_fire_obj_r * g_fire_obj_r || dy < -1.5f || dy > 3.0f) continue;
        if (nat < 16) at[nat][0] = fo[i].pos[0], at[nat][1] = fo[i].pos[1] + g_fire_spk_y[0], at[nat][2] = fo[i].pos[2], nat++;
        if (FW.ignite(fo[i].actor) <= 0) continue;
        lit++;
        if (strlen(names) + strlen(fo[i].name) + 2 < sizeof names) strcat(names, " "), strcat(names, fo[i].name);
    }
    if (g_fire_burn) {
        uint64_t ea[64];
        float ep[64][3];
        int32_t el[64];
        int en = linkpos_enemies(ea, ep, el, 64);
        for (int i = 0; i < en; i++) {
            float dx = ep[i][0] - c[0], dz = ep[i][2] - c[2], dy = ep[i][1] - c[1];
            if (el[i] <= 0 || dx * dx + dz * dz >= r * r || dy <= -h || dy >= 2 * h) continue;
            if (nat < 16 && (!remote || g_net_fire_espk)) at[nat][0] = ep[i][0], at[nat][1] = ep[i][1] + g_fire_spk_y[1], at[nat][2] = ep[i][2], nat++;  // (s176 review: `set netfirespk 0` = no sparks at the receiver's enemies)
            if ((!remote || g_net_fire_en) && FW.ignite(ea[i]) > 0) burn++;
        }
    }
    for (int i = 0; g_fire_spk_at && i < nat; i++)
        if (FW.spark(at[i], g_fire_spk_r, g_fire_spk_life) > 0) spk++;
    for (int i = 0; i < g_fire_spk && i < 16; i++) {  // (a ring at 0.9 r, half a step turned on the last tick)
        float a = 6.2831853f * ((float)i + (last ? 0.5f : 0.0f)) / (float)g_fire_spk;
        float p[3] = {c[0] + sinf(a) * 0.9f * r, c[1] + 0.5f, c[2] + cosf(a) * 0.9f * r};
        if (FW.spark(p, g_fire_spk_r, g_fire_spk_life) > 0) spk++;
    }
    for (int i = 0; burst && last && i < g_fire_burst_n && i < 16; i++) {
        float a = 6.2831853f * (float)i / (float)g_fire_burst_n;
        float p[3] = {c[0] + sinf(a) * g_fire_burst_r, c[1] + 0.3f, c[2] + cosf(a) * g_fire_burst_r};
        if (FW.burst(p) > 0) bst++;
    }
    if (remote) {
        res[0] = lit, res[1] = spk, res[2] = bst;
        return;
    }
    LOG("[sora] fire world (%s%s): lit %d%s, %d enemies set burning, %d sparks (%d burnables in reach), %d bursts\n", who,
        last ? ", last" : "", lit, names, burn, spk, nat, bst);
}
static void fire_world(const float c[3], bool last, float r, float h, const char *who) {
    if (!g_fire_world) return;
    net_fire_note(c, last, g_fire_world >= 2, r, h, !strcmp(who, "sora") ? 0 : !strcmp(who, "donald") ? 1 : 2);  // (s176: the other games' worlds burn too)
    if (!fire_fns()) return;
    fire_world_body(c, last, g_fire_world >= 2, r, h, who, false, NULL);
}
static bool g_loading;  // (defined with the load latch below)
// a Fire tick cast on another game (sora_net_fire.h): -> 0 done, -1 no world here, -2 no fire functions in the exe
static int fire_world_remote(const float c[3], bool last, bool burst, float r, float h, int who, int res[3]) {
    // (title / loading / menu / paused / no Link: no objects to light, the list may be stale; s176 review: NET.driven = this frame
    // read Link's position, the same gate the local Fire ticks under)
    if (!g_fire_world || !linkpos_world_frame() || g_loading || !NET.driven || NET.loading) return -1;
    if (!fire_fns()) return -2;
    if (r < 0.05f) r = 0.05f;
    if (h < 0.05f) h = 0.05f;
    fire_world_body(c, last, burst, r, h, who == 0 ? "sora" : who == 1 ? "donald" : "other", true, res);
    return 0;
}

// The wall (s50, bug 2: a swing into a wall hit what was behind it). KH2's forward wall probe (re:0x3BA750) has no
// BotW twin here (no ray cast yet), so a wall is what stopped Sora's own ground move: last frame's commanded step
// (section 5) against what the body did (section 1), WALL_N frames in a row, no enemy in the way. W.dir = the blocked
// step's direction, W.pos = where; it holds while he stays within WALL_STAY u of there.
#define WALL_N 3
static struct { float dir[2], pos[3], cmd[2]; int n; bool on; } W;
// (s52) the repel's copy of the last wall: W lets go past WALL_STAY (0.3 u), and the repel's own ~35 cm push did that
// every bounce - the next swing (in the push, or right after) went through (live_s52: bounce / through / bounce).
// RW holds while he stays within g_repel_stay u of it (live "set rstay <x100>").
static struct { float dir[2], pos[3]; bool on; } RW;
static float g_repel_stay = 1.5f;
static float g_wall_ratio = 0.35f;  // live "set wratio <x100>": blocked = moved under this part of the step
static float g_wall_stay = 0.3f, g_wall_gap = 0.45f, g_wall_cos = 0.5f;  // u, u (his body radius + a bit), cone
static int g_wall_on = 1;            // live "set wall 0/1"
static bool nearest_enemy(const float p[3], float out[2]);

static void wall_tick(const float p[3], float moved_cm, bool grounded) {
    float c = hypotf(W.cmd[0], W.cmd[1]), cmd[2] = {W.cmd[0], W.cmd[1]};
    W.cmd[0] = W.cmd[1] = 0;  // (section 5 sets it again on a grounded step: no stale step after the air)
    if (W.on && hypotf(p[0] - W.pos[0], p[2] - W.pos[2]) > g_wall_stay) W.on = false, LOG("[sora] wall: left it\n");
    if (!g_wall_on || !grounded || c < 1.0f) { W.n = 0; return; }  // (under 1 cm commanded: nothing to judge)
    float d[2] = {cmd[0] / c, cmd[1] / c}, e[2];
    bool enemy = false;  // (an enemy's body stops him too: that is not a wall)
    if (nearest_enemy(p, e)) {
        float ex = e[0] - p[0], ez = e[1] - p[2], ed = hypotf(ex, ez);
        enemy = ed < 1.5f && (ed < 1e-3f || (ex * d[0] + ez * d[1]) / ed > 0.5f);
    }
    if (moved_cm < c * g_wall_ratio && !enemy) {
        if (++W.n >= WALL_N) {
            if (!W.on) LOG("[sora] wall at %.3f %.3f %.3f dir %.2f %.2f (moved %.1f of %.1f cm)\n", p[0], p[1], p[2], d[0],
                           d[1], moved_cm, c);
            W.on = true, W.dir[0] = d[0], W.dir[1] = d[1], W.pos[0] = p[0], W.pos[1] = p[1], W.pos[2] = p[2];
            RW.on = true, memcpy(RW.dir, W.dir, sizeof RW.dir), memcpy(RW.pos, W.pos, sizeof RW.pos);
        }
    } else {
        W.n = 0;
        if (W.on && moved_cm >= c * 0.7f && d[0] * W.dir[0] + d[1] * W.dir[1] > 0.5f) W.on = false, LOG("[sora] wall: gone\n");
    }
}

static bool wall_ahead(const float p[3], float face) {  // KH2's ptya Id 1 condition (the probe's hit), approximated
    return W.on && hypotf(p[0] - W.pos[0], p[2] - W.pos[2]) <= g_wall_stay &&
           sinf(face) * W.dir[0] + cosf(face) * W.dir[1] > g_wall_cos;
}

// s85f #6 (glitch list): triangle standing in the open started the X-pressed climb try, whose X pulse is BotW's jump:
// Link (and Sora on him) hopped. Only a wall in reach in front gets the try: a ray from the chest along his facing,
// live "set xwall <cm>" (0 = the old rule, any X press tries).
static int g_xwall_cm = 120;
// (s112) live "set xwallhi <cm>": a wall must ALSO be hit this high above the feet (default 160; 0 = the old one ray). A shrine pedestal
// (~1.1 m) hit the 0.9 m ray = the climb try's X pulse = BotW skipped the Slate scene (Ja Baji, Owa Daim); low things now get Triangle = A
static int g_xwall_hi_cm = 160;
static int g_nox_len = 30;  // (s85f #6e: the no-wall triangle try's pick-up window, presents; live "set noxlen")
bool linkpos_raycast(const float from[3], const float to[3], float hit[3], float normal[3]);
static bool x_wall(const float p[3], float face) {
    if (g_xwall_cm <= 0) return true;
    float r = g_xwall_cm * 0.01f, a[3] = {p[0], p[1] + 0.9f, p[2]}, b[3] = {a[0] + sinf(face) * r, a[1], a[2] + cosf(face) * r}, h[3], n[3];
    bool hit = linkpos_raycast(a, b, h, n) && fabsf(n[1]) < 0.7f, low = false;
    if (hit && g_xwall_hi_cm > 0) {  // (s112) and up high too: else it is a low thing (pedestal, crate) = A, not a climb
        float a2[3] = {p[0], p[1] + g_xwall_hi_cm * 0.01f, p[2]}, b2[3] = {a2[0] + sinf(face) * r, a2[1], a2[2] + cosf(face) * r};
        low = !(linkpos_raycast(a2, b2, h, n) && fabsf(n[1]) < 0.7f);
        if (low) hit = false;
    }
    LOG("[sora] X press: %s in front (%d cm ray%s)\n", hit ? "a wall" : low ? "only a LOW thing (no climb: A)" : "no wall", g_xwall_cm,
        g_xwall_hi_cm > 0 ? ", + high ray" : "");
    return hit;
}

static bool wall_hides(const float e[3]) {  // an enemy past the wall's plane (its centre beyond his body + the gap)
    // (one standing on top of a low wall - feet 0.8 u or more above his - is what A330's hop up the wall reaches)
    return W.on && (e[0] - W.pos[0]) * W.dir[0] + (e[2] - W.pos[2]) * W.dir[1] > g_wall_gap && e[1] < W.pos[1] + 0.8f;
}

// KH2's Keyblade wall repel (WALL_REPEL_RECON.md 0-6). KH2: atkp Flags bit 0x01 = BGHit + RefactSelf 0 "Bound" ->
// the attack collision tests the BG and on contact re:0x3D2140 sets attack flag 0x200 and calls Sora's vtable +0xD0 =
// re:0x3D54B0: +0x120 |= bit 25 (swing connected), +0xD74-- if non-zero, then re:0x3D4650(actor, attack, 0, -1, -1.0):
// one repel per attack object (flag 0x40 latch), cut (blend 0) to motion 41 ground / 43 air, power -1 -> 50.0 =
// a push along -facing at 5 cm/frame x0.9 per frame until under 1 (re:0x3D4960, PHH hit_reaction_kh2.py), queue idle
// (ground) / Fall = 4 (air). Base Sora's BGHit rows: A300 atkp 126, A302 atkp 128, A330 atkp 131; the finishers
// (A301 A303 A304 A331-A333: Flags 0 / 0x20) go through walls. Live "set repel 0/1", "set rgap <x100>".
// (s53: blade_in_wall ray casts grip -> tip first - see blade_ray; what follows is the fallback.)
// [approx] KH2 tests the swing's own collision against the BG; we have no BotW world raycast here yet, so the blade
// (grip -> tip) is judged against wall_tick's plane: W.on, he stands at it (wall_ahead's distance) and faces into it,
// and the tip reaches past the plane by g_repel_gap u (his body radius - the wall face is about there). Only a wall he
// walked into counts (a swing started off a run at an unwalked wall does not bounce). TODO better: a BotW ray /
// shape cast along grip -> tip (hknp castRay through the actor's world, or the probe linkpos uses for the ledge hold)
// - that also gives the contact point + normal for the spark and works in the air and on walls not walked into.
// [GUESS] REPEL_*_MOT: mot_of(id) takes slot id*4+3, 2, 0, 1, so 41 -> P_EX100 slot 164, 43 -> 172 (x4 verified for
// 181 -> 724 and 42 -> 168 only). TODO check the bake (tools/sorabake/Program.cs slot list) has 164 / 172; if not,
// mot_of returns NULL and the repel only pushes (logged "no motion").
#define REPEL_GROUND_MOT 41  // TODO verify: P_EX100 slot 164 ("REFLECT" in PHH; shared with the guard reflect)
#define REPEL_AIR_MOT 43     // TODO verify: P_EX100 slot 172 ("REFLECT_AIR")
static int g_repel = 1;               // live "set repel 0/1"
static float g_repel_gap = 0.40f;     // u past W.pos along W.dir the tip must reach (live "set rgap <x100>")
static struct { int swing; float push; } R = {-1, 0};  // R.swing: the swing that already bounced (KH2's 0x40 latch)

static bool repel_row(int atkp) { return atkp == 126 || atkp == 128 || atkp == 131; }  // (atkp Flags & 1, base Sora)

// (s53) the BotW world ray cast (linkpos_raycast, BOTW_RAYCAST_RECON.md; static ground layers, no Link / enemies):
// the blade grip -> tip, and the tip's own path since last frame (a fast swing), a hit with a wall-ish normal
// (|n.y| < g_ray_wall_ny: floors and gentle slopes do not bounce) = the swing met the BG, KH2's attack-vs-BG test
// (re:0x3D2140) without a learned plane -> unwalked walls and the air too. RW (the walked-into plane) stays as the
// fallback. Live "set rray 0/1", "set rwny <x100>".
bool linkpos_raycast(const float from[3], const float to[3], float hit[3], float normal[3]);
static int g_rray = 1;
static float g_ray_wall_ny = 0.5f;
static struct { float pos[3], n[3]; bool ray; } RC;  // the blade's last BG contact (repel_try's spark, se + log)

static bool blade_ray(const float a[3], const float b[3]) {
    float h[3], n[3];
    if (!linkpos_raycast(a, b, h, n) || fabsf(n[1]) >= g_ray_wall_ny) return false;
    memcpy(RC.pos, h, sizeof h), memcpy(RC.n, n, sizeof n), RC.ray = true;
    return true;
}

// grip seg[0..2], tip seg[3..5] (prev = last frame's), world u
// (s65 audit #6) with the ray on, the RW plane is off too: all 3 repels in live1 were RW planes learned next to an enemy
// (the chain died at the Bokoblin), none by ray. Live "set rwfall 1" = the old fallback.
static int g_rw_fall = 0;
static bool blade_in_wall(const float seg[6], const float prev[6], const float p[3]) {
    RC.ray = false;
    if (g_rray && (blade_ray(seg, seg + 3) || blade_ray(prev + 3, seg + 3))) return true;
    if (g_rray && !g_rw_fall) return false;
    if (!RW.on || hypotf(p[0] - RW.pos[0], p[2] - RW.pos[2]) > g_repel_stay) return RW.on = false;
    if (sinf(S.face) * RW.dir[0] + cosf(S.face) * RW.dir[1] <= g_wall_cos) return false;  // (facing into it)
    float tip = (seg[3] - RW.pos[0]) * RW.dir[0] + (seg[5] - RW.pos[2]) * RW.dir[1];
    memcpy(RC.pos, seg + 3, sizeof RC.pos), RC.n[0] = -RW.dir[0], RC.n[1] = 0, RC.n[2] = -RW.dir[1];
    return tip > g_repel_gap && seg[4] < RW.pos[1] + 2.0f;  // (TODO: the wall's height is unknown; 2 u = Sora-ish)
}

// (s53) floor under Sora by ray cast (BOTW_RAYCAST_RECON.md "First live test"): logged every 120 grounded frames once
// he moved 0.25 u, live "set fray 0/1". Check: on the camp rock top y ~277.19, n ~(0, 1, 0).
static int g_fray = 1;
static void floor_probe(const float p[3], bool grounded) {
    static int n;
    static float last[3] = {1e9f, 0, 0};
    if (!g_fray || !grounded || ++n % 120 || hypotf(p[0] - last[0], p[2] - last[2]) < 0.25f) return;
    float a[3] = {p[0], p[1] + 1.0f, p[2]}, b[3] = {p[0], p[1] - 60.0f, p[2]}, h[3] = {0}, nr[3] = {0};
    bool ok = linkpos_raycast(a, b, h, nr);
    memcpy(last, p, sizeof last);
    LOG("[sora] floor ray at %.3f %.3f %.3f: %s y %.3f (%.3f below the feet) n %.2f %.2f %.2f\n", p[0], p[1], p[2],
        ok ? "hit" : "NO HIT", h[1], p[1] - h[1], nr[0], nr[1], nr[2]);
}

static void play(int m);  // (repel_try, s51: defined below)
static bool repel_try(const HitWin *win, int w, const float now[6], const float prev[6], const float feet[3], float to_u) {
    // (S.hit: this swing already hit an enemy - re:0x3D0BB0 skips the wall test then, WALL_REPEL_RECON.md 10)
    if (!g_repel || !repel_row(win->atkp) || R.swing == S.swing || S.hit || !blade_in_wall(now, prev, feet)) return false;
    if (RC.ray && contact_in_enemy(RC.pos)) {  // (s87: the ray met a Guardian's shell, not the BG)
        static int said;
        if (!(said++ % 20)) LOG("[sora] blade ray hit an enemy body at %.2f %.2f %.2f: no repel\n", RC.pos[0], RC.pos[1], RC.pos[2]);
        return false;
    }
    bool air = ACT_AIR(S.act);
    int from = S.motion, slot = S.mot ? S.mot->slot : -1;
    R.swing = S.swing + 1;  // (play() below counts the repel as the next swing: the latch holds through it)
    S.hit = true;           // re:0x3D54B0: +0x120 bit 25, the combo gate's "connected"
    if (S.d74 > 0) S.d74--;  // re:0x3D54B0: +0xD74-- (GUESS: the next press re-does this step)
    int m = air ? REPEL_AIR_MOT : REPEL_GROUND_MOT;
    bool have = mot_of(m) != NULL;
    if (have) play(m), S.special = SP_REPEL, S.next = air ? MOT_FALL : 0;  // (cut, blend 0; queue Fall / idle)
    else S.special = SP_NONE, S.motion = -1;  // (not baked: drop the swing, the push still runs)
    R.push = 5.0f;  // re:0x3D4960 power 50 * 0.1 cm/frame; air: TODO re:0x3D4800's hop back (ACTION slot 3)
    // re:0x3D1960 (WALL_REPEL_RECON.md 10-11): a fixed se 500010 = se500 #10, sfx.bin "wall_hit" (s52 pack, idx 143)
    // at the Keyblade - not the atkp hit se. (An sfx.bin loaded before s52 has no "wall_hit": the hit se instead.)
    static int wall_se = -2;
    if (wall_se == -2) wall_se = sora_sfx_id("wall_hit");
    if (wall_se >= 0) sora_sfx_play_at(wall_se, 1.0f, RC.pos);
    else sora_sfx_hit(win->atkp, 1.0f, RC.pos);
    // TODO fx: the EffectOnHit spark (1) at the BG contact (attack +0xB4, recon 4 GUESS); "repel" = a new sora_fx
    // event name, needs W_EX010 el 1 under that name in the fx bake (until then the plain "hit" spark).
    sora_fx_event(kbfx_name("hit_guard"), RC.pos, S.face, to_u);  // VERIFIED s52: w_ex010.3 = the Keyblade wall spark (re:0x3D1960)
    LOG("[sora] repel at %.3f %.3f %.3f: slot %d (motion %d) window %d atkp %d clock %.1f, tip %.2f %.2f %.2f, %s "
        "contact %.2f %.2f %.2f n %.2f %.2f %.2f, %s motion %d%s, d74 %d\n", feet[0], feet[1], feet[2], slot, from, w,
        win->atkp, S.clock, now[3], now[4], now[5], RC.ray ? "ray" : "plane (RW)", RC.pos[0], RC.pos[1], RC.pos[2],
        RC.n[0], RC.n[1], RC.n[2], air ? "air" : "ground", m, have ? "" : " (no motion)", S.d74);
    return true;
}

#include "sora_airhold.h"  // (s55 airhold: KH2 blow / air-hop reactions on BotW's ragdolls; S55_HELPER_AIRHOLD.md)

// (s69) Valor's two blades swing the same hit group on the same frames (B304 85-97 = atkp 526 + 527, both group 5):
// KH2 hits an enemy once per group, so a group one blade already hit this swing is spent for the other blade
static struct { uint64_t actor; uint16_t group; uint8_t blade; } g_grp_hit[32];
static int g_rehit = 1;  // (s89 anti: Interval re-hits, live "set rehit"; see g_hit_clk below)
static int g_ngrp_hit, g_grp_swing = -1;
static bool blade_group_free(int blade, int group, uint64_t actor) {
    if (!g_fmots_on) return true;
    if (g_grp_swing != S.swing) g_grp_swing = S.swing, g_ngrp_hit = 0;
    for (int k = 0; k < g_ngrp_hit; k++)
        if (g_grp_hit[k].actor == actor && g_grp_hit[k].group == group) {
            if (g_grp_hit[k].blade != blade) return false;
            if (g_rehit) return true;  // (s89 review: an Interval re-hit by the same blade - no duplicate row; 32 rows filled in one Anti spin)
        }
    if (g_ngrp_hit < 32) g_grp_hit[g_ngrp_hit].actor = actor, g_grp_hit[g_ngrp_hit].group = (uint16_t)group,
                         g_grp_hit[g_ngrp_hit++].blade = (uint8_t)blade;
    return true;
}

// (s89 anti) KH2's atkp Interval: an enemy already hit by a window is hit again every Interval KH frames while it stays
// live (HitWin.pad[1], only the bodyhits bake writes it: Anti's spins / bursts; GUESS that KH2 re-hits at exactly that
// period). The clock of each S.hit_actor[k] entry per blade. Live: set rehit 0 = one hit per window (the old rule).
static bool ffx_trail_win(const Mot *m, float clock);  // (s93 b7-ffx: sora_ffx.h; the form's own weapon_trail windows)
static void ffx_tick(float to_u);
static void ffx_air_trail(float to_u);  // (s95 fairfx: sora_ffx.h; Final's own ribbons on jump / fall / glide)
static void ffx_set_air(int v);
static void ffx_set(int v, int la);
static float g_hit_clk[2][16];
// ------------------------------------------------------------------ world (s122, recon/S122_HELPER_TREES.md + STASIS.md)
// The Keyblade sweep vs BotW's cuttable trees and Stasis'd objects (linkpos.c "world": the exe keeps both lists and
// makes the BotW calls; looked up by name, an exe linked before s122 has none -> "world: not in this exe", nothing).
// Trees: each blade hit = one BotW weapon hit on the tree's life (LumberjackTree Life 40); its power = Sora's KH2 attack
// (Str + Keyblade + Berserk / Mickey's own, x the window's atkp power / 100), as BotW's weapon Power (Traveler's Sword 5
// = 8 hits, Master Sword 30 = 2) [GUESS mapping]; a finisher counts double [GUESS]. The last hit fells it away from Sora.
// Stasis: each blade hit on a Stasis'd object = BotW's own StasisInfo::add with kind 0 (one-handed sword: 16 hits fill
// the arrow by BotW's defaults); a finisher = kind g_st_fin (1 = two-handed, 8 hits) [GUESS: "finisher counts stronger"].
// A Stasis'd breakable takes no break hit (BotW: Stasis'd things store the hits instead) [GUESS].
// Live "set tree 0/1", "set treek <pct>" (power x pct/100, 100), "set treefin <pct>" (finisher x, 200), "set treer <cm>"
// / "set treeh <cm>" (trunk cylinder, 90 / 300), "set stasis 0/1", "set stasisfin <kind>" (1), "set stasisfl <bits>"
// (BotW add() flags: 1 count, 2 launch angle, 4 no arrow refresh; 3), "set stasisr <cm>" / "set stasish <cm>" (100 / 200).
#include "linkpos_world.h"
static int g_tree_on = 1, g_tree_k = 100, g_tree_fin = 200, g_st_on = 1, g_st_fin = 1, g_st_fl = 3;
static float g_tree_r = 0.9f, g_tree_h = 3.0f, g_st_r = 1.0f, g_st_h = 2.0f;
static struct { LinkposTreesFn trees; LinkposTreeHitFn tree_hit; LinkposStasisObjsFn st_objs; LinkposStasisHitFn st_hit; int tried; } WD;
static void world_fns(void) {
    if (WD.tried) return;
    WD.tried = 1;
    HMODULE exe = GetModuleHandleA(NULL);
    WD.trees = (LinkposTreesFn)(void *)GetProcAddress(exe, "linkpos_trees");
    WD.tree_hit = (LinkposTreeHitFn)(void *)GetProcAddress(exe, "linkpos_tree_hit");
    WD.st_objs = (LinkposStasisObjsFn)(void *)GetProcAddress(exe, "linkpos_stasis_objs");
    WD.st_hit = (LinkposStasisHitFn)(void *)GetProcAddress(exe, "linkpos_stasis_hit");
    LOG("[sora] world: trees %s, Stasis %s\n", WD.trees && WD.tree_hit ? "on" : "not in this exe",
        WD.st_objs && WD.st_hit ? "on" : "not in this exe");
}
static bool st_frozen(uint64_t actor) {  // (is it Stasis'd now: breakables skip it)
    world_fns();
    if (!g_st_on || !WD.st_objs) return false;
    LinkposStasisObj o[16];
    int n = WD.st_objs(o, 16);
    for (int i = 0; i < n; i++) if (o[i].actor == actor) return true;
    return false;
}
// (s122b) BotW's Stasis launch (dm vslot 14 getStasisBlowVelocity 0x7100a7b68c) and arrow ratio (vslot 13) read the
// StasisInfo through the damage manager's cache dm +0x220, which only BotW's own hit entry (vslot 17) fills: calling
// StasisInfo::add directly left it 0 -> the arrow grew but nothing launched (s122b user). So fill it as vslot 17 does:
// info = the pool entry whose id == actor +0x60, at entry +0x10. The exe does the same from the next relink on.
static bool rd_ok(uint64_t a, size_t n);  // (below)
static void st_cache(uint64_t actor) {
    uint64_t sp = sora_sites()->stasis_pool;
    if (!sp || !rd_ok(sp, 8) || !rd_ok(actor, 0x848)) return;
    uint64_t pool = *(const uint64_t *)(uintptr_t)sp, dm = *(const uint64_t *)(uintptr_t)(actor + 0x840);
    if (!rd_ok(pool, 0x60 + 16 * 0x320) || !rd_ok(dm, 0x228)) return;
    int32_t id = *(const int32_t *)(uintptr_t)(actor + 0x60);
    for (int k = 0; k < 16; k++) {
        uint64_t e = pool + 0x60 + (uint64_t)k * 0x320;
        if (id == -1 || *(const int32_t *)(uintptr_t)(e + 8) != id) continue;
        uint64_t *cache = (uint64_t *)(uintptr_t)(dm + 0x220);
        if (*cache != e + 0x10) {
            LOG("[sora] stasis: dm %llx cache %llx -> info %llx (pool slot %d)\n", (unsigned long long)dm,
                (unsigned long long)*cache, (unsigned long long)(e + 0x10), k);
            *cache = e + 0x10;
        }
        return;
    }
}
static bool world_seen(uint64_t actor) {  // (already hit in this window; else counts it)
    int k = 0;
    while (k < S.nhit && S.hit_actor[k] != actor) k++;
    if (k < S.nhit) return true;
    if (S.nhit < 16) S.hit_actor[S.nhit++] = actor;
    return false;
}
static int world_blade(const HitWin *win, const float prev[6], const float now[6], const float feet[3], float to_u) {
    world_fns();
    bool fin = win->kind != ATKP_KIND_NORMAL;
    int hits = 0;
    if (g_tree_on && WD.trees && WD.tree_hit) {
        LinkposTree t[48];
        int n = WD.trees(t, 48);
        for (int i = 0; i < n; i++) {
            float dx = t[i].pos[0] - feet[0], dz = t[i].pos[2] - feet[2];
            if (dx * dx + dz * dz > 36 || !sweep_hits_rh(prev, now, t[i].pos, g_tree_r, g_tree_h) || world_seen(t[i].actor)) continue;
            sl_init();
            int st = g_wpn_cur.def ? g_wpn_cur.def->power + g_wpn_cur.atk_up  // (s128 wpn: Link's BotW weapon = its Attack)
                                   : sl_str_of(&SL_LV[sl_lv() - 1]) + SL_KEY_STR + (berserk_on() ? 2 : 0) + (IN_MICKEY ? mk_str_delta() : 0);
            float pw = (float)st * (float)win->power / 100.0f * (float)g_tree_k / 100.0f * (fin ? (float)g_tree_fin / 100.0f : 1.0f);
            int dmg = (int)lroundf(pw);
            if (dmg < 1) dmg = 1;
            int after = WD.tree_hit(t[i].actor, dmg, dx, dz);
            LOG("[sora] TREE hit %llx %s by %s: Str %d x power %d -> %d of %d life: %d left%s\n", (unsigned long long)t[i].actor,
                t[i].name, fin ? "finisher" : "swing", st, win->power, dmg, t[i].life_max, after,
                after == 0 ? " (FELLED)" : after < 0 ? " (refused)" : "");
            if (after < 0) continue;
            wpn_wear(1);  // (s128 wpn: GUESS vanilla: a tree hit wears the weapon too, S128_HELPER_DMG.md Q5)
            sora_fx_event(kbfx_name(fin ? "hit_finish" : "hit"), now + 3, S.face, to_u);
            sora_sfx_hit(win->atkp, 1.0f, now + 3);
            S.hit = true, hits++;
        }
    }
    if (g_st_on && WD.st_objs && WD.st_hit) {
        LinkposStasisObj o[16];
        int n = WD.st_objs(o, 16);
        for (int i = 0; i < n; i++) {
            float dx = o[i].pos[0] - feet[0], dz = o[i].pos[2] - feet[2], l = sqrtf(dx * dx + dz * dz);
            float ep[3] = {o[i].pos[0], o[i].pos[1] - g_st_h / 2, o[i].pos[2]};
            if (l > 8 || !sweep_hits_rh(prev, now, ep, g_st_r, g_st_h) || world_seen(o[i].actor)) continue;
            if (l < 1e-3f) dx = sinf(S.face), dz = cosf(S.face), l = 1;  // [GUESS] on top of it: his facing
            int kind = g_wpn_cur.def && g_wpn_cur.def->type == 1 ? 1 : fin ? g_st_fin : 0;  // (s128 wpn: a two-hander = BotW's kind 1)
            st_cache(o[i].actor);  // (s122b: BotW's launch reads the info through dm +0x220)
            int r = WD.st_hit(o[i].actor, dx / l, 0, dz / l, kind, g_st_fl);
            LOG("[sora] STASIS hit %llx %s by %s (kind %d): %s (stored %.2f, hits %d before)\n", (unsigned long long)o[i].actor,
                o[i].name, fin ? "finisher" : "swing", kind, r > 0 ? "added" : "refused", o[i].stored, o[i].hits);
            if (r <= 0) continue;
            wpn_wear(1);  // (s128 wpn: GUESS vanilla: Stasis hits wear the weapon)
            sora_fx_event(kbfx_name(fin ? "hit_finish" : "hit"), now + 3, S.face, to_u);
            sora_sfx_hit(win->atkp, 1.0f, now + 3);
            S.hit = true, hits++;
        }
    }
    return hits;
}

// (s122b worlddiag) every 120 frames while something changed: the exe's tree / Stasis lists and BotW's stasis pool read
// straight from memory (sora_sites()->stasis_pool), to see which half fails. Live "set worlddiag 0" = off.
static int g_world_diag = 1;
static bool rd_ok(uint64_t a, size_t n);  // (below)
static void world_diag(void) {
    static int n;
    static char last[512];
    if (!g_world_diag || ++n % 120) return;
    world_fns();
    float lm[12];
    if (!linkpos_link_mtx(lm)) return;
    char buf[512];
    int len = 0, nt = -1, ns = -1;
    float best = 1e9f;
    const char *bn = "-";
    LinkposTree t[48];
    if (WD.trees) {
        nt = WD.trees(t, 48);
        for (int i = 0; i < nt; i++) {
            float dx = t[i].pos[0] - lm[3], dz = t[i].pos[2] - lm[11], d = sqrtf(dx * dx + dz * dz);
            if (d < best) best = d, bn = t[i].name;
        }
    }
    LinkposStasisObj o[16];
    if (WD.st_objs) ns = WD.st_objs(o, 16);
    len += snprintf(buf + len, sizeof buf - len, "trees %d (nearest %s %.1f m) | stasis objs %d", nt, bn, nt > 0 ? best : -1.0f, ns);
    uint64_t pool = sora_sites()->stasis_pool && rd_ok(sora_sites()->stasis_pool, 8) ? *(const uint64_t *)(uintptr_t)sora_sites()->stasis_pool : 0;
    len += snprintf(buf + len, sizeof buf - len, " | pool %llx:", (unsigned long long)pool);
    if (pool && rd_ok(pool, 0x60 + 16 * 0x320))
        for (int k = 0; k < 16 && len < (int)sizeof buf - 80; k++) {
            uint64_t e = pool + 0x60 + (uint64_t)k * 0x320;
            int32_t id = *(const int32_t *)(uintptr_t)(e + 8);
            if (id == -1) continue;
            const uint8_t *info = (const uint8_t *)(uintptr_t)(e + 0x10);
            const float *v = (const float *)(info + 0x280);
            len += snprintf(buf + len, sizeof buf - len, " [%d] id %d data %llx hits %d valid %d vec %.2f %.2f %.2f", k, id,
                            (unsigned long long)*(const uint64_t *)(uintptr_t)e, info[0x27c], info[0x27d], v[0], v[1], v[2]);
        }
    if (strcmp(buf, last)) {
        memcpy(last, buf, sizeof last);
        LOG("[sora] worlddiag: %s\n", buf);
    }
}

static int blade_hits_one(int blade, float clock_old, float to_u) {  // (after the clock step; S.mot = the playing attack)
    const HitSlot *h = (S.special == SP_ATTACK || S.special == SP_LIMIT) && S.mot ? hit_slot(S.mot->slot, blade) : NULL;  // (s89: + Limit commands)
    float lm[12];
    if (!h || !linkpos_link_mtx(lm)) { S.seg_swing = -1; return 0; }
    float feet[3] = {lm[3], lm[7], lm[11]}, now[6], prev[6];
    blade_world(h, S.clock, feet, S.face, to_u, now);
    if (S.seg_swing == S.swing) memcpy(prev, S.seg, sizeof prev);
    else blade_world(h, clock_old, feet, S.face, to_u, prev);  // (first frame of a swing: its own start, here)
    if (blade == 0 && g_wpn_cur.def) wpn_reach(now), wpn_reach(prev);  // (s128 wpn: the weapon's own length)
    memcpy(S.seg, now, sizeof now), S.seg_swing = S.swing;
    if (!IN_ANTI) kbfx_trail(blade, now, now + 3, ffx_trail_win(S.mot, S.clock));  // (grip, tip: the swing ribbon; s89 kbfx: each blade, its Keyblade's look; Anti has no blade)
    int hits = 0;
    g_bh.n = -1;  // (s87: fetched once, at the first live window)
    for (int w = 0; w < h->nwin; w++) {
        const HitWin *win = &h->win[w];
        if (!win->power || S.clock < win->start || clock_old > win->end) continue;  // (power 0: guard / non-damage)
        if (S.hit_swing != S.swing || S.hit_win != w) S.hit_swing = S.swing, S.hit_win = w, S.nhit = 0;
        if (g_bh.n < 0) g_bh.n = linkpos_enemies(g_bh.actor, g_bh.pos, g_bh.life, 64);
        hits += brk_blade(win, prev, now, to_u);  // (s55: boxes first; a hit sets S.hit, so no repel off the box)
        hits += world_blade(win, prev, now, feet, to_u);  // (s122: trees + Stasis'd objects, also before the repel)
        if (repel_try(win, w, now, prev, feet, to_u)) return hits;  // (the blade met the wall first: no enemy hits; S.mot
                                                              // is now the repel, the windows below are not its own)
        uint64_t *actor = g_bh.actor;
        float (*pos)[3] = g_bh.pos;
        int32_t *life = g_bh.life;
        int n = g_bh.n;
        // (s89 anti) a bodyhits window's shape radius (pad[0], 2 cm units): the enemy cylinder grows by it
        float wrad = win->pad[0] * 2.0f * to_u;
        for (int i = 0; i < n; i++) {
            if (life[i] <= 0) continue;
            int k = 0;
            while (k < S.nhit && S.hit_actor[k] != actor[i]) k++;
            bool rehit = k < S.nhit && g_rehit && win->pad[1] && blade < 2 && S.clock - g_hit_clk[blade][k] >= win->pad[1];
            if (k < S.nhit && !rehit) continue;
            float hx = pos[i][0] - feet[0], hz = pos[i][2] - feet[2];
            if (hx * hx + hz * hz > (8 + wrad) * (8 + wrad)) continue;  // (s65: 5 u out no blade reaches; s87: 8 u, a Guardian's centre)
            float br, bh;
            enemy_body(actor[i], &br, &bh);
            if (sqrtf(hx * hx + hz * hz) > 5.0f + br - g_hit_r + wrad) continue;
            if (wall_hides(pos[i])) {
                static uint64_t said;
                if (said != actor[i])
                    said = actor[i], LOG("[sora] enemy %llx at %.2f %.2f %.2f is behind the wall: no hit\n",
                                         (unsigned long long)actor[i], pos[i][0], pos[i][1], pos[i][2]);
                continue;
            }
            float ep[3] = {pos[i][0], pos[i][1] - wrad, pos[i][2]};  // (s89 anti: wrad 0 = the old test)
            bool hit = sweep_hits_rh(prev, now, ep, br + wrad, bh + 2 * wrad);
            static int hit_log = -1;
            if (hit_log < 0) hit_log = getenv("SAVAGEWIND_SORA_HIT_LOG") != NULL;
            float dx = pos[i][0] - feet[0], dz = pos[i][2] - feet[2];
            if ((hit_log || (g_mkhitlog && IN_MICKEY)) && dx * dx + dz * dz < 25)
                LOG("[sora] sweep slot %d win %d clock %.1f: enemy %llx at %.2f %.2f %.2f, %.2f u from Sora, closest blade "
                    "sample %.2f u; grip %.2f %.2f %.2f tip %.2f %.2f %.2f%s\n", S.mot->slot, w, S.clock,
                    (unsigned long long)actor[i], pos[i][0], pos[i][1], pos[i][2], sqrtf(dx * dx + dz * dz), g_sweep_min,
                    now[0], now[1], now[2], now[3], now[4], now[5], hit ? " HIT" : "");
            if (!hit || !blade_group_free(blade, win->group, actor[i])) continue;
            if (rehit) g_hit_clk[blade][k] = S.clock;  // (s89 anti: Interval re-hit)
            else if (S.nhit < 16) g_hit_clk[blade & 1][S.nhit] = S.clock, S.hit_actor[S.nhit++] = actor[i];
            sl_str_bonus = (berserk_on() ? 2 : 0) + (IN_MICKEY ? mk_str_delta() : 0);  // (s106 Berserk Charge; s110: Mickey's own KH2 Strength)
            sl_dmg_force = blade == 0 ? wpn_dmg(win->power) : 0;  // (s128 wpn: Link's BotW weapon = its Attack, not KH2 Str)
            int dmg = sl_hit(actor[i], life[i], win->power, g_dmg_base, 0);  // (s54 levels; "set lvdmg 0" = the old rule)
            sl_str_bonus = 0, sl_dmg_force = 0;
            if (blade == 0) wpn_wear(1);  // (s128 wpn: one durability per enemy hit, like Link)
            if (blade == 0 && g_wpn_cur.def) LOG("[wpn] hit %llx: life %d -> dmg %d (Attack %d%+d x power %d%% x wpace %d%%)\n", (unsigned long long)actor[i], life[i], dmg, g_wpn_cur.def->power, g_wpn_cur.atk_up, win->power, g_wpace);  // (s129 test log)
            dmg = ab_finisher_dmg(win->kind, dmg);  // (s82 Combo Boost / Air Combo Boost)
            // (s65 audit #12) a finisher flings only small / medium enemies (ah_small, as Thunder): big ones take a
            // plain hit, KH2's big enemies resist blow. Live "set finbig 1" = fling them too (the old rule).
            int f54 = win->kind == ATKP_KIND_NORMAL ? F54_SMALL : g_fin_big || ah_small(actor[i]) ? g_fin_f54 : 0x11;
            int react = win->reaction, kb1 = 0x7fff, kb2 = 0;  // (no row in the table: sstm defaults)
            ah_atkp(win->atkp, &react, &kb1, &kb2);
            // (s55 airhold: a held enemy stays in BotW's BlownOff; s60b: a KH2 hop / blow in the air = Shock, no ragdoll)
            f54 = ah_hit_f54(actor[i], pos[i], f54, react, to_u);
            linkpos_damage_enemy(actor[i], dmg, f54);
            if (IN_VALOR) vl_hit();  // (s70: Valor EXP +1 per hit)
            ah_hit(actor[i], pos[i], feet, react, kb1, kb2, f54, to_u, "blade");
            sora_fx_event(kbfx_name(win->kind != ATKP_KIND_NORMAL ? "hit_finish" : "hit"), g_spark_at ? g_sweep_hit : now + 3, S.face,
                          to_u);  // (KH2 W_EX010 el 2 / 1)
            sora_sfx_hit(win->atkp, 1.0f, pos[i]);
            if (IN_MICKEY) mk_hit(win->atkp, actor[i]);  // (s110m: his own Drive gauge)
            else dg_hit(win->atkp, actor[i]);  // (s67 drive: KH2 hit_dealt)
            if (win->kind != ATKP_KIND_NORMAL) g_fin_hit_actor = actor[i], g_fin_hit_at = vk_frame_counter();  // (s138 review: KH2 sysobj_is_finish_blow, for Peter Pan)
            S.hit = true, hits++;
            LOG("[sora] HIT %llx (life %d) by slot %d%s window %d (frames %d..%d, atkp %d power %d kind %d) at clock %.1f: "
                "damage %d f54 %#x; tip %.2f %.2f %.2f enemy %.2f %.2f %.2f\n", (unsigned long long)actor[i], life[i],
                S.mot->slot, blade ? " (left blade)" : "", w, win->start, win->end, win->atkp, win->power, win->kind, S.clock, dmg, f54, now[3], now[4],
                now[5], pos[i][0], pos[i][1], pos[i][2]);
        }
    }
    return hits;
}

// (s69) Valor's left blade keeps its own per-swing state in these, swapped into S's fields around its sweep (the
// sweep, brk_blade and repel_try all work on S.seg / S.nhit / S.hit_actor): one hit per (swing, window, enemy, blade).
static struct { float seg[6]; int seg_swing, hit_swing, hit_win, nhit; uint64_t hit_actor[16]; } g_blade2 = {.seg_swing = -1};
static void blade2_swap(void) {
    float seg[6];
    uint64_t ha[16];
    int t;
    memcpy(seg, S.seg, sizeof seg), memcpy(S.seg, g_blade2.seg, sizeof seg), memcpy(g_blade2.seg, seg, sizeof seg);
    memcpy(ha, S.hit_actor, sizeof ha), memcpy(S.hit_actor, g_blade2.hit_actor, sizeof ha), memcpy(g_blade2.hit_actor, ha, sizeof ha);
    t = S.seg_swing, S.seg_swing = g_blade2.seg_swing, g_blade2.seg_swing = t;
    t = S.hit_swing, S.hit_swing = g_blade2.hit_swing, g_blade2.hit_swing = t;
    t = S.hit_win, S.hit_win = g_blade2.hit_win, g_blade2.hit_win = t;
    t = S.nhit, S.nhit = g_blade2.nhit, g_blade2.nhit = t;
}

static int ma_body_hits(float clock_old, float to_u);  // (s89 b4-master: sora_master.h, Master's body windows)
static void ma_fx_tick(float clock_old, float to_u);
static int blade_hits(float clock_old, float to_u) {
    int hits = blade_hits_one(0, clock_old, to_u);
    if ((IN_VALOR || IN_MASTER || IN_FINAL || IN_ANTI) && S.special == SP_ATTACK)  // (s89: Master's / Final's 2nd blade, Anti's track 1; kbfx: its own Keyblade's fx)
        blade2_swap(), g_kbfx_blade = 1, hits += blade_hits_one(1, clock_old, to_u), g_kbfx_blade = 0, blade2_swap();
    ffx_tick(to_u);  // (s93 b7-ffx: Final's KH2 swing fx at the motion's frame triggers)
    ffx_air_trail(to_u);  // (s95 fairfx: no swing playing = jump / fall / glide ribbons)
    if (IN_MASTER) hits += ma_body_hits(clock_old, to_u);  // (s89)
    else ma_fx_tick(clock_old, to_u);  // (s89 review: out of Master its attack fx fade, not left behind)
    return hits;
}

// A swing turns to the nearest live enemy within SAVAGEWIND_SORA_TARGET_R u (default 4; 0 = off), x / z out.
// [approx] KH2 picks its target through the ptya Type 1/2 entries and turns over the turn_to_lock range instead.
static bool nearest_enemy(const float p[3], float out[2]) {
    static float r = -1;
    if (r < 0) r = env_f("SAVAGEWIND_SORA_TARGET_R", 4);
    uint64_t actor[64];
    float pos[64][3], bd = r * r;
    int32_t life[64];
    int n = linkpos_enemies(actor, pos, life, 64);
    bool got = false;
    for (int i = 0; i < n; i++) {
        float dx = pos[i][0] - p[0], dz = pos[i][2] - p[2], d = dx * dx + dz * dz;
        if (life[i] > 0 && d < bd && fabsf(pos[i][1] - p[1]) < 3) bd = d, out[0] = pos[i][0], out[1] = pos[i][2], got = true;
    }
    return got;
}

static bool g_on, g_active, g_show_link;
static float g_k = 1.0f;  // KH cm -> u is 0.01 * g_k
static FILE *g_slog;

// ------------------------------------------------------------------ the pad (linkpos's filter tap, any thread)
static volatile uint64_t g_held, g_seen;  // buttons now / pressed at any poll since the last game frame
static volatile int32_t g_lx, g_ly;
static volatile uint64_t g_nc_btn;  // (s143) No Clip: the pad as it was before the attack / jump / roll bits and the stick were taken from Sora
static volatile int32_t g_nc_lx, g_nc_ly;
static volatile uint64_t g_nc_frame;  // presented frame of the last driven No Clip tick (pad_tap only eats the pad within 4 frames of it)

typedef struct { uint64_t from, to, b; int32_t lx, ly, rx, ry; int stop_axis; char stop_op; float stop; bool done; } ScriptWin;
static volatile float g_body[3];  // Link's body position, last game frame (the windows' stop coordinates)
static ScriptWin g_script[128];
static int g_nscript;
static volatile uint64_t g_script_base;  // the windows' frame 0: world entry (0), or the live test's start
static float g_live_stop = -1;           // live "set stop": SAVAGEWIND_SORA_ENEMY_STOP
static int g_swim_ghost = 35;  // live "set ghost <alpha x100>": his body under the water while swimming (0 = off)
static int g_puppet = 1, g_climb = 1, g_wstick = -1;  // live "set puppet / climb / wstick" (session 28: Link's own states)
// s85f #1: a talk / pickup / scripted stretch (Link's state word 0 held, see sora frame): live "set evzero <n>" presents
// of 0 in a row without screen 0x4a, "set evzero4a <n>" with it (0x4a = the message window, last/prev.log old man)
static int g_ev_zero = 15, g_ev_zero4a = 4;
static bool g_msg_screen;  // screen 0x4a exists (load_probe)
static bool g_scr33;  // (s106 chestanim recon) screen 0x33 exists
static bool g_item_screen;  // (s85g) screen 0xb exists: BotW's new-item popup (load_probe)
static int g_itemscr = 1;   // live "set itemscr 0/1": the pick-up window holds while 0xb is up
// (s86 glitch part 2: "use left dpad to switch" after a weapon pick up, the camp-cleared chest reveal) BotW holds Link
// while some screens are up but his state word stays ground, so Sora walked. Screens in this set freeze Sora like the
// item popup (press / speed 0) and give Link the pad (A closes the tip). 0x33 = GUESS from live11 (up right after a
// weapon's 0xb popup closed). Live "set frzscr <id> 0/1" (hex id ok: 0x33) edits it; "set frzlog 1" logs every
// screen that comes or goes while Sora drives (find the ids).
static uint8_t g_frz_mask[32] = {[0x33 / 8] = 1u << (0x33 % 8)};
static int g_frz_id = -1;   // a screen of the set that is up now (-1: none)
static int g_frz_log = 1;   // (on until the ids are confirmed)
static bool g_event;       // last frame was an event stretch: Link owns the pad, the KH2 menu stays shut (#5)
static int g_evfreeze = 0;  // (s94 GUESS default off until one user test; live "set evfreeze 1"; knobs in sora_chestev.h)
static int g_evrec = 0, g_evcam = 18, g_evang = 40, g_evon = 20, g_evoff = 10;
static int g_evmin = 3;  // (s94 review) the angle test only past this many m (first person / bow aim / a wall-pushed camera put Link off axis up close)
static bool g_chev;      // (s94 b8-chestev) the chest-unlock camera event is on (sora_chestev.h): Sora frozen, Link has the pad
static int g_lflash = 1;  // (s87c link flash; live "set lflash 0" = old behaviour)
static int g_puppet_ev = 1;  // live "set puppetev 0": no puppet in state 0x1 (s54 map warp; both hidden as in s53)
static int g_fm_keep_dying = 1;  // (s87) live "set fmdie 0": a Drive Form reverts the moment he dies (the old rule)
static int g_puppet_dead = 1;  // live "set deadpup 0": BotW's own Link while dying (s57 user: Sora over his death ragdoll)
static int g_zl_pass;  // live "set zl 1": BotW sees ZL too (its own Z-targeting camera, the L4 check)
static int g_savepic = 1;  // (s95 b9-savepic) live "set savepic 0": paused frames drop Sora's pose again (the old behaviour)
static int g_savepic_n = 8;  // (s96) live "set savepicn <presents>": how long into a pause the world pose is held (BotW takes
                             // the save picture at the pause start, s96 live test); then it is dropped (no frozen party on the map)
static uint64_t g_savepic_t0;  // (s96) present the current pause started at (0: not paused)
static int g_menu_pass = 1;  // (s86) live "set menupass 0": BotW's pause menu no longer gets ZL / L / d-pad
// (s93 b7-runes, runtime/sora_slate.h) Sheikah Slate: pad_tap keeps L + the d-pad for BotW while g_slate_pass (live "set slatepass 1", recon)
// or g_slate_mode (Slate mode: Link has the pad); the L pulse (frames [from, until)) is the tap that uses the selected rune
static volatile int g_slate_pass, g_slate_mode;
static bool sh_live_set(const char *key, int v);  // (s94 b8-slatehip, runtime/sora_slatehip.h)
static bool cx_live_set(const char *key, int v);  // (s106 b18-chestanim, runtime/sora_chestanim.h)
static bool ca_live_set(const char *key, int v);  // (s102 b16-chestabil, runtime/sora_chestabil.h)
static int mk_survive_mask(void);  // (s109 b22-mickey, runtime/sora_mickey.h)
static bool mk_drops_hits(void);  // (s110 review: Mickey mode drops Link's hits; sora_guard.h must not clear the exe drop)
static void mk_tick(int life, uint64_t present);
static bool mk_live_set(const char *key, int v);
static bool mc_input(uint64_t *press, uint64_t *held);  // (s117 mkchoice, sora_mkchoice.h)
// (s117 review) GetTickCount64 of the last frame the KO choice screen drew: pad_tap keeps A / B / X / Y / the shoulders / the
// left stick from BotW while it is fresh (Link has the pad while climbing / swimming); stale = released on every exit path.
static volatile uint64_t g_mc_pad_ms;
static bool skt_live_set(const char *key, int v);  // (s110 skate, runtime/sora_skate.h)
static bool sm_live_set(const char *key, int v);   // (s136 summon, runtime/sora_summon.h: summon scall sdone sdmg sstr sbars sendcall slog)
static bool sm_hold(void);                         // Sora plays the summon call: no pad
static void sm_fps_input(uint64_t *press, uint64_t *held);  // (s137 fps: FPS Mode's face buttons / R1, runtime/sora_summon_fps.h)
static bool sm_fps_eat_x(void);                    // ... Triangle is Firecracker (BotW gets no A)
static bool sm_fps_eat_rs(void);                   // ... the right stick is the FPS camera's
static bool sm_call_draw(float kh, float to_u);    // ... on the call actor, in place of his body
static bool sm_gauge(int *bars, int *fill, int *max);  // the summon gauge on the Drive plate
static void sm_tick(uint64_t present, int st, const float p[3], bool block);
static void sm_late(float kh, float to_u, int st);
static void sm_end(const char *why);
static bool orb_live_set(const char *key, int v);  // (s131 orbs, runtime/sora_orbs.h)
static bool sv_live_set(const char *key, int v);   // (s131 savept, runtime/sora_savept.h)
static void orb_tick(uint64_t present, const float p[3], bool block);
static void sv_tick(uint64_t present, const float p[3], bool block);
static void sh_tick(uint64_t present);
static volatile uint64_t g_slate_l_from, g_slate_l_until;
static volatile uint64_t g_slate_seen;  // (s93 review: the present sk_tick last ran in Slate mode; a frame body that returns early (no world, Sora mode off) must not leave L + d-pad with BotW)
#define SLATE_LIVE(now) (g_slate_mode && (now) - g_slate_seen < 30)
// (s59) the KH2 Game Over screen (gameover_tick below; recon/S59_HELPER_GAMEOVER.md). Live: "set kh2go 0/1" (0 = BotW's
// screen alone), "set gohold <ms>" (KH2 screen time before BotW's menu takes A again), "set gofade <ms>" (fade in),
// camera "set godist <cm> / goel <deg> / goyaw <deg> / gospin <deg per s> / gofov <deg>", "set gosy <%>" (his centre,
// down the screen), "set goy0 <%>" (the black from there down; 0 = the whole screen, BotW's GAME OVER logo too),
// "set gokb 1" (Keyblade shown, idle grip), "set golog 0/1", "set goblock 0/1" (A held from BotW during the hold).
// (s60 user: Sora covered Continue / Quit - now he floats small in the gap under the logo (~50-60 %), BotW's menu shown
// and usable (no black, no A block), until screen 0x30 goes; gohold > 0 = the old timed screen.)
static int g_kh2go = 1, g_go_hold = 0, g_go_fade = 800, g_go_dist = 900, g_go_el = 8, g_go_yaw = 0, g_go_spin = 0,
           g_go_fov = 30, g_go_sy = 55, g_go_y0 = 100, g_go_kb = 0, g_go_log = 1, g_go_block = 0;
static volatile int g_autocont;  // live "set autocont 1": linkpos's auto-Continue taps after a death reach BotW (pad_tap)
static volatile uint64_t g_go_block_until;  // pad_tap: A (the triangle's A, auto-Continue's taps) kept from BotW until (ms)
static void gameover_off(const char *why);

static void script_parse(const char *spec) {
    static const char *const names[] = {"A", "B", "X", "Y", "LSTICK", "RSTICK", "L", "R", "ZL", "ZR", "PLUS",
                                        "MINUS", "LEFT", "UP", "RIGHT", "DOWN"};
    char buf[4096];
    snprintf(buf, sizeof buf, "%s", spec);
    char *s = NULL;
    for (char *tok = strtok_r(buf, ",", &s); tok && g_nscript < 128; tok = strtok_r(NULL, ",", &s)) {
        unsigned long long from = 0, len = 10;
        char name[32] = "";
        unsigned pct = 100;
        if (*tok == '+') tok++;
        if (sscanf(tok, "%llu:%31[A-Z_0-9]", &from, name) < 2) continue;
        const char *rest = strchr(strchr(tok, ':') + 1, ':');
        const char *at = strchr(tok, '@');
        if (at) sscanf(at + 1, "%u", &pct), rest = strchr(at, ':');
        if (rest) sscanf(rest + 1, "%llu", &len);
        ScriptWin w = {from, from + len, 0, 0, 0, 0, 0, -1, 0, 0, false};
        const char *st = strchr(tok, '~');  // ~<x|y|z><'<'|'>'><value>: the window ends once the body passes it
        if (st && (st[1] == 'x' || st[1] == 'y' || st[1] == 'z') && (st[2] == '<' || st[2] == '>'))
            w.stop_axis = st[1] - 'x', w.stop_op = st[2], w.stop = strtof(st + 3, NULL);
        int32_t m = (int32_t)lroundf(32767.0f * (float)pct / 100.0f);
        if (!strcmp(name, "STICK_UP")) w.ly = m;
        else if (!strcmp(name, "STICK_DOWN")) w.ly = -m;
        else if (!strcmp(name, "STICK_LEFT")) w.lx = -m;
        else if (!strcmp(name, "STICK_RIGHT")) w.lx = m;
        else if (!strcmp(name, "STICK_ENEMY")) w.b = BTN_TEST_ENEMY;
        else if (!strcmp(name, "RSTICK_UP")) w.ry = m;
        else if (!strcmp(name, "RSTICK_DOWN")) w.ry = -m;
        else if (!strcmp(name, "RSTICK_LEFT")) w.rx = -m;
        else if (!strcmp(name, "RSTICK_RIGHT")) w.rx = m;
        else if (!strncmp(name, "STICK_", 6) && name[6] >= '0' && name[6] <= '9') {
            float r = (float)atoi(name + 6) * PI / 180;
            w.lx = (int32_t)lroundf(cosf(r) * (float)m), w.ly = (int32_t)lroundf(sinf(r) * (float)m);
        } else {
            int i = 0;
            while (i < 16 && strcmp(name, names[i])) i++;
            if (i == 16) { LOG("[sora] input: unknown %s\n", name); continue; }
            w.b = 1ull << i;
        }
        g_script[g_nscript++] = w;
    }
    LOG("[sora] input: %d scripted windows (frames from world entry)\n", g_nscript);
}

static int32_t clamp_axis(int32_t v) { return v > 32767 ? 32767 : v < -32767 ? -32767 : v; }

static volatile uint64_t g_climb_until;  // Link has the pad until this present (a climb try, sora_mod_frame)
#define CLIMB_TRY 24           // presents
#define BTN_CLIMB (1ull << 2)  // Switch X: BotW's "X Climb" at a wall (its stick push alone doesn't grab here: climb28a)
// (s50 user wish: a glide into a wall climbs it. The air try sends no X - X in the air opens BotW's paraglider - and
// no B, the held glide B: BotW grabs a wall a falling Link is pushed into on its own)
static volatile int g_climb_air;
static volatile int g_climb_nox;  // (s85f #6: the try with no wall in front: Link gets the pad (triangle = A, pick up) but no X pulse)
// (s89b user: "spam triangle while walking forward speeds me up": each no-wall try gives Link the pad >= 15 presents and
// BotW runs him at its own speed (log: ~370 mm / present), Sora following. In that window the stick BotW sees is capped at
// g_nox_stick % (GUESS: a BotW walk) so a pick-up still works but no faster run. Live "set noxstick 100" = old.
static volatile int g_nox_stick = 45;
static int g_gclimb = 1;  // live "set gclimb 0/1"
static int g_climb_air_ok = 0;  // (s117) live "set climbair 1": the ground climb try may start while Sora is in the air (old rule)
// (s88 b3-climb A) "set glideclimb 0/1/2": a glide into a climbable wall hands over to BotW's climb. 0 = the old rule only
// (glide stopped 6 frames, the real stick, no X). 1 = + a wall ray along the glide starts the try at once and the stick
// Link gets points INTO the wall (the real stick may be sideways after the camera moved). 2 = + if Link has not grabbed
// after GCL_X_AFTER presents, X pulses (BotW's paraglider; a paraglider pushed into a wall grabs it) and a stray
// paraglider is closed again (B tap) if no climb came.
// (s88 b3-climb B) "set kickup 0/1": Sora draws Link's own kick-up / jump-to-grab (X at a wall) bone for bone.
static int g_glideclimb = 2, g_kickup = 1;
static volatile float g_gcl_dir[2];                    // world dir into the wall (x, z), set by the tick for pad_tap
static volatile uint64_t g_gcl_watch_until, g_gcl_cancel_until;
// (s89b user: "glide into a wall doesn't climb, but the glide stops": log = Link grabbed (0x90) and was off the wall 8
// presents later. The try strips B, but the grab ends the try and the still-held glide B (BotW's B = let go of a wall)
// reached BotW. Now a grab after a glide keeps B eaten until the player lets go of it once. Live "set gclholdb 0" = old.
static volatile int g_gcl_eatb, g_gcl_holdb = 1;
#define GCL_X_AFTER 8  // presents into the air try before the X pulses (GUESS: the no-X try grabbed in s47/s52 logs)
// (s92 hud) live "set hearts 0/1": hide BotW's top-left hearts (hearts_tick); "set enemyhud 0/1": the locked enemy's KH2 gauge at the
// top-left where they were (needs an exe with sora_hud_enemy_place); "set enx / eny <HUD units>" its left end / centre line (0 = default)
static int g_hearts_hide = 1, g_enemy_left = 1, g_enx, g_eny;
static int g_wheel = 1;   // live "set wheel 0/1": BotW's stamina wheel with Link hidden (wheel_tick)
#define CLIMB_X(now) (!g_climb_air && !g_climb_nox && (now) < g_climb_until && (now) + CLIMB_TRY - 6 >= g_climb_until)

// Glide (session 31, the user's KH2 way): B pressed again in the air and held once Sora falls -> a glide try: the
// servo lets go (BotW's own fall), Link gets the pad and pad_tap pulses X (BotW opens the paraglider with X in the
// air) for GLIDE_TRY presents. His glide state -> KH2's Glide motions on BotW's paraglider physics + stamina. The held
// B stays Sora's (BotW never sees it); letting it go taps BotW's cancel. g_glide: 0 off, 1 the try, 2 gliding.
static volatile int g_glide;
static volatile uint64_t g_glide_until, g_glide_cancel_until;
static int g_glide_on = 1;                   // live "set glide 0/1"
// (s58) live "set glidestam 0/1/2": the KH2 glide pays BotW's paraglider stamina (glidestam_tick). 1 = through Link's
// own stamina delta (BotW's accounting), 2 = a direct PlayerInfo write (fallback), 0 = off (the s32 port: free)
static int g_glidestam = 1;
static void glidestam_set(int v);  // (s58 test knob "set stam <n>": Link's current stamina = n, 1000 a wheel)
static volatile uint64_t g_glide_cancel = 1ull << 1;  // live "set gcancel <mask>": BotW's paraglider close (B?)
#define GLIDE_TRY 40           // presents
#define BTN_GLIDE (1ull << 2)  // Switch X: BotW's jump, the paraglider in the air

static int g_tri_pickup = 1;  // live "set tri 0/1": the pad's own X (PS5 triangle) reaches BotW as A (pick up / talk)
static volatile int g_link_owns;  // Link's own state (swim, climb...): X stays BotW's X (swim dash, wall jump: s49)

// The lock-on camera (lockcam_tick): the right stick x Sora reads (after the script windows), the stick x pad_tap adds
// for BotW's camera, and whether the real one is eaten (a plain flick switches the target).
#define BTN_R2 (1ull << 9)  // Switch ZR: KH2's R2 (+ right stick left / right = switch the lock-on target)
static volatile int32_t g_rx_raw, g_ry_raw, g_cam_rx;
static volatile int g_eat_rx;
static volatile int g_eat_zr;  // (s44: locked with `set flick 0`, ZR is KH2's R2 switch modifier - BotW would draw the bow)

// (s78) the KH2 pause menu (camp): runtime/sora_camp.h. pad_tap raises the + request and, while it is up, eats the pad.
static volatile int g_camp_on = 1, g_camp_req, g_camp_eat, g_camp_pass_plus, g_camp_pulse;
static volatile uint64_t g_camp_swallow;  // (s92) buttons held at the BotW Menu handover: kept from BotW until released
static int g_bmswallow = 1;                // live: bmswallow 0/1 (0 = the old leak: the confirm press selects in BotW's menu)
static float g_hud_last[4];  // (the last sora_hud_set args: Sora's HP / MP for the camp's status rows)
static void camp_pad(uint64_t *b, int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry, uint64_t now);
static void camp_abort(const char *why);
static bool camp_live_set(const char *key, int v);

static void pad_tap(uint64_t *b, int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry) {
    uint64_t now = vk_frame_counter();
    // (s60 user: "turn off the auto load once sora dies, i can load the game myself") linkpos's auto-Continue ORs A in
    // on frames f % 60 < 8 after a death (title taps untouched: linkpos_world_frame 0 there). Drop those taps; an A
    // already held before the window opened is the player's and stays. Live "set autocont 1" = the old taps.
    static bool a_held;
    bool a_now = *b & BTN_ATTACK;
    if (!g_autocont && linkpos_world_frame() && now % 60 < 8 && a_now && !a_held && linkpos_link_life() <= 0) *b &= ~BTN_ATTACK;
    else a_held = a_now;
    uint64_t t0 = g_script_base ? g_script_base : linkpos_world_frame();
    if (g_nscript && t0 && vk_frame_counter() >= t0) {
        uint64_t f = vk_frame_counter() - t0;
        for (int i = 0; i < g_nscript; i++) {
            ScriptWin *w = &g_script[i];
            if (f < w->from || f >= w->to || w->done) continue;
            if (w->stop_axis >= 0) {
                float c = g_body[w->stop_axis];
                if (w->stop_op == '<' ? c < w->stop : c > w->stop) {
                    w->done = true;
                    LOG("[sora] input window %d reached its stop (%c %.3f) at rel frame %llu\n", i, 'x' + w->stop_axis,
                        c, (unsigned long long)f);
                    continue;
                }
            }
            *b |= w->b;
            static int s135_said = -1;  // (s135 diag) the window reaches the pad
            if (s135_said != i) s135_said = i, LOG("[sora] s135 input window %d on at rel frame %llu (lx %d ly %d)\n", i, (unsigned long long)f, w->lx, w->ly);
            *lx = clamp_axis(*lx + w->lx), *ly = clamp_axis(*ly + w->ly);
            *rx = clamp_axis(*rx + w->rx), *ry = clamp_axis(*ry + w->ry);
        }
    }
    // (after the script windows: a scripted X is the pad's own X, s49 - before, the triangle rule below never saw it)
    uint64_t raw_x = *b & BTN_CLIMB;  // (s40 user: X = BotW's jump made Link hop under Sora; A is BotW's pick up)
    if (CLIMB_X(now)) *b |= BTN_CLIMB;  // (X for the try's first 6 presents)
    g_held = *b, g_lx = *lx, g_ly = *ly, g_rx_raw = *rx, g_ry_raw = *ry;
    if (g_dbm_clip && vk_frame_counter() - g_nc_frame <= 4 && !g_camp_eat && !*(volatile uint32_t *)(uintptr_t)PAUSE_STATE) {  // (s143 dbggod No Clip) the stick + B / Y move the body (dbm_clip_tick): Sora and BotW see neither
        g_nc_btn = *b, g_nc_lx = *lx, g_nc_ly = *ly;
        *b &= ~(BTN_ATTACK | BTN_JUMP | BTN_ROLL | BTN_CLIMB), *lx = *ly = 0;
        g_held = *b, g_lx = 0, g_ly = 0;
    }
    if (g_eat_rx) *rx = 0;
    if (sm_fps_eat_rs()) *rx = 0, *ry = 0;  // (s137 fps: g_rx_raw / g_ry_raw above keep the real stick for the FPS camera)
    if (g_eat_zr) *b &= ~BTN_R2;
    if (g_cam_rx) *rx = clamp_axis(*rx + g_cam_rx);
    __atomic_fetch_or(&g_seen, *b, __ATOMIC_RELAXED);
    // (s86 glitch part 2: "in the botw menu i cant press left trigger to go to the left page": ZL was always eaten as
    // KH2's lock-on. BotW's pause menu / map (pause state 2) keeps ZL, L and the d-pad: Sora's tick doesn't run there,
    // and the KH2 pause menu, which also sits on pause state 2, still eats the whole pad in camp_pad below)
    bool bmenu = *(volatile uint32_t *)(uintptr_t)PAUSE_STATE == 2 && g_menu_pass;
    *b &= ~BTN_TEST_ENEMY;
    if (!bmenu && !g_slate_pass && !SLATE_LIVE(now)) *b &= ~BTN_MENU;  // (Sora's only: the game never sees them - the d-pad and L are KH2's menu; s93: unless the Slate)
    if (SLATE_LIVE(now) && now >= g_slate_l_from && now < g_slate_l_until) *b |= BTN_SHORTCUT;  // (s93: the L tap that uses the rune)
    if (!g_zl_pass && !bmenu) *b &= ~BTN_LOCK;  // (ZL is KH2's lock-on)
    // (KH2's triangle = the reaction command: BotW gets A instead of the jump; Sora still saw the raw pad above, and
    // the climb try's own X pulse is kept)
    // (s110m review: while Mickey is out the held Triangle is his D-Charge: neither BotW's A (pick up / talk / open) nor its X jump)
    if (raw_x && (mk_eat_x() || skt_eat_x() || rc_eat_x() || pl_eat_x() || sm_fps_eat_x()) && !g_link_owns && !CLIMB_X(now) && !*(volatile uint32_t *)(uintptr_t)PAUSE_STATE) *b &= ~BTN_CLIMB;
    else if (g_tri_pickup && raw_x && !g_link_owns && !CLIMB_X(now)) *b = (*b & ~BTN_CLIMB) | BTN_ATTACK;
    bool gcl_menu = *(volatile uint32_t *)(uintptr_t)PAUSE_STATE != 0;  // (s88 review: no stick / X / B into a menu)
    if (g_climb_air && now < g_climb_until) {
        *b &= ~(BTN_JUMP | BTN_CLIMB);  // (the glide's try: no B, no X)
        if (gcl_menu) {}
        else if (g_glideclimb >= 1 && (g_gcl_dir[0] != 0 || g_gcl_dir[1] != 0)) {  // (s88: the stick points into the wall)
            float cf[2] = {0, 1}, d0 = g_gcl_dir[0], d1 = g_gcl_dir[1];
            if (!(g_wstick > 0)) linkpos_camera_fwd(cf);
            *ly = (int32_t)((d0 * cf[0] + d1 * cf[1]) * 30000.0f);  // inverse of the tick's dir = cf*sy + right*sx
            *lx = (int32_t)((d1 * cf[0] - d0 * cf[1]) * 30000.0f);
        }
        // (s88 review: X only for a wall-ray try - a glide stalled by a headwind (b3-ride's wind) or an enemy's body also
        // passes the old "stopped" rule, and X there would open BotW's paraglider in mid-air)
        if (g_glideclimb >= 2 && !gcl_menu && (g_gcl_dir[0] != 0 || g_gcl_dir[1] != 0) && now + CLIMB_TRY - GCL_X_AFTER >= g_climb_until && now / 4 % 2 == 0) *b |= BTN_CLIMB;
    }
    if (now < g_gcl_cancel_until && !gcl_menu) *b |= g_glide_cancel;  // (s88: close a paraglider that found no wall)
    if (g_climb_nox && now < g_climb_until && g_nox_stick < 100) {  // (s89b: the pick-up window's stick = a walk at most)
        float m = hypotf((float)*lx, (float)*ly), cap = 32767.0f * g_nox_stick / 100.0f;
        if (m > cap) *lx = (int32_t)(*lx * cap / m), *ly = (int32_t)(*ly * cap / m);
    }
    if (g_gcl_eatb) {  // (s89b: the held glide B after a wall grab is not BotW's "let go"; released once = the player's again)
        if (*b & BTN_JUMP) *b &= ~BTN_JUMP;
        else g_gcl_eatb = 0;
    }
    // (the glide try's X is BotW's jump on the ground: only pulse once he is falling. His state word still reads
    // ground 0x18000002 for a few presents after the servo lets go mid-fall, so not gated on LINK_GROUND; instead
    // skip the try's first 3 presents and then pulse only while the body height dropped over the last 4 presents)
    static float gy[8];      // body height per present (ring)
    static uint64_t gstamp[8];
    if (gstamp[now % 8] != now) gstamp[now % 8] = now, gy[now % 8] = g_body[1];
    bool dropping = gstamp[(now - 4) % 8] == now - 4 && g_body[1] < gy[(now - 4) % 8] - 0.005f;
    if (g_glide && !*(volatile uint32_t *)(uintptr_t)PAUSE_STATE) {  // (paused: the menu keeps B - review s32 #2)
        *b &= ~BTN_JUMP;  // (the held B is Sora's glide hold, not BotW's)
        if (g_glide == 1 && now < g_glide_until && now + GLIDE_TRY - 3 >= g_glide_until && dropping && now / 4 % 2 == 0)
            *b |= BTN_GLIDE;  // (X 4 on, 4 off: presses)
        if (now < g_glide_cancel_until) *b |= g_glide_cancel;
    }
    if (g_go_block_until && GetTickCount64() < g_go_block_until) *b &= ~BTN_ATTACK;  // (s59 KH2 Game Over: last, after X -> A)
    // (s117 review) the KO choice screen owns the pad (Sora saw it in g_held above); + / - / the camera stick stay BotW's
    if (g_mc_pad_ms && GetTickCount64() - g_mc_pad_ms < 150 && !*(volatile uint32_t *)(uintptr_t)PAUSE_STATE)
        *b &= ~(0x3dfull | BTN_MENU), *lx = *ly = 0;  // (A B X Y, L-stick click, L R ZL ZR as linkpos's block + the d-pad)
    camp_pad(b, lx, ly, rx, ry, now);  // (s78 camp: last of all)
}

// One KH2 stick axis (re:0x39C180): the byte - 127.5, dead zone 60, rescaled over the remaining 67.5.
static int g_stick_round = 1;  // (s65) live "set stickround 0/1": the move stick's dead zone round (1) or per axis (0)
static float kh_axis(int32_t v) {
    float c = (float)v / 32767.0f * 127.5f;
    if (fabsf(c) <= KH_STICK_DZ) return 0;
    return (c - copysignf(KH_STICK_DZ, c)) / (127.5f - KH_STICK_DZ);
}

static float wrap(float a) {
    while (a > PI) a -= 2 * PI;
    while (a < -PI) a += 2 * PI;
    return a;
}

// ------------------------------------------------------------------ ACTION_JUMP (re:0x3CBB00 / 0x3CBC30 / 0x3CB360)
static void arc(float h) {
    S.air = 0, S.jh = h, S.apex = sqrtf(2 * h / KH_G), S.ceiling = false, S.req_set = false;
}

static void enter_jump(float z_up, bool set_vel) {  // re:0x3CBD30: the base is the height Sora is at now
    S.act = ACT_JUMP, S.base = z_up, S.peak = z_up, S.base0 = z_up;
    if (set_vel) S.vel[0] = S.hvel[0], S.vel[1] = S.hvel[1];
}

static void fall_start(float h, float z_up) {  // a jump arc of height h from here, horizontal velocity kept
    bool was_air = S.act == ACT_JUMP;
    float peak = S.peak, base0 = S.base0;
    arc(h);
    enter_jump(z_up, false);
    S.jump_held = S.jump_released = false;  // (_leave clears 0x70000, kh2ctl.py:651)
    if (was_air) S.peak = peak, S.base0 = base0;  // (the log's peak stays the whole jump's, over its first base)
}

static float arc_at(float air) {
    if (S.apex <= air) {
        float t = air - S.apex;
        return S.base + S.jh - KH_G * t * t * 0.5f;
    }
    float f = S.apex > 0 ? air / S.apex : air;
    return S.base + S.jh * sinf(sinf(f * PI * 0.5f) * PI * 0.5f);
}
static float arc_target(void) { return arc_at(S.air); }

// (s92 b6-formfx) a Drive Form's motions play the triggers of the FORM's own mset (footsteps / swing whooshes / voice rows of
// P_EX100_<form>.mset + its weapon msets), packed in sfx.bin at slot + 10000 * (form + 1) (tools_s92/formfx/sfxpack.py). Before,
// a form used the BASE Sora actor's triggers for its slot number. Needs the s92 sfx.bin (marker name formse_v1); an older one =
// the raw slot, as before. Limit's K4xx slots >= 844 stay raw (s91). Live "set formse 0" = the raw slot always.
static int g_formse_on = 1;
static int g_roxsfx = 1;  // (s110 roxsfx) live "set roxsfx 0": Roxas silent + Sora's ribbon windows as before (sora_roxsfx.h)
static int g_formfx_on = 1;  // (s92 b6-formfx) live "set formfx": Limit / Final / Anti entry fx + Anti entry SE (sora_form.h)
static int sfx_slot(const Mot *m) {
    if (!m) return -1;
    if (IN_MICKEY) return mk_se_ok() ? m->slot + 10000 * ((int)g_fform + 1) : -1;  // (s110m mickeyfx: his own rows at 80000+slot (mkse_v1); an old pack / mkfx 0 = silent, never Sora's voice)
    if (IN_ROXAS) return g_roxsfx && sora_sfx_id("roxse_v1") >= 0 ? m->slot + 10000 * ((int)g_fform + 1) : -1;  // (s110 roxsfx: his own rows at 70000+slot; an old pack / key 0 = silent, never Sora's voice)
    if (!g_fmots_on || !g_formse_on || (IN_LIMIT && m->slot >= 844) || sora_sfx_id("formse_v1") < 0) return m->slot;
    return m->slot + 10000 * ((int)g_fform + 1);
}
static int g_af_on = 1;  // (s107 b19-abilfx: live "set abilfx 0" = no combo-ability fx / SE; sora_abilfx.h)
static int g_lf_on = 1;  // (s91 b5-limfx: live "set limfx 0" = no Limit fx / K4xx slot SE; sora_limfx.h)
#define LF_SE_OK(m) (g_lf_on || !(IN_LIMIT && (m)->slot >= 844))  // (Limit-only mset slots 844..992 carry the new SE triggers)
static void set_motion(int m) {
    if (S.motion != m) {
        S.motion = m, S.clock = 0, S.mot = mot_of(m);
        if (S.mot && LF_SE_OK(S.mot)) sora_sfx_motion(sfx_slot(S.mot), -1, 0, NULL);  // (frame-0 triggers: grunts, barks)
    }
}

static int g_play_trig;  // (s88 review) sounds the last play()'s frame-0 triggers started (fm_drive's voice check)
static void play(int m) {  // a cut to the motion's start, also when it is already playing (the next swing)
    S.motion = m, S.clock = 0, S.mot = mot_of(m), S.swing++;
    g_play_trig = S.mot && LF_SE_OK(S.mot) ? sora_sfx_motion(sfx_slot(S.mot), -1, 0, NULL) : 0;
}
// (s95) the Aerial Dodge's stamina (user's glitch list part 4: "Air Dodge / double jump uses 1/4 of BotW's stamina
// wheel"; KH2 itself has no stamina, so this is the user's rule, not a port). Live: set dodgestam <n> = the cost (1000 a
// wheel; default 250 = 1/4, 0 = free = old), set dodgeempty 1/0 = no dodge while the stamina is out (BotW's exhausted rule,
// as the glide; default 1) / dodge anyway. Paid at the dodge's start through the glide's path (glidestam_tick's modes).
// (s97) the ground Dodge Roll pays too (user: "make dodge roll take 1/8 stamina"): set rollstam <n> (default 125 = 1/8,
// 0 = free = old); stamina out + dodgeempty 1 = no roll.
static int g_dodgestam = 250, g_dodgeempty = 1, g_rollstam = 125;
static bool stam_pay(const char *who, int cost);  // false = refused (stamina out); defined after glidestam_tick
#define dodgestam_pay(who) stam_pay(who, g_dodgestam)
#include "sora_master.h"  // (s89 b4-master: Master Form's Aerial Dodge + its body attack windows)
#include "sora_bdodge.h"  // (s91 b5-dodge: base Sora tap-B Aerial Dodge, hold-B Glide)

// ------------------------------------------------------------------ the attack press (KH2's, without targets yet)
// PHH's port of re:0x3D6CA0 (gate), re:0x402750 (eligible), re:0x402C50 (pick), re:0x4028D0/0x402D20 (select) is the
// reference (SORA_DRIVER_RECON.md 3.4); here only the Type 3 "always" entries and the defaults (no lock-on, no
// enemies, no abilities, no wall probe). With no target base Sora's ground combo is A300 -> A302 -> A304.
#define NPTYA_MAX ((int)(sizeof kh_ptya / sizeof *kh_ptya))
// (s69 drive slice 3) in Valor Form its own ptya set (pointer 2), combo limits 4 / 4, prty speeds + jump
// (s72) Wisdom Form: its ptya set (pointer 3), combo limits, prty speeds + jump (no High Jump: Wisdom grows Quick Run)
// (s89) Anti Form: its ptya set (pointer 6), combo limits 3 / 3 / 1 (plrp 0x86: no Combo Plus), prty Run 16 / Turn 0.2618
// (s89 b4-final) Final Form: ptya set 5, combo 3 / 3 / 1 (plrp 0x85: no Combo Plus), prty Run 16 / Jump 585 / Turn 0.196
// (s89 b4-limit) Limit Form: ptya set 66 (NeoMoveset 66); its plrp 0x83 row has no Combo Plus = 3 / 3 / 1; prty 66
// (s89 b4-master) Master Form: its ptya set (pointer 4: every entry an air swing, the ground ones jump up), combo 3 / 5
// (+ Air Combo Plus at Master LV4 / LV6), prty Run 10 / Jump 285
#define NPTYA (IN_MICKEY ? MICKEY_PTYA_N : IN_ROXAS ? ROXAS_PTYA_N : IN_ANTI ? ANTI_PTYA_N : IN_FINAL ? FINAL_PTYA_N : IN_LIMIT ? LIMIT_PTYA_N : IN_MASTER ? MASTER_PTYA_N : IN_WISDOM ? WISDOM_PTYA_N : g_fmots_on ? VALOR_PTYA_N : NPTYA_MAX)
#define PTYA (IN_MICKEY ? mickey_ptya : IN_ROXAS ? roxas_ptya : IN_ANTI ? anti_ptya : IN_FINAL ? final_ptya : IN_LIMIT ? limit_ptya : IN_MASTER ? master_ptya : IN_WISDOM ? wisdom_ptya : g_fmots_on ? valor_ptya : kh_ptya)
#define F_COMBO_GROUND_ (IN_MICKEY ? MICKEY_COMBO_GROUND_ : IN_ROXAS ? ROXAS_COMBO_GROUND_ : IN_ANTI ? ANTI_COMBO_GROUND : IN_FINAL ? FINAL_COMBO_GROUND : IN_LIMIT ? LIMIT_COMBO_GROUND : IN_MASTER ? MASTER_COMBO_GROUND : IN_WISDOM ? WISDOM_COMBO_GROUND + g_wis44 : g_fmots_on ? VALOR_COMBO_GROUND + vl_combo_plus() : KH_COMBO_GROUND)  // (s70 Combo Plus LV4 / LV6)
#define F_COMBO_AIR_ (IN_MICKEY ? MICKEY_COMBO_AIR_ : IN_ROXAS ? ROXAS_COMBO_AIR_ : IN_ANTI ? ANTI_COMBO_AIR : IN_FINAL ? FINAL_COMBO_AIR : IN_LIMIT ? LIMIT_COMBO_AIR : IN_MASTER ? MASTER_COMBO_AIR + ml_air_plus() : IN_WISDOM ? WISDOM_COMBO_AIR + g_wis44 : g_fmots_on ? VALOR_COMBO_AIR : KH_COMBO_AIR)
#define F_COMBO_FINISHERS_ (IN_MICKEY ? MICKEY_COMBO_FINISHERS : IN_ROXAS ? ROXAS_COMBO_FINISHERS : IN_ANTI ? ANTI_COMBO_FINISHERS : IN_FINAL ? FINAL_COMBO_FINISHERS : IN_LIMIT ? LIMIT_COMBO_FINISHERS : IN_MASTER ? MASTER_COMBO_FINISHERS : IN_WISDOM ? WISDOM_COMBO_FINISHERS : g_fmots_on ? VALOR_COMBO_FINISHERS : KH_COMBO_FINISHERS)
// (s109 roxas) combo limits: ptya has R300 -> R301 -> R302 + finisher R304 on the ground, R330 -> R331 + finisher R333 in the air (no R332 slot) = 3 / 2 / 1
// (VERIFIED from the mset slot list; the baked ROXAS_COMBO_* = Valor's plrp row, not used)
#define ROXAS_COMBO_GROUND_ 3
#define ROXAS_COMBO_AIR_ 2
// (s110 b22-mickey) Mickey: ground A330 -> A331 + finisher A335, air A330 -> A331 -> A332 + finisher (GUESS from the ptya entries; the baked 4 / 4 is Valor's row)
#define MICKEY_COMBO_GROUND_ 2
#define MICKEY_COMBO_AIR_ 3
// (s89 trial) Wisdom's combo limits come from its OWN plrp row 0x82 (no Combo Plus: 3 / 3); sora_params_wisdom.h was
// baked with Valor's 0x81 (4 / 4). Live "set wis44 1" = the old 4 / 4.
static int g_wis44 = 0;
// (s82) Sora's abilities on top, in forms too (KH2 applies Sora's list then the form's, re:0x4014A0): Negative Combo
// ground / air -1, Finishing Plus finishers +1 (sora_abil.h ab_limits)
static int g_comboabil = 1, g_combolog = 0;  // (s105) live "set comboabil 0/1", "set combolog 0/1" (ptya_ability_ok below)
// (s106) Berserk Charge (item 395; game text: "Increases strength by 2 during MP Charge and enables combos to continue
// endlessly. The combo finishing move becomes unavailable." VERIFIED). Live "set berserk 0/1" (default 1, under comboabil).
static bool mp_charging(void);
static int g_berserk = 1;
static bool berserk_on(void) { return g_comboabil && g_berserk && PTYA == kh_ptya && ab_on(AB.on, 395) && mp_charging(); }  // (review: base Sora only, as the other abilities)
static int ab_combo_lim(int which) {
    int g = F_COMBO_GROUND_, a = F_COMBO_AIR_, f = F_COMBO_FINISHERS_;
    ab_limits(AB.on, &g, &a, &f);
    if (berserk_on()) g = a = 99;  // (endless; the finisher entries are dropped in ptya_eligible)
    // (s85) the Keyblade's ability, no AP: Star Seeker's Air Combo Plus +1 air; in Valor the left sub-weapon's too
    // (vanilla help 0x3642); Damage Control / Form Boost / Drive Boost = no effect yet
    // (s89 trial) the 2nd slot's ability where the 2nd blade is drawn: Valor, Master, Final (KH2: their own sub-weapon rows)
    if (kb_ability(KB.kb[KB_MAIN]) == 163 || ((IN_VALOR || IN_MASTER || IN_FINAL) && kb_ability(kb_sub_id(KB.kb, g_fform)) == 163)) a++;  // (s91 mfkb: that form's own 2nd Keyblade)
    if (g_comboabil && !g_fmots_on && kb_ability(KB.kb[KB_MAIN]) == 162) g++;  // (s105) Sleeping Lion's Combo Plus: +1 ground
    return which == 0 ? g : which == 1 ? a : f;
}
#define F_COMBO_GROUND ab_combo_lim(0)
#define F_COMBO_AIR ab_combo_lim(1)
#define F_COMBO_FINISHERS ab_combo_lim(2)
// (s82) Combo Boost (ATKP kind 1 ComboFinisher) / Air Combo Boost (kind 2): + min(combo length, d74) x 10 %
static int ab_finisher_dmg(int kind, int dmg) {
    int id = kind == 1 ? AB_COMBO_BOOST : kind == 2 ? AB_AIR_COMBO_BOOST : 0;
    if (!id || !ab_on(AB.on, id)) return dmg;
    int pct = ab_combo_pct(kind == 1 ? F_COMBO_GROUND : F_COMBO_AIR, S.d74), d = ab_boost(dmg, pct);
    LOG("[sora] %s: finisher +%d%% (d74 %d) dmg %d -> %d\n", kind == 1 ? "Combo Boost" : "Air Combo Boost", pct, S.d74, dmg, d);
    return d;
}
#define F_RUN (IN_MICKEY ? MICKEY_RUN : IN_ROXAS ? ROXAS_RUN : IN_ANTI ? ANTI_RUN : IN_FINAL ? FINAL_RUN : IN_LIMIT ? LIMIT_RUN : IN_MASTER ? MASTER_RUN : IN_WISDOM ? WISDOM_RUN : g_fmots_on ? VALOR_RUN : KH_RUN)
#define F_WALK (IN_MICKEY ? MICKEY_WALK : IN_ROXAS ? ROXAS_WALK : IN_ANTI ? ANTI_WALK : IN_FINAL ? FINAL_WALK : IN_LIMIT ? LIMIT_WALK : IN_MASTER ? MASTER_WALK : IN_WISDOM ? WISDOM_WALK : g_fmots_on ? VALOR_WALK : KH_WALK)
#define F_TURN (IN_MICKEY ? MICKEY_TURN : IN_ROXAS ? ROXAS_TURN : IN_ANTI ? ANTI_TURN : IN_FINAL ? FINAL_TURN : IN_LIMIT ? LIMIT_TURN : IN_WISDOM ? WISDOM_TURN : KH_TURN)  // (s72: prty TurnSpeed; Valor's = base Sora's 0.2618; Master's = 0.2618 too)
#define F_JUMP_H (IN_MICKEY ? MICKEY_JUMP_H : IN_ROXAS ? ROXAS_JUMP_H : IN_ANTI ? ANTI_JUMP_H : IN_FINAL ? FINAL_JUMP_H : IN_LIMIT ? LIMIT_JUMP_H : IN_MASTER ? MASTER_JUMP_H : IN_WISDOM ? WISDOM_JUMP_H : g_fmots_on ? vl_jump_h() : vl_base_jump_h(KH_JUMP_H))  // (s70: High Jump by Valor LV; s89 Limit prty 66 = 285; Master prty 285, Master grows the Aerial Dodge instead)

static bool finisher_ok(uint32_t bits, int offset, bool air) {  // re:0x3D7780: the air / ground limit (+0x201 / +0x200)
    return (bits & RB_ALLOW_FINISHER) || S.d74 + offset >= (air ? F_COMBO_AIR : F_COMBO_GROUND);
}

static bool combo_over(uint32_t bits, bool air) {  // re:0x3D7850
    if (!(bits & RB_ALLOW_FINISHER) && S.d74 < (air ? F_COMBO_AIR : F_COMBO_GROUND)) return false;
    return S.d75 > 1;
}

// (s105 comboabil) KH2's ptya Ability is the ability's STAT id (03system item record +4: Guard 82 -> 1, Slapshot 262 -> 90 ...
// VERIFIED by dumping the type 0x13 records). Until s105 every entry with one was ineligible, equipped or not.
// Live "set comboabil 0" = that old rule; "set combolog 1" logs the combo plan once per combo.
static const struct { uint16_t stat, item; } PTYA_AB[] = {
    {18, 137}, {90, 262}, {91, 263}, {92, 264}, {93, 265}, {94, 266}, {95, 267}, {96, 268}, {97, 269}, {98, 270},
    {99, 271}, {100, 272}, {101, 273}, {177, 559}, {178, 560}, {179, 561}, {180, 562},
};
// an ability-gated entry plays only in base Sora, with the ability equipped, its motion baked AND its hit windows baked
// (A315 / A316 / A317 / A346 have none in sora_hits.bin: a swing that never hits stays out)
static bool ptya_ability_ok(const KhPtya *e) {
    if (!g_comboabil || g_fmots_on) return false;
    for (size_t i = 0; i < sizeof PTYA_AB / sizeof *PTYA_AB; i++) {
        if (PTYA_AB[i].stat != e->ability) continue;
        const Mot *m = ab_on(AB.on, PTYA_AB[i].item) ? mot_of(e->motion) : NULL;
        return m && hit_slot(m->slot, 0);
    }
    return false;
}

static bool ptya_eligible(const KhPtya *e, uint32_t bits, bool air) {
    if (!e->motion || (e->ability && !ptya_ability_ok(e))) return false;
    if ((e->flag & PTYA_FINISHER) && berserk_on()) return false;  // (s106 Berserk Charge: no finishing move)
    if (air != ((e->flag & PTYA_AIR) != 0)) return false;
    return e->flag & PTYA_FINISHER ? finisher_ok(bits, e->combo_offset, air) : !combo_over(bits, air);
}

static int ptya_index(int id) {
    for (int i = 0; i < NPTYA; i++)
        if (PTYA[i].id == id) return i;
    return -1;
}

static const KhPtya *ptya_pick(const bool *marks) {  // the first marked entry; a repeat of the playing motion -> Sub
    const KhPtya *fallback = NULL;
    for (int i = 0; i < NPTYA; i++) {
        const KhPtya *e = &PTYA[i];
        if (!marks[i]) continue;
        if (e->sub == e->id || e->motion != S.motion) return e;
        if (!fallback && e->sub >= 0 && ptya_index(e->sub) >= 0) fallback = &PTYA[ptya_index(e->sub)];
    }
    return fallback;
}

// (s110 mkhit) Mickey's ground table is one A335 lunge (id 0 / 5) in front of A330 / A331: the first-eligible rule alone always
// picks A335, a dead end (no combo window in its first 60 frames). His ground combo = A330 -> A331 -> A335 by the hit count
// (the gate's S.d74), then round again. GUESS for KH2's own order; "set mkhit 0" = the plain table pick.
static const KhPtya *mk_ground_fix(const KhPtya *e) {
    int step = S.d74 > 0 ? (S.d74 - 1) % 3 : 0, want = step == 0 ? 181 : step == 1 ? 182 : 186;
    if (e->motion == want) return e;
    const KhPtya *best = NULL;
    for (int i = 0; i < NPTYA; i++) {
        const KhPtya *p = &PTYA[i];
        if (p->motion != want || (p->flag & PTYA_AIR) || !mot_of(want)) continue;
        if (!best || (p->type == e->type && best->type != e->type) || (best->jump <= 0 && p->jump > 0)) best = p;
    }
    if (best) LOG("[mkhit] ground combo step %d: ptya id %d (motion %d) -> id %d (motion %d)\n", step + 1, e->id, e->motion, best->id, best->motion);
    return best ? best : e;
}

// (s105 comboabil, live "set combolog 1") the plan a combo runs under: limits, the combo abilities, which gated attacks are live
static void combo_plan_log(bool air, const KhPtya *first) {
    char gl[160] = "";
    for (size_t i = 0; i < sizeof PTYA_AB / sizeof *PTYA_AB; i++) {
        const KhPtya *p = NULL;
        for (int k = 0; k < NPTYA && !p; k++)
            if (PTYA[k].ability == PTYA_AB[i].stat) p = &PTYA[k];
        if (p && ptya_ability_ok(p)) snprintf(gl + strlen(gl), sizeof gl - strlen(gl), "%d(A3%02d) ", PTYA_AB[i].item, p->motion >= 191 ? p->motion - 191 + 40 : p->motion - 161 + 10);
    }
    LOG("[combo] plan %s: ground %d air %d finishers %d | NegCombo %d FinPlus %d ComboBoost %d AirBoost %d ComboMaster %d | gated attacks live (ability id): %s%s | first pick ptya id %d motion %d\n",
        air ? "air" : "ground", F_COMBO_GROUND, F_COMBO_AIR, F_COMBO_FINISHERS, ab_on(AB.on, AB_NEGATIVE_COMBO),
        ab_on(AB.on, AB_FINISHING_PLUS), ab_on(AB.on, AB_COMBO_BOOST), ab_on(AB.on, AB_AIR_COMBO_BOOST),
        g_combo_master || ab_on(AB.on, AB_COMBO_MASTER), g_comboabil ? "" : "(comboabil off) ", gl, first->id, first->motion);
}

static const KhPtya *ptya_select(uint32_t bits, bool air) {
    bool marks[NPTYA_MAX];
    memset(marks, 0, sizeof marks);
    for (int i = 0; i < NPTYA; i++)
        if (PTYA[i].type == PTYA_ALWAYS && ptya_eligible(&PTYA[i], bits, air)) marks[i] = true;
    const KhPtya *e = ptya_pick(marks);
    if (!e) {
        int d = ptya_index(air ? PTYA_ENTRY_AIR : PTYA_ENTRY_GROUND);
        if (d >= 0) marks[d] = true, e = ptya_pick(marks);
    }
    return e;
}

// ------------------------------------------------------------------ KH2's attack targets (merged from the helper's
// runtime/sora_target_draft.c, session 17): the lock-on block's auto target (re:0x3BE4B0 / re:0x3DCD70), the ptya
// Type 1 / Type 2 range test (re:0x4029F0 / re:0x4027F0), the select with targets (re:0x4028D0 / re:0x402D20), the
// attack start's lunge and jump start (re:0x3D78D0) and the turn_to_lock turn (range 28, re:0x3D4B10 / re:0x3B9F70).
// Every rule and its source: D:/SoraBotW/recon/TARGET_SELECT_RECON.md (sections cited as TSR x.y). Units: KH cm and
// KH frames inside, BotW (x, z) horizontal axes; the one vertical number is KH2's, y DOWN: dy = -(height above feet).
// ------------------------------------------------------------------ KH2 numbers (TSR 1: plyr row 0, P_EX100.mdlx)
#define KH_AUTO_RANGE 300.0f     // plyr AutoLockonRange, cm (03system.bin@0x175D0 row 0)
#define KH_AUTO_MIN_H -435.0f    // plyr AutoLockonMinHeight (KH y down: 435 cm above the part)
#define KH_AUTO_MAX_H 50.0f      // plyr AutoLockonMaxHeight (50 cm below)
#define KH_AUTO_TIME 60.0f       // plyr AutoLockonTime, frames (the hold while a turn_to_lock window runs)
#define KH_AUTO_H_ADJ 0.4f       // plyr AutoLockonHeightAdjust
#define KH_AUTO_INNER 0.25f      // plyr AutoLockonInnerAdjust
#define KH_LOCK_RANGE 1200.0f    // sstm LockOnRange, cm, surface distance from Sora's point (03system.bin@0x183E8)
#define KH_LOCK_RELEASE 1500.0f  // sstm LockOnReleaseRange (03system.bin@0x183EC): a LOCK farther is released
#define KH_SORA_PART_H 70.0f     // Sora's own target part: a sphere 70 cm above the feet, radius 55 (re:0x3D7520)
#define KH_SORA_PART_R 55.0f
#define KH_TURN_MIN_DIST 50.0f   // the target turn does nothing within this horizontal distance (re:0x5B532C)
#define KH_TURN_MULT 2.0f        // turn_to_lock rate = param deg * pi/180 * 2 per frame (re:0x5B18EC)
#define PTYA_LOW_MINUS (1u << 3)   // ptya Flag bits re:0x4027F0 (sora_params.h has AIR, JUMP_HIGH, FINISHER)
#define PTYA_HIGH_PLUS (1u << 4)

// A target as the selector reads it (re:0x3C9C00 part info): the part's centre relative to Sora's feet, its radius
// and height term (half-height). BotW enemies have one part: linkpos's feet + half the hit cylinder (TSR 4.1).
typedef struct {
    uint64_t actor;
    float d[2];     // centre - Sora's feet, horizontal, cm, BotW (x, z)
    float dy;       // centre - Sora's feet, vertical, cm, KH y DOWN (negative = above)
    float hd;       // |d| horizontal, cm
    float radius, height;  // part radius, half-height, cm
    int32_t life;
    bool big;              // (s88) a big-body cylinder target (radius = the body's)
    bool air;              // held in the air by sora_airhold.h (Thunder pop, air hop, blow), not a slide
} Target;

// (s66 user: after a Thunder pop the jump-up A331 needed him "really close") KH2's A331 Far is 260 cm of surface and its
// SpeedMax 8 cm/frame reaches ~250 cm; an enemy held in the air gets g_air_reach more on the auto target, the Type 1 / 2
// Far of the ground -> air entries (Jump > 0) and their SpeedMax scaled to match. Live "set airreach <cm>" (0 = KH2's).
static float g_air_reach = 100;
static int g_big_air = 1;  // (s88) live "set bigair 0" = the old 50 cm point at the origin

// (s65 audit #2) the aim is KH2's 300 again: at 600 Sora turned toward 3-6 m targets and whiffed (live1: every miss was
// one of those). The yellow icon keeps the s47 user's 600 by its own display-only search (g_icon_range, 0 = the aim's).
static float g_auto_range = KH_AUTO_RANGE;  // live "set autor <cm>"
static float g_icon_range = 600;            // live "set iconr <cm>" (s47 user: the icon showed only up close at 300)
static float g_part_r = 50.0f, g_part_h = 80.0f;  // enemy part radius / half-height, cm (from g_hit_r / g_hit_h)
static bool g_targets_on = true;

static void targets_init(float to_u) {  // after hits_load(): the enemy cylinder sora.c already uses, in cm
    g_targets_on = env_f("SAVAGEWIND_SORA_TARGETS", 1) != 0;
    g_part_r = g_hit_r / to_u, g_part_h = g_hit_h * 0.5f / to_u;
    LOG("[sora] targets %s (enemy part r %.0f h %.0f cm)\n", g_targets_on ? "on" : "off", g_part_r, g_part_h);
}

// ------------------------------------------------------------------ the enemy list as KH2 targets
static int targets_build(const float p[3], float to_u, Target *out, int max) {  // p = Sora's feet (u)
    uint64_t actor[64];
    float pos[64][3];
    int32_t life[64];
    int n = linkpos_enemies(actor, pos, life, 64), m = 0;
    for (int i = 0; i < n && m < max; i++) {
        if (life[i] <= 0) continue;  // (targetable, re:0x3BA260: alive; KH2's actor+0x6C8 bit 17 has no BotW twin yet)
        Target *t = &out[m++];
        t->actor = actor[i], t->life = life[i];
        t->d[0] = (pos[i][0] - p[0]) / to_u, t->d[1] = (pos[i][2] - p[2]) / to_u;
        t->dy = -((pos[i][1] + g_hit_h * 0.5f) - p[1]) / to_u;  // centre above Sora's feet -> KH y down
        t->hd = sqrtf(t->d[0] * t->d[0] + t->d[1] * t->d[1]);
        t->radius = g_part_r, t->height = g_part_h, t->big = false;
        if (g_big_air) {  // (s88 Guardian air combos) a big body is a cylinder, not a 50 cm point at its origin: the aim, the
            // Far / dist test and the jump speed work from its SURFACE, at Sora's own height (clamped to the body), so
            // the jump stops at the shell and every air hit connects without repositioning (KH2: large actors' part
            // radius is the body's; the attack_start body line = centre delta, radii = target r + 55)
            float br, bh;
            enemy_body(actor[i], &br, &bh);
            float sy = p[1] + KH_SORA_PART_H * to_u, y0 = pos[i][1], y1 = pos[i][1] + bh;
            float ay = sy < y0 ? y0 : sy > y1 ? y1 : sy;
            // (s88 review) medium bodies (Moblin 0.7, Lynel / Guardian Scout 0.9 u) keep the old point target while Sora
            // is level with their middle (ground combos exactly as before); only a real height gap (air) uses the shell
            bool med = br < 1.0f, gap = fabsf(ay - (pos[i][1] + g_hit_h * 0.5f)) > 0.3f;
            if (br > g_hit_r && (!med || gap)) {
                t->radius = br / to_u, t->height = g_part_h;
                t->dy = -(ay - p[1]) / to_u;
                t->big = true;
                static ULONGLONG said;
                ULONGLONG nw = GetTickCount64();
                if (nw - said > 1500 && t->hd < br / to_u + 600)
                    said = nw, LOG("[sora] bigair %llx: %.0f cm from the shell, aim y %.0f cm above Sora's feet, hold %s\n",
                                   (unsigned long long)actor[i], t->hd - t->radius, -t->dy, ah_find(actor[i]) ? "yes" : "no");
            }
        }
        const AhEnt *ah = ah_find(actor[i]);
        t->air = ah && ah->kind != AH_SLIDE;
    }
    return m;
}

static const Target *target_of(const Target *t, int n, uint64_t actor) {
    for (int i = 0; i < n; i++)
        if (t[i].actor == actor) return &t[i];
    return NULL;
}

// ------------------------------------------------------------------ the lock-on block (actor+0xDC8), AUTO state only
// re:0x3BE4B0: the auto-target search. Parts within 300 cm of the surface horizontally and with the centre from
// 435 + height above to 50 + height below the feet; score = (hd + |dy| x 0.4) x ((1 - cos) x 0.5 x 0.75 + 0.25),
// lowest wins (straight ahead x0.25, behind x1). TSR 3.1.
static const Target *target_auto(const Target *t, int n, float face, float range) {
    float fx = sinf(face), fz = cosf(face), best_s = 0;
    const Target *best = NULL;
    for (int i = 0; i < n; i++) {
        const Target *e = &t[i];
        if (e->dy < KH_AUTO_MIN_H - e->height || e->dy > KH_AUTO_MAX_H + e->height ||
            e->hd - e->radius > range + (e->air ? g_air_reach : 0))
            continue;
        float cosang = e->hd > 0 ? (e->d[0] * fx + e->d[1] * fz) / e->hd : 1;
        float s = (e->hd + fabsf(e->dy) * KH_AUTO_H_ADJ) * ((1 - cosang) * 0.5f * (1 - KH_AUTO_INNER) + KH_AUTO_INNER);
        if (!best || s < best_s) best = e, best_s = s;
    }
    return best;
}

// |part centre - Sora's point| - radius (re:0x3BDD90): his point is his own part's centre, 70 cm above the feet
static float target_surface(const Target *e) {
    float dy = e->dy + KH_SORA_PART_H;  // (KH y down: the point is at -70)
    return sqrtf(e->hd * e->hd + dy * dy) - e->radius;
}

// re:0x3BEA30: R1 with no target, the part nearest to Sora's point by surface distance within LockOnRange
static const Target *target_lock(const Target *t, int n) {
    const Target *best = NULL;
    float best_d = 0;
    for (int i = 0; i < n; i++) {
        float d = target_surface(&t[i]);
        if (d > KH_LOCK_RANGE) continue;  // (KH2's large actors have no limit: BotW has no flag for them yet)
        if (!best || d < best_d) best = &t[i], best_d = d;
    }
    return best;
}

static struct {
    uint64_t actor;  // the block's target (0: none): the auto target, or the R1 lock with `locked`
    float timer;     // AutoLockonTime hold (re:0x3DCCD0 fills it while a turn_to_lock window runs)
    bool locked;
} g_lock;

static void lock_release(const char *why) {
    if (g_lock.locked) sora_sfx_event("lock_off", 1), LOG("[sora] lock-on released (%s)\n", why);
    g_lock.actor = 0, g_lock.timer = 0, g_lock.locked = false;
    g_eat_rx = 0, g_eat_zr = 0, g_cam_rx = 0;  // (the lock-on camera lets go of the right stick and ZR)
}

// re:0x3DD280 + re:0x3DCD70 each frame (TSR 3.2): a gone / dead target is dropped, a LOCK past LockOnReleaseRange
// released; R1 (BotW's ZL) locks the AUTO target, else with none the nearest part (search_lock), and a second R1
// releases (SE 8). In AUTO the target is kept while the timer runs, else the search runs again and a better part
// replaces it; entering AUTO leaves the timer at 0, so outside swings the auto target is the best part every frame.
// (KH2's right-stick switch while locked = L5, not here.)
static void lock_tick(const Target *t, int n, float kh, uint32_t bits_prev, float face, bool r1) {
    const Target *cur = g_lock.actor ? target_of(t, n, g_lock.actor) : NULL;
    if (!cur && g_lock.actor) lock_release("target gone");
    if (cur && g_lock.locked && target_surface(cur) > KH_LOCK_RELEASE) lock_release("out of range"), cur = NULL;
    if (r1 && g_lock.locked) { lock_release("R1"); return; }
    if (r1) {  // NONE -> search_lock, AUTO -> its target (re:0x3DCD70: no new search)
        const Target *p = cur ? cur : target_lock(t, n);
        if (p) {
            g_lock.actor = p->actor, g_lock.locked = true, g_lock.timer = 0;
            sora_sfx_event("lock_on", 1);
            LOG("[sora] lock-on %llx (%.0f cm surface, %s)\n", (unsigned long long)p->actor, target_surface(p), cur ? "auto target" : "search");
        } else LOG("[sora] lock-on: nothing within %.0f cm\n", KH_LOCK_RANGE);
        return;
    }
    if (g_lock.locked) return;
    if (S.special && (bits_prev & RB_TURN_TO_LOCK) && cur) g_lock.timer = KH_AUTO_TIME;  // re:0x3DCCD0
    if (cur && g_lock.timer > 0) { g_lock.timer -= kh; return; }
    const Target *p = target_auto(t, n, face, g_auto_range);
    if (p) {
        if (p->actor != g_lock.actor) LOG("[sora] auto target %llx (%.0f cm, dy %.0f)\n", (unsigned long long)p->actor, p->hd, p->dy);
        g_lock.actor = p->actor;  // (re:0x3DCCF0: a different part replaces it, same state)
    } else
        g_lock.actor = 0, g_lock.timer = 0;
}

// ------------------------------------------------------------------ the lock-on camera + target switch (L4 / L5)
// re:0x3BD460 (lockon_kh2.py search_switch): the next part around Sora from the current target by the smallest
// positive angle times the sign, within LockOnRange. BotW's atan2(x, z) falls to the camera's right (its right is
// (-fz, fx)), so a right flick is sign -1 here.
static const Target *target_switch(const Target *t, int n, const Target *cur, float sign) {
    float base = atan2f(cur->d[0], cur->d[1]), best_a = 0;
    const Target *best = NULL;
    for (int i = 0; i < n; i++) {
        if (t[i].actor == cur->actor || target_surface(&t[i]) > KH_LOCK_RANGE) continue;
        float a = wrap(atan2f(t[i].d[0], t[i].d[1]) - base) * sign;
        if (a > 0 && (!best || a < best_a)) best = &t[i], best_a = a;
    }
    return best;
}

// A world point (BotW units) -> KH2's screen-rect flags (re:0x176710, PS2 rect x 32..480 / 512, y 8..408 / 416):
// 1 above, 2 below, 4 left, 8 right, 0x10 behind the camera. -1 without a camera.
static int cam_classify(const float w[3]) {
    float v[12], pr[4] = {0.1f, 10000, 50 * PI / 180, 16.0f / 9};
    if (!linkpos_camera_view(v)) return -1;
    linkpos_camera_proj(pr);
    float ex = v[0] * w[0] + v[1] * w[1] + v[2] * w[2] + v[3], ey = v[4] * w[0] + v[5] * w[1] + v[6] * w[2] + v[7],
          ez = v[8] * w[0] + v[9] * w[1] + v[10] * w[2] + v[11];
    if (-ez < 0.05f) return 0x10;
    float t = 1.0f / tanf(pr[2] * 0.5f), nx = t / pr[3] * ex / -ez, ny = -t * ey / -ez;  // (ndc, y down: build_mvp)
    int out = 0;
    if (nx < 32.0f / 256 - 1) out |= 4;
    if (nx > 480.0f / 256 - 1) out |= 8;
    if (ny < 8.0f / 208 - 1) out |= 1;
    if (ny > 408.0f / 208 - 1) out |= 2;
    return out;
}

// KH2's lock-on camera (re:0x176C70 -> re:0x176EC0, camera_kh2.py _lock_at): while the target or Sora's chest is
// off the screen rect, the wanted yaw is the look-at -> Sora direction (the camera behind him, facing the target)
// turned 20 deg past it (re:0x5B22EC) on the side the target left; the yaw then turns toward it (at most 10 deg a
// frame, re:0x718BE4). BotW owns the camera, so pad_tap adds a right stick x from a P-control on the yaw error (+rx
// turns BotW's atan2(fx, fz) down, ~0.9 deg a present at half stick: s43 rxsign). KH2's right stick still turns the
// camera while locked (that drops the wanted yaw) and R2 + a right-stick flick switches (re:0x3DCD70 bits 36 / 37);
// live `set flick 1` (default, the user's s41 wish) lets a plain flick switch too, and the real rx is then eaten.
static int g_flick = 1, g_lockcam = 1;  // live "set flick 0/1", "set lockcam 0/1"
static float g_cam_gain = 2.5f;          // live "set camgain <x10>": full stick at 1/gain rad of yaw error
static float g_cam_lead = 4;             // live "set camlead <presents>" (s44: 6 lagged, 4 settles faster, no overshoot): the yaw error is taken that far ahead
static struct { bool on, prev_ok; float want, yaw_prev; int flick; } g_lc;
static int g_follow = 1;          // live "set follow 0/1" (s47 user: KH2's lock camera follows all the time)
static float g_follow_gain = 1.0f;  // live "set fgain <x10>": full stick at 1/gain rad while following
static float g_follow_dead = 3;     // deg

static void lockcam_tick(const Target *t, int n, const float p[3], float to_u, uint64_t held) {
    const Target *lk = g_targets_on && g_lock.locked ? target_of(t, n, g_lock.actor) : NULL;
    float rx = kh_axis(g_rx_raw), ry = kh_axis(g_ry_raw);
    int flick = fabsf(rx) > 0.6f && fabsf(rx) > fabsf(ry) ? (rx > 0 ? 1 : -1) : fabsf(rx) < 0.3f ? 0 : g_lc.flick;
    bool flicked = flick && flick != g_lc.flick;
    g_lc.flick = flick;
    // (R2 held: the stick is the switch, not a camera turn - else each flick frame dropped the swing and re-logged it)
    g_eat_rx = lk && (g_flick || (held & BTN_R2)), g_eat_zr = lk && !g_flick;
    if (!lk) { g_lc.on = g_lc.prev_ok = false, g_cam_rx = 0; return; }
    if (flicked && (g_flick || (held & BTN_R2))) {
        const Target *nx = target_switch(t, n, lk, flick > 0 ? -1.0f : 1.0f);
        if (nx) {
            LOG("[sora] lock-on switch %s -> %llx (%.0f cm surface)\n", flick > 0 ? "right" : "left",
                (unsigned long long)nx->actor, target_surface(nx));
            g_lock.actor = nx->actor, lk = nx;
        } else {
            float nd = 0;
            for (int i = 0; i < n; i++)
                if (t[i].actor != lk->actor && (!nd || target_surface(&t[i]) < nd)) nd = target_surface(&t[i]);
            LOG("[sora] lock-on switch %s: no other target (%d in the list, next nearest %.0f cm)\n",
                flick > 0 ? "right" : "left", n, nd);
        }
    } else if (!g_eat_rx && fabsf(rx) > 0.2f) g_lc.on = false;  // (his own camera turn: KH2 sets the wanted yaw to it)
    float f[2];
    if (!g_lockcam || !linkpos_camera_fwd(f)) { g_lc.on = g_lc.prev_ok = false, g_cam_rx = 0; return; }
    float tw[3] = {p[0] + lk->d[0] * to_u, p[1] - lk->dy * to_u, p[2] + lk->d[1] * to_u};
    float chest[3] = {p[0], p[1] + 100 * to_u, p[2]};
    int u1 = cam_classify(tw), u2 = cam_classify(chest);
    if (u1 > 0 || u2 > 0) {
        int u = u1 > 0 ? u1 : u2;
        float off = u & 4 ? 0.3490658f : u & 8 ? -0.3490658f : 0;  // (left: 20 deg past it; BotW's left = a larger atan2)
        if (!g_lc.on) LOG("[sora] lock camera swing (target 0x%x, chest 0x%x): yaw %.0f -> %.0f deg\n", u1, u2,
                          atan2f(f[0], f[1]) * 180 / PI, wrap(atan2f(lk->d[0], lk->d[1]) + off) * 180 / PI);
        g_lc.want = wrap(atan2f(lk->d[0], lk->d[1]) + off), g_lc.on = true;
    }
    // (BotW's camera keeps turning ~6 presents after the stick lets go - s43 swing overshot 20 deg - so the error is
    // taken from the yaw that far ahead at this present's turn rate)
    float yaw = atan2f(f[0], f[1]), rate = g_lc.prev_ok ? wrap(yaw - g_lc.yaw_prev) : 0;
    g_lc.yaw_prev = yaw, g_lc.prev_ok = true;
    float err = g_lc.on ? wrap(g_lc.want - yaw - rate * g_cam_lead) : 0;
    if (g_lc.on && fabsf(err) < 2 * PI / 180 && fabsf(rate) < 1 * PI / 180)
        g_lc.on = false, err = 0, LOG("[sora] lock camera swing done (yaw %.0f deg)\n", yaw * 180 / PI);
    // the follow (s47): on screen, the camera keeps turning gently toward the target behind Sora (no log)
    if (!g_lc.on && g_follow && (g_eat_rx || fabsf(rx) <= 0.2f)) {
        float fe = wrap(atan2f(lk->d[0], lk->d[1]) - yaw - rate * g_cam_lead);
        if (fabsf(fe) > g_follow_dead * PI / 180) err = fe * g_follow_gain / g_cam_gain;
    }
    float a = -err * g_cam_gain;
    g_cam_rx = (int32_t)lroundf((a > 1 ? 1 : a < -1 ? -1 : a) * 32767);
}

// ------------------------------------------------------------------ Type 1 / 2 marks (re:0x4029F0 + re:0x4027F0)
// TSR 4.2: Near <= dist < Far, High' < dy <= Low', InnerMin < cos <= InnerMax, the Low / High window widened by the
// target's half-height (narrowed with the LOW_MINUS / HIGH_PLUS flags), High raised by the jump height with JUMP_HIGH.
static bool ptya_in_range(const KhPtya *e, float dist, float dy, float cosang, float height, float far_add) {
    float hi = e->high, lo = e->low;
    if (e->far_ > 0) dist -= far_add;  // (s66 air reach: Far + far_add; Near is 0 on those entries)
    if (dist < 0) dist = 0;
    if (e->flag & PTYA_JUMP_HIGH) hi -= F_JUMP_H;  // (re:0x3B5A40: High Jump would raise it)
    lo = (e->flag & PTYA_LOW_MINUS) ? lo - height : lo + height;
    hi = (e->flag & PTYA_HIGH_PLUS) ? hi + height : hi - height;
    return !(dist < e->near_ || e->far_ <= dist || lo < dy || dy <= hi || e->inner_max < cosang || cosang <= e->inner_min);
}

// one target against the entries of one Type: dist = horizontal surface distance (>= 0), cos = angle off `dir`
// (the facing, or the line to the block's target when there is one: re:0x4028D0, TSR 4.1), capped at 1
static void ptya_mark_target(bool *marks, const Target *t, const float dir[2], int type, uint32_t bits, bool air) {
    float dist = t->hd - t->radius, cosang = t->hd > 0 ? (t->d[0] * dir[0] + t->d[1] * dir[1]) / t->hd : 0;
    if (dist < 0) dist = 0;
    if (cosang > 1) cosang = 1;
    for (int i = 0; i < NPTYA; i++) {
        const KhPtya *e = &PTYA[i];
        float far_add = t->air && !air && e->jump > 0 && e->near_ <= 0 ? g_air_reach : 0;
        if (e->type == type && ptya_eligible(e, bits, air) && ptya_in_range(e, dist, t->dy, cosang, t->height, far_add))
            marks[i] = true;
    }
}

// re:0x4028D0 + re:0x402D20 with targets (replaces sora.c's ptya_select): Type 2 per targetable enemy, Type 1 the
// block's target, Type 3 always; no wall probe (Id 1) yet. The pick and the Id 5 / 9 default are sora.c's.
static const KhPtya *ptya_select_t(uint32_t bits, bool air, const Target *t, int n, bool wall) {
    bool marks[NPTYA_MAX];
    memset(marks, 0, sizeof marks);
    const Target *lock = g_targets_on && g_lock.actor ? target_of(t, n, g_lock.actor) : NULL;
    float dir[2] = {sinf(S.face), cosf(S.face)};
    if (lock && lock->hd > 0) dir[0] = lock->d[0] / lock->hd, dir[1] = lock->d[1] / lock->hd;
    if (g_targets_on)
        for (int i = 0; i < n; i++) ptya_mark_target(marks, &t[i], dir, PTYA_ENEMY, bits, air);
    if (lock) ptya_mark_target(marks, lock, dir, PTYA_LOCK, bits, air);
    for (int i = 0; i < NPTYA; i++)
        if (PTYA[i].type == PTYA_ALWAYS && ptya_eligible(&PTYA[i], bits, air)) marks[i] = true;
    // re:0x402D20 (PHH combo_kh2.py:450-453): grounded, no ACTION bit 1 / keep_kinetics, not on a platform, the forward
    // wall probe hit -> the wall entry Id 1 is marked, unconditionally (A330 with a 21 cm jump start: up the wall)
    if (wall && !air && !(bits & RB_KEEP_KINETICS)) {
        int w = ptya_index(PTYA_ENTRY_WALL);
        if (w >= 0) marks[w] = true;
    }
    const KhPtya *e = ptya_pick(marks);
    if (!e) {
        int d = ptya_index(air ? PTYA_ENTRY_AIR : PTYA_ENTRY_GROUND);
        if (d >= 0) marks[d] = true, e = ptya_pick(marks);
    }
    return e;
}

// ------------------------------------------------------------------ the attack start (re:0x3D78D0), after play()
// TSR 6. The body line: target part centre - Sora's part centre (70 cm up), radii = target r + 55.
static float ptya_clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static void attack_start_t(const KhPtya *e, const Target *t, int n, float z_up, bool air, uint32_t bits) {
    const Target *lock = g_targets_on && g_lock.actor ? target_of(t, n, g_lock.actor) : NULL;
    float bd[3] = {0, 0, 0}, radii = 0;  // body delta (x, y down, z) and the radii sum, with a target
    if (lock) {
        bd[0] = lock->d[0], bd[1] = lock->dy + KH_SORA_PART_H, bd[2] = lock->d[1];  // (his part is 70 up: y down -> +70)
        radii = lock->radius + KH_SORA_PART_R;
    }
    if (e->jump <= 0) {  // ground entry: the lunge (TSR 6.1). Base Sora: ACTION bit 1 is never set and no motion
        // has keep_kinetics, and every ground entry's speeds are 0 -> this stays 0. Kept for the Drive Forms.
        bool kinetics = (bits & RB_KEEP_KINETICS) != 0;  // (ACTION flags bit 1: none of Sora's ACTIONs)
        if (!kinetics || !lock) return;
        float n3 = sqrtf(bd[0] * bd[0] + bd[1] * bd[1] + bd[2] * bd[2]);
        float s = ptya_clamp(((n3 - radii) + e->dist_adjust) * KH_ATTACK_DIST_TO_SPEED, e->speed_min, e->speed_max);
        if (n3 > 0) S.vel[0] = bd[0] / n3 * s, S.vel[1] = bd[2] / n3 * s;  // (cm/frame; the y part dropped as PHH does)
        LOG("[sora] lunge %.1f cm/frame at ptya id %d\n", s, e->id);
        return;
    }
    // air entry (Jump > 0): a jump start toward the target (TSR 6.2); from the ground it restarts the combo count
    if (!air) S.d74 = S.d75 = 1;
    float h, dir[2], speed;
    if (!lock) {
        h = e->jump, dir[0] = sinf(S.face), dir[1] = cosf(S.face), speed = e->speed_max;
    } else {
        float jmax = e->jump_max + ((e->flag & PTYA_JUMP_HIGH) ? F_JUMP_H : 0);
        h = ptya_clamp(-bd[1], e->jump_min, jmax);  // the target centre's height above Sora's part centre
        float hn = sqrtf(bd[0] * bd[0] + bd[2] * bd[2]);
        dir[0] = hn > 0 ? bd[0] / hn : sinf(S.face), dir[1] = hn > 0 ? bd[2] / hn : cosf(S.face);
        float apex = sqrtf(fabsf(2 * h / KH_G));  // re:0x3B73F0
        float smax = e->speed_max;
        if (IN_MICKEY && g_mkhit) radii = lock->radius + g_mk_stop, smax *= 1.5f;  // (s110 mkhit: his blade reaches ~70 cm, not Sora's 130: close in)  // (s66 air reach: the cap grows with the Far, so the longer jump still arrives)
        if (lock->air && !air && e->far_ > 0 && e->near_ <= 0) smax *= (e->far_ + g_air_reach) / e->far_;
        speed = ptya_clamp(apex > 0 ? ((hn - radii) + e->dist_adjust) / apex : smax, e->speed_min, smax);
    }
    arc(h);  // the ACTION's jump start (vtable +0x18 = re:0x3CBB00): the arc from here, no take-off wind-up
    S.hvel[0] = dir[0] * speed, S.hvel[1] = dir[1] * speed;
    S.takeoff = 0;
    enter_jump(z_up, true);
    S.jump_held = S.jump_released = false;
    LOG("[sora] air attack jump h %.0f speed %.1f (ptya id %d)\n", h, speed, e->id);
}

// ------------------------------------------------------------------ turn_to_lock (range 28; TSR 7)
// The degrees come from sora_params.h's kh_turn_lock (sorabake: range 28's param, by mset slot).
#define KH_TURN_LOCK_DFLT_DEG 15

// re:0x3D4B10 -> re:0x3B9F70, every KH frame of the window: the horizontal line from Sora's feet to the block target's
// point; nothing within 50 cm; else the facing steps toward it by at most deg * pi/180 * 2 per frame. Steps `kh`
// frames at once (sora.c's frame = kh KH frames), as PHH does (kh2ctl.py:959).
// (s65 audit #4, TSR 7.4) true when it owned the facing this frame: the caller sets wish = facing then.
static int g_swing_turn = 1;  // live "set swingturn 0": swings ignore the wish yaw (the s64 rule)
static int g_late_hit = 1;    // live "set latehit 0": a press before this present's hit stays dropped (the s64 rule)
static bool turn_to_lock(uint32_t bits, float kh, const Target *t, int n) {
    if (!g_targets_on || !S.special || !(bits & RB_TURN_TO_LOCK) || !g_lock.actor) return false;
    const Target *lock = target_of(t, n, g_lock.actor);
    if (!lock || lock->hd <= KH_TURN_MIN_DIST) return false;
    int deg = KH_TURN_LOCK_DFLT_DEG;
    const KhTurnLock *tl = IN_MICKEY ? mickey_turn_lock : IN_ROXAS ? roxas_turn_lock : IN_ANTI ? anti_turn_lock : IN_FINAL ? final_turn_lock : IN_LIMIT ? limit_turn_lock : IN_MASTER ? master_turn_lock : IN_WISDOM ? wisdom_turn_lock : g_fmots_on ? valor_turn_lock : kh_turn_lock;  // (s69: Valor's own windows; s89 Limit, Master)
    size_t ntl = IN_MICKEY ? sizeof mickey_turn_lock / sizeof *mickey_turn_lock : IN_ROXAS ? sizeof roxas_turn_lock / sizeof *roxas_turn_lock : IN_ANTI ? sizeof anti_turn_lock / sizeof *anti_turn_lock : IN_FINAL ? sizeof final_turn_lock / sizeof *final_turn_lock : IN_LIMIT ? sizeof limit_turn_lock / sizeof *limit_turn_lock : IN_MASTER ? sizeof master_turn_lock / sizeof *master_turn_lock
               : IN_WISDOM ? sizeof wisdom_turn_lock / sizeof *wisdom_turn_lock
               : g_fmots_on ? sizeof valor_turn_lock / sizeof *valor_turn_lock : sizeof kh_turn_lock / sizeof *kh_turn_lock;
    for (size_t i = 0; i < ntl; i++)
        if (S.mot && tl[i].slot == S.mot->slot && S.clock >= tl[i].start && S.clock <= tl[i].end + 1)
            deg = tl[i].deg;
    float rate = (float)deg * PI / 180 * KH_TURN_MULT * kh, d = wrap(atan2f(lock->d[0], lock->d[1]) - S.face);
    S.face = wrap(S.face + (d > rate ? rate : d < -rate ? -rate : d));
    return true;
}

// Frame trigger 21 turn_to_lockon (re:0x40DD70 -> re:0x3D4AA0; A332 @frame 0, param 360): the same turn once, by
// deg x 2 at most (360 -> a full snap); param 0 would only set the wish yaw (re:0x3B65C0), which sora.c has no twin
// of yet. The bake keeps frame triggers with their params ({u16 frame, u8 id, u8 n, i16 p1, i16 p2}, sora.c 129).
// Drafted for the frame-0 case only (called right after play()); sora.c fires no other frame triggers yet.
static void turn_to_lock_once(const Target *t, int n) {
    const Mot *m = S.mot;
    if (!g_targets_on || !m || !g_lock.actor) return;
    for (int i = 0; i < m->nft; i++) {
        uint16_t fr;
        int16_t p1;
        memcpy(&fr, m->ft + i * 8, 2), memcpy(&p1, m->ft + i * 8 + 4, 2);
        if (m->ft[i * 8 + 2] != 21 || fr != 0 || p1 <= 0) continue;
        const Target *lock = target_of(t, n, g_lock.actor);
        if (!lock || lock->hd <= KH_TURN_MIN_DIST) return;
        float rate = (float)p1 * PI / 180 * KH_TURN_MULT, d = wrap(atan2f(lock->d[0], lock->d[1]) - S.face);
        S.face = wrap(S.face + (d > rate ? rate : d < -rate ? -rate : d));
        return;
    }
}

static bool combo_gate(uint32_t bits, bool free) {  // re:0x3D6CA0: continue, restart from a free frame, or drop
    bool cont = S.hit || g_combo_master || ab_on(AB.on, AB_COMBO_MASTER);  // (s92: the Abilities page row counts too)
    if ((bits & RB_ALLOW_FINISHER) && cont && S.d75 < F_COMBO_FINISHERS) S.d74++, S.d75++;
    else if ((bits & RB_ALLOW_COMBO) && cont) S.d74++;
    else if (free) S.d74 = S.d75 = 1;
    else return false;
    S.hit = false;
    return true;
}

// ------------------------------------------------------------------ the command menu + Magic (S32 helper L 4.2 / 4.5)
// KH2's command menu (recon/S32_HELPER_L_CMDMENU.md 1): top page Attack / Magic / Items / Drive (d-pad up / down,
// right or A opens), the Magic submenu (A casts, B / left closes), the shortcut menu while L is held (L + X / Y / B /
// A = slots 0-3). Items / Drive are greyed until they exist. Log-only until sora_hud.c draws it (helper M).
// Video corrections (recon/KH2_VIDEO_NOTES.md): Thunder LAUNCHES (f54 0x14), and a cast is part of the combo - the
// press goes through combo_gate like an attack, and at the finisher step the spell's FinishMotion plays.
enum { SPELL_FIRE, SPELL_BLIZZARD, SPELL_THUNDER, SPELL_CURE, SPELL_MAGNET, SPELL_REFLECT, NSPELL };  // (s133 magic: Magnet + Reflect, runtime/sora_magic2.h)
_Static_assert(SPELL_MAGNET == 4 && SPELL_REFLECT == 5, "sora_unlock.h UL_SPELL_RULES keys the spells by 4 / 5");
typedef struct { const char *name; int cmd, cost, motion, air_motion, finish; uint8_t f40, f54; } KhSpell;
static const KhSpell KH_SPELLS[NSPELL] = {
    {"fire", 49, 12, 56, 58, 57, 1, 0x0f},      // file:kh2_first/03system.bin@0x9C4, 00battle magc @0x339A8
    {"blizzard", 51, 15, 59, 61, 60, 2, 0x11},  // @0xA24, @0x33AF8
    {"thunder", 50, 18, 62, 64, 63, 3, 0x14},   // @0x9F4, @0x33C48 (f54 0x14 = the s19 launch: the video, 7:25)
    {"cure", 52, 255, 65, 67, -1, 0, 0},        // @0xA54, @0x33D98 (Cost 255 = all MP; no finisher)
    // (s133 magic) PHH cache mined/sora_spells.kh.json Id 4 / 5: cmd 174 Cost 30 @03system.bin@0x1F24, magc @0x33EE8 (ground 68, air 70,
    // no finisher = 65535); cmd 177 Cost 10 @0x1FB4, magc @0x34038 (ground 71, finisher 72, air 73). f40 0 / f54 0x0f: atkp Element 5
    // (neither fire / ice / electric), a plain hit (the pull / the burst rows set their own reaction in sora_magic2.h)
    {"magnet", 174, 30, 68, 70, -1, 0, 0x0f},
    {"reflect", 177, 10, 71, 73, 72, 0, 0x0f},
};
static struct {
    int cursor;        // top page entry (0 Attack .. 3 Drive)
    int sub;           // -1 none, 1 = the Magic submenu
    int sub_cursor;    // entry in the Magic list (KH2 order = KH_SPELLS order)
    bool shortcut;     // L held: the shortcut menu is shown
    float dead;        // KH frames the menu is dead (disable_command_time 8)
    // (s84) the shortcuts = SC.sc (sora_levels.h, saved per slot): 0 X (triangle), 1 Y (square), 2 B (cross), 3 A (circle)
} M = {0, -1, 0, false, 0};
static struct { float mp, mp_max, charge, charge_total; } MP = {100, 100, 0, 0};  // KH2 base MP 100; charge > 0 = MP Charge

static bool mp_charging(void) { return MP.charge_total > 0; }
static void mp_change(int delta) {  // re:0x3D3CF0: below 0 -> MP Charge (exactly 0 left: no charge)
    if (mp_charging()) {
        if (delta > 0) MP.charge -= (float)delta * 12.0f;  // (MpToMpDrive 12)
        if (MP.charge <= 0) MP.charge_total = 0, MP.mp = MP.mp_max;
        return;
    }
    float v = MP.mp + (float)delta;
    if (v < 0) MP.mp = 0, MP.charge = MP.charge_total = g_mp_charge_time, LOG("[sora] MP Charge (%.0f KH frames)\n", MP.charge);
    else MP.mp = v > MP.mp_max ? MP.mp_max : v;
}
static void mp_tick(float kh) {  // re:0x3C0DE0: at 0 the charge ends with full MP
    if (mp_charging() && (MP.charge -= kh) <= 0) MP.charge_total = 0, MP.mp = MP.mp_max, LOG("[sora] MP Charge done: MP %.0f\n", MP.mp);
}
// (s68 drive) runtime/sora_form.h, included after the party
static bool fm_in_form(void);
static int fm_state(int form);
static int fm_label(int form);
static int fm_cost(int form);
static bool fm_no_magic(void);
static int fm_top_state(void);
static void fm_hud_menu(void);
static bool fm_drive(int form, bool free_cost);
static void fm_revert(const char *why);
static void fm_tick(float kh);
static int fm_rows(int rf[FM_NFORMS]);  // (s89 kb: the Drive page rows = the unlocked forms)
static void fm_other(bool hard);
static bool fm_live_set(const char *key, int v);
#define CMD_FORM 100    // menu_tick: the Drive page's form row (s72: CMD_FORM + FORM_VALOR / FORM_WISDOM)
#define CMD_REVERT 110  // menu_tick: the top page's Revert (in a form)
// (s89 b4-limit) runtime/sora_limit.h (after sora_form.h): Limit Form's Limit page (M.sub 3, the Magic slot) + commands
#define LM_N 4
#define CMD_LIMIT 120   // menu_tick: the Limit page's row (CMD_LIMIT + 0..3)
static int lm_menu_tick(uint64_t *press);
static int lm_top_state(void);
static int lm_hud_rows(uint8_t *label, uint8_t *state, int16_t *cost);
static void lm_request(int i);
static void lm_abort(const char *why);
static int lm_top_label(void);
static bool lm_live_set(const char *key, int v);
// (s133 limits) runtime/sora_plimit.h (after sora_react.h): the party Limits (Comet, Whirli-Goof) + their player
#define CMD_PLIMIT 140   // menu_tick: the party Limit list's row (CMD_PLIMIT + 0..3); (s133 review: was 130 = CMD_SLATE)
#define PL_SUB 7         // M.sub of the Limit page
#define PL_NLIM 4
static int pl_menu_tick(uint64_t *press);
static bool pl_open_left(void);
static bool pl_hold(void);
static bool pl_hold_party(void);  // (s149 review) the s133 pl_hold(): a held Limit runs, ANY stage (the summon gates: Knocksmash's free K3 stages too)
static bool pl_invuln(void);
static void pl_cancel(const char *why);
static void pl_request(int k);
static bool pl_live_set(const char *key, int v);
static void pl_chain(const float p[3], float kh, float to_u);
static void pl_move(float move[2], float kh, const float sdir[2], float mag);
static void gl_move(float move[2], float kh);  // (s139 genielim: Sora's root travel in Genie's Limit, sora_summon_gl.h)
static void nl_move(float move[2]);  // (s139 review: Never Land's flight moves the body through BotW's physics, sora_summon_nl.h)
static void pl_tick_press(uint64_t *press, const Target *tg, int ntg, const float p[3], float to_u, float kh);
static void pl_draw(float to_u);
static void pl_hud(SoraHudMenu *m);
// (s137 smenu) runtime/sora_summon_menu.h (after sora_plimit.h): the Summon page (KH2 page 2), the FPS Mode pane, the owned rule
#define SMN_SUB 8             // M.sub of the Summon list
#define CMD_SUMMON_CAST 150   // menu_tick results
#define CMD_SUMMON_DISMISS 151
#define CMD_SUMMON_FPS 152
static int smn_top_tick(uint64_t *press, int *ret);
static bool smn_toggle(void);
static int smn_sub_tick(uint64_t *press);
static bool smn_page2(void);
static bool smn_slate_row(void);
static void smn_cast(int code);
static void smn_hud(SoraHudMenu *m);
static bool smn_pane(int (*pad)(int, const int8_t *, const int8_t *, const uint8_t *));
static int smn_gauge_state(void);
static bool smn_live_set(const char *key, int v);
static bool dbm_set_key(const char *key, int v);
static void dbm_clip_tick(const float p[3]);
static void dbm_clip_other(void);  // (s143 dbggod, sora_dbgmenu.h)  // (s140 dbgmenu, sora_dbgmenu.h)
// (s93 b7-runes) runtime/sora_slate.h (after sora_limit.h): the Sheikah Slate row (Magnesis) + rune recon logs
#define SK_SUB 5        // (s93 review) M.sub of the Slate page (4 = the meals page)
#define CMD_SLATE 130   // menu_tick: the Slate page's row (CMD_SLATE + 0..SK_N-1)
static bool sk_open(void);
static bool sk_open_left(void);
static int sk_menu_tick(uint64_t *press);
static void sk_hud(SoraHudMenu *m);
static void sk_request(int i, int st, uint64_t present);
static bool sub_text_rows(SoraHudMenu *m, const char *const *rows, int ns, int sel_row);  // (s98, sora_meals.h)
static int sk_tick(uint64_t present, int st, uint32_t sw);
static void sk_recon_tick(uint64_t present);
static bool sk_live_set(const char *key, int v);
// (s93 b7-meals) runtime/sora_meals.h (after sora_camp.h): cooked meals on the Items page, behind live "meals"
static int meal_top_state(void);
static void meal_open(void);
static int meal_menu_tick(uint64_t *press);
static void meal_fx_show(const float feet[3], float face, float to_u);  // (s99 potfx, sora_meals.h)
static void meal_item_tick(float kh);  // (s98 potion, sora_meals.h)
static void meal_hud(SoraHudMenu *m);
static bool meal_live_set(const char *key, int v);
static bool meal_owns_items(void);  // (s93 review: live "meals" on = Items is the meals page; the Slate row sits there only when off)

// (s133 magic) Magnet + Reflect: runtime/sora_magic2.h (included after the spell objects below)
static bool mg_owned(int sp);                                           // a spell the player has (Magnet / Reflect: BotW milestones, sora_unlock.h)
static void mg_spawn(int kind, const float feet[3], float face, uint64_t target);
static void mg_cancel(const char *why);                                 // every handle + enemy override freed
static bool mg_live_set(const char *key, int v);
static int spell_list(int out[NSPELL]) {  // the Magic page's rows: the owned spells in KH2 order (KH_SPELLS order)
    int n = 0;
    for (int sp = 0; sp < NSPELL; sp++)
        if (mg_owned(sp)) out[n++] = sp;
    return n;
}

static int spell_state(int sp) {  // KH2 entry state (re:0x401D60): 0 usable, 2 usable below its Cost, 5 greyed
    if (mp_charging() || MP.mp <= 0 || fm_no_magic() || !mg_owned(sp)) return 5;  // (s68: Valor has no Magic, cmd DisableForm bit 1; s72 Wisdom does)
    int cost = KH_SPELLS[sp].cost;
    return cost < 255 && MP.mp < cost ? 2 : 0;
}

// The spell to cast (>= 0) or -1; eats the presses it used. (BTN_CLIMB = X is defined with the climb, further down.)
static int menu_tick(uint64_t *press, uint64_t held, float kh) {
    if (M.dead > 0) M.dead -= kh;
    if (g_slate_pass || g_slate_mode) *press &= ~BTN_MENU, held &= ~BTN_MENU;  // (s93: L + d-pad are BotW's, the Slate)
    if (ROXCMD) {  // (s111 roxcmd) Roxas: only Attack + Items; a Magic / Drive / Limit page left open by the body switch closes
        if (M.sub == 1 || M.sub == 2 || M.sub == 3) M.sub = -1, M.cursor = 0, M.sub_cursor = 0;
        if (M.cursor == 1 || M.cursor == 3) M.cursor = 0;
    }
    if (MKCMD && g_mkpane) {  // (s119 mkmenu; review: only while the exe draws it - an old exe / cmd.bin keeps Sora's working menu) Mickey's pane has no pages / cursor / L1 shortcuts: the buttons act themselves (KH2 button pane, S119_HELPER_MKMENU)
        if (M.shortcut) M.shortcut = false;
        M.sub = -1, M.cursor = 0, M.sub_cursor = 0;
        return -1;
    }
    bool sc = (held & BTN_SHORTCUT) != 0 && !ROXCMD;  // re:0x3F2470: the shortcut menu while L1 is held (s111 roxcmd: none for Roxas)
    if (sc != M.shortcut) M.shortcut = sc, sora_sfx_event("page", 1), LOG("[sora] menu: shortcut %s\n", sc ? "on" : "off");
    if (M.shortcut) {
        uint64_t face = *press & ((1ull << 2) | BTN_ROLL | BTN_JUMP | BTN_ATTACK);
        int slot = (face & (1ull << 2)) ? 0 : (face & BTN_ROLL) ? 1 : (face & BTN_JUMP) ? 2 : (face & BTN_ATTACK) ? 3 : -1;
        *press &= ~((1ull << 2) | BTN_ROLL | BTN_JUMP | BTN_ATTACK);
        if (slot < 0) return -1;
        int sp = SC.sc[slot];
        if (M.dead > 0) return -1;
        // (s84) empty slot: error SE + one log per press. GUESS: KH2 FM's empty shortcut, not traced (S84_CUST_PAGE.md)
        if (sp < 0) { sora_sfx_event("error", 1); LOG("[sora] menu: shortcut %d empty (error)\n", slot); return -1; }
        if (spell_state(sp) >= 4) { sora_sfx_event("error", 1); LOG("[sora] menu: shortcut %s greyed (error)\n", KH_SPELLS[sp].name); return -1; }
        return sp;
    }
    bool magic_ok = !mp_charging() && MP.mp > 0 && !fm_no_magic();  // re:0x401D60 Execute 2 / re:0x3C1D10
    {   // (s137 smenu) the FPS Mode pane owns the pad; KH2's page 2 (Attack / Summon / Limit) handles its own top page
        int sret;
        if (smn_top_tick(press, &sret)) return sret;
    }
    if (M.sub < 0) {
        if (*press & BTN_UP) M.cursor = (M.cursor + 3) % 4, M.cursor = ROXCMD && (M.cursor & 1) ? (M.cursor + 3) % 4 : M.cursor, sora_sfx_event("cursor", 1), LOG("[sora] menu: cursor %d\n", M.cursor);
        if (*press & BTN_DOWN) M.cursor = (M.cursor + 1) % 4, M.cursor = ROXCMD && (M.cursor & 1) ? (M.cursor + 1) % 4 : M.cursor, sora_sfx_event("cursor", 1), LOG("[sora] menu: cursor %d\n", M.cursor);
        bool open = (*press & BTN_RIGHT) || ((*press & BTN_ATTACK) && M.cursor != 0);
        if (open && M.cursor == 1 && magic_ok) M.sub = 1, M.sub_cursor = 0, *press &= ~BTN_ATTACK, sora_sfx_event("submenu", 1), LOG("[sora] menu: Magic open\n");
        else if (open && M.cursor == 1 && IN_LIMIT && lm_top_state() == 0)  // (s89: Limit Form's "Limit" page, cmd 707 SubMenu 12)
            M.sub = 3, M.sub_cursor = 0, *press &= ~BTN_ATTACK, sora_sfx_event("submenu", 1), LOG("[limit] menu: Limit page open\n");
        else if ((*press & BTN_ATTACK) && M.cursor == 3 && fm_top_state() == 0 && fm_in_form()) {  // (s68: Revert)
            *press &= ~BTN_ATTACK;
            return M.dead > 0 ? -1 : CMD_REVERT;
        } else if (open && M.cursor == 3 && fm_top_state() == 0 && !fm_in_form())
            M.sub = 2, M.sub_cursor = 0, *press &= ~BTN_ATTACK, sora_sfx_event("submenu", 1), LOG("[sora] menu: Drive open\n");
        else if (open && M.cursor == 2 && meal_top_state() == 0) *press &= ~BTN_ATTACK, meal_open();  // (s93 meals)
        else if (open && sk_open()) *press &= ~BTN_ATTACK;  // (s93: the Slate page, on the Items slot while "meals" is off, until the 5th row exists)
        else if ((*press & BTN_RIGHT) && M.cursor == 0 && sk_open_left()) *press &= ~BTN_RIGHT;  // (s137 user: d-pad Right on the Attack row = Link's ability page (the Slate), as d-pad Left was before page 2)
        else if ((*press & BTN_RIGHT) && M.cursor == 0 && pl_open_left()) *press &= ~BTN_RIGHT;  // (only when the Slate is off; the party Limits are on page 2's Limit row)  // (s133 limits: d-pad Right on the Attack row = the party Limits list; Left does it too when the Slate page does not take it)
        else if (open && M.cursor != 0) *press &= ~BTN_ATTACK, sora_sfx_event("error", 1), LOG("[sora] menu: entry %d greyed (error)\n", M.cursor);
        else if ((*press & BTN_LEFT) && !(*press & BTN_RIGHT) && smn_toggle()) *press &= ~BTN_LEFT;  // (s137 smenu: KH2's page toggle, when a summon is owned; user s137: the Slate is then on page 2)
        else if ((*press & BTN_LEFT) && !(*press & BTN_RIGHT) && sk_open_left()) *press &= ~BTN_LEFT;  // (s98 slateleft: d-pad Left = the Slate page, meals on or off)
        else if ((*press & BTN_LEFT) && !(*press & BTN_RIGHT) && pl_open_left()) *press &= ~BTN_LEFT;  // (s133 limits: d-pad Left = KH2's second top page, its Limit entry: the party Limits list)
        return -1;  // (cursor on Attack: A stays the attack press, B the jump)
    }
    if (M.sub == SMN_SUB) return smn_sub_tick(press);  // (s137 smenu: the Summon list)
    if (M.sub == PL_SUB) return pl_menu_tick(press);  // (s133 limits: the party Limits list)
    if (M.sub == 3) return lm_menu_tick(press);  // (s89 b4-limit: the Limit page)
    if (M.sub == 4) return meal_menu_tick(press);  // (s93 meals: Items)
    if (M.sub == SK_SUB) return sk_menu_tick(press);  // (s93 b7-runes: the Slate page)
    if (M.sub == 2) {  // (s68) the Drive page: (s72) a row per form, Valor Form / Wisdom Form
        int rf[FM_NFORMS], nr = fm_rows(rf);  // (s89 kb: locked forms are not listed, as KH2's Drive page)
        if (M.sub_cursor >= nr) M.sub_cursor = 0;
        if (*press & BTN_UP) M.sub_cursor = (M.sub_cursor + nr - 1) % nr, sora_sfx_event("cursor", 1);
        if (*press & BTN_DOWN) M.sub_cursor = (M.sub_cursor + 1) % nr, sora_sfx_event("cursor", 1);
        if (*press & (BTN_JUMP | BTN_LEFT)) {
            *press &= ~BTN_JUMP;
            M.sub = -1, M.cursor = 3;
            sora_sfx_event("cancel", 1), LOG("[sora] menu: Drive closed\n");
        } else if (*press & BTN_ATTACK) {
            *press &= ~BTN_ATTACK;
            if (M.dead <= 0) return CMD_FORM + rf[M.sub_cursor];
        }
        return -1;
    }
    int spl[NSPELL], nspl = spell_list(spl);  // (s133 magic: the list is the owned spells; the cursor is a row of it)
    if (nspl < 1) nspl = 1, spl[0] = SPELL_FIRE;
    if (M.sub_cursor >= nspl) M.sub_cursor = 0;
    if (*press & BTN_UP) M.sub_cursor = (M.sub_cursor + nspl - 1) % nspl, sora_sfx_event("cursor", 1), LOG("[sora] menu: magic cursor %d\n", M.sub_cursor);
    if (*press & BTN_DOWN) M.sub_cursor = (M.sub_cursor + 1) % nspl, sora_sfx_event("cursor", 1), LOG("[sora] menu: magic cursor %d\n", M.sub_cursor);
    if (*press & (BTN_JUMP | BTN_LEFT)) {  // cancel / left: back, top cursor to 0 (re:0x3B11E0, re:0x3B20F0)
        *press &= ~BTN_JUMP;
        M.sub = -1, M.cursor = 0;
        sora_sfx_event("cancel", 1), LOG("[sora] menu: Magic closed\n");
        return -1;
    }
    if (*press & BTN_ATTACK) {
        *press &= ~BTN_ATTACK;
        int sp = spl[M.sub_cursor];
        if (M.dead > 0 || spell_state(sp) >= 4) { sora_sfx_event("error", 1); LOG("[sora] menu: %s greyed (error)\n", KH_SPELLS[sp].name); return -1; }
        return sp;  // (the submenu stays open until the cast starts: cast_start closes it)
    }
    return -1;
}

// The playing cast: its magic_cast frame (trigger 17 whose param = the motion id; A362 / A364 carry two)
static struct { int spell; float cast_at; bool fired; int fx_cast; } C = {-1, 0, false, -1};
// the cast glow's place: the Keyblade tip (KH2 bind 0x4003). GUESS: the cast slots have no blade segments in
// sora_hits.bin, so 110 cm up and 50 cm ahead of the feet
static void cast_tip(const float feet[3], float to_u, float out[3]) {
    out[0] = feet[0] + sinf(S.face) * 50 * to_u, out[1] = feet[1] + 110 * to_u, out[2] = feet[2] + cosf(S.face) * 50 * to_u;
}
static float cast_frame(const Mot *m, int motion) {
    for (int i = 0; m && i < m->nft; i++) {
        uint16_t fr;
        int16_t p1;
        memcpy(&fr, m->ft + i * 8, 2), memcpy(&p1, m->ft + i * 8 + 4, 2);
        if (m->ft[i * 8 + 2] == 17 && p1 == motion) return fr;
    }
    return mot_frame_trigger(m, 17, 0);
}

// Spell objects (L 2.2, Lv1; KH cm and KH frames)
typedef struct { int kind; float t, pos[3], dir[2]; uint64_t target, home; int nhit, ticks; uint64_t hit_actor[16]; bool on; int fx; int tier; } SpellObj;
// (s87c spell tiers) Fire -> Fira -> Firaga etc. (user 2026-11-01: unlocked by Sora's LV; Str / Mag stay at LV 1's, so
// the tiers are where spell damage grows). Tier 0 / 1 / 2 = KH2 Lv1 / Lv2 / Lv3. VERIFIED from 00battle.bin atkp
// (sub = level << 9: Lv1 0, Lv2 512, Lv3 1024; the normal cast, not the finisher rows): Blizzard 248 + Thunder 250 Power
// 250 / 300 / 375, Thunder kb1 30 / 36 / 42, Fire 502 ticks 50 x all, its last hit 150 / 150 / 175. Radii / speeds from
// PHH spells_kh2.py (recon/phh/PHH_PARTY_MAGIC.md 3.5): Fire ring r 200 / 220 / 240, Blizzard shard 40 / 70 / 100 cm per
// frame for 30 / 28 / 26 frames, Thunder burst r 170 / 240 / 340. Cure: KH2's heal scales with Mag (not mined); PHH's
// shares 0.3 / 0.45 / 0.6 -> 1 x / 1.5 x / 2 x the user's Cure (cureq, 2 hearts) = 2 / 3 / 4 hearts (APPROX).
// GUESS: the unlock LVs 25 / 50 (user picked "Sora's level", numbers mine). Live "set sptier2 <LV>", "set sptier3 <LV>",
// "set sptier <1..3>" = force a tier (0 = by LV), "set sptiers 0" = all Lv1 (the old rule).
static int g_sp_lv2 = 25, g_sp_lv3 = 50, g_sp_force = 0, g_sp_tiers = 1;
static const int SP_FIRE_R[3] = {200, 220, 240}, SP_FIRE_LAST[3] = {150, 150, 175};
static const int SP_POW[3] = {250, 300, 375}, SP_THU_KB[3] = {30, 36, 42}, SP_THU_R[3] = {170, 240, 340};
static const int SP_BLIZ_SPD[3] = {40, 70, 100}, SP_BLIZ_LIFE[3] = {30, 28, 26}, SP_CURE_X2[3] = {2, 3, 4};
static const char *const SP_TIER_NAME[NSPELL][3] = {
    {"Fire", "Fira", "Firaga"}, {"Blizzard", "Blizzara", "Blizzaga"}, {"Thunder", "Thundara", "Thundaga"}, {"Cure", "Cura", "Curaga"},
    {"Magnet", "Magnera", "Magnega"}, {"Reflect", "Reflera", "Reflega"}};  // (s133 magic: sys.bar messages 1300-1305)
static int spell_tier(void) {
    if (!g_sp_tiers) return 0;
    if (g_sp_force > 0) return g_sp_force > 3 ? 2 : g_sp_force - 1;
    int lv = sl_lv();
    return lv >= g_sp_lv3 ? 2 : lv >= g_sp_lv2 ? 1 : 0;
}
static void spell_tier_tick(void) {  // one LOG when the tier changes (LV up, slot load, live keys)
    static int last = -1;
    int t = spell_tier();
    if (t == last) return;
    LOG("[sora] spells now tier %d: %s / %s / %s / %s (LV %d; -ra at LV %d, -ga at LV %d)\n", t + 1, SP_TIER_NAME[0][t],
        SP_TIER_NAME[1][t], SP_TIER_NAME[2][t], SP_TIER_NAME[3][t], sl_lv(), g_sp_lv2, g_sp_lv3);
    last = t;
}
// (s88 menu labels) the command menu's atlas (cmd.bin, tools/hudbake) carries "Fira" .. "Curaga" as labels 15..22 =
// {Fira, Firaga, Blizzara, Blizzaga, Thundara, Thundaga, Cura, Curaga} (KH2's own sys.bar 1289 / 1290 / 1295 / 1296 / 1292 /
// 1293 / 1298 / 1299) once cmd_s88.bin is installed as cmd.bin; an older atlas has 15 labels (SKLX trailer) -> the old
// Fire / Blizzard / Thunder / Cure labels stay. The DLL reads the trailer's count itself (no exe change). Live "set splabel 0" = off.
static int g_sp_label = 1;
static int sp_label_n(void) {
    static int n = -1;
    if (n >= 0) return n;
    n = 0;
    const char *p = getenv("SAVAGEWIND_SORA_CMD");
    if (p && !strcmp(p, "0")) return n;
    if (!p || !*p || !strcmp(p, "1")) p = "D:/SoraBotW/data/hud/cmd.bin";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) return n;
    uint32_t magic = 0, lx = 0, t[2] = {0, 0};
    // header u32 x4, HImg[0] 16 B, HImg[1] {w, h, off, reserved}: its reserved word (file offset 16 + 16 + 12) = the 'SKLX' trailer
    if (fread(&magic, 4, 1, f) == 1 && !memcmp(&magic, "SKCM", 4) && !fseek(f, 16 + 16 + 12, SEEK_SET) && fread(&lx, 4, 1, f) == 1 && lx &&
        !fseek(f, (long)lx, SEEK_SET) && fread(t, 4, 2, f) == 2 && !memcmp(&t[0], "SKLX", 4) && t[1] <= 128)  // (s147: was 64 = the exe cap before s137 review M4; cmd.bin has 75 labels since s138 -> read as 0)
        n = (int)t[1];
    fclose(f);
    LOG("[sora] menu: %s has %d labels: spell tier names %s\n", p, n, n >= 23 ? "ON (Fira .. Curaga)" : "OFF (old labels: no s88 atlas)");
    return n;
}
// (s133 magic) labels 36..41 = Magnet Magnera Magnega Reflect Reflera Reflega (sys.bar 1300-1305; tools_s119/hudbake LabelNames, needs a
// cmd.bin rebaked with them: recon/S133_HELPER_MAGIC.md). An older atlas has no such labels: the rows show "Magic" (label 1), logged once.
#define MG_LABEL_BASE 36
#define MG_LABEL_N 42
static int spell_label(int sp) {  // cmd.bin label id of spell sp (KH_SPELLS order Fire Blizzard Thunder Cure Magnet Reflect) at the current tier
    int t = spell_tier();
    if (sp >= SPELL_MAGNET) {
        static int said;
        if (sp_label_n() >= MG_LABEL_N) return MG_LABEL_BASE + (sp - SPELL_MAGNET) * 3 + t;
        if (!said) said = 1, LOG("[magic] the command-menu atlas (cmd.bin) has no Magnet / Reflect labels (%d < %d): their rows read \"Magic\" until cmd_s133m.bin is installed\n", sp_label_n(), MG_LABEL_N);
        return 1;
    }
    if (!g_sp_label || t <= 0 || sp < 0 || sp > 3 || sp_label_n() < 23) return 4 + sp;
    return 15 + sp * 2 + (t - 1);  // (labels 15..22 = a (Lv2, Lv3) pair per spell)
}
// (s88 tier fx) KH2 has its own PAX block per spell level (magic/FIRE_2.mag = Fira, FIRE_3.mag = Firaga, THUNDER_2 / _3 ...,
// the same element numbers as Lv1: ring 2, hits 3 / 4, bolt 2 ...; the bigger rings / more bolts are IN those recordings,
// so the scale stays 1 = to_u). tools/fxbake.py (S88_TIERS) packs them as "<name>_2" / "<name>_3" into fx_s88.bin; with
// the old fx.bin (no such element) the Lv1 element plays. Live "set spfx 0" = always the Lv1 element.
static int g_sp_fx = 1, g_sp_dg = 1;  // (g_sp_dg: Drive per hit by the tier's atkp rows, live "set spdg 0" = Lv1 rows)
static const char *sp_fx(const char *base, int tier, char *buf, size_t n) {
    if (!g_sp_fx || tier <= 0) return base;
    // (s88 review) the old fx.bin has none of the tier elements: probed once (when the pack is up) so the per-cast
    // misses do not use up sora_fx's 8 "unknown element" warnings
    static int have = -1;
    if (have < 0 && sora_fx_id("fire_ring") >= 0) {
        have = sora_fx_id("fire_ring_2") >= 0;
        LOG("[sora] spell tier fx: %s\n", have ? "tier elements in the pack (fx_s88.bin)" : "old fx.bin, Lv1 elements for every tier");
    }
    if (have <= 0) return base;
    snprintf(buf, n, "%s_%d", base, tier + 1);
    return sora_fx_id(buf) >= 0 ? buf : base;
}
// (KH2 magi heights / radii per level, spells_kh2.py: Fire cylinder h 80 / 130 / 180, Blizzard shard r 80 / 100 / 120)
static const int SP_FIRE_H[3] = {80, 130, 180}, SP_BLIZ_RAD[3] = {80, 100, 120};
// (s59 user wish) Blizzard homing: KH2 Lv1 does NOT home (magi homing rate 0, only Wisdom finisher shards do) - this is
// PHH's user-designed passive assist (aim_assist.Homing.steer_yaw): turn toward the nearest living enemy AHEAD within
// g_bliz_r cm (the cast's lock target first), at most g_bliz_turn rad per KH frame, sticky while within 1.5 x g_bliz_r.
// Live "set blizhome 0/1", "set blizr <cm>", "set blizturn <mrad/frame>".
static int g_bliz_home = 1;
static float g_bliz_r = 500, g_bliz_turn = 0.07f;
static SpellObj g_spells[4];
static int g_cure_fx[2] = {-1, -1};  // Cure's flower + sparkles (BindOnlyPos: follow his centre while alive)
static int spell_dmg(int power) { int d = (int)lroundf(g_dmg_base * (float)power / 100.0f); return d < 1 ? 1 : d; }
static void spell_spawn(int kind, const float feet[3], float face, uint64_t target) {
    if (kind >= SPELL_MAGNET) { mg_spawn(kind, feet, face, target); return; }  // (s133 magic: their own state, sora_magic2.h)
    SpellObj *o = NULL;
    for (int i = 0; i < 4 && !o; i++) if (!g_spells[i].on) o = &g_spells[i];
    if (!o) o = &g_spells[0];
    memset(o, 0, sizeof *o);
    o->on = true, o->kind = kind, o->target = target, o->tier = spell_tier();  // (s87c: kept for the whole cast)
    memcpy(o->pos, feet, sizeof o->pos);
    o->dir[0] = sinf(face), o->dir[1] = cosf(face);
    float to_u = 0.01f * g_k, up[3] = {feet[0], feet[1] + 100 * to_u, feet[2]}, ctr[3] = {feet[0], feet[1] + 90 * to_u, feet[2]};
    o->fx = -1;
    char nb[48];
    if (kind == SPELL_FIRE) o->fx = sora_fx_event(sp_fx("fire_ring", o->tier, nb, sizeof nb), feet, face, to_u);
    else if (kind == SPELL_BLIZZARD) o->fx = sora_fx_event(sp_fx("blizzard_shard", o->tier, nb, sizeof nb), up, face, to_u);
    else if (kind != SPELL_THUNDER) {  // CURE (bind 0x4009 = his body centre, GUESS 90 cm)
        sora_fx_event(sp_fx("cure_ring", o->tier, nb, sizeof nb), feet, face, to_u);
        g_cure_fx[0] = sora_fx_event(sp_fx("cure_flower", o->tier, nb, sizeof nb), ctr, face, to_u);
        g_cure_fx[1] = sora_fx_event(sp_fx("cure_sparkle", o->tier, nb, sizeof nb), ctr, face, to_u);
    }
    LOG("[sora] spell %s (%s, tier %d) at %.2f %.2f %.2f (target %llx)\n", KH_SPELLS[kind].name,
        SP_TIER_NAME[kind][o->tier], o->tier + 1, feet[0], feet[1], feet[2], (unsigned long long)target);
}
static void spell_hit(SpellObj *o, uint64_t actor, const float epos[3], int power, int f54) {
    static const char *const HIT_FX[] = {"fire_hit", "blizzard_hit", "thunder_hit"};  // (KH_SPELLS order; Cure: none)
    if (o->kind >= 0 && o->kind < 3) {
        char nb[48];
        sora_fx_event(sp_fx(power >= 150 && o->kind == SPELL_FIRE ? "fire_hit_last" : HIT_FX[o->kind], o->tier, nb, sizeof nb), epos, 0, 0.01f * g_k);
    }
    if (o->nhit < 16) o->hit_actor[o->nhit++] = actor;
    int d = sl_hit(actor, -1, power, g_dmg_base, 1);  // (s54 levels: Mag; "set lvdmg 0" = spell_dmg's old rule)
    if (o->kind >= SPELL_FIRE && o->kind <= SPELL_THUNDER && ab_on(AB.on, AB_FIRE_BOOST + o->kind)) {  // (s82 element Boosts)
        int b = ab_boost(d, AB_ELEMENT_UP);
        LOG("[sora] %s Boost: +%d%% dmg %d -> %d\n", KH_SPELLS[o->kind].name, AB_ELEMENT_UP, d, b);
        d = b;
    }
    float to_u = 0.01f * g_k;
    int f40 = KH_SPELLS[o->kind].f40;
    if (o->kind == SPELL_THUNDER) ah_thunder_dmg(actor, &f54, &f40);  // (s55/s57 airhold: KH2 pop, no BotW ragdoll / shock)
    f54 = ah_f54(actor, epos, f54, to_u);  // (s55 airhold)
    linkpos_damage_enemy_ex(actor, d, f54, f40);
    {  // (s55 airhold) the spells' atkp rows, Lv1 (S55_HELPER_AIRHOLD.md 1.2): Thunder 250 = blow (30, 0), its finisher
       // code +1 = (45, 0); Fire 502 ticks = blow (6, 0), the last hit react 3; Blizzard 248 = react 3, strength 50
        int react = 3, kb1 = 0, kb2 = 0;
        if (o->kind == SPELL_THUNDER) {
            bool fin = KH_SPELLS[SPELL_THUNDER].finish >= 0 && S.motion == KH_SPELLS[SPELL_THUNDER].finish;
            float rise = g_ah_thunder_rise * (float)SP_THU_KB[o->tier] / 30.0f;  // (s87c: Lv2 / Lv3 kb 36 / 42 of 30)
            react = fin ? 7 : 4, kb1 = (int)lroundf(fin ? rise * 1.5f : rise);
        } else if (o->kind == SPELL_FIRE) react = power >= 150 ? 3 : 4, kb1 = power >= 150 ? 0 : 6;
        else if (o->kind == SPELL_BLIZZARD) kb1 = 50;
        ah_hit(actor, epos, o->pos, react, kb1, kb2, f54, to_u, KH_SPELLS[o->kind].name);
    }
    // (s67 drive: the spells' atkp rows; s88: Fira / Firaga's own rows for Fire (Blizzard 6 / Thunder 5 are the same at every level))
    int dkey = o->kind == SPELL_FIRE ? 502 : o->kind == SPELL_BLIZZARD ? 248 : 250;
    if (g_sp_dg && o->kind == SPELL_FIRE && o->tier > 0) dkey = 50200 + (o->tier - 1) * 10 + (power >= 150 ? 2 : o->ticks > 0 ? 1 : 0);
    dg_hit(dkey, actor);
    LOG("[sora] HIT %llx by %s: damage %d f54 %#x f40 %d\n", (unsigned long long)actor, KH_SPELLS[o->kind].name, d, f54,
        f40);
}
static bool spell_hit_before(const SpellObj *o, uint64_t a) {
    for (int k = 0; k < o->nhit; k++) if (o->hit_actor[k] == a) return true;
    return false;
}
#include "sora_magic2.h"  // (s133 magic: Magnet + Reflect; needs spell_tier, cast_tip, sl_hit, ah_*, dg_hit, kbfx_name, ul_spell_owned)
// (s73) Wisdom Form's Shoot (PHH projectiles_kh2.py, KH2 'wisd' BDX in P_EX100_MAGF.mdlx): each Shoot motion frame
// trigger id 1 with element 25..43 (C300 604 @ 10/20/30, finisher C303 616 x13, air C330 724 @ 10/20/30) fires one
// bullet from the Keyblade tip (bind 0x4003; GUESS cast_tip) along his facing: atkp 859, SubId 5 for element 40 / 3 for
// 27 32 35 43 / else 0 = Power 100 / 25 / 20, hit radius 50; speed 20 +2 per frame up to 80; homes on his battle target
// by <= 0.031416 rad per frame until within 100, then straight; after 24 frames it fades over 8; its first hit ends it.
// The hit volume around the enemy's feet point is a GUESS (BotW gives no body radius): r 50 + 50, -50..220 cm up.
// Live "set wshot 0" = off.
typedef struct { bool on; float t, speed, pos[3], dir[3]; uint64_t target; int power, sub, fx; } WisShot;
static WisShot g_wshot[16];
static int g_wshot_on = 1;
static int g_wshot_dmg = 40;  // (s74 user: shots hit too hard) % of the KH2 power; live "set wshotdmg N"
// (s87c wisdom combo) KH2's Wisdom shot combo: each swing of the ground/air combo (the existing combo_gate / limits, so
// Combo Plus etc. already set its length) fires faster and denser than the last: start speed 20 + 4/step ground (+8 more
// in the air), up to 2 extra bullets between the 3 trigger frames (air 1), and the last step (the C303 finisher, or the
// air combo's last swing) goes out at speed 44 with +50% power. Power per step +10%. The 859 rows (sub 0/768/1280 =
// Power 20/25/100, react 3/3/7) are VERIFIED from 00battle.bin atkp; the ramp numbers are a GUESS (no KH2 data for them).
// Live "set wcombo 0" = the old fixed burst (every swing identical).
static int g_wcombo = 1;
// (s89b user: "speeds up way too fast, make it like vanilla KH2") the ramp above is invented, so it is OFF by default:
// every step fires at the motion's own trigger frames, speed 20, no extra bullets, no +% power (KH2's atkp rows
// already give the finisher its power). The combo still advances (S.hit above). Live "set wramp 1" = the s87c ramp.
static int g_wramp = 0;
static bool wshot_free(void) {  // (s87c review) a free bullet + fx headroom (sora_fx.c's 32 slots evict the oldest)
    bool any = false;
    for (int i = 0; i < 16 && !any; i++) any = !g_wshot[i].on;
    return any && sora_fx_count() <= 24;
}
static void wshot_spawn(int element, const float tip[3], float face, uint64_t target, float to_u, float speed0, int pct, bool flash) {
    WisShot *o = NULL;
    for (int i = 0; i < 16 && !o; i++) if (!g_wshot[i].on) o = &g_wshot[i];
    if (!o) {  // (s87c review: full = the oldest gives way, not always slot 0)
        o = &g_wshot[0];
        for (int i = 1; i < 16; i++) if (g_wshot[i].t > o->t) o = &g_wshot[i];
        sora_fx_kill(o->fx);
    }
    memset(o, 0, sizeof *o);
    o->on = true, o->speed = speed0, o->target = target;
    o->sub = element == 40 ? 5 : (element == 27 || element == 32 || element == 35 || element == 43) ? 3 : 0;
    o->power = o->sub == 5 ? 100 : o->sub == 3 ? 25 : 20;
    o->power = (o->power * pct + 50) / 100;  // (s87c wisdom combo: step / finisher power)
    memcpy(o->pos, tip, sizeof o->pos);
    o->dir[0] = sinf(face), o->dir[1] = 0, o->dir[2] = cosf(face);
    char nm[32];
    snprintf(nm, sizeof nm, "wisdom_shot_%d", element);
    if (flash) sora_fx_event(nm, tip, face, to_u);  // (the tip flash; s87c review: not for the extra bullets - fx slots)
    o->fx = sora_fx_event("wisdom_bullet", tip, face, to_u);
    sora_sfx_event("wisdom_shot", 1.0f);
    LOG("[wisdom] shot element %d (power %d) at %.2f %.2f %.2f, target %llx\n", element, o->power, tip[0], tip[1], tip[2],
        (unsigned long long)target);
}
static void wshot_tick(float kh, float to_u) {
    uint64_t actor[64];
    float pos[64][3];
    int32_t life[64];
    int n = -1;
    for (int si = 0; si < 16; si++) {
        WisShot *o = &g_wshot[si];
        if (!o->on) continue;
        if (n < 0) n = linkpos_enemies(actor, pos, life, 64);
        bool fading = o->t >= 24;
        o->t += kh;
        int ti = -1;
        for (int i = 0; i < n && o->target; i++) if (actor[i] == o->target && life[i] > 0) ti = i;
        if (ti >= 0 && !fading) {  // (vector_homing re:0x3B7200: turn by <= 0.031416 rad per frame while > 100 cm away)
            float aim[3] = {pos[ti][0] - o->pos[0], pos[ti][1] + 90 * to_u - o->pos[1], pos[ti][2] - o->pos[2]};
            float d = sqrtf(aim[0] * aim[0] + aim[1] * aim[1] + aim[2] * aim[2]);
            if (d > 100 * to_u) {
                for (int c = 0; c < 3; c++) aim[c] /= d;
                float dot = o->dir[0] * aim[0] + o->dir[1] * aim[1] + o->dir[2] * aim[2];
                float ang = acosf(dot > 1 ? 1 : dot < -1 ? -1 : dot), lim = 0.031416f * kh;
                if (ang <= lim) memcpy(o->dir, aim, sizeof o->dir);
                else if (ang > 1e-5f) {  // (slerp by lim / ang)
                    float u = lim / ang, sa = sinf(ang), a = sinf((1 - u) * ang) / sa, b = sinf(u * ang) / sa, l = 0;
                    for (int c = 0; c < 3; c++) o->dir[c] = a * o->dir[c] + b * aim[c], l += o->dir[c] * o->dir[c];
                    l = sqrtf(l);
                    for (int c = 0; c < 3; c++) o->dir[c] /= l;
                }
            } else o->target = 0;  // (within 100: straight from here)
        }
        o->speed = fminf(80, o->speed + 2 * kh);
        for (int c = 0; c < 3; c++) o->pos[c] += o->dir[c] * o->speed * kh * to_u;
        sora_fx_move(o->fx, o->pos, atan2f(o->dir[0], o->dir[2]));
        float r = 100 * to_u;
        for (int i = 0; i < n && o->on && !fading; i++) {
            float dx = pos[i][0] - o->pos[0], dz = pos[i][2] - o->pos[2], dy = o->pos[1] - pos[i][1];
            if (life[i] <= 0 || dx * dx + dz * dz > r * r || dy < -50 * to_u || dy > 220 * to_u) continue;
            int pw = (int)lroundf(o->power * g_wshot_dmg / 100.0f);
            int dmg = sl_hit(actor[i], life[i], pw < 1 ? 1 : pw, g_dmg_base, 1);
            int f54 = o->sub != 5 ? F54_SMALL : g_fin_big || ah_small(actor[i]) ? g_fin_f54 : 0x11;
            // (s74) 859 has no AH_ATKP row: the old default react 4 = BLOW with kb1 0x7fff = a 32767 cm/frame rise (user:
            // enemies shot into the sky). A plain hit (class 3: airborne hop / BotW small flinch) instead.
            int react = 3, kb1 = 0x7fff, kb2 = 0;
            ah_atkp(859, &react, &kb1, &kb2);
            f54 = ah_hit_f54(actor[i], pos[i], f54, react, to_u);
            linkpos_damage_enemy(actor[i], dmg, f54);
            ah_hit(actor[i], pos[i], o->pos, react, kb1, kb2, f54, to_u, "wisdom");
            const char *burst = o->sub == 5 ? "wisdom_end_21" : "wisdom_end_22";  // (GUESS: which .21..24 is the hit)
            sora_fx_event(burst, o->pos, 0, to_u);
            int bse = sora_sfx_id(burst);  // (s75: the burst element's own SE, se999 #12 / #13; 859 has no hit SE row)
            if (bse >= 0) sora_sfx_play_at(bse, 1.0f, pos[i]);
            else sora_sfx_hit(859, 1.0f, pos[i]);
            dg_hit(859, actor[i]);
            // (s87c review) a bullet hit = the combo's "connected" (re:0x3D54B0 bit 25; GUESS for bullets, like the object
            // hit at the blade): without it combo_gate dropped every Wisdom press (live12: 50 attacks, all d74 1), so the
            // ramp / C303 finisher never came. Only with wcombo on ("set wcombo 0" = the old rule).
            if (g_wcombo && IN_WISDOM) S.hit = true;
            LOG("[wisdom] HIT %llx: power %d damage %d f54 %#x\n", (unsigned long long)actor[i], o->power, dmg, f54);
            o->on = false, sora_fx_kill(o->fx);
        }
        if (o->on && !fading && o->t >= 24) sora_fx_fadeout(o->fx, 8);
        if (o->on && o->t >= 32) o->on = false, sora_fx_kill(o->fx);
    }
}
static void spell_tick(float kh, float to_u, const float feet[3]) {
    spell_tier_tick();  // (s87c: one LOG per tier change)
    mg_tick(kh, to_u, feet);  // (s133 magic)
    uint64_t actor[64];
    float pos[64][3];
    int32_t life[64];
    int n = -1;
    for (int k = 0; k < 2; k++) {
        float ctr[3] = {feet[0], feet[1] + 90 * to_u, feet[2]};
        if (g_cure_fx[k] >= 0 && sora_fx_alive(g_cure_fx[k])) sora_fx_move(g_cure_fx[k], ctr, S.face);
        else g_cure_fx[k] = -1;
    }
    for (int si = 0; si < 4; si++) {
        SpellObj *o = &g_spells[si];
        if (!o->on) continue;
        if (n < 0) n = linkpos_enemies(actor, pos, life, 64);
        float t0 = o->t;
        o->t += kh;
        if (o->kind == SPELL_FIRE) {  // a cylinder on Sora, r 200 h 80, 44 frames, hits at 0 / 18 / 36 (the last Power 150)
            memcpy(o->pos, feet, sizeof o->pos);
            sora_fx_move(o->fx, feet, S.face);
            int tick = (int)(t0 / 18.0f);
            if (t0 == 0 || (int)(o->t / 18.0f) > tick) {
                bool last = o->t >= 36;
                o->ticks = (int)(o->t / 18.0f);  // (s88: the wave 0 / 1 / 2, for the Drive row)
                o->nhit = 0;  // (each tick re-hits)
                float r = SP_FIRE_R[o->tier] * to_u, h = SP_FIRE_H[o->tier] * to_u, best = 1e9f, bdy = 0;  // (s87c tier radius)
                for (int i = 0; i < n; i++) {
                    float ex = pos[i][0] - o->pos[0], ez = pos[i][2] - o->pos[2];
                    if (life[i] > 0 && ex * ex + ez * ez < best) best = ex * ex + ez * ez, bdy = pos[i][1] - o->pos[1];
                }
                LOG("[sora] fire tick at t %.0f: %d enemies, nearest %.2f u (dy %.2f), r %.2f u\n", t0, n, sqrtf(best), bdy, r);
                for (int i = 0; i < n; i++) {
                    float dx = pos[i][0] - o->pos[0], dz = pos[i][2] - o->pos[2], dy = pos[i][1] - o->pos[1];
                    if (life[i] > 0 && dx * dx + dz * dz < r * r && dy > -h && dy < 2 * h)
                        spell_hit(o, actor[i], pos[i], last ? SP_FIRE_LAST[o->tier] : 50, last ? 0x14 : 0x0f);
                }
                brk_spell(BRKHIT_FIRE, o->pos, r, h, 2 * h);  // (s55: wood in the ring burns at once)
                fire_world(o->pos, last, r, h, "sora");  // (s59: BotW fire: campfires, pots, torches, enemies)
            }
            if (o->t >= 44) o->on = false, sora_fx_kill(o->fx);  // (KH2 magi FireTime 44 kills the kept ring)
        } else if (o->kind == SPELL_BLIZZARD) {  // one shard along the facing, 40 / 70 / 100 cm/frame (tier), 30 / 28 / 26 frames, r 80, first hit ends it
            float step = SP_BLIZ_SPD[o->tier] * kh * to_u, r = SP_BLIZ_RAD[o->tier] * to_u;  // (s87c tier speed)
            int life_f = SP_BLIZ_LIFE[o->tier];
            if (g_bliz_home && o->t - kh < life_f) {  // (s59 assist; not while fading)
                float R = g_bliz_r * to_u, best = 1e30f;
                int pick = -1;
                for (int i = 0; i < n; i++) {
                    if (life[i] <= 0) continue;
                    float dx = pos[i][0] - o->pos[0], dz = pos[i][2] - o->pos[2], d2 = dx * dx + dz * dz;
                    if (actor[i] == o->home && d2 <= 2.25f * R * R) { pick = i; break; }  // (sticky: keep 1.5 x R)
                }
                if (pick < 0) {
                    o->home = 0;
                    for (int i = 0; i < n; i++) {
                        if (life[i] <= 0) continue;
                        float dx = pos[i][0] - o->pos[0], dz = pos[i][2] - o->pos[2], d2 = dx * dx + dz * dz;
                        if (d2 > R * R || dx * o->dir[0] + dz * o->dir[1] < 0) continue;  // (ahead only: no U-turns)
                        float k = actor[i] == o->target ? 0 : d2;  // (the cast's lock target first)
                        if (k < best) best = k, pick = i;
                    }
                    if (pick >= 0) {
                        o->home = actor[pick];
                        LOG("[sora] blizzard homes on %llx%s\n", (unsigned long long)o->home, o->home == o->target ? " (lock)" : "");
                    }
                }
                if (pick >= 0) {
                    float yaw = atan2f(o->dir[0], o->dir[1]), want = atan2f(pos[pick][0] - o->pos[0], pos[pick][2] - o->pos[2]);
                    float d = remainderf(want - yaw, 6.2831853f), lim = g_bliz_turn * kh;
                    yaw += d > lim ? lim : d < -lim ? -lim : d;
                    o->dir[0] = sinf(yaw), o->dir[1] = cosf(yaw);
                }
            }
            // (s87c review) sub-steps of <= 60 cm: Blizzara / Blizzaga move 140 / 200 cm per present (kh ~2), more than
            // the 160 cm hit width, and skipped enemies; each sub-step is tested like the old single step
            int nsub = (int)ceilf(step / (60 * to_u));
            if (nsub < 1) nsub = 1;
            for (int ss = 0; ss < nsub && o->on; ss++) {
                o->pos[0] += o->dir[0] * step / nsub, o->pos[2] += o->dir[1] * step / nsub;
                for (int i = 0; i < n && o->on; i++) {
                    float dx = pos[i][0] - o->pos[0], dz = pos[i][2] - o->pos[2], dy = pos[i][1] - o->pos[1];
                    if (life[i] > 0 && dx * dx + dz * dz < r * r && dy > -2 * r && dy < 2 * r)
                        spell_hit(o, actor[i], pos[i], SP_POW[o->tier], 0x11), o->on = false;
                }
                if (o->on && brk_spell(BRKHIT_ICE, o->pos, r, 2 * r, 2 * r)) o->on = false;  // (s55: Arrow rate; first hit ends it)
            }
            float sp[3] = {o->pos[0], o->pos[1] + 100 * to_u, o->pos[2]};
            sora_fx_move(o->fx, sp, atan2f(o->dir[0], o->dir[1]));
            if (!o->on) sora_fx_kill(o->fx);
            else if (o->t >= life_f) o->on = false, sora_fx_fadeout(o->fx, 12);  // (KH2 BLIZZARD_FADE 12)
        } else if (o->kind == SPELL_THUNDER) {  // a bolt at the target (none: 300 cm ahead) after 6 frames, burst r 170
            if (o->t < 6) continue;
            float at[3] = {o->pos[0] + o->dir[0] * 300 * to_u, o->pos[1], o->pos[2] + o->dir[1] * 300 * to_u};
            for (int i = 0; i < n; i++) if (actor[i] == o->target) memcpy(at, pos[i], sizeof at);
            float r = SP_THU_R[o->tier] * to_u;  // (s87c tier burst)
            for (int i = 0; i < n; i++) {
                float dx = pos[i][0] - at[0], dz = pos[i][2] - at[2], dy = pos[i][1] - at[1];
                if (life[i] > 0 && !spell_hit_before(o, actor[i]) && dx * dx + dz * dz + dy * dy < r * r * 4 &&
                    dx * dx + dz * dz < r * r)
                    spell_hit(o, actor[i], pos[i], SP_POW[o->tier], 0x14);
            }
            brk_spell(BRKHIT_THUNDER, at, r, 2 * r, 2 * r);  // (s55: BotW's lightning rule)
            char nb[48];
            sora_fx_event(sp_fx("thunder_bolt", o->tier, nb, sizeof nb), at, S.face, to_u);  // (s88: Thundara / Thundaga's own bolt)
            LOG("[sora] thunder at %.2f %.2f %.2f: %d hit\n", at[0], at[1], at[2], o->nhit);
            o->on = false;
        } else {  // CURE: heal at once
            // (s85f user: "doesn't heal anymore", wants 2 hearts: g_cure_q quarter hearts, live "set cureq"; 0 = the
            // old g_cure_pct share)
            extern int g_hud_life_max;
            int q = g_cure_q > 0 ? g_cure_q : (int)lroundf((float)g_hud_life_max * g_cure_pct / 100.0f);
            q = q * SP_CURE_X2[o->tier] / 2;  // (s87c: Cura 1.5 x, Curaga 2 x)
            int v = link_heal(q < 1 ? 1 : q);
            LOG("[sora] %s +%d quarter hearts: life %d (max %d)\n", SP_TIER_NAME[3][o->tier], q, v, g_hud_life_max);
            o->on = false;
        }
    }
}

// ------------------------------------------------------------------ per game frame
static void slog_open(void) {
    const char *prefix = getenv("SAVAGEWIND_LINK_PROBE");
    char path[512];
    snprintf(path, sizeof path, "%s.sora.txt", prefix && *prefix ? prefix : "build/sora");
    g_slog = fopen(path, "a");  // (s64: "w" wiped the play log at every hot reload; the header line marks a reload)
    if (g_slog)
        fprintf(g_slog, "# present rel act motion clock | sx sy | camfx camfz | dirx dirz speed | face_deg | "
                        "vx vy vz (u/s out) | x y z (body) | state | air apex base req | b | game time ratio | "
                        "slot clock special d74 root_cm hits\n");
}

// KH2's HP / MP gauge from Link's life (quarter hearts): 1 KH2 HP per quarter heart (30 hearts = 120 = KH2's max HP:
// the bar length follows the max). Link's max life isn't read yet: the highest value seen stands in (a floor of 20
// showed 3 full hearts as 12/20, hud_s17a). MP: no source yet - KH2's base 100 held full (mp < 0 = MP Charge, pink).
int g_hud_life_max;  // (Cure heals a part of it)
static void party_hud(void);  // (s57 party HUD: runtime/sora_party.h, included below: Donald's / Goofy's gauges)
// (s57 party HUD) live "set hudx / hudy <HUD units>": Sora's ring centre from the game image's bottom-right (0 = the
// exe's SAVAGEWIND_SORA_HUD_POS / default 60, 177); looked up by name: exes linked before s57 lack sora_hud_place
static int g_hud_place[2];
// (s92 hud) the enemy gauge's top-left place (sora_hud_enemy_place, looked up by name), resent when a setting changes. HUD units from
// the game image's top-left: left end 24, centre line 22 (GUESS: BotW's first heart at 720p ~ (40, 32) px, 1 unit = 1.607 px there)
static void enemy_place_sync(void) {
    static void (*fn)(float, float, bool);
    static int tried, last_on = -1, last_x = -1, last_y = -1;
    if (!tried) {
        tried = 1, fn = (void (*)(float, float, bool))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_enemy_place");
        LOG("[sora] HUD enemy place: sora_hud_enemy_place %s\n", fn ? "found" : "not in this exe (the enemy gauge stays top-right until the next exe link)");
    }
    if (!fn || (last_on == g_enemy_left && last_x == g_enx && last_y == g_eny)) return;
    last_on = g_enemy_left, last_x = g_enx, last_y = g_eny;
    fn(g_enx > 0 ? (float)g_enx : 24.0f, g_eny > 0 ? (float)g_eny : 22.0f, g_enemy_left != 0);
}
static void hud_place_send(void) {
    static void (*fn)(float, float);
    static int tried;
    if (!tried) {
        tried = 1, fn = (void (*)(float, float))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_place");
        LOG("[sora] HUD place: sora_hud_place %s\n", fn ? "found" : "not in this exe (the ring stays put until the next exe link)");
    }
    if (fn) fn((float)g_hud_place[0], (float)g_hud_place[1]);
}
// (s111 roxhud) the HUD portrait: sora_hud_face(1) = KH2's Roxas face (runtime/sora_hud.c, an EXE function: looked up by name; an exe from
// before it keeps Sora's face until the next relink), resent on change
static int g_mkface = 1;   // (s116) live "set mkface 0" = Sora's face on the HUD while Mickey is out (mk_live_set, sora_mickey.h)
static int g_roxface = 1;  // live "set roxface 0" = Sora's face on the HUD even as Roxas (rox_live_set, sora_roxas.h)
static void hud_face_sync(int who) {  // (s116 mkface: 0 Sora, 1 Roxas, 2 Mickey)
    static void (*fn)(int), (*old)(int);
    static int tried, last = -1;
    if (!tried) {
        tried = 1, fn = (void (*)(int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_face_id");
        if (!fn) old = (void (*)(int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_face");
        LOG("[sora] HUD face: %s\n", fn ? "sora_hud_face_id found (Sora / Roxas / Mickey)"
                                         : old ? "old exe: Roxas only (Mickey keeps Sora's face until the next exe link)"
                                               : "not in this exe (Sora's face stays on the HUD until the next exe link)");
    }
    if (who == last) return;
    last = who;
    if (fn) fn(who);
    else if (old) old(who == 1);
}
// (s176) live "set hurtlog 0/1": a [hurt] line each time Sora's life drops (R2 test: was the hit inside the remote's swing?)
static int g_hurt_log = 1;
static void hurt_log(int life) {
    static int prev = -1;
    if (g_hurt_log && prev > 0 && life >= 0 && life < prev) {
        uint64_t ea[64];
        float ep[64][3];
        int32_t el[64];
        int en = linkpos_enemies(ea, ep, el, 64), bi = -1;
        float best = 1e30f, me[3] = {0, 0, 0};
        linkpos_link_pos(me);
        for (int i = 0; i < en; i++) {
            float dx = ep[i][0] - me[0], dy = ep[i][1] - me[1], dz = ep[i][2] - me[2], d = dx * dx + dy * dy + dz * dz;
            if (el[i] > 0 && d < best) best = d, bi = i;
        }
        LOG("[hurt] Sora's life %d -> %d (-%d quarter hearts), frame %llu, t %llu ms, nearest enemy %s%.1f u (actor %llx)\n", prev, life,
            prev - life, (unsigned long long)vk_frame_counter(), (unsigned long long)GetTickCount64(), bi < 0 ? "none " : "",
            bi < 0 ? 0.0f : sqrtf(best), bi < 0 ? 0ull : (unsigned long long)ea[bi]);
    }
    prev = life;
}
static void hud_update(void) {
    enemy_place_sync();  // (s92 hud)
    int life = linkpos_link_life();
    hurt_log(life);
    if (life <= 0) { sora_hud_off(); sora_sfx_loop("danger", false); return; }
    if (life > g_hud_life_max) g_hud_life_max = life;
    // (s57: the max seen since the last hot reload made Cure +2 at 6 / 12; BotW's own max hearts, quarter hearts)
    static unsigned gdt_n;
    if (SLX.gdt_s32 && gdt_n++ % 30 == 0) {
        int32_t hv = -1;
        if (SLX.gdt_s32("MaxHartValue", &hv) && hv > 0 && hv < 200 && hv > g_hud_life_max) g_hud_life_max = hv;
    }
    float mp = mp_charging() ? -(MP.charge_total - MP.charge) / MP.charge_total * MP.mp_max : MP.mp;  // (< 0: pink, progress)
    float hud_hp = (float)life, hud_hpm = (float)g_hud_life_max, hud_mpm = MP.mp_max;
    mk_hud(&hud_hp, &hud_hpm, &mp, &hud_mpm);  // (s110m: Mickey's 60 HP / 100 MP while he is out)
    // (s111 roxcmd: KH2's Roxas has no MP: the exe draws no MP bar at mp_max 0; review: only the HUD call - g_hud_last below
    // keeps the real MP so the camp's status rows do not read "MP 0 / max" for Roxas)
    sora_hud_set(hud_hp, hud_hpm, ROXCMD ? 0 : mp, ROXCMD ? 0 : hud_mpm);
    hud_face_sync(IN_ROXAS && g_roxface ? 1 : IN_MICKEY && g_mkface ? 2 : 0);  // (s111 roxhud / s116 mkface: the guest's KH2 portrait while he is the body)
    g_hud_last[0] = (float)life, g_hud_last[1] = (float)g_hud_life_max, g_hud_last[2] = mp, g_hud_last[3] = MP.mp_max;
    AB.hp = (int)life, AB.hp_max = (int)g_hud_life_max;  // (s82 Experience Boost)
    party_hud();  // (s57 party HUD: Donald's / Goofy's plates above the ring)
    dg_hud();     // (s67 drive: KH2's Drive gauge)
    // the command menu (KH2 shows it all through play): top page states, the Magic page, the shortcut page
    SoraHudMenu m = {.on = true, .shortcut = M.shortcut, .cursor = M.cursor, .sub = M.sub, .sub_cursor = M.sub_cursor};
    bool magic_ok = !mp_charging() && MP.mp > 0 && !fm_no_magic();
    {  // (s111 roxcmd) Roxas: the exe draws only the Attack + Items rows (sora_hud_menu_rows); an exe without it keeps the greyed Magic / Drive rows
        static void (*rows)(int);
        static int tried;
        static int last = -1;
        if (!tried) tried = 1, rows = (void (*)(int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_rows");
        int mask = ROXCMD ? 0x5 : smn_page2() ? (smn_slate_row() ? 0xF : 0xB) : 0xF;  // (s137 user: Slate row on page 2)  // (s137 smenu: page 2 = Attack / Summon / Limit, no Party)
        if (mask != last) {
            if (rows) rows(mask), LOG("[roxcmd] command menu rows mask 0x%X (%s)\n", mask, mask == 0xF ? "all four" : "Attack + Items");
            else LOG("[roxcmd] exe lacks sora_hud_menu_rows: Magic / Drive rows stay (greyed) until the next exe link\n");
        }
        last = mask;
    }
    static int (*s137pad)(int, const int8_t *, const int8_t *, const uint8_t *);
    {   // (s137 smenu) the FPS Mode pane (Firecracker / End / Balls) rides the same exe export as Mickey's pane
        static int tried;
        if (!tried) tried = 1, s137pad = (int (*)(int, const int8_t *, const int8_t *, const uint8_t *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_pad");
    }
    bool s137pane = s137pad && !MKCMD && smn_pane(s137pad);
    if (!s137pane)
    {  // (s119 mkmenu) Mickey: KH2's own 3-row pane (Jump / Pearl / Attack); an exe without sora_hud_menu_pad keeps the old menu (logged once)
        static int (*pad)(int, const int8_t *, const int8_t *, const uint8_t *);
        static int tried, last = -1;
        if (!tried) {
            tried = 1, pad = (int (*)(int, const int8_t *, const int8_t *, const uint8_t *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_pad");
            if (!pad) LOG("[mkmenu] exe lacks sora_hud_menu_pad: Mickey keeps Sora's command menu until the next exe link\n");
        }
        if (pad && MKCMD) {
            const int8_t lab[3] = {28, 29, 0};  // Jump, Pearl, Attack (cmd.bin labels)
            // (review) the buttons by position (header: bottom B = jump = Cross, right A = attack = Circle, left Y = Square = Pearl):
            // Jump = Cross / Xbox A, Pearl = Square / Xbox X, Attack = Circle / Xbox B (cmd.bin 30 Circle 31 Square 32 Cross 33 B 34 X 35 A)
            const int8_t btn[3] = {(int8_t)(g_mkpad == 2 ? 35 : 32), (int8_t)(g_mkpad == 2 ? 34 : 31), (int8_t)(g_mkpad == 2 ? 33 : 30)};
            const uint8_t st[3] = {0, (uint8_t)(mp < (float)mk_pearl_cost() ? 2 : 0), 0};  // Pearl: state 2 (orange) below its 18 MP
            g_mkpane = pad(3, lab, btn, st) != 0;  // (review: 0 = this exe draws the normal menu (old cmd.bin): menu_tick keeps Sora's input)
            int on = g_mkpane ? 1 : 2;
            if (last != on) LOG(on == 1 ? "[mkmenu] Mickey command pane on (Jump / Pearl / Attack, %s icons)\n" : "[mkmenu] Mickey command pane not drawn by the exe (cmd.bin without labels 28..35, or not loaded yet): Sora's menu stays (%s icons)\n", g_mkpad == 2 ? "Xbox" : "PlayStation");
            last = on;
        } else {
            g_mkpane = false;
            if (pad && last > 0) pad(0, NULL, NULL, NULL), LOG("[mkmenu] Mickey command pane off\n");
            if (last > 0) last = 0;
        }
    }
    m.top_state[0] = 0, m.top_state[1] = magic_ok ? 0 : 5, m.top_state[2] = (uint8_t)meal_top_state();  // (s93 meals: greyed unless live "meals" and a dish is in the pouch)
    m.top_state[3] = (uint8_t)fm_top_state();  // (s68: Drive / Revert)
    if (IN_LIMIT) m.top_state[1] = (uint8_t)lm_top_state();  // (s89: slot 1 = "Limit" in Limit Form)
    if (!smn_page2()) fm_hud_menu();  // (s137 smenu: page 2 sets its own slot labels)
    if (M.sub == 4) meal_hud(&m);  // (s93 meals)
    else if (M.sub == 3) m.sub = 2, m.n_sub = lm_hud_rows(m.sub_label, m.sub_state, m.sub_cost);  // (s89: the Limit page, drawn as the Drive page's look: no title)
    else if (M.sub == 2) {  // (s72: Valor Form label 8, Wisdom Form label 10)
        int rf[FM_NFORMS];
        m.n_sub = fm_rows(rf);  // (s89 kb: the unlocked forms only)
        for (int i = 0; i < m.n_sub; i++)
            m.sub_label[i] = (uint8_t)fm_label(rf[i]), m.sub_state[i] = (uint8_t)fm_state(rf[i]), m.sub_cost[i] = (int16_t)fm_cost(rf[i]);
    } else {
        int spl[NSPELL];  // (s133 magic) only the owned spells are rows
        m.n_sub = spell_list(spl);
        for (int i = 0; i < m.n_sub; i++)
            m.sub_label[i] = (uint8_t)spell_label(spl[i]), m.sub_state[i] = (uint8_t)spell_state(spl[i]), m.sub_cost[i] = (int16_t)KH_SPELLS[spl[i]].cost;
    }
    sk_hud(&m);  // (s93 b7-runes: the Slate row / page, after the page rows above)
    pl_hud(&m);  // (s133 limits: the party Limits list / the Triangle prompt)
    smn_hud(&m);  // (s137 smenu: page 2 labels / states, the Summon list)
    for (int i = 0; i < 4; i++) {
        int sp = SC.sc[i];
        m.sc_label[i] = sp >= 0 ? spell_label(sp) : -1, m.sc_state[i] = sp >= 0 ? (uint8_t)spell_state(sp) : 5;
    }
    sora_hud_menu(&m);
    sora_sfx_loop("danger", life <= g_hud_life_max * 0.25f);  // (KH2's low-HP alarm, with the gauge's red flash)
}

// (s87c link face) minimap arrow / pick-up prompts read Link's facing, not Sora's: hand him Sora's yaw every driven frame.
static void link_face(void) {
    static int tried;
    static LinkposSetYawFn fn;
    static int last = -1;
    if (!tried) {
        tried = 1;
        fn = (LinkposSetYawFn)(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_set_link_yaw");
    }
    int on = g_linkface && fn ? 1 : 0;
    if (on != last) {
        LOG("[sora] link face: %s\n", on ? "on (Link turns with Sora)" : g_linkface ? "off (not in this exe: relink + restart)" : "off (set linkface 0)");
        last = on;
    }
    if (on) fn(S.face);
}

// (s108 b20-lynel) live "goto": a long teleport holds the body in the air until the far terrain collision has loaded
// (goto_tick, below live_poll). While it holds, every "let go of the servo" below keeps it at zero velocity instead.
static struct {
    bool on;
    float tgt[3];       // where the body is finally put (the ground estimate; the hit replaces y)
    float hold[3];      // the held spot, a few metres above it
    ULONGLONG t0, ok0;  // start / first ms of the current good-ground streak (0 = none)
    uint64_t mv_wf;     // world frame of the last correction (a move lands one frame later: no double moves)
    float lasth;        // last ray hit height
    int nmv;
    ULONGLONG last_log;
    bool timeout;
} GT;
static void drive_off(void) {
    if (GT.on || g_dbm_clip) {  // (s143: No Clip holds zero velocity too)
        linkpos_set_velocity(true, 0, 0);
        linkpos_set_velocity_y(true, 0);
        return;
    }
    linkpos_set_velocity(false, 0, 0);
    linkpos_set_velocity_y(false, 0);
}

// ------------------------------------------------------------------ Link's bone world matrices (ragdoll retarget)
// RAGDOLL_RECON.md 3: Link's body ModelUnit keeps 121 sead::Matrix34f bone world matrices (stride 0x30, rows = basis
// + translation column, BotW units) in one array. Found once by a pointer walk from his gsys::Model's units (no guest
// calls): the first readable pointer, up to 3 hops from a unit, at which 121 rigid matrices start with bone 0 at his
// feet (actor mMtx) and bone 1 (Skl_Root) within 2 u of them. Then a plain read per frame.
#define LINK_BONES 121
#define PLAYER_INFO_PTR (sora_sites()->player_info)
static uint64_t g_rag_bones;  // guest address of Link's bone world matrix array (0: not found)

static struct { uint64_t lo, hi; bool ok; } g_rd[256];
static int g_nrd;
static bool rd_ok(uint64_t a, size_t n) {  // (VirtualQuery is ~2 ms here: regions are cached, readable or not)
    if (a < 0x1000 || a + n < a) return false;
    for (int i = 0; i < g_nrd; i++)
        if (a >= g_rd[i].lo && a < g_rd[i].hi) return g_rd[i].ok && a + n <= g_rd[i].hi;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((const void *)(uintptr_t)a, &mbi, sizeof mbi)) return false;
    bool ok = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
              (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                              PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
    uint64_t lo = (uint64_t)(uintptr_t)mbi.BaseAddress, hi = lo + mbi.RegionSize;
    if (g_nrd < 256) g_rd[g_nrd].lo = lo, g_rd[g_nrd].hi = hi, g_rd[g_nrd].ok = ok, g_nrd++;
    return ok && a + n <= hi;
}

static uint64_t link_actor(void) {
    uint64_t pi = *(const uint64_t *)(uintptr_t)PLAYER_INFO_PTR;
    if (!rd_ok(pi, 0x68)) return 0;
    uint64_t a = *(const uint64_t *)(uintptr_t)(pi + 0x60);
    return rd_ok(a, 0x580) ? a : 0;
}

// (s85f Cure: "cure +4 quarter hearts: life 4" every cast - linkpos_link_heal (exe) caps at the highest life it has
// seen this run, so a save loaded at 1 heart (life 4) never heals past 4. The cap here is g_hud_life_max, which takes
// BotW's own MaxHartValue (12 in that log). The life field is linkpos's ACTOR_LIFE (+0x848) on the same actor; if
// link_actor's life doesn't match linkpos's, the old heal is used.)
static int link_heal(int q) {
    int cur = linkpos_link_life();
    uint64_t a = link_actor();
    if (cur <= 0 || !a || !rd_ok(a + 0x848, 4) || *(const int32_t *)(uintptr_t)(a + 0x848) != cur) {
        int v = linkpos_link_heal(q);
        LOG("[sora] heal %d: linkpos heal (life %d, actor %s): life %d\n", q, cur, a ? "life differs" : "none", v);
        return v;
    }
    int max = g_hud_life_max > cur ? g_hud_life_max : cur, v = cur + q > max ? max : cur + q;
    *(int32_t *)(uintptr_t)(a + 0x848) = v;
    return v;
}

#include "sora_guard.h"  // (s88 b3-guard: KH2 Guard on Square, stick neutral; S88_HELPER_GUARD.md)
// (s133 review) Sora not driven (climb / swim / cutscene / death / load): end Magnet + Reflect, give the enemies' controllers
// and the guard flags back (mg_tick only runs on driven frames)
static void mg_other(const char *why) {
    if (!MGN.on && !MGR.phase) return;
    bool rf = MGR.phase == 1;
    mg_cancel(why);
    if (rf) guard_release(why);
}
// ------------------------------------------------------------------ Type 0 combo abilities (s106 comboabil2)
// ptya Type 0 entries are never marked by the Type 1/2/3 passes: KH2 fires them from conditions. Ability text (sys.bar,
// VERIFIED by msgdump): Counterguard 0x3998 "Counterattacks nearby enemies with Attack while performing Guard.";
// Retaliating Slash 0x39a2 "When knocked down, quickly regains balance and counterattacks with Attack."; Upper Slash
// 0x1e61 "Knocks a target into the air during a combo with Attack."; Horizontal Slash 0x399e "Unleashes a powerful attack
// that hits a target from left to right during a midair combo with Attack."; Finishing Leap 0x3996 "Unleashes a powerful
// guard-piercing attack at the end of a combo with Attack." The exact code conditions are not traced: Counterguard (press
// during Guard) is the text's own; Retaliating's press window and the three combo ones are GUESS (recon/S106_HELPER_COMBOABIL2.md).
// Live "set t0 0" = off. Each entry plays only through ptya_ability_ok (equipped, base Sora, motion + hits baked).
static int g_t0 = 1;
static const KhPtya *t0_entry(int id) {
    if (!g_t0 || PTYA != kh_ptya) return NULL;
    int i = ptya_index(id);
    return i >= 0 && ptya_ability_ok(&PTYA[i]) ? &PTYA[i] : NULL;
}
// a press while Guard plays (Counterguard, id 39) / while getting up from a knock-back with no ragdoll left (Retaliating, id 40)
static const KhPtya *t0_reaction(void) {
    if (!g_t0 || PTYA != kh_ptya) return NULL;
    if (S.special == SP_GUARD && S.motion == MOT_GUARD_REFLECT) return t0_entry(39);  // (review: only after a real block, not any guard)
    if (S.special == SP_DAMAGE && S.motion >= MOT_AIR_FLINCH_FRONT && S.motion <= MOT_BLOW_BACK + 1 && linkpos_link_ragdoll() <= 0)
        return t0_entry(40);
    return NULL;
}
static float t0_near(const Target *t, int n) {  // surface distance of the nearest target, -1 = none
    float best = -1;
    for (int i = 0; i < n; i++) {
        float d = t[i].hd - t[i].radius;
        if (d < 0) d = 0;
        if (best < 0 || d < best) best = d;
    }
    return best;
}
// GUESS rules, applied to the pick: Upper Slash = the 2nd ground swing, Horizontal Slash = the 2nd air swing (a target within
// the Type 2 reach), Finishing Leap = the plain ground finisher when the nearest target is 120..650 cm away (a leap closes it).
static const KhPtya *t0_override(const KhPtya *e, bool air, const Target *t, int n) {
    if (!e || !g_t0 || PTYA != kh_ptya || berserk_on()) return e;
    float d = t0_near(t, n);
    const KhPtya *o = NULL;
    // (review: only over a plain swing: not jump-to-target / wall entries or another ability's pick)
    bool plain = e->ability == 0 && (air || e->jump <= 0) && e->id != 1;
    if (!plain) return e;
    if (!air && !(e->flag & PTYA_FINISHER) && S.d74 == 2 && d >= 0 && d < 180) o = t0_entry(12);
    else if (air && !(e->flag & PTYA_FINISHER) && S.d74 == 2 && d >= 0 && d < 260) o = t0_entry(38);
    else if (!air && e->id == 3 && d >= 120 && d < 650) o = t0_entry(37);
    if (o) LOG("[combo] Type 0 override: ptya id %d -> %d (motion %d -> %d, hit %d, nearest target %.0f cm) GUESS rule\n", e->id, o->id, e->motion, o->motion, S.d74, d);
    return o ? o : e;
}

// (s85g probe, user: "when the menu pops up showing me the new item i picked up i can move sora around and he will stay
// at whatever ground level he picked the item up until i press through the menu": no pause, state word stays
// 0x18000001, no screen change. From a no-wall triangle press, PR_LEN presents: Link's actor (0..0x1000) and its +0x578
// struct (0..0x400) as u32 words; at the end the words that changed 1..PR_HIST times are logged with their timeline =
// the popup's on / off flags. Live "set probe 0" = off.)
#define PR_LEN 900
#define PR_HIST 6
#define PR_A 1024
#define PR_P 256
static int g_probe_on = 0;  // (off after the s85g run: found screen 0xb)
static struct { uint64_t t0; uint32_t prev[PR_A + PR_P]; uint8_t n[PR_A + PR_P]; uint16_t at[PR_A + PR_P][PR_HIST];
    uint32_t val[PR_A + PR_P][PR_HIST]; bool on; } PR;
static void probe_start(uint64_t present) {
    if (!g_probe_on || PR.on) return;
    memset(&PR, 0, sizeof PR);
    PR.t0 = present, PR.on = true;
    LOG("[sora] probe: start at present %llu (%d presents)\n", (unsigned long long)present, PR_LEN);
}
static void probe_tick(uint64_t present, const float p[3]) {
    if (!PR.on) return;
    uint64_t a = link_actor(), ps = a && rd_ok(a + 0x578, 8) ? *(const uint64_t *)(uintptr_t)(a + 0x578) : 0;
    if (!a || !rd_ok(a, PR_A * 4)) { PR.on = false; LOG("[sora] probe: no actor\n"); return; }
    bool psok = ps && rd_ok(ps, PR_P * 4);
    uint16_t t = (uint16_t)(present - PR.t0);
    for (int i = 0; i < PR_A + PR_P; i++) {
        if (i >= PR_A && !psok) break;
        uint32_t v = i < PR_A ? ((const uint32_t *)(uintptr_t)a)[i] : ((const uint32_t *)(uintptr_t)ps)[i - PR_A];
        if (t == 0) PR.prev[i] = v;
        else if (v != PR.prev[i]) {
            if (PR.n[i] < PR_HIST) PR.at[i][PR.n[i]] = t, PR.val[i][PR.n[i]] = v;
            if (PR.n[i] < 255) PR.n[i]++;
            PR.prev[i] = v;
        }
    }
    if (t % 30 == 0) LOG("[sora] probe t %u: Link %.3f %.3f %.3f state %#x\n", t, p[0], p[1], p[2], linkpos_link_state_word());
    if (t + 1 < PR_LEN) return;
    PR.on = false;
    int k = 0;
    for (int i = 0; i < PR_A + PR_P; i++) {
        if (!PR.n[i] || PR.n[i] > PR_HIST) continue;
        char b[200];
        int m = snprintf(b, sizeof b, "%s+%#x:", i < PR_A ? "actor" : "ps", (i < PR_A ? i : i - PR_A) * 4);
        for (int j = 0; j < PR.n[i]; j++) m += snprintf(b + m, sizeof b - m, " t%u=%#x", PR.at[i][j], PR.val[i][j]);
        LOG("[sora] probe word %s\n", b), k++;
    }
    LOG("[sora] probe: done, %d words changed 1..%d times\n", k, PR_HIST);
}

// s54 (recon/S54_HELPER_WARP_GLOW.md, static): BotW's warp value = actor +0x4f4, set by the event action
// Demo_PlayerWarpEffectSet (Demo005_0 warp-out 0 -> 1 linear over 120 frames; Demo005_1 arrival 1, hold ~80, -> 0
// over 60); the game turns it into Link's dissolve (uk_proc_discard = 1 - v). 0 = normal, -1 = no actor / junk.
static int g_wtsrc = 1, g_wt_hide = 98;  // live "set wtsrc 0" = the timer guess; "set wthide <x100>" (> 100 = never)
static float link_warp_value(void) {
    uint64_t a = link_actor();
    if (!a) return -1;
    float v = *(const float *)(uintptr_t)(a + 0x4f4);
    return v >= 0 && v <= 1 ? v : -1;
}

// Link's water (s48 snap diff, recon/SAVAGE_WIND_RECON.md s48): actor +0x6f4 = water depth at his feet (u; 0 on dry
// land, ~0.3-0.7 wading, ~0.8 swimming), +0x6f0 = the surface y (0 dry), +0x20d4 = the surface y (-10000 dry).
static float link_water_depth(void) {
    uint64_t a = link_actor();
    return a && rd_ok(a + 0x6f4, 4) ? *(const float *)(uintptr_t)(a + 0x6f4) : 0;
}

// (s87c party water) the surface y of the water Link stands in (actor +0x6f0), or -10000 when he is on dry land.
static float link_water_surface(void) {
    uint64_t a = link_actor();
    if (!a || !rd_ok(a + 0x6f0, 4) || link_water_depth() <= 0.02f) return -10000.0f;
    float y = *(const float *)(uintptr_t)(a + 0x6f0);
    return y == y ? y : -10000.0f;
}

static int g_bones_layout;  // 0: sead Matrix34f (rows, translation m[3] m[7] m[11]); 1: g3d 4x3 (translation m[9..11])

static bool rigid(const float *m, int lay) {  // the 3x3's rows (or 4x3's first 3 rows) orthonormal within 3%, no NaN
    int st = lay ? 3 : 4;
    for (int r = 0; r < 3; r++) {
        float l = m[r * st] * m[r * st] + m[r * st + 1] * m[r * st + 1] + m[r * st + 2] * m[r * st + 2];
        if (!(l > 0.94f && l < 1.06f)) return false;
    }
    float d = m[0] * m[st] + m[1] * m[st + 1] + m[2] * m[st + 2];
    return d > -0.03f && d < 0.03f;
}

static void bone_t(const float *m, int lay, float t[3]) {
    if (lay) t[0] = m[9], t[1] = m[10], t[2] = m[11];
    else t[0] = m[3], t[1] = m[7], t[2] = m[11];
}

static bool bones_at_lay(uint64_t a, const float feet[3], int lay) {
    const float *m = (const float *)(uintptr_t)a;
    float t0[3], t1[3], o[3] = {feet[0], feet[1], feet[2]};
    bone_t(m, lay, t0), bone_t(m + 12, lay, t1);
    if (fabsf(t0[0]) + fabsf(t0[1]) + fabsf(t0[2]) < 0.01f) o[0] = o[1] = o[2] = 0;  // (model space: Root at 0)
    float dx = t0[0] - o[0], dy = t0[1] - o[1], dz = t0[2] - o[2];
    if (!(dx * dx + dy * dy + dz * dz < 0.25f)) return false;
    dx = t1[0] - o[0], dy = t1[1] - o[1], dz = t1[2] - o[2];
    if (!(dx * dx + dy * dy + dz * dz < 4.0f) || !(dx * dx + dy * dy + dz * dz > 0.04f)) return false;
    for (int b = 0; b < LINK_BONES; b++)
        if (!rigid(m + b * 12, lay)) return false;
    return true;
}

static bool bones_at(uint64_t a, const float feet[3]) {
    if (a & 3 || !rd_ok(a, LINK_BONES * 48)) return false;
    for (int lay = 0; lay < 2; lay++)
        if (bones_at_lay(a, feet, lay)) { g_bones_layout = lay; return true; }
    return false;
}

static uint64_t bones_walk(uint64_t obj, int depth, const float feet[3], char *path, int plen) {
    if (!rd_ok(obj, 0x800)) return 0;
    for (int off = 0; off < 0x800; off += 8) {
        uint64_t p = *(const uint64_t *)(uintptr_t)(obj + off);
        if (p < 0x7000000000ull || p >= 0x7320000000ull) continue;  // (guest memory only: rt.h GUEST_LO / HI)
        if (bones_at(p, feet)) { snprintf(path + plen, 128 - plen, " +%#x", off); return p; }
        if (depth > 0) {
            int n = snprintf(path + plen, 128 - plen, " +%#x", off);
            uint64_t r = bones_walk(p, depth - 1, feet, path, plen + n);
            if (r) return r;
        }
    }
    return 0;
}

// Fallback: every resident read-write guest page. Strict: a rigid matrix (either layout) within 0.3 u of Link's feet
// followed by a rigid one 0.2-2 u away (Root, Skl_Root). Relaxed (session 23): uniform scale allowed, and either a
// world-space start within 2 u of the feet or a model-space Root at 0 followed by Skl_Root 0.4-1.5 u up; only runs of
// >= 30 such matrices are logged (then skipped past, so one array is one line).
static bool scaled(const float *m, int lay) {  // the 3 basis rows: equal lengths (10%) in 0.3..3, orthogonal
    int st = lay ? 3 : 4;
    float l[3];
    for (int r = 0; r < 3; r++) {
        l[r] = m[r * st] * m[r * st] + m[r * st + 1] * m[r * st + 1] + m[r * st + 2] * m[r * st + 2];
        if (!(l[r] > 0.09f && l[r] < 9.0f)) return false;
    }
    if (!(fabsf(l[0] - l[1]) < 0.2f * l[0] && fabsf(l[0] - l[2]) < 0.2f * l[0])) return false;
    float d = m[0] * m[st] + m[1] * m[st + 1] + m[2] * m[st + 2], t[3];
    bone_t(m, lay, t);
    return fabsf(d) < 0.05f * l[0] && isfinite(t[0]) && isfinite(t[1]) && isfinite(t[2]);
}

#define RAG_HITS 24
static uint64_t g_rag_hit[RAG_HITS];
static int g_rag_hit_lay[RAG_HITS], g_nrag_hit;

static void rag_scan(const float feet[3], bool relaxed) {
    int found = 0;
    uint64_t a = 0x7000000000ull, t0 = GetTickCount64(), skip = 0;
    static PSAPI_WORKING_SET_EX_INFORMATION ws[1024];
    while (a < 0x7320000000ull && found < 24) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((const void *)(uintptr_t)a, &mbi, sizeof mbi)) break;
        uint64_t base = (uint64_t)(uintptr_t)mbi.BaseAddress, end = base + mbi.RegionSize;
        if (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE)) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)))
            for (uint64_t ch = base; ch < end && found < 24; ch += 1024 * 0x1000) {
                size_t np = 0;
                for (uint64_t pg = ch; pg < end && np < 1024; pg += 0x1000) ws[np++].VirtualAddress = (PVOID)(uintptr_t)pg;
                if (!K32QueryWorkingSetEx(GetCurrentProcess(), ws, (DWORD)(np * sizeof ws[0]))) continue;
                for (size_t q = 0; q < np && found < 24; q++) {
                    if (!ws[q].VirtualAttributes.Valid) continue;
                    uint64_t pg = (uint64_t)(uintptr_t)ws[q].VirtualAddress;
                    if (pg + 0x2000 > end) continue;  // (a matrix pair may run past the page: keep one spare page)
                    const float *f = (const float *)(uintptr_t)pg;
                    for (int i = 0; i < 0x1000 / 4; i++) {
                        for (int lay = 0; lay < 2; lay++) {
                            const float *m = lay ? f + i - 9 : f + i - 3;
                            if ((const uint8_t *)m < (const uint8_t *)(uintptr_t)base || (uint64_t)(uintptr_t)m < skip) continue;
                            float t[3], u[3];
                            bone_t(m, lay, t);
                            float dx = t[0] - feet[0], dy = t[1] - feet[1], dz = t[2] - feet[2];
                            if (!relaxed) {
                                if (!(dx * dx + dy * dy + dz * dz < 0.09f) || !rigid(m, lay) || !rigid(m + 12, lay)) continue;
                            } else {
                                bool world = dx * dx + dz * dz < 4.0f && dy > -0.5f && dy < 2.5f;
                                bool model = fabsf(t[0]) + fabsf(t[1]) + fabsf(t[2]) < 0.05f;
                                if (!(world || model) || !scaled(m, lay) || !scaled(m + 12, lay)) continue;
                                bone_t(m + 12, lay, u);
                                if (model && !world && !(fabsf(u[0]) < 0.4f && fabsf(u[2]) < 0.4f && u[1] > 0.4f && u[1] < 1.5f))
                                    continue;
                            }
                            bone_t(m + 12, lay, u);
                            float ex = u[0] - t[0], ey = u[1] - t[1], ez = u[2] - t[2], d2 = ex * ex + ey * ey + ez * ez;
                            if (!relaxed && !(d2 > 0.04f && d2 < 4.0f)) continue;
                            int nr = 0;
                            while (nr < 140 && rd_ok((uint64_t)(uintptr_t)(m + nr * 12), 48) &&
                                   (relaxed ? scaled(m + nr * 12, lay) : rigid(m + nr * 12, lay)))
                                nr++;
                            if (relaxed && nr < 30) continue;
                            LOG("[sora] bone scan%s: %llx layout %d, %d in a row; T %.3f %.3f %.3f, next %.3f %.3f %.3f, |row0| %.3f\n",
                                relaxed ? " (relaxed)" : "", (unsigned long long)(uintptr_t)m, lay, nr, t[0], t[1], t[2],
                                u[0], u[1], u[2], sqrtf(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]));
                            if (nr >= LINK_BONES && !g_rag_bones) g_rag_bones = (uint64_t)(uintptr_t)m, g_bones_layout = lay;
                            if (g_nrag_hit < RAG_HITS) g_rag_hit_lay[g_nrag_hit] = lay, g_rag_hit[g_nrag_hit++] = (uint64_t)(uintptr_t)m;
                            found++;
                            if (relaxed) skip = (uint64_t)(uintptr_t)(m + nr * 12);
                        }
                    }
                }
            }
        a = end;
    }
    LOG("[sora] bone scan%s: %d candidates, %llu ms, picked %llx\n", relaxed ? " (relaxed)" : "", found,
        (unsigned long long)(GetTickCount64() - t0), (unsigned long long)g_rag_bones);
}

// Session 23: each hit's neighbourhood (0x600 before .. 0x1e00 after) as floats, standing once and every 5th frame while
// Link ragdolls, to probe/linkhits.txt - a run that follows the tumble is a ragdoll body / bone array.
#define RAG_DUMP_LO 0x600
#define RAG_DUMP_N 0x2400
static void rag_hits_dump(int rag, uint64_t present, const float feet[3]) {
    static FILE *f;
    static int standing;
    if (!g_nrag_hit || (rag <= 0 && standing) || (rag > 0 && present % 5)) return;
    if (!f) f = fopen(sora_path("D:/SoraBotW/recon/probe/linkhits.txt"), "a");
    if (!f) return;
    standing = 1;
    fprintf(f, "frame %llu rag %d feet %.4f %.4f %.4f\n", (unsigned long long)present, rag, feet[0], feet[1], feet[2]);
    for (int h = 0; h < g_nrag_hit; h++) {
        uint64_t a = g_rag_hit[h] - RAG_DUMP_LO;
        if (!rd_ok(a, RAG_DUMP_N)) continue;
        const float *v = (const float *)(uintptr_t)a;
        fprintf(f, "hit %d %llx lay %d", h, (unsigned long long)g_rag_hit[h], g_rag_hit_lay[h]);
        for (int k = 0; k < RAG_DUMP_N / 4; k++) fprintf(f, " %.4f", v[k]);
        fprintf(f, "\n");
    }
    fflush(f);
}

static void rag_find(void) {
    uint64_t actor = link_actor();
    if (!actor) { LOG("[sora] Link's bones: no actor\n"); return; }
    uint64_t model = *(const uint64_t *)(uintptr_t)(actor + 0x4e0);
    if (!rd_ok(model, 0x48)) return;
    int n = *(const int32_t *)(uintptr_t)(model + 0x38);
    uint64_t arr = *(const uint64_t *)(uintptr_t)(model + 0x40);
    if (n <= 0 || n > 64 || !rd_ok(arr, 8 * (size_t)n)) { LOG("[sora] Link's bones: model %llx units %d\n", (unsigned long long)model, n); return; }
    const float *mt = (const float *)(uintptr_t)(actor + 0x398);
    float feet[3] = {mt[3], mt[7], mt[11]};
    // LINK_BONES_RECON.md (1.8.2 code): unit +0x198 = the g3d SkeletonObj world array (= [[unit+0x118]+0x20]), one
    // 0x40 block per bone: rows 0-2 = bone x/y/z axes (scale baked in), floats 12-14 = translation, world units.
    uint64_t unit0 = rd_ok(arr, 8) ? *(const uint64_t *)(uintptr_t)arr : 0;
    for (int hop = 0; hop < 2 && rd_ok(unit0, 0x1a0); hop++, unit0 = *(const uint64_t *)(uintptr_t)unit0) {  // (FAINT_LINK: unit may be one hop deeper)
        uint64_t skel = *(const uint64_t *)(uintptr_t)(unit0 + 0x118), w = *(const uint64_t *)(uintptr_t)(unit0 + 0x198);
        uint64_t res = rd_ok(skel, 0x28) ? *(const uint64_t *)(uintptr_t)skel : 0;
        int nb = rd_ok(res, 0x40) ? *(const uint16_t *)(uintptr_t)(res + 0x3c) : -1;
        if (nb == LINK_BONES && rd_ok(w, LINK_BONES * 0x40)) {
            const float *m = (const float *)(uintptr_t)w;
            LOG("[sora] Link's bones (unit+0x198): %llx, skel %llx, %d bones; feet %.3f %.3f %.3f, Root %.3f %.3f %.3f, "
                "Skl_Root +%.3f, Head +%.3f, [skel+0x20] %llx\n", (unsigned long long)w, (unsigned long long)skel, nb, feet[0],
                feet[1], feet[2], m[12], m[13], m[14], m[16 + 13] - feet[1], m[53 * 16 + 13] - feet[1],
                (unsigned long long)*(const uint64_t *)(uintptr_t)(skel + 0x20));
            g_rag_bones = w, g_bones_layout = 2;
            return;
        }
        LOG("[sora] Link's bones: unit %llx skel %llx bones %d world %llx (chain check failed)\n", (unsigned long long)unit0,
            (unsigned long long)skel, nb, (unsigned long long)w);
    }
    for (int depth = 0; depth < 3; depth++)  // (shortest path first)
        for (int i = 0; i < n; i++) {
            uint64_t info = *(const uint64_t *)(uintptr_t)(arr + 8 * i);
            if (!rd_ok(info, 8)) continue;
            uint64_t unit = *(const uint64_t *)(uintptr_t)info;
            char path[128];
            path[0] = 0;
            uint64_t a = bones_walk(unit, depth, feet, path, 0);
            if (a) {
                g_rag_bones = a;
                const float *m = (const float *)(uintptr_t)a;
                LOG("[sora] Link's bones: unit %d of %d (%llx)%s -> %llx; Root %.3f %.3f %.3f, Skl_Root %.3f %.3f %.3f\n", i,
                    n, (unsigned long long)unit, path, (unsigned long long)a, m[3], m[7], m[11], m[15], m[19], m[23]);
                return;
            }
        }
    LOG("[sora] Link's bones: not found in %d units (feet %.3f %.3f %.3f, %d regions)\n", n, feet[0], feet[1], feet[2], g_nrd);
    rag_scan(feet, false);
    if (!g_rag_bones) rag_scan(feet, true);
}

// s88 ride/wind: linkpos.c's ext carry (a platform's / fan's motion the servo used to overwrite); needs the s88 exe
static int g_ride_live = 1, g_wind_live = 1;  // (s113 goto) the user's ride / wind setting (the exe's default 1 / 1)
typedef void (*RideFn)(int, int);
static RideFn ride_fn(void) {
    static RideFn fn;
    static int tried;
    if (!tried) tried = 1, fn = (RideFn)(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_set_ride");
    return fn;
}
static void ride_wind_set(bool ride, bool on) {
    RideFn fn = ride_fn();
    if (ride) g_ride_live = on; else g_wind_live = on;
    if (fn) ride ? fn(on, -1) : fn(-1, on);
    LOG("[sora] %s %s%s\n", ride ? "ride" : "wind", on ? "on" : "off", fn ? "" : " (NOT in this exe: needs the s88 relink)");
}
// (s113 goto hang, Keh Namut: the hold drifted 4.9 -> 135 m in 20 s = the ride carry floated the held body across the sky)
// the carry is off while a goto holds, back to the user's setting when it ends
static void goto_carry(bool hold) {
    RideFn fn = ride_fn();
    if (fn) hold ? fn(0, 0) : fn(g_ride_live, g_wind_live);
}
// (s88 review) true while linkpos adds an updraft (ext y > 0.5 u/s): the glide / fall landing tests below would read
// the rise as "the sink was stopped" and land him mid-air. Old exe (no export): never.
static bool ride_updraft(void) {
    typedef int (*Fn)(float *);
    static Fn fn;
    static int tried;
    if (!tried) tried = 1, fn = (Fn)(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_ext_get");
    float e[3];
    if (!fn || !fn(e) || e[1] <= 0.5f) return false;
    static uint64_t n;
    if (n++ % 30 == 0) LOG("[sora] updraft %.2f u/s (ext): no landing test this frame (#%llu)\n", e[1], (unsigned long long)n);
    return true;
}

// ------------------------------------------------------------------ the ragdoll retarget (session 24)
// While Link ragdolls (linkpos_link_ragdoll 1 rolling / 2 dead), Sora takes his pose: the BLOW motion's baked pose is
// the base (twists, fingers, everything unmapped), then sora_ragmap.h top-down: the pelvis (Sora's root, everything)
// gets Link's pelvis frame and is put at his Skl_Root, the chest (67's subtree) his chest frame, every limb segment
// swings (shortest arc) onto Link's segment direction, children carried along. Link's joints = the translations of
// his bone world matrices ([unit+0x198], LINK_BONES_RECON.md), taken into Sora's model space the way
// sora_draw_world places it back (feet = Link's matrix, RotY(face), KH cm). The head takes Link's Head frame whole
// (Sora's head mesh bone 140 and Link's Head share bind axes: x up, y forward, z left). After the ragdoll Sora keeps
// copying Link through BotW's stand-up, then the last retargeted pose blends into idle over RAG_BLEND KH frames.
//   live: set ragofs <cm>   draw Sora that far to the camera's right of Link (side-by-side screenshots; 0 = on him)
//         set rag <0|1>     the retarget off (the old BLOW motion) / on
//         set head <0|1>    the head turn off (the neck's swing carries the head, session 24) / on
//         set getup <0|1>   0 (default, user's pick s25): Sora copies Link through BotW's stand-up, then idle;
//                           1: KH2's get-up (BLOW landing) starts as the ragdoll ends, blended from his pose
//                           (rag24a: waiting for Link to stand left Sora looping the BLOW air pose = "floating on his back")
#include "sora_ragmap.h"
#include "sora_ragmap_mickey.h"  // (s118 mkrig) Mickey (P_EX200, 198 + Keyblade 12 on bone 169): sora_ragmap_mickey[] / mk_rag_parent[] / MICKEY_J_*
#include "sora_ragmap_roxas.h"  // (s110 roxpup) Roxas' own bone order: sora_ragmap_roxas[] / rx_rag_parent[] / ROXAS_J_*
int sora_draw_bone_count(void);  // (runtime/sora_draw.c, exported by the exe since its 21:12 relink)
bool sora_draw_slot_pose(int slot, float clock, float *out);
void sora_draw_world(const float *bones, float face, float scale);
#define RAG_BLEND 16.0f

#define POSE_BUF_BONES 256  // (s68: a Drive Form's bake has more bones than base Sora's 240: Valor 252)
static float g_rpose[POSE_BUF_BONES * 12], g_rlast[POSE_BUF_BONES * 12];
// (s93 fsmooth, S93_HELPER_FSMOOTH.md) The exe's pose draw blends the two baked 60 Hz frames a clock falls between with a
// plain per-element matrix lerp (sora_draw.c pose_sample / skin_publish). For a bone that turns tens of degrees per
// frame (Final's whirls: up to ~120 deg / frame in E303, ~40 in E301) the lerp midpoint is a SHRUNK, sheared bone (scale
// cos(theta/2): 0.87 at 60 deg), so at every fractional clock (any game frame rate that is not exactly 60: ratio jitter,
// 47 fps dips, 90 / 120 / 144 Hz) the limbs / blades pop in size and the swing reads as a lower frame rate. Here the
// DLL samples frame f and f + 1 itself (sora_draw_slot_pose at whole frames = exact bake rows) and blends per bone:
// rows' lengths lerped, the rotation slerped, the translation lerped, then sora_draw_world. Live "set fsmooth 0" = the
// exe's lerp (old), 1 = this (default). Falls back to the old draw for a skewed / mirrored bone, a tiny angle, an
// unknown slot, a bone count over POSE_BUF_BONES. No data change, no exe change.
static int g_fsmooth = 1;
static int g_animdiag;  // (s113 anim60) live "set animdiag 1": log the driver call gaps + feet cadence (defined with animdiag_pos)
static float g_sm0[POSE_BUF_BONES * 12], g_sm1[POSE_BUF_BONES * 12], g_sm2[POSE_BUF_BONES * 12];
static void sm_lerp12(const float *a, const float *b, float u, float *o) {
    for (int i = 0; i < 12; i++) o[i] = a[i] + (b[i] - a[i]) * u;
}
static bool sm_rows(const float *m, float len[3], float n[9]) {  // unit rows of the 3x3; false: degenerate, skewed or mirrored
    for (int r = 0; r < 3; r++) {
        len[r] = sqrtf(m[r * 3] * m[r * 3] + m[r * 3 + 1] * m[r * 3 + 1] + m[r * 3 + 2] * m[r * 3 + 2]);
        if (!(len[r] > 1e-5f)) return false;
        for (int j = 0; j < 3; j++) n[r * 3 + j] = m[r * 3 + j] / len[r];
    }
    float d01 = n[0] * n[3] + n[1] * n[4] + n[2] * n[5], d02 = n[0] * n[6] + n[1] * n[7] + n[2] * n[8], d12 = n[3] * n[6] + n[4] * n[7] + n[5] * n[8];
    if (fabsf(d01) > 0.05f || fabsf(d02) > 0.05f || fabsf(d12) > 0.05f) return false;
    float det = n[0] * (n[4] * n[8] - n[5] * n[7]) - n[1] * (n[3] * n[8] - n[5] * n[6]) + n[2] * (n[3] * n[7] - n[4] * n[6]);
    return det > 0;
}
static void sm_quat(const float r[9], float q[4]) {  // (w x y z) of the matrix read as a column-convention rotation; the
    float tr = r[0] + r[4] + r[8];                    // rebuild below uses the same reading, so the pair is consistent
    if (tr > 0) {
        float s = sqrtf(tr + 1) * 2;
        q[0] = 0.25f * s, q[1] = (r[7] - r[5]) / s, q[2] = (r[2] - r[6]) / s, q[3] = (r[3] - r[1]) / s;
    } else if (r[0] > r[4] && r[0] > r[8]) {
        float s = sqrtf(1 + r[0] - r[4] - r[8]) * 2;
        q[0] = (r[7] - r[5]) / s, q[1] = 0.25f * s, q[2] = (r[1] + r[3]) / s, q[3] = (r[2] + r[6]) / s;
    } else if (r[4] > r[8]) {
        float s = sqrtf(1 + r[4] - r[0] - r[8]) * 2;
        q[0] = (r[2] - r[6]) / s, q[1] = (r[1] + r[3]) / s, q[2] = 0.25f * s, q[3] = (r[5] + r[7]) / s;
    } else {
        float s = sqrtf(1 + r[8] - r[0] - r[4]) * 2;
        q[0] = (r[3] - r[1]) / s, q[1] = (r[2] + r[6]) / s, q[2] = (r[5] + r[7]) / s, q[3] = 0.25f * s;
    }
}
static void sm_mat(const float q[4], float r[9]) {
    float w = q[0], x = q[1], y = q[2], z = q[3];
    r[0] = 1 - 2 * (y * y + z * z), r[1] = 2 * (x * y - z * w), r[2] = 2 * (x * z + y * w);
    r[3] = 2 * (x * y + z * w), r[4] = 1 - 2 * (x * x + z * z), r[5] = 2 * (y * z - x * w);
    r[6] = 2 * (x * z - y * w), r[7] = 2 * (y * z + x * w), r[8] = 1 - 2 * (x * x + y * y);
}
static void sm_bone(const float *a, const float *b, float u, float *o) {  // one 4x3 row-vector bone matrix, a -> b at u
    float la[3], lb[3], na[9], nb[9], qa[4], qb[4];
    if (!sm_rows(a, la, na) || !sm_rows(b, lb, nb)) { sm_lerp12(a, b, u, o); return; }
    sm_quat(na, qa), sm_quat(nb, qb);
    float d = qa[0] * qb[0] + qa[1] * qb[1] + qa[2] * qb[2] + qa[3] * qb[3];
    if (d < 0) for (int i = 0; i < 4; i++) qb[i] = -qb[i];
    d = fabsf(d);
    if (d > 0.9995f) { sm_lerp12(a, b, u, o); return; }  // under ~3.6 deg: the lerp is within 0.05%
    float th = acosf(d > 1 ? 1 : d), s = sinf(th), wa = sinf((1 - u) * th) / s, wb = sinf(u * th) / s, q[4], r[9];
    for (int i = 0; i < 4; i++) q[i] = qa[i] * wa + qb[i] * wb;
    float ql = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    for (int i = 0; i < 4; i++) q[i] /= ql;
    sm_mat(q, r);
    for (int k = 0; k < 3; k++) {
        float l = la[k] + (lb[k] - la[k]) * u;
        for (int j = 0; j < 3; j++) o[k * 3 + j] = r[k * 3 + j] * l;
    }
    for (int j = 9; j < 12; j++) o[j] = a[j] + (b[j] - a[j]) * u;
}
// = sora_draw_pose (slot, clock, face, scale), the whole-frame poses blended by sm_bone when g_fsmooth.
static void pose_draw_sm(int slot, float clock, float face, float scale) {
    int nb = sora_draw_bone_count();
    float f0 = clock >= 0 ? floorf(clock) : 0, u = clock - f0;
    if (!g_fsmooth || nb <= 0 || nb > POSE_BUF_BONES || !(clock == clock) || clock < 0 || u < 1e-3f || u > 1)
        { sora_draw_pose(slot, clock, face, scale); return; }
    if (!sora_draw_slot_pose(slot, f0, g_sm0) || !sora_draw_slot_pose(slot, f0 + 1, g_sm1)) { sora_draw_pose(slot, clock, face, scale); return; }
    for (int b = 0; b < nb; b++) sm_bone(g_sm0 + b * 12, g_sm1 + b * 12, u, g_sm2 + b * 12);
    sora_draw_world(g_sm2, face, scale);
}

static float g_rblend = -1;  // KH frames into the get-up blend (< 0: none)
static int g_rag_ofs, g_rag_on = 1, g_rag_getup, g_rag_head = 1, g_rag_was;  // (g_rag_was: the last frame was retargeted)
#define SORA_J_HEAD_MESH 140  // (138's axes lean 0.16 rad with the neck; 139 straightens them: 140 = bind x up, y fwd, z left)
// (s110 roxpup) one rig per skeleton the puppet can drive: Sora's (240 bones: 228 + Keyblade 12, the Drive Forms' extra blade rides
// on bone 86 past it) and Roxas' (P_EX110: the same proportions and joint positions in ANOTHER bone order, 229 + Keyblade 12).
// Live "set roxpup 0" = Roxas has no puppet (the s109 behaviour: BotW's own Link shows, hidden in state 0x1).
static int g_roxpup = 1;
// (s118 mkrig) the same for Mickey (P_EX200): live "set mkpup 0" = the s110 rule (no rig: BotW's own Link shows, hidden in state 0x1).
// "set mkysc <percent>" = the pelvis height scale (default -1 = auto: Mickey's root height / Sora's, ~0.39, so his short legs stay on the ground).
static int g_mkpup = 1, g_mkysc = -1;
// (s118 user: "while swimming he is underwater, raise him so half his body is above like Sora / Roxas") in the swim latch his
// pelvis scale is g_mkswim percent of Link's pelvis height instead of the ~0.39 ground ratio (GUESS 100 = Sora's height). "set mkswim <pct>".
static int g_mkswim = 100;
static bool g_rag_swim;  // (s118) the swim latch is on (set in the drive step)
#define RAG_MASKW 256  // (>= POSE_BUF_BONES)
#define RAG_MAXMAP 24
typedef struct {
    const SoraRagBone *map;
    int nmap;
    const int16_t *parent;
    int nbones, nbody;  // all bones (body + Keyblade), body only
    int kb_parent, head_mesh, root, head, ankle_l, palm_l, palm_r, mid_l, mid_r;
    const float (*bind)[3];  // (s118) bind world positions: the root height gives the pelvis height scale
    uint8_t mask[RAG_MAXMAP][RAG_MASKW];  // each map entry's subtree (the bone and its descendants)
    bool ready;
    bool ok;  // (s182) the KH2 skeleton data (data/ragmap/<char>.bin) loaded; false = this body has no ragdoll / puppet
} RagRig;
static RagRig g_rig_sora = {.map = sora_ragmap, .nmap = SORA_RAGMAP_N, .parent = sora_rag_parent, .nbones = SORA_RAG_BONES,
                            .nbody = SORA_RAG_SORA_BONES, .kb_parent = SORA_RAG_KB_PARENT, .head_mesh = SORA_J_HEAD_MESH,
                            .root = SORA_J_ROOT, .head = SORA_J_HEAD, .ankle_l = SORA_J_ANKLE_L, .palm_l = SORA_J_PALM_L,
                            .palm_r = SORA_J_PALM_R, .mid_l = SORA_J_FINGER_MID_L, .mid_r = SORA_J_FINGER_MID_R, .bind = sora_rag_bind_pos};
static RagRig g_rig_rox = {.map = sora_ragmap_roxas, .nmap = ROXAS_RAGMAP_N, .parent = rx_rag_parent, .nbones = ROXAS_RAG_BONES,
                           .nbody = ROXAS_RAG_BODY_BONES, .kb_parent = ROXAS_RAG_KB_PARENT, .head_mesh = ROXAS_J_HEAD_MESH,
                           .root = ROXAS_J_ROOT, .head = ROXAS_J_HEAD, .ankle_l = ROXAS_J_ANKLE_L, .palm_l = ROXAS_J_PALM_L,
                           .palm_r = ROXAS_J_PALM_R, .mid_l = ROXAS_J_FINGER_MID_L, .mid_r = ROXAS_J_FINGER_MID_R, .bind = rx_rag_bind_pos};
static RagRig g_rig_mk = {.map = sora_ragmap_mickey, .nmap = MICKEY_RAGMAP_N, .parent = mk_rag_parent, .nbones = MICKEY_RAG_BONES,
                          .nbody = MICKEY_RAG_BODY_BONES, .kb_parent = MICKEY_RAG_KB_PARENT, .head_mesh = MICKEY_J_HEAD_MESH,
                          .root = MICKEY_J_ROOT, .head = MICKEY_J_HEAD, .ankle_l = MICKEY_J_ANKLE_L, .palm_l = MICKEY_J_PALM_L,
                          .palm_r = MICKEY_J_PALM_R, .mid_l = MICKEY_J_FINGER_MID_L, .mid_r = MICKEY_J_FINGER_MID_R, .bind = mk_rag_bind_pos};
// (s182) the KH2 skeleton numbers (bone parents, bind positions, segment bind dirs) are not in the source: data/ragmap/{sora,roxas,mickey}.bin,
// built at install from the player's own KH2 files. A missing / mismatched file turns off just that body's ragdoll + puppet (Link shows as in BotW).
static void rag_load_all(void) {
    const char *why = "";
    g_rig_sora.ok = sora_ragmap_load(sora_path("D:/SoraBotW/data/ragmap/sora.bin"), SORA_RAG_BONES, sora_rag_parent, sora_rag_bind_pos,
                                     sora_ragmap, SORA_RAGMAP_N, &why);
    if (!g_rig_sora.ok) LOG("[sora] ragmap: data/ragmap/sora.bin unusable (%s): Sora has no ragdoll / puppet\n", why);
    g_rig_rox.ok = sora_ragmap_load(sora_path("D:/SoraBotW/data/ragmap/roxas.bin"), ROXAS_RAG_BONES, rx_rag_parent, rx_rag_bind_pos,
                                    sora_ragmap_roxas, ROXAS_RAGMAP_N, &why);
    if (!g_rig_rox.ok) LOG("[sora] ragmap: data/ragmap/roxas.bin unusable (%s): Roxas has no ragdoll / puppet\n", why);
    g_rig_mk.ok = sora_ragmap_load(sora_path("D:/SoraBotW/data/ragmap/mickey.bin"), MICKEY_RAG_BONES, mk_rag_parent, mk_rag_bind_pos,
                                   sora_ragmap_mickey, MICKEY_RAGMAP_N, &why);
    if (!g_rig_mk.ok) LOG("[sora] ragmap: data/ragmap/mickey.bin unusable (%s): Mickey has no ragdoll / puppet\n", why);
}
#define RIG_CUR (IN_MICKEY ? &g_rig_mk : IN_ROXAS ? &g_rig_rox : &g_rig_sora)
_Static_assert(SORA_RAG_BONES <= RAG_MASKW && ROXAS_RAG_BONES <= RAG_MASKW && MICKEY_RAG_BONES <= RAG_MASKW && RAG_MASKW <= POSE_BUF_BONES && SORA_RAGMAP_N <= RAG_MAXMAP &&
               ROXAS_RAGMAP_N <= RAG_MAXMAP && MICKEY_RAGMAP_N <= RAG_MAXMAP, "rig tables must fit the masks and the pose buffers");  // (s110 review)
static const RagRig *g_rlast_rig;  // (s110 review) the rig g_rlast was retargeted for (rag_draw's get-up blend)

// No Keyblade in Link's own states (user, session 29: not while climbing; KH2 has none swimming or gliding either):
// its bones 228.. (only its own vertices hang on them) shrink to a point at the right palm, so it draws nothing.
static int g_puppet_kb;  // live "set kb 1": keep it
static void keyblade_hide(float *bones) {
    const float *palm = bones + SORA_RAG_KB_PARENT * 12 + 9;
    for (int b = SORA_RAG_SORA_BONES; b < SORA_RAG_BONES; b++) {
        float *m = bones + b * 12;
        memset(m, 0, 9 * sizeof(float));
        m[9] = palm[0], m[10] = palm[1], m[11] = palm[2];
    }
}

static void rmask_init(RagRig *rg) {
    if (rg->ready) return;
    rg->ready = true;
    for (int i = 0; i < rg->nmap; i++)
        for (int k = 0; k < rg->nbones; k++)
            for (int a = k; a >= 0; a = rg->parent[a])
                if (a == rg->map[i].sora) { rg->mask[i][k] = 1; break; }
}

static float v3dot(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static void v3cross(const float *a, const float *b, float *o) {
    o[0] = a[1] * b[2] - a[2] * b[1], o[1] = a[2] * b[0] - a[0] * b[2], o[2] = a[0] * b[1] - a[1] * b[0];
}
static bool v3norm(float *a) {
    float l = sqrtf(v3dot(a, a));
    if (!(l > 1e-6f)) return false;
    a[0] /= l, a[1] /= l, a[2] /= l;
    return true;
}
static const float *rpos(const float *pose, int b) { return pose + b * 12 + 9; }  // (row r3 = the bone's position)

// v' = (v - p) R + p for the subtree's positions, r R for its axes (row vectors, as the bake)
static void rot_subtree(float *pose, const uint8_t *mask, const float R[3][3], const float p[3], int nbones) {
    for (int k = 0; k < nbones; k++) {
        if (!mask[k]) continue;
        for (int r = 0; r < 4; r++) {
            float *v = pose + k * 12 + r * 3, q[3] = {v[0], v[1], v[2]};
            if (r == 3) q[0] -= p[0], q[1] -= p[1], q[2] -= p[2];
            for (int j = 0; j < 3; j++) v[j] = q[0] * R[0][j] + q[1] * R[1][j] + q[2] * R[2][j] + (r == 3 ? p[j] : 0);
        }
    }
}

static void aim_rot(const float a[3], const float b[3], float R[3][3]) {  // a R = b, unit vectors, shortest arc
    float k[3], c = v3dot(a, b);
    v3cross(a, b, k);
    if (c < -0.9999f) {  // (opposite: half a turn about any axis normal to a)
        float t[3] = {1, 0, 0};
        if (fabsf(a[0]) > 0.9f) t[0] = 0, t[1] = 1;
        v3cross(a, t, k), v3norm(k);
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) R[i][j] = 2 * k[i] * k[j] - (i == j);
        return;
    }
    // column form Q = I + [k]x + [k]x^2 / (1 + c) (Q a = b); row form R = Q^T
    float K[3][3] = {{0, -k[2], k[1]}, {k[2], 0, -k[0]}, {-k[1], k[0], 0}}, f = 1 / (1 + c);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float k2 = K[i][0] * K[0][j] + K[i][1] * K[1][j] + K[i][2] * K[2][j];
            R[j][i] = (i == j) + K[i][j] + k2 * f;
        }
}

static bool frame_of(const float up[3], const float left[3], float A[3][3]) {  // rows: up, left (made normal), fwd
    float u[3] = {up[0], up[1], up[2]}, l[3] = {left[0], left[1], left[2]};
    if (!v3norm(u)) return false;
    float d = v3dot(l, u);
    l[0] -= d * u[0], l[1] -= d * u[1], l[2] -= d * u[2];
    if (!v3norm(l)) return false;
    memcpy(A[0], u, sizeof u), memcpy(A[1], l, sizeof l), v3cross(l, u, A[2]);
    return true;
}

static uint64_t link_bones_now(void) {  // [unit+0x198] re-resolved (heap: area loads move it), 0 if the chain breaks
    uint64_t actor = link_actor();
    if (!actor) return 0;
    uint64_t model = *(const uint64_t *)(uintptr_t)(actor + 0x4e0);
    if (!rd_ok(model, 0x48)) return 0;
    uint64_t arr = *(const uint64_t *)(uintptr_t)(model + 0x40);
    uint64_t u = rd_ok(arr, 8) ? *(const uint64_t *)(uintptr_t)arr : 0;
    for (int hop = 0; hop < 2 && rd_ok(u, 0x1a0); hop++, u = *(const uint64_t *)(uintptr_t)u) {
        uint64_t skel = *(const uint64_t *)(uintptr_t)(u + 0x118), w = *(const uint64_t *)(uintptr_t)(u + 0x198);
        uint64_t res = rd_ok(skel, 0x28) ? *(const uint64_t *)(uintptr_t)skel : 0;
        if (rd_ok(res, 0x40) && *(const uint16_t *)(uintptr_t)(res + 0x3c) == LINK_BONES && rd_ok(w, LINK_BONES * 0x40))
            return w;
    }
    return 0;
}

// s54: are Link's bones a real pose? (s28: save loads in state 0x1 gave none) Every translation finite and within
// BONES_SANE_U of his feet, and not all on one point; *maxd = the farthest bone from the feet (u), -1 without bones
#define BONES_SANE_U 3.0f
static bool link_bones_sane(float *maxd) {
    uint64_t w = link_bones_now();
    float lm[12];
    *maxd = -1;
    if (!w || !linkpos_link_mtx(lm)) return false;
    const float *bw = (const float *)(uintptr_t)w;
    float top = 0;
    for (int b = 0; b < LINK_BONES; b++) {
        const float *t = bw + b * 16 + 12;
        if (!isfinite(t[0]) || !isfinite(t[1]) || !isfinite(t[2])) { *maxd = INFINITY; return false; }
        float dx = t[0] - lm[3], dy = t[1] - lm[7], dz = t[2] - lm[11], d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d > top) top = d;
    }
    *maxd = top;
    return top < BONES_SANE_U && top > 0.3f;
}

// (s94 bed) The new game starts with Link lying on the Shrine of Resurrection's bed (pod) and getting up in a Demo; Sora
// used to stand inside it because the state word there reads ground / 0 (not a Link-owned state). Signal = Link's bone
// cloud: a LYING body is flat (vertical extent well below its horizontal extent), a standing one tall. Gated to the bed
// spot (BED_POS, GUESS: recon says Link spawns at -1130 237.4 1914.5) within live "set bedr" metres. While latched Sora
// puppets Link's bones (same retarget as swim / climb) with Link hidden and the pad Link's. Live "set bedpose 0/1/2"
// (0 off, 1 = at the bed spot only [default], 2 = anywhere Link lies flat: GUESS, other cutscene beds), "set bedr <m>",
// "set bedup <presents>" (upright presents before Sora drives again), "set bedlog 0/1".
static int g_bedpose = 1, g_bed_r = 12, g_bed_up = 30, g_bed_log = 1;
static const float BED_POS[3] = {-1130.0f, 237.4f, 1914.5f};
// (s99 b14-bed) The wake-up scene: Link's actor and bones exist from the first world frame (state word 0x18000001 the whole
// lying scene, last.log), so Sora is drawn on his bones from the FIRST frame instead of waiting for the late "flat" bed
// latch. Live "set wake 0/1/2" (0 off [default], 1 = on + recon log, 2 = recon log only), "set wakedx <presents>" (state
// word 0 held this long ends it: the Demo hands over to the event stretch; default 15 = g_ev_zero), "set wakelog <n>"
// (recon line every n presents, default 60), "set wakemax <presents>" (safety timeout, default 4800).
static int g_wake = 0, g_wake_dx = 15, g_wake_log = 60, g_wake_max = 4800;
static bool g_wake_on, g_wake_used;  // g_wake_on: Sora lies (party hidden); g_wake_used: the old late bed latch stays off
#define BED_FLAT 0.6f   // GUESS: lying when vert extent < 0.6 x horizontal extent (a standing / sitting-up body is >= 1)
#define BED_UPRIGHT 1.0f  // GUESS: upright again when vert >= 1.0 x horizontal
#define BED_MIN_EXT 0.3f  // bone cloud smaller than this (units) = junk bones
#define BED_TIMEOUT 7200  // presents: the latch drops by itself (a stuck pose must not hold Sora forever)
static bool bed_extent(float *vert, float *horiz) {
    uint64_t w = link_bones_now();
    *vert = *horiz = 0;
    if (!w) return false;
    const float *bw = (const float *)(uintptr_t)w;
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (int b = 0; b < LINK_BONES; b++) {
        const float *t = bw + b * 16 + 12;
        if (!isfinite(t[0]) || !isfinite(t[1]) || !isfinite(t[2])) return false;
        for (int k = 0; k < 3; k++) lo[k] = fminf(lo[k], t[k]), hi[k] = fmaxf(hi[k], t[k]);
    }
    *vert = hi[1] - lo[1];
    *horiz = fmaxf(hi[0] - lo[0], hi[2] - lo[2]);
    return true;
}

// Sora's pose from Link's into g_rpose; false (nothing to draw) without his bones or the bake
// (s69) bones past base Sora's 240 (a form's second weapon) ride rigidly on bone `parent`: after the retarget moved it
// from `old` to its new rows, bones lo..hi-1 get the same move (rows = x, y, z axes + position, v' = v R; the inverse
// by the general 3x3 so a scaled bone works too)
#define RAG_KB2_PARENT 86
// (s70) keyblade_hide for a form's second blade: bones 240.. collapse onto bone 86's palm (puppet in Valor)
static void keyblade2_hide(float *bones) {
    int nb = sora_draw_bone_count();
    if (IN_GUEST || nb <= SORA_RAG_BONES || nb > POSE_BUF_BONES) return;  // (s110: Roxas' bone 240 is his own Keyblade, not a second blade; s118: a guest has no second blade)
    const float *palm = bones + RAG_KB2_PARENT * 12 + 9;
    for (int b = SORA_RAG_BONES; b < nb; b++) {
        float *m = bones + b * 12;
        memset(m, 0, 9 * sizeof(float));
        m[9] = palm[0], m[10] = palm[1], m[11] = palm[2];
    }
}

// (s110 roxpup) the same for the rig in use: the Keyblade's bones past the body collapse onto its parent bone (Roxas: 229.. on 178)
static void keyblade_hide_rig(float *bones, const RagRig *rg) {
    const float *palm = bones + rg->kb_parent * 12 + 9;
    for (int b = rg->nbody; b < rg->nbones; b++) {
        float *m = bones + b * 12;
        memset(m, 0, 9 * sizeof(float));
        m[9] = palm[0], m[10] = palm[1], m[11] = palm[2];
    }
}

static void rag_follow_bone(float *pose, const float old[12], int parent, int lo, int hi) {
    const float *a = old, *n = pose + parent * 12;
    float det = a[0] * (a[4] * a[8] - a[5] * a[7]) - a[1] * (a[3] * a[8] - a[5] * a[6]) + a[2] * (a[3] * a[7] - a[4] * a[6]);
    if (fabsf(det) < 1e-12f) return;
    float inv[9] = {(a[4] * a[8] - a[5] * a[7]) / det, (a[2] * a[7] - a[1] * a[8]) / det, (a[1] * a[5] - a[2] * a[4]) / det,
                    (a[5] * a[6] - a[3] * a[8]) / det, (a[0] * a[8] - a[2] * a[6]) / det, (a[2] * a[3] - a[0] * a[5]) / det,
                    (a[3] * a[7] - a[4] * a[6]) / det, (a[1] * a[6] - a[0] * a[7]) / det, (a[0] * a[4] - a[1] * a[3]) / det};
    float D[9];  // old^-1 * new (3x3)
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) D[r * 3 + c] = inv[r * 3] * n[c] + inv[r * 3 + 1] * n[3 + c] + inv[r * 3 + 2] * n[6 + c];
    for (int k = lo; k < hi; k++)
        for (int r = 0; r < 4; r++) {
            float *v = pose + k * 12 + r * 3, q[3] = {v[0], v[1], v[2]};
            if (r == 3) q[0] -= a[9], q[1] -= a[10], q[2] -= a[11];
            for (int j = 0; j < 3; j++) v[j] = q[0] * D[j] + q[1] * D[3 + j] + q[2] * D[6 + j] + (r == 3 ? n[9 + j] : 0);
        }
}

// (s170 bonedraw) the retarget itself: Link's joints in model space L[121] (the rig's map reads the 24 NET_BONE_LINK ones), his
// Head axes hu / hl (model space; normalised in place), bent onto `pose` (the base KH2 pose of the rig, in / out, nb bones).
// The same math as ever, split out of rag_retarget so the remote player's avatar (sora_net_avatar.h) runs it on the joints a SNAP
// carries. ysc = the pelvis height scale; head_on = the Head frame step; form = bones past 240 follow bone 86. err / turn = out (log).
static void rag_retarget_core(RagRig *rg, const float (*L)[3], float hu[3], float hl[3], float *pose, int nb, bool form, float ysc,
                              bool head_on, float *err_o, float *turn_o) {
    float hand0[12];
    memcpy(hand0, pose + RAG_KB2_PARENT * 12, sizeof hand0);
    rmask_init(rg);
    float err = 0, turn = 0;
    for (int i = 0; i < rg->nmap; i++) {
        const SoraRagBone *m = &rg->map[i];
        float p[3], R[3][3];
        memcpy(p, rpos(pose, m->sora), sizeof p);
        int lto = m->link_to;
        if (m->role == SORA_RAG_PELVIS) {  // (Skl_Root and Spine_1 can sit on one point: then Spine_2)
            float d[3] = {L[lto][0] - L[m->link][0], L[lto][1] - L[m->link][1], L[lto][2] - L[m->link][2]};
            if (v3dot(d, d) < 9) lto = LINK_J_SPINE_2;
        }
        float su[3], lu[3];
        for (int j = 0; j < 3; j++) su[j] = rpos(pose, m->sora_to)[j] - p[j], lu[j] = L[lto][j] - L[m->link][j];
        if (m->role == SORA_RAG_PELVIS || m->role == SORA_RAG_CHEST) {
            float sl[3], ll[3], A[3][3], B[3][3];
            for (int j = 0; j < 3; j++)
                sl[j] = rpos(pose, m->sora_l)[j] - rpos(pose, m->sora_r)[j], ll[j] = L[m->link_l][j] - L[m->link_r][j];
            if (!frame_of(su, sl, A) || !frame_of(lu, ll, B)) continue;
            for (int a = 0; a < 3; a++)  // R = A^T B
                for (int b = 0; b < 3; b++) R[a][b] = A[0][a] * B[0][b] + A[1][a] * B[1][b] + A[2][a] * B[2][b];
            rot_subtree(pose, rg->mask[i], R, p, rg->nbones);
            if (m->role == SORA_RAG_PELVIS) {  // and Sora's root onto Link's Skl_Root
                // (s118 mkrig) a shorter body keeps his feet on the ground: Link's pelvis HEIGHT is scaled by ysc (this body's root height / Sora's;
                // the caller adds Mickey's mkysc / mkswim); x / z stay Link's.
                float t[3] = {L[m->link][0] - p[0], L[m->link][1] * ysc - p[1], L[m->link][2] - p[2]};
                for (int k = 0; k < rg->nbones; k++)
                    for (int j = 0; j < 3; j++) pose[k * 12 + 9 + j] += t[j];
            }
            continue;
        }
        if (m->role == SORA_RAG_HEAD) {  // Link's Head frame onto Sora's 140 (up = row x, left = row z in both)
            if (!head_on) continue;
            float A[3][3], B[3][3];
            const float *sh = pose + rg->head_mesh * 12;
            if (!frame_of(sh, sh + 6, A) || !frame_of(hu, hl, B)) continue;
            for (int a = 0; a < 3; a++)  // R = A^T B, as the frames above
                for (int b = 0; b < 3; b++) R[a][b] = A[0][a] * B[0][b] + A[1][a] * B[1][b] + A[2][a] * B[2][b];
            rot_subtree(pose, rg->mask[i], R, p, rg->nbones);
            turn = acosf(fmaxf(-1, fminf(1, (R[0][0] + R[1][1] + R[2][2] - 1) / 2)));
            v3norm(hu);
            continue;
        }
        if (!v3norm(su) || !v3norm(lu)) continue;
        aim_rot(su, lu, R);
        rot_subtree(pose, rg->mask[i], R, p, rg->nbones);
        float now[3];
        for (int j = 0; j < 3; j++) now[j] = rpos(pose, m->sora_to)[j] - rpos(pose, m->sora)[j];
        if (v3norm(now)) { float e = acosf(fminf(1, v3dot(now, lu))); if (e > err) err = e; }
    }
    if (form) rag_follow_bone(pose, hand0, RAG_KB2_PARENT, SORA_RAG_BONES, nb);
    if (err_o) *err_o = err;
    if (turn_o) *turn_o = turn;
}

// (s170 bonedraw) the last retarget's Link joints in BotW WORLD units (the 24 NET_BONE_LINK ones + Head axes rows 0 / 2) and the base
// motion it started from, for net_fill (the SNAP's bone block); g_drv_frame = sora_mod_frame's call counter (the stamp)
static uint32_t g_drv_frame;
static struct { bool ok; uint32_t frame; int slot; float clock, j[NET_BONE_J][3], hu[3], hl[3]; } g_rcap;

static bool rag_retarget(int slot, float clock, float face, float scale, bool log) {
    if ((IN_MICKEY && !g_mkpup) || (IN_ROXAS && !g_roxpup)) return false;  // (s118: set mkpup 0 = the s110 rule, Mickey has no puppet (BotW's Link shows); set roxpup 0 = the s109 rule)
    RagRig *rg = RIG_CUR;  // (s110 roxpup: Roxas' own map; Sora and the Drive Forms use Sora's, unchanged)
    if (!rg->ok) return false;  // (s182 no data/ragmap file)
    uint64_t w = link_bones_now();
    float lm[12];
    int nb = sora_draw_bone_count();  // (s69: Valor 252 = base Sora's 240 + its left blade 240..251 on bone 86)
    bool form = !IN_GUEST && nb > SORA_RAG_BONES && nb <= POSE_BUF_BONES && g_fmots_on;
    if (!w || (nb != rg->nbones && !form) || !linkpos_link_mtx(lm) || !sora_draw_slot_pose(slot, clock, g_rpose))
        return false;
    static float lb[LINK_BONES * 16], L[LINK_BONES][3];
    memcpy(lb, (const void *)(uintptr_t)w, sizeof lb);  // (one copy: calcWorld rewrites it in place)
    float s = sinf(face), c = cosf(face), ofs[3] = {0, 0, 0}, cf[2];
    if (g_rag_ofs && linkpos_camera_fwd(cf)) {  // camera right (-fz, fx) in world cm -> model space below
        float ox = -cf[1] * (float)g_rag_ofs, oz = cf[0] * (float)g_rag_ofs;
        ofs[0] = c * ox - s * oz, ofs[2] = s * ox + c * oz;
    }
    for (int b = 0; b < LINK_BONES; b++) {
        float dx = (lb[b * 16 + 12] - lm[3]) / scale, dy = (lb[b * 16 + 13] - lm[7]) / scale, dz = (lb[b * 16 + 14] - lm[11]) / scale;
        L[b][0] = c * dx - s * dz + ofs[0], L[b][1] = dy, L[b][2] = s * dx + c * dz + ofs[2];
    }
    float hu[3] = {0, 0, 0}, hl[3] = {0, 0, 0};
    if (g_rag_head) {  // Link's Head frame (g3d rows, a float4 each: x axis, y axis, z axis, translation): up = row 0, left = row 2
        const float *h = lb + LINK_J_HEAD * 16;
        for (int r = 0; r < 2; r++) {
            const float *a = h + r * 8;
            float *o = r ? hl : hu;
            o[0] = c * a[0] - s * a[2], o[1] = a[1], o[2] = s * a[0] + c * a[2];
        }
    }
    // (s118 mkrig) a shorter body keeps his feet on the ground: Link's pelvis HEIGHT is scaled by this body's root height / Sora's
    // (Sora 1, Roxas 1: his root is 102.6 too); x / z stay Link's. mkysc overrides it (percent).
    float ysc = rg->bind && sora_rag_bind_pos[SORA_J_ROOT][1] > 0 ? rg->bind[rg->root][1] / sora_rag_bind_pos[SORA_J_ROOT][1] : 1;
    if (IN_MICKEY && g_mkysc >= 0) ysc = (float)g_mkysc / 100;
    else if (IN_MICKEY && g_rag_swim) ysc = (float)g_mkswim / 100;  // (s118 mkswim)
    float err, turn;
    rag_retarget_core(rg, (const float (*)[3])L, hu, hl, g_rpose, nb, form, ysc, g_rag_head != 0, &err, &turn);
    if (log) {
        const float *r0 = rpos(g_rpose, rg->root), *hd = rpos(g_rpose, rg->head), *an = rpos(g_rpose, rg->ankle_l);
        LOG("[sora] rag retarget%s: Skl_Root %.0f %.0f %.0f cm (model), Sora root %.0f %.0f %.0f head %.0f %.0f %.0f "
            "ankle L %.0f %.0f %.0f; Link head %.0f %.0f %.0f ankle L %.0f %.0f %.0f; worst limb %.1f deg; head turn "
            "%.1f deg, Link head up %.2f %.2f %.2f\n",
            rg == &g_rig_mk ? " (Mickey)" : rg == &g_rig_rox ? " (Roxas)" : "", L[1][0], L[1][1], L[1][2], r0[0], r0[1], r0[2], hd[0], hd[1], hd[2], an[0], an[1], an[2], L[LINK_J_HEAD][0],
            L[LINK_J_HEAD][1], L[LINK_J_HEAD][2], L[LINK_J_ANKLE_L][0], L[LINK_J_ANKLE_L][1], L[LINK_J_ANKLE_L][2],
            err * 180 / PI, turn * 180 / PI, hu[0], hu[1], hu[2]);
    }
    // (s170 bonedraw) keep what the SNAP's bone block needs: Link's world joints + Head axes (unrotated), the base motion
    for (int i = 0; i < NET_BONE_J; i++) memcpy(g_rcap.j[i], lb + NET_BONE_LINK[i] * 16 + 12, 3 * sizeof(float));
    memcpy(g_rcap.hu, lb + LINK_J_HEAD * 16, 3 * sizeof(float));
    memcpy(g_rcap.hl, lb + LINK_J_HEAD * 16 + 8, 3 * sizeof(float));
    g_rcap.slot = slot, g_rcap.clock = clock, g_rcap.frame = g_drv_frame, g_rcap.ok = true;
    return true;
}

// ------------------------------------------------------------------ (s102 handpose) KH2 hands on Link's lift pose
// The puppet (rag_retarget above) aims every limb at Link's and carries Sora's idle fingers along rigidly, so a rune
// lift / hold (Slate mode: Remote Bomb, cube, Magnesis, Stasis aim ...) showed the idle hand shape turned to wherever the
// minimal swing put it ("open palms", user s100d). Here the finger subtrees (everything under the palm bones 86 / 178) are
// re-posed from a KH2 motion frame: each bone keeps its transform RELATIVE to the source palm, re-based onto the palm the
// retarget just placed, so the arms keep the lift pose. Source (live "set handsrc"): 0 = each hand from its own side, 1 =
// the RIGHT hand mirrored onto both (idle: the Keyblade grip = a closed hand; default), 2 = the LEFT mirrored onto both.
// Mirror: x-flip of the palm-relative transform (handmir -1 auto = decided from the finger bases' palm-local x, 0 = copy
// the same, 1 = flip). Blends in / out over HP_BLEND_MS (time, the game runs 60 fps). Live: "set handpose 0/1/2" (0 off,
// 1 = Slate mode only [default], 2 = every puppet state), handsrc, handmot (motion id, default 0 idle), handfr (frame,
// default 0), handmir, handlog. DLL only.
// (s107 handflip) the held-Slate hand is backwards (user s106c). Palm-local axes of this rig (the [handpose] finger-base log: fingers
// +x, palm normal ~y [GUESS], sideways = z, the two hands' frames are the SAME (copy), only the finger-mid z has opposite signs).
// handsrc 1 copies the RIGHT hand's relative pose onto the left hand UNMIRRORED, i.e. the left hand gets a right-hand shape (the thumb
// and palm on the wrong side = "backwards"). Live "set handflip n":
//   0 = old (left hand = right hand's shape copied)
//   1 = [default] MIRROR left/right: the copied hand is reflected across the palm plane (z) = a true left hand (thumb side swapped,
//       fingers still curl the same way). Only the hand that is copied (the left, with handsrc 1) changes.
//   2 = TURN OVER both hands: rotated 180 degrees about the finger axis (palm up <-> palm down)
//   3 = turn over the RIGHT hand only      4 = turn over the LEFT hand only
// The Slate is attached by BotW to Link's hand bone, not to Sora's fingers: this does not move the Slate. DLL only.
static int g_handflip = 1;
static int g_handpose = 1, g_handsrc = 1, g_handmot = 0, g_handfr = 0, g_handmir = -1, g_handlog = 1;
#define HP_BLEND_MS 150.0f
static float g_hpsrc[POSE_BUF_BONES * 12];
static float g_hw;            // blend weight 0..1
static ULONGLONG g_hw_t;      // tick of the last update
static bool g_hp_on;          // last logged edge
static void hp_m4(const float *p, float m[4][4]) {  // 4x3 bone rows -> 4x4 (row vectors)
    for (int r = 0; r < 4; r++) m[r][0] = p[r * 3], m[r][1] = p[r * 3 + 1], m[r][2] = p[r * 3 + 2], m[r][3] = r == 3;
}
static void hp_mul(const float a[4][4], const float b[4][4], float o[4][4]) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) o[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
}
static bool hp_inv(const float a[4][4], float o[4][4]) {  // affine (last column 0 0 0 1) inverse
    float m00 = a[0][0], m01 = a[0][1], m02 = a[0][2], m10 = a[1][0], m11 = a[1][1], m12 = a[1][2], m20 = a[2][0], m21 = a[2][1],
          m22 = a[2][2];
    float det = m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20) + m02 * (m10 * m21 - m11 * m20);
    if (!(fabsf(det) > 1e-9f)) return false;
    float id = 1 / det, r[3][3] = {
        {(m11 * m22 - m12 * m21) * id, (m02 * m21 - m01 * m22) * id, (m01 * m12 - m02 * m11) * id},
        {(m12 * m20 - m10 * m22) * id, (m00 * m22 - m02 * m20) * id, (m02 * m10 - m00 * m12) * id},
        {(m10 * m21 - m11 * m20) * id, (m01 * m20 - m00 * m21) * id, (m00 * m11 - m01 * m10) * id}};
    memset(o, 0, sizeof(float) * 16);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) o[i][j] = r[i][j];
    for (int j = 0; j < 3; j++) o[3][j] = -(a[3][0] * r[0][j] + a[3][1] * r[1][j] + a[3][2] * r[2][j]);
    o[3][3] = 1;
    return true;
}
static bool hp_desc(int k, int palm) {  // k under (not equal to) palm
    const int16_t *par = RIG_CUR->parent;  // (s110 roxpup: the body in use)
    for (int a = par[k]; a >= 0; a = par[a])
        if (a == palm) return true;
    return false;
}
// one hand: target palm tp (current pose in g_rpose), source palm sp in g_hpsrc, mirror x; w = blend weight
static void hp_hand(int tp, int sp, int mirax, bool turn, float w) {  // mirax: -1 none, else the palm-local axis reflected; turn: 180 deg about x
    int d = sp - tp;
    float sPi[4][4], tP[4][4], t[4][4];
    hp_m4(g_hpsrc + sp * 12, t);
    if (!hp_inv(t, sPi)) return;
    hp_m4(g_rpose + tp * 12, tP);
    const RagRig *rg = RIG_CUR;
    for (int k = tp + 1; k < rg->nbones; k++) {
        if (!hp_desc(k, tp)) continue;
        int ks = k + d;
        if (ks < 0 || ks >= rg->nbones) continue;
        if (rg->parent[k] != tp && rg->parent[ks] != rg->parent[k] + d) continue;  // (a different chain: leave it)
        float sk[4][4], rel[4][4], nw[4][4], o[12];
        hp_m4(g_hpsrc + ks * 12, sk);
        hp_mul(sk, sPi, rel);
        if (mirax >= 0)
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                    if ((i == mirax) != (j == mirax)) rel[i][j] = -rel[i][j];
        if (turn)  // rel * Rx(180): the columns y and z change sign (palm-local coordinates turned over the finger axis)
            for (int i = 0; i < 4; i++) rel[i][1] = -rel[i][1], rel[i][2] = -rel[i][2];
        hp_mul(rel, tP, nw);
        for (int r = 0; r < 4; r++) o[r * 3] = nw[r][0], o[r * 3 + 1] = nw[r][1], o[r * 3 + 2] = nw[r][2];
        sm_bone(g_rpose + k * 12, o, w, g_rpose + k * 12);
    }
}
// after rag_retarget in the puppet block; active = the state wants KH2 hands. Frames with no puppet reset it at the call site.
static void hp_apply(bool active) {
    ULONGLONG now = GetTickCount64();
    float dt = g_hw_t ? (float)(now - g_hw_t) : 0;
    g_hw_t = now;
    if (dt > 100) dt = 100;
    if (g_handpose && active) g_hw = fminf(1, g_hw + dt / HP_BLEND_MS);
    else g_hw = fmaxf(0, g_hw - dt / HP_BLEND_MS);
    bool on = g_hw > 0;
    if (on != g_hp_on) LOG("[handpose] %s\n", on ? "on" : "off"), g_hp_on = on;
    if (!on) return;
    const Mot *m = mot_of(g_handmot);
    int nb = sora_draw_bone_count();
    const RagRig *rg = RIG_CUR;  // (s110 roxpup: Roxas' palms / finger bases)
    const int pl = rg->palm_l, pr = rg->palm_r, ml = rg->mid_l, mr = rg->mid_r;
    if (!m || !rg->ok || nb < rg->nbones || nb > POSE_BUF_BONES || !sora_draw_slot_pose(m->slot, (float)g_handfr, g_hpsrc)) {
        static bool said;
        if (!said) said = true, LOG("[handpose] no source pose (motion %d frame %d, bones %d)\n", g_handmot, g_handfr, nb);
        return;
    }
    float w = g_hw * g_hw * (3 - 2 * g_hw);
    int mir = g_handmir;
    if (mir < 0) {  // finger-base x in palm-local axes: opposite signs on the two sides = x-mirrored rig
        float a[4][4], ai[4][4], b[4][4], bi[4][4], ra[4][4], rb[4][4];
        hp_m4(g_hpsrc + pl * 12, a), hp_m4(g_hpsrc + pr * 12, b);
        hp_m4(g_hpsrc + ml * 12, ra), hp_m4(g_hpsrc + mr * 12, rb);
        mir = 1;
        if (hp_inv(a, ai) && hp_inv(b, bi)) {
            float la[4][4], lb[4][4];
            hp_mul(ra, ai, la), hp_mul(rb, bi, lb);
            mir = fabsf(la[3][0] + lb[3][0]) < fabsf(la[3][0] - lb[3][0]);
            static bool said;
            if (!said && g_handlog)
                said = true, LOG("[handpose] finger base in palm axes: L (%.1f %.1f %.1f) R (%.1f %.1f %.1f) -> %s\n", la[3][0],
                                 la[3][1], la[3][2], lb[3][0], lb[3][1], lb[3][2], mir ? "mirror" : "copy");
        }
    }
    int fl = mir != 0 ? 0 : -1;  // (the old mirror: across x)
    int zm = -1;  // (s107 handflip 1) sideways mirror: the finger-mid z of the two palms has opposite signs
    if (g_handflip == 1) {
        float a[4][4], ai[4][4], b[4][4], bi[4][4], ra[4][4], rb[4][4], la[4][4], lb[4][4];
        hp_m4(g_hpsrc + pl * 12, a), hp_m4(g_hpsrc + pr * 12, b);
        hp_m4(g_hpsrc + ml * 12, ra), hp_m4(g_hpsrc + mr * 12, rb);
        zm = 2;
        if (hp_inv(a, ai) && hp_inv(b, bi)) hp_mul(ra, ai, la), hp_mul(rb, bi, lb), zm = la[3][2] * lb[3][2] < 0 ? 2 : -1;
    }
    bool tl = g_handflip == 2 || g_handflip == 4, tr = g_handflip == 2 || g_handflip == 3;
    int cm = g_handflip == 1 ? zm : fl;  // the mirror used for a copied hand
    if (g_handsrc == 0) hp_hand(pl, pl, -1, tl, w), hp_hand(pr, pr, -1, tr, w);
    else if (g_handsrc == 2) hp_hand(pl, pl, -1, tl, w), hp_hand(pr, pl, cm, tr, w);
    else hp_hand(pl, pr, cm, tl, w), hp_hand(pr, pr, -1, tr, w);
}

// a blended bone's axes: Gram-Schmidt, the rows rescaled to len[] (s52: NOT the lerped rows' own lengths - two poses
// far apart lerp to short rows, near 0 at 180 deg, and the bone went flat for the blend = the user's "paper Mario"
// get-up)
static void ortho_rows(float *m, const float len[3]) {
    float *x = m, *y = m + 3, *z = m + 6, lx = len[0], ly = len[1], lz = len[2];
    if (!v3norm(x)) return;
    float d = v3dot(y, x);
    y[0] -= d * x[0], y[1] -= d * x[1], y[2] -= d * x[2];
    if (!v3norm(y)) return;
    float n[3];
    v3cross(x, y, n);
    if (v3dot(n, z) < 0) n[0] = -n[0], n[1] = -n[1], n[2] = -n[2];  // (keep the bake's handedness)
    for (int j = 0; j < 3; j++) x[j] *= lx, y[j] *= ly, z[j] = n[j] * lz;
}

// Draws Sora for this frame when the ragdoll owns him (or its get-up blend runs); false = draw the motion as usual
static bool rag_draw(int rag, float kh, float scale) {
    int was = g_rag_was;
    g_rag_was = 0;
    if (!g_rag_on) { g_rblend = -1; return false; }
    bool blow = S.special == SP_DAMAGE && (S.motion == MOT_BLOW_FRONT || S.motion == MOT_BLOW_BACK);
    if ((rag > 0 || (!g_rag_getup && was && blow)) && S.mot) {  // (getup 0: on through Link's stand-up)
        if (rag <= 0) rag = 3;
        static uint64_t n;
        bool log = !was || ++n % 15 == 0;
        if (rag_retarget(S.mot->slot, S.clock, S.face, scale, log)) {
            if (!was) LOG("[sora] ragdoll %d: Sora takes Link's pose (retarget)\n", rag);
            g_rag_was = rag;
            memcpy(g_rlast, g_rpose, sizeof g_rpose);
            g_rlast_rig = RIG_CUR;  // (s110 review: the skeleton g_rlast is in)
            sora_draw_world(g_rpose, S.face, scale);
            return true;
        }
        if (log) LOG("[sora] ragdoll %d: retarget unavailable (bones %llx, bake bones %d)\n", rag,
                     (unsigned long long)link_bones_now(), sora_draw_bone_count());
        return false;
    }
    if (was) g_rblend = 0, LOG("[sora] ragdoll over: blending into motion %d\n", S.motion);
    if (g_rblend < 0 || !S.mot) return false;
    if (g_rlast_rig != RIG_CUR) { g_rblend = -1; return false; }  // (s110 review: the body changed (Roxas <-> Sora) since the ragdoll: no cross-skeleton blend)
    g_rblend += kh;
    if (g_rblend >= RAG_BLEND || !sora_draw_slot_pose(S.mot->slot, S.clock, g_rpose)) { g_rblend = -1; return false; }
    float u = g_rblend / RAG_BLEND, wgt = u * u * (3 - 2 * u);  // (smoothstep)
    float squash = 1;  // the shortest lerped row / its wanted length (what flattened him before s52)
    int squash_bone = -1, nbl = sora_draw_bone_count();  // (s69: a form's 252)
    if (nbl < RIG_CUR->nbones || nbl > POSE_BUF_BONES) nbl = RIG_CUR->nbones;  // (s118 review: Mickey's 210, not Sora's 240: no stale bones 210..239 in the blend / squash log)
    for (int k = 0; k < nbl; k++) {
        float *d = g_rpose + k * 12, *a = g_rlast + k * 12, len[3];
        for (int r = 0; r < 3; r++) {
            float la = sqrtf(v3dot(a + 3 * r, a + 3 * r)), lb = sqrtf(v3dot(d + 3 * r, d + 3 * r));
            len[r] = la + (lb - la) * wgt;
        }
        for (int j = 0; j < 12; j++) d[j] = a[j] + (d[j] - a[j]) * wgt;
        for (int r = 0; r < 3; r++) {
            float q = len[r] > 1e-6f ? sqrtf(v3dot(d + 3 * r, d + 3 * r)) / len[r] : 1;
            if (q < squash) squash = q, squash_bone = k;
        }
        ortho_rows(d, len);
    }
    if (squash < 0.7f) LOG("[sora] get-up blend %.1f/%.0f: bone %d row at %.2f of its length (was drawn squashed)\n",
                           g_rblend, RAG_BLEND, squash_bone, squash);
    sora_draw_world(g_rpose, S.face, scale);
    return true;
}

// Game Over and loading screens: BotW stops calling Link's controller step (so sora_mod_frame) while the presents go
// on, drawing Sora's last pose and the HUD over them (cont_s21a: no driver frame 1805 -> 2041; GODMODE keeps his life
// above 0, so the life-0 check never fires in tests). A watchdog thread switches both off once STALE_PRESENTS
// presents passed without a driver frame; the next driver frame publishes them again.
#define STALE_PRESENTS 6
static volatile uint64_t g_wd_frame;  // vk_frame_counter() at the last driver frame
static volatile LONG g_wd_stop;
static volatile int g_invdiag;  // (s100 invdiag, below)
static volatile LONG g_invdiag_thr;

static DWORD WINAPI stale_watchdog(LPVOID arg) {
    (void)arg;
    bool off = false;
    while (!g_wd_stop) {
        Sleep(16);
        uint64_t last = g_wd_frame;
        if (!last) continue;
        bool stale = (int64_t)(vk_frame_counter() - last) >= STALE_PRESENTS;
        if (stale && !off) sora_draw_off(), sora_hud_off(), off = true, LOG("[sora] no driver frame since present %llu "
                                                                              "(Game Over / loading): draw + HUD off\n", (unsigned long long)last);
        else if (!stale && off) off = false, LOG("[sora] driver frames again at present %llu\n", (unsigned long long)last);
    }
    return 0;
}

static volatile int g_invsora = 1;  // (s102: default ON after the live test: menu Link hidden + Sora drawn, VERIFIED) (s102 b16-invsora, sora_invsora.h: live "set invsora"; the scan loop below also runs for it)
static volatile LONG g_is_thr;
static void is_off(void);
static bool is_live_set(const char *key, int v);
// The mod DLL's entry points (runtime/sora_host.c loads them; a rebuilt DLL replaces this one while the game runs and
// starts fresh: Sora re-enters from idle where Link stands).
__declspec(dllexport) void sora_mod_unload(void) {  // (the old copy, before the new one's first frame)
    esa_unload(&g_esa);  // (s175: a running R2 attack is left with the exe Cpu before this copy goes; sora_mod_unload runs inside linkpos_frame via sora_frame -> load)
    g_wd_stop = 1;
    avatar_off();       // (mp: the new copy re-poses it)
    net_party_off();    // (mp slice 3: ... and his Donald + Goofy)
    net_set_on(false);  // (mp: BYE + the port free for the new copy)
    g_invdiag = 0;  // (s100: the diag thread must be gone before this DLL copy is unloaded)
    for (int w = 0; w < 100 && g_invdiag_thr; w++) Sleep(50);
    g_invsora = 0;
    is_off();  // (s102: the menu Link comes back, the screen camera goes)
    for (int w = 0; w < 600 && g_is_thr; w++) Sleep(50);  // (the scan thread: up to 25 s)
    parry_link_clear("unload");  // (s92: Link's guard bits never outlive this copy)
    pl_cancel("unload");  // (s150: a party Limit's effects / hidden Keyblade never outlive this copy)
    mg_cancel("unload");  // (s133 magic: Magnet's pulled enemies get their controllers back; before sora_fx_clear)
    sora_sfx_stop_all();
    sora_fx_clear();
    camp_abort("unload");  // (s78: stops eating the pad at once)
    linkpos_set_pad_tap(NULL);
    {  // (s137 review) the FPS Mode camera write ends with this copy at once (else it lapses after 20 presents)
        void (*cs)(bool, const float *, const float *, const float *) = (void (*)(bool, const float *, const float *, const float *))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_camera_set");
        if (cs) cs(false, NULL, NULL, NULL);
    }
    gameover_off("unload");  // (s59: the exe's screen mode and the A block end with this copy)
    drive_off();
    sora_draw_off();
    sora_hud_off();
    {  // (s111 roxcmd review) the exe keeps the row mask: give it back all four rows, so a DLL without roxcmd (or none) never inherits Roxas' 2 rows
        void (*rows)(int) = (void (*)(int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_rows");
        if (rows) rows(0xF);
    }
    {  // (s119 mkmenu) the exe keeps Mickey's pane too: switch it off with this copy
        int (*pad)(int, const int8_t *, const int8_t *, const uint8_t *) = (int (*)(int, const int8_t *, const int8_t *, const uint8_t *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_pad");
        if (pad) pad(0, NULL, NULL, NULL);
    }
    if (g_slog) fclose(g_slog), g_slog = NULL;
}

// Link's shadow probe (s35): his body is hidden through the opacity apply, but the user still sees his shadow.
// gsys::ModelUnit (zeldaret gsysModelUnit.h, offsets checked against mMaterialInfo +0x68): +0xc u16 mVisibilityMask,
// +0xe u8 mDepthShadowCascade, +0x12 u16 mAttributeFlags. Live "unit dump" logs them per unit; "unit <off> <size>
// <val>" writes one field on every unit each frame (size 1/2/4; "unit off" stops). Units: model = [actor+0x4e0],
// n = [model+0x38], unit i = [[model+0x40] + 8i] plus one hop (FAINT_LINK_RECON.md).
static struct { int on, off, size; uint32_t val; } g_upoke;
static int link_units(uint64_t *u, int max) {
    uint64_t actor = link_actor();
    if (!actor) return 0;
    uint64_t model = *(const uint64_t *)(uintptr_t)(actor + 0x4e0);
    if (!rd_ok(model, 0x48)) return 0;
    int n = *(const int32_t *)(uintptr_t)(model + 0x38), k = 0;
    uint64_t arr = *(const uint64_t *)(uintptr_t)(model + 0x40);
    if (n <= 0 || n > 64 || !rd_ok(arr, 8 * (size_t)n)) return 0;
    for (int i = 0; i < n && k < max; i++) {
        uint64_t p = *(const uint64_t *)(uintptr_t)(arr + 8 * (uint64_t)i);
        uint64_t q = rd_ok(p, 8) ? *(const uint64_t *)(uintptr_t)p : 0;
        if (rd_ok(q, 0x70)) u[k++] = q;
    }
    return k;
}
static void unit_dump(void) {
    uint64_t u[64];
    int n = link_units(u, 64);
    uint64_t w = link_bones_now();
    const float *b = w ? (const float *)(uintptr_t)w : NULL;  // (bones 0 and 20: do they still move with bit 0 off?)
    LOG("[sora] unit dump: %d units, bone0 %.3f %.3f %.3f bone20 %.3f %.3f %.3f\n", n, b ? b[12] : 0, b ? b[13] : 0,
        b ? b[14] : 0, b ? b[16 * 20 + 12] : 0, b ? b[16 * 20 + 13] : 0, b ? b[16 * 20 + 14] : 0);
    for (int i = 0; i < n; i++)
        LOG("[sora]   unit %d %llx vt %llx vis %#x cascade %u attr %#x flags %#x\n", i, (unsigned long long)u[i],
            (unsigned long long)*(const uint64_t *)(uintptr_t)u[i], *(const uint16_t *)(uintptr_t)(u[i] + 0xc),
            *(const uint8_t *)(uintptr_t)(u[i] + 0xe), *(const uint16_t *)(uintptr_t)(u[i] + 0x12),
            *(const uint8_t *)(uintptr_t)(u[i] + 0x8));
}
// Link hidden = his body (linkpos opacity) AND his shadow: mVisibilityMask bit 0 is the shadow view (live_s35 A/B:
// 0xfffe = no shadow, bits 1-3 don't matter; the game never rewrites the mask; his bones keep updating with it off).
// (s113 slvis) ROOT CAUSE of the invisible Slate (s100-s112): BotW copies Link's mask onto his gear every frame (Frida, s113: the
// Slate unit's mask is 0xfffe while Link's is, 0xffff with showlink 1; a forced bit was cleared again 361 times in 6 s), and bit 0
// is the whole MAIN view (shadow included), not only the shadow: the Slate was in no 3D pass whatever its opacity. Live "set slvis 1"
// (default) = while the Slate is to be shown (g_sh_vis_now, sora_slatehip.h sh_tick) Link's bit 0 stays ON (his body is still
// invisible through opacity; his shadow draws in those moments); 0 = the old hide (Slate invisible).
static int g_slvis = 1;
static int g_wpnvis = 1;  // (s128b) keep Link's view bit 0 while Sora holds the BotW weapon (live "set wpnvis 0" = off)
static volatile int g_sh_vis_now;
static void hide_link(bool h) {
    linkpos_hide_link(h);
    uint64_t u[64];
    int n = link_units(u, 64);
    // (s128b) Link's BotW weapon in Sora's hand needs the same bit (BotW copies Link's mask onto it): wpnvis 0 = off (sword invisible)
    bool keep0 = h && g_slvis && (g_sh_vis_now || (g_wpnvis && g_wpn_cur.def && !g_wpn_fake && !g_wpn_name_fn));  // (s128b exe: the sword's own units instead)
    for (int i = 0; i < n; i++) {
        volatile uint16_t *vis = (volatile uint16_t *)(uintptr_t)(u[i] + 0xc);
        *vis = h && !keep0 ? (uint16_t)(*vis & ~1u) : (uint16_t)(*vis | 1u);
    }
}

static void unit_poke_tick(void) {
    if (!g_upoke.on) return;
    uint64_t u[64];
    int n = link_units(u, 64);
    for (int i = 0; i < n; i++) {
        uintptr_t a = (uintptr_t)(u[i] + (uint64_t)g_upoke.off);
        if (g_upoke.size == 1) *(volatile uint8_t *)a = (uint8_t)g_upoke.val;
        else if (g_upoke.size == 2) *(volatile uint16_t *)a = (uint16_t)g_upoke.val;
        else *(volatile uint32_t *)a = g_upoke.val;
    }
}

// (s100 b15-invsora) glitch #62 diag: live "set invdiag 1" (default 0, DLL only, read-only, no guest calls). A thread (it runs
// whether or not sora_frame returns early on the pause) logs once a second what is needed to decide how to put Sora on BotW's
// inventory screen: the pause state, Link's actor matrix, the camera the exe found, Link's units + skeleton bones (do they move
// when the menu opens?), and - once before the menu and at the start of every pause - a memory scan for OTHER skeleton
// instances of Link's model (a menu-only Link). Every read goes through ReadProcessMemory on ourselves (a bad pointer just
// returns false). "set invdiag 2" = log + force a new scan now. Output: "[invdiag]" lines in the game log.
// recon/S100_HELPER_INVSORA.md.
static volatile int g_invdiag_scan;  // 1: scan at the next sample

static bool dg_rd(uint64_t a, void *b, size_t n) {
    SIZE_T got = 0;
    return a >= 0x1000 && ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)a, b, n, &got) && got == n;
}
static uint64_t dg_q(uint64_t a) {
    uint64_t v = 0;
    return dg_rd(a, &v, 8) ? v : 0;
}
// Link's unit that has the 121-bone skeleton (the link_bones_now chain, through dg_rd)
static bool dg_link_unit(uint64_t actor, uint64_t *unit, uint64_t *res, uint64_t *skel, uint64_t *bones) {
    uint64_t model = dg_q(actor + 0x4e0), arr = dg_q(model + 0x40), u = dg_q(arr);
    for (int hop = 0; hop < 2 && u; hop++, u = dg_q(u)) {
        uint64_t sk = dg_q(u + 0x118), w = dg_q(u + 0x198), r = dg_q(sk);
        uint16_t nb = 0;
        if (r && dg_rd(r + 0x3c, &nb, 2) && nb == LINK_BONES && w) {
            *unit = u, *res = r, *skel = sk, *bones = w;
            return true;
        }
    }
    return false;
}
// aligned qwords equal to one of want[0..nw) in the process's private read-write memory (no images); hits[] = their addresses
static int dg_scan(const uint64_t *want, int nw, uint64_t *hits, int max) {
    DWORD t0 = GetTickCount();
    enum { CH = 1 << 20 };
    uint64_t *buf = (uint64_t *)malloc(CH);
    int nh = 0;
    uint64_t total = 0, lim = 24ull << 30, local = 0;
    if (!buf) return 0;
    MEMORY_BASIC_INFORMATION mbi;
    for (uint64_t a = 0x7000000000ull; a < 0x7800000000ull && VirtualQuery(  // (s100: the guest range; from 0x10000 the 8 GB cap ran out first = 0 hits)
         (LPCVOID)(uintptr_t)a, &mbi, sizeof mbi) &&
                               GetTickCount() - t0 < 25000 && total < lim && (g_invdiag || g_invsora);
         a = (uint64_t)(uintptr_t)mbi.BaseAddress + mbi.RegionSize) {
        if (mbi.State != MEM_COMMIT || mbi.Type == MEM_IMAGE || !(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY)) ||
            (mbi.Protect & PAGE_GUARD))
            continue;
        uint64_t lo = (uint64_t)(uintptr_t)mbi.BaseAddress, hi = lo + mbi.RegionSize;
        for (uint64_t c = lo; c < hi && nh < max && (g_invdiag || g_invsora) && GetTickCount() - t0 < 25000; c += CH) {  // (s100 review: stop mid-region too)
            size_t n = hi - c < CH ? (size_t)(hi - c) : CH;
            if (!dg_rd(c, buf, n)) continue;
            total += n;
            for (size_t i = 0; i < n / 8; i++)
                for (int k = 0; k < nw; k++)
                    if (buf[i] == want[k]) {
                        uint64_t at = c + 8 * i;
                        if ((at >= (uint64_t)(uintptr_t)buf && at < (uint64_t)(uintptr_t)buf + CH) ||
                            (at + 0x20000 > (uint64_t)(uintptr_t)&local && at < (uint64_t)(uintptr_t)&local + 0x20000))
                            continue;  // (our own chunk buffer / this thread's stack)
                        if (nh < max) hits[nh++] = at;
                    }
        }
    }
    free(buf);
    LOG("[invdiag]   scanned %.2f GB in %.1f s\n", total / 1073741824.0, (GetTickCount() - t0) / 1000.0);
    return nh;
}
static void dg_scan_report(uint64_t lunit, uint64_t res, uint64_t skel, const char *why) {
    uint64_t sk[64], un[64], cand[64], vt = dg_q(lunit);
    int nsk = dg_scan(&res, 1, sk, 64), nc = 0, nu = 0;
    for (int i = 0; i < nsk; i++)
        if (sk[i] != skel) cand[nc++] = sk[i];  // (a holder of Link's skeleton resource that is not his own skeleton)
    LOG("[invdiag] SCAN (%s): %d holders of Link's skeleton resource %llx, %d besides his own skeleton %llx\n", why, nsk,
        (unsigned long long)res, nc, (unsigned long long)skel);
    if (nc) nu = dg_scan(cand, nc, un, 64);
    for (int i = 0; i < nu; i++) {
        uint64_t u = un[i] - 0x118, w = dg_q(u + 0x198);
        float b0[12] = {0};
        if (w) dg_rd(w, b0, 48);
        LOG("[invdiag]   OTHER unit %llx (points at a second Link skeleton at +0x118): vt %llx %s, vis %#x, flags %#x, bones %llx, "
            "bone0 t %.3f %.3f %.3f\n",
            (unsigned long long)u, (unsigned long long)dg_q(u), dg_q(u) == vt ? "(same vtable as Link's unit)" : "(other vtable)",
            (unsigned)(dg_q(u + 0xc) & 0xffff), (unsigned)(dg_q(u + 0x8) & 0xff), (unsigned long long)w, b0[3], b0[7], b0[11]);
    }
    if (!nu) LOG("[invdiag]   no second Link unit found (a menu Link, if any, shares Link's own units or is not a ModelUnit)\n");
}
static DWORD WINAPI invdiag_thread(LPVOID arg) {
    (void)arg;
    int last_pause = 0, base_done = 0;
    uint64_t n = 0;
    LOG("[invdiag] on: one line per second; a scan before the menu and at the start of each pause\n");
    while (!g_wd_stop && g_invdiag) {
        for (int w = 0; w < 20 && g_invdiag && !g_wd_stop; w++) Sleep(50);
        if (!g_invdiag || g_wd_stop) break;
        n++;
        uint32_t ps = 0;
        dg_rd((uint64_t)(uintptr_t)PAUSE_STATE, &ps, 4);
        uint64_t pi = dg_q(PLAYER_INFO_PTR), actor = pi ? dg_q(pi + 0x60) : 0, lunit = 0, res = 0, skel = 0, bones = 0;
        float am[12] = {0}, cv[12] = {0}, cp[4] = {0};
        if (actor) dg_rd(actor + 0x398, am, 48);
        bool cam = linkpos_camera_view(cv), prj = linkpos_camera_proj(cp);
        bool lu = actor && dg_link_unit(actor, &lunit, &res, &skel, &bones);
        float b0[3] = {0}, b20[3] = {0};
        uint32_t hsh = 2166136261u;
        if (lu) {
            static float all[LINK_BONES * 16];
            dg_rd(bones + 12 * 4, b0, 12);
            dg_rd(bones + 20 * 64 + 12 * 4, b20, 12);
            if (dg_rd(bones, all, sizeof all))
                for (size_t i = 0; i < sizeof all; i++) hsh = (hsh ^ ((unsigned char *)all)[i]) * 16777619u;
        }
        LOG("[invdiag] #%llu present %llu world %llu pause %u | actor %llx pos %.2f %.2f %.2f | cam %s %.2f %.2f %.2f proj %s "
            "fovy %.3f asp %.3f | linkunit %llx bones %llx b0 %.2f %.2f %.2f b20 %.2f %.2f %.2f hash %08x\n",
            (unsigned long long)n, (unsigned long long)vk_frame_counter(), (unsigned long long)linkpos_world_frame(), ps,
            (unsigned long long)actor, am[3], am[7], am[11], cam ? "yes" : "NO", cv[3], cv[7], cv[11], prj ? "yes" : "NO", cp[2],
            cp[3], (unsigned long long)lunit, (unsigned long long)bones, b0[0], b0[1], b0[2], b20[0], b20[1], b20[2], hsh);
        if (lu) {  // Link's own units (visibility mask / flags) and where a unit holds a copy of his position (a model-matrix offset)
            uint64_t uu[64], dm = dg_q(actor + 0x4e0), da = dg_q(dm + 0x40);  // (s100 review: link_units derefs directly = unsafe off-thread)
            uint32_t dn = 0;
            int nu = 0;
            if (dm && da && dg_rd(dm + 0x38, &dn, 4) && dn > 0 && dn <= 64)
                for (uint32_t i = 0; i < dn; i++) {
                    uint64_t q = dg_q(dg_q(da + 8ull * i));
                    if (q) uu[nu++] = q;
                }
            for (int i = 0; i < nu && i < 6; i++) {
                uint8_t ub[0x200];
                if (!dg_rd(uu[i], ub, sizeof ub)) continue;
                char hit[128] = "";
                for (int o = 0x20; o + 12 <= (int)sizeof ub && strlen(hit) < 100; o += 4) {
                    float f[3];
                    memcpy(f, ub + o, 12);
                    if (fabsf(f[0] - am[3]) < 0.01f && fabsf(f[1] - am[7]) < 0.01f && fabsf(f[2] - am[11]) < 0.01f &&
                        fabsf(am[3]) + fabsf(am[11]) > 1.0f)
                        sprintf(hit + strlen(hit), " +%#x", o);
                }
                uint64_t vt;
                uint16_t vis;
                memcpy(&vt, ub, 8);
                memcpy(&vis, ub + 0xc, 2);
                LOG("[invdiag]   unit %d %llx vt %llx vis %#x flags %#x pos-copy at:%s\n", i, (unsigned long long)uu[i],
                    (unsigned long long)vt, (unsigned)vis, (unsigned)ub[8], hit[0] ? hit : " none");
            }
        }
        if (lu && (g_invdiag_scan || (!base_done && !ps) || (ps && !last_pause))) {
            dg_scan_report(lunit, res, skel, g_invdiag_scan ? "forced" : ps ? "pause start" : "before the menu");
            if (!ps) base_done = 1;
            g_invdiag_scan = 0;
        }
        last_pause = ps != 0;
    }
    LOG("[invdiag] off\n");
    InterlockedExchange(&g_invdiag_thr, 0);
    return 0;
}
static void invdiag_set(int v) {
    g_invdiag = v != 0;
    if (v == 2) g_invdiag_scan = 1;
    if (v && !InterlockedExchange(&g_invdiag_thr, 1)) {
        HANDLE h = CreateThread(NULL, 0, invdiag_thread, NULL, 0, NULL);
        if (h) CloseHandle(h);
        else InterlockedExchange(&g_invdiag_thr, 0);
    }
}

// (s48 water recon) "snap on" / "snap off": every 30 frames append Link's actor (SNAP_ACTOR bytes, unreadable pages
// zeroed) + PlayerInfo (SNAP_PI bytes) to D:/SoraBotW/run/snap.bin, each after a 32-byte header {magic 'SNAP', frame,
// feet x y z, state word +0xcec, actor lo, actor hi}; recon/tools diff it for a water depth / surface field.
#define SNAP_ACTOR 0x4000
#define SNAP_PI 0x400
static FILE *g_snap;
static void snap_tick(const float p[3], uint64_t present) {
    if (!g_snap || present % 30) return;
    static uint8_t buf[SNAP_ACTOR + SNAP_PI];
    uint64_t pi = *(const uint64_t *)(uintptr_t)PLAYER_INFO_PTR, a = link_actor();
    if (!a) return;
    for (uint32_t o = 0; o < SNAP_ACTOR; o += 0x100)
        if (rd_ok(a + o, 0x100)) memcpy(buf + o, (const void *)(uintptr_t)(a + o), 0x100);
        else memset(buf + o, 0, 0x100);
    if (rd_ok(pi, SNAP_PI)) memcpy(buf + SNAP_ACTOR, (const void *)(uintptr_t)pi, SNAP_PI);
    else memset(buf + SNAP_ACTOR, 0, SNAP_PI);
    uint32_t h[8] = {0x50414e53, (uint32_t)present, 0, 0, 0, *(const uint32_t *)(uintptr_t)(a + 0xcec), (uint32_t)a,
                     (uint32_t)(a >> 32)};
    memcpy(&h[2], p, 12);
    fwrite(h, sizeof h, 1, g_snap), fwrite(buf, sizeof buf, 1, g_snap), fflush(g_snap);
}

// ---- (s54 floor ray, S54_HELPER_FLOOR_RAY.md) the real floor under Sora while he is in the air. KH2 casts down for
// "height above the floor" (re:0x3B87E0: GlideStart / GlideEnd height, the glide's sink); s53's world ray cast
// (linkpos_raycast, static ground layers 8 / 10 / 9 / 2 / 12, no Link / enemies / loose objects) gives it here. One
// cast straight down from g_fup u above his feet to g_flen u below them, cached: a new one every g_fevery presents,
// after g_fmove u of horizontal move, and on every frame once the floor is nearer than g_fnear_cm (the landing and the
// glide's end read a this-frame floor). Only from sora_mod_frame's driving path (inside linkpos_frame = g_in_frame;
// linkpos refuses it anywhere else). Users: the glide's h (height_above_floor), the landing (section 1: the same-step
// land of KH2's collision re:0x3B9090, and an optional veto of a "landing" with no floor under him) and the blob
// shadow. Not airborne / off / no cast yet -> the s53 S.ground_z rules. Live "set fdown 0/1" (all of it),
// "fevery <presents>", "fmove <x100 u>", "flen <u>", "fnear <cm>", "fglide / fland / fshadow 0/1", "flandcm <cm>",
// "fveto <frames, 0 off>", "fvetocm <cm>", "flog 0/1" (a line per cast).
static int g_fdown = 1, g_fevery = 2, g_fglide = 1, g_fland = 1, g_fshadow = 1, g_fveto = 0, g_flog = 0;
static float g_fmove = 0.5f, g_flen = 40.0f, g_fup = 0.5f;  // u
static float g_fnear_cm = 150.0f, g_fland_cm = 3.0f, g_fveto_cm = 30.0f;  // KH cm (fland_cm <= WALKOFF_DROP: no walk-off)
enum { FS_LAST, FS_RAY, FS_MISS };  // floor source: the last grounded height (s53), the ray's hit, no floor in reach
static const char *const FS_NAME[] = {"last ground", "ray", "ray miss"};
static struct {
    bool valid, hit, works;  // valid: cast in this air stretch and fresh; works: a hit was seen once (else a miss may
                             // be a refused cast - linkpos_raycast returns false for both)
    float y, n[3], at[3];    // hit y (u), its normal, where it was cast from (u)
    uint64_t present;        // present of the last cast (0: none this air stretch)
    int casts, hits, veto;   // this air stretch; veto = landings vetoed in a row
} FR;

static void floor_ray_tick(const float p[3], uint64_t present, bool air) {
    if (!g_fdown || !air) {  // (on the ground the floor is his feet: S.ground_z)
        if (FR.casts)
            LOG("[sora] floor ray: %d casts, %d hits this air stretch (last %s y %.3f)\n", FR.casts, FR.hits,
                FR.hit ? "hit" : "miss", FR.y);
        FR.valid = FR.hit = false, FR.casts = FR.hits = FR.veto = 0, FR.present = 0;
        return;
    }
    uint64_t every = (uint64_t)(g_fevery < 1 ? 1 : g_fevery);
    float moved = hypotf(p[0] - FR.at[0], p[2] - FR.at[2]), h_cm = (p[1] - FR.y) * 100 / g_k;
    bool due = !FR.present || present - FR.present >= every || moved >= g_fmove || (FR.hit && h_cm < g_fnear_cm);
    if (due && FR.present != present) {
        float a[3] = {p[0], p[1] + g_fup, p[2]}, b[3] = {p[0], p[1] - g_flen, p[2]}, h[3] = {0}, n[3] = {0};
        bool hit = linkpos_raycast(a, b, h, n), was = FR.hit, first = !FR.casts;
        FR.casts++, FR.present = present, memcpy(FR.at, p, sizeof FR.at);
        if (hit) FR.hits++, FR.works = true, FR.y = h[1], memcpy(FR.n, n, sizeof FR.n);
        FR.hit = hit;
        if (first || hit != was || g_flog)
            LOG("[sora] floor ray%s at %.3f %.3f %.3f: %s y %.3f (%.1f cm below the feet) n %.2f %.2f %.2f\n",
                first ? " (air)" : hit != was ? " (now)" : "", p[0], p[1], p[2], hit ? "hit" : "NO HIT", h[1],
                (p[1] - (hit ? h[1] : p[1] - g_flen)) * 100 / g_k, n[0], n[1], n[2]);
    }
    FR.valid = FR.present && present - FR.present <= every;
}

// (s64, bug: "the run sometimes slows down") the servo sets a level velocity and leaves y to the game, so on an
// uphill slope Havok's solver eats the part pushing into the ground: s64_run1/run2 at 4.8 u/s out, flat and downhill
// 100 %, 26-45 deg uphill 45-75 % of the speed, for as long as the slope lasts. KH2 keeps his ground speed level on a
// slope; here the y velocity follows the floor instead: a ray down g_slope_ahead u ahead of the feet, a walkable
// floor (n.y >= cos g_slope_max) within g_slope_reach u of the feet that rises along the move -> vy = the rise for
// the level speed (vx, vz unchanged). Downhill, steps (a level top: no rise) and steeper rock stay the game's (the
// step-up kick, BotW's slide). Live "set slope 0/1", "set slopemax <deg>", "set slopeahead <x100 u>", "set slopelog 0/1".
// (s64) idle / walk / run changes (and Land into them) blend over KH_LOOP_BLEND too (the drawer, live "set lblend 0/1")
static int g_lblend = 1, g_lb_from = -1, g_lb_prev = -1;  // blend's from-motion (-1: none), the motion drawn last
static float g_lb_clock, g_lb_prev_clock;
// (s66 audit #3 part 2) a grounded swing / roll ending (or freed by blend_to_idle) into idle / walk / run blends too,
// and walk <-> run keeps the cycle phase (SDR 3.5). Live "set eblend 0" / "set lphase 0" = the cuts as before.
static int g_eblend = 1, g_lphase = 1;
static bool g_lb_prev_sp;  // the motion drawn last was a grounded attack / roll
static bool loco_mot(int m) { return m == MOT_IDLE || m == MOT_WALK || m == MOT_RUN; }
static int g_slope = 1, g_slope_log = 0;
static float g_slope_max = 40.0f, g_slope_ahead = 0.3f, g_slope_reach = 0.8f;
static bool slope_follow(const float p[3], float vx, float vz, float *vy) {
    float sp = hypotf(vx, vz);
    if (!g_slope || sp < 0.3f) return false;
    float ax = p[0] + vx / sp * g_slope_ahead, az = p[2] + vz / sp * g_slope_ahead;
    float a[3] = {ax, p[1] + g_slope_reach, az}, b[3] = {ax, p[1] - g_slope_reach, az}, h[3] = {0}, n[3] = {0};
    if (!linkpos_raycast(a, b, h, n) || n[1] < cosf(g_slope_max * 3.14159265f / 180)) return false;
    float rise = -(n[0] * vx + n[2] * vz) / n[1];  // u/s up along the floor plane at the level speed
    if (rise <= 0.05f) return false;  // (level or downhill: the game's gravity keeps him on it)
    *vy = rise;
    static unsigned k;
    if (g_slope_log && k++ % 10 == 0)
        LOG("[sora] slope follow: n %.2f %.2f %.2f (%.0f deg) vy %.2f at %.2f u/s\n", n[0], n[1], n[2],
            acosf(n[1]) * 180 / 3.14159265f, rise, sp);
    return true;
}

// The floor under Sora, KH cm (absolute, like z_up), and where it came from. FS_MISS only once the ray has hit at
// least once (a cast refused by an old exe must not turn every floor into "far").
static int floor_under(float *floor_cm) {
    *floor_cm = S.ground_z;
    if (!g_fdown || !FR.valid) return FS_LAST;
    if (FR.hit) {
        *floor_cm = FR.y * 100 / g_k;
        return FS_RAY;
    }
    return FR.works ? FS_MISS : FS_LAST;
}
// a ray hit cast this very present (the same-step landing: a cached floor 0.5 u away on a slope is tens of cm off)
static bool floor_fresh_hit(uint64_t present) { return g_fdown && FR.valid && FR.hit && FR.present == present; }

// (s54) section 1's landing with the floor ray. KH2's collision lands him in the physics step he reaches the floor
// (re:0x3B9090 -> ACTION slot 6 re:0x3CB940); s53 sees it a frame later (the requested drop was not made). Same-step:
// last step's request was downward and this present's ray floor is within g_fland_cm under the feet -> landed now,
// S.ground_z = that floor (not above the feet: the walk-off test then sees no drop). "set fland 0" = s53 only.
static bool land_by_ray(uint64_t present, float z_up, float *ground) {
    if (!g_fland || !floor_fresh_hit(present) || !(S.req < 0)) return false;
    float f = FR.y * 100 / g_k;
    if (z_up - f > g_fland_cm) return false;
    *ground = f < z_up ? f : z_up;
    return true;
}
// Veto (live "set fveto <n>", 0 = off, the default): a stopped drop with the ray floor more than g_fveto_cm below his
// feet (or none within g_flen u) = BotW holds the body on a wall / an edge, no floor under Sora (KH2 only lands on
// one; s50_wall1 "landed" at the wall hop's peak) -> keep falling, at most g_fveto frames in a row (his capsule may
// really stand on an edge the centre ray misses, or on a loose object outside the ground layers: lands after that).
static bool land_veto(float z_up, const float p[3]) {
    float f;
    int src = g_fveto > 0 ? floor_under(&f) : FS_LAST;
    if (src == FS_LAST || FR.veto >= g_fveto) return false;
    float below = src == FS_RAY ? z_up - f : g_flen * 100 / g_k;
    if (below <= g_fveto_cm) return false;
    FR.veto++;
    LOG("[sora] land vetoed %d/%d at %.3f %.3f %.3f: %s %.1f cm below the feet\n", FR.veto, g_fveto, p[0], p[1], p[2],
        src == FS_RAY ? "floor ray" : "no floor ray hit within", below);
    return true;
}

// (s60b, bug: ground combos turned into air combos vs Bokoblins) live_s60 had 12 "walk-off (dropping)" mid ground
// swing (motion 151 / 153, after a step-up, a repel or an enemy's push) with the next ray floor 6-16 cm under the
// feet -> ACT_JUMP -> the next A an air combo. KH2 only walks off a real edge (air combos after a real jump / launch /
// drop): while swinging or with a live enemy within SAVAGEWIND_SORA_TARGET_R, a drop is a walk-off only when the ray
// finds no floor within g_wo_floor_cm under the feet; else he stays grounded, feet put on that floor (g_wo_snap).
// Live "set woguard 0 (s60) / 1 (swing or enemy near) / 2 (every drop)", "set wofloor <cm>", "set wosnap 0/1".
static int g_wo_guard = 1, g_wo_snap = 1, g_wo_cap = 1;
static float g_wo_floor_cm = 50.0f;
static bool walkoff_floor(const float p[3], float *floor_u) {  // true: a floor (not a wall) within reach, its y (u)
    float a[3] = {p[0], p[1] + 0.3f, p[2]}, b[3] = {p[0], p[1] - g_wo_floor_cm * 0.01f * g_k, p[2]}, h[3] = {0}, n[3] = {0};
    if (!linkpos_raycast(a, b, h, n) || n[1] < g_ray_wall_ny) return false;
    *floor_u = h[1];
    return true;
}

// Live tests in one running game (no restart per test; with `make sora` hot reload): the test file
// SAVAGEWIND_SORA_LIVE (default D:/SoraBotW/run/live_test.txt) is checked every 15 frames; when it changes, its lines
// run once, in order:
//   name <tag>            logged as "[sora] live test <tag>" (and a "# live" line in the .sora.txt log)
//   goto <x> <y> <z> | goto lynel <1..3> | goto off   safe long teleport (held until the ground is loaded; s108)
//   at <x> <y> <z>        teleport the body there (linkpos_move by the difference; give y some height, he drops)
//   set combo <0|1> | targets <0|1> | stop <u>    Combo Master, target select, STICK_ENEMY's stop distance
//   input <spec>          SAVAGEWIND_SORA_INPUT windows, frames from now + 30 (the teleport settles first)
// The input windows replace the previous ones. Camp: at -1141.005 258.024 1772.503 (the SERVO teleport's target).
#include "sora_prof.h"  // (s81 fps hunt: "prof <ms> <tag>")
#include "sora_mark.h"  // (s111: F5 bug marks + live "shot <name>")
static bool party_set(const char *key, int v);  // (s54 party3: runtime/sora_party.h, included below)
static void party_live(const char *cmd);
// ---- (s108 b20-lynel) live "goto <x> <y> <z>" / "goto lynel <1..3>" / "goto off": a safe long teleport. The terrain around
// the target is not loaded when the body arrives (BotW streams the map around Link's position; GUESS, not verified from
// code), so it is held there (zero velocity, position re-applied) a few metres above the target until a ray straight down
// has hit ground for 1 s in a row, then put on the hit + 0.1 and let go. 20 s without that = it stays held and logs.
static const struct { const char *what; float lynel[3], spot[3]; } GT_LYNEL[3] = {  // map data (recon/S108_HELPER_LYNEL.md)
    {"E-8 Junior", {-781.81f, 176.92f, 3656.41f}, {-799.5f, 176.6f, 3674.1f}},
    {"I-6 Junior", {3261.15f, 263.12f, 1211.79f}, {3286.1f, 264.9f, 1211.8f}},
    {"I-2 Middle", {3637.0f, 127.94f, -2965.5f}, {3637.0f, 128.4f, -2990.5f}},
};
#define GT_ABOVE 4.0f
static void goto_start(const float p[3], float x, float y, float z, const char *why) {
    GT.on = true, GT.timeout = false;
    GT.tgt[0] = x, GT.tgt[1] = y, GT.tgt[2] = z;
    GT.hold[0] = x, GT.hold[1] = y + GT_ABOVE, GT.hold[2] = z;
    GT.t0 = GetTickCount64(), GT.ok0 = 0, GT.last_log = GT.t0, GT.lasth = -1e9f, GT.nmv = 1;
    GT.mv_wf = linkpos_world_frame();
    goto_carry(true);  // (s113)
    linkpos_move(GT.hold[0] - p[0], GT.hold[1] - p[1], GT.hold[2] - p[2]);
    linkpos_set_velocity(true, 0, 0), linkpos_set_velocity_y(true, 0);
    S.nsafe = 0, S.air_frames = 0;
    LOG("[goto] %s: from %.2f %.2f %.2f to %.2f %.2f %.2f, held %.1f m above until the ground ray hits for 1 s (20 s timeout)\n",
        why, p[0], p[1], p[2], x, y, z, GT_ABOVE);
}
static void goto_tick(const float p[3]) {
    if (!GT.on) return;
    ULONGLONG now = GetTickCount64();
    uint64_t wf = linkpos_world_frame();
    float e[3] = {GT.hold[0] - p[0], GT.hold[1] - p[1], GT.hold[2] - p[2]};
    float err = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
    if (err > 0.05f && wf >= GT.mv_wf + 2) linkpos_move(e[0], e[1], e[2]), GT.mv_wf = wf, GT.nmv++;
    S.body_xz[0] = p[0] * 100 / g_k, S.body_xz[1] = p[2] * 100 / g_k;  // (no walk-off / shove spike from the jump)
    // (s113) the ray goes down where the body IS (it was only cast within 1 m of the spot: held 4.9 m off = never a hit, never let go)
    float a[3] = {p[0], GT.hold[1] + 40, p[2]}, b[3] = {p[0], GT.hold[1] - 80, p[2]}, h[3], n[3];
    bool hit = err < 10.0f && linkpos_raycast(a, b, h, n) && n[1] > 0.5f;
    if (hit && fabsf(h[1] - GT.lasth) < 0.3f) {
        if (!GT.ok0) GT.ok0 = now, LOG("[goto] ground ray hits at y %.2f (normal y %.2f), waiting 1 s\n", h[1], n[1]);
    } else {
        if (GT.ok0) LOG("[goto] ground ray lost / changed (hit %d, y %.2f): wait restarts\n", hit, hit ? h[1] : 0.f);
        GT.ok0 = 0;
    }
    if (hit) GT.lasth = h[1];
    if (hit && GT.ok0 && now - GT.ok0 >= 1000) {
        linkpos_move(0, h[1] + 0.1f - p[1], 0);
        GT.on = false;
        drive_off();
        goto_carry(false);
        S.nsafe = 0, S.air_frames = 0;
        S.body_xz[0] = p[0] * 100 / g_k, S.body_xz[1] = p[2] * 100 / g_k;
        party_live("warp");
        LOG("[goto] DONE after %llu ms, %d corrections: placed at %.2f %.2f %.2f (ray hit %.2f + 0.1, %.2f m from the spot), released, party warped\n",
            (unsigned long long)(now - GT.t0), GT.nmv, p[0], h[1] + 0.1f, p[2], h[1], err);
        return;
    }
    if (now - GT.t0 > 20000) {  // (s113: it stayed held forever + this line never logged: the 2 s log below reset last_log first)
        GT.on = false, GT.timeout = true;
        drive_off();
        goto_carry(false);
        LOG("[goto] TIMEOUT %llu ms: no stable ground under %.2f %.2f %.2f (err %.2f, hit %d): LET GO (falls from here)\n",
            (unsigned long long)(now - GT.t0), p[0], p[1], p[2], err, hit);
    } else if (now - GT.last_log >= 2000)
        GT.last_log = now, LOG("[goto] holding %llu ms: err %.2f, ray %s%.2f\n", (unsigned long long)(now - GT.t0), err, hit ? "hit y " : "no hit ", hit ? h[1] : 0.f);
}
static void live_poll(const float p[3], uint64_t present) {
    static const char *path;
    static long long seen = -1;
    // (s99 debug) live "hold <hexaddr> <float>": write that float every present (max 4 slots; "hold <hexaddr> off" / "hold off")
    static struct { uint64_t a; float v; } hold[4];
    for (int i = 0; i < 4; i++) if (hold[i].a) *(volatile float *)(uintptr_t)hold[i].a = hold[i].v;
    if (present % 15 && seen != -1) return;  // (s99: the FIRST read is not gated, so a "boot" file lands on the first world frame)  // (s99: the FIRST read is not gated, so a "boot" file lands on the first world frame)
    if (!path) path = sora_path(getenv("SAVAGEWIND_SORA_LIVE") && *getenv("SAVAGEWIND_SORA_LIVE") ? getenv("SAVAGEWIND_SORA_LIVE")
                                                                                                   : "D:/SoraBotW/run/live_test.txt");
    FILE *f = fopen(path, "rb");
    if (!f) { if (seen == -1) seen = 0; return; }  // (missing at the start: its first version is a test)
    char buf[8192];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    long long h = 1469598103934665603LL;  // (FNV-1a of the contents: a rewrite with new text is a new test)
    for (size_t i = 0; i < n; i++) h = (h ^ (unsigned char)buf[i]) * 1099511628211LL;
    if (h == seen) return;
    bool first = seen == -1;
    seen = h;
    // (a file left from before this game / reload: not a new test - unless it has a "boot" line (s99): then it applies at the
    // first world frame, e.g. "set wake 1" for the new game's wake-up scene, or settings to keep across a hot reload)
    if (first && strncmp(buf, "boot", 4) && !strstr(buf, "\nboot")) return;
    if (first) LOG("[sora] live: boot file applied at the first world frame %llu\n", (unsigned long long)present);
    char *s = NULL;
    for (char *ln = strtok_r(buf, "\r\n", &s); ln; ln = strtok_r(NULL, "\r\n", &s)) {
        float x, y, z;
        char key[16];
        int v, v2;
        if (!strncmp(ln, "name ", 5)) {
            LOG("[sora] live test %s at presented frame %llu\n", ln + 5, (unsigned long long)present);
            if (g_slog) fprintf(g_slog, "# live %s %llu\n", ln + 5, (unsigned long long)present), fflush(g_slog);
        } else if (!strncmp(ln, "peek ", 5) || !strncmp(ln, "hold ", 5)) {  // (s99 debug) peek <hexaddr> <n floats> / hold <hexaddr> <float|off>
            unsigned long long a = 0;
            char arg[32] = {0};
            MEMORY_BASIC_INFORMATION mi;
            if (!strcmp(ln + 5, "off")) { memset(hold, 0, sizeof hold); LOG("[sora] live: hold all off\n"); }
            else if (sscanf(ln + 5, "%llx %31s", &a, arg) < 1 || !VirtualQuery((void *)(uintptr_t)a, &mi, sizeof mi) || mi.State != MEM_COMMIT ||
                     (uintptr_t)a + 64 > (uintptr_t)mi.BaseAddress + mi.RegionSize || (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
                LOG("[sora] live: %s refused (not readable)\n", ln);
            else if (ln[0] == 'p') {
                int k = atoi(arg); k = k < 1 ? 4 : k > 16 ? 16 : k;
                char out[512]; int o = 0;
                for (int i = 0; i < k; i++) {
                    float fv = ((volatile float *)(uintptr_t)a)[i];
                    o += snprintf(out + o, sizeof out - o, " +%#x=%g(%08x)", i * 4, fv, ((volatile uint32_t *)(uintptr_t)a)[i]);
                }
                LOG("[sora] live: peek %llx:%s\n", a, out);
            } else {
                int slot = -1;
                for (int i = 0; i < 4; i++) if (hold[i].a == a) slot = i;
                if (!strcmp(arg, "off")) { if (slot >= 0) hold[slot].a = 0; LOG("[sora] live: hold %llx off\n", a); }
                else {
                    for (int i = 0; i < 4 && slot < 0; i++) if (!hold[i].a) slot = i;
                    if (slot >= 0) hold[slot].a = a, hold[slot].v = (float)atof(arg), LOG("[sora] live: hold %llx = %g\n", a, hold[slot].v);
                }
            }
        } else if (!strncmp(ln, "fx ", 3)) {  // (s39) audition an effect element at his feet
            int fh = sora_fx_event(ln + 3, p, S.face, 0.01f * g_k);
            LOG("[sora] live: fx %s -> handle %d (%d live)\n", ln + 3, fh, sora_fx_count());
        } else if (!strcmp(ln, "at enemy")) {  // (s34: spell hit tests) next to the nearest live enemy, 1 u short
            uint64_t ea[64];
            float ep[64][3];
            int32_t el[64];
            int en = linkpos_enemies(ea, ep, el, 64), bi = -1;
            float bd = 1e18f;
            for (int i = 0; i < en; i++) {
                float dx = ep[i][0] - p[0], dz = ep[i][2] - p[2];
                if (el[i] > 0 && dx * dx + dz * dz < bd) bd = dx * dx + dz * dz, bi = i;
            }
            if (bi >= 0) {
                float d = sqrtf(bd), k = d > 1 ? (d - 1) / d : 0;
                linkpos_move((ep[bi][0] - p[0]) * k, ep[bi][1] - p[1] + 0.3f, (ep[bi][2] - p[2]) * k);
                LOG("[sora] live: at enemy %llx (%.2f u away)\n", (unsigned long long)ea[bi], d);
            } else LOG("[sora] live: at enemy: none\n");
        } else if (!strcmp(ln, "goto off")) {
            if (GT.on) goto_carry(false);
            GT.on = false, drive_off();
            LOG("[goto] cancelled (let go at %.2f %.2f %.2f)\n", p[0], p[1], p[2]);
        } else if (sscanf(ln, "goto lynel %d", &v) == 1 && v >= 1 && v <= 3) {  // the standing spot 25 m from that Lynel
            LOG("[goto] lynel %d = %s at %.2f %.2f %.2f\n", v, GT_LYNEL[v - 1].what, GT_LYNEL[v - 1].lynel[0], GT_LYNEL[v - 1].lynel[1], GT_LYNEL[v - 1].lynel[2]);
            goto_start(p, GT_LYNEL[v - 1].spot[0], GT_LYNEL[v - 1].spot[1], GT_LYNEL[v - 1].spot[2], "goto lynel");
        } else if (sscanf(ln, "goto %f %f %f", &x, &y, &z) == 3) {
            goto_start(p, x, y, z, "goto");
        } else if (sscanf(ln, "at %f %f %f", &x, &y, &z) == 3) {
            linkpos_move(x - p[0], y - p[1], z - p[2]);
            LOG("[sora] live: teleport %.3f %.3f %.3f -> %.3f %.3f %.3f\n", p[0], p[1], p[2], x, y, z);
        } else if (!strncmp(ln, "prof ", 5)) {
            prof_start(ln + 5);
        } else if (!strncmp(ln, "shot ", 5)) {  // (s111) a PNG of the window: play/shots/<name>.png
            mark_live_shot(ln + 5);
        } else if (!strncmp(ln, "party ", 6)) {  // (s54 party3: party blow [pct] | warp | on | off)
            party_live(ln + 6);
        } else if (sscanf(ln, "set shortcut %d %d", &v, &v2) == 2) {  // (s84 slice 4: slot 0..3, spell 0..3 / -1 empty)
            sl_init();
            sc_set(v, v2, "live set shortcut");
        } else if (sscanf(ln, "set keyblade %15s %i", key, &v) == 2 && (!strcmp(key, "main") || !strcmp(key, "valor") || !strcmp(key, "master") || !strcmp(key, "final"))) {
            sl_init();  // (s85 slice 5: set keyblade main|valor <item id, e.g. 0x2b>; s91 mfkb: master | final too)
            kb_set_v(!strcmp(key, "valor") ? KB_VALOR : !strcmp(key, "master") ? KB_MASTER : !strcmp(key, "final") ? KB_FINAL : KB_MAIN, v, "live set keyblade");
        } else if (sscanf(ln, "set kbfxid %i", &v) == 1) {  // (s89 kbfx: force a Keyblade's fx set, e.g. 0x2b; 0 = equipped)
            kbfx_live_id(v);
        } else if (sscanf(ln, "set frzscr %i %d", &v, &v2) == 2 && v >= 0 && v < 256) {  // (s86 freeze screens)
            if (v2) g_frz_mask[v / 8] |= (uint8_t)(1u << (v % 8));
            else g_frz_mask[v / 8] &= (uint8_t)~(1u << (v % 8));
            LOG("[sora] live: freeze screen %#x %s\n", v, v2 ? "on" : "off");
        } else if (sscanf(ln, "set %15s %d", key, &v) == 2 && strcmp(key, "stop")) {
            if (!strcmp(key, "combo")) g_combo_master = v != 0;
            else if (!strcmp(key, "berserk")) g_berserk = v != 0, LOG("[sora] live: berserk %d (Berserk Charge: endless combo + Str+2 during MP Charge)\n", g_berserk);
            else if (!strcmp(key, "abilfx")) { g_af_on = v != 0; LOG("[sora] live: abilfx %d (KH2 fx + SE of the combo-ability attacks; fx.bin %s)\n", g_af_on, sora_fx_id("abil_fx_44") >= 0 ? "has abil_fx" : "WITHOUT abil_fx (fx_s107.bin not installed)"); }
            else if (!strcmp(key, "t0")) g_t0 = v != 0, LOG("[sora] live: t0 %d (Type 0 combo abilities: Counterguard, Retaliating Slash, Upper Slash, Horizontal Slash, Finishing Leap)\n", g_t0);
            else if (!strcmp(key, "comboabil")) g_comboabil = v != 0, LOG("[sora] live: comboabil %d (%s)\n", g_comboabil, g_comboabil ? "equipped Action abilities pick their attacks, Combo Plus Keyblade +1 ground" : "the old fixed combo");
            else if (!strcmp(key, "combolog")) g_combolog = v != 0, LOG("[sora] live: combolog %d\n", g_combolog);
            else if (!strcmp(key, "kbswap")) g_kbswap = v != 0, LOG("[sora] live: kbswap %d (picking a Keyblade another slot holds %s)\n", g_kbswap, g_kbswap ? "swaps the two" : "= error SE (the old rule)");  // (s118)
            else if (!strcmp(key, "eq2x") || !strcmp(key, "eq2y") || !strcmp(key, "eqtx") || !strcmp(key, "eqvx") || !strcmp(key, "eqky") ||
                     !strcmp(key, "eqpx") || !strcmp(key, "eqpy") || !strcmp(key, "eqax") || !strcmp(key, "eqhx") || !strcmp(key, "eqhy") ||
                     !strcmp(key, "eqpt") || !strcmp(key, "eqpc")) {  // (s120b panel tuning)
                int *k = !strcmp(key, "eq2x") ? &g_eq2x : !strcmp(key, "eq2y") ? &g_eq2y : !strcmp(key, "eqtx") ? &g_eqtx : !strcmp(key, "eqvx") ? &g_eqvx :
                         !strcmp(key, "eqky") ? &g_eqky : !strcmp(key, "eqpx") ? &g_eqpx : !strcmp(key, "eqpy") ? &g_eqpy : !strcmp(key, "eqax") ? &g_eqax :
                         !strcmp(key, "eqhx") ? &g_eqhx : !strcmp(key, "eqpt") ? &g_eqpt : !strcmp(key, "eqpc") ? &g_eqpc : &g_eqhy;
                *k = (int)v, LOG("[sora] live: %s %d (Equipment panel text)\n", key, *k);
            }
            else if (!strcmp(key, "wpn")) g_wpn_on = v != 0, LOG("[sora] live: wpn %d (%s)\n", g_wpn_on, g_wpn_on ? "Link's BotW weapon in Sora's hand when equipped" : "off: the Keyblade always");  // (s128)
            else if (!strcmp(key, "wpnrox")) g_wpn_rox = v != 0, LOG("[sora] live: wpnrox %d (%s)\n", g_wpn_rox, g_wpn_rox ? "Roxas swings Link's BotW weapon too" : "off: Roxas = Keyblade only");  // (s137 roxwpn)
            else if (!strcmp(key, "wgzrox") || !strcmp(key, "wrollrox") || !strcmp(key, "wpitchrox") || !strcmp(key, "wyawrox")) {  // (s137 roxwpn: Roxas-only extras on top of wgz / wroll / wpitch / wyaw, GUESS 0)
                int *p = !strcmp(key, "wgzrox") ? &g_wgz_rox : !strcmp(key, "wrollrox") ? &g_wroll_rox : !strcmp(key, "wpitchrox") ? &g_wpitch_rox : &g_wyaw_rox;
                *p = (int)v, LOG("[sora] live: %s %d (Roxas only, added to the Sora value)\n", key, *p);
            }
            else if (!strcmp(key, "wpnfake")) g_wpn_fake = (int)v, LOG("[sora] live: wpnfake %d (%s)\n", g_wpn_fake, g_wpn_fake > 0 && g_wpn_fake <= WPN_N ? WPN_TAB[g_wpn_fake - 1].name : "off: the real equipped weapon");  // (s128)
            else if (!strcmp(key, "eqlook")) g_eqlook = v != 0, LOG("[sora] live: eqlook %d (Equipment page: %s)\n", g_eqlook, g_eqlook ? "vanilla KH2 look" : "the old Customize-style page");
            else if (!strncmp(key, "wgz", 3) && key[3] >= '0' && key[3] <= '2' && !key[4]) g_wgz[key[3] - '0'] = (int)v, LOG("[sora] live: %s %d cm (BotW weapon type %c: model origin past the grip)\n", key, (int)v, key[3]);  // (s128)
            else if (!strcmp(key, "wparm")) g_wpitch_arm = (int)v, LOG("[sora] live: wparm %d deg (monster arms: extra pitch)\n", g_wpitch_arm);  // (s130)
            else if (!strcmp(key, "wyarm")) g_wyaw_arm = (int)v, LOG("[sora] live: wyarm %d deg (monster arms: extra yaw)\n", g_wyaw_arm);  // (s130)
            else if (!strcmp(key, "wrarm")) g_wroll_arm = (int)v, LOG("[sora] live: wrarm %d deg (monster arms: extra roll)\n", g_wroll_arm);  // (s131)
            else if (!strncmp(key, "wsarm", 5) && key[5] >= '0' && key[5] <= '2' && !key[6]) g_warm[key[5] - '0'].s_pct = (int)v, LOG("[sora] live: %s %d pct (%s scale)\n", key, (int)v, g_warm[key[5] - '0'].actor);  // (s131)
            else if (!strncmp(key, "wgxarm", 6) && key[6] >= '0' && key[6] <= '2' && !key[7]) g_warm[key[6] - '0'].gx_cm = (int)v, LOG("[sora] live: %s %d cm (%s grip point along the arm)\n", key, (int)v, g_warm[key[6] - '0'].actor);  // (s131)
            else if (!strcmp(key, "wroll") || !strcmp(key, "wpitch") || !strcmp(key, "wyaw"))  // (s128: the BotW weapon turned in Sora's hand, deg)
                *(key[1] == 'r' ? &g_wroll : key[1] == 'p' ? &g_wpitch : &g_wyaw) = (int)v, LOG("[sora] live: %s %d deg\n", key, (int)v);
            else if (!strcmp(key, "wlead")) g_wlead = (int)v, LOG("[sora] live: wlead %d ms (the BotW weapon led by Sora's speed x this)\n", g_wlead);  // (s129)
            else if (!strcmp(key, "wpace")) g_wpace = (int)v, LOG("[sora] live: wpace %d %% (BotW weapon damage = Attack x KH2 power x this)\n", g_wpace);  // (s128)
            else if (!strcmp(key, "wpnuse")) wpn_menu_pick((int)v), LOG("[sora] live: wpnuse %d (%s)\n", g_wpn_use, g_wpn_use ? "BotW weapon" : "Keyblade");  // (s128)
            else if (!strcmp(key, "mfkb")) g_mfkb = v != 0, LOG("[sora] live: mfkb %d (Master / Final %s)\n", g_mfkb, g_mfkb ? "have their own Keyblade slots" : "use Valor's slot (the pre-s91 look)");
            else if (!strcmp(key, "kbfx")) g_kbfx = v != 0;            // (s89 kbfx: 0 = the old single set)
            else if (!strcmp(key, "kbfxflip")) g_kbfx_vflip = v != 0;  // (s89 kbfx: ribbon texture v 0 at the grip)
            else if (!strcmp(key, "targets")) g_targets_on = v != 0;
            else if (!strcmp(key, "ragofs")) g_rag_ofs = v;
            else if (!strcmp(key, "puppetev")) g_puppet_ev = v != 0;
            else if (ul_set_key(key, v)) ;  // (s89 kb: kball, kbown, beasts, ullog)
            else if (dbg_set_key(key, v)) ;  // (s90: debugall)
            else if (dbm_set_key(key, v)) ;  // (s140 dbgmenu: dbmlv / dbmkb / dbmforms / dbmsmn / dbmoff / dbmlog)
            else if (!strcmp(key, "lflash")) g_lflash = v != 0;  // (s87c link flash: 0 = Link shown in death / loads as before)
            else if (!strcmp(key, "evzero")) g_ev_zero = v;
            else if (!strcmp(key, "evfreeze")) g_evfreeze = v != 0, LOG("[sora] chestev: live evfreeze %d\n", g_evfreeze);
            else if (!strcmp(key, "evrec")) g_evrec = v != 0, LOG("[sora] chestev: live evrec %d\n", g_evrec);
            else if (!strcmp(key, "evcam")) g_evcam = v;
            else if (!strcmp(key, "evang")) g_evang = v;
            else if (!strcmp(key, "evon")) g_evon = v;
            else if (!strcmp(key, "evoff")) g_evoff = v;
            else if (!strcmp(key, "evmin")) g_evmin = v;
            else if (!strcmp(key, "evzero4a")) g_ev_zero4a = v;
            else if (!strcmp(key, "deadpup")) g_puppet_dead = v != 0;
            else if (!strcmp(key, "kh2go")) g_kh2go = v != 0;     // (s59 KH2 Game Over)
            else if (!strcmp(key, "gohold")) g_go_hold = v;
            else if (!strcmp(key, "goblock")) g_go_block = v;
            else if (!strcmp(key, "autocont")) g_autocont = v;
            else if (!strcmp(key, "gofade")) g_go_fade = v;
            else if (!strcmp(key, "godist")) g_go_dist = v;
            else if (!strcmp(key, "goel")) g_go_el = v;
            else if (!strcmp(key, "goyaw")) g_go_yaw = v;
            else if (!strcmp(key, "gospin")) g_go_spin = v;
            else if (!strcmp(key, "gofov")) g_go_fov = v;
            else if (!strcmp(key, "gosy")) g_go_sy = v;
            else if (!strcmp(key, "goy0")) g_go_y0 = v;
            else if (!strcmp(key, "gokb")) g_go_kb = v != 0;
            else if (!strcmp(key, "golog")) g_go_log = v != 0;
            else if (light_set_key(key, v)) ;  // (s121 a1 light: light / lightstyle / lightamt / lightfloor / lighthour / lighthm / lightyaw / lightlog)
            else if (!strcmp(key, "wtint")) g_wtint = v != 0;
            else if (!strcmp(key, "brk")) g_brk_on = v != 0;          // (s55 breakables)
            else if (!strcmp(key, "brkk")) g_brk_k = v / 100.0f;
            else if (!strcmp(key, "brkltn")) g_brk_ltn = v != 0;
            else if (!strcmp(key, "tree")) g_tree_on = v != 0;        // (s122 world)
            else if (!strcmp(key, "treek")) g_tree_k = v;
            else if (!strcmp(key, "treefin")) g_tree_fin = v;
            else if (!strcmp(key, "treer")) g_tree_r = v / 100.0f;
            else if (!strcmp(key, "treeh")) g_tree_h = v / 100.0f;
            else if (!strcmp(key, "stasis")) g_st_on = v != 0;
            else if (!strcmp(key, "stasisfin")) g_st_fin = v;
            else if (!strcmp(key, "stasisfl")) g_st_fl = v;
            else if (!strcmp(key, "stasisr")) g_st_r = v / 100.0f;
            else if (!strcmp(key, "stasish")) g_st_h = v / 100.0f;
            else if (!strcmp(key, "worlddiag")) g_world_diag = v;
            else if (!strcmp(key, "fireworld")) g_fire_world = v;      // (s59 fire world)
            else if (!strcmp(key, "hurtlog")) g_hurt_log = v != 0;     // (s176 [hurt] lines)
            else if (!strcmp(key, "fireburn")) g_fire_burn = v != 0;
            else if (!strcmp(key, "firespk")) g_fire_spk = v;
            else if (!strcmp(key, "firespkr")) g_fire_spk_r = v / 100.0f;
            else if (!strcmp(key, "firespkl")) g_fire_spk_life = (float)v;
            else if (!strcmp(key, "firespkat")) g_fire_spk_at = v != 0;
            else if (!strcmp(key, "firespkoy")) g_fire_spk_y[0] = v / 100.0f;
            else if (!strcmp(key, "firespkey")) g_fire_spk_y[1] = v / 100.0f;
            else if (!strcmp(key, "wofloor")) g_wo_floor_cm = (float)v;
            else if (!strcmp(key, "woguard")) g_wo_guard = v;
            else if (!strcmp(key, "wosnap")) g_wo_snap = v != 0;
            else if (!strcmp(key, "firer")) g_fire_obj_r = v / 100.0f;
            else if (!strcmp(key, "firebr")) g_fire_burst_r = v / 100.0f;
            else if (!strcmp(key, "firebn")) g_fire_burst_n = v;
            else if (!strcmp(key, "wtsrc")) g_wtsrc = v != 0;
            else if (!strcmp(key, "wthide")) g_wt_hide = v;
            else if (!strcmp(key, "wtr")) g_wt_rgb[0] = v;
            else if (!strcmp(key, "wtg")) g_wt_rgb[1] = v;
            else if (!strcmp(key, "wtb")) g_wt_rgb[2] = v;
            else if (!strcmp(key, "wto0")) g_wt_out0 = v;
            else if (!strcmp(key, "wto1")) g_wt_out1 = v;
            else if (!strcmp(key, "wti0")) g_wt_in0 = v;
            else if (!strcmp(key, "wti1")) g_wt_in1 = v;
            else if (!strcmp(key, "rag")) g_rag_on = v != 0;
            else if (!strcmp(key, "linkface")) g_linkface = v != 0;  // (s87c link face)
            else if (!strcmp(key, "getup")) g_rag_getup = v != 0;
            else if (!strcmp(key, "puppet")) g_puppet = v != 0;
            else if (!strcmp(key, "roxpup")) g_roxpup = v != 0, LOG("[roxas] roxpup %d (Roxas %s in Link's own states)\n", g_roxpup, g_roxpup ? "puppets Link's pose" : "has no puppet: BotW's Link shows");
            else if (!strcmp(key, "mkpup")) g_mkpup = v != 0, LOG("[mickey] mkpup %d (Mickey %s in Link's own states)\n", g_mkpup, g_mkpup ? "puppets Link's pose" : "has no puppet: BotW's Link shows");
            else if (!strcmp(key, "mkswim")) g_mkswim = v, LOG("[mickey] mkswim %d (pelvis height while swimming, percent of Link's)\n", v);  // (s118)
            else if (!strcmp(key, "mkysc")) g_mkysc = v, LOG("[mickey] mkysc %d (pelvis height scale %s)\n", v, v < 0 ? "auto = root height ratio" : "percent of Link's");
            else if (!strcmp(key, "handpose")) g_handpose = v < 0 ? 0 : v > 2 ? 2 : v;  // (s102 handpose)
            else if (!strcmp(key, "handsrc")) g_handsrc = v < 0 ? 0 : v > 2 ? 2 : v;
            else if (!strcmp(key, "handmot")) g_handmot = v < 0 ? 0 : v;
            else if (!strcmp(key, "handfr")) g_handfr = v < 0 ? 0 : v;
            else if (!strcmp(key, "handflip")) g_handflip = v < 0 ? 0 : v > 4 ? 4 : v, LOG("[sora] handflip %d (0 old, 1 mirror the copied hand [default], 2 turn over both, 3 right only, 4 left only)\n", g_handflip);
            else if (!strcmp(key, "handmir")) g_handmir = v < -1 ? -1 : v > 1 ? 1 : v;
            else if (!strcmp(key, "handlog")) g_handlog = v != 0;
            else if (!strcmp(key, "bedpose")) g_bedpose = v < 0 ? 0 : v > 2 ? 2 : v;  // (s94 bed)
            else if (!strcmp(key, "bedr")) g_bed_r = v < 1 ? 1 : v > 200 ? 200 : v;
            else if (!strcmp(key, "bedup")) g_bed_up = v < 1 ? 1 : v > 600 ? 600 : v;
            else if (!strcmp(key, "bedlog")) g_bed_log = v != 0;
            else if (!strcmp(key, "wake")) g_wake = v < 0 ? 0 : v > 3 ? 3 : v;  // (s99 b14-bed)
            else if (!strcmp(key, "wakedx")) g_wake_dx = v < 1 ? 1 : v > 600 ? 600 : v;
            else if (!strcmp(key, "wakelog")) g_wake_log = v < 1 ? 1 : v > 6000 ? 6000 : v;
            else if (!strcmp(key, "wakemax")) g_wake_max = v < 60 ? 60 : v > 60000 ? 60000 : v;
            else if (party_set(key, v)) {}  // (s54 party3: party plog pray pfwd pslot pwarp pkdr plie pblow)
            else if (!strcmp(key, "climb")) g_climb = v != 0;
            else if (!strcmp(key, "gclimb")) g_gclimb = v != 0;
            else if (!strcmp(key, "climbair")) g_climb_air_ok = v != 0, LOG("[sora] climbair %d (%s)\n", g_climb_air_ok, g_climb_air_ok ? "old: a climb try also in the air" : "no climb try in the air");
            else if (!strcmp(key, "glideclimb")) g_glideclimb = v < 0 ? 0 : v > 2 ? 2 : v;  // (s88 b3-climb)
            else if (!strcmp(key, "bdodge")) g_bdodge = v != 0, LOG("[sora] base Aerial Dodge (tap B) %s\n", g_bdodge ? "on" : "off (old: B in the air = glide only)");
            else if (!strcmp(key, "bdodgef")) g_bd_tap = v < 0 ? 0 : v > 30 ? 30 : v, LOG("[sora] base dodge tap window: %d KH frames (60 Hz)\n", g_bd_tap);
            else if (!strcmp(key, "gclholdb")) g_gcl_holdb = v != 0, g_gcl_eatb = 0;  // (s89b: held glide B kept from BotW after a grab)
            else if (!strcmp(key, "kickup")) g_kickup = v != 0;  // (s88 b3-climb)
            else if (!strcmp(key, "wheel")) g_wheel = v != 0;
            else if (!strcmp(key, "hearts")) g_hearts_hide = v != 0, LOG("[sora] hearts %s\n", v ? "hidden (BotW's top-left hearts)" : "shown (BotW's own)");
            else if (!strcmp(key, "enemyhud")) g_enemy_left = v != 0, LOG("[sora] enemyhud %s\n", v ? "top-left" : "top-right (old)");
            else if (!strcmp(key, "enx")) g_enx = v, LOG("[sora] enx %d (enemy gauge left end, HUD units; 0 = 24)\n", v);
            else if (!strcmp(key, "eny")) g_eny = v, LOG("[sora] eny %d (enemy gauge centre line, HUD units; 0 = 22)\n", v);
            else if (!strcmp(key, "wall")) g_wall_on = v != 0, W.on = W.on && v;
            else if (!strcmp(key, "repel")) g_repel = v != 0, R.push = v ? R.push : 0;
            else if (!strcmp(key, "wpnvis")) g_wpnvis = v != 0, LOG("[sora] live: wpnvis %d\n", g_wpnvis);  // (s128b)
            else if (!strcmp(key, "slvis")) g_slvis = v != 0, LOG("[sora] slvis %d: %s\n", g_slvis, g_slvis ? "Link's view bit 0 kept while the Slate is shown (Slate drawn)" : "old hide (Slate invisible)");  // (s113)
            else if (!strcmp(key, "showlink")) g_show_link = v != 0, hide_link(!g_show_link), LOG("[sora] showlink %d (debug: Link + his gear drawn too)\n", g_show_link);  // (s99)
            else if (!strcmp(key, "bigair")) g_big_air = v != 0, LOG("[sora] bigair %d\n", g_big_air);
            else if (!strcmp(key, "fsmooth")) g_fsmooth = v != 0, LOG("[sora] fsmooth %d: %s\n", g_fsmooth, g_fsmooth ? "bones slerped between the baked frames" : "the exe's matrix lerp (old)");  // (s93)
            else if (!strcmp(key, "ffps")) ffps_set(v != 0);  // (s91 Final fps fix)
            else if (!strcmp(key, "drawsync") || !strcmp(key, "drawoff") || !strcmp(key, "latdiag") || !strcmp(key, "gearpark")) {
                // (s114) exe setters looked up by name (an older exe: logged, nothing else)
                void (*fs)(int, int, int) = (void (*)(int, int, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_set_sync");
                void (*fg)(int) = (void (*)(int))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_gear_park");
                if (!strcmp(key, "gearpark")) { if (fg) fg(v); }
                else if (fs) fs(!strcmp(key, "drawsync") ? v : -1, !strcmp(key, "drawoff") ? v : -99, !strcmp(key, "latdiag") ? v : -1);
                if (!(strcmp(key, "gearpark") ? (void *)fs : (void *)fg)) LOG("[sora] %s: not in this exe (NEEDS EXE RELINK s114)\n", key);
            }
            else if (!strcmp(key, "drawperf")) {  // (mp2-perf) remote avatar perf cuts: bits 1 C1, 2 C2, 4 C3, 8 C4, +16 = verify once
                void (*fp)(int) = (void (*)(int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_set_perf");
                static bool warned;
                if (fp) fp(v);
                else if (!warned) warned = true, LOG("[sora] drawperf: not in this exe (NEEDS EXE RELINK mp2-perf)\n");
            }
            else if (!strcmp(key, "animdiag")) g_animdiag = v != 0, LOG("[sora] animdiag %d: %s\n", g_animdiag, g_animdiag ? "logging the driver call gaps + feet cadence (\"[anim60]\" lines)" : "off");  // (s113 anim60)
            else if (!strcmp(key, "ffin")) ffin_set(v != 0);  // (s92 Final finishers: Interval re-hits + radius + body windows)
            else if (!strcmp(key, "bigbody")) g_big_body = v != 0;  // (s87 Guardian body cylinders)
            else if (!strcmp(key, "fmdie")) g_fm_keep_dying = v != 0;  // (s87 form body through the death pose)
            else if (!strcmp(key, "rgap")) g_repel_gap = v / 100.0f;
            else if (!strcmp(key, "rstay")) g_repel_stay = v / 100.0f;
            else if (!strcmp(key, "rray")) g_rray = v != 0;
            else if (!strcmp(key, "guard")) g_guard = v != 0, LOG("[sora] guard %s\n", v ? "on (Square, stick neutral)" : "off (Square always rolls)");
            else if (!strcmp(key, "guardgate")) g_guard_gate = v != 0, LOG("[sora] guardgate %d (%s)\n", v, v ? "Guard needs the Guard ability equipped (Abilities page); else Square rolls" : "Guard always available");
            else if (!strcmp(key, "guardinv")) g_guard_inv = v, LOG("[sora] guardinv %d (post-block invulnerable frames: 0 none, 1 enemy hits, 2 all)\n", v);
            else if (!strcmp(key, "bmswallow")) { g_bmswallow = v != 0; if (!v) g_camp_swallow = 0; LOG("[sora] bmswallow %d\n", g_bmswallow); }
            else if (!strcmp(key, "formfx")) g_formfx_on = v != 0, LOG("[sora] formfx %d (Limit / Final / Anti entry fx + Anti entry SE; 0 = off)\n", v);
            else if (!strcmp(key, "ffx")) ffx_set(v, 0);   // (s93 b7-ffx)
            else if (!strcmp(key, "fairfx")) ffx_set_air(v);  // (s95 fairfx: 0 = Final jump / fall / glide without Keyblade ribbons)
            else if (!strcmp(key, "lafx")) ffx_set(v, 1);  // (s93 b7-ffx)
            else if (!strcmp(key, "roxsfx")) g_roxsfx = v != 0, LOG("[sora] roxsfx %d (Roxas: his own KH2 sounds + ribbon windows + guard fx; 0 = silent, Sora's ribbon windows)\n", v);
            else if (!strcmp(key, "formse")) g_formse_on = v != 0, LOG("[sora] formse %d (form motions play the form's own mset SE triggers; 0 = the base Sora's)\n", v);
            else if (!strcmp(key, "kbpop")) g_kbpop = v != 0, LOG("[sora] kbpop %d (Keyblade obtained panel; 0 = log line only)\n", v);
            else if (!strcmp(key, "parry")) g_parry = v != 0, LOG("[sora] parry %s\n", v ? "on (Guard sends Guardian beams back)" : "off (beams explode on Sora)");
            else if (!strcmp(key, "parrylink")) { g_parry_link = v != 0; if (!g_parry_link) parry_link_clear("parrylink 0"); LOG("[sora] parrylink %s\n", v ? "on (Guard window raises Link's isGuard/isGuardJust: BotW reflects the beam)" : "off (Link's guard flags untouched)"); }
            else if (!strcmp(key, "parryfront")) g_parry_front = v, LOG("[sora] parryfront %d (0 none, 1 beam flies at his front, 2 nearest enemy in front)\n", v);
            else if (!strcmp(key, "parryrad")) g_parry_rad = v, LOG("[sora] parryrad %d cm\n", v);
            else if (!strcmp(key, "guardfrom")) g_guard_from = v < 0 ? 0 : v, LOG("[sora] guardfrom %d (guard window opens at GFR %d; 6 = KH2)\n", v, g_guard_from);
            else if (!strcmp(key, "rwny")) g_ray_wall_ny = v / 100.0f;
            else if (!strcmp(key, "fray")) g_fray = v != 0;
            else if (!strcmp(key, "slope")) g_slope = v != 0;  // (s64 slope follow)
            else if (!strcmp(key, "lblend")) g_lblend = v != 0;  // (s64 idle / walk / run blend)
            else if (!strcmp(key, "eblend")) g_eblend = v != 0;  // (s66 swing / roll end blend)
            else if (!strcmp(key, "lphase")) g_lphase = v != 0;  // (s66 walk <-> run keeps the phase)
            else if (!strcmp(key, "slopemax")) g_slope_max = (float)v;
            else if (!strcmp(key, "slopeahead")) g_slope_ahead = v / 100.0f;
            else if (!strcmp(key, "slopelog")) g_slope_log = v != 0;
            else if (!strcmp(key, "fdown")) g_fdown = v != 0, FR.valid = FR.valid && v;  // (s54 floor ray, all users)
            else if (!strcmp(key, "fevery")) g_fevery = v < 1 ? 1 : v;
            else if (!strcmp(key, "fmove")) g_fmove = v / 100.0f;
            else if (!strcmp(key, "flen")) g_flen = v < 1 ? 1.0f : (float)v;
            else if (!strcmp(key, "fnear")) g_fnear_cm = (float)v;
            else if (!strcmp(key, "fglide")) g_fglide = v != 0;
            else if (!strcmp(key, "fland")) g_fland = v != 0;
            else if (!strcmp(key, "flandcm")) g_fland_cm = (float)v;
            else if (!strcmp(key, "fveto")) g_fveto = v < 0 ? 0 : v;
            else if (!strcmp(key, "fvetocm")) g_fveto_cm = (float)v;
            else if (!strcmp(key, "fshadow")) g_fshadow = v != 0;
            else if (!strcmp(key, "flog")) g_flog = v != 0;
            else if (!strcmp(key, "wratio")) g_wall_ratio = v / 100.0f;
            else if (!strcmp(key, "wgap")) g_wall_gap = v / 100.0f;
            else if (!strcmp(key, "wstick")) g_wstick = v != 0;
            else if (!strcmp(key, "zl")) g_zl_pass = v != 0;
            else if (!strcmp(key, "head")) g_rag_head = v != 0;
            else if (!strcmp(key, "kb")) g_puppet_kb = v != 0;
            else if (!strcmp(key, "ride") || !strcmp(key, "wind")) ride_wind_set(key[0] == 'r', v != 0);  // s88: platform carry / wind push
            else if (!strcmp(key, "glide")) g_glide_on = v != 0;
            else if (!strcmp(key, "glidestam")) g_glidestam = v < 0 ? 0 : v > 2 ? 2 : v;
            else if (!strcmp(key, "dodgestam")) g_dodgestam = v < 0 ? 0 : v > 1000 ? 1000 : v, LOG("[sora] Aerial Dodge stamina cost %d (1000 a wheel, 0 = free)\n", g_dodgestam);
            else if (!strcmp(key, "trailsmooth")) {  // (s97 b11-trail: the exe's ribbon smoothing; 0 old, 1 adaptive, 2..8 fixed)
                void (*f)(int) = (void (*)(int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_fx_trail_smooth");
                if (f) f(v), LOG("[sora] ribbon smoothing %d (0 = straight, 1 = adaptive, 2..8 sub-steps)\n", v);
                else LOG("[sora] trailsmooth: this exe has no sora_fx_trail_smooth (before s97: NEEDS EXE RELINK)\n");
            }
            else if (!strcmp(key, "leafcut")) {  // (s102 #12 leaves: scene depth this close to the camera (cm) doesn't hide the party; 0 = old)
                void (*f)(int) = (void (*)(int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_near_cut");
                if (f) f(v), LOG("[sora] leafcut %d cm (camera-near leaves don't hide Sora + party; 0 = old)\n", v);
                else LOG("[sora] leafcut: this exe has no sora_draw_near_cut (before s102: NEEDS EXE RELINK)\n");
            }
            else if (!strcmp(key, "leafmode") || !strcmp(key, "leafk") || !strcmp(key, "leafthr") || !strcmp(key, "leafrange")) {
                // (s105 leaves v2: 0 = s102 flat near cut (leafcut), 1 = neighbourhood test with the near cut off;
                //  leafk = tap spread in tenths of a texel (10), leafthr = percent of the 16 taps that must hide Sora (90),
                //  leafrange = only occluders nearer than this many cm get the test (1000))
                int which = !strcmp(key, "leafmode") ? 0 : !strcmp(key, "leafk") ? 1 : !strcmp(key, "leafthr") ? 2 : 3;
                void (*f)(int, int) = (void (*)(int, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_leaf_cfg");
                if (f) f(which, v), LOG("[sora] %s %d\n", key, v);
                else LOG("[sora] %s: this exe has no sora_draw_leaf_cfg (before s105: NEEDS EXE RELINK)\n", key);
            }
            else if (!strcmp(key, "depthdbg")) {  // (s105) Sora painted grey = scene depth / his depth * 0.5, no restart needed
                void (*f)(int) = (void (*)(int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_depth_debug");
                if (f) f(v), LOG("[sora] depthdbg %d\n", v);
                else LOG("[sora] depthdbg: this exe has no sora_draw_depth_debug (before s105: NEEDS EXE RELINK)\n");
            }
            else if (!strcmp(key, "rollstam")) g_rollstam = v < 0 ? 0 : v > 1000 ? 1000 : v, LOG("[sora] Dodge Roll stamina cost %d (1000 a wheel, 0 = free)\n", g_rollstam);
            else if (!strcmp(key, "dodgeempty")) g_dodgeempty = v != 0, LOG("[sora] Aerial Dodge with no stamina: %s\n", g_dodgeempty ? "refused" : "allowed");
            else if (!strcmp(key, "stam")) glidestam_set(v);
            else if (!strcmp(key, "tri")) g_tri_pickup = v != 0;
            else if (!strcmp(key, "xwall")) g_xwall_cm = v;
            else if (!strcmp(key, "xwallhi")) g_xwall_hi_cm = v < 0 ? 0 : v, LOG("[sora] X press: high ray %d cm (0 = off)\n", g_xwall_hi_cm);
            else if (!strcmp(key, "noxlen")) g_nox_len = v < 16 ? 16 : v;
            else if (!strcmp(key, "noxstick")) g_nox_stick = v < 0 ? 0 : v > 100 ? 100 : v;  // (s89b pick-up window stick cap %)
            else if (!strcmp(key, "cureq")) g_cure_q = v < 0 ? 0 : v;
            else if (!strcmp(key, "sptier2")) g_sp_lv2 = v < 1 ? 1 : v > 99 ? 99 : v;  // (s87c spell tiers)
            else if (!strcmp(key, "sptier3")) g_sp_lv3 = v < 1 ? 1 : v > 99 ? 99 : v;
            else if (!strcmp(key, "sptier")) g_sp_force = v < 0 ? 0 : v > 3 ? 3 : v;
            else if (!strcmp(key, "sptiers")) g_sp_tiers = v != 0;
            else if (!strcmp(key, "spdg")) g_sp_dg = v != 0, LOG("[sora] spell tier Drive rows %s\n", g_sp_dg ? "on" : "off (Lv1 rows)");  // (s88)
            else if (!strcmp(key, "spfx")) g_sp_fx = v != 0, LOG("[sora] spell tier fx %s\n", g_sp_fx ? "on (Fira / Firaga elements when fx_s88.bin is installed)" : "off (Lv1 elements)");  // (s88)
            else if (!strcmp(key, "splabel")) g_sp_label = v != 0, LOG("[sora] menu spell tier labels %s\n", g_sp_label ? "on" : "off");  // (s88)
            else if (!strcmp(key, "probe")) g_probe_on = v;  // (1: at a no-wall triangle, 2: at a kill - s86)
            else if (!strcmp(key, "itemscr")) g_itemscr = v != 0;
            else if (!strcmp(key, "frzlog")) g_frz_log = v != 0;
            else if (!strcmp(key, "menupass")) g_menu_pass = v != 0;
            else if (is_live_set(key, v)) {}
            else if (net_live_set(key, v)) {}  // (mp: net / netloop / netport)  // (s102 invsora / isx / isy / iss / isface / isyaw / ismask / isopa / isreq)
            else if (!strcmp(key, "invdiag")) invdiag_set(v);  // (s100 glitch #62: inventory-screen Link diag)
            else if (!strcmp(key, "savepic")) g_savepic = v != 0, LOG("[sora] savepic %s\n", g_savepic ? "on (pose held at the pause start for the save picture)" : "off (pose dropped while paused)");
            else if (!strcmp(key, "savepicn")) g_savepic_n = v < 0 ? 0 : v > 600 ? 600 : v, LOG("[sora] savepic: pose held %d presents into a pause\n", g_savepic_n);
            else if (!strcmp(key, "glidelv")) g_glide_lv = v < 1 ? 1 : v > 5 ? 5 : v;
            else if (!strcmp(key, "aironce")) g_air_once = v != 0;
            else if (!strcmp(key, "airmagic")) g_air_magic = v != 0;  // (s68 air casts)
            else if (!strcmp(key, "gcancel")) g_glide_cancel = (uint64_t)v;
            else if (!strcmp(key, "mp")) MP.mp = (float)v, MP.charge = MP.charge_total = 0;
            else if (!strcmp(key, "mpcharge")) g_mp_charge_time = (float)v;
            else if (!strcmp(key, "cure")) g_cure_pct = (float)v;
            else if (!strcmp(key, "hudx")) g_hud_place[0] = v, hud_place_send();  // (s57 party HUD)
            else if (!strcmp(key, "hudy")) g_hud_place[1] = v, hud_place_send();
            else if (dg_live_set(key, v)) {}  // (s67 drive: drive / drivept / drivemul / drivelog)
            else if (rc_live_set(key, v)) {}  // (s131 react: react / reactauto / reacttest / reactlog)
            else if (meal_live_set(key, v)) {}  // (s93 meals: meals / mealq / meallog)
            else if (camp_live_set(key, v)) {}  // (s78 camp: camp / campopen / camplog)
            else if (vl_live_set(key, v)) {}  // (s70 Valor EXP: vexp / vgrow / vexpmul / vlvup)
            else if (su_live_set(key, v)) {}  // (s148 Summon EXP: sumexp / sumcap / sumrefill / sumlog / sumlvup)
            else if (wl_live_set(key, v)) {}  // (s75 Wisdom EXP: wexp / wexpmul / wlvup)
            else if (ll_live_set(key, v)) {}  // (s89 Limit EXP: lexp / lexpmul / llvup)
            else if (cx_live_set(key, v)) {}  // (s106 b18-chestanim: chestprev / chestanim)
            else if (orb_live_set(key, v)) {}  // (s131 orbs: orb orbfx orbmunny orbmag orblife orbscale orbhp orbmp orbpt orblog)
            else if (sv_live_set(key, v)) {}  // (s131 savept: savept savefx saver saveoff savelog savehere)
            else if (smn_live_set(key, v)) {}  // (s137 smenu: smenu / sown / sgauge)
            else if (sm_live_set(key, v)) {}   // (s136 summon: summon / scall (ONE-SHOT: remove after it fired) / sdone / sdmg / sstr / sbars / sendcall / slog)
            else if (skt_live_set(key, v)) {}  // (s110 skate: skate / skatex y z yaw here home / skmax skacc skbrake skfric skturn skr / skatelog)
            else if (mark_live_set(key, v)) {}  // (s111 mark / markse)
            else if (mk_live_set(key, v)) {}  // (s109 b22-mickey: mickey / mkforce / mkheal / mkany / mkchance / mkcharge / mkdmg / mklog)
            else if (ca_live_set(key, v)) {}  // (s102 b16-chestabil: chestabil / chestlog / chestfx / chestr / chestgive / chestreset)
            else if (sh_live_set(key, v)) {}  // (s94 b8-slatehip: slatehip / shsnap / shx shy shz / shyaw shpitch shroll / shlog)
            else if (sk_live_set(key, v)) {}  // (s93 b7-runes: slate / slatepass / slaterec / slateforce / slateend ...)
            else if (lm_live_set(key, v)) {}  // (s89 Limit commands: lmauto / lmshotdmg / lmraiddmg / lmlog / limit)
            else if (ml_live_set(key, v)) {}  // (s89 Master EXP + moves: fmmaster / mexp / mexpmul / mlvup / mdodge / mbody)
            else if (fl_live_set(key, v)) {}  // (s89 b4-final Final EXP: fexp / fexpmul / flvup)
            else if (fm_live_set(key, v)) {}  // (s68 drive: form / fmswap / fmtime)
            else if (sl_live_set(key, v)) {}  // (s54 levels: lv / exp / lvdmg / lvlog / khdmg / lvwin)
            else if (dream_live_set(key, v)) {}  // (s54 dream: dream / giveup / dreamui / dreamauto / dreamgu / dreamwait)
            else if (ah_live_set(key, v)) {}  // (s55 airhold: ah / ahlog / ahhop / ahblow / ahheld / ahhoph / ahthr / ahvmax / ahair)
            else if (!strcmp(key, "flick")) g_flick = v != 0;
            else if (!strcmp(key, "lockcam")) g_lockcam = v != 0;
            else if (!strcmp(key, "camgain")) g_cam_gain = v / 10.0f;
            else if (!strcmp(key, "camlead")) g_cam_lead = (float)v;
            else if (!strcmp(key, "follow")) g_follow = v != 0;
            else if (!strcmp(key, "fgain")) g_follow_gain = v / 10.0f;
            else if (!strcmp(key, "autor")) g_auto_range = (float)v;
            else if (!strcmp(key, "airreach")) g_air_reach = (float)v;  // (s66 jump-up reach on enemies in the air)
            else if (!strcmp(key, "iconr")) g_icon_range = (float)v;  // (s65 combo feel)
            else if (!strcmp(key, "rwfall")) g_rw_fall = v != 0;
            else if (!strcmp(key, "wocap")) g_wo_cap = v != 0;
            else if (!strcmp(key, "finbig")) g_fin_big = v != 0;
            else if (!strcmp(key, "sparkat")) g_spark_at = v != 0;
            else if (!strcmp(key, "swingturn")) g_swing_turn = v != 0;
            else if (!strcmp(key, "latehit")) g_late_hit = v != 0;
            else if (!strcmp(key, "stickround")) g_stick_round = v != 0;
            else if (!strcmp(key, "blizhome")) g_bliz_home = v;  // (s59)
            else if (!strcmp(key, "blizr")) g_bliz_r = (float)v;
            else if (!strcmp(key, "blizturn")) g_bliz_turn = v / 1000.0f;
            else if (!strcmp(key, "ghost")) g_swim_ghost = v;
            else if (mg_live_set(key, v)) {}  // s133 magic (magnet 0 / reflect 0 / spdiag 1 / mgsize N)
            else if (pl_live_set(key, v)) {}  // s133 limits: limitparty / lpdiag / lpauto / lpforce (lpforce is ONE-SHOT: remove it from live_test.txt after it fired)
            LOG("[sora] live: %s %d\n", key, v);
        } else if (sscanf(ln, "set stop %f", &x) == 1) {
            g_live_stop = x, LOG("[sora] live: stop %.2f\n", x);
        } else if (!strcmp(ln, "snap on")) {
            if (!g_snap) g_snap = fopen(sora_path("D:/SoraBotW/run/snap.bin"), "wb");
            LOG("[sora] live: snap %s%s%s\n", g_snap ? "on (" : "FAILED to open", g_snap ? sora_path("D:/SoraBotW/run/snap.bin") : "", g_snap ? ")" : "");
        } else if (!strcmp(ln, "snap off")) {
            if (g_snap) fclose(g_snap), g_snap = NULL;
            LOG("[sora] live: snap off\n");
        } else if (!strcmp(ln, "unit dump")) {
            unit_dump();
        } else if (!strcmp(ln, "unit off")) {
            g_upoke.on = 0, LOG("[sora] live: unit poke off\n");
        } else if (sscanf(ln, "unit %i %i %i", &g_upoke.off, &g_upoke.size, &v) == 3) {
            g_upoke.val = (uint32_t)v, g_upoke.on = g_upoke.off >= 0 && g_upoke.off < 0x70;
            LOG("[sora] live: unit poke +%#x size %d = %#x (%s)\n", g_upoke.off, g_upoke.size, g_upoke.val, g_upoke.on ? "on" : "bad offset");
        } else if (net_live_line(ln)) {  // (mp: net peer <ip>|off, net name <s>, net diag)
        } else if (!strncmp(ln, "input ", 6)) {
            g_nscript = 0;
            g_script_base = present + 30;
            script_parse(ln + 6);
        }
    }
}

// Loading-screen probe (session 28): a System > Load keeps the old Link standing with life and no pause (session
// 27), so look for the screen itself: the UI ScreenMgr's live screens (framepace.c's log_screens, same addresses)
// and ksys::VFR's interval override (+0x29, "stage loading: single steps"). The VFR isn't exported by the exe:
// found once by a scan of the guest's globals for a pointer to an object with framepace's ratio at +0x30 and +0x3c.
#define SCREEN_MGR_PP (sora_sites()->screen_mgr_got)
#define GV(T, base, off) (*(const volatile T *)(uintptr_t)((base) + (off)))
#include "sora_chestev.h"  // (s94 b8-chestev: the camp-cleared chest camera event; S94_HELPER_CHESTEV.md)
#include "sora_chestabil.h"  // (s102 b16-chestabil: treasure chests teach Sora KH2 support abilities; S102_HELPER_CHESTABIL.md)
#include "sora_chestanim.h"  // (s106 b18-chestanim slice 1: chestprev preview + chestanim 2 recon log; S106_HELPER_CHESTANIM1.md)
static uint64_t g_vfr;
static void vfr_find(void) {
    static int tries;
    if (g_vfr || tries >= 3) return;
    tries++;
    float r = framepace_game_ratio();
    int n = 0;
    ULONGLONG t0 = GetTickCount64();
    for (uint64_t a = sora_sites()->main_data; a < sora_sites()->main_end; a += 8) {  // (S61: was 0x7001b00000..0x7001e00000, 1.8.2)
        if (!(a & 0xfff) && GetTickCount64() - t0 > 300) { LOG("[sora] vfr scan: out of time at %#llx\n", (unsigned long long)a); break; }
        if (!rd_ok(a, 8)) { a |= 0xfff; a -= 7; continue; }
        uint64_t v = GV(uint64_t, a, 0);
        if ((v & 3) || !rd_ok(v, 0x40)) continue;
        float r30 = GV(float, v, 0x30), r3c = GV(float, v, 0x3c);
        uint32_t iv = GV(uint32_t, v, 0x34);
        uint8_t c = GV(uint8_t, v, 0x28), o = GV(uint8_t, v, 0x29);
        if (r30 == r && r3c == r && iv >= 1 && iv <= 3 && c <= 1 && o <= 1) {
            LOG("[sora] vfr candidate %d: [%#llx] = %#llx (ratio %.4f interval %u override %u)\n", n, (unsigned long long)a,
                (unsigned long long)v, r, iv, o);
            if (!n) g_vfr = v;
            if (++n >= 8) break;
        }
    }
    if (!n) LOG("[sora] vfr: no candidate (try %d, ratio %.4f)\n", tries, r);
}

// Screen 0x55 exists only while a load's tip card is up (load28a: from 57 presents after the menu closed until
// Link's state word reads 0x1): Sora + HUD off for it, like a Game Over. Those first ~57 presents (black fade, the
// card sliding in) have no screen of their own, but the pause ends while the Load confirm dialog (0x38) still exists,
// which a plain unpause never does (shots_s28/b11: Sora + HUD drawn there): latched from then until 0x55 has come
// and gone, or LOAD_WAIT presents if it never comes.
#define SCREEN_LOADING 0x55
#define SCREEN_DIALOG 0x38
#define LOAD_WAIT 240
static bool g_loading;
static uint64_t g_load_end;  // present the last load ended (s54 warp tint; set here: the frame code returns early in loads)
// (s87c link flash, user: "i see links body before the gameover screen / for a second after Continue / when entering or
// exiting a shrine") the non-puppet branch below showed BotW's Link whenever it wasn't a 0x1 event or the post-Continue
// hold: while dying with no sane bones, between Continue's life and the stand (up_at 0), and in every load that isn't
// a 0x1 warp; the early returns (no body mid-load) left linkpos's g_hide at its last value, and a respawned actor was
// drawn until the first hide. g_lflash (live "set lflash 0" = the old behaviour) keeps him hidden while dead / loading /
// LF_POST presents after a load or a missing body (blank frame beats Link); Link stays shown on purpose only in the
// other puppet-less states (climb etc. with junk bones) - unchanged.
#define LF_POST 90
static uint64_t g_lf_bad;  // present the body was last missing (a respawned / warped actor follows)
static void lflash_early(uint64_t present, bool nobody) {
    if (!g_lflash || !g_active || g_show_link) return;  // (s87c review: SAVAGEWIND_SORA_SHOW_LINK = never hidden)
    if (nobody) g_lf_bad = present;
    if (!g_loading && !nobody) return;
    static uint64_t said;
    linkpos_hide_link(true);
    if (present - said > 300) said = present, LOG("[sora] link flash: hide held (%s) at present %llu\n", nobody ? "no body" : "loading", (unsigned long long)present);
}
static bool screen_on(uint64_t mgr, uint32_t i) {
    uint64_t arr = GV(uint64_t, mgr, 0x30);
    return i < GV(uint32_t, mgr, 0x28) && arr && rd_ok(arr + 8 * i, 8) && GV(uint64_t, arr, 8 * i);
}
static void load_latch(uint64_t mgr, uint64_t present) {
    static uint32_t last_pause;
    static uint64_t since;  // present the latch started (0: off)
    static bool card;       // 0x55 seen since then
    uint32_t pause = GV(uint32_t, PAUSE_STATE, 0);
    bool loading = mgr && screen_on(mgr, SCREEN_LOADING);
    if (last_pause && !pause && mgr && screen_on(mgr, SCREEN_DIALOG) && !since)
        since = present, card = false, LOG("[sora] menu closed into a load at present %llu: Sora + HUD off\n", (unsigned long long)present);
    last_pause = pause;
    if (since) {
        if (loading) card = true;
        else if (card || present - since > LOAD_WAIT)
            LOG("[sora] load over at present %llu (%s)\n", (unsigned long long)present, card ? "card gone" : "no card"), since = 0;
    }
    if (g_loading && !(loading || since)) g_load_end = present;
    g_loading = loading || since;
}

// (s95 b9-savepic) log lines for the manual save picture: pause start / the Save dialog (screen 0x38) coming up while
// paused / pause end, with whether Link is hidden. Prove-it log: sora.log "savepic" lines + SAVAGEWIND_LOG_JPEG=1 ([jpeg] Encode).
// (s95 review) g_savepic_camp: the KH2 camp menu was up in this pause. Its menu party (sora_camp_walk.h: the same three
// actors posed in screen mode) replaced the world poses, and leaving screen mode drops every snapshot
// (sora_draw.c sora_draw_screen(false)), so after the camp's hand-over to BotW's menu there is no world pose left to
// hold: the picture of a save made that way has no party (not covered yet; recon/S95_REVIEW.md).
static bool g_savepic_camp;
// (s96) true while Sora's world pose should stay drawable for BotW's save picture: not paused yet (driving, or the KH2
// camp waiting for BotW's pause), or paused for fewer than g_savepic_n presents in a world. Tracks the pause start.
static bool savepic_hold(uint64_t present) {
    bool paused = *(volatile uint32_t *)(uintptr_t)PAUSE_STATE != 0 && linkpos_world_frame();
    if (!paused) { g_savepic_t0 = 0; return g_savepic != 0; }
    if (!g_savepic_t0) g_savepic_t0 = present ? present : 1;
    bool hold = g_savepic && !g_savepic_camp && present - g_savepic_t0 < (uint64_t)g_savepic_n;
    static uint64_t logged;
    if (!hold && g_savepic && logged != g_savepic_t0)
        logged = g_savepic_t0, LOG("[sora] savepic: pose dropped %llu presents into the pause (savepicn %d)\n",
                                   (unsigned long long)(present - g_savepic_t0), g_savepic_n);
    return hold;
}
static void savepic_note(uint64_t present, bool paused) {
    static bool was_paused, was_dlg;
    uint64_t pp = rd_ok(SCREEN_MGR_PP, 8) ? GV(uint64_t, SCREEN_MGR_PP, 0) : 0;
    uint64_t mgr = pp && rd_ok(pp, 8) ? GV(uint64_t, pp, 0) : 0;
    bool dlg = paused && mgr && rd_ok(mgr, 0x38) && screen_on(mgr, SCREEN_DIALOG);
    if (paused && !was_paused)
        LOG("[sora] savepic: paused at present %llu, pose %s\n", (unsigned long long)present,
            !g_savepic ? "dropped (savepic 0)" : g_savepic_camp ? "already gone (the KH2 menu's party replaced it)" : "held for the save picture (first savepicn presents)");
    if (dlg && !was_dlg)
        LOG("[sora] savepic: dialog (0x38) up while paused at present %llu: a manual save picture is taken about now (pose %s, Link %s)\n",
            (unsigned long long)present, !g_savepic ? "dropped" : "taken at the pause start (s96)",
            g_show_link ? "shown (SHOW_LINK)" : "hidden");
    if (!paused && was_paused)
        LOG("[sora] savepic: pause over at present %llu\n", (unsigned long long)present);
    if (!paused) g_savepic_camp = false;
    was_paused = paused, was_dlg = dlg;
}

// The stamina wheel (s50, recon/STAMINA_WHEEL_RECON.md): the gauge screen's show-check (f_7000c5f90c, vtable slot
// +0x50) hides it while Link's final opacity < 1 - and Link is hidden by opacity 0. Its byte +0x9e != 0 skips that
// gate (both show-checks), so it is set every frame while "set wheel 1" (default; side effects of +0x9e unknown).
#define STAMINA_SHOW_FN (sora_sites()->stamina_show_check)
#define STAMINA_VT (sora_sites()->stamina_gauge_vptr)  // (s50 live scan: the show-check pointer sits at 0x7001b7adc0 = this + 0x50)
static void wheel_tick(uint64_t mgr) {
    static uint64_t scr;
    static uint8_t was = 0xff;
    if (!mgr) return;
    uint32_t n = GV(uint32_t, mgr, 0x28);
    uint64_t arr = GV(uint64_t, mgr, 0x30);
    static uint64_t vscan;  // 0: the heap scan below runs (again 600 frames after the gauge is lost)
    static uint64_t lost;
    if (scr && (!rd_ok(scr, 0xa0) || GV(uint64_t, scr, 0) != STAMINA_VT))
        scr = 0, lost = 1, LOG("[sora] stamina screen gone (a load?)\n");
    if (lost && ++lost > 600) lost = 0, vscan = 0;
    // (s63: a fresh start ran the one scan at the title, before the gauge exists -> 0 found and no wheel all session;
    // now it runs again every 600 frames while not found, 15-31 ms each)
    static uint64_t retry;
    if (!scr && vscan && ++retry > 600) retry = 0, vscan = 0;
    if (!vscan) {  // (s50: where does the show-check sit in a vtable? the recon guessed ~0x7001b7ad70 + 0x50)
        static bool vt_logged;
        vscan = 1;
        for (uint64_t a = sora_sites()->main_data; !vt_logged && a < sora_sites()->main_end; a += 8) {  // (S61: was 0x7001a00000..0x7001d00000, 1.8.2)
            if (!rd_ok(a, 8)) { a |= 0xfff; a -= 7; continue; }
            if (GV(uint64_t, a, 0) == STAMINA_SHOW_FN) LOG("[sora] stamina show-check pointer at %#llx\n", (unsigned long long)a);
        }
        vt_logged = true;
        int hits = 0;  // the heap around the screens (0x72e9.. - 0x72ea..): objects with the gauge's vtable
        ULONGLONG t0 = GetTickCount64();
        for (uint64_t a = 0x72e8000000ull; a < 0x72ec000000ull && hits < 8 && GetTickCount64() - t0 < 2000; a += 8) {
            if (!(a & 0xfff) && !rd_ok(a, 0x1000)) { a += 0xff8; continue; }
            if (GV(uint64_t, a, 0) == STAMINA_VT) {
                hits++;
                LOG("[sora] gauge vtable object at %#llx (+0x9d %u +0x9e %u)\n", (unsigned long long)a, GV(uint8_t, a, 0x9d),
                    GV(uint8_t, a, 0x9e));
                if (!scr) scr = a;
            }
        }
        static int scans;
        if (hits || !scans++) LOG("[sora] gauge heap scan: %d found in %llu ms (scan %d)\n", hits, (unsigned long long)(GetTickCount64() - t0), scans);
    }
    static uint64_t tick;
    if (scr || ++tick % 120) goto write;  // (the field scan below: every 2 s at most while not found)
    for (uint32_t i = 0; !scr && i < n && i < 256 && arr && rd_ok(arr + 8 * i, 8); i++) {
        uint64_t o = GV(uint64_t, arr, 8 * i), vt = o && rd_ok(o, 0xa0) ? GV(uint64_t, o, 0) : 0;
        uint64_t hit = vt && rd_ok(vt + 0x50, 8) && GV(uint64_t, vt, 0x50) == STAMINA_SHOW_FN ? o : 0;
        for (uint32_t f = 8; !hit && o && f < 0x2000 && rd_ok(o + f, 8); f += 8) {  // (or a child the screen holds)
            uint64_t c = GV(uint64_t, o, f);
            if (c > 0x7000000000ull && !(c & 7) && rd_ok(c, 0xa0) && GV(uint64_t, c, 0) == STAMINA_VT)
                hit = c, LOG("[sora] stamina screen is screen %#x's field +%#x\n", i, f);
        }
        if (hit) {
            o = hit, vt = GV(uint64_t, o, 0);
            scr = o;
            if (was == 0xff) was = GV(uint8_t, o, 0x9e);
            LOG("[sora] stamina screen %#x at %#llx vtable %#llx (+0x9d %u +0x9e %u)\n", i, (unsigned long long)o,
                (unsigned long long)vt, GV(uint8_t, o, 0x9d), GV(uint8_t, o, 0x9e));
        }
    }
write:
    if (scr) *(volatile uint8_t *)(uintptr_t)(scr + 0x9e) = g_wheel ? 1 : (was == 0xff ? 0 : was);
}

// ------------------------------------------------------------------ s92: hide BotW's hearts (recon/S92_HELPER_HUD.md)
// BotW 1.9.0 HUD screen "MainScreen_00" (vtable 0x7101cf4120 image = 0x7001cf4120 runtime; its slot 95 = 0x7000e1e1a4, the method that
// finds the layout part "Pa_HeartGauge_00" and keeps the pane at this+0x3638 and the heart-gauge helper at this+0x3630). A ui2d pane
// is skipped by the draw when its visible flag is off; the game's own idiom is `pane+0x58 &= 0xfe` (119 sites) / `|= 1` (57 sites)
// (GUESS: +0x58 bit 0 = Pane visible). Every frame while "set hearts 1": find the MainScreen (a screen of the ScreenMgr or a field
// of one), clear that bit. Nothing else of the HUD is touched (stamina wheel, minimap, the item rows are other panes / screens).
#define MS_VT 0x7001cf4120ull
#define MS_SLOT95 0x7000e1e1a4ull
// (s93) REWRITE: the user saw the hearts still up; no live object had vptr MS_VT (list, field, full-memory and subclass
// scans all 0), so the MainScreen route is dropped. The live ui2d pane keeps its name inline: s93 name scan found two
// live "Pa_HeartGauge_00" panes on the heap (0x72e9be1d48 / 0x72eb2db4b8, plus .bflyt prt1 copies that are skipped),
// each with the pane's flag byte 0x83 at name - 0x60 and holders pointing at name - 0xb8 = the pane base (so +0x58 =
// the flag byte, bit 0 = visible: the s92 GUESS offset holds). VERIFIED live: name, layout of the bytes; GUESS: bit 0
// hides the draw. Every frame while "set hearts 1": clear bit 0 on every live pane found; rescan (heap 0x72e0..0x72f0,
// ~0.1 s) every 10 s while none is known, and when a known one's name is gone (a load).
#define HG_NAME "Pa_HeartGauge_00"
#define HG_BASE 0xb8   // pane base = name - 0xb8
static int hearts_name_ok(uint64_t pb) { return rd_ok(pb, HG_BASE + 17) && !memcmp((const void *)(uintptr_t)(pb + HG_BASE), HG_NAME, 17); }
static void hearts_tick(uint64_t mgr) {
    static uint64_t pane[4];
    static uint8_t was[4];
    static int np;
    static unsigned gap, fixed;
    (void)mgr;
    for (int i = 0; i < np; i++)
        if (!hearts_name_ok(pane[i])) {
            LOG("[sora] hearts: pane %#llx gone (a load?)\n", (unsigned long long)pane[i]);
            pane[i] = pane[--np], was[i] = was[np], i--;
        }
    if (!np && g_hearts_hide && gap++ % 600 == 0) {
        ULONGLONG t0 = GetTickCount64();
        MEMORY_BASIC_INFORMATION mbi;
        int files = 0;
        for (uint64_t r = 0x72e0000000ull; r < 0x72f0000000ull && np < 4 && GetTickCount64() - t0 < 1500 &&
             VirtualQuery((void *)(uintptr_t)r, &mbi, sizeof mbi); r = (uint64_t)(uintptr_t)mbi.BaseAddress + mbi.RegionSize) {
            if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                !(mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY))) continue;
            const char *b = (const char *)mbi.BaseAddress, *e = b + mbi.RegionSize - 17;
            for (const char *c = b + HG_BASE; c < e && np < 4; c++) {
                c = memchr(c, 'P', (size_t)(e - c));
                if (!c) break;
                if (memcmp(c, HG_NAME, 17)) continue;
                bool file = false;  // (the .bflyt's own blocks: a tag in the 0x60 bytes before the name)
                for (const char *t = c - 0x60; t < c - 3 && !file; t++)
                    file = !memcmp(t, "prt1", 4) || !memcmp(t, "pan1", 4) || !memcmp(t, "pas1", 4) || !memcmp(t, "pae1", 4);
                if (file) { files++; continue; }
                uint64_t pb = (uint64_t)(uintptr_t)c - HG_BASE;
                if ((pb & 7)) continue;
                pane[np] = pb, was[np] = GV(uint8_t, pb, 0x58);
                LOG("[sora] hearts: live pane %#llx (vptr %#llx) flag +0x58 = %#x\n", (unsigned long long)pb,
                    (unsigned long long)GV(uint64_t, pb, 0), was[np]);
                np++;
            }
        }
        static int said;
        if (np || said++ < 3) LOG("[sora] hearts: pane scan %d live (%d file copies) in %llu ms\n", np, files, (unsigned long long)(GetTickCount64() - t0));
    }
    for (int i = 0; i < np; i++) {
        volatile uint8_t *fl = (volatile uint8_t *)(uintptr_t)(pane[i] + 0x58);
        if (g_hearts_hide) {
            if (*fl & 1) {
                *fl = (uint8_t)(*fl & 0xfe);
                if (++fixed > (unsigned)np && (fixed - np) % 600 == 1) LOG("[sora] hearts: BotW switched them back on %u times (hidden again each frame)\n", fixed - np);
            }
        } else if (!(*fl & 1) && (was[i] & 1)) *fl = (uint8_t)(*fl | 1);  // (set hearts 0: back on)
    }
}

// ------------------------------------------------------------------ s59: the KH2 Game Over screen
// recon/S59_HELPER_GAMEOVER.md. BotW's Game Over screen = UI screen 0x30 (13 of 13 probed deaths in live_s29 .. s58b:
// it comes 85-120 presents after life 0 and goes when Continue starts the load, 0x55 next; never seen otherwise). While
// it is up and Link is dead: KH2's Game Over - Sora floating on his back in the dark, KH2's own motion (GAMEOVER.mset
// set 252 'AK10', Sora's sub-motion '0010': 150 frames at 30 fps, a seamless loop; data/gameover_pose.bin from
// recon/wip_s59_gameover/gobake), drawn by the exe's screen mode (sora_draw_screen: its own camera, over BotW's UI,
// black from g_go_y0 % down = under BotW's red GAME OVER logo, over its Continue / Quit menu) for g_go_hold ms. A is
// held back from BotW meanwhile (auto-Continue's taps too); then BotW's menu is back and Continue works as before.
// An exe without sora_draw_screen, or no pose file: BotW's screen alone (logged once).
#define SCREEN_GAMEOVER 0x30
static bool g_go_screen;  // screen 0x30 exists (load_probe, every frame)
static void gameover_screen(uint64_t mgr, uint64_t present) {
    bool on = mgr && screen_on(mgr, SCREEN_GAMEOVER);
    if (on != g_go_screen) LOG("[sora] Game Over screen 0x30 %s at present %llu\n", on ? "up" : "gone", (unsigned long long)present);
    g_go_screen = on;
}
static struct {
    bool on, done;        // done: this Game Over's KH2 screen is over (or not possible): BotW's alone until 0x30 goes
    uint64_t t0;          // GetTickCount64 at the start
    float *pose;          // poses x SORA_RAG_BONES x 12 (the bake's rows, KH cm)
    int n;                // poses (301: 60 Hz ticks 0..300, the last = the first)
    float c[3];           // his centre at pose 0 (the bones' bounding box, model space)
    int tried;
    void (*screen)(bool, const float *, const float *, const float *);
    void (*world)(int, const float *, const float *, float, float);
    void (*tint)(int, float, float, float, float);
} GO;
static float g_go_bones[SORA_RAG_BONES * 12];
static bool gameover_ready(void) {
    if (GO.tried) return GO.pose && GO.screen && GO.world;
    GO.tried = 1;
    HMODULE exe = GetModuleHandleA(NULL);
    GO.screen = (void (*)(bool, const float *, const float *, const float *))(void *)GetProcAddress(exe, "sora_draw_screen");
    GO.world = (void (*)(int, const float *, const float *, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_world");
    GO.tint = (void (*)(int, float, float, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_tint");
    const char *path = getenv("SAVAGEWIND_SORA_GO_POSE");
    if (!path || !*path) path = "D:/SoraBotW/data/gameover_pose.bin";
    path = sora_path(path);
    FILE *f = fopen(path, "rb");
    uint32_t h[4] = {0};
    struct { uint16_t slot, poses; uint32_t off; } d = {0};
    if (f && fread(h, 4, 4, f) == 4 && h[0] == 0x50524B53u && h[2] >= 1 && h[3] == SORA_RAG_BONES && fread(&d, 8, 1, f) == 1 &&
        d.poses >= 2 && !fseek(f, (long)d.off, SEEK_SET)) {
        size_t nf = (size_t)d.poses * SORA_RAG_BONES * 12;
        GO.pose = malloc(nf * sizeof(float));
        if (GO.pose && fread(GO.pose, sizeof(float), nf, f) == nf) GO.n = d.poses;
        else free(GO.pose), GO.pose = NULL;
    }
    if (f) fclose(f);
    if (GO.pose) {
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        for (int b = 0; b < SORA_RAG_SORA_BONES; b++)
            for (int j = 0; j < 3; j++) {
                float v = GO.pose[b * 12 + 9 + j];
                lo[j] = v < lo[j] ? v : lo[j], hi[j] = v > hi[j] ? v : hi[j];
            }
        for (int j = 0; j < 3; j++) GO.c[j] = (lo[j] + hi[j]) / 2;
    }
    LOG("[sora] KH2 Game Over: sora_draw_screen %s, sora_draw_actor_world %s, pose %s (%d poses, %u bones; centre %.1f %.1f %.1f cm)%s\n",
        GO.screen ? "found" : "NOT in this exe", GO.world ? "found" : "missing", path, GO.n, h[3], GO.c[0], GO.c[1], GO.c[2],
        GO.pose && GO.screen && GO.world ? "" : ": BotW's Game Over screen alone");
    return GO.pose && GO.screen && GO.world;
}
static void gameover_off(const char *why) {
    g_go_block_until = 0;
    if (!strcmp(why, "unload")) free(GO.pose), GO.pose = NULL, GO.tried = 0;  // (the hot reload: the next copy reads it again)
    if (!GO.on) return;
    GO.on = false;
    if (GO.screen) GO.screen(false, NULL, NULL, NULL);
    if (GO.tint) GO.tint(0, 0, 0, 0, 0);
    if (g_go_log) LOG("[sora] KH2 Game Over off after %llu ms (%s): BotW's menu, A to BotW again\n",
                      (unsigned long long)(GetTickCount64() - GO.t0), why);
}
// Every frame (sora_mod_frame, after the dead latch): on / off and the A block.
static void gameover_tick(bool dead, uint64_t present) {
    bool want = g_kh2go && dead && g_go_screen && !g_loading;
    if (!dead || !g_go_screen) GO.done = false;
    if (want && !GO.on && !GO.done) {
        if (!gameover_ready()) { GO.done = true; return; }
        GO.on = true, GO.t0 = GetTickCount64();
        if (g_go_log) LOG("[sora] KH2 Game Over on at present %llu: Sora floats for %d ms (0 = until the screen goes), A %s\n",
                          (unsigned long long)present, g_go_hold, g_go_block ? "held from BotW" : "to BotW");
    }
    if (!GO.on) return;
    g_go_block_until = g_go_block && g_go_hold > 0 ? GO.t0 + (uint64_t)g_go_hold + 500 : 0;  // (a safety bound: the frame code may stop)
    if (!want) gameover_off(!g_kh2go ? "set kh2go 0" : !g_go_screen ? "screen 0x30 gone" : !dead ? "Link back" : "a load");
    else if (g_go_hold > 0 && GetTickCount64() - GO.t0 >= (uint64_t)g_go_hold) GO.done = true, gameover_off("hold over");
}
// The not-driving branch, after its sora_draw_off: this frame's float pose with the screen camera. Sora at the origin
// (feet 0, face 0: model = world * 0.01 g_k), the camera on a sphere around his centre (yaw from his +z side, elevation
// up), moved up its own up axis so his centre sits g_go_sy % down the screen.
static void gameover_draw(void) {
    if (!GO.on || !GO.pose) return;
    uint64_t ms = GetTickCount64() - GO.t0;
    float tick = fmodf((float)ms * 0.06f, (float)(GO.n - 1));  // (60 Hz ticks; pose n-1 = pose 0)
    int i = (int)tick;
    if (i > GO.n - 2) i = GO.n - 2;
    float u = tick - (float)i;
    const float *a = GO.pose + (size_t)i * SORA_RAG_BONES * 12, *b = a + SORA_RAG_BONES * 12;
    for (int k = 0; k < SORA_RAG_BONES * 12; k++) g_go_bones[k] = a[k] + (b[k] - a[k]) * u;
    if (!g_go_kb) keyblade_hide(g_go_bones);
    const float s = 0.01f * g_k, D2R = 3.14159265f / 180;
    float el = (float)g_go_el, yaw = ((float)g_go_yaw + (float)g_go_spin * (float)ms / 1000.0f) * D2R;
    el = (el > 80 ? 80 : el < -80 ? -80 : el) * D2R;
    float fov = (float)(g_go_fov > 5 && g_go_fov < 120 ? g_go_fov : 30) * D2R, dist = (float)g_go_dist * 0.01f;
    float tg[3] = {GO.c[0] * s, GO.c[1] * s, GO.c[2] * s};
    float bk[3] = {sinf(yaw) * cosf(el), sinf(el), cosf(yaw) * cosf(el)};  // (target -> camera = the view's back axis)
    float rt[3] = {bk[2], 0, -bk[0]};  // (world up x back)
    float rl = sqrtf(rt[0] * rt[0] + rt[2] * rt[2]);
    rt[0] /= rl, rt[2] /= rl;
    float up[3] = {bk[1] * rt[2] - bk[2] * rt[1], bk[2] * rt[0] - bk[0] * rt[2], bk[0] * rt[1] - bk[1] * rt[0]};  // (back x right)
    float sh = ((float)g_go_sy / 100.0f - 0.5f) * 2 * dist * tanf(fov / 2);  // (up the camera's up: he sits lower)
    float cam[3];
    for (int j = 0; j < 3; j++) cam[j] = tg[j] + bk[j] * dist + up[j] * sh;
    float view[12] = {rt[0], rt[1], rt[2], -(rt[0] * cam[0] + rt[1] * cam[1] + rt[2] * cam[2]),
                      up[0], up[1], up[2], -(up[0] * cam[0] + up[1] * cam[1] + up[2] * cam[2]),
                      bk[0], bk[1], bk[2], -(bk[0] * cam[0] + bk[1] * cam[1] + bk[2] * cam[2])};
    float proj[4] = {0.05f, 100.0f, fov, 0};  // (aspect 0: the game image's)
    float y0 = (float)(g_go_y0 < 0 ? 0 : g_go_y0 > 100 ? 100 : g_go_y0) / 100.0f, clear[4] = {0, y0, 1, 1};
    GO.screen(true, view, proj, clear);
    if (GO.tint) {  // (the fade in: dark tint, KH2's screen comes up from black)
        float fade = g_go_fade > 0 && ms < (uint64_t)g_go_fade ? 1 - (float)ms / (float)g_go_fade : 0;
        GO.tint(0, 0, 0, 0, fade);
    }
    const float feet[3] = {0, 0, 0};
    GO.world(0, g_go_bones, feet, 0, s);
    static uint64_t said;
    if (g_go_log && ms - said >= 2000) said = ms, LOG("[sora] KH2 Game Over: %llu ms, tick %.1f, camera %.2f %.2f %.2f -> %.2f %.2f %.2f\n",
                                                     (unsigned long long)ms, tick, cam[0], cam[1], cam[2], tg[0], tg[1], tg[2]);
    if (ms < said) said = 0;
}

static void load_probe(uint64_t present) {
    static char last[512];
    vfr_find();
    char cur[512];
    int k = snprintf(cur, sizeof cur, "pause %u vfr override %d:", GV(uint32_t, PAUSE_STATE, 0),
                     g_vfr ? GV(uint8_t, g_vfr, 0x29) : -1);
    uint64_t pp = rd_ok(SCREEN_MGR_PP, 8) ? GV(uint64_t, SCREEN_MGR_PP, 0) : 0;
    uint64_t mgr = pp && rd_ok(pp, 8) ? GV(uint64_t, pp, 0) : 0;
    if (mgr && rd_ok(mgr, 0x38)) {
        uint32_t n = GV(uint32_t, mgr, 0x28);
        uint64_t arr = GV(uint64_t, mgr, 0x30);
        for (uint32_t i = 0; i < n && i < 256 && arr && rd_ok(arr + 8 * i, 8) && k < (int)sizeof cur - 8; i++)
            if (GV(uint64_t, arr, 8 * i)) k += snprintf(cur + k, sizeof cur - k, " %x", i);
    } else k += snprintf(cur + k, sizeof cur - k, " (no screen mgr)"), mgr = 0;
    load_latch(mgr, present);
    g_msg_screen = mgr && screen_on(mgr, 0x4a);
    g_item_screen = mgr && screen_on(mgr, 0xb);
    g_scr33 = mgr && screen_on(mgr, 0x33);
    {  // (s86) the freeze set + what comes / goes
        static uint8_t was[32];
        uint8_t now[32] = {0};
        uint32_t n = mgr ? GV(uint32_t, mgr, 0x28) : 0;
        for (uint32_t i = 0; i < n && i < 256; i++)
            if (screen_on(mgr, i)) now[i / 8] |= 1u << (i % 8);
        int id = -1;
        for (int i = 0; i < 256 && id < 0; i++)
            if (now[i / 8] & g_frz_mask[i / 8] & (1u << (i % 8))) id = i;
        if (id != g_frz_id) LOG("[sora] freeze screen %s %#x at present %llu (Link state %#x)\n", id >= 0 ? "up:" : "gone, was",
                                id >= 0 ? id : g_frz_id, (unsigned long long)present, linkpos_link_state_word());
        g_frz_id = id;
        if (g_frz_log && memcmp(now, was, sizeof now) && n) {
            char d[200];
            int m = 0;
            for (int i = 0; i < 256 && m < (int)sizeof d - 8; i++) {
                bool a = now[i / 8] & (1u << (i % 8)), b = was[i / 8] & (1u << (i % 8));
                if (a != b) m += snprintf(d + m, sizeof d - m, " %c%x", a ? '+' : '-', i);
            }
            LOG("[sora] screen diff at present %llu:%s (Link state %#x, pause %u, driving %d)\n", (unsigned long long)present,
                d, linkpos_link_state_word(), GV(uint32_t, PAUSE_STATE, 0), !g_link_owns);
        }
        memcpy(was, now, sizeof now);
    }
    gameover_screen(mgr, present);
    wheel_tick(mgr);
    hearts_tick(mgr);  // (s92 hud)
    if (strcmp(cur, last)) {
        LOG("[sora] screens at present %llu: %s (Link state %#x)\n", (unsigned long long)present, cur,
            linkpos_link_state_word());
        strcpy(last, cur);
    }
}

// KH2's Glide motions while BotW's paraglider carries Link (PHH kh2ctl.py ACT_GLIDE_HOVER / ACT_GLIDE, re:0x423840 /
// re:0x423A70): hover (A173) until the stick first points somewhere, then glide (A170), banking left / right (A171 /
// A172) while the stick is more than acos 0.98 off his facing. Facing = Link's (BotW turns him). Motion changes
// blend over GLIDE_BLEND KH frames (KH2's glide blend 16): the pose drawn last (the one before the glide too: Fall)
// is frozen in g_glast at the switch, and the new motion's pose lerps from it (bone rows lerped, re-orthonormalized
// like rag_draw) with weight t / 16, t advancing by kh. A switch mid-blend starts over from the blended pose drawn
// last, so chained hover -> move -> bank never pops. No bake match (bone count) -> the cut as before.
enum { MOT_GLIDE_MOVE = 205, MOT_GLIDE_LEFT = 206, MOT_GLIDE_RIGHT = 207, MOT_GLIDE_HOVER = 208 };
#define GLIDE_BANK_COS 0.98f
#define GLIDE_BLEND 16.0f
static float g_gblend = -1;  // KH frames into the glide blend (< 0: none)
static float g_gpose[POSE_BUF_BONES * 12], g_glast[POSE_BUF_BONES * 12];
static bool g_glast_ok;  // g_glast holds the pose drawn last frame (false: nothing drawn yet since the glide began)

// Draws this frame's glide pose: the from-pose (frozen) blended into the motion's, or the plain pose when no blend
static void glide_pose_draw(int from_motion, float from_clock, float face, float scale, float kh, float len) {
    bool ok = sora_draw_bone_count() == SORA_RAG_BONES;
    if (ok && from_motion != S.motion) {  // (the switch: freeze what was drawn last as the from-pose)
        const Mot *fm = mot_of(from_motion);
        if (!g_glast_ok && fm && sora_draw_slot_pose(fm->slot, from_clock, g_glast)) g_glast_ok = true;
        g_gblend = g_glast_ok ? 0 : -1;
        if (g_glast_ok) LOG("[sora] blend: motion %d -> %d, %.0f KH frames\n", from_motion, S.motion, len);
    }
    if (ok && g_gblend >= 0) {
        g_gblend += kh;
        if (g_gblend < len && sora_draw_slot_pose(S.mot->slot, S.clock, g_gpose)) {
            float wgt = g_gblend / len;
            for (int k = 0; k < SORA_RAG_BONES; k++) {
                float *d = g_gpose + k * 12, *a = g_glast + k * 12, len[3];
                for (int r = 0; r < 3; r++) {
                    float la = sqrtf(v3dot(a + 3 * r, a + 3 * r)), lb = sqrtf(v3dot(d + 3 * r, d + 3 * r));
                    len[r] = la + (lb - la) * wgt;
                }
                for (int j = 0; j < 12; j++) d[j] = a[j] + (d[j] - a[j]) * wgt;
                ortho_rows(d, len);
            }
            memcpy(g_glast, g_gpose, sizeof g_gpose), g_glast_ok = true;
            sora_draw_world(g_gpose, face, scale);
            return;
        }
        g_gblend = -1;
    }
    g_glast_ok = ok && sora_draw_slot_pose(S.mot->slot, S.clock, g_glast);  // (keep the drawn pose for a later switch)
    pose_draw_sm(S.mot->slot, S.clock, face, scale);
}

static bool glide_draw(const float lm[12], float kh) {
    float face = atan2f(lm[2], lm[10]), sx = kh_axis(g_lx), sy = kh_axis(g_ly), cf[2] = {0, 1};
    linkpos_camera_fwd(cf);
    float dir[2] = {cf[0] * sy - cf[1] * sx, cf[1] * sy + cf[0] * sx}, dl = hypotf(dir[0], dir[1]);
    int want = S.motion == MOT_GLIDE_HOVER && dl < 1e-4f ? MOT_GLIDE_HOVER : MOT_GLIDE_MOVE;
    if (dl > 1e-4f) {
        float fx = sinf(face), fz = cosf(face);
        dir[0] /= dl, dir[1] /= dl;
        if (fx * dir[0] + fz * dir[1] < GLIDE_BANK_COS)  // left = (fz, -fx): the stick's side of his facing
            want = fz * dir[0] - fx * dir[1] > 0 ? MOT_GLIDE_LEFT : MOT_GLIDE_RIGHT;
    }
    if (S.motion < MOT_GLIDE_MOVE || S.motion > MOT_GLIDE_HOVER) want = dl < 1e-4f ? MOT_GLIDE_HOVER : want;
    int from_motion = S.motion;
    float from_clock = S.clock;  // (the pose drawn last frame, when nothing was frozen yet)
    set_motion(want);
    if (!S.mot) return false;
    S.clock = fmodf(S.clock + kh, (float)S.mot->frames);
    S.face = face;
    glide_pose_draw(from_motion, from_clock, face, 0.01f * g_k, kh, GLIDE_BLEND);
    return true;
}

// ---- S32 glide port, section B (placed here so MOT_GLIDE_* / g_gblend / set_motion resolve)
// APPROX (KH2 re:0x3B87E0 casts down for it; no BotW ray cast yet): the floor under Sora = S.ground_z, the height of
// the last grounded frame (section 5, ACT_IDLE). Right for a jump over flat ground. Below it (off a cliff, down a
// slope) the real floor is further down and unknown: GLIDE_FLOOR_FAR, so the glide sinks at GlideFallMax and ends by
// the landing check (section 1: the sink stopped) instead of GlideEndHeight. TODO: BotW's fall ray field (helper H).
// (s54 floor ray) now the floor under him by ray cast (floor_ray_tick) when there is one: KH2's own h, so the sink
// eases to GlideFallHeight over a lower floor too and GlideEndHeight ends it there; a floor at / above the feet = 0
// (the end); no floor within g_flen u = GLIDE_FLOOR_FAR (as before). No cast / "set fglide 0" -> the s32 rule above.
static int g_hsrc;  // the source of the last height_above_floor (FS_*), for the glide logs
static float height_above_floor(const float p[3], float z_up) {
    (void)p;
    float f;
    g_hsrc = g_fglide ? floor_under(&f) : FS_LAST;
    if (g_hsrc == FS_RAY) return z_up - f < 0 ? 0 : z_up - f;
    if (g_hsrc == FS_MISS) return GLIDE_FLOOR_FAR;
    float h = z_up - S.ground_z;
    return h < 0 ? GLIDE_FLOOR_FAR : h;
}

// glide_pose_draw needs the motion drawn LAST (to freeze it at a switch): tracked across frames (section F).
// g_gl_tail: the glide ended, the Fall still blends in from the last glide pose (KH2's 16-frame blend, re:0x623BC4)
static int gl_from_motion = -1;
static float gl_from_clock;
static bool g_gl_tail;
static int g_glide_stuck;  // frames in a row the glide's sink was stopped (section 1)

// J 4.6: Fall blends in from the motion before it over len KH frames (KH2: 8 at the apex and the hop, re:0x3CB610;
// 16 on the walk-off, re:0x3D6430) with glide_pose_draw's two-pose mix; the drawer runs it while Fall plays
static int g_fb_from = -1;  // the motion Fall blends in from (-1: no blend)
static float g_fb_clock, g_fb_len;
static void fall_motion(float len) {
    if (S.motion == MOT_FALL) return;
    if (S.mot && S.motion >= 0 && len > 0)
        g_fb_from = S.motion, g_fb_clock = S.clock, g_fb_len = len, g_gblend = -1, g_glast_ok = false;
    set_motion(MOT_FALL);
}

// ---- (s58, recon/S58_HELPER_GLIDESTAM.md) the glide's stamina. The KH2 glide is Sora's own servo: Link never enters
// BotW's paraglider action, so nothing paid for it (user s55/s57). Now ACT_GLIDE / ACT_GLIDE_HOVER pays what
// PlayerParashawlGlide::calc_ (1.8.2 image 0x7100b6a880) pays: after a NoEnergyTime grace (30 delta frames, timer
// actor +0x1844 there), each game frame EnergyGlide 28 (Player_Link.baiprog) * 1/30 (0x710158a9e8) * Link's delta frame
// (actor +0x20f0, ~1.15 at 25 fps) via 0x7100bd69b8 -> 0x7100bd6a10 -> addStaminaDelta 0x7100babf08 = 28 a second,
// one wheel (1000) in ~36 s. BotW's own path (mode 1): the cost goes into Link's stamina delta accumulator actor +0xe7c
// (per core [0..2], the game's core 1 = [0]; Player::updateStamina 0x7100bd6b18 sums [0..cores-1] of the u32 cores
// 0x7001c4bb00, zeroes them, then: the yellow extra +0x2000 first takes what green lacks, clamp 0..max (actor +0x186c),
// PlayerInfo::setStaminaCurrentMax 0x7100bb3f30 = PlayerInfo +0xa4 (the CURRENT stamina despite zeldaret's name: the
// s31 hand-off drained it, recoverStamina 0x7100bb4020 sets it to the max) + the gdt flag, and the recovery wait +0x1d38
// restarts, so no regen while he pays). Then Link's calc (0x7100bbee6c -> 0x7100bd695c) sets the exhausted bit (actor
// +0xc44 bit 20) once green and yellow are 0; updateStamina clears it when the wheel is full again. BotW's glide ends at
// exhausted (0x7100bd5f98: the bit and not full) -> ours too (glide_end: Fall), and no new glide while exhausted (BotW
// won't open the paraglider either). Mode 2 (fallback, also when cores reads 0): the same sums written straight into
// PlayerInfo +0xa4 / +0x2000, the wait +0x1d38 = its reload +0x1d3c each paying frame; no gdt flag (the next BotW
// change writes it). Not ported: 0x7100bd6a10's cost factor (>= 1, its gate byte [[0x7001cb4410]]+0x80b; unknown).
// Runs from the driving path only (game thread, rt_vfr_update; paused frames never get here).
#define STAM_CUR 0xa4           // PlayerInfo: current stamina (1000 a wheel)
#define STAM_DELTA 0xe7c        // actor: stamina delta accumulator [0]
#define STAM_EXTRA 0x2000       // actor: yellow (extra) stamina
#define STAM_MAX 0x186c         // actor: max stamina (the wheels)
#define STAM_FLAGS 0xc44        // actor: u32, bit 20 = exhausted
#define STAM_EXHAUSTED (1u << 20)
#define STAM_WAIT 0x1d38        // actor: the recovery wait (+0x1d3c its reload)
#define STAM_DT 0x20f0          // actor: Link's delta frame
#define STAM_CORES (sora_sites()->stamina_cores)
#define GS_ENERGY_GLIDE 28.0f   // Player_Link.baiprog PlayerParashawlGlide EnergyGlide
#define GS_NO_ENERGY_TIME 30.0f // ... NoEnergyTime (delta frames)
static struct { bool on, out, told; int mode; float grace, paid, start; uint64_t at; } GS;

// Link's actor + PlayerInfo when both read sane (stamina 0..max, max 1..10000), else 0
static uint64_t gs_actor(uint64_t *pi) {
    uint64_t a = link_actor();
    *pi = *(const uint64_t *)(uintptr_t)PLAYER_INFO_PTR;
    if (!a || !rd_ok(*pi, 0xa8) || !rd_ok(a + STAM_FLAGS, 4) || !rd_ok(a + STAM_EXTRA, 4) || !rd_ok(a + STAM_DT, 4) ||
        !rd_ok(a + STAM_WAIT, 8))
        return 0;
    float cur = GV(float, *pi, STAM_CUR), max = GV(float, a, STAM_MAX);
    return max >= 1 && max <= 10000 && cur >= 0 && cur <= max + 1 ? a : 0;
}

// BotW's "exhausted" (0x7100bd5f98) or nothing left to pay with (green + yellow): no glide
static bool gs_empty(uint64_t a, uint64_t pi) {
    float cur = GV(float, pi, STAM_CUR), extra = GV(float, a, STAM_EXTRA);
    bool tired = (GV(uint32_t, a, STAM_FLAGS) & STAM_EXHAUSTED) && cur != GV(float, a, STAM_MAX);
    return tired || (cur <= 0 && extra <= 0);
}

// The glide start's check: false = refuse (exhausted / empty), logged once per refusal run
static bool glidestam_ok(void) {
    uint64_t pi, a = g_glidestam ? gs_actor(&pi) : 0;
    if (!a || !gs_empty(a, pi)) return GS.told = false, true;
    if (!GS.told)
        LOG("[sora] glidestam: no glide, stamina out (%.1f + extra %.1f, flags %#x)\n", GV(float, pi, STAM_CUR),
            GV(float, a, STAM_EXTRA), GV(uint32_t, a, STAM_FLAGS)), GS.told = true;
    return false;
}

// "set stam <n>" (tests): Link's current stamina = n (clamped 0..max), straight into PlayerInfo (no gdt flag; BotW's
// regen / next change carries on from it). Below max BotW shows the wheel; 0 = exhausted at his next calc.
static void glidestam_set(int v) {
    uint64_t pi, a = gs_actor(&pi);
    if (!a) { LOG("[sora] stam: no Link / PlayerInfo\n"); return; }
    float max = GV(float, a, STAM_MAX), was = GV(float, pi, STAM_CUR);
    *(float *)(uintptr_t)(pi + STAM_CUR) = v < 0 ? 0 : v > max ? max : (float)v;
    LOG("[sora] stam: %.1f -> %.1f (max %.1f, extra %.1f)\n", was, GV(float, pi, STAM_CUR), max,
        GV(float, a, STAM_EXTRA));
}

// Every driving frame: pays this frame's glide cost; true = the stamina is out (the glide ends)
static bool glidestam_tick(uint64_t present) {
    bool gl = S.act == ACT_GLIDE || S.act == ACT_GLIDE_HOVER;
    uint64_t pi = 0, a = gl && g_glidestam ? gs_actor(&pi) : 0;
    if (!a) {
        if (GS.on) {
            pi = *(const uint64_t *)(uintptr_t)PLAYER_INFO_PTR;
            LOG("[sora] glidestam off: paid %.1f in %llu presents, stamina %.1f -> %.1f\n", GS.paid,
                (unsigned long long)(present - GS.at), GS.start, rd_ok(pi, 0xa8) ? GV(float, pi, STAM_CUR) : -1.0f);
        }
        GS.on = GS.out = false;
        return false;
    }
    float *cur = (float *)(uintptr_t)(pi + STAM_CUR), *extra = (float *)(uintptr_t)(a + STAM_EXTRA);
    float dt = GV(float, a, STAM_DT);
    if (!(dt > 0.05f && dt < 4)) dt = framepace_game_ratio();  // (BotW's own delta first; framepace's as the stand-in)
    if (!(dt > 0.05f && dt < 4)) dt = 1;
    if (!GS.on) {
        uint32_t cores = rd_ok(STAM_CORES, 4) ? GV(uint32_t, STAM_CORES, 0) : 0;
        GS.on = true, GS.out = false, GS.grace = GS_NO_ENERGY_TIME, GS.paid = 0, GS.start = *cur, GS.at = present;
        GS.mode = g_glidestam == 2 || cores == 0 || cores > 3 ? 2 : 1;
        LOG("[sora] glidestam on: stamina %.1f + extra %.1f of %.1f, flags %#x, dt %.3f, cores %u -> mode %d (%s)\n", *cur,
            *extra, GV(float, a, STAM_MAX), GV(uint32_t, a, STAM_FLAGS), dt, cores, GS.mode,
            GS.mode == 1 ? "BotW's delta" : "direct");
    }
    if (GS.grace > 0) GS.grace -= dt;  // (BotW's grace: the first second of a glide is free)
    else if (!gs_empty(a, pi)) {
        float cost = GS_ENERGY_GLIDE / 30.0f * dt;
        if (GS.mode == 1) *(float *)(uintptr_t)(a + STAM_DELTA) -= cost;  // (updateStamina takes it this frame)
        else {  // (updateStamina's sums: green first, yellow takes the rest; the wait restarts = no regen)
            float v = *cur - cost;
            if (v < 0) *extra = *extra + v > 0 ? *extra + v : 0, v = 0;
            *cur = v;
            *(float *)(uintptr_t)(a + STAM_WAIT) = GV(float, a, STAM_WAIT + 4);
        }
        GS.paid += cost;
    }
    bool out = gs_empty(a, pi);
    if (out && !GS.out)
        LOG("[sora] glidestam out after %.1f paid in %llu presents (stamina %.1f extra %.1f flags %#x): glide ends\n",
            GS.paid, (unsigned long long)(present - GS.at), *cur, *extra, GV(uint32_t, a, STAM_FLAGS));
    else if ((present - GS.at) % 60 == 59)
        LOG("[sora] glidestam %.1f + extra %.1f (paid %.1f, dt %.3f%s)\n", *cur, *extra, GS.paid, dt,
            GS.grace > 0 ? ", grace" : "");
    GS.out = out;
    return out;
}

// (s95) one Aerial Dodge's stamina, paid at its start (base tap-B dodge + Master's B-again dodge; Final / Valor have none).
// g_dodgestam (1000 a wheel) as one lump through the glide's mode: mode 1 = Link's delta accumulator (updateStamina takes
// it this frame: green first, the yellow extra the rest, the recovery wait restarts), mode 2 / no cores = the same sums
// written straight in. Less left than the cost = pay what is there (he ends exhausted, as a BotW climb jump). Stamina out
// (BotW's exhausted bit, or nothing left) + g_dodgeempty = refused. No Link / PlayerInfo = free (logged).
static bool stam_pay(const char *who, int costi) {
    if (costi <= 0) return true;
    uint64_t pi, a = gs_actor(&pi);
    if (!a) { LOG("[sora] dodgestam: %s free (no Link / PlayerInfo)\n", who); return true; }
    float *cur = (float *)(uintptr_t)(pi + STAM_CUR), *extra = (float *)(uintptr_t)(a + STAM_EXTRA);
    float was = *cur, wasx = *extra, cost = (float)costi;
    if (gs_empty(a, pi) && g_dodgeempty) {
        LOG("[sora] dodgestam: NO %s, stamina out (%.1f + extra %.1f, flags %#x)\n", who, was, wasx, GV(uint32_t, a, STAM_FLAGS));
        return false;
    }
    uint32_t cores = rd_ok(STAM_CORES, 4) ? GV(uint32_t, STAM_CORES, 0) : 0;
    int mode = g_glidestam == 2 || cores == 0 || cores > 3 ? 2 : 1;
    if (mode == 1) *(float *)(uintptr_t)(a + STAM_DELTA) -= cost;
    else {
        float v = *cur - cost;
        if (v < 0) *extra = *extra + v > 0 ? *extra + v : 0, v = 0;
        *cur = v;
        *(float *)(uintptr_t)(a + STAM_WAIT) = GV(float, a, STAM_WAIT + 4);
    }
    LOG("[sora] dodgestam: %s paid %.0f (mode %d), stamina was %.1f + extra %.1f of %.1f\n", who, cost, mode, was, wasx,
        GV(float, a, STAM_MAX));
    return true;
}

// re:0x3A81F0's glide (PHH _glide_start): airborne ACTION that allows it, no special motion, higher than
// GlideStartHeight above the floor -> 17 (hover) without a stick intent, 18 (glide) with one.
static bool glide_start(const float p[3], float z_up, float sspeed) {
    if (!ACT_AIR(S.act) || S.act == ACT_GLIDE || S.act == ACT_GLIDE_HOVER || S.special) return false;
    if (!(height_above_floor(p, z_up) > KH_GLIDE_START_HEIGHT)) return false;
    if (!mot_of(MOT_GLIDE_MOVE)) return false;
    if (!glidestam_ok()) return false;  // (s58: exhausted - BotW won't open its paraglider either)
    S.act = sspeed > 0 ? ACT_GLIDE : ACT_GLIDE_HOVER;  // _switch: vel kept (no hvel_set), base untouched
    S.req_set = false, S.ceiling = false, S.jump_held = S.jump_released = false, S.glide_armed = false;
    g_gblend = -1, g_glast_ok = false;  // (the first glide pose blends from Fall, as glide_draw did)
    gl_from_motion = -1, g_gl_tail = false, g_glide_stuck = 0;
    set_motion(sspeed > 0 ? MOT_GLIDE_MOVE : MOT_GLIDE_HOVER);
    float h = height_above_floor(p, z_up);
    LOG("[sora] glide start LV%d%s (%s) at %.3f %.3f %.3f h %.1f (%s)\n", glide_lv_now(), IN_FINAL ? " (Final)" : "", S.act == ACT_GLIDE ? "glide" : "hover",
        p[0], p[1], p[2], h, FS_NAME[g_hsrc]);
    return true;
}

// re:0x4235B0 (PHH _glide_end): B let go or the floor nearer than GlideEndHeight -> Fall (16-frame blend) + fall start
static bool glide_end(const float p[3], float z_up, bool b_held) {
    float h = height_above_floor(p, z_up);
    if (b_held && KH_GLIDE_END_HEIGHT <= h) return false;
    LOG("[sora] glide end (%s) at %.3f %.3f %.3f after %.1f KH frames, h %.1f (%s)\n",
        b_held ? "floor" : GS.out ? "stamina out" : "B let go", p[0], p[1], p[2], S.air, h, FS_NAME[g_hsrc]);
    fall_start(0, z_up);  // -> ACT_JUMP, horizontal velocity kept; _leave: the turn speed back to prty's (KH_TURN)
    set_motion(MOT_FALL);  // (16-frame blend into Fall: g_gl_tail keeps glide_pose_draw on, from the last glide pose)
    g_gl_tail = gl_from_motion >= 0;
    return true;
}

// ------------------------------------------------------------------ the party (s54, party slices 2 + 3)
// Donald + Goofy follow Sora: runtime/sora_party.h (S54_HELPER_PARTY_FOLLOW.md; the draw actors = slice 1).
#include "sora_net_pcap.h"  // (mp slice 3: the SNAP's party block = what party_draw_member / pl_draw drew this frame)
#include "sora_party.h"
#include "sora_form.h"  // (s68 drive: Valor Form, Drive Forms slice 2; S64_HELPER_DRIVE_RECON.md 6)
#include "sora_roxas.h"  // (s109 roxas: the character switch, Roxas until the paraglider)
#include "sora_outfit.h"  // (s146 outfit: Sora's KH2 world outfits, the Debug Menu rows Outfit / Auto Outfits)
// (s110 chestfirst) the ground Dodge Roll needs the ability (KH2 growth 564): learned with the paraglider (Roxas -> Sora;
// KH2 Roxas has no roll motion anyway); Limit Form keeps its own growth roll, debugall owns everything; "set chestfirst 0" = always there
static bool roll_learned(void) {
    if (!g_chestfirst || g_dbgall || IN_LIMIT) return true;
    uint16_t o[AB_MAX_LEARN];
    if (ab_mask(o, ab_order(o)) & ab_bit(AB_DODGE_ROLL)) return true;
    static ULONGLONG said;
    if (GetTickCount64() - said > 5000) said = GetTickCount64(), LOG("[sora] Dodge Roll not learned yet (it comes with the paraglider; set chestfirst 0 = always)\n");
    return false;
}
static void ffps_set(int on) {  // (s91) Final's bakes old / fixed, live: reload its motions, hits, draw actor
    if (fm_in_form()) { LOG("[sora] ffps: refused while in a Drive Form (revert first)\n"); return; }
    g_ffps = on;
    SetEnvironmentVariableA("SORA_FFPS", on ? "1" : "0");
    g_nfhslots[0] = g_nfhslots[1] = 0;  // (s92: hits_file appends; without this the old rows stayed first and won)
    g_nfmots_tab[FORM_FINAL] = mot_file_load(ffps_path(FM_FORMS[FORM_FINAL].motion), g_fmots_tab[FORM_FINAL],
                                             (int)(sizeof g_fmots_tab[FORM_FINAL] / sizeof *g_fmots_tab[FORM_FINAL]));
    hits_file(ffps_path("D:/SoraBotW/data/forms/final_hits_r.bin"), g_fhslots[0], &g_nfhslots[0]);
    hits_file(ffps_path("D:/SoraBotW/data/forms/final_hits_l.bin"), g_fhslots[1], &g_nfhslots[1]);
    FM.actor[FORM_FINAL] = 0, FM.gave_up[FORM_FINAL] = false;  // (fm_load_actor: the new pose pair = its own draw actor)
    LOG("[sora] ffps %d: Final %s (%d motion slots, hits %d / %d)\n", on, on ? "60/120 fps fixed _s91 bakes" : "old x2 bakes",
        g_nfmots_tab[FORM_FINAL], g_nfhslots[0], g_nfhslots[1]);
}
static void ffin_set(int on) {  // (s92) Final's hit bakes _s91 / _s92, live (hits only; the motions are the same)
    g_ffin = on;
    SetEnvironmentVariableA("SORA_FFIN", on ? "1" : "0");
    g_nfhslots[0] = g_nfhslots[1] = 0;  // (hits_file appends)
    hits_file(ffps_path("D:/SoraBotW/data/forms/final_hits_r.bin"), g_fhslots[0], &g_nfhslots[0]);
    hits_file(ffps_path("D:/SoraBotW/data/forms/final_hits_l.bin"), g_fhslots[1], &g_nfhslots[1]);
    LOG("[sora] ffin %d: Final hits %s (%d / %d slots)\n", on, on ? "_s92 (re-hits + radius + body windows, if the files are installed)" : "_s91", g_nfhslots[0], g_nfhslots[1]);
}
#include "sora_limit.h"  // (s89 b4-limit: Limit Form's Limit page + Sonic Rave / Last Arcanum / Strike Raid / Infinity)
#include "sora_slate.h"  // (s93 b7-runes: Sheikah Slate row + Magnesis + rune recon)
#include "sora_slatehip.h"  // (s94 b8-slatehip: the Slate on Sora's hip)
#include "sora_roxsfx.h"  // (s110 roxsfx: Roxas sounds + fx)
#include "sora_ffx.h"    // (s93 b7-ffx: Final swing fx + Final / Limit own trail windows)
#include "sora_abilfx.h"  // (s107 b19-abilfx: KH2 fx + SE of the combo-ability attacks)
#include "sora_mickey.h"  // (s109 b22-mickey: KH2 Mickey rescue; S109_HELPER_MICKEY.md)
#include "sora_skate.h"   // (s110 skate: a skateboard outside the Oman Au Shrine; S110_HELPER_SKATE.md)
#include "sora_summon.h"  // (s136 summon: Chicken Little; needs sora_party.h / sora_skate.h / sora_form.h / sora_drive.h; S136_HELPER_SUMMON2.md)
#include "sora_orbs.h"    // (s131 orbs: KH2 HP / MP / Drive orbs + munny dropped by kills; S131_HELPER_ORBS.md)
#include "sora_savept.h"  // (s131 savept: KH2 save points at the shrines; needs sora_skate.h)
#include "sora_anti.h"  // (s89 anti: Anti Form's points / random trigger / no heal / x1.5 damage; S89_HELPER_ANTI.md)
#include "sora_debugall.h"  // (s90: live "set debugall 1" testing toggle)
#include "sora_camp.h"  // (s78 KH2 pause menu slice 1)
#include "sora_net_avatar.h"  // (mp Task 6: the remote Sora; needs sora_form.h / sora_mickey.h; Task 7: CAMP.seq for the name plate)
#include "sora_net_party.h"   // (mp slice 3: the remote Donald + Goofy; the party poses for the fx anchors)
#include "sora_invsora.h"  // (s102 b16-invsora: Sora instead of Link on BotW's inventory screen; S102_HELPER_INVSORA.md)
#include "sora_meals.h"  // (s93 b7-meals: cooked meals on the Items page)
#include "sora_mkchoice.h"  // (s117 mkchoice: KH2 "I won't give up!" / "It's all over..." screen; S117_HELPER_MKCHOICE.md)
#include "sora_rcslot.h"  // (s134 rcslot: KH2's Reaction Command slot over the command menu; used by sora_react.h / sora_plimit.h)
#include "sora_react.h"  // (s131 react: Triangle reaction commands on Guardians / Lynels)
#include "sora_plimit.h"  // (s133 limits: the party Limits Comet + Whirli-Goof and the small Limit player)
#include "sora_summon_menu.h"  // (s137 smenu: Summon page / FPS pane / owned rule; the FPS helper's sora_summon_fps.h must come BEFORE this line)
#include "sora_dbgmenu.h"  // (s140 dbgmenu: the camp Debug Menu page backend; needs sora_summon_menu.h)
#include "sora_sumlv_late.h"  // (s148 sumlv: charms owned + the debug rows; needs sora_summon_menu.h / sora_dbgmenu.h)
// (s85 slice 5) the picked Keyblades to every Sora body (base, Valor, Wisdom) each frame: cheap, and it follows slot
// loads, hot reloads and body switches without hooks. An exe before s85 (no sora_draw_actor_blade) -> logged once,
// the model stays the bake's default (stats + ability still change).
_Static_assert(FORM_MASTER == 3 && FORM_FINAL == 4, "sora_kb.h kb_sub_slot hardcodes the Master / Final form ids");
static void kb_send(void) {
    static void (*blade)(int, int, int);
    static int tried;
    if (!tried) {
        tried = 1, blade = (void (*)(int, int, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_actor_blade");
        if (!blade) LOG("[sora] keyblade: exe has no sora_draw_actor_blade (before s85): the blade model stays Kingdom Key\n");
    }
    if (!blade) return;
    int ids[1 + FM_NFORMS + OF_N * (FORM_ANTI + 2)], fs[1 + FM_NFORMS + OF_N * (FORM_ANTI + 2)], n = 0;
    ids[n] = 0, fs[n++] = FORM_VALOR;
    for (int f = 0; f < FM_NFORMS; f++)
        if (FM.actor[f] > 0) ids[n] = FM.actor[f], fs[n++] = f;
    n = ofx_blade_ids(ids, fs, n);  // (s146 outfit: the outfit bodies too)
    for (int i = 0; i < n; i++) blade(ids[i], 0, kb_alt(KB.kb[KB_MAIN])), blade(ids[i], 1, kb_alt(kb_sub_id(KB.kb, fs[i])));  // (s91 mfkb: per form)
}

// (s113 anim60) "Roxas / Sora run looks lower than 60 fps". The bakes (60 Hz, 0 duplicate rows) and the clock (+1.0 KH frame per
// 16.67 ms update) check out offline (recon/S113_HELPER_ANIM60.md), so the two things that could still make it step are
// logged here, live "set animdiag 1" (default 0 = silent), one "[anim60]" line per 240 updates while he moves:
//   gaps  = ms between two driver calls bucketed (<12 / 12-14.5 / 14.5-19 / 19-25 / >25): the average (the "[sora] perf" line)
//           hides an uneven 10 / 23 ms rhythm, which shows a pose for two presents then skips one;
//   feet  = how far Link's feet moved this update vs the commanded move (vx, vz u/s x the update's seconds): stalled (< 25%),
//           normal, doubled (> 175%): Link moving every other update would look like 30 fps whatever the pose does.
static int g_ad_gap[5], g_ad_n, g_ad_stall, g_ad_dbl, g_ad_ok;
static double g_ad_cmin = 1e9, g_ad_cmax = -1e9;
static void animdiag_gap(double ms) {
    if (!g_animdiag || ms <= 0 || ms > 60) return;
    g_ad_gap[ms < 12 ? 0 : ms < 14.5 ? 1 : ms < 19 ? 2 : ms < 25 ? 3 : 4]++;
}
static void animdiag_pos(const float p[3], float vx, float vz, float ratio, float clock) {
    static float pp[3], pclock;
    static bool have;
    if (!g_animdiag) { have = false; return; }
    float step = ratio / 30.0f, want = sqrtf(vx * vx + vz * vz) * step;  // (u this update)
    float dx = p[0] - pp[0], dz = p[2] - pp[2], got = sqrtf(dx * dx + dz * dz), dc = clock - pclock;
    bool had = have;
    pp[0] = p[0], pp[1] = p[1], pp[2] = p[2], pclock = clock, have = true;
    if (!had || want < 0.02f || ratio < 0.1f) return;  // (standing, or the first sample)
    g_ad_n++;
    if (got < 0.25f * want) g_ad_stall++;
    else if (got > 1.75f * want) g_ad_dbl++;
    else g_ad_ok++;
    if (dc > -100 && dc < 100) { if (dc < g_ad_cmin) g_ad_cmin = dc; if (dc > g_ad_cmax) g_ad_cmax = dc; }
    if (g_ad_n >= 240) {
        LOG("[anim60] %d moving updates: feet stalled %d, normal %d, doubled %d; call gaps ms <12:%d 12-14.5:%d 14.5-19:%d 19-25:%d >25:%d; clock step %.2f..%.2f (ratio %.3f)\n",
            g_ad_n, g_ad_stall, g_ad_ok, g_ad_dbl, g_ad_gap[0], g_ad_gap[1], g_ad_gap[2], g_ad_gap[3], g_ad_gap[4], g_ad_cmin, g_ad_cmax, ratio);
        g_ad_n = g_ad_stall = g_ad_dbl = g_ad_ok = 0, g_ad_cmin = 1e9, g_ad_cmax = -1e9;
        memset(g_ad_gap, 0, sizeof g_ad_gap);
    }
}

// (s61 perf) the driver's cost on the game thread: every 120 calls, "[sora] perf" = avg / max ms inside
// sora_mod_frame and the avg ms between two calls (the game's frame).
// (mp) the SNAP fields from Sora's state (sora_net.h; feet / player / seq / events are filled there)
static void net_fill(NetSnap *s) {
    int f = g_fmots_on ? g_fform : FORM_VALOR;  // (base Sora's second blade = the Valor slot's, as keyblade_sync)
    s->body = (uint8_t)(g_fmots_on ? g_fform + 1 : 0);  // 0 Sora, 1..6 Valor..Anti, 7 Roxas, 8 Mickey
    s->kb_chain = (uint8_t)kb_alt(KB.kb[KB_MAIN]);
    s->kb_alt = (uint8_t)kb_alt(kb_sub_id(KB.kb, f));
    s->slot = (uint16_t)(S.mot ? S.mot->slot : 0);
    s->clock = S.clock, s->yaw = S.face;
    // (glide: real slot poses, glide_pose_draw blends them -> not a bone pose; ragdoll / Link puppet / skate rider are)
    // (s172) the skate rider: its own KH2 actor (rider + board) playing board slot SKT.rs at SKT.rc -> NF_SKATE carries that slot / clock
    // (netskate 0 = the s170 rule: a bone pose with no block = the remote's idle / fall)
    bool rag = linkpos_link_ragdoll() > 0, skate = SKT.rider_on && g_net_skate && !S.puppet && !rag;
    bool bone = S.puppet || (SKT.rider_on && !skate) || rag;
    s->flags = net_flags(!S.puppet || g_puppet_kb, ACT_AIR(S.act), NET.wet, S.special == SP_GUARD, NET.dead, false, bone);
    if (skate) s->flags |= NF_SKATE, s->slot = (uint16_t)SKT.rs, s->clock = SKT.rc, g_net_skate_sent++;
    // (s170 bonedraw) the bone block: Link's joints the retarget used (this or the 2 frames before: net_fill runs after the body's draw in
    // the same driver call), in this SNAP's own model space (its feet = g_body, RotY(S.face), KH cm), so feet / yaw / joints always agree
    s->bones.slot = 0;
    if (bone && g_net_bone && g_rcap.ok && (uint32_t)(g_drv_frame - g_rcap.frame) <= 2) {
        float sn = sinf(S.face), cs = cosf(S.face), k = 1.0f / (0.01f * g_k);
        NetBones nb;
        bool ok = isfinite(g_rcap.clock) && g_rcap.clock >= 0;  // (s170 review SF-2/3: a bad block is skipped, never clamped / never kills the SNAP)
        for (int i = 0; i < NET_BONE_J && ok; i++) {
            float dx = (g_rcap.j[i][0] - g_body[0]) * k, dy = (g_rcap.j[i][1] - g_body[1]) * k, dz = (g_rcap.j[i][2] - g_body[2]) * k;
            nb.j[i][0] = cs * dx - sn * dz, nb.j[i][1] = dy, nb.j[i][2] = sn * dx + cs * dz;
            for (int c = 0; c < 3; c++) ok = ok && isfinite(nb.j[i][c]) && fabsf(nb.j[i][c]) < 3270.0f;
        }
        for (int r = 0; r < 2 && ok; r++) {
            const float *a = r ? g_rcap.hl : g_rcap.hu;
            float *o = r ? nb.hl : nb.hu;
            o[0] = cs * a[0] - sn * a[2], o[1] = a[1], o[2] = sn * a[0] + cs * a[2];
            float L = sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
            ok = isfinite(L) && L > 1e-4f;
            if (ok) o[0] /= L, o[1] /= L, o[2] /= L;
        }
        if (ok) {
            nb.slot = (uint16_t)g_rcap.slot, nb.clock = g_rcap.clock;
            s->bones = nb;
            s->flags |= NF_BONES;
            g_net_bone_sent++;
        } else g_net_bone_skip++;
    }
    float hp = g_hud_last[1] > 0 ? g_hud_last[0] / g_hud_last[1] : 0, mp = g_hud_last[3] > 0 ? g_hud_last[2] / g_hud_last[3] : 0;
    s->hp = (uint8_t)(hp <= 0 ? 0 : hp >= 1 ? 100 : hp * 100 + 0.5f);
    s->mp = (uint8_t)(mp <= 0 ? 0 : mp >= 1 ? 100 : mp * 100 + 0.5f);
    s->npm = (uint8_t)pcap_fill(s->pm);  // (slice 3: Donald + Goofy as drawn this frame)
    // (mp4) the outfit he wears on the shown body (Debug Menu Outfit / Auto Outfits); Roxas + Mickey have none. GUESS: when his own
    // outfit actor was refused (he is drawn Normal here) the WANTED one is still sent; the peer then loads it on his side if it can
    s->outfit = (uint8_t)(g_fmots_on && (g_fform == FORM_ROXAS || g_fform == FORM_MICKEY) ? 0 : ofx_want());
}
// (slice 3) fx anchors, sender: bodies 0 Sora (this frame's g_body + S.face, what the SNAP carries), 1 Donald, 2 Goofy
static void net_bodies_local(NetBody b[3]) {
    memset(b, 0, 3 * sizeof *b);
    if (NET.have_feet && !NET.loading) {
        b[0].ok = true, b[0].feet[0] = g_body[0], b[0].feet[1] = g_body[1], b[0].feet[2] = g_body[2], b[0].yaw = S.face;
    }
    for (int w = 0; w < NET_PARTY_N; w++) b[1 + w].ok = net_party_local_pose(w, b[1 + w].feet, &b[1 + w].yaw);
}
static float net_anchor_eps_u(void) { return 0.01f * g_k; }  // (1 cm)
static int net_anchor_event(const float pos[3], float yaw, float apos[3], float *ayaw) {
    NetBody b[3];
    net_bodies_local(b);
    int i = net_anchor_pick(b, 3, pos, NET_ANCHOR_CM * 0.01f * g_k);
    if (i < 0) return NANC_WORLD;
    net_anchor_enc(&b[i], pos, yaw, apos, ayaw);
    return NANC_SORA + i;
}
static int net_anchor_move(int anchor, const float pos[3], float yaw, float apos[3], float *ayaw) {
    if (anchor < NANC_SORA || anchor > NANC_GOOFY) return NANC_WORLD;
    NetBody b[3];
    net_bodies_local(b);
    const NetBody *a = &b[anchor - 1];
    float lim = NET_ANCHOR_CM * 0.01f * g_k, dx = pos[0] - a->feet[0], dy = pos[1] - a->feet[1], dz = pos[2] - a->feet[2];
    // (body no longer drawn, or the fx flew off > 5 m: it turns world for the rest of its life - no flapping back)
    if (!a->ok || dx * dx + dy * dy + dz * dz > lim * lim) return NANC_WORLD;
    net_anchor_enc(a, pos, yaw, apos, ayaw);
    return anchor;
}
static void sora_mod_frame_body(void);
__declspec(dllexport) void sora_mod_frame(void) {
    static LARGE_INTEGER f, last;
    static double sum, mx, gap;
    static int n;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    g_drv_frame++;  // (s170 bonedraw: the stamp of g_rcap)
    mark_poll();  // (s111: F5 marks work in every state, before the body's early returns)
    pcap_begin();  // (mp slice 3: a party draw record is only good for the frame it was drawn in)
    sora_mod_frame_body();
    if (g_on) {  // (mp: one net step a frame, after the body: recv, 20 Hz SNAP, HELLO / timeouts)
        float lp[3] = {g_body[0], g_body[1], g_body[2]};
        uint64_t now = GetTickCount64();
        g_av_local_ok = NET.driven && !NET.loading && !NET.dead && !g_rox_hold;  // (the remote: only in the playing world)
        net_frame(lp, now);
        avatar_tick(now);
        net_party_tick(now);
    } else avatar_off(), net_party_off(), net_sora_off();  // (mp2: Sora mode off = BYE + socket closed, his fx copies dropped)
    world_diag();  // (s122b)
    if (g_rox_hold) sora_draw_off();  // (s111 roxload: no Sora while a loaded Roxas save waits for his assets)
    QueryPerformanceCounter(&b);
    double ms = (b.QuadPart - a.QuadPart) * 1000.0 / f.QuadPart;
    double g1 = last.QuadPart ? (a.QuadPart - last.QuadPart) * 1000.0 / f.QuadPart : 0;
    gap += g1;
    animdiag_gap(g1);
    {
        int ncc = 0;
        for (int k = 0; k < AH_MAX; k++) ncc += g_ah[k].kind && g_ah[k].cc;
        g_prof_cc = ncc;
    }
    if (g1 > 60) {  // (s80 fps hunt) one line per slow game frame, with what airhold holds
        int na = 0, ncc = 0;
        for (int k = 0; k < AH_MAX; k++) na += g_ah[k].kind != AH_FREE, ncc += g_ah[k].kind && g_ah[k].cc;
        LOG("[sora] slow frame: %.1f ms since the last driver call (driver %.2f ms), airhold %d held, %d on the controller\n",
            g1, ms, na, ncc);
    }
    last = a;
    sum += ms;
    if (ms > mx) mx = ms;
    if (++n == 120) {
        LOG("[sora] perf: driver %.2f ms avg, %.2f max, frame %.2f ms avg\n", sum / n, mx, gap / (n - 1));
        sum = mx = gap = 0, n = 0, last.QuadPart = 0;
    }
}

static void sora_mod_frame_body(void) {
    static int init;
    if (!init) {
        init = 1;
        g_on = getenv("SAVAGEWIND_SORA") && atoi(getenv("SAVAGEWIND_SORA"));
        const char *in = getenv("SAVAGEWIND_SORA_INPUT");
        if (in && *in) script_parse(in);
        if (g_on || g_nscript) linkpos_set_pad_tap(pad_tap);
        if (!g_on) {  // Link's own baseline under the scripted pad: log what Sora would see (camera, stick, body)
            if (g_nscript) slog_open();
            return;
        }
        g_show_link = getenv("SAVAGEWIND_SORA_SHOW_LINK") != NULL;
        int lv = (int)env_f("SAVAGEWIND_SORA_GLIDE_LV", (float)g_glide_lv);
        g_glide_lv = lv < 1 ? 1 : lv > 5 ? 5 : lv;
        g_air_once = env_f("SAVAGEWIND_SORA_AIR_ONCE", (float)g_air_once) != 0;
        const char *k = getenv("SAVAGEWIND_SORA_SCALE");
        if (k && *k) g_k = strtof(k, NULL);
        slog_open();
        bake_load();
        hits_load();
        rag_load_all();  // (s182)
        party_load();  // (s54, party slice 2: Donald + Goofy draw actors)
        targets_init(0.01f * g_k);
        g_jump_trigger = mot_frame_trigger(mot_of(MOT_JUMP), 0, g_jump_trigger);
        const char *cm = getenv("SAVAGEWIND_SORA_COMBO_MASTER");
        if (cm && *cm) g_combo_master = atoi(cm) != 0;
        LOG("[sora] Sora mode on (scale %.2f, Link %s, jump trigger at %.0f, Combo Master %s)\n", g_k,
            g_show_link ? "shown" : "hidden", g_jump_trigger, g_combo_master ? "on" : "off");
        HANDLE wd = CreateThread(NULL, 0, stale_watchdog, NULL, 0, NULL);
        if (wd) CloseHandle(wd);
    }
    if (g_on) g_wd_frame = vk_frame_counter(), load_probe(g_wd_frame);
    if (!g_on) {
        g_camp_req = 0;  // (s79 review: a + with Sora mode off must not open the camp later)
        float p[3], cf[2] = {0, 0};
        uint64_t t0 = linkpos_world_frame(), present = vk_frame_counter();
        if (!g_slog || !t0 || !linkpos_link_pos(p)) return;
        bool have_cam = linkpos_camera_fwd(cf);
        fprintf(g_slog, "%llu %llu -1 -1 0 | %.2f %.2f | %.3f %.3f%s | 0 0 0 | 0 | 0 0 0 | %.3f %.3f %.3f | %#x | 0 0 0 0 | %#llx\n",
                (unsigned long long)present, (unsigned long long)(present - t0), kh_axis(g_lx), kh_axis(g_ly), cf[0],
                cf[1], have_cam ? "" : "?", p[0], p[1], p[2], linkpos_link_state_word(), (unsigned long long)g_held);
        if (present % 30 == 0) fflush(g_slog);
        return;
    }
    // (s78) the KH2 pause menu owns the frame while it is up, paused or not (its open waits for BotW's pause)
    // (s96) BotW takes the manual-save picture in the first presents of a pause (live test: the pose dropped before the
    // Save dialog still showed in it; dropped at the pause start it did not). The KH2 camp opens at that same +, so its
    // menu party waits until then: the world poses stay (frozen) through CP_WAIT_PAUSE and the first g_savepic_n paused
    // presents, under the camp's backdrop.
    bool sp_hold = savepic_hold(vk_frame_counter());
    if (camp_frame(vk_frame_counter())) {
        g_nd_camp++;
        skt_quiet();  // (s110 skate review: no board roll loop under a menu / a load)
        if (sp_hold) { sora_hud_off(); return; }
        g_savepic_camp = true;
        if (!mw_frame()) sora_draw_off();
        sora_hud_off();
        return;
    }
    mw_off();  // (s92 b6-menuwalk: the screen camera must be gone before the world poses Sora)
    uint64_t t0 = linkpos_world_frame();
    if (!t0 || !*(volatile uint32_t *)(uintptr_t)PAUSE_STATE) is_off();  // (s102: playing / no world: the inventory screen is gone)
    if (!t0 || *(volatile uint32_t *)(uintptr_t)PAUSE_STATE) {  // (paused: VFR updates twice, no physics)
        g_nd_pause++;
        skt_quiet();
        sv_tab_tick(GetTickCount64());  // (s131) the save point's System tab jump runs while BotW's menu is up
        float pp[3];
        if (t0 && linkpos_link_pos(pp)) live_poll(pp, vk_frame_counter());  // (menu tests: new input while paused)
        S.fresh = true;  // (the A that closes a menu is not Sora's attack: load27e swung A151 on the loading screen)
        lflash_early(vk_frame_counter(), false);
        // (s95 b9-savepic) BotW's manual-save picture (caption.jpg) is made from a paused frame (the pause menu's Save);
        // sora_draw_off here left no actor to draw into it: Sora's pose is held (Link stays hidden: g_hide is sticky)
        // so the picture shows the party like an autosave's. Off: "set savepic 0".
        // (s95 review) only in a real pause (t0 != 0): no world frame (title, a load) drops the pose as before, so a
        // load never draws the old spot's party into the new world's first composites.
        savepic_note(vk_frame_counter(), t0 != 0);
        if (t0 && !sp_hold && is_frame()) { sora_hud_off(); return; }  // (s102 b16-invsora: Sora on the inventory screen)
        if (!t0 || !sp_hold) sora_draw_off();  // (s96: held only for the first g_savepic_n paused presents)
        sora_hud_off();
        return;
    }
    savepic_note(vk_frame_counter(), false);
    float p[3];
    if (!linkpos_link_pos(p)) { g_nd_nolink++; mg_other("load / no Link"); pl_cancel("load / no Link"); sm_end("load / no Link"); lflash_early(vk_frame_counter(), true); drive_off(); sora_draw_off(); sora_hud_off(); skt_quiet(); return; }
    uint64_t present = vk_frame_counter();
    g_body[0] = p[0], g_body[1] = p[1], g_body[2] = p[2];
    NET.driven = true;  // (mp: this frame's SNAP carries Link's position; else NF_LOADING, sora_mod_frame -> net_frame)
    live_poll(p, present);
    goto_tick(p);  // (s108 b20-lynel: the held long teleport)
    probe_tick(present, p);  // (s85g: the item popup probe)
    chestev_tick(present, p);  // (s94 b8-chestev)
    sk_recon_tick(present);  // (s93 b7-runes: R1 rune flags / R2 R3 logs, off unless "set slaterec")
    unit_poke_tick();
    snap_tick(p, present);
    sl_tick();  // (s54 levels: kill rule -> EXP / level ups)
    ul_tick();  // (s89 kb: BotW milestones -> Keyblades, the form gate)
    ca_tick();  // (s102 b16-chestabil, s103: chests counted from BotW's TBox flags -> KH2 abilities)
    {  // (s86) "set probe 2": the Link-actor probe starts at every kill (the camp-cleared chest reveal's flags)
        static uint32_t kills_was;
        if (g_probe_on == 2 && SL.kills != kills_was && kills_was) PR.on = false, probe_start(present);  // (restarts: the last kill's)
        kills_was = SL.kills;
    }
    static int rag_tries;
    if (!g_rag_bones && rag_tries < 1 && present % 120 == 0) rag_tries++, rag_find();
    if (!g_rag_bones && g_nrag_hit) {
        uint64_t la = link_actor();
        if (la) {
            const float *mt = (const float *)(uintptr_t)(la + 0x398);
            float ft[3] = {mt[3], mt[7], mt[11]};
            rag_hits_dump(linkpos_link_ragdoll(), present, ft);
        }
    }
    if (g_rag_bones) {  // (the dump for the offline retarget: one standing frame, then every ragdoll frame)
        static FILE *bf;
        static int dumped;
        int r = linkpos_link_ragdoll();
        if (!bf) bf = fopen(sora_path("D:/SoraBotW/recon/probe/linkbones.txt"), "a");
        if (bf && ((r > 0 && present % 5 == 0) || !dumped)) {
            dumped = 1;
            const float *m = (const float *)(uintptr_t)g_rag_bones;
            fprintf(bf, "frame %llu rag %d face %.4f layout %d feet %.4f %.4f %.4f\n", (unsigned long long)present, r, S.face,
                    g_bones_layout, p[0], p[1], p[2]);
            for (int b = 0; b < LINK_BONES; b++) {  // (g3d: x axis, y axis, z axis, translation; sead: its 3 rows)
                const float *k = g_bones_layout == 2 ? m + b * 16 : m + b * 12;
                int st = g_bones_layout == 2 ? 4 : 4, nr = g_bones_layout == 2 ? 4 : 3, nc = g_bones_layout == 2 ? 3 : 4;
                fprintf(bf, "%d", b);
                for (int r2 = 0; r2 < nr; r2++)
                    for (int c = 0; c < nc; c++) fprintf(bf, " %.4f", k[r2 * st + c]);
                fprintf(bf, "\n");
            }
            fflush(bf);
        }
    }
    if (!g_active) {
        g_active = true;
        memset(&S, 0, sizeof S);
        S.fresh = true, S.edge_swing = -1;
        S.mot = mot_of(S.motion);  // (idle is motion 0 = the memset's: set_motion never changes it, so no draw: load27f)
        S.prev_z = p[1] * 100 / g_k;
        float f[2];
        if (linkpos_camera_fwd(f)) S.face = atan2f(f[0], f[1]);
        hide_link(!g_show_link);
        LOG("[sora] driving from presented frame %llu at %.3f %.3f %.3f\n", (unsigned long long)present, p[0], p[1], p[2]);
    }
    int st = linkpos_link_state(), life = linkpos_link_life();
    life = ca_survive(life);  // (s102 b16-chestabil: Second Chance / Once More)
    mk_tick(life, present);  // (s109 b22-mickey: KH2's Mickey rescue state machine)
    // Dead (life_s16a) until he stands again with life: Continue gives the life back while the Game Over menu and the
    // loading tip are still up (state 0x80080000 = air), which drew Sora and the HUD over both (go26a, presents
    // 59101-59251; the frames never stop, so the watchdog below doesn't see it). Same rule as linkpos's auto-Continue,
    // then DEAD_HOLD more presents: he stands while the tip still fades out (go26b shot 8, BotW's hearts already up).
#define DEAD_HOLD 60
    static bool dead;
    static uint64_t up_at;  // present he stood again (0: not yet)
    if (life == 0 && !dead) dead = true, up_at = 0, LOG("[sora] Link died: Sora + HUD off until he stands again\n");
    else if (dead && life > 0 && st == LINK_GROUND) {
        if (!up_at) up_at = present;
        if (present - up_at >= DEAD_HOLD) dead = false, S.fresh = true, LOG("[sora] back after the Game Over\n");
    } else if (dead) up_at = 0;
    static uint64_t life_at;  // (s87c review, link flash) present life came back while still "dead" (0: life 0 / alive)
    life_at = !dead || life == 0 ? 0 : life_at ? life_at : present;
    if (dead || g_loading) st = LINK_OTHER, S.fresh = true;  // (the A that started the load is not an attack after it)
    gameover_tick(dead, present);  // (s59: the KH2 Game Over screen while BotW's 0x30 is up)
    NET.dead = dead, NET.loading = g_loading;  // (mp: SNAP flags NF_DEAD / NF_LOADING)
    // Swim latch (s46): in a lake the state word flips swim 0x410 <-> air 0x18000002, and on each air stretch Sora's
    // fall servo pushed Link down (ghost3: surface y 125 -> 35 in 370 presents). Single ground frames come too (swim1).
    // After a swim frame, ground / air stay Link's (puppet) until he stands SWIM_STAND presents in a row, or SWIM_HOLD
    // presents without a swim frame.
#define SWIM_HOLD 600
#define SWIM_STAND 20
    static uint64_t swim_at;
    static int stand_n;
    uint32_t sw_now = linkpos_link_state_word();
    stand_n = st == LINK_GROUND ? stand_n + 1 : 0;
    if (sw_now == 0x410) {
        if (!swim_at) LOG("[sora] swim latch on\n");
        swim_at = present, stand_n = 0;
    } else if (swim_at && (stand_n >= SWIM_STAND || present - swim_at > SWIM_HOLD))
        swim_at = 0, LOG("[sora] swim latch off (%s)\n", stand_n >= SWIM_STAND ? "standing" : "timeout");
    // (s145 user: "i went through the ground into some water which caused the no clip to stop") No Clip keeps driving in water:
    // no swim latch, and BotW's swim word counts as air (dbm_clip_tick holds the body by position, Link's input stays blocked)
    if (g_dbm_clip && !dead && !g_loading) {
        if (swim_at) swim_at = 0, LOG("[dbgmenu] no clip: swim latch dropped (No Clip flies through water)\n");
        if (sw_now == 0x410) st = LINK_AIR;
    }
    g_rag_swim = swim_at != 0;  // (s118 mkswim: Mickey's swim height)
    if (swim_at && (st == LINK_AIR || st == LINK_GROUND)) st = LINK_OTHER;
    // (s88 b3-climb B) the kick-up: X at a wall (the climb try, ground start) makes BotW's Link hop / kick up to the wall
    // while his state word still reads "in the air" (0x8xxxxxxx) before the grab (0x2 / 0x90). Sora used to draw his own
    // Fall there ("walk-off (dropping)", log s47/s52/s53). Now that stretch is Link's: latch on when Link leaves the
    // ground inside a ground climb try, off when he stands again (3 presents), a grab takes over by itself (0x2 / 0x90
    // are LINK_OTHER anyway), or after KICK_HOLD presents. Live "set kickup 0/1".
#define KICK_HOLD 60
    {
        static uint64_t kick_at;
        static int kstand;
        kstand = st == LINK_GROUND ? kstand + 1 : 0;
        bool in_try = present < g_climb_until && !g_climb_air && !g_climb_nox;
        if (g_kickup && !kick_at && in_try && st == LINK_AIR && (sw_now & 0x80000000u) && !S.air_frames && S.act == ACT_IDLE && !S.special) {
            kick_at = present, kstand = 0;
            LOG("[sora] kick-up: Link left the ground in the climb try (state %#x), Sora takes his pose\n", sw_now);
        } else if (kick_at && (!g_kickup || kstand >= 3 || present - kick_at > KICK_HOLD || dead || g_loading)) {
            LOG("[sora] kick-up over (%s) after %llu presents, state %#x\n", kstand >= 3 ? "Link stands" : present - kick_at > KICK_HOLD ? "timeout" : "off", (unsigned long long)(present - kick_at), sw_now);
            kick_at = 0;
        }
        if (kick_at && (st == LINK_AIR || st == LINK_GROUND)) st = LINK_OTHER;
    }
    // s85f #1 (glitch list): talks, item pickups and some cutscenes leave Link's state word at 0, which linkpos reads
    // as LINK_AIR (a liftoff frame), so Sora kept driving - walking, swinging - and the A block ate BotW's text advance
    // (last/prev.log: the old man, word 0 after the 0x1 walk-up, screen 0x4a up). A 0 held g_ev_zero presents (or
    // g_ev_zero4a with the message screen) = an event: Link's, Sora takes his pose (puppet) like in 0x1.
    {
        static int zero_n;
        static bool was;
        if (sw_now) {  // (calibration: every 0 run of 3+ presents that ended, so the thresholds can be read off the log)
            if (zero_n >= 3 && !was) LOG("[sora] state 0 run of %d presents (screen 0x4a %d): not an event\n", zero_n, g_msg_screen);
            zero_n = 0;
        } else zero_n++;
        bool ev = !dead && !g_loading && zero_n >= (g_msg_screen ? g_ev_zero4a : g_ev_zero);
        if (ev != was) LOG("[sora] event stretch %s (state %#x, 0 x%d, screen 0x4a %d)\n", ev ? "on: Link's pad" : "off", sw_now, zero_n, g_msg_screen);
        was = ev;
        g_cxp_zero = ev;  // (s107 b19-chest)
        if (ev) st = LINK_OTHER;
        g_event = ev || (sw_now == 0x1 && !dead) || g_chev;
    }
    cprev_tick(st, dead);  // (s106 b18-chestanim: set chestprev <slot>)
    cx_tick(present, p);  // (s106 b18-chestanim: set chestanim 2 = recon log)
    // (s99 b14-bed) the wake-up scene from the first world frame (see g_wake above)
    {
        static bool seen_world, rec, armed, moved_ok;
        static uint64_t last_seen, wake_at, rec_at, zero_n, end_at;
        static uint32_t sw_was;
        static float end_p[3];
        float wv = 0, wh = 0, wdxz = hypotf(p[0] - BED_POS[0], p[2] - BED_POS[2]), wdy = fabsf(p[1] - BED_POS[1]);
        bool entry = !last_seen || present - last_seen > 30;  // (a world frame after a gap: a load / the title)
        last_seen = present;
        if (g_wake && entry && !dead && !g_loading && (g_wake >= 3 || !seen_world) && wdxz <= (float)g_bed_r && wdy <= 6.0f) {
            rec = true, rec_at = present, armed = g_wake != 2, wake_at = present, zero_n = 0, moved_ok = false, end_at = 0;
            LOG("[wake] world entry at present %llu near the bed (%.1f m, dy %.1f): %s\n", (unsigned long long)present, wdxz, wdy,
                g_wake == 2 ? "recon log only" : "Sora lies from this frame");
            if (armed) g_wake_on = true, g_wake_used = true, LOG("[wake] latched at present %llu: state %#x pos %.2f %.2f %.2f, Sora takes Link's pose, Donald / Goofy hidden\n",
                                                          (unsigned long long)present, sw_now, p[0], p[1], p[2]);
        }
        if (g_wake && entry) seen_world = true;
        if (rec && g_wake) {
            bool have = bed_extent(&wv, &wh);
            if (sw_now != sw_was || (g_wake_log && (present - rec_at) % (uint64_t)g_wake_log == 0)) {
                float cf[2] = {0, 0}, cv[12] = {0};
                bool c1 = linkpos_camera_fwd(cf), c2 = linkpos_camera_view(cv);
                LOG("[wake] rec +%llu presents: state %#x%s pos %.2f %.2f %.2f (%.1f m from the bed, dy %.1f) bones %s vert %.2f horiz %.2f | camera fwd %s%.2f %.2f view[9..11] %s%.2f %.2f %.2f | loading %d dead %d%s\n",
                    (unsigned long long)(present - rec_at), sw_now, sw_now != sw_was ? " (CHANGED)" : "", p[0], p[1], p[2], wdxz, wdy,
                    have ? "ok" : "none", wv, wh, c1 ? "" : "(none) ", cf[0], cf[1], c2 ? "" : "(none) ", cv[9], cv[10], cv[11], g_loading, dead, g_wake_on ? " [Sora lies]" : "");
            }
            sw_was = sw_now;
            zero_n = sw_now ? 0 : zero_n + 1;
            if (g_wake_on) {
                const char *why = !g_wake || g_wake == 2 ? "off" : dead ? "dead" : g_loading ? "loading" : wdxz > 40.0f ? "left the area"
                                  : present - wake_at > (uint64_t)g_wake_max ? "timeout"
                                  : zero_n >= (uint64_t)g_wake_dx && present - wake_at >= 300 ? "state word 0 (the Demo hands over to the event stretch)" : NULL;
                if (why) {
                    g_wake_on = false, end_at = present, memcpy(end_p, p, sizeof end_p);
                    party_unplace_all();  // (the first tick places them: "placed (first frame)")
                    LOG("[wake] end: %s after %llu presents, state %#x pos %.2f %.2f %.2f (%.1f m from the bed) bones vert %.2f horiz %.2f\n", why,
                        (unsigned long long)(present - wake_at), sw_now, p[0], p[1], p[2], wdxz, wv, wh);
                }
            } else if (!end_at && g_wake == 2 && zero_n >= (uint64_t)g_wake_dx && present - rec_at >= 300) end_at = present, memcpy(end_p, p, sizeof end_p);
            if (end_at && sw_now && hypotf(p[0] - end_p[0], p[2] - end_p[2]) > 1.0f) moved_ok = true;  // (the player walked)
            if (moved_ok || present - rec_at > 12000 || dead) rec = false, LOG("[wake] recon log over (%s) at present %llu\n", moved_ok ? "the player moved" : dead ? "dead" : "12000 presents", (unsigned long long)present);
        }
        if (!g_wake && g_wake_on) g_wake_on = false;
        if (g_wake_on) st = LINK_OTHER, g_event = true;
    }
    // (s94 bed) Link lying on the Shrine of Resurrection's bed / getting up: Link's, Sora puppets his bones
    {
        static bool bed_on, bed_block;  // (s94 review) bed_block: a timeout does not re-latch until upright / away from the bed
        static uint64_t bed_at, up_n, said;
        float vert = 0, horiz = 0, dxz = hypotf(p[0] - BED_POS[0], p[2] - BED_POS[2]), dyb = fabsf(p[1] - BED_POS[1]);
        bool near_bed = g_bedpose >= 2 || (dxz <= (float)g_bed_r && dyb <= 6.0f);
        bool look = near_bed || bed_on || dxz <= (float)g_bed_r;  // (s94 review: no bone walk far from the bed)
        bool have = g_bedpose && !g_wake_used && look && !dead && !g_loading && bed_extent(&vert, &horiz) && horiz >= BED_MIN_EXT;
        bool flat = have && vert < BED_FLAT * horiz, upright = have && vert >= BED_UPRIGHT * horiz;
        if (g_bed_log && g_bedpose && dxz <= (float)g_bed_r && present - said >= 120)
            said = present, LOG("[sora] bed: probe present %llu state %#x pos %.2f %.2f %.2f (%.1f m from the bed) bones vert %.2f horiz %.2f -> %s%s\n",
                                (unsigned long long)present, sw_now, p[0], p[1], p[2], dxz, vert, horiz, flat ? "lying" : upright ? "upright" : "between", bed_on ? " [latched]" : "");
        if (bed_block && (upright || !near_bed)) bed_block = false;
        if (!bed_on && !bed_block && flat && near_bed) {
            bed_on = true, bed_at = present, up_n = 0;
            LOG("[sora] bed: Link lies flat (vert %.2f horiz %.2f, state %#x, %.1f m from the bed): Sora takes his pose\n", vert, horiz, sw_now, dxz);
        } else if (bed_on) {
            up_n = upright ? up_n + 1 : 0;
            const char *why = !g_bedpose ? "off" : dead ? "dead" : g_loading ? "loading" : up_n >= (uint64_t)g_bed_up ? "upright" : present - bed_at > BED_TIMEOUT ? "timeout" : NULL;
            if (!why && !have && present - bed_at > 10) why = "no bones";
            if (why) bed_on = false, bed_block = !strcmp(why, "timeout"), LOG("[sora] bed: over (%s) after %llu presents, state %#x, vert %.2f horiz %.2f\n", why, (unsigned long long)(present - bed_at), sw_now, vert, horiz);
        }
        if (bed_on) st = LINK_OTHER, g_event = true;
    }
    sh_tick(present);  // (s94 b8-slatehip)
    bh_tick();  // (s102 bombhand)
    wpn_tick(!dead && !g_loading);  // (s128 wpn: Link's BotW weapon in Sora's hand)
    st = sk_tick(present, st, sw_now);  // (s93 b7-runes: Slate mode = Link's pad + Sora puppets him, like the swim latch)
    orb_tick(present, p, dead || g_loading);  // (s131 orbs)
    sv_tick(present, p, dead || g_loading);   // (s131 savept)
    skt_tick(present, st, p, dead || g_loading);  // (s110 skate: the board's place + draw, auto get-off)
    sm_tick(present, st, p, dead || g_loading);   // (s136 summon: the cast / call / gauge / aborts)
    bool wet = swim_at || link_water_depth() > 0.02f;  // (swimming or wading: the water surface hides his body)
    {   // (s107 b19-chest: set chestanim 1) the chest-open playback; the known events are excluded, see sora_chestanim.h
        uint32_t lo = sw_now & 0xffff;
        bool near_bed = hypotf(p[0] - BED_POS[0], p[2] - BED_POS[2]) <= (float)g_bed_r + 3.0f && fabsf(p[1] - BED_POS[1]) <= 6.0f;
        bool known = dead || sw_now == 0x1 || lo == 0x2 || lo == 0x90 || lo == 0x8002 || wet || g_chev || g_wake_on || SK.on || g_frz_id >= 0 || g_msg_screen || near_bed;
        cxp_tick(present, st, sw_now, known, p);
    }
    sora_draw_ghost(wet ? g_swim_ghost / 100.0f : 0.0f);
    NET.wet = wet;  // (mp: SNAP flag NF_GHOST)
    warp_tint(0);  // (s54: only the 0x1 puppet below glows)
    light_tick();  // (s121 a1 light)
    blob_tick();   // (s126: KH2 blob keep / off / auto)
    if (st != LINK_GROUND && st != LINK_AIR) {  // climbing, swimming, a cutscene...: Link's (the servo can't drive)
        rox_tick();  // (s110: the body switch also runs while Link has the pad: the new game's wake-up / walk-out cutscenes kept Sora until the first save)
        static uint64_t said;
        uint32_t sw = linkpos_link_state_word();
        if (present - said > 60) said = present, LOG("[sora] Link state %#x: not driving (Link gets the pad)\n", sw);
        // (s88 b3-climb A) the glide hand-over's X opened BotW's paraglider (0x8002) and no wall was grabbed by the end
        // of the try: close it again (B tap, pad_tap) so Sora is not left in a paraglider pose. A grab (0x2 / 0x90) ends the watch.
        // (s88 review) a grab ends the glide try at once: its X pulses would be BotW's climbing jump (stamina) and the
        // into-wall stick would override the player's climb
        if (g_climb_air && present < g_climb_until && ((sw & 0xffff) == 0x2 || (sw & 0xffff) == 0x90))
            g_climb_until = present, g_gcl_eatb = g_gcl_holdb,
            LOG("[sora] glide->climb: Link grabbed (state %#x), try over: the pad is the player's%s\n", sw, g_gcl_holdb ? " (held B eaten until let go)" : "");
        if (g_gcl_watch_until) {
            if (present >= g_gcl_watch_until || (sw & 0xffff) == 0x2 || (sw & 0xffff) == 0x90) {
                if (present < g_gcl_watch_until) LOG("[sora] glide->climb: grabbed (state %#x)\n", sw);
                g_gcl_watch_until = 0;
            } else if ((sw & 0xffff) == 0x8002 && present >= g_climb_until + 2) {
                g_gcl_cancel_until = present + 4, g_gcl_watch_until = 0;
                LOG("[sora] glide->climb: no wall grab, closing BotW's paraglider (state %#x)\n", sw);
            }
        }
        drive_off();
        linkpos_block_link_input(false);
        g_link_owns = !dead && !g_loading;
        // Puppet (session 28): in Link's own states (climbing 0x2 / 0x90, swimming, gliding...) BotW moves him with his
        // own animation and stamina; Link stays hidden and Sora takes his pose bone for bone (the ragdoll retarget on
        // the idle pose), facing his way. Not on Game Over / loads. 0x1 (s54, option B: the map warp-out/in; save
        // loads gave no real pose in s28) only while his bones pass link_bones_sane - else both hidden as in s53.
        float lm[12];
        const Mot *idle = mot_of(0);
        bool ev_ok = true;
        static bool was_ev, last_ev_ok, was_vis;
        // s54 glow: a 0x1 stretch with no load just before it = the warp-out (blue ramps up to the 0x55 card); the next
        // one right after a load = the arrival (starts blue, fades); after a load with no warp-out first = a save
        // load's spawn (no glow)
        static int ev_kind;  // 0 none, 1 warp-out, 2 arrival
        static uint64_t ev_at, out_at;
        bool vis = sw == 0x1 && !dead && !g_loading;
        if (vis && !was_vis) {
            bool after_load = g_load_end && present - g_load_end < 600, pending = out_at && present - out_at < 7200;
            ev_kind = !after_load ? 1 : pending ? 2 : 0, ev_at = present;
            if (ev_kind == 1) out_at = present;
            else out_at = 0;
            LOG("[sora] warp tint: 0x1 entry = %s\n", ev_kind == 1 ? "warp-out" : ev_kind == 2 ? "arrival" : "none (after a load)");
        }
        was_vis = vis;
        if (vis) {
            float maxd = -1;
            ev_ok = g_puppet_ev && link_bones_sane(&maxd);
            if (!was_ev || ev_ok != last_ev_ok)
                LOG("[sora] puppet in 0x1: bones %s (max dist %.2f u, bones %llx)%s\n", ev_ok ? "sane" : "junk", maxd,
                    (unsigned long long)link_bones_now(), g_puppet_ev ? "" : " [set puppetev 0]");
            last_ev_ok = ev_ok;
        } else if (sw == 0x1) ev_ok = false;
        was_ev = sw == 0x1;
        // (s57 user: "when sora dies i dont see link go into rag doll, it shows sora over him") dying = life 0, before
        // Continue gives life back (the Game Over menu / tip after it keep the old rule): Sora takes Link's death pose
        // (his ragdoll) while his bones pass link_bones_sane, Link hidden, no HUD
        float dmax = -1;
        bool dying = g_puppet_dead && dead && life == 0 && !up_at && !g_loading && !GO.on && link_bones_sane(&dmax);
        {
            static int last_dying = -1;
            if (dead && dying != last_dying)
                LOG("[sora] dying puppet %s (bones max dist %.2f u)\n", dying ? "on" : "off", dmax), last_dying = dying;
            if (!dead) last_dying = -1;
        }
        // (S32 glide port: the paraglider-glide puppet block is gone; the glide is Sora's own ACTION 17/18)
        bool puppet = g_puppet && (!dead || dying) && !g_loading && ev_ok && idle && linkpos_link_mtx(lm);
        if (puppet) {
            static uint64_t n;
            float face = atan2f(lm[2], lm[10]);
            puppet = rag_retarget(idle->slot, 0, face, 0.01f * g_k, ++n % 30 == 1);
            if (puppet) {
                if (!S.puppet) LOG("[sora] Link state %#x: Sora takes Link's pose (puppet)\n", sw);
                S.face = face;
                hp_apply(g_handpose == 2 || (g_handpose == 1 && SK.on));  // (s102 handpose)
                cxp_blend(g_rpose);  // (s107 b19-chest: KH2 chest motion over Link's pose)
                if (!g_puppet_kb) keyblade_hide_rig(g_rpose, RIG_CUR), keyblade2_hide(g_rpose);  // (s110 roxpup: the rig in use)
                bool gone = false;  // (s54: fully dissolved like BotW's Link at v = 1: not drawn)
                if (sw == 0x1) {
                    float wv = g_wtsrc ? link_warp_value() : -1;
                    static float l_wv = -2;  // (probe: BotW's ramp, a line per 0.05 step)
                    if (fabsf(wv - l_wv) >= 0.05f || (wv == 0) != (l_wv == 0))
                        LOG("[sora] warp value %.3f at present %llu (ev_kind %d)\n", wv, (unsigned long long)present,
                            ev_kind), l_wv = wv;
                    if (wv >= 0) warp_tint(wv), gone = wv * 100 >= g_wt_hide;
                    else if (ev_kind) {
                        float t = (float)(present - ev_at);
                        warp_tint(ev_kind == 1 ? smooth01(g_wt_out0, g_wt_out1, t) : 1 - smooth01(g_wt_in0, g_wt_in1, t));
                    }
                }
                if (gone) sora_draw_off();
                else sora_draw_world(g_rpose, face, 0.01f * g_k);
                hide_link(!g_show_link);
                if (dying) sora_hud_off();
                else hud_update();
            }
        }
        if (!puppet) { g_hw = 0, g_hw_t = 0; if (g_hp_on) LOG("[handpose] off\n"), g_hp_on = false; }  // (s102 handpose)
        if (!puppet && S.puppet) LOG("[sora] puppet off (state %#x)\n", sw);
        S.puppet = puppet;
        if (!puppet) {
            // (s50, roadmap 5: in the DEAD_HOLD after Continue he stands under the fading tip - Link stays hidden
            // there too, Sora comes after it; while dying / on the Game Over menu BotW's own Link is shown)
            // (s53, FAST_TRAVEL_HIDE_RECON.md) state 0x1 = an event Link (the map warp: from the map closing to the
            // 0x55 card, again on arrival; save loads): he stays hidden too - un-hiding him here showed him (waist up)
            // before the warp-out dust. [GUESS] a story cutscene in 0x1 would show neither of them.
            bool event = sw == 0x1 && !dead, hide = ((dead && up_at) || event) && !g_show_link;
            if (g_lflash && !g_show_link && !hide) {  // (s87c link flash)
                // (s87c review: "dead" only while life is 0 + LF_POST after it returns - a fairy revive then a swim / climb
                // keeps "dead" until he stands, which would leave him hidden with no puppet)
                const char *why = dead && (!life_at || present - life_at < LF_POST) ? "dead" : g_loading ? "loading" : g_load_end && present - g_load_end < LF_POST ? "post-load"
                                  : g_lf_bad && present - g_lf_bad < LF_POST ? "post-respawn" : NULL;
                static const char *last_why;
                if (why) hide = true;
                if (why != last_why && why) LOG("[sora] link flash: Link kept hidden (%s, state %#x) at present %llu\n", why, sw, (unsigned long long)present);
                last_why = why;
            }
            static int last_hide = -1, last_sw = -1;
            if (hide != last_hide || (int)sw != last_sw)
                LOG("[sora] hide probe: state %#x dead %d up_at %d loading %d -> Link %s\n", sw, dead, up_at != 0,
                    g_loading, hide ? "hidden" : "SHOWN"), last_hide = hide, last_sw = (int)sw;
            hide_link(hide);
            sora_draw_off();
            sora_hud_off();
        }
        gameover_draw();  // (s59: KH2's Game Over float over BotW's 0x30 screen; nothing when off)
        mc_other();  // (s117 review: the KO choice screen also while Link has the pad - a climb / swim / knock-down - else it hid there)
        S.act = ACT_IDLE, S.takeoff = 0, S.req_set = false, S.special = SP_NONE;
        gl_from_motion = -1, g_gl_tail = false, g_gblend = -1;  // (a glide that climbed: no glide blend afterwards)
        S.fresh = true;  // (the B that let go of a wall is not Sora's jump after it: letgo29b jumped +185 cm mid-air)
        // (s68 drive: revert on death / load, base body while not driven; s87 user: "when i die and start to ragdoll in
        // drive forms it just shows normal soras model" - the dying puppet keeps the form's body, the revert waits for
        // the Game Over screen / the end of the death pose; live "set fmdie 0" = the old instant revert)
        fm_other(g_loading || (dead && !(dying && g_fm_keep_dying)));
        pl_cancel("Link's own state (climb / swim / cutscene / load / death)");  // (s133 limits)
        party_other(dead || g_loading || g_wake_on);  // (s54 party3: Donald + Goofy off while not driven, kept while blown)
        dbm_clip_other();  // (s143)
        mg_other(g_loading ? "load" : dead ? "Sora KO" : "Link has the pad");  // (s133 review: Magnet / Reflect never freeze while not driven)
        return;
    }
    g_link_owns = 0;
    dbm_clip_tick(p);  // (s143 dbggod: No Clip, driven frames only: events / climb / swim / loads / the KO choice get the full pad)
    if (S.puppet) S.puppet = false, LOG("[sora] puppet off (driving again)\n");  // (else the next puppet isn't logged: swim30)
    float ratio = framepace_game_ratio();
    if (!(ratio > 0.1f && ratio < 3)) ratio = 1;
    const float kh = KH_PHYS_STEP * ratio, to_u = 0.01f * g_k;
    float z_up = p[1] * 100 / g_k;
    floor_ray_tick(p, present, ACT_AIR(S.act));  // (s54: the floor under him in the air; last frame's ACTION decides)
    // the blob's floor = the last grounded height (S35_HELPER_S_SHADOW.md); (s54) in the air the ray's floor when it hit
    float shadow_y = S.ground_z * g_k / 100, fcm;
    if (g_fshadow && ACT_AIR(S.act) && floor_under(&fcm) == FS_RAY) shadow_y = FR.y;
    sora_draw_shadow(shadow_y, blob_str(0.55f));  // (s126 blob)
    // (S32 glide port: the paraglider hand-off's g_glide 1/2 blocks are gone; the glide is Sora's own ACTION 17/18)
    linkpos_block_link_input(present >= g_climb_until && g_frz_id < 0 && !g_chev);  // (s86: a freeze screen: Link's pad, A closes it)
    hide_link(!g_show_link);

    // input: held now, pressed since the last frame
    static uint64_t prev;
    uint64_t held = g_held, seen = __atomic_exchange_n(&g_seen, 0, __ATOMIC_RELAXED) | held;
    if (mk_eat_x()) held &= ~(uint64_t)BTN_CLIMB, seen &= ~(uint64_t)BTN_CLIMB;  // (s110m: Triangle belongs to Mickey's D-Charge / Healing Light)
    bool fresh = S.fresh;
    if (S.fresh) prev = seen, S.fresh = false;  // (auto-Continue's A that brought Link in is not Sora's attack)
    uint64_t press = seen & ~prev;
    if (nl_st() == 2 && NL.log && press) LOG("[summon] nl raw: press 0x%llx seen 0x%llx held 0x%llx prev 0x%llx\n", (unsigned long long)press, (unsigned long long)seen, (unsigned long long)held, (unsigned long long)prev);  // (s140 diag: before mc / pl / fps)
    prev = held;
    // (s117 mkchoice: the KO choice screen owns the pad the same way as the Dream picker below; s117 review: BEFORE skt_input,
    // else the Triangle that the screen ignores got Sora on / off the board)
    bool mc_ui = mc_input(&press, &held);
    uint64_t pl_x = pl_eat_x() ? (press & (uint64_t)BTN_CLIMB) : 0;  // (s133 review: the Comet Rain / Whirli-Goof Triangle must not also climb, pick up or board)
    press &= ~pl_x;
    sm_fps_input(&press, &held);  // (s137 fps) Balls / Firecracker / End on the face buttons, R1 = target, before the Reaction Command sees them
    rc_tick(p, &press, &held, st == LINK_GROUND && S.act == ACT_IDLE && !S.special);  // (s131 react)
    skt_input(&press, &held, present, st);  // (s110 skate: Triangle gets on / off, the board eats swing / roll / X)
    // (s54 dream) the picker owns the pad while it is up: no presses, nothing held, no stick (below) - Sora stands
    bool dream_ui = dream_tick(present, st == LINK_GROUND && S.act == ACT_IDLE && !S.special, life, &press, &held);
    dream_ui |= mc_ui;
    int cast = menu_tick(&press, held, kh);  // (eats the presses the menu used)
    if (pl_hold()) cast = -1;  // (s133 limits: Sora plays the Limit's motions: no command; s133 review: before the Drive dispatch, KH2 takes none)
    if (cast >= CMD_FORM && cast < CMD_FORM + FM_NFORMS) fm_drive(cast - CMD_FORM, false), cast = -1;  // (s68 drive)
    else if (cast >= CMD_LIMIT && cast < CMD_LIMIT + LM_N) lm_request(cast - CMD_LIMIT), cast = -1;  // (s89 Limit Form)
    else if (cast >= CMD_PLIMIT && cast < CMD_PLIMIT + PL_NLIM) pl_request(cast - CMD_PLIMIT), cast = -1;  // (s133 limits)
    else if (cast >= CMD_SUMMON_CAST && cast <= CMD_SUMMON_FPS) smn_cast(cast), cast = -1;  // (s137 smenu)
    else if (cast >= CMD_SLATE && cast < CMD_SLATE + 8) sk_request(cast - CMD_SLATE, st, present), cast = -1;  // (s93 b7-runes)
    else if (cast == CMD_REVERT) fm_revert("Revert command"), M.dead = 8, M.cursor = 0, cast = -1;
    bool r1 = (press & BTN_LOCK) != 0;       // (taken before a reaction zeroes the presses: the block ticks anyway)
    mp_tick(kh);
    float sx = kh_axis(g_lx), sy = kh_axis(g_ly), mag = sqrtf(sx * sx + sy * sy);
    if (g_stick_round) {
        // (s65 user: diagonals walked on the pad, not on the keyboard) KH2's per-axis dead zone of 60 / 127.5 assumes
        // the PS2 pad's square reach; a round stick gives ~90 per axis on a diagonal -> 0.44 each, |v| 0.63 < the run
        // 0.8. Same dead zone as a radius instead. Live "set stickround 0" = the per-axis rule.
        float rx = (float)g_lx / 32767.0f * 127.5f, ry = (float)g_ly / 32767.0f * 127.5f, r = sqrtf(rx * rx + ry * ry);
        mag = r <= KH_STICK_DZ ? 0 : (r - KH_STICK_DZ) / (127.5f - KH_STICK_DZ);
        if (mag > 1) mag = 1;
        sx = mag > 0 ? rx / r * mag : 0, sy = mag > 0 ? ry / r * mag : 0;
    }
    if (mag > 1) sx /= mag, sy /= mag, mag = 1;
    // (s54 dream: the frame after it closes is fresh = no attack from the A that closed it. s56: only then - fresh
    // EVERY frame it was up made prev = seen each frame, so the picker itself never saw a press)
    static bool dream_was;
    if (dream_ui) sx = sy = mag = 0;
    else if (dream_was) S.fresh = true;
    dream_was = dream_ui;
    float cf[2] = {0, 1};  // (no camera yet: the stick's up is +z)
    static int world_stick = -1;
    if (world_stick < 0) world_stick = getenv("SAVAGEWIND_SORA_WORLD_STICK") != NULL;
    bool have_cam = !(g_wstick >= 0 ? g_wstick : world_stick) && linkpos_camera_fwd(cf);
    if (sm_fps_view(cf)) have_cam = true;  // (s137 review S4: FPS Mode walks relative to the first-person view, not the scanned BotW camera)
    // right = forward x up (y-up, right-handed): (-fz, fx)
    float dir[2] = {cf[0] * sy - cf[1] * sx, cf[1] * sy + cf[0] * sx}, dl = sqrtf(dir[0] * dir[0] + dir[1] * dir[1]);
    float speed = mag <= 0 ? 0 : mag >= KH_RUN_STICK ? F_RUN : F_WALK;
    if (dl > 1e-4f) dir[0] /= dl, dir[1] /= dl;
    else speed = 0;
    float tgt[2];
    if ((held & BTN_TEST_ENEMY) && nearest_enemy(p, tgt)) {  // (test pad: walk to the nearest enemy, stop at 1.2 u)
        float ex = tgt[0] - p[0], ez = tgt[1] - p[2], ed = sqrtf(ex * ex + ez * ez);
        static float stop = -1;  // SAVAGEWIND_SORA_ENEMY_STOP: the stop distance, u
        if (stop < 0) stop = env_f("SAVAGEWIND_SORA_ENEMY_STOP", 1.2f);
        if (g_live_stop > 0) stop = g_live_stop;
        dir[0] = ex / ed, dir[1] = ez / ed, speed = ed > stop ? KH_WALK : 0;
    }

    // Climb try (session 28): Link's pad is blocked while Sora drives, so BotW never starts a climb. Pushing into
    // something that stops Sora (under a third of the expected step for CLIMB_PUSH frames on the ground) gives Link the
    // pad for CLIMB_TRY presents: BotW climbs what he walks into (then the puppet above draws Sora). No climb by then:
    // blocked again, CLIMB_COOL presents before the next try.
#define CLIMB_PUSH 12
#define CLIMB_COOL 90
#define GLIDE_PUSH 6  // (the glide is fast: a wall stops it at once)
    {
        static float last[2];
        static int push;
        static uint64_t cool_until;
        float mv = hypotf(p[0] - last[0], p[2] - last[1]), want = speed * kh * to_u;
        last[0] = p[0], last[1] = p[2];
        // (s110, user: "every time I get on the skateboard and move around it pushes me off") riding: the board's slow start
        // under a pushed stick looked like a stopped push into a wall -> this climb try pulsed X = the board's Triangle = get off
        bool stopped = g_climb && !SKT.riding && st == LINK_GROUND && !S.special && !S.air_frames && speed > 0 && mv < want / 3 &&
                       S.act != ACT_GLIDE && S.act != ACT_GLIDE_HOVER;  // (the servo jump keeps the ground word)
        // (s117, user: "glide, then fall -> an invisible paraglider opens by itself": log = after the glide's B let go the fall
        // still read BotW's ground word and Sora barely moved sideways -> this try gave Link the pad with CLIMB_X's X = BotW's
        // paraglider in the air. Now never while Sora's own ACTION is an air one; live "set climbair 1" = the old rule)
        if (stopped && !g_climb_air_ok && ACT_AIR(S.act)) {
            static uint64_t said;
            if (!said || present - said > 600) said = present, LOG("[sora] climb try skipped: Sora is in the air (no X = no BotW paraglider)\n");
            stopped = false;
        }
        push = stopped ? push + 1 : 0;
        // the glide (s50): his last step (S.vel, cm/KH frame: still last frame's here) against what the body did
        static int gpush;
        float gwant = hypotf(S.vel[0], S.vel[1]) * kh * to_u;
        bool gstopped = g_climb && g_gclimb && S.act == ACT_GLIDE && !S.special && (st == LINK_AIR || st == LINK_GROUND) &&
                        gwant > 0.02f && mv < gwant / 3;
        gpush = gstopped ? gpush + 1 : 0;
        // (s88 b3-climb A) a wall ray along the glide (chest height): BotW grabs a wall it is pushed into only when it is
        // head-on enough, and Sora's stop test above needs 6 stopped frames and misses a slide along the face. Two frames
        // of a near, upright, facing wall = the hand-over, with the stick sent into the wall (pad_tap).
        static int wray_n;
        static float wray_dir[2], wray_d;
        bool wray = false;
        if (g_climb && g_gclimb && g_glideclimb >= 1 && S.act == ACT_GLIDE && !S.special && fm_glide_ok() && st == LINK_AIR && present >= g_climb_until) {
            float vl = hypotf(S.vel[0], S.vel[1]), dx = vl > 0.01f ? S.vel[0] / vl : sinf(S.face), dz = vl > 0.01f ? S.vel[1] / vl : cosf(S.face);
            float a[3] = {p[0], p[1] + 0.9f, p[2]}, b[3] = {a[0] + dx * 1.1f, a[1], a[2] + dz * 1.1f}, h[3], n[3];
            if (linkpos_raycast(a, b, h, n) && fabsf(n[1]) < 0.5f && n[0] * dx + n[2] * dz < -0.5f) {
                wray = true;
                wray_d = hypotf(h[0] - a[0], h[2] - a[2]);
                float nl = hypotf(n[0], n[2]);
                wray_dir[0] = -n[0] / nl, wray_dir[1] = -n[2] / nl;
            }
        }
        wray_n = wray ? wray_n + 1 : 0;
        if (present >= g_climb_until) g_climb_air = g_climb_nox = 0, g_gcl_dir[0] = g_gcl_dir[1] = 0;
        if (present < g_climb_until) {
            // (s85f #4 user: "i still can move around while picking up an item": the state word stays ground 0x18000001
            // through BotW's pick up, so the no-wall try holds Link's pad until BotW lets him walk again - his body
            // moves under the stick - or another button, or NOX_CAP presents)
#define NOX_CAP 150
            static uint64_t nox_at;
            if (g_climb_nox && present + CLIMB_TRY - 1 == g_climb_until) nox_at = present;  // (the frame after the try began)
            // (s85f #6d: user retest of 6c: "Link walks" after 7-13 presents = his slide-out of the servo's speed, not
            // BotW's walk; now 6 moving presents in a row from present 15, and the moves logged (mm / present) per window)
            static int mov_n, ml;
            static char mlog[400];
            if (g_climb_nox && present == nox_at) mov_n = 0, ml = 0, mlog[0] = 0;
            if (g_climb_nox && ml < (int)sizeof mlog - 8) ml += snprintf(mlog + ml, sizeof mlog - ml, " %d", (int)(mv * 1000));
            mov_n = mv > 0.03f ? mov_n + 1 : 0;
            // (s85f #6e: the moves 6d logged were the servo's own push - the driver kept sending Sora's stick speed in
            // the window. Sora's drive is now off in the window (below), so it ends after g_nox_len presents (live
            // "set noxlen") or on another button)
            // (s85g user retest of 6e: "sora's model freezes while i can slide him around": with the drive off, BotW
            // walks Link ~180 mm/present 0-12 presents after the press (log), the frozen Sora slid along. Now the
            // moves ARE BotW's: the window ends at Link's 2nd moving present in a row = BotW lets him walk again)
            // (s85g probe, user: "when the menu pops up showing me the new item ... i can move sora around": BotW's
            // new-item popup = screen 0xb, made at the pick up (t3), gone at the press that closes it; no pause.
            // While it is up the window holds - Sora stands, Link has the pad for the closing press. Live "set itemscr 0")
            static bool held_log;
            if (g_climb_nox && g_item_screen && g_itemscr) {
                if (!held_log) held_log = true, LOG("[sora] pick-up window: item popup (screen 0xb) up, held\n");
                if (present + 2 >= g_climb_until) g_climb_until = present + 2;
                mov_n = 0;
            } else if (g_climb_nox && present > nox_at + 15) {  // (s85g: 6d's minimum back: Sora drives again in it)
                if (held_log) held_log = false, LOG("[sora] pick-up window: item popup closed\n");
                bool moved = mov_n >= 2, len = present >= nox_at + (uint64_t)g_nox_len, walk = moved || len,
                     other = press & ~(uint64_t)BTN_CLIMB;
                if (walk || other) {
                    if (present + 1 < g_climb_until)
                        LOG("[sora] pick-up window: %s after %llu presents\n", moved ? "Link walks" : len ? "its length" : "a button", (unsigned long long)(present - nox_at));
                    g_climb_until = present + 1;
                } else if (present + 2 >= g_climb_until && present < nox_at + NOX_CAP) g_climb_until = present + 2;
            }
            if (g_climb_nox && present + 1 == g_climb_until) LOG("[sora] pick-up window moves (mm):%s\n", mlog);
            if (present + 1 == g_climb_until) cool_until = present + CLIMB_COOL, LOG("[sora] climb try: no climb\n");
        } else if (g_climb && !SKT.riding && !SKT.reach && (press & BTN_CLIMB) && st == LINK_GROUND && S.act == ACT_IDLE && !S.special) {  // (s110: Triangle at / on the board is the board's)
            // (ACT_IDLE: an X in the air - his ground word stays through a servo jump - gave Link the pad with B held,
            // BotW opened its paraglider: glide32_kh2a)
            // (the player's own X at BotW's "X Climb" prompt, user session 29: pushing at an angle slides Sora along
            // the wall, so the stopped-push try above never came)
            g_climb_until = present + CLIMB_TRY, push = 0, g_climb_nox = !x_wall(p, S.face);
            if (g_climb_nox) probe_start(present);
            LOG("[sora] climb try (X pressed%s) at %.3f %.3f %.3f: Link gets the pad\n", g_climb_nox ? ", no wall: pick up, no jump" : "", p[0], p[1], p[2]);
        } else if (push >= CLIMB_PUSH && present >= cool_until) {
            g_climb_until = present + CLIMB_TRY, push = 0;
            LOG("[sora] climb try at %.3f %.3f %.3f (moved %.3f of %.3f u): Link gets the pad\n", p[0], p[1], p[2], mv, want);
        } else if ((gpush >= GLIDE_PUSH || wray_n >= 2) && present >= cool_until) {
            bool by_ray = wray_n >= 2;
            g_climb_until = present + CLIMB_TRY, g_climb_air = 1, gpush = 0, wray_n = 0;
            g_gcl_dir[0] = by_ray ? wray_dir[0] : 0, g_gcl_dir[1] = by_ray ? wray_dir[1] : 0;
            g_gcl_watch_until = g_glideclimb >= 2 && by_ray ? present + CLIMB_TRY + 60 : 0;
            LOG("[sora] glide climb try at %.3f %.3f %.3f (%s; moved %.3f of %.3f u): Link gets the pad%s\n", p[0], p[1], p[2],
                by_ray ? "wall ray" : "stopped", mv, gwant, by_ray ? ", stick into the wall" : "");
            if (by_ray) LOG("[sora] glide->climb hand-over: wall %.2f u ahead, into-wall dir %.2f %.2f, X pulses after %d presents (glideclimb %d)\n", wray_d, wray_dir[0], wray_dir[1], GCL_X_AFTER, g_glideclimb);
        }
    }

    // 0. Link was hit (his life dropped): KH2's reaction motion, BotW's own reaction moves him (the servo lets go
    //    while it plays). A ragdoll (BotW's big hit) is KH2's blow: BLOW_* until he is up again, then its landing.
    static int hits_seen = -1;
    int link_hits = linkpos_link_hits(), rag = linkpos_link_ragdoll();
    if (hits_seen < 0 || fresh) hits_seen = link_hits;  // (hits while Link had the pad, e.g. the fatal fall: not Sora's)
    bool guarded = guard_frame(p, to_u, kh, &hits_seen, link_hits);  // (s88: a refunded hit is no flinch)
    // (s119) Mickey mode: the exe guard drops Link's hits (no life lost, no BotW reaction) and mk_tick charges them to
    // Mickey's HP, so link_hits never moves: a new guard block = his hit. Not during the Healing Light / Hi-Potion cast.
    static int mk_blocks_seen = -1;
    int mk_blocks = g_mkflinch && IN_MICKEY && MK.active && MK.guard_blocks ? MK.guard_blocks() : -1;
    bool mk_hit = mk_blocks >= 0 && mk_blocks_seen >= 0 && mk_blocks != mk_blocks_seen && !fresh && MK.phase != 2;
    mk_blocks_seen = mk_blocks;
    if ((link_hits != hits_seen && !guarded) || mk_hit) {
        hits_seen = link_hits;
        float e[2];
        bool front = !nearest_enemy(p, e) || (e[0] - p[0]) * sinf(S.face) + (e[1] - p[2]) * cosf(S.face) >= 0;
        int m = rag > 0 ? (front ? MOT_BLOW_FRONT : MOT_BLOW_BACK)
              : ACT_AIR(S.act) && (S.jh > 0 || S.act != ACT_JUMP) ? (front ? MOT_AIR_FLINCH_FRONT : MOT_AIR_FLINCH_BACK)
              : (front ? MOT_FLINCH_FRONT : MOT_FLINCH_BACK);
        if (mot_of(m)) {
            play(m), S.special = SP_DAMAGE, S.act = ACT_IDLE, S.takeoff = 0, S.req_set = false, S.air_frames = 0;
            S.d74 = S.d75 = 0, gl_from_motion = -1, g_gl_tail = false;  // (a hit while gliding: KH2 leaves 17/18,
                                                                         //  f120 &= ~0x70000)
            LOG("[sora] hit: motion %d (%s, ragdoll %d, state word %#x) at %.3f %.3f %.3f\n", m, front ? "front" : "back",
                rag, linkpos_link_state_word(), p[0], p[1], p[2]);
        }
    }
    if (S.special == SP_DAMAGE && (S.motion == MOT_BLOW_FRONT || S.motion == MOT_BLOW_BACK)) {
        if (rag <= 0 && g_rag_was && g_rag_on && !g_rag_getup) {  // (Sora copied Link's stand-up: once up, no landing)
            if (st == LINK_GROUND) S.special = SP_NONE, S.motion = -1, LOG("[sora] up with Link: idle\n");
        } else if (rag <= 0 && (st == LINK_GROUND || (g_rag_was && g_rag_on)))  // (retargeted: the get-up starts now)
            play(S.motion + 1), LOG("[sora] blow landing %d\n", S.motion);
    } else if (rag > 0 && mot_of(MOT_BLOW_FRONT)) {  // (a ragdoll without a hit, or one frame after it: rag_s21a)
        int m = S.special == SP_DAMAGE && (S.motion == MOT_FLINCH_BACK || S.motion == MOT_AIR_FLINCH_BACK)
                    ? MOT_BLOW_BACK : MOT_BLOW_FRONT;
        play(m), S.special = SP_DAMAGE, S.act = ACT_IDLE, S.takeoff = 0, S.req_set = false;
        LOG("[sora] ragdoll %d: blow %d\n", rag, m);
    }

    // 1. last step's collision results (KH2 resolves them inside its physics, we a frame later)
    bool updraft = ride_updraft();  // (s88 review: an updraft's rise is not a stopped sink)
    if ((S.act == ACT_GLIDE || S.act == ACT_GLIDE_HOVER) && S.req < -0.05f && !updraft) {  // the glide's sink was stopped: landed
        float got = z_up - S.prev_z;  // (no ray cast: under a far floor this is how a glide finds the ground)
        // (s54: with the floor ray the glide mostly ends by GlideEndHeight first; a vetoed frame does not count)
        if (got > S.req * LAND_GOT && !land_veto(z_up, p) && ++g_glide_stuck >= GLIDE_LAND_FRAMES) {
            LOG("[sora] glide land at %.3f %.3f %.3f after %.1f KH frames\n", p[0], p[1], p[2], S.air);
            S.act = ACT_IDLE, S.req_set = false, S.ground_z = z_up, S.ground_move = 0, g_glide_stuck = 0;
            g_gl_tail = gl_from_motion >= 0;  // (the Land blends in from the glide pose too)
            set_motion(MOT_LAND);
        } else if (!(got > S.req * LAND_GOT)) g_glide_stuck = 0, FR.veto = 0;
    }
    if (S.act == ACT_JUMP && S.req_set) {
        float got = z_up - S.prev_z, ray_ground = z_up;
        S.ceiling = false;  // J 4.5: +0x120 bit 22 is re-tested every collision step (kh2ctl.py:1592)
        if (S.req > 0 && got / S.req < KH_CEILING_STOP) S.ceiling = true;
        bool stopped = !updraft && S.req < -LAND_MIN_REQ && got > S.req * LAND_GOT;  // landed: the fall was stopped
        if (!stopped) FR.veto = 0;
        else if (land_veto(z_up, p)) stopped = false;  // (s54: no floor under him - BotW holds him on a wall / edge)
        bool by_ray = !stopped && land_by_ray(present, z_up, &ray_ground);  // (s54: the step he reaches the floor)
        if (stopped || by_ray) {
            LOG("[sora] land at %.3f %.3f %.3f after %.1f KH frames, peak +%.1f cm (%s)\n", p[0], p[1], p[2], S.air,
                S.peak - S.base0, by_ray ? "floor ray" : "stopped");
            if (by_ray) LOG("[sora] land by floor ray: %.1f cm above the floor, request %.1f cm\n", z_up - ray_ground, S.req);
            S.act = ACT_IDLE, S.req_set = false, S.ground_z = ray_ground, S.ground_move = 0, FR.veto = 0;
            // re:0x3B9090 -> ACTION slot 6 re:0x3CB940: Land at once unless the motion keeps it (none of A330-A333):
            // the air swing is cut, its windows stop (PHH kh2ctl.py:1566-1582; S32 helper K M4)
            // (s68: an air cast too, once its spell is out - before that the cast plays on, so the spell isn't lost)
            bool air_sp = S.special == SP_ATTACK || (S.special == SP_MAGIC && C.fired);
            // (s110 mkhit) Mickey's attacks are hops (every ground ptya entry has Jump > 0) and his swing is longer than the
            // hop (A335: 76 frames, hop 16): the landing must not cut it, or its combo window never opens (no chain)
            bool mk_keep = IN_MICKEY && g_mkhit && S.special == SP_ATTACK && S.mot;
            if (mk_keep) LOG("[mkhit] landed in swing %d at clock %.1f: the swing plays on (no land cut)\n", S.motion, S.clock);
            else if (air_sp && !(mot_bits(S.mot, S.clock) & RB_KEEP_MOTION))
                LOG("[sora] land cuts air %s %d at clock %.1f\n", S.special == SP_MAGIC ? "cast" : "swing", S.motion,
                    S.clock), S.special = SP_NONE, S.motion = -1;
            if (!mk_keep) set_motion(MOT_LAND);
        }
    }
    float bx = p[0] * 100 / g_k, bz = p[2] * 100 / g_k, shove = sqrtf((bx - S.body_xz[0]) * (bx - S.body_xz[0]) +
                                                                      (bz - S.body_xz[1]) * (bz - S.body_xz[1]));
    S.body_xz[0] = bx, S.body_xz[1] = bz;
    wall_tick(p, shove, S.act == ACT_IDLE && st == LINK_GROUND && S.special != SP_DAMAGE && S.takeoff <= 0 && !S.air_frames);
    floor_probe(p, S.act == ACT_IDLE && st == LINK_GROUND && !S.air_frames);
    if (shove < S.ground_move) shove = S.ground_move;  // (a hit's push down a slope is not a walk-off: hurt_s19b)
    bool dropping = S.act == ACT_IDLE && S.ground_z - z_up > WALKOFF_DROP + 1.2f * shove;
    // re:0x3B9090 skips the walk-off while +0x18C bit 23 (range 29 cant_leave_ground) is set: KH2's finishers A303 /
    // A304 (frames 0-38 / 0-39) keep their floor; A300-A302 and the roll have no range 29 and walk off (KH2 too).
    // Here the floor ahead is unknown (no BotW ray cast yet), so the body is put back where it last stood and the
    // swing's root move stops until the next swing (KH2 would slide along the edge; root left after frame 39 too)
    bool hold = S.special == SP_ATTACK && S.mot && (mot_bits(S.mot, S.clock) & RB_NO_FALL_OFF);
    if (S.special == SP_DAMAGE) S.air_frames = 0;  // (BotW's knockback, not a walk-off)
    else if (S.act == ACT_IDLE && dropping && hold && S.nsafe) {
        const float *q = S.safe[0];
        linkpos_move(q[0] - p[0], q[1] - p[1], q[2] - p[2]);
        S.edge_swing = S.swing, S.air_frames = 0, S.nsafe = 0;
        S.body_xz[0] = q[0] * 100 / g_k, S.body_xz[1] = q[2] * 100 / g_k;
        LOG("[sora] ledge hold (motion %d clock %.1f): back %.3f u to %.3f %.3f %.3f\n", S.motion, S.clock,
            sqrtf((q[0] - p[0]) * (q[0] - p[0]) + (q[2] - p[2]) * (q[2] - p[2])), q[0], q[1], q[2]);
        z_up = q[1] * 100 / g_k;  // (this step's ground_z: the height he is put back to)
    }
    // (only the height: with the servo driving, a real fall keeps the ground word 0x18000002 and the air word
    // 0x8xxxxxxx is BotW's hit reaction, a frame before his life drops - it started a Fall and the air flinch: hurt_s19a)
    else if (S.act == ACT_IDLE && S.takeoff <= 0 && dropping) {
        float e[2], fy;
        bool guard = g_wo_guard >= 2 || (g_wo_guard == 1 && (S.special == SP_ATTACK || nearest_enemy(p, e)));
        if (guard && walkoff_floor(p, &fy)) {  // (s60b: a floor within g_wo_floor_cm - no walk-off, stays grounded)
            static uint64_t logged;
            float below = (p[1] - fy) * 100 / g_k;
            if (present - logged > 30)
                LOG("[sora] walk-off held (motion %d, %s): floor ray %.1f cm under the feet at %.3f %.3f %.3f%s\n",
                    S.motion, S.special == SP_ATTACK ? "swing" : "enemy near", below, p[0], p[1], p[2],
                    g_wo_snap && below > 2 ? " (snap)" : "");
            logged = present;
            S.air_frames = 0;
            if (g_wo_snap && below > 2) {
                // (s65 audit #7) at most FallMax per KH frame, as KH2's grounded swing follows the floor by gravity:
                // the one-present pop of up to 44 cm mid swing is gone. Live "set wocap 0" = the whole way at once.
                float dy = fy + 0.01f - p[1], cap = KH_FALL_MAX * kh * g_k / 100;
                if (g_wo_cap && dy < -cap) dy = -cap;
                linkpos_move(0, dy, 0), z_up = (p[1] + dy) * 100 / g_k;
            }
        } else if (++S.air_frames >= 2) {  // walk-off: the last move carries on (at most 8 cm/frame), Fall from here
            float v = sqrtf(S.vel[0] * S.vel[0] + S.vel[1] * S.vel[1]);
            if (v > KH_WALKOFF_MAX) S.vel[0] *= KH_WALKOFF_MAX / v, S.vel[1] *= KH_WALKOFF_MAX / v;
            fall_start(0, z_up);
            S.special = SP_NONE;
            fall_motion(16);
            LOG("[sora] walk-off (%s) at %.3f %.3f %.3f\n", st == LINK_AIR ? "air state" : "dropping", p[0], p[1], p[2]);
        }
    } else S.air_frames = 0;

    // 2. the motion clock: an attack's / the roll's root move over (old, new], motion ends and loops; the jump
    //    motion's frame trigger starts ACTION_JUMP
    float clock_old = S.clock, root[3] = {0, 0, 0};
    float prev_clock = S.clock;
    S.clock += kh;
    sora_fx_tick(kh);  // (every effect element advances by the frame's KH frames, before the draw)
    if (S.mot && LF_SE_OK(S.mot)) sora_sfx_motion(sfx_slot(S.mot), prev_clock, S.clock, p);  // (KH2 mset triggers: whooshes, jump, land, steps)
    meal_item_tick(kh);  // (s98 potion: KH2's item task - the use_item frame, the user SE, the heal 30 frames later)
    if (S.special && S.mot && S.mot->root && S.edge_swing != S.swing) {
        float a[3], b[3];
        mot_root(S.mot, clock_old, a), mot_root(S.mot, S.clock, b);
        for (int c = 0; c < 3; c++) root[c] = b[c] - a[c];
    }
    lm_chain();  // (s89 Limit Form: a Limit command's step that ended -> its next step, before the end check below)
    pl_chain(p, kh, to_u);  // (s133 limits: the party Limit's clocks, its step end / next step)
    mk_dchg_loop();  // (s110m review: Mickey's held D-Charge loops 60..76 here, before the end check, at any frame rate)
    if (S.mot && S.clock >= S.mot->frames) {
        if (S.special == SP_DAMAGE && (S.motion == MOT_BLOW_FRONT || S.motion == MOT_BLOW_BACK) && S.mot->frames > S.mot->loop)
            S.clock = S.mot->loop + fmodf(S.clock - S.mot->frames, (float)(S.mot->frames - S.mot->loop));  // (until up)
        else if (S.special) {  // ended: its NextMotionId (0 = idle) - the ACTION tick picks idle / walk / run
            if (S.special == SP_ATTACK) LOG("[sora] attack %d ended at %.3f %.3f %.3f\n", S.motion, p[0], p[1], p[2]);
            S.special = SP_NONE, S.motion = -1;
            // re:0x3C8310 / re:0x3C6740: the air entries queue NextMotionId 4 = Fall; the arc goes on unchanged
            if (ACT_AIR(S.act)) set_motion(MOT_FALL);
        } else if ((S.mot->flags & 1) && S.mot->frames > S.mot->loop)
            S.clock = S.mot->loop + fmodf(S.clock - S.mot->frames, (float)(S.mot->frames - S.mot->loop));
        else
            S.clock = S.mot->frames;  // (Land holds its last frame until the ACTION tick changes it)
    }
    // (S32 glide port, G: the glide motions loop, as glide_draw's fmodf did)
    if ((S.act == ACT_GLIDE || S.act == ACT_GLIDE_HOVER) && S.mot && S.mot->frames > 0)
        S.clock = fmodf(S.clock, (float)S.mot->frames);
    uint32_t bits = S.special ? mot_bits(S.mot, S.clock) : RB_FREE;
    bool is_free = !S.special || (bits & RB_FREE);
    // (s89 review) a running Limit owns him to its last step's end: a free range of a K4xx step + the stick / B / roll
    // ended the step (blend_to_idle below) = "interrupted" before the follow-up could chain. Live "set lmhold 0" = old.
    if (S.special == SP_LIMIT && LM.on && g_lm_hold) is_free = false;
    if (pl_hold()) is_free = false;  // (s133 limits: Whirli-Goof owns Sora to its last step's end)
    if (S.special == SP_DAMAGE && !is_free) press = 0, speed = 0;  // (a reaction owns him until its free frames)
    // (s85f #6e froze Sora for the whole window; s85g user: "once i press triangle without an item in front of me sora
    // freezes for less than a second" - moving through a pick up is fine, only the new-item popup (screen 0xb) freezes him)
    bool nox_win = g_climb_nox && present < g_climb_until && g_item_screen && g_itemscr;
    if (nox_win || g_frz_id >= 0 || g_chev) press = 0, speed = 0;  // (s86: or a freeze screen, g_frz_mask)
    fm_tick(kh);  // (s68 drive: CHANGEFORM -> swap, the form gauge)
    rox_tick();   // (s109 roxas: Roxas until IsGet_PlayerStole2, then Sora + Donald + Goofy)
    ofx_tick();   // (s146 outfit: the Auto outfit, then the body on screen follows the wanted outfit)
    kb_send();    // (s85 slice 5)
    kbfx_tick();  // (s89 kbfx: logs the active sets on a change)
    if (S.special == SP_DRIVE) press = 0, speed = 0, cast = -1;  // (CHANGEFORM / the entry own him: KH2 no_hit, pad dead)
    if (sm_hold()) press = 0, speed = 0, cast = -1;  // (s136 summon: Sora plays the call; the pad is dead)
    if (S.special && is_free && (speed > 0 || (press & (BTN_JUMP | BTN_ROLL))))  // blend_to_idle: locomotion takes over
        S.special = SP_NONE;
    float sdir[2] = {dir[0], dir[1]}, sspeed = speed;  // the stick's intent (a press turns Sora toward it)
    if (SKT.riding) {  // (s110 skate: the board steers + pushes; locomotion sees no stick = the idle pose)
        skt_ride(sdir, sspeed, kh, to_u, p);
        if (SKT.riding) speed = 0;
    }
    if (is_free) S.wish = sspeed > 0 ? atan2f(sdir[0], sdir[1]) : S.face;  // (TSR 7.4: kept through the swing)
    Target tg[64];  // the enemies as KH2 targets; the lock-on block ticks in the player's pre-tick (re:0x3DD280)
    int ntg = targets_build(p, to_u, tg, 64);
    lock_tick(tg, ntg, kh, bits, S.face, r1 && g_targets_on && !sm_fps_busy());  // (s137 review S2: no R1 lock in FPS Mode, its R1 cycles targets)
    if (!sm_fps_busy()) lockcam_tick(tg, ntg, p, to_u, held);
    {  // the KH2 icon on the block's target (yellow AUTO, blue-white R1 LOCK): the enemy's origin (its centre minus half the hit height) + 1 u
        const Target *lk = g_targets_on && g_lock.actor ? target_of(tg, ntg, g_lock.actor) : NULL;
        // (s65) no aim target within 300: the icon alone may still show one out to g_icon_range (yellow, not aimed at)
        if (!lk && g_targets_on && g_icon_range > g_auto_range) lk = target_auto(tg, ntg, S.face, g_icon_range);
        float lc[3] = {0, 0, 0};
        if (lk) lc[0] = p[0] + lk->d[0] * to_u, lc[1] = p[1] - lk->dy * to_u - g_hit_h * 0.5f + 1.0f, lc[2] = p[2] + lk->d[1] * to_u;
        sora_hud_target(!lk ? 0 : g_lock.locked ? 2 : 1, lc);
        // L5: the locked enemy's KH2 gauge; its max life = the highest seen per actor (BotW's max isn't read yet)
        static struct { uint64_t actor; int32_t max; } seen[32];
        static int seen_next;
        int32_t lmax = 0;
        if (lk && g_lock.locked) {
            int i = 0;
            while (i < 32 && seen[i].actor != lk->actor) i++;
            if (i == 32) i = seen_next, seen_next = (seen_next + 1) % 32, seen[i].actor = lk->actor, seen[i].max = 0;
            if (lk->life > seen[i].max) seen[i].max = lk->life;
            lmax = seen[i].max;
        }
        sora_hud_enemy(lmax > 0 ? (float)lk->life : 0, (float)lmax);
    }
    // (s73) Wisdom moves while it shoots (KH2): the stick keeps steering his momentum through SP_ATTACK
    if (S.special && !(bits & RB_ALLOW_MOVEMENT) && !(IN_WISDOM && g_wis_mom && S.special == SP_ATTACK)) speed = 0;
    if (S.takeoff > 0 && (S.takeoff -= kh) <= 0) {
        enter_jump(z_up, true);  // ("held" was decided at the press, re:0x3CBD30: no pad sample here)
    }
    // a release is latched on any frame (also during the take-off), the hop fires at the first frame in its window
    // (kh2ctl.py:2027; S32 helper J 4.1: a tap used to give the full jump, KH2's 1-frame tap = a 115 cm hop)
    if (S.jump_held && !(held & BTN_JUMP)) S.jump_held = false, S.jump_released = true;

    // (s65 audit #7 of section 2) blade_hits runs after the presses: a press on the present the swing connected was
    // judged with S.hit still false and dropped. That press comes back here, once, if the hit did land.
    if (S.press_late) {
        if (S.hit && S.special == SP_ATTACK && !(press & BTN_ATTACK))
            press |= BTN_ATTACK, LOG("[sora] attack press re-run: the hit landed on its present\n");
        S.press_late = false;
    }
    lm_tick_press(&press, tg, ntg);  // (s89 Limit Form: start a Limit command; while one runs A = its follow-up)
    press |= pl_x;  // (s133 review: the prompt's Triangle, held back from the other Triangle uses above)
    pl_tick_press(&press, tg, ntg, p, to_u, kh);  // (s133 limits: start a party Limit; the Triangle prompt, the effects, the hits)
    // 3. presses (KH2's order: jump first)
    if ((press & BTN_JUMP) && S.act == ACT_IDLE && S.takeoff <= 0 && !S.air_frames && is_free) {
        arc(SKT.riding ? g_skt_jump : F_JUMP_H);  // (s110 skate: the ollie's own height)
        S.hvel[0] = dir[0] * speed, S.hvel[1] = dir[1] * speed;
        S.takeoff = g_jump_trigger;
        S.jump_held = true, S.jump_released = false;  // re:0x422630: +0x120 bit 16; re:0x3CBD30 makes it bit 17
        set_motion(MOT_JUMP);
        LOG("[sora] jump at %.3f %.3f %.3f (speed %.0f)\n", p[0], p[1], p[2], speed);
    }
    // glide (KH2 re:0x3A81F0, the user's button: B again in the air, held). KH2 starts at the press; here the press
    // arms it and the glide starts the frame he is past the apex (the user's feel from session 31). Held B keeps it
    // (+0x124 bit 2), the release ends it (glide_end in the tick). (S32 port: no paraglider hand-off any more.)
    if ((press & BTN_JUMP) && S.act == ACT_JUMP && g_glide_on && !S.special && fm_glide_ok() && !bd_press()) S.glide_armed = true;  // (s89: Final glides; s91: base Sora judges tap / hold first)
    bd_tick(held, kh, sdir, sspeed, z_up, p);  // (s91 b5-dodge)
    if (!(held & BTN_JUMP) || S.act != ACT_JUMP) S.glide_armed = false;
    if (S.glide_armed && S.air >= S.apex) glide_start(p, z_up, sspeed);
    ma_air_dodge(press, sdir, sspeed, z_up, p);  // (s89 b4-master: B again in the air in Master = KH2's Aerial Dodge)
    bool guard_took = guard_press(press, sspeed, is_free, p);  // (s88: Square with the stick neutral = Guard)
    if (!guard_took && (press & BTN_ROLL) && S.act == ACT_IDLE && S.takeoff <= 0 && !S.air_frames && is_free && mot_of(MOT_ROLL) &&
        (!fm_in_form() || IN_LIMIT) && roll_learned() && stam_pay("Dodge Roll", g_rollstam)) {  // (s69: Valor has no Dodge Roll; its bake has no roll slot. s89: Limit's own A180, growth Dodge Roll. s97: 1/8 wheel)
        if (sspeed > 0) S.face = atan2f(sdir[0], sdir[1]);  // the roll goes where the stick points
        play(MOT_ROLL), S.special = SP_ROLL;
        LOG("[sora] roll at %.3f %.3f %.3f face %.1f\n", p[0], p[1], p[2], S.face * 180 / PI);
    }
    // the air: re:0x3BA9A0 airborne = ACTION flag bit 0 (JUMP, GLIDE_HOVER, GLIDE), base Sora has no no-gravity
    // motions: each air swing is its own small jump start (re:0x3D78D0 -> re:0x3CBB00), which also leaves a glide
    // the cast (L 4.5 + the video: inside the combo). Like an attack press: combo_gate decides, and at the combo's
    // finisher step the FinishMotion plays. (s68) In the air (jump / fall / glide) the magc AirMotion plays (A370-A372,
    // slots 232 / 244 / 256 / 268), its state_no_gravity range holds him in place (the ACTION tick below), no finisher
    // variant in the air (GUESS: magc has one FinishMotion, a ground one). Live "set airmagic 0" = ground only (s67).
    bool air_cast = g_air_magic && ACT_AIR(S.act);
    if (cast >= 0 && (S.act == ACT_IDLE || air_cast) && S.takeoff <= 0 && !S.air_frames) {
        const KhSpell *sp = &KH_SPELLS[cast];
        int d74 = S.d74, d75 = S.d75;
        bool in_combo = S.special == SP_ATTACK || S.special == SP_MAGIC;
        if ((S.special && !in_combo) || !combo_gate(bits, is_free)) {
            S.d74 = d74, S.d75 = d75;
            LOG("[sora] cast %s dropped (gate, motion %d clock %.1f bits %#x)\n", sp->name, S.motion, S.clock, bits);
        } else {
            bool fin = !air_cast && in_combo && sp->finish >= 0 && finisher_ok(bits, 0, false) && mot_of(sp->finish) && !berserk_on();  // (review: Berserk = no finishers)
            int motion = air_cast ? sp->air_motion : fin ? sp->finish : sp->motion;
            if (air_cast && mot_of(motion) && S.act != ACT_JUMP) {  // (from a glide: the glide ends as an air swing's does)
                fall_start(0, z_up);
                gl_from_motion = -1, g_gl_tail = false, g_gblend = -1;
            }
            if (!mot_of(motion)) {
                S.d74 = d74, S.d75 = d75;
                LOG("[sora] cast %s: motion %d not in the bake\n", sp->name, motion);
            } else {
                play(motion), S.special = SP_MAGIC, S.next = 0;
                if (C.fx_cast >= 0 && !C.fired) sora_fx_kill(C.fx_cast);  // (the previous cast cut short)
                static const char *const CAST_FX[NSPELL] = {"fire_cast", "blizzard_cast", "thunder_cast", "cure_cast",
                                                            "magnet_cast", NULL};  // (s133: Magnet = magnet_1.0~34, the glow with its loop-end tail at the cast frame 34; Reflect's script starts none)
                float tp[3];
                cast_tip(p, to_u, tp);
                C.fx_cast = CAST_FX[cast] ? sora_fx_event(CAST_FX[cast], tp, S.face, to_u) : -1;
                C.spell = cast, C.cast_at = cast_frame(S.mot, fin ? sp->finish : sp->motion), C.fired = false;
                if (fin && C.cast_at <= 0) C.cast_at = cast_frame(S.mot, sp->motion);
                turn_to_lock_once(tg, ntg);
                M.dead = 8, M.sub = -1, M.cursor = 0, M.sub_cursor = 0;  // re:0x3B21D0: executed
                LOG("[sora] cast %s%s (cmd %d, motion %d slot %d, cast frame %.0f, hit %d, MP %.0f)\n", sp->name,
                    fin ? " finisher" : air_cast ? " in the air" : "", sp->cmd, motion, S.mot->slot, C.cast_at, S.d74, MP.mp);
            }
        }
    } else if (cast >= 0) LOG("[sora] cast %s refused (%s)\n", KH_SPELLS[cast].name, ACT_AIR(S.act) ? "air" : "take-off");
    if ((press & BTN_ATTACK) && (S.act == ACT_IDLE || ACT_AIR(S.act)) && S.takeoff <= 0 && !S.air_frames) {
        int d74 = S.d74, d75 = S.d75;
        bool air = ACT_AIR(S.act), new_combo = S.special != SP_ATTACK;
        bool was_glide = S.act == ACT_GLIDE || S.act == ACT_GLIDE_HOVER;
        // PHH kh2ctl.py:2068: one air combo per jump - a second combo START in the air waits for the ground
        // (continuations pass). [KH2's own mechanism UNVERIFIED; the feel is PHH's.] Live "set aironce 0" turns it off.
        // (a restart from a free frame of the air combo - after its finisher - is a new combo too: hop_mash_s34b
        // chained A330 restarts 1-2 cm above the ground forever)
        bool cont = S.hit || g_combo_master || ab_on(AB.on, AB_COMBO_MASTER);  // (s92: the Abilities page row counts too)
        bool restart = !new_combo && !((bits & (RB_ALLOW_FINISHER | RB_ALLOW_COMBO)) && cont);
        bool once = g_air_once && air && (new_combo || restart) && S.air_done;
        bool wall = wall_ahead(p, S.face);
        const KhPtya *e = !once ? t0_reaction() : NULL;  // (s106: Counterguard / Retaliating Slash)
        if (e) new_combo = true, S.d74 = S.d75 = 1, S.hit = false;
        else {
            // (s110 mkhit) Mickey's ground combo is hops: a press in the hop of a ground combo still picks from the GROUND entries
            bool sel_air = air;
            if (IN_MICKEY && g_mkhit) {
                if (new_combo) g_mk_gcombo = !air;
                sel_air = air && !(g_mk_gcombo && !new_combo);
            }
            e = !once && combo_gate(bits, is_free) ? t0_override(ptya_select_t(bits, sel_air, tg, ntg, wall), sel_air, tg, ntg) : NULL;
            if (e && IN_MICKEY && g_mkhit && !sel_air) e = mk_ground_fix(e);
        }
        if (e && g_combolog && (new_combo || e->jump > 0)) combo_plan_log(air, e);  // (s105) once per combo
        if (e && mot_of(e->motion)) {
            // (no snap at the press, TSR 7.4: the stick's turn of the free frames and turn_to_lock aim the swing; with
            // the targets off a first swing still faces the stick, as before)
            if (!g_targets_on && sspeed > 0 && is_free) S.face = atan2f(sdir[0], sdir[1]);
            play(e->motion), S.special = SP_ATTACK, S.next = e->next;
            attack_start_t(e, tg, ntg, z_up, air, bits);  // (an AIR entry: the arc from here, ACT_JUMP)
            if (air && new_combo) S.air_done = true;
            if (was_glide) gl_from_motion = -1, g_gl_tail = false, g_gblend = -1;  // (no glide blend into the swing)
            turn_to_lock_once(tg, ntg);
            const Target *lk = g_lock.actor ? target_of(tg, ntg, g_lock.actor) : NULL;
            LOG("[sora] attack %d (%s%s, ptya id %d type %d, hit %d, finisher %d) at %.3f %.3f %.3f face %.1f, target %llx "
                "%.0f cm\n", e->motion, air ? "air" : "ground", wall ? ", wall ahead" : "", e->id, e->type, S.d74, S.d75,
                p[0], p[1], p[2],
                S.face * 180 / PI, (unsigned long long)g_lock.actor, lk ? lk->hd : -1.0f);
        } else {
            S.d74 = d74, S.d75 = d75;
            S.press_late = g_late_hit && !once && !e && S.special == SP_ATTACK && !cont &&
                           (bits & (RB_ALLOW_FINISHER | RB_ALLOW_COMBO));  // (only "no hit yet" dropped it)
            LOG("[sora] attack press dropped (%s, motion %d clock %.1f bits %#x)\n", once ? "air combo done" : "gate",
                S.motion, S.clock, bits);
        }
    }

    // 4. the ACTION tick
    // (s58) the glide pays BotW's paraglider stamina; out -> it ends as if B were let go (Fall), as Link's glide does
    bool gs_out = glidestam_tick(present);
    // (s97b) user: a roll's 1/8 came straight back (ground regen outran the rolls). While a paid roll plays, BotW's
    // recovery wait is held at its reload (as while sprinting: no regen), so it refills only after BotW's normal delay.
    if (S.special == SP_ROLL && g_rollstam > 0) {
        uint64_t rpi, ra = gs_actor(&rpi);
        if (ra) *(float *)(uintptr_t)(ra + STAM_WAIT) = GV(float, ra, STAM_WAIT + 4);
    }
    if (S.act == ACT_JUMP) {
        if (S.jump_released && KH_HOP_MIN < S.air && S.air < S.apex - KH_APEX_EARLY) {  // kh2ctl.py:1512-1516
            // KH2 ticks every PHYS_STEP 2 frames, so its hop fires at air 8 (110 cm, J 1); from the curve, not the
            // body: z_up lags the request by a step (hop_s33a: +63.8 cm from z_up). The y servo catches up.
            float hop_air = S.air < KH_HOP_MIN + 2 ? KH_HOP_MIN + 2 : S.air, hop_z = arc_at(hop_air);
            LOG("[sora] short hop at air %.1f from +%.1f cm (body +%.1f)\n", S.air, hop_z - S.base, z_up - S.base);
            fall_start(KH_SHORT_HOP, hop_z);  // (clears the latch)
            fall_motion(8);
        } else if (S.apex <= S.air + KH_APEX_EARLY) S.jump_held = S.jump_released = false;  // kh2ctl.py:1511
        if (S.ceiling && KH_HOP_MIN < S.air && S.air < S.apex) fall_start(0, z_up), fall_motion(8);
        if ((S.motion == MOT_JUMP || S.motion == MOT_AIR_DODGE) && S.air >= S.apex - KH_APEX_EARLY) fall_motion(8);  // (s89: + the Aerial Dodge)
    } else if (S.act == ACT_GLIDE_HOVER && sspeed <= 0) {  // re:0x423840
        set_motion(MOT_GLIDE_HOVER);  // (16-frame blend: glide_pose_draw)
        glide_end(p, z_up, (held & BTN_JUMP) != 0 && !gs_out);
    } else if (S.act == ACT_GLIDE_HOVER || S.act == ACT_GLIDE) {  // re:0x423A70: move, banking left / right while the
        S.act = ACT_GLIDE;  // (hover with a stick intent -> glide; PHH runs the glide branch next frame)
        int want = MOT_GLIDE_MOVE;  //  stick is > acos 0.98 off his facing
        if (sspeed > 0) {
            float fx = sinf(S.face), fz = cosf(S.face);
            if (fx * sdir[0] + fz * sdir[1] < GLIDE_BANK_COS)  // left = (fz, -fx): the stick's side of his facing
                want = fz * sdir[0] - fx * sdir[1] > 0 ? MOT_GLIDE_LEFT : MOT_GLIDE_RIGHT;
        }
        set_motion(want);
        glide_end(p, z_up, (held & BTN_JUMP) != 0 && !gs_out);
    } else if (!S.special && S.takeoff <= 0) {
        // J 4.3, re:0x422520: idle / walk / run only in a grounded range, or in blend_to_idle with a stick; A005 has
        // keep_momentum 0-25, blend_to_idle 12-25 and no grounded range, so without a stick Land plays out (26
        // frames) and the idle follows (kh2ctl.py:1605); with one, walk / run from frame 12
        uint32_t lb = S.motion == MOT_LAND && S.mot ? mot_bits(S.mot, S.clock) : RB_GROUNDED;
        bool ended = S.motion == MOT_LAND && S.mot && S.clock >= S.mot->frames;
        if ((lb & RB_GROUNDED) || ((lb & RB_BLEND_TO_IDLE) && speed > 0) || ended) {
            int want = speed == 0 ? MOT_IDLE : speed < (F_RUN + F_WALK) / 2 ? MOT_WALK : MOT_RUN;
            bool cyc = (S.motion == MOT_WALK || S.motion == MOT_RUN) && (want == MOT_WALK || want == MOT_RUN);
            if (g_lphase && cyc && want != S.motion && S.mot && S.mot->frames > 0) {
                // (s66, SDR 3.5) walk <-> run starts the new cycle at the old one's phase, not at frame 0
                bool lp = (S.mot->flags & 1) && S.mot->frames > S.mot->loop;
                float lo = lp ? (float)S.mot->loop : 0, ph = (S.clock - lo) / ((float)S.mot->frames - lo);
                set_motion(want);
                if (S.mot && S.mot->frames > 0) {
                    lp = (S.mot->flags & 1) && S.mot->frames > S.mot->loop, lo = lp ? (float)S.mot->loop : 0;
                    S.clock = lo + (ph < 0 ? 0 : ph > 1 ? 1 : ph) * ((float)S.mot->frames - lo);
                    LOG("[sora] phase: motion %d at %.2f of its cycle\n", want, ph);
                }
            } else set_motion(want);
        }
    }
    if (turn_to_lock(bits, kh, tg, ntg)) S.wish = S.face;  // (owns the facing while its window runs)
    else if (g_swing_turn && S.special == SP_ATTACK && speed <= 0) {
        // (s65 audit #4, TSR 7.4 INFERRED) no target turn: the facing keeps stepping toward the last free frame's
        // stick at TurnSpeed through the swing (press attack while pushing left -> he swings left)
        float step = F_TURN * kh, d = wrap(S.wish - S.face);
        S.face = wrap(S.face + (d > step ? step : d < -step ? -step : d));
    }
    // re:0x423B90: the glide's turn speed while 17/18, prty's otherwise (_leave puts it back)
    if (speed > 0 || (S.act == ACT_GLIDE && sspeed > 0)) {
        float turn = (S.act == ACT_GLIDE || S.act == ACT_GLIDE_HOVER) ? KH_GLIDE_TURN : F_TURN;
        float step = turn * kh, d = wrap(atan2f(sdir[0], sdir[1]) - S.face);
        S.face = wrap(S.face + (d > step ? step : d < -step ? -step : d));
    }

    // 5. the ACTION physics (one step of PHYS_STEP KH frames): horizontal move (cm) and rise
    float move[2], dz = 0;
    if (pl_hold()) {  // (s135 diag: why the Whirli-Goof steer path never ran) once per Limit step
        static int pl_diag_play = -1, pl_diag_sa = -1;
        int sa = (int)S.special * 100 + (int)S.act + (sspeed > 0 ? 10000 : 0);
        if (pl_diag_play == PLM.plays && sa != pl_diag_sa)
            LOG("[plimit] s135 diag: change at step %d tstep %.0f: act %d special %d sspeed %.2f mag %.2f\n", PLM.step + 1, PLM.tstep, (int)S.act, (int)S.special, sspeed, mag);
        pl_diag_sa = sa;
        if (pl_diag_play != PLM.plays)
            pl_diag_play = PLM.plays, LOG("[plimit] s135 diag: step %d act %d special %d sspeed %.2f mag %.2f steering %d tstep %.0f\n",
                                          PLM.step + 1, (int)S.act, (int)S.special, sspeed, mag, (int)pl_steering(), PLM.tstep);
    }
    if (S.act == ACT_IDLE) {
        // J 4.2: A003 frames 0-7 have no free range (air from 8): KH2 zeroes the intent there (re:0x3A89A0), so the
        // ground frames before the jump trigger do not move him; hvel was taken at the press
        float sp = S.takeoff > 0 ? 0 : speed;
        if (((IN_WISDOM && g_wis_mom && (!S.special || S.special == SP_ATTACK)) || (IN_FINAL && g_fin_mom && !S.special)) && S.takeoff <= 0) {
            // (s72) Wisdom's momentum: prty Acceleration 0.85 eases toward the stick's speed, Brake 0.97 decays it with
            // the stick released (GUESS: per KH frame, the KH_AIR_KEEP shape; KH2's formula not traced). Live wmom 0.
            // (s89 b4-final) Final's hover slide: prty Accel 0.94 / Brake 0.99, same GUESS shape, not while attacking
            // (its swings carry root motion). Live "set fmom 0" = instant like base Sora.
            float k = powf(sp > 0 ? (IN_FINAL ? FINAL_ACCEL : WISDOM_ACCEL) : (IN_FINAL ? FINAL_BRAKE : WISDOM_BRAKE), kh);
            S.vel[0] = k * S.vel[0] + (1 - k) * dir[0] * sp, S.vel[1] = k * S.vel[1] + (1 - k) * dir[1] * sp;
        } else
            S.vel[0] = dir[0] * sp, S.vel[1] = dir[1] * sp;  // no acceleration: Sora's speed is instant
        move[0] = S.vel[0] * kh, move[1] = S.vel[1] * kh;
        S.root_cm = 0;
        if (S.special) {  // the root move, entity frame (+z forward, +x left) -> world by the facing; left = (fz, -fx)
            float fx = sinf(S.face), fz = cosf(S.face);
            move[0] += root[2] * fx + root[0] * fz, move[1] += root[2] * fz - root[0] * fx;
            pl_move(move, kh, sdir, sspeed > 0 ? mag : 0);  // (s133 limits: Whirli-Goof's script move toward its target; s134 lpsteer: the stick steers the spin)
            if (pl_steering() && pl_def()->member == 1 && pl_stage()->slot[2]) pl_goofy_pos(pl_stage(), p, to_u);  // (s134 review: Goofy on Sora's new facing this frame)
            S.vel[0] = move[0] / kh, S.vel[1] = move[1] / kh;  // (a walk-off mid-roll carries this)
            S.root_cm = sqrtf(root[0] * root[0] + root[2] * root[2]);
        }
        gl_move(move, kh);  // (s139 genielim) Sora's root travel while he plays Genie's Limit motions (his pad is dead: no stick move)
        nl_move(move);      // (s139 review) Never Land: the flight / Journey's End step as a velocity (walls + ledges stay BotW's), not a teleport
        if (R.push >= 1.0f) {  // the repel push: -facing, 5 cm/frame x0.9 (re:0x3D4960), ~15 frames / ~35 cm
            // (move[] = cm this step = vel * kh KH frames, like the root; TODO verify on a live wall)
            move[0] -= sinf(S.face) * R.push * kh, move[1] -= cosf(S.face) * R.push * kh;
            S.vel[0] = move[0] / kh, S.vel[1] = move[1] / kh;
            R.push *= powf(0.9f, kh);  // (x0.9 per KH frame at any frame pace)
            if (R.push < 1.0f) R.push = 0, LOG("[sora] repel push done\n");
        }
        S.req_set = false;
        S.ground_z = z_up, S.ground_move = sqrtf(move[0] * move[0] + move[1] * move[1]);
        W.cmd[0] = move[0], W.cmd[1] = move[1];  // (wall_tick judges it against the body's move next frame)
        S.air_done = false;  // (PHH: cleared on every grounded tick)
        S.ng = false;
        if (!dropping && !S.air_frames && S.special != SP_DAMAGE) {  // (where a ledge hold puts him back)
            if (S.nsafe == 3) memmove(S.safe[0], S.safe[1], sizeof S.safe[0] * 2), S.nsafe = 2;
            S.safe[S.nsafe][0] = p[0], S.safe[S.nsafe][1] = p[1], S.safe[S.nsafe][2] = p[2], S.nsafe++;
        }
    } else if (S.act == ACT_GLIDE_HOVER || S.act == ACT_GLIDE) {  // re:0x423650 / 0x423920 / 0x4237A0
        S.air += kh;
        float h = height_above_floor(p, z_up), d = h - KH_GLIDE_FALL_HEIGHT;
        float sink = d > 0 ? d * KH_GLIDE_FALL_RATIO : 0;  // cm/frame
        if (sink > KH_GLIDE_FALL_MAX) sink = KH_GLIDE_FALL_MAX;
        if (S.act == ACT_GLIDE_HOVER) {  // the hover brakes what he came in with, 0.9 per physics step
            float brake = powf(KH_GLIDE_HOVER_BRAKE, ratio);  // (once per PHYS_STEP KH frames, like KH_AIR_BRAKE)
            S.vel[0] *= brake, S.vel[1] *= brake;
        } else {  // the glide: a fixed speed along his facing, eased from the speed he had by GlideAcceleration
            float acc = powf(KH_GLIDE_ACCEL, ratio);  // (once per PHYS_STEP KH frames)
            float sp = (1 - acc) * KH_GLIDE_SPEED + hypotf(S.vel[0], S.vel[1]) * acc;
            S.vel[0] = sinf(S.face) * sp, S.vel[1] = cosf(S.face) * sp;  // (PHH cos/sin on x/y == our sin/cos on x/z)
        }
        move[0] = S.vel[0] * kh, move[1] = S.vel[1] * kh;
        dz = -sink * kh;
        S.req = dz, S.req_set = false, S.prev_z = z_up;  // (no landing check while gliding; glide_end's fall gets one)
        if (z_up > S.peak) S.peak = z_up;
    } else {
        // (s68 air magic) range 3 state_no_gravity (the air casts: Fire A371 16-74, Blizzard A370 16-66, Thunder A372
        // 16-58): the arc restarts at this height with no rise and its clock waits, so he hangs; the drift brakes, the
        // stick is ignored (GUESS: KH2's look, Sora still in the air). After it the h 0 arc = a fall from there.
        bool ng = S.special && S.mot && (mot_bits(S.mot, S.clock) & RB_NO_GRAVITY);
        if (ng && !S.ng) fall_start(0, z_up), LOG("[sora] no gravity from clock %.1f (motion %d)\n", S.clock, S.motion);
        S.ng = ng;
        if (!ng) S.air += kh;
        float keep = powf(KH_AIR_KEEP, ratio), brake = powf(KH_AIR_BRAKE, ratio);  // (once per PHYS_STEP KH frames)
        if (speed > 0 && !ng)
            for (int i = 0; i < 2; i++) S.vel[i] = keep * S.vel[i] + (1 - keep) * dir[i] * speed;
        else
            S.vel[0] *= brake, S.vel[1] *= brake;
        // (s50: KH2's collision eats a push into the wall; here BotW's held the body on the rock - the wall hop
        // "landed" at its peak, cutting A330 before its hit window: s50_wall1)
        float into = S.vel[0] * W.dir[0] + S.vel[1] * W.dir[1];
        if (W.on && into > 0 && hypotf(p[0] - W.pos[0], p[2] - W.pos[2]) <= g_wall_stay)
            S.vel[0] -= into * W.dir[0], S.vel[1] -= into * W.dir[1];
        move[0] = S.vel[0] * kh, move[1] = S.vel[1] * kh;
        dz = arc_target() - z_up;
        if (dz < -kh * KH_FALL_MAX) dz = -kh * KH_FALL_MAX;
        S.req = dz, S.req_set = true, S.prev_z = z_up;
        if (z_up > S.peak) S.peak = z_up;
    }

    skt_move(move, kh);  // (s110 skate: the board's velocity / the get-off slide in place of the ground / air move)
    // 6. out: cm per step -> u/s
    float vx = move[0] / kh * KH_FPS * to_u, vz = move[1] / kh * KH_FPS * to_u, vy = dz / kh * KH_FPS * to_u;
    bool vy_on = ACT_AIR(S.act);
    if (!vy_on && slope_follow(p, vx, vz, &vy)) vy_on = true;  // (s64: uphill at full speed)
    if (GT.on || g_dbm_clip || (S.special == SP_DAMAGE && !is_free) || nox_win) drive_off();  // (goto hold: zero velocity) BotW's hit push / ragdoll / pick up moves him
    else linkpos_set_velocity(true, vx, vz), linkpos_set_velocity_y(vy_on, vy);  // (the y servo on for the glide too)
    if (!((S.special == SP_DAMAGE && !is_free) || nox_win)) link_face();  // (s87c link face: ground / air only; set linkface 0 = off)
    if (SKT.rider_on) {  // (s110 skate: KH2's skate rider + board, one actor, in place of the body)
        skt_rider_draw(kh, to_u);
        g_lb_prev = g_lb_from = -1, g_lb_prev_sp = false;
    } else if (sm_call_draw(kh, to_u)) {  // (s136 summon: Sora's call motion on its own actor, in place of the body)
        g_lb_prev = g_lb_from = -1, g_lb_prev_sp = false;
    } else if (!rag_draw(rag, kh, to_u)) {  // (step 4: at Link's feet)
        if (S.mot && (S.act == ACT_GLIDE || S.act == ACT_GLIDE_HOVER)) {  // the glide motions blend (glide_pose_draw)
            if (gl_from_motion < 0) gl_from_motion = MOT_FALL, gl_from_clock = clock_old;
            glide_pose_draw(gl_from_motion, gl_from_clock, S.face, to_u, kh, GLIDE_BLEND);
            gl_from_motion = S.motion, gl_from_clock = S.clock;
            g_fb_from = -1;
        } else if (S.mot && g_gl_tail && (S.motion == MOT_FALL || S.motion == MOT_LAND) && !S.special) {
            glide_pose_draw(gl_from_motion, gl_from_clock, S.face, to_u, kh, GLIDE_BLEND);  // (glide end: Fall / Land blend in)
            gl_from_motion = S.motion, gl_from_clock = S.clock;
            if (g_gblend < 0) g_gl_tail = false, gl_from_motion = -1;  // (blend done: plain poses from here)
        } else if (S.mot && g_fb_from >= 0 && S.motion == MOT_FALL && !S.special) {  // J 4.6: Fall blends in
            glide_pose_draw(g_fb_from, g_fb_clock, S.face, to_u, kh, g_fb_len);
            g_fb_from = S.motion, g_fb_clock = S.clock;
            if (g_gblend < 0) g_fb_from = -1;
        } else if (S.mot && g_lblend && !S.special && loco_mot(S.motion) &&
                   (g_lb_from >= 0 || (g_lb_prev >= 0 && g_lb_prev != S.motion &&
                                       (loco_mot(g_lb_prev) || g_lb_prev == MOT_LAND || (g_eblend && g_lb_prev_sp))))) {
            // (s64 audit: KH2 blends idle / walk / run changes over KH_LOOP_BLEND, re:0x3A8FA0; Land -> them too)
            if (g_lb_from < 0) g_lb_from = g_lb_prev, g_lb_clock = g_lb_prev_clock, g_gblend = -1, g_glast_ok = false;
            g_gl_tail = false, gl_from_motion = -1, g_fb_from = -1;
            glide_pose_draw(g_lb_from, g_lb_clock, S.face, to_u, kh, KH_LOOP_BLEND);
            g_lb_from = S.motion, g_lb_clock = S.clock;
            if (g_gblend < 0) g_lb_from = -1;
        } else if (S.mot && fm_swap_draw(kh, to_u)) {  // (s71: base Sora -> Valor at the Drive swap)
            g_gl_tail = false, gl_from_motion = -1, g_fb_from = -1, g_lb_from = -1;
        } else if (S.mot) {
            g_gl_tail = false, gl_from_motion = -1, g_fb_from = -1, g_lb_from = -1;
            pose_draw_sm(S.mot->slot, S.clock, S.face, to_u);  // (s93 fsmooth: slerped between the baked frames)
        }
        g_lb_prev = S.mot ? S.motion : -1, g_lb_prev_clock = S.clock;
        g_lb_prev_sp = (S.special == SP_ATTACK || S.special == SP_ROLL) && !ACT_AIR(S.act);
    } else {
        g_lb_prev = g_lb_from = -1, g_lb_prev_sp = false;
    }
    party_tick(kh, to_u, st);                                       // (s54 party3: Donald + Goofy follow him)
    sm_late(kh, to_u, st);                                          // (s136 summon: Chicken Little's AI + draw)
    pl_draw(to_u);                                                  // (s133 limits: the member a party Limit holds)
    hud_update();                                                   // (his HP / MP gauge, runtime/sora_hud.c)
    int hits = blade_hits(clock_old, to_u);
    if (S.special != SP_MAGIC) C.spell = -1;  // (a hit reaction before the cast frame: no spend, no spell)
    float lmf[12];
    bool have_feet = linkpos_link_mtx(lmf);
    float feet[3] = {lmf[3], lmf[7], lmf[11]};
    if (C.fx_cast >= 0) {  // the cast glow follows the tip through the cast motion; cut short before the shot: gone
        if (S.special != SP_MAGIC) { if (!C.fired) sora_fx_kill(C.fx_cast); C.fx_cast = -1; }
        else if (have_feet) { float tp[3]; cast_tip(feet, to_u, tp); sora_fx_move(C.fx_cast, tp, S.face); }
    }
    if (S.special == SP_MAGIC && C.spell >= 0 && !C.fired && S.clock >= C.cast_at && have_feet) {
        C.fired = true;
        const KhSpell *sp = &KH_SPELLS[C.spell];
        mp_change(sp->cost >= 255 ? -(int)ceilf(MP.mp) - 1 : -sp->cost);  // (Cure: all MP, the charge starts)
        LOG("[sora] cast %s fires at clock %.1f: MP %.0f%s\n", sp->name, S.clock, MP.mp, mp_charging() ? " (charge)" : "");
        static const char *const CAST_SE[NSPELL] = {"fire_cast", "blizzard_cast", "thunder_cast", "cure_cast", "magnet_cast", "reflect_cast"};  // (KH_SPELLS order; s133: both in sfx.bin already)
        if (C.spell >= 0 && C.spell < NSPELL) sora_sfx_event(CAST_SE[C.spell], 1.0f);
        static const char *const SHOT_FX[NSPELL] = {"fire_shot", "blizzard_shot", "thunder_shot", "cure_shot", "magnet_shot", NULL};  // (Reflect: element 0 starts in spell_spawn)
        float tp[3];
        cast_tip(feet, to_u, tp);
        if (SHOT_FX[C.spell]) sora_fx_event(SHOT_FX[C.spell], tp, S.face, to_u);
        spell_spawn(C.spell, feet, S.face, g_lock.actor);
    }
    if (have_feet && IN_WISDOM && g_wshot_on && S.special == SP_ATTACK && S.mot) {  // (s73 Wisdom Shoot triggers)
        bool wair = ACT_AIR(S.act);  // (s87c wisdom combo)
        int wstep = S.d74 < 1 ? 1 : S.d74;
        bool wfin = g_wcombo && (wair ? wstep >= F_COMBO_AIR : S.mot->slot == WISDOM_MSLOT_FINISH_V);
        bool wr = g_wcombo && g_wramp;  // (s89b: ramp off by default = vanilla rate)
        int wn = !wr || wfin ? 0 : (wstep - 1 < (wair ? 1 : 2) ? wstep - 1 : (wair ? 1 : 2));
        float ws0 = !wr ? 20.0f : wfin ? 44.0f : fminf(44.0f, 20.0f + 4 * (wstep - 1) + (wair ? 8 : 0));
        int wpct = !wr ? 100 : wfin ? 150 : 100 + 10 * (wstep - 1);
        int wfirst = 1 << 30;  // (the swing's first trigger frame: one LOG line per step)
        for (int i = 0; i < S.mot->nft; i++) {
            uint16_t fr;
            int16_t el;
            memcpy(&fr, S.mot->ft + i * 8, 2), memcpy(&el, S.mot->ft + i * 8 + 4, 2);
            if (S.mot->ft[i * 8 + 2] == 1 && el >= 25 && el <= 43 && fr < wfirst) wfirst = fr;
        }
        for (int i = 0; i < S.mot->nft; i++) {
            uint16_t fr;
            int16_t el;
            memcpy(&fr, S.mot->ft + i * 8, 2), memcpy(&el, S.mot->ft + i * 8 + 4, 2);
            if (S.mot->ft[i * 8 + 2] != 1 || el < 25 || el > 43) continue;
            for (int j = 0; j <= wn; j++) {  // (j > 0: the ramped-up rate's extra bullets between the trigger frames)
                float wf = fr + j * 10.0f / (wn + 1);
                if (!(prev_clock < wf && S.clock >= wf)) continue;
                if (j > 0 && !wshot_free()) continue;  // (s87c review: extras never evict a live bullet / another fx)
                if (j == 0 && fr == wfirst && g_wcombo)
                    LOG("[wisdom] combo step %d/%d (%s%s): speed %.0f, power %d%%, +%d bullets per trigger\n", wstep,
                        wair ? F_COMBO_AIR : F_COMBO_GROUND, wair ? "air" : "ground", wfin ? ", FINISHER" : "", ws0, wpct, wn);
                float tp[3];
                cast_tip(feet, to_u, tp);
                wshot_spawn(el, tp, S.face, g_lock.actor, to_u, ws0, wpct, j == 0);
            }
        }
    }
    wshot_tick(kh, to_u);
    if (have_feet) lm_frame(kh, to_u, feet);  // (s89 Limit Form: Strike Raid's Keyblade, Infinity's shots)
    if (have_feet) rox_fx_tick(feet, to_u);  // (s110 roxsfx)
    if (have_feet) mk_fx_tick(feet, to_u);  // (s110m mickeyfx: his effect_caster triggers)
    mk_ball_tick(kh, to_u);  // (s117 pearl: the ball flies / lands its hits; it needs no feet)
    if (have_feet) af_tick(feet, to_u);  // (s107 b19-abilfx: the combo abilities' effect elements + their SE)
    if (have_feet) spell_tick(kh, to_u, feet);
    ah_tick(kh, to_u);  // (s55 airhold: servo the held / launched enemies' ragdolls to KH2's curves)
    {  // (s55: level-up chime + fx burst, sora_levels.h, every frame (the KH2 effect follows him); no feet: Link's body)
        if (!have_feet && sl_lvup_queue) LOG("[sora] level-up: no feet matrix, burst at the body %.2f %.2f %.2f\n", g_body[0], g_body[1], g_body[2]);
        float body[3] = {g_body[0], g_body[1], g_body[2]};
        sl_lvup_show(have_feet ? feet : body, S.face, to_u);
        vl_show(have_feet ? feet : body, S.face, to_u);  // (s70 Valor form level-up)
        su_show(have_feet ? feet : body, S.face, to_u);  // (s148 Summon level-up)
        wl_show(have_feet ? feet : body, S.face, to_u);  // (s75 Wisdom form level-up)
        meal_fx_show(have_feet ? feet : body, S.face, to_u);  // (s99 potfx: item-use effects)
        ll_show(have_feet ? feet : body, S.face, to_u);  // (s89 Limit form level-up)
        ml_show(have_feet ? feet : body, S.face, to_u);  // (s89 Master form level-up)
        fl_show(have_feet ? feet : body, S.face, to_u);  // (s89 b4-final Final form level-up)
        fm_fx_tick(have_feet ? feet : body, S.face, to_u);  // (s70 Drive / Valor / revert fx on their bones)
    }

    animdiag_pos(p, vx, vz, ratio, S.clock);  // (s113 anim60: cadence of Link's feet + the pose clock, log only)
    if (g_slog) {
        fprintf(g_slog, "%llu %llu %d %d %.0f | %.2f %.2f | %.3f %.3f%s | %.3f %.3f %.0f | %.1f | %.3f %.3f %.3f | "
                        "%.3f %.3f %.3f | %#x | %.0f %.1f %.1f %.1f | %#llx | %.3f | %d %.1f %d %d %.2f %d\n",
                (unsigned long long)present, (unsigned long long)(present - t0), S.act, S.motion, S.clock, sx, sy,
                cf[0], cf[1], have_cam ? "" : "?", dir[0], dir[1], speed, S.face * 180 / PI, vx,
                ACT_AIR(S.act) ? vy : 0.0f, vz, p[0], p[1], p[2], linkpos_link_state_word(), S.air, S.apex,
                ACT_AIR(S.act) ? S.base : 0.0f, ACT_AIR(S.act) ? S.req : 0.0f, (unsigned long long)held, ratio,
                S.mot ? S.mot->slot : -1, S.clock, S.special, S.d74, S.root_cm, hits);
        if (present % 30 == 0) fflush(g_slog);
    }
}

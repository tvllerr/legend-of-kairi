// Sora-in-BotW: Donald + Goofy FOLLOW Sora (s54 party3 = S35_HELPER_P_PARTY.md section 6 row 3; design + checks in
// recon/S54_HELPER_PARTY_FOLLOW.md). #included by runtime/sora.c ONCE, where slice 2's party block was (before
// sora_mod_frame): all state is static here. Draw = sora_draw.c's draw actors (slice 1, S54_HELPER_PARTY_DRAW.md),
// looked up in the exe by name (GetProcAddress, like sora.c's warp_tint): an exe linked before slice 1 -> no party.
//
// Two halves:
//  * CORE (pure C, no sora.c / linkpos / Windows): Sora's grounded trail ring, KH2's formation slots + speed bands with
//    hysteresis, turn rate, idle / walk / run by speed, warp-back, the floor under each member (world ray, rate-limited,
//    plane-extrapolated, fallback = trail height). Host-tested by party_host_test.c (-DSORA_PARTY_CORE_ONLY).
//  * GLUE (#ifndef SORA_PARTY_CORE_ONLY): reads S / Link's matrix, loads the bakes, draws (pose + 8-frame motion blend
//    + shadow), hides while Sora isn't driven, live keys.
// KH2 numbers (VERIFIED = mined by PHH, S35 4.1): bands stand [0,200] walk [160,240] run [200,1000] cm (re:0x1B41A0),
// slots (+-0.86, 0, 0.51) x 160 cm (re:0x1B2450 / re:0x723B90), walk 2 / run 8 cm/frame, turn 0.2618 rad/frame (prty
// 17 / 20 = data/party/*_params.h), motion blend 8 frames (PHH BLEND re:0x422520), warp 2500 cm (PHH WARP_UU, APPROX).
// GUESS: which slot axis is "forward" (PHH A17: live "set pfwd -1" flips it), the vertical warp, the floor ease / fall cap.
// (s55 combat, slice 1 of recon/S55_HELPER_PARTY_COMBAT.md) KH2's friend battle AI: target choice, approach, the A-row
// attacks with their baked hit windows, Donald's Fire / Blizzard / Thunder / Cure, Goofy's Tornado / Bash / Turbo, MP +
// MP Charge, the hop after an attack; damage through BotW's addDamage (linkpos_damage_enemy_ex) with the member's own
// KH2 Str / Mag (their lvup rows, the level their EXP reaches = Sora's EXP: KH2 gives all three the same), credited to
// Sora for the kill / EXP rule (sora_levels.h). Enemies do not target them yet (slice 2+). Section "COMBAT (CORE)".
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PARTY_LOG
#define PARTY_LOG LOG
#endif

#define PARTY_N 2
static bool g_party_absorbed[PARTY_N];  // (s68 drive: a Drive Form took him in - not ticked, drawn or shown on the HUD)
static bool g_plim_hold[PARTY_N];       // (s133 limits: a party Limit (sora_plimit.h) owns him - out of the follow / combat AI, drawn there; the HUD plate stays)
#define PTRAIL_N 200            // Sora's last grounded points (PHH Trail n 200)
#define PTRAIL_STEP_CM 40.0f    // ... at least this far apart (PHH step 40 UU) or PTRAIL_DY_CM up / down
#define PTRAIL_DY_CM 20.0f
#define PARTY_SIDE_CM 150.0f    // the return spot beside Sora (PHH RETURN_SIDE_UU)
#define PARTY_WARP_CM 2500.0f   // horizontal: beyond this a member reappears beside Sora (PHH WARP_UU)
#define PARTY_WARP_DY_CM 500.0f // (GUESS) vertical: back from a hidden stretch (a climb) with his floor this far from
                                // Sora's ground -> beside him. Only then: a timed rule loops when Sora stands on a
                                // pillar (the return spot is over the low ground too; host test G2)
#define PARTY_BLEND 8.0f        // KH frames, idle / walk / run change (PHH BLEND, re:0x422520)
#define PARTY_FLOOR_EASE 30.0f  // 1/s (PHH FLOOR_EASE): a JUMP of the floor source (new cast, other trail point,
                                // ray <-> trail) is eased out by 1 - exp(-dt x this) per present; a plane he walks
                                // along is followed exactly (PHH's plain ease sank 2.3 cm into a 1:4 rise: host test F)
#define PARTY_FALL_CM 15.0f     // (GUESS) cm per KH frame at most downward (a member off an edge sinks, not pops)
#define PARTY_RAY_UP 2.0f       // u above max(his floor, Sora's ground) the ray starts
#define PARTY_RAY_DOWN 6.0f     // u below min(his floor, Sora's ground, Sora's feet) it ends
#define PARTY_RAY_MAX 60.0f     // u, longest cast
#define PARTY_RAY_MOVE 0.24f    // u moved since his last cast: due again (PHH FLOOR_RETRACE_UU 24)
#define PARTY_RAY_EVERY 6       // presents between casts while moving (PHH 5 a second at ~31 fps)
#define PARTY_RAY_IDLE 60       // ... while standing
#define PARTY_RAY_NY 0.3f       // hits steeper than this (normal y) = a wall, not his floor
#define PARTY_RAY_REACH 1.5f    // u: the last hit's plane is his floor within this of it
#define PARTY_RAY_EXTRA 0.5f    // u: the plane may move the floor at most this far from the hit
#define PARTY_GAP 3             // presents without a tick = he was hidden (loads, pause, puppet): the reappear rules
#define PARTY_MISS_LONG 1       // misses in a row -> the next cast is PARTY_RAY_MAX long (Sora on a pillar: host G2);
                                // still none: the trail height (over a deep drop they float at it: GUESS rare)
// (s54 party3) KH2 knockdown = ACTION 4 BLOW (PHH hit_reaction_kh2.py, mined from 03system.bin: VERIFIED numbers)
#define KD_UP 24.0f             // sstm BlowUp, cm / frame (re:0x3CEA00)
#define KD_SPEED 40.0f          // sstm BlowSpeed, cm / frame
#define KD_BRAKE_XZ 0.8f        // horizontal x this per physics sub-step (re:0x422780) ...
#define KD_MIN_XZ 0.1f          // ... down to start x this (re:0x4228B0)
#define KD_BRAKE_UP 0.8f        // rising speed x this per sub-step
#define KD_G 0.40816328f        // gravity, cm / frame per sub-step (re:0x5CE184)
#define KD_FALL_MAX 16.0f       // sstm FallMax (re:0x40A440)
#define KD_STEP 2.0f            // the ACTION physics sub-step, KH frames (re:0x5B18EC)
#define KD_BLOW_FRONT 16        // motion ids: BLOW_FRONT / BLOW_BACK (re:0x3D2FE0); landing = + 1 (re:0x422700)
#define KD_BLOW_BACK 18
#define KD_AIR_MAX 300.0f       // (GUESS) KH frames in the air at most (no floor found): land where he is
#define KD_RADIUS_CM 600.0f     // (GUESS, coordinator) a ragdoll start blows members within this of Link (live pkdr)
enum { PSRC_RAY, PSRC_TRAIL, PSRC_LAST, PSRC_WARP };
enum { PBAND_STAND, PBAND_WALK, PBAND_RUN };
enum { KD_NONE, KD_AIR, KD_DOWN };  // knockdown: none / flying (blow loop) / landed (landing + down + get-up motion)
// (s55 combat) the member's action: an A-row attack, a cast (Donald's spells / Goofy's abilities), the hop after an
// attack (KH2 B rows: motion 3 = jump, then 5 = land), its landing
enum { PACT_NONE, PACT_ATTACK, PACT_CAST, PACT_HOP, PACT_LAND };

typedef struct { int slot; uint16_t frames; const float *root; } PMot;  // root: (frames + 1) x {x y z yaw} or NULL
typedef struct {
    const char *name;
    float side;               // +1 his left (Donald), -1 his right (Goofy): the return spot (PHH return_spot)
    int slot[3];              // idle / walk / run mset slots (field + weapon: A000 v2, A001 v3, A002 v3; *_params.h)
    float frames[3];          // loop lengths (KH frames; <name>_motion.bin when it loads, these defaults otherwise)
    float walk, run, turn;    // prty: cm / frame, rad / frame
    float shadow_r;           // u (slice 2, GUESS)
    // knockdown slots (id x 4 + 2, the field variant: the only one baked; partybake info): blow front / its landing,
    // blow back / its landing; frames + root curves from <name>_motion.bin (defaults = the bake as read 2026-10-29)
    PMot kd[4];
    float lie_at;             // (GUESS) the landing frame a "set plie" hold stops on (Donald's body lowest f27..57)
    // (s55 combat) battle stance idle / walk / run = variant 0 (battle + weapon: A000 v0, A001 v0, A002 v0; PHH party.py
    // PARTY_VARIANT_ORDER re:0x5C5D38) + their loop lengths (motion.bin)
    int bslot[3];
    float bframes[3];
    // friend_param header (party_ai.kh.json, re:0x71D850 Donald / re:0x71DD50 Goofy; VERIFIED): cm, KH frames
    float search, keep, leash, pause_min, pause_max, move_min, move_max, hop_chance, mp_gate_scale;
    int nhop;                 // B rows (hops after an attack, re:0x1CC810)
    struct { float angle, up, speed, brake, frames; } hop[2];
    uint8_t wstr, wmag;       // the weapon (03system.bin stat rows, VERIFIED): Mage's Staff Str 1 Mag 1 @0x13F34,
                              // Knight's Shield Str 1 @0x13FE4
    float mp_max, mp_charge;  // plrp MP 100 (00battle.bin@0x31150 / 0x311D0) and prty MpChargeTime 1200 (VERIFIED)
} PartyDef;
static PartyDef PARTY[PARTY_N] = {
    {"donald", 1, {2, 7, 11}, {144, 48, 36}, 2, 8, 0.261799f, 0.30f, {{66, 18, 0}, {70, 106, 0}, {74, 18, 0}, {78, 94, 0}}, 40,
     {0, 4, 8}, {400, 52, 36}, 800, 1200, 1200, 6, 60, 6, 180, 0.1f, 1, 1, {{180, 15, 12, 0.98f, 12}, {0}}, 1, 1, 100, 1200},
    {"goofy", -1, {2, 7, 11}, {264, 60, 52}, 2, 8, 0.261799f, 0.40f, {{66, 22, 0}, {70, 110, 0}, {74, 22, 0}, {78, 90, 0}}, 36,
     {0, 4, 8}, {292, 72, 40}, 800, 1200, 1200, 6, 60, 600, 900, 0.5f, 1, 2, {{180, 100, 18, 0.98f, 18}, {0, 100, 10, 0.98f, 18}},
     1, 0, 100, 1200},
};
static const float PARTY_BANDS[3][2] = {{0, 200}, {160, 240}, {200, 1000}};  // cm (min, max), re:0x1B41A0
static const char *const PARTY_BAND_NAME[3] = {"stand", "walk", "run"};
static const char *const PARTY_SRC_NAME[4] = {"ray", "trail", "last", "warp"};

typedef struct {
    bool placed;
    float pos[3], face;       // feet (world u; pos[1] = floor_s), facing (rad, direction (sin, cos) in x, z like S.face)
    float floor_s;            // the eased floor (u)
    int band, motion;         // motion 0 idle 1 walk 2 run (index into PartyDef.slot)
    float clock;
    int from_slot;            // the blend: from this slot / clock (frozen pose), blend KH frames since the switch
    float from_clock, blend;  // blend < 0: none
    bool blend_from_blend;    // the switch came mid-blend: the from-pose = the last blended pose (glue keeps it)
    float goal[3], d_cm;      // formation slot, distance to it (cm)
    int src;                  // this frame's floor source (PSRC_*)
    bool ray_ok, ray_due;     // a usable hit / cast him first
    float ray_y, ray_n[3], ray_at[3];
    uint64_t ray_present, ray_id;  // ray_id: P.casts at his last usable hit (a new plane = a new floor source)
    int64_t src_key;          // the floor source of the last present (a change -> its jump goes into floor_off)
    float floor_off;          // floor_s - source, eased to 0
    float floor_t;            // this present's floor (un-eased): a blown member lands on it
    int misses;               // ray misses in a row
    int warps;
    bool warp_held;           // (s118) the far warp is waiting for Sora to land (glide / fall)
    // knockdown (KH2 ACTION 4 BLOW): kd_k 0 front 1 back (PARTY.kd[2 k] = blow loop, [2 k + 1] = its landing)
    int kd, kd_k, kd_n;
    float kd_clock, kd_h, kd_hmin, kd_vy, kd_dir[2], kd_acc, kd_air, kd_lie, kd_from[3], kd_dist, kd_power;
    // (s55 combat) KH2's friend AI state (party_kh2 names: +0xBF8 target, +0xE18 flags, +0xEE8 current entry, +0xED0..
    // +0xEE0 the last three non-move entries, +0xF80 non-move count, +0xF64 the sustained entry's timer)
    bool stance;              // battle stance (variant 0 slots) - set with the battle flag
    float blend_n;            // this blend's length (KH frames; 0 = a cut)
    int tgt;                  // index into this present's PIn.en (-1 none); tgt_actor survives the re-indexing
    uint64_t tgt_actor;
    bool leash, tgt_changed;
    float t_d, t_r, t_dy, t_flat, t_dir[2], t_ctr[3];  // to the target's part: surface d, radius term, KH dy (y down),
                                                        // flat centre distance, flat unit dir (cm), centre (u)
    int cur, hist[3], nonmove;
    float sus_t;              // the sustained entry's timer (pause / move / run), KH frames
    float run_dir[2];         // Goofy's run (re:0x1CE8F0) direction
    bool gate;                // this think's MP gate roll (re:0x1B53B0, E18_MP_GATE)
    float mp, charge;         // MP (KH 100 scale), MP Charge frames left (> 0: charging)
    float hp, hp_max;         // (slice 2: damage to them; slice 1 keeps them full) KH HP
    // the playing action (busy: not free; KH2's attack / cast motions have no free range: free at their end)
    int act, act_entry, act_slot, act_spell, act_phase;
    float act_clock, act_frames, act_loop, act_release;
    uint64_t act_target;
    float act_tpos[3];
    bool act_fired, seg_ok;
    float seg[6];
    int act_nhit, act_win, intent;
    uint64_t act_hit[16];
    float act_hit_t[16];      // (long windows: KH frames since that enemy's last hit; re-hit every PC_REHIT, GUESS)
    float hop_v[3], hop_g, hop_brake;  // KH2 jump_start (re:0x3CBEC0): velocity cm / frame (y up), gravity, brake
    int forced;               // live "party cast <name>": this entry next think (-1 none)
    int hits;                 // (log)
} PMember;

typedef struct { float p[PTRAIL_N][3]; int head, n; } PTrail;  // ring: newest at (head - 1)

typedef bool (*PartyRay)(const float from[3], const float to[3], float hit[3], float normal[3]);
// (s55 combat) one enemy of linkpos's list this present: feet (u), life (> 0 alive)
typedef struct { uint64_t actor; float pos[3]; int32_t life; } PEnemy;
// What the combat core asks of the game (sora.c glue / the host test's stubs). NULL fields = not available.
typedef struct {
    // BotW damage for a party hit (KH2 rule: the member's Str / Mag x Power; credits Sora for the kill / EXP)
    int (*damage)(int member, uint64_t actor, int32_t life, int power, int magic);
    // queue it (linkpos_damage_enemy_ex) + its spark / hit se at 'at' (fx name, atkp row for the se, 0 = none)
    void (*hit)(int member, uint64_t actor, int dmg, int f54, int f40, const float at[3], const char *fx, int atkp);
    int (*heal_sora)(int member);      // Donald's Cure on Sora: returns Link's life after
    int (*fx)(const char *name, const float pos[3], float yaw);  // an effect (handle or -1)
    void (*fx_move)(int h, const float pos[3], float yaw);
    void (*fx_end)(int h, float fade);  // fade 0 = kill
    void (*weapon)(int member, bool shown);  // Goofy's shield in hand (Goofy Bash throws it)
    // (s56 party sfx, appended: older initializers leave it NULL = silent) a sound moment of an action the frame
    // triggers can't see (Donald's acts start their voice / effects from his object script, not from the motion):
    // ev PSND_*, arg = the spell (PS_*) or the cure target (0 Sora, 1 Donald, 2 Goofy), at = where (world u)
    void (*sound)(int member, int ev, int arg, const float at[3]);
} PHooks;
enum { PSND_ACT, PSND_CURE_VOICE, PSND_RELEASE, PSND_BOLT, PSND_CURE_ON };
typedef struct {
    float feet[3], face;      // Sora: feet (world u), facing
    bool grounded;            // driving, on the ground (trail points + the vertical warp rule)
    float kh, to_u;           // KH frames this present, u per KH cm
    uint64_t present;
    PartyRay ray;             // NULL = no casts (trail only)
    // (s55 combat) appended (older initializers leave them 0: no enemies = no battle)
    const PEnemy *en;
    int nen;
    float hit_r, hit_h;       // the enemy cylinder sora.c's swings use (u): radius, height (0: 0.5 / 1.6)
    float sora_hp, sora_hp_max;  // Link's life (quarter hearts) and its max: Donald's Cure rule
    const PHooks *hk;
} PIn;

static struct {
    PTrail trail;
    PMember m[PARTY_N];
    bool on, log, ray_on, ray_works, hidden;
    int spawn;                // (s99 b14-bed) live pspawn 1: placements use the KH2 slots + wall / floor rays; 0 = the old 150 cm beside
    int fwd, ray_turn;        // fwd: +1 the slots 0.51 r ahead of Sora (KH data as read), -1 behind (PHH A17 open)
    float slot_r_cm, kd_radius_cm, kd_lie;  // kd_lie: extra KH frames held at lie_at (live plie; 0 = the KH2 motion)
    uint64_t last_present, ticks, casts, hits, cast_present, casts_this_present;
    int max_casts_per_present;
    // (s55 combat) live: pcombat (the battle AI), pclog (decision / hit log), pair (the jump attack rows, off: no jump
    // arc yet), pstop (cm: how close they walk to an enemy's surface; KH2's body push stand-in)
    bool combat, clog, air_rows, battle;
    float stop_cm;
    uint32_t rnd;             // MSVC rand() state (KH2 PC: re:0x19BE90 rand_n / re:0x19BF50 rand_range)
    uint64_t battle_ticks;
    const PHooks *hk;         // (the last tick's: a knockdown cuts an action short)
} P = {.on = true, .spawn = 1 /* s183: default ON with the New Game wake-up (verified s99) */, .log = true, .ray_on = true, .fwd = 1, .slot_r_cm = 160, .kd_radius_cm = KD_RADIUS_CM,
       .combat = true, .clog = true, .stop_cm = 30, .rnd = 1};

static float pt_wrap(float a) { return atan2f(sinf(a), cosf(a)); }
static float pt_clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

// ---- the trail (Sora's grounded feet; PHH Trail; KH2's warp trail re:0x1B7750 is the same idea)
static void ptrail_reset(PTrail *t) { t->head = t->n = 0; }
static const float *ptrail_last(const PTrail *t) { return t->n ? t->p[(t->head + PTRAIL_N - 1) % PTRAIL_N] : NULL; }
static void ptrail_push(PTrail *t, const float q[3], float to_u) {
    const float *l = ptrail_last(t);
    if (l && hypotf(q[0] - l[0], q[2] - l[2]) > PARTY_WARP_CM * to_u) ptrail_reset(t), l = NULL;  // (a teleport)
    if (l && hypotf(q[0] - l[0], q[2] - l[2]) < PTRAIL_STEP_CM * to_u && fabsf(q[1] - l[1]) < PTRAIL_DY_CM * to_u) return;
    memcpy(t->p[t->head], q, sizeof t->p[0]);
    t->head = (t->head + 1) % PTRAIL_N;
    if (t->n < PTRAIL_N) t->n++;
}
static int ptrail_floor(const PTrail *t, float x, float z, float *y) {  // the nearest point's height (any distance)
    float bd = 1e30f;
    int best = -1;
    for (int i = 0; i < t->n; i++) {
        float d = (t->p[i][0] - x) * (t->p[i][0] - x) + (t->p[i][2] - z) * (t->p[i][2] - z);
        if (d < bd) bd = d, *y = t->p[i][1], best = i;
    }
    return best;  // (-1: empty)
}

// Sora's ground height: his feet while grounded, else the trail's newest point, else his feet
static float party_sora_ground(const PIn *in) {
    const float *l = ptrail_last(&P.trail);
    return in->grounded || !l ? in->feet[1] : l[1];
}

static bool pc_bash_out(int i);                       // (s58: Goofy's shield is out, flying or at his catch spot)
static void pc_bash_end(int i, const char *why);       // (s58: an act cut short ends its flight: KH2 kills the thread)
// (s99 b14-bed, live "pspawn 1") KH2's two slots (+-0.86 side, 0.51 behind) x r (slot_r_cm 160 cm, the formation's
// radius; the old placement was PARTY_SIDE_CM 150 beside): the spot is tried from Sora's feet (+0.5 m) with a wall ray, the
// next candidates are half the side, straight behind, then close beside him; a floor ray sets the height.
// Returns the slot / fallback name used (for the log).
static const char *party_spawn_spot(int i, const PIn *in, float out[3], float *gy_io) {
    float r = P.slot_r_cm * in->to_u, s = sinf(in->face), c = cosf(in->face);
    float sd = PARTY[i].side * 0.86f * r, bk = -0.51f * r;  // (side +1 = his left (cos, -sin); bk < 0 = behind (s, c) = forward)
    static const char *NAME[4] = {"kh2 slot", "half side", "straight behind", "close beside"};
    float cs[4][2] = {{sd, bk}, {0.5f * sd, bk}, {0.0f, bk}, {PARTY[i].side * 0.25f * r, 0.0f}};
    float up = 0.5f * 100.0f * in->to_u;  // 50 cm: above the feet, below a rim of a pod
    int pick = 3;
    float x = 0, z = 0;
    for (int k = 0; k < 4; k++) {
        x = in->feet[0] + c * cs[k][0] + s * cs[k][1], z = in->feet[2] - s * cs[k][0] + c * cs[k][1];
        if (!in->ray || k == 3) { pick = k; break; }
        float a[3] = {in->feet[0], in->feet[1] + up, in->feet[2]}, b[3] = {x, in->feet[1] + up, z}, h[3] = {0}, n[3] = {0};
        if (!in->ray(a, b, h, n)) { pick = k; break; }
        float full = hypotf(x - a[0], z - a[2]), got = hypotf(h[0] - a[0], h[2] - a[2]), keep = got - 25.0f * in->to_u;
        if (keep >= 0.4f * full && keep >= 40.0f * in->to_u) {  // (a wall past the middle: stop short of it)
            float t = keep / full;
            x = a[0] + (x - a[0]) * t, z = a[2] + (z - a[2]) * t, pick = k;
            break;
        }
    }
    float y = *gy_io;
    if (in->ray) {
        float a[3] = {x, in->feet[1] + 100.0f * in->to_u, z}, b[3] = {x, in->feet[1] - 300.0f * in->to_u, z}, h[3] = {0}, n[3] = {0};
        if (in->ray(a, b, h, n) && n[1] >= PARTY_RAY_NY && fabsf(h[1] - in->feet[1]) < 200.0f * in->to_u) y = h[1];
    }
    out[0] = x, out[1] = y, out[2] = z, *gy_io = y;
    return NAME[pick];
}

// The wake-up scene's end (sora.c): the members place again at the next tick ("first frame").
static void party_unplace_all(void) {
    for (int i = 0; i < PARTY_N; i++) P.m[i].placed = false;
}

static void party_place(int i, const PIn *in, const char *why) {
    PMember *m = &P.m[i];
    float d = PARTY[i].side * PARTY_SIDE_CM * in->to_u, s = sinf(in->face), c = cosf(in->face);
    float gy = party_sora_ground(in);
    const char *spot = NULL;
    if (P.spawn) {  // (s99 b14-bed) default 0 leaves the old line below untouched
        float q[3];
        spot = party_spawn_spot(i, in, q, &gy);
        m->pos[0] = q[0], m->pos[1] = q[1], m->pos[2] = q[2];
        ptrail_reset(&P.trail);  // (the trail may hold points of a place he was moved from, e.g. a pod)
        if (in->grounded) ptrail_push(&P.trail, in->feet, in->to_u);
    } else
    m->pos[0] = in->feet[0] + c * d, m->pos[1] = gy, m->pos[2] = in->feet[2] - s * d;  // ((cos, -sin) = his left)
    m->floor_s = gy, m->face = in->face, m->band = PBAND_STAND, m->motion = 0, m->clock = 0, m->blend = -1;
    m->ray_ok = false, m->ray_due = true, m->src = PSRC_WARP, m->src_key = -1, m->floor_off = 0;
    m->floor_t = gy, m->misses = 0, m->kd = KD_NONE;
    if (m->act && in->hk && in->hk->weapon) in->hk->weapon(i, true);  // (s55: an action cut short, e.g. a thrown shield)
    pc_bash_end(i, why);
    m->act = 0, m->cur = -1, m->sus_t = 0;
    if (m->placed) m->warps++;
    m->placed = true;
    if (spot) PARTY_LOG("[party] %s placed (%s) slot \"%s\" (pspawn): %.3f %.3f %.3f, Sora feet %.3f %.3f %.3f face %.2f, %.0f cm away, y from %s\n",
                        PARTY[i].name, why, spot, m->pos[0], m->pos[1], m->pos[2], in->feet[0], in->feet[1], in->feet[2], in->face,
                        hypotf(m->pos[0] - in->feet[0], m->pos[2] - in->feet[2]) / in->to_u, in->ray ? "floor ray / Sora's ground" : "Sora's ground");
    else
    PARTY_LOG("[party] %s placed beside Sora (%s) at %.3f %.3f %.3f\n", PARTY[i].name, why, m->pos[0], m->pos[1], m->pos[2]);
}

// KH2's formation (re:0x1B2450): two slots at (+-0.86 side, 0.51 forward) x r in Sora's frame; each member takes the
// nearer one, the nearer member keeps a shared one.
static void party_slots(const PIn *in) {
    float r = P.slot_r_cm * in->to_u, s = sinf(in->face), c = cosf(in->face), q[2][3];
    for (int k = 0; k < 2; k++) {
        float fw = 0.51f * r * (float)P.fwd, sd = (k ? -0.86f : 0.86f) * r;  // k 0 = his left (cos, -sin)
        q[k][0] = in->feet[0] + s * fw + c * sd, q[k][1] = in->feet[1], q[k][2] = in->feet[2] + c * fw - s * sd;
    }
    float d[PARTY_N][2];
    int pick[PARTY_N];
    for (int i = 0; i < PARTY_N; i++) {
        for (int k = 0; k < 2; k++) d[i][k] = hypotf(P.m[i].pos[0] - q[k][0], P.m[i].pos[2] - q[k][2]);
        pick[i] = d[i][0] <= d[i][1] ? 0 : 1;
    }
    if (pick[0] == pick[1]) {
        if (d[0][pick[0]] <= d[1][pick[1]]) pick[1] = 1 - pick[0];
        else pick[0] = 1 - pick[1];
    }
    for (int i = 0; i < PARTY_N; i++) memcpy(P.m[i].goal, q[pick[i]], sizeof q[0]);
}

// One floor cast for member i (from above both his floor and Sora's ground down past both and Sora's feet).
static void party_cast(int i, const PIn *in) {
    PMember *m = &P.m[i];
    float sg = party_sora_ground(in), hi = fmaxf(fmaxf(m->floor_s, sg), m->pos[1]) + PARTY_RAY_UP;  // (pos: a blow's arc)
    float lo = fminf(fminf(m->floor_s, sg), in->feet[1]) - PARTY_RAY_DOWN;
    if (hi - lo > PARTY_RAY_MAX || m->misses >= PARTY_MISS_LONG) lo = hi - PARTY_RAY_MAX;
    float a[3] = {m->pos[0], hi, m->pos[2]}, b[3] = {m->pos[0], lo, m->pos[2]}, h[3] = {0}, n[3] = {0};
    bool hit = in->ray(a, b, h, n);
    P.casts++, m->ray_present = in->present, m->ray_due = false, memcpy(m->ray_at, m->pos, sizeof m->ray_at);
    if (P.cast_present != in->present) P.cast_present = in->present, P.casts_this_present = 0;
    if (++P.casts_this_present > (uint64_t)P.max_casts_per_present) P.max_casts_per_present = (int)P.casts_this_present;
    if (hit) P.ray_works = true;
    m->ray_ok = hit && n[1] >= PARTY_RAY_NY;
    m->misses = m->ray_ok ? 0 : m->misses + 1;
    if (m->ray_ok) P.hits++, m->ray_y = h[1], m->ray_id = P.casts, memcpy(m->ray_n, n, sizeof m->ray_n);
}

// The cast budget: at most ONE per present for the whole party (sora.c's own floor / blade rays share the frame),
// given to a member that is due (ray_due first, else round robin).
static void party_rays(const PIn *in) {
    if (!P.ray_on || !in->ray) return;
    int pick = -1;
    for (int k = 0; k < PARTY_N && pick < 0; k++)
        if (P.m[k].ray_due) pick = k;
    for (int k = 0; k < PARTY_N && pick < 0; k++) {
        int i = (P.ray_turn + k) % PARTY_N;
        PMember *m = &P.m[i];
        uint64_t age = in->present - m->ray_present;
        float moved = hypotf(m->pos[0] - m->ray_at[0], m->pos[2] - m->ray_at[2]);
        bool moving = m->band != PBAND_STAND || m->kd != KD_NONE;
        if (moved >= PARTY_RAY_MOVE || age >= (moving ? PARTY_RAY_EVERY : PARTY_RAY_IDLE)) pick = i;
    }
    if (pick < 0) return;
    P.ray_turn = (pick + 1) % PARTY_N;
    party_cast(pick, in);
}

// His floor this present: the last hit's plane (near it), else the nearest trail point, else the last floor. A jump of
// the source goes into floor_off, eased out (PARTY_FLOOR_EASE); downward at most PARTY_FALL_CM a KH frame.
static void party_floor(int i, const PIn *in) {
    PMember *m = &P.m[i];
    float t = m->floor_s;
    int64_t key = PSRC_LAST;
    m->src = PSRC_LAST;
    float dx = m->pos[0] - m->ray_at[0], dz = m->pos[2] - m->ray_at[2];
    int tp;
    if (P.ray_on && m->ray_ok && hypotf(dx, dz) <= PARTY_RAY_REACH) {
        float e = -(m->ray_n[0] * dx + m->ray_n[2] * dz) / m->ray_n[1];  // (the plane through the hit)
        t = m->ray_y + pt_clampf(e, -PARTY_RAY_EXTRA, PARTY_RAY_EXTRA), m->src = PSRC_RAY;
        key = ((int64_t)m->ray_id << 2) | PSRC_RAY;
    } else if ((tp = ptrail_floor(&P.trail, m->pos[0], m->pos[2], &t)) >= 0)
        m->src = PSRC_TRAIL, key = ((int64_t)tp << 2) | PSRC_TRAIL;
    m->floor_t = t;
    if (m->kd == KD_AIR || m->act == PACT_HOP) { m->src_key = -1; return; }  // (in the air: the arc owns pos[1]; he
                                                                          // lands on floor_t)
    if (key != m->src_key) m->floor_off = m->floor_s - t, m->src_key = key;  // (LAST: t = floor_s, off 0)
    float dt = in->kh / 60.0f, k = 1 - expf(-dt * PARTY_FLOOR_EASE);
    m->floor_off *= 1 - k;
    if (fabsf(m->floor_off) < 1e-4f) m->floor_off = 0;
    float y = t + m->floor_off, fall = PARTY_FALL_CM * in->kh * in->to_u;
    if (y < m->floor_s - fall) y = m->floor_s - fall, m->floor_off = y - t;
    m->floor_s = m->pos[1] = y;
}

// (s55) the locomotion slot / loop length of motion k (0 idle 1 walk 2 run) in his stance (battle: variant 0)
static int party_loco_slot(int i, int k) { return P.m[i].stance ? PARTY[i].bslot[k] : PARTY[i].slot[k]; }
static float party_loco_frames(int i, int k) { return P.m[i].stance ? PARTY[i].bframes[k] : PARTY[i].frames[k]; }
// (s55) start the pose blend from what is drawn now (slot / clock) over n KH frames (0 = a cut)
static void party_blend_from(int i, int slot, float clock, float n) {
    PMember *m = &P.m[i];
    if (n <= 0) { m->blend = -1; return; }
    m->blend_from_blend = m->blend >= 0;
    m->from_slot = slot, m->from_clock = clock, m->blend = 0, m->blend_n = n;
}
static float party_blend_len(const PMember *m) { return m->blend_n > 0 ? m->blend_n : PARTY_BLEND; }

// Move member i toward goal with the bands (cm, hysteresis), turn rate, straight line (no world collision: PHH A17).
// stop_cm > 0: he stops that far (flat) from the goal (s55: an enemy's centre; KH2 pushes bodies apart re:0x3B9090).
// face_to: standing, he turns to this yaw (following: Sora's facing; fighting: toward the target).
static void party_steer(int i, const PIn *in, const float goal[3], const float (*bands)[2], float stop_cm, float face_to) {
    PMember *m = &P.m[i];
    const PartyDef *pd = &PARTY[i];
    float dx = goal[0] - m->pos[0], dz = goal[2] - m->pos[2], dist = hypotf(dx, dz);
    m->d_cm = dist / in->to_u;
    int band0 = m->band;
    if (m->d_cm > bands[m->band][1] && m->band < PBAND_RUN) m->band++;
    else if (m->d_cm < bands[m->band][0] && m->band > PBAND_STAND) m->band--;
    if (stop_cm > 0 && m->d_cm <= stop_cm) m->band = PBAND_STAND;
    float speed = m->band == PBAND_RUN ? pd->run : m->band == PBAND_WALK ? pd->walk : 0;
    float want = speed > 0 && dist > 1e-5f ? atan2f(dx, dz) : face_to;  // (stopped: he turns Sora's way, PHH)
    float step = pd->turn * in->kh, dface = pt_wrap(want - m->face);
    m->face = pt_wrap(m->face + pt_clampf(dface, -step, step));
    if (speed > 0 && dist > 1e-5f) {
        float mv = fminf(dist - (stop_cm > 0 ? stop_cm * in->to_u : 0), speed * in->kh * in->to_u);
        if (mv > 0) m->pos[0] += dx / dist * mv, m->pos[2] += dz / dist * mv;
    }
    int mot = m->band;  // (idle / walk / run = the band: speed 0 / walk / run)
    if (mot != m->motion) {
        party_blend_from(i, party_loco_slot(i, m->motion), m->clock, PARTY_BLEND);
        m->motion = mot, m->clock = 0;
    } else {
        m->clock += in->kh;
        if (m->blend >= 0 && (m->blend += in->kh) >= party_blend_len(m)) m->blend = -1;
    }
    float n = party_loco_frames(i, m->motion);
    if (n > 0) m->clock = fmodf(m->clock, n);
    if (band0 != m->band && P.log)
        PARTY_LOG("[party] %s band %s -> %s at d=%.0f cm\n", pd->name, PARTY_BAND_NAME[band0], PARTY_BAND_NAME[m->band],
                  m->d_cm);
}
// Follow Sora's formation slot (the s54 rule, unchanged).
static void party_move(int i, const PIn *in) { party_steer(i, in, P.m[i].goal, PARTY_BANDS, 0, in->face); }

// ---- <name>_motion.bin ('SKRM', sorabake / partybake; sora.c bake_load's layout): the loop lengths of idle / walk /
// run and the knockdown slots' frames + root curves. The file stays in memory (root points into it; ~25 KB, a hot
// reload loads it again: never freed).
// (s55) every slot is also kept in PMOT (frames, loop start, root curve, frame triggers {u16 frame, u8 id, u8 n, i16 p1,
// i16 p2}): the attack / cast motions.
typedef struct { uint16_t slot, frames, loop, flags, nft; const float *root; const uint8_t *ft; } PMotX;
#define PMOT_MAX 64
static PMotX PMOT[PARTY_N][PMOT_MAX];
static int PMOT_N[PARTY_N];
static const PMotX *pmot_find(int i, int slot) {
    for (int k = 0; k < PMOT_N[i]; k++)
        if (PMOT[i][k].slot == slot) return &PMOT[i][k];
    return NULL;
}
static float pmot_trigger(const PMotX *mo, int id, float dflt) {  // first frame trigger 'id' (9 = release / cast)
    for (int k = 0; mo && k < mo->nft; k++)
        if (mo->ft[k * 8 + 2] == id) {
            uint16_t fr;
            memcpy(&fr, mo->ft + k * 8, 2);
            return fr;
        }
    return dflt;
}
static int party_mot_load(int i, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = n > 16 ? (uint8_t *)malloc((size_t)n) : NULL;
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n || memcmp(b, "SKRM", 4)) {
        fclose(f), free(b);
        return 0;
    }
    fclose(f);
    uint32_t count;
    memcpy(&count, b + 8, 4);
    int got = 0;
    PMOT_N[i] = 0;
    for (uint32_t k = 0; k < count && 16 + (k + 1) * 24 <= (uint32_t)n; k++) {
        const uint8_t *d = b + 16 + k * 24;
        uint16_t slot, frames;
        uint32_t root;
        memcpy(&slot, d, 2), memcpy(&frames, d + 2, 2), memcpy(&root, d + 8, 4);
        if (!frames) continue;
        if (PMOT_N[i] < PMOT_MAX) {
            PMotX *x = &PMOT[i][PMOT_N[i]++];
            uint32_t ft;
            memcpy(&x->loop, d + 4, 2), memcpy(&x->flags, d + 6, 2), memcpy(&ft, d + 16, 4), memcpy(&x->nft, d + 20, 2);
            x->slot = slot, x->frames = frames;
            x->root = root && root + (frames + 1) * 16u <= (uint32_t)n ? (const float *)(b + root) : NULL;
            x->ft = ft && ft + x->nft * 8u <= (uint32_t)n ? b + ft : NULL;
            if (!x->ft) x->nft = 0;
        }
        for (int j = 0; j < 3; j++)
            if (slot == PARTY[i].bslot[j]) PARTY[i].bframes[j] = frames;
        for (int j = 0; j < 3; j++)
            if (slot == PARTY[i].slot[j]) PARTY[i].frames[j] = frames, got++;
        for (int j = 0; j < 4; j++)
            if (slot == PARTY[i].kd[j].slot) {
                PARTY[i].kd[j].frames = frames, got++;
                PARTY[i].kd[j].root = root && root + (frames + 1) * 16u <= (uint32_t)n ? (const float *)(b + root) : NULL;
            }
    }
    PARTY_LOG("[party] %s: motion %s: idle %.0f walk %.0f run %.0f, blow %u/%u land %u/%u frames (%d of 7 from the "
              "file)\n", PARTY[i].name, path, PARTY[i].frames[0], PARTY[i].frames[1], PARTY[i].frames[2],
              PARTY[i].kd[0].frames, PARTY[i].kd[2].frames, PARTY[i].kd[1].frames, PARTY[i].kd[3].frames, got);
    return got;
}
static void pmot_root(const PMot *mo, float t, float r[3]) {  // (sora.c mot_root: linear between the baked frames)
    r[0] = r[1] = r[2] = 0;
    if (!mo->root || !mo->frames) return;
    t = pt_clampf(t, 0, mo->frames);
    int f = (int)t;
    if (f >= mo->frames) f = mo->frames - 1;
    float u = t - (float)f;
    for (int c = 0; c < 3; c++) r[c] = mo->root[f * 4 + c] + (mo->root[(f + 1) * 4 + c] - mo->root[f * 4 + c]) * u;
}

// ---- knockdown: KH2's ACTION 4 BLOW on a member (PHH hit_reaction_kh2 Blow, re:0x3D2FE0 case 4 / re:0x4228B0 /
// re:0x422780): BLOW_FRONT when he faces the source (KH2 flies backwards facing it), else BLOW_BACK; horizontal speed
// away from the source braked x0.8 per 2-frame sub-step down to 0.1 x start, rise x0.8 braked plus gravity, fall
// capped; the blow motion loops in the air; on the floor its landing (motion + 1: land, lie, get up; root motion
// applied), then idle and following again. power 1 = sstm BlowUp 24 / BlowSpeed 40 cm/frame (both scaled).
static void party_knockdown_core(int i, const float from[3], float power, float to_u) {
    PMember *m = &P.m[i];
    if (!m->placed) return;
    if (m->act && P.hk && P.hk->weapon) P.hk->weapon(i, true);  // (s55: an attack / cast cut short)
    pc_bash_end(i, "blown");
    m->act = 0, m->cur = -1;
    power = pt_clampf(power, 0.1f, 3);
    float dx = m->pos[0] - from[0], dz = m->pos[2] - from[2], d = hypotf(dx, dz);
    if (d < 1e-3f) dx = -sinf(m->face), dz = -cosf(m->face), d = 1;  // (right on the source: backwards)
    bool front = sinf(m->face) * -dx + cosf(m->face) * -dz >= 0;
    // (GUESS: KH2's reaction turns him square to the hit; else a blast from the side plays the back-flying BLOW_FRONT
    // side-on to the flight) front: facing the source, back: facing away
    m->face = front ? atan2f(-dx, -dz) : atan2f(dx, dz);
    m->kd = KD_AIR, m->kd_k = front ? 0 : 1, m->kd_n++;
    m->kd_dir[0] = dx / d, m->kd_dir[1] = dz / d;
    m->kd_h = KD_SPEED * power, m->kd_hmin = m->kd_h * KD_MIN_XZ, m->kd_vy = -KD_UP * power;
    m->kd_clock = m->kd_acc = m->kd_air = m->kd_lie = 0, m->blend = -1, m->ray_due = true, m->band = PBAND_STAND;
    memcpy(m->kd_from, from, sizeof m->kd_from);
    m->kd_dist = hypotf(m->pos[0] - from[0], m->pos[2] - from[2]) / to_u, m->kd_power = power;
    PARTY_LOG("[party] %s blown (dist %.0f cm power %.2f, %s) from %.3f %.3f %.3f\n", PARTY[i].name, m->kd_dist, power,
              front ? "front" : "back", from[0], from[1], from[2]);
}
// Every placed member within radius_cm (3D) of 'from': power 1 at the source, 0.5 at the edge (GUESS).
static int party_blast_core(const float from[3], float radius_cm, float to_u) {
    int n = 0;
    for (int i = 0; i < PARTY_N; i++) {
        PMember *m = &P.m[i];
        float d = sqrtf((m->pos[0] - from[0]) * (m->pos[0] - from[0]) + (m->pos[1] - from[1]) * (m->pos[1] - from[1]) +
                        (m->pos[2] - from[2]) * (m->pos[2] - from[2])) / to_u;
        if (!m->placed || d > radius_cm) continue;
        party_knockdown_core(i, from, 1 - 0.5f * d / radius_cm, to_u), n++;
    }
    return n;
}
// The drawn slot + clock (the blend helper and the draw use it).
static int party_draw_slot(int i, float *clock) {
    const PMember *m = &P.m[i];
    if (m->kd) return *clock = m->kd_clock, PARTY[i].kd[2 * m->kd_k + (m->kd == KD_DOWN)].slot;
    if (m->act) return *clock = m->act_clock, m->act_slot;  // (s55: an attack / cast / hop)
    return *clock = m->clock, m->stance ? PARTY[i].bslot[m->motion] : PARTY[i].slot[m->motion];
}
static void party_kd_tick(int i, const PIn *in) {
    PMember *m = &P.m[i];
    const PartyDef *pd = &PARTY[i];
    if (m->kd == KD_AIR) {
        m->kd_acc += in->kh, m->kd_air += in->kh;
        while (m->kd_acc >= KD_STEP) {  // (re:0x422780, once per sub-step)
            m->kd_acc -= KD_STEP;
            m->kd_h = fmaxf(m->kd_h * KD_BRAKE_XZ, m->kd_hmin);
            float vy = m->kd_vy < 0 ? m->kd_vy * KD_BRAKE_UP : m->kd_vy;  // (KH2 y down: < 0 rising)
            m->kd_vy = fminf(vy + KD_G, KD_FALL_MAX);
            m->pos[0] += m->kd_dir[0] * m->kd_h * KD_STEP * in->to_u, m->pos[2] += m->kd_dir[1] * m->kd_h * KD_STEP * in->to_u;
            m->pos[1] -= m->kd_vy * KD_STEP * in->to_u;
        }
        float n = pd->kd[2 * m->kd_k].frames;
        m->kd_clock = n > 0 ? fmodf(m->kd_clock + in->kh, n) : 0;
        return;
    }
    const PMot *mo = &pd->kd[2 * m->kd_k + 1];  // KD_DOWN: the landing plays through
    float c0 = m->kd_clock, c1 = c0 + in->kh;
    if (P.kd_lie > 0 && m->kd_lie < P.kd_lie && c0 <= pd->lie_at && c1 > pd->lie_at) c1 = pd->lie_at, m->kd_lie += in->kh;
    float r0[3], r1[3], s = sinf(m->face), c = cosf(m->face);
    pmot_root(mo, c0, r0), pmot_root(mo, c1, r1);
    float rx = r1[0] - r0[0], rz = r1[2] - r0[2];  // (sora.c: x += rz sin + rx cos, z += rz cos - rx sin)
    m->pos[0] += (rz * s + rx * c) * in->to_u, m->pos[2] += (rz * c - rx * s) * in->to_u;
    m->kd_clock = c1;
    if (c1 >= mo->frames) {  // up again: idle (8-frame blend from the landing's last pose), following from next present
        m->kd = KD_NONE, m->from_slot = mo->slot, m->from_clock = (float)mo->frames, m->blend = 0;
        m->blend_from_blend = false, m->motion = 0, m->clock = 0, m->band = PBAND_STAND, m->blend_n = PARTY_BLEND;
        PARTY_LOG("[party] %s -> getup done at %.3f %.3f %.3f\n", pd->name, m->pos[0], m->pos[1], m->pos[2]);
    }
}
static void party_kd_land(int i) {
    PMember *m = &P.m[i];
    bool down = m->kd_vy > 0, inside = m->pos[1] < m->floor_t - 0.2f;  // (falling onto it / flew into a rise)
    if (!((down || inside) && m->pos[1] <= m->floor_t) && m->kd_air < KD_AIR_MAX) return;
    PARTY_LOG("[party] %s -> landed at %.3f %.3f %.3f after %.0f KH frames (floor %s %.3f, %.0f cm from the source)\n",
              PARTY[i].name, m->pos[0], m->floor_t, m->pos[2], m->kd_air, PARTY_SRC_NAME[m->src], m->floor_t,
              hypotf(m->pos[0] - m->kd_from[0], m->pos[2] - m->kd_from[2]) * 100);
    m->pos[1] = m->floor_s = m->floor_t, m->floor_off = 0, m->src_key = -1;
    m->kd = KD_DOWN, m->kd_clock = 0, m->kd_lie = 0;
}

// ===================================================================== (s55) COMBAT (CORE)
// KH2's friend battle AI for Donald / Goofy, in the shape of PHH's byte-exact port of it (party_kh2.py, read for the
// RULES only; recon/S55_HELPER_PARTY_COMBAT.md section 1). Once per present, for a member that is free (no attack /
// cast playing: KH2's think runs on free frames only, re:0x1B53B0):
//   target_update re:0x1B8910 - Battle Style Technic (the KH2 default): the last attacker (slice 2), else the nearest
//     enemy part by surface distance within friend_param search (800 cm); kept while within keep (1200 cm); the leash
//     (1200 cm flat from Sora) makes the target invalid until he is back at his slot;
//   the MP gate (re:0x1B53B0): rand <= mp_gate_scale x (0.25 + 0.75 x MP / max) lets entries with an MP cost run;
//   emergency re:0x1B3A30 - with someone at <= 25 % HP: Cure (Donald) on the first under 40 % of (that one, himself,
//     Sora, the other member);
//   normal re:0x1B1D40 - the sustained entry (pause / move / Goofy's run) continues while its timer runs and nothing
//     can attack; after an attack entry the hop (B row) with hop_chance; after two non-move entries in a row a
//     breather (default locomotion); else a uniform pick among the entries whose check passes (attacks / spells by
//     their A row distance + height windows re:0x1B2880, pause / move only when no attack can), never the entry that
//     was the last three (blocked); else default locomotion re:0x1B23D0 (approach the target with battle_approach
//     bands, else follow Sora's slot).
// Actions: an A-row attack plays its motion (variant 0 = battle + weapon), root motion applied, the weapon's baked hit
// windows swept against the enemies (sora.c's blade sweep, the member's own *_hits.bin); Donald's spells play their
// cast motion (Fire / Thunder / Cure 192, Blizzard 191: PHH from P_EX020.mdlx's BDX) and release at its frame trigger
// 9 (MP spent then); Goofy's abilities: Tornado 191 / Turbo 194 sweep the shield's windows, Bash 192 throws the shield
// (a projectile, GUESS speeds) and 193 catches it. Damage + kill credit + fx / se go through PHooks (sora.c glue).
#define PC_BATTLE_CM 1500.0f   // battle = a live enemy within this of Sora (PHH's stand-in for re:0x3AB690, VERIFIED PHH)
#define PC_ENEMY_MAX 64
#define PC_TURN_FRAMES 12.0f   // (GUESS) an attack turns toward its target over its first frames at TurnSpeed (Sora's
                               // A300 turn_to_lock window is 0..12; the party bakes carry no turn range)
#define PC_REHIT 30.0f         // (GUESS) a window longer than PC_LONG_WIN re-hits the same enemy after this many frames
#define PC_LONG_WIN 60
#define PC_AIR_MAX 120.0f      // KH frames: a hop that never finds the floor lands where it is
#define PC_BASH_SPEED 30.0f    // (GUESS) Goofy Bash: the shield's speed, cm / frame; out to the target (<= 750 cm), back
#define PC_BASH_R 70.0f        // (GUESS) its hit radius, cm
#define PC_BASH_MAX 240.0f     // (GUESS) KH frames before the catch at the latest
// (s58) KH2's Bash act (P_EX030 BDX, the act at w3420 / file 0x1AC8, its thread at w3506): motion_start(192, 8), the
// thread flies the weapon object (W_EX030 playing its motion 201 = slot 804, a 20-frame spin loop flat about its y)
// out for at most 180 frames (its timer, 180.0 at w3586), then motion_start(193, 8): the catch, whose weapon motion
// A402 (slot 772) flies the shield in from 1.5 m ahead of him into his hand in 6 frames. Spots in his model frame
// (KH cm, +x his left, +y up, +z forward) of the shield OBJECT (its spin orbits 10.6 cm above it, offline bake
// probe, recon/S58_HELPER_GOOFYSHIELD.md): in his hand at the release (768 f80; shield centre -41.6 75.3 49.6) and
// where the catch's frame 0 has it (772 f0, centre 76.3 65.4 151.7).
#define PC_BASH_OUT_MAX 180.0f
#define PC_BASH_CATCH_BLEND 8.0f
static const float PC_BASH_HAND[3] = {-41.6f, 64.7f, 49.6f};
static const float PC_BASH_CATCH[3] = {76.3f, 54.8f, 151.7f};
#define PC_TURBO_SPEED 16.0f   // (GUESS) Goofy Turbo's dash, cm / frame, inside its hit window
#define PC_TORNADO_SPEED 4.0f  // (GUESS) Goofy Tornado's drift toward the target, cm / frame, inside its hit window
#define F54_PARTY 0x0f         // BotW small damage (DAMAGE_ARGS_RECON Q2), every party melee window
#define F54_PARTY_BIG 0x11     // (GUESS) medium for Goofy Turbo / Bash (atkp 1162 / Bash: Power 100)
enum { PH_PAUSE, PH_MOVE, PH_RUN, PH_ATTACK, PH_SPELL, PH_CURE };
enum { PS_NONE = -1, PS_FIRE, PS_BLIZZARD, PS_THUNDER, PS_CURE, PS_TORNADO, PS_BASH, PS_TURBO };
enum { PI_LOCO, PI_PAUSE, PI_RUN, PI_NONE };  // what the member's feet do after the think
#define EF_MOVE 0x1
#define EF_ATTACK 0x2
#define EF_HP 0x10
#define EF_SUSTAINED 0x200
// An action table entry (party_ai.kh.json "actions" + its A row; VERIFIED rows). dist = surface distance to the target
// part (cm), h = its centre height above his feet (cm; KH2 keeps it y-down, the test flips the sign).
typedef struct {
    const char *name;
    uint16_t flags;
    uint8_t h;
    int8_t spell;
    int16_t row, motion;  // A row id; the motion it plays (attacks: the A row's; spells: the act's cast motion)
    int cost;
    float dmin, dmax, hmin, hmax, cure_t;
    bool jump;
} PEntry;
static const PEntry PENT_DONALD[] = {  // re:0x724D40 (handlers re:0x1CCBA0 / 0x1CCDF0 / 0x1CD0A0 / 0x1CC650 / 0x1CC1A0)
    {"pause", EF_MOVE | EF_SUSTAINED, PH_PAUSE, PS_NONE, -1, 0, 0, 0, 0, 0, 0, 0, false},
    {"move", EF_MOVE | EF_SUSTAINED, PH_MOVE, PS_NONE, -1, 0, 0, 0, 0, 0, 0, 0, false},
    {"attack1", EF_ATTACK, PH_ATTACK, PS_NONE, 0, 151, 0, 0, 200, -100, 150, 0, false},     // re:0x71D930 A300
    {"attack2", EF_ATTACK, PH_ATTACK, PS_NONE, 1, 152, 0, 0, 180, -100, 150, 0, false},     // re:0x71D9B0 A301
    {"air1", EF_ATTACK, PH_ATTACK, PS_NONE, 2, 181, 0, 0, 150, 100, 385, 0, true},          // re:0x71DA30 A330
    {"air2", EF_ATTACK, PH_ATTACK, PS_NONE, 3, 182, 0, 0, 130, 100, 385, 0, true},          // re:0x71DAB0 A331
    {"fire", EF_ATTACK, PH_SPELL, PS_FIRE, 57, 192, 10, 0, 350, -100, 200, 0, false},       // re:0x71DB30
    {"blizzard", EF_ATTACK, PH_SPELL, PS_BLIZZARD, 58, 191, 10, 250, 750, -100, 150, 0, false},
    {"thunder", EF_ATTACK, PH_SPELL, PS_THUNDER, 59, 192, 10, 0, 500, -500, 1000, 0, false},
    {"cure", EF_HP, PH_CURE, PS_CURE, 60, 192, 255, 0, 0, 0, 0, 0.4f, false},               // re:0x71DCB0 (A+0x3C 0.4)
};
static const PEntry PENT_GOOFY[] = {  // re:0x7253B0 (+ his run re:0x1CE8F0, Bash re:0x1CE8E0)
    {"pause", EF_MOVE | EF_SUSTAINED, PH_PAUSE, PS_NONE, -1, 0, 0, 0, 0, 0, 0, 0, false},
    {"move", EF_MOVE | EF_SUSTAINED, PH_MOVE, PS_NONE, -1, 0, 0, 0, 0, 0, 0, 0, false},
    {"run", EF_MOVE | EF_SUSTAINED, PH_RUN, PS_NONE, -1, 0, 0, 0, 0, 0, 0, 0, false},
    {"attack1", EF_ATTACK, PH_ATTACK, PS_NONE, 4, 151, 0, 0, 220, -100, 150, 0, false},     // re:0x71DE30
    {"attack2", EF_ATTACK, PH_ATTACK, PS_NONE, 5, 152, 0, 0, 300, -100, 150, 0, false},     // re:0x71DEB0
    {"air1", EF_ATTACK, PH_ATTACK, PS_NONE, 6, 181, 0, 0, 100, 100, 385, 0, true},
    {"air2", EF_ATTACK, PH_ATTACK, PS_NONE, 7, 182, 0, 0, 80, 100, 385, 0, true},
    {"tornado", EF_ATTACK, PH_SPELL, PS_TORNADO, 92, 191, 10, 0, 500, -100, 500, 0, false},  // re:0x71E030
    {"bash", EF_ATTACK, PH_SPELL, PS_BASH, 93, 192, 10, 250, 750, -500, 1000, 0, false},     // ("blade") re:0x71E0B0
    {"turbo", EF_ATTACK, PH_SPELL, PS_TURBO, 94, 194, 10, 0, 500, -100, 150, 0, false},      // re:0x71E130
};
static const PEntry *const PENT[PARTY_N] = {PENT_DONALD, PENT_GOOFY};
static const int PENT_N[PARTY_N] = {(int)(sizeof PENT_DONALD / sizeof PENT_DONALD[0]),
                                    (int)(sizeof PENT_GOOFY / sizeof PENT_GOOFY[0])};
static const float PC_APPROACH[3][2] = {{0, 0}, {160, 240}, {200, 1000}};  // battle_approach (re:0x71D850 +0x40)
static const float PC_RUNBANDS[3][2] = {{-1, -1}, {-1, -1}, {0, 1e9f}};     // (Goofy's run: always the run band)
static int pc_find_entry(int i, const char *name) {
    for (int k = 0; k < PENT_N[i]; k++)
        if (!strcmp(PENT[i][k].name, name)) return k;
    return -1;
}

// ---- KH2's rand (MSVC LCG; rand_n re:0x19BE90, rand_range re:0x19BF50)
static int pc_rand(void) { return P.rnd = P.rnd * 214013u + 2531011u, (int)((P.rnd >> 16) & 0x7fff); }
static int pc_rand_n(int n) { return (int)((float)pc_rand() * (float)n * (1.0f / 32768.0f)); }
static float pc_rand_range(float lo, float hi) { return (float)pc_rand() * (hi - lo) * (1.0f / 32768.0f) + lo; }

// ---- <name>_hits.bin ('SKRH', partybake hitbake; sora.c hits_load's layout): per slot the weapon's hit windows and
// its grip / tip per frame in the root-stripped entity frame (KH cm). Kept in memory (never freed, ~10-45 KB).
typedef struct { uint16_t start, end, atkp, group, power, kind; uint8_t reaction, effect, pad[2]; } PHWin;
typedef struct { uint16_t slot, nwin, nseg; const PHWin *win; const float *seg; } PHSlot;
#define PHIT_MAX 24
static PHSlot PHIT[PARTY_N][PHIT_MAX];
static int PHIT_N[PARTY_N];
static int party_hits_load(int i, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = n > 16 ? (uint8_t *)malloc((size_t)n) : NULL;
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n || memcmp(b, "SKRH", 4)) {
        fclose(f), free(b);
        return 0;
    }
    fclose(f);
    uint32_t count;
    memcpy(&count, b + 8, 4);
    PHIT_N[i] = 0;
    for (uint32_t k = 0; k < count && PHIT_N[i] < PHIT_MAX && 16 + (k + 1) * 16 <= (uint32_t)n; k++) {
        const uint8_t *d = b + 16 + k * 16;
        PHSlot h;
        uint32_t off[2];
        memcpy(&h.slot, d, 6), memcpy(off, d + 8, 8);
        if (!h.nseg || off[0] + h.nwin * 16u > (uint32_t)n || off[1] + h.nseg * 24u > (uint32_t)n) continue;
        h.win = (const PHWin *)(b + off[0]), h.seg = (const float *)(b + off[1]);
        PHIT[i][PHIT_N[i]++] = h;
    }
    PARTY_LOG("[party] %s: hits %s: %d slots\n", PARTY[i].name, path, PHIT_N[i]);
    return PHIT_N[i];
}
static const PHSlot *phit_find(int i, int slot) {
    for (int k = 0; k < PHIT_N[i]; k++)
        if (PHIT[i][k].slot == slot) return &PHIT[i][k];
    return NULL;
}
// the weapon (grip, tip) at motion time t -> world (u): feet + RotY(face) x to_u (sora.c blade_world)
static void phit_seg(const PHSlot *h, float t, const float feet[3], float face, float to_u, float out[6]) {
    t = pt_clampf(t, 0, (float)(h->nseg - 1));
    int f = (int)t;
    if (f >= h->nseg - 1) f = h->nseg > 1 ? h->nseg - 2 : 0;
    float u = h->nseg > 1 ? t - (float)f : 0, s = sinf(face), c = cosf(face);
    for (int k = 0; k < 2; k++) {
        const float *a = h->seg + f * 6 + k * 3, *b = h->nseg > 1 ? a + 6 : a;
        float q[3] = {a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u, a[2] + (b[2] - a[2]) * u};
        out[k * 3 + 0] = feet[0] + (c * q[0] + s * q[2]) * to_u;
        out[k * 3 + 1] = feet[1] + q[1] * to_u;
        out[k * 3 + 2] = feet[2] + (-s * q[0] + c * q[2]) * to_u;
    }
}
// the swept quad a -> b (5 x 4 samples, sora.c sweep_hits) against an upright cylinder at feet e (radius r, height h)
static bool phit_sweep(const float a[6], const float b[6], const float e[3], float r, float h, float pad, float at[3]) {
    float rr = (r + pad) * (r + pad);
    for (int ti = 0; ti < 4; ti++)
        for (int ui = 0; ui < 5; ui++) {
            float t = (float)ti / 3, u = (float)ui / 4, p[3];
            for (int c = 0; c < 3; c++) {
                float pa = a[c] + (a[3 + c] - a[c]) * u, pb = b[c] + (b[3 + c] - b[c]) * u;
                p[c] = pa + (pb - pa) * t;
            }
            float dx = p[0] - e[0], dz = p[2] - e[2];
            if (dx * dx + dz * dz <= rr && p[1] >= e[1] - pad && p[1] <= e[1] + h + pad) {
                memcpy(at, p, sizeof(float) * 3);
                return true;
            }
        }
    return false;
}

// ---- geometry: the target's part = an ellipsoid (radius r, half-height hh) at the enemy cylinder's centre (one part
// per enemy, PHH A25), KH2's surface distance re:0x3C99F0 type 0
static float pc_r_cm(const PIn *in) { return (in->hit_r > 0 ? in->hit_r : 0.5f) / in->to_u; }
static float pc_hh_cm(const PIn *in) { return (in->hit_h > 0 ? in->hit_h : 1.6f) * 0.5f / in->to_u; }
static void pc_geo(int i, const PIn *in, const PEnemy *e) {
    PMember *m = &P.m[i];
    float hh = pc_hh_cm(in), r = pc_r_cm(in);
    m->t_ctr[0] = e->pos[0], m->t_ctr[1] = e->pos[1] + hh * in->to_u, m->t_ctr[2] = e->pos[2];
    float v[3] = {(m->t_ctr[0] - m->pos[0]) / in->to_u, (m->t_ctr[1] - m->pos[1]) / in->to_u,
                  (m->t_ctr[2] - m->pos[2]) / in->to_u};
    float flat = hypotf(v[0], v[2]), len = sqrtf(flat * flat + v[1] * v[1]);
    m->t_dy = -v[1], m->t_flat = flat;  // (KH2 y down: a centre above his feet is dy < 0)
    m->t_dir[0] = flat > 1e-4f ? v[0] / flat : sinf(m->face), m->t_dir[1] = flat > 1e-4f ? v[2] / flat : cosf(m->face);
    float w[3] = {v[0], v[1] * r / hh, v[2]}, n = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]), d = 0;
    if (n > 1e-4f) {
        float k = (n - r) / n;
        w[0] *= k, w[1] *= k * hh / r, w[2] *= k;
        d = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
    }
    m->t_d = d, m->t_r = len - d;  // (+0xE60 surface distance, +0xE74 = centre distance - it)
}
static float pc_face_to(const PMember *m) { return atan2f(m->t_dir[0], m->t_dir[1]); }
static bool pc_alive(const PIn *in, int k) { return k >= 0 && k < in->nen && in->en[k].life > 0; }
static bool pc_target_valid(int i, const PIn *in) { return !P.m[i].leash && pc_alive(in, P.m[i].tgt); }  // re:0x1B4D20

// re:0x1B8910 with Battle Style Technic (keep re:0x1B3520, choose re:0x1B6B80). No attacker yet (enemies cannot hit
// them in slice 1): the nearest part within search.
static void pc_target_update(int i, const PIn *in) {
    PMember *m = &P.m[i];
    const PartyDef *pd = &PARTY[i];
    m->tgt_changed = false;
    if (hypotf(m->pos[0] - in->feet[0], m->pos[2] - in->feet[2]) / in->to_u > pd->leash) m->leash = true;
    int cur = -1;
    for (int k = 0; k < in->nen && cur < 0; k++)
        if (m->tgt_actor && in->en[k].actor == m->tgt_actor && in->en[k].life > 0) cur = k;
    if (!P.battle) {
        m->tgt_changed = m->tgt_actor != 0, m->tgt = -1, m->tgt_actor = 0;
        return;
    }
    bool choose = true;
    if (!m->leash && cur >= 0) pc_geo(i, in, &in->en[cur]), choose = m->t_d + m->t_r > pd->keep;
    if (choose) {
        uint64_t old = m->tgt_actor;
        int best = -1;
        float bd = pd->search;
        for (int k = 0; k < in->nen; k++) {
            if (in->en[k].life <= 0) continue;
            pc_geo(i, in, &in->en[k]);
            if (m->t_d < bd) bd = m->t_d, best = k;
        }
        m->tgt = best, m->tgt_actor = best >= 0 ? in->en[best].actor : 0;
        m->tgt_changed = m->tgt_actor != old;
    } else
        m->tgt = cur;
    if (m->tgt >= 0) pc_geo(i, in, &in->en[m->tgt]);
}

// ---- HP rules (re:0x3C1D30 low = HP <= max / 4, re:0x1B3310 below = HP < (int)(max x t)); who: 0 Sora, 1 + member
static bool pc_low(float hp, float mx) { return mx > 0 && hp <= (float)((int)mx / 4); }
static bool pc_below(float hp, float mx, float t) { return mx > 0 && hp < (float)(int)(mx * t); }
static float pc_hp_of(const PIn *in, int who, float *mx) {
    if (who == 0) return *mx = in->sora_hp_max, in->sora_hp;
    return *mx = P.m[who - 1].hp_max, P.m[who - 1].hp;
}
static int pc_heal_target(int i, const PIn *in) {  // re:0x1B53B0: the <= 25 % one: Sora, else me, else the other
    float mx, hp = pc_hp_of(in, 0, &mx);
    if (pc_low(hp, mx)) return 0;
    hp = pc_hp_of(in, 1 + i, &mx);
    if (pc_low(hp, mx)) return 1 + i;
    hp = pc_hp_of(in, 2 - i, &mx);
    return pc_low(hp, mx) ? 2 - i : -1;
}
static int pc_cure_who(int i, const PIn *in, float t) {  // re:0x1CC1A0: (heal target, me, Sora, the other) under t
    int order[4] = {pc_heal_target(i, in), 1 + i, 0, 2 - i};
    for (int k = 0; k < 4; k++) {
        float mx, hp;
        if (order[k] < 0) continue;
        hp = pc_hp_of(in, order[k], &mx);
        if (pc_below(hp, mx, t)) return order[k];
    }
    return -1;
}

// ---- the checks (re:0x1B2880 eligible + the handlers' check halves)
static bool pc_eligible(int i, const PIn *in, const PEntry *e) {
    PMember *m = &P.m[i];
    if (m->act || m->kd) return false;                                 // (idle + grounded)
    if (e->jump && !P.air_rows) return false;                          // (slice 1: no jump arc for the air rows)
    if (e->cost >= 1 && !(m->gate && m->charge <= 0)) return false;    // (the MP gate, MP Charge)
    if (!(e->flags & EF_ATTACK)) return true;
    if (!pc_target_valid(i, in)) return false;
    float d = m->t_d, r = m->t_r, dy = m->t_dy;
    if (!(d <= r + e->dmax && e->dmin - r <= d)) return false;        // c1 (dist window)
    if (!(-e->hmax - r <= dy && dy <= r - e->hmin)) return false;     // c2 (height window, y down)
    return sinf(m->face) * m->t_dir[0] + cosf(m->face) * m->t_dir[1] >= 0;  // in front (A row flag bit 27 clear)
}
static int pc_blocked(int i) {  // the entry the last three non-move entries were
    const PMember *m = &P.m[i];
    return m->hist[0] >= 0 && m->hist[0] == m->hist[1] && m->hist[0] == m->hist[2] ? m->hist[0] : -1;
}
static bool pc_attack_possible(int i, const PIn *in) {  // re:0x1B2B20
    int blocked = pc_blocked(i);
    for (int k = 0; k < PENT_N[i]; k++)
        if ((PENT[i][k].flags & EF_ATTACK) && k != blocked && pc_eligible(i, in, &PENT[i][k])) return true;
    return false;
}
static bool pc_check(int i, const PIn *in, int k, bool sustain) {
    PMember *m = &P.m[i];
    const PEntry *e = &PENT[i][k];
    switch (e->h) {
    case PH_PAUSE:  // re:0x1CCBA0
        if (!sustain) return m->cur != k && !pc_attack_possible(i, in);
        return !m->tgt_changed && m->sus_t > 0 && !pc_attack_possible(i, in);
    case PH_MOVE:  // re:0x1CCDF0
        if (sustain) return !m->tgt_changed && m->sus_t > 0 && !pc_attack_possible(i, in);
        return !pc_attack_possible(i, in);
    case PH_RUN:  // re:0x1CE8F0
        if (sustain && (m->tgt_changed || m->sus_t <= 0)) return false;
        return pc_target_valid(i, in);
    case PH_ATTACK:  // re:0x1CD0A0
    case PH_SPELL:   // re:0x1CC650 (+ "its act script present and idle": always here)
        if (e->spell == PS_BASH && pc_bash_out(i)) return false;  // (s58: one shield; KH2's act waits for its thread)
        return pc_eligible(i, in, e);
    case PH_CURE:  // re:0x1CC1A0
        return pc_eligible(i, in, e) && pc_cure_who(i, in, e->cure_t) >= 0;
    }
    return false;
}

// ---- actions
static const PMotX *pc_motion(int i, int id, int *slot) {  // battle + weapon order (0, 1, 3, 2): PHH re:0x5C5D38
    static const int var[4] = {0, 1, 3, 2};
    for (int k = 0; k < 4; k++) {
        const PMotX *mo = pmot_find(i, id * 4 + var[k]);
        if (mo) return *slot = mo->slot, mo;
    }
    return *slot = -1, NULL;
}
static void pc_drawn(int i, int *slot, float *clock);
static void pc_snd(const PIn *in, int i, int ev, int arg, const float at[3]) {  // (s56: PHooks.sound, NULL-safe)
    if (in->hk && in->hk->sound) in->hk->sound(i, ev, arg, at);
}
static void pc_mp_spend(int i, int cost) {  // (KH2: spent when the spell fires; at 0 MP Charge starts)
    PMember *m = &P.m[i];
    m->mp -= (float)cost;
    if (m->mp <= 0) m->mp = 0, m->charge = PARTY[i].mp_charge;
}
static bool pc_start(int i, const PIn *in, int k) {
    PMember *m = &P.m[i];
    const PEntry *e = &PENT[i][k];
    int slot;
    const PMotX *mo = pc_motion(i, e->motion, &slot);
    if (!mo) {
        PARTY_LOG("[party] %s: %s: motion %d not in the bake: skipped\n", PARTY[i].name, e->name, e->motion);
        return false;
    }
    int ds;
    float dc;
    pc_drawn(i, &ds, &dc);
    // blends: Donald's casts 8, Goofy Turbo 0 (PHH, from the acts' motion requests: VERIFIED); attacks a cut (GUESS)
    party_blend_from(i, ds, dc, e->h == PH_ATTACK || e->spell == PS_TURBO ? 0 : 8);
    m->act = e->h == PH_ATTACK ? PACT_ATTACK : PACT_CAST, m->act_entry = k, m->act_slot = slot, m->act_spell = e->spell;
    m->act_clock = 0, m->act_frames = mo->frames, m->act_loop = mo->loop, m->act_phase = 0, m->act_fired = false;
    m->act_release = pmot_trigger(mo, 9, e->motion == 191 ? 20.0f : 26.0f);  // (Donald 764 f20 / 768 f26, Goofy 768 f80)
    m->seg_ok = false, m->act_win = -1, m->act_nhit = 0;
    bool tv = pc_target_valid(i, in);
    m->act_target = tv ? m->tgt_actor : 0;
    if (tv) memcpy(m->act_tpos, m->t_ctr, sizeof m->act_tpos);
    else {
        float ahead = 300 * in->to_u;
        m->act_tpos[0] = m->pos[0] + sinf(m->face) * ahead, m->act_tpos[1] = m->pos[1], m->act_tpos[2] = m->pos[2] + cosf(m->face) * ahead;
    }
    if (e->spell == PS_TORNADO || e->spell == PS_TURBO) pc_mp_spend(i, e->cost);  // (no release trigger: at the start)
    // (s56) a spell act requests its voice before its motion (P_EX020 BDX trap_obj_voice; Goofy's acts request none):
    // Cure by its target (btlobj_target at the act's start: himself -> vtbl 21, else vtbl 3 = the partner's name)
    if (e->h == PH_CURE) {
        int who = pc_cure_who(i, in, e->cure_t);
        pc_snd(in, i, PSND_CURE_VOICE, who < 0 ? 0 : who, m->pos);
    } else if (e->h == PH_SPELL)
        pc_snd(in, i, PSND_ACT, e->spell, m->pos);
    if (P.clog)
        PARTY_LOG("[party] %s -> %s (motion %d slot %d, %u frames) at %llx d=%.0f cm h=%.0f cm, mp %.0f%s\n",
                  PARTY[i].name, e->name, e->motion, slot, mo->frames, (unsigned long long)m->act_target,
                  tv ? m->t_d : -1.0f, tv ? -m->t_dy : 0.0f, m->mp, m->charge > 0 ? " (charging)" : "");
    return true;
}
static void pc_hop(int i, const PIn *in, int b) {  // re:0x1B1D40's hop: dir = facing + B.angle, jump_start re:0x3CBEC0
    PMember *m = &P.m[i];
    const PartyDef *pd = &PARTY[i];
    int slot;
    const PMotX *mo = pc_motion(i, 3, &slot);  // (HOP_MOTION 3 = jump)
    float a = m->face + pd->hop[b].angle * 3.14159265f / 180.0f, fr = pd->hop[b].frames;
    int ds;
    float dc;
    pc_drawn(i, &ds, &dc);
    party_blend_from(i, ds, dc, 4);
    m->act = PACT_HOP, m->act_entry = -1, m->act_spell = PS_NONE, m->act_slot = mo ? slot : party_loco_slot(i, 0);
    m->act_clock = 0;
    m->act_frames = mo ? mo->frames : 44, m->act_loop = mo ? mo->loop : 0;
    m->hop_g = 2 * pd->hop[b].up / (fr * fr);
    m->hop_v[0] = sinf(a) * pd->hop[b].speed, m->hop_v[2] = cosf(a) * pd->hop[b].speed, m->hop_v[1] = m->hop_g * fr;
    m->hop_brake = pd->hop[b].brake, m->kd_air = 0, m->ray_due = true;
    if (P.clog) PARTY_LOG("[party] %s -> hop (B row %d: %.0f deg, up %.0f cm)\n", pd->name, b, pd->hop[b].angle, pd->hop[b].up);
    (void)in;
}
static void pc_pick(int i, const PIn *in, int k) {  // _run_pick: run the entry, keep the history
    PMember *m = &P.m[i];
    const PEntry *e = &PENT[i][k];
    switch (e->h) {
    case PH_PAUSE: m->sus_t = pc_rand_range(PARTY[i].pause_min, PARTY[i].pause_max), m->intent = PI_PAUSE; break;
    case PH_MOVE: m->sus_t = pc_rand_range(PARTY[i].move_min, PARTY[i].move_max), m->intent = PI_LOCO; break;
    case PH_RUN:
        m->sus_t = pc_rand_range(PARTY[i].pause_min, PARTY[i].pause_max), m->intent = PI_RUN;
        m->run_dir[0] = m->t_dir[0], m->run_dir[1] = m->t_dir[1];  // (the locomotion direction: toward the target)
        break;
    default: m->intent = pc_start(i, in, k) ? PI_NONE : PI_LOCO; break;
    }
    if (!(e->flags & EF_MOVE)) m->hist[2] = m->hist[1], m->hist[1] = m->hist[0], m->hist[0] = k, m->nonmove++;
    else m->nonmove = 0;
    m->cur = k;
}
static bool pc_emergency(int i, const PIn *in) {  // re:0x1B3A30
    PMember *m = &P.m[i];
    if (pc_heal_target(i, in) < 0 || m->act) return false;
    m->gate = true;  // (E18_MP_GATE stays set for the rest of this think, as in KH2)
    int cur = m->cur;
    m->hist[0] = m->hist[1] = m->hist[2] = m->cur = -1, m->nonmove = 0;
    int cands[16], nc = 0;
    for (int k = 0; k < PENT_N[i]; k++)
        if ((PENT[i][k].flags & EF_HP) && (PENT[i][k].cost < 1 || m->charge <= 0) && pc_check(i, in, k, false) && nc < 16)
            cands[nc++] = k;
    if (nc) { pc_pick(i, in, cands[pc_rand_n(nc)]); return true; }
    m->cur = cur;
    return false;
}
static void pc_normal(int i, const PIn *in) {  // re:0x1B1D40
    PMember *m = &P.m[i];
    const PartyDef *pd = &PARTY[i];
    if (pc_emergency(i, in) || m->act) return;
    if (m->cur >= 0 && (PENT[i][m->cur].flags & EF_SUSTAINED) && pc_check(i, in, m->cur, true)) {
        m->intent = PENT[i][m->cur].h == PH_PAUSE ? PI_PAUSE : PENT[i][m->cur].h == PH_RUN ? PI_RUN : PI_LOCO;
        return;
    }
    if (m->cur >= 0 && (PENT[i][m->cur].flags & EF_ATTACK) && pc_target_valid(i, in)) {
        float x = (float)pc_rand_n(20001) * 5e-5f;
        if (x < pd->hop_chance && pd->nhop > 0) {
            pc_hop(i, in, pc_rand_n(pd->nhop));
            m->nonmove = 0, m->cur = -1, m->intent = PI_NONE;
            return;
        }
    }
    if (m->nonmove >= 2) { m->intent = PI_LOCO, m->nonmove = 0, m->cur = -1; return; }  // (the breather)
    int blocked = pc_blocked(i), cands[16], nc = 0;
    for (int k = 0; k < PENT_N[i]; k++)
        if (pc_check(i, in, k, false) && nc < 16) cands[nc++] = k;
    if (nc) {
        int k = cands[pc_rand_n(nc)];
        if (k != blocked) { pc_pick(i, in, k); return; }
    }
    m->intent = PI_LOCO, m->nonmove = 0, m->cur = -1;
}
static void pc_think(int i, const PIn *in) {  // re:0x1B53B0 (a free frame)
    PMember *m = &P.m[i];
    const PartyDef *pd = &PARTY[i];
    if (m->sus_t > 0) m->sus_t -= in->kh;
    pc_target_update(i, in);
    if (m->forced >= 0) {  // (live "party cast <name>": that entry now, checks skipped)
        int k = m->forced;
        m->forced = -1, m->gate = true;
        pc_pick(i, in, k);
        return;
    }
    if (!P.battle) {
        m->intent = PI_LOCO, m->cur = -1;
        return;
    }
    float p = m->mp / pd->mp_max * 0.75f + 0.25f;
    m->gate = (float)pc_rand_n(20001) * 5e-5f <= pd->mp_gate_scale * p;
    if (!pc_emergency(i, in)) pc_normal(i, in);
}
// the feet after the think: re:0x1B23D0 default locomotion (approach a valid target, else follow Sora), the pause
// (stand facing the target), Goofy's run (straight on at run speed)
static void pc_feet(int i, const PIn *in) {
    PMember *m = &P.m[i];
    if (m->intent == PI_RUN) {
        float ahead[3] = {m->pos[0] + m->run_dir[0] * 100, m->pos[1], m->pos[2] + m->run_dir[1] * 100};
        m->band = PBAND_RUN;
        party_steer(i, in, ahead, PC_RUNBANDS, 0, m->face);
        return;
    }
    if (m->intent == PI_PAUSE) {
        party_steer(i, in, m->pos, PARTY_BANDS, 0, pc_target_valid(i, in) ? pc_face_to(m) : m->face);
        return;
    }
    if (pc_target_valid(i, in)) {
        party_steer(i, in, m->t_ctr, PC_APPROACH, pc_r_cm(in) + P.stop_cm, pc_face_to(m));
        return;
    }
    party_move(i, in);
    if (m->band == PBAND_STAND) m->leash = false;  // (re:0x1B41A0: back at his slot)
}

// ---- hits: one per (action, window, enemy); a long window re-hits after PC_REHIT frames (GUESS)
static int pc_hit_slot_of(PMember *m, uint64_t a) {
    for (int k = 0; k < m->act_nhit; k++)
        if (m->act_hit[k] == a) return k;
    return -1;
}
static void pc_damage(int i, const PIn *in, const PEnemy *e, int power, int magic, int f54, int f40, const float at[3],
                      const char *fx, int atkp, const char *what) {
    int dmg = in->hk && in->hk->damage ? in->hk->damage(i, e->actor, e->life, power, magic) : 1;
    if (in->hk && in->hk->hit) in->hk->hit(i, e->actor, dmg, f54, f40, at, fx, atkp);
    P.m[i].hits++;
    if (P.clog)
        PARTY_LOG("[party] %s HIT %llx (life %d) by %s power %d: damage %d f54 %#x f40 %d at %.2f %.2f %.2f\n",
                  PARTY[i].name, (unsigned long long)e->actor, e->life, what, power, dmg, f54, f40, at[0], at[1], at[2]);
}
static void pc_sweep(int i, const PIn *in, float c0, float c1) {
    PMember *m = &P.m[i];
    const PHSlot *h = phit_find(i, m->act_slot);
    if (!h) return;
    float now[6], prev[6];
    phit_seg(h, c1, m->pos, m->face, in->to_u, now);
    if (m->seg_ok) memcpy(prev, m->seg, sizeof prev);
    else phit_seg(h, c0, m->pos, m->face, in->to_u, prev);
    memcpy(m->seg, now, sizeof now), m->seg_ok = true;
    for (int k = 0; k < m->act_nhit; k++) m->act_hit_t[k] += in->kh;
    for (int w = 0; w < h->nwin; w++) {
        const PHWin *win = &h->win[w];
        if (!win->power || c1 < win->start || c0 > win->end) continue;  // (power 0: Goofy's guard windows)
        if (m->act_win != w) m->act_win = w, m->act_nhit = 0;
        bool long_win = win->end - win->start > PC_LONG_WIN;
        for (int k = 0; k < in->nen; k++) {
            const PEnemy *e = &in->en[k];
            if (e->life <= 0) continue;
            int hs = pc_hit_slot_of(m, e->actor);
            if (hs >= 0 && !(long_win && m->act_hit_t[hs] >= PC_REHIT)) continue;
            float at[3];
            if (!phit_sweep(prev, now, e->pos, in->hit_r > 0 ? in->hit_r : 0.5f, in->hit_h > 0 ? in->hit_h : 1.6f, 0.3f, at))
                continue;
            if (hs < 0 && m->act_nhit < 16) hs = m->act_nhit++, m->act_hit[hs] = e->actor;
            if (hs >= 0) m->act_hit_t[hs] = 0;
            int f54 = m->act_spell == PS_TURBO ? F54_PARTY_BIG : F54_PARTY;
            char what[48];
            snprintf(what, sizeof what, "%s window %d (atkp %d)", PENT[i][m->act_entry].name, w, win->atkp);
            pc_damage(i, in, e, win->power, 0, f54, 0, at, f54 > F54_PARTY ? "hit_finish" : "hit", win->atkp, what);
        }
    }
}

// ---- party spells (Donald's Fire / Blizzard / Thunder / Cure, Goofy's thrown shield): KH2 Lv1 numbers as sora.c's
// spell objects (magi rows: Fire r 200 h 80, 44 frames, ticks 0 / 18 / 36, last Power 150; Blizzard 40 cm/frame, 30
// frames, r 80, Power 250; Thunder after 6 frames r 170 Power 250; Cure). Caster = the member.
typedef struct {
    bool on;
    int kind, member, nhit, fx, leg;
    float t, pos[3], dir[2], travel, reach;
    float wyaw;               // (s58 Bash: the shield's flight direction, its draw yaw)
    uint64_t target, hit[16];
} PSpell;
#define PSP_N 6
static PSpell PSP[PSP_N];
static bool psp_hit_before(const PSpell *o, uint64_t a) {
    for (int k = 0; k < o->nhit; k++)
        if (o->hit[k] == a) return true;
    return false;
}
static int pc_fx(const PIn *in, const char *name, const float pos[3], float yaw) {
    return in->hk && in->hk->fx ? in->hk->fx(name, pos, yaw) : -1;
}
// (s58) a spot of his model frame (KH cm: +x his left, +y up, +z forward) -> world (the draw's placement rule)
static void pc_bash_spot(const PMember *m, const float k[3], float to_u, float out[3]) {
    float s = sinf(m->face), c = cosf(m->face);
    out[0] = m->pos[0] + (c * k[0] + s * k[2]) * to_u;
    out[1] = m->pos[1] + k[1] * to_u;
    out[2] = m->pos[2] + (-s * k[0] + c * k[2]) * to_u;
}
static void pc_spell_spawn(int i, const PIn *in, int kind) {
    PMember *m = &P.m[i];
    PSpell *o = NULL;
    for (int k = 0; k < PSP_N && !o; k++)
        if (!PSP[k].on) o = &PSP[k];
    if (!o) o = &PSP[0];
    memset(o, 0, sizeof *o);
    o->on = true, o->kind = kind, o->member = i, o->target = m->act_target, o->fx = -1;
    memcpy(o->pos, m->pos, sizeof o->pos);
    float dx = m->act_tpos[0] - m->pos[0], dz = m->act_tpos[2] - m->pos[2], d = hypotf(dx, dz);
    o->dir[0] = d > 1e-4f ? dx / d : sinf(m->face), o->dir[1] = d > 1e-4f ? dz / d : cosf(m->face);
    float up[3] = {m->pos[0], m->pos[1] + 100 * in->to_u, m->pos[2]}, yaw = atan2f(o->dir[0], o->dir[1]);
    if (kind == PS_FIRE) o->fx = pc_fx(in, "fire_ring", m->pos, m->face);
    else if (kind == PS_BLIZZARD) o->fx = pc_fx(in, "blizzard_shard", up, yaw);
    else if (kind == PS_BASH) {  // (s58: from his hand, as the release frame 768 f80 holds it, aimed from there)
        pc_bash_spot(m, PC_BASH_HAND, in->to_u, o->pos);
        float hx = m->act_tpos[0] - o->pos[0], hz = m->act_tpos[2] - o->pos[2], hd = hypotf(hx, hz);
        if (d > 1e-4f && hd > 1e-4f) o->dir[0] = hx / hd, o->dir[1] = hz / hd, d = hd;
        o->reach = fminf(fmaxf(d / in->to_u - pc_r_cm(in), 100), 750);
        o->wyaw = atan2f(o->dir[0], o->dir[1]);
        if (in->hk && in->hk->weapon) in->hk->weapon(i, false);
    }
    pc_snd(in, i, PSND_RELEASE, kind, m->pos);  // (s56: the act's own effect elements start here: their SEs)
    if (P.clog)
        PARTY_LOG("[party] %s: %s released at %.2f %.2f %.2f (target %llx), mp %.0f\n", PARTY[i].name,
                  kind == PS_BASH ? "shield" : PENT[i][m->act_entry].name, m->pos[0], m->pos[1], m->pos[2],
                  (unsigned long long)o->target, m->mp);
}
static void pc_cure(int i, const PIn *in) {
    PMember *m = &P.m[i];
    int who = pc_cure_who(i, in, PENT[i][m->act_entry].cure_t);
    if (who < 0) who = 0;  // (forced from the live line: Sora)
    pc_fx(in, "cure_ring", m->pos, m->face);
    pc_snd(in, i, PSND_RELEASE, PS_CURE, m->pos);  // (s56: element 11 on Donald, element 12 on the target)
    pc_snd(in, i, PSND_CURE_ON, who, who == 0 ? in->feet : P.m[who - 1].pos);
    if (who == 0) {
        float c[3] = {in->feet[0], in->feet[1] + 90 * in->to_u, in->feet[2]};
        pc_fx(in, "cure_flower", c, in->face);
        int v = in->hk && in->hk->heal_sora ? in->hk->heal_sora(i) : -1;
        PARTY_LOG("[party] %s: Cure on Sora (life %.0f of %.0f) -> %d\n", PARTY[i].name, in->sora_hp, in->sora_hp_max, v);
    } else {
        PMember *t = &P.m[who - 1];
        t->hp = fminf(t->hp_max, t->hp + t->hp_max * 0.3f);  // (GUESS: Cure LV1 share 0.3, PHH A36)
        PARTY_LOG("[party] %s: Cure on %s -> hp %.0f of %.0f\n", PARTY[i].name, PARTY[who - 1].name, t->hp, t->hp_max);
    }
}
static void pc_spells_tick(const PIn *in) {
    float to_u = in->to_u;
    for (int s = 0; s < PSP_N; s++) {
        PSpell *o = &PSP[s];
        if (!o->on) continue;
        PMember *m = &P.m[o->member];
        float t0 = o->t;
        o->t += in->kh;
        if (o->kind == PS_FIRE) {  // a cylinder on the caster
            memcpy(o->pos, m->pos, sizeof o->pos);
            if (o->fx >= 0 && in->hk && in->hk->fx_move) in->hk->fx_move(o->fx, o->pos, m->face);
            if (t0 == 0 || (int)(o->t / 18.0f) > (int)(t0 / 18.0f)) {
                bool last = o->t >= 36;
                float r = 200 * to_u, h = 80 * to_u;
                for (int k = 0; k < in->nen; k++) {
                    const PEnemy *e = &in->en[k];
                    float dx = e->pos[0] - o->pos[0], dz = e->pos[2] - o->pos[2], dy = e->pos[1] - o->pos[1];
                    if (e->life > 0 && dx * dx + dz * dz < r * r && dy > -h && dy < 2 * h)
                        pc_damage(o->member, in, e, last ? 150 : 50, 1, last ? 0x14 : 0x0f, 1, e->pos,
                                  last ? "fire_hit_last" : "fire_hit", 0, "fire");
                }
                fire_world(o->pos, last, r, h, "donald");  // (s59 fire world, sora.c)
            }
            if (o->t >= 44) {
                o->on = false;
                if (o->fx >= 0 && in->hk && in->hk->fx_end) in->hk->fx_end(o->fx, 0);
            }
        } else if (o->kind == PS_BLIZZARD) {  // one shard toward the target, the first hit ends it
            float step = 40 * in->kh * to_u, r = 80 * to_u;
            o->pos[0] += o->dir[0] * step, o->pos[2] += o->dir[1] * step;
            float sp[3] = {o->pos[0], o->pos[1] + 100 * to_u, o->pos[2]};
            if (o->fx >= 0 && in->hk && in->hk->fx_move) in->hk->fx_move(o->fx, sp, atan2f(o->dir[0], o->dir[1]));
            for (int k = 0; k < in->nen && o->on; k++) {
                const PEnemy *e = &in->en[k];
                float dx = e->pos[0] - o->pos[0], dz = e->pos[2] - o->pos[2], dy = e->pos[1] - o->pos[1];
                if (e->life > 0 && dx * dx + dz * dz < r * r && dy > -2 * r && dy < 2 * r)
                    pc_damage(o->member, in, e, 250, 1, 0x11, 2, e->pos, "blizzard_hit", 0, "blizzard"), o->on = false;
            }
            if ((!o->on || o->t >= 30) && o->fx >= 0 && in->hk && in->hk->fx_end) in->hk->fx_end(o->fx, o->on ? 12 : 0);
            if (o->t >= 30) o->on = false;
        } else if (o->kind == PS_THUNDER) {  // a bolt at the target (none: 300 cm ahead) after 6 frames, burst r 170
            if (o->t < 6) continue;
            float at[3] = {o->pos[0] + o->dir[0] * 300 * to_u, o->pos[1], o->pos[2] + o->dir[1] * 300 * to_u};
            for (int k = 0; k < in->nen; k++)
                if (in->en[k].actor == o->target) memcpy(at, in->en[k].pos, sizeof at);
            float r = 170 * to_u;
            for (int k = 0; k < in->nen; k++) {
                const PEnemy *e = &in->en[k];
                float dx = e->pos[0] - at[0], dz = e->pos[2] - at[2], dy = e->pos[1] - at[1];
                if (e->life > 0 && !psp_hit_before(o, e->actor) && dx * dx + dz * dz < r * r && fabsf(dy) < 2 * r) {
                    if (o->nhit < 16) o->hit[o->nhit++] = e->actor;
                    pc_damage(o->member, in, e, 250, 1, 0x14, 3, e->pos, "thunder_hit", 0, "thunder");
                }
            }
            pc_fx(in, "thunder_bolt", at, 0);
            pc_snd(in, o->member, PSND_BOLT, PS_THUNDER, at);  // (s56: the bolt element at the target: its SE)
            o->on = false;
        } else if (o->kind == PS_BASH) {  // Goofy's shield: out along the line to the target, then back to him
            // (s58) back to where his catch motion takes it (PC_BASH_CATCH), then leg 2 holds it there (still drawn
            // detached) until pc_act_tick starts the catch: its A402 flies it from that spot into his hand
            float step = PC_BASH_SPEED * in->kh, r = PC_BASH_R * to_u, back[3];
            pc_bash_spot(m, PC_BASH_CATCH, to_u, back);
            if (o->leg == 2) {
                memcpy(o->pos, back, sizeof o->pos);
                if (m->act != PACT_CAST || m->act_spell != PS_BASH) {  // (no catch coming: back in his hand)
                    o->on = false;
                    if (in->hk && in->hk->weapon) in->hk->weapon(o->member, true);
                }
                continue;
            }
            if (o->leg == 0) {
                if (t0 > 0)  // (s58: drawn at his hand on the release present)
                    o->pos[0] += o->dir[0] * step * to_u, o->pos[2] += o->dir[1] * step * to_u, o->travel += step;
                if (o->travel >= o->reach || o->t >= PC_BASH_OUT_MAX) o->leg = 1, o->nhit = 0;
            } else {
                float dx = back[0] - o->pos[0], dy = back[1] - o->pos[1], dz = back[2] - o->pos[2];
                float d = sqrtf(dx * dx + dy * dy + dz * dz);
                if (d <= step * to_u || o->t > PC_BASH_MAX) {
                    memcpy(o->pos, back, sizeof o->pos), o->leg = 2;
                    if (P.clog)
                        PARTY_LOG("[party] %s: shield back at his catch spot (t %.0f%s)\n", PARTY[o->member].name, o->t,
                                  d <= step * to_u ? "" : ", time out: snapped");
                    continue;
                }
                o->pos[0] += dx / d * step * to_u, o->pos[1] += dy / d * step * to_u, o->pos[2] += dz / d * step * to_u;
                if (hypotf(dx, dz) > 1e-4f) o->wyaw = atan2f(dx, dz);
            }
            for (int k = 0; k < in->nen; k++) {
                const PEnemy *e = &in->en[k];
                float dx = e->pos[0] - o->pos[0], dz = e->pos[2] - o->pos[2], dy = o->pos[1] - e->pos[1];
                if (e->life > 0 && !psp_hit_before(o, e->actor) && dx * dx + dz * dz < r * r && dy > -r &&
                    dy < (in->hit_h > 0 ? in->hit_h : 1.6f) + r) {
                    if (o->nhit < 16) o->hit[o->nhit++] = e->actor;
                    pc_damage(o->member, in, e, 100, 0, F54_PARTY_BIG, 0, o->pos, "hit_finish", 0, "bash (shield)");
                }
            }
        } else
            o->on = false;
    }
}
static bool pc_bash_out(int i) {
    for (int s = 0; s < PSP_N; s++)
        if (PSP[s].on && PSP[s].kind == PS_BASH && PSP[s].member == i) return true;
    return false;
}
static bool pc_bash_home(int i) {  // (s58) the shield waits at his catch spot (leg 2)
    for (int s = 0; s < PSP_N; s++)
        if (PSP[s].on && PSP[s].kind == PS_BASH && PSP[s].member == i && PSP[s].leg == 2) return true;
    return false;
}
static void pc_bash_end(int i, const char *why) {
    for (int s = 0; s < PSP_N; s++)
        if (PSP[s].on && PSP[s].kind == PS_BASH && PSP[s].member == i) {
            PSP[s].on = false;
            if (P.clog) PARTY_LOG("[party] %s: shield flight ended (%s, t %.0f)\n", PARTY[i].name, why, PSP[s].t);
        }
}
// (s58) member i's shield in flight this present (for the draw): the shield object's world spot, its yaw and the
// clock of its spin (KH frames since the throw); false = none out
static bool pc_bash_shield(int i, float pos[3], float *yaw, float *t) {
    for (int s = 0; s < PSP_N; s++)
        if (PSP[s].on && PSP[s].kind == PS_BASH && PSP[s].member == i) {
            memcpy(pos, PSP[s].pos, 3 * sizeof(float)), *yaw = PSP[s].wyaw, *t = PSP[s].t;
            return true;
        }
    return false;
}

// the playing action, one present: clock, root motion, the turn, hits, the release, the ability moves; free at its end
static void pc_act_end(int i, const char *why) {
    PMember *m = &P.m[i];
    party_blend_from(i, m->act_slot, fminf(m->act_clock, m->act_frames), PARTY_BLEND);
    m->act = PACT_NONE, m->motion = 0, m->clock = 0, m->band = PBAND_STAND;
    if (P.clog) PARTY_LOG("[party] %s: %s done (%s)\n", PARTY[i].name, m->act_entry >= 0 ? PENT[i][m->act_entry].name : "hop", why);
}
static void pc_act_tick(int i, const PIn *in) {
    PMember *m = &P.m[i];
    const PartyDef *pd = &PARTY[i];
    float c0 = m->act_clock, c1 = c0 + in->kh;
    if (m->blend >= 0 && (m->blend += in->kh) >= party_blend_len(m)) m->blend = -1;
    if (m->act == PACT_HOP) {  // the arc, per KH frame (velocity braked, gravity), lands on this present's floor
        for (float f = 0; f < in->kh; f += 1) {
            float k = fminf(1, in->kh - f);
            m->pos[0] += m->hop_v[0] * k * in->to_u, m->pos[2] += m->hop_v[2] * k * in->to_u;
            m->pos[1] += m->hop_v[1] * k * in->to_u;
            m->hop_v[0] *= m->hop_brake, m->hop_v[2] *= m->hop_brake, m->hop_v[1] -= m->hop_g * k;
        }
        m->kd_air += in->kh;
        m->act_clock = m->act_frames > 0 ? fminf(c1, m->act_frames - 0.01f) : 0;
        if ((m->hop_v[1] < 0 && m->pos[1] <= m->floor_t) || m->kd_air > PC_AIR_MAX) {
            int slot;
            const PMotX *mo = pc_motion(i, 5, &slot);  // (land)
            m->pos[1] = m->floor_s = m->floor_t, m->floor_off = 0, m->src_key = -1;
            if (!mo) { pc_act_end(i, "landed"); return; }
            m->act = PACT_LAND, m->act_slot = slot, m->act_clock = 0, m->act_frames = mo->frames;
            m->act_release = 0.8f * (float)mo->frames;  // (its first free frame: Donald 24 of 30, Goofy 34 of 40)
        }
        return;
    }
    if (m->act == PACT_LAND) {
        m->act_clock = c1;
        if (c1 >= m->act_release) pc_act_end(i, "landed");
        return;
    }
    // root motion (attack lunges: Donald A300 97 cm / A301 166 cm, Goofy 88 / 127 cm)
    const PMotX *mo = pmot_find(i, m->act_slot);
    if (mo && mo->root) {
        PMot pm = {mo->slot, mo->frames, mo->root};
        float r0[3], r1[3], s = sinf(m->face), c = cosf(m->face);
        pmot_root(&pm, c0, r0), pmot_root(&pm, fminf(c1, mo->frames), r1);
        float rx = r1[0] - r0[0], rz = r1[2] - r0[2];
        m->pos[0] += (rz * s + rx * c) * in->to_u, m->pos[2] += (rz * c - rx * s) * in->to_u;
    }
    // the turn toward the target (GUESS window) and the abilities' own moves
    bool tv = false;
    for (int k = 0; k < in->nen; k++)
        if (m->act_target && in->en[k].actor == m->act_target && in->en[k].life > 0) {
            float hh = pc_hh_cm(in) * in->to_u;
            m->act_tpos[0] = in->en[k].pos[0], m->act_tpos[1] = in->en[k].pos[1] + hh, m->act_tpos[2] = in->en[k].pos[2];
            tv = true;
        }
    float dx = m->act_tpos[0] - m->pos[0], dz = m->act_tpos[2] - m->pos[2], d = hypotf(dx, dz);
    const PHSlot *hs = phit_find(i, m->act_slot);
    bool in_win = hs && hs->nwin && c1 >= hs->win[0].start && c0 <= hs->win[hs->nwin - 1].end;
    bool turning = c0 < PC_TURN_FRAMES || ((m->act_spell == PS_TURBO || m->act_spell == PS_TORNADO) && in_win);
    if (tv && turning && d > 50 * in->to_u) {  // (KH2's turn: nothing within 50 cm, re:0x3B9F70)
        float step = pd->turn * in->kh, df = pt_wrap(atan2f(dx, dz) - m->face);
        m->face = pt_wrap(m->face + pt_clampf(df, -step, step));
    }
    if (in_win && (m->act_spell == PS_TURBO || m->act_spell == PS_TORNADO)) {
        float sp = (m->act_spell == PS_TURBO ? PC_TURBO_SPEED : PC_TORNADO_SPEED) * in->kh * in->to_u;
        float keep = (pc_r_cm(in) + P.stop_cm) * in->to_u;
        if (m->act_spell == PS_TURBO || d > keep) {  // (Turbo dashes through, along his facing; Tornado drifts to it)
            float mv = m->act_spell == PS_TURBO ? sp : fminf(sp, d - keep);
            m->pos[0] += sinf(m->face) * mv, m->pos[2] += cosf(m->face) * mv;
        }
    }
    pc_sweep(i, in, c0, c1);
    // the release (frame trigger 9): Donald's spells, Goofy's shield
    if (!m->act_fired && m->act == PACT_CAST && c1 >= m->act_release && c0 <= m->act_release + in->kh &&
        (m->act_spell <= PS_CURE || m->act_spell == PS_BASH)) {
        m->act_fired = true;
        int cost = PENT[i][m->act_entry].cost;
        if (m->act_spell == PS_CURE) pc_cure(i, in);
        else pc_spell_spawn(i, in, m->act_spell);
        pc_mp_spend(i, cost);
    }
    m->act_clock = c1;
    if (m->act_spell == PS_BASH && m->act_phase == 0) {  // 192 loops (from its loop frame) while the shield is out
        // (s58) KH2's 192 loops 78..120 cleanly (bake: f120 = f78 within 0.03): no blend at the wrap. The catch starts
        // as soon as the shield is home (the act's thread ended), blended 8 (motion_start(193, 8)), wherever the loop is
        bool out = m->act_fired && pc_bash_out(i) && !pc_bash_home(i);
        if (out || (!m->act_fired && c1 < m->act_frames)) {
            if (c1 >= m->act_frames) {
                float lp = m->act_loop > 0 && m->act_loop < m->act_frames ? m->act_loop : 0;
                m->act_clock = lp + fmodf(c1 - lp, m->act_frames - lp);
            }
            return;
        }
        int slot;
        const PMotX *mc = pc_motion(i, 193, &slot);  // (the catch)
        if (mc) {
            party_blend_from(i, m->act_slot, fminf(c1, m->act_frames - 0.01f), PC_BASH_CATCH_BLEND);
            m->act_phase = 1, m->act_slot = slot, m->act_clock = 0, m->act_frames = mc->frames, m->seg_ok = false;
            pc_bash_end(i, "caught");
            if (in->hk && in->hk->weapon) in->hk->weapon(i, true);  // (his catch motion's A402 has it, from the spot)
            return;
        }
        if (c1 < m->act_frames) return;
        pc_bash_end(i, "no catch motion");
        if (in->hk && in->hk->weapon) in->hk->weapon(i, true);
    }
    if (m->act_clock >= m->act_frames) pc_act_end(i, "motion end");
}
static void pc_drawn(int i, int *slot, float *clock) {
    const PMember *m = &P.m[i];
    if (m->act) *slot = m->act_slot, *clock = m->act_clock;
    else *slot = party_loco_slot(i, m->motion), *clock = m->clock;
}

// battle mode (PHH's stand-in: a live enemy within PC_BATTLE_CM of Sora); stance follows it on free frames
#define PC_BATTLE_HIT_HOLD 300  // presents (~10 s)
static void pc_battle(const PIn *in) {
    bool b = false;
    for (int k = 0; k < in->nen && !b; k++) {
        const PEnemy *e = &in->en[k];
        float dx = e->pos[0] - in->feet[0], dy = e->pos[1] - in->feet[1], dz = e->pos[2] - in->feet[2];
        b = e->life > 0 && sqrtf(dx * dx + dy * dy + dz * dz) / in->to_u < PC_BATTLE_CM;
    }
    // (s56: a hit on Sora keeps battle on for PC_BATTLE_HIT_HOLD presents - live_s56 died to hits from > 15 m, an
    // archer or a throw, with battle off, so Donald never cured; KH2's real check re:0x3AB690 is enemy awareness)
    static float last_hp = -1;
    static uint64_t hit_at;
    if (last_hp >= 0 && in->sora_hp > 0 && in->sora_hp < last_hp) hit_at = in->present ? in->present : 1;
    last_hp = in->sora_hp;
    if (hit_at && in->present - hit_at < PC_BATTLE_HIT_HOLD) b = true;
    if (b != P.battle && P.clog) PARTY_LOG("[party] battle %s (%d enemies listed)\n", b ? "ON" : "off", in->nen);
    if (!b && P.battle)
        for (int i = 0; i < PARTY_N; i++) P.m[i].hist[0] = P.m[i].hist[1] = P.m[i].hist[2] = -1, P.m[i].nonmove = 0;
    P.battle = b;
    if (b) P.battle_ticks++;
}
// KH2 pushes bodies apart (re:0x3B9090 -> re:0x40AD70, PHH pair_push); here (GUESS stand-in) a member is kept at least
// the enemy radius + P.stop_cm (flat) from every live enemy's centre - except while Goofy Turbo dashes through.
static void pc_push(int i, const PIn *in) {
    PMember *m = &P.m[i];
    if (m->act == PACT_HOP || (m->act == PACT_CAST && m->act_spell == PS_TURBO)) return;
    float keep = (pc_r_cm(in) + P.stop_cm) * in->to_u;
    for (int k = 0; k < in->nen; k++) {
        const PEnemy *e = &in->en[k];
        if (e->life <= 0 || fabsf(e->pos[1] - m->pos[1]) > (in->hit_h > 0 ? in->hit_h : 1.6f)) continue;
        float dx = m->pos[0] - e->pos[0], dz = m->pos[2] - e->pos[2], d = hypotf(dx, dz);
        if (d >= keep) continue;
        if (d < 1e-4f) dx = -sinf(m->face), dz = -cosf(m->face), d = 1;  // (right on it: back along his facing)
        float k2 = keep / d;
        m->pos[0] = e->pos[0] + dx * k2, m->pos[2] = e->pos[2] + dz * k2;
    }
}
// one member, one present (not blown): his action, else the think + the feet
static void party_fight(int i, const PIn *in) {
    PMember *m = &P.m[i];
    if (m->charge > 0 && (m->charge -= in->kh) <= 0) m->charge = 0, m->mp = PARTY[i].mp_max;  // (MP Charge done)
    if (m->act) {
        pc_act_tick(i, in);
        if (m->act) { pc_push(i, in); return; }
    }
    if (m->stance != P.battle) {  // (battle stance: the variant-0 locomotion slots; a blend into them)
        party_blend_from(i, party_loco_slot(i, m->motion), m->clock, PARTY_BLEND);
        m->stance = P.battle, m->clock = 0;
    }
    pc_think(i, in);
    if (!m->act && m->intent != PI_NONE) pc_feet(i, in);
    pc_push(i, in);
}
static void party_combat_init(void) {  // (first tick / hot reload: the members' combat state)
    for (int i = 0; i < PARTY_N; i++) {
        PMember *m = &P.m[i];
        if (m->hp_max > 0) continue;
        m->tgt = -1, m->cur = -1, m->forced = -1, m->act_entry = -1;
        m->hist[0] = m->hist[1] = m->hist[2] = -1;
        m->mp = PARTY[i].mp_max, m->hp = m->hp_max = 100;  // (HP on a 100 scale until slice 2 gives them KH HP)
    }
}

// The whole party for one present (Sora driving). Returns false when nothing is to be drawn.
static int g_pwarpair;  // (s118) live "set pwarpair 1" = the far warp fires mid-air too (the old rule)
static bool party_core_tick(const PIn *in) {
    if (!P.on) return false;
    if (in->grounded) ptrail_push(&P.trail, in->feet, in->to_u);
    bool back = P.ticks && in->present - P.last_present > PARTY_GAP;  // (hidden / not driven since: the reappear rule)
    if (P.hidden || back) {
        if (P.hidden) PARTY_LOG("[party] shown again (present %llu, %llu presents later)\n",
                                (unsigned long long)in->present, (unsigned long long)(in->present - P.last_present));
        P.hidden = false;
    }
    float sg = party_sora_ground(in);
    for (int i = 0; i < PARTY_N; i++) {
        PMember *m = &P.m[i];
        if (g_party_absorbed[i] || g_plim_hold[i]) continue;
        if (!m->placed) { party_place(i, in, "first frame"); continue; }
        float h = hypotf(m->pos[0] - in->feet[0], m->pos[2] - in->feet[2]) / in->to_u;
        float dy = fabsf(m->floor_s - sg) / in->to_u;
        char why[64];
        if (h > PARTY_WARP_CM && !in->grounded && !g_pwarpair) {  // (s118: user: "teleport only when Sora lands", not mid-glide)
            if (!m->warp_held) PARTY_LOG("[party] %s far %.0f cm but Sora is in the air (glide / fall): warp held until he lands\n", PARTY[i].name, h);
            m->warp_held = true;
            continue;
        }
        if (h > PARTY_WARP_CM) snprintf(why, sizeof why, m->warp_held ? "far %.0f cm, Sora landed" : "far %.0f cm", h);
        else if (back && dy > PARTY_WARP_DY_CM) snprintf(why, sizeof why, "back, %.0f cm below / above", dy);
        else { m->warp_held = false; continue; }
        m->warp_held = false;
        party_place(i, in, why);
    }
    party_slots(in);
    P.hk = in->hk;
    party_combat_init();
    pc_battle(in);  // (s55)
    for (int i = 0; i < PARTY_N; i++)
        if (g_party_absorbed[i] || g_plim_hold[i]) continue;
        else if (P.m[i].kd) party_kd_tick(i, in);
        else if (P.combat) party_fight(i, in);  // (s55: KH2's battle AI; out of battle it is the follow above)
        else party_move(i, in);
    party_rays(in);
    for (int i = 0; i < PARTY_N; i++) {
        party_floor(i, in);
        if (P.m[i].kd == KD_AIR) party_kd_land(i);
    }
    pc_spells_tick(in);  // (s55: Donald's spells, Goofy's thrown shield)
    P.last_present = in->present, P.ticks++;
    return true;
}

// Sora not driven this present (Link's state OTHER: climbing / swimming / puppet / cutscene / load / dead).
static bool party_core_hide(uint64_t present) {
    if (P.hidden || !P.ticks) return false;
    P.hidden = true;
    PARTY_LOG("[party] hidden (Sora not driven, present %llu)\n", (unsigned long long)present);
    return true;
}

// ---- BEGIN GENERATED (recon/wip_s55_party_combat/gen_party_levels.py; KH2 00battle.bin lvup, VERIFIED) ----
// PARTY_LV[member][L-1] = {EXP total that reaches level L, Str, Mag} (Donald = lvup character 2, Goofy = 3)
typedef struct { uint32_t exp; uint8_t str, mag; } PLv;
static const PLv PARTY_LV[2][99] = {
    {  // Donald, 00battle.bin@0x25f58
     {0, 1, 5}, {40, 1, 6}, {86, 2, 6}, {148, 2, 7}, {247, 3, 7}, {376, 3, 8}, {540, 4, 8}, {742, 4, 9},
     {987, 5, 9}, {1305, 5, 10}, {1704, 6, 10}, {2192, 6, 11}, {2777, 7, 11}, {3467, 7, 12}, {4306, 8, 12}, {5307, 8, 13},
     {6481, 9, 13}, {7883, 9, 14}, {9529, 10, 14}, {11435, 10, 15}, {13617, 11, 15}, {16142, 11, 16}, {19029, 12, 16}, {22292, 12, 17},
     {25943, 13, 17}, {29994, 13, 18}, {34514, 14, 18}, {39516, 14, 19}, {45015, 15, 19}, {51084, 15, 20}, {57739, 16, 20}, {65057, 16, 21},
     {73057, 17, 21}, {81820, 17, 22}, {91366, 18, 22}, {101780, 18, 23}, {113085, 19, 23}, {125370, 19, 24}, {138659, 20, 24}, {153045, 20, 25},
     {168555, 21, 25}, {185286, 21, 26}, {203266, 22, 26}, {222596, 22, 27}, {243307, 23, 27}, {265504, 23, 28}, {289219, 24, 28}, {314561, 24, 29},
     {341565, 25, 29}, {370344, 25, 30}, {401014, 26, 30}, {433614, 26, 31}, {467014, 27, 31}, {501214, 27, 32}, {536214, 28, 32}, {572014, 28, 33},
     {608614, 29, 33}, {646014, 29, 34}, {684214, 30, 34}, {723214, 30, 35}, {763014, 31, 35}, {803614, 31, 36}, {845014, 32, 36}, {887214, 32, 37},
     {930214, 33, 37}, {974014, 33, 38}, {1018614, 34, 38}, {1064014, 34, 39}, {1110214, 35, 39}, {1157214, 35, 40}, {1205014, 36, 40}, {1253614, 36, 41},
     {1303014, 37, 41}, {1353214, 37, 42}, {1404214, 38, 42}, {1456014, 38, 43}, {1508614, 39, 43}, {1562014, 39, 44}, {1616214, 40, 44}, {1671214, 40, 45},
     {1727014, 41, 45}, {1783614, 41, 46}, {1841014, 42, 46}, {1899214, 42, 47}, {1958214, 43, 47}, {2018014, 43, 48}, {2078614, 44, 48}, {2140014, 44, 49},
     {2202214, 45, 49}, {2265214, 45, 50}, {2329014, 46, 50}, {2393614, 46, 51}, {2459014, 47, 51}, {2525214, 47, 52}, {2592214, 48, 52}, {2660014, 48, 53},
     {2728614, 49, 53}, {2798014, 49, 54}, {2868214, 50, 54},
    },
    {  // Goofy, 00battle.bin@0x2658c
     {0, 5, 0}, {40, 5, 0}, {86, 6, 0}, {146, 6, 0}, {219, 7, 0}, {341, 7, 0}, {497, 8, 0}, {690, 8, 0},
     {925, 9, 0}, {1231, 9, 0}, {1616, 10, 0}, {2088, 10, 0}, {2655, 11, 0}, {3325, 11, 0}, {4141, 12, 0}, {5116, 12, 0},
     {6261, 13, 0}, {7630, 13, 0}, {9239, 14, 0}, {11104, 14, 0}, {13241, 15, 0}, {15716, 15, 0}, {18548, 16, 0}, {21750, 16, 0},
     {25334, 17, 0}, {29312, 17, 0}, {33752, 18, 0}, {38667, 18, 0}, {44072, 19, 0}, {50039, 19, 0}, {56584, 20, 0}, {63783, 20, 0},
     {71655, 21, 0}, {80280, 21, 0}, {89678, 22, 0}, {99933, 22, 0}, {111068, 23, 0}, {123171, 23, 0}, {136266, 24, 0}, {150445, 24, 0},
     {165735, 25, 0}, {182232, 25, 0}, {199964, 26, 0}, {219031, 26, 0}, {239464, 27, 0}, {261367, 27, 0}, {284772, 28, 0}, {309787, 28, 0},
     {336447, 29, 0}, {364864, 29, 0}, {395153, 30, 0}, {427353, 30, 0}, {460353, 31, 0}, {494153, 31, 0}, {528753, 32, 0}, {564153, 32, 0},
     {600353, 33, 0}, {637353, 33, 0}, {675153, 34, 0}, {713753, 34, 0}, {753153, 35, 0}, {793353, 35, 0}, {834353, 36, 0}, {876153, 36, 0},
     {918753, 37, 0}, {962153, 37, 0}, {1006353, 38, 0}, {1051353, 38, 0}, {1097153, 39, 0}, {1143753, 39, 0}, {1191153, 40, 0}, {1239353, 40, 0},
     {1288353, 41, 0}, {1338153, 41, 0}, {1388753, 42, 0}, {1440153, 42, 0}, {1492353, 43, 0}, {1545353, 43, 0}, {1599153, 44, 0}, {1653753, 44, 0},
     {1709153, 45, 0}, {1765353, 45, 0}, {1822353, 46, 0}, {1880153, 46, 0}, {1938753, 47, 0}, {1998153, 47, 0}, {2058353, 48, 0}, {2119353, 48, 0},
     {2181153, 49, 0}, {2243753, 49, 0}, {2307153, 50, 0}, {2371353, 50, 0}, {2436353, 51, 0}, {2502153, 51, 0}, {2568753, 52, 0}, {2636153, 52, 0},
     {2704353, 53, 0}, {2773353, 53, 0}, {2843153, 54, 0},
    },
};
// ---- END GENERATED ----

#ifndef SORA_PARTY_CORE_ONLY
// ===================================================================== GLUE (sora.c)
// Env (as slice 2): SAVAGEWIND_PARTY=0 no party, SAVAGEWIND_PARTY_DIR (default D:/SoraBotW/data/party),
// SAVAGEWIND_PARTY_RAY=0 trail floor only. Live (live_test.txt): set party 0/1, set plog 0/1, set pray 0/1,
// set pfwd 1/-1 (slot axis), set pslot <cm> (slot radius), set pwarp 1 (both beside Sora now), set pkdr <cm> (the
// ragdoll blast radius), set plie <KH frames> (extra lie on the floor), set pblow 1 / "party blow [power %]" (a test
// blast at Sora's feet: both blown, power 1 = KH2's default blow), "party warp", "party on", "party off".
static struct {  // (runtime/sora_draw.h sora_draw_actor_*)
    int (*load)(const char *model_path, const char *pose_path);
    void (*pose)(int id, int slot, float clock, const float feet[3], float face, float scale);
    void (*shadow)(int id, float floor_y, float strength);
    void (*shape)(int id, float radius, float fade_h, float min_scale, float min_alpha);
    void (*ghost)(int id, float alpha);  // (s87c party water; NULL = an older exe: no ghost)
    void (*off)(int id);
    int (*bones)(int id);
    bool (*slot_pose)(int id, int slot, float clock, float *out);
    void (*world)(int id, const float *bones, const float feet[3], float face, float scale);
    void (*weapon)(int id, bool shown);  // (s55: Goofy Bash; NULL = the shield stays drawn in his hand)
    // (s58: Goofy Bash's flying shield; NULL = an exe linked before s58: not drawn in flight, as before)
    void (*weapon_at)(int id, const float *bones, int n, const float pos[3], float yaw, float scale);
} PD;
// (s58) <name>_weapon_flight.bin ('SKRP' v1 of the weapon alone, partybake "wpose" with PARTYBAKE_SEEK=1): KH2's
// motion of the thrown weapon object, its own space (Goofy: W_EX030 motion 201 = slot 804, 20 frames, loops from 0).
// Kept in memory (~4 KB, never freed). None = the shield flies rigid (its hand pose, no spin).
#define PWF_SLOT 804
static struct { int nb, frames; const float *m; } g_pwf[PARTY_N];
static bool g_pw_on[PARTY_N];  // (the exe has a detached weapon from us for this member)
static void pwf_load(int i, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { LOG("[party] %s: no weapon flight bake %s\n", PARTY[i].name, path); return; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = n > 16 ? (uint8_t *)malloc((size_t)n) : NULL;
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n || memcmp(b, "SKRP", 4)) {
        fclose(f), free(b);
        LOG("[party] %s: bad weapon flight bake %s\n", PARTY[i].name, path);
        return;
    }
    fclose(f);
    uint32_t ns, nb;
    memcpy(&ns, b + 8, 4), memcpy(&nb, b + 12, 4);
    for (uint32_t k = 0; k < ns && 16 + (k + 1) * 8 <= (uint32_t)n; k++) {
        uint16_t slot, poses;
        uint32_t off;
        memcpy(&slot, b + 16 + k * 8, 2), memcpy(&poses, b + 18 + k * 8, 2), memcpy(&off, b + 20 + k * 8, 4);
        if (slot != PWF_SLOT || poses < 2 || !nb || nb > 16 || off + (size_t)poses * nb * 48 > (size_t)n) continue;
        g_pwf[i].nb = (int)nb, g_pwf[i].frames = poses - 1, g_pwf[i].m = (const float *)(b + off);
        LOG("[party] %s: weapon flight motion (slot %d, %d frames, %u bones)\n", PARTY[i].name, PWF_SLOT, poses - 1, nb);
        return;
    }
    free(b);
    LOG("[party] %s: weapon flight bake %s has no slot %d\n", PARTY[i].name, path, PWF_SLOT);
}
// its bones at KH clock t (looped; between baked frames blended, rows re-normalised as the party blend does)
static const float *pwf_sample(int i, float t) {
    static float out[16 * 12];
    int nb = g_pwf[i].nb, n = g_pwf[i].frames;
    if (!g_pwf[i].m || nb <= 0 || n <= 0) return NULL;
    float f = fmodf(t > 0 ? t : 0, (float)n);
    int f0 = (int)f;
    if (f0 >= n) f0 = n - 1;
    float u = f - (float)f0;
    const float *a = g_pwf[i].m + (size_t)f0 * nb * 12, *b = a + (size_t)nb * 12;  // (pose n = pose 0: the loop)
    for (int k = 0; k < nb; k++) {
        float *d = out + k * 12, len[3];
        const float *x = a + k * 12, *y = b + k * 12;
        for (int r = 0; r < 3; r++) {
            float la = sqrtf(v3dot(x + 3 * r, x + 3 * r)), lb = sqrtf(v3dot(y + 3 * r, y + 3 * r));
            len[r] = la + (lb - la) * u;
        }
        for (int j = 0; j < 12; j++) d[j] = x[j] + (y[j] - x[j]) * u;
        if (u > 0) ortho_rows(d, len);
    }
    return out;
}
#define PARTY_MAX_BONES 256
static int g_party_id[PARTY_N] = {-1, -1};

static float g_pb_from[PARTY_N][PARTY_MAX_BONES * 12], g_pb_cur[PARTY_N][PARTY_MAX_BONES * 12];
static bool g_pb_cur_ok[PARTY_N];
static bool g_party_warp_now;
static float g_party_blow_now;  // > 0: a test blast at Sora's feet with this power on the next tick

// ---- (s56) party sounds: KH2's own (recon/S56_HELPER_PARTYSFX.md; the names come from the s56 sfx.bin bake,
// recon/wip_s56_partysfx/sfxpack_s56.py). Data-driven by the motion each member plays (his bake's mset frame triggers:
// 14 = vsb_voice2 p1 vtbl, 2 = footstep p1 kind, 1 = effect_caster p1 element) + his weapon mset's play_sound rows
// (not in the body bake: PSFX_WEAPON) + the moments Donald's object script makes (PHooks.sound: the spell voice before
// the cast motion, the effect elements at the release / the bolt / the cure target). Voices: KH2's vtbl pick
// (re:0x3CDB60: weighted entries roll 0..99 in order, weight-0 entries share a uniform pick, -3 = the partner's name)
// and one voice channel per member (re:0x3CE580 / re:0x3CE260: taken when silent or priority >= the playing line's,
// which it stops). Played at the member's feet (sora_sfx_play_at), never through Sora's bark channel.
// An sfx.bin without the party sounds (the pre-s56 pack): the first probe logs one "unknown sound" line, then off.
// Live: set psfx 0 (off) / 1 (all, default) / 2 (no footsteps), set psfxlog 0/1. Env SAVAGEWIND_PARTY_SFX=0/1/2.
typedef struct { uint8_t member, vtbl, pri; const char *base; uint8_t n, name_w; } PSfxSet;
static const PSfxSet PSFX_SETS[] = {  // 00battle.bin vtbl rows (kh2_sounds.kh.json actors 1 / 2 vtbl): VERIFIED
    {0, 4, 1, "attack", 2, 0},   {0, 6, 1, "spell", 2, 0},    {0, 21, 1, "cure_self", 2, 0}, {0, 3, 1, "item", 2, 100},
    {0, 29, 2, "hurt", 1, 0},    {0, 30, 2, "hurt_low", 1, 0}, {0, 1, 2, "blown", 2, 0},     {0, 2, 4, "ko", 1, 0},
    {0, 10, 1, "stun_end", 1, 0},
    {0, 16, 1, "megaduck", 1, 0},  // (s150) Duck Flare's Megaduck voice = donald_megaduck.0 (hb0_donald #18)
    {1, 6, 1, "spell", 1, 0},
    {1, 4, 1, "attack", 2, 0},   {1, 3, 1, "item", 1, 100},   {1, 29, 2, "hurt", 1, 0},     {1, 30, 2, "hurt_low", 1, 0},
    {1, 1, 2, "blown", 1, 0},    {1, 2, 4, "ko", 1, 0},       {1, 10, 1, "stun_end", 1, 0},
};
typedef struct { uint8_t member; uint16_t slot, frame; const char *name; } PSfxW;
static const PSfxW PSFX_WEAPON[] = {  // W_EX020 / W_EX030.mset play_sound rows (the json's W_EX0x0 src): VERIFIED
    {0, 604, 34, "donald_swing"}, {0, 608, 18, "donald_swing"}, {0, 612, 34, "donald_swing"}, {0, 692, 18, "donald_swing"},
    {0, 724, 4, "donald_air_swing"}, {0, 728, 14, "donald_air_swing"},
    {1, 604, 24, "goofy_swing1"}, {1, 608, 30, "goofy_swing2"}, {1, 612, 24, "goofy_swing1"}, {1, 692, 30, "goofy_swing2"},
    {1, 724, 6, "goofy_swing2"}, {1, 728, 10, "goofy_swing1"}, {1, 764, 55, "goofy_tornado"},
};
static const int PSFX_FX_D[] = {0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12};  // his effect elements that carry an SE
static const int PSFX_FX_G[] = {0, 2, 3};
static struct {
    int ready;        // -1 not probed, 0 off (no party sounds in this sfx.bin / sfx off), 1 on
    int mode;         // 0 off, 1 all, 2 no footsteps (live psfx)
    bool log;
    uint32_t rnd;
    struct PSfxM { bool seen; int slot; float clock; int voice, vpri; uint64_t present; uint8_t step[11]; } m[PARTY_N];
} PS = {.ready = -1, .mode = 1, .m = {{.voice = -1}, {.voice = -1}}};
static bool psfx_on(void) {
    if (PS.ready < 0) {
        PS.mode = (int)env_f("SAVAGEWIND_PARTY_SFX", (float)PS.mode);
        PS.rnd = (uint32_t)GetTickCount() | 1;
        PS.ready = sora_sfx_ready() && sora_sfx_id("donald_attack.0") >= 0;  // (old pack: this one lookup logs once)
        LOG("[party] sfx: %s (set psfx 0/1/2)\n", PS.ready ? "Donald + Goofy KH2 sounds on" :
            "no party sounds in this sfx.bin (pre-s56 pack) or sfx off: party sounds off");
    }
    return PS.ready > 0 && PS.mode > 0;
}
static int psfx_rand(int n) { return PS.rnd = PS.rnd * 214013u + 2531011u, n > 0 ? (int)((PS.rnd >> 16) & 0x7fff) % n : 0; }
static int psfx_play(int i, const char *name, const float at[3]) {
    int id = sora_sfx_id(name);
    int h = id >= 0 ? sora_sfx_play_at(id, 1.0f, at ? at : P.m[i].pos) : -1;
    if (PS.log) LOG("[party] sfx %s: %s -> %d\n", PARTY[i].name, name, h);
    return h;
}
// a voice line by vtbl id; target = the KH2 character the line may name (1 Sora, 2 Donald, 3 Goofy; 0 none)
static void psfx_voice(int i, int vtbl, int target, const float at[3]) {
    const PSfxSet *s = NULL;
    for (size_t k = 0; k < sizeof PSFX_SETS / sizeof PSFX_SETS[0] && !s; k++)
        if (PSFX_SETS[k].member == i && PSFX_SETS[k].vtbl == vtbl) s = &PSFX_SETS[k];
    if (!s) return;  // (a row the party never plays here: no sound)
    char nm[48];
    static const char *const CHAR[4] = {"", "sora", "donald", "goofy"};
    if (s->name_w && target >= 1 && target <= 3 && target != i + 2 && psfx_rand(100) < s->name_w)
        snprintf(nm, sizeof nm, "%s_name_%s", PARTY[i].name, CHAR[target]);
    else if (s->n)
        snprintf(nm, sizeof nm, "%s_%s.%d", PARTY[i].name, s->base, psfx_rand(s->n));
    else
        return;
    if (PS.m[i].voice >= 0 && sora_sfx_playing(PS.m[i].voice)) {
        if (s->pri < PS.m[i].vpri) return;  // (the playing line keeps the channel)
        sora_sfx_stop(PS.m[i].voice);
    }
    PS.m[i].voice = psfx_play(i, nm, at), PS.m[i].vpri = s->pri;
}
static void psfx_fx(int i, int element, const float at[3]) {  // the SE of an element of his own effect set
    const int *e = i ? PSFX_FX_G : PSFX_FX_D;
    int n = i ? (int)(sizeof PSFX_FX_G / sizeof PSFX_FX_G[0]) : (int)(sizeof PSFX_FX_D / sizeof PSFX_FX_D[0]);
    for (int k = 0; k < n; k++)
        if (e[k] == element) {
            char nm[32];
            snprintf(nm, sizeof nm, "%s_fx%d", PARTY[i].name, element);
            psfx_play(i, nm, at);
            return;
        }
}
// every trigger of slot with a < frame <= b (the body bake's + the weapon mset's)
static void psfx_fire(int i, int slot, const PMotX *mo, float a, float b) {
    for (int k = 0; mo && k < mo->nft; k++) {
        uint16_t fr;
        int16_t p1;
        memcpy(&fr, mo->ft + k * 8, 2), memcpy(&p1, mo->ft + k * 8 + 4, 2);
        if (!(a < (float)fr && (float)fr <= b)) continue;
        int id = mo->ft[k * 8 + 2];
        if (id == 14) psfx_voice(i, p1, 0, NULL);
        else if (id == 1) psfx_fx(i, p1, NULL);
        else if (id == 2 && PS.mode == 1 && p1 >= 0 && p1 <= 10) {  // footstep kind: 0 run / 4 walk cycle 4 variants
            int n = p1 == 0 || p1 == 4 ? 4 : 1;
            char nm[32];
            snprintf(nm, sizeof nm, "%s_step%d.%d", PARTY[i].name, p1, PS.m[i].step[p1] % n);
            PS.m[i].step[p1] = (uint8_t)((PS.m[i].step[p1] + 1) % n);
            psfx_play(i, nm, NULL);
        }
    }
    for (size_t k = 0; k < sizeof PSFX_WEAPON / sizeof PSFX_WEAPON[0]; k++) {
        const PSfxW *w = &PSFX_WEAPON[k];
        if (w->member == i && w->slot == slot && a < (float)w->frame && (float)w->frame <= b) psfx_play(i, w->name, NULL);
    }
}
// once per drawn member per present: the triggers its motion passed since the last present
static void psfx_tick(int i, uint64_t present) {
    if (!psfx_on()) return;
    float c;
    int s = party_draw_slot(i, &c);
    struct PSfxM *q = &PS.m[i];
    bool fresh = !q->seen || present - q->present > PARTY_GAP;  // (load / hot reload / shown again: no burst)
    const PMotX *mo = pmot_find(i, s);
    if (!fresh) {
        if (s != q->slot) psfx_fire(i, s, mo, -1, c);  // a new motion: from its frame 0
        else if (c > q->clock) psfx_fire(i, s, mo, q->clock, c);
        else if (c < q->clock) {  // back: a loop (from its loop frame) or the same motion again
            if (mo && mo->loop < mo->frames) psfx_fire(i, s, mo, q->clock, mo->frames), psfx_fire(i, s, mo, mo->loop - 0.5f, c);
            else psfx_fire(i, s, mo, -1, c);
        }
    }
    q->seen = true, q->slot = s, q->clock = c, q->present = present;
}
// PHooks.sound: Donald's script moments (VERIFIED by his P_EX020 BDX; Goofy's acts request no voice / start no element
// from the script: his are the frame triggers above)
static void party_sound_cb(int i, int ev, int arg, const float at[3]) {
    if (!psfx_on() || i != 0) return;
    switch (ev) {
    case PSND_ACT:  // trap_obj_voice(self, 6) in the fire / blizzard / thunder acts
        if (arg == PS_FIRE || arg == PS_BLIZZARD || arg == PS_THUNDER) psfx_voice(i, 6, 0, at);
        break;
    case PSND_CURE_VOICE:  // himself -> vtbl 21; Sora / Goofy -> vtbl 3 (the -3 entry: "Sora!" / "Goofy!")
        if (arg == 1) psfx_voice(i, 21, 0, at);
        else psfx_voice(i, 3, arg == 0 ? 1 : 3, at);
        break;
    case PSND_RELEASE:  // fire: elements 0 + 1 bound on him; blizzard 3; thunder 7; cure 11
        if (arg == PS_FIRE) psfx_fx(i, 0, at), psfx_fx(i, 1, at);
        else if (arg == PS_BLIZZARD) psfx_fx(i, 3, at);
        else if (arg == PS_THUNDER) psfx_fx(i, 7, at);
        else if (arg == PS_CURE) psfx_fx(i, 11, at);
        break;
    case PSND_BOLT: psfx_fx(i, 8, at); break;      // thunder: element 8 / 9 started at the target (same SE)
    case PSND_CURE_ON: psfx_fx(i, 12, at); break;  // cure: element 12 bound on the target
    }
}

static void party_load(void) {
    const char *e = getenv("SAVAGEWIND_PARTY");
    if (e && *e && !atoi(e)) { LOG("[party] off (SAVAGEWIND_PARTY=0)\n"); return; }
    HMODULE exe = GetModuleHandleA(NULL);
    PD.load = (int (*)(const char *, const char *))(void *)GetProcAddress(exe, "sora_draw_actor_load");
    PD.pose = (void (*)(int, int, float, const float *, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_pose");
    PD.shadow = (void (*)(int, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_shadow");
    PD.shape = (void (*)(int, float, float, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_shadow_shape");
    PD.off = (void (*)(int))(void *)GetProcAddress(exe, "sora_draw_actor_off");
    PD.ghost = (void (*)(int, float))(void *)GetProcAddress(exe, "sora_draw_actor_ghost");
    PD.bones = (int (*)(int))(void *)GetProcAddress(exe, "sora_draw_actor_bone_count");
    PD.slot_pose = (bool (*)(int, int, float, float *))(void *)GetProcAddress(exe, "sora_draw_actor_slot_pose");
    PD.world = (void (*)(int, const float *, const float *, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_world");
    PD.weapon = (void (*)(int, bool))(void *)GetProcAddress(exe, "sora_draw_actor_weapon");
    PD.weapon_at = (void (*)(int, const float *, int, const float *, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_weapon_at");
    LOG("[party] flying shield: sora_draw_actor_weapon_at %s\n",
        PD.weapon_at ? "found" : "not in this exe (Goofy's Bash shield unseen in flight until the next exe link)");
    if (!PD.load || !PD.pose || !PD.shadow || !PD.shape || !PD.off) {
        LOG("[party] off: sora_draw_actor_* not in this exe (the s54 party draw needs the next exe link)\n");
        return;
    }
    if (!PD.bones || !PD.slot_pose || !PD.world) PD.bones = NULL, LOG("[party] no motion blend (actor pose API missing)\n");
    const char *dir = getenv("SAVAGEWIND_PARTY_DIR");
    if (!dir || !*dir) dir = "D:/SoraBotW/data/party";
    dir = sora_path(dir);
    P.ray_on = env_f("SAVAGEWIND_PARTY_RAY", 1) != 0;
    for (int i = 0; i < PARTY_N; i++) {
        char mp[512], pp[512], mo[512];
        snprintf(mp, sizeof mp, "%s/%s_model.bin", dir, PARTY[i].name);
        snprintf(pp, sizeof pp, "%s/%s_pose.bin", dir, PARTY[i].name);
        snprintf(mo, sizeof mo, "%s/%s_motion.bin", dir, PARTY[i].name);
        int id = PD.load(mp, pp);
        g_party_id[i] = id > 0 ? id : -1;
        if (id > 0) PD.shape(id, PARTY[i].shadow_r, 0, 0, 0);
        if (id > 0 && PD.weapon) PD.weapon(id, true);  // (s55: a reload mid-Bash must not leave the shield hidden)
        if (id > 0 && PD.weapon_at) PD.weapon_at(id, NULL, 0, NULL, 0, 0);  // (s58: ... nor flying from the old DLL)
        g_pw_on[i] = false;
        if (id > 0 && PD.bones && PD.bones(id) > PARTY_MAX_BONES) LOG("[party] %s: too many bones for the blend\n", PARTY[i].name);
        LOG("[party] %s: draw actor %d%s (s54 party3: follow + knockdown)\n", PARTY[i].name, id, id > 0 ? "" : " (not drawn)");
        if (!party_mot_load(i, mo)) LOG("[party] %s: no motion bake %s (built-in frame counts, no root motion)\n", PARTY[i].name, mo);
        snprintf(mo, sizeof mo, "%s/%s_hits.bin", dir, PARTY[i].name);  // (s55: the weapon's hit windows)
        if (!party_hits_load(i, mo)) LOG("[party] %s: no hit bake %s: his attacks don't hit\n", PARTY[i].name, mo);
        if (i == 1 && PD.weapon_at) {  // (s58: Goofy's thrown shield; Donald throws nothing)
            snprintf(mo, sizeof mo, "%s/%s_weapon_flight.bin", dir, PARTY[i].name);
            pwf_load(i, mo);
        }
        LOG("[party] %s: combat (s55 slice 1): %d actions, %d motion slots, %d hit slots, battle idle/walk/run %.0f/%.0f/%.0f "
            "frames\n", PARTY[i].name, PENT_N[i], PMOT_N[i], PHIT_N[i], PARTY[i].bframes[0], PARTY[i].bframes[1],
            PARTY[i].bframes[2]);
    }
    P.rnd = (uint32_t)GetTickCount();
    P.combat = env_f("SAVAGEWIND_PARTY_COMBAT", 1) != 0;
}

static void party_all_off(void) {
    for (int i = 0; i < PARTY_N; i++)
        if (g_party_id[i] > 0 && PD.off) PD.off(g_party_id[i]);
}

// (s57 user: "they are killing everything really easily") the party's damage x this %, after the KH rule; live "set pdmg <pct>"
static int g_party_dmg_pct = 50;
// (s174 user: "having 2 donalds and 2 goofys is very op ... lower their damage by like 3x or 4x") while a friend is connected
// (a live peer) every game's party hits the same enemies: their damage x this % (25 = 4x less; 100 = off); live "set pmpdmg <pct>"
static int g_party_mp_pct = 25;
static bool party_mp_live(void) {
    if (!NET.on) return false;
    for (int k = 0; k < NET_MAX_PEERS; k++) if (NET.slot[k].used && NET.slot[k].p.st == PEER_LIVE) return true;
    return false;
}
static int party_mp_scale(int d) {
    static bool was;
    bool mp = g_party_mp_pct != 100 && party_mp_live();
    if (mp != was) was = mp, LOG("[party] multiplayer damage cut %s (%d%%)\n", mp ? "ON: a friend is connected" : "off", g_party_mp_pct);
    if (!mp || d <= 0) return d;
    d = (int)lroundf((float)d * (float)g_party_mp_pct / 100.0f);
    return d < 1 ? 1 : d;
}
static int g_party_wet = 1;  // (s87c party water) live "set pwet 0" = off: their bodies are hidden by the water as before

// (s57 party HUD, recon/S57_HELPER_HUDPARTY.md) KH2's party gauges: each member's HP / MP to the exe's sora_hud_party
// (looked up by name: an exe linked before s57 lacks it -> logged once, no plates, nothing else changes). Called from
// sora.c hud_update, i.e. every frame Sora's gauge is up. Shown while the party is on and placed (also while they are
// hidden in the world for a climb / swim: KH2 keeps the plates up). HP: their KH HP (full until enemies hit them, slice
// 2). MP Charge: mp < 0 = the charge's progress (MpChargeTime). Live "set hudparty 0/1".
static bool g_party_hud = true;
static void party_hud(void) {
    static void (*fn)(int, float, float, float, float, bool);
    static int tried;
    if (!tried) {
        tried = 1, fn = (void (*)(int, float, float, float, float, bool))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_party");
        LOG("[party] HUD: sora_hud_party %s\n", fn ? "found" : "not in this exe (no party gauges until the next exe link)");
    }
    if (!fn) return;
    for (int i = 0; i < PARTY_N; i++) {
        const PMember *m = &P.m[i];
        bool shown = g_party_hud && P.on && P.ticks > 0 && g_party_id[i] > 0 && m->hp_max > 0 && !g_party_absorbed[i];
        float ct = PARTY[i].mp_charge > 0 ? PARTY[i].mp_charge : 1;
        float mp = m->charge > 0 ? -(1 - pt_clampf(m->charge / ct, 0, 1)) * PARTY[i].mp_max : m->mp;
        fn(i, m->hp, m->hp_max, mp, PARTY[i].mp_max, shown);
    }
}
static bool party_set(const char *key, int v) {  // (live keys; sora.c's "set" chain asks this)
    if (!strcmp(key, "party")) {
        P.on = v != 0;
        if (!P.on) party_all_off();
    } else if (!strcmp(key, "plog")) P.log = v != 0;
    else if (!strcmp(key, "pray")) P.ray_on = v != 0;
    else if (!strcmp(key, "pspawn")) P.spawn = v != 0;  // (s99 b14-bed: KH2 slots + wall / floor rays at a placement)
    else if (!strcmp(key, "pfwd")) P.fwd = v < 0 ? -1 : 1;
    else if (!strcmp(key, "pslot")) P.slot_r_cm = v < 20 ? 20.0f : (float)v;
    else if (!strcmp(key, "pwarp")) g_party_warp_now = v != 0;
    else if (!strcmp(key, "pwarpair")) g_pwarpair = v != 0, LOG("[party] pwarpair %d (%s)\n", g_pwarpair, g_pwarpair ? "the far warp fires in the air too (old)" : "the far warp waits until Sora lands");  // (s118)
    else if (!strcmp(key, "pkdr")) P.kd_radius_cm = v < 0 ? 0.0f : (float)v;
    else if (!strcmp(key, "plie")) P.kd_lie = v < 0 ? 0.0f : (float)v;
    else if (!strcmp(key, "pblow")) g_party_blow_now = v ? 1.0f : 0.0f;
    else if (!strcmp(key, "pcombat")) P.combat = v != 0;  // (s55: the battle AI; 0 = they only follow)
    else if (!strcmp(key, "pclog")) P.clog = v != 0;
    else if (!strcmp(key, "pair")) P.air_rows = v != 0;   // (the jump attack rows: no jump arc yet, default 0)
    else if (!strcmp(key, "pstop")) P.stop_cm = v < 0 ? 0.0f : (float)v;
    else if (!strcmp(key, "psfx")) PS.mode = v < 0 ? 0 : v > 2 ? 2 : v;  // (s56: 0 off, 1 all, 2 no footsteps)
    else if (!strcmp(key, "pwet")) g_party_wet = v != 0;  // (s87c party water: 0 = no ghost for Donald / Goofy)
    else if (!strcmp(key, "pdmg")) g_party_dmg_pct = v < 0 ? 0 : v;  // (s57: % of the party's damage)
    else if (!strcmp(key, "pmpdmg")) g_party_mp_pct = v < 0 ? 0 : v;  // (s174: % of the party's damage while a friend is connected)
    else if (!strcmp(key, "psfxlog")) PS.log = v != 0;
    else if (!strcmp(key, "hudparty")) g_party_hud = v != 0;  // (s57: KH2's party gauges on / off)
    else if (!strcmp(key, "pmp")) {
        for (int i = 0; i < PARTY_N; i++) P.m[i].mp = (float)(v < 0 ? 0 : v > 100 ? 100 : v), P.m[i].charge = 0;
    } else return false;
    return true;
}

static void party_live(const char *cmd) {  // (live "party <cmd>": blow [power %], warp, on, off)
    int pct = 100;
    if (!strncmp(cmd, "blow", 4)) {
        sscanf(cmd + 4, "%d", &pct);
        g_party_blow_now = (pct > 0 ? pct : 100) / 100.0f;
    } else if (!strcmp(cmd, "warp")) g_party_warp_now = true;
    else if (!strcmp(cmd, "on")) P.on = true;
    else if (!strcmp(cmd, "off")) P.on = false, party_all_off();
    else if (!strncmp(cmd, "cast ", 5)) {  // (s55) "party cast <action> [donald|goofy]": that action on the next think
        char nm[32] = {0}, who[16] = {0};
        sscanf(cmd + 5, "%31s %15s", nm, who);
        int n = 0;
        for (int i = 0; i < PARTY_N; i++) {
            int k = pc_find_entry(i, nm);
            if (k >= 0 && (!who[0] || !strcmp(who, PARTY[i].name))) P.m[i].forced = k, n++;
        }
        if (!n) { LOG("[party] live: no member has \"%s\" (attack1 attack2 fire blizzard thunder cure tornado bash turbo)\n", nm); return; }
    } else { LOG("[party] live: unknown \"party %s\" (blow [pct], warp, on, off, cast <action> [who])\n", cmd); return; }
    LOG("[party] live: party %s\n", cmd);
}

// For later slices (enemy hits): blow member 0 Donald / 1 Goofy away from world point 'from', power 1 = KH2's blow.
static void party_knockdown(int member, const float from[3], float power) {
    if (member >= 0 && member < PARTY_N && g_party_id[member] > 0 && P.on && !P.hidden)
        party_knockdown_core(member, from, power, 0.01f * g_k);
}

static bool party_ray_cb(const float a[3], const float b[3], float h[3], float n[3]) { return linkpos_raycast(a, b, h, n); }

// (s55 combat) the hooks the core calls. Damage: sora_levels.h's KH-relative rule (sl_hit) with the MEMBER's Str (or
// Mag for Donald's spells) + his weapon at his level, the level his own lvup table gives for Sora's EXP total (KH2 pays
// the same EXP to all three); "set lvdmg 0" = the flat rule (base x Power / 100). The hit is queued in sora_levels'
// credit list like Sora's own, so a kill by Donald / Goofy pays Sora's EXP (S54_HELPER_LV_LEVELS.md 3.3).
static int party_level(int i) {
    sl_init();
    int lv = 1;
    while (lv < 99 && SL.exp >= PARTY_LV[i][lv].exp) lv++;
    return lv;
}
static int party_dmg_cb(int i, uint64_t actor, int32_t life, int power, int magic) {
    sl_init();
    int flat = (int)lroundf(g_dmg_base * (float)power / 100.0f), d;
    if (flat < 1) flat = 1;
    d = flat;
    if (life < 0) life = sl_life_now(actor);
    SlCredit *c = life > 0 ? sl_credit(actor, life) : NULL;
    if (!c) return party_mp_scale(d);
    if (life > c->max_life) c->max_life = life;
    int lv = party_level(i);
    const PLv *r = &PARTY_LV[i][lv - 1];
    int st = magic ? r->mag + PARTY[i].wmag : r->str + PARTY[i].wstr, hp_pct = c->hp_pct;
    if (sl_khformula) {  // (s109) KH2's own damage function, same as Sora's; KH2 has no party multiplier (pdmg unused)
        char why[200];
        int k = sl_kh_apply(c, st, power, why, (int)sizeof why);
        if (SL.lvdmg) d = k;
        if (SL.lvlog && P.clog)
            LOG("[party] lvdmg %s LV%d %llx %s: %s: %s x max life %d -> %d%s\n", PARTY[i].name, lv,
                (unsigned long long)actor, c->name, magic ? "Mag+staff" : "Str+weapon", why, c->max_life, d,
                SL.lvdmg ? "" : " [lvdmg 0: flat]");
        d = party_mp_scale(d);
        c->hit_at = SL.clock, c->queued += d;
        return d;
    }
    float kh = (float)st * (float)power / 100.0f * (float)SL.khdmg / 100.0f;
    float kh_hp = (float)SL_LVPM[c->kh_level - 1].hp_mul * (float)hp_pct / 100.0f;
    float raw = kh / kh_hp * (float)c->max_life;
    if (SL.lvdmg) d = raw > 1e6f ? 1000000 : (int)lroundf(raw), d = d < 1 ? 1 : d;
    if (SL.lvlog && P.clog)
        LOG("[party] lvdmg %s LV%d %llx %s: %s %d x power %d = %.2f vs KH HP %.1f (KH LV%d, %d%%) x max life %d -> %d%s\n",
            PARTY[i].name, lv, (unsigned long long)actor, c->name, magic ? "Mag+staff" : "Str+weapon", st, power, kh, kh_hp,
            c->kh_level, hp_pct, c->max_life, d, SL.lvdmg ? "" : " [lvdmg 0: flat]");
    if (g_party_dmg_pct != 100) d = (int)lroundf((float)d * (float)g_party_dmg_pct / 100.0f), d = d < 1 ? 1 : d;
    d = party_mp_scale(d);
    c->hit_at = SL.clock, c->queued += d;  // (the kill rule: Sora's EXP)
    return d;
}
static void party_hit_cb(int i, uint64_t actor, int dmg, int f54, int f40, const float at[3], const char *fx, int atkp) {
    if (f40 == 3 && ah_enabled() && !ah_small(actor)) f54 = 0x11;  // (s58: no BlownOff on a big enemy / boss)
    if (f40 == 3 && ah_enabled() && g_ah_launch_f40 >= 0) f40 = g_ah_launch_f40;  // (s57: Donald's Thunder, no shock)
    linkpos_damage_enemy_ex(actor, dmg, f54, f40);
    if (fx) sora_fx_event(fx, at, P.m[i].face, 0.01f * g_k);
    // (s56 pack: the party's atkp rows -> their own hit SEs; set psfx 0 = Sora's base Keyblade hit, as before s56)
    if (atkp > 0 && (PS.mode ? sora_sfx_hit(atkp, 1.0f, at) : -1) < 0) {
        static int hit_se = -2;
        if (hit_se == -2) hit_se = sora_sfx_id("hit");
        if (hit_se >= 0) sora_sfx_play_at(hit_se, 1.0f, at);
    }
}
static int party_heal_cb(int i) {
    (void)i;
    int q = (int)lroundf((float)g_hud_life_max * g_cure_pct / 100.0f);  // (Sora's own Cure rule, UNVERIFIED share)
    return link_heal(q < 1 ? 1 : q);  // (s85f: up to BotW's max hearts)
}
static int party_fx_cb(const char *name, const float pos[3], float yaw) { return sora_fx_event(name, pos, yaw, 0.01f * g_k); }
static void party_fx_move_cb(int h, const float pos[3], float yaw) { if (sora_fx_alive(h)) sora_fx_move(h, pos, yaw); }
static void party_fx_end_cb(int h, float fade) {
    if (!sora_fx_alive(h)) return;
    if (fade > 0) sora_fx_fadeout(h, fade);
    else sora_fx_kill(h);
}
static void party_weapon_cb(int i, bool shown) { pcap_weapon(i, shown); if (PD.weapon && g_party_id[i] > 0) PD.weapon(g_party_id[i], shown); }
static const PHooks PARTY_HOOKS = {party_dmg_cb, party_hit_cb, party_heal_cb, party_fx_cb, party_fx_move_cb,
                                   party_fx_end_cb, party_weapon_cb, party_sound_cb};

// Draw member i: the 8-frame blend from the frozen last pose (sora.c's glide_pose_draw maths) or the plain pose.
static void party_draw_member(int i, float to_u) {
    PMember *m = &P.m[i];
    float clock;
    int id = g_party_id[i], slot = party_draw_slot(i, &clock), nb = PD.bones ? PD.bones(id) : 0;
    float floor_y = m->kd == KD_AIR ? m->floor_t : m->floor_s;
    bool wet_now = false;  // (slice 3: what the SNAP's party block carries)
    float sh[5];
    bool has_sh = false;
    PD.shadow(id, floor_y, blob_str(0.55f));  // (in the air: on the floor under him)
    if (PD.ghost) {  // (s87c party water) no per-actor water query exists: Link's surface y vs THEIR feet, drawn at Sora's ghost alpha
        static bool wet_was[PARTY_N];
        // (s87c review: > 3 u under his surface = ground below a ledge / bank, not his water - no X-ray silhouette there)
        float sy = link_water_surface();
        bool wet = g_party_wet && m->pos[1] < sy - 0.02f && m->pos[1] > sy - 3.0f;
        if (wet != wet_was[i]) LOG("[party] member %d %s the water (feet y %.2f)\n", i, wet ? "in" : "out of", m->pos[1]);
        wet_was[i] = wet;
        PD.ghost(id, wet ? g_swim_ghost / 100.0f : 0.0f);
        wet_now = wet && g_swim_ghost > 0;
    }
    if (PD.weapon_at) {  // (s58: Goofy Bash's shield in flight, spinning with KH2's W_EX030 motion 201)
        float wp[3], wy, wt;
        if (pc_bash_shield(i, wp, &wy, &wt)) {
            const float *wb = pwf_sample(i, wt);
            PD.weapon_at(id, wb, wb ? g_pwf[i].nb : 0, wp, wy, to_u), g_pw_on[i] = true;
            memcpy(sh, wp, sizeof wp), sh[3] = wy, sh[4] = wt, has_sh = true;
        } else if (g_pw_on[i])
            PD.weapon_at(id, NULL, 0, NULL, 0, 0), g_pw_on[i] = false;
    }
    // (s58) the catch's blend moves the body only: its weapon bones start where the flying shield ended (A402 f0)
    int wb0 = m->act == PACT_CAST && m->act_spell == PS_BASH && m->act_phase == 1 && g_pwf[i].nb > 0 ? nb - g_pwf[i].nb : nb;
    bool can = nb > 0 && nb <= PARTY_MAX_BONES;
    if (can && m->blend == 0) {  // (the switch: the from-pose = what was drawn last)
        if (!(m->blend_from_blend && g_pb_cur_ok[i]))
            g_pb_cur_ok[i] = PD.slot_pose(id, m->from_slot, m->from_clock, g_pb_cur[i]);
        if (g_pb_cur_ok[i]) memcpy(g_pb_from[i], g_pb_cur[i], sizeof(float) * 12 * (size_t)nb);
        else m->blend = -1;
    }
    if (can && m->blend >= 0 && PD.slot_pose(id, slot, clock, g_pb_cur[i])) {
        float w = m->blend / party_blend_len(m);
        for (int k = 0; k < wb0; k++) {
            float *d = g_pb_cur[i] + k * 12, *a = g_pb_from[i] + k * 12, len[3];
            for (int r = 0; r < 3; r++) {
                float la = sqrtf(v3dot(a + 3 * r, a + 3 * r)), lb = sqrtf(v3dot(d + 3 * r, d + 3 * r));
                len[r] = la + (lb - la) * w;
            }
            for (int j = 0; j < 12; j++) d[j] = a[j] + (d[j] - a[j]) * w;
            ortho_rows(d, len);
        }
        g_pb_cur_ok[i] = true;
        PD.world(id, g_pb_cur[i], m->pos, m->face, to_u);
        pcap_commit(i, m->pos, m->face, slot, clock, floor_y, wet_now, true, m->from_slot, m->from_clock, w, has_sh ? sh : NULL);
        return;
    }
    g_pb_cur_ok[i] = false;
    PD.pose(id, slot, clock, m->pos, m->face, to_u);
    pcap_commit(i, m->pos, m->face, slot, clock, floor_y, wet_now, false, 0, 0, 0, has_sh ? sh : NULL);
}

// Each driving frame after Sora's own draw (replaces slice 2's fixed idle placement); also from party_other while
// Link ragdolls. st = Link's state this frame. A ragdoll START (linkpos_link_ragdoll 0 -> > 0: a bomb, a big hit)
// blows every member within P.kd_radius_cm of Link's feet away from them.
static void party_tick(float kh, float to_u, int st) {
    float lm[12];
    if ((g_party_id[0] < 0 && g_party_id[1] < 0) || !P.on || !linkpos_link_mtx(lm)) return;
    PIn in = {{lm[3], lm[7], lm[11]}, S.face, st == LINK_GROUND && !ACT_AIR(S.act) && !S.air_frames, kh, to_u,
              vk_frame_counter(), party_ray_cb};
    static PEnemy en[PC_ENEMY_MAX];  // (s55: linkpos's enemy list this present)
    {
        uint64_t a[PC_ENEMY_MAX];
        float p[PC_ENEMY_MAX][3];
        int32_t l[PC_ENEMY_MAX];
        int n = P.combat ? linkpos_enemies(a, p, l, PC_ENEMY_MAX) : 0;
        for (int k = 0; k < n; k++) en[k].actor = a[k], memcpy(en[k].pos, p[k], sizeof en[k].pos), en[k].life = l[k];
        in.en = en, in.nen = n < 0 ? 0 : n, in.hit_r = g_hit_r, in.hit_h = g_hit_h;
        in.sora_hp = (float)linkpos_link_life(), in.sora_hp_max = (float)g_hud_life_max, in.hk = &PARTY_HOOKS;
    }
    static int rag_was;
    int rag = linkpos_link_ragdoll();
    bool rag_start = rag > 0 && rag_was <= 0;
    rag_was = rag;
    if (g_party_warp_now) {
        g_party_warp_now = false;
        for (int i = 0; i < PARTY_N; i++)
            if (P.m[i].placed) party_place(i, &in, "live warp");
    }
    if (!party_core_tick(&in)) return;
    if (rag_start) {
        int n = party_blast_core(in.feet, P.kd_radius_cm, to_u);
        LOG("[party] Link ragdoll %d at %.3f %.3f %.3f: %d member(s) blown (radius %.0f cm)\n", rag, in.feet[0], in.feet[1],
            in.feet[2], n, P.kd_radius_cm);
    }
    if (g_party_blow_now > 0) {  // (the live test: both, whatever the distance)
        for (int i = 0; i < PARTY_N; i++) party_knockdown_core(i, in.feet, g_party_blow_now, to_u);
        g_party_blow_now = 0;
    }
    bool say = P.log && P.ticks % 60 == 1;
    for (int i = 0; i < PARTY_N; i++) {
        if (g_party_id[i] < 0) continue;
        if (g_party_absorbed[i]) { if (PD.off) PD.off(g_party_id[i]); continue; }  // (s68 drive)
        if (g_plim_hold[i]) continue;  // (s133 limits: sora_plimit.h draws him)
        party_draw_member(i, to_u);
        psfx_tick(i, in.present);  // (s56: the KH2 sounds of the motion he plays)
        PMember *m = &P.m[i];
        float clock;
        int slot = party_draw_slot(i, &clock);
        if (say && P.battle)
            LOG("[party] %s battle: target %llx d=%.0f h=%.0f%s, %s, mp %.0f%s, hits %d, lv %d\n", PARTY[i].name,
                (unsigned long long)m->tgt_actor, m->tgt >= 0 ? m->t_d : -1.0f, m->tgt >= 0 ? -m->t_dy : 0.0f,
                m->leash ? " (leash)" : "", m->act ? PENT[i][m->act_entry >= 0 ? m->act_entry : 0].name : m->cur >= 0 ? PENT[i][m->cur].name : "loco",
                m->mp, m->charge > 0 ? " (charging)" : "", m->hits, party_level(i));
        if (say)
            LOG("[party] %s d=%.0f band=%s y=%.3f (%s) motion %s slot %d clock %.1f%s at %.3f %.3f face %.2f | Sora "
                "%.3f %.3f %.3f%s, trail %d, casts %llu hits %llu\n",
                PARTY[i].name, m->d_cm, PARTY_BAND_NAME[m->band], m->pos[1], PARTY_SRC_NAME[m->src],
                m->kd == KD_AIR ? "blown" : m->kd == KD_DOWN ? "down" : PARTY_BAND_NAME[m->motion], slot, clock,
                m->blend >= 0 ? " (blend)" : "", m->pos[0], m->pos[2], m->face, in.feet[0], in.feet[1], in.feet[2],
                in.grounded ? "" : " (air)", P.trail.n, (unsigned long long)P.casts, (unsigned long long)P.hits);
    }
}

// sora.c's "Link's own state" branch (Sora not driven). hard = dead / loading: both off. Else while Link ragdolls
// (BotW's ragdoll may report as OTHER) or a member is still blown / getting up: they keep ticking and drawing (the
// follow aims at Link's feet, nothing is pushed to the trail); otherwise (climb, swim, puppet, cutscene, warp 0x1)
// both off once per stretch (slice 2 left them frozen there).
static void party_other(bool hard) {
    bool kd = false;
    for (int i = 0; i < PARTY_N; i++) kd = kd || P.m[i].kd != KD_NONE;
    if (!hard && !P.hidden && (kd || linkpos_link_ragdoll() > 0)) {
        float ratio = framepace_game_ratio();
        if (!(ratio > 0.1f && ratio < 3)) ratio = 1;
        party_tick(KH_PHYS_STEP * ratio, 0.01f * g_k, LINK_OTHER);
        return;
    }
    if (party_core_hide(vk_frame_counter())) party_all_off();
}
#endif

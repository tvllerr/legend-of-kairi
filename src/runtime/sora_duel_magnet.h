// (s289 duelmagnet, duel plan 2.4 with the main session's changes; the user's pick "Pull + hit, can break free") Magnet on the
// duel opponent: the caster's Magnet pulls him and hits him once, he can break free with Guard / Dodge Roll / Aerial Dodge /
// Quick Run, and after any end of a pull he cannot be pulled again for 3 s. The PURE parts (host test runtime/test/duel_test.c
// magnet_tests). The game side is sora_duel.h (du_magnet_lock / du_magnet_pull for the caster, du_mag_* for the victim); the
// caster's hooks sit in sora_magic2.h mg_magnet_spawn / mg_magnet_tick, the victim's one sora.c line right after the driving
// tick's velocity out (du_mag_drive).
// KH2 (recon/s289/S289_HELPER_MAGNETRE.md, VERIFIED in the KH2 FM exe + 00battle.bin / MAGNET_1..3.mag):
//   * the grab: the sphere (radius 250 / 500 / 800) against the target's own hit shapes, sum of radii; reaction 5 IS the grab
//     (TORNADO, no flinch first); Power 100 once per cast (atkp 658, Interval 0);
//   * the hold radius rolled once at the grab (KB1..KB2 40..120 / 70..170 / 100..220), the swirl -3 / -6 / -12;
//   * every frame: v = clamp((d - hold) x 0.1, -20, 20) toward the 3D centre, REPLACING his velocity, + the flat tangent x swirl;
//     no gravity; he turns to face the centre;
//   * the window: 16 + grow + hold = 167 / 190 / 213 frames from the shot;
//   * KH2 lets go only at the sphere's end, on a blow-class hit, a landing (ground TORNADO) or death. It has no break-free, no
//     distance end and no immunity: those are DUEL rules here (the user's pick and the main session's brief), not KH2.
// The duel rules:
//   * PULL (op 7) follows the HIT (op 3) it belongs to, with that HIT's hseq. The victim's game applies it only if it TOOK that
//     HIT (not blocked) in this duel's FIGHT, once (its OWN seen list: the HIT's DuSeen would call every PULL a copy), not while
//     he is held already, not within 3 s of the last pull's end. A PULL seen before its HIT is not marked, so its UDP copy after
//     the HIT still counts. An older game drops op 7 as a "bad" packet and still takes the HIT (net_unpack_ex's range).
//   * ends: the window, a duel finisher on him or BotW's ragdoll (KH2's blow class ends a hold), Guard / Dodge Roll / Quick Run
//     (Square) or the Aerial Dodge (Cross in the air) pressed on HIS game, KO, the duel's end (a load ends the duel), duel 0 /
//     duelmagnet 0 / a goto / No Clip / the board / no driving frame for 1 s (Link has the pad), his feet more than 2 m beyond
//     the sphere radius from the centre (a warp). After ANY end: no new pull on him for DU_MAG_IMMUNE_MS.
// Live keys (sora_duel.h du_live_key): duelmagnet 0|1 (default 1 since s290; 0 = Magnet ignores the duel opponent, as before), duelmagnetlift 1|0
// (default 1 = KH2's lift: the 3D pull toward the raised centre, he floats; 0 = the same pull in the ground plane, he stays down).
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sora_net_proto.h"
#include "sora_duel_core.h"

#define DU_MAG_POW 100          // KH2 atkp 658's Power (= sora_magic2.h MG_MAG_POW)
#define DU_MAG_ATKP 658         // KH2's Magnet pull attack row (its hit sound)
#define DU_MAG_REACT 5          // atkp 658's EnemyReaction: the grab (the victim's game does not read it)
#define DU_MAG_RATE 0.1f        // KH2 re:0x6239F8 (= MG_PULL_RATE)
#define DU_MAG_VMAX 20.0f       // KH2 re:0x623BC8, cm per KH frame (= MG_PULL_MAX)
#define DU_MAG_IMMUNE_MS 3000   // (the user's rule) no new pull on him for 3 s after any end
#define DU_MAG_WARP_CM 200.0f   // (the brief's failsafe) his feet this far beyond the sphere radius from the centre = a warp
#define DU_MAG_NODRIVE_MS 1000  // a pull with no driving frame this long, unpaused (Link has the pad: a climb / swim / event), ends
#define DU_MAG_MAX_FRAMES 600   // a PULL's window above this (10 s at 60) is refused as bad (KH2's longest is 213)
#define DU_MAG_NHIT 8           // the Magnet HITs remembered (taken / blocked) for their PULLs
// the held pose (GUESS, the user's to judge): KH2's TORNADO motions 0x14 / 0x15 (motion 20 / 21) are DUMM in P_EX100.mset (KH2's
// Sora has none: KH2 never pulls the player). The closest Sora has: motion 14, the air flinch FRONT (he faces the centre), 36 frames,
// no loop, triggers at frames 0 and 36 only (data/sora_motion.bin), its clock held at frame 8 (where its range bits leave the
// hit stun). Not the blow loops 16 / 18: sora.c's blow landing plays their get-up whenever he touches the ground.
#define DU_MAG_MOT 14
#define DU_MAG_FRAME 8.0f

// KH2's grab, sum of radii (MAGNETRE 1): the sphere (R cm) against his own hit shape = KH2's Sora target part (sora_duel_core.h
// DU_BODY_*: a sphere 70 cm above his feet, radius 55, VERIFIED KH2 re:0x3D7520). c, feet: BotW units; to_u: units per cm.
static inline bool du_mag_grab(const float c[3], const float feet[3], float R_cm, float to_u) {
    if (!(R_cm > 0) || !(to_u > 0)) return false;
    float h = (DU_BODY_Y0_CM + 0.5f * DU_BODY_H_CM) * to_u, r = (R_cm + DU_BODY_R_CM) * to_u;
    float dx = c[0] - feet[0], dy = c[1] - (feet[1] + h), dz = c[2] - feet[2];
    return dx * dx + dy * dy + dz * dz <= r * r;
}

// KH2's step (FUN_14040f8e0), cm per KH frame, from his feet (KH2's "target position"; which body point it is was not traced:
// GUESS the feet, as mg_magnet_tick does and MAGNETRE's own 127 cm example) toward the centre: clamp((d - hold) x 0.1, +-20) along
// the line, plus the flat tangent (RotY(pi/2) of the line, y 0; mg_magnet_tick's handedness, a GUESS against KH2's axes) x swirl.
// lift = 3D (KH2); !lift = the same in the ground plane (the centre's height ignored, v[1] = 0). -> d, the distance used (cm).
static inline float du_mag_step(const float feet[3], const float c[3], float hold, float swirl, bool lift, float to_u, float v[3]) {
    v[0] = v[1] = v[2] = 0;
    if (!(to_u > 0)) return 0;
    float d[3] = {(c[0] - feet[0]) / to_u, lift ? (c[1] - feet[1]) / to_u : 0.0f, (c[2] - feet[2]) / to_u};
    float dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (!(dist > 1e-3f)) return dist;  // (on the centre: no direction; GUESS 0, as mg_magnet_tick)
    float u[3] = {d[0] / dist, d[1] / dist, d[2] / dist};
    float s = (dist - hold) * DU_MAG_RATE;
    s = s > DU_MAG_VMAX ? DU_MAG_VMAX : s < -DU_MAG_VMAX ? -DU_MAG_VMAX : s;
    float tl = sqrtf(u[0] * u[0] + u[2] * u[2]), tx = tl > 1e-4f ? u[2] / tl : 0, tz = tl > 1e-4f ? -u[0] / tl : 0;
    v[0] = u[0] * s + tx * swirl, v[1] = u[1] * s, v[2] = u[2] * s + tz * swirl;
    return dist;
}

// the Magnet HIT (duel plan 2.4): src 6, Power 100, atkp 658, a spell (S = the caster's Mag + Keyblade Mag), reaction 5, no
// finisher, no boost (KH2 has no Magnet Boost). du_hit_out fills op / id / hseq / from / at.
static inline void du_mag_hit_msg(NetDuelMsg *m, int stat) {
    memset(m, 0, sizeof *m);
    m->power = DU_MAG_POW, m->src = DU_SRC_MAGNET, m->flags = NDH_SPELL, m->react = DU_MAG_REACT, m->atkp = DU_MAG_ATKP;
    m->stat = (uint16_t)(stat < 0 ? 0 : stat > 65535 ? 65535 : stat);
}
// the PULL after that HIT: its hseq, the centre (BotW units), the sphere radius, the hold radius rolled at the grab (cm), the
// swirl (cm per KH frame), the KH frames left in the cast's pull window, the tier (0..2)
static inline void du_mag_pull_msg(NetDuelMsg *q, uint32_t id, uint16_t hseq, const float c[3], int R_cm, float hold, int swirl, float left, int tier) {
    memset(q, 0, sizeof *q);
    q->op = NDO_PULL, q->id = id, q->hseq = hseq;
    memcpy(q->c, c, sizeof q->c);
    q->radius_cm = (uint16_t)(R_cm < 1 ? 1 : R_cm > 3000 ? 3000 : R_cm);
    q->hold_cm = (uint16_t)(!(hold > 0) ? 0 : hold > 3000 ? 3000 : lroundf(hold));
    q->swirl = (int16_t)(swirl < -100 ? -100 : swirl > 100 ? 100 : swirl);
    float f = ceilf(left);
    q->frames = (uint16_t)(!(f >= 1) ? 1 : f > DU_MAG_MAX_FRAMES ? DU_MAG_MAX_FRAMES : f);
    q->tier = (uint8_t)(tier < 0 ? 0 : tier > 2 ? 2 : tier);
}

// ---- the victim's side: his pull, the Magnet HITs his game took or blocked, the PULLs it judged ----
enum { DUM_END_NONE = 0, DUM_END_WINDOW, DUM_END_FINISH, DUM_END_BREAK, DUM_END_KO, DUM_END_DUEL, DUM_END_OFF, DUM_END_WARP, DUM_END_N };
static const char *const DUM_END_NAME[DUM_END_N] = {"-", "the window ended", "knocked out of it (a finisher / BotW's ragdoll)", "broken", "KO",
                                                    "the duel ended", "off (duel 0 / duelmagnet 0 / a goto / No Clip / the board / no driving frame)",
                                                    "too far from the centre (a warp?)"};
enum { DUM_BRK_NONE = 0, DUM_BRK_GUARD, DUM_BRK_ROLL, DUM_BRK_QRUN, DUM_BRK_ADODGE, DUM_BRK_N };
static const char *const DUM_BRK_NAME[DUM_BRK_N] = {"-", "Guard", "Dodge Roll", "Quick Run", "Aerial Dodge"};
enum { DUMP_OK = 0, DUMP_OFF, DUMP_OTHER, DUMP_FIGHT, DUMP_REPEAT, DUMP_NO_HIT, DUMP_BAD, DUMP_BLOCKED, DUMP_HELD, DUMP_IMMUNE, DUMP_N };
static const char *const DUMP_WHY[DUMP_N] = {"ok", "duelmagnet is 0 here", "another duel", "no fight", "a copy", "its hit not taken (yet)",
                                             "bad values", "its hit was blocked", "already held", "immune"};
typedef struct {
    bool on, lift;
    uint16_t hseq;               // the HIT this pull came with
    float c[3];                  // the centre (BotW units)
    float radius, hold, swirl;   // cm, cm, cm per KH frame
    float left;                  // KH frames left in the window (driven frames only: a pause waits)
    int tier, why, brk;          // the last end (DUM_END_*) and its break move (DUM_BRK_*)
    uint64_t t0, t_end, immune_until, last_drive;  // ms (immune_until 0 = none)
    float dist;                  // cm from the centre at the last tick
    int nframes;                 // driven frames held
    struct { uint16_t hseq; uint8_t st; } hit[DU_MAG_NHIT];  // st 1 = taken, 2 = blocked / not applied
    int nhit;                    // (the ring's next slot)
    DuSeen seen;                 // the PULLs judged for good (their OWN list)
} DuMagPull;

static inline void du_mag_reset(DuMagPull *p) { memset(p, 0, sizeof *p); }  // (a new duel: no hold, no immunity, no lists)
// du_take_hit: a Magnet HIT (src 6) taken (true) or blocked / not applied (false); a copy of the same hseq overwrites
static inline void du_mag_note_hit(DuMagPull *p, uint16_t hseq, bool taken) {
    for (int k = 0; k < DU_MAG_NHIT; k++)
        if (p->hit[k].st && p->hit[k].hseq == hseq) { p->hit[k].st = taken ? 1 : 2; return; }
    int i = p->nhit < 0 || p->nhit >= DU_MAG_NHIT ? 0 : p->nhit;
    p->hit[i].hseq = hseq, p->hit[i].st = taken ? 1 : 2, p->nhit = (i + 1) % DU_MAG_NHIT;
}
static inline int du_mag_hit_st(const DuMagPull *p, uint16_t hseq) {  // 0 not seen, 1 taken, 2 blocked
    for (int k = 0; k < DU_MAG_NHIT; k++)
        if (p->hit[k].st && p->hit[k].hseq == hseq) return p->hit[k].st;
    return 0;
}
static inline bool du_mag_seen(const DuSeen *d, uint16_t hseq) {
    for (int k = 0; k < d->n && k < 32; k++)
        if (d->s[k] == hseq) return true;
    return false;
}
// a PULL that came in -> DUMP_OK = the pull starts now; else why not. duel_id = this duel's (0: not ours), fight = its FIGHT runs,
// key = duelmagnet here, lift = duelmagnetlift here (the victim's game moves his body: its own keys decide)
static inline int du_mag_accept(DuMagPull *p, const NetDuelMsg *m, uint32_t duel_id, bool fight, bool key, bool lift, uint64_t now) {
    if (!key) return DUMP_OFF;
    if (!m || !duel_id || m->id != duel_id) return DUMP_OTHER;
    if (!fight) return DUMP_FIGHT;
    if (du_mag_seen(&p->seen, m->hseq)) return DUMP_REPEAT;
    int st = du_mag_hit_st(p, m->hseq);
    if (!st) return DUMP_NO_HIT;  // (not marked: a copy after its HIT counts)
    du_seen_add(&p->seen, m->hseq);
    if (m->radius_cm < 1 || m->radius_cm > 3000 || m->hold_cm > m->radius_cm || m->frames < 1 || m->frames > DU_MAG_MAX_FRAMES ||
        m->tier > 2 || m->swirl < -100 || m->swirl > 100 || !isfinite(m->c[0]) || !isfinite(m->c[1]) || !isfinite(m->c[2]))
        return DUMP_BAD;
    if (st != 1) return DUMP_BLOCKED;
    if (p->on) return DUMP_HELD;
    if (p->immune_until && now < p->immune_until) return DUMP_IMMUNE;
    p->on = true, p->lift = lift, p->hseq = m->hseq;
    memcpy(p->c, m->c, sizeof p->c);
    p->radius = m->radius_cm, p->hold = m->hold_cm, p->swirl = m->swirl, p->left = m->frames, p->tier = m->tier;
    p->why = DUM_END_NONE, p->brk = DUM_BRK_NONE, p->t0 = p->last_drive = now, p->dist = 0, p->nframes = 0;
    return DUMP_OK;
}
// any end: the 3 s immunity starts (no-op when no pull runs)
static inline bool du_mag_end(DuMagPull *p, int why, int brk, uint64_t now) {
    if (!p->on) return false;
    p->on = false, p->why = why, p->brk = brk, p->t_end = now, p->immune_until = now + DU_MAG_IMMUNE_MS;
    return true;
}
// one driven frame of a held pull (paused / loading frames never get here, so the window waits): his feet (BotW units).
// -> DUM_END_NONE with v = KH2's step (cm per KH frame), or the end it just took (the warp failsafe, the window).
static inline int du_mag_tick(DuMagPull *p, float kh, const float feet[3], float to_u, uint64_t now, float v[3]) {
    v[0] = v[1] = v[2] = 0;
    if (!p->on) return DUM_END_NONE;
    p->last_drive = now;
    float dx = (p->c[0] - feet[0]) / to_u, dy = (p->c[1] - feet[1]) / to_u, dz = (p->c[2] - feet[2]) / to_u;
    p->dist = sqrtf(dx * dx + dy * dy + dz * dz);
    if (!(p->dist <= p->radius + DU_MAG_WARP_CM)) { du_mag_end(p, DUM_END_WARP, DUM_BRK_NONE, now); return DUM_END_WARP; }  // (NaN too)
    p->left -= kh > 0 ? kh : 0;
    if (p->left <= 0) { du_mag_end(p, DUM_END_WINDOW, DUM_BRK_NONE, now); return DUM_END_WINDOW; }
    du_mag_step(feet, p->c, p->hold, p->swirl, p->lift, to_u, v);
    p->nframes++;
    return DUM_END_NONE;
}
// the no-driving-frame rule (sora_duel.h du_frame, after the driving tick): true = end it (DUM_END_OFF). A paused game never
// ends it (the window waits with the game); the first unpaused frame's driving tick runs before du_frame and drives it again.
static inline bool du_mag_stale(const DuMagPull *p, bool paused, uint64_t now) {
    return p->on && !paused && now >= p->last_drive + DU_MAG_NODRIVE_MS;
}
// the break-free press (read raw on HIS game, before the held reaction drops it): Square = the button of Guard (stick neutral),
// the Dodge Roll (stick pushed) and Quick Run (stick pushed with no Dodge Roll but Quick Run: KH2's Square order, sora_qrun.h),
// anywhere; Cross IN THE AIR = the Aerial Dodge's button (glidebtn 1). Cross on the ground is a jump: not on the user's list.
static inline int du_mag_break(bool square, bool cross, bool air, bool stick, bool roll, bool qrun) {
    if (square) return !stick ? DUM_BRK_GUARD : !roll && qrun ? DUM_BRK_QRUN : DUM_BRK_ROLL;
    return cross && air ? DUM_BRK_ADODGE : DUM_BRK_NONE;
}

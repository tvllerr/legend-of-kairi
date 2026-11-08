// Sora-in-BotW: DRAFT for sora.c (not built, not included anywhere): KH2's attack targets. The lock-on block's auto
// target (re:0x3BE4B0 / re:0x3DCD70), the ptya Type 1 / Type 2 range test (re:0x4029F0 / re:0x4027F0), the select with
// targets (re:0x4028D0 / re:0x402D20), the attack start's lunge and jump start (re:0x3D78D0) and the turn_to_lock
// turn (range 28, re:0x3D4B10 / re:0x3B9F70). Every rule and its source: D:/SoraBotW/recon/TARGET_SELECT_RECON.md
// (sections cited as TSR x.y). Written to drop into sora.c after ptya_select (line 536): it uses S, kh_ptya, NPTYA,
// ptya_eligible, ptya_pick, ptya_index, arc, enter_jump, wrap, linkpos_enemies, g_hit_r / g_hit_h, env_f, LOG.
//
// Units as sora.c: KH cm and KH frames inside, BotW u at the linkpos boundary (1 u = 100 cm / g_k; cm = u / to_u).
// Horizontal axes stay BotW's (x, z) with Sora's facing direction (sin face, cos face), as sora.c's dir[]; the one
// vertical number the rules read is KH2's, y DOWN: dy = -(height above Sora's feet) (TSR 4.1).
//
// HOOKS (sora_mod_frame, by the step numbers of sora.c):
//   init        after hits_load(): `targets_init(0.01f * g_k);` (the enemy part = sora.c's hit cylinder; new env
//               SAVAGEWIND_SORA_TARGETS=0 turns the targets off and keeps Type 3 only, as today)
//   step 2      after `bits` is known (sora.c 707) and before the presses: `Target tg[64]; int ntg = targets_build(p,
//               to_u, tg, 64); lock_tick(tg, ntg, kh, bits, S.face);` (the block ticks in the player's pre-tick,
//               re:0x3DD280, before the press)
//   step 3      the attack press: `ptya_select(bits, false)` -> `ptya_select_t(bits, false, tg, ntg)`; drop the
//               nearest_enemy / stick snap (sora.c 735-737, TSR 7.4); after `play(e->motion), S.special = SP_ATTACK`:
//               `attack_start_t(e, tg, ntg, z_up, false, bits); turn_to_lock_once(tg, ntg);` - the first sets S.vel
//               for a lunge (base Sora: never, TSR 6.1) or starts the air arc of a Jump > 0 entry (TSR 6.2), the
//               second fires a frame-0 turn_to_lockon frame trigger (A332)
//   step 4      before the stick turn (sora.c 762): `turn_to_lock(bits, kh, tg, ntg);` - it owns the facing while
//               the window runs; the stick turn does not run then anyway (speed is 0 in a swing)
//   step 5      a lunge's S.vel only survives the ground branch (sora.c 770 overwrites S.vel = dir * speed) while
//               RB_KEEP_KINETICS is on: `if (S.special && (bits & RB_KEEP_KINETICS)) { keep S.vel } else ...`; no base
//               Sora motion has the range, so this hook can wait for the Drive Forms
//   air         the press (sora.c 731) runs only while ACT_IDLE; the air entries need it while ACT_JUMP too with
//               air = true (KH_COMBO_AIR in finisher_ok / combo_over, and a landing mid-attack: KH2's jump_land
//               trigger, not sora.c's MOT_LAND cut) - out of this draft
//   log         add g_lock.actor and the pick's type to the sora.txt line if wanted
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

// ------------------------------------------------------------------ KH2 numbers (TSR 1: plyr row 0, P_EX100.mdlx)
#define KH_AUTO_RANGE 300.0f     // plyr AutoLockonRange, cm (03system.bin@0x175D0 row 0)
#define KH_AUTO_MIN_H -435.0f    // plyr AutoLockonMinHeight (KH y down: 435 cm above the part)
#define KH_AUTO_MAX_H 50.0f      // plyr AutoLockonMaxHeight (50 cm below)
#define KH_AUTO_TIME 60.0f       // plyr AutoLockonTime, frames (the hold while a turn_to_lock window runs)
#define KH_AUTO_H_ADJ 0.4f       // plyr AutoLockonHeightAdjust
#define KH_AUTO_INNER 0.25f      // plyr AutoLockonInnerAdjust
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
} Target;

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
        t->radius = g_part_r, t->height = g_part_h;
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
static const Target *target_auto(const Target *t, int n, float face) {
    float fx = sinf(face), fz = cosf(face), best_s = 0;
    const Target *best = NULL;
    for (int i = 0; i < n; i++) {
        const Target *e = &t[i];
        if (e->dy < KH_AUTO_MIN_H - e->height || e->dy > KH_AUTO_MAX_H + e->height || e->hd - e->radius > KH_AUTO_RANGE)
            continue;
        float cosang = e->hd > 0 ? (e->d[0] * fx + e->d[1] * fz) / e->hd : 1;
        float s = (e->hd + fabsf(e->dy) * KH_AUTO_H_ADJ) * ((1 - cosang) * 0.5f * (1 - KH_AUTO_INNER) + KH_AUTO_INNER);
        if (!best || s < best_s) best = e, best_s = s;
    }
    return best;
}

static struct {
    uint64_t actor;  // the block's target (0: none); AUTO only - a LOCK state (R1 / BotW's ZL) would set `locked`
    float timer;     // AutoLockonTime hold (re:0x3DCCD0 fills it while a turn_to_lock window runs)
    bool locked;
} g_lock;

// re:0x3DD280 + re:0x3DCD70 each frame (TSR 3.2): a gone / dead target is dropped; in AUTO the target is kept while
// the timer runs, else the search runs again and a better part replaces it; entering AUTO leaves the timer at 0, so
// outside swings the auto target is simply the best-scored part every frame.
static void lock_tick(const Target *t, int n, float kh, uint32_t bits_prev, float face) {
    const Target *cur = g_lock.actor ? target_of(t, n, g_lock.actor) : NULL;
    if (!cur) g_lock.actor = 0, g_lock.timer = 0, g_lock.locked = false;
    if (g_lock.locked) return;  // (LOCK: kept while targetable; release / switch rules not drafted)
    if (S.special && (bits_prev & RB_TURN_TO_LOCK) && cur) g_lock.timer = KH_AUTO_TIME;  // re:0x3DCCD0
    if (cur && g_lock.timer > 0) { g_lock.timer -= kh; return; }
    const Target *p = target_auto(t, n, face);
    if (p) {
        if (p->actor != g_lock.actor) LOG("[sora] auto target %llx (%.0f cm, dy %.0f)\n", (unsigned long long)p->actor, p->hd, p->dy);
        g_lock.actor = p->actor;  // (re:0x3DCCF0: a different part replaces it, same state)
    } else
        g_lock.actor = 0, g_lock.timer = 0;
}

// ------------------------------------------------------------------ Type 1 / 2 marks (re:0x4029F0 + re:0x4027F0)
// TSR 4.2: Near <= dist < Far, High' < dy <= Low', InnerMin < cos <= InnerMax, the Low / High window widened by the
// target's half-height (narrowed with the LOW_MINUS / HIGH_PLUS flags), High raised by the jump height with JUMP_HIGH.
static bool ptya_in_range(const KhPtya *e, float dist, float dy, float cosang, float height) {
    float hi = e->high, lo = e->low;
    if (e->flag & PTYA_JUMP_HIGH) hi -= KH_JUMP_H;  // (re:0x3B5A40: High Jump would raise it)
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
        const KhPtya *e = &kh_ptya[i];
        if (e->type == type && ptya_eligible(e, bits, air) && ptya_in_range(e, dist, t->dy, cosang, t->height))
            marks[i] = true;
    }
}

// re:0x4028D0 + re:0x402D20 with targets (replaces sora.c's ptya_select): Type 2 per targetable enemy, Type 1 the
// block's target, Type 3 always; no wall probe (Id 1) yet. The pick and the Id 5 / 9 default are sora.c's.
static const KhPtya *ptya_select_t(uint32_t bits, bool air, const Target *t, int n) {
    bool marks[NPTYA];
    memset(marks, 0, sizeof marks);
    const Target *lock = g_targets_on && g_lock.actor ? target_of(t, n, g_lock.actor) : NULL;
    float dir[2] = {sinf(S.face), cosf(S.face)};
    if (lock && lock->hd > 0) dir[0] = lock->d[0] / lock->hd, dir[1] = lock->d[1] / lock->hd;
    if (g_targets_on)
        for (int i = 0; i < n; i++) ptya_mark_target(marks, &t[i], dir, PTYA_ENEMY, bits, air);
    if (lock) ptya_mark_target(marks, lock, dir, PTYA_LOCK, bits, air);
    for (int i = 0; i < NPTYA; i++)
        if (kh_ptya[i].type == PTYA_ALWAYS && ptya_eligible(&kh_ptya[i], bits, air)) marks[i] = true;
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
        float jmax = e->jump_max + ((e->flag & PTYA_JUMP_HIGH) ? KH_JUMP_H : 0);
        h = ptya_clamp(-bd[1], e->jump_min, jmax);  // the target centre's height above Sora's part centre
        float hn = sqrtf(bd[0] * bd[0] + bd[2] * bd[2]);
        dir[0] = hn > 0 ? bd[0] / hn : sinf(S.face), dir[1] = hn > 0 ? bd[2] / hn : cosf(S.face);
        float apex = sqrtf(fabsf(2 * h / KH_G));  // re:0x3B73F0
        speed = ptya_clamp(apex > 0 ? ((hn - radii) + e->dist_adjust) / apex : e->speed_max, e->speed_min, e->speed_max);
    }
    arc(h);  // the ACTION's jump start (vtable +0x18 = re:0x3CBB00): the arc from here, no take-off wind-up
    S.hvel[0] = dir[0] * speed, S.hvel[1] = dir[1] * speed;
    S.takeoff = 0;
    enter_jump(z_up, true);
    S.jump_held = false;
    LOG("[sora] air attack jump h %.0f speed %.1f (ptya id %d)\n", h, speed, e->id);
}

// ------------------------------------------------------------------ turn_to_lock (range 28; TSR 7)
// The bake keeps the window as RB_TURN_TO_LOCK only (Program.cs:145-153 drop the degree param), so the rate comes
// from this side table (P_EX100.mset rows, sora_attacks.kh.json; frames motion-relative). Until sorabake emits it.
static const struct { uint16_t motion, start, end, deg; } kh_turn_lock[] = {
    {151, 0, 12, 15}, {152, 0, 12, 15}, {153, 0, 12, 15}, {153, 32, 44, 15}, {154, 0, 8, 22},
    {155, 0, 4, 45}, {155, 32, 38, 5}, {181, 0, 23, 15}, {182, 0, 22, 15}, {184, 0, 10, 18},
};  // (183 A332 has frame trigger 21 turn_to_lockon @0 param 360 instead: a full snap at its first frame)
#define KH_TURN_LOCK_DFLT_DEG 15

// re:0x3D4B10 -> re:0x3B9F70, every KH frame of the window: the horizontal line from Sora's feet to the block target's
// point; nothing within 50 cm; else the facing steps toward it by at most deg * pi/180 * 2 per frame. Steps `kh`
// frames at once (sora.c's frame = kh KH frames), as PHH does (kh2ctl.py:959).
static void turn_to_lock(uint32_t bits, float kh, const Target *t, int n) {
    if (!g_targets_on || !S.special || !(bits & RB_TURN_TO_LOCK) || !g_lock.actor) return;
    const Target *lock = target_of(t, n, g_lock.actor);
    if (!lock || lock->hd <= KH_TURN_MIN_DIST) return;
    int deg = KH_TURN_LOCK_DFLT_DEG;
    for (size_t i = 0; i < sizeof kh_turn_lock / sizeof *kh_turn_lock; i++)
        if (kh_turn_lock[i].motion == S.motion && S.clock >= kh_turn_lock[i].start && S.clock < kh_turn_lock[i].end)
            deg = kh_turn_lock[i].deg;
    float rate = (float)deg * PI / 180 * KH_TURN_MULT * kh, d = wrap(atan2f(lock->d[0], lock->d[1]) - S.face);
    S.face = wrap(S.face + (d > rate ? rate : d < -rate ? -rate : d));
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

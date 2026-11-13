// sora_kbbeam.h (s216 beam): the Sealing Blade's full-health sword beam. Report: recon/s216/S216_HELPER_BEAM.md.
//
// Wish (user): "the Sealing Blade gets the Master Sword's effect: at FULL HEALTH a swing sends flying slashes at the enemy".
// This is OUR crescent on the mod's own projectile + damage code (as the Wisdom shot / Blizzard shard), NOT BotW's PlayerBeam
// actor (the mod has no actor-spawn code). DLL only: no exe change, hot reloads.
//
// When: a normal Keyblade swing (S.special == SP_ATTACK: ground + air combo swings, finishers too unless "kbbeamfin 0") starts
// its first live hit window (power > 0) with the Sealing Blade in the swinging hand (kbfx_item(blade): main hand, Valor's left,
// Master / Final's 2nd blade; "set kbfxid 32257" forces it on every blade) and Sora at full health. ONE crescent per swing
// (S.swing): Valor's two blades or a Drive Form multi-window move still give one. Never: Limit commands (SP_LIMIT), magic,
// summons, Anti Form, Mickey, Roxas (his own Keyblade model, no KB_TAB alts), Link's BotW weapon in Sora's hand (g_wpn_cur).
// Full health = Link's life (quarter hearts) >= BotW's own max hearts (gdt MaxHartValue), capped by the KH2 HUD gauge's max
// (g_hud_life_max, sora.c hud_update: the HP the HUD shows = life / g_hud_life_max). The cap only matters when the HUD max is
// below MaxHartValue (never after hud_update raised it); MaxHartValue unread -> the HUD max alone. (g_hud_life_max only
// grows: after yellow hearts it stays above the real max, so the HUD max alone would never read "full" again.)
// Flight: from the blade tip, level (height clamped 60..150 KH cm above the feet, GUESS), along Sora's facing; it steers
// (turn-rate limited) toward the R1 lock-on target, else the nearest live enemy in a forward cone within range, else flies
// straight. Hits the FIRST enemy it touches once, then ends (no piercing: GUESS). Ends at its range or on the static world
// (linkpos_raycast: walls / ground; "kbbeamwall 0" = off).
// Damage: Sora's blade path (sl_hit Str formula = KH2 damage, kill credit, EXP; linkpos_damage_enemy = the exe's edmg ring
// for the shared HP; ah_hit = KH2 reaction + the multiplayer hit note; dg_hit = Drive; Valor EXP). Power = kbbeampow % (33)
// of the launching window's KH2 power. SOURCE (BotW, romfs Actor/Pack): Weapon_Sword_070 bgparamlist Attack.Power 30,
// Rod.MagicName PlayerBeam / MagicPower 10 (= PlayerBeam bgparamlist Attack.Power 10); awakened MasterSword.TrueFormAttackPower
// 60 / TrueFormMagicPower 20: the beam is 1/3 of a swing in both forms -> 33 % of the swing's own power (the swing still hits).
// Range 20 u = Rod.MagicRange 20.0 (BotW m), radius 0.3 u = Rod.MagicRadius 0.3 (added to the enemy body radius).
// Speed: BotW's data has none (MagicSpeed 0.0; MagicSpeedByThrow 50.0 = the thrown sword): GUESS 40 u/s.
// Assets by name, resolved lazily (and again after a hot reload: the statics reset): fx "kbs1_beam" (fallback "wisdom_bullet",
// the Wisdom Form shot), fx "kbs1_beam_hit" (fallback the Keyblade's hit spark kbfx_name("hit")), sfx "kbs1_beam" on launch
// (fallback "wisdom_shot"), sfx "kbs1_beam_hit" on impact (fallback sora_sfx_hit with the swing's atkp row).
// Live: kbbeam 0/1 (DEFAULT 1 since s216: verified by the user), kbbeamlog 0/1 ([kbbeam] launch / hit / end / refusal lines),
// kbbeamtest 1 (ONE-SHOT: one crescent now, any Keyblade / health / kbbeam; remove the line after it fired), kbbeamspd <u/s>,
// kbbeamrange <u>, kbbeamr <cm>, kbbeampow <%>, kbbeamcone <deg half angle>, kbbeamturn <mrad per KH frame>, kbbeamfin 0/1,
// kbbeamwall 0/1.
// (s217) Its look: the user saw the s216 crescent "always facing up" (an upright arch, yaw only). Now it lies in the plane of the
// swing that threw it (a flying sword slash): kbbeamroll 1 (DEFAULT) = roll from the blade tip's move across the flight at launch
// (kbb_roll_swing: sweep -> blade line -> flat for thrusts), 0 = the s216 upright arch, 2 = always flat; kbbeamrolldeg <deg> = a
// fixed roll for tests (-1 off). Fixed at launch, kept while it steers. Drawn with fx.bin's pre-rolled kbs1_beam_r000..r165
// (sora_fx.c is exe side: no roll there, no exe change); an fx.bin before s217 = the old crescent. Report: recon/s217/S217_HELPER_BEAM.md.
// Multiplayer: no packets. The damage reaches peers through the existing edmg / netreact paths (as every Sora hit); the
// crescent itself is drawn only here.
//
// The pure part (above KBBEAM_HOST's #ifndef) is host-tested: runtime/test/kbbeam_test.c.
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define KBB_SEALING 0x7e01  // sora_kb.h KB_TAB row 25 "Sealing Blade"
#define KBB_MAX 4           // crescents in flight at once (a fast combo; the oldest gives way)
#define KBB_FRAME_S (1.0f / 60.0f)  // one KH frame (kh counts them)

// ------------------------------------------------------------------ pure logic (host-tested)
typedef struct {
    bool on;           // live kbbeam
    int item;          // the swinging blade's Keyblade (kbfx_item)
    bool attack;       // S.special == SP_ATTACK (Limit commands are SP_LIMIT, magic SP_MAGIC)
    bool anti, mickey, roxas;
    bool link_weapon;  // this blade is Link's BotW weapon (blade 0 + g_wpn_cur.def)
    bool finisher, allow_fin;
    int power;         // the window's KH2 power (0 = guard / non-damage)
    int life, hud_max, hv;  // Link's life, the HUD gauge's max, BotW's MaxHartValue (<= 0: unread), quarter hearts
} KbbIn;
enum { KBB_GO, KBB_OFF, KBB_NOT_SEALING, KBB_NOT_SWING, KBB_BODY, KBB_LINKWPN, KBB_FIN, KBB_NOPOWER, KBB_HURT, KBB_NREASON };
static const char *const KBB_REASON[KBB_NREASON] = {"go", "off (set kbbeam 1)", "not the Sealing Blade", "not a Keyblade swing",
                                                    "Anti / Mickey / Roxas", "Link's weapon", "finisher (kbbeamfin 0)",
                                                    "no power", "not at full health"};

static int kbb_full_max(int hud_max, int hv) { return hv > 0 && (hud_max <= 0 || hv < hud_max) ? hv : hud_max; }
static bool kbb_full(int life, int hud_max, int hv) {
    int m = kbb_full_max(hud_max, hv);
    return life > 0 && m > 0 && life >= m;
}
static int kbb_trigger(const KbbIn *in) {
    if (!in->on) return KBB_OFF;
    if (!in->attack) return KBB_NOT_SWING;
    if (in->anti || in->mickey || in->roxas) return KBB_BODY;
    if (in->link_weapon) return KBB_LINKWPN;
    if (in->item != KBB_SEALING) return KBB_NOT_SEALING;
    if (in->power <= 0) return KBB_NOPOWER;
    if (in->finisher && !in->allow_fin) return KBB_FIN;
    if (!kbb_full(in->life, in->hud_max, in->hv)) return KBB_HURT;
    return KBB_GO;
}

// one decision per (swing, blade) at its first live window, at most one crescent per swing
typedef struct { int fired, checked[2]; } KbbGate;
static void kbb_gate_reset(KbbGate *g) { g->fired = g->checked[0] = g->checked[1] = -(1 << 30); }
static bool kbb_gate_ask(KbbGate *g, int swing, int blade) {  // true = decide now
    blade = blade ? 1 : 0;
    if (g->fired == swing || g->checked[blade] == swing) return false;
    g->checked[blade] = swing;
    return true;
}
static void kbb_gate_fired(KbbGate *g, int swing) { g->fired = swing; }

typedef struct { uint64_t actor; float pos[3]; int32_t life; float r, h; } KbbEnemy;  // feet point, body cylinder (u)

// the target at launch: the lock-on one (alive, within 1.5 x range), else the nearest alive enemy within range whose direction
// is inside the cone (cos of the half angle) around dir, else -1 (straight)
static int kbb_pick(const float from[3], const float dir[2], uint64_t lock, const KbbEnemy *e, int n, float range, float cone_cos) {
    for (int i = 0; i < n && lock; i++) {
        float dx = e[i].pos[0] - from[0], dz = e[i].pos[2] - from[2];
        if (e[i].actor == lock && e[i].life > 0 && dx * dx + dz * dz <= 2.25f * range * range) return i;
    }
    int best = -1;
    float bd = range * range;
    for (int i = 0; i < n; i++) {
        if (e[i].life <= 0) continue;
        float dx = e[i].pos[0] - from[0], dz = e[i].pos[2] - from[2], d2 = dx * dx + dz * dz;
        if (d2 > bd || d2 < 1e-6f) continue;
        float d = sqrtf(d2);
        if ((dx * dir[0] + dz * dir[1]) / d < cone_cos) continue;
        bd = d2, best = i;
    }
    return best;
}

// turn dir (unit x, z) toward `to` by at most max_turn rad
static void kbb_steer(float dir[2], const float from[3], const float to[3], float max_turn) {
    float dx = to[0] - from[0], dz = to[2] - from[2];
    if (dx * dx + dz * dz < 1e-6f) return;
    float yaw = atan2f(dir[0], dir[1]), want = atan2f(dx, dz);
    float d = remainderf(want - yaw, 6.2831853f);
    yaw += d > max_turn ? max_turn : d < -max_turn ? -max_turn : d;
    dir[0] = sinf(yaw), dir[1] = cosf(yaw);
}

typedef struct {
    bool on;
    int blade, atkp, power, item, fx, respawns;
    int fx_el, rule;                 // (s217) the element it draws (a rolled crescent / kbs1_beam / the fallback), the roll rule
    uint32_t fxg;                    // (s217, s216 review F2) g_fx_gen[fx] at its spawn: fx_mine = the slot is still this crescent's
    float roll;                      // (s217) deg 0..180, -1 = the old upright arch
    float pos[3], dir[2], flown, t;  // t = KH frames alive
    uint64_t target, hit;            // hit = the enemy it hit (0: none)
} KbbShot;
enum { KBB_FLY, KBB_HIT, KBB_RANGE };

// does a beam point p touch enemy e (body cylinder grown by the beam radius r)?
static bool kbb_touch(const float p[3], const KbbEnemy *e, float r) {
    if (e->life <= 0) return false;
    float dx = e->pos[0] - p[0], dz = e->pos[2] - p[2], rr = e->r + r;
    return dx * dx + dz * dz <= rr * rr && p[1] >= e->pos[1] - r && p[1] <= e->pos[1] + e->h + r;
}

// moves the shot `dist` u along dir (level) in sub-steps of <= sub u, testing every enemy at each; stops at its range.
// -> KBB_HIT (*hit = the enemy index, the shot ends there, once), KBB_RANGE (ended at range), KBB_FLY
static int kbb_move(KbbShot *b, float dist, const KbbEnemy *e, int n, float r, float range, float sub, int *hit) {
    *hit = -1;
    if (!b->on) return KBB_RANGE;
    if (sub < 0.05f) sub = 0.05f;
    int ns = (int)ceilf(dist / sub);
    if (ns < 1) ns = 1;
    float step = dist / (float)ns;
    for (int s = 0; s < ns; s++) {
        float left = range - b->flown, st = step < left ? step : left;
        b->pos[0] += b->dir[0] * st, b->pos[2] += b->dir[1] * st, b->flown += st;
        for (int i = 0; i < n; i++)
            if (kbb_touch(b->pos, &e[i], r)) {
                *hit = i, b->hit = e[i].actor, b->on = false;
                return KBB_HIT;
            }
        if (b->flown >= range - 1e-4f) { b->on = false; return KBB_RANGE; }
    }
    return KBB_FLY;
}

// the launch point's height: the blade tip's, clamped to lo..hi above the feet (u)
static float kbb_launch_y(float tip_y, float feet_y, float lo, float hi) {
    float y = tip_y - feet_y;
    return feet_y + (y < lo ? lo : y > hi ? hi : y);
}

// ---- (s217 beam) the crescent lies in the plane the blade swept: a ROLL about its travel direction, fixed at launch.
// Its width runs along cos(roll) x Sora's right + sin(roll) x up (seen from behind: 0 = a flat crescent, a level line; 90 = an
// upright one; 45 = "/", 135 = "\"). The shape is symmetric, so roll + 180 = roll: 0..180. sora_fx.c (exe side) places an
// unbound element with a yaw only, so fx.bin holds the crescent pre-rolled every KBB_ROLL_STEP deg (kbownfx.py BEAM_ROLLS:
// kbs1_beam_r000 .. kbs1_beam_r165) and the nearest one is drawn; the s216 upright arch kbs1_beam is "kbbeamroll 0".
#define KBB_ROLL_STEP 15
#define KBB_NROLL (180 / KBB_ROLL_STEP)
enum { KBB_RULE_SWEEP, KBB_RULE_BLADE, KBB_RULE_THRUST, KBB_RULE_OVERRIDE, KBB_RULE_OLD, KBB_RULE_FLAT, KBB_RULE_TEST, KBB_NRULE };
static const char *const KBB_RULE[KBB_NRULE] = {"sweep: the tip's path across the flight", "blade: the tip barely moved, the blade's line",
                                                "flat: a thrust (tip path and blade along the flight)", "kbbeamrolldeg",
                                                "old upright arch (kbbeamroll 0)", "flat (kbbeamroll 2)", "flat (kbbeamtest)"};

static float kbb_fold180(float deg) {
    deg = fmodf(deg, 180.0f);
    if (deg < 0) deg += 180.0f;
    return deg >= 180.0f ? deg - 180.0f : deg;
}
// the nearest pre-rolled crescent (0 .. KBB_NROLL - 1; 172.5..180 = 0 again)
static int kbb_roll_slot(float deg) { return (int)floorf(kbb_fold180(deg) / KBB_ROLL_STEP + 0.5f) % KBB_NROLL; }

// a world vector seen across the flight (yaw = the travel direction, (sin, cos) in BotW x, z): a = along Sora's right, b = up.
// Right = (-cos yaw, 0, sin yaw): sora_fx.c's model +x is the LEFT (sorabake space), KH effect +x = model -x = the right.
static void kbb_across(const float v[3], float yaw, float *a, float *b) {
    *a = -cosf(yaw) * v[0] + sinf(yaw) * v[2];
    *b = v[1];
}
static float kbb_len3(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }

// the roll (deg 0..180) of a swing's crescent from the blade segment one frame before (prev) and now (grip xyz, tip xyz), yaw =
// the launch direction. 1) sweep: the tip's move prev -> now seen across the flight, when it moved >= min_move u across AND
// >= rel of its whole move (a lunge adds a move along the flight); 2) blade: else the blade's own line (grip -> tip) across the
// flight when >= rel of its length lies across (the swing plane holds the blade); 3) thrust: else flat (0).
static float kbb_roll_swing(const float prev[6], const float now[6], float yaw, float min_move, float rel, int *rule) {
    float v[3] = {now[3] - prev[3], now[4] - prev[4], now[5] - prev[5]}, a, b;
    kbb_across(v, yaw, &a, &b);
    float across = hypotf(a, b);
    if (across >= min_move && across >= rel * kbb_len3(v)) {
        *rule = KBB_RULE_SWEEP;
        return kbb_fold180(atan2f(b, a) * 57.2957795f);
    }
    float w[3] = {now[3] - now[0], now[4] - now[1], now[5] - now[2]};
    kbb_across(w, yaw, &a, &b);
    across = hypotf(a, b);
    float len = kbb_len3(w);
    if (len > 1e-4f && across >= rel * len) {
        *rule = KBB_RULE_BLADE;
        return kbb_fold180(atan2f(b, a) * 57.2957795f);
    }
    *rule = KBB_RULE_THRUST;
    return 0;
}

// which crescent a launch draws: -> roll deg 0..180, or -1 = the old upright arch (kbs1_beam, s216). mode = live kbbeamroll
// (0 old, 1 follow the swing, 2 always flat), override = kbbeamrolldeg (< 0 off; wins over every mode), test = the kbbeamtest
// crescent (no swing behind it: flat unless the override / mode 0). prev / now may be NULL for a test.
static float kbb_roll_pick(int mode, int override, bool test, const float prev[6], const float now[6], float yaw, float min_move,
                           float rel, int *rule) {
    if (override >= 0) { *rule = KBB_RULE_OVERRIDE; return kbb_fold180((float)override); }
    if (mode == 0) { *rule = KBB_RULE_OLD; return -1; }
    if (test || !prev || !now) { *rule = KBB_RULE_TEST; return 0; }
    if (mode == 2) { *rule = KBB_RULE_FLAT; return 0; }
    return kbb_roll_swing(prev, now, yaw, min_move, rel, rule);
}

#ifndef KBBEAM_HOST
// ------------------------------------------------------------------ game glue (sora.c, after wshot_tick)
// Needs: S, g_lock, kbfx_item / kbfx_name / g_kbfx_blade, g_wpn_cur, IN_*, SLX, sl_hit / sl_str_bonus, berserk_on, g_dmg_base,
// linkpos_*, enemy_body, ah_hit_f54 / ah_hit, dg_hit, vl_hit, sora_fx_* / sora_sfx_*, LOG.
extern int g_hud_life_max;  // (sora.c hud_update, defined below the include)
static int g_kbb_on = 1;          // live kbbeam (DEFAULT ON since s216: the user verified it on the 0.5.3 trial; "set kbbeam 0" = off)
static int g_kbb_log = 0;         // live kbbeamlog
static int g_kbb_test = 0;        // live kbbeamtest (one-shot)
static float g_kbb_spd = 40.0f;   // u/s GUESS (BotW data has no beam speed)
static float g_kbb_range = 20.0f; // u = BotW Weapon_Sword_070 Rod.MagicRange 20.0
static float g_kbb_r = 0.3f;      // u = Rod.MagicRadius 0.3
static int g_kbb_pct = 33;        // % of the swing's power = BotW beam 10 / sword 30 (True Form 20 / 60)
static float g_kbb_cone = 30.0f;  // deg half angle GUESS
static float g_kbb_turn = 0.10f;  // rad per KH frame GUESS (Blizzard's assist turns 0.07)
static int g_kbb_fin = 1;         // finishers fire too GUESS
static int g_kbb_wall = 1;        // ends on the static world
static int g_kbb_roll = 1;        // (s217) live kbbeamroll: 1 = the crescent lies in the swing's plane (DEFAULT: the user called the
                                  // s216 upright arch wrong), 0 = the s216 upright arch kbs1_beam, 2 = always flat
static int g_kbb_rolldeg = -1;    // (s217) live kbbeamrolldeg: a fixed roll in deg for tests (-1 = off; wins over kbbeamroll)
#define KBB_ROLL_MOVE_CM 3.0f     // (s217) GUESS: the tip must move >= 3 KH cm across the flight in the frame for the sweep rule
#define KBB_ROLL_REL 0.3f         // (s217) GUESS: ... and >= 30 % of its move (of the blade's length for the blade rule) across it
#define KBB_TEST_ATKP 126         // the test crescent's atkp row: A300 (base Sora's first ground swing, sora.c repel notes)
static KbbShot g_kbb[KBB_MAX];
static KbbGate g_kbb_gate = {-(1 << 30), {-(1 << 30), -(1 << 30)}};
static unsigned long long g_kbb_tick_ms;  // last kbbeam_tick (a long gap = not driven: the crescents end)

static struct { int fx_tried, sfx_tried, fx_fly, fx_hit, se_fly, se_hit, fx_roll[KBB_NROLL], nroll; const char *fly_name; } KBA = {
    0, 0, -1, -1, -1, -1, {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, 0, "none"};
static void kbb_assets(void) {
    if (!KBA.fx_tried && sora_fx_ready()) {
        KBA.fx_tried = 1;
        KBA.fx_fly = sora_fx_id("kbs1_beam"), KBA.fly_name = "kbs1_beam";
        if (KBA.fx_fly < 0) KBA.fx_fly = sora_fx_id("wisdom_bullet"), KBA.fly_name = "wisdom_bullet";
        KBA.fx_hit = sora_fx_id("kbs1_beam_hit");
        // (s217) the rolled crescents: all or none (an fx.bin before s217 has none: one "unknown element" line, then the old one)
        KBA.nroll = 0;
        if ((KBA.fx_roll[0] = sora_fx_id("kbs1_beam_r000")) >= 0) {
            KBA.nroll = 1;
            for (int k = 1; k < KBB_NROLL; k++) {
                char nm[32];
                snprintf(nm, sizeof nm, "kbs1_beam_r%03d", k * KBB_ROLL_STEP);
                if ((KBA.fx_roll[k] = sora_fx_id(nm)) >= 0) KBA.nroll++;
            }
            if (KBA.nroll < KBB_NROLL)
                for (int k = 0; k < KBB_NROLL; k++) KBA.fx_roll[k] = -1;  // (a broken set: the old crescent, never a mix)
        }
        LOG("[kbbeam] fx: crescent %s (%d), impact %s, swing-plane crescents %s\n", KBA.fx_fly >= 0 ? KBA.fly_name : "none", KBA.fx_fly,
            KBA.fx_hit >= 0 ? "kbs1_beam_hit" : "the Keyblade's hit spark (fallback)",
            KBA.nroll == KBB_NROLL ? "kbs1_beam_r000..r165" : KBA.nroll ? "INCOMPLETE (old crescent)" : "none (fx.bin before s217: old crescent)");
    }
    if (!KBA.sfx_tried && sora_sfx_ready()) {
        KBA.sfx_tried = 1;
        KBA.se_fly = sora_sfx_id("kbs1_beam");
        if (KBA.se_fly < 0) KBA.se_fly = sora_sfx_id("wisdom_shot");
        KBA.se_hit = sora_sfx_id("kbs1_beam_hit");
        LOG("[kbbeam] sfx: launch %d, impact %s\n", KBA.se_fly, KBA.se_hit >= 0 ? "kbs1_beam_hit" : "the swing's hit sound (fallback)");
    }
}

static int kbb_enemies(KbbEnemy *e, int max) {
    uint64_t a[64];
    float p[64][3];
    int32_t l[64];
    int n = linkpos_enemies(a, p, l, 64);
    if (n > 64) n = 64;
    if (n > max) n = max;
    for (int i = 0; i < n; i++) {
        e[i].actor = a[i], e[i].life = l[i];
        memcpy(e[i].pos, p[i], sizeof e[i].pos);
        enemy_body(a[i], &e[i].r, &e[i].h);
    }
    return n < 0 ? 0 : n;
}

// (s217, s216 review F2) the crescent's fx slot is still its own (not reused by another effect since its spawn)
static bool kbb_fx_mine(const KbbShot *b) { return b->fx >= 0 && fx_mine(b->fx, b->fxg); }
static void kbb_fx_spawn(KbbShot *b, float yaw, float to_u) {
    b->fx = b->fx_el >= 0 ? sora_fx_spawn(b->fx_el, b->pos, yaw, to_u) : -1;
    b->fxg = b->fx >= 0 && b->fx < 32 ? g_fx_gen[b->fx] : 0;
}

// roll: deg 0..180 = the swing-plane crescent kbs1_beam_r<nearest>, -1 = the old one (kbs1_beam / fallback); rule = KBB_RULE_*
static void kbb_launch(int blade, int atkp, int power, int item, const float tip[3], const float feet[3], float face, float to_u,
                       const char *why, float roll, int rule) {
    kbb_assets();
    KbbShot *b = NULL;
    for (int i = 0; i < KBB_MAX && !b; i++) if (!g_kbb[i].on) b = &g_kbb[i];
    if (!b) {  // full: the oldest gives way
        b = &g_kbb[0];
        for (int i = 1; i < KBB_MAX; i++) if (g_kbb[i].t > b->t) b = &g_kbb[i];
        if (kbb_fx_mine(b)) sora_fx_kill(b->fx);
    }
    memset(b, 0, sizeof *b);
    b->on = true, b->blade = blade, b->atkp = atkp, b->power = power, b->item = item, b->fx = -1;
    b->roll = roll, b->rule = rule;
    int rs = roll >= 0 && KBA.nroll == KBB_NROLL ? kbb_roll_slot(roll) : -1;
    b->fx_el = rs >= 0 ? KBA.fx_roll[rs] : KBA.fx_fly;
    b->pos[0] = tip[0], b->pos[2] = tip[2];
    b->pos[1] = kbb_launch_y(tip[1], feet[1], 60 * to_u, 150 * to_u);  // GUESS heights
    b->dir[0] = sinf(face), b->dir[1] = cosf(face);
    KbbEnemy e[64];
    int n = kbb_enemies(e, 64);
    int pick = kbb_pick(b->pos, b->dir, g_lock.locked ? g_lock.actor : 0, e, n, g_kbb_range, cosf(g_kbb_cone * 3.14159265f / 180));
    b->target = pick >= 0 ? e[pick].actor : 0;
    kbb_fx_spawn(b, face, to_u);
    if (KBA.se_fly >= 0) sora_sfx_play_at(KBA.se_fly, 1.0f, b->pos);
    if (g_kbb_log) {
        char el[48];
        if (rs >= 0) snprintf(el, sizeof el, "kbs1_beam_r%03d", rs * KBB_ROLL_STEP);
        else snprintf(el, sizeof el, "%s%s", KBA.fx_fly >= 0 ? KBA.fly_name : "none", roll >= 0 ? " (no rolled crescents)" : "");
        LOG("[kbbeam] launch (%s) blade %d, item 0x%x, atkp %d power %d -> beam power %d%%, at %.2f %.2f %.2f yaw %.0f, target %llx%s, "
            "roll %.0f (%s) -> %s\n",
            why, blade, item, atkp, power, g_kbb_pct, b->pos[0], b->pos[1], b->pos[2], face * 180 / 3.14159265f,
            (unsigned long long)b->target, !b->target ? " (straight)" : g_lock.locked && b->target == g_lock.actor ? " (lock-on)" : " (cone)",
            roll, KBB_RULE[rule], el);
    }
}

// the hook in blade_hits_one: every frame of a live window (after its power test); decides once per (swing, blade)
// (s217) prev / seg = this blade's segment one frame before / now (grip xyz, tip xyz): the swing's plane for the crescent's roll
static void kbbeam_swing(int blade, const HitWin *win, const float prev[6], const float seg[6], const float feet[3], float to_u) {
    if (!kbb_gate_ask(&g_kbb_gate, S.swing, blade)) return;
    KbbIn in = {.on = g_kbb_on != 0, .item = kbfx_item(blade), .attack = S.special == SP_ATTACK, .anti = IN_ANTI,
                .mickey = IN_MICKEY, .roxas = IN_ROXAS, .link_weapon = blade == 0 && g_wpn_cur.def != NULL,
                .finisher = win->kind != ATKP_KIND_NORMAL, .allow_fin = g_kbb_fin != 0, .power = win->power,
                .life = 0, .hud_max = g_hud_life_max, .hv = 0};
    int why = kbb_trigger(&in);
    if (why == KBB_HURT || why == KBB_GO) {  // (the health reads only when everything else says yes)
        in.life = linkpos_link_life();
        int32_t hv = -1;
        in.hv = SLX.gdt_s32 && SLX.gdt_s32("MaxHartValue", &hv) && hv > 0 && hv < 200 ? (int)hv : 0;
        why = kbb_trigger(&in);
    }
    if (why != KBB_GO) {
        if (g_kbb_log && g_kbb_on && in.item == KBB_SEALING)
            LOG("[kbbeam] swing %d blade %d: no beam (%s; life %d, full = %d)\n", S.swing, blade, KBB_REASON[why], in.life,
                kbb_full_max(in.hud_max, in.hv));
        return;
    }
    kbb_gate_fired(&g_kbb_gate, S.swing);
    int rule;
    float roll = kbb_roll_pick(g_kbb_roll, g_kbb_rolldeg, false, prev, seg, S.face, KBB_ROLL_MOVE_CM * to_u, KBB_ROLL_REL, &rule);
    kbb_launch(blade, win->atkp, win->power, in.item, seg + 3, feet, S.face, to_u, in.finisher ? "finisher" : "swing", roll, rule);
}

static void kbb_end(KbbShot *b, const char *how, bool fade) {
    if (!kbb_fx_mine(b)) {}  // (s217 F2: its slot holds another effect now: leave that one alone)
    else if (fade) sora_fx_fadeout(b->fx, 6);
    else sora_fx_kill(b->fx);
    b->on = false;
    if (g_kbb_log) LOG("[kbbeam] end (%s) at %.2f %.2f %.2f after %.2f u, %.0f KH frames\n", how, b->pos[0], b->pos[1], b->pos[2], b->flown, b->t);
}

static void kbb_impact(KbbShot *b, float to_u) {
    if (KBA.fx_hit >= 0) sora_fx_spawn(KBA.fx_hit, b->pos, atan2f(b->dir[0], b->dir[1]), to_u);
    else {
        int keep = g_kbfx_blade;
        g_kbfx_blade = b->blade;  // (that blade's Keyblade's own spark)
        sora_fx_event(kbfx_name("hit"), b->pos, atan2f(b->dir[0], b->dir[1]), to_u);
        g_kbfx_blade = keep;
    }
}

static void kbb_hit(KbbShot *b, const KbbEnemy *e, float to_u) {
    int pw = (int)lroundf((float)b->power * (float)g_kbb_pct / 100.0f);
    if (pw < 1) pw = 1;
    sl_str_bonus = berserk_on() ? 2 : 0;  // (as the blade's hit: Berserk Charge; Mickey never fires one)
    int dmg = sl_hit(e->actor, e->life, pw, g_dmg_base, 0);
    sl_str_bonus = 0;
    int react = 3, kb1 = 0x7fff, kb2 = 0;  // GUESS: a plain hit (class 3 flinch / hop), like the Wisdom shot; no blow
    int f54 = ah_hit_f54(e->actor, e->pos, F54_SMALL, react, to_u);
    linkpos_damage_enemy(e->actor, dmg, f54);
    if (IN_VALOR) vl_hit();
    ah_hit(e->actor, e->pos, b->pos, react, kb1, kb2, f54, to_u, "kbbeam");
    dg_hit(b->atkp, e->actor);
    kbb_impact(b, to_u);
    if (KBA.se_hit >= 0) sora_sfx_play_at(KBA.se_hit, 1.0f, e->pos);
    else sora_sfx_hit(b->atkp, 1.0f, e->pos);
    if (g_kbb_log)
        LOG("[kbbeam] HIT %llx (life %d) at %.2f %.2f %.2f: power %d x %d%% = %d -> damage %d f54 %#x, after %.2f u\n",
            (unsigned long long)e->actor, e->life, e->pos[0], e->pos[1], e->pos[2], b->power, g_kbb_pct, pw, dmg, f54, b->flown);
}

// every driven frame (sora.c frame body, after mk_ball_tick)
static void kbbeam_tick(float kh, float to_u) {
    unsigned long long now = GetTickCount64();
    bool gap = g_kbb_tick_ms && now - g_kbb_tick_ms > 1000;  // (not driven for a while: event, menu, load)
    g_kbb_tick_ms = now;
    if (g_kbb_test) {
        g_kbb_test = 0;
        float lm[12];
        if (linkpos_link_mtx(lm)) {
            float feet[3] = {lm[3], lm[7], lm[11]};
            float tip[3] = {feet[0] + sinf(S.face) * 0.5f, feet[1] + 100 * to_u, feet[2] + cosf(S.face) * 0.5f};
            int rule;
            float roll = kbb_roll_pick(g_kbb_roll, g_kbb_rolldeg, true, NULL, NULL, S.face, 0, 0, &rule);  // (s217: flat / override)
            kbb_launch(0, KBB_TEST_ATKP, 100, KB.kb[KB_MAIN], tip, feet, S.face, to_u, "kbbeamtest", roll, rule);
        } else LOG("[kbbeam] test: no feet matrix, nothing fired\n");
    }
    KbbEnemy e[64];
    int n = -1;
    for (int k = 0; k < KBB_MAX; k++) {
        KbbShot *b = &g_kbb[k];
        if (!b->on) continue;
        if (gap) { kbb_end(b, "not driven", false); continue; }
        if (n < 0) n = kbb_enemies(e, 64);
        b->t += kh;
        int ti = -1;
        for (int i = 0; i < n && b->target; i++) if (e[i].actor == b->target && e[i].life > 0) ti = i;
        if (b->target && ti < 0) b->target = 0;  // (gone / dead: straight on)
        if (ti >= 0) kbb_steer(b->dir, b->pos, e[ti].pos, g_kbb_turn * kh);
        float dist = g_kbb_spd * kh * KBB_FRAME_S, left = g_kbb_range - b->flown;
        if (dist > left) dist = left;
        bool wall = false;
        if (g_kbb_wall && dist > 0) {
            float to[3] = {b->pos[0] + b->dir[0] * dist, b->pos[1], b->pos[2] + b->dir[1] * dist}, h[3], nr[3];
            if (linkpos_raycast(b->pos, to, h, nr)) {
                float wd = hypotf(h[0] - b->pos[0], h[2] - b->pos[2]);
                if (wd < dist) dist = wd, wall = true;
            }
        }
        int hi;
        int r = kbb_move(b, dist, e, n, g_kbb_r, g_kbb_range, 0.25f, &hi);
        float yaw = atan2f(b->dir[0], b->dir[1]);
        if (r == KBB_HIT) {
            kbb_hit(b, &e[hi], to_u);
            kbb_end(b, "hit", false);
            continue;
        }
        if (r == KBB_RANGE) { kbb_end(b, "range", true); continue; }
        if (wall) {
            kbb_impact(b, to_u);
            kbb_end(b, "world", false);
            continue;
        }
        if (b->t > g_kbb_range / (g_kbb_spd > 1 ? g_kbb_spd : 1) / KBB_FRAME_S * 2 + 30) { kbb_end(b, "time", true); continue; }
        if (kbb_fx_mine(b) && sora_fx_alive(b->fx)) sora_fx_move(b->fx, b->pos, yaw);
        else if (b->fx_el >= 0 && b->respawns < 3) b->respawns++, kbb_fx_spawn(b, yaw, to_u);  // (a one-shot element ended mid-flight,
                                                                                            // or its slot was taken: its own element again)
    }
}

static bool kbbeam_live_set(const char *key, int v) {
    if (strncmp(key, "kbbeam", 6)) return false;
    const char *k = key + 6;
    if (!*k) g_kbb_on = v != 0, LOG("[kbbeam] %s\n", g_kbb_on ? "on: Sealing Blade swings at full health send a crescent" : "off");
    else if (!strcmp(k, "log")) g_kbb_log = v != 0, LOG("[kbbeam] log %s\n", g_kbb_log ? "on" : "off");
    else if (!strcmp(k, "test")) g_kbb_test = v != 0, LOG("[kbbeam] test crescent %s (one-shot: remove the line)\n", v ? "queued" : "off");
    else if (!strcmp(k, "spd")) g_kbb_spd = v < 1 ? 1.0f : (float)v, LOG("[kbbeam] speed %.0f u/s\n", g_kbb_spd);
    else if (!strcmp(k, "range")) g_kbb_range = v < 1 ? 1.0f : (float)v, LOG("[kbbeam] range %.0f u\n", g_kbb_range);
    else if (!strcmp(k, "r")) g_kbb_r = (v < 0 ? 0 : v) * 0.01f, LOG("[kbbeam] hit radius %.2f u (+ the enemy's body)\n", g_kbb_r);
    else if (!strcmp(k, "pow")) g_kbb_pct = v < 0 ? 0 : v, LOG("[kbbeam] power %d%% of the swing's\n", g_kbb_pct);
    else if (!strcmp(k, "cone")) g_kbb_cone = (float)(v < 0 ? 0 : v > 180 ? 180 : v), LOG("[kbbeam] cone %.0f deg half angle\n", g_kbb_cone);
    else if (!strcmp(k, "turn")) g_kbb_turn = (v < 0 ? 0 : v) * 0.001f, LOG("[kbbeam] turn %.3f rad per KH frame\n", g_kbb_turn);
    else if (!strcmp(k, "fin")) g_kbb_fin = v != 0, LOG("[kbbeam] finishers %s\n", g_kbb_fin ? "fire too" : "fire none");
    else if (!strcmp(k, "wall")) g_kbb_wall = v != 0, LOG("[kbbeam] world collision %s\n", g_kbb_wall ? "on" : "off");
    else if (!strcmp(k, "roll")) {  // (s217)
        g_kbb_roll = v < 0 || v > 2 ? 1 : v;
        LOG("[kbbeam] roll %d: %s\n", g_kbb_roll, g_kbb_roll == 0 ? "the s216 upright arch (old)" : g_kbb_roll == 2 ? "always flat" : "the crescent lies in the swing's plane");
    } else if (!strcmp(k, "rolldeg")) {  // (s217, tests)
        g_kbb_rolldeg = v < 0 ? -1 : v;
        if (g_kbb_rolldeg < 0) LOG("[kbbeam] rolldeg off\n");
        else LOG("[kbbeam] rolldeg %d: every crescent rolled %.0f deg (wins over kbbeamroll)\n", v, kbb_fold180((float)v));
    }
    else return false;
    return true;
}
#endif  // KBBEAM_HOST

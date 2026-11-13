// host tests for runtime/sora_kbbeam.h's pure part (s216 beam: trigger, full health, one per swing, target pick, steering,
// flight / hit / range end, one hit only). Build + run from the repo root (UCRT64 clang):
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/kbbeam_test.c -o build_h/kbbeam_test.exe && build_h/kbbeam_test.exe
#include <stdio.h>
#define KBBEAM_HOST  // (the pure part only)
#include "sora_kbbeam.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 1e-3f)

// (s217 roll) the glue's 30 % share (sora_kbbeam.h KBB_ROLL_REL lives in the game part)
#define KBB_ROLL_REL_T 0.3f
// a world point from Sora-frame parts at yaw y: r along his right (-cos y, 0, sin y), u up, f forward (sin y, 0, cos y)
static void pt_at(float out[3], float y, float r, float u, float f) {
    out[0] = -cosf(y) * r + sinf(y) * f, out[1] = u, out[2] = sinf(y) * r + cosf(y) * f;
}
// a blade segment (grip xyz, tip xyz): the grip at (0, grip_u, 0), the tip at (tip_r, tip_u, tip_f) in Sora's frame
static void seg_at(float s[6], float y, float tip_r, float grip_u, float tip_u, float tip_f) {
    pt_at(s, y, 0, grip_u, 0), pt_at(s + 3, y, tip_r, tip_u, tip_f);
}
// COPY of runtime/sora_fx.c emit_inst's unbound placement (exe side) for a direction: KH effect space (x, y down, z) -> sorabake
// model (-x, -y, z) -> world RotY(yaw) (wx = c mx + s mz, wz = -s mx + c mz)
static void emit_unbound(const float p[3], float yaw, float w[3]) {
    float s = sinf(yaw), c = cosf(yaw), mx = -p[0], my = -p[1], mz = p[2];
    w[0] = c * mx + s * mz, w[1] = my, w[2] = -s * mx + c * mz;
}

static KbbIn ok_in(void) {  // everything says yes
    KbbIn in = {.on = true, .item = KBB_SEALING, .attack = true, .power = 100, .allow_fin = true, .life = 40, .hud_max = 40, .hv = 40};
    return in;
}

int main(void) {
    // ---- full health (life vs BotW's MaxHartValue, capped by the HUD max)
    CHECK(kbb_full(40, 40, 40));
    CHECK(!kbb_full(39, 40, 40));
    CHECK(kbb_full(40, 48, 40));   // HUD max inflated by old yellow hearts: BotW's max decides
    CHECK(!kbb_full(44, 48, 0));   // MaxHartValue unread: the HUD max alone
    CHECK(kbb_full(48, 48, 0));
    CHECK(kbb_full(52, 48, 48));   // yellow hearts on top: still full
    CHECK(!kbb_full(0, 40, 40) && !kbb_full(5, 0, 0));
    CHECK(kbb_full_max(40, 44) == 40 && kbb_full_max(0, 44) == 44 && kbb_full_max(40, 0) == 40);

    // ---- trigger reasons
    KbbIn in = ok_in();
    CHECK(kbb_trigger(&in) == KBB_GO);
    in = ok_in(), in.on = false;        CHECK(kbb_trigger(&in) == KBB_OFF);
    in = ok_in(), in.attack = false;    CHECK(kbb_trigger(&in) == KBB_NOT_SWING);  // Limit command / magic / summon
    in = ok_in(), in.anti = true;       CHECK(kbb_trigger(&in) == KBB_BODY);
    in = ok_in(), in.mickey = true;     CHECK(kbb_trigger(&in) == KBB_BODY);
    in = ok_in(), in.roxas = true;      CHECK(kbb_trigger(&in) == KBB_BODY);
    in = ok_in(), in.link_weapon = true; CHECK(kbb_trigger(&in) == KBB_LINKWPN);
    in = ok_in(), in.item = 0x29;       CHECK(kbb_trigger(&in) == KBB_NOT_SEALING);  // Kingdom Key
    in = ok_in(), in.item = 0x7e02;     CHECK(kbb_trigger(&in) == KBB_NOT_SEALING);  // Guardian Core
    in = ok_in(), in.power = 0;         CHECK(kbb_trigger(&in) == KBB_NOPOWER);
    in = ok_in(), in.finisher = true;   CHECK(kbb_trigger(&in) == KBB_GO);
    in.allow_fin = false;               CHECK(kbb_trigger(&in) == KBB_FIN);
    in = ok_in(), in.life = 39;         CHECK(kbb_trigger(&in) == KBB_HURT);
    in = ok_in(), in.life = 0, in.hv = 0; CHECK(kbb_trigger(&in) == KBB_HURT);  // (the glue reads life only after a yes)
    CHECK(KBB_NREASON == (int)(sizeof KBB_REASON / sizeof KBB_REASON[0]));

    // ---- one decision per (swing, blade), at most one crescent per swing
    KbbGate g;
    kbb_gate_reset(&g);
    CHECK(kbb_gate_ask(&g, 7, 0));       // first live window of swing 7, main blade
    CHECK(!kbb_gate_ask(&g, 7, 0));      // later frames / windows of the same swing: no new decision
    CHECK(kbb_gate_ask(&g, 7, 1));       // Valor's left blade decides on its own (main refused)
    kbb_gate_fired(&g, 7);
    CHECK(!kbb_gate_ask(&g, 7, 1) && !kbb_gate_ask(&g, 7, 0));
    CHECK(kbb_gate_ask(&g, 8, 0));       // next swing
    kbb_gate_fired(&g, 8);
    CHECK(!kbb_gate_ask(&g, 8, 1));      // main fired: the left blade does not add a second one
    CHECK(kbb_gate_ask(&g, 9, 1) && kbb_gate_ask(&g, 9, 0));
    CHECK(kbb_gate_ask(&g, 0, 0));       // swing 0 after a reset is still a fresh swing

    // ---- target pick: lock-on, else nearest in the cone within range, else none
    float from[3] = {0, 1, 0}, fwd[2] = {0, 1};  // facing +z
    float cc = cosf(30 * 3.14159265f / 180);
    KbbEnemy e[4] = {
        {0xA, {0, 0, 10}, 10, 0.5f, 1.6f},    // ahead 10
        {0xB, {2, 0, 5}, 10, 0.5f, 1.6f},     // ahead-right 5.4, 21.8 deg: in the cone, nearest
        {0xC, {0, 0, -3}, 10, 0.5f, 1.6f},    // behind
        {0xD, {8, 0, 2}, 10, 0.5f, 1.6f},     // to the side, 76 deg: out of the cone
    };
    CHECK(kbb_pick(from, fwd, 0, e, 4, 20, cc) == 1);
    CHECK(kbb_pick(from, fwd, 0xC, e, 4, 20, cc) == 2);   // the lock wins even behind him
    CHECK(kbb_pick(from, fwd, 0xD, e, 4, 20, cc) == 3);
    e[2].life = 0;
    CHECK(kbb_pick(from, fwd, 0xC, e, 4, 20, cc) == 1);   // dead lock -> the cone
    e[2].life = 10;
    CHECK(kbb_pick(from, fwd, 0xC, e, 4, 1.9f, cc) == -1);  // lock 3 u away past 1.5 x 1.9, and nothing in range
    e[1].life = 0;
    CHECK(kbb_pick(from, fwd, 0, e, 4, 20, cc) == 0);
    CHECK(kbb_pick(from, fwd, 0, e, 4, 9, cc) == -1);     // the one ahead is past the range
    e[1].life = 10;
    CHECK(kbb_pick(from, fwd, 0, e, 0, 20, cc) == -1);    // no enemies: straight
    CHECK(kbb_pick(from, fwd, 0, e + 2, 1, 20, cc) == -1);  // only one behind: straight

    // ---- steering: turn-rate limited, wraps across +-pi, exact when within the limit
    float d[2] = {0, 1}, to_r[3] = {10, 1, 0};  // target 90 deg right
    kbb_steer(d, from, to_r, 0.1f);
    CHECK(NEAR(atan2f(d[0], d[1]), 0.1f) && NEAR(d[0] * d[0] + d[1] * d[1], 1));
    float d2[2] = {0, 1}, to_f[3] = {0.05f, 1, 10};  // 0.005 rad off: reached at once
    kbb_steer(d2, from, to_f, 0.1f);
    CHECK(NEAR(atan2f(d2[0], d2[1]), atan2f(0.05f, 10)));
    float d3[2] = {sinf(3.0f), cosf(3.0f)}, to_w[3] = {sinf(-3.0f) * 10, 1, cosf(-3.0f) * 10};  // yaw 3.0 -> -3.0: 0.28 rad the short way
    kbb_steer(d3, from, to_w, 0.1f);
    CHECK(NEAR(remainderf(atan2f(d3[0], d3[1]) - 3.1f, 6.2831853f), 0));  // turned +0.1 through pi, not -0.1 the long way
    float d4[2] = {0, 1};
    kbb_steer(d4, from, from, 0.1f);  // on top of it: no change
    CHECK(NEAR(d4[0], 0) && NEAR(d4[1], 1));

    // ---- flight: first enemy along the path, once; range end; dead / too high ignored; sub-steps catch thin bodies
    KbbShot b;
    memset(&b, 0, sizeof b);
    b.on = true, b.pos[1] = 1, b.dir[1] = 1;
    KbbEnemy f[3] = {
        {0x1, {0, 0, 12}, 10, 0.5f, 1.6f},
        {0x2, {0, 0, 6}, 10, 0.5f, 1.6f},   // nearer, listed second: hit first
        {0x3, {0, 0, 3}, 0, 0.5f, 1.6f},    // dead: ignored
    };
    int hi = 99, r = kbb_move(&b, 4.0f, f, 3, 0.3f, 20, 0.25f, &hi);
    CHECK(r == KBB_FLY && hi == -1 && b.on && NEAR(b.flown, 4) && NEAR(b.pos[2], 4));  // passed the dead one
    r = kbb_move(&b, 4.0f, f, 3, 0.3f, 20, 0.25f, &hi);
    CHECK(r == KBB_HIT && hi == 1 && !b.on && b.hit == 0x2);
    CHECK(b.pos[2] >= 6 - 0.8f - 1e-3f && b.pos[2] <= 6 - 0.8f + 0.25f + 1e-3f);  // stopped at the body (r 0.5 + 0.3), not beyond
    r = kbb_move(&b, 10.0f, f, 3, 0.3f, 20, 0.25f, &hi);
    CHECK(r == KBB_RANGE && hi == -1 && b.hit == 0x2);  // ended: never a second hit
    // range: 20 u then it ends, even with a big last step
    memset(&b, 0, sizeof b);
    b.on = true, b.pos[1] = 1, b.dir[0] = 1;
    int steps = 0;
    while (b.on && steps < 100) r = kbb_move(&b, 0.7f, f, 0, 0.3f, 20, 0.25f, &hi), steps++;
    CHECK(r == KBB_RANGE && NEAR(b.flown, 20) && NEAR(b.pos[0], 20) && steps == 29);
    // height: an enemy whose body is below the beam's level misses; level flight never changes y
    memset(&b, 0, sizeof b);
    b.on = true, b.pos[1] = 3.0f, b.dir[1] = 1;
    KbbEnemy lowe = {0x5, {0, 0, 2}, 10, 0.5f, 1.6f};  // top at 1.6 + 0.3 < 3
    r = kbb_move(&b, 4, &lowe, 1, 0.3f, 20, 0.25f, &hi);
    CHECK(r == KBB_FLY && hi == -1 && NEAR(b.pos[1], 3.0f));
    lowe.h = 2.8f;
    memset(&b, 0, sizeof b);
    b.on = true, b.pos[1] = 3.0f, b.dir[1] = 1;
    r = kbb_move(&b, 4, &lowe, 1, 0.3f, 20, 0.25f, &hi);
    CHECK(r == KBB_HIT && hi == 0);  // a tall one (2.8 + 0.3 >= 3) is hit
    // a thin body between two big steps: one 3 u move with 0.25 sub-steps still finds it
    memset(&b, 0, sizeof b);
    b.on = true, b.pos[1] = 1, b.dir[1] = 1;
    KbbEnemy thin = {0x6, {0, 0, 1.5f}, 10, 0.05f, 1.6f};
    r = kbb_move(&b, 3, &thin, 1, 0.1f, 20, 0.25f, &hi);
    CHECK(r == KBB_HIT && hi == 0);
    // a not-on shot does nothing
    memset(&b, 0, sizeof b);
    r = kbb_move(&b, 3, &thin, 1, 0.1f, 20, 0.25f, &hi);
    CHECK(r == KBB_RANGE && hi == -1 && NEAR(b.flown, 0));

    // ---- launch height clamp (u): 60..150 cm above the feet at to_u 0.01
    CHECK(NEAR(kbb_launch_y(10.2f, 10, 0.6f, 1.5f), 10.6f) && NEAR(kbb_launch_y(13, 10, 0.6f, 1.5f), 11.5f) &&
          NEAR(kbb_launch_y(11, 10, 0.6f, 1.5f), 11));

    // ---- a whole flight: launched straight ahead, the cone target off to the right is reached by steering and hit once
    memset(&b, 0, sizeof b);
    b.on = true, b.pos[1] = 1, b.dir[1] = 1;
    KbbEnemy tg[2] = {{0x7, {3, 0, 9}, 10, 0.5f, 1.6f}, {0x8, {0, 0, 30}, 10, 0.5f, 1.6f}};
    int pick = kbb_pick(b.pos, b.dir, 0, tg, 2, 20, cc);
    CHECK(pick == 0);
    float spd = 40, kh = 2;  // 30 fps presents: 2 KH frames each
    int frames = 0, hits = 0, last = KBB_FLY;
    while (b.on && frames < 200) {
        kbb_steer(b.dir, b.pos, tg[pick].pos, 0.10f * kh);
        last = kbb_move(&b, spd * kh * KBB_FRAME_S, tg, 2, 0.3f, 20, 0.25f, &hi);
        if (last == KBB_HIT) { hits++; CHECK(hi == 0); }
        frames++;
    }
    CHECK(last == KBB_HIT && hits == 1 && b.hit == 0x7 && frames < 20);
    // the same flight without steering misses it (3 u right of the line) and ends at range
    memset(&b, 0, sizeof b);
    b.on = true, b.pos[1] = 1, b.dir[1] = 1;
    tg[1].pos[0] = 5;  // (move the far one off the line too)
    frames = 0;
    while (b.on && frames < 200) last = kbb_move(&b, spd * kh * KBB_FRAME_S, tg, 2, 0.3f, 20, 0.25f, &hi), frames++;
    CHECK(last == KBB_RANGE && b.hit == 0 && NEAR(b.flown, 20));

    // ---- (s217) the crescent's roll: it lies in the plane the blade swept (0 flat, 90 upright, 45 "/", 135 "\" seen from behind)
    {
        int rule = -1;
        float mv = 0.03f, rel = KBB_ROLL_REL_T;  // 3 KH cm at to_u 0.01, 30 %
        float ys[4] = {0, 0.7f, -2.0f, 3.1f};
        for (int yi = 0; yi < 4; yi++) {
            float y = ys[yi], P[6], N[6];
            // horizontal sweep, right -> left and left -> right: flat
            seg_at(P, y, 0.3f, 0.2f, 1.0f, 0.4f), seg_at(N, y, -0.3f, 0.2f, 1.0f, 0.4f);
            CHECK(NEAR(kbb_roll_swing(P, N, y, mv, rel, &rule), 0) && rule == KBB_RULE_SWEEP);
            CHECK(NEAR(kbb_roll_swing(N, P, y, mv, rel, &rule), 0) && rule == KBB_RULE_SWEEP);
            // overhead (tip moving down): upright
            seg_at(P, y, 0, 0.6f, 1.6f, 0.3f), seg_at(N, y, 0, 0.6f, 1.2f, 0.5f);
            CHECK(NEAR(kbb_roll_swing(P, N, y, mv, rel, &rule), 90) && rule == KBB_RULE_SWEEP);
            // diagonals: down-right = "\" 135, down-left = "/" 45, up-right = "/" 45
            seg_at(P, y, -0.2f, 0.4f, 1.5f, 0.4f), seg_at(N, y, 0.2f, 0.4f, 1.1f, 0.4f);
            CHECK(NEAR(kbb_roll_swing(P, N, y, mv, rel, &rule), 135) && rule == KBB_RULE_SWEEP);
            seg_at(P, y, 0.2f, 0.4f, 1.5f, 0.4f), seg_at(N, y, -0.2f, 0.4f, 1.1f, 0.4f);
            CHECK(NEAR(kbb_roll_swing(P, N, y, mv, rel, &rule), 45) && rule == KBB_RULE_SWEEP);
            CHECK(NEAR(kbb_roll_swing(N, P, y, mv, rel, &rule), 45) && rule == KBB_RULE_SWEEP);
            // a lunge adds a move along the flight: still the sweep (0.2 across of 0.54)
            seg_at(P, y, 0.1f, 0.2f, 1.0f, 0.2f), seg_at(N, y, -0.1f, 0.2f, 1.0f, 0.7f);
            CHECK(NEAR(kbb_roll_swing(P, N, y, mv, rel, &rule), 0) && rule == KBB_RULE_SWEEP);
            // the tip barely moved (1 cm): the blade's own line (pointing up + forward = upright)
            seg_at(P, y, 0, 0.4f, 1.5f, 0.5f), seg_at(N, y, 0, 0.4f, 1.5f, 0.5f);  // (not moved at all; blade up + forward)
            CHECK(NEAR(kbb_roll_swing(P, N, y, mv, rel, &rule), 90) && rule == KBB_RULE_BLADE);
            seg_at(P, y, 0.61f, 1.0f, 1.0f, 0.5f), seg_at(N, y, 0.6f, 1.0f, 1.0f, 0.5f);  // 1 cm across; blade level, to the right
            CHECK(NEAR(kbb_roll_swing(P, N, y, mv, rel, &rule), 0) && rule == KBB_RULE_BLADE);
            // thrusts: tip and blade along the flight -> flat; a small sideways jitter (5 cm of 40) does not count
            seg_at(P, y, 0, 1.0f, 1.0f, 0.2f), seg_at(N, y, 0, 1.0f, 1.0f, 0.6f);
            CHECK(NEAR(kbb_roll_swing(P, N, y, mv, rel, &rule), 0) && rule == KBB_RULE_THRUST);
            seg_at(N, y, 0.05f, 1.0f, 1.0f, 0.6f);
            CHECK(NEAR(kbb_roll_swing(P, N, y, mv, rel, &rule), 0) && rule == KBB_RULE_THRUST);
        }
        // the modes, the override, the test crescent
        float P[6], N[6];
        seg_at(P, 0, 0, 0.6f, 1.6f, 0.3f), seg_at(N, 0, 0, 0.6f, 1.2f, 0.5f);  // an overhead swing: 90 when followed
        CHECK(NEAR(kbb_roll_pick(1, -1, false, P, N, 0, mv, rel, &rule), 90) && rule == KBB_RULE_SWEEP);
        CHECK(kbb_roll_pick(0, -1, false, P, N, 0, mv, rel, &rule) == -1 && rule == KBB_RULE_OLD);   // mode 0 = the s216 arch
        CHECK(NEAR(kbb_roll_pick(2, -1, false, P, N, 0, mv, rel, &rule), 0) && rule == KBB_RULE_FLAT);
        CHECK(NEAR(kbb_roll_pick(1, 30, false, P, N, 0, mv, rel, &rule), 30) && rule == KBB_RULE_OVERRIDE);
        CHECK(NEAR(kbb_roll_pick(0, 30, false, P, N, 0, mv, rel, &rule), 30) && rule == KBB_RULE_OVERRIDE);  // wins over mode 0 too
        CHECK(NEAR(kbb_roll_pick(2, 200, false, P, N, 0, mv, rel, &rule), 20) && rule == KBB_RULE_OVERRIDE);
        CHECK(NEAR(kbb_roll_pick(1, 0, false, P, N, 0, mv, rel, &rule), 0) && rule == KBB_RULE_OVERRIDE);
        CHECK(NEAR(kbb_roll_pick(1, -1, true, NULL, NULL, 0, 0, 0, &rule), 0) && rule == KBB_RULE_TEST);  // kbbeamtest: flat
        CHECK(NEAR(kbb_roll_pick(2, -1, true, NULL, NULL, 0, 0, 0, &rule), 0) && rule == KBB_RULE_TEST);
        CHECK(NEAR(kbb_roll_pick(1, 60, true, NULL, NULL, 0, 0, 0, &rule), 60) && rule == KBB_RULE_OVERRIDE);  // ... or the override
        CHECK(kbb_roll_pick(0, -1, true, NULL, NULL, 0, 0, 0, &rule) == -1 && rule == KBB_RULE_OLD);
        CHECK(NEAR(kbb_roll_pick(1, -1, false, NULL, N, 0, mv, rel, &rule), 0) && rule == KBB_RULE_TEST);  // (no segment: flat)
        // the nearest pre-rolled crescent (15 deg steps, 180 = 0)
        CHECK(KBB_NROLL == 12 && KBB_NRULE == (int)(sizeof KBB_RULE / sizeof KBB_RULE[0]));
        CHECK(kbb_roll_slot(0) == 0 && kbb_roll_slot(7) == 0 && kbb_roll_slot(8) == 1 && kbb_roll_slot(45) == 3 &&
              kbb_roll_slot(90) == 6 && kbb_roll_slot(135) == 9 && kbb_roll_slot(172) == 11 && kbb_roll_slot(173) == 0 &&
              kbb_roll_slot(179.9f) == 0 && kbb_roll_slot(-10) == 11 && kbb_roll_slot(180) == 0);
        CHECK(NEAR(kbb_fold180(-10), 170) && NEAR(kbb_fold180(370), 10) && NEAR(kbb_fold180(180), 0));
        // end to end in the WORLD: the tip moves in any direction across the flight; the chosen pre-rolled crescent's width (kbownfx
        // roll_verts of KH +x, then sora_fx.c emit_inst's placement, copied below) lies along that move within half a step
        int worst_ok = 1;
        float worst = 0;
        for (int yi = 0; yi < 8; yi++) {
            float y = -3.0f + yi * 0.8f;
            for (int a = 0; a < 360; a += 7) {
                float ar = a * 3.14159265f / 180, P2[6], N2[6];
                seg_at(P2, y, 0, 0.3f, 1.2f, 0.4f);
                seg_at(N2, y, 0.25f * cosf(ar), 0.3f, 1.2f + 0.25f * sinf(ar), 0.45f);  // (+ a little forward)
                float roll = kbb_roll_pick(1, -1, false, P2, N2, y, mv, rel, &rule);
                float k = kbb_roll_slot(roll) * (float)KBB_ROLL_STEP * 3.14159265f / 180;
                float kh_w[3] = {cosf(k), -sinf(k), 0}, w[3];  // kbownfx roll_verts: (1, 0, 0) -> (cos, -sin, 0)
                emit_unbound(kh_w, y, w);
                float v[3] = {N2[3] - P2[3], N2[4] - P2[4], N2[5] - P2[5]}, f3[3] = {sinf(y), 0, cosf(y)};
                float vf = v[0] * f3[0] + v[2] * f3[2];
                for (int c = 0; c < 3; c++) v[c] -= vf * f3[c];  // the move across the flight
                float cs = fabsf(v[0] * w[0] + v[1] * w[1] + v[2] * w[2]) / (kbb_len3(v) * kbb_len3(w));
                float ang = acosf(cs > 1 ? 1 : cs) * 180 / 3.14159265f;
                if (ang > worst) worst = ang;
                if (rule != KBB_RULE_SWEEP || ang > KBB_ROLL_STEP / 2.0f + 0.01f) worst_ok = 0;
            }
        }
        CHECK(worst_ok);
        printf("  roll end to end: worst angle between the swept line and the drawn crescent %.2f deg (half a step = %.1f)\n", worst,
               KBB_ROLL_STEP / 2.0f);
    }

    printf("kbbeam_test: %d/%d passed\n", runs - fails, runs);
    return fails != 0;
}

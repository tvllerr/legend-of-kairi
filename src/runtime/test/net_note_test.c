// host test for runtime/sora_net_note.h (s209): the pure parts of Melty's lab notes on the game's side: the on-screen ball
// test against a camera, the "was his sender moving" read of a peer's SNAP ring, the release id in Melty's setup marker
// and the HELLO's version string; (s209 step 2) the attack / jump slot table (sora_net_act.h) on the real ptya tables, the
// HURT step, and the autopilot (sora_net_pilot.h) against a fake game + a C copy of the kit's PilotSteer: arrive, stuck,
// autopilot off = nothing moves, face, attack, act, stop, replaced and unsupported commands.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/net_note_test.c -o build_h/net_note_test.exe && build_h/net_note_test.exe
//   (Linux: the same)
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sora_net_note.h"
#include "sora_net_act.h"
#include "sora_params_valor.h"
#include "sora_params_roxas.h"
#include "sora_net_pilot.h"
#pragma clang diagnostic ignored "-Wunused-function"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// a camera at eye e looking along yaw (0 = +z, turning towards +x), level: the view matrix as linkpos_camera_view gives it
// (rows right / up / back, translation in column 3: eye-space = R * (p - e))
static void cam(float v[12], const float e[3], float yaw) {
    float fx = sinf(yaw), fz = cosf(yaw);           // forward
    float r[3] = {-fz, 0, fx}, u[3] = {0, 1, 0};     // right = up x forward... (BotW: right of facing (sin, cos) is (-cos, sin))
    float b[3] = {-fx, 0, -fz};                      // back = -forward
    const float *R[3] = {r, u, b};
    for (int i = 0; i < 3; i++) {
        v[i * 4 + 0] = R[i][0], v[i * 4 + 1] = R[i][1], v[i * 4 + 2] = R[i][2];
        v[i * 4 + 3] = -(R[i][0] * e[0] + R[i][1] * e[1] + R[i][2] * e[2]);
    }
}

static void snap_at(NetPeer *p, uint64_t at, float x, float y, float z) {
    NetSnap s;
    memset(&s, 0, sizeof s);
    s.player = 7, s.seq = (uint16_t)(p->n + 1 + p->last_seq);
    s.feet[0] = x, s.feet[1] = y, s.feet[2] = z;
    peer_on_snap(p, &s, at);
}

// ---- (s209 step 2) the autopilot's fake game: Sora walks where the stick says (the driving tick's own formula:
// dir = cf * sy + (-cf.z, cf.x) * sx), the lab's queue, and a C copy of the kit's PilotSteer (melty_addon.h, y up; the real
// one is checked through the connector in runtime/melty/melty_addon_test.cpp)
static struct {
    bool on;
    sora_melty_pilot q[16]; int nq, rq;
    int nrep, rnum[512], rstate[512]; float rdist[512];
    float face; int faces;
    float peer[3]; int peer_id;
    int left, queued_last, nqueue; unsigned dropped;
    float tx, tz, r, best, since; bool active, jumped;  // the steer copy
    float feet[3], cf[2]; bool wall;
    int took_n, took_at_nqueue, took_at_ntarget, ntarget;  // (review 2) the hand-over: when, and what the pilot had done before it
} FG;
static bool fg_on(void) { return FG.on; }
static int fg_take(sora_melty_pilot *o) { if (FG.rq >= FG.nq) return 0; *o = FG.q[FG.rq++]; return 1; }
static void fg_report(int n, int st, float d) { if (FG.nrep < 512) FG.rnum[FG.nrep] = n, FG.rstate[FG.nrep] = st, FG.rdist[FG.nrep] = d, FG.nrep++; }
static void fg_target(float x, float y, float z, float r) { (void)y; FG.ntarget++; FG.tx = x, FG.tz = z, FG.r = r > 0 ? r : 1, FG.best = INFINITY, FG.since = 0, FG.active = true, FG.jumped = false; }
static void fg_stop(void) { FG.active = false; }
static void fg_update(float dt, float x, float y, float z, sora_melty_steer *o) {
    (void)y;
    memset(o, 0, sizeof *o);
    if (!FG.active) return;
    float da = FG.tx - x, db = FG.tz - z, d = sqrtf(da * da + db * db);
    o->distance = d, o->heading = atan2f(da, db), o->state = 1;
    if (d <= FG.r) { o->arrived = 1, o->state = 2, FG.active = false; return; }
    if (d < FG.best - 0.25f) FG.best = d, FG.since = 0, FG.jumped = false;
    else FG.since += dt > 0 ? dt : 0;
    if (!FG.jumped && FG.since >= 1.5f) o->want_jump = 1, FG.jumped = true;
    if (FG.since >= 5) o->stuck = 1, o->state = 3, FG.active = false;
}
static void fg_face(float y) { FG.face = y, FG.faces++; }
static bool fg_peer(int peer, float out[3]) { if (peer != FG.peer_id) return false; memcpy(out, FG.peer, sizeof FG.peer); return true; }
static void fg_attack(int n) { FG.nqueue++, FG.queued_last = n, FG.left = n; }
static int fg_left(void) { return FG.left; }
static unsigned fg_dropped(void) { return FG.dropped; }
static void fg_took(void) { FG.took_n++, FG.took_at_nqueue = FG.nqueue, FG.took_at_ntarget = FG.ntarget; }
static NetPilot g_p;
static void fg_reset(void) {
    memset(&FG, 0, sizeof FG);
    FG.cf[1] = 1;
    memset(&g_p, 0, sizeof g_p);
    g_p.io = (NpIo){fg_on, fg_take, fg_report, fg_target, fg_stop, fg_update, fg_face, fg_peer, fg_attack, fg_left, fg_dropped, fg_took};
}
static void fg_cmd(int number, int kind) {
    sora_melty_pilot *c = &FG.q[FG.nq++];
    memset(c, 0, sizeof *c);
    c->number = number, c->kind = kind;
}
// n frames of dt: the pilot ticks, then Sora moves by its stick (unless a wall holds him); -> frames until a final state for
// `number` (arrived / stuck / failed / idle), or -1
static int fg_run(int n, float dt, bool driven, int number) {
    for (int i = 0; i < n; i++) {
        np_tick(&g_p, driven, dt, FG.feet, FG.cf);
        for (int k = 0; k < FG.nrep; k++)
            if (FG.rnum[k] == number && FG.rstate[k] != SORA_MELTY_PILOT_MOVING) return i;
        float st[2];
        if (driven && !FG.wall && np_stick(&g_p, st)) {
            float mag = sqrtf(st[0] * st[0] + st[1] * st[1]), r[2] = {-FG.cf[1], FG.cf[0]};
            float dir[2] = {FG.cf[0] * st[1] + r[0] * st[0], FG.cf[1] * st[1] + r[1] * st[0]};
            float speed = mag >= 0.8f ? 6.0f : 2.0f;  // m/s (run / walk: any speed does for the test)
            FG.feet[0] += dir[0] / (mag > 0 ? mag : 1) * speed * dt, FG.feet[2] += dir[1] / (mag > 0 ? mag : 1) * speed * dt;
        }
    }
    return -1;
}
static int fg_last_state(int number) {
    for (int k = FG.nrep - 1; k >= 0; k--)
        if (FG.rnum[k] == number) return FG.rstate[k];
    return -1;
}

int main(void) {
    const float D = 3.14159265f / 180;
    float pr[4] = {0.31f, 25000, 50 * D, 16.0f / 9};
    float v[12], e[3] = {10, 2, 10};

    printf("1. on screen: a 1 m ball in the camera's view\n");
    cam(v, e, 0);  // looking along +z
    float c[3];
    c[0] = 10, c[1] = 2, c[2] = 20;
    CHECK(nnote_on_screen(v, pr, c, 1));                // 10 m straight ahead
    c[2] = 0;
    CHECK(!nnote_on_screen(v, pr, c, 1));               // 10 m behind
    c[2] = 10.5f;
    CHECK(nnote_on_screen(v, pr, c, 1));                // half a metre ahead: inside the near plane's reach with its radius
    c[2] = 10 - 2.0f;
    CHECK(!nnote_on_screen(v, pr, c, 1));               // 2 m behind the eye
    // the side: tan(25 deg) * 16/9 = 0.829 a metre ahead at 10 m: 8.29 m; with the ball's 1 m and the plane's slant ~ 9.6 m
    c[1] = 2, c[2] = 20;
    c[0] = 10 - 8.0f;
    CHECK(nnote_on_screen(v, pr, c, 1));                // (BotW's right is -x when facing +z: either side the same)
    c[0] = 10 + 8.0f;
    CHECK(nnote_on_screen(v, pr, c, 1));
    c[0] = 10 + 11.0f;
    CHECK(!nnote_on_screen(v, pr, c, 1));
    c[0] = 10 - 11.0f;
    CHECK(!nnote_on_screen(v, pr, c, 1));
    c[0] = 10 + 9.2f;
    CHECK(nnote_on_screen(v, pr, c, 1) && !nnote_on_screen(v, pr, c, 0));  // (in only with its radius)
    // up / down: tan(25 deg) * 10 = 4.66 m
    c[0] = 10, c[1] = 2 + 5.5f;
    CHECK(nnote_on_screen(v, pr, c, 1));
    c[1] = 2 + 7;
    CHECK(!nnote_on_screen(v, pr, c, 1));
    c[1] = 2 - 7;
    CHECK(!nnote_on_screen(v, pr, c, 1));
    // far
    c[1] = 2, c[2] = 10 + 26000;
    CHECK(!nnote_on_screen(v, pr, c, 1));
    // turned: the camera looks along +x now
    cam(v, e, 90 * D);
    c[0] = 20, c[1] = 2, c[2] = 10;
    CHECK(nnote_on_screen(v, pr, c, 1));
    c[0] = 10, c[2] = 20;
    CHECK(!nnote_on_screen(v, pr, c, 1));  // (now 90 deg to the side)
    // nonsense in: never "on screen"
    float bad[4] = {0.31f, 25000, 0, 16.0f / 9};
    c[0] = 20;
    CHECK(!nnote_on_screen(v, bad, c, 1));
    bad[2] = 50 * D, bad[3] = 0;
    CHECK(!nnote_on_screen(v, bad, c, 1));
    c[0] = NAN;
    CHECK(!nnote_on_screen(v, pr, c, 1));
    CHECK(!nnote_on_screen(NULL, pr, c, 1) && !nnote_on_screen(v, NULL, c, 1) && !nnote_on_screen(v, pr, NULL, 1));
    c[0] = 20;
    CHECK(!nnote_on_screen(v, pr, c, -1) && !nnote_on_screen(v, pr, c, NAN));

    printf("2. moving: his two SNAPs around the time drawn (now - %d ms)\n", NET_INTERP_MS);
    NetPeer p;
    memset(&p, 0, sizeof p);
    CHECK(!nnote_peer_moving(&p, 1000, 1, 0.1f));  // (nobody)
    snap_at(&p, 1000, 0, 0, 0);
    CHECK(!nnote_peer_moving(&p, 1200, 1, 0.1f));  // (one SNAP: unknown = not moving)
    snap_at(&p, 1050, 0.25f, 0, 0);                // 5 m/s
    snap_at(&p, 1100, 0.50f, 0, 0);
    snap_at(&p, 1150, 0.50f, 0, 0);                // stopped
    snap_at(&p, 1200, 0.50f, 0, 0);
    CHECK(nnote_peer_moving(&p, 1050 + NET_INTERP_MS + 10, 1, 0.1f));   // drawn between 1050 and 1100: moving
    CHECK(!nnote_peer_moving(&p, 1150 + NET_INTERP_MS + 10, 1, 0.1f));  // drawn between 1150 and 1200: still
    CHECK(!nnote_peer_moving(&p, 9000, 1, 0.1f));                        // past the newest: the newest two (still)
    CHECK(nnote_peer_moving(&p, 900, 1, 0.1f));                          // before the oldest: the oldest two (moving)
    CHECK(!nnote_peer_moving(&p, 1050 + NET_INTERP_MS + 10, 0.01f, 0.1f));  // (5 m/s in units of 1 cm = 0.05 m/s: still)
    NetPeer q;
    memset(&q, 0, sizeof q);
    snap_at(&q, 2000, 0, 0, 0);
    snap_at(&q, 2000, 5, 0, 0);  // (two that came together: no speed to read)
    CHECK(!nnote_peer_moving(&q, 2000 + NET_INTERP_MS, 1, 0.1f));
    memset(&q, 0, sizeof q);
    snap_at(&q, 3000, 0, 0, 0);
    snap_at(&q, 3050, 0.004f, 0, 0);  // 0.08 m/s: under the kit's 0.1
    CHECK(!nnote_peer_moving(&q, 3050 + NET_INTERP_MS, 1, 0.1f));
    snap_at(&q, 3100, 0.0104f, 0, 0);  // 0.128 m/s
    CHECK(nnote_peer_moving(&q, 3100 + NET_INTERP_MS, 1, 0.1f));
    CHECK(!nnote_peer_moving(NULL, 0, 1, 0.1f));

    printf("3. the release id in Melty's setup marker, and the HELLO's version\n");
    char id[17], ver[64];
    CHECK(nnote_ready_id("{\"ready\": \"savage-wind-kairi-melty-v2:8fff2ca146b9805a\", \"at\": \"2026-11-11T10:00:00\"}", id) && !strcmp(id, "8fff2ca146b9805a"));
    CHECK(nnote_ready_id("{\"at\":\"x\",\"ready\":\"a:b:8FFF2CA146B9805A\"}", id) && !strcmp(id, "8fff2ca146b9805a"));  // (the last ':', lowercase)
    CHECK(!nnote_ready_id("{\"ready\": \"savage-wind-kairi-melty-v2:8fff2ca146b9805\"}", id) && !id[0]);   // 15 hex
    CHECK(!nnote_ready_id("{\"ready\": \"savage-wind-kairi-melty-v2:8fff2ca146b9805ab\"}", id) && !id[0]); // 17 hex
    CHECK(!nnote_ready_id("{\"ready\": \"savage-wind-kairi-melty-v2:8fff2ca146b9805g\"}", id) && !id[0]);  // not hex
    CHECK(!nnote_ready_id("{\"ready\": \"8fff2ca146b9805a\"}", id));  // (no ':')
    CHECK(!nnote_ready_id("{\"ready\": 5}", id) && !nnote_ready_id("{\"ready\" \"x:8fff2ca146b9805a\"}", id));
    CHECK(!nnote_ready_id("{\"ready\": \"x:8fff2ca146b9805a", id));  // (no closing quote)
    CHECK(!nnote_ready_id("", id) && !nnote_ready_id(NULL, id) && !nnote_ready_id("x", NULL));
    nnote_version(ver, (int)sizeof ver, "{\"ready\": \"savage-wind-kairi-melty-v2:8fff2ca146b9805a\"}", 11);
    CHECK(!strcmp(ver, "8fff2ca146b9805a+net11"));
    nnote_version(ver, (int)sizeof ver, "", 11);
    CHECK(!strcmp(ver, "dev+net11"));
    nnote_version(ver, (int)sizeof ver, NULL, 12);
    CHECK(!strcmp(ver, "dev+net12"));
    nnote_version(ver, 5, NULL, 11);
    CHECK(!strcmp(ver, "dev+"));  // (cut to fit, never past the buffer)
    CHECK(strlen("8fff2ca146b9805a+net11") <= 64);  // (a note's label: 64 at most)

    printf("4. (step 2) attack / jump slots from the real ptya tables (both sides read the same)\n");
    int mo;
    int n0 = (int)(sizeof kh_ptya / sizeof *kh_ptya);
    CHECK(nact_kind(kh_ptya[0].motion * 4 + 3, kh_ptya, n0, &mo) == NACT_ATTACK && mo == kh_ptya[0].motion);  // (A340: 191)
    for (int v = 0; v < 4; v++) CHECK(nact_kind(kh_ptya[5].motion * 4 + v, kh_ptya, n0, NULL) == NACT_ATTACK);  // every variant
    for (int v = 0; v < 4; v++) CHECK(nact_kind(NACT_MOT_JUMP * 4 + v, kh_ptya, n0, &mo) == NACT_JUMP && mo == 3);
    CHECK(nact_kind(0, kh_ptya, n0, NULL) == NACT_NONE && nact_kind(1 * 4 + 3, kh_ptya, n0, NULL) == NACT_NONE);  // idle, walk
    CHECK(nact_kind(-1, kh_ptya, n0, &mo) == NACT_NONE && mo == -1);
    CHECK(nact_kind(valor_ptya[0].motion * 4, valor_ptya, VALOR_PTYA_N, NULL) == NACT_ATTACK);
    {   // a base Sora attack Roxas' table lacks is no attack of Roxas'
        int miss = -1;
        for (int i = 0; i < n0 && miss < 0; i++) {
            bool in = false;
            for (int j = 0; j < ROXAS_PTYA_N; j++) in = in || roxas_ptya[j].motion == kh_ptya[i].motion;
            if (!in) miss = kh_ptya[i].motion;
        }
        CHECK(miss >= 0 && nact_kind(miss * 4, roxas_ptya, ROXAS_PTYA_N, NULL) == NACT_NONE && nact_kind(miss * 4, kh_ptya, n0, NULL) == NACT_ATTACK);
    }
    CHECK(nact_kind(NACT_MOT_JUMP * 4, NULL, 0, NULL) == NACT_JUMP && nact_kind(500, NULL, 0, NULL) == NACT_NONE);
    char det[24];
    nact_detail(det, (int)sizeof det, 191);
    CHECK(!strcmp(det, "m191") && !strcmp(nact_name(NACT_ATTACK), "attack") && !strcmp(nact_name(NACT_JUMP), "jump") && !*nact_name(NACT_NONE));

    printf("5. (step 2) HURT: a raw life drop between counted frames\n");
    int prev = -1;
    CHECK(nnote_hurt_step(&prev, 12, false) == 0 && prev == 12);  // (the first frame: nothing to compare)
    CHECK(nnote_hurt_step(&prev, 10, false) == 2);
    CHECK(nnote_hurt_step(&prev, 10, false) == 0);
    CHECK(nnote_hurt_step(&prev, 12, false) == 0);   // (a heal / Second Chance's life back: never a hurt)
    CHECK(nnote_hurt_step(&prev, 4, true) == 0 && prev == -1);  // (a load / Game Over / Mickey's rescue: starts over)
    CHECK(nnote_hurt_step(&prev, 8, false) == 0);    // (the life after it is no drop)
    CHECK(nnote_hurt_step(&prev, 0, false) == 8);    // (the killing blow counts)
    CHECK(nnote_hurt_step(&prev, -1, false) == 0 && prev == -1);

    printf("6. (step 2) autopilot off: nothing is taken, nothing moves, nothing is pressed or said\n");
    fg_reset();
    FG.on = false;
    fg_cmd(1, SORA_MELTY_PILOT_GOTO), FG.q[0].x = 10;
    fg_cmd(2, SORA_MELTY_PILOT_ACT), snprintf(FG.q[1].action, sizeof FG.q[1].action, "attack");
    fg_cmd(3, SORA_MELTY_PILOT_ACT), snprintf(FG.q[2].action, sizeof FG.q[2].action, "jump");
    float st[2];
    CHECK(fg_run(300, 1.0f / 30, true, 1) == -1);
    CHECK(FG.rq == 0 && FG.nrep == 0 && FG.nqueue == 0 && FG.faces == 0 && !FG.active && FG.feet[0] == 0 && FG.feet[2] == 0);
    CHECK(!np_stick(&g_p, st) && !np_take_jump(&g_p, false));

    printf("7. (step 2) goto: walks there on the stick and arrives, whatever way the camera faces\n");
    float cams[4][2] = {{0, 1}, {1, 0}, {-0.6f, -0.8f}, {0.7071f, -0.7071f}};
    for (int c = 0; c < 4; c++) {
        fg_reset();
        FG.on = true, FG.cf[0] = cams[c][0], FG.cf[1] = cams[c][1];
        fg_cmd(7, SORA_MELTY_PILOT_GOTO), FG.q[0].x = 10, FG.q[0].y = 3, FG.q[0].z = -4;
        int fr = fg_run(600, 1.0f / 30, true, 7);
        float dx = FG.feet[0] - 10, dz = FG.feet[2] + 4;
        CHECK(fr > 0 && fr < 120 && fg_last_state(7) == SORA_MELTY_PILOT_ARRIVED && sqrtf(dx * dx + dz * dz) <= 1.01f);
        CHECK(FG.rstate[0] == SORA_MELTY_PILOT_MOVING && FG.rnum[0] == 7);  // (moving first)
        CHECK(!np_stick(&g_p, st));  // (arrived: the stick let go)
    }
    // walk: the stick at NP_WALK_STICK (KH2 walks under its run threshold 0.8 after the round dead zone of 60 / 127.5)
    fg_reset();
    FG.on = true;
    fg_cmd(8, SORA_MELTY_PILOT_GOTO), FG.q[0].z = 20, snprintf(FG.q[0].gait, sizeof FG.q[0].gait, "walk");
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    CHECK(np_stick(&g_p, st) && fabsf(st[1] - NP_WALK_STICK) < 1e-4f && fabsf(st[0]) < 1e-4f);
    float r127 = NP_WALK_STICK * 127.5f, mag = (r127 - 60.0f) / (127.5f - 60.0f);
    CHECK(mag > 0 && mag < 0.8f);
    // the stick for a heading, on its own: heading 90 deg (+x) with the camera along +z = full right
    float hs[2], cfz[2] = {0, 1};
    np_stick_for(1.5707963f, cfz, 1, hs);
    // (which sign is "right" is the driving tick's: (-cf.z, cf.x) = (-1, 0) for cf +z, so +x is stick x = -1 there)
    CHECK(fabsf(hs[0] + 1.0f) < 1e-4f && fabsf(hs[1]) < 1e-4f);
    np_stick_for(0, NULL, 1, hs);  // (no camera: the driving tick's own (0, 1))
    CHECK(fabsf(hs[1] - 1) < 1e-4f && fabsf(hs[0]) < 1e-4f);

    printf("8. (step 2) goto into a wall: one jump asked after 1.5 s, stuck after 5 s, the stick let go\n");
    fg_reset();
    FG.on = true, FG.wall = true;
    fg_cmd(9, SORA_MELTY_PILOT_GOTO), FG.q[0].x = 30;
    int jumps = 0, fr = -1;
    for (int i = 0; i < 400 && fr < 0; i++) {
        if (fg_run(1, 1.0f / 30, true, 9) == 0) fr = i;
        if (np_take_jump(&g_p, false)) jumps++;
    }
    CHECK(fr >= 140 && fr <= 160 && fg_last_state(9) == SORA_MELTY_PILOT_STUCK && jumps == 1 && !np_stick(&g_p, st));
    CHECK(fabsf(FG.rdist[FG.nrep - 1] - 30) < 1e-3f);  // (stuck says how far is left)
    // a fresh frame never takes the jump press
    g_p.jump = true;
    CHECK(!np_take_jump(&g_p, true) && np_take_jump(&g_p, false) && !np_take_jump(&g_p, false));
    // not driven (a load, a menu): the stuck clock waits
    fg_reset();
    FG.on = true, FG.wall = true;
    fg_cmd(10, SORA_MELTY_PILOT_GOTO), FG.q[0].x = 30;
    CHECK(fg_run(600, 1.0f / 30, false, 10) == -1 && fg_last_state(10) == SORA_MELTY_PILOT_MOVING);
    // autopilot turned off mid-goto: let go at once
    fg_reset();
    FG.on = true;
    fg_cmd(11, SORA_MELTY_PILOT_GOTO), FG.q[0].x = 30;
    fg_run(10, 1.0f / 30, true, 11);
    CHECK(np_stick(&g_p, st));
    FG.on = false;
    float before = FG.feet[0];
    fg_run(30, 1.0f / 30, true, 11);
    CHECK(!np_stick(&g_p, st) && !FG.active && FG.feet[0] == before && g_p.mode == NP_NONE);

    printf("9. (step 2) face, attack <peer>, act attack / jump, stop, a replaced command, the ones not done\n");
    fg_reset();
    FG.on = true;
    fg_cmd(20, SORA_MELTY_PILOT_FACE), FG.q[0].yaw = 1.25f;
    fg_run(1, 1.0f / 30, true, 20);
    CHECK(FG.faces == 1 && FG.face == 1.25f && fg_last_state(20) == SORA_MELTY_PILOT_ARRIVED);
    // attack peer 4, drawn 5 m along +x: faced (atan2(5, 0) = 90 deg), one press; arrived once it fired
    FG.peer_id = 4, FG.peer[0] = 5;
    fg_cmd(21, SORA_MELTY_PILOT_ATTACK), FG.q[FG.nq - 1].peer = 4;
    CHECK(fg_run(3, 1.0f / 30, true, 21) == -1 && fg_last_state(21) == SORA_MELTY_PILOT_MOVING);
    CHECK(FG.queued_last == 1 && fabsf(FG.face - 1.5707963f) < 1e-4f);
    FG.left = 0;
    CHECK(fg_run(1, 1.0f / 30, true, 21) == 0 && fg_last_state(21) == SORA_MELTY_PILOT_ARRIVED);
    // a player not drawn here: failed, nothing pressed
    int nq0 = FG.nqueue;
    fg_cmd(22, SORA_MELTY_PILOT_ATTACK), FG.q[FG.nq - 1].peer = 9;
    fg_run(1, 1.0f / 30, true, 22);
    CHECK(fg_last_state(22) == SORA_MELTY_PILOT_FAILED && FG.nqueue == nq0);
    // act attack 2 (s): 7 presses of 300 ms; dropped unfired (no driven frame for 15 s) = failed
    fg_cmd(23, SORA_MELTY_PILOT_ACT), snprintf(FG.q[FG.nq - 1].action, 40, "attack"), FG.q[FG.nq - 1].has_seconds = 1, FG.q[FG.nq - 1].seconds = 2;
    fg_run(1, 1.0f / 30, true, 23);
    CHECK(FG.queued_last == 7 && fg_last_state(23) == SORA_MELTY_PILOT_MOVING);
    FG.left = 0, FG.dropped++;
    fg_run(1, 1.0f / 30, true, 23);
    CHECK(fg_last_state(23) == SORA_MELTY_PILOT_FAILED);
    CHECK(np_presses(0, 0) == 1 && np_presses(1, 0.1f) == 1 && np_presses(1, 0.31f) == 2 && np_presses(1, 600) == NP_PRESS_MAX);
    // act jump: one jump press, done
    fg_cmd(24, SORA_MELTY_PILOT_ACT), snprintf(FG.q[FG.nq - 1].action, 40, "jump");
    fg_run(1, 1.0f / 30, true, 24);
    CHECK(fg_last_state(24) == SORA_MELTY_PILOT_ARRIVED && np_take_jump(&g_p, false));
    // act <other>, level, wander, die: not done
    fg_cmd(25, SORA_MELTY_PILOT_ACT), snprintf(FG.q[FG.nq - 1].action, 40, "dance");
    fg_cmd(26, SORA_MELTY_PILOT_LEVEL);
    fg_cmd(27, SORA_MELTY_PILOT_WANDER);
    fg_cmd(28, SORA_MELTY_PILOT_DIE);
    fg_cmd(29, SORA_MELTY_PILOT_EVENT);
    fg_run(1, 1.0f / 30, true, 29);
    CHECK(fg_last_state(25) == SORA_MELTY_PILOT_FAILED && fg_last_state(26) == SORA_MELTY_PILOT_FAILED && fg_last_state(27) == SORA_MELTY_PILOT_FAILED &&
          fg_last_state(28) == SORA_MELTY_PILOT_FAILED && fg_last_state(29) == SORA_MELTY_PILOT_FAILED);
    // a goto replaced by stop: the goto ends idle, stop is done, the stick let go
    fg_cmd(30, SORA_MELTY_PILOT_GOTO), FG.q[FG.nq - 1].x = 50;
    fg_run(5, 1.0f / 30, true, 30);
    CHECK(np_stick(&g_p, st));
    fg_cmd(31, SORA_MELTY_PILOT_STOP);
    fg_run(1, 1.0f / 30, true, 31);
    CHECK(fg_last_state(30) == SORA_MELTY_PILOT_IDLE && fg_last_state(31) == SORA_MELTY_PILOT_ARRIVED && !np_stick(&g_p, st) && !FG.active);
    // presses running, then stay: dropped (attack(0)), done
    fg_cmd(32, SORA_MELTY_PILOT_ACT), snprintf(FG.q[FG.nq - 1].action, 40, "attack");
    fg_run(1, 1.0f / 30, true, 32);
    CHECK(FG.left == 1);
    fg_cmd(33, SORA_MELTY_PILOT_STAY);
    fg_run(1, 1.0f / 30, true, 33);
    CHECK(FG.left == 0 && fg_last_state(32) == SORA_MELTY_PILOT_IDLE && fg_last_state(33) == SORA_MELTY_PILOT_ARRIVED);
    // a goto with no number in it: failed
    fg_cmd(34, SORA_MELTY_PILOT_GOTO), FG.q[FG.nq - 1].x = NAN;
    fg_run(1, 1.0f / 30, true, 34);
    CHECK(fg_last_state(34) == SORA_MELTY_PILOT_FAILED);
    CHECK(g_p.cmds == 15 && g_p.failed == 8);

    printf("10. (review 1) face with a huge yaw: into -PI..PI before Sora's facing (wrap() would loop forever)\n");
    float huge[5] = {2e8f, 1e9f, -1e9f, 3e38f, 7.0f};
    for (int i = 0; i < 5; i++) {
        fg_reset();
        FG.on = true;
        fg_cmd(40 + i, SORA_MELTY_PILOT_FACE), FG.q[0].yaw = huge[i];
        fg_run(1, 1.0f / 30, true, 40 + i);
        CHECK(FG.faces == 1 && isfinite(FG.face) && fabsf(FG.face) <= 3.1415930f && fg_last_state(40 + i) == SORA_MELTY_PILOT_ARRIVED);
        // the same heading: sin / cos agree (for 7 rad exactly; the huge ones are whatever float holds)
        if (i == 4) CHECK(fabsf(FG.face - (7.0f - 6.2831853f)) < 1e-5f);
    }

    printf("11. (review 2) one owner: the test mode's self-drive until the first command, the autopilot after it\n");
    // order 1: the test mode drives first; autopilot on, no command yet: the pilot touches nothing
    fg_reset();
    FG.on = true;
    for (int i = 0; i < 60; i++) {
        np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
        FG.on = i % 7 != 3;  // (on / off flips: still nothing)
    }
    FG.on = true;
    CHECK(FG.took_n == 0 && !g_p.owns && FG.nqueue == 0 && FG.ntarget == 0 && FG.nrep == 0 && !np_stick(&g_p, st) && !np_take_jump(&g_p, false));
    g_p.jump = true, g_p.stick_on = true, g_p.mode = NP_GOTO;  // (even forced: no stick, no jump before the first command)
    CHECK(!np_stick(&g_p, st) && !np_take_jump(&g_p, false));
    g_p.jump = false, g_p.stick_on = false, g_p.mode = NP_NONE;
    // the first command: the hand-over first (before the command starts), once
    fg_cmd(50, SORA_MELTY_PILOT_GOTO), FG.q[0].x = 5;
    fg_cmd(51, SORA_MELTY_PILOT_ACT), snprintf(FG.q[1].action, 40, "attack");
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    CHECK(FG.took_n == 1 && FG.took_at_nqueue == 0 && FG.took_at_ntarget == 0 && g_p.owns && FG.ntarget == 1 && FG.nqueue == 1);
    fg_cmd(52, SORA_MELTY_PILOT_GOTO), FG.q[2].x = 5;
    fg_run(5, 1.0f / 30, true, 52);
    CHECK(FG.took_n == 1 && np_stick(&g_p, st));  // (never again: the autopilot owns them for the session)
    // order 2: the first command before the test mode ever drove: handed over at once, the same way
    fg_reset();
    FG.on = true;
    fg_cmd(60, SORA_MELTY_PILOT_STOP);
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    CHECK(FG.took_n == 1 && FG.took_at_nqueue == 0 && g_p.owns && FG.nqueue == 1 && fg_last_state(60) == SORA_MELTY_PILOT_ARRIVED);
    FG.on = false;
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    FG.on = true;
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    CHECK(FG.took_n == 1 && g_p.owns);  // (autopilot off and on again: still the autopilot's)

    printf("12. (review 3) a blip lets the running command go: answered failed when autopilot is back, first\n");
    fg_reset();
    FG.on = true;
    fg_cmd(70, SORA_MELTY_PILOT_GOTO), FG.q[0].x = 30;
    fg_run(5, 1.0f / 30, true, 70);
    CHECK(fg_last_state(70) == SORA_MELTY_PILOT_MOVING && np_stick(&g_p, st));
    FG.on = false;  // (the connection drops)
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    CHECK(!np_stick(&g_p, st) && !FG.active && fg_last_state(70) == SORA_MELTY_PILOT_MOVING && g_p.lost == 70);  // (no note while off)
    FG.on = true;
    fg_cmd(71, SORA_MELTY_PILOT_FACE), FG.q[1].yaw = 1;
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    int i70 = -1, i71 = -1;
    for (int k = 0; k < FG.nrep; k++) {
        if (FG.rnum[k] == 70 && FG.rstate[k] == SORA_MELTY_PILOT_FAILED) i70 = k;
        if (FG.rnum[k] == 71) i71 = k;
    }
    CHECK(i70 >= 0 && i71 > i70 && g_p.lost == 0 && fg_last_state(71) == SORA_MELTY_PILOT_ARRIVED);
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    int n70 = 0;
    for (int k = 0; k < FG.nrep; k++) n70 += FG.rnum[k] == 70 && FG.rstate[k] == SORA_MELTY_PILOT_FAILED;
    CHECK(n70 == 1);  // (said once)
    // presses running at the blip: dropped (they were the autopilot's), then failed
    fg_cmd(72, SORA_MELTY_PILOT_ACT), snprintf(FG.q[2].action, 40, "attack");
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    CHECK(FG.left == 1);
    FG.on = false;
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    CHECK(FG.left == 0 && g_p.lost == 72);
    FG.on = true;
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    CHECK(fg_last_state(72) == SORA_MELTY_PILOT_FAILED && g_p.lost == 0);
    // nothing running at a blip: nothing to answer
    FG.on = false, np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    FG.on = true;
    int nrep0 = FG.nrep;
    np_tick(&g_p, true, 1.0f / 30, FG.feet, FG.cf);
    CHECK(FG.nrep == nrep0 && g_p.lost == 0);

    printf("net_note_test: %d/%d passed\n", runs - fails, runs);
    return fails ? 1 : 0;
}

// host test for runtime/sora_net_movers_core.h (s290 labfix, netmovers: the end of a friend's stream). The pure parts only (no
// game, no socket, no exe): the scene-driven names left out, our place kept when his stream begins (once: a new stream on the way
// back keeps the first), the way back (ride / near / at once steps, never past our place, its body stopped there NM_SETTLE_N
// frames, then nothing more written), the wait while our Link stands where it goes back to, the give-up after NM_BACK_MS, and the
// lab's E3 case (his elevator ride stops in the middle: ours goes back to the bottom and stops there).
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/net_movers_test.c -o build_h/net_movers_test.exe
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "sora_net_proto.h"
#include "sora_net_objs_core.h"
#include "sora_net_movers_core.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1e-3)

static void at(float m[12], float x, float y, float z) {
    memset(m, 0, sizeof(float) * 12);
    m[0] = m[5] = m[10] = 1, m[3] = x, m[7] = y, m[11] = z;
}
static float dist(const float a[12], const float b[12]) {
    float dx = a[3] - b[3], dy = a[7] - b[7], dz = a[11] - b[11];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static void test_names(void) {
    // (1.9.0 ActorInfo: the shrine's entrance elevator + shutter, both kinds) left out; the rest of the movers stay
    CHECK(nm_scene_driven("DgnObj_EntranceElevator_A_01") && nm_scene_driven("DgnObj_EntranceElevatorSP"));
    CHECK(nm_scene_driven("DgnObj_EntranceShutter_A_01") && nm_scene_driven("DgnObj_EntranceShutterSP"));
    CHECK(nm_kind("DgnObj_EntranceElevator_A_01") && nm_kind("DgnObj_EntranceShutter_A_01"));  // (they were movers before s290)
    CHECK(!nm_scene_driven("DgnObj_RailLift_A_01") && !nm_scene_driven("DgnObj_MagneGear_A_01") && !nm_scene_driven("Obj_GerdoElevatorStep_A"));
    CHECK(!nm_scene_driven("RemainsWind_Barrier") && !nm_scene_driven("") && !nm_scene_driven(NULL) && !nm_scene_driven("DgnObj_Entranc"));
}

static void test_keep(void) {
    NmBack b;
    memset(&b, 0, sizeof b);
    float a[12], c[12];
    at(a, 4, 8.3f, 24), at(c, 4, 30, 24);
    nm_back_his(&b, a, true);  // (his stream begins: ours at the bottom kept)
    CHECK(b.have && !b.on && NEAR(b.own[7], 8.3f));
    CHECK(nm_back_start(&b, 1000) && b.on && b.since == 1000);  // (his over: back)
    nm_back_his(&b, c, true);  // (his again on the way back: the way back stops, our first place kept)
    CHECK(!b.on && b.have && NEAR(b.own[7], 8.3f));
    CHECK(nm_back_start(&b, 2000) && b.on && NEAR(b.own[7], 8.3f));
    memset(&b, 0, sizeof b);
    nm_back_his(&b, a, false);  // (our place unreadable: nothing kept, no way back: the glue stops it where it is)
    CHECK(!b.have && !nm_back_start(&b, 10) && !b.on);
    float out[12];
    CHECK(nm_back_step(&b, a, false, false, false, 20, out) == NMB_NONE);  // (not on its way back: nothing)
}

// the lab's E3 (s290 test 6): pc's elevator at the bottom (4.0 8.3 24.0), PC2's leave shown through its body up to 18 m, his
// records end there; pc's Link 12 m away on the shrine floor (near, not at it). Ours: back to the bottom in 0.2 m steps, never past
// it, then NM_SETTLE_N frames of objs_body_to to the bottom (velocity 0), then nothing written.
static void test_e3(void) {
    NmBack b;
    memset(&b, 0, sizeof b);
    float own[12], cur[12], out[12];
    at(own, 4, 8.3f, 24);
    nm_back_his(&b, own, true);
    at(cur, 4, 18.28f, 24);
    CHECK(nm_back_start(&b, 5000));
    float L[3] = {4, 8.3f, 12};
    int frames = 0, both = 0, body = 0, settle = 0, w;
    uint64_t now = 5000;
    float last_d = dist(cur, own);
    bool past = false, grew = false;
    while ((w = nm_back_step(&b, cur, nm_link_at(L, cur, false), nm_link_at(L, own, false), nm_link_near(L, cur, own), now, out)) != NMB_DONE && frames < 1000) {
        frames++, now += 16;
        if (w == NMB_BOTH) both++;
        if (w == NMB_BODY) body++;
        float d = dist(out, own);
        if (out[7] < own[7] - 1e-4f) past = true;
        if (d > last_d + 1e-4f) grew = true;
        if (d < 1e-4f) settle++;
        CHECK(dist(out, cur) <= NM_NEAR_STEP + 1e-4f);  // (never a jump: Link is 12 m off, within 30 m)
        memcpy(cur, out, sizeof cur), last_d = d;
    }
    CHECK(w == NMB_DONE && !b.on && !b.have);
    CHECK(!past && !grew && body == 0 && both == frames);  // (down only, body + matrix as his near moves)
    CHECK(frames == 49 + NM_SETTLE_N);  // (9.98 m: 49 steps of 0.2 m, the 50th reaches it = the 1st of NM_SETTLE_N there)
    CHECK(settle >= NM_SETTLE_N && NEAR(cur[7], 8.3f));
    CHECK(nm_back_step(&b, cur, false, false, true, now + 16, out) == NMB_NONE);  // (then nothing more written: ours)
    // the same far from our Link (> 30 m): there at once, then the settle
    memset(&b, 0, sizeof b);
    nm_back_his(&b, own, true);
    at(cur, 4, 53.28f, 24);
    nm_back_start(&b, 0);
    CHECK(nm_back_step(&b, cur, false, false, false, 16, out) == NMB_BOTH && NEAR(out[7], 8.3f));
    memcpy(cur, out, sizeof cur);
    int n = 1;
    while ((w = nm_back_step(&b, cur, false, false, false, 16 + 16 * n, out)) == NMB_BOTH) {
        n++;
        CHECK(NEAR(out[7], 8.3f));
    }
    CHECK(w == NMB_DONE && n == NM_SETTLE_N);
}

static void test_ride_wait_cap(void) {
    NmBack b;
    float own[12], cur[12], out[12];
    // our Link ON it (at it where it is): ride steps through its body only (5 cm a frame), it carries him back
    memset(&b, 0, sizeof b);
    at(own, 0, 0, 0), nm_back_his(&b, own, true), at(cur, 0, 3, 0), nm_back_start(&b, 0);
    CHECK(nm_back_step(&b, cur, true, false, true, 16, out) == NMB_BODY && NEAR(out[7], 3 - NM_RIDE_STEP));
    // our Link stands where it goes back to (under it): it waits, stopped where it is once (out = cur), then nothing
    memset(&b, 0, sizeof b);
    at(own, 4, 8.3f, 24), nm_back_his(&b, own, true), at(cur, 4, 20, 24), nm_back_start(&b, 0);
    CHECK(nm_back_step(&b, cur, false, true, true, 16, out) == NMB_HOLD && NEAR(out[7], 20) && b.waiting);
    CHECK(nm_back_step(&b, cur, false, true, true, 32, out) == NMB_NONE);
    CHECK(nm_back_step(&b, cur, false, true, true, 60000, out) == NMB_NONE && b.on);  // (waits as long as he stands there: no give-up)
    CHECK(nm_back_step(&b, cur, false, false, true, 60016, out) == NMB_BOTH && !b.waiting && NEAR(out[7], 20 - NM_NEAR_STEP));  // (he left)
    // its own logic keeps it away (each frame back where it was): given up after NM_BACK_MS, stopped where it is
    memset(&b, 0, sizeof b);
    at(own, 0, 0, 0), nm_back_his(&b, own, true), at(cur, 0, 10, 0), nm_back_start(&b, 100);
    int w = 0, k;
    uint64_t now = 100;
    for (; now <= 100 + NM_BACK_MS; now += 16) CHECK((w = nm_back_step(&b, cur, false, false, true, now, out)) == NMB_BOTH);
    CHECK(nm_back_step(&b, cur, false, false, true, now, out) == NMB_STOP && NEAR(out[7], 10) && !b.on && !b.have);
    // (s291 review 2) a menu / load of 20 s in the middle (no way-back frames): not counted, the give-up still comes after
    // NM_BACK_MS of way-back frames
    memset(&b, 0, sizeof b);
    at(own, 0, 0, 0), nm_back_his(&b, own, true), at(cur, 0, 10, 0), nm_back_start(&b, 100);
    for (now = 100; now <= 100 + NM_BACK_MS / 2; now += 16) CHECK(nm_back_step(&b, cur, false, false, true, now, out) == NMB_BOTH);
    now += 20000;
    CHECK(nm_back_step(&b, cur, false, false, true, now, out) == NMB_BOTH && b.on);  // (the first frame after the menu: on)
    for (now += 16; now <= 20100 + NM_BACK_MS; now += 16) CHECK((w = nm_back_step(&b, cur, false, false, true, now, out)) == NMB_BOTH);
    CHECK(nm_back_step(&b, cur, false, false, true, now + 16, out) == NMB_STOP && !b.on);
    // its own logic sends it away (a lift going up by itself, 3 cm a frame past our steps): let go after NM_AWAY_N frames, nothing written
    memset(&b, 0, sizeof b);
    at(own, 0, 0, 0), nm_back_his(&b, own, true), at(cur, 0, 5, 0), nm_back_start(&b, 0);
    for (k = 0; k < 100; k++) {
        w = nm_back_step(&b, cur, false, false, true, 16 * (k + 1), out);
        if (w != NMB_BOTH) break;
        cur[7] = out[7] + NM_NEAR_STEP + 0.03f;  // (our step down undone, and 3 cm more up: its own)
    }
    CHECK(w == NMB_OWN && k == NM_AWAY_N && !b.on && !b.have);
    CHECK(nm_back_step(&b, cur, false, false, true, 99999, out) == NMB_NONE);
    // a lag of a few frames (the physics step after our write; a velocity still on it) is not its own logic
    memset(&b, 0, sizeof b);
    at(own, 0, 0, 0), nm_back_his(&b, own, true), at(cur, 0, 5, 0), nm_back_start(&b, 0);
    for (k = 0; k < 200; k++) {
        w = nm_back_step(&b, cur, false, false, true, 16 * (k + 1), out);
        if (w != NMB_BOTH) break;
        if (k < NM_AWAY_N - 2) cur[7] += 0.05f;  // (still rising a little, the first frames)
        else memcpy(cur, out, sizeof cur);
    }
    CHECK(w == NMB_DONE && NEAR(cur[7], 0));
    CHECK(nm_back_off(own, own) < 1e-3f && NEAR(nm_back_off(cur, own), 0));
    // a turn back too (a gear his moves turned): at most NM_NEAR_RAD a frame, then there
    memset(&b, 0, sizeof b);
    at(own, 0, 0, 0), nm_back_his(&b, own, true);
    at(cur, 0, 0, 0);
    float a = 0.3f;  // (0.3 rad about y)
    cur[0] = cosf(a), cur[2] = sinf(a), cur[8] = -sinf(a), cur[10] = cosf(a);
    nm_back_start(&b, 0);
    k = 0;
    while ((w = nm_back_step(&b, cur, false, false, true, 16 * (k + 1), out)) == NMB_BOTH && k < 100) memcpy(cur, out, sizeof cur), k++;
    CHECK(w == NMB_DONE && NEAR(cur[0], 1) && NEAR(cur[2], 0) && k >= 6 + NM_SETTLE_N - 1 && k <= 7 + NM_SETTLE_N);
}

int main(void) {
    test_names();
    test_keep();
    test_e3();
    test_ride_wait_cap();
    printf("net_movers_test: %d checks, %d failed: %s\n", runs, fails, fails ? "FAIL" : "PASS");
    return fails != 0;
}

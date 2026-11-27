// host test for runtime/sora_labhorse_core.h (s291 labhorse: the LAB-ONLY `labhorse` live line): the parse, the names it
// takes (= nethorse's nh_horse), the spot at Link's side, the nearest horse, a request's wait.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/labhorse_test.c -o build_h/labhorse_test.exe
#include <stdio.h>
#include <stdlib.h>
#include "sora_labhorse_core.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)
#define NEAR(a, b, e) (fabsf((a) - (b)) <= (e))

static void parse_tests(void) {
    char n[LH_NAME_MAX];
    float a;
    CHECK(lh_parse("give Weapon_Sword_020", n, &a) == LH_NONE);
    CHECK(lh_parse("labhorses", n, &a) == LH_NONE);
    CHECK(lh_parse("labhorsex 1", n, &a) == LH_NONE);
    CHECK(lh_parse(NULL, n, &a) == LH_NONE);
    CHECK(lh_parse("labhorse", n, &a) == LH_SPAWN && !strcmp(n, LH_DEFAULT));
    CHECK(lh_parse("labhorse   ", n, &a) == LH_SPAWN && !strcmp(n, LH_DEFAULT));
    CHECK(lh_parse("labhorse GameRomHorse17", n, &a) == LH_SPAWN && !strcmp(n, "GameRomHorse17"));
    CHECK(lh_parse("labhorse GameRomHorseEpona", n, &a) == LH_SPAWN && !strcmp(n, "GameRomHorseEpona"));
    CHECK(lh_parse("labhorse GameROMMotorcycle", n, &a) == LH_SPAWN);
    CHECK(lh_parse("labhorse GameRomHorseSaddle_00", n, &a) == LH_BAD);  // (tack: nethorse sends it with the horse, not as one)
    CHECK(lh_parse("labhorse GameRomHorseReins_00", n, &a) == LH_BAD);
    CHECK(lh_parse("labhorse Animal_Horse_A", n, &a) == LH_BAD);         // (the stall decor: nethorse does not count it)
    CHECK(lh_parse("labhorse Enemy_Lynel_Gold", n, &a) == LH_BAD);
    CHECK(lh_parse("labhorse GameRomHorse01 x", n, &a) == LH_BAD);
    CHECK(lh_parse("labhorse GameRomHorse0;1", n, &a) == LH_BAD);
    CHECK(lh_parse("labhorse here", n, &a) == LH_HERE);
    CHECK(lh_parse("labhorse here 3", n, &a) == LH_BAD);
    CHECK(lh_parse("labhorse off", n, &a) == LH_OFF);
    CHECK(lh_parse("labhorse diag", n, &a) == LH_DIAG);
    CHECK(lh_parse("labhorse near", n, &a) == LH_NEAR && a == LH_NEAR_M);
    CHECK(lh_parse("labhorse near 400", n, &a) == LH_NEAR && a == 400);
    CHECK(lh_parse("labhorse near 0", n, &a) == LH_BAD);
    CHECK(lh_parse("labhorse near -5", n, &a) == LH_BAD);
    CHECK(lh_parse("labhorse near 5000", n, &a) == LH_BAD);
    CHECK(lh_parse("labhorse near 40m", n, &a) == LH_BAD);
    CHECK(lh_parse("labhorse near nan", n, &a) == LH_BAD);
    CHECK(lh_parse("labhorse stam 1", n, &a) == LH_STAM && a == 1);
    CHECK(lh_parse("labhorse stam 0", n, &a) == LH_STAM && a == 0);
    CHECK(lh_parse("labhorse stam", n, &a) == LH_BAD);
    CHECK(lh_parse("labhorse stam 2", n, &a) == LH_BAD);
    char longn[80];
    snprintf(longn, sizeof longn, "labhorse GameRomHorse%040d", 1);
    CHECK(lh_parse(longn, n, &a) == LH_BAD);
}

static void name_tests(void) {
    // every horse pack of the 1.9.0 romfs (Actor/Pack/GameRomHorse*.sbactorpack, s291 listing) that is a horse
    const char *horses[] = {"GameRomHorse", "GameRomHorse00L", "GameRomHorse00S", "GameRomHorse01", "GameRomHorse23", "GameRomHorseBone",
                            "GameRomHorseBone_AllDay", "GameRomHorseEpona", "GameRomHorseNushi", "GameRomHorseZelda", "GameROMMotorcycle"};
    for (size_t i = 0; i < sizeof horses / sizeof *horses; i++) CHECK(lh_name_ok(horses[i]) && nh_horse(horses[i]));
    const char *not[] = {"GameRomHorseSaddle_00", "GameRomHorseReins_10", "Animal_Horse_A", "", "GameRomHorse 01", "HorseAssociationCustum001"};
    for (size_t i = 0; i < sizeof not / sizeof *not; i++) CHECK(!lh_name_ok(not[i]));
    CHECK(!lh_name_ok(NULL));
    CHECK(lh_name_ok(LH_DEFAULT));
}

static void side_tests(void) {
    float lm[12] = {1, 0, 0, 10, 0, 1, 0, 100, 0, 0, 1, -50}, m[12];
    lh_side_mtx(lm, m);  // facing +Z: side = +X
    CHECK(NEAR(m[3], 10 + LH_SIDE_M, 1e-5f) && NEAR(m[7], 100 + LH_UP_M, 1e-5f) && NEAR(m[11], -50 + LH_FWD_M, 1e-5f));
    CHECK(m[0] == 1 && m[5] == 1 && m[10] == 1 && m[1] == 0 && m[2] == 0 && m[8] == 0);
    // facing +X (yaw 90 deg: column 2 = (1, 0, 0)): forward +X, side -Z
    float l2[12] = {0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0};
    lh_side_mtx(l2, m);
    CHECK(NEAR(m[3], LH_FWD_M, 1e-5f) && NEAR(m[11], -LH_SIDE_M, 1e-5f));
    CHECK(NEAR(m[2], 1, 1e-6f) && NEAR(m[10], 0, 1e-6f) && NEAR(m[0], 0, 1e-6f) && NEAR(m[8], -1, 1e-6f));
    // any yaw: an upright rotation (orthonormal columns, det 1), the spot LH_SIDE_M / LH_FWD_M flat from Link
    for (int k = 0; k < 36; k++) {
        float t = k * 0.1745f, c = cosf(t), s = sinf(t), tilt = 0.2f;
        float l[12] = {c, 0, s, 5, 0, 1, 0, 7, -s, 0, c, 9};
        l[6] = tilt;  // (a tilted Link: the horse stays upright)
        lh_side_mtx(l, m);
        float c0[3] = {m[0], m[4], m[8]}, c1[3] = {m[1], m[5], m[9]}, c2[3] = {m[2], m[6], m[10]};
        float det = c0[0] * (c1[1] * c2[2] - c1[2] * c2[1]) - c0[1] * (c1[0] * c2[2] - c1[2] * c2[0]) + c0[2] * (c1[0] * c2[1] - c1[1] * c2[0]);
        CHECK(NEAR(det, 1, 1e-4f) && m[5] == 1 && m[6] == 0 && m[4] == 0);
        float dx = m[3] - 5, dz = m[11] - 9;
        CHECK(NEAR(sqrtf(dx * dx + dz * dz), sqrtf(LH_SIDE_M * LH_SIDE_M + LH_FWD_M * LH_FWD_M), 1e-4f) && NEAR(m[7], 7 + LH_UP_M, 1e-5f));
        CHECK(NEAR(dx * c2[0] + dz * c2[2], LH_FWD_M, 1e-4f));  // (ahead by LH_FWD_M along his flattened forward)
    }
    float bad[12] = {0};  // (a degenerate forward: faces +Z)
    bad[3] = 1;
    lh_side_mtx(bad, m);
    CHECK(NEAR(m[3], 1 + LH_SIDE_M, 1e-5f) && m[10] == 1);
    float up[12] = {1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0};  // (looking straight up: column 2 flat part 0)
    lh_side_mtx(up, m);
    CHECK(isfinite(m[0]) && isfinite(m[3]) && m[10] == 1);
}

static void nearest_tests(void) {
    float me[3] = {0, 0, 0}, p[4][3] = {{100, 0, 0}, {5, 0, 0}, {3, 0, 0}, {1, 0, 0}};
    const char *n[4] = {"GameRomHorse01", "GameRomHorse02", "Animal_Horse_A", "GameRomHorseSaddle_00"};
    float d = -1;
    CHECK(lh_nearest(me, (const float (*)[3])p, n, NULL, 4, 150, &d) == 1 && NEAR(d, 5, 1e-5f));  // (decor + tack skipped)
    bool skip[4] = {false, true, false, false};
    CHECK(lh_nearest(me, (const float (*)[3])p, n, skip, 4, 150, &d) == 0 && NEAR(d, 100, 1e-4f));
    CHECK(lh_nearest(me, (const float (*)[3])p, n, skip, 4, 50, &d) == -1 && d == 0);
    CHECK(lh_nearest(me, (const float (*)[3])p, n, NULL, 0, 150, &d) == -1);
    const char *n2[2] = {NULL, "GameRomHorse03"};
    float p2[2][3] = {{0, 0, 0}, {0, 20, 0}};  // (height counts: a horse on a cliff 20 m up)
    CHECK(lh_nearest(me, (const float (*)[3])p2, n2, NULL, 2, 150, &d) == 1 && NEAR(d, 20, 1e-5f));
}

static void wait_tests(void) {
    CHECK(lh_wait(-1, 0, false) == LHW_WAIT);
    CHECK(lh_wait(0, 1000, false) == LHW_WAIT);
    CHECK(lh_wait(2, 0, false) == LHW_TAKE);
    CHECK(lh_wait(3, 0, false) == LHW_FAILED && lh_wait(5, 99999, true) == LHW_FAILED);
    CHECK(lh_wait(1, LH_SLOW_MS, false) == LHW_SLOW);
    CHECK(lh_wait(1, LH_SLOW_MS + 5000, true) == LHW_WAIT);  // (said once)
    CHECK(lh_wait(-1, LH_SLOW_MS - 1, false) == LHW_WAIT);
}

int main(void) {
    parse_tests();
    name_tests();
    side_tests();
    nearest_tests();
    wait_tests();
    printf("labhorse_test: %d/%d %s\n", runs - fails, runs, fails ? "FAILED" : "PASS");
    return fails != 0;
}

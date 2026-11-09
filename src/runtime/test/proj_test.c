// host test for runtime/sora_proj_core.h (s183 amd FIX 2: which projection Sora's body pass uses).
//   clang -O1 -Wall -Iruntime runtime/test/proj_test.c -o proj_test.exe && ./proj_test.exe
#include <stdio.h>
#include <string.h>
#include "sora_proj_core.h"

static int runs, fails;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main(void) {
    const float D = 3.14159265f / 180;
    float out[4];
    // 16:9 PC: the scan's gameplay projection and the exe's world one (the same camera)
    float scan169[4] = {0.31f, 25000, 50 * D, 16.0f / 9}, exe169[4] = {tanf(25 * D), 16.0f / 9, 0.31f, 25000};
    // auto: the scan's values byte for byte (the s182 behaviour)
    memset(out, 0, sizeof out);
    CHECK(proj_pick(PROJ_AUTO, true, scan169, true, exe169, out) == PROJ_SRC_SCAN && !memcmp(out, scan169, sizeof out));
    CHECK(proj_agree(scan169, exe169));
    // exe first: the same camera from the exe (fovy via atan: within float rounding)
    CHECK(proj_pick(PROJ_EXE, true, scan169, true, exe169, out) == PROJ_SRC_EXE);
    CHECK(out[0] == 0.31f && out[1] == 25000 && fabsf(out[2] - 50 * D) < 1e-5f && out[3] == 16.0f / 9);
    // 4:3 / 5:4 (1280x1024) / 32:9 windows: the scan finds no gameplay projection (aspect rule 1.45..2.5) -> the exe's
    float exe54[4] = {tanf(25 * D) / (1.25f / (16.0f / 9)), 1.25f, 0.31f, 25000};  // (taller: tan / shape, aspect = window)
    float exe329[4] = {tanf(25 * D), 32.0f / 9, 0.5f, 25000};
    CHECK(proj_pick(PROJ_AUTO, false, NULL, true, exe54, out) == PROJ_SRC_EXE && out[3] == 1.25f && out[1] == 25000 && out[0] == 0.31f);
    CHECK(out[2] > 50 * D);  // (taller window: more above and below)
    CHECK(proj_pick(PROJ_AUTO, false, NULL, true, exe329, out) == PROJ_SRC_EXE && out[3] == 32.0f / 9);
    // scan only: none -> NONE (out untouched = the pass keeps its guess, occlusion off)
    out[0] = 123;
    CHECK(proj_pick(PROJ_SCAN, false, NULL, true, exe54, out) == PROJ_SRC_NONE && out[0] == 123);
    // nothing built yet (zeros) / garbage: NONE, or the scan when there is one
    float zero[4] = {0, 0, 0, 0}, nanp[4] = {NAN, 1.7f, 0.1f, 25000}, inv[4] = {0.4f, 1.7f, 100, 10};
    CHECK(proj_pick(PROJ_AUTO, false, NULL, true, zero, out) == PROJ_SRC_NONE);
    CHECK(proj_pick(PROJ_AUTO, false, NULL, false, exe169, out) == PROJ_SRC_NONE);
    CHECK(proj_pick(PROJ_AUTO, false, NULL, true, nanp, out) == PROJ_SRC_NONE);
    CHECK(proj_pick(PROJ_AUTO, false, NULL, true, inv, out) == PROJ_SRC_NONE);  // (near >= far)
    CHECK(proj_pick(PROJ_EXE, true, scan169, true, zero, out) == PROJ_SRC_SCAN && !memcmp(out, scan169, sizeof out));
    // a projection of another shape (a 16:9 one picked at a 4:3 window): auto takes the exe's (scan only keeps the scan's)
    float scan_ui[4] = {1, 10000, 50 * D, 16.0f / 9}, exe43[4] = {tanf(25 * D) * (16.0f / 9) / (4.0f / 3), 4.0f / 3, 0.31f, 25000};
    CHECK(!proj_agree(scan_ui, exe43));
    CHECK(proj_pick(PROJ_AUTO, true, scan_ui, true, exe43, out) == PROJ_SRC_EXE && out[3] == 4.0f / 3 && out[1] == 25000);
    CHECK(proj_pick(PROJ_SCAN, true, scan_ui, true, exe43, out) == PROJ_SRC_SCAN && out[3] == 16.0f / 9);
    CHECK(proj_pick(PROJ_EXE, true, scan_ui, true, exe43, out) == PROJ_SRC_EXE && out[3] == 4.0f / 3);
    // the same shape but other near / far (a cutscene's own camera object): auto keeps the scan's (the s182 behaviour)
    float scan_cs[4] = {0.5f, 10000, 40 * D, 16.0f / 9};
    CHECK(!proj_agree(scan_cs, exe169));
    CHECK(proj_pick(PROJ_AUTO, true, scan_cs, true, exe169, out) == PROJ_SRC_SCAN && !memcmp(out, scan_cs, sizeof out));
    // aspect 0.5% apart (window shape rounding): still the scan's
    float exe_r[4] = {tanf(25 * D), 16.0f / 9 * 1.005f, 0.31f, 25000};
    CHECK(proj_pick(PROJ_AUTO, true, scan169, true, exe_r, out) == PROJ_SRC_SCAN && !memcmp(out, scan169, sizeof out));
    // the field of view easing (2% apart) still agrees; 5% does not
    float exe_e[4] = {tanf(25 * D) * 1.02f, 16.0f / 9, 0.31f, 25000}, exe_f[4] = {tanf(25 * D) * 1.05f, 16.0f / 9, 0.31f, 25000};
    CHECK(proj_agree(scan169, exe_e) && !proj_agree(scan169, exe_f));
    printf("proj_test: %d checks, %d failed\n", runs, fails);
    return fails != 0;
}

// host test for runtime/sora_prompt.h (s194 promptx: BotW's A prompt drawn as Switch X's binding while Sora drives):
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/prompt_test.c -o build_h/prompt_test.exe && build_h/prompt_test.exe && build_h/prompt_test.exe oldexe
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---- stubs for what sora.c gives the header
#define LOG(...) printf(__VA_ARGS__)
static bool g_on;
static int g_tri_pickup = 1;
static uint32_t fake_pause;
#define PAUSE_STATE ((uintptr_t)&fake_pause)
static int nset, last_set = -99, have_export = 1, nlookup;
static void fake_set(int slot) { nset++, last_set = slot; }
static void *fake_proc(const char *name) { nlookup++; return have_export && !strcmp(name, "glyphs_set_a_slot") ? (void *)fake_set : NULL; }
#define GetModuleHandleA(x) ((void *)1)
#define GetProcAddress(m, n) ((void)(m), fake_proc(n))

#include "sora_prompt.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

// one game frame at present p: driven or not, then n pad polls at p
static void frame(uint64_t p, int driven) { px_frame_begin(); if (driven) g_px_drive_now = 1; px_frame_end(p); px_pad(p); }

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "oldexe")) {  // an exe before s194 (no export): one lookup, nothing called, no crash
        have_export = 0, g_on = true;
        for (uint64_t p = 1; p < 50; p++) frame(p, p > 10);
        fake_pause = 2, px_pad(50), fake_pause = 0;
        px_unload();
        CHECK(nset == 0 && nlookup == 1);
        printf("%s (old exe): %d checks, %d failed\n", fails ? "FAIL" : "PASS", runs, fails);
        return fails != 0;
    }
    // Sora mode never on: no lookup, no call
    for (uint64_t p = 1; p < 10; p++) frame(p, 0);
    CHECK(nlookup == 0 && nset == 0);

    g_on = true;
    frame(10, 0);  // on, not driving: A sent once (a fresh copy always sends)
    CHECK(nset == 1 && last_set == PX_SLOT_A);
    frame(11, 0);
    CHECK(nset == 1);  // (only on a change)
    frame(12, 1);  // driving: X
    CHECK(nset == 2 && last_set == PX_SLOT_X);
    for (uint64_t p = 13; p < 100; p++) frame(p, 1);
    CHECK(nset == 2);  // steady: no calls

    // a frame whose body returns early (driven mark not set) ends the X at once
    frame(100, 0);
    CHECK(last_set == PX_SLOT_A && nset == 3);
    frame(101, 1);
    CHECK(last_set == PX_SLOT_X && nset == 4);

    // paused: A at once on the next poll, even with no frame call in between
    fake_pause = 2, px_pad(101);
    CHECK(last_set == PX_SLOT_A && nset == 5);
    px_pad(102), px_pad(103);
    CHECK(nset == 5);
    fake_pause = 0, px_pad(104);
    CHECK(last_set == PX_SLOT_X && nset == 6);

    // the frame thread a present ahead of the poll: still X (no unsigned wrap)
    frame(200, 1);
    px_pad(199);
    CHECK(last_set == PX_SLOT_X && nset == 6);

    // no frame call for > PX_STALE presents (a load, Game Over): A
    px_pad(200 + PX_STALE);
    CHECK(last_set == PX_SLOT_X);
    px_pad(200 + PX_STALE + 1);
    CHECK(last_set == PX_SLOT_A && nset == 7);
    frame(300, 1);
    CHECK(last_set == PX_SLOT_X && nset == 8);

    // set tri 0 / set promptx 0 / Sora mode off: A
    g_tri_pickup = 0, frame(301, 1);
    CHECK(last_set == PX_SLOT_A);
    g_tri_pickup = 1, frame(302, 1);
    CHECK(last_set == PX_SLOT_X);
    g_promptx = 0, frame(303, 1);
    CHECK(last_set == PX_SLOT_A);
    g_promptx = 1, frame(304, 1);
    CHECK(last_set == PX_SLOT_X);
    g_on = false, frame(305, 1);
    CHECK(last_set == PX_SLOT_A);
    g_on = true, frame(306, 1);
    CHECK(last_set == PX_SLOT_X);

    // unload: A back, and once
    int n0 = nset;
    px_unload();
    CHECK(last_set == PX_SLOT_A && nset == n0 + 1);
    px_unload();
    CHECK(nset == n0 + 1 && nlookup == 1);
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "PASS", runs, fails);
    return fails != 0;
}

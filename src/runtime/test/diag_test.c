// host test for runtime/sora_diag_core.h (the on-screen "Sora's model can't be drawn" decision + the line wrap).
//   clang -O1 -Wall -Iruntime runtime/test/diag_test.c -o diag_test.exe && ./diag_test.exe
#include <stdio.h>
#include <string.h>
#include "sora_diag_core.h"

static int runs, fails;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// a 60 fps game: ticks of 16 ms from t0 for ms milliseconds; the exe draws (drawn_fr advances) when draws
static int run(DgCore *d, uint64_t *t, uint64_t ms, bool posed, bool playing, bool nolink, bool draws, uint64_t *fr) {
    int k = 0;
    for (uint64_t e = *t + ms; *t < e; *t += 16) {
        if (draws) (*fr)++;
        k = dgc_tick(d, *t, posed, playing, nolink, *fr);
    }
    return k;
}

static float meas(void *ud, const char *s, int len) { (void)ud; (void)s; return (float)len * 10; }

int main(void) {
    DgCore d;
    uint64_t t, fr;

    // healthy: posed and drawn forever = never a notice
    memset(&d, 0, sizeof d), t = 1000, fr = 0;
    CHECK(run(&d, &t, 60000, true, true, false, true, &fr) == DG_OK);

    // posed but the exe never draws: the notice after 5 s, not before
    memset(&d, 0, sizeof d), t = 1000, fr = 0;
    CHECK(run(&d, &t, 4800, true, true, false, false, &fr) == DG_OK);
    CHECK(run(&d, &t, 600, true, true, false, false, &fr) == DG_NODRAW);
    // ... and it clears the moment a frame is drawn
    CHECK(run(&d, &t, 32, true, true, false, true, &fr) == DG_OK);

    // not posed (a cutscene, a menu, the body intentionally hidden) = no notice, however long, as long as not "playing"
    memset(&d, 0, sizeof d), t = 1000, fr = 0;
    CHECK(run(&d, &t, 120000, false, false, false, false, &fr) == DG_OK);

    // playing in the world for 30 s with no pose = E1
    memset(&d, 0, sizeof d), t = 1000, fr = 0;
    CHECK(run(&d, &t, 29000, false, true, false, false, &fr) == DG_OK);
    CHECK(run(&d, &t, 2000, false, true, false, false, &fr) == DG_NOPOSE);
    CHECK(run(&d, &t, 100, true, true, false, true, &fr) == DG_OK);  // a pose + a draw ends it

    // a world but no Link for 20 s
    memset(&d, 0, sizeof d), t = 1000, fr = 0;
    CHECK(run(&d, &t, 19000, false, false, true, false, &fr) == DG_OK);
    CHECK(run(&d, &t, 2000, false, false, true, false, &fr) == DG_NOLINK);
    CHECK(run(&d, &t, 100, false, false, false, false, &fr) == DG_OK);  // Link found again

    // a stall (loading screen: no ticks for 10 s) restarts the timers: no notice right after it
    memset(&d, 0, sizeof d), t = 1000, fr = 0;
    CHECK(run(&d, &t, 3000, true, true, false, false, &fr) == DG_OK);
    t += 10000;  // the game loop did not run
    CHECK(run(&d, &t, 3000, true, true, false, false, &fr) == DG_OK);
    CHECK(run(&d, &t, 2500, true, true, false, false, &fr) == DG_NODRAW);

    // a pose gap (the body hidden for 3 s, then posed again) starts a new 5 s run
    memset(&d, 0, sizeof d), t = 1000, fr = 0;
    CHECK(run(&d, &t, 4000, true, true, false, false, &fr) == DG_OK);
    CHECK(run(&d, &t, 3000, false, false, false, false, &fr) == DG_OK);
    CHECK(run(&d, &t, 4000, true, true, false, false, &fr) == DG_OK);
    CHECK(run(&d, &t, 1500, true, true, false, false, &fr) == DG_NODRAW);

    // the clock going backwards does not fire anything
    memset(&d, 0, sizeof d), t = 100000, fr = 0;
    CHECK(run(&d, &t, 1000, true, true, false, false, &fr) == DG_OK);
    t = 500;
    CHECK(run(&d, &t, 1000, true, true, false, false, &fr) == DG_OK);

    // wrap: 10 px a char, 100 px wide = 10 chars a line, break at spaces
    {
        int st[8], ln[8];
        const char *m = "aaaa bbbb cccc dddddddddddd e";
        int n = dgc_wrap(m, 100.0f, meas, NULL, st, ln, 8);
        CHECK(n == 4);
        CHECK(n == 4 && st[0] == 0 && ln[0] == 9);                // "aaaa bbbb"
        CHECK(n == 4 && st[1] == 10 && ln[1] == 4);               // "cccc"
        CHECK(n == 4 && ln[2] == 12 && !strncmp(m + st[2], "dddddddddddd", 12));  // a too-long word: its own line
        CHECK(n == 4 && !strncmp(m + st[3], "e", 1) && ln[3] == 1);
        CHECK(dgc_wrap("", 100.0f, meas, NULL, st, ln, 8) == 0);
        CHECK(dgc_wrap("   ", 100.0f, meas, NULL, st, ln, 8) == 0);
        CHECK(dgc_wrap("a b c d e f g h i j", 10.0f, meas, NULL, st, ln, 3) == 3);  // capped
    }

    printf("diag_test: %d checks, %d failed\n", runs, fails);
    return fails != 0;
}

// host test for runtime/linkpos_t0.h (s291 ridestart): when linkpos.c sets g_t0 ("Link is in the world").
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/t0ride_test.c -o build_h/t0ride_test.exe
#include <stdio.h>
#include "linkpos_t0.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)

// the old rule, copied from linkpos.c state_of (before s291): g_t0 when state_of(actor) == LINK_GROUND
enum { LINK_NONE, LINK_GROUND, LINK_AIR, LINK_OTHER };
static int old_state_of(int have_actor, uint32_t s) {
    if (!have_actor) return LINK_NONE;
    return s & 0x08000000u ? LINK_GROUND : s & 0x80000000u || !s ? LINK_AIR : LINK_OTHER;
}

// a boot as the logs show it: title (0), the save load (0x1), then the world. Returns the frame g_t0 is set at (-1 never).
static int boot(const uint32_t *words, int n, int ride) {
    for (int i = 0; i < n; i++)
        if (t0_start_kind(1, words[i], ride)) return i;
    return -1;
}

int main(void) {
    // ride off = exactly the old rule, for every word the logs show + all single bits + some mixes
    static const uint32_t seen[] = {0, 0x1, 0x2, 0x90, 0x8002, 0x18000001, 0x18000000, 0x18000002, 0x08000002, 0x80000000,
                                    0x80080000, 0x100000, 0x600002, 0x100010, 0x18000001 | 0x100000, 0x80000000 | 0x100000};
    for (unsigned i = 0; i < sizeof seen / sizeof *seen; i++)
        for (int a = 0; a < 2; a++) CHECK((t0_start_kind(a, seen[i], 0) != T0_NO) == (old_state_of(a, seen[i]) == LINK_GROUND));
    for (int b = 0; b < 32; b++)
        for (int a = 0; a < 2; a++) {
            uint32_t w = 1u << b;
            CHECK((t0_start_kind(a, w, 0) != T0_NO) == (old_state_of(a, w) == LINK_GROUND));
            CHECK((t0_start_kind(a, w | 0x18000001u, 0) != T0_NO) == (old_state_of(a, w | 0x18000001u) == LINK_GROUND));
        }
    // ground stays ground whatever ride is (the log line and the old behaviour are the same)
    CHECK(t0_start_kind(1, 0x18000001, 0) == T0_GROUND);
    CHECK(t0_start_kind(1, 0x18000001, 1) == T0_GROUND);
    CHECK(t0_start_kind(1, 0x18000001 | 0x100000, 1) == T0_GROUND);
    // riding: only with ride on
    CHECK(t0_start_kind(1, 0x100000, 0) == T0_NO);
    CHECK(t0_start_kind(1, 0x100000, 1) == T0_RIDING);
    CHECK(t0_start_kind(1, 0x100000, 2) == T0_RIDING);
    CHECK(t0_start_kind(1, 0x100010, 1) == T0_RIDING);   // (the ride's way out)
    CHECK(t0_start_kind(1, 0x600002, 1) == T0_NO);       // (no riding bit: as before, waits for the ground)
    CHECK(t0_start_kind(0, 0x100000, 1) == T0_NO);       // (no actor: never)
    // never on the title (0) or the save load (0x1), ride on or off; climbing / gliding / air stay out
    for (int r = 0; r < 3; r++) {
        CHECK(t0_start_kind(1, 0, r) == T0_NO);
        CHECK(t0_start_kind(1, 0x1, r) == T0_NO);
        CHECK(t0_start_kind(1, 0x2, r) == T0_NO);
        CHECK(t0_start_kind(1, 0x90, r) == T0_NO);
        CHECK(t0_start_kind(1, 0x8002, r) == T0_NO);
        CHECK(t0_start_kind(1, 0x80000000, r) == T0_NO);
    }
    // a whole boot: a save made on foot = the same frame with ride on or off
    {
        uint32_t w[] = {0, 0, 0, 0x1, 0x1, 0x18000001, 0x1, 0x18000001};
        CHECK(boot(w, 8, 0) == 5 && boot(w, 8, 1) == 5);
    }
    // a save made on a horse (s291e: plain Link riding for 6+ min until B): ride off = only at the dismount, ride on = the
    // first riding frame (GUESS: the word right after the load is 0x100000, as at a mount; t0ride 2 logs it in game)
    {
        uint32_t w[] = {0, 0, 0x1, 0x1, 0x100000, 0x100000, 0x100000, 0x600002, 0x18000001};
        CHECK(boot(w, 9, 0) == 8);
        CHECK(boot(w, 9, 1) == 4);
    }
    // ride on but the load never leaves 0x1 / the title: never
    {
        uint32_t w[] = {0, 0, 0x1, 0x1, 0x1, 0};
        CHECK(boot(w, 6, 1) == -1);
    }
    printf("t0ride: %d of %d checks pass%s\n", runs - fails, runs, fails ? " - FAILED" : "");
    return fails != 0;
}

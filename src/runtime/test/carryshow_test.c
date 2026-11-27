// host test for runtime/linkpos_carryshow.h (s291 popin: with live carryheld 1 only the object Link holds, + the last one for
// CARRYSHOW_GRACE frames, is forced visible near hidden Link; other pots / barrels keep BotW's fades) - no linkpos, no guest memory.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/carryshow_test.c -o build_h/carryshow_test.exe
#include <stdio.h>
#include <string.h>
#include "linkpos_carryshow.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

#define BARREL 0x72f2276300ull
#define POT 0x72f203d290ull
#define POT2 0x72f2000100ull

static void test_nothing_held(void) {  // the bug: a pot near hidden Link fading, Link holds nothing -> BotW's fade
    CarryShowMem m;
    memset(&m, 0, sizeof m);
    for (uint64_t f = 1; f < 200; f++) CHECK(!carryshow_keep(&m, POT, 0, f));
    CHECK(!carryshow_keep(&m, 0, 0, 5));
}

static void test_held(void) {  // the carried barrel stays visible every frame; a pot beside it does not
    CarryShowMem m;
    memset(&m, 0, sizeof m);
    for (uint64_t f = 100; f < 400; f++) {
        CHECK(carryshow_keep(&m, BARREL, BARREL, f));
        CHECK(!carryshow_keep(&m, POT, BARREL, f));
    }
}

static void test_grace_after_throw(void) {  // Link +0x88 = 0 at the throw: the barrel stays forced for CARRYSHOW_GRACE frames, then BotW's
    CarryShowMem m;
    memset(&m, 0, sizeof m);
    CHECK(carryshow_keep(&m, BARREL, BARREL, 1000));
    CHECK(carryshow_keep(&m, BARREL, 0, 1001));
    CHECK(carryshow_keep(&m, BARREL, 0, 1000 + CARRYSHOW_GRACE));
    CHECK(!carryshow_keep(&m, BARREL, 0, 1001 + CARRYSHOW_GRACE));
    CHECK(!carryshow_keep(&m, POT, 0, 1001));  // (the grace is the thrown one's only)
}

static void test_held_seen_through_other_actor(void) {  // the memory follows Link +0x88 whichever actor's apply read it
    CarryShowMem m;
    memset(&m, 0, sizeof m);
    CHECK(!carryshow_keep(&m, POT, BARREL, 50));  // (the pot's apply saw the barrel held)
    CHECK(carryshow_keep(&m, BARREL, 0, 60));      // (the barrel, already thrown: still in its grace)
}

static void test_new_lift_replaces(void) {  // a second lift: the new object is held, the old one's grace ends
    CarryShowMem m;
    memset(&m, 0, sizeof m);
    CHECK(carryshow_keep(&m, BARREL, BARREL, 10));
    CHECK(carryshow_keep(&m, POT, POT, 20));
    CHECK(!carryshow_keep(&m, BARREL, POT, 21));
    CHECK(carryshow_keep(&m, POT, 0, 30));
    CHECK(!carryshow_keep(&m, POT2, 0, 30));
}

static void test_frame_order(void) {  // a stamp from a later frame (another thread) never wraps into "kept"
    CarryShowMem m;
    memset(&m, 0, sizeof m);
    CHECK(carryshow_keep(&m, BARREL, BARREL, 500));
    CHECK(!carryshow_keep(&m, BARREL, 0, 499));
    CHECK(carryshow_keep(&m, BARREL, 0, 500));
}

int main(void) {
    test_nothing_held();
    test_held();
    test_grace_after_throw();
    test_held_seen_through_other_actor();
    test_new_lift_replaces();
    test_frame_order();
    printf("carryshow: %d/%d checks pass%s\n", runs - fails, runs, fails ? " - FAIL" : "");
    return fails != 0;
}

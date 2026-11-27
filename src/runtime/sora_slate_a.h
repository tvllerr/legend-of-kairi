// (s291 bombrock) live "set slateeata 1" (default 0): the A press that picks a rune in the KH2 command menu is kept from BotW
// until it is let go. Why (s291 lab, copy2_s291c.log): Slate mode gives Link the pad on the next tick, the menu's A is still held
// ("[slate] t+2 Link state 0x18000001 -> 0x21 (held 0x1)"), so next to a liftable BotW's PlayerGrab takes it ("leaf GrabReady",
// "[hold] Link +0x88 = ... Obj_LiftRockWhite_A_01") one present BEFORE the L tap (t+3 .. t+4) that pulls out the bomb.
// Away from a liftable the same A does nothing in BotW, so the bomb came out there.
// Pure (no game state): pad_tap in sora.c calls it, runtime/test/slate_a_test.c tests it.
#pragma once
#include <stdint.h>

// b = the buttons BotW is about to get this poll, live = Slate mode is live now (SLATE_LIVE), *eat = 1 from sk_request until the
// A is up (or Slate mode is over). Returns the buttons BotW gets. A later, fresh A press reaches BotW as before.
static inline uint64_t slate_eat_a(uint64_t b, int live, volatile int *eat, uint64_t a_bit) {
    if (!*eat) return b;
    if (live && (b & a_bit)) return b & ~a_bit;
    *eat = 0;  // (A let go, or Slate mode ended: from here on A is BotW's again)
    return b;
}

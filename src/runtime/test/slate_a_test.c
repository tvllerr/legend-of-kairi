// host test for runtime/sora_slate_a.h (s291 bombrock: live "slateeata 1" keeps the A that picked a rune from BotW until it is up,
// so a liftable rock next to Link is not lifted before the Slate mode's L tap pulls out the bomb) - pure, no game.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/slate_a_test.c -o build_h/slate_a_test.exe
#include <stdio.h>
#include "sora_slate_a.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

#define A (1ull << 0)
#define B (1ull << 1)
#define L (1ull << 6)

static void test_key_off(void) {  // eat 0 (the key off, or no Slate mode): every press passes, unchanged
    volatile int eat = 0;
    uint64_t pads[] = {0, A, A | L, B, A | B | L, ~0ull};
    for (unsigned i = 0; i < sizeof pads / sizeof *pads; i++) {
        CHECK(slate_eat_a(pads[i], 1, &eat, A) == pads[i]);
        CHECK(slate_eat_a(pads[i], 0, &eat, A) == pads[i]);
    }
    CHECK(eat == 0);
}

static void test_lab_sequence(void) {  // the s291 lab: A held from the menu pick through t+2, the L tap at t+3..t+4, A up later
    volatile int eat = 1;
    for (int t = 0; t < 3; t++) CHECK(slate_eat_a(A, 1, &eat, A) == 0);  // (t+2: no A reaches PlayerGrab = no lift)
    CHECK(slate_eat_a(A | L, 1, &eat, A) == L);                           // the L tap still reaches BotW
    CHECK(slate_eat_a(A | L, 1, &eat, A) == L);
    CHECK(eat == 1);
    CHECK(slate_eat_a(0, 1, &eat, A) == 0);                               // A up: done
    CHECK(eat == 0);
    CHECK(slate_eat_a(A, 1, &eat, A) == A);                               // a fresh A later is BotW's again
}

static void test_short_tap(void) {  // the A was already up at the first poll: nothing eaten, the flag clears at once
    volatile int eat = 1;
    CHECK(slate_eat_a(B, 1, &eat, A) == B);
    CHECK(eat == 0);
    CHECK(slate_eat_a(A, 1, &eat, A) == A);
}

static void test_slate_over(void) {  // Slate mode no longer live while A is still held: A is BotW's again, the flag clears
    volatile int eat = 1;
    CHECK(slate_eat_a(A, 1, &eat, A) == 0);
    CHECK(slate_eat_a(A, 0, &eat, A) == A);
    CHECK(eat == 0);
    CHECK(slate_eat_a(A, 1, &eat, A) == A);  // (no re-arm without a new rune pick)
}

static void test_other_buttons_untouched(void) {  // only A is held back: B (Slate mode's end) and the rest pass
    volatile int eat = 1;
    CHECK(slate_eat_a(A | B, 1, &eat, A) == B);
    CHECK(slate_eat_a(~0ull, 1, &eat, A) == (~0ull & ~A));
    CHECK(eat == 1);
}

int main(void) {
    test_key_off();
    test_lab_sequence();
    test_short_tap();
    test_slate_over();
    test_other_buttons_untouched();
    printf("slate_a_test: %d/%d checks passed%s\n", runs - fails, runs, fails ? " - FAILED" : " - ok");
    return fails != 0;
}

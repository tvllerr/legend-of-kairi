// host test for runtime/sora_title.h (s199): the decision "Sora mode off on BotW's title after a quit", frame by frame.
//   clang -O1 -g -Wall -Wextra -DSORA_TITLE_TEST -Iruntime runtime/test/title_test.c -o build_h/title_test.exe
#include <stdio.h>
#ifndef SORA_TITLE_TEST
#define SORA_TITLE_TEST
#endif
#include "sora_title.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)

int main(void) {
    bool on = false;
    // the first boot: the title (screen 0x1) before Link ever stood in the world (t0 0) = the old path, no change, no line
    CHECK(title_step(&on, 1, 0, true) == TITLE_SAME && !on);
    CHECK(title_step(&on, 1, 0, false) == TITLE_SAME && !on);
    // in the world (t0 set): pause menu, inventory, loads, Game Over, shrines, cutscenes = no screen 0x1 = nothing changes
    for (int i = 0; i < 100; i++) CHECK(title_step(&on, 1, 1205, false) == TITLE_SAME && !on);
    // BotW's quit to its title: screen 0x1 comes with t0 still set -> off, once
    CHECK(title_step(&on, 1, 1205, true) == TITLE_ENTER && on);
    for (int i = 0; i < 100; i++) CHECK(title_step(&on, 1, 1205, true) == TITLE_SAME && on);
    // Continue / New Game: 0x1 goes when the load starts -> back, once
    CHECK(title_step(&on, 1, 1205, false) == TITLE_LEAVE && !on);
    CHECK(title_step(&on, 1, 1205, false) == TITLE_SAME && !on);
    // "set titleoff 0" on the title = the old behaviour at once; "set titleoff 1" there = off again
    CHECK(title_step(&on, 1, 1205, true) == TITLE_ENTER && on);
    CHECK(title_step(&on, 0, 1205, true) == TITLE_LEAVE && !on);
    CHECK(title_step(&on, 0, 1205, true) == TITLE_SAME && !on);
    CHECK(title_step(&on, 1, 1205, true) == TITLE_ENTER && on);
    // titleoff 0 in the world, then a quit: never off
    on = false;
    CHECK(title_step(&on, 0, 1205, false) == TITLE_SAME && !on);
    CHECK(title_step(&on, 0, 1205, true) == TITLE_SAME && !on);
    // a DLL hot reload on the title: the state starts again at false, the exe's t0 and the screen are still there -> off
    // on the new DLL's first frame (one ENTER line again)
    on = false;
    CHECK(title_step(&on, 1, 1205, true) == TITLE_ENTER && on);
    // a hot reload in the world: nothing
    on = false;
    CHECK(title_step(&on, 1, 1205, false) == TITLE_SAME && !on);
    // Savage Wind's F1 reboot (GUESS: the exe keeps g_t0 across it, as linkpos_reset does not clear it): the reboot's
    // title has screen 0x1 with t0 set -> off like a quit
    CHECK(title_step(&on, 1, 1205, true) == TITLE_ENTER && on);
    printf("title_test: %d/%d\n", runs - fails, runs);
    return fails != 0;
}

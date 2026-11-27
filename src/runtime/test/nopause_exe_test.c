// host test for runtime/nopause.c (s289 nopause, the exe's half of "the world runs in menus"; after Savage Wind PR #294 by
// byluki, MIT): with no permission from the DLL nothing is read or written and the paused step is the game's; a permission
// holds for the present it was given in and the next two only; while it holds, rt_nopause_vfr (every VFR update) makes the
// pause manager's 1 a 0 and counts each 0 it finds held from its last update, never touches a 2 / 4 / 5; once it ends (the
// DLL's 0 or the 2 presents) the manager's last value (its +0x28) is back at the next update if the word still holds our 0,
// with the UI pause state's rule when no manager is on the heap; netnopausestep halves the paused step only while it holds.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/nopause_exe_test.c -o build_h/nopause_exe_test.exe
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static uint64_t g_present = 1000;
uint64_t renderer_present_count(void) { return g_present; }

// the fake guest: the heap (8-aligned), the pause manager in it, the scene state, the UI pause state, the manager's pointer
static uint64_t g_heap[0x20];
#define NP_HEAP_LO ((uint64_t)(uintptr_t)g_heap)
#define NP_HEAP_SIZE ((uint64_t)sizeof g_heap)

#include "../nopause.c"

static SoraSites g_ss;
static int g_ss_calls;
const SoraSites *sora_sites(void) { g_ss_calls++; return &g_ss; }

static uint32_t g_word, g_ps;
static uint64_t g_pm_slot;
#define PM ((uint8_t *)g_heap + 0x20)
static uint64_t A(const void *p) { return (uint64_t)(uintptr_t)p; }
// the manager's calc (0x7000d46940): its +0x28 = v, then the store 0x7001281100
static void pm_store(uint32_t v) { memcpy(PM + 0x28, &v, 4), g_word = v; }
static void cnt(uint64_t c[3]) { sw_nopause_counts(c); }

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)

int main(void) {
    uint64_t c[3];
    // no DLL call yet: nothing read (the rows would crash a read), the game's step
    g_ss.np_scene_state = 0x10, g_ss.np_pause_mgr = 0x18, g_ss.pause_state = 0x20;
    for (int i = 0; i < 3; i++) rt_nopause_vfr();
    CHECK(g_ss_calls == 0 && !sw_nopause_now());
    CHECK(rt_nopause_paused_step(1.0f) == 1.0f && rt_nopause_paused_step(2.5f) == 2.5f);
    cnt(c);
    CHECK(c[0] == 0 && c[1] == 0 && c[2] == 0);
    g_ss.np_scene_state = A(&g_word), g_ss.np_pause_mgr = A(&g_pm_slot), g_ss.pause_state = A(&g_ps);
    g_pm_slot = A(PM), g_ps = 2;
    // a menu up (the manager's 1), still no permission: left as it is
    pm_store(1);
    rt_nopause_vfr();
    CHECK(g_word == 1 && g_ss_calls == 0);
    // the world may run: the 1 is 0 at A (the update right after the DLL's frame), held at B (the next frame's scene calc)
    sw_nopause_set(1, 0);
    CHECK(sw_nopause_now());
    rt_nopause_vfr();
    cnt(c);
    CHECK(g_word == 0 && c[0] == 1 && c[1] == 0 && c[2] == 0);
    rt_nopause_vfr();
    cnt(c);
    CHECK(g_word == 0 && c[0] == 1 && c[1] == 1);
    pm_store(1);  // (the manager's next store, PostCalc phase 0 / 2)
    sw_nopause_set(1, 0), rt_nopause_vfr();
    sw_nopause_set(1, 0), rt_nopause_vfr();
    cnt(c);
    CHECK(g_word == 0 && c[0] == 2 && c[1] == 2 && c[2] == 0);
    // the manager's 2 / 4 / 5: never touched, and nothing of ours left to put back
    pm_store(2), rt_nopause_vfr();
    CHECK(g_word == 2);
    pm_store(4), rt_nopause_vfr();
    CHECK(g_word == 4);
    pm_store(5), rt_nopause_vfr();
    CHECK(g_word == 5);
    sw_nopause_set(0, 0), rt_nopause_vfr();
    cnt(c);
    CHECK(g_word == 5 && c[0] == 2 && c[1] == 2 && c[2] == 0 && !sw_nopause_now());
    // the DLL's 0 (every stop path): the manager's 1 back at the same update
    sw_nopause_set(1, 0), pm_store(1), rt_nopause_vfr();
    CHECK(g_word == 0);
    sw_nopause_set(0, 0), rt_nopause_vfr();
    cnt(c);
    CHECK(g_word == 1 && c[2] == 1 && !sw_nopause_now());
    rt_nopause_vfr();  // (then nothing more: off)
    cnt(c);
    CHECK(g_word == 1 && c[0] == 3 && c[1] == 2 && c[2] == 1);
    int calls = g_ss_calls;
    for (int i = 0; i < 3; i++) rt_nopause_vfr();
    CHECK(g_ss_calls == calls);  // (off again: nothing read)
    // the menu closed while the world ran: the manager's own 0 / 4 since, left (written as it is or not at all)
    sw_nopause_set(1, 0), pm_store(1), rt_nopause_vfr();
    pm_store(0), g_ps = 0;
    sw_nopause_set(0, 0), rt_nopause_vfr();
    CHECK(g_word == 0);
    sw_nopause_set(1, 0), g_ps = 2, pm_store(1), rt_nopause_vfr();
    pm_store(4);
    sw_nopause_set(0, 0), rt_nopause_vfr();
    CHECK(g_word == 4);
    // the 2 presents: a DLL that stopped calling leaves the word 0 for this present and the next two only
    sw_nopause_set(1, 0), pm_store(1), rt_nopause_vfr();
    CHECK(g_word == 0);
    g_present += 2, rt_nopause_vfr();
    CHECK(g_word == 0 && sw_nopause_now());
    g_present += 1, rt_nopause_vfr();
    CHECK(g_word == 1 && !sw_nopause_now());
    // no manager on the heap (never with a menu up): the UI pause state's rule (0 = 0, bit 0x20 = 2, else 1)
    uint64_t bad[4] = {0, A(PM) + 4, A(&g_word), A(g_heap) + sizeof g_heap - 0x30};
    uint32_t ps[4] = {2, 0x22, 0, 4}, want[4] = {1, 2, 0, 1};
    for (int i = 0; i < 4; i++) {
        g_pm_slot = A(PM), sw_nopause_set(1, 0), pm_store(1), rt_nopause_vfr();
        g_pm_slot = bad[i], g_ps = ps[i], memset(PM + 0x28, 0xee, 4);  // (+0x28 garbage: must not be read)
        sw_nopause_set(0, 0), rt_nopause_vfr();
        CHECK(g_word == want[i]);
    }
    g_pm_slot = A(PM), g_ps = 2;
    // the live flag changes nothing; a live without the world is off
    sw_nopause_set(1, 1), pm_store(1), rt_nopause_vfr();
    CHECK(g_word == 0);
    sw_nopause_set(0, 1), rt_nopause_vfr();
    CHECK(g_word == 1 && !sw_nopause_now());
    // a game version without the row: never written, also with a permission
    g_ss.np_scene_state = 0;
    sw_nopause_set(1, 0), pm_store(1), rt_nopause_vfr();
    CHECK(g_word == 1);
    sw_nopause_set(0, 0), rt_nopause_vfr();
    CHECK(g_word == 1);
    g_ss.np_scene_state = A(&g_word);
    // netnopausestep 2: halved only while the world may run; anything but 2 = 1
    sw_nopause_step(2);
    CHECK(rt_nopause_paused_step(2.0f) == 2.0f);  // (no permission)
    sw_nopause_set(1, 0);
    CHECK(rt_nopause_paused_step(2.0f) == 1.0f && rt_nopause_paused_step(1.0f) == 0.5f);
    sw_nopause_step(3);
    CHECK(rt_nopause_paused_step(2.0f) == 2.0f);
    sw_nopause_step(2), sw_nopause_set(0, 0);
    CHECK(rt_nopause_paused_step(2.0f) == 2.0f);
    sw_nopause_step(1), sw_nopause_set(1, 0);
    CHECK(rt_nopause_paused_step(2.0f) == 2.0f);
    sw_nopause_set(0, 0), rt_nopause_vfr();
    printf("nopause_exe_test: %d checks, %d failed: %s\n", runs, fails, fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

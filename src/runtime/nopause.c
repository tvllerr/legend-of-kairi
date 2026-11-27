// s289 nopause (Sora in BotW; after Savage Wind PR #294 "Online mode" by byluki, fixes by mstampfli, MIT): the exe's half
// of "the world runs in menus" while a friend is in the Melty room. The Sora DLL (runtime/sora_nopause.h, live
// `set netnopause 1`) decides each frame and tells this file through sw_nopause_set (GetProcAddress); the DLL does the
// rest itself through sw_api guest calls (the actors' pause bits, the physics, the scene's paused byte).
//
// No game-code patch: the game library stays Savage Wind's own (game code id f63df6b1a6001541). The game scene's state
// word (1.9.0 0x7001e44ef0, sites row np_scene_state; 1 while a menu is up, and the frame skips the world's jobs while it
// is not 0, as PR #294 found) is written here instead (sb/s289/nopause, recon/s289/S289_HELPER_NOPAUSE.md, static):
//   - its only store is 0x7001281100, called only by the pause manager's calc (0x7000d466d0 -> 0x7000d46940), which runs
//     once a frame in gsys::SystemTask::PostCalc's job phase 0 or 2, after the game tasks' calc;
//   - rt_vfr_update (hle/framepace.c: linkpos_frame, where the DLL runs, then rt_nopause_vfr) runs twice a paused frame:
//     A in PostCalc's phase 9 (0x7001280f80 at 0x7001281044, right before its read of the word at 0x70012810b8) and B in
//     the next frame's scene calc (0x7000c4319c, after the reads at 0x70012800bc / 0x7001280164 / 0x700128322c);
//   - so a 1 made 0 at A holds for the read right after it and for the next frame's three, until the manager's next store.
// The readers that tell 0 from 1 are the world's; the menus' own code compares the word with 2, 4 or 5 only, and the
// manager works its value out of the UI's pause state, never out of the word.
//
// rt_nopause_vfr, at every VFR update: while the DLL's permission holds, a stored 1 becomes 0 ("zeroed"), and a 0 found
// that this file wrote counts as "held" (about one of each per paused frame = the timing above holds; zeroed alone = it
// does not, the world's reads saw the game's 1). The permission gone (the DLL's sw_nopause_set(0, ...), which every stop
// path of the DLL sends inside linkpos_frame, or 2 presents without a call): the game's own value back at once, the one
// the manager stored last (its +0x28), if the word still holds our 0 ("back"). The manager's next store writes its own
// value anyway, every frame.
//
// The paused game updates VFR twice a present (hle/framepace.c: its paused branch hands each update the whole present's
// length). If the world then runs twice as fast as in play behind a menu (not known without the game: the DLL's 1 s line
// shows the hour's step and the updates per present), the DLL's `set netnopausestep 2` halves that step, only while the
// world runs behind a menu (rt_nopause_paused_step).
//
// Off (the DLL's keys 0, a DLL without sora_nopause.h, no DLL, a DLL that stopped calling, a game version without the
// rows): nothing is read or written and rt_nopause_paused_step returns its step: exactly the game's own frames.
#include <stdint.h>
#ifndef NP_HEAP_LO   // (the host test, runtime/test/nopause_exe_test.c, brings its own heap)
#include "kernel.h"  // HEAP_BASE + kernel_heap_size: the guest heap's bounds
#define NP_HEAP_LO HEAP_BASE
#define NP_HEAP_SIZE kernel_heap_size()
#endif
#include "sora_sites.h"

extern uint64_t renderer_present_count(void);  // presented frames (nvn/vulkan/device.c)

static volatile uint64_t g_np_world_until;  // last present the permission holds for (0 = none)
static volatile int g_np_step_div = 1;      // the DLL's netnopausestep (1 = the game's paused step)
static int g_np_zeroed;                     // the word holds a 0 written here (rt_vfr_update's thread only)
static volatile uint64_t g_np_n[3];         // zeroed / held / back (sw_nopause_counts)

#define NP_GW(a) (*(volatile uint32_t *)(uintptr_t)(a))  // a guest u32 (the guest's addresses are the host's)

// the DLL, from linkpos_frame (game thread): world = the world runs behind the menu now. 0 = the game's own values from
// now on (rt_nopause_vfr, right after linkpos_frame, puts the word back). live: unused (a live picture behind the menu
// would need a game-code patch: the menu's frame asks the world's drawing paused with its own `mov w1, #1`).
void sw_nopause_set(int world, int live) {
    (void)live;
    g_np_world_until = world ? renderer_present_count() + 2 : 0;
}

// (for the DLL's diag) 1 while a permission holds now
int sw_nopause_now(void) {
    uint64_t w = g_np_world_until;
    return w && renderer_present_count() <= w;
}

// (for the DLL's 1 s line) the word's writes so far: out[0] zeroed, out[1] held, out[2] back
void sw_nopause_counts(uint64_t out[3]) {
    for (int i = 0; i < 3; i++) out[i] = g_np_n[i];
}

// the game's own value for the word: what the pause manager stored there last (its +0x28 = the store's argument, 1.9.0
// 0x7000d46940). No manager found (never with a menu up): its rule from the UI's pause state (0 = 0, bit 0x20 = 2, else
// 1; its 4 and 5 need guest calls, and its next store puts them back anyway).
static uint32_t np_game_value(const SoraSites *ss) {
    uint64_t pm = ss->np_pause_mgr ? *(volatile uint64_t *)(uintptr_t)ss->np_pause_mgr : 0;
    uint64_t lo = NP_HEAP_LO, size = NP_HEAP_SIZE;
    if (size >= 0x38 && !(pm & 7) && pm >= lo && pm - lo <= size - 0x38) return NP_GW(pm + 0x28);  // (the manager: 0x38 bytes)
    uint32_t ps = ss->pause_state ? NP_GW(ss->pause_state) : 0;
    return !ps ? 0 : (ps & 0x20) ? 2 : 1;
}

// hle/framepace.c rt_vfr_update, right after linkpos_frame (the DLL's frame), every VFR update
void rt_nopause_vfr(void) {
    if (!g_np_zeroed && !g_np_world_until) return;  // (off: nothing read)
    const SoraSites *ss = sora_sites();
    if (!ss->np_scene_state) return;  // (a game version without the row: never written)
    uint32_t v = NP_GW(ss->np_scene_state);
    if (sw_nopause_now()) {
        if (v == 1) NP_GW(ss->np_scene_state) = 0, g_np_zeroed = 1, g_np_n[0]++;
        else if (!v && g_np_zeroed) g_np_n[1]++;
        else if (v) g_np_zeroed = 0;  // (2 / 4 / 5: the manager's own since; left as it is)
        return;
    }
    g_np_world_until = 0;
    if (!g_np_zeroed) return;
    g_np_zeroed = 0;
    if (v) return;  // (the manager stored its own value since)
    NP_GW(ss->np_scene_state) = np_game_value(ss);
    g_np_n[2]++;
}

// the DLL's `set netnopausestep 1|2` (anything else = 1)
void sw_nopause_step(int div) { g_np_step_div = div == 2 ? 2 : 1; }

// hle/framepace.c rt_vfr_update's paused branch: the step it hands the game (the present's length, console frames)
float rt_nopause_paused_step(float step) {
    return g_np_step_div == 2 && sw_nopause_now() ? step * 0.5f : step;
}

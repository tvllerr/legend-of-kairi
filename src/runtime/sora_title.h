// sora_title.h (s199, DLL side): Sora mode fully off on BotW's title screen after a quit to it (BotW's own pause menu >
// back to the title; GUESS: also Savage Wind's F1 "Quit to Start Screen" reboot, no log of one yet), until Continue /
// New Game / Load leaves it.
// Why (s199 live, sb/s198/room/logs/pc_s199a.log 60823-63115): linkpos_world_frame() (exe linkpos.c g_t0) is set the first
// time Link stands in the world and never cleared; on the title the PAUSE word is 0 and linkpos_link_pos still reads a
// body, so sora_mod_frame_body's world gate let Sora drive there: HUD drawn, linkpos's input block took A B X Y L R ZL ZR
// (linkpos.c link_input_filter) and the title never saw Continue.
// Signal: UI screen 0x1 (load_probe's ScreenMgr list, g_title_scr). In every log under sb/ play/ recon/ (60 with the
// screen lines, 61 boots) it is up ONLY on the title: +1 at each boot title (Link state 0) and at the two quits to the
// title (pc_s199a present 29630, lastmp3 55987), -1 when the title's Continue starts the load; never in the world, a
// pause / inventory / map, a load card (0x55), Game Over (0x30), a shrine / Divine Beast, a cutscene or the KH2 camp.
// Only with t0 != 0: the first boot's title (t0 0) keeps the old no-world path (and no log line), as before.
// Live "set titleoff 0" = the old behaviour (Sora drives on the title again). Host test: runtime/test/title_test.c.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SCREEN_TITLE 0x1

enum { TITLE_SAME = 0, TITLE_ENTER = 1, TITLE_LEAVE = 2 };  // title_step: what changed this frame

// One frame of the decision. *on = the state kept between frames (true: Sora mode off for the title). key = "set titleoff"
// (0 = never), t0 = linkpos_world_frame() (0 = Link never stood in the world yet), scr = screen 0x1 is up now.
static int title_step(bool *on, int key, uint64_t t0, bool scr) {
    bool now = key && t0 && scr;
    if (now == *on) return TITLE_SAME;
    *on = now;
    return now ? TITLE_ENTER : TITLE_LEAVE;
}

#ifndef SORA_TITLE_TEST
// ---- the game side (sora.c: LOG) ----
static struct {
    volatile bool on;     // Sora mode off for BotW's title (pad_tap reads it on the input thread)
    int key;              // live "set titleoff 0|1" (default 1 = the fix)
    bool scr;             // screen 0x1 is up (load_probe, every Sora-mode frame)
    bool back_wait;       // left the title: log the first frame back in the world
    uint64_t left_at;     // present the title was left at
} TITLE = {.key = 1};

// Every frame at sora_mod_frame_body's world gate: true = BotW's title after a quit (the caller turns Sora mode off).
static bool title_tick(uint64_t t0, uint64_t present) {
    bool on = TITLE.on;
    int ch = title_step(&on, TITLE.key, t0, TITLE.scr);
    TITLE.on = on;
    if (ch == TITLE_ENTER)
        TITLE.back_wait = false, LOG("[sora] title: BotW's title screen (0x1) at present %llu: Sora mode off there (no HUD / draw / "
                                     "pad block; live \"set titleoff 0\" = the old behaviour)\n", (unsigned long long)present);
    else if (ch == TITLE_LEAVE && !TITLE.key)
        LOG("[sora] title: set titleoff 0 at present %llu: Sora drives on the title again (the old behaviour)\n", (unsigned long long)present);
    else if (ch == TITLE_LEAVE) TITLE.back_wait = true, TITLE.left_at = present;
    return on;
}

// The driving path, once a frame: one line when the world is back after the title (in_world = the room clock's own test).
static void title_back(uint64_t present, int in_world) {
    if (!TITLE.back_wait || !in_world) return;
    TITLE.back_wait = false;
    LOG("[sora] title: back in the world at present %llu (title left at present %llu): Sora mode on as after a save load\n",
        (unsigned long long)present, (unsigned long long)TITLE.left_at);
}

static bool title_set_key(const char *key, int v) {
    if (strcmp(key, "titleoff")) return false;
    TITLE.key = v != 0;
    return true;  // (logged by the live chain's "[sora] live: titleoff <v>")
}
#endif

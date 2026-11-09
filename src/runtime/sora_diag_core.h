// (s183 kairi-diag) The pure decision of the on-screen "Sora's model can't be drawn" notice (host-tested in test/diag_test.c).
// sora.c calls dgc_tick once a game frame (sora_mod_frame) with what it knows:
//   posed      the DLL handed the body a pose this frame (sora_draw_pose / sora_draw_world: it WANTS the body on screen)
//   playing    Sora is the one driving in the world (Link on the ground / in the air, not dead, not loading): the body should be visible
//   nolink     a BotW world is running (not paused, not in the camp menu) but the mod cannot find Link's position
//   drawn_fr   the exe's last frame that drew the body (sora_draw_diag), 0 = never
// and gets back why the notice should show: DG_NODRAW the DLL posed the body > 5 s but the exe drew nothing, DG_NOPOSE playing
// > 30 s without one pose, DG_NOLINK > 20 s without Link; 0 = no notice. Timers reset when the game loop stalls (loading, minimised,
// a debugger: ticks > 1.5 s apart), so a pause never counts and a notice cannot appear for a moment after one.
#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { DG_OK = 0, DG_NODRAW = 1, DG_NOPOSE = 2, DG_NOLINK = 3 };
#define DG_NODRAW_MS 5000   // posed but not drawn for this long
#define DG_NOPOSE_MS 30000  // playing but never posed
#define DG_NOLINK_MS 20000  // a world but no Link
#define DG_GAP_MS 1500      // a tick gap / pose gap longer than this restarts the timers

typedef struct {
    uint64_t last_tick, last_pose, run_start;   // run_start: the pose run's start (a pose after a gap starts a new run)
    uint64_t drawn_fr, drawn_change;            // the exe's last drawn frame seen, and when it changed
    uint64_t nopose_since, nolink_since;        // 0 = not counting
} DgCore;

static int dgc_tick(DgCore *d, uint64_t now, bool posed, bool playing, bool nolink, uint64_t drawn_fr) {
    if (!d->last_tick || now - d->last_tick > DG_GAP_MS || now < d->last_tick) {  // first tick, a stall, or the clock went back
        d->last_pose = 0, d->run_start = now, d->drawn_change = now, d->nopose_since = d->nolink_since = 0;
        d->drawn_fr = drawn_fr;
    }
    d->last_tick = now;
    if (posed) {
        if (!d->last_pose || now - d->last_pose > DG_GAP_MS) d->run_start = now, d->drawn_change = now;
        d->last_pose = now;
    }
    if (drawn_fr != d->drawn_fr) d->drawn_fr = drawn_fr, d->drawn_change = now;
    const bool posed_recent = d->last_pose && now - d->last_pose <= DG_GAP_MS;
    if (playing && !posed_recent) {
        if (!d->nopose_since) d->nopose_since = now;
    } else d->nopose_since = 0;
    if (nolink) {
        if (!d->nolink_since) d->nolink_since = now;
    } else d->nolink_since = 0;
    if (posed_recent && now - d->drawn_change > DG_NODRAW_MS && now - d->run_start > DG_NODRAW_MS) return DG_NODRAW;
    if (d->nopose_since && now - d->nopose_since > DG_NOPOSE_MS) return DG_NOPOSE;
    if (d->nolink_since && now - d->nolink_since > DG_NOLINK_MS) return DG_NOLINK;
    return DG_OK;
}

// Cut a message into lines no wider than max_w, breaking at spaces; the text measure is the caller's (ud: its context).
// start[i] = offset, len[i] = length; returns the line count (<= cap_lines). A word wider than max_w gets a line of its own.
typedef float (*DgMeasure)(void *ud, const char *s, int len);
static int dgc_wrap(const char *msg, float max_w, DgMeasure measure, void *ud, int *start, int *len, int cap_lines) {
    int n = 0, i = 0, total = 0;
    while (msg[total]) total++;
    while (i < total && n < cap_lines) {
        while (i < total && msg[i] == ' ') i++;
        if (i >= total) break;
        int end = i, last_ok = -1;
        while (end <= total) {
            if (end == total || msg[end] == ' ') {
                if (last_ok < 0 || measure(ud, msg + i, end - i) <= max_w) last_ok = end;
                else break;
            }
            end++;
        }
        start[n] = i, len[n] = last_ok - i, n++;
        i = last_ok;
    }
    return n;
}

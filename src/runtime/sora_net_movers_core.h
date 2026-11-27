// sora_net_movers_core.h (s290 labfix): the pure parts of netmovers' end of a friend's stream, and the scene-driven mechanisms
// left out. No game, no clock inside (times come in as ms): host-tested by runtime/test/net_movers_test.c. The glue:
// sora_net_movers.h (NmTrk.back). Needs sora_net_objs_core.h (nm_step, NM_RIDE_* / NM_NEAR_*) first.
//
// Why (lab s290 test 6, E3, Dungeon027): before s290, when his stream ended ("no record of PC2's for 1.5 s: ours again") we only
// stopped writing it, wherever his last record had put it and with whatever state our writes left in it; our game's own logic
// of it never moved it there. VERIFIED in the lab: our entrance elevator then rose by itself (9.4 -> 18 -> 27 -> 53 m, "ours:
// sending"), its pad had no "Examine" prompt (pc could not leave); the other way, after a DESCENT was shown, copy2's rose to 97 m.
// GUESS (no game here): not only a velocity left by objs_body_to (it sets the BODY's velocities to reach his place in one step,
// sora_sites.h; during the 1.5 s hold it is driven to his last place, so that velocity should be ~0, and a descent's would go
// down): the elevator's own logic, woken (objs_wake) and moved off its bottom, may run its own rise. Hence BOTH: the rule below
// for every mover, and the entrance elevators left out (scene-driven, below) until the lab shows the rule is enough for them.
// The rule now: when his stream is over (1.5 s without a record of his, or the actor gone to another unit), it goes back to the
// place OUR game had it at when his stream began (where our game's own logic left it, which never moved it since), and its body
// is stopped there (objs_body_to to the place it is at = velocity 0, NM_SETTLE_N frames) before we stop writing it. The way back
// never jumps under our Link (the same steps as his moves: ride / near / at once), waits while our Link stands where it goes
// back to (it would come down on him), lets go when its own logic moves it away from there (NM_AWAY_N frames in a row: a gear
// that turns by itself, a lift its own logic sends; nothing written then) and gives up after NM_BACK_MS (its body stopped where
// it is then).
// Scene-driven ones (nm_scene_driven): a shrine's entrance elevator and its shutter move in each player's OWN enter / leave
// scene: in his world he leaves, in ours we are still inside. They are not watched (not sent, not shown) unless the lab key
// netmoverelev 1 puts them back (to test this rule on them).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define NM_SETTLE_N 10      // frames at its own place (objs_body_to there) before we let go: the velocity his left is 0 again
#define NM_BACK_MS 10000    // the way back ends this late if never there: stopped where it is
#define NM_GAP_MS 250       // (s291 review 2) a gap this long between way-back frames = a menu / load / cutscene: not counted in NM_BACK_MS
#define NM_AWAY_N 8         // frames in a row it went farther from our place (its own logic moves it, not our steps): let go
#define NM_AWAY_M 0.01f     // ... by more than this (place m + turn rad) a frame

// a shrine's entrance elevator (DgnObj_EntranceElevator_A_01, DgnObj_EntranceElevatorSP) or its shutter
// (DgnObj_EntranceShutter_A_01, DgnObj_EntranceShutterSP): 1.9.0 ActorInfo names, read s290 (sb/s290/labfix/names.py)
static inline bool nm_scene_driven(const char *n) { return n && !strncmp(n, "DgnObj_Entrance", 15); }

typedef struct {
    bool have;          // own holds our place of it from before his stream
    bool on;            // on its way back now
    bool waiting;       // our Link stands where it goes back to: it waits (stopped once)
    int settle;         // frames at its own place so far
    int away;           // frames in a row it went farther from our place
    float off_was;      // how far from our place it was at the last frame (-1: not known)
    uint64_t since;     // the way back began (or our Link left its place) at
    uint64_t last;      // (s291) the last way-back frame at (gaps past NM_GAP_MS move since on)
    float own[12];      // our place of it before his stream began
} NmBack;

enum { NMB_NONE, NMB_BODY, NMB_BOTH, NMB_HOLD, NMB_STOP, NMB_DONE, NMB_OWN };

// how far m is from our place: its place (m) + its turn (rad); NaN = far
static inline float nm_back_off(const float m[12], const float own[12]) {
    float qa[4], qb[4];
    net_q_from_m(m, qa), net_q_from_m(own, qb);
    double dx = m[3] - own[3], dy = m[7] - own[7], dz = m[11] - own[11];
    double dot = fabs(qa[0] * qb[0] + qa[1] * qb[1] + qa[2] * qb[2] + qa[3] * qb[3]);
    double r = sqrt(dx * dx + dy * dy + dz * dz) + 2 * acos(dot > 1 ? 1 : dot);
    return r == r ? (float)r : 1e30f;
}

// his stream begins (his first record taken after none): our own place kept, cur = its matrix now (cur_ok: read). On its way
// back (a new stream of his before it got there): the way back stops, the place kept is still the first one (ours)
static inline void nm_back_his(NmBack *b, const float cur[12], bool cur_ok) {
    if (b->on || b->have) { b->on = b->waiting = false; if (b->have) return; }
    b->have = cur_ok;
    if (cur_ok) memcpy(b->own, cur, sizeof b->own);
}
// his stream is over: true = it goes back to our own place from now; false = no place kept (stop it where it is)
static inline bool nm_back_start(NmBack *b, uint64_t now) {
    b->settle = b->away = 0, b->off_was = -1, b->waiting = false, b->since = b->last = now, b->on = b->have;
    return b->on;
}
// one frame of the way back. cur = its matrix now; at_cur = our Link is at it where it is (on it), at_own = at the place it goes
// back to, nearby = within NM_SAFE_R of either. -> what the glue writes, out = the matrix:
//   NMB_BODY  through its body only, a ride step (our Link on it: it carries him, never jumps under him)
//   NMB_BOTH  body + matrix (a near step, or at once when our Link is far)
//   NMB_HOLD  its body stopped where it is (out = cur), once: it waits while our Link stands where it goes back to
//   NMB_STOP  its body stopped where it is (out = cur): given up (NM_BACK_MS), the way back is over
//   NMB_DONE  there and settled: nothing more written (ours again: its own logic)
//   NMB_OWN   its own logic moves it away from our place (NM_AWAY_N frames): let go, nothing written (ours again)
//   NMB_NONE  nothing to do (not on its way back, or still waiting)
static inline int nm_back_step(NmBack *b, const float cur[12], bool at_cur, bool at_own, bool nearby, uint64_t now, float out[12]) {
    if (!b->on) return NMB_NONE;
    if (b->last && now > b->last && now - b->last > NM_GAP_MS) b->since += now - b->last;  // (s291) menu time is not way-back time
    b->last = now;
    if (at_own && !at_cur) {
        b->since = now, b->settle = b->away = 0, b->off_was = -1;
        if (b->waiting) return NMB_NONE;
        b->waiting = true;
        memcpy(out, cur, sizeof(float) * 12);
        return NMB_HOLD;
    }
    b->waiting = false;
    float off = nm_back_off(cur, b->own);
    b->away = b->off_was >= 0 && off > b->off_was + NM_AWAY_M ? b->away + 1 : 0, b->off_was = off;
    if (b->away >= NM_AWAY_N) {
        b->on = b->have = false;
        return NMB_OWN;
    }
    if (now - b->since > NM_BACK_MS) {
        memcpy(out, cur, sizeof(float) * 12);
        b->on = b->have = false;
        return NMB_STOP;
    }
    bool held;
    if (at_cur) held = nm_step(cur, b->own, NM_RIDE_STEP, NM_RIDE_RAD, out);
    else if (nearby) held = nm_step(cur, b->own, NM_NEAR_STEP, NM_NEAR_RAD, out);
    else memcpy(out, b->own, sizeof(float) * 12), held = false;
    if (held) b->settle = 0;
    else if (++b->settle > NM_SETTLE_N) {
        b->on = b->have = false;
        return NMB_DONE;
    }
    return at_cur ? NMB_BODY : NMB_BOTH;
}

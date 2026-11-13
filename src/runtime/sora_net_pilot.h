// (s209 step 2, ours) Melty's autopilot, the game's side: the lab's commands (pilot=1 in melty.properties: Melty's lab
// only; the connector queues nothing otherwise) done Sora's own way, so his animation and physics are real:
//   goto x y z [gait]  the kit's steering (melty_addon.h PilotSteer through the connector: heading, arrived within 1 m,
//                      stuck = no 0.25 m of progress in 5 s, a jump wanted after 1.5 s) turned into the stick Sora reads, on
//                      the camera as the driving tick turns it (sora.c: dir = cf * sy + (-cf.z, cf.x) * sx), applied where
//                      Melty's test self-drive's stick is (mtest_stick: no pad, no window focus). gait walk = a stick of
//                      NP_WALK_STICK (under KH2's run threshold after its round dead zone), else full (run; Sora has no
//                      sprint). NOT the live "goto" (a held long teleport) and NOT the live "input" (needs focus).
//   face yaw           Sora turns to the kit's heading (his own S.face: the same convention, 0 faces +z, towards +x).
//   attack <peer>      Sora turns to that player's puppet as drawn here, then one press of his attack (the live "attack"
//                      path, atk_cmd_queue). GUESS: one swing per command.
//   act attack [s]     presses of his attack for s seconds (one each ATK_CMD_GAP_MS, at least 1, at most ATK_CMD_MAX) or 1.
//   act jump           one jump press.
//   stay / stop        the stick let go, waiting presses dropped.
//   level, wander, die, act <other>, act event:  not done: "failed".
// Each command's PILOT note: moving while it runs (the helper says it once a second), then arrived / stuck / failed; a new
// command ends the running one (idle). With autopilot off (np_tick's io.on() 0) nothing ever drives Sora, and a command
// still running is let go at once; (s209 review 3) when autopilot is back (a connection blip), that command is answered
// "failed" first.
// (s209 review 2) ONE owner of the stick and the attack presses: the DLL's own Melty test mode (sora_melty_test.h self-drive)
// until the FIRST command of the session is taken, the autopilot from then on (io.took_first, once, before that command
// starts: the test mode's self-drive stands down for the rest of the session). Before it the autopilot touches neither.
// Pure: every game / connector call goes through NpIo, so runtime/test/net_note_test.c runs it against fakes; the game
// binds it in sora.c (pilot_frame, pilot_stick, pilot_press).
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sora_melty_types.h"

#define NP_ARRIVE_M 1.0f      // the kit's PilotSteer default arrive radius (m)
#define NP_WALK_STICK 0.74f   // round dead zone 60 of 127.5: 0.74 -> magnitude 0.51, under KH2's run 0.8 (sora_params.h)
#define NP_PRESS_MS 300       // = sora.c ATK_CMD_GAP_MS (one press a combo step)
#define NP_PRESS_MAX 20       // = sora.c ATK_CMD_MAX
#define NP_DT_MAX 0.25f       // a longer frame (a stall) counts as this much for the stuck clock

typedef struct {
    bool (*on)(void);                                                   // autopilot on (pilot=1, in the room)
    int (*take)(sora_melty_pilot *out);                                 // the next command
    void (*report)(int number, int state, float distance);              // its PILOT note
    void (*steer_target)(float x, float y, float z, float radius);
    void (*steer_stop)(void);
    void (*steer_update)(float dt, float x, float y, float z, sora_melty_steer *out);
    void (*face)(float yaw);                                            // Sora turns to the kit's heading
    bool (*peer_feet)(int peer, float out[3]);                          // that player's puppet as drawn here
    void (*attack)(int presses);                                        // queue presses (0: drop the waiting ones)
    int (*attack_left)(void);                                           // presses still waiting
    unsigned (*attack_dropped)(void);                                   // queues dropped unfired so far
    void (*took_first)(void);                                           // (review 2) the first command: the stick is ours now
} NpIo;

enum { NP_NONE, NP_GOTO, NP_PRESS };
typedef struct {
    NpIo io;
    int number, mode;     // the running command and what it waits for
    bool walk;
    bool stick_on;        // the stick below drives Sora (goto only)
    float stick[2];       // -1..1, x right, y forward (the driving tick's)
    bool jump;            // one jump press wanted (np_take_jump)
    unsigned dropped0;    // attack_dropped() when the presses were queued
    unsigned cmds, failed;  // (diag) commands taken, answered failed
    bool owns;            // (review 2) a command was taken this session: the stick and the presses are the autopilot's
    int lost;             // (review 3) a command let go while autopilot was off (a blip): answered failed when it is back
} NetPilot;

// a world heading (the kit's: direction (sin h, cos h) in x, z) as the stick that walks there with the camera's forward
// cf (x, z; no camera: (0, 1), the driving tick's own fallback); magnitude mag
static inline void np_stick_for(float heading, const float cf_in[2], float mag, float out[2]) {
    float cf[2] = {0, 1};
    if (cf_in && isfinite(cf_in[0]) && isfinite(cf_in[1])) {
        float l = sqrtf(cf_in[0] * cf_in[0] + cf_in[1] * cf_in[1]);
        if (l > 1e-4f) cf[0] = cf_in[0] / l, cf[1] = cf_in[1] / l;
    }
    float w[2] = {sinf(heading), cosf(heading)}, r[2] = {-cf[1], cf[0]};
    out[0] = (w[0] * r[0] + w[1] * r[1]) * mag;   // sx: along the camera's right
    out[1] = (w[0] * cf[0] + w[1] * cf[1]) * mag; // sy: along its forward
}
// presses for "act attack [seconds]"
static inline int np_presses(int has_seconds, float seconds) {
    if (!has_seconds || !(seconds > 0)) return 1;
    int n = (int)ceilf(seconds * 1000.0f / NP_PRESS_MS);
    return n < 1 ? 1 : n > NP_PRESS_MAX ? NP_PRESS_MAX : n;
}

static inline void np_let_go(NetPilot *p) {
    if (p->mode == NP_GOTO && p->io.steer_stop) p->io.steer_stop();
    p->mode = NP_NONE, p->stick_on = false, p->stick[0] = p->stick[1] = 0;
}
static inline void np_fail(NetPilot *p, int number) {
    p->failed++;
    p->io.report(number, SORA_MELTY_PILOT_FAILED, 0);
}

// one command taken: the one running ends (idle), this one starts (or is answered at once)
static inline void np_start(NetPilot *p, const sora_melty_pilot *c, const float feet[3]) {
    if (p->mode != NP_NONE) {
        if (p->mode == NP_PRESS) p->io.attack(0);
        p->io.report(p->number, SORA_MELTY_PILOT_IDLE, 0);
        np_let_go(p);
    }
    p->cmds++;
    p->number = c->number, p->jump = false;
    switch (c->kind) {
        case SORA_MELTY_PILOT_GOTO:
            if (!isfinite(c->x) || !isfinite(c->y) || !isfinite(c->z)) { np_fail(p, c->number); return; }
            p->io.steer_target(c->x, c->y, c->z, NP_ARRIVE_M);
            p->mode = NP_GOTO, p->walk = !strcmp(c->gait, "walk");
            p->io.report(c->number, SORA_MELTY_PILOT_MOVING, 0);
            return;
        case SORA_MELTY_PILOT_FACE:
            if (!isfinite(c->yaw)) { np_fail(p, c->number); return; }
            // (s209 review 1) into -PI..PI first: S.face's wrap() is a subtract-2*PI loop that never ends for a huge yaw
            p->io.face(remainderf(c->yaw, 6.28318531f));
            p->io.report(c->number, SORA_MELTY_PILOT_ARRIVED, 0);
            return;
        case SORA_MELTY_PILOT_STAY:
        case SORA_MELTY_PILOT_STOP:
            p->io.attack(0);
            p->io.report(c->number, SORA_MELTY_PILOT_ARRIVED, 0);
            return;
        case SORA_MELTY_PILOT_ATTACK: {
            float pf[3];
            if (!feet || !p->io.peer_feet(c->peer, pf)) { np_fail(p, c->number); return; }  // (not drawn here: no one to face)
            float dx = pf[0] - feet[0], dz = pf[2] - feet[2];
            if (dx * dx + dz * dz > 1e-6f) p->io.face(atan2f(dx, dz));
            p->dropped0 = p->io.attack_dropped();
            p->io.attack(1);
            p->mode = NP_PRESS;
            p->io.report(c->number, SORA_MELTY_PILOT_MOVING, 0);
            return;
        }
        case SORA_MELTY_PILOT_ACT:
            if (!strcmp(c->action, "attack")) {
                p->dropped0 = p->io.attack_dropped();
                p->io.attack(np_presses(c->has_seconds, c->seconds));
                p->mode = NP_PRESS;
                p->io.report(c->number, SORA_MELTY_PILOT_MOVING, 0);
                return;
            }
            if (!strcmp(c->action, "jump")) {
                p->jump = true;
                p->io.report(c->number, SORA_MELTY_PILOT_ARRIVED, 0);
                return;
            }
            np_fail(p, c->number);
            return;
        default:  // level, wander, die, act event: not done
            np_fail(p, c->number);
            return;
    }
}

// once a mod frame. driven: Sora drove this frame (feet = his feet, BotW metres); cf = the camera's forward (NULL: none)
static inline void np_tick(NetPilot *p, bool driven, float dt, const float feet[3], const float cf[2]) {
    if (!p->io.on || !p->io.on()) {  // autopilot off: nothing drives Sora, ever
        if (p->mode != NP_NONE) p->lost = p->number;  // (review 3: answered failed when autopilot is back)
        if (p->owns && p->mode == NP_PRESS && p->io.attack) p->io.attack(0);
        np_let_go(p);
        p->jump = false;
        return;
    }
    if (p->lost) {  // (review 3) the command a blip let go: failed, before anything new
        p->io.report(p->lost, SORA_MELTY_PILOT_FAILED, 0);
        p->failed++, p->lost = 0;
    }
    sora_melty_pilot c;
    for (int i = 0; i < 8 && p->io.take(&c); i++) {
        if (!p->owns) {  // (review 2) the first command of the session: the stick and the presses are ours from now on
            p->owns = true;
            if (p->io.took_first) p->io.took_first();
        }
        np_start(p, &c, driven ? feet : NULL);
    }
    if (p->mode == NP_PRESS) {
        if (p->io.attack_left() > 0) return;
        bool dropped = p->io.attack_dropped() != p->dropped0;
        p->io.report(p->number, dropped ? SORA_MELTY_PILOT_FAILED : SORA_MELTY_PILOT_ARRIVED, 0);
        if (dropped) p->failed++;
        p->mode = NP_NONE;
        return;
    }
    if (p->mode != NP_GOTO || !driven || !feet) return;  // (the stuck clock runs only while Sora can move)
    if (!(dt > 0) || !isfinite(dt)) return;
    sora_melty_steer s;
    p->io.steer_update(dt > NP_DT_MAX ? NP_DT_MAX : dt, feet[0], feet[1], feet[2], &s);
    if (s.arrived || s.stuck) {
        p->io.report(p->number, s.arrived ? SORA_MELTY_PILOT_ARRIVED : SORA_MELTY_PILOT_STUCK, s.distance);
        p->mode = NP_NONE, p->stick_on = false, p->stick[0] = p->stick[1] = 0;
        return;
    }
    np_stick_for(s.heading, cf, p->walk ? NP_WALK_STICK : 1.0f, p->stick);
    p->stick_on = true;
    if (s.want_jump) p->jump = true;
    p->io.report(p->number, SORA_MELTY_PILOT_MOVING, s.distance);
}

// the driving tick asks: the stick (only while a goto runs, autopilot on), one jump press (not on a fresh frame)
static inline bool np_stick(const NetPilot *p, float out[2]) {
    if (!p->owns || !p->stick_on || p->mode != NP_GOTO) return false;
    out[0] = p->stick[0], out[1] = p->stick[1];
    return true;
}
static inline bool np_take_jump(NetPilot *p, bool fresh) {
    if (!p->owns || !p->jump || fresh) return false;
    p->jump = false;
    return true;
}

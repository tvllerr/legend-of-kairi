// The remote Sora's swing ribbon (multiplayer slice 2 Task 2): pure core, no sora.c statics (host-tested in test/net_test.c).
// Rebuilds the ribbon from his snapshot (form, slot, clock, feet, yaw) through callbacks for the hit table, the trail window
// and the blade point; decimated to 30 Hz (the local ribbon gets one sample per game frame), reset on a slot change or a
// backward clock, quiet for a bone pose / Anti / hidden. Output per blade: push? + on + grip + tip (-> sora_fx_trail_ex).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define NETRIB_BLADES 2
#define NETRIB_STEP_MS 33  // ~30 Hz

typedef struct {
    // hit table row of (form, slot, blade): NULL = no blade swing; form -1 = base Sora, else the form index (body - 1)
    const void *(*hit)(void *ud, int form, int slot, int blade);
    bool (*win)(void *ud, int form, int slot, float clock);  // inside a weapon_trail window
    // the blade at motion time `clock`: out = grip xyz + tip xyz, world units (feet + RotY(yaw) * to_u * model)
    void (*blade)(void *ud, const void *row, float clock, const float feet[3], float yaw, float to_u, float out[6]);
    void *ud;
} NetRibCb;

typedef struct {
    bool active;       // false: bone pose, Anti (no blade), hidden / dropped avatar -> only the closing on = false
    int form, slot;    // form -1 = base, else body - 1
    float clock;
    float feet[3];     // already incl. the loopback offset
    float yaw, to_u;
    uint64_t now;      // ms
} NetRibIn;

typedef struct { bool push, on; float grip[3], tip[3]; } NetRibOut;

typedef struct {
    bool live[NETRIB_BLADES];  // the last push was on = true (a closing on = false is still owed)
    int slot;                  // last slot seen, -1 = none
    float clock;
    uint64_t next;             // next 30 Hz tick
} NetRib;

static void netrib_init(NetRib *r) { memset(r, 0, sizeof *r); r->slot = -1; }

static void netrib_close(NetRib *r, NetRibOut out[NETRIB_BLADES]) {
    for (int b = 0; b < NETRIB_BLADES; b++) {
        memset(&out[b], 0, sizeof out[b]);
        if (r->live[b]) out[b].push = true;  // (on = false once, then quiet)
        r->live[b] = false;
    }
}

// one avatar tick -> out[blade]; returns the number of pushes
static int netrib_step(NetRib *r, const NetRibCb *cb, const NetRibIn *in, NetRibOut out[NETRIB_BLADES]) {
    int n = 0;
    if (!in->active || !cb || !cb->hit || !cb->win || !cb->blade) {
        netrib_close(r, out);
        r->slot = -1;
        for (int b = 0; b < NETRIB_BLADES; b++) n += out[b].push;
        return n;
    }
    bool reset = r->slot != in->slot || in->clock < r->clock - 0.001f;
    r->slot = in->slot, r->clock = in->clock;
    if (reset) {
        netrib_close(r, out);
        r->next = in->now;  // the new swing may push on the next tick
        for (int b = 0; b < NETRIB_BLADES; b++) n += out[b].push;
        return n;
    }
    for (int b = 0; b < NETRIB_BLADES; b++) memset(&out[b], 0, sizeof out[b]);
    if (in->now < r->next) return 0;  // (decimation: never per display frame)
    r->next += NETRIB_STEP_MS;
    if (r->next <= in->now) r->next = in->now + NETRIB_STEP_MS;
    for (int b = 0; b < NETRIB_BLADES; b++) {
        const void *row = cb->hit(cb->ud, in->form, in->slot, b);
        bool on = row && cb->win(cb->ud, in->form, in->slot, in->clock);
        if (on) {
            float w[6];
            cb->blade(cb->ud, row, in->clock, in->feet, in->yaw, in->to_u, w);
            memcpy(out[b].grip, w, sizeof out[b].grip), memcpy(out[b].tip, w + 3, sizeof out[b].tip);
            out[b].push = out[b].on = true, r->live[b] = true, n++;
        } else if (r->live[b]) {
            out[b].push = true, r->live[b] = false, n++;  // the window closed: on = false once
        }
    }
    return n;
}

// sora_net_gear_core.h (s289 netgear): the pure parts of "the other players see the BotW weapon in Sora's hand" - no game, no
// socket, no exe; host-tested by runtime/test/netgear_test.c. The glue is sora_net_gear.h (send / receive, live keys) and
// sora_net_gear_draw.h (the weapon model at the puppet's hand); the wire is NET_GEAR in sora_net_proto.h.
//   ngear_local   our list from what wpn_tick told the exe this frame (the weapon's actor name, in the hand or put away)
//   ngear_due     send now? (a change, NET_GEAR_GAP_MS after the last send at least; a new peer; the slow refresh)
//   ngear_fresh   is his last list still good (NET_GEAR_FORGET quiet periods = forgotten: a game that stopped sending)
//   ngear_hand    the melee weapon in his hand in a list (NULL = none)
//   ngear_rows    his grip bone (model space) + the exe's hand offsets -> the weapon bone of a 1-bone gear bake, the same
//                 placement linkpos.c wpn_model gives Link's weapon in the LOCAL Sora's hand
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "sora_net_proto.h"

#define NET_GEAR_MS 2000         // UDP: resend this often (a lost packet is fixed by the next one)
#define NET_GEAR_MELTY_MS 10000  // Melty room: the event is reliable, so only now and then (a seat change / a missed join)
#define NET_GEAR_FORGET 5        // his list not heard for this many of OUR periods = not known again (drawn as before)
#define NET_GEAR_GAP_MS 250      // two change sends at least this far apart (a weapon flicking = one packet per 250 ms at most)

// our list: n = 0 (no BotW melee weapon in use) or 1 (kind melee, NGF_HAND when it is in his hand now). A name that cannot go
// on the wire (too long, a letter net_area_chars refuses) = nothing.
static inline int ngear_local(NetGearItem out[NET_GEAR_N], const char *melee, bool in_hand) {
    memset(out, 0, sizeof(NetGearItem) * NET_GEAR_N);
    if (!melee || !melee[0]) return 0;
    int l = 0;
    while (l <= NET_GEAR_NAME_MAX && melee[l]) l++;
    if (l > NET_GEAR_NAME_MAX || !net_area_chars(melee, l)) return 0;
    out[0].kind = NGK_MELEE, out[0].flags = in_hand ? NGF_HAND : 0;
    memcpy(out[0].name, melee, (size_t)l);
    return 1;
}

static inline bool ngear_same(const NetGearItem *a, int na, const NetGearItem *b, int nb) {
    if (na != nb) return false;
    for (int i = 0; i < na; i++)
        if (a[i].kind != b[i].kind || a[i].flags != b[i].flags || strcmp(a[i].name, b[i].name)) return false;
    return true;
}

// send now? changed = our list is not the one sent last; joined = a peer slot appeared since; never sent = the first one goes
// at once. A change waits NET_GEAR_GAP_MS after the last send (it is still sent then: the list is compared again each tick).
static inline bool ngear_due(bool changed, bool joined, bool sent_any, uint64_t now, uint64_t next_ms, uint64_t last_tx_ms) {
    if (!sent_any) return true;
    bool gap = now >= last_tx_ms + NET_GEAR_GAP_MS || now < last_tx_ms;  // (a clock that went back: no wait)
    if ((changed || joined) && gap) return true;
    return now >= next_ms;
}

// his list taken at at_ms (0 = never) is still his: heard within NET_GEAR_FORGET periods
static inline bool ngear_fresh(uint64_t at_ms, uint64_t now, uint64_t period) {
    if (!at_ms) return false;
    return !(now > at_ms && now - at_ms > NET_GEAR_FORGET * period);
}

// the melee weapon in his hand (the first NGK_MELEE entry with NGF_HAND), NULL = none
static inline const char *ngear_hand(const NetGearItem *it, int n) {
    for (int i = 0; it && i < n; i++)
        if (it[i].kind == NGK_MELEE && (it[i].flags & NGF_HAND) && it[i].name[0]) return it[i].name;
    return NULL;
}

// The weapon bone for a 1-bone gear bake (its vertices in cm, the BotW model's own frame: blade +Z, metres x 100) in the
// puppet's model space, so sora_draw_actor_world(gear, out, feet, yaw, 0.01 k) puts the weapon where linkpos.c wpn_model puts
// Link's weapon for the local Sora:
//   grip   = the grip bone's model-space matrix (rows r0 r1 r2 = its axes, r3 = the joint, cm; sora_draw_actor_slot_pose)
//   off[8] = the exe's hand offsets (sora_wpn.h wpn_hand_off: back m, roll / pitch / yaw deg, scale, grip point m)
//   k      = g_k (the puppet is drawn KH cm x 0.01 k -> BotW units; BotW units are metres)
// The exe works in the world (axes = the model rows turned by the facing, unit length); a turn about y keeps every step below
// the same, so it is done here in model space and the draw turns the result. Weapon axes: X = A0, Y = A2, Z = -A1, then the
// turns (1 roll about Z, 2 pitch about X, 3 yaw about Y), scale s; origin = grip - Z back - s (X gx + Y gy + Z gz).
// In model space: rows = axes x s / k (cm -> cm), origin = grip + (that offset in m) x 100 / k.
// False = no usable grip (a hidden Keyblade's bones are squashed to a point: nothing to hold the weapon) or a bad k.
static inline bool ngear_rows(const float grip[12], const float off[8], float k, float out[12]) {
    if (!grip || !off || !out || !(k > 1e-6f)) return false;
    float A[3][3];
    for (int r = 0; r < 3; r++) {
        float x = grip[r * 3], y = grip[r * 3 + 1], z = grip[r * 3 + 2], l = sqrtf(x * x + y * y + z * z);
        if (!(l > 1e-6f)) return false;
        A[r][0] = x / l, A[r][1] = y / l, A[r][2] = z / l;
    }
    float X[3] = {A[0][0], A[0][1], A[0][2]}, Y[3] = {A[2][0], A[2][1], A[2][2]}, Z[3] = {-A[1][0], -A[1][1], -A[1][2]};
    for (int t = 1; t <= 3; t++) {  // (linkpos.c wpn_model's turns, the same order and signs)
        if (off[t] == 0) continue;
        float r = off[t] * 0.0174533f, cs = cosf(r), sn = sinf(r);
        float *p = t == 1 ? X : t == 2 ? Y : Z, *q = t == 1 ? Y : t == 2 ? Z : X;
        float np[3], nq[3];
        for (int i = 0; i < 3; i++) np[i] = cs * p[i] + sn * q[i], nq[i] = -sn * p[i] + cs * q[i];
        memcpy(p, np, sizeof np), memcpy(q, nq, sizeof nq);
    }
    float s = off[4] > 0.05f && off[4] < 4.0f ? off[4] : 1.0f, f = s / k, c = 100.0f / k;
    for (int i = 0; i < 3; i++) {
        out[0 + i] = X[i] * f, out[3 + i] = Y[i] * f, out[6 + i] = Z[i] * f;
        out[9 + i] = grip[9 + i] + c * (-Z[i] * off[0] - s * (X[i] * off[5] + Y[i] * off[6] + Z[i] * off[7]));
    }
    for (int i = 0; i < 12; i++)
        if (!(out[i] == out[i])) return false;  // (a NaN anywhere: not drawn)
    return true;
}

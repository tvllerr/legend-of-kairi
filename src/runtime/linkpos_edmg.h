// s166 E2 (shared enemy HP, exe side): the pure-C parts of linkpos_edmg_apply - no guest memory, no locks, no clocks
// (time = game frames passed in). Included by linkpos.c, host-tested by runtime/test/edmg_test.c.
//   - EdmgPendTab : hits / kills for an enemy (mubin HashId) that is not loaded here yet. Damage rides EDMG_PEND_TTL
//                   game frames (~120 s at 60 fps); a KILL has no TTL (rule 3.8 "far kills stay dead"): it waits until the
//                   enemy loads here, then it is killed (the game's own death sets its IsDefeat flag), or the session ends.
//   - EdmgEchoTab : the remote damage a ride put into an enemy this frame; the capture hook subtracts it again so it is
//                   never sent back out (no ping-pong, no double count).
//   - EdmgArgTab  : f54 / f40 of the frame's addDamage calls per actor, kept one frame longer (applyDamage may run first).
#pragma once
#include <stdint.h>
#include <string.h>

#define EDMG_PEND_MAX 256        // (spec said 64: kills have no TTL, so a long session needs room)
#define EDMG_PEND_TTL 7200       // game frames a damage-only entry waits
#define EDMG_DMG_CAP 1000000
#define EDMG_RIDE_TTL 120        // game frames a remote hit waits for its actor's addDamage call before it goes back to pending

typedef struct { uint32_t hid; int32_t dmg, f54, f40; uint8_t kill; uint64_t first; } EdmgPend;
typedef struct { EdmgPend e[EDMG_PEND_MAX]; int n; uint32_t evicted, expired; } EdmgPendTab;

// add a hit / kill for an enemy not loaded here. false = nothing stored (hid 0 or nothing to store).
static inline int edmg_pend_add(EdmgPendTab *t, uint32_t hid, int32_t dealt, int32_t f54, int32_t f40, int kill, uint64_t now) {
    if (!hid || (dealt <= 0 && !kill)) return 0;
    if (dealt < 0) dealt = 0;
    for (int i = 0; i < t->n; i++)
        if (t->e[i].hid == hid) {
            EdmgPend *p = &t->e[i];
            p->dmg = p->dmg + dealt > EDMG_DMG_CAP ? EDMG_DMG_CAP : p->dmg + dealt;
            if (f54 > p->f54) p->f54 = f54, p->f40 = f40;
            p->kill |= kill ? 1 : 0;
            return 1;
        }
    if (t->n == EDMG_PEND_MAX) {  // full: the oldest damage-only entry goes first, else the oldest kill
        int best = -1;
        for (int i = 0; i < t->n; i++)
            if (!t->e[i].kill && (best < 0 || t->e[i].first < t->e[best].first)) best = i;
        if (best < 0) {
            best = 0;
            for (int i = 1; i < t->n; i++) if (t->e[i].first < t->e[best].first) best = i;
        }
        t->e[best] = t->e[--t->n];
        t->evicted++;
    }
    EdmgPend *p = &t->e[t->n++];
    p->hid = hid, p->dmg = dealt > EDMG_DMG_CAP ? EDMG_DMG_CAP : dealt, p->f54 = f54, p->f40 = f40, p->kill = kill ? 1 : 0, p->first = now;
    return 1;
}
// drop damage-only entries older than the TTL
static inline void edmg_pend_expire(EdmgPendTab *t, uint64_t now) {
    for (int i = 0; i < t->n;)
        if (!t->e[i].kill && now - t->e[i].first > EDMG_PEND_TTL) t->e[i] = t->e[--t->n], t->expired++;
        else i++;
}
// the entry of this enemy (removed from the table): 1 = out filled; 0 = none / a damage-only entry that expired
static inline int edmg_pend_take(EdmgPendTab *t, uint32_t hid, uint64_t now, EdmgPend *out) {
    for (int i = 0; i < t->n; i++)
        if (t->e[i].hid == hid) {
            EdmgPend p = t->e[i];
            t->e[i] = t->e[--t->n];
            if (!p.kill && now - p.first > EDMG_PEND_TTL) { t->expired++; return 0; }
            *out = p;
            return 1;
        }
    return 0;
}

// ---- f54 / f40 of a ride entry (linkpos.c g_dmg_q: a frame's local hits + the remote part for one enemy) ----
// s194 BUG A: an entry made only for a remote hit used to start at f54 15 and keep the higher value, so the attacker's
// f54 7 (Shock: upright stagger) rode as 15 (small flinch, partial ragdoll). A fresh entry now starts at NONE (0 = no
// real hit yet); every real hit (local or remote) merges in with the old rule: f54 <= 0 = small (15), the highest
// f54 wins with its f40. The ride itself takes edmg_ride_f54 (NONE -> 15).
#define EDMG_F54_SMALL 15
#define EDMG_RIDE_F54_NONE 0
static inline void edmg_f54_merge(int32_t *cur_f54, int32_t *cur_f40, int32_t f54, int32_t f40) {
    if (f54 <= 0) f54 = EDMG_F54_SMALL;
    if (f54 > *cur_f54) *cur_f54 = f54, *cur_f40 = f40;
}
static inline int32_t edmg_ride_f54(int32_t f54) { return f54 > 0 ? f54 : EDMG_F54_SMALL; }

// ---- the amount a remote hit really puts into the local enemy (computed at ride time with its CURRENT life) ----
// kill: life > dealt -> dealt = life (HP drifted: the kill still kills). A hit that did not kill there never kills here
// (capped at life - 1: a drifted HP must not give a death the sender did not have). Dead / unknown life: 0.
static inline int32_t edmg_remote_amount(int32_t life, int32_t dealt, int kill) {
    if (life <= 0) return 0;
    if (kill) return dealt > life ? dealt : life;
    if (dealt <= 0) return 0;
    return dealt > life - 1 ? life - 1 : dealt;
}

enum { EDMG_IGNORED = 0, EDMG_QUEUED = 1, EDMG_PENDING = 2 };
// what linkpos_edmg_apply does with a hit: loaded = the enemy is in the list and was seen lately; life / state = its
// current values (state 3 = BaseProc being deleted; life -1 = could not be read).
// (s167 review SF-6) unreadable, or being deleted while still alive (unloading out of range) = pending, so a kill is not lost
static inline int edmg_apply_plan(int loaded, int32_t life, int state, int32_t dealt, int kill) {
    if (!loaded || life < 0 || (state == 3 && life > 0)) return (dealt > 0 || kill) ? EDMG_PENDING : EDMG_IGNORED;
    if (life <= 0) return EDMG_IGNORED;
    return edmg_remote_amount(life, dealt, kill) > 0 ? EDMG_QUEUED : EDMG_IGNORED;
}

// ---- echo guard ----
#define EDMG_ECHO_N 16
typedef struct { uint64_t actor, frame; int32_t amt; } EdmgEcho;
typedef struct { EdmgEcho e[EDMG_ECHO_N]; int n; } EdmgEchoTab;
static inline void edmg_echo_add(EdmgEchoTab *t, uint64_t actor, int32_t amt, uint64_t frame) {
    if (!actor || amt <= 0) return;
    for (int i = 0; i < t->n;)  // (entries two frames old were never taken: gone)
        if (frame - t->e[i].frame > 2 && frame >= t->e[i].frame) t->e[i] = t->e[--t->n];
        else i++;
    for (int i = 0; i < t->n; i++)
        if (t->e[i].actor == actor) { t->e[i].amt += amt, t->e[i].frame = frame; return; }
    if (t->n == EDMG_ECHO_N) {
        int o = 0;
        for (int i = 1; i < t->n; i++) if (t->e[i].frame < t->e[o].frame) o = i;
        t->e[o] = t->e[--t->n];
    }
    t->e[t->n].actor = actor, t->e[t->n].amt = amt, t->e[t->n++].frame = frame;
}
// the remote damage noted for this actor (added in the last 2 frames), removed; 0 = none
static inline int32_t edmg_echo_take(EdmgEchoTab *t, uint64_t actor, uint64_t frame) {
    for (int i = 0; i < t->n; i++)
        if (t->e[i].actor == actor) {
            int32_t a = t->e[i].amt;
            int fresh = frame >= t->e[i].frame && frame - t->e[i].frame <= 2;
            t->e[i] = t->e[--t->n];
            return fresh ? a : 0;
        }
    return 0;
}

// ---- f54 / f40 of a frame's hits per actor (review E1 SHOULD-FIX 1: applyDamage may run before addDamage) ----
#define EDMG_ARG_N 16
typedef struct { uint64_t actor, frame; int32_t f54, f40; } EdmgArg;
typedef struct { EdmgArg e[EDMG_ARG_N]; int n; } EdmgArgTab;
static inline void edmg_arg_note(EdmgArgTab *t, uint64_t actor, uint64_t frame, int32_t f54, int32_t f40) {
    for (int i = 0; i < t->n; i++)
        if (t->e[i].actor == actor) {
            EdmgArg *a = &t->e[i];
            if (a->frame != frame || f54 > a->f54) a->f54 = f54, a->f40 = f40;
            a->frame = frame;
            return;
        }
    if (t->n == EDMG_ARG_N) {
        int o = 0;
        for (int i = 1; i < t->n; i++) if (t->e[i].frame < t->e[o].frame) o = i;
        t->e[o] = t->e[--t->n];
    }
    t->e[t->n].actor = actor, t->e[t->n].frame = frame, t->e[t->n].f54 = f54, t->e[t->n++].f40 = f40;
}
// the highest f54 noted in this frame or the one before; 0 = none
static inline int edmg_arg_get(const EdmgArgTab *t, uint64_t actor, uint64_t frame, int32_t *f54, int32_t *f40) {
    for (int i = 0; i < t->n; i++)
        if (t->e[i].actor == actor && frame >= t->e[i].frame && frame - t->e[i].frame <= 1) { *f54 = t->e[i].f54, *f40 = t->e[i].f40; return 1; }
    return 0;
}

// s168 v7: "enemies act the same on every game", DLL side. Spec docs/specs/2026-11-07-multiplayer-enemy-sync-design.md,
// report recon/S168_HELPER_ESYNC_NET.md.  Two parts:
//   1. CORE (pure C, no NET, no clock, time passed in; host-tested by runtime/test/esync_test.c): owner election, per-hid sample ring,
//      interpolation, the exe interface (EsIface), sensor-event queue, packet building.
//   2. GLUE (only when sora_net.h defines NET_ESYNC_GLUE before including this): binds the exe exports, feeds players from NET,
//      sends / receives NET_ESYNC, live keys `set esync 0|1`, `set esynclog 0|1`, `net diag` line.
// Model: the owner (nearest player) runs the enemy's brain and sends its state 20 Hz; every other game turns that enemy's brain OFF and
// moves / animates the body from the owner's samples (a "puppet"). Hits on you stay local (your game decides).
#pragma once
#ifdef _WIN32
#include <windows.h>  // (s179 diag timers)
#endif
#include <stdio.h>
#include "sora_net_proto.h"

// ---- constants (all GUESS until tested in game) ----
#define ES_MAX 48               // enemies tracked at once (GUESS: a big camp is ~10)
#define ES_HANDOVER_M 5.0f      // GUESS: another player must be this much CLOSER than the owner ...
#define ES_HANDOVER_MS 1000     // GUESS: ... for this long, for a hand-over
#define ES_STALE_BAN_MS 60000   // s179: 60 s (was 10 s: "closer" hand-overs to a player without it loaded came back every ~10 s); s177: a player who went silent as owner cannot win it back as "closer" this long (unless his samples come back)
#define ES_STALE_MS 1000        // owner silent (no ESYNC for that hid) this long -> the next nearest player takes over
#define ES_SNAP_M 3.0f          // puppet further than this from its target = hard snap (flag to the exe)
#define ES_INTERP_MS 100        // GUESS: render this far behind the newest sample (2 packets at 20 Hz)
#define ES_SEND_MS 50           // 20 Hz
#define ES_FORGET_MS 10000      // an enemy neither loaded here nor heard of for this long leaves the table
#define ES_HOLD_MS 1000         // past the newest sample the puppet holds its pose / advances the frame at most this long
#define ES_ANIM_FPS 30.0f       // GUESS: unit of EsState.frame (game frames per second), only used when frame != 0
#define ES_TIE_M 0.001f
#define ES_BRAIN_REFRESH_MS 2000  // a puppet's brain(0) is told again this often (the exe drops hids it has not seen for ENEMY_STALE frames: loading screens)
#define ES_EVQ 32
#define ES_PLAYERS 4
// s180 two-phase hand-over (no split brain): nobody hands an enemy AWAY on its own timer. The player who should take it (closer, or the
// owner went silent) CLAIMS it: owns it at once and sends its samples with ES_FL_CLAIM. The old owner keeps owning + sending until it
// RECEIVES a claim sample, then yields. Third players follow the claim samples. Startup double claims keep the name tie-break.
#define ES_FL_CLAIM 0x8000u     // wire flag on a claimant's samples (the exe uses flags 1 / 2 only; stripped before the puppet sees it)
#define ES_CLAIM_MIN_MS 300     // a claim lasts at least this long (third players see >= 6 claim samples) ...
#define ES_CLAIM_QUIET_MS 300   // ... and until the old owner has been quiet this long = hand-over done
#define ES_CLAIM_MS 2000        // the old owner still sends after this long = it never saw the claim: give it back
#define ES_CLAIM_BAN_MS 5000    // after a refused claim, no new claim of that enemy this long
#define ES_RECLAIM_MS 3000      // after we yielded to a claim, we do not claim it back this long (views 100-200 ms apart)
#define ES_QUIET_MS 500         // a non-owner's samples while the owner is quiet this long = follow him (a missed claim)

typedef NetEsEnt EsState;       // the state struct of the exe interface = the wire entry (40 B)
typedef NetEsEv EsEvent;        // sensor event of the exe interface (seq is filled by the DLL)

// The exe side fills these (GetProcAddress in the glue: linkpos_esync_enum / _read / _puppet / _brain / _events / _sensor). All six or the feature is off.
//   enum_enemies(hids, max)        -> n: HashIds of the placed enemies loaded here and near ANY player (hid 0 = spawned: the DLL skips them)
//   read_state(hid, out)           -> 1 ok: the enemy's current body state (pos, yaw, anim slots, chg counters, action, flags). out->hid = hid
//   puppet_apply(hid, target, snap)  every frame while it is a puppet: move / turn the body to target->pos/yaw (snap=1: teleport, > ES_SNAP_M off),
//                                    start target->anim_hash[i] in bank anim_bank[i] when the hash OR anim_chg[i] differs from what it last applied
//                                    (hash 0 = no animation in that slot), target->action / flags for info, target->frame only if != 0
//   set_brain(hid, on)             only on CHANGE (and again after the enemy reloads here): on=0 -> the brain tick of this one enemy stops
//   take_events(out, max)          -> n: owner side, drains the attack-sensor ON / OFF events the exe saw since last call (hid, on, name hash, params)
//   puppet_sensor(hid, on, name, params)  puppet side: ON with the owner's name hash + 4 opaque u32; OFF with name 0 = ALL sensors of that hid off
//                                    (the DLL calls OFF/0 on every hand-over, stale owner, brain back on)
// Name strings: only HASHES travel. The hash function is the exe's; GUESS: crc32 of the lowercase AS / sensor name. The exe keeps a
// hash -> name table filled while it polls its own enemies' names (both sides run the same enemy types, so a puppet that needs a name
// it never saw polls it from its own ASList); the DLL never needs the strings.
typedef struct {
    int (*enum_enemies)(uint32_t *hids, int max);
    int (*read_state)(uint32_t hid, EsState *out);
    void (*puppet_apply)(uint32_t hid, const EsState *target, int snap);
    void (*set_brain)(uint32_t hid, int on);
    int (*take_events)(EsEvent *out, int max);
    void (*puppet_sensor)(uint32_t hid, int on, uint32_t name, const uint32_t params[4]);
} EsIface;

typedef struct { uint8_t id; char name[16]; float pos[3]; } EsPlayer;
typedef struct {
    bool used, local, have_pos;
    uint32_t hid;
    int owner, cand;                  // player ids, -1 = none
    uint64_t cand_since, alive_ms, touch_ms;
    int stale_pl; uint64_t stale_ms;  // s177: the last owner that went silent (-1 none): the enemy is not loaded there (dead / out of range)
    float pos[3];
    struct { uint64_t at; EsState s; } sm[3]; int nsm;
    EsState cur; bool cur_ok;         // the local body this step
    int brain;                        // -1 unknown, 0 off, 1 on (as last told the exe)
    uint64_t brain_ms;                // when brain(0) was last told
    int claim_from;                   // s180: >= 0 = we claimed it from this player and send ES_FL_CLAIM until he is quiet (-1 none)
    uint64_t claim_ms, claim_heard;   //   when we claimed / last heard the old owner's samples
    uint64_t claim_ban, yield_ms;     //   0 = never: a refused claim / we yielded to a claim at this time
} EsEnt;
typedef struct {
    uint32_t tx_ent, tx_ev, tx_pkt, tx_bytes, rx_ent, rx_ev, rx_pkt, rx_bytes, rx_dup, rx_bad, rx_notowner, rx_conflict, rx_off, rx_ev_dup, rx_ev_notowner,
        rx_ev_nolocal, hid0_skip, handovers, stale, full, sensor_off, ev_dropped, yielded;
    uint32_t claims, claim_done, claim_refused, claim_yield, rx_claimwait, tx_claim;  // s180
    uint32_t pup_apply, pup_notlocal, pup_nosample, steps;  // s179 diag: puppet frames applied / skipped (not in this frame's enum, no fresh sample)
} EsStats;
typedef struct {
    EsEnt e[ES_MAX];
    EsIface io;
    uint8_t me;
    EsPlayer pl[ES_PLAYERS]; int np;
    uint64_t next_send, t0;
    EsEvent evq[ES_EVQ]; uint8_t evrep[ES_EVQ]; int nevq; uint8_t evseq;
    bool log;                         // esynclog: per tx / rx lines
    EsStats st;
    // s178 R2 club sync (DLL only, optional): owner-side extra events (same owner + loaded filter as take_events) and the
    // receiver for events whose name is ES_EV_CLUB (never handed to the exe's puppet_sensor). now = GetTickCount64 ms.
    int (*take_extra)(EsEvent *out, int max);
    void (*on_extra)(const EsEvent *ev, uint64_t now);
} EsCtx;
#define ES_EV_CLUB 0x434c5542u        // 'CLUB': a rod / club hit ON / OFF of the owner's Bokoblin swing (sora_esa_probe.h), params = timing
typedef struct { EsState ent[ES_MAX]; int nent; } EsOut;

#ifndef ES_LOG
#define ES_LOG(...) LOG(__VA_ARGS__)
#endif

static inline uint64_t es_age(uint64_t now, uint64_t then) { return now > then ? now - then : 0; }  // (never negative: ticks and packets may stamp slightly different clocks)
static inline float es_dist(const float *a, const float *b) {
    float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return sqrtf(x * x + y * y + z * z);
}
static inline EsEnt *es_find(EsCtx *c, uint32_t hid) {
    for (int i = 0; i < ES_MAX; i++) if (c->e[i].used && c->e[i].hid == hid) return &c->e[i];
    return NULL;
}
static inline EsEnt *es_get(EsCtx *c, uint32_t hid, uint64_t now) {
    EsEnt *f = es_find(c, hid);
    if (f) return f;
    for (int i = 0; i < ES_MAX; i++)
        if (!c->e[i].used) {
            f = &c->e[i];
            memset(f, 0, sizeof *f);
            f->used = true, f->hid = hid, f->owner = f->cand = f->stale_pl = f->claim_from = -1, f->brain = -1, f->touch_ms = f->alive_ms = now;
            return f;
        }
    c->st.full++;
    return NULL;
}
static inline int es_pidx(const EsCtx *c, int id) {
    for (int i = 0; i < c->np; i++) if (c->pl[i].id == id) return i;
    return -1;
}
static inline const char *es_pname(const EsCtx *c, int id, char *buf) {
    int i = es_pidx(c, id);
    if (i >= 0) return c->pl[i].name;
    if (id < 0) return "none";
    snprintf(buf, 16, "player%d", id);
    return buf;
}
static inline void es_set_players(EsCtx *c, const EsPlayer *pl, int np) {
    c->np = np > ES_PLAYERS ? ES_PLAYERS : np;
    for (int i = 0; i < c->np; i++) c->pl[i] = pl[i];
}
static inline void es_drop_events(EsCtx *c, uint32_t hid) {
    for (int i = 0; i < c->nevq;)
        if (c->evq[i].hid == hid) {
            c->st.ev_dropped++;
            memmove(c->evq + i, c->evq + i + 1, sizeof c->evq[0] * (size_t)(c->nevq - i - 1));
            memmove(c->evrep + i, c->evrep + i + 1, (size_t)(c->nevq - i - 1));
            c->nevq--;
        } else i++;
}
static inline void es_sensors_off(EsCtx *c, EsEnt *e) {
    if (e->local && c->io.puppet_sensor) c->io.puppet_sensor(e->hid, 0, 0, NULL), c->st.sensor_off++;
}

// nearest player to p, skipping player id `excl` (-1 = none); ties (within ES_TIE_M) -> lower name, then lower id. -> index in pl, -1 = none
static inline int es_nearest(const EsCtx *c, const float *p, int excl) {
    int best = -1;
    float bd = 0;
    for (int i = 0; i < c->np; i++) {
        if (c->pl[i].id == excl) continue;
        float d = es_dist(c->pl[i].pos, p);
        bool better = best < 0 || d < bd - ES_TIE_M;
        if (!better && best >= 0 && d <= bd + ES_TIE_M) {
            int cmp = strcmp(c->pl[i].name, c->pl[best].name);
            better = cmp < 0 || (cmp == 0 && c->pl[i].id < c->pl[best].id);
        }
        if (better) best = i, bd = d;
    }
    return best;
}

static void es_assign(EsCtx *c, EsEnt *e, int idx, const char *why, uint64_t now) {
    char b1[16], b2[16];
    int old = e->owner;
    ES_LOG("[esync] hid=%08x owner %s -> %s%s\n", e->hid, es_pname(c, old, b1), es_pname(c, c->pl[idx].id, b2), why);
    e->owner = c->pl[idx].id, e->alive_ms = now, e->cand = -1;
    if (old >= 0) c->st.handovers++;
    if (old >= 0) es_sensors_off(c, e);                  // (a sensor left on mid-swing would stay on)
    if (old == c->me || e->owner == c->me) es_drop_events(c, e->hid);
    e->nsm = 0;                                          // (ours: our own body is the truth now; his: never blend the old owner's samples into his - s180)
}

// s180: we take it: owner = me at once, our samples carry ES_FL_CLAIM until the old owner is quiet (es_claim_progress)
static void es_claim(EsCtx *c, EsEnt *e, const char *why, uint64_t now) {
    int mi = es_pidx(c, c->me);
    if (mi < 0) return;
    int from = e->owner;
    uint64_t heard = e->alive_ms;                        // (the old owner's last sample)
    c->st.claims++;
    es_assign(c, e, mi, why, now);
    e->claim_from = from >= 0 && from != c->me ? from : 255;  // (255 = nobody to wait for: still flag ES_CLAIM_MIN_MS so others follow)
    e->claim_ms = now, e->claim_heard = e->claim_from == 255 ? 0 : heard;
}
static void es_claim_progress(EsCtx *c, EsEnt *e, uint64_t now) {
    if (e->owner != c->me) { e->claim_from = -1; return; }
    char b1[16];
    if (es_age(now, e->claim_ms) >= ES_CLAIM_MIN_MS && es_age(now, e->claim_heard) >= ES_CLAIM_QUIET_MS) {
        c->st.claim_done++;
        if (c->log) ES_LOG("[esync] hid=%08x claim done (%s quiet)\n", e->hid, es_pname(c, e->claim_from == 255 ? -1 : e->claim_from, b1));
        e->claim_from = -1;
    } else if (es_age(now, e->claim_ms) >= ES_CLAIM_MS) {  // he never saw it (one-way loss / old DLL): give it back, no new claim for a while
        int f = es_pidx(c, e->claim_from);
        c->st.claim_refused++;
        e->claim_from = -1, e->claim_ban = now ? now : 1;
        if (f >= 0) es_assign(c, e, f, " (claim refused: he kept sending)", now);
    }
}
static bool es_banned(const EsEnt *e, uint64_t now) {
    return (e->claim_ban && es_age(now, e->claim_ban) < ES_CLAIM_BAN_MS) || (e->yield_ms && es_age(now, e->yield_ms) < ES_RECLAIM_MS);
}

static void es_elect(EsCtx *c, EsEnt *e, uint64_t now) {
    if (c->np == 0 || !e->have_pos) return;
    if (e->claim_from >= 0) { es_claim_progress(c, e, now); return; }
    if (e->owner < 0) {                                  // nobody yet: everybody elects the nearest (double claims: es_on_recv tie-break)
        int n = es_nearest(c, e->pos, -1);
        if (n >= 0 && c->pl[n].id == c->me && !e->local) n = es_nearest(c, e->pos, c->me);  // (s179: never claim one not loaded here)
        if (n >= 0) es_assign(c, e, n, "", now);
        return;
    }
    if (e->owner == c->me) { e->cand = -1; return; }    // s180: never give it away on our own timers; a claim sample takes it
    int cur = es_pidx(c, e->owner);
    if (cur < 0 || es_age(now, e->alive_ms) > ES_STALE_MS) {  // owner gone / silent: the nearest OTHER player that has it loaded claims it
        e->cand = -1;
        if (!e->local || es_banned(e, now)) return;      // (we could not send it: wait for somebody's claim; loaded nowhere = nobody flips)
        int n = es_nearest(c, e->pos, e->owner);
        if ((n >= 0 && c->pl[n].id == c->me) || es_age(now, e->alive_ms) > 3 * ES_STALE_MS) {  // (the nearer one never claims: we do after 3 s)
            c->st.stale++;
            e->stale_pl = e->owner, e->stale_ms = now;
            es_claim(c, e, cur < 0 ? " (old owner gone: claim)" : " (stale owner: claim)", now);
        }
        return;
    }
    if (!e->local || es_banned(e, now)) { e->cand = -1; return; }
    int n = es_nearest(c, e->pos, -1);                   // only WE can claim it as "closer"; others wait for our claim samples
    if (n >= 0 && c->pl[n].id == c->me && es_dist(c->pl[cur].pos, e->pos) - es_dist(c->pl[n].pos, e->pos) >= ES_HANDOVER_M) {
        if (e->cand != c->me) e->cand = c->me, e->cand_since = now;
        if (es_age(now, e->cand_since) >= ES_HANDOVER_MS) es_claim(c, e, " (closer: claim)", now);
    } else e->cand = -1;
}

// the pose a puppet should have `now`: samples sm[0..nsm-1] ascending by arrival; render ES_INTERP_MS behind
static bool es_target(const EsEnt *e, uint64_t now, EsState *out) {
    if (e->nsm <= 0) return false;
    uint64_t r = now > ES_INTERP_MS ? now - ES_INTERP_MS : 0;
    int i = -1;
    for (int k = 0; k < e->nsm; k++) if (e->sm[k].at <= r) i = k;
    if (i < 0) { *out = e->sm[0].s; return true; }        // (render time before the oldest sample)
    const EsState *a = &e->sm[i].s;
    *out = *a;
    uint64_t el = r - e->sm[i].at;
    if (i + 1 < e->nsm) {
        const EsState *b = &e->sm[i + 1].s;
        uint64_t span = e->sm[i + 1].at - e->sm[i].at;
        float t = span ? (float)el / (float)span : 1.0f;
        if (t > 1) t = 1;
        for (int k = 0; k < 3; k++) out->pos[k] = a->pos[k] + (b->pos[k] - a->pos[k]) * t;
        float dy = b->yaw - a->yaw;
        const float pi = 3.14159265f;
        while (dy > pi) dy -= 2 * pi;
        while (dy < -pi) dy += 2 * pi;
        out->yaw = a->yaw + dy * t;
    } else if (el > ES_HOLD_MS) el = ES_HOLD_MS;
    if (a->frame != 0.0f) out->frame = a->frame + (float)el * ES_ANIM_FPS / 1000.0f;  // (0 = unknown: never advanced)
    return true;
}

static void es_push_sample(EsEnt *e, const EsState *s, uint64_t now) {
    if (e->nsm == 3) memmove(e->sm, e->sm + 1, sizeof e->sm[0] * 2), e->nsm--;
    e->sm[e->nsm].at = now, e->sm[e->nsm].s = *s, e->nsm++;
    e->pos[0] = s->pos[0], e->pos[1] = s->pos[1], e->pos[2] = s->pos[2], e->have_pos = true;
}

static void es_queue_event(EsCtx *c, const EsEvent *ev) {
    if (c->nevq == ES_EVQ) {  // full: the oldest gives way
        memmove(c->evq, c->evq + 1, sizeof c->evq[0] * (ES_EVQ - 1)), memmove(c->evrep, c->evrep + 1, ES_EVQ - 1), c->nevq--, c->st.ev_dropped++;
    }
    c->evq[c->nevq] = *ev, c->evq[c->nevq].seq = c->evseq++, c->evrep[c->nevq++] = NET_EVENT_REPEAT;
}

static double g_es_pup_ms_sum, g_es_pup_ms_max, g_es_tick_ms_max;  // s179 diag (Windows only; host tests too)
static uint32_t g_es_pup_ms_hid, g_es_pup_ms_act, g_es_pup_ms_over30;
// once per frame. Players must be set first. out = states due to send (every ES_SEND_MS).
static void es_step(EsCtx *c, uint64_t now, EsOut *out) {
    out->nent = 0;
    c->st.steps++;
    bool due = now >= c->next_send;
    if (due) c->next_send = c->next_send && now - c->next_send < ES_SEND_MS ? c->next_send + ES_SEND_MS : now + ES_SEND_MS;
    bool was_local[ES_MAX];
    for (int i = 0; i < ES_MAX; i++) was_local[i] = c->e[i].local, c->e[i].local = false, c->e[i].cur_ok = false;
    uint32_t hids[ES_MAX];
    int n = c->io.enum_enemies ? c->io.enum_enemies(hids, ES_MAX) : 0;
    if (n > ES_MAX) n = ES_MAX;
    for (int i = 0; i < n; i++) {
        if (!hids[i]) { c->st.hid0_skip++; continue; }
        EsEnt *e = es_get(c, hids[i], now);
        if (!e) continue;
        e->local = true, e->touch_ms = now;
        if (c->io.read_state && c->io.read_state(hids[i], &e->cur) > 0) {
            e->cur.hid = hids[i], e->cur_ok = true;
            e->pos[0] = e->cur.pos[0], e->pos[1] = e->cur.pos[1], e->pos[2] = e->cur.pos[2], e->have_pos = true;
        }
    }
    // owner side: the sensor events the exe saw (only for enemies we own and have loaded; others are dropped)
    EsEvent ev[16];
    int ne = c->io.take_events ? c->io.take_events(ev, 16) : 0;
    for (int i = 0; i < ne && i < 16; i++) {
        EsEnt *e = ev[i].hid ? es_find(c, ev[i].hid) : NULL;
        if (e && e->local && e->owner == c->me && ev[i].on <= 1) es_queue_event(c, &ev[i]);
    }
    ne = c->take_extra ? c->take_extra(ev, 16) : 0;  // (s178 club ON / OFF: same filter)
    for (int i = 0; i < ne && i < 16; i++) {
        EsEnt *e = ev[i].hid ? es_find(c, ev[i].hid) : NULL;
        if (e && e->local && e->owner == c->me && ev[i].on <= 1) es_queue_event(c, &ev[i]);
    }
    for (int i = 0; i < ES_MAX; i++) {
        EsEnt *e = &c->e[i];
        if (!e->used) continue;
        es_elect(c, e, now);
        if (!e->local && es_age(now, e->touch_ms) > ES_FORGET_MS) {  // (S168 MF-1: a brain we stopped is given back before the entry goes)
            if (e->brain == 0 && c->io.set_brain) c->io.set_brain(e->hid, 1);
            memset(e, 0, sizeof *e);
            continue;
        }
        if (!e->local) {
            if (e->owner >= 0 && e->owner != c->me && e->nsm > 0 && es_age(now, e->sm[e->nsm - 1].at) <= ES_STALE_MS) c->st.pup_notlocal++;
            continue;
        }
        if (!was_local[i]) e->brain = -1;                 // (just (re)loaded here: tell the exe again)
        if (e->owner == c->me || e->owner < 0) {
            if (e->brain != 1) c->io.set_brain(e->hid, 1), e->brain = 1;
            if (e->owner == c->me && e->cur_ok && due) {
                if (out->nent < ES_MAX) {
                    out->ent[out->nent] = e->cur;
                    if (e->claim_from >= 0) out->ent[out->nent].flags |= ES_FL_CLAIM, c->st.tx_claim++;  // (s180)
                    out->nent++;
                }
                e->alive_ms = now;
            }
            continue;
        }
        bool have = e->nsm > 0 && es_age(now, e->sm[e->nsm - 1].at) <= ES_STALE_MS;
        if (!have) c->st.pup_nosample++;
        if (!have) {                                      // owner not heard from: the brain runs again (the election hands it on)
            if (e->brain == 0) es_sensors_off(c, e);
            if (e->brain != 1) c->io.set_brain(e->hid, 1), e->brain = 1;
            continue;
        }
        if (e->brain != 0 || es_age(now, e->brain_ms) >= ES_BRAIN_REFRESH_MS) c->io.set_brain(e->hid, 0), e->brain = 0, e->brain_ms = now;
        EsState t;
        if (es_target(e, now, &t)) {
            t.hid = e->hid;
            LARGE_INTEGER q0, q1, qf;
            QueryPerformanceCounter(&q0);
            c->io.puppet_apply(e->hid, &t, e->cur_ok && es_dist(e->cur.pos, t.pos) > ES_SNAP_M);
            c->st.pup_apply++;
            QueryPerformanceCounter(&q1), QueryPerformanceFrequency(&qf);
            double pm = (double)(q1.QuadPart - q0.QuadPart) * 1000.0 / (double)qf.QuadPart;  // (s179 diag: one puppet's frame, ms)
            g_es_pup_ms_sum += pm;
            if (pm > g_es_pup_ms_max) g_es_pup_ms_max = pm, g_es_pup_ms_hid = e->hid, g_es_pup_ms_act = t.action;
            if (pm > 30) g_es_pup_ms_over30++;
        }
    }
}

// build the packets for out + the queued events; tx(bytes, len, user) per packet. A queued event rides NET_EVENT_REPEAT packets.
// gate = the 50 ms send gate opened this step (events alone go out only then).
static void es_packets(EsCtx *c, const EsOut *out, bool gate, uint8_t player, uint16_t *seq, void (*tx)(const uint8_t *, int, void *), void *user) {
    int i = 0;
    bool done[ES_EVQ] = {0};  // (an event rides ONE packet per call = per send slot: its NET_EVENT_REPEAT copies are 50 ms apart)
    for (;;) {
        bool pend = false;
        for (int j = 0; gate && j < c->nevq; j++) pend = pend || (c->evrep[j] > 0 && !done[j]);
        if (!(i < out->nent || pend)) break;
        NetEsEnt e[NET_ESYNC_MAX];
        NetEsEv v[NET_ESYNC_EVMAX];
        int k = 0, m = 0;
        while (k < NET_ESYNC_MAX && i < out->nent) e[k++] = out->ent[i++];
        for (int j = 0; gate && j < c->nevq && m < NET_ESYNC_EVMAX; j++)
            if (c->evrep[j] > 0 && !done[j]) v[m++] = c->evq[j], c->evrep[j]--, done[j] = true;
        if (k + m == 0) break;
        uint8_t b[NET_ESYNC_PKT];
        int len = net_pack_esync(player, ++*seq, e, k, v, m, b, (int)sizeof b);
        if (len <= 0) break;
        c->st.tx_pkt++, c->st.tx_ent += (uint32_t)k, c->st.tx_ev += (uint32_t)m, c->st.tx_bytes += (uint32_t)len;
        if (c->log) {
            for (int j = 0; j < k; j++)
                ES_LOG("[esync] tx hid=%08x pos %.1f %.1f %.1f yaw %.2f anim %08x/%08x chg %u/%u act %u fl %u (seq %u)\n", e[j].hid, e[j].pos[0], e[j].pos[1], e[j].pos[2],
                       e[j].yaw, e[j].anim_hash[0], e[j].anim_hash[1], e[j].anim_chg[0], e[j].anim_chg[1], e[j].action, e[j].flags, (unsigned)*seq);
            for (int j = 0; j < m; j++) ES_LOG("[esync] tx sensor hid=%08x %s name %08x (evseq %u)\n", v[j].hid, v[j].on ? "ON" : "OFF", v[j].name, v[j].seq);
        }
        tx(b, len, user);
    }
    for (int j = 0; j < c->nevq;)  // (finished events leave the queue)
        if (c->evrep[j] == 0) {
            memmove(c->evq + j, c->evq + j + 1, sizeof c->evq[0] * (size_t)(c->nevq - j - 1));
            memmove(c->evrep + j, c->evrep + j + 1, (size_t)(c->nevq - j - 1));
            c->nevq--;
        } else j++;
}

// a NET_ESYNC packet from player `sender` (already de-duped by packet seq). evseen = that peer's sensor-event seq window.
static void es_on_recv(EsCtx *c, uint8_t sender, const EsState *s, int ns, const EsEvent *ev, int nev, NetEvSeen *evseen, uint64_t now) {
    if (es_pidx(c, sender) < 0) { c->st.rx_notowner += (uint32_t)(ns + nev); return; }   // (not a player we know the place of: loading, or no SNAP yet)
    for (int i = 0; i < ns; i++) {
        c->st.rx_ent++;
        EsEnt *e = es_get(c, s[i].hid, now);
        if (!e) continue;
        if (!e->local) e->pos[0] = s[i].pos[0], e->pos[1] = s[i].pos[1], e->pos[2] = s[i].pos[2], e->have_pos = true;
        e->touch_ms = now;
        bool claim = (s[i].flags & ES_FL_CLAIM) != 0;
        int si = es_pidx(c, sender);
        if (e->owner < 0) {                               // first we hear of it: the sender claims it, we believe him
            es_assign(c, e, si, claim ? " (claim)" : " (first sample)", now);
        } else if (e->owner == c->me) {
            if (!claim && e->claim_from == sender) {      // s180: the old owner's last samples before he saw our claim
                e->claim_heard = now, c->st.rx_claimwait++;
                continue;
            }
            bool yield;
            if (claim && e->claim_from < 0) yield = true, c->st.claim_yield++;  // s180: a claim always takes one we own (we sent until now)
            else {                                        // both think they own it (startup / both claimed at once): the same tie-break on both games
                c->st.rx_conflict++;
                int mi = es_pidx(c, c->me);               // S168 MF-3: lower name (then lower id) keeps it
                int cmp = mi >= 0 ? strcmp(c->pl[si].name, c->pl[mi].name) : -1;
                bool cant = !e->local;                    // (s179: not loaded here = we could never send it: he keeps it, whatever the names say)
                yield = cant || mi < 0 || cmp < 0 || (cmp == 0 && sender < c->me);
            }
            if (!yield) continue;
            c->st.yielded++;
            e->claim_from = -1, e->yield_ms = now ? now : 1;
            es_assign(c, e, si, claim ? " (claim: yield)" : " (conflict: yield)", now);
        } else if (e->owner != sender && (claim || es_age(now, e->alive_ms) > ES_QUIET_MS)) {  // a claim, or the owner is quiet: follow him
            es_assign(c, e, si, claim ? " (claim)" : " (his samples arrived)", now);
        }
        if (e->owner != sender) { c->st.rx_notowner++; continue; }
        EsState sv = s[i];
        sv.flags &= (uint16_t)~ES_FL_CLAIM;
        es_push_sample(e, &sv, now);
        e->alive_ms = now;
        if (e->stale_pl == sender) e->stale_pl = -1;  // (s177: he has it loaded again)
    }
    for (int i = 0; i < nev; i++) {
        c->st.rx_ev++;
        if (!net_event_fresh(evseen, ev[i].seq)) { c->st.rx_ev_dup++; continue; }
        EsEnt *e = es_find(c, ev[i].hid);
        if (!e || e->owner != sender) { c->st.rx_ev_notowner++; continue; }
        if (!e->local) { c->st.rx_ev_nolocal++; continue; }
        if (ev[i].name == ES_EV_CLUB) {  // (s178: the DLL's club sync, not an exe sensor)
            if (c->on_extra) c->on_extra(&ev[i], now);
            if (c->log) ES_LOG("[esync] rx club hid=%08x %s at %u ms (evseq %u)\n", ev[i].hid, ev[i].on ? "ON" : "OFF", ev[i].params[0], ev[i].seq);
            continue;
        }
        if (c->io.puppet_sensor) c->io.puppet_sensor(ev[i].hid, ev[i].on, ev[i].name, ev[i].params);
        if (c->log) ES_LOG("[esync] rx sensor hid=%08x %s name %08x (evseq %u)\n", ev[i].hid, ev[i].on ? "ON" : "OFF", ev[i].name, ev[i].seq);
    }
}

// esync off / net off: every brain we stopped runs again, every sensor we forced is cleared, the table is empty
static void es_release_all(EsCtx *c) {
    for (int i = 0; i < ES_MAX; i++) {
        EsEnt *e = &c->e[i];
        if (e->used && e->brain == 0 && c->io.set_brain) es_sensors_off(c, e), c->io.set_brain(e->hid, 1);  // (S168 MF-1: unloaded ones too; sensors_off only if loaded)
    }
    memset(c->e, 0, sizeof c->e);
    c->nevq = 0, c->np = 0, c->next_send = 0;
}
static int es_count_owned(const EsCtx *c, int *puppets) {
    int o = 0, p = 0;
    for (int i = 0; i < ES_MAX; i++)
        if (c->e[i].used && c->e[i].local) { if (c->e[i].owner == c->me) o++; else if (c->e[i].brain == 0) p++; }
    if (puppets) *puppets = p;
    return o;
}

#ifdef NET_ESYNC_GLUE
// ===================== GLUE (needs NET, net_dests, net_send_to, LOG from sora_net.h) =====================
static bool g_net_esync;      // live "set esync 1" (default 0 = today's behaviour exactly: nothing sent, nothing applied)
static bool g_net_esynclog;   // live "set esynclog 1"

static struct { bool bound, active; EsCtx c; uint16_t seq; } NETES;

static void net_esync_bind(void) {
    if (NETES.bound) return;
    NETES.bound = true;
#ifndef NET_ESYNC_NO_EXE  // (host tests set NETES.c.io themselves)
    HMODULE exe = GetModuleHandleA(NULL);
    EsIface *io = &NETES.c.io;
    io->enum_enemies = (int (*)(uint32_t *, int))(void *)GetProcAddress(exe, "linkpos_esync_enum");
    io->read_state = (int (*)(uint32_t, EsState *))(void *)GetProcAddress(exe, "linkpos_esync_read");
    io->puppet_apply = (void (*)(uint32_t, const EsState *, int))(void *)GetProcAddress(exe, "linkpos_esync_puppet");
    io->set_brain = (void (*)(uint32_t, int))(void *)GetProcAddress(exe, "linkpos_esync_brain");
    io->take_events = (int (*)(EsEvent *, int))(void *)GetProcAddress(exe, "linkpos_esync_events");
    io->puppet_sensor = (void (*)(uint32_t, int, uint32_t, const uint32_t *))(void *)GetProcAddress(exe, "linkpos_esync_sensor");
#endif
    EsIface *io2 = &NETES.c.io;
    if (io2->enum_enemies && io2->read_state && io2->puppet_apply && io2->set_brain && io2->take_events && io2->puppet_sensor) LOG("[esync] exe exports found: enemy sync %s\n", g_net_esync ? "on" : "off (set esync 1)");
    else {
        LOG("[esync] this exe has no linkpos_esync_* exports (enum read puppet brain events sensor): enemy sync is OFF\n");
        memset(io2, 0, sizeof *io2);
    }
}
static bool net_esync_ready(void) { return NETES.c.io.enum_enemies != NULL; }

static void net_esync_stop(void) {
    if (NETES.active) { es_release_all(&NETES.c); NETES.active = false; LOG("[esync] stopped: every enemy brain is back on\n"); }
}

typedef struct { NetAddr d[NET_DESTS_MAX]; int nd; } NetEsDest;
static void net_esync_send(const uint8_t *b, int len, void *user) {
    NetEsDest *t = (NetEsDest *)user;
    for (int i = 0; i < t->nd; i++) net_send_to(t->d[i].ip, t->d[i].port, b, len);
}

// every net_tick (link_pos NULL = no world / loading)
static void net_esync_tick(const float link_pos[3], uint64_t now) {
    static bool env_done;  // s169: the launcher's SAVAGEWIND_LINK_ESYNC=1 (+ _LOG=1) = `set esync 1` (+ esynclog) at start; a live `set esync 0` still wins
    if (!env_done) {      // (the live file's first version does not run at game start: COPY2 stayed off in the s169 test)
        env_done = true;
        const char *e = getenv("SAVAGEWIND_LINK_ESYNC"), *l = getenv("SAVAGEWIND_LINK_ESYNC_LOG");
        if (e && atoi(e)) g_net_esync = true, LOG("[esync] on from the launcher (SAVAGEWIND_LINK_ESYNC=1)\n");
        if (l && atoi(l)) g_net_esynclog = true;
    }
    if (!g_net_esync || !NET.on) { net_esync_stop(); return; }
    net_esync_bind();
    if (!net_esync_ready()) return;
    EsCtx *c = &NETES.c;
    if (!NETES.active) NETES.active = true, c->t0 = now, memset(&c->st, 0, sizeof c->st), NETES.seq = (uint16_t)(now * 2654435761u >> 8);
    c->me = NET.me, c->log = g_net_esynclog;
    if (!link_pos || NET.loading || !NET.have_feet) return;  // (no world: nothing to run, nothing to send)
    EsPlayer pl[ES_PLAYERS];
    int np = 0;
    pl[np].id = NET.me, snprintf(pl[np].name, sizeof pl[np].name, "%s", NET.name), memcpy(pl[np].pos, NET.feet, sizeof NET.feet), np++;
    for (int k = 0; k < NET_MAX_PEERS && !g_net_loop && np < ES_PLAYERS; k++) {
        const NetPeerSlot *sl = &NET.slot[k];
        if (!sl->used || sl->p.st != PEER_LIVE || sl->p.n <= 0) continue;
        const NetSnap *s = &sl->p.ring[sl->p.n - 1];
        if (s->flags & NF_LOADING) continue;
        pl[np].id = sl->p.player, snprintf(pl[np].name, sizeof pl[np].name, "%s", sl->p.name), memcpy(pl[np].pos, s->feet, sizeof s->feet), np++;
    }
    es_set_players(c, pl, np);
    static EsOut out;
    uint64_t was = c->next_send;
    es_step(c, now, &out);
    {   // (s179 diag, esynclog: puppet frames applied vs skipped, every 10 s)
        static uint64_t next;
        static uint32_t l_ap, l_nl, l_ns, l_st;
        if (c->log && now >= next) {
            if (next) LOG("[esync] 10 s: steps %u, puppet frames applied %u, skipped not-in-enum %u, no fresh sample %u; puppet frame ms sum %.0f max %.1f (hid %08x action %u), over 30 ms %u\n", c->st.steps - l_st,
                          c->st.pup_apply - l_ap, c->st.pup_notlocal - l_nl, c->st.pup_nosample - l_ns, g_es_pup_ms_sum, g_es_pup_ms_max, g_es_pup_ms_hid, g_es_pup_ms_act, g_es_pup_ms_over30);
            g_es_pup_ms_sum = g_es_pup_ms_max = 0, g_es_pup_ms_over30 = 0;
            next = now + 10000, l_ap = c->st.pup_apply, l_nl = c->st.pup_notlocal, l_ns = c->st.pup_nosample, l_st = c->st.steps;
        }
    }
    bool gate = c->next_send != was;
    NetEsDest t;
    t.nd = g_net_loop ? 0 : net_dests(t.d, false);
    es_packets(c, &out, gate, NET.me, &NETES.seq, net_esync_send, &t);
}

// a NET_ESYNC packet from slot k (header already checked by net_unpack_ex)
static void net_esync_recv(int k, const uint8_t *buf, int n) {
    NetPeerSlot *sl = &NET.slot[k];
    EsCtx *c = &NETES.c;
    NetEsEnt e[NET_ESYNC_MAX];
    NetEsEv v[NET_ESYNC_EVMAX];
    int ne = 0, nv = 0;
    uint8_t pl = 0;
    uint16_t sq = 0;
    if (!net_unpack_esync(buf, n, &pl, &sq, e, &ne, v, &nv)) { c->st.rx_bad++; return; }
    if (!g_net_esync || !NETES.active) { c->st.rx_off++; return; }
    if (!net_esseq_fresh(&sl->es, sq)) { c->st.rx_dup++; return; }
    c->st.rx_pkt++, c->st.rx_bytes += (uint32_t)n;
    if (g_net_esynclog)
        for (int i = 0; i < ne; i++) ES_LOG("[esync] rx peer %d hid=%08x pos %.1f %.1f %.1f yaw %.2f (seq %u)\n", k + 1, e[i].hid, e[i].pos[0], e[i].pos[1], e[i].pos[2], e[i].yaw, (unsigned)sq);
    es_on_recv(c, sl->p.player, e, ne, v, nv, &sl->esev, GetTickCount64());
}

static void net_esync_diag(void) {
    const EsCtx *c = &NETES.c;
    int pup = 0, own = es_count_owned(c, &pup);
    uint64_t el = NETES.active ? (GetTickCount64() - c->t0) / 1000 : 0;
    LOG("[esync] (%s%s): tx %u packets / %u enemies / %u sensor events, %u B (%.1f packets/s); rx %u packets / %u enemies / %u events, %u B; owned %d, puppets %d; "
        "hand-overs %u (stale %u; claims %u done %u refused %u, yields to a claim %u, old-owner samples while claiming %u, claim samples sent %u); dropped: bad %u, repeat/old %u, off %u, not owner %u, conflict %u (yielded %u), table full %u, hid0 skipped %u, events dup %u / not owner %u / not loaded %u / queue %u; sensor-off forced %u\n",
        g_net_esync ? "on" : "OFF", NETES.bound && !net_esync_ready() ? ", NO EXE SUPPORT" : "", c->st.tx_pkt, c->st.tx_ent, c->st.tx_ev, c->st.tx_bytes, el ? (double)c->st.tx_pkt / (double)el : 0.0,
        c->st.rx_pkt, c->st.rx_ent, c->st.rx_ev, c->st.rx_bytes, own, pup, c->st.handovers, c->st.stale, c->st.claims, c->st.claim_done, c->st.claim_refused, c->st.claim_yield, c->st.rx_claimwait, c->st.tx_claim, c->st.rx_bad, c->st.rx_dup, c->st.rx_off, c->st.rx_notowner,
        c->st.rx_conflict, c->st.yielded, c->st.full, c->st.hid0_skip, c->st.rx_ev_dup, c->st.rx_ev_notowner, c->st.rx_ev_nolocal, c->st.ev_dropped, c->st.sensor_off);
}
#endif

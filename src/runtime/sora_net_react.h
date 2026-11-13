// s194 netreact: the KH2 hit reaction on an enemy ANOTHER game owns (recon/s194/S194_REACT_FACTS.md, GAP B). DLL only, no exe change.
//   Each enemy is simulated by one game (its ESYNC owner); the others show a puppet set from the owner's pose stream every frame.
//   Before this, Sora's ah_hit started a KH2 SLIDE / HOP / BLOW servo on the puppet and ah_tick pushed it while the puppet apply snapped
//   it back (the jitter), and the owner never played the reaction at all. Now (live `set netreact 1`):
//   - ATTACKER, the enemy is a puppet here (nr_puppet: the DLL esync table says another player owns it and its brain is frozen here):
//     no local ah_hit; the damage goes out exactly as before (EDMG), and one NET_REACT entry with what ah_hit would have used.
//   - OWNER, on a NET_REACT: if THIS game owns the enemy right now, has it loaded and alive, and the sender is a known peer on our map,
//     sora_airhold.h runs the same ah_hit with the packet's values (in the frame body, right before ah_tick); its pose stream does
//     the rest. Anything else is dropped and counted.
//   - A game that owns the enemy it hits: exactly as before (local ah_hit, nothing sent).
//   `set netreact 0` (compiled default 1 since s196, user verified) = the old behaviour: local ah_hit on puppets too, nothing
//   sent, received packets dropped. Log tag [react]: one line per send, one per receive with what was done; counters in `net diag`.
//   (s197 review S5) the receive lines are rate limited (NetLogRate, sora_net_proto.h: 8 per 2 s each, the rest counted in
//   `net diag`), and a received knockback is clamped into the atkp table span (-100..350, 0x7fff kept: net_unpack_react).
// Two parts: CORE (pure: decisions + queues, host-tested by runtime/test/react_test.c) and GLUE (#ifdef NET_REACT_GLUE, included by
// sora_net.h after the esync glue: it uses NET, NETES, net_dests, net_send_all, LOG; tested by runtime/test/net_glue_test.c).
// The wire format (NET_REACT, type 8) is in sora_net_proto.h.
#pragma once

// ===================== CORE =====================
// puppet = loaded here (in this step's enemy enum), owned by another player, its brain frozen by us (ESYNC applies the owner's pose).
// An enemy whose owner went quiet has its brain back on here (es_step): not a puppet, a local reaction is right for it.
// (s195b) owner == me wins whatever brain says: a hit claim made AT the hit (es_note_hit, before ah_hit asks this) makes us the owner
// first; it also tells the exe brain(1) at once (brain = 1 too), but owner != me alone already makes it "not a puppet".
static inline bool nr_puppet(bool esync_active, bool found, bool local, int owner, int me, int brain) {
    return esync_active && found && local && owner >= 0 && owner != me && brain == 0;
}
// this game runs the enemy right now: ESYNC on, the enemy in the table, loaded here, and the owner is us
static inline bool nr_owner_here(bool esync_active, bool found, bool local, int owner, int me) {
    return esync_active && found && local && owner == me;
}

// attacker: one Sora hit (ah_hit's arguments) -> what to do
enum { NRH_LOCAL = 0, NRH_SEND = 1, NRH_SKIP = 2 };  // SKIP: a puppet hit by a reaction ah_hit never plays (nothing local, nothing sent)
static inline int nr_hit_decide(bool netreact, bool puppet, int react) {
    if (!netreact || !puppet) return NRH_LOCAL;
    return net_react_plays(react) ? NRH_SEND : NRH_SKIP;
}

// owner, when the packet arrives: per entry
enum { NRR_QUEUE = 0, NRR_OFF, NRR_UNKNOWN, NRR_HIDDEN };
static inline int nr_rx_decide(bool netreact, bool peer_known, bool peer_hidden) {
    if (!netreact) return NRR_OFF;
    if (!peer_known) return NRR_UNKNOWN;
    if (peer_hidden) return NRR_HIDDEN;  // (s191 AREA: he is on another map; an hid is unique across maps, it cannot be ours anyway)
    return NRR_QUEUE;
}

// owner, in the frame body (where Sora's own hits run): per queued entry
#define NR_RX_TTL_MS 500  // a reaction not played this long after it arrived (menu / pause / loading) is dropped: its moment is gone
enum { NRP_PLAY = 0, NRP_OFF, NRP_LATE, NRP_NOT_OWNER, NRP_NOT_LOADED, NRP_DEAD };
static inline int nr_play_decide(bool netreact, uint64_t age_ms, bool owner_here, bool loaded, int life) {
    if (!netreact) return NRP_OFF;
    if (age_ms > NR_RX_TTL_MS) return NRP_LATE;
    if (!owner_here) return NRP_NOT_OWNER;
    if (!loaded) return NRP_NOT_LOADED;
    if (life <= 0) return NRP_DEAD;
    return NRP_PLAY;
}
static inline const char *nr_play_txt(int d) {
    return d == NRP_PLAY ? "played" : d == NRP_OFF ? "off (set netreact 0)" : d == NRP_LATE ? "late (not played in time: menu / pause / loading)"
         : d == NRP_NOT_OWNER ? "not owner" : d == NRP_NOT_LOADED ? "not loaded here" : "dead";
}

// (s195 reactidle) The frame body plays the queue (ah_net_play) and runs the servo (ah_tick) only on its DRIVING path; while Link has
// the pad (our Sora dead / on the Game Over screen, climbing, swimming, an event...) it returned before both, so every reaction sent
// to us expired as "late" (user test s194: 6 of 11 while the owner's Sora was dead) and a running slide / blow servo stopped (its
// controller override kept the last velocity). Live `set reactidle 1` (default 0 = exactly that old behaviour): the not-driving
// branch runs both too, when the world is there (a world frame + Link's body read), no load, BotW not paused.
enum { NRI_RUN = 0, NRI_OFF, NRI_NO_WORLD, NRI_LOADING, NRI_PAUSED };
static inline int nr_idle_decide(bool key, bool world, bool loading, bool paused) {
    if (!key) return NRI_OFF;
    if (!world) return NRI_NO_WORLD;
    if (loading) return NRI_LOADING;
    if (paused) return NRI_PAUSED;
    return NRI_RUN;
}
static inline const char *nr_idle_txt(int d) {
    return d == NRI_RUN ? "run" : d == NRI_OFF ? "off (set reactidle 0)" : d == NRI_NO_WORLD ? "no world" : d == NRI_LOADING ? "loading" : "paused";
}
// one log line the first time a reaction is played in a not-driving stretch (a driving frame ends the stretch). true = log it now
typedef struct { bool said; uint32_t played; } NrIdleLog;
static inline bool nr_idle_log_due(NrIdleLog *s, bool not_driving, int played) {
    if (!not_driving) { s->said = false, s->played = 0; return false; }
    if (played <= 0) return false;
    s->played += (uint32_t)played;
    if (s->said) return false;
    return s->said = true;
}

// one entry from ah_hit's arguments (clamped to the wire's ranges)
static inline NetReactEnt nr_ent_make(uint32_t hid, int react, int kb1, int kb2, int f54, const char *what, const float from[3], bool form) {
    NetReactEnt e;
    memset(&e, 0, sizeof e);
    e.hid = hid, e.react = (uint8_t)(react & 0xff), e.kind = (uint8_t)net_react_kind(what);
    e.kb1 = net_react_kb(kb1), e.kb2 = net_react_kb(kb2), e.f54 = (uint8_t)(f54 < 0 ? 0 : f54 > 255 ? 255 : f54);
    e.flags = form ? NRF_FORM : 0;
    for (int j = 0; j < 3; j++) e.from[j] = from[j];
    return e;
}

// outgoing entries waiting for the next pump (a full queue drops the oldest)
#define NR_TXQ 16
typedef struct { NetReactEnt q[NR_TXQ]; int n; uint32_t lost; } NrTxQ;
static inline void nr_txq_push(NrTxQ *t, const NetReactEnt *e) {
    if (t->n == NR_TXQ) memmove(t->q, t->q + 1, sizeof t->q[0] * (NR_TXQ - 1)), t->n--, t->lost++;
    t->q[t->n++] = *e;
}
// received entries waiting for the frame body (a full queue drops the oldest)
#define NR_RXQ 16
typedef struct { NetReactEnt e; uint8_t slot; uint64_t at; } NrRx;
typedef struct { NrRx q[NR_RXQ]; int n; uint32_t lost; } NrRxQ;
static inline void nr_rxq_push(NrRxQ *r, const NetReactEnt *e, int slot, uint64_t now) {
    if (r->n == NR_RXQ) memmove(r->q, r->q + 1, sizeof r->q[0] * (NR_RXQ - 1)), r->n--, r->lost++;
    NrRx *o = &r->q[r->n++];
    o->e = *e, o->slot = (uint8_t)slot, o->at = now;
}

#ifdef NET_REACT_GLUE
// ===================== GLUE (needs NET, NETES, net_dests, net_send_all, LOG from sora_net.h) =====================
static bool g_net_react = true;  // live "set netreact 0|1" (s196: default 1, user verified; 0 = the old behaviour exactly: local ah_hit on puppets too, nothing sent, nothing played)
_Static_assert(NET_REACT_PKT <= NET_EDMG_PKT, "a NET_REACT packet fits a NetEdOut");

static struct {
    NrTxQ tx;
    NetEdOut out[NET_EDMG_OUTQ]; int nout; uint16_t seq;
    NrRxQ rx;
    NetLogRate log_bad, log_dup, log_rx, log_play;  // (s197 review S5) the receive lines a peer's packets drive (sora_net_proto.h)
    struct { uint32_t hit_local, hit_send, hit_skip, tx_pkt, tx_send, tx_drop, tx_lost;
             uint32_t rx_pkt, rx_dup, rx_bad, rx_bad_ent, rx_ent, rx_off, rx_unknown, rx_hidden, played, not_owner, not_loaded, dead, late, play_off; } st;
} NETR;

// net on: empty queues, a fresh packet seq, counters from 0
static void net_react_reset(uint64_t now) {
    memset(&NETR, 0, sizeof NETR);
    NETR.seq = (uint16_t)(now * 2246822519u >> 7);
}
// net off: nothing of this session waits to go out or to be played
static void net_react_clear(void) { NETR.tx.n = 0, NETR.nout = 0, NETR.rx.n = 0; }

// the ESYNC side of an enemy here (hid 0 / not in the table: found false)
static bool net_react_es(uint32_t hid, bool *local, int *owner, int *brain) {
    const EsEnt *e = hid ? es_find(&NETES.c, hid) : NULL;
    *local = e && e->local, *owner = e ? e->owner : -1, *brain = e ? e->brain : -1;
    return e != NULL;
}
static bool net_react_esync_on(void) { return g_net_esync && NETES.active; }
// sora_airhold.h asks this first (cheap): only then is the actor's hid worth reading
static bool net_react_wanted(void) { return g_net_react && NET.on && net_react_esync_on(); }

// sora_airhold.h ah_hit, for every Sora hit (after the damage was queued as before). true = handled here (the enemy is a puppet:
// NO local reaction; sent when ah_hit would play one); false = run the local ah_hit as today.
static bool net_react_hit(uint32_t hid, const float from[3], int react, int kb1, int kb2, int f54, const char *what, bool form) {
    if (!g_net_react) return false;
    bool local = false;
    int owner = -1, brain = -1;
    bool found = net_react_es(hid, &local, &owner, &brain);
    int d = nr_hit_decide(true, nr_puppet(net_react_esync_on(), found, local, owner, NET.me, brain), react);
    if (d == NRH_LOCAL) { NETR.st.hit_local++; return false; }
    if (d == NRH_SKIP) { NETR.st.hit_skip++; return true; }
    NetReactEnt e = nr_ent_make(hid, react, kb1, kb2, f54, what, from, form);
    nr_txq_push(&NETR.tx, &e);
    NETR.st.hit_send++;
    LOG("[react] tx hid=%08x %s (%s) react %d kb %d/%d f54 %#x%s from %.1f %.1f %.1f: puppet here (owner player %d), no local reaction\n", hid,
        net_react_what(e.kind), what ? what : "?", e.react, e.kb1, e.kb2, e.f54, form ? " form" : "", from[0], from[1], from[2], owner);
    return true;
}

// every net_tick: the queued entries out as packets (up to 8 each), each NET_EVENT_REPEAT times over UDP, once in a Melty room
static void net_react_pump(uint64_t now) {
    NetAddr d[NET_DESTS_MAX];
    int nd = g_net_react && !g_net_loop ? net_dests(d, false) : 0;
    if (!nd) { NETR.st.tx_drop += (uint32_t)NETR.tx.n, NETR.tx.n = 0, NETR.nout = 0; return; }  // (off / loopback / alone: nothing to tell)
    for (int off = 0; off < NETR.tx.n; off += NET_REACT_MAX) {
        int k = NETR.tx.n - off < NET_REACT_MAX ? NETR.tx.n - off : NET_REACT_MAX;
        uint8_t b[NET_REACT_PKT];
        int len = net_pack_react(NET.me, ++NETR.seq, NETR.tx.q + off, k, b, (int)sizeof b);
        if (len <= 0) { NET.st.err++; continue; }
        NETR.st.tx_lost += (uint32_t)net_edout_push(NETR.out, &NETR.nout, b, len, now);
        if (NET.melty && NETR.nout) NETR.out[NETR.nout - 1].rep = 1;  // (a Melty room event is reliable: sent once, no repeats)
        NETR.st.tx_pkt++;
    }
    NETR.st.tx_lost += NETR.tx.lost, NETR.tx.lost = 0;
    NETR.tx.n = 0;
    if (NET.melty && !sora_melty_connected()) return;  // (sent once in a room: wait for the connection, as EDMG / FIRE)
    NetEdOut o;
    while (net_edout_due(NETR.out, &NETR.nout, now, &o)) {
        net_send_all(d, nd, o.b, o.len);
        NETR.st.tx_send++;
    }
}

// a NET_REACT packet from slot k (header already checked by net_unpack_ex)
static void net_react_recv(int k, const uint8_t *buf, int n, uint64_t now) {
    NetPeerSlot *sl = &NET.slot[k];
    NetReactEnt e[NET_REACT_MAX];
    uint8_t pl = 0;
    uint16_t sq = 0;
    int bad = 0, cnt = net_unpack_react(buf, n, &pl, &sq, e, &bad);
    NETR.st.rx_bad_ent += (uint32_t)bad;
    bool fq = false;
    if (cnt <= 0) {
        NETR.st.rx_bad++;
        if (g_net_react && net_lograte_ok(&NETR.log_bad, now, &fq)) LOG("[react] rx peer %d (seq %u) -> bad (%d bad entr%s)\n", k + 1, (unsigned)sq, bad, bad == 1 ? "y" : "ies");
        else if (fq) LOG("[react] (more bad packets in the next %d s are not logged: `net diag` counts them)\n", NET_LOGRATE_MS / 1000);
        return;
    }
    if (!net_edseq_fresh(&sl->rc, sq)) {
        NETR.st.rx_dup++;
        if (g_net_react && net_lograte_ok(&NETR.log_dup, now, &fq)) LOG("[react] rx peer %d (seq %u) -> dup (a repeat copy)\n", k + 1, (unsigned)sq);
        else if (fq) LOG("[react] (more repeat copies in the next %d s are not logged)\n", NET_LOGRATE_MS / 1000);
        return;
    }
    NETR.st.rx_pkt++;
    bool known = sl->used && sl->p.st != PEER_NONE;
    for (int i = 0; i < cnt; i++) {
        NETR.st.rx_ent++;
        int d = nr_rx_decide(g_net_react, known, sl->hidden);
        if (d == NRR_OFF) { NETR.st.rx_off++; continue; }  // (netreact 0: dropped quietly, as before this build)
        if (d == NRR_QUEUE) { nr_rxq_push(&NETR.rx, &e[i], k, now); continue; }
        if (d == NRR_HIDDEN) NETR.st.rx_hidden++;
        else NETR.st.rx_unknown++;
        if (net_lograte_ok(&NETR.log_rx, now, &fq))
            LOG("[react] rx peer %d hid=%08x %s react %d f54 %#x (seq %u) -> %s\n", k + 1, e[i].hid, net_react_what(e[i].kind), e[i].react, e[i].f54,
                (unsigned)sq, d == NRR_HIDDEN ? "hidden (he is on another map)" : "unknown peer");
        else if (fq) LOG("[react] (more dropped entries in the next %d s are not logged: `net diag` counts them)\n", NET_LOGRATE_MS / 1000);
    }
}

// the frame body (sora_airhold.h ah_net_play, right before ah_tick): every queued entry played or dropped.
//   find(hid, &actor, epos, &life, user) -> true = loaded here; play(actor, epos, entry, user) = the local ah_hit with its values
typedef bool (*NrFindFn)(uint32_t hid, uint64_t *actor, float epos[3], int32_t *life, void *user);
typedef void (*NrPlayFn)(uint64_t actor, const float epos[3], const NetReactEnt *e, void *user);
static int net_react_pending(void) { return NETR.rx.n; }
static int net_react_play(uint64_t now, NrFindFn find, NrPlayFn play, void *user) {
    int done = 0;
    for (int i = 0; i < NETR.rx.n; i++) {
        const NrRx *r = &NETR.rx.q[i];
        const NetReactEnt *e = &r->e;
        bool local = false;
        int owner = -1, brain = -1;
        bool found = net_react_es(e->hid, &local, &owner, &brain);
        bool here = nr_owner_here(net_react_esync_on(), found, local, owner, NET.me);
        uint64_t actor = 0;
        float epos[3] = {0, 0, 0};
        int32_t life = 0;
        bool loaded = here && find && find(e->hid, &actor, epos, &life, user);  // (no actor lookup unless we own it)
        int d = nr_play_decide(g_net_react, now > r->at ? now - r->at : 0, here, loaded, life);
        switch (d) {
        case NRP_PLAY: NETR.st.played++; break;
        case NRP_OFF: NETR.st.play_off++; break;
        case NRP_LATE: NETR.st.late++; break;
        case NRP_NOT_OWNER: NETR.st.not_owner++; break;
        case NRP_NOT_LOADED: NETR.st.not_loaded++; break;
        default: NETR.st.dead++; break;
        }
        bool fq = false;
        if (d != NRP_OFF && !net_lograte_ok(&NETR.log_play, now, &fq)) {
            if (fq) LOG("[react] (more received reactions in the next %d s are not logged: `net diag` counts them)\n", NET_LOGRATE_MS / 1000);
        } else if (d != NRP_OFF) {
            char ow[24];
            if (d == NRP_NOT_OWNER) snprintf(ow, sizeof ow, " (owner %d)", found ? owner : -1);
            else ow[0] = 0;
            LOG("[react] rx peer %d hid=%08x %s react %d kb %d/%d f54 %#x%s from %.1f %.1f %.1f -> %s%s\n", r->slot + 1, e->hid, net_react_what(e->kind), e->react,
                e->kb1, e->kb2, e->f54, e->flags & NRF_FORM ? " form" : "", e->from[0], e->from[1], e->from[2], nr_play_txt(d), ow);
        }
        if (d == NRP_PLAY && play) play(actor, epos, e, user), done++;
    }
    NETR.rx.n = 0;
    return done;
}

static void net_react_diag(void) {
    LOG("[react] (%s): hits on own / unsynced enemies %u (local), on puppets %u sent + %u skipped (no reaction to play); tx %u packets (sent %u, dropped %u: off / alone / "
        "loopback, lost %u: queue full); rx %u packets / %u entries (repeats skipped %u, bad packets %u, bad entries %u): played %u, not owner %u, not loaded %u, dead %u, "
        "late %u, other map %u, unknown peer %u, off %u (+%u queued then off), rx queue lost %u; log lines not written (rate limit) %u\n",
        g_net_react ? "on" : "OFF (set netreact 1)", NETR.st.hit_local, NETR.st.hit_send, NETR.st.hit_skip, NETR.st.tx_pkt, NETR.st.tx_send, NETR.st.tx_drop, NETR.st.tx_lost,
        NETR.st.rx_pkt, NETR.st.rx_ent, NETR.st.rx_dup, NETR.st.rx_bad, NETR.st.rx_bad_ent, NETR.st.played, NETR.st.not_owner, NETR.st.not_loaded, NETR.st.dead,
        NETR.st.late, NETR.st.rx_hidden, NETR.st.rx_unknown, NETR.st.rx_off, NETR.st.play_off, NETR.rx.lost,
        NETR.log_bad.quiet + NETR.log_dup.quiet + NETR.log_rx.quiet + NETR.log_play.quiet);
}
#endif

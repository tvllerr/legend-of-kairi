// sora_net_shots.h (s289 shots): a friend's arrows, Remote Bombs and thrown weapon fly in your world (`set netshots 1`), and
// what he holds with Magnesis / stopped with Stasis / raised with Cryonis is said to your game (`set netrunes 1`). User's wish
// (2026-11-26, "Arrows, bombs, runes"): "The friend's arrows, Remote Bombs and thrown weapons fly in your world. What he holds
// with Magnesis, freezes with Stasis or raises with Cryonis shows and moves for you."
// After Savage Wind PR #294 "Online mode" by byluki (fixes by mstampfli), MIT: its OnlineShots / rune state, sent as our own
// NET_SHOTS / NET_RUNES packets over our room (never its TCP server).
// Included by sora_net.h right after sora_net_gear.h (uses NET, net_dests, net_send_all, LOG, g_net_loop, sora_melty_connected).
// Pure parts: sora_net_shots_core.h; the wire: the end of sora_net_proto.h; the game side (which shots are his, the drawing):
// sora_net_shots_game.h (from sora_net_avatar.h).
//   ours     net_shots_local / net_runes_local (the game side, each sampled frame): his shots + booms / rune state now. Sent by
//            net_shots_tick (from net_tick): every NSHOT_MS while one moves or a boom is due, every NSHOT_SLOW_MS while all lie
//            still, NSHOT_EMPTY empty ones after the last; nothing while alone. A Melty room takes them as snapshots (the lanes of
//            sora_melty.cpp's ESYNC case): at most ONE NET_SHOTS and ONE NET_RUNES per 50 ms room tick = 2 of the 16 lanes.
//            A boom (a bomb that went off) rides in every NET_SHOTS for NSHOT_BOOM_MS and plays once by its seq.
//   theirs   net_shots_recv: per peer slot, the newest (net_esseq_fresh) onto NsPeer's tracks; the game side samples them
//            NET_INTERP_MS behind (as it does his puppet) and draws them.
//   keys     netshots 0|1 (default 1 since s290; 0 = as before: nothing sampled or sent, a received NET_SHOTS dropped and counted, nothing drawn),
//            netshotslog 0|1 (one line per shot taken / ended / sent boom / received boom), netshotstrace 0|1 (lab: per look one
//            line per followed arrow: actor, name, m from our feet, m/s, moved / away / still, on our bow, flies, alive; one line
//            per new arrow / bomb: taken or why not), netshotsmax N (models loaded for friends' shots this session, GUESS 6),
//            netshotsboom N (the boom's size in % of KH2's last Fire hit, 0 = no boom fx / sound; GUESS 200), netrunes 0|1
//            (default 1 since s290; 0: NET_RUNES not sampled, sent or kept), netshotsstuck N (s290, with netshots 1: his arrow that sticks
//            stays stuck in the friends' worlds until his game removes it, at most N s still; default NSHOT_STUCK_S 600 (s291; was 65 [GUESS:
//            BotW's StickTime 60 s + 5]: the s291 lab saw normal arrows stay 600+ s); 0 = as s289: gone at its hit / 3 s after it lies)
// Display only: places go out, pictures come back; no BotW actor, item, damage or save is made or written for them.
#pragma once
#include "sora_net_shots_core.h"

static int g_net_shots = 1;         // live "set netshots 0|1". (s290: passed the two-copy lab, sb/s290/LAB_RESULTS.md; the user: "Flip the passes ON" -> default ON)
static int g_net_shots_log;         // live "set netshotslog 1"
static int g_net_shots_trace;       // live "set netshotstrace 1" (lab trace, many lines a second)
static int g_net_shots_max = 6;     // live "set netshotsmax N" (GUESS: 3 arrows in flight + a bomb + a cube + one more kind)
static int g_net_shots_boom = 200;  // live "set netshotsboom N" (GUESS: a Remote Bomb's blast is about twice KH2's Fire hit)
static int g_net_runes = 1;         // live "set netrunes 0|1". (s290: passed the two-copy lab, sb/s290/LAB_RESULTS.md; the user: "Flip the passes ON" -> default ON)
static int g_net_shots_stuck = NSHOT_STUCK_S;  // live "set netshotsstuck N" (s290: s; 0 = as s289, see sora_net_shots_core.h)
static void (*g_nsg_diag)(void);    // (the game side's diag, set by sora_net_shots_game.h once it runs: no stub needed in the tests)
static void (*g_nsg_restart)(void); // (s292 coopfix: the game side's tracks dropped at a netshots change; set with g_nsg_diag)

#define NRUNE_MS 50        // rune state period while he holds / stops something or one of his pillars stands
#define NRUNE_FORGET_MS 1000  // a friend's rune state older than this is forgotten (quiet: left, crashed, key off there)

static struct {
    // ours (the game side writes them each sampled frame, the tick reads them)
    NetShot loc[NSHOT_MAX];
    NetBoom lboom[NSHOT_BOOMS];
    int nloc, nlboom;
    bool lmoving;
    uint64_t loc_at;
    NetRunes lrunes;
    uint64_t lrunes_at;
    // send
    uint16_t seq, rseq, boom_said;
    uint64_t last_tx, last_rtx;
    int empties, rempties;
    // theirs (a slot's record is wiped when the slot is someone else: its joined_ms changed)
    struct { NsPeer p; NetEsSeen seen, rseen; uint64_t joined, at; NetRunes runes; uint64_t runes_at; } peer[NET_MAX_PEERS];
    struct { uint32_t tx, tx_empty, tx_bad, rx, rx_dup, rx_bad, rx_off, rtx, rrx, rrx_dup, rrx_bad, rrx_off; } st;
} NETS;

static const char *net_shots_kind_txt(int k) { return k >= 0 && k < NSK_N ? NSK_NAME[k] : "?"; }

// the game side, each frame it samples (netshots 1, his world up): his shots and booms now; moving = one of them moves
static void net_shots_local(const NetShot *s, int n, const NetBoom *b, int nb, bool moving, uint64_t now) {
    n = n < 0 ? 0 : n > NSHOT_MAX ? NSHOT_MAX : n, nb = nb < 0 ? 0 : nb > NSHOT_BOOMS ? NSHOT_BOOMS : nb;
    if (n) memcpy(NETS.loc, s, sizeof NETS.loc[0] * (size_t)n);
    if (nb) memcpy(NETS.lboom, b, sizeof NETS.lboom[0] * (size_t)nb);
    NETS.nloc = n, NETS.nlboom = nb, NETS.lmoving = moving, NETS.loc_at = now ? now : 1;
}
static void net_runes_local(const NetRunes *r, uint64_t now) {
    if (!r) return;
    NETS.lrunes = *r, NETS.lrunes_at = now ? now : 1;
}

static bool net_shots_rec_ok(int k) {
    const NetPeerSlot *sl = &NET.slot[k];
    return sl->used && NETS.peer[k].joined == sl->joined_ms + 1;
}
static void net_shots_rec_fix(int k) {
    if (!net_shots_rec_ok(k)) memset(&NETS.peer[k], 0, sizeof NETS.peer[k]), NETS.peer[k].joined = NET.slot[k].joined_ms + 1;
}

// a NET_SHOTS packet of slot k (header checked by net_unpack_ex), also the loopback's own
static void net_shots_take(int k, const uint8_t *buf, int n, uint64_t now) {
    net_shots_rec_fix(k);
    NetShot s[NET_SHOTS_N];
    NetBoom b[NET_SHOTS_BOOMS];
    int ns = 0, nb = 0;
    uint8_t pl = 0;
    uint16_t sq = 0;
    if (!net_unpack_shots(buf, n, &pl, &sq, s, &ns, b, &nb)) { NETS.st.rx_bad++; return; }
    if (!net_esseq_fresh(&NETS.peer[k].seen, sq)) { NETS.st.rx_dup++; return; }  // (an older one that came late)
    NETS.st.rx++;
    NsPeer *P = &NETS.peer[k].p;
    uint32_t nt = P->st.new_tracks, nbm = P->st.booms;
    nsp_take(P, now, s, ns, b, nb);
    NETS.peer[k].at = now ? now : 1;
    if (g_net_shots_log && (P->st.new_tracks != nt || P->st.booms != nbm)) {
        for (int i = 0; i < ns; i++) {
            const NstTrack *t = nsp_track(P, s[i].id);
            if (t && t->ns == 1) LOG("[net] shots: peer %d's %s #%u at %.1f %.1f %.1f\n", k + 1, s[i].kind == NSK_THROWN ? s[i].name : net_shots_kind_txt(s[i].kind), s[i].id,
                                     s[i].pos[0], s[i].pos[1], s[i].pos[2]);
        }
        if (P->st.booms != nbm) LOG("[net] shots: peer %d's bomb went off (%u booms from him)\n", k + 1, P->st.booms);
    }
    if (g_net_shots_stuck && g_net_shots_log)  // (s290: his arrow lying still here, and its end)
        for (int i = 0; i < NST_TRACKS; i++) {
            NstTrack *t = &P->tr[i];
            if (!t->used || !nshot_kind_arrow(t->kind) || !t->still_at) continue;
            if (!t->stuck_said && !t->ended && t->ns) {
                t->stuck_said = true;
                const NstSample *z = &t->s[t->ns - 1];
                LOG("[net] shots: peer %d's %s #%u lies still here at %.1f %.1f %.1f: held until his game removes it\n", k + 1, t->name, t->id, z->p[0], z->p[1],
                    z->p[2]);
            } else if (t->stuck_said && t->ended && !t->end_said) {
                t->end_said = true;
                LOG("[net] shots: peer %d's %s #%u gone after %.1f s lying still: his game removed it\n", k + 1, t->name, t->id,
                    now >= t->still_at ? (double)(now - t->still_at) / 1000.0 : 0.0);
            }
        }
}

// (s290) each drawn frame (nsg_draw), before the tracks are sampled: (netshotsstuck N) his arrows still past N s +
// NSHOT_STUCK_SLACK_MS with no end from him hidden (nsp_cap), and the still ones whose packets stopped said; then nsp_reap (its
// result). netshotsstuck 0: nsp_reap only (as s289).
static int net_shots_reap(int k, uint64_t now, uint64_t at) {
    NsPeer *P = &NETS.peer[k].p;
    if (g_net_shots_stuck > 0) {
        uint16_t ids[NST_TRACKS];
        int nc = nsp_cap(P, now, (uint64_t)g_net_shots_stuck * 1000u + NSHOT_STUCK_SLACK_MS, ids, NST_TRACKS);
        for (int i = 0; i < nc && i < NST_TRACKS; i++) {
            NstTrack *t = nsp_track(P, ids[i]);
            if (t) t->end_said = true;
            if (g_net_shots_log)
                LOG("[net] shots: peer %d's %s #%u still %d s with no end from him: hidden here (netshotsstuck %d + %d s)\n", k + 1, t ? t->name : "arrow", ids[i],
                    g_net_shots_stuck + NSHOT_STUCK_SLACK_MS / 1000, g_net_shots_stuck, NSHOT_STUCK_SLACK_MS / 1000);
        }
        for (int i = 0; i < NST_TRACKS; i++) {
            NstTrack *t = &P->tr[i];
            if (t->used && !t->ended && t->stuck_said && !t->end_said && now > t->last_at && now - t->last_at > NSHOT_QUIET_MS) {
                t->end_said = true;
                if (g_net_shots_log) LOG("[net] shots: peer %d's %s #%u: his packets stopped (%d ms): hidden here\n", k + 1, t->name, t->id, NSHOT_QUIET_MS);
            }
        }
    }
    return nsp_reap(P, now, at);
}

// a NET_RUNES packet of slot k
static void net_runes_take(int k, const uint8_t *buf, int n, uint64_t now) {
    net_shots_rec_fix(k);
    NetRunes r;
    uint8_t pl = 0;
    uint16_t sq = 0;
    if (!net_unpack_runes(buf, n, &pl, &sq, &r)) { NETS.st.rrx_bad++; return; }
    if (!net_esseq_fresh(&NETS.peer[k].rseen, sq)) { NETS.st.rrx_dup++; return; }
    NETS.st.rrx++;
    NetRunes *was = &NETS.peer[k].runes;
    bool fresh = NETS.peer[k].runes_at && now - NETS.peer[k].runes_at <= NRUNE_FORGET_MS;
    if (g_net_shots_log && (!fresh || was->flags != r.flags || was->nice != r.nice || was->mag.hashid != r.mag.hashid || was->stop.hashid != r.stop.hashid))
        LOG("[net] runes: peer %d %s (hashid %#x)%s, %d Cryonis pillar(s)\n", k + 1, (r.flags & NRF_MAG) ? "holds an object with Magnesis" : "Magnesis free",
            r.mag.hashid, (r.flags & NRF_STOP) ? ", stopped one with Stasis" : "", r.nice);
    *was = r, NETS.peer[k].runes_at = now ? now : 1;
}

static void net_shots_recv(int k, int type, const uint8_t *buf, int n, uint64_t now) {
    if (type == NET_RUNES) {
        if (!g_net_runes) { NETS.st.rrx_off++; return; }  // (key 0: dropped = as before)
        net_runes_take(k, buf, n, now);
        return;
    }
    if (!g_net_shots) { NETS.st.rx_off++; return; }
    net_shots_take(k, buf, n, now);
}

// slot k's newest rune state (fresh), NULL = none / not known / key 0
static const NetRunes *net_runes_of(int k, uint64_t now) {
    if (!g_net_runes || k < 0 || k >= NET_MAX_PEERS || !net_shots_rec_ok(k) || !NETS.peer[k].runes_at || now < NETS.peer[k].runes_at ||
        now - NETS.peer[k].runes_at > NRUNE_FORGET_MS)
        return NULL;
    return &NETS.peer[k].runes;
}
// (for the objs helper) a friend holds this map object (its HashId, actor +0x640) with Magnesis or stopped it with Stasis now:
// his slot + 1; 0 = no friend does (none says so, netrunes 0, hashid 0). Two friends on one object: the first slot.
static int netshots_friend_holds(uint32_t map_hashid) {
    uint64_t now = GetTickCount64();
    for (int k = 0; map_hashid && k < NET_MAX_PEERS; k++)
        if (nrune_holds(net_runes_of(k, now), map_hashid)) return k + 1;
    return 0;
}

static bool net_shots_anyone(NetAddr *d, int *nd) {
    *nd = g_net_loop ? 0 : net_dests(d, false);
    return g_net_loop ? NET.slot[0].used : NET.melty ? netslot_count(NET.slot, NET_MAX_PEERS) > 0 : *nd > 0;
}

// every net_tick (after net_gear_tick): our shots / runes out when due
static void net_shots_tick(uint64_t now) {
    if (!g_net_shots && !g_net_runes) return;
    if (NET.melty && !sora_melty_connected() && !g_net_loop) return;
    NetAddr d[NET_DESTS_MAX];
    int nd = 0;
    if (g_net_shots) {
        bool fresh = NETS.loc_at && now >= NETS.loc_at && now - NETS.loc_at <= NSHOT_LOCAL_MS;
        int n = fresh ? NETS.nloc : 0, nb = fresh ? NETS.nlboom : 0;
        if (nshot_due(n + nb > 0, (n && NETS.lmoving) || nb > 0, NETS.empties, now, NETS.last_tx)) {
            if (!net_shots_anyone(d, &nd)) NETS.empties = 0;  // (alone: nothing to say, nothing owed)
            else {
                uint8_t b[NET_SHOTS_PKT];
                int len = net_pack_shots(NET.me, ++NETS.seq, NETS.loc, n, NETS.lboom, nb, b, (int)sizeof b);
                if (len <= 0) NETS.st.tx_bad++, NETS.nloc = NETS.nlboom = 0;  // (cannot happen: the game side fills them; dropped, not sent)
                else {
                    if (g_net_loop) net_shots_take(0, b, len, now);  // (loopback: slot 0 is ourselves, as a remote would get it)
                    else net_send_all(d, nd, b, len);
                    NETS.st.tx++, NETS.last_tx = now;
                    if (n + nb) NETS.empties = NSHOT_EMPTY;
                    else NETS.st.tx_empty++, NETS.empties--;
                    for (int i = nb - 1; i >= 0 && g_net_shots_log; i--)  // (oldest first: lboom[0] is the newest)
                        if ((uint16_t)(NETS.lboom[i].seq - NETS.boom_said) < 0x8000 && NETS.lboom[i].seq != NETS.boom_said)
                            NETS.boom_said = NETS.lboom[i].seq,
                            LOG("[net] shots: our %s went off at %.1f %.1f %.1f: boom #%u sent (%s)\n", net_shots_kind_txt(NETS.lboom[i].kind), NETS.lboom[i].pos[0],
                                NETS.lboom[i].pos[1], NETS.lboom[i].pos[2], NETS.lboom[i].seq, g_net_loop ? "loopback" : NET.melty ? "Melty room" : "UDP");
                }
            }
        }
    }
    if (g_net_runes) {
        bool fresh = NETS.lrunes_at && now >= NETS.lrunes_at && now - NETS.lrunes_at <= NSHOT_LOCAL_MS;
        NetRunes r;
        if (fresh) r = NETS.lrunes;
        else memset(&r, 0, sizeof r);
        bool state = r.flags || r.nice;
        if (nshot_due(state, state, NETS.rempties, now, NETS.last_rtx)) {
            if (!net_shots_anyone(d, &nd)) NETS.rempties = 0;
            else {
                uint8_t b[NET_RUNES_PKT];
                int len = net_pack_runes(NET.me, ++NETS.rseq, &r, b, (int)sizeof b);
                if (len > 0) {
                    if (g_net_loop) net_runes_take(0, b, len, now);
                    else net_send_all(d, nd, b, len);
                    NETS.st.rtx++, NETS.last_rtx = now;
                    NETS.rempties = state ? NSHOT_EMPTY : NETS.rempties - 1;
                }
            }
        }
    }
}

// net on: fresh seqs, nothing owed, no peer's shots kept (ours stay: the game side refills them anyway)
static void net_shots_reset(uint64_t now) {
    memset(NETS.peer, 0, sizeof NETS.peer);
    memset(&NETS.st, 0, sizeof NETS.st);
    NETS.seq = (uint16_t)(now * 2654435761u >> 9), NETS.rseq = (uint16_t)(now * 40503u >> 3);
    NETS.last_tx = NETS.last_rtx = 0, NETS.empties = NETS.rempties = 0;
}

static void net_shots_diag(void) {
    uint64_t now = GetTickCount64();
    LOG("[net] shots %s, runes %s: ours %d shot(s) %d boom(s) (%s); sent %u (empty %u, bad %u), received %u (late %u, bad %u, dropped with netshots 0 %u); runes sent %u, "
        "received %u (late %u, bad %u, dropped with netrunes 0 %u)\n",
        g_net_shots ? "on" : "OFF (set netshots 1)", g_net_runes ? "on" : "OFF (set netrunes 1)", NETS.nloc, NETS.nlboom,
        NETS.loc_at && now - NETS.loc_at <= NSHOT_LOCAL_MS ? "fresh" : "stale / none", NETS.st.tx, NETS.st.tx_empty, NETS.st.tx_bad, NETS.st.rx, NETS.st.rx_dup,
        NETS.st.rx_bad, NETS.st.rx_off, NETS.st.rtx, NETS.st.rrx, NETS.st.rrx_dup, NETS.st.rrx_bad, NETS.st.rrx_off);
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        if (!NET.slot[k].used || !net_shots_rec_ok(k)) continue;
        const NsPeer *P = &NETS.peer[k].p;
        int live = 0;
        for (int i = 0; i < NST_TRACKS; i++) live += P->tr[i].used;
        const NetRunes *r = net_runes_of(k, now);
        LOG("[net]   slot %d \"%s\": %d track(s) now, %u new, %u ended, %u full; booms %u (repeats %u); runes %s\n", k + 1, NET.slot[k].p.name, live, P->st.new_tracks,
            P->st.ended, P->st.full, P->st.booms, P->st.boom_dups,
            !r ? "none" : (r->flags & NRF_MAG) ? "Magnesis on an object" : (r->flags & NRF_STOP) ? "Stasis on an object" : r->nice ? "Cryonis pillars" : "nothing held");
        if (g_net_shots_stuck) {  // (s290)
            int still = 0;
            for (int i = 0; i < NST_TRACKS; i++) still += P->tr[i].used && !P->tr[i].ended && P->tr[i].still_at && nshot_kind_arrow(P->tr[i].kind);
            LOG("[net]   slot %d: %d arrow(s) of his lie still here now; hidden by the cap %u (netshotsstuck %d s)\n", k + 1, still, P->st.capped, g_net_shots_stuck);
        }
    }
    if (g_nsg_diag) g_nsg_diag();
}

// live keys (one line when a value changes: a live rewrite re-applies every line)
static bool net_shots_live_set(const char *key, int v) {
    if (!strcmp(key, "netshots")) {
        if (g_net_shots == (v != 0)) return true;
        g_net_shots = v != 0;
        for (int k = 0; k < NET_MAX_PEERS; k++) memset(&NETS.peer[k].p, 0, sizeof NETS.peer[k].p), memset(&NETS.peer[k].seen, 0, sizeof NETS.peer[k].seen);
        NETS.nloc = NETS.nlboom = 0, NETS.loc_at = 0, NETS.empties = 0, NETS.last_tx = 0;
        if (g_nsg_restart) g_nsg_restart();  // (s292 coopfix: our own tracks go too: not looked at while 0, stale at 0 -> 1)
        LOG("[net] shots %s\n", g_net_shots ? "on: our arrows / Remote Bombs / thrown weapon go to the other games, theirs fly here (needs data/gear shots models)"
                                            : "off (set netshots 0: nothing sent, friends' shots not drawn: as before)");
    } else if (!strcmp(key, "netshotslog")) {
        if (g_net_shots_log == (v != 0)) return true;
        g_net_shots_log = v != 0;
        LOG("[net] shots log %s\n", g_net_shots_log ? "on (one line per shot taken / ended / boom)" : "off");
    } else if (!strcmp(key, "netshotstrace")) {
        if (g_net_shots_trace == (v != 0)) return true;
        g_net_shots_trace = v != 0;
        LOG("[net] shots trace %s\n", g_net_shots_trace ? "on (lab: a line per followed arrow each frame, a line per new arrow / bomb)" : "off");
    } else if (!strcmp(key, "netshotsmax")) {
        int m = v < 0 ? 0 : v > 16 ? 16 : v;
        if (g_net_shots_max == m) return true;
        g_net_shots_max = m;
        LOG("[net] shots: at most %d shot models loaded for friends this session\n", m);
    } else if (!strcmp(key, "netshotsboom")) {
        int m = v < 0 ? 0 : v > 1000 ? 1000 : v;
        if (g_net_shots_boom == m) return true;
        g_net_shots_boom = m;
        LOG("[net] shots: a friend's Remote Bomb goes off here at %d%% of KH2's last Fire hit%s\n", m, m ? "" : " (0: no boom effect or sound)");
    } else if (!strcmp(key, "netshotsstuck")) {  // (s290)
        int m = v < 0 ? 0 : v > 600 ? 600 : v;
        if (g_net_shots_stuck == m) return true;
        g_net_shots_stuck = m;
        if (m) LOG("[net] shots: an arrow that sticks stays stuck in the other games until its shooter's game removes it (at most %d s still)\n", m);
        else LOG("[net] shots: netshotsstuck 0: a stuck arrow goes at its hit / 3 s after it lies (as s289)\n");
    } else if (!strcmp(key, "netrunes")) {
        if (g_net_runes == (v != 0)) return true;
        g_net_runes = v != 0;
        for (int k = 0; k < NET_MAX_PEERS; k++) NETS.peer[k].runes_at = 0, memset(&NETS.peer[k].rseen, 0, sizeof NETS.peer[k].rseen);
        NETS.lrunes_at = 0, NETS.rempties = 0, NETS.last_rtx = 0;
        LOG("[net] runes %s\n", g_net_runes ? "on: our Magnesis / Stasis object and Cryonis pillars go to the other games; theirs: pillars drawn here, what they hold is theirs for netobjs (no glow / beam / chains yet)"
                                            : "off (set netrunes 0: nothing sent, a friend's rune state dropped: as before)");
    } else return false;
    return true;
}

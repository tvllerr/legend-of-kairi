// s176 fire sync (DLL side, no exe change). Included by sora_net.h right after sora_net_edmg.h (it uses NET, net_dests, net_send_to).
//   A Fire tick cast on THIS game (sora.c fire_world, Sora or Donald) is queued by net_fire_note(); net_fire_pump() sends the entry as a
//   NET_FIRE packet (up to 4 per packet) NET_EVENT_REPEAT times NET_EDMG_REP_MS apart to every peer. On the other games net_fire_recv()
//   de-dupes by (peer, seq) and calls fire_world_remote() (sora.c): fire objects in reach ignite, flint sparks, last-tick bursts.
//   Enemies are NOT ignited on the receiver unless `set netfireen 1` (their HP is already shared over NET_EDMG).
//   live: `set netfire 0|1` (default 1), `set netfireen 0|1` (default 0), `set netfirelog 0|1` (default 1).
#pragma once

static bool g_net_fire = true, g_net_fire_en = false, g_net_fire_log = true, g_net_fire_espk = true;
// sora.c: the receiver's half of fire_world. -> 0 done (res = lit, sparks, bursts), -1 no world here (loading / menu / paused / title)
// or fireworld 0 here, -2 the exe has no fire functions
static int fire_world_remote(const float c[3], bool last, bool burst, float r, float h, int who, int res[3]);
#define NET_FIRE_RX_FRAME 8  // (s176 review: received entries applied per frame; the rest = capped, e.g. a socket full after a load)

#define NET_FIRE_QMAX 16
static struct {
    NetFireEnt q[NET_FIRE_QMAX]; int nq;          // entries waiting for the next pump
    NetEdOut out[NET_EDMG_OUTQ]; int nout; uint16_t seq;
    int rx_left;  // (this frame's receive budget, refilled by net_fire_pump after net_recv_all)
    struct { uint32_t tx_ent, tx_pkt, tx_send, tx_drop, tx_lost, rx_pkt, rx_dup, rx_bad, rx_ent, rx_lit, rx_spk, rx_bst, rx_off, rx_noworld, rx_nofn, rx_cap, rx_area; } st;  // (s191: rx_area = from a player on another map)
} NETF = {.rx_left = NET_FIRE_RX_FRAME};

static void net_fire_reset(uint64_t now) {
    NETF.nq = NETF.nout = 0, NETF.seq = (uint16_t)(now * 40503u >> 4), NETF.rx_left = NET_FIRE_RX_FRAME;
    memset(&NETF.st, 0, sizeof NETF.st);
}

// sora.c fire_world (a LOCAL cast): c = the caster's feet, r / h in u, who 0 sora / 1 donald / 2 other. Cheap no-op while net is off.
static void net_fire_note(const float c[3], bool last, bool burst, float r, float h, int who) {
    if (!NET.on || !g_net_fire || g_net_loop) return;
    if (NETF.nq == NET_FIRE_QMAX) { memmove(NETF.q, NETF.q + 1, sizeof NETF.q[0] * (NET_FIRE_QMAX - 1)), NETF.nq--, NETF.st.tx_lost++; }
    NetFireEnt *e = &NETF.q[NETF.nq++];
    e->pos[0] = c[0], e->pos[1] = c[1], e->pos[2] = c[2], e->r = net_fire_q(r), e->h = net_fire_q(h);
    e->flags = (uint8_t)((last ? NFRF_LAST : 0) | (burst ? NFRF_BURST : 0) | ((who < 0 || who > 2 ? 2 : who) << NFRF_WHO_SHIFT));
    if (g_net_fire_log)
        LOG("[net] fire tx (%s%s) at %.1f %.1f %.1f r %.2f h %.2f\n", who == 0 ? "sora" : who == 1 ? "donald" : "other", last ? ", last" : "", c[0], c[1], c[2], r, h);
}

static void net_fire_pump(uint64_t now) {
    NetAddr d[NET_DESTS_MAX];
    NETF.rx_left = NET_FIRE_RX_FRAME;  // (net_tick: after net_recv_all -> next frame's budget)
    int nd = g_net_fire && !g_net_loop ? net_dests(d, false) : 0;
    if (!nd) { NETF.st.tx_drop += (uint32_t)NETF.nq, NETF.nq = 0, NETF.nout = 0; return; }  // (off / loopback / alone: nothing is shared)
    for (int off = 0; off < NETF.nq; off += NET_FIRE_MAX) {
        int k = NETF.nq - off < NET_FIRE_MAX ? NETF.nq - off : NET_FIRE_MAX;
        uint8_t b[NET_FIRE_PKT];
        int len = net_pack_fire(NET.me, ++NETF.seq, NETF.q + off, k, b, (int)sizeof b);
        if (len <= 0) { NET.st.err++; continue; }
        NETF.st.tx_lost += (uint32_t)net_edout_push(NETF.out, &NETF.nout, b, len, now);
        if (NET.melty && NETF.nout) NETF.out[NETF.nout - 1].rep = 1;  // (s187: a Melty room event is reliable: sent once, no repeats)
        NETF.st.tx_pkt++, NETF.st.tx_ent += (uint32_t)k;
    }
    NETF.nq = 0;
    NetEdOut o;
    if (NET.melty && !sora_melty_connected()) return;  // (s189 review N1: sent once in a room, so wait for the connection)
    while (net_edout_due(NETF.out, &NETF.nout, now, &o)) {
        net_send_all(d, nd, o.b, o.len);
        NETF.st.tx_send++;
    }
}

// a NET_FIRE packet from slot k (header already checked by net_unpack_ex)
static void net_fire_recv(int k, const uint8_t *buf, int n) {
    NetPeerSlot *sl = &NET.slot[k];
    NetFireEnt e[NET_FIRE_MAX];
    uint8_t pl = 0;
    uint16_t sq = 0;
    int cnt = net_unpack_fire(buf, n, &pl, &sq, e);
    if (cnt <= 0) { NETF.st.rx_bad++; return; }
    if (!net_edseq_fresh(&sl->fi, sq)) { NETF.st.rx_dup++; return; }  // (one of the NET_EVENT_REPEAT copies)
    NETF.st.rx_pkt++;
    if (sl->hidden) { NETF.st.rx_ent += (uint32_t)cnt, NETF.st.rx_area += (uint32_t)cnt; return; }  // (s191: cast on another map: its place is not ours)
    for (int i = 0; i < cnt; i++) {
        NETF.st.rx_ent++;
        if (!g_net_fire) { NETF.st.rx_off++; continue; }
        if (NETF.rx_left <= 0) { NETF.st.rx_cap++; continue; }  // (s176 review: at most NET_FIRE_RX_FRAME per frame)
        NETF.rx_left--;
        int who = e[i].flags >> NFRF_WHO_SHIFT & NFRF_WHO_MASK, res[3] = {0, 0, 0};
        bool last = (e[i].flags & NFRF_LAST) != 0, burst = (e[i].flags & NFRF_BURST) != 0;
        int rc = fire_world_remote(e[i].pos, last, burst, e[i].r / 100.0f, e[i].h / 100.0f, who, res);
        if (rc == -1) NETF.st.rx_noworld++;
        else if (rc == -2) NETF.st.rx_nofn++;
        else NETF.st.rx_lit += (uint32_t)res[0], NETF.st.rx_spk += (uint32_t)res[1], NETF.st.rx_bst += (uint32_t)res[2];
        if (g_net_fire_log) {
            if (rc == 0)
                LOG("[net] fire rx peer %d (%s%s) at %.1f %.1f %.1f r %.2f h %.2f (seq %u) -> lit %d, sparks %d, bursts %d\n", k + 1,
                    who == 0 ? "sora" : who == 1 ? "donald" : "other", last ? ", last" : "", e[i].pos[0], e[i].pos[1], e[i].pos[2], e[i].r / 100.0f,
                    e[i].h / 100.0f, (unsigned)sq, res[0], res[1], res[2]);
            else
                LOG("[net] fire rx peer %d (seq %u) -> ignored (%s)\n", k + 1, (unsigned)sq, rc == -1 ? "no world here (loading / menu / paused) or fireworld 0 here" : "this exe has no fire functions");
        }
    }
}

static void net_fire_diag(void) {
    LOG("[net] fire (%s, enemies on the receiver %s): tx %u entries / %u packets (sent %u, dropped %u: off / alone / loopback, lost %u: queue full); "
        "rx %u packets / %u entries (repeats skipped %u, bad %u): lit %u, sparks %u, bursts %u, off %u, no world %u, no exe fn %u, over the per-frame cap %u, other map %u\n",
        g_net_fire ? "on" : "OFF", g_net_fire_en ? "ignited" : g_net_fire_espk ? "sparks only" : "left alone", NETF.st.tx_ent, NETF.st.tx_pkt, NETF.st.tx_send, NETF.st.tx_drop,
        NETF.st.tx_lost, NETF.st.rx_pkt, NETF.st.rx_ent, NETF.st.rx_dup, NETF.st.rx_bad, NETF.st.rx_lit, NETF.st.rx_spk, NETF.st.rx_bst,
        NETF.st.rx_off, NETF.st.rx_noworld, NETF.st.rx_nofn, NETF.st.rx_cap, NETF.st.rx_area);
}

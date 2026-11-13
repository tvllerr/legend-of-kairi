// s166 E2 (shared enemy HP, DLL side). Included by sora_net.h right after net_dests() (it uses NET, net_dests, net_send_to).
// docs/specs/2026-11-07-multiplayer-shared-enemy-hp-design.md sec 2 items 6-7, recon/S166_HELPER_EDMG_E2.md.
//   - each tick (net_edmg_pump): drain the exe's capture ring (linkpos_edmg_take) into NET_EDMG packets (up to 16 hits each,
//     own u16 seq), queue them, send each NET_EVENT_REPEAT times NET_EDMG_REP_MS apart to every peer of the mesh;
//   - on a NET_EDMG packet (net_edmg_recv): de-dupe by (peer, seq) and hand every hit to linkpos_edmg_apply.
//   Both exports are looked up with GetProcAddress: an exe without them (or without SAVAGEWIND_LINK_EDMG=1: apply returns -1,
//   take stays empty) = the feature is off, logged once.
//   live: `set edmg 0|1` (nothing sent / received hits ignored; the exe ring is still drained), `set edmglog 0|1`.
#pragma once

// (s196 killsync, sora_net_kills.h, included after this file) every kill seen here goes on the kill list (killsync 0 = nothing):
// our own (name = the exe's actor name: Enemy_* only), and peer slot k's EDMG kill with what its apply returned (-1 / no exe fn = waiting)
static void net_kills_own(uint32_t hid, const char *name, uint64_t now);
static void net_kills_edmg(int k, uint32_t hid, int apply_ret, uint64_t now);

typedef struct { uint64_t actor, frame; uint32_t hid; int32_t dealt, before, after, f54, f40, kill; char name[48]; } NetExEdmg;  // = linkpos.c LinkposEdmg
_Static_assert(sizeof(NetExEdmg) == 96, "linkpos.c LinkposEdmg is 96 bytes");

static struct {
    bool bound, size_logged;
    int (*take)(NetExEdmg *, int, int);                    // linkpos_edmg_take(out, max, sizeof)
    int (*apply)(uint32_t, int, int, int, int, int);       // linkpos_edmg_apply(hid, dealt, after, f54, f40, kill) -> 1 queued, 2 pending, 0 ignored, -1 off
    int (*stats)(uint64_t *, int);                         // linkpos_edmg_stats(out, n)
    int (*reset)(int *);                                   // (s197 S1, kill sync's Blood Moon clear) linkpos_edmg_reset(&kills): the pending map, queue, echoes go
    NetEdOut q[NET_EDMG_OUTQ]; int nq; uint16_t seq;
    struct { uint32_t tx_pkt, tx_ent, tx_send, tx_drop, tx_lost, rx_pkt, rx_dup, rx_bad, rx_ent, rx_queued, rx_pending, rx_ignored, rx_off, rx_nofn; } st;
} NETED;

static void net_edmg_bind(void) {
    if (NETED.bound) return;
    NETED.bound = true;
#ifndef NET_EDMG_NO_EXE  // (host tests set the function pointers themselves)
    HMODULE exe = GetModuleHandleA(NULL);
    NETED.take = (int (*)(NetExEdmg *, int, int))(void *)GetProcAddress(exe, "linkpos_edmg_take");
    NETED.apply = (int (*)(uint32_t, int, int, int, int, int))(void *)GetProcAddress(exe, "linkpos_edmg_apply");
    NETED.stats = (int (*)(uint64_t *, int))(void *)GetProcAddress(exe, "linkpos_edmg_stats");
    NETED.reset = (int (*)(int *))(void *)GetProcAddress(exe, "linkpos_edmg_reset");  // (s180 export, exe 22:21+; sora_levels.h calls it on a load)
#endif
    if (NETED.take && NETED.apply) LOG("[net] edmg: exe exports found (take, apply%s): shared enemy HP %s (the exe also needs SAVAGEWIND_LINK_EDMG=1)\n",
                                       NETED.stats ? ", stats" : "", g_net_edmg ? "on" : "off (set edmg 1)");
    else LOG("[net] edmg: this exe has no linkpos_edmg_%s: shared enemy HP is OFF (install the exe of this build)\n", NETED.take ? "apply" : "take");
    if (!NETED.apply) NETED.take = NULL;  // (s167 review SF-5: an E1 exe has take but no apply: truly off, or our hits go one way)
}

// net on: no hits of an earlier session in the ring or the repeat queue, a fresh packet seq
static void net_edmg_reset(uint64_t now) {
    NETED.nq = 0, NETED.seq = (uint16_t)(now * 2654435761u >> 8);
    memset(&NETED.st, 0, sizeof NETED.st);
    net_edmg_bind();
    NetExEdmg buf[NET_EDMG_MAX];
    for (int i = 0; NETED.take && i < 16; i++) if (NETED.take(buf, NET_EDMG_MAX, (int)sizeof buf[0]) <= 0) break;  // (hits from before)
}

static void net_edmg_pump(uint64_t now) {
    net_edmg_bind();
    if (!NETED.take) return;
    NetAddr d[NET_DESTS_MAX];
    int nd = g_net_edmg && !g_net_loop ? net_dests(d, false) : 0;
    NetExEdmg buf[NET_EDMG_MAX];
    for (int round = 0; round < 4; round++) {  // (the exe ring holds 32 = two packets)
        int n = NETED.take(buf, NET_EDMG_MAX, (int)sizeof buf[0]);
        if (n < 0) {
            if (!NETED.size_logged) NETED.size_logged = true, LOG("[net] edmg: linkpos_edmg_take refused the struct size %d (exe / DLL mismatch): shared enemy HP off\n", (int)sizeof buf[0]);
            NETED.take = NULL;
            return;
        }
        if (n == 0) break;
        for (int i = 0; i < n && g_net_edmg; i++)  // (s196 killsync: our own kills, also while alone = not sent)
            if (buf[i].kill && buf[i].hid) net_kills_own(buf[i].hid, buf[i].name, now);
        if (!nd) { NETED.st.tx_drop += (uint32_t)n; continue; }  // (off, loopback or alone: these hits are not shared)
        NetEdEnt e[NET_EDMG_MAX];
        int k = 0;
        for (int i = 0; i < n; i++) {
            if (!buf[i].hid || buf[i].dealt <= 0) continue;  // (hid 0 = a spawned enemy: stays local)
            NetEdEnt *o = &e[k++];
            o->hid = buf[i].hid, o->dealt = (uint16_t)(buf[i].dealt > 65535 ? 65535 : buf[i].dealt);
            o->after = (uint16_t)(buf[i].after < 0 ? 0 : buf[i].after > 65535 ? 65535 : buf[i].after);
            o->f54 = (uint8_t)(buf[i].f54 < 0 ? 0 : buf[i].f54 > 255 ? 255 : buf[i].f54);
            o->f40 = (uint8_t)(buf[i].f40 < 0 ? 0 : buf[i].f40 > 255 ? 255 : buf[i].f40);
            o->flags = buf[i].kill ? NEDF_KILL : 0;
            if (g_net_edmg_log)
                LOG("[net] edmg tx hid=%08x %s dealt=%d after=%d kill=%d f54=%d f40=%d (seq %u)\n", buf[i].hid, buf[i].name, buf[i].dealt, buf[i].after,
                    buf[i].kill, buf[i].f54, buf[i].f40, (unsigned)(uint16_t)(NETED.seq + 1));
        }
        if (!k) continue;
        uint8_t b[NET_EDMG_PKT];
        int len = net_pack_edmg(NET.me, ++NETED.seq, e, k, b, (int)sizeof b);
        if (len <= 0) { NET.st.err++; continue; }
        NETED.st.tx_lost += (uint32_t)net_edout_push(NETED.q, &NETED.nq, b, len, now);
        if (NET.melty && NETED.nq) NETED.q[NETED.nq - 1].rep = 1;  // (s187: a Melty room event is reliable: sent once, no repeats)
        NETED.st.tx_pkt++, NETED.st.tx_ent += (uint32_t)k;
    }
    NetEdOut o;
    if (!nd) { NETED.nq = 0; return; }
    if (NET.melty && !sora_melty_connected()) return;  // (s189 review N1: sent once in a room, so wait for the connection)
    while (net_edout_due(NETED.q, &NETED.nq, now, &o)) {
        net_send_all(d, nd, o.b, o.len);
        NETED.st.tx_send++;
    }
}

// a NET_EDMG packet from slot k (header already checked by net_unpack_ex)
static void net_edmg_recv(int k, const uint8_t *buf, int n, uint64_t now) {
    NetPeerSlot *sl = &NET.slot[k];
    NetEdEnt e[NET_EDMG_MAX];
    uint8_t pl = 0;
    uint16_t sq = 0;
    int cnt = net_unpack_edmg(buf, n, &pl, &sq, e);
    if (cnt <= 0) { NETED.st.rx_bad++; return; }
    if (!net_edseq_fresh(&sl->ed, sq)) { NETED.st.rx_dup++; return; }  // (one of the NET_EVENT_REPEAT copies)
    NETED.st.rx_pkt++;
    net_edmg_bind();
    for (int i = 0; i < cnt; i++) {
        NETED.st.rx_ent++;
        if (!g_net_edmg) { NETED.st.rx_off++; continue; }
        if (!NETED.apply) {
            NETED.st.rx_nofn++;
            if (e[i].flags & NEDF_KILL) net_kills_edmg(k, e[i].hid, -1, now);  // (s196: waiting)
            continue;
        }
        int r = NETED.apply(e[i].hid, e[i].dealt, e[i].after, e[i].f54, e[i].f40, e[i].flags & NEDF_KILL ? 1 : 0);
        if (e[i].flags & NEDF_KILL) net_kills_edmg(k, e[i].hid, r, now);  // (s196 killsync)
        if (r == 1) NETED.st.rx_queued++;
        else if (r == 2) NETED.st.rx_pending++;
        else NETED.st.rx_ignored++;
        if (g_net_edmg_log)
            LOG("[net] edmg rx peer %d hid=%08x dealt=%u after=%u kill=%d f54=%u f40=%u (seq %u) -> %s\n", k + 1, e[i].hid, e[i].dealt, e[i].after,
                e[i].flags & NEDF_KILL ? 1 : 0, e[i].f54, e[i].f40, (unsigned)sq,
                r == 1 ? "queued" : r == 2 ? "pending (not loaded here)" : r < 0 ? "exe has the capture off" : "ignored (dead / deleting)");
    }
}

static void net_edmg_diag(void) {
    LOG("[net] edmg (%s%s): tx %u packets / %u hits (sent %u, dropped %u: off / alone / loopback, lost %u: queue full); rx %u packets / %u hits "
        "(repeats skipped %u, bad %u): queued %u, pending %u, ignored %u, off %u, no exe fn %u\n", g_net_edmg ? "on" : "OFF", NETED.take && NETED.apply ? "" : ", NO EXE SUPPORT",
        NETED.st.tx_pkt, NETED.st.tx_ent, NETED.st.tx_send, NETED.st.tx_drop, NETED.st.tx_lost, NETED.st.rx_pkt, NETED.st.rx_ent, NETED.st.rx_dup, NETED.st.rx_bad,
        NETED.st.rx_queued, NETED.st.rx_pending, NETED.st.rx_ignored, NETED.st.rx_off, NETED.st.rx_nofn);
    uint64_t v[17];
    int c = NETED.stats ? NETED.stats(v, 17) : 0;
    if (c >= 17)
        LOG("[net] edmg exe: applied %llu (queued %llu, pending %llu, ignored %llu), rides %llu (dead on arrival %llu, expired %llu), echo subtracted %llu, "
            "pending hit when loaded %llu, queue full %llu; captured %llu (lost: ring full %llu), local-only (spawned) %llu; pending map %llu entries "
            "(%llu kills), evicted %llu, expired %llu\n",
            (unsigned long long)v[0], (unsigned long long)v[1], (unsigned long long)v[2], (unsigned long long)v[3], (unsigned long long)v[4],
            (unsigned long long)v[5], (unsigned long long)v[9], (unsigned long long)v[6], (unsigned long long)v[7], (unsigned long long)v[8],
            (unsigned long long)v[10], (unsigned long long)v[11], (unsigned long long)v[12], (unsigned long long)v[13], (unsigned long long)v[14],
            (unsigned long long)v[15], (unsigned long long)v[16]);
}

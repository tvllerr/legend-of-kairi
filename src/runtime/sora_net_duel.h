// (s272 duel, co-op item 6) the net side of the duels: NET_DUEL (sora_net_proto.h, a NEW type: no NET_VERSION change, an older
// game drops it as a "bad" packet without a log line). Included by sora_net.h after sora_net_warp.h (it uses NET, net_send_to,
// LOG, NetLogRate). Received packets addressed to us are QUEUED here; the game side (runtime/sora_duel.h du_frame, the same
// mod frame, after net_frame) takes them with net_duel_pop. With "set duel 0" (the default) nothing is queued or answered:
// an ASK from a friend is only counted (his game then says "no answer"), exactly as before this build.
// Also the small state the early parts of sora.c read (the Triangle eat in pad_tap, the KO floor in ca_survive / mk_tick, the
// Items page, the SNAP flag): DUS, written by sora_duel.h once a frame.
#pragma once
#include "sora_duel_core.h"

static bool g_duel = true;  // live "set duel 0" = off (no prompt, no answer, no hits); default ON since s274 (two-copy run verified); parsed in sora_duel.h
static bool g_duel_log;  // live "set duellog 1": one [duel] line per packet / step
static bool g_du_god = true;  // (s289 3c) live "set duelgod 1": God Mode's life refill waits while a duel runs here (sora_duel_core.h du_god_waits); parsed in sora_duel.h. (s290: passed the two-copy lab, sb/s290/LAB_RESULTS.md; the user: "Flip the passes ON" -> default ON); "set duelgod 0" = the refill in a duel too, as before

// the shared state (sora_duel.h writes it; read by the early hooks)
static struct {
    bool live;        // a countdown / fight / time-up runs (NF_DUEL in our SNAP, the KO floor, the blocks)
    bool floor;       // the KO floor: world damage can't take Sora below 1 (live + DU_OVER, ca_survive's mask 4, mk_tick skips it)
    bool ko_hold;     // (s289 3c) a KO's hold at 1 HP runs (sora_duel.h DU.heal_at, until the duel's own heal): God Mode's refill waits too
    volatile uint64_t eat_x_until;  // a duel prompt holds Triangle until then (pad_tap: BotW does not get its A from that press)
} DUS;

#define NETD_Q 24
#define NETD_RS 12
static struct {
    uint16_t seq;
    bool seeded;
    struct { int k; NetDuelMsg m; } q[NETD_Q];
    int nq;
    struct { int k; uint8_t player; uint8_t b[NET_DUEL_HIT_LEN]; int n, left; uint64_t at; } rs[NETD_RS];  // UDP resends
    struct { uint32_t tx[8], rx[8], bad, other, off, qfull, resent; } st;  // (by op; s289: 7 = PULL)
    NetLogRate lr;
} NETD;

static uint32_t net_duel_nonce(void) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    uint64_t x = (uint64_t)t.QuadPart * 0xD6E8FEB86659FD93ull ^ GetTickCount64() * 0x9E3779B97F4A7C15ull;
    x ^= x >> 29;
    uint32_t n = (uint32_t)x ^ (uint32_t)(x >> 32);
    return n ? n : 1;
}
static bool net_duel_slot_ok(int k) {
    return NET.on && k >= 0 && k < NET_MAX_PEERS && NET.slot[k].used && NET.slot[k].p.st != PEER_NONE && (NET.slot[k].ip || NET.melty);
}
static const char *const NDO_NAME[8] = {"?", "ASK", "ANSWER", "HIT", "END", "TIME", "RING", "PULL"};

// one packet to slot k (target = his player id; RING: 0). ANSWER / HIT / END / TIME / PULL are sent again DU_RESENDS times over UDP.
static bool net_duel_send(int k, const NetDuelMsg *m, uint64_t now) {
    if (!net_duel_slot_ok(k) || !m) return false;
    if (!NETD.seeded) NETD.seeded = true, NETD.seq = (uint16_t)(GetTickCount64() * 2654435761u >> 9);
    uint8_t b[NET_DUEL_HIT_LEN];
    NetDuelMsg c = *m;
    if (c.op != NDO_RING) c.target = NET.slot[k].p.player;
    int n = net_pack_duel(NET.me, ++NETD.seq, &c, b, (int)sizeof b);
    if (n <= 0) return false;
    net_send_to(NET.slot[k].ip, NET.slot[k].port, b, n);
    NETD.st.tx[c.op]++;
    if (!NET.melty && c.op != NDO_ASK && c.op != NDO_RING) {
        for (int i = 0; i < NETD_RS; i++)
            if (!NETD.rs[i].left) {
                NETD.rs[i].k = k, NETD.rs[i].player = NET.slot[k].p.player, memcpy(NETD.rs[i].b, b, (size_t)n), NETD.rs[i].n = n;
                NETD.rs[i].left = DU_RESENDS, NETD.rs[i].at = now + DU_RESEND_MS;
                break;
            }
    }
    if (g_duel_log && c.op != NDO_RING)
        LOG("[duel] net: %s to slot %d \"%s\" (duel %08x)\n", NDO_NAME[c.op], k + 1, NET.slot[k].p.name, (unsigned)c.id);
    return true;
}
// RING to every peer but the two duelists' own (the opponent draws his own ring)
static void net_duel_ring_all(const NetDuelMsg *m, int skip_k, uint64_t now) {
    for (int k = 0; k < NET_MAX_PEERS; k++)
        if (k != skip_k && net_duel_slot_ok(k)) net_duel_send(k, m, now);
}
// once a frame: the UDP resends that are due
static void net_duel_tick(uint64_t now) {
    for (int i = 0; i < NETD_RS; i++) {
        if (!NETD.rs[i].left || now < NETD.rs[i].at) continue;
        int k = NETD.rs[i].k;
        if (net_duel_slot_ok(k) && NET.slot[k].p.player == NETD.rs[i].player && !NET.melty) {
            net_send_to(NET.slot[k].ip, NET.slot[k].port, NETD.rs[i].b, NETD.rs[i].n);
            NETD.st.resent++;
            NETD.rs[i].left--, NETD.rs[i].at = now + DU_RESEND_MS;
        } else NETD.rs[i].left = 0;
    }
}
static void net_duel_drop_all(void) { NETD.nq = 0, memset(NETD.rs, 0, sizeof NETD.rs); }

// a NET_DUEL packet from slot k (header already checked by net_unpack_ex)
static void net_duel_recv(int k, const uint8_t *buf, int n, uint64_t now) {
    NetDuelMsg m;
    uint8_t pl = 0;
    uint16_t sq = 0;
    if (!net_unpack_duel(buf, n, &pl, &sq, &m)) { NETD.st.bad++; return; }
    if (m.op != NDO_RING && m.target != NET.me) { NETD.st.other++; return; }  // (for another player)
    NETD.st.rx[m.op]++;
    if (!g_duel) { NETD.st.off++; return; }  // (duel 0 = today: nothing answered, nothing done)
    if (NETD.nq >= NETD_Q) {
        NETD.st.qfull++;
        if (net_lograte_ok(&NETD.lr, now, NULL)) LOG("[duel] net: the queue is full: %s from slot %d dropped\n", NDO_NAME[m.op], k + 1);
        return;
    }
    NETD.q[NETD.nq].k = k, NETD.q[NETD.nq].m = m, NETD.nq++;
}
// the game side: the oldest queued packet (false = none)
static bool net_duel_pop(int *k, NetDuelMsg *m) {
    if (NETD.nq <= 0) return false;
    *k = NETD.q[0].k, *m = NETD.q[0].m;
    memmove(NETD.q, NETD.q + 1, sizeof NETD.q[0] * (size_t)(NETD.nq - 1)), NETD.nq--;
    return true;
}

static void net_duel_diag(void) {
    LOG("[net] duel: tx ask %u answer %u hit %u end %u time %u ring %u pull %u; rx ask %u answer %u hit %u end %u time %u ring %u pull %u; "
        "bad %u, for others %u, off %u, queue full %u, resent %u%s\n",
        NETD.st.tx[1], NETD.st.tx[2], NETD.st.tx[3], NETD.st.tx[4], NETD.st.tx[5], NETD.st.tx[6], NETD.st.tx[7], NETD.st.rx[1], NETD.st.rx[2],
        NETD.st.rx[3], NETD.st.rx[4], NETD.st.rx[5], NETD.st.rx[6], NETD.st.rx[7], NETD.st.bad, NETD.st.other, NETD.st.off, NETD.st.qfull,
        NETD.st.resent, DUS.live ? " (a duel runs)" : "");
}

// ---- the early hooks (sora.c before sora_duel.h) ----
static bool du_eat_x(void) { return g_duel && GetTickCount64() <= DUS.eat_x_until; }  // pad_tap: Triangle is the duel prompt's
static int du_survive_mask(void) { return g_duel && DUS.floor ? 4 : 0; }              // ca_survive: any fatal hit leaves 1
static bool du_floor_on(void) { return g_duel && DUS.floor; }                         // mk_tick: that clamp is not a Mickey KO
static bool du_no_items(void) { return g_duel && DUS.live; }                          // the Items page: no item in a duel
static bool du_on_wire(void) { return g_duel && DUS.live; }                           // net_fill: NF_DUEL in our SNAP
static bool du_god_wait(void) { return g_duel && du_god_waits(g_du_god, DUS.live, DUS.ko_hold); }  // (s289 3c) dbm_god_tick_: God Mode's refill waits

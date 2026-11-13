// s191 (Melty spec 4.1, task 2): who is on which map. Included by sora_net.h right after net_send_all (it uses NET, net_dests,
// net_send_all, LOG). The packet is NET_AREA (sora_net_proto.h, a NEW type: no other packet and no NET_VERSION change, so a
// 0.1.1-0.1.4 game still plays with this one and drops AREA as a "bad" packet without a log line).
//   send (net_area_tick, first thing in net_tick): NET.area on every change, when a peer slot appears, and every NET_AREA_MS
//        (UDP loses packets) / NET_AREA_MELTY_MS (a Melty room event is reliable). Nothing while alone / loopback.
//   receive (net_area_recv): the newest text per peer slot; a peer silent for NET_AREA_FORGET periods is "not known" again.
//   hidden (net_area_eval): we are both in KNOWN, DIFFERENT areas. A peer that never sent AREA (an old build), `set area 0`
//        (our NET.area is "" then, and so is what we send) or a "" from him = one world = never hidden. Hidden = his avatar,
//        party, plate, fx and sounds are not drawn (sora_net_avatar.h), his FIRE is dropped (sora_net_fire.h), he is "away" in
//        the enemy election (sora_net_esync.h: his samples / claims are still taken, he is never nearest / closer). EDMG stays for everyone (hids are unique across maps). The slot itself is
//        kept as it is: he shows again the frame both areas match.
//   log: one "[net] area: peer ..." line when his hidden state or his area changes, nothing per packet.
#pragma once

#define NET_AREA_MS 2000         // UDP: resend this often
#define NET_AREA_MELTY_MS 10000  // Melty room: the event is reliable, so only now and then (a seat change / a missed join)
#define NET_AREA_FORGET 5        // his AREA not heard for this many of OUR periods = not known again (an old build after a restart)

static struct {
    uint16_t seq;
    uint64_t next_ms;
    bool sent_any, said_real;  // (said_real: a non-empty area went out this session)
    char sent[NET_AREA_MAX + 1];
    uint64_t joined[NET_MAX_PEERS];  // the slots' joined_ms as last seen: a new one gets our area at once
    struct { uint32_t tx, rx, rx_dup, rx_bad, hid_changes; } st;
} NETA;

static uint64_t net_area_period(void) { return NET.melty ? NET_AREA_MELTY_MS : NET_AREA_MS; }

// his area as we know it now ("" = not known)
static const char *net_area_of(int k, uint64_t now) {
    const NetPeerSlot *sl = &NET.slot[k];
    if (!sl->used || !sl->area_ms) return "";
    if (now > sl->area_ms && now - sl->area_ms > NET_AREA_FORGET * net_area_period()) return "";
    return sl->area;
}

static void net_area_eval(int k, uint64_t now) {
    NetPeerSlot *sl = &NET.slot[k];
    if (!sl->used) return;
    const char *his = net_area_of(k, now);
    bool hidden = net_area_apart_txt(NET.area, his);
    if (hidden != sl->hidden || strcmp(his, sl->area_said_txt)) {
        NETA.st.hid_changes += hidden != sl->hidden;
        LOG("[net] area: peer \"%s\" is on %s (we are on %s): %s\n", sl->p.name[0] ? sl->p.name : "?", his[0] ? his : "(not known)",
            NET.area[0] ? NET.area : "(not known)", hidden ? "hidden" : "shown");
        snprintf(sl->area_said_txt, sizeof sl->area_said_txt, "%s", his);
    }
    sl->hidden = hidden;
}

// every net_tick, before the receive: our area out when due, every slot's hidden state
static void net_area_tick(uint64_t now) {
    bool force = strcmp(NET.area, NETA.sent) != 0 || !NETA.sent_any;
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        uint64_t j = NET.slot[k].used ? NET.slot[k].joined_ms + 1 : 0;  // (+1: a slot joined at ms 0 is still "a slot")
        if (j != NETA.joined[k]) force = force || j != 0, NETA.joined[k] = j;
    }
    // ("" before any real area = nothing to say: a peer that never heard from us treats us as one world anyway; once we said a map,
    // a "" goes out too, so `set area 0` shows us again everywhere)
    bool say = NET.area[0] || NETA.said_real;
    if (say && !g_net_loop && (force || now >= NETA.next_ms) && !(NET.melty && !sora_melty_connected())) {
        NetAddr d[NET_DESTS_MAX];
        int nd = net_dests(d, false);
        bool anyone = NET.melty ? netslot_count(NET.slot, NET_MAX_PEERS) > 0 : nd > 0;
        if (anyone) {
            uint8_t b[NET_AREA_PKT];
            int n = net_pack_area(NET.me, ++NETA.seq, NET.area, b, (int)sizeof b);
            if (n > 0) {
                net_send_all(d, nd, b, n);
                NETA.st.tx++, NETA.sent_any = true, NETA.said_real = NETA.said_real || NET.area[0], NETA.next_ms = now + net_area_period();
                snprintf(NETA.sent, sizeof NETA.sent, "%s", NET.area);
            }
        }
    }
    for (int k = 0; k < NET_MAX_PEERS; k++) net_area_eval(k, now);
}

// a NET_AREA packet from slot k (header already checked by net_unpack_ex)
static void net_area_recv(int k, const uint8_t *buf, int n, uint64_t now) {
    NetPeerSlot *sl = &NET.slot[k];
    char a[NET_AREA_MAX + 1];
    uint8_t pl = 0;
    uint16_t sq = 0;
    if (!net_unpack_area(buf, n, &pl, &sq, a)) { NETA.st.rx_bad++; return; }
    if (!net_esseq_fresh(&sl->ars, sq)) { NETA.st.rx_dup++; return; }  // (an older one that came late)
    NETA.st.rx++;
    memcpy(sl->area, a, sizeof sl->area), sl->area_ms = now ? now : 1;
    net_area_eval(k, now);  // (at once: a FIRE right behind it in this receive is already judged by it)
}

// net on: a fresh seq, sent again at the next tick
static void net_area_reset(uint64_t now) {
    memset(&NETA, 0, sizeof NETA);
    NETA.seq = (uint16_t)(now * 2246822519u >> 9);
}

static void net_area_diag(void) {
    char peers[256] = "";
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        const NetPeerSlot *sl = &NET.slot[k];
        if (!sl->used) continue;
        size_t l = strlen(peers);
        snprintf(peers + l, sizeof peers - l, "; slot %d \"%s\" %s%s", k + 1, sl->p.name, sl->area_ms ? (sl->area[0] ? sl->area : "(not known)") : "(never said: old build?)",
                 sl->hidden ? " HIDDEN" : "");
    }
    LOG("[net] area: we are on %s; sent %u, received %u (late %u, bad %u), hidden changes %u%s\n", NET.area[0] ? NET.area : "(not known)", NETA.st.tx, NETA.st.rx,
        NETA.st.rx_dup, NETA.st.rx_bad, NETA.st.hid_changes, peers);
}

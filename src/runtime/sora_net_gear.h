// sora_net_gear.h (s289 netgear): the other players see the BotW weapon in Sora's hand. User: "so the other sora in multiplayer
// can see sora holding the breath of the wild weapons instead of them being invisible if sora has them equip". Included by
// sora_net.h right after sora_net_area.h (uses NET, net_dests, net_send_all, LOG, g_net_loop). Pure parts: sora_net_gear_core.h;
// the wire: NET_GEAR in sora_net_proto.h; the drawing: sora_net_gear_draw.h (from sora_net_avatar.h).
//   ours     net_gear_local (sora.c, right after wpn_tick): the weapon wpn_tick put in Sora's hand this frame (g_wpn_hand) or
//            nothing. Sent (net_gear_tick, from net_tick) on a change, on a join and every NET_GEAR_MS / NET_GEAR_MELTY_MS;
//            never per frame. Nothing while alone. Loopback (`set netloop 1`): pack -> unpack -> slot 0, like the SNAPs.
//   theirs   net_gear_recv: the newest list per peer slot (net_esseq_fresh); net_gear_hand(k) = his melee weapon in his hand
//            now, NULL = none / not known (a game that never sent one, or quiet for NET_GEAR_FORGET periods).
//   keys     netgear 0|1 (default 1 since s290; 0 = as before: nothing sent, a received NET_GEAR dropped and counted, nothing new drawn),
//            netgearlog 0|1 (one line per change sent / received), netgearmax N (weapon models loaded for friends, GUESS 8).
// Display only: a name goes out and a picture comes back; no BotW actor, item or inventory is made, read or written for it.
#pragma once
#include "sora_net_gear_core.h"

static int g_net_gear = 1;      // live "set netgear 0|1". (s290: passed the two-copy lab, sb/s290/LAB_RESULTS.md; the user: "Flip the passes ON" -> default ON)
static int g_net_gear_log;      // live "set netgearlog 1"
static int g_net_gear_max = 8;  // live "set netgearmax N" (GUESS: 3 friends x 2-3 weapons each in a session)

static struct {
    uint16_t seq;
    uint64_t next_ms, last_tx_ms;
    bool sent_any, join_due;
    NetGearItem have[NET_GEAR_N], sent[NET_GEAR_N];
    int nhave, nsent;
    uint64_t joined[NET_MAX_PEERS];  // the slots' joined_ms + 1 as last seen: a new one gets our list at once
    struct { NetEsSeen seen; NetGearItem it[NET_GEAR_N]; int n; uint64_t at, joined; } peer[NET_MAX_PEERS];
    struct { uint32_t tx, rx, rx_dup, rx_bad, rx_off, ch_out, ch_in; } st;
} NETG;

static uint64_t net_gear_period(void) { return NET.melty ? NET_GEAR_MELTY_MS : NET_GEAR_MS; }

static void net_gear_txt(char *out, size_t cap, const NetGearItem *it, int n) {
    out[0] = 0;
    if (!n) { snprintf(out, cap, "nothing"); return; }
    for (int i = 0; i < n; i++) {
        size_t l = strlen(out);
        snprintf(out + l, cap - l, "%s%s %s (%s)", i ? ", " : "", it[i].kind == NGK_MELEE ? "melee" : it[i].kind == NGK_SHIELD ? "shield" : it[i].kind == NGK_BOW ? "bow" : "kind ?",
                 it[i].name, (it[i].flags & NGF_HAND) ? "in hand" : "put away");
    }
}

// sora.c, every driven frame after wpn_tick: melee = the weapon's actor name (NULL = Keyblade / none), in_hand = the exe was told
// "in his hand" (g_wpn_hand). Only stored: net_gear_tick decides what goes out.
static void net_gear_local(const char *melee, bool in_hand) { NETG.nhave = ngear_local(NETG.have, melee, in_hand); }

// slot k's record, wiped when the slot is someone else now (joined_ms changed) or free
static bool net_gear_rec_ok(int k) {
    const NetPeerSlot *sl = &NET.slot[k];
    return sl->used && NETG.peer[k].joined == sl->joined_ms + 1;
}

// a NET_GEAR packet of slot k (header checked by net_unpack_ex), also the loopback's own
static void net_gear_take(int k, const uint8_t *buf, int n, uint64_t now) {
    NetPeerSlot *sl = &NET.slot[k];
    if (!net_gear_rec_ok(k)) memset(&NETG.peer[k], 0, sizeof NETG.peer[k]), NETG.peer[k].joined = sl->joined_ms + 1;
    NetGearItem it[NET_GEAR_N];
    int ni = 0;
    uint8_t pl = 0;
    uint16_t sq = 0;
    if (!net_unpack_gear(buf, n, &pl, &sq, it, &ni)) { NETG.st.rx_bad++; return; }
    if (!net_esseq_fresh(&NETG.peer[k].seen, sq)) { NETG.st.rx_dup++; return; }  // (an older one that came late)
    NETG.st.rx++;
    bool had = ngear_fresh(NETG.peer[k].at, now, net_gear_period());
    bool changed = !had || !ngear_same(NETG.peer[k].it, NETG.peer[k].n, it, ni);
    memcpy(NETG.peer[k].it, it, sizeof it), NETG.peer[k].n = ni, NETG.peer[k].at = now ? now : 1;
    if (changed) {
        NETG.st.ch_in++;
        if (g_net_gear_log) {
            char t[256];
            net_gear_txt(t, sizeof t, it, ni);
            LOG("[net] gear: received from peer %d \"%s\": %s\n", k + 1, sl->p.name[0] ? sl->p.name : "?", t);
        }
    }
}

static void net_gear_recv(int k, const uint8_t *buf, int n, uint64_t now) {
    if (!g_net_gear) { NETG.st.rx_off++; return; }  // (key 0: dropped, nothing new drawn = as before)
    net_gear_take(k, buf, n, now);
}

// slot k's melee weapon in his hand now (his actor name), NULL = none / not known / key 0
static const char *net_gear_hand(int k, uint64_t now) {
    if (!g_net_gear || k < 0 || k >= NET_MAX_PEERS || !net_gear_rec_ok(k) || !ngear_fresh(NETG.peer[k].at, now, net_gear_period())) return NULL;
    return ngear_hand(NETG.peer[k].it, NETG.peer[k].n);
}

// every net_tick (after net_area_tick): our list out when due
static void net_gear_tick(uint64_t now) {
    if (!g_net_gear) return;
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        uint64_t j = NET.slot[k].used ? NET.slot[k].joined_ms + 1 : 0;  // (+1: a slot joined at ms 0 is still "a slot")
        if (j != NETG.joined[k]) NETG.join_due = NETG.join_due || j != 0, NETG.joined[k] = j;
    }
    if (NET.melty && !sora_melty_connected() && !g_net_loop) return;
    bool changed = !NETG.sent_any || !ngear_same(NETG.have, NETG.nhave, NETG.sent, NETG.nsent);
    if (!ngear_due(changed, NETG.join_due, NETG.sent_any, now, NETG.next_ms, NETG.last_tx_ms)) return;
    NetAddr d[NET_DESTS_MAX];
    int nd = g_net_loop ? 0 : net_dests(d, false);
    bool anyone = g_net_loop ? NET.slot[0].used : NET.melty ? netslot_count(NET.slot, NET_MAX_PEERS) > 0 : nd > 0;
    if (!anyone) return;  // (alone: nothing to say; the first peer's join sends it)
    uint8_t b[NET_GEAR_PKT];
    int n = net_pack_gear(NET.me, ++NETG.seq, NETG.have, NETG.nhave, b, (int)sizeof b);
    if (n <= 0) return;
    if (g_net_loop) net_gear_take(0, b, n, now);  // (loopback: slot 0 is ourselves, as a remote would get it)
    else net_send_all(d, nd, b, n);
    if (changed) {
        NETG.st.ch_out++;
        if (g_net_gear_log) {
            char t[256];
            net_gear_txt(t, sizeof t, NETG.have, NETG.nhave);
            LOG("[net] gear: sent %s (%s)\n", t, g_net_loop ? "loopback" : NET.melty ? "Melty room" : "UDP");
        }
    }
    NETG.st.tx++, NETG.sent_any = true, NETG.join_due = false, NETG.last_tx_ms = now, NETG.next_ms = now + net_gear_period();
    memcpy(NETG.sent, NETG.have, sizeof NETG.sent), NETG.nsent = NETG.nhave;
}

// net on: a fresh seq, our list out at the next tick, no peer's list kept
static void net_gear_reset(uint64_t now) {
    NetGearItem have[NET_GEAR_N];
    int nhave = NETG.nhave;
    memcpy(have, NETG.have, sizeof have);
    memset(&NETG, 0, sizeof NETG);
    memcpy(NETG.have, have, sizeof have), NETG.nhave = nhave;
    NETG.seq = (uint16_t)(now * 2654435761u >> 11);
}

static void net_gear_diag(void) {
    char ours[256], peers[600] = "";
    net_gear_txt(ours, sizeof ours, NETG.have, NETG.nhave);
    uint64_t now = GetTickCount64();
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        if (!NET.slot[k].used) continue;
        char t[256] = "never said (old build / netgear 0 there?)";
        if (net_gear_rec_ok(k) && NETG.peer[k].at) {
            net_gear_txt(t, sizeof t, NETG.peer[k].it, NETG.peer[k].n);
            if (!ngear_fresh(NETG.peer[k].at, now, net_gear_period())) snprintf(t + strlen(t), sizeof t - strlen(t), " (FORGOTTEN: quiet)");
        }
        size_t l = strlen(peers);
        snprintf(peers + l, sizeof peers - l, "; slot %d \"%s\" %s", k + 1, NET.slot[k].p.name, t);
    }
    LOG("[net] gear %s: ours %s; sent %u (changes %u), received %u (changes %u, late %u, bad %u, dropped with netgear 0 %u)%s\n",
        g_net_gear ? "on" : "OFF (set netgear 1)", ours, NETG.st.tx, NETG.st.ch_out, NETG.st.rx, NETG.st.ch_in, NETG.st.rx_dup, NETG.st.rx_bad,
        NETG.st.rx_off, peers);
}

// live keys (one line when a value changes: a live rewrite re-applies every line)
static bool net_gear_live_set(const char *key, int v) {
    if (!strcmp(key, "netgear")) {
        if (g_net_gear == (v != 0)) return true;
        g_net_gear = v != 0;
        memset(NETG.peer, 0, sizeof NETG.peer), NETG.sent_any = false;  // (on again = sent at once, theirs taken anew)
        LOG("[net] gear %s\n", g_net_gear ? "on: our BotW weapon goes to the other games, theirs is drawn in their Sora's hand (needs data/gear)"
                                          : "off (set netgear 0: nothing sent, friends' BotW weapons not drawn: as before)");
    } else if (!strcmp(key, "netgearlog")) {
        if (g_net_gear_log == (v != 0)) return true;
        g_net_gear_log = v != 0;
        LOG("[net] gear log %s\n", g_net_gear_log ? "on (one line per weapon change sent / received)" : "off");
    } else if (!strcmp(key, "netgearmax")) {
        int m = v < 0 ? 0 : v > 32 ? 32 : v;
        if (g_net_gear_max == m) return true;
        g_net_gear_max = m;
        LOG("[net] gear: at most %d weapon models loaded for friends this session\n", m);
    } else return false;
    return true;
}

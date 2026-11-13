// Multiplayer slice 1 glue (spec docs/specs/2026-11-05-multiplayer-slice1-design.md sections 3-4, plan Task 5).
// Included once by sora.c right after sora_fx.h (the fx / sfx wrappers there call net_note_*). (mp4, slice 4: up to
// NET_MAX_PEERS = 3 remote peers in a full mesh, one NetPeerSlot each, docs/specs/2026-11-06-multiplayer-slice4-design.md.)
// The game state that goes into a SNAP is read by net_fill() (sora.c, defined late: it needs S, KB, FM, SKT...).
// Called once per frame from sora_mod_frame: net_frame(now) -> net_tick(Link's pos | NULL while loading / menu).
//   env:   SAVAGEWIND_SORA_NET=1 (on at start), SAVAGEWIND_SORA_NET_NAME=<name> (default "Sora"), SAVAGEWIND_SORA_NET_PORT=<n>,
//          SAVAGEWIND_SORA_NET_PEERS=ip[:port],ip[:port],... (pins at start)
//   live:  set net 0|1, set netloop 0|1, set netbone 0|1 (bone-pose sync, default 1), set netskate 0|1 (skate rider sync, default 1), set netplate 0|1, set netparty 0|1, set netport <n>, set edmg 0|1 (shared enemy HP, default 1), set esync 0|1 (enemy sync, default 0), set esynclog 0|1,
//          set edmglog 0|1 (log each edmg tx / rx; the glue is sora_net_edmg.h), set netreact 0|1 (s194: hit reactions on enemies another game owns, default 1 since s196; sora_net_react.h),
//          set eshit 0|1 (s195: my hit takes over an enemy another game runs, default 1 since s196), set eshitms <ms> (s195: the owner's hits keep
//          "closer" hand-overs off this long, default 4000), set esdead 0|1 (s195: a dead player is nobody's nearest, default 1 since s196; sora_net_esync.h),
//          set killsync 0|1 (s196: kills stay dead after a reload / for a joining game, default 1 since s197), set killsclear 1 (one-shot), set killslog 0|1 (sora_net_kills.h),
//          set netfire 0|1 (fire sync, default 1), set netfireen 0|1 (also ignite enemies on the receiver, default 0), set netfirespk 0|1 (sparks at enemies on the receiver, default 1),set netfirelog 0|1 (default 1; sora_net_fire.h);
//          lines "net peer <ip>[:port]" (adds a pin) | "net peer off" (clears the pins) | "net peer" (logs the table), "net name <s>", "net diag"
// Net off (default): no socket, net_tick returns at once, net_note_* return at once (no sfx / fx name lookups).
// (s187) MELTY NET: when Melty started this copy (sora_melty_start() == 1, sora_net_sock.h) the packets go through Melty's room
// and not the UDP socket: NET.melty. No socket / port / HELLO / BYE / pins / LAN discovery then; a room player = a peer slot
// under the fake address nmelty_ip(room peer id):1, made when the room says he joined and freed at once when it says he left;
// a packet "to every peer" is ONE send to the room (net_send_all). Outside Melty NET.melty is false and nothing changes.
// (s191, spec 4.4) THE ROOM'S STORY is drained in ONE place (net_story_pump) and each key handed to its owner: so far only
// botw.clock:world -> the room clock (sora_clock.h: time of day, weather, Blood Moon). Live "set mclock 0|1" (default on when
// Melty started the game), "set mclockfake <hour>" (dev: the guest path with a fake clock, also outside a room; -1 = off),
// "set mclocktake 0|1" (s198, default 1: a new leader takes the room's clock first and writes nothing off the world).
// (s207) THE SOLO BUILD: env SAVAGEWIND_SORA_SOLO=1 (melty-setup/play.py, COOP = False) = no co-op at all in this copy:
// Melty's room is never asked, no UDP socket is ever opened (net_set_on refuses: also "set net 1", a pin list, Sora mode
// coming back), so no peers, no SNAPs, no enemy sync. Unset / 0 = everything as before.
#pragma once
#include "sora_net_sock.h"
#include "sora_net_peer.h"
#include "sora_net_note.h"  // (s209: Melty's lab notes, HELLO + FRAME: the counts and the HELLO's version)
#include "sora_clock.h"

#define NET_SNAP_MS 50      // 20 Hz
#define NET_HELLO_MS 1000   // while a peer / target has not answered
#define NET_HELLO_LIVE_MS 5000
#define NET_EVQ 64          // outgoing events waiting for a SNAP (mp4: 16 -> 64, with NET_MAX_EVENTS 12)
#define NET_TG_MAX 8        // (mp4) addresses we HELLO: pins (<= NET_MAX_PEERS) + learned from the peers' HELLO lists
#define NET_TG_LEARN_MS 30000  // a learned address that never answered is forgotten after this (gossip re-adds it)

static int g_net_bone = 1;  // (s170 bonedraw) live "set netbone 0": no bone block sent, the remote's ragdoll / puppet pose falls back to idle / fall
static unsigned g_net_bone_sent, g_net_bone_recv, g_net_bone_drawn, g_net_bone_fb, g_net_bone_skip;  // ("net diag": SNAPs with a block / frames a bone-pose peer had one / drawn from it / fell back)
static int g_net_skate = 1;  // (s172) live "set netskate 0": no NF_SKATE sent / drawn, a remote rider shows the old idle pose
static unsigned g_net_skate_sent, g_net_skate_drawn, g_net_skate_fb;  // ("net diag": SNAPs sent riding / frames a riding peer was drawn as the rider / fell back)
static bool g_net_loop;     // live "set netloop 1": own SNAPs -> pack -> unpack -> slot 0
static bool g_net_edmg = true;  // (s166) live "set edmg 0": no shared enemy HP (nothing sent, received hits ignored); the exe needs SAVAGEWIND_LINK_EDMG=1
static bool g_net_edmg_log;     // (s166) live "set edmglog 1": [net] edmg tx / rx lines
static bool g_net_noparty;  // live "set netparty 0": no remote Donald + Goofy (default on)
static bool g_net_noplate;  // live "set netplate 0": no name plate (perf A/B; default on)
static int g_net_anchor = 1;  // (slice 3) live "set netanchor 0": every fx event / move world-placed, every move re-sent (A/B)

typedef struct { uint32_t ip; uint16_t port; bool pinned; uint64_t born; } NetTarget;  // (mp4) an address we HELLO

static struct {
    bool on, inited, fail_logged, driven, loading;
    bool melty;                                               // (s187) in a Melty Net room: no socket, see the top of this file
    uint16_t melty_self;                                      // (s187) our room peer id as last seen (0 = not in the room yet)
    uint64_t melty_since;                                     // (s187) when sora_melty_start said yes (the "still connecting" log)
    bool melty_slow_logged;
    char area[32];                                            // (s190, Melty spec 4.1) the map we are on (net_set_area; "" = not known)
    struct { uint16_t id; char name[NET_NAME_MAX + 1]; bool said; } mroom[16]; int nmroom;  // (s187) who the room says is here (kept while net is off too), in join order (s198); said = his "no slot" line went
    NetSock sock;
    uint16_t port, seq;
    uint8_t me, evseq;
    char name[NET_NAME_MAX + 1];
    NetPeerSlot slot[NET_MAX_PEERS];                          // (mp4) the remote players; loopback = slot 0
    NetTarget tg[NET_TG_MAX]; int ntg;                        // (mp4) pins + learned addresses
    NetAddr refused[8]; int nrefused;                         // (mp4) addresses already logged as "players already"
    struct { uint32_t ip; uint16_t port; int player; } alias[12]; int nalias;  // (mp4 review) addresses net_learn skips: our own (player -1) / a 2nd address of a live player
    uint64_t learn_hello_at;                                  // (mp4 review) net_learn forces a HELLO round at most once per second
    uint64_t next_snap, next_hello, last_recv_ms;
    float feet[3]; bool have_feet;
    NetEvent evq[NET_EVQ]; uint8_t evrep[NET_EVQ]; int nevq;  // outgoing, each rides NET_EVENT_REPEAT SNAPs
    NetFxOut fxo[NET_FXO_N]; int fxrr;                             // (Task 8) our sent fx by handle: moves / kills to send
    uint32_t rfx_born;                                        // (mp2) arrival counter: which remote fx copy is the oldest (all peers)
    bool parked;                                              // (mp2) Sora mode off while net was on: BYE sent, socket closed, resumes with Sora
    bool wet, dead;                                           // set by the frame body (ghost / Game Over)
    struct { uint32_t sent, recv, bad, stale, other, echo, ev_sent, ev_fresh, ev_played, lost_out, lost_in, err;
        uint32_t mv_sent, mv_skip, anc_ev[4], anc_ev_in[4], anc_mv_in[4], anc_guess;
        uint32_t melty_wait; } st;  // (s187: melty_wait = packets not sent because the room was not connected yet)  // (slice 3: moves sent / skipped as unchanged, fx events by anchor, received moves by anchor, party anchors played at Sora; mp4: lost_out = our queue full, lost_in = a peer's inq full)
} NET;

static void avatar_diag(void);     // (sora_net_avatar.h: "net diag" logs the avatar too)
static void net_party_off(void);   // (sora_net_party.h: the remote Donald + Goofy hidden)
static void net_party_diag(void);  // ("net diag")
static void avatar_off(void);      // (sora_net_avatar.h: sora_mod_unload hides the remotes)
static void avatar_slot_off(int k);     // (mp4: one slot's Sora + plate + ribbon hidden)
static void net_party_slot_off(int k);  // (mp4: one slot's Donald + Goofy hidden)
static bool fx_mine(int h, uint32_t gen);  // (sora.c: this fx slot still holds the effect with that generation)
static void av_drop_events(void);  // (sora_net_avatar.h)
static void net_fill(NetSnap *s);  // (sora.c) body, Keyblades, slot, clock, yaw, flags, hp / mp - not feet / player / seq
// (slice 3, sora_net_party.h) the party members' drawn poses for the fx anchors: local (sender) / remote slot k (receiver)
static bool net_party_local_pose(int who, float feet[3], float *face);
static bool net_party_remote_pose(int k, int who, float feet[3], float *face);

#define NETP(k) (NET.slot[k].p)

static const char *net_ip_txt(uint32_t ip_be, char out[16]) {
    const uint8_t *q = (const uint8_t *)&ip_be;
    snprintf(out, 16, "%u.%u.%u.%u", q[0], q[1], q[2], q[3]);
    return out;
}

static uint8_t net_roll_id(uint8_t avoid) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    uint64_t x = (uint64_t)t.QuadPart ^ (uint64_t)(uintptr_t)&t ^ GetTickCount64() * 0x9E3779B97F4A7C15ull;
    x ^= x >> 29, x *= 0xBF58476D1CE4E5B9ull, x ^= x >> 32;
    uint8_t id = (uint8_t)x;
    return id == avoid ? (uint8_t)(id + 1) : id;
}
// (mp4) a fresh own id that no live slot uses ("we re-roll on a clash with ANY peer")
static uint8_t net_roll_free(uint8_t avoid) {
    uint8_t id = net_roll_id(avoid);
    for (int tries = 0; tries < 16; tries++) {
        bool clash = id == avoid;
        for (int k = 0; k < NET_MAX_PEERS; k++) clash = clash || (NET.slot[k].used && NETP(k).st != PEER_NONE && NETP(k).player == id);
        if (!clash) break;
        id = net_roll_id(id);
    }
    return id;
}

static void net_name_set(const char *s) {
    size_t n = 0;
    while (s && s[n] && n < NET_NAME_MAX) NET.name[n] = s[n], n++;
    NET.name[n] = 0;
    if (!n) strcpy(NET.name, "Sora");
}

static void net_send_to(uint32_t ip, uint16_t port, const uint8_t *buf, int n) {
    if (n <= 0) return;
    if (NET.melty) {  // (s187: one room player; the callers that mean "everyone" use net_send_all)
        if (!nmelty_is(ip, port)) return;
        if (!sora_melty_connected()) { NET.st.melty_wait++; return; }
        if (sora_melty_send(nmelty_peer(ip), buf, n) == n) NET.st.sent++;
        else NET.st.err++;
        return;
    }
    if (nsock_send(&NET.sock, ip, port, buf, n) == n) NET.st.sent++;
    else NET.st.err++;
}

// ---- (mp4) addresses: pins / learned targets ----
static int net_tg_find(uint32_t ip, uint16_t port) {
    for (int i = 0; i < NET.ntg; i++) if (NET.tg[i].ip == ip && NET.tg[i].port == port) return i;
    return -1;
}
static int net_npins(void) {
    int c = 0;
    for (int i = 0; i < NET.ntg; i++) c += NET.tg[i].pinned;
    return c;
}
static void net_tg_del(int i) { memmove(NET.tg + i, NET.tg + i + 1, sizeof NET.tg[0] * (size_t)(NET.ntg - i - 1)), NET.ntg--; }
static int net_slot_at(uint32_t ip, uint16_t port) {  // the used slot of exactly this address, -1 none
    for (int k = 0; k < NET_MAX_PEERS; k++) if (NET.slot[k].used && NET.slot[k].ip == ip && NET.slot[k].port == port) return k;
    return -1;
}
// a pin (explicit address, kept) or a learned one (from a HELLO list). false = refused (list full / pins full)
static bool net_tg_add(uint32_t ip, uint16_t port, bool pinned, uint64_t now) {
    int i = net_tg_find(ip, port);
    if (i >= 0) {
        if (pinned && !NET.tg[i].pinned) {
            if (net_npins() >= NET_MAX_PEERS) return false;
            NET.tg[i].pinned = true;
        }
        return true;
    }
    if (pinned && net_npins() >= NET_MAX_PEERS) return false;
    if (NET.ntg == NET_TG_MAX) {  // full: a learned one gives way to a pin
        int old = -1;
        for (int j = 0; j < NET.ntg; j++) if (!NET.tg[j].pinned && (old < 0 || NET.tg[j].born < NET.tg[old].born)) old = j;
        if (old < 0 || !pinned) return false;
        net_tg_del(old);
    }
    NET.tg[NET.ntg++] = (NetTarget){ip, port, pinned, now};
    return true;
}

// ---- (mp4) the outgoing set: slots (used, real address) + targets without a slot (pinned always, learned for HELLOs) ----
static int net_dests(NetAddr *out, bool with_learned) {
    int n = 0;
    for (int k = 0; k < NET_MAX_PEERS; k++)
        if (NET.slot[k].used && NET.slot[k].ip) out[n].ip = NET.slot[k].ip, out[n].port = NET.slot[k].port, n++;
    for (int i = 0; i < NET.ntg; i++)
        if ((NET.tg[i].pinned || with_learned) && net_slot_at(NET.tg[i].ip, NET.tg[i].port) < 0)
            out[n].ip = NET.tg[i].ip, out[n].port = NET.tg[i].port, n++;
    return n;
}
#define NET_DESTS_MAX (NET_MAX_PEERS + NET_TG_MAX)
// (s187) one packet to every peer: the UDP mesh sends it to each address of d[]; a Melty room takes it ONCE and hands it to
// the others (a SNAP / ESYNC as a snapshot, anything else as a reliable event: runtime/melty/sora_melty.h)
static void net_send_all(const NetAddr *d, int nd, const uint8_t *buf, int n) {
    if (n <= 0) return;
    if (NET.melty) {
        if (!sora_melty_connected()) { NET.st.melty_wait++; return; }
        if (sora_melty_send(SORA_MELTY_EVERYONE, buf, n) == n) NET.st.sent++;
        else NET.st.err++;
        return;
    }
    for (int i = 0; i < nd; i++) net_send_to(d[i].ip, d[i].port, buf, n);
}
#include "sora_net_area.h"  // (s191, Melty spec 4.1: NET_AREA, who is on which map; needs NET, net_dests, net_send_all)
#include "sora_net_edmg.h"  // (s166 v6: shared enemy HP; needs NET, net_dests, net_send_to)
#include "sora_net_fire.h"  // (s176 v10: fire sync; needs NET, net_dests, net_send_to, NetEdOut)
#define NET_ESYNC_GLUE
#include "sora_net_esync.h"  // (s168 v7: enemies act the same on every game; default OFF)
#include "sora_esa.h"  // (s175: the R2 attack runner in the DLL, `set esyncatk 2`)
#include "sora_esa_probe.h"  // (s176 R2 diag: `set esaprobe 1` = does the club hit turn on?)
#define NET_REACT_GLUE
#include "sora_net_react.h"  // (s194: the KH2 hit reaction on an enemy another game owns, `set netreact 1`; needs NETES)
#define NET_KILLS_GLUE
#include "sora_net_kills.h"  // (s196: kills stay dead after a reload / for a joining game, `set killsync 0|1`, default 1 since s197; needs NETED)

static inline uint8_t net_oct0(uint32_t ip_be) { return *(const uint8_t *)&ip_be; }
// an address worth HELLOing from a list sent by `from_ip`: not 0.x, 255.255.255.255, multicast 224-239.x, port 0; 127.x only from a 127.x sender
static bool net_addr_learnable(uint32_t ip, uint16_t port, uint32_t from_ip) {
    uint8_t o = net_oct0(ip);
    if (!port || o == 0 || ip == 0xFFFFFFFFu || (o >= 224 && o <= 239)) return false;
    if (o == 127 && net_oct0(from_ip) != 127) return false;
    return true;
}
// a loopback address is only listed to a loopback destination
static bool net_addr_listable_to(uint32_t ip, uint32_t to_ip) { return net_oct0(ip) != 127 || net_oct0(to_ip) == 127; }

// ---- (mp4 review) addresses net_learn must not take: our own (seen as an echo) and, later, a 2nd address of a live peer ----
static int net_alias_find(uint32_t ip, uint16_t port) {
    for (int i = 0; i < NET.nalias && i < 12; i++) if (NET.alias[i].ip == ip && NET.alias[i].port == port) return i;
    return -1;
}
static void net_alias_add(uint32_t ip, uint16_t port, int player) {  // player -1 = our own address
    int i = net_alias_find(ip, port);
    if (i < 0) i = NET.nalias++ % 12;
    NET.alias[i].ip = ip, NET.alias[i].port = port, NET.alias[i].player = player;
}
// (a pin-exclusive table: with a cap of 1 and pins set, only a pinned address may take the slot - the Deck keeps its PC)
static bool net_join_ok(uint32_t ip, uint16_t port, int cap) {
    if (cap > 1 || net_npins() == 0) return true;
    int t = net_tg_find(ip, port);
    return t >= 0 && NET.tg[t].pinned;
}
static void net_alias_drop_player(int player) {  // his slot left: his 2nd addresses are free again
    for (int i = 0; i < NET.nalias && i < 12; i++)
        if (NET.alias[i].player == player) NET.alias[i].ip = 0, NET.alias[i].port = 0, NET.alias[i].player = -2;
}

// HELLO for `to` (NULL = broadcast): our name + up to 3 LIVE peers, never `to` itself.
// (mp4 review) The broadcast carries NO list: a PC on the same LAN would learn its own address from it and ping-pong HELLOs.
static int net_pack_hello_for(const NetAddr *to, uint8_t *b, int cap) {
    NetAddr pa[NET_HELLO_PEERS];
    int np = 0;
    for (int k = 0; k < NET_MAX_PEERS && np < NET_HELLO_PEERS; k++) {
        const NetPeerSlot *s = &NET.slot[k];
        if (!s->used || !s->ip || s->p.st != PEER_LIVE) continue;
        if (!to) break;
        if (to->ip == s->ip && to->port == s->port) continue;
        if (!net_addr_listable_to(s->ip, to->ip)) continue;  // (mp4 review: never 127.x to a LAN / internet peer)
        pa[np].ip = s->ip, pa[np].port = s->port, np++;
    }
    return net_pack_hello_peers(NET.me, ++NET.seq, NET.name, pa, np, b, cap);
}

static void net_send_hellos(void) {
    if (NET.melty) return;  // (s187: the room says who is here)
    uint8_t b[NET_PKT_MAX];
    NetAddr d[NET_DESTS_MAX];
    int n = net_dests(d, true);
    for (int i = 0; i < n; i++) net_send_to(d[i].ip, d[i].port, b, net_pack_hello_for(&d[i], b, sizeof b));
    // LAN discovery: only without pins, while a slot is free (every round: 1 s while alone, 5 s once someone is here)
    if (net_npins() == 0 && netslot_count(NET.slot, NET_MAX_PEERS) < NET_MAX_PEERS)
        net_send_to(0xFFFFFFFFu, NET.port, b, net_pack_hello_for(NULL, b, sizeof b));
}

static void net_send_bye_all(void) {
    if (NET.melty) return;  // (s187: the room says who left)
    uint8_t b[NET_PKT_MAX];
    NetAddr d[NET_DESTS_MAX];
    int n = net_dests(d, false);
    for (int i = 0; i < n; i++) net_send_to(d[i].ip, d[i].port, b, net_pack_bye(NET.me, ++NET.seq, b, sizeof b));
}

// ---- (mp4) per-slot upkeep ----
// his fx copies killed, queued events dropped (the avatar / party / plate / ribbon of the slot follow in net_slot_free)
static void net_slot_drop(int k) {
    NetPeerSlot *s = &NET.slot[k];
    for (int i = 0; i < NET_RFX_N; i++) {
        NetRemoteFx *r = &s->rfx[i];
        if (r->h1 && fx_mine(r->h1 - 1, r->gen)) sora_fx_kill(r->h1 - 1);
        r->h1 = 0;
    }
    s->ninq = 0;
}
// leave: BYE / 10 s quiet / net off. The slot's avatar, party, plate, ribbon, fx copies and inq go; the slot is free again.
static void net_slot_free(int k) {
    if (NET.slot[k].used && NET.slot[k].p.st != PEER_NONE) net_alias_drop_player(NET.slot[k].p.player);
    net_slot_drop(k);
    avatar_slot_off(k);
    net_party_slot_off(k);
    memset(&NET.slot[k], 0, sizeof NET.slot[k]);
}
static void net_slots_free_all(void) { for (int k = 0; k < NET_MAX_PEERS; k++) net_slot_free(k); }

// (s207) the solo build (see the top of this file)
static bool net_solo(void) {
    const char *s = getenv("SAVAGEWIND_SORA_SOLO");
    return s && atoi(s) != 0;
}
static bool g_net_solo_said;  // ("net stays off" said once per DLL copy)

static void net_set_on(bool on) {
    if (on == NET.on) return;
    if (on) {
        if (net_solo()) {  // (s207: nothing turns co-op on in the solo build)
            if (!g_net_solo_said) g_net_solo_said = true, LOG("[net] solo build: co-op is off, net stays off (no room, no socket)\n");
            return;
        }
        if (!NET.port) NET.port = NET_PORT;
        if (!NET.name[0]) net_name_set(NULL);
        if (!NET.melty && !nsock_open(&NET.sock, NET.port, true)) {  // (s187: in a Melty room there is no socket and no port)
            if (!NET.fail_logged)
                LOG("[net] socket open failed on UDP port %u: net stays off (another copy on this PC? its port is exclusive:"
                    " SAVAGEWIND_SORA_NET_PORT / \"set netport N\")\n", NET.port);
            NET.fail_logged = true;
            return;
        }
        NET.on = true, NET.me = net_roll_id(0);
        if (NET.melty && (uint8_t)NET.melty_self) NET.me = (uint8_t)NET.melty_self;  // (s188: back on in the same seat = our room id again; net_melty_pump sets it only when the seat changes)
        NET.next_snap = NET.next_hello = 0, NET.nevq = 0;
        memset(NET.fxo, 0, sizeof NET.fxo), NET.fxrr = 0;  // (mp2: no old-seq moves / kills after a net 0 -> 1)
        memset(&NET.st, 0, sizeof NET.st);
        net_edmg_reset(GetTickCount64());
        net_fire_reset(GetTickCount64());
        net_area_reset(GetTickCount64());  // (s191)
        net_react_reset(GetTickCount64());  // (s194)
        net_kills_reset(GetTickCount64());  // (s196: asks / answers of an earlier session go; the kill list stays)
        for (int i = 0; i < NET.ntg; i++) if (!NET.tg[i].pinned) net_tg_del(i--);  // (learned addresses are stale)
        NET.nalias = 0, NET.learn_hello_at = 0;
        if (NET.melty) LOG("[net] on: Melty Net room (no UDP port), player %u, name \"%s\", up to %d peers%s\n", NET.me, NET.name, NET_MAX_PEERS, g_net_loop ? ", loopback" : "");
        else LOG("[net] on: UDP port %u, player %u, name \"%s\", %d pin(s)%s, up to %d peers%s\n", NET.port, NET.me, NET.name,
            net_npins(), net_npins() ? "" : " (discovery)", NET_MAX_PEERS, g_net_loop ? ", loopback" : "");
        net_kills_on_log();  // (s197: killsync default 1 = one line per net on)
    } else {
        net_send_bye_all();
        nsock_close(&NET.sock);
        NET.on = false;
        net_slots_free_all();
        NET.nevq = 0, NETED.nq = 0;
        net_react_clear();  // (s194)
        net_kills_stop_asks();  // (s196)
        net_esync_stop();  // (s168: every enemy brain we stopped runs again)
        if (NET.melty) LOG("[net] off: still in the Melty room, sending nothing (sent %u, recv %u)\n", NET.st.sent, NET.st.recv);
        else LOG("[net] off: BYE sent, socket closed (sent %u, recv %u)\n", NET.st.sent, NET.st.recv);
    }
}

// (mp2) Sora mode off (!g_on) while net is on: the net steps stop, so say BYE, drop his fx copies (they would freeze) and
// close the socket exactly like net 0; net_frame (Sora back on) resumes it. Called every frame while Sora is off: cheap.
static void net_melty_pump(uint64_t now);
static void net_clock_frame(void);
static void net_sora_off(void) {
    if (NET.melty) net_melty_pump(GetTickCount64());  // (s187: the room client is polled every frame, also while we send nothing)
    net_clock_frame();  // (s191: the room's clock also while Sora mode is off)
    if (!NET.on) return;
    LOG("[net] Sora mode off: leaving the session (resumes with Sora)\n");
    net_set_on(false);
    av_drop_events();
    NET.parked = true;
}

// "ip[:port],ip[:port],..." -> pins (port default NET_PORT)
static int net_pin_list(const char *txt, uint64_t now) {
    int added = 0;
    char item[48];
    while (txt && *txt) {
        size_t n = 0;
        while (*txt == ',' || *txt == ' ') txt++;
        while (txt[n] && txt[n] != ',' && n < sizeof item - 1) item[n] = txt[n], n++;
        item[n] = 0, txt += n;
        if (!n) continue;
        uint32_t ip; uint16_t port;
        char t[16];
        if (!nsock_parse_addr(item, &ip, &port)) LOG("[net] pin \"%s\": not an IPv4 address[:port]\n", item);
        else if (!net_tg_add(ip, port ? port : NET_PORT, true, now)) LOG("[net] pin %s: the pin list is full (%d)\n", item, NET_MAX_PEERS);
        else added++, LOG("[net] peer pinned to %s:%u\n", net_ip_txt(ip, t), port ? port : NET_PORT);
    }
    return added;
}

// (s209) The HELLO note's who-am-I, given to the connector before sora_melty_start (sora_net_note.h top): the version is
// the release id in the game folder's .melty-ready.json (Melty's setup writes it: SORA_ROOT is that folder), else "dev".
static void net_note_setup(void) {
    char text[1024] = "", ver[64], pay[32], path[600];
    const char *root = getenv("SORA_ROOT");
    if (root && *root) {
        size_t n = strlen(root);
        bool sep = n && (root[n - 1] == '/' || root[n - 1] == 0x5c);  // (0x5c: a backslash)
        snprintf(path, sizeof path, "%s%s.melty-ready.json", root, sep ? "" : "/");
        FILE *f = fopen(path, "rb");
        if (f) {
            size_t got = fread(text, 1, sizeof text - 1, f);
            text[got] = 0;
            fclose(f);
        }
    }
    nnote_version(ver, (int)sizeof ver, text, NET_VERSION);
    snprintf(pay, sizeof pay, "kairi-sn%d", NET_VERSION);
    sora_melty_addon_setup(NNOTE_ADDON, ver, pay, 1.0f);  // (1 m a unit: BotW's world units are metres, sora_melty.h)
}

// (s188 G1) Melty Net is asked ONLY when Melty launched this copy: MELTY_NET_DIR is set (Melty's launcher sets it), or
// SAVAGEWIND_SORA_MELTY=1; SAVAGEWIND_SORA_MELTY=0 = never. sora_melty_start alone would also say yes for a leftover
// %LOCALAPPDATA%/LegendOfKairi/melty.properties: an install from GitHub must never go online because of an old file.

static bool net_melty_wanted(void) {
    const char *m = getenv("SAVAGEWIND_SORA_MELTY"), *d = getenv("MELTY_NET_DIR");
    if (m && !strcmp(m, "0")) return false;
    if (m && !strcmp(m, "1")) return true;
    while (d && (*d == ' ' || *d == '\t')) d++;
    return d && *d;
}

static void net_init_from_env(void) {
    if (NET.inited) return;
    NET.inited = true;
    NET.port = NET_PORT;
    const char *pe = getenv("SAVAGEWIND_SORA_NET_PORT");  // (mp2 review: a 2nd copy on one PC, the port is exclusive)
    if (pe && atoi(pe) >= 1024 && atoi(pe) <= 65535) NET.port = (uint16_t)atoi(pe);
    net_name_set(getenv("SAVAGEWIND_SORA_NET_NAME"));
    if (net_solo()) {  // (s207: no Melty room, no pins, no UDP)
        g_net_solo_said = true;
        LOG("[net] solo build: co-op is off (SAVAGEWIND_SORA_SOLO=1: no Melty room, no UDP port, no peers)\n");
        return;
    }
    const char *on = getenv("SAVAGEWIND_SORA_NET");
    bool melty_failed = false;  // (s189 review S2)
    // (s187) Melty Net: asked once per DLL copy, and only when Melty launched us (net_melty_wanted). 1 = its contract folder
    // has melty.properties and the client started; then multiplayer is on without any switch (SAVAGEWIND_SORA_NET=0 alone
    // keeps this copy out of the room: solo).
    if (!(on && atoi(on) == 0) && net_melty_wanted()) {
        net_note_setup();  // (s209: the HELLO note's add-on, version, payload format, metres a unit)
        if (sora_melty_start("LegendOfKairi", "legend-of-kairi", NET.name) == 1) {
            NET.melty = true, NET.melty_since = GetTickCount64();
            LOG("[net] melty: started by Melty: the room carries the packets (no UDP port, no IP list)\n");
            net_set_on(true);
            return;
        }
        char line[512];
        while (sora_melty_log_next(line, (int)sizeof line)) LOG("%s\n", line);
        sora_melty_stop();  // (nothing of the client stays behind)
        melty_failed = true;
    }
    net_pin_list(getenv("SAVAGEWIND_SORA_NET_PEERS"), GetTickCount64());  // (mp4: launcher / friends)
    // (s189 review S2) A Melty launch whose client did not start follows net_melty_drop's rule: UDP only with a pin list.
    // Under Melty SAVAGEWIND_SORA_NET=1 is a default of the exe's (main.c), not the player's wish for LAN discovery: no
    // UDP port (and no firewall prompt) behind a Melty player's back. "set net 1" still turns LAN play on by hand.
    bool udp = on && atoi(on) == 1 && (!melty_failed || net_npins() > 0);
    if (melty_failed)
        LOG("[net] melty: launched by Melty, but Melty Net did not start (no melty.properties in its folder, or see above): %s\n",
            udp ? "UDP with your peer list" : "playing solo (no UDP port opened)");
    if (udp) net_set_on(true);
}

static int net_push_ev(int kind, int id, const float pos[3], float yaw, float scale, int anchor) {  // -> the event seq
    if (NET.nevq == NET_EVQ) {  // a burst bigger than the queue: the oldest gives way
        memmove(NET.evq, NET.evq + 1, sizeof NET.evq[0] * (NET_EVQ - 1));
        memmove(NET.evrep, NET.evrep + 1, NET_EVQ - 1);
        NET.nevq--, NET.st.lost_out++;
    }
    NetEvent *e = &NET.evq[NET.nevq];
    memset(e, 0, sizeof *e);
    e->seq = NET.evseq++, e->kind = (uint8_t)kind, e->id = (uint16_t)id, e->yaw = yaw, e->scale = scale, e->anchor = (uint8_t)anchor;
    const float *p = pos ? pos : NET.feet;
    e->pos[0] = p[0], e->pos[1] = p[1], e->pos[2] = p[2];
    NET.evrep[NET.nevq++] = NET_EVENT_REPEAT;
    NET.st.ev_sent++;
    return e->seq;
}
// pos / yaw are in the anchor's frame when anchor != NANC_WORLD (slice 3)
static int net_note_fx(int id, const float pos[3], float yaw, float scale, int anchor) {  // -> the event seq, -1 = not sent
    return NET.on && id >= 0 ? net_push_ev(0, id, pos, yaw, scale, anchor) : -1;
}
static void net_note_sfx(int id, float gain, const float pos[3]) {  // pos NULL = at Sora's feet
    if (NET.on && id >= 0) net_push_ev(1, id, pos, 0, gain, NANC_WORLD);
}
// menu / HUD sounds only the local player hears (sora_sfx_event names)
static bool net_ui_sfx(const char *n) {
    static const char *ui[] = {"cursor", "decide", "submenu", "cancel", "error", "page", "lock_on", "lock_off", "danger",
                               "game_over", "drive_bar", "drive_full", "form_level_up", "level_up", "bonus_up", "item"};
    for (size_t i = 0; i < sizeof ui / sizeof ui[0]; i++)
        if (!strcmp(n, ui[i])) return true;
    return false;
}

// (mp2) our copy of his fx under event seq `r`: still showing = kill it (a seq wrap / eviction must not orphan it)
static void net_rfx_free(NetRemoteFx *r) {
    if (r->h1 && fx_mine(r->h1 - 1, r->gen)) sora_fx_kill(r->h1 - 1);
    r->h1 = 0;
}
// (mp2) remote fx copies hold at most NET_RFX_MAX of the 32 fx slots, so a remote burst cannot push our own one-shots out.
// Policy: before a remote fx is spawned the OLDEST live remote copy(ies) are killed down to NET_RFX_MAX - 1 (newest wins);
// local fx are never touched. Copies that ended on their own (slot dead or reused) do not count.
// (mp4) NET_RFX_MAX stays the TOTAL across all peers: the oldest copy of ANY peer goes first.
#define NET_RFX_MAX 8
static void net_rfx_room(void) {
    for (;;) {
        int live = 0, old = -1, oldk = 0;
        for (int k = 0; k < NET_MAX_PEERS; k++)
            for (int i = 0; i < NET_RFX_N; i++) {
                NetRemoteFx *r = &NET.slot[k].rfx[i];
                if (!r->h1) continue;
                if (!fx_mine(r->h1 - 1, r->gen) || !sora_fx_alive(r->h1 - 1)) { r->h1 = 0; continue; }
                live++;
                if (old < 0 || (int32_t)(r->born - NET.slot[oldk].rfx[old].born) < 0) old = i, oldk = k;
            }
        if (live < NET_RFX_MAX) return;
        net_rfx_free(&NET.slot[oldk].rfx[old]);
    }
}

static void net_take_snap_events(int k, const NetSnap *s, uint64_t now) {
    NetPeerSlot *sl = &NET.slot[k];
    NetEvent got[NET_MAX_EVENTS];
    int n = peer_take_events(&sl->p, s, got);
    for (int i = 0; i < n; i++) {
        if (sl->ninq == NET_INQ) memmove(sl->inq, sl->inq + 1, sizeof sl->inq[0] * (NET_INQ - 1)), sl->ninq--, sl->st.lost_in++, NET.st.lost_in++;
        sl->inq[sl->ninq].ev = got[i], sl->inq[sl->ninq].at = now, sl->ninq++;
        sl->st.ev_fresh++, NET.st.ev_fresh++;
        NET.st.anc_ev_in[got[i].anchor & 3] += got[i].kind == 0;
        if (got[i].kind == 0) {  // (a new fx under this seq; an 8-bit seq wrap onto a live copy kills it first)
            NetRemoteFx *r = &sl->rfx[got[i].seq];
            net_rfx_free(r);
            memset(r, 0, sizeof *r);
            r->born = ++NET.rfx_born;
        }
    }
}
// (Task 8) his moving fx: places into the tracks, kills / fadeouts pending (the avatar applies them NET_INTERP_MS later)
static void net_take_snap_moves(int k, const NetSnap *s, uint64_t now) {
    for (int i = 0; i < s->nmv; i++) {
        const NetFxMove *m = &s->mv[i];
        NetRemoteFx *r = &NET.slot[k].rfx[m->ev];
        if (r->kill) continue;
        if (m->flags & (NMV_KILL | NMV_FADE)) r->kill = true, r->kflags = m->flags, r->fade = m->fade, r->kill_at = now;
        else fxtrack_push_anc(&r->tr, m->anchor, now, m->pos, m->yaw), NET.st.anc_mv_in[m->anchor & 3]++;  // (slice 3: an anchored place is an offset)
    }
}

// a packet from an address with no slot takes a free one (-1 = full: logged once per address)
static int net_slot_join(uint32_t ip, uint16_t port, int player, const char *name, int type, uint64_t now) {
    char t[16];
    int k = net_join_ok(ip, port, NET_MAX_PEERS) ? netslot_join(NET.slot, NET_MAX_PEERS, ip, port, player, name, now) : -1;
    if (k < 0) {
        bool seen = false;
        for (int i = 0; i < NET.nrefused && i < 8; i++) seen = seen || (NET.refused[i].ip == ip && NET.refused[i].port == port);
        if (!seen) {
            NET.refused[NET.nrefused++ & 7] = (NetAddr){ip, port};
            if (!net_join_ok(ip, port, NET_MAX_PEERS)) LOG("[net] %s:%u refused: this copy is pinned to its own peer (net peer off clears the pins)\n", net_ip_txt(ip, t), port);
            else LOG("[net] %d players already: %s:%u refused\n", NET_MAX_PEERS + 1, net_ip_txt(ip, t), port);
        }
        return -1;
    }
    int tg = net_tg_find(ip, port);
    if (tg >= 0 && !NET.tg[tg].pinned) net_tg_del(tg);  // (the slot covers a learned address)
    if (NET.melty) LOG("[net] peer %d joined: %s (Melty room peer %u)\n", k + 1, name[0] ? name : "(no name)", (unsigned)nmelty_peer(ip));
    else LOG("[net] peer %d joined: %s %s:%u\n", k + 1, type == NET_HELLO && name[0] ? name : "(snapshot)", net_ip_txt(ip, t), port);
    NET.next_hello = now;  // (answer at once: it learns our name + list)
    return k;
}

// ---- (s187) Melty Net room: polled once a frame, before anything is received or sent ----
// The room's joins / leaves make and free the peer slots (HELLO / BYE / the 10 s timeout do that on UDP), its log lines
// go to ours as "[net] melty: ...". While net is off (set net 0, Sora mode off) the client is still polled and what
// came is thrown away, so nothing piles up.
#define NET_MELTY_WAIT_MS 60000  // (s188 G2) no seat in the room this long after the start: give up on Melty Net
// (s188 G2) The room refused us, closed for good, or never answered: the client is stopped and this copy is back on the
// mod's own UDP path, so multiplayer is never silently dead. UDP comes on only with a pin list (SAVAGEWIND_SORA_NET_PEERS:
// a player who also has a Tailscale / LAN setup); a plain Melty player just plays solo then (no UDP port is opened behind
// his back: "set net 1" still turns LAN discovery on by hand).
static void net_melty_drop(const char *why) {
    char line[512];
    while (sora_melty_log_next(line, (int)sizeof line)) LOG("%s\n", line);
    bool parked = NET.parked, was = NET.on || parked;  // (s189 review S1: parked = Sora mode off, net resumes with Sora)
    if (NET.on) net_set_on(false);  // (frees every slot, stops the enemy sync; no BYE: still NET.melty)
    sora_melty_stop();
    NET.melty = false, NET.melty_self = 0, NET.nmroom = 0;
    net_pin_list(getenv("SAVAGEWIND_SORA_NET_PEERS"), GetTickCount64());
    bool udp = was && net_npins() > 0;
    LOG("[net] melty: %s: Melty Net is off for this run, %s\n", why,
        udp ? "back to the UDP path (your peer list)" : "playing solo (start the game from Melty again to rejoin)");
    if (!udp) NET.parked = false;  // (s189 S1: or Sora coming back would open the UDP port with LAN discovery)
    else if (!parked) net_set_on(true);  // (parked + a pin list: net_frame turns UDP on when Sora is back)
}
// (s188 G9) this DLL copy is going (sora_mod_unload: hot reload): leave the room now, the client's threads end before the
// new copy starts its own. A host's new copy makes a NEW room (dev only: players run with SAVAGEWIND_SORA_RELOAD=0).
static void net_melty_unload(void) {
    ck_unload();  // (s191: a hold / weather force of the room clock's goes first)
    if (!NET.melty) return;
    sora_melty_stop();
    NET.melty = false, NET.melty_self = 0, NET.nmroom = 0;
    LOG("[net] melty: left the room (this copy of the mod is going)\n");
}

static void net_melty_pump(uint64_t now) {
    sora_melty_frame();
    char line[512];
    while (sora_melty_log_next(line, (int)sizeof line)) LOG("%s\n", line);
    if (sora_melty_state() < 0) { net_melty_drop("the room refused us or closed"); return; }
    int self = sora_melty_self();
    if (!self && !NET.melty_self && now > NET.melty_since + NET_MELTY_WAIT_MS) { net_melty_drop("no seat in the room after 60 s"); return; }
    if (self && self != NET.melty_self) {  // a new seat in the room (the first welcome, or a new room after the old one closed)
        if (NET.melty_self) {
            LOG("[net] melty: new seat in the room (peer %u -> %d): every slot starts over\n", (unsigned)NET.melty_self, self);
            net_slots_free_all();
            NET.nmroom = 0;  // (the room names everyone again after a welcome)
        }
        NET.melty_self = (uint16_t)self;
        NET.me = (uint8_t)self;  // (room ids are unique: no two copies roll the same player byte; a clash past 255 re-rolls as on UDP)
        if (!NET.me) NET.me = net_roll_free(0);
    }
    if (!self && !NET.melty_slow_logged && now > NET.melty_since + 20000) {
        NET.melty_slow_logged = true;
        LOG("[net] melty: not in the room after 20 s (still trying for 40 s more; see the lines above)\n");
    }
    sora_melty_peer_event ev;
    while (sora_melty_peer(&ev)) {  // the roster: joins and leaves, kept also while net is off
        int r = -1;
        for (int i = 0; i < NET.nmroom; i++) if (NET.mroom[i].id == ev.peer) r = i;
        if (!ev.joined) {
            if (r >= 0) memmove(&NET.mroom[r], &NET.mroom[r + 1], sizeof NET.mroom[0] * (size_t)(NET.nmroom - r - 1)), NET.nmroom--;  // (s198: join order kept)
            int k = net_slot_at(nmelty_ip(ev.peer), NMELTY_PORT);
            if (k >= 0) {
                LOG("[net] peer %d \"%s\" left (Melty room)\n", k + 1, NET.slot[k].p.name);
                net_slot_free(k);
            }
            continue;
        }
        if (ev.peer == self || !ev.peer) continue;
        if (r < 0 && NET.nmroom < 16) r = NET.nmroom++, NET.mroom[r].said = false;
        if (r < 0) continue;  // (more players than the roster holds: no slot for him either)
        NET.mroom[r].id = ev.peer;
        nmelty_name(NET.mroom[r].name, (int)sizeof NET.mroom[r].name, ev.name, ev.peer);
    }
    // every room player has a slot while net is on. (s198) A full table: he waits, said ONCE per roster entry (not through
    // net_slot_join's 8-address list, which a crowd of more than 8 waiting players went round every frame); a free slot goes
    // to the one who has waited longest (the roster is in join order). Waiting = no slot: not drawn, every packet of his dropped.
    for (int i = 0; i < NET.nmroom && NET.on && !g_net_loop; i++) {
        uint32_t ip = nmelty_ip(NET.mroom[i].id);
        if (net_slot_at(ip, NMELTY_PORT) >= 0) continue;
        if (netslot_count(NET.slot, NET_MAX_PEERS) >= NET_MAX_PEERS) {
            if (!NET.mroom[i].said)
                NET.mroom[i].said = true, LOG("[net] %d players already: %s (Melty room peer %u) refused: not shown, his packets dropped until a slot is free\n",
                                              NET_MAX_PEERS + 1, NET.mroom[i].name, (unsigned)NET.mroom[i].id);
            continue;
        }
        int k = net_slot_join(ip, NMELTY_PORT, (uint8_t)NET.mroom[i].id, NET.mroom[i].name, NET_HELLO, now);
        if (k >= 0) peer_on_hello(&NET.slot[k].p, (uint8_t)NET.mroom[i].id, NET.mroom[i].name, now);
    }
    if (!NET.on) {  // (off: what the others sent is not ours to play)
        uint8_t buf[NET_PKT_MAX + 64];
        uint16_t from;
        for (int i = 0; i < 256 && sora_melty_recv(&from, buf, (int)sizeof buf) > 0; i++) {}
    }
}

// a HELLO's listed address we do not know yet becomes a learned target (HELLOed until it answers = becomes a slot)
static void net_learn(const NetAddr *a, uint32_t from_ip, uint16_t from_port, uint64_t now) {
    if (netslot_count(NET.slot, NET_MAX_PEERS) >= NET_MAX_PEERS) return;
    if ((a->ip == from_ip && a->port == from_port) || net_slot_at(a->ip, a->port) >= 0 || net_tg_find(a->ip, a->port) >= 0) return;
    if (net_alias_find(a->ip, a->port) >= 0 || !net_addr_learnable(a->ip, a->port, from_ip)) return;  // (mp4 review: our own address / an alias / unusable)
    if (net_tg_add(a->ip, a->port, false, now)) {
        char t[16];
        LOG("[net] learned peer address %s:%u (from a HELLO list)\n", net_ip_txt(a->ip, t), a->port);
        if (now >= NET.learn_hello_at + 1000 || now < NET.learn_hello_at) NET.next_hello = now, NET.learn_hello_at = now;  // (mp4 review: at most once a second)
    }
}

static void net_recv_all(uint64_t now) {
    uint8_t buf[NET_PKT_MAX + 64];
    for (int i = 0; i < (NET.melty ? 512 : 64); i++) {  // (s188: a room hands over a stall's backlog at once: read it all, old ones are skipped by seq)
        uint32_t ip = 0; uint16_t port = 0;
        int n;
        uint16_t mfrom = 0;
        if (NET.melty) {  // (s187: the room's packets, byte for byte as the sender's code made them, under his room peer id)
            n = sora_melty_recv(&mfrom, buf, (int)sizeof buf);
            ip = nmelty_ip(mfrom), port = NMELTY_PORT;
        } else n = nsock_recv(&NET.sock, buf, sizeof buf, &ip, &port);
        if (n < 0) { NET.st.err++; break; }
        if (n == 0) break;
        NET.st.recv++;
        int type = 0, npa = 0; uint8_t player = 0; uint16_t seq = 0; char name[NET_NAME_MAX + 1] = {0};
        NetSnap s;
        NetAddr pa[NET_HELLO_PEERS];
        if (!net_unpack_ex(buf, n, &type, &player, &seq, &s, name, pa, &npa)) {
            NET.st.bad++;
            int ov = net_other_version(buf, n);
            static NetVerSeen vseen;
            if (ov >= 0 && net_ver_first(&vseen, ip)) {  // (mp2: once per peer address, not only counted in "bad")
                char t[16];
                if (NET.melty) LOG("[net] packet from Melty room peer %u has protocol version %d, ours is %d: update both copies to the same build\n", (unsigned)mfrom, ov, NET_VERSION);
                else LOG("[net] packet from %s:%u has protocol version %d, ours is %d: update both copies to the same build\n", net_ip_txt(ip, t), port, ov, NET_VERSION);
            }
            continue;
        }
        if (NET.melty) {
            // (s187) the room never hands us our own packets back, and only its joins make slots: HELLO / BYE (an old build's)
            // and anything from a player the room has not announced (or who left) are dropped
            if (type == NET_HELLO || type == NET_BYE || mfrom == NET.melty_self) { NET.st.other++; continue; }
            int mk = net_slot_at(ip, port);
            if (mk < 0) { NET.st.other++; continue; }
            if (player == NET.me) {  // (two room ids with the same low byte: we re-roll, as on UDP)
                NET.me = net_roll_free(player);
                LOG("[net] player id clash with %s: now %u\n", NET.slot[mk].p.name[0] ? NET.slot[mk].p.name : "the peer", NET.me);
            }
        } else if (player == NET.me && (type == NET_EDMG || type == NET_ESYNC || type == NET_FIRE || type == NET_AREA || type == NET_REACT || type == NET_KILLS)) { NET.st.echo++; continue; }  // (s166: its seq is a different counter than the SNAP seq below)
        else if (player == NET.me) {
            // our own broadcast coming back (seq one we just used), or a peer that rolled our id: we re-roll
            if ((uint16_t)(NET.seq - seq) < 256) {
                NET.st.echo++;
                int tg = net_tg_find(ip, port);  // (mp4: a learned address that is ourselves)
                if (tg >= 0 && !NET.tg[tg].pinned) net_tg_del(tg);
                net_alias_add(ip, port, -1);  // (mp4 review: net_learn never takes our own address again)
                continue;
            }
            NET.me = net_roll_free(player);
            LOG("[net] player id clash with %s: now %u\n", name[0] ? name : "the peer", NET.me);
        }
        if (g_net_loop) { NET.st.other++; continue; }  // (loopback: slot 0 is ours)
        int k = netslot_find(NET.slot, NET_MAX_PEERS, ip, port, player);
        if (k < 0) {  // (mp4 review: the same player from a 2nd address (gossip / NAT hairpin) is his slot, not a new one)
            int al = net_alias_find(ip, port), own = -1;
            for (int j = 0; j < NET_MAX_PEERS; j++) {
                const NetPeerSlot *o = &NET.slot[j];
                if (o->used && o->p.st != PEER_NONE && o->p.player == player && (!name[0] || !o->p.name[0] || !strcmp(name, o->p.name))) own = j;
            }
            if (own >= 0 && (al < 0 || NET.alias[al].player == player)) {
                if (al < 0) {
                    char t[16];
                    int tg = net_tg_find(ip, port);
                    if (tg >= 0 && !NET.tg[tg].pinned) net_tg_del(tg);
                    net_alias_add(ip, port, player);
                    LOG("[net] %s:%u is a 2nd address of peer %d \"%s\": same slot\n", net_ip_txt(ip, t), port, own + 1, NET.slot[own].p.name);
                }
                if (type == NET_BYE) continue;  // (he leaves through his real address too)
                k = own;
            }
        }
        if (k < 0) {
            if (type == NET_BYE || type == NET_EDMG || type == NET_ESYNC || type == NET_FIRE || type == NET_AREA || type == NET_REACT || type == NET_KILLS) continue;  // (a stranger leaving / a hit from nobody we know)
            k = net_slot_join(ip, port, player, name, type, now);
            if (k < 0) { NET.st.other++; continue; }
        } else if (NET.slot[k].ip == ip && NET.slot[k].port != port) {
            char t[16];
            LOG("[net] peer %d moved to %s:%u\n", k + 1, net_ip_txt(ip, t), port);
            NET.slot[k].port = port;
        }
        NetPeerSlot *sl = &NET.slot[k];
        NET.last_recv_ms = sl->last_recv_ms = now;
        sl->st.recv++;
        if (type == NET_HELLO) {
            bool fresh = sl->p.st == PEER_NONE, had = sl->p.name[0] != 0;
            peer_on_hello_seq(&sl->p, player, name, seq, now);
            if (fresh || (!had && sl->p.name[0])) LOG("[net] hello from \"%s\" (player %u, peer %d)\n", sl->p.name, player, k + 1);
            if (fresh) NET.next_hello = now, memset(&sl->ed, 0, sizeof sl->ed), memset(&sl->rc, 0, sizeof sl->rc), memset(&sl->fi, 0, sizeof sl->fi), memset(&sl->es, 0, sizeof sl->es), memset(&sl->esev, 0, sizeof sl->esev);  // (a restarted peer counts its packet seqs anew)
            for (int j = 0; j < npa; j++) net_learn(&pa[j], ip, port, now);
        } else if (type == NET_SNAP) {
            bool cur = peer_on_snap(&sl->p, &s, now);
            sl->st.snap++, sl->last_snap_ms = now;
            if (!cur) NET.st.stale++, sl->st.stale++;
            net_take_snap_events(k, &s, now);
            if (cur) net_take_snap_moves(k, &s, now);  // (a stale SNAP's places are older than the track's)
        } else if (type == NET_EDMG) {
            net_edmg_recv(k, buf, n, now);
        } else if (type == NET_FIRE) {
            net_fire_recv(k, buf, n);
        } else if (type == NET_ESYNC) {
            net_esync_recv(k, buf, n);
        } else if (type == NET_AREA) {
            net_area_recv(k, buf, n, now);  // (s191)
        } else if (type == NET_REACT) {
            net_react_recv(k, buf, n, now);  // (s194)
        } else if (type == NET_KILLS) {
            net_kills_recv(k, buf, n, now);  // (s196)
        } else if (type == NET_BYE) {
            LOG("[net] peer %d \"%s\" left (BYE)\n", k + 1, sl->p.name);
            net_slot_free(k);
        }
    }
}

// (s190, Melty spec 4.1) the map we are on, the same text in every copy (sora_area.h area_now; NULL / "" = not known).
// (s191) It goes to the other games as NET_AREA (sora_net_area.h), over UDP and in a room; no other packet changes.
// (s209 step 2) In a Melty room a change to a known area is Melty's BEAT note "level" (detail = the area).
static void net_set_area(const char *a) {
    char was[sizeof NET.area];
    memcpy(was, NET.area, sizeof was);
    snprintf(NET.area, sizeof NET.area, "%s", a ? a : "");
    if (NET.melty && NET.area[0] && strcmp(was, NET.area)) sora_melty_addon_beat("level", 0, NET.area);
}

static void net_send_snap(uint64_t now, bool loading) {
    NetSnap s;
    memset(&s, 0, sizeof s);
    net_fill(&s);
    s.player = NET.me, s.seq = ++NET.seq;
    if (loading) s.flags |= NF_LOADING;
    for (int k = 0; k < 3; k++) s.feet[k] = NET.feet[k];
    // events: oldest first, up to NET_MAX_EVENTS (12), each in NET_EVENT_REPEAT SNAPs (sent or not: no peer = they lapse)
    for (int i = 0; i < NET.nevq && s.nev < NET_MAX_EVENTS; i++) s.ev[s.nev++] = NET.evq[i], NET.evrep[i]--;
    int keep = 0;
    for (int i = 0; i < NET.nevq; i++)
        if (NET.evrep[i]) NET.evq[keep] = NET.evq[i], NET.evrep[keep++] = NET.evrep[i];
    NET.nevq = keep;
    s.nmv = (uint8_t)net_pick_moves(NET.fxo, NET_FXO_N, &NET.fxrr, s.mv, NET_MAX_MOVES);  // (Task 8: moving fx)
    NET.st.mv_sent += s.nmv;
    uint8_t b[NET_PKT_MAX];
    int n = net_pack_snap(&s, b, sizeof b);
    if (n <= 0) { NET.st.err++; return; }
    if (g_net_loop) {  // pack -> unpack, as a remote would get it (slot 0)
        int type; uint8_t player; uint16_t seq; char name[NET_NAME_MAX + 1];
        NetSnap r;
        if (!net_unpack(b, n, &type, &player, &seq, &r, name)) { NET.st.bad++; return; }
        NET.st.sent++, NET.st.recv++, NET.last_recv_ms = now;
        NetPeerSlot *sl = &NET.slot[0];
        if (!sl->used) memset(sl, 0, sizeof *sl), sl->used = true, sl->joined_ms = now;
        sl->last_recv_ms = now, sl->st.recv++;
        if (sl->p.st == PEER_NONE || sl->p.player != player) peer_on_hello(&sl->p, player, NET.name, now);
        bool cur = peer_on_snap(&sl->p, &r, now);
        if (!cur) NET.st.stale++, sl->st.stale++;
        net_take_snap_events(0, &r, now);
        if (cur) net_take_snap_moves(0, &r, now);
        return;
    }
    NetAddr d[NET_DESTS_MAX];  // every known peer (no peer yet: HELLOs only)
    int nd = net_dests(d, false);
    // (s187) the room places us by this (who sees whom); the heading is S.face as it is (0 faces +z, turning towards +x).
    // (s190, spec 4.1) NET.area = the map we are on (sora_area.h: "MainField", a shrine's or Divine Beast's name; "" = not
    // known = one world): the room shows a player only to players in the same area.
    if (NET.melty) sora_melty_where(s.feet[0], s.feet[1], s.feet[2], s.yaw, NET.area);
    net_send_all(d, nd, b, n);  // (in a Melty room also with nobody here yet: our SNAP is how the room knows where we are)
}

// once per frame; link_pos NULL = no world / loading / menu (SNAPs carry NF_LOADING and the last feet)
static void net_tick(const float link_pos[3], uint64_t now) {
    if (!NET.on) return;
    bool loading = !link_pos || NET.loading;
    if (link_pos) NET.feet[0] = link_pos[0], NET.feet[1] = link_pos[1], NET.feet[2] = link_pos[2], NET.have_feet = true;
    net_area_tick(now);  // (s191: our map out when due, who is hidden; before the receive, so this frame's FIRE is judged by it)
    net_recv_all(now);
    int live = 0;
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        NetPeerSlot *sl = &NET.slot[k];
        if (!sl->used) continue;
        PeerState was = sl->p.st;
        // (s187) a Melty room player keeps his slot until the room says he left (on UDP his HELLOs every 5 s do that): quiet
        // for long = held faded out (no SNAP for 2 s fades him as ever), never the 10 s drop
        if (NET.melty && was != PEER_NONE && now > sl->p.last_ms + NET_DROP_MS - 1000) sl->p.last_ms = now - (NET_FADE_MS + NET_FADE_OUT_MS + 1);
        peer_tick(&sl->p, now);
        if (was != PEER_NONE && sl->p.st == PEER_NONE) {  // (peer_tick reset it: 10 s quiet)
            LOG("[net] peer %d \"%s\" timed out (10 s quiet)\n", k + 1, sl->p.name);
            net_slot_free(k);
        } else if (sl->p.st == PEER_NONE && !g_net_loop) net_slot_free(k);  // (never went live: nothing to keep)
        else live += sl->p.st == PEER_LIVE;
    }
    for (int i = 0; i < NET.ntg; i++)  // (a learned address that never answered is forgotten; gossip re-adds it)
        if (!NET.tg[i].pinned && now > NET.tg[i].born + NET_TG_LEARN_MS) net_tg_del(i--);
    if (now >= NET.next_hello && !g_net_loop) {
        net_send_hellos();
        bool waiting = live == 0;
        for (int i = 0; i < NET.ntg; i++) waiting = waiting || net_slot_at(NET.tg[i].ip, NET.tg[i].port) < 0;
        NET.next_hello = now + (waiting ? NET_HELLO_MS : NET_HELLO_LIVE_MS);
    }
    if (now >= NET.next_snap && NET.have_feet) {
        NET.next_snap = NET.next_snap && now - NET.next_snap < NET_SNAP_MS ? NET.next_snap + NET_SNAP_MS : now + NET_SNAP_MS;
        net_send_snap(now, loading);
    }
    net_fire_pump(now);  // (s176 v10: fire sync out)
    net_edmg_pump(now);  // (s166 v6: shared enemy HP out; every tick, also while loading)
    net_react_pump(now);  // (s194: hit reactions on enemies another game owns)
    net_kills_tick(loading ? NULL : link_pos, now);  // (s196 killsync: Blood Moon clear, asks / answers, the apply scan on driven frames)
    g_esa.log = g_net_esynclog;
    esa_tick(&g_esa);  // (s175: re-apply `set esyncatk 2` after a hot reload)
    esp_tick(now);     // (s176 R2 diag lines; s178 club ON / OFF sync hooks + counters)
    net_esync_tick(link_pos, now);  // (s168 v7: enemy sync, off unless `set esync 1`)
}
static void net_tick_idle(uint64_t now) { net_tick(NULL, now); }

// sora_mod_frame, after the frame body: driven (Link's pos read this frame) or idle (loading / menu / no world)
static uint32_t g_nd_camp, g_nd_pause, g_nd_nolink;  // s179 diag: why a mod frame was not driven (sora_mod_frame_body returns)
static void net_frame(const float link_pos[3], uint64_t now) {
    net_init_from_env();
    {   // (s179 diag, esynclog: driven frames every 10 s)
        static uint64_t next;
        static uint32_t fr, dr;
        fr++, dr += NET.driven;
        if (g_net_esynclog && now >= next) {
            if (next) LOG("[net] 10 s: mod frames %u, driven %u; not driven: menu/camp %u, paused/no world %u, no Link %u, other %u\n", fr, dr, g_nd_camp, g_nd_pause, g_nd_nolink,
                          fr - dr - g_nd_camp - g_nd_pause - g_nd_nolink);
            next = now + 10000, fr = dr = 0, g_nd_camp = g_nd_pause = g_nd_nolink = 0;
        }
    }
    if (NET.parked) NET.parked = false, net_set_on(true);  // (mp2: Sora is back on)
    if (NET.melty) net_melty_pump(now);  // (s187: the room first: its joins / leaves, then net_tick receives and sends)
    if (NET.driven) net_tick(link_pos, now);
    else net_tick_idle(now);
    NET.driven = false;
    net_clock_frame();  // (s191: the room's story, then the room clock)
}

// (s209) Once a mod frame (sora.c, after the puppets are drawn, also with Sora mode off): this frame's puppet counts
// (g_nnote, from sora_net_avatar.h) and FRAME's flags to the connector, which sends a FRAME note once a second while in the
// room. dt = the time since the last call (QueryPerformanceCounter). The counts are cleared every frame, in a room or not.
static void net_note_frame(int flags) {
    static LARGE_INTEGER f, last;
    NetNoteCount c = g_nnote;
    memset(&g_nnote, 0, sizeof g_nnote);
    if (!NET.melty) { last.QuadPart = 0; return; }
    LARGE_INTEGER t;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    double dt = last.QuadPart && f.QuadPart ? (double)(t.QuadPart - last.QuadPart) / (double)f.QuadPart : 0;
    last = t;
    if (!(dt > 0) || dt > 5) return;  // (the first frame, or a stall: no frame time to give; the counts of this one go)
    sora_melty_addon_frame((float)dt, flags, c.on_screen, c.drawn, c.animated, 0);
}

// (s191) The room's story, drained in ONE place: each change once, in order (a late join's whole story first). Every key goes
// to its owner here (4.3 adds botw.dead: / botw.flag: / botw.taken:); a key nobody owns yet is skipped. The leader's story
// events from guests (sora_melty_trigger_next) are drained here too and skipped until 4.3 owns them.
static void net_story_pump(void) {
    static char key[256];
    static uint8_t val[32768 + 64];  // (a story value is 32 KB at most)
    int len = 0, del = 0;
    for (int i = 0; i < 256 && sora_melty_story_next(key, (int)sizeof key, val, (int)sizeof val, &len, &del); i++) {
        if (!strcmp(key, CLOCK_KEY)) ck_value(val, len, del);
    }
    uint16_t from = 0;
    for (int i = 0; i < 256 && sora_melty_trigger_next(&from, key, (int)sizeof key, val, (int)sizeof val, &len); i++) {}
}
// once a frame (net_frame, net_sora_off): in a Melty room the story, then the clock; outside one ck_frame returns at once
// unless it still holds something (released then) or the fake clock is on
static void net_clock_frame(void) {
    if (NET.melty) net_story_pump();
    ck_frame(NET.melty && sora_melty_connected());
}

static void net_table_log(void) {
    char t[16];
    uint64_t now = GetTickCount64();
    if (NET.melty) LOG("[net] melty: %s, our room peer id %d, %u packet(s) not sent while connecting\n", sora_melty_connected() ? "in the room" : "NOT connected", sora_melty_self(), NET.st.melty_wait);
    LOG("[net] table: %d pin(s), %d target(s), %d of %d peer slots used\n", net_npins(), NET.ntg, netslot_count(NET.slot, NET_MAX_PEERS), NET_MAX_PEERS);
    for (int i = 0; i < NET.ntg; i++)
        LOG("[net]   %s %s:%u%s\n", NET.tg[i].pinned ? "pin    " : "learned", net_ip_txt(NET.tg[i].ip, t), NET.tg[i].port,
            net_slot_at(NET.tg[i].ip, NET.tg[i].port) >= 0 ? " (connected)" : "");
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        const NetPeerSlot *s = &NET.slot[k];
        if (s->used) LOG("[net]   slot %d: \"%s\" player %u %s:%u st %d, last recv %lld ms ago\n", k + 1, s->p.name, s->p.player,
                         net_ip_txt(s->ip, t), s->port, (int)s->p.st, s->last_recv_ms ? (long long)(now - s->last_recv_ms) : -1LL);
    }
}

static void net_diag_log(void) {
    uint64_t now = GetTickCount64();
    LOG("[net] diag: %s, port %u, player %u \"%s\"%s; peers %d/%d, pins %d; sent %u recv %u bad %u stale %u other %u "
        "echo %u err %u; events sent %u fresh %u played %u lost_out %u lost_in %u, queued out %d; last recv %lld ms ago\n",
        NET.on ? "on" : "off", NET.port, NET.me, NET.name, g_net_loop ? " loopback" : "", netslot_count(NET.slot, NET_MAX_PEERS), NET_MAX_PEERS,
        net_npins(), NET.st.sent, NET.st.recv, NET.st.bad, NET.st.stale, NET.st.other, NET.st.echo,
        NET.st.err, NET.st.ev_sent, NET.st.ev_fresh, NET.st.ev_played, NET.st.lost_out, NET.st.lost_in, NET.nevq,
        NET.last_recv_ms ? (long long)(now - NET.last_recv_ms) : -1LL);
    net_table_log();
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        const NetPeerSlot *s = &NET.slot[k];
        if (!s->used) continue;
        LOG("[net] slot %d \"%s\": st %d n %d, recv %u stale %u, events fresh %u played %u lost_in %u, queued in %d\n", k + 1, s->p.name,
            (int)s->p.st, s->p.n, s->st.recv, s->st.stale, s->st.ev_fresh, s->st.ev_played, s->st.lost_in, s->ninq);
        // (s187) one fixed line per slot for Melty's two-copy test (keep its words): is this slot receiving SNAPs?
        LOG("[net] diag slot %d \"%s\" snap rx %u last %lld ms ago\n", k + 1, s->p.name, s->st.snap, s->last_snap_ms ? (long long)(now - s->last_snap_ms) : -1LL);
    }
    LOG("[net] anchors (%s): fx events sent by anchor world %u sora %u donald %u goofy %u; moves sent %u skipped (unchanged) %u; "
        "received events %u/%u/%u/%u, moves %u/%u/%u/%u, party anchor shown at Sora %u\n", g_net_anchor ? "on" : "OFF (set netanchor 1)",
        NET.st.anc_ev[0], NET.st.anc_ev[1], NET.st.anc_ev[2], NET.st.anc_ev[3], NET.st.mv_sent, NET.st.mv_skip,
        NET.st.anc_ev_in[0], NET.st.anc_ev_in[1], NET.st.anc_ev_in[2], NET.st.anc_ev_in[3],
        NET.st.anc_mv_in[0], NET.st.anc_mv_in[1], NET.st.anc_mv_in[2], NET.st.anc_mv_in[3], NET.st.anc_guess);
    LOG("[net] bone: sent %u, recv %u, drawn %u, fallback %u, skipped %u (%s; recv / drawn / fallback count draw frames)\n", g_net_bone_sent, g_net_bone_recv,
        g_net_bone_drawn, g_net_bone_fb, g_net_bone_skip, g_net_bone ? "on" : "OFF (set netbone 1)");
    LOG("[net] skate: sent %u, drawn %u, fallback %u (%s; drawn / fallback count draw frames)\n", g_net_skate_sent, g_net_skate_drawn, g_net_skate_fb,
        g_net_skate ? "on" : "OFF (set netskate 1)");
    net_edmg_diag();
    net_area_diag();
    net_fire_diag();
    net_esync_diag();
    net_react_diag();  // (s194)
    net_kills_diag();  // (s196)
    avatar_diag();
    net_party_diag();
}

// live "set <key> <int>": true = a net key
static bool net_live_set(const char *key, int v) {
    if (!strcmp(key, "net")) { net_init_from_env(); NET.parked = false; net_set_on(v != 0); if (v && !NET.on) LOG(net_solo() ? "[net] still off (solo build)\n" : "[net] still off (socket)\n"); }
    else if (ck_live_set(key, v)) {}  // (s191: mclock)
    else if (net_kills_live_set(key, v)) {}  // (s196: killsync / killsclear / killslog)
    else if (!strcmp(key, "mclockfake")) ck_fake((float)v);  // (s191: whole hours; sora.c reads "set mclockfake 21.5" as a float first)
    else if (!strcmp(key, "netloop")) {
        if (g_net_loop == (v != 0)) return true;  // (a live rewrite re-applies every line: only a change resets the peers)
        g_net_loop = v != 0;
        net_slots_free_all();
        LOG("[net] loopback %s\n", g_net_loop ? "on (own SNAPs -> slot 0)" : "off");
    } else if (!strcmp(key, "netbone")) {
        if (g_net_bone == (v != 0)) return true;
        g_net_bone = v != 0;
        LOG("[net] bone-pose sync %s\n", g_net_bone ? "on" : "off (set netbone 0: no bone block sent, a remote ragdoll / puppet shows idle / fall)");
    } else if (!strcmp(key, "netskate")) {
        if (g_net_skate == (v != 0)) return true;
        g_net_skate = v != 0;
        LOG("[net] skate rider sync %s\n", g_net_skate ? "on" : "off (set netskate 0: a remote rider shows idle on no board)");
    } else if (!strcmp(key, "edmg")) {
        if (g_net_edmg == (v != 0)) return true;
        g_net_edmg = v != 0;
        LOG("[net] shared enemy HP %s\n", g_net_edmg ? "on" : "off (set edmg 0: nothing sent, received hits ignored)");
    } else if (!strcmp(key, "esync")) {
        if (g_net_esync == (v != 0)) return true;
        g_net_esync = v != 0;
        LOG("[esync] enemy sync %s\n", g_net_esync ? "on (needs the exe linkpos_esync_* exports; same build on every copy)" : "off (every enemy brain back on)");
        if (!g_net_esync) net_esync_stop();
    } else if (!strcmp(key, "netreact")) {  // (s194) the KH2 hit reaction on an enemy another game owns (sora_net_react.h)
        if (g_net_react == (v != 0)) return true;
        g_net_react = v != 0;
        if (!g_net_react) net_react_clear();
        LOG("[react] %s\n", g_net_react ? "on: a hit on a puppet sends the reaction to its owner, the owner plays it (set netreact 0 = the old way)"
                                         : "off: hits on puppets react locally as before, nothing sent, received reactions dropped");
    } else if (!strcmp(key, "eshit")) {  // (s195) the hit claim (sora_net_esync.h)
        if (g_net_eshit == (v != 0)) return true;
        g_net_eshit = v != 0;
        LOG("[esync] hit claim %s\n", g_net_eshit ? "on: my hit on an enemy another game runs takes it over (unless he keeps hitting it); his hits keep it his"
                                                 : "off: only the nearest player runs an enemy, as before (set eshit 1)");
    } else if (!strcmp(key, "eshitms")) {  // (s195) how long the owner's hits keep "closer" hand-overs off
        int ms = v < ES_HIT_HOLD_MIN ? ES_HIT_HOLD_MIN : v > ES_HIT_HOLD_MAX ? ES_HIT_HOLD_MAX : v;
        if (g_net_eshitms == ms) return true;
        g_net_eshitms = ms;
        LOG("[esync] hit hold %d ms%s\n", ms, ms != v ? " (clamped)" : "");
    } else if (!strcmp(key, "esdead")) {  // (s195) a dead player is nobody's nearest (his loaded enemies go to a living player)
        if (g_net_esdead == (v != 0)) return true;
        g_net_esdead = v != 0;
        LOG("[esync] dead players %s\n", g_net_esdead ? "away: a dead player is nobody's nearest, a living one takes his enemies (set esdead 0 = as before)"
                                                     : "count as before (set esdead 1)");
    } else if (!strcmp(key, "esaprobe")) {
        esp_set(v != 0);
    } else if (!strcmp(key, "esacalc")) {  // s180 R2: the copy's attack calc_ only every N game frames (1 = every tap)
        g_esa.calc_every = v < 1 ? 1 : v;
        LOG("[esa] calc_ every %d game frame(s)\n", g_esa.calc_every);
    } else if (!strcmp(key, "esaskip")) {  // s180 R2: after a calc_ over v ms, skip the next 2 taps' calc_ (0 = off)
        g_esa.skip_ms = v < 0 ? 0 : v;
        LOG("[esa] skip after a slow calc_: %s\n", g_esa.skip_ms ? "on" : "off");
    } else if (!strcmp(key, "esaprof")) {  // s182: sampling profiler of slow calc_ / enter_ calls (needs esynclog; default 1)
        g_esf_on = v != 0;
        LOG("[esa] prof sampler %s (needs esynclog 1)\n", g_esf_on ? "on" : "off");
    } else if (!strcmp(key, "esaclub")) {  // s178 R2: the copy turns its club on / off where the owner's swing did (default 1)
        esc_set(v != 0);
    } else if (!strcmp(key, "esynclog")) {
        g_net_esynclog = v != 0;
        LOG("[esync] log %s\n", g_net_esynclog ? "on" : "off");
    } else if (!strcmp(key, "esyncatk")) {  // s174 R2: puppets run their own copy of the owner's attack action (club / body hits); exe export
        esa_set_mode(&g_esa, v);  // 0 off, 1 the exe runner (as in s174), 2 the DLL runner (needs the s175 exe; else acts as 1), 3 = 2 in the AI phase (s181 exe; else acts as 2)
    } else if (!strcmp(key, "netfire")) {
        if (g_net_fire == (v != 0)) return true;
        g_net_fire = v != 0;
        LOG("[net] fire sync %s\n", g_net_fire ? "on" : "off (set netfire 0: nothing sent, received fire ignored)");
    } else if (!strcmp(key, "netfireen")) {
        g_net_fire_en = v != 0;
        LOG("[net] fire sync: enemies on the receiver %s\n", g_net_fire_en ? "ignited too (test only: doubles the burn damage)" : "get sparks only (their HP is shared by edmg)");
    } else if (!strcmp(key, "netfirespk")) {
        g_net_fire_espk = v != 0;
        LOG("[net] fire sync: sparks at enemies on the receiver %s\n", g_net_fire_espk ? "on" : "off (fire objects + bursts only)");
    } else if (!strcmp(key, "netfirelog")) {
        g_net_fire_log = v != 0;
        LOG("[net] fire log %s\n", g_net_fire_log ? "on" : "off");
    } else if (!strcmp(key, "edmglog")) {
        g_net_edmg_log = v != 0;
        LOG("[net] edmg log %s\n", g_net_edmg_log ? "on" : "off");
    } else if (!strcmp(key, "netanchor")) {
        if (g_net_anchor == (v != 0)) return true;
        g_net_anchor = v != 0;
        LOG("[net] fx anchors %s\n", g_net_anchor ? "on (bound fx follow a body)" : "off (set netanchor 0: world places, every move re-sent)");
    } else if (!strcmp(key, "netplate")) {
        if (g_net_noplate == (v == 0)) return true;
        g_net_noplate = v == 0;
        LOG("[net] name plate %s\n", g_net_noplate ? "off (set netplate 0)" : "on");
    } else if (!strcmp(key, "netparty")) {
        if (g_net_noparty == (v == 0)) return true;
        g_net_noparty = v == 0;
        LOG("[net] remote party %s\n", g_net_noparty ? "off (set netparty 0)" : "on");
    } else if (!strcmp(key, "netport")) {
        bool was = NET.on;
        if (v < 1024 || v > 65535) { LOG("[net] netport %d refused (1024..65535)\n", v); return true; }
        if (was) net_set_on(false);
        NET.port = (uint16_t)v;
        LOG("[net] port %u\n", NET.port);
        if (was) net_set_on(true);
    } else return false;
    return true;
}

// live line commands: true = a net line
static bool net_live_line(const char *ln) {
    if (strncmp(ln, "net ", 4)) return false;
    const char *a = ln + 4;
    while (*a == ' ') a++;
    if (!strncmp(a, "peer", 4) && (a[4] == 0 || a[4] == ' ' || a[4] == '\r' || a[4] == '\n')) {
        const char *arg = a + 4;
        while (*arg == ' ') arg++;
        size_t al = strlen(arg);
        while (al && (arg[al - 1] == ' ' || arg[al - 1] == '\r' || arg[al - 1] == '\n')) al--;
        if (!al) net_table_log();
        else if (al == 3 && !strncmp(arg, "off", 3)) {
            for (int i = 0; i < NET.ntg; i++) net_tg_del(i--);  // (pins and learned ones)
            LOG("[net] pins cleared: discovery\n");
        } else net_pin_list(arg, GetTickCount64());
        NET.next_hello = 0;
    } else if (!strncmp(a, "name ", 5)) {
        net_name_set(a + 5);
        NET.next_hello = 0;  // (tell the peers now)
        LOG("[net] name \"%s\"\n", NET.name);
    } else if (!strcmp(a, "diag")) net_diag_log();
    else LOG("[net] unknown line \"%s\" (net peer [<ip>[:port]|off], net name <s>, net diag)\n", ln);
    return true;
}

// Multiplayer slice 1 glue (spec docs/specs/2026-11-05-multiplayer-slice1-design.md sections 3-4, plan Task 5).
// Included once by sora.c right after sora_fx.h (the fx / sfx wrappers there call net_note_*). (mp4, slice 4: up to
// NET_MAX_PEERS = 3 remote peers in a full mesh, one NetPeerSlot each, docs/specs/2026-11-06-multiplayer-slice4-design.md.)
// The game state that goes into a SNAP is read by net_fill() (sora.c, defined late: it needs S, KB, FM, SKT...).
// Called once per frame from sora_mod_frame: net_frame(now) -> net_tick(Link's pos | NULL while loading / menu).
//   env:   SAVAGEWIND_SORA_NET=1 (on at start), SAVAGEWIND_SORA_NET_NAME=<name> (default "Sora"), SAVAGEWIND_SORA_NET_PORT=<n>,
//          SAVAGEWIND_SORA_NET_PEERS=ip[:port],ip[:port],... (pins at start)
//   live:  set net 0|1, set netloop 0|1, set netbone 0|1 (bone-pose sync, default 1), set netskate 0|1 (skate rider sync, default 1), set netplate 0|1, set netparty 0|1, set netport <n>, set edmg 0|1 (shared enemy HP, default 1), set esync 0|1 (enemy sync, default 0), set esynclog 0|1,
//          set edmglog 0|1 (log each edmg tx / rx; the glue is sora_net_edmg.h),
//          set netfire 0|1 (fire sync, default 1), set netfireen 0|1 (also ignite enemies on the receiver, default 0), set netfirespk 0|1 (sparks at enemies on the receiver, default 1),set netfirelog 0|1 (default 1; sora_net_fire.h);
//          lines "net peer <ip>[:port]" (adds a pin) | "net peer off" (clears the pins) | "net peer" (logs the table), "net name <s>", "net diag"
// Net off (default): no socket, net_tick returns at once, net_note_* return at once (no sfx / fx name lookups).
#pragma once
#include "sora_net_sock.h"
#include "sora_net_peer.h"

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
        uint32_t mv_sent, mv_skip, anc_ev[4], anc_ev_in[4], anc_mv_in[4], anc_guess; } st;  // (slice 3: moves sent / skipped as unchanged, fx events by anchor, received moves by anchor, party anchors played at Sora; mp4: lost_out = our queue full, lost_in = a peer's inq full)
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
#include "sora_net_edmg.h"  // (s166 v6: shared enemy HP; needs NET, net_dests, net_send_to)
#include "sora_net_fire.h"  // (s176 v10: fire sync; needs NET, net_dests, net_send_to, NetEdOut)
#define NET_ESYNC_GLUE
#include "sora_net_esync.h"  // (s168 v7: enemies act the same on every game; default OFF)
#include "sora_esa.h"  // (s175: the R2 attack runner in the DLL, `set esyncatk 2`)
#include "sora_esa_probe.h"  // (s176 R2 diag: `set esaprobe 1` = does the club hit turn on?)

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
    uint8_t b[NET_PKT_MAX];
    NetAddr d[NET_DESTS_MAX];
    int n = net_dests(d, true);
    for (int i = 0; i < n; i++) net_send_to(d[i].ip, d[i].port, b, net_pack_hello_for(&d[i], b, sizeof b));
    // LAN discovery: only without pins, while a slot is free (every round: 1 s while alone, 5 s once someone is here)
    if (net_npins() == 0 && netslot_count(NET.slot, NET_MAX_PEERS) < NET_MAX_PEERS)
        net_send_to(0xFFFFFFFFu, NET.port, b, net_pack_hello_for(NULL, b, sizeof b));
}

static void net_send_bye_all(void) {
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

static void net_set_on(bool on) {
    if (on == NET.on) return;
    if (on) {
        if (!NET.port) NET.port = NET_PORT;
        if (!NET.name[0]) net_name_set(NULL);
        if (!nsock_open(&NET.sock, NET.port, true)) {
            if (!NET.fail_logged)
                LOG("[net] socket open failed on UDP port %u: net stays off (another copy on this PC? its port is exclusive:"
                    " SAVAGEWIND_SORA_NET_PORT / \"set netport N\")\n", NET.port);
            NET.fail_logged = true;
            return;
        }
        NET.on = true, NET.me = net_roll_id(0);
        NET.next_snap = NET.next_hello = 0, NET.nevq = 0;
        memset(NET.fxo, 0, sizeof NET.fxo), NET.fxrr = 0;  // (mp2: no old-seq moves / kills after a net 0 -> 1)
        memset(&NET.st, 0, sizeof NET.st);
        net_edmg_reset(GetTickCount64());
        net_fire_reset(GetTickCount64());
        for (int i = 0; i < NET.ntg; i++) if (!NET.tg[i].pinned) net_tg_del(i--);  // (learned addresses are stale)
        NET.nalias = 0, NET.learn_hello_at = 0;
        LOG("[net] on: UDP port %u, player %u, name \"%s\", %d pin(s)%s, up to %d peers%s\n", NET.port, NET.me, NET.name,
            net_npins(), net_npins() ? "" : " (discovery)", NET_MAX_PEERS, g_net_loop ? ", loopback" : "");
    } else {
        net_send_bye_all();
        nsock_close(&NET.sock);
        NET.on = false;
        net_slots_free_all();
        NET.nevq = 0, NETED.nq = 0;
        net_esync_stop();  // (s168: every enemy brain we stopped runs again)
        LOG("[net] off: BYE sent, socket closed (sent %u, recv %u)\n", NET.st.sent, NET.st.recv);
    }
}

// (mp2) Sora mode off (!g_on) while net is on: the net steps stop, so say BYE, drop his fx copies (they would freeze) and
// close the socket exactly like net 0; net_frame (Sora back on) resumes it. Called every frame while Sora is off: cheap.
static void net_sora_off(void) {
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

static void net_init_from_env(void) {
    if (NET.inited) return;
    NET.inited = true;
    NET.port = NET_PORT;
    const char *pe = getenv("SAVAGEWIND_SORA_NET_PORT");  // (mp2 review: a 2nd copy on one PC, the port is exclusive)
    if (pe && atoi(pe) >= 1024 && atoi(pe) <= 65535) NET.port = (uint16_t)atoi(pe);
    net_name_set(getenv("SAVAGEWIND_SORA_NET_NAME"));
    net_pin_list(getenv("SAVAGEWIND_SORA_NET_PEERS"), GetTickCount64());  // (mp4: launcher / friends)
    const char *on = getenv("SAVAGEWIND_SORA_NET");
    if (on && atoi(on) == 1) net_set_on(true);
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
    LOG("[net] peer %d joined: %s %s:%u\n", k + 1, type == NET_HELLO && name[0] ? name : "(snapshot)", net_ip_txt(ip, t), port);
    NET.next_hello = now;  // (answer at once: it learns our name + list)
    return k;
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
    for (int i = 0; i < 64; i++) {
        uint32_t ip = 0; uint16_t port = 0;
        int n = nsock_recv(&NET.sock, buf, sizeof buf, &ip, &port);
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
                LOG("[net] packet from %s:%u has protocol version %d, ours is %d: update both copies to the same build\n", net_ip_txt(ip, t), port, ov, NET_VERSION);
            }
            continue;
        }
        if (player == NET.me && (type == NET_EDMG || type == NET_ESYNC || type == NET_FIRE)) { NET.st.echo++; continue; }  // (s166: its seq is a different counter than the SNAP seq below)
        if (player == NET.me) {
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
            if (type == NET_BYE || type == NET_EDMG || type == NET_ESYNC || type == NET_FIRE) continue;  // (a stranger leaving / a hit from nobody we know)
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
            if (fresh) NET.next_hello = now, memset(&sl->ed, 0, sizeof sl->ed), memset(&sl->fi, 0, sizeof sl->fi), memset(&sl->es, 0, sizeof sl->es), memset(&sl->esev, 0, sizeof sl->esev);  // (a restarted peer counts its packet seqs anew)
            for (int j = 0; j < npa; j++) net_learn(&pa[j], ip, port, now);
        } else if (type == NET_SNAP) {
            bool cur = peer_on_snap(&sl->p, &s, now);
            if (!cur) NET.st.stale++, sl->st.stale++;
            net_take_snap_events(k, &s, now);
            if (cur) net_take_snap_moves(k, &s, now);  // (a stale SNAP's places are older than the track's)
        } else if (type == NET_EDMG) {
            net_edmg_recv(k, buf, n);
        } else if (type == NET_FIRE) {
            net_fire_recv(k, buf, n);
        } else if (type == NET_ESYNC) {
            net_esync_recv(k, buf, n);
        } else if (type == NET_BYE) {
            LOG("[net] peer %d \"%s\" left (BYE)\n", k + 1, sl->p.name);
            net_slot_free(k);
        }
    }
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
    for (int i = 0; i < nd; i++) net_send_to(d[i].ip, d[i].port, b, n);
}

// once per frame; link_pos NULL = no world / loading / menu (SNAPs carry NF_LOADING and the last feet)
static void net_tick(const float link_pos[3], uint64_t now) {
    if (!NET.on) return;
    bool loading = !link_pos || NET.loading;
    if (link_pos) NET.feet[0] = link_pos[0], NET.feet[1] = link_pos[1], NET.feet[2] = link_pos[2], NET.have_feet = true;
    net_recv_all(now);
    int live = 0;
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        NetPeerSlot *sl = &NET.slot[k];
        if (!sl->used) continue;
        PeerState was = sl->p.st;
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
    if (NET.driven) net_tick(link_pos, now);
    else net_tick_idle(now);
    NET.driven = false;
}

static void net_table_log(void) {
    char t[16];
    uint64_t now = GetTickCount64();
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
    net_fire_diag();
    net_esync_diag();
    avatar_diag();
    net_party_diag();
}

// live "set <key> <int>": true = a net key
static bool net_live_set(const char *key, int v) {
    if (!strcmp(key, "net")) { net_init_from_env(); NET.parked = false; net_set_on(v != 0); if (v && !NET.on) LOG("[net] still off (socket)\n"); }
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

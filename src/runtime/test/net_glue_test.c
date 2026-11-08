// host test for the glue in runtime/sora_net.h (multiplayer slice 2 Task 3 minors): the game parts are stubbed.
//   Windows: clang -O1 -g -Wall -Wextra -Iruntime runtime/test/net_glue_test.c -o build_h/net_glue_test.exe -lws2_32
//   Linux:   the same without -lws2_32
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include "sora_win.h"  // (winsock2 + windows.h, before the socket header, as in the DLL)
#else
#include <time.h>
typedef union { long long QuadPart; } LARGE_INTEGER;
static void QueryPerformanceCounter(LARGE_INTEGER *t) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); t->QuadPart = ts.tv_sec * 1000000000LL + ts.tv_nsec; }
static unsigned long long GetTickCount64(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000ull + ts.tv_nsec / 1000000; }
#endif
#ifdef _WIN32
#define Sleep_ms(ms) Sleep(ms)
#else
#define Sleep_ms(ms) nanosleep(&(struct timespec){0, (ms) * 1000000L}, NULL)
#endif
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
static char g_log[16384];
static void test_log(const char *f, ...) {
    va_list a; va_start(a, f);
    size_t n = strlen(g_log);
    vsnprintf(g_log + n, sizeof g_log - n, f, a);
    va_end(a);
}
#define LOG test_log

// ---- stubs for sora.c / sora_fx.h / sora_net_avatar.h
static uint32_t g_fx_gen[32];
static bool g_fx_live[32];
static int g_kills[32], g_nkill;
static void sora_fx_kill(int h) { g_kills[h]++, g_nkill++, g_fx_live[h] = false; }
static bool sora_fx_alive(int h) { return h >= 0 && h < 32 && g_fx_live[h]; }
static inline bool fx_mine(int h, uint32_t gen) { return h >= 0 && h < 32 && g_fx_gen[h] == gen; }
static void avatar_diag(void) {}
static void avatar_off(void) {}
static int g_slot_off[8];  // (mp4) avatar_slot_off / net_party_slot_off calls per slot
static void avatar_slot_off(int k) { g_slot_off[k]++; }
static void net_party_slot_off(int k) { (void)k; }
static void net_party_off(void) {}
static void net_party_diag(void) {}
static int g_drops;
static void av_drop_events(void);
#include "sora_net_proto.h"
// (s176) the sora.c receiver half of the fire sync, stubbed: records the calls
static struct { int n; float pos[3], r, h; bool last, burst; int who; } g_fr[32];
static int g_nfr, g_fr_ret;
static int fire_world_remote(const float c[3], bool last, bool burst, float r, float h, int who, int res[3]) {
    if (g_nfr < 32) memcpy(g_fr[g_nfr].pos, c, 12), g_fr[g_nfr].r = r, g_fr[g_nfr].h = h, g_fr[g_nfr].last = last, g_fr[g_nfr].burst = burst, g_fr[g_nfr].who = who;
    g_nfr++;
    res[0] = 2, res[1] = 3, res[2] = last ? 6 : 0;
    return g_fr_ret;
}
static void net_fill(NetSnap *s);
#define NET_EDMG_NO_EXE  // (the test sets the exe function pointers itself)
#define NET_ESYNC_NO_EXE
#define ESA_NO_EXE
#include "sora_net.h"
static void net_fill(NetSnap *s) { (void)s; }
static void av_drop_events(void) {  // (as sora_net_avatar.h)
    g_drops++;
    for (int k = 0; k < NET_MAX_PEERS; k++) net_slot_drop(k);
}

static NetSnap snap_ev(uint16_t seq, int nev, int first_ev_seq) {
    NetSnap s;
    memset(&s, 0, sizeof s);
    s.player = 9, s.seq = seq, s.nev = (uint8_t)nev;
    for (int i = 0; i < nev; i++) s.ev[i].seq = (uint8_t)(first_ev_seq + i), s.ev[i].kind = 0, s.ev[i].id = 5;
    return s;
}

static void test_inq_64(void) {
    memset(&NET, 0, sizeof NET), NET.inited = true;
    peer_reset(&NETP(0), 0);
    CHECK(sizeof NET.slot[0].inq / sizeof NET.slot[0].inq[0] == 64);
    // 60 events arrive in bursts of 4 in 15 SNAPs: none lost
    for (int k = 0; k < 15; k++) {
        NetSnap s = snap_ev((uint16_t)(10 + k), 4, 4 * k);
        peer_on_snap(&NETP(0), &s, 100 + k * 50);
        net_take_snap_events(0, &s, 100 + k * 50);
    }
    CHECK(NET.slot[0].ninq == 60 && NET.st.lost_in == 0 && NET.st.ev_fresh == 60);
    CHECK(NET.slot[0].inq[0].ev.seq == 0 && NET.slot[0].inq[59].ev.seq == 59);
    // 8 more: the oldest 4 give way at 64
    for (int k = 15; k < 17; k++) {
        NetSnap s = snap_ev((uint16_t)(10 + k), 4, 4 * k);
        peer_on_snap(&NETP(0), &s, 100 + k * 50);
        net_take_snap_events(0, &s, 100 + k * 50);
    }
    CHECK(NET.slot[0].ninq == 64 && NET.st.lost_in == 4 && NET.slot[0].inq[0].ev.seq == 4);
}


// a remote copy of event seq `ev` shows in fx slot h (what the avatar does after sora_fx_spawn)
static void show_copy(int ev, int h) {
    g_fx_gen[h]++, g_fx_live[h] = true;
    NET.slot[0].rfx[ev].h1 = h + 1, NET.slot[0].rfx[ev].gen = g_fx_gen[h];
}
// ---- (s166 v6) a fake exe: linkpos_edmg_take / _apply
static NetExEdmg g_fq[64];
static int g_nfq;
static int fake_take(NetExEdmg *out, int max, int size) {
    if (size != (int)sizeof *out) return -1;
    int n = g_nfq < max ? g_nfq : max;
    memcpy(out, g_fq, sizeof *out * (size_t)n), memmove(g_fq, g_fq + n, sizeof *g_fq * (size_t)(g_nfq - n)), g_nfq -= n;
    return n;
}
static int bad_take(NetExEdmg *out, int max, int size) { (void)out; (void)max; (void)size; return -1; }
static struct { uint32_t hid; int dealt, after, f54, f40, kill; } g_ap[128];
static int g_nap, g_ap_ret = 1;
static int fake_apply(uint32_t hid, int dealt, int after, int f54, int f40, int kill) {
    if (g_nap < 128) g_ap[g_nap].hid = hid, g_ap[g_nap].dealt = dealt, g_ap[g_nap].after = after, g_ap[g_nap].f54 = f54, g_ap[g_nap].f40 = f40, g_ap[g_nap].kill = kill;
    g_nap++;
    return g_ap_ret;
}
static void queue_hit(uint32_t hid, int dealt, int after, int kill) {
    NetExEdmg *e = &g_fq[g_nfq++];
    memset(e, 0, sizeof *e);
    e->hid = hid, e->dealt = dealt, e->after = after, e->kill = kill, e->f54 = 15, e->f40 = 1;
    snprintf(e->name, sizeof e->name, "Enemy_Test");
}

// (s168 v7) fake enemy-sync exe
static int g_es_enum, g_es_brain_last = -1, g_es_brain_n, g_es_apply_n, g_es_sens_n;
static int es_enum(uint32_t *h, int max) { g_es_enum++; h[0] = 0x77, h[1] = 0; h[2] = 0x88; return 3; }
static int es_read(uint32_t hid, EsState *o) { memset(o, 0, sizeof *o); o->hid = hid, o->pos[0] = hid == 0x77 ? 3.0f : 70.0f, o->anim_hash[0] = 0xABCD; return 1; }
static void es_pup(uint32_t hid, const EsState *t, int snap) { g_es_apply_n++; }
static void es_brn(uint32_t hid, int on) { g_es_brain_last = on, g_es_brain_n++; }
static int es_evs(EsEvent *o, int max) { return 0; }
static void es_sns(uint32_t hid, int on, uint32_t name, const uint32_t *p) { g_es_sens_n++; }
static void reset_all(void) {
    memset(&NET, 0, sizeof NET), NET.inited = true;
    memset(&NETES, 0, sizeof NETES), NETES.bound = true;
    NETES.c.io = (EsIface){es_enum, es_read, es_pup, es_brn, es_evs, es_sns};
    g_net_esync = g_net_esynclog = false, g_es_enum = g_es_brain_n = g_es_apply_n = g_es_sens_n = 0, g_es_brain_last = -1;
    memset(&NETED, 0, sizeof NETED), NETED.bound = true, NETED.take = fake_take, NETED.apply = fake_apply;
    memset(&NETF, 0, sizeof NETF), g_net_fire = true, g_net_fire_en = false, g_net_fire_log = false, g_nfr = g_fr_ret = 0;
    g_nfq = g_nap = 0, g_ap_ret = 1, g_net_edmg = true, g_net_edmg_log = false;
    memset(g_fx_gen, 0, sizeof g_fx_gen), memset(g_fx_live, 0, sizeof g_fx_live), memset(g_kills, 0, sizeof g_kills);
    g_nkill = g_drops = 0, g_log[0] = 0;
}

static void test_rfx_seq_wrap(void) {
    reset_all();
    show_copy(7, 3);  // a long-lived fx under seq 7
    NetSnap s = snap_ev(10, 1, 7);  // 256 events later seq 7 again
    peer_on_snap(&NETP(0), &s, 100);
    net_take_snap_events(0, &s, 100);
    CHECK(g_kills[3] == 1 && !g_fx_live[3]);  // killed, not orphaned
    CHECK(NET.slot[0].rfx[7].h1 == 0 && NET.slot[0].rfx[7].born == 1);
    // a copy whose slot was reused by something else is not killed
    show_copy(9, 5), g_fx_gen[5]++;
    NetSnap s2 = snap_ev(11, 1, 9);
    peer_on_snap(&NETP(0), &s2, 150);
    net_take_snap_events(0, &s2, 150);
    CHECK(g_kills[5] == 0);
}

static void test_rfx_cap(void) {
    reset_all();
    for (int i = 0; i < 8; i++) { NET.slot[0].rfx[i].born = (uint32_t)(i + 1); show_copy(i, 10 + i); }
    NET.rfx_born = 8;
    g_fx_live[12] = false;  // (event 2) one ended on its own: does not count
    net_rfx_room();
    CHECK(g_nkill == 0 && NET.slot[0].rfx[2].h1 == 0);  // 7 live < 8: room
    show_copy(20, 25), NET.slot[0].rfx[20].born = 9;
    net_rfx_room();  // 8 live: the oldest (event 0, slot 10) gives way
    CHECK(g_nkill == 1 && g_kills[10] == 1 && NET.slot[0].rfx[0].h1 == 0);
    // never touches slots it does not own: local fx in slots 0..9 untouched (only 10..25 were remote)
    int local_kills = 0;
    for (int h = 0; h < 10; h++) local_kills += g_kills[h];
    CHECK(local_kills == 0);
    // a burst of 40 remote spawns keeps at most 8 live
    reset_all();
    int h = 0;
    for (int i = 0; i < 40; i++) {
        NET.slot[0].rfx[i].born = ++NET.rfx_born;
        net_rfx_room();
        show_copy(i, 8 + (h++ % 24));
        int live = 0;
        for (int k = 0; k < 256; k++) live += NET.slot[0].rfx[k].h1 != 0 && fx_mine(NET.slot[0].rfx[k].h1 - 1, NET.slot[0].rfx[k].gen) && g_fx_live[NET.slot[0].rfx[k].h1 - 1];
        CHECK(live <= NET_RFX_MAX);
    }
}

static void test_net_on_clears_fxo(void) {
    reset_all();
    NET.port = 47795;
    NET.fxo[3].on = true, NET.fxo[3].ev = 9, NET.fxo[3].rep = 2, NET.fxo[3].flags = NMV_KILL, NET.fxrr = 5;
    net_set_on(true);
    CHECK(NET.on);
    CHECK(NET.fxo[3].on == false && NET.fxo[3].rep == 0 && NET.fxrr == 0);
    net_set_on(false);
    CHECK(!NET.on);
}

// (mp2) a NET_VERSION mismatch is logged once per peer address
static void test_version_log(void) {
    reset_all();
    NET.port = 47796;
    net_set_on(true);
    CHECK(NET.on);
    NetSock tx;
    CHECK(nsock_open(&tx, 47797, false));
    uint32_t lo = 0;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    uint8_t pk[NET_PKT_MAX];
    int n = net_pack_hello(7, 1, "Old", pk, sizeof pk);
    pk[2] = NET_VERSION - 1;
    g_log[0] = 0;
    for (int i = 0; i < 3; i++) CHECK(nsock_send(&tx, lo, 47796, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 3; t++) { net_recv_all(1000); if (NET.st.recv < 3) Sleep_ms(5); }
    CHECK(NET.st.recv == 3 && NET.st.bad == 3);
    const char *a = strstr(g_log, "protocol version");
    CHECK(a && !strstr(a + 1, "protocol version"));  // 3 packets, 1 line
    char want[48];
    snprintf(want, sizeof want, "version %d, ours is %d", NET_VERSION - 1, NET_VERSION);
    CHECK(strstr(g_log, "127.0.0.1") && strstr(g_log, want));
    nsock_close(&tx);
    net_set_on(false);
}

// (mp2) Sora mode off while net is on: BYE goes out, his fx copies and queued events are dropped, the socket is closed;
// Sora back on resumes the session
static void test_sora_off_parks(void) {
    reset_all();
    NetSock rx;
    CHECK(nsock_open(&rx, 47798, false));
    NET.port = 47799;
    uint32_t lo7;
    CHECK(nsock_parse_ip("127.0.0.1", &lo7) && net_tg_add(lo7, 47798, true, 0));
    net_set_on(true);
    CHECK(NET.on);
    show_copy(4, 6);
    NET.slot[0].inq[0].ev.seq = 1, NET.slot[0].ninq = 1;
    net_sora_off();
    CHECK(!NET.on && NET.parked && !NET.sock.ok);
    CHECK(g_drops == 1 && g_kills[6] == 1 && NET.slot[0].ninq == 0 && NET.slot[0].rfx[4].h1 == 0);
    uint8_t got[NET_PKT_MAX];
    int n = 0, type = 0; uint8_t player = 0; uint16_t seq = 0; char name[NET_NAME_MAX + 1];
    for (int t = 0; t < 100 && n == 0; t++) { n = nsock_recv(&rx, got, sizeof got, NULL, NULL); if (!n) Sleep_ms(5); }
    CHECK(n > 0 && net_unpack(got, n, &type, &player, &seq, NULL, name) && type == NET_BYE);
    net_sora_off();  // (every frame while Sora is off: nothing more happens)
    CHECK(g_drops == 1);
    net_frame(NULL, 5000);  // Sora is back on
    CHECK(NET.on && !NET.parked && NET.sock.ok);
    net_set_on(false);
    nsock_close(&rx);
    // "set net 0" while parked = stays off
    net_set_on(true);
    net_sora_off();
    CHECK(NET.parked);
    net_live_set("net", 0);
    net_frame(NULL, 6000);
    CHECK(!NET.on && !NET.parked);
}

// (slice 3) anchored moves: tracked per anchor (a change resets), stats by anchor, netanchor key
static void test_anchor_moves(void) {
    reset_all();
    CHECK(g_net_anchor == 1);
    net_live_set("netanchor", 0);
    CHECK(g_net_anchor == 0);
    net_live_set("netanchor", 1);
    CHECK(g_net_anchor == 1);
    NetSnap s;
    memset(&s, 0, sizeof s);
    s.player = 9, s.seq = 1, s.nev = 1, s.ev[0].seq = 4, s.ev[0].kind = 0, s.ev[0].id = 5, s.ev[0].anchor = NANC_DONALD;
    peer_on_snap(&NETP(0), &s, 100);
    net_take_snap_events(0, &s, 100);
    CHECK(NET.st.anc_ev_in[NANC_DONALD] == 1 && NET.slot[0].inq[0].ev.anchor == NANC_DONALD);
    NetSnap m;
    memset(&m, 0, sizeof m);
    m.player = 9, m.seq = 2, m.nmv = 1, m.mv[0].ev = 4, m.mv[0].anchor = NANC_DONALD, m.mv[0].pos[2] = 1.5f;
    peer_on_snap(&NETP(0), &m, 150);
    net_take_snap_moves(0, &m, 150);
    CHECK(NET.slot[0].rfx[4].tr.n == 1 && NET.slot[0].rfx[4].tr.anc == NANC_DONALD && NET.st.anc_mv_in[NANC_DONALD] == 1);
    m.seq = 3, m.mv[0].anchor = NANC_WORLD, m.mv[0].pos[2] = 40;  // the sender dropped the anchor: world places from here
    peer_on_snap(&NETP(0), &m, 200);
    net_take_snap_moves(0, &m, 200);
    CHECK(NET.slot[0].rfx[4].tr.n == 1 && NET.slot[0].rfx[4].tr.anc == NANC_WORLD && NET.slot[0].rfx[4].tr.pos[0][2] == 40);
}


// ---- (mp4) slice 4: up to 3 peers over real loopback sockets, gossip, join / leave / full, pins, per-slot data ----
typedef struct { NetSock s; uint16_t port; uint8_t id; uint16_t seq; } Bot;
static uint32_t g_lo;
static bool bot_open(Bot *b, uint16_t port, uint8_t id) {
    memset(b, 0, sizeof *b);
    b->port = port, b->id = id, b->seq = 100;
    nsock_parse_ip("127.0.0.1", &g_lo);
    return nsock_open(&b->s, port, false);
}
static void bot_hello(Bot *b, uint16_t to, const char *name, const NetAddr *pa, int npa) {
    uint8_t p[NET_PKT_MAX];
    int n = net_pack_hello_peers(b->id, ++b->seq, name, pa, npa, p, sizeof p);
    nsock_send(&b->s, g_lo, to, p, n);
}
static void bot_snap(Bot *b, uint16_t to, int nev, int first_ev) {
    NetSnap s = snap_ev(++b->seq, nev, first_ev);
    s.player = b->id, s.feet[0] = b->port;  // (feet x = his port: tells the slots apart)
    uint8_t p[NET_PKT_MAX];
    int n = net_pack_snap(&s, p, sizeof p);
    nsock_send(&b->s, g_lo, to, p, n);
}
static void bot_bye(Bot *b, uint16_t to) {
    uint8_t p[NET_PKT_MAX];
    nsock_send(&b->s, g_lo, to, p, net_pack_bye(b->id, ++b->seq, p, sizeof p));
}
// waits (<= 0.5 s) for a packet of `want` type to the bot, dropping others; the HELLO's peer list into pa / npa
static bool bot_wait(Bot *b, int want, NetAddr *pa, int *npa) {
    for (int t = 0; t < 100; t++) {
        uint8_t buf[NET_PKT_MAX + 64];
        int n;
        while ((n = nsock_recv(&b->s, buf, sizeof buf, NULL, NULL)) > 0) {
            int type = 0, k = 0; uint8_t player; uint16_t seq; char name[NET_NAME_MAX + 1];
            if (net_unpack_ex(buf, n, &type, &player, &seq, NULL, name, pa, &k) && type == want) { if (npa) *npa = k; return true; }
        }
        Sleep_ms(5);
    }
    return false;
}
static int bot_drain(Bot *b, int want) {  // how many packets of `want` are waiting now
    int c = 0, n;
    uint8_t buf[NET_PKT_MAX + 64];
    Sleep_ms(30);
    while ((n = nsock_recv(&b->s, buf, sizeof buf, NULL, NULL)) > 0) {
        int type = 0; uint8_t player; uint16_t seq; char name[NET_NAME_MAX + 1];
        if (net_unpack(buf, n, &type, &player, &seq, NULL, name) && type == want) c++;
    }
    return c;
}
static const float k_feet[3] = {10, 0, 20};
// ticks at 60 ms steps until cond or 100 tries
#define TICK_UNTIL(cond) do { for (int tt = 0; tt < 100 && !(cond); tt++) { g_now += 60; net_tick(k_feet, g_now); if (!(cond)) Sleep_ms(5); } } while (0)

static void test_mesh_gossip_join_leave(void) {
#ifndef __linux__
    reset_all();
    static uint64_t g_now_s; g_now_s = 1000;
#define g_now g_now_s
    Bot B, C, D, E;
    CHECK(bot_open(&B, 47811, 21) && bot_open(&C, 47812, 22) && bot_open(&D, 47813, 23) && bot_open(&E, 47814, 24));
    NET.port = 47810;
    CHECK(net_tg_add(g_lo, 47811, true, 0));  // A pins only B
    net_set_on(true);
    CHECK(NET.on);
    NET.me = 200;
    net_name_set("Me");
    NetAddr pa[NET_HELLO_PEERS]; int npa = -1;
    // A's HELLO goes to the pin (and not to a broadcast: pinned)
    TICK_UNTIL(false);  // (100 ticks at 60 ms: ~6 s of game time, the HELLO + SNAPs go to B)
    g_now_s = 1000, NET.next_snap = 0;
    NET.next_hello = 0;
    net_tick(k_feet, g_now);
    CHECK(bot_wait(&B, NET_HELLO, pa, &npa) && npa == 0);
    // B knows C: his HELLO lists her; A joins B, learns C, HELLOs C with B in the list (gossip)
    NetAddr cl = {g_lo, 47812};
    bot_hello(&B, 47810, "Bee", &cl, 1);
    TICK_UNTIL(netslot_count(NET.slot, NET_MAX_PEERS) == 1 && net_tg_find(g_lo, 47812) >= 0);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1 && !strcmp(NETP(0).name, "Bee"));
    CHECK(net_tg_find(g_lo, 47812) >= 0 && !NET.tg[net_tg_find(g_lo, 47812)].pinned);  // learned, not pinned
    CHECK(strstr(g_log, "peer 1 joined: Bee") && strstr(g_log, "learned peer address 127.0.0.1:47812"));
    TICK_UNTIL(false);
    g_now_s = 5000, NET.next_snap = 0;
    NET.next_hello = 0;
    net_tick(k_feet, g_now);
    CHECK(bot_wait(&C, NET_HELLO, pa, &npa) && npa == 1 && pa[0].ip == g_lo && pa[0].port == 47811);
    // C answers: slot 2; the learned target is gone (the slot covers it)
    bot_hello(&C, 47810, "Sea", NULL, 0);
    TICK_UNTIL(netslot_count(NET.slot, NET_MAX_PEERS) == 2);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2 && net_tg_find(g_lo, 47812) < 0 && !strcmp(NETP(1).name, "Sea"));
    // D joins on his own (he knows our address): slot 3. A 4th player E is refused, logged once.
    bot_hello(&D, 47810, "Dee", NULL, 0);
    TICK_UNTIL(netslot_count(NET.slot, NET_MAX_PEERS) == 3);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 3);
    g_log[0] = 0;
    bot_hello(&E, 47810, "Eee", NULL, 0), bot_hello(&E, 47810, "Eee", NULL, 0);
    bot_snap(&E, 47810, 0, 0);
    for (int i = 0; i < 20; i++) g_now += 60, net_tick(k_feet, g_now), Sleep_ms(5);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 3 && strstr(g_log, "4 players already: 127.0.0.1:47814 refused"));
    CHECK(strstr(g_log, "refused") && !strstr(strstr(g_log, "refused") + 1, "refused"));  // once per address
    // A's SNAPs go to B, C and D (not E); the HELLO list for D has B and C only
    bot_hello(&B, 47810, "Bee", NULL, 0), bot_hello(&C, 47810, "Sea", NULL, 0), bot_hello(&D, 47810, "Dee", NULL, 0);  // (all three live again: quiet > 2 s = fading, not listed)
    Sleep_ms(30);
    for (int i = 0; i < 3; i++) g_now += 60, net_tick(k_feet, g_now);
    bot_drain(&B, 0), bot_drain(&C, 0), bot_drain(&D, 0), bot_drain(&E, 0);  // (old packets out of the way)
    NET.next_hello = 0;
    net_tick(k_feet, g_now += 60);
    CHECK(bot_wait(&D, NET_HELLO, pa, &npa) && npa == 2 && pa[0].port != 47813 && pa[1].port != 47813);
    CHECK(bot_wait(&B, NET_SNAP, NULL, NULL));
    CHECK(bot_wait(&C, NET_SNAP, NULL, NULL));
    CHECK(bot_wait(&D, NET_SNAP, NULL, NULL));
    CHECK(bot_drain(&E, NET_SNAP) == 0);
    // per-peer data: B and C send the same event seqs, each slot keeps its own queue and event window
    bot_snap(&B, 47810, 5, 0), bot_snap(&C, 47810, 5, 0);
    TICK_UNTIL(NET.slot[0].ninq >= 5 && NET.slot[1].ninq >= 5);
    CHECK(NET.slot[0].ninq == 5 && NET.slot[1].ninq == 5 && NET.slot[2].ninq == 0);
    CHECK(NET.slot[0].p.ring[NET.slot[0].p.n - 1].feet[0] == 47811.0f && NET.slot[1].p.ring[NET.slot[1].p.n - 1].feet[0] == 47812.0f);
    show_copy(2, 7);  // (fx copy under B's event 2 in slot 0: leaves with him)
    NET.slot[0].rfx[2] = NET.slot[0].rfx[0];
    NET.slot[0].rfx[2].h1 = 8, NET.slot[0].rfx[2].gen = g_fx_gen[7];
    int off0 = g_slot_off[0];
    // B says BYE: only slot 0 goes (its copies killed, avatar hidden), the others stay
    bot_bye(&B, 47810);
    TICK_UNTIL(netslot_count(NET.slot, NET_MAX_PEERS) == 2);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2 && !NET.slot[0].used && NET.slot[1].used && NET.slot[2].used);
    CHECK(g_slot_off[0] > off0 && NET.slot[0].ninq == 0 && NET.slot[0].rfx[2].h1 == 0 && NET.slot[1].ninq == 5);
    CHECK(strstr(g_log, "peer 1 \"Bee\" left (BYE)"));
    // E now takes the free slot 0
    bot_hello(&E, 47810, "Eee", NULL, 0);
    TICK_UNTIL(NET.slot[0].used);
    CHECK(NET.slot[0].used && !strcmp(NETP(0).name, "Eee") && NET.slot[0].port == 47814);
    // C and D go quiet for 10 s, E and the table go on: both time out, slot reuse
    for (int i = 0; i < 3; i++) g_now += 4000, bot_hello(&E, 47810, "Eee", NULL, 0), Sleep_ms(20), net_tick(k_feet, g_now);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1 && NET.slot[0].used && strstr(g_log, "peer 2 \"Sea\" timed out") && strstr(g_log, "peer 3 \"Dee\" timed out"));
    net_set_on(false);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 0);
    nsock_close(&B.s), nsock_close(&C.s), nsock_close(&D.s), nsock_close(&E.s);
#undef g_now
#endif
}

static void test_pins_env_and_table(void) {
#ifndef __linux__
    reset_all();
    NET.port = 47820;
    net_live_line("net peer 127.0.0.1:47821");
    net_live_line("net peer 127.0.0.1");
    CHECK(net_npins() == 2 && net_tg_find(g_lo, 47821) >= 0 && net_tg_find(g_lo, NET_PORT) >= 0);
    CHECK(net_pin_list("127.0.0.1:47822, bogus, 127.0.0.1:47823", 0) == 1);  // third pin ok, the 4th is refused, "bogus" bad
    CHECK(net_npins() == 3 && strstr(g_log, "pin list is full") && strstr(g_log, "bogus"));
    g_log[0] = 0;
    net_live_line("net peer");
    CHECK(strstr(g_log, "table: 3 pin(s)") && strstr(g_log, "127.0.0.1:47821"));
    CHECK(net_live_line("net peer 127.0.0.1:47821") && net_npins() == 3);  // the same pin again = no change
    net_live_line("net peer off");
    CHECK(net_npins() == 0 && NET.ntg == 0);
    // env pins at start
    reset_all();
    NET.inited = false;
    NET.port = 47820;
    int added = net_pin_list("127.0.0.1:47831,127.0.0.1:47832", 0);
    CHECK(added == 2 && net_npins() == 2);
    // a learned address never exceeds the target list and is forgotten after 30 s unanswered
    reset_all();
    NET.port = 47820;
    net_set_on(true);
    NET.me = 200;
    CHECK(net_tg_add(g_lo, 47840, false, 1000) && NET.ntg == 1);
    net_tick(k_feet, 1000 + 1000);
    CHECK(NET.ntg == 1);
    net_tick(k_feet, 1000 + NET_TG_LEARN_MS + 1000);
    CHECK(NET.ntg == 0);
    // a clash: a peer with OUR id and a seq outside our echo window = we re-roll
    Bot X;
    CHECK(bot_open(&X, 47841, 200));
    X.seq = (uint16_t)(NET.seq + 5000);
    bot_hello(&X, 47820, "Clash", NULL, 0);
    for (int t = 0; t < 100 && NET.me == 200; t++) net_tick(k_feet, 50000 + t * 60), Sleep_ms(5);
    CHECK(NET.me != 200 && strstr(g_log, "player id clash"));
    net_set_on(false);
    nsock_close(&X.s);
#endif
}

// (mp4 review 1) a peer's HELLO that lists OUR address: learned once at most, then remembered as our own (no HELLO ping-pong),
// and our broadcast HELLO carries no peer list
static void test_self_learn_no_storm(void) {
#ifndef __linux__
    reset_all();
    uint64_t now = 1000;
    Bot B;
    CHECK(bot_open(&B, 47821, 31));
    NET.port = 47820;
    net_set_on(true);
    NET.me = 200;
    bot_hello(&B, 47820, "Bee", NULL, 0);
    for (int i = 0; i < 20; i++) now += 60, net_tick(k_feet, now), Sleep_ms(5);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1 && net_npins() == 0);
    // the broadcast HELLO has no list even with a live peer
    uint8_t hb[NET_PKT_MAX];
    int hn = net_pack_hello_for(NULL, hb, sizeof hb);
    int type = 0, npa = -1; uint8_t pl; uint16_t sq; char nm[NET_NAME_MAX + 1]; NetAddr pa[NET_HELLO_PEERS];
    CHECK(hn > 0 && net_unpack_ex(hb, hn, &type, &pl, &sq, NULL, nm, pa, &npa) && type == NET_HELLO && npa == 0);
    int learned = 0, forced = 0;
    for (int round = 0; round < 5; round++) {
        g_log[0] = 0;
        NetAddr self = {g_lo, 47820};
        bot_hello(&B, 47820, "Bee", &self, 1);  // what an old broadcast would carry
        uint64_t before = NET.next_hello;
        now += 16; Sleep_ms(5); net_tick(k_feet, now);
        if (strstr(g_log, "learned peer address 127.0.0.1:47820")) learned++;
        if (NET.next_hello < before) forced++;
        now += 16; Sleep_ms(5); net_tick(k_feet, now);  // our own echo comes back: the target is deleted + remembered
        CHECK(net_tg_find(g_lo, 47820) < 0);
    }
    CHECK(learned <= 1);
    CHECK(net_alias_find(g_lo, 47820) >= 0);
    (void)forced;
    // net_learn may not force next_hello = now twice within a second
    NET.next_hello = now + 5000;
    NetAddr x1 = {g_lo, 47850}, x2 = {g_lo, 47851};
    NET.learn_hello_at = now;
    net_learn(&x1, g_lo, 47821, now + 100);
    CHECK(net_tg_find(g_lo, 47850) >= 0 && NET.next_hello == now + 5000);
    net_learn(&x2, g_lo, 47821, now + 1200);
    CHECK(net_tg_find(g_lo, 47851) >= 0 && NET.next_hello == now + 1200);
    net_set_on(false);
    nsock_close(&B.s);
#endif
}

// (mp4 review 2) one player reaching us from a 2nd address (127.0.0.1 pin + 127.0.0.2 = "LAN ip") is ONE slot
static SOCKET alias_sock(const char *ip, uint16_t port) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(port); inet_pton(AF_INET, ip, &a.sin_addr);
    if (bind(s, (struct sockaddr *)&a, sizeof a)) printf("bind %s failed %d\n", ip, WSAGetLastError());
    return s;
}
static void alias_send(SOCKET s, uint16_t to, int player, const char *name, bool snap) {
    static uint16_t seq = 100;
    uint8_t p[NET_PKT_MAX];
    int n;
    if (snap) { NetSnap sn = snap_ev(++seq, 0, 0); sn.player = (uint8_t)player; n = net_pack_snap(&sn, p, sizeof p); }
    else n = net_pack_hello_peers((uint8_t)player, ++seq, name, NULL, 0, p, sizeof p);
    struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(to); a.sin_addr.s_addr = htonl(0x7f000001);
    sendto(s, (const char *)p, n, 0, (struct sockaddr *)&a, sizeof a);
}
static void test_alias_one_slot(void) {
#ifndef __linux__
    reset_all();
    uint64_t now = 1000;
    NET.port = 47830;
    nsock_parse_ip("127.0.0.1", &g_lo);
    uint32_t lo2;
    nsock_parse_ip("127.0.0.2", &lo2);
    CHECK(net_tg_add(g_lo, 47831, true, 0));
    net_set_on(true);
    NET.me = 200;
    SOCKET s1 = alias_sock("127.0.0.1", 47831), s2 = alias_sock("127.0.0.2", 47831);
    alias_send(s1, 47830, 41, "Bee", false);
    for (int i = 0; i < 10; i++) now += 60, net_tick(k_feet, now), Sleep_ms(5);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1 && NET.slot[0].ip == g_lo);
    net_tg_add(lo2, 47831, false, now);  // (it was gossiped to us: a learned target)
    g_log[0] = 0;
    alias_send(s2, 47830, 41, "Bee", false);
    alias_send(s2, 47830, 41, NULL, true);
    for (int i = 0; i < 10; i++) now += 60, net_tick(k_feet, now), Sleep_ms(5);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1);  // not 2
    CHECK(NET.slot[0].ip == g_lo && NET.slot[0].port == 47831);  // the address was not moved
    CHECK(net_tg_find(lo2, 47831) < 0 && net_alias_find(lo2, 47831) >= 0);  // learned target dropped, remembered
    CHECK(strstr(g_log, "2nd address of peer 1") && !strstr(strstr(g_log, "2nd address") + 1, "2nd address"));
    NetAddr again = {lo2, 47831};
    net_learn(&again, g_lo, 47831, now);
    CHECK(net_tg_find(lo2, 47831) < 0);  // net_learn skips an alias
    // the same id with ANOTHER name (a random id clash) is a different player
    alias_send(alias_sock("127.0.0.3", 47832), 47830, 41, "Other", false);
    for (int i = 0; i < 10; i++) now += 60, net_tick(k_feet, now), Sleep_ms(5);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2);
    net_set_on(false);
    closesocket(s1), closesocket(s2);
#endif
}

// (mp4 review 3) cap 1 (the Deck) with pins: only a pinned address may take the slot; with a cap of 3 gossip still joins
static void test_pin_exclusive_cap1(void) {
    reset_all();
    nsock_parse_ip("127.0.0.1", &g_lo);
    CHECK(net_join_ok(g_lo, 47900, 1));  // no pins: discovery, anyone
    CHECK(net_tg_add(g_lo, 47901, true, 0));
    CHECK(net_join_ok(g_lo, 47901, 1) && !net_join_ok(g_lo, 47900, 1));
    CHECK(net_tg_add(g_lo, 47902, false, 0));  // learned, not pinned: still refused at cap 1
    CHECK(!net_join_ok(g_lo, 47902, 1));
    CHECK(net_join_ok(g_lo, 47900, 3) && net_join_ok(g_lo, 47902, 3));
    net_live_line("net peer off");
    CHECK(net_join_ok(g_lo, 47900, 1));
}

// (mp4 review 4) unusable addresses are never listed / learned
static void test_addr_filters(void) {
    reset_all();
    uint32_t lan = 0, lan2 = 0, lo = 0, bc = 0, mc = 0, zero = 0, mc2 = 0;
    CHECK(nsock_parse_ip("192.168.1.20", &lan) && nsock_parse_ip("10.0.0.7", &lan2) && nsock_parse_ip("127.0.0.1", &lo));
    CHECK(nsock_parse_ip("255.255.255.255", &bc) && nsock_parse_ip("224.0.0.251", &mc) && nsock_parse_ip("0.1.2.3", &zero) && nsock_parse_ip("239.255.0.1", &mc2));
    CHECK(net_addr_listable_to(lan, lan2) && net_addr_listable_to(lo, lo) && net_addr_listable_to(lan, lo));
    CHECK(!net_addr_listable_to(lo, lan2));
    CHECK(net_addr_learnable(lan, 7790, lan2) && net_addr_learnable(lo, 7791, lo) && net_addr_learnable(lan, 7790, lo));
    CHECK(!net_addr_learnable(lo, 7791, lan2));  // his 127.x is our own localhost
    CHECK(!net_addr_learnable(bc, 7790, lan2) && !net_addr_learnable(mc, 7790, lan2) && !net_addr_learnable(mc2, 7790, lan2));
    CHECK(!net_addr_learnable(zero, 7790, lan2) && !net_addr_learnable(lan, 0, lan2));
    NetAddr bad[4] = {{lo, 7791}, {bc, 7790}, {mc, 7790}, {zero, 7790}};
    for (int i = 0; i < 4; i++) net_learn(&bad[i], lan2, 7790, 1000);
    CHECK(NET.ntg == 0);
    NetAddr good = {lan, 7790};
    net_learn(&good, lan2, 7790, 1000);
    CHECK(NET.ntg == 1);
    // a live loopback peer is not listed in a HELLO to a LAN peer, but is to another loopback one
    NET.slot[0].used = true, NET.slot[0].ip = lo, NET.slot[0].port = 7791, NET.slot[0].p.st = PEER_LIVE;
    uint8_t hb[NET_PKT_MAX];
    int type = 0, npa = -1; uint8_t pl; uint16_t sq; char nm[NET_NAME_MAX + 1]; NetAddr pa[NET_HELLO_PEERS];
    NetAddr to_lan = {lan2, 7790}, to_lo = {lo, 7792};
    int hn = net_pack_hello_for(&to_lan, hb, sizeof hb);
    CHECK(net_unpack_ex(hb, hn, &type, &pl, &sq, NULL, nm, pa, &npa) && npa == 0);
    hn = net_pack_hello_for(&to_lo, hb, sizeof hb);
    CHECK(net_unpack_ex(hb, hn, &type, &pl, &sq, NULL, nm, pa, &npa) && npa == 1 && pa[0].ip == lo && pa[0].port == 7791);
    memset(&NET.slot[0], 0, sizeof NET.slot[0]);
}

// the loopback slot is slot 0; a 60-event burst at 20 Hz loses nothing; a burst bigger than the queue is counted lost_out
static void test_loopback_slot0_and_burst(void) {
    reset_all();
    g_net_loop = true;
    NET.on = true;  // (loop mode never sends: no socket needed)
    for (int i = 0; i < 60; i++) net_push_ev(0, 5, NULL, 0, 1, NANC_WORLD);
    CHECK(NET.nevq == 60 && NET.st.lost_out == 0);
    for (int k = 0; k < 20; k++) net_send_snap(1000 + 50 * (uint64_t)k, false);
    CHECK(NET.nevq == 0 && NET.slot[0].used && NET.slot[0].st.ev_fresh == 60 && NET.st.lost_out == 0 && NET.st.lost_in == 0);
    CHECK(NET.slot[0].ninq == 60 && !NET.slot[1].used && NETP(0).st == PEER_LIVE);
    for (int i = 0; i < 70; i++) net_push_ev(1, 3, NULL, 0, 1, NANC_WORLD);
    CHECK(NET.nevq == NET_EVQ && NET.st.lost_out == 6);  // 70 into 64: the oldest 6 gave way
    g_log[0] = 0;
    net_diag_log();
    CHECK(strstr(g_log, "lost_out 6 lost_in 0"));
    net_slots_free_all();
    CHECK(!NET.slot[0].used && NET.slot[0].ninq == 0);
    g_net_loop = false;
    NET.on = false;
}

// (s166 v6) shared enemy HP, sender: hits drained from the exe go out in packets of <= 16, each 3 times, to every peer of the mesh
static void test_edmg_send_repeats_mesh(void) {
    reset_all();
    int np = NET_MAX_PEERS;
    NetSock rx[3];
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47813;
    for (int i = 0; i < np; i++) CHECK(nsock_open(&rx[i], (uint16_t)(47810 + i), false) && net_tg_add(lo, (uint16_t)(47810 + i), true, 0));
    queue_hit(0xCCCC0001, 5, 8, 0);
    net_set_on(true);
    CHECK(NET.on);
    queue_hit(0xAAAA0001, 5, 8, 0);
    queue_hit(0, 5, 8, 0);                    // hid 0 = spawned: never sent
    queue_hit(0xAAAA0002, 0, 8, 0);           // nothing dealt: never sent
    for (int i = 3; i < 20; i++) queue_hit(0xBBBB0000u + (uint32_t)i, 70000, -4, i == 19);  // dealt / after are clamped
    net_edmg_pump(1000);                      // 20 hits = a take of 16 (14 valid) + a take of 4
    CHECK(g_nfq == 0 && NETED.st.tx_pkt == 2 && NETED.st.tx_ent == 18 && NETED.st.tx_send == 2);
    net_edmg_pump(1010);                      // repeats are NET_EDMG_REP_MS apart
    CHECK(NETED.st.tx_send == 2);
    net_edmg_pump(1000 + NET_EDMG_REP_MS);
    CHECK(NETED.st.tx_send == 4);
    net_edmg_pump(1000 + 2 * NET_EDMG_REP_MS);
    CHECK(NETED.st.tx_send == 6 && NETED.nq == 0);
    net_edmg_pump(5000);
    CHECK(NETED.st.tx_send == 6);             // 3 repeats, no more
    // every peer got 6 packets: seq a x3, seq a+1 x3
    for (int i = 0; i < np; i++) {
        uint8_t got[NET_PKT_MAX];
        int cnt[2] = {0, 0}, total = 0;
        uint16_t s0 = 0;
        for (int t = 0; t < 300 && total < 6; t++) {
            int n = nsock_recv(&rx[i], got, sizeof got, NULL, NULL);
            if (n <= 0) { Sleep_ms(2); continue; }
            NetEdEnt e[NET_EDMG_MAX];
            uint8_t pl = 0;
            uint16_t sq = 0;
            int k = net_unpack_edmg(got, n, &pl, &sq, e);
            CHECK(k > 0 && pl == NET.me);
            if (!total) s0 = sq;
            int w = (uint16_t)(sq - s0) == 0 ? 0 : 1;
            CHECK((uint16_t)(sq - s0) <= 1);
            if (w == 0) CHECK(k == 14 && e[0].hid == 0xAAAA0001 && e[0].dealt == 5 && e[0].after == 8 && e[0].f54 == 15 && e[0].f40 == 1 && !(e[0].flags & NEDF_KILL));
            else CHECK(k == 4 && e[0].hid == 0xBBBB0000u + 16 && e[0].dealt == 65535 && e[0].after == 0 && (e[3].flags & NEDF_KILL));
            cnt[w]++, total++;
        }
        CHECK(total == 6 && cnt[0] == 3 && cnt[1] == 3);
    }
    // set edmg 0: the exe ring is still drained, nothing is sent
    g_net_edmg = false;
    queue_hit(0xAAAA0009, 5, 8, 0);
    uint32_t sent = NETED.st.tx_send, pk = NETED.st.tx_pkt;
    net_edmg_pump(6000);
    CHECK(g_nfq == 0 && NETED.st.tx_drop == 1 && NETED.st.tx_send == sent && NETED.st.tx_pkt == pk);
    g_net_edmg = true;
    // an exe that refuses the struct size: logged once, the feature is off
    NETED.take = bad_take;
    g_log[0] = 0;
    net_edmg_pump(7000), net_edmg_pump(7050);
    CHECK(NETED.take == NULL && strstr(g_log, "refused the struct size") && !strstr(strstr(g_log, "refused the struct size") + 1, "refused the struct size"));
    net_set_on(false);
    for (int i = 0; i < np; i++) nsock_close(&rx[i]);
}

// (s166 v6) receiver: only from a known peer, never our own, repeats applied once, off = ignored, counters in net diag
static void test_edmg_receive(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47821;
    net_set_on(true);
    CHECK(NET.on);
    NetSock tx, tx2;
    CHECK(nsock_open(&tx, 47820, false) && nsock_open(&tx2, 47822, false));
    uint8_t pk[NET_PKT_MAX];
    NetEdEnt e[3] = {{0xDEAD0001, 4, 9, 0x11, 0, 0}, {0xDEAD0002, 13, 0, 0x15, 1, NEDF_KILL}, {0xDEAD0003, 1, 1, 15, 0, 0}};
    // a stranger (no HELLO yet) is ignored and takes no slot
    int n = net_pack_edmg(77, 500, e, 3, pk, sizeof pk);
    CHECK(n == NET_HDR + 36 && nsock_send(&tx, lo, 47821, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 1; t++) { net_recv_all(1000); if (NET.st.recv < 1) Sleep_ms(5); }
    CHECK(NET.st.recv == 1 && g_nap == 0 && netslot_count(NET.slot, NET_MAX_PEERS) == 0);
    // our own player id = ignored (echo), no slot
    n = net_pack_edmg(NET.me, 501, e, 3, pk, sizeof pk);
    CHECK(nsock_send(&tx2, lo, 47821, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 2; t++) { net_recv_all(1000); if (NET.st.recv < 2) Sleep_ms(5); }
    CHECK(g_nap == 0 && NET.st.echo == 1 && netslot_count(NET.slot, NET_MAX_PEERS) == 0);
    // HELLO makes the slot; then the packet, sent 3 times, is applied once
    int hn = net_pack_hello(77, 1, "Friend", pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47821, pk, hn) == hn);
    for (int t = 0; t < 100 && NET.st.recv < 3; t++) { net_recv_all(1000); if (NET.st.recv < 3) Sleep_ms(5); }
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1);
    n = net_pack_edmg(77, 502, e, 3, pk, sizeof pk);
    for (int r = 0; r < 3; r++) CHECK(nsock_send(&tx, lo, 47821, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 6; t++) { net_recv_all(1100); if (NET.st.recv < 6) Sleep_ms(5); }
    CHECK(NET.st.recv == 6 && g_nap == 3 && NETED.st.rx_pkt == 1 && NETED.st.rx_dup == 2 && NETED.st.rx_ent == 3);
    CHECK(g_ap[0].hid == 0xDEAD0001 && g_ap[0].dealt == 4 && g_ap[0].after == 9 && g_ap[0].f54 == 0x11 && !g_ap[0].kill);
    CHECK(g_ap[1].hid == 0xDEAD0002 && g_ap[1].kill == 1 && g_ap[1].f40 == 1 && g_ap[1].f54 == 0x15);
    CHECK(NETED.st.rx_queued == 3);
    // pending / ignored answers of the exe are counted
    g_ap_ret = 2;
    n = net_pack_edmg(77, 503, e, 1, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47821, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 7; t++) { net_recv_all(1200); if (NET.st.recv < 7) Sleep_ms(5); }
    g_ap_ret = 0;
    n = net_pack_edmg(77, 504, e, 1, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47821, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 8; t++) { net_recv_all(1200); if (NET.st.recv < 8) Sleep_ms(5); }
    CHECK(NETED.st.rx_pending == 1 && NETED.st.rx_ignored == 1);
    // set edmg 0: received hits are counted and dropped
    g_net_edmg = false;
    int before = g_nap;
    n = net_pack_edmg(77, 505, e, 2, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47821, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 9; t++) { net_recv_all(1300); if (NET.st.recv < 9) Sleep_ms(5); }
    CHECK(g_nap == before && NETED.st.rx_off == 2);
    g_net_edmg = true;
    // an old exe (no apply): counted, no crash
    NETED.apply = NULL;
    n = net_pack_edmg(77, 506, e, 1, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47821, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 10; t++) { net_recv_all(1400); if (NET.st.recv < 10) Sleep_ms(5); }
    CHECK(NETED.st.rx_nofn == 1);
    // a packet with a broken size is bad, not applied
    n = net_pack_edmg(77, 507, e, 2, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47821, pk, n - 5) == n - 5);
    for (int t = 0; t < 100 && NET.st.recv < 11; t++) { net_recv_all(1500); if (NET.st.recv < 11) Sleep_ms(5); }
    CHECK(NET.st.bad == 1);
    g_log[0] = 0;
    net_diag_log();
    CHECK(strstr(g_log, "[net] edmg (on") && strstr(g_log, "rx 5 packets"));
    net_set_on(false);
    nsock_close(&tx), nsock_close(&tx2);
}

// (s176 v10) fire sync, sender: one cast = one entry, 3 sends to every peer, nothing when off / loopback / alone
static void test_fire_send(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47833;
    NetSock rx[2];
    for (int i = 0; i < 2; i++) CHECK(nsock_open(&rx[i], (uint16_t)(47830 + i), false) && net_tg_add(lo, (uint16_t)(47830 + i), true, 0));
    float c[3] = {-1234.5f, 87.25f, 4321.0f};
    net_fire_note(c, false, false, 2.6f, 1.3f, 0);      // net off: nothing queued
    CHECK(NETF.nq == 0);
    net_set_on(true);
    CHECK(NET.on);
    net_fire_note(c, true, true, 2.6f, 1.3f, 1);
    CHECK(NETF.nq == 1);
    net_fire_pump(1000);
    CHECK(NETF.nq == 0 && NETF.st.tx_ent == 1 && NETF.st.tx_pkt == 1 && NETF.st.tx_send == 1);
    net_fire_pump(1010);
    CHECK(NETF.st.tx_send == 1);
    net_fire_pump(1000 + NET_EDMG_REP_MS);
    net_fire_pump(1000 + 2 * NET_EDMG_REP_MS);
    CHECK(NETF.st.tx_send == 3 && NETF.nout == 0);
    net_fire_pump(5000);
    CHECK(NETF.st.tx_send == 3);                 // 3 repeats, no more
    for (int i = 0; i < 2; i++) {
        uint8_t got[NET_PKT_MAX];
        int total = 0;
        uint16_t s0 = 0;
        for (int t = 0; t < 300 && total < 3; t++) {
            int n = nsock_recv(&rx[i], got, sizeof got, NULL, NULL);
            if (n <= 0) { Sleep_ms(2); continue; }
            NetFireEnt e[NET_FIRE_MAX];
            uint8_t pl = 0;
            uint16_t sq = 0;
            CHECK(net_unpack_fire(got, n, &pl, &sq, e) == 1 && pl == NET.me);
            CHECK(e[0].pos[0] == c[0] && e[0].pos[2] == c[2] && e[0].r == 260 && e[0].h == 130 && e[0].flags == (NFRF_LAST | NFRF_BURST | 1 << NFRF_WHO_SHIFT));
            if (!total) s0 = sq;
            CHECK(sq == s0);
            total++;
        }
        CHECK(total == 3);
    }
    // 5 casts in one tick = 2 packets (4 + 1)
    for (int i = 0; i < 5; i++) net_fire_note(c, false, false, 1, 1, 0);
    net_fire_pump(6000);
    CHECK(NETF.st.tx_ent == 6 && NETF.st.tx_pkt == 3);
    // set netfire 0: nothing queued, nothing sent
    g_net_fire = false;
    uint32_t pk = NETF.st.tx_pkt;
    net_fire_note(c, false, false, 1, 1, 0);
    net_fire_pump(7000);
    CHECK(NETF.nq == 0 && NETF.st.tx_pkt == pk);
    g_net_fire = true;
    // a cast turned off after queuing: dropped, counted
    net_fire_note(c, false, false, 1, 1, 0);
    g_net_fire = false;
    net_fire_pump(7100);
    CHECK(NETF.nq == 0 && NETF.st.tx_drop == 1);
    g_net_fire = true;
    // loopback: nothing noted, nothing sent
    g_net_loop = true;
    pk = NETF.st.tx_pkt;
    net_fire_note(c, false, false, 1, 1, 0);
    net_fire_pump(8000);
    CHECK(NETF.nq == 0 && NETF.st.tx_pkt == pk);
    g_net_loop = false;
    net_set_on(false);
    for (int i = 0; i < 2; i++) nsock_close(&rx[i]);
}

// (s176 v10) fire sync, receiver: known peers only, never our own, 3 copies applied once, off ignored, no world / no fn counted, in net diag
static void test_fire_receive(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47841;
    net_set_on(true);
    CHECK(NET.on);
    NetSock tx, tx2;
    CHECK(nsock_open(&tx, 47840, false) && nsock_open(&tx2, 47842, false));
    uint8_t pk[NET_PKT_MAX];
    NetFireEnt e[2] = {{{10, 20, 30}, 260, 130, NFRF_LAST | NFRF_BURST}, {{11, 21, 31}, 100, 50, (uint8_t)(1 << NFRF_WHO_SHIFT)}};
    int n = net_pack_fire(77, 900, e, 2, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47841, pk, n) == n);       // a stranger: ignored, no slot
    for (int t = 0; t < 100 && NET.st.recv < 1; t++) { net_recv_all(1000); if (NET.st.recv < 1) Sleep_ms(5); }
    CHECK(g_nfr == 0 && netslot_count(NET.slot, NET_MAX_PEERS) == 0);
    n = net_pack_fire(NET.me, 901, e, 2, pk, sizeof pk);   // our own id: echo
    CHECK(nsock_send(&tx2, lo, 47841, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 2; t++) { net_recv_all(1000); if (NET.st.recv < 2) Sleep_ms(5); }
    CHECK(g_nfr == 0 && NET.st.echo == 1 && netslot_count(NET.slot, NET_MAX_PEERS) == 0);
    int hn = net_pack_hello(77, 1, "Friend", pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47841, pk, hn) == hn);
    for (int t = 0; t < 100 && NET.st.recv < 3; t++) { net_recv_all(1000); if (NET.st.recv < 3) Sleep_ms(5); }
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1);
    n = net_pack_fire(77, 902, e, 2, pk, sizeof pk);
    for (int r = 0; r < 3; r++) CHECK(nsock_send(&tx, lo, 47841, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 6; t++) { net_recv_all(1100); if (NET.st.recv < 6) Sleep_ms(5); }
    CHECK(g_nfr == 2 && NETF.st.rx_pkt == 1 && NETF.st.rx_dup == 2 && NETF.st.rx_ent == 2);
    CHECK(g_fr[0].pos[0] == 10 && g_fr[0].pos[2] == 30 && g_fr[0].r == 2.6f && g_fr[0].h == 1.3f && g_fr[0].last && g_fr[0].burst && g_fr[0].who == 0);
    CHECK(g_fr[1].pos[1] == 21 && g_fr[1].r == 1.0f && !g_fr[1].last && !g_fr[1].burst && g_fr[1].who == 1);
    CHECK(NETF.nq == 0 && NETF.nout == 0);  // (s176 review: a received fire is never re-sent)
    CHECK(NETF.st.rx_lit == 4 && NETF.st.rx_spk == 6 && NETF.st.rx_bst == 6);
    // set netfire 0: counted, not applied
    g_net_fire = false;
    n = net_pack_fire(77, 903, e, 2, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47841, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 7; t++) { net_recv_all(1200); if (NET.st.recv < 7) Sleep_ms(5); }
    CHECK(g_nfr == 2 && NETF.st.rx_off == 2);
    g_net_fire = true;
    // no world here (loading): the stub says -1; no exe fn: -2
    g_fr_ret = -1;
    n = net_pack_fire(77, 904, e, 1, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47841, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 8; t++) { net_recv_all(1300); if (NET.st.recv < 8) Sleep_ms(5); }
    CHECK(g_nfr == 3 && NETF.st.rx_noworld == 1 && NETF.st.rx_lit == 4);
    g_fr_ret = -2;
    n = net_pack_fire(77, 905, e, 1, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47841, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 9; t++) { net_recv_all(1400); if (NET.st.recv < 9) Sleep_ms(5); }
    CHECK(NETF.st.rx_nofn == 1);
    g_fr_ret = 0;
    // (s176 review) per-frame cap: budget 1 -> the 2nd entry is capped, the pump refills the budget
    NETF.rx_left = 1;
    n = net_pack_fire(77, 907, e, 2, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47841, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 10; t++) { net_recv_all(1450); if (NET.st.recv < 10) Sleep_ms(5); }
    CHECK(g_nfr == 5 && NETF.st.rx_cap == 1 && NETF.rx_left == 0);
    net_fire_pump(1460);
    CHECK(NETF.rx_left == NET_FIRE_RX_FRAME);
    // a packet cut short is bad (counted by the header check)
    n = net_pack_fire(77, 906, e, 2, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47841, pk, n - 3) == n - 3);
    for (int t = 0; t < 100 && NET.st.recv < 11; t++) { net_recv_all(1500); if (NET.st.recv < 11) Sleep_ms(5); }
    CHECK(NET.st.bad == 1 && g_nfr == 5);
    g_log[0] = 0;
    net_diag_log();
    CHECK(strstr(g_log, "[net] fire (on") && strstr(g_log, "rx 5 packets") && strstr(g_log, "per-frame cap 1"));
    net_set_on(false);
    nsock_close(&tx), nsock_close(&tx2);
}

// (s168 v7) enemy sync glue: default OFF = nothing happens; on = 20 Hz packets to the mesh; receive rules; off restores the brains
static void test_esync_glue(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47851;
    NetSock rx, tx;
    CHECK(nsock_open(&rx, 47850, false) && nsock_open(&tx, 47852, false) && net_tg_add(lo, 47850, true, 0));
    net_set_on(true);
    CHECK(NET.on);
    NET.have_feet = true, NET.me = 5, snprintf(NET.name, sizeof NET.name, "Alpha");
    float feet[3] = {0, 0, 0};
    uint64_t T0 = GetTickCount64();
    // default: off -> nothing asked from the exe, nothing sent
    net_esync_tick(feet, T0 + 0);
    uint8_t got[NET_PKT_MAX];
    CHECK(g_es_enum == 0 && !NETES.active && NETES.c.st.tx_pkt == 0);
    // set esync 1: owner of both enemies (alone), hid 0 skipped, 20 Hz
    CHECK(net_live_set("esync", 1) && g_net_esync);
    net_esync_tick(feet, T0 + 0);
    CHECK(g_es_enum == 1 && NETES.active && NETES.c.st.tx_pkt == 1 && NETES.c.st.tx_ent == 2 && (int)NETES.c.st.hid0_skip == 1 && g_es_brain_last == 1);
    net_esync_tick(feet, T0 + 16);
    net_esync_tick(feet, T0 + 40);
    CHECK(NETES.c.st.tx_pkt == 1);
    net_esync_tick(feet, T0 + 50);
    CHECK(NETES.c.st.tx_pkt == 2);
    int n = 0, cnt = 0;
    NetEsEnt e[NET_ESYNC_MAX]; NetEsEv v[NET_ESYNC_EVMAX]; int ne, nv; uint8_t pl; uint16_t sq, sq0 = 0;
    for (int t = 0; t < 100 && cnt < 2; t++) {
        n = nsock_recv(&rx, got, sizeof got, NULL, NULL);
        if (n <= 0) { Sleep_ms(2); continue; }
        CHECK(net_unpack_esync(got, n, &pl, &sq, e, &ne, v, &nv) && pl == 5 && ne == 2 && nv == 0 && e[0].hid == 0x77 && e[1].hid == 0x88 && e[0].anim_hash[0] == 0xABCD);
        if (cnt++ == 0) sq0 = sq; else CHECK((uint16_t)(sq - sq0) == 1);
    }
    CHECK(cnt == 2);
    // not in a world (link_pos NULL) / loading: the exe is not asked
    int en = g_es_enum;
    net_esync_tick(NULL, T0 + 100);
    NET.loading = true, net_esync_tick(feet, T0 + 100), NET.loading = false;
    CHECK(g_es_enum == en);
    // receive: a stranger (no HELLO) takes no slot; our own id is an echo; HELLO + a SNAP make a player with a place
    uint8_t pk[NET_PKT_MAX];
    NetEsEnt in[2]; memset(in, 0, sizeof in);
    in[0].hid = 0x88, in[0].pos[0] = 69, in[1].hid = 0x77, in[1].pos[0] = 2;
    n = net_pack_esync(77, 900, in, 2, NULL, 0, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47851, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 1; t++) { net_recv_all(1200); if (NET.st.recv < 1) Sleep_ms(5); }
    CHECK(NET.st.recv == 1 && netslot_count(NET.slot, NET_MAX_PEERS) == 0 && NETES.c.st.rx_pkt == 0);
    n = net_pack_esync(NET.me, 901, in, 2, NULL, 0, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47851, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 2; t++) { net_recv_all(1200); if (NET.st.recv < 2) Sleep_ms(5); }
    CHECK(NET.st.echo == 1 && netslot_count(NET.slot, NET_MAX_PEERS) == 0);
    int hn = net_pack_hello(77, 1, "Zed", pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47851, pk, hn) == hn);
    for (int t = 0; t < 100 && NET.st.recv < 3; t++) { net_recv_all(1200); if (NET.st.recv < 3) Sleep_ms(5); }
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1);
    NetSnap sn; memset(&sn, 0, sizeof sn);
    sn.player = 77, sn.seq = 1, sn.feet[0] = 70;
    CHECK(peer_on_snap(&NET.slot[0].p, &sn, 1200));
    // his packet: 0x88 first sample -> Zed (70 m, nearest to 70): he owns it; 0x77 is mine (3 m): conflict, ignored
    net_esync_tick(feet, T0 + 200);  // (Zed is a player now and 70 m closer to 0x88: the hand-over is timed)
    CHECK(es_find(&NETES.c, 0x88)->owner == 5 && es_find(&NETES.c, 0x88)->cand == -1);  // (s180: I keep it; only Zed times it)
    in[0].flags |= ES_FL_CLAIM;  // (s180: Zed claims 0x88)
    n = net_pack_esync(77, 902, in, 2, NULL, 0, pk, sizeof pk);
    in[0].flags &= (uint16_t)~ES_FL_CLAIM;
    for (int r = 0; r < 2; r++) CHECK(nsock_send(&tx, lo, 47851, pk, n) == n);  // sent twice: applied once
    for (int t = 0; t < 100 && NET.st.recv < 5; t++) { net_recv_all(1250); if (NET.st.recv < 5) Sleep_ms(5); }
    CHECK(NET.st.recv == 5 && NETES.c.st.rx_pkt == 1 && NETES.c.st.rx_dup == 1);
    CHECK(es_find(&NETES.c, 0x88) && es_find(&NETES.c, 0x88)->owner == 77 && es_find(&NETES.c, 0x77)->owner == 5 && NETES.c.st.rx_conflict == 1 && NETES.c.st.claim_yield == 1);
    // the step now: 0x88 is a puppet (brain off, body driven), 0x77 ours
    net_esync_tick(feet, T0 + 300);
    CHECK(g_es_apply_n == 1 && g_es_brain_last == 0);
    // a reordered older seq is dropped
    n = net_pack_esync(77, 880, in, 1, NULL, 0, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47851, pk, n) == n);
    for (int t = 0; t < 100 && NET.st.recv < 6; t++) { net_recv_all(1300); if (NET.st.recv < 6) Sleep_ms(5); }
    CHECK(NETES.c.st.rx_pkt == 1 && NETES.c.st.rx_dup == 2);
    // a bad (truncated) packet is counted bad
    n = net_pack_esync(77, 903, in, 1, NULL, 0, pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47851, pk, n - 3) == n - 3);
    for (int t = 0; t < 100 && NET.st.recv < 7; t++) { net_recv_all(1300); if (NET.st.recv < 7) Sleep_ms(5); }
    CHECK(NET.st.bad == 1);
    g_log[0] = 0;
    net_diag_log();
    CHECK(strstr(g_log, "[esync] (on"));
    // set esync 0: every brain we stopped runs again, nothing is asked / sent
    int pkts = NETES.c.st.tx_pkt;
    en = g_es_enum;
    CHECK(net_live_set("esync", 0) && !g_net_esync && !NETES.active && g_es_brain_last == 1 && g_es_sens_n >= 1);
    net_esync_tick(feet, T0 + 300);
    CHECK(g_es_enum == en && (int)NETES.c.st.tx_pkt == pkts);
    nsock_close(&rx), nsock_close(&tx);
    net_set_on(false);
}

int main(void) {
    test_inq_64();
    test_rfx_seq_wrap();
    test_rfx_cap();
    test_net_on_clears_fxo();
    test_version_log();
    test_sora_off_parks();
    test_anchor_moves();
    test_mesh_gossip_join_leave();
    test_pins_env_and_table();
    test_self_learn_no_storm();
    test_alias_one_slot();
    test_pin_exclusive_cap1();
    test_addr_filters();
    test_loopback_slot0_and_burst();
    test_edmg_send_repeats_mesh();
    test_edmg_receive();
    test_fire_send();
    test_fire_receive();
    test_esync_glue();
    printf("net_glue_test: %d/%d passed\n", runs - fails, runs);
    return fails ? 1 : 0;
}

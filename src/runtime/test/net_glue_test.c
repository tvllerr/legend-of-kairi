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
// (s197) kill sync's world / exe hooks: sora_world_open (1 open world, 0 not, -1 not known) and linkpos_edmg_reset
static int g_open_v = 1, g_reset_calls, g_reset_ret = 3, g_reset_kills = 2;
static int fake_open(void) { return g_open_v; }
static int fake_reset(int *kills) { g_reset_calls++; if (kills) *kills = g_reset_kills; return g_reset_ret; }

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
    memset(&NETED, 0, sizeof NETED), NETED.bound = true, NETED.take = fake_take, NETED.apply = fake_apply, NETED.reset = fake_reset;
    memset(&NETF, 0, sizeof NETF), g_net_fire = true, g_net_fire_en = false, g_net_fire_log = false, g_nfr = g_fr_ret = 0;
    g_nfq = g_nap = 0, g_ap_ret = 1, g_net_edmg = true, g_net_edmg_log = false;
    g_open_v = 1, g_reset_calls = 0;  // (s197)
    memset(g_fx_gen, 0, sizeof g_fx_gen), memset(g_fx_live, 0, sizeof g_fx_live), memset(g_kills, 0, sizeof g_kills);
    g_nkill = g_drops = 0, g_log[0] = 0;
    memset(&NETR, 0, sizeof NETR), g_net_react = false;  // (s194)
    g_net_eshit = g_net_esdead = false, g_net_eshitms = ES_HIT_HOLD_MS;  // (s195)
    memset(&NETK, 0, sizeof NETK), NETK.bound = true, g_net_kills_log = false;  // (s196: no hooks unless a test sets them)
    NETK.open_fn = fake_open;  // (s197 S2: the open world, as in normal play; a test about shrines / no world says so)
    g_net_kills = NET_KILLS_DEFAULT;  // (s197: the compiled default, ON, in every test, as after a DLL load; a test about killsync 0 says so)
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

// (s191, Melty spec 4.1) who takes part in the enemy election (net_esync_tick): a peer is a player while his SNAPs come (PEER_LIVE)
// and his last SNAP is not NF_LOADING, at the feet of that SNAP. Over UDP the SNAP has no area: a peer in a shrine IS a player, at
// his shrine-local feet (shrine / Divine Beast interiors sit near the world origin) unless his NET_AREA says another map (task 2:
// then "away", test_area_filters). A peer whose SNAPs stop (as when a Melty room hides a player of another area) fades after
// NET_FADE_MS and is no player any more.
static void test_esync_players_area(void) {
    reset_all();
    NET.on = true, NET.have_feet = true, NET.me = 5, snprintf(NET.name, sizeof NET.name, "Alpha");
    g_net_esync = true;
    float feet[3] = {0, 0, 0};
    uint64_t T0 = 100000;
    NetPeerSlot *sl = &NET.slot[0];
    sl->used = true;  // (ip 0: no destination, nothing is sent)
    peer_on_hello(&sl->p, 77, "Zed", T0);
    NetSnap sn;
    memset(&sn, 0, sizeof sn);
    sn.player = 77, sn.seq = 1, sn.feet[0] = 48, sn.feet[1] = 6, sn.feet[2] = 3;  // (inside Vah Ruta: its start point, s190 log)
    CHECK(peer_on_snap(&sl->p, &sn, T0));
    net_esync_tick(feet, T0);
    int zi = es_pidx(&NETES.c, 77);
    CHECK(NETES.c.np == 2 && zi >= 0 && NETES.c.pl[zi].pos[0] == 48 && NETES.c.pl[zi].pos[2] == 3);
    sn.seq = 2, sn.flags |= NF_LOADING;  // his loading screen (leaving the beast): not a player
    peer_on_snap(&sl->p, &sn, T0 + 50);
    net_esync_tick(feet, T0 + 50);
    CHECK(NETES.c.np == 1 && es_pidx(&NETES.c, 77) < 0);
    sn.seq = 3, sn.flags = 0, sn.feet[0] = 1500, sn.feet[2] = -2000;  // back in the world: a player again, at the new map's place
    peer_on_snap(&sl->p, &sn, T0 + 100);
    net_esync_tick(feet, T0 + 100);
    zi = es_pidx(&NETES.c, 77);
    CHECK(NETES.c.np == 2 && zi >= 0 && NETES.c.pl[zi].pos[0] == 1500);
    peer_tick(&sl->p, T0 + 100 + NET_FADE_MS + 1);  // his SNAPs stop: fading = no player
    net_esync_tick(feet, T0 + 100 + NET_FADE_MS + 1);
    CHECK(sl->p.st == PEER_FADING && NETES.c.np == 1 && es_pidx(&NETES.c, 77) < 0);
    es_release_all(&NETES.c);
    NET.on = false;
}

// ---- (s191 task 2, spec 4.1) NET_AREA: who is on which map ----
static int count_in_log(const char *s) {
    int c = 0;
    for (const char *p = strstr(g_log, s); p; p = strstr(p + 1, s)) c++;
    return c;
}
static uint16_t g_fire_seq = 50, g_area_seq = 100;
static int fire_from_zed(void) {  // a Fire Zed cast inside Vah Ruta (its start point, the last tick, bursts on) -> fire calls made here
    NetFireEnt e = {{48, 6, 3}, 260, 130, NFRF_LAST | NFRF_BURST};
    uint8_t pk[NET_PKT_MAX];
    int n = net_pack_fire(77, g_fire_seq++, &e, 1, pk, sizeof pk), was = g_nfr;
    NETF.rx_left = NET_FIRE_RX_FRAME;
    net_fire_recv(0, pk, n);
    return g_nfr - was;
}
static void area_from_zed(const char *a, uint64_t now) {
    uint8_t pk[NET_PKT_MAX];
    net_area_recv(0, pk, net_pack_area(77, g_area_seq++, a, pk, sizeof pk), now);
}
static int players_now(uint64_t now) { float f[3] = {0, 0, 0}; net_esync_tick(f, now); return NETES.c.np; }
static int zed_away(void) { int i = es_pidx(&NETES.c, 77); return i < 0 ? -1 : NETES.c.pl[i].away; }  // (-1 = no player)
// a slot with Zed in it (live, one SNAP at his feet), we are Alpha on `mine`
static void zed_slot(const char *mine, uint64_t now) {
    reset_all();
    net_area_reset(now);
    NET.on = true, NET.have_feet = true, NET.me = 5, snprintf(NET.name, sizeof NET.name, "Alpha");
    g_net_esync = true;
    NetPeerSlot *sl = &NET.slot[0];
    sl->used = true, sl->joined_ms = now;  // (ip 0: no destination, nothing is sent)
    peer_on_hello(&sl->p, 77, "Zed", now);
    NetSnap sn;
    memset(&sn, 0, sizeof sn);
    sn.player = 77, sn.seq = 1, sn.feet[0] = 48, sn.feet[1] = 6, sn.feet[2] = 3;
    peer_on_snap(&sl->p, &sn, now);
    net_set_area(mine);
}

// the filters: old peer = today; another area = hidden (FIRE dropped, no player in the election, slot kept); same area = shown;
// `set area 0` (our area "") = shown; an AREA that is late / older / lost; the slot freed = forgotten
static void test_area_filters(void) {
    uint64_t T = 500000;
    // an OLD peer (never sends AREA): exactly today, whatever map we are on
    zed_slot("MainField", T);
    net_area_tick(T);
    CHECK(!NET.slot[0].hidden && fire_from_zed() == 1 && players_now(T) == 2 && zed_away() == 0 && count_in_log("[net] area:") == 0);
    // (the bug of s191 task 1, fixed) he says Dungeon012, we are on MainField: hidden at once, his FIRE dropped, "away" in the
    // election (his samples still taken, but never nearest / closer)
    area_from_zed("Dungeon012", T + 10);
    CHECK(NET.slot[0].hidden && strstr(g_log, "[net] area: peer \"Zed\" is on Dungeon012 (we are on MainField): hidden"));
    CHECK(fire_from_zed() == 0 && NETF.st.rx_area == 1 && players_now(T + 20) == 2 && zed_away() == 1);
    CHECK(NET.slot[0].used && NET.slot[0].p.st == PEER_LIVE);  // (the slot is held as it is)
    // the same AREA every 2 s: no new line; an OLDER one that comes late is ignored
    for (int i = 1; i <= 5; i++) area_from_zed("Dungeon012", T + 10 + 2000 * (uint64_t)i), net_area_tick(T + 10 + 2000 * (uint64_t)i);
    uint8_t pk[NET_PKT_MAX];
    net_area_recv(0, pk, net_pack_area(77, (uint16_t)(g_area_seq - 3), "MainField", pk, sizeof pk), T + 10100);
    CHECK(NET.slot[0].hidden && NETA.st.rx_dup == 1 && count_in_log("[net] area:") == 1);
    // we walk into HIS shrine: shown (one line), his Fire burns here, he is a player again
    uint64_t t = T + 10200;
    net_set_area("Dungeon012");
    net_area_tick(t);
    CHECK(!NET.slot[0].hidden && strstr(g_log, "[net] area: peer \"Zed\" is on Dungeon012 (we are on Dungeon012): shown") && count_in_log("[net] area:") == 2);
    CHECK(fire_from_zed() == 1 && g_fr[g_nfr - 1].pos[0] == 48 && players_now(t + 10) == 2 && zed_away() == 0);
    // he goes on to another shrine: hidden again
    area_from_zed("Dungeon013", t + 100);
    CHECK(NET.slot[0].hidden && fire_from_zed() == 0);
    // `set area 0` here (area_now() then gives ""): one world, shown
    net_set_area("");
    net_area_tick(t + 200);
    CHECK(!NET.slot[0].hidden && fire_from_zed() == 1 && strstr(g_log, "is on Dungeon013 (we are on (not known)): shown"));
    // area on again on MainField: hidden; then his AREAs stop (lost / he went back to an old build): after NET_AREA_FORGET periods
    // he is "not known" = shown again
    net_set_area("MainField");
    net_area_tick(t + 300);
    CHECK(NET.slot[0].hidden);
    net_area_tick(t + 100 + NET_AREA_FORGET * NET_AREA_MS);
    CHECK(NET.slot[0].hidden);
    net_area_tick(t + 100 + NET_AREA_FORGET * NET_AREA_MS + 1);
    CHECK(!NET.slot[0].hidden && strstr(g_log, "is on (not known) (we are on MainField): shown") && fire_from_zed() == 1);
    // AREA late (lost, then one comes): his Fire before it is used, after it dropped
    zed_slot("MainField", T);
    net_area_tick(T);
    CHECK(fire_from_zed() == 1);
    area_from_zed("RemainsWater", T + 3000);
    CHECK(fire_from_zed() == 0 && NET.slot[0].hidden);
    // the slot freed (he left): his area goes with it; a new slot of the same player starts as "never said"
    net_slot_free(0);
    NetPeerSlot *sl = &NET.slot[0];
    sl->used = true, sl->joined_ms = T + 4000;
    peer_on_hello(&sl->p, 77, "Zed", T + 4000);
    net_area_tick(T + 4000);
    CHECK(!sl->hidden && sl->area_ms == 0 && sl->area[0] == 0 && fire_from_zed() == 1);
    // EDMG is never filtered: a hit from a peer in another area is still applied
    area_from_zed("Dungeon012", T + 4100);
    CHECK(sl->hidden);
    NetEdEnt ed = {0x4444, 7, 30, 15, 1, 0};
    int n = net_pack_edmg(77, 9, &ed, 1, pk, sizeof pk), ap = g_nap;
    net_edmg_recv(0, pk, n, 1000);
    CHECK(g_nap == ap + 1);
    g_log[0] = 0;
    net_diag_log();
    CHECK(strstr(g_log, "[net] area: we are on MainField") && strstr(g_log, "slot 1 \"Zed\" Dungeon012 HIDDEN") && strstr(g_log, "other map "));
    es_release_all(&NETES.c);
    NET.on = false;
}

// the send side (UDP, a pinned peer): nothing while our area was never known; on a change at once; then every NET_AREA_MS; a
// new slot gets it at once; "" goes out once we said a map; loopback sends nothing
static int area_rx(NetSock *rx, char out[NET_AREA_MAX + 1]) {  // AREA packets waiting on rx (the last one's text in out)
    uint8_t got[NET_PKT_MAX];
    int c = 0;
    for (int t = 0; t < 20; t++) {
        int n = nsock_recv(rx, got, sizeof got, NULL, NULL);
        if (n <= 0) { if (c) break; Sleep_ms(2); continue; }
        if (got[3] == NET_AREA && net_unpack_area(got, n, NULL, NULL, out)) c++;
    }
    return c;
}
static void test_area_send(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47861;
    NetSock rx;
    CHECK(nsock_open(&rx, 47860, false) && net_tg_add(lo, 47860, true, 0));
    net_set_on(true);
    CHECK(NET.on);
    char a[NET_AREA_MAX + 1] = "?";
    uint64_t T = 900000;
    net_area_tick(T);
    CHECK(area_rx(&rx, a) == 0 && NETA.st.tx == 0);  // (area not known yet: nothing to say)
    net_set_area("MainField");
    net_area_tick(T + 10);
    CHECK(area_rx(&rx, a) == 1 && !strcmp(a, "MainField"));
    net_area_tick(T + 100), net_area_tick(T + 10 + NET_AREA_MS - 1);
    CHECK(area_rx(&rx, a) == 0);
    net_area_tick(T + 10 + NET_AREA_MS);
    CHECK(area_rx(&rx, a) == 1 && !strcmp(a, "MainField"));
    net_set_area("Dungeon012");
    net_area_tick(T + 2100);
    CHECK(area_rx(&rx, a) == 1 && !strcmp(a, "Dungeon012"));
    NET.slot[1].used = true, NET.slot[1].joined_ms = T + 2200;  // (a slot appears: ours goes out at once)
    net_area_tick(T + 2200);
    CHECK(area_rx(&rx, a) == 1);
    memset(&NET.slot[1], 0, sizeof NET.slot[1]);
    net_set_area("");
    net_area_tick(T + 2300);
    CHECK(area_rx(&rx, a) == 1 && a[0] == 0 && NETA.st.tx == 5);
    g_net_loop = true;
    net_set_area("MainField");
    net_area_tick(T + 2400);
    CHECK(area_rx(&rx, a) == 0);
    g_net_loop = false;
    net_set_on(false);
    nsock_close(&rx);
}

// ---- (s194) NET_REACT: the KH2 hit reaction on an enemy another game owns (sora_net_react.h) ----
static EsEnt *es_put(uint32_t hid, bool local, int owner, int brain) {
    EsEnt *e = es_get(&NETES.c, hid, 1000);
    e->local = local, e->owner = owner, e->brain = brain;
    return e;
}
static int react_rx(NetSock *rx, NetReactEnt *last, int *nent, uint16_t *seq, uint8_t *pl) {  // NET_REACT packets waiting on rx
    int c = 0;
    uint8_t got[NET_PKT_MAX];
    for (int t = 0; t < 60; t++) {
        int n = nsock_recv(rx, got, sizeof got, NULL, NULL);
        if (n <= 0) { if (c >= 3) break; Sleep_ms(3); continue; }
        if (got[3] != NET_REACT) continue;
        int k = net_unpack_react(got, n, pl, seq, last, NULL);
        if (k > 0) c++, *nent = k;
    }
    return c;
}
// the attacker: a hit on a puppet sends (no local reaction), a hit on our own / an unsynced enemy stays local; netreact 0 = as before
static void test_react_attacker(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47941;
    NetSock rx;
    CHECK(nsock_open(&rx, 47940, false) && net_tg_add(lo, 47940, true, 0));
    net_set_on(true);
    CHECK(NET.on);
    NET.me = 5;
    g_net_esync = true, NETES.active = true, NETES.c.me = 5;
    es_put(0x88, true, 77, 0);   // a puppet here: Zed owns it, its brain is frozen by us
    es_put(0x77, true, 5, 1);    // ours
    es_put(0x99, true, 77, 1);   // Zed's, but he went quiet: its brain runs here again
    es_put(0xaa, false, 77, 0);  // Zed's, not loaded here this step
    float from[3] = {-1234.5f, 87.0f, 4321.0f};
    // the compiled default: off = today's behaviour exactly (every hit local, nothing queued, no log)
    CHECK(!g_net_react && !net_react_wanted() && !net_react_hit(0x88, from, 3, 125, 0, 7, "blade", false) && NETR.tx.n == 0 && !strstr(g_log, "[react]"));
    CHECK(net_live_set("netreact", 1) && g_net_react && net_react_wanted() && strstr(g_log, "[react] on"));
    // own / quiet owner / not loaded / unknown / hid 0 (a spawned enemy): local, nothing sent
    CHECK(!net_react_hit(0x77, from, 3, 125, 0, 7, "blade", false) && !net_react_hit(0x99, from, 3, 125, 0, 7, "blade", false));
    CHECK(!net_react_hit(0xaa, from, 3, 125, 0, 7, "blade", false) && !net_react_hit(0x1234, from, 3, 125, 0, 7, "blade", false));
    CHECK(!net_react_hit(0, from, 4, 30, 0, 7, "thunder", false) && NETR.tx.n == 0 && NETR.st.hit_local == 5);
    // the puppet: no local reaction (true), one entry queued with what ah_hit would have used; a class-1 hit = nothing at all
    g_log[0] = 0;
    CHECK(net_react_hit(0x88, from, 3, 125, 0, 7, "blade", false) && NETR.tx.n == 1 && NETR.st.hit_send == 1);
    CHECK(strstr(g_log, "[react] tx hid=00000088 blade (blade) react 3 kb 125/0 f54 0x7 from -1234.5 87.0 4321.0: puppet here (owner player 77), no local reaction"));
    CHECK(net_react_hit(0x88, from, 1, 0, 0, 7, "blade", false) && NETR.tx.n == 1 && NETR.st.hit_skip == 1);
    float spell[3] = {10, 20, 30};
    CHECK(net_react_hit(0x88, spell, 4, 30, 0, 0x14, "thunder", true) && NETR.tx.n == 2 && strstr(g_log, "f54 0x14 form"));
    // the pump: one packet (2 entries), 3 copies with the SAME seq, 40 ms apart, then nothing
    net_react_pump(2000);
    CHECK(NETR.tx.n == 0 && NETR.st.tx_pkt == 1 && NETR.st.tx_send == 1);
    net_react_pump(2010);
    CHECK(NETR.st.tx_send == 1);
    net_react_pump(2000 + NET_EDMG_REP_MS), net_react_pump(2000 + 2 * NET_EDMG_REP_MS), net_react_pump(6000);
    CHECK(NETR.st.tx_send == 3 && NETR.nout == 0);
    NetReactEnt got[NET_REACT_MAX];
    int nent = 0;
    uint16_t sq = 0;
    uint8_t pl = 0;
    CHECK(react_rx(&rx, got, &nent, &sq, &pl) == 3 && nent == 2 && pl == 5);
    CHECK(got[0].hid == 0x88 && got[0].kind == NRK_BLADE && got[0].react == 3 && got[0].kb1 == 125 && got[0].f54 == 7 && got[0].flags == 0 && got[0].from[0] == from[0] && got[0].from[2] == from[2]);
    CHECK(got[1].kind == NRK_THUNDER && got[1].react == 4 && got[1].kb1 == 30 && got[1].f54 == 0x14 && got[1].flags == NRF_FORM && got[1].from[1] == 20);
    // 10 puppet hits in one frame = 2 packets (8 + 2)
    for (int i = 0; i < 10; i++) net_react_hit(0x88, from, 3, 125, 0, 7, "blade", false);
    net_react_pump(7000);
    CHECK(NETR.st.tx_pkt == 3 && NETR.nout == 2);
    // netreact 0: every hit local again, the queues emptied
    CHECK(net_live_set("netreact", 0) && !g_net_react && NETR.nout == 0 && NETR.tx.n == 0 && !net_react_hit(0x88, from, 3, 125, 0, 7, "blade", false));
    CHECK(net_live_set("netreact", 1));
    // loopback: queued hits are dropped, nothing goes out
    g_net_loop = true;
    net_react_hit(0x88, from, 3, 125, 0, 7, "blade", false);
    net_react_pump(8000);
    CHECK(NETR.st.tx_drop == 1 && NETR.st.tx_pkt == 3 && NETR.tx.n == 0);
    g_net_loop = false;
    // esync off: no puppets anywhere = every hit local
    g_net_esync = false;
    CHECK(!net_react_wanted() && !net_react_hit(0x88, from, 3, 125, 0, 7, "blade", false));
    g_net_esync = true;
    g_log[0] = 0;
    net_diag_log();
    CHECK(strstr(g_log, "[react] (on): hits on own / unsynced enemies") && strstr(g_log, "on puppets 13 sent + 1 skipped"));
    net_set_on(false);
    CHECK(NETR.tx.n == 0 && NETR.nout == 0 && !net_react_wanted());
    nsock_close(&rx);
    g_net_react = false;
}

// the owner: entries of known peers queued; played only for an enemy we own, have loaded and alive; repeats once; the rest counted
static struct { int n; uint64_t actor; float epos[3], from[3]; int kind, flags, react, kb1, f54; } g_rp;
static int g_rfind;
static bool rp_find(uint32_t hid, uint64_t *actor, float epos[3], int32_t *life, void *user) {
    (void)user;
    g_rfind++;
    if (hid == 0x66) return false;  // (ours in the table, but not in the enemy list now)
    *actor = 0x5000 + hid, *life = hid == 0x55 ? 0 : 12, epos[0] = 1, epos[1] = 2, epos[2] = (float)hid;
    return true;
}
static void rp_play(uint64_t actor, const float epos[3], const NetReactEnt *e, void *user) {
    (void)user;
    g_rp.n++, g_rp.actor = actor, memcpy(g_rp.epos, epos, 12), memcpy(g_rp.from, e->from, 12);
    g_rp.kind = e->kind, g_rp.flags = e->flags, g_rp.react = e->react, g_rp.kb1 = e->kb1, g_rp.f54 = e->f54;
}
static int react_send(NetSock *tx, uint32_t lo, uint8_t player, uint16_t seq, const NetReactEnt *e, int n, int copies, uint32_t recv_was, uint64_t now) {
    uint8_t pk[NET_PKT_MAX];
    int len = net_pack_react(player, seq, e, n, pk, sizeof pk);
    for (int r = 0; r < copies; r++) nsock_send(tx, lo, 47943, pk, len);
    for (int t = 0; t < 100 && NET.st.recv < recv_was + (uint32_t)copies; t++) { net_recv_all(now); if (NET.st.recv < recv_was + (uint32_t)copies) Sleep_ms(5); }
    return (int)(NET.st.recv - recv_was);
}
static void test_react_owner(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47943;
    net_set_on(true);
    CHECK(NET.on);
    NET.me = 5;
    NetSock tx, tx2;
    CHECK(nsock_open(&tx, 47942, false) && nsock_open(&tx2, 47944, false));
    g_net_esync = true, NETES.active = true, NETES.c.me = 5;
    es_put(0x77, true, 5, 1), es_put(0x66, true, 5, 1), es_put(0x55, true, 5, 1);  // ours (0x66: gone from the list, 0x55: dead)
    es_put(0x88, true, 77, 0);  // Zed's (a puppet here)
    memset(&g_rp, 0, sizeof g_rp), g_rfind = 0;
    float f[3] = {40, 6, -3};
    NetReactEnt e[5];
    e[0] = nr_ent_make(0x77, 3, 125, 0, 7, "blade", f, true);
    e[1] = nr_ent_make(0x88, 3, 125, 0, 7, "blade", f, false);
    e[2] = nr_ent_make(0x66, 4, 30, 0, 0x14, "thunder", f, false);
    e[3] = nr_ent_make(0x55, 4, 30, 0, 0x14, "thunder", f, false);
    e[4] = nr_ent_make(0x44, 11, -100, 45, 0xf, "wisdom", f, false);
    g_net_react = true;
    // a stranger (no HELLO): no slot, nothing taken; our own id: an echo
    CHECK(react_send(&tx, lo, 77, 10, e, 5, 1, NET.st.recv, 3000) == 1 && netslot_count(NET.slot, NET_MAX_PEERS) == 0 && NETR.st.rx_pkt == 0 && net_react_pending() == 0);
    CHECK(react_send(&tx2, lo, 5, 11, e, 5, 1, NET.st.recv, 3000) == 1 && NET.st.echo == 1 && NETR.st.rx_pkt == 0);
    uint8_t pk[NET_PKT_MAX];
    int hn = net_pack_hello(77, 1, "Zed", pk, sizeof pk);
    CHECK(nsock_send(&tx, lo, 47943, pk, hn) == hn);
    for (int t = 0; t < 100 && netslot_count(NET.slot, NET_MAX_PEERS) < 1; t++) { net_recv_all(3000); Sleep_ms(3); }
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1 && NET.slot[0].p.st == PEER_LIVE);
    // Zed's packet, 3 UDP copies: taken once, 5 entries queued (nothing played before the frame body runs)
    g_log[0] = 0;
    CHECK(react_send(&tx, lo, 77, 65535, e, 5, 3, NET.st.recv, 3000) == 3);
    CHECK(NETR.st.rx_pkt == 1 && NETR.st.rx_dup == 2 && NETR.st.rx_ent == 5 && net_react_pending() == 5 && g_rp.n == 0);
    CHECK(strstr(g_log, "[react] rx peer 1 (seq 65535) -> dup (a repeat copy)"));
    // the frame body: 0x77 played (ours, loaded, alive), 0x88 / 0x44 not ours, 0x66 not loaded, 0x55 dead
    g_log[0] = 0;
    CHECK(net_react_play(3010, rp_find, rp_play, NULL) == 1 && net_react_pending() == 0);
    CHECK(g_rp.n == 1 && g_rp.actor == 0x5077 && g_rp.epos[2] == (float)0x77 && g_rp.from[0] == 40 && g_rp.from[2] == -3 && g_rp.kind == NRK_BLADE && g_rp.flags == NRF_FORM);
    CHECK(g_rp.react == 3 && g_rp.kb1 == 125 && g_rp.f54 == 7 && !strcmp(net_react_what(g_rp.kind), "blade"));
    CHECK(NETR.st.played == 1 && NETR.st.not_owner == 2 && NETR.st.not_loaded == 1 && NETR.st.dead == 1 && g_rfind == 3);  // (no lookup for the ones we do not own)
    CHECK(strstr(g_log, "[react] rx peer 1 hid=00000077 blade react 3 kb 125/0 f54 0x7 form from 40.0 6.0 -3.0 -> played"));
    CHECK(strstr(g_log, "hid=00000088 blade react 3 kb 125/0 f54 0x7 from 40.0 6.0 -3.0 -> not owner (owner 77)") && strstr(g_log, "hid=00000044 other react 11 kb -100/45"));
    CHECK(strstr(g_log, "hid=00000066 thunder") && strstr(g_log, "-> not loaded here") && strstr(g_log, "hid=00000055 thunder react 4 kb 30/0 f54 0x14 from 40.0 6.0 -3.0 -> dead"));
    // the seq wraps (65535 -> 0): a new packet; an old copy of 65535 again: dup
    CHECK(react_send(&tx, lo, 77, 0, e, 1, 1, NET.st.recv, 3100) == 1 && NETR.st.rx_pkt == 2 && net_react_pending() == 1);
    CHECK(react_send(&tx, lo, 77, 65535, e, 1, 1, NET.st.recv, 3100) == 1 && NETR.st.rx_dup == 3 && net_react_pending() == 1);
    // not played in time (a menu): late
    CHECK(net_react_play(3100 + NR_RX_TTL_MS + 1, rp_find, rp_play, NULL) == 0 && NETR.st.late == 1 && g_rp.n == 1);
    // the owner changed between the packet and the frame (a hand-over): not owner, nothing played
    CHECK(react_send(&tx, lo, 77, 1, e, 1, 1, NET.st.recv, 3200) == 1);
    es_find(&NETES.c, 0x77)->owner = 77;
    CHECK(net_react_play(3210, rp_find, rp_play, NULL) == 0 && NETR.st.not_owner == 3);
    es_find(&NETES.c, 0x77)->owner = 5;
    // esync off here: we own nothing
    CHECK(react_send(&tx, lo, 77, 2, e, 1, 1, NET.st.recv, 3300) == 1);
    g_net_esync = false;
    CHECK(net_react_play(3310, rp_find, rp_play, NULL) == 0 && NETR.st.not_owner == 4);
    g_net_esync = true;
    // Zed is on another map (AREA): hidden, never queued
    NET.slot[0].hidden = true;
    g_log[0] = 0;
    CHECK(react_send(&tx, lo, 77, 3, e, 2, 1, NET.st.recv, 3400) == 1 && NETR.st.rx_hidden == 2 && net_react_pending() == 0);
    CHECK(strstr(g_log, "hid=00000077 blade react 3 f54 0x7 (seq 3) -> hidden (he is on another map)"));
    NET.slot[0].hidden = false;
    // a slot that is not a known peer (st NONE): unknown peer
    NET.slot[0].p.st = PEER_NONE;
    {
        uint8_t b[NET_PKT_MAX];
        int n = net_pack_react(77, 4, e, 1, b, sizeof b);
        net_react_recv(0, b, n, 3500);
    }
    CHECK(NETR.st.rx_unknown == 1 && net_react_pending() == 0);
    NET.slot[0].p.st = PEER_LIVE;
    // netreact 0: received entries dropped quietly; queued ones are cleared by the live switch
    CHECK(react_send(&tx, lo, 77, 5, e, 1, 1, NET.st.recv, 3600) == 1 && net_react_pending() == 1);
    CHECK(net_live_set("netreact", 0) && net_react_pending() == 0);
    g_log[0] = 0;
    CHECK(react_send(&tx, lo, 77, 6, e, 2, 1, NET.st.recv, 3700) == 1 && NETR.st.rx_off == 2 && net_react_pending() == 0 && !strstr(g_log, "[react]"));
    g_net_react = true;
    // a cut packet is bad at the header check; a whole one with only bad entries is a bad NET_REACT packet
    uint32_t bad = NET.st.bad;
    {
        int n = net_pack_react(77, 7, e, 1, pk, sizeof pk);
        nsock_send(&tx, lo, 47943, pk, n - 1);
        for (int t = 0; t < 100 && NET.st.bad == bad; t++) { net_recv_all(3800); if (NET.st.bad == bad) Sleep_ms(5); }
        CHECK(NET.st.bad == bad + 1 && NETR.st.rx_bad == 0);
        n = net_pack_react(77, 8, e, 1, pk, sizeof pk);
        net_w32f(pk + NET_HDR + 12, NAN);
        uint32_t r0 = NET.st.recv;
        nsock_send(&tx, lo, 47943, pk, n);
        for (int t = 0; t < 100 && NET.st.recv == r0; t++) { net_recv_all(3800); if (NET.st.recv == r0) Sleep_ms(5); }
        CHECK(NETR.st.rx_bad == 1 && NETR.st.rx_bad_ent == 1 && net_react_pending() == 0);
    }
    // a received reaction is never sent on
    CHECK(NETR.tx.n == 0 && NETR.nout == 0);
    g_log[0] = 0;
    net_diag_log();
    CHECK(strstr(g_log, "[react] (on)") && strstr(g_log, "played 1, not owner 4, not loaded 1, dead 1, late 1, other map 2, unknown peer 1, off 2"));
    net_set_on(false);
    nsock_close(&tx), nsock_close(&tx2);
    g_net_esync = false, g_net_react = false;
}

// ---- (s195) eshit / esdead: the live keys, the hit note glue, a dead player = "away" in the election ----
static void zed_snap(uint16_t seq, uint16_t flags, uint64_t now) {
    NetSnap sn;
    memset(&sn, 0, sizeof sn);
    sn.player = 77, sn.seq = seq, sn.flags = flags, sn.feet[0] = 48, sn.feet[1] = 6, sn.feet[2] = 3;
    peer_on_snap(&NET.slot[0].p, &sn, now);
}
static void test_eshit_glue(void) {
    reset_all();
    // the compiled defaults: hit claim off, hold 4000 ms, dead players count as before
    CHECK(!g_net_eshit && g_net_eshitms == ES_HIT_HOLD_MS && ES_HIT_HOLD_MS == 4000 && !g_net_esdead);
    CHECK(net_live_set("eshit", 1) && g_net_eshit && strstr(g_log, "[esync] hit claim on"));
    g_log[0] = 0;
    CHECK(net_live_set("eshit", 1) && !strstr(g_log, "[esync]"));  // (a live rewrite re-applies every line: no new line)
    CHECK(net_live_set("eshitms", 100) && g_net_eshitms == ES_HIT_HOLD_MIN && strstr(g_log, "[esync] hit hold 500 ms (clamped)"));
    CHECK(net_live_set("eshitms", 999999) && g_net_eshitms == ES_HIT_HOLD_MAX && net_live_set("eshitms", 2500) && g_net_eshitms == 2500 && strstr(g_log, "[esync] hit hold 2500 ms\n"));
    CHECK(net_live_set("esdead", 1) && g_net_esdead && strstr(g_log, "[esync] dead players away"));
    CHECK(net_live_set("esdead", 0) && !g_net_esdead && strstr(g_log, "[esync] dead players count as before"));
    CHECK(net_live_set("eshit", 0) && !g_net_eshit && strstr(g_log, "[esync] hit claim off"));
    // the hit note: esync off / not active / net off / hid 0 = nothing; else stored on the enemy, counted
    NET.on = true, NET.me = 5, NETES.c.me = 5;
    EsEnt *e = es_put(0x88, true, 77, 0);
    net_esync_note_hit(0x88);
    CHECK(!net_esync_hit_wanted() && e->hit_ms == 0 && NETES.c.st.hit_notes == 0);
    g_net_esync = true;
    net_esync_note_hit(0x88);
    CHECK(!net_esync_hit_wanted() && e->hit_ms == 0 && NETES.c.st.hit_notes == 0);  // (not active yet: no tick ran)
    NETES.active = true;
    net_esync_note_hit(0);
    CHECK(net_esync_hit_wanted() && NETES.c.st.hit_notes == 0);
    net_esync_note_hit(0x88);
    CHECK(e->hit_ms != 0 && NETES.c.st.hit_notes == 1);  // (eshit 0: stored all the same, only es_elect / es_step use it with eshit 1)
    NET.on = false;
    net_esync_note_hit(0x88);
    CHECK(NETES.c.st.hit_notes == 1);
    es_release_all(&NETES.c);
    // the tick hands the keys to the core; a dead peer (SNAP NF_DEAD) / me dead = "away" only with esdead 1
    uint64_t T = 700000;
    zed_slot("", T);
    g_net_eshitms = 2500;  // (zed_slot resets the keys)
    zed_snap(2, NF_DEAD, T);
    CHECK(players_now(T) == 2 && zed_away() == 0 && !NETES.c.hit_on && NETES.c.hit_hold_ms == 2500 && !NETES.c.pl[0].away);
    g_net_esdead = true, g_net_eshit = true;
    CHECK(players_now(T + 16) == 2 && zed_away() == 1 && NETES.c.hit_on && !NETES.c.pl[0].away);
    zed_snap(3, 0, T + 20);  // (he stands again)
    CHECK(players_now(T + 32) == 2 && zed_away() == 0);
    NET.dead = true;
    CHECK(players_now(T + 48) == 2 && NETES.c.pl[0].id == 5 && NETES.c.pl[0].away && zed_away() == 0);
    g_net_esdead = false;
    CHECK(players_now(T + 64) == 2 && !NETES.c.pl[0].away);
    NET.dead = false;
    zed_snap(4, NF_DEAD | NF_LOADING, T + 70);  // (loading still means no player at all)
    CHECK(players_now(T + 80) == 1);
    // net diag: the s195 line
    g_log[0] = 0;
    net_diag_log();
    CHECK(strstr(g_log, "[esync] hit claim on (hold 2500 ms): hits noted 0, hit claims 0") && strstr(g_log, "dead players count as before (set esdead 1)"));
    es_release_all(&NETES.c);
    NET.on = false, g_net_esync = false, g_net_eshit = false, g_net_eshitms = ES_HIT_HOLD_MS;
}

// (s195b) the claiming hit plays its reaction HERE: sora_airhold.h ah_hit notes the hit first (net_esync_note_hit: a qualifying hit
// claims at once), then asks netreact (net_react_hit): the enemy is ours now = local, no NET_REACT. A hit that does not qualify
// (owner HOT) or eshit 0: NET_REACT to the owner as before. (The two calls below are ah_hit's order.)
static void test_eshit_react(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47961;
    NetSock rx;
    CHECK(nsock_open(&rx, 47960, false) && net_tg_add(lo, 47960, true, 0));
    net_set_on(true);
    CHECK(NET.on);
    NET.me = 5, snprintf(NET.name, sizeof NET.name, "Alpha");
    g_net_esync = true, NETES.active = true, NETES.c.me = 5, g_net_react = true;
    g_net_eshit = true, NETES.c.hit_on = true;  // (net_esync_tick copies the key; no tick here)
    EsPlayer pl[2];
    memset(pl, 0, sizeof pl);
    pl[0].id = 5, snprintf(pl[0].name, sizeof pl[0].name, "Alpha"), pl[0].pos[0] = 40;
    pl[1].id = 77, snprintf(pl[1].name, sizeof pl[1].name, "Zed"), pl[1].pos[0] = 18;
    es_set_players(&NETES.c, pl, 2);
    uint64_t now = GetTickCount64();
    EsEnt *e = es_put(0x88, true, 77, 0), *h = es_put(0x99, true, 77, 0), *o = es_put(0xaa, true, 77, 0);
    e->have_pos = h->have_pos = o->have_pos = true, e->alive_ms = h->alive_ms = o->alive_ms = now;
    h->o_hot = h->o_held = true;  // (Zed keeps hitting 0x99)
    float from[3] = {1, 2, 3};
    g_log[0] = 0;
    // the claiming hit: ours at the note (brain told on), so netreact says local: no NET_REACT, ah_hit_ex plays it here
    CHECK(net_esync_note_hit(0x88) && e->owner == 5 && e->brain == 1 && g_es_brain_last == 1 && strstr(g_log, "owner Zed -> Alpha (hit: claim)"));
    CHECK(!net_react_hit(0x88, from, 3, 125, 0, 7, "blade", false) && NETR.tx.n == 0 && NETR.st.hit_local == 1 && NETR.st.hit_send == 0 && NETES.c.st.hit_sync == 1);
    // owner HOT: no claim, the reaction goes to him as before
    CHECK(!net_esync_note_hit(0x99) && h->owner == 77 && h->brain == 0 && net_react_hit(0x99, from, 3, 125, 0, 7, "blade", false) && NETR.tx.n == 1 && NETR.st.hit_send == 1);
    // eshit 0: exactly as before (a puppet hit sends)
    g_net_eshit = false, NETES.c.hit_on = false;
    CHECK(!net_esync_note_hit(0xaa) && o->owner == 77 && net_react_hit(0xaa, from, 4, 30, 0, 0x14, "thunder", false) && NETR.tx.n == 2 && NETR.st.hit_local == 1);
    // on the wire: one NET_REACT packet with 0x99 + 0xaa only
    net_react_pump(now);
    NetReactEnt got[NET_REACT_MAX];
    int nent = 0;
    uint16_t sq = 0;
    uint8_t who = 0;
    CHECK(react_rx(&rx, got, &nent, &sq, &who) >= 1 && nent == 2 && got[0].hid == 0x99 && got[1].hid == 0xaa && who == 5);
    CHECK(NETES.c.st.claims == 1 && NETES.c.st.hit_claims == 1 && NETES.c.st.hit_notes == 3);
    net_set_on(false);
    nsock_close(&rx);
    es_release_all(&NETES.c);
    g_net_esync = false, g_net_react = false, NETES.active = false;
}

// ---- (s196) killsync: the kill list, catch-up asks / answers over real sockets, the apply scan (sora_net_kills.h) ----
static uint32_t g_kl_hids[64];
static int g_kl_n, g_kl_calls, g_moons_v = -1;
static int fake_loaded(uint32_t *h, int max) { g_kl_calls++; int n = g_kl_n < max ? g_kl_n : max; memcpy(h, g_kl_hids, sizeof *h * (size_t)n); return n; }
static int fake_moons(void) { return g_moons_v; }
// NET_KILLS packets waiting on rx (others skipped): up to max into m[]
static int kills_rx(NetSock *rx, NetKillsMsg *m, int max) {
    int c = 0, idle = 0;
    uint8_t got[NET_PKT_MAX + 64];
    for (int t = 0; t < 200 && c < max; t++) {
        int n = nsock_recv(rx, got, sizeof got, NULL, NULL);
        if (n <= 0) { if (++idle > 6) break; Sleep_ms(2); continue; }
        idle = 0;
        if (n > 3 && got[3] == NET_KILLS && net_unpack_kills(got, n, NULL, NULL, &m[c])) c++;
    }
    return c;
}
static void kills_slot(int k, uint32_t ip, uint16_t port, uint8_t player, const char *name, uint64_t joined) {  // (a peer slot as the receive makes it)
    NetPeerSlot *sl = &NET.slot[k];
    memset(sl, 0, sizeof *sl);
    sl->used = true, sl->ip = ip, sl->port = port, sl->joined_ms = sl->last_recv_ms = joined;
    peer_reset(&sl->p, player), peer_set_name(&sl->p, name), sl->p.st = PEER_LIVE, sl->p.last_ms = joined;
}
// Zed (the rx socket) sends one NET_KILLS packet to us and we receive it
static void kills_from(NetSock *tx, uint32_t lo, const uint8_t *b, int n, uint64_t now) {
    uint32_t r0 = NET.st.recv;
    CHECK(n > 0 && nsock_send(tx, lo, NET.port, b, n) == n);
    for (int t = 0; t < 100 && NET.st.recv == r0; t++) { net_recv_all(now); if (NET.st.recv == r0) Sleep_ms(3); }
}
static float g_kfeet[3] = {10, 20, 30};
static int kills_count(const char *h, const char *n) { int c = 0; for (const char *p = strstr(h, n); p; p = strstr(p + 1, n)) c++; return c; }

// killsync 0 (the compiled default until s197; now `set killsync 0`): own kills and EDMG kills are not recorded, a new peer is not
// asked, his ask is not answered, nothing NET_KILLS goes out, no "[kills]" line after net on but the diag one
static void test_kills_off(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47981;
    NetSock rx;
    CHECK(nsock_open(&rx, 47980, false));
    CHECK(g_net_kills && net_live_set("killsync", 0) && !g_net_kills && strstr(g_log, "[kills] kill sync off (set killsync 1)"));  // (s197: off by the key)
    net_set_on(true);
    NET.me = 5;
    CHECK(NET.on && !g_net_kills && kills_count(g_log, "[kills] kill sync OFF (set killsync 0: not the compiled default)") == 1);
    g_log[0] = 0;  // (from here on: no "[kills]" line at all)
    NETK.loaded = fake_loaded, NETK.moons_fn = fake_moons;
    uint64_t T = 3000000;
    kills_slot(0, lo, 47980, 77, "Zed", T);
    queue_hit(0xA1, 40, 0, 1);
    net_edmg_pump(T);
    uint8_t pk[NET_KILLS_PKT];
    NetEdEnt ed = {0xA2, 50, 0, 15, 1, NEDF_KILL};
    int n = net_pack_edmg(77, 9, &ed, 1, pk, sizeof pk);
    kills_from(&rx, lo, pk, n, T + 1);
    CHECK(g_nap == 1 && g_ap[0].hid == 0xA2 && g_ap[0].kill == 1);  // (the EDMG apply as before)
    g_kl_n = 1, g_kl_hids[0] = 0xA2, g_moons_v = 3;
    for (int i = 0; i < 5; i++) net_kills_tick(g_kfeet, T + 2 + 200 * i);
    kills_from(&rx, lo, pk, net_pack_kills_ask(77, 1, 0x4242, 0, 0, NET_KILLS_MOON_NONE, pk, sizeof pk), T + 1100);
    for (int i = 0; i < 5; i++) net_kills_tick(g_kfeet, T + 1200 + 20 * i);
    NetKillsMsg m[4];
    CHECK(kills_rx(&rx, m, 4) == 0);
    CHECK(NETK.L.n == 0 && NETK.st.rx_off == 1 && NETK.st.ask_tx == 0 && NETK.st.part_tx == 0 && g_kl_calls == 0 && g_nap == 1);
    CHECK(!strstr(g_log, "[kills]"));
    net_diag_log();
    CHECK(strstr(g_log, "[kills] (OFF (set killsync 1))"));
    // (s197) `set killsync 1` again: the key's own line, and Zed (there all along) counts as new = asked at the next tick
    CHECK(net_live_set("killsync", 1) && g_net_kills && strstr(g_log, "[kills] kill sync on: placed enemies killed"));
    net_kills_tick(g_kfeet, T + 2000);
    CHECK(kills_rx(&rx, m, 4) == 1 && m[0].op == NKO_ASK && NETK.st.ask_tx == 1 && strstr(g_log, "asking peer 1 \"Zed\""));
    net_set_on(false);
    nsock_close(&rx);
}

// (s197) killsync is ON with no live key: the compiled default (also what a DLL hot reload starts with). Net on says so in ONE
// line (per net on, never per frame); own kills are kept, a new peer is asked, his ask answered; `set killsync 1` changes nothing
static void test_kills_default_on(void) {
    reset_all();
    CHECK(NET_KILLS_DEFAULT && g_net_kills);
    CHECK(!NETK.moons_known && !NETK.idle && !NETK.want_all && NETK.L.n == 0);  // (what `set killsync 1` would set up = the zero state)
    for (int k = 0; k < NET_MAX_PEERS; k++) CHECK(NETK.joined[k] == 0);
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47987;
    NetSock rx;
    CHECK(nsock_open(&rx, 47986, false));
    net_set_on(true);
    NET.me = 5;
    const char *on_line = "[kills] kill sync on (the compiled default since s197, no live key needed; set killsync 0 to change it)";
    CHECK(NET.on && kills_count(g_log, on_line) == 1 && kills_count(g_log, "[kills]") == 1);
    CHECK(strstr(g_log, "[net] on: UDP port 47987") < strstr(g_log, on_line));  // (right after the "[net] on:" line)
    NETK.loaded = fake_loaded, NETK.moons_fn = fake_moons, g_moons_v = 1, g_kl_n = 0, g_kl_calls = 0;
    uint64_t T = 6000000;
    for (int i = 0; i < 300; i++) net_kills_tick(g_kfeet, T + 16 * (uint64_t)i);  // (5 s of frames alone: no more lines, nothing out)
    NetKillsMsg m[8];
    CHECK(kills_count(g_log, "[kills]") == 1 && kills_rx(&rx, m, 4) == 0 && NETK.st.ask_tx == 0 && NETK.moons_known && NETK.moons == 1);
    CHECK(net_live_set("killsync", 1) && g_net_kills && kills_count(g_log, "[kills]") == 1);  // (a no-op now: no line)
    queue_hit(0xA5, 40, 0, 1);
    net_edmg_pump(T + 5000);
    CHECK(NETK.L.n == 1 && NETK.L.hid[0] == 0xA5 && NETK.st.own == 1);
    kills_slot(0, lo, 47986, 77, "Zed", T + 5000);
    net_kills_tick(g_kfeet, T + 5010);
    CHECK(kills_rx(&rx, m, 4) == 1 && m[0].op == NKO_ASK && NETK.st.ask_tx == 1);
    uint8_t pk[NET_KILLS_PKT];
    kills_from(&rx, lo, pk, net_pack_kills_ask(77, 3, 0x2525, 0, 0, NET_KILLS_MOON_NONE, pk, sizeof pk), T + 5020);
    net_kills_tick(g_kfeet, T + 5021);
    int c = kills_rx(&rx, m, 4), parts = 0;
    for (int i = 0; i < c; i++) parts += m[i].op == NKO_PART && m[i].nonce == 0x2525 && m[i].k == 1 && m[i].hid[0] == 0xA5;
    CHECK(parts == 1 && NETK.st.part_tx == 1);
    // net off -> on: one more line (not per frame), the list kept
    net_set_on(false);
    net_set_on(true);
    CHECK(kills_count(g_log, on_line) == 2 && NETK.L.n == 1);
    net_diag_log();
    CHECK(strstr(g_log, "[kills] (on): list 1"));
    net_set_on(false);
    nsock_close(&rx);
}

static void test_kills_glue(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47983;
    NetSock rx;
    CHECK(nsock_open(&rx, 47982, false));
    net_set_on(true);
    NET.me = 5;
    NETK.loaded = fake_loaded, NETK.moons_fn = fake_moons, g_moons_v = 2, g_kl_n = 0, g_kl_calls = 0;
    CHECK(g_net_kills && strstr(g_log, "[kills] kill sync on (the compiled default since s197"));  // (s197: on with no live key)
    uint64_t T = 4000000;
    NetKillsMsg m[64];
    uint8_t pk[NET_KILLS_PKT];
    // alone (no slot, no pin): an own kill is still recorded (and dropped from sending as ever)
    queue_hit(0xD1, 40, 0, 1), queue_hit(0xD2, 40, 10, 0), queue_hit(0, 40, 0, 1);
    net_edmg_pump(T);
    CHECK(NETK.L.n == 1 && NETK.L.hid[0] == 0xD1 && NETK.L.src[0] == KSRC_OWN && NETK.L.nwait == 0 && NETED.st.tx_drop == 3 && NETK.st.own == 1);
    queue_hit(0xD9, 40, 0, 1), snprintf(g_fq[g_nfq - 1].name, sizeof g_fq[0].name, "Animal_Boar_A");  // (an animal: not kept)
    net_edmg_pump(T + 1);
    CHECK(NETK.L.n == 1 && ks_find(&NETK.L, 0xD9) < 0 && NETED.st.tx_drop == 4);
    // a peer slot appears: asked at once (to his address only), the next ask only KS_REASK_MS later
    kills_slot(0, lo, 47982, 77, "Zed", T);
    net_kills_tick(g_kfeet, T + 10);
    CHECK(kills_rx(&rx, m, 4) == 1 && m[0].op == NKO_ASK && m[0].tag == 0 && m[0].have == 0 && m[0].nonce == NETK.ask[0].nonce && m[0].nonce);
    uint32_t nonce = m[0].nonce;
    net_kills_tick(g_kfeet, T + 10 + KS_REASK_MS - 1);
    CHECK(kills_rx(&rx, m, 4) == 0);
    net_kills_tick(g_kfeet, T + 10 + KS_REASK_MS);
    CHECK(kills_rx(&rx, m, 4) == 1 && m[0].nonce == nonce && NETK.st.ask_tx == 2);
    CHECK(m[0].moon == NET_KILLS_MOON_NONE);  // (s197: no Blood Moon seen in this session = "send everything")
    // his answer: 111 kills in 2 parts, part 1 first (reordered), part 1 again (a dup); then part 0: done
    uint32_t h[NET_KILLS_CHUNK];
    uint16_t ag[NET_KILLS_CHUNK];
    for (int i = 0; i < NET_KILLS_CHUNK; i++) h[i] = 0xB001u + (uint32_t)i, ag[i] = 30;
    uint32_t h2[5] = {0xB0A1, 0xB0A2, 0xB0A3, 0xB0A4, 0xD1};  // (0xD1: ours too)
    kills_from(&rx, lo, pk, net_pack_kills_part(77, 5, nonce, 0x0707, 1, 2, h2, ag, 5, pk, sizeof pk), T + 2000);
    kills_from(&rx, lo, pk, net_pack_kills_part(77, 6, nonce, 0x0707, 1, 2, h2, ag, 5, pk, sizeof pk), T + 2001);
    CHECK(NETK.L.n == 5 && NETK.st.part_dup == 1 && !NETK.ask[0].done);
    kills_from(&rx, lo, pk, net_pack_kills_part(77, 7, nonce, 0x0707, 0, 2, h, ag, NET_KILLS_CHUNK, pk, sizeof pk), T + 2002);
    CHECK(NETK.ask[0].done && NETK.L.n == 111 && NETK.L.nwait == 110 && NETK.st.ask_done == 1 && NETK.st.hid_rx == 111 && NETK.st.hid_new == 110);
    CHECK(strstr(g_log, "catch-up from peer 1 \"Zed\": 111 kill(s) in 2 part(s), 110 new to us, 0 from before our last Blood Moon"));
    int ib = ks_find(&NETK.L, 0xB001);
    CHECK(ib >= 0 && NETK.L.kt[ib] == (int64_t)(T + 2002) - 30999 && NETK.L.kt[ks_find(&NETK.L, 0xD1)] == (int64_t)T);  // (his age; ours stays newest)
    net_kills_tick(g_kfeet, T + 2500);
    CHECK(kills_rx(&rx, m, 4) == 0 && NETK.st.ask_tx == 2);  // (done: no more asks, also when the next one would have been due)
    // the apply scan: two of them loaded and alive here (+ one not on the list): kill applied, dealt 0; not while not driven
    g_kl_n = 3, g_kl_hids[0] = 0xB001, g_kl_hids[1] = 0xC000, g_kl_hids[2] = 0xB0A4;
    g_nap = 0, g_ap_ret = 1, g_kl_calls = 0;
    net_kills_tick(NULL, T + 3000);
    CHECK(g_kl_calls == 0 && g_nap == 0);
    net_kills_tick(g_kfeet, T + 3001);
    CHECK(g_kl_calls == 1 && g_nap == 2 && g_ap[0].hid == 0xB001 && g_ap[0].dealt == 0 && g_ap[0].kill == 1 && g_ap[1].hid == 0xB0A4 && g_ap[1].kill == 1);
    CHECK(NETK.st.applied == 2 && NETK.st.ap_queued == 2 && NETK.L.nwait == 108 && strstr(g_log, "hid=0000b001 (catch-up) is loaded here and alive: kill applied -> queued"));
    net_kills_tick(g_kfeet, T + 3001 + KS_SCAN_MS - 1);
    CHECK(g_kl_calls == 1);
    net_kills_tick(g_kfeet, T + 3001 + KS_SCAN_MS);
    CHECK(g_kl_calls == 2 && g_nap == 2);  // (scanned, but inside KS_REAPPLY_MS)
    CHECK(!strstr(g_log, "(again"));
    CHECK(net_live_set("killslog", 1) && g_net_kills_log);  // (a repeat apply is logged only with killslog 1)
    net_kills_tick(g_kfeet, T + 3001 + KS_REAPPLY_MS);
    CHECK(g_nap == 4 && NETK.st.applied == 4 && strstr(g_log, "(again: it was still alive)"));
    net_live_set("killslog", 0);
    // an EDMG kill from Zed: applied by EDMG as ever; pending there = waiting here, queued = done here; a hit (no kill) is not a kill
    NetEdEnt ed[3] = {{0xE1, 50, 0, 15, 1, NEDF_KILL}, {0xE2, 5, 20, 15, 1, 0}, {0xE3, 50, 0, 15, 1, NEDF_KILL}};
    g_ap_ret = 2;
    kills_from(&rx, lo, pk, net_pack_edmg(77, 20, ed, 1, pk, sizeof pk), T + 9000);
    g_ap_ret = 1;
    kills_from(&rx, lo, pk, net_pack_edmg(77, 21, ed + 1, 2, pk, sizeof pk), T + 9001);
    int ie1 = ks_find(&NETK.L, 0xE1), ie3 = ks_find(&NETK.L, 0xE3);
    CHECK(ie1 >= 0 && ie3 >= 0 && ks_find(&NETK.L, 0xE2) < 0 && NETK.L.applied[ie1] == 0 && NETK.L.applied[ie3] && NETK.st.edmg == 2 && NETK.L.n == 113);
    CHECK(NETK.L.kt[ie1] == (int64_t)(T + 9000) && NETK.L.kt[ie3] == (int64_t)(T + 9001));  // (a live EDMG kill: killed now)
    // his ask: our 113 kills in 2 parts, in our list's order, with their ages; his re-ask with part 0 in: only part 1 again
    kills_from(&rx, lo, pk, net_pack_kills_ask(77, 30, 0x5555, 0, 0, NET_KILLS_MOON_NONE, pk, sizeof pk), T + 9100);
    CHECK(NETK.st.ask_rx == 1 && NETK.st.ans_new == 1 && strstr(g_log, "peer 1 \"Zed\" asks for our kill list (he saw no Blood Moon): 113 of our 113 kill(s) are after it, in 2 part(s)"));
    net_kills_tick(g_kfeet, T + 9101);
    int c = kills_rx(&rx, m, 8);
    CHECK(c == 2 && m[0].op == NKO_PART && m[0].nonce == 0x5555 && m[0].i == 0 && m[0].n == 2 && m[0].k == NET_KILLS_CHUNK && m[1].i == 1 && m[1].k == 7);
    CHECK(m[0].hid[0] == 0xD1 && m[0].hid[1] == 0xB0A1 && m[1].hid[6] == 0xE3 && m[0].tag && m[0].tag == m[1].tag && NETK.st.part_tx == 2);
    CHECK(m[0].age[0] == 9 && m[0].age[1] == 38 && m[1].age[6] == 0);  // (D1: ours, 9.1 s; B0A1: his 30 s + 7.1 s + the rounding; E3: just now)
    uint16_t tag = m[0].tag;
    kills_from(&rx, lo, pk, net_pack_kills_ask(77, 31, 0x5555, tag, 1, NET_KILLS_MOON_NONE, pk, sizeof pk), T + 9200);
    net_kills_tick(g_kfeet, T + 9201);
    CHECK(kills_rx(&rx, m, 8) == 1 && m[0].i == 1 && m[0].tag == tag && NETK.st.ans_new == 1 && NETK.st.part_tx == 3);
    // our save is loaded: the list goes, every peer is asked again with a NEW nonce; a part of the old ask is stale
    net_kills_on_load(2);
    CHECK(NETK.L.n == 0 && NETK.st.clr_load == 1 && strstr(g_log, "[kills] save loaded (slot 2): forgot 113 kill(s)"));
    net_kills_tick(g_kfeet, T + 9300);
    CHECK(kills_rx(&rx, m, 4) == 1 && m[0].op == NKO_ASK && m[0].nonce != nonce && m[0].have == 0 && strstr(g_log, "(our save was loaded, nonce"));
    CHECK(m[0].moon == NET_KILLS_MOON_NONE && g_reset_calls == 0);  // (a load is not a Blood Moon; the exe reset at a load is sora_levels.h's)
    uint32_t nonce2 = m[0].nonce;
    kills_from(&rx, lo, pk, net_pack_kills_part(77, 40, nonce, 0x0707, 0, 2, h, ag, NET_KILLS_CHUNK, pk, sizeof pk), T + 9301);
    CHECK(NETK.L.n == 0 && NETK.st.part_stale == 1);
    kills_from(&rx, lo, pk, net_pack_kills_part(77, 41, nonce2, 0x0708, 0, 1, h2, ag, 5, pk, sizeof pk), T + 9302);
    CHECK(NETK.L.n == 5 && NETK.ask[0].done && NETK.st.ask_done == 2);
    // a Blood Moon in this game: the count went up = the list goes (the first count was adopted: no clear for it); (s197 S1) the
    // exe's pending map goes with it; (M1) "my last moon" = this tick
    CHECK(NETK.moons_known && NETK.moons == 2 && NETK.st.clr_moon == 0 && !NETK.moon_seen);
    g_moons_v = 3;
    net_kills_tick(g_kfeet, T + 9302 + KS_MOON_MS);
    CHECK(NETK.L.n == 0 && NETK.st.clr_moon == 1 && strstr(g_log, "Blood Moon in this game (count 2 -> 3): forgot 5 kill(s)"));
    CHECK(NETK.moon_seen && NETK.moon_t == T + 9302 + KS_MOON_MS && g_reset_calls == 1 && NETK.st.exe_reset == 1);
    CHECK(strstr(g_log, "[kills] Blood Moon in this game (count 2 -> 3): the exe's waiting remote hits were dropped too (3, 2 of them kills"));
    g_moons_v = -1;  // (no world: nothing)
    net_kills_tick(g_kfeet, T + 9302 + 2 * KS_MOON_MS);
    CHECK(NETK.moons == 3 && NETK.st.clr_moon == 1 && g_reset_calls == 1);
    // killsclear (one-shot): the list and the catch-up in flight go; (s197 S1) the exe reset too. (His ask comes 2.3 s after
    // the last snapshot for him: s197 S3 lets a new one be taken.)
    uint64_t Tk = T + 9100 + KS_ANS_NEW_MS + 200;
    kills_from(&rx, lo, pk, net_pack_kills_ask(77, 50, 0x6666, 0, 0, NET_KILLS_MOON_NONE, pk, sizeof pk), Tk);
    CHECK(NETK.st.ans_new == 2 && NETK.ans[0].active);
    queue_hit(0xD7, 40, 0, 1);
    net_edmg_pump(Tk + 1);
    CHECK(NETK.L.n == 1);
    CHECK(net_live_set("killsclear", 1) && NETK.L.n == 0 && NETK.st.clr_key == 1 && strstr(g_log, "set killsclear: forgot 1 kill(s)") && g_reset_calls == 2);
    CHECK(net_live_set("killsclear", 0) && NETK.st.clr_key == 1 && g_reset_calls == 2);
    net_kills_tick(g_kfeet, Tk + 2);
    CHECK(kills_rx(&rx, m, 4) == 0);  // (the answer in flight was dropped with the list: he asks again)
    // the exe has shared HP off: apply -1 = logged once, idle (no more applies / asks); the diag says so
    queue_hit(0xD8, 40, 0, 1);
    net_edmg_pump(T + 9500);
    int ix = ks_find(&NETK.L, 0xD8);
    CHECK(ix >= 0);
    NETK.L.applied[ix] = 0, NETK.L.nwait++;  // (as if it were a catch-up)
    g_kl_n = 1, g_kl_hids[0] = 0xD8, g_ap_ret = -1, g_nap = 0;
    net_kills_tick(g_kfeet, T + 20000);
    net_kills_tick(g_kfeet, T + 30000);
    CHECK(NETK.idle && g_nap == 1 && kills_count(g_log, "the exe has shared HP off") == 1);
    g_log[0] = 0;
    net_diag_log();
    CHECK(strstr(g_log, "[kills] (on, IDLE: exe shared HP off): list 1 (own 1, EDMG 0, catch-up 0; full refused 0, crowded 0), waiting 1"));
    // loopback: no asks / answers
    g_ap_ret = 1, NETK.idle = false;
    g_net_loop = true;
    memset(NETK.joined, 0, sizeof NETK.joined);
    net_kills_tick(g_kfeet, T + 40000);
    CHECK(kills_rx(&rx, m, 4) == 0);
    g_net_loop = false;
    // killsync 0: the list goes, his ask is not answered
    CHECK(net_live_set("killsync", 0) && !g_net_kills && NETK.L.n == 0 && strstr(g_log, "kill sync off"));
    kills_from(&rx, lo, pk, net_pack_kills_ask(77, 60, 0x7777, 0, 0, NET_KILLS_MOON_NONE, pk, sizeof pk), T + 41000);
    net_kills_tick(g_kfeet, T + 41001);
    CHECK(kills_rx(&rx, m, 4) == 0 && NETK.st.rx_off == 1);
    net_set_on(false);
    nsock_close(&rx);
    g_ap_ret = 1;
}

// (point 7) a DLL hot reload wipes every static: the new copy turns net on (killsync on by default since s197, no key), Zed's next HELLO makes
// his slot again = a new slot = we ask him, and his answer brings the list back
static void test_kills_hot_reload(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47985;
    NetSock rx;
    CHECK(nsock_open(&rx, 47984, false));
    net_set_on(true);  // (the new copy: NET and NETK all zero, g_net_kills back at its compiled default ON: s197, no live key)
    NET.me = 5;
    CHECK(g_net_kills && kills_count(g_log, "[kills] kill sync on (the compiled default since s197") == 1);
    NETK.loaded = fake_loaded, NETK.moons_fn = fake_moons, g_moons_v = 4;
    uint64_t T = 5000000;
    net_kills_tick(g_kfeet, T);
    NetKillsMsg m[8];
    CHECK(kills_rx(&rx, m, 4) == 0 && netslot_count(NET.slot, NET_MAX_PEERS) == 0);
    uint8_t pk[NET_KILLS_PKT];
    kills_from(&rx, lo, pk, net_pack_hello(77, 900, "Zed", pk, sizeof pk), T + 10);  // (his HELLO: his slot is made)
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1 && NET.slot[0].port == 47984);
    kills_from(&rx, lo, pk, net_pack_kills_ask(77, 902, 0x3131, 0, 0, NET_KILLS_MOON_NONE, pk, sizeof pk), T + 11);  // (his ask right behind it, before our tick)
    net_kills_tick(g_kfeet, T + 20);
    int c = kills_rx(&rx, m, 4), ia = -1, ip = -1;
    for (int i = 0; i < c; i++) { if (m[i].op == NKO_ASK) ia = i; else ip = i; }
    CHECK(c == 2 && ia >= 0 && ip >= 0 && strstr(g_log, "asking peer 1 \"Zed\""));  // (we ask him, and his ask is answered: not wiped by the join)
    CHECK(ip >= 0 && m[ip].nonce == 0x3131 && m[ip].n == 1 && m[ip].k == 0);
    // (s197 M1) the reload wiped "my last Blood Moon": the ask says none seen = he sends everything, also kills of not-known age
    CHECK(ia >= 0 && m[ia].moon == NET_KILLS_MOON_NONE && !NETK.moon_seen);
    uint32_t h[3] = {0xF1, 0xF2, 0xF3};
    kills_from(&rx, lo, pk, net_pack_kills_part(77, 901, ia >= 0 ? m[ia].nonce : 0, 0x0101, 0, 1, h, NULL, 3, pk, sizeof pk), T + 30);
    CHECK(NETK.L.n == 3 && NETK.ask[0].done && NETK.moons == 4 && NETK.st.clr_moon == 0);  // (the env-kept count is adopted, no clear)
    CHECK(NETK.L.kt[0] == KS_KT_OLD && NETK.st.hid_old == 0);
    net_set_on(false);
    nsock_close(&rx);
}

// (s197 review M1 + S1) the Blood Moon in THIS game sets "my last moon"; my asks carry its age; a catch-up drops kills from before
// it (also ones a slow / buggy answerer still sends), my answer to a peer holds back kills older than HIS moon; the exe reset runs
// at the moon (and at killsclear); an exe without the export is logged once; a moon while killsync was off is seen when it is on again
static void test_kills_moon_glue(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47991;
    NetSock rx;
    CHECK(nsock_open(&rx, 47990, false));
    net_set_on(true);
    NET.me = 5;
    NETK.loaded = fake_loaded, NETK.moons_fn = fake_moons, g_moons_v = 5, g_kl_n = 0, g_kl_calls = 0;
    uint64_t T = 7000000;
    NetKillsMsg m[8];
    uint8_t pk[NET_KILLS_PKT];
    net_kills_tick(g_kfeet, T);  // (count 5 adopted)
    queue_hit(0xA1, 40, 0, 1), queue_hit(0xA2, 40, 0, 1);
    net_edmg_pump(T + 100);
    CHECK(NETK.L.n == 2 && !NETK.moon_seen);
    // the Blood Moon: list gone, moon time = this tick, the exe reset
    g_moons_v = 6;
    net_kills_tick(g_kfeet, T + 1000);
    CHECK(NETK.L.n == 0 && NETK.moon_seen && NETK.moon_t == T + 1000 && g_reset_calls == 1 && NETK.st.clr_moon == 1);
    queue_hit(0xA3, 40, 0, 1);
    net_edmg_pump(T + 5000);  // (after it: kept)
    // Zed appears 59 s after our moon: our ask says so
    kills_slot(0, lo, 47990, 77, "Zed", T + 60000);
    net_kills_tick(g_kfeet, T + 60010);
    CHECK(kills_rx(&rx, m, 4) == 1 && m[0].op == NKO_ASK && m[0].moon == 59);
    uint32_t nonce = m[0].nonce;
    // his answer: 70 s old (before our moon), 30 s, not known, 58 s (inside the slack after our moon), 56 s (just after it)
    uint32_t h[5] = {0xC1, 0xC2, 0xC3, 0xC4, 0xC5};
    uint16_t ag[5] = {70, 30, NET_KILLS_AGE_OLD, 58, 56};
    kills_from(&rx, lo, pk, net_pack_kills_part(77, 9, nonce, 0x0303, 0, 1, h, ag, 5, pk, sizeof pk), T + 60020);
    CHECK(NETK.ask[0].done && NETK.L.n == 3 && ks_find(&NETK.L, 0xA3) >= 0 && ks_find(&NETK.L, 0xC2) >= 0 && ks_find(&NETK.L, 0xC5) >= 0);
    CHECK(ks_find(&NETK.L, 0xC1) < 0 && ks_find(&NETK.L, 0xC3) < 0 && ks_find(&NETK.L, 0xC4) < 0 && NETK.st.hid_old == 3);
    CHECK(strstr(g_log, "catch-up from peer 1 \"Zed\": 5 kill(s) in 1 part(s), 2 new to us, 3 from before our last Blood Moon (dropped)"));
    // his ask: his last moon 40 s ago: of our 3 only 0xC2 (killed ~32 s ago) goes, with its age
    kills_from(&rx, lo, pk, net_pack_kills_ask(77, 10, 0x4444, 0, 0, 40, pk, sizeof pk), T + 61000);
    CHECK(strstr(g_log, "peer 1 \"Zed\" asks for our kill list (his last Blood Moon 40 s ago): 1 of our 3 kill(s) are after it, in 1 part(s)"));
    net_kills_tick(g_kfeet, T + 61001);
    int c = kills_rx(&rx, m, 4), got = -1;
    for (int i = 0; i < c; i++) if (m[i].op == NKO_PART) got = i;
    CHECK(got >= 0 && m[got].nonce == 0x4444 && m[got].k == 1 && m[got].hid[0] == 0xC2 && m[got].age[0] == 31);
    // a peer with no moon gets all 3
    kills_from(&rx, lo, pk, net_pack_kills_ask(77, 11, 0x4445, 0, 0, NET_KILLS_MOON_NONE, pk, sizeof pk), T + 61000 + KS_ANS_NEW_MS);
    net_kills_tick(g_kfeet, T + 61001 + KS_ANS_NEW_MS);
    c = kills_rx(&rx, m, 4), got = -1;
    for (int i = 0; i < c; i++) if (m[i].op == NKO_PART) got = i;
    CHECK(got >= 0 && m[got].nonce == 0x4445 && m[got].k == 3);
    // moon then reload: the load's ask still carries the moon's age (a load is not a moon)
    net_kills_on_load(1);
    net_kills_tick(g_kfeet, T + 70000);
    CHECK(kills_rx(&rx, m, 4) == 1 && m[0].op == NKO_ASK && m[0].moon == 69 && m[0].nonce != nonce);
    // killsclear: the exe reset too; an exe without linkpos_edmg_reset: said once, nothing else breaks
    CHECK(net_live_set("killsclear", 1) && g_reset_calls == 2 && NETK.st.exe_reset == 2);
    NETED.reset = NULL;
    g_log[0] = 0;
    g_moons_v = 7;
    net_kills_tick(g_kfeet, T + 80000);
    CHECK(NETK.st.clr_moon == 2 && NETK.moon_t == T + 80000 && kills_count(g_log, "has no linkpos_edmg_reset") == 1 && g_reset_calls == 2);
    CHECK(net_live_set("killsclear", 1) && kills_count(g_log, "has no linkpos_edmg_reset") == 1);
    // a Blood Moon while killsync is off: seen at the first count read after `set killsync 1` (late = the safe side)
    NETED.reset = fake_reset;
    CHECK(net_live_set("killsync", 0) && !g_net_kills);
    g_moons_v = 8;
    net_kills_tick(g_kfeet, T + 90000);  // (off: nothing read)
    CHECK(NETK.moons == 7 && NETK.moon_t == T + 80000);
    CHECK(net_live_set("killsync", 1) && g_net_kills);
    net_kills_tick(g_kfeet, T + 95000);
    CHECK(NETK.moons == 8 && NETK.st.clr_moon == 3 && NETK.moon_t == T + 95000 && g_reset_calls == 3);
    g_log[0] = 0;
    net_diag_log();
    CHECK(strstr(g_log, "last Blood Moon 0 s ago") && strstr(g_log, "before our Blood Moon 3") && strstr(g_log, "(exe reset 3)"));
    net_set_on(false);
    nsock_close(&rx);
}

// (s197 review S2) kills are recorded and applied only in the open world (sora_world_open() == 1; 0 / -1 / no hook = not), never a
// Stal (`_Bone`), never an EDMG kill from a peer on another map; a catch-up is still taken in a shrine (it is about the open world)
static void test_kills_open_world(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47993;
    NetSock rx;
    CHECK(nsock_open(&rx, 47992, false));
    net_set_on(true);
    NET.me = 5;
    NETK.loaded = fake_loaded, NETK.moons_fn = fake_moons, g_moons_v = 1, g_kl_n = 0, g_kl_calls = 0;
    uint64_t T = 8000000;
    uint8_t pk[NET_KILLS_PKT];
    kills_slot(0, lo, 47992, 77, "Zed", T);
    net_kills_tick(g_kfeet, T);
    NetKillsMsg m[4];
    CHECK(kills_rx(&rx, m, 4) == 1 && m[0].op == NKO_ASK);
    // in a shrine (0), no world (-1), no hook: own kills and Zed's EDMG kills are not kept
    int vs[3] = {0, -1, 1};
    for (int v = 0; v < 3; v++) {
        g_open_v = vs[v];
        if (v == 2) NETK.open_fn = NULL;
        queue_hit(0xA0 + (uint32_t)v, 40, 0, 1);
        net_edmg_pump(T + 10 + (uint64_t)v);
        NetEdEnt ed = {0xB0 + (uint32_t)v, 50, 0, 15, 1, NEDF_KILL};
        kills_from(&rx, lo, pk, net_pack_edmg(77, (uint16_t)(30 + v), &ed, 1, pk, sizeof pk), T + 20 + (uint64_t)v);
    }
    CHECK(NETK.L.n == 0 && NETK.st.not_open == 6 && g_nap == 3);  // (the EDMG applies themselves as ever)
    NETK.open_fn = fake_open, g_open_v = 1;
    // a Stal: not kept (the name rule); any other Enemy_*: kept
    queue_hit(0xA5, 40, 0, 1), snprintf(g_fq[g_nfq - 1].name, sizeof g_fq[0].name, "Enemy_Bokoblin_Bone_Junior");
    queue_hit(0xA6, 40, 0, 1), snprintf(g_fq[g_nfq - 1].name, sizeof g_fq[0].name, "Enemy_Lizalfos_Junior");
    net_edmg_pump(T + 100);
    CHECK(NETK.L.n == 1 && ks_find(&NETK.L, 0xA6) == 0 && ks_find(&NETK.L, 0xA5) < 0);
    // Zed on another map (AREA hidden): his EDMG kill is not kept; on ours: kept
    NET.slot[0].hidden = true;
    NetEdEnt ed2 = {0xB7, 50, 0, 15, 1, NEDF_KILL};
    kills_from(&rx, lo, pk, net_pack_edmg(77, 40, &ed2, 1, pk, sizeof pk), T + 200);
    CHECK(ks_find(&NETK.L, 0xB7) < 0 && NETK.st.not_open == 7);
    NET.slot[0].hidden = false;
    ed2.hid = 0xB8;
    kills_from(&rx, lo, pk, net_pack_edmg(77, 41, &ed2, 1, pk, sizeof pk), T + 201);
    CHECK(ks_find(&NETK.L, 0xB8) >= 0 && NETK.L.n == 2);
    // a catch-up while in a shrine: taken (the list is about the open world); the apply scan waits for the open world
    g_open_v = 0;
    uint32_t h[2] = {0xC1, 0xC2};
    uint16_t ag[2] = {3, 4};
    kills_from(&rx, lo, pk, net_pack_kills_part(77, 50, m[0].nonce, 0x0909, 0, 1, h, ag, 2, pk, sizeof pk), T + 300);
    CHECK(NETK.ask[0].done && NETK.L.n == 4);
    g_kl_n = 2, g_kl_hids[0] = 0xC1, g_kl_hids[1] = 0xA6, g_nap = 0, g_kl_calls = 0;
    for (int i = 0; i < 5; i++) net_kills_tick(g_kfeet, T + 400 + 200 * (uint64_t)i);
    CHECK(g_kl_calls == 0 && g_nap == 0);
    g_open_v = -1;
    net_kills_tick(g_kfeet, T + 2000);
    CHECK(g_kl_calls == 0 && g_nap == 0);
    g_open_v = 1;
    net_kills_tick(g_kfeet, T + 2200);
    CHECK(g_kl_calls == 1 && g_nap == 1 && g_ap[0].hid == 0xC1);  // (0xA6: ours, seen dead here = inside KS_REAPPLY_MS)
    net_set_on(false);
    nsock_close(&rx);
}

// (s197 review S3 / S4) a flood of asks from one peer: one new snapshot per KS_ANS_NEW_MS, one line; asks from many peers: the line
// is rate limited; a crafted answer: one snapshot restart followed, the second ends the ask (logged once); one answer adds at most
// KS_ASK_NEW_MAX new kills (logged once; list full = new kills refused, the enemies stay alive)
static void test_kills_flood_caps(void) {
    reset_all();
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    NET.port = 47995;
    NetSock rx;
    CHECK(nsock_open(&rx, 47994, false));
    net_set_on(true);
    NET.me = 5;
    uint64_t T = 9000000;
    uint8_t pk[NET_KILLS_PKT];
    queue_hit(0xA1, 40, 0, 1);
    net_edmg_pump(T);
    kills_slot(0, lo, 47994, 77, "Zed", T);
    net_kills_tick(g_kfeet, T);
    NetKillsMsg m[64];
    CHECK(kills_rx(&rx, m, 4) == 1 && m[0].op == NKO_ASK);
    uint32_t ours = m[0].nonce;
    g_log[0] = 0;
    // S3: 30 asks with new nonces in 1 s: one snapshot, one line, the rest counted "too soon"
    for (int i = 0; i < 30; i++) {
        int n = net_pack_kills_ask(77, (uint16_t)(100 + i), 0x7000u + (uint32_t)i, 0, 0, NET_KILLS_MOON_NONE, pk, sizeof pk);
        net_kills_recv(0, pk, n, T + 10 + 33 * (uint64_t)i);
    }
    CHECK(NETK.st.ans_new == 1 && NETK.st.ans_later == 29 && kills_count(g_log, "asks for our kill list") == 1 && NETK.ans[0].nonce == 0x7000);
    // its re-ask (the same nonce, part 0 in) is served at once; a new nonce after KS_ANS_NEW_MS is a new snapshot
    int n = net_pack_kills_ask(77, 200, 0x7000, NETK.ans[0].tag, 0, NET_KILLS_MOON_NONE, pk, sizeof pk);
    net_kills_recv(0, pk, n, T + 1100);
    CHECK(NETK.st.ans_later == 29 && NETK.ans[0].active);
    n = net_pack_kills_ask(77, 201, 0x7100, 0, 0, NET_KILLS_MOON_NONE, pk, sizeof pk);
    net_kills_recv(0, pk, n, T + 10 + KS_ANS_NEW_MS);
    CHECK(NETK.st.ans_new == 2 && NETK.ans[0].nonce == 0x7100);
    // the line itself (belt and braces: with NET_MAX_PEERS 3 the snapshot limit alone keeps it to 3 lines per 2 s): 12 new snapshots
    // in the same 2 s (the snapshot timer wound back by hand) = 8 lines + one "not logged" line
    g_log[0] = 0;
    for (int k = 0; k < 12; k++) {
        NETK.ans[0].t_new = 0;
        n = net_pack_kills_ask(77, (uint16_t)(210 + k), 0x8000u + (uint32_t)k, 0, 0, NET_KILLS_MOON_NONE, pk, sizeof pk);
        net_kills_recv(0, pk, n, T + 4100 + (uint64_t)k);  // (a fresh log window: the last line was at T + 2010)
    }
    CHECK(kills_count(g_log, "asks for our kill list") == NET_LOGRATE_BURST && kills_count(g_log, "more kill-list asks in the next") == 1);
    CHECK(NETK.st.ans_new == 14 && NETK.ask_log.quiet == 12 - NET_LOGRATE_BURST);
    net_kills_tick(g_kfeet, T + 4200);
    while (kills_rx(&rx, m, 64) > 0) {}
    // S4: our ask to Zed (nonce `ours`): tag 1 parts, then tag 2 (one restart: followed), then tag 3 = capped, logged once
    g_log[0] = 0;
    uint32_t h[NET_KILLS_CHUNK];
    uint16_t ag[NET_KILLS_CHUNK];
    for (int i = 0; i < NET_KILLS_CHUNK; i++) h[i] = 0x01000000u + (uint32_t)i, ag[i] = 1;
    n = net_pack_kills_part(77, 300, ours, 1, 0, 3, h, ag, 2, pk, sizeof pk);
    net_kills_recv(0, pk, n, T + 4000);
    n = net_pack_kills_part(77, 301, ours, 2, 0, 3, h + 2, ag, 2, pk, sizeof pk);
    net_kills_recv(0, pk, n, T + 4001);
    CHECK(NETK.ask[0].restarts == 1 && NETK.ask[0].active && NETK.L.n == 5);
    for (int t = 3; t < 10; t++) {
        n = net_pack_kills_part(77, (uint16_t)(300 + t), ours, (uint16_t)t, 0, 3, h + 10, ag, 5, pk, sizeof pk);
        net_kills_recv(0, pk, n, T + 4000 + (uint64_t)t);
    }
    CHECK(!NETK.ask[0].active && NETK.ask[0].capped && NETK.st.part_cap == 1 && NETK.st.part_stale == 6 && NETK.L.n == 5);
    CHECK(kills_count(g_log, "his answer changed / grew past what one answer can be") == 1);
    // S4: one answer with more than KS_ASK_NEW_MAX new kills: the rest refused, one line; the list keeps working
    net_kills_on_load(3);
    net_kills_tick(g_kfeet, T + 5000);
    CHECK(kills_rx(&rx, m, 4) == 1 && m[0].op == NKO_ASK);
    uint32_t nn = m[0].nonce;
    g_log[0] = 0;
    for (int p = 0; p < 64; p++) {
        for (int i = 0; i < NET_KILLS_CHUNK; i++) h[i] = 0x02000000u + (uint32_t)(p * NET_KILLS_CHUNK + i);
        n = net_pack_kills_part(77, (uint16_t)(400 + p), nn, 7, p, 64, h, ag, NET_KILLS_CHUNK, pk, sizeof pk);
        net_kills_recv(0, pk, n, T + 5100 + (uint64_t)p);
    }
    CHECK(NETK.ask[0].done && NETK.L.n == KS_ASK_NEW_MAX && NETK.ask[0].refused == (uint32_t)(64 * NET_KILLS_CHUNK - KS_ASK_NEW_MAX));
    CHECK(kills_count(g_log, "new kills in one answer: the rest is not taken") == 1);
    queue_hit(0xA9, 40, 0, 1);
    net_edmg_pump(T + 6000);
    CHECK(NETK.L.n == KS_ASK_NEW_MAX + 1);  // (own kills still kept)
    net_set_on(false);
    nsock_close(&rx);
}

// (s197 review S5) the NET_REACT receive lines a peer can drive are rate limited (8 per 2 s each, then one "not logged" line),
// counted in `net diag`; a fresh window logs again
static void test_react_log_rate(void) {
    reset_all();
    g_net_react = true;
    uint32_t lo;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    kills_slot(0, lo, 47996, 77, "Zed", 1000);
    float f[3] = {1, 2, 3};
    NetReactEnt e[NET_REACT_MAX];
    for (int i = 0; i < NET_REACT_MAX; i++) e[i] = nr_ent_make(0x100 + (uint32_t)i, 3, 125, 0, 7, "blade", f, false);
    uint8_t pk[NET_PKT_MAX];
    int n = net_pack_react(77, 9, e, 1, pk, sizeof pk);
    net_react_recv(0, pk, n, 2000);
    g_log[0] = 0;
    for (int i = 0; i < 40; i++) net_react_recv(0, pk, n, 2001 + (uint64_t)i);  // (40 repeat copies)
    CHECK(NETR.st.rx_dup == 40 && kills_count(g_log, "-> dup (a repeat copy)") == NET_LOGRATE_BURST && kills_count(g_log, "more repeat copies") == 1);
    // bad packets (every entry bad)
    uint8_t bad[NET_PKT_MAX];
    memcpy(bad, pk, (size_t)n), memset(bad + NET_HDR, 0, 4);
    g_log[0] = 0;
    for (int i = 0; i < 30; i++) net_react_recv(0, bad, n, 2100 + (uint64_t)i);
    CHECK(NETR.st.rx_bad == 30 && kills_count(g_log, "-> bad (") == NET_LOGRATE_BURST && kills_count(g_log, "more bad packets") == 1);
    // he is on another map: 3 packets of 8 entries = 24 dropped entries, 8 lines
    NET.slot[0].hidden = true;
    g_log[0] = 0;
    for (int i = 0; i < 3; i++) {
        int m = net_pack_react(77, (uint16_t)(20 + i), e, NET_REACT_MAX, pk, sizeof pk);
        net_react_recv(0, pk, m, 2200 + (uint64_t)i);
    }
    CHECK(NETR.st.rx_hidden == 24 && kills_count(g_log, "hidden (he is on another map)") == NET_LOGRATE_BURST && kills_count(g_log, "more dropped entries") == 1);
    // a new window: logged again
    g_log[0] = 0;
    net_react_recv(0, bad, n, 2100 + NET_LOGRATE_MS);
    CHECK(kills_count(g_log, "-> bad (") == 1);
    // the played / dropped lines of the frame body: 16 queued, 8 lines (none of them ours: not owner)
    NET.slot[0].hidden = false;
    for (int i = 0; i < 2; i++) {
        int m = net_pack_react(77, (uint16_t)(40 + i), e, NET_REACT_MAX, pk, sizeof pk);
        net_react_recv(0, pk, m, 9000);
    }
    g_log[0] = 0;
    CHECK(net_react_pending() == 16 && net_react_play(9001, NULL, NULL, NULL) == 0);
    CHECK(kills_count(g_log, "-> not owner") == NET_LOGRATE_BURST && kills_count(g_log, "more received reactions") == 1);
    g_log[0] = 0;
    net_react_diag();
    uint32_t q = NETR.log_bad.quiet + NETR.log_dup.quiet + NETR.log_rx.quiet + NETR.log_play.quiet;
    CHECK(q == (40 - 8) + (30 - 8) + (24 - 8) + (16 - 8));
    char want[64];
    snprintf(want, sizeof want, "log lines not written (rate limit) %u", q);
    CHECK(strstr(g_log, want) != NULL);
    g_net_react = false;
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
    test_esync_players_area();
    test_area_filters();
    test_area_send();
    test_react_attacker();
    test_react_owner();
    test_eshit_glue();
    test_eshit_react();
    test_kills_off();
    test_kills_default_on();
    test_kills_glue();
    test_kills_hot_reload();
    test_kills_moon_glue();
    test_kills_open_world();
    test_kills_flood_caps();
    test_react_log_rate();
    printf("net_glue_test: %d/%d passed\n", runs - fails, runs);
    return fails ? 1 : 0;
}

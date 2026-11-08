// host tests for runtime/sora_net_proto.h + sora_net_peer.h (multiplayer slice 1): sh D:/SoraBotW/recon/make_net_test.sh
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "sora_net_proto.h"
#include "sora_net_peer.h"
#include "sora_net_pcap.h"
#pragma clang diagnostic ignored "-Wunused-function"  // (sora_camp_seq.h: the page code the plate does not use)
#include "sora_net_plate.h"
#include "sora_hud_label.h"
#include "sora_net_ribbon.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, eps) (fabs((double)(a) - (double)(b)) <= (eps))

static NetSnap sample_snap(int nev) {
    NetSnap s;
    memset(&s, 0, sizeof s);
    s.player = 42, s.seq = 1234;
    s.body = 3, s.kb_chain = 1, s.kb_alt = 7;
    s.slot = 412, s.clock = 37.4f;
    s.feet[0] = 1234.5f, s.feet[1] = 210.25f, s.feet[2] = -987.0f;
    s.yaw = 2.5f;
    s.flags = NF_WEAPON | NF_AIR;
    s.hp = 80, s.mp = 55;
    s.nev = (uint8_t)nev;
    for (int i = 0; i < nev; i++) {
        NetEvent *e = &s.ev[i];
        e->seq = (uint8_t)(200 + i), e->kind = (uint8_t)(i & 1), e->id = (uint16_t)(300 + i);
        e->pos[0] = 10.5f + i, e->pos[1] = -2.0f, e->pos[2] = 99.25f;
        e->yaw = 1.0f + i, e->scale = 1.5f;
    }
    return s;
}

static void test_snap_roundtrip(void) {
    NetSnap s = sample_snap(2), r;
    uint8_t buf[NET_PKT_MAX];
    int n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(n > 0);
    int type = 0; uint8_t player = 0; uint16_t seq = 0; char name[NET_NAME_MAX + 1];
    memset(&r, 0xcc, sizeof r);
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name));
    CHECK(type == NET_SNAP && player == 42 && seq == 1234);
    CHECK(r.player == 42 && r.seq == 1234);
    CHECK(r.body == 3 && r.kb_chain == 1 && r.kb_alt == 7 && r.slot == 412);
    CHECK(NEAR(r.clock, 37.4, 0.05));
    CHECK(r.feet[0] == 1234.5f && r.feet[1] == 210.25f && r.feet[2] == -987.0f);
    CHECK(NEAR(r.yaw, 2.5, 0.0002));
    CHECK(r.flags == (NF_WEAPON | NF_AIR) && r.hp == 80 && r.mp == 55);
    CHECK(r.nev == 2);
    for (int i = 0; i < 2; i++) {
        CHECK(r.ev[i].seq == 200 + i && r.ev[i].kind == (i & 1) && r.ev[i].id == 300 + i);
        CHECK(r.ev[i].pos[0] == 10.5f + i && r.ev[i].pos[1] == -2.0f && r.ev[i].pos[2] == 99.25f);
        CHECK(NEAR(r.ev[i].yaw, 1.0 + i, 0.0002));
        CHECK(NEAR(r.ev[i].scale, 1.5, 1.5 * 0.025));  // (log scale byte: within 2.5%)
    }
    // negative yaw comes back as the same angle in [0, 2 pi)
    s.yaw = -0.5f;
    n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name) && NEAR(r.yaw, 2 * M_PI - 0.5, 0.0002));
}

// (Task 8 LAN test) Sora's fx spawn at scale to_u = 0.01 * g_k: the old x32 u8 sent 0 = invisible on the remote
static void test_event_scale_range(void) {
    static const float v[] = {0.01f, 0.0125f, 0.02f, 0.05f, 0.25f, 0.6f, 1.0f, 1.5f, 3.0f, 8.0f};
    uint8_t buf[NET_PKT_MAX];
    int type = 0; uint8_t player = 0; uint16_t seq = 0; char name[NET_NAME_MAX + 1];
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        NetSnap s = sample_snap(1), r;
        s.ev[0].scale = v[i];
        int n = net_pack_snap(&s, buf, sizeof buf);
        CHECK(n > 0 && net_unpack(buf, n, &type, &player, &seq, &r, name));
        CHECK(NEAR(r.ev[0].scale / v[i], 1.0, 0.025));  // within 2.5% from fx scale 0.01 up to 8
    }
    NetSnap s = sample_snap(1), r;
    s.ev[0].scale = 0;  // a silent sound / zero scale stays exactly 0
    int n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name) && r.ev[0].scale == 0);
    s.ev[0].scale = 1.0f;  // full gain exact
    n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name) && r.ev[0].scale == 1.0f);
}

// (Task 8 LAN test: Blizzard's shard froze in the remote Sora) moving fx: SNAPs carry the newest place of the fx
// spawned by an event (keyed by its event seq), or its kill / fadeout
static void test_moves_roundtrip(void) {
    NetSnap s = sample_snap(2), r;
    s.nmv = 3;
    s.mv[0] = (NetFxMove){.ev = 200, .flags = 0, .pos = {1.5f, -2.25f, 300.0f}, .yaw = 1.25f};
    s.mv[1] = (NetFxMove){.ev = 17, .flags = NMV_KILL};
    s.mv[2] = (NetFxMove){.ev = 255, .flags = NMV_FADE, .fade = 12.5f};
    uint8_t buf[NET_PKT_MAX];
    int n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(n == NET_HDR + NET_SNAP_BODY + 2 * NET_EV_SIZE + 3 * NET_MV_SIZE);
    int type = 0; uint8_t player = 0; uint16_t seq = 0; char name[NET_NAME_MAX + 1];
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name));
    CHECK(r.nev == 2 && r.nmv == 3);
    CHECK(r.mv[0].ev == 200 && r.mv[0].flags == 0 && r.mv[0].pos[0] == 1.5f && r.mv[0].pos[1] == -2.25f && r.mv[0].pos[2] == 300.0f);
    CHECK(NEAR(r.mv[0].yaw, 1.25, 0.0002));
    CHECK(r.mv[1].ev == 17 && r.mv[1].flags == NMV_KILL);
    CHECK(r.mv[2].ev == 255 && r.mv[2].flags == NMV_FADE && NEAR(r.mv[2].fade, 12.5, 0.05));
    CHECK(r.ev[1].id == 301);  // (events still where they were)
    CHECK(!net_unpack(buf, n - 1, &type, &player, &seq, &r, name));  // a move cut short
    s.nmv = NET_MAX_MOVES + 1;
    CHECK(net_pack_snap(&s, buf, sizeof buf) == -1);
    s = sample_snap(NET_MAX_EVENTS), s.nmv = NET_MAX_MOVES;  // the biggest SNAP fits a packet
    n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(n > 0 && n <= NET_PKT_MAX && net_unpack(buf, n, &type, &player, &seq, &r, name) && r.nmv == NET_MAX_MOVES);
}
static void test_pick_moves(void) {
    NetFxOut t[32];
    memset(t, 0, sizeof t);
    NetFxMove out[NET_MAX_MOVES];
    int rr = 0;
    CHECK(net_pick_moves(t, 32, &rr, out, NET_MAX_MOVES) == 0);  // nothing on
    t[3] = (NetFxOut){.on = true, .ev = 9};                         // spawned, not moved yet: nothing to send
    CHECK(net_pick_moves(t, 32, &rr, out, NET_MAX_MOVES) == 0);
    t[3].dirty = true, t[3].pos[0] = 5, t[3].yaw = 2;
    CHECK(net_pick_moves(t, 32, &rr, out, NET_MAX_MOVES) == 1 && out[0].ev == 9 && out[0].flags == 0 && out[0].pos[0] == 5);
    CHECK(!t[3].dirty && t[3].on && net_pick_moves(t, 32, &rr, out, NET_MAX_MOVES) == 0);  // sent once per move
    t[3].rep = NET_EVENT_REPEAT, t[3].flags = NMV_KILL;  // killed: the kill rides 3 SNAPs, then the slot is off
    for (int i = 0; i < NET_EVENT_REPEAT; i++) CHECK(net_pick_moves(t, 32, &rr, out, NET_MAX_MOVES) == 1 && out[0].flags == NMV_KILL);
    CHECK(!t[3].on && net_pick_moves(t, 32, &rr, out, NET_MAX_MOVES) == 0);
    for (int i = 0; i < 8; i++) t[10 + i] = (NetFxOut){.on = true, .dirty = true, .ev = (uint8_t)(40 + i)};
    CHECK(net_pick_moves(t, 32, &rr, out, NET_MAX_MOVES) == NET_MAX_MOVES && out[0].ev == 40);
    for (int i = 0; i < 8; i++) t[10 + i].dirty = true;  // all moved again: the 2 left out go first (round robin)
    CHECK(net_pick_moves(t, 32, &rr, out, NET_MAX_MOVES) == NET_MAX_MOVES && out[0].ev == 46 && out[1].ev == 47);
}
// (final review I1) a kill / fade still riding its SNAPs survives the exe handing the same slot to a new fx
static void test_fxo_kill_then_reuse(void) {
    NetFxOut t[NET_FXO_N];
    memset(t, 0, sizeof t);
    NetFxMove out[NET_MAX_MOVES];
    int rr = 0;
    t[5] = (NetFxOut){.on = true, .ev = 9};
    t[5].rep = NET_EVENT_REPEAT, t[5].flags = NMV_KILL;  // killed; the slot is free in the exe at once
    net_fxo_reuse(t, 5, 10);                             // same-tick spawn into slot 5, event 10
    CHECK(t[5].on && t[5].ev == 10 && !t[5].rep);
    int kills = 0;
    for (int s = 0; s < NET_EVENT_REPEAT; s++) {
        int n = net_pick_moves(t, NET_FXO_N, &rr, out, NET_MAX_MOVES);
        for (int i = 0; i < n; i++) if (out[i].ev == 9 && out[i].flags == NMV_KILL) kills++;
    }
    CHECK(kills == NET_EVENT_REPEAT);  // the old copy's kill still rides every repeat
    CHECK(net_pick_moves(t, NET_FXO_N, &rr, out, NET_MAX_MOVES) == 0);  // then the spill is free again
    for (int i = NET_FXO_HANDLES; i < NET_FXO_N; i++) CHECK(!t[i].on);
    // a plain reuse (nothing pending) spills nothing; a not-sent spawn (ev -1) leaves the slot off
    net_fxo_reuse(t, 5, -1);
    CHECK(!t[5].on);
    for (int i = NET_FXO_HANDLES; i < NET_FXO_N; i++) CHECK(!t[i].on);
    // spill full: the newest pending kill takes the oldest spill entry (never out of bounds)
    for (int i = 0; i < NET_FXO_N - NET_FXO_HANDLES + 2; i++) {
        t[7] = (NetFxOut){.on = true, .ev = (uint8_t)(100 + i), .rep = NET_EVENT_REPEAT, .flags = NMV_FADE};
        net_fxo_reuse(t, 7, -1);
    }
    int on = 0;
    for (int i = NET_FXO_HANDLES; i < NET_FXO_N; i++) on += t[i].on;
    CHECK(on == NET_FXO_N - NET_FXO_HANDLES);
}
static void test_fxtrack(void) {
    FxTrack t;
    memset(&t, 0, sizeof t);
    float p[3], y;
    CHECK(!fxtrack_sample(&t, 500, p, &y));  // empty
    fxtrack_push(&t, 100, (float[3]){0, 0, 0}, 6.2f);
    fxtrack_push(&t, 150, (float[3]){10, 2, -4}, 0.1f);
    CHECK(!fxtrack_sample(&t, 90, p, &y));  // before the first: the fx stays where it spawned
    CHECK(fxtrack_sample(&t, 125, p, &y) && NEAR(p[0], 5, 1e-4) && NEAR(p[1], 1, 1e-4) && NEAR(p[2], -2, 1e-4));
    CHECK(NEAR(y, peer_wrap_2pi((6.2 + 0.1 + NET_TWO_PI) / 2), 1e-3));  // (yaw: the short way round)
    CHECK(fxtrack_sample(&t, 400, p, &y) && p[0] == 10);  // after the last: hold it
    fxtrack_push(&t, 200, (float[3]){20, 0, 0}, 0);
    fxtrack_push(&t, 250, (float[3]){30, 0, 0}, 0);  // full (3): the oldest goes
    CHECK(t.n == FXTRACK_N && t.at[0] == 150);
    CHECK(fxtrack_sample(&t, 225, p, &y) && NEAR(p[0], 25, 1e-4));
}

static void test_snap_size(void) {
    NetSnap s = sample_snap(0);
    uint8_t buf[NET_PKT_MAX];
    int n0 = net_pack_snap(&s, buf, sizeof buf);
    CHECK(n0 > 0 && n0 <= 40);
    s = sample_snap(4);
    int n4 = net_pack_snap(&s, buf, sizeof buf);
    CHECK(n4 > n0 && n4 <= 120);
    CHECK(net_pack_snap(&s, buf, n4 - 1) == -1);  // cap too small
    s.nev = NET_MAX_EVENTS + 1;
    CHECK(net_pack_snap(&s, buf, sizeof buf) == -1);  // too many events
}

static void test_reject(void) {
    NetSnap s = sample_snap(1), r;
    uint8_t buf[NET_PKT_MAX], bad[NET_PKT_MAX];
    int n = net_pack_snap(&s, buf, sizeof buf);
    int type; uint8_t player; uint16_t seq; char name[NET_NAME_MAX + 1];
    memcpy(bad, buf, n); bad[0] = 'X';
    CHECK(!net_unpack(bad, n, &type, &player, &seq, &r, name));
    memcpy(bad, buf, n); bad[2] = NET_VERSION + 1;  // another version
    CHECK(!net_unpack(bad, n, &type, &player, &seq, &r, name));
    CHECK(!net_unpack(buf, n - 1, &type, &player, &seq, &r, name));  // truncated by 1 byte
    CHECK(!net_unpack(buf, 0, &type, &player, &seq, &r, name));
    memcpy(bad, buf, n); bad[3] = 9;  // unknown type
    CHECK(!net_unpack(bad, n, &type, &player, &seq, &r, name));
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name));  // the original is fine
}

static void test_hello_name(void) {
    uint8_t buf[NET_PKT_MAX];
    int type; uint8_t player; uint16_t seq; NetSnap r; char name[NET_NAME_MAX + 1];
    int n = net_pack_hello(9, 77, "Kairi", buf, sizeof buf);
    CHECK(n > 0);
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name));
    CHECK(type == NET_HELLO && player == 9 && seq == 77 && strcmp(name, "Kairi") == 0);
    n = net_pack_hello(9, 78, "ABCDEFGHIJKLMNOPQRST", buf, sizeof buf);  // 20 chars
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name));
    CHECK(strlen(name) == NET_NAME_MAX && strcmp(name, "ABCDEFGHIJKLMNO") == 0);
    n = net_pack_bye(9, 79, buf, sizeof buf);
    CHECK(n > 0 && net_unpack(buf, n, &type, &player, &seq, &r, name) && type == NET_BYE && seq == 79);
}

static void test_seq_wrap_and_reorder(void) {
    CHECK(net_seq_newer(1, 65535));
    CHECK(!net_seq_newer(65535, 1));
    CHECK(!net_seq_newer(5, 5));
    CHECK(net_seq_newer(300, 200));
    CHECK(!net_seq_newer(200, 300));
}

static void test_event_dedupe(void) {
    NetEvSeen s;
    memset(&s, 0, sizeof s);
    CHECK(net_event_fresh(&s, 7));
    CHECK(!net_event_fresh(&s, 7));  // the 2 repeats
    CHECK(!net_event_fresh(&s, 7));
    CHECK(net_event_fresh(&s, 8));
    CHECK(!net_event_fresh(&s, 7));  // 7 again after 8
    CHECK(net_event_fresh(&s, 10));  // 9 lost / late
    CHECK(net_event_fresh(&s, 9));   // late but never played: fresh once
    CHECK(!net_event_fresh(&s, 9));
    for (int q = 11; q <= 255; q++) net_event_fresh(&s, (uint8_t)q);
    CHECK(net_event_fresh(&s, 0));   // wrap 255 -> 0
    CHECK(!net_event_fresh(&s, 255));
    CHECK(net_event_fresh(&s, 7));   // a new 7 after the wrap (old 7 was 256 events ago)
    CHECK(net_event_fresh(&s, 100));   // 93 ahead (u8: same as 163 behind) = newer
    CHECK(!net_event_fresh(&s, 228));  // exactly 128 from 100: ambiguous half-way point = too old
    CHECK(!net_event_fresh(&s, 7));    // 93 behind, already played
}

// ---- Task 3: peer state + interpolation (sora_net_peer.h) ----
static NetSnap snap_at(uint16_t seq, float x, float yaw, uint16_t slot, float clock) {
    NetSnap s;
    memset(&s, 0, sizeof s);
    s.player = 5, s.seq = seq, s.feet[0] = x, s.yaw = yaw, s.slot = slot, s.clock = clock, s.hp = 100;
    return s;
}
static double ang_diff(double a, double b) { return fabs(atan2(sin(a - b), cos(a - b))); }

static void test_interp_midpoint(void) {
    NetPeer p; memset(&p, 0, sizeof p);
    peer_on_hello(&p, 5, "Deck", 1000);
    NetSnap a = snap_at(1, 0, 0, 10, 0), b = snap_at(2, 100, 0, 10, 3);
    CHECK(peer_on_snap(&p, &a, 1000));
    CHECK(peer_on_snap(&p, &b, 1050));
    PeerPose q = peer_sample(&p, 1145);  // render time 1025
    CHECK(q.valid && NEAR(q.feet[0], 50, 0.5) && q.alpha == 1.0f);
    CHECK(strcmp(p.name, "Deck") == 0);
    q = peer_sample(&p, 1000);  // render before the first SNAP: hold the oldest, no extrapolation
    CHECK(q.valid && NEAR(q.feet[0], 0, 0.001));
    q = peer_sample(&p, 5000);  // long after the newest: hold the newest
    CHECK(q.valid && NEAR(q.feet[0], 100, 0.001));
}

static void test_yaw_shortest_arc(void) {
    NetPeer p; memset(&p, 0, sizeof p);
    peer_on_hello(&p, 5, "Deck", 0);
    NetSnap a = snap_at(1, 0, 6.2f, 10, 0), b = snap_at(2, 0, 0.1f, 10, 0);
    peer_on_snap(&p, &a, 1000);
    peer_on_snap(&p, &b, 1050);
    PeerPose q = peer_sample(&p, 1145);
    double mid = 6.2 + (0.1 + NET_TWO_PI - 6.2) / 2;  // 6.2916
    CHECK(ang_diff(q.yaw, mid) < 0.01);
    CHECK(ang_diff(q.yaw, 3.15) > 3.0);
    CHECK(q.yaw >= 0 && q.yaw < NET_TWO_PI);
}

static void test_slot_switch_resets_clock(void) {
    NetPeer p; memset(&p, 0, sizeof p);
    peer_on_hello(&p, 5, "Deck", 0);
    NetSnap a = snap_at(1, 0, 0, 10, 30), b = snap_at(2, 0, 0, 11, 0);
    peer_on_snap(&p, &a, 1000);
    peer_on_snap(&p, &b, 1050);
    PeerPose q = peer_sample(&p, 1180);  // render 1060, after the switch
    CHECK(q.slot == 11 && q.clock >= 0 && q.clock < 2);
    q = peer_sample(&p, 1160);  // render 1040, before the switch: old slot, its clock advanced 2.4
    CHECK(q.slot == 10 && NEAR(q.clock, 32.4, 0.1));
}

static void test_clock_follow_fps_independent(void) {
    NetPeer p; memset(&p, 0, sizeof p);
    peer_on_hello(&p, 5, "Deck", 0);
    NetSnap a = snap_at(1, 0, 0, 10, 0);
    peer_on_snap(&p, &a, 0);
    PeerPose q1 = peer_sample(&p, 620);
    for (int i = 0; i < 30; i++) peer_sample(&p, 120 + (uint64_t)i * 16);
    PeerPose q2 = peer_sample(&p, 620);
    CHECK(NEAR(q1.clock, 30, 0.1) && NEAR(q2.clock, 30, 0.1) && q1.clock == q2.clock);
}

static void test_timeouts(void) {
    NetPeer p; memset(&p, 0, sizeof p);
    peer_on_hello(&p, 5, "Deck", 0);
    NetSnap a = snap_at(1, 0, 0, 10, 0);
    peer_on_snap(&p, &a, 0);
    peer_tick(&p, 1999);
    CHECK(p.st == PEER_LIVE && peer_sample(&p, 1999).alpha == 1.0f);
    peer_tick(&p, 2001);
    PeerPose q = peer_sample(&p, 2001);
    CHECK(p.st == PEER_FADING && q.valid && q.alpha < 1.0f && q.alpha > 0.9f);
    peer_tick(&p, 2600);
    CHECK(p.st == PEER_FADING && peer_sample(&p, 2600).alpha == 0.0f);
    NetSnap b = snap_at(2, 0, 0, 10, 0);  // a SNAP while fading = LIVE again
    CHECK(peer_on_snap(&p, &b, 3000) && p.st == PEER_LIVE && peer_sample(&p, 3000).alpha == 1.0f);
    peer_tick(&p, 13001);
    q = peer_sample(&p, 13001);
    CHECK(p.st == PEER_NONE && !q.valid && q.alpha == 0.0f);
}

static void test_peer_restart(void) {
    NetPeer p; memset(&p, 0, sizeof p);
    peer_on_hello(&p, 5, "Deck", 0);
    NetSnap a = snap_at(5000, 0, 0, 10, 0);
    CHECK(peer_on_snap(&p, &a, 0));
    peer_on_bye(&p);
    CHECK(p.st == PEER_NONE && !peer_sample(&p, 100).valid);
    peer_on_hello(&p, 5, "Deck", 200);
    NetSnap b = snap_at(1, 7, 0, 10, 0);
    CHECK(peer_on_snap(&p, &b, 250) && p.st == PEER_LIVE);
    CHECK(NEAR(peer_sample(&p, 400).feet[0], 7, 0.001));
    // restart without BYE (crash): new player id -> accepted at once, old ring gone
    NetSnap c = snap_at(1, 9, 0, 10, 0); c.player = 6;
    CHECK(peer_on_snap(&p, &c, 300) && p.player == 6 && p.n == 1);
    // restart with the same id: its HELLO seq is older than what we have -> reset, then low seqs accepted
    NetSnap d = snap_at(900, 9, 0, 10, 0); d.player = 6;
    CHECK(peer_on_snap(&p, &d, 350));
    peer_on_hello_seq(&p, 6, "Deck", 2, 400);
    NetSnap e = snap_at(3, 11, 0, 10, 0); e.player = 6;
    CHECK(peer_on_snap(&p, &e, 450) && p.n == 1);
}

static void test_stale_snap_dropped(void) {
    NetPeer p; memset(&p, 0, sizeof p);
    peer_on_hello(&p, 5, "Deck", 0);
    NetSnap a = snap_at(10, 1, 0, 10, 0), b = snap_at(9, 2, 0, 10, 0);
    CHECK(peer_on_snap(&p, &a, 100));
    CHECK(!peer_on_snap(&p, &b, 110));
    CHECK(p.n == 1 && p.ring[0].seq == 10 && p.last_seq == 10);
    for (int i = 0; i < 12; i++) { NetSnap s = snap_at((uint16_t)(11 + i), (float)i, 0, 10, 0); peer_on_snap(&p, &s, 200 + i * 50); }
    CHECK(p.n == NET_RING && p.ring[NET_RING - 1].seq == 22 && p.ring[0].seq == 15);
}

static void test_take_events_once(void) {
    NetPeer p; memset(&p, 0, sizeof p);
    peer_on_hello(&p, 5, "Deck", 0);
    NetEvent out[NET_MAX_EVENTS];
    NetSnap s = snap_at(1, 0, 0, 10, 0);
    s.nev = 2, s.ev[0].seq = 40, s.ev[0].id = 3, s.ev[1].seq = 41, s.ev[1].id = 4;
    CHECK(peer_take_events(&p, &s, out) == 2 && out[0].id == 3 && out[1].id == 4);
    s.seq = 2, s.ev[0].seq = 41, s.ev[1].seq = 42, s.ev[1].id = 5;  // 41 repeated
    CHECK(peer_take_events(&p, &s, out) == 1 && out[0].id == 5);
}

static void test_fill_flags(void) {
    CHECK(net_flags(false, false, false, false, false, false, false) == 0);
    CHECK(net_flags(true, false, false, false, false, false, false) == NF_WEAPON);
    CHECK(net_flags(false, true, false, false, false, false, false) == NF_AIR);
    CHECK(net_flags(false, false, true, false, false, false, false) == NF_GHOST);
    CHECK(net_flags(false, false, false, true, false, false, false) == NF_GUARD);
    CHECK(net_flags(false, false, false, false, true, false, false) == NF_DEAD);
    CHECK(net_flags(false, false, false, false, false, true, false) == NF_LOADING);
    CHECK(net_flags(false, false, false, false, false, false, true) == NF_BONEPOSE);
    CHECK(net_flags(true, true, true, true, true, true, true) == 127);
}

// Task 6: the remote clock past its motion's end (loops wrap into [loop, frames), others hold) + mset trigger ranges
static void test_clock_wrap(void) {
    CHECK(NEAR(peer_clock_wrap(39, 40, 10, true), 39, 1e-4));
    CHECK(NEAR(peer_clock_wrap(45, 40, 10, true), 15, 1e-4));
    CHECK(NEAR(peer_clock_wrap(105, 40, 10, true), 15, 1e-4));   // (several laps)
    CHECK(NEAR(peer_clock_wrap(45, 40, 10, false), 40, 1e-4));   // (non-loop holds its last frame)
    CHECK(NEAR(peer_clock_wrap(45, 40, 40, true), 40, 1e-4));    // (loop == frames: nothing to loop)
    CHECK(NEAR(peer_clock_wrap(45, 0, 0, true), 45, 1e-4));      // (unknown motion: as is)
    CHECK(NEAR(peer_clock_wrap(-3, 40, 0, true), 0, 1e-4));
}
static void test_trigger_ranges(void) {
    PeerRange r[2]; float keep;
    int n = peer_trigger_ranges(-1, 0, 412, 3, 40, 10, true, r, &keep);  // first sample = a motion start
    CHECK(n == 1 && r[0].from == -1 && NEAR(r[0].to, 3, 1e-4) && NEAR(keep, 3, 1e-4));
    n = peer_trigger_ranges(412, 3, 412, 8, 40, 10, true, r, &keep);   // forward
    CHECK(n == 1 && NEAR(r[0].from, 3, 1e-4) && NEAR(r[0].to, 8, 1e-4) && NEAR(keep, 8, 1e-4));
    n = peer_trigger_ranges(412, 8, 412, 7, 40, 10, true, r, &keep);   // a new SNAP a little behind: nothing, prev kept
    CHECK(n == 0 && NEAR(keep, 8, 1e-4));
    n = peer_trigger_ranges(412, 8, 412, 9, 40, 10, true, r, &keep);   // on from the kept prev
    CHECK(n == 1 && NEAR(r[0].from, 8, 1e-4) && NEAR(r[0].to, 9, 1e-4));
    n = peer_trigger_ranges(412, 38, 412, 12, 40, 10, true, r, &keep); // loop wrap: the tail + from the loop point
    CHECK(n == 2 && NEAR(r[0].from, 38, 1e-4) && NEAR(r[0].to, 40, 1e-4) && r[1].from < 10 && r[1].from > 9.9f &&
          NEAR(r[1].to, 12, 1e-4) && NEAR(keep, 12, 1e-4));
    n = peer_trigger_ranges(412, 30, 412, 2, 40, 0, false, r, &keep);  // same non-loop motion restarted
    CHECK(n == 1 && r[0].from == -1 && NEAR(r[0].to, 2, 1e-4));
    n = peer_trigger_ranges(412, 30, 413, 2, 40, 0, false, r, &keep);  // slot change
    CHECK(n == 1 && r[0].from == -1 && NEAR(r[0].to, 2, 1e-4) && NEAR(keep, 2, 1e-4));
    n = peer_trigger_ranges(412, 40, 412, 40, 40, 0, false, r, &keep); // held at the end: nothing again
    CHECK(n == 0);
}

// Task 7: the name plate's quads (DLL) + the exe's anchor mapping (sora_hud_label)
static void test_plate_quads(void) {
    static uint8_t ascii[128], adv[96];
    for (int c = 0x21; c < 0x7F; c++) ascii[c] = (uint8_t)c;  // (test font: cell = char - 0x20, as the sys font)
    for (int i = 0; i < 96; i++) adv[i] = 10;
    CampSeq s; memset(&s, 0, sizeof s);
    s.ascii = ascii, s.adv = adv, s.n_adv = 96;
    SoraHudQuad q[PLATE_MAX];
    int n = plate_quads(&s, "Deck", 1.0f, q, PLATE_MAX);
    CHECK(n == 8);  // (shadow + text per letter)
    float w = 4 * 10 * PLATE_SCALE * 0.85f;  // (cs_text: x scale = scale * 0.85)
    // the text pass (second half): centred on x 0, bottom on y 0, white, sys font
    CHECK(NEAR(q[4].x[0], -w / 2, 1e-3) && NEAR(q[4].y[3], 0, 1e-3) && NEAR(q[4].y[0], -24 * PLATE_SCALE, 1e-3));
    CHECK(q[4].tex == 1 && q[4].col[0] == 0x80808080u);
    CHECK(q[4].u[0] == (float)(('D' - 0x20) % 28 * 18) && q[4].v[0] == (float)(('D' - 0x20) / 28 * 24));
    // the shadow (first half): 1.5 px down-right, black, 3/4 alpha
    CHECK(NEAR(q[0].x[0], q[4].x[0] + 1.5f, 1e-3) && NEAR(q[0].y[0], q[4].y[0] + 1.5f, 1e-3) && q[0].col[0] == 0x60000000u);
    n = plate_quads(&s, "Deck", 0.5f, q, PLATE_MAX);  // (fading: alpha scales)
    CHECK(n == 8 && q[4].col[0] == 0x40808080u && q[0].col[0] == 0x30000000u);
    CHECK(plate_quads(&s, "", 1.0f, q, PLATE_MAX) == 0);
    CHECK(plate_quads(&s, NULL, 1.0f, q, PLATE_MAX) == 0);
    CHECK(plate_quads(&s, "", 1.0f, q, PLATE_MAX) == 0);  // (no glyph: nothing)
    CHECK(plate_quads(&s, "Deck", 1.0f, q, 5) <= 5);                 // (cap kept)
    CHECK(plate_quads(&s, "Deck", 0.0f, q, PLATE_MAX) == 0);         // (invisible: nothing)
    n = plate_quads(&s, "A B", 1.0f, q, PLATE_MAX);                  // (space = gap, no quad)
    CHECK(n == 4 && NEAR(q[3].x[0] - q[2].x[0], (10 + 6) * PLATE_SCALE * 0.85f, 1e-3));  // (A advance + space 6)
}
static void test_label_origin(void) {
    // camp_draw maps virtual (qx, qy) -> window (cx + (qx - 256) * sx, vy + qy * sy); the origin puts virtual 0, 0 on px, py
    const float vx = 100, vy = 20, vw = 1280, vh = 720;
    float sy = vh / 416.0f, sx = sy * 416.0f / 384.0f, cx = vx + vw * 0.5f, ox, oy;
    hud_label_origin(333, 444, vx, vy, vw, vh, 1480, 760, &ox, &oy);
    CHECK(NEAR(cx + (ox - 256) * sx, 333, 1e-3) && NEAR(vy + oy * sy, 444, 1e-3));
    hud_label_origin(cx, vy, vx, vy, vw, vh, 1480, 760, &ox, &oy);  // (frame centre top = virtual 256, 0)
    CHECK(NEAR(ox, 256, 1e-4) && NEAR(oy, 0, 1e-4));
    hud_label_origin(640, 400, 0, 0, 0, 0, 1280, 800, &ox, &oy);  // (no game image: the window, as camp_draw)
    CHECK(NEAR(ox, 256, 1e-3) && NEAR(oy, 208, 1e-3));
}

// (mp2) NaN / Inf never reach the fx / pose calls; sfx gain is clamped; wrong-version packets are told apart
static void test_nonfinite_and_gain(void) {
    uint8_t buf[NET_PKT_MAX];
    int type; uint8_t player; uint16_t seq; NetSnap r; char name[NET_NAME_MAX + 1];
    float bad[3] = {NAN, INFINITY, -INFINITY};
    for (int k = 0; k < 3; k++) {
        NetSnap s = sample_snap(2);  // feet
        s.feet[k] = bad[k];
        int n = net_pack_snap(&s, buf, sizeof buf);
        CHECK(n > 0 && !net_unpack(buf, n, &type, &player, &seq, &r, name));
        s = sample_snap(2);  // event pos: that event alone is dropped, the rest of the packet stays
        s.ev[1].pos[k] = bad[k];
        n = net_pack_snap(&s, buf, sizeof buf);
        CHECK(n > 0 && net_unpack(buf, n, &type, &player, &seq, &r, name) && r.nev == 1 && r.ev[0].seq == s.ev[0].seq);
        s = sample_snap(1);  // move pos: a place move is dropped, a kill (pos unused) is kept
        s.nmv = 2, s.mv[0].ev = 3, s.mv[0].flags = 0, s.mv[0].pos[k] = bad[k];
        s.mv[1].ev = 4, s.mv[1].flags = NMV_KILL, s.mv[1].pos[0] = bad[k], s.mv[1].pos[1] = s.mv[1].pos[2] = 0;
        n = net_pack_snap(&s, buf, sizeof buf);
        CHECK(n > 0 && net_unpack(buf, n, &type, &player, &seq, &r, name) && r.nmv == 1 && r.mv[0].ev == 4 &&
              (r.mv[0].flags & NMV_KILL) && r.nev == 1);
    }
    NetSnap ok = sample_snap(1);
    ok.nmv = 1, ok.mv[0].ev = 3, ok.mv[0].pos[0] = 1, ok.mv[0].pos[1] = 2, ok.mv[0].pos[2] = 3;
    int n = net_pack_snap(&ok, buf, sizeof buf);
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name) && r.nmv == 1);  // finite = fine
    // sfx gain: 20 would decode ~20 -> NET_GAIN_MAX; 2 stays; an fx (kind 0) scale is not a gain
    NetSnap g = sample_snap(3);
    g.ev[0].kind = 1, g.ev[0].scale = 20.0f;
    g.ev[1].kind = 1, g.ev[1].scale = 2.0f;
    g.ev[2].kind = 0, g.ev[2].scale = 20.0f;
    n = net_pack_snap(&g, buf, sizeof buf);
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name));
    CHECK(r.ev[0].scale == NET_GAIN_MAX && NEAR(r.ev[1].scale, 2.0, 0.07) && r.ev[2].scale > NET_GAIN_MAX);
    // version helper
    NetSnap v = sample_snap(0);
    n = net_pack_snap(&v, buf, sizeof buf);
    CHECK(net_other_version(buf, n) == -1);
    buf[2] = NET_VERSION - 1;
    CHECK(net_other_version(buf, n) == NET_VERSION - 1);
    buf[0] = 'X';
    CHECK(net_other_version(buf, n) == -1 && net_other_version(buf, 2) == -1);
    NetVerSeen t;
    memset(&t, 0, sizeof t);
    CHECK(net_ver_first(&t, 0x0100007f) && !net_ver_first(&t, 0x0100007f) && net_ver_first(&t, 0x0200007f));
    for (uint32_t ip = 100; ip < 120; ip++) CHECK(net_ver_first(&t, ip) && !net_ver_first(&t, ip));
}

// ---- slice 2 Task 2: the remote swing ribbon core (sora_net_ribbon.h)
static const char RIB_ROW[2];  // two distinct row addresses stand in for the hit table rows
static const void *rib_hit(void *ud, int form, int slot, int blade) {
    (void)ud;
    if (form == 99 || slot < 0) return NULL;       // (a form / slot with no blade table)
    if (blade == 1 && form != 0) return NULL;      // (only form 0 has a left blade here)
    return &RIB_ROW[blade];
}
static bool rib_win(void *ud, int form, int slot, float clock) { (void)ud; (void)form; (void)slot; return clock >= 10 && clock <= 20; }
static void rib_blade(void *ud, const void *row, float clock, const float feet[3], float yaw, float to_u, float out[6]) {
    (void)ud; (void)row;
    for (int i = 0; i < 3; i++) out[i] = feet[i], out[3 + i] = feet[i] + 1;
    out[0] += clock, out[3] += yaw + to_u;
}
static NetRibCb rib_cb(void) { NetRibCb c = {rib_hit, rib_win, rib_blade, NULL}; return c; }
static NetRibIn rib_in(int form, int slot, float clock, uint64_t now) {
    NetRibIn i = {true, form, slot, clock, {100, 5, -50}, 0.5f, 0.01f, now};
    return i;
}

static void test_ribbon_decimation(void) {
    NetRib r; NetRibOut o[2]; NetRibCb cb = rib_cb();
    netrib_init(&r);
    NetRibIn i = rib_in(0, 7, 12, 1000);
    CHECK(netrib_step(&r, &cb, &i, o) == 0);           // (first sight of a slot = a reset, nothing owed)
    int pushes = 0;
    for (int t = 0; t < 100; t++) {                    // 100 display frames at 4 ms = 400 ms
        i = rib_in(0, 7, 12 + t * 0.01f, 1000 + t * 4);
        pushes += netrib_step(&r, &cb, &i, o) > 0;
    }
    CHECK(pushes >= 11 && pushes <= 13);               // ~30 Hz, not 100
}

static void test_ribbon_window_and_feet(void) {
    NetRib r; NetRibOut o[2]; NetRibCb cb = rib_cb();
    netrib_init(&r);
    NetRibIn i = rib_in(0, 7, 5, 0);
    netrib_step(&r, &cb, &i, o);                       // reset
    i = rib_in(0, 7, 6, 40);
    CHECK(netrib_step(&r, &cb, &i, o) == 0);           // outside the window: quiet
    i = rib_in(0, 7, 12, 80);
    CHECK(netrib_step(&r, &cb, &i, o) == 2);           // window opens (the mock gives form 0 both blades)
    CHECK(o[0].push && o[0].on && o[1].push && o[1].on);
    CHECK(NEAR(o[0].grip[0], 112, 1e-4) && NEAR(o[0].grip[1], 5, 1e-4) && NEAR(o[0].grip[2], -50, 1e-4));  // feet pass through
    CHECK(NEAR(o[0].tip[0], 101 + 0.5 + 0.01, 1e-3) && NEAR(o[0].tip[1], 6, 1e-4));
    i = rib_in(0, 7, 25, 120);
    netrib_step(&r, &cb, &i, o);                       // window closed: one on = false per live blade
    CHECK(o[0].push && !o[0].on && o[1].push && !o[1].on);
    i = rib_in(0, 7, 26, 160);
    CHECK(netrib_step(&r, &cb, &i, o) == 0);           // then quiet
    netrib_init(&r);                                   // a form with one blade only
    i = rib_in(3, 7, 12, 0); netrib_step(&r, &cb, &i, o);
    i = rib_in(3, 7, 13, 40);
    CHECK(netrib_step(&r, &cb, &i, o) == 1 && o[0].on && !o[1].push);
    netrib_init(&r);                                   // a form without a table: nothing
    i = rib_in(99, 7, 12, 0); netrib_step(&r, &cb, &i, o);
    i = rib_in(99, 7, 13, 40);
    CHECK(netrib_step(&r, &cb, &i, o) == 0);
}

static void test_ribbon_reset(void) {
    NetRib r; NetRibOut o[2]; NetRibCb cb = rib_cb();
    netrib_init(&r);
    NetRibIn i = rib_in(3, 7, 12, 0); netrib_step(&r, &cb, &i, o);
    i = rib_in(3, 7, 13, 40); CHECK(netrib_step(&r, &cb, &i, o) == 1);
    i = rib_in(3, 8, 13, 80);                          // slot change: on = false once
    CHECK(netrib_step(&r, &cb, &i, o) == 1 && o[0].push && !o[0].on);
    i = rib_in(3, 8, 14, 90);
    CHECK(netrib_step(&r, &cb, &i, o) == 1 && o[0].on);   // the new swing may push at once
    i = rib_in(3, 8, 11, 130);                         // clock went backward (peer_clock_wrap loop)
    CHECK(netrib_step(&r, &cb, &i, o) == 1 && o[0].push && !o[0].on);
    i = rib_in(3, 8, 11.5f, 140);
    CHECK(netrib_step(&r, &cb, &i, o) == 1 && o[0].on);
    i = rib_in(3, 8, 11.5f, 190);                      // same clock again (paused): not a reset
    netrib_step(&r, &cb, &i, o);
    CHECK(!(o[0].push && !o[0].on));
}

static void test_ribbon_inactive(void) {
    NetRib r; NetRibOut o[2]; NetRibCb cb = rib_cb();
    netrib_init(&r);
    NetRibIn i = rib_in(3, 7, 12, 0); netrib_step(&r, &cb, &i, o);
    i = rib_in(3, 7, 13, 40); netrib_step(&r, &cb, &i, o);
    i = rib_in(3, 7, 14, 80); i.active = false;        // bone pose / Anti / hidden / avatar off
    CHECK(netrib_step(&r, &cb, &i, o) == 1 && o[0].push && !o[0].on);
    for (int t = 1; t < 20; t++) {
        i = rib_in(3, 7, 14 + t, 80 + t * 40); i.active = false;
        CHECK(netrib_step(&r, &cb, &i, o) == 0);       // quiet from then on
    }
    netrib_init(&r);                                   // never active: never a push
    i = rib_in(3, 7, 14, 0); i.active = false;
    CHECK(netrib_step(&r, &cb, &i, o) == 0 && !o[0].push && !o[1].push);
}

// (slice 3) the party block: both members, Goofy with a flying shield, round trip
static void sample_party(NetSnap *s) {
    s->npm = 2;
    NetPartyMember *d = &s->pm[0], *g = &s->pm[1];
    memset(d, 0, sizeof *d), memset(g, 0, sizeof *g);
    d->who = 0, d->flags = NPM_WEAPON | NPM_BLEND, d->slot = 1508, d->clock = 12.3f;
    d->feet[0] = 1236.5f, d->feet[1] = 210.0f, d->feet[2] = -985.5f, d->face = 1.25f, d->floor_dy = -0.42f;
    d->from_slot = 4, d->from_clock = 30.0f, d->blend = 0.5f;
    g->who = 1, g->flags = NPM_GHOST | NPM_SHIELD, g->slot = 400, g->clock = 3.0f;
    g->feet[0] = 1230.0f, g->feet[1] = 209.5f, g->feet[2] = -990.0f, g->face = 4.0f, g->floor_dy = 0;
    g->shield[0] = 1240.0f, g->shield[1] = 211.0f, g->shield[2] = -995.0f, g->shield_yaw = 0.75f, g->shield_t = 7.5f;
}
static void test_party_roundtrip(void) {
    NetSnap s = sample_snap(4), r;
    sample_party(&s);
    s.nmv = NET_MAX_MOVES;
    for (int i = 0; i < NET_MAX_MOVES; i++) memset(&s.mv[i], 0, sizeof s.mv[i]), s.mv[i].ev = (uint8_t)i;
    uint8_t buf[NET_PKT_MAX];
    int n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(n == NET_HDR + NET_SNAP_BODY + 4 * NET_EV_SIZE + NET_MAX_MOVES * NET_MV_SIZE + 2 * NET_PM_SIZE + NET_PM_SHIELD);
    CHECK(n <= NET_PKT_MAX && n <= 400);  // (the biggest SNAP stays small)
    int type; uint8_t player; uint16_t seq; char name[NET_NAME_MAX + 1];
    memset(&r, 0xcc, sizeof r);
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name));
    CHECK(r.npm == 2 && r.nev == 4 && r.nmv == NET_MAX_MOVES);
    const NetPartyMember *d = &r.pm[0], *g = &r.pm[1];
    CHECK(d->who == 0 && d->flags == (NPM_WEAPON | NPM_BLEND) && d->slot == 1508 && NEAR(d->clock, 12.3, 0.05));
    CHECK(d->feet[0] == 1236.5f && d->feet[1] == 210.0f && d->feet[2] == -985.5f && NEAR(d->face, 1.25, 0.0002));
    CHECK(NEAR(d->floor_dy, -0.42, 0.006) && d->from_slot == 4 && NEAR(d->from_clock, 30.0, 0.05) && NEAR(d->blend, 0.5, 0.003));
    CHECK(g->who == 1 && g->flags == (NPM_GHOST | NPM_SHIELD) && g->slot == 400 && NEAR(g->face, 4.0, 0.0002));
    CHECK(g->shield[0] == 1240.0f && g->shield[1] == 211.0f && g->shield[2] == -995.0f);
    CHECK(NEAR(g->shield_yaw, 0.75, 0.0002) && NEAR(g->shield_t, 7.5, 0.05) && g->blend == 0);
    // no party = the old size + nothing; one member without a shield
    s.npm = 0;
    int n0 = net_pack_snap(&s, buf, sizeof buf);
    CHECK(n0 == n - 2 * NET_PM_SIZE - NET_PM_SHIELD);
    CHECK(net_unpack(buf, n0, &type, &player, &seq, &r, name) && r.npm == 0);
    s.npm = NET_PARTY_N + 1;
    CHECK(net_pack_snap(&s, buf, sizeof buf) == -1);
}
static void test_party_reject(void) {
    NetSnap s = sample_snap(0), r;
    sample_party(&s);
    uint8_t buf[NET_PKT_MAX], bad[NET_PKT_MAX];
    int n = net_pack_snap(&s, buf, sizeof buf);
    int type; uint8_t player; uint16_t seq; char name[NET_NAME_MAX + 1];
    CHECK(!net_unpack(buf, n - 1, &type, &player, &seq, &r, name));  // truncated inside the shield
    int g0 = NET_HDR + NET_SNAP_BODY + NET_PM_SIZE;                  // Goofy's entry
    memcpy(bad, buf, n); bad[g0 + 1] &= (uint8_t)~NPM_SHIELD;       // shield flag off: the size no longer adds up
    CHECK(!net_unpack(bad, n, &type, &player, &seq, &r, name));
    memcpy(bad, buf, n); bad[NET_HDR + 27] = 3;                      // 3 members
    CHECK(!net_unpack(bad, n, &type, &player, &seq, &r, name));
    memcpy(bad, buf, n); bad[g0] = 0;                                // Goofy says he is Donald: the repeat is dropped
    CHECK(net_unpack(bad, n, &type, &player, &seq, &r, name) && r.npm == 1 && r.pm[0].who == 0);
    memcpy(bad, buf, n); bad[g0] = 7;                                // unknown member
    CHECK(net_unpack(bad, n, &type, &player, &seq, &r, name) && r.npm == 1);
    memcpy(bad, buf, n); net_wf(bad + NET_HDR + NET_SNAP_BODY + 6, NAN);  // Donald's feet NaN: Donald alone dropped
    CHECK(net_unpack(bad, n, &type, &player, &seq, &r, name) && r.npm == 1 && r.pm[0].who == 1);
    memcpy(bad, buf, n); net_wf(bad + g0 + NET_PM_SIZE, INFINITY);  // a bad shield: Goofy kept, shield off
    CHECK(net_unpack(bad, n, &type, &player, &seq, &r, name) && r.npm == 2 && !(r.pm[1].flags & NPM_SHIELD));
}
// (slice 3) fx anchors: 2 bits beside the event kind / move flags; the local frame round trip
static void test_anchors(void) {
    NetSnap s = sample_snap(2), r;
    s.ev[0].anchor = NANC_GOOFY, s.ev[1].anchor = NANC_SORA;
    s.nmv = 2;
    memset(s.mv, 0, sizeof s.mv[0] * 2);
    s.mv[0].ev = 5, s.mv[0].anchor = NANC_DONALD, s.mv[0].pos[2] = 1.5f;
    s.mv[1].ev = 6, s.mv[1].flags = NMV_FADE, s.mv[1].fade = 8, s.mv[1].anchor = NANC_SORA;
    uint8_t buf[NET_PKT_MAX];
    int n = net_pack_snap(&s, buf, sizeof buf);
    int type; uint8_t player; uint16_t seq; char name[NET_NAME_MAX + 1];
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name));
    CHECK(r.ev[0].anchor == NANC_GOOFY && r.ev[0].kind == 0 && r.ev[1].anchor == NANC_SORA && r.ev[1].kind == 1);
    CHECK(r.mv[0].anchor == NANC_DONALD && r.mv[0].flags == 0 && r.mv[0].pos[2] == 1.5f);
    CHECK(r.mv[1].anchor == NANC_SORA && r.mv[1].flags == NMV_FADE && NEAR(r.mv[1].fade, 8, 0.05));
    // local frame: +z = facing (sin yaw, cos yaw), round trip exact-ish for any yaw
    float feet[3] = {100, 20, -50}, l[3], w[3];
    float fwd[3] = {feet[0] + sinf(0.7f) * 2, feet[1] + 1, feet[2] + cosf(0.7f) * 2};
    net_to_local(feet, 0.7f, fwd, l);
    CHECK(NEAR(l[0], 0, 1e-4) && NEAR(l[1], 1, 1e-4) && NEAR(l[2], 2, 1e-4));
    for (int k = 0; k < 8; k++) {
        float yaw = -3.0f + k * 0.9f, p[3] = {103.5f - k, 18.0f + k, -47.25f + 2 * k};
        net_to_local(feet, yaw, p, l);
        net_from_local(feet, yaw, l, w);
        CHECK(NEAR(w[0], p[0], 1e-3) && NEAR(w[1], p[1], 1e-3) && NEAR(w[2], p[2], 1e-3));
    }
    // a bound offset follows a moved + turned body
    float feet2[3] = {200, 25, 10};
    net_from_local(feet2, 0.7f + 3.14159265f / 2, (float[3]){0, 1, 2}, w);  // turned 90 deg: facing (cos .7, -sin .7)
    CHECK(NEAR(w[0], 200 + cosf(0.7f) * 2, 1e-3) && NEAR(w[1], 26, 1e-4) && NEAR(w[2], 10 - sinf(0.7f) * 2, 1e-3));
}

// (slice 3, unit B) which body an fx is anchored to, when a moving anchored fx is re-sent, the receiver's track reset
static void test_anchor_logic(void) {
    NetBody b[3];
    memset(b, 0, sizeof b);
    b[0].ok = true, b[0].feet[0] = 0, b[0].yaw = 0.5f;
    b[1].ok = true, b[1].feet[0] = 4, b[1].yaw = 1.0f;     // Donald 4 u east
    b[2].ok = false, b[2].feet[0] = 1;                       // Goofy not drawn
    float p[3] = {3.0f, 0.5f, 0}, p2[3] = {-9, 0, 0};
    CHECK(net_anchor_pick(b, 3, p, 5.0f) == 1);              // nearest = Donald (1 u) not Sora (3 u)
    CHECK(net_anchor_pick(b, 3, p2, 5.0f) == -1);            // 9 u from Sora: world
    float p3[3] = {1, 0, 0};
    CHECK(net_anchor_pick(b, 3, p3, 5.0f) == 0 && net_anchor_pick(b, 0, p3, 5.0f) == -1);  // Goofy (not ok, closest) ignored
    float py[3] = {0, 4.9f, 0};
    CHECK(net_anchor_pick(b, 3, py, 5.0f) == 0);             // vertical counts
    py[1] = 5.1f;
    CHECK(net_anchor_pick(b, 3, py, 5.0f) == -1);
    // encode / decode round trip on Donald, then on another pose (the drawn body moved: the fx follows)
    float off[3], oy, w[3], wy, wp[3] = {5.0f, 1.0f, 2.0f};
    net_anchor_enc(&b[1], wp, 2.5f, off, &oy);
    net_anchor_dec(&b[1], off, oy, w, &wy);
    CHECK(NEAR(w[0], 5, 1e-4) && NEAR(w[1], 1, 1e-4) && NEAR(w[2], 2, 1e-4) && NEAR(wy, 2.5, 1e-4));
    NetBody m = b[1];
    m.feet[0] = 10, m.feet[2] = -3, m.yaw = 1.0f + 0.7f;
    net_anchor_dec(&m, off, oy, w, &wy);
    CHECK(NEAR(wy, 3.2, 1e-4) && NEAR(hypot(w[0] - 10, w[2] + 3), hypot(1, 2), 1e-4));  // same distance, turned with the body
    net_anchor_enc(&b[1], wp, 0.1f, off, &oy);
    CHECK(oy >= 0 && oy < NET_TWO_PI);                       // negative relative yaw wraps
    // receiver body choice: a missing party member falls back to the remote Sora, world = none
    NetBody rb[3];
    memcpy(rb, b, sizeof rb);
    CHECK(net_anchor_body(rb, NANC_WORLD) == NULL && net_anchor_body(rb, NANC_SORA) == &rb[0]);
    CHECK(net_anchor_body(rb, NANC_DONALD) == &rb[1] && net_anchor_body(rb, NANC_GOOFY) == &rb[0]);
    // sender: a moving fx is queued only when it changed by > 1 cm / ~1 degree, then rides NET_EVENT_REPEAT picks
    NetFxOut t[4];
    memset(t, 0, sizeof t);
    t[0].on = true, t[0].ev = 7;
    float bp[3] = {1, 2, 3}, bpn[3] = {1.0005f, 2, 3}, bpf[3] = {1.02f, 2, 3};
    net_fxo_base(&t[0], NANC_SORA, bp, 0.5f);
    CHECK(net_fxo_place(&t[0], NANC_SORA, bp, 0.5f, 0.01f, 0.0175f, false) && t[0].moved);  // (merge fix) the first move always goes
    t[0].dirty = false, t[0].mrep = 0;                                                       // (as if sent)
    CHECK(!net_fxo_place(&t[0], NANC_SORA, bpn, 0.5f, 0.01f, 0.0175f, false) && !t[0].dirty);   // 0.05 cm: skipped
    CHECK(!net_fxo_place(&t[0], NANC_SORA, bp, 0.5f + 0.01f, 0.01f, 0.0175f, false));           // 0.6 deg: skipped
    CHECK(net_fxo_place(&t[0], NANC_SORA, bp, 0.5f + 0.03f, 0.01f, 0.0175f, false));            // 1.7 deg: queued
    NetFxMove out[NET_MAX_MOVES];
    int rr = 0;
    for (int i = 0; i < NET_EVENT_REPEAT; i++) {
        CHECK(net_pick_moves(t, 4, &rr, out, NET_MAX_MOVES) == 1 && out[0].ev == 7 && out[0].anchor == NANC_SORA && out[0].flags == 0);
        CHECK(NEAR(out[0].yaw, 0.53, 1e-5));
    }
    CHECK(!t[0].dirty && net_pick_moves(t, 4, &rr, out, NET_MAX_MOVES) == 0 && t[0].on);       // then quiet
    CHECK(net_fxo_place(&t[0], NANC_SORA, bpf, 0.53f, 0.01f, 0.0175f, false));                  // 2 cm: queued again
    CHECK(net_pick_moves(t, 4, &rr, out, NET_MAX_MOVES) == 1);
    CHECK(net_fxo_place(&t[0], NANC_SORA, bpf, 0.53f, 0.01f, 0.0175f, false) == false);          // the same place: skipped
    CHECK(net_fxo_place(&t[0], NANC_WORLD, bpf, 0.53f, 0.01f, 0.0175f, false) && t[0].anchor == NANC_WORLD);  // anchor change alone
    CHECK(net_pick_moves(t, 4, &rr, out, NET_MAX_MOVES) == 1 && out[0].anchor == NANC_WORLD);
    t[0].dirty = false, t[0].mrep = 0;
    CHECK(net_fxo_place(&t[0], NANC_WORLD, bpf, 0.53f, 0.01f, 0.0175f, true));                  // netanchor 0: always, one send
    CHECK(net_pick_moves(t, 4, &rr, out, NET_MAX_MOVES) == 1 && net_pick_moves(t, 4, &rr, out, NET_MAX_MOVES) == 0);
    // yaw wrap: 6.28 -> 0.01 is 0.013 rad apart, not queued
    t[1] = (NetFxOut){.on = true, .ev = 8, .moved = true};  // (a later move: its first one already went)
    net_fxo_base(&t[1], NANC_WORLD, bp, 6.28f);
    CHECK(!net_fxo_place(&t[1], NANC_WORLD, bp, 0.01f, 0.01f, 0.0175f, false));
    // a kill rides its repeats and carries no anchor
    t[1].dirty = true, t[1].mrep = 2, t[1].anchor = NANC_GOOFY, t[1].rep = NET_EVENT_REPEAT, t[1].flags = NMV_KILL;
    CHECK(net_pick_moves(t, 4, &rr, out, NET_MAX_MOVES) == 1 && out[0].flags == NMV_KILL && out[0].anchor == NANC_WORLD);
    // receiver: an anchor change resets the track
    FxTrack tr;
    memset(&tr, 0, sizeof tr);
    float a[3] = {1, 0, 0}, c[3] = {9, 0, 0}, sp[3];
    float sy;
    fxtrack_push_anc(&tr, NANC_SORA, 100, a, 0), fxtrack_push_anc(&tr, NANC_SORA, 150, c, 0);
    CHECK(tr.n == 2 && tr.anc == NANC_SORA && fxtrack_sample(&tr, 125, sp, &sy) && NEAR(sp[0], 5, 1e-4));
    fxtrack_push_anc(&tr, NANC_DONALD, 200, a, 1);
    CHECK(tr.n == 1 && tr.anc == NANC_DONALD && !fxtrack_sample(&tr, 150, sp, &sy) && fxtrack_sample(&tr, 200, sp, &sy) && NEAR(sp[0], 1, 1e-4));
    fxtrack_push_anc(&tr, NANC_DONALD, 250, c, 1);
    CHECK(tr.n == 2);                                        // same anchor: appended
}

// (slice 3 unit A) sender record: only members drawn THIS frame, flags / floor_dy / blend / shield
static void test_pcap_record(void) {
    NetPartyMember pm[NET_PARTY_N];
    float feet[3] = {1, 10, 3}, f[3], face;
    pcap_begin();
    CHECK(pcap_fill(pm) == 0 && !pcap_get(0, f, &face));
    pcap_commit(1, feet, 0.5f, 700, 12.5f, 8.0f, true, true, 650, 30.0f, 0.25f, NULL);
    CHECK(pcap_fill(pm) == 1 && pm[0].who == 1 && pm[0].slot == 700 && NEAR(pm[0].floor_dy, -2.0, 1e-6));
    CHECK(pm[0].flags == (NPM_GHOST | NPM_WEAPON | NPM_BLEND) && pm[0].from_slot == 650 && NEAR(pm[0].blend, 0.25, 1e-6));
    CHECK(pcap_get(1, f, &face) && f[1] == 10 && face == 0.5f && !pcap_get(0, f, &face));
    pcap_weapon(0, false);
    float sh[5] = {4, 5, 6, 1.5f, 7.0f};
    pcap_commit(0, feet, 1.0f, 100, 0, 10.0f, false, false, 0, 0, 0, sh);
    CHECK(pcap_fill(pm) == 2 && pm[0].who == 0 && pm[0].flags == NPM_SHIELD && pm[0].shield[2] == 6 && pm[0].shield_t == 7.0f);
    pcap_begin();  // next frame: nothing drawn = absent (absorbed / hidden)
    CHECK(pcap_fill(pm) == 0 && pcap_get(1, f, &face));  // (merge fix: the anchors still see last frame's draw)
    pcap_begin();                                         // 2 frames not drawn: gone for the anchors too
    CHECK(pcap_fill(pm) == 0 && !pcap_get(1, f, &face));
    pcap_weapon(0, true);
}

static NetSnap psnap(int seq, float x, int who, int flags) {
    NetSnap s;
    memset(&s, 0, sizeof s);
    s.player = 5, s.seq = (uint16_t)seq;
    if (who >= 0) {
        s.npm = 1, s.pm[0].who = (uint8_t)who, s.pm[0].flags = (uint8_t)flags, s.pm[0].slot = 300, s.pm[0].clock = 10;
        s.pm[0].feet[0] = x, s.pm[0].face = 0.1f, s.pm[0].floor_dy = -1.0f;
    }
    return s;
}

static void test_party_sample(void) {
    NetPeer p; memset(&p, 0, sizeof p);
    peer_on_hello(&p, 5, "Deck", 1000);
    NetSnap a = psnap(1, 0, 0, NPM_WEAPON), b = psnap(2, 100, 0, NPM_WEAPON), c = psnap(3, 200, -1, 0);
    CHECK(peer_on_snap(&p, &a, 1000) && peer_on_snap(&p, &b, 1100));
    PartyPose q = peer_party_sample(&p, 0, 1170);  // render 1050: halfway a -> b
    CHECK(q.valid && NEAR(q.feet[0], 50, 0.01) && NEAR(q.clock, 10 + 0.05 * 60, 0.01) && NEAR(q.floor_dy, -1, 1e-6));
    CHECK(!peer_party_sample(&p, 1, 1170).valid);  // Goofy is not in the SNAPs: not shown
    q = peer_party_sample(&p, 0, 5000);  // after the newest: hold it
    CHECK(q.valid && NEAR(q.feet[0], 100, 0.001));
    CHECK(peer_on_snap(&p, &c, 1200));  // newest SNAP without him (absorbed)
    q = peer_party_sample(&p, 0, 1270);  // render 1150: bracket b -> c, b has him, c not: held at b, not lerped
    CHECK(q.valid && NEAR(q.feet[0], 100, 0.001));
    CHECK(!peer_party_sample(&p, 0, 1400).valid);  // render 1280 >= c's arrival: gone
    q = peer_party_sample(&p, 0, 1000);  // before the first SNAP: the oldest
    CHECK(q.valid && NEAR(q.feet[0], 0, 0.001));
}

static void test_party_sample_arc_blend_shield(void) {
    NetPeer p; memset(&p, 0, sizeof p);
    peer_on_hello(&p, 5, "Deck", 0);
    NetSnap a = psnap(1, 0, 1, NPM_BLEND | NPM_SHIELD), b = psnap(2, 10, 1, NPM_BLEND | NPM_SHIELD);
    a.pm[0].face = 6.2f, b.pm[0].face = 0.2f;  // shortest arc across 2 pi
    a.pm[0].from_slot = b.pm[0].from_slot = 99, a.pm[0].blend = 0.2f, b.pm[0].blend = 0.6f;
    a.pm[0].shield[0] = 0, b.pm[0].shield[0] = 20, a.pm[0].shield_t = 5;
    CHECK(peer_on_snap(&p, &a, 0) && peer_on_snap(&p, &b, 100));
    PartyPose q = peer_party_sample(&p, 1, 170);  // render 50
    CHECK(q.valid && (q.flags & NPM_BLEND) && NEAR(q.blend, 0.4, 1e-5) && NEAR(q.shield[0], 10, 1e-4));
    CHECK(NEAR(q.shield_t, 5 + 0.05 * 60, 1e-4));
    CHECK(ang_diff(q.face, 0.0584) < 1e-3);  // halfway between 6.2 and 0.2 across 0: 6.2 + 0.1416 = 0.0584
}


// ---- (mp4) protocol v5 + the peer table ----
static void test_v5_worst_snap(void) {
    CHECK(NET_VERSION == 11 && NET_MAX_EVENTS == 12 && NET_SNAP_WORST == 450 && NET_SNAP_WORST <= NET_PKT_MAX);
    NetSnap s = sample_snap(NET_MAX_EVENTS), r;
    s.nmv = NET_MAX_MOVES;
    for (int i = 0; i < NET_MAX_MOVES; i++) s.mv[i].ev = (uint8_t)(10 + i);
    s.npm = NET_PARTY_N;
    for (int i = 0; i < NET_PARTY_N; i++) s.pm[i].who = (uint8_t)i, s.pm[i].flags = NPM_SHIELD, s.pm[i].feet[0] = 5.0f + i, s.pm[i].shield[1] = 2.0f;
    uint8_t buf[NET_PKT_MAX];
    int n = net_pack_snap(&s, buf, sizeof buf), type = 0; uint8_t player = 0; uint16_t seq = 0; char name[NET_NAME_MAX + 1];
    CHECK(n == 450);
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name) && r.nev == 12 && r.nmv == 6 && r.npm == 2);
    CHECK(r.ev[11].seq == 211 && r.ev[11].id == 311 && r.pm[1].feet[0] == 6.0f);
    s.nev = NET_MAX_EVENTS + 1;
    CHECK(net_pack_snap(&s, buf, sizeof buf) == -1);
    s.nev = 12;
    n = net_pack_snap(&s, buf, sizeof buf);
    buf[7 + 25] = 13;  // a packet claiming 13 events
    CHECK(!net_unpack(buf, n, &type, &player, &seq, &r, name));
}
static void test_v4_is_other_version(void) {
    uint8_t buf[NET_PKT_MAX];
    int type; uint8_t player; uint16_t seq; char name[NET_NAME_MAX + 1];
    int n = net_pack_hello(9, 1, "Deck", buf, sizeof buf);
    buf[2] = 4;
    CHECK(!net_unpack(buf, n, &type, &player, &seq, NULL, name) && net_other_version(buf, n) == 4);
    // a v4-shaped HELLO body (no peer count byte) under a v5 header is bad
    n = net_pack_hello(9, 1, "Deck", buf, sizeof buf);
    CHECK(!net_unpack(buf, n - 1, &type, &player, &seq, NULL, name));
}
static void test_hello_peer_list(void) {
    uint8_t buf[NET_PKT_MAX];
    int type, npa = -1; uint8_t player; uint16_t seq; char name[NET_NAME_MAX + 1];
    NetAddr pa[NET_HELLO_PEERS], in[4];
    for (int i = 0; i < 4; i++) in[i].ip = 0x0100007Fu + (uint32_t)i * 0x01000000u, in[i].port = (uint16_t)(7790 + i);
    int n = net_pack_hello_peers(9, 5, "Kairi", in, 3, buf, sizeof buf);
    CHECK(n == NET_HDR + 1 + 5 + 1 + 3 * NET_ADDR_SIZE);
    CHECK(net_unpack_ex(buf, n, &type, &player, &seq, NULL, name, pa, &npa) && type == NET_HELLO && npa == 3 && !strcmp(name, "Kairi"));
    for (int i = 0; i < 3; i++) CHECK(pa[i].ip == in[i].ip && pa[i].port == in[i].port);
    n = net_pack_hello_peers(9, 5, "T", in, 4, buf, sizeof buf);  // more than 3 are cut
    CHECK(net_unpack_ex(buf, n, &type, &player, &seq, NULL, name, pa, &npa) && npa == 3);
    n = net_pack_hello(9, 5, "T", buf, sizeof buf);  // no list
    CHECK(net_unpack_ex(buf, n, &type, &player, &seq, NULL, name, pa, &npa) && npa == 0 && net_unpack(buf, n, &type, &player, &seq, NULL, name));
    n = net_pack_hello_peers(9, 5, "T", in, 2, buf, sizeof buf);
    CHECK(net_unpack(buf, n, &type, &player, &seq, NULL, name));  // the old entry point still reads it (list ignored)
    CHECK(!net_unpack(buf, n - 1, &type, &player, &seq, NULL, name));  // cut
    buf[NET_HDR + 2] = 4;  // claims 4 entries
    CHECK(!net_unpack(buf, n, &type, &player, &seq, NULL, name));
    NetAddr bad[2] = {{0, 7790}, {0x0100007Fu, 0}};  // ip 0 / port 0 entries are dropped
    n = net_pack_hello_peers(9, 5, "T", bad, 2, buf, sizeof buf);
    CHECK(net_unpack_ex(buf, n, &type, &player, &seq, NULL, name, pa, &npa) && npa == 0);
}

static NetPeerSlot g_tab[NET_MAX_PEERS];
static void test_peer_table(void) {
    memset(g_tab, 0, sizeof g_tab);
    const uint32_t A = 0x0100007Fu, B = 0x0200007Fu, C = 0x0300007Fu, D = 0x0400007Fu;
    (void)B, (void)C, (void)D;
    int a = netslot_join(g_tab, NET_MAX_PEERS, A, 7790, 11, "A", 100);
    CHECK(a == 0 && g_tab[0].used && !strcmp(g_tab[0].p.name, "A") && g_tab[0].p.player == 11);
    NetSnap s;
    memset(&s, 0, sizeof s), s.player = 11, s.seq = 1;
    CHECK(peer_on_snap(&g_tab[0].p, &s, 110));
    CHECK(netslot_find(g_tab, NET_MAX_PEERS, A, 7790, 11) == 0);
    CHECK(netslot_find(g_tab, NET_MAX_PEERS, B, 7790, 11) == -1);       // another ip with his id = a stranger
    CHECK(netslot_find(g_tab, NET_MAX_PEERS, A, 7795, 11) == 0);        // same ip + id, port changed = same slot
    CHECK(netslot_find(g_tab, NET_MAX_PEERS, A, 7795, 12) == -1);       // same ip, another port + id = new peer
    CHECK(netslot_find(g_tab, NET_MAX_PEERS, A, 7790, -1) == 0);
#ifndef __linux__
    CHECK(NET_MAX_PEERS == 3);
    CHECK(netslot_join(g_tab, NET_MAX_PEERS, B, 7790, 12, "B", 120) == 1);
    CHECK(netslot_join(g_tab, NET_MAX_PEERS, C, 7790, 13, "C", 130) == 2);
    CHECK(netslot_count(g_tab, NET_MAX_PEERS) == 3);
    CHECK(netslot_join(g_tab, NET_MAX_PEERS, D, 7790, 14, "D", 140) == -1);  // full
    // B leaves: his slot is free, the next newcomer takes it, wiped
    g_tab[1].used = false;
    g_tab[1].ninq = 5, g_tab[1].rfx[3].h1 = 4;
    CHECK(netslot_count(g_tab, NET_MAX_PEERS) == 2);
    CHECK(netslot_join(g_tab, NET_MAX_PEERS, D, 7790, 14, "D", 150) == 1);
    CHECK(g_tab[1].ninq == 0 && g_tab[1].rfx[3].h1 == 0 && !strcmp(g_tab[1].p.name, "D") && g_tab[1].p.player == 14);
    // two peers with the same id from different addresses are two slots (the table never merges by id alone)
    g_tab[2].used = false;
    CHECK(netslot_join(g_tab, NET_MAX_PEERS, B, 7790, 11, "B2", 160) == 2);
    CHECK(netslot_find(g_tab, NET_MAX_PEERS, B, 7790, 11) == 2 && netslot_find(g_tab, NET_MAX_PEERS, A, 7790, 11) == 0);
    // per-peer event freshness: the same event seq is fresh once for EACH peer
    NetSnap e1, e2;
    memset(&e1, 0, sizeof e1), memset(&e2, 0, sizeof e2);
    e1.player = 11, e1.nev = 1, e1.ev[0].seq = 7;
    e2.player = 11, e2.nev = 1, e2.ev[0].seq = 7;
    NetEvent got[NET_MAX_EVENTS];
    CHECK(peer_take_events(&g_tab[0].p, &e1, got) == 1);
    CHECK(peer_take_events(&g_tab[2].p, &e2, got) == 1);
    CHECK(peer_take_events(&g_tab[0].p, &e1, got) == 0 && peer_take_events(&g_tab[2].p, &e2, got) == 0);
    // 12 events in one SNAP all come out
    NetSnap big;
    memset(&big, 0, sizeof big), big.nev = 12;
    for (int i = 0; i < 12; i++) big.ev[i].seq = (uint8_t)(20 + i);
    CHECK(peer_take_events(&g_tab[1].p, &big, got) == 12);
    // timeout of one slot leaves the others
    peer_on_hello(&g_tab[0].p, 11, "A", 1000), peer_on_hello(&g_tab[1].p, 14, "D", 1000);
    peer_tick(&g_tab[0].p, 1000 + NET_DROP_MS + 1);
    CHECK(g_tab[0].p.st == PEER_NONE && g_tab[1].p.st == PEER_LIVE);
#endif
}

// the evq throughput: 60 queued events at 12 per SNAP, each repeated 3 SNAPs: every event goes out, none lost
static void evq_snap(int *ev, uint8_t *rep, int *n, int *sent_once) {
    int out = 0, keep = 0;
    for (int i = 0; i < *n && out < NET_MAX_EVENTS; i++) out++, rep[i]--, sent_once[ev[i]] = 1;
    for (int i = 0; i < *n; i++) if (rep[i]) ev[keep] = ev[i], rep[keep++] = rep[i];
    *n = keep;
}
static void test_evq_throughput(void) {
    uint8_t rep[64];
    int ev[64], n = 0, sent_once[256], lost = 0;
    memset(sent_once, 0, sizeof sent_once);
    for (int burst = 0; burst < 3; burst++) {  // 3 bursts of 20 events, one per 100 ms (2 SNAPs apart)
        for (int i = 0; i < 20; i++) {
            if (n == 64) lost++;
            else ev[n] = burst * 20 + i, rep[n++] = NET_EVENT_REPEAT;
        }
        for (int snap = 0; snap < 2; snap++) evq_snap(ev, rep, &n, sent_once);
    }
    for (int snap = 0; snap < 40 && n; snap++) evq_snap(ev, rep, &n, sent_once);
    int got = 0;
    for (int i = 0; i < 60; i++) got += sent_once[i];
    CHECK(lost == 0 && got == 60 && n == 0);
}

static void test_outfit_byte(void) {
    NetSnap s = sample_snap(1), r;
    uint8_t buf[NET_PKT_MAX];
    int type; uint8_t player; uint16_t seq; char name[NET_NAME_MAX + 1];
    for (int o = 0; o < NET_OUTFITS; o++) {
        s.outfit = (uint8_t)o;
        int n = net_pack_snap(&s, buf, sizeof buf);
        CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name) && r.outfit == o && r.ev[0].seq == 200);  // (events still where they were)
    }
    s.outfit = 3;
    int n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(buf[NET_HDR + 28] == 3);
    buf[NET_HDR + 28] = NET_OUTFITS;  // out of range on the wire = Normal
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name) && r.outfit == 0);
    buf[NET_HDR + 28] = 255;
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name) && r.outfit == 0);
    s.outfit = 99;  // a bad sender value packs as 0
    n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(buf[NET_HDR + 28] == 0);
    {   // the sampled pose carries it
        NetPeer p; memset(&p, 0, sizeof p); peer_on_hello(&p, 42, "X", 0);
        s.outfit = 2; s.seq = 1; peer_on_snap(&p, &s, 100);
        CHECK(peer_sample(&p, 1000).outfit == 2);
    }
}

// ---- (s166 v6) NET_EDMG: shared enemy HP ----
static void test_edmg_codec(void) {
    CHECK(NET_VERSION == 11 && NET_EDMG == 4 && NET_EDMG_ENT == 12 && NET_EDMG_MAX == 16 && NET_EDMG_PKT == 199 && NET_EDMG_PKT <= NET_PKT_MAX);
    NetEdEnt e[NET_EDMG_MAX], r[NET_EDMG_MAX];
    for (int i = 0; i < NET_EDMG_MAX; i++)
        e[i].hid = 0x80000000u + 0x01020304u * (uint32_t)i + 1, e[i].dealt = (uint16_t)(1 + 4000 * i), e[i].after = (uint16_t)(65535 - i),
        e[i].f54 = (uint8_t)(15 + i), e[i].f40 = (uint8_t)(i & 3), e[i].flags = (uint8_t)(i == 15 ? NEDF_KILL : 0);
    uint8_t buf[NET_PKT_MAX], pl = 0;
    uint16_t sq = 0;
    int n = net_pack_edmg(9, 0xFFFE, e, 16, buf, sizeof buf);
    CHECK(n == 199 && buf[0] == 'S' && buf[1] == 'N' && buf[2] == 11 && buf[3] == NET_EDMG && buf[4] == 9);
    CHECK(net_unpack_edmg(buf, n, &pl, &sq, r) == 16 && pl == 9 && sq == 0xFFFE);
    for (int i = 0; i < 16; i++) CHECK(r[i].hid == e[i].hid && r[i].dealt == e[i].dealt && r[i].after == e[i].after && r[i].f54 == e[i].f54 && r[i].f40 == e[i].f40 && r[i].flags == e[i].flags);
    // little endian on the wire
    NetEdEnt one = {0x11223344, 0x0102, 0x0304, 5, 6, NEDF_KILL};
    n = net_pack_edmg(1, 2, &one, 1, buf, sizeof buf);
    CHECK(n == NET_HDR + 12 && buf[7] == 0x44 && buf[8] == 0x33 && buf[9] == 0x22 && buf[10] == 0x11 && buf[11] == 2 && buf[12] == 1 && buf[13] == 4 && buf[14] == 3 &&
          buf[15] == 5 && buf[16] == 6 && buf[17] == NEDF_KILL && buf[18] == 0);
    // goes through the general unpacker as a valid type (size checked there), seq = the header's
    int type = 0; uint8_t player = 0; uint16_t seq = 0; char name[NET_NAME_MAX + 1];
    CHECK(net_unpack(buf, n, &type, &player, &seq, NULL, name) && type == NET_EDMG && player == 1 && seq == 2);
    // bad sizes / magic / version / type
    CHECK(!net_unpack(buf, n - 1, &type, &player, &seq, NULL, name) && !net_unpack(buf, NET_HDR, &type, &player, &seq, NULL, name));
    CHECK(net_unpack_edmg(buf, n - 1, NULL, NULL, r) == 0 && net_unpack_edmg(buf, NET_HDR, NULL, NULL, r) == 0);
    uint8_t big[NET_HDR + 17 * 12];
    memset(big, 0, sizeof big);
    memcpy(big, buf, NET_HDR);
    CHECK(net_unpack_edmg(big, sizeof big, NULL, NULL, r) == 0 && !net_unpack(big, sizeof big, &type, &player, &seq, NULL, name));  // 17 entries: too many
    uint8_t bad[32];
    memcpy(bad, buf, (size_t)n), bad[2] = 5;
    CHECK(net_unpack_edmg(bad, n, NULL, NULL, r) == 0 && net_other_version(bad, n) == 5);
    memcpy(bad, buf, (size_t)n), bad[3] = NET_SNAP;
    CHECK(net_unpack_edmg(bad, n, NULL, NULL, r) == 0);
    memcpy(bad, buf, (size_t)n), bad[0] = 'X';
    CHECK(net_unpack_edmg(bad, n, NULL, NULL, r) == 0);
    // hid 0 on the wire is dropped; pack refuses 0 / 17 entries
    NetEdEnt two[2] = {{0, 1, 1, 15, 0, 0}, {7, 2, 0, 15, 0, 1}};
    n = net_pack_edmg(1, 3, two, 2, buf, sizeof buf);
    CHECK(net_unpack_edmg(buf, n, NULL, NULL, r) == 1 && r[0].hid == 7);
    CHECK(net_pack_edmg(1, 3, two, 0, buf, sizeof buf) == -1 && net_pack_edmg(1, 3, two, 17, buf, sizeof buf) == -1 && net_pack_edmg(1, 3, two, 2, buf, 30) == -1);
}
// ---- (s176 v10) NET_FIRE: fire sync ----
static void test_fire_codec(void) {
    CHECK(NET_VERSION == 11 && NET_FIRE == 6 && NET_FIRE_ENT == 18 && NET_FIRE_MAX == 4 && NET_FIRE_PKT == 79 && NET_FIRE_PKT <= NET_PKT_MAX);
    NetFireEnt e[NET_FIRE_MAX], r[NET_FIRE_MAX];
    for (int i = 0; i < NET_FIRE_MAX; i++)
        e[i].pos[0] = 100.5f + (float)i, e[i].pos[1] = -3.25f, e[i].pos[2] = -2000.0f - (float)i, e[i].r = (uint16_t)(260 + i), e[i].h = (uint16_t)(130 + i),
        e[i].flags = (uint8_t)((i & 1 ? NFRF_LAST : 0) | (i % 3) << NFRF_WHO_SHIFT);
    uint8_t buf[NET_PKT_MAX], pl = 0;
    uint16_t sq = 0;
    int n = net_pack_fire(7, 4242, e, 4, buf, sizeof buf), type = 0;
    CHECK(n == 79 && buf[0] == 'S' && buf[1] == 'N' && buf[2] == 11 && buf[3] == NET_FIRE && buf[4] == 7);
    CHECK(net_unpack_fire(buf, n, &pl, &sq, r) == 4 && pl == 7 && sq == 4242);
    for (int i = 0; i < 4; i++) CHECK(!memcmp(&r[i].pos, &e[i].pos, 12) && r[i].r == e[i].r && r[i].h == e[i].h && r[i].flags == e[i].flags);
    char name[NET_NAME_MAX + 1];
    CHECK(net_unpack(buf, n, &type, &pl, &sq, NULL, name) && type == NET_FIRE && pl == 7 && sq == 4242);
    // 1 entry is valid, the other lengths are not
    n = net_pack_fire(7, 1, e, 1, buf, sizeof buf);
    CHECK(n == 25 && net_unpack_fire(buf, n, NULL, NULL, r) == 1);
    CHECK(!net_unpack_fire(buf, n - 1, NULL, NULL, r) && !net_unpack_fire(buf, NET_HDR, NULL, NULL, r) && !net_unpack_fire(buf, n + 1, NULL, NULL, r));
    CHECK(!net_unpack(buf, n - 1, &type, &pl, &sq, NULL, name) && !net_unpack(buf, NET_HDR + 5 * NET_FIRE_ENT, &type, &pl, &sq, NULL, name));
    // wrong type / version / magic
    uint8_t bad[NET_PKT_MAX];
    memcpy(bad, buf, (size_t)n), bad[3] = NET_EDMG;
    CHECK(net_unpack_fire(bad, n, NULL, NULL, r) == 0);
    memcpy(bad, buf, (size_t)n), bad[2] = NET_VERSION - 1;
    CHECK(net_unpack_fire(bad, n, NULL, NULL, r) == 0 && !net_unpack(bad, n, &type, &pl, &sq, NULL, name));
    memcpy(bad, buf, (size_t)n), bad[0] = 'X';
    CHECK(net_unpack_fire(bad, n, NULL, NULL, r) == 0);
    // a non-finite / huge position is dropped alone; pack refuses 0 / 5 entries and a short buffer
    NetFireEnt two[2] = {e[0], e[1]};
    two[0].pos[1] = NAN;
    n = net_pack_fire(1, 3, two, 2, buf, sizeof buf);
    CHECK(net_unpack_fire(buf, n, NULL, NULL, r) == 1 && r[0].pos[0] == e[1].pos[0]);
    two[0].pos[1] = 0, two[1].pos[2] = 5e6f;
    n = net_pack_fire(1, 3, two, 2, buf, sizeof buf);
    CHECK(net_unpack_fire(buf, n, NULL, NULL, r) == 1 && r[0].pos[0] == e[0].pos[0]);
    CHECK(net_pack_fire(1, 3, two, 0, buf, sizeof buf) == -1 && net_pack_fire(1, 3, two, 5, buf, sizeof buf) == -1 && net_pack_fire(1, 3, two, 2, buf, 30) == -1);
    // the clamp: 1/100 u, rounded, negative / NaN = 0, huge = 65535
    CHECK(net_fire_q(2.6f) == 260 && net_fire_q(0.004f) == 0 && net_fire_q(0.006f) == 1 && net_fire_q(-5) == 0 && net_fire_q(NAN) == 0);
    CHECK(net_fire_q(655.35f) == 65535 && net_fire_q(1e9f) == 65535 && net_fire_q(1000.0f) == 65535);
}

static void test_edmg_seq_dedupe(void) {
    NetEdSeen s;
    memset(&s, 0, sizeof s);
    CHECK(net_edseq_fresh(&s, 100));
    CHECK(!net_edseq_fresh(&s, 100) && !net_edseq_fresh(&s, 100));       // the repeats
    CHECK(net_edseq_fresh(&s, 101) && !net_edseq_fresh(&s, 101) && !net_edseq_fresh(&s, 100));
    CHECK(net_edseq_fresh(&s, 103));                                    // 102 skipped (lost): still fresh if it shows up late
    CHECK(net_edseq_fresh(&s, 102) && !net_edseq_fresh(&s, 102) && !net_edseq_fresh(&s, 103));
    CHECK(!net_edseq_fresh(&s, 101) && !net_edseq_fresh(&s, 100));
    // 16-seq window: 16 behind is still tracked, 17 behind is dropped
    memset(&s, 0, sizeof s);
    CHECK(net_edseq_fresh(&s, 1000) && net_edseq_fresh(&s, 1016));
    CHECK(!net_edseq_fresh(&s, 1000));                                  // 16 behind, seen
    CHECK(net_edseq_fresh(&s, 1001) && !net_edseq_fresh(&s, 1001));     // 15 behind, new then seen
    CHECK(net_edseq_fresh(&s, 1017) && !net_edseq_fresh(&s, 1000) && !net_edseq_fresh(&s, 1001) && !net_edseq_fresh(&s, 1016));
    CHECK(net_edseq_fresh(&s, 1002) && !net_edseq_fresh(&s, 1002));     // 15 behind 1017
    CHECK(!net_edseq_fresh(&s, 1000) && !net_edseq_fresh(&s, 999));     // 17 / 18 behind: too old
    // a jump of more than 16 forgets the old ones
    memset(&s, 0, sizeof s);
    CHECK(net_edseq_fresh(&s, 10) && net_edseq_fresh(&s, 5000));
    CHECK(!net_edseq_fresh(&s, 5000) && net_edseq_fresh(&s, 4990) && !net_edseq_fresh(&s, 4990));
    CHECK(!net_edseq_fresh(&s, 4744));                                  // 256 behind: still a repeat window miss (too old)
    // (s167 SF-3) far behind (> 256) = the peer restarted with a new random seq: taken, and its repeats de-duped from there
    CHECK(net_edseq_fresh(&s, 10) && !net_edseq_fresh(&s, 10) && net_edseq_fresh(&s, 11) && !net_edseq_fresh(&s, 11));
    // wrap at 65535 -> 0
    memset(&s, 0, sizeof s);
    CHECK(net_edseq_fresh(&s, 65534) && net_edseq_fresh(&s, 65535) && net_edseq_fresh(&s, 0) && net_edseq_fresh(&s, 1));
    CHECK(!net_edseq_fresh(&s, 65534) && !net_edseq_fresh(&s, 65535) && !net_edseq_fresh(&s, 0) && !net_edseq_fresh(&s, 1));
    CHECK(net_edseq_fresh(&s, 3) && net_edseq_fresh(&s, 2) && !net_edseq_fresh(&s, 2));       // 2 skipped, late
    CHECK(net_edseq_fresh(&s, 65530) && !net_edseq_fresh(&s, 65530));   // 9 behind 3 across the wrap, never seen
    CHECK(!net_edseq_fresh(&s, 65520));                                 // 19 behind: too old
    // the repeats of a stream: 3 copies each, interleaved, every seq applied exactly once across the wrap
    memset(&s, 0, sizeof s);
    int applied = 0;
    for (int i = 0; i < 40; i++) {
        uint16_t base = (uint16_t)(65520 + i);
        for (int c = 0; c < 3; c++) {
            if (net_edseq_fresh(&s, base)) applied++;
            if (i > 0 && c == 1 && net_edseq_fresh(&s, (uint16_t)(base - 1))) applied++;  // a late copy of the previous one
        }
    }
    CHECK(applied == 40);
}
static void test_edmg_out_queue(void) {
    NetEdOut q[NET_EDMG_OUTQ], o;
    int nq = 0;
    uint8_t p[NET_EDMG_PKT];
    memset(p, 0xAB, sizeof p);
    CHECK(net_edout_push(q, &nq, p, 31, 1000) == 0 && nq == 1);
    CHECK(net_edout_due(q, &nq, 1000, &o) && o.len == 31 && o.b[0] == 0xAB && o.b[30] == 0xAB);
    CHECK(!net_edout_due(q, &nq, 1000 + NET_EDMG_REP_MS - 1, &o));
    CHECK(net_edout_due(q, &nq, 1000 + NET_EDMG_REP_MS, &o) && !net_edout_due(q, &nq, 1000 + NET_EDMG_REP_MS, &o));
    CHECK(net_edout_due(q, &nq, 1000 + 2 * NET_EDMG_REP_MS, &o) && nq == 0);   // third copy: gone
    CHECK(!net_edout_due(q, &nq, 9999, &o));
    // exactly NET_EVENT_REPEAT copies of every packet, even when pushed together
    for (int i = 0; i < 3; i++) p[0] = (uint8_t)i, net_edout_push(q, &nq, p, 20, 2000);
    int got[3] = {0, 0, 0}, total = 0;
    for (uint64_t t = 2000; t < 2000 + 4 * NET_EDMG_REP_MS; t++) while (net_edout_due(q, &nq, t, &o)) got[o.b[0]]++, total++;
    CHECK(total == 9 && got[0] == 3 && got[1] == 3 && got[2] == 3 && nq == 0);
    // full queue: the oldest gives way
    for (int i = 0; i < NET_EDMG_OUTQ; i++) { p[0] = (uint8_t)(i + 1); CHECK(net_edout_push(q, &nq, p, 20, 3000) == 0); }
    p[0] = 99;
    CHECK(net_edout_push(q, &nq, p, 20, 3000) == 1 && nq == NET_EDMG_OUTQ && q[0].b[0] == 2 && q[NET_EDMG_OUTQ - 1].b[0] == 99);
}

// ---- (v8, s170) NF_BONES bone-pose block ----
static NetSnap bones_snap(void) {
    NetSnap s = sample_snap(2);
    s.flags = NF_BONEPOSE | NF_BONES;
    s.bones.slot = 777, s.bones.clock = 12.5f;
    for (int k = 0; k < NET_BONE_J; k++)
        for (int i = 0; i < 3; i++) s.bones.j[k][i] = (k * 3 + i) * 1.37f - 40.123f;
    s.bones.hu[0] = 0.6f, s.bones.hu[1] = 0.8f, s.bones.hu[2] = 0;
    s.bones.hl[0] = 0, s.bones.hl[1] = -0.28f, s.bones.hl[2] = 0.96f;
    return s;
}
static void test_bones_wire(void) {
    uint8_t buf[NET_PKT_MAX], b2[NET_PKT_MAX];
    int type; uint8_t player; uint16_t seq; char name[NET_NAME_MAX + 1];
    CHECK(NET_VERSION == 11 && NET_PKT_MAX == 768 && NET_BONE_SIZE == 162);
    NetSnap s = bones_snap(), r;
    NetSnap plain = sample_snap(2);
    int n0 = net_pack_snap(&plain, b2, sizeof b2);
    CHECK(n0 == NET_HDR + NET_SNAP_BODY + 2 * NET_EV_SIZE && b2[2] == 11);  // the v7 layout, version 9
    int n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(n == n0 + NET_BONE_SIZE);
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name) && (r.flags & NF_BONES) && r.bones.slot == 777 && r.bones.clock == 12.5f);
    bool ok = true;
    for (int k = 0; k < NET_BONE_J; k++)
        for (int i = 0; i < 3; i++) ok = ok && NEAR(r.bones.j[k][i], s.bones.j[k][i], 0.051);
    CHECK(ok);
    for (int i = 0; i < 3; i++) CHECK(NEAR(r.bones.hu[i], s.bones.hu[i], 2.0 / 32767) && NEAR(r.bones.hl[i], s.bones.hl[i], 2.0 / 32767));
    CHECK(r.nev == 2 && r.slot == 412);
    // clamp at +-3276.7 cm
    s.bones.j[0][0] = 99999.0f, s.bones.j[0][1] = -99999.0f, s.bones.j[0][2] = 3276.7f;
    s.bones.hu[0] = 5.0f, s.bones.hu[1] = -5.0f;
    n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(net_unpack(buf, n, &type, &player, &seq, &r, name));
    CHECK(NEAR(r.bones.j[0][0], 3276.7, 1e-3) && NEAR(r.bones.j[0][1], -3276.7, 1e-3) && NEAR(r.bones.j[0][2], 3276.7, 0.051));
    CHECK(NEAR(r.bones.hu[0], 1.0, 1e-6) && NEAR(r.bones.hu[1], -1.0, 1e-6));
    // length +1 / -1 rejected, with and without the block
    s = bones_snap();
    n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(!net_unpack(buf, n - 1, &type, &player, &seq, &r, name));
    buf[n] = 0;
    CHECK(!net_unpack(buf, n + 1, &type, &player, &seq, &r, name));
    CHECK(net_pack_snap(&s, buf, n - 1) == -1);
    CHECK(net_unpack(b2, n0, &type, &player, &seq, &r, name) && !(r.flags & NF_BONES));
    CHECK(!net_unpack(b2, n0 - 1, &type, &player, &seq, &r, name));
    b2[n0] = 0;
    CHECK(!net_unpack(b2, n0 + 1, &type, &player, &seq, &r, name));
    // the flag set but the block cut off = rejected
    n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(!net_unpack(buf, n - NET_BONE_SIZE, &type, &player, &seq, &r, name));
    // NF_BONES without NF_BONEPOSE = accepted, flag cleared, block ignored
    s.flags = NF_BONES;
    n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(n == n0 + NET_BONE_SIZE && net_unpack(buf, n, &type, &player, &seq, &r, name) && !(r.flags & NF_BONES) && r.bones.slot == 0 && r.nev == 2);
    // a non-finite clock = a bad packet
    s = bones_snap(); s.bones.clock = NAN;
    n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(!net_unpack(buf, n, &type, &player, &seq, &r, name));
    // worst SNAP + block
    s = sample_snap(NET_MAX_EVENTS);
    s.nmv = NET_MAX_MOVES;
    s.npm = NET_PARTY_N;
    for (int i = 0; i < NET_PARTY_N; i++) s.pm[i].who = (uint8_t)i, s.pm[i].flags = NPM_SHIELD;
    s.flags = NF_BONEPOSE | NF_BONES, s.bones = bones_snap().bones;
    n = net_pack_snap(&s, buf, sizeof buf);
    CHECK(n == 612 && n <= NET_PKT_MAX && net_unpack(buf, n, &type, &player, &seq, &r, name) && r.npm == 2 && r.bones.slot == 777);
}
static void test_bones_sample(void) {
    NetPeer p; memset(&p, 0, sizeof p);
    peer_on_hello(&p, 42, "x", 0);
    NetSnap a = bones_snap(), b = bones_snap();
    a.seq = 1; b.seq = 2;
    for (int k = 0; k < NET_BONE_J; k++) for (int i = 0; i < 3; i++) a.bones.j[k][i] = 0, b.bones.j[k][i] = 10.0f * (k + 1);
    a.bones.hu[0] = 1, a.bones.hu[1] = 0, a.bones.hu[2] = 0;
    b.bones.hu[0] = 0, b.bones.hu[1] = 1, b.bones.hu[2] = 0;
    a.bones.hl[0] = 0, a.bones.hl[1] = 0, a.bones.hl[2] = 1;
    b.bones.hl[0] = 0, b.bones.hl[1] = 0, b.bones.hl[2] = -1;  // opposite: the lerp is ~0 -> a's axis
    a.bones.clock = 10, b.bones.clock = 16;  // (s170 SF-1: the base clock is lerped between the two SNAPs)
    peer_on_snap(&p, &a, 100);
    peer_on_snap(&p, &b, 200);
    PeerPose q = peer_sample(&p, 270);  // render 150 = t 0.5
    CHECK(q.valid && q.bones_ok && q.bones.slot == 777);
    CHECK(NEAR(q.bones.j[0][0], 5.0, 1e-4) && NEAR(q.bones.j[23][2], 120.0, 1e-3));
    CHECK(NEAR(q.bones.hu[0], 0.70710678, 1e-5) && NEAR(q.bones.hu[1], 0.70710678, 1e-5));
    CHECK(NEAR(q.bones.hl[2], 1.0, 1e-6));
    CHECK(NEAR(q.bones.clock, 13.0, 1e-4));
    {   // (s170 SF-1) a held base clock (the puppet's 0) stays 0, not pushed ahead
        NetPeer ph; memset(&ph, 0, sizeof ph);
        peer_on_hello(&ph, 42, "x", 0);
        NetSnap ha = bones_snap(), hb = bones_snap();
        ha.seq = 1, hb.seq = 2, ha.bones.clock = hb.bones.clock = 0;
        peer_on_snap(&ph, &ha, 100), peer_on_snap(&ph, &hb, 200);
        PeerPose qh = peer_sample(&ph, 270);
        CHECK(qh.bones_ok && NEAR(qh.bones.clock, 0.0, 1e-6));
    }
    // different slots: no lerp across them
    b.bones.slot = 778; b.seq = 3;
    NetPeer p2; memset(&p2, 0, sizeof p2);
    peer_on_hello(&p2, 42, "x", 0);
    peer_on_snap(&p2, &a, 100);
    peer_on_snap(&p2, &b, 200);
    q = peer_sample(&p2, 270);
    CHECK(q.bones_ok && q.bones.slot == 777 && q.bones.j[0][0] == 0.0f && q.bones.hu[0] == 1.0f);
    // a without NF_BONES: bones_ok false (even when b has it)
    a.flags = NF_BONEPOSE; b.bones.slot = 777;
    NetPeer p3; memset(&p3, 0, sizeof p3);
    peer_on_hello(&p3, 42, "x", 0);
    peer_on_snap(&p3, &a, 100);
    peer_on_snap(&p3, &b, 200);
    q = peer_sample(&p3, 270);
    CHECK(q.valid && !q.bones_ok);
}

// ---- (v9, s172) NF_SKATE: the skate rider's board slot + clock ride the normal SNAP fields ----
static void test_skate_wire(void) {
    NetSnap s = sample_snap(0), r;
    s.flags = NF_SKATE | NF_AIR, s.slot = 166, s.clock = 41.5f, s.body = 7;
    uint8_t buf[NET_PKT_MAX];
    int n = net_pack_snap(&s, buf, sizeof buf);
    int type = 0; uint8_t player = 0; uint16_t seq = 0; char name[NET_NAME_MAX + 1];
    memset(&r, 0xcc, sizeof r);
    CHECK(n > 0 && net_unpack(buf, n, &type, &player, &seq, &r, name) && type == NET_SNAP);
    CHECK(r.flags == (NF_SKATE | NF_AIR) && r.slot == 166 && r.body == 7 && NEAR(r.clock, 41.5, 0.05));
    CHECK(NET_VERSION == 11 && NF_SKATE == 256 && !(NF_SKATE & (NF_BONEPOSE | NF_BONES)));
}

int main(void) {
    test_edmg_codec();
    test_edmg_seq_dedupe();
    test_edmg_out_queue();
    test_snap_roundtrip();
    test_event_scale_range();
    test_moves_roundtrip();
    test_pick_moves();
    test_fxo_kill_then_reuse();
    test_fxtrack();
    test_snap_size();
    test_reject();
    test_nonfinite_and_gain();
    test_hello_name();
    test_seq_wrap_and_reorder();
    test_event_dedupe();
    test_interp_midpoint();
    test_yaw_shortest_arc();
    test_slot_switch_resets_clock();
    test_clock_follow_fps_independent();
    test_timeouts();
    test_peer_restart();
    test_stale_snap_dropped();
    test_take_events_once();
    test_fill_flags();
    test_clock_wrap();
    test_trigger_ranges();
    test_plate_quads();
    test_label_origin();
    test_ribbon_decimation();
    test_ribbon_window_and_feet();
    test_ribbon_reset();
    test_ribbon_inactive();
    test_party_roundtrip();
    test_party_reject();
    test_anchors();
    test_anchor_logic();
    test_pcap_record();
    test_party_sample();
    test_party_sample_arc_blend_shield();
    test_v5_worst_snap();
    test_v4_is_other_version();
    test_hello_peer_list();
    test_peer_table();
    test_evq_throughput();
    test_outfit_byte();
    test_bones_wire();
    test_bones_sample();
    test_skate_wire();
    test_fire_codec();
    printf("net_test: %d/%d passed, %d fails\n", runs - fails, runs, fails);
    return fails != 0;
}

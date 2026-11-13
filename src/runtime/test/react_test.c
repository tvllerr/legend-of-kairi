// host test for the s194 netreact CORE: NET_REACT pack / unpack (runtime/sora_net_proto.h) + the decisions and queues of
// runtime/sora_net_react.h (no NET, no game). The glue (send / receive / play over real sockets) is in net_glue_test.c.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/react_test.c -o build_h/react_test.exe
#include <stdio.h>
#include <math.h>
#include "sora_net_proto.h"
#include "sora_net_react.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static NetReactEnt ent(uint32_t hid, int react, int kind) {
    float from[3] = {-1234.5f, 87.25f, 4321.0f};
    return nr_ent_make(hid, react, 125, 0, 7, net_react_what(kind), from, false);
}
static bool same(const NetReactEnt *a, const NetReactEnt *b) {
    return a->hid == b->hid && a->react == b->react && a->kind == b->kind && a->kb1 == b->kb1 && a->kb2 == b->kb2 && a->f54 == b->f54 &&
           a->flags == b->flags && memcmp(a->from, b->from, sizeof a->from) == 0;
}

static void test_round_trip(void) {
    NetReactEnt e[NET_REACT_MAX], o[NET_REACT_MAX];
    float f1[3] = {1.5f, -2.25f, 1e5f};
    e[0] = nr_ent_make(0x77d426c7u, 3, 125, 0, 7, "blade", f1, false);
    e[1] = nr_ent_make(0x06838a85u, 4, 30, 0, 0x14, "thunder", f1, true);
    e[2] = nr_ent_make(0xdeadbeefu, 11, -100, 45, 0x0f, "wisdom", f1, true);
    e[3] = nr_ent_make(1, 3, 0x7fff, 0, 0xf, "summon", f1, false);
    uint8_t b[NET_REACT_PKT + 8];
    int n = net_pack_react(42, 0x1234, e, 4, b, (int)sizeof b);
    CHECK(n == NET_HDR + 4 * NET_REACT_ENT && n == 103);
    CHECK(b[0] == 'S' && b[1] == 'N' && b[2] == NET_VERSION && b[3] == NET_REACT && b[3] == 8 && b[4] == 42 && b[5] == 0x34 && b[6] == 0x12);
    // the documented byte layout of entry 0
    const uint8_t *p = b + NET_HDR;
    CHECK(p[0] == 0xc7 && p[1] == 0x26 && p[2] == 0xd4 && p[3] == 0x77 && p[4] == 3 && p[5] == NRK_BLADE && p[6] == 125 && p[7] == 0 && p[8] == 0 && p[9] == 0 &&
          p[10] == 7 && p[11] == 0);
    float fx;
    memcpy(&fx, p + 12, 4);
    CHECK(fx == 1.5f);
    CHECK(p[NET_REACT_ENT + 11] == NRF_FORM && p[NET_REACT_ENT + 5] == NRK_THUNDER);
    CHECK(p[2 * NET_REACT_ENT + 6] == 0x9c && p[2 * NET_REACT_ENT + 7] == 0xff);  // (kb1 -100 = 0xff9c LE)
    // the header check of the general receive path takes it
    int type = 0; uint8_t pl = 0; uint16_t sq = 0;
    CHECK(net_unpack_ex(b, n, &type, &pl, &sq, NULL, NULL, NULL, NULL) && type == NET_REACT && pl == 42 && sq == 0x1234);
    int bad = -1;
    pl = 0, sq = 0;
    CHECK(net_unpack_react(b, n, &pl, &sq, o, &bad) == 4 && bad == 0 && pl == 42 && sq == 0x1234);
    for (int i = 0; i < 4; i++) CHECK(same(&e[i], &o[i]));
    CHECK(o[2].kb1 == -100 && o[2].kb2 == 45 && o[3].kb1 == 0x7fff && o[1].flags == NRF_FORM && o[2].kind == NRK_OTHER);
    // a full packet: 8 entries = 199 B (= NET_EDMG_PKT, fits the repeat queue)
    for (int i = 0; i < NET_REACT_MAX; i++) e[i] = ent((uint32_t)(100 + i), 3, NRK_BLADE);
    n = net_pack_react(1, 2, e, NET_REACT_MAX, b, (int)sizeof b);
    CHECK(n == NET_REACT_PKT && n == 199 && net_unpack_react(b, n, NULL, NULL, o, NULL) == NET_REACT_MAX && o[7].hid == 107);
    CHECK(net_unpack_ex(b, n, &type, NULL, NULL, NULL, NULL, NULL, NULL));
    // pack refuses: 0 / 9 entries, no entries, a short buffer
    CHECK(net_pack_react(1, 2, e, 0, b, (int)sizeof b) == -1 && net_pack_react(1, 2, e, NET_REACT_MAX + 1, b, (int)sizeof b) == -1);
    CHECK(net_pack_react(1, 2, NULL, 1, b, (int)sizeof b) == -1 && net_pack_react(1, 2, e, 2, b, NET_HDR + NET_REACT_ENT) == -1);
}

static void test_bad_input(void) {
    NetReactEnt e[2] = {ent(5, 3, NRK_BLADE), ent(6, 4, NRK_THUNDER)}, o[NET_REACT_MAX];
    uint8_t b[NET_REACT_PKT + 64];
    int n = net_pack_react(9, 77, e, 2, b, (int)sizeof b), type = 0, bad = 0;
    CHECK(n == 55);
    // short / long / not a whole entry / the header alone / past 8 entries: both the general check and the reader refuse it
    int lens[] = {NET_HDR, NET_HDR + 1, n - 1, n + 1, NET_HDR + NET_REACT_ENT - 1, NET_HDR + NET_REACT_ENT + 3, NET_HDR + 9 * NET_REACT_ENT};
    for (size_t i = 0; i < sizeof lens / sizeof *lens; i++) {
        uint8_t c[NET_HDR + 9 * NET_REACT_ENT + 8];
        memset(c, 0, sizeof c);
        memcpy(c, b, (size_t)(lens[i] < n ? lens[i] : n));
        CHECK(!net_unpack_ex(c, lens[i], &type, NULL, NULL, NULL, NULL, NULL, NULL));
        CHECK(net_unpack_react(c, lens[i], NULL, NULL, o, &bad) == 0);
    }
    CHECK(net_unpack_react(NULL, n, NULL, NULL, o, &bad) == 0);
    // another version / magic / type byte
    uint8_t c[64];
    memcpy(c, b, (size_t)n), c[2] = NET_VERSION + 1;
    CHECK(net_unpack_react(c, n, NULL, NULL, o, NULL) == 0 && !net_unpack_ex(c, n, &type, NULL, NULL, NULL, NULL, NULL, NULL));
    memcpy(c, b, (size_t)n), c[0] = 'X';
    CHECK(net_unpack_react(c, n, NULL, NULL, o, NULL) == 0);
    memcpy(c, b, (size_t)n), c[3] = NET_AREA;
    CHECK(net_unpack_react(c, n, NULL, NULL, o, NULL) == 0);
    // bad entries are dropped alone (the good one stays), counted in *bad
    struct { int off; uint8_t v; } pokes[] = {
        {0, 0},          // hid 0 (all four bytes below)
        {5, NRK_N},      // kind out of range
        {5, 0xff},
        {4, 1},          // react class 1: nothing ah_hit plays
        {4, 0},
        {4, 0x30},       // an unknown reaction (class 0x30)
    };
    for (size_t i = 0; i < sizeof pokes / sizeof *pokes; i++) {
        memcpy(c, b, (size_t)n);
        uint8_t *p = c + NET_HDR;
        if (pokes[i].off == 0) memset(p, 0, 4);
        else p[pokes[i].off] = pokes[i].v;
        uint16_t sq = 0;
        CHECK(net_unpack_react(c, n, NULL, &sq, o, &bad) == 1 && bad == 1 && o[0].hid == 6 && sq == 77);
        CHECK(net_unpack_ex(c, n, &type, NULL, NULL, NULL, NULL, NULL, NULL));  // (the size is right: the header check takes it)
    }
    // kb out of range (both fields); the 0x7fff default is fine
    int16_t kbs[] = {1001, -1001, 0x7ffe, (int16_t)0x8000};
    for (size_t i = 0; i < sizeof kbs / sizeof *kbs; i++)
        for (int f = 0; f < 2; f++) {
            memcpy(c, b, (size_t)n);
            net_w16(c + NET_HDR + 6 + 2 * f, (uint16_t)kbs[i]);
            CHECK(net_unpack_react(c, n, NULL, NULL, o, &bad) == 1 && bad == 1);
        }
    memcpy(c, b, (size_t)n);
    net_w16(c + NET_HDR + 6, 0x7fff), net_w16(c + NET_HDR + 8, (uint16_t)(int16_t)-1000);
    CHECK(net_unpack_react(c, n, NULL, NULL, o, &bad) == 2 && bad == 0 && o[0].kb1 == 0x7fff && o[0].kb2 == NET_REACT_KB_LO);  // (s197 S5: -1000 taken as -100)
    // NaN / inf / huge in any coordinate of `from`
    float badf[] = {NAN, -NAN, INFINITY, -INFINITY, 1e6f, -1e6f, 3e38f};
    for (size_t i = 0; i < sizeof badf / sizeof *badf; i++)
        for (int j = 0; j < 3; j++) {
            memcpy(c, b, (size_t)n);
            net_w32f(c + NET_HDR + NET_REACT_ENT + 12 + 4 * j, badf[i]);  // (entry 1)
            CHECK(net_unpack_react(c, n, NULL, NULL, o, &bad) == 1 && bad == 1 && o[0].hid == 5);
        }
    // both bad: a valid frame with no good entry = 0 (the glue counts it as a bad packet)
    memcpy(c, b, (size_t)n), memset(c + NET_HDR, 0, 4), c[NET_HDR + NET_REACT_ENT + 5] = 9;
    CHECK(net_unpack_react(c, n, NULL, NULL, o, &bad) == 0 && bad == 2);
    // unknown flag bits are ignored (a later build may add some); f54 is any byte
    memcpy(c, b, (size_t)n), c[NET_HDR + 11] = 0xfe, c[NET_HDR + 10] = 0xff;
    CHECK(net_unpack_react(c, n, NULL, NULL, o, &bad) == 2 && o[0].flags == 0xfe && !(o[0].flags & NRF_FORM) && o[0].f54 == 0xff);
}

static void test_make_and_kinds(void) {
    float f[3] = {1, 2, 3};
    CHECK(net_react_kind("blade") == NRK_BLADE && net_react_kind("thunder") == NRK_THUNDER && net_react_kind("fire") == NRK_OTHER);
    CHECK(net_react_kind("wisdom") == NRK_OTHER && net_react_kind("limit") == NRK_OTHER && net_react_kind(NULL) == NRK_OTHER && net_react_kind("Blade") == NRK_OTHER);
    CHECK(!strcmp(net_react_what(NRK_BLADE), "blade") && !strcmp(net_react_what(NRK_THUNDER), "thunder") && !strcmp(net_react_what(NRK_OTHER), "other"));
    CHECK(!strcmp(net_react_what(99), "other"));
    NetReactEnt e = nr_ent_make(7, 0x104, 5000, -5000, 300, "thunder", f, true);  // (out of range: clamped to what the wire carries)
    CHECK(e.react == 4 && e.kb1 == NET_REACT_KB_HI && e.kb2 == NET_REACT_KB_LO && e.f54 == 255 && e.flags == NRF_FORM && e.kind == NRK_THUNDER);
    e = nr_ent_make(7, 3, 0x7fff, 0, -4, "blade", f, false);
    CHECK(e.kb1 == 0x7fff && e.f54 == 0 && e.flags == 0 && e.from[2] == 3 && net_react_ent_ok(&e));
    // the class table = KH2 re:0x3CE980 (the same one ah_class uses)
    int c3[] = {3, 6, 8, 10, 0xd, 0x10, 0x12}, c4[] = {4, 7, 9, 0xb, 0xf, 0x11};
    for (size_t i = 0; i < 7; i++) CHECK(net_react_class(c3[i]) == 3 && net_react_plays(c3[i]));
    for (size_t i = 0; i < 6; i++) CHECK(net_react_class(c4[i]) == 4 && net_react_plays(c4[i]));
    CHECK(net_react_class(1) == 1 && net_react_class(0xc) == 1 && net_react_class(5) == 5 && net_react_class(19) == 19 && net_react_class(0x103) == 3);
    CHECK(!net_react_plays(0) && !net_react_plays(1) && !net_react_plays(5) && !net_react_plays(19) && net_react_plays(17));
}

// the receiver's de-dupe = net_edseq_fresh per peer: the 3 UDP copies once, late copies inside the window refused, a wrap through 0
static void test_dedupe(void) {
    NetEdSeen s;
    memset(&s, 0, sizeof s);
    int taken = 0;
    for (int pk = 0; pk < 5; pk++)
        for (int r = 0; r < NET_EVENT_REPEAT; r++) taken += net_edseq_fresh(&s, (uint16_t)(1000 + pk));
    CHECK(taken == 5);
    // interleaved repeats of two packets (repeat timers 40 ms apart)
    taken = 0;
    uint16_t order[] = {2000, 2001, 2000, 2001, 2002, 2000, 2001, 2002, 2002};
    for (size_t i = 0; i < sizeof order / sizeof *order; i++) taken += net_edseq_fresh(&s, order[i]);
    CHECK(taken == 3);
    // a reordered older packet that never came is still taken once; one 17 behind is too old
    CHECK(net_edseq_fresh(&s, 2010) && net_edseq_fresh(&s, 2005) && !net_edseq_fresh(&s, 2005) && !net_edseq_fresh(&s, 1993));
    // the u16 wrap: 65534 65535 0 1, with their repeats
    memset(&s, 0, sizeof s);
    taken = 0;
    for (int pk = 0; pk < 4; pk++)
        for (int r = 0; r < NET_EVENT_REPEAT; r++) taken += net_edseq_fresh(&s, (uint16_t)(65534 + pk));
    CHECK(taken == 4 && s.last == 1);
    CHECK(!net_edseq_fresh(&s, 65535) && !net_edseq_fresh(&s, 0) && net_edseq_fresh(&s, 2));
    // a restarted sender (seq far behind) is a new base, not a run of "dups"
    CHECK(net_edseq_fresh(&s, 40000) && net_edseq_fresh(&s, 40001));
}

static void test_decisions(void) {
    // the puppet test: loaded here, someone else owns it, its brain frozen by us
    CHECK(nr_puppet(true, true, true, 9, 3, 0));
    CHECK(!nr_puppet(false, true, true, 9, 3, 0));   // esync off
    CHECK(!nr_puppet(true, false, true, 9, 3, 0));   // not in the table (hid 0 / spawned / unknown)
    CHECK(!nr_puppet(true, true, false, 9, 3, 0));   // not loaded here this step
    CHECK(!nr_puppet(true, true, true, 3, 3, 0));    // we own it
    CHECK(!nr_puppet(true, true, true, -1, 3, 0));   // nobody owns it yet
    CHECK(!nr_puppet(true, true, true, 9, 3, 1));    // owner quiet: its brain runs here again
    CHECK(!nr_puppet(true, true, true, 9, 3, -1));
    // attacker + puppet = send, no local; attacker + own = local, no send; netreact 0 = local always
    CHECK(nr_hit_decide(true, true, 3) == NRH_SEND && nr_hit_decide(true, true, 4) == NRH_SEND && nr_hit_decide(true, true, 0x11) == NRH_SEND);
    CHECK(nr_hit_decide(true, false, 3) == NRH_LOCAL && nr_hit_decide(true, false, 1) == NRH_LOCAL);
    CHECK(nr_hit_decide(false, true, 3) == NRH_LOCAL && nr_hit_decide(false, true, 1) == NRH_LOCAL && nr_hit_decide(false, false, 4) == NRH_LOCAL);
    CHECK(nr_hit_decide(true, true, 1) == NRH_SKIP && nr_hit_decide(true, true, 5) == NRH_SKIP);  // (a puppet, nothing to play: no local either)
    // owner here
    CHECK(nr_owner_here(true, true, true, 3, 3));
    CHECK(!nr_owner_here(false, true, true, 3, 3) && !nr_owner_here(true, false, true, 3, 3) && !nr_owner_here(true, true, false, 3, 3));
    CHECK(!nr_owner_here(true, true, true, 9, 3) && !nr_owner_here(true, true, true, -1, 3));
    // receive: off / unknown peer / hidden peer / queued
    CHECK(nr_rx_decide(true, true, false) == NRR_QUEUE);
    CHECK(nr_rx_decide(false, true, false) == NRR_OFF && nr_rx_decide(false, false, true) == NRR_OFF);
    CHECK(nr_rx_decide(true, false, false) == NRR_UNKNOWN && nr_rx_decide(true, false, true) == NRR_UNKNOWN);
    CHECK(nr_rx_decide(true, true, true) == NRR_HIDDEN);
    // play: owner / not owner / not loaded / dead / late / off
    CHECK(nr_play_decide(true, 0, true, true, 10) == NRP_PLAY && nr_play_decide(true, NR_RX_TTL_MS, true, true, 1) == NRP_PLAY);
    CHECK(nr_play_decide(true, 0, false, true, 10) == NRP_NOT_OWNER && nr_play_decide(true, 0, false, false, 0) == NRP_NOT_OWNER);
    CHECK(nr_play_decide(true, 0, true, false, 10) == NRP_NOT_LOADED && nr_play_decide(true, 0, true, false, 0) == NRP_NOT_LOADED);
    CHECK(nr_play_decide(true, 0, true, true, 0) == NRP_DEAD && nr_play_decide(true, 0, true, true, -3) == NRP_DEAD);
    CHECK(nr_play_decide(true, NR_RX_TTL_MS + 1, true, true, 10) == NRP_LATE && nr_play_decide(false, 0, true, true, 10) == NRP_OFF);
    CHECK(!strcmp(nr_play_txt(NRP_PLAY), "played") && !strcmp(nr_play_txt(NRP_NOT_OWNER), "not owner") && !strcmp(nr_play_txt(NRP_DEAD), "dead"));
}

static void test_queues(void) {
    NrTxQ t;
    memset(&t, 0, sizeof t);
    for (int i = 0; i < NR_TXQ + 3; i++) { NetReactEnt e = ent((uint32_t)(i + 1), 3, NRK_BLADE); nr_txq_push(&t, &e); }
    CHECK(t.n == NR_TXQ && t.lost == 3 && t.q[0].hid == 4 && t.q[NR_TXQ - 1].hid == NR_TXQ + 3);
    NrRxQ r;
    memset(&r, 0, sizeof r);
    for (int i = 0; i < NR_RXQ + 2; i++) { NetReactEnt e = ent((uint32_t)(i + 1), 4, NRK_THUNDER); nr_rxq_push(&r, &e, i % 3, 1000 + (uint64_t)i); }
    CHECK(r.n == NR_RXQ && r.lost == 2 && r.q[0].e.hid == 3 && r.q[0].slot == 2 && r.q[0].at == 1002 && r.q[NR_RXQ - 1].e.hid == NR_RXQ + 2);
}

// (s195 reactidle) the not-driving branch of the frame body (Link has the pad: dead / Game Over, climb, swim, event...): may the
// reactions other games sent (ah_net_play) and the servo (ah_tick) run there this frame
static void test_idle(void) {
    // key 0 = today's behaviour: never (whatever the rest says)
    CHECK(nr_idle_decide(false, true, false, false) == NRI_OFF && nr_idle_decide(false, false, true, true) == NRI_OFF);
    // key 1 + the world there + no load + no pause = run
    CHECK(nr_idle_decide(true, true, false, false) == NRI_RUN);
    // no world (no world frame / no Link body) / a load / BotW paused: not run
    CHECK(nr_idle_decide(true, false, false, false) == NRI_NO_WORLD && nr_idle_decide(true, false, true, true) == NRI_NO_WORLD);
    CHECK(nr_idle_decide(true, true, true, false) == NRI_LOADING && nr_idle_decide(true, true, true, true) == NRI_LOADING);
    CHECK(nr_idle_decide(true, true, false, true) == NRI_PAUSED);
    CHECK(!strcmp(nr_idle_txt(NRI_RUN), "run") && !strcmp(nr_idle_txt(NRI_LOADING), "loading") && !strcmp(nr_idle_txt(NRI_OFF), "off (set reactidle 0)"));
    // the log: one line the first time a reaction is played in a not-driving stretch; a driving frame starts a new stretch
    NrIdleLog s;
    memset(&s, 0, sizeof s);
    CHECK(!nr_idle_log_due(&s, true, 0));                                       // not driving, nothing played: no line
    CHECK(nr_idle_log_due(&s, true, 1) && s.played == 1);                       // the first play: the line
    CHECK(!nr_idle_log_due(&s, true, 2) && !nr_idle_log_due(&s, true, 0) && s.played == 3);  // the same stretch: no more lines
    CHECK(!nr_idle_log_due(&s, false, 0) && s.played == 0 && !s.said);         // driving again: the stretch is over
    CHECK(!nr_idle_log_due(&s, false, 3));                                      // (the driving path never logs it)
    CHECK(nr_idle_log_due(&s, true, 1));                                        // the next stretch logs again
    CHECK(!nr_idle_log_due(&s, true, -1) && s.played == 1);                     // (a negative count is nothing played)
}

// (s197 review S5) a received knockback: refused past +-1000 (garbage), else clamped into Sora's atkp table span -100..350; the 0x7fff
// "no row" default passes as it is; the table's own values pass unchanged. The sender clamps the same way.
static void test_kb_span(void) {
    NetReactEnt e[2];
    float f[3] = {1, 2, 3};
    e[0] = nr_ent_make(5, 3, 125, 0, 7, "blade", f, false);
    e[1] = nr_ent_make(6, 4, 40, 10, 7, "thunder", f, false);
    uint8_t b[NET_REACT_PKT + 8], c[NET_REACT_PKT + 8];
    int n = net_pack_react(3, 77, e, 2, b, (int)sizeof b);
    NetReactEnt o[NET_REACT_MAX];
    int bad = 0;
    struct { int16_t in, out; } cs[] = {{1000, NET_REACT_KB_HI}, {351, 350}, {350, 350}, {349, 349}, {0, 0}, {-100, -100}, {-101, -100}, {-1000, -100},
                                        {125, 125}, {-25, -25}, {300, 300}, {0x7fff, 0x7fff}};
    for (size_t i = 0; i < sizeof cs / sizeof *cs; i++)
        for (int fld = 0; fld < 2; fld++) {
            memcpy(c, b, (size_t)n);
            net_w16(c + NET_HDR + NET_REACT_ENT + 6 + 2 * fld, (uint16_t)cs[i].in);  // (entry 1)
            CHECK(net_unpack_react(c, n, NULL, NULL, o, &bad) == 2 && bad == 0 && (fld ? o[1].kb2 : o[1].kb1) == cs[i].out && o[0].kb1 == 125);
        }
    CHECK(NET_REACT_KB_LO == -100 && NET_REACT_KB_HI == 350 && NET_REACT_KB_MAX == 1000);
    CHECK(net_react_kb(5000) == 350 && net_react_kb(-5000) == -100 && net_react_kb(0x7fff) == 0x7fff && net_react_kb(-100) == -100 && net_react_kb(200) == 200);
    CHECK(!net_react_kb_ok(1001) && !net_react_kb_ok(-1001) && net_react_kb_ok(1000) && net_react_kb_ok(0x7fff));
    // the receive log limiter (sora_net_proto.h NetLogRate): NET_LOGRATE_BURST lines per NET_LOGRATE_MS, the first refused one says so
    NetLogRate r;
    memset(&r, 0, sizeof r);
    int ok = 0, fq_n = 0;
    bool fq;
    for (int i = 0; i < 100; i++) { ok += net_lograte_ok(&r, 5000 + (uint64_t)i, &fq); fq_n += fq; }
    CHECK(ok == NET_LOGRATE_BURST && fq_n == 1 && r.quiet == 100 - NET_LOGRATE_BURST);
    CHECK(net_lograte_ok(&r, 5000 + NET_LOGRATE_MS, &fq) && !fq);  // (a new window)
    for (int i = 0; i < 20; i++) net_lograte_ok(&r, 5000 + NET_LOGRATE_MS + 1, NULL);
    CHECK(r.quiet == 100 - NET_LOGRATE_BURST + 20 - (NET_LOGRATE_BURST - 1));
    CHECK(net_lograte_ok(&r, 3, &fq));  // (a clock behind the window start: a new window, never stuck)
}

int main(void) {
    test_kb_span();
    test_idle();
    test_round_trip();
    test_bad_input();
    test_make_and_kinds();
    test_dedupe();
    test_decisions();
    test_queues();
    printf("react_test: %d/%d passed\n", runs - fails, runs);
    return fails != 0;
}

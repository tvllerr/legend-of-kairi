// host test for runtime/sora_net_esync.h core + the NET_ESYNC codec in sora_net_proto.h (s168 v7). No sockets, no game.
//   Windows: clang -O1 -g -Wall -Wextra -Iruntime runtime/test/esync_test.c -o build_h/esync_test.exe
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, e) (fabs((double)(a) - (double)(b)) <= (e))
static char g_log[16384];
static void test_log(const char *f, ...) {
    va_list a; va_start(a, f);
    size_t n = strlen(g_log);
    vsnprintf(g_log + n, sizeof g_log - n, f, a);
    va_end(a);
}
#define LOG test_log
#include "sora_net_esync.h"

// ---- fake exe ----
static uint32_t f_hids[ES_MAX + 8]; static int f_n;
static EsState f_state[ES_MAX + 8];
static int f_brain_calls, f_last_brain[256]; static uint32_t f_brain_hid[256];
static int f_apply_n, f_apply_snap; static EsState f_apply_last; static uint32_t f_apply_hid;
static struct { uint32_t hid; int on; uint32_t name, p0; } f_sens[256]; static int f_nsens;
static EsEvent f_ev[16]; static int f_nev;
static int fk_enum(uint32_t *h, int max) { int n = f_n < max ? f_n : max; memcpy(h, f_hids, sizeof(uint32_t) * (size_t)n); return n; }
static int fk_read(uint32_t hid, EsState *o) { for (int i = 0; i < f_n; i++) if (f_hids[i] == hid) { *o = f_state[i]; return 1; } return 0; }
static void fk_puppet(uint32_t hid, const EsState *t, int snap) { f_apply_n++, f_apply_snap = snap, f_apply_last = *t, f_apply_hid = hid; }
static void fk_brain(uint32_t hid, int on) { if (f_brain_calls < 256) f_brain_hid[f_brain_calls] = hid, f_last_brain[f_brain_calls] = on; f_brain_calls++; }
static int fk_events(EsEvent *o, int max) { int n = f_nev < max ? f_nev : max; memcpy(o, f_ev, sizeof(EsEvent) * (size_t)n); f_nev = 0; return n; }
static void fk_sensor(uint32_t hid, int on, uint32_t name, const uint32_t *p) {
    if (f_nsens < 256) f_sens[f_nsens].hid = hid, f_sens[f_nsens].on = on, f_sens[f_nsens].name = name, f_sens[f_nsens].p0 = p ? p[0] : 0;
    f_nsens++;
}
static void fake_reset(EsCtx *c) {
    memset(c, 0, sizeof *c);
    c->io = (EsIface){fk_enum, fk_read, fk_puppet, fk_brain, fk_events, fk_sensor};
    c->me = 1;
    f_n = f_brain_calls = f_apply_n = f_nsens = f_nev = 0;
    g_log[0] = 0;
}
static void add_enemy(uint32_t hid, float x, float y, float z) {
    f_hids[f_n] = hid;
    memset(&f_state[f_n], 0, sizeof f_state[0]);
    f_state[f_n].hid = hid, f_state[f_n].pos[0] = x, f_state[f_n].pos[1] = y, f_state[f_n].pos[2] = z;
    f_n++;
}
static EsPlayer mk(uint8_t id, const char *nm, float x) { EsPlayer p; memset(&p, 0, sizeof p); p.id = id; snprintf(p.name, sizeof p.name, "%s", nm); p.pos[0] = x; return p; }
static void players2(EsCtx *c, float me, float b) { EsPlayer p[2] = {mk(1, "Alpha", me), mk(2, "Bob", b)}; es_set_players(c, p, 2); }
static EsOut O;
static int step(EsCtx *c, uint64_t now) { es_step(c, now, &O); return O.nent; }
static EsState sm(uint32_t hid, float x, float yaw) { EsState s; memset(&s, 0, sizeof s); s.hid = hid, s.pos[0] = x, s.yaw = yaw; return s; }
static NetEvSeen g_seen;  // a fresh sensor-event window (zeroed per use)
static NetEvSeen *fresh_seen(void) { memset(&g_seen, 0, sizeof g_seen); return &g_seen; }
static void recv1(EsCtx *c, uint8_t from, EsState s, uint64_t now) { es_on_recv(c, from, &s, 1, NULL, 0, fresh_seen(), now); }
static void claim1(EsCtx *c, uint8_t from, EsState s, uint64_t now) { s.flags |= ES_FL_CLAIM; recv1(c, from, s, now); }  // (s180 a claimant's sample)

// ---- codec ----
static void test_codec(void) {
    CHECK(NET_VERSION == 11 && NET_ESYNC == 5 && NET_ESYNC_ENT == 40 && NET_ESYNC_EV == 26 && NET_ESYNC_PKT == 485 && NET_ESYNC_PKT <= NET_PKT_MAX);
    NetEsEnt e[NET_ESYNC_MAX], r[NET_ESYNC_MAX];
    NetEsEv v[NET_ESYNC_EVMAX], rv[NET_ESYNC_EVMAX];
    for (int i = 0; i < NET_ESYNC_MAX; i++) {
        memset(&e[i], 0, sizeof e[i]);
        e[i].hid = 0x80000000u + (uint32_t)i * 0x01020304u + 1, e[i].pos[0] = 1.5f + (float)i, e[i].pos[1] = -2.25f, e[i].pos[2] = 1000.0f * (float)i, e[i].yaw = -3.0f + 0.1f * (float)i;
        e[i].anim_hash[0] = 0xDEAD0000u + (uint32_t)i, e[i].anim_hash[1] = 0xBEEF0000u + (uint32_t)i, e[i].anim_bank[0] = (uint8_t)i, e[i].anim_bank[1] = 7,
        e[i].anim_chg[0] = (uint8_t)(200 + i), e[i].anim_chg[1] = (uint8_t)i, e[i].frame = i == 3 ? 0.0f : 12.5f + (float)i, e[i].action = (uint16_t)(65000 + i), e[i].flags = (uint16_t)(i & 3);
    }
    for (int i = 0; i < NET_ESYNC_EVMAX; i++) {
        v[i].seq = (uint8_t)(250 + i), v[i].on = (uint8_t)(i & 1), v[i].hid = 0x1000u + (uint32_t)i, v[i].name = 0xCAFE0000u + (uint32_t)i;
        for (int k = 0; k < 4; k++) v[i].params[k] = 0x11111111u * (uint32_t)(k + 1) + (uint32_t)i;
    }
    uint8_t buf[NET_PKT_MAX], pl = 0;
    uint16_t sq = 0;
    int ne = -1, nv = -1;
    int n = net_pack_esync(9, 0xFFFE, e, 8, v, 6, buf, sizeof buf);
    CHECK(n == 485 && buf[0] == 'S' && buf[1] == 'N' && buf[2] == 11 && buf[3] == NET_ESYNC && buf[4] == 9 && buf[7] == 8 && buf[8] == 6);
    CHECK(net_unpack_esync(buf, n, &pl, &sq, r, &ne, rv, &nv) && pl == 9 && sq == 0xFFFE && ne == 8 && nv == 6);
    for (int i = 0; i < 8; i++) CHECK(memcmp(&r[i], &e[i], sizeof r[i]) == 0);
    for (int i = 0; i < 6; i++) CHECK(rv[i].seq == v[i].seq && rv[i].on == v[i].on && rv[i].hid == v[i].hid && rv[i].name == v[i].name && !memcmp(rv[i].params, v[i].params, 16));
    // little endian: hid 0x11223344 first, then pos x 1.0f = 0x3F800000
    NetEsEnt one; memset(&one, 0, sizeof one);
    one.hid = 0x11223344, one.pos[0] = 1.0f, one.action = 0x0102, one.flags = 0x0304;
    n = net_pack_esync(1, 2, &one, 1, NULL, 0, buf, sizeof buf);
    CHECK(n == NET_HDR + 2 + 40 && buf[9] == 0x44 && buf[10] == 0x33 && buf[11] == 0x22 && buf[12] == 0x11 && buf[13] == 0 && buf[14] == 0 && buf[15] == 0x80 && buf[16] == 0x3F);
    CHECK(buf[9 + 36] == 2 && buf[9 + 37] == 1 && buf[9 + 38] == 4 && buf[9 + 39] == 3);
    // events only is fine
    n = net_pack_esync(1, 3, NULL, 0, v, 2, buf, sizeof buf);
    CHECK(n == NET_HDR + 2 + 52 && net_unpack_esync(buf, n, NULL, NULL, r, &ne, rv, &nv) && ne == 0 && nv == 2);
    // general unpacker accepts it as a type, checks the exact size
    int type = 0; uint8_t player = 0; uint16_t seq = 0; char name[NET_NAME_MAX + 1];
    CHECK(net_unpack(buf, n, &type, &player, &seq, NULL, name) && type == NET_ESYNC && player == 1 && seq == 3);
    CHECK(!net_unpack(buf, n - 1, &type, &player, &seq, NULL, name) && !net_unpack(buf, n + 1, &type, &player, &seq, NULL, name) && !net_unpack(buf, NET_HDR, &type, &player, &seq, NULL, name));
    // bad input
    CHECK(!net_unpack_esync(buf, n - 1, NULL, NULL, r, &ne, rv, &nv) && ne == 0 && nv == 0 && !net_unpack_esync(buf, 3, NULL, NULL, r, &ne, rv, &nv) && !net_unpack_esync(NULL, 50, NULL, NULL, r, &ne, rv, &nv));
    uint8_t bad[NET_PKT_MAX];
    memcpy(bad, buf, (size_t)n), bad[2] = 6;
    CHECK(!net_unpack_esync(bad, n, NULL, NULL, r, &ne, rv, &nv) && net_other_version(bad, n) == 6);
    memcpy(bad, buf, (size_t)n), bad[3] = NET_EDMG;
    CHECK(!net_unpack_esync(bad, n, NULL, NULL, r, &ne, rv, &nv));
    memcpy(bad, buf, (size_t)n), bad[0] = 'X';
    CHECK(!net_unpack_esync(bad, n, NULL, NULL, r, &ne, rv, &nv));
    memcpy(bad, buf, (size_t)n), bad[NET_HDR] = 9, bad[NET_HDR + 1] = 0;  // 9 entries: too many (and the length does not fit)
    CHECK(!net_unpack_esync(bad, n, NULL, NULL, r, &ne, rv, &nv));
    memset(bad, 0, sizeof bad), memcpy(bad, buf, NET_HDR);  // 0 + 0
    CHECK(!net_unpack_esync(bad, NET_HDR + 2, NULL, NULL, r, &ne, rv, &nv) && !net_unpack(bad, NET_HDR + 2, &type, &player, &seq, NULL, name));
    CHECK(net_pack_esync(1, 1, e, 9, NULL, 0, buf, sizeof buf) < 0 && net_pack_esync(1, 1, e, 1, v, 7, buf, sizeof buf) < 0 && net_pack_esync(1, 1, NULL, 0, NULL, 0, buf, sizeof buf) < 0 &&
          net_pack_esync(1, 1, e, 8, v, 6, buf, 484) < 0);
    // hid 0, NaN / Inf / huge numbers, on > 1 are dropped alone
    NetEsEnt q[5]; memset(q, 0, sizeof q);
    for (int i = 0; i < 5; i++) q[i].hid = 100 + (uint32_t)i, q[i].pos[0] = 1;
    q[0].hid = 0; q[1].pos[2] = NAN; q[2].yaw = INFINITY; q[3].frame = 1e9f;
    NetEsEv w[3]; memset(w, 0, sizeof w);
    w[0].hid = 5, w[0].on = 1; w[1].hid = 0, w[1].on = 1; w[2].hid = 6, w[2].on = 2;
    n = net_pack_esync(1, 4, q, 5, w, 3, buf, sizeof buf);
    CHECK(net_unpack_esync(buf, n, NULL, NULL, r, &ne, rv, &nv) && ne == 1 && r[0].hid == 104 && nv == 1 && rv[0].hid == 5);
    // seq: only newer; wrap 65535 -> 0; dup / older dropped; restart (> 256 behind) = new base
    NetEsSeen s; memset(&s, 0, sizeof s);
    CHECK(net_esseq_fresh(&s, 65530) && !net_esseq_fresh(&s, 65530) && net_esseq_fresh(&s, 65535) && !net_esseq_fresh(&s, 65532));
    CHECK(net_esseq_fresh(&s, 0) && net_esseq_fresh(&s, 1) && !net_esseq_fresh(&s, 65535) && !net_esseq_fresh(&s, 1) && net_esseq_fresh(&s, 5));
    CHECK(!net_esseq_fresh(&s, (uint16_t)(5 - 256)) && net_esseq_fresh(&s, (uint16_t)(5 - 257)));  // (a peer that restarted at a lower seq)
    CHECK(net_esseq_fresh(&s, (uint16_t)(5 - 256)));                                              // (now the base: newer)
    NetEsSeen big; memset(&big, 0, sizeof big);
    int ok = 1;
    for (uint32_t i = 0; i < 70000; i++) ok &= net_esseq_fresh(&big, (uint16_t)(1000 + i));
    CHECK(ok);
}

// ---- election ----
static void test_election(void) {
    EsCtx c; fake_reset(&c);
    // me (Alpha) at 0, Bob at 30; the enemy at 4: I own it, brain on, state due
    add_enemy(0xA1, 4, 0, 0);
    players2(&c, 0, 30);
    CHECK(step(&c, 1000) == 1 && O.ent[0].hid == 0xA1 && es_find(&c, 0xA1)->owner == 1 && f_brain_calls == 1 && f_last_brain[0] == 1);
    CHECK(strstr(g_log, "[esync] hid=000000a1 owner none -> Alpha"));
    // 20 Hz
    CHECK(step(&c, 1016) == 0 && step(&c, 1033) == 0 && step(&c, 1050) == 1 && step(&c, 1066) == 0 && step(&c, 1100) == 1);
    CHECK(f_brain_calls == 1);  // (brain told once)
    f_state[0].pos[0] = 20;
    step(&c, 1150);
    CHECK(es_find(&c, 0xA1)->owner == 1);
    // s180: Bob walks to 24 (16 m closer): I keep owning + sending however long (never on my own timer) ...
    players2(&c, 0, 24);
    int sent = 0;
    for (uint64_t t = 1200; t < 5000; t += 50) sent += step(&c, t);
    CHECK(es_find(&c, 0xA1)->owner == 1 && sent >= 70 && c.st.handovers == 0 && !strstr(g_log, "(closer"));
    // ... until his CLAIM sample arrives: I yield at once, sensors off
    claim1(&c, 2, sm(0xA1, 20, 0), 5000);
    CHECK(es_find(&c, 0xA1)->owner == 2 && strstr(g_log, "[esync] hid=000000a1 owner Alpha -> Bob (claim: yield)") && c.st.claim_yield == 1);
    CHECK(es_find(&c, 0xA1)->nsm == 1 && es_find(&c, 0xA1)->sm[0].s.flags == 0);  // (the claim flag never reaches the puppet)
    CHECK(step(&c, 5050) == 0);  // (not ours any more: nothing sent)
    CHECK(f_nsens >= 1 && f_sens[f_nsens - 1].hid == 0xA1 && f_sens[f_nsens - 1].on == 0 && f_sens[f_nsens - 1].name == 0);  // sensors forced off
    CHECK(c.st.handovers == 1);
    // s180: the CANDIDATE side. Bob owns it (his samples keep coming); I come closer. A dip below the slack resets my timer
    fake_reset(&c);
    add_enemy(0xA2, 20, 0, 0);
    players2(&c, 40, 17);  // me 20 m, Bob 3 m
    recv1(&c, 2, sm(0xA2, 20, 0), 0);
    step(&c, 0);
    CHECK(es_find(&c, 0xA2)->owner == 2);
    players2(&c, 22, 10);  // me 2, Bob 10: 8 m closer
    for (uint64_t t = 50; t <= 800; t += 50) recv1(&c, 2, sm(0xA2, 20, 0), t), step(&c, t);
    players2(&c, 22, 14);  // me 2, Bob 6: only 4 m closer: timer resets
    recv1(&c, 2, sm(0xA2, 20, 0), 850), step(&c, 850);
    players2(&c, 22, 10);
    for (uint64_t t = 900; t <= 1850; t += 50) recv1(&c, 2, sm(0xA2, 20, 0), t), step(&c, t);
    CHECK(es_find(&c, 0xA2)->owner == 2);  // 950 ms since the restart of the timer
    recv1(&c, 2, sm(0xA2, 20, 0), 1900);
    CHECK(step(&c, 1900) == 1 && es_find(&c, 0xA2)->owner == 1 && strstr(g_log, "Bob -> Alpha (closer: claim)") && (O.ent[0].flags & ES_FL_CLAIM));
    // his samples while I claim: counted, never used, keep my claim (flag) on; I own + send the whole time
    for (uint64_t t = 1950; t <= 2400; t += 50) { recv1(&c, 2, sm(0xA2, 20, 0), t); step(&c, t); }
    CHECK(es_find(&c, 0xA2)->owner == 1 && es_find(&c, 0xA2)->claim_from == 2 && c.st.rx_claimwait >= 10 && es_find(&c, 0xA2)->nsm == 0);
    // he saw the claim and went quiet: 300 ms later the claim is done, my samples go out without the flag
    for (uint64_t t = 2450; t <= 2750; t += 50) step(&c, t);
    CHECK(es_find(&c, 0xA2)->claim_from == -1 && c.st.claim_done == 1);
    step(&c, 2800);
    CHECK(O.nent == 1 && !(O.ent[0].flags & ES_FL_CLAIM));
    // exactly 5 m closer counts (>=), 4.9 m never
    fake_reset(&c);
    add_enemy(0xA3, 10, 0, 0);
    players2(&c, 15, 0);  // me 5, Bob 10
    recv1(&c, 2, sm(0xA3, 10, 0), 0);
    for (uint64_t t = 0; t <= 950; t += 50) recv1(&c, 2, sm(0xA3, 10, 0), t), step(&c, t);
    CHECK(es_find(&c, 0xA3)->owner == 2);
    step(&c, 1050);
    CHECK(es_find(&c, 0xA3)->owner == 1);
    fake_reset(&c);
    add_enemy(0xA6, 10, 0, 0);
    players2(&c, 15.1f, 0);
    for (uint64_t t = 0; t <= 5000; t += 50) recv1(&c, 2, sm(0xA6, 10, 0), t), step(&c, t);
    CHECK(es_find(&c, 0xA6)->owner == 2 && c.st.handovers == 0 && c.st.claims == 0);
    // not loaded here: I never claim it however close
    fake_reset(&c);
    players2(&c, 10, 40);
    for (uint64_t t = 0; t <= 3000; t += 50) recv1(&c, 2, sm(0xA7, 10, 0), t), step(&c, t);
    CHECK(es_find(&c, 0xA7)->owner == 2 && c.st.claims == 0);
    // ties on the first election: lower peer NAME wins whatever the id / order is
    fake_reset(&c);
    add_enemy(0xA4, 10, 0, 0);
    { EsPlayer p[2] = {mk(1, "Zed", 0), mk(2, "Bob", 20)}; es_set_players(&c, p, 2); }
    step(&c, 0);
    CHECK(es_find(&c, 0xA4)->owner == 2);
    // 4 players: nearest of 4, a tie by name
    fake_reset(&c);
    add_enemy(0xA5, 50, 0, 0);
    { EsPlayer p[4] = {mk(1, "Me", 0), mk(2, "Ann", 100), mk(3, "Cy", 53), mk(4, "Di", 47)}; es_set_players(&c, p, 4); }
    step(&c, 0);
    CHECK(es_find(&c, 0xA5)->owner == 3);  // 3 m each: Cy < Di
    { EsPlayer p[4] = {mk(1, "Me", 0), mk(2, "Ann", 100), mk(3, "Cy", 53), mk(4, "Di", 51)}; es_set_players(&c, p, 4); }
    step(&c, 100);  // Di is 2 m closer: below the slack, Cy keeps it
    CHECK(es_find(&c, 0xA5)->owner == 3);
    // s180: the owner leaves the player list: Di (nearest left) claims it; I (not the nearest) wait for her claim
    { EsPlayer p[3] = {mk(1, "Me", 0), mk(2, "Ann", 100), mk(4, "Di", 51)}; es_set_players(&c, p, 3); }
    step(&c, 150);
    CHECK(es_find(&c, 0xA5)->owner == 3 && c.st.claims == 0);
    claim1(&c, 4, sm(0xA5, 50, 0), 200);
    CHECK(es_find(&c, 0xA5)->owner == 4 && strstr(g_log, "player3 -> Di (claim)"));
    // ... and if I am the nearest left, I claim at once
    { EsPlayer p[2] = {mk(1, "Me", 49), mk(2, "Ann", 100)}; es_set_players(&c, p, 2); }
    step(&c, 250);
    CHECK(es_find(&c, 0xA5)->owner == 1 && strstr(g_log, "(old owner gone: claim)") && es_find(&c, 0xA5)->claim_from == 4);
}

// ---- stale owner ----
static void test_stale(void) {
    EsCtx c; fake_reset(&c);
    add_enemy(0xB1, 30, 0, 0);
    players2(&c, 0, 28);  // Bob nearest
    step(&c, 1000);
    CHECK(es_find(&c, 0xB1)->owner == 2 && f_last_brain[0] == 1);  // no samples from Bob yet: our brain keeps running
    recv1(&c, 2, sm(0xB1, 30, 0), 1050);
    step(&c, 1100);
    CHECK(f_brain_calls == 2 && f_last_brain[1] == 0 && f_apply_n == 1);  // puppet: brain off once, body driven
    step(&c, 1116);
    CHECK(f_brain_calls == 2 && f_apply_n == 2);
    // Bob falls silent: after 1 s from the last sample the election moves it to us, brain on, sensors off
    int sens0 = f_nsens;
    for (uint64_t t = 1150; t <= 2000; t += 50) step(&c, t);
    CHECK(es_find(&c, 0xB1)->owner == 2);
    step(&c, 2100);  // 1050 + 1000 < 2100
    CHECK(es_find(&c, 0xB1)->owner == 1 && c.st.stale == 1 && strstr(g_log, "(stale owner: claim)"));
    step(&c, 2116);
    CHECK(f_brain_calls == 3 && f_last_brain[2] == 1 && f_nsens > sens0);
    // s180: Bob is still nearer: I keep it (no hand-back on my timer) until HE claims it; the claim (Bob silent) is done after 300 ms
    for (uint64_t t = 2150; t <= 8000; t += 50) step(&c, t);
    CHECK(es_find(&c, 0xB1)->owner == 1 && c.st.claim_done == 1 && c.st.handovers == 1);
    claim1(&c, 2, sm(0xB1, 30, 0), 8000);
    CHECK(es_find(&c, 0xB1)->owner == 2 && strstr(g_log, "(claim: yield)"));
    // and after yielding I do not claim it back for ES_RECLAIM_MS even when he goes silent again
    for (uint64_t t = 8050; t < 8000 + ES_RECLAIM_MS; t += 50) step(&c, t);
    CHECK(es_find(&c, 0xB1)->owner == 2);
    step(&c, 8000 + ES_RECLAIM_MS + 50);
    CHECK(es_find(&c, 0xB1)->owner == 1);
    // alone (single player): never stale-flaps, brain stays on
    fake_reset(&c);
    add_enemy(0xB2, 1, 0, 0);
    EsPlayer solo[1] = {mk(1, "Alpha", 0)};
    es_set_players(&c, solo, 1);
    for (uint64_t t = 0; t < 5000; t += 100) step(&c, t);
    CHECK(es_find(&c, 0xB2)->owner == 1 && c.st.stale == 0 && c.st.handovers == 0 && f_brain_calls == 1);
    // three players: B (nearest) silent -> C (next nearest) claims it; I only follow C's claim
    fake_reset(&c);
    add_enemy(0xB3, 50, 0, 0);
    { EsPlayer p[3] = {mk(1, "Alpha", 0), mk(2, "Bob", 49), mk(3, "Cy", 40)}; es_set_players(&c, p, 3); }
    step(&c, 0);
    CHECK(es_find(&c, 0xB3)->owner == 2);
    step(&c, 1001);
    CHECK(es_find(&c, 0xB3)->owner == 2 && c.st.claims == 0);
    claim1(&c, 3, sm(0xB3, 50, 0), 1100);
    CHECK(es_find(&c, 0xB3)->owner == 3);
    // ... and if C never claims (not loaded there), I take it after 3 s of silence
    fake_reset(&c);
    add_enemy(0xB5, 50, 0, 0);
    { EsPlayer p[3] = {mk(1, "Alpha", 0), mk(2, "Bob", 49), mk(3, "Cy", 40)}; es_set_players(&c, p, 3); }
    step(&c, 0);
    step(&c, 2900);
    CHECK(es_find(&c, 0xB5)->owner == 2);
    step(&c, 3100);
    CHECK(es_find(&c, 0xB5)->owner == 1 && c.st.stale == 1);
    // owner == me but the enemy is not loaded here (nothing to send): silent for 1 s -> the next nearest takes over
    fake_reset(&c);
    players2(&c, 0, 20);
    f_n = 0;
    recv1(&c, 1, sm(0xB4, 2, 0), 0);          // (a sample claimed by "me": never believed) -> entry exists, no owner change needed
    CHECK(es_find(&c, 0xB4) != NULL && es_find(&c, 0xB4)->owner == 1);
    step(&c, 500);
    CHECK(es_find(&c, 0xB4)->owner == 1);
    step(&c, 5100);
    CHECK(es_find(&c, 0xB4)->owner == 1 && c.st.stale == 0);  // s180: still "mine" on paper ...
    recv1(&c, 2, sm(0xB4, 2, 0), 5200);                        // ... Bob's sample (even without a claim) takes it: I cannot send it
    CHECK(es_find(&c, 0xB4)->owner == 2 && es_find(&c, 0xB4)->nsm == 1);  // (s180: an owner change drops the old owner's samples)
}

// ---- interpolation, snap, frame advance ----
static void test_interp(void) {
    EsCtx c; fake_reset(&c);
    add_enemy(0xC1, 0, 0, 0);  // our body sits at the origin
    players2(&c, 0, 8);
    step(&c, 0);
    f_state[0].pos[0] = 9;  // next to Bob
    for (uint64_t t = 50; t <= 1300; t += 50) step(&c, t);
    claim1(&c, 2, sm(0xC1, 9, 0), 1300);  // (s180: Bob claims it)
    CHECK(es_find(&c, 0xC1)->owner == 2);
    EsState a = sm(0xC1, 10, 3.0f), b = sm(0xC1, 20, -3.0f);
    a.frame = 10, a.anim_hash[0] = 0x111, a.anim_chg[0] = 4; b.anim_hash[0] = 0x222, b.anim_chg[0] = 5;
    recv1(&c, 2, a, 2000);
    recv1(&c, 2, b, 2100);
    f_state[0].pos[0] = 14;  // our body is near the target
    f_apply_n = 0;
    step(&c, 2150);  // render time 2050: halfway
    CHECK(f_apply_n == 1 && NEAR(f_apply_last.pos[0], 15, 1e-3) && f_apply_last.anim_hash[0] == 0x111 && f_apply_last.anim_chg[0] == 4 && f_apply_hid == 0xC1);
    CHECK(NEAR(f_apply_last.frame, 10 + 50 * ES_ANIM_FPS / 1000.0, 1e-3) && f_apply_snap == 0);
    CHECK(fabsf(f_apply_last.yaw) > 3.1f);  // yaw takes the short way round (3.0 -> -3.0 crosses pi)
    // hold the newest sample (no extrapolation), the anim of the newest sample
    step(&c, 2400);  // render 2300 > 2100
    CHECK(NEAR(f_apply_last.pos[0], 20, 1e-3) && f_apply_last.anim_hash[0] == 0x222 && f_apply_last.anim_chg[0] == 5);
    // hard snap when the body is > 3 m from the target
    f_state[0].pos[0] = 10;
    step(&c, 2450);
    CHECK(f_apply_snap == 1);
    f_state[0].pos[0] = 18;
    step(&c, 2500);
    CHECK(f_apply_snap == 0);
    // frame 0 = unknown: never advanced
    recv1(&c, 2, sm(0xC1, 21, 0), 2600);
    step(&c, 2800);
    CHECK(f_apply_last.frame == 0.0f);
    // a known frame advances by the time since the sample, at most ES_HOLD_MS
    EsState k2 = sm(0xC1, 22, 0); k2.frame = 100;
    recv1(&c, 2, k2, 3000);
    step(&c, 3900);  // render 3800: 800 after the sample
    CHECK(NEAR(f_apply_last.frame, 100 + 800 * ES_ANIM_FPS / 1000.0, 1e-3));
    CHECK(es_find(&c, 0xC1)->nsm == 3);  // only 3 samples kept
    // before the oldest sample: that sample as is (no negative advance)
    fake_reset(&c);
    add_enemy(0xC2, 0, 0, 0);
    players2(&c, 0, 8);
    EsState f1 = sm(0xC2, 7, 0); f1.frame = 5;
    recv1(&c, 2, f1, 5000);  // first sample claims the enemy for Bob
    step(&c, 5010);          // render 4910 < 5000
    CHECK(f_apply_n == 1 && NEAR(f_apply_last.pos[0], 7, 1e-3) && f_apply_last.frame == 5.0f);
    // two samples arriving in the same ms do not divide by zero
    recv1(&c, 2, sm(0xC2, 9, 0), 5000);
    step(&c, 5200);
    CHECK(isfinite(f_apply_last.pos[0]));
}

// ---- hid 0, table, release ----
static void test_misc(void) {
    EsCtx c; fake_reset(&c);
    add_enemy(0, 1, 0, 0);
    add_enemy(0xD1, 1, 0, 0);
    players2(&c, 0, 30);
    CHECK(step(&c, 0) == 1 && O.ent[0].hid == 0xD1 && c.st.hid0_skip == 1 && es_find(&c, 0) == NULL);
    // the table is full at ES_MAX: more are counted, not stored
    fake_reset(&c);
    players2(&c, 0, 300);
    for (int i = 0; i < ES_MAX; i++) add_enemy(0x1000u + (uint32_t)i, 1, 0, 0);
    CHECK(step(&c, 0) == ES_MAX && c.st.full == 0);
    recv1(&c, 2, sm(0x9999, 5, 0), 10);
    CHECK(c.st.full == 1 && es_find(&c, 0x9999) == NULL);
    // an enemy neither loaded nor heard of for ES_FORGET_MS leaves the table
    fake_reset(&c);
    players2(&c, 0, 30);
    add_enemy(0xD2, 1, 0, 0);
    step(&c, 0);
    f_n = 0;
    step(&c, 5000);
    CHECK(es_find(&c, 0xD2) != NULL);
    step(&c, 10001);
    CHECK(es_find(&c, 0xD2) == NULL);
    // release: puppets get their brain back + sensors cleared, table empty
    fake_reset(&c);
    add_enemy(0xD3, 30, 0, 0);
    players2(&c, 0, 28);
    recv1(&c, 2, sm(0xD3, 30, 0), 0);
    step(&c, 100);
    CHECK(f_brain_calls == 1 && f_last_brain[0] == 0);
    int b0 = f_brain_calls, s0 = f_nsens;
    es_release_all(&c);
    CHECK(f_brain_calls == b0 + 1 && f_last_brain[b0] == 1 && f_brain_hid[b0] == 0xD3 && f_nsens == s0 + 1 && f_sens[s0].on == 0 && f_sens[s0].name == 0 && es_find(&c, 0xD3) == NULL);
    // an enemy that unloads and reloads gets its brain state told again
    fake_reset(&c);
    add_enemy(0xD4, 30, 0, 0);
    players2(&c, 0, 28);
    recv1(&c, 2, sm(0xD4, 30, 0), 0);
    step(&c, 10);
    int n1 = f_brain_calls;
    CHECK(n1 == 1 && f_last_brain[0] == 0);
    f_n = 0, step(&c, 20);
    add_enemy(0xD4, 30, 0, 0);
    recv1(&c, 2, sm(0xD4, 30, 0), 25);
    step(&c, 30);
    CHECK(f_brain_calls == n1 + 1 && f_last_brain[n1] == 0);
}

// ---- events and receive-side rules ----
typedef struct { uint8_t b[NET_ESYNC_PKT]; int len; } Pk;
static Pk g_pk[64]; static int g_npk;
static void collect(const uint8_t *b, int len, void *u) { (void)u; memcpy(g_pk[g_npk].b, b, (size_t)len); g_pk[g_npk++].len = len; }
static void test_events(void) {
    EsCtx c; fake_reset(&c);
    add_enemy(0xE1, 2, 0, 0);
    players2(&c, 0, 40);
    uint16_t seq = 100;
    // owner side: the exe reports sensor ON / OFF: queued, each rides 3 packets, then gone
    step(&c, 0);
    f_ev[0] = (EsEvent){.hid = 0xE1, .on = 1, .name = 0xAAAA, .params = {1, 2, 3, 4}};
    f_ev[1] = (EsEvent){.hid = 0xE1, .on = 0, .name = 0xAAAA};
    f_ev[2] = (EsEvent){.hid = 0xFF, .on = 1, .name = 0xBBBB};  // not loaded here: dropped
    f_ev[3] = (EsEvent){.hid = 0, .on = 1, .name = 0xBBBB};     // hid 0: dropped
    f_nev = 4;
    g_npk = 0;
    uint64_t was = c.next_send;
    step(&c, 50);
    es_packets(&c, &O, c.next_send != was, 1, &seq, collect, NULL);
    CHECK(g_npk == 1 && c.nevq == 2 && seq == 101);
    NetEsEnt e[8]; NetEsEv v[6]; int ne, nv;
    CHECK(net_unpack_esync(g_pk[0].b, g_pk[0].len, NULL, NULL, e, &ne, v, &nv) && ne == 1 && nv == 2 && v[0].on == 1 && v[0].name == 0xAAAA && v[0].params[3] == 4 && v[1].on == 0 && v[0].seq != v[1].seq);
    uint8_t evs0 = v[0].seq;
    for (int rep = 0; rep < 2; rep++) {  // 2 more repeats, in the next send slots (the state rides along)
        was = c.next_send;
        step(&c, 100 + (uint64_t)rep * 50);
        es_packets(&c, &O, c.next_send != was, 1, &seq, collect, NULL);
    }
    CHECK(c.nevq == 0 && g_npk == 3);
    CHECK(net_unpack_esync(g_pk[2].b, g_pk[2].len, NULL, NULL, e, &ne, v, &nv) && nv == 2 && v[0].seq == evs0);
    was = c.next_send;
    step(&c, 200);
    int before = g_npk;
    es_packets(&c, &O, c.next_send != was, 1, &seq, collect, NULL);
    CHECK(g_npk == before + 1);
    CHECK(net_unpack_esync(g_pk[g_npk - 1].b, g_pk[g_npk - 1].len, NULL, NULL, e, &ne, v, &nv) && nv == 0 && ne == 1);
    // event-only packet: no enemy loaded any more but events wait and the gate is open
    f_n = 0;
    c.evq[0] = (EsEvent){.seq = 3, .on = 0, .hid = 0xE1, .name = 1}, c.evrep[0] = 1, c.nevq = 1;
    g_npk = 0;
    was = c.next_send;
    step(&c, 250);
    es_packets(&c, &O, c.next_send != was, 1, &seq, collect, NULL);
    CHECK(O.nent == 0 && g_npk == 1 && g_pk[0].len == 7 + 2 + 26 && c.nevq == 0);
    // 9 enemies -> 2 packets of states (8 + 1); 8 events -> 6 + 2 riding along
    fake_reset(&c);
    players2(&c, 0, 400);
    for (int i = 0; i < 9; i++) add_enemy(0xE100u + (uint32_t)i, 1, 0, 0);
    step(&c, 0);
    seq = 0, g_npk = 0;
    for (int i = 0; i < 8; i++) f_ev[i] = (EsEvent){.hid = 0xE100u + (uint32_t)i, .on = 1, .name = 7};
    f_nev = 8;
    was = c.next_send;
    step(&c, 50);
    es_packets(&c, &O, c.next_send != was, 1, &seq, collect, NULL);
    CHECK(g_npk == 2 && g_pk[0].len == 7 + 2 + 8 * 40 + 6 * 26 && g_pk[1].len == 7 + 2 + 1 * 40 + 2 * 26 && c.nevq == 8);
    // a hand-over drops my queued events of that hid (the new owner plays it)
    fake_reset(&c);
    add_enemy(0xE2, 10, 0, 0);
    players2(&c, 0, 50);
    step(&c, 0);
    f_ev[0] = (EsEvent){.hid = 0xE2, .on = 1, .name = 1};
    f_nev = 1;
    step(&c, 10);
    CHECK(c.nevq == 1);
    players2(&c, 30, 12);  // Bob 2 m away, me 20: he claims it
    claim1(&c, 2, sm(0xE2, 10, 0), 60);
    CHECK(es_find(&c, 0xE2)->owner == 2 && c.nevq == 0 && c.st.ev_dropped == 1);
    // receive side: events only from the owner, applied once (event seq window), for loaded enemies only
    fake_reset(&c);
    add_enemy(0xE3, 40, 0, 0);
    players2(&c, 0, 38);
    NetEvSeen seen; memset(&seen, 0, sizeof seen);
    EsState st = sm(0xE3, 40, 0);
    EsEvent on = {.seq = 7, .on = 1, .hid = 0xE3, .name = 0xF00D, .params = {9, 8, 7, 6}};
    step(&c, 0);
    es_on_recv(&c, 2, &st, 1, &on, 1, &seen, 10);
    CHECK(f_nsens == 1 && f_sens[0].on == 1 && f_sens[0].name == 0xF00D && f_sens[0].p0 == 9);
    es_on_recv(&c, 2, &st, 1, &on, 1, &seen, 20);  // a repeat (next packet): not applied again
    CHECK(f_nsens == 1 && c.st.rx_ev_dup == 1);
    EsEvent off = {.seq = 8, .on = 0, .hid = 0xE3, .name = 0xF00D};
    EsEvent both[2] = {on, off};
    es_on_recv(&c, 2, &st, 1, both, 2, &seen, 30);  // 7 again (dup), 8 new
    CHECK(f_nsens == 2 && f_sens[1].on == 0 && c.st.rx_ev_dup == 2);
    EsEvent w1 = {.seq = 255, .on = 1, .hid = 0xE3, .name = 1}, w2 = {.seq = 0, .on = 0, .hid = 0xE3, .name = 1};  // wrap 255 -> 0
    es_on_recv(&c, 2, &st, 1, &w1, 1, &seen, 40);
    es_on_recv(&c, 2, &st, 1, &w2, 1, &seen, 50);
    es_on_recv(&c, 2, &st, 1, &w2, 1, &seen, 60);
    CHECK(f_nsens == 4);
    // an event from a player who is not the owner: dropped
    EsEvent bad = {.seq = 20, .on = 1, .hid = 0xE3, .name = 5};
    { EsPlayer p[3] = {mk(1, "Alpha", 0), mk(2, "Bob", 38), mk(3, "Cy", 90)}; es_set_players(&c, p, 3); }
    es_on_recv(&c, 3, &st, 1, &bad, 1, &seen, 70);
    CHECK(f_nsens == 4 && c.st.rx_ev_notowner == 1 && c.st.rx_notowner >= 1);
    // an event for an enemy not loaded here: counted, not applied
    f_n = 0;
    step(&c, 80);
    EsEvent far_ev = {.seq = 21, .on = 1, .hid = 0xE3, .name = 5};
    es_on_recv(&c, 2, &st, 1, &far_ev, 1, &seen, 90);
    CHECK(f_nsens == 4 && c.st.rx_ev_nolocal == 1);
    // a sample from an unknown player id (no SNAP yet) is dropped
    uint32_t nb = c.st.rx_notowner;
    es_on_recv(&c, 9, &st, 1, NULL, 0, &seen, 100);
    CHECK(c.st.rx_notowner == nb + 1);
    // conflict: I own it too -> mine wins, his sample is not used
    fake_reset(&c);
    add_enemy(0xE4, 1, 0, 0);
    players2(&c, 0, 40);
    step(&c, 0);
    recv1(&c, 2, sm(0xE4, 1, 0), 10);
    CHECK(c.st.rx_conflict == 1 && es_find(&c, 0xE4)->nsm == 0 && es_find(&c, 0xE4)->owner == 1);
    // his samples arriving while I time the hand-over to him = I follow at once
    fake_reset(&c);
    add_enemy(0xE5, 12, 0, 0);
    players2(&c, 10, 40);
    step(&c, 0);
    f_state[0].pos[0] = 15;
    players2(&c, -3, 14);  // me 18 m, Bob 1 m
    step(&c, 100);
    CHECK(es_find(&c, 0xE5)->owner == 1 && es_find(&c, 0xE5)->cand == -1);  // (s180: only the candidate times it)
    recv1(&c, 2, sm(0xE5, 15, 0), 200);   // his plain sample = a conflict (tie-break: Alpha keeps it)
    CHECK(es_find(&c, 0xE5)->owner == 1 && es_find(&c, 0xE5)->nsm == 0 && c.st.rx_conflict == 1);
    claim1(&c, 2, sm(0xE5, 15, 0), 250);  // his claim: yield, the sample is used
    CHECK(es_find(&c, 0xE5)->owner == 2 && es_find(&c, 0xE5)->nsm == 1 && strstr(g_log, "(claim: yield)"));
    // a third player's plain samples while the owner is quiet > ES_QUIET_MS = follow him (a missed claim); not while the owner talks
    { EsPlayer p[3] = {mk(1, "Alpha", -3), mk(2, "Bob", 14), mk(3, "Cy", 16)}; es_set_players(&c, p, 3); }
    recv1(&c, 2, sm(0xE5, 15, 0), 300);
    recv1(&c, 3, sm(0xE5, 15, 0), 400);
    CHECK(es_find(&c, 0xE5)->owner == 2);
    recv1(&c, 3, sm(0xE5, 15, 0), 900);
    CHECK(es_find(&c, 0xE5)->owner == 3 && strstr(g_log, "(his samples arrived)"));
}


// S168 review: MF-1 (no brain stays off after release / forget), MF-3 (two owners settle), brain(0) refresh
// s178 R2 club sync: extra owner events (take_extra) use the same owner + loaded filter; ES_EV_CLUB events go to on_extra, never to the exe
static EsEvent x_ev[8]; static int x_nev;
static int fx_take(EsEvent *o, int max) { int n = x_nev < max ? x_nev : max; memcpy(o, x_ev, sizeof(EsEvent) * (size_t)n); x_nev = 0; return n; }
static EsEvent x_rx[8]; static int x_nrx; static uint64_t x_rx_now;
static void fx_on(const EsEvent *e, uint64_t now) { if (x_nrx < 8) x_rx[x_nrx++] = *e; x_rx_now = now; }
static void test_stale_nowhere(void) {  // s178: an enemy loaded nowhere here, its owner goes silent: we never take it, no flip-flop
    EsCtx c; fake_reset(&c);
    players2(&c, 0, 40);
    recv1(&c, 2, sm(0xD1, 40, 0), 0);
    step(&c, 0);
    CHECK(es_find(&c, 0xD1) && es_find(&c, 0xD1)->owner == 2);
    for (uint64_t t = 50; t <= 4000; t += 50) step(&c, t);
    CHECK(es_find(&c, 0xD1)->owner == 2 && c.st.stale == 0 && c.st.handovers == 0);
}
static void test_club(void) {
    EsCtx c; fake_reset(&c);
    c.take_extra = fx_take, c.on_extra = fx_on;
    x_nev = x_nrx = 0;
    add_enemy(0xC1, 2, 0, 0);
    players2(&c, 0, 40);
    uint16_t seq = 1;
    step(&c, 0);
    x_ev[0] = (EsEvent){.hid = 0xC1, .on = 1, .name = ES_EV_CLUB, .params = {655, 0x3f800000, 0x40000000, 0}};
    x_ev[1] = (EsEvent){.hid = 0xFF, .on = 1, .name = ES_EV_CLUB};  // not loaded here: dropped
    x_nev = 2;
    g_npk = 0;
    uint64_t was = c.next_send;
    step(&c, 50);
    es_packets(&c, &O, c.next_send != was, 1, &seq, collect, NULL);
    NetEsEnt e[8]; NetEsEv v[6]; int ne, nv;
    CHECK(g_npk == 1 && net_unpack_esync(g_pk[0].b, g_pk[0].len, NULL, NULL, e, &ne, v, &nv) && nv == 1 && v[0].name == ES_EV_CLUB && v[0].params[0] == 655 && v[0].on == 1);
    // not the owner (Bob 1 m away owns it after the hand-over): our extra events are dropped
    players2(&c, 30, 1);
    claim1(&c, 2, sm(0xC1, 2, 0), 100);
    for (uint64_t t = 100; t <= 1300; t += 50) step(&c, t);
    CHECK(es_find(&c, 0xC1)->owner == 2);
    int q0 = c.nevq;
    x_ev[0] = (EsEvent){.hid = 0xC1, .on = 0, .name = ES_EV_CLUB};
    x_nev = 1;
    step(&c, 1350);
    CHECK(c.nevq == q0);
    // receive: from the owner -> on_extra (with now), the exe's puppet_sensor is NOT called
    fake_reset(&c);
    c.take_extra = fx_take, c.on_extra = fx_on;
    x_nrx = 0;
    add_enemy(0xC3, 40, 0, 0);
    players2(&c, 0, 38);
    NetEvSeen seen; memset(&seen, 0, sizeof seen);
    EsState st = sm(0xC3, 40, 0);
    step(&c, 0);
    EsEvent on = {.seq = 3, .on = 1, .hid = 0xC3, .name = ES_EV_CLUB, .params = {700, 1, 2, 0}};
    es_on_recv(&c, 2, &st, 1, &on, 1, &seen, 10);
    CHECK(x_nrx == 1 && x_rx[0].params[0] == 700 && x_rx[0].on == 1 && x_rx_now == 10 && f_nsens == 0);
    es_on_recv(&c, 2, &st, 1, &on, 1, &seen, 20);  // repeat: once only
    CHECK(x_nrx == 1);
    EsEvent sens = {.seq = 4, .on = 1, .hid = 0xC3, .name = 0xF00D};  // a normal sensor event still goes to the exe
    es_on_recv(&c, 2, &st, 1, &sens, 1, &seen, 30);
    CHECK(x_nrx == 1 && f_nsens == 1);
    EsEvent bad = {.seq = 5, .on = 1, .hid = 0xC3, .name = ES_EV_CLUB};  // from a non-owner: dropped
    { EsPlayer p[3] = {mk(1, "Alpha", 0), mk(2, "Bob", 38), mk(3, "Cy", 90)}; es_set_players(&c, p, 3); }
    es_on_recv(&c, 3, &st, 1, &bad, 1, &seen, 40);
    CHECK(x_nrx == 1);
}

static void test_review_fixes(void) {
    EsCtx c; fake_reset(&c);
    // MF-1a: a puppet that UNLOADED, then esync off -> its brain still comes back (no sensor call: not loaded)
    add_enemy(0xF1, 30, 0, 0);
    players2(&c, 0, 28);
    recv1(&c, 2, sm(0xF1, 30, 0), 0);
    step(&c, 100);
    CHECK(f_brain_calls == 1 && f_last_brain[0] == 0);
    f_n = 0, step(&c, 200);
    CHECK(es_find(&c, 0xF1) && !es_find(&c, 0xF1)->local && es_find(&c, 0xF1)->brain == 0);
    int b0 = f_brain_calls, s0 = f_nsens;
    es_release_all(&c);
    CHECK(f_brain_calls == b0 + 1 && f_last_brain[b0] == 1 && f_brain_hid[b0] == 0xF1 && f_nsens == s0);
    // MF-1b: the 10 s forget gives the brain back first
    fake_reset(&c);
    add_enemy(0xF2, 30, 0, 0);
    players2(&c, 0, 28);
    recv1(&c, 2, sm(0xF2, 30, 0), 0);
    step(&c, 100);
    f_n = 0, step(&c, 200);
    b0 = f_brain_calls;
    step(&c, 200 + ES_FORGET_MS + 1);
    CHECK(es_find(&c, 0xF2) == NULL && f_brain_calls == b0 + 1 && f_last_brain[b0] == 1 && f_brain_hid[b0] == 0xF2);
    // refresh: a puppet's brain(0) is told again every ES_BRAIN_REFRESH_MS (the exe forgets unloaded hids)
    fake_reset(&c);
    add_enemy(0xF3, 30, 0, 0);
    players2(&c, 0, 28);
    uint64_t t = 0;
    for (; t <= 2 * ES_BRAIN_REFRESH_MS + 50; t += 50) { recv1(&c, 2, sm(0xF3, 30, 0), t); step(&c, t); }
    int offs = 0;
    for (int i = 0; i < f_brain_calls && i < 256; i++) offs += f_last_brain[i] == 0;
    CHECK(offs == 3 && f_brain_calls == 3);
    // MF-3: both claim it. I am "Bob" (id 2, the higher name): his sample makes me yield, and the sample is used
    fake_reset(&c);
    c.me = 2;
    add_enemy(0xF4, 1, 0, 0);
    { EsPlayer p[2] = {mk(2, "Bob", 0), mk(1, "Alpha", 3.0f)}; es_set_players(&c, p, 2); }
    step(&c, 0);
    CHECK(es_find(&c, 0xF4)->owner == 2);
    recv1(&c, 1, sm(0xF4, 1, 0), 10);
    CHECK(c.st.rx_conflict == 1 && c.st.yielded == 1 && es_find(&c, 0xF4)->owner == 1 && es_find(&c, 0xF4)->nsm == 1 && strstr(g_log, "(conflict: yield)"));
    step(&c, 60);
    CHECK(f_brain_calls >= 2 && f_last_brain[f_brain_calls - 1] == 0 && f_apply_n == 1);   // now a puppet
    // ... and the other side ("Alpha") keeps it: Bob's conflicting sample is dropped, no yield
    fake_reset(&c);
    add_enemy(0xF5, 1, 0, 0);
    { EsPlayer p[2] = {mk(1, "Alpha", 0), mk(2, "Bob", 3.0f)}; es_set_players(&c, p, 2); }
    step(&c, 0);
    recv1(&c, 2, sm(0xF5, 1, 0), 10);
    CHECK(c.st.rx_conflict == 1 && c.st.yielded == 0 && es_find(&c, 0xF5)->owner == 1 && es_find(&c, 0xF5)->nsm == 0);
    // same name on both PCs: the lower player id keeps it
    fake_reset(&c);
    c.me = 2;
    add_enemy(0xF6, 1, 0, 0);
    { EsPlayer p[2] = {mk(2, "Link", 0), mk(1, "Link", 3.0f)}; es_set_players(&c, p, 2); }
    step(&c, 0);
    recv1(&c, 1, sm(0xF6, 1, 0), 10);
    CHECK(c.st.yielded == 1 && es_find(&c, 0xF6)->owner == 1);
}

// ---- s180: claim refused, two games with disagreeing views (the s179 split brain) ----
static void test_claim_refused(void) {
    EsCtx c; fake_reset(&c);
    add_enemy(0x71, 20, 0, 0);
    players2(&c, 22, 10);  // me 2 m, Bob 10 m
    uint64_t t = 0;
    for (; t <= 1100; t += 50) recv1(&c, 2, sm(0x71, 20, 0), t), step(&c, t);
    CHECK(es_find(&c, 0x71)->owner == 1 && es_find(&c, 0x71)->claim_from == 2);
    for (; t <= 3200; t += 50) recv1(&c, 2, sm(0x71, 20, 0), t), step(&c, t);  // he never yields (old DLL / one-way loss)
    CHECK(es_find(&c, 0x71)->owner == 2 && c.st.claim_refused == 1 && strstr(g_log, "(claim refused: he kept sending)"));
    for (; t <= 3200 + ES_CLAIM_BAN_MS - 100; t += 50) recv1(&c, 2, sm(0x71, 20, 0), t), step(&c, t);
    CHECK(es_find(&c, 0x71)->owner == 2 && c.st.claims == 1);  // (banned)
    for (; t <= 3200 + ES_CLAIM_BAN_MS + 1200; t += 50) recv1(&c, 2, sm(0x71, 20, 0), t), step(&c, t);
    CHECK(c.st.claims == 2);  // (ban over: claims again)
}

// two EsCtx = two games; the fake exe is shared (same enemies, same bodies on both). Packets go through the real codec.
typedef struct { EsCtx c; uint16_t seq; NetEvSeen seen; } SimG;
static void sim_send(SimG *a, SimG *b, uint64_t now, int deliver) {
    uint64_t was = a->c.next_send;
    es_step(&a->c, now, &O);
    g_npk = 0;
    es_packets(&a->c, &O, a->c.next_send != was, a->c.me, &a->seq, collect, NULL);
    for (int k = 0; k < g_npk && deliver; k++) {
        NetEsEnt e[NET_ESYNC_MAX]; NetEsEv v[NET_ESYNC_EVMAX]; int ne = 0, nv = 0; uint8_t pl; uint16_t sq;
        if (net_unpack_esync(g_pk[k].b, g_pk[k].len, &pl, &sq, e, &ne, v, &nv)) es_on_recv(&b->c, pl, e, ne, v, nv, &b->seen, now);
    }
}
static SimG SA, SB;
static void sim_init(void) {
    fake_reset(&SA.c), fake_reset(&SB.c);
    SA.c.me = 1, SB.c.me = 2, SA.seq = 100, SB.seq = 5000;
    memset(&SA.seen, 0, sizeof SA.seen), memset(&SB.seen, 0, sizeof SB.seen);
}
// each game's own view: (my x, the other's x as I see him)
static void sim_views(float a_me, float a_b, float b_me, float b_a) {
    EsPlayer pa[2] = {mk(1, "PC", a_me), mk(2, "PC2", a_b)}, pb[2] = {mk(2, "PC2", b_me), mk(1, "PC", b_a)};
    es_set_players(&SA.c, pa, 2), es_set_players(&SB.c, pb, 2);
}
static int sim_owner(SimG *g, uint32_t hid) { EsEnt *e = es_find(&g->c, hid); return e ? e->owner : -9; }
static void test_two_games(void) {
    // A: PC owns it; PC2 walks up. PC sees PC2 6 m closer, PC2 sees itself only 4 m closer (old SNAPs) = the s179 split brain.
    sim_init();
    add_enemy(0x91, 20, 0, 0);
    sim_views(18, 40, 40, 18);
    uint64_t t = 0;
    for (; t < 1000; t += 16) sim_send(&SA, &SB, t, 1), sim_send(&SB, &SA, t, 1);
    CHECK(sim_owner(&SA, 0x91) == 1 && sim_owner(&SB, 0x91) == 1);
    sim_views(26, 20, 20, 26.0f - 2.0f);  // PC: me 6, him 0 (6 closer); PC2: me 0, PC 4 (4 closer)
    int flips = 0, disagree = 0, a_sent = 0;
    for (; t < 11000; t += 16) {
        int oa = sim_owner(&SA, 0x91);
        sim_send(&SA, &SB, t, 1), a_sent += O.nent;
        sim_send(&SB, &SA, t, 1);
        flips += sim_owner(&SA, 0x91) != oa;
        disagree += sim_owner(&SA, 0x91) != sim_owner(&SB, 0x91);
    }
    CHECK(flips == 0 && disagree == 0 && sim_owner(&SA, 0x91) == 1 && a_sent >= 190);  // (PC keeps it and keeps sending: no stale / back)
    CHECK(SA.c.st.handovers == 0 && SB.c.st.handovers == 0 && SB.c.st.claims == 0);
    // B: the other way: PC2 sees itself 6 m closer, PC sees only 4 m -> PC2 claims, PC yields on the claim. One hand-over, then calm.
    sim_views(26, 22, 20, 26);           // PC: me 6, him 2 (4 closer); PC2: me 0, PC 6 (6 closer)
    flips = 0, disagree = 0;
    int dis_after = 0;
    uint64_t t_b = t;
    for (; t < t_b + 10000; t += 16) {
        int oa = sim_owner(&SA, 0x91);
        sim_send(&SA, &SB, t, 1), sim_send(&SB, &SA, t, 1);
        flips += sim_owner(&SA, 0x91) != oa;
        int d = sim_owner(&SA, 0x91) != sim_owner(&SB, 0x91);
        disagree += d;
        if (t > t_b + 1500) dis_after += d;
    }
    CHECK(sim_owner(&SA, 0x91) == 2 && sim_owner(&SB, 0x91) == 2 && flips == 1 && dis_after == 0 && disagree <= 4);
    CHECK(SB.c.st.claims == 1 && SB.c.st.claim_done == 1 && SA.c.st.claim_yield == 1 && SA.c.st.claims == 0);
    CHECK(SA.c.e[0].brain == 0 || es_find(&SA.c, 0x91)->brain == 0);  // (PC is the puppet now)
    // C: the new owner's game stalls 1.5 s (sends nothing): PC claims it as stale (PC2 yields to that claim on its next frame);
    // PC2 still sees itself 6 m closer, so it claims it back after its ES_RECLAIM_MS + ES_HANDOVER_MS. Never two owners for long.
    t_b = t;
    for (; t < t_b + 1500; t += 16) sim_send(&SA, &SB, t, 1);  // (PC2 does not run; its socket still gets PC's packets)
    CHECK(sim_owner(&SA, 0x91) == 1 && sim_owner(&SB, 0x91) == 1);
    disagree = 0;
    for (; t < t_b + 7000; t += 16) sim_send(&SA, &SB, t, 1), sim_send(&SB, &SA, t, 1), disagree += sim_owner(&SA, 0x91) != sim_owner(&SB, 0x91);
    CHECK(sim_owner(&SA, 0x91) == 2 && sim_owner(&SB, 0x91) == 2 && disagree <= 4 && SA.c.st.claim_refused == 0 && SB.c.st.claim_refused == 0);
    // D: both claim a silent third owner at once (both see themselves nearest): the name tie-break leaves ONE owner on both
    sim_init();
    add_enemy(0x92, 20, 0, 0);
    { EsPlayer pa[3] = {mk(1, "PC", 21), mk(2, "PC2", 25), mk(3, "Cy", 10)}, pb[3] = {mk(2, "PC2", 21), mk(1, "PC", 25), mk(3, "Cy", 10)};
      es_set_players(&SA.c, pa, 3), es_set_players(&SB.c, pb, 3); }
    { EsState s0 = sm(0x92, 20, 0); recv1(&SA.c, 3, s0, 0), recv1(&SB.c, 3, s0, 0); }  // Cy owns it, then goes silent
    for (t = 0; t < 6000; t += 16) sim_send(&SA, &SB, t, 1), sim_send(&SB, &SA, t, 1);
    CHECK(sim_owner(&SA, 0x92) == 1 && sim_owner(&SB, 0x92) == 1);
    // E: startup, each game alone at first (no SNAP of the other yet: both elect themselves), then the real views (PC 1 m nearer):
    // the name tie-break leaves PC, PC2 never claims (not 5 m closer), calm for 10 s
    sim_init();
    add_enemy(0x93, 20, 0, 0);
    { EsPlayer pa[1] = {mk(1, "PC", 20)}, pb[1] = {mk(2, "PC2", 21)}; es_set_players(&SA.c, pa, 1), es_set_players(&SB.c, pb, 1); }
    for (t = 0; t < 500; t += 16) sim_send(&SA, &SB, t, 0), sim_send(&SB, &SA, t, 0);
    CHECK(sim_owner(&SA, 0x93) == 1 && sim_owner(&SB, 0x93) == 2);
    sim_views(20, 21, 21, 20);
    disagree = 0;
    for (; t < 10500; t += 16) sim_send(&SA, &SB, t, 1), sim_send(&SB, &SA, t, 1), disagree += t > 600 && sim_owner(&SA, 0x93) != sim_owner(&SB, 0x93);
    CHECK(sim_owner(&SA, 0x93) == 1 && sim_owner(&SB, 0x93) == 1 && disagree == 0 && SB.c.st.yielded == 1 && SA.c.st.handovers == 0);
    // F: an enemy loaded only on PC2 that PC2 does not own (PC elected it, PC cannot send): PC2 takes it, no ping-pong for 20 s
    sim_init();
    sim_views(20, 60, 60, 20);  // PC is nearer on both views
    { EsState s0 = sm(0x94, 20, 0); recv1(&SA.c, 2, s0, 0); }   // (PC heard of it once)
    es_find(&SA.c, 0x94)->owner = 1;                             // (PC thinks it owns it: elected while it was loaded)
    flips = 0;
    // only PC2 has it loaded: the shared fake exe would load it on both, so run PC2 with it and PC without
    for (t = 0; t < 20000; t += 16) {
        int oa = sim_owner(&SA, 0x94);
        f_n = 0, sim_send(&SA, &SB, t, 1);
        add_enemy(0x94, 20, 0, 0), sim_send(&SB, &SA, t, 1), f_n = 0;
        flips += sim_owner(&SA, 0x94) != oa;
    }
    CHECK(sim_owner(&SA, 0x94) == 2 && sim_owner(&SB, 0x94) == 2 && flips == 1);
}

int main(void) {
    test_codec();
    test_election();
    test_stale();
    test_interp();
    test_misc();
    test_events();
    test_club();
    test_stale_nowhere();
    test_review_fixes();
    test_claim_refused();
    test_two_games();
    printf("esync_test: %d/%d passed\n", runs - fails, runs);
    return fails ? 1 : 0;
}

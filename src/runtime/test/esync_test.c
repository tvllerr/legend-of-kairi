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
static char g_log[16384];
static void test_log(const char *f, ...) {
    va_list a; va_start(a, f);
    size_t n = strlen(g_log);
    vsnprintf(g_log + n, sizeof g_log - n, f, a);
    va_end(a);
}
#define LOG test_log
#include "sora_net_esync.h"
#undef NEAR  // (s195: windows.h, pulled in by the s179 diag timers, defines NEAR as empty: the checks below compared nothing)
#define NEAR(a, b, e) (fabs((double)(a) - (double)(b)) <= (e))

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

// ---- s191 (Melty spec 4.1): players on DIFFERENT maps over plain UDP, NO NET_AREA known (= an old build; task 2's AREA cases are
// test_areas_fixed below). ESYNC itself has no area (NET_VERSION wire unchanged):
// a shrine player's SNAP feet are shrine-local, and shrine / Divine Beast interiors sit near the world origin (s191 data:
// shrine enemies x -65..62 z -116..42, Vah Ruta start 48 6 3), i.e. in central Hyrule's x/z. Each game has its OWN fake exe
// here (what is loaded differs per map). The enemy hids of different maps never collide (recon/S166_HELPER_ENEMYIDS.md:
// 0 shared hids among the 6300 enemy / animal rows of MainField + 41 shrine packs), so the sim uses disjoint hids.
typedef struct {
    uint32_t hid[8]; EsState st[8]; int n;
    int brain[8], apply[8], sens_on[8];  // per loaded enemy: last brain told (-1 never), puppet frames, sensor ONs
    int foreign;                         // brain(0) / puppet_apply / sensor ON for an hid NOT loaded here (must stay 0)
    EsEvent ev[4]; int nev;              // owner-side sensor events the exe "saw" this frame
} AWorld;
static AWorld *g_aw;  // the game whose exe is being called
static int aw_find(uint32_t hid) { for (int i = 0; i < g_aw->n; i++) if (g_aw->hid[i] == hid) return i; return -1; }
static int aw_enum(uint32_t *h, int max) { int n = g_aw->n < max ? g_aw->n : max; memcpy(h, g_aw->hid, sizeof(uint32_t) * (size_t)n); return n; }
static int aw_read(uint32_t hid, EsState *o) { int i = aw_find(hid); if (i < 0) return 0; *o = g_aw->st[i]; return 1; }
static void aw_puppet(uint32_t hid, const EsState *t, int snap) { int i = aw_find(hid); if (i < 0) g_aw->foreign++; else g_aw->apply[i]++; }
static void aw_brain(uint32_t hid, int on) { int i = aw_find(hid); if (i < 0) { if (!on) g_aw->foreign++; } else g_aw->brain[i] = on; }  // (brain(1) of an unloaded one = MF-1, fine)
static int aw_events(EsEvent *o, int max) { int n = g_aw->nev < max ? g_aw->nev : max; memcpy(o, g_aw->ev, sizeof(EsEvent) * (size_t)n); g_aw->nev = 0; return n; }
static void aw_sensor(uint32_t hid, int on, uint32_t name, const uint32_t *p) {  // (OFF of an unloaded hid: the exe ignores it, linkpos_esync_sensor)
    int i = aw_find(hid);
    if (i < 0) { if (on) g_aw->foreign++; } else if (on) g_aw->sens_on[i]++;
}
#define AG_N 3
typedef struct { EsCtx c; uint16_t seq; NetEvSeen seen[AG_N]; AWorld w; float x; bool loading, in; char area[32]; } AreaG;  // in = in the session; area = what its AREA says ("" = none)
static AreaG AG[AG_N];
static const char *ag_name[AG_N] = {"Ann", "Ben", "Cid"};
static void ag_init(void) {
    memset(AG, 0, sizeof AG);
    for (int g = 0; g < AG_N; g++) {
        AG[g].c.io = (EsIface){aw_enum, aw_read, aw_puppet, aw_brain, aw_events, aw_sensor};
        AG[g].c.me = (uint8_t)(g + 1), AG[g].seq = (uint16_t)(100 + 3000 * g);
    }
    g_log[0] = 0;
}
static void ag_add(int g, uint32_t hid, float x) {
    AWorld *w = &AG[g].w;
    memset(&w->st[w->n], 0, sizeof w->st[0]);
    w->hid[w->n] = hid, w->st[w->n].hid = hid, w->st[w->n].pos[0] = x, w->brain[w->n] = -1, w->n++;
}
static int ag_idx(int g, uint32_t hid) { AWorld *s = g_aw; g_aw = &AG[g].w; int i = aw_find(hid); g_aw = s; return i; }
static int ag_owner(int g, uint32_t hid) { EsEnt *e = es_find(&AG[g].c, hid); return e ? e->owner : -9; }
// what each game knows of the players (as net_esync_tick builds it): itself + every peer in the session whose last SNAP is not NF_LOADING
// (s191 task 2) a player whose area is known and not ours is "away" (sora_net_area.h hidden; "" = an old build / area off = never)
static void ag_players(int g) {
    EsPlayer p[AG_N]; int n = 0;
    p[n++] = mk((uint8_t)(g + 1), ag_name[g], AG[g].x);
    for (int h = 0; h < AG_N; h++)
        if (h != g && AG[h].in && !AG[h].loading) p[n] = mk((uint8_t)(h + 1), ag_name[h], AG[h].x), p[n++].away = net_area_apart_txt(AG[g].area, AG[h].area);
    es_set_players(&AG[g].c, p, n);
}
// one frame of every game: step (a loading game does not step: net_esync_tick returns early), its packets to every other game
// (a loading game still receives); stall[g] += frames where a loaded enemy's brain is off but no puppet frame was applied
static int g_stall[AG_N];
static void ag_frame(uint64_t now) {
    for (int g = 0; g < AG_N; g++) if (AG[g].in) ag_players(g);
    for (int g = 0; g < AG_N; g++) {
        if (!AG[g].in || AG[g].loading) continue;
        g_aw = &AG[g].w;
        int ap0[8];
        memcpy(ap0, AG[g].w.apply, sizeof ap0);
        uint64_t was = AG[g].c.next_send;
        es_step(&AG[g].c, now, &O);
        for (int i = 0; i < AG[g].w.n; i++) if (AG[g].w.brain[i] == 0 && AG[g].w.apply[i] == ap0[i]) g_stall[g]++;
        g_npk = 0;
        es_packets(&AG[g].c, &O, AG[g].c.next_send != was, AG[g].c.me, &AG[g].seq, collect, NULL);
        for (int k = 0; k < g_npk; k++)
            for (int h = 0; h < AG_N; h++) {
                if (h == g || !AG[h].in) continue;
                NetEsEnt e[NET_ESYNC_MAX]; NetEsEv v[NET_ESYNC_EVMAX]; int ne = 0, nv = 0; uint8_t pl; uint16_t sq;
                g_aw = &AG[h].w;
                if (net_unpack_esync(g_pk[k].b, g_pk[k].len, &pl, &sq, e, &ne, v, &nv)) es_on_recv(&AG[h].c, pl, e, ne, v, nv, &AG[h].seen[g], now);
            }
    }
}
#define S_SHR 0x5001u   // a shrine enemy (Ann's map only)
#define E_CAMP 0x6001u  // an overworld camp enemy (Ben's / Cid's map only)
static void test_areas(void) {
    // Q1: Ann in a shrine owns its enemy, Ben in the overworld owns his camp enemy, far apart even in coordinates. 10 s of traffic
    // both ways + a sensor event: neither game ever drives, stops or sensors an enemy that is not loaded there.
    ag_init();
    AG[0].in = AG[1].in = true, AG[0].x = 500, AG[1].x = 30;
    ag_add(0, S_SHR, 502), ag_add(1, E_CAMP, 12);
    uint64_t t = 0;
    for (; t < 10000; t += 16) {
        if (t == 2000) AG[0].w.ev[0] = (EsEvent){.hid = S_SHR, .on = 1, .name = 0xF00D}, AG[0].w.nev = 1;
        ag_frame(t);
    }
    CHECK(ag_owner(0, S_SHR) == 1 && ag_owner(1, S_SHR) == 1 && ag_owner(1, E_CAMP) == 2 && ag_owner(0, E_CAMP) == 2);
    CHECK(AG[0].w.foreign == 0 && AG[1].w.foreign == 0 && AG[0].c.st.tx_ev == NET_EVENT_REPEAT && AG[1].c.st.rx_ev_nolocal == 1 && AG[1].c.st.rx_ev_dup == NET_EVENT_REPEAT - 1);
    CHECK(AG[0].w.brain[0] == 1 && AG[1].w.brain[0] == 1 && AG[0].w.apply[0] == 0 && AG[1].w.apply[0] == 0);  // (both run their own brains)
    CHECK(AG[0].c.st.claims == 0 && AG[1].c.st.claims == 0 && AG[0].c.st.stale == 0 && AG[1].c.st.stale == 0);
    // ... Ann leaves the session (her packets stop): Ben never claims her shrine enemy (not loaded here), it leaves his table after ES_FORGET_MS
    AG[0].in = false;
    for (; t < 10000 + ES_FORGET_MS + 500; t += 16) ag_frame(t);
    CHECK(es_find(&AG[1].c, S_SHR) == NULL && AG[1].c.st.claims == 0 && AG[1].w.foreign == 0 && AG[1].w.brain[0] == 1);

    // Q2: Ann's shrine-local feet are 1 m from Ben's camp enemy; Ben is 18 m from it. Ben's fresh election names Ann (nearest), his
    // brain keeps running (no samples come), after ES_STALE_MS he claims it (two-phase: Ann is silent, done after 300 ms). From then
    // on Ben keeps it for good: Ann never claims (not loaded there) and nobody claims it for her. No frame with the brain off.
    ag_init();
    AG[0].in = AG[1].in = true, AG[0].x = 11, AG[1].x = 30;
    ag_add(0, S_SHR, 10), ag_add(1, E_CAMP, 12);
    memset(g_stall, 0, sizeof g_stall);
    int flips = 0, first_ben = -1;
    for (t = 0; t < 20000; t += 16) {
        int o = ag_owner(1, E_CAMP);
        ag_frame(t);
        if (t == 0) CHECK(ag_owner(1, E_CAMP) == 1 && strstr(g_log, "hid=00006001 owner none -> Ann"));
        if (o >= 0 && ag_owner(1, E_CAMP) != o) flips++;
        if (first_ben < 0 && ag_owner(1, E_CAMP) == 2) first_ben = (int)t;
        AG[0].x = 11.0f + 0.5f * sinf((float)t * 0.001f);  // (she walks about in her shrine, always right "at" the camp)
    }
    CHECK(first_ben >= ES_STALE_MS && first_ben <= ES_STALE_MS + 100 && flips == 1 && ag_owner(1, E_CAMP) == 2 && ag_owner(0, E_CAMP) == 2);
    CHECK(g_stall[1] == 0 && AG[1].w.brain[0] == 1 && AG[1].w.apply[0] == 0 && AG[1].c.st.claims == 1 && AG[1].c.st.claim_done == 1 && AG[1].c.st.claim_refused == 0);
    CHECK(AG[0].c.st.claims == 0 && AG[0].w.foreign == 0 && AG[1].w.foreign == 0 && strstr(g_log, "Ann -> Ben (stale owner: claim)"));

    // Q2, three games: Ann (shrine) 1 m, Ben 18 m, Cid 23 m; Ben and Cid have the camp loaded. Both elect Ann first, both brains run;
    // after 1 s Ben (nearest after Ann) claims, Cid follows his claim and becomes a real puppet. Calm after 2 s, never a stall.
    ag_init();
    AG[0].in = AG[1].in = AG[2].in = true, AG[0].x = 11, AG[1].x = 30, AG[2].x = 35;
    ag_add(0, S_SHR, 10), ag_add(1, E_CAMP, 12), ag_add(2, E_CAMP, 12);
    memset(g_stall, 0, sizeof g_stall);
    int dis = 0;
    for (t = 0; t < 20000; t += 16) {
        ag_frame(t);
        if (t > 2000) dis += ag_owner(1, E_CAMP) != 2 || ag_owner(2, E_CAMP) != 2 || ag_owner(0, E_CAMP) != 2;
    }
    CHECK(dis == 0 && g_stall[1] == 0 && g_stall[2] == 0 && AG[2].w.brain[0] == 0 && AG[2].w.apply[0] > 1000 && AG[1].w.brain[0] == 1);
    CHECK(AG[0].c.st.claims == 0 && AG[2].c.st.claims == 0 && AG[1].c.st.claims == 1 && AG[0].w.foreign == 0 && AG[1].w.foreign == 0 && AG[2].w.foreign == 0);
    // ... Cid walks up to 2 m (16 m closer than Ben). With Ann's phantom at 0 m he is NOT the nearest player, so he never claims it
    // as "closer": Ben (18 m) keeps running it. Not a stall (Ben runs it, Cid's puppet follows), only the hand-over to the truly
    // nearest player is blocked while the phantom stands there.
    AG[0].x = 12, AG[2].x = 14;
    uint64_t tb = t;
    for (; t < tb + 10000; t += 16) ag_frame(t);
    CHECK(ag_owner(1, E_CAMP) == 2 && ag_owner(2, E_CAMP) == 2 && AG[2].c.st.claims == 0 && g_stall[2] == 0);
    // ... control: the same with Ann gone (she left the session): Cid claims it as closer after ES_HANDOVER_MS
    AG[0].in = false;
    tb = t;
    int cid_at = -1;
    for (; t < tb + 6000; t += 16) {
        ag_frame(t);
        if (cid_at < 0 && ag_owner(2, E_CAMP) == 3) cid_at = (int)(t - tb);
    }
    CHECK(cid_at >= ES_HANDOVER_MS && cid_at <= ES_HANDOVER_MS + 100 && ag_owner(1, E_CAMP) == 3 && AG[2].c.st.claims == 1);

    // Q2, the owner leaves: Ben (18 m) owns the camp, Cid 23 m, Ann's phantom 1 m. Ben goes into a loading screen (he left the area):
    // Cid's nearest after Ben is the phantom, so Cid waits the 3 s fallback (instead of 1 s, counted from Ben's last sample) and then
    // claims. Never a stall: his puppet holds on Ben's last samples for ES_STALE_MS, then his own brain runs.
    ag_init();
    AG[0].in = AG[1].in = AG[2].in = true, AG[0].x = 11, AG[1].x = 30, AG[2].x = 35;
    ag_add(0, S_SHR, 10), ag_add(1, E_CAMP, 12), ag_add(2, E_CAMP, 12);
    for (t = 0; t < 4000; t += 16) ag_frame(t);
    CHECK(ag_owner(2, E_CAMP) == 2);
    memset(g_stall, 0, sizeof g_stall);
    AG[1].loading = true, AG[1].w.n = 0;
    tb = t, cid_at = -1;
    for (; t < tb + 8000; t += 16) {
        ag_frame(t);
        if (cid_at < 0 && ag_owner(2, E_CAMP) == 3) cid_at = (int)(t - tb);
    }
    CHECK(cid_at >= 3 * ES_STALE_MS - 100 && cid_at <= 3 * ES_STALE_MS + 100 && ag_owner(2, E_CAMP) == 3 && g_stall[2] == 0 && AG[2].w.brain[0] == 1);
    CHECK(AG[0].w.foreign == 0 && AG[2].w.foreign == 0);

    // Q2, the owner goes INTO a shrine: Ann owns the camp (she stands 1 m from it, Ben is a puppet 18 m away), then loads into a shrine
    // (NF_LOADING: not a player for Ben) and comes back with shrine-local feet 0.5 m from the camp's coordinates. Ben claims at once
    // ("old owner gone"; she is "player1" in his log: not a player while loading), Ann's stale "mine" yields to his claim, Ben keeps it for 20 s while her phantom stands there.
    ag_init();
    AG[0].in = AG[1].in = true, AG[0].x = 13, AG[1].x = 30;
    ag_add(0, E_CAMP, 12), ag_add(1, E_CAMP, 12);
    for (t = 0; t < 3000; t += 16) ag_frame(t);
    CHECK(ag_owner(0, E_CAMP) == 1 && ag_owner(1, E_CAMP) == 1 && AG[1].w.brain[0] == 0 && AG[1].w.apply[0] > 100);
    AG[0].loading = true, AG[0].w.n = 0;
    tb = t;
    int ben_at = -1;
    for (; t < tb + 4000; t += 16) {
        ag_frame(t);
        if (ben_at < 0 && ag_owner(1, E_CAMP) == 2) ben_at = (int)(t - tb);
    }
    CHECK(ben_at >= 0 && ben_at <= 50 && strstr(g_log, "player1 -> Ben (old owner gone: claim)") && ag_owner(0, E_CAMP) == 2 && AG[0].c.st.claim_yield == 1);
    AG[0].loading = false, AG[0].x = 12.5f;
    ag_add(0, S_SHR, 10);
    memset(g_stall, 0, sizeof g_stall);
    flips = 0;
    for (tb = t; t < tb + 20000; t += 16) {
        int o = ag_owner(1, E_CAMP);
        ag_frame(t);
        flips += ag_owner(1, E_CAMP) != o;
    }
    CHECK(flips == 0 && ag_owner(1, E_CAMP) == 2 && ag_owner(0, E_CAMP) == 2 && g_stall[1] == 0 && AG[1].w.brain[0] == 1);
    CHECK(AG[0].c.st.claims == 0 && AG[0].w.foreign == 0 && AG[1].w.foreign == 0 && ag_owner(1, S_SHR) == 1);
}

// (s191 task 2) the same with NET_AREA known: a player on another map is no player in the election (sora_net_esync.h glue,
// sl->hidden). Ann "Dungeon012", Ben + Cid "MainField". (test_areas above = Ann an old build: no AREA = today's behaviour.)
static bool log_line2(const char *a, const char *b) {  // one log line holds both
    for (const char *p = g_log; p && *p;) {
        const char *e = strchr(p, '\n'), *x = strstr(p, a), *y = strstr(p, b);
        if (x && y && (!e || (x < e && y < e))) return true;
        p = e ? e + 1 : NULL;
    }
    return false;
}
static void ag_set_area(int g, const char *a) { snprintf(AG[g].area, sizeof AG[g].area, "%s", a); }
// one 3-game run of the camp (Ann's phantom 1 m, Ben 18 m, Cid 23 m), Cid walks up at 4 s; trace = Cid's owner each frame
static int g_trace[2000];
static void ag_run3(const char *aa, const char *ab, const char *ac, int *n) {
    ag_init();
    AG[0].in = AG[1].in = AG[2].in = true, AG[0].x = 11, AG[1].x = 30, AG[2].x = 35;
    ag_set_area(0, aa), ag_set_area(1, ab), ag_set_area(2, ac);
    ag_add(0, S_SHR, 10), ag_add(1, E_CAMP, 12), ag_add(2, E_CAMP, 12);
    memset(g_stall, 0, sizeof g_stall);
    *n = 0;
    for (uint64_t t = 0; t < 12000; t += 16) {
        if (t == 4000) AG[2].x = 14;
        ag_frame(t);
        if (*n < 2000) g_trace[(*n)++] = ag_owner(2, E_CAMP) * 16 + ag_owner(1, E_CAMP);
    }
}
static void test_areas_fixed(void) {
    // same map for everyone ("MainField" on all three) = exactly what no AREA at all gives: the same owners frame by frame
    static int t1[2000];
    int n1, n2;
    ag_run3("", "", "", &n1);
    memcpy(t1, g_trace, sizeof t1);
    EsStats s1 = AG[2].c.st;
    ag_run3("MainField", "MainField", "MainField", &n2);
    CHECK(n1 == n2 && !memcmp(t1, g_trace, sizeof(int) * (size_t)n1) && !memcmp(&s1, &AG[2].c.st, sizeof s1));
    ag_run3("Dungeon012", "Dungeon012", "Dungeon012", &n2);  // (all three in the same shrine: the same again)
    CHECK(n1 == n2 && !memcmp(t1, g_trace, sizeof(int) * (size_t)n1) && !memcmp(&s1, &AG[2].c.st, sizeof s1));
    // Ann in her shrine: never elected (Ben owns it from the first frame), and Cid walking up to 2 m claims it as "closer" after
    // ES_HANDOVER_MS although her phantom stands at 1 m (task 1 side effect A, fixed)
    ag_run3("Dungeon012", "MainField", "MainField", &n2);
    CHECK(!log_line2("hid=00006001", "-> Ann") && g_trace[0] == 2 * 16 + 2 && ag_owner(1, E_CAMP) == 3 && ag_owner(2, E_CAMP) == 3 && AG[2].c.st.claims == 1);
    CHECK(g_stall[1] == 0 && g_stall[2] == 0 && AG[0].c.st.claims == 0 && AG[0].w.foreign == 0 && AG[1].w.foreign == 0 && AG[2].w.foreign == 0);
    int cid_at = -1;
    for (int i = 0; i < n2 && cid_at < 0; i++) if (g_trace[i] / 16 == 3) cid_at = i * 16 - 4000;
    CHECK(cid_at >= ES_HANDOVER_MS && cid_at <= ES_HANDOVER_MS + 100);
    // Ben (the owner) loads out: Cid claims at once ("old owner gone"; the phantom is nobody's nearest), not after the 3 s fallback
    // (side effect B, fixed)
    ag_init();
    AG[0].in = AG[1].in = AG[2].in = true, AG[0].x = 11, AG[1].x = 30, AG[2].x = 35;
    ag_set_area(0, "Dungeon012"), ag_set_area(1, "MainField"), ag_set_area(2, "MainField");
    ag_add(0, S_SHR, 10), ag_add(1, E_CAMP, 12), ag_add(2, E_CAMP, 12);
    uint64_t t = 0, tb;
    for (; t < 4000; t += 16) ag_frame(t);
    CHECK(ag_owner(2, E_CAMP) == 2);
    memset(g_stall, 0, sizeof g_stall);
    AG[1].loading = true, AG[1].w.n = 0;
    cid_at = -1;
    for (tb = t; t < tb + 8000; t += 16) {
        ag_frame(t);
        if (cid_at < 0 && ag_owner(2, E_CAMP) == 3) cid_at = (int)(t - tb);
    }
    CHECK(cid_at >= 0 && cid_at <= 50 && log_line2("hid=00006001", "-> Cid (old owner gone: claim)") && g_stall[2] == 0 && AG[2].w.brain[0] == 1);
    // the owner walks INTO a shrine and back: Ann owns the camp (Ben puppets it), loads into Dungeon012 (Ben claims at once, she is
    // hidden for him while she is in there), then loads back out next to the camp: a player again, 17 m closer than Ben, she claims
    // it back as "closer"; Ben yields. Never a stall on either side.
    ag_init();
    AG[0].in = AG[1].in = true, AG[0].x = 13, AG[1].x = 30;
    ag_set_area(0, "MainField"), ag_set_area(1, "MainField");
    ag_add(0, E_CAMP, 12), ag_add(1, E_CAMP, 12);
    for (t = 0; t < 3000; t += 16) ag_frame(t);
    CHECK(ag_owner(0, E_CAMP) == 1 && ag_owner(1, E_CAMP) == 1 && AG[1].w.brain[0] == 0);
    memset(g_stall, 0, sizeof g_stall);
    AG[0].loading = true, AG[0].w.n = 0;
    for (tb = t; t < tb + 3000; t += 16) ag_frame(t);
    CHECK(ag_owner(1, E_CAMP) == 2 && ag_owner(0, E_CAMP) == 2);
    AG[0].loading = false, AG[0].x = 12.5f, ag_set_area(0, "Dungeon012");
    ag_add(0, S_SHR, 10);
    for (tb = t; t < tb + 10000; t += 16) ag_frame(t);
    CHECK(ag_owner(1, E_CAMP) == 2 && AG[1].c.np == 2 && AG[1].c.pl[1].away && AG[0].c.pl[1].away && AG[1].w.brain[0] == 1 && g_stall[1] == 0);
    CHECK(ag_owner(0, E_CAMP) == 2 && AG[0].c.st.claims == 0);  // (her table keeps following his samples: no stale claim when she is back)
    AG[0].loading = true, AG[0].w.n = 0;
    for (tb = t; t < tb + 2000; t += 16) ag_frame(t);
    AG[0].loading = false, AG[0].x = 13, ag_set_area(0, "MainField");
    ag_add(0, E_CAMP, 12);
    int ann_at = -1;
    for (tb = t; t < tb + 6000; t += 16) {
        ag_frame(t);
        if (ann_at < 0 && ag_owner(1, E_CAMP) == 1 && ag_owner(0, E_CAMP) == 1) ann_at = (int)(t - tb);
    }
    CHECK(ann_at >= ES_HANDOVER_MS && ann_at <= ES_HANDOVER_MS + 500 && AG[1].w.brain[0] == 0 && AG[1].w.apply[0] > 100 && g_stall[0] == 0 && g_stall[1] == 0);
    CHECK(AG[0].w.foreign == 0 && AG[1].w.foreign == 0);
}

// ---- s195 eshit: the hit claim (my Sora hit an enemy another game runs -> I take it at once), held / sticky owners ----
static void recvf(EsCtx *c, uint8_t from, EsState s, uint16_t fl, uint64_t now) { s.flags |= fl; recv1(c, from, s, now); }
static int own(EsCtx *c, uint32_t hid) { EsEnt *e = es_find(c, hid); return e ? e->owner : -9; }
static void test_hit(void) {
    EsCtx c; fake_reset(&c);
    c.hit_on = true;
    add_enemy(0x5A1, 20, 0, 0);
    players2(&c, 40, 18);  // me 20 m, Bob 2 m: Bob runs it, I am nowhere near "closer"
    uint64_t t = 0;
    for (; t <= 500; t += 50) recv1(&c, 2, sm(0x5A1, 20, 0), t), step(&c, t);
    CHECK(own(&c, 0x5A1) == 2 && f_last_brain[f_brain_calls - 1] == 0 && c.st.claims == 0);
    // one Sora hit: I claim it on the next step (no 5 m / 1 s wait); my samples carry CLAIM + HELD + HOT
    es_note_hit(&c, 0x5A1, 520);
    CHECK(c.st.hit_notes == 1 && es_find(&c, 0x5A1)->hit_ms == 520);
    step(&c, 525);
    CHECK(own(&c, 0x5A1) == 1 && strstr(g_log, "hid=000005a1 owner Bob -> Alpha (hit: claim)") && c.st.hit_claims == 1 && c.st.claims == 1 && es_find(&c, 0x5A1)->claim_from == 2);
    uint16_t fl0 = 0xffff, fl_hot_end = 0xffff, fl_held_end = 0xffff;
    for (t = 550; t <= 5000; t += 50) {
        if (step(&c, t) != 1) continue;
        if (fl0 == 0xffff) fl0 = O.ent[0].flags;
        if (t == 2050) fl_hot_end = O.ent[0].flags;   // (1530 ms after the hit: HOT gone, HELD still on, claim done)
        if (t == 4550) fl_held_end = O.ent[0].flags;  // (4030 ms: nothing)
    }
    CHECK(fl0 == (ES_FL_CLAIM | ES_FL_HELD | ES_FL_HOT) && fl_hot_end == ES_FL_HELD && fl_held_end == 0 && c.st.claim_done == 1 && c.st.tx_held > 0);
    // live hold: eshitms 1000 = HELD only for 1 s (HOT stays 1.5 s: it is the sticky rule, not the hold)
    c.hit_hold_ms = 1000;
    es_note_hit(&c, 0x5A1, 5000);
    uint16_t fh = 0xffff;
    for (t = 5050; t <= 6200; t += 50) if (step(&c, t) == 1 && t == 6100) fh = O.ent[0].flags;
    CHECK(fh == ES_FL_HOT);
    // eshit 0 (the compiled default): hits change nothing, however many
    fake_reset(&c);
    add_enemy(0x5A2, 20, 0, 0);
    players2(&c, 40, 18);
    for (t = 0; t <= 5000; t += 50) { recv1(&c, 2, sm(0x5A2, 20, 0), t); if (t % 200 == 0) es_note_hit(&c, 0x5A2, t); step(&c, t); }
    CHECK(own(&c, 0x5A2) == 2 && c.st.claims == 0 && c.st.hit_claims == 0 && c.st.hit_notes == 26 && !strstr(g_log, "(hit"));
    // not loaded here: no claim (we could not send it)
    fake_reset(&c);
    c.hit_on = true;
    players2(&c, 40, 18);
    for (t = 0; t <= 3000; t += 50) recv1(&c, 2, sm(0x5A3, 20, 0), t), es_note_hit(&c, 0x5A3, t), step(&c, t);
    CHECK(own(&c, 0x5A3) == 2 && c.st.claims == 0 && c.st.hit_notes == 61);
    // an enemy not in the table yet: the note is counted, nothing stored, nothing breaks
    es_note_hit(&c, 0x5FF, 3000);
    CHECK(es_find(&c, 0x5FF) == NULL && c.st.hit_notes == 62);
    // banned (we yielded to his claim ES_RECLAIM_MS ago): no hit claim; a hit that is old (> ES_HIT_FRESH_MS) when the ban ends never
    // claims; the next fresh hit does
    fake_reset(&c);
    c.hit_on = true;
    add_enemy(0x5A4, 20, 0, 0);
    players2(&c, 22, 18);  // 2 m each: the tie-break gives it to me (Alpha < Bob), never 5 m closer
    step(&c, 0);
    CHECK(own(&c, 0x5A4) == 1);
    claim1(&c, 2, sm(0x5A4, 20, 0), 100);
    CHECK(own(&c, 0x5A4) == 2);
    uint64_t ban_end = 100 + ES_RECLAIM_MS;
    for (t = 150; t < ban_end + 1000; t += 50) {
        recv1(&c, 2, sm(0x5A4, 20, 0), t);
        if (t % 300 == 0 && t < ban_end - ES_HIT_FRESH_MS - 100) es_note_hit(&c, 0x5A4, t);
        step(&c, t);
    }
    CHECK(own(&c, 0x5A4) == 2 && c.st.claims == 0 && c.st.hit_notes > 3);
    es_note_hit(&c, 0x5A4, t);
    recv1(&c, 2, sm(0x5A4, 20, 0), t), step(&c, t);
    CHECK(own(&c, 0x5A4) == 1 && c.st.hit_claims == 1);
    // a refused claim bans hit claims too (ES_CLAIM_BAN_MS)
    for (uint64_t t2 = t + 50; t2 <= t + ES_CLAIM_MS + 100; t2 += 50) recv1(&c, 2, sm(0x5A4, 20, 0), t2), step(&c, t2);  // (he kept sending: old DLL)
    CHECK(own(&c, 0x5A4) == 2 && c.st.claim_refused == 1);
    t += ES_CLAIM_MS + 150;
    es_note_hit(&c, 0x5A4, t), recv1(&c, 2, sm(0x5A4, 20, 0), t), step(&c, t);
    CHECK(own(&c, 0x5A4) == 2 && c.st.hit_claims == 1);

    // HELD blocks "closer" claims while it lasts (counted once), then the normal 1 s wait starts
    fake_reset(&c);
    c.hit_on = true;
    add_enemy(0x5A5, 20, 0, 0);
    players2(&c, 22, 10);  // me 2 m, Bob 10 m: 8 m closer
    for (t = 0; t <= 3000; t += 50) recvf(&c, 2, sm(0x5A5, 20, 0), ES_FL_HELD, t), step(&c, t);
    CHECK(own(&c, 0x5A5) == 2 && c.st.claims == 0 && c.st.closer_blocked == 1 && es_find(&c, 0x5A5)->o_held);
    CHECK(f_apply_n > 0 && (f_apply_last.flags & 0xE000) == 0 && es_find(&c, 0x5A5)->sm[es_find(&c, 0x5A5)->nsm - 1].s.flags == 0);  // (never reaches the puppet)
    uint64_t t0 = t;
    int at = -1;
    for (; t <= t0 + 2000; t += 50) { if (own(&c, 0x5A5) == 2) recv1(&c, 2, sm(0x5A5, 20, 0), t); step(&c, t); if (at < 0 && own(&c, 0x5A5) == 1) at = (int)(t - t0); }
    CHECK(at == ES_HANDOVER_MS && strstr(g_log, "Bob -> Alpha (closer: claim)") && c.st.hit_claims == 0);
    // eshit 0: HELD (and HOT) samples block nothing = today; the bits are stripped all the same
    fake_reset(&c);
    add_enemy(0x5A6, 20, 0, 0);
    players2(&c, 22, 10);
    at = -1;
    for (t = 0; t <= 2000; t += 50) { if (own(&c, 0x5A6) != 1) recvf(&c, 2, sm(0x5A6, 20, 0), ES_FL_HELD | ES_FL_HOT, t); step(&c, t); if (at < 0 && own(&c, 0x5A6) == 1) at = (int)t; }
    CHECK(at == ES_HANDOVER_MS && c.st.closer_blocked == 0 && f_apply_n > 0 && (f_apply_last.flags & 0xE000) == 0);
    // a hit on a HELD (not HOT) owner = claims (the owner stopped hitting ES_HIT_STICKY_MS ago)
    fake_reset(&c);
    c.hit_on = true;
    add_enemy(0x5A8, 20, 0, 0);
    players2(&c, 40, 18);
    for (t = 0; t <= 500; t += 50) recvf(&c, 2, sm(0x5A8, 20, 0), ES_FL_HELD, t), step(&c, t);
    es_note_hit(&c, 0x5A8, 510), step(&c, 520);
    CHECK(own(&c, 0x5A8) == 1 && c.st.hit_claims == 1 && c.st.hit_blocked == 0);

    // STICKY ("stays with the first"): the owner keeps hitting (HOT): my hits never claim, each blocked hit counted once ...
    fake_reset(&c);
    c.hit_on = true;
    add_enemy(0x5A7, 20, 0, 0);
    players2(&c, 40, 18);
    for (t = 0; t <= 3000; t += 50) { recvf(&c, 2, sm(0x5A7, 20, 0), ES_FL_HELD | ES_FL_HOT, t); if (t % 200 == 0) es_note_hit(&c, 0x5A7, t); step(&c, t); }
    CHECK(own(&c, 0x5A7) == 2 && c.st.claims == 0 && c.st.hit_blocked == 16 && c.st.hit_claims == 0);
    // ... he stops at 3000 (HOT off in his samples from 4500); I keep hitting: I claim with the first sample without HOT
    at = -1;
    for (t = 3050; t <= 6000; t += 50) {
        if (own(&c, 0x5A7) == 2) recvf(&c, 2, sm(0x5A7, 20, 0), t < 3000 + ES_HIT_STICKY_MS ? ES_FL_HELD | ES_FL_HOT : ES_FL_HELD, t);
        if (t % 200 == 0) es_note_hit(&c, 0x5A7, t);
        step(&c, t);
        if (at < 0 && own(&c, 0x5A7) == 1) at = (int)t;
    }
    CHECK(at == 3000 + ES_HIT_STICKY_MS && strstr(g_log, "Bob -> Alpha (hit: claim)") && c.st.hit_claims == 1 && c.st.claims == 1);
    // ... and if I had stopped hitting > ES_HIT_FRESH_MS before his HOT ended, nothing happens (a claim follows a RECENT hit)
    fake_reset(&c);
    c.hit_on = true;
    add_enemy(0x5A9, 20, 0, 0);
    players2(&c, 40, 18);
    for (t = 0; t <= 6000; t += 50) {
        recvf(&c, 2, sm(0x5A9, 20, 0), t < 4000 ? ES_FL_HELD | ES_FL_HOT : ES_FL_HELD, t);
        if (t == 2000) es_note_hit(&c, 0x5A9, t);
        step(&c, t);
    }
    CHECK(own(&c, 0x5A9) == 2 && c.st.claims == 0 && c.st.hit_blocked == 1);
}

// two games (real codec): a hit claim, the sticky rule across the wire, then HELD keeping a "closer" claim off for the hold
static void test_hit_two_games(void) {
    sim_init();
    SA.c.hit_on = SB.c.hit_on = true;
    add_enemy(0x9A, 20, 0, 0);
    sim_views(21, 36, 36, 21);  // PC 1 m, PC2 16 m: PC runs it
    uint64_t t = 0;
    for (; t < 1000; t += 16) sim_send(&SA, &SB, t, 1), sim_send(&SB, &SA, t, 1);
    CHECK(sim_owner(&SA, 0x9A) == 1 && sim_owner(&SB, 0x9A) == 1);
    // A: PC2's Sora hits it once: PC2 claims at once, PC yields on the claim sample
    uint64_t t0 = t, b_hit = t;
    es_note_hit(&SB.c, 0x9A, t);
    int b_at = -1;
    for (; t < t0 + 1000; t += 16) {
        sim_send(&SA, &SB, t, 1), sim_send(&SB, &SA, t, 1);
        if (b_at < 0 && sim_owner(&SA, 0x9A) == 2 && sim_owner(&SB, 0x9A) == 2) b_at = (int)(t - t0);
    }
    CHECK(b_at >= 0 && b_at <= 16 && SB.c.st.hit_claims == 1 && SA.c.st.claim_yield == 1 && SB.c.st.claim_done == 1 && SA.c.st.claim_refused == 0 && SB.c.st.claim_refused == 0);
    CHECK(strstr(g_log, "owner PC -> PC2 (hit: claim)") && es_find(&SA.c, 0x9A)->o_held && es_find(&SA.c, 0x9A)->o_hot);
    EsEnt *pa = es_find(&SA.c, 0x9A);
    CHECK(pa->nsm > 0 && pa->sm[pa->nsm - 1].s.flags == 0 && pa->brain == 0);  // (PC is the puppet; no wire bit reaches it)
    // B: both keep hitting for 6 s (PC2 every 400 ms, PC every 300 ms): PC is banned 3 s (it yielded), then PC2 is HOT: no claim;
    // PC is 15 m closer, but PC2 is HELD: no "closer" claim either
    int flips = 0;
    for (; t < t0 + 6000; t += 16) {
        int o = sim_owner(&SA, 0x9A);
        if ((t - t0) % 400 < 16) es_note_hit(&SB.c, 0x9A, t), b_hit = t;
        if ((t - t0) % 300 < 16) es_note_hit(&SA.c, 0x9A, t);
        sim_send(&SA, &SB, t, 1), sim_send(&SB, &SA, t, 1);
        flips += sim_owner(&SA, 0x9A) != o;
    }
    CHECK(flips == 0 && sim_owner(&SA, 0x9A) == 2 && sim_owner(&SB, 0x9A) == 2 && SA.c.st.claims == 0 && SA.c.st.hit_blocked > 5 && SA.c.st.closer_blocked >= 1);
    // C: PC2 stops hitting; PC keeps on: PC claims ES_HIT_STICKY_MS after PC2's last hit (+ one sample / frame)
    int a_at = -1;
    uint64_t t1 = t;
    for (; t < t1 + 4000; t += 16) {
        if ((t - t0) % 300 < 16) es_note_hit(&SA.c, 0x9A, t);
        sim_send(&SA, &SB, t, 1), sim_send(&SB, &SA, t, 1);
        if (a_at < 0 && sim_owner(&SA, 0x9A) == 1 && sim_owner(&SB, 0x9A) == 1) a_at = (int)(t - b_hit);
    }
    CHECK(a_at >= ES_HIT_STICKY_MS && a_at <= ES_HIT_STICKY_MS + 70 && SA.c.st.hit_claims == 1 && SB.c.st.claim_yield == 1 && strstr(g_log, "owner PC2 -> PC (hit: claim)"));
    // D: PC stops hitting; PC2 walks up (1 m, PC 16 m): PC2 waits PC's hold (4 s after PC's last hit), then the normal 1 s "closer"
    uint64_t a_hit = 0;
    for (uint64_t k = t1; k < t; k += 16) if ((k - t0) % 300 < 16) a_hit = k;
    sim_views(36, 21, 21, 36);
    b_at = -1;
    uint64_t t2 = t;
    for (; t < t2 + 8000; t += 16) {
        sim_send(&SA, &SB, t, 1), sim_send(&SB, &SA, t, 1);
        if (b_at < 0 && sim_owner(&SA, 0x9A) == 2 && sim_owner(&SB, 0x9A) == 2) b_at = (int)(t - a_hit);
    }
    CHECK(b_at >= ES_HIT_HOLD_MS + ES_HANDOVER_MS && b_at <= ES_HIT_HOLD_MS + ES_HANDOVER_MS + 100 && SB.c.st.closer_blocked >= 1 && strstr(g_log, "owner PC -> PC2 (closer: claim)"));
    // E: calm afterwards, never two owners for long, nothing refused
    flips = 0;
    int dis = 0;
    for (uint64_t t3 = t; t < t3 + 5000; t += 16) {
        int o = sim_owner(&SA, 0x9A);
        sim_send(&SA, &SB, t, 1), sim_send(&SB, &SA, t, 1);
        flips += sim_owner(&SA, 0x9A) != o, dis += sim_owner(&SA, 0x9A) != sim_owner(&SB, 0x9A);
    }
    CHECK(flips == 0 && dis == 0 && SA.c.st.claim_refused == 0 && SB.c.st.claim_refused == 0);
}

// eshit 0 = the bytes on the wire are exactly today's, whatever the hits; eshit 1 without hits = the same bytes too
static uint8_t g_rec[3][400000]; static int g_nrec[3];
static void sim_rec(int r, uint64_t now, SimG *a, SimG *b) {
    sim_send(a, b, now, 1);
    for (int k = 0; k < g_npk; k++) {
        if (g_nrec[r] + g_pk[k].len > (int)sizeof g_rec[r]) return;
        memcpy(g_rec[r] + g_nrec[r], g_pk[k].b, (size_t)g_pk[k].len), g_nrec[r] += g_pk[k].len;
    }
}
static int wire_bits(int r) {  // entries with a s195 bit (0x6000) in recording r
    int n = 0, off = 0;
    while (off < g_nrec[r]) {
        int len = NET_HDR + 2 + 40 * g_rec[r][off + 7] + 26 * g_rec[r][off + 8];
        NetEsEnt e[NET_ESYNC_MAX]; NetEsEv v[NET_ESYNC_EVMAX]; int ne = 0, nv = 0;
        if (net_unpack_esync(g_rec[r] + off, len, NULL, NULL, e, &ne, v, &nv)) for (int i = 0; i < ne; i++) n += (e[i].flags & 0x6000) != 0;
        off += len;
    }
    return n;
}
static void wire_run(int r, bool on, bool hits) {
    sim_init();
    SA.c.hit_on = SB.c.hit_on = on;
    g_nrec[r] = 0;
    add_enemy(0x9B, 20, 0, 0), add_enemy(0x9C, 30, 0, 0);
    sim_views(21, 40, 40, 21);
    for (uint64_t t = 0; t < 12000; t += 16) {
        if (t == 4000) sim_views(40, 22, 22, 40);  // (PC2 walks up: "closer" hand-overs of both)
        if (hits && t % 250 < 16) es_note_hit(&SA.c, 0x9B, t), es_note_hit(&SB.c, 0x9C, t);
        sim_rec(r, t, &SA, &SB), sim_rec(r, t, &SB, &SA);
    }
}
static void test_hit_wire(void) {
    wire_run(0, false, false);   // (= today: nobody calls es_note_hit)
    wire_run(1, false, true);    // eshit 0, hits
    CHECK(g_nrec[0] > 10000 && g_nrec[0] == g_nrec[1] && !memcmp(g_rec[0], g_rec[1], (size_t)g_nrec[0]) && wire_bits(0) == 0 && wire_bits(1) == 0);
    wire_run(1, true, false);    // eshit 1, no hits
    CHECK(g_nrec[0] == g_nrec[1] && !memcmp(g_rec[0], g_rec[1], (size_t)g_nrec[0]));
    wire_run(2, true, true);     // (control: eshit 1 + hits does change the bytes: the test can see it)
    CHECK(wire_bits(2) > 100 && (g_nrec[2] != g_nrec[0] || memcmp(g_rec[0], g_rec[2], (size_t)g_nrec[0])));
}

// ---- s195b: the claiming hit claims AT the hit (es_note_hit, before ah_hit asks netreact), through the one predicate es_elect uses ----
static void test_hit_sync(void) {
    EsCtx c; fake_reset(&c);
    c.hit_on = true;
    add_enemy(0x5B1, 20, 0, 0);
    players2(&c, 40, 18);  // me 20 m, Bob 2 m: a puppet here
    uint64_t t = 0;
    for (; t <= 500; t += 50) recv1(&c, 2, sm(0x5B1, 20, 0), t), step(&c, t);
    EsEnt *e = es_find(&c, 0x5B1);
    CHECK(e->owner == 2 && e->brain == 0 && es_hit_claim_ok(&c, e, 510) == ES_HC_NO);  // (no hit yet)
    int nb = f_brain_calls, ns = f_nsens;
    // the hit: claimed right here (no step in between), brain told ON at once, the old owner's sensors forced off
    CHECK(es_note_hit(&c, 0x5B1, 510) && e->owner == 1 && e->claim_from == 2 && e->brain == 1 && f_brain_calls == nb + 1 && f_last_brain[nb] == 1 && f_brain_hid[nb] == 0x5B1);
    CHECK(f_nsens == ns + 1 && c.st.hit_claims == 1 && c.st.hit_sync == 1 && c.st.claims == 1 && strstr(g_log, "Bob -> Alpha (hit: claim)"));
    CHECK(es_hit_claim_ok(&c, e, 511) == ES_HC_NO);  // (ours now)
    // the next steps: no second claim, no second brain call, our samples go out with CLAIM (+ HELD + HOT), no puppet frame
    int sent = 0, ap = f_apply_n;
    uint16_t fl = 0;
    for (t = 520; t <= 900; t += 16) if (step(&c, t)) { if (!sent) fl = O.ent[0].flags; sent++; }
    CHECK(sent >= 6 && fl == (ES_FL_CLAIM | ES_FL_HELD | ES_FL_HOT) && c.st.claims == 1 && c.st.hit_claims == 1 && f_brain_calls == nb + 1 && f_apply_n == ap);
    // a second hit while it is ours: nothing (no claim, no brain call)
    CHECK(!es_note_hit(&c, 0x5B1, 910) && c.st.claims == 1 && f_brain_calls == nb + 1);
    // does NOT qualify -> false, nothing changes: owner HOT, eshit off, banned, not loaded, owner silent (the stale path's), not in the table
    fake_reset(&c);
    c.hit_on = true;
    add_enemy(0x5B2, 20, 0, 0);
    players2(&c, 40, 18);
    for (t = 0; t <= 500; t += 50) recvf(&c, 2, sm(0x5B2, 20, 0), ES_FL_HELD | ES_FL_HOT, t), step(&c, t);
    e = es_find(&c, 0x5B2);
    nb = f_brain_calls;
    CHECK(!es_note_hit(&c, 0x5B2, 510) && e->owner == 2 && e->brain == 0 && es_hit_claim_ok(&c, e, 510) == ES_HC_HOT && f_brain_calls == nb && c.st.claims == 0);
    c.hit_on = false;
    CHECK(es_hit_claim_ok(&c, e, 510) == ES_HC_NO);
    recvf(&c, 2, sm(0x5B2, 20, 0), 0, 520);  // (HOT gone, eshit off: still no)
    CHECK(!es_note_hit(&c, 0x5B2, 530) && e->owner == 2);
    c.hit_on = true;
    e->yield_ms = 500;
    CHECK(!es_note_hit(&c, 0x5B2, 540) && e->owner == 2 && es_hit_claim_ok(&c, e, 540) == ES_HC_NO);
    e->yield_ms = 0, e->local = false;
    CHECK(!es_note_hit(&c, 0x5B2, 550) && e->owner == 2);
    e->local = true;
    CHECK(!es_note_hit(&c, 0x5B2, 520 + ES_STALE_MS + 1) && e->owner == 2);  // (Bob silent > ES_STALE_MS)
    CHECK(!es_note_hit(&c, 0x5BF, 560) && c.st.claims == 0);
    CHECK(es_note_hit(&c, 0x5B2, 600) && e->owner == 1 && c.st.claims == 1);  // (control: all fine again -> claimed)

    // ONE predicate: for every combination es_elect claims exactly when es_hit_claim_ok says YES (the hit time set directly, so only
    // es_elect acts), and counts a blocked hit exactly when it says HOT. YES needs: on, loaded, not yielded / refused, not stale,
    // fresh, and not (HOT + alive owner) = 3 of the 4 (HOT, away) pairs; HOT = 1
    int bad = 0, yes = 0, hot = 0;
    for (int m = 0; m < 256; m++) {
        fake_reset(&c);
        c.hit_on = m & 1;
        add_enemy(0x5C0, 20, 0, 0);
        EsPlayer p[2] = {mk(1, "Alpha", 40), mk(2, "Bob", 18)};
        p[1].away = (m >> 5) & 1;
        es_set_players(&c, p, 2);
        recvf(&c, 2, sm(0x5C0, 20, 0), (m & 4) ? ES_FL_HOT | ES_FL_HELD : 0, 5000);
        step(&c, 5000);
        e = es_find(&c, 0x5C0);
        if (!(m & 2)) e->local = false;
        if (m & 8) e->yield_ms = 4900;
        uint64_t now = (m & 16) ? 5000 + ES_STALE_MS + 1 : 5010;  // (late enough that now - ES_HIT_FRESH_MS > 0)
        e->hit_ms = (m & 64) ? now - ES_HIT_FRESH_MS : now - 5;
        if (m & 128) e->claim_ban = 4990;
        int want = es_hit_claim_ok(&c, e, now);
        uint32_t c0 = c.st.hit_claims, b0 = c.st.hit_blocked;
        es_elect(&c, e, now);
        bool claimed = c.st.hit_claims != c0;
        bad += claimed != (want == ES_HC_YES) || (c.st.hit_blocked != b0) != (want == ES_HC_HOT);
        yes += want == ES_HC_YES, hot += want == ES_HC_HOT;
    }
    CHECK(bad == 0 && yes == 3 && hot == 1);
}

// ---- s195 esdead: a dead player is "away" (net_esync_tick, `set esdead 1`): never nearest / closer, never held ----
static void test_dead(void) {
    // the owner died next to it: I (10 m, alive) claim it as "closer" after ES_HANDOVER_MS (his dead feet count as INFINITY)
    EsCtx c; fake_reset(&c);
    add_enemy(0xDE1, 20, 0, 0);
    EsPlayer p[2] = {mk(1, "Alpha", 30), mk(2, "Bob", 21)};
    es_set_players(&c, p, 2);
    uint64_t t = 0;
    for (; t <= 500; t += 50) recv1(&c, 2, sm(0xDE1, 20, 0), t), step(&c, t);
    CHECK(own(&c, 0xDE1) == 2);
    p[1].away = 1, es_set_players(&c, p, 2);
    int at = -1;
    for (t = 550; t <= 3000; t += 50) {
        if (own(&c, 0xDE1) == 2) recv1(&c, 2, sm(0xDE1, 20, 0), t);  // (a dead owner keeps sending until he sees the claim)
        step(&c, t);
        if (at < 0 && own(&c, 0xDE1) == 1) at = (int)(t - 550);
    }
    CHECK(at == ES_HANDOVER_MS && strstr(g_log, "Bob -> Alpha (closer: claim)") && c.st.claim_refused == 0);
    // a hit claim against a dead owner: his last samples say HOT (he hit it, then died) - ignored, he is away
    fake_reset(&c);
    c.hit_on = true;
    add_enemy(0xDE2, 20, 0, 0);
    EsPlayer q[2] = {mk(1, "Alpha", 40), mk(2, "Bob", 21)};
    es_set_players(&c, q, 2);
    for (t = 0; t <= 500; t += 50) recvf(&c, 2, sm(0xDE2, 20, 0), ES_FL_HELD | ES_FL_HOT, t), step(&c, t);
    es_note_hit(&c, 0xDE2, 510), step(&c, 520);
    CHECK(own(&c, 0xDE2) == 2 && c.st.hit_blocked == 1);  // (alive + HOT: blocked)
    q[1].away = 1, es_set_players(&c, q, 2);
    recvf(&c, 2, sm(0xDE2, 20, 0), ES_FL_HELD | ES_FL_HOT, 540), step(&c, 550);
    CHECK(own(&c, 0xDE2) == 1 && c.st.hit_claims == 1 && strstr(g_log, "(hit: claim)"));
    // MY side, dead owner: I keep owning + sending (never on my own timer), no HELD / HOT while dead, until a claim sample arrives
    fake_reset(&c);
    c.hit_on = true;
    add_enemy(0xDE3, 20, 0, 0);
    EsPlayer r[2] = {mk(1, "Alpha", 21), mk(2, "Bob", 40)};
    es_set_players(&c, r, 2);
    step(&c, 0);
    es_note_hit(&c, 0xDE3, 10);
    CHECK(step(&c, 50) == 1 && own(&c, 0xDE3) == 1 && (O.ent[0].flags & ES_FL_HELD));
    r[0].away = 1, es_set_players(&c, r, 2);
    int sent = 0, bits = 0;
    for (t = 100; t <= 6000; t += 50) if (step(&c, t)) sent++, bits += (O.ent[0].flags & (ES_FL_HELD | ES_FL_HOT)) != 0;
    CHECK(own(&c, 0xDE3) == 1 && sent >= 115 && bits == 0 && c.st.handovers == 0);
    claim1(&c, 2, sm(0xDE3, 20, 0), 6000);
    CHECK(own(&c, 0xDE3) == 2 && strstr(g_log, "(claim: yield)"));
    // me dead: I never claim as "closer" (I lie 1 m from it, the owner is 20 m away), nor as a fresh election
    fake_reset(&c);
    add_enemy(0xDE4, 20, 0, 0);
    EsPlayer d[2] = {mk(1, "Alpha", 21), mk(2, "Bob", 40)};
    d[0].away = 1;
    es_set_players(&c, d, 2);
    for (t = 0; t <= 5000; t += 50) recv1(&c, 2, sm(0xDE4, 20, 0), t), step(&c, t);
    CHECK(own(&c, 0xDE4) == 2 && c.st.claims == 0);
    fake_reset(&c);
    add_enemy(0xDE5, 20, 0, 0);
    es_set_players(&c, d, 2);
    step(&c, 0);
    CHECK(own(&c, 0xDE5) == 2);  // (the first election skips me: Bob, 20 m)
    // the only player, dead: nobody elected, its brain runs, nothing sent, no crash; alive again -> mine
    fake_reset(&c);
    add_enemy(0xDE6, 1, 0, 0);
    EsPlayer solo[1] = {mk(1, "Alpha", 0)};
    solo[0].away = 1;
    es_set_players(&c, solo, 1);
    sent = 0;
    for (t = 0; t <= 3000; t += 50) sent += step(&c, t);
    CHECK(own(&c, 0xDE6) == -1 && sent == 0 && f_brain_calls == 1 && f_last_brain[0] == 1 && c.st.claims == 0);
    solo[0].away = 0, es_set_players(&c, solo, 1);
    CHECK(step(&c, 3050) == 1 && own(&c, 0xDE6) == 1);
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
    test_areas();
    test_areas_fixed();
    test_hit();
    test_hit_two_games();
    test_hit_wire();
    test_hit_sync();
    test_dead();
    printf("esync_test: %d/%d passed\n", runs - fails, runs);
    return fails ? 1 : 0;
}

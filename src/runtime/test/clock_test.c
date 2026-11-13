// host test for runtime/sora_clock.h (s191, Melty spec 4.4): the pure core, several copies against one fake story.
//   clang -O1 -g -Wall -Wextra -DSORA_CLOCK_TEST -Iruntime runtime/test/clock_test.c -o build_h/clock_test.exe
//   UBSan: add -fsanitize=undefined,bounds -fsanitize-trap=all (-o build_h/clock_test_ubsan.exe)
// The fake story: members in join order, the first one leads (as the room: the host, then the longest here); a change of the
// leader's goes to every OTHER member that has the story (s198: the kit's server.ts relays a SET to others() only, never
// back to its writer; s191 echoed it to the leader too, a GUESS that hid the stale-value case); a member that joins gets
// every key first (the snapshot) when st_deliver() runs, and only then reads story_ready 1. A member index >= NC is a stand-in
// that is not a copy (a leader we write for by hand).
#include <stdio.h>
#include <stdlib.h>
#ifndef SORA_CLOCK_TEST
#define SORA_CLOCK_TEST
#endif
#include "sora_clock.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)
#define NEAR(a, b, e) (fabs((double)(a) - (double)(b)) <= (e))
#define HNEAR(a, b, e) (clock_dist((a), (b)) <= (e))

#define NC 3          // copies
#define STANDIN 7     // a member that is not a copy
#define QMAX 2048
typedef struct { char key[40]; uint8_t v[40]; int len; bool del; } Chg;
static struct {
    int mem[8], nm;
    bool ready[8];
    Chg q[8][QMAX];
    int nq[8], rq[8];
    struct { char key[40]; uint8_t v[40]; int len; } kv[8];
    int nkv, clears, sets, tries;
    bool refuse;
    char clear_prefix[40];
} ST;

static double g_now = 1000000.0;  // the room clock, ms

typedef struct {
    int id;
    Clock c;
    float hour;
    bool held, world, running, hook_wx, hook_moon, play_ok, busy;
    int weather, forced, moons, plays, holds, releases, forces, open, climate, gate, gate_puts, busy_left, holds_busy;
    int inworld;  // (s198) 1 Link stands in the world, 0 the title screen / a load
    char log[16384];
} Game;
static Game G[NC];

static bool st_is_member(int id) { for (int i = 0; i < ST.nm; i++) if (ST.mem[i] == id) return true; return false; }
static int st_leader(void) {
    for (int i = 0; i < ST.nm; i++) if (ST.ready[ST.mem[i]]) return ST.mem[i];
    return -1;
}
static void st_push(int id, const char *key, const void *v, int len, bool del) {
    if (ST.nq[id] >= QMAX) { printf("(queue full)\n"); return; }
    Chg *c = &ST.q[id][ST.nq[id]++];
    snprintf(c->key, sizeof c->key, "%s", key);
    c->len = len, c->del = del;
    if (len > 0) memcpy(c->v, v, (size_t)len);
}
static void st_put(const char *key, const void *v, int len, int by) {  // the room's map + every other ready member's queue
    int k = -1;
    for (int i = 0; i < ST.nkv; i++) if (!strcmp(ST.kv[i].key, key)) k = i;
    if (k < 0) k = ST.nkv++, snprintf(ST.kv[k].key, sizeof ST.kv[k].key, "%s", key);
    memcpy(ST.kv[k].v, v, (size_t)len), ST.kv[k].len = len;
    for (int i = 0; i < ST.nm; i++) if (ST.ready[ST.mem[i]] && ST.mem[i] != by) st_push(ST.mem[i], key, v, len, false);
}
static void st_join(int id) { ST.mem[ST.nm++] = id, ST.ready[id] = false, ST.nq[id] = ST.rq[id] = 0; }
static void st_leave(int id) {
    int j = 0;
    for (int i = 0; i < ST.nm; i++) if (ST.mem[i] != id) ST.mem[j++] = ST.mem[i];
    ST.nm = j, ST.ready[id] = false, ST.nq[id] = ST.rq[id] = 0;
}
static void st_deliver(void) {  // the snapshot to each member that has none yet
    for (int i = 0; i < ST.nm; i++) {
        int id = ST.mem[i];
        if (ST.ready[id]) continue;
        for (int k = 0; k < ST.nkv; k++) st_push(id, ST.kv[k].key, ST.kv[k].v, ST.kv[k].len, false);
        ST.ready[id] = true;
    }
}
static void st_reset(void) { memset(&ST, 0, sizeof ST); }
// the stand-in leader writes a value by hand
static int g_raw_cl = -1;  // (the climate byte st_raw writes: a 27-byte value carries it)
static void st_raw(double at, double hour, double rate, int wx, int moons, int len) {
    ClockVal v = {at, hour, rate, wx, moons, g_raw_cl};
    uint8_t b[CLOCK_WIRE_LEN];
    clock_encode(&v, b);
    st_put(CLOCK_KEY, b, len, STANDIN);
}

// ---- the copies' hooks (ctx = Game *)
static double io_now(void *x) { (void)x; return g_now; }
static int io_ready(void *x) { Game *g = x; return st_is_member(g->id) && ST.ready[g->id]; }
static int io_leader(void *x) { Game *g = x; return st_leader() == g->id; }
static int io_set(void *x, const char *k, const void *v, int n) {
    if (!io_leader(x)) return 0;
    ST.tries++;
    if (ST.refuse) return 0;
    ST.sets++;
    st_put(k, v, n, ((Game *)x)->id);
    return 1;
}
static int io_get(void *x, const char *k, void *b, int cap) {
    Game *g = x;
    if (!ST.ready[g->id]) return -1;
    for (int i = 0; i < ST.nkv; i++)
        if (!strcmp(ST.kv[i].key, k)) { int n = ST.kv[i].len < cap ? ST.kv[i].len : cap; memcpy(b, ST.kv[i].v, (size_t)n); return n; }
    return -1;
}
static int io_clear(void *x, const char *p) {
    if (!io_leader(x)) return 0;
    ST.clears++, snprintf(ST.clear_prefix, sizeof ST.clear_prefix, "%s", p);
    return 1;
}
static float io_hour(void *x, bool *held) { Game *g = x; *held = g->held; return g->world ? g->hour : -1; }
static void io_hold(void *x, float h) { Game *g = x; g->held = true, g->hour = h, g->holds++, g->gate = 1, g->holds_busy += g->busy; }  // (write_time sets the gate)
static void io_release(void *x) { Game *g = x; g->held = false, g->releases++, g->gate = 1; }
static int io_wx(void *x) { Game *g = x; return g->weather; }
static bool io_wforce(void *x, int t) { Game *g = x; g->forced = t, g->forces++; return true; }
static int io_moons(void *x) { Game *g = x; return g->moons; }
static bool io_play(void *x) { Game *g = x; g->plays++; if (g->play_ok) g->busy = true, g->busy_left = 12; return g->play_ok; }
static int io_open(void *x) { Game *g = x; return g->open; }
static int io_climate(void *x) { Game *g = x; return g->climate; }
static bool io_busy(void *x) { Game *g = x; return g->busy; }
static int io_gate(void *x) { Game *g = x; return g->gate; }
static void io_gate_put(void *x, int v) { Game *g = x; g->gate = v, g->gate_puts++; }
#ifndef CLOCK_TEST_OLD
static int io_inworld(void *x) { Game *g = x; return g->inworld; }
#endif
static void io_log(void *x, const char *s) {
    Game *g = x;
    size_t n = strlen(g->log);
    if (n + strlen(s) + 2 >= sizeof g->log) g->log[0] = 0, n = 0;
    snprintf(g->log + n, sizeof g->log - n, "%s\n", s);
}
static int count_str(const char *hay, const char *needle) {
    int c = 0;
    for (const char *p = strstr(hay, needle); p; p = strstr(p + 1, needle)) c++;
    return c;
}

static void game_new(int i, float hour, bool wx, bool moon) {
    Game *g = &G[i];
    memset(g, 0, sizeof *g);
    g->id = i, g->hour = hour, g->world = g->running = true, g->hook_wx = wx, g->hook_moon = moon, g->play_ok = true, g->forced = -1;
    g->open = 1, g->climate = 3, g->gate = 1, g->inworld = 1;
    ClockIO io = {g, io_now, io_ready, io_leader, io_set, io_get, io_clear, io_hour, NULL, io_hold, io_release,
                  wx ? io_wx : NULL, wx ? io_wforce : NULL, moon ? io_moons : NULL, moon ? io_play : NULL, io_log,
                  io_open, io_climate, moon ? io_busy : NULL, io_gate, io_gate_put,
#ifndef CLOCK_TEST_OLD  // (s198: -DCLOCK_TEST_OLD builds the s198 rejoin cases against the s191 header, which has no in_world)
                  io_inworld
#endif
    };
    clock_init(&g->c, &io);
}
// one frame of copy i: its story changes in, then the clock (active = in the room, key on)
static void frame1(int i, bool active) {
    Game *g = &G[i];
    while (ST.rq[i] < ST.nq[i]) {
        Chg *c = &ST.q[i][ST.rq[i]++];
        if (!strcmp(c->key, CLOCK_KEY)) clock_on_value(&g->c, c->v, c->len, c->del);
    }
    clock_frame(&g->c, active, CLOCK_OFF);
}
// dt ms pass: every game's own clock runs (unless held / stopped / no world), then copies [0, n) each run one frame
static void run(double dt, int n, bool active) {
    g_now += dt;
    for (int i = 0; i < NC; i++) {
        if (G[i].world && G[i].running && !G[i].held) G[i].hour = (float)clock_wrap24(G[i].hour + dt * CLOCK_RATE_NORMAL);
        if (G[i].busy && --G[i].busy_left <= 0) G[i].busy = false, G[i].hour = 0;  // (played: the game's own midnight; the exe's held value is stale)
    }
    for (int i = 0; i < n; i++) frame1(i, active);
}
static void run_for(double ms, int n, bool active) { for (double t = 0; t < ms; t += 16) run(16, n, active); }
static ClockVal last_room(void) {  // the room's current value
    ClockVal v = {0, -1, 0, -1, -1, -1};
    for (int i = 0; i < ST.nkv; i++) if (!strcmp(ST.kv[i].key, CLOCK_KEY)) clock_decode(ST.kv[i].v, ST.kv[i].len, &v);
    return v;
}

static void test_wire(void) {
    ClockVal v = {123456.5, 21.25, CLOCK_RATE_NORMAL, 3, 250, 17}, o;
    uint8_t b[CLOCK_WIRE_LEN + 8];
    CHECK(clock_encode(&v, b) == 27);
    CHECK(clock_decode(b, 27, &o) && o.at == v.at && o.hour == v.hour && o.rate == v.rate && o.weather == 3 && o.moons == 250 && o.climate == 17);
    CHECK(clock_decode(b, 26, &o) && o.moons == 250 && o.climate == -1);  // (26 = no climate byte)
    CHECK(clock_decode(b, 34, &o) && o.moons == 250 && o.climate == 17);  // (longer: the rest is ignored)
    v.climate = -1, clock_encode(&v, b);
    CHECK(b[26] == 0xff && clock_decode(b, 27, &o) && o.climate == -1);
    v.climate = 300, clock_encode(&v, b);
    CHECK(b[26] == 0xff);
    v.climate = 17;
    // Melty's stand-in writes 24 bytes: weather 0xff, count not known
    CHECK(clock_decode(b, 24, &o) && o.weather == -1 && o.moons == -1 && o.hour == 21.25);
    CHECK(clock_decode(b, 25, &o) && o.weather == 3 && o.moons == -1);
    CHECK(!clock_decode(b, 23, &o) && !clock_decode(NULL, 26, &o));
    // weather / count bytes: -1 and 255 = 0xff; the count wraps; a negative count goes as 0
    v.weather = -1, v.moons = 256 + 4, clock_encode(&v, b);
    CHECK(b[24] == 0xff && b[25] == 4 && clock_decode(b, 26, &o) && o.weather == -1 && o.moons == 4);
    v.weather = 255, v.moons = -1, clock_encode(&v, b);
    CHECK(b[24] == 0xff && b[25] == 0);
    // bad values
    double bad[][3] = {{NAN, 1, 0}, {1, NAN, 0}, {1, 1, NAN}, {INFINITY, 1, 0}, {1, INFINITY, 0}, {1, 1, -INFINITY},
                       {1, 24.5, 0}, {1, -0.01, 0}, {1, 5, 2.0 / 1000}, {1, 5, -2.0 / 1000}};
    int nbad = 0;
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        memcpy(b, &bad[i][0], 8), memcpy(b + 8, &bad[i][1], 8), memcpy(b + 16, &bad[i][2], 8);
        nbad += !clock_decode(b, 26, &o);
    }
    CHECK(nbad == 10);
    // edges that are fine: hour 24 (= 0), hour 0, a rate of exactly an hour a second, a rate back
    double ok[][3] = {{1, 24, 0}, {1, 0, 0}, {-5, 3, 1.0 / 1000}, {1, 3, -CLOCK_RATE_NORMAL}};
    int nok = 0;
    for (size_t i = 0; i < 4; i++) {
        memcpy(b, &ok[i][0], 8), memcpy(b + 8, &ok[i][1], 8), memcpy(b + 16, &ok[i][2], 8);
        nok += clock_decode(b, 26, &o) && o.hour >= 0 && o.hour < 24;
    }
    CHECK(nok == 4);
    // a bad value in the core: logged once, the good one kept
    st_reset(), game_new(0, 10, false, false);
    v = (ClockVal){g_now, 8, 0, -1, -1, -1}, clock_encode(&v, b);
    clock_on_value(&G[0].c, b, 26, false);
    double nan = NAN;
    memcpy(b + 8, &nan, 8);
    clock_on_value(&G[0].c, b, 26, false), clock_on_value(&G[0].c, b, 26, false), clock_on_value(&G[0].c, b, 12, false);
    CHECK(G[0].c.have && G[0].c.val.hour == 8 && G[0].c.st.bad == 3 && count_str(G[0].log, "bad botw.clock:world") == 1);
    // hours on the circle
    CHECK(NEAR(clock_wrap24(25.5), 1.5, 1e-9) && NEAR(clock_wrap24(-0.5), 23.5, 1e-9) && clock_wrap24(24) == 0);
    CHECK(NEAR(clock_delta(23.5, 0.5), 1, 1e-9) && NEAR(clock_delta(0.5, 23.5), -1, 1e-9) && NEAR(clock_dist(23.9, 0.1), 0.2, 1e-9));
}

static void test_leader(void) {
    st_reset(), game_new(0, 9.5f, true, true);
    G[0].weather = 0, G[0].moons = 3;
    st_join(0), st_deliver();
    run(16, 1, true);
    CHECK(G[0].c.role == CLOCK_LEADER && ST.sets == 0);  // (the rate is measured for a second first)
    run_for(1100, 1, true);
    CHECK(ST.sets == 1);
    ClockVal r = last_room();
    CHECK(HNEAR(clock_at(&r, g_now), G[0].hour, 0.001) && r.rate == CLOCK_RATE_NORMAL && r.weather == 0 && r.moons == 0 && NEAR(r.at, g_now, 200));
    // every 5 s, the 5 s writes logged once a minute
    int s0 = ST.sets;
    run_for(60000, 1, true);
    CHECK(ST.sets - s0 >= 11 && ST.sets - s0 <= 12);
    CHECK(count_str(G[0].log, "(every 5 s") == 1);
    CHECK(G[0].holds == 0 && !G[0].held);  // (the leader never holds its own clock)
    // a jump (a sleep at a campfire): at once
    run_for(300, 1, true);
    s0 = ST.sets;
    G[0].hour = (float)clock_wrap24(G[0].hour + 2);
    run(16, 1, true);
    CHECK(ST.sets == s0 + 1 && HNEAR(last_room().hour, G[0].hour, 0.001) && strstr(G[0].log, "(hour jump)"));
    // a small drift (< 0.05 h) waits for the 5 s write
    s0 = ST.sets;
    G[0].hour = (float)clock_wrap24(G[0].hour + 0.04);
    run_for(2000, 1, true);
    CHECK(ST.sets == s0);
    // the clock stops (a menu): rate 0 within about 2 s; runs again: the normal rate
    run_for(4000, 1, true);
    s0 = ST.sets;
    G[0].running = false;
    run_for(2100, 1, true);
    CHECK(ST.sets >= s0 + 1 && last_room().rate == 0 && strstr(G[0].log, "(our clock stopped)"));
    s0 = ST.sets;
    run_for(10000, 1, true);
    CHECK(ST.sets == s0 + 2 && last_room().rate == 0);  // (stopped: still the 5 s writes)
    G[0].running = true;
    run_for(2100, 1, true);
    CHECK(last_room().rate == CLOCK_RATE_NORMAL && strstr(G[0].log, "(our clock runs again)"));
    // weather
    run_for(5100, 1, true);
    s0 = ST.sets;
    G[0].weather = 2;
    run(16, 1, true);
    CHECK(ST.sets == s0 + 1 && last_room().weather == 2 && strstr(G[0].log, "(weather)"));
    G[0].weather = -1;  // (not known any more: 0xff)
    run_for(300, 1, true);
    CHECK(last_room().weather == -1);
    // a Blood Moon in our game: count + 1 in the next write, botw.dead: cleared
    run_for(300, 1, true);
    s0 = ST.sets;
    G[0].moons = 4;
    run(16, 1, true);
    CHECK(ST.sets == s0 + 1 && last_room().moons == 1 && ST.clears == 1 && !strcmp(ST.clear_prefix, "botw.dead:"));
    // another save loaded (the count jumps): no Blood Moon
    run_for(300, 1, true);
    G[0].moons = 9;
    run_for(300, 1, true);
    G[0].moons = 2;
    run_for(300, 1, true);
    CHECK(last_room().moons == 1 && ST.clears == 1);
    // never more than 4 writes a second: the weather flips every frame for 2 s
    run_for(300, 1, true);
    s0 = ST.sets;
    for (int f = 0; f < 125; f++) G[0].weather = f & 1, run(16, 1, true);
    CHECK(ST.sets - s0 >= 6 && ST.sets - s0 <= 9);
    // loading (no world): nothing is written, the rate is measured again after
    G[0].world = false;
    s0 = ST.sets;
    run_for(12000, 1, true);
    CHECK(ST.sets == s0);
    G[0].world = true;
    run_for(1200, 1, true);
    CHECK(ST.sets >= s0 + 1);
    // the room refuses a write (e.g. a moment between leaders): logged once, tried again 250 ms later, not every frame
    run_for(300, 1, true);
    ST.refuse = true, ST.tries = 0;
    G[0].hour = (float)clock_wrap24(G[0].hour + 3);  // (a jump: a write is due every frame)
    run_for(1000, 1, true);
    CHECK(ST.tries >= 3 && ST.tries <= 5 && count_str(G[0].log, "did not take the write") == 1 && G[0].c.role == CLOCK_LEADER);
    ST.refuse = false;
    s0 = ST.sets;
    run_for(300, 1, true);
    ClockVal rr = last_room();
    CHECK(ST.sets == s0 + 1 && HNEAR(clock_at(&rr, g_now), G[0].hour, 0.001));
}

// the stand-in leads; copy 0 is a guest
static void guest_setup(float hour, bool wx, bool moon) {
    st_reset(), game_new(0, hour, wx, moon);
    st_join(STANDIN), st_join(0), st_deliver();
}
static void test_guest(void) {
    // the target wraps at 24
    guest_setup(15, false, false);
    st_raw(g_now - 60000, 23.5, CLOCK_RATE_NORMAL, -1, -1, 26);
    run(16, 1, true);
    CHECK(G[0].c.role == CLOCK_GUEST && G[0].held && HNEAR(G[0].hour, 0.5, 0.01) && G[0].holds == 1);
    CHECK(strstr(G[0].log, "guest: hour 0.50 from the room (was 15.00)") != NULL);
    // rate 0: the hour stays
    st_raw(g_now - 1e6, 10, 0, -1, -1, 26);
    run(16, 1, true);
    CHECK(HNEAR(G[0].hour, 10, 1e-4) && strstr(G[0].log, "-> 10.00 (the room's clock jumped)"));
    int h0 = G[0].holds;
    run_for(20000, 1, true);
    CHECK(G[0].holds == h0 && HNEAR(G[0].hour, 10, 1e-4));
    // the normal rate: one hold a game minute (= once a real second), never off by more than a minute
    st_raw(g_now, 8, CLOCK_RATE_NORMAL, -1, -1, 26);
    run(16, 1, true);
    h0 = G[0].holds;
    double worst = 0;
    for (int f = 0; f < 625; f++) {  // 10 s
        run(16, 1, true);
        double d = clock_dist(G[0].hour, clock_at(&G[0].c.val, g_now));
        if (d > worst) worst = d;
    }
    CHECK(G[0].holds - h0 >= 9 && G[0].holds - h0 <= 11);
    CHECK(worst <= CLOCK_STEP_H + 1e-6);
    CHECK(HNEAR(G[0].hour, 8 + 10.0 / 60, 1.0 / 60));
    // a value whose target is within a minute of ours: no new hold; a far one: at once, logged
    h0 = G[0].holds;
    st_raw(g_now, G[0].hour + 0.005, CLOCK_RATE_NORMAL, -1, -1, 26);
    run(16, 1, true);
    CHECK(G[0].holds == h0);
    st_raw(g_now, 20, CLOCK_RATE_NORMAL, -1, -1, 26);
    run(16, 1, true);
    CHECK(G[0].holds == h0 + 1 && HNEAR(G[0].hour, 20, 0.001));
    // someone let our hold go (the control `time` command): held again at once, and still ours
    G[0].held = false;
    run(16, 1, true);
    CHECK(G[0].held && G[0].holds == h0 + 2 && G[0].c.ours);
    // story not ready (a rejoin): off, our hold released; ready again: back
    ST.ready[0] = false;
    run(16, 1, true);
    CHECK(G[0].c.role == CLOCK_OFF && !G[0].held && G[0].releases == 1 && strstr(G[0].log, "released the hour (waiting for the room's story)"));
    st_deliver();
    run(16, 1, true);
    CHECK(G[0].c.role == CLOCK_GUEST && G[0].held && G[0].holds == h0 + 3);
    // the key turned off (set mclock 0) = not active: released
    run(16, 1, false);
    CHECK(G[0].c.role == CLOCK_OFF && !G[0].held && G[0].releases == 2);
    run(16, 1, true);
    // the key deleted in the room: released; a new value: held again
    for (int i = 0; i < ST.nm; i++) if (ST.ready[ST.mem[i]]) st_push(ST.mem[i], CLOCK_KEY, NULL, 0, true);
    run(16, 1, true);
    CHECK(!G[0].held && G[0].releases == 3 && !G[0].c.have);
    st_raw(g_now, 6, CLOCK_RATE_NORMAL, -1, -1, 26);
    run(16, 1, true);
    CHECK(G[0].held && HNEAR(G[0].hour, 6, 0.001));
}

// the "shown hour" hook: the game's own clock runs under our hold (a TimeMgr that came back after a load with its speed on)
static float g_shown = -1;
static float io_shown(void *x) { (void)x; return g_shown; }
static void test_shown(void) {
    guest_setup(12, false, false);
    G[0].c.io.hour_shown = io_shown;
    st_raw(g_now, 12, 0, -1, -1, 26);
    run(16, 1, true);
    int h0 = G[0].holds;
    g_shown = 12.01f;  // (within 2 minutes: fine)
    run_for(6000, 1, true);
    CHECK(G[0].holds == h0);
    g_shown = 12.2f;  // (off: held again, then not every frame)
    run_for(6000, 1, true);
    CHECK(G[0].holds == h0 + 2);  // (at once, and 5 s later: the shown hour never comes back here)
    g_shown = -1;
}

static void test_bloodmoon_weather(void) {
    guest_setup(12, true, true);
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, 3, 5, 26);
    run(16, 1, true);
    CHECK(G[0].plays == 0 && G[0].c.moons_known && G[0].c.moons == 5);  // (the first value after a join: adopted)
    CHECK(G[0].forced == 3 && G[0].forces == 1);
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, 3, 5, 26);
    run(16, 1, true);
    CHECK(G[0].plays == 0 && G[0].forces == 1);  // (same count, same weather: nothing)
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, 0xff, 6, 26);
    run(16, 1, true);
    CHECK(G[0].plays == 1 && G[0].forced == -1 && G[0].forces == 2);  // (one Blood Moon; weather 0xff: the force released)
    CHECK(strstr(G[0].log, "weather force released: the room's weather is not known (0xff)") != NULL);
    for (int f = 0; f < 50; f++) run(16, 1, true);
    CHECK(G[0].plays == 1 && G[0].holds_busy == 0);  // (one per value, not per frame; no hold while it played)
    CHECK(G[0].held && HNEAR(G[0].hour, 12 + 0.8 / 60, 1.0 / 60) && strstr(G[0].log, "Blood Moon done: hour back to the room's"));
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, -1, -1, 24);  // (Melty's 24-byte form: the count is not known: kept)
    run(16, 1, true);
    CHECK(G[0].plays == 1 && G[0].c.moons == 6);
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, 1, 7, 26);
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, 1, 8, 26);  // (two values in one frame: one play)
    run(16, 1, true);
    CHECK(G[0].plays == 2 && G[0].forced == 1);
    run_for(500, 1, true);
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, 1, 0, 26);  // (8 -> 0: a change)
    run(16, 1, true);
    CHECK(G[0].plays == 3);
    run_for(500, 1, true);
    // leaving: the weather force goes too
    run(16, 1, false);
    CHECK(G[0].forced == -1 && G[0].releases == 1 && strstr(G[0].log, "released the hour and the weather"));
    // a rejoin: the first count is adopted again (no play)
    run(16, 1, true);
    CHECK(G[0].plays == 3 && G[0].c.moons == 0);
    // no hooks: weather / count ignored, no crash
    guest_setup(12, false, false);
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, 3, 5, 26);
    run(16, 1, true);
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, 4, 6, 26);
    run(16, 1, true);
    CHECK(G[0].plays == 0 && G[0].forces == 0 && G[0].held);
    // a leader with no hooks: weather 0xff, the count goes on from the room's (no Blood Moon played by a hook-less guest)
    st_reset(), game_new(0, 10, false, false);
    st_join(STANDIN), st_join(0), st_deliver();
    st_raw(g_now, 10, CLOCK_RATE_NORMAL, -1, 42, 26);
    run(16, 1, true);
    st_leave(STANDIN);
    run_for(1200, 1, true);
    CHECK(G[0].c.role == CLOCK_LEADER && last_room().weather == -1 && last_room().moons == 42);
}

// (s191 task 2) the Blood Moon the game cannot take here (a shrine, an event): owed, tried every 5 s, ONE play
static void test_owed(void) {
    guest_setup(12, true, true);
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, -1, 5, 26);
    run(16, 1, true);
    G[0].play_ok = false;
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, -1, 6, 26);
    run(16, 1, true);
    CHECK(G[0].plays == 1 && G[0].c.owed && strstr(G[0].log, "Blood Moon owed (count 6)"));
    run_for(4900, 1, true);
    CHECK(G[0].plays == 1);  // (not every frame)
    run_for(200, 1, true);
    CHECK(G[0].plays == 2);  // (5 s later)
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, -1, 7, 26);  // (a second change while one is owed: still ONE)
    run_for(10100, 1, true);
    CHECK(G[0].plays == 4 && count_str(G[0].log, "Blood Moon owed") == 1);
    G[0].play_ok = true;
    run_for(5100, 1, true);
    CHECK(G[0].plays == 5 && !G[0].c.owed && strstr(G[0].log, "the owed one plays now") && G[0].c.st.plays == 1);
    run_for(20000, 1, true);
    CHECK(G[0].plays == 5);  // (the second change did not owe a second one)
    // owed and the guest leaves: dropped
    G[0].play_ok = false;
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, -1, 8, 26);
    run(16, 1, true);
    CHECK(G[0].c.owed);
    run(16, 1, false);
    CHECK(!G[0].c.owed && strstr(G[0].log, "the owed Blood Moon is dropped"));
}

// (s191 task 2) while our Blood Moon is on its way no hold lands (it would undo the 24:00 push); then one at once
static void test_busy(void) {
    guest_setup(12, false, true);
    st_raw(g_now, 15, CLOCK_RATE_NORMAL, -1, 1, 26);
    run(16, 1, true);
    int h0 = G[0].holds;
    G[0].busy = true, G[0].busy_left = 400;  // (a long one: 6.4 s, past several minute steps and a far value)
    st_raw(g_now, 18, CLOCK_RATE_NORMAL, -1, 1, 26);
    run_for(6000, 1, true);
    CHECK(G[0].holds == h0 && G[0].holds_busy == 0);
    run_for(500, 1, true);  // (done: the game says 0:00, test_warp still says the old hold)
    CHECK(G[0].holds == h0 + 1 && HNEAR(G[0].hour, 18 + 6.5 / 60, 1.0 / 60) && G[0].held);
}

// (s191 task 2) weather per region: the climate rule and set mclockwx 0 / 1 / 2
static void test_climate(void) {
    // mode 1 (default), the leader's climate 3, ours 3: forced
    guest_setup(12, true, false);
    g_raw_cl = 3;
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, 4, -1, 27);
    run(16, 1, true);
    CHECK(G[0].c.wxmode == 1 && G[0].forced == 4 && strstr(G[0].log, "weather 4 from the room (same climate)"));
    // we walk into another climate: released within a second, back when we return
    G[0].climate = 9;
    run(16, 1, true);
    CHECK(G[0].forced == 4);  // (decided once a second)
    run_for(1000, 1, true);
    CHECK(G[0].forced == -1 && strstr(G[0].log, "another climate here (9, the leader's 3)"));
    G[0].climate = 3;
    run_for(1000, 1, true);
    CHECK(G[0].forced == 4);
    // a shrine: released; out again: forced
    G[0].open = 0;
    run_for(1000, 1, true);
    CHECK(G[0].forced == -1 && strstr(G[0].log, "not in the open world here"));
    G[0].open = 1;
    run_for(1000, 1, true);
    CHECK(G[0].forced == 4);
    // our climate not known: not forced (mode 1 needs the same one)
    G[0].climate = -1;
    run_for(1000, 1, true);
    CHECK(G[0].forced == -1);
    // mode 2: whatever the climate, but still never outside the open world
    G[0].c.wxmode = 2, G[0].climate = 9;
    run_for(1000, 1, true);
    CHECK(G[0].forced == 4 && strstr(G[0].log, "(set mclockwx 2)"));
    G[0].open = 0;
    run_for(1000, 1, true);
    CHECK(G[0].forced == -1);
    G[0].open = 1;
    run_for(1000, 1, true);
    CHECK(G[0].forced == 4);
    // mode 0: never
    G[0].c.wxmode = 0;
    run_for(1000, 1, true);
    CHECK(G[0].forced == -1 && strstr(G[0].log, "set mclockwx 0"));
    G[0].c.wxmode = 1;
    // a 26-byte value (no climate): mode 1 = the open world only
    G[0].climate = 9;
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, 2, -1, 26);
    run(16, 1, true);
    CHECK(G[0].forced == 2 && strstr(G[0].log, "(no climate in the value)"));
    // a new value is decided at once (not on the 1 s rhythm)
    g_raw_cl = 9;
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, 6, -1, 27);
    run(16, 1, true);
    CHECK(G[0].forced == 6);
    // a 24-byte value: weather 0xff = released
    st_raw(g_now, 12, CLOCK_RATE_NORMAL, 6, -1, 24);
    run(16, 1, true);
    CHECK(G[0].forced == -1);
    g_raw_cl = -1;
    // the leader: weather + climate in the value, 0xff for both outside the open world (a shrine's "0 Bluesky")
    st_reset(), game_new(0, 10, true, false);
    G[0].weather = 2, G[0].climate = 5;
    st_join(0), st_deliver();
    run_for(1100, 1, true);
    ClockVal r = last_room();
    CHECK(ST.sets == 1 && r.weather == 2 && r.climate == 5 && ST.kv[0].len == 27);
    run_for(300, 1, true);
    int s0 = ST.sets;
    G[0].climate = 6;  // (it walks into another climate: written)
    run(16, 1, true);
    CHECK(ST.sets == s0 + 1 && last_room().climate == 6 && strstr(G[0].log, "(climate)"));
    run_for(300, 1, true);
    G[0].open = 0, G[0].weather = 0;
    run(16, 1, true);
    r = last_room();
    CHECK(r.weather == -1 && r.climate == -1);
}

// (s191 task 2) the paraglider clock gate (TimeMgr +0x14e): a 0 read before our first hold is put back after our release
static void test_gate(void) {
    guest_setup(8, false, false);
    G[0].gate = 0;  // (no paraglider yet: the game stops its clock at 11:00)
    st_raw(g_now, 20, CLOCK_RATE_NORMAL, -1, -1, 26);
    run_for(2000, 1, true);
    CHECK(G[0].gate == 1 && G[0].c.gate_old == 0);  // (each hold set it to 1)
    run(16, 1, false);
    CHECK(G[0].gate == 0 && G[0].gate_puts == 1 && strstr(G[0].log, "paraglider clock gate back to 0"));
    // it was 1 (the paraglider): nothing put back
    guest_setup(8, false, false);
    st_raw(g_now, 20, CLOCK_RATE_NORMAL, -1, -1, 26);
    run(16, 1, true);
    run(16, 1, false);
    CHECK(G[0].gate == 1 && G[0].gate_puts == 0);
    // a hold that was not ours: the gate is not ours either
    guest_setup(8, false, false);
    G[0].gate = 0, G[0].held = true;
    st_raw(g_now, 20, CLOCK_RATE_NORMAL, -1, -1, 26);
    run(16, 1, true);
    run(16, 1, false);
    CHECK(G[0].gate == 1 && G[0].gate_puts == 0 && G[0].held);
}

static void test_release_ours(void) {
    // a hold that was there before us (SAVAGEWIND_TEST_TIME): followed, but never released
    guest_setup(7, false, false);
    G[0].held = true;
    st_raw(g_now, 13, CLOCK_RATE_NORMAL, -1, -1, 26);
    run_for(3000, 1, true);
    CHECK(G[0].held && HNEAR(G[0].hour, 13 + 3.0 / 60, 1.0 / 60) && !G[0].c.ours);
    run(16, 1, false);
    CHECK(G[0].held && G[0].releases == 0 && strstr(G[0].log, "the hour stays held"));
    // ours: released
    guest_setup(7, false, false);
    st_raw(g_now, 13, CLOCK_RATE_NORMAL, -1, -1, 26);
    run(16, 1, true);
    run(16, 1, false);
    CHECK(!G[0].held && G[0].releases == 1);
    run_for(500, 1, false);
    CHECK(G[0].releases == 1);  // (once)
    // never held (no world the whole time): nothing to release
    guest_setup(7, false, false);
    G[0].world = false;
    st_raw(g_now, 13, CLOCK_RATE_NORMAL, -1, -1, 26);
    run_for(500, 1, true);
    run(16, 1, false);
    CHECK(G[0].holds == 0 && G[0].releases == 0);
}

static void test_no_world(void) {
    guest_setup(7, true, true);
    G[0].world = false;
    st_raw(g_now, 13, CLOCK_RATE_NORMAL, 2, 5, 26);
    run_for(2000, 1, true);
    CHECK(G[0].holds == 0 && G[0].forces == 0 && G[0].c.pending);  // (nothing while loading; the value waits)
    CHECK(strstr(G[0].log, "-> guest") != NULL);
    char t[24];
    clock_state_txt(&G[0].c, t, sizeof t);
    CHECK(!strcmp(t, "-"));
    G[0].world = true;
    run(16, 1, true);
    CHECK(G[0].holds == 1 && HNEAR(G[0].hour, 13 + 2.0 / 60, 0.01) && G[0].forced == 2 && !G[0].c.pending);
    clock_state_txt(&G[0].c, t, sizeof t);
    CHECK(!strcmp(t, "13.03Gh") || !strcmp(t, "13.04Gh"));
    if (strcmp(t, "13.03Gh") && strcmp(t, "13.04Gh")) printf("    state \"%s\"\n", t);
    // a load mid-game: no world again, the hold put back after it (the exe re-attaches its held hour; we re-hold when the
    // target moved a minute)
    G[0].world = false;
    run_for(3000, 1, true);
    int h0 = G[0].holds;
    G[0].world = true;
    run(16, 1, true);
    CHECK(G[0].holds == h0 + 1 && HNEAR(G[0].hour, 13 + 5.0 / 60, 0.01));
}

static void test_role_changes(void) {
    // guest -> leader: our hold goes, our clock runs on from the synced hour, the next write carries it
    guest_setup(3, false, false);
    st_raw(g_now, 18, CLOCK_RATE_NORMAL, -1, -1, 26);
    run_for(2000, 1, true);
    CHECK(G[0].held && HNEAR(G[0].hour, 18 + 2.0 / 60, 1.0 / 60));
    st_leave(STANDIN);
    run(16, 1, true);
    CHECK(G[0].c.role == CLOCK_LEADER && !G[0].held && G[0].releases == 1 && strstr(G[0].log, "guest -> leader"));
    int s0 = ST.sets, h0 = G[0].holds;
    run_for(1200, 1, true);
    CHECK(ST.sets == s0 + 1 && HNEAR(last_room().hour, 18 + 3.2 / 60, 1.5 / 60) && G[0].holds == h0);
    // leader -> guest (a rejoin: another copy leads now): our game obeys
    st_reset();
    st_join(STANDIN), st_join(0), st_deliver();  // (the stand-in is first: it leads)
    st_raw(g_now, 6, 0, -1, -1, 26);
    run(16, 1, true);
    CHECK(G[0].c.role == CLOCK_GUEST && G[0].held && HNEAR(G[0].hour, 6, 1e-4) && strstr(G[0].log, "leader -> guest"));
    // a value that came before we listened (it is in the room's map, not in our queue): read with story_get on the way in
    guest_setup(3, false, false);
    st_raw(g_now, 11, 0, -1, -1, 26);
    ST.rq[0] = ST.nq[0];  // (as if the glue drained it while the key was off)
    run(16, 1, true);
    CHECK(G[0].held && HNEAR(G[0].hour, 11, 1e-4));
}

// two copies against one fake story, a late third joiner, the leader leaves
static void test_three_copies(void) {
    st_reset();
    game_new(0, 9.0f, true, true), game_new(1, 21.0f, true, true), game_new(2, 2.0f, true, true);
    G[0].weather = 1, G[0].moons = 10, G[1].moons = 3, G[2].moons = 0;
    st_join(0), st_join(1), st_deliver();
    run_for(10000, 2, true);
    CHECK(G[0].c.role == CLOCK_LEADER && G[1].c.role == CLOCK_GUEST);
    CHECK(!G[0].held && G[1].held && HNEAR(G[1].hour, G[0].hour, CLOCK_STEP_H + 0.001) && G[1].forced == 1);
    // the leader sleeps 8 h at a campfire: the guest follows within a frame
    G[0].hour = (float)clock_wrap24(G[0].hour + 8);
    run(16, 2, true);
    run(16, 2, true);
    CHECK(HNEAR(G[1].hour, G[0].hour, CLOCK_STEP_H + 0.001) && strstr(G[1].log, "the room's clock jumped"));
    // a Blood Moon in the leader's game: the guest plays one
    G[0].moons = 11;
    run_for(300, 2, true);
    CHECK(G[1].plays == 1 && G[0].plays == 0 && ST.clears == 1);
    // a third copy joins late: nothing until it has the whole story, then it is on the room's hour at once, no Blood Moon
    run_for(10000, 2, true);
    st_join(2);
    run_for(500, 3, true);
    CHECK(G[2].c.role == CLOCK_OFF && G[2].holds == 0 && HNEAR(G[2].hour, 2.0 + 20.8 / 60, 0.05));
    st_deliver();
    run(16, 3, true);
    CHECK(G[2].c.role == CLOCK_GUEST && G[2].held && HNEAR(G[2].hour, G[0].hour, CLOCK_STEP_H + 0.001) && G[2].plays == 0 && G[2].c.moons == 1);
    // the leader leaves: the longest-here copy (1) leads, lets its hold go and writes; copy 2 follows it
    run_for(3000, 3, true);
    int h1 = G[1].holds;
    st_leave(0);
    run(16, 3, true);
    CHECK(G[1].c.role == CLOCK_LEADER && !G[1].held && G[1].releases == 1 && G[2].c.role == CLOCK_GUEST);
    int s0 = ST.sets;
    run_for(1200, 3, true);
    CHECK(ST.sets > s0);
    run_for(30000, 3, true);
    CHECK(HNEAR(G[2].hour, G[1].hour, CLOCK_STEP_H + 0.001) && G[2].held && !G[1].held && G[1].holds == h1);
    // the count goes on from the old leader's: a Blood Moon in the new leader's game is count 2, copy 2 plays one
    CHECK(last_room().moons == 1);
    G[1].moons = 4;
    run_for(300, 3, true);
    CHECK(last_room().moons == 2 && G[2].plays == 1 && ST.clears == 2);
    // the old leader (copy 0) never held, never released
    CHECK(G[0].holds == 0 && G[0].releases == 0);
}

// ---- (s198) a new leader takes the room's clock (recon/s198/S198_ROOM_TEST.md Bugs 1)
// a member that joins at the FRONT: Melty gives the room host's game the lead the moment it is back (kit story.ts chooseLeader,
// why BACK); the snapshot (st_deliver) carries the store and the lead together, as melty_net.h STORY_S_SNAPSHOT does
static void st_join_host(int id) {
    for (int i = ST.nm; i > 0; i--) ST.mem[i] = ST.mem[i - 1];
    ST.mem[0] = id, ST.nm++, ST.ready[id] = false, ST.nq[id] = ST.rq[id] = 0;
}
static double room_now(void) { ClockVal r = last_room(); return clock_at(&r, g_now); }
// the exact case from the log: the room at 14.89 (COPY2 = copy 1 leads after the host left), the host's game (copy 0)
// comes back with its own 5.25; the s191 code wrote 5.25 and COPY2 logged "hour 14.89 -> 5.25 (the room's clock jumped)"
static void rejoin_setup(void) {
    st_reset();
    game_new(0, 5.25f, true, true), game_new(1, 14.79f, true, true);
    G[0].weather = G[1].weather = 0, G[0].moons = G[1].moons = 0;
    st_join(1), st_deliver();
    run_for(6000, 2, true);  // (copy 1 leads alone; copy 0 is not in the room yet: off, its own clock)
    G[0].hour = 5.25f;
}
static void test_rejoin(void) {
    rejoin_setup();
    CHECK(G[1].c.role == CLOCK_LEADER && G[0].c.role == CLOCK_OFF && ST.sets >= 1 && HNEAR(room_now(), 14.89, 0.01));
    st_join_host(0), st_deliver();
    run_for(3000, 2, true);
    CHECK(G[0].c.role == CLOCK_LEADER && G[1].c.role == CLOCK_GUEST);
    double want = 14.89 + 3000 * CLOCK_RATE_NORMAL;
    CHECK(HNEAR(G[0].hour, want, 0.02) && HNEAR(room_now(), want, 0.02));  // (no backwards jump)
    CHECK(HNEAR(G[1].hour, want, CLOCK_STEP_H + 0.01) && G[1].held);
    CHECK(strstr(G[1].log, "the room's clock jumped") == NULL);
    CHECK(strstr(G[0].log, "took the room's clock 14.89 as the new leader (our game was at 5.25") != NULL);
    CHECK(count_str(G[0].log, "leader: hour 5.") == 0);
    if (!HNEAR(room_now(), want, 0.02)) printf("    rejoin: room %.2f (want %.2f), host %.2f, copy2 %.2f\n", room_now(), want, G[0].hour, G[1].hour);
    // our own clock runs on (not held: hold + release once), the 5 s writes follow it, the guest with them
    CHECK(!G[0].held && G[0].holds == 1 && G[0].releases == 1);
    run_for(20000, 2, true);
    CHECK(HNEAR(room_now(), want + 20000 * CLOCK_RATE_NORMAL, 0.02) && HNEAR(G[1].hour, G[0].hour, CLOCK_STEP_H + 0.01));
}
// the stored value is old: the room's last leader left 20 s before the next one came: run on by the room time
static void test_take_old(void) {
    st_reset(), game_new(0, 5.25f, true, true);
    st_join(STANDIN), st_deliver();
    st_raw(g_now, 14.89, CLOCK_RATE_NORMAL, 0, 3, 26);
    st_leave(STANDIN);
    g_now += 20000;
    st_join(0), st_deliver();
    run_for(1500, 1, true);
    double want = 14.89 + 21500 * CLOCK_RATE_NORMAL;
    CHECK(G[0].c.role == CLOCK_LEADER && HNEAR(G[0].hour, want, 0.02) && strstr(G[0].log, "the value is 20.0 s old, run on at the normal rate"));
    ClockVal r = last_room();
    CHECK(ST.sets == 1 && HNEAR(clock_at(&r, g_now), want, 0.02) && r.moons == 3);
    // a value whose clock was stopped (the old leader was in a menu): its hour, not run on
    st_reset(), game_new(0, 5.25f, true, true);
    st_join(STANDIN), st_deliver();
    st_raw(g_now, 14.89, 0, 0, 3, 26);
    st_leave(STANDIN);
    g_now += 20000;
    st_join(0), st_deliver();
    run(16, 1, true);
    CHECK(HNEAR(G[0].hour, 14.89, 0.001) && strstr(G[0].log, "the room's clock was stopped"));
#ifndef CLOCK_TEST_OLD
    // the game puts its save's hour back right after our take (GUESS that a load can): set again before the first write;
    // a game that keeps doing it gets CLOCK_TAKE_MAX tries, then the leader leads from its own hour
    st_reset(), game_new(0, 5.25f, true, true);
    st_join(STANDIN), st_deliver();
    st_raw(g_now, 14.89, CLOCK_RATE_NORMAL, 0, 3, 26);
    st_leave(STANDIN);
    st_join(0), st_deliver();
    run(16, 1, true);
    CHECK(G[0].c.take_n == 1 && HNEAR(G[0].hour, 14.89, 0.01));
    run_for(300, 1, true);
    G[0].hour = 4.42f;
    run_for(1500, 1, true);
    CHECK(G[0].c.take_n == 2 && strstr(G[0].log, "our game moved the hour to 4.43 after the take (the room's 14.91): taken again") &&
          HNEAR(room_now(), 14.89 + 1800 * CLOCK_RATE_NORMAL, 0.02) && HNEAR(G[0].hour, room_now(), 0.01));
    st_reset(), game_new(0, 5.25f, true, true);
    st_join(STANDIN), st_deliver();
    st_raw(g_now, 14.89, CLOCK_RATE_NORMAL, 0, 3, 26);
    st_leave(STANDIN);
    st_join(0), st_deliver();
    for (int f = 0, last = 0; f < 400; f++) {  // (each take is undone at once)
        run(16, 1, true);
        if (G[0].c.take_n != last) last = G[0].c.take_n, G[0].hour = 4.42f;
    }
    CHECK(G[0].c.take_n == CLOCK_TAKE_MAX && ST.sets >= 1 && count_str(G[0].log, "taken again") == CLOCK_TAKE_MAX - 1 && HNEAR(room_now(), G[0].hour, 0.01));
#endif
}
// the stored value comes a little after the lead (the first second in the world is measured before any write anyway)
static void test_take_late(void) {
    st_reset(), game_new(0, 5.25f, true, true);
    st_join(0), st_deliver();
    run_for(400, 1, true);
    CHECK(G[0].c.role == CLOCK_LEADER && ST.sets == 0);
    st_raw(g_now - 2000, 14.89, CLOCK_RATE_NORMAL, -1, 2, 26);  // (lands in copy 0's queue: drained next frame)
    run_for(2000, 1, true);
    double want = 14.89 + 4000 * CLOCK_RATE_NORMAL;
    CHECK(HNEAR(G[0].hour, want, 0.02) && HNEAR(room_now(), want, 0.02) && last_room().moons == 2);
    CHECK(count_str(G[0].log, "leader: hour 5.") == 0 && strstr(G[0].log, "took the room's clock 14.92 as the new leader (our game was at 5.26; the value is 2.0 s old"));
}
// a truly fresh room: the first leader's own hour, as ever
static void test_take_fresh(void) {
    st_reset(), game_new(0, 9.5f, true, true);
    st_join(0), st_deliver();
    run_for(1100, 1, true);
    CHECK(ST.sets == 1 && HNEAR(room_now(), 9.5 + 1100 * CLOCK_RATE_NORMAL, 0.002) && G[0].holds == 0);
    CHECK(strstr(G[0].log, "the room has no clock yet: our hour 9.5") != NULL && strstr(G[0].log, "took the room's") == NULL);
    // a guest that becomes the leader (the old one left) is on the room's hour already: nothing to set
    guest_setup(3, false, false);
    st_raw(g_now, 18, CLOCK_RATE_NORMAL, -1, -1, 26);
    run_for(2000, 1, true);
    int h0 = G[0].holds;
    st_leave(STANDIN);
    run_for(1200, 1, true);
    CHECK(G[0].c.role == CLOCK_LEADER && G[0].holds == h0 && strstr(G[0].log, "our game is there already"));
}
// the leader writes its own hour; another game takes the lead back (BACK) and has written nothing yet: our guest role
// starts from OUR last write (the room's value; the room does not echo it to us), not the older value we had as a guest
static void test_back_after_jump(void) {
    st_reset(), game_new(1, 8.0f, false, false);
    st_join(STANDIN), st_join(1), st_deliver();
    st_raw(g_now, 8, CLOCK_RATE_NORMAL, -1, -1, 26);
    run_for(1000, 2, true);
    st_leave(STANDIN);
    run_for(1500, 2, true);
    CHECK(G[1].c.role == CLOCK_LEADER);
    G[1].hour = (float)clock_wrap24(G[1].hour + 8);  // (a sleep at a campfire)
    run_for(300, 2, true);
    double after = room_now();
    CHECK(HNEAR(after, G[1].hour, 0.01));
    st_join_host(STANDIN), st_deliver();
    run_for(2000, 2, true);
    CHECK(G[1].c.role == CLOCK_GUEST && G[1].held && HNEAR(G[1].hour, after + 2000 * CLOCK_RATE_NORMAL, CLOCK_STEP_H + 0.01));
    CHECK(strstr(G[1].log, "the room's clock jumped") == NULL);
}
#ifndef CLOCK_TEST_OLD
// a leader on the title screen / loading its save: no writes, the guests run on; in the world: take, then lead
static void test_take_title(void) {
    rejoin_setup();
    G[0].inworld = 0;  // (the title screen shows 5.25)
    int s0 = ST.sets, t0 = ST.tries, h1 = G[1].holds;
    st_join_host(0), st_deliver();
    run_for(20000, 2, true);
    CHECK(G[0].c.role == CLOCK_LEADER && ST.sets == s0 && ST.tries == t0 && G[0].holds == 0);
    CHECK(count_str(G[0].log, "not leading yet: not in the world") == 1);
    double want = 14.89 + 20000 * CLOCK_RATE_NORMAL;
    CHECK(G[1].held && HNEAR(G[1].hour, want, CLOCK_STEP_H + 0.01) && G[1].holds - h1 >= 18);  // (the guest's own time runs on)
    G[0].hour = 4.42f, G[0].inworld = 1;  // (the save loaded: its own hour)
    run_for(1500, 2, true);
    want += 1500 * CLOCK_RATE_NORMAL;
    CHECK(HNEAR(G[0].hour, want, 0.02) && HNEAR(room_now(), want, 0.02) && ST.sets == s0 + 1);
    CHECK(strstr(G[1].log, "the room's clock jumped") == NULL && count_str(G[0].log, "leader: hour 4.") == 0);
    // a load later on (in_world 0 while leading): nothing written, nothing taken after it (we lead the room's clock)
    run_for(300, 2, true);
    s0 = ST.sets;
    G[0].inworld = 0;
    run_for(12000, 2, true);
    CHECK(ST.sets == s0);
    G[0].inworld = 1;
    run_for(1200, 2, true);
    CHECK(ST.sets >= s0 + 1 && G[0].holds == 1);
    // a fresh room from the title: no write until in the world, then the SAVE's hour starts it (not the title's 5.25)
    st_reset(), game_new(0, 5.25f, true, true);
    G[0].inworld = 0;
    st_join(0), st_deliver();
    run_for(10000, 1, true);
    CHECK(ST.sets == 0 && G[0].c.role == CLOCK_LEADER);
    G[0].hour = 4.42f, G[0].inworld = 1;
    run_for(1100, 1, true);
    CHECK(ST.sets == 1 && HNEAR(room_now(), 4.42 + 1100 * CLOCK_RATE_NORMAL, 0.002) && strstr(G[0].log, "no clock yet: our hour 4.44"));
}
// set mclocktake 0: the s191 behaviour (its own hour at once, also from the title screen)
static void test_take_key_off(void) {
    rejoin_setup();
    G[0].c.take_on = G[1].c.take_on = false;
    G[0].inworld = 0;
    st_join_host(0), st_deliver();
    run_for(1200, 2, true);
    CHECK(G[0].c.role == CLOCK_LEADER && HNEAR(room_now(), 5.25 + 1200 * CLOCK_RATE_NORMAL, 0.002) &&
          strstr(G[1].log, "guest: hour 14.89 -> 5.27 (the room's clock jumped)"));  // (the room log's line: 14.89 -> 5.25)
    CHECK(G[0].holds == 0 && strstr(G[0].log, "took the room's") == NULL && strstr(G[0].log, "not leading yet") == NULL);
}
#endif
// ---- (s198 b) a leader that loads a save (a death's Continue, the pause menu's Load) keeps the room's clock
#ifdef CLOCK_TEST_NO_RELOAD  // (built against 27e3bc7's header, which has no load event: the game just loads)
#define clock_on_load(c, slot) ((void)(c), (void)(slot))
#endif
// copy 0 leads a fresh room at 14.0, copy 1 is its guest
static void reload_setup(void) {
    st_reset();
    game_new(0, 14.0f, true, true), game_new(1, 9.0f, true, true);
    G[0].weather = G[1].weather = 0, G[0].moons = G[1].moons = 0;
    st_join(0), st_join(1), st_deliver();
    run_for(6000, 2, true);
}
// the load as BotW does it: the save is read (sora_levels.h's fs event) with Link still "in the world" (the Game Over screen)
// or already out of it (the pause menu's Load), `out_ms` out of the world, back with the save's hour
static void leader_load(bool read_in_world, double out_ms, float save_hour) {
    if (!read_in_world) G[0].inworld = 0;
    clock_on_load(&G[0].c, 3);
    if (read_in_world) run_for(500, 2, true);  // (Continue picked: the card comes next)
    G[0].inworld = 0;
    run_for(out_ms, 2, true);
    G[0].hour = save_hour, G[0].inworld = 1;
}
static void test_reload(void) {
    // a death: read on the Game Over screen, 3 s of loading, back with the save's 4.42
    reload_setup();
    CHECK(G[0].c.role == CLOCK_LEADER && G[1].c.role == CLOCK_GUEST && HNEAR(G[1].hour, 14.1, 0.02));
    double t0 = g_now;
    int s0 = ST.sets;
    leader_load(true, 3000, 4.42f);
    CHECK(ST.sets == s0);  // (nothing written from the read on)
    run_for(3000, 2, true);
    double want = 14.0 + (g_now - t0 + 6000) * CLOCK_RATE_NORMAL;
    CHECK(HNEAR(room_now(), want, 0.02) && HNEAR(G[0].hour, want, 0.02) && HNEAR(G[1].hour, want, CLOCK_STEP_H + 0.01));
    CHECK(strstr(G[1].log, "the room's clock jumped") == NULL && count_str(G[0].log, "leader: hour 4.") == 0);
    CHECK(strstr(G[0].log, "back in the world after a load: took the room's clock 14.") && strstr(G[0].log, "again (the save says 4.42"));
    if (!HNEAR(room_now(), want, 0.02)) printf("    reload: room %.2f (want %.2f), leader %.2f, guest %.2f\n", room_now(), want, G[0].hour, G[1].hour);
    // the pause menu's Load: read while already out of the world
    double t1 = g_now, h1 = room_now();
    leader_load(false, 2000, 6.0f);
    run_for(2000, 2, true);
    want = h1 + (g_now - t1) * CLOCK_RATE_NORMAL;
    CHECK(HNEAR(room_now(), want, 0.02) && HNEAR(G[1].hour, want, CLOCK_STEP_H + 0.01) && strstr(G[1].log, "the room's clock jumped") == NULL);
#ifndef CLOCK_TEST_NO_RELOAD
    CHECK(G[0].c.st.reloads == 2 && count_str(G[0].log, "our game reads a save (slot 3)") == 2);
    // the save's hour lands a moment after Link stands: the guard takes it again before the first write
    double t2 = g_now, h2 = room_now();
    leader_load(true, 2000, 4.42f);
    run_for(300, 2, true);
    G[0].hour = 4.42f;
    run_for(2000, 2, true);
    want = h2 + (g_now - t2) * CLOCK_RATE_NORMAL;
    CHECK(HNEAR(room_now(), want, 0.02) && strstr(G[0].log, "our game moved the hour to 4.4") && strstr(G[1].log, "the room's clock jumped") == NULL);
    // a save read with no trip out of the world (GUESS: never seen; a check read): 30 s without writes, then nothing taken
    s0 = ST.sets;
    int h0 = G[0].holds;
    clock_on_load(&G[0].c, 3);
    run_for(29000, 2, true);
    CHECK(ST.sets == s0);
    run_for(1500, 2, true);
    CHECK(ST.sets > s0 && G[0].holds == h0 && strstr(G[0].log, "never left the world in 30 s: nothing taken"));
#endif
}
// a REAL time skip by the leader (a sleep at a fire / bed / inn) reads no save: its hour jump goes out as ever, also when
// (GUESS: not seen) the sleep's fade had made the game look out of the world for a moment
static void test_reload_sleep(void) {
    reload_setup();
    G[0].hour = (float)clock_wrap24(G[0].hour + 8);
    run(16, 2, true), run(16, 2, true);
    CHECK(HNEAR(room_now(), G[0].hour, 0.01) && HNEAR(G[1].hour, G[0].hour, CLOCK_STEP_H + 0.01) && strstr(G[0].log, "(hour jump)"));
    G[0].inworld = 0;
    run_for(2000, 2, true);
    G[0].hour = (float)clock_wrap24(G[0].hour + 6), G[0].inworld = 1;
    run_for(1200, 2, true);
    CHECK(HNEAR(room_now(), G[0].hour, 0.01) && HNEAR(G[1].hour, G[0].hour, CLOCK_STEP_H + 0.01) && G[0].holds == 0 && strstr(G[0].log, "took the room's") == NULL);
}
// a Blood Moon the guest owes (it is in a shrine) while the leader loads: the room's count stays, the guest plays ONE
static void test_reload_owed(void) {
    reload_setup();
    G[1].play_ok = false;
    G[0].moons = 1;  // (a Blood Moon in the leader's game: room count 1, botw.dead: cleared)
    run_for(300, 2, true);
    CHECK(last_room().moons == 1 && ST.clears == 1 && G[1].c.owed);
    leader_load(true, 3000, 4.42f);
    run_for(3000, 2, true);
    CHECK(last_room().moons == 1 && ST.clears == 1 && G[1].c.owed && G[1].plays >= 1);
    G[1].play_ok = true;
    int p0 = G[1].plays;
    run_for(5200, 2, true);
    CHECK(!G[1].c.owed && G[1].plays == p0 + 1 && G[1].c.st.plays == 1);
    run_for(10000, 2, true);
    CHECK(G[1].c.st.plays == 1 && last_room().moons == 1);
}
#ifndef CLOCK_TEST_NO_RELOAD
// set mclocktake 0: the load's hour is written (the s191 / 27e3bc7 behaviour)
static void test_reload_key_off(void) {
    reload_setup();
    G[0].c.take_on = G[1].c.take_on = false;
    leader_load(true, 3000, 4.42f);
    run_for(1500, 2, true);
    CHECK(HNEAR(room_now(), 4.42 + 1500 * CLOCK_RATE_NORMAL, 0.03) && strstr(G[1].log, "-> 4.4") && strstr(G[1].log, "(the room's clock jumped)"));
    CHECK(strstr(G[0].log, "reads a save") == NULL && G[0].c.st.reloads == 0);
}
#endif

int main(void) {
    test_wire();
    test_leader();
    test_guest();
    test_shown();
    test_bloodmoon_weather();
    test_release_ours();
    test_owed();
    test_busy();
    test_climate();
    test_gate();
    test_no_world();
    test_role_changes();
    test_three_copies();
    test_rejoin();
    test_take_old();
    test_take_late();
    test_take_fresh();
    test_back_after_jump();
#ifndef CLOCK_TEST_OLD
    test_take_title();
    test_take_key_off();
    test_reload();
    test_reload_sleep();
    test_reload_owed();
#ifndef CLOCK_TEST_NO_RELOAD
    test_reload_key_off();
#endif
#endif
    printf("clock_test: %d/%d\n", runs - fails, runs);
    return fails != 0;
}

// host test for runtime/sora_net_noauto.h (s289 noauto, `set roomnoauto 1`: BotW's enemy auto-placement held off while a
// friend is connected). Part 1, the pure core: the write-or-not answer (key, net on, peers, mgr checks, driven), the value
// stored over BotW's byte, the pointer verdict, the logged start / stop with its reason, a model of BotW's hold byte
// countdown under our stores, and the count line's enemy filter. Part 2, the glue on stubs (no game, no socket): NET's peer
// slots, LOG, GetProcAddress and a fake sw_api over host buffers standing in for the guest's AutoPlacementMgr and actors.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/noauto_test.c -o build_h/noauto_test.exe
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sw_api.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)

// ---- stubs for the glue (sora_net.h's NET + PEER_LIVE, sora.c's LOG, windows.h's module lookups) ----
static char g_log[16384];
static int g_nlog;
static void test_log(const char *f, ...) {
    va_list a;
    va_start(a, f);
    size_t n = strlen(g_log);
    vsnprintf(g_log + n, sizeof g_log - n, f, a);
    va_end(a);
    g_nlog++;
}
#define LOG test_log
static void log_clear(void) { g_log[0] = 0, g_nlog = 0; }
#define LOGGED(s) (strstr(g_log, (s)) != NULL)

typedef enum { PEER_NONE, PEER_LIVE, PEER_FADING } PeerState;
#define NET_MAX_PEERS 3
static struct { bool on; struct { bool used; struct { PeerState st; } p; } slot[NET_MAX_PEERS]; } NET;

// the fake guest: host buffers registered as regions (readable; writable unless said)
typedef struct { uint64_t lo, hi; bool w; } FakeRegion;
static FakeRegion g_reg[16];
static int g_nreg, g_rd_calls, g_wr_calls, g_site_calls;
static void reg_add(const void *p, size_t n, bool w) {
    g_reg[g_nreg].lo = (uint64_t)(uintptr_t)p, g_reg[g_nreg].hi = g_reg[g_nreg].lo + n, g_reg[g_nreg].w = w;
    g_nreg++;
}
static int reg_of(uint64_t a, size_t n) {
    for (int i = 0; i < g_nreg; i++)
        if (a >= g_reg[i].lo && a + n <= g_reg[i].hi && a + n >= a) return i;
    return -1;
}
static bool f_in_guest(uint64_t a, size_t n) { return reg_of(a, n) >= 0; }
static size_t f_readable(uint64_t a, size_t n) { return g_rd_calls++, reg_of(a, n) >= 0 ? n : 0; }
static bool f_write(uint64_t a, const void *in, size_t n) {
    g_wr_calls++;
    int r = reg_of(a, n);
    if (r < 0 || !g_reg[r].w) return false;
    memcpy((void *)(uintptr_t)a, in, n);
    return true;
}
#define FAKE_VT 0x7001ca8ab0ull
static uint64_t g_fake_ptr;  // the singleton pointer (sites row autoplace_mgr_ptr = its address)
static bool g_rows = true;   // false = an exe without the s289 rows
static uint64_t f_site(const char *name) {
    g_site_calls++;
    if (!g_rows) return 0;
    if (!strcmp(name, "autoplace_mgr_ptr")) return (uint64_t)(uintptr_t)&g_fake_ptr;
    if (!strcmp(name, "autoplace_mgr_vt")) return FAKE_VT;
    return 0;
}
static struct { uint8_t m[0x800]; } g_act[4];
static struct { uint8_t m[0x60]; } g_mo[4];
static const char *g_act_name[4] = {"Enemy_Chuchu_Junior", "Enemy_Keese", "Enemy_Bokoblin_Junior", "Enemy_Dragon_Fire"};
static const float g_act_pos[4][3] = {{10, 0, 0}, {0, 0, 100}, {5, 0, 0}, {0, 40, 30}};
static const char *f_actor_name(uint64_t a) {
    for (int i = 0; i < 4; i++)
        if (a == (uint64_t)(uintptr_t)&g_act[i]) return g_act_name[i];
    return "";
}
static int f_enemies(uint64_t *actor, float (*pos)[3], int32_t *life, int max) {
    int n = 0;
    for (int i = 0; i < 4 && n < max; i++, n++) actor[n] = (uint64_t)(uintptr_t)&g_act[i], memcpy(pos[n], g_act_pos[i], 12), life[n] = 3;
    return n;
}
static SwApi g_fapi = {.version = 1, .size = sizeof(SwApi), .in_guest = f_in_guest, .readable = f_readable, .write = f_write,
                       .site = f_site, .actor_name = f_actor_name};
static const SwApi *f_get(uint32_t want) { return want > SW_API_VERSION ? NULL : &g_fapi; }
static bool g_has_api = true;
static void *fake_proc(void *m, const char *n) {
    (void)m;
    if (!strcmp(n, SW_API_EXPORT)) return g_has_api ? (void *)f_get : NULL;
    if (!strcmp(n, "linkpos_enemies")) return (void *)f_enemies;
    return NULL;
}
typedef void *HMODULE;
#define GetModuleHandleA(n) NULL
#define GetProcAddress(m, n) fake_proc((m), (n))

#define NET_NOAUTO_GLUE
#include "sora_net_noauto.h"

// ======== part 1: the pure core ========
static void block_tests(void) {
    // the check order: key, net, peers, mgr; every input on = hold
    CHECK(na_block(false, false, 0, false) == NA_KEY);
    CHECK(na_block(false, true, 3, true) == NA_KEY);  // (key 0 wins over everything: the old frames)
    CHECK(na_block(true, false, 3, true) == NA_NET);
    CHECK(na_block(true, false, 0, false) == NA_NET);
    CHECK(na_block(true, true, 0, true) == NA_PEER);
    CHECK(na_block(true, true, -1, true) == NA_PEER);
    CHECK(na_block(true, true, 0, false) == NA_PEER);  // (no friend: the mgr is not even asked)
    CHECK(na_block(true, true, 1, false) == NA_MGR);
    CHECK(na_block(true, true, 1, true) == NA_HOLD);
    CHECK(na_block(true, true, 3, true) == NA_HOLD);
    // a store only while holding AND driven
    for (int w = 0; w < NA_N; w++) {
        CHECK(!na_write_now((NaWhy)w, false));
        CHECK(na_write_now((NaWhy)w, true) == (w == NA_HOLD));
    }
    // key 0 / net off / no peer: never a store, whatever else is on (all 2 x 2 x 4 x 2 x 2 inputs)
    int stores = 0, want = 0;
    for (int key = 0; key < 2; key++)
        for (int on = 0; on < 2; on++)
            for (int peers = -1; peers < 3; peers++)
                for (int mok = 0; mok < 2; mok++)
                    for (int drv = 0; drv < 2; drv++) {
                        bool s = na_write_now(na_block(key, on, peers, mok), drv);
                        stores += s;
                        if (s) CHECK(key && on && peers > 0 && mok && drv);
                        want += key && on && peers > 0 && mok && drv;
                    }
    CHECK(stores == want && want == 2);  // (peers 1 and 2)
    for (int i = 0; i < NA_N; i++) {
        CHECK(NA_STATE_TXT[i] && NA_STATE_TXT[i][0]);
        CHECK(NA_START_TXT[i] && NA_START_TXT[i][0]);
        CHECK(NA_STOP_TXT[i] && NA_STOP_TXT[i][0]);
    }
}

static void store_tests(void) {
    // 5 over anything below 5 (BotW's countdown, 0, a negative s8), never over 5 or more (BotW's own setter stores 5)
    for (int cur = -128; cur < 5; cur++) CHECK(na_store_val(cur) == NA_HOLD_VAL);
    for (int cur = 5; cur < 128; cur++) CHECK(na_store_val(cur) == -1);
    CHECK(NA_HOLD_VAL == 5);
    // the layout: the counters (7 types) between the pause counter and the hold bytes, the span covers all 7 bytes
    CHECK(NA_CNT_OFF == NA_PAUSE_OFF + 4);
    CHECK(NA_HOLD_OFF == NA_CNT_OFF + 4 * 7);
    CHECK(NA_PAUSE_OFF + NA_SPAN >= NA_HOLD_OFF + 7);
    CHECK(NA_PAUSE_OFF + NA_SPAN <= NA_MGR_SIZE);
}

static void mgr_tests(void) {
    const uint64_t vt = 0x7001ca8ab0ull, m = 0x10a0000000ull;
    CHECK(na_mgr_ok(m, true, true, vt, vt));
    CHECK(!na_mgr_ok(0, true, true, vt, vt));           // (title before the singleton / after its disposer)
    CHECK(!na_mgr_ok(m, false, true, vt, vt));          // (outside guest memory)
    CHECK(!na_mgr_ok(m, true, false, vt, vt));          // (not readable)
    CHECK(!na_mgr_ok(m, true, true, vt + 8, vt));       // (another object there)
    CHECK(!na_mgr_ok(m, true, true, 0, vt));
    CHECK(!na_mgr_ok(m, true, true, 0, 0));             // (no vtable row: an older exe)
    CHECK(!na_mgr_ok(m, true, true, vt, 0));
}

static void step_tests(void) {
    NaState s;
    memset(&s, 0, sizeof s);  // (zero = key off: the compiled default)
    NaWhy why = NA_N;
    CHECK(s.last == NA_KEY);
    CHECK(na_step(&s, NA_KEY, &why) == 0);
    CHECK(na_step(&s, NA_PEER, &why) == 0);  // (key on, nobody there: no line)
    CHECK(na_step(&s, NA_HOLD, &why) == 1 && why == NA_PEER);  // "peer joined"
    CHECK(na_step(&s, NA_HOLD, &why) == 0);
    CHECK(na_step(&s, NA_PEER, &why) == -1 && why == NA_PEER);  // "peer left"
    // the join seen on a frame that is not driven: the mgr waits for the next driven frame, the start still says "peer joined"
    CHECK(na_step(&s, NA_MGR, &why) == 0);
    CHECK(na_step(&s, NA_MGR, &why) == 0);
    CHECK(na_step(&s, NA_HOLD, &why) == 1 && why == NA_PEER);
    // the pointer lost while holding (a reboot, another object): stop "pointer missing", back = "found again"
    CHECK(na_step(&s, NA_MGR, &why) == -1 && why == NA_MGR);
    CHECK(na_step(&s, NA_MGR, &why) == 0);
    CHECK(na_step(&s, NA_HOLD, &why) == 1 && why == NA_MGR);
    // key off / on while holding
    CHECK(na_step(&s, NA_KEY, &why) == -1 && why == NA_KEY);
    CHECK(na_step(&s, NA_HOLD, &why) == 1 && why == NA_KEY);
    // the session left (net off) and back, with a mgr wait in between
    CHECK(na_step(&s, NA_NET, &why) == -1 && why == NA_NET);
    CHECK(na_step(&s, NA_MGR, &why) == 0);
    CHECK(na_step(&s, NA_HOLD, &why) == 1 && why == NA_NET);
    // a wait for the mgr after another wait: the last one before the mgr wait is named
    CHECK(na_step(&s, NA_KEY, &why) == -1 && why == NA_KEY);
    CHECK(na_step(&s, NA_MGR, &why) == 0);   // (key on, mgr missing)
    CHECK(na_step(&s, NA_PEER, &why) == 0);  // (then the friend left)
    CHECK(na_step(&s, NA_MGR, &why) == 0);   // (back, mgr not checked yet)
    CHECK(na_step(&s, NA_HOLD, &why) == 1 && why == NA_PEER);
    // starts and stops alternate over any sequence
    memset(&s, 0, sizeof s);
    int on = 0, bad = 0;
    srand(289);
    for (int i = 0; i < 20000; i++) {
        int e = na_step(&s, (NaWhy)(rand() % NA_N), &why);
        if (e > 0) bad += on, on = 1, bad += why == NA_HOLD;
        if (e < 0) bad += !on, on = 0, bad += why == NA_HOLD;
        bad += on != (s.last == NA_HOLD);
    }
    CHECK(bad == 0);
}

// BotW's byte under our stores: per calc run "if (counter == 0 && byte > 0) byte--" (S289_HELPER_SPAWNED.md Q2), the spawn
// check = "forbidden while byte > 0". frames[i]: 1 = we store this frame (holding + driven), 0 = no store; calc runs per frame.
static int model(const char *frames, int calc_per_frame, int counter, int8_t *byte, int *open_frames) {
    int min = 127;
    *open_frames = 0;
    for (const char *f = frames; *f; f++) {
        if (*f == '1') {
            int v = na_store_val(*byte);
            if (v >= 0) *byte = (int8_t)v;
        }
        for (int k = 0; k < calc_per_frame; k++)
            if (counter == 0 && *byte > 0) (*byte)--;
        if (*byte < min) min = *byte;
        *open_frames += *byte <= 0;
    }
    return min;
}

static void countdown_tests(void) {
    int8_t b = 0;
    int open = 0;
    // held: our store each frame, one calc a frame: 4..5, never open
    CHECK(model("1111111111111111111111111111111111111111", 1, 0, &b, &open) == 4 && open == 0 && b == 4);
    // stopped: the game's own countdown opens it at the 4th frame after our last store (5 calc runs after that store)
    b = 0;
    int mn = model("1111111111" "0000000000", 1, 0, &b, &open);
    CHECK(mn == 0 && b == 0 && open == 7);  // (zeros: 3 2 1 0 0 0 0 0 0 0)
    b = 0;
    model("1" "0000", 1, 0, &b, &open);
    CHECK(b == 0 && open == 1);  // (5 stored, then 5 calc runs: 4 3 2 1 0)
    // two calc runs between our frames: 3..5, never open
    b = 0;
    CHECK(model("11111111111111111111", 2, 0, &b, &open) == 3 && open == 0);
    // GUESS gap in the header: frames that are not driven write nothing; up to 3 of them keep it closed, 4 open it
    b = 0;
    CHECK(model("111" "000" "111", 1, 0, &b, &open) == 1 && open == 0);
    b = 0;
    CHECK(model("111" "0000" "111", 1, 0, &b, &open) == 0 && open == 1);
    // BotW's own hold (its counter 1, byte 5 from its setter): no countdown, our store never changes it
    b = 5;
    CHECK(model("1111100000", 1, 1, &b, &open) == 5 && b == 5 && open == 0);
    // BotW's own hold ends while ours runs (counter back to 0): ours keeps it closed
    b = 5;
    CHECK(model("1111111111", 1, 0, &b, &open) == 4 && open == 0);
    // a value above 5 (never seen in BotW's code: GUESS) is left alone, counts down to 5, then ours holds it
    b = 9;
    CHECK(model("1111111111", 1, 0, &b, &open) == 4 && open == 0);
}

static void count_tests(void) {
    const float link[3] = {100, 50, -200};
    NaEnemy e[] = {
        {"Enemy_Chuchu_Junior", 0, {110, 50, -200}, 4},        // auto, near (10 m)
        {"Enemy_Keese", 0, {100, 300, -150}, 1},               // auto, near (50 m on the ground plane; 250 m up does not count)
        {"Enemy_Bokoblin_Bone_Junior", 0, {200, 50, -200}, 2}, // auto, 100 m: not near
        {"Enemy_Bokoblin_Junior", 0xA1B2C3D4u, {101, 50, -200}, 13},  // placed
        {"Enemy_Dragon_Fire", 0, {120, 90, -200}, 1},          // dragon: exempt, on its own
        {"Animal_Boar_A", 0, {100, 50, -200}, 8},              // the list's animals: not counted
        {"Enemy_Chuchu_Middle", 0, {100, 50, -200}, 0},        // dead: not counted
        {NULL, 0, {100, 50, -200}, 5},                         // unreadable name: not counted
        {"Enemy_Lizalfos_Junior", 0, {142, 50, -200}, 6},      // auto, 42 m: near
        {"Enemy_Octarock", 0, {100, 50, -260}, 3},             // auto, exactly 60 m: near (<=)
        {"Enemy_Octarock", 0, {100, 50, -260.5f}, 3},          // auto, 60.5 m: not near
    };
    int n = (int)(sizeof e / sizeof *e);
    NaCount c;
    na_count(e, n, link, NA_NEAR_M, &c);
    CHECK(c.near0 == 4);
    CHECK(c.all0 == 6);
    CHECK(c.placed == 1);
    CHECK(c.dragons == 1);
    na_count(e, n, NULL, NA_NEAR_M, &c);  // (no Link position yet)
    CHECK(c.near0 == 0 && c.all0 == 6 && c.placed == 1 && c.dragons == 1);
    na_count(NULL, 5, link, NA_NEAR_M, &c);
    CHECK(c.near0 == 0 && c.all0 == 0 && c.placed == 0 && c.dragons == 0);
    na_count(e, 0, link, NA_NEAR_M, &c);
    CHECK(c.all0 == 0);
    CHECK(na_is_dragon("Enemy_Dragon_Ice") && na_is_dragon("Enemy_Dragon_Electric") && !na_is_dragon("Enemy_Lizalfos_Dark"));
    CHECK(!na_is_dragon(NULL));
}

// ======== part 2: the glue on the stubs ========
static uint8_t *g_mgr, *g_mgr2;  // two fake AutoPlacementMgr objects (NA_MGR_SIZE each)
#define BYTE_AT(m) (*(volatile int8_t *)((m) + NA_HOLD_OFF))
#define CNT_AT(m) (*(volatile uint32_t *)((m) + NA_CNT_OFF))
static const float g_link[3] = {0, 0, 0};
static uint64_t g_now = 1000;
static void frame(bool driven) { net_noauto_frame(driven, driven ? g_link : NULL, g_now), g_now += 33; }
static void frames(bool driven, int n) { for (int i = 0; i < n; i++) frame(driven); }
static void fake_reset(void) {
    memset(&NA, 0, sizeof NA);
    g_net_noauto = g_net_noauto_log = false;
    memset(&NET, 0, sizeof NET);
    g_rows = true, g_has_api = true;
    g_nreg = 0;
    g_rd_calls = g_wr_calls = g_site_calls = 0;
    memset(g_mgr, 0, NA_MGR_SIZE), memset(g_mgr2, 0, NA_MGR_SIZE);
    *(uint64_t *)g_mgr = FAKE_VT, *(uint64_t *)g_mgr2 = FAKE_VT;
    g_fake_ptr = (uint64_t)(uintptr_t)g_mgr;
    reg_add(&g_fake_ptr, 8, true);
    reg_add(g_mgr, NA_MGR_SIZE, true);
    reg_add(g_mgr2, NA_MGR_SIZE, false);  // (the second one's pages are not writable: api->write refuses)
    for (int i = 0; i < 4; i++) reg_add(&g_act[i], sizeof g_act[i], false), reg_add(&g_mo[i], sizeof g_mo[i], false);
    // actors: Chuchu (auto, 10 m), Keese (auto, 100 m), Bokoblin (placed, hid 0x1234), dragon (auto, exempt)
    memset(g_act, 0, sizeof g_act), memset(g_mo, 0, sizeof g_mo);
    uint64_t mo = (uint64_t)(uintptr_t)&g_mo[2];
    memcpy(g_act[2].m + 0x7c8, &mo, 8);
    uint32_t hid = 0x1234;
    memcpy(g_mo[2].m + 0x58, &hid, 4);
    log_clear();
    g_now = 1000;
}
static void peer(int k, bool live) { NET.slot[k].used = live, NET.slot[k].p.st = live ? PEER_LIVE : PEER_NONE; }

// (s290 flip) the compiled default is ON (the user: "Flip the passes ON"): single player / a session with nobody in it = nothing
// read, written or bound, no line (as with key 0); a friend = the hold; "set roomnoauto 0" = one line, nothing written after it
static void glue_default_tests(void) {
    CHECK(g_net_noauto == true && g_net_noauto_log == false);
    bool was = g_net_noauto;
    fake_reset();
    g_net_noauto = was;  // (fake_reset zeroes the keys: the compiled value back)
    frames(true, 50);    // (net off: single player)
    CHECK(BYTE_AT(g_mgr) == 0 && g_rd_calls == 0 && g_wr_calls == 0 && g_site_calls == 0 && !NA.tried && g_nlog == 0);
    NET.on = true;  // (a session, nobody in it)
    frames(true, 50);
    CHECK(BYTE_AT(g_mgr) == 0 && g_rd_calls == 0 && g_wr_calls == 0 && g_site_calls == 0 && !NA.tried && g_nlog == 0);
    peer(0, true);  // (a friend: the hold, no "set" line needed)
    frame(true);
    CHECK(BYTE_AT(g_mgr) == 5 && g_wr_calls == 1 && NA.tried);
    log_clear();
    CHECK(net_noauto_live_set("roomnoauto", 0) && !g_net_noauto && g_nlog == 1 && LOGGED("roomnoauto 0"));
    int wr = g_wr_calls;
    BYTE_AT(g_mgr) = 4;  // (BotW's calc)
    frames(true, 20);
    CHECK(BYTE_AT(g_mgr) == 4 && g_wr_calls == wr);  // (off: nothing written)
}

static void glue_off_tests(void) {
    // key 0 (the default before s290): with a session and a friend, driven frames: nothing read, nothing written, no line
    fake_reset();
    NET.on = true, peer(0, true);
    frames(true, 200);
    CHECK(BYTE_AT(g_mgr) == 0 && g_rd_calls == 0 && g_wr_calls == 0 && g_site_calls == 0 && !NA.tried && g_nlog == 0);
    frame(false);  // (net_sora_off's call)
    CHECK(g_rd_calls == 0 && g_nlog == 0);
    // a live rewrite of "set roomnoauto 0" (already 0): no line
    CHECK(net_noauto_live_set("roomnoauto", 0) && g_nlog == 0);
    CHECK(!net_noauto_live_set("roomnoautox", 1) && !net_noauto_live_set("noauto", 1));  // (other keys: not ours)
}

static void glue_hold_tests(void) {
    fake_reset();
    CHECK(net_noauto_live_set("noautolog", 1) && LOGGED("[noauto] log on"));
    log_clear();
    CHECK(net_noauto_live_set("roomnoauto", 1) && g_nlog == 1 && LOGGED("roomnoauto 1") && LOGGED("sites ok"));
    log_clear();
    CHECK(net_noauto_live_set("roomnoauto", 1) && g_nlog == 0);  // (re-applied by a live rewrite: no line)
    // no session yet: nothing written
    frames(true, 10);
    CHECK(BYTE_AT(g_mgr) == 0 && g_wr_calls == 0);
    // net on, nobody there: still nothing
    NET.on = true;
    frames(true, 10);
    CHECK(BYTE_AT(g_mgr) == 0 && g_wr_calls == 0);
    // a friend joins: the hold starts at once (driven), the first store through api->write, then plain stores
    peer(1, true);
    log_clear();
    frame(true);
    CHECK(BYTE_AT(g_mgr) == 5 && g_wr_calls == 1);
    CHECK(LOGGED("hold started (peer joined)") && LOGGED("AutoPlacementMgr at"));
    for (int i = 0; i < 50; i++) {
        BYTE_AT(g_mgr) = BYTE_AT(g_mgr) - 1;  // (BotW's calc between our frames)
        frame(true);
        CHECK(BYTE_AT(g_mgr) == 5);
    }
    CHECK(g_wr_calls == 1 && CNT_AT(g_mgr) == 0);  // (no more VirtualQuery writes; BotW's counter never touched)
    // BotW's own hold (its setter: counter 1, byte 5) under ours: the counter stays its own, a byte above 5 is never lowered
    CNT_AT(g_mgr) = 1;
    BYTE_AT(g_mgr) = 7;
    frames(true, 5);
    CHECK(CNT_AT(g_mgr) == 1 && BYTE_AT(g_mgr) == 7);
    CNT_AT(g_mgr) = 0, BYTE_AT(g_mgr) = 0;
    // frames that are not driven (menu / loading): nothing written, the hold stays (no line), counted
    log_clear();
    uint32_t idle0 = NA.st.idle;
    frames(false, 20);
    CHECK(BYTE_AT(g_mgr) == 0 && g_nlog == 0 && NA.st.idle == idle0 + 20 && NA.s.last == NA_HOLD);
    frame(true);
    CHECK(BYTE_AT(g_mgr) == 5);
    // the friend leaves: stop "peer left", nothing written after (the byte runs out by BotW's own countdown)
    peer(1, false);
    log_clear();
    BYTE_AT(g_mgr) = 0;
    frame(true);
    CHECK(BYTE_AT(g_mgr) == 0 && LOGGED("hold stopped (peer left)"));
    frames(true, 10);
    CHECK(BYTE_AT(g_mgr) == 0);
    // a FADING peer (no SNAP for 2 s) is not live: no hold (party_mp_live's rule)
    NET.slot[0].used = true, NET.slot[0].p.st = PEER_FADING;
    frames(true, 3);
    CHECK(BYTE_AT(g_mgr) == 0);
    // back while not driven: no store, no start yet; the next driven frame checks the mgr, starts "peer joined", stores
    peer(0, true);
    log_clear();
    frames(false, 3);
    CHECK(BYTE_AT(g_mgr) == 0 && g_nlog == 0);
    frame(true);
    CHECK(BYTE_AT(g_mgr) == 5 && LOGGED("hold started (peer joined)"));
    // the singleton pointer goes 0 (its disposer, a reboot): stop "pointer missing", nothing written; back: "found again"
    g_fake_ptr = 0;
    BYTE_AT(g_mgr) = 0;
    log_clear();
    frames(true, 5);
    CHECK(BYTE_AT(g_mgr) == 0 && LOGGED("hold stopped (AutoPlacementMgr pointer missing)") && LOGGED("pointer missing (0 /"));
    CHECK(NA.st.miss >= 5);
    g_fake_ptr = (uint64_t)(uintptr_t)g_mgr;
    log_clear();
    frame(true);
    CHECK(BYTE_AT(g_mgr) == 5 && LOGGED("hold started (AutoPlacementMgr found again)"));
    // another object at the pointer (a wrong vtable): nothing written
    *(uint64_t *)g_mgr = FAKE_VT + 0x10;
    BYTE_AT(g_mgr) = 0;
    frames(true, 5);
    CHECK(BYTE_AT(g_mgr) == 0 && NA.s.last == NA_MGR);
    *(uint64_t *)g_mgr = FAKE_VT;
    frame(true);
    CHECK(BYTE_AT(g_mgr) == 5);
    // a pointer outside guest memory: nothing written (and nothing read through it)
    g_fake_ptr = 0x10;
    BYTE_AT(g_mgr) = 0;
    frames(true, 3);
    CHECK(BYTE_AT(g_mgr) == 0 && NA.s.last == NA_MGR);
    // a new mgr whose pages are not writable: api->write refuses, nothing stored, tried again only after 5 s
    g_fake_ptr = (uint64_t)(uintptr_t)g_mgr2;
    int wr0 = g_wr_calls;
    log_clear();
    frame(true);
    CHECK(BYTE_AT(g_mgr2) == 0 && g_wr_calls == wr0 + 1 && NA.st.refused == 1 && LOGGED("not writable"));
    frames(true, 100);  // (3.3 s)
    CHECK(BYTE_AT(g_mgr2) == 0 && g_wr_calls == wr0 + 1);
    frames(true, 60);  // (past 5 s)
    CHECK(BYTE_AT(g_mgr2) == 0 && g_wr_calls == wr0 + 2 && NA.st.refused == 2);
    g_fake_ptr = (uint64_t)(uintptr_t)g_mgr;
    frame(true);
    CHECK(BYTE_AT(g_mgr) == 5 && NA.s.last == NA_HOLD);
    // the session left (Sora mode off: net_sora_off's call, then NET.on false): stop "net off", nothing written
    BYTE_AT(g_mgr) = 0;
    NET.on = false;
    log_clear();
    frame(false);
    CHECK(BYTE_AT(g_mgr) == 0 && LOGGED("hold stopped (net off)"));
    NET.on = true;
    frame(true);
    CHECK(BYTE_AT(g_mgr) == 5 && LOGGED("hold started (net on)"));
    // key off: its one line; the next frame stops "key off"; nothing written after, nothing read
    log_clear();
    CHECK(net_noauto_live_set("roomnoauto", 0) && g_nlog == 1 && LOGGED("roomnoauto 0"));
    BYTE_AT(g_mgr) = 0;
    frame(true);
    CHECK(LOGGED("hold stopped (key off)"));
    int rd0 = g_rd_calls;
    net_noauto_live_set("noautolog", 0);
    frames(true, 50);
    CHECK(BYTE_AT(g_mgr) == 0 && g_rd_calls == rd0);
    CHECK(NA.st.starts == NA.st.stops);
}

static void glue_log_tests(void) {
    fake_reset();
    net_noauto_live_set("noautolog", 1);
    net_noauto_live_set("roomnoauto", 1);
    NET.on = true, peer(2, true);
    log_clear();
    frame(true);
    // the count line at the first driven frame, then every 10 s: Chuchu near, Keese far, Bokoblin placed, the dragon on its own
    CHECK(LOGGED("[noauto] count: 1 auto-placed (hid 0) within 60 m of Link, 2 in the whole list; placed 1, dragons 1"));
    CHECK(LOGGED("type-0 hold byte 5, BotW counter 0"));
    log_clear();
    frames(true, 300);  // (9.9 s)
    CHECK(!LOGGED("count:"));
    frames(true, 10);
    CHECK(LOGGED("count:"));
    // the count line also with roomnoauto 0 (the "before" numbers), read-only: no store
    net_noauto_live_set("roomnoauto", 0);
    frame(true);
    BYTE_AT(g_mgr) = 0;
    int wr0 = g_wr_calls;
    log_clear();
    frames(true, 310);
    CHECK(LOGGED("count: 1 auto-placed") && LOGGED("off (set roomnoauto 1)") && BYTE_AT(g_mgr) == 0 && g_wr_calls == wr0);
    // net diag: the state, the byte, BotW's counter, the count
    net_noauto_live_set("roomnoauto", 1);
    frame(true);
    log_clear();
    net_noauto_diag();
    CHECK(g_nlog == 1 && LOGGED("[noauto] roomnoauto 1, noautolog 1: HOLDING") && LOGGED("live peers 1"));
    CHECK(LOGGED("type-0 hold byte 5, BotW counter 0, pause counter 0") && LOGGED("enemies: 1 auto-placed"));
    // net diag with everything off: one line, read-only
    fake_reset();
    net_noauto_diag();
    CHECK(g_nlog == 1 && LOGGED("roomnoauto 0, noautolog 0: off") && g_wr_calls == 0 && BYTE_AT(g_mgr) == 0);
}

static void glue_oldexe_tests(void) {
    // an exe without the s289 rows: the key's line says so, nothing is written
    fake_reset();
    g_rows = false;
    net_noauto_live_set("roomnoauto", 1);
    CHECK(LOGGED("NEEDS EXE RELINK"));
    NET.on = true, peer(0, true);
    frames(true, 20);
    CHECK(BYTE_AT(g_mgr) == 0 && g_wr_calls == 0 && NA.s.last == NA_MGR);
    // an exe without sw_api (before s175)
    fake_reset();
    g_has_api = false;
    net_noauto_live_set("noautolog", 1);
    log_clear();
    net_noauto_live_set("roomnoauto", 1);
    CHECK(LOGGED("no sw_api"));
    NET.on = true, peer(0, true);
    frames(true, 20);
    CHECK(BYTE_AT(g_mgr) == 0 && g_wr_calls == 0 && NA.s.last == NA_MGR && LOGGED("pointer missing (this exe has no sw_api"));
    CHECK(LOGGED("count: no enemy list"));
}

int main(void) {
    block_tests();
    store_tests();
    mgr_tests();
    step_tests();
    countdown_tests();
    count_tests();
    g_mgr = calloc(1, NA_MGR_SIZE), g_mgr2 = calloc(1, NA_MGR_SIZE);
    if (!g_mgr || !g_mgr2) return printf("noauto_test: no memory\n"), 2;
    glue_default_tests();
    glue_off_tests();
    glue_hold_tests();
    glue_log_tests();
    glue_oldexe_tests();
    free(g_mgr), free(g_mgr2);
    printf("noauto_test: %d / %d checks pass%s\n", runs - fails, runs, fails ? " - FAIL" : "");
    return fails != 0;
}

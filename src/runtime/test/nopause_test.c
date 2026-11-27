// host test for runtime/sora_nopause.h (s289 nopause, `set netnopause 1`: the world runs behind menus while a friend is in the
// Melty room; after Savage Wind PR #294 by byluki, MIT). Part 1, the pure core: the run-or-not answer (key, exe, room, UI pause
// values, a load, Game Over, the pouch rule, a refused call), when the world is stopped again, the pouch's new A / X, the
// attack AS names, the reach / front test, the arrow's delay, the no-death floor. Part 2, the glue on stubs (no game, no
// socket): NET's peer slots, CAMP, g_held, LOG, GetProcAddress and a fake sw_api whose guest calls act on host buffers standing
// in for the BaseProcMgr, the physics system, the game scene, Link and the enemies: which calls go out with which arguments,
// none with the key 0, the world stopped again on every way out while a menu is up, the blows and their life writes.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/nopause_test.c -o build_h/nopause_test.exe
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sw_api.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)

// ---- stubs for the glue (sora_net.h's NET + PEER_LIVE, sora_camp.h's CAMP, sora.c's LOG / g_held / helpers) ----
static char g_log[32768];
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
static struct { bool on, melty, dead, downed; struct { bool used; struct { PeerState st; } p; } slot[NET_MAX_PEERS]; } NET;
static bool g_conn = true;
static bool sora_melty_connected(void) { return g_conn; }
enum { CP_CLOSED, CP_WAIT_PAUSE, CP_OPEN, CP_CLOSING, CP_PULSE, CP_HANDOVER, CP_SWITCH };  // (= sora_camp_page.h's)
static struct { struct { int state; } page; } CAMP;
static volatile uint64_t g_held;

// the fake guest: host buffers registered as readable regions
typedef struct { uint64_t lo, hi; } FakeRegion;
static FakeRegion g_reg[64];
static int g_nreg;
static void reg_add(const void *p, size_t n) { g_reg[g_nreg].lo = (uint64_t)(uintptr_t)p, g_reg[g_nreg].hi = g_reg[g_nreg].lo + n, g_nreg++; }
static int g_rd_calls;
static bool rd_ok(uint64_t a, size_t n) {
    g_rd_calls++;
    for (int i = 0; i < g_nreg; i++)
        if (a >= g_reg[i].lo && a + n <= g_reg[i].hi && a + n >= a) return true;
    return false;
}
typedef struct { uint64_t pause_state, aslist_none_got; } SoraSites;
static uint32_t g_ps;
static SoraSites g_ss;
static const SoraSites *sora_sites(void) { return &g_ss; }
static uint64_t g_present;
static uint64_t g_nm_lo, g_nm_hi;  // (s290: the readable range of the name test)
static bool np_ok_adapter_test(uint64_t a, size_t n) { return a >= g_nm_lo && a + n <= g_nm_hi; }
static uint64_t vk_frame_counter(void) { return g_present; }
static int g_drive_off;
static void drive_off(void) { g_drive_off++; }

static uint8_t g_bpm[0x200], g_phys[0x80], g_scene[0x900];
static uint64_t g_bpm_slot, g_phys_obj, g_phys_gotv, g_scene_obj, g_scene_gotv;
typedef struct { uint8_t m[0x900]; uint8_t asl[0xd0]; uint8_t slots[2 * 0x50]; uint8_t bank[2][0x98]; uint8_t sb[2][0x18]; char name[2][32]; uint8_t unit[0x50]; } FakeActor;
static FakeActor g_link, g_en[3];
static float g_link_pos[3] = {0, 10, 0};
static bool g_link_ok = true;
static int g_nen = 0;
static const char *g_en_name[3] = {"Enemy_Bokoblin_Junior", "Enemy_Moriblin_Junior", "Enemy_Bokoblin_Bow"};
static uint64_t A(const void *p) { return (uint64_t)(uintptr_t)p; }
static void as_set(FakeActor *a, int slot, const char *name) { snprintf(a->name[slot], sizeof a->name[slot], "%s", name); }
static void actor_init(FakeActor *a) {
    memset(a, 0, sizeof *a);
    *(uint64_t *)(a->m + 0x560) = A(a->asl);
    *(int32_t *)(a->asl + 0xb8) = 2;
    *(uint64_t *)(a->asl + 0xc0) = A(a->slots);
    for (int s = 0; s < 2; s++) {
        *(int32_t *)(a->slots + s * 0x50 + 0x20) = 1;
        *(uint64_t *)(a->slots + s * 0x50 + 0x28) = A(a->bank[s]);
        *(uint64_t *)(a->bank[s] + 0x18) = 1;  // (the AS object: one runs)
        *(uint64_t *)(a->bank[s]) = A(a->sb[s]);
        *(uint64_t *)(a->sb[s] + 0x10) = A(a->name[s]);
    }
    // (s290) its proc unit: actor +0x68 -> unit; unit +0x48 = the actor, unit +0x40 = its id (= actor +0x60); actor +0x64 = 1 (alive)
    static uint32_t next_id = 0x5000;
    uint32_t id = next_id++;
    *(uint64_t *)(a->m + 0x68) = A(a->unit), *(uint32_t *)(a->m + 0x60) = id, a->m[0x64] = 1;
    *(uint64_t *)(a->unit + 0x48) = A(a), *(uint32_t *)(a->unit + 0x40) = id;
    reg_add(a, sizeof *a);
}
// an enemy at (x, z) on Link's ground, facing (fx, fz)
static void en_place(int i, float x, float z, float fx, float fz) {
    float *m = (float *)(g_en[i].m + 0x398);
    memset(m, 0, 48);
    m[0] = 1, m[5] = 1, m[10] = fz, m[2] = fx, m[3] = x, m[7] = g_link_pos[1], m[11] = z;
}
static int linkpos_link_life(void) { return g_link_ok ? *(const int32_t *)(g_link.m + 0x848) : -1; }
static bool linkpos_link_pos(float p[3]) { if (!g_link_ok) return false; memcpy(p, g_link_pos, 12); return true; }
static uint64_t link_actor(void) { return g_link_ok ? A(&g_link) : 0; }
static int linkpos_enemies(uint64_t *actor, float (*pos)[3], int32_t *life, int max) {
    int n = 0;
    for (int i = 0; i < g_nen && n < max; i++, n++) {
        const float *m = (const float *)(g_en[i].m + 0x398);
        actor[n] = A(&g_en[i]), pos[n][0] = m[3], pos[n][1] = m[7], pos[n][2] = m[11], life[n] = 12;
    }
    return n;
}

// the fake exe: sites, guest calls (acting on the buffers), the nopause exports
#define F_SET 0x70015195d0ull
#define F_CLEAR 0x70015195e0ull
#define F_PROCS 0x7001519720ull
#define F_PHYS 0x700156f61cull
typedef struct { uint64_t fn, x0, x1, x2; } Call;
static Call g_calls[64];
static int g_ncall, g_site_calls;
static bool g_call_ok = true, g_in_frame = true, g_state_rows = true, g_rows = true;
static uint64_t f_site(const char *n) {
    g_site_calls++;
    if (!strcmp(n, "np_scene_state")) return g_state_rows ? 0x7001e44ef0ull : 0;  // (the exe's rows for the scene state write)
    if (!strcmp(n, "np_pause_mgr")) return g_state_rows ? 0x7001e16128ull : 0;
    if (!g_rows) return 0;
    if (!strcmp(n, "np_bpm_ptr")) return A(&g_bpm_slot);
    if (!strcmp(n, "np_bits_set")) return F_SET;
    if (!strcmp(n, "np_bits_clear")) return F_CLEAR;
    if (!strcmp(n, "np_procs")) return F_PROCS;
    if (!strcmp(n, "np_phys_pause")) return F_PHYS;
    if (!strcmp(n, "phys_system_got")) return A(&g_phys_gotv);
    if (!strcmp(n, "np_scene_got")) return A(&g_scene_gotv);
    return 0;
}
static bool f_call(Cpu *c, uint64_t fn, const SwArgs *a, SwRet *r) {
    (void)c, (void)r;
    if (g_ncall < 64) g_calls[g_ncall] = (Call){fn, a->x[0], a->nx > 1 ? a->x[1] : 0xdead, a->nx > 2 ? a->x[2] : 0xdead};
    g_ncall++;
    if (!g_call_ok) return false;
    uint16_t *bits = (uint16_t *)(g_bpm + 0x168);
    if (fn == F_SET && a->x[0] == A(g_bpm)) *bits |= (uint16_t)a->x[1];
    else if (fn == F_CLEAR && a->x[0] == A(g_bpm)) *bits &= (uint16_t)~a->x[1];
    else if (fn == F_PROCS && a->x[0] == A(g_bpm)) g_bpm[0x163] = 0;
    else if (fn == F_PHYS && a->x[0] == A(g_phys)) g_phys[0x60] = (uint8_t)a->x[1];
    else return false;
    return true;
}
static bool f_in_frame(void) { return g_in_frame; }
static const char *f_actor_name(uint64_t a) {
    for (int i = 0; i < 3; i++)
        if (a == A(&g_en[i])) return g_en_name[i];
    return "";
}
static SwApi g_fapi = {.version = 1, .size = sizeof(SwApi), .site = f_site, .call = f_call, .in_frame = f_in_frame, .actor_name = f_actor_name};
static const SwApi *f_get(uint32_t want) { return want > SW_API_VERSION ? NULL : &g_fapi; }
static int g_set_world = -1, g_set_live = -1, g_nset, g_step = -1;
static void f_np_set(int world, int live) { g_set_world = world, g_set_live = live, g_nset++; }
static void f_np_step(int d) { g_step = d; }
static uint64_t g_cnt[3];  // (the exe's scene state writes: zeroed / held / back)
static void f_np_counts(uint64_t out[3]) { memcpy(out, g_cnt, sizeof g_cnt); }
static float g_hour = 12.0f;
static float f_hour(bool *held) { if (held) *held = false; return g_hour; }
static uint64_t g_vfr;
static uint64_t f_vfr(void) { return g_vfr; }
static bool g_has_exe = true, g_has_step = true, g_has_counts = true;
static bool g_bm_busy;  // (sora_world.h: a Blood Moon of the room's on its way here)
static bool sora_world_bloodmoon_busy(void) { return g_bm_busy; }
static void *fake_proc(void *m, const char *n) {
    (void)m;
    if (!strcmp(n, SW_API_EXPORT)) return (void *)f_get;
    if (!strcmp(n, "sw_nopause_set")) return g_has_exe ? (void *)f_np_set : NULL;
    if (!strcmp(n, "sw_nopause_step")) return g_has_exe && g_has_step ? (void *)f_np_step : NULL;
    if (!strcmp(n, "sw_nopause_counts")) return g_has_exe && g_has_counts ? (void *)f_np_counts : NULL;
    if (!strcmp(n, "test_warp_hour")) return (void *)f_hour;
    if (!strcmp(n, "framepace_game_frame")) return (void *)f_vfr;
    return NULL;
}
typedef void *HMODULE;
#define GetModuleHandleA(n) NULL
#define GetProcAddress(m, n) fake_proc((m), (n))

#include "sora_nopause.h"

// ======== part 1: the pure core ========
static void test_core(void) {
    NpIn in = {.key = true, .exe_ok = true, .room = true, .ps = NP_PS_MENU};
    CHECK(np_decide(&in) == NP_ON);
    in.ps = NP_PS_QUICK; CHECK(np_decide(&in) == NP_ON);
    in.ps = 0; CHECK(np_decide(&in) == NP_NOMENU);
    in.ps = NP_PS_LOAD | NP_PS_MENU; CHECK(np_decide(&in) == NP_NOMENU);  // (a load from the map: the world waits for it)
    in.ps = 0x10; CHECK(np_decide(&in) == NP_OTHERUI);
    in.ps = 6; CHECK(np_decide(&in) == NP_OTHERUI);  // (exact values only)
    in.ps = NP_PS_MENU;
    in.key = false; CHECK(np_decide(&in) == NP_KEY); in.key = true;
    in.exe_ok = false; CHECK(np_decide(&in) == NP_NOEXE); in.exe_ok = true;
    in.room = false; CHECK(np_decide(&in) == NP_NOROOM); in.room = true;
    in.dead = true; CHECK(np_decide(&in) == NP_DEAD); in.dead = false;
    in.held = true; CHECK(np_decide(&in) == NP_HELD); in.held = false;
    in.failed = true; CHECK(np_decide(&in) == NP_FAILED); in.failed = false;
    in.night = true; CHECK(np_decide(&in) == NP_NIGHT);
    in.dead = true; CHECK(np_decide(&in) == NP_DEAD); in.dead = false;
    in.held = true; CHECK(np_decide(&in) == NP_NIGHT); in.held = false, in.night = false;
    CHECK(!np_midnight(23.94f) && np_midnight(23.95f) && np_midnight(24.0f) && np_midnight(0) && np_midnight(0.049f));
    CHECK(!np_midnight(0.05f) && !np_midnight(-1) && !np_midnight(12) && !np_midnight(23.5f));
    CHECK(!strcmp(NP_WHY_TXT[NP_NIGHT], "midnight / a Blood Moon on its way") && !strcmp(NP_WHY_TXT[NP_FAILED], "a call was refused / no Link"));
    in.key = false, in.room = false; CHECK(np_decide(&in) == NP_KEY);  // (the key first: nothing else is looked at)
    in.key = true, in.room = true;
    // another UI value: never starts it; one that comes while it runs keeps it running (the PR's way); a load never runs
    in.active = true;
    in.ps = 0x10; CHECK(np_decide(&in) == NP_ON);
    in.ps = NP_PS_LOAD; CHECK(np_decide(&in) == NP_NOMENU);
    in.ps = 0; CHECK(np_decide(&in) == NP_NOMENU);
    in.ps = 0x10, in.room = false; CHECK(np_decide(&in) == NP_NOROOM); in.room = true;
    in.ps = 0x10, in.dead = true; CHECK(np_decide(&in) == NP_DEAD); in.dead = false;
    in.active = false, in.ps = NP_PS_MENU;
    // a stop owed: now with a menu value up, later with another UI value up, settled with none / a load
    CHECK(np_owed(NP_PS_MENU) == 1 && np_owed(NP_PS_QUICK) == 1);
    CHECK(np_owed(0x10) == 0 && np_owed(6) == 0);
    CHECK(np_owed(0) == -1 && np_owed(NP_PS_LOAD) == -1 && np_owed(NP_PS_LOAD | NP_PS_MENU) == -1);
    // the pouch's buttons
    CHECK(np_newly(1, 0) == 1 && np_newly(4, 0) == 4 && np_newly(5, 1) == 4);
    CHECK(np_newly(1, 1) == 0 && np_newly(2, 0) == 0 && np_newly(8, 0) == 0);  // (held / B / Y)
    {  // (s290 labfix) who has the pad: the camp (nothing), BotW (closed, as ever), BotW behind the camp's "BotW Menu" (handover)
        uint64_t was = 0;
        CHECK(np_pad_newly(1, NP_PAD_CAMP, NP_PAD_CAMP, &was) == 0 && was == 0);  // (the camp's A)
        CHECK(np_pad_newly(1, NP_PAD_HANDOVER, NP_PAD_CAMP, &was) == 0 && was == 1);  // (the A that picked "BotW Menu", still down)
        CHECK(np_pad_newly(1, NP_PAD_HANDOVER, NP_PAD_HANDOVER, &was) == 0);           // (still held)
        CHECK(np_pad_newly(0, NP_PAD_HANDOVER, NP_PAD_HANDOVER, &was) == 0 && was == 0);
        CHECK(np_pad_newly(1, NP_PAD_HANDOVER, NP_PAD_HANDOVER, &was) == 1);           // (A in the pouch behind "BotW Menu": a take)
        CHECK(np_pad_newly(5, NP_PAD_HANDOVER, NP_PAD_HANDOVER, &was) == 4);           // (X on top)
        CHECK(np_pad_newly(2 | 8, NP_PAD_HANDOVER, NP_PAD_HANDOVER, &was) == 0);       // (B / Y: not the pouch's)
        was = 0;
        CHECK(np_pad_newly(4, NP_PAD_HANDOVER, NP_PAD_CAMP, &was) == 0);  // (X down at the handover: the camp's too)
        was = 0;  // (the map / a menu without the camp: as before s290, a held A given back by the camp counts, the safe side)
        CHECK(np_pad_newly(1, NP_PAD_BOTW, NP_PAD_CAMP, &was) == 1 && np_pad_newly(1, NP_PAD_BOTW, NP_PAD_BOTW, &was) == 0);
        CHECK(np_pad_newly(0, NP_PAD_BOTW, NP_PAD_BOTW, &was) == 0 && np_pad_newly(4, NP_PAD_BOTW, NP_PAD_BOTW, &was) == 4);
        CHECK(np_pad_newly(1, NP_PAD_CAMP, NP_PAD_BOTW, &was) == 0 && was == 0);  // (the camp opens: its buttons)
    }
    // attack AS names
    CHECK(np_attack_as("Attack") && np_attack_as("JumpAttack") && np_attack_as("Attack_Normal") && np_attack_as("Kick"));
    CHECK(!np_attack_as("Wait") && !np_attack_as("ArrowShoot") && !np_attack_as("attack") && !np_attack_as(NULL));
    CHECK(np_big("Enemy_Moriblin_Junior") && np_big("Enemy_Lynel_Senior") && np_big("Enemy_Giant_Junior") && np_big("Enemy_Golem_Junior"));
    CHECK(!np_big("Enemy_Bokoblin_Junior") && !np_big(NULL));
    // reach / front: enemy at the origin facing +z, Link 2 m in front
    float m[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}, d = 0;
    float L1[3] = {0, 0, 2}, L2[3] = {0, 0, -2}, L3[3] = {2, 0, 0}, L4[3] = {0, 0, 3.2f}, L5[3] = {0, 3, 2}, L6[3] = {0.3f, 0, -0.5f};
    CHECK(np_in_reach(L1, m, false, &d) && d > 1.99f && d < 2.01f);
    CHECK(!np_in_reach(L2, m, false, &d));   // (behind it)
    CHECK(!np_in_reach(L3, m, false, &d));   // (to its side: cos 0 < 0.3)
    CHECK(!np_in_reach(L4, m, false, &d));   // (3.2 m > 3.0 m)
    CHECK(np_in_reach(L4, m, true, &d));     // (a big one: 4.3 m)
    CHECK(!np_in_reach(L5, m, false, &d));   // (3 m above)
    CHECK(np_in_reach(L6, m, false, &d));    // (closer than 0.8 m: any side)
    float mn[12] = {1, 0, NAN, NAN, 0, 1, 0, 0, 0, 0, NAN, 0};
    CHECK(!np_in_reach(L1, mn, false, &d));  // (a NaN matrix never hits)
    // arrows: an archer at the origin facing +z
    CHECK(np_arrow_ms((float[3]){0, 0, 20}, m, &d) == 17 + 500 && d > 19.9f);
    CHECK(np_arrow_ms((float[3]){0, 0, 41}, m, &d) == 0);    // (too far)
    CHECK(np_arrow_ms((float[3]){0, 0, 0.3f}, m, &d) == 0);  // (too close)
    CHECK(np_arrow_ms((float[3]){10, 0, 10}, m, &d) == 0);   // (45 degrees off: cos 0.71 < 0.9)
    CHECK(np_arrow_ms((float[3]){0, 0, -20}, m, &d) == 0);   // (behind it)
    // the floor
    CHECK(np_blow(12, 4) == 8 && np_blow(5, 4) == 1 && np_blow(4, 4) == 1 && np_blow(2, 8) == 1);
    CHECK(np_blow(1, 4) == 1 && np_blow(0, 4) == 0 && np_blow(-1, 4) == -1 && np_blow(12, 0) == 12);
    CHECK(np_clamp(-3, 0, 120) == 0 && np_clamp(500, 0, 120) == 120 && np_clamp(7, 1, 2) == 2 && np_clamp(0, 1, 2) == 1);
}

// ======== part 2: the glue ========
static uint64_t g_now = 100000;
static float g_feet[3];
static bool frame(void) {  // one mod frame (a present each: the test does not model the paused double update)
    g_present++, g_now += 17, g_vfr += 2;
    return np_frame(g_now, g_feet);
}
static void calls_clear(void) { g_ncall = 0; }
static bool called(int i, uint64_t fn, uint64_t x0, uint64_t x1, uint64_t x2) {
    return i < g_ncall && g_calls[i].fn == fn && g_calls[i].x0 == x0 && g_calls[i].x1 == x1 && g_calls[i].x2 == x2;
}
static uint16_t bits(void) { return *(uint16_t *)(g_bpm + 0x168); }
// what BotW does as a menu opens (1.9.0 0x7000d461f8..): the bits, the manager's +0x163 = 1, the physics, the scene's byte, the state
static void menu_opens(uint32_t ps) {
    g_ps = ps, *(uint16_t *)(g_bpm + 0x168) |= 0x7f, g_bpm[0x163] = 1, g_phys[0x60] = 1, g_scene[0x8b8] = 1;
}
static void menu_closes(void) {  // (its own resume, 0x7000d4636c..)
    g_ps = 0, *(uint16_t *)(g_bpm + 0x168) &= (uint16_t)~0x7f, g_bpm[0x163] = 0, g_phys[0x60] = 0, g_scene[0x8b8] = 0;
}
static bool world_stopped(void) { return (bits() & 0x7f) == 0x7f && g_bpm[0x163] == 1 && g_phys[0x60] == 1 && g_scene[0x8b8] == 1; }
static bool world_runs(void) { return (bits() & 0x7f) == 0 && g_bpm[0x163] == 0 && g_phys[0x60] == 0 && g_scene[0x8b8] == 0; }
// the stop again: the physics, the bits (and +0x163 written, no call), nothing else called
static bool stop_calls(void) { return called(0, F_PHYS, A(g_phys), 1, 0) && called(1, F_SET, A(g_bpm), 0x7f, 0xdead) && g_ncall == 2; }

static void glue_setup(void) {
    reg_add(&g_ps, 4), reg_add(g_bpm, sizeof g_bpm), reg_add(g_phys, sizeof g_phys), reg_add(g_scene, sizeof g_scene);
    reg_add(&g_bpm_slot, 8), reg_add(&g_phys_obj, 8), reg_add(&g_phys_gotv, 8), reg_add(&g_scene_obj, 8), reg_add(&g_scene_gotv, 8);
    g_bpm_slot = A(g_bpm), g_phys_obj = A(g_phys), g_phys_gotv = A(&g_phys_obj), g_scene_obj = A(g_scene), g_scene_gotv = A(&g_scene_obj);
    g_ss.pause_state = A(&g_ps);
    actor_init(&g_link);
    *(int32_t *)(g_link.m + 0x848) = 12;
    as_set(&g_link, 0, "Wait"), as_set(&g_link, 1, "Wait");
    for (int i = 0; i < 3; i++) actor_init(&g_en[i]), as_set(&g_en[i], 0, "Wait");
    for (int i = 0; i < 3; i++) reg_add(g_en_name[i], strlen(g_en_name[i]) + 1);  // (s290: the name is read with a bound, its first byte checked)
    NET.on = NET.melty = true, NET.slot[1].used = true, NET.slot[1].p.st = PEER_LIVE;
}

static void test_glue(void) {
    glue_setup();
    CHECK(g_np_key == 1);  // (s291: the compiled default is ON)
    g_np_key = 0;          // (the key-0 cases below)
    // key 0: nothing read, called or written, also with a menu up and a friend in the room
    menu_opens(NP_PS_MENU);
    g_rd_calls = g_site_calls = 0, calls_clear(), log_clear();
    for (int i = 0; i < 5; i++) CHECK(!frame());
    CHECK(g_rd_calls == 0 && g_site_calls == 0 && g_ncall == 0 && g_nset == 0 && g_drive_off == 0 && g_nlog == 0 && world_stopped());
    menu_closes();
    // the key: one line per change, refused on an exe without the half
    g_has_exe = false;
    CHECK(np_live_set("netnopause", 1) && LOGGED("NEEDS THE s289 nopause EXE") && LOGGED("REFUSED"));
    menu_opens(NP_PS_MENU), calls_clear();
    CHECK(!frame() && g_ncall == 0 && world_stopped());
    menu_closes();
    np_live_set("netnopause", 0);
    // an exe with sw_nopause_set but no sw_nopause_counts (before round 3: it never writes the scene state): refused too
    NOPA.tried = false, g_has_exe = true, g_has_counts = false, log_clear();
    CHECK(np_live_set("netnopause", 1) && LOGGED("NEEDS THE s289 nopause EXE") && LOGGED("REFUSED"));
    menu_opens(NP_PS_MENU), calls_clear();
    CHECK(!frame() && g_ncall == 0 && g_set_world != 1 && world_stopped());  // (only ever told "off")
    menu_closes();
    np_live_set("netnopause", 0);
    NOPA.tried = false, g_has_counts = true;  // (a new DLL copy on the new exe)
    log_clear();
    CHECK(np_live_set("netnopause", 1) && LOGGED("netnopause 1: in a Melty room") && g_nlog == 1);
    CHECK(np_live_set("netnopause", 1) && g_nlog == 1);  // (the live file's rewrite: no second line)
    CHECK(np_live_set("netnopauselog", 1) && np_live_set("netnopausedmg", 4) && !np_live_set("netnopausex", 1));
    // playing: no menu, nothing called
    calls_clear();
    CHECK(!frame() && g_ncall == 0 && g_set_world == 0);
    // the pause menu opens: the world's resume, once per pause of the bits
    menu_opens(NP_PS_MENU), calls_clear(), log_clear(), g_drive_off = 0;
    CHECK(frame());
    CHECK(called(0, F_PHYS, A(g_phys), 0, 0) && called(1, F_CLEAR, A(g_bpm), 0x7f, 0xdead) && called(2, F_PROCS, A(g_bpm), 0xdead, 0xdead) && g_ncall == 3);
    CHECK(world_runs() && g_set_world == 1 && g_set_live == 0 && g_drive_off == 1);
    CHECK(g_feet[0] == g_link_pos[0] && g_feet[1] == g_link_pos[1] && g_feet[2] == g_link_pos[2]);
    CHECK(LOGGED("[nopause] on: the UI's pause 0x2"));
    calls_clear();
    CHECK(frame() && g_ncall == 0 && g_drive_off == 2);  // (bits clear: nothing to resume)
    *(uint16_t *)(g_bpm + 0x168) |= 0x7f;                // (the game sets them again)
    CHECK(frame() && g_ncall == 3 && world_runs());
    // the 1 s line: hour, an enemy's walk, VFR updates per present
    g_nen = 1, en_place(0, 0, 10, 0, -1);
    log_clear();
    for (int i = 0; i < 130; i++) {  // (lines at ~1 s and ~2 s; the exe zeroes twice and finds one held, a frame)
        g_hour += 0.0167f / 60, g_cnt[0] += 2, g_cnt[1] += 1;
        if (i == 80) en_place(0, 0, 8, 0, -1);
        frame();
    }
    CHECK(LOGGED("1 s behind the menu: hour") && LOGGED("Enemy_Bokoblin_Junior moved 2.00 m, 1 enemies listed") && LOGGED("VFR updates 2.00 a present"));
    {
        const char *z = strstr(g_log, "scene state zeroed ");
        unsigned long long zn = 0, hn = 0;
        CHECK(z && sscanf(z, "scene state zeroed %llu held %llu", &zn, &hn) == 2 && zn == 2 * hn && hn >= 50 && hn <= 62);
        z = z ? strstr(z + 1, "scene state zeroed ") : NULL;  // (the 2 s line: one second of frames)
        CHECK(z && sscanf(z, "scene state zeroed %llu held %llu", &zn, &hn) == 2 && zn == 2 * hn && hn >= 58 && hn <= 60);
    }
    if (!LOGGED("moved 2.00 m")) printf("%s", g_log);
    // a blow: Attack starts in front of Link within reach, lands 167 ms later: 12 -> 8
    en_place(0, 0, 2, 0, -1), as_set(&g_en[0], 0, "Attack");
    log_clear();
    for (int i = 0; i < 5; i++) frame();  // (85 ms)
    CHECK(*(int32_t *)(g_link.m + 0x848) == 12);
    for (int i = 0; i < 8; i++) frame();  // (221 ms)
    CHECK(*(int32_t *)(g_link.m + 0x848) == 8 && LOGGED("Enemy_Bokoblin_Junior's blow hit us behind the menu") && LOGGED("life 12 -> 8"));
    for (int i = 0; i < 20; i++) frame();
    CHECK(*(int32_t *)(g_link.m + 0x848) == 8);  // (one AS = one blow)
    // the same AS again after another one: a second blow; guarding blocks it
    as_set(&g_en[0], 0, "Wait"), frame(), as_set(&g_en[0], 0, "Attack"), as_set(&g_link, 1, "GuardWait");
    for (int i = 0; i < 14; i++) frame();
    CHECK(*(int32_t *)(g_link.m + 0x848) == 8 && LOGGED("blocked (Link guards)"));
    as_set(&g_link, 1, "Wait");
    // behind it: a miss
    as_set(&g_en[0], 0, "Wait"), frame(), en_place(0, 0, 2, 0, 1), as_set(&g_en[0], 0, "Kick");
    log_clear();
    for (int i = 0; i < 14; i++) frame();
    CHECK(*(int32_t *)(g_link.m + 0x848) == 8 && LOGGED("blow behind the menu missed"));
    // a big one: twice; then the floor at 1
    g_nen = 2, en_place(1, 3.5f, 0, -1, 0), as_set(&g_en[1], 0, "Attack");
    for (int i = 0; i < 14; i++) frame();
    CHECK(*(int32_t *)(g_link.m + 0x848) == 1);  // (8 - 8 would be 0: kept at 1)
    as_set(&g_en[1], 0, "Wait"), frame(), as_set(&g_en[1], 0, "JumpAttack");
    log_clear();
    for (int i = 0; i < 14; i++) frame();
    CHECK(*(int32_t *)(g_link.m + 0x848) == 1 && LOGGED("never the last quarter heart"));
    *(int32_t *)(g_link.m + 0x848) = 12;
    // an archer 20 m off facing him: one arrow, d / 40 s later
    g_nen = 3, as_set(&g_en[0], 0, "Wait"), as_set(&g_en[1], 0, "Wait"), en_place(2, 20, 0, -1, 0), as_set(&g_en[2], 0, "ArrowShoot");
    log_clear();
    for (int i = 0; i < 20; i++) frame();  // (340 ms < 517)
    CHECK(*(int32_t *)(g_link.m + 0x848) == 12);
    for (int i = 0; i < 20; i++) frame();  // (680 ms)
    CHECK(*(int32_t *)(g_link.m + 0x848) == 8 && LOGGED("Enemy_Bokoblin_Bow's arrow hit us"));
    for (int i = 0; i < 60; i++) frame();
    CHECK(*(int32_t *)(g_link.m + 0x848) == 8);  // (the same ArrowShoot: once)
    // (s290 rv3fix, review 3 #6) an enemy we follow must still be the one we saw: unit back pointer, id, not being deleted
    *(int32_t *)(g_link.m + 0x848) = 12;
    g_nen = 1, en_place(0, 0, 2, 0, -1), as_set(&g_en[0], 0, "Wait");
    for (int i = 0; i < 3; i++) frame();
    {
        uint32_t *uid = (uint32_t *)(g_en[0].unit + 0x40), keep_id = *uid;
        uint64_t *uact = (uint64_t *)(g_en[0].unit + 0x48), keep_act = *uact;
        uint64_t *aunit = (uint64_t *)(g_en[0].m + 0x68), keep_unit = *aunit;
        uint32_t gone0 = NOPA.gone;
        for (int why = 0; why < 4; why++) {
            if (why == 0) *uid = keep_id + 1;           // (its memory used again: the unit holds another id)
            if (why == 1) *uact = keep_act + 0x10;      // (the unit points at another actor)
            if (why == 2) g_en[0].m[0x64] = 3;          // (being deleted)
            if (why == 3) *aunit = 0;                   // (no unit)
            as_set(&g_en[0], 0, "Attack");
            log_clear();
            for (int i = 0; i < 14; i++) frame();
            CHECK(*(int32_t *)(g_link.m + 0x848) == 12 && !LOGGED("'s blow") && !LOGGED("plays"));  // (nothing read of it, no blow)
            *uid = keep_id, *uact = keep_act, g_en[0].m[0x64] = 1, *aunit = keep_unit;
            as_set(&g_en[0], 0, "Wait");
            for (int i = 0; i < 3; i++) frame();
        }
        CHECK(NOPA.gone > gone0);
        // the real one again: its entry was dropped and made afresh; a blow lands as before
        as_set(&g_en[0], 0, "Attack");
        for (int i = 0; i < 14; i++) frame();
        CHECK(*(int32_t *)(g_link.m + 0x848) == 8);
    }
    // the unit tests alone
    CHECK(np_unit_ok(0x7000001000ull, 0x7000001000ull, 7, 7, 7, 1) && !np_unit_ok(0, 0, 7, 7, 7, 1));
    CHECK(!np_unit_ok(0x7000001000ull, 0x7000001010ull, 7, 7, 7, 1) && !np_unit_ok(0x7000001000ull, 0x7000001000ull, 8, 7, 7, 1));
    CHECK(!np_unit_ok(0x7000001000ull, 0x7000001000ull, 7, 7, 9, 1) && !np_unit_ok(0x7000001000ull, 0x7000001000ull, 7, 7, 7, 3));
    // an entry made for an id is not kept for another enemy on the same memory
    {
        uint32_t keep = *(uint32_t *)(g_en[0].m + 0x60);
        NpEn *e1 = np_en(A(&g_en[0]), g_present, keep);
        e1->blow_at = 5;
        NpEn *e2 = np_en(A(&g_en[0]), g_present, keep + 1);
        CHECK(e2->actor == A(&g_en[0]) && e2->id == keep + 1 && e2->blow_at == 0);
        memset(e2, 0, sizeof *e2);
    }
    // a name read with a bound: 47 letters at most, and not past the page the name's own page ends at
    {
        static char pg[8192] __attribute__((aligned(4096)));
        char out[NP_NAME_MAX];
        memset(pg, 'A', sizeof pg);
        strcpy(pg + 100, "Enemy_Moriblin_Junior");
        g_nm_lo = (uint64_t)(uintptr_t)pg, g_nm_hi = g_nm_lo + sizeof pg;
        np_copy_name(pg + 100, out, np_ok_adapter_test);
        CHECK(!strcmp(out, "Enemy_Moriblin_Junior"));
        g_nm_hi = g_nm_lo + 4096;  // (only the first page is readable; the name starts 6 bytes before its end, no NUL)
        np_copy_name(pg + 4090, out, np_ok_adapter_test);
        CHECK(!strcmp(out, "AAAAAA") && strlen(out) == 6);
        np_copy_name(pg + 4096, out, np_ok_adapter_test);  // (unreadable from the first byte)
        CHECK(out[0] == 0);
        np_copy_name(NULL, out, np_ok_adapter_test);
        CHECK(out[0] == 0);
        g_nm_hi = g_nm_lo + sizeof pg;
        np_copy_name(pg + 200, out, np_ok_adapter_test);  // (readable, no NUL for 8000 bytes: cut at 47)
        CHECK(strlen(out) == NP_NAME_MAX - 1);
    }
    g_nen = 0;
    // the pouch: A with the pause menu up stops the world as it was stopped, until the menu goes
    calls_clear(), log_clear();
    g_held = 1;
    CHECK(!frame());
    CHECK(stop_calls());
    CHECK(world_stopped() && g_set_world == 0 && LOGGED("the pouch's take") && LOGGED("A / X in the pouch"));
    calls_clear(), g_held = 0;
    for (int i = 0; i < 5; i++) CHECK(!frame());
    CHECK(g_ncall == 0 && world_stopped());  // (stays stopped, no more calls)
    menu_closes(), frame();
    menu_opens(NP_PS_MENU), calls_clear();
    CHECK(frame() && g_ncall == 3 && world_runs());  // (a new menu: afresh)
    // A already down as the menu opened is not a take; the KH2 camp's A is the camp's; X counts
    menu_closes(), g_held = 1, frame(), menu_opens(NP_PS_MENU);
    CHECK(frame() && world_runs());
    g_held = 0, frame();
    CAMP.page.state = CP_OPEN, g_held = 1;
    CHECK(frame() && frame() && world_runs());  // (the camp's A: the camp eats it, BotW's menu never sees it)
    CAMP.page.state = CP_CLOSED;
    CHECK(!frame() && world_stopped());  // (the camp gave a still-held A back to BotW's menu: a new press there, the safe side)
    g_held = 0, menu_closes(), frame(), menu_opens(NP_PS_MENU);
    CHECK(frame() && world_runs());
    g_held = 4;
    CHECK(!frame() && world_stopped());  // (X)
    g_held = 0, menu_closes(), frame();
    // (s290 labfix, lab s290 test 1 step 5) BotW's pause menu reached through the KH2 camp's "BotW Menu": the camp opens over
    // the pause (CP_OPEN, it eats the pad), A on "BotW Menu" hands over (CP_HANDOVER until the pause ends): the A still down from
    // that pick is the camp's (the world keeps running), a new A / X in BotW's pouch then stops the world as on the map
    menu_opens(NP_PS_MENU), CAMP.page.state = CP_OPEN;
    CHECK(frame() && world_runs());
    g_held = 1;
    CHECK(frame() && world_runs());  // (A on "BotW Menu": the camp's)
    CAMP.page.state = CP_HANDOVER;
    CHECK(frame() && frame() && world_runs());  // (the same A, still down after the handover: not a take)
    g_held = 0;
    CHECK(frame() && frame() && world_runs());
    log_clear(), calls_clear();
    g_held = 1;  // (A on an item in BotW's pouch)
    CHECK(!frame() && world_stopped() && LOGGED("the pouch's take") && stop_calls());
    g_held = 0;
    CHECK(!frame() && world_stopped());  // (until the menu goes)
    menu_closes(), CAMP.page.state = CP_CLOSED, frame();
    menu_opens(NP_PS_MENU), CAMP.page.state = CP_OPEN, frame(), CAMP.page.state = CP_HANDOVER;
    CHECK(frame() && world_runs());  // (a new menu through the camp: afresh)
    g_held = 4;
    CHECK(!frame() && world_stopped());  // (X behind "BotW Menu")
    g_held = 0, menu_closes(), CAMP.page.state = CP_CLOSED, frame();
    {  // the key 0 behind "BotW Menu": nothing called, the world stopped as the menu stops it (as ever)
        int k = g_np_key;
        g_np_key = 0, menu_opens(NP_PS_MENU), CAMP.page.state = CP_HANDOVER, calls_clear();
        g_held = 1, frame(), g_held = 0, frame(), g_held = 4, frame();
        CHECK(g_ncall == 0);
        g_held = 0, menu_closes(), CAMP.page.state = CP_CLOSED, frame();
        g_np_key = k;
    }
    // the quick menus run too; A there is not the pouch's
    menu_opens(NP_PS_QUICK);
    CHECK(frame() && world_runs());
    g_held = 1;
    CHECK(frame() && world_runs());
    g_held = 0;
    // another UI value while it runs: it keeps running (the PR's way), logged once; it pauses the world its own way: undone
    g_ps = 0x10, log_clear(), calls_clear();
    CHECK(frame() && LOGGED("the UI's pause 0x10 came while the world runs") && g_ncall == 0 && world_runs() && g_set_world == 1);
    log_clear();
    *(uint16_t *)(g_bpm + 0x168) |= 0x7f;
    CHECK(frame() && g_nlog == 0 && g_ncall == 3 && world_runs());
    // the friend leaves while it is up: no stop laid on it (owed), the stop comes once the menu's value is back
    NET.slot[1].p.st = PEER_NONE, calls_clear(), log_clear();
    CHECK(!frame() && g_ncall == 0 && world_runs() && g_set_world == 0 && LOGGED("once a menu value is back"));
    calls_clear();
    CHECK(!frame() && g_ncall == 0);
    g_ps = NP_PS_QUICK, calls_clear();
    CHECK(!frame() && stop_calls() && world_stopped());
    calls_clear();
    CHECK(!frame() && g_ncall == 0);
    NET.slot[1].p.st = PEER_LIVE, calls_clear();
    CHECK(frame() && world_runs() && g_ncall == 3);
    // ... and once the UI pause closes with the menu (no UI pause): settled, the game's own resume, no stop
    g_ps = 0x10, frame(), NET.slot[1].p.st = PEER_NONE, frame(), calls_clear();
    menu_closes();
    CHECK(!frame() && g_ncall == 0);
    g_ps = NP_PS_MENU;  // (a later menu while the friend is away: nothing owed any more)
    CHECK(!frame() && g_ncall == 0);
    NET.slot[1].p.st = PEER_LIVE, menu_closes(), frame();
    // another UI value from play never starts it (one line); a menu after it does
    g_ps = 0x10, log_clear(), calls_clear();
    *(uint16_t *)(g_bpm + 0x168) |= 0x7f;
    CHECK(!frame() && g_ncall == 0 && LOGGED("the UI's pause 0x10 is not a menu") && g_set_world == 0);
    menu_closes(), frame(), menu_opens(NP_PS_QUICK), calls_clear();
    CHECK(frame() && world_runs() && g_ncall == 3);
    // the friend leaves (and fading = gone): stopped again at once
    NET.slot[1].p.st = PEER_FADING, calls_clear(), log_clear();
    CHECK(!frame() && world_stopped() && called(0, F_PHYS, A(g_phys), 1, 0) && LOGGED("no friend in the Melty room"));
    calls_clear();
    CHECK(!frame() && g_ncall == 0);
    NET.slot[1].p.st = PEER_LIVE;
    CHECK(frame() && world_runs());
    // not connected to the room: the same
    g_conn = false;
    CHECK(!frame() && world_stopped());
    g_conn = true;
    CHECK(frame() && world_runs());
    // DOWNED (revive) / dead
    NET.downed = true;
    CHECK(!frame() && world_stopped());
    NET.downed = false;
    CHECK(frame() && world_runs());
    // a load from the map: the game's own way (no stop calls), the exe's permission dropped
    g_ps = NP_PS_LOAD | NP_PS_MENU, calls_clear();
    CHECK(!frame() && g_ncall == 0 && g_set_world == 0);
    menu_closes(), frame();
    // a refused call: this menu stays stopped, the next one runs again
    menu_opens(NP_PS_MENU), g_call_ok = false, log_clear();
    CHECK(!frame() && LOGGED("REFUSED"));
    g_call_ok = true, calls_clear();
    for (int i = 0; i < 3; i++) CHECK(!frame());
    CHECK(g_ncall == 0);
    menu_closes(), frame(), menu_opens(NP_PS_MENU);
    CHECK(frame() && world_runs());
    // not inside linkpos_frame (never in game: the mod frame is): call refused, the same
    // no Link body: nothing stands for him, stopped again
    g_link_ok = false, calls_clear();
    CHECK(!frame() && world_stopped());
    g_link_ok = true, menu_closes(), frame(), menu_opens(NP_PS_MENU);
    CHECK(frame() && world_runs());
    // Sora mode off (net_sora_off: feet NULL)
    g_present++;
    CHECK(!np_frame(g_now, NULL) && world_stopped());
    CHECK(frame() && world_runs());
    // midnight (BotW never crosses it with a menu up: the new day, the Blood Moon's Demo011_0): stopped again for this menu
    g_hour = 23.96f, calls_clear(), log_clear();
    CHECK(!frame() && stop_calls() && world_stopped());
    CHECK(LOGGED("midnight (hour 23.960): the world stays stopped") && LOGGED("again behind the menu (midnight / a Blood Moon on its way)"));
    g_hour = 0.5f, calls_clear();
    for (int i = 0; i < 3; i++) CHECK(!frame());
    CHECK(g_ncall == 0 && world_stopped());  // (for the rest of this menu, also past the window)
    menu_closes(), frame(), menu_opens(NP_PS_MENU);
    CHECK(frame() && world_runs());  // (the next menu: afresh)
    // a menu opened in the window never starts it (no "on" line, nothing called)
    menu_closes(), frame(), g_hour = 0.02f, menu_opens(NP_PS_MENU), calls_clear(), log_clear();
    CHECK(!frame() && g_ncall == 0 && world_stopped() && LOGGED("midnight (hour 0.020)") && !LOGGED("[nopause] on:"));
    CHECK(!frame() && g_ncall == 0);
    menu_closes(), frame(), g_hour = 12.0f, menu_opens(NP_PS_MENU);
    CHECK(frame() && world_runs());
    // a Blood Moon of the room's on its way here (sora_world.h WORLD.bm): the same, and not before the menu goes
    g_bm_busy = true, calls_clear(), log_clear();
    CHECK(!frame() && stop_calls() && world_stopped() && LOGGED("a Blood Moon of the room's on its way (hour 12.000)"));
    g_bm_busy = false;
    CHECK(!frame() && world_stopped());
    menu_closes(), frame(), menu_opens(NP_PS_MENU);
    CHECK(frame() && world_runs());
    // the key 0 while it runs: stopped again once, then nothing
    calls_clear(), log_clear();
    CHECK(np_live_set("netnopause", 0) && LOGGED("netnopause 0"));
    CHECK(!frame() && world_stopped() && stop_calls());
    calls_clear(), g_rd_calls = 0;
    for (int i = 0; i < 3; i++) CHECK(!frame());
    CHECK(g_ncall == 0 && g_rd_calls == 0);
    // netnopausestep: to the exe, refused without it; unload puts it back and stops a running world
    CHECK(np_live_set("netnopausestep", 2) && g_step == 2 && g_np_step == 2);
    np_live_set("netnopause", 1);
    CHECK(frame() && world_runs());
    np_unload();
    CHECK(world_stopped() && g_step == 1 && g_set_world == 0 && !NOPA.active);
    menu_closes();
    // unload with another UI value up while it runs: no stop laid on it, one line, the exe's permission dropped
    menu_opens(NP_PS_MENU), frame(), g_ps = 0x10, frame(), calls_clear(), log_clear();
    CHECK(NOPA.active && world_runs());
    np_unload();
    CHECK(g_ncall == 0 && LOGGED("unload with the UI's pause 0x10 up") && g_set_world == 0 && !NOPA.active && !NOPA.owed);
    menu_closes();
    g_np_step = 1, g_has_step = false, NOPA.tried = false, log_clear();
    CHECK(np_live_set("netnopausestep", 2) && LOGGED("REFUSED") && g_np_step == 1);
    g_has_step = true;
    // an exe without the scene state rows for this game version: refused, never runs
    np_live_set("netnopause", 0), NOPA.tried = false, g_state_rows = false, log_clear();
    CHECK(np_live_set("netnopause", 1) && LOGGED("a site is 0 on this game version") && !LOGGED("NEEDS"));
    menu_opens(NP_PS_MENU), calls_clear();
    CHECK(!frame() && g_ncall == 0 && world_stopped());
    // netnopausedmg clamps and logs a change only
    log_clear();
    CHECK(np_live_set("netnopausedmg", 999) && g_np_dmg == 120 && g_nlog == 1);
    CHECK(np_live_set("netnopausedmg", 120) && g_nlog == 1);
    CHECK(np_live_set("netnopausedmg", -5) && g_np_dmg == 0);
}

int main(void) {
    test_core();
    test_glue();
    printf("nopause_test: %d checks, %d failed: %s\n", runs, fails, fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

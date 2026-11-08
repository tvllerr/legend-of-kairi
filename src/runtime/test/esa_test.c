// host test for runtime/sora_esa.h (s175 DLL attack runner) against a FAKE SwApi: fake node memory, fake AI tree, fake vcall.
//   Windows: clang -O1 -g -Wall -Wextra -Iruntime runtime/test/esa_test.c -o build_h/esa_test.exe
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
static char g_log[32768];
static void test_log(const char *f, ...) {
    va_list a; va_start(a, f);
    size_t n = strlen(g_log);
    vsnprintf(g_log + n, sizeof g_log - n, f, a);
    va_end(a);
}
#define LOG test_log
#define ESA_NO_EXE
static unsigned long long GetTickCount64(void) { return 0; }  // (s176: the enter / leave lines carry a ms time)
#include "sora_esa.h"

#define VT_A 0xA000ull
#define VT_B 0xB000ull
#define VT_X 0xC000ull
#define ACTOR 0xAC70ull
#define NODE_A 0x1000ull
#define NODE_B 0x2000ull
#define NODE_X 0x3000ull
// ---- fake memory: three 0x40-byte nodes ----
static uint8_t f_mem[3][0x40];
static uint8_t *mem_at(uint64_t a, size_t n) {
    static const uint64_t base[3] = {NODE_A, NODE_B, NODE_X};
    for (int i = 0; i < 3; i++) if (a >= base[i] && a + n <= base[i] + 0x40) return f_mem[i] + (a - base[i]);
    return NULL;
}
static void mk_node(uint64_t node, uint64_t vt, uint64_t actor, uint16_t idx) {
    uint8_t *m = mem_at(node, 0x40);
    memset(m, 0, 0x40);
    memcpy(m, &vt, 8), memcpy(m + 8, &actor, 8), memcpy(m + 0x18, &idx, 2);
}
static size_t f_readable(uint64_t a, size_t n) { return mem_at(a, n) ? n : 0; }
static bool f_read(uint64_t a, void *out, size_t n) { uint8_t *m = mem_at(a, n); if (!m) return false; memcpy(out, m, n); return true; }
static bool f_write(uint64_t a, const void *in, size_t n) { uint8_t *m = mem_at(a, n); if (!m) return false; memcpy(m, in, n); return true; }
static uint64_t f_now = 1000;
static uint64_t f_frame_id(void) { return f_now; }
static uint64_t f_site(const char *n) { return !strcmp(n, "atkact_attack_vt") ? VT_A : !strcmp(n, "atkact_stepdouble_vt") ? VT_B : 0; }
static uint64_t f_site0(const char *n) { return 0; }
static bool f_write_fail(uint64_t a, const void *in, size_t n) { return false; }
static int f_es_found, f_es_pup, f_in_frame_v = 1;
static uint64_t f_es_actor = ACTOR;
static bool f_esync_actor(uint32_t hid, uint64_t *actor, int *puppet) { if (!f_es_found) return false; *actor = f_es_actor, *puppet = f_es_pup; return true; }
static bool f_in_frame(void) { return f_in_frame_v; }
static int f_walks;
static int f_ai_walk(Cpu *c, uint64_t actor, SwAiNode *out, int max) {
    f_walks++;
    SwAiNode t[] = {{0x9000, 0x9100, 1, 0, 0}, {NODE_A, VT_A, 5, 2, 1}, {NODE_X, VT_X, 7, 2, 1}, {NODE_B, VT_B, 6, 2, 1}};
    int n = 4 < max ? 4 : max;
    memcpy(out, t, (size_t)n * sizeof *t);
    return n;
}
static struct { uint64_t node; int slot; } f_vc[256];
static int f_nvc, f_fin_on_calc, f_vc_ok = 1;
static bool f_vcall(Cpu *c, uint64_t obj, int slot, const SwArgs *a, SwRet *r) {
    if (!f_vc_ok) return false;
    if (f_nvc < 256) f_vc[f_nvc].node = obj, f_vc[f_nvc++].slot = slot;
    CHECK(a->scratch >= 0x600 && a->nx == 2 && (a->x_scratch & 2) && a->x[1] == 0);
    if (slot == 31 && f_fin_on_calc) *mem_at(obj + 0x1b, 1) |= 1;
    return true;
}
static void *f_tap, *f_tap_ai; static int f_tap_calls;
static bool f_tap_set(int tap, void *fn) {  // (s181: per tap id)
    if (tap == SW_TAP_PUPPET_ATTACK) f_tap_calls++, f_tap = fn;
    else if (tap == SW_TAP_PUPPET_AI) f_tap_ai = fn;
    return tap == SW_TAP_PUPPET_ATTACK || tap == SW_TAP_PUPPET_AI;
}
static int f_atk_on = -9, f_atk_calls;
static int f_atkfn(int on, const uint64_t *v, int n) { f_atk_on = on; f_atk_calls++; return on ? 2 : 0; }

static SwApi f_api;
static EsaCtx *X;
static Cpu *CPU = (Cpu *)0x1;
static int count(int slot) { int k = 0; for (int i = 0; i < f_nvc; i++) k += f_vc[i].slot == slot; return k; }
static int tap(uint32_t hid, uint64_t actor, uint16_t action, uint8_t chg, int alive) {
    NetEsEnt t;
    memset(&t, 0, sizeof t);
    t.hid = hid, t.action = action, t.anim_chg[0] = chg;
    return esa_frame(X, CPU, hid, actor, &t, alive);
}
static void reset(int with_api) {
    free(X);
    X = calloc(1, sizeof *X);
    X->mode = -1, X->pending = -1, X->want = -1, X->log = 1;
    X->api = with_api ? &f_api : NULL;
    X->atkfn = f_atkfn;
    f_nvc = f_walks = f_tap_calls = f_atk_calls = 0, f_tap = f_tap_ai = NULL, f_atk_on = -9, f_fin_on_calc = 0, f_vc_ok = 1, f_now = 1000;
    g_log[0] = 0;
    mk_node(NODE_A, VT_A, ACTOR, 5), mk_node(NODE_B, VT_B, ACTOR, 6), mk_node(NODE_X, VT_X, ACTOR, 7);
    *mem_at(NODE_A + 0x1b, 1) = 3;  // finished + failed set: enter must clear them
}

int main(void) {
    memset(&f_api, 0, sizeof f_api);
    f_api.version = SW_API_VERSION, f_api.size = sizeof f_api;
    f_api.readable = f_readable, f_api.read = f_read, f_api.write = f_write, f_api.frame_id = f_frame_id, f_api.site = f_site;
    f_api.ai_walk = f_ai_walk, f_api.vcall = f_vcall, f_api.tap_set = f_tap_set;

    printf("mode 2 sets the tap, exe runner off\n");
    reset(1);
    esa_set_mode(X, 2);
    CHECK(X->mode == 2 && f_tap == (void *)esa_tap_fn && f_atk_on == 0 && X->nvt == 2);

    printf("no action -> nothing\n");
    CHECK(tap(1, ACTOR, 0, 0, 1) == 0);
    CHECK(f_walks == 0 && f_nvc == 0);

    printf("owner attack idx 5: walk once, enter + calc\n");
    f_now = 1100;
    CHECK(tap(1, ACTOR, 5, 1, 1) == 1);
    CHECK(f_walks == 1 && count(12) == 1 && count(31) == 1 && f_vc[0].node == NODE_A && f_vc[0].slot == 12);
    CHECK((*mem_at(NODE_A + 0x1b, 1) & 3) == 0);
    CHECK(strstr(g_log, "runs its own attack idx 5") && strstr(g_log, "(dll)") && strstr(g_log, "esync attack walk of ac70: 2 attack nodes in 4 distinct nodes (dll)"));
    f_now = 1101;
    CHECK(tap(1, ACTOR, 5, 1, 1) == 1);
    CHECK(f_walks == 1 && count(12) == 1 && count(31) == 2);

    printf("finished bit -> leave once, no re-enter until chg changed + gap passed\n");
    f_fin_on_calc = 1;
    f_now = 1102;
    CHECK(tap(1, ACTOR, 5, 1, 1) == 0);
    CHECK(count(14) == 1 && X->n.fin == 1 && strstr(g_log, "attack idx 5 left (finished) (dll)"));
    f_fin_on_calc = 0;
    f_now = 1103;
    CHECK(tap(1, ACTOR, 5, 1, 1) == 0 && count(12) == 1);   // same chg
    f_now = 1104;
    CHECK(tap(1, ACTOR, 5, 2, 1) == 0 && count(12) == 1);   // chg changed but inside the gap
    f_now = 1102 + ESA_GAP;
    CHECK(tap(1, ACTOR, 5, 1, 1) == 0 && count(12) == 1);   // gap passed, chg still the absorbed one
    CHECK(tap(1, ACTOR, 5, 2, 1) == 1 && count(12) == 2);   // both -> runs again

    printf("swap 5 -> 6\n");
    reset(1); esa_set_mode(X, 2);
    f_now = 2000; tap(2, ACTOR, 5, 1, 1);
    f_now = 2001; CHECK(tap(2, ACTOR, 6, 1, 1) == 1);
    CHECK(count(14) == 1 && count(12) == 2 && X->n.swap == 1 && X->pup.e[0].atk_node == NODE_B);
    CHECK(strstr(g_log, "left (owner swapped attack) (dll)"));

    printf("owner leaves the attack -> leave\n");
    f_now = 2002; CHECK(tap(2, ACTOR, 0, 1, 1) == 0);
    CHECK(count(14) == 2 && strstr(g_log, "left (owner's action changed) (dll)"));

    printf("3 s cap\n");
    reset(1); esa_set_mode(X, 2);
    f_now = 3000; tap(3, ACTOR, 5, 1, 1);
    for (f_now = 3001; f_now <= 3000 + ESA_CAP + 1; f_now++) tap(3, ACTOR, 5, 1, 1);  // (updates keep coming: the stale rule stays out of it)
    CHECK(X->n.cap == 1 && strstr(g_log, "left (3 s cap) (dll)"));

    printf("dead -> leave\n");
    reset(1); esa_set_mode(X, 2);
    f_now = 4000; tap(4, ACTOR, 5, 1, 1);
    f_now = 4001; CHECK(tap(4, ACTOR, 5, 1, 0) == 0);
    CHECK(count(14) == 1 && esa_running(X) == 0);

    printf("actor pointer changes -> forget\n");
    reset(1); esa_set_mode(X, 2);
    f_now = 5000; tap(5, ACTOR, 5, 1, 1);
    CHECK(esa_running(X) == 1);
    f_now = 5001; tap(5, ACTOR + 0x100, 0, 1, 1);
    CHECK(esa_running(X) == 0 && X->n.forgot == 1 && count(14) == 0);

    printf("node of another actor -> no enter\n");
    reset(1); esa_set_mode(X, 2);
    mk_node(NODE_A, VT_A, 0xDEAD, 5);
    f_now = 6000; CHECK(tap(6, ACTOR, 5, 1, 1) == 0 && count(12) == 0);

    printf("vcall refused -> bad fn, no run\n");
    reset(1); esa_set_mode(X, 2);
    f_vc_ok = 0;
    f_now = 7000; CHECK(tap(7, ACTOR, 5, 1, 1) == 0 && X->n.bad_fn == 1 && esa_running(X) == 0);

    printf("counter line every 600 frames\n");
    reset(1); esa_set_mode(X, 2);
    g_log[0] = 0;
    f_now = 8000; tap(8, ACTOR, 0, 0, 1);
    CHECK(strstr(g_log, "esync attack (dll): mode 2"));
    char *first = strstr(g_log, "esync attack (dll)");
    f_now = 8100; tap(8, ACTOR, 0, 0, 1);
    CHECK(first && strstr(first + 5, "esync attack (dll)") == NULL);
    f_now = 8601; tap(8, ACTOR, 0, 0, 1);
    CHECK(first && strstr(first + 5, "esync attack (dll)") != NULL);

    printf("api NULL: mode 2 falls back to 1\n");
    reset(0);
    esa_set_mode(X, 2);
    CHECK(X->mode == 1 && f_atk_on == 1 && f_tap_calls == 0 && strstr(g_log, "this exe has no sw_api"));

    printf("modes 0 / 1 as before\n");
    reset(1);
    esa_set_mode(X, 1);
    CHECK(X->mode == 1 && f_atk_on == 1 && f_tap_calls == 0 && f_tap == NULL);
    esa_set_mode(X, 0);
    CHECK(X->mode == 0 && f_atk_on == 0);
    esa_set_mode(X, 5);
    CHECK(X->mode == 1);

    printf("2 -> 1 with an attack running: tap drains, then exe runner\n");
    reset(1); esa_set_mode(X, 2);
    f_now = 9000; tap(9, ACTOR, 5, 1, 1);
    esa_set_mode(X, 1);
    CHECK(X->mode == 2 && X->pending == 1);
    f_now = 9001; tap(9, ACTOR, 5, 1, 1);
    CHECK(count(14) == 1 && X->mode == 1 && f_tap == NULL && f_atk_on == 1);

    printf("2 -> 0 idle: immediate\n");
    reset(1); esa_set_mode(X, 2); esa_set_mode(X, 0);
    CHECK(X->mode == 0 && f_tap == NULL && f_atk_on == 0);

    printf("drain timeout\n");
    reset(1); esa_set_mode(X, 2);
    f_now = 9500; tap(10, ACTOR, 5, 1, 1);
    esa_set_mode(X, 0);
    f_now = 9500 + ESA_DRAIN_FRAMES + 1; esa_tick(X);
    CHECK(X->mode == 0 && esa_running(X) == 0);

    printf("api present but site() returns 0: 2 acts as 1 with the right log\n");
    reset(1);
    f_api.site = f_site0;
    esa_set_mode(X, 2);
    CHECK(X->mode == 1 && X->nvt == 0 && f_tap_calls == 0 && strstr(g_log, "sw_api present but no attack classes (site lookup 0)") && !strstr(g_log, "no sw_api"));
    f_api.site = f_site;

    printf("negative set value acts as 1\n");
    reset(1);
    esa_set_mode(X, -3);
    CHECK(X->mode == 1 && f_atk_on == 1);

    printf("set 2 twice re-applies the tap (idempotent)\n");
    reset(1); esa_set_mode(X, 2);
    f_tap = NULL;
    esa_set_mode(X, 2);
    CHECK(X->mode == 2 && f_tap == (void *)esa_tap_fn);

    printf("esa_tick re-asserts the tap in mode 2 (exe reboot)\n");
    f_tap = NULL;
    esa_tick(X);
    CHECK(f_tap == (void *)esa_tap_fn);

    printf("stale: no puppet update for > ESA_STALE frames -> leave (tap)\n");
    reset(1); esa_set_mode(X, 2);
    f_now = 10000; tap(11, ACTOR, 5, 1, 1);
    f_now = 10000 + ESA_STALE + 1; tap(11, ACTOR, 5, 1, 1);
    CHECK(strstr(g_log, "left (no puppet update)") && count(14) == 1);

    printf("calc_ on a node that went away -> no vcall, forgot\n");
    reset(1); esa_set_mode(X, 2);
    f_now = 10100; tap(12, ACTOR, 5, 1, 1);
    mk_node(NODE_A, VT_A, 0xBAD, 5);
    int c31 = count(31);
    f_now = 10101; tap(12, ACTOR, 5, 1, 1);
    CHECK(count(31) == c31 && X->n.forgot >= 1 && count(14) == 0);

    printf("table full: oldest idle entry is evicted, a new hid gets in\n");
    reset(1); esa_set_mode(X, 2);
    for (uint32_t h = 0; h < ESX_PUP_MAX; h++) { f_now = 11000 + h; tap(100 + h, ACTOR, 0, 0, 1); }
    CHECK(X->pup.n == ESX_PUP_MAX);
    f_now = 11100; tap(100, ACTOR, 5, 1, 1);   // hid 100 runs an attack (atk_node != 0, oldest atk_seen)
    f_now = 11101; tap(999, ACTOR, 0, 0, 1);
    int has999 = 0, has100 = 0, has101 = 0;
    for (int i = 0; i < X->pup.n; i++) has999 |= X->pup.e[i].hid == 999, has100 |= X->pup.e[i].hid == 100, has101 |= X->pup.e[i].hid == 101;
    CHECK(X->pup.n == ESX_PUP_MAX && has999 && has100 && !has101);   // the running one stays, the oldest idle one goes

    printf("flag write refused -> no enter\n");
    reset(1); esa_set_mode(X, 2);
    f_api.write = f_write_fail;
    f_now = 12000; CHECK(tap(13, ACTOR, 5, 1, 1) == 0 && X->n.badwrite == 1 && count(12) == 0 && esa_running(X) == 0);
    f_api.write = f_write;

    printf("sweep: brain back on / body replaced / stale / hot reload leave with a NULL Cpu\n");
    f_api.esync_actor = f_esync_actor; f_api.in_frame = f_in_frame;
    reset(1); esa_set_mode(X, 2);
    f_now = 13000; tap(14, ACTOR, 5, 1, 1);
    f_es_found = 1, f_es_actor = ACTOR, f_es_pup = 1; f_now = 13001;
    esa_sweep(X);
    CHECK(esa_running(X) == 1);
    f_es_pup = 0; esa_sweep(X);
    CHECK(esa_running(X) == 0 && count(14) == 1 && strstr(g_log, "left (puppet gone / brain back)"));
    f_es_pup = 1; f_now = 13100; tap(14, ACTOR, 5, 2, 1);
    CHECK(esa_running(X) == 1);
    f_es_actor = ACTOR + 8; f_now = 13101; esa_sweep(X);
    CHECK(esa_running(X) == 0 && count(14) == 1);
    f_es_actor = ACTOR;
    reset(1); esa_set_mode(X, 2);
    f_now = 13200; tap(15, ACTOR, 5, 1, 1);
    f_es_found = 0; f_now = 13201; esa_sweep(X);
    CHECK(esa_running(X) == 1);   // not found = not drawn: wait
    f_now = 13200 + ESA_STALE + 1; esa_sweep(X);
    CHECK(esa_running(X) == 1);   // (s175 re-review: not found is never leave_'d; forgotten after 60 frames like the exe)
    f_now = 13200 + 61; esa_sweep(X);
    CHECK(esa_running(X) == 0 && count(14) == 0);
    f_es_found = 1;
    reset(1); esa_set_mode(X, 2);
    f_now = 13300; tap(16, ACTOR, 5, 1, 1);
    f_in_frame_v = 1; esa_unload(X);
    CHECK(esa_running(X) == 0 && count(14) == 1 && strstr(g_log, "left (dll unload)"));
    reset(1); esa_set_mode(X, 2);   // unload with the body gone: no leave_ on a node we cannot vouch for
    f_now = 13350; tap(18, ACTOR, 5, 1, 1);
    f_es_found = 0; f_in_frame_v = 1; esa_unload(X);
    CHECK(esa_running(X) == 0 && count(14) == 0);
    f_es_found = 1;
    reset(1); esa_set_mode(X, 2);
    f_now = 13400; tap(17, ACTOR, 5, 1, 1);
    f_in_frame_v = 0; esa_unload(X);
    CHECK(esa_running(X) == 0 && count(14) == 0);   // outside linkpos_frame: nothing is called
    f_in_frame_v = 1;

    printf("hot reload while the env says 1: the exe runner is switched on\n");
    reset(1);
    esa_commit_exe(X, 1);
    CHECK(f_atk_on == 1 && X->mode == 1);

    // ---- s181 mode 3: the AI-phase runner ----
    printf("mode 3 on an exe without the AI tap (taps_called 0): acts as 2\n");
    f_api.taps_called = 0;
    reset(1); esa_set_mode(X, 3);
    CHECK(X->mode == 2 && f_tap == (void *)esa_tap_fn && f_tap_ai == NULL && strstr(g_log, "set esyncatk 3 acts as 2"));
    f_api.taps_called = (1u << SW_TAP_PUPPET_ATTACK) | (1u << SW_TAP_PUPPET_AI);

    printf("mode 3: both taps set, exe runner off\n");
    reset(1); esa_set_mode(X, 3);
    CHECK(X->mode == 3 && f_tap == (void *)esa_tap_fn && f_tap_ai == (void *)esa_ai_tap_fn && f_atk_on == 0 && strstr(g_log, "IN THE AI PHASE"));

    printf("mode 3: the net tick makes no guest call; the AI tap walks, enters + calcs\n");
    f_now = 20000;
    CHECK(tap(30, ACTOR, 5, 1, 1) == 0);      // (no walk yet: nothing predicted)
    CHECK(f_nvc == 0 && f_walks == 0);
    esa_ai_tap(X, CPU, ACTOR);
    CHECK(f_walks == 1 && count(12) == 1 && count(31) == 1 && f_vc[0].node == NODE_A);
    CHECK(strstr(g_log, "(dll, ai phase)") != NULL);
    CHECK(esa_running(X) == 1);
    esa_ai_tap(X, CPU, ACTOR);              // same game frame: no second step
    CHECK(count(31) == 1);
    f_now = 20001;
    CHECK(tap(30, ACTOR, 5, 1, 1) == 1);      // running: the exe keeps slot 0
    CHECK(f_nvc == 2);
    esa_ai_tap(X, CPU, ACTOR);
    CHECK(count(31) == 2);

    printf("mode 3: an actor without a slot / a busy slot is skipped\n");
    esa_ai_tap(X, CPU, 0xBEEF0ull);
    CHECK(X->an.noslot == 1);
    EsaAiSlot *s30 = esa_ai_by_hid(X, 30);
    CHECK(s30 != NULL);
    f_now = 20002;
    s30->lk.x = 1;
    esa_ai_tap(X, CPU, ACTOR);
    CHECK(X->an.busy == 1 && count(31) == 2);
    s30->lk.x = 0;

    printf("mode 3: the owner's action changes -> the AI tap leaves\n");
    f_now = 20003;
    tap(30, ACTOR, 0, 2, 1);
    esa_ai_tap(X, CPU, ACTOR);
    CHECK(count(14) == 1 && esa_running(X) == 0);

    printf("mode 3: predicted enter -> the net tick answers 1 before the AI tap ran\n");
    f_now = 20100;
    CHECK(tap(30, ACTOR, 6, 3, 1) == 1 && X->an.predict == 1);   // (idx 6 = NODE_B, walked already)
    esa_ai_tap(X, CPU, ACTOR);
    CHECK(count(12) == 2 && esa_running(X) == 1);

    printf("mode 3: no net update for > 30 frames -> the AI tap leaves\n");
    f_now = 20100 + 31;
    esa_ai_tap(X, CPU, ACTOR);
    CHECK(count(14) == 2 && esa_running(X) == 0 && strstr(g_log, "left (no puppet update)"));

    printf("mode 3: sweep leaves when the brain is back on\n");
    f_now = 20300;
    tap(30, ACTOR, 5, 4, 1);
    esa_ai_tap(X, CPU, ACTOR);
    CHECK(esa_running(X) == 1);
    f_es_found = 1, f_es_pup = 0, f_es_actor = ACTOR, f_in_frame_v = 1;
    f_api.esync_actor = f_esync_actor, f_api.in_frame = f_in_frame;
    esa_sweep(X);
    CHECK(esa_running(X) == 0 && count(14) == 3 && X->an.sweep_left == 1);
    f_es_pup = 1;

    printf("mode 3 -> 1 while running: the AI tap drains, then commits\n");
    f_now = 20400;
    tap(30, ACTOR, 5, 5, 1);
    esa_ai_tap(X, CPU, ACTOR);
    CHECK(esa_running(X) == 1);
    esa_set_mode(X, 1);
    CHECK(X->mode == 3 && X->pending == 1);
    f_now = 20401;
    esa_ai_tap(X, CPU, ACTOR);
    CHECK(esa_running(X) == 0 && count(14) == 4);
    tap(30, ACTOR, 5, 5, 1);
    CHECK(X->mode == 1 && f_tap == NULL && f_tap_ai == NULL && f_atk_on == 1);

    printf("mode 3: body replaced -> forgotten, no leave_\n");
    reset(1); esa_set_mode(X, 3);
    f_now = 20500;
    tap(31, ACTOR, 5, 1, 1);
    esa_ai_tap(X, CPU, ACTOR);
    CHECK(esa_running(X) == 1);
    f_now = 20501;
    tap(31, 0xAC80ull, 5, 1, 1);
    CHECK(esa_running(X) == 0 && count(14) == 0 && X->an.replaced == 1);

    printf("mode 3: hot reload -> AI tap cleared first, running attack left with the exe Cpu\n");
    reset(1); esa_set_mode(X, 3);
    f_now = 20600;
    tap(32, ACTOR, 5, 1, 1);
    esa_ai_tap(X, CPU, ACTOR);
    f_in_frame_v = 1; esa_unload(X);
    CHECK(f_tap_ai == NULL && X->ai_dying && esa_running(X) == 0 && count(14) == 1);
    esa_ai_tap(X, CPU, ACTOR);              // (a late worker into the old copy: nothing)
    CHECK(f_nvc == 3);

    printf("mode 3 (review 1): a new hid at an old hid's address takes over: one slot per actor\n");
    reset(1); esa_set_mode(X, 3);
    f_now = 20700;
    tap(40, ACTOR, 5, 1, 1);
    esa_ai_tap(X, CPU, ACTOR);
    CHECK(esa_running(X) == 1);
    f_now = 20701;
    tap(41, ACTOR, 6, 1, 1);
    CHECK(esa_ai_by_hid(X, 40) == NULL && esa_ai_by_hid(X, 41) != NULL && esa_ai_by_actor(X, ACTOR) == esa_ai_by_hid(X, 41));
    CHECK(count(14) == 0 && esa_running(X) == 0);   // (old body's node forgotten, never leave_'d)
    esa_ai_tap(X, CPU, ACTOR);
    CHECK(esa_running(X) == 1 && f_vc[f_nvc - 2].slot == 12 && f_vc[f_nvc - 2].node == NODE_B);

    printf("mode 3 (review 1): an old slot busy -> the net tick waits a tick\n");
    reset(1); esa_set_mode(X, 3);
    f_now = 20800;
    tap(42, ACTOR, 0, 0, 1);
    EsaAiSlot *s42 = esa_ai_by_hid(X, 42);
    s42->lk.x = 1;
    f_now = 20801;
    CHECK(tap(43, ACTOR, 0, 0, 1) == 0 && esa_ai_by_hid(X, 43) == NULL && X->an.net_busy == 1);
    s42->lk.x = 0;
    CHECK(tap(43, ACTOR, 0, 0, 1) == 0 && esa_ai_by_hid(X, 43) != NULL && esa_ai_by_hid(X, 42) == NULL);

    printf("mode 3 (review 1): sweep frees a slot whose body is gone\n");
    f_es_found = 0;
    f_now = 20801 + 61; esa_sweep(X);
    CHECK(esa_ai_by_hid(X, 43) == NULL);
    f_es_found = 1;

    printf("mode 3 (review 2): leaving mode 3 while a worker holds a slot (inside enter_) waits for it\n");
    reset(1); esa_set_mode(X, 3);
    f_now = 20900;
    tap(44, ACTOR, 0, 0, 1);
    EsaAiSlot *s44 = esa_ai_by_hid(X, 44);
    s44->lk.x = 1;
    esa_set_mode(X, 1);
    CHECK(X->mode == 3 && X->pending == 1 && f_tap_ai == (void *)esa_ai_tap_fn);
    s44->lk.x = 0;
    esa_tick(X);
    CHECK(X->mode == 1 && X->pending == -1 && f_tap_ai == NULL && f_atk_on == 1);
    esa_ai_tap(X, CPU, ACTOR);               // (a late worker: mode 1 -> no enter)
    CHECK(count(12) == 0);

    printf("mode 3 -> 0 with nothing running and no worker: commits at once\n");
    reset(1); esa_set_mode(X, 3);
    esa_set_mode(X, 0);
    CHECK(X->mode == 0 && f_tap_ai == NULL);

    printf("mode 3: hot reload with env 3 re-applies both taps\n");
    reset(1);
    X->init = 0;
    esa_set_mode(X, 3);   // (stands in for esa_tick's env read: ESA_NO_EXE has no env)
    CHECK(X->mode == 3 && f_tap_ai == (void *)esa_ai_tap_fn);

    printf("%d checks, %d failed\n", runs, fails);
    return fails != 0;
}

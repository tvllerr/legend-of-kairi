// host test for runtime/sw_api_core.h (s175 idea 1: hook table, tap table, guest-call argument packing) - no linkpos, no guest memory.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/sw_api_test.c -o build_h/sw_api_test.exe
#include <stdio.h>
#include <string.h>
#include "sw_api_core.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static SwCore K;
static void fn_a(Cpu *c, int id, void *u) { (void)c; (void)id; (void)u; }
static void fn_b(Cpu *c, int id, void *u) { (void)c; (void)id; (void)u; }

static void test_hooks(void) {
    sw_core_reset(&K);
    int nw;
    int a = sw_core_hook_alloc(&K, 0x1000, 3, 0x7000, (void *)&K, "a", &nw);
    CHECK(a == 0 && nw == 1 && K.h[a].used && K.h[a].orig == 0x7000 && !strcmp(K.h[a].name, "a"));
    CHECK(sw_core_hook_alloc(&K, 0x1000, 3, 0x9999, NULL, "zz", &nw) == a && nw == 0 && K.h[a].orig == 0x7000);  // same vt+slot: same id, untouched
    int b = sw_core_hook_alloc(&K, 0x1000, 4, 0x7100, NULL, NULL, &nw);
    CHECK(b == 1 && nw == 1 && K.h[b].name[0] == 0);
    CHECK(sw_core_hook_alloc(&K, 0x2000, 3, 1, NULL, "c", &nw) == 2);
    CHECK(sw_core_hook_find(&K, 0x1000, 4) == b && sw_core_hook_find(&K, 0x1000, 5) == -1 && sw_core_hook_find(&K, 0x3000, 3) == -1);
    // bad args
    CHECK(sw_core_hook_alloc(&K, 0, 3, 1, NULL, "x", &nw) == -1);
    CHECK(sw_core_hook_alloc(&K, 0x5000, -1, 1, NULL, "x", &nw) == -1);
    CHECK(sw_core_hook_alloc(&K, 0x5000, SW_SLOT_MAX, 1, NULL, "x", &nw) == -1);
    CHECK(sw_core_hook_alloc(&K, 0x5000, SW_SLOT_MAX - 1, 1, NULL, "x", &nw) >= 0);
    // fn / user
    void *u = NULL;
    CHECK(sw_core_hook_fn(&K, a, &u) == NULL);
    sw_core_hook_set(&K, a, fn_a, &K);
    CHECK(sw_core_hook_fn(&K, a, &u) == fn_a && u == &K);
    sw_core_hook_set(&K, a, fn_b, NULL);
    CHECK(sw_core_hook_fn(&K, a, &u) == fn_b && u == NULL);
    CHECK(sw_core_hook_fn(&K, -1, &u) == NULL && sw_core_hook_fn(&K, SW_HOOKS, &u) == NULL);
    // long names are cut, still terminated
    int ln = sw_core_hook_alloc(&K, 0x6000, 0, 1, NULL, "0123456789012345678901234567890123456789", &nw);
    CHECK(ln >= 0 && strlen(K.h[ln].name) == sizeof K.h[ln].name - 1);
    // free + reuse (origfn survives a free, fn / user do not)
    sw_core_hook_set(&K, b, fn_a, &K);
    K.h[b].origfn = (void *)&K;
    sw_core_hook_free(&K, b);
    CHECK(!K.h[b].used && sw_core_hook_fn(&K, b, &u) == NULL && u == NULL && sw_core_hook_find(&K, 0x1000, 4) == -1 && K.h[b].origfn == (void *)&K);
    sw_core_hook_free(&K, b);   // twice: harmless
    sw_core_hook_free(&K, -5), sw_core_hook_free(&K, 99);
    int fresh = sw_core_hook_alloc(&K, 0x7000, 1, 2, NULL, "r", &nw);
    CHECK(fresh != b && fresh >= 0 && nw == 1 && K.h[fresh].ever);   // a never-used id is preferred over the freed one (a stub may still run on it)
    sw_core_hook_free(&K, fresh);
    // hooks_clear (reboot) keeps the taps
    sw_core_tap_set(&K, SW_TAP_PUPPET_ATTACK, (void *)fn_a);
    sw_core_hook_set(&K, ln, fn_a, &K);
    sw_core_hooks_clear(&K);
    CHECK(sw_core_hooks_used(&K) == 0 && sw_core_hook_fn(&K, ln, NULL) == NULL && sw_core_tap_get(&K, SW_TAP_PUPPET_ATTACK) == (void *)fn_a);
    sw_core_tap_set(&K, SW_TAP_PUPPET_ATTACK, NULL);
    // fill: all 32, then -1
    sw_core_reset(&K);
    CHECK(sw_core_hooks_used(&K) == 0);
    for (int i = 0; i < SW_HOOKS; i++) CHECK(sw_core_hook_alloc(&K, 0x1000 + 0x100 * (uint64_t)i, 7, 1, NULL, "f", &nw) == i);
    CHECK(sw_core_hooks_used(&K) == SW_HOOKS);
    CHECK(sw_core_hook_alloc(&K, 0xdead000, 7, 1, NULL, "full", &nw) == -1 && nw == 0);
    CHECK(sw_core_hook_alloc(&K, 0x1000, 7, 1, NULL, "again", &nw) == 0);   // an existing one still answers when full
    sw_core_hook_free(&K, 17);
    CHECK(sw_core_hook_alloc(&K, 0xdead000, 7, 1, NULL, "full", &nw) == 17);   // every id was used once: the freed one is reused
    // a failed unhook write-back keeps the entry in use with fn NULL: it is not handed out again, find still answers
    sw_core_hook_set(&K, 5, fn_a, &K);
    sw_core_hook_set(&K, 5, NULL, NULL);
    CHECK(K.h[5].used && sw_core_hook_fn(&K, 5, NULL) == NULL && sw_core_hook_find(&K, 0x1000 + 0x500, 7) == 5);
    sw_core_hook_free(&K, 5);
    CHECK(sw_core_hook_alloc(&K, 0xbeef000, 7, 1, NULL, "n", &nw) == 5);
    // reset forgets everything
    sw_core_hook_set(&K, 3, fn_a, &K);
    sw_core_reset(&K);
    CHECK(sw_core_hooks_used(&K) == 0 && sw_core_hook_fn(&K, 3, NULL) == NULL && sw_core_hook_find(&K, 0x1000, 7) == -1);
}

static void test_taps(void) {
    sw_core_reset(&K);
    CHECK(sw_core_tap_get(&K, SW_TAP_PUPPET_ATTACK) == NULL);
    CHECK(sw_core_tap_set(&K, SW_TAP_PUPPET_ATTACK, (void *)fn_a) && sw_core_tap_get(&K, SW_TAP_PUPPET_ATTACK) == (void *)fn_a);
    CHECK(sw_core_tap_set(&K, SW_TAPS - 1, (void *)fn_b) && sw_core_tap_get(&K, SW_TAPS - 1) == (void *)fn_b);
    CHECK(!sw_core_tap_set(&K, SW_TAPS, (void *)fn_a) && !sw_core_tap_set(&K, -1, (void *)fn_a) && !sw_core_tap_set(&K, 1000, NULL));
    CHECK(sw_core_tap_get(&K, SW_TAPS) == NULL && sw_core_tap_get(&K, -1) == NULL);
    CHECK(sw_core_tap_set(&K, SW_TAP_PUPPET_ATTACK, NULL) && sw_core_tap_get(&K, SW_TAP_PUPPET_ATTACK) == NULL);   // NULL = the exe's own path
    sw_core_taps_clear(&K);
    CHECK(sw_core_tap_get(&K, SW_TAPS - 1) == NULL);
}

static void test_pack(void) {
    SwArgs a;
    SwPack p;
    char buf[16], out[16];
    const uint64_t SP = 0x7fff00008;   // not 16-aligned on purpose
    memset(&a, 0, sizeof a);
    CHECK(sw_core_pack(&a, SP, &p) && p.rs == 0 && (p.base & 0xf) == 0 && (p.newsp & 0xf) == 0);
    CHECK(p.base <= SP - SW_REDZONE && p.newsp < p.base && p.newsp <= SP - SW_REDZONE - SW_CALLEE_GAP);   // red zone kept, callee sp below
    // rounding to 16
    a.scratch = 1;
    CHECK(sw_core_pack(&a, SP, &p) && p.rs == 16);
    a.scratch = 16;
    CHECK(sw_core_pack(&a, SP, &p) && p.rs == 16);
    a.scratch = 17;
    CHECK(sw_core_pack(&a, SP, &p) && p.rs == 32);
    a.scratch = SW_SCRATCH_MAX;
    CHECK(sw_core_pack(&a, SP, &p) && p.rs == SW_SCRATCH_MAX && p.base + p.rs <= SP - SW_REDZONE + 0xf && p.newsp + SW_CALLEE_GAP <= p.base);
    // bounds
    a.scratch = SW_SCRATCH_MAX + 1;
    CHECK(!sw_core_pack(&a, SP, &p));
    memset(&a, 0, sizeof a);
    a.scratch = 32, a.init_len = 33, a.init = buf;
    CHECK(!sw_core_pack(&a, SP, &p));
    a.init_len = 32;
    CHECK(sw_core_pack(&a, SP, &p));
    a.init = NULL;
    CHECK(!sw_core_pack(&a, SP, &p));                        // init_len without init
    a.init_len = 0, a.out_len = 33, a.out = out;
    CHECK(!sw_core_pack(&a, SP, &p));
    a.out_len = 32;
    CHECK(sw_core_pack(&a, SP, &p));
    a.out = NULL;
    CHECK(!sw_core_pack(&a, SP, &p));                        // out_len without out
    a.out_len = 0;
    a.scratch = 0, a.init_len = 1, a.init = buf;
    CHECK(!sw_core_pack(&a, SP, &p));                        // init_len > scratch (0)
    memset(&a, 0, sizeof a);
    a.nx = 9;
    CHECK(!sw_core_pack(&a, SP, &p));
    a.nx = 8, a.nv = 5;
    CHECK(!sw_core_pack(&a, SP, &p));
    a.nv = 4;
    CHECK(sw_core_pack(&a, SP, &p));
    CHECK(!sw_core_pack(NULL, SP, &p) && !sw_core_pack(&a, SP, NULL));
    // a sp that is not a real stack pointer
    CHECK(!sw_core_pack(&a, 0, &p) && !sw_core_pack(&a, 0x100, &p));
    // x_scratch: bit i makes x[i] an address inside the scratch (only for i < nx)
    memset(&a, 0, sizeof a);
    a.scratch = 0x40, a.nx = 3, a.x[0] = 0x1234, a.x[1] = 0x10, a.x[2] = 0x40, a.x_scratch = 0x6;
    CHECK(sw_core_pack(&a, SP, &p));
    CHECK(p.x[0] == 0x1234 && p.x[1] == p.base + 0x10 && p.x[2] == p.base + 0x40);
    CHECK(p.x[1] >= p.base && p.x[1] < p.base + p.rs && p.x[2] == p.base + p.rs);
    a.x[2] = 0x41;
    CHECK(!sw_core_pack(&a, SP, &p));                        // offset past the scratch
    a.x[2] = 0x40, a.x_scratch = 0x8, a.x[3] = 0xffff;       // bit 3 but nx = 3: ignored, the value is kept
    CHECK(sw_core_pack(&a, SP, &p) && p.x[3] == 0xffff);
    memset(&a, 0, sizeof a);
    a.nx = 1, a.x_scratch = 1;                               // scratch 0: offset 0 = the (empty) base
    CHECK(sw_core_pack(&a, SP, &p) && p.x[0] == p.base);
    // different sps never overlap the red zone
    for (uint64_t sp = 0x10000; sp < 0x10000 + 64; sp++) {
        memset(&a, 0, sizeof a);
        a.scratch = 100;
        CHECK(sw_core_pack(&a, sp, &p) && (p.base & 0xf) == 0 && p.base + p.rs <= sp - SW_REDZONE + 15 && (p.newsp & 0xf) == 0);
        CHECK(p.base + p.rs <= sp && p.newsp < p.base);
    }
}

int main(void) {
    test_hooks();
    test_taps();
    test_pack();
    printf("sw_api_test: %d checks, %d failed\n", runs, fails);
    return fails ? 1 : 0;
}

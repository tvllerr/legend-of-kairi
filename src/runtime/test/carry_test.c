// host tests for runtime/sora_carry.h's pure part (s217 world, feature 2: carry mode): the latch, every exit, the block after a
// forced exit, the safety timeout. Build + run from the repo root (UCRT64 clang):
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/carry_test.c -o build_h/carry_test.exe && build_h/carry_test.exe
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define CARRY_HOST  // (the pure part only)
#include "sora_carry.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static CarryIn in0(uint64_t present, bool grab) {  // carry 1, standing, nothing special
    CarryIn in = {.mode = 1, .grab = grab, .ground = true, .form = 0, .present = present, .max = 7200};
    return in;
}
// n frames of `grab`, from *t on; returns the last event that was not CR_NONE (CR_NONE if none)
static int run(CarrySm *m, uint64_t *t, int n, bool grab) {
    int last = CR_NONE;
    for (int i = 0; i < n; i++) {
        CarryIn in = in0((*t)++, grab);
        int ev = carry_step(m, &in);
        if (ev != CR_NONE) last = ev;
    }
    return last;
}
static void latch(CarrySm *m, uint64_t *t) {  // into carry mode
    memset(m, 0, sizeof *m);
    run(m, t, CARRY_IN_N, true);
}
// one forced-exit case: on, then a frame with `mod` applied while Link still grabs -> leave with `why`, blocked
typedef void (*Mod)(CarryIn *);
static void m_off(CarryIn *in) { in->mode = 0; }
static void m_mode(CarryIn *in) { in->mode = 2; }
static void m_load(CarryIn *in) { in->loading = true; }
static void m_dead(CarryIn *in) { in->dead = true; }
static void m_swim(CarryIn *in) { in->swim = true; }
static void m_mickey(CarryIn *in) { in->mickey = true; }
static void m_form(CarryIn *in) { in->form = 3; }
static void forced(Mod mod, int why) {
    CarrySm m;
    uint64_t t = 100;
    latch(&m, &t);
    CHECK(m.on);
    CarryIn in = in0(t++, true);
    mod(&in);
    CHECK(carry_step(&m, &in) == CR_LEAVE);
    CHECK(!m.on && m.why == why && m.block);
    // still grabbing, all normal again: blocked, no re-latch
    CHECK(run(&m, &t, 30, true) == CR_NONE && !m.on && m.block);
    // the grab ends: the block goes; the next grab latches again
    CHECK(run(&m, &t, 1, false) == CR_NONE && !m.block);
    CHECK(run(&m, &t, CARRY_IN_N, true) == CR_ENTER && m.on);
}


// ---- the AI chain walk on a fake tree in host memory (s217 follow-up: the live walk stopped at PlayerNormal, 89 children)
typedef struct { uint8_t b[0x40]; } FakeNode;
static bool host_ok(uint64_t a, size_t n) { (void)n; return a >= 0x1000; }
static void fn_set(FakeNode *f, uint64_t vt, uint64_t actor, uint16_t prog_idx, uint16_t cur, uint32_t cnt, uint64_t *kids) {
    memset(f, 0, sizeof *f);
    uint64_t k = (uint64_t)(uintptr_t)kids;
    memcpy(f->b, &vt, 8), memcpy(f->b + 8, &actor, 8), memcpy(f->b + 0x18, &prog_idx, 2), memcpy(f->b + 0x1c, &cur, 2);
    memcpy(f->b + 0x28, &cnt, 4), memcpy(f->b + 0x30, &k, 8);
}
static void walk_tests(void) {
    static uint8_t actor_mem[0x600];
    static FakeNode root, proot, pnormal, grab, wait, idle, kids_dummy[600];
    static uint64_t k_root[1], k_proot[2], k_normal[600], k_grab[9];
    uint64_t A = (uint64_t)(uintptr_t)actor_mem, R = (uint64_t)(uintptr_t)&root;
    memcpy(actor_mem + 0x558, &R, 8);
    for (int i = 0; i < 600; i++) fn_set(&kids_dummy[i], 0x7001000000ull + i, A, 0, 0xffff, 0, NULL), k_normal[i] = (uint64_t)(uintptr_t)&kids_dummy[i];
    fn_set(&root, 0x7001d1dec0ull, A, 0, 0, 1, k_root), k_root[0] = (uint64_t)(uintptr_t)&proot;
    fn_set(&proot, 0x7001cd62e8ull, A, 2, 1, 2, k_proot), k_proot[0] = (uint64_t)(uintptr_t)&kids_dummy[0], k_proot[1] = (uint64_t)(uintptr_t)&pnormal;
    fn_set(&pnormal, 0x7001cd60a0ull, A, 1, 15, 89, k_normal), k_normal[15] = (uint64_t)(uintptr_t)&grab;
    fn_set(&grab, CARRY_VT_GRAB, A, 15, 2, 9, k_grab);
    for (int i = 0; i < 9; i++) k_grab[i] = (uint64_t)(uintptr_t)&kids_dummy[100 + i];
    fn_set(&wait, 0x7001ccd618ull, A, 118, 0xffff, 0, NULL), k_grab[2] = (uint64_t)(uintptr_t)&wait;
    fn_set(&idle, 0x7001aaaaa0ull, A, 116, 0xffff, 0, NULL);
    CarryWalk w;
    // lifting: root -> PlayerRoot -> PlayerNormal (89 children, the running one = PlayerGrab) -> GrabWait
    carry_walk(A, &w, host_ok);
    CHECK(w.res == 1 && w.n == 4 && w.grab_at == 3 && w.vt[2] == 0x7001cd60a0ull && w.leaf_vt == 0x7001ccd618ull && w.leaf_idx == 118);
    CHECK(89 > 64 && CARRY_KIDS_MAX >= 89);  // (the old cap of 64 refused exactly this child array)
    // idle: PlayerNormal runs another child (an Action: no child list) -> ok, no PlayerGrab, the action is the 4th node
    k_normal[15] = (uint64_t)(uintptr_t)&idle;
    carry_walk(A, &w, host_ok);
    CHECK(w.res == 0 && w.n == 4 && w.vt[3] == 0x7001aaaaa0ull);
    k_normal[15] = (uint64_t)(uintptr_t)&grab;
    // a child count over the cap / an index past the count / a node that is not Link's: the walk stops there (no PlayerGrab)
    memcpy(pnormal.b + 0x28, &(uint32_t){CARRY_KIDS_MAX + 1}, 4);
    carry_walk(A, &w, host_ok);
    CHECK(w.res == 0 && w.n == 3);
    memcpy(pnormal.b + 0x28, &(uint32_t){89}, 4), memcpy(pnormal.b + 0x1c, &(uint16_t){89}, 2);
    carry_walk(A, &w, host_ok);
    CHECK(w.res == 0 && w.n == 3);
    memcpy(pnormal.b + 0x1c, &(uint16_t){15}, 2);
    uint64_t other = A + 8;
    memcpy(grab.b + 8, &other, 8);
    carry_walk(A, &w, host_ok);
    CHECK(w.res == 0 && w.n == 3);
    memcpy(grab.b + 8, &A, 8);
    carry_walk(A, &w, host_ok);
    CHECK(w.res == 1);
    // the grab's running child not Link's: still found, leaf unknown
    memcpy(wait.b + 8, &other, 8);
    carry_walk(A, &w, host_ok);
    CHECK(w.res == 1 && w.leaf_vt == 0);
    // no actor / a RootAi that is not Link's
    carry_walk(0, &w, host_ok);
    CHECK(w.res == -1);
    memcpy(root.b + 8, &other, 8);
    carry_walk(A, &w, host_ok);
    CHECK(w.res == -2 && w.root == R && w.root_actor == other);
}

// ---- (s217 follow-up 2) the hold: PlayerGrab ends after the lift, Link +0x88 keeps the object until it leaves his hands
static int run2(CarrySm *m, uint64_t *t, int n, bool grab, bool hold, bool ground) {
    int last = CR_NONE;
    for (int i = 0; i < n; i++) {
        CarryIn in = in0((*t)++, grab);
        in.hold = hold, in.ground = ground;
        int ev = carry_step(m, &in);
        if (ev != CR_NONE) last = ev;
    }
    return last;
}
static void hold_forced(Mod mod, int why) {  // a forced exit while he only HOLDS (no PlayerGrab node): leave, blocked while he holds
    CarrySm m;
    uint64_t t = 100;
    latch(&m, &t);
    CHECK(run2(&m, &t, 50, false, true, true) == CR_NONE && m.on);
    CarryIn in = in0(t++, false);
    in.hold = true;
    mod(&in);
    CHECK(carry_step(&m, &in) == CR_LEAVE && m.why == why && m.block);
    CHECK(run2(&m, &t, 40, false, true, true) == CR_NONE && !m.on && m.block);   // still holding: no re-latch
    CHECK(run2(&m, &t, 3, true, true, true) == CR_NONE && !m.on && m.block);     // even a put-down's PlayerGrab while blocked
    CHECK(run2(&m, &t, 1, false, false, true) == CR_NONE && !m.block);           // out of his hands: the block goes
    CHECK(run2(&m, &t, CARRY_IN_N, true, false, true) == CR_ENTER);              // the next lift latches again
}
static void hold_tests(void) {
    CarrySm m;
    uint64_t t;
    // lift -> hold for a long walk -> stays on (the s217 live failure: off 66 presents after the lift)
    t = 0, latch(&m, &t);
    CHECK(run2(&m, &t, 3, true, true, true) == CR_NONE && m.on);       // GrabUp attaches it
    CHECK(run2(&m, &t, 3000, false, true, true) == CR_NONE && m.on);   // PlayerGrab gone, PlayerWait / PlayerMove: still on
    CHECK(run2(&m, &t, 300, false, true, false) == CR_NONE && m.on);   // a jump / fall while holding: still on
    // put down: PlayerGrab again (GrabPut, still holding), then GrabStand (released), then nothing: off CARRY_OFF_N later
    CHECK(run2(&m, &t, 20, true, true, true) == CR_NONE && m.on);
    CHECK(run2(&m, &t, 15, true, false, true) == CR_NONE && m.on);
    CHECK(run2(&m, &t, CARRY_OFF_N - 1, false, false, true) == CR_NONE && m.on);
    CHECK(run2(&m, &t, 1, false, false, true) == CR_LEAVE && m.why == CRW_ENDED && !m.block);
    // throw: the object leaves at the throw while GrabThrow still runs
    t = 0, latch(&m, &t);
    run2(&m, &t, 500, false, true, true);
    CHECK(run2(&m, &t, 10, true, true, true) == CR_NONE);
    CHECK(run2(&m, &t, 20, true, false, true) == CR_NONE && m.on);
    CHECK(run2(&m, &t, CARRY_OFF_N, false, false, true) == CR_LEAVE && m.why == CRW_ENDED);
    // it explodes / breaks in his hands, or a hit makes him drop it (no PlayerGrab at all): the hold just ends
    t = 0, latch(&m, &t);
    run2(&m, &t, 200, false, true, true);
    CHECK(run2(&m, &t, CARRY_OFF_N - 1, false, false, true) == CR_NONE && m.on);
    CHECK(run2(&m, &t, 1, false, false, true) == CR_LEAVE && m.why == CRW_ENDED);
    // ... while knocked into the air: CARRY_OFF_AIR frames
    t = 0, latch(&m, &t);
    run2(&m, &t, 200, false, true, true);
    CHECK(run2(&m, &t, CARRY_OFF_AIR - 1, false, false, false) == CR_NONE && m.on);
    CHECK(run2(&m, &t, 1, false, false, false) == CR_LEAVE);
    // a lift whose object never attached (hold never true): ends as before, CARRY_OFF_N after the grab
    t = 0, latch(&m, &t);
    CHECK(run2(&m, &t, CARRY_OFF_N, false, false, true) == CR_LEAVE);
    // holding alone never STARTS carry mode (only a PlayerGrab episode does)
    memset(&m, 0, sizeof m), t = 0;
    CHECK(run2(&m, &t, 100, false, true, true) == CR_NONE && !m.on);
    // a short hold dropout (one frame) does not end it
    t = 0, latch(&m, &t);
    run2(&m, &t, 50, false, true, true);
    run2(&m, &t, 1, false, false, true);
    CHECK(run2(&m, &t, 100, false, true, true) == CR_NONE && m.on);
    // forced exits while only holding
    hold_forced(m_off, CRW_OFF);
    hold_forced(m_mode, CRW_MODE);
    hold_forced(m_load, CRW_LOAD);
    hold_forced(m_dead, CRW_DEATH);
    hold_forced(m_swim, CRW_SWIM);
    hold_forced(m_mickey, CRW_MICKEY);
    hold_forced(m_form, CRW_FORM);
    // the safety timeout during a hold
    t = 0, latch(&m, &t);
    {
        uint64_t t0 = m.t0;
        CHECK(run2(&m, &t, (int)(t0 + 7200 - t), false, true, true) == CR_NONE && m.on);
        CHECK(run2(&m, &t, 1, false, true, true) == CR_LEAVE && m.why == CRW_TIMEOUT && m.block);
        CHECK(run2(&m, &t, 100, false, true, true) == CR_NONE && !m.on);
    }
}

int main(void) {
    walk_tests();
    hold_tests();
    CarrySm m;
    uint64_t t;

    // ---- the latch: CARRY_IN_N frames in PlayerGrab
    memset(&m, 0, sizeof m), t = 1000;
    CHECK(run(&m, &t, CARRY_IN_N - 1, true) == CR_NONE && !m.on);
    CHECK(run(&m, &t, 1, true) == CR_ENTER && m.on && m.t0 == t - 1 && m.mode0 == 1);
    CHECK(run(&m, &t, 100, true) == CR_NONE && m.on);  // carrying: stays on
    // a one-frame dropout does not latch in again or reset t0 (no flicker)
    memset(&m, 0, sizeof m), t = 1000;
    run(&m, &t, 1, true), run(&m, &t, 1, false);
    CHECK(run(&m, &t, 1, true) == CR_NONE && !m.on);   // the in-run restarted
    CHECK(run(&m, &t, 1, true) == CR_ENTER);

    // ---- no latch in these
    memset(&m, 0, sizeof m), t = 0;
    for (int k = 0; k < 6; k++) {
        memset(&m, 0, sizeof m);
        int ev = CR_NONE;
        for (int i = 0; i < 10; i++) {
            CarryIn in = in0(t++, true);
            if (k == 0) in.mode = 0;
            if (k == 1) in.loading = true;
            if (k == 2) in.dead = true;
            if (k == 3) in.swim = true;
            if (k == 4) in.mickey = true;
            if (k == 5) m.block = true;  // (kept blocked while grabbing)
            int e = carry_step(&m, &in);
            if (e != CR_NONE) ev = e;
        }
        CHECK(ev == CR_NONE && !m.on);
    }

    // ---- natural end (thrown / put down / dropped): CARRY_OFF_N frames out + standing
    t = 50, latch(&m, &t);
    CHECK(run(&m, &t, CARRY_OFF_N - 1, false) == CR_NONE && m.on);
    CHECK(run(&m, &t, 1, false) == CR_LEAVE && !m.on && m.why == CRW_ENDED && !m.block);
    CHECK(run(&m, &t, 1, false) == CR_NONE);  // only once
    // the grab came back inside the window (GrabStand -> a new lift): the count restarts
    t = 50, latch(&m, &t);
    run(&m, &t, CARRY_OFF_N - 2, false);
    run(&m, &t, 1, true);
    CHECK(run(&m, &t, CARRY_OFF_N - 1, false) == CR_NONE && m.on);
    CHECK(run(&m, &t, 1, false) == CR_LEAVE);
    // not standing (dropped into the air / a ledge): CARRY_OFF_AIR frames out
    t = 50, latch(&m, &t);
    {
        int ev = CR_NONE, at = -1;
        for (int i = 0; i < CARRY_OFF_AIR + 5; i++) {
            CarryIn in = in0(t++, false);
            in.ground = false;
            int e = carry_step(&m, &in);
            if (e != CR_NONE && at < 0) ev = e, at = i + 1;
        }
        CHECK(ev == CR_LEAVE && at == CARRY_OFF_AIR && m.why == CRW_ENDED && !m.block);
    }
    // ... landing ends it at once once CARRY_OFF_N frames are out
    t = 50, latch(&m, &t);
    for (int i = 0; i < CARRY_OFF_N + 3; i++) { CarryIn in = in0(t++, false); in.ground = false; CHECK(carry_step(&m, &in) == CR_NONE); }
    { CarryIn in = in0(t++, false); CHECK(carry_step(&m, &in) == CR_LEAVE && m.why == CRW_ENDED); }

    // ---- forced exits while Link still grabs: leave + block until the grab ends
    forced(m_off, CRW_OFF);
    forced(m_mode, CRW_MODE);
    forced(m_load, CRW_LOAD);
    forced(m_dead, CRW_DEATH);
    forced(m_swim, CRW_SWIM);
    forced(m_mickey, CRW_MICKEY);
    forced(m_form, CRW_FORM);
    // a forced exit when the grab is ALREADY gone: no block
    t = 10, latch(&m, &t);
    { CarryIn in = in0(t++, false); in.loading = true; CHECK(carry_step(&m, &in) == CR_LEAVE && m.why == CRW_LOAD && !m.block); }
    // the priority of the reasons (several at once): off > mode > load > death > swim > Mickey > form > timeout > grab over
    t = 10, latch(&m, &t);
    { CarryIn in = in0(t++, true); in.loading = in.dead = true; CHECK(carry_step(&m, &in) == CR_LEAVE && m.why == CRW_LOAD); }
    t = 10, latch(&m, &t);
    { CarryIn in = in0(t++, true); in.dead = in.swim = true, in.form = 7; CHECK(carry_step(&m, &in) == CR_LEAVE && m.why == CRW_DEATH); }

    // ---- the safety timeout (a walk that keeps saying PlayerGrab: Sora gets the frame back, blocked)
    t = 5000, latch(&m, &t);
    {
        uint64_t t0 = m.t0;
        int ev = CR_NONE;
        while (t < t0 + 7200) { CarryIn in = in0(t++, true); ev = carry_step(&m, &in); if (ev != CR_NONE) break; }
        CHECK(ev == CR_NONE && m.on);
        CarryIn in = in0(t++, true);
        CHECK(carry_step(&m, &in) == CR_LEAVE && m.why == CRW_TIMEOUT && m.block);
        CHECK(run(&m, &t, 500, true) == CR_NONE && !m.on);  // never re-latches while that grab lasts
    }
    // a small carrymax, and a present counter that went backwards (reboot) = timeout at once
    t = 100, latch(&m, &t);
    { CarryIn in = in0(t, true); in.max = 2; CHECK(carry_step(&m, &in) == CR_NONE); }
    { CarryIn in = in0(t + 1, true); in.max = 2; CHECK(carry_step(&m, &in) == CR_LEAVE && m.why == CRW_TIMEOUT); }
    t = 100, latch(&m, &t);
    { CarryIn in = in0(5, true); CHECK(carry_step(&m, &in) == CR_LEAVE && m.why == CRW_TIMEOUT); }
    t = 100, latch(&m, &t);
    { CarryIn in = in0(t + 1, true); in.max = 0; CHECK(carry_step(&m, &in) == CR_LEAVE && m.why == CRW_TIMEOUT); }  // (max 0 -> 1)

    // ---- carry 2 (log only) runs the same machine
    memset(&m, 0, sizeof m), t = 0;
    { int ev = CR_NONE; for (int i = 0; i < CARRY_IN_N; i++) { CarryIn in = in0(t++, true); in.mode = 2; ev = carry_step(&m, &in); } CHECK(ev == CR_ENTER && m.mode0 == 2); }
    { CarryIn in = in0(t++, true); in.mode = 2; CHECK(carry_step(&m, &in) == CR_NONE && m.on); }

    // ---- names line up with the reasons; the default is off
    CHECK(sizeof CRW_NAME / sizeof *CRW_NAME == CRW_TIMEOUT + 1 && !strcmp(CRW_NAME[CRW_ENDED], "grab + hold over") && !strcmp(CRW_NAME[CRW_TIMEOUT], "timeout"));
    CHECK(CARRY_DEFAULT == 1 && CARRY_IN_N >= 1 && CARRY_OFF_N < CARRY_OFF_AIR);

    printf("carry_test: %d/%d passed\n", runs - fails, runs);
    return fails != 0;
}

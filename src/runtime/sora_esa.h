// s175 "idea 1": the DLL's R2 attack runner (a puppet runs its OWN copy of the owner's attack action node, so the club / body hit
// happens locally). Same logic as esx_atk_walk / esx_atk_frame / esx_atk_end in linkpos_esync_block.inc, built on the exe's
// primitives table (runtime/sw_api.h) so a change is a hot reload, not a relink. `set esyncatk 0|1|2`:
//   0 both runners off, 1 the exe's runner (s174, tap NULL), 2 this one (tap SW_TAP_PUPPET_ATTACK = esa_tap_fn, exe runner off),
//   3 (s181) this one with the guest calls in the game's AI phase (+ tap SW_TAP_PUPPET_AI, sora_esa_ai.h; an exe before s181: 3 acts as 2).
// An exe without sw_api (built before s175): 2 acts as 1. The step logic (EsxPupEnt, esx_atk_step) is shared with the exe
// (linkpos_esync.h, host-tested). Host test: runtime/test/esa_test.c (fake SwApi). Plan: docs/specs/2026-11-07-idea1-exe-primitives-plan.md
// Define ESA_NO_EXE (tests) to skip GetProcAddress / the process env; the test sets EsaCtx.api / .atkfn itself.
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sw_api.h"
#include "sora_net_proto.h"  // NetEsEnt (= EsState)
#include "linkpos_esync.h"   // EsxPupEnt, esx_pup_*, esx_atk_step
#ifndef ESA_NO_EXE
#include "sora_sites.h"      // sora_sites(): the fallback for api->site (the exe's own export)
#endif

#ifndef ESA_CAP
#define ESA_CAP 180          // frames (3 s) one local attack may run (= the exe's ESX_ATK_CAP)
#endif
#ifndef ESA_GAP
#define ESA_GAP 20           // frames before the same index may run again here (= ESX_ATK_GAP, GUESS)
#endif
#define ESA_STALE 30         // frames without a puppet update before a running attack is left (= the exe's ESX_ATK_STALE)
#define ESA_VT_MAX 12
#define ESA_WALK_MAX 4096    // the exe's walk visits <= 4000 nodes
#define ESA_DRAIN_FRAMES 120 // leaving mode 2 with attacks running: wait this long for the tap to leave them, then drop them
#define ESA_ENV "SAVAGEWIND_LINK_ESYNC_ATK"
#define ESA_ACTOR_OFF 0x8    // ACTION_ACTOR
#define ESA_FLAGS_OFF 0x1b   // node byte: bit 0 finished (clear 0-1 before enter_)

// s181 mode 3 (the AI-phase runner, sora_esa_ai.h): per-actor locks. The test build (ESA_NO_EXE, one thread) gets a fake lock it can
// set "busy" by hand.
#ifndef ESA_NO_EXE
#include <psapi.h>  // (K32GetProcessMemoryInfo lives in kernel32: no new lib)
typedef SRWLOCK EsaLk;  // (zero = SRWLOCK_INIT)
#define esa_lk_tryx(l) (TryAcquireSRWLockExclusive(l) != 0)
#define esa_lk_xu(l) ReleaseSRWLockExclusive(l)
#define esa_lk_trys(l) (TryAcquireSRWLockShared(l) != 0)
#define esa_lk_su(l) ReleaseSRWLockShared(l)
#define esa_sleep_ms(n) Sleep(n)
static double esa_qms(void) { LARGE_INTEGER q, f; QueryPerformanceCounter(&q), QueryPerformanceFrequency(&f); return (double)q.QuadPart * 1000.0 / (double)f.QuadPart; }
// s182 diag: wall vs thread CPU vs page faults around one timed call (cycles = QueryThreadCycleTime, k+u = GetThreadTimes in 100 ns, pf = process-wide)
typedef struct { double ms; uint64_t cyc, ku100ns, pf; } EsaRes;
static uint64_t esa_ft(FILETIME f) { return ((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime; }
static EsaRes esa_res(void) {
    EsaRes r; memset(&r, 0, sizeof r);
    ULONG64 cy = 0; FILETIME cr, ex, kt, ut; PROCESS_MEMORY_COUNTERS pmc;
    r.ms = esa_qms();
    if (QueryThreadCycleTime(GetCurrentThread(), &cy)) r.cyc = (uint64_t)cy;
    if (GetThreadTimes(GetCurrentThread(), &cr, &ex, &kt, &ut)) r.ku100ns = esa_ft(kt) + esa_ft(ut);
    pmc.cb = sizeof pmc;
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) r.pf = pmc.PageFaultCount;
    return r;
}
// cycles per ms, measured once with QPC over a ~10 ms busy loop on the calling thread
static double esa_cyc_per_ms(void) {
    static double v = 0; static volatile LONG busy = 0;
    if (v > 0) return v;
    if (InterlockedExchange(&busy, 1)) return 0;
    ULONG64 c0 = 0, c1 = 0; double t0 = esa_qms();
    QueryThreadCycleTime(GetCurrentThread(), &c0);
    while (esa_qms() - t0 < 10) {}
    QueryThreadCycleTime(GetCurrentThread(), &c1);
    double ms = esa_qms() - t0;
    v = ms > 0 ? (double)(c1 - c0) / ms : 0;
    return v;
}
#else
typedef struct { int x, s; } EsaLk;
static inline bool esa_lk_tryx(EsaLk *l) { if (l->x || l->s) return false; l->x = 1; return true; }
static inline void esa_lk_xu(EsaLk *l) { l->x = 0; }
static inline bool esa_lk_trys(EsaLk *l) { if (l->x) return false; l->s++; return true; }
static inline void esa_lk_su(EsaLk *l) { l->s--; }
#define esa_sleep_ms(n) ((void)0)
static double esa_qms(void) { return 0; }
typedef struct { double ms; uint64_t cyc, ku100ns, pf; } EsaRes;
static EsaRes esa_res(void) { EsaRes r; memset(&r, 0, sizeof r); return r; }
static double esa_cyc_per_ms(void) { return 0; }
#endif
#include "sora_esa_samp.h"  // (s182 sampling profiler of slow calc_ / enter_ calls; esa_unload stops its thread)
static inline void esa_add(uint64_t *v) { __atomic_fetch_add(v, 1, __ATOMIC_RELAXED); }  // (s181: counters workers bump too)
static inline void esa_add_u32(uint32_t *v) { __atomic_fetch_add(v, 1, __ATOMIC_RELAXED); }
#define ESA_AI_MAX 64
typedef struct {
    EsaLk lk;                    // the AI tap holds it exclusively for its guest calls; the net tick only try-shares it
    volatile uint64_t actor;     // 0 = free
    volatile uint32_t hid;
    volatile uint64_t tgt;       // the owner's latest wish (net tick): action | anim_chg[0] << 16 | alive << 24
    volatile uint64_t tgt_frame; // game frame of that net tick
    volatile int running;        // p.atk_node != 0 (the net tick returns it)
    volatile uint64_t last_step; // game frame of the last AI-tap step (one step per game frame)
    EsxPupEnt p;                 // belongs to whoever holds lk exclusively
} EsaAiSlot;

typedef struct {
    const SwApi *api;
    int (*atkfn)(int, const uint64_t *, int);  // the exe's linkpos_esync_atk (s174), NULL = exe older than s174
    int bound, mode, want, pending, init, log;
    uint64_t drain_since;
    uint64_t vt[ESA_VT_MAX];
    int nvt;
    EsxPup pup;
    uint64_t next_log;
    // s178 timing diag (logged every 10 s while mode 2): DLL frame gaps (esa_tick = once per sora_mod_frame) and tap gaps per running attack
    struct { uint64_t last_tick, last_tap_hid, gap_log; double tick_last, tick_max, tick_sum; uint32_t ticks, tick_over100;
             double tap_max, tap_sum; uint32_t taps, tap_over100; uint32_t hid[16]; double hid_last[16];
             // s179 diag: per running attack, taps counted in GAME frames (frame_id) + a gap histogram (1, 2, 3, 4, 5, 6+ frames)
             uint32_t af_hid[16], af_n[16]; uint64_t af_first[16], af_last[16]; uint32_t gap_hist[6]; } tm;
    struct { uint64_t enter, leave, fin, cap, swap, nonode, badwrite, walk, walk_nodes, bad_fn, forgot; } n;
    // s180 R2 throttle (live, default OFF = calc_ every tap as before): `set esacalc N` = calc_ only every N game frames of a running
    // attack (the owner's own swing got ~27 calcs/s in s177); `set esaskip MS` = after one calc_ took > MS ms, skip the next 2 taps
    int calc_every, skip_ms;
    uint32_t sk_hid[16]; int sk_left[16];
    struct { uint32_t calls, skipped, over5, over30; double sum, max, mcyc, ku_ms; uint64_t pf; } cs;  // calc_ timing per 10 s window
    SwAiNode walk[ESA_WALK_MAX];
    // s181 mode 3: slots by actor (never moved), the AI tap's own counters (atomic adds; workers), alloc lock, unload flag
    EsaAiSlot ai[ESA_AI_MAX];
    EsaLk ai_alloc, cs_lk;
    volatile int ai_dying;
    struct { uint64_t taps, steps, busy, noslot, net_busy, full, replaced, walk_oom, sweep_left, sweep_forgot, predict; } an;
} EsaCtx;
static EsaCtx g_esa = {.mode = -1, .pending = -1, .want = -1};

static bool esa_vt_is(const EsaCtx *x, uint64_t vt) {
    if (!vt) return false;
    for (int i = 0; i < x->nvt; i++) if (x->vt[i] == vt) return true;
    return false;
}
static void esa_defaults(EsaCtx *x) {
    static const char *const names[] = {"atkact_attack_vt", "atkact_stepdouble_vt", "atkact_large_vt", "atkact_jump_vt", "atkact_forkweapon_vt", "atkact_animtiming_vt"};
    x->nvt = 0;
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        uint64_t v = SW_HAS(x->api, site) ? x->api->site(names[i]) : 0;
#ifndef ESA_NO_EXE
        if (!v && sora_sites()) {  // (api->site found none: the DLL reads the exe's SoraSites row by field name itself)
#define ESA_SS(n, a, b) if (!strcmp(names[i], #n)) v = sora_sites()->n;
            SORA_SITES(ESA_SS)
#undef ESA_SS
        }
#endif
        if (v && x->nvt < ESA_VT_MAX) x->vt[x->nvt++] = v;
    }
}
static int esa_running(const EsaCtx *x) {
    int k = 0;
    for (int i = 0; i < x->pup.n; i++) k += x->pup.e[i].atk_node != 0;
    for (int i = 0; i < ESA_AI_MAX; i++) k += x->ai[i].actor && x->ai[i].running;  // (s181 mode 3 slots)
    return k;
}
static bool esa_u64(const SwApi *api, uint64_t a, uint64_t *out) { return api->read(a, out, 8); }
static bool esa_node_ok(const SwApi *api, uint64_t node, uint64_t actor) {
    uint64_t ac;
    return node && api->readable(node, 0x40) >= 0x40 && esa_u64(api, node + ESA_ACTOR_OFF, &ac) && ac == actor;
}
// one virtual of an action node: 12 enter_ (x1 = a zeroed InlineParamPack), 31 calc_ (the exe passes a zeroed one too), 14 leave_
static bool esa_vcall(EsaCtx *x, Cpu *c, uint64_t node, int slot) {
    SwArgs a;
    memset(&a, 0, sizeof a);
    a.scratch = 0x600, a.nx = 2, a.x_scratch = 2, a.x[1] = 0;
    SwRet r;
    if (!x->api->vcall(c, node, slot, &a, &r)) { esa_add(&x->n.bad_fn); return false; }
    return true;
}
static void esa_walk(EsaCtx *x, Cpu *c, uint64_t actor, EsxPupEnt *p) {
    const SwApi *api = x->api;
    int n = SW_HAS(api, ai_walk) ? api->ai_walk(c, actor, x->walk, ESA_WALK_MAX) : 0;
    p->atk_walked = 1;
    for (int i = 0; i < n; i++)
        if (esa_vt_is(x, x->walk[i].vt)) esx_pup_atk_put(p, x->walk[i].idx, x->walk[i].node);
    x->n.walk++, x->n.walk_nodes += (uint64_t)(n > 0 ? n : 0);
    if (x->log) LOG("[linkpos] esync attack walk of %llx: %d attack nodes in %d distinct nodes (dll)\n", (unsigned long long)actor, p->natk, n);
    if (x->log)
        for (int i = 0; i < p->natk; i++) LOG("[linkpos] esync   attack node idx %u = %llx (dll)\n", (unsigned)p->aidx[i], (unsigned long long)p->anode[i]);
}
static void esa_end(EsaCtx *x, Cpu *c, EsxPupEnt *p, uint8_t chg0, const char *why) {
    uint64_t node = p->atk_node;
    uint16_t idx = p->atk_idx;
    p->atk_node = 0, p->atk_last = p->atk_idx, p->atk_lchg = chg0, p->atk_end = x->api->frame_id();
    if (!node) return;
    {   // (s179 diag: how many game frames did this attack get its tap?)
        int k = (int)(p->hid % 16);
        if (x->tm.af_hid[k] == p->hid && x->tm.af_n[k]) {
            if (x->log) LOG("[esa] attack hid=%08x idx %u: tapped %u of %llu game frames (%s)\n", p->hid, (unsigned)idx, x->tm.af_n[k],
                            (unsigned long long)(x->tm.af_last[k] - x->tm.af_first[k] + 1), why);
            x->tm.af_n[k] = 0;
        }
    }
    // (c == NULL: the exe's own Cpu, only inside linkpos_frame: the hot-reload / sweep / evict paths. s175 re-review: there the body
    //  must still be FOUND as the same actor (esx_actor_ok fails while it is being deleted), like the exe's esync_frame, which never
    //  calls leave_ on a body it cannot find - a freed node can still read back its old actor pointer)
    uint64_t a = 0;
    int pup = 0;
    bool live = c || (SW_HAS(x->api, in_frame) && x->api->in_frame() && SW_HAS(x->api, esync_actor) &&
                      x->api->esync_actor(p->hid, &a, &pup) && a == p->actor);
    if (live && esa_node_ok(x->api, node, p->actor)) esa_vcall(x, c, node, 14);
    esa_add(&x->n.leave);
    if (x->log) LOG("[linkpos] esync puppet hid=%08x attack idx %u left (%s) (dll) t %llu ms\n", p->hid, (unsigned)idx, why, (unsigned long long)GetTickCount64());
}
static void esa_log_counts(EsaCtx *x) {
    LOG("[linkpos] esync attack (dll): mode %d, %d classes; enter %llu leave %llu (finished %llu, cap %llu, swap %llu, forgot %llu), no-node %llu (bad flag write %llu), "
        "walks %llu (%llu nodes), bad fn %llu\n", x->mode, x->nvt, (unsigned long long)x->n.enter, (unsigned long long)x->n.leave, (unsigned long long)x->n.fin,
        (unsigned long long)x->n.cap, (unsigned long long)x->n.swap, (unsigned long long)x->n.forgot, (unsigned long long)x->n.nonode,
        (unsigned long long)x->n.badwrite, (unsigned long long)x->n.walk, (unsigned long long)x->n.walk_nodes, (unsigned long long)x->n.bad_fn);
}

static void esa_commit(EsaCtx *x, int eff);
// the table is full and a new hid wants a slot: drop the entry with the oldest atk_seen that runs nothing (none: the oldest, ended first)
static void esa_evict(EsaCtx *x, uint32_t hid) {
    int best = -1;
    for (int i = 0; i < x->pup.n; i++) {
        const EsxPupEnt *e = &x->pup.e[i];
        if (e->hid == hid) return;  // (already there: no room needed)
        if (best < 0) { best = i; continue; }
        const EsxPupEnt *b = &x->pup.e[best];
        int ei = e->atk_node == 0, bi = b->atk_node == 0;
        if (ei != bi ? ei : e->atk_seen < b->atk_seen) best = i;
    }
    if (best < 0) return;
    EsxPupEnt *e = &x->pup.e[best];
    if (e->atk_node) esa_end(x, NULL, e, 0, "table full");
    x->pup.e[best] = x->pup.e[--x->pup.n];
}
// s179 diag: time one runner step; log it when over 5 ms (esa_qms: top of the file)
static void esa_slow(const char *what, uint32_t hid, uint64_t node, EsaRes r0) {
    EsaRes r1 = esa_res();
    double ms = r1.ms - r0.ms;
    if (ms > 5) {
        double cpm = esa_cyc_per_ms(), mc = (double)(r1.cyc - r0.cyc) / 1e6;
        LOG("[esa] SLOW %s hid=%08x node %llx: %.1f ms, cpu %.1f ms (%.1f Mcyc), k+u %.1f ms, pf %llu\n", what, hid, (unsigned long long)node, ms,
            cpm > 0 ? mc * 1e6 / cpm : 0.0, mc, (double)(r1.ku100ns - r0.ku100ns) / 1e4, (unsigned long long)(r1.pf - r0.pf));
    }
}
static bool esa_vcall_t(EsaCtx *x, Cpu *c, uint64_t node, int slot, uint32_t hid) {
    EsfSlot *ps = esf_begin(x->log);
    EsaRes t0 = esa_res();
    bool r = esa_vcall(x, c, node, slot);
    EsaRes t1 = esa_res();
    double ms = t1.ms - t0.ms;
    esf_end(ps, ms);
    esa_slow("calc_", hid, node, t0);
#ifndef ESA_NO_EXE
    {   // (s182c diag: the slow part is readable() of the call's stack scratch in swa_invoke: what does Windows say about it?)
        static volatile LONG64 next_ms;
        if (ms > 20 && c && t1.ms > next_ms) {
            next_ms = (LONG64)t1.ms + 2000;
            uint64_t a = c->sp - 0x200;
            MEMORY_BASIC_INFORMATION mb;
            memset(&mb, 0, sizeof mb);
            double q0 = esa_qms();
            SIZE_T got = VirtualQuery((const void *)(uintptr_t)a, &mb, sizeof mb);
            double q1 = esa_qms();
            size_t r1 = x->api->readable(a, 0x40);
            double q2 = esa_qms();
            size_t r2 = x->api->readable(a, 0x40);
            double q3 = esa_qms();
            LOG("[esa] stackq sp=%llx: VirtualQuery %.2f ms (ok %d) base %llx size %llx state %lx prot %lx type %lx alloc %llx; readable x2 %.2f / %.2f ms (%zu %zu)\n",
                (unsigned long long)c->sp, q1 - q0, (int)(got != 0), (unsigned long long)(uintptr_t)mb.BaseAddress, (unsigned long long)mb.RegionSize,
                (unsigned long)mb.State, (unsigned long)mb.Protect, (unsigned long)mb.Type, (unsigned long long)(uintptr_t)mb.AllocationBase, q2 - q1, q3 - q2, r1, r2);
        }
    }
#endif
    while (!esa_lk_tryx(&x->cs_lk)) {}  // (s181: mode 3 calls this from several workers at once; the hold is a few adds)
    x->cs.mcyc += (double)(t1.cyc - t0.cyc) / 1e6, x->cs.ku_ms += (double)(t1.ku100ns - t0.ku100ns) / 1e4, x->cs.pf += t1.pf - t0.pf;
    x->cs.calls++, x->cs.sum += ms;
    if (ms > x->cs.max) x->cs.max = ms;
    if (ms > 5) x->cs.over5++;
    if (ms > 30) x->cs.over30++;
    esa_lk_xu(&x->cs_lk);
    int k = (int)(hid % 16);
    if (x->skip_ms > 0 && ms > x->skip_ms) x->sk_hid[k] = hid, x->sk_left[k] = 2;  // (s180 esaskip: let the game catch up)
    return r;
}
// s180: skip this tap's calc_? (esacalc / esaskip; the attack keeps running, the finished check waits for the next calc_)
static bool esa_calc_skip(EsaCtx *x, const EsxPupEnt *p, uint32_t hid, uint64_t now) {
    int k = (int)(hid % 16);
    if (x->sk_hid[k] == hid && x->sk_left[k] > 0) { x->sk_left[k]--; return true; }
    return x->calc_every > 1 && (now - p->atk_frame) % (uint64_t)x->calc_every != 0;
}
// s181: the enter / calc_ / leave part of one step (mode 2 from the net tick, mode 3 from the AI tap). 1 = a local attack runs
static int esa_apply(EsaCtx *x, Cpu *c, EsxPupEnt *p, uint32_t hid, uint64_t actor, uint16_t act, uint64_t want, int step, int capped, uint8_t chg0, int alive, uint64_t now) {
    const SwApi *api = x->api;
    int ret = 0;
    if (step == ESX_ATK_STEP_LEAVE || step == ESX_ATK_STEP_SWAP) {
        if (capped) esa_add(&x->n.cap);
        if (step == ESX_ATK_STEP_SWAP) esa_add(&x->n.swap);
        esa_end(x, c, p, chg0, capped ? "3 s cap" : step == ESX_ATK_STEP_SWAP ? "owner swapped attack" : !alive ? "dead / feature off" : "owner's action changed");
    }
    if (step == ESX_ATK_STEP_ENTER || step == ESX_ATK_STEP_SWAP) {
        if (!esa_node_ok(api, want, actor)) { esa_add(&x->n.nonode); return 0; }
        uint8_t fl = 0;
        if (!api->read(want + ESA_FLAGS_OFF, &fl, 1)) { esa_add(&x->n.nonode); return 0; }
        fl &= 0xfc;  // (finished / failed: ActionBase::enter clears them; we call enter_ itself. GUESS bits)
        if (!api->write(want + ESA_FLAGS_OFF, &fl, 1)) { esa_add(&x->n.badwrite); return 0; }
        EsfSlot *ps = esf_begin(x->log);
        EsaRes t0 = esa_res();
        bool okv = esa_vcall(x, c, want, 12);
        esf_end(ps, esa_res().ms - t0.ms);
        esa_slow("enter_", hid, want, t0);
        if (!okv) return 0;
        p->atk_node = want, p->atk_idx = act, p->atk_frame = now;
        esa_add(&x->n.enter);
        uint64_t vt = 0;
        esa_u64(api, want, &vt);
        if (x->log) LOG("[linkpos] esync puppet hid=%08x runs its own attack idx %u (node %llx vptr %llx) (dll%s) t %llu ms\n", hid, (unsigned)act, (unsigned long long)want, (unsigned long long)vt, x->mode == 3 ? ", ai phase" : "", (unsigned long long)GetTickCount64());
    }
    if (p->atk_node) {
        uint64_t node = p->atk_node;
        if (!esa_node_ok(api, node, actor)) p->atk_node = 0, esa_add(&x->n.forgot);  // (the node went away under us: no vcall)
        else if (esa_calc_skip(x, p, hid, now)) esa_add_u32(&x->cs.skipped), ret = 1;
        else if (!esa_vcall_t(x, c, node, 31, hid)) esa_end(x, c, p, chg0, "calc_ missing");
        else {
            uint8_t fl = 0;
            if (api->read(node + ESA_FLAGS_OFF, &fl, 1) && (fl & 1)) { esa_add(&x->n.fin); esa_end(x, c, p, chg0, "finished"); }
            else ret = 1;
        }
    }
    return ret;
}
#include "sora_esa_ai.h"  // (s181 mode 3: the AI-phase runner)

// the tap body: per puppet frame. 1 = the local attack runs (the exe then keeps slot 0 as it is)
static int esa_frame(EsaCtx *x, Cpu *c, uint32_t hid, uint64_t actor, const NetEsEnt *t, int alive) {
    const SwApi *api = x->api;
    if (!api || !t || !hid) return 0;
    if (x->mode == 3) {  // (s181: the AI tap does the guest calls; here only the owner's wish + the AS answer)
        int r = esa_ai_net(x, hid, actor, t, alive);
        if (x->pending >= 0 && !esa_running(x) && esa_ai_quiet(x)) esa_commit(x, x->pending);
        uint64_t nw = api->frame_id();
        if (x->mode == 3 && nw >= x->next_log) x->next_log = nw + 600, esa_log_counts(x);
        return r;
    }
    int draining = x->pending >= 0;
    int on = x->mode == 2 && !draining;
    uint64_t now = api->frame_id();
    for (int i = 0; i < x->pup.n; i++)  // (the body was replaced: its nodes went with it)
        if (x->pup.e[i].hid == hid && x->pup.e[i].actor != actor && x->pup.e[i].atk_node) x->n.forgot++;
    if (on && x->pup.n == ESX_PUP_MAX) esa_evict(x, hid);
    EsxPupEnt *p = esx_pup_get(&x->pup, hid, actor, on);
    if (!p) { if (draining && !esa_running(x)) esa_commit(x, x->pending); return 0; }
    if (p->atk_seen && now - p->atk_seen > ESA_STALE) {  // (the enemy was not drawn for a while: the exe ends its attack the same way)
        if (p->atk_node) esa_end(x, c, p, t->anim_chg[0], "no puppet update");  // (it re-checks the actor)
        p->atk_walked = 0, p->natk = 0;  // (s175 re-review: idle entries too - a body back at the same address must walk again)
    }
    if (draining) alive = 0;
    if (on && t->action && !p->atk_walked) { EsaRes t0 = esa_res(); esa_walk(x, c, actor, p); esa_slow("walk", hid, 0, t0); }
    uint64_t want = on && t->action ? esx_pup_atk_node(p, t->action) : 0;
    int step = esx_atk_step(p, t->action, want != 0, t->anim_chg[0], now, ESA_CAP, ESA_GAP, alive);
    int capped = p->atk_node && now - p->atk_frame > ESA_CAP;
    p->atk_seen = now;
    if (p->atk_node) {  // (s178 diag: how often does a running puppet attack get its tap?)
        double ms = (double)GetTickCount64();
        int k = (int)(hid % 16);
        if (x->tm.hid[k] == hid && x->tm.hid_last[k] > 0) {
            double g = ms - x->tm.hid_last[k];
            if (g < 2000) { x->tm.tap_sum += g, x->tm.taps++; if (g > x->tm.tap_max) x->tm.tap_max = g; if (g > 100) x->tm.tap_over100++; }
        }
        x->tm.hid[k] = hid, x->tm.hid_last[k] = ms;
        if (x->tm.af_hid[k] == hid && x->tm.af_n[k] > 0) {
            uint64_t fg = now - x->tm.af_last[k];
            if (fg >= 1) x->tm.gap_hist[fg >= 6 ? 5 : fg - 1]++;
        } else x->tm.af_hid[k] = hid, x->tm.af_first[k] = now, x->tm.af_n[k] = 0;
        if (x->tm.af_n[k] == 0 || now != x->tm.af_last[k]) x->tm.af_n[k]++;
        x->tm.af_last[k] = now;
    }
    int ret = esa_apply(x, c, p, hid, actor, t->action, want, step, capped, t->anim_chg[0], alive, now);
    if (draining && !esa_running(x)) esa_commit(x, x->pending);
    if (x->mode == 2 && now >= x->next_log) x->next_log = now + 600, esa_log_counts(x);
    return ret;
}

// ---- mode switching ----
static void esa_env_put(int m) {
#ifndef ESA_NO_EXE
    char b[8];
    snprintf(b, sizeof b, "%d", m);
    SetEnvironmentVariableA(ESA_ENV, b);  // (a hot reload resets this copy's statics: the process env remembers the mode)
#else
    (void)m;
#endif
}
static int esa_env_get(void) {
#ifndef ESA_NO_EXE
    char b[16];
    DWORD n = GetEnvironmentVariableA(ESA_ENV, b, sizeof b);
    return n > 0 && n < sizeof b ? atoi(b) : -1;
#else
    return -1;
#endif
}
static int esa_tap_fn(Cpu *c, uint32_t hid, uint64_t actor, const void *target, int alive) { return esa_frame(&g_esa, c, hid, actor, (const NetEsEnt *)target, alive); }
static void esa_bind(EsaCtx *x) {
    if (x->bound) return;
    x->bound = 1;
#ifndef ESA_NO_EXE
    HMODULE exe = GetModuleHandleA(NULL);
    const SwApi *(*get)(uint32_t) = (const SwApi *(*)(uint32_t))(void *)GetProcAddress(exe, SW_API_EXPORT);
    x->api = get ? get(SW_API_VERSION) : NULL;
    x->atkfn = (int (*)(int, const uint64_t *, int))(void *)GetProcAddress(exe, "linkpos_esync_atk");
#endif
    if (x->api) esa_defaults(x);
}
static bool esa_api_ok(const EsaCtx *x) {
    const SwApi *a = x->api;
    return SW_HAS(a, tap_set) && SW_HAS(a, vcall) && SW_HAS(a, ai_walk) && SW_HAS(a, read) && SW_HAS(a, write) && SW_HAS(a, readable) && SW_HAS(a, frame_id) && x->nvt > 0;
}
static int esa_call_exe(EsaCtx *x, int on) { return x->atkfn ? x->atkfn(on, NULL, 0) : -2; }
static void esa_commit(EsaCtx *x, int eff) {
    int was = x->mode;
    x->mode = eff, x->pending = -1;
    if (eff == 2 || eff == 3) {
        x->api->tap_set(SW_TAP_PUPPET_ATTACK, (void *)esa_tap_fn);
        x->api->tap_set(SW_TAP_PUPPET_AI, eff == 3 ? (void *)esa_ai_tap_fn : NULL);  // (s181 mode 3: the AI-phase tap does the guest calls)
        esa_call_exe(x, 0);  // (the exe runner off: the tap replaces it)
        x->next_log = 0;
        if (eff == 3) LOG("[esync] puppet attack actions: DLL runner on IN THE AI PHASE (mode 3, %d classes; RootAi::calc tap set, exe runner off)\n", x->nvt);
        else
        LOG("[esync] puppet attack actions: DLL runner on (%d classes; tap set, exe runner off)\n", x->nvt);
    } else {
        if ((was == 2 || was == 3) && x->api) x->api->tap_set(SW_TAP_PUPPET_ATTACK, NULL), x->api->tap_set(SW_TAP_PUPPET_AI, NULL);
        int r = esa_call_exe(x, eff);
        if (r == -2) LOG("[esync] this exe has no linkpos_esync_atk (built before s174): set esyncatk does nothing\n");
        else LOG("[esync] puppet attack actions -> %d classes in use (0 = off, -1 = exe esync off)\n", r);
    }
}
// idempotent: the exe runner at mode 0 / 1 (what esa_commit does for those, without the tap or the log lines)
static void esa_commit_exe(EsaCtx *x, int m) {
    x->mode = m, x->pending = -1;
    esa_call_exe(x, m);
}
// live `set esyncatk <0|1|2|3>` (m > 3 or m < 0 acts as 1: the old key was "!= 0"). 3 = s181 AI-phase runner (needs the s181 exe; else 2)
static void esa_set_mode(EsaCtx *x, int m) {
    esa_bind(x);
    if (m < 0 || m > 3) m = 1;  // (the old key was "nonzero = on")
    int eff = m;
    if (m >= 2 && !esa_api_ok(x)) {
        if (!x->api) LOG("[esa] this exe has no sw_api (built before s175): set esyncatk 2 acts as 1\n");
        else if (x->nvt == 0) LOG("[esa] sw_api present but no attack classes (site lookup 0): set esyncatk 2 acts as 1\n");
        else LOG("[esa] sw_api present but lacks a needed primitive: set esyncatk 2 acts as 1\n");
        eff = 1;
    } else if (m == 3 && !esa_ai_supported(x)) {
        LOG("[esa] this exe does not call the AI-phase tap (built before s181): set esyncatk 3 acts as 2\n");
        eff = 2;
    }
    x->want = m;
    esa_env_put(m);
    if (x->mode == 3 && eff != 3) {  // (s181 review 2: pending FIRST, so a worker locking a slot after this sees "off"; then the barrier)
        x->pending = eff, x->drain_since = x->api->frame_id();
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        if (!esa_running(x) && esa_ai_quiet(x)) { esa_commit(x, eff); return; }
        LOG("[esync] puppet attack actions: AI-phase runner leaving %d running attack(s) before switching to %d\n", esa_running(x), eff);
        return;
    }
    if (x->mode == 2 && eff != 2 && esa_running(x)) {  // the tap leaves what runs (its Cpu is needed), then commits
        x->pending = eff, x->drain_since = x->api->frame_id();
        LOG("[esync] puppet attack actions: DLL runner leaving %d running attack(s) before switching to %d\n", esa_running(x), eff);
        return;
    }
    esa_commit(x, eff);  // (idempotent: also for 2 -> 2, which re-asserts the tap; a drain in progress is cancelled)
}
// Attacks the tap can no longer end: the enemy died / its brain is back on / it is not a puppet any more / it unloaded. The tap is not called
// for those frames, so the sweep (every net tick, inside linkpos_frame) does what the exe's esync_frame does. "dead" has no cheap read here:
// the tap stops being called at life <= 0, so it ends through the ESA_STALE rule below (<= 30 frames).
static void esa_sweep(EsaCtx *x) {
    const SwApi *api = x->api;
    if (!api || !SW_HAS(api, esync_actor) || !SW_HAS(api, frame_id) || !SW_HAS(api, in_frame) || !api->in_frame()) return;
    uint64_t now = api->frame_id();
    for (int i = 0; i < x->pup.n; i++) {
        EsxPupEnt *p = &x->pup.e[i];
        if (!p->atk_node) continue;
        uint64_t a = 0;
        int pup = 0;
        bool found = api->esync_actor(p->hid, &a, &pup);
        if (!found) {  // (s175 re-review: like the exe (ENEMY_STALE 60): a body we cannot find is only forgotten, never leave_'d)
            if (now - p->atk_seen > 60) p->atk_node = 0, x->n.forgot++;
            continue;
        }
        const char *why = a != p->actor ? "body replaced" : !pup ? "puppet gone / brain back" : now - p->atk_seen > ESA_STALE ? "no puppet update" : NULL;
        if (!why) continue;
        if (a != p->actor) p->atk_node = 0, x->n.forgot++;  // (its nodes went with the old body)
        else esa_end(x, NULL, p, 0, why);
    }
    esa_ai_sweep(x);  // (s181 mode 3 slots)
}
// the old DLL copy going away: leave every running attack with the exe's Cpu (a hot reload mid-swing)
static void esa_unload(EsaCtx *x) {
    esf_stop();  // (s182: FIRST: join the sampler thread, it runs code of this DLL copy; no calc_ / enter_ can start one again)
    if (!x->api) return;
    esa_ai_unload(x);  // (s181: the AI tap off first, then its running attacks)
    bool inf = SW_HAS(x->api, in_frame) && x->api->in_frame();
    for (int i = 0; i < x->pup.n; i++) {
        EsxPupEnt *p = &x->pup.e[i];
        if (!p->atk_node) continue;
        if (inf) esa_end(x, NULL, p, 0, "dll unload");
        else p->atk_node = 0;
    }
}
// every net tick: after a hot reload this copy's statics are fresh and the exe cleared the taps - take the mode from the process env
// and re-apply it (only mode 2 needs it: 0 / 1 are the exe's own state, which a reload does not touch)
static void esa_tick(EsaCtx *x) {
    {   // (s178 diag: DLL frame gaps)
        double ms = (double)GetTickCount64();
        if (x->tm.tick_last == 0) LOG("[esa] diag: thread cycles per ms = %.0f (QPC calibration)\n", esa_cyc_per_ms());
        if (x->tm.tick_last > 0) {
            double g = ms - x->tm.tick_last;
            x->tm.tick_sum += g, x->tm.ticks++;
            if (g > x->tm.tick_max) x->tm.tick_max = g;
            if (g > 100) x->tm.tick_over100++;
        }
        x->tm.tick_last = ms;
        if (x->mode >= 2 && ms >= (double)x->tm.gap_log) {
            x->tm.gap_log = (uint64_t)ms + 10000;
            if (x->tm.ticks)
                LOG("[esa] timing 10 s: mod frames %u (avg %.0f ms, max %.0f ms, %u over 100 ms); running puppet attack taps %u (avg %.0f ms, max %.0f ms, %u over 100 ms)\n",
                    x->tm.ticks, x->tm.tick_sum / x->tm.ticks, x->tm.tick_max, x->tm.tick_over100, x->tm.taps, x->tm.taps ? x->tm.tap_sum / x->tm.taps : 0.0,
                    x->tm.tap_max, x->tm.tap_over100);
            if (x->tm.ticks)
                LOG("[esa] timing 10 s: tap gaps in game frames 1:%u 2:%u 3:%u 4:%u 5:%u 6+:%u\n", x->tm.gap_hist[0], x->tm.gap_hist[1], x->tm.gap_hist[2],
                    x->tm.gap_hist[3], x->tm.gap_hist[4], x->tm.gap_hist[5]);
            if (x->cs.calls || x->cs.skipped)
                LOG("[esa] timing 10 s: calc_ %u calls (avg %.2f ms, max %.1f ms, %u over 5 ms, %u over 30 ms), skipped %u (esacalc %d, esaskip %d)\n", x->cs.calls,
                    x->cs.calls ? x->cs.sum / x->cs.calls : 0.0, x->cs.max, x->cs.over5, x->cs.over30, x->cs.skipped, x->calc_every, x->skip_ms);
            if (x->cs.calls) {
                double cpm = esa_cyc_per_ms();
                LOG("[esa] timing 10 s: calc_ wall %.0f ms, cpu %.0f ms (%.0f Mcyc), k+u %.0f ms, pf %llu (cycles/ms %.0f)\n", x->cs.sum,
                    cpm > 0 ? x->cs.mcyc * 1e6 / cpm : 0.0, x->cs.mcyc, x->cs.ku_ms, (unsigned long long)x->cs.pf, cpm);
            }
            if (x->mode == 3)
                LOG("[esa] timing 10 s (ai phase): taps %llu, steps %llu, slot busy %llu, no slot %llu, net busy %llu, full %llu, replaced %llu, AS predicted %llu, sweep left %llu forgot %llu, walk oom %llu\n",
                    (unsigned long long)x->an.taps, (unsigned long long)x->an.steps, (unsigned long long)x->an.busy, (unsigned long long)x->an.noslot,
                    (unsigned long long)x->an.net_busy, (unsigned long long)x->an.full, (unsigned long long)x->an.replaced, (unsigned long long)x->an.predict,
                    (unsigned long long)x->an.sweep_left, (unsigned long long)x->an.sweep_forgot, (unsigned long long)x->an.walk_oom);
            while (!esa_lk_tryx(&x->cs_lk)) {}  // (s181: workers add to cs)
            memset(&x->cs, 0, sizeof x->cs);
            esa_lk_xu(&x->cs_lk);
            memset(x->tm.gap_hist, 0, sizeof x->tm.gap_hist);
            x->tm.ticks = x->tm.taps = x->tm.tick_over100 = x->tm.tap_over100 = 0;
            x->tm.tick_sum = x->tm.tick_max = x->tm.tap_sum = x->tm.tap_max = 0;
        }
    }
    if (!x->init) {
        x->init = 1;
        int m = esa_env_get();
        if (m == 2 || m == 3) esa_set_mode(x, m);
        else if (m >= 0) {
            x->want = m;
            esa_bind(x);
            if (x->atkfn) esa_commit_exe(x, m);  // (a reload during a 2 -> 1 drain: make sure the exe runner is really on)
        }
    }
    esa_sweep(x);
    if (x->mode == 3 && x->pending >= 0 && !esa_running(x) && esa_ai_quiet(x)) esa_commit(x, x->pending);  // (s181: the drain also ends with no puppet ticks)
    if ((x->mode == 2 || x->mode == 3) && x->pending < 0 && SW_HAS(x->api, tap_set)) {  // (the exe's reboot keeps the taps; a re-assert is one atomic store)
        x->api->tap_set(SW_TAP_PUPPET_ATTACK, (void *)esa_tap_fn);
        if (x->mode == 3) x->api->tap_set(SW_TAP_PUPPET_AI, (void *)esa_ai_tap_fn);
    }
    if (x->pending >= 0 && x->api && x->api->frame_id() - x->drain_since > ESA_DRAIN_FRAMES) {
        LOG("[esync] puppet attack actions: tap did not leave the running attacks in time, dropping them\n");
        for (int i = 0; i < x->pup.n; i++) x->pup.e[i].atk_node = 0;
        esa_ai_drop(x);  // (s181 mode 3 slots)
        esa_commit(x, x->pending);
    }
}

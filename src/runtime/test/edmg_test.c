// host test for runtime/linkpos_edmg.h (s166 E2: pending map, echo guard, f54 table, kill clamp) - no linkpos, no guest memory.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/edmg_test.c -o build_h/edmg_test.exe   (sh recon/make_edmg_test.sh <tree>)
#include <stdio.h>
#include <string.h>
#include "linkpos_edmg.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static EdmgPendTab T;

static void test_pending_basic(void) {
    memset(&T, 0, sizeof T);
    EdmgPend o;
    CHECK(!edmg_pend_add(&T, 0, 5, 15, 0, 0, 10));           // hid 0 never stored
    CHECK(!edmg_pend_add(&T, 7, 0, 15, 0, 0, 10));           // nothing to store
    CHECK(edmg_pend_add(&T, 7, 3, 15, 0, 0, 10) && T.n == 1);
    CHECK(edmg_pend_add(&T, 7, 4, 0x15, 1, 0, 20) && T.n == 1);   // same hid: summed, highest f54 wins
    CHECK(edmg_pend_take(&T, 7, 30, &o) && o.dmg == 7 && o.f54 == 0x15 && o.f40 == 1 && !o.kill && T.n == 0);
    CHECK(!edmg_pend_take(&T, 7, 30, &o));                   // taken once
}
static void test_pending_ttl_and_kill(void) {
    memset(&T, 0, sizeof T);
    EdmgPend o;
    edmg_pend_add(&T, 1, 5, 15, 0, 0, 100);                  // damage, expires
    edmg_pend_add(&T, 2, 0, 15, 0, 1, 100);                  // kill (dealt 0 is fine for a kill), no TTL
    edmg_pend_add(&T, 3, 2, 15, 0, 0, 100);
    edmg_pend_add(&T, 3, 2, 15, 0, 1, 150);                  // damage then kill on the same enemy: kill wins
    CHECK(T.n == 3);
    edmg_pend_expire(&T, 100 + EDMG_PEND_TTL);               // exactly the TTL: still there
    CHECK(T.n == 3);
    edmg_pend_expire(&T, 100 + EDMG_PEND_TTL + 1);           // damage-only entries older than TTL go; kills stay
    CHECK(T.n == 2 && T.expired == 1);
    CHECK(!edmg_pend_take(&T, 1, 0, &o));
    CHECK(edmg_pend_take(&T, 2, 100 + 100 * EDMG_PEND_TTL, &o) && o.kill && o.dmg == 0);   // a far kill survives "forever"
    CHECK(edmg_pend_take(&T, 3, 100 + 100 * EDMG_PEND_TTL, &o) && o.kill && o.dmg == 4);
    // an expired entry found by take (no expire call before it) is refused and removed
    edmg_pend_add(&T, 9, 5, 15, 0, 0, 0);
    CHECK(!edmg_pend_take(&T, 9, EDMG_PEND_TTL + 5, &o) && T.n == 0 && T.expired == 2);
}
static void test_pending_full(void) {
    memset(&T, 0, sizeof T);
    for (uint32_t h = 1; h <= EDMG_PEND_MAX; h++) edmg_pend_add(&T, h, 1, 15, 0, h % 2 == 0, h);   // even hids = kills
    CHECK(T.n == EDMG_PEND_MAX);
    edmg_pend_add(&T, 1000, 1, 15, 0, 0, 1000);               // full: the oldest DAMAGE entry (hid 1) is evicted, kills stay
    CHECK(T.n == EDMG_PEND_MAX && T.evicted == 1);
    EdmgPend o;
    CHECK(!edmg_pend_take(&T, 1, 1000, &o) && edmg_pend_take(&T, 2, 1000, &o) && o.kill);
    memset(&T, 0, sizeof T);
    for (uint32_t h = 1; h <= EDMG_PEND_MAX; h++) edmg_pend_add(&T, h, 1, 15, 0, 1, h + 10);          // all kills
    edmg_pend_add(&T, 2000, 1, 15, 0, 1, 5000);               // the oldest kill (hid 1) goes
    CHECK(T.n == EDMG_PEND_MAX && T.evicted == 1 && !edmg_pend_take(&T, 1, 5000, &o) && edmg_pend_take(&T, 2000, 5000, &o));
}

// (s191, Melty spec 4.1) a player inside a shrine for a long time gets every overworld hit / kill of the others as "not loaded here"
// = pending (linkpos.c: edmg_pend_expire + edmg_pend_add on each pending hit, expire again every 600 frames). Nothing piles up past
// EDMG_PEND_MAX, damage-only entries never outlive the TTL, the newest kills are kept (an hid is unique across maps, s166 data:
// a pending entry can only ever meet its own enemy).
static void test_pending_other_map_flood(void) {
    memset(&T, 0, sizeof T);
    int maxn = 0, stale_dmg = 0;
    uint64_t f = 0;
    uint32_t last_kill = 0;
    for (int i = 0; i < 20000; i++, f += 6) {  // (~33 min at 60 fps: 4000 overworld enemies hit, 80 of them killed)
        uint32_t hid = 0x10000u + (uint32_t)(i % 4000);
        int kill = i % 50 == 49;
        edmg_pend_expire(&T, f);
        edmg_pend_add(&T, hid, 7, 15, 0, kill, f);
        if (kill) last_kill = hid;
        if (f % 600 == 0) edmg_pend_expire(&T, f);
        if (T.n > maxn) maxn = T.n;
        for (int k = 0; k < T.n; k++) stale_dmg += !T.e[k].kill && f - T.e[k].first > EDMG_PEND_TTL;
    }
    CHECK(maxn <= EDMG_PEND_MAX && T.n == EDMG_PEND_MAX && stale_dmg == 0 && T.evicted > 0);
    int kills = 0;
    for (int k = 0; k < T.n; k++) kills += T.e[k].kill;
    for (uint64_t g = f; g <= f + EDMG_PEND_TTL + 600; g += 600) edmg_pend_expire(&T, g);  // (she stays in: the 600-frame expiry runs on)
    CHECK(kills == 80 && T.n == 80);  // (only the kills are left)
    EdmgPend o;
    CHECK(edmg_pend_take(&T, last_kill, f + 100 * EDMG_PEND_TTL, &o) && o.kill);  // (the newest far kill waits as long as it takes)
}

static void test_amount_plan(void) {
    CHECK(edmg_remote_amount(13, 4, 0) == 4);
    CHECK(edmg_remote_amount(13, 40, 0) == 12);               // a hit that did not kill there never kills here
    CHECK(edmg_remote_amount(1, 4, 0) == 0);
    CHECK(edmg_remote_amount(13, 4, 1) == 13);                // kill, local HP higher than the damage: dealt = local life
    CHECK(edmg_remote_amount(3, 50, 1) == 50);                // kill, local HP lower: the sender's damage
    CHECK(edmg_remote_amount(0, 50, 1) == 0);                 // already dead
    CHECK(edmg_remote_amount(-3, 5, 0) == 0);
    CHECK(edmg_remote_amount(13, 0, 1) == 13);                // a kill with dealt 0 (a far kill from pending)
    CHECK(edmg_apply_plan(0, 0, 0, 4, 0) == EDMG_PENDING);    // not loaded -> pending
    CHECK(edmg_apply_plan(0, 0, 0, 0, 1) == EDMG_PENDING);    // a kill is kept
    CHECK(edmg_apply_plan(0, 0, 0, 0, 0) == EDMG_IGNORED);
    CHECK(edmg_apply_plan(1, 13, 0, 4, 0) == EDMG_QUEUED);
    CHECK(edmg_apply_plan(1, 0, 0, 4, 0) == EDMG_IGNORED);    // life 0
    CHECK(edmg_apply_plan(1, 13, 3, 4, 1) == EDMG_PENDING);   // being deleted while alive (unloading): kept (s167 SF-6)
    CHECK(edmg_apply_plan(1, 0, 3, 4, 1) == EDMG_IGNORED);    // being deleted, dead
    CHECK(edmg_apply_plan(1, -1, 0, 0, 1) == EDMG_PENDING);   // life unreadable: kept
    CHECK(edmg_apply_plan(1, -1, 0, 0, 0) == EDMG_IGNORED);
    CHECK(edmg_apply_plan(1, 1, 0, 4, 0) == EDMG_IGNORED);    // 1 life left, non-kill hit: capped to 0
    CHECK(edmg_apply_plan(1, 1, 0, 4, 1) == EDMG_QUEUED);
}

static void test_echo(void) {
    EdmgEchoTab e;
    memset(&e, 0, sizeof e);
    edmg_echo_add(&e, 0xA0, 5, 100);
    edmg_echo_add(&e, 0xA0, 3, 100);                          // same frame, same actor: summed
    edmg_echo_add(&e, 0xB0, 2, 100);
    CHECK(e.n == 2);
    CHECK(edmg_echo_take(&e, 0xA0, 100) == 8 && e.n == 1);    // the capture hook of that frame: subtract 8
    CHECK(edmg_echo_take(&e, 0xA0, 100) == 0);                // consumed
    CHECK(edmg_echo_take(&e, 0xB0, 101) == 2);                // applyDamage one frame later still counts
    edmg_echo_add(&e, 0xC0, 4, 200);
    CHECK(edmg_echo_take(&e, 0xC0, 203) == 0 && e.n == 0);    // 3 frames old = never taken, gone (it would eat a LOCAL hit otherwise)
    edmg_echo_add(&e, 0xD0, 4, 300);
    edmg_echo_add(&e, 0xE0, 4, 310);                          // adding purges the old one
    CHECK(e.n == 1);
    for (int i = 0; i < EDMG_ECHO_N + 4; i++) edmg_echo_add(&e, 0x1000 + (uint64_t)i, 1, 400 + (uint64_t)i / 4);
    CHECK(e.n <= EDMG_ECHO_N);
    edmg_echo_add(&e, 0, 4, 500);
    edmg_echo_add(&e, 0xF0, 0, 500);
    CHECK(edmg_echo_take(&e, 0xF0, 500) == 0);
}

// the capture hook's arithmetic with the echo guard (what dmg_apply_hook does, minus the guest): drop = before - after
static int hook_dealt(EdmgEchoTab *e, uint64_t actor, uint64_t frame, int before, int after) {
    int drop = before - after, echo = edmg_echo_take(e, actor, frame);
    if (echo > drop) echo = drop;
    return drop - echo;
}
static void test_echo_flow(void) {
    EdmgEchoTab e;
    memset(&e, 0, sizeof e);
    // remote 4 damage ridden at frame 10, applied the same frame: nothing is re-sent
    edmg_echo_add(&e, 0xA0, 4, 10);
    CHECK(hook_dealt(&e, 0xA0, 10, 13, 9) == 0);
    // remote 4 + a local hit of 2 in the same drop (6): only the local 2 goes out
    edmg_echo_add(&e, 0xA0, 4, 11);
    CHECK(hook_dealt(&e, 0xA0, 11, 9, 3) == 2);
    // a later local hit is not eaten by a stale echo
    CHECK(hook_dealt(&e, 0xA0, 12, 3, 1) == 2);
    // remote kill clamped by the local life (3 left, remote amount 13 via the kill clamp): drop 3, echo 13 -> 0 sent
    edmg_echo_add(&e, 0xA0, 13, 20);
    CHECK(hook_dealt(&e, 0xA0, 20, 3, 0) == 0);
    // two players hitting at once (no ping-pong): A got B's 4 as remote, its own local 5 in the same frame -> sends 5 only
    edmg_echo_add(&e, 0xB0, 4, 30);
    CHECK(hook_dealt(&e, 0xB0, 30, 20, 11) == 5);
}

static void test_args(void) {
    EdmgArgTab a;
    memset(&a, 0, sizeof a);
    int32_t f54 = 0, f40 = 0;
    edmg_arg_note(&a, 0xA0, 50, 15, 0);
    edmg_arg_note(&a, 0xA0, 50, 0x15, 1);                     // same frame: highest f54 with its f40
    edmg_arg_note(&a, 0xA0, 50, 0x11, 2);
    CHECK(edmg_arg_get(&a, 0xA0, 50, &f54, &f40) && f54 == 0x15 && f40 == 1);
    CHECK(edmg_arg_get(&a, 0xA0, 51, &f54, &f40) && f54 == 0x15);   // applyDamage one frame later (SHOULD-FIX 2)
    CHECK(!edmg_arg_get(&a, 0xA0, 52, &f54, &f40));
    edmg_arg_note(&a, 0xA0, 60, 15, 0);                       // a new frame starts over
    CHECK(edmg_arg_get(&a, 0xA0, 60, &f54, &f40) && f54 == 15 && f40 == 0);
    CHECK(!edmg_arg_get(&a, 0xB0, 60, &f54, &f40));
    for (int i = 0; i < EDMG_ARG_N + 3; i++) edmg_arg_note(&a, 0x100 + (uint64_t)i, 70 + (uint64_t)i, 15, 0);
    CHECK(a.n == EDMG_ARG_N);
}

// s194 BUG A: a ride entry made only for a remote hit had f54 = 15 as a placeholder and kept the higher value, so the
// attacker's f54 7 (Shock: upright stagger) was applied as 15 (small flinch + partial ragdoll) on the owner's game.
typedef struct { int32_t f54, f40; } RideArgs;
static void test_ride_f54(void) {
    RideArgs r = {EDMG_RIDE_F54_NONE, 0};                     // fresh entry (memset): no real hit yet
    CHECK(edmg_ride_f54(r.f54) == EDMG_F54_SMALL);            // nothing merged: the ride uses the small default
    edmg_f54_merge(&r.f54, &r.f40, 7, 0);                     // remote f54 7 on a fresh entry stays 7
    CHECK(r.f54 == 7 && edmg_ride_f54(r.f54) == 7);
    edmg_f54_merge(&r.f54, &r.f40, 7, 0);                     // a local f54 7 hit on the same enemy, same frame: still 7
    CHECK(r.f54 == 7);
    edmg_f54_merge(&r.f54, &r.f40, 0x15, 1);                  // between real hits the highest wins, with its f40
    CHECK(r.f54 == 0x15 && r.f40 == 1);
    edmg_f54_merge(&r.f54, &r.f40, 7, 2);                     // a lower one later does not undo it
    CHECK(r.f54 == 0x15 && r.f40 == 1);
    RideArgs s = {EDMG_RIDE_F54_NONE, 0};
    edmg_f54_merge(&s.f54, &s.f40, 0, 3);                     // f54 <= 0 from a caller = small (15), counts as a real hit
    CHECK(s.f54 == EDMG_F54_SMALL && s.f40 == 3);
    edmg_f54_merge(&s.f54, &s.f40, 7, 0);                     // a real 15 beats a real 7 (the old rule between real hits)
    CHECK(s.f54 == EDMG_F54_SMALL && s.f40 == 3);
    RideArgs u = {EDMG_RIDE_F54_NONE, 0};
    edmg_f54_merge(&u.f54, &u.f40, 3, 2);                     // remote low f54 + element on a fresh entry: both kept
    CHECK(u.f54 == 3 && u.f40 == 2);
}

int main(void) {
    test_ride_f54();
    test_pending_basic();
    test_pending_ttl_and_kill();
    test_pending_full();
    test_pending_other_map_flood();
    test_amount_plan();
    test_echo();
    test_echo_flow();
    test_args();
    printf("%d checks, %d failed\n", runs, fails);
    return fails != 0;
}

// host test for the s289 netloot CORE (runtime/sora_net_loot.h without its glue) + the NET_LOOT wire (runtime/sora_net_proto.h):
// the names, gids, the list (add / take / tombs / expiry / the tie lookup), who killed it, the death watch (first-seen rule, what
// it held, the nearest, the body it follows), what falls near Link, Link's ring, pickups, the map list, the hush, the placement
// matrix, the revival flag's name + hash, the GameData lookup over fake memory, and the wire: round trips, refusals, malformed
// packets, NET_WORLD (14) rejected. No NET, no game (the glue is compiled with -fsyntax-only through net_glue_test.c).
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/netloot_test.c -o build_h/netloot_test.exe
#include <stdio.h>
#include <stdlib.h>
#include "sora_net_loot.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static NetLootAdd mkadd(uint32_t gid, uint32_t src, uint8_t kind, const char *name, float x, float y, float z) {
    NetLootAdd a;
    memset(&a, 0, sizeof a);
    a.gid = gid, a.src = src, a.kind = kind;
    snprintf(a.name, sizeof a.name, "%s", name);
    a.pos[0] = x, a.pos[1] = y, a.pos[2] = z;
    return a;
}

static void test_names(void) {
    CHECK(lt_is_loot("Item_Enemy_00") && lt_is_loot("Weapon_Sword_001") && lt_is_loot("PutRupee_Gold") && lt_is_loot("Item_Ore_A"));
    CHECK(!lt_is_loot("Item_Conductor") && !lt_is_loot("Item_Magnetglove") && !lt_is_loot("Item_MagnetgloveLv2"));
    CHECK(!lt_is_loot("Enemy_Bokoblin_Junior") && !lt_is_loot("Obj_TreeApple") && !lt_is_loot("Armor_001_Head") && !lt_is_loot(NULL));
    // (s289 review3 #1/#2: the receive side refuses these: a chest's flag, an enemy made here from a friend's packet)
    CHECK(!lt_is_loot("TBox_Field_Stone") && !lt_is_loot("Enemy_Lynel_Dark") && !lt_is_loot("") && !lt_is_loot("Item"));
    CHECK(lt_drop_class("Item_Enemy_01", false) && lt_drop_class("Item_Meat_01", false) && lt_drop_class("PutRupee", false));
    CHECK(!lt_drop_class("Weapon_Sword_001", false) && lt_drop_class("Weapon_Sword_001", true) && !lt_drop_class("Item_Ore_A", false));
    CHECK(!lt_drop_class(NULL, false) && lt_drop_class(NULL, true));
    // "fell near Link": his always; an enemy's kind or first seen by an enemy never
    CHECK(lt_fell_ok("Item_Enemy_00", true, true) && !lt_fell_ok("Item_Enemy_00", false, false) && lt_fell_ok("Item_Ore_A", false, false));
    CHECK(!lt_fell_ok("Item_Ore_A", false, true) && !lt_fell_ok("PutRupee_Silver", false, false) && lt_fell_ok("Weapon_Sword_001", false, false));
}

static void test_gids(void) {
    uint32_t ctr = 0xfffffe;
    CHECK(lt_gid_next(5, &ctr) == 0x05ffffffu && ctr == 0xffffff);
    CHECK(lt_gid_next(5, &ctr) == 0x05000001u && ctr == 1);  // (the counter never gives 0)
    CHECK(lt_gid_origin(0x05000001u) == 5 && lt_gid_origin(0xff123456u) == 255);
    uint32_t c0 = 0;
    CHECK(lt_gid_next(0, &c0) == 1);  // (player 0: still not 0)
    int n = 0;
    for (int i = 0; i < 1000; i++) n = lt_ring_next(n, 16);
    CHECK(n >= 16 && n < 32);  // (a ring's count stays bounded and "full")
    CHECK(lt_ring_next(0, 16) == 1 && lt_ring_next(15, 16) == 16);
}

static LtList g_l;
static void test_list(void) {
    LtList *L = &g_l;
    memset(L, 0, sizeof *L);
    NetLootAdd a = mkadd(0x01000001u, 0x1234, NLK_DROP, "Item_Enemy_00", 1, 2, 3), b = mkadd(0x02000001u, 0, NLK_FELL, "Item_Ore_A", 4, 5, 6);
    int at = 5;
    CHECK(lt_add(L, &a, 1000, &at) == LTR_NEW && at == 0 && L->n == 1 && L->e[0].hix == -1 && L->e[0].state == LTE_WAIT);
    CHECK(L->e[0].here[0] == 1 && L->e[0].here[2] == 3 && L->e[0].at == 1000);
    CHECK(lt_add(L, &a, 2000, &at) == LTR_DUP && at == 0 && L->n == 1);
    CHECK(lt_add(L, &b, 0, &at) == LTR_NEW && at == 1 && L->e[1].at == 1);  // (at 0 -> 1: "came" is never 0)
    NetLootAdd z = a;
    z.gid = 0;
    CHECK(lt_add(L, &z, 5, &at) == LTR_BAD && at == -1);
    z = a, z.gid = 9, z.name[0] = 0;
    CHECK(lt_add(L, &z, 5, NULL) == LTR_BAD && lt_add(L, NULL, 5, NULL) == LTR_BAD && L->n == 2);
    CHECK(lt_find(L, a.gid) == 0 && lt_find(L, b.gid) == 1 && lt_find(L, 0) < 0 && lt_find(L, 77) < 0);
    // taken: off the list, remembered (a late ADD of it is dropped)
    LtEnt out;
    CHECK(lt_take(L, a.gid, &out) && out.w.gid == a.gid && L->n == 1 && lt_find(L, b.gid) == 0);
    CHECK(!lt_take(L, a.gid, NULL) && lt_tombed(L, a.gid) && lt_add(L, &a, 3000, &at) == LTR_TOMB && L->n == 1);
    CHECK(!lt_take(L, 0x0300abcdu, NULL) && lt_tombed(L, 0x0300abcdu));  // (a TAKE before its ADD: the ADD that comes later is dropped)
    CHECK(!lt_tombed(L, 0));
    // the tombs: a ring of LT_TOMB
    for (uint32_t g = 1; g <= LT_TOMB + 10; g++) lt_tomb(L, 0x07000000u | g);
    CHECK(lt_tombed(L, 0x07000000u | (LT_TOMB + 10)) && !lt_tombed(L, 0x07000001u));
    // full
    memset(L, 0, sizeof *L);
    for (int i = 0; i < LT_LIST_MAX; i++) {
        NetLootAdd c = mkadd(0x01000000u | (uint32_t)(i + 1), 0, NLK_FELL, "Item_Fruit_A", 0, 0, 0);
        CHECK(lt_add(L, &c, 10, NULL) == LTR_NEW);
    }
    NetLootAdd c = mkadd(0x01ffffffu, 0, NLK_FELL, "Item_Fruit_A", 0, 0, 0);
    CHECK(lt_add(L, &c, 10, NULL) == LTR_FULL && L->n == LT_LIST_MAX);
    // expiry: 30 min, no tomb (it stays where it lies), at most max at once
    memset(L, 0, sizeof *L);
    NetLootAdd e1 = mkadd(0x01000001u, 0, NLK_FELL, "Item_A", 0, 0, 0), e2 = mkadd(0x01000002u, 0, NLK_FELL, "Item_B", 0, 0, 0),
               e3 = mkadd(0x01000003u, 0, NLK_FELL, "Item_C", 0, 0, 0);
    lt_add(L, &e1, 1000, NULL), lt_add(L, &e2, 2000, NULL), lt_add(L, &e3, 1000 + LT_LIST_AGE_MS, NULL);
    LtEnt old[2];
    CHECK(lt_expire(L, 1000 + LT_LIST_AGE_MS, old, 2) == 0);  // (exactly 30 min: not yet)
    CHECK(lt_expire(L, 2001 + LT_LIST_AGE_MS, old, 1) == 1 && L->n == 2);
    CHECK(lt_expire(L, 2001 + LT_LIST_AGE_MS, old, 2) == 1 && L->n == 1 && L->e[0].w.gid == e3.gid && !lt_tombed(L, e1.gid));
    // the tie lookup: a drop of src from a LOWER id, on the list in the last LT_TIE_MS
    memset(L, 0, sizeof *L);
    NetLootAdd t1 = mkadd(0x01000001u, 0xabc, NLK_DROP, "Item_Enemy_00", 0, 0, 0), t2 = mkadd(0x03000001u, 0xdef, NLK_DROP, "Item_Enemy_00", 0, 0, 0),
               t3 = mkadd(0x01000002u, 0x777, NLK_FELL, "Item_Enemy_00", 0, 0, 0);
    lt_add(L, &t1, 10000, NULL), lt_add(L, &t2, 10000, NULL), lt_add(L, &t3, 10000, NULL);
    CHECK(lt_has_src_below(L, 0xabc, 2, 10000) && lt_has_src_below(L, 0xabc, 2, 10000 + LT_TIE_MS));
    CHECK(!lt_has_src_below(L, 0xabc, 2, 10001 + LT_TIE_MS));  // (an older drop of it: a later life of the enemy, not a tie)
    CHECK(!lt_has_src_below(L, 0xabc, 1, 10000) && !lt_has_src_below(L, 0xdef, 2, 10000) && !lt_has_src_below(L, 0x777, 2, 10000));
    CHECK(!lt_has_src_below(L, 0, 9, 10000) && !lt_has_src_below(L, 0x999, 9, 10000));
    // a late joiner
    CHECK(lt_join_sends(3, 3, true, false) && lt_join_sends(4, 3, false, true) && !lt_join_sends(4, 3, true, true) && !lt_join_sends(4, 3, false, false));
}

static void test_kind(void) {
    uint64_t now = 100000;
    CHECK(lt_death_kind(0, now, now, 2, true, (int64_t)now) == LTD_OWN);  // (spawned: this game's only)
    CHECK(lt_death_kind(7, now, now - 100, 1, true, (int64_t)now - 1000) == LTD_OWN);  // (our fresh own kill wins over a friend's packet)
    CHECK(lt_death_kind(7, now, 0, 1, true, (int64_t)now - LT_OLD_KILL_MS) == LTD_OWN);
    CHECK(lt_death_kind(7, now, 0, 1, true, (int64_t)now - LT_OLD_KILL_MS - 1) == LTD_OLD);
    CHECK(lt_death_kind(7, now, 0, 1, false, 0) == LTD_OLD);  // (an own kill of unknown age: a re-kill)
    CHECK(lt_death_kind(7, now, now - LT_REMOTE_KILL_MS, 0, false, 0) == LTD_REMOTE);
    CHECK(lt_death_kind(7, now, now - LT_REMOTE_KILL_MS - 1, 0, false, 0) == LTD_OWN);
    CHECK(lt_death_kind(7, now, now + 50, 0, false, 0) == LTD_OWN);  // (a peek "from the future": not counted)
    CHECK(lt_death_kind(7, now, 0, 2, true, (int64_t)now) == LTD_REMOTE && lt_death_kind(7, now, 0, 0, false, 0) == LTD_OWN);
    CHECK(lt_death_kind(7, now, now - 10, 1, true, (int64_t)now - 9000) == LTD_REMOTE);  // (old own kill, a friend's fresh one: his)
    CHECK(lt_tie(2, 1, true) == LTT_RETRACT_OURS && lt_tie(1, 2, true) == LTT_DELAY_THEIRS && lt_tie(1, 2, false) == LTT_NONE && lt_tie(2, 1, false) == LTT_NONE);
}

static void test_deaths(void) {
    LtDeath d[4];
    memset(d, 0, sizeof d);
    float p0[3] = {0, 0, 0}, p1[3] = {100, 0, 0};
    uint64_t t = 50000;
    int a = lt_death_start(d, 4, p0, 0xa0, 0x111, LTD_OWN, t), b = lt_death_start(d, 4, p1, 0xb0, 0x222, LTD_REMOTE, t + 10);
    CHECK(a == 0 && b == 1 && d[0].start == t && d[0].until == t + LT_DEATH_MS && d[1].kind == LTD_REMOTE && d[0].actor == 0xa0);
    // the first-seen rule: an item first seen before the death began is not its drop; after: it is
    float q[3] = {3, 0, 0};
    CHECK(lt_death_match(d, 4, q, 1, t, t + 100) == 0 && lt_death_match(d, 4, q, 1, t - 1, t + 100) < 0);
    lt_death_held_add(&d[0], 2);  // (what it held: its own, first seen long before)
    CHECK(lt_death_match(d, 4, q, 2, 0, t + 100) == 0 && lt_death_is_held(&d[0], 2) && !lt_death_is_held(&d[0], 1));
    lt_death_before(&d[0], 3);  // (what lay there in the look before: never)
    CHECK(lt_death_match(d, 4, q, 3, t + 50, t + 100) < 0);
    float far[3] = {9, 0, 0};
    CHECK(lt_death_match(d, 4, far, 1, t + 50, t + 100) < 0);  // (beyond LT_DEATH_R)
    CHECK(lt_death_match(d, 4, q, 1, t + 50, t + LT_DEATH_MS) < 0);  // (the watch ended)
    // the nearest one; what a watch's enemy held wins over a nearer one
    int c = lt_death_start(d, 4, (float[3]){6, 0, 0}, 0xc0, 0x333, LTD_REMOTE, t + 20);
    float q2[3] = {4, 0, 0};
    CHECK(c == 2 && lt_death_match(d, 4, q2, 9, t + 50, t + 100) == 2 && lt_death_match(d, 4, q, 9, t + 50, t + 100) == 0);
    lt_death_held_add(&d[0], 10);
    CHECK(lt_death_match(d, 4, q2, 10, t + 50, t + 100) == 0);
    // its watch follows the body (while it runs)
    lt_death_follow(d, 4, 0xc0, (float[3]){50, 0, 0}, t + 100);
    CHECK(d[2].pos[0] == 50 && lt_death_match(d, 4, q2, 9, t + 50, t + 100) == 0 && lt_death_match(d, 4, (float[3]){52, 0, 0}, 9, t + 50, t + 100) == 2);
    lt_death_follow(d, 4, 0xc0, (float[3]){70, 0, 0}, t + 20 + LT_DEATH_MS);
    CHECK(d[2].pos[0] == 50);  // (ended: not moved)
    lt_death_follow(d, 4, 0, (float[3]){70, 0, 0}, t + 100);
    CHECK(d[0].pos[0] == 0);  // (actor 0: nothing)
    // our own watch of a HashId
    CHECK(lt_death_own_until(d, 4, 0x111, t + 100) == t + LT_DEATH_MS && lt_death_own_until(d, 4, 0x222, t + 100) == 0);
    CHECK(lt_death_own_until(d, 4, 0x111, t + LT_DEATH_MS) == 0 && lt_death_own_until(d, 4, 0, t) == 0);
    // all busy: the one that ends first goes
    lt_death_start(d, 4, p0, 0xd0, 0x444, LTD_OWN, t + 30);
    int e = lt_death_start(d, 4, p0, 0xe0, 0x555, LTD_OLD, t + 40);
    CHECK(e == 0 && d[0].hid == 0x555 && d[0].nheld == 0 && d[0].nbefore == 0);
    // a slot whose watch ended is used first
    int f = lt_death_start(d, 4, p0, 0xf0, 0x666, LTD_OWN, t + 20 + LT_DEATH_MS);
    CHECK(f == 1 || f == 2);
    // the before / held lists are bounded
    LtDeath one;
    memset(&one, 0, sizeof one);
    for (uint32_t i = 0; i < LT_BEFORE + 5; i++) lt_death_before(&one, i + 1);
    for (uint32_t i = 0; i < LT_HELD + 5; i++) lt_death_held_add(&one, 1000 + i);
    CHECK(one.nbefore == LT_BEFORE && one.nheld == LT_HELD);
}

static void test_seen(void) {
    LtSeen s[4];
    memset(s, 0, sizeof s);
    float p[3] = {1, 1, 1};
    int k = lt_seen_add(s, 4, 42, p, true, 1000, 1000);
    CHECK(k == 0 && lt_seen_find(s, 4, 42) == 0 && lt_seen_find(s, 4, 43) < 0 && s[0].first == 1000 && s[0].st == LTS_WATCH);
    CHECK(!lt_seen_step(&s[0], p, false, 1000 + LT_SETTLE_MS - 1));
    CHECK(lt_seen_step(&s[0], p, false, 1000 + LT_SETTLE_MS) && s[0].st == LTS_MINED);
    CHECK(!lt_seen_step(&s[0], p, false, 9000));  // (once)
    k = lt_seen_add(s, 4, 50, p, true, 2000, 2000);
    float moved[3] = {1.1f, 1, 1};
    CHECK(!lt_seen_step(&s[k], moved, false, 2000 + LT_SETTLE_MS) && s[k].t == 2000 + LT_SETTLE_MS);  // (moved: the clock restarts)
    CHECK(!lt_seen_step(&s[k], moved, true, 2000 + 2 * LT_SETTLE_MS));  // (held: restarts)
    CHECK(!lt_seen_step(&s[k], moved, false, 2000 + 3 * LT_SETTLE_MS - 1) && lt_seen_step(&s[k], moved, false, 2000 + 3 * LT_SETTLE_MS));
    float tiny[3] = {1.1f + 0.01f, 1, 1};
    int m = lt_seen_add(s, 4, 60, moved, true, 3000, 3000);
    CHECK(!lt_seen_step(&s[m], tiny, false, 3000 + LT_SETTLE_MS - 1) && lt_seen_step(&s[m], tiny, false, 3000 + LT_SETTLE_MS));  // (1 cm: still)
    int o = lt_seen_add(s, 4, 70, p, false, 0, 4000);
    CHECK(!lt_seen_step(&s[o], p, false, 99999) && s[o].first == 0);  // (old: never ours)
    // full: the one seen least lately goes
    s[0].last = 10, s[1].last = 5000, s[2].last = 6000, s[3].last = 7000;
    int r = lt_seen_add(s, 4, 80, p, true, 8000, 8000);
    CHECK(r == 0 && lt_seen_find(s, 4, 42) < 0 && lt_seen_find(s, 4, 80) == 0);
    s[1].st = LTS_JUDGED;
    CHECK(lt_seen_find(s, 4, 50) == 1 && !lt_seen_step(&s[1], moved, false, 99999));
    s[1].st = LTS_FREE;
    CHECK(lt_seen_find(s, 4, 50) < 0);
}

static void test_ring_pick(void) {
    LtRing r;
    memset(&r, 0, sizeof r);
    CHECK(lt_ring_find(&r, 5) < 0 && lt_ring_add(&r, 5, false) == 0 && lt_ring_find(&r, 5) == 0 && !r.thrown[0]);
    CHECK(lt_ring_add(&r, 5, true) == 0 && r.thrown[0] && r.n == 1);  // (the same one: thrown now)
    for (uint32_t i = 0; i < LT_RING; i++) lt_ring_add(&r, 100 + i, false);
    CHECK(lt_ring_find(&r, 5) < 0 && lt_ring_find(&r, 100 + LT_RING - 1) >= 0 && lt_ring_find(&r, 0) < 0);
    float link[3] = {10, 5, 10};
    CHECK(lt_at_link((float[3]){10.5f, 5, 11}, link) && lt_at_link((float[3]){10, 4.1f, 10}, link) && lt_at_link((float[3]){10, 7.4f, 10}, link));
    CHECK(!lt_at_link((float[3]){11.2f, 5, 11.2f}, link) && !lt_at_link((float[3]){10, 3.9f, 10}, link) && !lt_at_link((float[3]){10, 7.6f, 10}, link));
    CHECK(lt_pick(true, true, 1e6f) == LTP_PICKED && lt_pick(true, false, 0) == LTP_THERE && lt_pick(false, false, 3.9f * 3.9f) == LTP_PICKED);
    CHECK(lt_pick(false, false, 4.1f * 4.1f) == LTP_GONE);
    CHECK(lt_map_picked(true, true, 1e6f, true) && !lt_map_picked(true, false, 0, false) && lt_map_picked(false, false, 3.4f * 3.4f, false));
    CHECK(!lt_map_picked(false, false, 3.4f * 3.4f, true) && !lt_map_picked(false, false, 3.6f * 3.6f, false));
}

static LtMapList g_m;
static void test_map_hush(void) {
    LtMapList *m = &g_m;
    memset(m, 0, sizeof *m);
    CHECK(lt_map_add(m, 0, 1, "Item_A") == -1 && lt_map_add(m, 77, 1, "Item_Fruit_A") == 1 && lt_map_add(m, 77, 2, "Item_X") == 0);
    CHECK(lt_map_find(m, 77) == 0 && m->origin[0] == 1 && !strcmp(m->name[0], "Item_Fruit_A") && lt_map_find(m, 78) < 0 && lt_map_find(m, 0) < 0);
    CHECK(lt_map_add(m, 79, 3, NULL) == 1 && m->name[1][0] == 0);
    for (uint32_t i = 0; i < LT_MAP_MAX; i++) lt_map_add(m, 1000 + i, 1, "Item_A");
    CHECK(m->n == LT_MAP_MAX && lt_map_add(m, 5, 1, "Item_A") == -1);
    LtHush h[3];
    memset(h, 0, sizeof h);
    float a[3] = {0, 0, 0};
    lt_hush_add(h, 3, a, 1000);
    CHECK(lt_hushed(h, 3, (float[3]){4.9f, 0, 0}, 1000) && !lt_hushed(h, 3, (float[3]){5.1f, 0, 0}, 1000));
    CHECK(lt_hushed(h, 3, a, 1000 + LT_HUSH_MS - 1) && !lt_hushed(h, 3, a, 1000 + LT_HUSH_MS));
    lt_hush_add(h, 3, (float[3]){50, 0, 0}, 2000), lt_hush_add(h, 3, (float[3]){90, 0, 0}, 3000), lt_hush_add(h, 3, (float[3]){200, 0, 0}, 4000);
    CHECK(!lt_hushed(h, 3, a, 4000) && lt_hushed(h, 3, (float[3]){200, 0, 0}, 4000) && lt_hushed(h, 3, (float[3]){50, 0, 0}, 4000));  // (the earliest went)
    // (s289 loot + objs) our own break: the "old" items first seen near it from LT_BROKE_BACK_MS before are watched now
    LtSeen s[6];
    memset(s, 0, sizeof s);
    float brk[3] = {10, 0, 10};
    int r0 = lt_seen_add(s, 6, 1, (float[3]){11, 0, 10}, false, 9000, 9000);     // a rupee seen 1 s before the break, 1 m off: taken up
    int r1 = lt_seen_add(s, 6, 2, (float[3]){10, 0, 16}, false, 9500, 9500);     // 6 m off: not
    int r2 = lt_seen_add(s, 6, 3, (float[3]){10, 0, 11}, false, 8000, 8000);     // seen 2 s before: lay there already, not
    int r3 = lt_seen_add(s, 6, 4, (float[3]){10, 0, 10}, false, 0, 9900);        // there before our first look (first 0): not
    int r4 = lt_seen_add(s, 6, 5, (float[3]){10, 1, 10}, true, 9800, 9800);      // watched already: left as it is (its clock too)
    int r5 = lt_seen_add(s, 6, 6, (float[3]){10, 0, 9}, false, 9700, 9700);
    s[r5].st = LTS_JUDGED;                                                       // a death watch took it: not
    CHECK(lt_seen_broke(s, 6, brk, 10000) == 1);
    CHECK(s[r0].st == LTS_WATCH && s[r0].t == 10000 && s[r1].st == LTS_OLD && s[r2].st == LTS_OLD && s[r3].st == LTS_OLD);
    CHECK(s[r4].st == LTS_WATCH && s[r4].t == 9800 && s[r5].st == LTS_JUDGED);
    CHECK(!lt_seen_step(&s[r0], (float[3]){11, 0, 10}, false, 10000 + LT_SETTLE_MS - 1) && lt_seen_step(&s[r0], (float[3]){11, 0, 10}, false, 10000 + LT_SETTLE_MS));
    CHECK(s[r0].st == LTS_MINED && lt_seen_broke(s, 6, brk, 10450) == 0 && s[r0].st == LTS_MINED);  // (a 2nd break in its window: ours
                                                                                                      //  already, never shared twice)
    CHECK(lt_seen_broke(s, 6, brk, 9000 + LT_BROKE_BACK_MS + 1) == 0 && s[r0].st == LTS_MINED);  // (past its window: not again)
    int r6 = lt_seen_add(s, 6, 7, (float[3]){10, 0, 10}, false, 20000, 20000);
    CHECK(lt_seen_broke(s, 6, brk, 20000 + LT_BROKE_BACK_MS) == 1 && s[r6].st == LTS_WATCH);       // (the edge: in)
    s[r6].st = LTS_OLD;
    CHECK(lt_seen_broke(s, 6, brk, 20000 + LT_BROKE_BACK_MS + 1) == 0 && s[r6].st == LTS_OLD);   // (past it: out)
    CHECK(lt_seen_broke(s, 6, brk, 19999) == 0);  // (a first after now (a clock step back): never)
    // (s289 loot + objs fix 2) a friend's pushed one broke here while it followed his: what was first seen near it lately, not ours on
    // the list yet, is marked to go (old and watched ones); ours already, a death watch's, older, further, before our first look: not
    LtSeen f[8];
    memset(f, 0, sizeof f);
    int f0 = lt_seen_add(f, 8, 1, (float[3]){11, 0, 10}, false, 9000, 9000);   // a rupee 1 s before, 1 m off: goes
    int f1 = lt_seen_add(f, 8, 2, (float[3]){10, 0, 12}, true, 9800, 9800);    // watched (not settled): goes
    int f2 = lt_seen_add(f, 8, 3, (float[3]){10, 0, 11}, true, 9500, 9500);
    f[f2].st = LTS_MINED;                                                        // ours on the list already: stays (a limitation)
    int f3 = lt_seen_add(f, 8, 4, (float[3]){10, 0, 9}, false, 9700, 9700);
    f[f3].st = LTS_JUDGED;                                                       // a death watch's: stays
    int f4 = lt_seen_add(f, 8, 5, (float[3]){10, 0, 16}, false, 9900, 9900);   // 6 m off: stays
    int f5 = lt_seen_add(f, 8, 6, (float[3]){10, 0, 10}, false, 8000, 8000);   // 2 s before: lay there already, stays
    int f6 = lt_seen_add(f, 8, 7, (float[3]){10, 0, 10}, false, 0, 9900);      // before our first look: stays
    CHECK(lt_seen_hush_back(f, 8, brk, 10000) == 2 && f[f0].st == LTS_HUSH && f[f1].st == LTS_HUSH);
    CHECK(f[f2].st == LTS_MINED && f[f3].st == LTS_JUDGED && f[f4].st == LTS_OLD && f[f5].st == LTS_OLD && f[f6].st == LTS_OLD);
    CHECK(!lt_seen_step(&f[f1], (float[3]){10, 0, 12}, false, 99999) && f[f1].st == LTS_HUSH);  // (marked: never ours)
    CHECK(lt_seen_broke(f, 8, brk, 10000) == 0 && f[f0].st == LTS_HUSH);                       // (and not ours by a break of ours)
    CHECK(lt_seen_hush_back(f, 8, brk, 10000) == 0 && lt_seen_hush_back(f, 8, brk, 7999) == 0 && f[f5].st == LTS_OLD);  // (once; first after
                                                                                                                         //  now (a clock step back): never)
}

static void test_mtx(void) {
    float id[12] = {1, 0, 0, 7, 0, 1, 0, 8, 0, 0, 1, 9};
    NetLootAdd a = mkadd(1, 0, NLK_FELL, "Item_A", 7, 8, 9);
    lt_rot_pack(id, a.rot);
    CHECK(a.rot[0] == 32767 && a.rot[4] == 32767 && a.rot[8] == 32767 && a.rot[1] == 0 && a.rot[3] == 0);
    float m[12];
    lt_place_mtx(&a, m);
    CHECK(fabsf(m[0] - 1) < 1e-4f && fabsf(m[5] - 1) < 1e-4f && m[1] == 0 && m[3] == 7 && fabsf(m[7] - 8.02f) < 1e-4f && m[11] == 9);
    float odd[12] = {NAN, 2.0f, -3.0f, 0, 0.5f, -0.5f, INFINITY, 0, 0, 0, -1, 0};
    lt_rot_pack(odd, a.rot);
    CHECK(a.rot[0] == 0 && a.rot[1] == 32767 && a.rot[2] == -32767 && a.rot[3] == 16384 && a.rot[4] == -16384 && a.rot[5] == 0 && a.rot[8] == -32767);
    NetLootAdd u = mkadd(1, 0, NLK_FELL, "Item_A", 1, 2, 3);  // (no rotation sent: upright, 20 cm above)
    lt_place_mtx(&u, m);
    CHECK(m[0] == 1 && m[5] == 1 && m[10] == 1 && m[1] == 0 && m[3] == 1 && fabsf(m[7] - 2.2f) < 1e-5f && m[11] == 3);
}

// ---- the GameData lookup over fake memory (addresses = host pointers into one arena) ----
static uint8_t g_mem[0x10000];
static uint64_t A(uint32_t off) { return (uint64_t)(uintptr_t)(g_mem + off); }
static uint64_t g_bad;  // (an address the fake read refuses)
static bool f_rd(uint64_t a, void *out, size_t n) {
    if (a == g_bad || a < A(0) || a + n > A(sizeof g_mem)) return false;
    memcpy(out, (const void *)(uintptr_t)a, n);
    return true;
}
static void put32(uint32_t off, uint32_t v) { memcpy(g_mem + off, &v, 4); }
static void put64(uint32_t off, uint64_t v) { memcpy(g_mem + off, &v, 8); }
static void test_gdt(void) {
    CHECK(lt_crc32("123456789") == 0xCBF43926u && lt_crc32("") == 0);
    char f[128];
    CHECK(lt_flag_name(f, sizeof f, "MainField", "Item_Fruit_A", 1234567u) > 0 && !strcmp(f, "MainField_Item_Fruit_A_1234567"));
    CHECK(!strcmp(LT_FLAG_MAPS[0], "MainField") && !strcmp(LT_FLAG_MAPS[3], "CDungeon"));
    memset(g_mem, 0, sizeof g_mem);
    const uint32_t hs[5] = {10, 20, 30, 40, 0xfffffff0u};
    put32(0x8, 5), put64(0x10, A(0x100));  // (buf: +8 the count, +0x10 the array of pointers)
    for (uint32_t i = 0; i < 5; i++) {
        uint32_t pf = 0x1000 + i * 0x18, def = 0x4000 + i * 0x20;
        put64(0x100 + 8 * i, A(pf));
        put64(pf + 8, A(def)), put32(pf + 0x10, i == 2 ? 1 : 0);  // (the proxy: +8 its definition, +0x10 the value)
        put32(def + 0xc, hs[i]);
    }
    CHECK(lt_gdt_find(f_rd, A(0), 30) == A(0x1000 + 2 * 0x18 + 0x10) && lt_gdt_find(f_rd, A(0), 10) == A(0x1000 + 0x10));
    CHECK(lt_gdt_find(f_rd, A(0), 0xfffffff0u) == A(0x1000 + 4 * 0x18 + 0x10) && lt_gdt_find(f_rd, A(0), 40) == A(0x1000 + 3 * 0x18 + 0x10));
    CHECK(!lt_gdt_find(f_rd, A(0), 25) && !lt_gdt_find(f_rd, A(0), 5) && !lt_gdt_find(f_rd, A(0), 0xffffffffu));
    CHECK(!lt_gdt_find(f_rd, 0, 30) && !lt_gdt_find(NULL, A(0), 30));
    put32(0x8, 0);
    CHECK(!lt_gdt_find(f_rd, A(0), 30));
    put32(0x8, 200001);
    CHECK(!lt_gdt_find(f_rd, A(0), 30));
    put32(0x8, 5), put64(0x10, 0);
    CHECK(!lt_gdt_find(f_rd, A(0), 30));
    put64(0x10, A(0x100));
    g_bad = A(0x4000 + 2 * 0x20 + 0xc);  // (one definition unreadable: none, not a wrong flag)
    CHECK(!lt_gdt_find(f_rd, A(0), 30));
    g_bad = 0;
    CHECK(lt_gdt_find(f_rd, A(0), 30) != 0);
}

// ---- the wire ----
static void test_wire(void) {
    uint8_t b[NET_PKT_MAX + 64], pl = 0;
    uint16_t sq = 0;
    static NetLootMsg m;
    CHECK(NET_LOOT == 13 && NET_WORLD == 14 && NET_LOOT_PKT == 705 && NET_LOOT_PKT <= NET_PKT_MAX);
    NetLootAdd a = mkadd(0x02000005u, 0xdeadbeefu, NLK_THROWN, "Weapon_Sword_001", 1.5f, -2.25f, 1000.0f);
    for (int j = 0; j < 9; j++) a.rot[j] = (int16_t)(j * 1000 - 4000);
    int n = net_pack_loot_add(2, 77, &a, 1, b, (int)sizeof b);
    CHECK(n == NET_LOOT_HEAD + NET_LOOT_ADD_FIX + 16);
    int t = 0;
    CHECK(net_unpack_ex(b, n, &t, NULL, NULL, NULL, NULL, NULL, NULL) && t == NET_LOOT);
    CHECK(net_unpack_loot(b, n, &pl, &sq, &m) && pl == 2 && sq == 77 && m.op == NLO_ADD && m.n == 1);
    CHECK(m.add[0].gid == a.gid && m.add[0].src == a.src && m.add[0].kind == NLK_THROWN && !strcmp(m.add[0].name, "Weapon_Sword_001"));
    CHECK(m.add[0].pos[0] == 1.5f && m.add[0].pos[1] == -2.25f && m.add[0].pos[2] == 1000.0f && m.add[0].rot[0] == -4000 && m.add[0].rot[8] == 4000);
    // 8 entries of 47 letters: the biggest packet
    NetLootAdd eight[NET_LOOT_ADD_MAX];
    char nm47[48];
    memset(nm47, 'A', 47), nm47[47] = 0, memcpy(nm47, "Item_", 5);
    for (int i = 0; i < NET_LOOT_ADD_MAX; i++) eight[i] = mkadd(0x01000000u | (uint32_t)(i + 1), (uint32_t)i, NLK_DROP, nm47, (float)i, 0, 0);
    n = net_pack_loot_add(1, 1, eight, NET_LOOT_ADD_MAX, b, (int)sizeof b);
    CHECK(n == NET_LOOT_PKT && net_unpack_loot(b, n, &pl, &sq, &m) && m.n == NET_LOOT_ADD_MAX && !strcmp(m.add[7].name, nm47) && m.add[7].pos[0] == 7);
    CHECK(net_pack_loot_add(1, 1, eight, NET_LOOT_ADD_MAX, b, NET_LOOT_PKT - 1) == -1);  // (cap too small)
    // refusals
    NetLootAdd bad = a;
    CHECK(net_pack_loot_add(1, 1, &a, 0, b, (int)sizeof b) == -1 && net_pack_loot_add(1, 1, eight, NET_LOOT_ADD_MAX + 1, b, (int)sizeof b) == -1);
    bad.gid = 0;
    CHECK(net_pack_loot_add(1, 1, &bad, 1, b, (int)sizeof b) == -1);
    bad = a, bad.name[0] = 0;
    CHECK(net_pack_loot_add(1, 1, &bad, 1, b, (int)sizeof b) == -1);
    bad = a, snprintf(bad.name, sizeof bad.name, "Item Enemy");  // (a space: not a name)
    CHECK(net_pack_loot_add(1, 1, &bad, 1, b, (int)sizeof b) == -1);
    bad = a, memset(bad.name, 'B', 48);  // (48 letters, no end: too long)
    CHECK(net_loot_namelen(bad.name) == 48 && net_pack_loot_add(1, 1, &bad, 1, b, (int)sizeof b) == -1);
    bad = a, bad.pos[1] = NAN;
    CHECK(net_pack_loot_add(1, 1, &bad, 1, b, (int)sizeof b) == -1);
    bad = a, bad.pos[2] = INFINITY;
    CHECK(net_pack_loot_add(1, 1, &bad, 1, b, (int)sizeof b) == -1);
    CHECK(net_pack_loot_add(1, 1, NULL, 1, b, (int)sizeof b) == -1 && net_pack_loot_add(1, 1, &a, 1, NULL, 100) == -1);
    // a kind out of range is packed as given and refused when read
    bad = a, bad.kind = 0;
    n = net_pack_loot_add(1, 1, &bad, 1, b, (int)sizeof b);
    CHECK(n > 0 && !net_unpack_loot(b, n, &pl, &sq, &m));
    bad.kind = 4;
    n = net_pack_loot_add(1, 1, &bad, 1, b, (int)sizeof b);
    CHECK(n > 0 && !net_unpack_loot(b, n, &pl, &sq, &m));
    // TAKE
    uint32_t g[NET_LOOT_TAKE_MAX + 1];
    for (int i = 0; i <= NET_LOOT_TAKE_MAX; i++) g[i] = 0x03000000u | (uint32_t)(i + 1);
    n = net_pack_loot_take(3, 9, g, 1, b, (int)sizeof b);
    CHECK(n == NET_LOOT_HEAD + 4 && net_unpack_loot(b, n, &pl, &sq, &m) && m.op == NLO_TAKE && m.n == 1 && m.gid[0] == g[0] && pl == 3 && sq == 9);
    n = net_pack_loot_take(3, 9, g, NET_LOOT_TAKE_MAX, b, (int)sizeof b);
    CHECK(n == NET_LOOT_HEAD + 4 * NET_LOOT_TAKE_MAX && net_unpack_loot(b, n, &pl, &sq, &m) && m.n == NET_LOOT_TAKE_MAX && m.gid[63] == g[63]);
    CHECK(net_pack_loot_take(3, 9, g, 0, b, (int)sizeof b) == -1 && net_pack_loot_take(3, 9, g, NET_LOOT_TAKE_MAX + 1, b, (int)sizeof b) == -1);
    g[1] = 0;
    CHECK(net_pack_loot_take(3, 9, g, 2, b, (int)sizeof b) == -1 && net_pack_loot_take(3, 9, g, 1, b, NET_LOOT_HEAD + 3) == -1);
    n = net_pack_loot_take(3, 9, g, 1, b, (int)sizeof b);
    memset(b + NET_LOOT_HEAD, 0, 4);  // (a gid 0 on the wire)
    CHECK(n > 0 && !net_unpack_loot(b, n, &pl, &sq, &m));
    // MAPTAKE
    NetLootMap mp[NET_LOOT_MAP_MAX + 1];
    for (int i = 0; i <= NET_LOOT_MAP_MAX; i++) mp[i].hid = 1000u + (uint32_t)i, snprintf(mp[i].name, sizeof mp[i].name, "%s", nm47);
    snprintf(mp[0].name, sizeof mp[0].name, "Item_Mushroom_E");
    n = net_pack_loot_map(4, 5, mp, 1, b, (int)sizeof b);
    CHECK(n == NET_LOOT_HEAD + NET_LOOT_MAP_FIX + 15 && net_unpack_loot(b, n, &pl, &sq, &m) && m.op == NLO_MAPTAKE && m.map[0].hid == 1000 &&
          !strcmp(m.map[0].name, "Item_Mushroom_E"));
    n = net_pack_loot_map(4, 5, mp, NET_LOOT_MAP_MAX, b, (int)sizeof b);
    CHECK(n > 0 && n <= NET_PKT_MAX && net_unpack_loot(b, n, &pl, &sq, &m) && m.n == NET_LOOT_MAP_MAX && !strcmp(m.map[11].name, nm47));
    CHECK(net_pack_loot_map(4, 5, mp, 0, b, (int)sizeof b) == -1 && net_pack_loot_map(4, 5, mp, NET_LOOT_MAP_MAX + 1, b, (int)sizeof b) == -1);
    NetLootMap mb = mp[0];
    mb.hid = 0;
    CHECK(net_pack_loot_map(4, 5, &mb, 1, b, (int)sizeof b) == -1);
    mb = mp[0], mb.name[0] = 0;
    CHECK(net_pack_loot_map(4, 5, &mb, 1, b, (int)sizeof b) == -1);
    mb = mp[0], mb.name[4] = '/';
    CHECK(net_pack_loot_map(4, 5, &mb, 1, b, (int)sizeof b) == -1);
    n = net_pack_loot_map(4, 5, mp, 1, b, (int)sizeof b);
    memset(b + NET_LOOT_HEAD, 0, 4);  // (a HashId 0 on the wire)
    CHECK(n > 0 && !net_unpack_loot(b, n, &pl, &sq, &m));
}

// malformed packets: every one refused by net_unpack_ex (so sora_net.h counts it "bad") and by net_unpack_loot
static bool refused(const uint8_t *p, int n) {
    static NetLootMsg m;
    int t = 0;
    return !net_unpack_ex(p, n, &t, NULL, NULL, NULL, NULL, NULL, NULL) && !net_unpack_loot(p, n, NULL, NULL, &m);
}
static void test_malformed(void) {
    uint8_t b[NET_PKT_MAX + 64], c[NET_PKT_MAX + 64];
    NetLootAdd a = mkadd(0x02000005u, 7, NLK_DROP, "Item_Enemy_00", 1, 2, 3);
    int n = net_pack_loot_add(2, 1, &a, 1, b, (int)sizeof b);
    CHECK(n > 0 && !refused(b, n));
    memcpy(c, b, (size_t)n), c[NET_HDR] = 0;  // (op 0)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_HDR] = 5;  // (op 5: no such op)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_HDR] = NLO_STATE;  // (an ADD's body under the STATE op: the length is wrong)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_HDR + 1] = 0;  // (count 0)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_HDR + 1] = 2;  // (count 2, one entry)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_HDR + 1] = NET_LOOT_ADD_MAX + 1;
    CHECK(refused(c, n));
    CHECK(refused(b, n - 1) && refused(b, NET_HDR + 1) && refused(b, NET_HDR));  // (cut short)
    memcpy(c, b, (size_t)n), c[n] = 0;  // (one byte too many)
    CHECK(refused(c, n + 1));
    memcpy(c, b, (size_t)n), c[NET_LOOT_HEAD + 9] = 0;  // (a name of 0 letters)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_LOOT_HEAD + 9] = NET_LOOT_NAME_MAX + 1;
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_LOOT_HEAD + 9] = 14;  // (the length one longer than the letters: the size no longer adds up)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_LOOT_HEAD + 10 + 4] = ' ';  // (a space in the name)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_LOOT_HEAD + 10 + 4] = 0;  // (a NUL in the name)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[2] ^= 1;  // (another protocol version)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[0] = 'X';  // (not our magic)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[3] = NET_WORLD;  // (s289: NET_WORLD 14 is only reserved: refused, whatever its body)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[3] = 15;  // (another helper's type, not this build's)
    int t = 0;
    CHECK(!net_unpack_ex(c, n, &t, NULL, NULL, NULL, NULL, NULL, NULL));
    // a NaN position on the wire: the header passes, the entry is refused
    memcpy(c, b, (size_t)n);
    float qn = NAN;
    uint32_t bits;
    memcpy(&bits, &qn, 4);
    net_w32(c + NET_LOOT_HEAD + 10 + 13, bits);
    static NetLootMsg m;
    CHECK(!net_unpack_loot(c, n, NULL, NULL, &m));
    // TAKE: count / size
    uint32_t g[2] = {0x01000001u, 0x01000002u};
    n = net_pack_loot_take(1, 1, g, 2, b, (int)sizeof b);
    CHECK(n > 0 && !refused(b, n) && refused(b, n - 1));
    memcpy(c, b, (size_t)n), c[NET_HDR + 1] = 3;
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_HDR + 1] = NET_LOOT_TAKE_MAX + 1;
    CHECK(refused(c, n));
    // MAPTAKE: name length / letters
    NetLootMap mp = {5000, "Item_Fruit_A"};
    n = net_pack_loot_map(1, 1, &mp, 1, b, (int)sizeof b);
    CHECK(n > 0 && !refused(b, n));
    memcpy(c, b, (size_t)n), c[NET_LOOT_HEAD + 4] = 0;
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_LOOT_HEAD + 4] = 13;
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_LOOT_HEAD + 5] = '#';
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_HDR + 1] = NET_LOOT_MAP_MAX + 1;
    CHECK(refused(c, n));
    // net_unpack_loot on something else (a SNAP-sized junk / NULLs)
    static NetLootMsg m2;
    CHECK(!net_unpack_loot(NULL, 20, NULL, NULL, &m2) && !net_unpack_loot(b, n, NULL, NULL, NULL));
}

// two games through the real wire: an item out, a copy on the other list, picked up there, gone here; a late ADD stays gone
static LtList g_la, g_lb;
// (NLO_STATE) the friend-shares state, and the kill rule it feeds
static void test_state(void) {
    uint8_t b[64], c[64], pl = 0;
    uint16_t sq = 0;
    static NetLootMsg m;
    for (int on = 0; on < 2; on++)
        for (int ask = 0; ask < 2; ask++) {
            int n = net_pack_loot_state(3, (uint16_t)(10 + on * 2 + ask), on != 0, ask != 0, b, (int)sizeof b);
            CHECK(n == NET_LOOT_HEAD + 1 && !refused(b, n));
            CHECK(net_unpack_loot(b, n, &pl, &sq, &m) && pl == 3 && sq == 10 + on * 2 + ask && m.op == NLO_STATE && m.n == 1 && m.on == (on != 0) &&
                  m.ask == (ask != 0));
        }
    int n = net_pack_loot_state(3, 1, true, false, b, (int)sizeof b);
    CHECK(net_pack_loot_state(3, 1, true, false, b, NET_LOOT_HEAD) == -1 && net_pack_loot_state(3, 1, true, false, NULL, 64) == -1);  // (cap, no buffer)
    memcpy(c, b, (size_t)n), c[NET_HDR + 1] = 2;  // (count 2)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_HDR + 1] = 0;  // (count 0)
    CHECK(refused(c, n));
    memcpy(c, b, (size_t)n), c[NET_HDR + 2] = 4;  // (an unknown bit)
    CHECK(refused(c, n));
    CHECK(refused(b, n - 1));  // (cut short)
    memcpy(c, b, (size_t)n), c[n] = 1;  // (one byte too many)
    CHECK(refused(c, n + 1));
    // a friend's kill takes our drops of it away only where the killer's game shares; not known who: any friend sharing
    CHECK(!lt_remote_keeps(true, true, false) && lt_remote_keeps(true, false, true));
    CHECK(!lt_remote_keeps(false, false, true) && lt_remote_keeps(false, false, false));
}

static void test_two_games(void) {
    LtList *A_ = &g_la, *B_ = &g_lb;
    memset(A_, 0, sizeof *A_), memset(B_, 0, sizeof *B_);
    uint8_t b[NET_PKT_MAX], pl = 0;
    uint16_t sq = 0;
    static NetLootMsg m;
    uint32_t ca = 100;
    NetLootAdd w = mkadd(lt_gid_next(1, &ca), 0xabcd, NLK_DROP, "Item_Enemy_02", 10, 0, 10);
    CHECK(lt_add(A_, &w, 1000, NULL) == LTR_NEW);
    int n = net_pack_loot_add(1, 1, &w, 1, b, (int)sizeof b);
    CHECK(net_unpack_loot(b, n, &pl, &sq, &m) && lt_add(B_, &m.add[0], 1050, NULL) == LTR_NEW && B_->n == 1 && lt_gid_origin(B_->e[0].w.gid) == 1);
    CHECK(net_unpack_loot(b, n, &pl, &sq, &m) && lt_add(B_, &m.add[0], 1060, NULL) == LTR_DUP);  // (a join's resend: no second copy)
    uint32_t gid = B_->e[0].w.gid;
    CHECK(lt_take(B_, gid, NULL) && B_->n == 0);
    n = net_pack_loot_take(2, 1, &gid, 1, b, (int)sizeof b);
    LtEnt e;
    CHECK(net_unpack_loot(b, n, &pl, &sq, &m) && m.op == NLO_TAKE && lt_take(A_, m.gid[0], &e) && e.w.src == 0xabcd && A_->n == 0);
    n = net_pack_loot_add(1, 2, &w, 1, b, (int)sizeof b);
    CHECK(net_unpack_loot(b, n, &pl, &sq, &m) && lt_add(B_, &m.add[0], 2000, NULL) == LTR_TOMB && lt_add(A_, &m.add[0], 2000, NULL) == LTR_TOMB);
    // the tie: both games announced drops of 0x5555; the lower id (1) keeps them
    NetLootAdd d1 = mkadd(0x01000010u, 0x5555, NLK_DROP, "Item_Enemy_00", 0, 0, 0), d2 = mkadd(0x02000010u, 0x5555, NLK_DROP, "Item_Enemy_00", 0, 0, 0);
    lt_add(A_, &d1, 3000, NULL), lt_add(B_, &d2, 3000, NULL);
    CHECK(lt_tie(2, lt_gid_origin(d1.gid), true) == LTT_RETRACT_OURS && lt_tie(1, lt_gid_origin(d2.gid), true) == LTT_DELAY_THEIRS);
    lt_add(B_, &d1, 3050, NULL);
    CHECK(lt_has_src_below(B_, 0x5555, 2, 3100) && !lt_has_src_below(A_, 0x5555, 1, 3100));
}

int main(void) {
    test_names();
    test_gids();
    test_list();
    test_kind();
    test_deaths();
    test_seen();
    test_ring_pick();
    test_map_hush();
    test_mtx();
    test_gdt();
    test_wire();
    test_malformed();
    test_state();
    test_two_games();
    printf("netloot_test: %d/%d passed\n", runs - fails, runs);
    return fails ? 1 : 0;
}

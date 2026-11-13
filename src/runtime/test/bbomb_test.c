// host tests for runtime/sora_bbomb.h's pure part (s217 world, feature 1: explosive barrels): the table's row pick, the validity
// rule before a life write, the one-hit life, the spell cylinder. Build + run from the repo root (UCRT64 clang):
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/bbomb_test.c -o build_h/bbomb_test.exe && build_h/bbomb_test.exe
#include <stdio.h>
#define BBOMB_HOST  // (the pure part only)
#include "sora_bbomb.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static BbRow row_at(uint64_t actor, uint64_t frame) {
    BbRow r = {.actor = actor, .node = actor + 0x1000, .vt = 0x7001d00000ull, .frame = frame, .gen = 3, .life = 20, .used = 1};
    return r;
}
static BbLive live_ok(const BbRow *r, uint64_t now) {  // everything says yes
    BbLive l = {.now_frame = now, .gen = 3, .read_ok = true, .node_vt = BB_VT, .node_actor = r->actor, .actor_vt = r->vt, .state = 1,
                .name = "BarrelBomb", .life = 20};
    return l;
}

int main(void) {
    // ---- row pick: own row, free row, stalest reusable row, full
    BbRow t[4];
    memset(t, 0, sizeof t);
    CHECK(bb_row_pick(t, 4, 0xA0, 100) == 0);                  // empty table: the first free row
    t[0] = row_at(0xA0, 100);
    CHECK(bb_row_pick(t, 4, 0xA0, 101) == 0);                  // its own row
    CHECK(bb_row_pick(t, 4, 0xB0, 101) == 1);                  // another barrel: the next free row
    t[1] = row_at(0xB0, 101), t[2] = row_at(0xC0, 102), t[3] = row_at(0xD0, 103);
    CHECK(bb_row_pick(t, 4, 0xE0, 104) == -1);                 // full, nobody stale: not recorded
    CHECK(bb_row_pick(t, 4, 0xC0, 104) == 2);                  // full but its own row: still recorded
    CHECK(bb_row_pick(t, 4, 0xE0, 100 + BB_REUSE) == 0);       // row 0 not seen for BB_REUSE frames: reused
    CHECK(bb_row_pick(t, 4, 0xE0, 99 + BB_REUSE) == -1);       // one frame short
    t[2].frame = 50;
    CHECK(bb_row_pick(t, 4, 0xE0, 50 + BB_REUSE + 60) == 2);   // the STALEST row goes first
    t[0].frame = 500;                                          // a frame id from the future (a reboot reset the counter)
    CHECK(bb_row_pick(t, 4, 0xE0, 200) == 2);                  // ... is never "stale enough" to steal; the old row 2 is

    // ---- validity before a write
    BbRow r = row_at(0xA0, 1000);
    BbLive l = live_ok(&r, 1000);
    CHECK(bb_check(&r, &l) == BB_OK);
    l = live_ok(&r, 1000 + BB_FRESH);      CHECK(bb_check(&r, &l) == BB_OK);       // recorded BB_FRESH frames ago: still fresh
    l = live_ok(&r, 1001 + BB_FRESH);      CHECK(bb_check(&r, &l) == BB_STALE);    // exploded / unloaded: no calc lately
    l = live_ok(&r, 999);                  CHECK(bb_check(&r, &l) == BB_STALE);    // row newer than now (counter went back)
    l = live_ok(&r, 1000), l.gen = 4;      CHECK(bb_check(&r, &l) == BB_GEN);      // recorded before a hook off / on
    l = live_ok(&r, 1000), l.read_ok = false; CHECK(bb_check(&r, &l) == BB_UNREAD);
    l = live_ok(&r, 1000), l.node_vt = 0x7001c47640ull; CHECK(bb_check(&r, &l) == BB_NODE);  // node freed / reused
    l = live_ok(&r, 1000), l.node_actor = 0xB0; CHECK(bb_check(&r, &l) == BB_NODE);
    l = live_ok(&r, 1000), l.actor_vt = 0x7001d00008ull; CHECK(bb_check(&r, &l) == BB_VTABLE);  // actor memory reused
    l = live_ok(&r, 1000), l.state = 3;    CHECK(bb_check(&r, &l) == BB_DELETING);
    l = live_ok(&r, 1000), l.name = "Barrel"; CHECK(bb_check(&r, &l) == BB_NAME);  // a plain barrel never
    l = live_ok(&r, 1000), l.name = "";    CHECK(bb_check(&r, &l) == BB_NAME);
    l = live_ok(&r, 1000), l.name = NULL;  CHECK(bb_check(&r, &l) == BB_NAME);
    l = live_ok(&r, 1000), l.name = "BarrelBomb3"; CHECK(bb_check(&r, &l) == BB_OK);
    l = live_ok(&r, 1000), l.name = "BarrelBomb_Balloon"; CHECK(bb_check(&r, &l) == BB_OK);
    l = live_ok(&r, 1000), l.life = 0;     CHECK(bb_check(&r, &l) == BB_SPENT);    // already going off: no second write
    l = live_ok(&r, 1000), l.life = -5;    CHECK(bb_check(&r, &l) == BB_SPENT);
    BbRow u = r; u.used = 0;
    l = live_ok(&u, 1000);                 CHECK(bb_check(&u, &l) == BB_UNUSED);
    CHECK(!strcmp(BB_WHY[BB_SPENT], "life already < 1") && sizeof BB_WHY / sizeof *BB_WHY == BB_SPENT + 1);

    // ---- one hit = life 0 (any life)
    CHECK(bb_hit_life(20) == 0 && bb_hit_life(40) == 0 && bb_hit_life(80) == 0 && bb_hit_life(1) == 0);
    CHECK(bb_hit_life(0) == -1 && bb_hit_life(-1) == -1);

    // ---- spell cylinder (Fire: r, below h, above 2h at Sora's feet; Thunder: r, 2r, 2r at the bolt)
    float c[3] = {10, 5, 10};
    float in1[3] = {11, 5, 10}, edge[3] = {12, 5, 10}, high[3] = {10, 7.1f, 10}, low[3] = {10, 3.9f, 10}, ok_hi[3] = {10, 6.9f, 10};
    CHECK(bb_in_cyl(in1, c, 2, 1, 2));
    CHECK(!bb_in_cyl(edge, c, 2, 1, 2));     // on the radius: out (strict, as brk_spell)
    CHECK(!bb_in_cyl(high, c, 2, 1, 2) && bb_in_cyl(ok_hi, c, 2, 1, 2));
    CHECK(!bb_in_cyl(low, c, 2, 1, 2));
    float diag[3] = {11.4f, 5, 11.4f};       // 1.98 u away on the diagonal: in
    CHECK(bb_in_cyl(diag, c, 2, 1, 2));
    float diag2[3] = {11.5f, 5, 11.5f};      // 2.12 u: out
    CHECK(!bb_in_cyl(diag2, c, 2, 1, 2));

    // ---- the constants the glue relies on
    CHECK(BB_DEFAULT_ON == 1);               // s218: verified by the user -> default on
    CHECK(BB_VT == 0x7001c47500ull && BB_SLOT == 32 && BB_CALC == 0x70007eb98cull);
    CHECK(BB_FRESH < BB_GONE && BB_GONE < BB_REUSE);

    printf("bbomb_test: %d/%d passed\n", runs - fails, runs);
    return fails != 0;
}

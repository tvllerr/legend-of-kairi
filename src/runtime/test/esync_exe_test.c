// host test for runtime/linkpos_esync.h (s168 ESYNC exe side: hash + name table, puppet set, hook table, event ring, AS change counters,
// puppet applied-state, matrix) - no linkpos, no guest memory.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/esync_exe_test.c -o build_h/esync_exe_test.exe
#include <stdio.h>
#include <string.h>
#include "linkpos_esync.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static EsxNameTab NT;
static EsxEvQ Q;

static void test_hash_names(void) {
    memset(&NT, 0, sizeof NT);
    CHECK(esx_hash("") == 2166136261u);                       // FNV-1a offset basis (nonzero)
    CHECK(esx_hash("a") == 0xe40c292cu);                      // FNV-1a 32 of "a"
    CHECK(esx_hash("foobar") == 0xbf9cf968u);                 // FNV-1a 32 of "foobar"
    CHECK(esx_hash("Wait") != esx_hash("wait"));              // case sensitive
    CHECK(esx_hash(NULL) == 2166136261u);
    CHECK(esx_name_put(&NT, "Wait") == 1 && NT.n == 1);
    CHECK(esx_name_put(&NT, "Wait") == 0 && NT.n == 1);       // known
    CHECK(esx_name_put(&NT, "") == -1 && esx_name_put(&NT, NULL) == -1);
    char big[200];
    memset(big, 'x', sizeof big - 1), big[sizeof big - 1] = 0;
    CHECK(esx_name_put(&NT, big) == -1 && NT.toolong == 1);   // 64+ chars refused
    CHECK(!strcmp(esx_name_get(&NT, esx_hash("Wait")), "Wait"));
    CHECK(esx_name_get(&NT, esx_hash("Nope")) == NULL && esx_name_get(&NT, 0) == NULL);
    // many names: all retrievable, table refuses (never loops) when full
    char nm[32];
    int ok = 0;
    for (int i = 0; i < 600; i++) { snprintf(nm, sizeof nm, "Anim_%d", i); if (esx_name_put(&NT, nm) >= 0) ok++; }
    CHECK(ok >= 500 && ok <= ESX_NAMES);
    CHECK(NT.full > 0);
    int found = 0;
    for (int i = 0; i < 600; i++) { snprintf(nm, sizeof nm, "Anim_%d", i); const char *g = esx_name_get(&NT, esx_hash(nm)); if (g && !strcmp(g, nm)) found++; }
    CHECK(found == ok);
    CHECK(esx_name_get(&NT, esx_hash("Not there at all")) == NULL);
    // a real hash collision keeps the first name (hand-made: same slot hash)
    EsxNameTab *t2 = &NT;
    memset(t2, 0, sizeof *t2);
    uint32_t hh = esx_hash("A1");
    t2->e[hh & (ESX_NAMES - 1)].h = hh, strcpy(t2->e[hh & (ESX_NAMES - 1)].s, "other"), t2->n = 1;
    CHECK(esx_name_put(t2, "A1") == -1);                       // same hash, different text: refused
}

static void test_off_set(void) {
    EsxOff o;
    memset(&o, 0, sizeof o);
    CHECK(!esx_off_has(&o, 5));
    CHECK(esx_off_add(&o, 5) == 1 && esx_off_add(&o, 5) == 0 && o.n == 1 && esx_off_has(&o, 5));
    CHECK(esx_off_add(&o, 6) == 1 && esx_off_del(&o, 5) == 1 && !esx_off_has(&o, 5) && esx_off_has(&o, 6) && o.n == 1);
    CHECK(esx_off_del(&o, 5) == 0);
    for (uint32_t i = 100; o.n < ESX_OFF_MAX; i++) CHECK(esx_off_add(&o, i) == 1);
    CHECK(esx_off_add(&o, 999999) == -1 && o.n == ESX_OFF_MAX);
    CHECK(esx_off_del(&o, 100) == 1 && esx_off_add(&o, 999999) == 1);
}

static void test_hook_table(void) {
    EsxHook h;
    memset(&h, 0, sizeof h);
    CHECK(!esx_hook_has(&h, 0x1000) && !esx_hook_has(&h, 0));
    CHECK(esx_hook_add(&h, 0x1000) == 1 && esx_hook_add(&h, 0x1000) == 0 && h.live == 1);
    CHECK(esx_hook_has(&h, 0x1000) && !esx_hook_has(&h, 0x2000));
    CHECK(esx_hook_del(&h, 0x1000) == 1 && !esx_hook_has(&h, 0x1000) && h.live == 0 && esx_hook_del(&h, 0x1000) == 0);
    for (uint64_t i = 1; i <= ESX_HOOK_MAX; i++) CHECK(esx_hook_add(&h, i * 0x10) == 1);
    CHECK(esx_hook_add(&h, 0xdead0) == -1 && h.live == ESX_HOOK_MAX);
    CHECK(esx_hook_has(&h, ESX_HOOK_MAX * 0x10));
    CHECK(esx_hook_del(&h, 0x10) == 1 && esx_hook_add(&h, 0xdead0) == 1);   // the freed slot is reused
    CHECK(!esx_hook_has(&h, 0) );                                           // 0 is never "an actor"
}

static void test_events(void) {
    memset(&Q, 0, sizeof Q);
    EsxEv e, out[8];
    memset(&e, 0, sizeof e);
    CHECK(esx_ev_take(&Q, out, 8) == 0);
    for (uint32_t i = 1; i <= 5; i++) e.hid = i, e.on = (uint8_t)(i & 1), e.name = 100 + i, e.params[0] = i * 10, esx_ev_push(&Q, &e);
    CHECK(Q.n == 5);
    CHECK(esx_ev_take(&Q, out, 3) == 3 && out[0].hid == 1 && out[2].hid == 3 && out[0].on == 1 && out[1].on == 0 && out[2].params[0] == 30 && Q.n == 2);
    CHECK(esx_ev_take(&Q, out, 8) == 2 && out[0].hid == 4 && out[1].hid == 5 && Q.n == 0);
    CHECK(esx_ev_take(&Q, out, 0) == 0);
    for (uint32_t i = 0; i < ESX_EVQ + 7; i++) e.hid = 1000 + i, esx_ev_push(&Q, &e);   // overflow drops the oldest
    CHECK(Q.n == ESX_EVQ && Q.dropped == 7);
    EsxEv all[ESX_EVQ];
    CHECK(esx_ev_take(&Q, all, ESX_EVQ) == ESX_EVQ && all[0].hid == 1007 && all[ESX_EVQ - 1].hid == 1000 + ESX_EVQ + 6);
}

static void test_trk(void) {
    EsxTrk t;
    memset(&t, 0, sizeof t);
    EsxTrkEnt *e = esx_trk_get(&t, 7, 1);
    CHECK(e && e->hid == 7 && t.n == 1 && esx_trk_get(&t, 7, 2) == e && e->seen == 2);
    uint8_t c0 = esx_trk_slot(e, 0, 0, 0, 0);
    CHECK(c0 == 0);                                              // nothing -> nothing: no change
    uint8_t c1 = esx_trk_slot(e, 0, 0xAA, 0x1000, 0);
    CHECK(c1 == 1);                                              // an AS started
    CHECK(esx_trk_slot(e, 0, 0xAA, 0x1000, 0) == 1);             // same AS running: unchanged
    CHECK(esx_trk_slot(e, 0, 0xAA, 0x2000, 0) == 2);             // same name, new AS object: a restart
    CHECK(esx_trk_slot(e, 0, 0xBB, 0x2000, 0) == 3);             // another name
    CHECK(esx_trk_slot(e, 0, 0xBB, 0x2000, 1) == 3);             // it finished: no bump yet
    CHECK(esx_trk_slot(e, 0, 0xBB, 0x2000, 0) == 4);             // finished then playing again (same object): a restart
    CHECK(esx_trk_slot(e, 0, 0, 0, 0) == 5);                     // stopped: counts (hash 0 goes out)
    CHECK(esx_trk_slot(e, 1, 0xCC, 0x3000, 0) == 1 && e->chg[0] == 5);   // slots are independent
    uint8_t w = 250;
    e->chg[1] = w;
    for (int i = 0; i < 10; i++) esx_trk_slot(e, 1, 0xCC, 0x3000 + (uint64_t)i + 1, 0);
    CHECK(e->chg[1] == (uint8_t)(w + 10));                       // wraps at 256 like the wire byte
    for (uint32_t h = 100; h < 100 + ESX_TRK_MAX + 5; h++) esx_trk_get(&t, h, h);
    CHECK(t.n == ESX_TRK_MAX);                                   // full: the stalest is evicted, never overflows
}

static void test_pup(void) {
    EsxPup p;
    memset(&p, 0, sizeof p);
    CHECK(esx_pup_get(&p, 5, 0x100, 0) == NULL);
    EsxPupEnt *e = esx_pup_get(&p, 5, 0x100, 1);
    CHECK(e && e->hid == 5 && e->actor == 0x100 && p.n == 1 && esx_pup_get(&p, 5, 0x100, 1) == e);
    CHECK(esx_pup_need(e, 0, 0, 0) == 0);                        // hash 0 = nothing wanted
    CHECK(esx_pup_need(e, 0, 0xAA, 3) == 1);                     // never applied
    esx_pup_done(e, 0, 0xAA, 3);
    CHECK(esx_pup_need(e, 0, 0xAA, 3) == 0);                     // same hash + chg: nothing to do
    CHECK(esx_pup_need(e, 0, 0xAA, 4) == 1);                     // chg bumped: restart
    CHECK(esx_pup_need(e, 0, 0xBB, 3) == 1);                     // another name
    CHECK(esx_pup_need(e, 1, 0xAA, 3) == 1);                     // slot 1 independent
    esx_pup_done(e, 0, 0xBB, 4);
    CHECK(esx_pup_need(e, 0, 0, 0) == 0 && e->have[0] == 0);     // nothing wanted forgets the state
    CHECK(esx_pup_need(e, 0, 0xBB, 4) == 1);                     // ... so the same one later restarts
    // the enemy reloaded = a new actor: the state resets
    e->nsens = 2;
    EsxPupEnt *e2 = esx_pup_get(&p, 5, 0x900, 1);
    CHECK(e2 == e && e2->actor == 0x900 && e2->nsens == 0 && !e2->have[0] && !e2->defs_learned);
    CHECK(esx_pup_get(&p, 5, 0x100, 0) == NULL);                 // lookup with the old actor and no create: not it
    // sensors
    CHECK(esx_pup_sens_add(e2, 11) == 1 && esx_pup_sens_add(e2, 11) == 0 && e2->nsens == 1);
    for (uint32_t i = 20; e2->nsens < ESX_PUP_SENS; i++) CHECK(esx_pup_sens_add(e2, i) == 1);
    CHECK(esx_pup_sens_add(e2, 99) == -1);
    CHECK(esx_pup_sens_del(e2, 11) == 1 && esx_pup_sens_del(e2, 11) == 0 && esx_pup_sens_add(e2, 99) == 1);
    // table: drop + full
    esx_pup_drop(&p, 5);
    CHECK(p.n == 0 && esx_pup_get(&p, 5, 0x900, 0) == NULL);
    for (uint32_t h = 1; h <= ESX_PUP_MAX; h++) CHECK(esx_pup_get(&p, h, h, 1) != NULL);
    CHECK(esx_pup_get(&p, 9999, 1, 1) == NULL && p.n == ESX_PUP_MAX);
}

static void test_mtx(void) {
    float m[12], pos[3] = {10.5f, 3.25f, -7.0f};
    esx_mtx(m, pos, 0.0f);
    CHECK(m[0] == 1 && m[2] == 0 && m[8] == 0 && m[10] == 1 && m[5] == 1 && m[3] == 10.5f && m[7] == 3.25f && m[11] == -7.0f);
    CHECK(m[1] == 0 && m[4] == 0 && m[6] == 0 && m[9] == 0);
    for (float y = -3.1f; y < 3.15f; y += 0.37f) {                 // round trip through the matrix, rows stay unit length
        esx_mtx(m, pos, y);
        float d = esx_yaw(m) - y;
        CHECK(d > -1e-4f && d < 1e-4f);
        CHECK(m[0] * m[0] + m[2] * m[2] > 0.9999f && m[0] * m[0] + m[2] * m[2] < 1.0001f);
        CHECK(m[0] == m[10] && m[2] == -m[8]);                      // a pure rotation about y
    }
    esx_mtx(m, pos, 1.5707963f);                                    // +90 deg: facing +x (sin, cos) = (1, 0)
    CHECK(m[2] > 0.9999f && m[10] < 1e-4f && m[10] > -1e-4f);
}


// S168 review fixes: freeze waits for our own sensor (MF-2), find without create, puppet sensor-dup + own nodes (SF-2 / SF-6)
static void test_review_fixes(void) {
    static EsxTrk T;
    memset(&T, 0, sizeof T);
    CHECK(esx_trk_find(&T, 0x77) == NULL && T.n == 0);            // find never creates
    CHECK(esx_trk_may_freeze(NULL, 100, 120) == 1);               // never seen = free to freeze
    EsxTrkEnt *t = esx_trk_get(&T, 0x77, 10);
    CHECK(esx_trk_find(&T, 0x77) == t && esx_trk_may_freeze(t, 11, 120) == 1);
    t->son = 1, t->son_frame = 50, t->son_name = 0xABCD;
    CHECK(esx_trk_may_freeze(t, 50, 120) == 0 && esx_trk_may_freeze(t, 170, 120) == 0);   // own sensor on: wait
    CHECK(esx_trk_may_freeze(t, 171, 120) == -1);                                          // cap passed: caller forces it off
    t->son = 0;
    CHECK(esx_trk_may_freeze(t, 171, 120) == 1);
    static EsxPup P;
    memset(&P, 0, sizeof P);
    EsxPupEnt *p = esx_pup_get(&P, 0x55, 0x1000, 1);
    CHECK(!esx_pup_has_sens(p, 9) && esx_pup_sens_add(p, 9) == 1 && esx_pup_has_sens(p, 9));
    CHECK(esx_pup_node_get(p, 5) == 0);
    esx_pup_node_put(p, 5, 0x2000), esx_pup_node_put(p, 5, 0x3000);   // first node of a name wins
    CHECK(esx_pup_node_get(p, 5) == 0x2000 && p->nnode == 1);
    for (uint32_t i = 0; i < 40; i++) esx_pup_node_put(p, 100 + i, 0x4000 + i);
    CHECK(p->nnode == ESX_PUP_NODES && esx_pup_node_get(p, 100) == 0x4000 && esx_pup_node_get(p, 139) == 0);
    p->off_pending = 1;
    EsxPupEnt *q = esx_pup_get(&P, 0x55, 0x1001, 1);                  // reload = new actor: all of it resets
    CHECK(q == p && !q->off_pending && !q->nnode && !q->nsens);
}

static void test_atk_step(void) {  // s174 R2: the puppet's local attack decisions
    static EsxPup P;
    memset(&P, 0, sizeof P);
    EsxPupEnt *p = esx_pup_get(&P, 0x77, 0x1000, 1);
    CHECK(esx_pup_atk_node(p, 6) == 0);
    esx_pup_atk_put(p, 6, 0x2000), esx_pup_atk_put(p, 6, 0x3000), esx_pup_atk_put(p, 0, 0x4000);  // first wins, index 0 refused
    CHECK(esx_pup_atk_node(p, 6) == 0x2000 && p->natk == 1);
    for (int i = 0; i < 40; i++) esx_pup_atk_put(p, (uint16_t)(100 + i), 0x5000 + i);
    CHECK(p->natk == ESX_PUP_ATK && esx_pup_atk_node(p, 100) == 0x5000 && esx_pup_atk_node(p, 139) == 0);
    CHECK(esx_atk_step(p, 0, 0, 0, 10, 180, 20, 1) == ESX_ATK_STEP_NONE);     // owner runs nothing
    CHECK(esx_atk_step(p, 22, 0, 0, 10, 180, 20, 1) == ESX_ATK_STEP_NONE);    // not an attack
    CHECK(esx_atk_step(p, 6, 1, 0, 10, 180, 20, 0) == ESX_ATK_STEP_NONE);     // dead: never enter
    CHECK(esx_atk_step(p, 6, 1, 3, 10, 180, 20, 1) == ESX_ATK_STEP_ENTER);
    p->atk_node = 0x2000, p->atk_idx = 6, p->atk_frame = 10;
    CHECK(esx_atk_step(p, 6, 1, 3, 100, 180, 20, 1) == ESX_ATK_STEP_NONE);    // keeps running
    CHECK(esx_atk_step(p, 6, 1, 3, 191, 180, 20, 1) == ESX_ATK_STEP_LEAVE);   // cap
    CHECK(esx_atk_step(p, 22, 0, 3, 50, 180, 20, 1) == ESX_ATK_STEP_LEAVE);   // owner moved on
    CHECK(esx_atk_step(p, 181, 1, 3, 50, 180, 20, 1) == ESX_ATK_STEP_SWAP);   // owner swapped to another attack
    CHECK(esx_atk_step(p, 6, 1, 3, 50, 180, 20, 0) == ESX_ATK_STEP_LEAVE);    // died mid-swing
    p->atk_node = 0, p->atk_last = 6, p->atk_lchg = 4, p->atk_end = 60;       // the swing finished here, owner chg was 4
    CHECK(esx_atk_step(p, 6, 1, 4, 61, 180, 20, 1) == ESX_ATK_STEP_NONE);     // same swing: not again
    CHECK(esx_atk_step(p, 6, 1, 4, 500, 180, 20, 1) == ESX_ATK_STEP_NONE);    // still the same swing, however long
    CHECK(esx_atk_step(p, 6, 1, 5, 70, 180, 20, 1) == ESX_ATK_STEP_NONE);     // owner AS changed but inside the gap
    CHECK(esx_atk_step(p, 6, 1, 5, 80, 180, 20, 1) == ESX_ATK_STEP_ENTER);    // a new swing of the same action
    CHECK(esx_atk_step(p, 181, 1, 4, 61, 180, 20, 1) == ESX_ATK_STEP_ENTER);  // another attack: at once
    EsxPupEnt *q = esx_pup_get(&P, 0x77, 0x1001, 1);                          // reload: attack state resets too
    CHECK(q == p && !q->natk && !q->atk_node && !q->atk_walked);
}

int main(void) {
    test_hash_names();
    test_off_set();
    test_hook_table();
    test_events();
    test_trk();
    test_pup();
    test_mtx();
    test_review_fixes();
    test_atk_step();
    printf("%d checks, %d failed\n", runs, fails);
    return fails != 0;
}

// host test for the s196 killsync CORE: NET_KILLS pack / unpack (runtime/sora_net_proto.h) + the kill list, the asker and the
// answerer of runtime/sora_net_kills.h (no NET, no game). Two or three "games" talk through a fake channel built on the real wire
// functions (loss, reorder, duplicates). The glue (sockets, the Blood Moon count, the load hook, the apply scan, killsync 0, and
// (s197) killsync ON by default with no live key) is in net_glue_test.c / net_melty_test.c.
// (s197 review fixes) M1: kill ages + the asker's Blood Moon age (two games, three games forwarding, saturated ages, an answer that
// crosses a moon); S2: the Stal name rule; S3: one new snapshot per peer per KS_ANS_NEW_MS; S4: restart / part / new-kill / probe caps.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/kills_test.c -o build_h/kills_test.exe
#include <stdio.h>
#include <stdlib.h>
#include "sora_net_proto.h"
#include "sora_net_kills.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
#define OLD KS_KT_OLD

// ---- the list ----
static KsList g_l;
static void test_list(void) {
    KsList *L = &g_l;
    ks_clear(L);
    uint32_t e0 = L->epoch;
    CHECK(L->n == 0 && ks_find(L, 0x1234) < 0 && ks_find(L, 0) < 0);
    CHECK(ks_add(L, 0, KSRC_OWN, 5, 5) == KSA_BAD && ks_add(L, 7, 9, 5, 5) == KSA_BAD && L->n == 0);
    CHECK(ks_add(L, 0x1234, KSRC_OWN, 100, 100) == KSA_NEW && ks_add(L, 0x5678, KSRC_EDMG, 0, 50) == KSA_NEW && ks_add(L, 0x9abc, KSRC_CATCH, 0, OLD) == KSA_NEW);
    CHECK(L->n == 3 && L->nwait == 2 && L->nsrc[KSRC_OWN] == 1 && L->nsrc[KSRC_EDMG] == 1 && L->nsrc[KSRC_CATCH] == 1);
    CHECK(ks_find(L, 0x1234) == 0 && ks_find(L, 0x5678) == 1 && ks_find(L, 0x9abc) == 2 && L->hid[1] == 0x5678 && L->src[2] == KSRC_CATCH);
    CHECK(L->kt[0] == 100 && L->kt[1] == 50 && L->kt[2] == OLD);
    // a duplicate: not added again, keeps its first source; a waiting one that comes again as applied is no longer waiting
    CHECK(ks_add(L, 0x1234, KSRC_CATCH, 0, 90) == KSA_DUP && L->n == 3 && L->src[0] == KSRC_OWN && L->applied[0] == 100);
    CHECK(ks_add(L, 0x5678, KSRC_OWN, 200, 200) == KSA_DUP && L->nwait == 1 && L->applied[1] == 200 && L->src[1] == KSRC_EDMG);
    CHECK(ks_add(L, 0x9abc, KSRC_CATCH, 0, OLD) == KSA_DUP && L->nwait == 1);
    // (s197) a duplicate keeps the NEWEST kill time (an older one, or "not known", never makes it older)
    CHECK(L->kt[0] == 100 && L->kt[1] == 200 && L->kt[2] == OLD);
    CHECK(ks_add(L, 0x9abc, KSRC_CATCH, 0, 70) == KSA_DUP && L->kt[2] == 70 && ks_add(L, 0x9abc, KSRC_CATCH, 0, OLD) == KSA_DUP && L->kt[2] == 70);
    ks_mark(L, 2, 300);
    CHECK(L->nwait == 0 && L->applied[2] == 300);
    ks_mark(L, 2, 400);  // (marked again: the count does not go below 0)
    CHECK(L->nwait == 0 && L->applied[2] == 400);
    ks_mark(L, 99, 1);   // (out of range: nothing)
    CHECK(L->nwait == 0);
    // hids that collide in the table (same top 14 bits of hid * golden): all found
    ks_clear(L);
    CHECK(L->epoch == e0 + 1 && L->n == 0 && ks_find(L, 0x1234) < 0);
    uint32_t base = 0x00010000u, col[8];
    int nc = 0;
    for (uint32_t h = base; nc < 8 && h < base + 2000000u; h++)
        if (ks_slot0(h) == ks_slot0(base)) col[nc++] = h;
    CHECK(nc == 8);
    for (int i = 0; i < nc; i++) CHECK(ks_add(L, col[i], KSRC_CATCH, 0, OLD) == KSA_NEW);
    for (int i = 0; i < nc; i++) CHECK(ks_find(L, col[i]) == i);
    CHECK(ks_add(L, col[3], KSRC_CATCH, 0, OLD) == KSA_DUP && L->n == nc);
    // full: KS_CAP kills, the next new one is refused (counted), the ones on the list stay, a duplicate is still a duplicate
    ks_clear(L);
    for (uint32_t i = 0; i < KS_CAP; i++) if (ks_add(L, 0x10000000u + i * 7919u, KSRC_CATCH, 0, OLD) != KSA_NEW) { CHECK(0); break; }
    CHECK(L->n == KS_CAP && L->nwait == KS_CAP && L->crowd == 0);
    CHECK(ks_add(L, 0x7fffffffu, KSRC_OWN, 1, 1) == KSA_FULL && L->full == 1 && L->n == KS_CAP);
    CHECK(ks_add(L, 0x10000000u + 5 * 7919u, KSRC_OWN, 9, 9) == KSA_DUP && L->full == 1);
    bool all = true;
    for (uint32_t i = 0; i < KS_CAP; i += 97) all = all && ks_find(L, 0x10000000u + i * 7919u) == (int)i;
    CHECK(all && ks_find(L, 0x7fffffffu) < 0);
    ks_clear(L);
    CHECK(L->n == 0 && L->full == 0 && L->nwait == 0 && ks_find(L, 0x10000000u) < 0);
    // only placed enemies are kept dead; (s197 S2) never a Stal (they come back every night)
    CHECK(ks_name_ok("Enemy_Bokoblin_Junior") && ks_name_ok("Enemy_Guardian_Mini_Baby") && !ks_name_ok("Animal_Boar_A") && !ks_name_ok("Animal_Wolf_A"));
    CHECK(!ks_name_ok("") && !ks_name_ok(NULL) && !ks_name_ok("Enemy") && !ks_name_ok("enemy_Bokoblin"));
    CHECK(!ks_name_ok("Enemy_Bokoblin_Bone_Junior") && !ks_name_ok("Enemy_Moriblin_Bone_Junior") && !ks_name_ok("Enemy_Lizalfos_Bone") && !ks_name_ok("Enemy_Giant_Bone"));
    CHECK(ks_name_ok("Enemy_Lizalfos_Electric") && ks_name_ok("Enemy_Giant_Junior") && ks_name_ok("Enemy_Lynel_Dark"));
}

// (s197 review S4) crafted hids that all want one home slot: an add / find walks at most KS_PROBE_MAX slots; the 65th is refused
// (counted "crowd", the enemy stays alive), the 64 before it stay findable; other hids are not affected
static void test_probe_cap(void) {
    KsList *L = &g_l;
    ks_clear(L);
    static uint32_t col[KS_PROBE_MAX + 4];
    int nc = 0;
    uint32_t base = 0x00200000u;
    for (uint32_t h = base; nc < KS_PROBE_MAX + 4 && h < base + 20000000u; h++)
        if (ks_slot0(h) == ks_slot0(base)) col[nc++] = h;
    CHECK(nc == KS_PROBE_MAX + 4);
    for (int i = 0; i < KS_PROBE_MAX; i++) CHECK(ks_add(L, col[i], KSRC_CATCH, 0, OLD) == KSA_NEW);
    CHECK(ks_add(L, col[KS_PROBE_MAX], KSRC_CATCH, 0, OLD) == KSA_FULL && L->crowd == 1 && L->full == 0 && L->n == KS_PROBE_MAX);
    CHECK(ks_add(L, col[KS_PROBE_MAX + 1], KSRC_CATCH, 0, OLD) == KSA_FULL && L->crowd == 2);
    bool all = true;
    for (int i = 0; i < KS_PROBE_MAX; i++) all = all && ks_find(L, col[i]) == i;
    CHECK(all && ks_find(L, col[KS_PROBE_MAX]) < 0 && ks_find(L, col[KS_PROBE_MAX + 3]) < 0);
    CHECK(ks_add(L, col[5], KSRC_OWN, 1, 1) == KSA_DUP);  // (a duplicate inside the run is still found)
    CHECK(ks_slot0(0x55555555u) != ks_slot0(base) && ks_add(L, 0x55555555u, KSRC_OWN, 1, 1) == KSA_NEW && L->crowd == 2);  // (another home: fine)
    ks_clear(L);
}

// ---- which loaded enemies get the kill now ----
static void test_pick(void) {
    KsList *L = &g_l;
    ks_clear(L);
    ks_add(L, 0xA1, KSRC_CATCH, 0, OLD), ks_add(L, 0xA2, KSRC_EDMG, 0, 900), ks_add(L, 0xA3, KSRC_OWN, 1000, 1000);
    uint32_t loaded[6] = {0xB0, 0xA1, 0xA2, 0xA1, 0xA3, 0};
    int out[8];
    // at 1500: A1 + A2 (waiting); A1 twice in the enum = once; A3 (own, seen at 1000) is inside KS_REAPPLY_MS; B0 / 0 not on the list
    int n = ks_pick(L, loaded, 6, 1500, out, 8);
    CHECK(n == 2 && out[0] == 0 && out[1] == 1);
    ks_mark(L, 0, 1500), ks_mark(L, 1, 1500);
    CHECK(L->nwait == 0);
    CHECK(ks_pick(L, loaded, 6, 1500 + KS_REAPPLY_MS - 1, out, 8) == 1 && out[0] == 2);  // (A3: 1000 + 3000 <= 4499)
    n = ks_pick(L, loaded, 6, 1500 + KS_REAPPLY_MS, out, 8);  // (still alive 3 s later: again)
    CHECK(n == 3);
    CHECK(ks_pick(L, loaded, 6, 1500 + KS_REAPPLY_MS, out, 1) == 1);  // (the cap)
    CHECK(ks_pick(L, loaded, 0, 99999, out, 8) == 0 && ks_pick(L, NULL, 0, 1, out, 8) == 0);
    CHECK(ks_pick(L, loaded, 6, 10, out, 8) == 3);  // (a clock behind the stamp: no stuck guard)
}

// ---- (s197 M1) ages, moon ages, cuts ----
static void test_ages(void) {
    uint64_t now = 10000000;  // (10 000 s)
    // kill time -> age: whole seconds rounded down, 0 for the future, saturated (and "not known") = OLD
    CHECK(ks_age_s((int64_t)now, now) == 0 && ks_age_s((int64_t)now - 999, now) == 0 && ks_age_s((int64_t)now - 1000, now) == 1);
    CHECK(ks_age_s((int64_t)now - 61999, now) == 61 && ks_age_s((int64_t)now + 5000, now) == 0);
    CHECK(ks_age_s((int64_t)now - 65534999, now) == 65534 && ks_age_s((int64_t)now - 65535000, now) == NET_KILLS_AGE_OLD);
    CHECK(ks_age_s((int64_t)now - 90000000, now) == NET_KILLS_AGE_OLD && ks_age_s(OLD, now) == NET_KILLS_AGE_OLD && ks_age_s(-5, 3) == 0);
    // age -> kill time: the OLDER end of the second (+999), never younger than it was
    CHECK(ks_kt_from_age(0, now) == (int64_t)now - 999 && ks_kt_from_age(61, now) == (int64_t)now - 61999 && ks_kt_from_age(NET_KILLS_AGE_OLD, now) == OLD);
    CHECK(ks_kt_from_age(5000, 1000) == 1000 - 5000999LL);  // (older than this game's clock start: a negative time is fine)
    // a round trip never makes a kill younger
    for (int64_t a = 0; a < 70000; a += 333) {
        int64_t kt = (int64_t)now - a;
        CHECK(ks_kt_from_age(ks_age_s(kt, now), now) <= kt);
    }
    // the ask's moon: none seen = NONE; else whole seconds, saturating at 0xFFFE (never NONE); a clock behind = 0
    CHECK(ks_moon_s(false, 123, now) == NET_KILLS_MOON_NONE && ks_moon_s(true, now, now) == 0 && ks_moon_s(true, now - 59999, now) == 59);
    CHECK(ks_moon_s(true, now + 50, now) == 0 && ks_moon_s(true, 0, 99999999999ull) == 0xFFFE && ks_moon_s(true, 100000000 - 65534000, 100000000) == 0xFFFE);
    CHECK(ks_moon_s(true, 100000000 - 65533999, 100000000) == 0xFFFD);
    // cuts: NONE = all (also OLD); else a kill must be more than KS_MOON_SLACK_MS younger than the moon
    int64_t c = ks_cut(60, now);
    CHECK(ks_cut(NET_KILLS_MOON_NONE, now) == KS_CUT_ALL && ks_kt_after(OLD, KS_CUT_ALL) && ks_kt_after(5, KS_CUT_ALL));
    CHECK(c == (int64_t)now - 60000 + KS_MOON_SLACK_MS && !ks_kt_after(c, c) && ks_kt_after(c + 1, c) && !ks_kt_after(OLD, c));
    CHECK(ks_cut(0xFFFE, now) == (int64_t)now - 65534000LL + KS_MOON_SLACK_MS);
    CHECK(ks_my_cut(false, 77) == KS_CUT_ALL && ks_my_cut(true, 5000) == 5000 + KS_MOON_SLACK_MS);
    // a saturated kill age only goes to an asker with no moon: no real moon age (<= 0xFFFE) lets an age of 0xFFFF through
    for (int m = 0; m <= 0xFFFE; m += 4095) CHECK(!ks_kt_after(ks_kt_from_age(NET_KILLS_AGE_OLD, now), ks_cut((uint16_t)m, now)));
    CHECK(!ks_kt_after(ks_kt_from_age(NET_KILLS_AGE_OLD, now), ks_cut(0xFFFE, now)) && ks_kt_after(ks_kt_from_age(NET_KILLS_AGE_OLD, now), ks_cut(NET_KILLS_MOON_NONE, now)));
}

// ---- the wire ----
static void test_wire(void) {
    uint8_t b[NET_KILLS_PKT + 16];
    int n = net_pack_kills_ask(42, 0x1234, 0xA1B2C3D4u, 0x0102, 0x8000000000000005ull, 0xBEEF, b, (int)sizeof b);
    CHECK(n == 24 && n == NET_KILLS_ASK_LEN);
    CHECK(b[0] == 'S' && b[1] == 'N' && b[2] == NET_VERSION && b[3] == NET_KILLS && b[3] == 9 && b[4] == 42 && b[5] == 0x34 && b[6] == 0x12);
    CHECK(b[7] == NKO_ASK && b[8] == 0xD4 && b[11] == 0xA1 && b[12] == 0x02 && b[13] == 0x01 && b[14] == 0x05 && b[21] == 0x80 && b[22] == 0xEF && b[23] == 0xBE);
    NetKillsMsg m;
    uint8_t pl = 0;
    uint16_t sq = 0;
    int t = 0;
    CHECK(net_unpack_ex(b, n, &t, &pl, &sq, NULL, NULL, NULL, NULL) && t == NET_KILLS && pl == 42 && sq == 0x1234);
    CHECK(net_unpack_kills(b, n, &pl, &sq, &m) && m.op == NKO_ASK && m.nonce == 0xA1B2C3D4u && m.tag == 0x0102 && m.have == 0x8000000000000005ull && m.moon == 0xBEEF);
    CHECK(!net_unpack_kills(b, n - 1, &pl, &sq, &m) && !net_unpack_ex(b, n + 1, &t, NULL, NULL, NULL, NULL, NULL, NULL));
    CHECK(!net_unpack_ex(b, 22, &t, NULL, NULL, NULL, NULL, NULL, NULL));  // (the s196 22-byte ask: refused, never half-read)
    CHECK(net_pack_kills_ask(1, 1, 0, 0, 0, 0, b, (int)sizeof b) == -1 && net_pack_kills_ask(1, 1, 5, 0, 0, 0, b, 23) == -1);
    n = net_pack_kills_ask(1, 2, 7, 0, 0, NET_KILLS_MOON_NONE, b, (int)sizeof b);
    CHECK(net_unpack_kills(b, n, &pl, &sq, &m) && m.moon == NET_KILLS_MOON_NONE && m.have == 0 && m.tag == 0);
    // a part: 106 kills with ages, the byte layout; hid 0 in the middle is dropped alone (its age with it)
    uint32_t h[NET_KILLS_CHUNK];
    uint16_t ag[NET_KILLS_CHUNK];
    for (int i = 0; i < NET_KILLS_CHUNK; i++) h[i] = 0x11000000u + (uint32_t)i, ag[i] = (uint16_t)(100 + i);
    h[7] = 0, ag[105] = NET_KILLS_AGE_OLD;
    n = net_pack_kills_part(7, 3, 0xCAFEBABEu, 0x0203, 4, 9, h, ag, NET_KILLS_CHUNK, b, (int)sizeof b);
    CHECK(n == NET_KILLS_PKT && n == 653 && n <= 657 && n <= NET_PKT_MAX);
    CHECK(b[7] == NKO_PART && b[8] == 0xBE && b[12] == 0x03 && b[13] == 0x02 && b[14] == 4 && b[15] == 9 && b[16] == 106 && b[17] == 0x00 && b[20] == 0x11);
    CHECK(b[21] == 100 && b[22] == 0 && b[23] == 0x01 && b[27] == 101 && b[28] == 0);  // (hid 0 LE, then its u16 age; then hid 1)
    CHECK(net_unpack_kills(b, n, &pl, &sq, &m) && m.op == NKO_PART && pl == 7 && m.nonce == 0xCAFEBABEu && m.tag == 0x0203 && m.i == 4 && m.n == 9);
    CHECK(m.k == NET_KILLS_CHUNK - 1 && m.hid[0] == 0x11000000u && m.hid[6] == 0x11000006u && m.hid[7] == 0x11000008u && m.hid[104] == 0x11000069u);
    CHECK(m.age[0] == 100 && m.age[6] == 106 && m.age[7] == 108 && m.age[104] == NET_KILLS_AGE_OLD);
    // no ages given = every one OLD
    n = net_pack_kills_part(7, 3, 9, 1, 0, 1, h, NULL, 3, b, (int)sizeof b);
    CHECK(n == NET_KILLS_PART_HDR + 18 && net_unpack_kills(b, n, &pl, &sq, &m) && m.k == 3 && m.age[0] == NET_KILLS_AGE_OLD && m.age[2] == NET_KILLS_AGE_OLD);
    // an empty answer: one part with k 0
    n = net_pack_kills_part(7, 4, 5, 1, 0, 1, NULL, NULL, 0, b, (int)sizeof b);
    CHECK(n == NET_KILLS_PART_HDR && net_unpack_kills(b, n, &pl, &sq, &m) && m.k == 0 && m.n == 1 && m.i == 0);
    // refused by pack: tag 0, nonce 0, i >= n, n 0 / 65, k 107, no hids, a small buffer
    CHECK(net_pack_kills_part(7, 4, 5, 0, 0, 1, h, ag, 1, b, (int)sizeof b) == -1 && net_pack_kills_part(7, 4, 0, 1, 0, 1, h, ag, 1, b, (int)sizeof b) == -1);
    CHECK(net_pack_kills_part(7, 4, 5, 1, 1, 1, h, ag, 1, b, (int)sizeof b) == -1 && net_pack_kills_part(7, 4, 5, 1, 0, 0, h, ag, 1, b, (int)sizeof b) == -1);
    CHECK(net_pack_kills_part(7, 4, 5, 1, 0, 65, h, ag, 1, b, (int)sizeof b) == -1 && net_pack_kills_part(7, 4, 5, 1, 0, 1, h, ag, 107, b, (int)sizeof b) == -1);
    CHECK(net_pack_kills_part(7, 4, 5, 1, 0, 1, NULL, ag, 3, b, (int)sizeof b) == -1 && net_pack_kills_part(7, 4, 5, 1, 0, 1, h, ag, 3, b, 34) == -1);
    // refused by the receiver: hand-made bad ones (net_unpack_ex is what the glue's receive loop asks first)
    n = net_pack_kills_part(7, 4, 5, 1, 2, 3, h, ag, 2, b, (int)sizeof b);
    CHECK(n == 29 && net_unpack_ex(b, n, &t, NULL, NULL, NULL, NULL, NULL, NULL));
    uint8_t c[NET_KILLS_PKT + 16];
    memcpy(c, b, (size_t)n), c[14] = 3;  // i == n
    CHECK(!net_unpack_ex(c, n, &t, NULL, NULL, NULL, NULL, NULL, NULL) && !net_unpack_kills(c, n, &pl, &sq, &m));
    memcpy(c, b, (size_t)n), c[15] = 0, c[14] = 0;  // n 0
    CHECK(!net_unpack_ex(c, n, &t, NULL, NULL, NULL, NULL, NULL, NULL));
    memcpy(c, b, (size_t)n), c[15] = 65;  // n 65
    CHECK(!net_unpack_ex(c, n, &t, NULL, NULL, NULL, NULL, NULL, NULL));
    memcpy(c, b, (size_t)n), c[16] = 3;  // k says 3, 2 there
    CHECK(!net_unpack_ex(c, n, &t, NULL, NULL, NULL, NULL, NULL, NULL));
    CHECK(!net_unpack_ex(c, NET_KILLS_PART_HDR + 4 * 3, &t, NULL, NULL, NULL, NULL, NULL, NULL));  // (k 3 in the s196 4-byte layout: refused)
    memcpy(c, b, (size_t)n), c[7] = 3;  // op 3
    CHECK(!net_unpack_ex(c, n, &t, NULL, NULL, NULL, NULL, NULL, NULL));
    memcpy(c, b, (size_t)n), c[12] = c[13] = 0;  // tag 0
    CHECK(net_unpack_ex(c, n, &t, NULL, NULL, NULL, NULL, NULL, NULL) && !net_unpack_kills(c, n, &pl, &sq, &m));
    memcpy(c, b, (size_t)n), c[8] = c[9] = c[10] = c[11] = 0;  // nonce 0
    CHECK(!net_unpack_kills(c, n, &pl, &sq, &m));
    memcpy(c, b, (size_t)n), c[2] = NET_VERSION + 1;  // another version
    CHECK(!net_unpack_ex(c, n, &t, NULL, NULL, NULL, NULL, NULL, NULL) && net_other_version(c, n) == NET_VERSION + 1);
    CHECK(!net_unpack_ex(b, NET_HDR, &t, NULL, NULL, NULL, NULL, NULL, NULL));  // (no op byte)
    // a 107-kill part made by hand (k 107, exact length): refused
    static uint8_t big[NET_KILLS_PART_HDR + NET_KILLS_ENT * 107];
    memcpy(big, b, NET_KILLS_PART_HDR), big[16] = 107;
    CHECK(!net_unpack_ex(big, (int)sizeof big, &t, NULL, NULL, NULL, NULL, NULL, NULL));
    // another type's unpack does not take it, ours does not take another type
    NetReactEnt re[NET_REACT_MAX];
    CHECK(net_unpack_react(b, n, NULL, NULL, re, NULL) == 0);
    uint8_t ab[NET_AREA_PKT];
    int an = net_pack_area(1, 2, "MainField", ab, (int)sizeof ab);
    CHECK(an > 0 && !net_unpack_kills(ab, an, &pl, &sq, &m));
    // old builds: the version byte did not change (no "update" log in a 0.1.4 / 7be3a1d game: v11), and the types are past NET_AREA,
    // the last one those builds know (their net_unpack_ex: "else return false" = a "bad" packet, skipped before any slot is touched)
    CHECK(NET_VERSION == 11 && NET_KILLS > NET_AREA && NET_REACT > NET_AREA && b[2] == 11 && b[3] == 9);
}

// ---- several games through a fake channel (the real wire) ----
#define G_MAX 3
typedef struct {
    KsList L;
    KsAsk ask[G_MAX];  // by the OTHER game's index
    KsAns ans[G_MAX];
    uint16_t tagc, seq;
    uint32_t nctr;
    bool moon_seen;     // (s197 M1) this game's last Blood Moon (the glue's NETK.moon_seen / moon_t)
    uint64_t moon_t;
    bool ignore_cut;    // (a hostile / buggy answerer: sends every kill, whatever the asker's moon)
    struct { int ask_tx, part_tx, take, done, dup, stale, bad, aborted, later, cap, old, fresh; } st;
} Game;
static Game g_g[G_MAX];
static uint64_t g_now;  // (the time of the last tick: the receive uses it)
typedef struct { int from, to, len; uint8_t b[NET_KILLS_PKT]; } Pkt;
static Pkt g_q[4096];
static int g_nq;
static int g_drop_part[NET_KILLS_CHUNKS_MAX], g_drop_asks;  // (the next part i from any game is dropped g_drop_part[i] times; the next asks)
static void send_pkt(int from, int to, const uint8_t *b, int len) {
    if (len <= 0) { CHECK(0); return; }
    if (b[7] == NKO_PART && g_drop_part[b[14]] > 0) { g_drop_part[b[14]]--; return; }
    if (b[7] == NKO_ASK && g_drop_asks > 0) { g_drop_asks--; return; }
    if (g_nq < 4096) g_q[g_nq].from = from, g_q[g_nq].to = to, g_q[g_nq].len = len, memcpy(g_q[g_nq].b, b, (size_t)len), g_nq++;
}
static void g_reset(int ng) {
    memset(g_g, 0, sizeof g_g), g_nq = 0, memset(g_drop_part, 0, sizeof g_drop_part), g_drop_asks = 0, g_now = 0;
    for (int i = 0; i < ng; i++) ks_clear(&g_g[i].L);
}
// n own kills, killed at kt
static void g_fill_at(int gi, uint32_t first, int n, uint32_t step, int64_t kt) {
    for (int i = 0; i < n; i++) ks_add(&g_g[gi].L, first + (uint32_t)i * step, KSRC_OWN, 1, kt);
}
static void g_fill(int gi, uint32_t first, int n, uint32_t step) { g_fill_at(gi, first, n, step, 1); }
static void g_ask(int gi, int peer, uint64_t now) { ks_ask_start(&g_g[gi].ask[peer], ks_nonce(now, ++g_g[gi].nctr * 31u + (uint32_t)gi), now); }
// a Blood Moon in game gi now (as the glue: list cleared, every ask / answer stopped, the moon time set)
static void g_moon(int gi, uint64_t now) {
    Game *g = &g_g[gi];
    ks_clear(&g->L);
    for (int p = 0; p < G_MAX; p++) g->ask[p].active = false, g->ans[p].active = false;
    g->moon_seen = true, g->moon_t = now;
}
// one tick of game gi with ng games: asks due out, up to `budget` parts out (all peers)
static void g_tick(int gi, int ng, uint64_t now, bool melty, int budget) {
    Game *g = &g_g[gi];
    g_now = now;
    uint8_t b[NET_KILLS_PKT];
    for (int p = 0; p < ng; p++) {
        if (p == gi) continue;
        if (ks_ask_step(&g->ask[p], now, melty) == KSD_SEND) {
            send_pkt(gi, p, b, net_pack_kills_ask((uint8_t)gi, ++g->seq, g->ask[p].nonce, g->ask[p].tag, ks_ask_have(&g->ask[p]),
                                                  ks_moon_s(g->moon_seen, g->moon_t, now), b, (int)sizeof b));
            g->st.ask_tx++;
        }
    }
    for (int p = 0; p < ng && budget > 0; p++) {
        int i, k, ab;
        uint32_t h[NET_KILLS_CHUNK];
        uint16_t ag[NET_KILLS_CHUNK];
        if (g->ignore_cut) g->ans[p].cut = KS_CUT_ALL;
        while (budget > 0 && ks_ans_next(&g->ans[p], &g->L, now, &i, h, ag, &k, &ab)) {
            send_pkt(gi, p, b, net_pack_kills_part((uint8_t)gi, ++g->seq, g->ans[p].nonce, g->ans[p].tag, i, g->ans[p].n, h, ag, k, b, (int)sizeof b));
            g->st.part_tx++, budget--;
        }
        g->st.aborted += ab;
    }
}
static void g_recv(const Pkt *q) {
    Game *g = &g_g[q->to];
    static NetKillsMsg m;
    uint8_t pl;
    uint16_t sq;
    if (!net_unpack_kills(q->b, q->len, &pl, &sq, &m)) { g->st.bad++; return; }
    if (m.op == NKO_ASK) { g->st.later += ks_ans_ask(&g->ans[q->from], m.nonce, m.tag, m.have, m.moon, &g->L, &g->tagc, g_now) == KSAN_LATER; return; }
    KsAsk *a = &g->ask[q->from];
    int r = ks_ask_part(a, m.nonce, m.tag, m.i, m.n);
    if (r == KSP_STALE) { g->st.stale++; return; }
    if (r == KSP_DUP) { g->st.dup++; return; }
    if (r == KSP_BAD) { g->st.bad++; return; }
    if (r == KSP_CAP) { g->st.cap++; return; }
    int old = 0, full = 0;
    g->st.fresh += ks_take(&g->L, a, m.hid, m.age, m.k, g_now, ks_my_cut(g->moon_seen, g->moon_t), &old, &full);
    g->st.old += old;
    g->st.take++, g->st.done += r == KSP_DONE;
}
// deliver everything queued (order 0 = as sent, 1 = reversed, 2 = every packet twice)
static void g_deliver(int order) {
    static Pkt q[4096];
    int n = g_nq;
    memcpy(q, g_q, sizeof q[0] * (size_t)n), g_nq = 0;
    for (int i = 0; i < n; i++) {
        const Pkt *p = &q[order == 1 ? n - 1 - i : i];
        g_recv(p);
        if (order == 2) g_recv(p);
    }
}
static bool g_has_all(int gi, uint32_t first, int n, uint32_t step) {
    for (int i = 0; i < n; i++) if (ks_find(&g_g[gi].L, first + (uint32_t)i * step) < 0) return false;
    return true;
}
static bool g_has_none(int gi, uint32_t first, int n, uint32_t step) {
    for (int i = 0; i < n; i++) if (ks_find(&g_g[gi].L, first + (uint32_t)i * step) >= 0) return false;
    return true;
}
// run games 0..ng-1 for `ticks` ticks of dt ms from t0, delivering after each game's tick
static void g_run(int ng, uint64_t t0, int ticks, uint64_t dt, int budget) {
    for (int t = 0; t < ticks; t++)
        for (int gi = 0; gi < ng; gi++) g_tick(gi, ng, t0 + dt * (uint64_t)t, false, budget), g_deliver(0);
}

// no loss: B's 1000 kills reach A in ks_parts(1000) parts; nothing re-asked; B's own list unchanged
static void test_catchup_clean(void) {
    g_reset(2);
    g_fill(1, 0x20000001u, 1000, 3);
    int np = ks_parts(1000);
    CHECK(np == 10);
    uint64_t T = 50000;
    g_ask(0, 1, T);
    for (int t = 0; t < 10; t++) g_tick(0, 2, T + 16 * t, false, 4), g_tick(1, 2, T + 16 * t, false, 4), g_deliver(0);
    CHECK(g_g[0].ask[1].done && !g_g[0].ask[1].active && g_g[0].st.done == 1 && g_g[0].st.take == np && g_g[0].ask[1].n == np);
    CHECK(g_g[0].L.n == 1000 && g_has_all(0, 0x20000001u, 1000, 3) && g_g[0].L.nwait == 1000 && g_g[0].L.nsrc[KSRC_CATCH] == 1000);
    CHECK(g_g[0].st.ask_tx == 1 && g_g[1].st.part_tx == np && g_g[0].st.dup == 0 && g_g[0].st.stale == 0 && g_g[1].L.n == 1000);
    for (int t = 0; t < 400; t++) g_tick(0, 2, T + 1000 + 50 * t, false, 4), g_tick(1, 2, T + 1000 + 50 * t, false, 4), g_deliver(0);
    CHECK(g_g[0].st.ask_tx == 1 && g_g[1].st.part_tx == np);  // (done: never asked again, no timeout)
    // an empty list: one part, k 0, done
    g_reset(2);
    g_ask(0, 1, T);
    for (int t = 0; t < 3; t++) g_tick(0, 2, T + t, false, 4), g_tick(1, 2, T + t, false, 4), g_deliver(0);
    CHECK(g_g[0].ask[1].done && g_g[0].ask[1].n == 1 && g_g[1].st.part_tx == 1 && g_g[0].L.n == 0);
}

// lost parts: the re-ask carries the parts we have and only the missing ones come again; a lost ask is sent again too
static void test_catchup_loss(void) {
    g_reset(2);
    g_fill(1, 0x30000001u, 1000, 5);
    uint64_t T = 70000;
    g_drop_part[2] = 1, g_drop_part[5] = 1;
    g_ask(0, 1, T);
    for (int t = 0; t < 10; t++) g_tick(0, 2, T + 16 * t, false, 4), g_tick(1, 2, T + 16 * t, false, 4), g_deliver(0);
    CHECK(!g_g[0].ask[1].done && g_g[0].st.take == 8 && g_g[0].L.n == 1000 - 2 * NET_KILLS_CHUNK && g_g[1].st.part_tx == 10);
    CHECK(ks_ask_have(&g_g[0].ask[1]) == (ks_all(10) & ~((1ull << 2) | (1ull << 5))));
    // no re-ask before KS_REASK_MS
    for (int t = 0; t < 5; t++) g_tick(0, 2, T + KS_REASK_MS - 10 + t, false, 4), g_tick(1, 2, T + KS_REASK_MS - 10 + t, false, 4), g_deliver(0);
    CHECK(g_g[0].st.ask_tx == 1);
    for (int t = 0; t < 5; t++) g_tick(0, 2, T + KS_REASK_MS + 16 * t, false, 4), g_tick(1, 2, T + KS_REASK_MS + 16 * t, false, 4), g_deliver(0);
    CHECK(g_g[0].ask[1].done && g_g[0].st.ask_tx == 2 && g_g[1].st.part_tx == 12 && g_g[0].L.n == 1000 && g_has_all(0, 0x30000001u, 1000, 5));
    CHECK(g_g[0].st.dup == 0 && g_g[0].st.stale == 0 && g_g[1].st.later == 0);  // (a re-ask of the same snapshot is never "too soon")
    // the first ask lost, then part 0 lost twice: re-asks until it is in
    g_reset(2);
    g_fill(1, 0x31000001u, 300, 1);
    g_drop_asks = 1, g_drop_part[0] = 2;
    g_ask(0, 1, T);
    for (int t = 0; t < 400; t++) {
        uint64_t now = T + 20 * t;
        g_tick(0, 2, now, false, 4), g_tick(1, 2, now, false, 4), g_deliver(0);
    }
    CHECK(g_g[0].ask[1].done && g_g[0].st.ask_tx == 4 && g_g[0].L.n == 300 && g_has_all(0, 0x31000001u, 300, 1));
    CHECK(g_g[1].st.part_tx == 3 + 1 + 1 && g_g[0].st.dup == 0);  // (ask 2: all 3 parts (0 lost), ask 3: part 0 (lost), ask 4: part 0)
}

// reordered and doubled parts: all in, each counted once; the budget paces the parts over ticks
static void test_catchup_reorder_dup(void) {
    g_reset(2);
    g_fill(1, 0x40000001u, 1500, 11);
    uint64_t T = 90000;
    g_ask(0, 2, T);  // (no game 2 here: nothing)
    g_ask(0, 1, T);
    g_tick(0, 2, T, false, 4), g_deliver(0);
    for (int t = 0; t < 4; t++) g_tick(1, 2, T + 1 + t, false, 4);  // (15 parts, 4 a tick: 4 + 4 + 4 + 3, all queued, then delivered backwards)
    CHECK(g_g[1].st.part_tx == 15 && g_nq == 15);
    g_deliver(1);
    CHECK(g_g[0].ask[1].done && g_g[0].L.n == 1500 && g_has_all(0, 0x40000001u, 1500, 11) && g_g[0].st.dup == 0);
    // every packet twice (a stall's backlog / a repeat)
    g_reset(2);
    g_fill(1, 0x41000001u, 700, 1);
    g_ask(0, 1, T);
    for (int t = 0; t < 5; t++) g_tick(0, 2, T + t, false, 2), g_tick(1, 2, T + t, false, 2), g_deliver(2);  // (7 parts, 2 a tick from tick 1)
    CHECK(g_g[0].ask[1].done && g_g[0].L.n == 700 && g_g[0].st.take == 7 && g_g[0].st.dup == 7);
    // a late copy after done = dup, never taken again
    g_q[0].from = 1, g_q[0].to = 0;
    uint32_t h1 = 0x41000001u;
    uint16_t a1 = 0;
    g_q[0].len = net_pack_kills_part(1, 77, g_g[0].ask[1].nonce, g_g[0].ask[1].tag, 0, g_g[0].ask[1].n, &h1, &a1, 1, g_q[0].b, NET_KILLS_PKT), g_nq = 1;
    g_deliver(0);
    CHECK(g_g[0].st.dup == 8 && g_g[0].L.n == 700);
}

// a load clears my list and asks again (a new nonce): parts of the old ask are stale, never taken; (s197 S3) his new ask inside
// KS_ANS_NEW_MS of the last snapshot is refused (LATER), the one after it is answered
static void test_stale_after_load(void) {
    g_reset(2);
    g_fill(1, 0x50000001u, 400, 1);
    uint64_t T = 110000;
    g_ask(0, 1, T);
    g_tick(0, 2, T, false, 4), g_deliver(0);
    g_tick(1, 2, T + 1, false, 1);  // (part 0 of 4 out, queued)
    uint32_t n1 = g_g[0].ask[1].nonce;
    // A loads a save: list cleared, asks again
    ks_clear(&g_g[0].L);
    g_ask(0, 1, T + 2);
    CHECK(g_g[0].ask[1].nonce != n1);
    g_tick(1, 2, T + 3, false, 4);  // (the old answer's parts 1..3)
    g_deliver(0);
    CHECK(g_g[0].st.stale == 4 && g_g[0].L.n == 0 && !g_g[0].ask[1].done);
    // the new ask comes 3 ms after B's snapshot: too soon (no new snapshot, nothing sent)
    g_fill(1, 0x51000001u, 10, 1);
    g_tick(0, 2, T + 4, false, 4), g_deliver(0);
    g_tick(1, 2, T + 5, false, 4), g_deliver(0);
    CHECK(g_g[1].st.later == 1 && g_nq == 0 && !g_g[0].ask[1].done && g_g[1].ans[0].nonce == n1);
    // A's re-ask KS_REASK_MS later is still too soon; the next one is answered from a NEW snapshot (B's list grew meanwhile)
    for (uint64_t t = T + 6; t < T + 3 * KS_REASK_MS; t += 16) g_tick(0, 2, t, false, 4), g_deliver(0), g_tick(1, 2, t, false, 4), g_deliver(0);
    CHECK(g_g[0].ask[1].done && g_g[0].L.n == 410 && g_g[0].st.stale == 4 && g_g[1].st.later == 2 && g_g[0].st.ask_tx == 4);
    // an abandoned (stopped) ask: its parts are stale too
    g_reset(2);
    g_fill(1, 0x52000001u, 5, 1);
    g_ask(0, 1, T);
    g_tick(0, 2, T, false, 4), g_deliver(0);
    g_g[0].ask[1].active = false;  // (the glue: killsclear / a Blood Moon stop every ask)
    g_tick(1, 2, T + 1, false, 4), g_deliver(0);
    CHECK(g_g[0].st.stale == 1 && g_g[0].L.n == 0);
    // a part with a nonce nobody asked = stale; a part before any ask = stale
    g_reset(2);
    uint32_t hh = 5;
    g_q[0].from = 1, g_q[0].to = 0, g_q[0].len = net_pack_kills_part(1, 1, 0x1234, 1, 0, 1, &hh, NULL, 1, g_q[0].b, NET_KILLS_PKT), g_nq = 1;
    g_deliver(0);
    CHECK(g_g[0].st.stale == 1 && g_g[0].L.n == 0);
}

// the answerer's list is cleared under its answer (its Blood Moon / its load): the rest of the old snapshot is not sent; the asker's
// re-ask gets the new snapshot (another tag, once KS_ANS_NEW_MS has passed) and starts over on it
static void test_answerer_cleared(void) {
    g_reset(2);
    g_fill(1, 0x60000001u, 800, 1);
    uint64_t T = 130000;
    g_ask(0, 1, T);
    g_tick(0, 2, T, false, 4), g_deliver(0);
    g_tick(1, 2, T + 1, false, 2), g_deliver(0);  // (parts 0, 1 of 8 in)
    CHECK(g_g[0].st.take == 2 && g_g[0].L.n == 2 * NET_KILLS_CHUNK);
    uint16_t tag1 = g_g[0].ask[1].tag;
    ks_clear(&g_g[1].L);
    g_fill(1, 0x61000001u, 3, 1);  // (B's new list: 3 kills after its Blood Moon)
    g_tick(1, 2, T + 2, false, 4);
    CHECK(g_g[1].st.aborted == 1 && g_nq == 0);
    for (int t = 0; t < 4; t++) g_tick(0, 2, T + KS_REASK_MS + t, false, 4), g_tick(1, 2, T + KS_REASK_MS + t, false, 4), g_deliver(0);
    CHECK(!g_g[0].ask[1].done && g_g[1].st.later == 1);  // (1.5 s after the first snapshot: too soon for a new one)
    for (int t = 0; t < 4; t++) g_tick(0, 2, T + 2 * KS_REASK_MS + t, false, 4), g_tick(1, 2, T + 2 * KS_REASK_MS + t, false, 4), g_deliver(0);
    CHECK(g_g[0].ask[1].done && g_g[0].ask[1].tag != tag1 && g_g[0].ask[1].n == 1 && g_g[0].L.n == 2 * NET_KILLS_CHUNK + 3);  // (the ones it had stay)
    CHECK(g_g[0].ask[1].restarts == 1);
    // the same ask again, same snapshot, part bitmap full: nothing to send (a repeat is served at any rate)
    KsAns *s = &g_g[1].ans[0];
    uint64_t now = T + 2 * KS_REASK_MS + 10;
    CHECK(ks_ans_ask(s, s->nonce, s->tag, 1, NET_KILLS_MOON_NONE, &g_g[1].L, &g_g[1].tagc, now) == KSAN_SAME && !s->active && s->todo == 0);
    // the same ask, but with a stale tag in it: everything again (the asker's bitmap is of another snapshot)
    CHECK(ks_ans_ask(s, s->nonce, (uint16_t)(s->tag + 1), 1, NET_KILLS_MOON_NONE, &g_g[1].L, &g_g[1].tagc, now) == KSAN_SAME && s->active && s->todo == 1);
    // the tag counter never hands out 0
    uint16_t tc = 0xFFFF;
    KsAns s2;
    memset(&s2, 0, sizeof s2);
    CHECK(ks_ans_ask(&s2, 9, 0, 0, NET_KILLS_MOON_NONE, &g_g[1].L, &tc, now) == KSAN_NEW);
    CHECK(s2.tag == 1 && tc == 1);
}

// a join: both games ask each other; each ends with the union; a third game joining later gets the union from either
static void test_union_join(void) {
    g_reset(3);
    g_fill(0, 0x70000001u, 500, 1);   // A: 500
    g_fill(1, 0x70000101u, 500, 1);   // B: 500, 244 of them A's too (0x70000101..0x700001F4)
    uint64_t T = 150000;
    g_ask(0, 1, T), g_ask(1, 0, T);
    for (int t = 0; t < 10; t++) g_tick(0, 2, T + t, false, 4), g_tick(1, 2, T + t, false, 4), g_deliver(0);
    int uni = 0x100 + 500;  // 0x70000001 .. 0x700002F4 = 756 (244 on both)
    CHECK(g_g[0].ask[1].done && g_g[1].ask[0].done);
    CHECK(g_g[0].L.n == uni && g_g[1].L.n == uni && g_has_all(0, 0x70000001u, uni, 1) && g_has_all(1, 0x70000001u, uni, 1));
    CHECK(g_g[0].L.nsrc[KSRC_OWN] == 500 && g_g[0].L.nsrc[KSRC_CATCH] == (uint32_t)(uni - 500));
    // C joins: it asks A and B, they ask C (empty): C has the union, A / B unchanged
    g_ask(2, 0, T + 100), g_ask(2, 1, T + 100), g_ask(0, 2, T + 100), g_ask(1, 2, T + 100);
    for (int t = 0; t < 10; t++)
        for (int gi = 0; gi < 3; gi++) g_tick(gi, 3, T + 100 + t, false, 4), g_deliver(0);
    CHECK(g_g[2].ask[0].done && g_g[2].ask[1].done && g_g[0].ask[2].done && g_g[1].ask[2].done);
    CHECK(g_g[2].L.n == uni && g_has_all(2, 0x70000001u, uni, 1) && g_g[0].L.n == uni && g_g[1].L.n == uni);
}

// a peer that never answers (an old build): the ask goes out every KS_REASK_MS, given up once at KS_ASK_TIMEOUT_MS; Melty: KS_REASK_MELTY_MS
static void test_timeout_and_melty(void) {
    KsAsk a;
    uint64_t T = 1000000;
    ks_ask_start(&a, 0x99, T);
    int sends = 0, touts = 0;
    for (uint64_t now = T; now < T + KS_ASK_TIMEOUT_MS + 5000; now += 10) {
        int d = ks_ask_step(&a, now, false);
        sends += d == KSD_SEND, touts += d == KSD_TIMEOUT;
    }
    CHECK(touts == 1 && sends == (KS_ASK_TIMEOUT_MS - 1) / KS_REASK_MS + 1 && !a.active && !a.done);
    CHECK(ks_ask_part(&a, 0x99, 1, 0, 1) == KSP_STALE);  // (a late answer after the timeout is not taken)
    ks_ask_start(&a, 0x9a, T);
    sends = touts = 0;
    for (uint64_t now = T; now < T + KS_ASK_TIMEOUT_MS + 5000; now += 10) {
        int d = ks_ask_step(&a, now, true);
        sends += d == KSD_SEND, touts += d == KSD_TIMEOUT;
    }
    CHECK(touts == 1 && sends == (KS_ASK_TIMEOUT_MS - 1) / KS_REASK_MELTY_MS + 1);
    // Melty: a room delivers in order, nothing lost: ONE ask, every part once
    g_reset(2);
    g_fill(1, 0x80000001u, 2000, 1);
    g_ask(0, 1, T);
    for (int t = 0; t < 20; t++) g_tick(0, 2, T + 16 * t, true, 4), g_tick(1, 2, T + 16 * t, true, 4), g_deliver(0);
    CHECK(g_g[0].ask[1].done && g_g[0].st.ask_tx == 1 && g_g[1].st.part_tx == ks_parts(2000) && ks_parts(2000) == 19 && g_g[0].L.n == 2000);
    // a never-started ask does nothing; a done one neither; nonce 0 -> 1
    KsAsk z;
    memset(&z, 0, sizeof z);
    CHECK(ks_ask_step(&z, T, false) == KSD_NONE && ks_ask_part(&z, 0, 1, 0, 1) == KSP_STALE);
    ks_ask_start(&z, 0, T);
    CHECK(z.nonce == 1);
    CHECK(ks_ask_part(&z, 1, 0, 0, 1) == KSP_BAD && ks_ask_part(&z, 1, 3, 1, 1) == KSP_BAD && ks_ask_part(&z, 1, 3, 0, 65) == KSP_BAD);
    CHECK(ks_ask_part(&z, 1, 3, 0, 2) == KSP_TAKE && ks_ask_part(&z, 1, 3, 1, 3) == KSP_BAD && ks_ask_part(&z, 1, 3, 1, 2) == KSP_DONE);
    CHECK(z.done && ks_ask_step(&z, T + 99999, false) == KSD_NONE && ks_ask_part(&z, 1, 4, 0, 1) == KSP_STALE && ks_ask_part(&z, 1, 3, 0, 2) == KSP_DUP);
    for (int i = 0; i < 50; i++) CHECK(ks_nonce((uint64_t)i * 977, (uint32_t)i) != 0);
    CHECK(ks_nonce(5, 1) != ks_nonce(5, 2) && ks_nonce(5, 1) != ks_nonce(6, 1));
}

// a FULL list (KS_CAP): 64 parts, all of it goes out; (s197 S4) ONE answer adds at most KS_ASK_NEW_MAX new kills (the rest is
// refused: those enemies stay alive), and the receiver's own full list refuses the rest and counts it
static void test_full_answer(void) {
    g_reset(2);
    for (uint32_t i = 0; i < KS_CAP; i++) ks_add(&g_g[1].L, 0x90000000u + i, KSRC_OWN, 1, 1);
    uint64_t T = 2000000;
    g_ask(0, 1, T);
    for (int t = 0; t < 30; t++) g_tick(0, 2, T + t, false, 4), g_tick(1, 2, T + t, false, 4), g_deliver(0);
    CHECK(g_g[0].ask[1].done && g_g[0].ask[1].n == 64 && g_g[1].st.part_tx == 64 && g_g[0].L.n == KS_ASK_NEW_MAX && g_g[0].L.full == 0);
    CHECK(g_g[0].ask[1].capped && g_g[0].ask[1].refused == (uint32_t)(KS_CAP - KS_ASK_NEW_MAX) && g_g[0].ask[1].newk == KS_ASK_NEW_MAX);
    CHECK(ks_find(&g_g[0].L, 0x90000000u) == 0 && ks_find(&g_g[0].L, 0x90000000u + KS_ASK_NEW_MAX) < 0);
    // A had 100 others already; B and C each bring KS_ASK_NEW_MAX new ones: the last 100 are refused (full), counted
    g_reset(3);
    for (uint32_t i = 0; i < KS_ASK_NEW_MAX; i++) ks_add(&g_g[1].L, 0x90000000u + i, KSRC_OWN, 1, 1), ks_add(&g_g[2].L, 0x98000000u + i, KSRC_OWN, 1, 1);
    g_fill(0, 0xA0000000u, 100, 1);
    g_ask(0, 1, T), g_ask(0, 2, T);
    g_run(3, T, 30, 1, 4);
    CHECK(g_g[0].ask[1].done && g_g[0].ask[2].done && g_g[0].L.n == KS_CAP && g_g[0].L.full == 100 && ks_find(&g_g[0].L, 0xA0000000u) == 0);
    CHECK(!g_g[0].ask[1].capped && !g_g[0].ask[2].capped);
}

// ---- (s197 M1) Blood Moons over plain UDP: every game has its own ----
// A has a moon at Tm, B does not. A reloads -> gets only B's kills after Tm; B reloads -> gets A's (all after A's moon: A's list
// was cleared at it). A buggy / hostile B that sends everything anyway: A's own receive filter drops the pre-moon ones.
static void test_moon_two_games(void) {
    g_reset(2);
    uint64_t T0 = 3000000, Tm = T0 + 600000;  // (A's Blood Moon 10 min in)
    g_fill_at(0, 0xB1000001u, 40, 1, (int64_t)T0 + 1000);  // (both killed these early on)
    g_fill_at(1, 0xB1000001u, 40, 1, (int64_t)T0 + 1000);
    g_fill_at(1, 0xB2000001u, 30, 1, (int64_t)Tm - 60000);  // (B: a minute before A's moon)
    g_moon(0, Tm);                                          // (A: Blood Moon, list cleared, its enemies respawn)
    CHECK(g_g[0].L.n == 0 && g_g[1].L.n == 70);
    g_fill_at(0, 0xB3000001u, 5, 1, (int64_t)Tm + 30000);   // (A after its moon)
    g_fill_at(1, 0xB4000001u, 20, 1, (int64_t)Tm + 60000);  // (B a minute after A's moon)
    g_fill_at(1, 0xB5000001u, 3, 1, (int64_t)Tm + 1000);    // (B 1 s after A's moon: inside the slack, not sent = the safe side)
    // A's Sora dies, A Continues: list cleared again, asks B with "my moon was N s ago"
    uint64_t T1 = Tm + 120000;
    ks_clear(&g_g[0].L);
    g_ask(0, 1, T1);
    g_run(2, T1, 10, 16, 4);
    CHECK(g_g[0].ask[1].done && g_g[0].L.n == 20 && g_has_all(0, 0xB4000001u, 20, 1));
    CHECK(g_has_none(0, 0xB1000001u, 40, 1) && g_has_none(0, 0xB2000001u, 30, 1) && g_has_none(0, 0xB5000001u, 3, 1));
    CHECK(g_g[1].ans[0].sendable == 20 && g_g[1].ans[0].count == 93 && g_g[0].st.old == 0);  // (B held them back; A dropped nothing)
    // their ages came across: A's kill times are B's, within the 1 s rounding (to the older side) + the 16 ms ticks
    int i0 = ks_find(&g_g[0].L, 0xB4000001u);
    CHECK(i0 >= 0 && g_g[0].L.kt[i0] <= (int64_t)Tm + 60000 && g_g[0].L.kt[i0] >= (int64_t)Tm + 60000 - 1000 - 200);
    // B reloads: B saw no moon (NONE): it gets A's list, all of it (A's list holds only kills after A's moon)
    ks_clear(&g_g[1].L);
    g_ask(1, 0, T1 + 5000);
    g_run(2, T1 + 5000, 10, 16, 4);
    CHECK(g_g[1].ask[0].done && g_g[1].L.n == 20 && g_has_all(1, 0xB4000001u, 20, 1));
    // a later Blood Moon in B too, then B reloads: A hands back only what came after B's moon (none of the old ones)
    uint64_t Tb = T1 + 600000;
    g_moon(1, Tb);
    g_fill_at(0, 0xB6000001u, 4, 1, (int64_t)Tb + 50000);   // (A, after B's moon)
    ks_clear(&g_g[1].L);
    g_ask(1, 0, Tb + 100000);
    g_run(2, Tb + 100000, 10, 16, 4);
    CHECK(g_g[1].ask[0].done && g_g[1].L.n == 4 && g_has_all(1, 0xB6000001u, 4, 1) && g_has_none(1, 0xB4000001u, 20, 1));
    // a buggy / hostile answerer that ignores the cut: the asker's own filter drops what is not after its moon
    g_reset(2);
    g_g[0].moon_seen = true, g_g[0].moon_t = Tm;
    g_fill_at(1, 0xB7000001u, 50, 1, (int64_t)Tm - 5000), g_fill_at(1, 0xB8000001u, 7, 1, (int64_t)Tm + 9000), g_fill_at(1, 0xB9000001u, 2, 1, OLD);
    g_g[1].ignore_cut = true;
    g_ask(0, 1, Tm + 20000);
    g_run(2, Tm + 20000, 10, 16, 4);
    CHECK(g_g[0].ask[1].done && g_g[0].L.n == 7 && g_has_all(0, 0xB8000001u, 7, 1) && g_g[0].st.old == 52 && g_g[0].ask[1].old == 52);
}

// an answer that crosses the asker's moon: A asks with "no moon seen", B answers part by part; A's Blood Moon plays in between
// (the glue stops A's asks there: the late parts are stale); and even if A kept taking them, its receive filter drops the old kills
static void test_moon_mid_answer(void) {
    g_reset(2);
    uint64_t T = 4000000;
    g_fill_at(1, 0xC1000001u, 3 * NET_KILLS_CHUNK, 1, (int64_t)T - 30000);  // (B: 3 parts of kills, all 30 s old)
    g_ask(0, 1, T);
    g_tick(0, 2, T, false, 4), g_deliver(0);
    g_tick(1, 2, T + 1, false, 1), g_deliver(0);  // (part 0 in: before A's moon, fine)
    CHECK(g_g[0].L.n == NET_KILLS_CHUNK);
    // A's moon: (glue) the list goes, the ask stops: parts 1, 2 are stale
    g_moon(0, T + 2);
    g_tick(1, 2, T + 3, false, 4), g_deliver(0);
    CHECK(g_g[0].L.n == 0 && g_g[0].st.stale == 2);
    // the same, but A's ask is NOT stopped (an answerer slow enough that A asked again after its moon with the same nonce): dropped
    g_reset(2);
    g_fill_at(1, 0xC1000001u, 3 * NET_KILLS_CHUNK, 1, (int64_t)T - 30000);
    g_ask(0, 1, T);
    g_tick(0, 2, T, false, 4), g_deliver(0);
    g_tick(1, 2, T + 1, false, 1), g_deliver(0);
    ks_clear(&g_g[0].L), g_g[0].moon_seen = true, g_g[0].moon_t = T + 2;  // (the moon, ask left running)
    g_tick(1, 2, T + 3, false, 4), g_deliver(0);
    CHECK(g_g[0].ask[1].done && g_g[0].L.n == 0 && g_g[0].st.old == 2 * NET_KILLS_CHUNK);
    // and its re-ask now says the moon: B's snapshot cut moves to it (a later part would carry only kills after A's moon)
    KsAns *s = &g_g[1].ans[0];
    CHECK(s->cut == KS_CUT_ALL);
    CHECK(ks_ans_ask(s, s->nonce, s->tag, 0, ks_moon_s(true, T + 2, T + 4000), &g_g[1].L, &g_g[1].tagc, T + 4000) == KSAN_SAME && s->cut != KS_CUT_ALL);
    CHECK(s->cut >= (int64_t)T + 2 + KS_MOON_SLACK_MS && s->cut < (int64_t)T + 2 + KS_MOON_SLACK_MS + 1000);  // (whole seconds: up to 1 s later)
    int i, k, ab;
    uint32_t h[NET_KILLS_CHUNK];
    uint16_t ag[NET_KILLS_CHUNK];
    CHECK(ks_ans_next(s, &g_g[1].L, T + 4000, &i, h, ag, &k, &ab) == 1 && k == 0);  // (part 0 again: none of its kills is after the moon)
    // a NONE ask later on never moves the cut back to "everything"
    CHECK(ks_ans_ask(s, s->nonce, s->tag, 0, NET_KILLS_MOON_NONE, &g_g[1].L, &g_g[1].tagc, T + 5000) == KSAN_SAME && s->cut != KS_CUT_ALL);
}

// three games: A's kills reach B by catch-up (B saw no moon), then C (moon at Tc) asks B: B forwards A's kills with their TRUE
// age (not "received now"), so C gets only those made after its moon, even though B got all of them after it
static void test_moon_three_games(void) {
    g_reset(3);
    uint64_t T0 = 5000000, Tc = T0 + 300000;
    g_fill_at(0, 0xD1000001u, 25, 1, (int64_t)Tc - 10000);  // (A: 10 s before C's moon)
    g_fill_at(0, 0xD2000001u, 15, 1, (int64_t)Tc + 30000);  // (A: 30 s after it)
    g_fill_at(0, 0xD3000001u, 2, 1, OLD);                   // (A: not known how old)
    g_g[2].moon_seen = true, g_g[2].moon_t = Tc;
    // B joins A a minute after C's moon (B: no moon seen = everything)
    uint64_t T1 = Tc + 60000;
    g_ask(1, 0, T1);
    g_run(2, T1, 10, 16, 4);
    CHECK(g_g[1].ask[0].done && g_g[1].L.n == 42);
    int ib = ks_find(&g_g[1].L, 0xD1000001u), io = ks_find(&g_g[1].L, 0xD3000001u);
    CHECK(ib >= 0 && g_g[1].L.kt[ib] <= (int64_t)Tc - 10000 && g_g[1].L.kt[ib] >= (int64_t)Tc - 10000 - 1200 && io >= 0 && g_g[1].L.kt[io] == OLD);
    // C asks B two minutes later: only A's post-moon 15, with ages that still say "30 s after C's moon" (+ the rounding)
    uint64_t T2 = Tc + 180000;
    g_ask(2, 1, T2);
    g_run(3, T2, 10, 16, 4);
    CHECK(g_g[2].ask[1].done && g_g[2].L.n == 15 && g_has_all(2, 0xD2000001u, 15, 1) && g_has_none(2, 0xD1000001u, 25, 1) && g_has_none(2, 0xD3000001u, 2, 1));
    int ic = ks_find(&g_g[2].L, 0xD2000001u);
    CHECK(ic >= 0 && g_g[2].L.kt[ic] <= (int64_t)Tc + 30000 && g_g[2].L.kt[ic] >= (int64_t)Tc + 30000 - 2 * 1000 - 400);  // (two hops: <= 1 s each)
    // a kill handed on twice is never made younger by the hops: at most the transit (here 0) younger, up to 1 s per hop older
    g_ask(0, 2, T2 + 10000);
    g_run(3, T2 + 10000, 10, 16, 4);
    CHECK(g_g[0].ask[2].done && g_g[0].L.n == 42);  // (A has them all already: dups, its own times stay the newest)
    int ia = ks_find(&g_g[0].L, 0xD2000001u);
    CHECK(ia >= 0 && g_g[0].L.kt[ia] == (int64_t)Tc + 30000);
}

// saturated / not-known ages over the wire: a 20 h old kill and a not-known one go only to an asker that saw no moon; an asker
// whose moon is 0xFFFE s or more ago still gets the ones younger than 0xFFFE s
static void test_moon_saturated(void) {
    g_reset(2);
    uint64_t T = 200000000;  // (55.5 h of uptime)
    g_fill_at(1, 0xE1000001u, 3, 1, (int64_t)T - 72000000);  // (20 h old: OLD on the wire)
    g_fill_at(1, 0xE2000001u, 2, 1, OLD);
    g_fill_at(1, 0xE3000001u, 4, 1, (int64_t)T - 3600000);   // (1 h old)
    g_ask(0, 1, T);
    g_run(2, T, 10, 16, 4);
    CHECK(g_g[0].ask[1].done && g_g[0].L.n == 9);  // (no moon here: everything)
    int i = ks_find(&g_g[0].L, 0xE1000001u);
    CHECK(i >= 0 && g_g[0].L.kt[i] == OLD && g_g[0].L.kt[ks_find(&g_g[0].L, 0xE2000001u)] == OLD);
    // an asker whose moon was 30 h ago (saturated 0xFFFE): only the 1 h old ones (the 20 h old ones are OLD now: unknown = never)
    g_reset(2);
    g_fill_at(1, 0xE1000001u, 3, 1, (int64_t)T - 72000000), g_fill_at(1, 0xE2000001u, 2, 1, OLD), g_fill_at(1, 0xE3000001u, 4, 1, (int64_t)T - 3600000);
    g_g[0].moon_seen = true, g_g[0].moon_t = T - 108000000;
    g_ask(0, 1, T);
    g_run(2, T, 10, 16, 4);
    CHECK(g_g[0].ask[1].done && g_g[0].L.n == 4 && g_has_all(0, 0xE3000001u, 4, 1) && g_g[0].st.old == 0);
    // an asker whose moon was 5 h ago: the same 4 (and the receive filter agrees)
    g_reset(2);
    g_fill_at(1, 0xE1000001u, 3, 1, (int64_t)T - 72000000), g_fill_at(1, 0xE3000001u, 4, 1, (int64_t)T - 3600000);
    g_g[0].moon_seen = true, g_g[0].moon_t = T - 18000000;
    g_g[1].ignore_cut = true;
    g_ask(0, 1, T);
    g_run(2, T, 10, 16, 4);
    CHECK(g_g[0].ask[1].done && g_g[0].L.n == 4 && g_g[0].st.old == 3);
}

// ---- (s197 S3 / S4) a flood of asks, crafted answers ----
static void test_ask_flood_and_caps(void) {
    KsList *L = &g_l;
    ks_clear(L);
    for (uint32_t i = 0; i < 300; i++) ks_add(L, 0xF0000001u + i, KSRC_OWN, 1, 1);
    // S3: asks with a new nonce every packet: ONE new snapshot per KS_ANS_NEW_MS, the rest LATER (nothing changes for them)
    KsAns s;
    memset(&s, 0, sizeof s);
    uint16_t tc = 0;
    uint64_t T = 6000000;
    int nw = 0, lt = 0;
    for (int j = 0; j < 600; j++) {  // (10 s of 60 asks a second)
        int r = ks_ans_ask(&s, 0x1000u + (uint32_t)j, 0, 0, NET_KILLS_MOON_NONE, L, &tc, T + (uint64_t)j * 16);
        nw += r == KSAN_NEW, lt += r == KSAN_LATER;
    }
    CHECK(nw == (600 * 16 + KS_ANS_NEW_MS - 1) / KS_ANS_NEW_MS && nw <= 5 && lt == 600 - nw && tc == nw);
    // the current snapshot's re-asks are always served (SAME), also inside the window
    uint32_t cur = s.nonce;
    for (int j = 0; j < 5; j++) CHECK(ks_ans_ask(&s, cur, s.tag, 1, NET_KILLS_MOON_NONE, L, &tc, s.t_new + 1 + (uint64_t)j) == KSAN_SAME && s.todo == (ks_all(s.n) & ~1ull));
    // a clock behind the last snapshot (a reset tick count): not stuck
    CHECK(ks_ans_ask(&s, 0x9999, 0, 0, NET_KILLS_MOON_NONE, L, &tc, 5) == KSAN_NEW);
    // S4: snapshot restarts: one tag change is followed (a clear there), the second ends the ask
    KsAsk a;
    ks_ask_start(&a, 0x77, T);
    CHECK(ks_ask_part(&a, 0x77, 10, 0, 3) == KSP_TAKE && ks_ask_part(&a, 0x77, 11, 0, 3) == KSP_TAKE && a.restarts == 1 && a.got == 1);
    CHECK(ks_ask_part(&a, 0x77, 12, 0, 3) == KSP_CAP && a.capped && !a.active && !a.done && ks_ask_part(&a, 0x77, 11, 1, 3) == KSP_STALE);
    // S4: the parts cap (a part bitmap never takes more than 64 anyway: this is the belt and braces with a restart)
    ks_ask_start(&a, 0x78, T);
    for (int i = 0; i < 63; i++) CHECK(ks_ask_part(&a, 0x78, 20, i, 64) == KSP_TAKE);
    CHECK(ks_ask_part(&a, 0x78, 21, 0, 64) == KSP_TAKE && a.restarts == 1);  // (restart: 63 + 1 so far)
    for (int i = 1; i < 64; i++) CHECK(ks_ask_part(&a, 0x78, 21, i, 64) == (i == 63 ? KSP_DONE : KSP_TAKE));
    CHECK(a.done && a.parts_in == 2 * 64 - 1 && a.parts_in <= KS_ASK_PARTS_MAX);
    // S4: new kills per ask (ks_take): past KS_ASK_NEW_MAX new ones are refused, duplicates still update
    ks_clear(L);
    ks_ask_start(&a, 0x79, T);
    static uint32_t hh[NET_KILLS_CHUNK];
    static uint16_t ag[NET_KILLS_CHUNK];
    int taken = 0, old = 0, full = 0;
    for (int p = 0; p < 64; p++) {
        for (int j = 0; j < NET_KILLS_CHUNK; j++) hh[j] = 0x01000000u + (uint32_t)(p * NET_KILLS_CHUNK + j), ag[j] = 5;
        taken += ks_take(L, &a, hh, ag, NET_KILLS_CHUNK, T, KS_CUT_ALL, &old, &full);
    }
    CHECK(taken == KS_ASK_NEW_MAX && L->n == KS_ASK_NEW_MAX && a.capped && a.refused == (uint32_t)(64 * NET_KILLS_CHUNK - KS_ASK_NEW_MAX) && full == 0);
    hh[0] = 0x01000000u, ag[0] = 0;  // (one already on the list, now said to be younger: its time moves, no new entry)
    int64_t kt0 = L->kt[0];
    CHECK(ks_take(L, &a, hh, ag, 1, T + 9000, KS_CUT_ALL, &old, &full) == 0 && L->kt[0] > kt0 && L->n == KS_ASK_NEW_MAX);
    ks_clear(L);
}

int main(void) {
    CHECK(NET_KILLS_DEFAULT == true);  // (s197: killsync is compiled ON; the glue's on-by-default checks are in net_glue_test.c / net_melty_test.c)
    test_list();
    test_probe_cap();
    test_pick();
    test_ages();
    test_wire();
    test_catchup_clean();
    test_catchup_loss();
    test_catchup_reorder_dup();
    test_stale_after_load();
    test_answerer_cleared();
    test_union_join();
    test_timeout_and_melty();
    test_full_answer();
    test_moon_two_games();
    test_moon_mid_answer();
    test_moon_three_games();
    test_moon_saturated();
    test_ask_flood_and_caps();
    printf("kills_test: %d/%d passed\n", runs - fails, runs);
    return fails ? 1 : 0;
}

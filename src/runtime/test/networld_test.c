// host test for runtime/sora_net_world.h (s290 networld) + its NET_WORLD block in sora_net_proto.h: the wire (round trips,
// every refusal), the readers (Yaz0 / SARC / BYML, made here), the families + gifts, the list build, the union (two friends,
// SETs, pages, our own deltas, never unset), the Blood Moon rule, the save guard (state + a real copy into a scratch folder;
// s290 revfix: per slot (F1), torn copies (F2), the retry, no new folder for an unchanged save (F4)).
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/networld_test.c -o build_h/networld_test.exe
//   networld_test [scratch dir] [romfs dir] [dump file]   (romfs: the real 1.9.0 files are read too, counts printed; dump: one
//                                                          line "hash fam rev name" per entry, for sb/s290/networld/compare.py)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sora_net_world.h"
#pragma clang diagnostic ignored "-Wunused-function"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

// ---------------------------------------------------------------- a little BYML writer (v2, little endian)
typedef struct BN { int t; const char *s; int n; const char *keys[8]; struct BN *kid[64]; } BN;  // t: NWDB_STR / NWDB_ARR / NWDB_HASH
static BN pool[8192];
static int npool;
static BN *bs(const char *s) { BN *b = &pool[npool++]; memset(b, 0, sizeof *b); b->t = NWDB_STR, b->s = s; return b; }
static BN *ba(void) { BN *b = &pool[npool++]; memset(b, 0, sizeof *b); b->t = NWDB_ARR; return b; }
static BN *bh(void) { BN *b = &pool[npool++]; memset(b, 0, sizeof *b); b->t = NWDB_HASH; return b; }
static void push(BN *c, BN *k) { c->kid[c->n++] = k; }
static void put(BN *h, const char *k, BN *v) { h->keys[h->n] = k, h->kid[h->n++] = v; }
typedef struct { const char *v[4096]; int n; } StrSet;
static int ss_find(StrSet *s, const char *x) {
    for (int i = 0; i < s->n; i++)
        if (!strcmp(s->v[i], x)) return i;
    return -1;
}
static void ss_add(StrSet *s, const char *x) { if (ss_find(s, x) < 0) s->v[s->n++] = x; }
static int cmpstr(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }
static void collect(BN *b, StrSet *keys, StrSet *strs) {
    if (b->t == NWDB_STR) { ss_add(strs, b->s); return; }
    for (int i = 0; i < b->n; i++) {
        if (b->t == NWDB_HASH) ss_add(keys, b->keys[i]);
        collect(b->kid[i], keys, strs);
    }
}
static uint8_t *W;
static size_t WN;
static void w8(size_t at, uint8_t v) { W[at] = v; }
static void w24(size_t at, uint32_t v) { W[at] = (uint8_t)v, W[at + 1] = (uint8_t)(v >> 8), W[at + 2] = (uint8_t)(v >> 16); }
static void w32(size_t at, uint32_t v) { w24(at, v), W[at + 3] = (uint8_t)(v >> 24); }
static size_t walign(void) { while (WN & 3) W[WN++] = 0; return WN; }
static size_t wtab(StrSet *s) {
    size_t o = walign();
    w8(o, NWDB_STRTAB), w24(o + 1, (uint32_t)s->n);
    size_t at = o + 4 + 4 * ((size_t)s->n + 1);
    for (int i = 0; i < s->n; i++) {
        w32(o + 4 + 4 * (size_t)i, (uint32_t)(at - o));
        size_t l = strlen(s->v[i]) + 1;
        memcpy(W + at, s->v[i], l), at += l;
    }
    w32(o + 4 + 4 * (size_t)s->n, (uint32_t)(at - o));
    WN = at;
    return o;
}
static StrSet KS, SS;
static size_t wnode(BN *b) {  // a container -> its offset
    size_t o = walign();
    w8(o, (uint8_t)b->t), w24(o + 1, (uint32_t)b->n);
    size_t vals;
    if (b->t == NWDB_HASH) WN = o + 4 + 8 * (size_t)b->n, vals = 0;
    else WN = o + 4 + (((size_t)b->n + 3) & ~(size_t)3) + 4 * (size_t)b->n, vals = o + 4 + (((size_t)b->n + 3) & ~(size_t)3);
    for (int i = 0; i < b->n; i++) {
        BN *k = b->kid[i];
        uint32_t v = k->t == NWDB_STR ? (uint32_t)ss_find(&SS, k->s) : (uint32_t)wnode(k);
        if (b->t == NWDB_HASH) {
            size_t e = o + 4 + 8 * (size_t)i;
            w24(e, (uint32_t)ss_find(&KS, b->keys[i])), w8(e + 3, (uint8_t)k->t), w32(e + 4, v);
        } else
            w8(o + 4 + (size_t)i, (uint8_t)k->t), w32(vals + 4 * (size_t)i, v);
    }
    return o;
}
static size_t byml(BN *root, uint8_t *out) {
    memset(&KS, 0, sizeof KS), memset(&SS, 0, sizeof SS);
    collect(root, &KS, &SS);
    qsort(KS.v, (size_t)KS.n, sizeof *KS.v, cmpstr), qsort(SS.v, (size_t)SS.n, sizeof *SS.v, cmpstr);
    W = out, WN = 16;
    memset(out, 0, 16);
    out[0] = 'Y', out[1] = 'B', out[2] = 2;
    size_t k = wtab(&KS), s = wtab(&SS);
    size_t r = wnode(root);
    w32(4, (uint32_t)k), w32(8, (uint32_t)s), w32(12, (uint32_t)r);
    return walign();
}
// a SARC (little endian) of n files
static size_t sarc(const char **names, const uint8_t **data, const size_t *lens, int n, uint8_t *out) {
    memset(out, 0, 0x14 + 12 + 16 * (size_t)n + 8);
    memcpy(out, "SARC", 4), out[4] = 0x14, out[6] = 0xFF, out[7] = 0xFE, out[16] = 0, out[17] = 1;
    size_t f = 0x14;
    memcpy(out + f, "SFAT", 4), out[f + 4] = 12, out[f + 6] = (uint8_t)n, out[f + 8] = 0x65;
    size_t t = f + 12 + 16 * (size_t)n;
    memcpy(out + t, "SFNT", 4), out[t + 4] = 8;
    size_t at = t + 8, name_off[64];
    for (int i = 0; i < n; i++) {
        name_off[i] = at - (t + 8);
        size_t l = strlen(names[i]) + 1;
        memcpy(out + at, names[i], l), at += l;
        while ((at - (t + 8)) & 3) out[at++] = 0;
    }
    while (at & 0x7f) out[at++] = 0;
    size_t doff = at, beg[64];
    for (int i = 0; i < n; i++) {
        beg[i] = at - doff;
        memcpy(out + at, data[i], lens[i]), at += lens[i];
        while (at & 7) out[at++] = 0;
    }
    W = out;
    for (int i = 0; i < n; i++) {
        size_t e = f + 12 + 16 * (size_t)i;
        w32(e, 0x1234), w32(e + 4, 0x01000000u | (uint32_t)(name_off[i] / 4)), w32(e + 8, (uint32_t)beg[i]), w32(e + 12, (uint32_t)(beg[i] + lens[i]));
    }
    w32(8, (uint32_t)at), w32(12, (uint32_t)doff);
    return at;
}
static size_t yaz0_lit(const uint8_t *s, size_t n, uint8_t *out) {  // (literals only)
    memcpy(out, "Yaz0", 4);
    out[4] = (uint8_t)(n >> 24), out[5] = (uint8_t)(n >> 16), out[6] = (uint8_t)(n >> 8), out[7] = (uint8_t)n;
    memset(out + 8, 0, 8);
    size_t o = 16;
    for (size_t i = 0; i < n; i += 8) {
        out[o++] = 0xFF;
        for (size_t k = i; k < n && k < i + 8; k++) out[o++] = s[k];
    }
    return o;
}

static uint8_t g_gdt[1 << 20], g_quest[1 << 18], g_f1[1 << 18], g_f2[1 << 18], g_f3[1 << 18];
static NwdList g_L;
static const char *const BOOLS[] = {
    "IsGet_PortableUnit", "IsPlayed_Demo700_0", "MapTower_03", "MapTower_16", "Clear_Dungeon012", "Open_Dungeon012", "Enter_Dungeon7",
    "Clear_DungeonX", "MainField_Npc_HiddenKorokGround_1234567", "MainField_Npc_HiddenKorokFly_42", "MainField_TBox_Field_Iron_99",
    "IsGet_Obj_HeroSoul_Rito", "Get_HeartUtuwa_Wind", "Clear_RemainsWind", "GameClear", "Wind_Relic_Activated",
    "IsGet_Obj_RemoteBomb", "FindDungeon_Finish", "IsGet_Armor_001", "IsPlayed_Demo999_0", "Shop_Item_01_SoldOut", "GemShop_Open",
    "Npc_Talk_Done", "Kakariko_Thing", "SomeLoneFlag", "plumless", "buckeroo", "Quest_Step_A", "Quest_Dep_B", "Quest_Dep_Int",
    "IsNewPictureBook_Item_Fruit_A"};
static const char *const REVS[] = {"MainField_Item_Fruit_A_111", "AocField_Weapon_Sword_222", "CDungeon_Obj_Ore_333", "MainFieldDungeon_Barrel_444",
                                   "MainField_Kibako_555", "MarioClubTestDungeon_Item_666", "MainField_Enemy_Bokoblin_777", "MainField_Item_Fruit_A_111",
                                   "MainField_Item_AVeryLongNameThatGoesPastTheLimit_888"};
static void build_synth(void) {
    npool = 0;
    BN *r1 = bh(), *a1 = ba();
    put(r1, "bool_data", a1);
    for (size_t i = 0; i < sizeof BOOLS / sizeof *BOOLS; i++) {
        BN *e = bh();
        put(e, "DataName", bs(BOOLS[i]));
        put(e, "HashValue", bs("x"));  // (another key, ignored)
        push(a1, e);
    }
    BN *dup = bh();  // (a name twice in one file: merged)
    put(dup, "DataName", bs("GameClear"));
    push(a1, dup);
    size_t n1 = byml(r1, g_f1);
    BN *r2 = bh(), *a2 = ba();
    put(r2, "bool_data", a2);
    for (size_t i = 0; i < sizeof REVS / sizeof *REVS; i++) {
        BN *e = bh();
        put(e, "DataName", bs(REVS[i]));
        push(a2, e);
    }
    size_t n2 = byml(r2, g_f2);
    BN *r3 = bh(), *a3 = ba();  // (an s32_data file: not read)
    put(r3, "bool_data", a3);
    BN *e3 = bh();
    put(e3, "DataName", bs("ShouldNotCount"));
    push(a3, e3);
    size_t n3 = byml(r3, g_f3);
    const char *names[3] = {"/bool_data_0.bgdata", "/revival_bool_data_0.bgdata", "/s32_data_0.bgdata"};
    const uint8_t *data[3] = {g_f1, g_f2, g_f3};
    size_t lens[3] = {n1, n2, n3};
    static uint8_t raw[1 << 20];
    size_t gn = sarc(names, data, lens, 3, raw);
    (void)gn;
    memcpy(g_gdt, raw, gn);
    // QuestProduct: one Main quest (step NextFlag Quest_Step_A, bool dep Quest_Dep_B, an s32 dep Quest_Dep_Int: not taken) + a
    // "Other" type quest naming SomeLoneFlag (not taken)
    BN *q = ba(), *q1 = bh(), *st = ba(), *s1 = bh(), *dp = ba(), *d1 = bh(), *d2 = bh(), *q2 = bh(), *st2 = ba(), *s2 = bh();
    put(q1, "Type", bs("Main")), put(q1, "Steps", st), put(q1, "StepDependencyFlags", dp);
    put(s1, "NextFlag", bs("Quest_Step_A")), push(st, s1);
    put(d1, "DependencyFlag", bs("Quest_Dep_B")), put(d1, "DependencyFlagType", bs("bool")), push(dp, d1);
    put(d2, "DependencyFlag", bs("Quest_Dep_Int")), put(d2, "DependencyFlagType", bs("s32")), push(dp, d2);
    put(q2, "Type", bs("Other")), put(q2, "Steps", st2), put(s2, "NextFlag", bs("SomeLoneFlag")), push(st2, s2);
    push(q, q1), push(q, q2);
    g_quest[0] = 0;
    size_t qn = byml(q, g_quest);
    (void)qn;
    g_quest[(1 << 18) - 1] = 0;
    static size_t s_gn, s_qn;
    s_gn = gn, s_qn = qn;
    nwd_list_free(&g_L);
    int r = nwd_list_build(g_gdt, s_gn, g_quest, s_qn, &g_L);
    CHECK(r == NWDE_OK);
}
static int fam_of(const char *n) {
    int i = nwd_index_of(&g_L, lt_crc32(n));
    return i < 0 ? 0 : g_L.e[i].fam;
}

static void test_wire(void) {
    uint8_t b[NET_PKT_MAX], pl = 0;
    uint16_t sq = 0;
    static NetWorldMsg m;
    int n = net_pack_world_state(5, 77, true, true, 0xdeadbeef, 3000, 255, b, sizeof b);
    CHECK(n == NET_WORLD_STATE_LEN);
    CHECK(net_unpack_world(b, n, &pl, &sq, &m) && pl == 5 && sq == 77 && m.op == NWO_STATE && m.on && m.ask && m.fp == 0xdeadbeef &&
          m.count == 3000 && m.fams == 255);
    CHECK(net_world_fits(&m, 0xdeadbeef, 3000) && !net_world_fits(&m, 0xdeadbeee, 3000) && !net_world_fits(&m, 0xdeadbeef, 2999));
    n = net_pack_world_state(5, 78, false, false, 1, 1, 0, b, sizeof b);
    CHECK(net_unpack_world(b, n, &pl, &sq, &m) && !m.on && !m.ask);
    CHECK(net_pack_world_state(5, 1, true, false, 1, 0, 0, b, sizeof b) < 0);       // (count 0)
    CHECK(net_pack_world_state(5, 1, true, false, 1, 65536, 0, b, sizeof b) < 0);   // (count past u16)
    CHECK(net_pack_world_state(5, 1, true, false, 1, 9, 0, b, NET_WORLD_STATE_LEN - 1) < 0);  // (no room)
    n = net_pack_world_state(5, 1, true, false, 1, 9, 0, b, sizeof b);
    CHECK(!net_unpack_world(b, n - 1, &pl, &sq, &m));  // (cut short)
    CHECK(!net_unpack_world(b, n + 1, &pl, &sq, &m));  // (a byte too many)
    b[NET_HDR + 1] = 4;
    CHECK(!net_unpack_world(b, n, &pl, &sq, &m));  // (bits past on / ask)
    b[NET_HDR + 1] = 1, net_w32(b + NET_HDR + 6, 0);
    CHECK(!net_unpack_world(b, n, &pl, &sq, &m));  // (count 0 on the wire)
    net_w32(b + NET_HDR + 6, 70000);
    CHECK(!net_unpack_world(b, n, &pl, &sq, &m));
    net_w32(b + NET_HDR + 6, 9), b[NET_HDR] = 9;
    CHECK(!net_unpack_world(b, n, &pl, &sq, &m));  // (op 9)
    b[NET_HDR] = 0;
    CHECK(!net_unpack_world(b, n, &pl, &sq, &m));  // (op 0)
    b[NET_HDR] = NWO_STATE, b[3] = NET_LOOT;
    CHECK(!net_unpack_world(b, n, &pl, &sq, &m));  // (another type)
    b[3] = NET_WORLD, b[0] = 'X';
    CHECK(!net_unpack_world(b, n, &pl, &sq, &m));  // (not ours)
    CHECK(!net_unpack_world(NULL, 20, &pl, &sq, &m) && !net_unpack_world(b, NET_HDR, &pl, &sq, &m));

    // PAGE: a 12000-flag list = 3 pages (5120, 5120, 1760 -> 220 bytes)
    uint8_t bits[640];
    for (int i = 0; i < 640; i++) bits[i] = (uint8_t)(i * 7 + 1);
    CHECK(net_world_pages(12000) == 3 && net_world_page_bytes(12000, 0) == 640 && net_world_page_bytes(12000, 2) == 220 &&
          net_world_page_bytes(12000, 3) == 0 && net_world_page_bytes(0, 0) == 0 && net_world_pages(65535) == 13);
    CHECK(net_world_page_bytes(5120, 0) == 640 && net_world_pages(5120) == 1 && net_world_page_bytes(5121, 1) == 1);
    n = net_pack_world_page(2, 9, 0xabc, 4, 0, 12000, bits, b, sizeof b);
    CHECK(n == NET_WORLD_PAGE_HEAD + 640 && n <= NET_PKT_MAX);
    CHECK(net_unpack_world(b, n, &pl, &sq, &m) && m.op == NWO_PAGE && m.fp == 0xabc && m.gen == 4 && m.page == 0 && m.pages == 3 &&
          m.count == 12000 && m.nbytes == 640 && !memcmp(m.bits, bits, 640));
    CHECK(net_world_fits(&m, 0xabc, 12000) && !net_world_fits(&m, 0xabc, 12001));
    n = net_pack_world_page(2, 9, 0xabc, 4, 2, 12000, bits, b, sizeof b);
    CHECK(n == NET_WORLD_PAGE_HEAD + 220 && net_unpack_world(b, n, &pl, &sq, &m) && m.nbytes == 220 && m.page == 2);
    CHECK(net_pack_world_page(2, 9, 0xabc, 4, 3, 12000, bits, b, sizeof b) < 0);  // (no page 3)
    CHECK(net_pack_world_page(2, 9, 0xabc, 4, -1, 12000, bits, b, sizeof b) < 0);
    CHECK(net_pack_world_page(2, 9, 0xabc, 4, 0, 12000, bits, b, 100) < 0);
    CHECK(net_pack_world_page(2, 9, 0xabc, 4, 0, 12000, NULL, b, sizeof b) < 0);
    uint8_t c[NET_PKT_MAX];
    n = net_pack_world_page(2, 9, 0xabc, 4, 2, 12000, bits, b, sizeof b);
    memcpy(c, b, (size_t)n), c[NET_HDR + 7] = 3;
    CHECK(!net_unpack_world(c, n, &pl, &sq, &m));  // (page >= pages)
    memcpy(c, b, (size_t)n), c[NET_HDR + 8] = 4;
    CHECK(!net_unpack_world(c, n, &pl, &sq, &m));  // (pages not the count's)
    memcpy(c, b, (size_t)n), net_w16(c + NET_HDR + 13, 221);
    CHECK(!net_unpack_world(c, n, &pl, &sq, &m) && !net_unpack_world(c, n + 1, &pl, &sq, &m));  // (nbytes not the page's)
    memcpy(c, b, (size_t)n), c[NET_HDR + 7] = 1;
    CHECK(!net_unpack_world(c, n, &pl, &sq, &m));  // (page 1 is 640 bytes, not 220)
    memcpy(c, b, (size_t)n), net_w32(c + NET_HDR + 9, 0);
    CHECK(!net_unpack_world(c, n, &pl, &sq, &m));
    CHECK(!net_unpack_world(b, n - 1, &pl, &sq, &m) && !net_unpack_world(b, NET_HDR + 15, &pl, &sq, &m));

    // SET
    uint16_t idx[NET_WORLD_SET_MAX];
    for (int i = 0; i < NET_WORLD_SET_MAX; i++) idx[i] = (uint16_t)(i * 3);
    n = net_pack_world_set(1, 3, 0x55, idx, NET_WORLD_SET_MAX, b, sizeof b);
    CHECK(n == NET_WORLD_SET_HEAD + 2 * NET_WORLD_SET_MAX && n <= NET_PKT_MAX);
    CHECK(net_unpack_world(b, n, &pl, &sq, &m) && m.op == NWO_SET && m.fp == 0x55 && m.n == NET_WORLD_SET_MAX && m.idx[249] == 747);
    CHECK(net_world_fits(&m, 0x55, 748) && !net_world_fits(&m, 0x55, 747) && !net_world_fits(&m, 0x56, 748));
    CHECK(net_pack_world_set(1, 3, 0x55, idx, 0, b, sizeof b) < 0 && net_pack_world_set(1, 3, 0x55, idx, 251, b, sizeof b) < 0);
    n = net_pack_world_set(1, 3, 0x55, idx, 2, b, sizeof b);
    CHECK(n == NET_WORLD_SET_HEAD + 4 && net_unpack_world(b, n, &pl, &sq, &m) && m.n == 2 && m.idx[1] == 3);
    CHECK(!net_unpack_world(b, n - 1, &pl, &sq, &m) && !net_unpack_world(b, n + 2, &pl, &sq, &m));
    memcpy(c, b, (size_t)n), c[NET_HDR + 5] = 0;
    CHECK(!net_unpack_world(c, NET_WORLD_SET_HEAD, &pl, &sq, &m));  // (n 0)
    memcpy(c, b, (size_t)n), c[NET_HDR + 5] = 251;
    CHECK(!net_unpack_world(c, n, &pl, &sq, &m));
    // the generic unpack (sora_net.h's receive) refuses a bad NET_WORLD too
    int t = 0;
    CHECK(net_unpack_ex(b, n, &t, &pl, &sq, NULL, NULL, NULL, NULL) && t == NET_WORLD);
    CHECK(!net_unpack_ex(b, n - 1, &t, &pl, &sq, NULL, NULL, NULL, NULL));
}

static void test_readers(void) {
    // Yaz0: literals, a back reference (len 2..17), a long one (0x12 + n), and refusals
    const uint8_t src[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    uint8_t z[128], d[64];
    size_t zn = yaz0_lit(src, 36, z);
    CHECK(nwd_yaz0_size(z, zn) == 36 && nwd_yaz0_unpack(z, zn, d, 36) && !memcmp(d, src, 36));
    CHECK(!nwd_yaz0_unpack(z, zn - 1, d, 36));  // (cut short)
    CHECK(nwd_yaz0_size(src, 36) == 0 && nwd_yaz0_size(z, 8) == 0);
    uint8_t br[] = {'Y', 'a', 'z', '0', 0, 0, 0, 29, 0, 0, 0, 0, 0, 0, 0, 0, 0xE0, 'a', 'b', 'c', 0x40, 0x02, 0x00, 0x00, 0x02};
    // 3 literals, (len 6, dist 3), (len 0x12 + 2 = 20, dist 1): "abcabcabc" + 20 x 'c'
    CHECK(nwd_yaz0_size(br, sizeof br) == 29 && nwd_yaz0_unpack(br, sizeof br, d, 29));
    CHECK(!memcmp(d, "abcabcabccccccccccccccccccccc", 29));
    uint8_t bad[] = {'Y', 'a', 'z', '0', 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x30, 0x05};  // (a reference before the start)
    CHECK(!nwd_yaz0_unpack(bad, sizeof bad, d, 5));
    // SARC + BYML
    build_synth();
    NwdSarc s;
    CHECK(nwd_sarc_open(&s, g_gdt, sizeof g_gdt) && s.nodes == 3);
    uint32_t beg, end;
    uint32_t b9, e9;
    CHECK(!nwd_sarc_find(&s, "/nope", &b9, &e9));
    CHECK(nwd_sarc_find(&s, "/revival_bool_data_0.bgdata", &beg, &end) && end > beg);
    NwdByml b;
    CHECK(nwd_byml_open(&b, g_gdt + s.data_off + beg, end - beg) && b.root_t == NWDB_HASH);
    uint8_t t;
    uint32_t o;
    CHECK(nwd_byml_get(&b, b.root_t, b.root, "bool_data", &t, &o));
    CHECK(nwd_byml_count(&b, t, o, NWDB_ARR) == sizeof REVS / sizeof *REVS);
    CHECK(!nwd_byml_get(&b, b.root_t, b.root, "int_data", &t, &o));
    CHECK(!nwd_sarc_open(&s, g_gdt, 0x10) && !nwd_byml_open(&b, g_gdt, 32));  // (too short / a SARC is not a BYML)
    uint8_t big[64] = {'S', 'A', 'R', 'C', 0x14, 0, 0xFE, 0xFF};  // (big endian: refused)
    CHECK(!nwd_sarc_open(&s, big, sizeof big));
    uint8_t yb[16] = {'Y', 'B', 2, 0, 0xff, 0xff, 0, 0};  // (key table out of the buffer)
    CHECK(!nwd_byml_open(&b, yb, sizeof yb));
    // a list from data with no bool files / not a SARC
    NwdList L2;
    CHECK(nwd_list_build(g_f1, 100, NULL, 0, &L2) == NWDE_GDT && L2.n == 0);
    const char *nm[1] = {"/s32_data_0.bgdata"};
    const uint8_t *dd[1] = {g_f3};
    size_t ll[1] = {64};
    static uint8_t only[1 << 16];
    size_t on = sarc(nm, dd, ll, 1, only);
    CHECK(nwd_list_build(only, on, NULL, 0, &L2) == NWDE_NOFLAGS);
}

static void test_families(void) {
    CHECK(nwd_family("IsGet_PortableUnit", false, false) == NWDF_STORY && nwd_family("MapTower_03", false, false) == NWDF_STORY &&
          nwd_family("MapTower_16", false, false) == 0 && nwd_family("MapTower_DemoFirst", false, false) == NWDF_STORY);
    CHECK(nwd_family("Clear_Dungeon012", false, false) == NWDF_SHRINE && nwd_family("Enter_Dungeon7", false, false) == NWDF_SHRINE &&
          nwd_family("Clear_DungeonX", false, false) == 0 && nwd_family("Clear_Dungeon", false, false) == 0);
    CHECK(nwd_family("GameClear", false, false) == NWDF_BEAST && nwd_family("IsGet_Obj_HeroSoul_Zora", false, false) == NWDF_BEAST &&
          nwd_family("Get_HeartUtuwa_Electric", false, false) == NWDF_BEAST && nwd_family("Clear_RemainsFire", false, false) == NWDF_BEAST &&
          nwd_family("Wind_Relic_Activated", false, false) == NWDF_BEAST && nwd_family("IsHyruleCastlePGanonWaterDead", false, false) == NWDF_BEAST);
    CHECK(nwd_family("MainField_Npc_HiddenKorokFly_42", true, false) == NWDF_KOROK && nwd_family("MainField_TBox_Field_99", true, false) == NWDF_CHEST);
    CHECK(nwd_family("MainField_Item_Fruit_A_111", true, false) == NWDF_WORLD && nwd_family("MainField_Item_Fruit_A_111", false, false) == 0);
    CHECK(nwd_family("MarioClubTestDungeon_Item_666", true, false) == 0 && nwd_family("IsNewPictureBook_Item_Fruit", true, false) == 0);
    CHECK(nwd_family("MainField_Enemy_Bokoblin_777", true, false) == 0);  // (enemy dead flags: killsync / netloot own those)
    CHECK(nwd_family("MainField_Item_AVeryLongNameThatGoesPastTheLimit_888", true, false) == 0);
    CHECK(nwd_family("Quest_Step_A", false, true) == NWDF_QUEST && nwd_family("IsGet_Armor_001", false, true) == NWDF_QUEST);
    CHECK(nwd_family("IsGet_Armor_001", false, false) == 0 && nwd_family("IsPlayed_Demo999_0", false, false) == 0);
    CHECK(nwd_family("Shop_Item_01_SoldOut", false, false) == NWDF_SHOP && nwd_family("GemShop_Open", false, false) == NWDF_SHOP &&
          nwd_family("SoldOut", false, false) == 0);
    CHECK(nwd_family("Npc_Talk_Done", false, false) == NWDF_MORE && nwd_family("Kakariko_Thing", false, false) == NWDF_MORE &&
          nwd_family("NpcNoUnderscore", false, false) == 0 && nwd_family("RemainsX", false, false) == 0 && nwd_family("Remains_X", false, false) == 0);
    CHECK(nwd_family("Village_LinkTag_And_3", false, false) == NWDF_MORE && nwd_family("Village_WallCrack_01", false, false) == NWDF_MORE &&
          nwd_family("Vil lage_Pot_1", false, false) == 0 && nwd_family("SomeLoneFlag", false, false) == 0);
    CHECK(nwd_family("", false, false) == 0 && nwd_family(NULL, false, false) == 0);
    char longn[80];
    memset(longn, 'A', 70), longn[70] = 0, memcpy(longn, "Clear_Dungeon", 13);
    CHECK(nwd_family(longn, false, false) == 0);
    // gifts (PR quests_update)
    const char *it = NULL;
    CHECK(nwd_gift("IsGet_PortableUnit", &it) == NWDG_ITEM && !strcmp(it, "Obj_DRStone_Get"));
    CHECK(nwd_gift("IsGet_Obj_StopTimerLv2", &it) == NWDG_ITEM && !strcmp(it, "Obj_StopTimerLv2"));
    CHECK(nwd_gift("IsGet_Obj_PictureBook", &it) == NWDG_NONE && !it);  // (in STORY, no item: the PR gives none)
    CHECK(nwd_gift("FindDungeon_Finish", &it) == NWDG_ITEM && !strcmp(it, "PlayerStole2"));
    CHECK(nwd_gift("MainField_Npc_HiddenKorokGround_5", &it) == NWDG_SEED && !strcmp(it, "Obj_KorokNuts"));
    CHECK(nwd_gift("Clear_Dungeon044", &it) == NWDG_ORB && !strcmp(it, "Obj_DungeonClearSeal"));
    CHECK(nwd_gift("Open_Dungeon044", &it) == NWDG_NONE);
    CHECK(nwd_gift("IsGet_Obj_HeroSoul_Gerudo", &it) == NWDG_ITEM && !strcmp(it, "Obj_HeroSoul_Gerudo"));
    CHECK(nwd_gift("Get_HeartUtuwa_Fire", &it) == NWDG_HEART);
    CHECK(nwd_gift("MainField_TBox_X", &it) == NWDG_NONE && nwd_gift(NULL, &it) == NWDG_NONE);
    CHECK(nwd_slate_group("OP_DRC") && nwd_slate_group("IsPlayed_Demo700_0") && !nwd_slate_group("IsGet_Obj_RemoteBomb"));
    bool fly = false;
    CHECK(nwd_korok_id("MainField_Npc_HiddenKorokFly_1234567", &fly) == 1234567 && fly);
    CHECK(nwd_korok_id("MainField_Npc_HiddenKorokGround_4294967295", &fly) == 4294967295u && !fly);
    CHECK(nwd_korok_id("MainField_Npc_HiddenKorokGround_4294967296", &fly) == 0 && nwd_korok_id("MainField_Npc_HiddenKorokGround_", &fly) == 0 &&
          nwd_korok_id("X_12a", &fly) == 0 && nwd_korok_id(NULL, &fly) == 0);
}

static void test_list(void) {
    build_synth();
    CHECK(g_L.gdt_files == 2);  // (bool_data + revival_bool_data; the s32 file is not read)
    CHECK(g_L.bool_n == sizeof BOOLS / sizeof *BOOLS + 1 + sizeof REVS / sizeof *REVS);
    CHECK(g_L.dup == 2);       // (GameClear twice, MainField_Item_Fruit_A_111 twice)
    CHECK(g_L.clash == 1);     // (plumless / buckeroo: one crc32)
    CHECK(lt_crc32("plumless") == lt_crc32("buckeroo"));
    CHECK(g_L.quest_n == 2 && g_L.quest_bool == 2);  // (Quest_Step_A + Quest_Dep_B; the s32 dep and the "Other" quest are not)
    for (uint32_t i = 1; i < g_L.n; i++) CHECK(g_L.e[i - 1].hash < g_L.e[i].hash);
    CHECK(fam_of("IsGet_PortableUnit") == NWDF_STORY && fam_of("MapTower_03") == NWDF_STORY && fam_of("MapTower_16") == 0);
    CHECK(fam_of("Clear_Dungeon012") == NWDF_SHRINE && fam_of("Open_Dungeon012") == NWDF_SHRINE && fam_of("Clear_DungeonX") == 0);
    CHECK(fam_of("MainField_Npc_HiddenKorokGround_1234567") == NWDF_KOROK && fam_of("MainField_TBox_Field_Iron_99") == NWDF_CHEST);
    CHECK(fam_of("GameClear") == NWDF_BEAST && fam_of("Get_HeartUtuwa_Wind") == NWDF_BEAST);
    CHECK(fam_of("MainField_Item_Fruit_A_111") == NWDF_WORLD && fam_of("AocField_Weapon_Sword_222") == NWDF_WORLD &&
          fam_of("CDungeon_Obj_Ore_333") == NWDF_WORLD && fam_of("MainFieldDungeon_Barrel_444") == NWDF_WORLD && fam_of("MainField_Kibako_555") == NWDF_WORLD);
    CHECK(fam_of("MarioClubTestDungeon_Item_666") == 0 && fam_of("MainField_Enemy_Bokoblin_777") == 0 && fam_of("IsNewPictureBook_Item_Fruit_A") == 0);
    CHECK(fam_of("Quest_Step_A") == NWDF_QUEST && fam_of("Quest_Dep_B") == NWDF_QUEST && fam_of("Quest_Dep_Int") == 0);
    CHECK(fam_of("Shop_Item_01_SoldOut") == NWDF_SHOP && fam_of("Npc_Talk_Done") == NWDF_MORE && fam_of("SomeLoneFlag") == 0);
    CHECK(fam_of("plumless") == 0 && fam_of("buckeroo") == 0 && fam_of("ShouldNotCount") == 0 && fam_of("IsGet_Armor_001") == 0);
    int i = nwd_index_of(&g_L, lt_crc32("MainField_Item_Fruit_A_111"));
    CHECK(i >= 0 && g_L.e[i].rev && !strcmp(nwd_name(&g_L, (uint32_t)i), "MainField_Item_Fruit_A_111"));
    CHECK(nwd_index_of(&g_L, 0x12345678) < 0 && !strcmp(nwd_name(&g_L, 99999), ""));
    CHECK(g_L.fam_n[0] >= 4 && g_L.fp != 0);
    uint32_t fp = g_L.fp, n = g_L.n;
    build_synth();  // (the same files: the same list)
    CHECK(g_L.fp == fp && g_L.n == n);
    // no QuestProduct: no QUEST family, another fingerprint (a friend without it is refused)
    NwdList L3;
    NwdSarc s;
    nwd_sarc_open(&s, g_gdt, sizeof g_gdt);
    size_t gn = nwd_le32(g_gdt + 8);
    CHECK(nwd_list_build(g_gdt, gn, NULL, 0, &L3) == NWDE_OK && L3.fam_n[6] == 0 && L3.fp != fp && L3.n == n - 2);
    nwd_list_free(&L3);
    CHECK(L3.n == 0 && L3.e == NULL);
}

static uint32_t IX(const char *n) { int i = nwd_index_of(&g_L, lt_crc32(n)); if (i < 0) printf("  (no %s)\n", n); return (uint32_t)i; }
static void page_all(NwdCore *c, int k, uint16_t gen, const uint8_t *set) {  // (a friend's whole set as his pages)
    int pages = net_world_pages(c->n);
    for (int p = 0; p < pages; p++) nwd_core_page(c, k, gen, p, pages, set + (size_t)p * 640, net_world_page_bytes(c->n, p));
}

static void test_union(void) {
    build_synth();
    NwdCore C;
    CHECK(nwd_core_init(&C, &g_L, NWDF_DEFAULT));
    uint8_t tmp[64], his[64], his2[64];
    uint32_t lack[64], tot = 0;
    uint16_t own[64];
    int total = 0;
    uint32_t cleared = 0;
    CHECK(C.nb <= sizeof tmp);
    uint32_t chest = IX("MainField_TBox_Field_Iron_99"), fruit = IX("MainField_Item_Fruit_A_111"), ore = IX("CDungeon_Obj_Ore_333"),
             korok = IX("MainField_Npc_HiddenKorokGround_1234567"), shrine = IX("Clear_Dungeon012"), more = IX("Npc_Talk_Done"),
             tower = IX("MapTower_03");
    // nothing known of anyone: nothing to set
    CHECK(nwd_core_lack(&C, tmp, lack, 64, &tot) == 0 && tot == 0);
    // our first pass: our own set (a chest), no deltas yet (our pages carry it)
    nwd_bset(C.mine, chest);
    CHECK(nwd_core_pass_done(&C, own, 64, &total, &cleared) == 0 && total == 0 && C.passed);
    // a friend (slot 0) shares: his pages say fruit + ore + korok + Npc_Talk_Done (MORE: off by default)
    memset(his, 0, sizeof his);
    nwd_bset(his, fruit), nwd_bset(his, ore), nwd_bset(his, korok), nwd_bset(his, more);
    C.on[0] = true;
    page_all(&C, 0, 1, his);
    CHECK(C.have[0]);
    int n = nwd_core_lack(&C, tmp, lack, 64, &tot);
    CHECK(n == 3 && tot == 3);  // (fruit, ore, korok; not MORE; not our chest)
    bool got_more = false;
    for (int i = 0; i < n; i++) got_more |= lack[i] == more;
    CHECK(!got_more);
    // a friend who is not "on" is not in the union
    C.on[0] = false;
    CHECK(nwd_core_lack(&C, tmp, lack, 64, &tot) == 0);
    C.on[0] = true;
    // max: only 2 back, the total says 3
    CHECK(nwd_core_lack(&C, tmp, lack, 2, &tot) == 2 && tot == 3);
    // we set fruit + ore from him (applied): no longer lacking, and NOT sent back as our own on the next pass
    nwd_bset(C.applied, fruit), nwd_bset(C.applied, ore);
    CHECK(nwd_core_lack(&C, tmp, lack, 64, &tot) == 1 && lack[0] == korok);
    nwd_bset(C.mine, fruit), nwd_bset(C.mine, ore);
    nwd_bset(C.mine, shrine);  // (and our player cleared a shrine himself)
    n = nwd_core_pass_done(&C, own, 64, &total, &cleared);
    CHECK(n == 1 && own[0] == shrine && total == 1 && cleared == 0);
    CHECK(!nwd_bit(C.applied, fruit) && !nwd_bit(C.applied, ore));  // (seen set: ours now)
    CHECK(nwd_core_lack(&C, tmp, lack, 64, &tot) == 1 && lack[0] == korok);
    // a failed one (the game has no such flag) is not tried again
    nwd_bset(C.failed, korok);
    CHECK(nwd_core_lack(&C, tmp, lack, 64, &tot) == 0);
    nwd_bclr(C.failed, korok);
    // a second friend (slot 1) sends a SET: a tower
    C.on[1] = true;
    CHECK(nwd_core_set(&C, 1, (uint16_t[]){(uint16_t)tower, (uint16_t)tower}, 2) == 1);
    CHECK(nwd_core_set(&C, 1, (uint16_t[]){(uint16_t)tower}, 1) == 0);  // (known already)
    CHECK(nwd_core_lack(&C, tmp, lack, 64, &tot) == 2);  // (korok from 0, tower from 1: the UNION)
    CHECK(nwd_core_set(&C, 7, (uint16_t[]){1}, 1) == 0 && nwd_core_set(&C, -1, (uint16_t[]){1}, 1) == 0);
    // families off: networldfam without KOROK -> only the tower
    nwd_core_fams(&C, &g_L, NWDF_DEFAULT & ~NWDF_KOROK);
    CHECK(nwd_core_lack(&C, tmp, lack, 64, &tot) == 1 && lack[0] == tower);
    nwd_core_fams(&C, &g_L, NWDF_DEFAULT);
    // our pages carry only the families shared
    nwd_bset(C.mine, more);
    nwd_core_ours(&C, tmp);
    CHECK(nwd_bit(tmp, chest) && nwd_bit(tmp, shrine) && !nwd_bit(tmp, more));
    nwd_bclr(C.mine, more);
    // a flag is never unset: what we have and he lacks is not touched (lack only lists what WE lack)
    CHECK(!nwd_bit(his, chest) && nwd_bit(C.mine, chest));
    // pages: refusals (wrong page count / byte count / slot) and a half snapshot keeps the old set
    int pages = net_world_pages(C.n);
    CHECK(nwd_core_page(&C, 0, 2, 0, pages + 1, his, net_world_page_bytes(C.n, 0)) == -1);
    CHECK(nwd_core_page(&C, 0, 2, pages, pages, his, 1) == -1);
    CHECK(nwd_core_page(&C, 0, 2, 0, pages, his, net_world_page_bytes(C.n, 0) + 1) == -1);
    CHECK(nwd_core_page(&C, NWD_PEERS, 2, 0, pages, his, net_world_page_bytes(C.n, 0)) == -1);
    // the friend turns it off: his set is forgotten
    nwd_core_peer_reset(&C, 1);
    CHECK(!C.on[1] && nwd_core_lack(&C, tmp, lack, 64, &tot) == 1 && lack[0] == korok);
    nwd_core_free(&C);

    // ---- the Blood Moon rule ----
    CHECK(nwd_core_init(&C, &g_L, NWDF_DEFAULT));
    nwd_bset(C.mine, fruit), nwd_bset(C.mine, ore), nwd_bset(C.mine, chest);
    nwd_core_pass_done(&C, own, 64, &total, &cleared);
    memset(his, 0, sizeof his);
    nwd_bset(his, fruit), nwd_bset(his, ore), nwd_bset(his, chest);
    C.on[0] = true;
    page_all(&C, 0, 1, his);
    CHECK(nwd_core_lack(&C, tmp, lack, 64, &tot) == 0);
    // a Blood Moon here: fruit + ore (revival) clear in our game, the chest (not revival) stays
    nwd_bclr(C.mine, fruit), nwd_bclr(C.mine, ore);
    n = nwd_core_pass_done(&C, own, 64, &total, &cleared);
    CHECK(n == 0 && cleared == 2);
    uint32_t h = nwd_core_moon(&C, tmp);
    CHECK(h == 2 && nwd_bit(C.held, fruit) && nwd_bit(C.held, ore) && !nwd_bit(C.held, chest));
    // the friend still has them (from before it): NOT set here again
    CHECK(nwd_core_lack(&C, tmp, lack, 64, &tot) == 0);
    CHECK(nwd_core_release(&C, tmp) == 0);
    // our player takes the fruit again himself: ours (sent), held stays (harmless: we have it)
    nwd_bset(C.mine, fruit);
    n = nwd_core_pass_done(&C, own, 64, &total, &cleared);
    CHECK(n == 1 && own[0] == fruit);
    nwd_bclr(C.mine, fruit), nwd_core_pass_done(&C, own, 64, &total, &cleared);
    // the friend's Blood Moon: his fresh pages (gen 2) lack fruit + ore -> released here
    memset(his2, 0, sizeof his2);
    nwd_bset(his2, chest);
    CHECK(nwd_core_page(&C, 0, 2, 0, pages, his2, net_world_page_bytes(C.n, 0)) == (pages == 1 ? 1 : 0));
    for (int p = 1; p < pages; p++) nwd_core_page(&C, 0, 2, p, pages, his2 + p * 640, net_world_page_bytes(C.n, p));
    CHECK(!nwd_bit(C.known[0], fruit) && nwd_bit(C.known[0], chest));  // (the whole new set replaced the old)
    CHECK(nwd_core_release(&C, tmp) == 2 && !nwd_bit(C.held, fruit) && !nwd_bit(C.held, ore));
    // then he takes the fruit anew: set here again
    nwd_core_set(&C, 0, (uint16_t[]){(uint16_t)fruit}, 1);
    CHECK(nwd_core_lack(&C, tmp, lack, 64, &tot) == 1 && lack[0] == fruit);
    // a held flag that a SECOND friend takes anew after both moons is released as soon as no friend has it from before:
    // (here: held while ANY friend's set has it, released when it left all of them)
    nwd_bclr(C.mine, chest);
    n = nwd_core_pass_done(&C, own, 64, &total, &cleared);
    CHECK(cleared == 0);  // (the chest is not a revival flag: it does not count toward the 64)
    nwd_core_free(&C);
    // a big clear counts: 64+ revival flags gone in one pass (the glue's rule) - here the count only
    CHECK(nwd_core_init(&C, &g_L, NWDF_DEFAULT));
    for (uint32_t i = 0; i < C.n; i++) nwd_bset(C.mine, i);
    nwd_core_pass_done(&C, own, 64, &total, &cleared);
    memset(C.mine, 0, C.nb);
    nwd_core_pass_done(&C, own, 64, &total, &cleared);
    uint32_t revs = 0;
    for (uint32_t i = 0; i < C.n; i++) revs += nwd_bit(C.rev, i);
    CHECK(cleared == revs && revs >= 5);
    nwd_core_free(&C);
    CHECK(!nwd_core_init(&C, NULL, 0));
}

static void wfile(const char *p, const char *s) {
    FILE *f = fopen(p, "wb");
    if (f) fputs(s, f), fclose(f);
}
static long fsize(const char *p) {
    struct stat st;
    return stat(p, &st) ? -1 : (long)st.st_size;
}
// (s290 revfix) a done copy: COPY, the job's end, the settle, CLEAN
static void gate_copy(NwdGate *g, uint32_t room, uint64_t t, const char *path) {
    CHECK(nwd_gate_step(g, room, t, true) == NWDA_COPY);
    nwd_gate_job(g, NWDJ_OK, false, path, "", t + 50);
    CHECK(nwd_gate_step(g, room, t + 50 + NWD_SETTLE_MS, true) == NWDA_CLEAN && nwd_gate_ok(g, room));
}
// (s290 revfix, review F1 / F2 / FYI 2) the gate per slot
static void test_gate_state(void) {
    const char *A = "save/save_0/3", *B = "save/save_0/1", *C = "save/save_0/4", *D = "save/save_0/2";
    NwdGate g;
    memset(&g, 0, sizeof g);
    uint64_t t = 1000;
    // dirs: '/' or '\\', any case, a trailing slash; never a prefix
    CHECK(nwd_same_dir("save/save_0/3", "save\\save_0\\3\\") && nwd_same_dir("D:/X/Save/save_0/3", "d:/x/save/save_0/3"));
    CHECK(!nwd_same_dir("save/save_0/3", "save/save_0/30") && !nwd_same_dir("", "") && !nwd_same_dir(NULL, "a") && !nwd_same_dir("a", NULL));
    // no room / no friend's set: nothing; no save known: it waits (the variant: never guessed, however long)
    CHECK(!nwd_gate_ok(&g, 0) && nwd_gate_step(&g, 0, t, true) == NWDA_NONE);
    CHECK(nwd_gate_step(&g, 1, t, false) == NWDA_NONE);
    CHECK(nwd_gate_step(&g, 1, t, true) == NWDA_NOSAVE && !nwd_gate_ok(&g, 1));
    CHECK(nwd_gate_step(&g, 1, t + 3600000, true) == NWDA_NOSAVE && !nwd_gate_ok(&g, 1));
    // the game loads A: A is copied; the union waits through the copy and its settle
    nwd_gate_event(&g, 0, A, t);
    CHECK(nwd_gate_step(&g, 1, t, true) == NWDA_COPY && !strcmp(g.run, A) && !nwd_gate_ok(&g, 1) && nwd_gate_busy(&g));
    CHECK(nwd_gate_step(&g, 1, t + 10, true) == NWDA_NONE && !nwd_gate_ok(&g, 1));
    nwd_gate_job(&g, NWDJ_OK, false, "bk/a1", "", t + 50);
    CHECK(!nwd_gate_ok(&g, 1) && nwd_gate_busy(&g));
    CHECK(nwd_gate_step(&g, 1, t + 50 + NWD_SETTLE_MS - 1, true) == NWDA_NONE && !nwd_gate_ok(&g, 1));
    CHECK(nwd_gate_step(&g, 1, t + 50 + NWD_SETTLE_MS, true) == NWDA_CLEAN && nwd_gate_ok(&g, 1) && !strcmp(g.path, "bk/a1") && !g.same);
    CHECK(nwd_gate_step(&g, 1, t + 9000, true) == NWDA_NONE && nwd_gate_ok(&g, 1));
    // the game saves (prev60.log: write, written out, read back): an autosave to C, a save to A: covered, the union goes on
    t = 20000;
    nwd_gate_event(&g, 1, C, t), nwd_gate_event(&g, 2, C, t + 100), nwd_gate_event(&g, 0, C, t + 200);
    CHECK(nwd_gate_ok(&g, 1) && !strcmp(g.cur, C) && nwd_gate_covers(&g, C));
    nwd_gate_event(&g, 1, A, t + 300), nwd_gate_event(&g, 2, A, t + 400), nwd_gate_event(&g, 0, A, t + 500);
    CHECK(nwd_gate_ok(&g, 1) && nwd_gate_step(&g, 1, t + 600, true) == NWDA_NONE);
    // F1: Load of B (another branch) from the pause menu: the union waits at once; B is copied before the next union
    nwd_gate_event(&g, 0, B, t + 1000);
    CHECK(!nwd_gate_ok(&g, 1));
    // ... and B's own autosave before its copy began (no friend's set at that moment) does not cover it
    CHECK(nwd_gate_step(&g, 1, t + 1000, false) == NWDA_NONE);
    nwd_gate_event(&g, 1, B, t + 1100), nwd_gate_event(&g, 2, B, t + 1200), nwd_gate_event(&g, 0, B, t + 1300);
    nwd_gate_event(&g, 1, D, t + 1400), nwd_gate_event(&g, 2, D, t + 1500);  // (an autosave of B's state to D)
    CHECK(!nwd_gate_ok(&g, 1) && !nwd_gate_covers(&g, D));
    gate_copy(&g, 1, t + 2000, "bk/b1");
    CHECK(!strcmp(g.run, B) && nwd_gate_ok(&g, 1));
    // B's copy covers B only: A and C hold the old branch; reading C again = copied again
    CHECK(!nwd_gate_covers(&g, A) && !nwd_gate_covers(&g, C) && !nwd_gate_covers(&g, D));
    nwd_gate_event(&g, 0, C, t + 9000);
    CHECK(!nwd_gate_ok(&g, 1) && nwd_gate_step(&g, 1, t + 9000, true) == NWDA_COPY && !strcmp(g.run, C));
    nwd_gate_job(&g, NWDJ_OK, false, "bk/c1", "", t + 9050);
    // a read of another slot while C's copy settles: C done, then that one first
    nwd_gate_event(&g, 0, A, t + 9100);
    CHECK(nwd_gate_step(&g, 1, t + 9050 + NWD_SETTLE_MS, true) == NWDA_CLEAN && !nwd_gate_ok(&g, 1));
    CHECK(nwd_gate_step(&g, 1, t + 9100 + NWD_SETTLE_MS, true) == NWDA_COPY && !strcmp(g.run, A));
    nwd_gate_job(&g, NWDJ_OK, false, "bk/a2", "", t + 12000);
    CHECK(nwd_gate_step(&g, 1, t + 12000 + NWD_SETTLE_MS, true) == NWDA_CLEAN && nwd_gate_ok(&g, 1));

    // F2: the game writes the slot during its copy: torn, kept, copied again once the write is over (into a new folder)
    memset(&g, 0, sizeof g);
    t = 100000;
    nwd_gate_event(&g, 0, A, t);
    CHECK(nwd_gate_step(&g, 1, t, true) == NWDA_COPY);
    nwd_gate_event(&g, 1, A, t + 5);  // (its first write: during the copy)
    nwd_gate_job(&g, NWDJ_OK, false, "bk/a1", "", t + 50);
    CHECK(nwd_gate_step(&g, 1, t + 60, true) == NWDA_TORN && !nwd_gate_ok(&g, 1) && g.tries == 1 && !strcmp(g.path, "bk/a1"));
    CHECK(nwd_gate_step(&g, 1, t + 70, true) == NWDA_NONE);  // (the write goes on: the copy waits)
    nwd_gate_event(&g, 2, A, t + 400);                       // (written out)
    CHECK(nwd_gate_step(&g, 1, t + 410, true) == NWDA_COPY && !g.torn && !g.path[0]);
    nwd_gate_job(&g, NWDJ_OK, false, "bk/a2", "", t + 460);
    CHECK(nwd_gate_step(&g, 1, t + 460 + NWD_SETTLE_MS, true) == NWDA_CLEAN && nwd_gate_ok(&g, 1) && g.tries == 0 && !strcmp(g.path, "bk/a2"));
    // a late event (inside the settle) and the worker's own check (size / time changed) are torn too
    t = 200000;
    nwd_gate_event(&g, 0, B, t);
    CHECK(nwd_gate_step(&g, 1, t, true) == NWDA_COPY);
    nwd_gate_job(&g, NWDJ_OK, false, "bk/b1", "", t + 50);
    nwd_gate_event(&g, 2, B, t + 50 + NWD_SETTLE_MS - 1);
    CHECK(nwd_gate_step(&g, 1, t + 50 + NWD_SETTLE_MS, true) == NWDA_TORN && !nwd_gate_ok(&g, 1));
    CHECK(nwd_gate_step(&g, 1, t + 60 + NWD_SETTLE_MS, true) == NWDA_COPY);
    nwd_gate_job(&g, NWDJ_TORN, false, "bk/b2", "", t + 5000);
    CHECK(nwd_gate_step(&g, 1, t + 5001, true) == NWDA_TORN && g.tries == 2);
    // another slot's write during the copy: not torn; covered (the game's state came from the slot being copied)
    CHECK(nwd_gate_step(&g, 1, t + 5002, true) == NWDA_COPY);
    nwd_gate_event(&g, 1, D, t + 5003), nwd_gate_event(&g, 2, D, t + 5004);
    nwd_gate_job(&g, NWDJ_OK, false, "bk/b3", "", t + 5050);
    CHECK(nwd_gate_step(&g, 1, t + 5050 + NWD_SETTLE_MS, true) == NWDA_CLEAN && nwd_gate_ok(&g, 1) && nwd_gate_covers(&g, D));
    // torn NWD_COPY_TRIES times: given up for that slot in this room; a new room join tries again with a fresh budget
    memset(&g, 0, sizeof g);
    t = 300000;
    nwd_gate_event(&g, 0, A, t);
    for (int k = 0; k < NWD_COPY_TRIES; k++) {
        CHECK(nwd_gate_step(&g, 1, t, true) == NWDA_COPY);
        nwd_gate_job(&g, NWDJ_TORN, false, "bk/x", "", t + 10);
        int a = nwd_gate_step(&g, 1, t + 20, true);
        CHECK(a == (k + 1 < NWD_COPY_TRIES ? NWDA_TORN : NWDA_GIVEUP));
        t += 1000;
    }
    CHECK(nwd_gate_failed(&g, 1) && !nwd_gate_ok(&g, 1) && strstr(g.why, "during each"));
    CHECK(nwd_gate_step(&g, 1, t + 3600000, true) == NWDA_NONE);  // (no more in this room)
    CHECK(nwd_gate_step(&g, 2, t + 3600000, true) == NWDA_COPY && g.tries == 0);
    // FYI 2: a failed copy is tried again NWD_RETRY_MS later (into a new folder), NWD_COPY_TRIES times at most in a room
    memset(&g, 0, sizeof g);
    t = 400000;
    nwd_gate_event(&g, 0, A, t);
    for (int k = 0; k < NWD_COPY_TRIES; k++) {
        CHECK(nwd_gate_step(&g, 1, t, true) == NWDA_COPY);
        nwd_gate_job(&g, NWDJ_FAIL, false, NULL, "disk full", t + 10);
        CHECK(nwd_gate_failed(&g, 1) && !nwd_gate_ok(&g, 1) && !strcmp(g.why, "disk full") && g.tries == k + 1);
        CHECK(nwd_gate_step(&g, 1, t + 10 + NWD_RETRY_MS - 1, true) == NWDA_NONE);
        t += 10 + NWD_RETRY_MS;
    }
    CHECK(nwd_gate_step(&g, 1, t + 3600000, true) == NWDA_NONE && !nwd_gate_ok(&g, 1));
    // ... another slot read meanwhile: at once, a fresh budget
    nwd_gate_event(&g, 0, B, t);
    CHECK(nwd_gate_step(&g, 1, t, true) == NWDA_COPY && !strcmp(g.run, B) && g.tries == 0);
    // a write under way: the copy waits for it, NWD_SAVE_WAIT_MS at most
    memset(&g, 0, sizeof g);
    t = 500000;
    nwd_gate_event(&g, 0, A, t), nwd_gate_event(&g, 1, A, t + 1);
    CHECK(nwd_gate_step(&g, 1, t + 2, true) == NWDA_NONE);
    CHECK(nwd_gate_step(&g, 1, t + 1 + NWD_SAVE_WAIT_MS, true) == NWDA_COPY);
    // a new game: no read in this run, the slot it writes is the one it is on
    memset(&g, 0, sizeof g);
    t = 600000;
    nwd_gate_event(&g, 1, D, t), nwd_gate_event(&g, 2, D, t + 10);
    CHECK(!strcmp(g.cur, D) && !g.cur_read);
    gate_copy(&g, 1, t + 100, "bk/d1");
    nwd_gate_event(&g, 1, C, t + 200), nwd_gate_event(&g, 2, C, t + 210);  // (its autosave: covered; no read yet, so cur follows it)
    CHECK(!strcmp(g.cur, C) && nwd_gate_ok(&g, 1));
    // a new room join on a covered slot: copied again (the room's own copy), what was covered stays covered
    nwd_gate_event(&g, 0, C, t + 300);
    CHECK(!nwd_gate_ok(&g, 2));
    gate_copy(&g, 2, t + 400, "bk/c1");
    CHECK(!strcmp(g.run, C) && nwd_gate_covers(&g, D) && nwd_gate_ok(&g, 2));
    nwd_gate_event(&g, 0, D, t + 5000);
    CHECK(nwd_gate_ok(&g, 2));
    // the old room: never ok again
    CHECK(!nwd_gate_ok(&g, 1));
    // more slots written than NWD_GATE_COVER: the extra ones are copied again when read (the safe side)
    char dirs[NWD_GATE_COVER + 2][40];
    for (int k = 0; k < NWD_GATE_COVER + 2; k++) snprintf(dirs[k], sizeof dirs[k], "save/save_0/%d", 10 + k), nwd_gate_event(&g, 2, dirs[k], t + 6000);
    CHECK(g.ncover == NWD_GATE_COVER);
    nwd_gate_event(&g, 0, dirs[NWD_GATE_COVER + 1], t + 7000);
    CHECK(!nwd_gate_ok(&g, 2));
    // a stray event: no dir
    nwd_gate_event(&g, 0, "", t), nwd_gate_event(&g, 0, NULL, t);
    CHECK(!strcmp(g.cur, dirs[NWD_GATE_COVER + 1]));
    // the job's end when no copy runs: nothing
    int st = g.st;
    nwd_gate_job(&g, NWDJ_OK, false, "x", "", t);
    CHECK(g.st == st);
}

// (s290 revfix F4) this code's own folder names, nothing else
static void test_backup_names(void) {
    int s = -1, k = -1;
    CHECK(nwd_backup_name("2026-11-26_06-00-00_slot3", &s, &k) && s == 3 && k == 1);
    CHECK(nwd_backup_name("2026-11-26_06-00-00_slot3_2", &s, &k) && s == 3 && k == 2);
    CHECK(nwd_backup_name("2026-11-26_06-00-00_slot125_99", &s, &k) && s == 125 && k == 99);
    CHECK(!nwd_backup_name("2026-11-26_06-00-00_slot3_1", &s, &k) && !nwd_backup_name("2026-11-26_06-00-00_slot3_100", &s, &k));
    CHECK(!nwd_backup_name("2026-11-26_06-00-00_slot1234", &s, &k) && !nwd_backup_name("2026-11-26_06-00-00_slot", &s, &k));
    CHECK(!nwd_backup_name("2026-11-26_06-00-00_slot3x", &s, &k) && !nwd_backup_name("2026-11-26_06-00-00_slot3_", &s, &k));
    CHECK(!nwd_backup_name("2026-11-26_06-00-0_slot3", &s, &k) && !nwd_backup_name("2026-11-26 06-00-00_slot3", &s, &k));
    CHECK(!nwd_backup_name("save-backups", &s, &k) && !nwd_backup_name("", &s, &k) && !nwd_backup_name(NULL, &s, &k) && !nwd_backup_name(".", &s, &k));
}
// (the test's own leftovers in its scratch folder: only names nwd_backup_name says are ours, only their plain files)
static void clean_backups(const char *d3) {
    DIR *d = opendir(d3);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!nwd_backup_name(de->d_name, NULL, NULL)) continue;
        char dir[800];
        snprintf(dir, sizeof dir, "%s/%s", d3, de->d_name);
        DIR *e = opendir(dir);
        struct dirent *fe;
        while (e && (fe = readdir(e)))
            if (strcmp(fe->d_name, ".") && strcmp(fe->d_name, "..")) {
                char f[1100];
                snprintf(f, sizeof f, "%s/%s", dir, fe->d_name), remove(f);
            }
        if (e) closedir(e);
        rmdir(dir);
    }
    closedir(d);
}
// (s290 revfix F4) no new folder while the save is unchanged since this code's newest copy of that slot; nothing deleted
static void test_backup_same(const char *sd, const char *d3) {
    char p[700], q[900], path[700], why[300], want[700];
    long long bytes = 0;
    bool same = true, torn = true;
    snprintf(p, sizeof p, "%s/game_data.sav", sd), wfile(p, "SAVEDATA-0123456789");  // (as the first copies: _slot3 and _slot3_2)
    // the newest of slot 3 is _slot3_3, the partial folder the refusal above left (caption.jpg only): never reused, a new copy
    snprintf(q, sizeof q, "%s/2026-11-26_06-00-00_slot3_3/caption.jpg", d3);
    CHECK(fsize(q) == 3);
    int f = nwd_backup_run(sd, "2026-11-26_06-00-01", path, sizeof path, &bytes, &same, &torn, why, sizeof why);
    snprintf(want, sizeof want, "%s/2026-11-26_06-00-01_slot3", d3);
    CHECK(f == 2 && !same && !torn && bytes == 22 && !strcmp(path, want));
    // unchanged since: no new folder, that copy is the one
    same = torn = false, bytes = 0;
    f = nwd_backup_run(sd, "2026-11-26_06-00-01", path, sizeof path, &bytes, &same, &torn, why, sizeof why);
    CHECK(f == 2 && same && !torn && bytes == 22 && !strcmp(path, want));
    snprintf(q, sizeof q, "%s/2026-11-26_06-00-01_slot3_2", d3);
    CHECK(!nwd_exists(q));
    // a change of the same size: a new copy
    snprintf(p, sizeof p, "%s/caption.jpg", sd), wfile(p, "JPX");
    f = nwd_backup_run(sd, "2026-11-26_06-00-01", path, sizeof path, &bytes, &same, &torn, why, sizeof why);
    CHECK(f == 2 && !same && !torn && !strcmp(path, q) && nwd_same_file(p, "nope") == 0);
    snprintf(want, sizeof want, "%s/caption.jpg", q);
    CHECK(nwd_same_file(p, want) == 1 && nwd_same_file(p, p) == 1);
    f = nwd_backup_run(sd, "2026-11-26_06-00-02", path, sizeof path, &bytes, &same, &torn, why, sizeof why);
    CHECK(f == 2 && same && !strcmp(path, q));
    // only the NEWEST of the slot counts: back to the older content = a new copy
    wfile(p, "JPG");
    f = nwd_backup_run(sd, "2026-11-26_06-00-02", path, sizeof path, &bytes, &same, &torn, why, sizeof why);
    snprintf(want, sizeof want, "%s/2026-11-26_06-00-02_slot3", d3);
    CHECK(f == 2 && !same && !strcmp(path, want));
    // the newest holds a file more (or one less): not the same
    snprintf(q, sizeof q, "%s/extra.txt", want), wfile(q, "x");
    f = nwd_backup_run(sd, "2026-11-26_06-00-03", path, sizeof path, &bytes, &same, &torn, why, sizeof why);
    snprintf(want, sizeof want, "%s/2026-11-26_06-00-03_slot3", d3);
    CHECK(f == 2 && !same && !strcmp(path, want));
    // another slot's folder and names not ours are never looked at, even when newer
    char o[700];
    snprintf(o, sizeof o, "%s/2026-11-26_07-00-00_slot4", d3), nwd_mkdir(o);
    snprintf(q, sizeof q, "%s/game_data.sav", o), wfile(q, "OTHER");
    snprintf(o, sizeof o, "%s/2026-11-26_09-00-00_slot3x", d3), nwd_mkdir(o);
    f = nwd_backup_run(sd, "2026-11-26_06-00-04", path, sizeof path, &bytes, &same, &torn, why, sizeof why);
    CHECK(f == 2 && same && !strcmp(path, want));
    rmdir(o);
    // a file missing in the newest: not the same (a new copy, the old one untouched)
    snprintf(q, sizeof q, "%s/caption.jpg", want), remove(q);
    f = nwd_backup_run(sd, "2026-11-26_06-00-04", path, sizeof path, &bytes, &same, &torn, why, sizeof why);
    snprintf(q, sizeof q, "%s/2026-11-26_06-00-04_slot3", d3);
    CHECK(f == 2 && !same && !strcmp(path, q));
    snprintf(q, sizeof q, "%s/game_data.sav", want);
    CHECK(fsize(q) == 19);
    // no game_data.sav: refused, no folder made
    snprintf(p, sizeof p, "%s/game_data.sav", sd), remove(p);
    f = nwd_backup_run(sd, "2026-11-26_06-00-05", path, sizeof path, &bytes, &same, &torn, why, sizeof why);
    snprintf(q, sizeof q, "%s/2026-11-26_06-00-05_slot3", d3);
    CHECK(f < 0 && !same && strstr(why, "no game_data.sav") && !nwd_exists(q));
    snprintf(p, sizeof p, "%s/game_data.sav", sd), wfile(p, "SAVEDATA-0123456789");
}
static void test_gate(const char *tmpdir) {
    test_gate_state();
    test_backup_names();
    // backup roots
    char r[600];
    int slot = -1;
    CHECK(nwd_backup_root("D:/x/Savage Wind/save/save_0/3", r, sizeof r, &slot) && slot == 3 && !strcmp(r, "D:/x/Savage Wind/sora-networld-backups"));
    CHECK(nwd_backup_root("D:\\x\\save\\save_0\\12\\", r, sizeof r, &slot) && slot == 12 && !strcmp(r, "D:\\x/sora-networld-backups"));
    CHECK(nwd_backup_root("save/save_0/0", r, sizeof r, &slot) && slot == 0 && !strcmp(r, "sora-networld-backups"));
    CHECK(!nwd_backup_root("D:/x/save/save_1/3", r, sizeof r, &slot) && !nwd_backup_root("D:/x/save/save_0/abc", r, sizeof r, &slot) &&
          !nwd_backup_root("", r, sizeof r, &slot) && !nwd_backup_root(NULL, r, sizeof r, &slot));
    // a real copy in the scratch folder
    char base[400], d1[500], d2[500], d3[500], sd[500], p[600], path[700], why[300];
    snprintf(base, sizeof base, "%s/nwgate", tmpdir);
    snprintf(d1, sizeof d1, "%s/save", base), snprintf(d2, sizeof d2, "%s/save/save_0", base), snprintf(sd, sizeof sd, "%s/save/save_0/3", base);
    nwd_mkdir(tmpdir), nwd_mkdir(base), nwd_mkdir(d1), nwd_mkdir(d2), nwd_mkdir(sd);
    snprintf(d3, sizeof d3, "%s/sora-networld-backups", base);
    clean_backups(d3);  // (a leftover from an earlier run of this test: cleared)
    snprintf(p, sizeof p, "%s/game_data.sav", sd), wfile(p, "SAVEDATA-0123456789");
    snprintf(p, sizeof p, "%s/caption.jpg", sd), wfile(p, "JPG");
    long long bytes = 0;
    int f = nwd_backup_slot(sd, "2026-11-26_06-00-00", path, sizeof path, &bytes, why, sizeof why);
    CHECK(f == 2 && bytes == 22);
    char want[700];
    snprintf(want, sizeof want, "%s/2026-11-26_06-00-00_slot3", d3);
    CHECK(!strcmp(path, want));
    snprintf(p, sizeof p, "%s/game_data.sav", want);
    CHECK(fsize(p) == 19);
    // the same stamp again: never into the earlier folder, a new name
    f = nwd_backup_slot(sd, "2026-11-26_06-00-00", path, sizeof path, &bytes, why, sizeof why);
    snprintf(want, sizeof want, "%s/2026-11-26_06-00-00_slot3_2", d3);
    CHECK(f == 2 && !strcmp(path, want));
    snprintf(p, sizeof p, "%s/2026-11-26_06-00-00_slot3/game_data.sav", d3);
    CHECK(fsize(p) == 19);  // (the first one untouched)
    // refusals: no game_data.sav / not a slot folder / not there
    snprintf(p, sizeof p, "%s/game_data.sav", sd), remove(p);
    f = nwd_backup_slot(sd, "2026-11-26_06-00-00", path, sizeof path, &bytes, why, sizeof why);
    CHECK(f < 0 && strstr(why, "no game_data.sav"));
    f = nwd_backup_slot(d2, "2026-11-26_06-00-00", path, sizeof path, &bytes, why, sizeof why);
    CHECK(f < 0 && strstr(why, "not a save_0"));
    f = nwd_backup_slot("Z:/no/such/save/save_0/1", "x", path, sizeof path, &bytes, why, sizeof why);
    CHECK(f < 0 && strstr(why, "not there"));
    // copy never overwrites
    snprintf(p, sizeof p, "%s/caption.jpg", sd);
    snprintf(want, sizeof want, "%s/2026-11-26_06-00-00_slot3/caption.jpg", d3);
    CHECK(nwd_copy_file(p, want) == -1 && fsize(want) == 3);
    test_backup_same(sd, d3);
}

// the real 1.9.0 files (read only): counts + an optional dump for sb/s290/networld/compare.py
static void test_real(const char *romfs, const char *dump) {
    char p1[700], p2[700], why[300] = "", w2[300] = "";
    snprintf(p1, sizeof p1, "%s/Pack/Bootup.pack", romfs), snprintf(p2, sizeof p2, "%s/Pack/TitleBG.pack", romfs);
    size_t gn = 0, qn = 0;
    uint8_t *g = nwd_read_packed(p1, "GameData/gamedata.ssarc", &gn, why, sizeof why);
    uint8_t *q = nwd_read_packed(p2, "Quest/QuestProduct.sbquestpack", &qn, w2, sizeof w2);
    if (!g) { printf("  real files: %s (skipped)\n", why); return; }
    CHECK(gn == 6514692);  // (oead: 6514692 bytes unpacked)
    CHECK(q != NULL);
    NwdList L;
    int r = nwd_list_build(g, gn, q, qn, &L);
    CHECK(r == NWDE_OK);
    CHECK(L.bool_n == 42031);  // (oead: 42031 bool flags, 42027 names)
    CHECK(L.dup == 4 && L.clash == 0);
    CHECK(L.fam_n[3] == 900);   // (Koroks)
    CHECK(L.fam_n[4] == 1613);  // (chests: oead's 1615 *_TBox_* less Fire_ / Wind_Relic_TBox_Appear, BEAST by the PR's Relic rule)
    printf("  real files: %u flags shared of %u (dup %u, clash %u), fp %08x, quest names %u (bool %u):", L.n, L.bool_n, L.dup, L.clash, L.fp, L.quest_n, L.quest_bool);
    for (int b = 0; b < NWDF_COUNT; b++) printf(" %s %u", NWDF_TXT[b], L.fam_n[b]);
    printf("\n");
    CHECK(L.n <= NET_WORLD_COUNT_MAX);
    if (dump) {
        FILE *f = fopen(dump, "wb");
        if (f) {
            for (uint32_t i = 0; i < L.n; i++) fprintf(f, "%08x %u %u %s\n", L.e[i].hash, L.e[i].fam, L.e[i].rev, nwd_name(&L, i));
            fclose(f);
        }
    }
    // the default families' size on the wire: pages of the whole set
    printf("  real files: %d pages for the whole set (%u flags), %d bytes each at most\n", net_world_pages(L.n), L.n, NET_WORLD_PAGE_HEAD + 640);
    nwd_list_free(&L);
    free(g), free(q);
}

int main(int argc, char **argv) {
    const char *tmp = argc > 1 ? argv[1] : "build_h";
    test_wire();
    test_readers();
    test_families();
    test_list();
    test_union();
    test_gate(tmp);
    if (argc > 2) test_real(argv[2], argc > 3 ? argv[3] : NULL);
    nwd_list_free(&g_L);
    printf("networld_test: %d/%d passed\n", runs - fails, runs);
    return fails != 0;
}

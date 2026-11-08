// cust_test.c: host tests for sora_cust.h (pause menu slice 4, step 2). sh D:/SoraBotW/recon/make_cust_test.sh
#include <stdio.h>
#include <string.h>
#include "sora_cust.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static void test_cust_default_packs(void) {
    int sc[4];
    cust_default(sc);
    CHECK(sc[0] == 0 && sc[1] == 1 && sc[2] == 2 && sc[3] == 3);
    CHECK(cust_pack(sc) == 0x03020100u);
}

static void test_cust_roundtrip(void) {
    int a[4] = { 3, CUST_EMPTY, 0, 3 }, b[4];
    CHECK(cust_unpack(cust_pack(a), b) == 0);
    CHECK(b[0] == 3 && b[1] == CUST_EMPTY && b[2] == 0 && b[3] == 3);
}

static void test_cust_unpack_bad_byte(void) {
    int sc[4] = { 9, 9, 9, 9 };
    CHECK(cust_unpack(0x03FF0907u, sc) == 2);  // 0x07 / 0x09 bad -> empty, 0xFF = empty, 0x03 kept
    CHECK(sc[0] == CUST_EMPTY && sc[1] == CUST_EMPTY && sc[2] == CUST_EMPTY && sc[3] == 3);
}

static void test_cust_empty_slot(void) {
    int sc[4];
    cust_default(sc);
    CHECK(cust_set(sc, 2, CUST_EMPTY) == 1);
    CHECK(sc[2] == CUST_EMPTY && ((cust_pack(sc) >> 16) & 0xFF) == 0xFF);
    CHECK(cust_set(sc, 2, CUST_EMPTY) == 0);  // already empty
}

static void test_cust_duplicate_rule(void) {  // KH2 FUN_140359b70: no duplicate check -> allowed, the other slot kept
    int sc[4];
    cust_default(sc);
    CHECK(cust_set(sc, 1, 0) == 1);
    CHECK(sc[0] == 0 && sc[1] == 0 && sc[2] == 2 && sc[3] == 3);
    CHECK(cust_set(sc, 1, 0) == 0);
}

static void test_cust_set_range(void) {
    int sc[4];
    cust_default(sc);
    CHECK(cust_set(sc, 4, 0) == -1 && cust_set(sc, -1, 0) == -1);
    CHECK(cust_set(sc, 0, 7) == -1 && cust_set(sc, 0, -2) == -1 && cust_set(sc, 0, CUST_NSPELL) == -1);
    CHECK(cust_pack(sc) == 0x03020100u);
}

static void test_cust_str(void) {  // log names: the KH_SPELLS names, "empty" for CUST_EMPTY, "?" out of range
    CHECK(!strcmp(cust_str(0), "fire") && !strcmp(cust_str(3), "cure"));
    CHECK(!strcmp(cust_str(CUST_EMPTY), "empty") && !strcmp(cust_str(7), "?"));
}

static void test_cust_load(void) {  // sidecar "shortcuts": missing -> default, present -> unpack (bad bytes counted)
    int sc[4] = { 9, 9, 9, 9 };
    CHECK(cust_load(0, 0xFFFFFFFFu, sc) == 0 && cust_pack(sc) == 0x03020100u);
    CHECK(cust_load(1, 0x00FF0203u, sc) == 0 && sc[0] == 3 && sc[1] == 2 && sc[2] == CUST_EMPTY && sc[3] == 0);
    CHECK(cust_load(1, 0x0303FF09u, sc) == 1 && sc[0] == CUST_EMPTY && sc[1] == CUST_EMPTY && sc[2] == 3);
}

int main(void) {
    test_cust_default_packs(); test_cust_roundtrip(); test_cust_unpack_bad_byte();
    test_cust_empty_slot(); test_cust_duplicate_rule(); test_cust_set_range();
    test_cust_str(); test_cust_load();
    printf("cust_test: %d/%d checks pass\n", runs - fails, runs);
    return fails != 0;
}

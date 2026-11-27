// (s289 b9fix) mgelem's per-class reaction-object cache (linkpos_ercache.h): found / none entries, lookup, LRU replace.
// clang -O1 -g -Wall -Wno-unused-function -Iruntime runtime/test/ercache_test.c -o ercache.exe
#include <stdio.h>
#include <string.h>
#include "linkpos_ercache.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static void test_found_and_none(void) {
    static ErCache c;
    memset(&c, 0, sizeof c);
    CHECK(erc_find(&c, 0x71001000) == NULL);
    uint64_t ev = 1;
    erc_put(&c, 0x71001000, 1, 1, 0x58, 7, &ev);  // a class with the object: pointer at damage manager + 0x58, slot 7
    CHECK(ev == 0);
    erc_put(&c, 0x71002000, -1, 0, 0, -1, &ev);  // a class searched with nothing found
    ErcEnt *a = erc_find(&c, 0x71001000), *b = erc_find(&c, 0x71002000);
    CHECK(a && a->base == 1 && a->ptr == 1 && a->off == 0x58 && a->slot == 7);
    CHECK(b && b->slot < 0);  // "none" is a hit: never searched again
    CHECK(erc_find(&c, 0x71003000) == NULL && erc_find(&c, 0) == NULL);  // vtable 0 = no class key, never cached
    erc_put(&c, 0x71002000, 0, 0, 0x100, 3, &ev);  // the same class again: its own entry, not a second one
    CHECK(c.n == 2 && ev == 0 && erc_find(&c, 0x71002000)->slot == 3);
}

static void test_lru(void) {
    static ErCache c;
    memset(&c, 0, sizeof c);
    uint64_t ev = 0;
    for (int i = 0; i < ERC_N; i++) { erc_put(&c, 0x72000000 + 0x100 * (uint64_t)i, 0, 0, 8 * i, i % 5, &ev); CHECK(ev == 0); }
    CHECK(c.n == ERC_N);
    for (int i = 1; i < ERC_N; i++) CHECK(erc_find(&c, 0x72000000 + 0x100 * (uint64_t)i) != NULL);  // all used again but class 0
    erc_put(&c, 0x7300, -1, 0, 0, -1, &ev);  // full: the least recently used (class 0) goes
    CHECK(ev == 0x72000000 && c.n == ERC_N);
    CHECK(erc_find(&c, 0x72000000) == NULL && erc_find(&c, 0x7300) && erc_find(&c, 0x7300)->slot < 0);
    CHECK(erc_find(&c, 0x72000100) && erc_find(&c, 0x72000100)->off == 8);
    erc_put(&c, 0x7400, 0, 0, 0, 1, &ev);  // next: class 2 is now the least used (class 1 was just found)
    CHECK(ev == 0x72000200);
}

int main(void) {
    test_found_and_none();
    test_lru();
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}

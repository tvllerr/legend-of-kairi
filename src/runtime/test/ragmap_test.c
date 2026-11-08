// host test for sora_ragmap_load (runtime/sora_ragmap.h): the data/ragmap/<char>.bin reader. Builds a synthetic file in the
// tools/sorabuild/ragmap/ragbin.py layout, checks the round trip and that every bad / missing file is refused with the tables untouched.
//   clang -O1 -Wall -Iruntime runtime/test/ragmap_test.c -o ragmap_test.exe && ./ragmap_test.exe
#include <stdio.h>
#include <string.h>
#include "sora_ragmap.h"
#include "sora_ragmap_roxas.h"
#include "sora_ragmap_mickey.h"

static int runs, fails;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void write_file(const char *p, int nb, int nm, uint32_t magic, size_t cut) {
    unsigned char buf[8192];
    size_t n = 0;
    uint32_t h[4] = {magic, 1, (uint32_t)nb, (uint32_t)nm};
    memcpy(buf, h, 16), n = 16;
    for (int i = 0; i < nb; i++) { int16_t v = (int16_t)(i - 1); memcpy(buf + n, &v, 2), n += 2; }
    while (n % 4) buf[n++] = 0;
    for (int i = 0; i < nb; i++) { float v[3] = {(float)i, (float)i * 2, (float)i * 3}; memcpy(buf + n, v, 12), n += 12; }
    for (int i = 0; i < nm; i++) { float v[4] = {0.f, 1.f, 0.f, 10.f + (float)i}; memcpy(buf + n, v, 16), n += 16; }
    FILE *f = fopen(p, "wb");
    fwrite(buf, 1, cut && cut < n ? cut : n, f);
    fclose(f);
}

int main(void) {
    const char *why = "", *p = "ragmap_test.tmp";
    static int16_t par[SORA_RAG_BONES];
    static float pos[SORA_RAG_BONES][3];
    // the tables ship without KH2 numbers: the map's bind dirs start at 0
    CHECK(sora_ragmap[0].bind_len == 0 && sora_ragmap_roxas[0].bind_len == 0 && sora_ragmap_mickey[0].bind_len == 0);

    write_file(p, SORA_RAG_BONES, SORA_RAGMAP_N, 0x47415253u, 0);
    CHECK(sora_ragmap_load(p, SORA_RAG_BONES, par, pos, sora_ragmap, SORA_RAGMAP_N, &why) == 1);
    CHECK(par[0] == -1 && par[239] == 238 && pos[239][2] == 717.f && pos[5][1] == 10.f);
    CHECK(sora_ragmap[0].bind_dir[1] == 1.f && sora_ragmap[17].bind_len == 27.f);

    memset(par, 0x55, sizeof par);
    CHECK(sora_ragmap_load("ragmap_no_such_file.bin", SORA_RAG_BONES, par, pos, sora_ragmap, SORA_RAGMAP_N, &why) == 0 && !strcmp(why, "cannot open"));
    CHECK(sora_ragmap_load(NULL, SORA_RAG_BONES, par, pos, sora_ragmap, SORA_RAGMAP_N, &why) == 0);
    write_file(p, SORA_RAG_BONES, SORA_RAGMAP_N, 0x47415253u, 100);   // truncated
    CHECK(sora_ragmap_load(p, SORA_RAG_BONES, par, pos, sora_ragmap, SORA_RAGMAP_N, &why) == 0 && !strcmp(why, "wrong file size"));
    write_file(p, SORA_RAG_BONES, SORA_RAGMAP_N, 0x12345678u, 0);     // bad magic
    CHECK(sora_ragmap_load(p, SORA_RAG_BONES, par, pos, sora_ragmap, SORA_RAGMAP_N, &why) == 0 && !strcmp(why, "not a SRAG v1 file"));
    write_file(p, SORA_RAG_BONES - 1, SORA_RAGMAP_N, 0x47415253u, 0);  // another character's file (other bone count)
    CHECK(sora_ragmap_load(p, SORA_RAG_BONES, par, pos, sora_ragmap, SORA_RAGMAP_N, &why) == 0);
    write_file(p, SORA_RAG_BONES, SORA_RAGMAP_N - 1, 0x47415253u, 0);
    CHECK(sora_ragmap_load(p, SORA_RAG_BONES, par, pos, sora_ragmap, SORA_RAGMAP_N, &why) == 0);
    for (int k = 0; k < 2; k++) {  // (s182 review S1) a parent pointing forward (a cycle risk) / a NaN position: refused
        write_file(p, SORA_RAG_BONES, SORA_RAGMAP_N, 0x47415253u, 0);
        FILE *f = fopen(p, "r+b");
        if (k == 0) { int16_t v = 7; fseek(f, 16 + 2 * 5, SEEK_SET), fwrite(&v, 2, 1, f); }
        else { float v = 0.f / 0.f; fseek(f, 16 + ((SORA_RAG_BONES * 2 + 3) & ~3) + 12 * 9 + 4, SEEK_SET), fwrite(&v, 4, 1, f); }
        fclose(f);
        CHECK(sora_ragmap_load(p, SORA_RAG_BONES, par, pos, sora_ragmap, SORA_RAGMAP_N, &why) == 0 && !strcmp(why, "bad parent / pos"));
    }
    CHECK(par[0] == 0x5555);   // refused files leave the tables alone
    remove(p);
    printf("ragmap_test: %d checks, %d failed\n", runs, fails);
    return fails != 0;
}

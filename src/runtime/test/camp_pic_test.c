// host test for runtime/sora_camp_pic.h: sh tools_s120/make_pic_test.sh  (optionally: camp_pic_test.exe <itempic.bin> checks the grid against the bake)
#include <stdio.h>
#include <stdlib.h>
#include "sora_camp_pic.h"
#include "sora_kb.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

int main(int argc, char **argv) {
    SoraHudQuad q;
    CHECK(CP_N == KB_N);
    CHECK(cp_pic_quad(&q, 0, 10, 20, 100, 80, 255) && q.tex == 3 && q.blend == 0);
    CHECK(q.x[0] == 10 && q.x[1] == 110 && q.x[2] == 10 && q.x[3] == 110);
    CHECK(q.y[0] == 20 && q.y[1] == 20 && q.y[2] == 100 && q.y[3] == 100);
    CHECK(q.u[0] == 0 && q.u[1] == 256 && q.v[0] == 0 && q.v[2] == 256);
    CHECK(q.col[0] == 0x80808080u && q.col[3] == 0x80808080u);
    CHECK(cp_pic_quad(&q, 7, 0, 0, 1, 1, 0) && q.u[0] == 256 && q.u[1] == 512 && q.v[0] == 256 && q.v[2] == 512 && (q.col[0] >> 24) == 0);
    CHECK(cp_pic_quad(&q, 23, 0, 0, 1, 1, 128) && q.u[0] == 5 * 256 && q.v[0] == 3 * 256 && q.u[1] == 1536 && q.v[2] == 1024);
    CHECK((q.col[0] >> 24) == 64);
    q.tex = 9;
    CHECK(cp_pic_quad(&q, 25, 0, 0, 1, 1, 255) && q.u[0] == 256 && q.v[0] == 4 * 256);  // (s214) Guardian Core's cell
    q.tex = 9;
    CHECK(!cp_pic_quad(&q, CP_N, 0, 0, 1, 1, 255) && q.tex == 9);
    CHECK(!cp_pic_quad(&q, -1, 0, 0, 1, 1, 255) && !cp_pic_quad(NULL, 0, 0, 0, 1, 1, 255));
    for (int i = 0; i < CP_N; i++) {  // every cell inside the atlas, no two alike
        CHECK(cp_pic_quad(&q, i, 0, 0, 1, 1, 255) && q.u[1] <= CP_ATLAS_W && q.v[2] <= 5 * CP_CELL);
        for (int j = 0; j < i; j++) { SoraHudQuad r; cp_pic_quad(&r, j, 0, 0, 1, 1, 255); CHECK(r.u[0] != q.u[0] || r.v[0] != q.v[0]); }
    }
    if (argc > 1) {  // the baked file: header + per-cell table = the DLL's grid
        FILE *f = fopen(argv[1], "rb");
        CHECK(f != NULL);
        if (f) {
            uint32_t h[6];
            CHECK(fread(h, 4, 6, f) == 6 && !memcmp(h, "SKIP", 4) && h[1] == 1 && h[2] == CP_ATLAS_W && h[3] == 5 * CP_CELL && h[4] == CP_N);
            for (int i = 0; i < CP_N; i++) {
                uint16_t e[6];
                CHECK(fread(e, 2, 6, f) == 6 && e[0] == (i % CP_COLS) * CP_CELL && e[1] == (i / CP_COLS) * CP_CELL && e[2] == CP_CELL && e[3] == CP_CELL);
            }
            fclose(f);
        }
    }
    printf("camp_pic_test: %d checks, %d failed\n", runs, fails);
    return fails != 0;
}

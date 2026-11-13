// host tests for runtime/sora_kbfx.h (s216 kbfx: our two Keyblades' own sets / hit sounds and their fallbacks):
//   clang -O1 -g -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -Iruntime runtime/test/kbfx_test.c -o build/kbfx_test.exe && build/kbfx_test.exe
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "sora_kb.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

// ---- the stand-ins sora.c gives the header
#define LOG(...) ((void)0)
static struct { int kb[KB_SLOTS]; } KB;
static int g_fmots_on, g_fform, g_anti, g_mickey;  // (g_mfkb: sora_kb.h)
#define IN_ANTI (g_anti)
#define IN_MICKEY (g_mickey)
static const char *mk_kbfx_name(const char *kind) { return g_mickey ? (!strcmp(kind, "hit") ? "mk_hit" : NULL) : NULL; }
// fx.bin: a name list (index = id)
static const char *const *g_fx;
static int g_nfx;
static bool sora_fx_ready(void) { return true; }
static int sora_fx_id(const char *n) { for (int i = 0; i < g_nfx; i++) if (!strcmp(g_fx[i], n)) return i; return -1; }
static void sora_fx_trail(const float g[3], const float t[3], bool on) { (void)g, (void)t, (void)on; }
// sfx.bin
static const char *const *g_sfx;
static int g_nsfx, g_played, g_hit_atkp;
static bool sora_sfx_ready(void) { return true; }
static int sora_sfx_id(const char *n) { for (int i = 0; i < g_nsfx; i++) if (!strcmp(g_sfx[i], n)) return i; return -1; }
static int sora_sfx_play_at(int id, float gain, const float pos[3]) { (void)gain, (void)pos; g_played = id, g_hit_atkp = -1; return 0; }
static int sora_sfx_hit(int atkp, float gain, const float pos[3]) { (void)gain, (void)pos; g_played = -2, g_hit_atkp = atkp; return 0; }

#include "sora_kbfx.h"

static void fresh(const char *const *fx, int nfx, const char *const *sfx, int nsfx) {  // a new pack: the caches start over
    g_fx = fx, g_nfx = nfx, g_sfx = sfx, g_nsfx = nsfx;
    memset(g_kbfx_ids, 0, sizeof g_kbfx_ids), memset(g_kbfx_ids_alt, 0, sizeof g_kbfx_ids_alt), memset(g_kbse_sfx, 0, sizeof g_kbse_sfx);
    g_kbfx_pack = -1, g_kbown = 1, g_kbse = 1, g_kbfx = 1, g_kbfx_id = 0, g_kbfx_blade = 0, g_anti = g_mickey = 0;
}
static int trail_of(int item) { const KbfxIds *d = kbfx_use(kbfx_row(item), NULL); return d ? d->trail : -99; }

int main(void) {
    static const char *const FX_NEW[] = {"hit", "hit_finish", "hit_guard", "kb00_trail", "kb20_hit", "kb20_hit_finish", "kb20_hit_guard",
                                         "kb20_trail", "kbm0_hit", "kbm0_hit_finish", "kbm0_hit_guard", "kbm0_trail", "kb90_hit",
                                         "kb90_hit_finish", "kb90_hit_guard", "kb90_trail", "kbs1_hit", "kbs1_hit_finish", "kbs1_hit_guard",
                                         "kbs1_trail", "kbg1_hit", "kbg1_hit_finish", "kbg1_hit_guard", "kbg1_trail", "kbs1_beam", "kbs1_beam_hit"};
    static const char *const FX_OLD[] = {"hit", "hit_finish", "hit_guard", "kb00_trail", "kb20_hit", "kb20_hit_finish", "kb20_hit_guard",
                                         "kb20_trail", "kbm0_hit", "kbm0_hit_finish", "kbm0_hit_guard", "kbm0_trail", "kb90_hit",
                                         "kb90_hit_finish", "kb90_hit_guard", "kb90_trail"};
    static const char *const FX_S89[] = {"hit", "hit_finish", "hit_guard"};  // before the per-Keyblade sets
    static const char *const SFX_NEW[] = {"hit", "wall_hit", "kbs1_hit", "kbg1_hit", "kbs1_beam", "kbs1_beam_hit"};
    static const char *const SFX_OLD[] = {"hit", "wall_hit"};
    const float p[3] = {0, 0, 0};
    kb_default(KB.kb);

    // the table: our rows point at their own sets, with the s214 placeholders as alt; KH2's rows have no alt / no own sound
    int rs = kbfx_row(0x7e01), rg = kbfx_row(0x7e02);
    CHECK(rs > 0 && !strcmp(KBFX_TAB[rs].set, "s1") && !strcmp(KBFX_TAB[rs].alt, "m0") && !strcmp(KBFX_TAB[rs].se, "kbs1_hit"));
    CHECK(rg > 0 && !strcmp(KBFX_TAB[rg].set, "g1") && !strcmp(KBFX_TAB[rg].alt, "90") && !strcmp(KBFX_TAB[rg].se, "kbg1_hit"));
    for (int i = 0; i < KBFX_N; i++)
        if (i != rs && i != rg) CHECK(!KBFX_TAB[i].alt[0] && !KBFX_TAB[i].se && (!KBFX_TAB[i].set[0] || KBFX_TAB[i].set[1] == '0'));

    // 1. the new packs: own sets + own sound
    fresh(FX_NEW, 26, SFX_NEW, 6);
    KB.kb[KB_MAIN] = 0x7e01;
    CHECK(!strcmp(kbfx_name("hit"), "kbs1_hit") && !strcmp(kbfx_name("hit_finish"), "kbs1_hit_finish") && !strcmp(kbfx_name("hit_guard"), "kbs1_hit_guard"));
    CHECK(trail_of(0x7e01) == 19 && trail_of(0x7e02) == 23);
    kbse_hit(126, 1, p);
    CHECK(g_played == 2);
    KB.kb[KB_MAIN] = 0x7e02;
    CHECK(!strcmp(kbfx_name("hit"), "kbg1_hit"));
    kbse_hit(126, 1, p);
    CHECK(g_played == 3);
    // a KH2 Keyblade keeps its own set and the KH2 hit; the Kingdom Key the plain names
    KB.kb[KB_MAIN] = 0x2b;
    CHECK(!strcmp(kbfx_name("hit"), "kb20_hit") && trail_of(0x2b) == 7);
    kbse_hit(127, 1, p);
    CHECK(g_played == -2 && g_hit_atkp == 127);
    KB.kb[KB_MAIN] = 0x29;
    CHECK(!strcmp(kbfx_name("hit"), "hit") && trail_of(0x29) == 3);
    kbse_hit(126, 1, p);
    CHECK(g_played == -2 && g_hit_atkp == 126);
    // the second blade (Valor's left / Master's / Final's): its own Keyblade's sound and sparks
    KB.kb[KB_MAIN] = 0x29, KB.kb[KB_VALOR] = 0x7e02, g_kbfx_blade = 1;
    CHECK(!strcmp(kbfx_name("hit"), "kbg1_hit"));
    kbse_hit(126, 1, p);
    CHECK(g_played == 3);
    g_kbfx_blade = 0;
    kbse_hit(126, 1, p);
    CHECK(g_played == -2);
    // live kbfxid forces both
    g_kbfx_id = 0x7e01;
    CHECK(!strcmp(kbfx_name("hit_guard"), "kbs1_hit_guard"));
    kbse_hit(126, 1, p);
    CHECK(g_played == 2);
    g_kbfx_id = 0;
    // Mickey / Anti / kbfx 0 / kbse 0: the normal hit sound
    KB.kb[KB_MAIN] = 0x7e01;
    g_mickey = 1; kbse_hit(130, 1, p); CHECK(g_played == -2 && g_hit_atkp == 130); CHECK(!strcmp(kbfx_name("hit"), "mk_hit")); g_mickey = 0;
    g_anti = 1; kbse_hit(130, 1, p); CHECK(g_played == -2); CHECK(!strcmp(kbfx_name("hit"), "hit")); g_anti = 0;
    g_kbfx = 0; kbse_hit(130, 1, p); CHECK(g_played == -2); CHECK(!strcmp(kbfx_name("hit"), "hit")); g_kbfx = 1;
    g_kbse = 0; kbse_hit(130, 1, p); CHECK(g_played == -2); CHECK(!strcmp(kbfx_name("hit"), "kbs1_hit")); g_kbse = 1;
    kbse_hit(130, 1, p); CHECK(g_played == 2);
    // live kbown 0: the placeholders (sparks + ribbon), the sound stays its own
    g_kbown = 0;
    CHECK(!strcmp(kbfx_name("hit"), "kbm0_hit") && trail_of(0x7e01) == 11 && trail_of(0x7e02) == 15);
    KB.kb[KB_MAIN] = 0x7e02;
    CHECK(!strcmp(kbfx_name("hit_finish"), "kb90_hit_finish"));
    kbse_hit(126, 1, p); CHECK(g_played == 3);
    g_kbown = 1;
    CHECK(!strcmp(kbfx_name("hit_finish"), "kbg1_hit_finish") && trail_of(0x7e02) == 23);

    // 2. an older fx.bin (s89 sets, no s1 / g1) + an older sfx.bin: the placeholders, NOT the Kingdom Key's; the normal hit
    fresh(FX_OLD, 16, SFX_OLD, 2);
    KB.kb[KB_MAIN] = 0x7e01;
    CHECK(!strcmp(kbfx_name("hit"), "kbm0_hit") && trail_of(0x7e01) == 11);
    kbse_hit(126, 1, p); CHECK(g_played == -2 && g_hit_atkp == 126);
    kbse_hit(127, 1, p); CHECK(g_played == -2 && g_hit_atkp == 127);  // (asked once, cached)
    KB.kb[KB_MAIN] = 0x7e02;
    CHECK(!strcmp(kbfx_name("hit_guard"), "kb90_hit_guard") && trail_of(0x7e02) == 15);
    // 3. fx.bin before the per-Keyblade sets: everything the Kingdom Key's (as for every Keyblade then)
    fresh(FX_S89, 3, SFX_OLD, 2);
    KB.kb[KB_MAIN] = 0x7e01;
    CHECK(!strcmp(kbfx_name("hit"), "hit") && trail_of(0x7e01) == -1);
    // 4. new sfx.bin with an old fx.bin (or the other way round): each falls back on its own
    fresh(FX_OLD, 16, SFX_NEW, 6);
    CHECK(!strcmp(kbfx_name("hit"), "kbm0_hit"));
    kbse_hit(126, 1, p); CHECK(g_played == 2);
    fresh(FX_NEW, 26, SFX_OLD, 2);
    CHECK(!strcmp(kbfx_name("hit"), "kbs1_hit"));
    kbse_hit(126, 1, p); CHECK(g_played == -2);

    printf("kbfx_test: %d/%d passed\n", runs - fails, runs);
    return fails != 0;
}

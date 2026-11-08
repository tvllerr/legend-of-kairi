// abil_test.c: host tests for sora_abil.h (pause menu slice 3, step 1). sh D:/SoraBotW/recon/make_abil_test.sh
#include <stdio.h>
#include "sora_abil.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint16_t LVAB[99];

static void test_learned(void) {
    LVAB[5] = 390; LVAB[8] = 402; LVAB[11] = 390; LVAB[20] = 999;  // LV6, LV9, LV12 dup, LV21 unknown
    uint16_t o[AB_MAX_LEARN];
    g_abilfix = 0;  // (s91 review) the pre-s91 list: lvup table + Auto Valor / Wisdom only
    CHECK(ab_learned(LVAB, 5, 1, 1, 1, 1, 1, o) == 0);
    int n = ab_learned(LVAB, 30, 2, 1, 2, 2, 2, o);
    CHECK(n == 3 && o[0] == 390 && o[1] == 402 && o[2] == AB_AUTO_VALOR);
    n = ab_learned(LVAB, 99, 3, 2, 1, 1, 1, o);
    CHECK(n == 4 && o[3] == AB_AUTO_WISDOM);
    CHECK(ab_idx(82) < 0 && ab_bit(82) == 0);  // appended rows hidden
    g_abilfix = 1;  // (s91) + the story Action abilities by LV, + Auto Limit / Master / Final at form LV2
    n = ab_learned(LVAB, 1, 1, 1, 1, 1, 1, o);
    CHECK(n == 3 && o[0] == 82 && o[1] == 138 && o[2] == 158);  // Guard / Scan / Aerial Recovery at LV1
    n = ab_learned(LVAB, 99, 2, 2, 2, 2, 2, o);
    CHECK(n == 2 + 22 + 5);  // 390 402 (the dup / unknown skipped) + 22 story + 5 Auto
    CHECK(o[n - 1] == AB_AUTO_FINAL && o[n - 2] == AB_AUTO_MASTER && o[n - 3] == AB_AUTO_LIMIT);
    CHECK(AB_N == 50 && ab_idx(539) == 49);
}

static void test_mask64(void) {  // (s91 review) bits >= 32 everywhere
    abm_t b = ab_bit(539);
    CHECK(b == (abm_t)1 << 49);
    uint16_t o[] = {390, 539, 158};  // 5 + 4 + 2
    abm_t on = 0, seen = 0;
    CHECK(ab_sync(&on, &seen, o, 3, 99));
    CHECK(ab_on(on, 539) && ab_on(on, 158) && ab_used(on) == 11 && seen == ab_mask(o, 3));
    CHECK(ab_toggle(&on, ab_mask(o, 3), 539, 99) == 0 && !ab_on(on, 539) && ab_used(on) == 7);
    on = ab_mask(o, 3);
    ab_sync(&on, &seen, o, 3, 7);  // AP drop: off from the end of the order (158 then 539)
    CHECK(ab_on(on, 390) && !ab_on(on, 539) && !ab_on(on, 158));
    g_abilfix = 0;
    CHECK(ab_used(ab_mask(o, 3) | (abm_t)1 << 49) == 5);  // hidden rows cost nothing with abilfix 0
    g_abilfix = 1;
}

static void test_sync(void) {
    uint16_t o[] = {390, 402, 393};  // 5 + 1 + 5
    abm_t on = 0, seen = 0;
    CHECK(ab_sync(&on, &seen, o, 3, 6));  // old save: auto-equip in order while AP fits
    CHECK(ab_on(on, 390) && ab_on(on, 402) && !ab_on(on, 393) && ab_used(on) == 6);
    CHECK(seen == ab_mask(o, 3));
    CHECK(!ab_sync(&on, &seen, o, 3, 20));  // seen + off stays off when AP grows
    CHECK(!ab_on(on, 393));
    on &= ~ab_bit(390);  // user turns Combo Boost off
    uint16_t o2[] = {390, 402, 393, 401};
    ab_sync(&on, &seen, o2, 4, 20);  // newly learned Experience Boost auto-equips, Combo Boost stays off
    CHECK(!ab_on(on, 390) && ab_on(on, 401) && ab_on(on, 402));
    on = ab_mask(o2, 4);  // 18 AP on, level drop to AP max 7: off from the end of the order
    ab_sync(&on, &seen, o2, 4, 7);
    CHECK(ab_used(on) <= 7 && ab_on(on, 390) && ab_on(on, 402) && !ab_on(on, 401) && !ab_on(on, 393));
    ab_sync(&on, &seen, o, 1, 20);  // only 390 learned now (Dream Weapon change): unlearned bits drop
    CHECK(on == ab_bit(390));
}

static void test_toggle(void) {
    uint16_t o[] = {390, 402};
    abm_t L = ab_mask(o, 2), on = 0;
    CHECK(ab_toggle(&on, L, 393, 99) == -1);  // not learned
    CHECK(ab_toggle(&on, L, 390, 5) == 1);
    CHECK(ab_toggle(&on, L, 402, 5) == -1);   // AP short
    CHECK(ab_toggle(&on, L, 390, 5) == 0 && on == 0);
    CHECK(ab_toggle(&on, L, 12345, 99) == -1);
}

static void test_effects(void) {
    CHECK(ab_combo_pct(3, 5) == 30 && ab_combo_pct(7, 4) == 40 && ab_combo_pct(0, 4) == 0);
    CHECK(ab_boost(10, 30) == 13 && ab_boost(10, 0) == 10 && ab_boost(1, 20) == 2);
    int g = 3, a = 3, f = 1;
    ab_limits(ab_bit(393) | ab_bit(394), &g, &a, &f);
    CHECK(g == 2 && a == 2 && f == 2);
    g = 1; a = 1; f = 1;
    ab_limits(ab_bit(394), &g, &a, &f);
    CHECK(g == 1 && a == 1 && f == 1);
    CHECK(ab_exp(ab_bit(401), 10, 50, 100) == 20 && ab_exp(ab_bit(401), 10, 51, 100) == 10 && ab_exp(0, 10, 1, 100) == 10);
    CHECK(AB_N <= 64);
}

int main(void) {
    test_learned(); test_mask64(); test_sync(); test_toggle(); test_effects();
    printf("abil_test: %d/%d pass\n", runs - fails, runs);
    return fails != 0;
}

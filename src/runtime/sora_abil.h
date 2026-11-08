// sora_abil.h: KH2 Sora abilities + AP (pause menu slice 3, plan docs/plans/2026-10-30-kh2-pause-menu-slice3-abilities.md).
// Pure: no DLL state, the caller passes the level table column, levels and AP max. Masks: bit i = AB_TAB[i].
#pragma once
#include <stdint.h>
#include <stdbool.h>

// AP cost + kind (VERIFIED: sora_items.kh.json abilities, 03system.bin; kind 2 = form auto ability, 0 = support)
typedef struct { uint16_t id; uint8_t ap, kind; } AbDef;
static const AbDef AB_TAB[] = {
    {390, 5, 0}, {391, 4, 0}, {392, 2, 0}, {393, 5, 0}, {394, 2, 0}, {395, 5, 0}, {396, 3, 0}, {397, 3, 0},
    {400, 4, 0}, {401, 4, 0}, {402, 1, 0}, {403, 2, 0}, {405, 3, 0}, {406, 4, 0}, {408, 3, 0}, {409, 4, 0},
    {410, 5, 0}, {411, 2, 0}, {414, 3, 0}, {415, 4, 0}, {416, 4, 0}, {540, 5, 0}, {542, 5, 0},
    {385, 1, 2}, {386, 1, 2},  // Auto Valor / Auto Wisdom: listed once the form is LV2 (user, s79d)
    // ---- (s91 b5-abil) APPENDED (bit order = saved masks): Action / support rows missing before. id, AP, kind = the 03system
    // item record (VERIFIED, recon/S91_HELPER_ABIL.md): kind 2 / 1 = Action, 3 = Growth, 0 = Support.
    {82, 2, 2}, {137, 4, 2}, {271, 2, 2}, {267, 5, 2}, {273, 3, 2}, {262, 2, 2}, {263, 2, 2}, {559, 2, 2}, {264, 2, 2},
    {562, 2, 2}, {265, 3, 2}, {266, 3, 2}, {269, 2, 2}, {560, 2, 2}, {270, 2, 2}, {272, 3, 2}, {561, 3, 2}, {268, 4, 2},
    {198, 5, 1}, {568, 1, 2}, {387, 1, 2}, {388, 1, 2}, {138, 2, 0}, {158, 2, 0}, {539, 4, 0},
    // ---- (s110 chestfirst) APPENDED: Dodge Roll = KH2 growth 564 (LV1 of the Limit growth row, sora_limit_lv.h; growth = 0 AP, kind 3).
    // Base Sora's roll had no gate before; now the 1st chest teaches it (user s109d), the 2nd one Guard.
    {564, 0, 3},
};
#define AB_N_OLD 25  // the pre-s91 list (live "set abilfix 0" = only these)
typedef uint64_t abm_t;  // (s91) equipped / seen masks: bit i = AB_TAB[i] (50 rows > 32 bits)
static int g_abilfix = 1;  // live "set abilfix 0" = the old list + classification
// (s110, user: chest #1 Guard, #2 Second Chance, Dodge Roll with the paraglider - KH2 Roxas has no roll motion) live
// "set chestfirst 0" = old: Guard at LV 1, the roll always there. 1 (default): chest #1 = Guard, chest #2 = Second Chance
// (sora_chestabil.h); without a chest the story still teaches Guard at CHESTFIRST_LV (GUESS 10: an old save past that level
// never loses it); Dodge Roll is learned once IsGet_PlayerStole2 (the paraglider = Roxas turns into Sora) is set: g_ab_glider,
// written by sora_levels.h's world-level sweep.
static int g_chestfirst = 1, g_ab_glider = 0;
#define CHESTFIRST_LV 10
enum { AB_DODGE_ROLL = 564 };
#define AB_N ((int)(sizeof AB_TAB / sizeof AB_TAB[0]))
#define AB_MAX_LEARN AB_N
enum { AB_GUARD = 82, AB_AUTO_LIMIT = 568, AB_AUTO_MASTER = 387, AB_AUTO_FINAL = 388, AB_COMBO_BOOST = 390, AB_AIR_COMBO_BOOST = 391, AB_FINISHING_PLUS = 393, AB_NEGATIVE_COMBO = 394,
       AB_EXP_BOOST = 401, AB_FIRE_BOOST = 408, AB_BLIZZARD_BOOST = 409, AB_THUNDER_BOOST = 410,
       AB_AUTO_VALOR = 385, AB_AUTO_WISDOM = 386,
       AB_COMBO_MASTER = 539 };  // (s92: the combo gate reads it; before, only "set combo 1" did, so the row did nothing)

static int ab_idx(int id) {
    for (int i = 0; i < AB_N; i++) if (AB_TAB[i].id == id) return i < AB_N_OLD || g_abilfix ? i : -1;
    return -1;
}
static abm_t ab_bit(int id) { int i = ab_idx(id); return i < 0 ? 0 : (abm_t)1 << i; }
static bool ab_on(abm_t on, int id) { return (on & ab_bit(id)) != 0; }
static int ab_used(abm_t on) {
    int n = 0;
    for (int i = 0; i < (g_abilfix ? AB_N : AB_N_OLD); i++) if (on >> i & 1) n += AB_TAB[i].ap;  // (s91 review: abilfix 0 = the hidden rows' kept bits cost nothing)
    return n;
}

// (s91) the abilities Sora learns outside the lvup table. KH2 teaches the Action ones by STORY EVENT (00battle.bin bons rows,
// kh2_sora_levels.json sora_bonus; khwiki "Action ability": "learned after completing certain events"); the mod has no KH2
// story, so LV = 2 + (reward - 2) * 7 / 10 (GUESS: bons reward order compressed onto LV 2..49). Guard / Scan / Aerial Recovery:
// the new-game start items (sora_base row 4), LV 1. Trinity Limit: bons reward 6.
typedef struct { uint16_t id; uint8_t lv; } AbStory;
static const AbStory AB_STORY[] = {
    {82, 1}, {138, 1}, {158, 1}, {564, 1},  // (s110: with chestfirst 1, 82 -> CHESTFIRST_LV and 564 only with the paraglider, see ab_story_lv)
    {137, 2}, {273, 2}, {268, 4}, {198, 5}, {561, 6}, {269, 7}, {263, 12}, {559, 13}, {272, 15}, {562, 22}, {267, 26},
    {264, 30}, {560, 40}, {271, 42}, {265, 42}, {266, 43}, {270, 44}, {262, 45}, {539, 49},
};
#define AB_STORY_N ((int)(sizeof AB_STORY / sizeof AB_STORY[0]))
static int ab_story_lv(int i) {
    int id = AB_STORY[i].id;
    if (!g_chestfirst) return AB_STORY[i].lv;
    if (id == AB_DODGE_ROLL) return g_ab_glider ? 1 : 0;  // (0 = never: the LV loop starts at 1)
    return id == AB_GUARD ? CHESTFIRST_LV : AB_STORY[i].lv;
}

// learned ids in KH2 learn order: lvab[l - 1] = the ability learned at LV l (SL_LV[l - 1].ab[dream], 0 = none), then (s91) the
// story ones of that LV, then Auto Valor / Wisdom / Limit / Master / Final by form LV2. Returns the count (<= AB_MAX_LEARN).
static int ab_learned(const uint16_t lvab[99], int lv, int vlv, int wlv, int llv, int mlv, int flv, uint16_t order[AB_MAX_LEARN]) {
    int n = 0; abm_t got = 0;
    for (int l = 1; l <= lv && l <= 99; l++) {
        abm_t b = ab_bit(lvab[l - 1]);
        if (b && !(got & b) && n < AB_MAX_LEARN) { got |= b; order[n++] = lvab[l - 1]; }
        if (g_abilfix)
            for (int i = 0; i < AB_STORY_N; i++) {
                b = ab_bit(AB_STORY[i].id);
                if (ab_story_lv(i) == l && b && !(got & b) && n < AB_MAX_LEARN) { got |= b; order[n++] = AB_STORY[i].id; }
            }
    }
    if (vlv >= 2 && n < AB_MAX_LEARN) order[n++] = AB_AUTO_VALOR;
    if (wlv >= 2 && n < AB_MAX_LEARN) order[n++] = AB_AUTO_WISDOM;
    if (g_abilfix) {  // Limit / Master / Final LV2 abilities (sora_limit_lv.h / sora_master_lv.h / sora_final_lv.h headers)
        if (llv >= 2 && n < AB_MAX_LEARN) order[n++] = AB_AUTO_LIMIT;
        if (mlv >= 2 && n < AB_MAX_LEARN) order[n++] = AB_AUTO_MASTER;
        if (flv >= 2 && n < AB_MAX_LEARN) order[n++] = AB_AUTO_FINAL;
    }
    return n;
}
static abm_t ab_mask(const uint16_t *order, int n) {
    abm_t m = 0;
    for (int i = 0; i < n; i++) m |= ab_bit(order[i]);
    return m;
}

// on each load / learn: on-bits that are not learned drop (Dream Weapon / level change); learned + unseen -> seen,
// and on while the AP left fits (auto-equip, user s79d: anything turned off stays off); then if the on total is over
// ap_max (level dropped) turn off from the end of the learn order until it fits. Returns true if on / seen changed.
static bool ab_sync(abm_t *on, abm_t *seen, const uint16_t *order, int n, int ap_max) {
    abm_t o0 = *on, s0 = *seen;
    *on &= ab_mask(order, n);
    int used = ab_used(*on);
    for (int i = 0; i < n; i++) {
        abm_t b = ab_bit(order[i]);
        if (*seen & b) continue;
        *seen |= b;
        int c = AB_TAB[ab_idx(order[i])].ap;
        if (used + c <= ap_max) { *on |= b; used += c; }
    }
    for (int i = n - 1; i >= 0 && used > ap_max; i--) {
        abm_t b = ab_bit(order[i]);
        if (*on & b) { *on &= ~b; used -= AB_TAB[ab_idx(order[i])].ap; }
    }
    return *on != o0 || *seen != s0;
}

// menu toggle: 1 = now on, 0 = now off, -1 = refused (not learned, or AP short: KH2 error SE)
static int ab_toggle(abm_t *on, abm_t learned, int id, int ap_max) {
    abm_t b = ab_bit(id);
    if (!b || !(learned & b)) return -1;
    if (*on & b) { *on &= ~b; return 0; }
    if (ab_used(*on) + AB_TAB[ab_idx(id)].ap > ap_max) return -1;
    *on |= b;
    return 1;
}

// ---- effect math (re:0x3C12F0 / 0x3D74A0 / 0x401730, NOTES "KH2 data")
// Combo / Air Combo Boost: pct = min(combo length, hit's d74) x ComboDamageUp 10
static int ab_combo_pct(int len, int d74) { int k = len < d74 ? len : d74; return k > 0 ? k * 10 : 0; }
static int ab_boost(int dmg, int pct) { return pct > 0 ? dmg + (pct * dmg + 99) / 100 : dmg; }
#define AB_ELEMENT_UP 20  // Fire / Blizzard / Thunder Boost: sstm+0x14C ElementUp %
// Finishing Plus: finishers +1; Negative Combo: ground -1, air -1 (floored at 1)
static void ab_limits(abm_t on, int *ground, int *air, int *fin) {
    if (ab_on(on, AB_FINISHING_PLUS)) (*fin)++;
    if (ab_on(on, AB_NEGATIVE_COMBO)) {
        if (*ground > 1) (*ground)--;
        if (*air > 1) (*air)--;
    }
}
// Experience Boost: EXP x2 while HP <= 50% (GUESS: sstm ExpChance 0.5 name-matched, the exact KH2 rule not in PHH)
static uint32_t ab_exp(abm_t on, uint32_t exp, int hp, int hp_max) {
    return ab_on(on, AB_EXP_BOOST) && hp_max > 0 && hp * 2 <= hp_max ? exp * 2 : exp;
}

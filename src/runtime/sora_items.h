// sora_items.h (s131 helper B, feature 2): BotW dishes shown and treated as KH2 items.
// Live keys (default ON, 0 = the old "Dish C-17" rows and plain BotW heal):
//   set kh2item 0/1    1 (default) = the Items rows show the KH2 item name, the use adds the KH2 item's own extras
//   set kh2itemhp 0/1  0 (default) = HP healed is the dish's own CookEffect0 value (BotW balance); 1 = the KH2 item's percent of max HP
//   set kh2itemlog 0/1 log the mapping on every scan (dish, hearts, effect id/level -> KH2 item)
// Mapping (see recon/S131_HELPER_ITEMS.md): the dish's heal in quarter hearts (CookEffect0 x, VERIFIED read, s98) and its
// CookEffect1 (effect id, level; read here: the second 60-entry vec2 array, same layout as CookEffect0) pick the KH2 item.
// KH2 item names are real; the thresholds and the percents below are GUESS unless marked (Hi-Potion = half HP is the
// s110 Mickey ruling). What the effect id numbers mean is NOT known: only "has an effect (id > 0)" and "level >= 2" are used.
// Included at the top of sora_meals.h (needs rd_ok, MP, mp_change, dg_add, dg_max, link_heal, g_hud_life_max, LOG by the time of use).
#pragma once

enum { KI_NONE, KI_POTION, KI_HIPOTION, KI_MEGAPOTION, KI_ETHER, KI_MEGAETHER, KI_ELIXIR, KI_MEGALIXIR, KI_DRIVE, KI_HIDRIVE, KI_N };
static int g_kh2item = 1, g_kh2item_hp = 0, g_kh2item_log = 0;

static const struct { const char *name; int hp_pct, mp_pct, drive; } KI[KI_N] = {  // percents / drive points: GUESS (Hi-Potion 50 = s110)
    {"", 0, 0, 0},
    {"Potion", 25, 0, 0},
    {"Hi-Potion", 50, 0, 0},
    {"Mega-Potion", 100, 0, 0},       // KH2: whole party; here Sora only (party HP not touched)
    {"Ether", 0, 25, 0},
    {"Mega-Ether", 0, 100, 0},        // KH2: whole party; Sora only
    {"Elixir", 100, 100, 0},
    {"Megalixir", 100, 100, 0},       // KH2: whole party; Sora only
    {"Drive Recovery", 0, 0, 100},    // 1 bar (GUESS)
    {"High Drive Recovery", 0, 0, -1},  // -1 = fill the gauge
};

// CookEffect1 of the pouch's n-th food entry: effect id + level (floats). false when unreadable.
// GUESS: that the SECOND 60-entry vec2 array is CookEffect1 with x = effect id, y = level (s98 only verified the FIRST = CookEffect0).
// If the order is wrong every dish maps to Ether / Drive Recovery: run `set kh2itemlog 1` and check before trusting it.
static bool ki_cook1(int fi, float *id, float *lvl) {
    const SoraSites *ss = sora_sites();
    if (fi < 0 || !ss || !ss->gdt_mgr || !rd_ok(ss->gdt_mgr, 8)) return false;
    uint64_t mgr = *(const uint64_t *)(uintptr_t)ss->gdt_mgr;
    if (!rd_ok(mgr + 0xbd8, 16)) return false;
    uint64_t tp = *(const uint64_t *)(uintptr_t)(mgr + (g_meal_tp ? 0xbd8 : 0xbe0));  // (review 10: the same TriggerParam as meal_cook0)
    if (!rd_ok(tp, 8)) return false;
    uint64_t param = *(const uint64_t *)(uintptr_t)tp;
    if (!rd_ok(param + 0xf8, 16)) return false;
    uint32_t n = *(const uint32_t *)(uintptr_t)(param + 0xf8);
    uint64_t arr = *(const uint64_t *)(uintptr_t)(param + 0x100);
    if (n == 0 || n > 64 || !rd_ok(arr, 8ull * n)) return false;
    int seen = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t af = ((const uint64_t *)(uintptr_t)arr)[i];
        if (!rd_ok(af, 16)) continue;
        uint64_t el = *(const uint64_t *)(uintptr_t)(af + 8);
        if (*(const uint32_t *)(uintptr_t)af != 60 || !rd_ok(el, 8 * 60)) continue;
        if (seen++ != 1) continue;  // the SECOND 60-array = CookEffect1
        if (fi >= 60) return false;
        uint64_t o = ((const uint64_t *)(uintptr_t)el)[fi];
        if (!rd_ok(o, 0x18)) return false;
        *id = *(const float *)(uintptr_t)(o + 0x10), *lvl = *(const float *)(uintptr_t)(o + 0x14);
        return true;
    }
    return false;
}

// q = quarter hearts (-1 unknown), eff/lvl = CookEffect1 (eff < 0 = unknown)
static int ki_pick(int q, float eff, float lvl) {
    bool fx = eff > 0.5f;
    if (fx && q <= 0) return lvl >= 1.5f ? KI_MEGAETHER : KI_ETHER;  // an elixir: no heal, only an effect
    if (fx) return lvl >= 1.5f ? KI_HIDRIVE : KI_DRIVE;              // a food buff: Drive Recovery (plus its own heal)
    if (q <= 8) return KI_POTION;       // up to 2 hearts
    if (q <= 20) return KI_HIPOTION;    // up to 5 hearts
    if (q <= 39) return KI_MEGAPOTION;  // up to 9.75 hearts
    if (q <= 99) return KI_ELIXIR;
    return KI_MEGALIXIR;                // 25+ hearts (a full heal)
}

// the extras of a KH2 item on top of the dish's own heal. Returns the HP quarter hearts to heal if kh2itemhp is on, else -1.
static int ki_apply(int id, int q_own) {
    if (id <= KI_NONE || id >= KI_N) return q_own;
    int mx = g_hud_life_max > 0 ? g_hud_life_max : 12, q = q_own;
    if (g_kh2item_hp && KI[id].hp_pct > 0) q = (mx * KI[id].hp_pct + 99) / 100;
    if (KI[id].mp_pct > 0) {
        int d = (int)(MP.mp_max * (float)KI[id].mp_pct / 100.0f + 0.5f);
        mp_change(d);
    }
    if (KI[id].drive != 0) dg_add(KI[id].drive < 0 ? dg_max() * 100 : KI[id].drive, "KH2 item");
    LOG("[kh2item] %s: HP %d qh, MP +%d%%, Drive %s\n", KI[id].name, q, KI[id].mp_pct, KI[id].drive < 0 ? "full" : KI[id].drive ? "+1 bar" : "-");
    return q;
}

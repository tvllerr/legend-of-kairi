// (s75) Wisdom Form EXP + form levels (DLL, hot reload), the Wisdom twin of sora_valor_lv.h. Data: S64_HELPER_DRIVE_RECON.md
// (CONFIRMED 00battle.bin fmlv @0x3446C, form 2 rows): EXP to next 12 / 24 / 48 / 76 / 133 / 157 (450 = LV7); +1 per
// enemy defeated while in Wisdom (KH2: per Heartless, PHH CHARACTER table; BotW: any fighter, not the animals that do
// not fight = sl_kill flags 2). Per level: the form gauge = (LV - 1 + 3) bars (re:0x3C17C0), as Valor. Not here yet:
// growth Quick Run (byte 1,1,2,2,3,3,4) and abilities 386 Auto Wisdom / 412 MP Rage / 413 MP Haste.
// Level up: KH2's form level-up element (fx "form_level_up" = system.16, sfx se000 #26), once per batch.
// Saved: the levels sidecar's "wisdom" (EXP) per BotW slot; hot reload: the process env (WL_ENV).
// Live: set wexp <n> (EXP total) | set wexpmul <n> (EXP per kill) | set wlvup <n> (show a level-up).
// Included after IN_WISDOM (sora.c): sl_kill (sora_levels.h) calls wl_kill through its forward declaration.
#pragma once
#define WL_ENV "SORA_KH2_WISDOM_STATE"
#define WL_MAX_LV 7
static const uint32_t WL_EXP_AT[WL_MAX_LV] = {0, 12, 36, 84, 160, 293, 450};  // EXP total to reach LV n+1

static struct {
    bool init;
    uint32_t exp;
    int queue;  // level-ups waiting to show
    int mul;    // EXP per kill (live wexpmul, test)
} WL = {.mul = 1};

static int wl_lv(void) {
    int lv = 1;
    while (lv < WL_MAX_LV && WL.exp >= WL_EXP_AT[lv]) lv++;
    return lv;
}
static void wl_persist(void) {
    char b[48];
    snprintf(b, sizeof b, "v1 exp=%u", WL.exp);
    SetEnvironmentVariableA(WL_ENV, b);
}
static void wl_init(void) {
    if (WL.init) return;
    WL.init = true;
    char b[48];
    DWORD n = GetEnvironmentVariableA(WL_ENV, b, sizeof b);
    unsigned e = 0;
    if (n && n < sizeof b && sscanf(b, "v1 exp=%u", &e) == 1) WL.exp = e, LOG("[wisdom] hot reload: EXP %u (LV%d)\n", WL.exp, wl_lv());
}
static uint32_t wl_exp_get(void) { wl_init(); return WL.exp; }
static void wl_load(int exp, const char *why) {  // (sl_slot_load: -1 = no "wisdom" in the sidecar)
    wl_init();
    WL.exp = exp < 0 ? 0 : exp > (int)WL_EXP_AT[WL_MAX_LV - 1] ? WL_EXP_AT[WL_MAX_LV - 1] : (uint32_t)exp;
    WL.queue = 0;
    LOG("[wisdom] %s: EXP %u (LV%d)\n", why, WL.exp, wl_lv());
    wl_persist();
}

// an enemy died with Sora's credit (sora_levels.h sl_kill)
static void wl_kill(int flags) {
    if (!IN_WISDOM || (flags & 2)) return;
    wl_init();
    int lv0 = wl_lv();
    uint32_t mx = WL_EXP_AT[WL_MAX_LV - 1];
    WL.exp = WL.exp + WL.mul > mx ? mx : WL.exp + WL.mul;
    int lv = wl_lv();
    LOG("[wisdom] kill: EXP %u (LV%d)\n", WL.exp, lv);
    if (lv > lv0) WL.queue += lv - lv0, LOG("[wisdom] LEVEL UP: Wisdom LV%d (EXP %u)\n", lv, WL.exp);
    wl_persist();
}

static float wl_gauge_bars(void) { return (float)(wl_lv() - 1 + 3); }

static bool wl_live_set(const char *key, int v) {
    if (!strcmp(key, "wexp")) wl_init(), WL.exp = v < 0 ? 0 : (uint32_t)v, wl_persist(), LOG("[wisdom] set: EXP %u (LV%d)\n", WL.exp, wl_lv());
    else if (!strcmp(key, "wexpmul")) WL.mul = v < 0 ? 0 : v;
    else if (!strcmp(key, "wlvup")) WL.queue = v > 0 ? v : 1;
    else return false;
    return true;
}

// every frame after vl_show (sora.c): the same KH2 form level-up burst + SE
static int wl_show_h = -1;
static void wl_show(const float feet[3], float face, float to_u) {
    float ctr[3] = {feet[0], feet[1] + 90 * to_u, feet[2]};
    if (wl_show_h >= 0) { if (sora_fx_alive(wl_show_h)) sora_fx_move(wl_show_h, ctr, face); else wl_show_h = -1; }
    if (!WL.queue) return;
    int n = WL.queue;
    WL.queue = 0;
    bool fx = sora_fx_id("form_level_up") >= 0, se = sora_sfx_id("form_level_up") >= 0;
    if (fx) wl_show_h = sora_fx_event("form_level_up", ctr, face, to_u);
    if (se) sora_sfx_event("form_level_up", 1.0f);
    sl_form_lvup_hud(2, wl_lv(), n);  // (s76: KH2's panel "LV. n  Wisdom" + the level's ability)
    LOG("[wisdom] level-up shown (%d): fx %s, se %s\n", n, fx ? "system.16" : "MISSING (fx pack)", se ? "se000 #26" : "MISSING (sfx pack)");
}

// (s89 b4-limit) Limit Form EXP + form levels (DLL, hot reload), the Limit twin of sora_wisdom_lv.h. Data (VERIFIED
// 00battle.bin fmlv, FM form 3 rows Unk0 49..55): EXP to next 3 / 6 / 12 / 19 / 23 / 27 (90 = LV7), growth byte
// 1,1,2,2,3,3,4 (Limit's growth = Dodge Roll), abilities LV2 568 Auto Limit, LV4 405 Draw, LV6 407 Lucky Lucky.
// EXP: +1 per Limit command FINISHING move unleashed in Limit Form (sys.bar 20124, KH2's own help text) = the finisher
// step of Sonic Rave / Last Arcanum / Strike Raid / Infinity starting (sora_limit.h lm_chain). Per level: the form gauge
// = (LV - 1 + 3) bars (re:0x3C17C0), as Valor / Wisdom. Not here yet: Auto Limit, Draw, Lucky Lucky, the Dodge Roll
// growth levels (base Sora's roll has no levels yet).
// Level up: KH2's form level-up element (fx "form_level_up" = system.16, sfx se000 #26), once per batch; the panel's
// name slot SORA_LVUP_NAME_FORM(3) needs a "Limit" name in lvup.bin (hudbake lvup; missing = "Sora", the exe logs it).
// Saved: the levels sidecar's "limit" (EXP) per BotW slot; hot reload: the process env (LL_ENV).
// Live: set lexp <n> (EXP total) | set lexpmul <n> (EXP per finisher) | set llvup <n> (show a level-up).
#pragma once
#define LL_ENV "SORA_KH2_LIMIT_STATE"
#define LL_MAX_LV 7
static const uint32_t LL_EXP_AT[LL_MAX_LV] = {0, 3, 9, 21, 40, 63, 90};  // EXP total to reach LV n+1

static struct {
    bool init;
    uint32_t exp;
    int queue;  // level-ups waiting to show
    int mul;    // EXP per finisher (live lexpmul, test)
} LL = {.mul = 1};

static int ll_lv(void) {
    int lv = 1;
    while (lv < LL_MAX_LV && LL.exp >= LL_EXP_AT[lv]) lv++;
    return lv;
}
static void ll_persist(void) {
    char b[48];
    snprintf(b, sizeof b, "v1 exp=%u", LL.exp);
    SetEnvironmentVariableA(LL_ENV, b);
}
static void ll_init(void) {
    if (LL.init) return;
    LL.init = true;
    char b[48];
    DWORD n = GetEnvironmentVariableA(LL_ENV, b, sizeof b);
    unsigned e = 0;
    if (n && n < sizeof b && sscanf(b, "v1 exp=%u", &e) == 1) LL.exp = e, LOG("[limit] hot reload: EXP %u (LV%d)\n", LL.exp, ll_lv());
}
static uint32_t ll_exp_get(void) { ll_init(); return LL.exp; }
static void ll_load(int exp, const char *why) {  // (sl_slot_load: -1 = no "limit" in the sidecar)
    ll_init();
    LL.exp = exp < 0 ? 0 : exp > (int)LL_EXP_AT[LL_MAX_LV - 1] ? LL_EXP_AT[LL_MAX_LV - 1] : (uint32_t)exp;
    LL.queue = 0;
    LOG("[limit] %s: EXP %u (LV%d)\n", why, LL.exp, ll_lv());
    ll_persist();
}

// a Limit command's finishing move started in Limit Form (sora_limit.h)
static void ll_finish(const char *what) {
    ll_init();
    int lv0 = ll_lv();
    uint32_t mx = LL_EXP_AT[LL_MAX_LV - 1];
    LL.exp = LL.exp + LL.mul > mx ? mx : LL.exp + LL.mul;
    int lv = ll_lv();
    LOG("[limit] finisher %s: EXP %u (LV%d)\n", what, LL.exp, lv);
    if (lv > lv0) LL.queue += lv - lv0, LOG("[limit] LEVEL UP: Limit LV%d (EXP %u)\n", lv, LL.exp);
    ll_persist();
}

static float ll_gauge_bars(void) { return (float)(ll_lv() - 1 + 3); }

static bool ll_live_set(const char *key, int v) {
    if (!strcmp(key, "lexp")) ll_init(), LL.exp = v < 0 ? 0 : (uint32_t)v, ll_persist(), LOG("[limit] set: EXP %u (LV%d)\n", LL.exp, ll_lv());
    else if (!strcmp(key, "lexpmul")) LL.mul = v < 0 ? 0 : v;
    else if (!strcmp(key, "llvup")) LL.queue = v > 0 ? v : 1;
    else return false;
    return true;
}

// every frame after wl_show (sora.c): the same KH2 form level-up burst + SE
static int ll_show_h = -1;
static void ll_show(const float feet[3], float face, float to_u) {
    float ctr[3] = {feet[0], feet[1] + 90 * to_u, feet[2]};
    if (ll_show_h >= 0) { if (sora_fx_alive(ll_show_h)) sora_fx_move(ll_show_h, ctr, face); else ll_show_h = -1; }
    if (!LL.queue) return;
    int n = LL.queue;
    LL.queue = 0;
    bool fx = sora_fx_id("form_level_up") >= 0, se = sora_sfx_id("form_level_up") >= 0;
    if (fx) ll_show_h = sora_fx_event("form_level_up", ctr, face, to_u);
    if (se) sora_sfx_event("form_level_up", 1.0f);
    sl_form_lvup_hud(3, ll_lv(), n);  // (KH2's panel "LV. n  Limit" + the level's ability; name needs the lvup.bin row)
    LOG("[limit] level-up shown (%d): fx %s, se %s\n", n, fx ? "system.16" : "MISSING (fx pack)", se ? "se000 #26" : "MISSING (sfx pack)");
}

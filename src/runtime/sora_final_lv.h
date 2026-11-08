// (s89 b4-final) Final Form EXP + form levels (DLL, hot reload), the Final twin of sora_wisdom_lv.h. Data (CONFIRMED
// 00battle.bin fmlv @0x3446C, form 5 rows 0x51..0x57): EXP to next 12 / 24 / 48 / 76 / 133 / 157 (450 = LV7), growth
// byte 1,1,2,2,3,3,4 (= Final's growth ability Glide, PHH kh2ctl FORM_GROWTH), abilities lv2 388 (Auto Final), lv4 + lv6
// 398 (Form Boost) - abilities not applied here. EXP: +1 per enemy defeated while in Final (KH2: per Nobody, PHH
// CHARACTER table; BotW has no Nobodies = any fighter, not the animals that don't fight = sl_kill flags 2: design).
// Per level: the form gauge = (LV - 1 + 3) bars (re:0x3C17C0), as Valor / Wisdom.
// Level up: KH2's form level-up element (fx "form_level_up" = system.16, sfx se000 #26), once per batch. KH2's panel
// "LV. n  Final" (s89 trial: sl_form_lvup_hud form 5; the name / ability rows are in the s89 lvup bake, lvup_s89.bin).
// Saved: the levels sidecar's "final" (EXP) per BotW slot; hot reload: the process env (FL_ENV).
// Live: set fexp <n> (EXP total) | set fexpmul <n> (EXP per kill) | set flvup <n> (show a level-up).
// Included after IN_FINAL (sora.c): sl_kill (sora_levels.h) calls fl_kill through its forward declaration.
#pragma once
#define FL_ENV "SORA_KH2_FINAL_STATE"
#define FL_MAX_LV 7
static const uint32_t FL_EXP_AT[FL_MAX_LV] = {0, 12, 36, 84, 160, 293, 450};  // EXP total to reach LV n+1
static const uint8_t FL_GROWTH[FL_MAX_LV] = {1, 1, 2, 2, 3, 3, 4};             // fmlv growth byte = Glide level

static struct {
    bool init;
    uint32_t exp;
    int queue;  // level-ups waiting to show
    int mul;    // EXP per kill (live fexpmul, test)
} FL = {.mul = 1};

static int fl_lv(void) {
    int lv = 1;
    while (lv < FL_MAX_LV && FL.exp >= FL_EXP_AT[lv]) lv++;
    return lv;
}
static void fl_persist(void) {
    char b[48];
    snprintf(b, sizeof b, "v1 exp=%u", FL.exp);
    SetEnvironmentVariableA(FL_ENV, b);
}
static void fl_init(void) {
    if (FL.init) return;
    FL.init = true;
    char b[48];
    DWORD n = GetEnvironmentVariableA(FL_ENV, b, sizeof b);
    unsigned e = 0;
    if (n && n < sizeof b && sscanf(b, "v1 exp=%u", &e) == 1) FL.exp = e, LOG("[final] hot reload: EXP %u (LV%d)\n", FL.exp, fl_lv());
}
static uint32_t fl_exp_get(void) { fl_init(); return FL.exp; }
static void fl_load(int exp, const char *why) {  // (sl_slot_load: -1 = no "final" in the sidecar)
    fl_init();
    FL.exp = exp < 0 ? 0 : exp > (int)FL_EXP_AT[FL_MAX_LV - 1] ? FL_EXP_AT[FL_MAX_LV - 1] : (uint32_t)exp;
    FL.queue = 0;
    LOG("[final] %s: EXP %u (LV%d)\n", why, FL.exp, fl_lv());
    fl_persist();
}

// an enemy died with Sora's credit (sora_levels.h sl_kill)
static void fl_kill(int flags) {
    if (!IN_FINAL || (flags & 2)) return;
    fl_init();
    int lv0 = fl_lv();
    uint32_t mx = FL_EXP_AT[FL_MAX_LV - 1];
    FL.exp = FL.exp + FL.mul > mx ? mx : FL.exp + FL.mul;
    int lv = fl_lv();
    LOG("[final] kill: EXP %u (LV%d)\n", FL.exp, lv);
    if (lv > lv0) FL.queue += lv - lv0, LOG("[final] LEVEL UP: Final LV%d (EXP %u)\n", lv, FL.exp);
    fl_persist();
}

static float fl_gauge_bars(void) { return (float)(fl_lv() - 1 + 3); }
static int fl_glide_growth(void) { fl_init(); return FL_GROWTH[fl_lv() - 1]; }

static bool fl_live_set(const char *key, int v) {
    if (!strcmp(key, "fexp")) fl_init(), FL.exp = v < 0 ? 0 : (uint32_t)v, fl_persist(), LOG("[final] set: EXP %u (LV%d)\n", FL.exp, fl_lv());
    else if (!strcmp(key, "fexpmul")) FL.mul = v < 0 ? 0 : v;
    else if (!strcmp(key, "flvup")) FL.queue = v > 0 ? v : 1;
    else return false;
    return true;
}

// every frame after wl_show (sora.c): the same KH2 form level-up burst + SE
static int fl_show_h = -1;
static void fl_show(const float feet[3], float face, float to_u) {
    float ctr[3] = {feet[0], feet[1] + 90 * to_u, feet[2]};
    if (fl_show_h >= 0) { if (sora_fx_alive(fl_show_h)) sora_fx_move(fl_show_h, ctr, face); else fl_show_h = -1; }
    if (!FL.queue) return;
    int n = FL.queue;
    FL.queue = 0;
    bool fx = sora_fx_id("form_level_up") >= 0, se = sora_sfx_id("form_level_up") >= 0;
    if (fx) fl_show_h = sora_fx_event("form_level_up", ctr, face, to_u);
    if (se) sora_sfx_event("form_level_up", 1.0f);
    sl_form_lvup_hud(5, fl_lv(), n);  // (s89 trial: KH2's panel "LV. n  Final" + the level's ability; lvup_s89.bin has fn5 / ab106-108 / 388 / 398)
    LOG("[final] level-up shown (%d, LV%d): fx %s, se %s\n", n, fl_lv(),
        fx ? "system.16" : "MISSING (fx pack)", se ? "se000 #26" : "MISSING (sfx pack)");
}

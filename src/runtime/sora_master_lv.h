// (s89 b4-master) Master Form EXP + form levels (DLL, hot reload), the Master twin of sora_valor_lv.h / sora_wisdom_lv.h.
// Data (CONFIRMED, read s89 from kh2_first/original/00battle.bin fmlv @0x3446C, form 4 rows 0x41..0x47): EXP to next
// 40 / 80 / 140 / 224 / 448 / 668 (1600 = LV7), growth byte 1,1,2,2,3,3,4 (= Aerial Dodge LV), abilities LV2 387 (Auto
// Master, not here), LV4 + LV6 163 (Air Combo Plus: + 1 air combo hit each). KH2 gives Master EXP per Drive orb picked
// up while in Master (PHH re:0x3E8760); BotW has no orbs, so (GUESS mapping) +1 per orb of sora_drive.h's kill rule
// (dg_kill: 2 orbs for a small / medium enemy, 4 for a big one / boss; animals that do not fight 0).
// Per level: the form gauge = (LV - 1 + 3) bars (re:0x3C17C0), as Valor / Wisdom.
// Aerial Dodge (fmab @0x17A70, stride 0x44, rows = growth level - 1, CONFIRMED in our 03system.bin s89): AirDodgeHeight
// 145 / 155 / 200 / 300 / 1700 cm, AirDodgeSpeed 18 / 24 / 30 / 36 / 36 cm/frame (row 5 = MAX, never reached by fmlv).
// Base Sora's growth (KH2: the level below, re:0x3E8380) is NOT given here: his B in the air is the user's glide.
// Level up: KH2's form level-up element (fx "form_level_up" = system.16, sfx se000 #26), once per batch + the KH2 panel
// (sl_form_lvup_hud form 4: lvup.bin needs fn4 / ab102-105 / ab387 / ab163, see recon/S89_HELPER_MASTER.md).
// Saved: the levels sidecar's "master" (EXP) per BotW slot; hot reload: the process env (ML_ENV).
// Live: set mexp <n> (EXP total) | set mexpmul <n> (EXP per orb) | set mlvup <n> (show a level-up) | set fmmaster 0 (the
// form greyed / not offered) | set mdodge 0 (no Aerial Dodge) | set mbody 0 (no D334 body hits).
// Included after IN_MASTER (sora.c): sl_kill (sora_levels.h) calls ml_kill through its forward declaration.
#pragma once
#define ML_ENV "SORA_KH2_MASTER_STATE"
#define ML_MAX_LV 7
static const uint32_t ML_EXP_AT[ML_MAX_LV] = {0, 40, 120, 260, 484, 932, 1600};  // EXP total to reach LV n+1
static const uint8_t ML_GROWTH[ML_MAX_LV] = {1, 1, 2, 2, 3, 3, 4};               // fmlv growth byte = Aerial Dodge LV
typedef struct { float height, speed; } MlDodgeRow;
static const MlDodgeRow ML_DODGE[5] = {{145, 18}, {155, 24}, {200, 30}, {300, 36}, {1700, 36}};  // fmab AirDodgeHeight / Speed

static struct {
    bool init;
    uint32_t exp;
    int queue;  // level-ups waiting to show
    int mul;    // EXP per orb (live mexpmul, test)
    int on;     // live fmmaster: 0 = Master is never offered (Drive page row greyed)
} ML = {.mul = 1, .on = 1};

static int ml_lv(void) {
    int lv = 1;
    while (lv < ML_MAX_LV && ML.exp >= ML_EXP_AT[lv]) lv++;
    return lv;
}
static void ml_persist(void) {
    char b[48];
    snprintf(b, sizeof b, "v1 exp=%u on=%d", ML.exp, ML.on);
    SetEnvironmentVariableA(ML_ENV, b);
}
static void ml_init(void) {
    if (ML.init) return;
    ML.init = true;
    char b[48];
    DWORD n = GetEnvironmentVariableA(ML_ENV, b, sizeof b);
    unsigned e = 0;
    int on = 1;
    if (n && n < sizeof b && sscanf(b, "v1 exp=%u on=%d", &e, &on) >= 1)
        ML.exp = e, ML.on = on, LOG("[master] hot reload: EXP %u (LV%d)%s\n", ML.exp, ml_lv(), ML.on ? "" : ", form OFF (fmmaster 0)");
}
static uint32_t ml_exp_get(void) { ml_init(); return ML.exp; }
static void ml_load(int exp, const char *why) {  // (sl_slot_load: -1 = no "master" in the sidecar)
    ml_init();
    ML.exp = exp < 0 ? 0 : exp > (int)ML_EXP_AT[ML_MAX_LV - 1] ? ML_EXP_AT[ML_MAX_LV - 1] : (uint32_t)exp;
    ML.queue = 0;
    LOG("[master] %s: EXP %u (LV%d)\n", why, ML.exp, ml_lv());
    ml_persist();
}

// an enemy died with Sora's credit (sora_levels.h sl_kill): the Drive orbs it would drop, +1 EXP each, while in Master
static void ml_kill(int pct, int flags) {
    if (!IN_MASTER || (flags & 2)) return;
    ml_init();
    int orbs = (flags & 1) || pct > 150 ? 4 : 2;  // (dg_kill's sizes: 2 small / 2 big / 4 big orbs)
    int lv0 = ml_lv();
    uint32_t mx = ML_EXP_AT[ML_MAX_LV - 1], add = (uint32_t)(orbs * ML.mul);
    ML.exp = ML.exp + add > mx ? mx : ML.exp + add;
    int lv = ml_lv();
    LOG("[master] kill: %d orbs -> EXP %u (LV%d)\n", orbs, ML.exp, lv);
    if (lv > lv0) ML.queue += lv - lv0, LOG("[master] LEVEL UP: Master LV%d (EXP %u)\n", lv, ML.exp);
    ml_persist();
}

static float ml_gauge_bars(void) { return (float)(ml_lv() - 1 + 3); }
static int ml_air_plus(void) { int lv = ml_lv(); return (lv >= 4) + (lv >= 6); }  // (Air Combo Plus 163 at LV4 / LV6)
static int ml_dodge_lv(void) { return ML_GROWTH[ml_lv() - 1]; }
static bool ml_form_on(void) { ml_init(); return ML.on != 0; }

static int g_ma_dodge = 1, g_ma_body = 1;  // (sora_master.h: live mdodge / mbody)
static int g_ma_fx = 1;  // (sora_master.h: live mfx)
static int g_ma_fx_drop = 1;  // (s89 review, sora_master.h ma_fx_drop: live mfxdrop)
static int g_fm_master_both = 1;  // (sora_form.h: live mboth; 1 = KH2, Master takes in Donald AND Goofy)
static bool ml_live_set(const char *key, int v) {
    if (!strcmp(key, "mexp")) ml_init(), ML.exp = v < 0 ? 0 : (uint32_t)v, ml_persist(), LOG("[master] set: EXP %u (LV%d)\n", ML.exp, ml_lv());
    else if (!strcmp(key, "mexpmul")) ML.mul = v < 0 ? 0 : v;
    else if (!strcmp(key, "mlvup")) ML.queue = v > 0 ? v : 1;
    else if (!strcmp(key, "fmmaster")) ml_init(), ML.on = v != 0, ml_persist(), LOG("[master] Master Form %s (set fmmaster)\n", ML.on ? "ON" : "OFF: greyed");
    else if (!strcmp(key, "mdodge")) g_ma_dodge = v != 0, LOG("[master] Aerial Dodge %s\n", g_ma_dodge ? "on" : "off");
    else if (!strcmp(key, "mbody")) g_ma_body = v != 0, LOG("[master] D334 body hits %s\n", g_ma_body ? "on" : "off");
    else if (!strcmp(key, "mfx")) g_ma_fx = v != 0, LOG("[master] attack fx %s\n", g_ma_fx ? "on" : "off");
    else if (!strcmp(key, "mfxdrop")) g_ma_fx_drop = v != 0, LOG("[master] attack fx fade when the swing ends %s\n", g_ma_fx_drop ? "on" : "off (old)");
    else if (!strcmp(key, "mboth")) g_fm_master_both = v != 0, LOG("[master] Master absorbs %s\n", g_fm_master_both ? "Donald + Goofy (KH2)" : "Donald only");
    else return false;
    return true;
}

// every frame after wl_show (sora.c): the same KH2 form level-up burst + SE + panel
static int ml_show_h = -1;
static void ml_show(const float feet[3], float face, float to_u) {
    float ctr[3] = {feet[0], feet[1] + 90 * to_u, feet[2]};
    if (ml_show_h >= 0) { if (sora_fx_alive(ml_show_h)) sora_fx_move(ml_show_h, ctr, face); else ml_show_h = -1; }
    if (!ML.queue) return;
    int n = ML.queue;
    ML.queue = 0;
    bool fx = sora_fx_id("form_level_up") >= 0, se = sora_sfx_id("form_level_up") >= 0;
    if (fx) ml_show_h = sora_fx_event("form_level_up", ctr, face, to_u);
    if (se) sora_sfx_event("form_level_up", 1.0f);
    sl_form_lvup_hud(4, ml_lv(), n);  // (KH2 form 4 = Master: "LV. n  Master" + the level's ability)
    LOG("[master] level-up shown (%d): fx %s, se %s\n", n, fx ? "system.16" : "MISSING (fx pack)", se ? "se000 #26" : "MISSING (sfx pack)");
}

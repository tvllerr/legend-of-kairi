// (s70) Valor Form EXP + form levels, Drive Forms slice 3 part 2 (DLL, hot reload). Data: S64_HELPER_DRIVE_RECON.md 2
// (CONFIRMED 00battle.bin fmlv @0x3446C, form 1 rows): EXP to next 80 / 160 / 280 / 448 / 560 / 672 (2200 = LV7);
// +1 per hit landed in Valor (PHH, re:0x3E86B0). Per level: the form gauge = (LV - 1 + 3) bars (re:0x3C17C0), Combo Plus
// at LV4 + LV6 (ability 162: ground combo +1 each), High Jump growth byte 1,1,2,2,3,3,4 -> fmab HighJumpHeight 235 / 310
// / 385 / 585 cm in Valor; base Sora gets growth - 1 (0 = none: his prty 185). Auto Valor (LV2, ability 385) = not here.
// Level up: KH2's form level-up element (fx "form_level_up" = system.16, sfx se000 #26), once per batch.
// Saved: the levels sidecar's "valor" (EXP) per BotW slot; hot reload: the process env (VL_ENV).
// Live: set vexp <n> (EXP total) | set vgrow 0 (base Sora: no High Jump from Valor) | set vexpmul <n> (EXP per hit).
#pragma once
#define VL_ENV "SORA_KH2_VALOR_STATE"
#define VL_MAX_LV 7
static const uint32_t VL_EXP_AT[VL_MAX_LV] = {0, 80, 240, 520, 968, 1528, 2200};  // EXP total to reach LV n+1
static const uint8_t VL_GROWTH[VL_MAX_LV] = {1, 1, 2, 2, 3, 3, 4};
static const float VL_HIGH_JUMP[4] = {235, 310, 385, 585};  // fmab HighJumpHeight LV1-4

static struct {
    bool init;
    uint32_t exp;
    int queue;      // level-ups waiting to show
    int grow_base;  // live vgrow
    int mul;        // EXP per hit (live vexpmul, test)
} VL = {.grow_base = 1, .mul = 1};

static int vl_lv(void) {
    int lv = 1;
    while (lv < VL_MAX_LV && VL.exp >= VL_EXP_AT[lv]) lv++;
    return lv;
}
static void vl_persist(void) {
    char b[48];
    snprintf(b, sizeof b, "v1 exp=%u", VL.exp);
    SetEnvironmentVariableA(VL_ENV, b);
}
static void vl_init(void) {
    if (VL.init) return;
    VL.init = true;
    char b[48];
    DWORD n = GetEnvironmentVariableA(VL_ENV, b, sizeof b);
    unsigned e = 0;
    if (n && n < sizeof b && sscanf(b, "v1 exp=%u", &e) == 1) VL.exp = e, LOG("[valor] hot reload: EXP %u (LV%d)\n", VL.exp, vl_lv());
}
static uint32_t vl_exp_get(void) { vl_init(); return VL.exp; }
static void vl_load(int exp, const char *why) {  // (sl_slot_load: -1 = no "valor" in the sidecar)
    vl_init();
    VL.exp = exp < 0 ? 0 : exp > (int)VL_EXP_AT[VL_MAX_LV - 1] ? VL_EXP_AT[VL_MAX_LV - 1] : (uint32_t)exp;
    VL.queue = 0;
    LOG("[valor] %s: EXP %u (LV%d)\n", why, VL.exp, vl_lv());
    vl_persist();
}

// a hit landed in Valor (sora.c blade_hits)
static void vl_hit(void) {
    vl_init();
    int lv0 = vl_lv();
    uint32_t mx = VL_EXP_AT[VL_MAX_LV - 1];
    VL.exp = VL.exp + VL.mul > mx ? mx : VL.exp + VL.mul;
    int lv = vl_lv();
    if (lv > lv0) VL.queue += lv - lv0, LOG("[valor] LEVEL UP: Valor LV%d (EXP %u)\n", lv, VL.exp);
    vl_persist();
}

// the numbers that depend on the form level
static float vl_gauge_bars(void) { return (float)(vl_lv() - 1 + 3); }
static int vl_combo_plus(void) { int lv = vl_lv(); return (lv >= 4) + (lv >= 6); }
static float vl_jump_h(void) { return VL_HIGH_JUMP[VL_GROWTH[vl_lv() - 1] - 1]; }
static float vl_base_jump_h(float base) {
    int g = VL.grow_base ? VL_GROWTH[vl_lv() - 1] - 1 : 0;
    return g > 0 && VL_HIGH_JUMP[g - 1] > base ? VL_HIGH_JUMP[g - 1] : base;
}

static bool vl_live_set(const char *key, int v) {
    if (!strcmp(key, "vexp")) vl_init(), VL.exp = v < 0 ? 0 : (uint32_t)v, vl_persist(), LOG("[valor] set: EXP %u (LV%d)\n", VL.exp, vl_lv());
    else if (!strcmp(key, "vgrow")) VL.grow_base = v != 0;
    else if (!strcmp(key, "vexpmul")) VL.mul = v < 0 ? 0 : v;
    else if (!strcmp(key, "vlvup")) VL.queue = v > 0 ? v : 1;
    else return false;
    return true;
}

// every frame after sl_lvup_show (sora.c): KH2's form level-up burst bound to his centre + its SE, once per batch
static int vl_show_h = -1;
static void vl_show(const float feet[3], float face, float to_u) {
    float ctr[3] = {feet[0], feet[1] + 90 * to_u, feet[2]};
    if (vl_show_h >= 0) { if (sora_fx_alive(vl_show_h)) sora_fx_move(vl_show_h, ctr, face); else vl_show_h = -1; }
    if (!VL.queue) return;
    int n = VL.queue;
    VL.queue = 0;
    bool fx = sora_fx_id("form_level_up") >= 0, se = sora_sfx_id("form_level_up") >= 0;
    if (fx) vl_show_h = sora_fx_event("form_level_up", ctr, face, to_u);
    if (se) sora_sfx_event("form_level_up", 1.0f);
    sl_form_lvup_hud(1, vl_lv(), n);  // (s76: KH2's panel "LV. n  Valor" + the level's ability)
    LOG("[valor] level-up shown (%d): fx %s, se %s\n", n, fx ? "system.16" : "MISSING (fx pack)", se ? "se000 #26" : "MISSING (sfx pack)");
}

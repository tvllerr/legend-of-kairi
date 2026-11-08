// (s148 sumlv) KH2 Summon EXP + Summon Level (DLL, hot reload). KH2 truth: recon/S139_RESEARCH_SUMEXP.md (KH2 FM exe Ghidra + 00battle.bin).
//   * ONE level + ONE EXP pool for every summon (KH2 globals 0x1409ACDD6 / 0x1409ACF94). fmlv form 0 rows: EXP to next 6 / 16 / 25 / 42 / 63 / 98,
//     LV7 = max (cumulative 6, 22, 47, 89, 152, 250). No ability per level.
//   * EXP: 1 per whole-bar boundary the summon gauge falls across (rva 3c0b50 -> 40d530): bar index = floor(t / 600), minus 1 when t == max.
//     A full N-bar gauge drained to 0 = N - 1 EXP. Time drain AND Limit costs count; refills give none.
//   * the gauge at cast = (level + 3) bars x 600 KH frames (rva 3de610 / 3c2740): LV1 4 bars .. LV7 10 bars. Nothing else reads the level.
//   * cap by charms owned (rva 40d680): 1 -> LV3, 2 -> LV4, 3 -> LV5, 4 -> LV7. At the cap no EXP is kept.
//     Charms here = the 4 summons' unlocks (Chicken Little = paraglider, Stitch = Vah Ruta, Genie = Vah Naboris, Peter Pan = Vah Medoh). Stitch is
//     PARKED (not in the Summon list) but his charm still counts when Vah Ruta is done (s148 call: else LV7 could never be reached).
//   * hits Sora lands while a summon is out refill its gauge: atkp DriveDrain (x0.75 common enemy) x ChangeTimeAddRate 12 frames, only while
//     the gauge is above 0, clamped to max (rva 3a8800 -> 3a9cf0, VERIFIED for Sora's own hits; the summon's own hits: GUESS no).
//   * level up: KH2's form level-up element (fx "form_level_up" = system.16, se000 #26), once per batch. KH2's panel line 0xC5BD is NOT shown
//     (the lvup panel has no "Summon" name baked: a later data slice).
//   * Debug: EXP earned while Debug Menu Unlock Summons / Unlock Drive Forms or debugall is ON is thrown away when it goes OFF (the s142 rule).
// Saved: the levels sidecar's "summon" (EXP total) per BotW slot; hot reload: the process env (SU_ENV).
// Live: set sumexp <n> (EXP total) | sumcap 0 (ignore the charm cap) | sumrefill 0 (no hit refill) | sumlog 1 (per-EXP log) | sumlvup 1 (show the fx)
#pragma once
#define SU_ENV "SORA_KH2_SUMMON_STATE"
#define SU_MAX_LV 7
static const uint32_t SU_EXP_AT[SU_MAX_LV] = {0, 6, 22, 47, 89, 152, 250};  // EXP total to reach LV n+1

static struct {
    bool init;
    uint32_t exp;
    int queue;       // level-ups waiting to show
    int cap_on, refill, log;
    float prev;      // the gauge value last tracked (-1 = none)
    int64_t snap;    // the real EXP while a debug unlock is ON (-1 = none)
} SU = {.cap_on = 1, .refill = 1, .prev = -1, .snap = -1};

static int su_charms_(void);   // (sora_sumlv_late.h: needs the owned rule of sora_summon_menu.h)
static bool su_debug_(void);   // (sora_sumlv_late.h: needs sora_dbgmenu.h)

static int su_lv_of(uint32_t e) {
    int lv = 1;
    while (lv < SU_MAX_LV && e >= SU_EXP_AT[lv]) lv++;
    return lv;
}
static int su_lv(void) { return su_lv_of(SU.exp); }
static int su_cap(void) {
    if (!SU.cap_on) return SU_MAX_LV;
    int n = su_charms_();
    return n >= 4 ? 7 : n == 3 ? 5 : n == 2 ? 4 : 3;  // (rva 40d680: 2 + charms, 6 -> 7; 0 charms = no summon to cast anyway)
}
static void su_persist(void) {
    char b[64];
    snprintf(b, sizeof b, "v1 exp=%u snap=%lld", SU.exp, (long long)SU.snap);
    SetEnvironmentVariableA(SU_ENV, b);
}
static void su_init(void) {
    if (SU.init) return;
    SU.init = true;
    char b[64];
    DWORD n = GetEnvironmentVariableA(SU_ENV, b, sizeof b);
    unsigned e = 0;
    long long s = -1;
    if (n && n < sizeof b && sscanf(b, "v1 exp=%u snap=%lld", &e, &s) >= 1)
        SU.exp = e > SU_EXP_AT[SU_MAX_LV - 1] ? SU_EXP_AT[SU_MAX_LV - 1] : e, SU.snap = s,
        LOG("[sumlv] hot reload: EXP %u (LV%d)%s\n", SU.exp, su_lv(), SU.snap >= 0 ? ", debug snapshot kept" : "");
}
// what the sidecar saves: the real EXP (a debug snapshot wins)
static uint32_t su_exp_get(void) { su_init(); return SU.snap >= 0 ? (uint32_t)SU.snap : SU.exp; }
static void su_load(int exp, const char *why) {  // (sl_slot_load: -1 = no "summon" in the sidecar = LV1; sl_forms_reset: New Game)
    su_init();
    SU.exp = exp < 0 ? 0 : exp > (int)SU_EXP_AT[SU_MAX_LV - 1] ? SU_EXP_AT[SU_MAX_LV - 1] : (uint32_t)exp;
    if (SU.snap >= 0) SU.snap = SU.exp;  // (a load while a debug row is ON: this slot's value becomes the real one, as dbm_resume)
    SU.queue = 0;
    LOG("[sumlv] %s: EXP %u (Summon LV%d)\n", why, SU.exp, su_lv());
    su_persist();
}

// n whole bars crossed downward (40d530): EXP += n, level ups while the cap allows; at the cap the EXP stops there
static void su_exp_add(int n, const char *why) {
    su_init();
    if (n <= 0) return;
    int lv0 = su_lv(), cap = su_cap();
    if (lv0 >= cap) { if (SU.log) LOG("[sumlv] +%d EXP (%s) dropped: Summon LV%d = the cap (%d charm(s))\n", n, why, lv0, su_charms_()); return; }
    uint32_t mx = SU_EXP_AT[cap - 1];
    SU.exp = SU.exp + (uint32_t)n > mx ? mx : SU.exp + (uint32_t)n;
    int lv = su_lv();
    LOG("[sumlv] +%d EXP (%s): EXP %u, Summon LV%d%s\n", n, why, SU.exp, lv, lv < SU_MAX_LV ? "" : " (max)");
    if (lv > lv0) SU.queue += lv - lv0, LOG("[sumlv] LEVEL UP: Summon LV%d -> the next cast has %d bars\n", lv, lv + 3);
    su_persist();
}

// the summon's gauge at a cast: (level + 3) bars (3de610)
static int su_gauge_bars(void) { su_init(); return su_lv() + 3; }

// bar index of the KH2 boundary rule (3c0b50): floor(t / 600), minus 1 when the gauge is full
static int su_bar_idx_(float t, float total) {
    int i = (int)(t / 600.0f);
    return t >= total - 0.001f ? i - 1 : i;
}
// sm_cast: tracking starts at the full gauge
static void su_track_start(float total) { su_init(); SU.prev = total; }
// sm_tick (each frame a summon is out) + sm_end: the boundaries crossed since the last call
static void su_track(float t, float total, const char *why) {
    if (SU.prev < 0) return;
    if (t < 0) t = 0;
    int n = su_bar_idx_(SU.prev, total) - su_bar_idx_(t, total);
    SU.prev = t;
    if (n > 0) su_exp_add(n, why);
}
static void su_track_stop(void) { SU.prev = -1; }

// dg_hit while a summon is out: DriveDrain (x0.75 common) x 12 frames back into its gauge (3a9cf0 state 3); returns the frames added
static float su_refill(float drain, float *t, float total) {
    if (!SU.refill || !(*t > 0) || drain <= 0) return 0;
    float add = drain * 12.0f, before = *t;
    *t = *t + add > total ? total : *t + add;
    if (SU.log) LOG("[sumlv] hit refill: %.1f drain x 12 -> gauge %.0f -> %.0f of %.0f\n", drain, before, *t, total);
    return *t - before;
}

// each driven frame (sm_tick): the debug snapshot (s142 rule: EXP earned under a debug unlock is thrown away at OFF)
static void su_debug_tick(void) {
    su_init();
    bool on = su_debug_();
    if (on && SU.snap < 0) SU.snap = SU.exp, su_persist(), LOG("[sumlv] debug ON: Summon EXP snapshot %u (LV%d)\n", SU.exp, su_lv());
    else if (!on && SU.snap >= 0) {
        uint32_t was = SU.exp;
        SU.exp = (uint32_t)SU.snap, SU.snap = -1, SU.queue = 0, su_persist();
        LOG("[sumlv] debug OFF: Summon EXP back to %u (LV%d), %u -> thrown away\n", SU.exp, su_lv(), was);
    }
}

static bool su_live_set(const char *key, int v) {
    if (!strcmp(key, "sumexp")) su_init(), SU.exp = v < 0 ? 0 : v > (int)SU_EXP_AT[SU_MAX_LV - 1] ? SU_EXP_AT[SU_MAX_LV - 1] : (uint32_t)v, su_persist(), LOG("[sumlv] set: EXP %u (Summon LV%d, cap LV%d)\n", SU.exp, su_lv(), su_cap());
    else if (!strcmp(key, "sumcap")) SU.cap_on = v != 0, LOG("[sumlv] charm cap %s (LV%d)\n", SU.cap_on ? "on (KH2)" : "OFF", su_cap());
    else if (!strcmp(key, "sumrefill")) SU.refill = v != 0, LOG("[sumlv] hit refill %s\n", SU.refill ? "on (KH2)" : "off");
    else if (!strcmp(key, "sumlog")) SU.log = v != 0;
    else if (!strcmp(key, "sumlvup")) SU.queue = v > 0 ? v : 1;
    else return false;
    return true;
}

// every frame next to vl_show (sora.c): KH2's form level-up burst on Sora's centre + its SE, once per batch (KH2 uses the same element 16)
static int su_show_h = -1;
static void su_show(const float feet[3], float face, float to_u) {
    float ctr[3] = {feet[0], feet[1] + 90 * to_u, feet[2]};
    if (su_show_h >= 0) { if (sora_fx_alive(su_show_h)) sora_fx_move(su_show_h, ctr, face); else su_show_h = -1; }
    if (!SU.queue) return;
    int n = SU.queue;
    SU.queue = 0;
    bool fx = sora_fx_id("form_level_up") >= 0, se = sora_sfx_id("form_level_up") >= 0;
    if (fx) su_show_h = sora_fx_event("form_level_up", ctr, face, to_u);
    if (se) sora_sfx_event("form_level_up", 1.0f);
    LOG("[sumlv] level-up shown (%d): Summon LV%d, fx %s, se %s\n", n, su_lv(), fx ? "system.16" : "MISSING (fx pack)", se ? "se000 #26" : "MISSING (sfx pack)");
}

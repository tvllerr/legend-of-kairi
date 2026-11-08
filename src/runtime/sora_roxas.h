// (s109 roxas) KH2 Roxas as the player until the paraglider, then Sora + Donald + Goofy. Plan + data: recon/S109_HELPER_ROXAS.md.
//
// Roxas is NOT a Drive Form: he reuses the Drive Forms' machinery (FM_FORMS row FORM_ROXAS = his own draw actor + motion table
// + pose bake + hit table, sora.c's IN_ROXAS chains for the ptya set / speeds / turn locks), but FM.st stays FM_BASE: no gauge,
// no Drive page (fm_top_state 5), no Magic (fm_no_magic), no glide (fm_glide_ok), no Drive Form command (fm_drive refuses).
// mot_of() / mot_slot() never fall back to Sora's slots while IN_ROXAS: an action whose KH2 slot Roxas lacks (Dodge Roll
// 836.., Glide 820.., Air Dodge, magic casts, summons, CHANGEFORM) is simply off. Party: P.on is forced off while he is
// Roxas (KH2: Roxas fights alone), the user's value comes back when he turns into Sora.
//
// Switch rule ("set char <0|1|2>"): 0 = auto (Roxas while the BotW flag IsGet_PlayerStole2 (the paraglider) is false), 1 = Roxas
// now, 2 = Sora now. Default 0 (auto) since s110 (was 2 until the first in-game test); the paraglider is read through the exe's
// linkpos_gdt_bool (as sora_unlock.h). A flag read that fails (no gdt manager: title / loading) keeps the current body; a change
// of the wanted body must hold ROX_HOLD_MS before it applies (a save load briefly reads every flag false) and waits for a free
// moment (no attack / reaction / Drive Form running).
#pragma once

static bool mk_body(void);  // (s110 b22-mickey, sora_mickey.h: Mickey's body is out)
#define ROX_HOLD_MS 1200
#define ROX_HINT_MS 60000  // (s111 roxload) how long a slot-load body hint waits for a free moment / the assets
static int g_char = 0;  // 0 auto, 1 Roxas, 2 Sora; env SORA_CHAR sets the start value (hot reload keeps it that way). (s110: default 0 = auto, user-tested s109d)
static struct {
    bool on, party_was, init;
    int want;            // the last wanted body (1 Roxas, 0 Sora), -1 none yet
    ULONGLONG want_since;
    bool warned;
} RX = {.want = -1};
static int rox_on_now(void) { return RX.on; }  // (s111 roxload, forward-declared in sora_levels.h)
// (s146 outfit) Debug Menu "Become Sora" (dbmsora 1): Sora is the body whatever the story / "set char" says (g_dbm_sora is declared in
// sora_levels.h); never saved (the sidecar's "roxas" field is skipped while it is on); Off = back to the char setting (0 = auto).

static bool rox_assets_ok(void) {
    if (!g_fm_rox_load) g_fm_rox_load = g_fm_load_now = true;  // (s109 review: his draw actor loads on the first want only)
    fm_init(), fm_load_actor();
    return FM.set_body && FM.actor[FORM_ROXAS] > 0 && g_nfmots_tab[FORM_ROXAS] > 0 && g_nrhslots > 0;
}

// (s109 review) the playing locomotion / jump / fall motion re-picked from the new body's table: S.motion = -1 left an
// airborne switch with no motion (nothing drawn) until he landed, since the ACT_JUMP branch only sets Fall at the apex
static void rox_motion_reset(void) {
    S.special = SP_NONE, S.next = 0;
    S.mot = S.motion >= 0 ? mot_of(S.motion) : NULL;
    if (!S.mot) S.motion = -1, set_motion(ACT_AIR(S.act) ? MOT_FALL : MOT_IDLE);
}

static void rox_enter(void) {
    if (fm_in_form()) fm_revert("char: Roxas");
    g_fform = FORM_ROXAS, g_fmots_on = true;
    FM.set_body(FM.actor[FORM_ROXAS]);
    RX.on = true, RX.party_was = P.on;
    P.on = false, party_all_off();
    rox_motion_reset();
    LOG("[roxas] ROXAS: body actor %d, %d motion slots, %d hit slots, party off (was %d)\n", FM.actor[FORM_ROXAS],
        g_nfmots_tab[FORM_ROXAS], g_nrhslots, RX.party_was);
}

static void rox_leave(void) {
    g_fmots_on = false, g_fform = FORM_VALOR;  // (g_fform back to its start value: kbfx / sfx / fx read it as "the last form")
    if (FM.set_body) FM.set_body(ofx_actor_base());  // (s146 outfit: 0 while Normal)
    RX.on = false;
    if (RX.party_was) P.on = true;  // (party_core_tick places Donald + Goofy beside him next tick)
    rox_motion_reset();
    LOG("[roxas] SORA: base body back, party %s\n", P.on ? "on (Donald + Goofy join)" : "off");
}

// the body the story wants: 1 Roxas, 0 Sora, -1 unknown (keep)
static int rox_wanted(void) {
    if (g_dbm_sora) return 0;  // (s146 outfit; before "set char 1": the Debug Menu row is a debug override of everything)
    if (g_char == 1) return 1;
    if (g_char == 2) return 0;
    int glider = 0;
    if (!SLX.gdt_bool || !SLX.gdt_bool("IsGet_PlayerStole2", &glider)) return -1;
    return glider ? 0 : 1;
}

static void rox_tick(void) {
    if (!RX.init) {
        RX.init = true;
        const char *e = getenv("SORA_CHAR");
        if (e && *e) g_char = atoi(e) < 0 ? 0 : atoi(e) > 2 ? 2 : atoi(e);
    }
    if (RX.on && P.on) RX.party_was = true, P.on = false, party_all_off();  // ("set party 1" while Roxas: no party now, but it is on when Sora comes back)
    ULONGLONG now = GetTickCount64();
    if (g_rox_hint >= 0 && g_loading) g_rox_hint_at = now;  // (the 60 s window runs from the first world frame)
    if (g_loading || mk_body()) return;  // (s110: not while Mickey's rescue body is out)
    // (s111 roxload) the body the loaded slot was saved with applies at once (no hold, no flag read); the live flag takes over after
    int hw = g_rox_hint;
    if (hw >= 0 && (!g_roxload || g_char != 0 || g_dbm_sora || now - g_rox_hint_at > ROX_HINT_MS)) {
        LOG("[roxas] slot body hint dropped (%s)\n", g_char != 0 || g_dbm_sora || !g_roxload ? "mode changed" : "timeout");
        hw = g_rox_hint = -1, g_rox_hold = 0;
    }
    if (hw >= 0 && hw == (RX.on ? 1 : 0)) hw = g_rox_hint = -1, g_rox_hold = 0;  // (already the right body)
    int w = hw >= 0 ? hw : rox_wanted();
    if (w < 0 || w == (RX.on ? 1 : 0)) { RX.want = -1; return; }
    if (RX.want != w) RX.want = w, RX.want_since = now;
    bool forced = g_char != 0 || g_dbm_sora || hw >= 0;  // (a live "set char 1 / 2" applies at once, only auto waits)
    if (!forced && now - RX.want_since < ROX_HOLD_MS) return;
    if (S.special != SP_NONE && S.special != SP_DRIVE) return;  // (not in the middle of a swing / reaction)
    if (S.act == ACT_GLIDE || S.act == ACT_GLIDE_HOVER) return;  // (s109 review: Roxas has no glide slots: a mid-glide switch drew nothing until the glide ended)
    if (w == 1 && !rox_assets_ok()) {
        if (hw >= 0) g_rox_hold = 1;  // (s111 roxload: no Sora while Roxas' assets load)
        if (!RX.warned) RX.warned = true, LOG("[roxas] wanted but not ready (draw actor %d, motions %d, hits %d; data/forms/roxas_*.bin installed? exe actor table full?)\n",
                                              FM.actor[FORM_ROXAS], g_nfmots_tab[FORM_ROXAS], g_nrhslots);
        return;
    }
    RX.warned = false;
    if (w == 1) rox_enter();
    else rox_leave();
    if (hw >= 0) LOG("[roxas] slot body %s applied at once\n", w ? "Roxas" : "Sora");
    g_rox_hint = -1, g_rox_hold = 0;
    RX.want = -1;
}

// (s111 roxmenu) 1 (default) = while Roxas is the body, BotW's inventory screen (invsora) and the KH2 camp menu (menuwalk + the
// member rows) show Roxas alone instead of Sora (+ Donald + Goofy). Live "set roxmenu 0" = the old behaviour. S111_HELPER_ROXMENU.md
static int g_roxmenu = 1;
// the draw actor the menus pose: Roxas' own while he is the body (the exe's body redirect only covers the world draw), else 0 = Sora
static int rm_actor(void) { return g_roxmenu && IN_ROXAS && FM.actor[FORM_ROXAS] > 0 ? FM.actor[FORM_ROXAS] : 0; }
// Roxas' Keyblade shown (the world idle may have left his actor sheathed); once per menu open is enough but it is cheap
static void rm_weapon(int id) { if (id > 0 && PD.weapon) PD.weapon(id, true); }

static bool rox_live_set(const char *key, int v) {
    if (!strcmp(key, "roxmenu")) {
        g_roxmenu = v != 0;
        LOG("[roxmenu] roxmenu %d (1 Roxas alone in the BotW inventory + KH2 menu while he is the body, 0 old: Sora + party)\n", g_roxmenu);
        return true;
    }
    if (!strcmp(key, "roxcmd")) {
        g_roxcmd = v != 0;
        LOG("[roxcmd] roxcmd %d (%s)\n", g_roxcmd, g_roxcmd ? "Roxas: no MP bar, no Drive gauge, command menu Attack + Items only, no L1 shortcuts" : "old: the full Sora HUD + command menu");
        return true;
    }
    if (!strcmp(key, "roxface")) { g_roxface = v != 0; LOG("[roxas] roxface %d (HUD portrait: %s)\n", g_roxface, g_roxface ? "Roxas while he is the body" : "always Sora"); return true; }
    if (!strcmp(key, "roxload")) {
        g_roxload = v != 0;
        if (!g_roxload) g_rox_hint = -1, g_rox_hold = 0;
        LOG("[roxas] roxload %d (%s)\n", g_roxload, g_roxload ? "a slot load applies the saved body at once" : "old: the live flag + 1.2 s hold decide");
        return true;
    }
    if (strcmp(key, "char")) return false;
    g_char = v < 0 ? 0 : v > 2 ? 2 : v;
    LOG("[roxas] char %d (%s)\n", g_char, g_char == 0 ? "auto: Roxas until the paraglider" : g_char == 1 ? "Roxas" : "Sora");
    return true;
}

// sora_camp.h: the KH2 pause menu (camp) glue, slice 1 (docs/plans/2026-10-30-kh2-pause-menu-slice1.md Task 7).
// + from a running world opens KH2's top page over BotW's own pause (BotW still gets the +, the opaque backdrop hides
// its screen); B / + closes it (the camp pulses + until BotW unpauses), "BotW Menu" hands the pause to BotW's menu.
// The page / state machine is pure (sora_camp_page.h, host-tested); this file feeds it the pad, PAUSE_STATE and the
// values, plays its SEs and sends its quads to the exe (sora_hud_quads, by GetProcAddress: an older exe = camp off).
// Included by sora.c after sora_form.h (needs SL / sl_lv, P.m / PARTY, GO, SLX, kh_axis).
#pragma once
#include "sora_camp_page.h"
#include "sora_camp_pic.h"  // (s120 itempic) the Equipment page's Keyblade picture quad
#include "sora_armor.h"     // (s120) Link's worn armor for the Equipment page's Armor rows

#define CAMP_QUADS 2048
static struct {
    bool tried, ok;  // camp_seq.bin + sora_hud_quads looked for (once per DLL copy); ok = both there
    void (*quads)(const SoraHudQuad *, int);
    void (*quads2)(const SoraHudQuad *, int, int);  // (s94) sora_hud_quads2 (NULL before the s94 exe)
    uint8_t *buf;
    CampSeq seq;
    CampPage page;
    CpVals v;
    bool drew, db_stale;
    int log;
} CAMP = {.log = 1};
static SoraHudQuad g_camp_q[CAMP_QUADS];
static int dbm_set(int row, int val, const char *why);
static int dbm_state(int row);  // (s140 dbgmenu, sora_dbgmenu.h)

static void camp_load(void) {
    CAMP.tried = true;
    CAMP.quads = (void (*)(const SoraHudQuad *, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_quads");
    if (!CAMP.quads) { LOG("[sora] camp: sora_hud_quads not in this exe, + stays BotW's\n"); return; }
    CAMP.quads2 = (void (*)(const SoraHudQuad *, int, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_quads2");
    const char *path = getenv("SAVAGEWIND_SORA_CAMPSEQ");
    if (path && !strcmp(path, "0")) { LOG("[sora] camp: off (SAVAGEWIND_SORA_CAMPSEQ=0)\n"); return; }
    if (!path || !*path) path = "D:/SoraBotW/data/hud/camp_seq.bin";
    path = sora_path(path);
    FILE *f = fopen(path, "rb");
    if (!f) { LOG("[sora] camp: %s missing, + stays BotW's\n", path); return; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    CAMP.buf = n > 0 ? malloc((size_t)n) : NULL;
    bool ok = CAMP.buf && fread(CAMP.buf, 1, (size_t)n, f) == (size_t)n && cs_load(&CAMP.seq, CAMP.buf, (size_t)n);
    fclose(f);
    if (!ok) { free(CAMP.buf), CAMP.buf = NULL; LOG("[sora] camp: %s bad, + stays BotW's\n", path); return; }
    CAMP.ok = true;
    LOG("[sora] camp: %s loaded (%ld bytes)\n", path, n);
}

// the page's values, snapshotted at open (cp_tick rebuilds the tree on a row change from them)
static void camp_vals(CpVals *v) {
    memset(v, 0, sizeof *v);
    g_cp_solo = rm_actor() > 0;  // (s111 roxmenu: Roxas alone in the member rows / plates)
    if (g_cp_solo) LOG("[roxmenu] camp: Roxas is the body: one member, no Donald / Goofy\n");
    int32_t munny = -1, t = -1;
    if (!SLX.gdt_s32 || !SLX.gdt_s32("CurrentRupee", &munny)) munny = -1;
    if (!SLX.gdt_s32 || !SLX.gdt_s32("PlayReport_PlayTime", &t)) t = -1;  // seconds (checked s79: real saves tick ~65-71 per minute of play)
    if (orb_munny() >= 0) munny = orb_munny();  // (s131 orbs: the KH2 munny counter replaces the rupee read, live "set orbmunny 2" = rupees)
    v->munny = munny, v->time_s = t;
    LOG("[sora] camp: munny %d time %d\n", (int)munny, (int)t);
    v->hp[0] = (int)g_hud_last[0], v->hp_max[0] = (int)g_hud_last[1];
    v->mp[0] = (int)fmaxf(g_hud_last[2], 0), v->mp_max[0] = (int)g_hud_last[3];
    for (int i = 0; i < PARTY_N; i++) {
        g_cp_name[i + 1] = pchar_menu_name(g_pchar, i);  // (s220 riku2: a stand-in's own name in the member rows)
        if (g_cp_name[i + 1]) LOG("[sora] camp: member %d = %s (stands in for %s)\n", i + 1, g_cp_name[i + 1], PCH_TAB[PCH_HOME[i]].name);
        v->hp[i + 1] = (int)P.m[i].hp, v->hp_max[i + 1] = (int)P.m[i].hp_max;
        v->mp[i + 1] = (int)P.m[i].mp, v->mp_max[i + 1] = (int)PARTY[i].mp_max;
    }
    for (int i = 0; i < 3; i++) v->lv[i] = sl_lv();  // (GUESS slice 1: Donald / Goofy share Sora's level)
    int lv = sl_lv();
    const SlLevel *s = &SL_LV[lv - 1];
    int bonus = 0;  // (GUESS: KH2 Bonus LV = Sora's bonus-level count)
    for (int l = 1; l <= lv; l++) bonus += SL_LV[l - 1].ab[sl_dream()] != 0;
    v->st[0] = (CpStat){lv, (int)SL.exp, cp_next_left(lv, SL.exp, sl_next(lv)), v->hp[0], v->hp_max[0], v->mp[0],
                        v->mp_max[0], ab_used(AB.on), 2 + s->ap,  // (s82 slice 3: AP used)
                       
                        dg_max(), bonus, sl_str_of(s) + SL_KEY_STR, sl_mag_of(s) + SL_KEY_MAG, s->def, {0, 0, 0, 0}};
    for (int i = 1; i < 3; i++)  // party table has no Str / Mag / Def / AP yet -> "--"
        v->st[i] = (CpStat){lv, -1, -1, v->hp[i], v->hp_max[i], v->mp[i], v->mp_max[i], -1, -1, -1, -1, -1, -1, -1, {0, 0, 0, 0}};
    v->eq_base_str = sl_str_of(s), v->eq_base_mag = sl_mag_of(s);  // (s120 eqlook) the Equipment panel's [ total ] = base + the main Keyblade
    uint16_t o[AB_MAX_LEARN];
    v->ab_n = ab_order(o), v->ab_ap_max = ab_ap_max(), v->ab_on = AB.on;  // (s83 slice 3 Abilities page)
    for (int i = 0; i < v->ab_n && i < (int)(sizeof v->ab_id / sizeof v->ab_id[0]); i++) v->ab_id[i] = o[i];
    {   // (s91 b5-abil) the page's rows per KH2 category (kind 1/2 Action, 3 Growth, 0 Support)
        int c[3] = {0, 0, 0};
        for (int i = 0; i < v->ab_n; i++) c[cp_ab_cat(v->ab_id[i])]++;
        LOG("[sora] camp: abilities page: %d Action, %d Growth, %d Support (abilfix %d)\n", c[0], c[1], c[2], g_abilfix);
    }
    memcpy(v->sc, SC.sc, sizeof v->sc);  // (s84 slice 4 Customize page)
    for (int sp = 0; sp < CUST_NSPELL; sp++) if (!mg_owned(sp)) v->sp_hide |= 1 << sp;  // (s147) the picker lists owned spells only
    v->kb_forms = ul_form_unlocked("Master") | ul_form_unlocked("Final") << 1;  // (s91 mfkb: which form rows the Equipment page shows)
    if (g_mfkb) kb_forms_sync(v->kb_forms, "menu open");  // (s118: no Keyblade hides in a locked form's slot)
    memcpy(v->kb, KB.kb, sizeof v->kb);  // (s85 slice 5 Equipment page)
    v->db_real_lv = SL.lv;
    v->db_party_have = party_have_mask();  // (s220 members: who the Party 1 / 2 rows may offer)
    for (int r = 0; r < CP_DB_ROWS; r++) v->db_v[r] = dbm_state(r);  // (s146: by backend row)  // (s140 dbgmenu: the Debug page)
    v->kb_own = ul_kb_own_mask();  // (s89 kb: the picker lists the owned ones)
    v->arm_n = armor_worn(v->arm_name, v->arm_def);  // (s120 eqlook: the Armor rows; 0 = old exe / nothing named worn)
    {   // (s128 wpnmenu) Link's BotW weapon for the Weapons slot: only Sora's base form (not a Drive Form, Roxas or Mickey)
        WpnCur wc;
        bool base = !IN_VALOR && !IN_WISDOM && !IN_LIMIT && !IN_MASTER && !IN_FINAL && !IN_ANTI && !IN_MICKEY && (!IN_ROXAS || (g_wpn_rox && wpn_rox_exe()));  // (s137 roxwpn: Roxas too, with the s137 exe)
        v->wpn_on = base && wpn_equipped(&wc), v->wpn_use = g_wpn_use != 0;
        v->wpn_name[0] = 0, v->wpn_atk = v->wpn_life = v->wpn_life_max = 0;
        if (v->wpn_on) {
            snprintf(v->wpn_name, sizeof v->wpn_name, "%s", wc.def->name);
            v->wpn_atk = wc.def->power + wc.atk_up, v->wpn_life = wc.life, v->wpn_life_max = wc.life_max;
        }
    }
    LOG("[sora] camp: armor rows %d%s%s%s%s%s%s\n", v->arm_n, v->arm_n > 0 ? " " : "", v->arm_n > 0 ? v->arm_name[0] : "",
        v->arm_n > 1 ? ", " : "", v->arm_n > 1 ? v->arm_name[1] : "", v->arm_n > 2 ? ", " : "", v->arm_n > 2 ? v->arm_name[2] : "");
    LOG("[sora] camp: status lv %d str %d mag %d def %d ap %d drive %d\n", lv, v->st[0].str, v->st[0].mag, v->st[0].def,
        v->st[0].ap_max, v->st[0].drive);
}

#include "sora_camp_walk.h"  // (s92 b6-menuwalk: the party behind the menu)

// (s120 itempic + eqlook) the Equipment page's Keyblade picture: one tex 3 quad after the page's quads (above the panel
// window; the cursor sits in the list on the left, so it never covers it). Needs the exe's sora_hud_itempic_ready (s120
// exe, itempic.bin loaded); older exe = no picture (logged once). Alpha follows the panel window's open / end anim [G].
static void camp_pic(CsOut *o) {
    static bool (*ready)(void);
    static int bound;
    if (!bound) {
        bound = 1;
        ready = (bool (*)(void))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_itempic_ready");
        LOG("[sora] camp: %s\n", ready ? "exe sora_hud_itempic_ready found (Keyblade pictures)" : "sora_hud_itempic_ready not in this exe: no Keyblade pictures");
    }
    int kb, a;
    float r[4];
    if (!ready || !o || o->n >= o->cap || !cp_eq_pic(&CAMP.page, &kb, r) || (a = cp_eq_pic_alpha(&CAMP.page)) <= 0 || !ready()) return;
    if (cp_pic_quad(&o->q[o->n], kb, r[0], r[1], r[2], r[3], (uint8_t)(a > 255 ? 255 : a))) o->n++;
}

static void camp_send(int n) {
    if (n > 0 && CAMP.quads2) CAMP.quads2(g_camp_q, n, mw_under() ? g_cp_split : 0), CAMP.drew = true;  // (s94)
    else if (n > 0) CAMP.quads(g_camp_q, n), CAMP.drew = true;
    else if (CAMP.drew) CAMP.quads(NULL, 0), CAMP.drew = false;
}

// (s131 user: the save point's Triangle must open BotW's OWN menu, not KH2's) until this time (ms) a + does not open the camp:
// BotW gets it as if the mod were not there (sora_savept.h sv_menu_now sets it)
// the last line of pad_tap: while the camp is up BotW sees nothing but the + it needs (the opening + held, the pulses)
static void camp_pad(uint64_t *b, int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry, uint64_t now) {
    (void)now;
    static uint64_t prev;
    uint64_t held = g_held;
    if (g_camp_on && (held & CP_BTN_PLUS) && !(prev & CP_BTN_PLUS) && !g_camp_eat && GetTickCount64() >= g_camp_skip_until &&
        !*(volatile uint32_t *)(uintptr_t)PAUSE_STATE)
        g_camp_req = 1;
    prev = held;
    if (g_camp_swallow) {  // (s92) the confirm that opened BotW's menu: BotW sees no button until it is let go
        g_camp_swallow &= held;
        *b &= ~g_camp_swallow;
    }
    if (!g_camp_eat) return;
    uint64_t keep = 0;
    if (g_camp_pass_plus) keep |= *b & CP_BTN_PLUS;
    if (g_camp_pulse) keep |= CP_BTN_PLUS;
    *b = keep, *lx = *ly = *rx = *ry = 0;
}

static void camp_flags(const CpOut *o) { g_camp_eat = o->eat, g_camp_pass_plus = o->pass_plus, g_camp_pulse = o->pulse_plus; }

// first thing in the frame (paused or playing): true = the camp owns it (the caller hides Sora + the HUD and returns)
static bool camp_frame(uint64_t present) {
    (void)present;
    bool req = g_camp_req;
    if (!req && CAMP.page.state == CP_CLOSED) return false;
    g_camp_req = 0;
    if (!CAMP.tried) camp_load();
    if (!CAMP.ok) { g_camp_on = 0; return false; }
    if (req && CAMP.page.state == CP_CLOSED) {
        // (the + edge saw PAUSE_STATE 0 in pad_tap: BotW's pause is that same +, so the request is judged as of then)
        bool world_ok = linkpos_world_frame() && linkpos_link_life() > 0 && !GO.on && !g_event;  // (s85f #5: no KH2 menu where BotW pauses not: talks, cutscenes)
        cp_request(&CAMP.page, 0, world_ok);
        if (CAMP.page.state == CP_CLOSED) return false;
        camp_vals(&CAMP.v);
    }
    static uint64_t prevb;
    uint64_t held = g_held, seen = __atomic_exchange_n(&g_seen, 0, __ATOMIC_RELAXED) | held;
    CAMP.page.held_btn = held;  // (s140 dbgmenu: the Debug page's hold-to-repeat)
    uint64_t press = seen & ~prevb;
    prevb = held;
    uint32_t pause = *(volatile uint32_t *)(uintptr_t)PAUSE_STATE;
    int row0 = CAMP.page.row, st0 = CAMP.page.state;
    CsOut o = {.q = g_camp_q, .n = 0, .cap = CAMP_QUADS};
    CpOut out;
    mw_pre();
    cp_tick(&CAMP.page, &CAMP.seq, &o, pause, press, kh_axis(g_ly), &CAMP.v, &out);
    camp_flags(&out);
    if (CAMP.page.state == CP_HANDOVER && st0 != CP_HANDOVER && g_bmswallow) g_camp_swallow = held;
    if (out.ab_set) {  // (s83) the Abilities page equipped / unequipped one: keep it, and the Status AP, now
        AB.on = CAMP.v.ab_on = out.ab_on;
        CAMP.v.st[0].ap_used = ab_used(AB.on);
        LOG("[sora] camp: ability %d %s -> %s (AP %d/%d)\n", out.ab_id, ab_name(out.ab_id), ab_on(AB.on, out.ab_id) ? "on" : "off",
            ab_used(AB.on), CAMP.v.ab_ap_max);
        sl_persist_save();
    }
    if (out.sc_set) {  // (s84) the Customize page set a shortcut: M's menu + the HUD read SC.sc, sc_set saves it
        sc_set(out.sc_slot, out.sc_spell, "camp");
        memcpy(CAMP.v.sc, SC.sc, sizeof CAMP.v.sc);
        LOG("[sora] camp: shortcut %d -> %s\n", out.sc_slot, cust_str(out.sc_spell));
    }
    if (CAMP.db_stale && out.log && !strcmp(out.log, "debug back")) {  // (s140 review) leaving the Debug page: re-read the values like the kb_set block
        int an = CAMP.v.arm_n, ad[3];
        char am[3][24];
        memcpy(ad, CAMP.v.arm_def, sizeof ad), memcpy(am, CAMP.v.arm_name, sizeof am);
        camp_vals(&CAMP.v);
        CAMP.v.arm_n = an, memcpy(CAMP.v.arm_def, ad, sizeof ad), memcpy(CAMP.v.arm_name, am, sizeof am);
        CAMP.db_stale = false;
    }
    if (out.db_set) {  // (s140 dbgmenu) the Debug page changed a row: apply it, show what is really in force
        int row = out.db_set - 1, got = dbm_set(row, out.db_val, "camp Debug Menu");
        CpVals *cv = &CAMP.v;
        if (row >= 0 && row < CP_DB_ROWS) CAMP.page.db_v[row] = cv->db_v[row] = got;
        CAMP.page.db_rebuild = 1;
        CAMP.db_stale = true;  // the other pages' values (abilities, stats, Keyblades) follow the new level / unlocks: refreshed when the page goes back
        if (got != out.db_val) out.se = CP_SE_ERROR;  // refused (debugall is ON)
        LOG("[dbgmenu] camp: row %d -> %d%s\n", row, got, got != out.db_val ? " (refused)" : "");
    }
    if (out.wpn_set) {  // (s128 wpnmenu) the Equipment picker chose the BotW weapon (1) or a Keyblade in the Weapons slot (2)
        wpn_menu_pick(out.wpn_set == 1);
        CAMP.v.wpn_use = g_wpn_use != 0;
        LOG("[sora] camp: Weapons slot -> %s\n", g_wpn_use ? "BotW weapon" : "Keyblade");
        sl_persist_save();
    }
    if (out.kb_set && (out.kb_swap ? kb_swap_v(out.kb_slot, out.kb_id, "camp") : kb_set_v(out.kb_slot, out.kb_id, "camp")) >= 0) {  // (s85; s118 kbswap)
        int an = CAMP.v.arm_n, ad[3];                                // (s120) the Armor rows stay as read at open: the paused world
        char am[3][24];                                              // may stop drawing the armor actors (the exe's 1 s window)
        memcpy(ad, CAMP.v.arm_def, sizeof ad), memcpy(am, CAMP.v.arm_name, sizeof am);
        camp_vals(&CAMP.v);                                          // kb_send follows next frame; Status Str / Mag now
        CAMP.v.arm_n = an, memcpy(CAMP.v.arm_def, ad, sizeof ad), memcpy(CAMP.v.arm_name, am, sizeof am);
    }
    static const char *const SE[] = {NULL, "submenu", "cursor", "decide", "cancel", "error"};
    if (out.se > CP_SE_NONE && out.se <= CP_SE_ERROR) sora_sfx_event(SE[out.se], 1);
    if (CAMP.log) {
        if (out.log) LOG("[sora] camp: %s\n", out.log);
        if (out.log_row >= 0) LOG("[sora] camp: row %d\n", out.log_row);
        if (out.se == CP_SE_DECIDE || out.se == CP_SE_ERROR) LOG("[sora] camp: decide %d\n", row0);
    }
    if (out.draw) camp_pic(&o);  // (s120 itempic)
    camp_send(out.draw ? o.n : 0);
    if (CAMP.page.state == CP_CLOSED && st0 != CP_CLOSED) S.fresh = true;  // (the B / + that closed it is not Sora's)
    return out.draw;
}

static void camp_abort(const char *why) {
    g_camp_req = g_camp_eat = g_camp_pass_plus = g_camp_pulse = 0, g_camp_swallow = 0;
    if (CAMP.page.state != CP_CLOSED) LOG("[sora] camp: abort %s\n", why);
    CAMP.page.state = CP_CLOSED;
    if (CAMP.quads) CAMP.quads(NULL, 0);
    CAMP.drew = false;
    mw_off();
}

// live: camp 0/1 (0 = + is BotW's), campopen 1 (a + request from the live file: BotW does not pause from it, so this
// shows the abort no-pause path - the real test is + on the pad), camplog 0/1
static bool camp_live_set(const char *key, int v) {
    if (!strcmp(key, "camp")) { g_camp_on = v != 0; if (!v) camp_abort("live off"); }
    else if (!strcmp(key, "campopen")) { if (v && !*(volatile uint32_t *)(uintptr_t)PAUSE_STATE) g_camp_req = 1; }
    else if (!strcmp(key, "camplog")) CAMP.log = v != 0;
    else if (mw_live_set(key, v)) {}
    else return false;
    return true;
}

// sora_debugall.h: (s90) the testing toggle, live "set debugall 1" / "set debugall 0".
// ON  = Sora LV 99 (KH2's max: the level table has 99 rows), every Keyblade owned, every Drive Form unlocked (Divine
//       Beasts faked to 4) at form LV 7, Drive gauge max 9 + full, every ability learned + equipped (AP not counted),
//       spell tier 3 (Firaga ...: by LV), full MP.
// OFF = exactly the real values from the moment it went ON: EXP + kills, owned Keyblades, the form EXPs, Anti points,
//       the Drive gauge, the ability masks, "set beasts". The equipped Keyblades stay if they are really owned, else
//       they go back to the ones equipped before. EXP / kills / form EXP earned while ON are thrown away.
// Saves: the debug values never reach a sidecar. A BotW save / load while ON (sl_slot_saved / sl_slot_load) runs with
// the real values put back for the moment (dbg_suspend), then the debug values come back (dbg_resume: a new snapshot,
// so a load of another slot while ON makes THAT slot's values the real ones). A save capture pending while ON gets the
// real EXP at OFF. Hot reload: the snapshot rides in the process env (DBG_ENV), debugall stays ON.
// Reuses: SL.exp (levels), KB.own (s89 kb), UL.beasts_fake ("set beasts"), the form EXP structs, DG, AB, MP.
// Included from sora.c after sora_anti.h (needs every form's EXP struct). g_dbgall + the prototypes: sora_levels.h.
// MODE 2 (s108 b20-lynel, "set debugall 2") = the combat-feel test: every ability learned + equipped (AP not counted),
//       every Keyblade owned (unlocking does NOT equip: the equipped ones stay, so Keyblade strength is the player's own),
//       Drive Forms unlocked (beasts faked 4) and the Drive gauge + MP filled, BUT the REAL level / EXP / kills (so Str / Mag /
//       Def, the HP / MP max and the spell tier follow the real LV), the REAL form EXPs (form levels) and the real Anti points.
//       Rule: "real level and real stats, all abilities". EXP earned in mode 2 is real and is kept at 2 -> 0 / 2 -> 1.
//       Switching 0 / 1 / 2 in any order restores the real values exactly (one DBR snapshot taken at 0 -> 1/2).
#pragma once
#define DBG_ENV "SORA_DBGALL"
_Static_assert(KB_SLOTS == 4, "the env line holds four Keyblade slots (kb0..kb1, then kb2 / kb3 = Master / Final, s91 mfkb)");

static struct {
    uint32_t exp, kills, own, ve, we, le, me, fe;
    abm_t ab_on, ab_seen;  // (s91: 64-bit masks)
    int kb[KB_SLOTS], anti, drive, beasts_fake;
} DBR;  // the REAL values while debugall is ON

static void dbm_pre_debugall(void);  // (s140, sora_dbgmenu.h)
static void dbg_sync_real(void);  // mode 2: the live EXP / form EXP / Anti are the real ones -> DBR
static void dbg_persist(void) {
    if (!g_dbgall) { SetEnvironmentVariableA(DBG_ENV, NULL); return; }
    dbg_sync_real();
    char b[400];
    snprintf(b, sizeof b, "v1 exp=%u kills=%u own=%u ve=%u we=%u le=%u me=%u fe=%u ab=%llu abs=%llu kb0=%d kb1=%d anti=%d drive=%d bf=%d kb2=%d kb3=%d m=%d",
             DBR.exp, DBR.kills, DBR.own, DBR.ve, DBR.we, DBR.le, DBR.me, DBR.fe, (unsigned long long)DBR.ab_on, (unsigned long long)DBR.ab_seen, DBR.kb[0], DBR.kb[1],
             DBR.anti, DBR.drive, DBR.beasts_fake, DBR.kb[2], DBR.kb[3], g_dbgall);
    SetEnvironmentVariableA(DBG_ENV, b);
}
// sl_init (first thing): a hot reload while ON = the snapshot from the env, still ON
static void dbg_init(void) {
    static bool done;
    if (done) return;
    done = true;
    char b[400];
    DWORD n = GetEnvironmentVariableA(DBG_ENV, b, sizeof b);
    if (!n || n >= sizeof b) return;
    if (sscanf(b, "v1 exp=%u kills=%u own=%u ve=%u we=%u le=%u me=%u fe=%u ab=%llu abs=%llu kb0=%d kb1=%d anti=%d drive=%d bf=%d",
               &DBR.exp, &DBR.kills, &DBR.own, &DBR.ve, &DBR.we, &DBR.le, &DBR.me, &DBR.fe, &DBR.ab_on, &DBR.ab_seen, &DBR.kb[0],
               &DBR.kb[1], &DBR.anti, &DBR.drive, &DBR.beasts_fake) != 15) {
        LOG("[debugall] hot reload: env line not understood, left OFF: %s\n", b);
        SetEnvironmentVariableA(DBG_ENV, NULL);
        return;
    }
    const char *k2 = strstr(b, " kb2=");  // (s91 mfkb: absent in an env line from the previous DLL = auto)
    if (!k2 || sscanf(k2, " kb2=%d kb3=%d", &DBR.kb[2], &DBR.kb[3]) != 2) DBR.kb[2] = DBR.kb[3] = 0;
    const char *m2 = strstr(b, " m=");  // (b20-lynel: absent = mode 1)
    g_dbgall = m2 && atoi(m2 + 3) == 2 ? 2 : 1;
    LOG("[debugall] hot reload: still ON (mode %d) (real LV %d EXP %u, %u kills, owned 0x%06x)\n", g_dbgall, sl_level_of(DBR.exp), DBR.exp, DBR.kills, DBR.own);
}

static void dbg_snap(void) {  // live values -> DBR (they are the real ones: debugall is OFF / suspended)
    (void)vl_exp_get(), (void)wl_exp_get(), (void)ll_exp_get(), (void)ml_exp_get(), (void)fl_exp_get(), (void)anti_pts_get();
    dg_init();
    DBR.exp = SL.exp, DBR.kills = SL.kills, DBR.own = KB.own;
    memcpy(DBR.kb, KB.kb, sizeof DBR.kb);
    DBR.ve = VL.exp, DBR.we = WL.exp, DBR.le = LL.exp, DBR.me = ML.exp, DBR.fe = FL.exp, DBR.anti = AN.pts;
    DBR.drive = dg_points();
    DBR.ab_on = SL.devlv ? AB.dev_on : AB.on, DBR.ab_seen = SL.devlv ? AB.dev_seen : AB.seen;
    DBR.beasts_fake = UL.beasts_fake;
}
static void dbg_forms_persist(void) { vl_persist(), wl_persist(), ll_persist(), ml_persist(), fl_persist(), anti_persist(); }
static void dbg_sync_real(void) {  // mode 2 only: what was earned meanwhile is real (mode 1 throws it away)
    if (g_dbgall != 2) return;
    uint32_t o[8] = {DBR.exp, DBR.kills, DBR.ve, DBR.we, DBR.le, DBR.me, DBR.fe, (uint32_t)DBR.anti};
    DBR.exp = SL.exp, DBR.kills = SL.kills, DBR.ve = VL.exp, DBR.we = WL.exp, DBR.le = LL.exp, DBR.me = ML.exp, DBR.fe = FL.exp;
    DBR.anti = AN.pts;
    uint32_t n[8] = {DBR.exp, DBR.kills, DBR.ve, DBR.we, DBR.le, DBR.me, DBR.fe, (uint32_t)DBR.anti};
    if (memcmp(o, n, sizeof o)) dbg_persist();  // (the env line is re-written only on a change)
}
// per frame (ab_tick): keeps the mode 2 snapshot's real EXP current so a hot reload loses nothing
static void dbg_tick(void) { dbg_sync_real(); }
static void dbg_apply(void) {  // the debug values (debugall ON); g_dbgall = the mode (1 or 2), already set
    if (g_dbgall == 1) SL.exp = sl_exp_max(), SL.lv = sl_level_of(SL.exp);  // (mode 2: the real LV stays)
    KB.own = ul_all();  // (unlocking never equips: KB.kb is left alone)
    if (g_dbgall == 1) {
    VL.exp = VL_EXP_AT[VL_MAX_LV - 1], WL.exp = WL_EXP_AT[WL_MAX_LV - 1], LL.exp = LL_EXP_AT[LL_MAX_LV - 1];
    ML.exp = ML_EXP_AT[ML_MAX_LV - 1], FL.exp = FL_EXP_AT[FL_MAX_LV - 1];
    }  // (mode 2: the real form EXP / levels stay)
    VL.queue = WL.queue = LL.queue = ML.queue = FL.queue = 0;  // (no level-up bursts for the jump)
    dbg_forms_persist();
    UL.beasts_fake = 4;
    AB.on = AB.seen = 0, AB.dirty = true;  // (ab_tick: every learned ability auto-equips; ab_ap_max is 999 while ON)
    dg_load_points(DG_MAX_CAP * 100 + 100, "debugall on: full");  // (the max is 9 now: the form LVs)
    MP.mp = MP.mp_max, MP.charge = MP.charge_total = 0;
}
// DBR -> live. exact_kb: both slots = the ones before ON (a suspend); else a Keyblade really owned stays equipped.
static void dbg_restore(bool exact_kb) {
    SL.exp = DBR.exp, SL.kills = DBR.kills, SL.lv = sl_level_of(SL.exp);
    KB.own = DBR.own;
    uint32_t real = kb_own_fix(DBR.own, DBR.kb);
    for (int i = 0; i < KB_SLOTS; i++)
        if (exact_kb || !(real & kb_own_bit(KB.kb[i]))) KB.kb[i] = DBR.kb[i];
    {   // (s91 review) one copy per Keyblade: a slot put back can now hold the blade another slot kept (picked while ON) ->
        // Valor gives way as in a hand-edited save (kb_unpack), Master / Final go auto (kb_unpack2)
        uint32_t p2 = kb_pack2(KB.kb);
        int bad = kb_unpack(kb_pack(KB.kb), KB.kb);
        bad += kb_unpack2(p2, KB.kb);
        if (bad) LOG("[debugall] restore: %d Keyblade slot(s) reset (one copy per Keyblade)\n", bad);
    }
    VL.exp = DBR.ve, WL.exp = DBR.we, LL.exp = DBR.le, ML.exp = DBR.me, FL.exp = DBR.fe, AN.pts = DBR.anti;
    VL.queue = WL.queue = LL.queue = ML.queue = FL.queue = 0;
    dbg_forms_persist();
    UL.beasts_fake = DBR.beasts_fake;
    AB.on = DBR.ab_on, AB.seen = DBR.ab_seen, AB.dirty = true;
    if (SL.devlv) AB.dev_on = AB.on, AB.dev_seen = AB.seen;
    dg_load_points(DBR.drive, exact_kb ? "debugall: the real gauge for a save / load" : "debugall off: the real gauge");
}

static void dbg_set(int mode, const char *why) {
    sl_init();
    mode = mode <= 0 ? 0 : mode >= 2 ? 2 : 1;
    if (mode == g_dbgall) {
        LOG("[debugall] already %s %d (%s)\n", g_dbgall ? "ON, mode" : "OFF, mode", g_dbgall, why);
        return;
    }
    if (mode && !g_dbgall) dbm_pre_debugall();  // (s140 dbgmenu: its level / Keyblade / Drive Form toggles go back before the snapshot)
    if (mode && g_dbgall) {  // 1 <-> 2: back to the real values (the one DBR snapshot), then the new mode on top
        dbg_sync_real();
        int hold[KB_SLOTS];
        memcpy(hold, KB.kb, sizeof hold);  // (what was picked while ON stays equipped)
        g_dbgall = 0;
        dbg_restore(false);
        g_dbgall = mode;
        dbg_apply();
        memcpy(KB.kb, hold, sizeof KB.kb);
        dbg_persist();
        sl_persist_save();
        LOG("[debugall] mode %d (%s): LV %d EXP %u (%s), all abilities, Drive max %d full\n", mode, why, SL.lv, SL.exp,
            mode == 2 ? "real LV + stats" : "LV 99", dg_max());
        return;
    }
    if (mode) {
        if (SL.devlv) {  // (devlv's own stash would fight this one: back to the real masks, devlv off)
            AB.on = AB.dev_on, AB.seen = AB.dev_seen, SL.devlv = 0;
            LOG("[debugall] devlv was on: set to 0\n");
        }
        dbg_snap();
        g_dbgall = mode;
        dbg_apply();
        LOG("[debugall] ON mode %d (%s): LV %d, all %d Keyblades, Drive Forms LV7 (beasts 4), Drive max %d full, all abilities, "
            "MP full%s. Real values kept: LV %d EXP %u, %u kills, owned 0x%06x, Valor %u Wisdom %u Limit %u Master %u Final %u "
            "EXP, Anti %d, drive %d, beasts %d\n", mode, why, SL.lv, KB_N, dg_max(), mode == 2 ? " (mode 2: the real LV / EXP / form levels stay, only abilities + unlocks + gauge)" : "", sl_level_of(DBR.exp), DBR.exp, DBR.kills,
            DBR.own, DBR.ve, DBR.we, DBR.le, DBR.me, DBR.fe, DBR.anti, DBR.drive, DBR.beasts_fake);
    } else {
        dbg_sync_real();  // (mode 2: EXP earned meanwhile is real)
        g_dbgall = 0;
        dbg_restore(false);
        for (int i = 0; i < SL_SLOTS; i++)  // (a save capture taken while ON holds the debug EXP)
            if (SLS.f[i].pend) SLS.f[i].exp = SL.exp, SLS.f[i].kills = SL.kills;
        kb_log("debugall off");
        LOG("[debugall] OFF (%s): back to LV %d EXP %u, %u kills, owned 0x%06x, Drive max %d\n", why, SL.lv, SL.exp, SL.kills,
            KB.own, dg_max());
    }
    dbg_persist();
    sl_persist_save();
}

// around BotW's save / load: the real values for that moment, then the debug ones again (the Keyblades picked while ON
// are kept)
static int dbg_kb_hold[KB_SLOTS];
static int dbg_suspend(const char *why) {
    if (!g_dbgall) return 0;
    dbg_sync_real();
    int mode = g_dbgall;
    memcpy(dbg_kb_hold, KB.kb, sizeof dbg_kb_hold);
    g_dbgall = 0;
    dbg_restore(true);
    LOG("[debugall] %s: real values for it (LV %d EXP %u)\n", why, SL.lv, SL.exp);
    return mode;
}
static void dbg_resume(int was, const char *why) {
    if (!was) return;
    dbg_snap();  // (a load: that slot's values are the real ones now)
    g_dbgall = was;
    dbg_apply();
    memcpy(KB.kb, dbg_kb_hold, sizeof KB.kb);
    dbg_persist();
    sl_persist_save();
    LOG("[debugall] %s done: debug values back (real LV %d EXP %u)\n", why, sl_level_of(DBR.exp), DBR.exp);
}

static bool dbg_set_key(const char *key, int v) {
    if (strcmp(key, "debugall")) return false;
    dbg_set(v, "live set debugall");
    return true;
}

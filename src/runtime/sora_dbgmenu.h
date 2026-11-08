// sora_dbgmenu.h: (s140 b140-dbgmenu) the KH2 pause menu's "Debug Menu" page backend + its live keys. The page itself is
// sora_camp_page.h (CP_ROW_DEBUG, page 5); this file turns its four rows into game state, each one independent:
//   row 0 Sora Level  = SL.devlv (the s67 "set devlv N": rules / Str / Mag / Drive max / abilities act as LV N; the real
//                       SL.lv / EXP are untouched and the sidecar never gets the test level). 0 = Off = the real level.
//   row 1 Keyblades   = UL.kball (every Keyblade usable, nothing written to KB.own). Off puts the equipped slots back
//                       (a blade picked while ON that is not really owned goes back to the one before; same rule as
//                       debugall's restore), so the save never learns an unowned Keyblade.
//   row 2 Drive Forms = UL.beasts_fake 4 (the Divine Beast gate of every form, "set beasts"). Off = the value before.
//   row 3 Summons     = SMN.own / smn_own_x[2] / smn_own_x[3] forced 1 (Chicken Little, Genie, Peter Pan). Stitch stays
//                       as it is (parked: SMN.own_st). Off = the values before.
// Not combined with debugall 1 / 2 (they snapshot the same fields): rows 0-2 refuse while it is ON, and "set debugall" first
// puts these three back (dbm_pre_debugall). A BotW save / load puts the Keyblade slots back for the moment
// (dbm_suspend / dbm_resume, called from sl_slot_saved / sl_slot_load next to debugall's). Hot reload: the ON flags + the
// snapshot ride in the process env (DBM_ENV); the test level (a static of the old DLL) resets to Off.
//   (s146 outfit) row 7 Become Sora = g_dbm_sora (sora_roxas.h: Sora is the body whatever the story says; the sidecar's roxas field is
//                       skipped while on; Off = the char setting again, 0 = auto: Roxas until the paraglider),
//                 row 8 Outfit = g_outfit 0 Normal / 1 Halloween Town / 2 Christmas Town / 3 Space Paranoids / 4 Timeless River,
//                 row 9 Auto Outfits = g_outfit_auto (night -> Halloween Town, cold -> Christmas Town; sora_outfit.h). None is saved.
//   The page shows them on top in the order Become Sora, Outfit, Auto Outfits (CP_DB_ORD in sora_camp_page.h).
// Live keys: dbmlv N (0 = off, 1..99), dbmkb 0/1, dbmforms 0/1, dbmsmn 0/1, dbmab, dbmgod, dbmclip, dbmsora 0/1, dbmoutfit 0..4,
// dbmautoout 0/1 (+ dbmcold / dbmofres, sora_outfit.h), dbmoff 1 (all off), dbmlog 1 (state line).
// Included from sora.c after sora_summon_menu.h. All LOG lines carry "[dbgmenu]".
#pragma once
#define DBM_ENV "SORA_DBGMENU"

static struct {
    int kb, forms, smn;            // 1 = ON
    int kbs[KB_SLOTS];             // the equipped Keyblades when kb went ON (the real ones)
    int kb_hold[KB_SLOTS];         // a save: the slots picked while ON, put back after it
    int beasts_prev;               // UL.beasts_fake before forms went ON
    int own_prev, gn_prev, pp_prev;  // SMN.own / smn_own_x[2] / smn_own_x[3] before summons went ON
    int ab, ab_susp;               // (s141) All Abilities ON; ab_susp 1 = suspended around a save / load / level change (AB holds the layer under it)
    int inited;
} DM;
// (s142 dbgexp) the REAL EXP while the toggles are ON (the EXP earned meanwhile is thrown away, debugall mode 1's rule):
//  Unlock Drive Forms -> the five form EXPs + Anti points (f_on); All Abilities -> Sora's EXP + kills (e_on).
static struct { int f_on, e_on; uint32_t ve, we, le, me, fe, exp, kills; int anti; } DX;
// (s142 review) a BotW save puts the real values back for the write, then the LIVE debug values come back (only a load / New Game drop them)
static struct { int f, e; uint32_t ve, we, le, me, fe, exp, kills; int anti; abm_t aon, aseen; } DH;
static abm_t dbm_autos_(void) {
    return ab_bit(AB_AUTO_VALOR) | ab_bit(AB_AUTO_WISDOM) | ab_bit(AB_AUTO_LIMIT) | ab_bit(AB_AUTO_MASTER) | ab_bit(AB_AUTO_FINAL);
}

static void dbm_persist(void) {
    if (!DM.kb && !DM.forms && !DM.smn && !DM.ab && !g_dbm_god && !g_dbm_clip && !g_dbm_sora && !g_outfit && !g_outfit_auto) { SetEnvironmentVariableA(DBM_ENV, NULL); return; }
    char b[640];
    int n = snprintf(b, sizeof b, "v1 kb=%d fm=%d sm=%d k0=%d k1=%d k2=%d k3=%d bp=%d op=%d gp=%d pp=%d ab=%d", DM.kb, DM.forms, DM.smn, DM.kbs[0],
             DM.kbs[1], DM.kbs[2], DM.kbs[3], DM.beasts_prev, DM.own_prev, DM.gn_prev, DM.pp_prev, DM.ab);
    n += snprintf(b + n, sizeof b - (size_t)n, " gd=%d nc=%d", g_dbm_god, g_dbm_clip);
    n += snprintf(b + n, sizeof b - (size_t)n, " so=%d of=%d au=%d", g_dbm_sora, g_outfit, g_outfit_auto);  // (s146 outfit)
    if (DX.f_on) n += snprintf(b + n, sizeof b - (size_t)n, " xf=1 ve=%u we=%u le=%u me=%u fe=%u an=%d", DX.ve, DX.we, DX.le, DX.me, DX.fe, DX.anti);
    if (DX.e_on) n += snprintf(b + n, sizeof b - (size_t)n, " xe=1 ex=%u ki=%u", DX.exp, DX.kills);
    SetEnvironmentVariableA(DBM_ENV, b);
}
static void dbm_smn_dirty_(void) { SMN.owned_last = -1, SMN.owned_mask_last = -1; }

// a hot reload while any toggle is ON: the flags come back from the env (UL / SMN statics are fresh in the new DLL)
static void dbm_init(void) {
    if (DM.inited) return;
    DM.inited = 1;
    char b[640];
    DWORD n = GetEnvironmentVariableA(DBM_ENV, b, sizeof b);
    if (!n || n >= sizeof b) return;
    int kb, fm, sm, k[4], bp, op, gp, pp;
    if (sscanf(b, "v1 kb=%d fm=%d sm=%d k0=%d k1=%d k2=%d k3=%d bp=%d op=%d gp=%d pp=%d", &kb, &fm, &sm, &k[0], &k[1], &k[2], &k[3], &bp,
               &op, &gp, &pp) != 11) {
        LOG("[dbgmenu] hot reload: env line not understood, all OFF: %s\n", b);
        SetEnvironmentVariableA(DBM_ENV, NULL);
        return;
    }
    DM.kb = kb != 0, DM.forms = fm != 0, DM.smn = sm != 0;
    for (int i = 0; i < KB_SLOTS; i++) DM.kbs[i] = k[i];
    DM.beasts_prev = bp, DM.own_prev = op, DM.gn_prev = gp, DM.pp_prev = pp;
    { const char *a = strstr(b, " ab="); DM.ab = a && atoi(a + 4) != 0; }  // (s141; applied by dbm_ab_tick once the real masks are loaded)
    {   // (s142) the EXP snapshots
        const char *x = strstr(b, " xf=1 ");
        if (x && sscanf(x, " xf=1 ve=%u we=%u le=%u me=%u fe=%u an=%d", &DX.ve, &DX.we, &DX.le, &DX.me, &DX.fe, &DX.anti) == 6) DX.f_on = 1;
        x = strstr(b, " xe=1 ");
        if (x && sscanf(x, " xe=1 ex=%u ki=%u", &DX.exp, &DX.kills) == 2) DX.e_on = 1;
        if (DX.f_on || DX.e_on) LOG("[dbgmenu] hot reload: EXP snapshots kept (forms %d, sora %d)\n", DX.f_on, DX.e_on);
    }
    { const char *g = strstr(b, " gd="); int a = 0, c = 0; if (g && sscanf(g, " gd=%d nc=%d", &a, &c) == 2) g_dbm_god = a != 0, g_dbm_clip = c != 0; }  // (s143)
    { const char *g = strstr(b, " so="); int a = 0, c = 0, d = 0; if (g && sscanf(g, " so=%d of=%d au=%d", &a, &c, &d) == 3) g_dbm_sora = a != 0, g_outfit = c < 0 || c >= OF_N ? 0 : c, g_outfit_auto = d != 0; }  // (s146)
    if (DM.kb) UL.kball = 1;
    if (DM.forms) UL.beasts_fake = 4;
    if (DM.smn) SMN.own = 1, smn_own_x[2] = 1, smn_own_x[3] = 1, dbm_smn_dirty_();
    LOG("[dbgmenu] hot reload: still ON (keyblades %d, drive forms %d, summons %d, all abilities %d, god %d, no clip %d, become sora %d, outfit %d, auto outfits %d)\n", DM.kb, DM.forms, DM.smn, DM.ab, g_dbm_god, g_dbm_clip, g_dbm_sora, g_outfit, g_outfit_auto);
}

// the Keyblade slots after kball: a slot whose blade is not really owned goes back to the snapshot's (debugall's rule)
static void dbm_kb_fix_(const char *why) {
    uint32_t real = kb_own_fix(KB.own, DM.kbs);
    int n = 0;
    for (int i = 0; i < KB_SLOTS; i++)
        if (!(real & kb_own_bit(KB.kb[i]))) KB.kb[i] = DM.kbs[i], n++;
    uint32_t p2 = kb_pack2(KB.kb);
    int bad = kb_unpack(kb_pack(KB.kb), KB.kb);
    bad += kb_unpack2(p2, KB.kb);  // (one copy per Keyblade, as dbg_restore)
    LOG("[dbgmenu] keyblades back to what is really owned (%s): %d slot(s) put back, %d reset by the one-copy rule\n", why, n, bad);
}

static int dbm_state(int row) { return row == 0 ? SL.devlv : row == 1 ? DM.kb : row == 2 ? DM.forms : row == 3 ? DM.smn : row == 4 ? DM.ab : row == 5 ? g_dbm_god : row == 6 ? g_dbm_clip : row == 7 ? g_dbm_sora : row == 8 ? g_outfit : row == 9 ? g_outfit_auto : 0; }  // (the camp page at open)

// ---- (s142 dbgexp) the real EXP snapshots
static void dbm_forms_snap_(const char *why) {
    (void)vl_exp_get(), (void)wl_exp_get(), (void)ll_exp_get(), (void)ml_exp_get(), (void)fl_exp_get(), (void)anti_pts_get();
    DX.ve = VL.exp, DX.we = WL.exp, DX.le = LL.exp, DX.me = ML.exp, DX.fe = FL.exp, DX.anti = AN.pts, DX.f_on = 1;
    LOG("[dbgmenu] form EXP snapshot (%s): Valor %u Wisdom %u Limit %u Master %u Final %u, Anti %d\n", why, DX.ve, DX.we, DX.le, DX.me, DX.fe, DX.anti);
}
// values back (the flag stays as it is: dbm_suspend uses this too). Growth abilities the form levels granted meanwhile: the Auto
// ones that the restored levels do not learn drop their "seen" bit (ab_sync drops the on bit), so they auto-equip again when really earned.
static void dbm_forms_put_(void) {
    VL.exp = DX.ve, WL.exp = DX.we, LL.exp = DX.le, ML.exp = DX.me, FL.exp = DX.fe, AN.pts = DX.anti;
    VL.queue = WL.queue = LL.queue = ML.queue = FL.queue = 0;
    dbg_forms_persist();
    abm_t un = 0;
    if (vl_lv() < 2) un |= ab_bit(AB_AUTO_VALOR);
    if (wl_lv() < 2) un |= ab_bit(AB_AUTO_WISDOM);
    if (ll_lv() < 2) un |= ab_bit(AB_AUTO_LIMIT);
    if (ml_lv() < 2) un |= ab_bit(AB_AUTO_MASTER);
    if (fl_lv() < 2) un |= ab_bit(AB_AUTO_FINAL);
    AB.seen &= ~un, AB.on &= ~un, AB.dirty = true;
    AB.dev_on &= ~un, AB.dev_seen &= ~un, AB.a_on &= ~un, AB.a_seen &= ~un;  // (s142 review) the stashed real layers too (the sidecar / env write those)
}
static void dbm_forms_restore_(const char *why) {
    if (!DX.f_on) return;
    dbm_forms_put_();
    DX.f_on = 0;
    LOG("[dbgmenu] form EXP restored (%s): Valor %u Wisdom %u Limit %u Master %u Final %u, Anti %d (EXP earned while ON thrown away)\n", why, DX.ve, DX.we, DX.le, DX.me, DX.fe, DX.anti);
}
static void dbm_exp_snap_(const char *why) {
    DX.exp = SL.exp, DX.kills = SL.kills, DX.e_on = 1;
    LOG("[dbgmenu] Sora EXP snapshot (%s): EXP %u (LV %d), %u kills\n", why, DX.exp, sl_level_of(DX.exp), DX.kills);
}
static void dbm_exp_put_(void) {
    SL.exp = DX.exp, SL.kills = DX.kills, SL.lv = sl_level_of(SL.exp);
    for (int i = 0; i < SL_SLOTS; i++)  // (a save capture taken while ON holds the debug EXP: debugall's rule)
        if (SLS.f[i].pend) SLS.f[i].exp = SL.exp, SLS.f[i].kills = SL.kills;
    AB.dirty = true;
}
static void dbm_exp_restore_(const char *why) {
    if (!DX.e_on) return;
    dbm_exp_put_();
    DX.e_on = 0;
    LOG("[dbgmenu] Sora EXP restored (%s): EXP %u (LV %d), %u kills (EXP earned while ON thrown away)\n", why, DX.exp, SL.lv, DX.kills);
}

// ---- abilities (s141 dbglv). AB.on / seen = the masks in force. Layers: real (AB.dev_* while devlv, else AB.on) <- test level
// (devlv) <- All Abilities (AB.all; AB.a_* = the masks under it). Saves / env keep ab_real_on() / ab_real_seen().
// the abilities that come from the LEVEL table / story rows (any level; not the chest ones, form autos): the test level re-derives these
static abm_t dbm_lvmask_(void) {
    abm_t m = 0;
    for (int l = 0; l < 99; l++) m |= ab_bit(SL_LV[l].ab[sl_dream()]);
    if (g_abilfix)
        for (int i = 0; i < AB_STORY_N; i++) m |= ab_bit(AB_STORY[i].id);
    return m & ~CA.mask;
}
// test level just set: the level / story abilities start unseen, so ab_sync learns the ones this LV has (SL_LV[].ab[Dream Weapon]
// + the story rows: the same ab_learned list a real level-up uses) and equips them in learn order while the AP fits = a real
// level-up's auto-equip. Chest abilities, form autos and everything else keep their real on / seen bits.
static void dbm_lv_layer_(const char *why) {
    abm_t m = dbm_lvmask_();
    AB.on &= ~m, AB.seen &= ~m, AB.dirty = true;
    ab_tick();
    ab_log("test level");
    LOG("[dbgmenu] level abilities for test LV %d (Dream Weapon %s): re-derived from the level table (%s)\n", SL.devlv, SL_DREAM_NAME[sl_dream()], why);
}
static int dbm_ab_pause_(void) {  // the layer under All Abilities back into AB.on / seen; 1 = it was applied
    if (!AB.all) return 0;
    AB.on = AB.a_on, AB.seen = AB.a_seen, AB.all = false, AB.dirty = true;
    return 1;
}
static void dbm_ab_apply_(void) {  // AB.on / seen (the layer under) are kept in a_*, All Abilities goes on (ab_tick learns + equips everything)
    AB.a_on = AB.on, AB.a_seen = AB.seen, AB.on = AB.seen = 0, AB.all = true, AB.dirty = true;
}
static void dbm_god_tick_(void);
static void dbm_ab_tick(void) {
    dbm_god_tick_();  // (s143 dbggod) the per-frame hook of this module (ab_tick -> here)  // after a hot reload (the env says ON, the real masks were loaded since) / a save or load
    if (DM.ab && !AB.all && !DM.ab_susp && !g_dbgall) dbm_ab_apply_();
}

// ---- God Mode / No Clip (s143 dbggod)
// God: the exe's hit drop (linkpos_guard_set 1, the Guard / Mickey drop: every addDamage on Link dropped, falls too through its own branch)
// is held on via mk_drops_hits() (sora_guard.h / sora_plimit.h send it each frame), and Link's life is refilled to g_hud_life_max each
// frame as the backup for anything that does not come through addDamage (GUESS: burning / cold / drowning ticks). HP only, not MP.
static void dbm_god_tick_(void) {
    if (!g_dbm_god) return;
    guard_bind();  // every world frame, driven or not (a climb / swim / event frame must not let the exe's hit drop go stale); mk_tick runs later and wins for Mickey
    if (G.exe_ok && p_guard_set) p_guard_set(1);
    static int hv_cached;  // a FRESH BotW MaxHartValue (g_hud_life_max only grows: it holds the largest life ever seen / yellow hearts)
    static unsigned n;
    if (n++ % 30 == 0) {
        int32_t hv = -1;
        hv_cached = SLX.gdt_s32 && SLX.gdt_s32("MaxHartValue", &hv) && hv > 0 && hv < 200 ? (int)hv : 0;
    }
    int target = hv_cached > 0 ? hv_cached : g_hud_life_max;
    if (target > g_hud_life_max) target = g_hud_life_max;  // (link_heal caps there)
    int life = linkpos_link_life();
    if (life > 0 && life < target) link_heal(target - life);
}
// No Clip: Sora's own pad (stick, jump, roll) is taken in pad_tap (g_nc_*), the body is moved by position (linkpos_move, the goto hold's
// way) with zero velocity, so no gravity / servo. GUESS: BotW's collision may still push the body out of geometry each frame (the target
// is re-applied, so it should only jitter); not verified without the game. Speed: dbmclipspd u/s, 0 = KH2's run speed.
static float g_dbm_clip_spd;
static struct { bool on; float pos[3]; ULONGLONG t; uint64_t mv_wf; } NC;
static void dbm_clip_other(void) {  // not driven (event / climb / swim / load / death): BotW owns the body and the pad
    if (NC.on) NC.on = false, LOG("[dbgmenu] no clip: released (Link has the pad / not driven); comes back with the next driven frame\n");
}
static void dbm_clip_tick(const float p[3]) {
    if (!g_dbm_clip || GT.on || linkpos_link_life() <= 0 || MK.choosing) {
        if (NC.on) NC.on = false, LOG("[dbgmenu] no clip: released at %.2f %.2f %.2f (normal movement; if this is inside geometry, turn it back on and move out)\n", p[0], p[1], p[2]);
        return;
    }
    ULONGLONG now = GetTickCount64();
    uint64_t wf = vk_frame_counter();  // (s144 fix: linkpos_world_frame() is the CONSTANT first-in-world frame, so the move never ran)
    g_nc_frame = vk_frame_counter();
    if (!NC.on) {
        NC.on = true, NC.t = now, NC.mv_wf = wf;
        memcpy(NC.pos, p, sizeof NC.pos);
        LOG("[dbgmenu] no clip: ON at %.2f %.2f %.2f (stick = camera-relative, B up, Y down, speed %.1f u/s)\n", p[0], p[1], p[2],
            g_dbm_clip_spd > 0 ? g_dbm_clip_spd : KH_RUN * KH_FPS * 0.05f * g_k);
    }
    float dt = (float)(now - NC.t) / 1000.0f;
    NC.t = now;
    if (dt > 0.1f) dt = 0.1f;
    float spd = g_dbm_clip_spd > 0 ? g_dbm_clip_spd : KH_RUN * KH_FPS * 0.05f * g_k;
    float sx = kh_axis(g_nc_lx), sy = kh_axis(g_nc_ly), cf[2] = {0, 1};
    linkpos_camera_fwd(cf);
    float dir[2] = {cf[0] * sy - cf[1] * sx, cf[1] * sy + cf[0] * sx}, dl = sqrtf(dir[0] * dir[0] + dir[1] * dir[1]);
    if (dl > 1.0f) dir[0] /= dl, dir[1] /= dl;
    float vy = ((g_nc_btn & BTN_JUMP) ? 1.0f : 0.0f) - ((g_nc_btn & BTN_ROLL) ? 1.0f : 0.0f);
    NC.pos[0] += dir[0] * spd * dt, NC.pos[2] += dir[1] * spd * dt, NC.pos[1] += vy * spd * 0.7f * dt;
    float e[3] = {NC.pos[0] - p[0], NC.pos[1] - p[1], NC.pos[2] - p[2]};
    float err = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
    if (err > fmaxf(8.0f, 3.0f * spd * 0.1f)) {  // (scales with the speed: the body is corrected every 2nd world frame) pushed / teleported away (a load, a cutscene): follow the body, do not fly back
        memcpy(NC.pos, p, sizeof NC.pos);
        e[0] = e[1] = e[2] = 0;
    }
    if (wf >= NC.mv_wf + 2 && err > 0.01f) linkpos_move(e[0], e[1], e[2]), NC.mv_wf = wf;
    linkpos_set_velocity(true, 0, 0), linkpos_set_velocity_y(true, 0);
    S.nsafe = 0, S.air_frames = 0;
    S.body_xz[0] = p[0] * 100 / g_k, S.body_xz[1] = p[2] * 100 / g_k;
}

// row: 0 level (val 0..99), 1 Keyblades, 2 Drive Forms, 3 Summons, 4 All Abilities, 5 God, 6 No Clip, 7 Become Sora (val 0/1), 8 Outfit (0..4), 9 Auto Outfits (0/1). Returns the value now in force.
static int dbm_set(int row, int val, const char *why) {
    sl_init();
    dbm_init();
    if (row == 0) {
        if (g_dbgall) { LOG("[dbgmenu] level: refused, debugall is ON (%s)\n", why); return SL.devlv; }
        val = val < 0 ? 0 : val > 99 ? 99 : val;
        if (val == SL.devlv) return val;
        int had = DM.ab_susp;
        DM.ab_susp = 1;
        int under = dbm_ab_pause_();  // All Abilities ON: the layer under it is what devlv stashes / restores
        sl_live_set("devlv", val);  // the s67 devlv: stash of the real ability masks, rules use LV val, never saved
        if (val) dbm_lv_layer_(why);  // (s141) the abilities KH2's table + story give by that level, equipped like a level-up
        DM.ab_susp = had;
        if (under) dbm_ab_apply_(), ab_tick();
        LOG("[dbgmenu] level: %s -> test LV %d (real LV %d EXP %u untouched) (%s)\n", val ? "ON" : "OFF", val, SL.lv, SL.exp, why);
        return SL.devlv;
    }
    if (row == 7) {  // (s146 outfit) Become Sora: rox_tick applies it at once (forced); Mickey's rescue logic is untouched (it needs Sora anyway)
        val = val != 0;
        if (val == g_dbm_sora) return val;
        g_dbm_sora = val;
        LOG("[dbgmenu] become sora: %s (%s) (char setting %d)\n", val ? "ON, Sora is the body now" : "OFF, the story decides again", why, g_char);
        dbm_persist();
        return val;
    }
    if (row == 8) {  // (s146 outfit)
        val = val < 0 || val >= OF_N ? 0 : val;
        if (val == g_outfit) return val;
        ofx_set(val, why);
        dbm_persist();
        return val;
    }
    if (row == 9) {  // (s146 outfit)
        val = val != 0;
        if (val == g_outfit_auto) return val;
        g_outfit_auto = val;
        LOG("[dbgmenu] auto outfits: %s (%s)\n", val ? "ON, night -> Halloween Town, cold -> Christmas Town (the Outfit row wins when not Normal)" : "OFF", why);
        dbm_persist();
        return val;
    }
    val = val != 0;
    if (row == 1) {
        if (g_dbgall) { LOG("[dbgmenu] keyblades: refused, debugall is ON (%s)\n", why); return DM.kb; }
        if (val == DM.kb) return val;
        if (val) {
            memcpy(DM.kbs, KB.kb, sizeof DM.kbs);
            DM.kb = 1, UL.kball = 1;
            LOG("[dbgmenu] keyblades: ON, all %d usable (nothing written to the owned set; equipped now %x/%x/%x/%x) (%s)\n", KB_N,
                KB.kb[0], KB.kb[1], KB.kb[2], KB.kb[3], why);
        } else {
            UL.kball = 0, DM.kb = 0;
            dbm_kb_fix_(why);
            kb_log("dbgmenu off");
            sl_persist_save();
            LOG("[dbgmenu] keyblades: OFF (%s)\n", why);
        }
    } else if (row == 2) {
        if (g_dbgall) { LOG("[dbgmenu] drive forms: refused, debugall is ON (%s)\n", why); return DM.forms; }
        if (val == DM.forms) return val;
        if (val) DM.beasts_prev = UL.beasts_fake, UL.beasts_fake = 4, DM.forms = 1, dbm_forms_snap_(why);
        else UL.beasts_fake = DM.beasts_prev, DM.forms = 0, dbm_forms_restore_(why), sl_persist_save();
        LOG("[dbgmenu] drive forms: %s (beasts for the gate %d, was %d) (%s)\n", val ? "ON" : "OFF", ul_beasts(), DM.beasts_prev, why);
        ul_forms_log();
    } else if (row == 3) {
        if (val == DM.smn) return val;
        if (val) {
            DM.own_prev = SMN.own, DM.gn_prev = smn_own_x[2], DM.pp_prev = smn_own_x[3];
            SMN.own = 1, smn_own_x[2] = 1, smn_own_x[3] = 1, DM.smn = 1;  // (Stitch: SMN.own_st untouched = parked)
        } else {
            SMN.own = DM.own_prev, smn_own_x[2] = DM.gn_prev, smn_own_x[3] = DM.pp_prev, DM.smn = 0;
        }
        dbm_smn_dirty_();
        LOG("[dbgmenu] summons: %s (Chicken Little %d, Genie %d, Peter Pan %d, Stitch %d; before %d/%d/%d) (%s)\n", val ? "ON" : "OFF",
            smn_owned_def(0), smn_owned_def(2), smn_owned_def(3), smn_owned_def(1), DM.own_prev, DM.gn_prev, DM.pp_prev, why);
    } else if (row == 4) {
        if (g_dbgall) { LOG("[dbgmenu] all abilities: refused, debugall is ON (%s)\n", why); return DM.ab; }
        if (val == DM.ab) return val;
        if (val) {
            DM.ab = 1;
            dbm_exp_snap_(why);
            dbm_ab_apply_();
            ab_tick();
        } else {
            DM.ab = 0;
            dbm_exp_restore_(why);
            if (AB.all) AB.on = AB.a_on, AB.seen = AB.a_seen, AB.all = false, AB.dirty = true;  // the level row's set if on, else the real masks
            ab_tick();
        }
        sl_persist_save();
        ab_log("dbgmenu all abilities");
        LOG("[dbgmenu] all abilities: %s (%s%s)\n", val ? "ON, every ability learned + equipped incl. chest ones" : "OFF", why,
            val ? "" : SL.devlv ? ", back to the test level's set" : ", back to the real masks");
    } else if (row == 5) {
        if (val == g_dbm_god) return val;
        g_dbm_god = val;
        LOG("[dbgmenu] god mode: %s (%s)\n", val ? "ON, hits dropped + life kept full" : "OFF", why);
    } else if (row == 6) {
        if (val == g_dbm_clip) return val;
        g_dbm_clip = val;  // dbm_clip_tick logs the start / the release at the next driven frame
        LOG("[dbgmenu] no clip: %s (%s)\n", val ? "ON" : "OFF", why);
    } else return 0;
    dbm_persist();
    return val;
}

static void dbm_log(const char *why) {
    LOG("[dbgmenu] state (%s): become sora %d, outfit %d (%s), auto outfits %d (auto now %s), test level %d (real LV %d), keyblades %d, drive forms %d, summons %d, god %d, no clip %d (speed %.1f), all abilities %d (AP %d/%d) (owned: Chicken %d Stitch %d Genie %d PeterPan %d)\n",
        why, g_dbm_sora, g_outfit, OF_NAME[g_outfit], g_outfit_auto, OF_NAME[OF.auto_now], SL.devlv, SL.lv, DM.kb, DM.forms, DM.smn, g_dbm_god, g_dbm_clip, g_dbm_clip_spd, DM.ab, ab_used(AB.on), ab_ap_max(), smn_owned_def(0), smn_owned_def(1), smn_owned_def(2), smn_owned_def(3));
}
static void dbm_all_off(const char *why) {
    for (int r = 9; r >= 0; r--) dbm_set(r, 0, why);  // (s146: + Become Sora / Outfit / Auto Outfits)
}
// "set debugall 1 / 2" is about to snapshot the live values: the level, Keyblade and Drive Form toggles go back first
static void dbm_pre_debugall(void) {
    if (!SL.devlv && !DM.kb && !DM.forms && !DM.ab) return;
    LOG("[dbgmenu] debugall is going ON: level, keyblades, drive forms and all abilities toggles put back first\n");
    for (int r = 4; r >= 0; r--) if (r != 3) dbm_set(r, 0, "before debugall");
}

// around BotW's save / load (sl_slot_saved / sl_slot_load): the save must see the real Keyblade slots
static int dbm_suspend(void) {
    int w = 0;
    if (DM.kb) {
        memcpy(DM.kb_hold, KB.kb, sizeof DM.kb_hold);
        dbm_kb_fix_("BotW save / load");
        w |= 1;
    }
    if (DM.ab) {  // (s141) the save sees the masks under All Abilities
        DM.ab_susp = 1;
        dbm_ab_pause_();
        w |= 2;
    }
    if (DX.f_on) {
        DH.f = 1, DH.ve = VL.exp, DH.we = WL.exp, DH.le = LL.exp, DH.me = ML.exp, DH.fe = FL.exp, DH.anti = AN.pts;
        DH.aon = AB.on & dbm_autos_(), DH.aseen = AB.seen & dbm_autos_();
        dbm_forms_put_(), w |= 4;
    }
    if (DX.e_on) DH.e = 1, DH.exp = SL.exp, DH.kills = SL.kills;
  // (s142) the save / load sees the real EXP (the debug values are not kept: a re-snapshot at resume)
    if (DX.e_on) dbm_exp_put_(), w |= 8;
    return w;
}
static void dbm_resume(int was, bool is_load) {
    if (was & 2) {  // (s141) All Abilities goes back on over what is now in AB.on / seen (a load: that slot's; a New Game save: empty)
        if (is_load && SL.devlv) dbm_lv_layer_("load");  // (ab_susp still 1: ab_tick must not apply All Abilities under it)
        DM.ab_susp = 0;
        dbm_ab_apply_();
        ab_tick();
        LOG("[dbgmenu] all abilities: %s done, layer back on\n", is_load ? "load" : "save");
    }
    if (was & 4) dbm_forms_snap_(is_load ? "after load: that slot's values" : "after save");  // (flag still on)
    if (was & 8) dbm_exp_snap_(is_load ? "after load: that slot's values" : "after save");
    {   // (s142 review) a save (not a load / New Game): the live debug values come back; the pending captures stay on the real ones
        bool ng = g_dbm_newgame;
        g_dbm_newgame = 0;
        if (!is_load && !ng) {
            if ((was & 4) && DH.f) {
                VL.exp = DH.ve, WL.exp = DH.we, LL.exp = DH.le, ML.exp = DH.me, FL.exp = DH.fe, AN.pts = DH.anti;
                VL.queue = WL.queue = LL.queue = ML.queue = FL.queue = 0;
                dbg_forms_persist();
                if (!AB.all) AB.on |= DH.aon, AB.seen |= DH.aseen;
                else AB.a_on |= DH.aon, AB.a_seen |= DH.aseen;  // (the All Abilities layer's stored set / the test level's)
                AB.dirty = true;
                LOG("[dbgmenu] form EXP: debug values back after the save (Valor %u Wisdom %u Limit %u Master %u Final %u, Anti %d)\n", DH.ve, DH.we, DH.le, DH.me, DH.fe, DH.anti);
            }
            if ((was & 8) && DH.e) {
                SL.exp = DH.exp, SL.kills = DH.kills, SL.lv = sl_level_of(SL.exp), AB.dirty = true;
                LOG("[dbgmenu] Sora EXP: debug value back after the save (EXP %u, LV %d, %u kills)\n", SL.exp, SL.lv, SL.kills);
            }
            sl_persist_save();
        } else if (was & 12) LOG("[dbgmenu] EXP: %s: the new real values stay (snapshots retaken)\n", is_load ? "load" : "New Game");
        DH.f = DH.e = 0;
    }
    if (!(was & 1) || !DM.kb) { if (was & 14) dbm_persist(); return; }
    memcpy(DM.kbs, KB.kb, sizeof DM.kbs);  // load AND save: KB.kb is the real set now (a New Game save resets it to the defaults)
    if (!is_load) memcpy(KB.kb, DM.kb_hold, sizeof KB.kb);  // kball is still on: what was picked while ON stays
    dbm_persist();
    LOG("[dbgmenu] keyblades: %s done, %s\n", is_load ? "load" : "save", is_load ? "new real snapshot" : "debug picks back");
}

static bool dbm_set_key(const char *key, int v) {
    if (strncmp(key, "dbm", 3)) return false;
    if (!strcmp(key, "dbmlv")) dbm_set(0, v, "live set dbmlv");
    else if (!strcmp(key, "dbmkb")) dbm_set(1, v, "live set dbmkb");
    else if (!strcmp(key, "dbmforms")) dbm_set(2, v, "live set dbmforms");
    else if (!strcmp(key, "dbmsmn")) dbm_set(3, v, "live set dbmsmn");
    else if (!strcmp(key, "dbmab")) dbm_set(4, v, "live set dbmab");
    else if (!strcmp(key, "dbmgod")) dbm_set(5, v, "live set dbmgod");
    else if (!strcmp(key, "dbmclip")) dbm_set(6, v, "live set dbmclip");
    else if (!strcmp(key, "dbmsora")) dbm_set(7, v, "live set dbmsora");  // (s146 outfit)
    else if (!strcmp(key, "dbmoutfit")) dbm_set(8, v, "live set dbmoutfit");
    else if (!strcmp(key, "dbmautoout")) dbm_set(9, v, "live set dbmautoout");
    else if (!strcmp(key, "dbmcold")) g_of_cold = v < -1 ? -1 : v, LOG("[outfit] dbmcold %d (%s)\n", g_of_cold, g_of_cold == -1 ? "snow weather" : g_of_cold == 0 ? "never cold" : g_of_cold == 1 ? "always cold" : "climate id = value - 2");
    else if (!strcmp(key, "dbmofres")) g_of_reserve = v < 0 ? 0 : v, LOG("[outfit] draw actors an outfit leaves free: %d\n", g_of_reserve);
    else if (!strcmp(key, "dbmclipspd")) g_dbm_clip_spd = v > 0 ? (float)v : 0, LOG("[dbgmenu] no clip speed %d u/s (0 = KH2 run speed)\n", v);
    else if (!strcmp(key, "dbmoff")) { if (v) dbm_all_off("live set dbmoff"); }
    else if (!strcmp(key, "dbmlog")) { if (v) dbm_log("live set dbmlog"); }
    else return false;
    return true;
}

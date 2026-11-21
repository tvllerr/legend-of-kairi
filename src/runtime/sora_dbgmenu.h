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
//                       skipped while on; Off = the char setting again, 0 = auto: Roxas until the paraglider; s233: + Dodge Roll learned
//                       while on, never saved: dbm_roll_sync_ / sora_abil.h g_ab_roll_dbg),
//                 row 8 Outfit = g_outfit 0 Normal / 1 Halloween Town / 2 Christmas Town / 3 Space Paranoids / 4 Timeless River / (s263 looks) 5 Roxas / 6 Kairi / 7 Green Tunic,
//                 row 9 Auto Outfits = g_outfit_auto (night -> Halloween Town, cold -> Christmas Town; sora_outfit.h). None is saved.
//   The page shows them on top in the order Become Sora, Outfit, Auto Outfits (CP_DB_ORD in sora_camp_page.h).
//   (s217 riku / s220 members) row 10 Party 1 = who stands in Donald's place, row 11 Party 2 = in Goofy's place: the value = the
//                 character id (sora_pchar.h PCH_*: 0 Donald, 1 Goofy, 2 Riku, 3 Auron, 4 Mulan, 5 Aladdin, 6 Sparrow, 7 Cloud); sora_party.h
//                 party_swap (session only, rides a hot reload in its own env var SORA_PARTY_PICK, never saved).
// Live keys: dbmparty 0/1 (s217: Donald / Riku in Donald's place), dbmparty1 <id> / dbmparty2 <id> (s220), dbmlv N (0 = off, 1..99), dbmkb 0/1, dbmforms 0/1, dbmsmn 0/1, dbmab, dbmgod, dbmclip, dbmsora 0/1, dbmoutfit 0..4,
// dbmautoout 0/1 (+ dbmcold / dbmofres, sora_outfit.h), dbmoff 1 (all off), dbmlog 1 (state line).
//   (s233 dbgmenu, recon/s233/S233_HELPER_DBGMENU.md) row 12 Infinite Drive = DM.inf: the Drive gauge is refilled to full every world
//                 frame (dbm_world_tick), a form's own gauge never runs down (sora_form.h fm_tick asks dbm_form_hold), and a real Drive
//                 makes no Anti roll / Anti point change (sora_anti.h anti_choice asks dbm_inf_drive). Off = the gauge from before it went on
//                 (DM.dg_prev; a BotW save writes that one, a load makes the slot's the new "before"). Refused while debugall is ON.
//                 row 13 Drive Form = DFM.want (0 Off, 1 Valor .. 6 Anti = KH2 form id): the real Drive path (fm_drive free, unlock gate
//                 skipped, no Anti roll) on the next free driven frame (dbm_form_tick, sora.c next to the Drive command), the form's gauge
//                 held full (dbm_form_hold); another form = revert, then the new one; Off = fm_revert. A Revert command / Mickey / Roxas
//                 ends it (row Off); a death / load reverts it and it comes back when Sora is driven again.
//                 row 14 Mickey Rescue = DMK: 0 Off, 1 Any Enemy (g_mk_any held 1; Off = the value before), 2 Now (one shot: mkforce on the
//                 next world frame, the roll 100 %, Sora stands in for Roxas (g_dbm_mksora) and everything goes back at "Mickey mode over").
//                 row 15 Infinite MP = DM.imp: sora.c mp_change skips every MP spend (no MP Charge can start) + MP refilled each world
//                 frame; Off = the MP / MP Charge from before it went on (MP is never saved).
// Live keys (s233): dbmdrive 0/1, dbmform 0..6, dbmmickey 0..2, dbmmp 0/1.
//   (dbgjump) row 16 High Jump / row 17 Aerial Dodge = g_dbm_hjump / g_dbm_adodge (sora.c): Off / LV1 / LV2 / LV3 = base Sora's High
//                 Jump (235 / 310 / 385 cm) / Aerial Dodge level is at least that, at once (KH2 makes him earn them: Valor's / Master's
//                 growth - 1, LV3 only at form LV7); a higher earned level still wins; Off = KH2's rule alone. Drive Forms keep their
//                 own. Never saved (session; a hot reload keeps them via DBM_ENV). Live keys dbmhjump 0..3, dbmadodge 0..3.
// Included from sora.c after sora_summon_menu.h. All LOG lines carry "[dbgmenu]".
#pragma once
_Static_assert(CP_DB_OUTFITS == OF_N, "s263: the camp Debug page's Outfit tags (sora_camp_dbg.h) must match sora_outfit.h OF_N");
#define DBM_ENV "SORA_DBGMENU"
// backend rows (dbm_set / dbm_state; the page's CP_DB_ORD shows them in its own order). (s233) a new row = a number here + its name /
// help / value list in sora_camp_dbg.h + a place in CP_DB_ORD + its case in dbm_set / dbm_state / dbm_persist + a DBM_KEYS entry
enum { DBM_R_LV, DBM_R_KB, DBM_R_FORMS, DBM_R_SMN, DBM_R_AB, DBM_R_GOD, DBM_R_CLIP, DBM_R_SORA, DBM_R_OUTFIT, DBM_R_AUTOOUT, DBM_R_PARTY1,
       DBM_R_PARTY2, DBM_R_INFDRIVE, DBM_R_FORM, DBM_R_MICKEY, DBM_R_INFMP, DBM_R_HJUMP, DBM_R_ADODGE, DBM_NROWS };
_Static_assert(DBM_NROWS == CP_DB_ROWS, "sora_camp_page.h CP_DB_ROWS = the backend rows here");

static struct {
    int kb, forms, smn;            // 1 = ON
    int inf, dg_prev;              // (s233) Infinite Drive ON; the Drive gauge points (bars x 100 + fill) before it went ON
    int imp;                       // (s233) Infinite MP ON
    float mp_prev, mpc_prev, mpt_prev;  // (s233) MP.mp / charge / charge_total before Infinite MP went ON
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

// (s233) the Drive Form row: want 0 Off / 1..6 = FORM_VALOR..FORM_ANTI + 1 (KH2 form id, = "set form N"); phase: DFP_WANT = drive on the next free
// driven frame, DFP_OURS = our CHANGEFORM / form is running (cur = its form), DFP_REVERT = our revert for a switch, the next drive after `wait` frames
enum { DFP_IDLE, DFP_WANT, DFP_OURS, DFP_REVERT };
#define DBM_FORM_N 7       // Off + 6 forms
#define DBM_FORM_TRIES 5   // drives in a row that never reached the form (Link took the pad mid-change ...) -> the row gives up (Off)
#define DBM_SWITCH_WAIT 20 // driven frames between our revert and the next form (the revert fx plays; GUESS)
static struct { int want, phase, cur, wait, tries; const char *wait_why; } DFM;
// (s233) the Mickey Rescue row: any = Any Enemy ON (g_mk_any held 1, any_prev = its value before); now = the one-shot "Now" (DMK_*)
enum { DMK_NONE, DMK_WAIT_SORA, DMK_KO, DMK_RUN };
static struct { int any, any_prev, now; uint64_t t; } DMK;
static const char *const DBM_FORM_NAME[DBM_FORM_N] = {"Off", "Valor", "Wisdom", "Limit", "Master", "Final", "Anti"};

static void dbm_persist(void) {
    if (!DM.kb && !DM.forms && !DM.smn && !DM.ab && !g_dbm_god && !g_dbm_clip && !g_dbm_sora && !g_outfit && !g_outfit_auto && !DM.inf && !DM.imp && !DFM.want && !DMK.any && !g_dbm_hjump && !g_dbm_adodge) { SetEnvironmentVariableA(DBM_ENV, NULL); return; }
    char b[640];
    int n = snprintf(b, sizeof b, "v1 kb=%d fm=%d sm=%d k0=%d k1=%d k2=%d k3=%d bp=%d op=%d gp=%d pp=%d ab=%d", DM.kb, DM.forms, DM.smn, DM.kbs[0],
             DM.kbs[1], DM.kbs[2], DM.kbs[3], DM.beasts_prev, DM.own_prev, DM.gn_prev, DM.pp_prev, DM.ab);
    n += snprintf(b + n, sizeof b - (size_t)n, " gd=%d nc=%d", g_dbm_god, g_dbm_clip);
    n += snprintf(b + n, sizeof b - (size_t)n, " so=%d of=%d au=%d", g_dbm_sora, g_outfit, g_outfit_auto);  // (s146 outfit)
    if (DX.f_on) n += snprintf(b + n, sizeof b - (size_t)n, " xf=1 ve=%u we=%u le=%u me=%u fe=%u an=%d", DX.ve, DX.we, DX.le, DX.me, DX.fe, DX.anti);
    if (DX.e_on) n += snprintf(b + n, sizeof b - (size_t)n, " xe=1 ex=%u ki=%u", DX.exp, DX.kills);
    // (s233) appended fields: an older DLL ignores them, an older env line without them = all three rows Off. The "Now" one-shot is never written.
    if (DM.inf) n += snprintf(b + n, sizeof b - (size_t)n, " id=1 ip=%d", DM.dg_prev);
    if (DFM.want) n += snprintf(b + n, sizeof b - (size_t)n, " fo=%d", DFM.want);
    if (DMK.any) n += snprintf(b + n, sizeof b - (size_t)n, " mk=1 mp=%d", DMK.any_prev);
    if (DM.imp) n += snprintf(b + n, sizeof b - (size_t)n, " im=1 mq=%.2f mc=%.2f mt=%.2f", DM.mp_prev, DM.mpc_prev, DM.mpt_prev);
    if (g_dbm_hjump || g_dbm_adodge) n += snprintf(b + n, sizeof b - (size_t)n, " hj=%d ad=%d", g_dbm_hjump, g_dbm_adodge);  // (dbgjump)
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
    { const char *g = strstr(b, " hj="); int a = 0, c = 0; if (g && sscanf(g, " hj=%d ad=%d", &a, &c) == 2) g_dbm_hjump = a < 0 ? 0 : a > 3 ? 3 : a, g_dbm_adodge = c < 0 ? 0 : c > 3 ? 3 : c; }  // (dbgjump)
    { const char *g = strstr(b, " so="); int a = 0, c = 0, d = 0; if (g && sscanf(g, " so=%d of=%d au=%d", &a, &c, &d) == 3) g_dbm_sora = a != 0, g_outfit = c < 0 || c >= OF_N ? 0 : c, g_outfit_auto = d != 0; }  // (s146)
    {   // (s233) Infinite Drive / Drive Form / Mickey Rescue Any Enemy / Infinite MP
        const char *g = strstr(b, " id=");
        int a = 0, c = 0;
        if (g && sscanf(g, " id=%d ip=%d", &a, &c) == 2 && a) DM.inf = 1, DM.dg_prev = c;
        g = strstr(b, " fo=");
        if (g && sscanf(g, " fo=%d", &a) == 1 && a > 0 && a < DBM_FORM_N) DFM.want = a, DFM.phase = DFP_WANT;  // (the new DLL's FM starts in base Sora: drive it again)
        g = strstr(b, " mk=");
        if (g && sscanf(g, " mk=%d mp=%d", &a, &c) == 2 && a) DMK.any = 1, DMK.any_prev = c != 0, g_mk_any = 1;
        g = strstr(b, " im=");
        float q = 100, qc = 0, qt = 0;
        if (g && sscanf(g, " im=%d mq=%f mc=%f mt=%f", &a, &q, &qc, &qt) == 4 && a) DM.imp = 1, DM.mp_prev = q, DM.mpc_prev = qc, DM.mpt_prev = qt;
        if (DM.inf || DFM.want || DMK.any || DM.imp)
            LOG("[dbgmenu] hot reload: infinite drive %d (gauge before %d), drive form %s (driven again), mickey any enemy %d, infinite mp %d (MP before %.0f)\n",
                DM.inf, DM.dg_prev, DBM_FORM_NAME[DFM.want], DMK.any, DM.imp, DM.mp_prev);
    }
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

static int dbm_state(int row) { return row == 0 ? SL.devlv : row == 1 ? DM.kb : row == 2 ? DM.forms : row == 3 ? DM.smn : row == 4 ? DM.ab : row == 5 ? g_dbm_god : row == 6 ? g_dbm_clip : row == 7 ? g_dbm_sora : row == 8 ? g_outfit : row == 9 ? g_outfit_auto : row == 10 || row == 11 ? (party_slot_off(row - 10) ? PCH_NONE : g_pchar[row - 10]) :row == DBM_R_INFDRIVE ? DM.inf : row == DBM_R_FORM ? DFM.want : row == DBM_R_MICKEY ? (DMK.now ? 2 : DMK.any) : row == DBM_R_INFMP ? DM.imp : row == DBM_R_HJUMP ? g_dbm_hjump : row == DBM_R_ADODGE ? g_dbm_adodge : 0; }  // (the camp page at open)

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
// (s233 player report) Become Sora = Sora's Dodge Roll too (sora_abil.h g_ab_roll_dbg): follows g_dbm_sora (the row, dbmsora, a hot reload).
// ON: ab_tick's sync learns + equips it (AP 0) the way the paraglider does (no popup: a real learn has none either). OFF on a save without the
// paraglider: the bit leaves every mask layer (and debugall's real snapshot), so no "seen / off" Dodge Roll is left to reach a save.
static void dbm_roll_sync_(void) {
    int want = g_dbm_sora != 0;
    if (want == g_ab_roll_dbg) return;
    g_ab_roll_dbg = want, AB.dirty = true;
    if (!want && !g_ab_glider && g_chestfirst) {
        abm_t b = ab_bit(AB_DODGE_ROLL);
        AB.on &= ~b, AB.seen &= ~b, AB.dev_on &= ~b, AB.dev_seen &= ~b, AB.a_on &= ~b, AB.a_seen &= ~b;
        if (g_dbgall) DBR.ab_on &= ~b, DBR.ab_seen &= ~b;
    }
    LOG("[dbgmenu] become sora: Dodge Roll %s (paraglider %d, chestfirst %d)\n", want ? "learned with Sora's body (not saved)"
        : g_ab_glider || !g_chestfirst ? "stays (really learned)" : "gone again (Roxas has no roll; it comes with the paraglider)", g_ab_glider, g_chestfirst);
}
static void dbm_ab_tick(void) {
    dbm_god_tick_();  // (s143 dbggod) the per-frame hook of this module (ab_tick -> here)  // after a hot reload (the env says ON, the real masks were loaded since) / a save or load
    dbm_roll_sync_();  // (s233)
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

// ---- (s233) Infinite Drive / Infinite MP
static bool dbm_inf_drive(void) { return DM.inf && !g_dbgall; }
static bool dbm_inf_mp(void) { return DM.imp != 0; }
static int dbm_inf_set_(int val, const char *why) {
    val = val != 0;
    if (val == DM.inf) return val;
    if (val && g_dbgall) { LOG("[dbgmenu] infinite drive: refused, debugall is ON (it owns the Drive gauge snapshot) (%s)\n", why); return 0; }
    dg_init();
    if (val) {
        DM.dg_prev = dg_points(), DM.inf = 1;
        LOG("[dbgmenu] infinite drive: ON (%s): Drive gauge kept full (max %d bars), a form's gauge never runs down, a Drive makes no Anti roll / point change; gauge before = %d bars + %d\n",
            why, dg_max(), DM.dg_prev / 100, DM.dg_prev % 100);
    } else {
        DM.inf = 0;
        dg_load_points(DM.dg_prev, "dbgmenu infinite drive off: the gauge from before");
        LOG("[dbgmenu] infinite drive: OFF (%s): gauge back to %d bars + %d, the form gauge runs down again from where it is\n", why, DG.bars, DG.fill);
    }
    dbm_persist();
    return val;
}
static int dbm_mp_set_(int val, const char *why) {
    val = val != 0;
    if (val == DM.imp) return val;
    if (val) {
        DM.mp_prev = MP.mp, DM.mpc_prev = MP.charge, DM.mpt_prev = MP.charge_total, DM.imp = 1;
        LOG("[dbgmenu] infinite mp: ON (%s): MP spends cost nothing, no MP Charge, MP kept full; MP before = %.0f%s\n", why, DM.mp_prev,
            DM.mpt_prev > 0 ? " (MP Charge running)" : "");
    } else {
        DM.imp = 0;
        MP.mp = DM.mp_prev > MP.mp_max ? MP.mp_max : DM.mp_prev, MP.charge = DM.mpc_prev, MP.charge_total = DM.mpt_prev;
        LOG("[dbgmenu] infinite mp: OFF (%s): MP back to %.0f%s\n", why, MP.mp, MP.charge_total > 0 ? " (the MP Charge from before goes on)" : "");
    }
    dbm_persist();
    return val;
}
static void dbm_inf_tick_(void) {
    if (dbm_inf_drive()) {  // silent refill (a Drive just spent its bars: they are back the next frame; no bar / full SE spam)
        dg_init();
        int mx = dg_max();
        if (DG.bars < mx || DG.fill < 100) DG.bars = mx, DG.fill = 100, dg_persist();
    }
    if (DM.imp) MP.mp = MP.mp_max, MP.charge = MP.charge_total = 0;  // (a charge that was running when it went ON ends; the HUD bar is full)
}

// ---- (s233) the Drive Form row
// a reason the form can NOT run now and will not by waiting (the row is refused / goes Off), else NULL
static const char *dbm_form_block_(int f) {
    if (RX.on && rox_wanted() != 0) return "Roxas is the body (no Drive Forms before the paraglider; Become Sora first)";
    if (MK.active || MK.choosing || MK.body) return "Mickey's rescue is running";
    if (sm_out()) return "a summon is out (KH2: no Drive while a summon is out)";
    if (f == FORM_LIMIT && !g_fm_limit_on) return "Limit Form is switched off (set fmlimit 1)";
    if (f == FORM_MASTER && !ml_form_on()) return "Master Form is switched off (set fmmaster 1)";
    if (f == FORM_FINAL && !g_fmfinal_on) return "Final Form is switched off (set fmfinal 1)";
    if (FM.tried && (!FM.set_body || FM.actor[f] <= 0 || g_nfmots_tab[f] <= 0))
        return "its draw actor / motion bake is not loaded (data/forms, exe draw actor table)";
    return NULL;
}
// a reason to wait (not now, the row stays and tries again on a later driven frame), else NULL
static const char *dbm_form_busy_(bool ground) {
    if (linkpos_link_life() <= 0) return "KO";
    if (RX.on) return "Roxas -> Sora switch pending";
    if (!ground || ACT_AIR(S.act)) return "in the air";
    if (S.special != SP_NONE) return "busy (attack / reaction / item)";
    if (SKT.riding) return "on the skateboard";
    if (pl_hold()) return "a party Limit is playing";
    return NULL;
}
static void dbm_form_off_(const char *why) {
    DFM.want = 0, DFM.phase = DFP_IDLE, DFM.tries = 0, DFM.wait_why = NULL;
    LOG("[dbgmenu] form: row Off (%s)\n", why);
    dbm_persist();
}
static bool dbm_form_hold(void) { return dbm_inf_drive() || (DFM.want && DFM.phase == DFP_OURS && FM.st == FM_FORM && g_fform == DFM.cur); }
// every driven frame, right after sora.c's Drive / Revert command dispatch (ground = Link on the ground, no picker / KO screen up)
static void dbm_form_tick(bool ground) {
    if (!DFM.want && DFM.phase == DFP_IDLE) return;
    if (DFM.phase == DFP_OURS && FM.st == FM_BASE) {  // our form ended, not by this row
        if (!DFM.want) DFM.phase = DFP_IDLE;
        else if (!strcmp(g_fm_rev_why, "dead / loading") || !strcmp(g_fm_rev_why, "not driven during the change")) {
            if (++DFM.tries >= DBM_FORM_TRIES) { dbm_form_off_("the form kept ending before it started"); return; }
            DFM.phase = DFP_WANT;
            LOG("[dbgmenu] form: %s ended (%s): it comes back on the next free driven frame\n", FM_FORMS[DFM.cur].name, g_fm_rev_why);
        } else {
            char w[96];
            snprintf(w, sizeof w, "Sora left %s Form: %s", FM_FORMS[DFM.cur].name, g_fm_rev_why);
            dbm_form_off_(w);
            return;
        }
    }
    if (DFM.phase == DFP_OURS && FM.st == FM_FORM && g_fform == DFM.cur) DFM.tries = 0;  // (reached the form)
    if (!DFM.want) {  // Off: the real Revert path
        if (DFM.phase == DFP_OURS && FM.st != FM_BASE) fm_revert("Debug Menu Form Off");
        DFM.phase = DFP_IDLE, DFM.tries = 0, DFM.wait_why = NULL;
        return;
    }
    int f = DFM.want - 1;
    if (DFM.phase == DFP_OURS) {
        if (DFM.cur == f) return;  // in it (the gauge is held by dbm_form_hold)
        fm_revert("Debug Menu Form: switch");  // KH2 has no form-to-form Drive: revert first, the next one after DBM_SWITCH_WAIT frames
        DFM.phase = DFP_REVERT, DFM.wait = 0;
        return;
    }
    if (DFM.phase == DFP_REVERT) {
        if (++DFM.wait < DBM_SWITCH_WAIT) return;
        DFM.phase = DFP_WANT;
    }
    if (FM.st == FM_CHANGE) return;  // (a real Drive's CHANGEFORM is running: let it land, then replace it)
    if (FM.st == FM_FORM && g_fform == f) {  // a real Drive already gave this form: it is ours now (held from here on)
        DFM.cur = f, DFM.phase = DFP_OURS, DFM.tries = 0;
        LOG("[dbgmenu] form: Sora is already in %s Form: the row holds it\n", FM_FORMS[f].name);
        return;
    }
    if (FM.st == FM_FORM) {          // a real Drive's form (not ours): revert it, then ours
        fm_revert("Debug Menu Form: replace");
        DFM.phase = DFP_REVERT, DFM.wait = 0;
        return;
    }
    fm_init(), fm_load_actor();
    const char *no = dbm_form_block_(f);
    if (no) { char w[128]; snprintf(w, sizeof w, "%s refused: %s", FM_FORMS[f].name, no); dbm_form_off_(w); return; }
    const char *busy = dbm_form_busy_(ground);
    if (busy) {
        if (busy != DFM.wait_why) LOG("[dbgmenu] form: %s waits (%s)\n", FM_FORMS[f].name, busy);
        DFM.wait_why = busy;
        return;
    }
    DFM.wait_why = NULL;
    int af = g_antiforce;  // (the row gives exactly the picked form: antiforce stays for the next real Drive)
    g_antiforce = 0, g_fm_gate_skip = 1;
    bool ok = fm_drive(f, true);  // the Drive command's own path, free: CHANGEFORM, sphere SE + fx, swap, entry, party absorbed
    g_fm_gate_skip = 0, g_antiforce = af;
    if (!ok) { char w[96]; snprintf(w, sizeof w, "%s refused by fm_drive (see the [form] line)", FM_FORMS[f].name); dbm_form_off_(w); return; }
    DFM.cur = g_fform, DFM.phase = DFP_OURS;
    LOG("[dbgmenu] form: DRIVE %s (Debug Menu: free, unlock gate skipped, no Anti roll; its gauge stays full until the row is Off)\n", FM_FORMS[DFM.cur].name);
}
static int dbm_form_set_(int val, const char *why) {
    val = val < 0 ? 0 : val >= DBM_FORM_N ? DBM_FORM_N - 1 : val;
    if (val == DFM.want) return val;
    if (val) {
        const char *no = dbm_form_block_(val - 1);
        if (no) { LOG("[dbgmenu] form: %s refused (%s): %s\n", DBM_FORM_NAME[val], why, no); return DFM.want; }
    }
    DFM.want = val, DFM.tries = 0, DFM.wait_why = NULL;
    if (val && DFM.phase == DFP_IDLE) DFM.phase = DFP_WANT;
    LOG("[dbgmenu] form: %s (%s)%s\n", DBM_FORM_NAME[val], why, val ? ": Sora drives into it on the next free driven frame and stays" : ": reverts on the next driven frame");
    dbm_persist();
    return val;
}

// ---- (s233) the Mickey Rescue row
static void dbm_mk_done_(const char *why) {
    DMK.now = DMK_NONE, DMK.t = 0, MK.sure = false;
    if (g_dbm_mksora) g_dbm_mksora = 0, LOG("[dbgmenu] mickey: Sora's stand-in for Roxas ends (the story / char rule picks the body again)\n");
    LOG("[dbgmenu] mickey: Now over (%s): row back to %s\n", why, DMK.any ? "Any Enemy" : "Off");
}
// every unpaused world frame, right after mk_tick: the forced KO goes in here (not at the menu: mkforce's 5 s window must not run under the pause)
static void dbm_mk_tick_(void) {
    if (!DMK.now) return;
    uint64_t now = GetTickCount64();
    if (!DMK.t) DMK.t = now;
    if (DMK.now == DMK_WAIT_SORA) {
        if (!g_mickey) { dbm_mk_done_("Mickey's rescue is off (set mickey 1)"); return; }
        if (MK.active || MK.choosing) { dbm_mk_done_("a rescue started by itself"); return; }
        if (RX.on) {  // KH2 has no rescue as Roxas: Sora stands in (rox_tick switches at the next free moment), Roxas comes back after
            if (!g_dbm_mksora) g_dbm_mksora = 1, LOG("[dbgmenu] mickey: Roxas is the body: Sora stands in for the rescue\n");
            if (now - DMK.t > 15000) dbm_mk_done_("Sora never became the body within 15 s (Roxas busy / assets)");
            return;
        }
        MK.force = true, MK.force_t = now, MK.sure = true, DMK.now = DMK_KO, DMK.t = now;
        LOG("[dbgmenu] mickey: KO forced now (mkforce): the KH2 choice screen, then Mickey for sure (100 %%, not counted in the fight's chance steps)\n");
        return;
    }
    if (DMK.now == DMK_KO) {
        if (MK.active || MK.choosing) { DMK.now = DMK_RUN, LOG("[dbgmenu] mickey: rescue running (%s)\n", MK.choosing ? "choice screen" : "Mickey is out"); return; }
        if (!MK.force) dbm_mk_done_("the forced KO started no rescue (timed out / Mickey off)");
        return;
    }
    if (!MK.active && !MK.choosing) dbm_mk_done_("Mickey mode over / the choice closed");
}
static int dbm_mk_set_(int val, const char *why) {
    val = val < 0 ? 0 : val > 2 ? 2 : val;
    int cur = DMK.now ? 2 : DMK.any;
    if (val == cur) return cur;
    if (DMK.now >= DMK_KO) {
        LOG("[dbgmenu] mickey: refused (%s): the forced rescue is running; the row goes back to %s by itself after it\n", why, DMK.any ? "Any Enemy" : "Off");
        return 2;
    }
    if (DMK.now) dbm_mk_done_("cancelled before it fired");  // (Now picked, then changed in the same menu visit: nothing fired yet)
    if (val == 2) {
        const char *no = !g_mickey ? "Mickey's rescue is off (set mickey 1)" : MK.active || MK.choosing ? "a rescue is already running"
                       : linkpos_link_life() <= 0 ? "Sora is KO'd" : NULL;
        if (no) { LOG("[dbgmenu] mickey: Now refused (%s): %s\n", why, no); return DMK.any; }
        // (s235 review fix) picking Now by hand passes through Any Enemy: take that back, so Now does not leave mkany on
        if (DMK.any) g_mk_any = DMK.any_prev, DMK.any = 0, dbm_persist();
        DMK.now = DMK_WAIT_SORA, DMK.t = 0;  // (the clock starts at the first world frame: dbm_mk_tick_)
        LOG("[dbgmenu] mickey: Now (%s): Sora's KO is forced on the next world frame%s\n", why, RX.on ? " (Roxas is the body: Sora stands in, Roxas comes back after)" : "");
        return 2;
    }
    if (val && !DMK.any) DMK.any_prev = g_mk_any, g_mk_any = 1, DMK.any = 1;
    else if (!val && DMK.any) g_mk_any = DMK.any_prev, DMK.any = 0;
    LOG("[dbgmenu] mickey: %s (%s) (mkany now %d)\n", DMK.any ? "Any Enemy: a KO by any enemy can call Mickey (KH2 chance table)" : "Off", why, g_mk_any);
    dbm_persist();
    return DMK.now ? 2 : DMK.any;
}

// ---- (s233) the Party rows' "None": the slot empty (sora_party.h g_pslot_off held through g_party_absorbed). on = None; false = refused
static bool dbm_party_none_(int slot, bool on, const char *why) {
    if (slot < 0 || slot >= PARTY_N || party_slot_off(slot) == on) return true;
    const char *home = PCH_TAB[PCH_HOME[slot]].name;
    if (on) {
        if (g_plim_hold[slot] || P.m[slot].kd != KD_NONE) {
            LOG("[dbgmenu] party: None in %s's place refused (%s): %s is %s right now, try again in a moment\n", home, why,
                PCH_TAB[g_pchar[slot]].name, g_plim_hold[slot] ? "in a party Limit" : "knocked down");
            return false;
        }
        if (!pchar_home(g_pchar, slot)) {  // a stand-in goes first (the slot keeps its own member underneath: he may stand in the other place now)
            if (g_party_absorbed[slot]) {
                LOG("[dbgmenu] party: None in %s's place refused (%s): %s is in a Drive Form / summon, try again after it\n", home, why, PCH_TAB[g_pchar[slot]].name);
                return false;
            }
            if (party_swap(slot, PCH_HOME[slot], why) != PCH_HOME[slot]) { LOG("[dbgmenu] party: None in %s's place refused (%s): the [party] line says why\n", home, why); return false; }
        }
        g_pslot_off[slot] = true;
        if (!g_party_absorbed[slot]) {
            g_party_absorbed[slot] = true;
            if (g_party_id[slot] > 0 && PD.off) PD.off(g_party_id[slot]);
        }
        LOG("[dbgmenu] party: None in %s's place (%s): not drawn, no fighting / sounds / Cure / HUD plate; forms / Limits that need him are refused, summons still work\n", home, why);
    } else {
        g_pslot_off[slot] = false;
        if (FM.st == FM_FORM && fm_takes(g_fform, slot))
            LOG("[dbgmenu] party: %s's place filled again (%s): %s comes back at the Drive Form's revert\n", home, why, home);
        else if (sm_out()) SM.abs[slot] = true, LOG("[dbgmenu] party: %s's place filled again (%s): %s comes back when the summon ends\n", home, why, home);
        else {
            g_party_absorbed[slot] = false, P.m[slot].placed = false;  // (party_core_tick places him beside Sora next tick)
            LOG("[dbgmenu] party: %s's place filled again (%s): %s back beside Sora\n", home, why, home);
        }
    }
    party_pick_persist();
    return true;
}

// every unpaused world frame (sora.c, right after mk_tick)
static void dbm_world_tick(void) {
    if (!DM.inited) return;  // (sl_init -> dbm_init runs first: the env's rows are in before anything refills)
    dbm_inf_tick_();
    dbm_mk_tick_();
}

// row: 0 level (val 0..99), 1 Keyblades, 2 Drive Forms, 3 Summons, 4 All Abilities, 5 God, 6 No Clip, 7 Become Sora (val 0/1), 8 Outfit (0..OF_N - 1), 9 Auto Outfits (0/1). Returns the value now in force.
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
        dbm_roll_sync_(), ab_tick();  // (s233: his Dodge Roll with him, in the Abilities list at once)
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
    if (row == 10 || row == 11) {  // (s217 riku / s220) Party 1 / 2: the character val in Donald's / Goofy's place; the one in force comes back
        int slot = row - 10;
        if (val == PCH_NONE || val < 0) return dbm_party_none_(slot, true, why) ? PCH_NONE : g_pchar[slot];  // (s233: None = the slot empty)
        if (party_slot_off(slot) && !dbm_party_none_(slot, false, why)) return PCH_NONE;  // (s233: a character again: the slot back first)
        int c = party_swap(slot, val, why);
        LOG("[dbgmenu] party: %s in %s's place (%s)%s\n", PCH_TAB[c].name, slot ? "Goofy" : "Donald", why, c != val ? " (refused: the [party] line says why)" : "");
        return c;
    }
    if (row == DBM_R_INFDRIVE) return dbm_inf_set_(val, why);  // (s233)
    if (row == DBM_R_FORM) return dbm_form_set_(val, why);
    if (row == DBM_R_MICKEY) return dbm_mk_set_(val, why);
    if (row == DBM_R_INFMP) return dbm_mp_set_(val, why);
    if (row == DBM_R_HJUMP || row == DBM_R_ADODGE) {  // (dbgjump) base Sora's High Jump / Aerial Dodge level at least val (0 = KH2's rule)
        int *g = row == DBM_R_HJUMP ? &g_dbm_hjump : &g_dbm_adodge;
        val = val < 0 ? 0 : val > 3 ? 3 : val;
        if (val == *g) return val;
        *g = val;
        if (row == DBM_R_HJUMP)
            LOG("[dbgmenu] high jump: %s %d (%s): base Sora jumps %.0f cm now (Valor LV%d)\n", val ? "debug LV" : "OFF, KH2's rule", val, why,
                vl_base_jump_h(KH_JUMP_H), vl_lv());
        else LOG("[dbgmenu] aerial dodge: %s %d (%s): base Sora's Aerial Dodge LV%d now (Master LV%d)\n", val ? "debug LV" : "OFF, KH2's rule", val, why,
                 bd_lv(), ml_lv());
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
    LOG("[dbgmenu] party: %s in Donald's place, %s in Goofy's (installed mask %#x) (absorbed %d %d)\n", party_slot_off(0) ? "None" : PCH_TAB[g_pchar[0]].name,
        party_slot_off(1) ? "None" : PCH_TAB[g_pchar[1]].name, party_have_mask(), g_party_absorbed[0], g_party_absorbed[1]);  // (s217 / s220; s233 None)
    LOG("[dbgmenu] state (%s): become sora %d, outfit %d (%s), auto outfits %d (auto now %s), test level %d (real LV %d), keyblades %d, drive forms %d, summons %d, god %d, no clip %d (speed %.1f), all abilities %d (AP %d/%d) (owned: Chicken %d Stitch %d Genie %d PeterPan %d)\n",
        why, g_dbm_sora, g_outfit, OF_NAME[g_outfit], g_outfit_auto, OF_NAME[OF.auto_now], SL.devlv, SL.lv, DM.kb, DM.forms, DM.smn, g_dbm_god, g_dbm_clip, g_dbm_clip_spd, DM.ab, ab_used(AB.on), ab_ap_max(), smn_owned_def(0), smn_owned_def(1), smn_owned_def(2), smn_owned_def(3));
    LOG("[dbgmenu] state 2 (%s): infinite drive %d (gauge %d bars + %d, before %d), infinite mp %d (MP %.0f/%.0f, charge %d, before %.0f), drive form row %s (phase %d, our form %s, now %s %s, gauge %.0f/%.0f, held %d), "
        "mickey row %s (now %d, mkany %d, Sora stand-in %d, mickey %d, active %d, choosing %d)\n",
        why, DM.inf, DG.bars, DG.fill, DM.dg_prev, DM.imp, MP.mp, MP.mp_max, mp_charging(), DM.mp_prev, DBM_FORM_NAME[DFM.want], DFM.phase,
        DFM.phase == DFP_OURS ? FM_FORMS[DFM.cur].name : "-", FM.st == FM_BASE ? "base" : FM.st == FM_CHANGE ? "CHANGEFORM" : "in", FM.st == FM_BASE ? "" : FM_FORMS[g_fform].name,
        FM.t, FM.total, dbm_form_hold(), DMK.now ? "Now" : DMK.any ? "Any Enemy" : "Off", DMK.now, g_mk_any, g_dbm_mksora, g_mickey, MK.active, MK.choosing);
    LOG("[dbgmenu] state 3 (%s): high jump row %d (base jump %.0f cm, Valor LV%d), aerial dodge row %d (base LV%d, Master LV%d)\n", why,
        g_dbm_hjump, vl_base_jump_h(KH_JUMP_H), vl_lv(), g_dbm_adodge, bd_lv(), ml_lv());  // (dbgjump)
}
static int dbm_off_val_(int r) { return r == DBM_R_PARTY1 || r == DBM_R_PARTY2 ? PCH_HOME[r - DBM_R_PARTY1] : 0; }  // (the Party rows' "off" = Donald / Goofy)
static void dbm_all_off(const char *why) {
    for (int r = DBM_NROWS - 1; r >= 0; r--) dbm_set(r, dbm_off_val_(r), why);  // (s146: + Become Sora / Outfit / Auto Outfits; s217 / s220: + Party 1 / 2 = Donald / Goofy; s233: + rows 12-15)
}
// "set debugall 1 / 2" is about to snapshot the live values: the level, Keyblade, Drive Form and Infinite Drive toggles go back first
static void dbm_pre_debugall(void) {
    if (!SL.devlv && !DM.kb && !DM.forms && !DM.ab && !DM.inf) return;
    LOG("[dbgmenu] debugall is going ON: level, keyblades, drive forms, all abilities and infinite drive toggles put back first\n");
    if (DM.inf) dbm_set(DBM_R_INFDRIVE, 0, "before debugall");  // (s233: its "before" gauge is the real one debugall must snapshot)
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
    if (DM.inf) {  // (s233) the sidecar's "drive" = the gauge from before Infinite Drive went ON (the refill comes back the next world frame)
        dg_load_points(DM.dg_prev, "dbgmenu infinite drive: the real gauge for a BotW save / load");
        w |= 16;
    }
    return w;
}
static void dbm_resume(int was, bool is_load) {
    if (was & 16) {  // (s233) a load / New Game: that gauge is the new "before"; a save: the same value
        DM.dg_prev = dg_points();
        LOG("[dbgmenu] infinite drive: %s done: gauge before = %d bars + %d, kept full again from the next world frame\n", is_load ? "load" : "save", DM.dg_prev / 100, DM.dg_prev % 100);
        dbm_persist();
    }
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
    else if (!strcmp(key, "dbmparty")) dbm_set(10, pchar_of_pick(v), "live set dbmparty");  // (s217 riku: 0 Donald, 1 Riku)
    else if (!strcmp(key, "dbmparty1")) dbm_set(10, v, "live set dbmparty1");  // (s220: character id in Donald's place)
    else if (!strcmp(key, "dbmparty2")) dbm_set(11, v, "live set dbmparty2");  // (s220: ... in Goofy's place)
    else if (!strcmp(key, "dbmcold")) g_of_cold = v < -1 ? -1 : v, LOG("[outfit] dbmcold %d (%s)\n", g_of_cold, g_of_cold == -1 ? "snow weather" : g_of_cold == 0 ? "never cold" : g_of_cold == 1 ? "always cold" : "climate id = value - 2");
    else if (!strcmp(key, "dbmofres")) g_of_reserve = v < 0 ? 0 : v, LOG("[outfit] draw actors an outfit leaves free: %d\n", g_of_reserve);
    else if (!strcmp(key, "dbmclipspd")) g_dbm_clip_spd = v > 0 ? (float)v : 0, LOG("[dbgmenu] no clip speed %d u/s (0 = KH2 run speed)\n", v);
    else if (!strcmp(key, "dbmoff")) { if (v) dbm_all_off("live set dbmoff"); }
    else if (!strcmp(key, "dbmlog")) { if (v) dbm_log("live set dbmlog"); }
    else {  // (s233) one table line per row from here on
        static const struct { const char *key; int row; } DBM_KEYS[] = {
            {"dbmdrive", DBM_R_INFDRIVE}, {"dbmform", DBM_R_FORM}, {"dbmmickey", DBM_R_MICKEY}, {"dbmmp", DBM_R_INFMP},
            {"dbmhjump", DBM_R_HJUMP}, {"dbmadodge", DBM_R_ADODGE}};  // (dbgjump)
        for (size_t i = 0; i < sizeof DBM_KEYS / sizeof *DBM_KEYS; i++)
            if (!strcmp(key, DBM_KEYS[i].key)) {
                char why[40];
                snprintf(why, sizeof why, "live set %s", DBM_KEYS[i].key);
                dbm_set(DBM_KEYS[i].row, v, why);
                return true;
            }
        return false;
    }
    return true;
}

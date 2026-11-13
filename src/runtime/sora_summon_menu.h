// (s137 smenu) The Summon command-menu entry, the FPS Mode pane and the owned-summon rule. Notes: recon/S137_HELPER_SMENU.md.
// KH2 truth (KH2 FM exe, Ghidra kh2proj; decomps in recon_s137/menu1.c, menu2.c):
//   * the Summon entry sits on KH2's SECOND top page (kind 6, FUN_1403f3860): Attack / Summon (cmd 72) / Party (73) / Limit (81). While a summon is out
//     (summon state DAT_142a24edc != 0) the Summon slot is cmd 316 "Dismiss" (state 0 only when the state is 2 and no Limit object runs, FUN_140401d60).
//     Summon (cmd 72, SubMenu 7) opens the list of owned summons; Chicken Little = cmd 447 (Execute 72, Cost 3 Drive bars, ground only).
//   * the Limit slot (81) is overwritten by the summon's own Limit while it is out (FPS Mode, cmd 529: Cost 250, Time 300); inside FPS Mode KH2 shows the
//     summon's special menu (FUN_1404060e0 kind 5): Firecracker (531) on Triangle, End (532) on Square, Balls (530) on the decide button (English table 0x5CC488).
//   * names (sys.bar): 1284 Summon, 15225 Chicken Little, 10434 Dismiss, 17051 FPS Mode, 17052 Balls, 17053 Firecracker, 17054 End.
// Here: page 2 = d-pad Left on the top page (as KH2's page toggle; only when a summon is owned and "smenu" is on); rows Attack / Summon-or-Dismiss / Limit
// (Party is not ours: hidden through sora_hud_menu_rows 0xB). Summon opens a text-row list (M.sub SMN_SUB). In FPS Mode the exe draws Mickey's button pane.
// Owned: Chicken Little comes with the paraglider (S132 s6 user decision): derived each frame from the flags, nothing saved.
// Live keys: smenu 0|1 (default 1; 0 = no menu entry, live keys only), sown -1|0|1 (-1 auto = paraglider, 0 / 1 force), sgauge 0|1|2 (see smn_gauge_state).
#pragma once
#include "sora_summon_fps_stub.h"

// cmd.bin labels (s137 bake, out_s137menu/cmd.bin; tools_s119/hudbake LabelNames)
enum { SMN_L_SUMMON = 44, SMN_L_CHICKEN, SMN_L_FPS, SMN_L_BALLS, SMN_L_FIRECRACKER, SMN_L_END, SMN_L_DISMISS, SMN_L_LIMIT = 23,
       SMN_L_STITCH = 51, SMN_L_OHANA, SMN_L_SHOOT, SMN_L_UKULELE, SMN_L_BLAST };  // (s137 stitch) 51..55 = tools_s119/hudbake LabelNames

// (s137 stitch) the Summon list shows every OWNED summon of SM_DEF (table order); pick = the list row that was chosen -> the row cast.
static struct { int menu, own, own_st, gauge; bool glist, page2, pane_on; int owned_last, pick, owned_mask_last; } SMN = {.menu = 1, .own = -1, .own_st = 0, .gauge = 2, .owned_last = -1, .owned_mask_last = -1};  // (s138 user: Stitch PARKED - KH2 draws him on the screen, ours walks in the world; own_st 0 = hidden from the Summon list; set sownst -1 = the 1st-beast rule, 1 = forced)

// ---- ownership: Chicken Little's charm comes with the paraglider, Stitch's with the 1st Divine Beast, Genie's with the 4th, Peter Pan's with the Master Sword (S132 section 6); Sora's body only
static int smn_own_x[SM_NDEF] = {[0 ... SM_NDEF - 1] = -1};  // (s138) sowngn (row 2) / sownpp (row 3): -1 = the real rule, 0 / 1 = forced
static const char *smn_unlock_name(int u) {
    static const char *const B[4] = {"Vah Ruta", "Vah Medoh", "Vah Rudania", "Vah Naboris"};
    return u >= SMU_RUTA && u <= SMU_NABORIS ? B[u - SMU_RUTA] : u == SMU_BEAST1 ? "1st Divine Beast" : u == SMU_BEAST4 ? "4th Divine Beast" : u == SMU_MSWORD ? "Master Sword" : "paraglider flag";
}
static bool smn_owned_def(int d) {
    const SmDef *D = &SM_DEF[d];
    int forced = d == 0 ? SMN.own : d == 1 ? SMN.own_st : smn_own_x[d];  // sown / sownst / sowngn / sownpp: -1 = the real rule, 0 / 1 = forced
    if (forced >= 0) return forced != 0;
    if (D->unlock == SMU_BEAST1) return ul_beasts() >= 1;
    if (D->unlock == SMU_BEAST4) return ul_beasts() >= 4;
    if (D->unlock == SMU_MSWORD) return UL.have && UL.st.msword;
    if (D->unlock >= SMU_RUTA && D->unlock <= SMU_NABORIS)  // (s147) that one beast (UL_BEAST_FLAG order Ruta Medoh Rudania Naboris); a fake 4 (set beasts 4 / debugall) = all done
        return (UL.have && UL.st.beast[D->unlock - SMU_RUTA]) || UL.beasts_fake >= 4;
    return g_ab_glider || (UL.have && UL.st.glider);
}
static int smn_owned_rows(int *rows) {  // the SM_DEF rows the player owns, in table order
    int n = 0;
    for (int d = 0; d < SM_NDEF; d++)
        if (smn_owned_def(d)) rows[n++] = d;
    return n;
}
static bool smn_owned(void) {
    int r[SM_NDEF];
    return smn_owned_rows(r) > 0;
}
static bool smn_menu_ok(void) { return SMN.menu && SM.on && smn_owned() && !IN_GUEST && !fm_in_form() && !g_loading; }
static bool smn_page2(void) { return SMN.page2 && smn_menu_ok(); }

// KH2 entry states (FUN_140401d60): the Summon entry is 5 (grey) when the Drive gauge < Cost 3, a party member is missing / KO'd (FUN_1404005e0), airborne (ground-only flag)
static int smn_state(void) {
    if (SM.st != SM_OFF) return 5;
    if (DG.bars < 3 || !sm_member_ok(0) || !sm_member_ok(1) || ACT_AIR(S.act) || SKT.riding || pl_hold_party()) return 5;
    if (!pchar_need_ok(g_pchar, PNEED_SUMMON)) return 5;  // (s217 riku: Donald + Goofy both, as KH2's member check: greyed)
    return 0;
}
static int smn_dismiss_state(void) { return SM.st == SM_ACTIVE && !pl_hold_party() && !sm_fps_busy() ? 0 : 5; }  // (s137 review S1: greyed while FPS Mode runs)  // cmd 316: state 2 (out) and no Limit object
static int smn_limit_state(void) { return sm_out() ? ((SMD()->fps || SMD()->ohana || SMD()->neverland || SMD()->genie) && sm_fps_can() ? 0 : 5) : (pl_any_member() && !mp_charging() && MP.mp > 0 ? 0 : 5); }  // (s138: FPS Mode / Ohana!; s139: Never Land / Genie's Limits; a row with none is greyed)
static int smn_limit_label(void) { return !sm_out() ? SMN_L_LIMIT : SMD()->fps ? SMN_L_FPS : smx_limit_label() ? smx_limit_label() : SMN_L_OHANA; }  // (s138: Genie / Peter Pan name their own, greyed until built; Stitch = Ohana!, built)

static bool gl_menu(void);
static void gl_hud_prompt(void);
static int gl_switch_state(void);
static bool gl_switch(int v);
static bool smn_slate_row(void) { return (g_sk && g_sk_left) || gl_menu(); }   // (s139 genielim) slot 2 is KH2's Party slot: while Genie is out it is his version menu (cmd 605, exe FUN_1403f3860), else the Slate  // (s137 user) the Slate on page 2 slot 2
static bool smn_fps_pane(void) { return SMN.menu && sm_out() && sm_fps_on(); }

// ---- input (menu_tick)
// d-pad Left on the top page: the page toggle (KH2 toggles page kind 0 / 6). Called from menu_tick's top-page chain after the Slate.
static bool smn_toggle(void) {
    if (!smn_menu_ok()) return false;
    SMN.page2 = true;
    if (M.cursor == 2) M.cursor = 1;
    sora_sfx_event("page", 1);
    LOG("[summon] menu: page 2 (Attack / Summon / Limit)\n");
    return true;
}
static int smn_sub_tick(uint64_t *press) {
    if (!smn_menu_ok()) { M.sub = -1, M.cursor = 0; return -1; }
    if (*press & (BTN_JUMP | BTN_LEFT)) {
        *press &= ~(BTN_JUMP | BTN_LEFT);
        M.sub = -1, M.cursor = SMN.glist ? 2 : 1, SMN.glist = false;
        sora_sfx_event("cancel", 1);
        return -1;
    }
    if (SMN.glist) {   // (s139 genielim) Genie's version list: one row per OWNED form (exe FUN_1403de370; the current one stays in the list)
        int vs[4], nv = 0;
        for (int v = 1; v <= 4; v++) if (smx_ver_owned(v)) vs[nv++] = v;
        if (nv < 1 || !gl_menu()) { M.sub = -1, M.cursor = 1, SMN.glist = false; return -1; }
        if (M.sub_cursor < 0 || M.sub_cursor >= nv) M.sub_cursor = 0;
        if (*press & BTN_UP) M.sub_cursor = (M.sub_cursor + nv - 1) % nv, sora_sfx_event("cursor", 1);
        if (*press & BTN_DOWN) M.sub_cursor = (M.sub_cursor + 1) % nv, sora_sfx_event("cursor", 1);
        if (*press & BTN_ATTACK) {
            *press &= ~BTN_ATTACK;
            if (M.dead > 0 || gl_switch_state() != 0 || !gl_switch(vs[M.sub_cursor])) { sora_sfx_event("error", 1); return -1; }
            M.sub = -1, M.cursor = 0, M.dead = 8, SMN.glist = false;
        }
        return -1;
    }
    int rows[SM_NDEF], nr = smn_owned_rows(rows);
    if (nr < 1) { M.sub = -1, M.cursor = 1; return -1; }
    if (M.sub_cursor < 0 || M.sub_cursor >= nr) M.sub_cursor = 0;
    if (*press & BTN_UP) M.sub_cursor = (M.sub_cursor + nr - 1) % nr, sora_sfx_event("cursor", 1);
    if (*press & BTN_DOWN) M.sub_cursor = (M.sub_cursor + 1) % nr, sora_sfx_event("cursor", 1);
    if (*press & BTN_ATTACK) {
        *press &= ~BTN_ATTACK;
        if (M.dead > 0 || smn_state() != 0) { sora_sfx_event("error", 1); LOG("[summon] menu: %s greyed\n", SM_DEF[rows[M.sub_cursor]].name); return -1; }
        SMN.pick = rows[M.sub_cursor];
        return CMD_SUMMON_CAST;
    }
    return -1;
}
// (s137 review M2) the FPS buttons are read by sm_fps_input (sora_summon_fps.h: A Balls 530, X Firecracker 531, Y End 532 = KH2 table 0x5CC488);
// the pane here only draws them and keeps the rest of the menu out of the way.
// menu_tick, before the top page: returns 1 when handled (ret = the cast code or -1)
static int smn_top_tick(uint64_t *press, int *ret) {
    *ret = -1;
    if (smn_fps_pane()) { M.sub = -1, M.cursor = 0; return 1; }
    if (!smn_page2()) { SMN.page2 = false; return 0; }
    if (M.sub >= 0) return 0;
    int SL[4] = {0, 1, 3, 0}, n = 3;  // (s137 user: the Slate takes page 2's spare slot 2, KH2's Party slot)
    if (smn_slate_row()) SL[0] = 0, SL[1] = 1, SL[2] = 2, SL[3] = 3, n = 4;
    int i = 0;
    for (int k = 0; k < n; k++) if (SL[k] == M.cursor) i = k;
    if (*press & BTN_UP) i = (i + n - 1) % n, M.cursor = SL[i], sora_sfx_event("cursor", 1);
    if (*press & BTN_DOWN) i = (i + 1) % n, M.cursor = SL[i], sora_sfx_event("cursor", 1);
    if (M.cursor != SL[i]) M.cursor = SL[i];
    if ((*press & BTN_LEFT) && !(*press & BTN_RIGHT)) {
        SMN.page2 = false, sora_sfx_event("page", 1);
        LOG("[summon] menu: page 1\n");
        return 1;
    }
    bool dec = (*press & BTN_ATTACK) != 0, go = (*press & BTN_RIGHT) || (dec && M.cursor != 0);
    if (!go) return 1;
    *press &= ~(BTN_ATTACK | BTN_RIGHT);
    if (M.cursor == 2 && gl_menu()) {   // (s139 genielim) Genie's version list
        if (gl_switch_state() == 0) M.sub = SMN_SUB, M.sub_cursor = 0, SMN.glist = true, sora_sfx_event("submenu", 1), LOG("[summon] menu: Genie version list open\n");
        else sora_sfx_event("error", 1);
        return 1;
    }
    if (M.cursor == 2) {  // the Slate page (a submenu: A or Right)
        if (!sk_open_left()) sora_sfx_event("error", 1);
        return 1;
    }
    if (M.cursor == 1) {
        if (sm_out()) {
            if (!dec) return 1;
            if (M.dead > 0 || smn_dismiss_state() != 0) sora_sfx_event("error", 1);
            else *ret = CMD_SUMMON_DISMISS;
        } else if (smn_state() == 0) M.sub = SMN_SUB, M.sub_cursor = 0, sora_sfx_event("submenu", 1), LOG("[summon] menu: Summon list open\n");
        else sora_sfx_event("error", 1), LOG("[summon] menu: Summon greyed\n");
    } else if (M.cursor == 3) {
        if (sm_out()) {
            if (!dec) return 1;
            if (M.dead > 0 || smn_limit_state() != 0) sora_sfx_event("error", 1);
            else *ret = CMD_SUMMON_FPS;
        } else if (!pl_open_left()) sora_sfx_event("error", 1);
    }
    return 1;
}
// the cast codes (sora.c's dispatch)
static void smn_cast(int code) {
    M.sub = -1, M.cursor = 0, M.dead = 8, M.sub_cursor = 0;
    if (code == CMD_SUMMON_CAST) { LOG("[summon] menu: %s cast\n", SM_DEF[SMN.pick].name); sm_request_def(SMN.pick, false); }
    else if (code == CMD_SUMMON_DISMISS) { LOG("[summon] menu: Dismiss\n"); if (SM.st == SM_ACTIVE) SM.req = 3; }
    else if (code == CMD_SUMMON_FPS) { LOG("[summon] menu: %s\n", SMD()->ohana ? "Ohana!" : SMD()->neverland ? "Never Land" : "FPS Mode"); sm_fps_request(); }
}

// ---- HUD
static bool smn_pane(int (*pad)(int, const int8_t *, const int8_t *, const uint8_t *)) {  // hud_update: the FPS pane via the Mickey pane export
    bool on = smn_fps_pane();
    if (on) {
        // (s138 ohana) Stitch: Blast (command_override slot 1 = the reaction / Triangle slot, VERIFIED re:0x3B2850 callers) / Ukulele (Square, English table 0x5CC470) / Shoot (decide)
        const bool oh = SMD()->ohana != 0;
        const int8_t lab[3] = {oh ? SMN_L_BLAST : SMN_L_FIRECRACKER, oh ? SMN_L_UKULELE : SMN_L_END, oh ? SMN_L_SHOOT : SMN_L_BALLS};
        const int8_t btn[3] = {(int8_t)(g_mkpad == 2 ? 43 : 42), (int8_t)(g_mkpad == 2 ? 34 : 31), (int8_t)(g_mkpad == 2 ? 33 : 30)};  // (s137 review M2: Balls = A = Circle / Xbox B icon)
        const uint8_t st[3] = {(uint8_t)(sm_fps_cmd_ok(oh ? 527 : 531) ? 0 : 5), (uint8_t)(sm_fps_cmd_ok(oh ? 526 : 532) ? 0 : 5), (uint8_t)(sm_fps_cmd_ok(oh ? 525 : 530) ? 0 : 5)};
        int8_t lab2[3];
        uint8_t st2[3];
        if (SMD()->neverland) {  // (s139) Never Land: slot 1 (470 / 479) on the Triangle / X button, Stop (482) on Square / Y, slot 0 (471 / 479) on the decide / A button (GUESS slot -> button)
            lab2[0] = (int8_t)nl_label(nl_slot_cmd(1)), lab2[1] = (int8_t)nl_label(482), lab2[2] = (int8_t)nl_label(nl_slot_cmd(0));
            st2[0] = (uint8_t)(nl_cmd_ok(nl_slot_cmd(1)) ? 0 : 5), st2[1] = (uint8_t)(nl_cmd_ok(482) ? 0 : 5), st2[2] = (uint8_t)(nl_cmd_ok(nl_slot_cmd(0)) ? 0 : 5);
        } else memcpy(lab2, lab, 3), memcpy(st2, st, 3);
        int r = pad(3, lab2, btn, st2);
        if (!SMN.pane_on) LOG("[summon] menu: %s pane %s\n", oh ? "Ohana!" : "FPS", !r ? "NOT drawn by the exe (cmd.bin without the s137 labels?)" : oh ? "on (Blast / Ukulele / Shoot)" : "on (Firecracker / End / Balls)");
        SMN.pane_on = true;
    } else if (SMN.pane_on) {
        SMN.pane_on = false;
        pad(0, NULL, NULL, NULL);
        LOG("[summon] menu: FPS pane off\n");
    }
    return on;
}
// (s139 sumplate) the summon's orb plate + face portrait on the HUD (exe sora_hud_summon_face, data/hud/sumface.bin): the id of the row that is out
// (1 Chicken Little, 2 Stitch, 3 Genie, 4 Peter Pan; one id for all of Genie's versions), 0 once the dismiss starts / nothing is out (the plate fades out).
// Looked up by name: an exe from before s139 logs once and goes on. Resent on change (a DLL reload resets last, so it resends). Live "set sface 0" = off.
static int g_sface = 1;
static void smn_face_sync(void) {
    static void (*fn)(int);
    static int tried, last = -1;
    if (!tried) {
        tried = 1, fn = (void (*)(int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_summon_face");
        LOG("[summon] HUD portrait: sora_hud_summon_face %s\n", fn ? "found" : "not in this exe (no orb plate / portrait until the next exe link)");
    }
    int id = g_sface && (SM.st == SM_CALL || SM.st == SM_ACTIVE) ? g_sm_def + 1 : 0;
    if (id == last) return;
    last = id;
    LOG("[summon] hud face id %d\n", id);
    if (fn) fn(id);
}
static void smn_hud(SoraHudMenu *m) {
    smn_face_sync();  // (s139 sumplate: the orb plate portrait)
    bool own = smn_owned();
    if ((int)own != SMN.owned_last) {
        SMN.owned_last = own;
        LOG("[summon] unlock: summons %s\n", own ? "available" : "none owned");
    }
    {
        int mask = 0;
        for (int d = 0; d < SM_NDEF; d++) mask |= smn_owned_def(d) << d;
        if (mask != SMN.owned_mask_last) {
            SMN.owned_mask_last = mask;
            for (int d = 0; d < SM_NDEF; d++)
                LOG("[summon] unlock: %s %s (%s)\n", SM_DEF[d].name, (mask >> d) & 1 ? "OWNED" : "not owned",
                    (d == 0 ? SMN.own : d == 1 ? SMN.own_st : smn_own_x[d]) >= 0 ? "sown override" : smn_unlock_name(SM_DEF[d].unlock));
        }
    }
    gl_hud_prompt();   // (s139 genielim) the stage-2 command in the Triangle slot, whatever page the menu is on
    if (!smn_menu_ok() && SMN.page2 && !g_loading) SMN.page2 = false;
    if (!smn_page2()) {
        SMN.glist = false;
        if (M.sub == SMN_SUB) M.sub = -1;
        return;
    }
    static void (*top)(int, int);
    static int tried;
    if (!tried) tried = 1, top = (void (*)(int, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_top");
    bool out = sm_out();
    if (top) top(0, 0), top(1, out ? SMN_L_DISMISS : SMN_L_SUMMON), top(3, smn_limit_label());
    if (top && gl_menu()) top(2, 58), m->top_state[2] = (uint8_t)gl_switch_state();   // (s139 genielim) cmd.bin label 58 "Genie" (cmd 605's slot)
    else if (top && smn_slate_row()) top(2, SK_PLACEHOLDER_TOP), m->top_state[2] = (uint8_t)sk_top_state();  // (s137 user: the Slate on page 2; same label page 1 shows)
    m->top_state[0] = 0, m->top_state[1] = (uint8_t)(out ? smn_dismiss_state() : smn_state()), m->top_state[3] = (uint8_t)smn_limit_state();
    if (M.sub == SMN_SUB && SMN.glist) {   // (s139 genielim) the versions; (s139 review) as cmd.bin labels 64..67 = KH2's own "Valor Genie" .. "Final Genie" (msgs 17078..17081, in the s138 cmd.bin)
        int nv = 0;
        for (int v = 1; v <= 4; v++) if (smx_ver_owned(v)) m->sub_label[nv] = (uint8_t)(63 + v), m->sub_state[nv] = (uint8_t)gl_switch_state(), m->sub_cost[nv] = 0, nv++;
        m->sub = 2, m->n_sub = nv;
    } else if (M.sub == SMN_SUB) {
        const char *names[SM_NDEF];
        int rows[SM_NDEF], nr = smn_owned_rows(rows);
        for (int i = 0; i < nr; i++) {
            names[i] = SM_DEF[rows[i]].name;
            m->sub_label[i] = 23, m->sub_state[i] = (uint8_t)smn_state(), m->sub_cost[i] = 3;
        }
        m->sub = 2, m->n_sub = nr;
        sub_text_rows(m, names, nr, M.sub_cursor);
    }
}

// the Drive-plate state sora_hud_drive gets while a summon is out: 2 = KH2's summon plate (exe with the s137 drive.bin groups, falls back itself), 1 = the old plain plate
static int smn_gauge_state(void) { return SMN.gauge == 1 ? 1 : 2; }

static bool smn_live_set(const char *key, int v) {
    if (!strcmp(key, "smenu")) {
        SMN.menu = v != 0;
        if (!SMN.menu) SMN.page2 = false, M.sub = M.sub == SMN_SUB ? -1 : M.sub;
        LOG("[summon] smenu %d (%s)\n", SMN.menu, SMN.menu ? "Summon menu entry on" : "live keys only");
    } else if (!strcmp(key, "sown")) SMN.own = v < 0 ? -1 : v != 0, SMN.owned_last = -1, SMN.owned_mask_last = -1;  // Chicken Little: -1 = the paraglider, 0 / 1 = forced
    else if (!strcmp(key, "sownst")) SMN.own_st = v < 0 ? -1 : v != 0, SMN.owned_mask_last = -1;                // Stitch: -1 = Vah Ruta, 0 / 1 = forced
    else if (!strcmp(key, "sowngn")) SMN.owned_mask_last = -1, smn_own_x[2] = v < 0 ? -1 : v != 0;  // Genie: -1 = Vah Naboris, 0 / 1 = forced
    else if (!strcmp(key, "sownpp")) SMN.owned_mask_last = -1, smn_own_x[3] = v < 0 ? -1 : v != 0;  // Peter Pan: -1 = Vah Medoh, 0 / 1 = forced
    else if (!strcmp(key, "sface")) g_sface = v != 0, LOG("[summon] sface %d (orb plate portrait %s)\n", g_sface, g_sface ? "on" : "off");
    else if (!strcmp(key, "sgauge")) SMN.gauge = v < 0 ? 0 : v > 2 ? 2 : v, LOG("[summon] gauge %d (%s)\n", SMN.gauge, SMN.gauge == 1 ? "old Drive-plate fallback" : "KH2 summon plate (needs the s137 exe + drive.bin)");
    else return false;
    return true;
}

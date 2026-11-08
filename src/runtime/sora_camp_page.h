// sora_camp_page.h: the KH2 pause menu top page (camp) + its open / close state machine. Pure: no game calls, the
// glue (sora_camp.h) feeds pause state + pad and acts on CpOut. Host-tested in test/camp_test.c (Task 5 of
// docs/plans/2026-10-30-kh2-pause-menu-slice1.md). Tree = campbake Program.cs Top() (OpenKH camp menu descs).
#pragma once
#include <stdio.h>
#include "sora_camp_seq.h"
#include "sora_abil.h"
#include "sora_cust.h"
#include "sora_kb.h"
#define CP_DB_ROWS 10             // (s140/s141/s146 dbgmenu) Debug page rows, shown top to bottom: Become Sora, Outfit, Auto Outfits, Sora Level, Keyblades, Drive Forms, Summons, All Abilities, God Mode, No Clip
// (s146 outfit) the BACKEND row of each shown row (dbm_set / dbm_state in sora_dbgmenu.h: 0 level, 1 Keyblades, 2 Drive Forms, 3 Summons, 4 All Abilities, 5 God, 6 No Clip, 7 Become Sora, 8 Outfit, 9 Auto Outfits)
static const int CP_DB_ORD[CP_DB_ROWS] = {7, 8, 9, 0, 1, 2, 3, 4, 5, 6};

// (s111 roxmenu) 1 = Roxas is the body (RX.on, set by camp_vals at open): the camp lists ONE member, "Roxas" (KH2's prologue
// menu has no party; GUESS), no Donald / Goofy plates / rows. A literal text wins over msg_key, so his name is a text.
static int g_cp_solo;
static int cp_nm(void) { return g_cp_solo ? 1 : 3; }
static void cp_name_(CsDesc *d, int i) {
    if (g_cp_solo && i == 0) snprintf(d->text, sizeof d->text, "Roxas"), d->msg_key = 0;
    else d->msg_key = 30 + i;
}

// one Status page column set (slice 2): a value < 0 shows "--"
typedef struct { int lv, exp, next, hp, hp_max, mp, mp_max, ap_used, ap_max, drive, bonus_lv, str, mag, def, res[4]; } CpStat;
typedef struct { int munny; int time_s; int lv[3]; int hp[3], hp_max[3], mp[3], mp_max[3]; CpStat st[3];
                 uint16_t ab_id[64]; int ab_n, ab_ap_max; abm_t ab_on; int sc[CUST_SLOTS]; int kb[KB_SLOTS]; uint32_t kb_own; int kb_forms;
                 int sp_hide;  // (s147) bit sp = a spell the player does not have yet: left out of the Customize picker (KH2 lists learned spells; 0 = all)
                 int arm_n; char arm_name[3][24]; int arm_def[3];  // (s120 eqlook) Link's worn armor (Head / Body / Legs) for the Armor rows
                 int db_v[CP_DB_ROWS], db_real_lv;  // (s140 dbgmenu; s146: by BACKEND row) the Debug page's values at open: test level (0 = Off), toggles 0/1, Outfit 0..4; the real level
                 int eq_base_str, eq_base_mag;  // eq_base_* = Sora's Str / Mag without the Keyblade (0 / 0 = use st[0])
                 int wpn_on, wpn_use, wpn_atk, wpn_life, wpn_life_max; char wpn_name[32]; } CpVals;  // (s128 wpnmenu) Link's BotW weapon: wpn_on = equipped + usable here (the picker's top entry), wpn_use = Sora swings it (g_wpn_use), wpn_atk = power + atk_up
// ab_* = Sora's learned abilities (slice 3), sc = his shortcuts (slice 4), kb = his Keyblades (slice 5); 0 Sora 1 Donald 2 Goofy; munny / time_s < 0 = "--"
// kb_own = the owned Keyblades (s89 kb: bit i = KB_TAB[i]; the picker adds KK + Star Seeker + the equipped ones)

enum { CP_CLOSED, CP_WAIT_PAUSE, CP_OPEN, CP_CLOSING, CP_PULSE, CP_HANDOVER, CP_SWITCH };  // SWITCH = page end anims, then the other page
enum { CP_SE_NONE, CP_SE_OPEN, CP_SE_CURSOR, CP_SE_DECIDE, CP_SE_CANCEL, CP_SE_ERROR };

// page 0 = top, 1 = Status, 2 = Abilities, 3 = Customize (who = 0 Sora 1 Donald 2 Goofy); Abilities sub 0 member step,
// 1 list, 2 popup; Customize sub 0 member step, 1 slot list (cu_slot), 2 spell picker (cu_pick 0 Remove, 1 + spell);
// 4 = Equipment: sub 0 member step, 1 slot list (eq_slot = KB_MAIN / KB_VALOR), 2 Keyblade picker (eq_pick = KB_TAB index)
typedef struct { int state, row, timer, held, layout_l2, layout_f, page, who, next, nroot, sub, ab_row, ab_top, pop_row, cu_slot, cu_pick, eq_slot, eq_pick;
                 int eq_wpnrow;  // (s128 wpnmenu) 1 = the Weapons row shows Link's BotW weapon (set before each equip tree build)
                 int eq_top;  // (s89 kb) the picker's first shown entry (owned list position)
                 int db_row, db_top, db_v[CP_DB_ROWS], db_rebuild, db_rep;  // (s140 dbgmenu; s146: db_row = SHOWN row, db_v by BACKEND row) the Debug page: cursor row, the values (test level 0 = Off, toggles, Outfit), rebuild asked by the glue, hold-repeat count
                 uint64_t held_btn;  // (s140) the buttons held this present (set by the glue before cp_tick; the Debug page's hold-to-repeat)
                 abm_t ab_on; int sc[CUST_SLOTS]; int kb[KB_SLOTS]; CsTree tree; int root[7]; } CampPage;  // zero = CLOSED
// ab_set: the page changed Sora's abilities to ab_on (ab_id = the one toggled); sc_set: shortcut sc_slot <- sc_spell;
// kb_set: Keyblade slot kb_slot <- item id kb_id
typedef struct { uint8_t db_set /* (s140) Debug page: BACKEND row + 1 changed */; int db_val; uint8_t eat, pulse_plus, pass_plus, draw, se, ab_set, sc_set, kb_set, wpn_set /* (s128) 1 = BotW weapon picked, 2 = a Keyblade picked in the Weapons slot */, kb_swap /* (s118) other slot + 1 */; uint16_t ab_id; abm_t ab_on; int sc_slot, sc_spell, kb_slot, kb_id;
                 int log_row; const char *log; } CpOut;  // log_row -1 = none

#define CP_ROWS 6
#define CP_ROW_EQUIP 0            // "Equipment": opens the Equipment page (slice 5)
#define CP_ROW_ABIL 1             // "Abilities": opens the Abilities page (slice 3)
#define CP_ROW_CUST 2             // "Customize": opens the Customize page (slice 4)
#define CP_ROW_STATUS 3           // "Status": opens the Status page (slice 2)
#define CP_ROW_BOTW 4             // "BotW Menu": hands the pause over to BotW's own menu
#define CP_ROW_DEBUG 5            // (s140 dbgmenu) "Debug Menu": opens the Debug page (page 5): test level + unlock-all toggles
#define CP_BTN_LEFT (1ull << 12)  // = sora.c BTN_LEFT / BTN_RIGHT (d-pad)
#define CP_BTN_RIGHT (1ull << 14)
#define CP_OPEN_WAIT 30           // presents to wait for BotW's pause (state 2) after the + edge
#define CP_PULSE_WAIT 60          // presents of + pulses before giving up on BotW unpausing
#define CP_SWITCH_MAX 120         // presents of page end anims before switching anyway (guard)
#define CP_REPEAT_FIRST 18        // cursor repeat (GUESS: KH2 feel)
#define CP_REPEAT_NEXT 6
#define CP_BTN_A (1ull << 0)      // = sora.c BTN_ATTACK
#define CP_BTN_B (1ull << 1)      // = BTN_JUMP
#define CP_BTN_X (1ull << 2)      // Switch X = KH2 Triangle: the Abilities instant toggle (GUESS: pad bit 0x1000)
#define CP_BTN_ZL (1ull << 8)     // KH2 L2 / R2: Abilities "Switch" member
#define CP_BTN_ZR (1ull << 9)
#define CP_BTN_PLUS (1ull << 10)
#define CP_BTN_UP (1ull << 13)
#define CP_BTN_DOWN (1ull << 15)

#define CP_SEQ_(s_, l_, e_) .start = (s_), .loop = (l_), .end = (e_)
static void cp_num_(char *b, int n, const char *fmt, int v) { if (v < 0) snprintf(b, n, "--"); else snprintf(b, n, fmt, v); }

// the Munny / Time states (107-115) shared by the top and Status pages; kids point into k_munny / k_time
static void cp_bg_state_(CsDesc k_munny[1], CsDesc k_time[1], CsDesc bg_state[3], const CpVals *v) {
    k_munny[0] = (CsDesc){ CP_SEQ_(-1, 124, -1), .anchor = CS_BOTTOM_RIGHT };
    cp_num_(k_munny[0].text, sizeof k_munny[0].text, "%d", v->munny);
    k_time[0] = (CsDesc){ CP_SEQ_(-1, 124, -1), .anchor = CS_BOTTOM_RIGHT };
    if (v->time_s < 0) snprintf(k_time[0].text, sizeof k_time[0].text, "--");
    else snprintf(k_time[0].text, sizeof k_time[0].text, "%d:%02d", v->time_s / 3600, v->time_s / 60 % 60);  // KH2: h:mm
    bg_state[0] = (CsDesc){ CP_SEQ_(107, 108, 109), .nkids = 1, .kids = k_munny };
    bg_state[1] = (CsDesc){ CP_SEQ_(110, 111, 112) };
    bg_state[2] = (CsDesc){ CP_SEQ_(113, 114, 115), .nkids = 1, .kids = k_time };
}

// the top page as campbake Top() builds it; r = the 5 roots (bg states, menu bg, rows, party, help). Descs are copied
// into the tree, so locals are fine. -1 in r = pool full.
static void cp_build_top(CsTree *t, int row, const CpVals *v, int r[5]) {
    CsDesc k_munny[1], k_time[1], bg_state[3], bg_menu[1], sel_kids[2], menu[CP_ROWS], help[1];
    CsDesc ch_stats[3][8], ch_frame[3][2], ch_box[3][1], chars[3];
    cs_tree_clear(t);
    cp_bg_state_(k_munny, k_time, bg_state, v);
    bg_menu[0] = (CsDesc){ CP_SEQ_(46, 47, 48) };
    sel_kids[0] = (CsDesc){ CP_SEQ_(-1, 25, -1), .anchor = CS_BOTTOM_LEFT, .flags = CS_A_NO_CHILD_TX };
    sel_kids[1] = (CsDesc){ CP_SEQ_(27, 28, 29) };
    static const int row_key[CP_ROWS] = { 0, 2, 1, 3, 0, 0 };  // camp_text.txt: Abilities 0x044d, Customize 0x0451, Status 0x044f
    static const char *row_lit[CP_ROWS] = { "Equipment", "", "", "", "BotW Menu", "Debug Menu" };
    for (int i = 0; i < CP_ROWS; i++) {
        if (i != row) menu[i] = (CsDesc){ CP_SEQ_(133, 134, 135), .stack_index = (int16_t)i, .stack_w = 0, .stack_h = CS_DEFAULT_STACK, .flags = CS_A_TEXT_TX };
        else menu[i] = (CsDesc){ CP_SEQ_(-1, 132, 135), .stack_index = (int16_t)i, .stack_w = 0, .stack_h = CS_DEFAULT_STACK,
                                 .flags = CS_A_TEXT_TX | CS_A_CHILD_STACK_H, .nkids = 2, .kids = sel_kids };
        menu[i].msg_key = row_key[i];
        snprintf(menu[i].text, sizeof menu[i].text, "%s", row_lit[i]);
    }
    help[0] = (CsDesc){ .x = 150, .y = 352, CP_SEQ_(-1, 124, -1), .flags = CS_A_TEXT_IGNORE_COLOR, .anchor = CS_BOTTOM_LEFT, .msg_key = 20 + row };
    if (row == CP_ROW_DEBUG) help[0].msg_key = 0, snprintf(help[0].text, sizeof help[0].text, "Test tools (not saved)");  // (s140; a literal wins)
    for (int i = 0; i < cp_nm(); i++) {
        CsDesc *st = ch_stats[i];
        st[0] = (CsDesc){ CP_SEQ_(-1, 124, -1), .msg_key = 40, .anchor = CS_BOTTOM_LEFT };
        st[1] = (CsDesc){ CP_SEQ_(-1, 124, -1), .anchor = CS_BOTTOM_RIGHT };
        cp_num_(st[1].text, sizeof st[1].text, "%d", v->lv[i]);
        st[2] = (CsDesc){ .stack_index = 1, CP_SEQ_(-1, 121, -1), .msg_key = 41, .anchor = CS_BOTTOM_LEFT };
        st[3] = (CsDesc){ .stack_index = 1, CP_SEQ_(-1, 121, -1), .anchor = CS_BOTTOM_RIGHT };
        snprintf(st[3].text, sizeof st[3].text, "%d/%d", v->hp[i], v->hp_max[i]);
        st[4] = (CsDesc){ .stack_index = 1, CP_SEQ_(-1, 98, -1), .anchor = CS_BOTTOM_LEFT };
        st[5] = (CsDesc){ .stack_index = 2, CP_SEQ_(-1, 118, -1), .msg_key = 42, .anchor = CS_BOTTOM_LEFT };
        st[6] = (CsDesc){ .stack_index = 2, CP_SEQ_(-1, 118, -1), .anchor = CS_BOTTOM_RIGHT };
        snprintf(st[6].text, sizeof st[6].text, "%d/%d", v->mp[i], v->mp_max[i]);
        st[7] = (CsDesc){ .stack_index = 2, CP_SEQ_(-1, 99, -1), .anchor = CS_BOTTOM_LEFT };
        ch_frame[i][0] = (CsDesc){ CP_SEQ_(-1, 124, -1), .msg_key = 30 + i, .flags = CS_A_TEXT_IGNORE_COLOR, .anchor = CS_BOTTOM_CENTER };
        cp_name_(&ch_frame[i][0], i);
        ch_frame[i][1] = (CsDesc){ CP_SEQ_(-1, 90, -1), .nkids = 8, .kids = st };
        ch_box[i][0] = (CsDesc){ CP_SEQ_(-1, 93, -1), .nkids = 2, .kids = ch_frame[i] };
        chars[i] = (CsDesc){ CP_SEQ_(101, 102, 103), .stack_index = (int16_t)(i + 1), .stack_w = CS_DEFAULT_STACK, .nkids = 1, .kids = ch_box[i] };
    }
    r[0] = cs_tree_add(t, bg_state, 3); r[1] = cs_tree_add(t, bg_menu, 1); r[2] = cs_tree_add(t, menu, CP_ROWS);
    r[3] = cs_tree_add(t, chars, cp_nm()); r[4] = cs_tree_add(t, help, 1);
}

#include "sora_camp_status.h"
#include "sora_camp_abil.h"
#include "sora_camp_cust.h"
#include "sora_camp_equip.h"
static int cp_build_dbg(CsTree *t, const CampPage *p, const CpVals *v, int r[4]);  // (s140, sora_camp_dbg.h below cp_equip_input_)

static int cp_nroot_(const CampPage *p) { return p->nroot; }  // top 5 roots, Status 4, Abilities 5 / 6 / 7, Customize + Equipment 4 / 4 / 5
static bool cp_built_(const CampPage *p) { for (int k = 0; k < cp_nroot_(p); k++) if (p->root[k] < 0) return false; return true; }
static void cp_build_(CampPage *p, const CpVals *v) {
    if (p->page == 5) cp_build_dbg(&p->tree, p, v, p->root), p->nroot = 4;
    else if (p->page == 4 && p->sub == 0) cp_build_equip_member(&p->tree, p->who, v, p->root), p->nroot = 4;
    else if (p->page == 4) p->eq_wpnrow = cp_eq_wpn_row(v), p->nroot = cp_build_equip_list(&p->tree, p->kb, v, p->eq_slot, p->sub == 2 ? p->eq_pick : -1, p->eq_top, p->root);
    else if (p->page == 3 && p->sub == 0) cp_build_cust_member(&p->tree, p->who, v, p->root), p->nroot = 4;
    else if (p->page == 3) p->nroot = cp_build_cust_list(&p->tree, p->sc, v, p->cu_slot, p->sub == 2 ? p->cu_pick : -1, p->root);
    else if (p->page == 2 && p->sub == 0) cp_build_abil_member(&p->tree, p->who, v, p->ab_on, p->root), p->nroot = 5;
    else if (p->page == 2) p->nroot = cp_build_abil_list(&p->tree, p->who, v, p->ab_on, p->ab_row, p->ab_top, p->sub == 2 ? p->pop_row : -1, p->root);
    else if (p->page) cp_build_status(&p->tree, p->who, &v->st[p->who], v, p->root), p->nroot = 4;
    else cp_build_top(&p->tree, p->row, v, p->root), p->nroot = 5;
}

// slice 1 backdrop: one solid opaque black quad over any aspect (spec 3.3: BotW's own pause screen stays hidden)
static int g_cp_split;  // (s94) quads before the top page's name plates (root 3) in the last draw; 0 = none
static int g_cp_nobackdrop;  // (s92 b6-menuwalk) 1 = the actors behind the menu show: no black quad in the open / steady / closing frames
static void cp_backdrop_(CsOut *o) {
    if (!o || o->n >= o->cap) return;
    SoraHudQuad *q = &o->q[o->n++];
    memset(q, 0, sizeof *q);
    const float X0 = -2048, X1 = 2560, Y0 = -1024, Y1 = 1440;
    q->x[0] = X0; q->y[0] = Y0; q->x[1] = X1; q->y[1] = Y0; q->x[2] = X0; q->y[2] = Y1; q->x[3] = X1; q->y[3] = Y1;
    for (int k = 0; k < 4; k++) q->col[k] = 0x80000000u;
    q->tex = 2;
}

static void cp_out_clear_(CpOut *out) { memset(out, 0, sizeof *out); out->log_row = -1; }

// the + edge from the game: opens only from a running, unpaused world
static void cp_request(CampPage *p, uint32_t pause_state, bool world_ok) {
    if (p->state != CP_CLOSED || pause_state != 0 || !world_ok) return;
    p->state = CP_WAIT_PAUSE; p->row = 0; p->timer = 0; p->held = 0; p->layout_l2 = 0; p->layout_f = 0; p->page = 0; p->who = 0;
    p->sub = 0; p->nroot = 5; p->root[0] = -1;  // built on the first tick (needs the values)
}

static void cp_abort(CampPage *p, const char *why, CpOut *out) {
    p->state = CP_CLOSED; p->timer = 0;
    if (out) { out->eat = out->draw = out->pass_plus = out->pulse_plus = 0; out->log = why; }
}

// = run_top's per-frame step, in two halves: Update (not on the first frame) + the layout L2 0 -> 1 switch, then the
// draw (Draw moves the state on); a close lands between them, as run_top's close frame
static void cp_update_(CampPage *p, const CampSeq *s, bool first) {
    if (!first) { p->layout_f++; for (int k = 0; k < cp_nroot_(p); k++) cs_update(&p->tree, p->root[k], 1 / 60.0); }
    if (p->layout_l2 == 0 && cs_layout_last(s, 0, p->layout_f)) { p->layout_f = 0; p->layout_l2 = 1; }
}
static void cp_draw_(CampPage *p, const CampSeq *s, CsOut *o) {
    if (!g_cp_nobackdrop) cp_backdrop_(o);
    cs_draw_layout(s, o, p->layout_l2, p->layout_f);
    g_cp_split = 0;
    for (int k = 0; k < cp_nroot_(p); k++) {
        if (p->page == 0 && k == 3 && o) g_cp_split = o->n;  // (s94: backdrop + layout + munny/time + menu rows go under the party)
        cs_draw(&p->tree, s, o, p->root[k], 0, 0);
    }
}

// stick / d-pad -> -1 up, +1 down, 0 none; the d-pad is edges (press), the stick repeats while held
static int cp_dir_(CampPage *p, uint64_t press, float stick_y) {
    int d = (press & CP_BTN_UP) ? -1 : (press & CP_BTN_DOWN) ? 1 : 0;
    if (d) { p->held = 0; return d; }
    int sd = stick_y > 0.5f ? -1 : stick_y < -0.5f ? 1 : 0;
    if (!sd) { p->held = 0; return 0; }
    int h = p->held++;
    if (h == 0) return sd;
    if (h >= CP_REPEAT_FIRST && (h - CP_REPEAT_FIRST) % CP_REPEAT_NEXT == 0) return sd;
    return 0;
}

// rebuild for a new row (top) / who (Status), already past the open animations (cs_skip: start -> loop)
static void cp_set_row_(CampPage *p, const CampSeq *s, int row, const CpVals *v) {
    if (p->page) p->who = row; else p->row = row;
    if (p->page == 2 && p->sub) p->who = 0;  // (the list / popup reuse this for a rebuild in place)
    cp_build_(p, v);
    if (!cp_built_(p)) return;
    for (int k = 0; k < cp_nroot_(p); k++) { cs_begin(&p->tree, p->root[k]); cs_skip(&p->tree, p->root[k]); }
    for (int k = 0; k < cp_nroot_(p); k++) cs_draw(&p->tree, s, NULL, p->root[k], 0, 0);  // settle state, no quads
}
// top <-> Status: the current page's end anims, then the other page from its start anims (CP_SWITCH)
static void cp_switch_(CampPage *p, int next) {
    p->state = CP_SWITCH; p->timer = 0; p->held = 0; p->next = next;
    for (int k = 0; k < cp_nroot_(p); k++) cs_end(&p->tree, p->root[k]);
}

// ---- the Abilities page (slice 3): S79 helper section 2 ----
static int cp_ab_sel_(const CpVals *v, int row) {  // the id on list row (display order), 0 = none
    uint16_t id[AB_N];
    int n = v->ab_n < AB_N ? v->ab_n : AB_N;
    for (int i = 0; i < n; i++) id[i] = v->ab_id[i];
    cp_ab_sort(id, n);
    return row >= 0 && row < n ? id[row] : 0;
}
// a sub-state change: rebuild; bg (root 0) and, unless fresh, the list's own roots skip their start anims (the popup,
// root 6, plays its own)
static void cp_abil_go_(CampPage *p, const CampSeq *s, const CpVals *v, int sub, bool fresh) {
    p->sub = sub;
    cp_build_(p, v);
    if (!cp_built_(p)) return;
    for (int k = 0; k < cp_nroot_(p); k++) {
        cs_begin(&p->tree, p->root[k]);
        if (k == 0 || (!fresh && k < 6)) cs_skip(&p->tree, p->root[k]);
    }
    for (int k = 0; k < cp_nroot_(p); k++) cs_draw(&p->tree, s, NULL, p->root[k], 0, 0);
}
static void cp_ab_apply_(CampPage *p, const CpVals *v, int id, bool want, CpOut *out) {
    if (ab_on(p->ab_on, id) == want) return;
    if (ab_toggle(&p->ab_on, ab_mask(v->ab_id, v->ab_n), id, v->ab_ap_max) < 0) return;
    out->ab_set = 1; out->ab_id = (uint16_t)id; out->ab_on = p->ab_on;
}
// layout up, + handled by the caller. A on Donald / Goofy = error (GUESS: no party abilities yet); L2 / R2 = error
// (one member row with a list); up / down clamp in the list and popup (GUESS)
static void cp_abil_input_(CampPage *p, const CampSeq *s, uint64_t press, float stick_y, const CpVals *v, CpOut *out) {
    if (p->sub == 0) {
        if (press & CP_BTN_B) { cp_switch_(p, 0); out->se = CP_SE_CANCEL; out->log = "abilities back"; return; }
        if (press & CP_BTN_A) {
            if (p->who != 0) { out->se = CP_SE_ERROR; return; }
            p->ab_row = p->ab_top = 0;
            cp_abil_go_(p, s, v, 1, true);
            out->se = CP_SE_DECIDE; out->log = "abil list";
            return;
        }
        int d = cp_dir_(p, press, stick_y);
        if (d) { cp_set_row_(p, s, (p->who + d + cp_nm()) % cp_nm(), v); out->se = CP_SE_CURSOR; out->log_row = p->who; }
        return;
    }
    int id = cp_ab_sel_(v, p->ab_row);
    bool ok = id && cp_ab_cost(id) && cp_ab_fits(v, p->ab_on, id);
    if (p->sub == 1) {
        if (press & CP_BTN_B) { cp_abil_go_(p, s, v, 0, true); out->se = CP_SE_CANCEL; out->log = "abil list back"; return; }
        if (press & (CP_BTN_ZL | CP_BTN_ZR)) { out->se = CP_SE_ERROR; return; }
        if (press & (CP_BTN_A | CP_BTN_X)) {
            if (!ok) { out->se = CP_SE_ERROR; return; }
            if (press & CP_BTN_A) { p->pop_row = ab_on(p->ab_on, id) ? 1 : 0; cp_abil_go_(p, s, v, 2, false); out->log = "abil popup"; }
            else { cp_ab_apply_(p, v, id, !ab_on(p->ab_on, id), out); cp_abil_go_(p, s, v, 1, false); out->log = "abil toggle"; }
            out->se = CP_SE_DECIDE;
            return;
        }
        int d = cp_dir_(p, press, stick_y), r = p->ab_row + d;
        if (!d || r < 0 || r >= v->ab_n) return;
        p->ab_row = r;
        if (r < p->ab_top) p->ab_top = r;
        if (r >= p->ab_top + CP_AB_VIS) p->ab_top = r - CP_AB_VIS + 1;
        cp_abil_go_(p, s, v, 1, false);
        out->se = CP_SE_CURSOR; out->log_row = r;
        return;
    }
    if (press & CP_BTN_B) { cp_abil_go_(p, s, v, 1, false); out->se = CP_SE_CANCEL; out->log = "abil popup back"; return; }
    if (press & CP_BTN_A) {
        cp_ab_apply_(p, v, id, p->pop_row == 1, out);
        cp_abil_go_(p, s, v, 1, false);
        out->se = CP_SE_DECIDE; out->log = "abil set";
        return;
    }
    int d = cp_dir_(p, press, stick_y), r = p->pop_row + d;
    if (!d || r < 0 || r > 1) return;
    p->pop_row = r;
    cp_abil_go_(p, s, v, 2, false);
    out->se = CP_SE_CURSOR; out->log_row = r;
}

// ---- the Customize page (slice 4): recon/S84_CUST_PAGE.md ----
// a sub-state change: rebuild; bg (root 0) and, unless fresh, the list's own roots skip their start anims (the picker,
// root 4, plays its own)
static void cp_cust_go_(CampPage *p, const CampSeq *s, const CpVals *v, int sub, bool fresh) {
    bool keep4 = !fresh && p->page == 4 && g_eqlook && p->sub == sub;  // (s120 review) a row move: the Equipment panel (root 4) stays open
    p->sub = sub;
    cp_build_(p, v);
    if (!cp_built_(p)) return;
    for (int k = 0; k < cp_nroot_(p); k++) {
        cs_begin(&p->tree, p->root[k]);
        if (k == 0 || (!fresh && k < 4) || (keep4 && k == 4)) cs_skip(&p->tree, p->root[k]);
    }
    for (int k = 0; k < cp_nroot_(p); k++) cs_draw(&p->tree, s, NULL, p->root[k], 0, 0);
}
// layout up, + handled by the caller. A on Donald / Goofy = error (user scope: Sora's shortcuts only; KH2 opens 0x1b);
// L2 / R2 = error (one member with a list); up / down clamp in the list and picker (GUESS, as slice 3). A on a picker
// entry = cust_set (duplicates allowed), back to the list on the same slot
static void cp_cust_input_(CampPage *p, const CampSeq *s, uint64_t press, float stick_y, const CpVals *v, CpOut *out) {
    if (p->sub == 0) {
        if (press & CP_BTN_B) { cp_switch_(p, 0); out->se = CP_SE_CANCEL; out->log = "customize back"; return; }
        if (press & CP_BTN_A) {
            if (p->who != 0) { out->se = CP_SE_ERROR; return; }
            p->cu_slot = 0;
            cp_cust_go_(p, s, v, 1, true);
            out->se = CP_SE_DECIDE; out->log = "cust list";
            return;
        }
        int d = cp_dir_(p, press, stick_y);
        if (d) { p->who = (p->who + d + cp_nm()) % cp_nm(); cp_cust_go_(p, s, v, 0, false); out->se = CP_SE_CURSOR; out->log_row = p->who; }
        return;
    }
    if (p->sub == 1) {
        if (press & CP_BTN_B) { cp_cust_go_(p, s, v, 0, true); out->se = CP_SE_CANCEL; out->log = "cust list back"; return; }
        if (press & (CP_BTN_ZL | CP_BTN_ZR)) { out->se = CP_SE_ERROR; return; }
        if (press & CP_BTN_A) {
            p->cu_pick = cp_cu_row_of_(v, p->sc[p->cu_slot]);  // empty (-1) / a spell not listed -> Remove
            cp_cust_go_(p, s, v, 2, false);
            out->se = CP_SE_DECIDE; out->log = "cust picker";
            return;
        }
        int d = cp_dir_(p, press, stick_y), r = p->cu_slot + d;
        if (!d || r < 0 || r >= CUST_SLOTS) return;
        p->cu_slot = r;
        cp_cust_go_(p, s, v, 1, false);
        out->se = CP_SE_CURSOR; out->log_row = r;
        return;
    }
    if (press & CP_BTN_B) { cp_cust_go_(p, s, v, 1, false); out->se = CP_SE_CANCEL; out->log = "cust picker back"; return; }
    if (press & CP_BTN_A) {
        int sl[CUST_NSPELL], sp = p->cu_pick > 0 && p->cu_pick <= cp_cu_spells_(v, sl) ? sl[p->cu_pick - 1] : CUST_EMPTY;
        if (cust_set(p->sc, p->cu_slot, sp) > 0) { out->sc_set = 1; out->sc_slot = p->cu_slot; out->sc_spell = sp; }
        cp_cust_go_(p, s, v, 1, false);
        out->se = CP_SE_DECIDE; out->log = "cust set";
        return;
    }
    int sl[CUST_NSPELL], d = cp_dir_(p, press, stick_y), r = p->cu_pick + d;
    if (!d || r < 0 || r > cp_cu_spells_(v, sl)) return;
    p->cu_pick = r;
    cp_cust_go_(p, s, v, 2, false);
    out->se = CP_SE_CURSOR; out->log_row = r;
}

// ---- the Equipment page (slice 5): Customize's flow (cp_cust_go_ rebuilds any page 3 / 4 sub-state) ----
// A on Donald / Goofy = error (Sora only); L2 / R2 = error; up / down clamp. A on a picker entry = kb_set (one copy: the
// other slot's Keyblade = error SE, the picker stays), back to the list on the same slot
static void cp_equip_input_(CampPage *p, const CampSeq *s, uint64_t press, float stick_y, const CpVals *v, CpOut *out) {
    if (p->sub == 0) {
        if (press & CP_BTN_B) { cp_switch_(p, 0); out->se = CP_SE_CANCEL; out->log = "equipment back"; return; }
        if (press & CP_BTN_A) {
            if (p->who != 0) { out->se = CP_SE_ERROR; return; }
            p->eq_slot = KB_MAIN;
            cp_cust_go_(p, s, v, 1, true);
            out->se = CP_SE_DECIDE; out->log = "equip list";
            return;
        }
        int d = cp_dir_(p, press, stick_y);
        if (d) { p->who = (p->who + d + cp_nm()) % cp_nm(); cp_cust_go_(p, s, v, 0, false); out->se = CP_SE_CURSOR; out->log_row = p->who; }
        return;
    }
    if (p->sub == 1) {
        if (press & CP_BTN_B) { cp_cust_go_(p, s, v, 0, true); out->se = CP_SE_CANCEL; out->log = "equip list back"; return; }
        if (press & (CP_BTN_ZL | CP_BTN_ZR)) { out->se = CP_SE_ERROR; return; }
        if ((press & CP_BTN_A) && p->eq_slot >= CP_EQ_ARM) { out->se = CP_SE_ERROR; out->log = "equip armor"; return; }  // (s120) show only
        if (press & CP_BTN_A) {
            int i = kb_index(kb_slot_id(p->kb, p->eq_slot)), l[KB_N + 1], n = cp_eq_plist(v, p->kb, p->eq_slot, l), e;
            p->eq_pick = cp_eq_has_wpn(v, p->eq_slot) && v->wpn_use ? CP_EQ_WPN : i < 0 ? 0 : i;  // (s128 wpnmenu) the cursor starts on what is in the hand
            e = cp_eq_pos(l, n, p->eq_pick);  // (s89 kb) scroll so the slot's Keyblade shows
            p->eq_top = e < CP_EQ_VIS ? 0 : e - CP_EQ_VIS + 1;
            cp_cust_go_(p, s, v, 2, false);
            out->se = CP_SE_DECIDE; out->log = "equip picker";
            return;
        }
        int vs[CP_EQ_ROWS_MAX], nv = cp_eq_rows(v, vs), vi = 0;  // (s91 mfkb: over the visible rows; s120: + armor rows)
        while (vi < nv - 1 && vs[vi] != p->eq_slot) vi++;
        int d = cp_dir_(p, press, stick_y), r = vi + d;
        if (!d || r < 0 || r >= nv) return;
        p->eq_slot = vs[r];
        cp_cust_go_(p, s, v, 1, false);
        out->se = CP_SE_CURSOR; out->log_row = r;
        return;
    }
    if (press & CP_BTN_B) { cp_cust_go_(p, s, v, 1, false); out->se = CP_SE_CANCEL; out->log = "equip picker back"; return; }
    if ((press & CP_BTN_A) && (p->eq_slot < 0 || p->eq_slot >= KB_SLOTS)) { out->se = CP_SE_ERROR; out->log = "equip bad slot"; return; }  // (s120 review) never kb_set an armor row
    if ((press & CP_BTN_A) && p->eq_pick == CP_EQ_WPN) {  // (s128 wpnmenu) the BotW weapon entry
        CpVals nv = *v;
        nv.wpn_use = 1;
        out->wpn_set = 1;
        cp_cust_go_(p, s, &nv, 1, false);
        out->se = CP_SE_DECIDE; out->log = "equip botw weapon";
        return;
    }
    if (press & CP_BTN_A) {
        int id = KB_TAB[p->eq_pick].id;
        CpVals nv = *v;  // (s128 wpnmenu) a Keyblade picked in the Weapons slot = the Keyblade (wpn_use 0)
        int kbw = cp_eq_has_wpn(v, p->eq_slot);  // (s128 review: eqlook 0 has no weapon entry = never switches it off)
        nv.wpn_use = kbw ? 0 : v->wpn_use;
        if (!kb_offer(p->kb, p->eq_slot, id)) {  // (s118 kbswap: another slot holds it -> swap the two)
            int o = kb_holder(p->kb, p->eq_slot, id);
            if (kb_swap(p->kb, p->eq_slot, id) < 0) { out->se = CP_SE_ERROR; out->log = "equip one-copy"; return; }
            out->kb_set = 1, out->kb_slot = p->eq_slot, out->kb_id = id, out->kb_swap = (uint8_t)(o + 1);
            if (kbw) out->wpn_set = 2;
            cp_cust_go_(p, s, &nv, 1, false);
            out->se = CP_SE_DECIDE; out->log = "equip swap";
            return;
        }
        if (kb_set(p->kb, p->eq_slot, id) > 0) { out->kb_set = 1; out->kb_slot = p->eq_slot; out->kb_id = id; }
        if (kbw) out->wpn_set = 2;
        cp_cust_go_(p, s, &nv, 1, false);
        out->se = CP_SE_DECIDE; out->log = "equip set";
        return;
    }
    int l[KB_N + 1], n = cp_eq_plist(v, p->kb, p->eq_slot, l), d = cp_dir_(p, press, stick_y), e = cp_eq_pos(l, n, p->eq_pick) + d, r;
    if (!d || e < 0 || e >= n) return;  // (s89 kb) moves over the owned list, clamped
    p->eq_pick = r = l[e];
    if (e < p->eq_top) p->eq_top = e;
    if (e >= p->eq_top + CP_EQ_VIS) p->eq_top = e - CP_EQ_VIS + 1;
    cp_cust_go_(p, s, v, 2, false);
    out->se = CP_SE_CURSOR; out->log_row = r;
}

#include "sora_camp_dbg.h"

// one present
static void cp_tick(CampPage *p, const CampSeq *s, CsOut *o, uint32_t pause_state, uint64_t press, float stick_y,
                    const CpVals *v, CpOut *out) {
    cp_out_clear_(out);
    switch (p->state) {
    case CP_CLOSED: return;
    case CP_HANDOVER:
        if (pause_state == 0) { p->state = CP_CLOSED; out->log = "closed"; }
        return;
    case CP_WAIT_PAUSE: {
        bool first = p->timer == 0;
        if (first) {
            cp_build_(p, v);
            if (!cp_built_(p)) { cp_abort(p, "abort tree-full", out); return; }
            for (int k = 0; k < cp_nroot_(p); k++) cs_begin(&p->tree, p->root[k]);
            out->se = CP_SE_OPEN; out->log = "open"; out->pass_plus = 1;
        }
        if (pause_state == 2) p->state = CP_OPEN;
        else if (p->timer >= CP_OPEN_WAIT) { cp_abort(p, "abort no-pause", out); return; }
        p->timer++;
        out->eat = out->draw = 1;
        cp_update_(p, s, first);
        cp_draw_(p, s, o);
        return;
    }
    case CP_OPEN: {
        out->eat = out->draw = 1;
        cp_update_(p, s, false);
        if (p->layout_l2 == 1) {
            if (p->page >= 2 && !(press & CP_BTN_PLUS)) {
                if (p->page == 5) cp_dbg_input_(p, s, press, stick_y, v, out);
                else if (p->page == 2) cp_abil_input_(p, s, press, stick_y, v, out);
                else if (p->page == 4) cp_equip_input_(p, s, press, stick_y, v, out);
                else cp_cust_input_(p, s, press, stick_y, v, out);
                if (!cp_built_(p)) { cp_abort(p, "abort tree-full", out); return; }
                cp_draw_(p, s, o);
                return;
            }
            if (p->page && (press & CP_BTN_B) && !(press & CP_BTN_PLUS)) {  // Status: B = back to the top page
                cp_switch_(p, 0);
                out->se = CP_SE_CANCEL; out->log = "status back";
                cp_draw_(p, s, o);
                return;
            }
            if (press & (CP_BTN_B | CP_BTN_PLUS)) {  // B wins over A
                p->state = CP_CLOSING; p->timer = 0;
                out->se = CP_SE_CANCEL; out->log = "close";
                for (int k = 0; k < cp_nroot_(p); k++) cs_end(&p->tree, p->root[k]);
                p->layout_l2 = 2; p->layout_f = 0;
                cp_draw_(p, s, o);
                return;
            }
            if ((press & CP_BTN_A) && p->page) {  // nothing to pick on the Status page yet
            } else if (press & CP_BTN_A) {
                if (p->row == CP_ROW_ABIL) {
                    p->ab_on = v->ab_on;
                    cp_switch_(p, 2);
                    out->se = CP_SE_DECIDE; out->log = "abilities";
                    cp_draw_(p, s, o);
                    return;
                }
                if (p->row == CP_ROW_EQUIP) {
                    memcpy(p->kb, v->kb, sizeof p->kb);
                    cp_switch_(p, 4);
                    out->se = CP_SE_DECIDE; out->log = "equipment";
                    cp_draw_(p, s, o);
                    return;
                }
                if (p->row == CP_ROW_CUST) {
                    memcpy(p->sc, v->sc, sizeof p->sc);
                    cp_switch_(p, 3);
                    out->se = CP_SE_DECIDE; out->log = "customize";
                    cp_draw_(p, s, o);
                    return;
                }
                if (p->row == CP_ROW_STATUS) {
                    cp_switch_(p, 1);
                    out->se = CP_SE_DECIDE; out->log = "status";
                    cp_draw_(p, s, o);
                    return;
                }
                if (p->row == CP_ROW_DEBUG) {  // (s140 dbgmenu)
                    memcpy(p->db_v, v->db_v, sizeof p->db_v), p->db_row = 0, p->db_top = 0, p->db_rep = 0;
                    cp_switch_(p, 5);
                    out->se = CP_SE_DECIDE; out->log = "debug menu";
                    cp_draw_(p, s, o);
                    return;
                }
                if (p->row == CP_ROW_BOTW) {
                    p->state = CP_HANDOVER; out->se = CP_SE_DECIDE; out->log = "handover"; out->eat = out->draw = 0;
                    return;
                }
                out->se = CP_SE_ERROR;
            } else {
                int d = cp_dir_(p, press, stick_y);
                if (d) {
                    int n = p->page ? cp_nm() : CP_ROWS, cur = p->page ? p->who : p->row;
                    cp_set_row_(p, s, (cur + d + n) % n, v);
                    if (!cp_built_(p)) { cp_abort(p, "abort tree-full", out); return; }
                    out->se = CP_SE_CURSOR; out->log_row = p->page ? p->who : p->row;
                }
            }
        }
        cp_draw_(p, s, o);
        return;
    }
    case CP_SWITCH: {  // input ignored until the other page is up
        out->eat = out->draw = 1;
        cp_update_(p, s, false);
        int n0 = o ? o->n : 0;
        cp_draw_(p, s, o);
        bool done = true;
        for (int k = 0; k < cp_nroot_(p) && done; k++) if (!cs_is_end(&p->tree, p->root[k])) done = false;
        if (!done && ++p->timer < CP_SWITCH_MAX) return;
        if (!done) out->log = "switch timeout";
        int from = p->page;
        p->page = p->next;
        if (p->page) p->who = 0, p->sub = 0, p->ab_row = p->ab_top = 0, p->cu_slot = 0, p->eq_slot = 0;
        else p->row = from == 5 ? CP_ROW_DEBUG : from == 2 ? CP_ROW_ABIL : from == 3 ? CP_ROW_CUST : from == 4 ? CP_ROW_EQUIP : CP_ROW_STATUS;
        cp_build_(p, v);
        if (!cp_built_(p)) { cp_abort(p, "abort tree-full", out); return; }
        for (int k = 0; k < cp_nroot_(p); k++) cs_begin(&p->tree, p->root[k]);
        p->state = CP_OPEN; p->timer = 0;
        if (o) o->n = n0;
        cp_draw_(p, s, o);  // the new page's frame 0 replaces the old page's last
        return;
    }
    case CP_CLOSING: {
        out->eat = out->draw = 1;
        cp_update_(p, s, false);
        cp_draw_(p, s, o);
        bool done = cs_layout_last(s, 2, p->layout_f);
        for (int k = 0; k < cp_nroot_(p) && done; k++) if (!cs_is_end(&p->tree, p->root[k])) done = false;
        if (done) { p->state = CP_PULSE; p->timer = 0; }
        return;
    }
    case CP_PULSE:
        if (pause_state == 0) { p->state = CP_CLOSED; out->log = "closed"; return; }
        if (p->timer >= CP_PULSE_WAIT) { cp_abort(p, "abort close-timeout", out); return; }
        out->eat = out->draw = 1;
        out->pulse_plus = (p->timer / 4) % 2 == 0;
        p->timer++;
        cp_backdrop_(o);
        return;
    }
}

// sora_camp_dbg.h: (s140 dbgmenu) the KH2 pause menu "Debug Menu" page (page 5). Pure, included by sora_camp_page.h after the
// Equipment page. The layout is the Customize slot list (sora_camp_cust.h: window 227-229, rows ag 264 at y = 26 * i, left tag
// ag 282): the row name is the row text, the VALUE sits in the tag column (Off / LV 45 / On), so every position is one the
// Customize page already uses. GUESS: that a value in the tag column reads well (not seen in game); the tag column is as wide as
// "L+Triangle", the row names stay <= 23 chars (CsDesc.text). All text literal (no new baked text / no font work).
// Input: Up / Down move (clamped, hold repeats via cp_dir_), Left / Right change the value (hold repeats: 18 presents, then every
// 3; the level steps by 5 after 60 held presents), A = toggle (level row: Off), B = back to the top page.
// The page owns its copy of the values (p->db_*); the glue (sora_camp.h) applies out->db_set / db_val to the game and writes the
// values really in force back (a refusal shows), then p->db_rebuild redraws.
#pragma once
#include "sora_pchar.h"  // (s220 members: the Party rows' characters + offer rules; pure)

// (s146 outfit) the arrays are by SHOWN row; CP_DB_ORD (sora_camp_page.h) gives the backend row of each. Become Sora / Outfit / Auto Outfits on top.
static const char *const CP_DB_NAME[CP_DB_ROWS] = { "Become Sora", "Outfit", "Auto Outfits", "Party 1", "Party 2", "Sora Level", "Unlock Keyblades", "Unlock Drive Forms", "Unlock Summons", "All Abilities", "God Mode", "No Clip" };
static const char *const CP_DB_HELP[CP_DB_ROWS] = { "Roxas turns into Sora now (Off: story)", "Halloween/Christmas Town, Space Paranoids, Timeless River", "Night: Halloween, cold: Christmas", "Left/Right: who stands in Donald's place (this session)", "Left/Right: who stands in Goofy's place (this session)", "Left/Right: set level", "A or Left/Right: toggle", "A or Left/Right: toggle", "A or Left/Right: toggle", "A or Left/Right: toggle", "A or Left/Right: toggle", "Stick, B up, Y down" };
#define CP_DB_VIS 7  // (s146) rows shown at once = the 7-row list the window was built for (s140); the list scrolls (p->db_top)
#define CP_DB_OUTFITS 5  // (s146) Outfit row values: Normal, Halloween Town, Christmas Town, Space Paranoids, Timeless River
static const char *const CP_DB_OUTFIT_TAG[CP_DB_OUTFITS] = { "Normal", "Halloween", "Christmas", "Paranoids", "Timeless" };  // (<= 9 chars: the tag column is as wide as "L+Triangle"; GUESS that fits)

// (s220 members) Party 1 / Party 2 (backend rows 10 / 11): the value = the character id in Donald's / Goofy's place (sora_pchar.h PCH_*),
// shown by name; the row offers its slot's own member + every installed stand-in not standing in the other slot (pchar_offer)

static void cp_db_title_(CsDesc title[1]) {
    title[0] = (CsDesc){ CP_SEQ_(75, 76, 77), .stack_index = 0, .stack_w = CS_DEFAULT_STACK, .anchor = CS_BOTTOM_LEFT,
                         .flags = CS_A_TEXT_IGNORE_COLOR | CS_A_TEXT_TX | CS_A_CHILD_STACK_H };
    snprintf(title[0].text, sizeof title[0].text, "Debug Menu");
}
static void cp_db_value_(char *b, int n, const CampPage *p, int row) {
    int be = CP_DB_ORD[row], v = p->db_v[be];
    if (be == 0) { if (v > 0) snprintf(b, (size_t)n, "LV %d", v); else snprintf(b, (size_t)n, "Off"); }
    else if (be == 8) snprintf(b, (size_t)n, "%s", CP_DB_OUTFIT_TAG[v < 0 || v >= CP_DB_OUTFITS ? 0 : v]);
    else if (be == 10 || be == 11) snprintf(b, (size_t)n, "%s", pchar_valid(v) ? PCH_TAB[v].name : "?");  // (s217 riku / s220)
    else snprintf(b, (size_t)n, "%s", v ? "On" : "Off");
}

// 4 roots: bg states, title, window, help
static int cp_build_dbg(CsTree *t, const CampPage *p, const CpVals *v, int r[4]) {
    CsDesc k_munny[1], k_time[1], bg_state[3], title[1], sel_kids[2], kids[CP_DB_VIS * 2], rk[CP_DB_VIS][3], win[1], help[1];
    cs_tree_clear(t);
    cp_bg_state_(k_munny, k_time, bg_state, v);
    cp_db_title_(title);
    sel_kids[0] = (CsDesc){ CP_SEQ_(-1, 25, -1), .anchor = CS_BOTTOM_LEFT, .flags = CS_A_NO_CHILD_TX };
    sel_kids[1] = (CsDesc){ CP_SEQ_(27, 28, 29) };
    int nk = 0;
    for (int k = 0; k < CP_DB_VIS; k++) {
        int i = p->db_top + k, m = 0;  // i = the SHOWN row, k = its place in the window
        if (i >= CP_DB_ROWS) break;
        rk[k][m] = (CsDesc){ CP_SEQ_(-1, 123, -1), .anchor = CS_BOTTOM_LEFT };
        snprintf(rk[k][m].text, sizeof rk[k][m].text, "%s", CP_DB_NAME[i]);
        m++;
        if (i == p->db_row) { rk[k][m++] = sel_kids[0]; rk[k][m++] = sel_kids[1]; }
        kids[nk++] = (CsDesc){ .y = 26.0f * k, CP_SEQ_(-1, 264, -1), .nkids = (uint8_t)m, .kids = rk[k] };
        kids[nk] = (CsDesc){ .y = 26.0f * k, CP_SEQ_(-1, 282, -1), .flags = CS_A_TEXT_IGNORE_SCALE, .anchor = CS_BOTTOM_LEFT };
        cp_db_value_(kids[nk].text, (int)sizeof kids[nk].text, p, i);
        nk++;
    }
    win[0] = (CsDesc){ CP_SEQ_(227, 228, 229), .nkids = (uint8_t)nk, .kids = kids };
    help[0] = (CsDesc){ .x = 150, .y = 352, CP_SEQ_(-1, 124, -1), .flags = CS_A_TEXT_IGNORE_COLOR, .anchor = CS_BOTTOM_LEFT };
    snprintf(help[0].text, sizeof help[0].text, "%s", CP_DB_HELP[p->db_row]);
    r[0] = cs_tree_add(t, bg_state, 3); r[1] = cs_tree_add(t, title, 1); r[2] = cs_tree_add(t, win, 1); r[3] = cs_tree_add(t, help, 1);
    return 4;
}

// rebuild in place (a cursor move / a value change): bg and the list skip their start anims
static void cp_dbg_go_(CampPage *p, const CampSeq *s, const CpVals *v) {
    cp_build_(p, v);
    if (!cp_built_(p)) return;
    for (int k = 0; k < cp_nroot_(p); k++) { cs_begin(&p->tree, p->root[k]); cs_skip(&p->tree, p->root[k]); }
    for (int k = 0; k < cp_nroot_(p); k++) cs_draw(&p->tree, s, NULL, p->root[k], 0, 0);
}

// Left / Right: edges from `press`, then hold-repeat from the held buttons (p->db_rep counts the presents held)
static int cp_db_lr_(CampPage *p, uint64_t press, int *step) {
    int d = (press & CP_BTN_LEFT) ? -1 : (press & CP_BTN_RIGHT) ? 1 : 0;
    *step = 1;
    if (d) { p->db_rep = 0; return d; }
    uint64_t h = p->held_btn & (CP_BTN_LEFT | CP_BTN_RIGHT);
    if (h != CP_BTN_LEFT && h != CP_BTN_RIGHT) { p->db_rep = 0; return 0; }
    int n = ++p->db_rep;
    if (n >= 60) *step = 5;
    if (n >= 18 && (n - 18) % 3 == 0) return h == CP_BTN_LEFT ? -1 : 1;
    return 0;
}

static void cp_dbg_input_(CampPage *p, const CampSeq *s, uint64_t press, float stick_y, const CpVals *v, CpOut *out) {
    if (p->db_rebuild) { p->db_rebuild = 0; cp_dbg_go_(p, s, v); return; }  // (the glue applied a change: redraw, input next present)
    if (press & CP_BTN_B) { cp_switch_(p, 0); out->se = CP_SE_CANCEL; out->log = "debug back"; return; }
    int d = cp_dir_(p, press, stick_y);
    if (d) {
        int r = p->db_row + d;
        if (r < 0 || r >= CP_DB_ROWS) return;
        p->db_row = r;
        if (r < p->db_top) p->db_top = r;  // (s146: the window follows the cursor)
        if (r >= p->db_top + CP_DB_VIS) p->db_top = r - CP_DB_VIS + 1;
        cp_dbg_go_(p, s, v);
        out->se = CP_SE_CURSOR; out->log_row = r;
        return;
    }
    int step, lr = cp_db_lr_(p, press, &step), row = CP_DB_ORD[p->db_row];  // (s146: the BACKEND row)
    if (row == 8) {  // (s146 outfit) Normal <-> 4 outfits: Left / Right step (clamped), A = next (wraps)
        int nv = p->db_v[8];
        if (lr > 0) nv++;
        else if (lr < 0) nv--;
        else if (press & CP_BTN_A) nv = (nv + 1) % CP_DB_OUTFITS;
        else return;
        if (nv < 0 || nv >= CP_DB_OUTFITS) { out->se = CP_SE_ERROR; return; }  // (at an end)
        p->db_v[8] = nv;
        out->db_set = 9, out->db_val = nv;
        out->se = CP_SE_CURSOR; out->log = "debug outfit";
        return;
    }
    if (row == 10 || row == 11) {  // (s220 members) Party 1 / 2: Left / Right step through the offered characters (clamped), A = the next (wraps);
        // a row that offers nobody else (no stand-in installed, or the only one stands in the other slot): every press = the error SE
        int slot = row - 10, cur[PCH_SLOTS] = {p->db_v[10], p->db_v[11]};
        if (!lr && !(press & CP_BTN_A)) return;
        int nv = pchar_step(slot, cur, v->db_party_have, lr > 0 ? 1 : lr < 0 ? -1 : 0);
        if (nv < 0 || nv == p->db_v[row]) { out->se = CP_SE_ERROR; return; }
        p->db_v[row] = nv;
        out->db_set = (uint8_t)(row + 1), out->db_val = nv;
        out->se = CP_SE_DECIDE; out->log = slot ? "debug party 2" : "debug party 1";
        return;
    }
    if (row == 0) {  // the level slider: Off <-> 1..99; Right from Off starts at the real level
        int nv = p->db_v[0];
        if (lr > 0) nv = nv == 0 ? (v->db_real_lv > 0 ? v->db_real_lv : 1) : nv + step;
        else if (lr < 0) nv = nv - step;
        else if (press & CP_BTN_A) nv = 0;
        else return;
        nv = nv < 0 ? 0 : nv > 99 ? 99 : nv;
        if (nv == p->db_v[0]) { if (lr) out->se = CP_SE_ERROR; return; }  // (at an end)
        p->db_v[0] = nv;
        out->db_set = 1, out->db_val = nv;
        out->se = CP_SE_CURSOR; out->log = "debug level";
        return;
    }
    int *on = &p->db_v[row], nv = *on;
    if (lr > 0) nv = 1;
    else if (lr < 0) nv = 0;
    else if (press & CP_BTN_A) nv = !nv;
    else return;
    if (nv == *on) return;
    *on = nv;
    out->db_set = (uint8_t)(row + 1), out->db_val = nv;  // (row = backend row)
    out->se = CP_SE_DECIDE; out->log = "debug toggle";
}

// sora_camp_equip.h: the KH2 pause menu Equipment page trees (slice 5 step 6, plan
// docs/plans/2026-10-30-kh2-pause-menu-slice5-equipment.md). Pure, included by sora_camp_page.h after Customize.
// Layout = Customize's (RULING s85b: FM's Equipment window anim groups not traced, recon/S85_EQUIP_PAGE.md). Text keys:
// camp_text.txt 10 title, 20 member help, 162..165 rows + helps, 166.. Keyblade names / descs (KB_TEXT + 2 * index).
// GUESSes: a row = the equipped Keyblade's name, the slot name ("Weapons" / "Valor") in the tag (ag 282); the picker
// lists KB_TAB in table order with the cursor on the slot's Keyblade (no "equipped" mark); the other slot's Keyblade
// stays listed and A on it = error SE (one copy, kb_offer).
// (s89 kb) the picker lists the OWNED Keyblades only (CpVals kb_own, kb_own_fix), CP_EQ_VIS rows with a scroll (as
// the Abilities list; 9 = GUESS); text keys by kb_text_key (KB_TEXT 166.. for the first 4, KB_TEXT2 400.. after).
#pragma once
#include "sora_kb.h"

#define CP_EQ_KEY_TITLE 10          // "Equipment"
#define CP_EQ_KEY_MEMBER_HELP 20    // "Equip weapons and armor."
#define CP_EQ_KEY_ROW 162           // + 2 * slot: Weapons / Valor, + 1 = the row's help
#define CP_EQ_VIS 9                 // (s89 kb) visible picker rows (GUESS: the Abilities list's 9)
// (s91 mfkb) the visible slot rows: Weapons + Valor always, Master / Final when that Drive Form is unlocked (v->kb_forms bit 0 /
// 1: sora_unlock.h ul_form_unlocked; vanilla KH2 lists a form's sub-weapon row once it is obtained, S85_EQUIP_PAGE.md kind 6)
// and live "set mfkb" is on. -> count, out = the slots. Text: 440.. = Master / Final row + help (camp_text_s91_mfkb.txt)
#define CP_EQ_KEY_ROW2 440          // + 2 * (slot - KB_MASTER): Master / Final row, + 1 = the row's help
static int cp_eq_row_key(int slot) { return slot < KB_MASTER ? CP_EQ_KEY_ROW + 2 * slot : CP_EQ_KEY_ROW2 + 2 * (slot - KB_MASTER); }
static int cp_eq_vis(const CpVals *v, int out[KB_SLOTS]) {
    int n = 0;
    out[n++] = KB_MAIN, out[n++] = KB_VALOR;
    if (g_mfkb && (v->kb_forms & 1)) out[n++] = KB_MASTER;
    if (g_mfkb && (v->kb_forms & 2)) out[n++] = KB_FINAL;
    return n;
}
static int cp_eq_name_key(int id) { int i = kb_index(id); return kb_text_key(i < 0 ? 0 : i); }
// (s89 kb) the picker's entries: the owned KB_TAB indices -> count; pos = the entry of KB_TAB index i (-1 none)
static int cp_eq_list(const CpVals *v, const int kb[KB_SLOTS], int out[KB_N]) { return kb_own_list(kb_own_fix(v->kb_own, kb), out); }
static int cp_eq_pos(const int *l, int n, int i) { for (int k = 0; k < n; k++) if (l[k] == i) return k; return -1; }

// ======================= (s128 wpnmenu) Link's BotW weapon in the Weapons slot =======================
// The picker's entries are a VIRTUAL list: [CP_EQ_WPN] (only for Sora's base-form Weapons slot, v->wpn_on) + the owned Keyblades
// (cp_eq_plist). CampPage.eq_pick = a KB_TAB index, or CP_EQ_WPN for the top entry; eq_top / the cursor move over this list, so
// the extra entry shifts every Keyblade one place down and the [ total ] numbers (cp_eq_*_total) just follow the list.
// The Weapons row shows the weapon's name while it is in use (v->wpn_use); a Keyblade pick sets wpn_use 0 (CpOut.wpn_set 2).
#define CP_EQ_WPN 1000
static int cp_eq_wpn_row(const CpVals *v) { return g_eqlook && v->wpn_on && v->wpn_use; }  // (only the vanilla look: the old page has no panel / literal rows)
static int cp_eq_has_wpn(const CpVals *v, int slot) { return g_eqlook && v->wpn_on && slot == KB_MAIN; }
static int cp_eq_plist(const CpVals *v, const int kb[KB_SLOTS], int slot, int out[KB_N + 1]) {
    int n = 0;
    if (cp_eq_has_wpn(v, slot)) out[n++] = CP_EQ_WPN;
    return n + cp_eq_list(v, kb, out + n);
}
// Attack / Magic of Sora's main hand (the weapon in use, else the main Keyblade)
// (s128 review) Strength / Magic [ totals ] = level + the main KEYBLADE always: the BotW weapon's Attack is its own number (wpn_dmg
// does not use Str) and the Drive Form rows keep their Keyblades.
static int cp_eq_main_str(const CpVals *v, int main_id) { (void)v; return kb_str(main_id); }
static int cp_eq_main_mag(const CpVals *v, int main_id) { (void)v; return kb_mag(main_id); }

// sub 0 = the member step (Customize's, own title + help). 4 roots.
static void cp_build_equip_member(CsTree *t, int who, const CpVals *v, int r[4]) {
    cp_build_cust_member(t, who, v, r);
    if (r[1] < 0 || r[3] < 0) return;
    t->n[t->n[r[1]].kid0].d.msg_key = CP_EQ_KEY_TITLE;
    t->n[t->n[r[3]].kid0].d.msg_key = CP_EQ_KEY_MEMBER_HELP;  // (a root's kid0 = its first desc, as camp_test cust_win_)
}

// sub 1 = the slot list (+ sub 2 = the Keyblade picker over it, pick = KB_TAB index >= 0, top = the first entry shown).
// 4 roots, 5 with the picker.
static int cp_build_equip_list_old(CsTree *t, const int kb[KB_SLOTS], const CpVals *v, int slot, int pick, int top, int r[5]) {
    CsDesc k_munny[1], k_time[1], bg_state[3], title[2], sel_kids[2], kids[KB_SLOTS * 2], rk[KB_SLOTS][3], win[1], help[1];
    CsDesc pkids[CP_EQ_VIS], prk[CP_EQ_VIS][3], pick_win[1];
    cs_tree_clear(t);
    cp_bg_state_(k_munny, k_time, bg_state, v);
    cp_cu_title_(title, 0);
    title[0].msg_key = CP_EQ_KEY_TITLE;
    sel_kids[0] = (CsDesc){ CP_SEQ_(-1, 25, -1), .anchor = CS_BOTTOM_LEFT, .flags = CS_A_NO_CHILD_TX };
    sel_kids[1] = (CsDesc){ CP_SEQ_(27, 28, 29) };
    int nk = 0, vs[KB_SLOTS], nv = cp_eq_vis(v, vs);
    for (int i = 0; i < nv; i++) {
        int m = 0;
        rk[i][m++] = (CsDesc){ CP_SEQ_(-1, 123, -1), .msg_key = cp_eq_name_key(kb_slot_id(kb, vs[i])), .anchor = CS_BOTTOM_LEFT };
        if (vs[i] == slot && pick < 0) { rk[i][m++] = sel_kids[0]; rk[i][m++] = sel_kids[1]; }
        kids[nk++] = (CsDesc){ .y = 26.0f * i, CP_SEQ_(-1, 264, -1), .nkids = (uint8_t)m, .kids = rk[i] };
        kids[nk++] = (CsDesc){ .y = 26.0f * i, CP_SEQ_(-1, 282, -1), .flags = CS_A_TEXT_IGNORE_SCALE, .anchor = CS_BOTTOM_LEFT,
                               .msg_key = cp_eq_row_key(vs[i]) };
    }
    win[0] = (CsDesc){ CP_SEQ_(227, 228, 229), .nkids = (uint8_t)nk, .kids = kids };
    int hk = pick < 0 ? cp_eq_row_key(slot) + 1 : kb_text_key(pick) + 1;
    help[0] = (CsDesc){ .x = 150, .y = 352, CP_SEQ_(-1, 124, -1), .flags = CS_A_TEXT_IGNORE_COLOR, .anchor = CS_BOTTOM_LEFT, .msg_key = hk };
    r[0] = cs_tree_add(t, bg_state, 3); r[1] = cs_tree_add(t, title, 2); r[2] = cs_tree_add(t, win, 1); r[3] = cs_tree_add(t, help, 1);
    if (pick < 0) return 4;
    int l[KB_N], n = cp_eq_list(v, kb, l), np = 0;
    for (int e = top < 0 ? 0 : top; e < n && np < CP_EQ_VIS; e++, np++) {
        int i = l[e], m = 0;
        prk[np][m++] = (CsDesc){ CP_SEQ_(-1, 123, -1), .msg_key = kb_text_key(i), .anchor = CS_BOTTOM_LEFT };
        if (i == pick) { prk[np][m++] = sel_kids[0]; prk[np][m++] = sel_kids[1]; }
        pkids[np] = (CsDesc){ .y = 26.0f * np, CP_SEQ_(-1, 264, -1), .nkids = (uint8_t)m, .kids = prk[np] };
    }
    pick_win[0] = (CsDesc){ .x = 40, .y = 26, CP_SEQ_(227, 228, 229), .nkids = (uint8_t)np, .kids = pkids };
    r[4] = cs_tree_add(t, pick_win, 1);
    return 5;
}

// ======================= (s120 eqlook) the vanilla KH2 FM Equipment look, recon/S120_HELPER_EQLOOK.md =======================
// g_eqlook 0 (live "set eqlook 0") = the old page above (Customize's window / rows, no panel, no Armor rows).
// [V] = VERIFIED from the exe tables / decompile, [G] = GUESS (a position / colour read off the reference photos).
#define CP_EQ_ARM 100            // eq_slot >= CP_EQ_ARM = an armor row (eq_slot - CP_EQ_ARM); only with g_eqlook
#define CP_EQ_KEY_ARMOR 600      // "Armor" tag (sys 0x0463)
#define CP_EQ_KEY_ABILITY 601    // "Ability" label (sys 0x464c)
#define CP_EQ_ROWS_MAX (KB_SLOTS + 3)
// the ability name of a Keyblade: the Abilities page's key, else keys 610.. (the 8 Keyblade abilities not in AB_TAB; names only)
static int cp_eq_ab_key(int ab) {
    static const int ID[8] = {162, 163, 398, 412, 413, 422, 404, 541};
    int k = cp_ab_name_key(ab);
    if (k) return k;
    for (int i = 0; i < 8; i++) if (ID[i] == ab) return 610 + i;
    return 0;
}
// the rows of the state 2 page: Weapons, Valor / Master / Final (cp_eq_vis), then the armor pieces
static int cp_eq_rows(const CpVals *v, int out[CP_EQ_ROWS_MAX]) {
    int n = cp_eq_vis(v, out);
    if (g_eqlook) for (int i = 0; i < v->arm_n && i < 3; i++) out[n++] = CP_EQ_ARM + i;
    return n;
}
// [V] FM tables: row ag 0x5bc0d0 {265 Weapons, 252 Armor} + 0x5bc0e0 {266 Valor, 267 Master, 268 Final}; tag ag 0x5bc0f0
// {283 Weapons, 278 Armor} + 0x5bc110 {284, 285, 286}. Colours (ABGR in the sequence): red / teal / teal / teal / yellow.
static int cp_eq_row_ag(int slot) { return slot >= CP_EQ_ARM ? 252 : slot == KB_MAIN ? 265 : slot == KB_VALOR ? 266 : slot == KB_MASTER ? 267 : 268; }
static int cp_eq_tag_ag(int slot) { return slot >= CP_EQ_ARM ? 278 : slot == KB_MAIN ? 283 : slot == KB_VALOR ? 284 : slot == KB_MASTER ? 285 : 286; }
// [V] picker kinds, tables 0x5bc120 (start, end, loop) / 0x5bc150 (panel) / 0x5bc170 (row) / 0x5bc188 (2nd column):
// Weapons 207-209 / 213-215 / 211 / 212, form sub-weapon 217-219 / 223-225 / 221 / 222
static int cp_eq_is_sub(int slot) { return slot != KB_MAIN; }

// the totals in the panel brackets. [G] = the Status page's number = eq_base (Level part) + the MAIN Keyblade; CpVals.eq_base_*
// both 0 = fall back to the Status page's st[0] value.
static int cp_eq_total_str(const CpVals *v, int main_kb_str) { return v->eq_base_str || v->eq_base_mag ? v->eq_base_str + main_kb_str : v->st[0].str; }
static int cp_eq_total_mag(const CpVals *v, int main_kb_mag) { return v->eq_base_str || v->eq_base_mag ? v->eq_base_mag + main_kb_mag : v->st[0].mag; }
static int cp_eq_arm_total(const CpVals *v) { int s = 0; for (int i = 0; i < v->arm_n && i < 3; i++) s += v->arm_def[i]; return s; }

#define CP_SEQ2_(s_, l_, e_) .start = (s_), .loop = (l_), .end = (e_)
static CsDesc cp_eq_txt_(int ag, const char *lit, int key, int anchor, float x, float y) {
    CsDesc d = { .x = x, .y = y, CP_SEQ2_(-1, ag, -1), .msg_key = key, .anchor = (uint8_t)anchor, .flags = CS_A_TEXT_IGNORE_SCALE };
    if (lit) snprintf(d.text, sizeof d.text, "%s", lit);
    return d;
}
// state 2 stat line: orange label (ag 32 [V]: camp 0x35c4 = 0x20 for mode 1) + "value [ total ]" ([G] positions)
// (s120b user marks) ag 32's own sprites ARE the brackets ("[" at +29, "]" at +73 from the ag origin, camp_seq1_all.txt) and the
// label (+-85): the text holds only the numbers, total centred between the brackets, value right-aligned before "[".
// Live tuning (s120b): eq2x / eq2y move the state-2 panel text, eqtx / eqvx / eqky the numbers inside a line (GUESS spots).
static void cp_eq_stat_line_(CsDesc *line, CsDesc kids[2], int key, int val, int total, float y) {
    kids[0] = (CsDesc){ .x = (float)g_eqtx, .y = (float)g_eqky, CP_SEQ2_(-1, 124, -1), .flags = CS_A_TEXT_TX | CS_A_TEXT_IGNORE_SCALE, .anchor = CS_BOTTOM_CENTER };
    snprintf(kids[0].text, sizeof kids[0].text, "%d", total);
    kids[1] = (CsDesc){ .x = (float)g_eqvx, .y = (float)g_eqky, CP_SEQ2_(-1, 123, -1), .flags = CS_A_TEXT_TX | CS_A_TEXT_IGNORE_SCALE, .anchor = CS_BOTTOM_RIGHT };
    snprintf(kids[1].text, sizeof kids[1].text, "%d", val);
    *line = (CsDesc){ .x = -95 + g_eq2x, .y = y + g_eq2y, CP_SEQ2_(-1, 32, -1), .msg_key = key, .anchor = CS_BOTTOM_LEFT,
                      .flags = CS_A_TEXT_IGNORE_SCALE | CS_A_TEXT_TX, .nkids = 2, .kids = kids };
}
// picker stat lines: label (ag 37 [V] mode 3) + "[ new total ]", second line "cur > new" (ag 38 [V]). Colours [G] from ref1:
// the new number + total red (122) when lower, cyan (118) when higher, white when equal.
static int cp_eq_cmp_ag_(int cur, int nw) { return nw < cur ? 122 : nw > cur ? 118 : 123; }
static void cp_eq_cmp_lines_(CsDesc out[2], CsDesc kids[4], int key, int cur, int nw, int total, float y) {
    int ag = cp_eq_cmp_ag_(cur, nw);
    char b[24];
    // (s120d mark_165345_3: x 29 drew the total ~57 left of the brackets, over the label -> live eqpt / eqpc, measured from the mark)
    // (s120c user mark: the cur > new row sat ON the label row -> one row lower, ref1) (s120b) ag 37's sprites = the brackets (+7 / +51), ag 38's = the line + the arrow (+3, 27): no literal "[ ]" / ">"
    kids[0] = (CsDesc){ .x = (float)g_eqpt, .y = (float)g_eqky, CP_SEQ2_(-1, ag, -1), .flags = CS_A_TEXT_TX | CS_A_TEXT_IGNORE_SCALE, .anchor = CS_BOTTOM_CENTER };
    snprintf(kids[0].text, sizeof kids[0].text, "%d", total);
    out[0] = (CsDesc){ .x = -77 + g_eqpx, .y = y + g_eqpy, CP_SEQ2_(-1, 37, -1), .msg_key = key, .anchor = CS_BOTTOM_LEFT,
                       .flags = CS_A_TEXT_IGNORE_SCALE | CS_A_TEXT_TX, .nkids = 1, .kids = kids };
    snprintf(b, sizeof b, "%d", cur);
    kids[1] = cp_eq_txt_(123, b, 0, CS_BOTTOM_RIGHT, (float)g_eqpc + g_eqax, 0);  // (s120d mark_165345_3: 9 drew it ~62 right of the arrow)
    snprintf(b, sizeof b, "%d", nw);
    kids[2] = cp_eq_txt_(ag, b, 0, CS_BOTTOM_LEFT, 14 + g_eqax, 0);
    out[1] = (CsDesc){ .x = -131 + g_eqpx + 46, .y = y + g_eqpy + 2, CP_SEQ2_(-1, 38, -1), .anchor = CS_BOTTOM_LEFT, .flags = CS_A_TEXT_IGNORE_SCALE, .nkids = 2, .kids = kids + 1 };
}
// the header tabs: Equipment > Sora [> Weapons / Valor ..] (state 5 / 6 = ref1's three tabs)
static void cp_eq_title3_(CsDesc title[3], int third_key) {
    cp_cu_title_(title, 0);
    title[0].msg_key = CP_EQ_KEY_TITLE;
    title[2] = title[1];
    title[2].stack_index = 2;
    title[2].msg_key = third_key;
    title[2].text[0] = 0;
}

// sub 1 / sub 2 with the vanilla look. Roots: 0 bg states, 1 title, 2 window (the list; the picker's window in sub 2), 3 help,
// 4 right panel. -> 5.
static int cp_build_equip_look(CsTree *t, const int kb[KB_SLOTS], const CpVals *v, int slot, int pick, int top, int r[5]) {
    CsDesc k_munny[1], k_time[1], bg_state[3], title[3], sel_kids[2], kids[CP_EQ_ROWS_MAX * 2], rk[CP_EQ_ROWS_MAX][3], win[1], help[1];
    CsDesc pkids[CP_EQ_VIS * 2], prk[CP_EQ_VIS][3], pcol[CP_EQ_VIS][1], pnl[1], pk[8], pl1[2], pl2[2], pl3[4], pl4[4];
    int vs[CP_EQ_ROWS_MAX], nv = cp_eq_rows(v, vs), hk = 0, np = 0, nt = 2;
    char hlit[64] = "";
    int wrow = cp_eq_wpn_row(v);  // (s128 wpnmenu) the Weapons row shows Link's BotW weapon
    cs_tree_clear(t);
    cp_bg_state_(k_munny, k_time, bg_state, v);
    sel_kids[0] = (CsDesc){ CP_SEQ_(-1, 25, -1), .anchor = CS_BOTTOM_LEFT, .flags = CS_A_NO_CHILD_TX };
    sel_kids[1] = (CsDesc){ CP_SEQ_(27, 28, 29) };
    if (pick < 0) {  // ---- state 2: the page ----
        cp_eq_title3_(title, 0);
        int nk = 0;
        for (int i = 0; i < nv; i++) {
            int m = 0, s = vs[i];
            if (s >= CP_EQ_ARM) rk[i][m++] = cp_eq_txt_(123, v->arm_name[s - CP_EQ_ARM], 0, CS_BOTTOM_LEFT, 0, 0);
            else if (s == KB_MAIN && wrow) rk[i][m++] = cp_eq_txt_(123, v->wpn_name, 0, CS_BOTTOM_LEFT, 0, 0);  // (s128 wpnmenu)
            else rk[i][m++] = (CsDesc){ CP_SEQ_(-1, 123, -1), .msg_key = cp_eq_name_key(kb_slot_id(kb, s)), .anchor = CS_BOTTOM_LEFT };
            if (s == slot) { rk[i][m++] = sel_kids[0]; rk[i][m++] = sel_kids[1]; }
            kids[nk++] = (CsDesc){ .y = 26.0f * i, CP_SEQ_(-1, cp_eq_row_ag(s), -1), .nkids = (uint8_t)m, .kids = rk[i] };
            if (s < CP_EQ_ARM || s == CP_EQ_ARM)  // the tag: every Keyblade row, the first armor row only (FUN_140358c50)
                kids[nk++] = (CsDesc){ .y = 26.0f * i, CP_SEQ_(-1, cp_eq_tag_ag(s), -1), .flags = CS_A_TEXT_IGNORE_SCALE, .anchor = CS_BOTTOM_LEFT,
                                       .msg_key = s >= CP_EQ_ARM ? CP_EQ_KEY_ARMOR : cp_eq_row_key(s) };
        }
        win[0] = (CsDesc){ CP_SEQ_(227, 228, 229), .nkids = (uint8_t)nk, .kids = kids };
        if (slot >= CP_EQ_ARM) snprintf(hlit, sizeof hlit, "Armor worn in BotW.");
        else if (slot == KB_MAIN && wrow) snprintf(hlit, sizeof hlit, "Link's BotW weapon.");  // (s128 review: CsDesc.text is 24 bytes)  // (s128 wpnmenu)
        else { int ki = kb_index(kb_slot_id(kb, slot)); hk = ki >= 0 ? kb_text_key(ki) + 1 : cp_eq_row_key(slot) + 1; }  // (s120b ref0: the Keyblade's description)
    } else {  // ---- state 5 / 6: the picker (vanilla swaps the window and the panel ags) ----
        cp_eq_title3_(title, cp_eq_row_key(slot));
        nt = 3;
        int l[KB_N + 1], n = cp_eq_plist(v, kb, slot, l), sub = cp_eq_is_sub(slot);
        for (int e = top < 0 ? 0 : top; e < n && np < CP_EQ_VIS; e++, np++) {
            int i = l[e], m = 0;
            if (i == CP_EQ_WPN) prk[np][m++] = cp_eq_txt_(123, v->wpn_name, 0, CS_BOTTOM_LEFT, 0, 0);  // (s128 wpnmenu) the top entry
            else prk[np][m++] = (CsDesc){ CP_SEQ_(-1, 123, -1), .msg_key = kb_text_key(i), .anchor = CS_BOTTOM_LEFT };
            if (i == pick) { prk[np][m++] = sel_kids[0]; prk[np][m++] = sel_kids[1]; }
            pkids[np * 2] = (CsDesc){ .y = 26.0f * np, CP_SEQ_(-1, sub ? 221 : 211, -1), .nkids = (uint8_t)m, .kids = prk[np] };
            if (i == CP_EQ_WPN) {  // 2nd column = the Attack number (GUESS; a Keyblade shows its ability)
                char at[12];
                snprintf(at, sizeof at, "%d", v->wpn_atk);
                pcol[np][0] = cp_eq_txt_(124, at, 0, CS_BOTTOM_CENTER, 44.5f, 0);
            } else pcol[np][0] = (CsDesc){ .x = 44.5f, CP_SEQ_(-1, 124, -1), .msg_key = cp_eq_ab_key(KB_TAB[i].ability), .anchor = CS_BOTTOM_CENTER };
            pkids[np * 2 + 1] = (CsDesc){ .y = 26.0f * np, CP_SEQ_(-1, sub ? 222 : 212, -1), .nkids = 1, .kids = pcol[np] };
        }
        win[0] = sub ? (CsDesc){ CP_SEQ_(217, 218, 219), .nkids = (uint8_t)(np * 2), .kids = pkids }
                     : (CsDesc){ CP_SEQ_(207, 208, 209), .nkids = (uint8_t)(np * 2), .kids = pkids };
        if (pick == CP_EQ_WPN) snprintf(hlit, sizeof hlit, "Use Link's BotW weapon.");  // (s128 review: <= 23 chars)  // (s128 wpnmenu)
        else hk = kb_text_key(pick) + 1;
    }
    help[0] = (CsDesc){ .x = (float)g_eqhx, .y = (float)g_eqhy, CP_SEQ_(-1, 124, -1), .flags = CS_A_TEXT_IGNORE_COLOR, .anchor = CS_BOTTOM_LEFT, .msg_key = hk };  // (s120b: was 150 352)
    if (hlit[0]) snprintf(help[0].text, sizeof help[0].text, "%s", hlit);
    // ---- the right panel (state 2: ag 287-289 [V]; picker: kind panel ags) ----
    int main_id = kb_slot_id(kb, KB_MAIN), cur_id = slot >= CP_EQ_ARM ? 0 : kb_slot_id(kb, slot), npk = 0;
    if (pick < 0 && slot >= CP_EQ_ARM) {  // [G] Armor: "Defense <piece> [ <sum worn> ]", no Ability line
        cp_eq_stat_line_(&pk[npk++], pl1, 69, v->arm_def[slot - CP_EQ_ARM], cp_eq_arm_total(v), 8);
        pnl[0] = (CsDesc){ CP_SEQ_(287, 288, 289), .nkids = (uint8_t)npk, .kids = pk };
    } else if (pick < 0 && slot == KB_MAIN && wrow) {  // (s128 wpnmenu) GUESS: "Attack  power [ base + power ]", "Durability  life [ max ]", no Ability line
        cp_eq_stat_line_(&pk[npk], pl1, 0, v->wpn_atk, v->wpn_atk, 8);  // (s128 review: [ Attack ] alone, not + level Str)
        snprintf(pk[npk].text, sizeof pk[npk].text, "Attack");
        npk++;
        cp_eq_stat_line_(&pk[npk], pl2, 0, v->wpn_life, v->wpn_life_max, 27);
        snprintf(pk[npk].text, sizeof pk[npk].text, "Durability");
        npk++;
        pnl[0] = (CsDesc){ CP_SEQ_(287, 288, 289), .nkids = (uint8_t)npk, .kids = pk };
    } else if (pick < 0) {
        cp_eq_stat_line_(&pk[npk++], pl1, 67, kb_str(cur_id), cp_eq_total_str(v, cp_eq_main_str(v, main_id)), 8);
        cp_eq_stat_line_(&pk[npk++], pl2, 68, kb_mag(cur_id), cp_eq_total_mag(v, cp_eq_main_mag(v, main_id)), 27);
        pk[npk++] = (CsDesc){ .x = -95 + g_eq2x, .y = 46 + g_eq2y, CP_SEQ2_(-1, 33, -1), .msg_key = CP_EQ_KEY_ABILITY, .anchor = CS_BOTTOM_LEFT, .flags = CS_A_TEXT_IGNORE_SCALE };  // (s120b ag 33 = label, no brackets)
        pk[npk++] = cp_eq_txt_(124, NULL, cp_eq_ab_key(kb_ability(cur_id)), CS_BOTTOM_CENTER, -100 + g_eq2x, 66 + g_eq2y);
        pnl[0] = (CsDesc){ CP_SEQ_(287, 288, 289), .nkids = (uint8_t)npk, .kids = pk };
    } else {
        int sub = cp_eq_is_sub(slot), isw = pick == CP_EQ_WPN, nw = isw ? 0 : KB_TAB[pick].id;
        int curw = slot == KB_MAIN && wrow;  // (s128 wpnmenu) the weapon is in the hand now / is picked
        int cur_s = curw ? v->wpn_atk : kb_str(cur_id), cur_m = curw ? 0 : kb_mag(cur_id);
        int nw_s = isw ? v->wpn_atk : kb_str(nw), nw_m = isw ? 0 : kb_mag(nw);
        int tot_s = slot == KB_MAIN ? nw_s : cp_eq_main_str(v, main_id), tot_m = slot == KB_MAIN ? nw_m : cp_eq_main_mag(v, main_id);
        cp_eq_cmp_lines_(&pk[0], pl3, 67, cur_s, nw_s, isw && slot == KB_MAIN ? nw_s : cp_eq_total_str(v, tot_s), 26);  // (s128 review: the weapon = [ Attack ])
        cp_eq_cmp_lines_(&pk[2], pl4, 68, cur_m, nw_m, cp_eq_total_mag(v, tot_m), 64);
        if (isw || curw) pk[0].msg_key = 0, snprintf(pk[0].text, sizeof pk[0].text, "Attack");
        pnl[0] = sub ? (CsDesc){ CP_SEQ_(223, 224, 225), .nkids = 4, .kids = pk } : (CsDesc){ CP_SEQ_(213, 214, 215), .nkids = 4, .kids = pk };
    }
    r[0] = cs_tree_add(t, bg_state, 3); r[1] = cs_tree_add(t, title, nt); r[2] = cs_tree_add(t, win, 1);
    r[3] = cs_tree_add(t, help, 1); r[4] = cs_tree_add(t, pnl, 1);
    return 5;
}
static int cp_build_equip_list(CsTree *t, const int kb[KB_SLOTS], const CpVals *v, int slot, int pick, int top, int r[5]) {
    return g_eqlook ? cp_build_equip_look(t, kb, v, slot, pick, top, r) : cp_build_equip_list_old(t, kb, v, slot, pick, top, r);
}

// the PICTURE SLOT (branch b32-itempic draws a Keyblade atlas picture here): *kb_idx = the KB_TAB index the right panel shows
// (state 2: the slot's Keyblade, picker: the highlighted one), rect = x, y, w, h in KH2's 512x416 virtual px; returns 1 when
// a picture is wanted, 0 (and *kb_idx = -1) for the member step / the armor row / g_eqlook 0. [G] rects from the photos
// (panel 323..500 x 72..321 in state 2, 375..500 in the picker; the picture sits at the panel's top above the divider).
static int cp_eq_pic(const CampPage *p, int *kb_idx, float rect[4]) {
    *kb_idx = -1;
    if (!g_eqlook || p->page != 4 || p->sub == 0 || (p->sub == 1 && p->eq_slot >= CP_EQ_ARM)) return 0;
    if (p->sub == 2 && p->eq_pick == CP_EQ_WPN) return 0;  // (s128 wpnmenu) no picture for the BotW weapon (GUESS: the atlas has Keyblades only)
    if (p->sub == 1 && p->eq_slot == KB_MAIN && p->eq_wpnrow) return 0;
    if (p->sub == 2) { *kb_idx = p->eq_pick; rect[0] = 382, rect[1] = 80, rect[2] = 115, rect[3] = 115; return 1; }
    int i = kb_index(kb_slot_id(p->kb, p->eq_slot));
    *kb_idx = i < 0 ? 0 : i;
    rect[0] = 351, rect[1] = 78, rect[2] = 122, rect[3] = 122;  // (s120 review) square: the atlas cell is square (was 152 x 110 = squashed)
    return 1;
}
// (s120 review) the picture's alpha 0..255 [G]: it follows the panel window (root 4's first node): the start anim fades it in
// over CP_EQ_PIC_FADE frames, the end anim (page switch / close) fades it out; 0 when the page tree is not drawn.
#define CP_EQ_PIC_FADE 8
static int cp_eq_pic_alpha(const CampPage *p) {
    if (p->state != CP_OPEN && p->state != CP_SWITCH && p->state != CP_CLOSING) return 0;
    if (p->nroot < 5 || p->root[4] < 0 || p->root[4] >= p->tree.used) return 0;
    const CsNode *r = &p->tree.n[p->root[4]];
    if (r->nkids < 1) return 255;
    const CsNode *w = &p->tree.n[r->kid0];
    float f = (float)w->frame / CP_EQ_PIC_FADE;
    if (f > 1) f = 1;
    if (w->is_end) return 0;
    if (!w->running) return (int)(255 * (1 - f));                        // the end anim plays
    if (w->d.start >= 0 && w->anim == w->d.start) return (int)(255 * f);  // the start anim plays
    return 255;
}

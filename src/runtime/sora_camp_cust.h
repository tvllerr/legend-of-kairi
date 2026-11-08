// sora_camp_cust.h: the KH2 pause menu Customize page trees (slice 4 step 4, plan
// docs/plans/2026-10-30-kh2-pause-menu-slice4-customize.md). Pure, included by sora_camp_page.h after the Abilities page.
// Layout = recon/S84_CUST_PAGE.md (KH2 FM exe FUN_14035aa80 member pick 0x19, FUN_14035ccb0 list 0x1a, FUN_14035a180
// picker 0x1d; row ag table 0x5bc218, tag ag table 0x5bc228). Text keys: camp_text.txt 150..161.
// GUESSes: no font icons (SysText has none, as slice 3) so the tags (ag 282) carry the button as literal text
// (CP_CU_TAG, user ask at the s84 step 5 test) and the rows show the spell name only; the picker is a second slot window (227-229) cascaded over the list at +40, +26 (KH2's 0x1d
// window ag not traced); cursor = the top page's hand 25 + sparks 27-29 on the row; MP Charge / Auto-Reload rows
// left out (user scope: shortcuts only).
#pragma once
#include "sora_cust.h"

#define CP_CU_KEY_MEMBER_HELP 150   // + (who != 0): Sora / party member help
#define CP_CU_KEY_EMPTY 152         // the empty-slot glyphs, also the picker's Remove entry
#define CP_CU_KEY_REMOVE_HELP 153
#define CP_CU_KEY_SPELL 154         // + spell: Fire / Blizzard / Thunder / Cure
#define CP_CU_KEY_SPELL_HELP 158    // + spell
#define CP_CU_KEY_SPELL2 620        // (s147) Magnet / Reflect names (sys.bar 0x0514 / 0x0517), 622.. their helps (0x063d / 0x063f)
#define CP_CU_KEY_SPELL2_HELP 622
static const char *const CP_CU_TAG[CUST_SLOTS] = { "L+Triangle", "L+Square", "L+Cross", "L+Circle" };  // KH2 icons 219/223/222/218
static int cp_cu_name_key(int sp) { return sp >= 0 && sp < 4 ? CP_CU_KEY_SPELL + sp : sp >= 4 && sp < CUST_NSPELL ? CP_CU_KEY_SPELL2 + sp - 4 : CP_CU_KEY_EMPTY; }
static int cp_cu_help_key(int sp) { return sp >= 0 && sp < 4 ? CP_CU_KEY_SPELL_HELP + sp : sp >= 4 && sp < CUST_NSPELL ? CP_CU_KEY_SPELL2_HELP + sp - 4 : 0; }

// (s147) the picker's spells: those the player has (v->sp_hide), KH_SPELLS order -> count; row 0 = Remove, row i = out[i - 1]
static int cp_cu_spells_(const CpVals *v, int out[CUST_NSPELL]) {
    int n = 0;
    for (int sp = 0; sp < CUST_NSPELL; sp++) if (!(v->sp_hide >> sp & 1)) out[n++] = sp;
    return n;
}
static int cp_cu_row_of_(const CpVals *v, int sp) {
    int sl[CUST_NSPELL], n = cp_cu_spells_(v, sl);
    for (int i = 0; i < n; i++) if (sl[i] == sp) return i + 1;
    return 0;
}

static void cp_cu_title_(CsDesc title[2], int who) {
    for (int i = 0; i < 2; i++)
        title[i] = (CsDesc){ CP_SEQ_(75, 76, 77), .stack_index = (int16_t)i, .stack_w = CS_DEFAULT_STACK, .anchor = CS_BOTTOM_LEFT,
                             .flags = CS_A_TEXT_IGNORE_COLOR | CS_A_TEXT_TX | CS_A_CHILD_STACK_H };
    title[0].msg_key = 1;  // "Customize" (0x0451)
    title[1].msg_key = 30 + who;
    cp_name_(&title[1], who);
}

// sub 0 = the member step: r = bg states, title, list (Sora / Donald / Goofy), help. 4 roots.
static void cp_build_cust_member(CsTree *t, int who, const CpVals *v, int r[4]) {
    CsDesc k_munny[1], k_time[1], bg_state[3], title[2], sel_kids[2], list[3], help[1];
    cs_tree_clear(t);
    cp_bg_state_(k_munny, k_time, bg_state, v);
    cp_cu_title_(title, who);
    sel_kids[0] = (CsDesc){ CP_SEQ_(-1, 25, -1), .anchor = CS_BOTTOM_LEFT, .flags = CS_A_NO_CHILD_TX };
    sel_kids[1] = (CsDesc){ CP_SEQ_(27, 28, 29) };
    for (int i = 0; i < cp_nm(); i++) {
        list[i] = (CsDesc){ CP_SEQ_(133, i == who ? 132 : 134, 135), .stack_index = (int16_t)i, .stack_w = 0, .stack_h = CS_DEFAULT_STACK,
                            .msg_key = 30 + i, .flags = CS_A_TEXT_TX | CS_A_CHILD_STACK_H };
        cp_name_(&list[i], i);
        if (i == who) { list[i].nkids = 2; list[i].kids = sel_kids; }
    }
    help[0] = (CsDesc){ .x = 150, .y = 352, CP_SEQ_(-1, 124, -1), .flags = CS_A_TEXT_IGNORE_COLOR, .anchor = CS_BOTTOM_LEFT,
                        .msg_key = CP_CU_KEY_MEMBER_HELP + (who != 0) };
    r[0] = cs_tree_add(t, bg_state, 3); r[1] = cs_tree_add(t, title, 2); r[2] = cs_tree_add(t, list, cp_nm()); r[3] = cs_tree_add(t, help, 1);
}

// sub 1 = the slot list (+ sub 2 = the spell picker over it, pick >= 0): r = bg states, title, slot window, help,
// [picker]. 4 roots, 5 with the picker. Rows at 26 px (FUN_14035d6a0: tag objects at y = 26 * i).
static int cp_build_cust_list(CsTree *t, const int sc[CUST_SLOTS], const CpVals *v, int slot, int pick, int r[5]) {
    CsDesc k_munny[1], k_time[1], bg_state[3], title[2], sel_kids[2], kids[CUST_SLOTS * 2], rk[CUST_SLOTS][3], win[1], help[1];
    CsDesc pkids[1 + CUST_NSPELL], prk[1 + CUST_NSPELL][3], pick_win[1];
    cs_tree_clear(t);
    cp_bg_state_(k_munny, k_time, bg_state, v);
    cp_cu_title_(title, 0);
    sel_kids[0] = (CsDesc){ CP_SEQ_(-1, 25, -1), .anchor = CS_BOTTOM_LEFT, .flags = CS_A_NO_CHILD_TX };
    sel_kids[1] = (CsDesc){ CP_SEQ_(27, 28, 29) };
    int nk = 0;
    for (int i = 0; i < CUST_SLOTS; i++) {
        int m = 0;
        rk[i][m++] = (CsDesc){ CP_SEQ_(-1, 123, -1), .msg_key = cp_cu_name_key(sc[i]), .anchor = CS_BOTTOM_LEFT };
        if (i == slot && pick < 0) { rk[i][m++] = sel_kids[0]; rk[i][m++] = sel_kids[1]; }
        kids[nk++] = (CsDesc){ .y = 26.0f * i, CP_SEQ_(-1, 264, -1), .nkids = (uint8_t)m, .kids = rk[i] };
        kids[nk] = (CsDesc){ .y = 26.0f * i, CP_SEQ_(-1, 282, -1), .flags = CS_A_TEXT_IGNORE_SCALE, .anchor = CS_BOTTOM_LEFT };
        snprintf(kids[nk].text, sizeof kids[nk].text, "%s", CP_CU_TAG[i]);
        nk++;
    }
    win[0] = (CsDesc){ CP_SEQ_(227, 228, 229), .nkids = (uint8_t)nk, .kids = kids };
    int sl[CUST_NSPELL], ns = cp_cu_spells_(v, sl);
    int hk = pick < 0 ? cp_cu_help_key(sc[slot]) : pick == 0 || pick > ns ? CP_CU_KEY_REMOVE_HELP : cp_cu_help_key(sl[pick - 1]);
    help[0] = (CsDesc){ .x = 150, .y = 352, CP_SEQ_(-1, 124, -1), .flags = CS_A_TEXT_IGNORE_COLOR, .anchor = CS_BOTTOM_LEFT, .msg_key = hk };
    r[0] = cs_tree_add(t, bg_state, 3); r[1] = cs_tree_add(t, title, 2); r[2] = cs_tree_add(t, win, 1); r[3] = cs_tree_add(t, help, 1);
    if (pick < 0) return 4;
    for (int i = 0; i <= ns; i++) {  // entry 0 = Remove, 1.. = the owned spells (s147; KH2 FUN_14035c180: each listed one assignable)
        int m = 0;
        prk[i][m++] = (CsDesc){ CP_SEQ_(-1, 123, -1), .msg_key = i ? cp_cu_name_key(sl[i - 1]) : CP_CU_KEY_EMPTY, .anchor = CS_BOTTOM_LEFT };
        if (i == pick) { prk[i][m++] = sel_kids[0]; prk[i][m++] = sel_kids[1]; }
        pkids[i] = (CsDesc){ .y = 26.0f * i, CP_SEQ_(-1, 264, -1), .nkids = (uint8_t)m, .kids = prk[i] };
    }
    pick_win[0] = (CsDesc){ .x = 40, .y = 26, CP_SEQ_(227, 228, 229), .nkids = (uint8_t)(1 + ns), .kids = pkids };
    r[4] = cs_tree_add(t, pick_win, 1);
    return 5;
}

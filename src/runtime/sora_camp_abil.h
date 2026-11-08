// sora_camp_abil.h: the KH2 pause menu Abilities page trees (slice 3 step 5, plan
// docs/plans/2026-10-30-kh2-pause-menu-slice3-abilities.md). Pure, included by sora_camp_page.h after the Status page.
// Layout = recon/S79_HELPER_ABIL_PAGE.md (KH2 FM exe FUN_1403623b0 / 363db0 / 363510 / 362ca0 / 361a00) and the
// scratch campbake AbilSeqs (recon/wip_s79_abil). Text keys: camp_text.txt 80..90, ability name 100 + 2 * AB_TAB index,
// description 101 + 2 * index.
// GUESSes: Drive Form member rows and the Weapons row left out; no font icon (SysText has none) so on = white text
// 123, off = grey 119 (KH2: grey = off and too dear, the icon shows on / off); popup rows at 26 px; AP bars left out
// (no scale in the seq player); cursor = the top page's hand 25 + sparks 27-29 on the row.
#pragma once
#include "sora_abil.h"

#define CP_AB_VIS 9                 // visible list rows (FUN_1402f9140(list, 9))
#define CP_AB_KEY_CAT 80            // + cat: Action / Growth / Support / Weapons
#define CP_AB_KEY_EMPTY 84
#define CP_AB_KEY_POP 85            // + row: Don't Equip / Equip; + 2 = their help
#define CP_AB_KEY_MEMBER_HELP 89
#define CP_AB_KEY_SWITCH 90
// (s91) rows 0..24 = keys 100 + 2 i (name) / +1 (desc); the appended rows 25.. = keys 500 + 2 (i - 25) (camp_text_s91_abil.txt)
static int cp_ab_key_(int id) { int i = ab_idx(id); return i < 0 ? 0 : i < AB_N_OLD ? 100 + 2 * i : 500 + 2 * (i - AB_N_OLD); }
static int cp_ab_name_key(int id) { return cp_ab_key_(id); }
static int cp_ab_desc_key(int id) { int k = cp_ab_key_(id); return k ? k + 1 : 0; }
static int cp_ab_cat(int id) { int i = ab_idx(id); int k = i < 0 ? 0 : AB_TAB[i].kind; return k == 1 || k == 2 ? 0 : k == 3 ? 1 : 2; }
static int cp_ab_cost(int id) { int i = ab_idx(id); return i < 0 ? 0 : AB_TAB[i].ap; }

// display order: category, then the record's place in 03system's item table (VERIFIED offsets, S79 helper abrec:
// 406 Jackpot < 540 Drive Converter < 408 Fire Boost, 414 Defender < 542 Damage Control < 415 Second Chance)
static int cp_ab_rank_(int id) {
    // (s91) REC = the 03system item table order of every row (VERIFIED, recon/S91_HELPER_ABIL.md); Action then Support
    static const uint16_t REC[] = { 82, 137, 271, 267, 273, 262, 263, 559, 264, 562, 265, 266, 269, 560, 270, 272, 561, 268,
                                    385, 386, 568, 387, 388, 198, 138, 158, 539, 390, 391, 392, 393, 394, 395, 396, 397, 400,
                                    401, 402, 403, 405, 406, 540, 408, 409, 410, 411, 414, 542, 415, 416 };
    for (int i = 0; i < (int)(sizeof REC / sizeof REC[0]); i++) if (REC[i] == id) return cp_ab_cat(id) * 100 + i;
    return 999;
}
static void cp_ab_sort(uint16_t *id, int n) {
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && cp_ab_rank_(id[j - 1]) > cp_ab_rank_(id[j]); j--) { uint16_t t = id[j]; id[j] = id[j - 1]; id[j - 1] = t; }
}

static void cp_ab_title_(CsDesc title[3], int who, int pop_cat) {
    for (int i = 0; i < 3; i++)
        title[i] = (CsDesc){ CP_SEQ_(75, 76, 77), .stack_index = (int16_t)i, .stack_w = CS_DEFAULT_STACK, .anchor = CS_BOTTOM_LEFT,
                             .flags = CS_A_TEXT_IGNORE_COLOR | CS_A_TEXT_TX | CS_A_CHILD_STACK_H };
    title[0].msg_key = 2;  // "Abilities" (0x044d)
    title[1].msg_key = 30 + who;
    cp_name_(&title[1], who);
    title[2].msg_key = CP_AB_KEY_CAT + (pop_cat < 0 ? 0 : pop_cat);
}

// sub 0 = the member step: r = bg states, title, list (Sora / Donald / Goofy), equipped grid (140-142), help. 5 roots.
static void cp_build_abil_member(CsTree *t, int who, const CpVals *v, abm_t on, int r[5]) {
    CsDesc k_munny[1], k_time[1], bg_state[3], title[3], sel_kids[2], list[3], cells[20], win[1], help[1];
    cs_tree_clear(t);
    cp_bg_state_(k_munny, k_time, bg_state, v);
    cp_ab_title_(title, who, -1);
    sel_kids[0] = (CsDesc){ CP_SEQ_(-1, 25, -1), .anchor = CS_BOTTOM_LEFT, .flags = CS_A_NO_CHILD_TX };
    sel_kids[1] = (CsDesc){ CP_SEQ_(27, 28, 29) };
    for (int i = 0; i < cp_nm(); i++) {
        list[i] = (CsDesc){ CP_SEQ_(133, i == who ? 132 : 134, 135), .stack_index = (int16_t)i, .stack_w = 0, .stack_h = CS_DEFAULT_STACK,
                            .msg_key = 30 + i, .flags = CS_A_TEXT_TX | CS_A_CHILD_STACK_H };
        cp_name_(&list[i], i);
        if (i == who) { list[i].nkids = 2; list[i].kids = sel_kids; }
    }
    // the hovered member's equipped abilities, 2 x 10 (FUN_1403642a0); Donald / Goofy have none here yet
    static const int16_t CELL_AG[4] = { 137, 139, 143, 150 };
    uint16_t id[AB_N];
    int n = 0, nc = 0;
    if (who == 0) for (int i = 0; i < v->ab_n && n < AB_N; i++) if (ab_on(on, v->ab_id[i])) id[n++] = v->ab_id[i];
    cp_ab_sort(id, n);
    for (int i = 0; i < n && nc < 20; i++, nc++)
        cells[nc] = (CsDesc){ .x = (float)(nc & 1) * 170.0f, .y = (float)(nc >> 1) * 24.0f, CP_SEQ_(-1, CELL_AG[cp_ab_cat(id[i])], -1),
                              .msg_key = cp_ab_name_key(id[i]), .anchor = CS_BOTTOM_LEFT,
                              .flags = CS_A_TEXT_IGNORE_SCALE | CS_A_TEXT_TX | CS_A_CHILD_STACK_H };
    win[0] = (CsDesc){ CP_SEQ_(140, 141, 142), .stack_h = CS_DEFAULT_STACK, .flags = CS_A_TEXT_TX, .nkids = (uint8_t)nc, .kids = cells };
    help[0] = (CsDesc){ .x = 150, .y = 352, CP_SEQ_(-1, 124, -1), .flags = CS_A_TEXT_IGNORE_COLOR, .anchor = CS_BOTTOM_LEFT,
                        .msg_key = CP_AB_KEY_MEMBER_HELP };
    r[0] = cs_tree_add(t, bg_state, 3); r[1] = cs_tree_add(t, title, 2); r[2] = cs_tree_add(t, list, cp_nm());
    r[3] = cs_tree_add(t, win, 1); r[4] = cs_tree_add(t, help, 1);
}

// sub 1 = the list (+ sub 2 = the Don't Equip / Equip popup over it): r = bg states, title, list window (230-232),
// AP box (272-274), prompt (237-239), help, [popup]. 6 roots, 7 with the popup (pop_row >= 0).
static int cp_build_abil_list(CsTree *t, int who, const CpVals *v, abm_t on, int row, int top, int pop_row, int r[7]) {
    CsDesc k_munny[1], k_time[1], bg_state[3], title[3], sel_kids[2], kids[CP_AB_VIS * 2], rk[CP_AB_VIS][4], win[1];
    CsDesc ap[1], prompt[1], help[1], pop_kids[3], pk[2][2], pop[1];
    static const int16_t ROW_AG[4] = { 248, 252, 262, 265 }, HDR_AG[4] = { 276, 278, 280, 283 };
    cs_tree_clear(t);
    cp_bg_state_(k_munny, k_time, bg_state, v);
    uint16_t id[AB_N];
    int n = v->ab_n < AB_N ? v->ab_n : AB_N;
    for (int i = 0; i < n; i++) id[i] = v->ab_id[i];
    cp_ab_sort(id, n);
    int sel = row < n ? id[row] : 0;
    cp_ab_title_(title, who, sel ? cp_ab_cat(sel) : 0);
    sel_kids[0] = (CsDesc){ CP_SEQ_(-1, 25, -1), .anchor = CS_BOTTOM_LEFT, .flags = CS_A_NO_CHILD_TX };
    sel_kids[1] = (CsDesc){ CP_SEQ_(27, 28, 29) };
    int nk = 0;
    if (n == 0) {
        rk[0][0] = (CsDesc){ CP_SEQ_(-1, 123, -1), .msg_key = CP_AB_KEY_EMPTY, .anchor = CS_BOTTOM_LEFT };
        kids[nk++] = (CsDesc){ CP_SEQ_(-1, 236, -1), .nkids = 1, .kids = rk[0] };
    }
    for (int i = top; i < n && i < top + CP_AB_VIS; i++) {
        int k = i - top, c = cp_ab_cat(id[i]), cost = cp_ab_cost(id[i]), m = 0;
        bool is_on = ab_on(on, id[i]);
        rk[k][m++] = (CsDesc){ CP_SEQ_(-1, is_on ? 123 : 119, -1), .msg_key = cp_ab_name_key(id[i]), .anchor = CS_BOTTOM_LEFT };
        if (cost) {
            rk[k][m] = (CsDesc){ CP_SEQ_(-1, 251, -1), .flags = CS_A_TEXT_IGNORE_SCALE, .anchor = CS_BOTTOM_LEFT };
            snprintf(rk[k][m].text, sizeof rk[k][m].text, "%d", cost);
            m++;
        }
        if (i == row && pop_row < 0) { rk[k][m++] = sel_kids[0]; rk[k][m++] = sel_kids[1]; }
        kids[nk++] = (CsDesc){ .y = 26.0f * k, CP_SEQ_(-1, ROW_AG[c], -1), .nkids = (uint8_t)m, .kids = rk[k] };
        if (i == top || cp_ab_cat(id[i - 1]) != c)
            kids[nk++] = (CsDesc){ .y = 26.0f * k, CP_SEQ_(-1, HDR_AG[c], -1), .msg_key = CP_AB_KEY_CAT + c,
                                   .flags = CS_A_TEXT_IGNORE_SCALE, .anchor = CS_BOTTOM_LEFT };
    }
    win[0] = (CsDesc){ CP_SEQ_(230, 231, 232), .nkids = (uint8_t)nk, .kids = kids };
    ap[0] = (CsDesc){ CP_SEQ_(272, 273, 274), .flags = CS_A_TEXT_IGNORE_SCALE, .anchor = CS_BOTTOM_LEFT };
    snprintf(ap[0].text, sizeof ap[0].text, "%d/%d", ab_used(on), v->ab_ap_max);
    prompt[0] = (CsDesc){ CP_SEQ_(237, 238, 239), .msg_key = CP_AB_KEY_SWITCH, .anchor = CS_BOTTOM_LEFT };
    help[0] = (CsDesc){ .x = 150, .y = 352, CP_SEQ_(-1, 124, -1), .flags = CS_A_TEXT_IGNORE_COLOR, .anchor = CS_BOTTOM_LEFT,
                        .msg_key = pop_row >= 0 ? CP_AB_KEY_POP + 2 + pop_row : sel ? cp_ab_desc_key(sel) : 0 };
    int nr = 6;
    r[0] = cs_tree_add(t, bg_state, 3); r[1] = cs_tree_add(t, title, pop_row >= 0 ? 3 : 2); r[2] = cs_tree_add(t, win, 1);
    r[3] = cs_tree_add(t, ap, 1); r[4] = cs_tree_add(t, prompt, 1); r[5] = cs_tree_add(t, help, 1);
    if (pop_row >= 0 && sel) {  // FUN_140361a00: window / title text / row ags by category (Weapons not listed here)
        static const int16_t WIN[3][3] = { { 153, 154, 155 }, { 171, 172, 173 }, { 180, 181, 182 } };
        static const int16_t TTL[3] = { 161, 179, 188 }, ROW[3] = { 157, 175, 184 };
        int c = cp_ab_cat(sel);
        pop_kids[0] = (CsDesc){ CP_SEQ_(-1, TTL[c], -1), .msg_key = cp_ab_name_key(sel), .anchor = CS_BOTTOM_LEFT, .flags = CS_A_TEXT_IGNORE_SCALE };
        for (int i = 0; i < 2; i++) {
            pk[i][0] = sel_kids[0]; pk[i][1] = sel_kids[1];
            pop_kids[1 + i] = (CsDesc){ .y = 26.0f * (i + 1), CP_SEQ_(-1, ROW[c], -1), .msg_key = CP_AB_KEY_POP + i,
                                        .anchor = CS_BOTTOM_LEFT, .flags = CS_A_TEXT_TX | CS_A_TEXT_IGNORE_SCALE, .nkids = (uint8_t)(i == pop_row ? 2 : 0), .kids = pk[i] };
        }
        pop[0] = (CsDesc){ CP_SEQ_(WIN[c][0], WIN[c][1], WIN[c][2]), .nkids = 3, .kids = pop_kids };
        r[6] = cs_tree_add(t, pop, 1);
        nr = 7;
    }
    return nr;
}
// can row id be picked (A) / turned on (Triangle)? = FUN_140361620 / FUN_140364720: on, or the cost fits what is left
static bool cp_ab_fits(const CpVals *v, abm_t on, int id) { return ab_on(on, id) || ab_used(on) + cp_ab_cost(id) <= v->ab_ap_max; }

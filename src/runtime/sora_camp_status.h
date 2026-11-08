// sora_camp_status.h: the KH2 pause menu Status page tree (slice 2). Pure, included by sora_camp_page.h after
// cp_build_top. Port of campbake Program.cs StatusSeqs() = OpenKh.Game MenuStatus + MenuState's title tab.
#pragma once

// "Next LV" = the EXP still needed for lv + 1 (GUESS: KH2 shows the remainder; LV1 = 40 either way), 0 at LV99.
// next_exp = sl_next(lv), the EXP that reaches lv + 1.
static int cp_next_left(int lv, uint32_t exp, uint32_t next_exp) { return lv >= 99 || exp >= next_exp ? 0 : (int)(next_exp - exp); }

// r = the 4 roots: bg states (Munny / Time), title tab, Sora / Donald / Goofy list, stats window. -1 in r = pool full.
static void cp_build_status(CsTree *t, int who, const CpStat *st, const CpVals *v, int r[4]) {
    CsDesc k_munny[1], k_time[1], bg_state[3], title_kids[2], title[1], sel_kids[2], list[3], val[15], rows[15], win[1];
    cs_tree_clear(t);
    cp_bg_state_(k_munny, k_time, bg_state, v);
    title_kids[0] = (CsDesc){ CP_SEQ_(-1, -1, -1) };
    title_kids[1] = (CsDesc){ CP_SEQ_(27, 28, 29) };
    title[0] = (CsDesc){ CP_SEQ_(75, 76, 77), .stack_w = CS_DEFAULT_STACK, .msg_key = 50, .anchor = CS_BOTTOM_LEFT,
                         .flags = CS_A_TEXT_IGNORE_COLOR | CS_A_TEXT_TX | CS_A_CHILD_STACK_H, .nkids = 2, .kids = title_kids };
    sel_kids[0] = (CsDesc){ CP_SEQ_(-1, 25, -1), .anchor = CS_BOTTOM_LEFT, .flags = CS_A_NO_CHILD_TX };
    sel_kids[1] = (CsDesc){ CP_SEQ_(27, 28, 29) };
    for (int i = 0; i < cp_nm(); i++) {
        list[i] = (CsDesc){ CP_SEQ_(133, i == who ? 132 : 134, 135), .stack_index = (int16_t)i, .stack_w = 0, .stack_h = CS_DEFAULT_STACK,
                            .msg_key = 30 + i, .flags = CS_A_TEXT_TX | CS_A_CHILD_STACK_H };
        cp_name_(&list[i], i);
        if (i == who) { list[i].nkids = 2; list[i].kids = sel_kids; }
    }
    // Row(x, y, key, seq, value): campbake column x (170 px), line y (24 px); the value is a right-anchored 124 child.
    // Sora = the 15 MenuStatus rows. GUESS: KH2 party Status rows = Donald / Goofy drop Experience, Next LV, Drive Gauge
    // and Bonus LV, and the left column closes up (LV / HP / MP / AP on lines 0-3)
    char num[15][24];
    cp_num_(num[0], 24, "%d", st->lv);
    cp_num_(num[1], 24, "%d", st->exp);
    cp_num_(num[2], 24, "%d", st->next);
    if (st->hp < 0 || st->hp_max < 0) snprintf(num[3], 24, "--"); else snprintf(num[3], 24, "%d/%d", st->hp, st->hp_max);
    if (st->mp < 0 || st->mp_max < 0) snprintf(num[4], 24, "--"); else snprintf(num[4], 24, "%d/%d", st->mp, st->mp_max);
    if (st->ap_used < 0 || st->ap_max < 0) snprintf(num[5], 24, "--"); else snprintf(num[5], 24, "%d/%d", st->ap_used, st->ap_max);
    cp_num_(num[6], 24, "%d", st->drive);
    cp_num_(num[7], 24, "%d", st->str);
    cp_num_(num[8], 24, "%d", st->mag);
    cp_num_(num[9], 24, "%d", st->def);
    for (int k = 0; k < 4; k++) cp_num_(num[10 + k], 24, "%d%%", st->res[k]);
    cp_num_(num[14], 24, "%d", st->bonus_lv);
    static const struct { int8_t x, y, py; uint8_t key, seq; bool sora_only; } R[15] = {  // py = the party line
        { 0, 0, 0, 60, 137, false }, { 0, 1, 0, 61, 137, true }, { 0, 2, 0, 62, 137, true }, { 0, 3, 1, 63, 143, false },
        { 0, 4, 2, 64, 143, false }, { 0, 5, 3, 65, 143, false }, { 0, 6, 0, 66, 143, true }, { 1, 0, 0, 67, 150, false },
        { 1, 1, 1, 68, 150, false }, { 1, 2, 2, 69, 150, false }, { 1, 3, 3, 70, 139, false }, { 1, 4, 4, 71, 139, false },
        { 1, 5, 5, 72, 139, false }, { 1, 6, 6, 73, 139, false }, { 0, 7, 0, 74, 144, true } };
    int n = 0;
    for (int i = 0; i < 15; i++) {
        if (who != 0 && R[i].sora_only) continue;
        val[n] = (CsDesc){ CP_SEQ_(-1, 124, -1), .stack_w = CS_DEFAULT_STACK, .stack_h = CS_DEFAULT_STACK, .flags = CS_A_TEXT_TX, .anchor = CS_BOTTOM_RIGHT };
        snprintf(val[n].text, sizeof val[n].text, "%s", num[i]);
        rows[n] = (CsDesc){ .x = R[i].x * 170.0f, .y = (who == 0 ? R[i].y : R[i].py) * 24.0f, CP_SEQ_(-1, R[i].seq, -1), .msg_key = R[i].key,
                            .anchor = CS_BOTTOM_LEFT, .flags = CS_A_TEXT_IGNORE_SCALE | CS_A_TEXT_TX | CS_A_CHILD_STACK_H, .nkids = 1, .kids = &val[n] };
        n++;
    }
    win[0] = (CsDesc){ CP_SEQ_(140, 141, 142), .stack_h = CS_DEFAULT_STACK, .flags = CS_A_TEXT_TX, .nkids = (uint8_t)n, .kids = rows };
    r[0] = cs_tree_add(t, bg_state, 3); r[1] = cs_tree_add(t, title, 1); r[2] = cs_tree_add(t, list, cp_nm()); r[3] = cs_tree_add(t, win, 1);
}

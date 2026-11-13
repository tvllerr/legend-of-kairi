// camp_test.c: host tests for sora_camp_seq.h + sora_camp_page.h (KH2 pause menu slice 1, Tasks 3-5).
// Goldens = OpenKH's own renderer through campbake (D:/SoraBotW/data/hud/camp_golden, format in the plan).
// usage: camp_test.exe <camp_seq.bin> <golden dir>   (sh D:/SoraBotW/recon/make_camp_test.sh)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "sora_camp_page.h"

static CampSeq S;
static const char *GOLD;
static int fails;
static SoraHudQuad QB[4096];

static void cs_dump(FILE *f, const CsOut *o) {  // the plan's quad dump format (= campbake Render.cs DumpQuad)
    for (int i = 0; i < o->n; i++) {
        const SoraHudQuad *q = &o->q[i];
        fprintf(f, "%d %d", q->tex, q->blend);
        for (int k = 0; k < 4; k++) fprintf(f, " %.3f %.3f", q->x[k], q->y[k]);
        for (int k = 0; k < 4; k++) fprintf(f, " %.3f %.3f", q->u[k], q->v[k]);
        for (int k = 0; k < 4; k++) fprintf(f, " %08X", q->col[k]);
        for (int k = 0; k < 4; k++) fprintf(f, " %.3f", q->wrap[k]);
        fprintf(f, "\n");
    }
}

// compare against a golden with campdiff.py's tolerances (pos / uv / wrap +-0.02, colour +-1 per channel, same count + order);
// writes build/camp_out_<name>.txt for campdiff on a fail
static bool golden_eq(const char *name, const CsOut *o) {
    // (s140 dbgmenu) the top-page goldens are OpenKH 5-row renders: with the 6th row (Debug Menu) they differ by its quads -> skipped, not failed
    if (CP_ROWS != 5 && (!strncmp(name, "top_", 4) || !strncmp(name, "close_", 6))) { printf("  golden %s skipped (CP_ROWS %d, goldens are 5-row)\n", name, CP_ROWS); return true; }
    char path[512];
    snprintf(path, sizeof path, "build/camp_out_%s.txt", name);
    FILE *w = fopen(path, "w"); if (w) { cs_dump(w, o); fclose(w); }
    snprintf(path, sizeof path, "%s/%s.txt", GOLD, name);
    FILE *f = fopen(path, "r");
    if (!f) { printf("  no golden %s\n", path); return false; }
    char line[1024]; int n = 0; bool ok = true;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '\n' || !line[0]) continue;
        if (n >= o->n) { n++; continue; }
        const SoraHudQuad *q = &o->q[n];
        int tex, blend; float g[16 + 4]; unsigned c[4];
        int got = sscanf(line, "%d %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %x %x %x %x %f %f %f %f",
                         &tex, &blend, &g[0], &g[1], &g[2], &g[3], &g[4], &g[5], &g[6], &g[7], &g[8], &g[9], &g[10], &g[11],
                         &g[12], &g[13], &g[14], &g[15], &c[0], &c[1], &c[2], &c[3], &g[16], &g[17], &g[18], &g[19]);
        if (got != 26) { printf("  bad golden line %d\n", n); ok = false; break; }
        float m[20];
        for (int k = 0; k < 4; k++) { m[2 * k] = q->x[k]; m[2 * k + 1] = q->y[k]; m[8 + 2 * k] = q->u[k]; m[9 + 2 * k] = q->v[k]; m[16 + k] = q->wrap[k]; }
        bool lok = tex == q->tex && blend == q->blend;
        for (int k = 0; k < 20; k++) if (!(fabsf(m[k] - g[k]) <= 0.02f)) lok = false;
        for (int k = 0; k < 4; k++)
            for (int ch = 0; ch < 4; ch++) {
                int a = (q->col[k] >> (8 * ch)) & 0xFF, b = (c[k] >> (8 * ch)) & 0xFF;
                if (abs(a - b) > 1) lok = false;
            }
        if (!lok && ok) printf("  quad %d differs (first)\n", n);
        if (!lok) ok = false;
        n++;
    }
    fclose(f);
    if (n != o->n) { printf("  count %d, golden %d\n", o->n, n); ok = false; }
    return ok;
}

#define CHECK(c) do { if (!(c)) { printf("  check failed: %s (line %d)\n", #c, __LINE__); return false; } } while (0)
static CsOut out_(void) { CsOut o = { QB, 0, (int)(sizeof QB / sizeof QB[0]) }; return o; }

static bool test_load(void) {
    CHECK(S.n_spr == 136 && S.n_grp == 136 && S.n_ag == 360 && S.n_l2g == 3);
    CHECK(cs_group_end(&S, 132) > 0);
    return true;
}
static bool test_ag133_f5(void) { CsOut o = out_(); cs_draw_group(&S, &o, 133, 5, 0, 0, 1, NULL); return golden_eq("ag133_f5", &o); }
static bool test_ag132_f20(void) { CsOut o = out_(); cs_draw_group(&S, &o, 132, 20, 0, 0, 1, NULL); return golden_eq("ag132_f20", &o); }
static bool test_l2_0_f6(void) { CsOut o = out_(); cs_draw_layout(&S, &o, 0, 6); return golden_eq("l2_0_f6", &o); }
static bool test_l2_1_f30(void) { CsOut o = out_(); cs_draw_layout(&S, &o, 1, 30); return golden_eq("l2_1_f30", &o); }
static uint8_t *BUF; static size_t BUFN;
static bool test_load_rejects_truncated(void) { CampSeq t; CHECK(!cs_load(&t, BUF, 100)); CHECK(!cs_load(&t, BUF, BUFN - 1)); return true; }
static bool test_load_rejects_bad_msg(void) {  // a msg entry longer than the file
    uint8_t *b = malloc(BUFN); memcpy(b, BUF, BUFN);
    uint16_t big = 0xFFFF; memcpy(b + S.h->msg.off + 2, &big, 2);
    CampSeq t; bool ok = cs_load(&t, b, BUFN);
    free(b);
    CHECK(!ok);
    return true;
}
static bool test_layout_last(void) {  // campbake: layout L2 0 is done at f16 (-> L2 1)
    CHECK(!cs_layout_last(&S, 0, 15));
    CHECK(cs_layout_last(&S, 0, 16));
    return true;
}
static bool test_msg(void) {
    int len = 0;
    const uint8_t *m = cs_msg(&S, 10, &len);  // camp_text.txt 10 = lit "Equipment"
    CHECK(m && len == 9);
    CHECK(m[0] == cs_ascii(&S, 'E') && cs_ascii(&S, 'E') != 0);
    CHECK(!cs_msg(&S, 0x7FFF, &len) && len == 0);
    return true;
}

// ---- slice 2 Task 1: the Status page strings (camp_text.txt 50 title, 60..74 window rows) ----
static bool test_status_text_keys(void) {
    int l3 = 0, l50 = 0, len = 0;
    const uint8_t *m3 = cs_msg(&S, 3, &l3), *m50 = cs_msg(&S, 50, &l50);
    CHECK(m3 && l3 > 0 && m50 && l50 == l3 && !memcmp(m3, m50, l3));  // the title tab = the top row "Status"
    for (int k = 60; k <= 74; k++) { CHECK(cs_msg(&S, k, &len) && len > 0); }
    return true;
}

// ---- Task 4 / 5: the top page tree (sora_camp_page.h cp_build_top), golden values Munny 1234 / Time 12:34 ----
static CsTree T;
static SoraHudQuad QS[4096];
static const CpVals GV = { .munny = 1234, .time_s = 12 * 3600 + 34 * 60, .lv = { 1, 1, 1 },
                           .hp = { 20, 20, 30 }, .hp_max = { 20, 20, 30 }, .mp = { 100, 100, 100 }, .mp_max = { 100, 100, 100 } };

// = Quads(): frame 0 Begin, then per frame layout frame++ and Update(1/60); every frame draws (state moves in Draw);
// the last frame's quads land in *o
static int R[5];
static int run_top(CsOut *o, int row, int frames, int close_at) {
    cs_tree_clear(&T);
    int *r = R; cp_build_top(&T, row, &GV, r);
    for (int k = 0; k < 5; k++) if (r[k] < 0) return -1;
    for (int k = 0; k < 5; k++) cs_begin(&T, r[k]);
    int l2 = 0, lf = 0, maxq = 0;
    for (int f = 0; f <= frames; f++) {
        if (f > 0) { lf++; for (int k = 0; k < 5; k++) cs_update(&T, r[k], 1 / 60.0); }
        if (f == close_at) { for (int k = 0; k < 5; k++) cs_end(&T, r[k]); l2 = 2; lf = 0; }
        if (l2 == 0 && cs_layout_last(&S, 0, lf)) { lf = 0; l2 = 1; }
        CsOut sc = { QS, 0, (int)(sizeof QS / sizeof QS[0]) };
        CsOut *d = f == frames ? o : &sc;
        d->n = 0;
        cs_draw_layout(&S, d, l2, lf);
        for (int k = 0; k < 5; k++) cs_draw(&T, &S, d, r[k], 0, 0);
        if (d->n > maxq) maxq = d->n;
    }
    return maxq;
}
static bool top_(const char *name, int row, int frames, int close_at) { CsOut o = out_(); CHECK(run_top(&o, row, frames, close_at) >= 0); return golden_eq(name, &o); }
static bool test_top_r0_f6(void) { return top_("top_r0_f6", 0, 6, -1); }
static bool test_top_r0_f12(void) { return top_("top_r0_f12", 0, 12, -1); }
static bool test_top_r0_f120(void) { return top_("top_r0_f120", 0, 120, -1); }
static bool test_top_r3_f120(void) { return top_("top_r3_f120", 3, 120, -1); }
static bool test_top_r4_f120(void) { return top_("top_r4_f120", 4, 120, -1); }
static bool test_close_f126(void) { return top_("close_f126", 0, 126, 120); }
static bool test_close_f140(void) {
    if (!top_("close_f140", 0, 140, 120)) return false;
    for (int k = 0; k < 5; k++) CHECK(cs_is_end(&T, R[k]));  // campbake: ends 11111 at close f140
    return true;
}
static bool test_max_quads(void) {
    CsOut o = out_();
    int m = run_top(&o, 0, 140, 120);
    printf("  max quads over frames 0-140 (row 0, close at 120): %d\n", m);
    CHECK(m > 0 && m < 2048);
    return true;
}
// ---- slice 2 Task 2: the Status tree (sora_camp_status.h cp_build_status), golden = campbake statusquads 30 0 ----
static const CpStat ST_LV1 = { 1, 0, 40, 20, 20, 100, 100, 0, 2, 3, 1, 5, 7, 2, { 0, 0, 0, 0 } };
static int RS[4];
static int run_status(CsOut *o, int who, const CpStat *st, int frames) {  // = Quads(status): as run_top over 4 roots
    cp_build_status(&T, who, st, &GV, RS);
    for (int k = 0; k < 4; k++) if (RS[k] < 0) return -1;
    for (int k = 0; k < 4; k++) cs_begin(&T, RS[k]);
    int l2 = 0, lf = 0, maxq = 0;
    for (int f = 0; f <= frames; f++) {
        if (f > 0) { lf++; for (int k = 0; k < 4; k++) cs_update(&T, RS[k], 1 / 60.0); }
        if (l2 == 0 && cs_layout_last(&S, 0, lf)) { lf = 0; l2 = 1; }
        CsOut sc = { QS, 0, (int)(sizeof QS / sizeof QS[0]) };
        CsOut *d = f == frames ? o : &sc;
        d->n = 0;
        cs_draw_layout(&S, d, l2, lf);
        for (int k = 0; k < 4; k++) cs_draw(&T, &S, d, RS[k], 0, 0);
        if (d->n > maxq) maxq = d->n;
    }
    return maxq;
}
static const CsNode *status_win_(void) { return &T.n[T.n[RS[3]].kid0]; }  // r[3] root -> the window (140-142)
static const char *status_val_(int i) { const CsNode *w = status_win_(); return T.n[T.n[w->kid0 + i].kid0].d.text; }
static bool test_status_tree_matches_golden(void) {
    CsOut o = out_();
    CHECK(run_status(&o, 0, &ST_LV1, 30) >= 0);
    return golden_eq("status_sora_lv1", &o);
}
static bool test_status_lv99_values(void) {
    const CpStat st = { 99, 2875578, 0, 150, 150, 200, 200, 0, 66, 9, 25, 55, 55, 53, { 0, 0, 0, 0 } };  // exp = SL_LV[98].exp
    cp_build_status(&T, 0, &st, &GV, RS);
    for (int k = 0; k < 4; k++) CHECK(RS[k] >= 0);
    static const char *want[15] = { "99", "2875578", "0", "150/150", "200/200", "0/66", "9", "55", "55", "53", "0%", "0%", "0%", "0%", "25" };
    CHECK(status_win_()->nkids == 15);
    for (int i = 0; i < 15; i++) if (strcmp(status_val_(i), want[i])) { printf("  row %d: '%s' want '%s'\n", i, status_val_(i), want[i]); return false; }
    return true;
}
static bool test_status_next_left(void) {  // review s79d: "Next LV" = EXP still needed (GUESS: KH2 shows the remainder), 0 at LV99
    CHECK(cp_next_left(1, 0, 40) == 40);
    CHECK(cp_next_left(1, 25, 40) == 15);
    CHECK(cp_next_left(5, 300, 200) == 0);
    CHECK(cp_next_left(99, 2875578, 2875578) == 0);
    CHECK(cp_next_left(99, 0, 2875578) == 0);
    return true;
}
static bool test_status_max_quads(void) {
    CsOut o = out_();
    int m = run_status(&o, 0, &ST_LV1, 120);
    printf("  max quads over Status frames 0-120: %d\n", m);
    CHECK(m > 0 && m < 2048);
    return true;
}
static bool test_status_party_rows(void) {
    CpStat st = ST_LV1; st.str = -1;
    cp_build_status(&T, 1, &st, &GV, RS);
    for (int k = 0; k < 4; k++) CHECK(RS[k] >= 0);
    const CsNode *w = status_win_();
    CHECK(w->nkids == 11);
    bool str_dash = false;
    for (int i = 0; i < w->nkids; i++) {
        int key = T.n[w->kid0 + i].d.msg_key;
        CHECK(key != 61 && key != 62 && key != 66 && key != 74);
        if (key == 67) str_dash = !strcmp(status_val_(i), "--");
    }
    CHECK(str_dash);
    return true;
}

static bool test_text_digits(void) {
    CsOut o = out_();
    uint8_t b[5]; const char *s = "12:34";
    for (int i = 0; i < 5; i++) b[i] = cs_ascii(&S, s[i]);
    float w = 0, h = 0; const float white[4] = { 1, 1, 1, 1 };
    cs_text(&S, &o, b, 5, 10, 20, 1.0f, white, &w, &h);
    CHECK(o.n == 5);
    for (int i = 0; i < 5; i++) {
        int cell = b[i] - 0x20;
        CHECK(b[i] >= 0x20 && o.q[i].tex == 1);
        CHECK(o.q[i].u[0] == (float)(cell % 28 * 18) && o.q[i].v[0] == (float)(cell / 28 * 24));
        CHECK(o.q[i].u[3] == o.q[i].u[0] + 18 && o.q[i].v[3] == o.q[i].v[0] + 24);
        CHECK(o.q[i].col[0] == 0x80808080u);
    }
    CHECK(o.q[0].x[0] == 10 && o.q[0].y[0] == 20 && w > 0 && fabsf(h - 24) < 0.01f);
    CsOut m = out_(); float w2 = 0, h2 = 0;
    cs_text(&S, NULL, b, 5, 0, 0, 1.0f, white, &w2, &h2);  // measure only
    CHECK(m.n == 0 && fabsf(w2 - w) < 0.001f);
    return true;
}

// ---- Task 5: the page state machine ----
static CampPage P;
static CpOut PO;
static CsOut PQ;
static const CpVals *PV = &GV;  // (slice 3 abil tests point it at GA)
static void pt_(uint32_t pause, uint64_t press) { PQ = out_(); cp_tick(&P, &S, &PQ, pause, press, 0, PV, &PO); }
static bool page_open_(void) {  // request + pause 2 until the layout is in L2 1 (input live)
    memset(&P, 0, sizeof P);
    cp_request(&P, 0, true);
    for (int i = 0; i < 40 && !(P.state == CP_OPEN && P.layout_l2 == 1); i++) pt_(2, 0);
    return P.state == CP_OPEN && P.layout_l2 == 1;
}
static bool test_page_draw_matches_top_r0_f120(void) {  // frames 0..120 = 121 presents
    memset(&P, 0, sizeof P);
    cp_request(&P, 0, true);
    for (int i = 0; i <= 120; i++) { pt_(2, 0); if (i == 0) CHECK(PO.se == CP_SE_OPEN && PO.pass_plus && PO.log && !strcmp(PO.log, "open")); }
    CHECK(P.state == CP_OPEN && PO.draw && PO.eat && PQ.n > 1);
    CHECK(PQ.q[0].tex == 2 && PQ.q[0].col[0] == 0x80000000u && PQ.q[0].x[0] < -85 && PQ.q[0].x[3] > 597);
    CsOut rest = { PQ.q + 1, PQ.n - 1, PQ.cap - 1 };
    return golden_eq("top_r0_f120", &rest);
}
static bool test_page_open_needs_unpaused(void) {
    memset(&P, 0, sizeof P);
    cp_request(&P, 2, true); CHECK(P.state == CP_CLOSED);
    cp_request(&P, 0, false); CHECK(P.state == CP_CLOSED);
    cp_request(&P, 0, true); CHECK(P.state == CP_WAIT_PAUSE);
    return true;
}
static bool test_page_open_times_out(void) {
    memset(&P, 0, sizeof P);
    cp_request(&P, 0, true);
    for (int i = 0; i < 30; i++) { pt_(0, 0); CHECK(P.state == CP_WAIT_PAUSE && PO.eat); }
    pt_(0, 0);
    CHECK(P.state == CP_CLOSED && PO.eat == 0 && PO.draw == 0 && PO.log && !strcmp(PO.log, "abort no-pause"));
    return true;
}
static bool test_page_cursor_wraps(void) {
    CHECK(page_open_());
    pt_(2, CP_BTN_UP); CHECK(P.row == CP_ROWS - 1 && PO.se == CP_SE_CURSOR && PO.log_row == CP_ROWS - 1);
    pt_(2, CP_BTN_DOWN); CHECK(P.row == 0 && PO.se == CP_SE_CURSOR && PO.log_row == 0);
    int moves = 0;  // stick held down: 1 step, then after 18 presents every 6
    for (int i = 0; i < 30; i++) { PQ = out_(); cp_tick(&P, &S, &PQ, 2, 0, -1.0f, &GV, &PO); if (PO.se == CP_SE_CURSOR) moves++; }
    CHECK(moves == 1 + 2);  // held 0, 18, 24
    return true;
}
static bool test_page_error_on_unbuilt_rows(void) {  // (slice 2: row 3 = Status opens, see test_status_*)
    CHECK(page_open_());
    for (int r = 0; r < 3; r++) {  // (slice 3: row 1 = Abilities opens, see test_abil_*; slice 4: row 2 = Customize, test_cust_*; slice 5: row 0 = Equipment, test_equip_*)
        CHECK(P.row == r);
        if (r != CP_ROW_EQUIP && r != CP_ROW_ABIL && r != CP_ROW_CUST) { pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_ERROR && P.state == CP_OPEN); }
        pt_(2, CP_BTN_DOWN);
    }
    return true;
}
static bool test_page_ignores_input_while_animating(void) {
    memset(&P, 0, sizeof P);
    cp_request(&P, 0, true);
    pt_(2, CP_BTN_DOWN); CHECK(P.row == 0 && PO.se == CP_SE_OPEN);
    pt_(2, CP_BTN_DOWN); CHECK(P.row == 0 && PO.se == CP_SE_NONE && P.layout_l2 == 0);
    pt_(2, CP_BTN_B); CHECK(P.state == CP_OPEN);
    pt_(2, CP_BTN_A); CHECK(P.state == CP_OPEN && PO.se == CP_SE_NONE);
    return true;
}
static bool page_to_pulse_(void) {
    if (!page_open_()) return false;
    pt_(2, CP_BTN_B | CP_BTN_A);  // B and A together = B
    CHECK(P.state == CP_CLOSING && PO.se == CP_SE_CANCEL && PO.log && !strcmp(PO.log, "close") && P.layout_l2 == 2);
    for (int i = 0; i < 200 && P.state == CP_CLOSING; i++) { pt_(2, 0); CHECK(PO.eat && PO.draw); }
    CHECK(P.state == CP_PULSE);
    return true;
}
static bool test_page_close_then_pulse_until_unpaused(void) {
    CHECK(page_to_pulse_());
    static const uint8_t want[8] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    for (int i = 0; i < 8; i++) {
        pt_(2, 0);
        CHECK(P.state == CP_PULSE && PO.eat && PO.draw && PO.pulse_plus == want[i]);
        CHECK(PQ.n == 1 && PQ.q[0].tex == 2);  // the backdrop only
    }
    pt_(0, 0);
    CHECK(P.state == CP_CLOSED && PO.eat == 0 && PO.log && !strcmp(PO.log, "closed"));
    return true;
}
static bool test_page_close_gives_up(void) {
    CHECK(page_to_pulse_());
    for (int i = 0; i < 60; i++) { pt_(2, 0); CHECK(P.state == CP_PULSE); }
    pt_(2, 0);
    CHECK(P.state == CP_CLOSED && PO.eat == 0 && PO.log && !strcmp(PO.log, "abort close-timeout"));
    return true;
}
static bool test_page_handover_returns_closed(void) {
    CHECK(page_open_());
    for (int i = CP_ROWS; i > CP_ROW_BOTW; i--) pt_(2, CP_BTN_UP);  // wrap up to the BotW Menu row (Debug Menu sits below it)
    CHECK(P.row == CP_ROW_BOTW);
    pt_(2, CP_BTN_A);
    CHECK(P.state == CP_HANDOVER && PO.se == CP_SE_DECIDE && PO.eat == 0 && PO.draw == 0 && !strcmp(PO.log, "handover"));
    pt_(2, 0); CHECK(P.state == CP_HANDOVER && PO.eat == 0 && PO.draw == 0);
    pt_(0, 0); CHECK(P.state == CP_CLOSED);
    cp_request(&P, 0, true); CHECK(P.state == CP_WAIT_PAUSE);
    pt_(2, 0); CHECK(P.state == CP_OPEN && PO.se == CP_SE_OPEN);
    return true;
}

// ---- slice 2 Task 3: the Status sub-state ----
static bool page_status_(void) {  // top page -> row 3 -> A -> the Status page's first frame
    if (!page_open_()) return false;
    for (int i = 0; i < 3; i++) pt_(2, CP_BTN_DOWN);
    CHECK(P.row == CP_ROW_STATUS);
    pt_(2, CP_BTN_A);
    CHECK(PO.se == CP_SE_DECIDE && PO.log && !strcmp(PO.log, "status") && P.state == CP_SWITCH && PO.eat && PO.draw);
    for (int i = 0; i < 60 && P.state == CP_SWITCH; i++) pt_(2, 0);
    CHECK(P.state == CP_OPEN && P.page == 1 && P.who == 0 && cp_built_(&P));
    return true;
}
static bool test_status_opens_on_decide(void) {
    CHECK(page_status_());
    pt_(2, 0); CHECK(PO.draw && PO.eat && PQ.n > 1);
    return true;
}
static bool status_back_(int ticks) {  // B on the Status page, then back on the top page (row 3) within ticks
    pt_(2, CP_BTN_B);
    CHECK(PO.se == CP_SE_CANCEL && PO.log && !strcmp(PO.log, "status back") && P.state == CP_SWITCH);
    for (int i = 0; i < ticks && P.state == CP_SWITCH; i++) pt_(2, 0);
    CHECK(P.state == CP_OPEN && P.page == 0 && P.row == CP_ROW_STATUS && cp_built_(&P));
    return true;
}
static bool test_status_b_returns_row3(void) {
    CHECK(page_status_());
    for (int i = 0; i < 60; i++) pt_(2, 0);  // fully in
    CHECK(status_back_(60));
    pt_(2, CP_BTN_DOWN); CHECK(P.row == CP_ROW_BOTW && PO.se == CP_SE_CURSOR);  // the top page takes input again
    return true;
}
static bool test_status_who_wraps_no_treefull(void) {
    CHECK(page_status_());
    pt_(2, CP_BTN_UP); CHECK(P.who == 2 && PO.se == CP_SE_CURSOR && PO.log_row == 2);
    pt_(2, CP_BTN_DOWN); CHECK(P.who == 0);
    for (int i = 0; i < 20; i++) { pt_(2, CP_BTN_DOWN); CHECK(cp_built_(&P) && P.state == CP_OPEN && PO.se == CP_SE_CURSOR); }
    CHECK(P.who == 20 % 3 && P.page == 1);
    return true;
}
static bool test_status_plus_closes(void) {
    CHECK(page_status_());
    pt_(2, CP_BTN_PLUS);
    CHECK(P.state == CP_CLOSING && PO.se == CP_SE_CANCEL && PO.log && !strcmp(PO.log, "close") && P.layout_l2 == 2);
    for (int i = 0; i < 200 && P.state == CP_CLOSING; i++) pt_(2, 0);
    CHECK(P.state == CP_PULSE);
    pt_(0, 0); CHECK(P.state == CP_CLOSED);
    cp_request(&P, 0, true); pt_(2, 0); CHECK(P.state == CP_OPEN && P.page == 0 && P.row == 0);  // reopens on the top page
    return true;
}
static bool test_status_b_during_open_anim(void) { CHECK(page_status_()); return status_back_(60); }
static bool test_status_other_rows_still_error(void) {
    CHECK(page_status_());
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_NONE && P.state == CP_OPEN && P.page == 1);  // A on Status = nothing
    CHECK(status_back_(60));
    for (int i = 0; i < CP_ROWS - 1; i++) pt_(2, CP_BTN_DOWN);  // row 3 -> 2
    for (int r = 2; r >= 0; r--) { CHECK(P.row == r); if (r != CP_ROW_EQUIP && r != CP_ROW_ABIL && r != CP_ROW_CUST) { pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_ERROR && P.page == 0); } pt_(2, CP_BTN_UP); }
    return true;
}

// ---- slice 3 step 5: the Abilities page (sora_camp_abil.h + the page 2 sub-states) ----
static CpVals GA;
static void ga_(int ap_max, uint32_t on, int n) {  // n learned of a LV-40-like set, unsorted
    static const uint16_t L[] = { 401, 411, 403, 408, 390, 402, 396, 405, 416, 409, 392, 394, 414, 385, 386, 391, 393, 395,
                                  397, 400, 406, 410, 415, 540, 542 };
    GA = GV; GA.ab_n = n; GA.ab_ap_max = ap_max; GA.ab_on = on;
    for (int i = 0; i < n; i++) GA.ab_id[i] = L[i];
    PV = &GA;
}
static bool test_abil_text_keys(void) {
    int len = 0;
    for (int k = 80; k <= 90; k++) { CHECK(cs_msg(&S, k, &len) && len > 0); }
    for (int i = 0; i < AB_N; i++) { CHECK(cs_msg(&S, cp_ab_name_key(AB_TAB[i].id), &len) && len > 0); CHECK(cs_msg(&S, cp_ab_desc_key(AB_TAB[i].id), &len) && len > 0); }
    int l2 = 0; const uint8_t *m2 = cs_msg(&S, 2, &l2);
    CHECK(m2 && l2 > 0);
    return true;
}
static bool test_abil_sort_record_order(void) {
    uint16_t id[] = { 408, 385, 542, 390, 415, 540, 406 }, want[] = { 385, 390, 406, 540, 408, 542, 415 };
    cp_ab_sort(id, 7);
    for (int i = 0; i < 7; i++) CHECK(id[i] == want[i]);
    return true;
}
static bool abil_open_(void) {  // top -> row 1 -> A -> the Abilities member step, then Sora's list
    if (!page_open_()) return false;
    pt_(2, CP_BTN_DOWN); CHECK(P.row == CP_ROW_ABIL);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_DECIDE && PO.log && !strcmp(PO.log, "abilities") && P.state == CP_SWITCH);
    for (int i = 0; i < 60 && P.state == CP_SWITCH; i++) pt_(2, 0);
    CHECK(P.state == CP_OPEN && P.page == 2 && P.sub == 0 && P.nroot == 5 && cp_built_(&P));
    for (int i = 0; i < 40; i++) pt_(2, 0);
    pt_(2, CP_BTN_DOWN); CHECK(P.who == 1 && PO.se == CP_SE_CURSOR);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_ERROR && P.sub == 0);  // Donald: no list yet
    pt_(2, CP_BTN_UP); CHECK(P.who == 0);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_DECIDE && P.sub == 1 && P.nroot == 6 && cp_built_(&P));
    return true;
}
static bool test_abil_toggle_popup_back(void) {
    ga_(28, 0, 14);  // nothing on; row 0 = Auto Valor (Action, 1 AP)
    CHECK(abil_open_());
    CHECK(cp_ab_sel_(PV, 0) == 385 && cp_ab_sel_(PV, 1) == 390);
    pt_(2, CP_BTN_X); CHECK(PO.se == CP_SE_DECIDE && PO.ab_set && PO.ab_id == 385 && PO.ab_on == ab_bit(385) && P.sub == 1);
    pt_(2, CP_BTN_DOWN); CHECK(P.ab_row == 1 && PO.se == CP_SE_CURSOR);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_DECIDE && P.sub == 2 && P.pop_row == 0 && P.nroot == 7 && cp_built_(&P));
    pt_(2, CP_BTN_UP); CHECK(P.pop_row == 0 && PO.se == CP_SE_NONE);  // clamp
    pt_(2, CP_BTN_DOWN); CHECK(P.pop_row == 1 && PO.se == CP_SE_CURSOR);
    pt_(2, CP_BTN_A); CHECK(PO.ab_set && PO.ab_id == 390 && PO.ab_on == (ab_bit(385) | ab_bit(390)) && P.sub == 1 && P.nroot == 6);
    pt_(2, CP_BTN_A); CHECK(P.sub == 2 && P.pop_row == 1);  // on -> the cursor starts on Equip
    pt_(2, CP_BTN_B); CHECK(PO.se == CP_SE_CANCEL && P.sub == 1 && !PO.ab_set);
    pt_(2, CP_BTN_ZR); CHECK(PO.se == CP_SE_ERROR);
    pt_(2, CP_BTN_B); CHECK(PO.se == CP_SE_CANCEL && P.sub == 0 && P.nroot == 5);
    pt_(2, CP_BTN_B); CHECK(PO.se == CP_SE_CANCEL && PO.log && !strcmp(PO.log, "abilities back") && P.state == CP_SWITCH);
    for (int i = 0; i < 60 && P.state == CP_SWITCH; i++) pt_(2, 0);
    CHECK(P.state == CP_OPEN && P.page == 0 && P.row == CP_ROW_ABIL && P.ab_on == (ab_bit(385) | ab_bit(390)));
    PV = &GV;
    return true;
}
static bool test_abil_ap_short_error(void) {
    ga_(3, 0, 14);  // 3 AP: Combo Boost (5) can't go on
    CHECK(abil_open_());
    pt_(2, CP_BTN_DOWN); CHECK(cp_ab_sel_(PV, P.ab_row) == 390);
    pt_(2, CP_BTN_X); CHECK(PO.se == CP_SE_ERROR && !PO.ab_set);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_ERROR && P.sub == 1);
    PV = &GV;
    return true;
}
static bool test_abil_scroll_all_learned(void) {
    ga_(99, 0, 25);
    CHECK(abil_open_());
    for (int i = 0; i < 30; i++) { pt_(2, CP_BTN_DOWN); CHECK(cp_built_(&P) && P.state == CP_OPEN); }
    CHECK(P.ab_row == 24 && P.ab_top == 24 - CP_AB_VIS + 1);
    pt_(2, CP_BTN_A); CHECK(P.sub == 2 && cp_built_(&P));  // popup over a full list: no tree-full
    for (int i = 0; i < 40; i++) pt_(2, 0);
    CHECK(PQ.n > 1 && PQ.n < 2048);
    PV = &GV;
    return true;
}
static bool test_abil_empty_list(void) {
    ga_(2, 0, 0);
    CHECK(abil_open_());
    pt_(2, CP_BTN_X); CHECK(PO.se == CP_SE_ERROR);
    pt_(2, CP_BTN_DOWN); CHECK(P.ab_row == 0);
    PV = &GV;
    return true;
}

// ---- slice 4 step 4: the Customize page (sora_camp_cust.h + the page 3 sub-states) ----
static CpVals GC;
static void gc_(int s0, int s1, int s2, int s3) { GC = GV; GC.sc[0] = s0; GC.sc[1] = s1; GC.sc[2] = s2; GC.sc[3] = s3; PV = &GC; }
static bool test_cust_text_keys(void) {
    int len = 0;
    for (int k = 150; k <= 161; k++) { CHECK(cs_msg(&S, k, &len) && len > 0); }
    CHECK(cs_msg(&S, 1, &len) && len > 0);  // title "Customize"
    return true;
}
static bool cust_open_(void) {  // top -> row 2 -> A -> the Customize member step (Donald = error), then Sora's slot list
    if (!page_open_()) return false;
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_DOWN); CHECK(P.row == CP_ROW_CUST);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_DECIDE && PO.log && !strcmp(PO.log, "customize") && P.state == CP_SWITCH);
    for (int i = 0; i < 60 && P.state == CP_SWITCH; i++) pt_(2, 0);
    CHECK(P.state == CP_OPEN && P.page == 3 && P.sub == 0 && P.nroot == 4 && cp_built_(&P));
    for (int i = 0; i < 40; i++) pt_(2, 0);
    pt_(2, CP_BTN_DOWN); CHECK(P.who == 1 && PO.se == CP_SE_CURSOR);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_ERROR && P.sub == 0);  // Donald: no party settings (user scope)
    pt_(2, CP_BTN_UP); CHECK(P.who == 0);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_DECIDE && P.sub == 1 && P.cu_slot == 0 && P.nroot == 4 && cp_built_(&P));
    return true;
}
static const CsNode *cust_win_(void) { return &P.tree.n[P.tree.n[P.root[2]].kid0]; }  // root 2 -> the slot window
static bool test_cust_set_spell(void) {
    gc_(0, 1, 2, 3);
    CHECK(cust_open_());
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_DOWN); CHECK(P.cu_slot == 2 && PO.se == CP_SE_CURSOR);
    pt_(2, CP_BTN_ZL); CHECK(PO.se == CP_SE_ERROR && P.sub == 1);  // one member with a list
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_DECIDE && P.sub == 2 && P.cu_pick == 3 && P.nroot == 5 && cp_built_(&P));  // on Thunder
    pt_(2, CP_BTN_DOWN); CHECK(P.cu_pick == 4 && PO.se == CP_SE_CURSOR);
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_DOWN); CHECK(P.cu_pick == 6 && PO.se == CP_SE_CURSOR);  // (s147) Magnet 5, Reflect 6
    pt_(2, CP_BTN_DOWN); CHECK(P.cu_pick == 6 && PO.se == CP_SE_NONE);  // clamp
    pt_(2, CP_BTN_UP); pt_(2, CP_BTN_UP); CHECK(P.cu_pick == 4);  // back on Cure
    pt_(2, CP_BTN_A);
    CHECK(PO.se == CP_SE_DECIDE && PO.sc_set && PO.sc_slot == 2 && PO.sc_spell == 3 && P.sub == 1 && P.cu_slot == 2 && P.nroot == 4);
    CHECK(P.sc[2] == 3 && P.sc[3] == 3);  // duplicates allowed (KH2 FUN_140359b70)
    pt_(2, CP_BTN_A); CHECK(P.sub == 2 && P.cu_pick == 4);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_DECIDE && !PO.sc_set && P.sub == 1);  // same spell: nothing to apply
    PV = &GV;
    return true;
}
static bool test_cust_remove_and_empty(void) {
    gc_(0, CUST_EMPTY, 2, 3);
    CHECK(cust_open_());
    pt_(2, CP_BTN_DOWN); CHECK(P.cu_slot == 1);
    pt_(2, CP_BTN_A); CHECK(P.sub == 2 && P.cu_pick == 0);  // empty -> the cursor starts on Remove
    pt_(2, CP_BTN_UP); CHECK(P.cu_pick == 0 && PO.se == CP_SE_NONE);
    pt_(2, CP_BTN_B); CHECK(PO.se == CP_SE_CANCEL && P.sub == 1 && P.cu_slot == 1 && !PO.sc_set);
    pt_(2, CP_BTN_UP); CHECK(P.cu_slot == 0);
    pt_(2, CP_BTN_UP); CHECK(P.cu_slot == 0 && PO.se == CP_SE_NONE);  // list clamps (GUESS)
    pt_(2, CP_BTN_A); CHECK(P.cu_pick == 1);
    pt_(2, CP_BTN_UP); pt_(2, CP_BTN_A); CHECK(PO.sc_set && PO.sc_slot == 0 && PO.sc_spell == CUST_EMPTY && P.sc[0] == CUST_EMPTY);
    // the slot list rows: 4 rows, row text = the spell name (154 + spell) or the empty glyphs (152)
    const CsNode *w = cust_win_();
    int keys[4] = { 0 }, nrow = 0;
    for (int i = 0; i < w->nkids && nrow < 4; i++) {
        const CsNode *row = &P.tree.n[w->kid0 + i];
        if (row->d.loop != 264) continue;
        keys[nrow++] = P.tree.n[row->kid0].d.msg_key;
    }
    CHECK(nrow == 4 && keys[0] == 152 && keys[1] == 152 && keys[2] == 156 && keys[3] == 157);
    PV = &GV;
    return true;
}
static bool test_cust_back_keeps_row(void) {
    gc_(0, 1, 2, 3);
    CHECK(cust_open_());
    pt_(2, CP_BTN_B); CHECK(PO.se == CP_SE_CANCEL && P.sub == 0 && P.nroot == 4);
    pt_(2, CP_BTN_B); CHECK(PO.se == CP_SE_CANCEL && PO.log && !strcmp(PO.log, "customize back") && P.state == CP_SWITCH);
    for (int i = 0; i < 60 && P.state == CP_SWITCH; i++) pt_(2, 0);
    CHECK(P.state == CP_OPEN && P.page == 0 && P.row == CP_ROW_CUST);
    PV = &GV;
    return true;
}
// the slot tags (ag 282) say which button each row is, in slot order, and fit the tag (hangs 83 px left of the row)
static bool test_cust_tags_name_buttons(void) {
    static const char *want[4] = { "L+Triangle", "L+Square", "L+Cross", "L+Circle" };
    gc_(0, 1, 2, 3);
    CHECK(cust_open_());
    const CsNode *w = cust_win_();
    int nt = 0;
    for (int i = 0; i < w->nkids; i++) {
        const CsNode *tg = &P.tree.n[w->kid0 + i];
        if (tg->d.loop != 282) continue;
        CHECK(nt < 4 && !strcmp(tg->d.text, want[nt]) && tg->d.y == 26.0f * nt);
        uint8_t b[24]; int n = 0;
        for (const char *c = tg->d.text; *c; c++) { uint8_t g = cs_ascii(&S, *c); CHECK(g); b[n++] = g; }
        float tw, th;
        cs_text(&S, NULL, b, n, 0, 0, 0.7f, NULL, &tw, &th);
        printf("  tag %d \"%s\" w %.1f\n", nt, tg->d.text, tw);
        CHECK(tw <= 83.0f);
        nt++;
    }
    CHECK(nt == 4);
    PV = &GV;
    return true;
}
static bool test_cust_picker_draws(void) {
    gc_(0, 1, 2, 3);
    CHECK(cust_open_());
    pt_(2, CP_BTN_A);
    for (int i = 0; i < 40; i++) pt_(2, 0);
    CHECK(P.sub == 2 && PQ.n > 1 && PQ.n < 2048);
    PV = &GV;
    return true;
}

// ---- slice 5 step 6: the Equipment page (sora_camp_equip.h + the page 4 sub-states) ----
static CpVals GE;
static void ge_(int main_kb, int valor_kb) { GE = GV; GE.kb[KB_MAIN] = main_kb; GE.kb[KB_VALOR] = valor_kb; GE.kb_own = 0xf; PV = &GE; }  // (s89 kb: the 4 slice-5 blades owned)
static bool test_equip_text_keys(void) {
    int len = 0;
    for (int k = 162; k <= 173; k++) { CHECK(cs_msg(&S, k, &len) && len > 0); }
    CHECK(cs_msg(&S, CP_EQ_KEY_TITLE, &len) && len > 0 && cs_msg(&S, CP_EQ_KEY_MEMBER_HELP, &len) && len > 0);
    return true;
}
static bool equip_open_(void) {  // top row 0 -> A -> the Equipment member step (Donald = error), then Sora's slot list
    if (!page_open_()) return false;
    CHECK(P.row == CP_ROW_EQUIP);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_DECIDE && PO.log && !strcmp(PO.log, "equipment") && P.state == CP_SWITCH);
    for (int i = 0; i < 60 && P.state == CP_SWITCH; i++) pt_(2, 0);
    CHECK(P.state == CP_OPEN && P.page == 4 && P.sub == 0 && P.nroot == 4 && cp_built_(&P));
    CHECK(P.tree.n[P.tree.n[P.root[1]].kid0].d.msg_key == CP_EQ_KEY_TITLE);
    CHECK(P.tree.n[P.tree.n[P.root[3]].kid0].d.msg_key == CP_EQ_KEY_MEMBER_HELP);
    for (int i = 0; i < 40; i++) pt_(2, 0);
    pt_(2, CP_BTN_DOWN); CHECK(P.who == 1 && PO.se == CP_SE_CURSOR);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_ERROR && P.sub == 0);  // Donald: Sora's Keyblades only
    pt_(2, CP_BTN_UP); CHECK(P.who == 0);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_DECIDE && P.sub == 1 && P.eq_slot == KB_MAIN && P.nroot == 5 && cp_built_(&P));  // (s120 eqlook) + the right panel root
    return true;
}
// the slot list rows: name key of each slot's Keyblade (264 row's first kid) + the slot tag (282) key
static bool equip_rows_n_(int want_name[KB_SLOTS], int rows) {
    const CsNode *w = cust_win_();
    int nr = 0, nt = 0;
    for (int i = 0; i < w->nkids; i++) {
        const CsNode *k = &P.tree.n[w->kid0 + i];
        if (k->d.loop == cp_eq_row_ag(nr < 2 ? nr : KB_MASTER + nr - 2)) { CHECK(nr < rows && P.tree.n[k->kid0].d.msg_key == want_name[nr]); nr++; }
        else if (k->d.loop == cp_eq_tag_ag(nt < 2 ? nt : KB_MASTER + nt - 2)) { CHECK(nt < rows && k->d.msg_key == cp_eq_row_key(nt < 2 ? nt : KB_MASTER + nt - 2) && k->d.y == 26.0f * nt); nt++; }
        else CHECK(false);
    }
    CHECK(nr == rows && nt == rows);
    return true;
}
static bool equip_rows_(int want_name[KB_SLOTS]) { return equip_rows_n_(want_name, 2); }  // (s91 mfkb: no Master / Final row before the forms)
static bool test_equip_set_main(void) {
    ge_(KB_KINGDOM_KEY, KB_STAR_SEEKER);
    CHECK(equip_open_());
    int w0[KB_SLOTS] = { KB_TEXT, KB_TEXT + 2 };
    CHECK(equip_rows_(w0));
    CHECK(P.tree.n[P.tree.n[P.root[3]].kid0].d.msg_key == kb_text_key(kb_index(KB_KINGDOM_KEY)) + 1);  // (s120b) the Keyblade's description
    pt_(2, CP_BTN_ZR); CHECK(PO.se == CP_SE_ERROR && P.sub == 1);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_DECIDE && P.sub == 2 && P.eq_pick == 0 && P.nroot == 5 && cp_built_(&P));  // on KK
    CHECK(P.tree.n[P.tree.n[P.root[3]].kid0].d.msg_key == KB_TEXT + 1);  // help = the Keyblade's desc
    for (int i = 0; i < 3; i++) pt_(2, CP_BTN_DOWN);
    CHECK(P.eq_pick == 3 && PO.se == CP_SE_CURSOR && P.tree.n[P.tree.n[P.root[3]].kid0].d.msg_key == KB_TEXT + 7);
    pt_(2, CP_BTN_DOWN); CHECK(P.eq_pick == 3 && PO.se == CP_SE_NONE);  // clamp
    pt_(2, CP_BTN_A);
    CHECK(PO.se == CP_SE_DECIDE && PO.kb_set && PO.kb_slot == KB_MAIN && PO.kb_id == 0x2b && P.sub == 1 && P.eq_slot == KB_MAIN);
    CHECK(P.kb[KB_MAIN] == 0x2b && P.kb[KB_VALOR] == KB_STAR_SEEKER);
    int w1[KB_SLOTS] = { KB_TEXT + 6, KB_TEXT + 2 };
    CHECK(equip_rows_(w1));
    pt_(2, CP_BTN_A); CHECK(P.sub == 2 && P.eq_pick == 3);  // the cursor starts on the slot's Keyblade
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_DECIDE && !PO.kb_set && P.sub == 1);  // same one: nothing to apply
    PV = &GV;
    return true;
}
static bool test_equip_valor_one_copy(void) {
    ge_(0x2b, KB_STAR_SEEKER);
    CHECK(equip_open_());
    pt_(2, CP_BTN_DOWN); CHECK(P.eq_slot == KB_VALOR && PO.se == CP_SE_CURSOR);
    CHECK(P.tree.n[P.tree.n[P.root[3]].kid0].d.msg_key == kb_text_key(kb_index(KB_STAR_SEEKER)) + 1);  // (s120b) the Valor Keyblade's description
    pt_(2, CP_BTN_DOWN); CHECK(P.eq_slot == KB_VALOR && PO.se == CP_SE_NONE);  // clamp
    pt_(2, CP_BTN_A); CHECK(P.sub == 2 && P.eq_pick == 1);  // on Star Seeker
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_DOWN); CHECK(P.eq_pick == 3);
    g_kbswap = 0;  // (s118) the old rule: main holds it -> error
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_ERROR && !PO.kb_set && P.sub == 2 && P.kb[KB_VALOR] == KB_STAR_SEEKER);  // main holds it
    g_kbswap = 1;  // (s118 kbswap) now it swaps: Valor takes Oblivion, main takes Star Seeker, back to the slot list
    pt_(2, CP_BTN_A);
    CHECK(PO.se == CP_SE_DECIDE && PO.kb_set && PO.kb_swap == KB_MAIN + 1 && PO.kb_slot == KB_VALOR && PO.kb_id == 0x2b &&
          P.kb[KB_VALOR] == 0x2b && P.kb[KB_MAIN] == KB_STAR_SEEKER && P.sub == 1);
    pt_(2, CP_BTN_A); CHECK(P.sub == 2);  // reopen the picker on Valor (cursor on its Oblivion)
    pt_(2, CP_BTN_UP); pt_(2, CP_BTN_A);
    CHECK(PO.kb_set && !PO.kb_swap && PO.kb_slot == KB_VALOR && PO.kb_id == 0x2a && P.kb[KB_VALOR] == 0x2a && P.eq_slot == KB_VALOR);
    PV = &GV;
    return true;
}
static bool test_equip_back_keeps_row(void) {
    ge_(KB_KINGDOM_KEY, KB_STAR_SEEKER);
    CHECK(equip_open_());
    pt_(2, CP_BTN_A); pt_(2, CP_BTN_B); CHECK(PO.se == CP_SE_CANCEL && P.sub == 1 && !PO.kb_set);
    pt_(2, CP_BTN_B); CHECK(PO.se == CP_SE_CANCEL && P.sub == 0 && P.nroot == 4);
    pt_(2, CP_BTN_B); CHECK(PO.se == CP_SE_CANCEL && PO.log && !strcmp(PO.log, "equipment back") && P.state == CP_SWITCH);
    for (int i = 0; i < 60 && P.state == CP_SWITCH; i++) pt_(2, 0);
    CHECK(P.state == CP_OPEN && P.page == 0 && P.row == CP_ROW_EQUIP);
    PV = &GV;
    return true;
}
static bool test_equip_picker_draws(void) {
    ge_(KB_KINGDOM_KEY, KB_STAR_SEEKER);
    CHECK(equip_open_());
    pt_(2, CP_BTN_A);
    for (int i = 0; i < 40; i++) pt_(2, 0);
    CHECK(P.sub == 2 && PQ.n > 1 && PQ.n < 2048);
    PV = &GV;
    return true;
}

// ---- s120 eqlook: the vanilla look (ags per kind, armor rows, panel values, picker columns, off switch, picture slot) ----
static const CsNode *eq_root_(int r) { return &P.tree.n[P.tree.n[P.root[r]].kid0]; }
static bool eq_has_text_(const CsNode *n, const char *lit, int depth) {  // a literal text anywhere under n
    if (n->d.text[0] && !strcmp(n->d.text, lit)) return true;
    if (depth > 0) for (int i = 0; i < n->nkids; i++) if (eq_has_text_(&P.tree.n[n->kid0 + i], lit, depth - 1)) return true;
    return false;
}
static bool test_eqlook_rows_armor(void) {
    ge_(KB_KINGDOM_KEY, KB_STAR_SEEKER);
    GE.arm_n = 3; snprintf(GE.arm_name[0], 24, "Hylian Hood"); snprintf(GE.arm_name[1], 24, "Hylian Tunic"); snprintf(GE.arm_name[2], 24, "Hylian Trousers");
    GE.arm_def[0] = 3; GE.arm_def[1] = 4; GE.arm_def[2] = 2;
    CHECK(equip_open_());
    const CsNode *w = eq_root_(2);
    CHECK(w->d.start == 227 && w->d.loop == 228 && w->d.end == 229 && w->nkids == 2 + 2 + 3 + 1);  // 2 Keyblade rows + tags, 3 armor rows + 1 tag
    int ag[8], n = 0;
    for (int i = 0; i < w->nkids; i++) ag[n++] = P.tree.n[w->kid0 + i].d.loop;
    CHECK(ag[0] == 265 && ag[1] == 283 && ag[2] == 266 && ag[3] == 284 && ag[4] == 252 && ag[5] == 278 && ag[6] == 252 && ag[7] == 252);
    CHECK(eq_has_text_(&P.tree.n[w->kid0 + 4], "Hylian Hood", 1));
    // walk Weapons -> Valor -> Head -> Body -> Legs, clamp; A on armor = error, no picker
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_DOWN); CHECK(P.eq_slot == CP_EQ_ARM);
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_DOWN); CHECK(P.eq_slot == CP_EQ_ARM + 2 && PO.se == CP_SE_CURSOR);
    pt_(2, CP_BTN_DOWN); CHECK(P.eq_slot == CP_EQ_ARM + 2 && PO.se == CP_SE_NONE);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_ERROR && P.sub == 1);
    // the panel on the Legs row: Defense 2 [ 9 ], no Ability line
    const CsNode *pn = eq_root_(4);
    CHECK(pn->d.start == 287 && pn->nkids == 1 && pn->d.end == 289);
    const CsNode *l = &P.tree.n[pn->kid0];
    CHECK(l->d.loop == 32 && l->d.msg_key == 69 && !strcmp(P.tree.n[l->kid0].d.text, "9") && !strcmp(P.tree.n[l->kid0 + 1].d.text, "2"));
    float rc[4]; int ki;
    CHECK(!cp_eq_pic(&P, &ki, rc) && ki == -1);
    PV = &GV;
    return true;
}
// (s128 wpnmenu) the BotW weapon: the Weapons row + the picker's top entry; Keyblade picks / the weapon pick -> CpOut.wpn_set
static bool test_wpn_picker(void) {
    ge_(KB_KINGDOM_KEY, KB_STAR_SEEKER);
    GE.eq_base_str = 10; GE.eq_base_mag = 20;
    GE.wpn_on = 1; GE.wpn_use = 1; GE.wpn_atk = 12; GE.wpn_life = 7; GE.wpn_life_max = 20; snprintf(GE.wpn_name, sizeof GE.wpn_name, "Hylian Sword");
    CHECK(equip_open_());
    CHECK(P.eq_wpnrow == 1);
    const CsNode *pn = eq_root_(4);
    CHECK(pn->nkids == 2 && !strcmp(P.tree.n[pn->kid0].d.text, "Attack") && !strcmp(P.tree.n[P.tree.n[pn->kid0].kid0 + 1].d.text, "12"));
    CHECK(!strcmp(P.tree.n[pn->kid0 + 1].d.text, "Durability"));
    float rc[4]; int ki;
    CHECK(!cp_eq_pic(&P, &ki, rc));  // no Keyblade picture for the weapon
    pt_(2, CP_BTN_A); CHECK(P.sub == 2 && P.eq_pick == CP_EQ_WPN);  // the cursor starts on the weapon
    CHECK(eq_root_(2)->nkids == 10);  // 4 owned Keyblades + the weapon, 2 nodes each
    pt_(2, CP_BTN_DOWN); CHECK(P.eq_pick == 0);  // first Keyblade = one place down
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_DECIDE && PO.wpn_set == 2 && P.sub == 1 && P.eq_wpnrow == 0);
    pt_(2, CP_BTN_A); CHECK(P.sub == 2 && P.eq_pick == CP_EQ_WPN);  // (the view still says wpn_use 1)
    pt_(2, CP_BTN_UP); CHECK(P.eq_pick == CP_EQ_WPN);  // clamped at the top
    pt_(2, CP_BTN_A); CHECK(PO.wpn_set == 1 && P.sub == 1 && P.eq_wpnrow == 1);
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_A); CHECK(P.sub == 2 && P.eq_slot == KB_VALOR && P.eq_pick != CP_EQ_WPN && eq_root_(2)->nkids == 8);  // no weapon entry in the Valor picker
    PV = &GV;
    return true;
}
static bool test_eqlook_panel_keyblade(void) {
    ge_(KB_KINGDOM_KEY, KB_STAR_SEEKER);
    GE.eq_base_str = 10; GE.eq_base_mag = 20;
    CHECK(equip_open_());
    const CsNode *pn = eq_root_(4);
    CHECK(pn->d.start == 287 && pn->nkids == 4);
    const CsNode *s = &P.tree.n[pn->kid0], *m = &P.tree.n[pn->kid0 + 1], *a = &P.tree.n[pn->kid0 + 2], *an = &P.tree.n[pn->kid0 + 3];
    CHECK(s->d.msg_key == 67 && !strcmp(P.tree.n[s->kid0].d.text, "13") && !strcmp(P.tree.n[s->kid0 + 1].d.text, "3"));  // KK str 3
    CHECK(m->d.msg_key == 68 && !strcmp(P.tree.n[m->kid0].d.text, "21") && !strcmp(P.tree.n[m->kid0 + 1].d.text, "1"));
    CHECK(a->d.msg_key == CP_EQ_KEY_ABILITY && an->d.msg_key == cp_eq_ab_key(KB_TAB[0].ability));
    float rc[4]; int ki;
    CHECK(cp_eq_pic(&P, &ki, rc) && ki == 0 && rc[2] > 0 && rc[3] > 0);
    CHECK(rc[2] == rc[3] && cp_eq_pic_alpha(&P) < 255);  // (s120 review) square cell; the panel start anim fades it in
    for (int i = 0; i < CP_EQ_PIC_FADE + 2; i++) pt_(2, 0);
    CHECK(P.state == CP_OPEN && cp_eq_pic_alpha(&P) == 255);
    { int st = P.state; P.state = CP_PULSE; CHECK(cp_eq_pic_alpha(&P) == 0); P.state = st; }  // no picture over the bare backdrop
    pt_(2, CP_BTN_DOWN);  // Valor row: Star Seeker, the total keeps the MAIN Keyblade
    CHECK(eq_root_(4)->anim == 288 && cp_eq_pic_alpha(&P) == 255);  // (s120 review) a row move keeps the panel open (no start replay)
    CHECK(cp_eq_pic(&P, &ki, rc) && ki == 1);
    pn = eq_root_(4);
    CHECK(!strcmp(P.tree.n[P.tree.n[pn->kid0].kid0].d.text, "13"));
    // Weapons picker: the Oblivion (str 6 mag 2) highlighted vs KK 3 / 1: red? no, higher = cyan 118; total 10 + 6
    pt_(2, CP_BTN_UP); pt_(2, CP_BTN_A); CHECK(P.sub == 2);
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_DOWN);
    CHECK(P.eq_pick == 3 && cp_eq_pic(&P, &ki, rc) && ki == 3);
    const CsNode *pw = eq_root_(2);
    CHECK(pw->d.start == 207 && pw->d.end == 209 && pw->nkids == 8 && P.tree.n[pw->kid0].d.loop == 211 && P.tree.n[pw->kid0 + 1].d.loop == 212);
    CHECK(P.tree.n[P.tree.n[pw->kid0 + 1].kid0].d.msg_key == cp_eq_ab_key(KB_TAB[0].ability));  // the ability column
    CHECK(P.tree.n[P.root[1]].nkids == 3);  // Equipment > Sora > Weapons
    pn = eq_root_(4);
    CHECK(pn->d.start == 213 && pn->nkids == 4);
    const CsNode *sl = &P.tree.n[pn->kid0], *sa = &P.tree.n[pn->kid0 + 1];
    CHECK(sl->d.loop == 37 && !strcmp(P.tree.n[sl->kid0].d.text, "16") && P.tree.n[sl->kid0].d.loop == 118);
    CHECK(sa->d.loop == 38 && !strcmp(P.tree.n[sa->kid0].d.text, "3") &&  // (s120b: ag 38 draws the arrow itself)
          !strcmp(P.tree.n[sa->kid0 + 1].d.text, "6") && P.tree.n[sa->kid0 + 1].d.loop == 118);
    // Valor picker: its own window / panel ags (217-219 / 223-225), rows 221 + 222
    pt_(2, CP_BTN_B); pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_A); CHECK(P.sub == 2 && P.eq_slot == KB_VALOR);
    pw = eq_root_(2); pn = eq_root_(4);
    CHECK(pw->d.start == 217 && pn->d.start == 223 && P.tree.n[pw->kid0].d.loop == 221 && P.tree.n[pw->kid0 + 1].d.loop == 222);
    CHECK(P.tree.n[P.root[1]].nkids == 3);
    PV = &GV;
    return true;
}
static bool test_eqlook_off_switch(void) {
    ge_(KB_KINGDOM_KEY, KB_STAR_SEEKER);
    GE.arm_n = 1; snprintf(GE.arm_name[0], 24, "Hylian Hood"); GE.arm_def[0] = 3;
    g_eqlook = 0;
    CHECK(equip_open_() == false);  // the old page has 4 roots (equip_open_ checks the new 5)
    CHECK(P.nroot == 4);
    const CsNode *w = cust_win_();
    CHECK(w->nkids == 4 && P.tree.n[w->kid0].d.loop == 264 && P.tree.n[w->kid0 + 1].d.loop == 282);  // old: Customize's rows, no armor
    g_eqlook = 1;
    PV = &GV;
    return true;
}
static bool test_eqlook_text_keys(void) {
    int len = 0;
    for (int i = 0; i < KB_N; i++) { if (!KB_TAB[i].ability) { CHECK(cp_eq_ab_key(0) == 0); continue; } int k = cp_eq_ab_key(KB_TAB[i].ability); CHECK(k && cs_msg(&S, k, &len) && len > 0); }  // every Keyblade with an ability has its name (s214: our own two have none yet = no line)
    CHECK(cs_msg(&S, CP_EQ_KEY_ARMOR, &len) && len > 0 && cs_msg(&S, CP_EQ_KEY_ABILITY, &len) && len > 0);
    return true;
}

// ---- s89 kb: the owned-only picker + its scroll, the new Keyblades' text keys ----
static bool test_equip_kb_text_keys(void) {
    int len = 0;
    for (int i = 0; i < KB_N; i++) { CHECK(cs_msg(&S, kb_text_key(i), &len) && len > 0 && cs_msg(&S, kb_text_key(i) + 1, &len) && len > 0); }
    CHECK(kb_text_key(3) == 172 && kb_text_key(4) == 400 && kb_text_key(23) == 438 && kb_text_key(KB_N - 1) == 450 + 2 * (KB_N - 25));  // (s214: 450.. after row 24)
    return true;
}
// owned: KK, Star Seeker (always), Fenrir (index 20) only: the picker lists 3, down from KK skips to SS then Fenrir
static bool test_equip_owned_only(void) {
    ge_(KB_KINGDOM_KEY, KB_STAR_SEEKER);
    GE.kb_own = 1u << 20;  // (KK + SS come from kb_own_fix)
    CHECK(equip_open_());
    pt_(2, CP_BTN_A); CHECK(P.sub == 2 && P.eq_pick == 0 && P.eq_top == 0);
    const CsNode *pw = &P.tree.n[P.tree.n[P.root[2]].kid0];  // (s120 eqlook) the picker window = root 2, 2 kids per row (name + ability column)
    CHECK(pw->nkids == 6);
    pt_(2, CP_BTN_DOWN); CHECK(P.eq_pick == 1);
    pt_(2, CP_BTN_DOWN); CHECK(P.eq_pick == 20 && PO.se == CP_SE_CURSOR);
    CHECK(P.tree.n[P.tree.n[P.root[3]].kid0].d.msg_key == kb_text_key(20) + 1);  // help = Fenrir's desc
    pt_(2, CP_BTN_DOWN); CHECK(P.eq_pick == 20 && PO.se == CP_SE_NONE);  // clamp at the last owned one
    pt_(2, CP_BTN_A); CHECK(PO.kb_set && PO.kb_id == 0x1f3 && P.kb[KB_MAIN] == 0x1f3);
    PV = &GV;
    return true;
}
// (s91 mfkb) Master + Final unlocked: 4 rows (tags 440 / 442), Final auto shows Valor's Keyblade, pick Oblivion for it
// ---- (s146 outfit) the Debug page: Become Sora / Outfit / Auto Outfits on top, backend rows via CP_DB_ORD
static bool dbg_open_(void) {
    if (!page_open_()) return false;
    for (int i = 0; i < CP_ROW_DEBUG; i++) pt_(2, CP_BTN_DOWN);
    CHECK(P.row == CP_ROW_DEBUG);
    pt_(2, CP_BTN_A);
    for (int i = 0; i < 60 && P.state == CP_SWITCH; i++) pt_(2, 0);
    CHECK(P.state == CP_OPEN && P.page == 5 && P.db_row == 0 && cp_built_(&P));
    return true;
}
static CpVals GD;  // (s220 members) the Debug page's vals: Goofy in Party 2 (dbm_state 11 = his id), the installed stand-ins
static bool test_dbg_outfit_rows(void) {
    GD = GV, GD.db_v[11] = PCH_GOOFY, GD.db_party_have = 1u << PCH_RIKU | 1u << PCH_AURON | 1u << PCH_SPARROW;  // (Mulan / Aladdin: no bake)
    PV = &GD;
    CHECK(dbg_open_());
    pt_(2, CP_BTN_A);  // row 0 = Become Sora = backend row 7 (db_set = row + 1)
    CHECK(PO.db_set == 8 && PO.db_val == 1 && P.db_v[7] == 1);
    pt_(2, CP_BTN_DOWN);  // row 1 = Outfit = backend row 8: A = next (wraps), Right steps, 4 outfits then an error
    CHECK(P.db_row == 1);
    pt_(2, CP_BTN_A); CHECK(PO.db_set == 9 && PO.db_val == 1 && P.db_v[8] == 1);
    pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 9 && PO.db_val == 2);
    pt_(2, CP_BTN_RIGHT); pt_(2, CP_BTN_RIGHT); CHECK(P.db_v[8] == 4 && PO.db_val == 4);
    pt_(2, CP_BTN_RIGHT); CHECK(PO.se == CP_SE_ERROR && PO.db_set == 0 && P.db_v[8] == 4);
    pt_(2, CP_BTN_A); CHECK(PO.db_set == 9 && PO.db_val == 0 && P.db_v[8] == 0);  // A wraps to Normal
    pt_(2, CP_BTN_DOWN);  // row 2 = Auto Outfits = backend row 9
    pt_(2, CP_BTN_A); CHECK(PO.db_set == 10 && PO.db_val == 1 && P.db_v[9] == 1);
    pt_(2, CP_BTN_DOWN);  // (s217 riku / s220) row 3 = Party 1 = backend row 10 (Donald's place), row 4 = Party 2 = backend 11 (Goofy's)
    CHECK(P.db_row == 3 && CP_DB_ORD[3] == 10 && CP_DB_ORD[4] == 11 && P.db_v[10] == PCH_DONALD && P.db_v[11] == PCH_GOOFY);
    char pb[32];
    cp_db_value_(pb, sizeof pb, &P, 3); CHECK(!strcmp(pb, "Donald"));
    cp_db_value_(pb, sizeof pb, &P, 4); CHECK(!strcmp(pb, "Goofy"));
    // Right walks Donald -> the installed stand-ins in table order (Mulan / Aladdin have no bake: never offered), clamped at the end
    pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 11 && PO.db_val == PCH_RIKU && P.db_v[10] == PCH_RIKU);
    pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 11 && PO.db_val == PCH_AURON && P.db_v[10] == PCH_AURON);
    pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 11 && PO.db_val == PCH_SPARROW && P.db_v[10] == PCH_SPARROW);
    cp_db_value_(pb, sizeof pb, &P, 3); CHECK(!strcmp(pb, "Sparrow"));
    pt_(2, CP_BTN_RIGHT); CHECK(PO.se == CP_SE_ERROR && PO.db_set == 0 && P.db_v[10] == PCH_SPARROW);
    pt_(2, CP_BTN_A); CHECK(PO.db_set == 11 && PO.db_val == PCH_DONALD && P.db_v[10] == PCH_DONALD);  // A wraps
    pt_(2, CP_BTN_LEFT); CHECK(PO.se == CP_SE_ERROR && PO.db_set == 0 && P.db_v[10] == PCH_DONALD);
    pt_(2, CP_BTN_RIGHT); pt_(2, CP_BTN_RIGHT); CHECK(P.db_v[10] == PCH_AURON);  // Auron in Donald's place
    pt_(2, CP_BTN_LEFT); CHECK(PO.db_set == 11 && PO.db_val == PCH_RIKU); pt_(2, CP_BTN_RIGHT); CHECK(P.db_v[10] == PCH_AURON);
    pt_(2, CP_BTN_DOWN);  // row 4 = Party 2: Goofy, then Riku and Sparrow; Auron stands in Party 1 = never offered here (no character twice)
    CHECK(P.db_row == 4);
    pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 12 && PO.db_val == PCH_RIKU && P.db_v[11] == PCH_RIKU);
    pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 12 && PO.db_val == PCH_SPARROW && P.db_v[11] == PCH_SPARROW);
    cp_db_value_(pb, sizeof pb, &P, 4); CHECK(!strcmp(pb, "Sparrow"));
    pt_(2, CP_BTN_RIGHT); CHECK(PO.se == CP_SE_ERROR && PO.db_set == 0 && P.db_v[11] == PCH_SPARROW);
    pt_(2, CP_BTN_A); CHECK(PO.db_set == 12 && PO.db_val == PCH_GOOFY && P.db_v[11] == PCH_GOOFY);
    pt_(2, CP_BTN_A); CHECK(PO.db_set == 12 && PO.db_val == PCH_RIKU);  // A = the next one
    pt_(2, CP_BTN_UP);  // back on Party 1: Riku stands in Party 2 now, so Party 1 skips him
    pt_(2, CP_BTN_LEFT); CHECK(PO.db_set == 11 && PO.db_val == PCH_DONALD && P.db_v[10] == PCH_DONALD);
    pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 11 && PO.db_val == PCH_AURON);
    pt_(2, CP_BTN_LEFT); pt_(2, CP_BTN_DOWN);  // Party 1 = Donald again: Party 2 offers Auron again
    pt_(2, CP_BTN_A); CHECK(P.db_v[10] == PCH_DONALD && P.db_v[11] == PCH_AURON);
    pt_(2, CP_BTN_A); CHECK(P.db_v[11] == PCH_SPARROW);
    pt_(2, CP_BTN_A); CHECK(P.db_v[11] == PCH_GOOFY);
    // no stand-in installed: every press on a Party row is refused on the page itself (the slot's own member stays)
    GD.db_party_have = 0;
    pt_(2, CP_BTN_RIGHT); CHECK(PO.se == CP_SE_ERROR && PO.db_set == 0 && P.db_v[11] == PCH_GOOFY);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_ERROR && PO.db_set == 0 && P.db_v[11] == PCH_GOOFY);
    pt_(2, CP_BTN_UP); pt_(2, CP_BTN_LEFT); CHECK(PO.se == CP_SE_ERROR && PO.db_set == 0 && P.db_v[10] == PCH_DONALD);
    pt_(2, CP_BTN_DOWN);
    GD.db_party_have = 1u << PCH_RIKU;  // one stand-in: in Party 2 he is not offered to Party 1 any more
    pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 12 && PO.db_val == PCH_RIKU);
    pt_(2, CP_BTN_UP); pt_(2, CP_BTN_RIGHT); CHECK(PO.se == CP_SE_ERROR && PO.db_set == 0 && P.db_v[10] == PCH_DONALD);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_ERROR && PO.db_set == 0);
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_LEFT); CHECK(PO.db_set == 12 && PO.db_val == PCH_GOOFY);
    // (s223 cloud) Cloud installed: the last one on both rows ("Cloud"); in Party 2 he is never offered on Party 1
    GD.db_party_have = 1u << PCH_RIKU | 1u << PCH_SPARROW | 1u << PCH_CLOUD;
    pt_(2, CP_BTN_RIGHT); pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 12 && PO.db_val == PCH_SPARROW);
    pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 12 && PO.db_val == PCH_CLOUD && P.db_v[11] == PCH_CLOUD);
    cp_db_value_(pb, sizeof pb, &P, 4); CHECK(!strcmp(pb, "Cloud"));
    pt_(2, CP_BTN_RIGHT); CHECK(PO.se == CP_SE_ERROR && PO.db_set == 0 && P.db_v[11] == PCH_CLOUD);
    pt_(2, CP_BTN_UP);  // Party 1: Riku, Sparrow, then the end (Cloud stands in Party 2)
    pt_(2, CP_BTN_RIGHT); pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 11 && PO.db_val == PCH_SPARROW && P.db_v[10] == PCH_SPARROW);
    pt_(2, CP_BTN_RIGHT); CHECK(PO.se == CP_SE_ERROR && PO.db_set == 0 && P.db_v[10] == PCH_SPARROW);
    pt_(2, CP_BTN_A); CHECK(PO.db_set == 11 && PO.db_val == PCH_DONALD && P.db_v[10] == PCH_DONALD);  // A wraps past the absent Cloud
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_A); CHECK(PO.db_set == 12 && PO.db_val == PCH_GOOFY && P.db_v[11] == PCH_GOOFY);  // A wraps from Cloud
    pt_(2, CP_BTN_UP);  // Party 1 may take Cloud now (Party 2 = Goofy): Right x3 = Cloud, then the end
    pt_(2, CP_BTN_RIGHT); pt_(2, CP_BTN_RIGHT); pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 11 && PO.db_val == PCH_CLOUD);
    cp_db_value_(pb, sizeof pb, &P, 3); CHECK(!strcmp(pb, "Cloud"));
    pt_(2, CP_BTN_RIGHT); CHECK(PO.se == CP_SE_ERROR && P.db_v[10] == PCH_CLOUD);
    pt_(2, CP_BTN_A); CHECK(PO.db_set == 11 && PO.db_val == PCH_DONALD && P.db_v[10] == PCH_DONALD);
    pt_(2, CP_BTN_DOWN); CHECK(P.db_row == 4 && P.db_v[11] == PCH_GOOFY);
    pt_(2, CP_BTN_DOWN);  // row 5 = Sora Level = backend row 0
    pt_(2, CP_BTN_RIGHT); CHECK(PO.db_set == 1 && PO.db_val >= 1 && P.db_v[0] >= 1);
    pt_(2, CP_BTN_DOWN);  // row 6 = Unlock Keyblades = backend row 1
    pt_(2, CP_BTN_A); CHECK(PO.db_set == 2 && PO.db_val == 1 && P.db_v[1] == 1);
    for (int i = 0; i < CP_DB_ROWS; i++) pt_(2, CP_BTN_DOWN);  // the list scrolls: the last row (No Clip) is reachable
    CHECK(P.db_row == CP_DB_ROWS - 1 && P.db_top == CP_DB_ROWS - CP_DB_VIS && CP_DB_ORD[P.db_row] == 6);
    pt_(2, CP_BTN_A); CHECK(PO.db_set == 7 && PO.db_val == 1);
    char b[32];
    cp_db_value_(b, sizeof b, &P, 1); CHECK(!strcmp(b, "Normal"));
    P.db_v[8] = 1; cp_db_value_(b, sizeof b, &P, 1); CHECK(!strcmp(b, "Halloween"));
    PV = &GV;
    return true;
}

static bool test_equip_forms(void) {
    ge_(KB_KINGDOM_KEY, KB_STAR_SEEKER);
    GE.kb[KB_MASTER] = 0x2a, GE.kb[KB_FINAL] = 0, GE.kb_forms = 3;
    int len = 0;
    for (int k = 440; k <= 443; k++) { CHECK(cs_msg(&S, k, &len) && len > 0); }
    CHECK(equip_open_() && P.nroot == 5);
    int w[KB_SLOTS] = { kb_text_key(0), kb_text_key(1), kb_text_key(2), kb_text_key(1) };
    CHECK(equip_rows_n_(w, 4));
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_DOWN); CHECK(P.eq_slot == KB_MASTER && PO.se == CP_SE_CURSOR);
    CHECK(P.tree.n[P.tree.n[P.root[3]].kid0].d.msg_key == kb_text_key(kb_index(0x2a)) + 1);  // (s120b)
    pt_(2, CP_BTN_DOWN); CHECK(P.eq_slot == KB_FINAL);
    CHECK(P.tree.n[P.tree.n[P.root[3]].kid0].d.msg_key == kb_text_key(kb_index(kb_slot_id(GE.kb, KB_FINAL))) + 1);  // (s120b)
    pt_(2, CP_BTN_DOWN); CHECK(P.eq_slot == KB_FINAL && PO.se == CP_SE_NONE);  // clamp
    pt_(2, CP_BTN_A); CHECK(P.sub == 2 && P.eq_pick == 1);  // auto = Valor's (Star Seeker)
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_ERROR && !PO.kb_set);  // Valor holds it: one copy
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_DOWN); CHECK(P.eq_pick == 3);
    pt_(2, CP_BTN_UP); CHECK(P.eq_pick == 2);
    pt_(2, CP_BTN_A); CHECK(PO.se == CP_SE_ERROR && !PO.kb_set);  // Master holds Oathkeeper
    pt_(2, CP_BTN_DOWN); pt_(2, CP_BTN_A); CHECK(PO.kb_set && PO.kb_slot == KB_FINAL && PO.kb_id == 0x2b && P.kb[KB_FINAL] == 0x2b && P.sub == 1);
    // only Master unlocked: 3 rows; live mfkb 0: 2 rows
    ge_(KB_KINGDOM_KEY, KB_STAR_SEEKER);
    GE.kb_forms = 1;
    CHECK(equip_open_());
    int vs[KB_SLOTS];
    CHECK(cp_eq_vis(&GE, vs) == 3 && vs[2] == KB_MASTER);
    g_mfkb = 0; CHECK(cp_eq_vis(&GE, vs) == 2); g_mfkb = 1;
    PV = &GV;
    return true;
}
// all 26 owned (s214): 9 rows shown, the list scrolls with the cursor (as Abilities) and back
static bool test_equip_scroll(void) {
    ge_(KB_KINGDOM_KEY, KB_STAR_SEEKER);
    GE.kb_own = 0x3ffffffu;  // (s214: 26)
    CHECK(equip_open_());
    pt_(2, CP_BTN_A); CHECK(P.sub == 2 && P.eq_pick == 0);
    CHECK(P.tree.n[P.tree.n[P.root[2]].kid0].nkids == 2 * CP_EQ_VIS);
    for (int i = 0; i < KB_N + 3; i++) pt_(2, CP_BTN_DOWN);
    CHECK(P.eq_pick == KB_N - 1 && P.eq_top == KB_N - CP_EQ_VIS);
    const CsNode *pw = &P.tree.n[P.tree.n[P.root[2]].kid0];
    CHECK(pw->nkids == 2 * CP_EQ_VIS && P.tree.n[P.tree.n[pw->kid0 + 2 * (CP_EQ_VIS - 1)].kid0].d.msg_key == kb_text_key(KB_N - 1));
    for (int i = 0; i < CP_EQ_VIS - 1; i++) pt_(2, CP_BTN_UP);
    CHECK(P.eq_top == KB_N - CP_EQ_VIS);  // the cursor walks up to the window's top row first
    pt_(2, CP_BTN_UP); CHECK(P.eq_top == KB_N - CP_EQ_VIS - 1);
    pt_(2, CP_BTN_B); pt_(2, CP_BTN_A);  // reopen: scrolled so the equipped one (KK, entry 0) shows
    CHECK(P.eq_pick == 0 && P.eq_top == 0);
    PV = &GV;
    return true;
}

#define RUN(t) do { bool ok_ = t(); printf("%s %s\n", ok_ ? "PASS" : "FAIL", #t); if (!ok_) fails++; } while (0)

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: camp_test <camp_seq.bin> <golden dir>\n"); return 2; }
    GOLD = argv[2];
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "no %s\n", argv[1]); return 2; }
    fseek(f, 0, SEEK_END); BUFN = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    BUF = malloc(BUFN);
    if (fread(BUF, 1, BUFN, f) != BUFN) return 2;
    fclose(f);
    if (!cs_load(&S, BUF, BUFN)) { printf("FAIL cs_load\n"); return 1; }
    RUN(test_load);
    RUN(test_ag133_f5);
    RUN(test_ag132_f20);
    RUN(test_l2_0_f6);
    RUN(test_l2_1_f30);
    RUN(test_load_rejects_truncated);
    RUN(test_load_rejects_bad_msg);
    RUN(test_layout_last);
    RUN(test_msg);
    RUN(test_text_digits);
    RUN(test_status_text_keys);
    RUN(test_status_tree_matches_golden);
    RUN(test_status_lv99_values);
    RUN(test_status_party_rows);
    RUN(test_status_next_left);
    RUN(test_status_max_quads);
    RUN(test_top_r0_f6);
    RUN(test_top_r0_f12);
    RUN(test_top_r0_f120);
    RUN(test_top_r3_f120);
    RUN(test_top_r4_f120);
    RUN(test_close_f126);
    RUN(test_close_f140);
    RUN(test_max_quads);
    RUN(test_page_draw_matches_top_r0_f120);
    RUN(test_page_open_needs_unpaused);
    RUN(test_page_open_times_out);
    RUN(test_page_cursor_wraps);
    RUN(test_page_error_on_unbuilt_rows);
    RUN(test_page_ignores_input_while_animating);
    RUN(test_page_close_then_pulse_until_unpaused);
    RUN(test_page_close_gives_up);
    RUN(test_page_handover_returns_closed);
    RUN(test_status_opens_on_decide);
    RUN(test_status_b_returns_row3);
    RUN(test_status_who_wraps_no_treefull);
    RUN(test_status_plus_closes);
    RUN(test_status_b_during_open_anim);
    RUN(test_status_other_rows_still_error);
    RUN(test_abil_text_keys);
    RUN(test_abil_sort_record_order);
    RUN(test_abil_toggle_popup_back);
    RUN(test_abil_ap_short_error);
    RUN(test_abil_scroll_all_learned);
    RUN(test_abil_empty_list);
    RUN(test_cust_text_keys);
    RUN(test_cust_set_spell);
    RUN(test_cust_remove_and_empty);
    RUN(test_cust_back_keeps_row);
    RUN(test_cust_tags_name_buttons);
    RUN(test_cust_picker_draws);
    RUN(test_equip_text_keys);
    RUN(test_equip_set_main);
    RUN(test_equip_valor_one_copy);
    RUN(test_equip_back_keeps_row);
    RUN(test_equip_picker_draws);
    RUN(test_equip_kb_text_keys);
    RUN(test_equip_owned_only);
    RUN(test_equip_scroll);
    RUN(test_eqlook_text_keys);
    RUN(test_eqlook_rows_armor);
    RUN(test_eqlook_panel_keyblade);
    RUN(test_wpn_picker);
    RUN(test_eqlook_off_switch);
    RUN(test_equip_forms);
    RUN(test_dbg_outfit_rows);
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}

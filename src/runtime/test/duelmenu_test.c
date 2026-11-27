// host test for runtime/sora_duelmenu_core.h (s289 duelmenu: the command menu's "Duel" list on page 2): the rows of every phase,
// the action each row calls, the greyed rows and their reasons (his: away / in a duel / too far, the Triangle prompt's 6 m flat and
// 3 m height band; ours: the gate, plus the reason row), the row texts and the name cut, the pretend friend, the cap, dum_show
// (when page 2 shows "Duel"), dum_find_ask, the settle guard.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/duelmenu_test.c -o build_h/duelmenu_test.exe
//   (UBSan: + -fsanitize=undefined,bounds -fsanitize-trap=all)
#include <stdio.h>
#include <stdlib.h>
#include "sora_duelmenu_core.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)
#define FAKE_K (-2)  // sora_duel.h DU_FAKE_K
#define NEAR_M 6.0f  // DU_NEAR_M

static DuMenuFriend fr(int k, const char *name, float dist, float dy) {
    DuMenuFriend f = {.k = k, .name = name, .pose = true, .dist = dist, .dy = dy};
    return f;
}

static void idle_tests(void) {
    DuMenuRow r[DUM_MAX];
    DuMenuFriend f[4];
    // one friend 5 m away: "Ask copy2" -> du_ask(0)
    f[0] = fr(0, "copy2", 5.0f, 0.2f);
    int n = du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX);
    CHECK(n == 1);
    CHECK(!strcmp(r[0].text, "Ask copy2") && r[0].act == DUM_ASK && r[0].k == 0 && !r[0].grey && r[0].why == DUG_OK);
    // the 6 m edge: d <= near is in (du_input: d <= best, best starts at g_du_near_m)
    f[0].dist = 6.0f;
    n = du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX);
    CHECK(n == 1 && !r[0].grey && !strcmp(r[0].text, "Ask copy2"));
    f[0].dist = 6.01f;
    n = du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX);
    CHECK(n == 1 && r[0].grey && r[0].why == DUG_FAR && r[0].act == DUM_ASK && r[0].k == 0);
    CHECK(!strcmp(r[0].text, "copy2: too far"));
    // the live duelnear distance is the one used
    n = du_menu_rows(DU_IDLE, DUG_OK, f, 1, 12.0f, r, DUM_MAX);
    CHECK(n == 1 && !r[0].grey);
    n = du_menu_rows(DU_IDLE, DUG_OK, f, 1, 1.0f, r, DUM_MAX);
    CHECK(n == 1 && r[0].grey && r[0].why == DUG_FAR);
    // the height band: |dy| < 3 (du_input: fabsf(f[1] - me[1]) < 3.0f)
    f[0] = fr(0, "copy2", 2.0f, 2.99f);
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX) == 1 && !r[0].grey);
    f[0].dy = 3.0f;
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX) == 1 && r[0].grey && r[0].why == DUG_FAR);
    f[0].dy = -3.0f;
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX) == 1 && r[0].grey && r[0].why == DUG_FAR);
    f[0].dy = -2.5f;
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX) == 1 && !r[0].grey);
    // no pose here (another map / faded out) = too far, whatever the numbers say
    f[0] = fr(0, "copy2", 1.0f, 0.0f), f[0].pose = false;
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX) == 1 && r[0].grey && r[0].why == DUG_FAR);
    // a bad number is never "near"
    f[0] = fr(0, "copy2", NAN, 0.0f);
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX) == 1 && r[0].grey && r[0].why == DUG_FAR);
    f[0] = fr(0, "copy2", 1.0f, INFINITY);
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX) == 1 && r[0].grey && r[0].why == DUG_FAR);
    f[0] = fr(0, "copy2", -1.0f, 0.0f);
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX) == 1 && r[0].grey && r[0].why == DUG_FAR);
    // his reasons, first one wins: away, in a duel, too far
    f[0] = fr(1, "pc", 2.0f, 0.0f), f[0].away = true;
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX) == 1 && r[0].grey && r[0].why == DUMW_AWAY && !strcmp(r[0].text, "pc: away"));
    f[0].in_duel = true, f[0].dist = 50.0f;
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX) == 1 && r[0].why == DUMW_AWAY);
    f[0].away = false;
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX) == 1 && r[0].why == DUMW_DUEL && !strcmp(r[0].text, "pc: in a duel"));
    CHECK(r[0].act == DUM_ASK && r[0].k == 1);
    f[0].in_duel = false;
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX) == 1 && r[0].why == DUG_FAR && !strcmp(r[0].text, "pc: too far"));
    // no friend: no rows (page 2 shows no "Duel" then either: dum_show)
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 0, NEAR_M, r, DUM_MAX) == 0);
    CHECK(du_menu_rows(DU_IDLE, DUG_ENEMY, NULL, 3, NEAR_M, r, DUM_MAX) == 0);
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, -1, NEAR_M, r, DUM_MAX) == 0);
}

static void gate_tests(void) {
    DuMenuRow r[DUM_MAX];
    DuMenuFriend f[3] = {fr(0, "copy2", 3.0f, 0.0f), fr(1, "Tyler", 4.0f, 0.0f), fr(2, "far", 30.0f, 0.0f)};
    // our gate says no: every Ask row greyed with it (his own reason first), then one reason row
    int n = du_menu_rows(DU_IDLE, DUG_ENEMY, f, 3, NEAR_M, r, DUM_MAX);
    CHECK(n == 4);
    CHECK(!strcmp(r[0].text, "Ask copy2") && r[0].grey && r[0].why == DUG_ENEMY && r[0].act == DUM_ASK && r[0].k == 0);
    CHECK(!strcmp(r[1].text, "Ask Tyler") && r[1].grey && r[1].why == DUG_ENEMY && r[1].k == 1);
    CHECK(!strcmp(r[2].text, "far: too far") && r[2].grey && r[2].why == DUG_FAR && r[2].k == 2);  // (his reason, not ours)
    CHECK(!strcmp(r[3].text, "Enemy near") && r[3].grey && r[3].act == DUM_NONE && r[3].k == -1 && r[3].why == DUG_ENEMY);
    // every gate reason has a short text (the reason row) and a log text; none is empty except ok
    for (int g = 0; g < DUMW_N; g++) {
        CHECK(dum_why_short(g) != NULL && dum_why_long(g) != NULL && dum_why_long(g)[0]);
        CHECK(g == DUG_OK ? !dum_why_short(g)[0] : dum_why_short(g)[0] != 0);
    }
    CHECK(!strcmp(dum_why_short(DUG_AIR), "In the air") && !strcmp(dum_why_short(DUG_COOL), "Wait 5 s"));
    CHECK(!strcmp(dum_why_long(DUG_ENEMY), DUG_WHY[DUG_ENEMY]) && !strcmp(dum_why_long(DUMW_DUEL), "he is in a duel"));
    CHECK(!strcmp(dum_why_short(99), "not now") && !strcmp(dum_why_long(-5), "?"));
    // each gate reason greys the near friend's row with that reason
    for (int g = DUG_OFF; g < DUG_N; g++) {
        n = du_menu_rows(DU_IDLE, g, f, 1, NEAR_M, r, DUM_MAX);
        CHECK(n == 2 && r[0].grey && r[0].why == g && r[1].act == DUM_NONE && r[1].why == g && !strcmp(r[1].text, dum_why_short(g)));
    }
    // over (the result banner): du_ask refuses until idle -> greyed "Wait 5 s", also when the gate itself says ok
    n = du_menu_rows(DU_OVER, DUG_OK, f, 2, NEAR_M, r, DUM_MAX);
    CHECK(n == 3 && r[0].grey && r[0].why == DUG_BUSY && r[1].grey && r[2].act == DUM_NONE && !strcmp(r[2].text, "Wait 5 s"));
    n = du_menu_rows(DU_OVER, DUG_COOL, f, 1, NEAR_M, r, DUM_MAX);
    CHECK(n == 2 && r[0].why == DUG_COOL && !strcmp(r[1].text, "Wait 5 s"));
    // idle + the 5 s cool after a duel / a refusal
    n = du_menu_rows(DU_IDLE, DUG_COOL, f, 1, NEAR_M, r, DUM_MAX);
    CHECK(n == 2 && r[0].grey && r[0].why == DUG_COOL);
}

static void phase_tests(void) {
    DuMenuRow r[DUM_MAX];
    DuMenuFriend f[2] = {fr(0, "copy2", 3.0f, 0.0f), fr(1, "pc", 3.0f, 0.0f)};
    // asking (we asked): Cancel only, never greyed (the END cancel needs no gate)
    int n = du_menu_rows(DU_ASKING, DUG_AIR, f, 2, NEAR_M, r, DUM_MAX);
    CHECK(n == 1 && !strcmp(r[0].text, "Cancel") && r[0].act == DUM_CANCEL && !r[0].grey && r[0].k == -1);
    CHECK(du_menu_rows(DU_ASKING, DUG_OK, NULL, 0, NEAR_M, r, DUM_MAX) == 1 && r[0].act == DUM_CANCEL);  // (no friend listed any more)
    // asked (he asked us): Accept / Decline; the gate greys Accept only (du_accept's strict gate) + the reason row
    n = du_menu_rows(DU_ASKED, DUG_OK, f, 2, NEAR_M, r, DUM_MAX);
    CHECK(n == 2 && !strcmp(r[0].text, "Accept") && r[0].act == DUM_ACCEPT && !r[0].grey);
    CHECK(!strcmp(r[1].text, "Decline") && r[1].act == DUM_DECLINE && !r[1].grey);
    n = du_menu_rows(DU_ASKED, DUG_AIR, f, 2, NEAR_M, r, DUM_MAX);
    CHECK(n == 3 && r[0].act == DUM_ACCEPT && r[0].grey && r[0].why == DUG_AIR);
    CHECK(r[1].act == DUM_DECLINE && !r[1].grey);
    CHECK(r[2].act == DUM_NONE && r[2].grey && !strcmp(r[2].text, "In the air"));
    // countdown / fight / time up: Forfeit
    const int live[3] = {DU_COUNT, DU_FIGHT, DU_TIMEUP};
    for (int i = 0; i < 3; i++) {
        n = du_menu_rows(live[i], DUG_ENEMY, f, 2, NEAR_M, r, DUM_MAX);
        CHECK(n == 1 && !strcmp(r[0].text, "Forfeit") && r[0].act == DUM_FORFEIT && !r[0].grey);
    }
    // an unknown phase: nothing (no pick can call anything)
    CHECK(du_menu_rows(DU_NPH, DUG_OK, f, 2, NEAR_M, r, DUM_MAX) == 0);
    CHECK(du_menu_rows(-1, DUG_OK, f, 2, NEAR_M, r, DUM_MAX) == 0);
}

static void text_tests(void) {
    DuMenuRow r[DUM_MAX];
    // the pretend friend (live duelfake 1): its own k, its name
    DuMenuFriend f[3] = {fr(FAKE_K, "Pretend", 2.0f, 0.0f)};
    int n = du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX);
    CHECK(n == 1 && r[0].k == FAKE_K && r[0].act == DUM_ASK && !strcmp(r[0].text, "Ask Pretend"));
    CHECK(dum_find_ask(r, n, FAKE_K) == 0 && dum_find_ask(r, n, 0) == -1);
    // the name is cut to DUM_NAME_MAX characters; none / empty = "friend"; a control byte is not drawn as one
    f[0] = fr(0, "Abcdefghijklmnop", 2.0f, 0.0f);
    f[1] = fr(1, NULL, 2.0f, 0.0f);
    f[2] = fr(2, "", 20.0f, 0.0f);
    n = du_menu_rows(DU_IDLE, DUG_OK, f, 3, NEAR_M, r, DUM_MAX);
    CHECK(n == 3 && !strcmp(r[0].text, "Ask Abcdefghij"));
    CHECK(!strcmp(r[1].text, "Ask friend") && !strcmp(r[2].text, "friend: too far"));
    f[0].name = "a\tb";
    du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX);
    CHECK(!strcmp(r[0].text, "Ask a?b"));
    // the longest text fits its buffer (10-char name + the longest reason)
    f[0] = fr(0, "WWWWWWWWWWWWWWWW", 2.0f, 0.0f), f[0].away = false, f[0].in_duel = true;
    du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, DUM_MAX);
    CHECK(!strcmp(r[0].text, "WWWWWWWWWW: in a duel") && strlen(r[0].text) < DUM_TEXT);
    for (int g = 0; g < DUMW_N; g++) CHECK(strlen(dum_why_short(g)) + DUM_NAME_MAX + 2 < DUM_TEXT);
    // dum_find_ask finds a greyed Ask row too (the next frame's check reads its grey / why)
    f[0] = fr(0, "copy2", 9.0f, 0.0f), f[1] = fr(1, "pc", 1.0f, 0.0f);
    n = du_menu_rows(DU_IDLE, DUG_OK, f, 2, NEAR_M, r, DUM_MAX);
    CHECK(n == 2 && dum_find_ask(r, n, 0) == 0 && r[0].grey && dum_find_ask(r, n, 1) == 1 && !r[1].grey && dum_find_ask(r, n, 2) == -1);
    n = du_menu_rows(DU_ASKED, DUG_OK, f, 2, NEAR_M, r, DUM_MAX);
    CHECK(dum_find_ask(r, n, 0) == -1 && dum_find_ask(r, n, 1) == -1);  // (no Ask rows once he asked)
}

static void cap_tests(void) {
    DuMenuRow r[DUM_MAX + 4];
    DuMenuFriend f[12];
    for (int i = 0; i < 12; i++) f[i] = fr(i, "f", 1.0f, 0.0f);
    memset(r, 0x5a, sizeof r);
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 12, NEAR_M, r, DUM_MAX + 4) == DUM_MAX);  // (never past the exe's 8 rows)
    CHECK((unsigned char)r[DUM_MAX].text[0] == 0x5a);                                // (row 9 not written)
    CHECK(du_menu_rows(DU_IDLE, DUG_ENEMY, f, 12, NEAR_M, r, DUM_MAX) == DUM_MAX);
    CHECK(r[DUM_MAX - 1].act == DUM_NONE && r[DUM_MAX - 2].act == DUM_ASK);         // (the reason row keeps its place at the end)
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 12, NEAR_M, r, 3) == 3);
    CHECK(du_menu_rows(DU_IDLE, DUG_ENEMY, f, 12, NEAR_M, r, 1) == 1 && r[0].act == DUM_ASK && r[0].grey);  // (1 row: the Ask, greyed)
    CHECK(du_menu_rows(DU_ASKED, DUG_AIR, f, 1, NEAR_M, r, 2) == 2 && r[0].act == DUM_ACCEPT && r[1].act == DUM_DECLINE);
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, r, 0) == 0);
    CHECK(du_menu_rows(DU_IDLE, DUG_OK, f, 1, NEAR_M, NULL, 4) == 0);
}

static void show_tests(void) {
    // page 2's "Duel": key + duels on + a friend in the room; our duel past idle keeps it without one
    CHECK(!dum_show(false, true, DU_IDLE, 2));   // (key off: page 2 as before)
    CHECK(!dum_show(true, false, DU_FIGHT, 2));  // (duel 0)
    CHECK(dum_show(true, true, DU_IDLE, 1));
    CHECK(!dum_show(true, true, DU_IDLE, 0));    // (out of a room / no friend: page 2 as before)
    CHECK(!dum_show(true, true, DU_OVER, 0));
    CHECK(dum_show(true, true, DU_OVER, 1));
    const int keep[5] = {DU_ASKING, DU_ASKED, DU_COUNT, DU_FIGHT, DU_TIMEUP};
    for (int i = 0; i < 5; i++) CHECK(dum_show(true, true, keep[i], 0));
    // the settle guard: a press within DUM_SETTLE_MS of a phase change under the list is not a pick
    CHECK(dum_settled(0, 5));
    CHECK(!dum_settled(1000, 1000) && !dum_settled(1000, 1000 + DUM_SETTLE_MS - 1) && dum_settled(1000, 1000 + DUM_SETTLE_MS));
}

int main(void) {
    idle_tests();
    gate_tests();
    phase_tests();
    text_tests();
    cap_tests();
    show_tests();
    printf("duelmenu_test: %d / %d checks pass\n", runs - fails, runs);
    return fails ? 1 : 0;
}

// host test for runtime/sora_duel_core.h + the NET_DUEL packet in runtime/sora_net_proto.h (s272 duel, co-op item 6: duels
// between two players): the gate order, KH2's damage formula with a player victim, the 1 HP floor, the time-limit judge, the ring
// and its leave rule, the countdown, the duel state machine (asker / friend), the peer-gone rule, the hit de-dupe, the body
// tests, the wire format of every op, and NF_DUEL riding a SNAP. (s289 kbthrow: the thrown Keyblade against the opponent, throw_tests)
// (s289 duelreflect) + runtime/sora_duel_reflect.h (Reflect's burst on the opponent: the A count per tier at kh 1 and 2, then one
// B; the damage table of duel plan 2.1; the cylinder; src 7's name; a src 7 HIT on the wire, reflect_tests) and
// runtime/sora_duel_act.h (the live "duel act <verb>" lines, the verb table, the pending act, act_tests).
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/duel_test.c -o build_h/duel_test.exe
//   (UBSan: + -fsanitize=undefined,bounds -fsanitize-trap=all)
#include <stdio.h>
#include <stdlib.h>
#include "sora_net_proto.h"
#include "sora_duel_core.h"
#include "sora_duel_reflect.h"
#include "sora_duel_act.h"
#include "sora_duel_magnet.h"  // (s289 duelmagnet, magnet_tests)

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)
#define NEAR(a, b, e) (fabsf((a) - (b)) <= (e))
#define T0 1000000ull  // a GetTickCount64-like start (never 0)

static void gate_tests(void) {
    DuGateIn g = {0};
    CHECK(du_gate(NULL, true) == DUG_OFF);
    CHECK(du_gate(&g, true) == DUG_OFF);
    g.on = true, g.enemy_m = g.chase_m = -1, g.now = T0;
    CHECK(du_gate(&g, false) == DUG_NOT_DRIVEN);
    g.driven = true;
    CHECK(du_gate(&g, false) == DUG_OK);    // (not strict: in the air / a menu are fine when an ASK comes in)
    CHECK(du_gate(&g, true) == DUG_AIR);    // (strict: standing on the ground)
    g.ground = true;
    CHECK(du_gate(&g, true) == DUG_OK);
    // the order: busy, cool, menu, form, summon, board, air, enemy, chase, hit; the first block wins
    DuGateIn b = g;
    b.busy = b.menu = b.form = b.summon = b.board = true, b.ground = false, b.cool_until = T0 + 1, b.enemy_m = 5, b.chase_m = 40, b.last_hit_ms = T0 - 1;
    CHECK(du_gate(&b, true) == DUG_BUSY);
    b.busy = false;
    CHECK(du_gate(&b, true) == DUG_COOL);
    b.cool_until = T0;  // (now == the end: over)
    CHECK(du_gate(&b, true) == DUG_MENU);
    CHECK(du_gate(&b, false) == DUG_FORM);  // (not strict: the menu does not refuse)
    b.menu = false;
    CHECK(du_gate(&b, true) == DUG_FORM);
    b.form = false;
    CHECK(du_gate(&b, true) == DUG_SUMMON);
    b.summon = false;
    CHECK(du_gate(&b, true) == DUG_BOARD);
    CHECK(du_gate(&b, false) == DUG_ENEMY);  // (not strict: board / air are fine)
    b.board = false;
    CHECK(du_gate(&b, true) == DUG_AIR);
    b.ground = true;
    CHECK(du_gate(&b, true) == DUG_ENEMY);
    b.enemy_m = 30.0f;  // (the edge is in)
    CHECK(du_gate(&b, true) == DUG_ENEMY);
    b.enemy_m = 30.01f;
    CHECK(du_gate(&b, true) == DUG_CHASE);
    b.chase_m = 60.5f;
    CHECK(du_gate(&b, true) == DUG_HIT);
    b.last_hit_ms = T0 - DU_HIT_HOLD_MS;  // (10 s ago exactly: over)
    CHECK(du_gate(&b, true) == DUG_OK);
    b.last_hit_ms = T0 - DU_HIT_HOLD_MS + 1;
    CHECK(du_gate(&b, true) == DUG_HIT);
    b.last_hit_ms = 0;  // (never hit)
    CHECK(du_gate(&b, true) == DUG_OK);
    b.on = false;
    CHECK(du_gate(&b, true) == DUG_OFF);
    for (int i = 0; i < DUG_N; i++) CHECK(DUG_WHY[i] && DUG_WHY[i][0]);
    // enemies vs animals; BotW's target states
    CHECK(du_enemy_name("Enemy_Bokoblin_Junior") && du_enemy_name("Enemy_Lynel_Dark"));
    CHECK(!du_enemy_name("Animal_Fox_A") && !du_enemy_name("Animal_Horse") && !du_enemy_name(NULL) && !du_enemy_name("Enemy") && !du_enemy_name(""));
    // (s290 duelhorse) mountable horses
    CHECK(du_horse_name("Animal_Horse_A") && du_horse_name("GameRomHorse00L") && du_horse_name("GameROMMotorcycle"));
    CHECK(!du_horse_name("GameRomHorseSaddle_00") && !du_horse_name("GameRomHorseReins_01") && !du_horse_name("Animal_Fox_A") &&
          !du_horse_name("Enemy_Bokoblin_Junior") && !du_horse_name(NULL) && !du_horse_name(""));
    // (s291 duelride) a friend's horse puppet counts as the horse it stands for; its saddle / reins do not
    CHECK(du_horse_name("OnlineHorse_GameRomHorse01") && du_horse_name("OnlineHorse_GameROMMotorcycle") && du_horse_name("OnlineHorse_Animal_Horse_A"));
    CHECK(!du_horse_name("OnlineHorse_GameRomHorseSaddle_00") && !du_horse_name("OnlineHorse_GameRomHorseReins_05") && !du_horse_name("OnlineHorse_"));
    // (s291 duelride) no "Duel" while we ride / a friend's horse puppet is within the duelhorse distance; duelride 0 = as before
    CHECK(du_ride_block(1, true, -1, DU_HORSE_M) == DURB_WE_RIDE && du_ride_block(1, true, 99, 0) == DURB_WE_RIDE);
    CHECK(du_ride_block(1, false, 0.9f, DU_HORSE_M) == DURB_PUPPET && du_ride_block(1, false, DU_HORSE_M, DU_HORSE_M) == DURB_PUPPET);
    CHECK(du_ride_block(1, false, DU_HORSE_M + 0.01f, DU_HORSE_M) == DURB_NONE && du_ride_block(1, false, -1, DU_HORSE_M) == DURB_NONE);
    CHECK(du_ride_block(1, false, 0.9f, 0) == DURB_NONE);  // (duelhorse 0: puppets do not count either)
    CHECK(du_ride_block(0, true, 0.9f, DU_HORSE_M) == DURB_NONE);  // (duelride 0)
    CHECK(du_ride_skip(1, true) && !du_ride_skip(1, false) && !du_ride_skip(0, true));
    CHECK(DURB_NONE == DUR_NONE && DURB_NONE == 0);  // (the duel result enum keeps its own names)
    CHECK(du_chasing_state(2) && du_chasing_state(4));
    CHECK(!du_chasing_state(0) && !du_chasing_state(1) && !du_chasing_state(3) && !du_chasing_state(5) && !du_chasing_state(-1));
}

static void damage_tests(void) {
    int v = -1;
    // KH2 rows (sora_levels.h SL_LV, VERIFIED lvup): LV20 Str 12 / Def 13; the Kingdom Key's Str +3 (GUESS row, any key: S = Str + key)
    CHECK(du_dmg(12 + 3, 13, 100, 0, 1, &v) == 2 && v == 2);   // V 2 x 100 % = 2 quarter hearts
    CHECK(du_dmg(12 + 3, 13, 100, 0, 0, &v) == 1);             // Beginner: x0.5, ceil -> 1
    CHECK(du_dmg(12 + 3, 13, 100, 0, 2, &v) == 4);             // Proud: x2
    // (s286) dueldmg: x pct, rounded to the nearest, at least 1; 100 = unchanged; 0 / negative damage untouched
    CHECK(du_dmg_scale(3, 100) == 3 && du_dmg_scale(3, 50) == 2 && du_dmg_scale(2, 50) == 1 && du_dmg_scale(1, 50) == 1);
    CHECK(du_dmg_scale(6, 50) == 3 && du_dmg_scale(9, 50) == 5 && du_dmg_scale(1, 1) == 1 && du_dmg_scale(4, 0) == 1);
    CHECK(du_dmg_scale(0, 50) == 0 && du_dmg_scale(-1, 50) == -1 && du_dmg_scale(5000, 5000) == 50000);
    CHECK(du_dmg(12 + 3, 13, 100, 0, 3, &v) == 4);             // Critical: x2 (taken)
    CHECK(du_dmg(10, 13, 100, 0, 1, &v) == 1 && v == 1);       // S < Def: V = Min 1 (GUESS for a player victim)
    CHECK(du_dmg(13, 13, 100, 0, 1, &v) == 1 && v == 1);       // S == Def: V 0 -> 1
    CHECK(du_dmg(18, 13, 250, 0, 1, &v) == 13 && v == 5);      // a spell Power 250: ceil(12.5) = 13
    CHECK(du_dmg(16, 13, 150, 0, 1, &v) == 5);                 // Fire's last wave 150: ceil(4.5) = 5
    CHECK(du_dmg(16, 13, 50, 0, 1, &v) == 2);                  // Fire's tick 50: ceil(1.5) = 2
    CHECK(du_dmg(15, 13, 100, 20, 1, &v) == 3);                // + 20 % boost: 2 + ceil(0.4) = 3 (ab_boost)
    CHECK(du_dmg(15, 13, 0, 0, 1, &v) == 0);                   // Power 0 (a guard window) = no damage
    CHECK(du_dmg(15, 13, 100, 0, -5, &v) == 1 && du_dmg(15, 13, 100, 0, 9, &v) == 4);  // (difficulty clamped 0..3)
    CHECK(du_dmg(52 + 3, 13, 100, 0, 1, &v) == 42);            // LV99 Str vs LV20 Def: no Cap (GUESS)
    CHECK(du_dmg(15, 13, 1, 0, 1, NULL) == 1);                 // (never 0 for a real hit)
    // the floor
    bool ko = true;
    CHECK(du_life_after(40, 3, &ko) == 37 && !ko);
    CHECK(du_life_after(3, 1, &ko) == 2 && !ko);
    CHECK(du_life_after(3, 2, &ko) == 1 && ko);   // (to 1 exactly = the KO)
    CHECK(du_life_after(3, 30, &ko) == 1 && ko);  // (overkill: still 1, never 0)
    CHECK(du_life_after(1, 1, &ko) == 1 && ko);   // (at 1 already: any hit is the KO)
    CHECK(du_life_after(0, 5, &ko) == 0 && !ko);  // (dead / unknown: untouched)
    CHECK(du_life_after(20, 0, &ko) == 20 && !ko);
    // HP permille + the judge
    CHECK(du_pm(48, 48) == 1000 && du_pm(24, 48) == 500 && du_pm(1, 48) == 20 && du_pm(0, 48) == 0 && du_pm(5, 0) == 0 && du_pm(60, 48) == 1000);
    CHECK(du_time_result(600, 500) == DUR_WIN && du_time_result(400, 500) == DUR_LOSE && du_time_result(500, 500) == DUR_DRAW);
    CHECK(du_result_from_peer(DUE_KO, DUR_LOSE) == DUR_WIN);
    CHECK(du_result_from_peer(DUE_FORFEIT, DUR_LOSE) == DUR_WIN);
    CHECK(du_result_from_peer(DUE_GAVEUP, DUR_LOSE) == DUR_WIN && DUE_GAVEUP == 8 && DUE_CANCEL == 7);  // (s289: appended, the old numbers keep)
    CHECK(du_result_from_peer(DUE_GONE, DUR_WIN) == DUR_LOSE);
    CHECK(du_result_from_peer(DUE_KO, DUR_DRAW) == DUR_DRAW);
    CHECK(du_result_from_peer(DUE_ENEMY, DUR_CANCEL) == DUR_CANCEL && du_result_from_peer(DUE_CANCEL, DUR_NONE) == DUR_CANCEL);
    for (int i = 0; i < DUR_N; i++) CHECK(DUR_NAME[i] && DUR_NAME[i][0]);
    for (int i = 0; i < DUE_N; i++) CHECK(DUE_NAME[i] && DUE_NAME[i][0]);
    for (int i = 0; i < DU_NPH; i++) CHECK(DU_PH_NAME[i] && DU_PH_NAME[i][0]);
}

static void ring_tests(void) {
    const float a[3] = {100, 20, 300}, b[3] = {106, 22, 300};
    float c[3];
    du_mid(a, b, c);
    CHECK(NEAR(c[0], 103, 1e-4f) && NEAR(c[1], 21, 1e-4f) && NEAR(c[2], 300, 1e-4f));
    CHECK(NEAR(du_hdist(a, b), 6, 1e-4f));
    float p[3] = {103 + 9.99f, 21, 300};
    CHECK(du_inside(c, 10, p));
    p[0] = 103 + 10.01f;
    CHECK(!du_inside(c, 10, p));
    p[0] = 103, p[2] = 300 - 10;  // (on the line: in)
    CHECK(du_inside(c, 10, p));
    p[2] = 300, p[1] = 21 + DU_RING_DY + 0.1f;  // (above the ring: out)
    CHECK(!du_inside(c, 10, p));
    p[1] = 21 - DU_RING_DY;  // (the edge below: in)
    CHECK(du_inside(c, 10, p));
    p[1] = NAN;
    CHECK(!du_inside(c, 10, p));
    // 0.5 s outside in a row
    DuOut o = {0};
    CHECK(!du_out_step(&o, true, T0));
    CHECK(!du_out_step(&o, true, T0 + DU_OUT_MS - 1));
    CHECK(du_out_step(&o, true, T0 + DU_OUT_MS));
    CHECK(!du_out_step(&o, false, T0 + 600));  // (back in: starts over)
    CHECK(!du_out_step(&o, true, T0 + 700));
    CHECK(!du_out_step(&o, true, T0 + 1100));
    CHECK(du_out_step(&o, true, T0 + 1200));
    // the countdown + the clock
    CHECK(du_count_digit(0, T0) == -1 && du_count_digit(T0, T0 - 1) == -1);
    CHECK(du_count_digit(T0, T0) == 3 && du_count_digit(T0, T0 + 999) == 3 && du_count_digit(T0, T0 + 1000) == 2);
    CHECK(du_count_digit(T0, T0 + 2000) == 1 && du_count_digit(T0, T0 + 2999) == 1);
    CHECK(du_count_digit(T0, T0 + 3000) == 0 && du_count_digit(T0, T0 + 3000 + DU_GO_MS - 1) == 0);
    CHECK(du_count_digit(T0, T0 + 3000 + DU_GO_MS) == -1);
    char t[16];
    du_clock_txt(180000, t, sizeof t);
    CHECK(!strcmp(t, "3:00"));
    du_clock_txt(179000, t, sizeof t);
    CHECK(!strcmp(t, "2:59"));
    du_clock_txt(179001, t, sizeof t);
    CHECK(!strcmp(t, "3:00"));  // (rounded up: 0:00 only at the end)
    du_clock_txt(1, t, sizeof t);
    CHECK(!strcmp(t, "0:01"));
    du_clock_txt(0, t, sizeof t);
    CHECK(!strcmp(t, "0:00"));
    du_clock_txt(65000, t, sizeof t);
    CHECK(!strcmp(t, "1:05"));
    // the body tests
    const float feet[3] = {0, 10, 0};
    float q[3] = {1.5f, 10.5f, 0};
    CHECK(du_body_near(feet, q, 2.0f, -1, 2));
    q[0] = 2.1f;
    CHECK(!du_body_near(feet, q, 2.0f, -1, 2));
    q[0] = 0, q[1] = 12.5f;  // (dy 2.5 > hi 2)
    CHECK(!du_body_near(feet, q, 2.0f, -1, 2));
    q[1] = 8.9f;  // (dy -1.1 < lo -1)
    CHECK(!du_body_near(feet, q, 2.0f, -1, 2));
    const float body[3] = {0, 0, 0}, ahead[3] = {0, 0, 5}, behind[3] = {0, 0, -5}, side[3] = {5, 0, 0};
    CHECK(du_from_front(body, 0, ahead) && !du_from_front(body, 0, behind));
    CHECK(du_from_front(body, 3.14159265f, behind) && !du_from_front(body, 3.14159265f, ahead));
    CHECK(du_from_front(body, 1.5707963f, side));  // (yaw 90 deg faces +x)
    const float fw0[2] = {0, 1}, f45[3] = {3, 0, 3}, f70[3] = {3, 0, 1.0f}, fb[3] = {0, 0, -3}, fon[3] = {0.2f, 0, -0.2f};
    CHECK(du_facing(body, fw0, ahead) && du_facing(body, fw0, f45) && !du_facing(body, fw0, f70) && !du_facing(body, fw0, fb));
    CHECK(du_facing(body, fw0, fon));  // (on top of him)
    CHECK(DU_BODY_Y0_CM + DU_BODY_H_CM == 125.0f && NEAR((DU_BODY_Y0_CM + DU_BODY_H_CM * 0.5f), 70.0f, 1e-4f) && DU_BODY_R_CM == 55.0f);  // (KH2's sphere: centre 70, r 55)
}

static void sm_tests(void) {
    DuSm m = {0};
    m.limit_ms = 60000;
    // asker: ask, no answer
    CHECK(!du_sm_ask(&m, 0, T0));  // (id 0 is not valid)
    CHECK(du_sm_ask(&m, 0x1234, T0) && m.phase == DU_ASKING && m.role == DU_ROLE_ASKER && m.limit_ms == 60000);
    CHECK(!du_sm_ask(&m, 0x5678, T0) && !du_sm_asked(&m, 0x5678, T0));  // (busy)
    CHECK(du_sm_busy(&m) && !du_sm_live(&m));
    CHECK(du_sm_step(&m, T0 + DU_ACCEPT_MS + DU_ASK_SLACK_MS - 1) == DUS_NONE);
    CHECK(du_sm_step(&m, T0 + DU_ACCEPT_MS + DU_ASK_SLACK_MS) == DUS_ASK_TIMEOUT);
    du_sm_reset(&m);
    CHECK(m.phase == DU_IDLE && m.limit_ms == 60000 && !du_sm_busy(&m));  // (the live limit outlives a reset)
    // friend: asked, the 10 s run out
    CHECK(du_sm_asked(&m, 0x99, T0) && m.phase == DU_ASKED && m.role == DU_ROLE_FRIEND);
    CHECK(du_sm_step(&m, T0 + DU_ACCEPT_MS - 1) == DUS_NONE);
    CHECK(du_sm_step(&m, T0 + DU_ACCEPT_MS) == DUS_PROMPT_TIMEOUT);
    // friend accepts at +4 s: countdown, fight, time up, the judge, over, idle
    du_sm_reset(&m);
    CHECK(!du_sm_count(&m, T0));  // (nothing to accept)
    du_sm_asked(&m, 0x99, T0);
    uint64_t a = T0 + 4000;
    CHECK(du_sm_count(&m, a) && m.phase == DU_COUNT && m.count0 == a && du_sm_live(&m));
    CHECK(!du_sm_count(&m, a));  // (once)
    CHECK(du_sm_left(&m, a) == 0);  // (no clock during the count)
    CHECK(du_sm_step(&m, a + DU_COUNT_MS - 1) == DUS_NONE && m.phase == DU_COUNT);
    CHECK(du_sm_step(&m, a + DU_COUNT_MS) == DUS_FIGHT && m.phase == DU_FIGHT);
    CHECK(du_sm_left(&m, a + DU_COUNT_MS) == 60000 && du_sm_left(&m, a + DU_COUNT_MS + 59000) == 1000);
    du_sm_time_his(&m, 400);  // (his clock ran ahead: his TIME came during our fight)
    CHECK(!du_sm_time_ready(&m));
    CHECK(du_sm_step(&m, a + DU_COUNT_MS + 59999) == DUS_NONE);
    CHECK(du_sm_step(&m, a + DU_COUNT_MS + 60000) == DUS_TIMEUP && m.phase == DU_TIMEUP && du_sm_live(&m));
    CHECK(!du_sm_time_ready(&m));
    du_sm_time_mine(&m, 700);
    CHECK(du_sm_time_ready(&m) && du_time_result(m.my_pm, m.his_pm) == DUR_WIN);
    uint64_t e = a + DU_COUNT_MS + 60100;
    du_sm_end(&m, DUR_WIN, DUE_TIME, e);
    CHECK(m.phase == DU_OVER && m.result == DUR_WIN && m.reason == DUE_TIME && !du_sm_live(&m) && du_sm_busy(&m));
    CHECK(du_sm_step(&m, e + DU_OVER_MS - 1) == DUS_NONE);
    CHECK(du_sm_step(&m, e + DU_OVER_MS) == DUS_IDLE && m.phase == DU_IDLE && m.id == 0 && m.limit_ms == 60000);
    // the time wait: no TIME from him in 3 s
    du_sm_ask(&m, 7, T0), du_sm_count(&m, T0 + 100), du_sm_step(&m, T0 + 100 + DU_COUNT_MS);
    CHECK(du_sm_step(&m, T0 + 100 + DU_COUNT_MS + 60000) == DUS_TIMEUP);
    uint64_t tu = m.t0;
    CHECK(du_sm_step(&m, tu + DU_TIME_WAIT_MS - 1) == DUS_NONE);
    CHECK(du_sm_step(&m, tu + DU_TIME_WAIT_MS) == DUS_TIME_DECIDE);
    // the default limit = 3 min
    du_sm_reset(&m);
    m.limit_ms = 0;
    du_sm_ask(&m, 8, T0), du_sm_count(&m, T0), du_sm_step(&m, T0 + DU_COUNT_MS);
    CHECK(du_limit(&m) == 180000 && du_sm_left(&m, T0 + DU_COUNT_MS) == 180000);
    CHECK(du_sm_step(&m, T0 + DU_COUNT_MS + 179999) == DUS_NONE && du_sm_step(&m, T0 + DU_COUNT_MS + 180000) == DUS_TIMEUP);
    // an end from the asking phase
    du_sm_reset(&m), du_sm_ask(&m, 9, T0), du_sm_end(&m, DUR_CANCEL, DUE_CANCEL, T0 + 5);
    CHECK(m.phase == DU_OVER && !du_sm_live(&m));
    // the other game gone: no NF_DUEL after the grace / for 3 s
    CHECK(!du_peer_gone(0, 0, T0));
    CHECK(!du_peer_gone(T0, 0, T0 + DU_PEER_GRACE_MS - 1));
    CHECK(du_peer_gone(T0, 0, T0 + DU_PEER_GRACE_MS));       // (never seen since the accept)
    CHECK(du_peer_gone(T0, T0 - 50, T0 + DU_PEER_GRACE_MS)); // (a flag from before the accept does not count)
    CHECK(!du_peer_gone(T0, T0 + 2000, T0 + 4999));
    CHECK(du_peer_gone(T0, T0 + 2000, T0 + 2000 + DU_PEER_MS));
    CHECK(!du_peer_gone(T0, T0 + 9000, T0 + 9500));
}

static void seen_tests(void) {
    DuSeen d = {0};
    CHECK(du_seen_add(&d, 1) && !du_seen_add(&d, 1) && du_seen_add(&d, 2) && !du_seen_add(&d, 2) && !du_seen_add(&d, 1));
    for (uint16_t i = 3; i < 40; i++) CHECK(du_seen_add(&d, i));
    CHECK(d.n == 32);
    CHECK(!du_seen_add(&d, 39) && !du_seen_add(&d, 8));  // (the last 32 are kept)
    CHECK(du_seen_add(&d, 1));                           // (an old one that fell out counts again: a new duel resets the list anyway)
    CHECK(du_seen_add(&d, 0) && du_seen_add(&d, 65535));
}

static int pack(const NetDuelMsg *m, uint8_t *b) { return net_pack_duel(0x42, 0x0102, m, b, NET_PKT_MAX); }
static void wire_tests(void) {
    uint8_t b[NET_PKT_MAX];
    NetDuelMsg m = {0}, r;
    uint8_t pl = 0;
    uint16_t sq = 0;
    int t = 0;
    CHECK(NET_DUEL == 11 && NET_WARP == 10 && NET_VERSION == 12);  // (a new type, no version bump)
    CHECK(NF_DUEL == 2048 && !(NF_DUEL & (NF_WEAPON | NF_AIR | NF_GHOST | NF_GUARD | NF_DEAD | NF_LOADING | NF_BONEPOSE | NF_BONES | NF_SKATE | 512 | 1024)));
    CHECK(NET_DUEL_HEAD == 13 && NET_DUEL_ANSWER_LEN == 29 && NET_DUEL_HIT_LEN == 51 && NET_DUEL_END_LEN == 17 && NET_DUEL_TIME_LEN == 15 && NET_DUEL_RING_LEN == 31 && NET_DUEL_PULL_LEN == 37);
    CHECK(NET_DUEL_HIT_LEN <= NET_PKT_MAX);
    // bad inputs
    m.op = NDO_ASK;
    CHECK(pack(&m, b) < 0);  // (id 0)
    m.id = 0xDEADBEEF, m.op = 0;
    CHECK(pack(&m, b) < 0);
    m.op = 8;  // (s289: 7 = PULL now; 8 is the first unknown op)
    CHECK(pack(&m, b) < 0);
    m.op = NDO_HIT;
    CHECK(net_pack_duel(1, 1, &m, b, NET_DUEL_HIT_LEN - 1) < 0 && net_pack_duel(1, 1, NULL, b, 64) < 0);
    // ASK
    memset(&m, 0, sizeof m), m.op = NDO_ASK, m.target = 0x77, m.id = 0xDEADBEEF;
    int n = pack(&m, b);
    CHECK(n == 13 && b[3] == NET_DUEL);
    CHECK(net_unpack_ex(b, n, &t, &pl, &sq, NULL, NULL, NULL, NULL) && t == NET_DUEL && pl == 0x42 && sq == 0x0102);
    CHECK(net_unpack_duel(b, n, &pl, &sq, &r) && r.op == NDO_ASK && r.target == 0x77 && r.id == 0xDEADBEEF);
    CHECK(!net_unpack_duel(b, n - 1, &pl, &sq, &r) && !net_unpack_duel(b, n + 1, &pl, &sq, &r));  // (exact size)
    // ANSWER
    memset(&m, 0, sizeof m), m.op = NDO_ANSWER, m.target = 3, m.id = 99, m.ans = DUA_ACCEPT, m.why = 0, m.c[0] = -1234.5f, m.c[1] = 210.25f, m.c[2] = 2048.75f, m.radius_cm = 1000;
    n = pack(&m, b);
    CHECK(n == 29 && net_unpack_duel(b, n, &pl, &sq, &r) && r.ans == DUA_ACCEPT && r.c[0] == -1234.5f && r.c[1] == 210.25f && r.c[2] == 2048.75f && r.radius_cm == 1000);
    m.ans = DUA_BUSY, m.why = DUG_ENEMY;
    n = pack(&m, b);
    CHECK(net_unpack_duel(b, n, &pl, &sq, &r) && r.ans == DUA_BUSY && r.why == DUG_ENEMY);
    m.ans = 0, n = pack(&m, b);
    CHECK(!net_unpack_duel(b, n, &pl, &sq, &r));  // (answer 0)
    m.ans = 4, n = pack(&m, b);
    CHECK(!net_unpack_duel(b, n, &pl, &sq, &r));
    m.ans = DUA_ACCEPT, m.c[1] = NAN, n = pack(&m, b);
    CHECK(!net_unpack_duel(b, n, &pl, &sq, &r));  // (a NaN centre)
    m.c[1] = INFINITY, n = pack(&m, b);
    CHECK(!net_unpack_duel(b, n, &pl, &sq, &r));
    // HIT
    memset(&m, 0, sizeof m), m.op = NDO_HIT, m.target = 9, m.id = 5, m.hseq = 65535, m.power = 250, m.stat = 31, m.flags = NDH_SPELL | NDH_FINISH, m.src = 3;
    m.boost = 20, m.react = 7, m.atkp = 502;
    for (int j = 0; j < 3; j++) m.from[j] = 1.5f * (float)(j + 1), m.at[j] = -2.25f * (float)(j + 1);
    n = pack(&m, b);
    CHECK(n == 51 && net_unpack_duel(b, n, &pl, &sq, &r));
    CHECK(r.hseq == 65535 && r.power == 250 && r.stat == 31 && r.flags == (NDH_SPELL | NDH_FINISH) && r.src == 3 && r.boost == 20 && r.react == 7 && r.atkp == 502);
    CHECK(r.from[0] == 1.5f && r.from[2] == 4.5f && r.at[1] == -4.5f && r.at[2] == -6.75f);
    m.at[2] = NAN, n = pack(&m, b);
    CHECK(!net_unpack_duel(b, n, &pl, &sq, &r));
    // END / TIME
    memset(&m, 0, sizeof m), m.op = NDO_END, m.id = 6, m.reason = DUE_KO, m.result = DUR_LOSE, m.pm = 20;
    n = pack(&m, b);
    CHECK(n == 17 && net_unpack_duel(b, n, &pl, &sq, &r) && r.reason == DUE_KO && r.result == DUR_LOSE && r.pm == 20);
    memset(&m, 0, sizeof m), m.op = NDO_TIME, m.id = 6, m.pm = 1000;
    n = pack(&m, b);
    CHECK(n == 15 && net_unpack_duel(b, n, &pl, &sq, &r) && r.op == NDO_TIME && r.pm == 1000);
    // RING
    memset(&m, 0, sizeof m), m.op = NDO_RING, m.id = 77, m.a = 200, m.b = 13, m.phase = 2, m.c[0] = 10, m.c[1] = 20, m.c[2] = 30, m.radius_cm = 1500;
    n = pack(&m, b);
    CHECK(n == 31 && net_unpack_duel(b, n, &pl, &sq, &r) && r.a == 200 && r.b == 13 && r.phase == 2 && r.c[2] == 30 && r.radius_cm == 1500 && r.target == 0);
    // id 0 on the wire, an unknown op, a wrong length for the op: refused by net_unpack_ex itself
    uint8_t c[NET_PKT_MAX];
    memcpy(c, b, (size_t)n);
    c[NET_HDR + 2] = c[NET_HDR + 3] = c[NET_HDR + 4] = c[NET_HDR + 5] = 0;
    CHECK(net_unpack_ex(c, n, &t, &pl, &sq, NULL, NULL, NULL, NULL) && !net_unpack_duel(c, n, &pl, &sq, &r));
    memcpy(c, b, (size_t)n), c[NET_HDR] = 8;  // (s289: the first unknown op; 7 = PULL)
    CHECK(!net_unpack_ex(c, n, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    memcpy(c, b, (size_t)n), c[NET_HDR] = NDO_PULL;  // (a RING-sized PULL)
    CHECK(!net_unpack_ex(c, n, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    memcpy(c, b, (size_t)n), c[NET_HDR] = 0;
    CHECK(!net_unpack_ex(c, n, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    memcpy(c, b, (size_t)n), c[NET_HDR] = NDO_ASK;  // (a RING-sized ASK)
    CHECK(!net_unpack_ex(c, n, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    CHECK(!net_unpack_ex(b, NET_DUEL_HEAD - 1, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    // every op's exact size only
    for (int op = NDO_ASK; op <= NDO_PULL; op++) {
        memset(&m, 0, sizeof m), m.op = (uint8_t)op, m.id = 3, m.ans = 1;
        n = pack(&m, b);
        CHECK(n == NET_DUEL_LEN[op] && net_unpack_duel(b, n, &pl, &sq, &r) && r.op == op);
        for (int l = NET_HDR; l <= NET_DUEL_HIT_LEN + 2; l++) if (l != n) CHECK(!net_unpack_ex(b, l, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    }
    // NF_DUEL rides a SNAP (the flags are copied raw)
    NetSnap s, sr;
    memset(&s, 0, sizeof s), s.player = 5, s.seq = 9, s.flags = NF_DUEL | NF_AIR;
    n = net_pack_snap(&s, b, sizeof b);
    char name[NET_NAME_MAX + 1];
    memset(&sr, 0xcc, sizeof sr);
    CHECK(n > 0 && net_unpack(b, n, &t, &pl, &sq, &sr, name) && t == NET_SNAP && sr.flags == (NF_DUEL | NF_AIR));
}

// (s288 port) du_block_why: why a friend's duel hit is blocked (du_take_hit). User ruling 2026-11-25: the Dodge Roll's
// (rollinv, via g_rc_inv) and the Aerial Dodge block's (adblock) frames block duel hits too; a paused player takes the hit.
static void block_tests(void) {
    //                      paused guard  reflect inv    adodge limit
    CHECK(du_block_why(false, false, false, false, false, false) == DUB_NONE);
    CHECK(du_block_why(false, true, false, false, false, false) == DUB_GUARD);
    CHECK(du_block_why(false, false, true, false, false, false) == DUB_REFLECT);
    CHECK(du_block_why(false, false, false, true, false, false) == DUB_INV);      // (Dodge Roll / Quick Run / post-block frames)
    CHECK(du_block_why(false, false, false, false, true, false) == DUB_ADODGE);   // (the Aerial Dodge block)
    CHECK(du_block_why(false, false, false, false, false, true) == DUB_LIMIT);
    CHECK(du_block_why(true, true, true, true, true, true) == DUB_NONE);          // (paused: every reason off, s284)
    CHECK(du_block_why(true, false, false, false, true, false) == DUB_NONE);
    // the old log order: Guard, then Reflect, then the invulnerable frames, then Limit; the Aerial Dodge after the frames
    CHECK(du_block_why(false, true, true, true, true, true) == DUB_GUARD);
    CHECK(du_block_why(false, false, true, true, true, true) == DUB_REFLECT);
    CHECK(du_block_why(false, false, false, true, true, true) == DUB_INV);
    CHECK(du_block_why(false, false, false, false, true, true) == DUB_ADODGE);
    // the log text: both dodges read "invulnerable frames" (the main session greps "blocked: invulnerable frames")
    CHECK(!strcmp(du_block_name(DUB_INV), "invulnerable frames"));
    CHECK(!strncmp(du_block_name(DUB_ADODGE), "invulnerable frames", 19));
    CHECK(!strcmp(du_block_name(DUB_GUARD), "Guard") && !strcmp(du_block_name(DUB_REFLECT), "Reflect") &&
          !strcmp(du_block_name(DUB_LIMIT), "Limit"));
}

// ---- (s289 duelreflect) Reflect's burst on the duel opponent ----
// The KH2 numbers below are the atkp rows' own (sb/s289/duelplan/rows.txt, 00battle.bin, VERIFIED; the game reads them from
// sora_magic2.h MG_REF_*): A rows 574 / 576 / 578 Power 50, Interval 10 / 8 / 6 at SubId 0 / 512 / 1024 (Reflect / Reflera /
// Reflega); B rows 667 / 668 / 669 Power 550, EnemyReaction 6 / 6 / 7; the burst cylinder radius 240 .. 440 cm, height 150 /
// 180 / 220 (+20 each for B). MG_REF_TA = 28 (A's life) is the s133 script field (GUESS meaning).
#define REF_TA 28.0f
// one burst in mg_reflect_tick's call order: the barrier's end = an A call at bt 0, then every frame bt += kh and an A call, and
// in the frame bt reaches REF_TA the one B call. in(bt) = the opponent inside the cylinder then. -> the A hits; *nb = the B hits
typedef bool (*ReflIn)(float bt);
static bool in_always(float bt) { (void)bt; return true; }
static bool in_never(float bt) { (void)bt; return false; }
static bool in_from10(float bt) { return bt >= 10; }               // (walks in at bt 10)
static bool in_gap(float bt) { return bt < 1 || bt >= 20; }         // (in at the start, out 1 .. 19, back from 20)
static bool in_not_b(float bt) { return bt < REF_TA; }              // (out of reach at the B call)
static int refl_run(DuRefl *s, float kh, float iv, ReflIn in, int *nb) {
    int na = 0;
    float bt = 0;
    *nb = 0;
    int w = du_refl_step(s, bt, false, iv, in(bt));
    if (w == DU_REFL_A) na++;
    CHECK(w != DU_REFL_B);
    for (int frame = 0; frame < 1000; frame++) {
        bt += kh;
        w = du_refl_step(s, bt, false, iv, in(bt));
        CHECK(w != DU_REFL_B);  // (an A call never gives B)
        if (w == DU_REFL_A) na++;
        if (bt >= REF_TA) {
            if (du_refl_step(s, bt, true, iv, in(bt)) == DU_REFL_B) (*nb)++;
            CHECK(du_refl_step(s, bt, true, iv, in(bt)) == DU_REFL_NONE);  // (a second B call: nothing)
            break;
        }
    }
    CHECK(s->na == na && s->nb == *nb);
    return na;
}
static void reflect_tests(void) {
    // the A count per spell tier (Interval 10 / 8 / 6) at kh 1 (60 fps) and kh 2 (30 fps), then exactly one B
    static const float IV[3] = {10, 8, 6};
    static const int NA[3] = {3, 4, 5};  // (bt 0, 10, 20 | 0, 8, 16, 24 | 0, 6, 12, 18, 24: the next is past 28 each time)
    DuRefl s;
    memset(&s, 0, sizeof s);
    for (int k = 1; k <= 2; k++)
        for (int t = 0; t < 3; t++) {
            int nb = -1, na = refl_run(&s, (float)k, IV[t], in_always, &nb);
            CHECK(na == NA[t] && nb == 1);
            if (na != NA[t] || nb != 1) printf("  (kh %d tier %d: %d A, %d B)\n", k, t + 1, na, nb);
        }
    // the next burst starts clean (its bt 0 call resets the last one's state: B done, the A timer)
    int nb = -1;
    CHECK(s.b_done && s.a_on);
    CHECK(refl_run(&s, 2, 10, in_always, &nb) == 3 && nb == 1);
    // out of the cylinder the whole burst: nothing; the reset still happens at bt 0 (a key turned on mid-burst starts clean)
    CHECK(refl_run(&s, 2, 10, in_never, &nb) == 0 && nb == 0);
    CHECK(!s.a_on && !s.b_done && s.na == 0);
    // walks in at bt 10: the first contact hits at once, then the Interval (bt 10, 20) + B
    CHECK(refl_run(&s, 2, 10, in_from10, &nb) == 2 && nb == 1);
    // in at 0, out, back at 20: bt 0, then 20 (past next = 10) + B
    CHECK(refl_run(&s, 2, 10, in_gap, &nb) == 2 && nb == 1);
    // out of reach at the B call: the A hits only
    CHECK(refl_run(&s, 2, 10, in_not_b, &nb) == 3 && nb == 0);
    // an Interval of 0 (no row timer): every A call hits (kh 2: bt 0, 2, .., 28 = 15 calls)
    CHECK(refl_run(&s, 2, 0, in_always, &nb) == 15 && nb == 1);

    // the damage table (duel plan 2.1; Standard difficulty = sl_khdiff 1, dueldmg 100 -> 50 % = the compiled default)
    static const struct { int v, a100, a50, b100, b50; } D[3] = {{1, 1, 1, 6, 3}, {5, 3, 2, 28, 14}, {10, 5, 3, 55, 28}};
    for (int i = 0; i < 3; i++) {
        NetDuelMsg a, b;
        memset(&a, 0, sizeof a), memset(&b, 0, sizeof b);
        du_refl_hit(&a, false, 50, 574, 3, 13 + D[i].v);  // (S = Mag + Keyblade Mag; Def 13: V = S - Def)
        du_refl_hit(&b, true, 550, 667, 6, 13 + D[i].v);
        int v = -1, da = du_dmg(a.stat, 13, a.power, a.boost, 1, &v);
        CHECK(v == D[i].v && da == D[i].a100 && du_dmg_scale(da, 50) == D[i].a50 && du_dmg_scale(da, 100) == D[i].a100);
        int db = du_dmg(b.stat, 13, b.power, b.boost, 1, &v);
        CHECK(v == D[i].v && db == D[i].b100 && du_dmg_scale(db, 50) == D[i].b50);
    }
    // the cylinder (du_body_near, -H .. 2H as the enemies'): Reflect lv1, burst tier 1 at g_k 1 (to_u 0.01): A r 2.4 h 1.5, B r 2.6 h 1.7
    float c[3] = {100, 50, -20}, ra = 2.4f, ha = 1.5f, rb = 2.6f, hb = 1.7f;
#define AT(dx, dy, dz) (float[3]){c[0] + (dx), c[1] + (dy), c[2] + (dz)}
    CHECK(du_body_near(c, AT(2.3f, 0, 0), ra, -ha, 2 * ha) && du_body_near(c, AT(0, 0, -2.3f), ra, -ha, 2 * ha));
    CHECK(!du_body_near(c, AT(2.5f, 0, 0), ra, -ha, 2 * ha) && du_body_near(c, AT(2.5f, 0, 0), rb, -hb, 2 * hb));  // (2.5 m: B only)
    CHECK(!du_body_near(c, AT(3.0f, 0, 0), ra, -ha, 2 * ha) && !du_body_near(c, AT(0, 0, 3.0f), rb, -hb, 2 * hb));  // (duelfake's 3 m: out)
    CHECK(du_body_near(c, AT(1.69f, 0, 1.69f), ra, -ha, 2 * ha) && !du_body_near(c, AT(1.71f, 0, 1.71f), ra, -ha, 2 * ha));  // (2.39 / 2.42 m)
    CHECK(du_body_near(c, AT(0.5f, -1.49f, 0), ra, -ha, 2 * ha) && !du_body_near(c, AT(0.5f, -1.52f, 0), ra, -ha, 2 * ha));  // (below: -H)
    CHECK(du_body_near(c, AT(0.5f, 2.99f, 0), ra, -ha, 2 * ha) && !du_body_near(c, AT(0.5f, 3.02f, 0), ra, -ha, 2 * ha));    // (above: 2H)
    CHECK(!du_body_near(c, AT(NAN, 0, 0), ra, -ha, 2 * ha) && !du_body_near(c, AT(0, NAN, 0), ra, -ha, 2 * ha));             // (a NaN pose: out)
    CHECK(du_body_near(c, AT(4.3f, 0, 0), 4.4f, -2.2f, 4.4f) && !du_body_near(c, AT(4.5f, 0, 0), 4.4f, -2.2f, 4.4f));       // (Reflega, burst tier 3: 4.4 m)
#undef AT

    // src 7's name: the victim's "hit by ... (Reflect[ finisher], #n)", the attacker's "HIT ... (#n, Reflect[ finisher], ...)";
    // 0..3 as in s272, 4 kbthrow's; 5 / 6 reserved, no name yet; an unknown one
    CHECK(DU_SRC_REFLECT == 7 && DU_SRC_BLADE == 0 && DU_SRC_FIRE == 1 && DU_SRC_BLIZZARD == 2 && DU_SRC_THUNDER == 3 && DU_SRC_KBTHROW == 4);
    CHECK(!strcmp(du_src_name(0), "blade") && !strcmp(du_src_name(1), "Fire") && !strcmp(du_src_name(2), "Blizzard") && !strcmp(du_src_name(3), "Thunder"));
    CHECK(!strcmp(du_src_name(7), "Reflect") && !strcmp(du_src_kind(7), "Reflect") && !strcmp(du_src_name(4), "Keyblade throw"));
    CHECK(!strcmp(du_src_name(5), "another attack") && !strcmp(du_src_name(6), "Magnet"));  // (s289 duelmagnet: 6 = Magnet)
    CHECK(!strcmp(du_src_name(8), "another attack") && !strcmp(du_src_name(255), "another attack") && !strcmp(du_src_name(-1), "another attack"));

    // a src 7 HIT on the wire (du_hit_out fills op / id / hseq / from / at), A then B
    uint8_t buf[NET_PKT_MAX];
    uint8_t pl = 0;
    uint16_t sq = 0;
    NetDuelMsg m, r;
    memset(&m, 0, sizeof m);
    du_refl_hit(&m, false, 50, 576, 4, 21);
    m.op = NDO_HIT, m.id = 0x5EED, m.hseq = 41, m.target = 2;
    for (int j = 0; j < 3; j++) m.from[j] = 10.0f * (float)(j + 1), m.at[j] = 11.5f * (float)(j + 1);
    int n = pack(&m, buf);
    CHECK(n == NET_DUEL_HIT_LEN && net_unpack_duel(buf, n, &pl, &sq, &r));
    CHECK(r.op == NDO_HIT && r.src == 7 && r.flags == NDH_SPELL && r.power == 50 && r.atkp == 576 && r.react == 4 && r.stat == 21 && r.boost == 0 && r.hseq == 41);
    CHECK(r.from[1] == 20.0f && r.at[2] == 34.5f && !strcmp(du_src_name(r.src), "Reflect"));
    du_refl_hit(&m, true, 550, 669, 7, 21);
    m.hseq = 42, n = pack(&m, buf);
    CHECK(n == NET_DUEL_HIT_LEN && net_unpack_duel(buf, n, &pl, &sq, &r));
    CHECK(r.src == 7 && r.flags == (NDH_SPELL | NDH_FINISH) && r.power == 550 && r.atkp == 669 && r.react == 7 && r.hseq == 42);
    // the victim takes each burst hit once (UDP copies: the s272 de-dupe)
    DuSeen sn;
    memset(&sn, 0, sizeof sn);
    CHECK(du_seen_add(&sn, 41) && du_seen_add(&sn, 42) && !du_seen_add(&sn, 42));
    // du_refl_hit's clamps (the wire's u8 / u16)
    du_refl_hit(&m, false, -5, 70000, 300, -1);
    CHECK(m.power == 0 && m.atkp == 65535 && m.react == 255 && m.stat == 0);
}

// ---- (s289 lab hooks) the live "duel act <verb>" lines ----
static void act_tests(void) {
    int v = -1;
    CHECK(du_act_line("act reflect", &v) == DUAL_OK && v == DUACT_REFLECT);
    CHECK(du_act_line("act magnet", &v) == DUAL_OK && v == DUACT_MAGNET);
    CHECK(du_act_line("act throw", &v) == DUAL_OK && v == DUACT_THROW);
    CHECK(du_act_line("act guard", &v) == DUAL_OK && v == DUACT_GUARD);
    CHECK(du_act_line("act roll", &v) == DUAL_OK && v == DUACT_ROLL);
    CHECK(du_act_line("act   roll  ", &v) == DUAL_OK && v == DUACT_ROLL);   // (spaces around the verb)
    CHECK(du_act_line("act\troll\r\n", &v) == DUAL_OK && v == DUACT_ROLL);  // (a CR LF file)
    CHECK(du_act_line("act", &v) == DUAL_EMPTY && v == DUACT_NONE);
    CHECK(du_act_line("act   ", &v) == DUAL_EMPTY);
    CHECK(du_act_line("act jump", &v) == DUAL_BAD && v == DUACT_NONE);       // (no such verb)
    CHECK(du_act_line("act Reflect", &v) == DUAL_BAD);                        // (lower case, as every live line)
    CHECK(du_act_line("act refl", &v) == DUAL_BAD && du_act_line("act reflects", &v) == DUAL_BAD);
    CHECK(du_act_line("act reflect now", &v) == DUAL_BAD && v == DUACT_NONE); // (one verb, no arguments yet)
    CHECK(du_act_line("actor", &v) == DUAL_NO && du_act_line("act-reflect", &v) == DUAL_NO);  // (not an act line: the old usage line)
    CHECK(du_act_line("ask 1", &v) == DUAL_NO && du_act_line("accept", &v) == DUAL_NO && du_act_line("", &v) == DUAL_NO);
    CHECK(du_act_line(NULL, &v) == DUAL_NO);
    // the verb table: names round trip, kinds, the usage text names them all
    CHECK(du_act_kind(DUACT_REFLECT) == DUAK_CAST && du_act_kind(DUACT_MAGNET) == DUAK_CAST && du_act_kind(DUACT_THROW) == DUAK_HOLD);
    CHECK(du_act_kind(DUACT_GUARD) == DUAK_PRESS && du_act_kind(DUACT_ROLL) == DUAK_PRESS);
    CHECK(du_act_kind(DUACT_NONE) == DUAK_NONE && du_act_kind(DUACT_N) == DUAK_NONE && du_act_kind(-3) == DUAK_NONE);
    CHECK(!strcmp(du_act_name(DUACT_NONE), "?") && !strcmp(du_act_name(DUACT_N), "?") && !strcmp(du_act_name(DUACT_THROW), "throw"));
    for (int i = DUACT_NONE + 1; i < DUACT_N; i++) {
        const char *nm = DU_ACT_VERBS[i].name;
        CHECK(nm && nm[0] && du_act_find(nm, strlen(nm)) == i && !strcmp(du_act_name(i), nm) && strstr(DU_ACT_USAGE, nm));
        CHECK(du_act_kind(i) != DUAK_NONE);
    }
    CHECK(du_act_find("guardx", 5) == DUACT_GUARD && du_act_find("guard", 4) == DUACT_NONE && du_act_find("", 0) == DUACT_NONE);
    // the pending act: a new one replaces a waiting one (that one is named for its line); the clocks
    DuAct a;
    memset(&a, 0, sizeof a);
    CHECK(!du_act_late(&a, T0 + 999999, 0, 1));  // (nothing pending: never late)
    CHECK(du_act_queue(&a, DUACT_REFLECT, T0) == DUACT_NONE && a.ph == DUAP_WAIT && a.verb == DUACT_REFLECT && a.t0 == T0 && a.spell == -1);
    a.ph = DUAP_CHECK, a.stick = 2, a.spell = 5, snprintf(a.why, sizeof a.why, "x");
    CHECK(du_act_queue(&a, DUACT_GUARD, T0 + 5) == DUACT_REFLECT);
    CHECK(a.ph == DUAP_WAIT && a.verb == DUACT_GUARD && a.t0 == T0 + 5 && a.stick == 0 && a.spell == -1 && !a.why[0]);
    CHECK(!du_act_late(&a, T0 + 5 + DU_ACT_WAIT_MS, a.t0, DU_ACT_WAIT_MS) && du_act_late(&a, T0 + 6 + DU_ACT_WAIT_MS, a.t0, DU_ACT_WAIT_MS));
    CHECK(du_act_late(&a, T0 + 6 + DU_ACT_EXPIRE_MS, a.t0, DU_ACT_EXPIRE_MS) && DU_ACT_WAIT_MS < DU_ACT_EXPIRE_MS);
    CHECK(du_act_queue(&a, DUACT_ROLL, 0) == DUACT_GUARD && a.t0 == 1);  // (a zero clock: never 0)
}

// (s289 kbthrow) the thrown Keyblade against the duel opponent: sora_duel_core.h du_throw_near / du_throw_step / du_throw_power /
// du_src_name (used by sora_duel.h du_throw, which sora_limit.h lm_frame calls after its enemy test). throw_fly copies lm_frame's
// kbthrow flight: 30 KH cm a KH frame, out kbthrowout 24 KH frames, then straight back to the thrower's chest (the way back turns at
// once), caught within 60 cm; a hit on the way out turns it back, as at an enemy target. kh 2 (30 fps), the tip 1 m up, 0.5 m
// ahead (GUESS cast_tip). The friend stands at a until frame step_at, then at b + hv x (frames since step_at); face = his yaw.
typedef struct { int out, back, out_front, back_front; } ThrowRun;
static ThrowRun throw_fly(const float me[3], float yaw, const float a[3], const float b[3], const float hv[3], int step_at, float face) {
    const float to_u = 0.01f, kh = 2;
    ThrowRun r = {0};
    float pos[3] = {me[0] + sinf(yaw) * 0.5f, me[1] + 1.0f, me[2] + cosf(yaw) * 0.5f}, dir[3] = {sinf(yaw), 0, cosf(yaw)}, t = 0;
    bool back = false;
    uint8_t legs = 0;
    for (int f = 1; f < 200; f++) {
        t += kh;
        if (back) {  // (lm_frame: back to his chest, the turn without a limit; caught within 60 cm)
            float aim[3] = {me[0] - pos[0], me[1] + 1.0f - pos[1], me[2] - pos[2]};
            float d = sqrtf(aim[0] * aim[0] + aim[1] * aim[1] + aim[2] * aim[2]);
            if (d < 60 * to_u) break;
            for (int c = 0; c < 3; c++) dir[c] = aim[c] / d;
        }
        float prev[3] = {pos[0], pos[1], pos[2]};
        for (int c = 0; c < 3; c++) pos[c] += dir[c] * 30 * kh * to_u;
        const float *base = f < step_at ? a : b;
        float k = f < step_at ? 0 : (float)(f - step_at);
        float him[3] = {base[0] + hv[0] * k, base[1] + hv[1] * k, base[2] + hv[2] * k};
        int h = du_throw_step(&legs, back, him, pos, DU_THROW_R_CM, to_u);
        if (h == DUT_OUT) r.out++, r.out_front = du_from_front(him, face, prev), back = true;  // (lm_frame: o->back = true, o->nhit = 0)
        else if (h == DUT_BACK) r.back++, r.back_front = du_from_front(him, face, prev);
        if (!back && t >= 24) back = true;  // (no target reached: back after kbthrowout frames)
    }
    return r;
}
static void throw_tests(void) {
    const float to_u = 0.01f;  // (g_k 1: KH cm -> BotW units)
    const float feet[3] = {10, 5, 20};
    // the contact test: his feet within a flat 160 cm of the blade, the blade -50..220 cm above his feet (lm_frame's enemy test)
    float b[3] = {feet[0] + 1.59f, feet[1] + 1.0f, feet[2]};
    CHECK(DU_THROW_R_CM == 160);
    CHECK(du_throw_near(feet, b, DU_THROW_R_CM, to_u));
    b[0] = feet[0] + 1.61f;
    CHECK(!du_throw_near(feet, b, DU_THROW_R_CM, to_u));
    b[0] = feet[0] + 1.12f, b[2] = feet[2] - 1.12f;  // (flat 1.584 m on a diagonal)
    CHECK(du_throw_near(feet, b, DU_THROW_R_CM, to_u));
    b[0] = feet[0] + 1.14f, b[2] = feet[2] - 1.14f;  // (1.612 m)
    CHECK(!du_throw_near(feet, b, DU_THROW_R_CM, to_u));
    b[0] = feet[0], b[2] = feet[2], b[1] = feet[1] - 0.49f;
    CHECK(du_throw_near(feet, b, DU_THROW_R_CM, to_u));
    b[1] = feet[1] - 0.51f;  // (below his feet by more than 50 cm)
    CHECK(!du_throw_near(feet, b, DU_THROW_R_CM, to_u));
    b[1] = feet[1] + 2.19f;
    CHECK(du_throw_near(feet, b, DU_THROW_R_CM, to_u));
    b[1] = feet[1] + 2.21f;  // (over his head by more than 220 cm)
    CHECK(!du_throw_near(feet, b, DU_THROW_R_CM, to_u));
    b[1] = NAN;
    CHECK(!du_throw_near(feet, b, DU_THROW_R_CM, to_u));
    // live duelthrowr tunes the flat reach; g_k scales it (to_u)
    b[1] = feet[1] + 1, b[0] = feet[0] + 1.3f;
    CHECK(du_throw_near(feet, b, DU_THROW_R_CM, to_u) && !du_throw_near(feet, b, 100, to_u) && du_throw_near(feet, b, 140, to_u));
    b[0] = feet[0] + 1.9f;
    CHECK(!du_throw_near(feet, b, DU_THROW_R_CM, to_u) && du_throw_near(feet, b, DU_THROW_R_CM, 0.012f));  // (g_k 1.2: 192 cm)

    // one hit per leg: the way out once, the way back once; a miss uses no leg
    uint8_t legs = 0;
    const float in[3] = {feet[0] + 1.0f, feet[1] + 1.0f, feet[2]}, out_of[3] = {feet[0] + 3.0f, feet[1] + 1.0f, feet[2]};
    CHECK(du_throw_step(&legs, false, feet, in, DU_THROW_R_CM, to_u) == DUT_OUT && legs == 1);
    CHECK(du_throw_step(&legs, false, feet, in, DU_THROW_R_CM, to_u) == DUT_NONE);  // (the way out had its hit)
    CHECK(du_throw_step(&legs, true, feet, in, DU_THROW_R_CM, to_u) == DUT_BACK && legs == 3);
    CHECK(du_throw_step(&legs, true, feet, in, DU_THROW_R_CM, to_u) == DUT_NONE);
    CHECK(du_throw_step(&legs, false, feet, in, DU_THROW_R_CM, to_u) == DUT_NONE);
    legs = 0;
    CHECK(du_throw_step(&legs, false, feet, out_of, DU_THROW_R_CM, to_u) == DUT_NONE && legs == 0);
    CHECK(du_throw_step(&legs, true, feet, in, DU_THROW_R_CM, to_u) == DUT_BACK && legs == 2);  // (missed out, hit back)
    CHECK(du_throw_step(&legs, true, feet, in, DU_THROW_R_CM, to_u) == DUT_NONE);
    CHECK(du_throw_step(NULL, false, feet, in, DU_THROW_R_CM, to_u) == DUT_NONE);

    // whole flights (throw_fly): Sora at the origin throws along +z
    const float me[3] = {0, 0, 0}, z0[3] = {0, 0, 0}, PI_ = 3.14159265f;
    // A: the friend 5 m ahead, facing Sora: one hit on the way out, "from" his front (a Guard facing the blade blocks it);
    //    the blade turns back there and flies away from him: no second hit
    const float at5[3] = {0, 0, 5};
    ThrowRun r = throw_fly(me, 0, at5, at5, z0, 0, PI_);
    CHECK(r.out == 1 && r.back == 0 && r.out_front == 1);
    // B: the same, his back to Sora: the hit comes from behind (KH2's Guard blocks the front only)
    r = throw_fly(me, 0, at5, at5, z0, 0, 0);
    CHECK(r.out == 1 && r.back == 0 && r.out_front == 0);
    // C: he stands 2.5 m off the line while the blade flies out (no hit), then steps onto it: the way back hits him once,
    //    "from" behind him (he faces Sora; the blade comes home from beyond him)
    const float side[3] = {2.5f, 0, 6}, line[3] = {0, 0, 6};
    r = throw_fly(me, 0, side, line, z0, 13, PI_);
    CHECK(r.out == 0 && r.back == 1 && r.back_front == 0);
    // D: he runs at Sora (0.3 m a frame): hit on the way out, then once more on the way back (both from the front), never more
    const float run[3] = {0, 0, -0.3f};
    r = throw_fly(me, 0, at5, at5, run, 0, PI_);
    CHECK(r.out == 1 && r.back == 1 && r.out_front == 1 && r.back_front == 1);
    // E: he stands 2.5 m below the throw's line (a slope): the blade 3.5 m above his feet never meets him (> 220 cm)
    const float low[3] = {0, -2.5f, 5};
    r = throw_fly(me, 0, low, low, z0, 0, PI_);
    CHECK(r.out == 0 && r.back == 0);
    // F: off to the side by more than 1.6 m the whole time: nothing on either leg
    const float off[3] = {1.8f, 0, 5};
    r = throw_fly(me, 0, off, off, z0, 0, PI_);
    CHECK(r.out == 0 && r.back == 0);

    // src 4's name (the victim's "hit by ... (Keyblade throw, #n)"; the attacker's "HIT ... (#n, Keyblade throw, ...)")
    CHECK(DU_SRC_BLADE == 0 && DU_SRC_FIRE == 1 && DU_SRC_BLIZZARD == 2 && DU_SRC_THUNDER == 3 && DU_SRC_KBTHROW == 4);
    CHECK(!strcmp(du_src_name(DU_SRC_KBTHROW), "Keyblade throw"));
    CHECK(!strcmp(du_src_name(0), "blade") && !strcmp(du_src_name(1), "Fire") && !strcmp(du_src_name(2), "Blizzard") &&
          !strcmp(du_src_name(3), "Thunder"));  // (the s272 victim log words, unchanged)
    CHECK(!strcmp(du_src_kind(0), "blade") && !strcmp(du_src_kind(1), "spell") && !strcmp(du_src_kind(2), "spell") &&
          !strcmp(du_src_kind(3), "spell") && !strcmp(du_src_kind(4), "Keyblade throw"));  // (the attacker / fake log words)
    bool named = true;  // (5 BotW weapon throw, 6 Magnet are reserved: no name yet, still printable; 7 Reflect has its name since
                        //  the s289 duelreflect merge: reflect_tests)
    for (int s = -1; s < 256; s++) named = named && du_src_name(s) && du_src_name(s)[0] && du_src_kind(s) && du_src_kind(s)[0];
    CHECK(named && !strcmp(du_src_name(5), "another attack") && !strcmp(du_src_name(6), "Magnet"));  // (s289 duelmagnet: 6 = Magnet)

    // Power 100 damage: Power 200 (atkp 1866) x kbthrowdmg 50 %, KH2's formula as a physical hit (S = Str + Keyblade Str)
    CHECK(du_throw_power(200, 50) == 100);
    CHECK(du_throw_power(200, 100) == 200 && du_throw_power(200, 1) == 2 && du_throw_power(200, 400) == 800 && du_throw_power(200, 33) == 66);
    CHECK(du_throw_power(1, 1) == 1);  // (never 0)
    int v = 0;
    CHECK(du_dmg(12 + 3, 13, du_throw_power(200, 50), 0, 1, &v) == 2 && v == 2 && du_dmg_scale(2, 50) == 1);  // (S 15 vs Def 13)
    CHECK(du_dmg(30 + 3, 20, du_throw_power(200, 50), 0, 2, &v) == 26 && v == 13 && du_dmg_scale(26, 50) == 13);  // (Proud x2, dueldmg 50)
    CHECK(du_dmg(33, 20, du_throw_power(200, 50), 0, 1, NULL) == du_dmg(33, 20, 100, 0, 1, NULL));  // (= a plain Power 100 blade hit)

    // the HIT a throw sends: src 4, Power 100, atkp 1866, react 8, no flags; the wire carries it as it is (no new op, NET_VERSION 12)
    uint8_t w[NET_PKT_MAX], pl = 0;
    uint16_t sq = 0;
    NetDuelMsg m = {.op = NDO_HIT, .id = 7, .hseq = 3, .power = (uint16_t)du_throw_power(200, 50), .stat = 15, .src = DU_SRC_KBTHROW,
                    .react = 8, .atkp = 1866}, rm;
    m.from[0] = 1.5f, m.from[1] = 2, m.from[2] = 3.25f, m.at[0] = 4, m.at[1] = 5, m.at[2] = 6;
    int n = net_pack_duel(1, 1, &m, w, NET_PKT_MAX);
    CHECK(n == NET_DUEL_HIT_LEN && net_unpack_duel(w, n, &pl, &sq, &rm));
    CHECK(rm.src == DU_SRC_KBTHROW && rm.power == 100 && rm.stat == 15 && rm.atkp == 1866 && rm.react == 8 && rm.flags == 0 &&
          rm.from[0] == 1.5f && rm.from[2] == 3.25f && rm.at[2] == 6);
    CHECK(NET_VERSION == 12);
}

// (s289 3c, the user's pick) du_god_waits / du_god_edge: the Debug Menu's God Mode in a duel (sora_dbgmenu.h dbm_god_tick_, live
// duelgod). One God Mode frame with the row on: -> the refill waits now; np / nb count the "paused" / "back" log lines.
typedef struct { bool dw; int np, nb; } GodLog;
static bool god_frame_(GodLog *g, bool key, const DuSm *m, bool ko_hold) {
    int e = du_god_edge(&g->dw, du_god_waits(key, du_sm_live(m), ko_hold));
    g->np += e > 0, g->nb += e < 0;
    return g->dw;
}
static void god_tests(void) {
    // the rule: duelgod 0 never waits (as before); duelgod 1 waits while live or this game's KO hold runs
    for (int i = 0; i < 4; i++) CHECK(!du_god_waits(false, i & 1, i & 2));
    CHECK(!du_god_waits(true, false, false) && du_god_waits(true, true, false) && du_god_waits(true, false, true) && du_god_waits(true, true, true));
    // the edge: one +1 into the wait, one -1 out of it, 0 while it stays
    bool p = false;
    CHECK(du_god_edge(&p, false) == 0 && !p);
    CHECK(du_god_edge(&p, true) == 1 && p);
    CHECK(du_god_edge(&p, true) == 0 && p);
    CHECK(du_god_edge(&p, false) == -1 && !p);
    CHECK(du_god_edge(&p, false) == 0 && !p);
    // a whole duel ending on time: asked, the countdown, the fight, time up + the judge, over, idle. The refill waits in the
    // countdown, the fight and the judge only; one "paused" line, one "back" line
    DuSm m = {0};
    m.limit_ms = 60000;
    GodLog g = {0};
    CHECK(!god_frame_(&g, true, &m, false));                          // (idle)
    du_sm_asked(&m, 0x99, T0);
    CHECK(!god_frame_(&g, true, &m, false));                          // (the prompt: no duel yet)
    uint64_t a = T0 + 4000;
    CHECK(du_sm_count(&m, a) && god_frame_(&g, true, &m, false) && g.np == 1);  // (3-2-1)
    CHECK(du_sm_step(&m, a + DU_COUNT_MS) == DUS_FIGHT && god_frame_(&g, true, &m, false));
    CHECK(du_sm_step(&m, a + DU_COUNT_MS + 60000) == DUS_TIMEUP && god_frame_(&g, true, &m, false));  // (the judge reads the real HP %)
    uint64_t e = a + DU_COUNT_MS + 60100;
    du_sm_end(&m, DUR_WIN, DUE_TIME, e);
    CHECK(m.phase == DU_OVER && !god_frame_(&g, true, &m, false) && g.nb == 1);  // (back at once: the duel healed both)
    CHECK(du_sm_step(&m, e + DU_OVER_MS) == DUS_IDLE && !god_frame_(&g, true, &m, false));
    CHECK(g.np == 1 && g.nb == 1);
    // a KO: the wait goes on through the KO hold (the loser's 1 HP moment), back when the duel's own heal ends it
    g = (GodLog){0};
    du_sm_ask(&m, 7, T0);
    CHECK(!god_frame_(&g, true, &m, false));  // (asking)
    du_sm_count(&m, T0 + 100), du_sm_step(&m, T0 + 100 + DU_COUNT_MS);
    CHECK(m.phase == DU_FIGHT && god_frame_(&g, true, &m, false));
    du_sm_end(&m, DUR_LOSE, DUE_KO, T0 + 9000);
    CHECK(m.phase == DU_OVER && god_frame_(&g, true, &m, true) && g.nb == 0);  // (the KO hold)
    CHECK(!god_frame_(&g, true, &m, false) && g.np == 1 && g.nb == 1);         // (the hold's heal)
    // a load / Sora mode off / duel 0 mid-fight (du_off_all: reset, DUS cleared): back at once
    du_sm_reset(&m), g = (GodLog){0};
    du_sm_ask(&m, 8, T0), du_sm_count(&m, T0), du_sm_step(&m, T0 + DU_COUNT_MS);
    CHECK(god_frame_(&g, true, &m, false));
    du_sm_reset(&m);
    CHECK(!god_frame_(&g, true, &m, false) && g.np == 1 && g.nb == 1);
    // duelgod 0 (the default): a whole duel, KO hold too, never waits and never logs (today's refill every frame)
    du_sm_reset(&m), g = (GodLog){0};
    du_sm_asked(&m, 9, T0), du_sm_count(&m, T0 + 10), du_sm_step(&m, T0 + 10 + DU_COUNT_MS);
    CHECK(m.phase == DU_FIGHT && !god_frame_(&g, false, &m, false));
    du_sm_end(&m, DUR_WIN, DUE_KO, T0 + 20000);
    CHECK(!god_frame_(&g, false, &m, true) && !god_frame_(&g, false, &m, false) && g.np == 0 && g.nb == 0);
    // the key flipped mid-fight: back at once, and paused again when it is set again
    du_sm_reset(&m), g = (GodLog){0};
    du_sm_ask(&m, 10, T0), du_sm_count(&m, T0), du_sm_step(&m, T0 + DU_COUNT_MS);
    CHECK(god_frame_(&g, true, &m, false) && !god_frame_(&g, false, &m, false) && god_frame_(&g, true, &m, false));
    CHECK(g.np == 2 && g.nb == 1);
    // a duel called off before the countdown (cancel from asking): never live, never a line
    du_sm_reset(&m), g = (GodLog){0};
    du_sm_ask(&m, 11, T0), du_sm_end(&m, DUR_CANCEL, DUE_CANCEL, T0 + 5);
    CHECK(m.phase == DU_OVER && !god_frame_(&g, true, &m, false) && g.np == 0 && g.nb == 0);
}

// ---- (s289 duelmagnet) runtime/sora_duel_magnet.h: KH2's grab (sum of radii), the step (clamp, 10 % near the hold shell, the
// swirl's flat tangent, lift on / off), the HIT + PULL bodies, the PULL on the wire (round trip, size, op 8, an older game's
// range), the victim's rules (another duel, a copy, before its HIT, a blocked HIT, not eaten by the HIT's DuSeen, held, bad
// values, the 3 s immunity), every end, the no-drive rule, the break-free presses.
static bool old_duel_ok(const uint8_t *b, int len) {  // the s288 net_unpack_ex NET_DUEL check (ops 1..6), as an older game runs it
    static const uint8_t DL[7] = {0, 13, 29, 51, 17, 15, 31};
    return len >= 13 && b[NET_HDR] >= 1 && b[NET_HDR] <= 6 && len == DL[b[NET_HDR]];
}
static int mag_pull(DuMagPull *P, uint16_t hseq, int frames, bool lift, uint64_t now) {  // a taken HIT, then its PULL (duel 77)
    float cc[3] = {0, 1.80f, 0};
    NetDuelMsg q;
    du_mag_note_hit(P, hseq, true);
    du_mag_pull_msg(&q, 77, hseq, cc, 250, 80, -3, (float)frames, 0);
    return du_mag_accept(P, &q, 77, true, true, lift, now);
}
static void magnet_tests(void) {
    const float U = 0.01f;  // (to_u at g_k 1: 1 cm = 0.01 u)
    float f0[3] = {0, 0, 0}, v[3];
    // the grab: the sphere R against his body (KH2's Sora target part: 70 cm above the feet, r 55): |c - body| <= R + 55
    CHECK(DU_BODY_Y0_CM + DU_BODY_H_CM / 2 == 70 && DU_BODY_R_CM == 55);
    float cb[3] = {0, 0.70f, 0}, e[3] = {3.04f, 0, 0};
    CHECK(du_mag_grab(cb, e, 250, U));                       // (3.04 m <= 2.5 + 0.55)
    e[0] = 3.06f;
    CHECK(!du_mag_grab(cb, e, 250, U));
    e[0] = 0, e[1] = -3.04f;
    CHECK(du_mag_grab(cb, e, 250, U));                       // (straight below: the body, 70 cm over his feet, 3.04 m under the centre)
    e[1] = -3.07f;
    CHECK(!du_mag_grab(cb, e, 250, U) && du_mag_grab(cb, e, 500, U));  // (3.07 m: out of tier 1's, in tier 2's 5 m)
    CHECK(!du_mag_grab(cb, f0, 0, U) && !du_mag_grab(cb, f0, 250, 0) && !du_mag_grab(cb, f0, NAN, U));
    // the step, cm per KH frame toward the centre: clamp((d - hold) x 0.1, -20, 20)
    float c1[3] = {10.0f, 0, 0};
    CHECK(NEAR(du_mag_step(f0, c1, 100, 0, true, U, v), 1000, 0.01f) && NEAR(v[0], 20, 1e-4f) && v[1] == 0 && v[2] == 0);  // (far: +20)
    float c2[3] = {0.5f, 0, 0};
    du_mag_step(f0, c2, 300, 0, true, U, v);
    CHECK(NEAR(v[0], -20, 1e-4f) && v[2] == 0);              // (inside the hold shell: out at -20)
    float c3[3] = {1.5f, 0, 0};
    du_mag_step(f0, c3, 100, 0, true, U, v);
    CHECK(NEAR(v[0], 5, 1e-4f));                             // (50 cm off the shell: 10 % = 5)
    du_mag_step(f0, c3, 150, 0, true, U, v);
    CHECK(NEAR(v[0], 0, 1e-4f) && v[1] == 0 && v[2] == 0);   // (on the shell: still)
    // the swirl: the flat tangent RotY(pi/2) of the line (mg_magnet_tick's handedness), y 0, x swirl
    float c4[3] = {1.0f, 0, 0};
    du_mag_step(f0, c4, 100, -3, true, U, v);
    CHECK(NEAR(v[0], 0, 1e-4f) && v[1] == 0 && NEAR(v[2], 3, 1e-4f));
    float c5[3] = {1.0f, 1.0f, 0};
    du_mag_step(f0, c5, 0, -12, true, U, v);
    CHECK(NEAR(v[1], 0.70711f * 14.142f, 0.01f) && NEAR(v[2], 12, 1e-3f));  // (the tangent stays flat: y only from the pull)
    float c6[3] = {0, 2.0f, 0};
    du_mag_step(f0, c6, 50, -12, true, U, v);
    CHECK(v[0] == 0 && NEAR(v[1], 15, 1e-4f) && v[2] == 0);  // (right under the centre: no tangent, up 15)
    // lift on = 3D toward the raised centre; off = the same in the ground plane (v[1] 0)
    float c7[3] = {3.0f, 4.0f, 0};
    CHECK(NEAR(du_mag_step(f0, c7, 100, 0, true, U, v), 500, 0.01f) && NEAR(v[0], 12, 1e-3f) && NEAR(v[1], 16, 1e-3f) && v[2] == 0);
    CHECK(NEAR(du_mag_step(f0, c7, 100, 0, false, U, v), 300, 0.01f) && NEAR(v[0], 20, 1e-4f) && v[1] == 0 && v[2] == 0);
    du_mag_step(f0, c6, 50, -12, false, U, v);
    CHECK(v[0] == 0 && v[1] == 0 && v[2] == 0);              // (flat under the centre: nothing to pull)
    CHECK(du_mag_step(f0, c7, 100, 0, true, 0, v) == 0 && v[0] == 0 && v[1] == 0 && v[2] == 0);
    // the HIT: src 6 "Magnet", Power 100, a spell, atkp 658, reaction 5 (no finisher, no boost)
    NetDuelMsg h, q, r;
    du_mag_hit_msg(&h, 31);
    CHECK(h.power == 100 && h.src == DU_SRC_MAGNET && h.src == 6 && h.flags == NDH_SPELL && h.atkp == 658 && h.react == 5 && h.stat == 31 && !h.boost);
    CHECK(!strcmp(du_src_name(h.src), "Magnet") && !strcmp(du_src_kind(h.src), "Magnet"));
    du_mag_hit_msg(&h, -5);
    CHECK(h.stat == 0);
    du_mag_hit_msg(&h, 70000);
    CHECK(h.stat == 65535);
    uint8_t b[NET_PKT_MAX], c[NET_PKT_MAX];
    uint8_t pl = 0;
    uint16_t sq = 0;
    int t = 0;
    du_mag_hit_msg(&h, 40), h.op = NDO_HIT, h.id = 77, h.hseq = 9;
    int n = net_pack_duel(0x42, 5, &h, b, sizeof b);
    CHECK(n == NET_DUEL_HIT_LEN && net_unpack_duel(b, n, &pl, &sq, &r) && r.src == 6 && r.power == 100 && r.atkp == 658 && r.flags == NDH_SPELL);
    CHECK(old_duel_ok(b, n));                                // (an older game takes the Magnet HIT: any src is damage there)
    // the PULL: 37 bytes, every field back, a NaN centre refused
    float cc[3] = {-1234.5f, 210.25f, 2048.75f};
    du_mag_pull_msg(&q, 0xABCD1234u, 4242, cc, 800, 187.4f, -12, 212.3f, 2);
    CHECK(q.op == NDO_PULL && q.id == 0xABCD1234u && q.hseq == 4242 && q.radius_cm == 800 && q.hold_cm == 187 && q.swirl == -12 && q.frames == 213 && q.tier == 2);
    n = net_pack_duel(0x42, 6, &q, b, sizeof b);
    CHECK(n == 37 && n == NET_DUEL_PULL_LEN && n == NET_DUEL_LEN[NDO_PULL] && b[3] == NET_DUEL && b[NET_HDR] == NDO_PULL);
    CHECK(net_unpack_ex(b, n, &t, &pl, &sq, NULL, NULL, NULL, NULL) && t == NET_DUEL && pl == 0x42 && sq == 6);
    memset(&r, 0xcc, sizeof r);
    CHECK(net_unpack_duel(b, n, &pl, &sq, &r) && r.op == NDO_PULL && r.id == 0xABCD1234u && r.hseq == 4242);
    CHECK(r.c[0] == cc[0] && r.c[1] == cc[1] && r.c[2] == cc[2] && r.radius_cm == 800 && r.hold_cm == 187 && r.swirl == -12 && r.frames == 213 && r.tier == 2);
    CHECK(!net_unpack_duel(b, n - 1, &pl, &sq, &r) && !net_unpack_duel(b, n + 1, &pl, &sq, &r));  // (exact size)
    CHECK(!old_duel_ok(b, n) && NDO_PULL > NDO_RING);       // (an older game drops it as "bad": its range ends at RING)
    q.c[2] = NAN, n = net_pack_duel(0x42, 6, &q, b, sizeof b);
    CHECK(n == 37 && !net_unpack_duel(b, n, &pl, &sq, &r));
    q.c[2] = 1, q.swirl = 100, n = net_pack_duel(0x42, 6, &q, b, sizeof b);
    CHECK(net_unpack_duel(b, n, &pl, &sq, &r) && r.swirl == 100);
    q.swirl = -100, n = net_pack_duel(0x42, 6, &q, b, sizeof b);
    CHECK(net_unpack_duel(b, n, &pl, &sq, &r) && r.swirl == -100);  // (the i16 sign)
    // op 8 refused: by the packer and on the wire (a PULL-sized op 8)
    q.op = 8;
    CHECK(net_pack_duel(0x42, 6, &q, b, sizeof b) < 0);
    q.op = NDO_PULL, n = net_pack_duel(0x42, 6, &q, b, sizeof b);
    memcpy(c, b, (size_t)n), c[NET_HDR] = 8;
    CHECK(!net_unpack_ex(c, n, &t, &pl, &sq, NULL, NULL, NULL, NULL) && !net_unpack_duel(c, n, &pl, &sq, &r));
    memcpy(c, b, (size_t)n), c[NET_HDR] = NDO_HIT;           // (a PULL-sized HIT)
    CHECK(!net_unpack_ex(c, n, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    CHECK(net_pack_duel(0x42, 6, &q, b, NET_DUEL_PULL_LEN - 1) < 0);
    // the PULL body's clamps
    du_mag_pull_msg(&q, 1, 2, cc, 0, NAN, 200, 0, -1);
    CHECK(q.radius_cm == 1 && q.hold_cm == 0 && q.swirl == 100 && q.frames == 1 && q.tier == 0);
    du_mag_pull_msg(&q, 1, 2, cc, 5000, 5000, -200, 1000, 5);
    CHECK(q.radius_cm == 3000 && q.hold_cm == 3000 && q.swirl == -100 && q.frames == DU_MAG_MAX_FRAMES && q.tier == 2);
    du_mag_pull_msg(&q, 1, 2, cc, 250, 40, -3, NAN, 0);
    CHECK(q.frames == 1);
    // ---- the victim's rules ----
    DuMagPull P;
    DuSeen hs = {0};
    float c8[3] = {0, 1.80f, 0};
    du_mag_reset(&P);
    CHECK(!P.on && !P.immune_until && !du_mag_hit_st(&P, 10));
    du_mag_pull_msg(&q, 77, 10, c8, 250, 80, -3, 150, 0);
    CHECK(du_mag_accept(&P, &q, 77, true, false, true, T0) == DUMP_OFF);    // (duelmagnet 0 here: as an older game)
    CHECK(du_mag_accept(&P, &q, 78, true, true, true, T0) == DUMP_OTHER);   // (another duel id)
    CHECK(du_mag_accept(&P, &q, 0, true, true, true, T0) == DUMP_OTHER);    // (not ours: no duel / another slot)
    CHECK(du_mag_accept(&P, NULL, 77, true, true, true, T0) == DUMP_OTHER);
    CHECK(du_mag_accept(&P, &q, 77, false, true, true, T0) == DUMP_FIGHT);  // (no fight)
    CHECK(du_mag_accept(&P, &q, 77, true, true, true, T0) == DUMP_NO_HIT && !du_mag_seen(&P.seen, 10));  // (before its HIT: not marked)
    // the HIT comes: the HIT's own DuSeen marks hseq 10 (sora_duel.h du_on_msg), du_take_hit notes it taken
    CHECK(du_seen_add(&hs, 10));
    du_mag_note_hit(&P, 10, true);
    CHECK(du_mag_hit_st(&P, 10) == 1);
    // the PULL's copy now counts: NOT eaten by the HIT's list (which has 10), its own list marks it
    CHECK(!du_seen_add(&hs, 10));                                            // (the HIT's copy: still eaten)
    CHECK(du_mag_accept(&P, &q, 77, true, true, true, T0) == DUMP_OK);
    CHECK(P.on && P.lift && P.hseq == 10 && P.radius == 250 && P.hold == 80 && P.swirl == -3 && P.left == 150 && P.tier == 0 && P.t0 == T0 && P.last_drive == T0);
    CHECK(P.c[1] == 1.80f && du_mag_seen(&P.seen, 10) && P.why == DUM_END_NONE);
    CHECK(du_mag_accept(&P, &q, 77, true, true, true, T0) == DUMP_REPEAT);  // (its UDP copy)
    // a second Magnet while held: its HIT counts (damage), its PULL is refused
    du_mag_note_hit(&P, 11, true);
    du_mag_pull_msg(&q, 77, 11, c8, 500, 100, -6, 170, 1);
    CHECK(du_mag_accept(&P, &q, 77, true, true, true, T0 + 100) == DUMP_HELD && P.on && P.hseq == 10 && P.radius == 250);
    CHECK(du_mag_accept(&P, &q, 77, true, true, true, T0 + 200) == DUMP_REPEAT);  // (judged once)
    // a blocked HIT (Guard / Reflect / i-frames): its PULL is ignored, once
    du_mag_reset(&P);
    du_mag_note_hit(&P, 12, false);
    du_mag_pull_msg(&q, 77, 12, c8, 250, 80, -3, 150, 0);
    CHECK(du_mag_hit_st(&P, 12) == 2 && du_mag_accept(&P, &q, 77, true, true, true, T0) == DUMP_BLOCKED && !P.on);
    CHECK(du_mag_accept(&P, &q, 77, true, true, true, T0) == DUMP_REPEAT && !P.on);
    // bad values (each with its own taken HIT): judged bad, marked
    struct { int R, hold, frames, tier, swirl; float y; } bad[] = {{0, 0, 10, 0, 0, 1}, {250, 251, 10, 0, 0, 1}, {250, 80, 0, 0, 0, 1}, {250, 80, 601, 0, 0, 1},
                                                                   {250, 80, 10, 3, 0, 1}, {250, 80, 10, 0, 101, 1}, {250, 80, 10, 0, -101, 1}, {3001, 80, 10, 0, 0, 1},
                                                                   {250, 80, 10, 0, 0, NAN}};
    for (int i = 0; i < (int)(sizeof bad / sizeof *bad); i++) {
        du_mag_reset(&P);
        memset(&q, 0, sizeof q), q.op = NDO_PULL, q.id = 77, q.hseq = (uint16_t)(30 + i), q.c[1] = bad[i].y;
        q.radius_cm = (uint16_t)bad[i].R, q.hold_cm = (uint16_t)bad[i].hold, q.frames = (uint16_t)bad[i].frames, q.tier = (uint8_t)bad[i].tier, q.swirl = (int16_t)bad[i].swirl;
        du_mag_note_hit(&P, q.hseq, true);
        CHECK(du_mag_accept(&P, &q, 77, true, true, true, T0) == DUMP_BAD && !P.on && du_mag_seen(&P.seen, q.hseq));
    }
    // the note ring: 8 Magnet HITs remembered, a copy overwrites in place
    du_mag_reset(&P);
    for (int i = 0; i < DU_MAG_NHIT; i++) du_mag_note_hit(&P, (uint16_t)(100 + i), true);
    du_mag_note_hit(&P, 100, false);
    CHECK(du_mag_hit_st(&P, 100) == 2 && du_mag_hit_st(&P, 107) == 1);
    du_mag_note_hit(&P, 108, true);                                          // (the 9th: the oldest slot goes)
    CHECK(du_mag_hit_st(&P, 108) == 1 && !du_mag_hit_st(&P, 100) && du_mag_hit_st(&P, 101) == 1);
    // ---- the ends; after ANY end no new pull for 3 s ----
    CHECK(DU_MAG_IMMUNE_MS == 3000);
    for (int why = DUM_END_WINDOW; why < DUM_END_N; why++) {
        du_mag_reset(&P);
        CHECK(mag_pull(&P, 1, 150, true, T0) == DUMP_OK);
        CHECK(du_mag_end(&P, why, why == DUM_END_BREAK ? DUM_BRK_ROLL : DUM_BRK_NONE, T0 + 500) && !P.on && P.why == why && P.t_end == T0 + 500);
        CHECK(P.immune_until == T0 + 3500 && !du_mag_end(&P, why, 0, T0 + 600) && P.t_end == T0 + 500);  // (no pull: nothing)
        CHECK(DUM_END_NAME[why] && DUM_END_NAME[why][0]);
        CHECK(mag_pull(&P, 2, 150, true, T0 + 3499) == DUMP_IMMUNE && !P.on);
        CHECK(mag_pull(&P, 3, 150, true, T0 + 3500) == DUMP_OK && P.on && P.hseq == 3);
    }
    CHECK(!strcmp(DUM_BRK_NAME[DUM_BRK_GUARD], "Guard") && !strcmp(DUM_BRK_NAME[DUM_BRK_ROLL], "Dodge Roll") &&
          !strcmp(DUM_BRK_NAME[DUM_BRK_QRUN], "Quick Run") && !strcmp(DUM_BRK_NAME[DUM_BRK_ADODGE], "Aerial Dodge"));
    // the window: KH frames on driven ticks only (kh 1: 9 ticks move him, the 10th ends it; kh 2: 4 + the end)
    du_mag_reset(&P);
    CHECK(mag_pull(&P, 5, 10, true, T0) == DUMP_OK);
    bool moving = true;
    for (int i = 0; i < 9; i++) moving = moving && du_mag_tick(&P, 1, f0, U, T0 + 16 * (uint64_t)i, v) == DUM_END_NONE && v[1] > 0;
    CHECK(moving && P.on && P.nframes == 9 && P.last_drive == T0 + 128 && NEAR(P.dist, 180, 0.01f));
    CHECK(du_mag_tick(&P, 1, f0, U, T0 + 144, v) == DUM_END_WINDOW && !P.on && P.why == DUM_END_WINDOW && P.immune_until == T0 + 144 + 3000);
    CHECK(v[0] == 0 && v[1] == 0 && v[2] == 0 && P.nframes == 9);
    CHECK(du_mag_tick(&P, 1, f0, U, T0 + 160, v) == DUM_END_NONE && v[1] == 0);  // (none on: nothing)
    du_mag_reset(&P);
    CHECK(mag_pull(&P, 6, 10, true, T0) == DUMP_OK);
    int ticks = 0;
    while (du_mag_tick(&P, 2, f0, U, T0 + (uint64_t)ticks, v) == DUM_END_NONE && ticks < 20) ticks++;
    CHECK(ticks == 4 && P.why == DUM_END_WINDOW);
    du_mag_reset(&P);
    CHECK(mag_pull(&P, 7, 10, true, T0) == DUMP_OK);
    CHECK(du_mag_tick(&P, 0, f0, U, T0, v) == DUM_END_NONE && P.left == 10);  // (kh 0: the window waits)
    // lift off: the tick's step is flat (he stays down)
    du_mag_reset(&P);
    CHECK(mag_pull(&P, 8, 150, false, T0) == DUMP_OK && !P.lift);
    float fx[3] = {1.5f, 0, 0};
    CHECK(du_mag_tick(&P, 1, fx, U, T0, v) == DUM_END_NONE && v[1] == 0 && NEAR(v[0], -7, 1e-3f) && NEAR(P.dist, 234.31f, 0.05f));  // (150 cm flat, hold 80)
    // the warp failsafe: his feet more than 2 m beyond the sphere radius (250 + 200 cm) from the centre
    float fw[3] = {4.0f, 0, 0}, fz[3] = {4.2f, 0, 0}, fn[3] = {NAN, 0, 0};
    CHECK(du_mag_tick(&P, 1, fw, U, T0 + 1, v) == DUM_END_NONE);            // (4.39 m)
    CHECK(du_mag_tick(&P, 1, fz, U, T0 + 2, v) == DUM_END_WARP && !P.on && P.why == DUM_END_WARP && P.immune_until == T0 + 3002);
    du_mag_reset(&P);
    CHECK(mag_pull(&P, 9, 150, true, T0) == DUMP_OK);
    CHECK(du_mag_tick(&P, 1, fn, U, T0, v) == DUM_END_WARP);                // (a NaN body: ended, never a NaN velocity)
    // no driving frame for 1 s (Link has the pad) ends it; a paused game never does
    du_mag_reset(&P);
    CHECK(mag_pull(&P, 10, 150, true, T0) == DUMP_OK);
    CHECK(!du_mag_stale(&P, false, T0 + 999) && du_mag_stale(&P, false, T0 + 1000) && !du_mag_stale(&P, true, T0 + 5000));
    du_mag_tick(&P, 1, f0, U, T0 + 900, v);
    CHECK(!du_mag_stale(&P, false, T0 + 1899) && du_mag_stale(&P, false, T0 + 1900));
    du_mag_end(&P, DUM_END_OFF, DUM_BRK_NONE, T0 + 1900);
    CHECK(!du_mag_stale(&P, false, T0 + 9000));
    // the break-free presses: Square = Guard (stick neutral) / Dodge Roll / Quick Run (no Dodge Roll learned), anywhere;
    // Cross in the air = the Aerial Dodge; Cross on the ground = a jump (no break)
    CHECK(du_mag_break(true, false, false, false, true, true) == DUM_BRK_GUARD);
    CHECK(du_mag_break(true, false, true, false, true, true) == DUM_BRK_GUARD);
    CHECK(du_mag_break(true, false, false, true, true, true) == DUM_BRK_ROLL);
    CHECK(du_mag_break(true, false, false, true, true, false) == DUM_BRK_ROLL);
    CHECK(du_mag_break(true, false, false, true, false, true) == DUM_BRK_QRUN);
    CHECK(du_mag_break(true, false, false, true, false, false) == DUM_BRK_ROLL);  // (neither learned: the press still breaks free)
    CHECK(du_mag_break(false, true, true, false, false, false) == DUM_BRK_ADODGE);
    CHECK(du_mag_break(false, true, false, true, true, true) == DUM_BRK_NONE);
    CHECK(du_mag_break(false, false, true, true, true, true) == DUM_BRK_NONE);
    CHECK(du_mag_break(true, true, true, false, true, true) == DUM_BRK_GUARD);   // (both: Square's)
}

int main(void) {
    god_tests();
    block_tests();
    throw_tests();
    gate_tests();
    damage_tests();
    ring_tests();
    sm_tests();
    seen_tests();
    wire_tests();
    reflect_tests();
    act_tests();
    magnet_tests();
    printf("duel_test: %d / %d checks pass\n", runs - fails, runs);
    return fails ? 1 : 0;
}

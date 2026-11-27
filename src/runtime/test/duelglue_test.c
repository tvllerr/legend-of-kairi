// (s291 dueltests) host test of runtime/sora_duel.h's GLUE on stubs (no game, no socket): the s273 review Lows that s284 fixed
// (b56c0df), each case driven ON ITS OWN through the real du_on_msg / net_duel_recv / du_take_hit / du_input / du_spell / du_frame.
// GLITCHES "[s285 PASSED ... 8 / 12 not run separately]": s285 ran the far ASK (two copies), Guard + the invulnerable frames while
// paused (one game) and the time judge 20 s vs 180 s (two copies); the 8 not run alone are marked [8] below. All 12 are here.
//   L6a       an ASK from a friend drawn too far away (flat > 3 x duelnear)      -> ANSWER busy DUG_FAR ("too far / another map")
//   L6b  [8]  an ASK from a friend 20+ m above / below                          -> ANSWER busy DUG_FAR
//   L6c  [8]  an ASK from a friend with no pose here (hidden = another map)     -> ANSWER busy DUG_FAR
//   L6d  [8]  an ASK flood: each one answered busy, the LOG line 8 per 2 s (net_lograte_ok), no duel, no banner
//   L8   [8]  a faded-out friend (PEER_FADING): his ASK = DUG_FAR, no "Duel" prompt toward him, a spell does not hit him
//   L9a       paused: a hit inside the Guard window is taken
//   L9b       paused: a hit in the invulnerable frames (G.inv, the post-block frames) is taken
//   L9c  [8]  paused: a hit while Reflect is up is taken (+ the Dodge Roll's g_rc_inv and the Aerial Dodge block, s288)
//   L9d  [8]  paused: a hit during a party Limit's invulnerability is taken
//   L10       the time judge sends END with its result: a game with a longer dueltime takes the same result (reason time)
//   L11  [8]  his TIME lost: his END (judged before his heal) decides here, not his healed SNAP
//   L12  [8]  "set duel 0" with only a third pair's ring seen here: the ring state + its fx are cleaned up
// Two games: runtime/test/duelglue_game.h compiled twice (duelglue_game_a.c / _b.c), each with its own static duel state;
// the test hands one game's sent bytes to the other's net_duel_recv. Tests only: no behaviour of sora_duel.h is changed.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/duelglue_test.c runtime/test/duelglue_game_a.c runtime/test/duelglue_game_b.c
//         -o build_h/duelglue_test.exe        (UBSan: + -fsanitize=undefined,bounds -fsanitize-trap=all)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "sora_net_peer.h"
#include "sora_duel_core.h"
#include "duelglue_api.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)
#define T0 1000000ull

static const DgGame *const A = &DGA, *const B = &DGB;

static int log_count(const DgGame *g, const char *s) {
    int n = 0;
    for (const char *p = g->log(); (p = strstr(p, s)) != NULL; p += strlen(s)) n++;
    return n;
}
#define LOGGED(g, s) (strstr((g)->log(), (s)) != NULL)
// the packets a game sent, unpacked
static int sent_n(const DgGame *g, int op) {
    int c = 0;
    for (int i = 0; i < g->ntx(); i++) {
        NetDuelMsg m;
        uint8_t pl;
        uint16_t sq;
        if (net_unpack_duel(g->tx(i)->b, g->tx(i)->n, &pl, &sq, &m) && m.op == op) c++;
    }
    return c;
}
static bool sent_last(const DgGame *g, int op, NetDuelMsg *out) {
    for (int i = g->ntx() - 1; i >= 0; i--) {
        uint8_t pl;
        uint16_t sq;
        if (net_unpack_duel(g->tx(i)->b, g->tx(i)->n, &pl, &sq, out) && out->op == op) return true;
    }
    return false;
}
// one packet "from slot k" on the wire into g (net_duel_recv; du_frame pops it)
static void wire(const DgGame *g, int k, uint8_t from_player, const NetDuelMsg *m, uint64_t now) {
    uint8_t b[64];
    static uint16_t seq;
    int n = net_pack_duel(from_player, ++seq, m, b, (int)sizeof b);
    CHECK(n > 0);
    g->recv(k, b, n, now);
}
// everything `from` sent goes to `to` (as slot k there); drop_op != 0: that op is lost on the way
static void deliver(const DgGame *from, const DgGame *to, int k, uint64_t now, int drop_op) {
    for (int i = 0; i < from->ntx(); i++) {
        const DgPkt *p = from->tx(i);
        NetDuelMsg m;
        uint8_t pl;
        uint16_t sq;
        if (drop_op && net_unpack_duel(p->b, p->n, &pl, &sq, &m) && m.op == drop_op) continue;
        to->recv(k, p->b, p->n, now);
    }
    from->tx_clear();
}
static int pct(int life) { return life * 100 / 48; }

// ---- L6: an ASK from a friend not drawn near us = "too far / another map" ----
static NetDuelMsg ask_msg(uint32_t id) {
    NetDuelMsg m;
    memset(&m, 0, sizeof m);
    m.op = NDO_ASK, m.id = id, m.target = 1;
    return m;
}
// one ASK from slot 0 (player 2) standing at feet; -> the game's phase afterwards; *why = the busy answer's why (-1 = none sent)
static int ask_from(const float feet[3], int st, bool hidden, bool pose, uint32_t id, int *why) {
    A->reset(1);
    A->peer(0, 2, "copy2", feet, st, hidden, 0, 100, T0);
    if (!pose) A->no_pose(0);
    NetDuelMsg m = ask_msg(id), r;
    wire(A, 0, 2, &m, T0);
    A->frame(T0);  // (the queue is popped here: du_on_msg's NDO_ASK)
    *why = -1;
    if (sent_last(A, NDO_ANSWER, &r)) {
        CHECK(r.id == id && r.ans == DUA_BUSY && r.target == 2);
        *why = r.why;
    }
    return A->view().phase;
}
static void l6_tests(void) {
    int why;
    // L6a: flat distance. 3 x duelnear (6 m) = 18 m: over = too far, the edge itself is in
    float f[3] = {18.5f, 0, 0};
    CHECK(ask_from(f, PEER_LIVE, false, true, 0x1001, &why) == DU_IDLE && why == DUG_FAR);
    CHECK(LOGGED(A, "\"copy2\" asked for a duel: answered busy (too far / another map)"));
    CHECK(A->view().prompt == 0 && A->view().banner[0] == 0);  // (no "wants to duel" banner, no "Accept Duel")
    f[0] = 18.0f;
    CHECK(ask_from(f, PEER_LIVE, false, true, 0x1002, &why) == DU_ASKED && why == -1);
    CHECK(A->view().prompt == 2 && !strcmp(A->view().banner, "copy2 wants to duel"));
    f[0] = 0, f[2] = -17.9f;  // (any direction)
    CHECK(ask_from(f, PEER_LIVE, false, true, 0x1003, &why) == DU_ASKED);
    f[0] = 13, f[2] = 13;     // (18.4 m on the diagonal)
    CHECK(ask_from(f, PEER_LIVE, false, true, 0x1004, &why) == DU_IDLE && why == DUG_FAR);
    // the live duelnear moves the limit with it (s285's two-copy setup: duelnear 2 = 6 m)
    {
        A->reset(1);
        CHECK(A->live_key("duelnear", 2));
        float g6[3] = {6.5f, 0, 0};
        A->peer(0, 2, "copy2", g6, PEER_LIVE, false, 0, 100, T0);
        NetDuelMsg m = ask_msg(0x1005), r;
        A->on_msg(0, &m, T0);
        CHECK(A->view().phase == DU_IDLE && sent_last(A, NDO_ANSWER, &r) && r.ans == DUA_BUSY && r.why == DUG_FAR);
    }
    // L6b [8]: height. More than 20 m above or below = too far, at 19.5 m it is a normal ASK
    float h[3] = {2, 20.5f, 0};
    CHECK(ask_from(h, PEER_LIVE, false, true, 0x1101, &why) == DU_IDLE && why == DUG_FAR);
    h[1] = -20.5f;
    CHECK(ask_from(h, PEER_LIVE, false, true, 0x1102, &why) == DU_IDLE && why == DUG_FAR);
    h[1] = 19.5f;
    CHECK(ask_from(h, PEER_LIVE, false, true, 0x1103, &why) == DU_ASKED && why == -1);
    h[1] = -19.5f;
    CHECK(ask_from(h, PEER_LIVE, false, true, 0x1104, &why) == DU_ASKED);
    // L6c [8]: no pose of him here. Hidden (we are both in known, different areas: another map) / no SNAP kept at all
    float n2[3] = {2, 0, 0};
    CHECK(ask_from(n2, PEER_LIVE, true, true, 0x1201, &why) == DU_IDLE && why == DUG_FAR);
    CHECK(LOGGED(A, "answered busy (too far / another map)"));
    CHECK(ask_from(n2, PEER_LIVE, false, false, 0x1202, &why) == DU_IDLE && why == DUG_FAR);
    CHECK(ask_from(n2, PEER_LIVE, false, true, 0x1203, &why) == DU_ASKED);  // (the same friend drawn here: a normal ASK)
    // the other gates still come first: duel 0 here = nothing answered at all (as before the fix)
    {
        A->reset(1);
        A->live_key("duel", 0);
        A->peer(0, 2, "copy2", n2, PEER_LIVE, false, 0, 100, T0);
        NetDuelMsg m = ask_msg(0x1204);
        wire(A, 0, 2, &m, T0);
        A->frame(T0);
        CHECK(A->ntx() == 0 && A->view().phase == DU_IDLE);
    }
    // L6d [8]: a flood of ASKs with new ids from a far friend: every one answered busy, 8 log lines per 2 s, no duel
    {
        A->reset(1);
        float far50[3] = {50, 0, 0};
        A->peer(0, 2, "flood", far50, PEER_LIVE, false, 0, 100, T0);
        for (int i = 0; i < 30; i++) {
            NetDuelMsg m = ask_msg(0x2000u + (uint32_t)i);
            A->on_msg(0, &m, T0 + (uint64_t)i);  // (30 in 30 ms)
        }
        CHECK(sent_n(A, NDO_ANSWER) == 30);
        CHECK(log_count(A, "asked for a duel: answered busy") == 8);  // (NET_LOGRATE_BURST in NET_LOGRATE_MS)
        CHECK(A->view().phase == DU_IDLE && A->view().prompt == 0 && A->view().banner[0] == 0);
        NetDuelMsg m = ask_msg(0x2100);
        A->on_msg(0, &m, T0 + 1999);  // (the same 2 s window: still quiet)
        CHECK(log_count(A, "asked for a duel: answered busy") == 8 && sent_n(A, NDO_ANSWER) == 31);
        m.id = 0x2101;
        A->on_msg(0, &m, T0 + 2000);  // (a new window: logged again)
        CHECK(log_count(A, "asked for a duel: answered busy") == 9 && sent_n(A, NDO_ANSWER) == 32);
        // the flood forced nothing: the same friend walks up and asks, a normal "Accept Duel"
        A->peer(0, 2, "flood", n2, PEER_LIVE, false, 0, 100, T0 + 2100);
        m.id = 0x2102;
        A->on_msg(0, &m, T0 + 2100);
        CHECK(A->view().phase == DU_ASKED && A->view().id == 0x2102 && A->view().prompt == 2);
    }
}

// ---- L8 [8]: a faded-out friend is no Duel target ----
// A in a FIGHT with slot 0 (player 2): A asks, his ANSWER accept comes in, 3 s later "Go!". -> the duel id
static uint32_t to_fight(const DgGame *g, const float his_feet[3], uint64_t now) {
    g->peer(0, 2, "copy2", his_feet, PEER_LIVE, false, 0, 100, now);
    g->ask(0, now);
    uint32_t id = g->view().id;
    CHECK(g->view().phase == DU_ASKING && id != 0);
    NetDuelMsg a;
    memset(&a, 0, sizeof a);
    a.op = NDO_ANSWER, a.id = id, a.ans = DUA_ACCEPT, a.radius_cm = 2000, a.c[0] = 1.5f;
    g->on_msg(0, &a, now + 100);
    CHECK(g->view().phase == DU_COUNT && g->view().live == false);  // (DUS.live is set by the next du_frame)
    g->peer(0, 2, "copy2", his_feet, PEER_LIVE, false, NF_DUEL, 100, now + 3100);
    g->frame(now + 3100);
    CHECK(g->view().phase == DU_FIGHT && g->view().live);
    g->tx_clear(), g->log_clear();
    return id;
}
static void l8_tests(void) {
    int why;
    // his ASK from 2 m away while his slot fades (no SNAP for 2 s: PEER_FADING, drawn fading out): too far / another map
    float f2[3] = {2, 0, 0};
    CHECK(ask_from(f2, PEER_FADING, false, true, 0x3001, &why) == DU_IDLE && why == DUG_FAR);
    CHECK(ask_from(f2, PEER_LIVE, false, true, 0x3002, &why) == DU_ASKED);  // (the control: live, the same spot)
    // the "Duel" prompt (du_input, its own clock): a live friend 2 m ahead = "Duel" toward him; faded = none, Triangle stays BotW's
    for (int fade = 0; fade < 2; fade++) {
        A->reset(1);
        uint64_t now = GetTickCount64();
        float ahead[3] = {0, 0, 2};
        A->peer(0, 2, "copy2", ahead, fade ? PEER_FADING : PEER_LIVE, false, 0, 100, now);
        A->idle_long();
        uint64_t press = 0, held = 0;
        A->input(&press, &held, true);
        if (!fade) CHECK(A->view().prompt == 1 && A->view().prompt_k == 0);
        else CHECK(A->view().prompt == 0);
        // Triangle pressed now: live = du_ask (an ASK goes out, BotW does not get the press); faded = untouched, no ASK
        press = held = 2;  // (the stub's BTN_CLIMB = Triangle)
        A->input(&press, &held, true);
        if (!fade) CHECK(A->view().phase == DU_ASKING && sent_n(A, NDO_ASK) == 1 && press == 0);
        else CHECK(A->view().phase == DU_IDLE && A->ntx() == 0 && press == 2);
    }
    // in a fight: a spell on his drawn feet hits a live opponent, not one whose slot faded out
    {
        A->reset(1);
        float him[3] = {3, 0, 0};
        uint32_t id = to_fight(A, him, T0);
        CHECK(A->spell(0, 100, him, 1.0f, -1.0f, 2.0f));
        NetDuelMsg h;
        CHECK(sent_last(A, NDO_HIT, &h) && h.id == id && h.src == 1 && h.target == 2);
        A->tx_clear();
        A->peer(0, 2, "copy2", him, PEER_FADING, false, NF_DUEL, 100, T0 + 3200);
        CHECK(!A->spell(0, 100, him, 1.0f, -1.0f, 2.0f));
        CHECK(sent_n(A, NDO_HIT) == 0);
        A->peer(0, 2, "copy2", him, PEER_LIVE, false, NF_DUEL, 100, T0 + 3300);  // (back: hit again)
        CHECK(A->spell(0, 100, him, 1.0f, -1.0f, 2.0f) && sent_n(A, NDO_HIT) == 1);
    }
}

// ---- L9: a paused player takes the hit (every block reason counts on driven frames only) ----
static void l9_one(int what, const char *name) {
    A->reset(1);
    float him[3] = {0, 0, 3};
    uint32_t id = to_fight(A, him, T0);
    A->life(40);
    A->block(what, true);
    NetDuelMsg h;
    memset(&h, 0, sizeof h);
    h.op = NDO_HIT, h.id = id, h.power = 100, h.stat = 15, h.from[2] = 1.5f;  // (from in front of us: we face +z)
    // unpaused: blocked (life kept, one block)
    h.hseq = 1;
    A->on_msg(0, &h, T0 + 4000);
    DgView v = A->view();
    if (!(v.life == 40 && v.nblocked == 1 && v.nhits_taken == 0)) printf("  (%s unpaused: life %d, blocked %d, taken %d)\n", name, v.life, v.nblocked, v.nhits_taken);
    CHECK(v.life == 40 && v.nblocked == 1 && v.nhits_taken == 0);
    CHECK(what == DGB_ADODGE ? v.nad_block == 1 : v.nguard_block == 1);
    // paused, the same reason still "up" (its clock stands while BotW's menu is open): the hit is TAKEN
    A->pause(true);
    h.hseq = 2;
    A->on_msg(0, &h, T0 + 4100);
    v = A->view();
    if (!(v.life == 39 && v.nblocked == 1 && v.nhits_taken == 1)) printf("  (%s paused: life %d, blocked %d, taken %d)\n", name, v.life, v.nblocked, v.nhits_taken);
    CHECK(v.life == 39 && v.nblocked == 1 && v.nhits_taken == 1);  // (S 15 - Def 13 = V 2 x 100 % = 2, dueldmg 50 % -> 1)
    h.hseq = 3;
    A->on_msg(0, &h, T0 + 4200);
    CHECK(A->view().life == 38 && A->view().nhits_taken == 2 && A->view().nblocked == 1);  // (and every later one: s273 L9's "blocked for as long as the menu stays open")
    // unpaused again: blocked again
    A->pause(false);
    h.hseq = 4;
    A->on_msg(0, &h, T0 + 4300);
    CHECK(A->view().life == 38 && A->view().nblocked == 2);
    CHECK(A->view().phase == DU_FIGHT);  // (no KO, the duel goes on)
}
static void l9_tests(void) {
    l9_one(DGB_GUARD, "Guard window");        // L9a (s285 one-game)
    l9_one(DGB_INV, "invulnerable frames");   // L9b (s285 one-game)
    l9_one(DGB_REFLECT, "Reflect");           // L9c [8]
    l9_one(DGB_RCINV, "Dodge Roll frames");   // L9c [8] (g_rc_inv, s288 ruling)
    l9_one(DGB_ADODGE, "Aerial Dodge block"); // L9c [8] (s288 ruling)
    l9_one(DGB_LIMIT, "party Limit");         // L9d [8]
    // no block reason up: paused or not, the hit is taken (the control)
    A->reset(1);
    float him[3] = {0, 0, 3};
    uint32_t id = to_fight(A, him, T0);
    A->life(40);
    NetDuelMsg h;
    memset(&h, 0, sizeof h);
    h.op = NDO_HIT, h.id = id, h.power = 100, h.stat = 15, h.from[2] = 1.5f, h.hseq = 1;
    A->on_msg(0, &h, T0 + 4000);
    CHECK(A->view().life == 39 && A->view().nblocked == 0);
    // a Guard window facing AWAY from the attacker never blocked (front test), paused or not
    A->block(DGB_GUARD, true);
    h.hseq = 2, h.from[2] = -1.5f;
    A->on_msg(0, &h, T0 + 4100);
    CHECK(A->view().life == 38 && A->view().nblocked == 0);
}

// ---- L10 + L11: the time judge, two games ----
// A = player 1 at 0 0 0, B = player 2 at 3 0 0; each game's slot 0 is the other one. One tick: each sees the other's SNAP
// (feet, NF_DUEL while his duel is live, his HP %), runs its frame, and its packets reach the other (drop_b_op: lost B -> A)
static void two_setup(void) {
    A->reset(1), B->reset(2);
    B->body(3, 0, 0);
}
static void tick(uint64_t now, int drop_b_op) {
    float fa[3] = {0, 0, 0}, fb[3] = {3, 0, 0};
    DgView va = A->view(), vb = B->view();
    A->peer(0, 2, "copy2", fb, PEER_LIVE, false, vb.live ? NF_DUEL : 0, pct(vb.life), now);
    B->peer(0, 1, "PC", fa, PEER_LIVE, false, va.live ? NF_DUEL : 0, pct(va.life), now);
    A->frame(now);
    deliver(A, B, 0, now, 0);
    B->frame(now);
    deliver(B, A, 0, now, drop_b_op);
}
// A asks, B accepts; both in the countdown. -> the time after
static uint64_t two_start(void) {
    uint64_t t = T0;
    tick(t, 0);
    A->ask(0, t);
    deliver(A, B, 0, t, 0);
    tick(t += 100, 0);
    CHECK(B->view().phase == DU_ASKED);
    B->accept(t += 100);
    deliver(B, A, 0, t, 0);
    tick(t += 100, 0);
    CHECK(A->view().phase == DU_COUNT && B->view().phase == DU_COUNT && A->view().id == B->view().id);
    return t;
}
static void l10_tests(void) {
    // L10: A dueltime 20, B dueltime 180 (the s285 two-copy setup). A judges by HP at 20 s; B is still fighting and takes A's END
    two_setup();
    CHECK(A->live_key("dueltime", 20));
    uint64_t t = two_start();
    A->life(30), B->life(40);  // (after the start heal: A 62.5 %, B 83 %)
    bool a_timeup = false, b_fight_at_end = false;
    for (int i = 0; i < 400 && A->view().phase != DU_OVER; i++) {
        tick(t += 100, 0);
        a_timeup = a_timeup || A->view().phase == DU_TIMEUP;
        if (A->view().phase == DU_OVER) b_fight_at_end = true;
    }
    tick(t += 100, 0);  // (A's END reaches B's next frame)
    DgView va = A->view(), vb = B->view();
    CHECK(a_timeup && b_fight_at_end);
    CHECK(va.phase == DU_OVER && va.reason == DUE_TIME && va.result == DUR_LOSE);
    CHECK(LOGGED(A, "time limit: you 62.5 % vs copy2 83.0 %"));
    CHECK(LOGGED(A, "no TIME from copy2: his last SNAP's HP 83 % decides"));  // (B at 180 s sends no TIME: the SNAP decides on A)
    CHECK(vb.phase == DU_OVER && vb.reason == DUE_TIME && vb.result == DUR_WIN);  // (the same result on both, reason "time limit")
    CHECK(!strcmp(va.banner, "You lose...") && !strcmp(vb.banner, "You win!"));  // (not "You win! (your friend left)")
    CHECK(!LOGGED(B, "shows no duel for"));
    CHECK(vb.life == 48 && va.life == 48);  // (both healed after)

    // L10 the other way: A leads, so B (180 s) takes a LOSE from A's END at A's 20 s
    two_setup();
    A->live_key("dueltime", 20);
    t = two_start();
    A->life(44), B->life(10);
    for (int i = 0; i < 400 && B->view().phase != DU_OVER; i++) tick(t += 100, 0);
    CHECK(A->view().result == DUR_WIN && B->view().result == DUR_LOSE && B->view().reason == DUE_TIME);
    CHECK(!strcmp(B->view().banner, "You lose..."));
}
static void l11_tests(void) {
    // L11 [8]: both dueltime 20. B (asked first: B's countdown started 100 ms before A's) reaches the time up first; B's TIME to A
    // is LOST. A's TIME reaches B: B judges (B 41.6 % vs A 83.3 %: B loses), ends, is healed (his SNAP says 100 %), and sends END.
    // Before s284 A would wait 3 s and judge by that healed SNAP (100 % vs 83.3 %: A loses a duel it led). Now B's END decides.
    two_setup();
    A->live_key("dueltime", 20), B->live_key("dueltime", 20);
    uint64_t t = two_start();
    A->life(40), B->life(20);
    for (int i = 0; i < 400 && A->view().phase != DU_OVER; i++) tick(t += 100, NDO_TIME);
    tick(t += 100, NDO_TIME);
    DgView va = A->view(), vb = B->view();
    CHECK(vb.phase == DU_OVER && vb.reason == DUE_TIME && vb.result == DUR_LOSE);
    CHECK(va.phase == DU_OVER && va.reason == DUE_TIME && va.result == DUR_WIN);
    CHECK(LOGGED(A, "time up: your HP 83.3 %"));             // (A reached its own time up)
    CHECK(!LOGGED(A, "no TIME from copy2"));                // (the healed-SNAP fallback never ran on A)
    CHECK(!LOGGED(A, "time limit: you"));                   // (A did not judge itself: B's END did)
    CHECK(!strcmp(va.banner, "You win!") && !strcmp(vb.banner, "You lose..."));
    CHECK(pct(vb.life) == 100);                             // (B healed: what A's fallback would have read)
    // FYI (not a check, the fix does not cover it): with B's END lost TOO, A still falls back to B's healed SNAP after 3 s
    two_setup();
    A->live_key("dueltime", 20), B->live_key("dueltime", 20);
    t = two_start();
    A->life(40), B->life(20);
    for (int i = 0; i < 400 && A->view().phase != DU_OVER; i++) {
        A->frame(t += 100);  // (B's packets all lost from here: TIME and END)
        float fb[3] = {3, 0, 0};
        DgView vb2 = B->view();
        float fa[3] = {0, 0, 0};
        DgView va2 = A->view();
        A->peer(0, 2, "copy2", fb, PEER_LIVE, false, vb2.live ? NF_DUEL : 0, pct(vb2.life), t);
        B->peer(0, 1, "PC", fa, PEER_LIVE, false, va2.live ? NF_DUEL : 0, pct(va2.life), t);
        deliver(A, B, 0, t, 0);
        B->frame(t);
        B->tx_clear();
    }
    printf("  FYI L11 with TIME + END both lost: A %s (%s), B %s; A's log %s the healed-SNAP fallback\n", DUR_NAME[A->view().result],
           DUE_NAME[A->view().reason], DUR_NAME[B->view().result], LOGGED(A, "no TIME from copy2") ? "used" : "did not use");
}

// ---- L12 [8]: "set duel 0" also clears two OTHER players' ring seen here ----
static void l12_tests(void) {
    A->reset(1);
    float f2[3] = {5, 0, 5}, f3[3] = {7, 0, 5};
    A->peer(0, 2, "copy2", f2, PEER_LIVE, false, NF_DUEL, 100, T0);
    A->peer(1, 3, "copy3", f3, PEER_LIVE, false, NF_DUEL, 100, T0);
    NetDuelMsg r;
    memset(&r, 0, sizeof r);
    r.op = NDO_RING, r.id = 0x777, r.a = 2, r.b = 3, r.phase = 2, r.radius_cm = 2000, r.c[0] = 6, r.c[2] = 5;
    wire(A, 0, 2, &r, T0);
    A->frame(T0);
    DgView v = A->view();
    CHECK(v.others_on == 1 && v.fx_live == 1 && v.nfx_spawn == 1);  // (their Cure ring drawn here, 6 m off)
    CHECK(v.phase == DU_IDLE && !v.live && v.k == -1);             // (nothing of OUR duel: the only state is theirs)
    A->frame(T0 + 100);
    CHECK(A->view().fx_live == 1 && A->view().nfx_spawn == 1);     // (kept, not re-spawned)
    A->tx_clear(), A->log_clear();
    CHECK(A->live_key("duel", 0));
    A->frame(T0 + 200);
    v = A->view();
    CHECK(v.others_on == 0 && v.fx_live == 0 && v.nfx_kill == 1);  // (s284: cleaned up although we had no duel / ring of our own)
    CHECK(A->ntx() == 0);                                          // (nothing sent: no END / ANSWER for a duel we were not in)
    A->frame(T0 + 300);
    CHECK(A->view().nfx_kill == 1);                                // (once)
    // duel 1 again: a new RING draws it again
    A->live_key("duel", 1);
    wire(A, 0, 2, &r, T0 + 400);
    A->frame(T0 + 400);
    CHECK(A->view().others_on == 1 && A->view().fx_live == 1);
    // Sora mode off takes the same way out (du_frame: !g_on) -- not reachable from the API here; duel 0 covers the branch
}

int main(void) {
    l6_tests();
    l8_tests();
    l9_tests();
    l10_tests();
    l11_tests();
    l12_tests();
    printf("duelglue_test: %d / %d passed\n", runs - fails, runs);
    return fails ? 1 : 0;
}

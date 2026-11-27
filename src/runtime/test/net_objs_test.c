// host test for runtime/sora_net_objs_core.h + NET_OBJS / NET_OBJEV in sora_net_proto.h (s289 objs: netbump + netobjs, after
// Savage Wind PR #294 by byluki, MIT). The pure parts only (no game, no socket, no exe): the bump capsule and its "why not",
// the object kinds, the "whose an object is" rules (who began first, the tie, held = his, a new stream after a gap, the
// send-while-moving rule, the follow hold), the break test, the room's broken list (index, full, rotation) and the wire.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/net_objs_test.c -o build_h/net_objs_test.exe
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "sora_net_proto.h"
#include "sora_net_objs_core.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1e-4)

static void rot_m(float m[12], double ax, double ay, double az, double a, float x, float y, float z);  // (below)
static void test_bump(void) {
    float me[3] = {10, 5, 20}, out[2];
    float at[3][3];
    uint8_t id[3] = {2, 3, 4};
    CHECK(!nb_push(me, (const float (*)[3])at, id, 0, 1, out) && out[0] == 0 && out[1] == 0);  // (nobody)
    at[0][0] = 9.5f, at[0][1] = 5, at[0][2] = 20;  // (0.5 m to -x: pushed +x by 0.2)
    CHECK(nb_push(me, (const float (*)[3])at, id, 1, 1, out) && NEAR(out[0], 0.2f) && NEAR(out[1], 0));
    at[0][0] = 10, at[0][2] = 20.6f;  // (0.6 m to +z: pushed -z by 0.1)
    CHECK(nb_push(me, (const float (*)[3])at, id, 1, 1, out) && NEAR(out[0], 0) && NEAR(out[1], -0.1f));
    at[0][2] = 20.71f;  // (outside 0.7 m)
    CHECK(!nb_push(me, (const float (*)[3])at, id, 1, 1, out));
    at[0][2] = 20.3f, at[0][1] = 5 + 1.8f;  // (above the 1.7 m band: a friend on a ledge)
    CHECK(!nb_push(me, (const float (*)[3])at, id, 1, 1, out));
    at[0][1] = 5 - 1.6f;  // (within it)
    CHECK(nb_push(me, (const float (*)[3])at, id, 1, 1, out) && NEAR(out[1], -0.4f));
    at[0][1] = NAN;  // (a bad place: no push)
    CHECK(!nb_push(me, (const float (*)[3])at, id, 1, 1, out));
    // exactly on top of each other: the lower id goes +x, the other -x, by the whole radius, clamped to the 0.5 m step
    at[0][0] = 10, at[0][1] = 5, at[0][2] = 20;
    CHECK(nb_push(me, (const float (*)[3])at, id, 1, 1, out) && NEAR(out[0], NB_STEP) && NEAR(out[1], 0));
    uint8_t lo = 1;
    CHECK(nb_push(me, (const float (*)[3])at, &lo, 1, 2, out) && NEAR(out[0], -NB_STEP));  // (his side, the same moment)
    CHECK(!nb_push(me, (const float (*)[3])at, &lo, 1, 1, out));  // (the same id: no side)
    // two friends either side at the same distance cancel; three overlapping ones are clamped to 0.5 m
    at[0][0] = 9.6f, at[0][2] = 20, at[1][0] = 10.4f, at[1][1] = 5, at[1][2] = 20;
    CHECK(!nb_push(me, (const float (*)[3])at, id, 2, 1, out));
    at[1][0] = 9.6f, at[1][2] = 20.05f, at[2][0] = 9.65f, at[2][1] = 5, at[2][2] = 19.95f;
    CHECK(nb_push(me, (const float (*)[3])at, id, 3, 1, out) && NEAR(sqrt((double)out[0] * out[0] + (double)out[1] * out[1]), NB_STEP) && out[0] > 0);
    // both games, each its own Link: the pushes are opposite (they move apart, never the same way)
    float a[3] = {0, 0, 0}, b[3] = {0.3f, 0, 0.2f}, pa[2], pb[2];
    uint8_t ia = 7, ib = 9;
    CHECK(nb_push(a, (const float (*)[3])&b, &ib, 1, ia, pa) && nb_push(b, (const float (*)[3])&a, &ia, 1, ib, pb));
    CHECK(NEAR(pa[0], -pb[0]) && NEAR(pa[1], -pb[1]) && pa[0] < 0);
    // the reasons for none
    CHECK(nb_why(false, false, false, 0x18000000u, 0, false) == NBW_OK);
    CHECK(nb_why(true, true, true, 0, 1, true) == NBW_DOWNED);
    CHECK(nb_why(false, true, false, 0x18000000u, 0, false) == NBW_DEAD);
    CHECK(nb_why(false, false, true, 0x18000000u, 0, false) == NBW_LOADING);
    CHECK(nb_why(false, false, false, 0x100000u, 0, false) == NBW_RIDING);
    CHECK(nb_why(false, false, false, 0x100010u, 0, false) == NBW_RIDING);
    CHECK(nb_why(false, false, false, 0, 0, false) == NBW_EVENT && nb_why(false, false, false, 1, 0, false) == NBW_EVENT);
    CHECK(nb_why(false, false, false, 0x10, 1, false) == NBW_RAGDOLL);
    CHECK(nb_why(false, false, false, 0x10, 0, true) == NBW_FAST);
    CHECK(nb_why(false, false, false, 0x10, -1, false) == NBW_OK);  // (linkpos_link_ragdoll -1 = not known)
    for (int i = 0; i < NBW_N; i++) CHECK(NBW_TXT[i] && NBW_TXT[i][0]);
}

static void test_kind(void) {
    CHECK(no_kind("Obj_BoxWood_A_01") && no_kind("Obj_BarrelWood_A") && no_kind("Obj_TreeApple_A_L_01_Trunk") && no_kind("DgnObj_IronBall_A"));
    CHECK(!no_kind("Enemy_Bokoblin_Junior") && !no_kind("Npc_King") && !no_kind("Item_Apple") && !no_kind("Weapon_Sword_001"));
    CHECK(!no_kind("Animal_Horse_A") && !no_kind("GameRomHorse00") && !no_kind("PlayerObj") && !no_kind("OnlineHorse_A"));
    CHECK(!no_kind("Obj_LiftRockWhite_A_01") && !no_kind("DgnObj_Elevator_A") && !no_kind("TwnObj_Gate_A") && !no_kind("DgnObj_RotateFloor"));
    CHECK(!no_kind("") && !no_kind(NULL));
    CHECK(no_kind("PlayerObjX"));  // (only the player's exact name: the PR's strcmp)
}

static void test_owner(void) {
    float m[12] = {1, 0, 0, 5, 0, 1, 0, 6, 0, 0, 1, 7};
    NoOwn o;
    // his record, we are not sending: his, followed 90 frames, our own moves quiet
    memset(&o, 0, sizeof o);
    CHECK(no_his(&o, 1000, 1, 2, false, m) && o.hold == NO_HOLD && o.quiet_until == 1000 + NO_QUIET_MS && o.his_id == 2 && o.his_m[7] == 6);
    CHECK(!no_ours(&o, 1050, 1.0f, false));  // (we drive it: not sent)
    for (int f = 0; f < NO_HOLD - 1; f++) CHECK(no_follow(&o, 1100 + f) == 1);
    CHECK(no_follow(&o, 1200) == 2 && no_follow(&o, 1201) == 0 && o.quiet_until == 1200 + NO_QUIET_MS);
    CHECK(!no_ours(&o, 1200 + NO_QUIET_MS - 1, 1.0f, false));  // (still quiet)
    CHECK(no_ours(&o, 1200 + NO_QUIET_MS, 1.0f, false) && o.sending);  // (then a move of it is ours)
    // we began long before his stream: ours, his record dropped, ours keeps going
    memset(&o, 0, sizeof o);
    CHECK(no_ours(&o, 0 + 10, 0.05f, false) && o.sending && o.send_t == 10);
    CHECK(!no_his(&o, 1000, 3, 2, false, m) && o.sending && o.hold == 0);
    // both within the tie: the lower id keeps it
    memset(&o, 0, sizeof o);
    no_ours(&o, 5000, 0.05f, false);
    CHECK(!no_his(&o, 5100, 1, 2, false, m));  // (we 1 < he 2: ours)
    memset(&o, 0, sizeof o);
    no_ours(&o, 5000, 0.05f, false);
    CHECK(no_his(&o, 5100, 3, 2, false, m) && !o.sending && o.hold == NO_HOLD);  // (we 3 > he 2: his)
    memset(&o, 0, sizeof o);
    no_ours(&o, 5000, 0.05f, false);
    CHECK(no_his(&o, 4800, 1, 2, false, m) == false);  // (he began 0.2 s before us: a tie still, we are lower)
    // he began well before us (a stream of his we were not following yet: its first record came, then ours started)
    memset(&o, 0, sizeof o);
    o.his_seen = 4550, o.his_start = 4000;  // (his stream running since 4.0 s, his last record 50 ms ago)
    o.sending = true, o.send_t = 4500;      // (ours since 4.5 s: after his + the tie)
    CHECK(no_his(&o, 4600, 1, 2, false, m));  // (his, even with our lower id)
    // a new stream after a gap
    memset(&o, 0, sizeof o);
    no_his(&o, 1000, 1, 2, false, m);
    no_his(&o, 1400, 1, 2, false, m);
    CHECK(o.his_start == 1000);
    no_his(&o, 1950, 1, 2, false, m);
    CHECK(o.his_start == 1950);
    // held by him (Magnesis / Stasis): his even if we began first, and 4 s after his last word of it
    memset(&o, 0, sizeof o);
    no_ours(&o, 100, 0.05f, false);
    CHECK(no_his(&o, 2000, 1, 2, true, m) && o.his_until == 2000 + NO_HELD_MS);
    o.hold = 0, o.quiet_until = 0;
    CHECK(!no_ours(&o, 2000 + NO_HELD_MS - 1, 1.0f, false) && !no_ours(&o, 1, 1.0f, true));
    CHECK(no_ours(&o, 2000 + NO_HELD_MS, 1.0f, false));
    // ours while it moves, then 20 still samples (the last = where it rests), then nothing
    memset(&o, 0, sizeof o);
    CHECK(!no_ours(&o, 10, 0.01f, false) && !o.sending);  // (2 cm or less: not moving)
    CHECK(no_ours(&o, 20, 0.03f, false) && o.sending);
    int sent = 0;
    for (int i = 0; i < NO_STILL + 5; i++) sent += no_ours(&o, 30 + i, 0.0f, false);
    CHECK(sent == NO_STILL && !o.sending);
    CHECK(no_ours(&o, 100, 0.5f, false) && o.send_t == 100);  // (moving again: a new run of ours)
}

static void test_break(void) {
    CHECK(no_broke(false, false, 3, 0.5f, false));
    CHECK(!no_broke(true, false, 3, 0.5f, false));       // (loading: the world's actors go)
    CHECK(!no_broke(false, false, 15.5f, 0.5f, false));  // (too far from our Link)
    CHECK(!no_broke(false, false, 3, 10.5f, false));     // (our Link went far since: unloaded, not broken)
    CHECK(!no_broke(false, true, 3, 0.5f, false) && no_broke(false, true, 3, 0.5f, true));  // (a tree: its trunk at its foot)
    CHECK(!no_broke(false, false, NAN, 0.5f, false));
    int8_t d[2];
    no_dir_enc(1, 0, d);
    CHECK(d[0] == 127 && d[1] == 0);
    no_dir_enc(-3, 4, d);
    CHECK(d[0] == -76 && d[1] == 102);
    no_dir_enc(0, 0, d);
    CHECK(d[0] == 0 && d[1] == 0);
    no_dir_enc(NAN, 1, d);
    CHECK(d[0] == 0 && d[1] == 0);
}

// (s289 objs fix, lab 2:30 am) what went while we followed it is not our break; a breakable is put there only at rest
static void test_followed(void) {
    static NoFollowed f;
    memset(&f, 0, sizeof f);
    CHECK(!nf_recent(&f, 0xb86d5173u, 1000));
    nf_note(&f, 0xb86d5173u, 1000);  // (a frame of the follow)
    CHECK(nf_recent(&f, 0xb86d5173u, 1000) && nf_recent(&f, 0xb86d5173u, 1000 + NO_FOLLOW_GRACE_MS - 1));
    CHECK(!nf_recent(&f, 0xb86d5173u, 1000 + NO_FOLLOW_GRACE_MS));  // (1 s after the last frame: ours again)
    CHECK(!nf_recent(&f, 0x8136b931u, 1000));                       // (its neighbour was never followed)
    nf_note(&f, 0xb86d5173u, 1500);                                  // (still followed: the grace runs from the last frame)
    CHECK(nf_recent(&f, 0xb86d5173u, 2400) && !nf_recent(&f, 0xb86d5173u, 2500));
    int n = 0;
    for (int i = 0; i < NO_FOLLOWED_N; i++) n += f.h[i] == 0xb86d5173u;
    CHECK(n == 1);  // (one entry per HashId)
    nf_note(&f, 0, 1000);  // (no HashId: nothing)
    CHECK(!nf_recent(&f, 0, 1000));
    // the lab's case: followed while it broke (the break scan runs within 250 ms of it), then a real break of ours later
    memset(&f, 0, sizeof f);
    for (uint64_t t = 5000; t <= 5800; t += 16) nf_note(&f, 0xb86d5173u, t);  // (followed 0.8 s, then it went at 5800)
    CHECK(nf_recent(&f, 0xb86d5173u, 5800 + 250));                          // (the scan: not told)
    CHECK(!nf_recent(&f, 0xb86d5173u, 5800 + NO_FOLLOW_GRACE_MS + 1));      // (a break of ours a while after: told)
    // full: 32 followed, a 33rd takes the slot that ends first; expired slots are reused before that
    memset(&f, 0, sizeof f);
    for (uint32_t k = 1; k <= NO_FOLLOWED_N; k++) nf_note(&f, k, 100 + k);
    nf_note(&f, 999, 200);
    CHECK(nf_recent(&f, 999, 200) && !nf_recent(&f, 1, 200) && nf_recent(&f, 2, 200) && nf_recent(&f, NO_FOLLOWED_N, 200));
    nf_note(&f, 1000, 100 + NO_FOLLOW_GRACE_MS + 3);  // (2 and 3 have ended: 2's slot, the first expired one)
    CHECK(nf_recent(&f, 1000, 100 + NO_FOLLOW_GRACE_MS + 3) && nf_recent(&f, 999, 100 + NO_FOLLOW_GRACE_MS + 3) &&
          nf_recent(&f, 4, 100 + NO_FOLLOW_GRACE_MS + 3));
    n = 0;
    for (int i = 0; i < NO_FOLLOWED_N; i++) n += f.h[i] == 2;
    CHECK(n == 0);
    // at rest: put there only when ours is off his by more than a jitter
    float a[12] = {1, 0, 0, -1117.7f, 0, 1, 0, 237.3f, 0, 0, 1, 1892.6f}, b[12];
    memcpy(b, a, sizeof b);
    CHECK(!no_put_off(a, b));
    b[3] += 0.04f, b[7] -= 0.02f;  // (4.5 cm: a jitter)
    CHECK(!no_put_off(a, b));
    b[3] += 0.02f;  // (6.3 cm)
    CHECK(no_put_off(a, b));
    memcpy(b, a, sizeof b);
    b[11] -= 2.0f;  // (the lab: pushed 2 m)
    CHECK(no_put_off(a, b));
    memcpy(b, a, sizeof b);
    float c = cosf(0.1f), s = sinf(0.1f);  // (turned 5.7 degrees about y, in place)
    b[0] = c, b[2] = s, b[8] = -s, b[10] = c;
    CHECK(no_put_off(a, b));
    c = cosf(0.03f), s = sinf(0.03f);  // (1.7 degrees: a jitter)
    b[0] = c, b[2] = s, b[8] = -s, b[10] = c;
    CHECK(!no_put_off(a, b));
    b[5] = NAN;  // (a bad record of his: never put there)
    CHECK(!no_put_off(a, b));
    memcpy(b, a, sizeof b), b[3] = INFINITY;
    CHECK(!no_put_off(a, b));
    float bad[12];
    memcpy(bad, a, sizeof bad), bad[5] = NAN;  // (ours unreadable in a rotation entry, his good: put there)
    CHECK(no_put_off(bad, a));
    // (s289 objs fix 2) the warp waits while something stands where it would go
    float to[3] = {-1120.2f, 237.7f, 1894.6f}, o[3][3] = {{-1108.1f, 237.7f, 1893.8f}, {-1119.6f, 237.7f, 1894.3f}, {NAN, 0, 0}};
    CHECK(no_put_block(to, NULL, NULL, 0) == -1);                          // (nothing near)
    CHECK(no_put_block(to, NULL, (const float (*)[3])o, 1) == -1);         // (the next barrel, 12 m away)
    CHECK(no_put_block(to, NULL, (const float (*)[3])o, 2) == 2);          // (another 0.67 m from the spot: o[1])
    float nan_only[1][3] = {{NAN, NAN, NAN}};
    CHECK(no_put_block(to, NULL, (const float (*)[3])nan_only, 1) == -1);  // (an unreadable place: nothing in the way)
    float L[3] = {-1120.6f, 237.0f, 1894.9f};                              // (our Link 0.5 m off in x z, 0.7 m lower)
    CHECK(no_put_block(to, L, (const float (*)[3])o, 2) == 0);             // (our Link first)
    L[1] = 240.0f;                                                         // (2.3 m over it: a ledge above)
    CHECK(no_put_block(to, L, (const float (*)[3])o, 1) == -1);
    L[1] = 237.7f, L[0] = to[0] + NO_PUT_CLEAR + 0.01f, L[2] = to[2];      // (just out of reach)
    CHECK(no_put_block(to, L, NULL, 0) == -1);
    o[0][0] = to[0], o[0][1] = to[1] + NO_PUT_CLEAR - 0.01f, o[0][2] = to[2];  // (one standing on the spot's top)
    CHECK(no_put_block(to, NULL, (const float (*)[3])o, 1) == 1);
    // (s289 objs fix 3) after the put it settles: the lab's roll (0.41 m in 50 ms) is the put's until NO_STILL still samples
    NoSettle ns;
    memset(&ns, 0, sizeof ns);
    CHECK(nos_sample(&ns, 1000, 0.5f, 50.0f) == NOS_LATE);                  // (not settling: nothing to do)
    nos_start(&ns, 1000);
    CHECK(ns.on && nos_sample(&ns, 1050, 0.41f, 60.0f) == NOS_ON);           // (rolling, our Link 60 m up the cliff)
    int kk = 0, rr = NOS_ON;
    for (; kk < 200 && rr == NOS_ON; kk++) rr = nos_sample(&ns, 1100 + (uint64_t)kk * 50, kk < 40 ? 0.3f : 0.005f, 60.0f);
    CHECK(rr == NOS_RESTS && kk == 40 + NO_STILL && !ns.on);                  // (40 rolling samples, then 20 still = at rest)
    nos_start(&ns, 1000);
    CHECK(nos_sample(&ns, 1050, 0.005f, NAN) == NOS_ON && ns.still == 1);   // (no Link place known: not near)
    CHECK(nos_sample(&ns, 1100, NAN, 60.0f) == NOS_ON && ns.still == 0);    // (an unreadable move = moving)
    CHECK(nos_sample(&ns, 1150, 0.005f, NO_SETTLE_LINK_M - 0.1f) == NOS_LINK && !ns.on);  // (our Link came to it: ours again)
    nos_start(&ns, 1000);
    CHECK(nos_sample(&ns, 1000 + NO_SETTLE_MS - 1, 0.3f, 60.0f) == NOS_ON);
    CHECK(nos_sample(&ns, 1000 + NO_SETTLE_MS, 0.3f, 60.0f) == NOS_LATE && !ns.on);  // (still rolling at 8 s: ours again)
    // at rest: near his place = kept; far from his rest = put once more; far from a place that was no rest = kept, not sent
    CHECK(nos_pick(0.94f, true, 0) == 0 && nos_pick(NO_SETTLE_OK_M, false, 0) == 0);
    CHECK(nos_pick(22.4f, true, 0) == 1 && nos_pick(22.4f, true, NO_REPUTS) == 2);
    CHECK(nos_pick(22.4f, false, 0) == 2 && nos_pick(NAN, true, 0) == 1 && nos_pick(NAN, false, 0) == 2);
}

static NoWorld W;
static void test_world(void) {
    nw_clear(&W);
    NwEnt e = {.h = 0x1234, .kind = NOK_BREAK, .pos = {1, 2, 3}};
    CHECK(nw_find(&W, 0x1234) < 0 && nw_add(&W, &e, 1000) == 1 && nw_find(&W, 0x1234) == 0 && W.new_t == 1000);
    CHECK(nw_add(&W, &e, 2000) == 0 && W.n == 1 && W.new_t == 1000);  // (already there: not new, not told again)
    NwEnt z = {.h = 0};
    CHECK(nw_add(&W, &z, 1) == -1 && nw_find(&W, 0) < 0 && nw_add(&W, NULL, 1) == -1);
    // HashIds that share a slot are all found
    uint32_t same[6], s0 = nw_slot(0x1234);
    int ns = 0;
    for (uint32_t h = 1; ns < 6 && h < 50000000u; h++)
        if (h != 0x1234 && nw_slot(h) == s0) same[ns++] = h;
    CHECK(ns == 6);
    for (int i = 0; i < ns; i++) { NwEnt x = {.h = same[i], .kind = NOK_TREE}; CHECK(nw_add(&W, &x, 3000) == 1); }
    for (int i = 0; i < ns; i++) CHECK(nw_find(&W, same[i]) == i + 1);
    CHECK(nw_find(&W, 0x1234) == 0);
    // rotation: not while breaks are new, then one each 1.5 s, round the list
    CHECK(!nw_rotate(&W, 3000 + 4499));
    const NwEnt *r = nw_rotate(&W, 3000 + 4500);
    CHECK(r && r->h == 0x1234);
    CHECK(!nw_rotate(&W, 3000 + 4500 + 1499));
    r = nw_rotate(&W, 3000 + 6000);
    CHECK(r && r->h == same[0]);
    uint64_t t = 3000 + 6000;
    for (int i = 1; i < ns; i++) t += NW_ROTATE_MS, r = nw_rotate(&W, t);
    CHECK(r && r->h == same[ns - 1]);
    r = nw_rotate(&W, t + NW_ROTATE_MS);
    CHECK(r && r->h == 0x1234);  // (round again)
    NwEnt nw = {.h = 0x9999, .kind = NOK_BREAK};
    nw_add(&W, &nw, t + 2000);
    CHECK(!nw_rotate(&W, t + 2000 + NW_ROTATE_MS));  // (a new break: quiet again first)
    // full at NW_MAX: the next one refused, all the others still found
    nw_clear(&W);
    for (uint32_t h = 1; h <= NW_MAX; h++) { NwEnt x = {.h = h * 7919u}; nw_add(&W, &x, 1); }
    NwEnt over = {.h = 0x7fffffffu};
    CHECK(W.n == NW_MAX && nw_add(&W, &over, 2) == -1);
    int found = 0;
    for (uint32_t h = 1; h <= NW_MAX; h++) found += nw_find(&W, h * 7919u) == (int)h - 1;
    CHECK(found == NW_MAX && nw_find(&W, 0x7fffffffu) < 0);
    nw_clear(&W);
    CHECK(W.n == 0 && nw_find(&W, 7919u) < 0 && !nw_rotate(&W, 999999));
}

static void test_wire(void) {
    uint8_t b[NET_PKT_MAX + 16];
    NetObjRec r[NET_OBJS_N + 1], o[NET_OBJS_N];
    memset(r, 0, sizeof r);
    for (int i = 0; i <= NET_OBJS_N; i++) {
        r[i].kind = NOR_OBJ, r[i].flags = (uint8_t)(i & 1), r[i].h = 0x10000u + (uint32_t)i, r[i].extra = 0;
        rot_m(r[i].m, 0.1, 1, 0.2, 0.3 * i, 0, 0, 0);  // (s290: kind 1 needs a real rotation)
        for (int j = 3; j < 12; j += 4) r[i].m[j] = (float)(i * 100 + j) * 0.5f - 1000.25f;
    }
    int n = net_pack_objs(5, 77, r, NET_OBJS_N, b, (int)sizeof b), no = 0;
    CHECK(n == NET_OBJS_PKT && n == 704);
    uint8_t pl = 0;
    uint16_t sq = 0;
    int t = 0;
    CHECK(net_unpack_ex(b, n, &t, &pl, &sq, NULL, NULL, NULL, NULL) && t == NET_OBJS && pl == 5 && sq == 77);
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == NET_OBJS_N);
    CHECK(o[11].h == 0x1000bu && o[11].flags == 1 && o[11].m[11] == r[11].m[11] && o[0].m[0] == r[0].m[0] && o[3].kind == NOR_OBJ);
    CHECK(net_pack_objs(5, 1, r, NET_OBJS_N + 1, b, (int)sizeof b) < 0 && net_pack_objs(5, 1, r, 0, b, (int)sizeof b) < 0);
    CHECK(net_pack_objs(5, 1, r, 2, b, NET_HDR + 1 + 2 * NET_OBJS_REC - 1) < 0);
    n = net_pack_objs(5, 2, r, 3, b, (int)sizeof b);
    CHECK(n == NET_HDR + 1 + 3 * NET_OBJS_REC);
    CHECK(!net_unpack_ex(b, n - 1, &t, &pl, &sq, NULL, NULL, NULL, NULL) && !net_unpack_ex(b, n + 1, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    b[NET_HDR] = 4;  // (a count that does not match the size)
    CHECK(!net_unpack_objs(b, n, &pl, &sq, o, &no));
    b[NET_HDR] = 3;
    // one bad record (HashId 0 / a kind this build does not take (4) / NaN / huge) is dropped alone
    NetObjRec bad[4];
    memcpy(bad, r, sizeof bad);
    bad[0].h = 0, bad[1].kind = 4, bad[2].m[5] = NAN;
    n = net_pack_objs(5, 3, bad, 4, b, (int)sizeof b);
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == 1 && o[0].h == r[3].h);
    bad[3].m[3] = 2e6f;
    n = net_pack_objs(5, 4, bad, 4, b, (int)sizeof b);
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == 0);
    // NET_OBJEV
    NetObjEv e = {.op = NOE_BREAK, .kind = NOK_TREE, .h = 0xabcdef01u, .pos = {-1234.5f, 210.25f, 3456.75f}, .dir = {-76, 102}}, f;
    n = net_pack_objev(9, 300, &e, b, (int)sizeof b);
    CHECK(n == NET_OBJEV_LEN && n == 29);
    CHECK(net_unpack_objev(b, n, &pl, &sq, &f) && pl == 9 && sq == 300 && f.op == NOE_BREAK && f.kind == NOK_TREE && f.h == e.h);
    CHECK(f.pos[0] == e.pos[0] && f.pos[2] == e.pos[2] && f.dir[0] == -76 && f.dir[1] == 102);
    CHECK(!net_unpack_objs(b, n, &pl, &sq, o, &no) && !net_unpack_objev(b, n - 1, &pl, &sq, &f));
    b[NET_HDR] = 3;  // (op 3: not ours)
    CHECK(!net_unpack_ex(b, n, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    b[NET_HDR] = NOE_TOLD, b[NET_HDR + 1] = 3;  // (kind 3)
    CHECK(!net_unpack_ex(b, n, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    b[NET_HDR + 1] = NOK_BREAK;
    CHECK(net_unpack_objev(b, n, &pl, &sq, &f) && f.op == NOE_TOLD && f.kind == NOK_BREAK);
    e.pos[1] = NAN;
    n = net_pack_objev(9, 301, &e, b, (int)sizeof b);
    CHECK(n == NET_OBJEV_LEN && !net_unpack_objev(b, n, &pl, &sq, &f));
    e.pos[1] = 1, e.h = 0;
    CHECK(net_pack_objev(9, 302, &e, b, (int)sizeof b) < 0);
    e.h = 1, e.op = 3;
    CHECK(net_pack_objev(9, 303, &e, b, (int)sizeof b) < 0);
    e.op = NOE_BREAK;
    CHECK(net_pack_objev(9, 304, &e, b, NET_OBJEV_LEN - 1) < 0);
    // another type is not taken as ours; ours are not taken as another (a NET_GEAR body check on an OBJS size)
    n = net_pack_objs(5, 5, r, 1, b, (int)sizeof b);
    b[3] = NET_GEAR;
    CHECK(!net_unpack_objs(b, n, &pl, &sq, o, &no));
    CHECK(NET_OBJS != NET_GEAR && NET_OBJEV != NET_DUEL && NET_OBJS == 17 && NET_OBJEV == 18);
}

// (parts 3-4) a rotation about axis (ax, ay, az) by a radians at place (x, y, z), row-major 3x4
static void rot_m(float m[12], double ax, double ay, double az, double a, float x, float y, float z) {
    double L = sqrt(ax * ax + ay * ay + az * az);
    ax /= L, ay /= L, az /= L;
    double c = cos(a), s = sin(a), C = 1 - c;
    m[0] = (float)(c + ax * ax * C), m[1] = (float)(ax * ay * C - az * s), m[2] = (float)(ax * az * C + ay * s), m[3] = x;
    m[4] = (float)(ay * ax * C + az * s), m[5] = (float)(c + ay * ay * C), m[6] = (float)(ay * az * C - ax * s), m[7] = y;
    m[8] = (float)(az * ax * C - ay * s), m[9] = (float)(az * ay * C + ax * s), m[10] = (float)(c + az * az * C), m[11] = z;
}
static double m_err(const float a[12], const float b[12]) {
    double e = 0;
    for (int k = 0; k < 12; k++) e = fmax(e, fabs((double)a[k] - b[k]));
    return e;
}

static void test_pose_wire(void) {
    // the quaternion both ways: any rotation (also near 180 degrees, where w is small) comes back within 1e-3
    float m[12], back[12], q[4];
    const double axes[6][3] = {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}, {1, 1, 0}, {-0.3, 0.8, 0.5}, {0.2, -0.1, 0.97}};
    const double angs[7] = {0, 0.3, 1.2, 2.5, 3.1, 3.14159, -2.0};
    int bad_n = 0;
    for (int a = 0; a < 6; a++)
        for (int g = 0; g < 7; g++) {
            rot_m(m, axes[a][0], axes[a][1], axes[a][2], angs[g], 12.5f, -3.25f, 1999.75f);
            if (!net_q_from_m(m, q) || q[3] < 0) bad_n++;
            float t[3] = {m[3], m[7], m[11]};
            net_m_from_q(q, t, back);
            if (m_err(m, back) > 1e-4) bad_n++;
        }
    CHECK(bad_n == 0);
    // pose records on the wire: kind 2 (a mover) and 3 (a horse, its saddle), place exact, rotation within 1e-3, the AS name
    NetObjRec r[3], o[NET_OBJS_N];
    memset(r, 0, sizeof r);
    r[0].kind = NOR_MOVER, r[0].flags = NORF_REST, r[0].h = 0x55aa1234u;
    rot_m(r[0].m, 0, 1, 0, 1.0, -812.5f, 133.25f, 2201.0f);
    snprintf(r[0].as, sizeof r[0].as, "Move_Up");
    r[1].kind = NOR_HORSE, r[1].flags = NORH_HORSE, r[1].h = nh_crc32("GameRomHorse12");
    rot_m(r[1].m, 0.1, 1, 0, 3.1, 100, 20, 300);
    snprintf(r[1].as, sizeof r[1].as, "Move_Gear4");
    r[2].kind = NOR_HORSE, r[2].flags = NORH_SADDLE, r[2].h = nh_crc32("GameRomHorseSaddle_00");
    rot_m(r[2].m, 0, 1, 0, -0.5, 100, 21, 300);
    uint8_t b[NET_PKT_MAX + 16], pl = 0;
    uint16_t sq = 0;
    int n = net_pack_objs(3, 9, r, 3, b, (int)sizeof b), no = 0;
    CHECK(n == NET_HDR + 1 + 3 * NET_OBJS_REC);
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == 3);
    CHECK(o[0].kind == NOR_MOVER && o[0].flags == NORF_REST && o[0].h == 0x55aa1234u && !strcmp(o[0].as, "Move_Up"));
    CHECK(o[0].m[3] == r[0].m[3] && o[0].m[7] == r[0].m[7] && o[0].m[11] == r[0].m[11] && m_err(o[0].m, r[0].m) < 1e-3);
    CHECK(o[1].kind == NOR_HORSE && nh_part(o[1].flags) == 0 && !strcmp(o[1].as, "Move_Gear4") && m_err(o[1].m, r[1].m) < 1e-3);
    CHECK(nh_part(o[2].flags) == 1 && o[2].as[0] == 0 && o[2].h == nh_crc32("GameRomHorseSaddle_00"));
    CHECK(o[0].coat == 0 && o[1].coat == 0 && o[2].coat == 0);  // (s291 horsefix: no coat set = 0, as an older build sends)
    // (s291 horsefix) the coat byte rides in a horse record's last AS byte: it comes back, the AS name stays whole; a mover's is not sent
    r[1].coat = nh_coat_byte(NHC_TOUCHY, 17), r[0].coat = 99;
    n = net_pack_objs(3, 9, r, 3, b, (int)sizeof b);
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == 3);
    CHECK(o[1].coat == r[1].coat && !strcmp(o[1].as, "Move_Gear4") && o[0].coat == 0 && !strcmp(o[0].as, "Move_Up"));
    CHECK(b[NET_HDR + 1 + NET_OBJS_REC + 30 + NET_OBJS_AS - 1] == r[1].coat && b[NET_HDR + 1 + 30 + NET_OBJS_AS - 1] == 0);
    // an older build's horse record (byte 0 there) -> coat 0; a 27-letter AS name still ends before the coat byte
    b[NET_HDR + 1 + NET_OBJS_REC + 30 + NET_OBJS_AS - 1] = 0;
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && o[1].coat == 0 && !strcmp(o[1].as, "Move_Gear4"));
    snprintf(r[1].as, sizeof r[1].as, "%s", "Horse_Link_Collar_Shader_Im");
    n = net_pack_objs(3, 9, r, 3, b, (int)sizeof b);
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && strlen(o[1].as) == 27 && o[1].coat == r[1].coat);
    r[0].coat = r[1].coat = 0;
    snprintf(r[1].as, sizeof r[1].as, "Move_Gear4");
    // a 27-letter name fits, a longer one is cut at 27
    snprintf(r[0].as, sizeof r[0].as, "%s", "WeakPointAll_Clear_ForTimeLine");
    n = net_pack_objs(3, 10, r, 1, b, (int)sizeof b);
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == 1 && strlen(o[0].as) == 27 && !strncmp(o[0].as, "WeakPointAll_Clear_ForTimeL", 27));
    // a bad letter in the name: the name goes, the pose stays
    n = net_pack_objs(3, 11, r, 1, b, (int)sizeof b);
    b[NET_HDR + 1 + 30 + 2] = 0x07;
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == 1 && o[0].as[0] == 0 && o[0].h == 0x55aa1234u);
    // a quaternion longer than 1, a NaN, a huge place, kind 3 with part 12: dropped
    n = net_pack_objs(3, 12, r, 1, b, (int)sizeof b);
    net_w32f(b + NET_HDR + 1 + 6, 0.9f), net_w32f(b + NET_HDR + 1 + 10, 0.9f);
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == 0);
    n = net_pack_objs(3, 13, r, 1, b, (int)sizeof b);
    net_w32f(b + NET_HDR + 1 + 14, NAN);
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == 0);
    n = net_pack_objs(3, 14, r, 1, b, (int)sizeof b);
    net_w32f(b + NET_HDR + 1 + 22, 3e6f);
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == 0);
    r[1].flags = NORH_PART;
    n = net_pack_objs(3, 15, &r[1], 1, b, (int)sizeof b);
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == 0);
    // a kind-1 record keeps its full matrix and extra beside pose records
    NetObjRec mix[2];
    memset(mix, 0, sizeof mix);
    mix[0].kind = NOR_OBJ, mix[0].h = 7, mix[0].extra = 0x01020304u;
    rot_m(mix[0].m, 1, 0, 0, 0.5, 0, 0, 0);
    mix[0].m[3] = 4.5f, mix[0].m[7] = 1.5f, mix[0].m[11] = 33;
    mix[1] = r[0], mix[1].as[0] = 'W', mix[1].as[1] = 0;
    n = net_pack_objs(3, 16, mix, 2, b, (int)sizeof b);
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == 2 && o[0].extra == 0x01020304u && o[0].m[3] == 4.5f && o[0].m[1] == mix[0].m[1] && !strcmp(o[1].as, "W"));
}

// (s290 rv3fix, review 3 #3) a kind-1 record's rotation part must be a rotation; its place near our copy
static void test_rot_near(void) {
    float m[12];
    rot_m(m, 0.3, 1, -0.2, 1.1, 10, 20, 30);
    CHECK(net_rot_ok(m));
    rot_m(m, 0, 0, 1, 0, 5, 6, 7);  // (identity)
    CHECK(net_rot_ok(m));
    float z[12] = {0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 3};  // (case A: all rotation rows 0)
    CHECK(!net_rot_ok(z));
    rot_m(m, 0, 1, 0, 0.4, 0, 0, 0);
    float t[12];
    memcpy(t, m, sizeof t), t[0] *= 1.2f, t[1] *= 1.2f, t[2] *= 1.2f;  // (a row 20% long)
    CHECK(!net_rot_ok(t));
    memcpy(t, m, sizeof t), t[0] *= 1.05f, t[1] *= 1.05f, t[2] *= 1.05f;  // (5% = rounding / slight scale: kept)
    CHECK(net_rot_ok(t));
    memcpy(t, m, sizeof t), t[4] = t[0], t[5] = t[1], t[6] = t[2];  // (two equal rows: not orthogonal, det 0)
    CHECK(!net_rot_ok(t));
    memcpy(t, m, sizeof t), t[8] = -t[8], t[9] = -t[9], t[10] = -t[10];  // (a mirror: det -1)
    CHECK(!net_rot_ok(t));
    memcpy(t, m, sizeof t), t[1] += 0.3f;  // (a shear: rows no longer orthogonal)
    CHECK(!net_rot_ok(t));
    memcpy(t, m, sizeof t), t[5] = NAN;
    CHECK(!net_rot_ok(t));
    // on the wire: a bad rotation is dropped alone, the good one beside it kept; a record with a good rotation round-trips
    uint8_t b[NET_PKT_MAX + 16], pl;
    uint16_t sq;
    NetObjRec r[3], o[3];
    memset(r, 0, sizeof r);
    for (int i = 0; i < 3; i++) r[i].kind = NOR_OBJ, r[i].h = 100u + (uint32_t)i, rot_m(r[i].m, 0, 1, 0, 0.5 * i, 1.f * i, 2, 3);
    memset(r[1].m, 0, 36);
    r[1].m[3] = 1, r[1].m[7] = 2, r[1].m[11] = 3;
    int n = net_pack_objs(4, 9, r, 3, b, (int)sizeof b), no = 0;
    CHECK(net_unpack_objs(b, n, &pl, &sq, o, &no) && no == 2 && o[0].h == 100 && o[1].h == 102);
    // near our copy: 10 m, NaN refused
    float ref[3] = {0, 0, 0}, w[12];
    rot_m(w, 0, 1, 0, 0, 1, 2, 3);
    CHECK(no_rec_near(ref, w));  // (3.7 m: a push)
    w[3] = 9.0f, w[7] = 0, w[11] = 0;
    CHECK(no_rec_near(ref, w));
    w[3] = 10.5f;
    CHECK(!no_rec_near(ref, w));  // (10.5 m: not our object / a bad sender)
    w[3] = 10000.0f;
    CHECK(!no_rec_near(ref, w));  // (case B: 10 km)
    w[3] = NAN;
    CHECK(!no_rec_near(ref, w));
    w[3] = 0, w[7] = 7.5f, w[11] = 7.5f;  // (a fall: 10.6 m in 3-D)
    CHECK(!no_rec_near(ref, w));
    w[7] = 5, w[11] = 5;  // (7.1 m: a real fall of a few records, still taken)
    CHECK(no_rec_near(ref, w));
}

// (s290 revfix F3) the review's boulder: he rolled it 30 m while we were far; our copy stands at its old place; his next push must
// be taken (its first record within NO_FAR of ours), and a jump inside his stream is still refused
static void test_rec_take(void) {
    NoRxNear s;
    memset(&s, 0, sizeof s);
    float ours[3] = {100, 50, 200}, w[12];
    bool st = true;
    rot_m(w, 0, 1, 0, 0, 130, 50, 200);  // (his push begins 30 m from our copy)
    CHECK(!no_rec_near(ours, w));                         // (the old rule: refused, and for good)
    CHECK(no_rec_take(&s, 10000, ours, w, &st) && !st);   // (new: the first record of his stream, 30 m < NO_FAR: taken)
    w[3] = 131.0f;
    CHECK(no_rec_take(&s, 10050, ours, w, &st) && st);    // (his stream goes on, 1 m on from his last: taken although 31 m from ours)
    for (int k = 0; k < 20; k++) {  // (a roll: 2 m a record)
        w[3] += 2.0f;
        CHECK(no_rec_take(&s, 10100 + 50 * (uint64_t)k, ours, w, NULL));
    }
    float x = w[3];
    w[3] = x + 15.0f;
    CHECK(!no_rec_take(&s, 11100, ours, w, &st) && st);  // (a 15 m jump inside his stream: refused)
    w[3] = x + 1.0f;
    CHECK(no_rec_take(&s, 11150, ours, w, NULL));         // (his next real one: taken, from his last that passed)
    // refusals do not keep the stream going: after NO_GAP_MS a record is a first record again (NO_FAR from ours)
    rot_m(w, 0, 1, 0, 0, 100, 50, 250);  // (a reset of his: 50 m from his last, 50 m from ours)
    CHECK(!no_rec_take(&s, 11200, ours, w, &st) && st);
    CHECK(!no_rec_take(&s, 11400, ours, w, &st) && st);
    CHECK(!no_rec_take(&s, 11650, ours, w, &st) && st);   // (500 ms after his last that passed: still his stream)
    CHECK(no_rec_take(&s, 11651, ours, w, &st) && !st);   // (501 ms: a first record, 50 m from ours: taken)
    // a stream's first record farther than NO_FAR from ours, or ours unreadable: refused
    memset(&s, 0, sizeof s);
    rot_m(w, 0, 1, 0, 0, 100, 50, 301);  // (101 m)
    CHECK(!no_rec_take(&s, 20000, ours, w, &st) && !st && s.at == 0);
    rot_m(w, 0, 1, 0, 0, 100, 50, 299);  // (99 m)
    CHECK(no_rec_take(&s, 20050, ours, w, NULL));
    memset(&s, 0, sizeof s);
    CHECK(!no_rec_take(&s, 20100, NULL, w, NULL));
    w[3] = NAN;
    CHECK(!no_rec_take(&s, 20150, ours, w, NULL));
    // now 0 is still a time (at = 1)
    memset(&s, 0, sizeof s);
    rot_m(w, 0, 1, 0, 0, 100, 50, 200);
    CHECK(no_rec_take(&s, 0, ours, w, NULL) && s.at == 1);
}

// (s290 rv3fix, review 3 #4) NET_OBJS' seq per sender slot: older ones dropped per record, but a REST record of an object the
// newer packet did not hold is still his last word of it
static void test_rx_seq(void) {
    NoRx rx;
    memset(&rx, 0, sizeof rx);
    bool f = no_rx_begin(&rx, 1000, 10);  // (first packet from this slot)
    CHECK(f && no_rx_rec(&rx, 0xA, 10, f) && no_rx_rec(&rx, 0xB, 10, f));
    f = no_rx_begin(&rx, 1000, 11);  // (newer: A moves again)
    CHECK(f && no_rx_rec(&rx, 0xA, 11, f));
    f = no_rx_begin(&rx, 1000, 10);  // (the same / older packet again, late: both its objects: A heard newer, B not)
    CHECK(!f && !no_rx_rec(&rx, 0xA, 10, f) && !no_rx_rec(&rx, 0xB, 10, f));  // (a duplicate of packet 10: B was in it already)
    // the review's case: his REST record of a barrel (seq 21) comes before the moving record of the tick before (seq 20)
    f = no_rx_begin(&rx, 1000, 21);
    CHECK(f && no_rx_rec(&rx, 0xC, 21, f));  // (REST of C)
    f = no_rx_begin(&rx, 1000, 20);
    CHECK(!f && !no_rx_rec(&rx, 0xC, 20, f));  // (the older moving record of C: dropped, so no put_wait cancel / his_m step back)
    CHECK(no_rx_rec(&rx, 0xD, 20, f));          // (D was not in the newer packet: its record of seq 20 is still his last word: taken)
    CHECK(!no_rx_rec(&rx, 0xD, 19, f));         // (an even older one of D: dropped now)
    // wrap: 65535 then 1 is newer; an old 65530 is not
    memset(&rx, 0, sizeof rx);
    f = no_rx_begin(&rx, 5, 65535);
    CHECK(f && no_rx_rec(&rx, 0xA, 65535, f));
    f = no_rx_begin(&rx, 5, 1);
    CHECK(f && no_rx_rec(&rx, 0xA, 1, f));
    f = no_rx_begin(&rx, 5, 65530);
    CHECK(!f && !no_rx_rec(&rx, 0xA, 65530, f));
    // another peer in the slot (joined changed): starts over, a low seq is fresh
    f = no_rx_begin(&rx, 6, 3);
    CHECK(f && no_rx_rec(&rx, 0xA, 3, f));
    // a peer that restarted (seq more than 256 behind): the new base, its table starts over
    memset(&rx, 0, sizeof rx);
    f = no_rx_begin(&rx, 7, 5000);
    CHECK(f && no_rx_rec(&rx, 0xA, 5000, f));
    f = no_rx_begin(&rx, 7, 40);
    CHECK(f && no_rx_rec(&rx, 0xA, 40, f));
    f = no_rx_begin(&rx, 7, 39);
    CHECK(!f && !no_rx_rec(&rx, 0xA, 39, f));
    // the table is round-robin: past NO_RXH_N objects an old entry is forgotten (its older record is then taken: the safe side)
    memset(&rx, 0, sizeof rx);
    f = no_rx_begin(&rx, 8, 100);
    for (uint32_t h = 1; h <= NO_RXH_N + 1; h++) CHECK(no_rx_rec(&rx, h, 100, f));
    f = no_rx_begin(&rx, 8, 99);
    CHECK(!f && no_rx_rec(&rx, 1, 99, f) && !no_rx_rec(&rx, 5, 99, f));
}

static void test_movers(void) {
    // the PR's kinds: mechanisms yes; objects, enemies, far / stump / tag / lift-rock names no
    CHECK(nm_kind("DgnObj_EV_Lift_A_01") && nm_kind("Obj_Elevator_B") && nm_kind("RemainsWater_Body") && nm_kind("TwnObj_Windmill_A"));
    CHECK(nm_kind("DgnObj_RotateFloor_A") && nm_kind("DgnObj_Seesaw_A_01") && nm_kind("Obj_TreasureBox_Gate"));
    CHECK(!nm_kind("Obj_LiftRockWhite_A_01") && !nm_kind("DgnObj_StoneLift_A") && !nm_kind("FldObj_Lift_Far") && !nm_kind("Obj_Tag_Lift"));
    CHECK(!nm_kind("Enemy_Lift_Bokoblin") && !nm_kind("Npc_Remains_Fancier001") && !nm_kind("Obj_BoxWood_A") && !nm_kind("") && !nm_kind(NULL));
    CHECK(!nm_kind("GameRomHorse12") && !nm_kind("TwnObj_Tree_Stump_Door"));
    // movers and objects never share a name
    CHECK(!no_kind("DgnObj_EV_Lift_A_01") && !no_kind("Obj_Elevator_B") && no_kind("Obj_BoxWood_A"));
    // moved: 1 cm / 0.002 of a rotation entry; NaN = moved
    float a[12], b[12];
    rot_m(a, 0, 1, 0, 0.5, 10, 20, 30);
    memcpy(b, a, sizeof b);
    CHECK(!nm_moved(a, b));
    b[7] += 0.009f;
    CHECK(!nm_moved(a, b));
    b[7] += 0.002f;
    CHECK(nm_moved(a, b));
    memcpy(b, a, sizeof b), b[0] += 0.003f;
    CHECK(nm_moved(a, b));
    memcpy(b, a, sizeof b), b[5] = NAN;
    CHECK(nm_moved(a, b));
    // our Link at it: 6 m in x z, 2 m under to 6 m over its place
    float L[3] = {10 + 5.9f, 20 + 1.0f, 30};
    CHECK(nm_link_at(L, a, false));
    L[0] = 10 + 6.1f;
    CHECK(!nm_link_at(L, a, false));
    L[0] = 10, L[1] = 20 - 1.9f;
    CHECK(nm_link_at(L, a, false));
    L[1] = 20 - 2.1f;
    CHECK(!nm_link_at(L, a, false));
    L[1] = 20 + 5.9f;
    CHECK(nm_link_at(L, a, false));
    L[1] = NAN;
    CHECK(!nm_link_at(L, a, false));
    // a Divine Beast and its parts (Remains*): at it from 220 m in x z, 150 m under / over
    CHECK(nm_big("RemainsWind") && nm_big("MergedGrudge_RemainsWater_0") && !nm_big("DgnObj_EV_Lift_A_01") && !nm_big(NULL));
    L[0] = 10 + 150, L[1] = 20 - 100, L[2] = 30 + 150;            // (212 m x z, 100 m under)
    CHECK(nm_link_at(L, a, true) && !nm_link_at(L, a, false));
    L[2] = 30 + 160;                                              // (219 m)
    CHECK(nm_link_at(L, a, true));
    L[2] = 30 + 157, L[0] = 10 + 157;                             // (222 m)
    CHECK(!nm_link_at(L, a, true));
    L[0] = 10, L[2] = 30, L[1] = 20 + 151;
    CHECK(!nm_link_at(L, a, true));
    L[1] = NAN;
    CHECK(!nm_link_at(L, a, true));
    // near it: within 30 m of its place here or of where his record takes it (NaN: near, the safe side)
    rot_m(b, 0, 1, 0, 0.5, 100, 20, 30);
    L[0] = 10 + 29.9f, L[1] = 20, L[2] = 30;
    CHECK(nm_link_near(L, a, b));
    L[0] = 100 - 29.9f;
    CHECK(nm_link_near(L, a, b));                                 // (near his place: 29.9 m)
    L[0] = 55;                                                    // (45 m from both)
    CHECK(!nm_link_near(L, a, b));
    L[1] = 20 + 31;
    L[0] = 10;
    CHECK(!nm_link_near(L, a, b));                                // (31 m over its place: 3D)
    L[1] = NAN;
    CHECK(nm_link_near(L, a, b));
    // whose: ours moving and our id lower = ours; else his (and then ours stops for 700 ms)
    NmOwn o;
    memset(&o, 0, sizeof o);
    CHECK(nm_ours(&o, 1000, true) && o.quiet == 0);              // (it moves here: ours, sent)
    CHECK(!nm_take(&o, 1010, 1, 2) && o.quiet == 0);             // (his too, our id lower: ours)
    CHECK(nm_take(&o, 1020, 3, 2) && o.quiet == NM_QUIET && o.his_id == 2);  // (our id higher: his)
    CHECK(!nm_ours(&o, 1500, true));                             // (his for 700 ms: not ours to send)
    CHECK(nm_ours(&o, 1721, true) && o.theirs_ms == 0);          // (his stopped: it moves here by itself = ours)
    memset(&o, 0, sizeof o), o.quiet = NM_QUIET;
    CHECK(nm_take(&o, 5000, 1, 2));                              // (ours quiet: his, whatever the ids)
    memset(&o, 0, sizeof o);
    CHECK(nm_ours(&o, 1, true));
    int sent = 0;
    for (int i = 0; i < 40; i++) sent += nm_ours(&o, 100 + (uint64_t)i * 50, false);
    CHECK(sent == NM_QUIET - 1 && o.quiet == NM_QUIET);          // (still: sent 1.5 s more, then not)
    // his records, drawn 120 ms behind: between two = lerp / nlerp, held at the ends
    NmRing rg;
    memset(&rg, 0, sizeof rg);
    float out[12];
    CHECK(!nm_ring_at(&rg, 100, out));
    rot_m(a, 0, 1, 0, 0.0, 0, 0, 0);
    rot_m(b, 0, 1, 0, 1.0, 10, 2, -4);
    nm_ring_push(&rg, 1000, a), nm_ring_push(&rg, 1100, b);
    CHECK(nm_ring_at(&rg, 900, out) && m_err(out, a) < 1e-4);    // (before the first: the first)
    CHECK(nm_ring_at(&rg, 1200, out) && m_err(out, b) < 1e-4);   // (after the last: the last)
    float half[12];
    rot_m(half, 0, 1, 0, 0.5, 5, 1, -2);
    CHECK(nm_ring_at(&rg, 1050, out) && m_err(out, half) < 1e-3);
    for (int i = 0; i < 6; i++) nm_ring_push(&rg, 1200 + (uint64_t)i * 50, b);
    CHECK(rg.n == NM_RING && rg.t[NM_RING - 1] == 1450);
    nm_ring_push(&rg, 500, a);                                   // (a clock that went back: started over)
    CHECK(rg.n == 1 && rg.t[0] == 500);
    // one frame's step (netmoverride): at most 5 cm and 0.03 rad; a small one = there at once
    rot_m(a, 0, 1, 0, 0, 0, 0, 0);
    rot_m(b, 0, 1, 0, 0.5, 3, 0, 4);
    CHECK(nm_step(a, b, NM_RIDE_STEP, NM_RIDE_RAD, out));       // (held back: not there this frame)
    CHECK(NEAR(sqrtf(out[3] * out[3] + out[7] * out[7] + out[11] * out[11]), NM_RIDE_STEP));
    float qo[4];
    net_q_from_m(out, qo);
    CHECK(fabs(2 * acos(qo[3]) - NM_RIDE_RAD) < 1e-3);
    rot_m(b, 0, 1, 0, 0.01, 0.02f, 0, 0.01f);
    CHECK(!nm_step(a, b, NM_RIDE_STEP, NM_RIDE_RAD, out));      // (small: there at once)
    CHECK(m_err(out, b) < 1e-4);
    // near our Link: at most 0.2 m / 0.05 rad a frame; a 20 m first catch-up takes 100 frames, a lift's few cm a frame none
    rot_m(a, 0, 1, 0, 0, 0, 0, 0);
    rot_m(b, 0, 1, 0, 0, 0, 20, 0);
    float cur[12];
    memcpy(cur, a, sizeof cur);
    int frames = 0;
    while (nm_step(cur, b, NM_NEAR_STEP, NM_NEAR_RAD, out) && frames < 1000) memcpy(cur, out, sizeof cur), frames++;
    CHECK(frames >= 99 && frames <= 100 && m_err(out, b) < 1e-4);  // (99 held steps, the 100th lands: +-1 for float sums)
    rot_m(b, 0, 1, 0, 0.04, 0, 0.05f, 0);
    CHECK(!nm_step(a, b, NM_NEAR_STEP, NM_NEAR_RAD, out) && m_err(out, b) < 1e-4);
    rot_m(b, 0, 1, 0, 0.06, 0, 0.05f, 0);
    CHECK(nm_step(a, b, NM_NEAR_STEP, NM_NEAR_RAD, out));        // (turns 0.06 rad: held, 0.05 this frame)
    net_q_from_m(out, qo);
    CHECK(fabs(2 * acos(qo[3]) - NM_NEAR_RAD) < 1e-3);
    memcpy(cur, a, sizeof cur), cur[7] = NAN;                     // (a non-finite place here: his at once, never NaN out)
    CHECK(!nm_step(cur, b, NM_NEAR_STEP, NM_NEAR_RAD, out) && m_err(out, b) < 1e-6);
    memcpy(cur, a, sizeof cur), cur[0] = INFINITY;
    CHECK(!nm_step(cur, b, NM_RIDE_STEP, NM_RIDE_RAD, out) && m_err(out, b) < 1e-6);
}

static void test_horse(void) {
    // the horse one rides, its tack, the rider's AS (PR)
    CHECK(nh_horse("GameRomHorse12") && nh_horse("GameRomHorseEpona") && nh_horse("GameRomHorse00L") && nh_horse("GameROMMotorcycle"));
    CHECK(!nh_horse("GameRomHorseSaddle_00") && !nh_horse("GameRomHorseReins_05") && !nh_horse("Enemy_Lynel_Dark") && !nh_horse(NULL));
    CHECK(nh_tack("GameRomHorseSaddle_00L") == 0 && nh_tack("GameRomHorseReins_10") == 1 && nh_tack("GameRomHorse12") == -1 && nh_tack(NULL) == -1);
    CHECK(nh_riding_as("Horse_Wait") && nh_riding_as("Motorcycle_Run") && !nh_riding_as("Wait") && !nh_riding_as(NULL));
    // zlib's crc32 (python: zlib.crc32(b"GameRomHorse12") and the check value of "123456789")
    CHECK(nh_crc32("123456789") == 0xCBF43926u && nh_crc32("") == 0 && nh_crc32("a") == 0xE8B7BE43u);
    CHECK(nh_part(NORH_HORSE) == 0 && nh_part(NORH_SADDLE) == 1 && nh_part(NORH_REINS) == 2 && nh_part(NORH_PART) == -1);
    // (s292 coopfix) his horse records go with his slot: free (lab s291d F6: he left) or someone else's; kept while he is there;
    // nothing kept = nothing to forget
    CHECK(nh_rx_stale(true, 2, false, 0) && nh_rx_stale(true, 2, false, 2) && nh_rx_stale(true, 2, true, 3));
    CHECK(!nh_rx_stale(true, 2, true, 2) && !nh_rx_stale(false, 0, false, 0) && !nh_rx_stale(false, 0, true, 5));
    // (s292 coopfix, nethorsefar) the distance gate: off at N 0 / below; far past N, near again only under 0.8 N; unknown = as it was
    CHECK(!nh_far(false, 1840, 0) && !nh_far(true, 1840, 0) && !nh_far(true, 1840, -5));
    CHECK(nh_far(false, 1840, 250) && nh_far(false, 330, 250) && !nh_far(false, 250, 250) && !nh_far(false, 40, 250));
    CHECK(nh_far(true, 240, 250) && nh_far(true, 200, 250) && !nh_far(true, 199, 250) && !nh_far(true, 10, 250));
    CHECK(nh_far(true, NAN, 250) && !nh_far(false, NAN, 250));
    // the wait after a removal: 2 s, doubling, at most 30 s; none before the first
    CHECK(nh_rm_wait_ms(0) == 0 && nh_rm_wait_ms(-1) == 0 && nh_rm_wait_ms(1) == 2000 && nh_rm_wait_ms(2) == 4000 && nh_rm_wait_ms(4) == 16000);
    CHECK(nh_rm_wait_ms(5) == 30000 && nh_rm_wait_ms(9) == 30000 && nh_rm_wait_ms(255) == 30000);
    {   // the lab s292a loop (made, removed by the game about a second later, again and again) for 120 s: asks before 96 times,
        // now a handful; one that then lives 10 s starts the count over
        uint64_t t = 0, until = 0, alive_since = 0;
        int n = 0, asks = 0;
        for (; t < 120000; t += 1000) {
            if (t < until) continue;
            asks++;                                   // (made)
            n++, until = t + 1000 + nh_rm_wait_ms(n);  // (removed a second later)
        }
        CHECK(asks >= 4 && asks <= 8);
        alive_since = t;
        if (t + NH_RM_STABLE_MS - alive_since >= NH_RM_STABLE_MS) n = 0;
        CHECK(n == 0 && nh_rm_wait_ms(n + 1) == NH_RM_WAIT_MS);
    }
    // horse.txt: one line each, CR LF or LF, spaces at the end, junk lines skipped
    NhMap m;
    const char *txt = "GameRomHorse12=OnlineHorse_GameRomHorse12\r\nGameRomHorseSaddle_00=OnlineHorse_GameRomHorseSaddle_00  \n"
                      "junk line\n=OnlineHorse_x\nGameROMMotorcycle=\nGameROMMotorcycle=OnlineHorse_GameROMMotorcycle";
    CHECK(nh_map_parse(txt, &m) == 3);
    CHECK(nh_map_find(&m, nh_crc32("GameRomHorse12")) && !strcmp(nh_map_find(&m, nh_crc32("GameRomHorse12")), "OnlineHorse_GameRomHorse12"));
    CHECK(!strcmp(nh_map_find(&m, nh_crc32("GameRomHorseSaddle_00")), "OnlineHorse_GameRomHorseSaddle_00"));
    CHECK(!strcmp(nh_map_find(&m, nh_crc32("GameROMMotorcycle")), "OnlineHorse_GameROMMotorcycle"));
    CHECK(!nh_map_find(&m, nh_crc32("GameRomHorse13")) && !nh_map_find(NULL, 1) && nh_map_parse("", &m) == 0 && nh_map_parse(NULL, &m) == 0);
    // (s291 horsefix) the coat: slot-4 AS -> family (ASList name or its file name), byte <-> the bake's Coat_<f>_<nn>
    CHECK(nh_coat_family("Collar_Shader") == NHC_DOCILE && nh_coat_family("Horse_Link_Collar_Shader") == NHC_DOCILE);
    CHECK(nh_coat_family("Collar_Shader_Coward") == NHC_TOUCHY && nh_coat_family("Horse_Link_Collar_Shader_Impatient") == NHC_TOUCHY);
    CHECK(nh_coat_family("Collar_Shader_EponaHorse") == NHC_EPONA && nh_coat_family("Collar_Shader_ZeldaHorse") == NHC_ZELDA);
    CHECK(nh_coat_family("Move_Gear4") == -1 && nh_coat_family("") == -1 && nh_coat_family(NULL) == -1 && nh_coat_family("Collar_Shader_X") == -1);
    CHECK(nh_coat_byte(NHC_DOCILE, 0) == 1 && nh_coat_byte(NHC_ZELDA, 31) == NHC_N * NH_COAT_FRAMES && nh_coat_byte(NHC_TOUCHY, 7) == 1 + 32 + 7);
    CHECK(nh_coat_byte(-1, 3) == 0 && nh_coat_byte(NHC_N, 3) == 0 && nh_coat_byte(NHC_DOCILE, -1) == 0 && nh_coat_byte(NHC_DOCILE, NH_COAT_FRAMES) == 0);
    char cn[16];
    CHECK(nh_coat_name(nh_coat_byte(NHC_DOCILE, 7), cn) && !strcmp(cn, "Coat_D_07"));
    CHECK(nh_coat_name(nh_coat_byte(NHC_TOUCHY, 30), cn) && !strcmp(cn, "Coat_T_30"));
    CHECK(nh_coat_name(nh_coat_byte(NHC_EPONA, 0), cn) && !strcmp(cn, "Coat_E_00"));
    CHECK(nh_coat_name(nh_coat_byte(NHC_ZELDA, 31), cn) && !strcmp(cn, "Coat_Z_31"));
    CHECK(!nh_coat_name(0, cn) && cn[0] == 0 && !nh_coat_name(NHC_N * NH_COAT_FRAMES + 1, cn) && cn[0] == 0 && !nh_coat_name(255, cn));
    int round = 0;
    for (int f = 0; f < NHC_N; f++)
        for (int fr = 0; fr < NH_COAT_FRAMES; fr++) {
            uint8_t c = nh_coat_byte(f, fr);
            round += c && nh_coat_name(c, cn) && cn[5] == NHC_LETTER[f] && (cn[7] - '0') * 10 + (cn[8] - '0') == fr;
        }
    CHECK(round == NHC_N * NH_COAT_FRAMES);
}

// (s291 coat) the AS-slot calls: (mgr, TARGET, SLOT) / (action, name, w2, TARGET, SLOT), as BotW's code (the Horse's Collar AS:
// target 0, slot 4); the horsefix order (slot as the target) is what the s291e lab could not read / play. And the diag line.
static void test_as_slots(void) {
    uint64_t x[5];
    memset(x, 0xee, sizeof x);
    no_as_in_slot_args(x, 0x7012345678ull, 0, NH_COAT_SLOT);
    CHECK(x[0] == 0x7012345678ull && x[1] == 0 && x[2] == 4 && x[3] == 0xeeeeeeeeeeeeeeeeull);  // (the Horse: 0x710149f24c w2 0, w3 4)
    no_as_in_slot_args(x, 1, 0, 0);
    CHECK(x[1] == 0 && x[2] == 0);  // (slot 0 = the s289 call: x1 0, x2 0)
    no_as_in_slot_args(x, 1, 1, 2);
    CHECK(x[1] == 1 && x[2] == 2);
    memset(x, 0xee, sizeof x);
    no_play_as_args(x, 0x10, 0x20, true, 0, NH_COAT_SLOT);
    CHECK(x[0] == 0x10 && x[1] == 0x20 && x[2] == 0 && x[3] == 0 && x[4] == 4);  // (w3 = target 0, w4 = slot 4)
    no_play_as_args(x, 0x10, 0x20, false, 0, 0);
    CHECK(x[2] == 1 && x[3] == 0 && x[4] == 0);  // (slot 0, not again = the s289 call: x2 1, x3 0, x4 0)
    no_play_as_args(x, 0x10, 0x20, false, 0, -1);
    CHECK(x[4] == 0xffffffffull);  // (a 32-bit w4, as the callee reads)

    static char nm[NO_AS_TARGETS_MAX][NO_AS_SLOTS_MAX][NET_OBJS_AS];
    memset(nm, 0, sizeof nm);
    snprintf(nm[0][0], NET_OBJS_AS, "Wait_Search"), snprintf(nm[0][4], NET_OBJS_AS, "Collar_Shader"), snprintf(nm[1][0], NET_OBJS_AS, "Move_Gear1");
    int ns[NO_AS_TARGETS_MAX] = {6, 3};
    char line[700];
    nh_slots_line(line, sizeof line, 2, ns, (const char (*)[NO_AS_SLOTS_MAX][NET_OBJS_AS])nm);
    CHECK(!strcmp(line, "t0 (6): 0 Wait_Search, 1 -, 2 -, 3 -, 4 Collar_Shader, 5 -; t1 (3): 0 Move_Gear1, 1 -, 2 -"));
    nh_slots_line(line, sizeof line, 0, ns, (const char (*)[NO_AS_SLOTS_MAX][NET_OBJS_AS])nm);
    CHECK(!strcmp(line, "no AnmTarget"));
    int big[NO_AS_TARGETS_MAX] = {99, -3, 0, 1};
    nh_slots_line(line, sizeof line, 7, big, (const char (*)[NO_AS_SLOTS_MAX][NET_OBJS_AS])nm);
    CHECK(!strncmp(line, "t0 (99, read 8): 0 Wait_Search,", 31) && strstr(line, "; t1 (-3, read 0):; t2 (0):; t3 (1): 0 -; (7 targets, read 4)"));
    char tiny[12];
    nh_slots_line(tiny, sizeof tiny, 2, ns, (const char (*)[NO_AS_SLOTS_MAX][NET_OBJS_AS])nm);
    CHECK(strlen(tiny) == sizeof tiny - 1 && !strncmp(tiny, "t0 (6): 0 W", 11));  // (cut, ends in 0, no overrun)
    CHECK(nh_slots_line(tiny, 0, 2, ns, (const char (*)[NO_AS_SLOTS_MAX][NET_OBJS_AS])nm) == tiny);
}

int main(void) {
    test_bump();
    test_kind();
    test_owner();
    test_break();
    test_followed();
    test_world();
    test_wire();
    test_pose_wire();
    test_rot_near();
    test_rec_take();
    test_rx_seq();
    test_movers();
    test_horse();
    test_as_slots();
    printf("net_objs_test: %d checks, %d failed: %s\n", runs, fails, fails ? "FAIL" : "all pass");
    return fails != 0;
}

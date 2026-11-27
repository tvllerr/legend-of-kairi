// (s289 revive) host test of runtime/sora_revive_core.h (co-op item 7: the DOWNED state's pure rules): the arm rule, the KO owner
// (one owner per KO, Mickey first), the timer, the radius count, the give-up hold, the ends, doomed, the heal.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/revive_test.c -o build_h/revive_test.exe
#include <stdio.h>
#include <string.h>
#include "sora_revive_core.h"

static int pass, fail;
#define CHECK(c) do { if (c) pass++; else { fail++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static const RvCfg CFG = {2.0f, 3.0f, 30.0f};  // the user's picks: 2 m, 3 s, 30 s
static RvIn in_(float dt, int life, int nlive, float near_m, bool gu) {
    RvIn i = {dt, life, nlive, near_m, gu, false, true};
    return i;
}
// steps of dt until an event or t seconds; returns the event, *took = the seconds stepped
static int run(RvCore *c, RvIn i, float t, float *took) {
    float s = 0;
    int e = RVE_NONE;
    while (s < t - 1e-4f && !e) e = rvc_step(c, &CFG, &i), s += i.dt;
    if (took) *took = s;
    return e;
}
static RvCore down_core(void) {
    RvCore c;
    memset(&c, 0, sizeof c);
    rvc_go_down(&c);
    return c;
}

int main(void) {
    // ---- the arm rule ----
    CHECK(rvc_can_arm(true, 12, 0, false, 1));
    CHECK(!rvc_can_arm(false, 12, 0, false, 1));  // revive 0
    CHECK(!rvc_can_arm(true, 0, 0, false, 1));    // dead
    CHECK(!rvc_can_arm(true, -1, 0, false, 1));
    CHECK(!rvc_can_arm(true, 12, 4, false, 1));   // Mickey armed: his KO (Mickey first)
    CHECK(!rvc_can_arm(true, 12, 0, true, 1));    // a duel
    CHECK(!rvc_can_arm(true, 12, 0, false, 0));   // no live friend = today's death
    CHECK(rvc_can_arm(true, 1, 0, false, 3));

    // ---- the mask + the owner, one owner per KO ----
    RvCore c;
    memset(&c, 0, sizeof c);
    CHECK(rvc_mask(&c, false) == 0 && !rvc_ko_mine(&c));  // nobody: never revive's
    CHECK(rvc_mask(&c, true) == 4);                       // frame 1 armed ...
    CHECK(!rvc_ko_mine(&c));                              // ... a count change seen now came from frame 0's mask (not ours)
    CHECK(rvc_mask(&c, true) == 4 && rvc_ko_mine(&c));    // frame 2: frame 1's mask was ours
    CHECK(rvc_mask(&c, false) == 0 && rvc_ko_mine(&c));   // frame 3: Mickey armed now, but the hit landed under frame 2's mask = ours
    CHECK(rvc_mask(&c, false) == 0 && !rvc_ko_mine(&c));  // frame 4: frame 3's clamp was Mickey's = his KO
    rvc_go_down(&c);
    CHECK(rvc_ko_mine(&c));                               // DOWNED: every clamp change is revive's (a fall while down: no Mickey screen)
    CHECK(rvc_mask(&c, false) == 4 && rvc_mask(&c, false) == 4);  // DOWNED holds the clamp whatever the arm says
    CHECK(rvc_drops_hits(&c));
    // revive 0 forever: mask 0, never the owner (Mickey's flow untouched)
    memset(&c, 0, sizeof c);
    for (int f = 0; f < 10; f++) CHECK(rvc_mask(&c, rvc_can_arm(false, 12, 0, false, 1)) == 0 && !rvc_ko_mine(&c) && !rvc_drops_hits(&c));
    // revive 1, no friend: the same
    for (int f = 0; f < 10; f++) CHECK(rvc_mask(&c, rvc_can_arm(true, 12, 0, false, 0)) == 0 && !rvc_ko_mine(&c) && !rvc_drops_hits(&c));

    // ---- the timer ----
    float took = 0;
    c = down_core();
    CHECK(run(&c, in_(0.1f, 1, 1, 10.0f, false), 29.8f, &took) == RVE_NONE && c.down);
    int e = run(&c, in_(0.1f, 1, 1, 10.0f, false), 1.0f, &took);
    CHECK(e == RVE_TIMEOUT && !c.down && c.doomed && rve_gameover(e));
    CHECK(c.t_down >= 30.0f - 1e-3f && c.t_down < 30.1f);
    CHECK(rvc_mask(&c, true) == 0);                        // doomed: no clamp, the next hit kills
    rvc_undoom(&c, 1, false);
    CHECK(c.doomed);
    rvc_undoom(&c, 0, false);
    CHECK(c.doomed);
    rvc_undoom(&c, 2, false);                              // healed
    CHECK(!c.doomed && rvc_mask(&c, true) == 4);
    c.doomed = true, rvc_undoom(&c, 0, true);              // the Game Over's reload
    CHECK(!c.doomed);
    // a hitch is not seconds of waiting: one 5 s frame counts RV_DT_MAX
    c = down_core();
    RvIn h = in_(5.0f, 1, 1, 10.0f, false);
    CHECK(rvc_step(&c, &CFG, &h) == RVE_NONE && c.t_down > 0.19f && c.t_down < 0.21f);
    h.dt = -1.0f;
    CHECK(rvc_step(&c, &CFG, &h) == RVE_NONE && c.t_down < 0.21f);
    h.dt = 0.0f / 0.0f;  // (NaN)
    CHECK(rvc_step(&c, &CFG, &h) == RVE_NONE && c.t_down < 0.21f);

    // ---- the radius count ----
    c = down_core();  // (1/8 s frames: exact sums)
    CHECK(run(&c, in_(0.125f, 1, 1, 1.5f, false), 2.875f, &took) == RVE_NONE && c.t_near == 2.875f);
    e = run(&c, in_(0.125f, 1, 1, 1.5f, false), 1.0f, &took);
    CHECK(e == RVE_REVIVED && !c.down && !c.doomed && c.grace == RV_GRACE_S && rvc_drops_hits(&c) && took == 0.125f);
    // continuous: 2 s near, one frame away, 2 s near = no revive; 1 s more = revive
    c = down_core();
    CHECK(run(&c, in_(0.1f, 1, 1, 1.0f, false), 2.0f, NULL) == RVE_NONE);
    CHECK(run(&c, in_(0.1f, 1, 1, 2.5f, false), 0.1f, NULL) == RVE_NONE && c.t_near == 0);
    CHECK(run(&c, in_(0.1f, 1, 1, 1.0f, false), 2.0f, NULL) == RVE_NONE && c.down);
    CHECK(run(&c, in_(0.1f, 1, 1, 1.0f, false), 1.05f, NULL) == RVE_REVIVED);
    // the edge: exactly revivem counts, a bit more does not; no pose (-1) does not
    c = down_core();
    CHECK(run(&c, in_(0.1f, 1, 1, 2.0f, false), 3.1f, NULL) == RVE_REVIVED);
    c = down_core();
    CHECK(run(&c, in_(0.1f, 1, 1, 2.01f, false), 5.0f, NULL) == RVE_NONE && c.t_near == 0);
    CHECK(run(&c, in_(0.1f, 1, 1, -1.0f, false), 5.0f, NULL) == RVE_NONE && c.t_near == 0);
    // the near friend's distance with no live friend counted (he went down / loading): no count
    c = down_core();
    CHECK(run(&c, in_(0.1f, 1, 0, 1.0f, false), 0.5f, NULL) == RVE_NONE && c.t_near == 0);
    // revived and timed out in the same frame: the revive wins; nobody near in that frame = the timeout
    {
        RvIn j = in_(0.125f, 1, 1, 1.0f, false);
        c = down_core(), c.t_down = 29.9f, c.t_near = 2.9f;
        CHECK(rvc_step(&c, &CFG, &j) == RVE_REVIVED);
        j.near_m = 9.0f;
        c = down_core(), c.t_down = 29.9f, c.t_near = 2.9f;
        CHECK(rvc_step(&c, &CFG, &j) == RVE_TIMEOUT && c.t_near == 0);
    }
    // other numbers (live keys): 5 m / 1 s
    {
        RvCfg g = {5.0f, 1.0f, 30.0f};
        c = down_core();
        RvIn i = in_(0.1f, 1, 1, 4.5f, false);
        int n = 0;
        while (!(e = rvc_step(&c, &g, &i)) && n < 100) n++;
        CHECK(e == RVE_REVIVED && n >= 9 && n <= 10);
    }

    // ---- the give-up hold ----
    c = down_core();
    CHECK(run(&c, in_(0.1f, 1, 1, 10.0f, true), 5.0f, NULL) == RVE_NONE && !c.gu_ready && c.t_gu == 0);  // held since the KO: not a give-up
    CHECK(run(&c, in_(0.1f, 1, 1, 10.0f, false), 0.1f, NULL) == RVE_NONE && c.gu_ready);                // let go once
    CHECK(run(&c, in_(0.1f, 1, 1, 10.0f, true), 1.4f, NULL) == RVE_NONE && c.t_gu > 1.3f);              // 1.4 s: not yet
    CHECK(run(&c, in_(0.1f, 1, 1, 10.0f, false), 0.1f, NULL) == RVE_NONE && c.t_gu == 0);               // let go: back at 0
    e = run(&c, in_(0.1f, 1, 1, 10.0f, true), 2.0f, &took);
    CHECK(e == RVE_GIVEUP && took > 1.45f && took < 1.65f && c.doomed && !c.down);
    // give-up and revive in the same frame: the player's own choice wins
    c = down_core(), c.gu_ready = true, c.t_gu = 1.45f, c.t_near = 2.95f;
    CHECK(run(&c, in_(0.125f, 1, 1, 1.0f, true), 0.125f, NULL) == RVE_GIVEUP);
    // the lab key
    c = down_core();
    CHECK(rvc_giveup_now(&c) == RVE_GIVEUP && c.doomed && !c.down);
    CHECK(rvc_giveup_now(&c) == RVE_NONE);

    // ---- no live friend left ----
    c = down_core();
    CHECK(run(&c, in_(0.1f, 1, 0, -1.0f, false), 0.9f, NULL) == RVE_NONE && c.t_nofriend > 0.8f);  // (one SNAP gap / an area edge)
    CHECK(run(&c, in_(0.1f, 1, 1, 20.0f, false), 0.1f, NULL) == RVE_NONE && c.t_nofriend == 0);   // back
    e = run(&c, in_(0.1f, 1, 0, -1.0f, false), 2.0f, &took);
    CHECK(e == RVE_NOFRIEND && took > 0.95f && took < 1.15f && c.doomed && rve_gameover(e));

    // ---- the other ends ----
    c = down_core();
    RvIn i = in_(0.1f, 2, 1, 10.0f, false);  // (food / a potion / a fairy / God Mode)
    CHECK(rvc_step(&c, &CFG, &i) == RVE_HEALED && !c.down && !c.doomed && c.grace == RV_GRACE_S);
    c = down_core();
    i = in_(0.1f, 0, 1, 10.0f, false);
    CHECK(rvc_step(&c, &CFG, &i) == RVE_DIED && !c.down && !c.doomed && !rvc_drops_hits(&c));
    c = down_core();
    i = in_(0.1f, 1, 1, 10.0f, false), i.load = true;  // (fast travel at 1 quarter heart: the Game Over anyway)
    CHECK(rvc_step(&c, &CFG, &i) == RVE_LOAD_KILL && c.doomed && rve_gameover(RVE_LOAD_KILL));
    c = down_core();
    i = in_(0.1f, 9, 1, 10.0f, false), i.load = true;  // (a save load: its own life)
    CHECK(rvc_step(&c, &CFG, &i) == RVE_LOAD_UP && !c.doomed && !c.down && c.grace == 0);
    c = down_core();
    i = in_(0.1f, 1, 1, 1.0f, false), i.on = false;
    CHECK(rvc_step(&c, &CFG, &i) == RVE_OFF && !c.down && !c.doomed && !rvc_drops_hits(&c));
    CHECK(!rve_gameover(RVE_REVIVED) && !rve_gameover(RVE_HEALED) && !rve_gameover(RVE_DIED) && !rve_gameover(RVE_LOAD_UP) && !rve_gameover(RVE_OFF));
    for (int k = 0; k < RVE_N; k++) CHECK(RVE_NAME[k] && RVE_NAME[k][0]);

    // ---- the grace: 3 s of dropped hits after getting up, counted while not down ----
    c = down_core();
    CHECK(run(&c, in_(0.1f, 1, 1, 1.0f, false), 3.1f, NULL) == RVE_REVIVED);
    CHECK(run(&c, in_(0.1f, 7, 1, 1.0f, false), 2.9f, NULL) == RVE_NONE && rvc_drops_hits(&c));
    CHECK(run(&c, in_(0.1f, 7, 1, 1.0f, false), 0.2f, NULL) == RVE_NONE && !rvc_drops_hits(&c) && c.grace == 0);
    CHECK(!c.down && rvc_mask(&c, true) == 4);  // (armed again for the next KO)

    // ---- taking a KO Mickey gave up ("It's all over..." / a lost roll) or the forced one ----
    memset(&c, 0, sizeof c);
    CHECK(rvc_can_take(&c, true, 1, false, 1));
    CHECK(!rvc_can_take(&c, false, 1, false, 1));
    CHECK(!rvc_can_take(&c, true, 0, false, 1));
    CHECK(!rvc_can_take(&c, true, 1, true, 1));
    CHECK(!rvc_can_take(&c, true, 1, false, 0));
    c.doomed = true;
    CHECK(!rvc_can_take(&c, true, 1, false, 1));
    c = down_core();
    CHECK(!rvc_can_take(&c, true, 1, false, 1));  // (already down)
    CHECK(c.down && !c.doomed && !c.gu_ready && c.t_down == 0 && c.t_near == 0 && c.t_gu == 0 && c.t_nofriend == 0);

    // ---- the heal: KH2's Hi-Potion half ----
    CHECK(rvc_heal_q(12, 50) == 6);
    CHECK(rvc_heal_q(40, 50) == 20);
    CHECK(rvc_heal_q(0, 50) == 6);      // (no max read yet: 3 hearts)
    CHECK(rvc_heal_q(3, 10) == 1);      // (at least 1 quarter heart)
    CHECK(rvc_heal_q(12, 100) == 12);
    CHECK(rvc_heal_q(12, 500) == 12);   // (clamped)
    CHECK(rvc_heal_q(12, -5) == 1);

    printf("revive_test: %d/%d passed\n", pass, pass + fail);
    return fail != 0;
}

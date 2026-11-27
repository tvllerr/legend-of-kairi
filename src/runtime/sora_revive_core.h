// sora_revive_core.h (s289 revive, co-op plan item 7; R1 of recon/s267/S267_HELPER_EXP_REVIVE.md 7.2): the pure rules of the
// DOWNED state, host-tested by runtime/test/revive_test.c. No Windows / game includes (runtime/sora_revive.h is the game side).
//   arm:     revive on, Link alive, Mickey's clamp mask 0 (Mickey first, KH2's own rule), no duel, >= 1 live friend in the room
//   owner:   one owner per KO. The clamp count change seen in frame N comes from the mask set in frame N-1 (the exe applies the
//            mask set before its physics step), so "mine" = last frame's mask was revive's (own_prev), or revive is DOWNED.
//   down:    the timer (revivetime) and the radius count (a live friend within revivem for revives, continuous: it starts again
//            from 0 when he steps out), the give-up hold (a fresh hold: a button already held at the KO does not count).
//   ends:    revived / healed by other means (food, a fairy, God Mode: life > 1) / life 0 / a load / revive 0, or the Game Over
//            (timeout, give up, no live friend left for RV_NOFRIEND_S): then "doomed" (mask 0) until healed or a load.
// The user's picks (s289 brief): proximity only (no button for the friend), 2 m / 3 s, half life (KH2's Hi-Potion), 30 s.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define RV_DT_MAX 0.2f       // a frame counts at most this long (a hitch or a breakpoint is not seconds of waiting)
#define RV_GIVEUP_S 1.5f     // hold the give-up button this long (GUESS)
#define RV_NOFRIEND_S 1.0f   // no live friend for this long = the Game Over (GUESS: one SNAP gap / an area edge is not)
#define RV_GRACE_S 3.0f      // after getting up: hits dropped this long (design 7.2: 3 s)

typedef struct { float radius_m, need_s, time_s; } RvCfg;  // revivem, revives, revivetime

typedef struct {
    bool down, doomed;
    bool own_now, own_prev;  // this frame's clamp mask was revive's / last frame's (the KO owner)
    bool gu_ready;           // the give-up button was seen up since the KO
    float t_down, t_near, t_gu, t_nofriend, grace;
} RvCore;

typedef struct {
    float dt;      // this world frame's seconds (capped here)
    int life;      // Link's quarter hearts
    int nlive;     // live friends: used, PEER_LIVE, same map, no NF_DEAD / NF_LOADING / NF_DOWNED in his last SNAP
    float near_m;  // the nearest live friend's feet, 3D metres (< 0 = no pose)
    bool gu_held;  // the give-up button is held (the raw pad)
    bool load;     // a load ended since the last world frame
    bool on;       // the revive key
} RvIn;

enum { RVE_NONE, RVE_REVIVED, RVE_TIMEOUT, RVE_GIVEUP, RVE_NOFRIEND, RVE_HEALED, RVE_DIED, RVE_LOAD_KILL, RVE_LOAD_UP, RVE_OFF, RVE_N };
static const char *const RVE_NAME[RVE_N] = {"none", "revived", "timeout", "gave up", "no live friend", "healed", "life 0",
                                            "load (still 1 quarter heart)", "load", "revive 0"};
static inline bool rve_gameover(int e) { return e == RVE_TIMEOUT || e == RVE_GIVEUP || e == RVE_NOFRIEND || e == RVE_LOAD_KILL; }

static inline bool rvc_can_arm(bool on, int life, int mk_mask, bool duel, int nlive) {
    return on && life > 0 && mk_mask == 0 && !duel && nlive > 0;
}
// a KO that Mickey gave up ("It's all over..." / a lost roll) or the lab's forced KO: the same rule without Mickey's mask
static inline bool rvc_can_take(const RvCore *c, bool on, int life, bool duel, int nlive) {
    return on && !c->down && !c->doomed && life > 0 && !duel && nlive > 0;
}

// once a world frame (ca_survive): the mask to OR in (4 = any fatal hit leaves 1 quarter heart) + the owner shift
static inline int rvc_mask(RvCore *c, bool can_arm) {
    int m = c->doomed ? 0 : (c->down || can_arm) ? 4 : 0;
    c->own_prev = c->own_now, c->own_now = c->down || m != 0;
    return m;
}
static inline bool rvc_ko_mine(const RvCore *c) { return c->own_prev || c->down; }
static inline bool rvc_drops_hits(const RvCore *c) { return c->down || c->grace > 0; }

static inline void rvc_go_down(RvCore *c) {
    c->down = true, c->doomed = false, c->gu_ready = false;
    c->t_down = c->t_near = c->t_gu = c->t_nofriend = c->grace = 0;
}
// healed (life > 1) or a load: armed again after a Game Over that did not kill (no BotW death followed the life write)
static inline void rvc_undoom(RvCore *c, int life, bool load) {
    if (c->doomed && (life > 1 || load)) c->doomed = false;
}
static inline int rvc_end_(RvCore *c, int e) {
    c->down = false, c->t_near = c->t_gu = c->t_nofriend = 0;
    if (rve_gameover(e)) c->doomed = true, c->grace = 0;
    else if (e == RVE_REVIVED || e == RVE_HEALED) c->grace = RV_GRACE_S;
    else c->grace = 0;
    return e;
}
static inline int rvc_giveup_now(RvCore *c) { return c->down ? rvc_end_(c, RVE_GIVEUP) : RVE_NONE; }

// once a world frame; returns the event that ended the DOWNED state this frame (RVE_NONE = none)
static inline int rvc_step(RvCore *c, const RvCfg *g, const RvIn *in) {
    float dt = !(in->dt > 0) ? 0 : in->dt > RV_DT_MAX ? RV_DT_MAX : in->dt;
    if (!c->down) {
        if (c->grace > 0 && (c->grace -= dt) < 0) c->grace = 0;
        return RVE_NONE;
    }
    if (!in->on) return rvc_end_(c, RVE_OFF);
    if (in->life <= 0) return rvc_end_(c, RVE_DIED);
    if (in->load) return rvc_end_(c, in->life <= 1 ? RVE_LOAD_KILL : RVE_LOAD_UP);
    if (in->life > 1) return rvc_end_(c, RVE_HEALED);
    c->t_down += dt;
    if (!in->gu_held) c->gu_ready = true, c->t_gu = 0;
    else if (c->gu_ready && (c->t_gu += dt) >= RV_GIVEUP_S) return rvc_end_(c, RVE_GIVEUP);
    if (in->nlive > 0 && in->near_m >= 0 && in->near_m <= g->radius_m) {
        if ((c->t_near += dt) >= g->need_s) return rvc_end_(c, RVE_REVIVED);
    } else c->t_near = 0;
    if (in->nlive <= 0) {
        if ((c->t_nofriend += dt) >= RV_NOFRIEND_S) return rvc_end_(c, RVE_NOFRIEND);
    } else c->t_nofriend = 0;
    if (c->t_down >= g->time_s) return rvc_end_(c, RVE_TIMEOUT);
    return RVE_NONE;
}

// the heal on a revive: pct of the max life (KH2's Hi-Potion = half), at least 1 quarter heart
static inline int rvc_heal_q(int life_max, int pct) {
    int mx = life_max > 0 ? life_max : 12;
    int q = mx * (pct < 1 ? 1 : pct > 100 ? 100 : pct) / 100;
    return q < 1 ? 1 : q;
}

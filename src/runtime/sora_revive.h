// sora_revive.h (s289 revive, co-op plan item 7; R1 of recon/s267/S267_HELPER_EXP_REVIVE.md 7.2-7.5): a KO with a live friend in
// the room puts Sora DOWNED instead of BotW's death; the friend standing by him revives him. DLL only (the exe exports are the
// s110 ones Mickey uses: linkpos_survive_set / _count, linkpos_guard_set; the HUD text is sora_hud_notice, as the duel's).
// The rules (arm, the KO owner, the timer, the radius count, the give-up) are pure: sora_revive_core.h, runtime/test/revive_test.c.
//
// The user's picks (s289): "Stand by you 3 s" = the friend within revivem (2 m) for revives (3 s), nothing to press; "Go down,
// friend revives" = Mickey still comes first (KH2's own rule): while mk_survive_mask() > 0 the KO is his; when his rescue fails
// ("It's all over..." or a lost roll) the KO comes here; "Half life, 30 s" = revivehp 50 % of the max life (KH2's Hi-Potion),
// Game Over after revivetime 30 s down.
//
// live keys (sora.c's live chain, after mk_live_set): set revive 0|1 (default 1 since s290; 0 = as before exactly: no mask, no flag bit, no state),
// revivelog 0|1 (the per-event lines: armed / disarmed, the friend near / away, a friend's NF_DOWNED on this side),
// revivem <m> (2), revives <s> (3), revivehp <%> (50), revivetime <s> (30); lab: revforce 1 (a forced KO now: life -> 1 quarter
// heart, DOWNED; needs a live friend, like a real KO), revgiveup 1 (give up now: an unfocused lab copy reads no pad).
// The DOWNED / revived / Game Over lines are always logged (once per KO, as Mickey's).
//
// Hooks (each one line): sora_chestabil.h ca_survive ORs rv_survive_mask; sora_mickey.h (mk_drops_hits || rv_drops_hits, the KO
// test && !rv_ko_mine, "It's all over..." / the lost roll try rv_take_ko before mc_kill_link); sora.c rv_tick after mk_tick,
// rv_input next to mc_input, rv_pose_hold next to mk_dchg_loop, NF_DOWNED in net_fill (NET.downed), rv_live_set;
// sora_net_esync.h: a DOWNED player is "away" like a dead one (esdead).
// Included from sora.c after sora_duelmenu.h (needs MK, mc_kill_link, DU / du_text, CAMP, KDG, NET, S, play, link_heal).
#pragma once
#include "sora_revive_core.h"

#define RV_HOLD_FRONT 13.0f  // A211 (KH2 blow landing, front) f13: top of the body 84 cm, hips 60 cm, the bake's lowest held pose
#define RV_HOLD_BACK 14.0f   // A214 (back) f14: top 84 cm, hips 64 cm; both not free (bits 0x4000), no frame trigger within 3 frames
#define RV_TEXT_Y 262.0f     // the two lines, KH2 virtual px of the 512 x 416 frame (GUESS look, as the duel's text)
#define RV_NQ 160
#define RV_GU_NAME (g_mkpad == 2 ? "Y" : "Triangle")  // the give-up button = Switch X (BTN_CLIMB): PS Triangle / Xbox Y, as mkpad names Mickey's

static bool g_revive = true;          // live "set revive 0|1". (s290: passed the two-copy lab, sb/s290/LAB_RESULTS.md; the user: "Flip the passes ON" -> default ON)
static bool g_revive_log;             // live "set revivelog 1"
static int g_revive_m = 2, g_revive_s = 3, g_revive_hp = 50, g_revive_time = 30;

typedef struct { int nlive, near_k, down_k; float near_m, down_m; } RvRoom;
static struct {
    RvCore c;
    bool bound, force, gu_lab, armed_was, notice_sent, near_said, just, peer_down[NET_MAX_PEERS];  // (just: went DOWNED this frame)
    int (*count)(int *);
    void (*guard_set)(int);
    void (*notice)(const SoraHudQuad *, int);
    int seen, last_k;
    uint64_t t_tick, load_seen;
    SoraHudQuad q[RV_NQ];
    int nq;
} RV = {.last_k = -1};

static void rv_bind(void) {
    if (RV.bound) return;
    RV.bound = true;
    HMODULE m = GetModuleHandleA(NULL);
    RV.count = (int (*)(int *))(void *)GetProcAddress(m, "linkpos_survive_count");
    RV.guard_set = (void (*)(int))(void *)GetProcAddress(m, "linkpos_guard_set");
    if (RV.count) RV.seen = RV.count(NULL);
    RV.load_seen = g_load_end;
    if (!RV.count || !RV.guard_set) LOG("[revive] exe exports: survive count %s, guard %s (no DOWNED state on this exe)\n", RV.count ? "found" : "MISSING", RV.guard_set ? "found" : "MISSING");
}

// the room as revive sees it: live friends (used, PEER_LIVE, same map, no NF_DEAD / NF_LOADING / NF_DOWNED in his last SNAP) and
// the nearest one's 3D distance (-1 = no pose); the nearest DOWNED friend (the hint on this side). Loopback (netloop) = nobody.
static RvRoom rv_room(uint64_t now) {
    RvRoom r = {0, -1, -1, -1.0f, -1.0f};
    if (!NET.on || g_net_loop) return r;
    float me[3] = {g_body[0], g_body[1], g_body[2]};
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        const NetPeerSlot *sl = &NET.slot[k];
        if (!sl->used || sl->hidden || sl->p.st != PEER_LIVE || sl->p.n <= 0) continue;
        uint16_t fl = sl->p.ring[sl->p.n - 1].flags;
        bool down = (fl & NF_DOWNED) && !(fl & (NF_DEAD | NF_LOADING));
        if ((fl & (NF_DEAD | NF_LOADING | NF_DOWNED)) && !down) continue;
        PeerPose q = peer_sample(&sl->p, now);
        float d = -1.0f;
        if (q.valid && isfinite(q.feet[0]) && isfinite(q.feet[1]) && isfinite(q.feet[2])) {
            float dx = q.feet[0] - me[0], dy = q.feet[1] - me[1], dz = q.feet[2] - me[2];
            d = sqrtf(dx * dx + dy * dy + dz * dz);
        }
        if (down) {
            if (r.down_k < 0 || (d >= 0 && (r.down_m < 0 || d < r.down_m))) r.down_k = k, r.down_m = d;
            continue;
        }
        r.nlive++;
        if (r.near_k < 0 || (d >= 0 && (r.near_m < 0 || d < r.near_m))) r.near_k = k, r.near_m = d;
    }
    return r;
}
static const char *rv_name(int k) { return k >= 0 && k < NET_MAX_PEERS && NET.slot[k].p.name[0] ? NET.slot[k].p.name : "a friend"; }
static const char *rv_gone_why(int k) {  // (the Game Over line: what the last live friend did)
    if (k < 0 || k >= NET_MAX_PEERS) return "nobody live in the room";
    const NetPeerSlot *sl = &NET.slot[k];
    if (!NET.on || !sl->used || sl->p.st != PEER_LIVE) return "he left the room";
    if (sl->hidden) return "he left the map";
    uint16_t fl = sl->p.n > 0 ? sl->p.ring[sl->p.n - 1].flags : 0;
    return fl & NF_DEAD ? "he died" : fl & NF_LOADING ? "he is loading" : fl & NF_DOWNED ? "he is down too" : "no pose";
}
// never in a duel (the duel's own KO floor owns the clamp there): its state through its own functions, sora_duel*.h untouched
static bool rv_duel_on(void) { return du_floor_on() || du_on_wire() || du_sm_live(&DU.sm); }

// ---- the early hooks (forward-declared in sora.c) ----
// ca_survive, once a world frame: 4 while armed / DOWNED; also the KO owner shift (rvc_mask)
static int rv_survive_mask(int mk_mask, int life) {
    if (!g_revive && !RV.c.down) return rvc_mask(&RV.c, false);  // (revive 0: no scan, mask 0)
    rv_bind();
    RvRoom r = rv_room(GetTickCount64());
    bool duel = rv_duel_on(), can = RV.count && RV.guard_set && rvc_can_arm(g_revive, life, mk_mask, duel, r.nlive);
    if (g_revive_log && can != RV.armed_was)
        LOG("[revive] %s\n", can ? "armed: a fatal hit = DOWNED (a live friend in the room)"
                             : life <= 0 ? "disarmed: life 0" : mk_mask ? "disarmed: Mickey is armed (a boss near / his KO)"
                             : duel ? "disarmed: a duel" : r.nlive <= 0 ? "disarmed: no live friend in the room" : "disarmed");
    RV.armed_was = can;
    return rvc_mask(&RV.c, can);
}
static bool rv_ko_mine(void) { return rvc_ko_mine(&RV.c); }        // mk_tick: this clamp count change is revive's KO, not Mickey's
static bool rv_drops_hits(void) { return rvc_drops_hits(&RV.c); }  // mk_drops_hits: the exe guard drops every hit (DOWNED + 3 s)

static void rv_life_to_1(int cur) {  // (a forced KO: DOWNED is always 1 quarter heart, as the clamp leaves it)
    uint64_t a = link_actor();
    if (a && cur > 1 && rd_ok(a + 0x848, 4) && *(const int32_t *)(uintptr_t)(a + 0x848) == cur) {
        *(int32_t *)(uintptr_t)(a + 0x848) = 1;
        LOG("[revive] forced KO: Link's life %d -> 1\n", cur);
    } else if (cur > 1) LOG("[revive] forced KO: could not write Link's life (actor %s, life %d)\n", a ? "life differs" : "none", cur);
}
static void rv_down(const char *why, int life, const RvRoom *r) {
    rvc_go_down(&RV.c);
    RV.t_tick = GetTickCount64(), RV.near_said = false, RV.just = true;
    if (r->near_k >= 0) RV.last_k = r->near_k;
    if (life > 1) rv_life_to_1(life);
    NET.downed = true;
    char nr[48];
    if (r->near_m >= 0) snprintf(nr, sizeof nr, "%.15s %.1f m", rv_name(r->near_k), r->near_m);
    else snprintf(nr, sizeof nr, "%s", r->nlive ? "no pose yet" : "-");
    LOG("[revive] DOWNED (%s, life %d): %d live friend%s, nearest %s; a friend within %d m for %d s revives, Game Over in %d s "
        "(hold %s %.1f s = give up)\n", why, life, r->nlive, r->nlive == 1 ? "" : "s", nr, g_revive_m, g_revive_s, g_revive_time, RV_GU_NAME, RV_GIVEUP_S);
}
// mk_tick: Mickey gave this KO up ("It's all over..." / a lost roll): DOWNED instead of the Game Over when a live friend is here
static bool rv_take_ko(const char *why) {
    if (!g_revive) return false;
    rv_bind();
    int life = linkpos_link_life();
    RvRoom r = rv_room(GetTickCount64());
    if (!RV.count || !RV.guard_set || !rvc_can_take(&RV.c, g_revive, life, rv_duel_on(), r.nlive)) return false;
    rv_down(why, life, &r);
    return true;
}

// the DOWNED state ended this frame (e = RVE_*): the revive's heal, or the Game Over
static void rv_end(int e, int life, const RvRoom *r) {
    float secs = RV.c.t_down;
    NET.downed = false;
    if (e == RVE_REVIVED) {
        int q = rvc_heal_q(g_hud_life_max, g_revive_hp), v = link_heal(q);
        LOG("[revive] revived by %.15s (slot %d, %.1f m for %d s) after %.1f s down: life %d -> %d (+%d = %d%% of %d), %.0f s of no hits\n",
            rv_name(r->near_k), r->near_k + 1, r->near_m, g_revive_s, secs, life, v, q, g_revive_hp, g_hud_life_max, RV_GRACE_S);
        if (NET.melty) sora_melty_addon_beat("revive", 1, "friend");  // (value 1 / "friend": 0 / "" stays "back after the Game Over")
    } else if (e == RVE_HEALED) LOG("[revive] healed while down (life %d: food / a potion / a fairy / God Mode): up, %.0f s of no hits\n", life, RV_GRACE_S);
    else if (e == RVE_DIED) LOG("[revive] life 0 while down: BotW's death runs (the KH2 Game Over)\n");
    else if (e == RVE_LOAD_UP) LOG("[revive] a load while down (life %d after it): up, no Game Over\n", life);
    else if (e == RVE_OFF) LOG("[revive] revive 0 while down: up (life stays %d)\n", life);
    else if (rve_gameover(e)) {
        LOG("[revive] %s%s%s after %.1f s down -> Game Over (clamp off, life 0)\n", RVE_NAME[e], e == RVE_NOFRIEND ? ": " : "",
            e == RVE_NOFRIEND ? rv_gone_why(RV.last_k) : "", secs);
        mc_kill_link();  // (BotW's death -> the KH2 Game Over; mc_kill_watch: no death within 10 s -> 1 quarter heart back, still doomed)
    }
}

// the screen text (sora_hud_notice: one slot; kairi-diag's notice wins, the duel's / warp's text draws after this one)
static void rv_draw(const RvRoom *r) {
    char l1[64] = "", l2[48] = "";
    if (RV.c.down) {
        snprintf(l1, sizeof l1, "Waiting for a friend... (hold %s to give up)", RV_GU_NAME);
        if (RV.c.t_gu > 0) snprintf(l2, sizeof l2, "Giving up...");
        else if (RV.c.t_near > 0) snprintf(l2, sizeof l2, "Reviving... %d%%", (int)(100.0f * RV.c.t_near / (float)g_revive_s));
        else {
            int left = (int)ceilf((float)g_revive_time - RV.c.t_down);
            snprintf(l2, sizeof l2, "Game Over in %d s", left > 0 ? left : 0);
        }
    } else if (g_revive && r->down_k >= 0) {  // (this side: a downed friend; GUESS: over his puppet needs sora_net_avatar.h, a follow-up)
        if (r->down_m >= 0 && r->down_m <= (float)g_revive_m) snprintf(l1, sizeof l1, "Reviving %.15s... stay close", rv_name(r->down_k));
        else if (r->down_m >= 0) snprintf(l1, sizeof l1, "%.15s is down! %d m away: stand close to revive", rv_name(r->down_k), (int)(r->down_m + 0.5f));
        else snprintf(l1, sizeof l1, "%.15s is down! Stand close to revive", rv_name(r->down_k));
    }
    bool want = false;
    RV.nq = 0;
    if (l1[0]) {
        if (!RV.notice) RV.notice = (void (*)(const SoraHudQuad *, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_notice");
        if (RV.notice && !KDG.shown_kind) {
            if (!CAMP.tried) camp_load();
            if (CAMP.ok) {
                CsOut o = {RV.q, 0, RV_NQ};
                du_text(&o, l1, 256, RV_TEXT_Y, 0.55f, 1, true);
                if (l2[0]) du_text(&o, l2, 256, RV_TEXT_Y + 20, 0.5f, 1, true);
                RV.nq = o.n, want = o.n > 0;
            }
        }
    }
    if (want) RV.notice(RV.q, RV.nq), RV.notice_sent = true;
    else if (RV.notice_sent) {
        if (RV.notice && !KDG.shown_kind) RV.notice(NULL, 0);
        RV.notice_sent = false;
    }
}

// every unpaused world frame, right after mk_tick (life = Link's quarter hearts)
static void rv_tick(int life, uint64_t present) {
    (void)present;
    if (!g_revive && !RV.c.down && !RV.c.doomed && RV.c.grace <= 0 && !RV.notice_sent && !RV.force && !RV.gu_lab) { NET.downed = false; return; }  // (revive 0: nothing)
    rv_bind();
    uint64_t now = GetTickCount64();
    float dt = RV.t_tick ? (float)(now - RV.t_tick) / 1000.0f : 0.0f;
    RV.t_tick = now;
    bool load = g_load_end != RV.load_seen;
    RV.load_seen = g_load_end;
    rvc_undoom(&RV.c, life, load);
    RvRoom r = rv_room(now);
    if (r.near_k >= 0) RV.last_k = r.near_k;
    // the KO: the clamp saved a fatal hit while last frame's mask was revive's (one owner per KO, rvc_ko_mine)
    if (RV.count) {
        int what = 0, n = RV.count(&what);
        if (n != RV.seen) {
            RV.seen = n;
            if (what == 3 && RV.c.own_prev && !RV.c.down && !RV.c.doomed && g_revive && !rv_duel_on())  // (a duel that started this frame: its KO, as mk_tick's du_floor_on)
                rv_down("a fatal hit", life, &r);
        }
    }
    if (RV.force) {
        RV.force = false;
        if (RV.c.down) LOG("[revive] revforce: already DOWNED\n");
        else if (!RV.count || !RV.guard_set) LOG("[revive] revforce: no exe clamp / guard: nothing\n");
        else if (!rvc_can_take(&RV.c, g_revive, life, rv_duel_on(), r.nlive))
            LOG("[revive] revforce: not armed (%s): nothing\n", !g_revive ? "revive 0" : RV.c.doomed ? "doomed until healed / a load" : life <= 0 ? "life 0"
                                                             : rv_duel_on() ? "a duel" : "no live friend in the room");
        else rv_down("revforce (lab)", life, &r);
    }
    int e = RVE_NONE;
    if (RV.gu_lab) {
        RV.gu_lab = false;
        if (RV.c.down) e = rvc_giveup_now(&RV.c);
        else LOG("[revive] revgiveup: not DOWNED\n");
    }
    if (RV.just) {  // (went DOWNED this frame, here or in mk_tick: a forced KO's life write is not in this frame's life yet)
        RV.just = false;
        if (life > 1) life = 1;
        load = false;  // (the KO came after this frame's load check)
    }
    if (!e) {
        RvCfg g = {(float)g_revive_m, (float)g_revive_s, (float)g_revive_time};
        RvIn in = {dt, life, r.nlive, r.near_m, (g_held & BTN_CLIMB) != 0, load, g_revive};
        e = rvc_step(&RV.c, &g, &in);
    }
    if (e) rv_end(e, life, &r);
    else if (RV.c.down && g_revive_log && (RV.c.t_near > 0) != RV.near_said) {
        RV.near_said = RV.c.t_near > 0;
        LOG("[revive] %s (%.15s %.1f m, radius %d m)\n", RV.near_said ? "a friend is near: the count starts" : "the friend stepped away: the count is back at 0",
            rv_name(r.near_k), r.near_m, g_revive_m);
    }
    if (rvc_drops_hits(&RV.c) && RV.guard_set) RV.guard_set(1);  // (every frame: the driving path's guard_frame sets it again through mk_drops_hits)
    if (RV.c.down) g_mc_pad_ms = GetTickCount64();                // (pad_tap eats BotW's pad as for Mickey's choice screen; + / - / the camera stay BotW's)
    NET.downed = RV.c.down;
    if (g_revive_log)
        for (int k = 0; k < NET_MAX_PEERS; k++) {  // (this side: a friend's NF_DOWNED, the bit as received)
            const NetPeerSlot *sl = &NET.slot[k];
            bool d = NET.on && sl->used && sl->p.st == PEER_LIVE && sl->p.n > 0 && (sl->p.ring[sl->p.n - 1].flags & NF_DOWNED);
            if (d != RV.peer_down[k]) RV.peer_down[k] = d, LOG("[revive] friend %.15s (slot %d): NF_DOWNED %s\n", rv_name(k), k + 1, d ? "on (he is down)" : "off");
        }
    rv_draw(&r);
}

// the driving path, next to mc_input: DOWNED owns the pad (no presses, nothing held; dream_ui zeroes the stick)
static bool rv_input(uint64_t *press, uint64_t *held) {
    if (!RV.c.down) return false;
    *press = 0, *held = 0;
    return true;
}

// the driving path, before the motion end check (next to mk_dchg_loop): while DOWNED Sora drops into KH2's blow landing and holds
// its deepest frame (GUESS: KH2 has no co-op KO motion; P_EX100.mset has no lying / KO entry among A000-A214, and the bake's
// lowest frames of all 97 slots are these crouches: no lying frame). A blow still in the air lands first (the reaction step's
// play(S.motion + 1)); a running special other than a reaction (a Limit, a summon) ends first; on the ground only. On the revive
// the hold lets go: the landing plays on (free from f40 = the get-up). Not driving (swim, climb, ragdoll): the puppet stays Link's.
static void rv_pose_hold(void) {
    if (!RV.c.down) return;
    if (S.special == SP_DAMAGE && (S.motion == MOT_BLOW_FRONT || S.motion == MOT_BLOW_BACK)) return;
    bool back = S.special == SP_DAMAGE && (S.motion == MOT_FLINCH_BACK || S.motion == MOT_AIR_FLINCH_BACK || S.motion == MOT_BLOW_BACK + 1);
    int m = back ? MOT_BLOW_BACK + 1 : MOT_BLOW_FRONT + 1;
    if (S.special != SP_DAMAGE || S.motion != m) {
        if ((S.special && S.special != SP_DAMAGE) || linkpos_link_state() != LINK_GROUND || !mot_of(m)) return;
        play(m), S.special = SP_DAMAGE, S.act = ACT_IDLE, S.takeoff = 0, S.req_set = false, S.air_frames = 0;
        if (g_revive_log) LOG("[revive] pose: motion %d (KH2 blow landing, %s), held at f%.0f\n", m, back ? "back" : "front", back ? RV_HOLD_BACK : RV_HOLD_FRONT);
    }
    float hold = back ? RV_HOLD_BACK : RV_HOLD_FRONT;
    if (S.clock > hold) S.clock = hold;
}

// ---- live ----
static bool rv_live_set(const char *key, int v) {
    if (!strcmp(key, "revive")) {
        if (g_revive == (v != 0)) return true;  // (s290: a live rewrite re-applies every line: one line only when it changes)
        g_revive = v != 0;
        LOG("[revive] revive %d (%s)\n", g_revive, g_revive ? "a KO with a live friend in the room = DOWNED, he revives you by standing close" : "off: BotW's death as before");
    } else if (!strcmp(key, "revivelog")) g_revive_log = v != 0, LOG("[revive] revivelog %d\n", g_revive_log);
    else if (!strcmp(key, "revivem")) g_revive_m = v < 1 ? 1 : v > 10 ? 10 : v, LOG("[revive] a friend within %d m revives\n", g_revive_m);
    else if (!strcmp(key, "revives")) g_revive_s = v < 1 ? 1 : v > 30 ? 30 : v, LOG("[revive] ... after %d s close\n", g_revive_s);
    else if (!strcmp(key, "revivehp")) g_revive_hp = v < 1 ? 1 : v > 100 ? 100 : v, LOG("[revive] back with %d%% of the max life\n", g_revive_hp);
    else if (!strcmp(key, "revivetime")) g_revive_time = v < 5 ? 5 : v > 300 ? 300 : v, LOG("[revive] Game Over after %d s down\n", g_revive_time);
    else if (!strcmp(key, "revforce")) { if (v) RV.force = true, LOG("[revive] revforce: a forced KO at the next world frame\n"); }
    else if (!strcmp(key, "revgiveup")) { if (v) RV.gu_lab = true, LOG("[revive] revgiveup: give up at the next world frame\n"); }
    else return false;
    return true;
}

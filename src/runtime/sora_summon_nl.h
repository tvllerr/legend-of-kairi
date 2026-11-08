// (s139 neverland) Peter Pan's summon Limit "Never Land" (KH2 cmd 468; its sub-commands 470 The Flying Boy / 471 Tiny Fairy / 479 Journey's End / 482 Stop).
// Included from sora_summon.h right after sora_summon_ohana.h; Ohana! (sora_summon_ohana.h) is the pattern (gate, cost on the summon gauge, Time, the face-button
// commands through the sm_fps_* hooks, end conditions, ah_hit reactions). Notes, sources, VERIFIED / GUESS list, tests: recon/S139_HELPER_NEVERLAND.md.
//
// KH2 truth used (KH2's OWN files; Peter's object script P_EX330 bdx = wt137-sumdata/out_s137sumdata/dis/P_EX330.txt, offsets are the first column; 03system.bin; 00battle.bin atkp):
//   cmd 468 Never Land: Execute 81, Cost 250, Time 180 (u16 @+28 / @+32 of the 48-byte row). 470 / 471: Cost 0, Time 0, group 468. 479 / 482: Cost 0, group 0.
//   Dispatcher (bdx 238, arg = the command): 468 = attach Peter to Sora (0x4002, bdx 543), Peter flies (obj_fly, 580), thread 674: Sora capture 253 (A801) + Peter 211 (60A)
//   loop while the Limit is active; se 595042 + prize_drain_start + hide shadow (764); thread 811 -> hover loop 955. 470 = Sora capture 255 + Peter 212 (bdx 2386), 471 the same
//   (the difference: 470 resets collision group 4 and disables group 2, 471 the reverse: bdx 322-338 / 351-367); 479 = se 595043, Sora 256 + Peter 213, then the Limit ends
//   at once (w44 = 0, w360 = 1: no white-in) (bdx 2336); 482 = w44 = 0 (bdx 2490); a normal end (Time <= 0 or 482) = white-in 8 f, friend_end_limit (bdx 2862-2900).
//   The 3 command slots are set EVERY frame by a thread (bdx 2700 / 2729 / 2774): trap_command_override slot 1 <- w48[0] (470), slot 0 <- w48[4] (471), slot 2 <- w48[8] (482).
//   Journey's End: bdx 473 / 252: after (uses of 470) + (uses of 471) >= 8 the slots w48[0] and w48[4] become 479 (both buttons offer Journey's End; Stop stays).
//   Hover loop (bdx 955-1400): the pull attack 1574 (Power 0, ER 5, kb 175/175) on Peter, radius 300 height 200 (bdx 961 / 974); target = Sora's target (w48);
//   the offset Peter - target is shrunk x0.95 per frame while >= 200 cm, rotated by 0.01745 rad x frametime, height w36 = 150 + 0.1 x frametime up to 300 (bdx 1132-1400).
//   No target: random point 400..800 cm (trap_bg_get_random_pos).
//   Hit callback (bdx 9170): prize_appear(przt 128) on atkp 1573 hits, prize_appear(przt 127) on atkp 1225 hits.
//   atkp (00battle.bin): 1573 Pow 20 ER 8 Interval 12, 855 Pow 20 ER 8, 1225 Pow 20 ER 18 Interval 12, 860 Pow 20 ER 18, 1227 Pow 20 ER 18 Interval 8, 1733 Pow 50 ER 17 kb 40.
//   Windows (Peter's mset, S137 facts): 212: 0..30 (1573 / 1225) and 35..40 (855 / 860); 213: 0..60 (1227), 60.. (1733).
// Keys (live_test.txt, ONE-SHOT ones must be removed after they fired): snl 1 (start Never Land, normal gate), snlcmd 470|471|479|482 (ONE-SHOT command),
//   nllog 0|1 (verbose), nltime <units> (Limit Time, 180), nlunit <frames> (KH frames per Time unit, default 1 = KH frames, s139 review).
#pragma once

#define NL_COST_UNITS 250        // VERIFIED cmd 468 Cost
#define NL_TIME_DEF 180.0f       // VERIFIED cmd 468 Time
#define NL_UNIT_DEF 1.0f         // (s139 review) KH frames per Time unit = 1, as FPS Mode (529) / Ohana! (519) and Genie's 542-548 (his Time 60 = range 26 174..234 = the motion's
                                 // end, frame-exact). The clock is PAUSED while Sora's motion has range 45: Sora 255 has 45:0..32 (facts_peterpan.md), 253 / 256 none.
                                 // So 180 f = the standby; a move costs only its frames 32..40+ -> 8 chained moves fit easily (the helper's "cannot hold 8 moves" assumed no pause)
#define NL_PAUSE_END 32.0f       // VERIFIED Sora 255 range 45 (limit timer paused) 0..32
#define NL_MOVES_FOR_END 8       // VERIFIED bdx 473: uses of 470 + 471 >= 8 -> Journey's End
#define NL_ORBIT_MIN_CM 200.0f   // VERIFIED bdx 1172
#define NL_SHRINK 0.95f          // VERIFIED bdx 1195
#define NL_ORBIT_RAD 0.01745299994945526f  // VERIFIED bdx 1212 (x trap_frametime; GUESS: frametime = 1 per KH frame)
#define NL_H0 150.0f             // VERIFIED bdx 985 (hover height start), +0.1 per frame up to 300 (bdx 1383-1394; GUESS: cm)
#define NL_H_STEP 0.1f
#define NL_H_MAX 300.0f
#define NL_TAKEOFF_CM_F 5.0f     // GUESS: the rise from Sora's feet to the 150 cm hover, cm per KH frame (the script's fly-up is in obj_fly, not decoded)
#define NL_PULL_R_CM 300.0f      // VERIFIED bdx 974 (attack 1574 radius)
#define NL_PULL_H_CM 350.0f      // 200 verified as the attack height (bdx 976); the hover offset is added (GUESS)
#define NL_HIT_R_CM 350.0f       // GUESS: the range-10 hitbox shape of the mset is not decoded; everything this close to Peter is hit
#define NL_HIT_DY_CM 600.0f
#define NL_SPEED_CM 35.0f        // GUESS: cap on the unit's move per KH frame (the script teleports along the orbit)
#define NL_WANDER_CM 6.0f        // GUESS: drift speed toward the random point when there is no target
#define NL_WANDER_FRAMES 150.0f  // GUESS: new random point every N frames
#define NL_GRP_470 4             // VERIFIED (s139 review) bdx 322-338: 470 = reset_collision(4) + disable_collision(2); 471 (351-367) the reverse; Peter 212's range 10 group
                                 // param: 1573 / 855 = group 2, 1225 / 860 = group 4 -> 470 = 1225 / 860 (ER 18), 471 = 1573 / 855 (ER 8). (Older notes had it reversed.)

typedef struct { float a, b; int atkp, power, react, kb1; } NlW;
// 212 (44 f): 0..30 re-hits every Interval 12 f -> 3 windows, then 35..40
static const NlW NL_W_G4[] = {{0, 11.99f, 1225, 20, 18, 0}, {12, 23.99f, 1225, 20, 18, 0}, {24, 30, 1225, 20, 18, 0}, {35, 40, 860, 20, 18, 0}};
static const NlW NL_W_G2[] = {{0, 11.99f, 1573, 20, 8, 0}, {12, 23.99f, 1573, 20, 8, 0}, {24, 30, 1573, 20, 8, 0}, {35, 40, 855, 20, 8, 0}};
// 213 (76 f): 0..60 atkp 1227 (Interval 8) -> 8 windows, then 60..76 atkp 1733 (Power 50 ER 17 kb 40)
static const NlW NL_W_END[] = {{0, 7.99f, 1227, 20, 18, 0}, {8, 15.99f, 1227, 20, 18, 0}, {16, 23.99f, 1227, 20, 18, 0}, {24, 31.99f, 1227, 20, 18, 0},
                               {32, 39.99f, 1227, 20, 18, 0}, {40, 47.99f, 1227, 20, 18, 0}, {48, 55.99f, 1227, 20, 18, 0}, {56, 59.99f, 1227, 20, 18, 0}, {60, 76, 1733, 50, 17, 40}};

// forward declarations of the orb file (included after this one)
static void orb_spawn(int kind, int big, int amt, const float at[3]);
static bool orb_off_now(void);
static int orb_munny(void);

static struct {
    int log;
    float time0, unit;
    int st;                 // 0 off, 2 the Limit (no intro: the script goes straight into 253 / 211)
    int req, cmd_req;       // req 1 = start asked; cmd_req = 470 / 471 / 479 / 482 pending
    float time, gauge_spent;
    int cur;                // the running command (0 = standby)
    int uses;               // 470 + 471 used (bdx w64 + w68)
    bool pr_atk, pr_tri, pr_sq, done;
    float u[3];             // the flying unit's logical position (world), y unused
    float dmove[2];         // (s139 review) the body's step for sora.c's next physics step (cm): a velocity through BotW's collision, not a linkpos_move teleport
    float lift;             // cm above Sora's body feet
    float hover;            // the script's w36 (cm)
    float face;
    float start[3];         // where it began (the wander centre)
    float wander[3], wander_t;
    bool has_wander;
    uint64_t tgt;
    uint64_t pulled[16]; int npulled;
    float root_y;           // Journey's End hop height (cm)
    float body_lag;         // frames since the last resync log
    int n_hits, n_moves;
} NL = {.time0 = NL_TIME_DEF, .unit = NL_UNIT_DEF};

#define NLOG(...) do { if (NL.log) LOG(__VA_ARGS__); } while (0)
#define NL_SORA_IDLE 253
#define NL_SORA_MOVE 255
#define NL_SORA_END 256

static int nl_st(void) { return NL.st; }

// ---- the 3 command slots (bdx 2700): slot 1 (the reaction / Triangle slot, VERIFIED slot number; the button name = GUESS) <- 470, slot 0 <- 471, slot 2 <- 482
// after 8 moves slots 1 and 0 both carry 479. Buttons in the mod: slot 1 = FP_TRI_BTN (X), slot 0 = FP_ATTACK_BTN (A), slot 2 = FP_SQ_BTN (Y): GUESS (slots 0 / 2 not mapped in the exe tables).
static int nl_slot_cmd(int slot) {
    bool fin = NL.uses >= NL_MOVES_FOR_END;
    return slot == 1 ? (fin ? 479 : 470) : slot == 0 ? (fin ? 479 : 471) : 482;
}
static int nl_label(int cmd) { return cmd == 470 ? 60 : cmd == 471 ? 61 : cmd == 479 ? 62 : 63; }  // cmd.bin labels (tools_s119/hudbake)

static float nl_sora_frames(int slot, float dflt) {
    const Mot *m = mot_find(g_sm_cc, g_sm_ncc, slot);
    return m && m->frames ? (float)m->frames : dflt;
}

// ---- the gate (menu greying / start)
static const char *nl_gate(int st) {
    if (!SMD()->neverland) return "this summon has no Never Land Limit";
    if (SM.st != SM_ACTIVE) return "Peter Pan is not out and ready";
    if (NL.st) return "Never Land is already running";
    if (SM.hold || SM.call_on || SM.hidden) return "Sora is in the call / hidden";
    if (!(SM.t > 0)) return "the summon gauge is empty (KH2: the Limit is offered while the gauge > 0)";
    if (st != LINK_GROUND || S.act != ACT_IDLE || S.special != SP_NONE) return "Sora is not free on the ground";
    if (pl_hold_party()) return "a party Limit is running";
    return NULL;
}
static bool nl_can(void) { return nl_gate(FP.last_st) == NULL; }
static void nl_request(void) { NL.req = 1; }

// ---- commands
static bool nl_cmd_ok(int cmd) {
    if (NL.st != 2 || NL.done || NL.time <= 0) return false;
    if (cmd == 482) return NL.cur != 479;
    if (cmd == 479) return NL.uses >= NL_MOVES_FOR_END && NL.cur != 479;
    if (cmd != 470 && cmd != 471) return false;
    if (NL.uses >= NL_MOVES_FOR_END) return false;   // the slots carry 479 now
    if (NL.cur == 0) return true;
    // Sora's 255 re-opens the group 468 (type-26 range, param 468) from f40 (VERIFIED, mset facts); 253 has it from f0; 256 never
    return (NL.cur == 470 || NL.cur == 471) && SM.cslot == NL_SORA_MOVE && SM.cclock >= 40.0f;
}
static void nl_abort(const char *why) {
    if (NL.st) LOG("[summon] nl abort/end (%s)\n", why);
    NL.st = 0, NL.req = 0, NL.cmd_req = 0, NL.cur = 0, NL.done = false, NL.dmove[0] = NL.dmove[1] = 0;
}
// sora.c, Sora's ground step (after gl_move): the flight's step since the last physics step joins his move (cm)
static void nl_move(float move[2]) {
    if (!NL.st) { NL.dmove[0] = NL.dmove[1] = 0; return; }
    move[0] += NL.dmove[0], move[1] += NL.dmove[1];
    NL.dmove[0] = NL.dmove[1] = 0;
}

static void nl_windows(int cmd, const NlW **w, int *n) {
    if (cmd == 479) *w = NL_W_END, *n = (int)(sizeof NL_W_END / sizeof NL_W_END[0]);
    else if ((cmd == 470 ? NL_GRP_470 : 6 - NL_GRP_470) == 4) *w = NL_W_G4, *n = (int)(sizeof NL_W_G4 / sizeof NL_W_G4[0]);
    else *w = NL_W_G2, *n = (int)(sizeof NL_W_G2 / sizeof NL_W_G2[0]);
}

static void nl_drop(int atkp, const float at[3]) {   // bdx 9170: atkp 1573 -> przt 128, 1225 -> przt 127. przt rows: 127 = byte 1 at +2, 128 = byte 2 at +8 (columns half decoded: GUESS HP small x1 / munny small x2)
    if (atkp != 1573 && atkp != 1225) return;
    if (orb_off_now()) return;
    if (atkp == 1225) orb_spawn(0, 0, 0, at);
    else if (orb_munny() >= 0)
        for (int i = 0; i < 2; i++) orb_spawn(3, 0, 5 + rand() % 5, at);
}

static void nl_cmd_run(int cmd, const float feet[3], float to_u) {
    if (!nl_cmd_ok(cmd)) { NLOG("[summon] nl cmd %d refused (%s)\n", cmd, NL.st != 2 ? "not in Never Land" : NL.time <= 0 ? "time over" : "not offered now"); return; }
    if (cmd == 482) { NL.done = true; LOG("[summon] nl CMD 482 Stop\n"); return; }
    NL.cur = cmd, SM.win = -1, SM.nhit = 0;
    int pslot = cmd == 479 ? 213 : 212;
    sm_play(pslot);
    SM.blend = -1, SM.clock = 0;
    SM.cslot = cmd == 479 ? NL_SORA_END : NL_SORA_MOVE, SM.cclock = 0;
    NL.root_y = 0;
    if (cmd != 479) NL.uses++, NL.n_moves++;
    else {
        int se = sora_sfx_id("pp_se43");   // obj_play_se 595043 (bdx 2338)
        if (se >= 0) sora_sfx_play_at(se, 1.0f, feet);
        NL.done = false;
    }
    sm_cues_slot(SMC_SUMMON, pslot, -1, 0, SM.pos);
    sm_cues_slot(SMC_SORA, SM.cslot, -1, 0, feet);
    (void)to_u;
    LOG("[summon] nl CMD %d (%s): Peter %d + Sora %d, hit group %d, uses %d/%d, Limit time left %.1f units\n", cmd, cmd == 470 ? "The Flying Boy" : cmd == 471 ? "Tiny Fairy" : "Journey's End",
        pslot, SM.cslot, cmd == 479 ? 3 : (cmd == 470 ? NL_GRP_470 : 6 - NL_GRP_470), NL.uses, NL_MOVES_FOR_END, NL.time);
}

// the windows of the running move: each window hits every enemy near Peter once (the Interval re-hits are separate windows)
static void nl_hit_window(const SmEn *en, int n, float to_u) {
    if (!NL.cur) return;
    const NlW *W;
    int nw;
    nl_windows(NL.cur, &W, &nw);
    float c = SM.clock;
    int wi = -1;
    for (int w = 0; w < nw; w++)
        if (c >= W[w].a && c <= W[w].b) { wi = w; break; }
    if (wi < 0) return;
    if (wi != SM.win) SM.win = wi, SM.nhit = 0;
    for (int k = 0; k < n; k++) {
        if (en[k].life <= 0) continue;
        int had = 0;
        for (int h = 0; h < SM.nhit; h++) had += SM.hit[h] == en[k].actor;
        if (had) continue;
        float d = sm_dist_cm(en[k].pos, SM.pos, to_u), dy = fabsf(en[k].pos[1] - SM.pos[1]) / to_u;
        if (d > NL_HIT_R_CM || dy > NL_HIT_DY_CM) continue;
        if (SM.nhit >= 16) return;
        SM.hit[SM.nhit++] = en[k].actor;
        int dmg = sm_damage(en[k].actor, en[k].life, W[wi].power);
        int react = W[wi].react, kb1 = W[wi].kb1, kb2 = 0;
        ah_atkp(W[wi].atkp, &react, &kb1, &kb2);
        int f54 = ah_hit_f54(en[k].actor, en[k].pos, F54_SMALL, react, to_u);
        linkpos_damage_enemy(en[k].actor, dmg, f54);
        ah_hit(en[k].actor, en[k].pos, SM.pos, react, kb1, kb2, f54, to_u, "neverland");
        float at[3] = {en[k].pos[0], en[k].pos[1] + 0.9f, en[k].pos[2]};
        sora_fx_event("hit", at, SM.face, to_u);
        nl_drop(W[wi].atkp, at);
        NL.n_hits++;
        SM.hits++;
        if (NL.log || SM.log) LOG("[summon] nl HIT %llx (life %d) cmd %d f%.0f window %d atkp %d power %d: damage %d\n", (unsigned long long)en[k].actor, en[k].life, NL.cur, c, wi, W[wi].atkp, W[wi].power, dmg);
    }
}

// attack 1574 (Power 0, ER 5, kb 175/175; radius 300): every enemy that comes within it is pulled once (Interval 0). The pull direction = toward Peter (GUESS: ah_hit pushes away from `from`, so the mirror point is passed)
static void nl_pull(const SmEn *en, int n, float to_u) {
    for (int k = 0; k < n; k++) {
        if (en[k].life <= 0) continue;
        bool had = false;
        for (int i = 0; i < NL.npulled; i++) had |= NL.pulled[i] == en[k].actor;
        if (had) continue;
        float d = sm_dist_cm(en[k].pos, SM.pos, to_u), dy = fabsf(en[k].pos[1] - SM.pos[1]) / to_u;
        if (d > NL_PULL_R_CM || dy > NL_PULL_H_CM) continue;
        if (NL.npulled < 16) NL.pulled[NL.npulled++] = en[k].actor;
        float from[3] = {2 * en[k].pos[0] - SM.pos[0], en[k].pos[1], 2 * en[k].pos[2] - SM.pos[2]};
        int f54 = ah_hit_f54(en[k].actor, en[k].pos, F54_SMALL, 5, to_u);
        ah_hit(en[k].actor, en[k].pos, from, 5, 175, 175, f54, to_u, "neverland pull");
        NLOG("[summon] nl pull (atkp 1574, ER 5) on %llx at %.0f cm\n", (unsigned long long)en[k].actor, d);
    }
}

// ---- Sora's call-actor clock (sora_summon.h sm_tick calls this while the Limit runs, instead of the generic call clock)
static void nl_call_clock(float kh) {
    float fr = nl_sora_frames(SM.cslot, SM.cslot == NL_SORA_IDLE ? 60.0f : 76.0f), now = SM.cclock + kh, f[3];
    if (sm_feet(f)) sm_cues_slot(SMC_SORA, SM.cslot, SM.cclock, now > fr ? fr : now, f);
    SM.cclock = now;
    if (SM.cslot == NL_SORA_IDLE) { if (SM.cclock >= fr) SM.cclock = fmodf(SM.cclock, fr); }
    else if (SM.cclock > fr) SM.cclock = fr;
}

// the call draw (sora_summon.h sm_call_draw): Sora hangs below Peter, lifted above his body feet; the shadow is hidden (bdx 782 status_hide_shadow). true = adjusted
static bool nl_call_adj(float feet[3], float *face) {
    if (!NL.st) return false;
    feet[1] += NL.lift * 0.01f * g_k;
    *face = NL.face;
    return true;
}

// ---- start / end
static void nl_start(const float feet[3], float to_u) {
    float cost = (float)NL_COST_UNITS * FP_RATE, before = SM.t;
    SM.t -= cost;
    if (SM.t < 0) SM.t = 0;
    NL.gauge_spent = before - SM.t;
    NL.st = 2, NL.time = NL.time0, NL.cur = 0, NL.uses = 0, NL.done = false, NL.cmd_req = 0, NL.pr_atk = NL.pr_tri = NL.pr_sq = false;
    NL.n_hits = NL.n_moves = 0, NL.npulled = 0, NL.tgt = 0, NL.has_wander = false, NL.wander_t = 0;
    NL.u[0] = feet[0], NL.u[1] = feet[1], NL.u[2] = feet[2];
    memcpy(NL.start, feet, sizeof NL.start);
    NL.lift = 0, NL.hover = NL_H0, NL.face = S.face, NL.root_y = 0, NL.dmove[0] = NL.dmove[1] = 0;
    SM.hold = SM.call_on = true, SM.cslot = NL_SORA_IDLE, SM.cclock = 0;
    SM.atk = false;
    SM.pos[0] = feet[0], SM.pos[1] = feet[1], SM.pos[2] = feet[2], SM.face = S.face, SM.placed = true;
    sm_fx_clear();
    sm_play(211);
    SM.blend = -1, SM.clock = 0, SM.win = -1, SM.nhit = 0;
    if (SM.body) SM.off(SM.body());
    int se = sora_sfx_id("pp_se42");   // obj_play_se 595042 (bdx 773)
    if (se >= 0) sora_sfx_play_at(se, 1.0f, feet);
    LOG("[summon] nl START: gauge %.0f -> %.0f frames (cost %d x %.0f = %.0f, spent %.0f), Limit time %.0f units x %.0f f (paused in Sora 255 f0..32), Sora 253 + Peter 211, BotW camera (GUESS: KH2's camera events are not rebuilt)\n",
        before, SM.t, NL_COST_UNITS, FP_RATE, cost, NL.gauge_spent, NL.time, NL.unit);
    (void)to_u;
}

static void nl_end(const char *why, const float feet[3], float to_u) {
    LOG("[summon] nl END (%s): %.1f of %.0f time units left, %d moves, %d hits, gauge now %.0f\n", why, NL.time < 0 ? 0.0f : NL.time, NL.time0, NL.n_moves, NL.n_hits, SM.t);
    NL.st = 0, NL.cur = 0, NL.done = false, NL.lift = 0, NL.dmove[0] = NL.dmove[1] = 0;
    SM.hold = SM.call_on = false;
    if (SM.acall > 0 && SM.off) SM.off(SM.acall);
    sm_fx_clear();
    SM.slot = -1, SM.atk = false;
    sm_place_beside(feet, to_u, "Never Land ended");
    sm_play(SMS_IDLE);
    SM.band = PBAND_STAND, SM.pause = 30;
}

// ---- input: Triangle slot (X) = slot 1, decide (A) = slot 0, Square (Y) = slot 2 (GUESS, see nl_slot_cmd)
static void nl_input(uint64_t *press) {
    if (NL.st != 2) return;
    if (*press) NLOG("[summon] nl pad: press 0x%llx (A %d X %d Y %d)\n", (unsigned long long)*press, !!(*press & FP_ATTACK_BTN), !!(*press & FP_TRI_BTN), !!(*press & FP_SQ_BTN));  // (s140 diag)
    if (*press & FP_ATTACK_BTN) NL.pr_atk = true;
    if (*press & FP_TRI_BTN) NL.pr_tri = true;
    if (*press & FP_SQ_BTN) NL.pr_sq = true;
    *press &= ~(FP_ATTACK_BTN | FP_TRI_BTN | FP_SQ_BTN);
}

// ---- the flight (bdx 955-1400): returns the new unit position; Sora's body is moved along by the same delta
static void nl_fly(const SmEn *en, int n, const float feet[3], float kh, float to_u) {
    // the target: Sora's lock target, else the nearest (oh_pick_target); keep it while alive
    int ti = -1;
    for (int k = 0; k < n; k++) if (NL.tgt && en[k].actor == NL.tgt && en[k].life > 0) ti = k;
    if (ti < 0) {
        NL.tgt = oh_pick_target(en, n, feet, to_u);
        for (int k = 0; k < n; k++) if (NL.tgt && en[k].actor == NL.tgt) ti = k;
    }
    float nu[3] = {NL.u[0], NL.u[1], NL.u[2]};
    if (ti >= 0) {
        const float *T = en[ti].pos;
        float ox = (NL.u[0] - T[0]) / to_u, oz = (NL.u[2] - T[2]) / to_u, len = hypotf(ox, oz);
        if (len < 1.0f) ox = NL_ORBIT_MIN_CM, oz = 0, len = ox;
        if (len >= NL_ORBIT_MIN_CM) { float s = powf(NL_SHRINK, kh); ox *= s, oz *= s; }
        float a = NL_ORBIT_RAD * kh, cs = cosf(a), sn = sinf(a), rx = ox * cs + oz * sn, rz = -ox * sn + oz * cs;   // roty direction: GUESS
        nu[0] = T[0] + rx * to_u, nu[2] = T[2] + rz * to_u;
        NL.face = pt_wrap(NL.face + pt_clampf(pt_wrap(atan2f(T[0] - nu[0], T[2] - nu[2]) - NL.face), -SM_TURN * kh, SM_TURN * kh));
        NL.has_wander = false;
    } else {
        NL.wander_t -= kh;
        if (!NL.has_wander || NL.wander_t <= 0) {   // trap_bg_get_random_pos(Peter, 400, 800), GUESS: around the cast spot
            float ang = (float)(rand() % 6283) / 1000.0f, r = 400.0f + (float)(rand() % 400);
            NL.wander[0] = NL.start[0] + sinf(ang) * r * to_u, NL.wander[2] = NL.start[2] + cosf(ang) * r * to_u;
            NL.has_wander = true, NL.wander_t = NL_WANDER_FRAMES;
        }
        float dx = NL.wander[0] - NL.u[0], dz = NL.wander[2] - NL.u[2], d = hypotf(dx, dz);
        float mv = fminf(d, NL_WANDER_CM * kh * to_u);
        if (d > 1e-5f) nu[0] += dx / d * mv, nu[2] += dz / d * mv, NL.face = pt_wrap(NL.face + pt_clampf(pt_wrap(atan2f(dx, dz) - NL.face), -SM_TURN * kh, SM_TURN * kh));
    }
    float dx = nu[0] - NL.u[0], dz = nu[2] - NL.u[2], d = hypotf(dx, dz), cap = NL_SPEED_CM * kh * to_u;
    if (d > cap) dx *= cap / d, dz *= cap / d;
    NL.u[0] += dx, NL.u[2] += dz;
    // Sora's body follows through sora.c's move (nl_move: walls / ledges / water are BotW's; a teleport could put him inside a wall). The unit is resynced
    // to the body when it falls 150 cm behind (nl_late)
    NL.dmove[0] += dx / to_u, NL.dmove[1] += dz / to_u;
    // the hover height (w36): rise to the 150 cm hover first, then 0.1 per frame up to 300
    if (NL.lift < NL.hover) NL.lift = fminf(NL.hover, NL.lift + NL_TAKEOFF_CM_F * kh);
    else { NL.hover = fminf(NL_H_MAX, NL.hover + NL_H_STEP * kh); NL.lift = NL.hover; }
}

// ---- every driven frame (sm_fps_late routes here while Peter Pan is the current row). true = Peter's draw is handled here
static bool nl_late(float kh, float to_u, int st) {
    float feet[3];
    if (SM.st == SM_OFF) {
        if (NL.st) nl_abort("summon gone");
        if (NL.req) NL.req = 0, LOG("[summon] nl refused: no summon is out\n");
        return false;
    }
    bool has_feet = sm_feet(feet);
    if (has_feet) memcpy(FP.last_feet, feet, sizeof feet), FP.have_feet = true;
    if (NL.req) {
        NL.req = 0;
        const char *why = has_feet ? nl_gate(st) : "no Link matrix";
        if (!why && !(SM.slot_pose && SM.ach > 0 && SM.acall > 0 && SM.slot_pose(SM.ach, 212, 0, g_sm_from) && SM.slot_pose(SM.ach, 213, 0, g_sm_from) && SM.slot_pose(SM.acall, NL_SORA_END, 0, g_sm_from)))
            why = "the Never Land pose slots are missing (peterpan_pose.bin 211 / 212 / 213, peterpancall_pose.bin 253 / 255 / 256)";
        if (why) { LOG("[summon] nl refused: %s\n", why); sora_sfx_event("error", 1.0f); }
        else nl_start(feet, to_u);
    }
    if (!NL.st || !has_feet) return false;
    if (!SM.call_on) { nl_end("Sora's call was cut short", feet, to_u); return false; }
    SmEn en[64];
    int n = sm_enemies(en, 64);
    M.dead = 8, M.sub = -1;
    // the unit follows the real body (a wall / the ground may have stopped it)
    if (sm_dist_cm(NL.u, feet, to_u) > 150.0f) NL.u[0] = feet[0], NL.u[2] = feet[2];
    if (!(SM.cslot == NL_SORA_MOVE && SM.cclock < NL_PAUSE_END)) NL.time -= kh / NL.unit;   // range 45 on Sora 255 0..32 pauses the Limit clock
    if (S.act != ACT_IDLE) NL.dmove[0] = NL.dmove[1] = 0;   // (s139 review) off the ground (a ledge): sora.c's ground step does not run, drop the step (no burst on landing)
    if (NL.pr_tri) NL.cmd_req = nl_slot_cmd(1);
    else if (NL.pr_sq) NL.cmd_req = nl_slot_cmd(2);
    else if (NL.pr_atk) NL.cmd_req = nl_slot_cmd(0);
    NL.pr_atk = NL.pr_tri = NL.pr_sq = false;
    if (NL.cmd_req) { int c = NL.cmd_req; NL.cmd_req = 0; nl_cmd_run(c, feet, to_u); }
    if (NL.cur != 479) nl_fly(en, n, feet, kh, to_u);   // 479 sets w44 = 0: the flight thread is over, only the hop remains
    else {   // Journey's End: Sora's 256 root (0, 49, 300) carries the unit ahead (bone-0 root of the baked call motion)
        const Mot *m = mot_find(g_sm_cc, g_sm_ncc, NL_SORA_END);
        float a[3], b[3], c0 = SM.cclock - kh < 0 ? 0 : SM.cclock - kh;
        mot_root(m, c0, a), mot_root(m, SM.cclock, b);
        float fx = sinf(NL.face), fz = cosf(NL.face), dx = ((b[2] - a[2]) * fx + (b[0] - a[0]) * fz) * to_u, dz = ((b[2] - a[2]) * fz - (b[0] - a[0]) * fx) * to_u;
        NL.dmove[0] += dx / to_u, NL.dmove[1] += dz / to_u;   // (s139 review: through sora.c's move, as the flight)
        NL.u[0] += dx, NL.u[2] += dz;
        NL.root_y = b[1];
    }
    S.face = NL.face;
    float at[3] = {feet[0], feet[1] + (NL.lift + NL.root_y) * 0.01f * g_k, feet[2]};
    SM.pos[0] = at[0], SM.pos[1] = at[1], SM.pos[2] = at[2], SM.face = NL.face, SM.floor_s = feet[1];
    if (NL.cur != 479) nl_pull(en, n, to_u);
    // Peter's motion: the standby loops (211), a move plays 212 / 213 with its hit windows
    float c0 = SM.clock;
    SM.clock += kh;
    if (NL.cur) {
        float fr = sm_frames(SM.slot, 60.0f);
        if (SM.clock > fr) SM.clock = fr;
        sm_cues_slot(SMC_SUMMON, SM.slot, c0, SM.clock, at);
        nl_hit_window(en, n, to_u);
        bool done = SM.clock >= fr && SM.cclock >= nl_sora_frames(SM.cslot, fr);
        if (done && NL.cur == 479) { nl_end("Journey's End over", feet, to_u); return false; }
        if (done) {
            NL.cur = 0;
            SM.cslot = NL_SORA_IDLE, SM.cclock = 0;
            sm_play(211);
            SM.blend = -1, SM.clock = 0, SM.win = -1;
        }
    } else {
        float fr = sm_frames(211, 60.0f);
        if (fr > 0 && SM.clock >= fr) SM.clock = fmodf(SM.clock, fr);
    }
    if (NL.done) { nl_end("Stop (482)", feet, to_u); return false; }
    if (NL.time <= 0 && NL.cur != 479) { nl_end("Limit time over", feet, to_u); return false; }   // bdx 2862: the end thread does not wait for a move
    if (SM.ach > 0 && SM.pose && !SM.hidden) {
        SM.shadow(SM.ach, feet[1], 0.0f);
        SM.pose(SM.ach, SM.slot, SM.clock, at, NL.face, to_u);
    }
    return true;
}

static bool nl_live_set(const char *key, int v) {
    if (!strcmp(key, "snl")) { if (v) nl_request(); }
    else if (!strcmp(key, "snlcmd")) {
        if ((v == 470 || v == 471 || v == 479 || v == 482) && NL.st) NL.cmd_req = v;
        else if (v) LOG("[summon] nl snlcmd %d ignored (%s)\n", v, NL.st ? "not 470/471/479/482" : "Never Land is not running");
    }
    else if (!strcmp(key, "nllog")) NL.log = v != 0, LOG("[summon] nl log %s\n", NL.log ? "on" : "off");
    else if (!strcmp(key, "nltime")) NL.time0 = v < 10 ? 10.0f : v > 6000 ? 6000.0f : (float)v, LOG("[summon] nl Limit time %.0f units\n", NL.time0);
    else if (!strcmp(key, "nlunit")) NL.unit = v < 1 ? 1.0f : v > 600 ? 600.0f : (float)v, LOG("[summon] nl unit %.0f KH frames per Time unit\n", NL.unit);
    else return false;
    return true;
}

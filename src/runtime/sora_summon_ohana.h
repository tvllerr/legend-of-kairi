// (s138 ohana) Stitch's summon Limit "Ohana!" (KH2 cmd 519; the face-button commands 525 Shoot / 526 Ukulele / 527 Blast).
// Included from sora_summon.h right after sora_summon_fps.h; Chicken Little's FPS Mode is the pattern (gate, intro, button commands, Time, cost on the summon
// gauge, end conditions, hit reactions via ah_hit). Notes, sources, VERIFIED / GUESS list, tests: recon/S138_HELPER_OHANA.md.
//
// KH2 truth used (KH2's OWN files + the KH2 FM exe, Ghidra kh2proj sha1 2548F119; decomps in recon/s138ohana/):
//   03system.bin cmd rows (48 bytes, Cost u16 @28, Time u16 @32, group u16 @34): 519 Ohana! Cost 250 Time 300 (same numbers as FPS Mode 529), 525 / 526 / 527 Cost 0 group 600.
//   exe dispatch FUN_1401c7bf0 (rva 1C7BF0): 525 -> 1C8610 (Stitch motion 201 + Sora 257 f then 254), 526 -> 1C8700 (Stitch 205 + Sora 255 then 254; effect hb040.4 on both;
//     area attack atkp 1210), 527 -> 1C8440 (Stitch 242 + Sora 256; effect hb040.13 on Sora; atkp 1210). The shots are atkp 883 hit-scans (Power 20, ER 8): re:1C9920 / 1B82F0.
//   exe start_limit FUN_1401c9d60 (rva 1C9D60): Stitch idle (motion 0) + Sora motion 254 (the standby loop, mset type-26 range "group 600" = the commands are accepted).
//   Stitch's script (N_HB040_BTL.mdlx, bdx, recon/s137stitch/stitch_bdx_disasm.txt): intro = Stitch 241 + Sora capture 253 (140 f each), se 598#50 near its end, then the Limit
//     starts; a command runs only while limit_time > 0; Blast sets a flag; the Limit ends when limit_time <= 0 or (after a Blast) when motion 242 ends: se 598#51, whiteout 60 f,
//     both motions back to 0, friend_end_limit. Mset ranges: Sora 254 accepts group 600 from f0, 257 (Shoot) from f30, 255 / 256 never (no range).
//   00battle.bin atkp 883 (Power 20, ER 8, no knock-back) and 1210 (Power 0, ER 9, knock-back 5).
// Keys (live_test.txt, ALL one-shot ones must be removed after they fired): sohana 1 (start Ohana!, normal gate: Stitch out and ready), socmd 525|526|527 (ONE-SHOT command),
//   ohlog 0|1 (verbose), ohtime <frames> (Limit Time, 300), ohcomp 0|1 (1 = hold Stitch in place during his 241 intro, default 1).
#pragma once

#define OH_COST_UNITS 250        // VERIFIED cmd 519 Cost
#define OH_TIME_DEF 300.0f       // VERIFIED cmd 519 Time (unit: GUESS 1 per KH frame, as FPS Mode)
#define OH_AREA_REACT 9          // VERIFIED atkp 1210 EnemyReaction
#define OH_AREA_KB1 5            // VERIFIED atkp 1210 KnockbackStrength1
#define OH_AREA_REACH_CM 1500.0f // GUESS: the exe hits every target that is on screen (not culled); here everything within the party leash of Sora
// (s138 review S1) VERIFIED atkp 1210 Interval = 22 (u16 @+38). The Ukulele / Blast handlers (re:0x1C8700 / 0x1C8440) call the area attack re:0x1CAAC0 on EVERY run of the
// command (the start block is skipped once the action is active, the same per-frame handler pattern as re:0x1CA6E0's state machine); 1CAAC0 keeps ONE attack object
// (+0x630, atkp 0x4BA) and re-hits through it, so a target is staggered every Interval frames while Stitch's 205 / 242 plays (GUESS: the gate = the Interval)
#define OH_AREA_INTERVAL 22.0f
#define OH_LEFT_CM 150.0f        // GUESS: where Stitch stands (KH2 shows him in screen space at (-185, 90), the BotW camera has no such place)

static struct {
    int log, comp;
    float time0;
    int st;                 // 0 off, 1 intro (Stitch 241 + Sora 253), 2 the Limit (commands)
    int req, cmd_req;       // 1 = start asked; cmd_req = 525..527 pending
    float time, ic, gauge_spent;
    int cur;                // the running command (0 = standby)
    float area_t;           // (review S1) frames to the next 1210 area hit of the running Ukulele / Blast
    bool blast_called;
    uint64_t tgt;
    int n_shoot, n_uke, n_blast, n_hits;
    bool pr_atk, pr_tri, pr_sq;
    bool pose_ok_checked;
} OH = {.comp = 1, .time0 = OH_TIME_DEF};

#define OLOG(...) do { if (OH.log) LOG(__VA_ARGS__); } while (0)
#define OH_SORA_IDLE 254
#define OH_SORA_SHOOT 257
#define OH_SORA_UKE 255
#define OH_SORA_BLAST 256

static int oh_st(void) { return OH.st; }

// ---- helpers
static float oh_sora_frames(int slot, float dflt) {
    const Mot *m = mot_find(g_sm_cc, g_sm_ncc, slot);
    return m && m->frames ? (float)m->frames : dflt;
}
static int oh_sora_slot_of(int cmd) { return cmd == 525 ? OH_SORA_SHOOT : cmd == 526 ? OH_SORA_UKE : OH_SORA_BLAST; }
static int oh_stitch_slot_of(int cmd) { return cmd == 525 ? 201 : cmd == 526 ? 205 : 242; }

// bone 0 of slot <slot> at <clk> minus the idle's first frame, as a world offset (x left / z ahead -> world by <face>): Stitch's 241 (the intro, an event shot) travels
// ~6 m through the air in the pose (VERIFIED from stitch_pose.bin: bone 0 z 604, x -322, y 669 at its last frame; 201 / 205 / 242 stay within ~1 m), so it is cancelled
static bool oh_pose_off(int slot, float clk, float face, float to_u, float w[3]) {
    w[0] = w[1] = w[2] = 0;
    float a[SM_MAXB * 12], b[SM_MAXB * 12];
    int nb = SM.bones && SM.ach > 0 ? SM.bones(SM.ach) : 0;
    if (nb <= 0 || nb > SM_MAXB || !SM.slot_pose) return false;
    if (!(SM.slot_pose(SM.ach, slot, clk, a) && SM.slot_pose(SM.ach, SMS_IDLE, 0, b))) return false;
    float sh[3], fx = sinf(face), fz = cosf(face);
    for (int k = 0; k < 3; k++) sh[k] = a[9 + k] - b[9 + k];
    w[0] = (sh[2] * fx + sh[0] * fz) * to_u, w[1] = sh[1] * to_u, w[2] = (sh[2] * fz - sh[0] * fx) * to_u;
    return true;
}

static void oh_anchor(const float feet[3], float to_u) {  // Stitch stands OH_LEFT_CM to Sora's left (the party's return spot), facing as Sora
    float fx = sinf(S.face), fz = cosf(S.face);
    SM.pos[0] = feet[0] + fz * OH_LEFT_CM * to_u, SM.pos[1] = feet[1], SM.pos[2] = feet[2] - fx * OH_LEFT_CM * to_u;
    SM.floor_s = SM.ray_y = feet[1], SM.ray_ok = false, SM.yoff = 0;
}

static uint64_t oh_pick_target(const SmEn *en, int n, const float feet[3], float to_u) {
    // KH2 (re:0x1C96C0): Sora's lock-on target when it is a valid target, else the first one of the target list. Here: the lock target, else the nearest within the leash.
    if (g_lock.actor)
        for (int k = 0; k < n; k++) if (en[k].actor == g_lock.actor && en[k].life > 0) return g_lock.actor;
    uint64_t best = 0;
    float bd = SM_LEASH_CM;
    for (int k = 0; k < n; k++) {
        if (en[k].life <= 0) continue;
        float d = sm_dist_cm(en[k].pos, feet, to_u);
        if (d < bd) bd = d, best = en[k].actor;
    }
    return best;
}

// ---- the gate (menu greying / start)
static const char *oh_gate(int st) {
    if (!SMD()->ohana) return "this summon has no Ohana! Limit";
    if (SM.st != SM_ACTIVE) return "Stitch is not out and ready";
    if (OH.st) return "Ohana! is already running";
    if (SM.hold || SM.call_on || SM.hidden) return "Sora is in the call / hidden";
    if (!(SM.t > 0)) return "the summon gauge is empty (KH2: the Limit is offered while the gauge > 0)";
    if (st != LINK_GROUND || S.act != ACT_IDLE || S.special != SP_NONE) return "Sora is not free on the ground";
    if (pl_hold_party()) return "a party Limit is running";
    return NULL;
}
static bool oh_can(void) { return oh_gate(FP.last_st) == NULL; }
static void oh_request(void) { OH.req = 1; }

// ---- commands
static bool oh_cmd_ok(int cmd) {
    if (OH.st != 2 || OH.time <= 0 || OH.blast_called || cmd < 525 || cmd > 527) return false;
    if (OH.cur == 0) return true;
    // the Sora motion's type-26 range "group 600" (VERIFIED, mset): 254 = from f0 (standby), 257 Shoot = from f30; 255 / 256 never
    return OH.cur == 525 && SM.cslot == OH_SORA_SHOOT && SM.cclock >= 30.0f;
}
static void oh_abort(const char *why) {
    if (OH.st) LOG("[summon] ohana abort/end (%s)\n", why);
    OH.st = 0, OH.req = 0, OH.cmd_req = 0, OH.cur = 0, OH.blast_called = false;
}

static void oh_cmd_run(int cmd, const float feet[3], const SmEn *en, int n, float to_u) {
    if (!oh_cmd_ok(cmd)) { OLOG("[summon] ohana cmd %d refused (%s)\n", cmd, OH.st != 2 ? "not in Ohana!" : OH.blast_called ? "Blast is running" : OH.time <= 0 ? "time over" : "a command is running"); return; }
    OH.cur = cmd, OH.area_t = 0;
    OH.tgt = oh_pick_target(en, n, feet, to_u);
    int k = -1;
    for (int i = 0; i < n; i++) if (en[i].actor == OH.tgt) k = i;
    if (cmd != 526 && k >= 0) SM.face = atan2f(en[k].pos[0] - SM.pos[0], en[k].pos[2] - SM.pos[2]);  // Stitch turns to the target to shoot (GUESS: the exe aims the shots at it)
    else SM.face = S.face;
    sm_play(oh_stitch_slot_of(cmd));
    SM.blend = -1, SM.clock = 0, SM.win = -1, SM.nhit = 0, SM.atk_i = cmd == 527 ? 3 : 0, SM.tgt = OH.tgt;
    SM.cslot = oh_sora_slot_of(cmd), SM.cclock = 0;
    if (cmd == 525) OH.n_shoot++;
    else if (cmd == 526) OH.n_uke++;
    else OH.n_blast++, OH.blast_called = true;
    sm_cues_slot(SMC_SUMMON, SM.slot, -1, 0, SM.pos);   // Stitch's frame-0 voice / effect rows
    sm_cues_slot(SMC_SORA, SM.cslot, -1, 0, feet);      // Sora's frame-0 rows (the effects / sounds of 255 / 256 start at f1: fired by oh_call_clock)
    LOG("[summon] ohana CMD %d (%s): Stitch %d + Sora %d, target %llx, Limit time left %.0f f\n", cmd, cmd == 525 ? "Shoot" : cmd == 526 ? "Ukulele" : "Blast", SM.slot, SM.cslot,
        (unsigned long long)OH.tgt, OH.time);
}

// KH2 atkp 1210 (re:0x1CAAC0, Power 0 ER 9 knock-back 5): the Ukulele / Blast stagger every enemy in reach, at the start of the move and then every OH_AREA_INTERVAL
// frames while Stitch's motion plays (review S1). Power 0 = no damage, so only the reaction (ah_hit) is applied.
static void oh_area(const SmEn *en, int n, const float feet[3], float to_u) {
    int hit = 0;
    for (int k = 0; k < n; k++) {
        if (en[k].life <= 0 || sm_dist_cm(en[k].pos, feet, to_u) > OH_AREA_REACH_CM) continue;
        int f54 = ah_hit_f54(en[k].actor, en[k].pos, F54_SMALL, OH_AREA_REACT, to_u);
        ah_hit(en[k].actor, en[k].pos, feet, OH_AREA_REACT, OH_AREA_KB1, 0, f54, to_u, "ohana");
        hit++;
    }
    OLOG("[summon] ohana area attack (atkp 1210, ER 9): %d enemy(ies) staggered\n", hit);
}

// ---- Sora's call-actor clock (sora_summon.h sm_tick calls this while the Limit runs, instead of the generic call clock)
static void oh_call_clock(float kh) {
    float fr = oh_sora_frames(SM.cslot, SM.cslot == OH_SORA_IDLE ? 300.0f : 100.0f), now = SM.cclock + kh, f[3];
    if (sm_feet(f)) sm_cues_slot(SMC_SORA, SM.cslot, SM.cclock, now > fr ? fr : now, f);
    SM.cclock = now;
    if (SM.cslot == OH_SORA_IDLE) { if (SM.cclock >= fr) SM.cclock = fmodf(SM.cclock, fr); }   // the standby loops
    else if (SM.cclock > fr) SM.cclock = fr;
}

// ---- start / end
static void oh_start(const float feet[3], float to_u) {
    float cost = (float)OH_COST_UNITS * FP_RATE, before = SM.t;   // cmd 519 Cost 250 x sstm ChangeTimeAddRate 12 (VERIFIED for cmd 529, same unit)
    SM.t -= cost;
    if (SM.t < 0) SM.t = 0;
    OH.gauge_spent = before - SM.t;
    OH.st = 1, OH.ic = 0, OH.time = OH.time0, OH.cur = 0, OH.blast_called = false, OH.cmd_req = 0, OH.pr_atk = OH.pr_tri = OH.pr_sq = false;
    OH.n_shoot = OH.n_uke = OH.n_blast = OH.n_hits = 0;
    // the intro: Sora plays 253 on the call actor, Stitch 241 beside him (the generic call hold + cues)
    SM.hold = SM.call_on = true, SM.cslot = SMS_CALL_END, SM.cclock = 0;
    SM.atk = false;
    oh_anchor(feet, to_u);
    SM.face = S.face;
    sm_fx_clear();
    sm_play(241);
    SM.clock = 0, SM.blend = -1;
    if (SM.body) SM.off(SM.body());
    sm_cues_slot(SMC_SUMMON, 241, -1, 0, SM.pos);
    LOG("[summon] ohana START: gauge %.0f -> %.0f frames (cost %d x %.0f = %.0f, spent %.0f), Limit time %.0f f, intro Stitch 241 + Sora 253 (%.0f f), BotW camera (GUESS: KH2's intro camera shot is not rebuilt)\n",
        before, SM.t, OH_COST_UNITS, FP_RATE, cost, OH.gauge_spent, OH.time, sm_frames(241, 140.0f));
}

static void oh_end(const char *why, const float feet[3], float to_u) {
    LOG("[summon] ohana END (%s): %.0f of %.0f Limit frames left, %d Shoot, %d Ukulele, %d Blast, %d hits, gauge now %.0f\n", why, OH.time, OH.time0, OH.n_shoot, OH.n_uke, OH.n_blast,
        OH.n_hits, SM.t);
    int se = sora_sfx_id("st_se51");   // the script's end sound (bank 598 #51)
    if (se >= 0) sora_sfx_play_at(se, 1.0f, feet);
    OH.st = 0, OH.cur = 0, OH.blast_called = false;
    SM.hold = SM.call_on = false;
    if (SM.acall > 0 && SM.off) SM.off(SM.acall);
    sm_fx_clear();
    SM.slot = -1, SM.atk = false;
    sm_place_beside(feet, to_u, "Ohana! ended");
    sm_play(SMS_IDLE);
    SM.band = PBAND_STAND, SM.pause = 30;
}

// ---- input (sora.c, sm_fps_input): A = Shoot (the "decide" button, English table 0x5CC470: 525 -> button 4), Y = Ukulele (526 -> button 3), X = Blast.
//      (review) Blast: the script's per-frame thread (bdx L823-L866, from the end of the intro while motion != 242) calls trap_command_override(slot 1, 527) =
//      re:0x430010 -> re:0x3B2850(menu + 0xDD0, slot, cmd): VERIFIED slot 1, the slot every native exe caller of 3B2850 uses for reaction commands (13 call sites, all EDX = 1),
//      i.e. KH2's Triangle (reaction) prompt. Mapping it to the mod's FP_TRI_BTN (Switch X, = FPS Mode's Firecracker slot) = the button NAME is the s137 GUESS, the slot is VERIFIED.
static void oh_input(uint64_t *press) {
    if (OH.st != 2) return;
    if (*press & FP_ATTACK_BTN) OH.pr_atk = true;
    if (*press & FP_TRI_BTN) OH.pr_tri = true;
    if (*press & FP_SQ_BTN) OH.pr_sq = true;
    *press &= ~(FP_ATTACK_BTN | FP_TRI_BTN | FP_SQ_BTN);
}

// ---- every driven frame (sm_fps_late routes here while Stitch is the current row). true = Stitch's draw is handled here
static bool oh_late(float kh, float to_u, int st) {
    float feet[3];
    if (SM.st == SM_OFF) {
        if (OH.st) oh_abort("summon gone");
        if (OH.req) OH.req = 0, LOG("[summon] ohana refused: no summon is out\n");
        return false;
    }
    bool has_feet = sm_feet(feet);
    if (has_feet) memcpy(FP.last_feet, feet, sizeof feet), FP.have_feet = true;
    if (OH.req) {
        OH.req = 0;
        const char *why = has_feet ? oh_gate(st) : "no Link matrix";
        if (!why && !(SM.slot_pose && SM.ach > 0 && SM.acall > 0 && SM.slot_pose(SM.ach, 205, 0, g_sm_from) && SM.slot_pose(SM.acall, OH_SORA_UKE, 0, g_sm_from)))
            why = "the Ohana! pose slots are missing (run tools_s138ohana/install_ohana.sh: stitch_pose.bin needs 205, stitchcall_pose.bin 254-257)";
        if (why) { LOG("[summon] ohana refused: %s\n", why); sora_sfx_event("error", 1.0f); }
        else oh_start(feet, to_u);
    }
    if (!OH.st) return false;
    if (!has_feet) return false;
    SmEn en[64];
    int n = sm_enemies(en, 64);
    bool handled = false;
    float w[3] = {0, 0, 0};
    if (OH.st == 1) {   // the intro
        OH.ic += kh;
        float c0 = SM.clock, fr = sm_frames(241, 140.0f);
        SM.clock = OH.ic > fr ? fr : OH.ic;
        oh_anchor(feet, to_u);
        SM.face = S.face;
        sm_cues_slot(SMC_SUMMON, 241, c0, SM.clock, SM.pos);
        if (OH.comp) oh_pose_off(241, SM.clock, SM.face, to_u, w);
        if (SM.ach > 0 && SM.pose && !SM.hidden) {
            float at[3] = {SM.pos[0] - w[0], feet[1] - w[1], SM.pos[2] - w[2]};
            SM.shadow(SM.ach, feet[1], blob_str(0.55f));
            SM.pose(SM.ach, 241, SM.clock, at, SM.face, to_u);
        }
        handled = true;
        if (!SM.call_on) {   // Sora's 253 is over (or a hit cut it short: then the Limit does not start)
            if (S.special == SP_DAMAGE) { oh_end("Sora was hit during the intro", feet, to_u); return false; }
            int se = sora_sfx_id("st_se50");   // the script's sound before the Limit starts (bank 598 #50)
            if (se >= 0) sora_sfx_play_at(se, 1.0f, feet);
            OH.st = 2;
            SM.hold = SM.call_on = true, SM.cslot = OH_SORA_IDLE, SM.cclock = 0;
            sm_play(SMS_IDLE);
            SM.blend = -1, SM.clock = 0;
            LOG("[summon] ohana: intro over after %.0f f, Limit running (time %.0f f): A Shoot, Y Ukulele, X Blast\n", OH.ic, OH.time);
        }
    }
    if (OH.st == 2) {
        M.dead = 8, M.sub = -1;
        if (!SM.call_on) { oh_end("Sora was hit", feet, to_u); return false; }   // sm_tick cut the call short
        OH.time -= kh;
        if (OH.pr_tri) OH.cmd_req = 527;
        else if (OH.pr_sq) OH.cmd_req = 526;
        else if (OH.pr_atk) OH.cmd_req = 525;
        OH.pr_atk = OH.pr_tri = OH.pr_sq = false;
        if (OH.cmd_req) { int c = OH.cmd_req; OH.cmd_req = 0; oh_cmd_run(c, feet, en, n, to_u); }
        oh_anchor(feet, to_u);
        if (OH.cur) {   // a command: Stitch's motion (hits + cues), Sora's was set at the start
            float c0 = SM.clock, fr = sm_frames(SM.slot, 100.0f);
            SM.clock += kh;
            if (SM.clock > fr) SM.clock = fr;
            sm_cues_slot(SMC_SUMMON, SM.slot, c0, SM.clock, SM.pos);
            if (OH.cur != 526) {
                int h0 = SM.hits;
                sm_hit_window(en, n, to_u);
                OH.n_hits += SM.hits - h0;
            }
            if (OH.cur != 525 && SM.clock < fr && (OH.area_t -= kh) <= 0) OH.area_t += OH_AREA_INTERVAL, oh_area(en, n, feet, to_u);  // (review S1) every 22 f while 205 / 242 plays
            float sfr = oh_sora_frames(SM.cslot, fr);
            bool done = SM.clock >= fr && SM.cclock >= sfr;   // both motions over
            if (done && OH.cur == 527) { oh_end("Blast over", feet, to_u); return false; }   // the script waits for motion 242, then ends the Limit
            if (done) {
                OH.cur = 0;
                SM.cslot = OH_SORA_IDLE, SM.cclock = 0;   // exe: Sora's next motion is 254
                sm_play(SMS_IDLE);
                SM.blend = -1, SM.clock = 0, SM.face = S.face;
            }
        } else {   // standby: Stitch's idle loops
            SM.clock += kh;
            float fr = sm_frames(SMS_IDLE, 170.0f);
            if (fr > 0) SM.clock = fmodf(SM.clock, fr);
            SM.face = S.face;
        }
        // (review M1, VERIFIED Stitch's bdx L899-L945: the end thread loops while limit_time > 0 AND motion != 242; once 242 plays it waits for motion_is_end) a Blast
        // that is running always plays out, even past Time 0; Shoot / Ukulele / standby end at once at Time 0 (L947: se 598#51, action clear, whiteout)
        if (OH.st == 2 && OH.time <= 0 && OH.cur != 527) { oh_end("Limit time over", feet, to_u); return false; }
        if (SM.ach > 0 && SM.pose && !SM.hidden) {
            float at[3] = {SM.pos[0], feet[1], SM.pos[2]};
            SM.shadow(SM.ach, feet[1], blob_str(0.55f));
            SM.pose(SM.ach, SM.slot, SM.clock, at, SM.face, to_u);
        }
        handled = true;
    }
    return handled;
}

static bool oh_live_set(const char *key, int v) {
    if (!strcmp(key, "sohana")) { if (v) oh_request(); }
    else if (!strcmp(key, "socmd")) {  // (review) only while Ohana! runs: a stale line in a later rewrite is dropped with a log, not queued
        if (v >= 525 && v <= 527 && OH.st) OH.cmd_req = v;
        else if (v) LOG("[summon] ohana socmd %d ignored (%s)\n", v, OH.st ? "not 525..527" : "Ohana! is not running");
    }
    else if (!strcmp(key, "ohlog")) OH.log = v != 0, LOG("[summon] ohana log %s\n", OH.log ? "on" : "off");
    else if (!strcmp(key, "ohtime")) OH.time0 = v < 30 ? 30.0f : v > 6000 ? 6000.0f : (float)v, LOG("[summon] ohana Limit time %.0f f\n", OH.time0);
    else if (!strcmp(key, "ohcomp")) OH.comp = v != 0, LOG("[summon] ohana intro pose travel %s\n", OH.comp ? "cancelled (Stitch stays beside Sora)" : "kept (raw 241)");
    else return false;
    return true;
}

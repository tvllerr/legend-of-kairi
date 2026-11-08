// (s139 genielim) Genie's summon Limits, one per Drive-Form version, and his version-switch menu. Notes, sources, VERIFIED / GUESS list, tests: recon/S139_HELPER_GENIELIM.md.
// Included from sora_summon.h right after sora_summon_ohana.h. Pattern: Ohana! (gate, cost on the summon gauge, Time, Sora on the call actor, end rules).
//
// KH2 truth used (KH2's own files; exe = KH2 FM, Ghidra kh2proj, headless; decomps in recon/s139genielim/):
//   03system.bin cmd rows (48 B, Cost u16 @28, Time u16 @32, group u16 @34): 542 Sonic Cost 50 Time 24, 544 Strike 50 / 24, 546 Arcana 150 / 60, 548 Infinity 250 / 60;
//     543 Rave / 545 Judgment / 547 Bash / 549 Impact Cost 0 Time 0 and GROUP 535 / 536 / 537 / 538; 535..538 {Valor,Wisdom,Master,Final} Genie Cost 0.
//   Genie's object script (P_AL010 bdx, dis/P_AL010.txt): dispatcher L150: 542 -> offers 543 (L3413) + Sora capture 253 / Genie 254 (L414 / L430); 543 -> 255 / 256 (L620),
//     the Limit flag is cleared after it (L640); 544 -> shots (999) + 257 / 258, 545 -> shots (1555, period 14) + 259 / 260; 546 -> 261 / 262, 547 -> orb spawner (period 12) + 263 / 264;
//     548 -> magic thread 3317 + hover + 265 / 266, 549 -> obj_idle + enemy_stop_all(17) + 267 / 268 + flag clear. The Limit thread (L3806 / L3835): while limit_time > 0 and the flag is set it
//     calls trap_command_override(slot 1, <follow-up cmd>) every frame when trap_obj_can_decide_command allows it (slot 1 = the reaction / Triangle slot, as Ohana's Blast).
//     End (L3941): when limit_time <= 0 or the flag clears -> friend_end_limit, then it WAITS for the Sora capture to end (the motion is never cut).
//   Sora motion ranges (mset, facts_genie.md): range 45 (limit timer paused) ends at f96 / 140 / 174 / 320 of the stage-1 motions; range 26 param 535..538 (= the follow-up command's GROUP) starts there.
//     So the follow-up is accepted from that frame for Time frames: Master 174..234 and Final 320..380 end exactly at the motion's end (60 f); Valor 96..120 and Wisdom 140..164 are inside it.
//   Menu (exe FUN_1403f3860, rva 3f3860): while the summon is out and its id is 201 (Genie) the second top page's Party slot is cmd 605 with a sub-menu built from the OWNED forms
//     (FUN_1403de370: forms 1..5 without 3, FUN_1403de240 -> vftable+0x1a6 table -> cmd 535..538); the current form is NOT left out. The Summon slot stays (Dismiss 316).
//   atkp (00battle.bin; Power u16 @6, ER @10, kb s16 @12/14, Interval u16 @38): 988 P100 ER9 kb10/15 Intv10 | 996 P150 ER11 kb65/15 | 999 P10 ER8 kb1/0 | 1555 P20 ER17 kb8/5 |
//     1478 P20 ER9 kb4/0 Intv16 | 1373 P20 ER17 kb4/1 | 1629 P20 ER9 kb3/-1 Intv8 | 1631 P65 ER17 kb5/5 Intv8.
// Keys (live_test.txt; the ONE-SHOT ones must be removed after they fired): sgl 1 (start the current version's Limit; normal gate), sglcmd <543|545|547|549|535..538> (ONE-SHOT: stage 2 /
//   version switch), gllog 0|1, gltime <frames> (0 = the cmd row's Time), sgver <1-4> (pp.h: switch / force the version).
#pragma once

static bool rcs_show(const char *name, bool flash);   // sora_rcslot.h (included later by sora.c)
#define GL_NPROJ 24
#define GL_NHIT 64

typedef struct { float a, b; int atkp, power, react, kb1, kb2, intv; } GlW;   // a hit window of the Genie motion clock
typedef struct {
    int sora_slot;                // Sora's capture slot; Genie plays slot + 1 (VERIFIED pairing rule, L430)
    float reach, dy;              // GUESS hit volume (cm) round Genie
    int nwin; GlW w[12];
    int ne; float e_a[3], e_b[3], e_period; int e_atkp, e_power, e_react, e_kb1, e_kb2; float e_rad;   // projectile emitters (ranges a..b, period) - Wisdom / Master
} GlStage;
typedef struct {
    const char *name1, *name2;    // Sonic / Rave ...
    int cmd1, cmd2, cost, time;   // VERIFIED cmd rows
    float f26;                    // the follow-up window opens (range 26 / end of range 45, stage 1)
    GlStage st[2];
} GlVer;

// windows: VERIFIED from facts_genie.md (range 10 triggers / kind 33 for 996); reach / dy = GUESS
static const GlVer GLV[4] = {
    {"Sonic", "Rave", 542, 543, 50, 24, 96.0f, {
        {253, 420.0f, 500.0f, 4, {{6, 18, 988, 100, 9, 10, 15, 10}, {28, 41, 988, 100, 9, 10, 15, 10}, {52, 69, 988, 100, 9, 10, 15, 10}, {73, 85, 988, 100, 9, 10, 15, 10}}, 0},
        {255, 450.0f, 700.0f, 2, {{10, 18, 988, 100, 9, 10, 15, 10}, {33, 60, 996, 150, 11, 65, 15, 0}}, 0}}},
    {"Strike", "Judgment", 544, 545, 50, 24, 140.0f, {
        {257, 0, 0, 0, {{0}}, 3, {28, 36, 16}, {150, 150, 150}, 8.0f, 999, 10, 8, 1, 0, 80.0f},
        {259, 0, 0, 0, {{0}}, 3, {19, 26, 19}, {132, 132, 132}, 14.0f, 1555, 20, 17, 8, 5, 80.0f}}},
    {"Arcana", "Bash", 546, 547, 150, 60, 174.0f, {
        {261, 600.0f, 700.0f, 1, {{0, 234, 1478, 20, 9, 4, 0, 16}}, 0},
        {263, 0, 0, 0, {{0}}, 1, {14, 0, 0}, {199, 0, 0}, 12.0f, 1373, 20, 17, 4, 1, 60.0f}}},
    {"Infinity", "Impact", 548, 549, 250, 60, 320.0f, {
        {265, 450.0f, 600.0f, 11, {{6, 24, 1629, 20, 9, 3, -1, 8}, {42, 53, 1629, 20, 9, 3, -1, 8}, {90, 99, 1629, 20, 9, 3, -1, 8}, {104, 110, 1629, 20, 9, 3, -1, 8},
                                    {113, 125, 1629, 20, 9, 3, -1, 8}, {133, 139, 1629, 20, 9, 3, -1, 8}, {146, 158, 1629, 20, 9, 3, -1, 8}, {163, 169, 1629, 20, 9, 3, -1, 8},
                                    {172, 184, 1629, 20, 9, 3, -1, 8}, {188, 197, 1629, 20, 9, 3, -1, 8}, {204, 337, 1629, 20, 9, 3, -1, 8}}, 0},
        {267, 700.0f, 800.0f, 1, {{150, 220, 1631, 65, 17, 5, 5, 8}}, 0}}},
};

typedef struct { bool on; float p[3]; uint64_t tgt; int fx, atkp, power, react, kb1, kb2; float rad, life; uint32_t fxg; } GlProj;   // fxg = the fx slot's g_fx_gen at the spawn
typedef struct { uint64_t actor; int win; float at; } GlHit;
static struct {
    int log;
    int time_ov;
    int st;                 // 0 off, 1 a stage plays
    int req, cmd_req;       // 1 = start asked; cmd_req = 543.. / 535..538 (ONE-SHOT or the Triangle press)
    int stage;              // 1 / 2
    int ver;                // 1..4 the running Limit's version
    float time, gauge_spent;
    float org[3], face, ybase, sy, ylast;   // stage origin (world), facing, hover height carried into stage 2 (cm), Sora's hover now (cm), Sora's root y at the end of the stage
    float dmove[2];         // Sora's root step since the last gl_move (world cm)
    float rprev[3];         // Sora's root at the last clock
    float timer[3];         // emitter cooldowns
    GlProj pj[GL_NPROJ];
    GlHit hit[GL_NHIT]; int nhit;
    int aura, aura_ver; uint64_t aura_at;     // al010.(ver-1) bound on Genie
    uint32_t aura_g;        // (s139 review) its g_fx_gen: exe fx handles are reused slot numbers (0 is a valid one), as the Tink review
    int n_hits, n_proj;
    bool pr_tri;
    bool shown;
} GL = {.aura = -1};

#define GLOG(...) do { if (GL.log) LOG(__VA_ARGS__); } while (0)
static int gl_st(void) { return GL.st; }
static const GlVer *gl_ver(void) { int v = GL.st ? GL.ver : SMX.ver; return &GLV[(v >= 1 && v <= 4 ? v : 1) - 1]; }
static const GlStage *gl_stage(void) { return &gl_ver()->st[GL.stage == 2 ? 1 : 0]; }

static float gl_frames(int slot, bool sora, float dflt) {
    const Mot *m = sora ? mot_find(g_sm_cc, g_sm_ncc, slot) : sm_mot(slot);
    return m && m->frames ? (float)m->frames : dflt;
}
static void gl_rot(const float r[3], float out[3], float face, float to_u) {   // entity root (x left, y up, z ahead, cm) -> world (the rule of sm_root / sora.c)
    float fx = sinf(face), fz = cosf(face);
    out[0] = (r[2] * fx + r[0] * fz) * to_u, out[1] = r[1] * to_u, out[2] = (r[2] * fz - r[0] * fx) * to_u;
}
static void gl_root(const Mot *m, float c, float r[3]) { r[0] = r[1] = r[2] = 0; if (m && m->root) mot_root(m, c, r); }

// ---- the aura: al010.0-3 per version bound on Genie, restarted after a version change (bdx L4104: genie_get_limit_command -> obj_effect_start_bind(self, ver-1, loop)); (s139 hand row)
static bool gl_aura_mine(void) { return fx_mine(GL.aura, GL.aura_g) && sora_fx_alive(GL.aura); }
static void gl_aura_kill(void) {
    if (gl_aura_mine()) sora_fx_kill(GL.aura);
    GL.aura = -1;
}
static void gl_aura_tick(float to_u) {
    if (!SMD()->genie || SM.st != SM_ACTIVE || SM.hidden || SMX.pend) { if (SM.st != SM_ACTIVE || SMX.pend) gl_aura_kill(); return; }
    int v = SMX.ver >= 1 && SMX.ver <= 4 ? SMX.ver : 1;
    if (GL.aura_ver == v && gl_aura_mine()) { float at[3] = {SM.pos[0], SM.floor_s + SM.yoff * to_u, SM.pos[2]}; sora_fx_move(GL.aura, at, SM.face); return; }
    gl_aura_kill();
    if (vk_frame_counter() - GL.aura_at < 90) return;   // (a short element is not respawned every frame)
    GL.aura_at = vk_frame_counter();
    char nm[16];
    snprintf(nm, sizeof nm, "gn_fx_%d", v - 1);
    int id = sora_fx_id(nm);
    if (id < 0) return;
    float at[3] = {SM.pos[0], SM.floor_s + SM.yoff * to_u, SM.pos[2]};
    GL.aura = sora_fx_spawn(id, at, SM.face, to_u), GL.aura_ver = v;
    GL.aura_g = GL.aura >= 0 && GL.aura < 32 ? g_fx_gen[GL.aura] : 0;
}
static void gl_aura_reset(void) { gl_aura_kill(); }   // smx_set_ver: the version changed
static void gl_reset(void) { gl_aura_kill(); }          // smx_reset (sm_end): the aura goes with him

// ---- the hit (copy of sm_hit_window's per-hit part)
static void gl_hit_one(uint64_t actor, int32_t life, const float epos[3], const float from[3], int atkp, int power, int react, int kb1, int kb2, float to_u, const char *what) {
    ah_atkp(atkp, &react, &kb1, &kb2);
    int dmg = sm_damage(actor, life, power);
    int f54 = ah_hit_f54(actor, epos, F54_SMALL, react, to_u);
    linkpos_damage_enemy(actor, dmg, f54);
    ah_hit(actor, epos, from, react, kb1, kb2, f54, to_u, "summon");
    float at[3] = {epos[0], epos[1] + 0.9f, epos[2]};
    sora_fx_event("hit", at, SM.face, to_u);
    static int hit_se = -2;
    if (hit_se == -2) hit_se = sora_sfx_id("hit");
    if (hit_se >= 0) sora_sfx_play_at(hit_se, 1.0f, at);
    SM.hits++, GL.n_hits++;
    GLOG("[summon] gl HIT %llx (life %d) %s atkp %d power %d: damage %d\n", (unsigned long long)actor, life, what, atkp, power, dmg);
}

// Genie's melee windows (range 10 in his motion): every enemy within reach of him, one hit per Interval (0 = once per window)
static void gl_windows(const GlStage *S2, const SmEn *en, int n, const float gp[3], float c, float to_u) {
    for (int w = 0; w < S2->nwin; w++) {
        const GlW *W = &S2->w[w];
        if (c < W->a || c > W->b) continue;
        for (int k = 0; k < n; k++) {
            if (en[k].life <= 0) continue;
            float d = hypotf(en[k].pos[0] - gp[0], en[k].pos[2] - gp[2]) / to_u, dy = fabsf(en[k].pos[1] - SM.floor_s) / to_u;
            if (d > S2->reach || dy > S2->dy) continue;
            int hi = -1;
            for (int h = 0; h < GL.nhit; h++) if (GL.hit[h].actor == en[k].actor && GL.hit[h].win == w) hi = h;
            if (hi >= 0) {
                if (W->intv <= 0 || c - GL.hit[hi].at < (float)W->intv) continue;
                GL.hit[hi].at = c;
            } else {
                if (GL.nhit >= GL_NHIT) continue;
                GL.hit[GL.nhit++] = (GlHit){en[k].actor, w, c};
            }
            gl_hit_one(en[k].actor, en[k].life, en[k].pos, gp, W->atkp, W->power, W->react, W->kb1, W->kb2, to_u, "melee window");
        }
    }
}

// Wisdom's shots / Master's orbs (bdx emitters: period 8 / 14 / 12 per range, attack radius 80 / 80 / 60, effect al010.17): GUESS = speed 30 cm/f, homing on the nearest enemy, spawn 150 cm up
static void gl_proj_spawn(const GlStage *S2, int e, const SmEn *en, int n, const float gp[3], float to_u) {
    GlProj *P = NULL;
    for (int i = 0; i < GL_NPROJ; i++) if (!GL.pj[i].on) { P = &GL.pj[i]; break; }
    if (!P) return;
    memset(P, 0, sizeof *P);
    float fx = sinf(GL.face), fz = cosf(GL.face), side = (e - (S2->ne - 1) * 0.5f) * 70.0f;   // GUESS: the emitters are bones; spread them sideways
    P->on = true, P->p[0] = gp[0] + (fz * side) * to_u, P->p[1] = SM.floor_s + 150.0f * to_u, P->p[2] = gp[2] + (-fx * side) * to_u;
    float bd = SM_LEASH_CM;
    for (int k = 0; k < n; k++) {
        if (en[k].life <= 0) continue;
        float d = sm_dist_cm(en[k].pos, gp, to_u);
        if (d < bd) bd = d, P->tgt = en[k].actor;
    }
    P->atkp = S2->e_atkp, P->power = S2->e_power, P->react = S2->e_react, P->kb1 = S2->e_kb1, P->kb2 = S2->e_kb2, P->rad = S2->e_rad, P->life = 90.0f, P->fx = -1;
    int id = sora_fx_id("gn_fx_17");
    if (id >= 0) P->fx = sora_fx_spawn(id, P->p, GL.face, to_u), P->fxg = P->fx >= 0 && P->fx < 32 ? g_fx_gen[P->fx] : 0;
    GL.n_proj++;
}
static void gl_proj_end(GlProj *P) {
    P->on = false;
    if (fx_mine(P->fx, P->fxg) && sora_fx_alive(P->fx)) sora_fx_fadeout(P->fx, 8.0f);
    P->fx = -1;
}
static void gl_proj_tick(const SmEn *en, int n, float kh, float to_u) {
    for (int i = 0; i < GL_NPROJ; i++) {
        GlProj *P = &GL.pj[i];
        if (!P->on) continue;
        P->life -= kh;
        const SmEn *T = NULL;
        for (int k = 0; k < n; k++) if (en[k].actor == P->tgt && en[k].life > 0) T = &en[k];
        float dir[3] = {sinf(GL.face), 0, cosf(GL.face)};
        if (T) {
            float g[3] = {T->pos[0] - P->p[0], T->pos[1] + 0.9f - P->p[1], T->pos[2] - P->p[2]}, l = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
            if (l > 1e-4f) dir[0] = g[0] / l, dir[1] = g[1] / l, dir[2] = g[2] / l;
        }
        float step = 30.0f * kh * to_u;
        for (int c = 0; c < 3; c++) P->p[c] += dir[c] * step;
        if (P->fx >= 0) { if (fx_mine(P->fx, P->fxg) && sora_fx_alive(P->fx)) sora_fx_move(P->fx, P->p, atan2f(dir[0], dir[2])); else P->fx = -1; }
        if (T) {
            float d = sqrtf((T->pos[0] - P->p[0]) * (T->pos[0] - P->p[0]) + (T->pos[1] + 0.9f - P->p[1]) * (T->pos[1] + 0.9f - P->p[1]) + (T->pos[2] - P->p[2]) * (T->pos[2] - P->p[2])) / to_u;
            if (d <= P->rad + 40.0f) {
                gl_hit_one(T->actor, T->life, T->pos, P->p, P->atkp, P->power, P->react, P->kb1, P->kb2, to_u, "projectile");
                gl_proj_end(P);
                continue;
            }
        }
        if (P->life <= 0) gl_proj_end(P);
    }
}
static void gl_proj_clear(void) { for (int i = 0; i < GL_NPROJ; i++) if (GL.pj[i].on) gl_proj_end(&GL.pj[i]); }

// ---- the gate (menu greying / start)
static const char *gl_gate(int st) {
    if (!SMD()->genie) return "this summon has no version Limit";
    if (SM.st != SM_ACTIVE) return "Genie is not out and ready";
    if (GL.st) return "a Genie Limit is already running";
    if (SM.hold || SM.call_on || SM.hidden) return "Sora is in the call / hidden";
    if (SMX.pend || SM.atk) return "Genie is changing his form";
    if (!(SM.t > 0)) return "the summon gauge is empty (bdx L4296: the Limit is offered while the summon's Drive time > 0)";
    if (st != LINK_GROUND || S.act != ACT_IDLE || S.special != SP_NONE) return "Sora is not free on the ground";
    if (pl_hold_party()) return "a party Limit is running";
    return NULL;
}
static bool gl_can(void) { return gl_gate(FP.last_st) == NULL; }
static void gl_request(void) { GL.req = 1; }
// the version-switch menu entry (cmd 605 / 535..538): Genie out, idle, no Limit
static bool gl_menu(void) { return SMD()->genie && sm_out(); }
static int gl_switch_state(void) { return SM.st == SM_ACTIVE && !SM.hold && !SM.call_on && !SMX.pend && !SM.atk && !GL.st && !sm_fps_busy() ? 0 : 5; }

// stage-2 command (543 / 545 / 547 / 549): offered in the Triangle slot while the stage-1 follow-up window is open
static bool gl_cmd2_ok(void) {
    const GlVer *V = gl_ver();
    return GL.st == 1 && GL.stage == 1 && GL.time > 0 && SM.cclock >= V->f26;
}
static bool gl_prompt(void) { return gl_cmd2_ok(); }
static const char *gl_prompt_name(void) { return gl_ver()->name2; }
static void gl_input(uint64_t *press) {
    if (GL.st != 1) return;
    if (*press & FP_TRI_BTN) GL.pr_tri = true;
    *press &= ~(uint64_t)FP_TRI_BTN;
}

static void gl_stage_begin(int stage, const float feet[3], float to_u, const SmEn *en, int n) {
    const GlVer *V = &GLV[GL.ver - 1];
    const GlStage *G = &V->st[stage - 1];
    GL.stage = stage, GL.nhit = 0, GL.pr_tri = false, GL.rprev[0] = GL.rprev[1] = GL.rprev[2] = 0;
    GL.timer[0] = GL.timer[1] = GL.timer[2] = 0;
    memcpy(GL.org, feet, sizeof GL.org);
    GL.ybase = stage == 2 ? GL.ylast : 0;
    // face the lock-on target, else the nearest enemy in reach, else Sora's facing (GUESS: the capture is attached to Sora's frame, 0x4002)
    float face = S.face;
    uint64_t tg = 0;
    if (g_lock.actor) for (int k = 0; k < n; k++) if (en[k].actor == g_lock.actor && en[k].life > 0) tg = g_lock.actor;
    float bd = SM_LEASH_CM;
    for (int k = 0; k < n && !tg; k++) if (en[k].life > 0) { float d = sm_dist_cm(en[k].pos, feet, to_u); if (d < bd) bd = d, tg = en[k].actor; }
    for (int k = 0; k < n; k++) if (en[k].actor == tg) face = atan2f(en[k].pos[0] - feet[0], en[k].pos[2] - feet[2]);
    GL.face = S.face = SM.face = face;
    SM.hold = SM.call_on = true, SM.cslot = G->sora_slot, SM.cclock = 0;
    SM.pos[0] = feet[0], SM.pos[2] = feet[2], SM.floor_s = SM.ray_y = feet[1], SM.ray_ok = false, SM.yoff = 0;
    SM.atk = false, SM.nhit = 0, SM.win = -1;
    sm_play(G->sora_slot + 1);
    SM.blend = -1, SM.clock = 0;
    sm_fx_clear();
    gl_aura_kill();
    gl_proj_clear();
    if (SM.body) SM.off(SM.body());
    sm_cues_slot(SMC_SUMMON, SM.slot, -1, 0, SM.pos);
    sm_cues_slot(SMC_SORA, SM.cslot, -1, 0, feet);
    LOG("[summon] gl STAGE %d (%s %s): Sora %d + Genie %d (%.0f f), face %.2f, hover base %.0f cm, target %llx\n", stage, V->name1, stage == 1 ? V->name1 : V->name2, G->sora_slot, G->sora_slot + 1,
        gl_frames(G->sora_slot, true, 100), face, GL.ybase, (unsigned long long)tg);
}

static void gl_start(const float feet[3], float to_u, const SmEn *en, int n) {
    GL.ver = SMX.ver >= 1 && SMX.ver <= 4 ? SMX.ver : 1;
    const GlVer *V = &GLV[GL.ver - 1];
    float cost = (float)V->cost * FP_RATE, before = SM.t;   // cmd Cost x sstm ChangeTimeAddRate 12 (VERIFIED for FPS Mode's cmd 529, the same unit)
    SM.t -= cost;
    if (SM.t < 0) SM.t = 0;
    GL.gauge_spent = before - SM.t;
    GL.st = 1, GL.time = GL.time_ov > 0 ? (float)GL.time_ov : (float)V->time, GL.cmd_req = 0, GL.n_hits = GL.n_proj = 0, GL.ylast = 0;
    lock_release("Genie Limit");
    LOG("[summon] gl START %s (cmd %d): gauge %.0f -> %.0f frames (cost %d x %.0f, spent %.0f), follow-up %s (cmd %d, group %d) from stage frame %.0f for %.0f f\n", V->name1, V->cmd1, before, SM.t, V->cost,
        FP_RATE, GL.gauge_spent, V->name2, V->cmd2, 535 + GL.ver - 1, V->f26, GL.time);
    gl_stage_begin(1, feet, to_u, en, n);
}

static void gl_end(const char *why, const float feet[3], float to_u) {
    LOG("[summon] gl END (%s) stage %d: %d hit(s), %d shot(s), gauge now %.0f\n", why, GL.stage, GL.n_hits, GL.n_proj, SM.t);
    gl_proj_clear();
    GL.st = 0, GL.stage = 0, GL.sy = 0, GL.dmove[0] = GL.dmove[1] = 0;
    SM.hold = SM.call_on = false;
    if (SM.acall > 0 && SM.off) SM.off(SM.acall);
    sm_fx_clear();
    float gp[3] = {SM.pos[0], 0, SM.pos[2]};
    SM.slot = -1, SM.atk = false;
    if (feet && hypotf(gp[0] - feet[0], gp[2] - feet[2]) / to_u < 700.0f) {   // he stays where his motion ended (he is attached to Sora there), else beside Sora
        SM.floor_s = SM.ray_y = feet[1], SM.placed = true, SM.ray_ok = false, SM.yoff = 0;
        sm_play(SMS_IDLE);
    } else if (feet) {
        sm_place_beside(feet, to_u, "Genie Limit ended");
        sm_play(SMS_IDLE);
    }
    SM.blend = -1, SM.band = PBAND_STAND, SM.pause = 30;
}
static void gl_abort(const char *why) {
    if (GL.st) LOG("[summon] gl abort/end (%s)\n", why);
    gl_proj_clear();
    GL.st = 0, GL.req = 0, GL.cmd_req = 0, GL.stage = 0, GL.sy = 0, GL.dmove[0] = GL.dmove[1] = 0;
}

// ---- Sora's call-actor clock (sm_tick, instead of the generic call clock) + his root travel (hover y now, the horizontal step for gl_move)
static void gl_call_clock(float kh) {
    const GlStage *G = gl_stage();
    float fr = gl_frames(SM.cslot, true, 100.0f), now = SM.cclock + kh, f[3];
    if (sm_feet(f)) sm_cues_slot(SMC_SORA, SM.cslot, SM.cclock, now > fr ? fr : now, f);
    SM.cclock = now > fr ? fr : now;
    if (GL.st == 1 && GL.stage == 1 && SM.cclock >= GLV[GL.ver - 1].f26) GL.time -= kh;   // range 45 (limit timer paused) ended; Time runs
    const Mot *m = mot_find(g_sm_cc, g_sm_ncc, G->sora_slot);   // Sora's own root: Link moves with it
    float r[3], d[3];
    gl_root(m, SM.cclock, r);
    float dr[3] = {r[0] - GL.rprev[0], r[1] - GL.rprev[1], r[2] - GL.rprev[2]};
    gl_rot(dr, d, GL.face, 1.0f);   // cm
    if (S.act != ACT_IDLE) GL.dmove[0] = GL.dmove[1] = 0;   // (s139 review) off the ground sora.c's ground step (gl_move) does not run: no stored burst for the landing
    else GL.dmove[0] += d[0], GL.dmove[1] += d[2];
    memcpy(GL.rprev, r, sizeof GL.rprev);
    float y = GL.ybase + r[1];
    if (y < 0) y = 0;
    float left = fr - SM.cclock;   // the last 20 f of the last stage ease the hover back to the ground (GUESS: KH2's capture just ends; here no pop below the floor)
    GL.sy = y, GL.ylast = y;
    if (left < 20.0f && (GL.stage == 2 || GL.time <= 0)) GL.sy = y * (left / 20.0f);
}
static float gl_yoff_cm(void) { return GL.st ? GL.sy : 0; }
static void gl_move(float move[2], float kh) {   // sora.c, after the root block: Sora's root travel (VERIFIED equal-ish curves: 250 cm Valor 1, ~700 cm Valor 2, ...)
    (void)kh;
    if (!GL.st) return;
    move[0] += GL.dmove[0], move[1] += GL.dmove[1];
    GL.dmove[0] = GL.dmove[1] = 0;
}

static bool gl_cmd_ok(int cmd) {
    if (cmd >= 535 && cmd <= 538) return gl_switch_state() == 0;
    return GL.st == 1 && gl_cmd2_ok() && cmd == gl_ver()->cmd2;
}

// ---- every driven frame (sm_fps_late routes here while Genie is the current row). true = Genie's draw is handled here
static bool gl_late(float kh, float to_u, int st) {
    float feet[3];
    if (SM.st == SM_OFF) {
        if (GL.st) gl_abort("summon gone");
        if (GL.req) GL.req = 0, LOG("[summon] gl refused: no summon is out\n");
        gl_aura_kill();
        return false;
    }
    bool has_feet = sm_feet(feet);
    if (has_feet) memcpy(FP.last_feet, feet, sizeof feet), FP.have_feet = true;
    SmEn en[64];
    int n = 0;
    if (GL.req) {
        GL.req = 0;
        const char *why = has_feet ? gl_gate(st) : "no Link matrix";
        const GlVer *V = &GLV[(SMX.ver >= 1 && SMX.ver <= 4 ? SMX.ver : 1) - 1];
        if (!why && !(SM.slot_pose && SM.ach > 0 && SM.acall > 0 && SM.slot_pose(SM.ach, V->st[0].sora_slot + 1, 0, g_sm_from) && SM.slot_pose(SM.acall, V->st[0].sora_slot, 0, g_sm_from)
                      && SM.slot_pose(SM.ach, V->st[1].sora_slot + 1, 0, g_sm_from) && SM.slot_pose(SM.acall, V->st[1].sora_slot, 0, g_sm_from)))
            why = "the Limit pose slots are missing (geniecall_pose.bin 253-267 / genie_pose.bin 254-268: install the sumdata bakes)";
        if (why) { LOG("[summon] gl refused: %s\n", why); sora_sfx_event("error", 1.0f); }
        else { n = sm_enemies(en, 64); gl_start(feet, to_u, en, n); }
    }
    if (!GL.st) { gl_aura_tick(to_u); return false; }
    if (!has_feet) return false;
    if (!n) n = sm_enemies(en, 64);
    if (!SM.call_on) { gl_end("Sora was hit", feet, to_u); return false; }   // sm_tick cut the call short
    // the stage-2 command: the Triangle press or the live key, only inside the window (range 26 group, Time left)
    if (GL.pr_tri) GL.cmd_req = gl_ver()->cmd2;
    GL.pr_tri = false;
    if (GL.cmd_req) {
        int c = GL.cmd_req;
        GL.cmd_req = 0;
        if (gl_cmd_ok(c) && c == gl_ver()->cmd2) {
            LOG("[summon] gl CMD %d (%s) accepted at stage-1 frame %.0f (Time left %.0f f)\n", c, gl_ver()->name2, SM.cclock, GL.time);
            gl_stage_begin(2, feet, to_u, en, n);
        } else GLOG("[summon] gl cmd %d refused (%s)\n", c, GL.stage != 1 ? "stage 2 / not running" : GL.time <= 0 ? "Time over" : "the follow-up window is not open (range 26)");
    }
    const GlStage *G = gl_stage();
    int gslot = G->sora_slot + 1;
    float c0 = SM.clock, fr = gl_frames(gslot, false, 100.0f);
    SM.clock += kh;
    if (SM.clock > fr) SM.clock = fr;
    // Genie's place = the stage origin + his own root (x left, y up, z ahead; Sora and he end at the same point, VERIFIED equal end values for Valor / Wisdom / Final)
    float gr[3], gw[3];
    gl_root(sm_mot(gslot), SM.clock, gr);
    gl_rot(gr, gw, GL.face, to_u);
    SM.pos[0] = GL.org[0] + gw[0], SM.pos[2] = GL.org[2] + gw[2];
    SM.face = GL.face;
    SM.yoff = 0;
    float gy = GL.ybase + gr[1];
    if (gy < 0) gy = 0;
    float at[3] = {SM.pos[0], SM.floor_s + gy * to_u, SM.pos[2]};
    sm_cues_slot(SMC_SUMMON, SM.slot, c0, SM.clock, at);
    // hits: windows + projectile emitters
    gl_windows(G, en, n, SM.pos, SM.clock, to_u);
    for (int e = 0; e < G->ne; e++) {
        if (SM.clock < G->e_a[e] || SM.clock > G->e_b[e]) continue;
        GL.timer[e] -= kh;
        if (GL.timer[e] <= 0) { GL.timer[e] += G->e_period; if (GL.timer[e] < 0) GL.timer[e] = 0; gl_proj_spawn(G, e, en, n, at, to_u); }
    }
    gl_proj_tick(en, n, kh, to_u);
    // end of the stage: the motion is never cut (bdx L3941: after friend_end_limit it waits for the capture); stage 1 without the follow-up ends the Limit
    if (SM.clock >= fr && SM.cclock >= gl_frames(SM.cslot, true, fr)) { gl_end(GL.stage == 2 ? "stage 2 over" : GL.time <= 0 ? "stage 1 over, no follow-up" : "stage 1 over", feet, to_u); return false; }
    if (SM.ach > 0 && SM.pose && !SM.hidden) {
        SM.shadow(SM.ach, SM.floor_s, blob_str(0.55f));
        SM.pose(SM.ach, SM.slot, SM.clock, at, SM.face, to_u);
    }
    return true;
}

static void gl_hud_prompt(void) {   // smn_hud: the Triangle slot (KH2's Reaction Command box, as the party Limits' prompts)
    if (gl_prompt()) { rcs_show(gl_prompt_name(), false); GL.shown = true; }
    else GL.shown = false;
}

static bool gl_live_set(const char *key, int v) {
    if (!strcmp(key, "sgl")) { if (v) gl_request(); }
    else if (!strcmp(key, "sglcmd")) {   // ONE-SHOT: stage 2 (543 / 545 / 547 / 549) or a version switch (535..538)
        if (v >= 535 && v <= 538) { if (SMD()->genie && SM.st == SM_ACTIVE) sm_genie_change(v - 534); else LOG("[summon] gl sglcmd %d ignored: Genie is not out\n", v); }
        else if (v && GL.st) GL.cmd_req = v;
        else if (v) LOG("[summon] gl sglcmd %d ignored (no Genie Limit is running)\n", v);
    }
    else if (!strcmp(key, "gllog")) GL.log = v != 0, LOG("[summon] gl log %s\n", GL.log ? "on" : "off");
    else if (!strcmp(key, "gltime")) GL.time_ov = v < 0 ? 0 : v > 6000 ? 6000 : v, LOG("[summon] gl Limit time %s\n", GL.time_ov ? "overridden" : "= the cmd row's Time");
    else return false;
    return true;
}

// the version menu's pick (cmd 535..538 -> Genie 90A + genie_change_form): sora_summon_pp.h's sm_genie_change
static bool gl_switch(int v) { return sm_genie_change(v); }

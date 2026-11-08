// (s89 b4-limit) Limit Form's own commands (KH2 FM): the command menu's Magic slot becomes "Limit" (cmd 707, SubMenu 12)
// with four MP-cost Limit commands (VERIFIED kh2_first/03system.bin cmd rows, Execute 707):
//   698 Sonic Rave MP 60, 701 Last Arcanum MP 75 (KH1's Ars Arcanum), 704 Strike Raid MP 65, 683 Infinity MP 80
//   (KH1's Ragnarok; the PC data calls them Last Arcanum / Infinity). Row order = Limit's plrp row (0x83: 569 570 571 572).
// Each is a chain of P_EX100_KH1F.mset motions (PHH kh2ctl FORM_LIMITS: each group carries its command id as the range-26
// trigger, VERIFIED in our bake): Sonic Rave 211 K400 -> 212 K401 "Rave" (a 571 cm dash, atkp 1878 Power 150) -> 213
// K402 "Finish" (1879 Power 300); Last Arcanum 221 K410 (7 hits 1883-1889) -> 222..226 K411-K415 "Bash" (1890-1894
// Power 150) -> 227 K416 "The End" (1895 Power 250); Strike Raid 231 K420 -> 232 K421 the throw "Raid" (the Keyblade
// flies at its frame trigger 9 = frame 8: atkp 1866 Power 200) -> 234 K423 the catch -> 233 K422 "Judgment" (trigger 9
// at frame 27, atkp 1867 Power 500) -> 234; Infinity 241 K430 -> 242..245 K431-K434 (1857-1860 Power 100) -> 246 K435
// (1861 Power 200) -> 247 K436 "Impact" (frame trigger 7 p1 100 at frame 2 = the shots: 17 x atkp 1862 Power 65).
// The follow-ups (KH2: the Triangle prompts Rave / Finish, Bash / The End, Raid / Judgment, Shoot / Impact / Giga Impact)
// are the ATTACK button here (no reaction-command prompt is drawn yet): a press any time during a step queues the next
// one; a step that ends with nothing queued ends the Limit (KH2: "Execute the attacks quickly, or the limit will be
// canceled!"). Live "set lmauto 1" = the whole chain without presses. The repeat counts (3 Raves, 1 extra Raid) and
// "Impact" (17 shots, not Shoot 10 / Giga Impact 25) are a GUESS: KH2 runs these on its 'limi' BDX script (P_EX100_KH1F.mdlx).
// MP: the cost is paid at the start (mp_change: below 0 = MP Charge, as a spell); the rows grey out in MP Charge / at 0 MP
// and show "can't afford" (state 2) below the cost (KH2's entry rule re:0x401D60 for spells; for Limits a GUESS).
// The KH1F effects (P_EX100_KH1F.a.us: 20 Infinity charge, 23 shot, 32 the Raid trail, 34 the catch) are not in fx.bin:
// fx names limit_shot / limit_raid_trail are tried (fxbake rows, reviewer) else the flights are invisible but hit.
// Live: lmauto 0/1 | lmshotdmg <pct of KH2 power> (Infinity's shots, default 40 like Wisdom's) | lmraiddmg <pct> | lmlog.
#pragma once

typedef struct { int16_t motion; uint8_t need, act; } LmStep;  // need: 0 auto, 1 a press, 2 a press + the finisher
enum { LMA_NONE, LMA_RAID, LMA_JUDGMENT, LMA_SHOTS };
typedef struct { const char *name; int cmd, cost, label; int n; LmStep st[8]; } LmCmd;
static const LmCmd LM_CMDS[LM_N] = {
    {"Sonic Rave", 698, 60, 24, 5, {{211, 0, 0}, {212, 1, 0}, {212, 1, 0}, {212, 1, 0}, {213, 2, 0}}},
    {"Last Arcanum", 701, 75, 26, 7, {{221, 0, 0}, {222, 1, 0}, {223, 1, 0}, {224, 1, 0}, {225, 1, 0}, {226, 1, 0}, {227, 2, 0}}},
    {"Strike Raid", 704, 65, 25, 7, {{231, 0, 0}, {232, 0, LMA_RAID}, {234, 0, 0}, {232, 1, LMA_RAID}, {234, 0, 0},
                                     {233, 2, LMA_JUDGMENT}, {234, 0, 0}}},
    {"Infinity", 683, 80, 27, 7, {{241, 0, 0}, {242, 1, 0}, {243, 1, 0}, {244, 1, 0}, {245, 1, 0}, {246, 1, 0}, {247, 2, LMA_SHOTS}}},
};
#define LM_LABEL_TOP 23   // cmd_s89_limit.bin label "Limit" (23) + the rows 24..27; an older atlas = 11 "Limit Form"
#define LM_TARGET_R 1000  // cm: a Limit's target when nothing is locked (GUESS; INFINITY_SEARCH 1000 in the script)

static struct {
    bool on, queued, spawned, face;
    int cmd, step, req;
    uint64_t target;
} LM = {.req = -1};
static int g_lm_auto = 0, g_lm_shot_pct = 40, g_lm_raid_pct = 100, g_lm_log = 1;
static int g_lm_hold = 1;  // (s89 review) live "set lmhold 0": a step's free frames + stick / B / roll end the Limit (old)
#include "sora_limfx.h"  // (s91 b5-limfx: the Limit fx + SE at the motions' frame triggers)

static bool lm_labels(void) { return sp_label_n() >= 28; }
static int lm_top_label(void) { return lm_labels() ? LM_LABEL_TOP : 11; }
static int lm_top_state(void) { return IN_LIMIT && !mp_charging() && MP.mp > 0 ? 0 : 5; }
static int lm_row_state(int i) {
    if (i < 0 || i >= LM_N || !IN_LIMIT || mp_charging() || MP.mp <= 0 || LM.on) return 5;
    return MP.mp < LM_CMDS[i].cost ? 2 : 0;
}
// hud_update: the Limit page's rows (labels, states, MP costs), the count
static int lm_hud_rows(uint8_t *label, uint8_t *state, int16_t *cost) {
    for (int i = 0; i < LM_N; i++)
        label[i] = (uint8_t)(lm_labels() ? LM_CMDS[i].label : 11), state[i] = (uint8_t)lm_row_state(i), cost[i] = (int16_t)LM_CMDS[i].cost;
    return LM_N;
}

// menu_tick's page 3 (the Limit page, opened from the top page's slot 1 in Limit Form): up / down, A = the command
static int lm_menu_tick(uint64_t *press) {
    if (!IN_LIMIT) { M.sub = -1, M.cursor = 0; return -1; }  // (reverted with the page open)
    if (*press & BTN_UP) M.sub_cursor = (M.sub_cursor + LM_N - 1) % LM_N, sora_sfx_event("cursor", 1);
    if (*press & BTN_DOWN) M.sub_cursor = (M.sub_cursor + 1) % LM_N, sora_sfx_event("cursor", 1);
    if (*press & (BTN_JUMP | BTN_LEFT)) {
        *press &= ~BTN_JUMP;
        M.sub = -1, M.cursor = 1;
        sora_sfx_event("cancel", 1), LOG("[limit] menu: Limit page closed\n");
    } else if (*press & BTN_ATTACK) {
        *press &= ~BTN_ATTACK;
        int i = M.sub_cursor;
        if (M.dead > 0 || lm_row_state(i) >= 4) {
            sora_sfx_event("error", 1);
            LOG("[limit] menu: %s greyed (MP %.0f, charge %d, running %d)\n", LM_CMDS[i].name, MP.mp, mp_charging(), LM.on);
            return -1;
        }
        return CMD_LIMIT + i;
    }
    return -1;
}

static void lm_request(int i) { if (i >= 0 && i < LM_N) LM.req = i, LOG("[limit] command %s requested\n", LM_CMDS[i].name); }

// (the flights) Strike Raid's Keyblade (one) and Infinity's shots: KH cm / KH frames, BotW units for positions
typedef struct { bool on, back; int kind, atkp, power, pct, nhit; float t, life, speed, turn, r, spin, pos[3], dir[3];
                 uint64_t target, hit[16]; int fx; } LmShot;
#define LM_SHOTS 32
static LmShot g_lms[LM_SHOTS];
static void (*g_lm_weapon_at)(int, const float *, int, const float *, float, float);
static bool g_lm_weapon_tried;
static void lm_blade_hand(void) {  // the Keyblade back in Sora's hand (weapon_at pos NULL)
    if (!g_lm_weapon_at) return;
    if (FM.actor[FORM_LIMIT] > 0) g_lm_weapon_at(FM.actor[FORM_LIMIT], NULL, 0, NULL, 0, 0);
    for (int o = 1; o < 5; o++)  // (s146b: every LOADED Limit outfit body too, never loads one; an outfit change mid-throw leaves no stale blade)
        if (ofx_peek(o, FORM_LIMIT) > 0) g_lm_weapon_at(ofx_peek(o, FORM_LIMIT), NULL, 0, NULL, 0, 0);
}
static void lm_shots_kill(void) {
    for (int i = 0; i < LM_SHOTS; i++)
        if (g_lms[i].on) { if (g_lms[i].fx >= 0) sora_fx_kill(g_lms[i].fx); g_lms[i].on = false; }
    lm_blade_hand();
}

static void lm_end(const char *why) {
    if (!LM.on) return;
    LOG("[limit] %s ended at step %d/%d (%s)\n", LM_CMDS[LM.cmd].name, LM.step + 1, LM_CMDS[LM.cmd].n, why);
    LM.on = LM.queued = false;
}
static void lm_abort(const char *why) {  // (fm_revert / death / load: the flights go too)
    LM.req = -1;
    lm_end(why);
    lm_shots_kill();
}

static void lm_play_step(void) {
    const LmStep *s = &LM_CMDS[LM.cmd].st[LM.step];
    play(s->motion), S.special = SP_LIMIT, S.next = 0;
    LM.spawned = false, LM.face = true;
    if (s->need == 2) ll_finish(LM_CMDS[LM.cmd].name);  // (KH2: 1 Limit EXP per finishing move unleashed)
    if (g_lm_log)
        LOG("[limit] %s step %d/%d: motion %d slot %d%s%s\n", LM_CMDS[LM.cmd].name, LM.step + 1, LM_CMDS[LM.cmd].n, s->motion,
            S.mot ? S.mot->slot : -1, s->need == 2 ? " FINISHER" : s->need ? " (follow-up)" : "",
            s->act == LMA_RAID ? " throw" : s->act == LMA_JUDGMENT ? " Judgment throw" : s->act == LMA_SHOTS ? " shots" : "");
}

// sora.c step 2, before the motion-end check: the step that just ended -> the next one, or the end of the Limit
static void lm_chain(void) {
    if (!LM.on) return;
    if (S.special != SP_LIMIT) { lm_end("interrupted (hit / fall / revert)"); return; }
    if (!S.mot || S.clock < S.mot->frames) return;
    const LmCmd *c = &LM_CMDS[LM.cmd];
    int nx = LM.step + 1;
    while (nx < c->n && !mot_of(c->st[nx].motion)) nx++;  // (a motion missing from the bake: skipped)
    if (nx >= c->n) { lm_end("done"); return; }
    if (c->st[nx].need && !LM.queued && !g_lm_auto) { lm_end("no follow-up press: canceled"); return; }
    if (c->st[nx].need) LM.queued = false;
    LM.step = nx;
    lm_play_step();
}

// sora.c step 3 (the presses, with the targets of the frame): start a requested command; while one runs, the attack
// press = the follow-up and the pad's jump / roll / attack go to nobody else; the step's facing to its target
static void lm_tick_press(uint64_t *press, const Target *tg, int ntg) {
    if (LM.req >= 0) {
        int i = LM.req;
        LM.req = -1;
        bool ground = S.act == ACT_IDLE && S.takeoff <= 0 && !S.air_frames;
        bool busy = S.special && S.special != SP_ATTACK && S.special != SP_GUARD;
        const LmCmd *c = &LM_CMDS[i];
        if (!IN_LIMIT || LM.on || lm_row_state(i) >= 4 || !ground || busy || !mot_of(c->st[0].motion)) {
            sora_sfx_event("error", 1);
            LOG("[limit] %s refused (in Limit %d, running %d, state %d, ground %d, special %d, motion %d %s)\n", c->name,
                IN_LIMIT, LM.on, lm_row_state(i), ground, S.special, c->st[0].motion, mot_of(c->st[0].motion) ? "ok" : "MISSING");
        } else {
            float mp0 = MP.mp;
            mp_change(-c->cost);
            const Target *t = g_lock.actor ? target_of(tg, ntg, g_lock.actor) : NULL;
            if (!t) t = target_auto(tg, ntg, S.face, LM_TARGET_R);
            LM.on = true, LM.cmd = i, LM.step = 0, LM.queued = false, LM.target = t ? t->actor : 0;
            M.dead = 8, M.sub = -1, M.cursor = 0, M.sub_cursor = 0;  // (re:0x3B21D0: executed)
            LOG("[limit] LIMIT %s (cmd %d): MP %.0f -> %.0f%s, target %llx\n", c->name, c->cmd, mp0, MP.mp,
                mp_charging() ? " (MP Charge)" : "", (unsigned long long)LM.target);
            lm_play_step();
        }
    }
    if (!LM.on) return;
    if (*press & BTN_ATTACK) {
        if (!LM.queued && g_lm_log) LOG("[limit] follow-up press at step %d (clock %.1f)\n", LM.step + 1, S.clock);
        LM.queued = true;
    }
    *press &= ~(BTN_ATTACK | BTN_JUMP | BTN_ROLL);
    if (LM.face) {  // (each step turns to the Limit's target: KH2's Limit motions home on it; GUESS = a snap)
        LM.face = false;
        const Target *t = LM.target ? target_of(tg, ntg, LM.target) : NULL;
        if (!t || t->life <= 0) {
            t = target_auto(tg, ntg, S.face, LM_TARGET_R);
            LM.target = t ? t->actor : 0;
        }
        if (t && t->hd > 1) S.face = atan2f(t->d[0], t->d[1]);
    }
}

// one flight hits one enemy (KH2's atkp row: power x pct, its reaction / knockback)
static void lm_hit(LmShot *o, uint64_t actor, int32_t life, const float epos[3], float to_u) {
    int pw = (o->power * o->pct + 50) / 100;
    int dmg = sl_hit(actor, life, pw < 1 ? 1 : pw, g_dmg_base, 0);
    int react = 3, kb1 = 0x7fff, kb2 = 0;
    ah_atkp(o->atkp, &react, &kb1, &kb2);
    int f54 = ah_class(react) != 4 ? F54_SMALL : g_fin_big || ah_small(actor) ? g_fin_f54 : 0x11;
    f54 = ah_hit_f54(actor, epos, f54, react, to_u);
    linkpos_damage_enemy(actor, dmg, f54);
    ah_hit(actor, epos, o->pos, react, kb1, kb2, f54, to_u, "limit");
    sora_fx_event(o->kind == LMA_SHOTS ? "hit" : "hit_finish", o->pos, 0, to_u);
    sora_sfx_hit(o->atkp, 1.0f, epos);
    dg_hit(o->atkp, actor);
    LOG("[limit] HIT %llx (life %d) by %s: atkp %d power %d x %d%% -> damage %d f54 %#x\n", (unsigned long long)actor, life,
        o->kind == LMA_RAID ? "Raid" : o->kind == LMA_JUDGMENT ? "Judgment" : "Infinity shot", o->atkp, o->power, o->pct, dmg, f54);
}

static LmShot *lm_shot_new(void) {
    for (int i = 0; i < LM_SHOTS; i++) if (!g_lms[i].on) { memset(&g_lms[i], 0, sizeof g_lms[i]); g_lms[i].fx = -1; return &g_lms[i]; }
    return NULL;
}

// the step's flight starts at its trigger frame: Raid / Judgment = frame trigger 9 (the release), the shots = trigger 7
static void lm_spawn(const float feet[3], float to_u) {
    const LmStep *s = &LM_CMDS[LM.cmd].st[LM.step];
    if (LM.spawned || s->act == LMA_NONE || !S.mot) return;
    float at = s->act == LMA_SHOTS ? mot_frame_trigger(S.mot, 7, 2) : mot_frame_trigger(S.mot, 9, s->act == LMA_JUDGMENT ? 27 : 8);
    if (S.clock < at) return;
    LM.spawned = true;
    float tip[3];
    cast_tip(feet, to_u, tip);
    if (s->act == LMA_RAID || s->act == LMA_JUDGMENT) {
        for (int i = 0; i < LM_SHOTS; i++)
            if (g_lms[i].on && (g_lms[i].kind == LMA_RAID || g_lms[i].kind == LMA_JUDGMENT)) g_lms[i].on = false;  // (one blade)
        LmShot *o = lm_shot_new();
        if (!o) return;
        bool j = s->act == LMA_JUDGMENT;
        // projectiles_kh2 THROW_*: speed 30 / frame, homing 0.087266 rad / frame; the Raid's hit radius 160 (attack_set_radius)
        o->on = true, o->kind = s->act, o->atkp = j ? 1867 : 1866, o->power = j ? 500 : 200, o->pct = g_lm_raid_pct;
        o->speed = 30, o->turn = 0.087266f, o->r = 160, o->life = 90, o->target = LM.target;
        memcpy(o->pos, tip, sizeof o->pos);
        o->dir[0] = sinf(S.face), o->dir[1] = 0, o->dir[2] = cosf(S.face);
        if (lf_has("limit_raid_trail")) o->fx = sora_fx_event("limit_raid_trail", tip, S.face, to_u);
        if (g_lf_on) {  // (s91: limit.32's SE, se999 #50)
            int se = sora_sfx_event("limit_raid_trail", 1.0f);
            if (g_lm_log) LOG("[limit] limfx: throw fx %d, SE %d\n", o->fx, se);
        }
        LOG("[limit] %s: the Keyblade flies (atkp %d power %d, target %llx)\n", j ? "Judgment" : "Raid", o->atkp, o->power,
            (unsigned long long)o->target);
    } else {
        // Infinity's shots (projectiles_kh2 INFINITY_*): from the Keyblade tip (bind 0x4003; GUESS cast_tip), first out in a
        // ring about his facing (INFINITY_UP turned by 2 pi i / n), speed 16, homing 0.087266 on targets within 1000 cm
        // (the lock target first: 3 shots), life 150 + rand 30, hit radius 80, atkp 1862 Power 65
        uint64_t ea[64];
        float ep[64][3];
        int32_t el[64];
        int ne = linkpos_enemies(ea, ep, el, 64), nt = 0;
        uint64_t tl[64];
        for (int i = 0; i < ne; i++) {
            float dx = (ep[i][0] - feet[0]) / to_u, dz = (ep[i][2] - feet[2]) / to_u;
            if (el[i] > 0 && dx * dx + dz * dz < 1000.0f * 1000.0f) tl[nt++] = ea[i];
        }
        int n = 17, made = 0;  // (Impact: INFINITY_SHOTS 685 = 17)
        float fx = sinf(S.face), fz = cosf(S.face);
        for (int k = 0; k < n; k++) {
            LmShot *o = lm_shot_new();
            if (!o) break;
            float a = 6.283185f * k / n, up = cosf(a), side = sinf(a);  // (the ring in the plane across his facing)
            o->on = true, o->kind = LMA_SHOTS, o->atkp = 1862, o->power = 65, o->pct = g_lm_shot_pct;
            o->speed = 16, o->turn = 0.087266f, o->r = 80, o->life = 150 + (float)(rand() % 31);
            o->target = k < 3 && LM.target ? LM.target : nt ? tl[k % nt] : 0;
            memcpy(o->pos, tip, sizeof o->pos);
            o->dir[0] = fz * side, o->dir[1] = up, o->dir[2] = -fx * side;
            if (lf_has("limit_shot")) o->fx = sora_fx_event("limit_shot", tip, S.face, to_u);
            made++;
        }
        LOG("[limit] Infinity: %d shots (%d targets in 1000 cm)%s\n", made, nt, lf_has("limit_shot") ? "" : " - no limit_shot fx (fxbake): invisible");
    }
}

// every frame (sora.c, next to wshot_tick): the step's flight start, the flights, the thrown Keyblade's draw
static void lm_frame(float kh, float to_u, const float feet[3]) {
    if (!g_lm_weapon_tried) {
        g_lm_weapon_tried = true;
        g_lm_weapon_at = (void (*)(int, const float *, int, const float *, float, float))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_actor_weapon_at");
        LOG("[limit] thrown Keyblade draw: sora_draw_actor_weapon_at %s\n", g_lm_weapon_at ? "found" : "not in this exe (it stays in his hand)");
    }
    lf_tick(feet, to_u);  // (s91 b5-limfx)
    if (LM.on && IN_LIMIT) lm_spawn(feet, to_u);
    uint64_t actor[64];
    float pos[64][3];
    int32_t life[64];
    int n = -1;
    bool blade_out = false;
    for (int si = 0; si < LM_SHOTS; si++) {
        LmShot *o = &g_lms[si];
        if (!o->on) continue;
        if (n < 0) n = linkpos_enemies(actor, pos, life, 64);
        o->t += kh;
        bool blade = o->kind == LMA_RAID || o->kind == LMA_JUDGMENT;
        float aim[3];
        bool have = false;
        if (blade && o->back) {  // (back to his chest; caught within 60 cm: GUESS)
            aim[0] = feet[0] - o->pos[0], aim[1] = feet[1] + 100 * to_u - o->pos[1], aim[2] = feet[2] - o->pos[2], have = true;
            float d = sqrtf(aim[0] * aim[0] + aim[1] * aim[1] + aim[2] * aim[2]);
            if (d < 60 * to_u || o->t > o->life + 60) {
                o->on = false;
                if (o->fx >= 0) sora_fx_kill(o->fx);
                if (lf_has("limit_catch")) sora_fx_event("limit_catch", o->pos, S.face, to_u);
                if (g_lm_log) LOG("[limit] the Keyblade is caught (%.0f KH frames)\n", o->t);
                continue;
            }
        } else {
            for (int i = 0; i < n && o->target; i++)
                if (actor[i] == o->target && life[i] > 0)
                    aim[0] = pos[i][0] - o->pos[0], aim[1] = pos[i][1] + 90 * to_u - o->pos[1], aim[2] = pos[i][2] - o->pos[2], have = true;
        }
        if (have) {  // (vector_homing re:0x3B7200: turn by <= o->turn rad per frame; the way back: no limit, GUESS)
            float d = sqrtf(aim[0] * aim[0] + aim[1] * aim[1] + aim[2] * aim[2]);
            if (d > 1e-4f) {
                for (int c = 0; c < 3; c++) aim[c] /= d;
                float dot = o->dir[0] * aim[0] + o->dir[1] * aim[1] + o->dir[2] * aim[2];
                float ang = acosf(dot > 1 ? 1 : dot < -1 ? -1 : dot), lim = (blade && o->back) ? 9 : o->turn * kh;
                if (ang <= lim) memcpy(o->dir, aim, sizeof o->dir);
                else if (ang > 1e-5f) {
                    float u = lim / ang, sa = sinf(ang), a = sinf((1 - u) * ang) / sa, b = sinf(u * ang) / sa, l = 0;
                    for (int c = 0; c < 3; c++) o->dir[c] = a * o->dir[c] + b * aim[c], l += o->dir[c] * o->dir[c];
                    l = sqrtf(l);
                    for (int c = 0; c < 3; c++) o->dir[c] /= l;
                }
            }
        }
        for (int c = 0; c < 3; c++) o->pos[c] += o->dir[c] * o->speed * kh * to_u;
        if (o->fx >= 0) sora_fx_move(o->fx, o->pos, atan2f(o->dir[0], o->dir[2]));
        float r = o->r * to_u;
        for (int i = 0; i < n && o->on; i++) {
            float dx = pos[i][0] - o->pos[0], dz = pos[i][2] - o->pos[2], dy = o->pos[1] - pos[i][1];
            if (life[i] <= 0 || dx * dx + dz * dz > r * r || dy < -50 * to_u || dy > 220 * to_u) continue;
            int k = 0;
            while (k < o->nhit && o->hit[k] != actor[i]) k++;
            if (k < o->nhit) continue;
            if (o->nhit < 16) o->hit[o->nhit++] = actor[i];
            lm_hit(o, actor[i], life[i], pos[i], to_u);
            if (o->kind == LMA_SHOTS) { o->on = false; if (o->fx >= 0) sora_fx_kill(o->fx); }  // (a shot ends at its hit)
            else if (actor[i] == o->target && !o->back) o->back = true, o->nhit = 0;  // (the Keyblade turns back at its target)
        }
        if (!o->on) continue;
        if (blade && !o->back && o->t >= 24) o->back = true, o->nhit = 0;  // (no target reached: back after 24 frames, GUESS)
        if (!blade && o->t >= o->life) { o->on = false; if (o->fx >= 0) sora_fx_kill(o->fx); continue; }
        if (blade) {
            blade_out = true;
            o->spin += 0.6f * kh;  // (the spin: GUESS, about the vertical only - weapon_at takes a yaw)
            if (g_lm_weapon_at && FM.actor[FORM_LIMIT] > 0) g_lm_weapon_at(ofx_shown_limit(), NULL, 0, o->pos, o->spin, to_u);
        }
    }
    static bool was_out;
    if (was_out && !blade_out) lm_blade_hand();
    was_out = blade_out;
}

static bool lm_live_set(const char *key, int v) {
    if (!strcmp(key, "lmauto")) g_lm_auto = v != 0, LOG("[limit] follow-ups %s\n", g_lm_auto ? "automatic (whole chain)" : "on the attack press (KH2)");
    else if (!strcmp(key, "lmshotdmg")) g_lm_shot_pct = v < 1 ? 1 : v > 400 ? 400 : v;
    else if (!strcmp(key, "lmraiddmg")) g_lm_raid_pct = v < 1 ? 1 : v > 400 ? 400 : v;
    else if (!strcmp(key, "lmlog")) g_lm_log = v != 0;
    else if (!strcmp(key, "limfx")) lf_set(v);  // (s91 b5-limfx: Limit fx + SE, default 1)
    else if (!strcmp(key, "lmhold")) g_lm_hold = v != 0, LOG("[limit] a running Limit %s\n", g_lm_hold ? "owns Sora to its end" : "ends in a free range on stick / B / roll (old)");
    else if (!strcmp(key, "limit")) lm_request(v);  // (test: set limit <0..3> = the Limit page's row, in Limit Form)
    else return false;
    return true;
}

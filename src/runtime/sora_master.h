// (s89 b4-master) Master Form's own moves (DLL, hot reload): the Aerial Dodge and the body attack windows of its air
// finisher. Included by sora.c right after play() (needs arc / enter_jump / play / mot_of and the hit helpers).
// Notes: D:/SoraBotW/recon/S89_HELPER_MASTER.md.
//
// Aerial Dodge (KH2 re:0x3CB9F0 as PHH kh2ctl._air_dodge ports it): B (KH2's jump) pressed again while airborne in
// ACTION_JUMP, nothing else playing: with the growth at LV1-4 once per jump (+0x124 bit 1: armed when the air is entered
// from a ground ACTION, re:0x3CBD30), LV5 always. A jump of fmab AirDodgeHeight from where he is, along the stick at
// AirDodgeSpeed (0 with the stick neutral), motion 202 (A160, Master slot 810) from frame 0, ACTION_JUMP entered again at
// once, Fall at the apex (sora.c's jump rule, now also for 202), the short hop on an early release (+0x120 bit 16).
// Only Master grows it (fmlv growth byte: sora_master_lv.h ML_GROWTH); no glide in Master (KH2: Final's).
// Live: set mdodge 0 = off.
//
// D334 body windows: Master's air finisher D334 (motion 184, slot 736, 254 frames) has NO Keyblade hit windows; its
// damage is two range-10 windows on the BODY (P_EX100_TRIF.mset slot 736, read s89 with sorabake info): frames 20..210
// atkp 1100 (Power 10, Interval 20 = it hits the same enemy again every 20 frames, react 5, Kind 2) and 210..218 atkp
// 1101 (Power 150, react 11 = blow, Kind 2), both on collision 3 (the shape is in the model's collision list: NOT read;
// GUESS = a cylinder MA_BODY_R around him, MA_BODY_H up / down from his middle). Live: set mbody 0 = off.
#pragma once
enum { MOT_AIR_DODGE = 202 };  // A160 (PHH kh2ctl AIR_DODGE); Master slot 810, 12 frames, loops from 4

static struct { bool ready; int prev_act; } MA = {.prev_act = -1};

// every driven frame, in the presses (sora.c, after the glide arm)
static void ma_air_dodge(uint64_t press, const float sdir[2], float sspeed, float z_up, const float p[3]) {
    bool air = ACT_AIR(S.act);
    if (air && MA.prev_act >= 0 && !ACT_AIR(MA.prev_act)) MA.ready = true;  // (into the air from a ground ACTION)
    if (!air) MA.ready = false;
    MA.prev_act = S.act;
    if (!IN_MASTER || !g_ma_dodge || !(press & BTN_JUMP) || S.act != ACT_JUMP || S.special || S.takeoff > 0) return;
    int lv = ml_dodge_lv();
    if (lv < 1 || lv > 5 || !(lv > 4 || MA.ready)) {
        LOG("[master] Aerial Dodge: not again in this jump (LV%d)\n", lv);
        return;
    }
    if (!mot_of(MOT_AIR_DODGE)) {
        LOG("[master] Aerial Dodge: motion %d not in the bake\n", MOT_AIR_DODGE);
        return;
    }
    if (!dodgestam_pay("Master Aerial Dodge")) return;  // (s95: 1/4 wheel; out = no dodge, MA.ready kept)
    const MlDodgeRow *r = &ML_DODGE[lv - 1];
    float w = sspeed > 0 ? r->speed : 0;
    play(MOT_AIR_DODGE);  // (S.special stays SP_NONE: a jump motion, as A003)
    arc(r->height);
    S.hvel[0] = sdir[0] * w, S.hvel[1] = sdir[1] * w;
    enter_jump(z_up, true);
    S.jump_held = true, S.jump_released = false;  // (+0x120 bit 16: an early release = the short hop)
    MA.ready = false;
    LOG("[master] AERIAL DODGE LV%d: +%.0f cm, %.0f cm/frame at %.3f %.3f %.3f (slot %d)\n", lv, r->height, w, p[0], p[1],
        p[2], S.mot ? S.mot->slot : -1);
}

typedef struct { uint16_t slot, start, end, atkp, power, interval; uint8_t kind; } MaBodyWin;
static const MaBodyWin MA_BODY[] = {
    {736, 20, 210, 1100, 10, 20, 2},  // D334: the spin (00battle.bin atkp 1100: react 5, kb 150 / 250, el 5)
    {736, 210, 218, 1101, 150, 0, 2}, // D334: the last burst (atkp 1101: react 11, kb 50 / 20)
};
#define MA_BODY_R 250.0f  // cm around his middle, horizontal (GUESS: collision 3 not read)
#define MA_BODY_H 200.0f  // cm up / down from his middle (GUESS)
static struct { int swing, n; uint64_t actor[32]; uint8_t win[32]; float next[32]; } MB = {.swing = -1};

// Master's attack effects: the swing's own effect frame triggers (KH2 frame trigger id 1 = an element of the actor's
// .a.us; P_EX100_TRIF: D334 slot 736 @20 -> master.20 the spin (241 f), @200 -> .21 the burst; D333 slot 732 @0..108
// -> .25..32), packed as "master_fx_<n>" (tools_s89/master/fxbake.py; a fx.bin without them = nothing, logged once).
// All root-bound (bone 0x4000): spawned at his feet and moved with him. Live: set mfx 0 = off.
// (g_ma_fx: sora_master_lv.h)
static struct { int h[8], swing; } MFX = {{-1, -1, -1, -1, -1, -1, -1, -1}, -1};
// (s89 review) the live attack fx fade out when their swing is gone (a hit / revert / death cut D334 short): before,
// ma_fx_tick ran only in Master, so after a revert the 241-frame spin stayed where he left it (not moved, not killed).
// Live "set mfxdrop 0" = the old rule (g_ma_fx_drop: sora_master_lv.h).
static void ma_fx_drop(const char *why) {
    int n = 0;
    for (int i = 0; i < 8; i++)
        if (MFX.h[i] >= 0) { if (sora_fx_alive(MFX.h[i])) sora_fx_fadeout(MFX.h[i], 8), n++; MFX.h[i] = -1; }
    if (n) LOG("[master] attack fx: %d faded (%s)\n", n, why);
}
static void ma_fx_tick(float clock_old, float to_u) {
    if (g_ma_fx_drop && (!IN_MASTER || S.special != SP_ATTACK)) {
        bool any = false;
        for (int i = 0; i < 8; i++) any |= MFX.h[i] >= 0;
        if (any) ma_fx_drop(!IN_MASTER ? "out of Master" : "the swing ended / was cut");
    }
    if (!IN_MASTER) return;  // (s89 review: called every frame outside Master too - nothing else to do there)
    float lm[12];
    if (!linkpos_link_mtx(lm)) return;
    float feet[3] = {lm[3], lm[7], lm[11]};
    for (int i = 0; i < 8; i++)
        if (MFX.h[i] >= 0) { if (sora_fx_alive(MFX.h[i])) sora_fx_move(MFX.h[i], feet, S.face); else MFX.h[i] = -1; }
    if (!IN_MASTER || !g_ma_fx || S.special != SP_ATTACK || !S.mot) return;
    float from = clock_old;
    if (MFX.swing != S.swing) MFX.swing = S.swing, from = -1;  // (a new swing: its frame-0 triggers too)
    for (int k = 0; k < S.mot->nft; k++) {
        const uint8_t *t = S.mot->ft + k * 8;
        uint16_t f;
        int16_t p1;
        memcpy(&f, t, 2), memcpy(&p1, t + 4, 2);
        if (t[2] != 1 || p1 < 20 || p1 > 32 || (float)f <= from || (float)f > S.clock) continue;
        char nm[24];
        snprintf(nm, sizeof nm, "master_fx_%d", p1);
        int h = sora_fx_event(nm, feet, S.face, to_u), j = 0;
        while (j < 8 && MFX.h[j] >= 0) j++;
        if (h >= 0 && j < 8) MFX.h[j] = h;
        static bool said;
        if (h < 0 && !said) said = true, LOG("[master] fx %s not in fx.bin (fx_s89_master.bin rows not installed?)\n", nm);
    }
}

// after blade_hits' two blades (sora.c): enemies inside the body shape while a window runs; a window with an Interval
// hits the same enemy again every Interval KH frames, one without it once per swing
static int ma_body_hits(float clock_old, float to_u) {
    ma_fx_tick(clock_old, to_u);
    if (!g_ma_body || S.special != SP_ATTACK || !S.mot) return 0;
    const MaBodyWin *w0 = NULL;
    for (size_t k = 0; k < sizeof MA_BODY / sizeof *MA_BODY; k++)
        if (MA_BODY[k].slot == S.mot->slot) { w0 = &MA_BODY[k]; break; }
    if (!w0) return 0;
    float lm[12];
    if (!linkpos_link_mtx(lm)) return 0;
    float feet[3] = {lm[3], lm[7], lm[11]}, mid = feet[1] + 90 * to_u;
    if (MB.swing != S.swing) MB.swing = S.swing, MB.n = 0;
    uint64_t actor[64];
    float pos[64][3];
    int32_t life[64];
    int n = -1, hits = 0;
    for (size_t k = 0; k < sizeof MA_BODY / sizeof *MA_BODY; k++) {
        const MaBodyWin *w = &MA_BODY[k];
        if (w->slot != S.mot->slot || S.clock < w->start || clock_old > w->end) continue;
        if (n < 0) n = linkpos_enemies(actor, pos, life, 64);
        for (int i = 0; i < n; i++) {
            if (life[i] <= 0) continue;
            float br, bh;
            enemy_body(actor[i], &br, &bh);
            float hx = pos[i][0] - feet[0], hz = pos[i][2] - feet[2], hd = sqrtf(hx * hx + hz * hz);
            float dy = pos[i][1] + bh * 0.5f - mid;
            if (hd - br > MA_BODY_R * to_u || fabsf(dy) > MA_BODY_H * to_u + bh * 0.5f || wall_hides(pos[i])) continue;
            int j = 0;
            while (j < MB.n && !(MB.actor[j] == actor[i] && MB.win[j] == k)) j++;
            if (j < MB.n && (!w->interval || S.clock < MB.next[j])) continue;
            if (j == MB.n) {
                if (MB.n >= 32) continue;
                MB.actor[j] = actor[i], MB.win[j] = (uint8_t)k, MB.n++;
            }
            MB.next[j] = S.clock + (float)w->interval;
            int dmg = sl_hit(actor[i], life[i], w->power, g_dmg_base, 0);
            dmg = ab_finisher_dmg(w->kind, dmg);
            bool last = !w->interval;  // (the burst flings; the spin's ticks are plain hits: GUESS, react 5 has no BotW twin)
            int f54 = !last ? F54_SMALL : g_fin_big || ah_small(actor[i]) ? g_fin_f54 : 0x11;
            int react = last ? 11 : 5, kb1 = 0x7fff, kb2 = 0;
            ah_atkp(w->atkp, &react, &kb1, &kb2);
            f54 = ah_hit_f54(actor[i], pos[i], f54, react, to_u);
            linkpos_damage_enemy(actor[i], dmg, f54);
            ah_hit(actor[i], pos[i], feet, react, kb1, kb2, f54, to_u, "master");
            float at[3] = {pos[i][0], pos[i][1] + bh * 0.5f, pos[i][2]};
            sora_fx_event(last ? "hit_finish" : "hit", at, S.face, to_u);
            sora_sfx_hit(w->atkp, 1.0f, pos[i]);
            dg_hit(w->atkp, actor[i]);
            S.hit = true, hits++;
            LOG("[master] BODY HIT %llx (life %d) slot %d frames %d..%d atkp %d power %d at clock %.1f: damage %d f54 %#x, "
                "%.0f cm from him\n", (unsigned long long)actor[i], life[i], w->slot, w->start, w->end, w->atkp, w->power,
                S.clock, dmg, f54, hd / to_u);
        }
    }
    return hits;
}

// (s91 b5-limfx) Limit Form's KH2 effects + sounds (recon/S91_HELPER_LIMFX.md). Root cause of "no fx / no SE": s89 baked
// the Limit moveset but no Limit effect or sound data, and wired no frame-trigger player for it:
//  - fx: fx.bin had no limit.* elements (the P_EX100_KH1F.a.us set, PHH recording names "limit.<n>"). The ability motions
//    carry KH2's frame trigger id 1 with the element number as param (VERIFIED in limit_motion.bin: slot 844 K400 @0 -> 40;
//    848 K401 @2/6 -> 28/26/27; 852 K402 @6/16 -> 31/29/30; 884 K410 @0 -> 41; 924 K420 @0 -> 42; 936 K423 @0 -> 33; 964 K430
//    @0 -> 43; 988 K436 @2 -> 21; 992 K437 @2 -> 22; finishers 616 @28/36 -> 38/39, 620 Zantetsuken @0..26 -> 47..58, 732
//    @14/58 -> 44/45). Only the Master player (sora_master.h) read such triggers; nothing did for Limit.
//  - SE: sfx.bin held the per-slot triggers of Sora's mset only (slots <= 838); Limit's own K4xx slots 844..992 had none (their
//    swoosh / footstep / voice triggers), and no Limit effect element SE (json "effects" limit.N +0x18 SE: se999 #45.. #56).
// New data (tools_s91/limfx): fx_s91_limfx.bin = fx.bin + "limit_fx_<n>" (the trigger elements), limit_charge (.20, Infinity's
// charge, K435 trigger 9 @28), limit_shot (.23, 180 f cap), limit_raid_trail (.32, 80 f cap), limit_catch (.34);
// sfx_s91_limfx.bin = sfx.bin + the K4xx slot triggers + names limit_fx_<n> (the element's SE) + the 23 atkp rows 1857..1895.
// Each fx element is root-bound (0x4000: his feet), Keyblade-tip-bound (0x4003: cast_tip), or on a skeleton bone (the
// recording's bone, the same convention as the Drive entry sets); 0x4005 = GUESS the Keyblade hand (bone 178, as Master's).
// KH2 plays an element's own SE when the element starts (same GUESS as the Drive sphere SE, s87c): fired with the fx.
// Live: set limfx 0 = off (no trigger fx / SE, no shot / trail / catch fx, no K4xx slot SE = the old silent Limit).
#pragma once

static const struct { int16_t el, bone; } LF_EL[] = {  // bone: -1 feet (0x4000 / 0x0), -2 tip (0x4003), >= 0 skeleton bone
    {21, -2}, {22, -2}, {26, -1}, {27, -1}, {28, -1}, {29, -1}, {30, -1}, {31, -1}, {33, 178}, {38, -2}, {39, -1}, {40, -1},
    {41, 0x43}, {42, 0x43}, {43, -1}, {44, -1}, {45, -1}, {47, 0x43}, {48, 0xaa}, {49, 0x4e}, {50, 0x02}, {51, 0xb1}, {52, 0x55},
    {53, 0x28}, {54, 0x0b}, {55, -2}, {56, -2}, {57, -1}, {58, -1}};
#define LF_N 24
static struct { int h, bone; } g_lf[LF_N];
static float g_lf_pose[POSE_BUF_BONES * 12];
static struct { int slot, swing; float clock, frames; bool act; int logged, spawned; } LF = {.slot = -1, .swing = -1};
static bool g_lf_said_fx, g_lf_said_se, g_lf_init;

static void lf_init(void) {
    g_lf_init = true;
    for (int i = 0; i < LF_N; i++) g_lf[i].h = -1;
}
static bool lf_has(const char *name) { return g_lf_on && sora_fx_id(name) >= 0; }
static void lf_drop(const char *why) {
    int n = 0;
    for (int i = 0; i < LF_N; i++)
        if (g_lf[i].h >= 0) { if (sora_fx_alive(g_lf[i].h)) sora_fx_fadeout(g_lf[i].h, 8), n++; g_lf[i].h = -1; }
    if (n && g_lm_log) LOG("[limit] limfx: %d fx faded (%s)\n", n, why);
}
static void lf_pos(int bone, bool pose_ok, const float feet[3], float to_u, float out[3]) {
    if (bone == -2) { cast_tip(feet, to_u, out); return; }
    int nb = sora_draw_bone_count();
    if (bone < 0 || !pose_ok || bone >= nb) { memcpy(out, feet, 12); return; }
    const float *q = g_lf_pose + bone * 12 + 9;  // (as sora_form.h fm_fx_bone_world)
    float s = sinf(S.face), c = cosf(S.face);
    out[0] = feet[0] + (c * q[0] + s * q[2]) * to_u, out[1] = feet[1] + q[1] * to_u, out[2] = feet[2] + (-s * q[0] + c * q[2]) * to_u;
}
static void lf_spawn_el(const char *name, int bone, bool pose_ok, const float feet[3], float to_u, float at) {
    float p[3];
    lf_pos(bone, pose_ok, feet, to_u, p);
    int h = sora_fx_event(name, p, S.face, to_u), j = 0;
    if (h < 0 && !g_lf_said_fx) g_lf_said_fx = true, LOG("[limit] limfx: %s not in fx.bin (fx_s91_limfx.bin not installed?)\n", name);
    if (h >= 0) {
        while (j < LF_N && g_lf[j].h >= 0) j++;
        if (j < LF_N) g_lf[j].h = h, g_lf[j].bone = bone;
    }
    int id = sora_sfx_id(name), v = id >= 0 ? sora_sfx_play(id, 1.0f) : -1;
    if (id < 0 && !g_lf_said_se)
        g_lf_said_se = true, LOG("[limit] limfx: no SE '%s' in sfx.bin (sfx_s91_limfx.bin not installed? elements without a KH2 SE are silent too)\n", name);
    LF.spawned++;
    if ((h >= 0 || v >= 0) && LF.logged++ < 8)  // (rate limit: 8 lines per swing; s91 review: nothing spawned = only the one-time warnings, no line per trigger with the old fx.bin / sfx.bin)
        LOG("[limit] limfx: %s slot %d @%.0f bone %d -> fx %d, SE %d\n", name, S.mot ? S.mot->slot : -1, at, bone, h, v);
}

// every frame from lm_frame (Limit Form): follow the live fx, fire the motion's fx triggers (+ their SE)
static void lf_tick(const float feet[3], float to_u) {
    if (!g_lf_init) lf_init();
    if (!g_lf_on || !IN_LIMIT) {
        lf_drop(!g_lf_on ? "limfx off" : "out of Limit Form");
        LF.act = false, LF.slot = -1;
        return;
    }
    bool act = (S.special == SP_LIMIT || S.special == SP_ATTACK) && S.mot;
    // (s91 review) the pose fills one matrix per bone of the current body: only into the buffer when it fits (as sora.c's ragdoll)
    bool pose_ok = act && sora_draw_bone_count() <= POSE_BUF_BONES && sora_draw_slot_pose(S.mot->slot, S.clock, g_lf_pose);
    for (int i = 0; i < LF_N; i++) {
        if (g_lf[i].h < 0) continue;
        if (!sora_fx_alive(g_lf[i].h)) { g_lf[i].h = -1; continue; }
        float p[3];
        lf_pos(g_lf[i].bone, pose_ok, feet, to_u, p);
        sora_fx_move(g_lf[i].h, p, S.face);
    }
    if (!act) {
        if (LF.act && LF.clock + 2 < LF.frames) lf_drop("the move was cut short");
        LF.act = false;
        return;
    }
    float from = LF.clock;
    if (!LF.act || LF.slot != S.mot->slot || LF.swing != S.swing || S.clock < LF.clock) from = -1, LF.logged = 0;  // (a new step: its frame-0 triggers too)
    LF.act = true, LF.slot = S.mot->slot, LF.swing = S.swing, LF.clock = S.clock, LF.frames = S.mot->frames;
    for (int k = 0; k < S.mot->nft; k++) {
        const uint8_t *t = S.mot->ft + k * 8;
        uint16_t f;
        int16_t p1;
        memcpy(&f, t, 2), memcpy(&p1, t + 4, 2);
        if ((float)f <= from || (float)f > S.clock) continue;
        if (t[2] == 1) {
            int e = -1;
            for (size_t m = 0; m < sizeof LF_EL / sizeof *LF_EL; m++)
                if (LF_EL[m].el == p1) { e = (int)m; break; }
            if (e < 0) continue;
            char nm[24];
            snprintf(nm, sizeof nm, "limit_fx_%d", p1);
            lf_spawn_el(nm, LF_EL[e].bone, pose_ok, feet, to_u, f);
        } else if (t[2] == 9 && S.mot->slot == 984) {  // K435 (Infinity's charge): trigger 9 @28 = limit.20 at the Keyblade tip
            lf_spawn_el("limit_charge", -2, pose_ok, feet, to_u, f);
        }
    }
}

static void lf_set(int v) {
    g_lf_on = v != 0;
    if (!g_lf_on) lf_drop("limfx off");
    LOG("[limit] limfx %s (fx.bin %s, sfx.bin K4xx triggers %s)\n", g_lf_on ? "ON" : "OFF",
        sora_fx_id("limit_fx_26") >= 0 ? "has the Limit rows" : "WITHOUT the Limit rows", sora_sfx_id("limit_fx_43") >= 0 ? "packed" : "NOT packed");
}

// (s107 b19-abilfx) KH2's own effects + sounds for the combo-ability attacks (Guard Break, Explosion, Finishing Leap, Counterguard,
// Flash Step, Vicinity Break, Retaliating Slash, Aerial Finish, Magnet Burst). Recon: recon/S107_HELPER_ABILFX.md.
// Root cause of "Magnet Burst / Explosion have no fx / sfx": the mset slots of these attacks carry KH2's frame trigger id 1 with a
// P_EX100.a.us element number as param (VERIFIED in the base sora_motion.bin: slot 660 @24/28/30 -> 31/30/32, 664 @0 -> 44..47
// and @34 -> 48, 668 @2/28 -> 33/34, 676 @8 -> 53, @18 -> 54/55/56, 680 @14 -> 49/50, 684 @18 -> 40/41, 688 @0 -> 39,
// 776 @18/32/48/80 -> 35/36/37/38, 788 @10/60 -> 51/52), but fx.bin held none of sora.30..56 and nothing in the DLL fired a base
// Sora fx trigger (only the Limit / Final / Master / Wisdom players did). Their play_sound / footstep / voice triggers were
// already in sfx.bin and already played; what was silent is the elements' own SE (json "effects" sora.N +0x18).
// Data (tools_s107/abilfx): fx_s107.bin = fx.bin + abil_fx_<N> (sora.N, 25 elements); sfx_s107.bin = sfx.bin + name rows abil_fx_<N>
// (the element's SE; elements with no KH2 SE have none). Bind per element = the recording's bone (same convention as LF_EL):
// -1 feet (0x4000), -2 Keyblade tip (0x4003), else a skeleton bone. KH2 plays an element's own SE when the element starts
// (GUESS, as the Drive sphere s87c and Limit s91). Base Sora only (a Drive Form uses its own mset and fx set).
// Live: set abilfx 0|1 (default 1) = these fx + their SE. Without the new fx.bin: one log line, nothing else changes.
#pragma once

static const struct { int16_t el, bone; } AF_EL[] = {
    {30, -1}, {31, -1}, {32, -1}, {33, -2}, {34, -1}, {35, -1}, {36, -1}, {37, -1}, {38, -1}, {39, -1}, {40, -1}, {41, 0x43},
    {44, -2}, {45, 0x43}, {46, 0xaa}, {47, 0x4e}, {48, -1}, {49, -1}, {50, -1}, {51, -1}, {52, -1}, {53, 0x12}, {54, 0x12},
    {55, 0x2f}, {56, -1}};
#define AF_N 24
static struct { int h, bone; } g_af[AF_N];
static float g_af_pose[POSE_BUF_BONES * 12];
static struct { int slot, swing; float clock; bool act; int logged; } AF = {.slot = -1, .swing = -1};
static bool g_af_said_fx, g_af_said_se, g_af_init;

static void af_pos(int bone, bool pose_ok, const float feet[3], float to_u, float out[3]) {
    if (bone == -2) { cast_tip(feet, to_u, out); return; }
    int nb = sora_draw_bone_count();
    if (bone < 0 || !pose_ok || bone >= nb) { memcpy(out, feet, 12); return; }
    const float *q = g_af_pose + bone * 12 + 9;  // (as lf_pos / sora_form.h fm_fx_bone_world)
    float s = sinf(S.face), c = cosf(S.face);
    out[0] = feet[0] + (c * q[0] + s * q[2]) * to_u, out[1] = feet[1] + q[1] * to_u, out[2] = feet[2] + (-s * q[0] + c * q[2]) * to_u;
}
static void af_drop(const char *why) {
    int n = 0;
    for (int i = 0; i < AF_N; i++)
        if (g_af[i].h >= 0) { if (sora_fx_alive(g_af[i].h)) sora_fx_fadeout(g_af[i].h, 8), n++; g_af[i].h = -1; }
    if (n) LOG("[abilfx] %d fx faded (%s)\n", n, why);
}
static void af_spawn(int el, int bone, bool pose_ok, const float feet[3], float to_u, float at) {
    char nm[24];
    float p[3];
    snprintf(nm, sizeof nm, "abil_fx_%d", el);
    af_pos(bone, pose_ok, feet, to_u, p);
    int h = sora_fx_event(nm, p, S.face, to_u), j = 0;
    if (h < 0 && !g_af_said_fx) g_af_said_fx = true, LOG("[abilfx] %s not in fx.bin (fx_s107.bin not installed?)\n", nm);
    if (h >= 0) {
        while (j < AF_N && g_af[j].h >= 0) j++;
        if (j < AF_N) g_af[j].h = h, g_af[j].bone = bone;
    }
    int id = sora_sfx_id(nm), v = id >= 0 ? sora_sfx_play(id, 1.0f) : -1;
    if (id < 0 && !g_af_said_se && h >= 0) g_af_said_se = true, LOG("[abilfx] no SE rows in sfx.bin (sfx_s107.bin not installed? some elements have no KH2 SE)\n");
    if ((h >= 0 || v >= 0) && AF.logged++ < 8)
        LOG("[abilfx] %s slot %d @%.0f bone %d -> fx %d, SE %d\n", nm, S.mot ? S.mot->slot : -1, at, bone, h, v);
}

// every frame (sora.c, next to lm_frame): follow the live fx, fire the playing motion's element triggers (base Sora only)
static void af_tick(const float feet[3], float to_u) {
    if (!g_af_init) {
        g_af_init = true;
        for (int i = 0; i < AF_N; i++) g_af[i].h = -1;
    }
    bool act = g_af_on && !g_fmots_on && S.special == SP_ATTACK && S.mot && S.mot->nft;
    bool any = false;
    for (int i = 0; i < AF_N; i++) any |= g_af[i].h >= 0;
    if (!g_af_on && any) af_drop("abilfx off");
    if (!any && !act) { AF.act = false; return; }
    bool pose_ok = act && sora_draw_bone_count() <= POSE_BUF_BONES && sora_draw_slot_pose(S.mot->slot, S.clock, g_af_pose);
    for (int i = 0; i < AF_N; i++) {
        if (g_af[i].h < 0) continue;
        if (!sora_fx_alive(g_af[i].h)) { g_af[i].h = -1; continue; }
        float p[3];
        af_pos(g_af[i].bone, pose_ok, feet, to_u, p);
        sora_fx_move(g_af[i].h, p, S.face);
    }
    if (!act) { AF.act = false; return; }
    float from = AF.clock;
    if (!AF.act || AF.slot != S.mot->slot || AF.swing != S.swing || S.clock < AF.clock) from = -1, AF.logged = 0;  // (a new swing: its frame-0 triggers too)
    AF.act = true, AF.slot = S.mot->slot, AF.swing = S.swing, AF.clock = S.clock;
    for (int k = 0; k < S.mot->nft; k++) {
        const uint8_t *t = S.mot->ft + k * 8;
        uint16_t f;
        int16_t p1;
        memcpy(&f, t, 2), memcpy(&p1, t + 4, 2);
        if (t[2] != 1 || (float)f <= from || (float)f > S.clock) continue;
        for (size_t m = 0; m < sizeof AF_EL / sizeof *AF_EL; m++)
            if (AF_EL[m].el == p1) { af_spawn(p1, AF_EL[m].bone, pose_ok, feet, to_u, f); break; }
    }
}

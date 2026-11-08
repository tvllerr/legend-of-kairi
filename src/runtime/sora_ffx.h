// (s93 b7-ffx) Final Form swing fx + Limit Form (Last Arcanum ...) swing ribbons. Notes: recon/S93_HELPER_FFX.md.
// Root causes (VERIFIED in data):
//  1. RIBBON: fx.bin's weapon_trail windows (sora_fx_trail_window) were baked from Sora's BASE actor only (kh2_sounds actors.0).
//     Final / Limit motions looked the windows up by the form's raw mset slot: Final's 604 / 608 / 612 / 616 / 724 / 728 / 732
//     matched the BASE Sora's windows of the same number (wrong frames: e.g. 604 got [13,23] instead of Final's five windows
//     [8,17] [32,42] [55,84] ...), Limit's K4xx slots (884 = Last Arcanum K410 .. 908) matched nothing = no ribbon at all.
//     Fix: fx_s93_ffx.bin carries each form's own windows at slot + 10000 * (form + 1) (Final 50000+, Limit 30000+, the same
//     offset scheme as the form SE triggers); the DLL passes that slot. Old fx.bin = the raw slot (old behaviour).
//  2. FINAL FX: Final's motions carry KH2 frame trigger id 1 with the element number as param (final_motion.bin: 604 @58..84
//     -> 52..57; 612 @26 70 88 104 120 -> 20..24; 616 -> 25..40; 724 -> 58..61; 728 -> 62 63; 732 -> 42..51; 160 @0 -> 64).
//     Only the entry set (final.0..12, 100..102) was baked; final.20..64 (the glowing swing trails, auras, finisher bursts) were not,
//     and nothing played them. Packed as final_fx_<N> (+ SE of the same name), played here like sora_limfx.h.
// Live: set ffx 0/1 (Final: own windows + trigger fx, default 1), set lafx 0/1 (Limit: own windows, default 1).
#pragma once

static int g_ffx = 1, g_lafx = 1;
static int g_ffx_pack = -1;  // fx_s93_ffx.bin loaded (final_fx_20 present): 1 / 0 / -1 not asked yet

static bool ffx_pack(void) {
    if (g_ffx_pack < 0) {
        if (!sora_fx_ready()) return false;
        g_ffx_pack = sora_fx_id("final_fx_20") >= 0;
        if (!g_ffx_pack) LOG("[sora] ffx: fx.bin has no s93 form rows (fx_s93_ffx.bin not installed): Final / Limit ribbons use the old windows, no Final swing fx\n");
    }
    return g_ffx_pack > 0;
}

// the trail-window lookup for the playing motion (blade_hits_one): the form's own windows in Final / Limit when packed
static bool ffx_trail_win_form(int form, int slot, float clock) {  // (mp2-rib: form -1 = base Sora, else FORM_*; the remote Sora's windows)
    if (form == FORM_MICKEY && mk_fx_ok()) slot += 10000 * (form + 1);  // (s110m mickeyfx: his own windows, W_EX200 range 12)
    if (form == FORM_ROXAS && g_roxsfx && rox_fx_pack()) slot += 10000 * (form + 1);  // (s110 roxsfx: Roxas' own windows, W_EX010_RX range 12)
    if (((form == FORM_FINAL && g_ffx) || (form == FORM_LIMIT && g_lafx)) && ffx_pack()) slot += 10000 * (form + 1);
    return sora_fx_trail_window(slot, clock);
}
static bool ffx_trail_win(const Mot *m, float clock) { return ffx_trail_win_form(g_fmots_on ? (int)g_fform : -1, m->slot, clock); }

// Final's element bones (recording bone ids: 0x4000 root, 0x4005 right Keyblade hand = 178, 0x4006 left hand = 86, small = skeleton bone)
static const struct { int16_t el, bone; } FFX_EL[] = {
    {20, -1}, {21, -1}, {22, -1}, {23, -1}, {24, -1}, {25, 67}, {26, 177}, {27, 85}, {28, -1}, {29, 67}, {30, -1}, {31, -1},
    {32, -1}, {33, -1}, {34, -1}, {35, -1}, {36, -1}, {37, -1}, {38, -1}, {39, -1}, {40, -1}, {41, 0}, {42, -1}, {43, -1},
    {44, -1}, {45, -1}, {46, -1}, {47, -1}, {48, -1}, {49, -1}, {50, -1}, {51, -1}, {52, 178}, {53, 86}, {54, 178}, {55, 86},
    {56, 178}, {57, 86}, {58, -1}, {59, -1}, {60, -1}, {61, -1}, {62, -1}, {63, -1}, {64, -1}};
#define FFX_N 24
static struct { int h, bone; } g_ffxh[FFX_N];
static struct { int slot, swing; float clock, frames; bool act; int logged; } FFX = {.slot = -1, .swing = -1};
static bool g_ffx_init, g_ffx_said_fx, g_ffx_said_se;

static void ffx_drop(const char *why) {
    int n = 0;
    for (int i = 0; i < FFX_N; i++)
        if (g_ffxh[i].h >= 0) { if (sora_fx_alive(g_ffxh[i].h)) sora_fx_fadeout(g_ffxh[i].h, 8), n++; g_ffxh[i].h = -1; }
    if (n) LOG("[sora] ffx: %d fx faded (%s)\n", n, why);
}

// every frame (blade_hits): follow the live fx, fire the motion's fx triggers (+ their SE) in Final Form
static void ffx_tick(float to_u) {
    if (!g_ffx_init) {
        g_ffx_init = true;
        for (int i = 0; i < FFX_N; i++) g_ffxh[i].h = -1;
    }
    if (!g_ffx || !IN_FINAL || !ffx_pack()) {
        ffx_drop(!g_ffx ? "ffx off" : "out of Final Form");
        FFX.act = false, FFX.slot = -1;
        return;
    }
    float lm[12];
    bool act = S.special == SP_ATTACK && S.mot && S.mot->slot != 215 && linkpos_link_mtx(lm);
    float feet[3] = {act ? lm[3] : 0, act ? lm[7] : 0, act ? lm[11] : 0};
    bool pose_ok = act && sora_draw_bone_count() <= POSE_BUF_BONES && sora_draw_slot_pose(S.mot->slot, S.clock, g_lf_pose);
    if (act)
        for (int i = 0; i < FFX_N; i++) {
            if (g_ffxh[i].h < 0) continue;
            if (!sora_fx_alive(g_ffxh[i].h)) { g_ffxh[i].h = -1; continue; }
            float p[3];
            lf_pos(g_ffxh[i].bone, pose_ok, feet, to_u, p);
            sora_fx_move(g_ffxh[i].h, p, S.face);
        }
    if (!act) {
        if (FFX.act && FFX.clock + 2 < FFX.frames) ffx_drop("the move was cut short");
        FFX.act = false;
        return;
    }
    float from = FFX.clock;
    if (!FFX.act || FFX.slot != S.mot->slot || FFX.swing != S.swing || S.clock < FFX.clock) from = -1, FFX.logged = 0;
    FFX.act = true, FFX.slot = S.mot->slot, FFX.swing = S.swing, FFX.clock = S.clock, FFX.frames = S.mot->frames;
    for (int k = 0; k < S.mot->nft; k++) {
        const uint8_t *t = S.mot->ft + k * 8;
        uint16_t f;
        int16_t p1;
        memcpy(&f, t, 2), memcpy(&p1, t + 4, 2);
        if (t[2] != 1 || (float)f <= from || (float)f > S.clock) continue;
        int e = -1;
        for (size_t m = 0; m < sizeof FFX_EL / sizeof *FFX_EL; m++)
            if (FFX_EL[m].el == p1) { e = (int)m; break; }
        if (e < 0) continue;
        char nm[24];
        snprintf(nm, sizeof nm, "final_fx_%d", p1);
        float p[3];
        lf_pos(FFX_EL[e].bone, pose_ok, feet, to_u, p);
        int h = sora_fx_event(nm, p, S.face, to_u), j = 0;
        if (h < 0 && !g_ffx_said_fx) g_ffx_said_fx = true, LOG("[sora] ffx: %s not in fx.bin\n", nm);
        if (h >= 0) {
            while (j < FFX_N && g_ffxh[j].h >= 0) j++;
            if (j < FFX_N) g_ffxh[j].h = h, g_ffxh[j].bone = FFX_EL[e].bone;
        }
        int id = sora_sfx_id(nm), v = id >= 0 ? sora_sfx_play(id, 1.0f) : -1;
        if (id < 0 && !g_ffx_said_se) g_ffx_said_se = true, LOG("[sora] ffx: no SE '%s' in sfx.bin (sfx_s93_ffx.bin not installed? some elements have none in KH2)\n", nm);
        if ((h >= 0 || v >= 0) && FFX.logged++ < 8)
            LOG("[sora] ffx: %s slot %d @%d bone %d -> fx %d, SE %d\n", nm, S.mot->slot, (int)f, FFX_EL[e].bone, h, v);
    }
}

static void ffx_set(int v, int la) {
    // (s93 review: no ffx_tick from live_poll - "ffx 1" mid-swing fired every past trigger of the motion at once. On: the
    // playing swing counts as seen up to now; off: the live fx fade now)
    if (la) { g_lafx = v != 0; goto said; }
    g_ffx = v != 0;
    if (!g_ffx_init) {
        g_ffx_init = true;
        for (int i = 0; i < FFX_N; i++) g_ffxh[i].h = -1;
    }
    if (!g_ffx) ffx_drop("ffx off"), FFX.act = false, FFX.slot = -1;
    else if (S.mot) FFX.act = true, FFX.slot = S.mot->slot, FFX.swing = S.swing, FFX.clock = S.clock, FFX.frames = S.mot->frames;
said:
    LOG("[sora] live: %s %d (%s; fx.bin s93 rows %s)\n", la ? "lafx" : "ffx", v,
        la ? "Limit swing ribbons use Limit's own trail windows" : "Final: own trail windows + KH2 swing fx",
        g_ffx_pack > 0 ? "present" : g_ffx_pack == 0 ? "MISSING" : "not checked yet");
}

// ---- (s95 b9-fairfx) Final Form's Keyblade ribbons while jumping / falling / gliding.
// CAUSE: the swing ribbon (kbfx_trail, with each Keyblade's own look) was only drawn by blade_hits_one, which returns at once
// unless S.special is SP_ATTACK / SP_LIMIT (sora.c blade_hits_one: `h = (SP_ATTACK || SP_LIMIT) && S.mot ? hit_slot(..) : NULL`).
// Final's plain motions carry KH2 weapon_trail windows and blade bakes too (fx_s93_ffx.bin 50012 / 50016 / 50020 / 50032 / 50036 /
// 50064..50076 / 50160 / 50224.. / 50820..50832 = jump, fall, land, glide move / left / right / hover; final_hits_{r,l}_s92.bin
// has the same slots), but they run with S.special == SP_NONE, so no ribbon was ever drawn there. Here: no special, a motion
// whose slot has a blade bake -> the same kbfx_trail call (both blades), window from the form's own table. Ribbon only: no hit
// windows, no sparks (those motions have none). Live: set fairfx 0 = off (the old look: ribbons on swings only).
static int g_fairfx = 1;
static int g_fairfx_slot = -1, g_fairfx_open;
static int g_fairfx_logs;  // (s95 review: the "ribbons on" line is capped: ~3 a jump - jump / fall / land - and every glide turn)

// (s95 review) only with Final's own windows (ffx 1 + fx_s93_ffx.bin): without them ffx_trail_win falls back to the BASE
// Sora's windows of the same raw slot number, which s93 root cause 1 above showed are other motions' frames (ribbons at the
// wrong time), so no air ribbons then.
static void ffx_air_trail(float to_u) {
    if (!g_fairfx || !IN_FINAL || IN_ANTI || S.special != SP_NONE || !S.mot || S.motion < 0 || !g_ffx || !ffx_pack()) {
        g_fairfx_slot = -1;
        return;
    }
    float lm[12];
    if (!linkpos_link_mtx(lm)) return;
    float feet[3] = {lm[3], lm[7], lm[11]};
    bool on = ffx_trail_win(S.mot, S.clock), any = false;
    for (int blade = 0; blade < 2; blade++) {
        const HitSlot *h = hit_slot(S.mot->slot, blade);
        if (!h) continue;
        float now[6];
        blade_world(h, S.clock, feet, S.face, to_u, now);
        kbfx_trail(blade, now, now + 3, on);
        any = true;
    }
    if (any && (g_fairfx_slot != S.mot->slot || g_fairfx_open != (int)on)) {
        g_fairfx_slot = S.mot->slot, g_fairfx_open = on;
        if (on && (++g_fairfx_logs <= 60 || g_fairfx_logs % 100 == 0))
            LOG("[sora] fairfx: ribbons on motion %d (mset slot %d, Final's own window) clock %.1f, Keyblades 0x%x / 0x%x (#%d)\n",
                S.motion, S.mot->slot, S.clock, kbfx_item(0), kbfx_item(1), g_fairfx_logs);
    }
}

static void ffx_set_air(int v) {
    g_fairfx = v != 0, g_fairfx_slot = -1;
    LOG("[sora] live: fairfx %d (%s)\n", v, !g_fairfx ? "ribbons on swings only (old)"
        : g_ffx && ffx_pack() ? "Final: Keyblade ribbons also on jump / fall / glide" : "on, but ffx 0 / no fx_s93_ffx.bin: no air ribbons");
}

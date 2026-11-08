// (s91 b5-dodge) Base Sora's Aerial Dodge, the KH2 way: tap B again in the air = Aerial Dodge, hold B = the Glide (as before).
// KH2 rule (sora_master_lv.h header, re:0x3E8380 as the user's brief states it): base Sora's Aerial Dodge level = Master
// Form's growth level - 1 (ML_GROWTH[master LV - 1]; the fmlv growth byte 1,1,2,2,3,3,4). Master LV1-2 -> 0 = NO dodge (B in
// the air is the glide at once, as before), LV3-4 -> LV1, LV5-6 -> LV2, LV7 -> LV3. The level is read with ml_dodge_lv()
// like every other Master reader, so debugall (ML.exp maxed -> LV7 -> growth 4 -> base LV3) is consistent. The motion,
// height / speed rows (ML_DODGE), once-per-airtime rule (MA.ready, reset on landing / a new jump) are Master's own
// (sora_master.h ma_air_dodge); only the "is it a tap" gate and the base level are new here.
// Tap vs hold (GUESS): B released within g_bd_tap KH frames (default 8 = 133 ms; s91 review: counted in KH frames, the
// frame's kh = KH_PHYS_STEP * framepace ratio as the rest of sora.c, so it is the same time at ~29 fps and at 60 fps; it
// counted driven frames before = 8 frames = 275 ms at 29 fps) of the press = tap; still held after them = the glide arms then (a late arm: the apex rule in sora.c is unchanged). KH2 starts the dodge at the press
// itself; we need the delay to tell a tap from a hold, so a tap fires at the release (<= 8 KH frames) and has no early-release
// short hop (the release already happened).
// Live: set bdodge 0 = old (B in the air = glide only) | set bdodgef <KH frames> (tap window, GUESS 8, 0 = every press is a hold).
#pragma once
static int g_bdodge = 1, g_bd_tap = 8;
static struct { bool pend; float frames; } BD;  // frames = KH frames B has been held (s91 review)

static int bd_lv(void) {  // base Sora's Aerial Dodge level (0 = none)
    int lv = ml_dodge_lv() - 1;
    return lv < 0 ? 0 : lv > 4 ? 4 : lv;
}
// at the B press in the air (S.act == ACT_JUMP, nothing else playing): true = tap-or-hold is judged, the glide does NOT arm yet
static bool bd_press(void) {
    int lv = bd_lv();
    if (!g_bdodge || g_bd_tap <= 0 || IN_GUEST || fm_in_form() || lv < 1 || !MA.ready) return false;  // (s109: Roxas has no Aerial Dodge; his slot 810 is not one)
    BD.pend = true, BD.frames = 0;
    return true;
}
// every driven frame before the glide arm is cleared / used (sora.c): decides a pending press
static void bd_tick(uint64_t held, float kh, const float sdir[2], float sspeed, float z_up, const float p[3]) {
    if (!BD.pend) return;
    if (S.act != ACT_JUMP || S.special || fm_in_form()) { BD.pend = false; return; }  // (something else took over)
    if (held & BTN_JUMP) {
        if ((BD.frames += kh) < (float)g_bd_tap) return;
        BD.pend = false;
        S.glide_armed = true;  // a hold: the Glide, as before
        LOG("[sora] B held %.1f KH frames in the air: Glide (not a tap, bdodgef %d)\n", BD.frames, g_bd_tap);
        return;
    }
    BD.pend = false;  // released inside the window: a tap = Aerial Dodge
    int lv = bd_lv();
    if (lv < 1) return;  // (s91 review: the level dropped between the press and the release, e.g. debugall off: ML_DODGE[-1])
    if (!mot_of(MOT_AIR_DODGE)) { LOG("[sora] base Aerial Dodge: motion %d not in the bake\n", MOT_AIR_DODGE); return; }
    if (!dodgestam_pay("base Aerial Dodge")) return;  // (s95: 1/4 wheel; out = the tap does nothing)
    const MlDodgeRow *r = &ML_DODGE[lv - 1];
    float w = sspeed > 0 ? r->speed : 0;
    play(MOT_AIR_DODGE);
    arc(r->height);
    S.hvel[0] = sdir[0] * w, S.hvel[1] = sdir[1] * w;
    enter_jump(z_up, true);
    S.jump_held = S.jump_released = false;  // (B is already up: no early-release short hop on the dodge)
    S.glide_armed = false;
    MA.ready = false;  // (one per airtime; the next B press is the glide at once)
    LOG("[sora] BASE AERIAL DODGE LV%d (Master growth %d - 1): +%.0f cm, %.0f cm/frame, tapped after %.1f KH frames at %.3f %.3f %.3f\n",
        lv, ml_dodge_lv(), r->height, w, BD.frames, p[0], p[1], p[2]);
}

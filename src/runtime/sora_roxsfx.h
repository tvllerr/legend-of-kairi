// (s110 roxsfx) KH2 Roxas' own sounds + effects. Notes: recon/S110_HELPER_ROXSFX.md.
// SOUND: sfx.bin carries P_EX110.mset + W_EX010_RX.mset's frame triggers (footsteps, Keyblade whooshes, the voice grunts) at
// slot + 10000 * (FORM_ROXAS + 1) = 70000 + slot, his voice rows as vtbl 7000 + KH2's (tt0_roxas.win32.scd), marker name
// "roxse_v1" (tools_s110/roxsfx/sfxroxas.py). sora.c sfx_slot() picks them while IN_ROXAS.
// FX: fx.bin carries Roxas' own weapon_trail windows at slot + 70000 (W_EX010_RX.mset range trigger 12; sora_ffx.h ffx_trail_win) and
// roxas_fx_0 = P_EX110.a.us element 0, the only effect_caster trigger of his mset (frame trigger id 1 param 0 @0 of slot 162, the
// guard-like R212), played here when that motion starts together with its own SE (se999 #1 = the "wisdom_enter" name).
// Live: set roxsfx 0|1 (default 1; 0 = Roxas silent with Sora's ribbon windows, as before s110). Old sfx.bin / fx.bin = as before.
#pragma once

static bool rox_sfx_pack(void) { return sora_sfx_id("roxse_v1") >= 0; }
static bool rox_fx_pack(void) { return sora_fx_ready() && sora_fx_id("roxas_fx_0") >= 0; }

// every frame (next to af_tick; feet = Roxas' feet)
static void rox_fx_tick(const float feet[3], float to_u) {
    static int seen_motion = -1, seen_swing = -1, said;
    if (!said && sora_fx_ready()) {
        said = 1;
        LOG("[roxsfx] sfx.bin %s, fx.bin %s\n", rox_sfx_pack() ? "has Roxas' rows (70000+slot)" : "has NO Roxas rows (sfx_s110.bin not installed: Roxas is silent)",
            rox_fx_pack() ? "has Roxas' trail windows + roxas_fx_0" : "has NO Roxas rows (fx_s110.bin not installed: Sora's ribbon windows)");
    }
    if (!IN_ROXAS || !g_roxsfx || !S.mot || S.mot->slot != 162) { seen_motion = seen_swing = -1; return; }
    if (S.motion == seen_motion && S.swing == seen_swing) return;
    seen_motion = S.motion, seen_swing = S.swing;
    if (!rox_fx_pack()) return;
    int h = sora_fx_event("roxas_fx_0", feet, S.face, to_u);
    sora_sfx_event("wisdom_enter", 1.0f);
    LOG("[roxsfx] guard motion (slot 162): roxas_fx_0 -> fx %d\n", h);
}

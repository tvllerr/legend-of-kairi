// (s139 tink) Peter Pan's TINKER BELL LIGHT + HEAL. Notes, sources, VERIFIED / GUESS list, tests: recon/S139_HELPER_TINK.md.
// Included from sora_summon.h next to sora_summon_pp.h (needs SM, SMD(), sm_feet, sm_frames, S, link_heal, sora_fx_*, sora_sfx_*).
//
// KH2 (P_EX330 bdx, disassembled from KH2's own bytes; the opcode NAMES come from the PHH decoder, the bytes are KH2's):
//   * Peter's thread (bdx L5179, started at the end of his act "appear" (5158), in "appear_skip" (5933) and in "appear_area_start" (5945)) spawns effect ex330.7 (= pp_fx_7, a looping
//     600-frame ball of light) and moves it every frame; ex330.8 (pp_fx_8) is spawned ONLY when the thread is started with flag 1 = from "appear_skip" (bdx 5230-5253): not here.
//   * Orbit (bdx 5478-5648): two angles A (speed sA) and B (speed sB), both += speed * frametime and wrapped (fixrad). Light = Sora's position (obj_pos) +
//     (R sin A, -140 + H cos B, R cos A) in KH2 cm, y DOWN (so 1.4 m up, bobbing +-H); the effect's yaw is fixrad(A - pi/2).
//     idle: sA = sB = 0.0698132 (4 deg/frame), R = 40, H = 16.
//     for 72 frames after each heal (timer > 228): sA = 0.174533 (10 deg), sB = 0.10472 (6 deg), s = |(timer - 228) - 36| / 36 (1 -> 0 -> 1), R = 40 s + 30 (1 - s), H = 16 s + 24 (1 - s).
//   * Heal (bdx 5659): timer (bdx work var 88, starts 0) -= frametime; at <= 0: effect ex330.5 (= pp_fx_5, 71 frames, bone 67) bound on Sora, obj_add_hp(Sora, 5, 0, 0), timer = 300.
//     So the first heal comes on the very first frame, then every 300 frames (frametime = 1.0 per 60 Hz KH frame, the mod's `kh` unit: GUESS, see the report).
//   * The thread ends and kills the light when summon_is_tink_exist() is false: exe rva 4331e0 -> FUN_1403de320 = (a summon is out) && (it is cmd 0x132 = Peter's row) && flag 1409acdd7;
//     the flag is set to 1 at the cast of 0x132 and cleared by FUN_1403de360, called only from the REAR "reraise" spawn (rva 3fcb50 / 3fcec0: Tinker Bell reviving a KO'd Sora, NOT built)
//     and by the summon's end (FUN_1403de010 zeroes the cmd index). So: the light lives from the end of Peter's entry until he is dismissed / the gauge runs out. Never Land does not end it
//     (the summon stays out), so it keeps orbiting and healing through the Limit (GUESS that KH2's thread does not pause: nothing in bdx 5303-5651 tests a Limit).
//   * (s139 review) fx handles are kept with their g_fx_gen (smt_own), like MgFx: a bare slot number may already be another effect's.
//   * The mod's HP scale: sora.c:2947 "1 KH2 HP per quarter heart" -> obj_add_hp(Sora, 5) = link_heal(5) (5 quarter hearts = 1.25 hearts), the call the HP orbs / Cure use.
// GUESSes: the mirroring of KH2's x (-x, as sora_fx.c's W(p) for an unbound element), the yaw sign, that the heal effect follows Sora's feet (bone 67 is not followed), the effect's
// own sound pp_se34 (SM_FXSE_PETERPAN) played at the heal.
// Live keys: tinkfx 0|1 (the light and the heal flash; default 1), tinkheal 0|1 (the HP; default 1), tinklog 0|1 (a line every second of orbit; default 0).
#pragma once

#define TK_HEAL_Q 5          // obj_add_hp(Sora, 5): KH2 HP = quarter hearts in this mod
#define TK_HEAL_PERIOD 300.0f

static struct {
    bool on;                 // the thread runs (Peter's entry is over, he is not dismissed)
    int fx, heal, log;       // live keys
    float a, b, timer;       // angle A, angle B, heal timer (KH frames)
    int h, hh;               // the light's / the heal flash's fx handles
    uint32_t g, gh;          // (s139 review) their g_fx_gen at the spawn: exe fx handles are reused slot numbers (the flash ends after 71 f and its slot
                             // goes to the next effect), so a bare handle could move / kill another module's effect (orb, spark, aura). Same rule as MgFx.
    int id7, id5, se34, looked;
    float logt;
    int heals;
} TK = {.fx = 1, .heal = 1, .h = -1, .hh = -1};
// (s139 review) hot reload: sora_mod_unload's sora_fx_clear() ends every effect of the old copy, so no orphaned light can outlive it (no env list needed).

static bool smt_own(int *h, uint32_t g) {  // the handle is still our effect (alive, and no DLL spawn took the slot since); else it is dropped
    if (*h >= 0 && ((*h < 32 && !fx_mine(*h, g)) || !sora_fx_alive(*h))) *h = -1;
    return *h >= 0;
}
static int smt_spawn(int id, const float pos[3], float yaw, float to_u, uint32_t *g) {
    int h = sora_fx_spawn(id, pos, yaw, to_u);
    *g = h >= 0 && h < 32 ? g_fx_gen[h] : 0;
    return h;
}
static void smt_hide(void) {  // the effects off, the thread state kept (hidden stretch; smx_off)
    if (smt_own(&TK.h, TK.g)) sora_fx_kill(TK.h);
    if (smt_own(&TK.hh, TK.gh)) sora_fx_kill(TK.hh);
    TK.h = TK.hh = -1;
}
static void smt_stop(const char *why) {
    if (!TK.on) return;
    smt_hide();
    TK.on = false;
    LOG("[summon] tink stop (%s): the light is gone after %d heal(s)\n", why, TK.heals);
}
static void smt_lookup(void) {
    if (TK.looked) return;
    TK.looked = 1;
    TK.id7 = sora_fx_id("pp_fx_7"), TK.id5 = sora_fx_id("pp_fx_5"), TK.se34 = sora_sfx_id("pp_se34");
    LOG("[summon] tink: pack ids light pp_fx_7 %d, heal flash pp_fx_5 %d, heal sound pp_se34 %d%s\n", TK.id7, TK.id5, TK.se34,
        TK.id7 < 0 ? " (fx.bin has no pp_fx_7: no light, the heal still works)" : "");
}
static void smt_start(void) {
    smt_lookup();
    TK.on = true, TK.a = TK.b = 0, TK.timer = 0, TK.logt = 0, TK.heals = 0, TK.h = TK.hh = -1;
    LOG("[summon] tink start: Peter's entry is over, the light orbits Sora (heal %d HP every %.0f frames, first one now; light %s, heal %s)\n", TK_HEAL_Q, TK_HEAL_PERIOD,
        TK.fx ? "on" : "off (tinkfx 0)", TK.heal ? "on" : "off (tinkheal 0)");
}
static void smt_heal(const float feet[3], float to_u) {
    int before = linkpos_link_life(), after = before;
    if (TK.heal && before > 0) after = link_heal(TK_HEAL_Q);
    TK.heals++;
    LOG("[summon] tink heal #%d: Sora %d -> %d quarter hearts (+%d asked%s)\n", TK.heals, before, after, TK_HEAL_Q,
        !TK.heal ? ", tinkheal 0: not applied" : before <= 0 ? ", Sora is down: not applied" : "");
    if (!TK.fx) return;
    if (smt_own(&TK.hh, TK.gh)) sora_fx_kill(TK.hh);
    TK.hh = TK.id5 >= 0 ? smt_spawn(TK.id5, feet, S.face, to_u, &TK.gh) : -1;
    if (TK.hh >= 0 && TK.se34 >= 0) sora_sfx_play_at(TK.se34, 1.0f, feet);
}
static float smt_wrap(float a) {  // fixrad
    const float pi = 3.14159265f;
    while (a > pi) a -= 2 * pi;
    while (a < -pi) a += 2 * pi;
    return a;
}
// every driven, non-hidden frame (smx_late): start when Peter's entry is over, then the thread
static void smt_tick(float kh, float to_u) {
    if (!SMD()->tink || SM.st == SM_OFF || kh <= 0) return;
    if (!TK.on) {
        if (SM.st == SM_DISMISS || !SM.placed) return;
        bool entry_over = SM.slot != SMS_ENTRY || SM.clock >= sm_frames(SMS_ENTRY, 330.0f) - 0.01f;  // (bdx 5161-5166: the thread starts when his motion 231 has ended)
        if (!entry_over) return;
        smt_start();
    }
    float feet[3];
    if (!sm_feet(feet)) return;
    // (bdx 5339-5359) timer -= frametime; <= 0 -> heal, timer = 300
    TK.timer -= kh;
    if (TK.timer <= 0) smt_heal(feet, to_u), TK.timer = TK_HEAL_PERIOD;
    // (bdx 5361-5476) speeds / radius / height
    float sa = 0.06981300f, sb = 0.06981300f, R = 40.0f, H = 16.0f, u = TK.timer - 228.0f;
    if (u > 0) {
        float s = fabsf(u - 36.0f) / 36.0f;
        sa = 0.17453299f, sb = 0.10471999f;
        R = 40.0f * s + 30.0f * (1.0f - s), H = 16.0f * s + 24.0f * (1.0f - s);
    }
    TK.a = smt_wrap(TK.a + sa * kh), TK.b = smt_wrap(TK.b + sb * kh);
    // (bdx 5502-5648) the place: KH2 (R sin A, -140 + H cos B, R cos A) cm, y down; x mirrored like an unbound element in sora_fx.c (GUESS)
    float pos[3] = {feet[0] - R * sinf(TK.a) * to_u, feet[1] + (140.0f - H * cosf(TK.b)) * to_u, feet[2] + R * cosf(TK.a) * to_u};
    float yaw = 1.57079633f - TK.a;  // KH2's rotation y = A - pi/2, mirrored with x (GUESS)
    if (smt_own(&TK.hh, TK.gh)) sora_fx_move(TK.hh, feet, S.face);  // (bind_other on Sora: follows him; dropped once it ended / its slot is another's)
    if (!TK.fx) {
        if (TK.h >= 0) smt_hide();
        return;
    }
    smt_lookup();
    if (TK.id7 < 0) return;
    if (!smt_own(&TK.h, TK.g)) {  // (not spawned yet, or lost: evicted when all 32 slots are busy, or its slot went to another effect)
        TK.h = smt_spawn(TK.id7, pos, yaw, to_u, &TK.g);
        if (TK.h >= 0) LOG("[summon] tink: light spawned (handle %d) at %.2f %.2f %.2f\n", TK.h, pos[0], pos[1], pos[2]);
    } else sora_fx_move(TK.h, pos, yaw);
    if (TK.log) {
        TK.logt += kh;
        if (TK.logt >= 60.0f) TK.logt = 0, LOG("[summon] tink orbit: A %.2f B %.2f R %.1f H %.1f timer %.0f at %.2f %.2f %.2f\n", TK.a, TK.b, R, H, TK.timer, pos[0], pos[1], pos[2]);
    }
}
static bool smt_live_set(const char *key, int v) {
    if (!strcmp(key, "tinkfx")) {
        TK.fx = v != 0;
        if (!TK.fx) smt_hide();
        LOG("[summon] tinkfx %d: the light and the heal flash %s\n", TK.fx, TK.fx ? "on" : "off");
    } else if (!strcmp(key, "tinkheal")) TK.heal = v != 0, LOG("[summon] tinkheal %d: the heal %s\n", TK.heal, TK.heal ? "on" : "off");
    else if (!strcmp(key, "tinklog")) TK.log = v != 0, LOG("[summon] tinklog %d\n", TK.log);
    else return false;
    return true;
}

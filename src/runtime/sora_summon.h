// (s136 summon) KH2 Summons, slice 2: CHICKEN LITTLE (P_EX350). Notes + GUESS list + tests: recon/S136_HELPER_SUMMON2.md.
// Data bakes (s134, recon/S134_HELPER_SUMMON_BAKE.md; tools_s136/install_s136_summon.sh puts them in data/forms):
//   chicken_{model,pose,motion,hits}.bin   Chicken Little (198 bones = 186 body + 12 dead weapon bones); hits.bin is empty (hand-written windows below)
//   chickencall_{pose,motion}.bin          Sora's call motions 252 (A830, 350 f) and 253 (A831, 66 f) on Sora's 240-bone skeleton: they are drawn on a
//                                          second draw actor that wears Sora's OWN model (data/sora_model.bin, 240 bones) and replaces his body while they play
// KH2 rules followed (S132 section 6 = user decisions, S134 = the cmd 447 numbers):
//   * cast needs >= 3 Drive bars (cmd 447 Cost 3, VERIFIED S134) and Donald AND Goofy present and not KO'd; Sora is the body (not Roxas / Mickey / a Drive Form),
//     on the ground (GUESS: the ground / idle gate is ours), not skating / swimming / gliding
//   * the summon REPLACES Donald + Goofy: both are absorbed (hidden, no HUD plate) while it is out, and come back beside Sora when it ends
//   * how long it stays = the summon gauge (summon level + 3) bars of DriveTime (S134, PHH): summon LV 1 = 4 bars (GUESS: LV 1), drained like a Drive Form
//     (600 KH frames per bar, x0.5 out of battle: GUESS, S134 did not check the summon drain rate; this is the Drive Form rate); the gauge shows on the Drive plate (fm_gauge hook)
// Sequence: Sora plays the call (252, 350 f) on the call actor while Chicken Little plays his entry (231, 350 f, root motion) beside him; then he follows Sora
//   (idle 0 / walk 1 / run 2), fights the nearest enemy with 201 / 202 / 203, and when the gauge runs out Sora plays 253 and he plays his exit (232).
// Input: LIVE KEY ONLY for now (set scall 1, below). The honest reason: a command-menu entry needs new exe labels (a later slice, the Summon page); every
//   free button is already a KH2 shortcut. sm_request_def() is the entry the menu page calls.
// Live keys ("set <key> <v>" lines of play/live_test.txt): summon 0|1 (feature on / off; off ends a running summon), scall 1 = Chicken Little / 2 = Stitch / 3 = Genie / 4 = Peter Pan (cast NOW, skips the Drive gauge and
//   the Donald + Goofy gate; ONE-SHOT, remove it from the file afterwards), sdone 1 (dismiss now, with the exit motions), sdmg <pct> (damage %, default 100),
//   sstr <n> (Chicken Little's Str, 0 = Sora's own), sbars <n> (gauge bars, default 4), sendcall 0|1 (Sora's 253 at the end, default 0: s137 review, KH2's script plays 253 only as the FPS Mode intro), skb 0|1 (show the Keyblade on the call actor, default 0), slog 0|1 (per-hit log).
#pragma once

#define SM_DATA "D:/SoraBotW/data/forms/"
#define SM_CALL_MODEL "D:/SoraBotW/data/sora_model.bin"  // Sora's own bake (240 bones = the call pose's skeleton, VERIFIED header)

// (s137 stitch) THE SUMMON TABLE: every per-summon fact (files, mset slots, hit windows, cues, unlock, cast cmd) is a row of SM_DEF in
// sora_summon_tbl.h; the state machine below is shared. Row 0 = Chicken Little (behaves exactly as before), row 1 = Stitch.
// SMD() = the row of the summon that is out (or being cast). Only one is out at a time; each row loads its OWN two draw actors on its first cast
// (the exe API has no re-point: 14 + 2 per summon of the 24 actors, Peter Pan / Genie fit as rows 2 / 3).
#include "sora_summon_tbl.h"

// the current row's mset slots (the names the FPS file and the state machine have always used)
#define SMS_IDLE (SMD()->idle)
#define SMS_WALK (SMD()->walk)
#define SMS_RUN (SMD()->run)
#define SMS_ENTRY (SMD()->entry)
#define SMS_EXIT (SMD()->exit)
#define SMS_CALL (SMD()->call)
#define SMS_CALL_END (SMD()->call_end)
#define SM_HIT_R_CM (SMD()->hit_r_cm)
#define SM_HIT_DY_CM (SMD()->hit_dy_cm)
#define SM_STOP_CM (SMD()->stop_cm)
#define SM_ATK_CM (SMD()->atk_cm)
#define SM_HIT_ARC 1.6f        // GUESS: cos-free half angle (rad) in front of him: a wide front arc
#define SM_SEARCH_CM 900.0f    // GUESS: a target within this of him
#define SM_LEASH_CM 1500.0f    // GUESS: and within this of Sora (the party's PC_BATTLE_CM)
#define SM_STR_GUESS 0         // 0 = Sora's own Str (GUESS: KH2's summon Str is not mined)
#define SM_WALK_CM 2.0f        // prty-like (the party's Donald / Goofy), cm per KH frame
#define SM_RUN_CM 8.0f
#define SM_TURN 0.261799f      // rad per KH frame
#define SM_BLEND 8.0f          // KH frames (PHH BLEND, as the party)
#define SM_BAR_FRAMES 600.0f   // sstm DriveTime (GUESS for a summon: the Drive Form rate, unchecked)
enum { SM_OFF, SM_CALL, SM_ACTIVE, SM_DISMISS };
static const char *const SM_STNAME[4] = {"off", "call", "active", "dismiss"};

#define SM_MAXB 256
static Mot g_sm_ch[32], g_sm_cc[32];  // the CURRENT summon's motion tables (copied from g_sm_chs / g_sm_ccs by sm_select)
static int g_sm_nch, g_sm_ncc;
static Mot g_sm_chs[SM_NDEF][32], g_sm_ccs[SM_NDEF][32];
static int g_sm_nchs[SM_NDEF], g_sm_nccs[SM_NDEF];
static float g_sm_from[SM_MAXB * 12], g_sm_cur[SM_MAXB * 12];

static struct {
    int on, log, dmg_pct, str, bars, endcall, kb;  // kb: 1 = the Keyblade shows on the call actor (default 0: its 252 chain looks garbled mid-motion in sorabake view)
    bool init, data_ok, failed_d[SM_NDEF], hidden;  // failed_d: (s137 review S3) "this exe has too few actors" logged, per row
    int st;                       // SM_*
    int req;                      // 1 = cast when allowed, 2 = forced (the live key)
    uint64_t try_at_d[SM_NDEF], load_seen, ray_at;  // (s137 review S3) the load retry timer per row
    int ach, acall;               // the CURRENT summon's draw actors (> 0 once loaded)
    int req_def;                  // (s137 stitch) the SM_DEF row a cast request asks for (the row that is out = g_sm_def, sora_summon_tbl.h)
    int ach_d[SM_NDEF], acall_d[SM_NDEF];  // each row's own two draw actors
    bool data_d[SM_NDEF];
    int win;                      // the hit window of the attack that is running (a new window = a fresh hit list)
    float t, total;               // the summon gauge, KH frames
    bool hold, call_on;           // Sora is held / the call actor replaces his body
    int cslot;                    // the call actor's slot
    float cclock;                 // its clock
    // Chicken Little
    float pos[3], face, floor_s, ray_y, yoff, yv;  // yv: (s136 S4) his fall speed after an attack ended in the air, cm per KH frame
    bool placed, ray_ok;
    int slot, from_slot, band, combo, atk_i;
    float clock, from_clock, blend;
    bool atk;
    float pause, idle_t;
    uint64_t tgt, hit[16];
    int nhit;
    bool abs[2];                  // WE absorbed this member (a Drive form's absorb is not ours to undo)
    bool battle;
    int hits;
    // exe functions
    int (*load)(const char *, const char *);
    void (*pose)(int, int, float, const float *, float, float);
    void (*off)(int);
    void (*shadow)(int, float, float);
    void (*shape)(int, float, float, float, float);
    int (*cap)(void);
    int (*body)(void);
    bool (*slot_pose)(int, int, float, float *);
    void (*world)(int, const float *, const float *, float, float);
    int (*bones)(int);
    void (*blade)(int, int, int);
    void (*weapon)(int, bool);
} SM = {.on = 1, .dmg_pct = 100, .bars = 0, .endcall = 0, .blend = -1};  // (s137 review: endcall 0 = KH2; P_EX350's Sora-motion syscall runs only for 252 (cast) and 253 (FPS intro))

// (s137 fps) FPS Mode (Chicken Little's Limit, sora_summon_fps.h, included below): forward declarations for the state machine above it
static bool sm_fps_busy(void);
static bool sm_fps_on(void);
static bool sm_fps_hide_sora(void);
static void sm_fps_abort(const char *why);
static void sm_fps_leave(const char *why);
static bool sm_fps_late(float kh, float to_u, int st);
static bool fp_live_set(const char *key, int v);
// (s138 ppgenie) Peter Pan's Tinker Bell + Genie's version: hooks implemented in sora_summon_pp.h (included below the state machine)
static bool smx_ready(int d);
static bool smx_actors(int d);
static void smx_env(char *b, size_t n);
static void smx_off(void);
static void smx_cast(int d, const float feet[3]);
static void smx_dismiss(void);
static void smx_reset(void);
static int smx_pick_atk(uint64_t tgt);
static void smx_late(float kh, float to_u);
static void smx_atk_tick(float c0, float c1);
static bool smx_live_set(const char *key, int v);
static void oh_call_clock(float kh);   // (s138 ohana) Stitch's Limit drives Sora's call-actor clock (sora_summon_ohana.h)
static int oh_st(void);
static void nl_call_clock(float kh);   // (s139 neverland) Peter Pan's Limit (sora_summon_nl.h)
static int nl_st(void);
static bool nl_call_adj(float feet[3], float *face);
static int gl_st(void);   // (s139 genielim) Genie's Limits (sora_summon_gl.h)
static void gl_call_clock(float kh);
static float gl_yoff_cm(void);
static bool gl_live_set(const char *key, int v);

static bool sm_out(void) { return SM.st != SM_OFF; }
static bool sm_hold(void) { return SM.hold; }

// ---- (s136 merge) the sound / effect cues: runtime/sora_summon_fx.h (wt136-chickfx, recon/S136_HELPER_CHICKFX.md) = KH2's own mset frame
// triggers of P_EX350 (its rows match the baked ftrigs). who: 0 = Chicken Little's own motion, 1 = Sora's call motion. Sounds / voices play at `at`;
// an effect spawns at `at` (his feet / Sora's feet) and, while his motion's cues run, follows him (sora_fx_move). Bone-bound rows (bone 92) are
// placed at his feet too: GUESS (no bone follow yet). Without the s136 sfx.bin / fx.bin the ids are -1 and every cue is silent.
#include "sora_summon_fx.h"
static float g_sm_u = 1.0f;  // to_u of the last tick (the effects' scale)
#define SM_NFXH 12   // (s138 ppgenie: Genie's entry starts ~20 effects)
static int g_sm_fxh[SM_NFXH] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static void sm_fx_follow(const float at[3]) {
    for (int i = 0; i < SM_NFXH; i++)
        if (g_sm_fxh[i] >= 0) {
            if (sora_fx_alive(g_sm_fxh[i])) sora_fx_move(g_sm_fxh[i], at, SM.face);
            else g_sm_fxh[i] = -1;
        }
}
static void sm_fx_clear(void) {
    for (int i = 0; i < SM_NFXH; i++) {
        if (g_sm_fxh[i] >= 0 && sora_fx_alive(g_sm_fxh[i])) sora_fx_kill(g_sm_fxh[i]);
        g_sm_fxh[i] = -1;
    }
}
static void sm_cue(const SmCue *c, const float at[3]) {
    static int ids[SM_NDEF][SM_MAXCUE], looked[SM_NDEF];
    const SmDef *d = SMD();
    if (d->ncues > SM_MAXCUE) return;
    if (!looked[g_sm_def]) {
        looked[g_sm_def] = 1;
        for (int k = 0; k < d->ncues; k++) ids[g_sm_def][k] = d->cues[k].kind == SMC_FX ? sora_fx_id(d->cues[k].name) : sora_sfx_id(d->cues[k].name);
    }
    int k = (int)(c - d->cues), id = ids[g_sm_def][k];
    if (SM.log) LOG("[summon] cue %s %s %d f%d: %s%s\n", d->name, c->who == SMC_SORA ? "sora" : "own", c->slot, c->frame, c->name, id < 0 ? " (not in the pack)" : "");
    if (id < 0) return;
    if (c->kind != SMC_FX) { sora_sfx_play_at(id, 1.0f, at); return; }
    int h = sora_fx_spawn(id, at, c->who == SMC_SORA ? S.face : SM.face, g_sm_u);
    for (int q = 0; h >= 0 && q < d->nfxse; q++)  // (s138) the element's own sound (PAX se field), played when it starts (its small `wait` is dropped: GUESS)
        if (!strcmp(d->fxse[q].fx, c->name)) { int se = sora_sfx_id(d->fxse[q].se); if (se >= 0) sora_sfx_play_at(se, 1.0f, at); }
    if (h < 0 || c->who == SMC_SORA) return;
    if (d->exit < 0 && c->slot == d->exit_cue) return;  // (s137 review S1) the leave puff outlives him: not followed, not killed by sm_end
    for (int i = 0; i < SM_NFXH; i++)
        if (g_sm_fxh[i] < 0) { g_sm_fxh[i] = h; break; }
}
static void sm_cues_slot(int who, int slot, float from, float to, const float at[3]) {  // cues of mset slot <slot> with from < frame <= to
    if (who == SMC_SUMMON) sm_fx_follow(at);
    const SmDef *d = SMD();
    if (d->sfx_base) sora_sfx_motion(d->sfx_base + slot, from, to, at);  // (s138) sounds / voices = sfx.bin triggers at sfx_base + mset slot
    for (int k = 0; k < d->ncues; k++) {
        const SmCue *c = &d->cues[k];
        if (c->who == who && c->slot == slot && (float)c->frame > from && (float)c->frame <= to) sm_cue(c, at);
    }
}
static void sm_cues(int who, const Mot *m, float from, float to, const float at[3]) {  // cues with from < frame <= to
    if (!m) return;
    sm_cues_slot(who, m->slot, from, to, at);
}

static const Mot *sm_mot(int slot) { return mot_find(g_sm_ch, g_sm_nch, slot); }
static float sm_frames(int slot, float dflt) { const Mot *m = sm_mot(slot); return m && m->frames ? (float)m->frames : dflt; }
static bool sm_looped(int slot) { return slot == SMS_IDLE || slot == SMS_WALK || slot == SMS_RUN; }

static void sm_actors_off(void) {
    if (!SM.off) return;
    if (SM.ach > 0) SM.off(SM.ach);
    if (SM.acall > 0) SM.off(SM.acall);
    smx_off();
}

// first tick of this DLL (also a hot reload): find the exe functions; a previous DLL's actors may still be drawn (their last pose stays in the exe) -> hide them
static void sm_init(void) {
    if (SM.init) return;
    SM.init = true;
    HMODULE exe = GetModuleHandleA(NULL);
    SM.load = (int (*)(const char *, const char *))(void *)GetProcAddress(exe, "sora_draw_actor_load");
    SM.pose = (void (*)(int, int, float, const float *, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_pose");
    SM.off = (void (*)(int))(void *)GetProcAddress(exe, "sora_draw_actor_off");
    SM.shadow = (void (*)(int, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_shadow");
    SM.shape = (void (*)(int, float, float, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_shadow_shape");
    SM.cap = (int (*)(void))(void *)GetProcAddress(exe, "sora_draw_actor_capacity");
    SM.body = (int (*)(void))(void *)GetProcAddress(exe, "sora_draw_body");
    SM.slot_pose = (bool (*)(int, int, float, float *))(void *)GetProcAddress(exe, "sora_draw_actor_slot_pose");
    SM.world = (void (*)(int, const float *, const float *, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_world");
    SM.bones = (int (*)(int))(void *)GetProcAddress(exe, "sora_draw_actor_bone_count");
    SM.blade = (void (*)(int, int, int))(void *)GetProcAddress(exe, "sora_draw_actor_blade");
    SM.weapon = (void (*)(int, bool))(void *)GetProcAddress(exe, "sora_draw_actor_weapon");
    char b[160];
    if (GetEnvironmentVariableA("SORA_SM_ACT", b, sizeof b) && SM.off) {  // (survives the DLL's hot reload) "a,b,c,d": each row's two actors
        int hid = 0;
        for (char *q = b; *q;) {
            int a = (int)strtol(q, &q, 10);
            if (a > 0) SM.off(a), hid++;
            if (*q == ',') q++;
            else break;
        }
        LOG("[summon] hot reload: %d summon actor(s) hidden (%s), nothing absorbed (module state reset)\n", hid, b);
    }
    LOG("[summon] %s; draw functions %s, exe actor table %d (needs %d for %s, %d for %s); %d summon(s) in the table\n", SM.on ? "on" : "off (set summon 1)",
        SM.load && SM.pose && SM.off && SM.body && SM.slot_pose && SM.world && SM.bones ? "found" : "MISSING (no summon)", SM.cap ? SM.cap() : 0,
        SM_DEF[0].need, SM_DEF[0].name, SM_DEF[SM_NDEF - 1].need, SM_DEF[SM_NDEF - 1].name, SM_NDEF);
}

// the row <d> becomes the current one: its actors and motion tables (loaded by sm_actors) are what SM.ach / SM.acall / g_sm_ch / g_sm_cc mean from now on
static void sm_select(int d) {
    g_sm_def = d;
    SM.ach = SM.ach_d[d], SM.acall = SM.acall_d[d], SM.data_ok = SM.data_d[d];
    memcpy(g_sm_ch, g_sm_chs[d], sizeof g_sm_ch), memcpy(g_sm_cc, g_sm_ccs[d], sizeof g_sm_cc);
    g_sm_nch = g_sm_nchs[d], g_sm_ncc = g_sm_nccs[d];
}

// (row <d>) both draw actors + the two motion bakes; retried every 600 presents when a file is missing; an old exe (< need actors) is refused with one log line
static bool sm_actors(int d, uint64_t present) {
    const SmDef *D = &SM_DEF[d];
    if (SM.ach_d[d] > 0 && SM.acall_d[d] > 0 && SM.data_d[d] && smx_ready(d)) return true;  // (s136 review S1: data_ok too)
    if (!SM.load || !SM.pose || !SM.off || !SM.body || !SM.slot_pose || !SM.world || !SM.bones || !SM.cap) return false;
    if (SM.cap() < D->need) {
        if (!SM.failed_d[d]) SM.failed_d[d] = true, LOG("[summon] no %s: this exe has %d draw actors, it needs %d (relink savagewind.exe)\n", D->name, SM.cap(), D->need);
        return false;
    }
    if (SM.try_at_d[d] && present - SM.try_at_d[d] < 600) return false;
    SM.try_at_d[d] = present ? present : 1;
    char f1[160], f2[160], f3[160];
    if (!SM.data_d[d]) {
        snprintf(f1, sizeof f1, SM_DATA "%s", D->motion), snprintf(f2, sizeof f2, SM_DATA "%s", D->callmotion);
        g_sm_nchs[d] = mot_file_load(f1, g_sm_chs[d], 32);
        g_sm_nccs[d] = mot_file_load(f2, g_sm_ccs[d], 32);
        SM.data_d[d] = g_sm_nchs[d] > 0 && g_sm_nccs[d] > 0;
        if (!SM.data_d[d]) LOG("[summon] %s motion bakes missing (run the summon's install script with the game closed)\n", D->name);
    }
    snprintf(f1, sizeof f1, SM_DATA "%s", D->model), snprintf(f2, sizeof f2, SM_DATA "%s", D->pose), snprintf(f3, sizeof f3, SM_DATA "%s", D->callpose);
    if (SM.ach_d[d] <= 0) SM.ach_d[d] = SM.load(f1, f2);
    if (SM.acall_d[d] <= 0) SM.acall_d[d] = SM.load(SM_CALL_MODEL, f3);
    int ach = SM.ach_d[d], acall = SM.acall_d[d];
    LOG("[summon] draw actors: %s %d, Sora's call %d%s\n", D->name, ach, acall, ach > 0 && acall > 0 && SM.data_d[d] ? "" : " (a bake is missing: no summon)");
    if (ach > 0 && SM.shape) SM.shape(ach, D->shape, 0, 0, 0);
    if (acall > 0 && SM.blade) SM.blade(acall, 0, kb_alt(KB.kb[KB_MAIN])), SM.blade(acall, 1, kb_alt(KB.kb[KB_MAIN]));  // (the Keyblade of the call = his main one)
    if (acall > 0 && SM.weapon) SM.weapon(acall, SM.kb != 0);
    char b[160];
    b[0] = 0;
    for (int k = 0; k < SM_NDEF; k++) {  // every row's actors, for the hot reload
        char t[24];
        snprintf(t, sizeof t, "%s%d,%d", k ? "," : "", SM.ach_d[k], SM.acall_d[k]);
        strncat(b, t, sizeof b - strlen(b) - 1);
    }
    bool xok = smx_actors(d);  // (s138) Tinker Bell's body actor (Peter Pan), Genie's default version
    smx_env(b, sizeof b);
    SetEnvironmentVariableA("SORA_SM_ACT", b);
    return ach > 0 && acall > 0 && SM.data_d[d] && xok;
}

// ---- who may be summoned right now
static bool sm_member_ok(int mb) { return P.on && P.ticks && g_party_id[mb] > 0 && P.m[mb].hp > 0 && !g_party_absorbed[mb]; }
static const char *sm_gate(bool force, int st) {
    if (!SM.on) return "summon is off (set summon 1)";
    if (SM.st != SM_OFF) return "a summon is already out";
    if (IN_GUEST) return "Sora is not the body (Roxas / Mickey)";
    if (fm_in_form()) return "in a Drive Form";
    if (SKT.riding) return "on the skateboard";
    if (pl_hold_party()) return "a party Limit is running";
    if (st != LINK_GROUND || ACT_AIR(S.act) || S.air_frames > 0 || S.takeoff > 0) return "not on the ground";
    if (S.act != ACT_IDLE || S.special != SP_NONE) return "Sora is mid-action";
    if (link_water_depth() > 0.02f) return "in water";
    if (g_event || g_link_owns) return "an event / Link's own state";
    if (!force) {
        if (DG.bars < 3) return "needs 3 Drive bars (cast cmd Cost 3: Chicken Little 447, Stitch 88, Genie 89, Peter Pan 306)";
        if (!sm_member_ok(0) || !sm_member_ok(1)) return "needs Donald AND Goofy present and not KO'd";
        if (!pchar_need_ok(g_pchar, PNEED_SUMMON)) return "needs Donald AND Goofy (a stand-in has a place: Debug Menu Party 1 / 2)";  // (s217 riku / s220)
    }
    return NULL;
}
// the entry of the Summon menu page / the live keys: asks for a cast of row <d>; the tick checks the gate
static void sm_request_def(int d, bool force) { SM.req = force ? 2 : 1, SM.req_def = d < 0 || d >= SM_NDEF ? 0 : d; }

// ---- the end: both actors off, Donald + Goofy back beside Sora (only the ones WE absorbed, and not the ones a Drive form is taking)
static void sm_end(const char *why) {
    if (SM.st == SM_OFF) return;
    su_track(SM.t, SM.total, "gauge"), su_track_stop();  // (s148 sumlv: the last boundaries, then no tracking until the next cast)
    LOG("[summon] END (%s) from %s: %.0f of %.0f gauge frames left, %d hit(s)\n", why, SM_STNAME[SM.st], SM.t, SM.total, SM.hits);
    sm_actors_off();
    sm_fx_clear();  // (s136 merge) his effects go with him
    sm_fps_abort(why);  // (s137 fps) the camera, the reticle and the shots go with him
    smx_reset();  // (s138) Tinker Bell's stage
    for (int mb = 0; mb < PARTY_N; mb++) {
        if (!SM.abs[mb]) continue;
        SM.abs[mb] = false;
        if (fm_in_form() && fm_takes(g_fform, mb)) continue;  // (the Drive form owns his absorb now; it gives him back at its revert)
        if (g_party_absorbed[mb]) g_party_absorbed[mb] = false, P.m[mb].placed = false;  // (party_core_tick places him beside Sora)
        LOG("[summon] %s back beside Sora\n", mb ? "Goofy" : "Donald");
    }
    SM.st = SM_OFF, SM.hold = SM.call_on = false, SM.placed = false, SM.atk = false, SM.blend = -1, SM.hidden = false;
}

static void sm_play(int slot) {  // switch Chicken Little's motion with a blend from the frozen last pose
    if (slot == SM.slot && SM.placed) return;
    SM.from_slot = SM.slot, SM.from_clock = SM.clock;
    SM.blend = SM.placed && SM.slot >= 0 && SM.from_slot != slot ? 0 : -1;
    SM.slot = slot, SM.clock = 0, SM.yoff = 0;
}

static bool sm_feet(float f[3]) {
    float lm[12];
    if (!linkpos_link_mtx(lm)) return false;
    f[0] = lm[3], f[1] = lm[7], f[2] = lm[11];
    return true;
}

static void sm_place_beside(const float feet[3], float to_u, const char *why) {  // 150 cm to Sora's left (the party's return spot)
    float fx = sinf(S.face), fz = cosf(S.face);
    SM.pos[0] = feet[0] + fz * 150.0f * to_u, SM.pos[1] = feet[1], SM.pos[2] = feet[2] - fx * 150.0f * to_u;
    SM.floor_s = SM.ray_y = feet[1], SM.face = S.face, SM.ray_ok = false, SM.placed = true;
    LOG("[summon] %s placed beside Sora (%s) at %.2f %.2f %.2f\n", SMD()->name, why, SM.pos[0], SM.pos[1], SM.pos[2]);
}

// (entry_shift rows) bone 0's travel of the entry (its last frame) minus the idle's first frame, entity frame cm (x left, y up, z ahead): the pose carries the run-in
// (the bake did not root-strip slot 231), so at the end of the entry the origin must move by this much to keep him where he stands. VERIFIED from stitch_pose.bin.
// (s137 review S2/S6) at = the entry clock (< 0 = its last frame): where he stands at that clock relative to the idle's spot.
static void sm_entry_shift_at(float at, float out[3]) {
    out[0] = out[1] = out[2] = 0;
    float a[SM_MAXB * 12], b[SM_MAXB * 12];
    int nb = SM.bones && SM.ach > 0 ? SM.bones(SM.ach) : 0;
    if (nb <= 0 || nb > SM_MAXB || !SM.slot_pose) return;
    float clk = at < 0 ? sm_frames(SMS_ENTRY, 328.0f) : at;
    if (SM.slot_pose(SM.ach, SMS_ENTRY, clk, a) && SM.slot_pose(SM.ach, SMS_IDLE, 0, b))
        for (int k = 0; k < 3; k++) out[k] = a[9 + k] - b[9 + k];
}
static void sm_entry_shift(float out[3]) { sm_entry_shift_at(-1, out); }
// the same, as a world offset (his facing, to_u): x left / z ahead -> world (the rule of sm_root)
static void sm_entry_shift_world(float clk, float to_u, float w[3]) {
    float sh[3], fx = sinf(SM.face), fz = cosf(SM.face);
    sm_entry_shift_at(clk, sh);
    w[0] = (sh[2] * fx + sh[0] * fz) * to_u, w[1] = 0, w[2] = (sh[2] * fz - sh[0] * fx) * to_u;
}
// (s137 review S6) an entry_shift row leaves its entry early (a dismiss / a re-place mid-run-in): the origin goes to where he stands now, idle, no blend
static void sm_entry_leave_early(float to_u) {
    if (!SMD()->entry_shift || SM.slot != SMS_ENTRY || !SM.placed) return;
    float w[3];
    sm_entry_shift_world(SM.clock, to_u, w);
    SM.pos[0] += w[0], SM.pos[2] += w[2];
    sm_play(SMS_IDLE);
    SM.blend = -1;
    sm_fx_clear();
    LOG("[summon] %s leaves his entry early at f%.0f (origin moved %.0f cm)\n", SMD()->name, SM.clock, hypotf(w[0], w[2]) / to_u);
}

// ---- the cast: 3 Drive bars, Donald + Goofy leave, Sora starts the call, the summon his entry
static void sm_cast(int d, bool force, const float feet[3], float to_u) {
    sm_select(d);  // (s137 stitch) this row's actors + motion tables from now on
    g_sm_u = to_u;
    if (!force) {
        int b0 = DG.bars;
        bool f0 = dg_full();
        if (f0) DG.fill = 0;  // (re:0x3C2780: the partial bar of a full gauge is lost)
        DG.bars -= 3;
        if (DG.bars < 0) DG.bars = 0, DG.fill = 0;
        char why[64];
        snprintf(why, sizeof why, "summon %s (cmd %d, 3 bars)", SMD()->name, SMD()->cast_cmd);
        dg_changed(b0, f0, why);
    }
    for (int mb = 0; mb < PARTY_N; mb++) {
        if (g_party_absorbed[mb]) continue;
        g_party_absorbed[mb] = true, SM.abs[mb] = true;
        if (g_party_id[mb] > 0 && PD.off) PD.off(g_party_id[mb]);
    }
    int bars = SM.bars > 0 ? (SM.bars > 10 ? 10 : SM.bars) : su_gauge_bars();  // (s148 sumlv: KH2 3de610 = Summon LV + 3 bars, VERIFIED; live sbars N overrides)
    SM.total = SM.t = (float)bars * SM_BAR_FRAMES;
    su_track_start(SM.total);
    LOG("[sumlv] cast: Summon LV%d (EXP %u, cap LV%d) -> %d bars%s\n", su_lv(), SU.exp, su_cap(), bars, SM.bars > 0 ? " (sbars override)" : "");
    SM.st = SM_CALL, SM.hold = SM.call_on = true, SM.cslot = SMS_CALL, SM.cclock = 0, SM.hits = 0, SM.nhit = 0, SM.tgt = 0, SM.atk = false, SM.hidden = false;
    SM.pause = 0, SM.combo = 0, SM.band = 0, SM.slot = -1, SM.placed = false;
    float fx = sinf(S.face), fz = cosf(S.face);
    // his entry (231) ends 90 cm to his right and 145 cm ahead of where it starts (the baked root curve): start 190 cm to Sora's left, level with him,
    // so he ends 100 cm to Sora's left and 150 cm ahead (GUESS: KH2 places him with the object script, not mined)
    const SmDef *D = SMD();
    float left = D->start_left, ahead = D->start_ahead;  // entry_shift: where the entry ENDS (his follow-like spot); else where the origin starts
    if (D->attach) {  // (s138 review) KH2 obj_attach(Sora, 0x4002) at the cast (Peter P_EX330 bdx 5766, Genie P_AL010 6123): the pose is relative to Sora's feet
        SM.pos[0] = feet[0], SM.pos[2] = feet[2];
    } else if (D->entry_shift) {  // (Stitch) the entry's travel is in the pose: put the origin so that his LAST entry frame stands on (left, ahead)
        float sh[3];
        sm_entry_shift(sh);
        float ex = left - sh[0], ez = ahead - sh[2];  // entity frame (+x left, +z ahead), cm
        SM.pos[0] = feet[0] + (fz * ex + fx * ez) * to_u, SM.pos[2] = feet[2] + (-fx * ex + fz * ez) * to_u;
    } else SM.pos[0] = feet[0] + (fz * left + fx * ahead) * to_u, SM.pos[2] = feet[2] + (-fx * left + fz * ahead) * to_u;
    SM.pos[1] = feet[1];
    SM.floor_s = SM.ray_y = feet[1], SM.face = S.face, SM.ray_ok = false, SM.placed = true, SM.yoff = 0;
    SM.slot = -1, SM.blend = -1;
    sm_play(SMS_ENTRY);
    SM.blend = -1;
    if (SM.body) SM.off(SM.body());
    LOG("[summon] CAST %s%s: %d bars of gauge, Donald + Goofy absorbed, Sora's call motion %d, he enters (motion %d) at %.2f %.2f %.2f; Drive now %d bars + %d\n",
        D->name, force ? " (forced: no gauge / party gate)" : "", bars, SMS_CALL, SMS_ENTRY, SM.pos[0], SM.pos[1], SM.pos[2], DG.bars, DG.fill);
    smx_cast(d, feet);  // (s138) Genie: the random owned version; Peter Pan: Tinker Bell flies in with him (her 231)
    sm_cues(0, sm_mot(SMS_ENTRY), -1, 0, SM.pos);
    sm_cues(1, mot_find(g_sm_cc, g_sm_ncc, SMS_CALL), -1, 0, feet);
}

static void sm_begin_dismiss(const char *why) {
    sm_entry_leave_early(g_sm_u);  // (s137 review S6: Stitch's run-in lives in the pose; leave it where he stands)
    smx_dismiss();  // (s138) Tinker Bell (still in her intro) goes with him
    SM.st = SM_DISMISS;
    SM.atk = false;
    bool clean = SM.endcall && SMS_CALL_END >= 0 && S.act == ACT_IDLE && !S.special && !ACT_AIR(S.act) && S.air_frames <= 0 && S.takeoff <= 0 && !pl_hold_party();
    SM.hold = SM.call_on = clean, SM.cslot = SMS_CALL_END, SM.cclock = 0;
    sm_play(SMS_EXIT >= 0 ? SMS_EXIT : SMS_IDLE);  // (a row without an exit motion stands in his idle and vanishes in a puff: exit_cue / exit_frames)
    if (SMS_EXIT < 0) SM.clock = 0, SM.blend = -1;  // (he is already in idle: the clock is the puff timer)
    if (clean && SM.body) SM.off(SM.body());
    LOG("[summon] DISMISS %s (%s): plays his exit (%d)%s\n", SMD()->name, why, SMS_EXIT, clean ? ", Sora plays 253 (sendcall 1: NOT KH2, test only)" : "");
    float f[3];
    if (sm_feet(f)) {
        if (SMS_EXIT >= 0) sm_cues(0, sm_mot(SMS_EXIT), -1, 0, SM.pos);
        else sm_cues_slot(SMC_SUMMON, SMD()->exit_cue, -1, 0, SM.pos);
        if (clean) sm_cues(1, mot_find(g_sm_cc, g_sm_ncc, SMS_CALL_END), -1, 0, f);
    }
}

// ---- every frame, early (after the Slate's state change, next to skt_tick): the state machine, the gauge, the aborts
static void sm_tick(uint64_t present, int st, const float p[3], bool block) {
    (void)p;
    sm_init();
    su_debug_tick();  // (s148 sumlv: EXP under a debug unlock is thrown away at OFF)
    bool loaded = g_load_end != SM.load_seen;
    if (!SM.load_seen) loaded = false;
    SM.load_seen = g_load_end;
    float ratio = framepace_game_ratio();
    if (!(ratio > 0.1f && ratio < 3)) ratio = 1;
    float kh = KH_PHYS_STEP * ratio, to_u = 0.01f * g_k;
    if (SM.st == SM_OFF) {
        if (!SM.req || SM.req == 3) { SM.req = 0; return; }
        bool force = SM.req == 2;
        SM.req = 0;
        float feet[3];
        const char *why = block ? "dead / loading" : sm_gate(force, st);
        if (!why && !sm_feet(feet)) why = "no Link matrix";
        int rd = SM.req_def;
        if (!why && !sm_actors(rd, present)) why = "draw actors / bakes not ready";
        if (why) { LOG("[summon] cast refused (%s): %s\n", SM_DEF[rd].name, why); sora_sfx_event("error", 1.0f); return; }
        sm_cast(rd, force, feet, to_u);
        return;
    }
    // ---- a summon is out
    su_track(SM.t, SM.total, "gauge");  // (s148 sumlv: bar boundaries crossed since the last frame: drain + Limit costs)
    if (SM.req == 1 || SM.req == 2) SM.req = 0, LOG("[summon] cast ignored: already out\n");
    const char *end = !SM.on ? "summon 0" : block ? "Sora KO / loading" : loaded ? "a load" : IN_GUEST ? "Roxas / Mickey" : fm_in_form() ? "a Drive Form"
                    : SKT.riding ? "the skateboard" : NULL;
    if (end) { sm_end(end); return; }
    if (st == LINK_OTHER) {  // swim / climb / puppet / event: the calls cannot play, an active summon waits hidden (the gauge holds)
        if (sm_fps_busy()) sm_fps_leave("Link's own state");  // (s137 review M1: sm_late does not run here, so FPS Mode ends now)
        if (SM.st != SM_ACTIVE) { sm_end("Link's own state during the call"); return; }
        if (!SM.hidden) SM.hidden = true, sm_actors_off(), LOG("[summon] hidden (Sora not driven)\n");
        return;
    }
    if (SM.hidden) {
        SM.hidden = false;
        float f[3];
        if (sm_feet(f) && hypotf(SM.pos[0] - f[0], SM.pos[2] - f[2]) / to_u > 1500.0f) sm_entry_leave_early(to_u), sm_place_beside(f, to_u, "back from a hidden stretch");
        else LOG("[summon] shown again\n");
    }
    if (SM.hold || SM.call_on) {  // Sora plays the call: the menu stays dead; a hit breaks it off (GUESS, not checked in KH2)
        M.dead = 8, M.sub = -1;
        if (S.special == SP_DAMAGE && nl_st() == 0) {   // (s139: carried by Peter in Never Land, a hit does not break the Limit off: GUESS)
            LOG("[summon] Sora was hit during the %s: the call is cut short\n", SM_STNAME[SM.st]);
            SM.hold = SM.call_on = false;
            if (SM.acall > 0 && SM.off) SM.off(SM.acall);
            if (SM.st == SM_CALL) SM.st = SM_ACTIVE;
        }
    }
    float now = SM.cclock + kh;
    if (gl_st()) gl_call_clock(kh);  // (s139 genielim) Genie's Limit: Sora's capture motions + root travel
    else if (oh_st() >= 2) oh_call_clock(kh);  // (s138 ohana) the Limit proper: Sora loops 254 / plays the command motions, no generic call end
    else if (nl_st() >= 1) nl_call_clock(kh);  // (s139 neverland) Sora loops 253 / plays 255, 256
    else if (SM.call_on) {
        const Mot *cm = mot_find(g_sm_cc, g_sm_ncc, SM.cslot);
        float fr = cm && cm->frames ? (float)cm->frames : (SM.cslot == SMS_CALL ? 350.0f : 66.0f);
        float f[3];
        if (sm_feet(f)) sm_cues(1, cm, SM.cclock, now > fr ? fr : now, f);
        SM.cclock = now;
        if (SM.cclock >= fr) {
            SM.cclock = fr, SM.call_on = SM.hold = false;
            if (SM.acall > 0 && SM.off) SM.off(SM.acall);
            if (SM.st == SM_CALL) SM.st = SM_ACTIVE, LOG("[summon] the call is over: %s fights beside Sora (gauge %.0f of %.0f frames)\n", SMD()->name, SM.t, SM.total);
        }
    } else if (SM.st == SM_CALL) {  // (the call was cut short and Chicken Little's entry goes on)
        SM.st = SM_ACTIVE;
    }
    if (SM.st == SM_ACTIVE) {
        SM.t -= kh * (SM.battle ? 1.0f : FM_RELAX);
        if (SM.t < 0) SM.t = 0;  // (s147: a Limit holding the end kept draining -> "gauge now -137" in the END logs)
        if (SM.req == 3 && !sm_fps_busy()) SM.req = 0, sm_begin_dismiss("sdone");  // (s137 fps: the Limit holds the summon's end, as KH2's cmd gate does)
        else if (SM.t <= 0 && !sm_fps_busy()) SM.t = 0, sm_begin_dismiss("the gauge ran out");
    } else if (SM.st == SM_DISMISS) {
        bool ch_done = SMS_EXIT >= 0 ? SM.clock >= sm_frames(SMS_EXIT, 60.0f) - 0.01f : SM.clock >= SMD()->exit_frames;
        if (ch_done && !SM.call_on) sm_end("dismissed");
    }
}

// ---- Chicken Little's damage: the KH-relative rule of Sora's own hits / the party's (sl_kh_apply), Str = GUESS (Sora's), credited to Sora
static int sm_str(void) {
    if (SM.str > 0) return SM.str;
    sl_init();
    return sl_str_of(&SL_LV[sl_lv() - 1]);  // GUESS: the summon hits like Sora's bare Str (no Keyblade bonus)
}
static int sm_damage(uint64_t actor, int32_t life, int power) {
    sl_init();
    int flat = (int)lroundf(g_dmg_base * (float)power / 100.0f), d;
    if (flat < 1) flat = 1;
    d = flat;
    if (life < 0) life = sl_life_now(actor);
    SlCredit *c = life > 0 ? sl_credit(actor, life) : NULL;
    if (!c) return d;
    if (life > c->max_life) c->max_life = life;
    int st = sm_str();
    if (sl_khformula) {
        char why[200];
        int k = sl_kh_apply(c, st, power, why, (int)sizeof why);
        if (SL.lvdmg) d = k;
        if (SM.dmg_pct != 100) d = (int)lroundf((float)d * (float)SM.dmg_pct / 100.0f), d = d < 1 ? 1 : d;
        if (SM.log) LOG("[summon] dmg %llx %s: Str %d: %s -> %d (sdmg %d%%)\n", (unsigned long long)actor, c->name, st, why, d, SM.dmg_pct);
        c->hit_at = SL.clock, c->queued += d;
        return d;
    }
    d = (int)lroundf(g_dmg_base * (float)power / 100.0f * (float)SM.dmg_pct / 100.0f);
    if (d < 1) d = 1;
    c->hit_at = SL.clock, c->queued += d;
    return d;
}

typedef struct { uint64_t actor; float pos[3]; int32_t life; } SmEn;
static int sm_enemies(SmEn *en, int max) {
    uint64_t a[64];
    float q[64][3];
    int32_t l[64];
    int n = linkpos_enemies(a, q, l, max < 64 ? max : 64);
    if (n < 0) n = 0;
    for (int k = 0; k < n; k++) en[k].actor = a[k], memcpy(en[k].pos, q[k], sizeof en[k].pos), en[k].life = l[k];
    return n;
}
static float sm_dist_cm(const float a[3], const float b[3], float to_u) { return hypotf(a[0] - b[0], a[2] - b[2]) / to_u; }

static void sm_hit_window(const SmEn *en, int n, float to_u) {  // GUESS windows / volume (the row's SmAtk)
    const SmAtk *A = &SMD()->atk[SM.atk_i];
    float c = SM.clock;
    int wi = -1;
    for (int w = 0; w < A->nwin; w++)
        if (c >= A->win[w].a && c <= A->win[w].b) { wi = w; break; }
    if (wi < 0) return;
    if (wi != SM.win) SM.win = wi, SM.nhit = 0;  // a new window of the same swing = a fresh hit list (Chicken Little: one window per swing, as before)
    const SmHitW *HW = &A->win[wi];
    const SmDef *D = SMD();
    int per = HW->n > 0 ? HW->n : 1;  // (s137 review M1) hits per target in this window (Stitch: 2 shots per arm-0/1 trigger, re:0x1C9920)
    float fx = sinf(SM.face), fz = cosf(SM.face);
    for (int k = 0; k < n; k++) {
        if (en[k].life <= 0) continue;
        int had = 0;
        for (int h = 0; h < SM.nhit; h++) had += SM.hit[h] == en[k].actor;
        if (had >= per) continue;
        float dx = en[k].pos[0] - SM.pos[0], dz = en[k].pos[2] - SM.pos[2], d = hypotf(dx, dz) / to_u;
        float dy = fabsf(en[k].pos[1] - (SM.pos[1] + SM.yoff * to_u)) / to_u;
        if (d > SM_HIT_R_CM || dy > SM_HIT_DY_CM) continue;
        if (D->ranged) { if (en[k].actor != SM.tgt) continue; }  // (Stitch) a shot hits the target he is aiming at, wherever it stands in range
        else if (d > 90.0f && acosf(pt_clampf((dx * fx + dz * fz) / (d * to_u), -1, 1)) > SM_HIT_ARC) continue;
        for (int s = had; s < per; s++) {  // (all of a window's shots land together: KH2 fires them in the same call)
            if (SM.nhit >= 16) return;  // (s136 review S5: a full list = no more hits this window, not a hit every frame)
            SM.hit[SM.nhit++] = en[k].actor;
            int dmg = sm_damage(en[k].actor, en[k].life, HW->power);
            float at[3] = {en[k].pos[0], en[k].pos[1] + 0.9f, en[k].pos[2]};
            if (D->atkp > 0) {  // (s137 review M1) the row's reaction, as fp_hit_enemy / blade_hits_one: ah_hit_f54 + damage + ah_hit
                int react = D->react, kb1 = D->kb1, kb2 = D->kb2, ap = D->atkp;
                if (HW->atkp > 0) ap = HW->atkp, react = HW->react, kb1 = HW->kb1, kb2 = HW->kb2;  // (s138) the window's own 00battle.bin row (Peter Pan: 690 / 691 / 1135)
                ah_atkp(ap, &react, &kb1, &kb2);  // (a row in AH_ATKP wins; 883 is not in it: the table's VERIFIED 00battle.bin values)
                int f54 = ah_hit_f54(en[k].actor, en[k].pos, F54_SMALL, react, to_u);
                linkpos_damage_enemy(en[k].actor, dmg, f54);
                ah_hit(en[k].actor, en[k].pos, SM.pos, react, kb1, kb2, f54, to_u, "summon");
            } else linkpos_damage_enemy_ex(en[k].actor, dmg, F54_PARTY, 0);
            sora_fx_event("hit", at, SM.face, to_u);
            static int hit_se = -2;
            if (hit_se == -2) hit_se = sora_sfx_id("hit");
            if (hit_se >= 0) sora_sfx_play_at(hit_se, 1.0f, at);
            SM.hits++;
            if (SM.log) LOG("[summon] HIT %llx (life %d) by %s motion %d f%.0f window %d shot %d power %d%s: damage %d\n", (unsigned long long)en[k].actor, en[k].life, D->name, A->slot, c, wi,
                            s + 1, HW->power, D->atkp > 0 ? (HW->atkp > 0 ? " (window atkp row)" : " (atkp row)") : "", dmg);
        }
    }
}

// his floor: a ray down beside/under him (the party's rules, simplified: every 6 presents moving, every 60 standing, eased)
static void sm_floor(const float feet[3], uint64_t present, bool moving, float kh, float to_u) {
    if (!SM.ray_ok || present - SM.ray_at >= (moving ? 6u : 60u)) {
        SM.ray_at = present;
        float hi = fmaxf(SM.floor_s, feet[1]) + 2.0f, lo = fminf(SM.floor_s, feet[1]) - 6.0f;
        float a[3] = {SM.pos[0], hi, SM.pos[2]}, b[3] = {SM.pos[0], lo, SM.pos[2]}, h[3] = {0}, n[3] = {0};
        if (linkpos_raycast(a, b, h, n) && n[1] > 0.3f) SM.ray_y = h[1], SM.ray_ok = true;
        else if (hypotf(SM.pos[0] - feet[0], SM.pos[2] - feet[2]) < 800.0f * to_u) SM.ray_y = feet[1], SM.ray_ok = true;  // no hit: Sora's floor near him
        else SM.ray_ok = true;  // (far from Sora and no hit: keep the last)
    }
    float k = 1.0f - expf(-kh / 60.0f * 30.0f), y = SM.floor_s + (SM.ray_y - SM.floor_s) * k, fall = 15.0f * kh * to_u;
    if (y < SM.floor_s - fall) y = SM.floor_s - fall;
    SM.floor_s = SM.pos[1] = y;
}

static void sm_steer(const float goal[3], float to_u, float kh, float stop_cm, const float (*bands)[2], float face_to, bool force_band, int fband) {
    float dx = goal[0] - SM.pos[0], dz = goal[2] - SM.pos[2], dist = hypotf(dx, dz), dcm = dist / to_u;
    if (force_band) SM.band = fband;
    else {
        if (dcm > bands[SM.band][1] && SM.band < PBAND_RUN) SM.band++;
        else if (dcm < bands[SM.band][0] && SM.band > PBAND_STAND) SM.band--;
    }
    float speed = SM.band == PBAND_RUN ? SM_RUN_CM : SM.band == PBAND_WALK ? SM_WALK_CM : 0;
    if (stop_cm > 0 && dcm <= stop_cm) speed = 0, SM.band = PBAND_STAND;
    float want = speed > 0 && dist > 1e-5f ? atan2f(dx, dz) : face_to, step = SM_TURN * kh, df = pt_wrap(want - SM.face);
    SM.face = pt_wrap(SM.face + pt_clampf(df, -step, step));
    if (speed > 0 && dist > 1e-5f) {
        float mv = fminf(dist - (stop_cm > 0 ? stop_cm * to_u : 0), speed * kh * to_u);
        if (mv > 0) SM.pos[0] += dx / dist * mv, SM.pos[2] += dz / dist * mv;
    }
}

static void sm_loco_motion(float kh) {  // idle / walk / run by the band, looped (a row may share one slot: Stitch has only an idle)
    sm_play(SM.band == PBAND_RUN ? SMS_RUN : SM.band == PBAND_WALK ? SMS_WALK : SMS_IDLE);
    SM.clock += kh;
    float n = sm_frames(SM.slot, 30.0f);
    const Mot *lm = sm_mot(SM.slot);
    if (n > 0 && SMD()->loop_from && lm && lm->loop > 0 && lm->loop < (int)n && SM.clock >= n) SM.clock = (float)lm->loop + fmodf(SM.clock - (float)lm->loop, n - (float)lm->loop);  // (s138) Peter's 10A / 14A loop from f20
    else if (n > 0) SM.clock = fmodf(SM.clock, n);
}

// root motion of a slot between two clocks, entity frame (+z ahead, +x left) -> the world by his facing (sora.c's rule); y = vertical (the 202 hop)
static void sm_root(int slot, float c0, float c1, float to_u) {
    const Mot *m = sm_mot(slot);
    if (!m || !m->root) return;
    float a[3], b[3];
    mot_root(m, c0, a), mot_root(m, c1, b);
    float fx = sinf(SM.face), fz = cosf(SM.face), dx = b[0] - a[0], dz = b[2] - a[2];
    SM.pos[0] += (dz * fx + dx * fz) * to_u, SM.pos[2] += (dz * fz - dx * fx) * to_u;
    SM.yoff = b[1];
}

// ---- Chicken Little: every driven frame after Sora's physics (next to party_tick): the AI, the move, the draw
static void sm_late(float kh, float to_u, int st) {
    g_sm_u = to_u;
    if (SM.st != SM_OFF && !SM.hidden) smx_late(kh, to_u);  // (s138) Tinker Bell (Peter Pan's intro)  (s139 review: before the Limit hook, so her light + heal go on in Never Land)
    if (sm_fps_late(kh, to_u, st)) return;  // (s137 fps) FPS Mode: the intro / the hidden chicken are drawn there
    if (SM.st == SM_OFF || SM.hidden || !SM.placed || SM.ach <= 0) return;
    (void)st;
    float feet[3];
    if (!sm_feet(feet)) return;
    uint64_t present = vk_frame_counter();
    SmEn en[64];
    int n = SM.st == SM_ACTIVE && !sm_fps_on() ? sm_enemies(en, 64) : 0;  // (s137 fps: no melee AI while the Limit runs)
    SM.battle = false;
    for (int k = 0; k < n; k++)
        if (en[k].life > 0 && sm_dist_cm(en[k].pos, feet, to_u) < SM_LEASH_CM) SM.battle = true;
    float c0 = SM.clock;
    bool moving = false;
    if (SM.st == SM_CALL || SM.st == SM_DISMISS || (SM.slot == SMS_ENTRY && SM.clock < sm_frames(SMS_ENTRY, 350.0f))) {  // (s136 review S3: a call cut short lets his entry play on) the entry / the exit: the baked motion, root motion, no AI
        SM.clock += kh;
        float fr = sm_frames(SM.slot, 350.0f);
        if (SM.clock > fr) SM.clock = fr;
        sm_root(SM.slot, c0, SM.clock, to_u);
        float cat[3] = {SM.pos[0], SM.pos[1], SM.pos[2]};
        if (SMD()->entry_shift && SM.slot == SMS_ENTRY) {  // (s137 review S2) his entry effects / sounds follow where the pose has him, not the origin
            float w[3];
            sm_entry_shift_world(SM.clock, to_u, w);
            cat[0] += w[0], cat[2] += w[2];
        }
        sm_cues(0, sm_mot(SM.slot), c0, SM.clock, cat);
        moving = SM.slot == SMS_ENTRY;
    } else {
        if (SM.slot == SMS_ENTRY || SM.slot == SMS_EXIT) {
            SM.atk = false, SM.band = PBAND_STAND;
            if (SMD()->entry_shift && SM.slot == SMS_ENTRY) {  // (Stitch) the run-in lives in the pose: move the origin to where his last frame stands, no blend
                float sh[3], fx = sinf(SM.face), fz = cosf(SM.face);
                sm_entry_shift(sh);
                SM.pos[0] += (sh[2] * fx + sh[0] * fz) * to_u, SM.pos[2] += (sh[2] * fz - sh[0] * fx) * to_u;
                sm_play(SMS_IDLE);
                SM.blend = -1;
                sm_fx_clear();  // (his entry effects: hb040.6 is a capped loop) end with the entry
            } else sm_play(SMS_IDLE);  // (the entry is over: idle, blended)
        }
        // the target: keep the current one while alive and near, else the nearest enemy to him within SM_SEARCH_CM (and SM_LEASH_CM of Sora)
        int cur = -1, best = -1;
        float bd = SM_SEARCH_CM;
        for (int k = 0; k < n; k++) {
            if (en[k].life <= 0) continue;
            if (SM.tgt && en[k].actor == SM.tgt && sm_dist_cm(en[k].pos, SM.pos, to_u) < SM_SEARCH_CM * 1.3f) cur = k;
            float d = sm_dist_cm(en[k].pos, SM.pos, to_u);
            if (SM.battle && d < bd && sm_dist_cm(en[k].pos, feet, to_u) < SM_LEASH_CM) bd = d, best = k;
        }
        if (!SM.atk && SM.yoff != 0) {  // (s136 review S4) an attack (202) ended up in the air: fall like gravity, 980 cm/s2 = 0.27 cm per KH frame2 (GUESS)
            SM.yv += 0.27f * kh, SM.yoff -= SM.yv * kh;
            if (SM.yoff <= 0) SM.yoff = 0, SM.yv = 0;
        }
        int tk = SMD()->natk <= 0 ? -1 : cur >= 0 ? cur : best;  // (s138: natk 0 = Genie, no attack state of his own: he only follows)
        SM.tgt = tk >= 0 ? en[tk].actor : 0;
        if (SM.atk) {  // an attack: face the target for its first frames, the baked motion + root, the GUESS hit window
            if (tk >= 0 && SM.clock < 12.0f) {
                float want = atan2f(en[tk].pos[0] - SM.pos[0], en[tk].pos[2] - SM.pos[2]), step = SM_TURN * kh;
                SM.face = pt_wrap(SM.face + pt_clampf(pt_wrap(want - SM.face), -step, step));
            }
            SM.clock += kh;
            sm_root(SM.slot, c0, SM.clock, to_u);
            sm_cues(0, sm_mot(SM.slot), c0, SM.clock, SM.pos);
            smx_atk_tick(c0, SM.clock);  // (s138) Genie's version change (90A)
            sm_hit_window(en, n, to_u);
            if (SM.clock >= sm_frames(SM.slot, 60.0f)) {
                SM.atk = false, SM.nhit = 0, SM.pause = 30.0f + (float)(rand() % 60), SM.idle_t = 0;
                SM.band = PBAND_STAND;
                float y = SM.yoff;  // (s136 review S4: no 141 cm snap after 202: he keeps his height and falls, above)
                sm_play(SMS_IDLE);
                SM.yoff = y, SM.yv = 0;
            }
        } else {
            if (SM.pause > 0) SM.pause -= kh;
            if (tk >= 0) {
                float ed = sm_dist_cm(en[tk].pos, SM.pos, to_u);
                if (ed > SM_ATK_CM) {
                    float goal[3] = {en[tk].pos[0], SM.pos[1], en[tk].pos[2]};
                    sm_steer(goal, to_u, kh, SM_STOP_CM, PARTY_BANDS, atan2f(en[tk].pos[0] - SM.pos[0], en[tk].pos[2] - SM.pos[2]), true,
                             ed > 450.0f ? PBAND_RUN : PBAND_WALK);
                    moving = SM.band != PBAND_STAND;
                } else {  // in reach: face him; swing when the pause is over
                    float want = atan2f(en[tk].pos[0] - SM.pos[0], en[tk].pos[2] - SM.pos[2]), step = SM_TURN * kh;
                    SM.face = pt_wrap(SM.face + pt_clampf(pt_wrap(want - SM.face), -step, step));
                    SM.band = PBAND_STAND;
                    if (SM.pause <= 0 && fabsf(pt_wrap(want - SM.face)) < 0.35f) {
                        SM.idle_t = 0;
                        SM.atk_i = smx_pick_atk(SM.tgt), SM.combo++, SM.atk = true, SM.nhit = 0, SM.win = -1;  // (s138 review: Peter Pan picks by KH2's battle act; others: combo % natk)
                        sm_play(SMD()->atk[SM.atk_i].slot);
                        sm_cues(0, sm_mot(SM.slot), -1, 0, SM.pos);
                        if (SM.log) LOG("[summon] attack %d (motion %d) on %llx at %.0f cm\n", SM.atk_i + 1, SMD()->atk[SM.atk_i].slot, (unsigned long long)SM.tgt, ed);
                    }
                }
            } else {  // follow Sora: 138 cm to his left, 82 cm ahead (the party's slot, GUESS the left side), the party's bands; far = back beside him
                if (!SM.battle) SM.idle_t += kh;
                float fx = sinf(S.face), fz = cosf(S.face), gx = fz * 138.0f + fx * 82.0f, gz = -fx * 138.0f + fz * 82.0f;
                float goal[3] = {feet[0] + gx * to_u, feet[1], feet[2] + gz * to_u};
                if (sm_dist_cm(SM.pos, feet, to_u) > PARTY_WARP_CM && st == LINK_GROUND) sm_place_beside(feet, to_u, "far from Sora");
                sm_steer(goal, to_u, kh, 0, PARTY_BANDS, S.face, false, 0);
                moving = SM.band != PBAND_STAND;
            }
            if (!SM.atk) sm_loco_motion(kh);
        }
    }
    sm_floor(feet, present, moving || SM.atk, kh, to_u);
    // ---- draw (the party's blend: the frozen last pose of the old motion lerps into the new one over SM_BLEND frames)
    int id = SM.ach, nb = SM.bones ? SM.bones(id) : 0;
    SM.shadow(id, SM.floor_s, blob_str(0.55f));
    float at[3] = {SM.pos[0], SM.floor_s + SM.yoff * to_u, SM.pos[2]};
    bool can = nb > 0 && nb <= SM_MAXB && SM.slot_pose;
    if (can && SM.blend == 0) {
        if (SM.slot_pose(id, SM.from_slot, SM.from_clock, g_sm_from)) SM.blend = 0.001f;
        else SM.blend = -1;
    }
    if (can && SM.blend > 0 && SM.slot_pose(id, SM.slot, SM.clock, g_sm_cur)) {
        SM.blend += kh;
        float w = SM.blend / SM_BLEND;
        if (w >= 1.0f) SM.blend = -1;
        else {
            for (int k = 0; k < nb; k++) {
                float *d = g_sm_cur + k * 12, *a2 = g_sm_from + k * 12, len[3];
                for (int r = 0; r < 3; r++) {
                    float la = sqrtf(v3dot(a2 + 3 * r, a2 + 3 * r)), lb = sqrtf(v3dot(d + 3 * r, d + 3 * r));
                    len[r] = la + (lb - la) * w;
                }
                for (int j = 0; j < 12; j++) d[j] = a2[j] + (d[j] - a2[j]) * w;
                ortho_rows(d, len);
            }
            SM.world(id, g_sm_cur, at, SM.face, to_u);
            return;
        }
    }
    SM.pose(id, SM.slot, SM.clock, at, SM.face, to_u);
}

// Sora's call, in place of his body (sora.c's draw chain, next to the skate rider): true = drawn here
static bool sm_call_draw(float kh, float to_u) {
    g_sm_u = to_u;
    (void)kh;
    if (sm_fps_hide_sora()) { if (SM.body && SM.off) SM.off(SM.body()); return true; }  // (s137 fps) KH2 draws Sora at alpha 0 in FPS Mode
    if (!SM.call_on || SM.acall <= 0 || !SM.pose) return false;
    float lm[12];
    if (!linkpos_link_mtx(lm)) return false;
    float feet[3] = {lm[3], lm[7], lm[11]}, cface = S.face;
    bool nls = nl_call_adj(feet, &cface);   // (s139 neverland) Sora hangs below Peter in the air, no shadow
    if (SM.body && SM.off) SM.off(SM.body());
    if (SM.shadow) SM.shadow(SM.acall, S.ground_z * g_k / 100.0f, nls ? 0.0f : blob_str(0.55f));
    feet[1] += gl_yoff_cm() * to_u;  // (s139 genielim) Genie's Limit lifts him (Master 277 cm, Valor 2 arc)
    SM.pose(SM.acall, SM.cslot, SM.cclock, feet, cface, to_u);
    return true;
}

// (s148 sumlv) dg_hit while a summon is out: KH2 3a9cf0 state 3 adds DriveDrain x 12 to the summon gauge (only while > 0, clamped to max)
static void sm_hit_refill(float drain) {
    if (SM.st != SM_ACTIVE && SM.st != SM_CALL) return;
    if (su_refill(drain, &SM.t, SM.total) > 0 && SU.prev >= 0 && SU.prev < SM.t) SU.prev = SM.t;  // (a refill gives no EXP: the tracker moves up with it)
}

// the summon gauge on the Drive plate (sora_form.h fm_gauge falls back to it)
static bool sm_gauge(int *bars, int *fill, int *max) {
    if (SM.st == SM_OFF || SM.total <= 0) return false;
    float t = SM.t < 0 ? 0 : SM.t;
    *max = (int)(SM.total / SM_BAR_FRAMES + 0.5f);
    *bars = (int)(t / SM_BAR_FRAMES);
    *fill = (int)((t - (float)*bars * SM_BAR_FRAMES) * 100 / SM_BAR_FRAMES);
    if (*bars >= *max) *bars = *max, *fill = 100;
    return true;
}

#include "sora_summon_tink.h"  // (s139 tink) Tinker Bell's light + heal
#include "sora_summon_pp.h"   // (s138 ppgenie) Peter Pan + Tinker Bell, Genie's versions
#include "sora_summon_fps.h"  // (s137 fps) Chicken Little's FPS Mode
#include "sora_summon_ohana.h"  // (s138 ohana) Stitch's Ohana!
#include "sora_summon_nl.h"  // (s139 neverland) Peter Pan's Never Land
#include "sora_summon_gl.h"  // (s139 genielim) Genie's version Limits + the version menu

static bool sm_live_set(const char *key, int v) {
    if (!strcmp(key, "summon")) {
        SM.on = v != 0;
        LOG("[summon] %s\n", SM.on ? "on" : "off");
    } else if (!strcmp(key, "scall")) {  // scall 1 = Chicken Little, 2 = Stitch, 3 = Genie, 4 = Peter Pan (the row number, 1-based)
        if (v) sm_request_def(v - 1, true);
    } else if (!strcmp(key, "sdone")) {
        if (v && SM.st == SM_ACTIVE) SM.req = 3;
        else if (v) LOG("[summon] sdone: not in the active stretch (%s)\n", SM_STNAME[SM.st]);
    } else if (!strcmp(key, "sdmg")) SM.dmg_pct = v < 1 ? 1 : v > 1000 ? 1000 : v, LOG("[summon] damage %d%%\n", SM.dmg_pct);
    else if (!strcmp(key, "sstr")) SM.str = v < 0 ? 0 : v > 255 ? 255 : v, LOG("[summon] Str %d%s\n", SM.str, SM.str ? "" : " (= Sora's)");
    else if (!strcmp(key, "sbars")) SM.bars = v < 0 ? 0 : v > 10 ? 10 : v, LOG("[summon] gauge %d bars%s\n", SM.bars ? SM.bars : su_gauge_bars(), SM.bars ? " (override)" : " (by Summon LV)");
    else if (!strcmp(key, "sendcall")) SM.endcall = v != 0, LOG("[summon] Sora's end call (253) %s\n", SM.endcall ? "on (NOT KH2)" : "off (KH2)");
    else if (!strcmp(key, "skb")) SM.kb = v != 0, SM.weapon && SM.acall > 0 ? SM.weapon(SM.acall, SM.kb != 0) : (void)0, LOG("[summon] Keyblade on the call actor %s\n", SM.kb ? "shown" : "hidden");
    else if (!strcmp(key, "slog")) SM.log = v != 0;
    else if (smx_live_set(key, v)) {}  // (s138) sgver
    else if (fp_live_set(key, v)) {}  // (s137 fps) sfps sfcmd fpslog fpscam ...
    else if (gl_live_set(key, v)) {}  // (s139 genielim) sgl sglcmd gllog gltime
    else if (oh_live_set(key, v)) {}  // (s138 ohana) sohana socmd ohlog ohtime ohcomp
    else if (nl_live_set(key, v)) {}  // (s139 neverland) snl snlcmd nllog nltime nlunit
    else return false;
    return true;
}

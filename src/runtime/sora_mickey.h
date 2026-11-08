// sora_mickey.h (s109 b22-mickey slice 1, s110 slices 2 + 3): KH2's Mickey rescue. Plan, KH2 rule, asset table: recon/S109_HELPER_MICKEY.md
// + recon/S110_HELPER_MICKEY.md. Included from sora.c after sora_roxas.h (hook lines: forward declarations near ca_live_set, the
// live-key chain, mk_tick after ca_survive, mk_survive_mask inside ca_survive, mk_body in sora_roxas.h's rox_tick).
//
// KH2 (khwiki King Mickey page): when Sora is KO'd in certain boss fights Mickey becomes playable (60 HP, 100 MP; combo, Pearl,
// D-Charge fills his Drive gauge; at 3 bars Healing Light = Sora's full HP / MP / Drive back; reduced to 1 HP first he uses a
// Hi-Potion = half of Sora's HP). Rescue chance 100 / 80 / 64 / 50 % (50 % from the fourth on). User ruling (s110): automatic by
// CHANCE, no choice screen; the fights = MK_BOSSES below; Ganon and the Guardians never (KH2's last fights: Game Over).
// BotW mapping: a boss within MK_R metres = the fight. While armed the exe clamp (linkpos_survive_set mask 4 = ANY fatal hit leaves
// 1 quarter heart, even a lone one at 1 quarter heart) keeps Link alive and counts it (what 3). That count = Sora's HP hit 0:
//   roll won: Mickey's body (FORM_MICKEY draw actor, own motions / hits / Keyblade W_EX200) replaces Sora, Donald + Goofy off,
//           Link's hits are dropped (linkpos_guard_set) and charged to Mickey's 60 HP; the player runs / jumps / swings him with
//           the normal Sora controls (Pearl / D-Charge are slice 4); after MK_CHARGE_MS (D-Charge stand-in, GUESS) or "set mkheal 1"
//           he plays his cast (A370, GUESS: Healing Light) and Link is healed to full, then Sora is back; Mickey's HP at 1 = Hi-Potion
//           (A110 item motion) = half of Link's hearts.
//   roll lost: "It's all over...": the clamp is off until Link is healed / the fight is over, so the next hit kills him for real.
// s110m (mickeyfx): the rescue now follows KH2's rule instead of the 8 s stand-in: Mickey has HIS OWN Drive gauge (3 bars of 100) that his hits
// (the atkp rows' DriveDrain) and D-Charge (HOLD Triangle = Switch X, A122 slot 772, loops while held) fill; at 3 bars Triangle = Healing Light
// (A121 slot 768, 204 frames, heal at the effect frame 138) = Link full HP + Sora's MP / Drive full; 1 HP = the Hi-Potion. Square = Pearl (A370
// slot 776, 18 of his 100 MP, a homing 5-hit light shot). Sounds / effects: sfx.bin rows at 80000 + slot (mkse_v1) and fx.bin mk_* elements
// (recon/S110_HELPER_MICKEYFX.md). `mkcharge <ms>` is now only an optional timeout fallback (default 0 = off).
// Never while Roxas is the body (KH2 has no rescue in the prologue; no shared draw actor needed). Live keys (DLL only): mickey 0/1
// (default 0 = everything off), mkforce 1 (KO now, no boss needed), mkheal 1, mkany 1 (any enemy counts as a boss), mkchance <pct>
// (-1 = the KH2 table), mkcharge <ms>, mkdmg <pct>, mkanim 0/1, mklog 0/1. Log lines "[mickey] ...". An exe without the survive /
// guard exports: no effect (logged once); an exe without the 11th draw actor / without the data files: the rescue runs with Sora's body.
#ifndef SORA_MICKEY_H
#define SORA_MICKEY_H

extern int g_hud_life_max;
int linkpos_enemy_name(uint64_t actor, char *buf, int n);

// (s117 mkchoice) sora_mkchoice.h (after sora_camp.h): KH2's choice screen. mc_open = false: not drawable -> the automatic roll.
static bool mc_open(void);                 // open the screen; false = camp font / exe export missing
static int mc_result(void);                // 0 still choosing, 1 "I won't give up!", 2 "It's all over...", 3 aborted
static void mc_close(void);
static void mc_kill_link(void);            // "It's all over...": Link dies for real (BotW's death -> the KH2 Game Over screen)
static void mc_kill_watch(void);           // (s117 review) no BotW death after that write = life 1 back (the next hit kills)
static int g_mkchoice = 1, g_mkroll = 1;   // live mkchoice 0 = today's automatic roll; mkroll 0 = the choice alone decides (no chance roll)

#define MK_R 35.0f          // metres: a boss this close = "in the fight" (GUESS)
#define MK_FORGET_MS 30000  // no boss for this long = the fight is over, the rescue count starts again
#define MK_HP 60            // Mickey's HP (khwiki)
#define MK_SLOT_ITEM 36     // P_EX200 A110 (ITEM, the Hi-Potion), VERIFIED slot in the mset dump
#define MK_SLOT_CAST 768    // P_EX200 A121 = Healing Light (VERIFIED: PHH mickey.kh.json motions.healing_light), 204 frames, effect_caster 6 at frame 138
#define MK_CAST_AT 138.0f
#define MK_SLOT_DCHG 772    // P_EX200 A122 = D-Charge (VERIFIED: mickey.kh.json motions.d_charge): 76 frames, wind-up 0..60, loop 60..76 while held
#define MK_SLOT_PEARL 776   // P_EX200 A370 = Pearl (VERIFIED: motions.pearl): 70 frames, burst effect at 40
#define MK_GMAX 300         // his Drive gauge: 3 bars of 100 (khwiki: "fill the Drive gauge to 3 bars")
#define MK_PEARL_MP 18      // cmds 452 Pearl Cost 18 (VERIFIED mickey.kh.json cmds)
#define MK_PEARL_AT 40.0f   // first hit frame (the burst effect trigger)
#define MK_PEARL_HITS 5     // khwiki: "up to 5 hits"
#define MK_PEARL_GAP 6.0f   // frames between the hits (GUESS)
#define MK_PEARL_R 25.0f    // metres: the homing reach (GUESS)
#define MK_ANIM_WAIT_MS 2500  // waiting for a free ground moment to play the cast, then the heal comes without it

// (s110, user: "cut mickey damage to half") g_mk_dmg default 100 -> 50 (% of the HP share a hit costs him; live "set mkdmg N")
static int g_mickey = 0, g_mk_any = 0, g_mk_chance = -1, g_mk_charge = 0, g_mk_dmg = 50, g_mk_log = 1, g_mk_anim = 1;
// (s119, user: "Mickey has no hit reaction, HP drops, no flinch") the exe guard drops Link's hits in Mickey mode, so sora.c's
// flinch (Link's life drop count) never saw one: a hit charged to Mickey (guard block count) now plays KH2's flinch.
// Live "set mkflinch 0" = old (no flinch).
static int g_mkflinch = 1;
// (s119) "set mkrag 0" = every hit fully dropped (old); 1 = large hits (explosions, blow-aways) knock him down like BotW's Link
static int g_mkrag = 1;
// (s110, user: "give him KH2's own Mickey strength") KH2 lvup row 4 = Mickey (00battle.bin lvup, character order Sora / Donald /
// Goofy / Mickey, VERIFIED read 2026-11-03): Strength per LV 1..99. His LV = Sora's LV (GUESS: KH2 scales Mickey with the story).
// The equipped Keyblade's Str bonus stays on top (GUESS stand-in for Kingdom Key D's). Live "set mkstr 0" = Sora's Strength.
static const uint8_t MK_STR[99] = {4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18,
    19, 19, 20, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37,
    37, 38, 38, 39, 39, 40, 40, 41, 41, 42, 43, 44, 44, 45, 45, 46, 46, 47, 47, 48, 48, 49, 49, 50, 50, 51, 51, 52, 52, 53, 54, 55, 55,
    56, 56, 57, 57, 58};
static int g_mkstr = 1;
static int mk_str_delta(void) {  // Mickey's Str minus Sora's at this LV (sl_hit adds sl_str_bonus to Sora's)
    if (!g_mkstr) return 0;
    int l = sl_lv(), i = sl_lvstr ? (l < 1 ? 1 : l > 99 ? 99 : l) - 1 : 0;
    return MK_STR[i] - sl_str_of(&SL_LV[i]);
}
static int g_mk_hitmul = 4, g_mk_passive = 2;  // s111 (GUESS): x hit fill (KH2 DriveDrain x4 = a 3-bar gauge in ~30 hits), passive points/s (so a rescue never stalls)
static int g_mkball = 1;  // (s117) live "set mkball 0" = the old instant Pearl hits
static int g_mk_dchg = 75, g_mk_pearl_pow = 40, g_mk_mpregen = 5, g_mkfx = 1;  // D-Charge points/s (GUESS), Pearl power per hit (GUESS), MP/s (GUESS), live "set mkfx 0" = silent / no fx
static struct {
    bool bound;
    void (*surv_set)(int);
    int (*surv_count)(int *);
    void (*guard_set)(int);
    int (*guard_blocks)(void);
    int (*guard_dmg)(void);
    bool active;            // Mickey mode
    bool body, party_was;   // Mickey's body is drawn / the user's party flag
    int hp;                 // Mickey's HP (units of KH2 HP)
    uint64_t t0;            // GetTickCount64 at the start of Mickey mode
    int phase;              // 0 free, 1 waiting for the cast to start, 2 cast playing (heal at its effect frame), 3 D-Charge held, 4 Pearl playing
    uint64_t t_ready;       // last 'Healing Light ready' chime
    int gauge;              // his Drive gauge 0..MK_GMAX (points; 100 = one bar)
    float mp;               // his MP (100 at the start)
    float gfrac;            // D-Charge fractions
    bool tri_prev, sq_prev;
    uint64_t t_tick;        // last mk_tick ms (dt)
    int dch_h[2];           // D-Charge aura fx handles (-1 = none; sora_fx handles start at 0)
    int bfx_h[6]; bool bfx_tip[6];  // (s110m review) his bound elements (tip / root glows) follow him: sora_fx_move each frame
    uint64_t load_seen;     // (s110m review) g_load_end when he came: a load ends the rescue
    int pearl_swing, pearl_n; uint64_t pearl_target; float pearl_pos[3];
    // (s117 pearl) KH2's Pearl as a visible homing ball: flies from the Keyblade tip to the target, the hits land on arrival
    struct { bool on; int fx, hits, mode, swing; float t, next, speed, pos[3], dir[3]; uint64_t target; } ball;  // mode 0 flying, 1 on the target (the hits)
    uint64_t force_t;       // when mkforce was written (a pending forced KO times out after 5 s)
    float fx_last; int fx_swing;
    bool potion, healed;    // this finish is the Hi-Potion / the heal already happened in phase 2
    uint64_t t_phase;
    int seen_surv, seen_blocks;
    int rescues;            // rescues this fight (the chance steps down)
    uint64_t boss_ms;       // last time a boss was near
    bool choosing;          // (s117 mkchoice) the KH2 "I won't give up!" / "It's all over..." choice is up (sora_mkchoice.h)
    bool nearb, doomed;     // a boss is near this frame / the roll was lost (the clamp stays off until he is healed)
    char boss[48];
    bool force, heal, warned;  // live one-shots
    unsigned rng;
} MK;

static bool mk_body(void) { return MK.body; }

// BotW boss classes (names from SL_ENEMY, runtime/sora_levels.h). ONE table (s110, the main session's pick; the user may change it):
// Lynel (all), Hinox + Stalnox (Enemy_Giant_*), Talus (Enemy_Golem_* minus Golem_Little*), Molduga (Enemy_Sandworm*), the four
// Blights (Enemy_SiteBoss_*). NOT the Guardians, NOT Ganon (KH2's final fights give no rescue).
static const char *const MK_BOSSES[] = {"Enemy_Lynel", "Enemy_Giant_", "Enemy_Golem_", "Enemy_Sandworm", "Enemy_SiteBoss_"};
static bool mk_is_boss(const char *n) {
    if (g_mk_any) return true;
    if (!strncmp(n, "Enemy_Golem_Little", 18)) return false;
    for (size_t i = 0; i < sizeof MK_BOSSES / sizeof *MK_BOSSES; i++)
        if (!strncmp(n, MK_BOSSES[i], strlen(MK_BOSSES[i]))) return true;
    return false;
}

static void mk_bind(void) {
    if (MK.bound) return;
    MK.bound = true;
    HMODULE m = GetModuleHandleA(NULL);
    MK.surv_set = (void (*)(int))(void *)GetProcAddress(m, "linkpos_survive_set");
    MK.surv_count = (int (*)(int *))(void *)GetProcAddress(m, "linkpos_survive_count");
    MK.guard_set = (void (*)(int))(void *)GetProcAddress(m, "linkpos_guard_set");
    MK.guard_blocks = (int (*)(void))(void *)GetProcAddress(m, "linkpos_guard_blocks");
    MK.guard_dmg = (int (*)(void))(void *)GetProcAddress(m, "linkpos_guard_last_damage");
    if (MK.surv_count) MK.seen_surv = MK.surv_count(NULL);
    if (MK.guard_blocks) MK.seen_blocks = MK.guard_blocks();
    MK.rng = (unsigned)GetTickCount64() | 1;
    LOG("[mickey] exe exports: survive %s, guard %s\n", MK.surv_set && MK.surv_count ? "found" : "MISSING (no rescue)",
        MK.guard_set && MK.guard_blocks && MK.guard_dmg ? "found" : "MISSING (Mickey takes no hits)");
}

// The mask ca_survive ORs in: armed (a boss near, or a forced KO) = 4 (any fatal hit leaves 1). Never as Roxas, never after a lost roll.
// (An exe older than s110 ignores bit 4: then only Second Chance / Once More saves exist and the rescue never triggers.)
// (s110 review) also while Mickey is out: the exe drop skips falls (and anything in the first frames), so without the clamp a fall
// could kill Link mid-rescue; a clamp then is no new KO (mk_tick: ko only while !MK.active).
static int mk_survive_mask(void) { return g_mickey && !MK.doomed && !RX.on && (MK.active || MK.nearb || MK.force || MK.choosing) ? 4 : 0; }
static bool mk_drops_hits(void) { return MK.active || MK.choosing; }  // (s117 review: + the choice screen; sora_guard.h ran after mk_tick and cleared its drop)

static int mk_chance_pct(void) {
    if (g_mk_chance >= 0) return g_mk_chance > 100 ? 100 : g_mk_chance;
    static const int tab[] = {100, 80, 64, 50};  // khwiki: 100 / 80 / 64 / 50 % then 50 % for all later rescues
    return tab[MK.rescues < 3 ? MK.rescues : 3];
}

static bool mk_assets_ok(void) {
    if (!g_fm_mk_load) g_fm_mk_load = g_fm_load_now = true;  // (his draw actor loads on the first rescue only: 11th exe actor slot, 26 MB pose)
    fm_init(), fm_load_actor();
    return FM.set_body && FM.actor[FORM_MICKEY] > 0 && g_nfmots_tab[FORM_MICKEY] > 0 && g_nmkhslots > 0;
}

static void mk_enter_body(void) {
    if (!mk_assets_ok()) {
        if (!MK.warned) MK.warned = true, LOG("[mickey] no Mickey body (draw actor %d, motions %d, hits %d; data/forms/mickey_*.bin installed? exe with 11 draw actors?): the rescue runs with Sora's body\n",
                                                FM.actor[FORM_MICKEY], g_nfmots_tab[FORM_MICKEY], g_nmkhslots);
        return;
    }
    if (fm_in_form()) fm_revert("mickey rescue");
    g_fform = FORM_MICKEY, g_fmots_on = true;
    FM.set_body(FM.actor[FORM_MICKEY]);
    MK.body = true, MK.party_was = P.on;
    P.on = false, party_all_off();
    S.special = SP_NONE;  // (the hit that KO'd Sora left him in a reaction; Mickey stands up)
    rox_motion_reset();
    LOG("[mickey] MICKEY: body actor %d, %d motion slots, %d hit slots, party off (was %d)\n", FM.actor[FORM_MICKEY],
        g_nfmots_tab[FORM_MICKEY], g_nmkhslots, MK.party_was);
}

static void mk_leave_body(void) {
    if (!MK.body) return;
    g_fmots_on = false, g_fform = FORM_VALOR;
    if (FM.set_body) FM.set_body(ofx_actor_base());  // (s146 outfit)
    MK.body = false;
    if (MK.party_was) P.on = true;
    rox_motion_reset();
    LOG("[mickey] SORA: base body back, party %s\n", P.on ? "on (Donald + Goofy join)" : "off");
}

static void mk_aura_fade(void) {  // the D-Charge aura (two looping elements) fades when the button is let go
    for (int k = 0; k < 2; k++) {
        if (MK.dch_h[k] >= 0 && sora_fx_alive(MK.dch_h[k])) sora_fx_fadeout(MK.dch_h[k], 8);
        MK.dch_h[k] = -1;
    }
}

static void mk_end(const char *why) {
    if (!MK.active) return;
    MK.active = false;
    mk_aura_fade();
    for (int k = 0; k < 6; k++) {  // (s110m review: his bound glows go with him)
        if (MK.bfx_h[k] >= 0 && sora_fx_alive(MK.bfx_h[k])) sora_fx_fadeout(MK.bfx_h[k], 8);
        MK.bfx_h[k] = -1;
    }
    MK.phase = 0, MK.gauge = 0, MK.gfrac = 0;
    if (MK.guard_set) MK.guard_set(0);
    mk_leave_body();
    LOG("[mickey] Mickey mode over after %llu ms (%s)\n", (unsigned long long)(GetTickCount64() - MK.t0), why);
}

// the packs (sfx.bin mkse_v1 / fx.bin mk_hit): without them Mickey is silent / plain (sora.c sfx_slot, sora_ffx.h, sora_kbfx.h ask these)
static bool mk_se_ok(void) { return g_mkfx && sora_sfx_id("mkse_v1") >= 0; }
static bool mk_fx_ok(void) {  // (s110m review: the id once - fx.bin loads at start; sora_fx_id's unknown-name warn list holds only 8 names)
    static int id = -2;
    if (!g_mkfx || !sora_fx_ready()) return false;
    if (id == -2) id = sora_fx_id("mk_hit");
    return id >= 0;
}
static const char *mk_kbfx_name(const char *kind) {  // his Keyblade's hit sparks (W_EX200.a.us w_ex200.1 / .2 / .3), else NULL = the caller's own
    if (!IN_MICKEY || !mk_fx_ok()) return NULL;
    return !strcmp(kind, "hit") ? "mk_hit" : !strcmp(kind, "hit_finish") ? "mk_hit_finish" : !strcmp(kind, "hit_guard") ? "mk_hit_guard" : NULL;
}
static bool mk_eat_x(void) { return MK.active && MK.body; }  // Triangle (Switch X) is D-Charge / Healing Light here, never a climb try or a pick-up

// his Drive gauge (hits + D-Charge); 100 points = a bar, 3 bars = full. The HUD (sora_drive.h dg_hud) shows it in place of Sora's while he is out.
static void mk_gauge_add(float x, const char *why) {
    if (!MK.active || MK.gauge >= MK_GMAX || x <= 0) return;
    int b0 = MK.gauge / 100;
    MK.gfrac += x;
    int p = (int)MK.gfrac;
    MK.gfrac -= (float)p;
    MK.gauge += p;
    if (MK.gauge >= MK_GMAX) MK.gauge = MK_GMAX, sora_sfx_event("drive_full", 1.0f), LOG("[mickey] Drive gauge FULL (%s): Triangle = Healing Light\n", why);
    else if (MK.gauge / 100 > b0) sora_sfx_event("drive_bar", 1.0f), LOG("[mickey] Drive gauge %d bar(s) (%s)\n", MK.gauge / 100, why);
}
static bool mk_gauge(int *bars, int *fill, int *max) {
    if (!MK.active || !MK.body) return false;
    *max = MK_GMAX / 100, *bars = MK.gauge / 100, *fill = MK.gauge % 100;
    if (MK.gauge >= MK_GMAX) *bars = *max, *fill = 100;
    return true;
}
static int mk_pearl_cost(void) { return MK_PEARL_MP; }  // (s119 mkmenu: the command pane's "can't afford" state)
static bool mk_hud(float *hp, float *hpm, float *mp, float *mpm) {
    if (!MK.active || !MK.body) return false;
    *hp = (float)MK.hp, *hpm = (float)MK_HP, *mp = MK.mp, *mpm = 100.0f;
    return true;
}
// DriveDrain of his atkp rows (VERIFIED: mickey.kh.json atkp, SubId 0): x0.75 on a common enemy as Sora's (sora_drive.h DG_DRIVE_ENEMY)
static void mk_hit(int atkp, uint64_t actor) {
    static const struct { uint16_t atkp; uint8_t drain; } T[] = {{1088, 3}, {1089, 2}, {1090, 3}, {1091, 3}, {1092, 6}, {1121, 3}, {1122, 3}, {1151, 6}, {1190, 3}, {1199, 3}};
    if (!MK.active) return;
    bool boss = false;
    for (int i = 0; i < SL_NC; i++)
        if (SL.c[i].used && SL.c[i].actor == actor && SL.c[i].named) boss = (SL.c[i].flags & 1) != 0;
    for (size_t i = 0; i < sizeof T / sizeof *T; i++)
        if (T[i].atkp == atkp) {
            char why[40];
            snprintf(why, sizeof why, "hit atkp %d", atkp);
            mk_gauge_add((float)(T[i].drain * g_mk_hitmul) * (boss ? 1.0f : 0.75f), why);
            return;
        }
}

// plays one of Mickey's own mset slots as an owning motion (as the chestprev preview does); false = not baked
static bool mk_play_slot(int slot) {
    const Mot *m = mot_slot(slot);
    if (!m) return false;
    S.motion = slot / 4, S.clock = 0, S.mot = m, S.swing++;
    S.special = SP_ITEM, S.next = 0;
    if (mk_se_ok()) sora_sfx_motion(sfx_slot(m), -1, 0, NULL);  // (frame-0 triggers: his voice)
    LOG("[mickey] playing slot %d (%d frames = %.2f s)\n", slot, m->frames, m->frames / 60.0);
    return true;
}

static void mk_finish(void) {  // the heal itself
    int mx = g_hud_life_max > 0 ? g_hud_life_max : 12, q = MK.potion ? mx / 2 : mx;
    int v = link_heal(q);
    LOG("[mickey] %s: Sora healed +%d (life %d)\n", MK.potion ? "Hi-Potion" : "Healing Light", q, v);
    if (!MK.potion) {  // KH2: Healing Light "completely restores Sora's HP, MP and Drive gauge"; the Hi-Potion only HP
        MP.mp = MP.mp_max, MP.charge = MP.charge_total = 0;  // (s110m review: also ends a running MP Charge, as debugall / the form exit do)
        dg_add(dg_max() * 100, "Healing Light");
    }
    MK.heal = false;
}

// (s117 pearl) Pearl as KH2's ball of light. Data: PHH recorded P_EX200's world-space elements ex200.2 (the orb, a 600 f loop), ex200.3 (the
// hit burst) and ex200.4 (a bubble pop) which fx.bin (s117 bake) packs as mk_pearl_ball / _hit / _pop; the Pearl motion's only triggers
// are the cast glow (el 0 @0) and the burst (el 1 @40), so the ball is spawned by code at 40 (VERIFIED: no trigger names it). Speed, turn
// rate and the "lingers on the enemy while the 5 hits tick" rule are GUESS (atkp 1151 sub 0 / 1: Element 5, Interval 6, which the old
// MK_PEARL_GAP already uses, is the likely ball hit-box: GUESS). An old fx.bin without mk_pearl_ball = the old instant hits, logged once.
#define MK_BALL_SPEED0 24.0f  // cm per KH frame at the start (GUESS), +2 per frame
#define MK_BALL_SPEEDMAX 56.0f
#define MK_BALL_TURN 0.07f    // rad per KH frame homing (GUESS; Wisdom's bullet is 0.0314)
#define MK_BALL_ARRIVE 70.0f  // cm: closer than this to the target's aim point = it lands
static bool mk_ball_ok(void) {
    static int id = -2, warned = 0;
    if (!g_mkball || !g_mkfx || !sora_fx_ready()) return false;
    if (id == -2) id = sora_fx_id("mk_pearl_ball");
    if (id < 0 && !warned) warned = 1, LOG("[mickey] Pearl ball: fx.bin has no mk_pearl_ball (old pack): the old instant hits stay\n");
    return id >= 0;
}
static void mk_ball_kill(bool fade) {
    if (MK.ball.on) { if (fade) sora_fx_fadeout(MK.ball.fx, 6); else sora_fx_kill(MK.ball.fx); }
    MK.ball.on = false;
}
// one Pearl hit (hit k of MK_PEARL_HITS): damage, spark, sound, gauge. With the ball the first hit also plays mk_pearl_hit, the last mk_pearl_pop (GUESS roles).
static void mk_pearl_strike(int k, uint64_t actor, const float epos[3], const float spark[3]) {
    int d = sl_hit(actor, -1, g_mk_pearl_pow, g_dmg_base, 1);
    linkpos_damage_enemy(actor, d, F54_SMALL);
    const char *nm = mk_fx_ok() ? "mk_hit" : "hit";
    if (MK.ball.on && mk_ball_ok()) nm = k == 0 ? "mk_pearl_hit" : k == MK_PEARL_HITS - 1 ? "mk_pearl_pop" : nm;
    sora_fx_event(nm, spark, S.face, 0.01f * g_k);
    sora_sfx_hit(1090, 1.0f, epos);
    mk_gauge_add(k == 0 ? 4.5f : 0.75f, "Pearl hit");
    LOG("[mickey] Pearl hit %d/%d on %llx: damage %d\n", k + 1, MK_PEARL_HITS, (unsigned long long)actor, d);
}
// frame 40: the ball leaves the tip tp for the locked / nearest living enemy within MK_PEARL_R, else straight ahead
static void mk_ball_launch(const float tp[3], float to_u) {
    uint64_t ea[64];
    float ep[64][3], lp[3];
    int32_t el[64];
    mk_ball_kill(true);
    memset(&MK.ball, 0, sizeof MK.ball);
    if (linkpos_link_pos(lp)) {
        int n = linkpos_enemies(ea, ep, el, 64);
        float best = MK_PEARL_R;
        for (int i = 0; i < n; i++) {
            if (el[i] <= 0) continue;
            float d = sqrtf((ep[i][0] - lp[0]) * (ep[i][0] - lp[0]) + (ep[i][1] - lp[1]) * (ep[i][1] - lp[1]) + (ep[i][2] - lp[2]) * (ep[i][2] - lp[2]));
            if (g_lock.locked && ea[i] == g_lock.actor) d = 0;
            if (d < best) best = d, MK.ball.target = ea[i];
        }
    }
    memcpy(MK.ball.pos, tp, sizeof MK.ball.pos);
    MK.ball.dir[0] = sinf(S.face), MK.ball.dir[1] = 0, MK.ball.dir[2] = cosf(S.face);
    MK.ball.speed = MK_BALL_SPEED0;
    MK.ball.fx = sora_fx_event("mk_pearl_ball", tp, S.face, to_u);
    MK.ball.on = MK.ball.fx >= 0;
    if (MK.ball.on) MK.ball.swing = S.swing;  // (s117 review: this cast's hits belong to the ball, even after a live "set mkball 0" mid-flight)
    LOG("[mickey] Pearl ball launched (fx %d) at %.2f %.2f %.2f, target %llx%s\n", MK.ball.fx, tp[0], tp[1], tp[2], (unsigned long long)MK.ball.target,
        MK.ball.target ? "" : " (none in reach: straight ahead, fades)");
}
// every frame (sora.c, after mk_fx_tick): fly, home, land, tick the hits
static void mk_ball_tick(float kh, float to_u) {
    if (!MK.ball.on) return;
    if (!IN_MICKEY || !MK.active || !sora_fx_alive(MK.ball.fx)) { mk_ball_kill(false); return; }
    uint64_t ea[64];
    float ep[64][3];
    int32_t el[64];
    int n = linkpos_enemies(ea, ep, el, 64), ti = -1;
    for (int i = 0; i < n && MK.ball.target; i++) if (ea[i] == MK.ball.target && el[i] > 0) ti = i;
    MK.ball.t += kh;
    if (MK.ball.mode == 0) {
        if (MK.ball.target && ti < 0) MK.ball.target = 0, LOG("[mickey] Pearl ball: the target is gone, straight on\n");
        if (ti >= 0) {
            float aim[3] = {ep[ti][0] - MK.ball.pos[0], ep[ti][1] + 90 * to_u - MK.ball.pos[1], ep[ti][2] - MK.ball.pos[2]};
            float d = sqrtf(aim[0] * aim[0] + aim[1] * aim[1] + aim[2] * aim[2]);
            if (d <= MK_BALL_ARRIVE * to_u + MK.ball.speed * kh * to_u * 0.5f) {  // lands: the hits tick from here, the orb rides the enemy
                MK.ball.mode = 1, MK.ball.next = MK.ball.t;
                LOG("[mickey] Pearl ball reached %llx after %.0f frames\n", (unsigned long long)MK.ball.target, MK.ball.t);
            } else {
                for (int c = 0; c < 3; c++) aim[c] /= d;
                float dot = MK.ball.dir[0] * aim[0] + MK.ball.dir[1] * aim[1] + MK.ball.dir[2] * aim[2];
                float ang = acosf(dot > 1 ? 1 : dot < -1 ? -1 : dot), lim = MK_BALL_TURN * kh;
                if (ang <= lim) memcpy(MK.ball.dir, aim, sizeof MK.ball.dir);
                else if (ang > 3.0f && sinf(ang) < 1e-3f) {  // (s117 review) target straight behind: slerp's sin(ang) ~ 0 = NaN; turn about the up axis
                    float c = cosf(lim), s = sinf(lim), x = MK.ball.dir[0], z = MK.ball.dir[2];
                    if (x * x + z * z < 1e-12f) x = sinf(S.face), z = cosf(S.face);  // (straight up / down: Mickey's facing)
                    MK.ball.dir[0] = x * c + z * s, MK.ball.dir[2] = -x * s + z * c;
                    float l = sqrtf(MK.ball.dir[0] * MK.ball.dir[0] + MK.ball.dir[1] * MK.ball.dir[1] + MK.ball.dir[2] * MK.ball.dir[2]);
                    for (int c2 = 0; c2 < 3; c2++) MK.ball.dir[c2] /= l;
                } else if (ang > 1e-5f) {
                    float u = lim / ang, sa = sinf(ang), a = sinf((1 - u) * ang) / sa, b = sinf(u * ang) / sa, l = 0;
                    for (int c = 0; c < 3; c++) MK.ball.dir[c] = a * MK.ball.dir[c] + b * aim[c], l += MK.ball.dir[c] * MK.ball.dir[c];
                    l = sqrtf(l);
                    for (int c = 0; c < 3; c++) MK.ball.dir[c] /= l;
                }
            }
        }
        if (MK.ball.mode == 0) {
            MK.ball.speed = fminf(MK_BALL_SPEEDMAX, MK.ball.speed + 2 * kh);
            for (int c = 0; c < 3; c++) MK.ball.pos[c] += MK.ball.dir[c] * MK.ball.speed * kh * to_u;
            sora_fx_move(MK.ball.fx, MK.ball.pos, atan2f(MK.ball.dir[0], MK.ball.dir[2]));
            if (MK.ball.t >= 120.0f || (!MK.ball.target && MK.ball.t >= 50.0f)) {  // no target (or never got there): the orb fades out
                LOG("[mickey] Pearl ball fades (%s)\n", MK.ball.target ? "never arrived" : "no target");
                mk_ball_kill(true);
            }
        }
    }
    if (MK.ball.on && MK.ball.mode == 1) {
        if (ti < 0) { LOG("[mickey] Pearl ball: the target died after %d hits\n", MK.ball.hits); mk_ball_kill(true); return; }
        float aim[3] = {ep[ti][0], ep[ti][1] + 90 * to_u, ep[ti][2]};
        memcpy(MK.ball.pos, aim, sizeof aim);
        sora_fx_move(MK.ball.fx, aim, S.face);
        while (MK.ball.hits < MK_PEARL_HITS && MK.ball.t >= MK.ball.next) {
            float sp[3] = {ep[ti][0], ep[ti][1] + 1.0f, ep[ti][2]};
            mk_pearl_strike(MK.ball.hits, ea[ti], ep[ti], sp);
            MK.ball.hits++, MK.ball.next += MK_PEARL_GAP;
        }
        if (MK.ball.hits >= MK_PEARL_HITS) mk_ball_kill(true);
    }
}

// effect_caster triggers of the playing motion (the baked motion's frame triggers, type 1, p1 = P_EX200.a.us element) -> the packed mk_* elements
static void mk_fx_tick(const float feet[3], float to_u) {
    static int seen_slot = -1;
    if (!IN_MICKEY || !S.mot || !mk_fx_ok()) { MK.fx_swing = -1; return; }
    {  // (s110m review) the bound elements follow the tip / his feet
        float tp[3];
        cast_tip(feet, to_u, tp);
        for (int b = 0; b < 6; b++)
            if (MK.bfx_h[b] >= 0) { if (sora_fx_alive(MK.bfx_h[b])) sora_fx_move(MK.bfx_h[b], MK.bfx_tip[b] ? tp : feet, S.face); else MK.bfx_h[b] = -1; }
        for (int k = 0; k < 2; k++) if (MK.dch_h[k] >= 0 && sora_fx_alive(MK.dch_h[k])) sora_fx_move(MK.dch_h[k], feet, S.face);
    }
    float prev = MK.fx_swing != S.swing || seen_slot != S.mot->slot ? -1.0f : MK.fx_last;
    MK.fx_swing = S.swing, seen_slot = S.mot->slot, MK.fx_last = S.clock;
    if (S.clock < prev) return;  // (D-Charge's loop: no re-trigger of the wind-up's frames)
    static const struct { int8_t el; const char *name; bool tip; } EL[] = {
        {0, "mk_pearl_cast", true}, {1, "mk_pearl_burst", true}, {5, "mk_heal_a", true}, {6, "mk_heal_b", true},
        {8, "mk_dcharge_a", false}, {9, "mk_dcharge_b", false}, {22, "mk_swing_b", false}, {23, "mk_swing_a", true}};
    for (int i = 0; i < S.mot->nft; i++) {
        uint16_t fr;
        int16_t el;
        memcpy(&fr, S.mot->ft + i * 8, 2), memcpy(&el, S.mot->ft + i * 8 + 4, 2);
        if (S.mot->ft[i * 8 + 2] != 1 || !(prev < (float)fr && (float)fr <= S.clock)) continue;
        for (size_t k = 0; k < sizeof EL / sizeof *EL; k++) {
            if (EL[k].el != el) continue;
            float tp[3];
            if (EL[k].tip) cast_tip(feet, to_u, tp);
            const char *fxn = EL[k].name;
            if (el == 0 && mk_ball_ok() && sora_fx_id("mk_pearl_cast2") >= 0) fxn = "mk_pearl_cast2";  // (s117: the cast glow with its loop frames)
            int h = sora_fx_event(fxn, EL[k].tip ? tp : feet, S.face, to_u);
            if (el == 1 && S.mot->slot == MK_SLOT_PEARL && mk_ball_ok()) mk_ball_launch(tp, to_u);
            if (el == 8 || el == 9) MK.dch_h[el - 8] = h;
            else if (h >= 0)  // (s110m review: the bound ones (fx.bin bone 0x4000 | 3 = tip, 0x4000 = root) follow him, as sora_ffx.h's)
                for (int b = 0; b < 6; b++)
                    if (MK.bfx_h[b] < 0 || !sora_fx_alive(MK.bfx_h[b])) { MK.bfx_h[b] = h, MK.bfx_tip[b] = EL[k].tip; break; }
            if (g_mk_log) LOG("[mickey] fx element %d (%s) at frame %u of slot %d -> fx %d\n", el, EL[k].name, fr, S.mot->slot, h);
        }
    }
    // D-Charge held longer than the aura's 150 frames: light it again
    if (MK.phase == 3 && S.clock >= 60.0f && MK.dch_h[0] >= 0 && !sora_fx_alive(MK.dch_h[0]) && (MK.dch_h[1] < 0 || !sora_fx_alive(MK.dch_h[1]))) {
        MK.dch_h[0] = sora_fx_event("mk_dcharge_a", feet, S.face, to_u);
        MK.dch_h[1] = sora_fx_event("mk_dcharge_b", feet, S.face, to_u);
    }
}

// Pearl (Square, 18 MP): the homing light shot, up to 5 hits on the nearest (or locked) enemy; at the burst frame 40 of A370
static void mk_pearl_hits(int n, const uint64_t *ea, float (*ep)[3], const int32_t *el, const float lp[3]) {
    if (mk_ball_ok() || MK.ball.swing == MK.pearl_swing) return;  // (s117) the ball (mk_ball_launch at frame 40, mk_ball_tick) lands the hits
    if (MK.pearl_n == 0 && !MK.pearl_target) {  // pick the target at the first hit: the locked one, else the nearest living enemy in reach
        float best = MK_PEARL_R;
        for (int i = 0; i < n; i++) {
            if (el[i] <= 0) continue;
            float d = sqrtf((ep[i][0] - lp[0]) * (ep[i][0] - lp[0]) + (ep[i][1] - lp[1]) * (ep[i][1] - lp[1]) + (ep[i][2] - lp[2]) * (ep[i][2] - lp[2]));
            if (g_lock.locked && ea[i] == g_lock.actor) d = 0;
            if (d < best) best = d, MK.pearl_target = ea[i];
        }
        if (!MK.pearl_target) { LOG("[mickey] Pearl: no enemy within %.0f m, the shot fizzles\n", MK_PEARL_R); MK.pearl_n = MK_PEARL_HITS; return; }
    }
    while (MK.pearl_n < MK_PEARL_HITS && S.clock >= MK_PEARL_AT + (float)MK.pearl_n * MK_PEARL_GAP) {
        int idx = -1;
        for (int i = 0; i < n; i++) if (ea[i] == MK.pearl_target && el[i] > 0) idx = i;
        if (idx < 0) { MK.pearl_n = MK_PEARL_HITS; break; }
        float sp[3] = {ep[idx][0], ep[idx][1] + 1.0f, ep[idx][2]};
        mk_pearl_strike(MK.pearl_n, ea[idx], ep[idx], sp);
        MK.pearl_n++;
    }
}

// (s110m review) sora.c, after the clock advance and before the motion end check: the held D-Charge loops 60..76 (A122 loop 60, flags 1;
// an owning motion would end at 76). Here instead of in mk_tick so a slow frame (kh up to 6) can never run it past its end.
static void mk_dchg_loop(void) {
    if (MK.phase != 3 || !IN_MICKEY || S.special != SP_ITEM || !S.mot || S.mot->slot != MK_SLOT_DCHG || !(g_held & BTN_CLIMB)) return;
    float lo = S.mot->loop > 0 && S.mot->loop < S.mot->frames ? (float)S.mot->loop : 60.0f, hi = (float)S.mot->frames;
    if (S.clock >= hi && hi > lo) S.clock = lo + fmodf(S.clock - hi, hi - lo);
}

// Every frame, right after ca_survive in sora_mod_frame: life = Link's quarter hearts.
static void mk_tick(int life, uint64_t present) {
    (void)present;
    mc_kill_watch();
    if (!g_mickey) {
        if (MK.active) mk_end("set mickey 0");
        if (MK.choosing) MK.choosing = false, mc_close(), MK.guard_set ? MK.guard_set(0) : (void)0;
        MK.nearb = MK.doomed = false, MK.force = MK.heal = false;
        return;
    }
    mk_bind();
    uint64_t now = GetTickCount64();
    // the fight: a boss alive within MK_R
    float lp[3] = {0, 0, 0};
    uint64_t ea[64];
    float ep[64][3];
    int32_t el[64];
    int ne = 0;
    MK.nearb = false;
    if (life > 0 && linkpos_link_pos(lp)) {
        int n = ne = linkpos_enemies(ea, ep, el, 64);
        float best = MK_R;
        for (int i = 0; i < n; i++) {
            if (el[i] <= 0) continue;
            float d = sqrtf((ep[i][0] - lp[0]) * (ep[i][0] - lp[0]) + (ep[i][1] - lp[1]) * (ep[i][1] - lp[1]) + (ep[i][2] - lp[2]) * (ep[i][2] - lp[2]));
            char nm[48];
            if (d >= best || !linkpos_enemy_name(ea[i], nm, sizeof nm) || !mk_is_boss(nm)) continue;
            best = d, MK.nearb = true, snprintf(MK.boss, sizeof MK.boss, "%s", nm);
        }
    }
    if (MK.nearb) MK.boss_ms = now;
    else if (MK.boss_ms && now - MK.boss_ms > MK_FORGET_MS && !MK.active) {
        if (MK.rescues) LOG("[mickey] fight over: rescue count %d -> 0\n", MK.rescues);
        MK.rescues = 0, MK.boss_ms = 0, MK.doomed = false;
    }
    if (MK.doomed && life > 1) MK.doomed = false;  // (healed / new life: armed again)
    if (MK.force && now - MK.force_t > 5000) MK.force = false, LOG("[mickey] forced KO timed out (Sora was not the body within 5 s)\n");
    if (RX.on) {  // (no rescue as Roxas; (s110m) a forced KO stays PENDING until Sora is the body: "set char 2" + "set mkforce 1" in one go)
        if (MK.active) mk_end("Roxas is the body");
        if (MK.choosing) MK.choosing = false, mc_close(), MK.guard_set ? MK.guard_set(0) : (void)0;
        return;
    }
    if (MK.body && P.on) MK.party_was = true, P.on = false, party_all_off();
    // the KO: the clamp saved a fatal hit while armed (what 3), or a forced one
    bool ko = false, chose = false;  // (chose: s117 review - the roll after "I won't give up!", not the font-missing fallback)
    if (MK.surv_count) {
        int what = 0, n = MK.surv_count(&what);
        if (n != MK.seen_surv) MK.seen_surv = n, ko = what == 3 && !MK.active;
    }
    if (MK.force && !MK.active) ko = true;
    MK.force = false;
    if (MK.choosing) {  // (s117) the choice screen is up: hits are dropped, the clamp holds, the pad is the screen's
        if (MK.guard_set) MK.guard_set(1);
        int r = life <= 0 ? 3 : mc_result();
        if (r == 0 && (!g_mickey || !g_mkchoice || g_load_end != MK.load_seen)) r = 3;
        if (r) {
            MK.choosing = false;
            mc_close();
            if (MK.guard_set) MK.guard_set(0);
            if (r == 3) LOG("[mickey] choice aborted (life %d, load or live key)\n", life);
            else if (r == 2) {
                LOG("[mickey] choice: \"It's all over...\" -> KH2 Game Over\n");
                MK.rescues++, MK.doomed = true;
                mc_kill_link();
            } else {
                LOG("[mickey] choice: \"I won't give up!\"\n");
                ko = chose = true;  // (falls into the roll below: the KH2 chance, then Mickey or the Game Over)
                goto roll;
            }
        }
        return;
    }
    if (ko && g_mkchoice && !MK.active && mc_open()) {
        MK.choosing = true, MK.load_seen = g_load_end;
        if (MK.guard_blocks) MK.seen_blocks = MK.guard_blocks();
        LOG("[mickey] choice open (boss %s, life %d, rescue #%d, chance %d%%): up / down + A\n", MK.nearb ? MK.boss : "none/forced", life,
            MK.rescues + 1, mk_chance_pct());
        return;
    }
    roll:
    if (ko) {
        int pct = chose && !g_mkroll ? 100 : mk_chance_pct();
        MK.rng = MK.rng * 1664525u + 1013904223u;
        bool ok = (int)((MK.rng >> 8) % 100) < pct;
        LOG("[mickey] Sora's HP hit 0 (boss %s, life %d): rescue #%d, %d%% -> %s\n", MK.nearb ? MK.boss : "none/forced", life, MK.rescues + 1, pct,
            ok ? "I won't give up! Mickey comes" : "It's all over... (the clamp is off: the next hit kills him)");
        MK.rescues++;
        if (ok) {
            MK.active = true, MK.hp = MK_HP, MK.t0 = now, MK.phase = 0, MK.potion = MK.healed = false;
            MK.gauge = 0, MK.gfrac = 0, MK.mp = 100.0f, MK.t_tick = now, MK.pearl_n = 0, MK.pearl_target = 0;
            MK.tri_prev = (g_held & BTN_CLIMB) != 0, MK.sq_prev = (g_held & BTN_ROLL) != 0;
            MK.dch_h[0] = MK.dch_h[1] = -1, MK.load_seen = g_load_end;
            for (int k = 0; k < 6; k++) MK.bfx_h[k] = -1;
            if (MK.guard_blocks) MK.seen_blocks = MK.guard_blocks();
            mk_enter_body();
            LOG("[mickey] KH2 rule: hit enemies / HOLD Triangle (D-Charge) to fill the Drive gauge (3 bars), then Triangle = Healing Light; Square = Pearl (%d MP)\n", MK_PEARL_MP);
        } else {
            MK.doomed = true;
            if (chose) mc_kill_link();  // (s117) the lost roll after the choice: Game Over now, not at the next hit
        }
    }
    if (!MK.active) return;
    float dt = MK.t_tick ? (float)(now - MK.t_tick) / 1000.0f : 0.0f;
    if (dt > 0.2f) dt = 0.2f;
    MK.t_tick = now;
    if (g_mk_passive > 0 && MK.body) mk_gauge_add((float)g_mk_passive * dt, "passive");  // (s111 GUESS) a slow trickle
    if (MK.gauge >= MK_GMAX && MK.phase == 0 && now - MK.t_ready > 6000) MK.t_ready = now, sora_sfx_event("drive_bar", 1.0f);  // (s111) "ready" reminder chime
    // Mickey mode: Link's hits are dropped and charged to Mickey's 60 HP (floor 1: he cannot be KO'd, at 1 HP he drinks a Hi-Potion)
    // (s119) mode 2 (new exe): a large hit keeps BotW's knock-down / ragdoll with no life lost; an older exe treats 2 as 1
    if (MK.guard_set) MK.guard_set(g_mkrag ? 2 : 1);
    if (MK.guard_blocks && MK.guard_dmg) {
        int b = MK.guard_blocks();
        if (b != MK.seen_blocks) {
            MK.seen_blocks = b;
            int dmg = MK.guard_dmg(), mx = g_hud_life_max > 0 ? g_hud_life_max : 12;
            int loss = (dmg * MK_HP * g_mk_dmg) / (mx * 100);  // the share of Link's max life the hit would have cost (GUESS)
            if (loss < 1) loss = 1;
            MK.hp -= loss;
            if (MK.hp < 1) MK.hp = 1;
            if (g_mk_log) LOG("[mickey] hit %d quarter hearts -> Mickey -%d HP (%d left)\n", dmg, loss, MK.hp);
        }
    }
    if (life <= 0 || g_loading || g_load_end != MK.load_seen) { mk_end("Link died / loading"); return; }  // (s110m review: + a load the frame code returned through)
    // the pad: Triangle (Switch X) and Square (Switch Y), own edges
    bool tri = (g_held & BTN_CLIMB) != 0, sq = (g_held & BTN_ROLL) != 0;
    bool tri_press = tri && !MK.tri_prev, sq_press = sq && !MK.sq_prev;
    MK.tri_prev = tri, MK.sq_prev = sq;
    bool free = S.act == ACT_IDLE && S.takeoff <= 0 && !S.air_frames && !S.special;
    // (s118 user: "when i press triangle to dash in the water mickey shouts and his drive starts to rise") Triangle / Square
    // belong to BotW's Link while he swims (swim latch) or climbs (state 0x90): no D-Charge / Healing Light / Pearl then
    if (g_rag_swim || linkpos_link_state_word() == 0x90) free = false;
    if (MK.phase != 4 && MK.mp < 100.0f) MK.mp = MK.mp + (float)g_mk_mpregen * dt > 100.0f ? 100.0f : MK.mp + (float)g_mk_mpregen * dt;
    // (s110m review) without his body (no data / old exe) nothing fills his gauge: the old 8 s stand-in then, unless mkcharge says otherwise
    int chg = g_mk_charge > 0 ? g_mk_charge : !MK.body ? 8000 : 0;
    if (MK.phase == 0) {
        if (MK.hp <= 1) MK.potion = true, MK.phase = 1, MK.t_phase = now, LOG("[mickey] 1 HP: Hi-Potion\n");
        else if (MK.heal || (chg > 0 && now - MK.t0 >= (uint64_t)chg)) {  // (cheat key / the optional timeout fallback)
            MK.potion = false, MK.phase = 1, MK.t_phase = now;
            LOG("[mickey] Healing Light by %s\n", MK.heal ? "set mkheal" : MK.body ? "the mkcharge timeout" : "the 8 s stand-in (no Mickey body)");
        } else if (tri_press && free && MK.body) {  // (s110m review: Sora's body = no D-Charge / Pearl: mot_slot would play SORA's slots 772 / 776)
            if (MK.gauge >= MK_GMAX) MK.potion = false, MK.phase = 1, MK.t_phase = now, LOG("[mickey] Triangle at a full gauge: Healing Light\n");
            else if (mk_play_slot(MK_SLOT_DCHG)) MK.phase = 3, LOG("[mickey] D-Charge (gauge %d/%d)\n", MK.gauge, MK_GMAX);
        } else if (sq_press && free && MK.body && MK.mp >= MK_PEARL_MP && mk_play_slot(MK_SLOT_PEARL)) {
            MK.mp -= MK_PEARL_MP, MK.phase = 4, MK.pearl_n = 0, MK.pearl_target = 0, MK.pearl_swing = S.swing;
            LOG("[mickey] Pearl (MP left %.0f)\n", MK.mp);
        }
    }
    if (MK.phase == 3) {  // D-Charge: the wind-up plays to frame 60, then 60..76 loops while Triangle is held (mk_dchg_loop); the gauge fills in the loop
        bool over = S.special != SP_ITEM || !S.mot || S.mot->slot != MK_SLOT_DCHG;
        // (s111 diag: in the user's test every D-Charge ended right after it started, gauge never rose - log why)
        const char *why = over ? (S.special != SP_ITEM ? "motion cut (special)" : "motion changed") : NULL;
        if (!over) {
            // (s111) the wind-up always plays through and counts (a Triangle TAP = one wind-up of points); the loop only while held
            if (tri || S.clock < 60.0f) mk_gauge_add((float)g_mk_dchg * dt, "D-Charge");
            if ((!tri && S.clock >= 60.0f) || MK.gauge >= MK_GMAX) over = true, S.special = SP_NONE, why = !tri ? "Triangle not held" : "gauge full";
        }
        if (over) {
            mk_aura_fade();
            MK.phase = 0;
            LOG("[mickey] D-Charge over: gauge %d/%d (%s at clock %.1f, special %d, raw held 0x%llx)\n", MK.gauge, MK_GMAX, why, S.clock,
                (int)S.special, (unsigned long long)g_held);
        }
    }
    if (MK.phase == 4) {  // Pearl: the hits at frame 40 + 6 k
        bool over = S.special != SP_ITEM || !S.mot || S.mot->slot != MK_SLOT_PEARL || S.swing != MK.pearl_swing;
        if (!over) mk_pearl_hits(ne, ea, ep, el, lp);
        if (over) MK.phase = 0;
    }
    if (MK.phase == 1) {  // the cast starts on a free ground moment (not mid-swing / air); else after MK_ANIM_WAIT_MS the heal comes alone
        if (!MK.body || !g_mk_anim) mk_finish(), mk_end(MK.potion ? "Hi-Potion" : "Healing Light");
        else if (free && mk_play_slot(MK.potion ? MK_SLOT_ITEM : MK_SLOT_CAST)) MK.phase = 2, MK.t_phase = now;
        else if (now - MK.t_phase > MK_ANIM_WAIT_MS) mk_finish(), mk_end(MK.potion ? "Hi-Potion (no free moment for the motion)" : "Healing Light (no free moment for the motion)");
    } else if (MK.phase == 2) {
        float at = MK.potion ? mot_frame_trigger(S.mot, 6, 18) : MK_CAST_AT;
        bool done = S.special != SP_ITEM || now - MK.t_phase > 6000;
        if (!MK.healed && (S.clock >= at || done)) mk_finish(), MK.healed = true;
        if (done) mk_end(MK.potion ? "Hi-Potion" : "Healing Light");
    }
}

static bool mk_live_set(const char *key, int v) {
    if (!strcmp(key, "mickey")) g_mickey = v != 0, LOG("[mickey] live mickey %d\n", g_mickey);
    else if (!strcmp(key, "mkforce")) { if (v) MK.force = true, MK.force_t = GetTickCount64(), LOG("[mickey] forced KO (pending until Sora is the body; times out after 5 s)\n"); }
    else if (!strcmp(key, "mkheal")) { if (v) MK.heal = true; }
    else if (!strcmp(key, "mkchoice")) g_mkchoice = v != 0, LOG("[mickey] mkchoice %d (%s)\n", g_mkchoice, g_mkchoice ? "KH2 choice screen on a KO" : "automatic roll");
    else if (!strcmp(key, "mkroll")) g_mkroll = v != 0, LOG("[mickey] mkroll %d (%s)\n", g_mkroll, g_mkroll ? "the KH2 chance rolls after \"I won't give up!\"" : "\"I won't give up!\" always works");
    else if (!strcmp(key, "mkany")) g_mk_any = v != 0, LOG("[mickey] mkany %d (any enemy counts as a boss)\n", g_mk_any);
    else if (!strcmp(key, "mkchance")) g_mk_chance = v, LOG("[mickey] mkchance %d (-1 = KH2 table)\n", v);
    else if (!strcmp(key, "mkcharge")) g_mk_charge = v < 0 ? 0 : v, LOG("[mickey] mkcharge %d ms (0 = the KH2 rule only, no timeout)\n", g_mk_charge);
    else if (!strcmp(key, "mkdchg")) g_mk_dchg = v < 1 ? 1 : v, LOG("[mickey] D-Charge %d points/s (100 = a bar)\n", g_mk_dchg);
    else if (!strcmp(key, "mkfill")) g_mk_hitmul = v < 1 ? 1 : v, LOG("[mickey] hit fill x%d\n", g_mk_hitmul);
    else if (!strcmp(key, "mkpass")) g_mk_passive = v < 0 ? 0 : v, LOG("[mickey] passive fill %d points/s\n", g_mk_passive);
    else if (!strcmp(key, "mkpearl")) g_mk_pearl_pow = v < 1 ? 1 : v, LOG("[mickey] Pearl power %d per hit\n", g_mk_pearl_pow);
    else if (!strcmp(key, "mkball")) g_mkball = v != 0, LOG("[mickey] mkball %d (Pearl %s)\n", g_mkball, g_mkball ? "= the homing ball of light" : "= the old instant hits");
    else if (!strcmp(key, "mkmp")) g_mk_mpregen = v < 0 ? 0 : v, LOG("[mickey] MP regen %d/s\n", g_mk_mpregen);
    else if (!strcmp(key, "mkfx")) g_mkfx = v != 0, LOG("[mickey] mkfx %d (Mickey's own sounds / effects)\n", g_mkfx);
    else if (!strcmp(key, "mkmenu")) g_mkmenu = v != 0, LOG("[mkmenu] mkmenu %d (%s)\n", g_mkmenu, g_mkmenu ? "Mickey's KH2 command pane: Jump / Pearl / Attack" : "old: Sora's command menu while Mickey is out");
    else if (!strcmp(key, "mkpad")) g_mkpad = v == 2 ? 2 : 1, LOG("[mkmenu] mkpad %d (%s button icons)\n", g_mkpad, g_mkpad == 2 ? "Xbox" : "PlayStation");
    else if (!strcmp(key, "mkface")) g_mkface = v != 0, LOG("[mickey] mkface %d (HUD portrait: %s)\n", g_mkface, g_mkface ? "Mickey while he is out" : "always Sora");
    else if (!strcmp(key, "mkdmg")) g_mk_dmg = v, LOG("[mickey] mkdmg %d%%\n", v);
    else if (!strcmp(key, "mkstr")) g_mkstr = v != 0, LOG("[mickey] mkstr %d (Mickey's KH2 Strength: %+d over Sora's at this LV)\n", g_mkstr, mk_str_delta());
    else if (!strcmp(key, "mkanim")) g_mk_anim = v != 0;
    else if (!strcmp(key, "mklog")) g_mk_log = v != 0;
    else if (!strcmp(key, "mkflinch")) g_mkflinch = v != 0, LOG("[mickey] mkflinch %d (%s)\n", g_mkflinch, g_mkflinch ? "KH2 flinch on a hit" : "old: no flinch");
    else if (!strcmp(key, "mkrag")) g_mkrag = v != 0, LOG("[mickey] mkrag %d (%s)\n", g_mkrag, g_mkrag ? "large hits knock him down (new exe)" : "every hit dropped");
    else if (!strcmp(key, "mkhit")) g_mkhit = v != 0, LOG("[mkhit] mkhit %d (his hit/combo fixes: reach, hop stop gap, no land cut, A330 -> A331 -> A335)\n", g_mkhit);
    else if (!strcmp(key, "mkhitlog")) g_mkhitlog = v != 0, LOG("[mkhit] mkhitlog %d (a [sora] sweep line every frame of a window within 5 u of an enemy)\n", g_mkhitlog);
    else if (!strcmp(key, "mkreach")) g_mk_reach = v < 0 ? 0 : v / 100.0f, LOG("[mkhit] mkreach %.2f u extra sweep radius\n", g_mk_reach);
    else if (!strcmp(key, "mkstop")) g_mk_stop = (float)v, LOG("[mkhit] mkstop %d cm hop stop gap\n", v);
    else return false;
    return true;
}

#endif

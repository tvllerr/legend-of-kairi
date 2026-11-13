// sora_plimit.h (s133 helper "limits"): KH2's PARTY LIMITS in the command menu: Comet (Sora + Donald) and Whirli-Goof (Sora + Goofy),
// and the small data-driven player the later Limits (Trinity, Duck Flare, Knocksmash) reuse. Included by sora.c after sora_react.h.
// Report: recon/S133_HELPER_LIMITS.md (decoded choreography, GUESS list, installs, tests). Nothing here is tested in the game.
//
// WHAT IS KH2 (VERIFIED from limit/fm/donald.bar + goofy.bar: BDX scripts disassembled with PHH's bdx_vm, motion sets carved and baked,
// 00battle limt / atkp rows, 03system cmd rows + sys.bar text):
//   limt 5 Comet (cmd 282, Donald + Sora), limt 3 Whirli-Goof (cmd 255, Goofy + Sora); Cost 255 = all MP, then MP Charge.
//   Whirli-Goof: the first press chain is the script's counter (L104 / L814 / L1219): the Limit starts motion 252 (Sora A930 + Goofy), a
//     Triangle press inside the motion's range-26 window (param 254, GFR 30..80) restarts it; after the 2nd press the prompt becomes
//     Whirli-Goofra (256) and plays 253 (window 40..80); after 3 presses of that (count 5) Whirli-Goofga (257) = motion 254 (230 frames,
//     Sora atkp 710 + Goofy atkp 642, 22..190). Hits come from the MOTIONS' range-10 windows: 252 Sora Keyblade 637 / Goofy shield 640,
//     253 638 / 641, 254 710 / 642 (atkp Power / Interval / EnemyReaction rows below).
//   Comet: Donald appears at bg_get_random_pos_air(Sora, 200, 250, -150, -150), motion 253 (70 frames, trigger 1 at frame 5 = the first
//     volley: up to 5 distinct enemies, each 3 comets 20 frames apart = atkp 664 SubId 0 / 1 / 2 = Power 25 / 50 / 50, 6 frames after the
//     effect), then he hovers (252, range 26 param 279 = "Comet Rain" 281 may be chosen) for the limit time 300 frames (cmd Time); the Rain
//     = motion 254 (210 frames), trigger 1 at frame 108 starts 31 area blasts, one every 6 frames, each atkp 665 Power 100 r 300 h 200.
//   Elements of the Limit's PAX: lim_donald_0..6 / lim_goofy_0..5 (tools_s133l/limitfx.py appends them to fx.bin by name).
// WHAT IS A GUESS (marked GUESS below and listed in the report): the hit volumes (KH2 uses the weapon / object colliders), the hit flags
//   (BotW has no pull / hold), Donald's hover place, the comet search radius, the blast ring, no camera, no motion SE, the time-out rules.
// (s150) Duck Flare (limt 6, cmd 344) added: report recon/S150_HELPER_DUCK.md. Keys lpdf*, lpforce 6.
// (s149) Knocksmash (limt 4, cmd 338) added: report recon/S149_HELPER_KNOCK.md. Keys: lpks, lpksshield, lpksdive, lpkssnd, lpksfx; lpforce 4 starts it.
// Live keys (sora.c "set" parser, "// s133 limits"): lpsteer 0|1|2 (s134: the stick steers Whirli-Goof's spins, default 1), limitparty 0|1 (0 = no Limit entry, a running one ends), lpdiag 0|1 (log every hit),
//   lpforce <limt id> (ONE-SHOT debug start, 3 = Whirli-Goof, 5 = Comet: remove it from live_test.txt after it fired), lpauto 0|1 (press the
//   prompts for you at the window's start: a test without a pad).
#pragma once

#define PLOG(...) LOG(__VA_ARGS__)
#define PL_ACT 3                  // 0 Sora, 1 Donald, 2 Goofy (a member's party index = actor - 1)
#define PL_MAXH 24                // enemies one hit window remembers
#define PL_COMETS 56
#define PL_FXH 40

enum { PLG_NONE, PLG_VOLLEY, PLG_RAIN, PLG_SHIELD, PLG_DUO, PLG_DIVE };  // (s149 Knocksmash: PLG_SHIELD = K0's thrown shield, PLG_DUO = Duo Raid's two volleys, PLG_DIVE = Goofy's solo homing dive)
enum { PSN_SORA_VOICE, PSN_GOOFY_VOICE, PSN_SE };  // (s149) a stage sound: Sora's vtbl voice / Goofy's vtbl voice / a named sfx.bin cue
enum { PLB_FEET = -1, PLB_TIP = -2, PLB_CHEST = -3 };  // fx anchor: the actor's feet / Sora's Keyblade tip / 100 cm above the feet

typedef struct { const char *name; int actor, anchor; float at; bool loop, first_only; } PlFx;  // at: stage frame
typedef struct { int actor, atkp, power; float a, b, every, r, h; int ehit; } PlHit;          // window frames a..b, cm
typedef struct { uint8_t kind, actor, vtbl; bool first_only; float at; const char *name; } PlSnd;  // (s149) at: stage frame; actor: whose place a PSN_SE plays at (0 Sora, 2 Goofy)
typedef struct {
    int slot[PL_ACT];            // motion slot per actor (0 = none): Sora 1200.. (mot_of(slot / 4)), members' pose / motion bins
    float len, win_a, win_b;     // stage frames (GFR); the Triangle window (win_b 0: none)
    float att[3];                // Goofy's T(0) in Sora's motion frame (x left, y up, z forward cm; Info of the carved motion)
    float push_v, push_k, push_decay;  // Sora's script move (goofy.bar L294 / L978): cap cm per frame, x distance, decay per frame
    int gen;                     // PLG_*
    float gen_at, tlimit;        // generator frame, time limit (frames since the step began; 0 none)
    bool loop;
    PlFx fx[8];
    int nfx;
    PlHit hit[2];
    int nhit;
    PlSnd snd[8];                // (s149) the motions' voice / SE triggers (0 for Comet / Whirli-Goof: unchanged)
    int nsnd;
    bool sora_free;              // (s149) Sora is handed back to the player for this stage (Knocksmash's solo dive)
} PlStage;
typedef struct { uint8_t stage, auto_next; uint16_t prompt; } PlStep;  // prompt: the cmd offered in this step's window (0 none)
typedef struct {
    const char *name;
    int cmd, limt, member;       // member: party index (0 Donald, 1 Goofy)
    const PlStage *st;
    int nst;
    const PlStep *step;
    int nstep;
    bool sora_motion;            // Sora plays the Limit's motions (Whirli-Goof) and is held; else he stays free (Comet)
    const char *spark;           // (s149) EffectOnHit 1 spark of this Limit (NULL: the Comet / Whirli-Goof choice in pl_hit_one)
    bool custom;                 // (s150) Duck Flare: its own flow (pl_df_*), not the press-chain steps
} PlDef;

static const struct { int cmd; const char *name; } PL_CMDS[] = {  // sys.bar text of the cmd rows (label +0x8: 0x8498 / 0x8499 / 0x849a / 0x9e53 / 0x9e54)
    {255, "Whirli-Goof"}, {283, "Whirli-Goof"}, {256, "Whirli-Goofra"}, {257, "Whirli-Goofga"}, {282, "Comet"}, {281, "Comet Rain"},
    {338, "Knocksmash"}, {339, "Knocksmash"}, {340, "Duo Raid"}, {341, "Cosmo Boost"},
    {344, "Duck Flare"}, {347, "Rocket Flare"}, {348, "Megaduck Flare"}};  // (s150) cmd 344 / 347 / 348  // (s149) sys.bar names of cmd 338-341
static const char *pl_cmd_name(int cmd) {
    for (size_t i = 0; i < sizeof PL_CMDS / sizeof PL_CMDS[0]; i++)
        if (PL_CMDS[i].cmd == cmd) return PL_CMDS[i].name;
    return "?";
}

// ---- Whirli-Goof (goofy.bar): 252 A930 / 253 A931 / 254 A932; Sora 1200 / 1204 / 1208 (= id 300 / 301 / 302), Goofy the same slots
static const PlStage PLS_WHIRLI[3] = {
    {.slot = {1200, 0, 1200}, .len = 90, .win_a = 30, .win_b = 80, .att = {0, 0, 0},
     .push_v = 50, .push_k = 0.4f, .push_decay = 0.2f,  // L294: speed = min(0.4 x distance, 50), x (1 - 0.2) per frame, toward the target
     .nfx = 3, .fx = {{"lim_goofy_3", 0, PLB_FEET, 0, false, false}, {"lim_goofy_2", 2, PLB_FEET, 0, false, true}, {NULL}},
     .nhit = 2, .hit = {{0, 637, 1, 0, 70, 8, 250, 150, 1}, {2, 640, 1, 0, 70, 8, 250, 150, 2}}},   // GUESS: r 250 / h 150 cm volumes
    {.slot = {1204, 0, 1204}, .len = 100, .win_a = 40, .win_b = 80, .att = {-47.27f, 0, 0},
     .push_v = 5, .push_k = 0, .push_decay = 0.01f,     // L978: 5.0, x (1 - 0.01) per frame
     .nfx = 1, .fx = {{"lim_goofy_4", 0, PLB_CHEST, 0, false, false}, {NULL}},  // PAX bone 85 (a Sora bone): GUESS his chest
     .nhit = 2, .hit = {{0, 638, 20, 0, 70, 12, 250, 150, 1}, {2, 641, 20, 0, 70, 12, 250, 150, 2}}},
    {.slot = {1208, 0, 1208}, .len = 230, .win_a = 0, .win_b = 0, .att = {-46.14f, 0, -57.62f},
     .nfx = 1, .fx = {{"lim_goofy_5", 0, PLB_TIP, 20, false, false}, {NULL}},
     .nhit = 2, .hit = {{0, 710, 65, 22, 190, 20, 250, 150, 1}, {2, 642, 65, 24, 190, 20, 250, 150, 2}}},
};
static const PlStep PLSTEP_WHIRLI[7] = {{0, 0, 283}, {0, 0, 283}, {0, 0, 256}, {1, 0, 256}, {1, 0, 256}, {1, 0, 257}, {2, 0, 0}};

// ---- Comet (donald.bar): 253 A921 = 1304 (intro), 252 A920 = 1300 (hover), 254 A922 = 1308 (Rain)
static const PlStage PLS_COMET[3] = {
    {.slot = {0, 1304, 0}, .len = 70, .gen = PLG_VOLLEY, .gen_at = 5,
     .nfx = 2, .fx = {{"lim_donald_0", 1, PLB_CHEST, 0, true, true}, {"lim_donald_1", 1, PLB_CHEST, 20, false, false}, {NULL}}},
    {.slot = {0, 1300, 0}, .len = 90, .win_a = 0, .win_b = 9999, .tlimit = 300, .loop = true},
    {.slot = {0, 1308, 0}, .len = 210, .gen = PLG_RAIN, .gen_at = 108,
     .nfx = 1, .fx = {{"lim_donald_6", 1, PLB_CHEST, 0, false, false}, {NULL}}},
};
static const PlStep PLSTEP_COMET[3] = {{0, 1, 0}, {1, 0, 281}, {2, 0, 0}};

// ---- (s149) Knocksmash (goofy2.bar, limt 4, cmd 338; Sora + Goofy). KH2 truth: recon/S139_RESEARCH_LIMITS2.md sections 2, 3, 6.
// Sora slots 1400 / 1408 / 1420 (= motion ids 350 / 352 / 355), Goofy 1400 / 1408 / 1420 / 1428 (dive) / 1424 (landing). The windows are
// the printed GFR windows (range 26 param 254); S139 caveat: the baked flag can drop up to ~6 frames later - adjust these four if the test says so.
#define KS_K0_WIN_A 30.0f
#define KS_K0_WIN_B 92.0f
#define KS_K1_WIN_A 64.0f
#define KS_K1_WIN_B 94.0f
#define KS_K0_THROW 20.0f   // Goofy's trigger id 9 in 1400 (VERIFIED, S139)
#define KS_K1_THROW 18.0f   // Goofy's trigger id 9 in 1408 (VERIFIED, S139)
static const PlStage PLS_KNOCK[5] = {
    // K0 Knocksmash: Sora 104 f / Goofy 112 f; time frozen (range 45) the whole motion
    {.slot = {1400, 0, 1400}, .len = 104, .win_a = KS_K0_WIN_A, .win_b = KS_K0_WIN_B, .att = {-54.3f, 0.3f, 5.8f}, .gen = PLG_SHIELD, .gen_at = KS_K0_THROW,
     .nfx = 3, .fx = {{"lim_goofy2_0", 0, PLB_FEET, 0, false, true}, {"lim_goofy2_14", 2, PLB_CHEST, 0, false, false}, {"lim_goofy2_1", 0, PLB_FEET, 12, false, false}, {NULL}},  // e0 bound at LIMIT_START (once); e14 on Goofy's hand (GUESS: chest); e1 Sora root @12
     .nsnd = 6, .snd = {{PSN_SORA_VOICE, 0, 4, false, 17}, {PSN_GOOFY_VOICE, 2, 4, false, 0}, {PSN_SE, 0, 0, false, 10, "ks_se500_41"}, {PSN_SE, 2, 0, false, 20, "ks_se500_36"},
                        {PSN_SE, 0, 0, true, 0, "ks_se999_108"}, {PSN_SE, 2, 0, false, 0, "ks_se999_115"}}},
    // K1 Duo Raid: 106 f
    {.slot = {1408, 0, 1408}, .len = 106, .win_a = KS_K1_WIN_A, .win_b = KS_K1_WIN_B, .att = {-50.5f, 0, 0}, .gen = PLG_DUO, .gen_at = KS_K1_THROW,
     .nsnd = 3, .snd = {{PSN_SORA_VOICE, 0, 4, false, 0}, {PSN_GOOFY_VOICE, 2, 4, false, 0}, {PSN_SE, 2, 0, false, 1, "ks_se500_37"}}},
    // K2 Cosmo Boost: 160 f, no window; Goofy's weapon window atkp 712 from his frame 100 (range 10 `100..-1`, VERIFIED S139)
    {.slot = {1420, 0, 1420}, .len = 160, .att = {-69.9f, 0, 0},
     .nfx = 7, .fx = {{"lim_goofy2_6", 2, PLB_CHEST, 44, false, false}, {"lim_goofy2_5", 0, PLB_TIP, 56, false, false}, {"lim_goofy2_7", 0, PLB_FEET, 96, false, false},
                      {"lim_goofy2_8", 0, PLB_FEET, 96, false, false}, {"lim_goofy2_11", 2, PLB_CHEST, 98, false, false}, {"lim_goofy2_9", 2, PLB_CHEST, 100, false, false},
                      {"lim_goofy2_10", 2, PLB_FEET, 100, false, false}, {NULL}},  // anchors: bone 140 -> Keyblade tip, bones 136 / 131 / 9 -> chest, bone 2 -> feet (GUESS)
     .nsnd = 5, .snd = {{PSN_SORA_VOICE, 0, 6, false, 0}, {PSN_GOOFY_VOICE, 2, 6, false, 0}, {PSN_SE, 2, 0, false, 44, "ks_se999_111"}, {PSN_SE, 0, 0, false, 96, "ks_se999_112"},
                        {PSN_SE, 2, 0, false, 98, "ks_se999_113"}},
     .nhit = 1, .hit = {{2, 712, 200, 100, 159, 16, 250, 150, 1}}},  // baked weapon window 100..159 (data helper); volume GUESS volume r 250 / h 150 cm (as Whirli-Goof)
    // K3a Goofy's solo homing dive (1428, 120 f, Sora FREE): atkp 712 for the whole motion
    {.slot = {0, 0, 1428}, .len = 120, .gen = PLG_DIVE, .gen_at = 0, .sora_free = true,
     .nhit = 1, .hit = {{2, 712, 200, 0, 119, 16, 250, 150, 1}}},  // baked weapon window 0..119
    // K3b Goofy lands (1424, 60 f), then rejoins
    {.slot = {0, 0, 1424}, .len = 60, .sora_free = true},
};
// the press chain = the script's counter (goofy2.bar L2385 / L4985: array 340 at counter >= 3, 341 at >= 5; what raises it is UNRESOLVED in S139 ->
// GUESS: it counts our presses, as Whirli-Goof's verified counter does): K0[339] x3, K0[340], K1[340], K1[341], then K2 -> K3a -> K3b.
// The Limit time (cmd Time 150) drains outside the frozen motions only (range 45 = all of K0 / K1 / K2): GUESS -> no separate timer, the press window decides.
static const PlStep PLSTEP_KNOCK[9] = {{0, 0, 339}, {0, 0, 339}, {0, 0, 339}, {0, 0, 340}, {1, 0, 340}, {1, 0, 341}, {2, 1, 0}, {3, 1, 0}, {4, 0, 0}};

// ---- (s150) Duck Flare (donald2.bar): Donald slots 1508 ride-on 46 f / 1500 idle loop 120 f / 1504 Rocket 70 f / 1516 Megaduck 148 f / 1512 dismount 40 f.
// The flow is pl_df_* (below pl_play_step): the steps are just the stage indices, the prompt is dynamic (347 / 348).
static const PlStage PLS_DUCK[5] = {
    {.slot = {0, 1508, 0}, .len = 46},
    {.slot = {0, 1500, 0}, .len = 120, .loop = true},
    {.slot = {0, 1504, 0}, .len = 70},
    {.slot = {0, 1516, 0}, .len = 148},
    {.slot = {0, 1512, 0}, .len = 40},
};
static const PlStep PLSTEP_DUCK[5] = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}, {4, 0, 0}};

#define PL_NLIM_DEFS 4
static const PlDef PL_LIMS[PL_NLIM_DEFS] = {
    {"Comet", 282, 5, 0, PLS_COMET, 3, PLSTEP_COMET, 3, false, NULL},
    {"Whirli-Goof", 255, 3, 1, PLS_WHIRLI, 3, PLSTEP_WHIRLI, 7, true, NULL},
    {"Knocksmash", 338, 4, 1, PLS_KNOCK, 5, PLSTEP_KNOCK, 9, true, "lim_goofy2_0"},
    {"Duck Flare", 344, 6, 0, PLS_DUCK, 5, PLSTEP_DUCK, 5, false, NULL, true},  // (s150)
};
_Static_assert(PL_NLIM_DEFS <= 8, "SoraHudMenu sub_label[8]");
_Static_assert(PL_NLIM == PL_NLIM_DEFS, "sora.c PL_NLIM must match the PL_LIMS table (s149)");

// comet / blast objects (Donald's volley: atkp 664; the Rain: atkp 665)
typedef struct { bool on, blast, struck, fxed; float t_hit, pos[3]; uint64_t target; int sub, fx; } PlComet;
typedef struct { bool on; uint64_t target; float delay; int n; int rk; uint32_t rg; } PlThread;  // (s135) rk = rocket fx handle + 1 (0 = none), rg its g_fx_gen

#define PL_TRK 8
static int g_pl_fxfollow = 1;  // (s135 lpfxfollow) 1 = the stage effects follow Sora / the member, 0 = old (stay where spawned)
static int g_plimit = 1, g_pl_diag = 0, g_pl_auto = 0, g_pl_force = -1, g_pl_steer = 1, g_pl_rocket = 1;  // (s135 lprocket) 1 = the flying rocket before each volley comet
static bool g_plim_said_fx, g_plim_said_slot[8];
// (s149) Knocksmash live keys (each = one new behaviour, 0 = off): lpks (the Limit itself), lpksshield (the thrown shields + their hits), lpksdive (Goofy's solo dive;
// 0 = the Limit ends after Cosmo Boost), lpkssnd (the Limit's voices / SEs), lpksfx (the shields' effects)
static int g_df_on = 1;  // (s150) lpdf: Duck Flare usable (the other lpdf* keys sit with the Duck Flare code)
static int g_ks_on = 1, g_ks_shield = 1, g_ks_dive = 1, g_ks_snd = 1, g_ks_fx = 1;
#define PL_SHIELDS 6
typedef struct {
    bool on, tok, hit1;
    int kind, phase, atkp, power, fx, nhs, fxe;       // kind 0 = K0's shield, 1 / 2 = Duo Raid's volley A / B; phase 0 out, 1 back
    float pos[3], start[3], tpos[3], fdir[3], age, u, D, side, spd, r, every, linger, travel;
    uint64_t target;
    uint32_t fxg;
    struct { uint64_t a; float since; } hs[12];
} PlShield;
static struct {
    bool on, shown, prompt;
    int lim, step, req;
    float t, tp, tstep;                  // frames since the Limit began / this step began (tp: previous tick)
    int plays;                           // steps started (the first play spawns the one-shot effects)
    float clk[PL_ACT];                   // motion clocks of the members (Sora's is S.clock)
    int slot[PL_ACT];
    float mpos[2][3], mface[2];          // the members' feet (u) and facing
    bool held[2];
    float spawn[3];
    float pv, pdir[2];                   // Sora's script move
    float sv;                            // (s134 lpsteer) the steered spin's speed, cm per KH frame (goofy.bar L2137: 10 -> 8)
    int steered;                         // frames of this step the stick steered (log once)
    int fxh[PL_FXH], nfx;                // handles to free (fxg: g_fx_gen when spawned: a slot another effect took since is not ours)
    uint32_t fxg[PL_FXH], loop_g;
    int loop_fx[PL_ACT];
    struct { int h, actor, anchor; uint32_t g; } tr[PL_TRK];  // (s135) stage effects that follow their actor
    int ntr, ntr_next;
    struct { uint64_t actor[PL_MAXH]; float since[PL_MAXH]; int n; } hs[2];
    PlComet cm[PL_COMETS];
    PlThread th[5], rr[4];  // rr: (s135) the Comet Rain rockets (n = the blast they fly to)
    bool rain; int rain_k; float rain_t, rain_ang, rain_rad; int rain_side;
    bool gen_done;
    unsigned fxdone, sndone;                    // bit per PlFx of this step already started
    uint32_t rnd;
    int nhits;
    float mp0, spk_t;
    PlShield sh[PL_SHIELDS];  // (s149) Knocksmash's thrown shields
    struct { bool on, near_, notgt; uint64_t target; float dir[3], age, near_t; } dv;  // (s149) Goofy's solo dive
    SoraHudQuad q[96];
} PLM = {.req = -1, .rnd = 12345};

static int pl_rand(int n) { PLM.rnd = PLM.rnd * 214013u + 2531011u; return n > 0 ? (int)((PLM.rnd >> 16) & 0x7fff) % n : 0; }
static float pl_randf(float lo, float hi) { PLM.rnd = PLM.rnd * 214013u + 2531011u; return lo + (hi - lo) * (float)((PLM.rnd >> 16) & 0x7fff) / 32768.0f; }
static const PlDef *pl_def(void) { return &PL_LIMS[PLM.lim]; }
static const PlStep *pl_step(void) { return &pl_def()->step[PLM.step]; }
static const PlStage *pl_stage(void) { return &pl_def()->st[pl_step()->stage]; }

static bool pl_hold(void) { return PLM.on && pl_def()->sora_motion && !pl_stage()->sora_free; }  // Sora plays the Limit's motions and owns no input
// (s149 review) the old pl_hold() for the summon gates: Knocksmash's K3a / K3b free Sora, but Goofy is still the Limit's (no summon may take him)
static bool pl_hold_party(void) { return PLM.on && pl_def()->sora_motion; }
static bool pl_invuln(void) { return PLM.on && !pl_stage()->sora_free; }  // (s149 review should-fix: Sora free in the K3 dive = hittable)
//                        // sora_guard.h: the exe drops damage while a Limit runs
static bool pl_eat_x(void) { return PLM.on && PLM.prompt; }              // Triangle belongs to the prompt (BotW gets no X)

static void pl_fx_free(int h, uint32_t g) {
    if (fx_mine(h, g) && sora_fx_alive(h)) sora_fx_kill(h);
}
static void pl_fx_keep(int h) {
    if (h < 0 || h >= 32) return;
    for (int i = 0; i < PLM.nfx; i++) if (PLM.fxh[i] == h) { PLM.fxg[i] = g_fx_gen[h]; return; }
    if (PLM.nfx == PL_FXH) {
        pl_fx_free(PLM.fxh[0], PLM.fxg[0]);  // the oldest gives way
        for (int i = 1; i < PL_FXH; i++) PLM.fxh[i - 1] = PLM.fxh[i], PLM.fxg[i - 1] = PLM.fxg[i];
        PLM.nfx--;
    }
    PLM.fxh[PLM.nfx] = h, PLM.fxg[PLM.nfx++] = g_fx_gen[h];
}
// prio (s134): the comets / Rain blasts, the Limit's whole picture, use a higher cap than the sparks (sparks give way first)
static int pl_fx_p(const char *name, const float pos[3], float yaw, float to_u, bool keep, bool prio) {
    if (sora_fx_id(name) < 0) {
        if (!g_plim_said_fx) g_plim_said_fx = true, PLOG("[plimit] effect %s not in fx.bin (install_limitfx.sh not run?): skipped\n", name);
        return -1;
    }
    int live = sora_fx_count();
    if (live >= (prio ? 31 : 28)) {  // the 32 slots are shared with spells / orbs: a Limit never starves them
        if (g_pl_diag) PLOG("[plimit] fx %s DROPPED: %d live elements%s\n", name, live, prio ? " (even the priority cap)" : "");
        return -1;
    }
    int h = sora_fx_event(name, pos, yaw, to_u);
    if (g_pl_diag && prio) PLOG("[plimit] fx %s -> slot %d at %.2f %.2f %.2f (%d live)\n", name, h, pos[0], pos[1], pos[2], live + 1);
    if (keep) pl_fx_keep(h);
    return h;
}
static int pl_fx(const char *name, const float pos[3], float yaw, float to_u, bool keep) { return pl_fx_p(name, pos, yaw, to_u, keep, false); }

// ---- positions
static void pl_anchor(int actor, int anchor, const float p[3], float to_u, float out[3]) {
    const float *f = actor == 0 ? p : PLM.mpos[actor - 1];
    out[0] = f[0], out[1] = f[1], out[2] = f[2];
    if (anchor == PLB_CHEST) out[1] += 100 * to_u;
    else if (anchor == PLB_TIP && actor == 0) cast_tip(p, to_u, out);
}
static void pl_proot(const PMotX *mo, float t, float r[3]) {  // a member's baked root (frames + 1) x {x y z yaw}, linear
    r[0] = r[1] = r[2] = 0;
    if (!mo || !mo->root || !mo->frames) return;
    if (t < 0) t = 0;
    if (t > mo->frames) t = (float)mo->frames;
    int i = (int)t;
    if (i >= mo->frames) i = mo->frames - 1;
    float u = t - (float)i;
    for (int c = 0; c < 3; c++) r[c] = mo->root[i * 4 + c] + (mo->root[(i + 1) * 4 + c] - mo->root[i * 4 + c]) * u;
}

// Goofy drawn in Sora's motion frame (trap_obj_attach(Goofy, Sora, 0x4002, 1), re:0x3CCB90 / 0x3B50F0 case 0x4002; PHH limits_kh2.frame_attached):
// Sora's motion origin = his feet minus his root T(t), Goofy's root T(t) (+ his T(0) in that frame) on top.
static void pl_goofy_pos(const PlStage *st, const float p[3], float to_u) {
    float ts[3], tg[3];
    mot_root(S.mot, S.clock, ts);
    pl_proot(pmot_find(1, st->slot[2]), PLM.clk[2], tg);
    float rx = st->att[0] + tg[0] - ts[0], ry = st->att[1] + tg[1], rz = st->att[2] + tg[2] - ts[2];
    float fx = sinf(S.face), fz = cosf(S.face);
    PLM.mpos[1][0] = p[0] + (fz * rx + fx * rz) * to_u;   // left = (fz, -fx), forward = (fx, fz)
    PLM.mpos[1][2] = p[2] + (-fx * rx + fz * rz) * to_u;
    PLM.mpos[1][1] = p[1] + ry * to_u;
    PLM.mface[1] = S.face;
}

// ---- hits
static bool pl_cyl(const float a[3], const float e[3], float r_cm, float h_cm, float to_u) {  // GUESS volumes
    float dx = e[0] - a[0], dz = e[2] - a[2], r = r_cm * to_u;
    return dx * dx + dz * dz <= r * r && fabsf(e[1] - a[1]) <= h_cm * to_u + 0.5f;
}
static void pl_hit_one(int actor_idx, int atkp, int power, int ehit, uint64_t actor, int32_t life, const float epos[3], float to_u, bool magic, const char *what) {
    float at[3] = {epos[0], epos[1] + 0.9f, epos[2]};
    int f54 = power <= 25 ? F54_SMALL : (g_fin_big || ah_small(actor)) ? g_fin_f54 : 0x11;  // GUESS: BotW has no pull / hold reaction (EnemyReaction 9 / 11 / 17 / 18)
    int dmg;
    if (actor_idx == 0) {
        dmg = sl_hit(actor, life, power, g_dmg_base, magic ? 1 : 0);
        linkpos_damage_enemy(actor, dmg, f54);
        sora_sfx_hit(atkp, 1.0f, epos);
        dg_hit(atkp, actor);
    } else {
        int mi = actor_idx - 1;
        dmg = party_dmg_cb(mi, actor, life, power, magic ? 1 : 0);
        party_hit_cb(mi, actor, dmg, f54, 0, at, NULL, atkp);
    }
    const char *sp = pl_def()->spark ? pl_def()->spark : ehit == 2 ? "lim_goofy_1" : "lim_goofy_0";  // (s149: Knocksmash's EffectOnHit 1 = lim_goofy2_0)
    if (!pl_def()->custom) pl_fx(sora_fx_id(sp) >= 0 && !magic ? sp : ehit == 2 ? "hit_finish" : "hit", at, S.face, to_u, false);  // (s150: Duck Flare has no generic spark)
    if (pl_def()->spark && g_ks_snd && PLM.t - PLM.spk_t >= 8.0f) {  // element 0 carries SE se999#108 (S139): at most every 8 frames
        int id = sora_sfx_id("ks_se999_108");
        PLM.spk_t = PLM.t;
        if (id >= 0) sora_sfx_play_at(id, 1.0f, at);
    }
    PLM.nhits++;
    if (g_pl_diag)
        PLOG("[plimit] HIT %s %llx (life %d) atkp %d power %d: damage %d f54 %#x at %.2f %.2f %.2f\n", what, (unsigned long long)actor, life, atkp, power, dmg,
             f54, at[0], at[1], at[2]);
}

static void pl_hits(const float p[3], float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne, float kh) {
    const PlStage *st = pl_stage();
    for (int j = 0; j < st->nhit && j < 2; j++) {
        const PlHit *h = &st->hit[j];
        for (int k = 0; k < PLM.hs[j].n; k++) PLM.hs[j].since[k] += kh;
        if (PLM.tstep < h->a || PLM.tstep > h->b) continue;
        const float *a = h->actor == 0 ? p : PLM.mpos[h->actor - 1];
        for (int i = 0; i < ne; i++) {
            if (el[i] <= 0 || !pl_cyl(a, ep[i], h->r, h->h, to_u)) continue;
            int slot = -1;
            for (int k = 0; k < PLM.hs[j].n; k++) if (PLM.hs[j].actor[k] == ea[i]) slot = k;
            if (slot >= 0 && PLM.hs[j].since[slot] < h->every) continue;  // the atkp row's Interval (frames) between hits on one enemy
            if (slot < 0 && PLM.hs[j].n < PL_MAXH) slot = PLM.hs[j].n++, PLM.hs[j].actor[slot] = ea[i];
            if (slot < 0) continue;  // (s133 review: list full = no Interval timer: skip, else it hits every frame)
            PLM.hs[j].since[slot] = 0;
            pl_hit_one(h->actor, h->atkp, h->power, h->ehit, ea[i], el[i], ep[i], to_u, false, "window");
        }
    }
}

// ---- Comet: the volley and the Rain (donald.bar L443 / L564 / L951 / L1160 / L1426 and L3040 / L3287 / L3473)
static PlComet *pl_comet_new(void) {
    for (int i = 0; i < PL_COMETS; i++) if (!PLM.cm[i].on) { memset(&PLM.cm[i], 0, sizeof PLM.cm[i]); PLM.cm[i].fx = -1; return &PLM.cm[i]; }
    return NULL;
}
static void pl_volley(const float p[3], float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne) {
    int order[PL_MAXH], n = 0;
    float dist[PL_MAXH];
    for (int i = 0; i < ne && n < PL_MAXH; i++) {  // GUESS: radius 1000 cm (the script's searcher is 0..300 around Sora: BotW enemies keep farther)
        float dx = ep[i][0] - p[0], dz = ep[i][2] - p[2], d = sqrtf(dx * dx + dz * dz) / to_u;
        if (el[i] > 0 && d <= 1000) order[n] = i, dist[n] = d, n++;
    }
    for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) if (dist[b] < dist[a]) { float t = dist[a]; dist[a] = dist[b]; dist[b] = t; int o = order[a]; order[a] = order[b]; order[b] = o; }
    int m = n < 5 ? n : 5;
    for (int k = 0; k < 5; k++) PLM.th[k].on = false;
    for (int k = 0; k < m; k++) PLM.th[k] = (PlThread){true, ea[order[k]], 16.0f + 2.0f * (float)k, 0, 0, 0};  // the threads' delays 16, 18, 20 ...
    PLOG("[plimit] Comet volley: %d target(s) (each 3 comets, atkp 664 Power 25 / 50 / 50)%s\n", m, m ? "" : ", none in range: 3 scattered comets (look only)");
    if (!m) {  // L683: no target = 3 comets scattered in front of Sora (visual only)
        for (int k = 0; k < 3; k++) {
            PlComet *c = pl_comet_new();
            if (!c) break;
            float ang = S.face + pl_randf(-0.8f, 0.8f), r = (400 + pl_randf(-100, 100)) * to_u;
            c->on = true, c->t_hit = 6 + 20.0f * (float)k, c->struck = true, c->sub = k;
            c->pos[0] = p[0] + sinf(ang) * r, c->pos[1] = p[1], c->pos[2] = p[2] + cosf(ang) * r;
            c->fxed = false;  // the effect starts 6 frames before the (absent) strike
        }
    }
}
static void pl_rain_start(void) {
    PLM.rain = true, PLM.rain_k = 0, PLM.rain_t = 0, PLM.rain_side = pl_rand(2) ? 1 : -1;
    PLM.rain_ang = S.face - (float)PLM.rain_side * 2.094395f, PLM.rain_rad = 250.0f;  // L3048: roty(.., 2.0944 x -side), 250 out
    PLOG("[plimit] Comet Rain: 31 blasts, atkp 665 Power 100, one every 6 frames, from %.0f cm out, side %d (GUESS: the ring)\n", PLM.rain_rad, PLM.rain_side);
}

// ---- (s135) the Comet rocket. ROOT CAUSE of "only the explosions": fx.bin's lim_donald_2..5 (= KH2's PAX elements 2..5, PHH recording) hold ONLY the
// burst (every particle starts at the effect origin and spreads, max y -274 cm; the one moving thing is the radial sparks), and donald.bar spawns nothing else
// (its syscalls: one trap_limit_effect_start per comet AT the target, no object / projectile / effect_set_pos per frame). So no flying rocket exists in
// the data the mod has; this synthesizes one (GUESS): lim_donald_1 (Donald's own staff glow) flown from his staff to the flare point 143 cm above the
// target (the recorded firework flares sit at y -143 cm: VERIFIED in lim_donald.4) over the last PL_RFLIGHT frames before the burst. lprocket 0 = off.
#define PL_RFLIGHT 16.0f
static bool g_pl_rsaid;
static void pl_rocket_end(PlThread *t) {
    if (t->rk) { pl_fx_free(t->rk - 1, t->rg); t->rk = 0; }
}
static void pl_rocket(PlThread *t, const float p[3], const float tgt[3], float to_u) {
    if (t->delay > PL_RFLIGHT || t->delay <= 0) return;
    float u = 1.0f - t->delay / PL_RFLIGHT;           // 0 at launch, 1 at the burst
    u = u * u * (3.0f - 2.0f * u) * 0.5f + u * 0.5f;  // soft ease (GUESS)
    float a[3] = {PLM.mpos[0][0], PLM.mpos[0][1] + 60 * to_u, PLM.mpos[0][2]};   // Donald's staff (GUESS: 60 cm above his feet)
    float b[3] = {tgt[0], tgt[1] + 143 * to_u, tgt[2]};
    float q[3] = {a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u + sinf(u * 3.14159f) * 80 * to_u, a[2] + (b[2] - a[2]) * u};  // GUESS: an 80 cm arc
    if (!t->rk) {
        int h = pl_fx_p("lim_donald_1", q, 0, to_u, false, true);
        if (h < 0) return;
        t->rk = h + 1, t->rg = g_fx_gen[h];
        if (!g_pl_rsaid) {
            g_pl_rsaid = true;
            PLOG("[plimit] Comet rocket: lim_donald_1 flown staff -> 143 cm above each target over %.0f frames before its burst (slot %d; GUESS, the KH2 data has no rocket; lprocket 0 = off)\n", PL_RFLIGHT, h);
        }
        return;
    }
    if (fx_mine(t->rk - 1, t->rg) && sora_fx_alive(t->rk - 1)) sora_fx_move(t->rk - 1, q, 0);
}
static void pl_comets(const float p[3], float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne, float kh) {
    for (int k = 0; k < 5; k++) {  // the volley's threads
        PlThread *t = &PLM.th[k];
        if (!t->on) continue;
        int ti = -1;
        for (int i = 0; i < ne; i++) if (ea[i] == t->target && el[i] > 0) ti = i;
        if (ti < 0) { pl_rocket_end(t); t->on = false; continue; }   // the target died / left
        t->delay -= kh;
        if (g_pl_rocket) pl_rocket(t, p, ep[ti], to_u);
        if (t->delay > 0) continue;
        pl_rocket_end(t);
        PlComet *c = pl_comet_new();
        if (!c) continue;
        c->on = true, c->t_hit = 6, c->target = t->target, c->sub = t->n;
        memcpy(c->pos, ep[ti], sizeof c->pos);
        static const char *const CN[3] = {"lim_donald_2", "lim_donald_3", "lim_donald_4"};  // 2 + random_get(3)
        c->fx = pl_fx_p(CN[pl_rand(3)], c->pos, 0, to_u, false, true), c->fxed = true;  // (s134: not kept: they end by themselves; 46 kept handles pushed the aura out)
        t->n++, t->delay = 20;
        if (t->n >= 3) t->on = false;
    }
    if (PLM.rain && PLM.rain_k < 31) {
        PLM.rain_t -= kh;
        // (s135 user: the Rain had no rockets) blast b pops in rain_t + 6 (b - rain_k) frames at the ring point (b - rain_k) steps on: its rocket
        // flies the last PL_RFLIGHT frames before (same GUESS rocket as the volley; the first blasts' rockets start mid-flight; blast timing unchanged)
        for (int j = 0; g_pl_rocket && j < 4; j++) {
            int b = PLM.rain_k + j;
            float rem = PLM.rain_t + 6.0f * (float)j;
            if (b >= 31 || rem <= 0 || rem > PL_RFLIGHT) continue;
            PlThread *t = NULL, *fr = NULL;
            for (int i = 0; i < 4; i++) {
                if (PLM.rr[i].on && PLM.rr[i].n == b) t = &PLM.rr[i];
                else if (!PLM.rr[i].on && !fr) fr = &PLM.rr[i];
            }
            if (!t && fr) t = fr, *t = (PlThread){true, 0, 0, b, 0, 0};
            if (!t) continue;
            t->delay = rem;
            float a = PLM.rain_ang + (float)PLM.rain_side * 0.628319f * (float)j, r = (PLM.rain_rad + 10.0f * (float)j) * to_u;
            float tg[3] = {p[0] + sinf(a) * r, p[1], p[2] + cosf(a) * r};
            pl_rocket(t, p, tg, to_u);
        }
        while (PLM.rain_t <= 0 && PLM.rain_k < 31) {
            for (int i = 0; i < 4; i++) if (PLM.rr[i].on && PLM.rr[i].n == PLM.rain_k) pl_rocket_end(&PLM.rr[i]), PLM.rr[i].on = false;
            PlComet *c = pl_comet_new();
            if (!c) break;
            c->on = true, c->blast = true, c->t_hit = 6, c->sub = 0;
            c->pos[0] = p[0] + sinf(PLM.rain_ang) * PLM.rain_rad * to_u, c->pos[1] = p[1], c->pos[2] = p[2] + cosf(PLM.rain_ang) * PLM.rain_rad * to_u;
            static const char *const CR[4] = {"lim_donald_2", "lim_donald_3", "lim_donald_4", "lim_donald_5"};  // 2 + random_get(4)
            c->fx = pl_fx_p(CR[pl_rand(4)], c->pos, 0, to_u, false, true), c->fxed = true;
            PLM.rain_ang += (float)PLM.rain_side * 0.628319f, PLM.rain_rad += 10, PLM.rain_k++, PLM.rain_t += 6;
        }
        if (PLM.rain_k >= 31) PLM.rain = false;
    }
    for (int i = 0; i < PL_COMETS; i++) {
        PlComet *c = &PLM.cm[i];
        if (!c->on) continue;
        if (!c->fxed && c->t_hit <= 6) c->fxed = true, c->fx = pl_fx_p("lim_donald_2", c->pos, 0, to_u, false, true);
        if ((c->t_hit -= kh) > 0) continue;
        if (c->blast) {  // L3473: atkp 665, radius 300, height 200 (attack_set_radius 300 / 200), Power 100
            for (int e = 0; e < ne; e++)
                if (el[e] > 0 && pl_cyl(c->pos, ep[e], 300, 200, to_u) && ep[e][1] >= c->pos[1] - 0.5f - 50 * to_u)
                    pl_hit_one(1, 665, 100, 1, ea[e], el[e], ep[e], to_u, true, "Comet Rain blast");
        } else if (!c->struck) {  // L1426: atkp 664 on the target, SubId = the comet's number in its thread
            for (int e = 0; e < ne; e++)
                if (ea[e] == c->target && el[e] > 0) pl_hit_one(1, 664, c->sub == 0 ? 25 : 50, 1, ea[e], el[e], ep[e], to_u, true, "Comet");
        }
        c->on = false;
    }
}
static bool pl_comets_busy(void) {
    if (PLM.rain) return true;
    for (int k = 0; k < 5; k++) if (PLM.th[k].on) return true;
    for (int i = 0; i < PL_COMETS; i++) if (PLM.cm[i].on) return true;
    return false;
}

// ---- (s149) Knocksmash: Goofy's thrown shields, his solo dive, the stage sounds (goofy2.bar; S139 section 3; S134 section 3 for the NOT RE-READ numbers)
#define KS_SEARCH_R 1000.0f    // GUESS: K0's lock-on-else-nearest radius, cm (as the Comet volley)
#define KS_DUO_SEARCH_R 2000.0f // L2459 searcher range 0..2000 (S139), angle 90 deg: taken as the front half plane (GUESS)
#define KS_SH_SPD 18.0f        // GUESS: K0's shield out speed, cm per KH frame (S139 has the radius 60 only)
#define KS_SH_RET 14.0f        // GUESS: the return speed, cm per KH frame
#define KS_SH_R 60.0f          // atkp 711's radius (VERIFIED S139, attack_set_radius at L1021)
#define KS_NOTGT_DIST 400.0f   // GUESS: no target = the shield flies this far ahead, then returns
#define KS_DUO_R_A 60.0f       // Duo Raid volley radii 60 / 80 (S134, NOT RE-READ)
#define KS_DUO_R_B 80.0f
#define KS_DUO_SPD_A 6.0f      // cm per frame (S134 L2981 / L3995, NOT RE-READ)
#define KS_DUO_SPD_B 10.0f
#define KS_DUO_ARC 0.5236f     // +-30 deg arcs (S134, NOT RE-READ)
#define KS_DUO_LINGER 8.0f     // GUESS: frames a volley stays on the target before it returns
#define KS_DIVE_V0 90.0f       // VERIFIED constants (S139 #21): speed 90 -> 70 by -0.1 per frame
#define KS_DIVE_V1 70.0f
#define KS_DIVE_T0 0.2618f     // homing turn per frame, +0.0349 per frame after the 2nd, up to 0.7854
#define KS_DIVE_TD 0.0349f
#define KS_DIVE_TM 0.7854f
#define KS_DIVE_NEAR 200.0f    // aimed 50 cm below the target until within 200 cm, then 2.857 frames on
#define KS_DIVE_TAIL 2.857f
#define KS_DIVE_NOTGT 30.0f    // GUESS: no target = fly this many frames on the 80-degree heading, then land
static bool g_ks_said_pull, g_ks_said_snd[16];

static PlShield *pl_sh_new(void) {
    for (int i = 0; i < PL_SHIELDS; i++) if (!PLM.sh[i].on) { memset(&PLM.sh[i], 0, sizeof PLM.sh[i]); PLM.sh[i].fx = -1; return &PLM.sh[i]; }
    return NULL;
}
static void pl_sh_off(PlShield *s) {
    if (s->fx >= 0) pl_fx_free(s->fx, s->fxg);
    s->fx = -1, s->on = false;
}
static bool pl_sh_busy(void) {
    for (int i = 0; i < PL_SHIELDS; i++) if (PLM.sh[i].on) return true;
    return false;
}
static void pl_goofy_chest(float to_u, float out[3]) { out[0] = PLM.mpos[1][0], out[1] = PLM.mpos[1][1] + 100 * to_u, out[2] = PLM.mpos[1][2]; }
// the target of a throw: the lock-on, else the nearest enemy within r cm (front half plane when cone)
static int pl_ks_target(const float p[3], float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne, float r_cm, bool cone) {
    int best = -1;
    float bd = 0;
    for (int i = 0; i < ne; i++) {
        if (el[i] <= 0) continue;
        if (g_lock.actor && ea[i] == g_lock.actor) return i;
        float dx = ep[i][0] - p[0], dz = ep[i][2] - p[2], d = sqrtf(dx * dx + dz * dz) / to_u;
        if (d > r_cm) continue;
        if (cone && dx * sinf(S.face) + dz * cosf(S.face) < 0) continue;
        if (best < 0 || d < bd) best = i, bd = d;
    }
    return best;
}
static void pl_sh_throw(int kind, const float p[3], float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne) {
    if (!g_ks_shield) { PLOG("[plimit] Knocksmash: shield throw skipped (lpksshield 0)\n"); return; }
    PlShield *s = pl_sh_new();
    if (!s) { PLOG("[plimit] Knocksmash: shield pool full, throw skipped\n"); return; }
    int ti = pl_ks_target(p, to_u, ea, ep, el, ne, kind ? KS_DUO_SEARCH_R : KS_SEARCH_R, kind != 0);
    s->on = true, s->kind = kind, s->phase = 0;
    pl_goofy_chest(to_u, s->start);
    memcpy(s->pos, s->start, sizeof s->pos);
    if (kind == 0) s->atkp = 711, s->power = 100, s->r = KS_SH_R, s->every = 0, s->spd = KS_SH_SPD;
    else if (kind == 1) s->atkp = 769, s->power = 25, s->r = KS_DUO_R_A, s->every = 10, s->spd = KS_DUO_SPD_A, s->side = 1;   // atkp 769: Power 25, Interval 10 (VERIFIED)
    else s->atkp = 768, s->power = 25, s->r = KS_DUO_R_B, s->every = 10, s->spd = KS_DUO_SPD_B, s->side = -1;                // atkp 768
    if (ti >= 0) {
        s->tok = true, s->target = ea[ti];
        memcpy(s->tpos, ep[ti], sizeof s->tpos), s->tpos[1] += 0.9f;
        float dx = s->tpos[0] - s->pos[0], dy = s->tpos[1] - s->pos[1], dz = s->tpos[2] - s->pos[2];
        s->D = sqrtf(dx * dx + dy * dy + dz * dz) / to_u;
        if (s->D < 30) s->D = 30;
    } else {  // no target: straight ahead (GUESS)
        s->fdir[0] = sinf(S.face), s->fdir[2] = cosf(S.face);
        s->D = KS_NOTGT_DIST;
    }
    if (g_ks_fx) {
        static const char *const FN[3] = {"lim_goofy2_2", "lim_goofy2_3", "lim_goofy2_4"};  // e2 (K0), e3 / e4 (Duo Raid A / B): 600-frame loops riding the shield (mapping beyond the literals GUESS)
        s->fx = pl_fx_p(FN[kind], s->pos, S.face, to_u, false, true);
        if (s->fx >= 0) s->fxg = g_fx_gen[s->fx];
    }
    if (kind && g_ks_snd) {  // e3 / e4 carry se999#109 / #110
        int id = sora_sfx_id(kind == 1 ? "ks_se999_109" : "ks_se999_110");
        if (id >= 0) sora_sfx_play_at(id, 1.0f, s->pos);
    }
    if (kind == 1 && !g_ks_said_pull)
        g_ks_said_pull = true, PLOG("[plimit] Knocksmash: Duo Raid's pull ring (atkp 760, Power 0, r 150) is skipped: BotW has no pull / hold (GUESS)\n");
    PLOG("[plimit] Knocksmash: shield %d thrown (atkp %d, %s, D %.0f cm)\n", kind, s->atkp, s->tok ? "target" : "no target: straight", s->D);
}
static void pl_sh_hit(PlShield *s, float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne) {
    for (int i = 0; i < ne; i++) {
        if (el[i] <= 0 || !pl_cyl(s->pos, ep[i], s->r, 150, to_u)) continue;
        int slot = -1;
        for (int k = 0; k < s->nhs; k++) if (s->hs[k].a == ea[i]) slot = k;
        if (slot >= 0 && (s->every <= 0 || s->hs[slot].since < s->every)) continue;  // Interval 0 = once per throw
        if (slot < 0 && s->nhs < 12) slot = s->nhs++, s->hs[slot].a = ea[i];
        if (slot < 0) continue;  // list full = skip (the s133 review rule)
        s->hs[slot].since = 0;
        pl_hit_one(2, s->atkp, s->power, 1, ea[i], el[i], ep[i], to_u, false, "Knocksmash shield");
        if (s->kind == 0) s->hit1 = true;
    }
}
static void pl_sh_tick(const float p[3], float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne, float kh) {
    (void)p;
    for (int n = 0; n < PL_SHIELDS; n++) {
        PlShield *s = &PLM.sh[n];
        if (!s->on) continue;
        s->age += kh;
        for (int k = 0; k < s->nhs; k++) s->hs[k].since += kh;
        int ti = -1;
        for (int i = 0; i < ne; i++) if (s->tok && ea[i] == s->target && el[i] > 0) ti = i;
        if (s->tok && ti >= 0) memcpy(s->tpos, ep[ti], sizeof s->tpos), s->tpos[1] += 0.9f;
        float goal[3];
        pl_goofy_chest(to_u, goal);
        if (s->phase == 0) {
            if (s->tok && ti < 0) s->phase = 1;  // the target died / left: back
            else if (s->kind == 0) {
                if (s->tok) {  // homing: straight at the target's centre
                    float dx = s->tpos[0] - s->pos[0], dy = s->tpos[1] - s->pos[1], dz = s->tpos[2] - s->pos[2], d = sqrtf(dx * dx + dy * dy + dz * dz), st = s->spd * kh * to_u;
                    if (d > st) s->pos[0] += dx / d * st, s->pos[1] += dy / d * st, s->pos[2] += dz / d * st;
                    else memcpy(s->pos, s->tpos, sizeof s->pos);
                } else {
                    float st = s->spd * kh;
                    s->pos[0] += s->fdir[0] * st * to_u, s->pos[2] += s->fdir[2] * st * to_u, s->travel += st;
                    if (s->travel >= s->D) s->phase = 1;
                }
                pl_sh_hit(s, to_u, ea, ep, el, ne);
                if (s->hit1) s->phase = 1;  // atkp 711 hits once, then the shield flies back
            } else {  // Duo Raid: a bowed path whose first heading is +-30 deg off the target line (S134 arcs)
                s->u += s->spd * kh / s->D;
                float uu = s->u > 1 ? 1 : s->u;
                if (s->tok) {
                    float dx = s->tpos[0] - s->start[0], dz = s->tpos[2] - s->start[2], hl = sqrtf(dx * dx + dz * dz);
                    float px = hl > 1e-4f ? dz / hl : 0, pz = hl > 1e-4f ? -dx / hl : 0;  // left of the line
                    float bow = s->side * sinf(3.14159265f * uu) * s->D * tanf(KS_DUO_ARC) / 3.14159265f * to_u;
                    for (int c = 0; c < 3; c++) s->pos[c] = s->start[c] + (s->tpos[c] - s->start[c]) * uu;
                    s->pos[0] += px * bow, s->pos[2] += pz * bow;
                    if (s->u >= 1 && (s->linger += kh) >= KS_DUO_LINGER) s->phase = 1;
                } else {
                    s->pos[0] = s->start[0] + s->fdir[0] * uu * s->D * to_u, s->pos[2] = s->start[2] + s->fdir[2] * uu * s->D * to_u;
                    if (s->u >= 1) s->phase = 1;
                }
                pl_sh_hit(s, to_u, ea, ep, el, ne);
            }
            if (s->age > 150) s->phase = 1;
        }
        if (s->phase == 1) {  // back to Goofy; Duo Raid's return pass keeps its volley rows (S139: 769 / 768 + the pull ring, skipped)
            float dx = goal[0] - s->pos[0], dy = goal[1] - s->pos[1], dz = goal[2] - s->pos[2], d = sqrtf(dx * dx + dy * dy + dz * dz), st = KS_SH_RET * kh * to_u;
            if (d > st) s->pos[0] += dx / d * st, s->pos[1] += dy / d * st, s->pos[2] += dz / d * st;
            if (s->kind) pl_sh_hit(s, to_u, ea, ep, el, ne);
            if (d <= st || d < 40 * to_u || s->age > 320) {  // caught: e13 (K0) / e15 (Duo Raid) on Goofy
                if (g_ks_fx) pl_fx_p(s->kind ? "lim_goofy2_15" : "lim_goofy2_13", goal, PLM.mface[1], to_u, false, false);
                PLOG("[plimit] Knocksmash: shield %d caught after %.0f frames\n", s->kind, s->age);
                pl_sh_off(s);
                continue;
            }
        }
        if (s->fx >= 0) {
            if (fx_mine(s->fx, s->fxg) && sora_fx_alive(s->fx)) sora_fx_move(s->fx, s->pos, S.face);
            else s->fx = -1;
        }
    }
}

// K3a: Goofy's solo homing dive (script L5148-5929, constants VERIFIED S139 #21): he leaves Sora's frame and flies at the target
static void pl_dive_start(const float p[3], float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne) {
    memset(&PLM.dv, 0, sizeof PLM.dv);
    PLM.dv.on = true;
    int ti = pl_ks_target(p, to_u, ea, ep, el, ne, 2500.0f, false);  // GUESS: radius 2500 cm
    if (ti >= 0) {
        PLM.dv.target = ea[ti];
        PLM.dv.dir[0] = sinf(S.face), PLM.dv.dir[2] = cosf(S.face);  // GUESS: starts along Sora's facing, then homes
    } else {  // no target: roty(Sora -> Goofy, 80 deg)
        float dx = PLM.mpos[1][0] - p[0], dz = PLM.mpos[1][2] - p[2], l = sqrtf(dx * dx + dz * dz), a = 80.0f * 3.14159265f / 180.0f;
        if (l < 1e-4f) dx = sinf(S.face), dz = cosf(S.face), l = 1;
        dx /= l, dz /= l;
        PLM.dv.dir[0] = dx * cosf(a) + dz * sinf(a), PLM.dv.dir[2] = -dx * sinf(a) + dz * cosf(a);
        PLM.dv.notgt = true;
    }
    PLOG("[plimit] Knocksmash: Goofy's dive starts (%s; speed %.0f -> %.0f, turn %.4f +%.4f/frame up to %.4f)\n", ti >= 0 ? "homing" : "no target: 80 deg heading", KS_DIVE_V0, KS_DIVE_V1, KS_DIVE_T0, KS_DIVE_TD, KS_DIVE_TM);
}
static void pl_dive_tick(float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne, float kh) {
    if (!PLM.dv.on) return;
    PLM.dv.age += kh;
    float v = KS_DIVE_V0 - 0.1f * PLM.dv.age;
    if (v < KS_DIVE_V1) v = KS_DIVE_V1;
    float turn = KS_DIVE_T0 + (PLM.dv.age > 2 ? KS_DIVE_TD * (PLM.dv.age - 2) : 0);
    if (turn > KS_DIVE_TM) turn = KS_DIVE_TM;
    turn *= kh;
    int ti = -1;
    for (int i = 0; i < ne; i++) if (PLM.dv.target && ea[i] == PLM.dv.target && el[i] > 0) ti = i;
    if (!PLM.dv.notgt && ti < 0) {  // the target died / left: land here
        PLM.dv.on = false, PLM.tstep = pl_stage()->len;
        return;
    }
    if (ti >= 0) {
        float a[3] = {ep[ti][0], ep[ti][1] + (PLM.dv.near_ ? 0.9f : 0.4f), ep[ti][2]};  // 50 cm below the centre until within 200 cm (GUESS: centre = feet + 90 cm)
        float d[3] = {a[0] - PLM.mpos[1][0], a[1] - PLM.mpos[1][1], a[2] - PLM.mpos[1][2]};
        float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (l > 1e-5f) {
            for (int c = 0; c < 3; c++) d[c] /= l;
            float cs = PLM.dv.dir[0] * d[0] + PLM.dv.dir[1] * d[1] + PLM.dv.dir[2] * d[2];
            cs = cs > 1 ? 1 : cs < -1 ? -1 : cs;
            float ang = acosf(cs), t = ang <= turn ? 1.0f : turn / ang;
            float n[3] = {PLM.dv.dir[0] + (d[0] - PLM.dv.dir[0]) * t, PLM.dv.dir[1] + (d[1] - PLM.dv.dir[1]) * t, PLM.dv.dir[2] + (d[2] - PLM.dv.dir[2]) * t};
            float nl = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (nl > 1e-5f) for (int c = 0; c < 3; c++) PLM.dv.dir[c] = n[c] / nl;
        }
        if (!PLM.dv.near_ && l <= KS_DIVE_NEAR * to_u) PLM.dv.near_ = true, PLOG("[plimit] Knocksmash: dive within %.0f cm of the target\n", KS_DIVE_NEAR);
    }
    if (PLM.dv.near_ && (PLM.dv.near_t += kh) > KS_DIVE_TAIL) {  // GUESS: the landing follows
        PLM.dv.on = false, PLM.tstep = pl_stage()->len;
        PLOG("[plimit] Knocksmash: dive ends on the target (-> landing)\n");
        return;
    }
    if (PLM.dv.notgt && PLM.dv.age > KS_DIVE_NOTGT) {
        PLM.dv.on = false, PLM.tstep = pl_stage()->len;
        PLOG("[plimit] Knocksmash: dive ends (no target, GUESS)\n");
        return;
    }
    float st = v * kh * to_u;
    for (int c = 0; c < 3; c++) PLM.mpos[1][c] += PLM.dv.dir[c] * st;
    PLM.mface[1] = atan2f(PLM.dv.dir[0], PLM.dv.dir[2]);
}

// the stage's voices / SEs (the motions' triggers + the PAX elements' SEs)
static void pl_goofy_voice(int vtbl, const float at[3]) {
    if (psfx_on()) psfx_voice(1, vtbl, 0, at);  // vtbl 4 = goofy_attack.N, vtbl 6 = goofy_spell.0 (PSFX_SETS row added s149)
}
static void pl_snd_tick(const float p[3]) {
    const PlStage *st = pl_stage();
    for (int i = 0; i < st->nsnd && i < 8; i++) {
        const PlSnd *s = &st->snd[i];
        if ((PLM.sndone >> i & 1) || PLM.tstep < s->at) continue;
        PLM.sndone |= 1u << i;
        if ((s->first_only && PLM.plays > 1) || !g_ks_snd) continue;
        const float *at = s->actor ? PLM.mpos[s->actor - 1] : p;
        if (s->kind == PSN_SORA_VOICE) {
            static bool said;
            if (sora_sfx_voice(s->vtbl) < 0 && !said) said = true, PLOG("[plimit] Sora voice vtbl %d gave no sound (row missing in sfx.bin, or a line is still playing)\n", s->vtbl);
        }
        else if (s->kind == PSN_GOOFY_VOICE) pl_goofy_voice(s->vtbl, at);
        else if (s->name) {
            int id = sora_sfx_id(s->name);
            if (id < 0) {
                if (!g_ks_said_snd[i]) g_ks_said_snd[i] = true, PLOG("[plimit] sound %s not in sfx.bin (data not installed yet?): skipped\n", s->name);
            } else sora_sfx_play_at(id, 1.0f, at);
        }
        if (g_pl_diag) PLOG("[plimit] snd %d (kind %d vtbl %d %s) at stage frame %.0f\n", i, s->kind, s->vtbl, s->name ? s->name : "-", PLM.tstep);
    }
}

// ---- start / end
static void pl_df_cleanup(void);
static void pl_cleanup(void) {
    for (int i = 0; i < PLM.nfx; i++) pl_fx_free(PLM.fxh[i], PLM.fxg[i]);
    PLM.nfx = 0;
    for (int i = 0; i < PL_COMETS; i++) PLM.cm[i].on = false;
    for (int k = 0; k < 5; k++) pl_rocket_end(&PLM.th[k]), PLM.th[k].on = false;
    for (int k = 0; k < 4; k++) pl_rocket_end(&PLM.rr[k]), PLM.rr[k].on = false;
    PLM.rain = false;
    for (int i = 0; i < PL_SHIELDS; i++) if (PLM.sh[i].on) pl_sh_off(&PLM.sh[i]);  // (s149)
    PLM.dv.on = false;
    pl_df_cleanup();  // (s150)
}
static void pl_end(const char *why) {
    if (!PLM.on) return;
    const PlDef *d = pl_def();
    PLOG("[plimit] %s ended at step %d/%d (%s): %d hits, %.0f frames; members back beside Sora\n", d->name, PLM.step + 1, d->nstep, why, PLM.nhits, PLM.t);
    pl_cleanup();
    for (int i = 0; i < 2; i++) {
        if (!PLM.held[i]) continue;
        PLM.held[i] = false, g_plim_hold[i] = false;
        P.m[i].placed = false, P.m[i].blend = -1, P.m[i].act = PACT_NONE;  // party_core_tick places him beside Sora (the Drive revert's spot)
        if (g_party_id[i] > 0 && PD.off) PD.off(g_party_id[i]);
    }
    PLM.on = false, PLM.prompt = false;
    if (G.exe_ok && p_guard_set) p_guard_set(mk_drops_hits() || g_dbm_god ? 1 : 0);  // (s133 review: guard_frame only runs while driven - a climb / swim cancel left the drop on)
    if (PLM.shown && CAMP.quads && !CAMP.drew) CAMP.quads(NULL, 0);
    PLM.shown = false;
    if (S.special == SP_LIMIT && !LM.on) S.special = SP_NONE, S.motion = -1;
}
static void pl_cancel(const char *why) {
    if (PLM.on) pl_end(why);
    PLM.req = -1;
}

// (s217 riku) KH2 offers a party Limit only with its member in the party: shown while the member's slot holds his OWN character
// (Comet / Duck Flare: Donald in slot 0, Whirli-Goof / Knocksmash: Goofy in slot 1); with Riku in Donald's place they are not listed
static bool pl_shown(int k) { return k >= 0 && k < PL_NLIM && pchar_limit_ok(g_pchar, PL_LIMS[k].member); }
static int pl_vis(int rows[PL_NLIM]) {  // the listed Limits, in table order
    int n = 0;
    for (int k = 0; k < PL_NLIM; k++)
        if (pl_shown(k)) rows[n++] = k;
    return n;
}
static const char *pl_gate(int k) {  // NULL = can start
    const PlDef *d = &PL_LIMS[k];
    int mi = d->member;
    static char msg[64];
    if (!g_plimit) return "limitparty 0";
    if (!pl_shown(k)) return mi ? "Goofy is not in the party (a stand-in has his place)" : "Donald is not in the party (a stand-in has his place)";  // (s217 riku / s220)
    if (d->spark && !g_ks_on) return "lpks 0";  // (s149) Knocksmash off
    if (d->custom && !g_df_on) return "lpdf 0";  // (s150) Duck Flare off
    if (PLM.on) return "a Limit is running";
    if (IN_GUEST || fm_in_form()) return "not in base Sora";
    if (!P.on || g_party_id[mi] <= 0 || !PD.pose || !PD.slot_pose) return "the member is not drawn";
    if (g_party_absorbed[mi]) return "the member is absorbed by a Drive Form";
    if (P.m[mi].hp <= 0 || P.m[mi].kd != KD_NONE) return "the member is down";
    if (mp_charging() || MP.mp <= 0) return "MP Charge";
    if (S.act != ACT_IDLE || S.takeoff > 0 || S.air_frames || (S.special && S.special != SP_ATTACK)) return "Sora is busy / in the air";
    for (int s = 0; s < d->nst; s++)
        for (int a = 0; a < PL_ACT; a++) {
            int slot = d->st[s].slot[a];
            if (!slot) continue;
            bool ok;
            if (a == 0) {
                static float buf[POSE_BUF_BONES * 12];
                ok = mot_of(slot / 4) && sora_draw_slot_pose(slot, 0, buf);
            } else {
                static float buf[PARTY_MAX_BONES * 12];
                ok = pmot_find(a - 1, slot) && PD.slot_pose(g_party_id[a - 1], slot, 0, buf);
            }
            if (!ok) {
                if (k < 8 && !g_plim_said_slot[k])
                    g_plim_said_slot[k] = true, PLOG("[plimit] %s refused: %s motion slot %d is not in the bakes (install_bakes.sh not run?)\n", d->name, a == 0 ? "Sora's" : PARTY[a - 1].name, slot);
                snprintf(msg, sizeof msg, "motion slot %d missing", slot);
                return msg;
            }
        }
    return NULL;
}
static int pl_state(int k) { return k >= 0 && k < PL_NLIM && !pl_gate(k) ? 0 : 5; }  // KH2 command state: 0 usable, 5 greyed

static void pl_play_step(const Target *tg, int ntg, const float p[3], float to_u) {
    const PlDef *d = pl_def();
    const PlStage *st = pl_stage();
    PLM.tstep = 0, PLM.tp = 0, PLM.plays++;
    memset(PLM.hs, 0, sizeof PLM.hs);
    for (int a = 0; a < PL_ACT; a++) PLM.slot[a] = st->slot[a], PLM.clk[a] = 0;
    g_pl_rsaid = false;
    PLM.gen_done = false, PLM.fxdone = 0, PLM.sndone = 0, PLM.sv = 10.0f, PLM.steered = 0;  // (s134 lpsteer: L1747 / L1839 init: the speed starts at 10)
    PLM.dv.on = false;  // (s149 review) a dive that ran K3a to its length must not fly on (and cut the landing short) in K3b
    if (d->sora_motion && st->slot[0]) {
        const Target *t = g_lock.actor ? target_of(tg, ntg, g_lock.actor) : NULL;
        if (!t) t = target_auto(tg, ntg, S.face, 1000);
        if (t && t->hd > 1) S.face = atan2f(t->d[0], t->d[1]), PLM.pdir[0] = t->d[0] / t->hd, PLM.pdir[1] = t->d[1] / t->hd;
        else PLM.pdir[0] = sinf(S.face), PLM.pdir[1] = cosf(S.face);
        PLM.pv = st->push_k > 0 && t ? fminf(st->push_k * t->hd, st->push_v) : (st->push_k > 0 ? 0.0f : st->push_v);
        play(st->slot[0] / 4), S.special = SP_LIMIT, S.next = 0;
        S.d74 = S.d75 = 0;
    }
    if (d->sora_motion && st->sora_free) {  // (s149 Knocksmash K3a) Sora is handed back: no motion, no hold, the pad is his
        if (S.special == SP_LIMIT && !LM.on) S.special = SP_NONE, S.motion = -1;
        PLOG("[plimit] %s: Sora is free for this stage (S.special %d)\n", d->name, (int)S.special);
    }
    PLOG("[plimit] %s step %d/%d: stage %d, slots %d/%d/%d, %.0f frames%s, prompt \"%s\"\n", d->name, PLM.step + 1, d->nstep, pl_step()->stage, st->slot[0], st->slot[1], st->slot[2],
         st->len, st->loop ? " (loops)" : "", pl_step()->prompt ? pl_cmd_name(pl_step()->prompt) : "-");
    (void)p, (void)to_u;
}
// ---- (s150) Duck Flare (donald2.bar, limt 6, cmd 344; Donald + Sora). KH2 truth: recon/S150_DUCKFLARE_DECODE.md (every number there is KH2's), S139 sections 2, 4, 6.
// Sora is FREE (no motion), Donald rides his hand (Keyblade hidden), throws flares: a 21-flare intro fan, up to 16 hover flares (Rocket Flare 347 fires them as
// missiles; after 3 Rocket Flares Megaduck 348 = 180 frames of bombs), time-out dismount. Each flare = one fx instance moved every frame + a sphere hit.
// KH2 y is DOWN: every y below is already flipped (up). Positions in u, speeds / radii in cm (x to_u) per 60 fps frame (x kh).
// Live keys: lpdf (the Limit, 0 = greyed), lpdffan, lpdfhover, lpdfrocket, lpdfmega, lpdfsnd, lpdffx, lpdfe5 (GUESS: e5 as a hit burst), lpdftime, lpdfkb (hide the Keyblade),
// lpdfhx / lpdfhy / lpdfhz (Donald's seat on Sora, cm left / up / forward). lpforce 6 starts it. lpdiag 1 logs every flare.
static int g_df_fxs = 100;  // (s150c) flare effect size x100 (lpdfscale; the recorded sprites read small in game, S150_RESEARCH_FLAREMODEL.md)
static int g_df_fan = 1, g_df_hover = 1, g_df_rocket = 1, g_df_mega = 1, g_df_snd = 1, g_df_fx = 1, g_df_e5 = 0, g_df_time = 300, g_df_kb = 1;
static int g_df_hand[3] = {-25, 75, 20};  // GUESS: Donald's feet on Sora's right hand: cm left (negative = right) / up / forward
#define DF_N 48
#define DF_HOVER_MAX 16
#define DF_EN_R 50.0f          // GUESS: an enemy is a 50 cm radius, 180 cm tall capsule for the flares' spheres
#define DF_RAD(d) ((d) * 0.017453293f)
enum { DFK_HOVER, DFK_FAN, DFK_ROCKET, DFK_BOMB };
enum { DFS_FREE, DFS_HOVER, DFS_APPEAR, DFS_FLY, DFS_FADE };
typedef struct {
    uint8_t st, kind, mode, leg;
    bool near_, tok, arrived;  // arrived (s150b): hover homing off inside 200 cm of the aim point (KH2 L2763 -> L2996), back on at the re-speed-up (L2000)
    int fx, atkp, power;
    uint32_t fxg;
    float pos[3], dir[3], v, vmax, vacc, r, age, t, life, hoff, leash[2], h[2], leg_t, ldur;
    uint64_t target;
} DfFl;
static struct {
    DfFl fl[DF_N];
    bool live, attached, kb_hidden, timed, mega, fan_pending, bb_on, bb_pend;  // live: pl_df_begin ran (the zeroed struct's fx 0 / aura 0 are not handles)
    int rockets, nmode, nbomb, aura, offer, kb_body;
    uint32_t aurag;
    float spawn_acc, offer_age, floor_y, base[3], pp[3], dface, endwait, bb_t, bb_acc, bb_c[3], se_t[6];
    struct { bool on; float acc; } rk;
} DF;
static bool g_df_said_se[6];
static const char *const DF_FXN[6] = {"lim_donald2_0", "lim_donald2_1", "lim_donald2_2", "lim_donald2_3", "lim_donald2_4", "lim_donald2_5"};
static const char *const DF_SEN[6] = {"df_se999_125", "df_se999_126", "df_se999_127", "df_se999_128", "df_se999_129", "df_se999_130"};  // PAX e0..e5 SEs (moment GUESS)

static float df_len3(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
static float df_to(const float a[3], const float b[3], float u[3]) {  // unit vector a -> b, returns the distance (u); u = 0 when ~0
    float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, l = df_len3(d);
    if (l > 1e-5f) u[0] = d[0] / l, u[1] = d[1] / l, u[2] = d[2] / l; else u[0] = u[1] = u[2] = 0;
    return l;
}
static void df_steer(float dir[3], const float to[3], float turn) {  // turn dir toward the unit vector `to` by at most `turn` rad
    if (turn <= 0 || df_len3(to) < 0.5f) return;
    float cs = dir[0] * to[0] + dir[1] * to[1] + dir[2] * to[2];
    cs = cs > 1 ? 1 : cs < -1 ? -1 : cs;
    float ang = acosf(cs);
    if (ang < 1e-5f) return;
    float t = ang <= turn ? 1.0f : turn / ang, n[3] = {dir[0] + (to[0] - dir[0]) * t, dir[1] + (to[1] - dir[1]) * t, dir[2] + (to[2] - dir[2]) * t}, nl = df_len3(n);
    if (nl > 1e-5f) for (int c = 0; c < 3; c++) dir[c] = n[c] / nl;
}
static float df_yaw(const float d[3]) { return atan2f(d[0], d[2]); }

static void df_sefx(int i, const float at[3]) {  // element i's SE df_se999_(125 + i); missing = one log, skip
    if (!g_df_snd || i < 0 || i > 5 || PLM.t - DF.se_t[i] < 3.0f) return;
    int id = sora_sfx_id(DF_SEN[i]);
    if (id < 0) {
        if (!g_df_said_se[i]) g_df_said_se[i] = true, PLOG("[plimit] sound %s not in sfx.bin (data not installed yet?): skipped\n", DF_SEN[i]);
        return;
    }
    DF.se_t[i] = PLM.t;
    sora_sfx_play_at(id, 1.0f, at);
}
static int df_body(void) { int b = FM.body ? FM.body() : 0; return b > 0 ? b : 0; }  // the draw actor Sora is drawn with (0 base, an outfit, a form / guest body)
static void df_kb(bool hidden) {  // Sora's Keyblade hidden while Donald rides (GUESS: the weapon groups of Sora's drawn body)
    if (hidden == DF.kb_hidden) return;
    if (hidden && !g_df_kb) return;
    DF.kb_hidden = hidden;
    if (!PD.weapon) return;
    // (s150 review) on the CURRENT body: sora_draw_set_body carries hide_weapon over to the next body (Drive Form / guest / outfit swap
    // mid-Limit), so showing actor 0 only would leave the form's / guest's Keyblade (and, after the revert, base Sora's) hidden for good
    int b = df_body();
    if (hidden) DF.kb_body = b, PD.weapon(b, false);
    else {
        PD.weapon(b, true);
        if (DF.kb_body != b) PD.weapon(DF.kb_body, true);
    }
}
static void df_fx_kill(DfFl *f, float fade) {
    if (f->fx >= 0 && fx_mine(f->fx, f->fxg) && sora_fx_alive(f->fx)) {
        if (fade > 0) sora_fx_fadeout(f->fx, fade); else sora_fx_kill(f->fx);
    }
    f->fx = -1;
}
static void df_fx_start(DfFl *f, int el, float to_u) {  // swap the flare's effect for element el (0..3); the old one is killed
    df_fx_kill(f, 0);
    if (!g_df_fx) return;
    int h = pl_fx_p(DF_FXN[el], f->pos, df_yaw(f->dir), to_u * (float)g_df_fxs * 0.01f, false, true);
    if (h >= 0) f->fx = h, f->fxg = g_fx_gen[h];
    df_sefx(el, f->pos);
}
static void df_off(DfFl *f) { df_fx_kill(f, 0); f->st = DFS_FREE; }
static DfFl *df_new(void) {
    for (int i = 0; i < DF_N; i++) if (DF.fl[i].st == DFS_FREE) { memset(&DF.fl[i], 0, sizeof DF.fl[i]); DF.fl[i].fx = -1; return &DF.fl[i]; }
    return NULL;
}
static int df_count(int kind, int st) {
    int n = 0;
    for (int i = 0; i < DF_N; i++) if (DF.fl[i].st == st && DF.fl[i].kind == kind) n++;
    return n;
}
static bool df_busy(void) {
    for (int i = 0; i < DF_N; i++) if (DF.fl[i].st != DFS_FREE) return true;
    return false;
}
static void df_fade_hovers(void) {  // dismissed: 8-frame fade, then freed
    for (int i = 0; i < DF_N; i++) {
        DfFl *f = &DF.fl[i];
        if (f->st != DFS_HOVER) continue;
        if (f->fx >= 0 && fx_mine(f->fx, f->fxg) && sora_fx_alive(f->fx)) sora_fx_fadeout(f->fx, 8);
        f->st = DFS_FADE, f->t = 8;  // (the handle stays: pl_df_cleanup kills it if the Limit ends first)
    }
}
static int df_find(const uint64_t *ea, const int32_t *el, int ne, uint64_t actor) {
    for (int i = 0; i < ne; i++) if (actor && ea[i] == actor && el[i] > 0) return i;
    return -1;
}
static int df_search(const float c[3], float to_u, const float (*ep)[3], const int32_t *el, int ne, int out[20]) {  // 0..1000 cm around c, up to 20
    int n = 0;
    for (int i = 0; i < ne && n < 20; i++) {
        if (el[i] <= 0) continue;
        float d[3] = {ep[i][0] - c[0], ep[i][1] - c[1], ep[i][2] - c[2]};
        if (df_len3(d) / to_u <= 1000.0f) out[n++] = i;
    }
    return n;
}
static int df_nearest(const float c[3], float to_u, float r_cm, const float (*ep)[3], const int32_t *el, int ne) {
    int b = -1;
    float bd = 0;
    for (int i = 0; i < ne; i++) {
        if (el[i] <= 0) continue;
        float d[3] = {ep[i][0] - c[0], ep[i][1] - c[1], ep[i][2] - c[2]}, l = df_len3(d) / to_u;
        if (l <= r_cm && (b < 0 || l < bd)) b = i, bd = l;
    }
    return b;
}
static bool df_hit(DfFl *f, float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne) {  // first hit ends the flare
    for (int i = 0; i < ne; i++) {
        if (el[i] <= 0) continue;
        float dx = ep[i][0] - f->pos[0], dz = ep[i][2] - f->pos[2], hr = (f->r + DF_EN_R) * to_u;
        if (dx * dx + dz * dz > hr * hr) continue;
        if (f->pos[1] < ep[i][1] - f->r * to_u || f->pos[1] > ep[i][1] + (180.0f + f->r) * to_u) continue;
        pl_hit_one(1, f->atkp, f->power, 1, ea[i], el[i], ep[i], to_u, true, "Duck Flare");
        if (g_df_e5) {  // GUESS: element 5 (the 100-frame burst) as a hit burst, default off
            pl_fx_p(DF_FXN[5], f->pos, 0, to_u, false, false);
            df_sefx(5, f->pos);
        }
        return true;
    }
    return false;
}

// hover flare (L1435 / L1494 / L1645): homing on a point 0.5..2.5 m above Sora, 5 deg per frame, speed 4 (+0.01 up to 8), braked x0.96 in the near zone
static void df_spawn_hover(const float p[3], float to_u) {
    DfFl *f = df_new();
    if (!f) return;
    float a = PLM.mface[0] + DF_RAD(pl_randf(-60, 60)), r = pl_randf(50, 150) * to_u;
    f->kind = DFK_HOVER, f->st = DFS_HOVER, f->mode = (uint8_t)(DF.nmode++ & 1);
    f->pos[0] = PLM.mpos[0][0] + sinf(a) * r, f->pos[1] = PLM.mpos[0][1] + 200.0f * to_u, f->pos[2] = PLM.mpos[0][2] + cosf(a) * r;
    f->hoff = pl_randf(50, 250);
    float aim[3] = {p[0], p[1] + f->hoff * to_u, p[2]};
    df_to(f->pos, aim, f->dir);
    if (df_len3(f->dir) < 0.5f) f->dir[1] = 1;
    f->v = 4, f->vmax = 8, f->vacc = 0.01f;
    df_fx_start(f, f->mode ? 2 : 0, to_u);
    if (g_pl_diag) PLOG("[plimit] Duck Flare: hover flare mode %d at %.2f %.2f %.2f (%d hovering)\n", f->mode, f->pos[0], f->pos[1], f->pos[2], df_count(DFK_HOVER, DFS_HOVER));
}
static void df_hover_tick(DfFl *f, const float p[3], float to_u, float kh) {
    float aim[3] = {p[0], p[1] + f->hoff * to_u, p[2]}, u[3];
    float d = df_to(f->pos, aim, u) / to_u;
    if (d < 200.0f) f->arrived = true;  // KH2: homing "arrived" distance 200 cm stops the steering
    if (!f->arrived) df_steer(f->dir, u, 0.0872665f * kh);
    if (!f->near_) {
        if (d < 500.0f) f->near_ = true, f->v *= powf(0.96f, kh);
        else f->v = fminf(f->vmax, f->v + f->vacc * kh);
    } else if (d > 300.0f) {  // (the decode's two thresholds as written: brake inside 500, speed up again beyond 300, +1 per frame up to 10)
        f->near_ = false, f->arrived = false, f->vacc = 1.0f, f->vmax = 10.0f;
        f->v = fminf(f->vmax, f->v + f->vacc * kh);
    } else f->v *= powf(0.96f, kh);
    for (int c = 0; c < 3; c++) f->pos[c] += f->dir[c] * f->v * to_u * kh;
}

// intro fan (L6220 / L6581): 3 rows x 7 behind Sora, appear 20..50 f, then straight at the target, 30..70 cm/f, 45 f
static void df_fan(const float p[3], float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne) {
    int li = df_find(ea, el, ne, g_lock.actor);
    if (li < 0) li = df_nearest(p, to_u, 1500.0f, (const float (*)[3])ep, el, ne);  // GUESS: no lock-on = the nearest enemy (the script has the lock-on only)
    float base = li >= 0 ? atan2f(ep[li][0] - p[0], ep[li][2] - p[2]) : S.face;
    base += 3.14159265f;
    static const float ROW[3] = {30.0f, 100.0f, 170.0f};  // KH2 y 60 / -10 / -80 (down) around Sora's target pos, taken 90 cm up (GUESS), flipped
    int k = 0;
    for (int row = 0; row < 3; row++)
        for (int i = 0; i < 7; i++, k++) {
            DfFl *f = df_new();
            if (!f) continue;
            float a = base + (float)i * DF_RAD(30);
            f->kind = DFK_FAN, f->st = DFS_APPEAR, f->mode = (uint8_t)(k & 1);
            f->pos[0] = p[0] + sinf(a) * 200.0f * to_u - sinf(S.face) * 120.0f * to_u;
            f->pos[1] = p[1] + ROW[row] * to_u;
            f->pos[2] = p[2] + cosf(a) * 200.0f * to_u - cosf(S.face) * 120.0f * to_u;
            f->t = pl_randf(20, 50), f->atkp = 744, f->power = 1, f->r = 60;
            if (li >= 0) f->tok = true, f->target = ea[li];
            float tp[3] = {li >= 0 ? ep[li][0] : f->pos[0] + sinf(S.face), li >= 0 ? ep[li][1] + 0.9f : f->pos[1], li >= 0 ? ep[li][2] : f->pos[2] + cosf(S.face)};
            df_to(f->pos, tp, f->dir);
            df_fx_start(f, f->mode ? 2 : 0, to_u);
        }
    PLOG("[plimit] Duck Flare: intro fan volley, %d flares (atkp 744 radius 60), %s\n", k, li >= 0 ? "at the target" : "no target: straight ahead");
}
// a flare that finished appearing flies: fan (straight, 30..70, 45 f) or bomb (homing, 60 +1 up to 70, 20 f)
static void df_appear_tick(DfFl *f, float to_u, float kh, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne) {
    int ti = f->tok ? df_find(ea, el, ne, f->target) : -1;
    if (ti >= 0) {  // pointing at the target
        float tp[3] = {ep[ti][0], ep[ti][1] + 0.9f, ep[ti][2]}, u[3];
        if (df_to(f->pos, tp, u) > 0) memcpy(f->dir, u, sizeof u);
    }
    if (f->fx >= 0 && fx_mine(f->fx, f->fxg) && sora_fx_alive(f->fx)) sora_fx_move(f->fx, f->pos, df_yaw(f->dir));
    f->t -= kh;
    if (f->t > 0) return;
    if (f->kind == DFK_FAN) {
        if (ti < 0) f->dir[0] = sinf(S.face), f->dir[1] = 0, f->dir[2] = cosf(S.face);  // no target: Sora's facing, y 0
        f->v = pl_randf(30, 70), f->vmax = f->v, f->vacc = 0, f->life = 45;
    } else {
        f->v = 60, f->vmax = 70, f->vacc = 1, f->life = 20;
    }
    f->st = DFS_FLY, f->age = 0;
    df_fx_start(f, f->mode ? 3 : 1, to_u);
}
static void df_fly_tick(DfFl *f, float to_u, float kh, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne) {
    if (f->kind == DFK_BOMB && f->tok) {  // homing 30 -> 40 deg per frame (the max at once)
        int ti = df_find(ea, el, ne, f->target);
        if (ti >= 0) {
            float tp[3] = {ep[ti][0], ep[ti][1] + 0.9f, ep[ti][2]}, u[3];
            df_to(f->pos, tp, u);
            df_steer(f->dir, u, 0.6981f * kh);
        }
    }
    f->v = fminf(f->vmax, f->v + f->vacc * kh);
    for (int c = 0; c < 3; c++) f->pos[c] += f->dir[c] * f->v * to_u * kh;
    f->age += kh, f->life -= kh;
    if (df_hit(f, to_u, ea, ep, el, ne) || f->life <= 0) { df_off(f); return; }
    if (f->fx >= 0 && fx_mine(f->fx, f->fxg) && sora_fx_alive(f->fx)) sora_fx_move(f->fx, f->pos, df_yaw(f->dir));
}

// Rocket Flare (L350 / L3248): a hover flare becomes a missile: 2 detours around the target, then homing
static void df_rocket_fire(DfFl *f, int ti, float to_u, const uint64_t *ea) {
    f->kind = DFK_ROCKET, f->st = DFS_FLY, f->leg = 1, f->age = 0, f->leg_t = 0;
    f->v = 5, f->vmax = 20, f->vacc = 0.5f, f->atkp = 744, f->power = 1, f->r = 60;
    if (ti >= 0) {
        float ya = pl_randf(0, 6.2831853f), ll = pl_randf(400, 800);
        f->tok = true, f->target = ea[ti], f->life = 600;
        f->leash[0] = sinf(ya) * ll, f->leash[1] = cosf(ya) * ll;
        f->h[0] = pl_randf(100, 800), f->h[1] = pl_randf(100, 800), f->ldur = pl_randf(8, 16);
    } else f->life = 30;
    df_fx_start(f, f->mode ? 3 : 1, to_u);
    if (g_pl_diag) PLOG("[plimit] Duck Flare: rocket missile mode %d %s\n", f->mode, ti >= 0 ? "with a target" : "no target (30 f)");
}
static void df_rocket_tick(DfFl *f, float to_u, float kh, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne) {
    int ti = f->tok ? df_find(ea, el, ne, f->target) : -1;
    if (f->tok && ti < 0) f->tok = false, f->life = fminf(f->life, 30.0f);  // the target died: 30 f left
    f->age += kh, f->leg_t += kh;
    if (f->tok) {
        float tp[3] = {ep[ti][0], ep[ti][1] + 0.9f, ep[ti][2]}, goal[3] = {tp[0], tp[1], tp[2]};
        if (f->leg == 1) goal[0] -= f->leash[0] * to_u, goal[2] -= f->leash[1] * to_u, goal[1] = ep[ti][1] + f->h[0] * to_u;
        else if (f->leg == 2) goal[0] += f->leash[0] * to_u, goal[2] += f->leash[1] * to_u, goal[1] = ep[ti][1] + f->h[1] * to_u;
        float u[3], d = df_to(f->pos, goal, u) / to_u;
        float turn = f->leg == 3 ? DF_RAD(10) : fminf(DF_RAD(10), DF_RAD(3) + DF_RAD(0.1f) * f->leg_t);
        if (f->leg_t > (f->leg == 1 ? 2.0f : f->leg == 2 ? 0.0f : 4.0f)) df_steer(f->dir, u, turn * kh);
        if (f->leg == 1 && (d <= 300.0f || f->leg_t >= f->ldur)) f->leg = 2, f->leg_t = 0, f->vacc = 5, f->vmax = 40;
        else if (f->leg == 2 && (d <= 300.0f || f->leg_t >= 60.0f)) f->leg = 3, f->leg_t = 0;  // (60 f = a safety cap, GUESS)
    }
    f->v = fminf(f->vmax, f->v + f->vacc * kh);
    for (int c = 0; c < 3; c++) f->pos[c] += f->dir[c] * f->v * to_u * kh;
    f->life -= kh;
    if (df_hit(f, to_u, ea, ep, el, ne) || f->life <= 0) { df_off(f); return; }
    if (f->fx >= 0 && fx_mine(f->fx, f->fxg) && sora_fx_alive(f->fx)) sora_fx_move(f->fx, f->pos, df_yaw(f->dir));
}
static void df_rocket_thread(float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne, float kh) {
    if (!DF.rk.on) return;
    if (PLM.step != 2) { DF.rk.on = false; return; }  // the motion (70 f) is over, or Megaduck took over
    DF.rk.acc += kh;
    for (int guard = 0; DF.rk.acc >= 2.0f && guard < 4; guard++) {  // every 2 frames: up to 3 ready flares
        DF.rk.acc -= 2.0f;
        int li = df_find(ea, el, ne, g_lock.actor), idx[20];
        const float *c = li >= 0 ? ep[li] : PLM.mpos[0];
        int n = df_search(c, to_u, (const float (*)[3])ep, el, ne, idx), fired = 0;
        for (int i = 0; i < DF_N && fired < 3; i++) {
            DfFl *f = &DF.fl[i];
            if (f->st != DFS_HOVER) continue;
            df_rocket_fire(f, n ? idx[pl_rand(n)] : -1, to_u, ea);
            fired++;
        }
    }
}

// Megaduck Flare (L4280 / L4632 / L5166): 180 frames, every 6 frames 2 bombs above the lock-on target
static void df_bomb_spawn(float to_u, const uint64_t *ea, const float (*ep)[3], const int idx[20], int n) {
    DfFl *f = df_new();
    if (!f) return;
    float a = pl_randf(0, 6.2831853f), r = pl_randf(100, 400) * to_u;
    f->kind = DFK_BOMB, f->st = DFS_APPEAR, f->mode = (uint8_t)(DF.nbomb++ & 1);
    f->pos[0] = DF.bb_c[0] + sinf(a) * r, f->pos[1] = DF.bb_c[1] + pl_randf(200, 300) * to_u, f->pos[2] = DF.bb_c[2] + cosf(a) * r;
    f->t = pl_randf(15, 20), f->atkp = f->mode ? 991 : 745, f->power = f->mode ? 0 : 1, f->r = 100;
    if (n > 0) {
        int ti = idx[pl_rand(n)];
        float tp[3] = {ep[ti][0], ep[ti][1] + 0.9f, ep[ti][2]};
        f->tok = true, f->target = ea[ti];
        df_to(f->pos, tp, f->dir);
    } else {  // no target: a random yaw, mostly down (dir = (cos, down 0..5, sin) normalised)
        float ya = pl_randf(0, 6.2831853f), v[3] = {cosf(ya), -pl_randf(0, 5), sinf(ya)}, l = df_len3(v);
        for (int c = 0; c < 3; c++) f->dir[c] = v[c] / (l > 1e-5f ? l : 1);
    }
    df_fx_start(f, f->mode ? 2 : 0, to_u);
}
static void df_bomb_thread(float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne, float kh) {
    if (!DF.bb_on) return;
    DF.bb_t += kh, DF.bb_acc += kh;
    for (int guard = 0; DF.bb_acc >= 6.0f && guard < 3; guard++) {
        DF.bb_acc -= 6.0f;
        int idx[20], n = df_search(DF.bb_c, to_u, (const float (*)[3])ep, el, ne, idx);
        for (int k = 0; k < 2; k++) df_bomb_spawn(to_u, ea, (const float (*)[3])ep, idx, n);
    }
    if (DF.bb_t >= 180.0f) DF.bb_on = false, PLOG("[plimit] Duck Flare: Megaduck bombs done (180 frames)\n");
}

// Donald: on Sora's hand (ride-on, idle, Rocket) or detached (Megaduck / time-out dismount: his root curves, copying Sora's move in Megaduck)
static void df_donald(const float p[3], float to_u) {
    float *m = PLM.mpos[0];
    DF.floor_y = p[1];
    if (DF.attached) {
        float fx = sinf(S.face), fz = cosf(S.face), L = (float)g_df_hand[0], U = (float)g_df_hand[1], F = (float)g_df_hand[2];
        m[0] = p[0] + (fz * L + fx * F) * to_u, m[1] = p[1] + U * to_u, m[2] = p[2] + (-fx * L + fz * F) * to_u;
        PLM.mface[0] = S.face;
    } else if (PLM.step >= 3) {
        if (PLM.step == 3) for (int c = 0; c < 3; c++) DF.base[c] += p[c] - DF.pp[c];  // Megaduck: he copies Sora's moved delta
        float r[3], fx = sinf(DF.dface), fz = cosf(DF.dface);
        pl_proot(pmot_find(0, PLM.slot[1]), PLM.clk[1], r);
        m[0] = DF.base[0] + (fz * r[0] + fx * r[2]) * to_u, m[1] = DF.base[1] + r[1] * to_u, m[2] = DF.base[2] + (-fx * r[0] + fz * r[2]) * to_u;
        if (m[1] < p[1]) m[1] = p[1];
        PLM.mface[0] = DF.dface;
    }
    memcpy(DF.pp, p, sizeof DF.pp);
}
static void df_stage(int s, const float p[3], float to_u) { PLM.step = s; pl_play_step(NULL, 0, p, to_u); }
static void df_detach(const float p[3], float to_u) {  // Donald leaves Sora's hand: y -10 (10 cm above Sora's feet), facing Sora's way
    DF.attached = false;
    DF.base[0] = PLM.mpos[0][0], DF.base[1] = p[1] + 10.0f * to_u, DF.base[2] = PLM.mpos[0][2], DF.dface = S.face;
    memcpy(DF.pp, p, sizeof DF.pp);
}

static void pl_df_begin(const float p[3], float to_u) {
    memset(&DF, 0, sizeof DF);
    for (int i = 0; i < DF_N; i++) DF.fl[i].fx = -1;
    DF.aura = -1, DF.attached = true, DF.live = true;
    for (int i = 0; i < 6; i++) DF.se_t[i] = -100.0f;
    memcpy(DF.pp, p, sizeof DF.pp);
    df_donald(p, to_u);
    df_kb(true);
    if (g_df_fx) {  // e4 = the ride aura, bound to Donald for the whole Limit
        float at[3] = {PLM.mpos[0][0], PLM.mpos[0][1] + 40.0f * to_u, PLM.mpos[0][2]};
        int h = pl_fx_p(DF_FXN[4], at, S.face, to_u, false, true);
        if (h >= 0) DF.aura = h, DF.aurag = g_fx_gen[h];
        df_sefx(4, at);
    }
    PLOG("[plimit] Duck Flare: Donald rides Sora's hand (seat %d/%d/%d cm), Keyblade %s, aura %d\n", g_df_hand[0], g_df_hand[1], g_df_hand[2], DF.kb_hidden ? "hidden" : "shown", DF.aura);
}
static void pl_df_cleanup(void) {  // (pl_cleanup: every Limit's end; s150 review: a no-op unless a Duck Flare ran, so Comet's end never kills fx slot 0)
    if (!DF.live) return;
    DF.live = false;
    for (int i = 0; i < DF_N; i++) { df_fx_kill(&DF.fl[i], 0); DF.fl[i].st = DFS_FREE; }
    if (DF.aura >= 0 && fx_mine(DF.aura, DF.aurag) && sora_fx_alive(DF.aura)) sora_fx_kill(DF.aura);
    DF.aura = -1, DF.rk.on = false, DF.bb_on = false, DF.bb_pend = false, DF.attached = false, DF.fan_pending = false;
    if (DF.kb_hidden) df_kb(false);
}

// the offer + the Triangle (sora.c pl_tick_press, in place of the step window)
static void pl_df_press(uint64_t *press, const float p[3], float to_u) {
    int offer = 0;
    bool left = PLM.t < (float)g_df_time;
    if ((PLM.step == 1 || PLM.step == 2) && !DF.timed && !DF.mega && left) {
        if (DF.rockets >= 3) offer = g_df_mega ? 348 : 0;
        else if (g_df_rocket && !DF.rk.on && PLM.step == 1 && df_count(DFK_HOVER, DFS_HOVER) >= 10) offer = 347;
    }
    DF.offer_age = offer && offer == DF.offer ? DF.offer_age + 1 : 0;
    DF.offer = offer, PLM.prompt = offer != 0;
    if (!offer) return;
    bool hit = (*press & BTN_CLIMB) || (g_pl_auto && DF.offer_age >= 2);
    if (!hit) return;
    *press &= ~(uint64_t)BTN_CLIMB;
    sora_sfx_event("decide", 1.0f);
    PLOG("[plimit] press \"%s\" at limit frame %.0f (%d hover flares)\n", pl_cmd_name(offer), PLM.t, df_count(DFK_HOVER, DFS_HOVER));
    PLM.prompt = false, DF.offer = 0;
    if (offer == 347) {
        DF.rockets++, DF.rk.on = true, DF.rk.acc = 0;
        if (g_df_snd && psfx_on()) psfx_voice(0, 4, 0, PLM.mpos[0]);  // Donald vtbl4 @0 (donald_attack.N)
        df_stage(2, p, to_u);
    } else {
        DF.mega = true, DF.rk.on = false;
        df_fade_hovers();
        df_detach(p, to_u);
        DF.bb_on = true, DF.bb_t = 0, DF.bb_acc = 6.0f, DF.bb_pend = true;  // the centre is picked on the first tick (it needs the enemy list)
        memcpy(DF.bb_c, p, sizeof DF.bb_c);
        if (g_df_snd && psfx_on()) psfx_voice(0, 16, 0, PLM.mpos[0]);  // Donald vtbl16 @0 (donald_megaduck.0)
        df_stage(3, p, to_u);
    }
}
static void pl_df_timeout(const float p[3], float to_u) {
    PLOG("[plimit] Duck Flare: limit time over (%.0f frames): flares fade, Donald dismounts\n", PLM.t);
    DF.timed = true, DF.rk.on = false;
    df_fade_hovers();
    df_kb(false);
    df_detach(p, to_u);
    df_stage(4, p, to_u);
}
// sora.c pl_chain (after the clocks): the stage flow
static void pl_df_chain(const float p[3], float to_u, float kh) {
    const PlStage *st = pl_stage();
    switch (PLM.step) {
    case 0:  // ride-on 1508 (46 f), then the fan and the idle loop
        if (PLM.tstep >= st->len) DF.fan_pending = g_df_fan != 0, df_stage(1, p, to_u);
        break;
    case 1:  // idle 1500 (loops, re-issued every pass)
        if (PLM.t >= (float)g_df_time) pl_df_timeout(p, to_u);
        break;
    case 2:  // Rocket 1504 (70 f)
        if (PLM.tstep >= st->len) DF.rk.on = false, df_stage(1, p, to_u);
        break;
    case 3:  // Megaduck 1516 (148 f); the Keyblade is back at the end; the Limit ends after the 180 f of bombs
        if (PLM.tstep >= st->len) df_kb(false);
        if (PLM.tstep >= st->len && !DF.bb_on && !df_busy()) pl_end("done");
        else if (PLM.tstep > 520.0f) pl_end("Megaduck timeout");  // (safety: 148 + 180 + the bombs' life, never reached)
        break;
    case 4:  // dismount 1512 (40 f), then wait for the flares in flight (bounded)
        if (PLM.tstep >= st->len) {
            DF.endwait += kh;
            if (!df_busy() || DF.endwait > 120.0f) pl_end("limit time over");
        }
        break;
    default: break;
    }
}
// sora.c pl_tick_press (every frame): Donald's place, the aura, the spawners and every flare
static void pl_df_tick(const float p[3], float to_u, const uint64_t *ea, float (*ep)[3], const int32_t *el, int ne, float kh) {
    df_donald(p, to_u);
    if (DF.aura >= 0) {
        if (fx_mine(DF.aura, DF.aurag) && sora_fx_alive(DF.aura)) {
            float at[3] = {PLM.mpos[0][0], PLM.mpos[0][1] + 40.0f * to_u, PLM.mpos[0][2]};
            sora_fx_move(DF.aura, at, PLM.mface[0]);
        } else DF.aura = -1;
    }
    if (DF.fan_pending) DF.fan_pending = false, df_fan(p, to_u, ea, ep, el, ne);
    if (DF.bb_pend) {  // Megaduck's bomb centre (once): the lock-on target, else the nearest enemy in front (1000 cm), else Sora; 1 m up
        DF.bb_pend = false;
        int li = df_find(ea, el, ne, g_lock.actor);
        if (li < 0) {
            float bd = 0;
            for (int i = 0; i < ne; i++) {
                float dx = ep[i][0] - p[0], dz = ep[i][2] - p[2], d = sqrtf(dx * dx + dz * dz) / to_u;
                if (el[i] <= 0 || d > 1000.0f || dx * sinf(S.face) + dz * cosf(S.face) < 0) continue;
                if (li < 0 || d < bd) li = i, bd = d;
            }
        }
        if (li >= 0) memcpy(DF.bb_c, ep[li], sizeof DF.bb_c);
        DF.bb_c[1] += 100.0f * to_u;
        PLOG("[plimit] Duck Flare: Megaduck bombs centre %.2f %.2f %.2f (%s)\n", DF.bb_c[0], DF.bb_c[1], DF.bb_c[2], li >= 0 ? "target" : "Sora");
    }
    // hover spawn: every 2 frames while idle, < 3 Rocket Flares, no fan / missile alive, < 16 hovering
    if ((PLM.step == 1 || PLM.step == 2) && g_df_hover && !DF.timed && !DF.mega && DF.rockets < 3 && PLM.t < (float)g_df_time && !df_count(DFK_FAN, DFS_APPEAR) &&
        !df_count(DFK_FAN, DFS_FLY) && !df_count(DFK_ROCKET, DFS_FLY)) {
        DF.spawn_acc += kh;
        for (int guard = 0; DF.spawn_acc >= 2.0f && guard < 2; guard++) {
            DF.spawn_acc -= 2.0f;
            if (df_count(DFK_HOVER, DFS_HOVER) < DF_HOVER_MAX) df_spawn_hover(p, to_u);
        }
    } else DF.spawn_acc = 0;
    df_rocket_thread(to_u, ea, ep, el, ne, kh);
    df_bomb_thread(to_u, ea, ep, el, ne, kh);
    for (int i = 0; i < DF_N; i++) {
        DfFl *f = &DF.fl[i];
        if (f->st == DFS_HOVER) {
            df_hover_tick(f, p, to_u, kh);
            f->age += kh;
            if (f->age > 600.0f) df_off(f);
        } else if (f->st == DFS_APPEAR) df_appear_tick(f, to_u, kh, ea, ep, el, ne);
        else if (f->st == DFS_FLY) {
            if (f->kind == DFK_ROCKET) df_rocket_tick(f, to_u, kh, ea, ep, el, ne);
            else df_fly_tick(f, to_u, kh, ea, ep, el, ne);
        } else if (f->st == DFS_FADE && (f->t -= kh) <= 0) df_off(f);
    }
    // the hover flares keep 60 cm apart (the exe pool's push-apart)
    for (int i = 0; i < DF_N; i++) {
        if (DF.fl[i].st != DFS_HOVER) continue;
        for (int j = i + 1; j < DF_N; j++) {
            if (DF.fl[j].st != DFS_HOVER) continue;
            DfFl *a = &DF.fl[i], *b = &DF.fl[j];
            float d[3] = {b->pos[0] - a->pos[0], b->pos[1] - a->pos[1], b->pos[2] - a->pos[2]}, l = df_len3(d), min = 60.0f * to_u;
            if (l >= min) continue;
            if (l < 1e-5f) d[0] = 1, l = 1;
            float push = (min - (l < min ? l : min)) * 0.5f;
            for (int c = 0; c < 3; c++) a->pos[c] -= d[c] / l * push, b->pos[c] += d[c] / l * push;
        }
    }
    for (int i = 0; i < DF_N; i++) {  // the hover effects follow
        DfFl *f = &DF.fl[i];
        if (f->st == DFS_HOVER && f->fx >= 0) {
            if (fx_mine(f->fx, f->fxg) && sora_fx_alive(f->fx)) sora_fx_move(f->fx, f->pos, df_yaw(f->dir));
            else f->fx = -1;
        }
    }
}

static bool pl_begin(int k, const Target *tg, int ntg, const float p[3], float to_u) {
    const char *why = pl_gate(k);
    if (why) {
        sora_sfx_event("error", 1);
        PLOG("[plimit] %s refused: %s\n", PL_LIMS[k].name, why);
        return false;
    }
    const PlDef *d = &PL_LIMS[k];
    memset(&PLM.cm, 0, sizeof PLM.cm);
    PLM.on = true, PLM.lim = k, PLM.step = 0, PLM.t = 0, PLM.plays = 0, PLM.nhits = 0, PLM.nfx = 0, PLM.rain = false, PLM.prompt = false;
    PLM.mp0 = MP.mp, PLM.spk_t = -100.0f;
    memset(&PLM.sh, 0, sizeof PLM.sh), memset(&PLM.dv, 0, sizeof PLM.dv);  // (s149)
    for (int i = 0; i < PL_SHIELDS; i++) PLM.sh[i].fx = -1;
    for (int a = 0; a < PL_ACT; a++) PLM.loop_fx[a] = -1;
    PLM.ntr = 0, PLM.ntr_next = 0;  // (s135 lpfxfollow)
    mp_change(-(int)ceilf(MP.mp) - 1);  // Cost 255 = all MP, then MP Charge (the Cure rule, sora.c)
    for (int i = 0; i < PL_COMETS; i++) PLM.cm[i].fx = -1;
    int mi = d->member;
    PLM.held[mi] = true, g_plim_hold[mi] = true;
    // the member leaves the follow / combat AI where he stands (the draw is ours); Donald is set by the script (bg_get_random_pos_air)
    memcpy(PLM.mpos[mi], P.m[mi].pos, sizeof PLM.mpos[mi]);
    PLM.mface[mi] = P.m[mi].face;
    if (k == 0) {  // Comet: Donald appears 200..250 cm from Sora, 150 cm above (KH2 y -150 = up; GUESS: PHH's port reads it as below)
        float ang = pl_randf(0, 6.2831853f), r = pl_randf(200, 250) * to_u;
        PLM.mpos[0][0] = p[0] + sinf(ang) * r, PLM.mpos[0][1] = p[1] + 150 * to_u, PLM.mpos[0][2] = p[2] + cosf(ang) * r;
        PLM.mface[0] = atan2f(p[0] - PLM.mpos[0][0], p[2] - PLM.mpos[0][2]);
    }
    M.dead = 8, M.sub = -1, M.cursor = 0, M.sub_cursor = 0;
    PLOG("[plimit] LIMIT %s (limt %d, cmd %d): MP %.0f -> %.0f%s, with %s\n", d->name, d->limt, d->cmd, PLM.mp0, MP.mp, mp_charging() ? " (MP Charge)" : "", PARTY[mi].name);
    if (d->custom) pl_df_begin(p, to_u);  // (s150)
    pl_play_step(tg, ntg, p, to_u);
    return true;
}
static void pl_request(int k) { if (k >= 0 && k < PL_NLIM) PLM.req = k, PLOG("[plimit] %s requested\n", PL_LIMS[k].name); }

static void pl_next_step(const Target *tg, int ntg, const float p[3], float to_u, const char *how) {
    PLM.step++;
    PLOG("[plimit] %s -> step %d/%d (%s)\n", pl_def()->name, PLM.step + 1, pl_def()->nstep, how);
    if (pl_stage()->gen == PLG_DIVE && !g_ks_dive) { pl_end("lpksdive 0 (no solo dive)"); return; }  // (s149)
    pl_play_step(tg, ntg, p, to_u);
}

// sora.c step 2, before the motion-end check (the lm_chain place): the clocks, the step's end, the next step
static void pl_chain(const float p[3], float kh, float to_u) {
    if (!PLM.on) return;
    if (linkpos_link_life() <= 0) { pl_cancel("Sora KO"); return; }
    if (fm_in_form()) { pl_cancel("Drive Form"); return; }
    if (!g_plimit) { pl_cancel("limitparty 0"); return; }
    const PlDef *d = pl_def();
    if (g_party_absorbed[d->member]) { pl_cancel("the member was absorbed"); return; }
    if (IN_GUEST) { pl_cancel("the body is a guest (Roxas / Mickey)"); return; }  // (s133 review)
    if (g_party_id[d->member] <= 0) { pl_cancel("the member is gone (party off)"); return; }  // (s133 review: no invisible member hitting on)
    const PlStage *st = pl_stage();
    PLM.tp = PLM.tstep;
    PLM.tstep += kh, PLM.t += kh;
    for (int a = 1; a < PL_ACT; a++) {
        if (!PLM.slot[a]) continue;
        const PMotX *mo = pmot_find(a - 1, PLM.slot[a]);
        PLM.clk[a] += kh;
        if (mo && PLM.clk[a] >= mo->frames) PLM.clk[a] = st->loop ? fmodf(PLM.clk[a], (float)mo->frames) : (float)mo->frames - 0.01f;
    }
    if (d->custom) { pl_df_chain(p, to_u, kh); return; }  // (s150) Duck Flare's own flow
    if (st->tlimit > 0 && PLM.tstep >= st->tlimit) { pl_end("limit time over (no follow-up)"); return; }
    if (PLM.tstep >= st->len && !st->loop) {
        const PlStep *sp = pl_step();
        if (PLM.step + 1 < d->nstep && sp->auto_next) pl_next_step(NULL, 0, p, to_u, "motion ended");
        else if (PLM.step + 1 >= d->nstep) {
            if (!pl_comets_busy() && !pl_sh_busy()) pl_end("done");
        } else pl_end("no follow-up press");
    }
}

// sora.c, inside the ACT_IDLE special-root block: Sora's script move (L294 / L978) and, (s134 lpsteer) the stick's steering of the spin.
// goofy.bar L1338 (the hook of Sora's motion 254 = Whirli-Goofga, stage 3, VERIFIED): every frame the stick (trap_player_get_movement) turns the
//   movement direction toward it by at most 0.20944 rad (12 deg) x frametime x stick magnitude; below magnitude 0.1 it keeps the direction at 0.1;
//   speed = [L2137 value: starts 10, x (1 - 0.05 x frametime) per frame down to 8.0 cm per frame] x magnitude (0.1 .. 1), trap_obj_set_movement.
//   (the jump arc of the same hook, uptime / gravity 0.027222 / max 10, is NOT ported: BotW ground only.) L2137 runs while the motion's
//   range 1 is on: GUESS = the whole step here. KH2's stick is camera relative like the walk: sdir / mag come from sora.c's walk code.
// goofy.bar L978 (stage 2, motion 253 = Whirli-Goofra) has NO stick in the script (5 cm per frame toward the nearest target, x 0.99): lpsteer 1
//   also steers that spin with the same turn rate and 5 cm per frame x (0.1 .. 1) x 0.99 per frame: GUESS (the user asked to move in it).
//   lpsteer 2 = stage 3 only (the script's own), 0 = the old fixed move.
static bool pl_steering(void) {
    if (!pl_hold() || !g_pl_steer) return false;
    const PlStage *st = pl_stage();
    if (st->slot[0] == 1208) return true;                  // stage 3: VERIFIED script
    return g_pl_steer == 1 && (st->slot[0] == 1204 || st->slot[0] == 1200);  // stages 1 + 2: GUESS (s135 user: move in stage 1 too)
}
static void pl_move(float move[2], float kh, const float sdir[2], float mag) {
    if (!pl_hold()) return;
    if (pl_steering() && pl_stage()->slot[0] == 1200) {
        // (s135 user: "I want to move in the first stage too") goofy.bar L294 has NO stick here: the script's dash toward the
        // target stays as it was; the stick adds a walk of 5 cm per KH frame x magnitude (GUESS = stage 2's speed) and turns
        // Sora toward it at the stage-3 rate (12 deg x frametime x magnitude, L1338)
        move[0] += PLM.pdir[0] * PLM.pv * kh, move[1] += PLM.pdir[1] * PLM.pv * kh;
        PLM.pv *= powf(1.0f - pl_stage()->push_decay, kh);
        if (mag >= 0.1f && (sdir[0] != 0 || sdir[1] != 0)) {
            float m = mag > 1 ? 1 : mag, d = wrap(atan2f(sdir[0], sdir[1]) - S.face), cap = 0.20944f * kh * m;
            S.face = wrap(S.face + (d > cap ? cap : d < -cap ? -cap : d));
            move[0] += sdir[0] * 5.0f * m * kh, move[1] += sdir[1] * 5.0f * m * kh;
            if (PLM.steered++ == 0)
                PLOG("[plimit] %s: the stick moves Sora in stage 1 (lpsteer %d, GUESS, 5 cm per frame x mag %.2f)\n", pl_def()->name, g_pl_steer, m);
        }
        return;
    }
    if (!pl_steering()) {
        move[0] += PLM.pdir[0] * PLM.pv * kh, move[1] += PLM.pdir[1] * PLM.pv * kh;
        PLM.pv *= powf(1.0f - pl_stage()->push_decay, kh);
        return;
    }
    const PlStage *st = pl_stage();
    float m = mag < 0.1f ? 0.1f : mag > 1 ? 1 : mag;
    if (mag >= 0.1f && (sdir[0] != 0 || sdir[1] != 0)) {  // L1431-1550: the angle from the facing to the stick, capped at 12 deg x frametime x magnitude
        float cur = atan2f(PLM.pdir[0], PLM.pdir[1]), d = wrap(atan2f(sdir[0], sdir[1]) - cur), cap = 0.20944f * kh * m;
        cur = wrap(cur + (d > cap ? cap : d < -cap ? -cap : d));
        PLM.pdir[0] = sinf(cur), PLM.pdir[1] = cosf(cur);
        S.face = cur;  // Goofy hangs on Sora's frame (pl_goofy_pos), so he turns with him
    }
    float v;
    if (st->slot[0] == 1208) {  // L2137: [8] = max([8] - 0.05 x [8] x frametime, 8.0), starting at 10
        v = PLM.sv, PLM.sv = fmaxf(PLM.sv * (1.0f - 0.05f * kh), 8.0f);
    } else {                    // stage 2 GUESS: L978's 5 cm per frame x 0.99 per frame (PLM.pv: 5 decaying 1 %), stick-scaled
        v = PLM.pv, PLM.pv *= powf(1.0f - st->push_decay, kh);
    }
    v *= (st->slot[0] == 1208 || mag >= 0.1f) ? m : 1.0f;  // (s134 review: stage 2 stick released = the script's own 5 cm, not 0.5)
    move[0] += PLM.pdir[0] * v * kh, move[1] += PLM.pdir[1] * v * kh;
    if (PLM.steered++ == 0)
        PLOG("[plimit] %s: the stick steers the spin (lpsteer %d, %s, %.1f cm per frame at mag %.2f)\n", pl_def()->name, g_pl_steer, st->slot[0] == 1208 ? "L1338 VERIFIED" : "GUESS", v, m);
}

// sora.c, after lm_tick_press: start a requested Limit, the presses, the effects / generators / hits of this tick
static void pl_tick_press(uint64_t *press, const Target *tg, int ntg, const float p[3], float to_u, float kh) {
    if (g_pl_force >= 0) {  // lpforce <limt id>: ONE-SHOT
        int k = -1;
        for (int i = 0; i < PL_NLIM_DEFS; i++) if (PL_LIMS[i].limt == g_pl_force) k = i;  // 5 Comet, 3 Whirli-Goof, 4 Knocksmash (s149)
        PLOG("[plimit] lpforce %d fired (remove the line from live_test.txt)\n", g_pl_force);
        g_pl_force = -1;
        if (k >= 0) PLM.req = k; else PLOG("[plimit] lpforce: limt id must be 3 (Whirli-Goof), 4 (Knocksmash), 5 (Comet) or 6 (Duck Flare)\n");
    }
    if (PLM.req >= 0 && !PLM.on) {
        int k = PLM.req;
        PLM.req = -1;
        pl_begin(k, tg, ntg, p, to_u);
    }
    if (!PLM.on) { PLM.prompt = false; return; }
    const PlDef *d = pl_def();
    const PlStage *st = pl_stage();
    const PlStep *sp = pl_step();
    bool atk = (*press & BTN_ATTACK) != 0;
    if (pl_hold()) *press &= ~(BTN_JUMP | BTN_ROLL | BTN_ATTACK);  // (s149: not in a stage that hands Sora back)  // the pad is the Limit's (Sora plays its motions)
    // the Triangle window (range 26 param 254 / 279): the prompt is up inside it
    bool in_win = !d->custom && sp->prompt && st->win_b > 0 && PLM.tstep >= st->win_a && PLM.tstep <= st->win_b;
    PLM.prompt = in_win;
    if (d->custom) pl_df_press(press, p, to_u), st = pl_stage(), sp = pl_step();  // (s150)
    if (in_win) {
        bool hit = (*press & BTN_CLIMB) || (d->sora_motion && atk) || (g_pl_auto && PLM.tstep >= st->win_a + 2);
        if (hit && PLM.step + 1 < d->nstep) {
            *press &= ~(uint64_t)BTN_CLIMB;
            sora_sfx_event("decide", 1.0f);
            PLOG("[plimit] press \"%s\" at stage frame %.0f%s\n", pl_cmd_name(sp->prompt), PLM.tstep, g_pl_auto ? " (lpauto)" : "");
            pl_next_step(tg, ntg, p, to_u, pl_cmd_name(sp->prompt));
            st = pl_stage(), sp = pl_step();
        }
    }
    // the enemies (u)
    static uint64_t ea[64];
    static float ep[64][3];
    static int32_t el[64];
    int ne = linkpos_enemies(ea, ep, el, 64);
    if (ne < 0) ne = 0;
    // the members' places
    if (d->member == 1 && st->slot[2] && !st->sora_free) pl_goofy_pos(st, p, to_u);
    if (d->member == 0 && !d->custom) {  // Comet: Donald eases to 120 cm from Sora at 30 cm up and hovers facing him (GUESS: PHH CometMove / COMET_RADIUS 120, simplified)
        float dx = PLM.mpos[0][0] - p[0], dz = PLM.mpos[0][2] - p[2], r = sqrtf(dx * dx + dz * dz) / to_u;
        float a = r > 1 ? atan2f(dx, dz) : S.face;
        float gr = 120 * to_u, k = 1.0f - powf(0.93f, kh);
        float gx = p[0] + sinf(a) * gr, gz = p[2] + cosf(a) * gr, gy = p[1] + 30 * to_u;
        if (PLM.step >= 1) PLM.mpos[0][0] += (gx - PLM.mpos[0][0]) * k, PLM.mpos[0][2] += (gz - PLM.mpos[0][2]) * k;
        PLM.mpos[0][1] += (gy - PLM.mpos[0][1]) * (1.0f - powf(0.97f, kh));
        PLM.mface[0] = atan2f(p[0] - PLM.mpos[0][0], p[2] - PLM.mpos[0][2]);
    }
    // the stage's effects (the motion / script frame triggers); a first_only one starts with the Limit's first play only
    for (int i = 0; i < st->nfx; i++) {
        const PlFx *f = &st->fx[i];
        if (!f->name || (PLM.fxdone >> i & 1) || PLM.tstep < f->at) continue;
        PLM.fxdone |= 1u << i;
        if (f->first_only && PLM.plays > 1) continue;
        if (f->first_only && !d->sora_motion && PLM.step > 0) continue;
        float pos[3];
        pl_anchor(f->actor, f->anchor, p, to_u, pos);
        int h = pl_fx(f->name, pos, f->actor == 0 ? S.face : PLM.mface[f->actor - 1], to_u, true);
        if (f->loop && h >= 0) PLM.loop_fx[f->actor] = h, PLM.loop_g = g_fx_gen[h];
        else if (h >= 0 && g_pl_fxfollow) {  // (s135 user: stage 2's effect froze while Sora + Goofy steered away) it rides its actor
            int k = PLM.ntr < PL_TRK ? PLM.ntr++ : (PLM.ntr_next++ % PL_TRK);
            PLM.tr[k].h = h, PLM.tr[k].g = g_fx_gen[h], PLM.tr[k].actor = f->actor, PLM.tr[k].anchor = f->anchor;
        }
        PLOG("[plimit] fx %s at stage frame %.0f -> %d\n", f->name, f->at, h);
    }
    for (int k = 0; k < PLM.ntr; k++) {  // (s135 lpfxfollow, GUESS: KH2's trap_limit_effect_start binds the stage effect to its actor / bone)
        if (PLM.tr[k].h < 0 || !fx_mine(PLM.tr[k].h, PLM.tr[k].g) || !sora_fx_alive(PLM.tr[k].h)) { PLM.tr[k].h = -1; continue; }
        float pos[3];
        pl_anchor(PLM.tr[k].actor, PLM.tr[k].anchor, p, to_u, pos);
        sora_fx_move(PLM.tr[k].h, pos, PLM.tr[k].actor == 0 ? S.face : PLM.mface[PLM.tr[k].actor - 1]);
    }
    if (PLM.loop_fx[1] >= 0 && d->member == 0 && fx_mine(PLM.loop_fx[1], PLM.loop_g) && sora_fx_alive(PLM.loop_fx[1])) {
        float pos[3];
        pl_anchor(1, PLB_CHEST, p, to_u, pos);
        sora_fx_move(PLM.loop_fx[1], pos, PLM.mface[0]);
    }
    // the generators (frame trigger 1)
    if (st->gen && !PLM.gen_done && PLM.tstep >= st->gen_at) {
        PLM.gen_done = true;
        if (st->gen == PLG_VOLLEY) pl_volley(p, to_u, ea, ep, el, ne);
        else if (st->gen == PLG_RAIN) pl_rain_start();
        else if (st->gen == PLG_SHIELD) pl_sh_throw(0, p, to_u, ea, ep, el, ne);
        else if (st->gen == PLG_DUO) pl_sh_throw(1, p, to_u, ea, ep, el, ne), pl_sh_throw(2, p, to_u, ea, ep, el, ne);
        else if (st->gen == PLG_DIVE) pl_dive_start(p, to_u, ea, ep, el, ne);
    }
    pl_snd_tick(p);
    pl_dive_tick(to_u, ea, ep, el, ne, kh);
    pl_sh_tick(p, to_u, ea, ep, el, ne, kh);
    pl_comets(p, to_u, ea, ep, el, ne, kh);
    pl_hits(p, to_u, ea, ep, el, ne, kh);
    if (d->custom) pl_df_tick(p, to_u, ea, ep, el, ne, kh);  // (s150)
}

// sora.c after party_tick: the held members are drawn here
static void pl_draw(float to_u) {
    if (!PLM.on) return;
    const PlStage *st = pl_stage();
    for (int i = 0; i < 2; i++) {
        if (!PLM.held[i] || g_party_id[i] <= 0 || !PD.pose) continue;
        int a = i + 1, slot = st->slot[a];
        if (!slot) {  // no motion this step: the last pose stays (not drawn: GUESS)
            if (PCAP.stamp[i] && PCAP.stamp[i] + 1 == PCAP.frame) PCAP.stamp[i] = PCAP.frame;  // (mp3 review M2: the remote keeps him too)
            continue;
        }
        float floor_y = pl_def()->custom ? DF.floor_y : PLM.mpos[i][1];
        if (pl_def()->custom) PD.shadow(g_party_id[i], DF.floor_y, DF.attached ? 0.0f : blob_str(0.55f));  // (s150: on Sora's hand = no blob; else on the ground under Sora)
        else PD.shadow(g_party_id[i], PLM.mpos[i][1], blob_str(0.55f));
        if (PD.ghost) PD.ghost(g_party_id[i], 0.0f);
        PD.pose(g_party_id[i], slot, PLM.clk[a], PLM.mpos[i], PLM.mface[i], to_u);
        pcap_commit(i, PLM.mpos[i], PLM.mface[i], slot, PLM.clk[a], floor_y, false, false, 0, 0, 0, NULL);  // (slice 3: the SNAP's party block; on Sora's hand the blob is off, the floor_dy is only the remote's blob y)
    }
}

// ---- the menu: KH2 lists party Limits under the "Limit" entry (cmd 81, SubMenu 9) of its SECOND top page (kind 6: Attack / Summon / Party / Limit,
// built by re:0x3F3860; PHH command_menu.py), greyed in MP Charge / MP 0 (re:0x401D60), the member absent / KO'd, and (ours) absorbed by a Drive Form.
// Here the list opens with d-pad Right on the Attack row (d-pad Left too when the Slate page does not take it); A runs the cursor's Limit.
// The names are drawn with the camp font (the baked command atlas has no Comet / Whirli-Goof labels).
static bool pl_any_member(void) { return g_plimit && P.on && (g_party_id[0] > 0 || g_party_id[1] > 0); }
static bool pl_open_left(void) {
    int vis[PL_NLIM];
    if (!pl_any_member() || ROXCMD || PLM.on || !pl_vis(vis)) return false;  // (s217 riku: nothing listed = no page)
    M.sub = PL_SUB, M.sub_cursor = 0;
    sora_sfx_event("submenu", 1);
    PLOG("[plimit] menu: Limit page open (Comet %s, Whirli-Goof %s, Knocksmash %s, Duck Flare %s)\n", pl_gate(0) ? pl_gate(0) : "ready", pl_gate(1) ? pl_gate(1) : "ready", pl_gate(2) ? pl_gate(2) : "ready", pl_gate(3) ? pl_gate(3) : "ready");
    return true;
}
static int pl_menu_tick(uint64_t *press) {
    int vis[PL_NLIM], nv = pl_vis(vis);  // (s217 riku: the cursor walks the LISTED Limits; M.sub_cursor = their row)
    if (!pl_any_member() || PLM.on || !nv) { M.sub = -1, M.cursor = 0; return -1; }
    if (M.sub_cursor >= nv) M.sub_cursor = nv - 1;
    if (*press & BTN_UP) M.sub_cursor = (M.sub_cursor + nv - 1) % nv, sora_sfx_event("cursor", 1);
    if (*press & BTN_DOWN) M.sub_cursor = (M.sub_cursor + 1) % nv, sora_sfx_event("cursor", 1);
    if (*press & (BTN_JUMP | BTN_LEFT | BTN_RIGHT)) {
        *press &= ~(BTN_JUMP | BTN_LEFT | BTN_RIGHT);
        M.sub = -1, M.cursor = 0;
        sora_sfx_event("cancel", 1);
        return -1;
    }
    if (*press & BTN_ATTACK) {
        *press &= ~BTN_ATTACK;
        int k = vis[M.sub_cursor];
        const char *why = pl_gate(k);
        if (M.dead > 0 || why) {
            sora_sfx_event("error", 1);
            PLOG("[plimit] menu: %s greyed (%s)\n", PL_LIMS[k].name, why ? why : "menu dead");
            return -1;
        }
        return CMD_PLIMIT + k;
    }
    return -1;
}

// ---- the overlay (the camp font, as the reaction prompt): the Limit page's list and the Triangle prompt
static void pl_quad_box(CsOut *o, float x0, float y0, float w, float h, const float c[4]) {
    SoraHudQuad *q = &o->q[o->n++];
    memset(q, 0, sizeof *q);
    q->x[0] = q->x[2] = x0, q->x[1] = q->x[3] = x0 + w, q->y[0] = q->y[1] = y0, q->y[2] = q->y[3] = y0 + h;
    for (int k = 0; k < 4; k++) q->col[k] = cs_pack_(c);
    q->tex = 2;
}
static void pl_text(CsOut *o, const char *txt, float x, float y, const float col[4]) {
    uint8_t msg[32];
    int len = 0;
    for (int c = 0; txt[c] && len < 31; c++) {
        uint8_t b = cs_ascii(&CAMP.seq, txt[c]);
        if (b) msg[len++] = b;
    }
    const float sh[4] = {0, 0, 0, 0.75f * col[3]};
    cs_text(&CAMP.seq, o, msg, len, x + 1.5f, y + 1.5f, 1.0f, sh, NULL, NULL);
    cs_text(&CAMP.seq, o, msg, len, x, y, 1.0f, col, NULL, NULL);
}
static bool g_plbox = false;  // live "plbox 1": the old camp-font list box (the page is a real command-menu page otherwise)
static int pl_prompt_cmd(void) { return pl_def()->custom ? DF.offer : pl_step()->prompt; }  // (s150) Duck Flare's offer is dynamic (347 / 348)
static void pl_hud(SoraHudMenu *m) {
    int vis[PL_NLIM], nv = pl_vis(vis);  // (s217 riku: only the listed Limits)
    bool list = M.sub == PL_SUB && pl_any_member() && nv, prompt = PLM.on && PLM.prompt;
    if (list && !g_plbox) {  // (s134 rcslot) the Limit list = a real command-menu page, as the Slate page (sk_hud): the Drive page's look + the names as text rows
        m->sub = 2, m->n_sub = nv;
        const char *rows[PL_NLIM];
        for (int j = 0; j < nv; j++) rows[j] = PL_LIMS[vis[j]].name, m->sub_label[j] = 23, m->sub_state[j] = (uint8_t)pl_state(vis[j]), m->sub_cost[j] = 0;
        sub_text_rows(m, rows, nv, M.sub_cursor);
        list = false;
    } else if (M.sub == PL_SUB) m->sub = -1;  // plbox 1: the exe draws the top page only; the list is ours
    if (prompt && rcs_show(pl_cmd_name(pl_prompt_cmd()), false)) {  // (s134 rcslot) KH2's Reaction Command slot
        if (PLM.shown && CAMP.quads && !CAMP.drew) CAMP.quads(NULL, 0);
        PLM.shown = false;
        prompt = false;
    }
    if (!list && !prompt) {
        if (PLM.shown && CAMP.quads && !CAMP.drew) CAMP.quads(NULL, 0);
        PLM.shown = false;
        return;
    }
    if (!CAMP.tried) camp_load();
    if (!CAMP.ok || !CAMP.quads || CAMP.drew || REACT.kind) return;
    CsOut o = {PLM.q, 0, 96};
    if (list) {
        const float box[4] = {0.05f, 0.05f, 0.08f, 0.75f};
        pl_quad_box(&o, 150, 292, 190, 24.0f * nv + 16, box);
        for (int j = 0; j < nv; j++) {  // (s217 riku: j = the listed row, vis[j] = the Limit)
            bool ok = pl_state(vis[j]) == 0;
            const float on[4] = {1, 1, 1, 1}, off[4] = {0.55f, 0.55f, 0.55f, 1}, cur[4] = {1, 0.9f, 0.4f, 1};
            pl_text(&o, PL_LIMS[vis[j]].name, 176, 300.0f + 24.0f * (float)j, !ok ? off : j == M.sub_cursor ? cur : on);
            if (j == M.sub_cursor) pl_text(&o, ">", 158, 300.0f + 24.0f * (float)j, cur);
        }
    }
    if (prompt) {  // the reaction-command box look of sora_react.h: a Triangle + the command's name
        const float bc[4] = {0.05f, 0.05f, 0.08f, 0.7f};
        pl_quad_box(&o, 28, 318, 58 + 120, 30, bc);
        SoraHudQuad *q = &o.q[o.n++];
        memset(q, 0, sizeof *q);
        float cx = 50, cy = 333, r = 11;
        q->x[0] = q->x[1] = cx, q->y[0] = q->y[1] = cy - r;
        q->x[2] = cx - r, q->x[3] = cx + r, q->y[2] = q->y[3] = cy + r * 0.8f;
        const float tc[4] = {0.35f, 0.9f, 0.55f, 1};
        for (int k = 0; k < 4; k++) q->col[k] = cs_pack_(tc);
        q->tex = 2;
        const float w[4] = {1, 1, 1, 1};
        pl_text(&o, pl_cmd_name(pl_prompt_cmd()), 74, 324, w);
    }
    CAMP.quads(PLM.q, o.n);
    PLM.shown = true;
}

static bool pl_live_set(const char *key, int v) {  // s133 limits
    if (!strcmp(key, "limitparty")) {
        g_plimit = v != 0;
        if (!g_plimit) pl_cancel("limitparty 0");
        PLOG("[plimit] limitparty %d\n", g_plimit);
    } else if (!strcmp(key, "plbox")) g_plbox = v != 0, PLOG("[plimit] plbox %d (%s)\n", g_plbox, g_plbox ? "the old camp-font list box" : "the Limit list is a command-menu page");
    else if (!strcmp(key, "lpdiag")) g_pl_diag = v != 0, PLOG("[plimit] lpdiag %d\n", g_pl_diag);
    else if (!strcmp(key, "lpauto")) g_pl_auto = v != 0, PLOG("[plimit] lpauto %d (the prompts are pressed for you)\n", g_pl_auto);
    else if (!strcmp(key, "lpsteer")) g_pl_steer = v, PLOG("[plimit] lpsteer %d (0 old fixed move, 1 stick steers both spins, 2 stage 3 only)\n", g_pl_steer);
    else if (!strcmp(key, "lpfxfollow")) g_pl_fxfollow = v != 0, PLOG("[plimit] lpfxfollow %d (the stage effects follow Sora / the member; 0 = old)\n", g_pl_fxfollow);
    else if (!strcmp(key, "lprocket")) g_pl_rocket = v != 0, PLOG("[plimit] lprocket %d (the synthesized rocket before each volley comet, GUESS; 0 = old)\n", g_pl_rocket);
    else if (!strcmp(key, "lpks")) g_ks_on = v != 0, PLOG("[plimit] lpks %d (Knocksmash %s)\n", g_ks_on, g_ks_on ? "usable" : "greyed");
    else if (!strcmp(key, "lpksshield")) g_ks_shield = v != 0, PLOG("[plimit] lpksshield %d (Knocksmash's thrown shields + their hits)\n", g_ks_shield);
    else if (!strcmp(key, "lpksdive")) g_ks_dive = v != 0, PLOG("[plimit] lpksdive %d (Goofy's solo dive; 0 = the Limit ends after Cosmo Boost)\n", g_ks_dive);
    else if (!strcmp(key, "lpkssnd")) g_ks_snd = v != 0, PLOG("[plimit] lpkssnd %d (Knocksmash's voices / SEs)\n", g_ks_snd);
    else if (!strcmp(key, "lpksfx")) g_ks_fx = v != 0, PLOG("[plimit] lpksfx %d (the shields' effects)\n", g_ks_fx);
    else if (!strcmp(key, "lpdf")) g_df_on = v != 0, PLOG("[plimit] lpdf %d (Duck Flare %s)\n", g_df_on, g_df_on ? "usable" : "greyed");
    else if (!strcmp(key, "lpdffan")) g_df_fan = v != 0, PLOG("[plimit] lpdffan %d (Duck Flare's intro fan volley)\n", g_df_fan);
    else if (!strcmp(key, "lpdfhover")) g_df_hover = v != 0, PLOG("[plimit] lpdfhover %d (the hover flares)\n", g_df_hover);
    else if (!strcmp(key, "lpdfrocket")) g_df_rocket = v != 0, PLOG("[plimit] lpdfrocket %d (Rocket Flare)\n", g_df_rocket);
    else if (!strcmp(key, "lpdfmega")) g_df_mega = v != 0, PLOG("[plimit] lpdfmega %d (Megaduck Flare)\n", g_df_mega);
    else if (!strcmp(key, "lpdfsnd")) g_df_snd = v != 0, PLOG("[plimit] lpdfsnd %d (Duck Flare's voices / SEs)\n", g_df_snd);
    else if (!strcmp(key, "lpdffx")) g_df_fx = v != 0, PLOG("[plimit] lpdffx %d (the flares' effects)\n", g_df_fx);
    else if (!strcmp(key, "lpdfscale")) g_df_fxs = v < 10 ? 10 : v > 1000 ? 1000 : v, PLOG("[plimit] lpdfscale %d (flare effect size x%.2f)\n", g_df_fxs, g_df_fxs * 0.01f);
    else if (!strcmp(key, "lpdfe5")) g_df_e5 = v != 0, PLOG("[plimit] lpdfe5 %d (element 5 as a hit burst, GUESS)\n", g_df_e5);
    else if (!strcmp(key, "lpdftime")) g_df_time = v < 60 ? 60 : v > 3000 ? 3000 : v, PLOG("[plimit] lpdftime %d (Duck Flare's limit time, frames; KH2 300)\n", g_df_time);
    else if (!strcmp(key, "lpdfkb")) g_df_kb = v != 0, PLOG("[plimit] lpdfkb %d (Sora's Keyblade hidden while Donald rides)\n", g_df_kb);
    else if (!strcmp(key, "lpdfhx")) g_df_hand[0] = v, PLOG("[plimit] lpdfhx %d cm (Donald's seat, left)\n", v);
    else if (!strcmp(key, "lpdfhy")) g_df_hand[1] = v, PLOG("[plimit] lpdfhy %d cm (Donald's seat, up)\n", v);
    else if (!strcmp(key, "lpdfhz")) g_df_hand[2] = v, PLOG("[plimit] lpdfhz %d cm (Donald's seat, forward)\n", v);
    else if (!strcmp(key, "lpforce")) g_pl_force = v;  // ONE-SHOT: fires on the next Sora tick
    else return false;
    return true;
}

// (s68 drive) KH2 Drive Forms slice 2: Valor Form for Sora in BotW (DLL-only, hot reload). Design + data:
// D:/SoraBotW/recon/S64_HELPER_DRIVE_RECON.md sec 2-3 + 6 (slice 2) + "S68 helper VALOR BAKE" (data/forms/valor_*.bin).
// Needs the s68 exe (sora_draw_set_body, sora_hud_menu_top; cmd_s68.bin labels 8 Valor Form / 9 Revert): looked up by
// name, an older exe = the Drive page stays greyed (logged once).
//
// Drive: the top page's Drive -> the Drive page (Valor Form; greyed below 3 bars, while Goofy is KO'd, in the air, or
// without the exe calls) -> 3 whole bars spent (a full gauge loses its partial bar, re:0x3C2780) -> base Sora's
// CHANGEFORM (motion 52, slot 210 A120; pad dead) -> at FM.swap_at KH frames into it (GUESS 40 = the motion's "return
// 40"; live "set fmswap <f>"): full HP + full MP (KH2 re:0x3C17C0; the user's s68 rule), the body = Valor's draw actor,
// Valor's motion bake laid over base Sora's (same KH2 slot numbers: idle 0, run 8, combo 604..), Goofy absorbed (gone,
// plate hidden), Valor's entry (motion 53 -> slot 215 A121). The form gauge: (form level 1 - 1 + 3) bars x DriveTime 600
// KH frames, -1 per KH frame in battle (the party's battle flag), x DriveTimeRelax 0.5 outside (re:0x3A92C0). At 0, or
// the Revert command (top slot 4 = label 9 while in the form; A on it): base Sora back, Goofy placed beside him.
// While not driven (climb / swim / puppet) base Sora's body draws (the puppet retargets need his 240 bones) and the gauge
// holds; death / Game Over / a load = revert at once. No Magic in Valor (cmd DisableForm bit 1), no glide (KH2: Final's).
// Valor's own moveset (speeds, combo limits, two-blade hits, form EXP) = slice 3: until then base Sora's ACTION code
// runs Valor's motions with base Sora's hit windows.
// Live: form 1 (enter now, no cost) / form 0 (revert) | fmswap <KH frames> | fmtime <KH frames left>.
// (s72) a form table FM_FORMS: Valor (absorbs Goofy, no Magic, label 8) + Wisdom (P_EX100_MAGF bake, absorbs Donald,
// Magic allowed (cmd DisableForm bit 2 clear), label 10, 3 bars). Each form = its own draw actor (exe SORA_DRAW_ACTORS 6
// since s72; an older exe of 4 = Wisdom greyed, logged) + motion table (sora.c g_fmots_tab[g_fform]). The Drive page
// lists both (up / down). Wisdom's shot / momentum slide / entry fx / form EXP = s72 (c), not yet. Live: form 2 = Wisdom.
// (s89 b4-limit) Limit Form (KH2 FM, P_EX100_KH1F bake, objentry 0x95D NeoMoveset 66): cmd 0x2A1 Cost 4, label 11
// "Limit Form", absorbs NO ONE (PHH ABSORBS limit = (); sys.bar 20098 "Access Limit Form alone with the help of friends
// faraway": Donald + Goofy stay in the field), no Magic (Magic cmd DisableForm 0xFD4A bit 3 = FM form 3): its Magic slot
// is the "Limit" page (sora_limit.h). Gauge (LV - 1 + 3) bars (sora_limit_lv.h). Entry fx: P_EX100_KH1F.a.us is not in
// fx.bin yet (none plays; the entry SE does). Live: form 3 = Limit free, fmlimit 0 = Limit greyed / refused.
// (s89 b4-master) Master Form (P_EX100_TRIF bake, recon/S89_HELPER_MASTER.md): cost 4 (03system cmd 0xB @0x484), label 12,
// Magic allowed (DisableForm bit 4 clear), absorbs Donald AND Goofy (KH2 re:0x4005E0 via PHH DRIVE_NEEDS; live "set mboth 0"
// = Donald only), gauge (Master LV - 1 + 3) bars, entry fx master.* (fx_s89_master.bin rows). Live: form 4 = Master free (s89 trial numbering),
// fmmaster 0 = Master greyed / refused. Its moves: sora_master.h; its EXP / LV: sora_master_lv.h.
#pragma once

// (s146 outfit, sora_outfit.h) the draw actor a body is shown with: the normal one while the outfit is Normal
static int ofx_actor_base(void);
static int ofx_actor_form(int f);
static int ofx_peek(int o, int bi);  // (s146b) the loaded actor of (outfit, body) or 0, never loads
static int ofx_shown_limit(void);  // (s146b) the Limit body on screen (outfit or normal), never loads

typedef struct {
    const char *name, *model, *pose, *motion;
    int cost;      // cmd Cost, bars (03system.bin: Valor cmd 0x6 @0x3C4, Wisdom 0x7 @0x3F4)
    int label;     // cmd.bin label of its Drive page row
    int absorb;    // the party member it takes in (sora_party.h: 0 Donald, 1 Goofy; S64 recon 2; -1 = none, s89 Limit; FM_ABSORB_BOTH)
    bool no_magic; // cmd DisableForm bit (Valor bit 1)
    bool no_menu;  // (s89) never a Drive page row (Anti: KH2 only gives it by its random roll, re:0x421D60)
} FmForm;
#define FM_ABSORB_BOTH 2  // (s89: Master / Final / Anti take Donald AND Goofy, re:0x4005E0 DRIVE_NEEDS)
#define FM_DIR "D:/SoraBotW/data/forms/"
static const FmForm FM_FORMS[FM_NFORMS] = {
    [FORM_VALOR] = {"Valor", FM_DIR "valor_model.bin", FM_DIR "valor_pose.bin", FM_DIR "valor_motion.bin", 3, 8, 1, true},
    [FORM_WISDOM] = {"Wisdom", FM_DIR "wisdom_model.bin", FM_DIR "wisdom_pose.bin", FM_DIR "wisdom_motion.bin", 3, 10, 0, false},
    [FORM_LIMIT] = {"Limit", FM_DIR "limit_model.bin", FM_DIR "limit_pose.bin", FM_DIR "limit_motion.bin", 4, 11, -1, true},  // (s89: cmd 0x2A1 @0x7A14 Cost 4)
    [FORM_MASTER] = {"Master", FM_DIR "master_model.bin", FM_DIR "master_pose.bin", FM_DIR "master_motion.bin", 4, 12, FM_ABSORB_BOTH, false},  // (s89: cmd 0xB @0x484 Cost 4)
    // (s89 b4-final) Final: P_EX100_ULTF bake + floating blades W_EX010_ULTF_R / _L, cmd 0xC Cost 5 (03system @0x4B4),
    // label 13 "Final Form" (cmd.bin since s71), absorbs both, Magic allowed (Magic DisableForm 0xFD4A bit 5 clear)
    [FORM_FINAL] = {"Final", FM_DIR "final_model.bin", FM_DIR "final_pose.bin", FM_DIR "final_motion.bin", 5, 13, FM_ABSORB_BOTH, false},
    // (s89) Anti: P_EX100_HTLF (objentry 0x59, NeoMoveset 6; HTLF_BTL 0x934 is a type-4 AI copy), no Keyblade (the
    // weapon chain's bones kept, no mesh), cmd 0xD Cost 9 (@0x4E4: drive_spend clamps = the whole gauge, GUESS),
    // label 14 Antiform, absorbs both, no Magic (DisableForm bit 6), not on the Drive page (sora_anti.h rolls it)
    [FORM_ANTI] = {"Anti", FM_DIR "anti_model.bin", FM_DIR "anti_pose.bin", FM_DIR "anti_motion.bin", 9, 14, FM_ABSORB_BOTH, true, true},
    // (s109 roxas) NOT a Drive Form: Roxas' body / motions / pose, loaded and drawn through this table's machinery; entered by
    // sora_roxas.h (never by the Drive page: no_menu, cost / label unused), FM.st stays FM_BASE
    [FORM_ROXAS] = {"Roxas", FM_DIR "roxas_model.bin", FM_DIR "roxas_pose.bin", FM_DIR "roxas_motion.bin", 99, 8, -1, true, true},
    // (s110 b22-mickey) NOT a Drive Form either: KH2 Mickey's body for the boss rescue (sora_mickey.h), same machinery as Roxas
    [FORM_MICKEY] = {"Mickey", FM_DIR "mickey_model.bin", FM_DIR "mickey_pose.bin", FM_DIR "mickey_motion.bin", 99, 8, -1, true, true},
};
static const char *const FM_MEMBER[3] = {"Donald", "Goofy", "Donald + Goofy"};
#define FM_DRIVE_TIME 600.0f     // sstm DriveTime (@0x18404), KH frames per form bar
#define FM_RELAX 0.5f            // sstm DriveTimeRelax (@0x18408)
#define MOT_CHANGEFORM 52        // A120 (slot 210)
#define MOT_FORM_ENTRY 53        // A121 (Valor slot 215, Wisdom 215)

enum { FM_BASE, FM_CHANGE, FM_FORM };
static struct {
    int st;
    float clock;      // KH frames into CHANGEFORM
    float t, total;   // the form gauge, KH frames
    float swap_at;
    int actor[FM_NFORMS];  // each form's draw actor (-1 = failed / not loaded)
    bool gave_up[FM_NFORMS];
    bool tried, away; // away: not driven, base body shown meanwhile
    void (*set_body)(int);
    int (*body)(void);
    void (*menu_top)(int, int);
} FM = {.swap_at = 40, .actor = {-1, -1, -1}};

static bool fm_in_form(void) { return FM.st != FM_BASE; }
static int g_fm_limit_on = 1;  // (s89) live "set fmlimit 0" = Limit Form greyed on the Drive page / refused
static const char *fm_member_name(int f) {  // (s89 trial: for the logs)
    int a = FM_FORMS[f].absorb;
    return a < 0 ? "nobody" : a == FM_ABSORB_BOTH ? (f == FORM_MASTER && !g_fm_master_both ? "Donald" : "Donald + Goofy") : FM_MEMBER[a];
}
static int g_fmfinal_on = 1;  // (s89 b4-final) live "set fmfinal 0": Final greyed on the Drive page (and reverted if in it)
static int g_fin_mom = 1;     // (s89 b4-final) Final's hover slide (sora.c ACTION physics); live "set fmom 0" = instant
// (s89 b4-final) who may glide: base Sora, or Final Form (KH2: Final's growth ability is Glide; other forms have none)
static bool fm_glide_ok(void) { return !IN_GUEST && (!fm_in_form() || (FM.st == FM_FORM && IN_FINAL && g_fmfinal_on)); }
static int g_wis_mom = 1;  // (s72) Wisdom's momentum run (sora.c ACTION physics); live "set wmom 0" = instant like base Sora

static void fm_init(void) {
    if (FM.tried) return;
    FM.tried = true;
    HMODULE exe = GetModuleHandleA(NULL);
    FM.set_body = (void (*)(int))(void *)GetProcAddress(exe, "sora_draw_set_body");
    FM.body = (int (*)(void))(void *)GetProcAddress(exe, "sora_draw_body");
    FM.menu_top = (void (*)(int, int))(void *)GetProcAddress(exe, "sora_hud_menu_top");
    if (FM.body && FM.set_body && FM.body() != 0) FM.set_body(0), LOG("[form] hot reload: body back to base Sora\n");
    if (FM.menu_top) FM.menu_top(3, 3);
    for (int f = 0; f < FM_NFORMS; f++) {
        g_nfmots_tab[f] = mot_file_load(ffps_path(FM_FORMS[f].motion), g_fmots_tab[f], (int)(sizeof g_fmots_tab[f] / sizeof *g_fmots_tab[f]));
        LOG("[form] %s: %d motion slots\n", FM_FORMS[f].name, g_nfmots_tab[f]);
    }
    LOG("[form] exe calls set_body %s, menu_top %s\n", FM.set_body ? "ok" : "MISSING", FM.menu_top ? "ok" : "MISSING");
}
// the forms' draw actors: loaded once Sora's own bake is in (sora_draw_actor_load is -1 before it), retried every 300
// calls; a form that fails after another one loaded = the exe's actor table is full (s72: an exe older than 6) = given up
// (s109 review) Roxas' actor only once a Roxas body is wanted (sora_roxas.h sets g_fm_rox_load + one immediate try): with
// "char 2" (the default) the exe's 10th actor slot stays free (ffps' second Final pose pair needs it) and his 36 MB pose
// bake is never read
static bool g_fm_rox_load, g_fm_mk_load, g_fm_load_now;  // (s110 b22-mickey: Mickey's actor likewise only once a rescue wants him)
static void fm_load_actor(void) {
    static int (*load)(const char *, const char *);
    static int wait;
    if (!FM.set_body || (wait-- > 0 && !g_fm_load_now)) return;
    g_fm_load_now = false;
    if (!load) load = (int (*)(const char *, const char *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_actor_load");
    if (!load) return;
    wait = 300;
    for (int f = 0; f < FM_NFORMS; f++) {
        if (FM.actor[f] > 0 || FM.gave_up[f] || (f == FORM_ROXAS && !g_fm_rox_load) || (f == FORM_MICKEY && !g_fm_mk_load)) continue;
        FM.actor[f] = load(FM_FORMS[f].model, ffps_path(FM_FORMS[f].pose));
        bool other = false;
        for (int g = 0; g < FM_NFORMS; g++) other |= g != f && FM.actor[g] > 0;
        if (FM.actor[f] <= 0 && other) FM.gave_up[f] = true;
        if (FM.actor[f] > 0 || FM.gave_up[f])
            LOG("[form] %s draw actor: %d%s\n", FM_FORMS[f].name, FM.actor[f], FM.gave_up[f] ? " (table full? exe before s72)" : "");
    }
}

// the member a form absorbs, for the Drive page: needed and not KO'd (a party that is off / not loaded does not block it)
// (s89) FM_ABSORB_BOTH takes in both members (Master: live mboth 0 = Donald only)
static bool fm_takes(int f, int mb) {  // (s89 trial: absorb -1 none / 0 / 1 / FM_ABSORB_BOTH; Master + mboth 0 = Donald only)
    int a = FM_FORMS[f].absorb;
    return a == FM_ABSORB_BOTH ? f != FORM_MASTER || g_fm_master_both || mb == 0 : a == mb;
}
static bool fm_member_ok(int f) {
    for (int mb = 0; mb < 2; mb++) {  // (s89: none for Limit, both for Master / Final / Anti)
        if (!fm_takes(f, mb) || !P.on || g_party_id[mb] <= 0 || !P.ticks) continue;
        if (P.m[mb].hp <= 0 || g_party_absorbed[mb]) return false;
    }
    return true;
}
static bool fm_ready(int f) {  // (s89 kb: + the unlock gate, sora_unlock.h; s89 master / final: live fmmaster 0 / fmfinal 0)
    fm_init(); fm_load_actor();
    if (f == FORM_MASTER && !ml_form_on()) return false;
    if (f == FORM_FINAL && !g_fmfinal_on) return false;  // (s89 b4-final: live fmfinal 0)
    return FM.actor[f] > 0 && FM.set_body && g_nfmots_tab[f] > 0 && ul_form_unlocked(FM_FORMS[f].name);
}
static int fm_rows(int rf[FM_NFORMS]) {  // (s89 kb) the Drive page rows: the unlocked menu forms, FM_FORMS order -> count (>= 1)
    int n = 0;
    for (int f = 0; f < FM_NFORMS; f++)
        if (!FM_FORMS[f].no_menu && ul_form_unlocked(FM_FORMS[f].name)) rf[n++] = f;  // (s89 anti: Anti is never a row)
    if (!n) rf[n++] = 0;
    return n;
}
// KH2 entry state of a form's Drive page row / of the top page's Drive: 0 usable, 5 greyed
static int fm_state(int f) {
    if (f < 0 || f >= FM_NFORMS) return 5;
    if (f == FORM_LIMIT && !g_fm_limit_on) return 5;  // (s89 live fmlimit 0)
    return !fm_in_form() && !sm_out() && fm_ready(f) && DG.bars >= FM_FORMS[f].cost && fm_member_ok(f) && !ACT_AIR(S.act) ? 0 : 5;  // (s136 review M1: no form while a summon is out, KH2 drive state 3)
}
static int fm_label(int f) { return f >= 0 && f < FM_NFORMS ? FM_FORMS[f].label : 8; }
static int fm_cost(int f) { return f >= 0 && f < FM_NFORMS ? FM_FORMS[f].cost : 3; }
static bool fm_no_magic(void) { return IN_GUEST || (fm_in_form() && (FM.st == FM_CHANGE || FM_FORMS[g_fform].no_magic)); }
static bool anti_revert_ok(void);  // (s89 sora_anti.h: KH2 greys Anti's Revert in battle, re:0x3F36C0)
static int fm_top_state(void) {
    if (IN_GUEST) return 5;  // (s109 roxas / s110 mickey: no Drive)
    if (sm_out()) return 5;  // (s136 review M1: a summon is out = KH2 drive state 3, no Drive Form)
    if (FM.st == FM_FORM) return g_fform == FORM_ANTI && !anti_revert_ok() ? 5 : 0;  // (Revert)
    bool any = false;
    for (int f = 0; f < FM_NFORMS; f++) any |= f != FORM_ROXAS && f != FORM_MICKEY && fm_ready(f);
    return FM.st == FM_BASE && any ? 0 : 5;
}

static void fm_hud_menu(void) {
    if (FM.menu_top) FM.menu_top(3, FM.st == FM_FORM ? 9 : 3);
    if (FM.menu_top) FM.menu_top(1, FM.st == FM_FORM && g_fform == FORM_LIMIT ? lm_top_label() : 1);  // (s89: Magic -> "Limit")
}

// the form gauge on KH2's Drive plate (slice 2: the normal plate; the form groups 0x31-0x35 are not baked): bars left
// + the part bar, max = the form's bars. False = not in a form (the Drive gauge shows).
static bool fm_gauge(int *bars, int *fill, int *max) {
    if (FM.st != FM_FORM) return sm_gauge(bars, fill, max);  // (s136 summon: the summon gauge shows on the plate while Chicken Little is out)
    float t = FM.t < 0 ? 0 : FM.t;
    *max = (int)(FM.total / FM_DRIVE_TIME + 0.5f);
    *bars = (int)(t / FM_DRIVE_TIME);
    *fill = (int)((t - *bars * FM_DRIVE_TIME) * 100 / FM_DRIVE_TIME);
    if (*bars >= *max) *bars = *max, *fill = 100;
    return true;
}

static void fm_absorb(int f, bool absorbed) {  // (s72: the form's member, Goofy for Valor / Donald for Wisdom; s89 Master both)
    for (int mb = 0; mb < 2; mb++) {
        // (a revert gives back whoever is absorbed: mboth may have changed while in the form)
        if (absorbed ? !fm_takes(f, mb) : !g_party_absorbed[mb]) continue;
        if (g_party_absorbed[mb] == absorbed) continue;
        g_party_absorbed[mb] = absorbed;
        if (!absorbed) P.m[mb].placed = false;  // (party_core_tick places him beside Sora next tick)
        else if (g_party_id[mb] > 0 && PD.off) PD.off(g_party_id[mb]);
        LOG("[form] %s %s\n", FM_MEMBER[mb], absorbed ? "absorbed (hidden)" : "back beside Sora");
    }
}

// (s70) KH2's Drive effects (fx.bin s70 rows, S64_HELPER_DRIVE_RECON.md 3): each element bound to its KH2 bone (the
// recording's bone; -1 = the actor root) and moved there every frame from the playing motion's pose. CHANGEFORM frame 0:
// sora.1 sphere + sora.2..13 limb glows (600 f loops, gone at the swap = the actor is replaced); Valor's entry:
// valor.0..12 + 100..102 (its loops valor.1 / 5 / 6 fade when the entry motion ends: GUESS); revert: sora.15..27.
// Voice: vtbl 8 at CHANGEFORM frame 0 (hb0_sora #25 / #32 / #33). Live: set fmfx 0/1.
typedef struct { const char *name; int bone; } FmFxDef;
static const FmFxDef FX_CHANGE[] = {{"drive_sphere", -1}, {"drive_glow_2", 67}, {"drive_glow_3", 2}, {"drive_glow_4", 170},
    {"drive_glow_5", 78}, {"drive_glow_6", 177}, {"drive_glow_7", 85}, {"drive_glow_8", 40}, {"drive_glow_9", 11},
    {"drive_glow_10", 41}, {"drive_glow_11", 12}, {"drive_glow_12", 44}, {"drive_glow_13", 15}};
static const FmFxDef FX_ENTER[] = {{"valor_enter_0", -1}, {"valor_enter_1", 67}, {"valor_enter_2", 2}, {"valor_enter_3", 170},
    {"valor_enter_4", 78}, {"valor_enter_5", 177}, {"valor_enter_6", 85}, {"valor_enter_7", 40}, {"valor_enter_8", 11},
    {"valor_enter_9", 41}, {"valor_enter_10", 12}, {"valor_enter_11", 44}, {"valor_enter_12", 15}, {"valor_enter_100", 67},
    {"valor_enter_101", 67}, {"valor_enter_102", 67}};
// (s73) Wisdom's entry (slot 215 triggers 1..12 + 100..102 = wisdom.0..12 + 100..102, the recording's bones)
static const FmFxDef FX_ENTER_W[] = {{"wisdom_enter_0", -1}, {"wisdom_enter_1", 67}, {"wisdom_enter_2", 2},
    {"wisdom_enter_3", 170}, {"wisdom_enter_4", 78}, {"wisdom_enter_5", 177}, {"wisdom_enter_6", 85}, {"wisdom_enter_7", 11},
    {"wisdom_enter_8", 40}, {"wisdom_enter_9", 41}, {"wisdom_enter_10", 12}, {"wisdom_enter_11", 44}, {"wisdom_enter_12", 15},
    {"wisdom_enter_100", -1}, {"wisdom_enter_101", 67}, {"wisdom_enter_102", 67}};
// (s89 b4-master) Master's entry (slot 215 triggers 1..12 + 100..102 = master.0..12 + 100..102; the recording's bones;
// .100 / .102 are bound to 0x4005 / 0x4006 = GUESS the two Keyblade hands 178 / 86)
static const FmFxDef FX_ENTER_M[] = {{"master_enter_0", -1}, {"master_enter_1", 67}, {"master_enter_2", 2},
    {"master_enter_3", 170}, {"master_enter_4", 78}, {"master_enter_5", 177}, {"master_enter_6", 85}, {"master_enter_7", 40},
    {"master_enter_8", 11}, {"master_enter_9", 41}, {"master_enter_10", 12}, {"master_enter_11", 44}, {"master_enter_12", 15},
    {"master_enter_100", 178}, {"master_enter_101", 85}, {"master_enter_102", 86}};
// (s92 b6-formfx) the entry sets of Limit / Final / Anti: every form's slot 215 (A121) has the same 15 frame-0 fx triggers as Valor's
// (motion_triggers.kh.json actors 12 / 14 / 15: [1, 0] x 15) = elements 0..12 + 100..102 of P_EX100_KH1F / _ULTF / _HTLF.a.us (PHH
// recording limit.* / final.* / anti.*), the recording's bones (0x4000 -> -1 the root; 0x4005 / 0x4006 -> the Keyblade hands 178 / 86 =
// GUESS, as Master). fx.bin s92 rows limit_enter_N / final_enter_N / anti_enter_N (600 f loops capped 240). Live: set formfx 0 = none (s89).
static const FmFxDef FX_ENTER_L[] = {{"limit_enter_0", -1}, {"limit_enter_1", 67}, {"limit_enter_2", 2},
    {"limit_enter_3", 170}, {"limit_enter_4", 78}, {"limit_enter_5", 177}, {"limit_enter_6", 85},
    {"limit_enter_7", 11}, {"limit_enter_8", 40}, {"limit_enter_9", 41}, {"limit_enter_10", 12},
    {"limit_enter_11", 44}, {"limit_enter_12", 15}, {"limit_enter_100", -1}, {"limit_enter_101", 67},
    {"limit_enter_102", 67}};
static const FmFxDef FX_ENTER_F[] = {{"final_enter_0", -1}, {"final_enter_1", 67}, {"final_enter_2", 2},
    {"final_enter_3", 170}, {"final_enter_4", 78}, {"final_enter_5", 177}, {"final_enter_6", 85},
    {"final_enter_7", 40}, {"final_enter_8", 11}, {"final_enter_9", 41}, {"final_enter_10", 12},
    {"final_enter_11", 44}, {"final_enter_12", 15}, {"final_enter_100", -1}, {"final_enter_101", 178},
    {"final_enter_102", 86}};
static const FmFxDef FX_ENTER_A[] = {{"anti_enter_0", -1}, {"anti_enter_1", 67}, {"anti_enter_2", 2},
    {"anti_enter_3", 170}, {"anti_enter_4", 78}, {"anti_enter_5", 177}, {"anti_enter_6", 85}, {"anti_enter_7", 40},
    {"anti_enter_8", 11}, {"anti_enter_9", 41}, {"anti_enter_10", 12}, {"anti_enter_11", 44}, {"anti_enter_12", 15},
    {"anti_enter_100", -1}, {"anti_enter_101", 67}, {"anti_enter_102", 67}};
static const FmFxDef FX_REVERT[] = {{"drive_revert_15", 67}, {"drive_revert_16", 67}, {"drive_revert_17", 2},
    {"drive_revert_18", 170}, {"drive_revert_19", 78}, {"drive_revert_20", 177}, {"drive_revert_21", 85},
    {"drive_revert_22", 40}, {"drive_revert_23", 11}, {"drive_revert_24", 41}, {"drive_revert_25", 12},
    {"drive_revert_26", 44}, {"drive_revert_27", 15}};
#define FX_LIVE_N 48
static struct { int h, bone, set; } g_fmfx[FX_LIVE_N];
static int g_nfmfx, g_fmfx_on = 1;
// (s87c drive sfx) KH2's SEs ride the effect elements' own +0x18 SE (P_EX100.a.us sora.1 = se000#114 the Drive sphere,
// sora.15 = #115 the revert; P_EX100_BTLF / _MAGF .0 = se999#1 the form's entry; json "effects", VERIFIED ids; that the
// effect player plays an element's SE when it starts it = GUESS, same as system.15 / .16). Live: set fmse 0 = silent.
static int g_fmse_on = 1;
static int g_fmse_vol = 60;  // (s89b user: drive SEs "slightly loud") % volume; live: set fmsevol N (100 = the s87c level)
static int g_fmvoice_once = 1;  // (s88) the entry voice once, not twice (see fm_drive); live: set fmvoice
static int g_fmse_sphere = -1;  // (s87c review) the sphere's handle: a change cut short / a death or load stops it
static int fm_se(const char *name, const char *what) {
    if (!g_fmse_on) return -1;
    int h = sora_sfx_event(name, g_fmse_vol / 100.0f);
    LOG("[form] %s SE '%s' -> %d%s\n", what, name, h, h < 0 ? " (not in sfx.bin? rebake sfxpack)" : "");
    return h;
}
enum { FXS_CHANGE = 1, FXS_ENTER, FXS_REVERT };
static void fm_fx_end(int set, float fade) {  // set 0 = all
    for (int i = 0; i < g_nfmfx; i++)
        if (g_fmfx[i].h >= 0 && (!set || g_fmfx[i].set == set)) {
            if (fade > 0) sora_fx_fadeout(g_fmfx[i].h, fade); else sora_fx_kill(g_fmfx[i].h);
            g_fmfx[i].set = 0;  // (fading: no longer moved by set, still moved until gone)
        }
}
static int g_fmfx_pending;  // a set to spawn at the next fm_fx_tick (it has the feet / pose)
static void fm_fx_start(int set) { if (g_fmfx_on) g_fmfx_pending = set; }
static float g_fmfx_pose[POSE_BUF_BONES * 12];
static void fm_fx_bone_world(int bone, bool pose_ok, const float feet[3], float face, float to_u, float out[3]) {
    int nb = sora_draw_bone_count();
    if (bone < 0 || !pose_ok || bone >= nb) { memcpy(out, feet, 12); return; }
    const float *q = g_fmfx_pose + bone * 12 + 9;
    float s = sinf(face), c = cosf(face);
    out[0] = feet[0] + (c * q[0] + s * q[2]) * to_u, out[1] = feet[1] + q[1] * to_u, out[2] = feet[2] + (-s * q[0] + c * q[2]) * to_u;
}
// every frame (sora.c, next to vl_show)
static void fm_fx_tick(const float feet[3], float face, float to_u) {
    bool any = g_fmfx_pending;
    for (int i = 0; i < g_nfmfx && !any; i++) any = g_fmfx[i].h >= 0;
    if (!any) { g_nfmfx = 0; return; }
    bool pose_ok = S.mot && sora_draw_slot_pose(S.mot->slot, S.clock, g_fmfx_pose);
    if (g_fmfx_pending) {
        int set = g_fmfx_pending;
        g_fmfx_pending = 0;
        const FmFxDef *ent = g_fform == FORM_WISDOM ? FX_ENTER_W : g_fform == FORM_MASTER ? FX_ENTER_M : g_fform == FORM_LIMIT ? FX_ENTER_L
                           : g_fform == FORM_FINAL ? FX_ENTER_F : g_fform == FORM_ANTI ? FX_ENTER_A : FX_ENTER;  // (s89 Master, s92 Limit / Final / Anti)
        int nent = g_fform == FORM_WISDOM ? (int)(sizeof FX_ENTER_W / sizeof *FX_ENTER_W) : g_fform == FORM_MASTER ? (int)(sizeof FX_ENTER_M / sizeof *FX_ENTER_M)
                 : g_fform == FORM_LIMIT ? (int)(sizeof FX_ENTER_L / sizeof *FX_ENTER_L) : g_fform == FORM_FINAL ? (int)(sizeof FX_ENTER_F / sizeof *FX_ENTER_F)
                 : g_fform == FORM_ANTI ? (int)(sizeof FX_ENTER_A / sizeof *FX_ENTER_A) : (int)(sizeof FX_ENTER / sizeof *FX_ENTER);
        const FmFxDef *d = set == FXS_CHANGE ? FX_CHANGE : set == FXS_ENTER ? ent : FX_REVERT;
        int n = set == FXS_CHANGE ? (int)(sizeof FX_CHANGE / sizeof *FX_CHANGE) : set == FXS_ENTER ? nent : (int)(sizeof FX_REVERT / sizeof *FX_REVERT);
        int ok = 0;
        for (int k = 0; k < n; k++) {
            int j = 0;
            while (j < g_nfmfx && g_fmfx[j].h >= 0) j++;
            if (j >= FX_LIVE_N) break;
            float p[3];
            fm_fx_bone_world(d[k].bone, pose_ok, feet, face, to_u, p);
            int h = sora_fx_event(d[k].name, p, face, to_u);
            if (h < 0) continue;
            g_fmfx[j].h = h, g_fmfx[j].bone = d[k].bone, g_fmfx[j].set = set, ok++;
            if (j == g_nfmfx) g_nfmfx++;
        }
        LOG("[form] fx set %d: %d of %d elements spawned (pose %s)%s\n", set, ok, n, pose_ok ? "ok" : "none",
            ok ? "" : " - fx.bin without the s70 rows? (restart)");
    }
    if (FM.st == FM_FORM && S.motion != MOT_FORM_ENTRY) fm_fx_end(FXS_ENTER, 15);  // (GUESS: Valor's loops end with A121)
    for (int i = 0; i < g_nfmfx; i++) {
        if (g_fmfx[i].h < 0) continue;
        if (!sora_fx_alive(g_fmfx[i].h)) { g_fmfx[i].h = -1; continue; }
        float p[3];
        fm_fx_bone_world(g_fmfx[i].bone, pose_ok, feet, face, to_u, p);
        sora_fx_move(g_fmfx[i].h, p, face);
    }
}

// (s71) KH2's Drive screen dim (re:0x40D000; PHH DRIVE_DIM_IN 12 / out 15): in over 12 KH frames from the Drive command,
// held through CHANGEFORM, out over 15 from the swap (and on a revert). Peak 0x30 / 0x80 read as the black quad's alpha
// (GUESS: could be the brightness left; live "set fmdim <pct>", 0 = off). The exe's sora_hud_dim (s71 exe; older = none).
static struct { float a; int dir; bool tried; void (*fn)(float); } FD;
static int g_fmdim_pct = 0;  // (s71 user: looks better without it; 38 = the KH2 guess)
static void fm_dim_tick(float kh) {
    if (!FD.tried) {
        FD.tried = true, FD.fn = (void (*)(float))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_dim");
        LOG("[form] dim: sora_hud_dim %s\n", FD.fn ? "found" : "not in this exe");
    }
    float peak = g_fmdim_pct / 100.0f;
    if (FD.dir > 0) { if ((FD.a += kh * peak / 12) >= peak) FD.a = peak; }
    else if (FD.dir < 0) { if ((FD.a -= kh * peak / 15) <= 0) FD.a = 0, FD.dir = 0; }
    if (FD.a > 0 && FD.fn) FD.fn(FD.a);
}

// (s71) the swap blend: base Sora's CHANGEFORM pose at the swap (sampled in fm_enter BEFORE set_body: slot 210 is in
// base Sora's bake) lerps into Valor's pose over g_fsw_len KH frames (smoothstep), bones 0..239 (Valor = Sora 228 +
// Keyblade 12 + the second blade 240..: those draw as Valor's). KH2 itself cuts under the sphere; the blend is ours
// (user s71). Live "set fmblend <KH frames>" (0 = the cut).
static float g_fsw_from[POSE_BUF_BONES * 12], g_fsw_pose[POSE_BUF_BONES * 12];
static float g_fsw_t = -1, g_fsw_len = 10;
static bool fm_swap_draw(float kh, float scale) {  // sora.c's pose drawer: true = drawn here this frame
    if (g_fsw_t < 0 || !S.mot) return false;
    g_fsw_t += kh;
    if (g_fsw_t >= g_fsw_len || !sora_draw_slot_pose(S.mot->slot, S.clock, g_fsw_pose)) { g_fsw_t = -1; return false; }
    float u = g_fsw_t / g_fsw_len, wgt = u * u * (3 - 2 * u);
    int nb = sora_draw_bone_count();
    if (nb > SORA_RAG_BONES) nb = SORA_RAG_BONES;
    for (int k = 0; k < nb; k++) {
        float *d = g_fsw_pose + k * 12, *a = g_fsw_from + k * 12, len[3];
        for (int r = 0; r < 3; r++) {
            float la = sqrtf(v3dot(a + 3 * r, a + 3 * r)), lb = sqrtf(v3dot(d + 3 * r, d + 3 * r));
            len[r] = la + (lb - la) * wgt;
        }
        for (int j = 0; j < 12; j++) d[j] = a[j] + (d[j] - a[j]) * wgt;
        ortho_rows(d, len);
    }
    sora_draw_world(g_fsw_pose, S.face, scale);
    return true;
}

static void fm_revert(const char *why) {
    if (FM.st == FM_BASE) return;
    bool was_form = FM.st == FM_FORM;
    FM.st = FM_BASE, FM.away = false;
    lm_abort(why);  // (s89: a running Limit command + its flights end with the form)
    g_fmots_on = false;
    if (FM.set_body) FM.set_body(ofx_actor_base());  // (s146 outfit: 0 while Normal)
    fm_absorb(g_fform, false);
    fm_fx_end(0, 10);
    g_fsw_t = -1;  // (s71 swap blend)
    if (FD.a > 0) FD.dir = -1;  // (s71 dim)
    if (was_form && strcmp(why, "dead / loading")) fm_fx_start(FXS_REVERT), fm_se("drive_revert", "revert");  // (s70; s87c drive sfx: sora.15's SE)
    else sora_sfx_stop(g_fmse_sphere);  // (s87c review: 2.7 s sphere SE; handles are serial-checked, an ended one is a no-op)
    g_fmse_sphere = -1;
    if (S.special == SP_DRIVE || (was_form && S.special && !ACT_AIR(S.act))) S.special = SP_NONE, S.motion = -1;
    S.mot = S.motion >= 0 ? mot_of(S.motion) : NULL;  // (S.mot may point at a Valor slot)
    fm_hud_menu();
    LOG("[form] revert from %s (%s), %.0f KH frames were left; Drive %d bars + %d\n", FM_FORMS[g_fform].name, why, FM.t,
        DG.bars, DG.fill);
    if (g_fform == FORM_MASTER) LOG("[master] DRIVE OUT (%s): Master LV%d EXP %u\n", why, ml_lv(), ML.exp);  // (s89)
}

static int anti_choice(int f, bool free_cost);  // (s89 sora_anti.h)
static float anti_gauge_bars(void);
static void anti_on_enter(void);
static void anti_life_tick(void);
static bool anti_live_set(const char *key, int v);
static bool rox_live_set(const char *key, int v);  // (s109 sora_roxas.h)
// The Drive command (the Drive page's A on form <f>): spend, CHANGEFORM. free_cost = the live "form 1" / "form 2".
static bool fm_drive(int f, bool free_cost) {
    if (f < 0 || f >= FM_NFORMS || f == FORM_ROXAS || f == FORM_MICKEY || IN_GUEST) return false;  // (s109 roxas: no Drive Forms as Roxas)
    if (!free_cost && fm_state(f) != 0) {
        sora_sfx_event("error", 1);
        LOG("[form] %s Form greyed (bars %d, %s ok %d, air %d, ready %d)\n", FM_FORMS[f].name, DG.bars,
            fm_member_name(f), fm_member_ok(f), ACT_AIR(S.act), fm_ready(f));
        return false;
    }
    if (f == FORM_LIMIT && !g_fm_limit_on) { LOG("[form] Limit Form is off (set fmlimit 1)\n"); return false; }  // (s89: free too)
    if (!fm_ready(f) || fm_in_form()) return false;
    f = anti_choice(f, free_cost);  // (s89 anti: KH2's form_choice re:0x421D60 - the Anti points + roll; may give Anti)
    g_fform = f;  // (s72: before anything reads the form's tables)
    if (!free_cost) {
        int b0 = DG.bars;
        bool f0 = dg_full();
        if (f0) DG.fill = 0;  // (re:0x3C2780: the partial bar of a full gauge is lost)
        DG.bars -= FM_FORMS[f].cost;
        if (DG.bars < 0) DG.bars = 0, DG.fill = 0;  // (s89: Anti's Cost 9 > the bars: the whole gauge, GUESS on KH2's clamp)
        char why[32];  // (s89: "drive <form>")
        snprintf(why, sizeof why, "drive %s", FM_FORMS[f].name);
        dg_changed(b0, f0, why);
    }
    FM.st = FM_CHANGE, FM.clock = 0;
    FD.dir = 1;  // (s71 dim)
    if (mot_of(MOT_CHANGEFORM)) play(MOT_CHANGEFORM), S.special = SP_DRIVE, S.next = 0;
    g_fmse_sphere = fm_se("drive_sphere", "change");  // (s87c drive sfx: sora.1's SE, the sphere; the old "drive" name never existed)
    {  // (s70) CHANGEFORM frame 0: voice vtbl 8 + the sphere / limb glows
        // (s88) play(MOT_CHANGEFORM) above already fired the motion's frame-0 trigger (sfx.bin: slot 210 frame 0, kind 1 =
        // voice vtbl 8): this second sora_sfx_voice(8) had the same priority, so it STOPPED the first line (cut off) and
        // started another = the "plays twice" bug. Only call it when the motion (and so its trigger) is missing.
        // Live: set fmvoice 0 = the old double call.
        // (s88 review) and when the motion's start fired nothing (g_play_trig 0: an sfx.bin without that row, or the
        // trigger's voice roll came out silent) - so the fix never leaves the change mute
        int v = !g_fmvoice_once || !mot_of(MOT_CHANGEFORM) || g_play_trig <= 0 ? sora_sfx_voice(8) : -2;
        fm_fx_start(FXS_CHANGE);
        LOG("[form] change voice vtbl 8 -> %s%s\n", v == -2 ? "the motion's frame-0 trigger (once)" : v < 0 ? "none" : "played",
            v == -1 ? " (not in sfx.bin?)" : "");
    }
    M.dead = 8, M.sub = -1, M.cursor = 0, M.sub_cursor = 0;
    LOG("[form] DRIVE %s: CHANGEFORM motion %d slot %d, swap at %.0f KH frames\n", FM_FORMS[f].name, MOT_CHANGEFORM,
        S.mot ? S.mot->slot : -1, FM.swap_at);
    return true;
}

// (s89 trial) sora_drive.h dg_max: +1 Drive bar per form level gained (user decision; Valor .. Final LV2..7)
static int fm_lv_bonus(void) {
    (void)vl_exp_get(), (void)wl_exp_get(), (void)ll_exp_get(), (void)ml_exp_get(), (void)fl_exp_get();  // (their env loads)
    return (vl_lv() - 1) + (wl_lv() - 1) + (ll_lv() - 1) + (ml_lv() - 1) + (fl_lv() - 1);
}
// (s89 trial) a form's own gauge: (its LV - 1 + 3) bars (re:0x3C17C0)
static float fm_form_bars(int f) {
    return f == FORM_VALOR ? vl_gauge_bars() : f == FORM_LIMIT ? ll_gauge_bars() : f == FORM_MASTER ? ml_gauge_bars()
         : f == FORM_FINAL ? fl_gauge_bars() : f == FORM_ANTI ? anti_gauge_bars() : wl_gauge_bars();
}
static void fm_enter(void) {
    FM.st = FM_FORM;
    // (LV - 1 + 3) bars (re:0x3C17C0; s70 Valor LV; s75 Wisdom LV)
    FM.total = FM.t = fm_form_bars(g_fform) * FM_DRIVE_TIME;  // (LV - 1 + 3) bars per form (s75 Wisdom LV; s89 Limit / Master LV)
    int life = link_heal(9999);  // (full: up to BotW's max hearts, s85f)
    MP.charge = MP.charge_total = 0, MP.mp = MP.mp_max;
    g_fsw_t = g_fsw_len > 0 && S.mot && sora_draw_bone_count() == SORA_RAG_BONES &&
              sora_draw_slot_pose(S.mot->slot, S.clock, g_fsw_from) ? 0 : -1;  // (s71: before the body changes)
    g_fmots_on = true;
    FM.set_body(ofx_actor_form(g_fform));  // (s146 outfit: FM.actor[g_fform] while Normal)
    fm_absorb(g_fform, true);
    S.mot = S.motion >= 0 ? mot_of(S.motion) : NULL;
    fm_fx_end(FXS_CHANGE, 8);
    // (s92 b6-formfx) Limit / Final / Anti now have KH2's own entry fx too (FX_ENTER_L / _F / _A, fx.bin s92 rows) and Anti the entry SE
    // (anti.0 = se999 #1 like every form, + anti.100's se000 #132); live "set formfx 0" = the s89/s91 way (no entry fx for those three, no Anti SE).
    if (g_formfx_on || (g_fform != FORM_ANTI && g_fform != FORM_LIMIT && g_fform != FORM_FINAL)) fm_fx_start(FXS_ENTER);  // (s73: Wisdom's = FX_ENTER_W, s89 Master's)
    if (g_formfx_on || g_fform != FORM_ANTI) {
        fm_se("wisdom_enter", "entry");  // (s87c drive sfx: valor.0 / wisdom.0 / .. = se999#1, the same sound; packed since s75)
        if (g_formfx_on && g_fform == FORM_ANTI) fm_se("anti_enter_100", "entry 2");  // (anti.100's SE; not in an older sfx.bin = logged, silent)
    }
    FD.dir = -1;  // (s71 dim)  // (s70: the old actor's glows go, Valor's come)
    if (mot_of(MOT_FORM_ENTRY)) play(MOT_FORM_ENTRY), S.special = SP_DRIVE, S.next = 0;
    else S.special = SP_NONE, S.motion = -1;
    fm_hud_menu();
    if (g_fform == FORM_ANTI) anti_on_enter();  // (s89: no-heal / damage watch starts from this life)
    LOG("[form] in %s Form: life %d, MP %.0f, gauge %.0f KH frames, entry slot %d\n", FM_FORMS[g_fform].name, life, MP.mp, FM.t,
        S.mot ? S.mot->slot : -1);
    if (g_fform == FORM_MASTER)  // (s89)
        LOG("[master] DRIVE IN: Master LV%d (EXP %u), %.0f bars, combo air %d, Aerial Dodge LV%d, absorbs %s\n", ml_lv(), ML.exp,
            FM.total / FM_DRIVE_TIME, F_COMBO_AIR, ml_dodge_lv(), g_fm_master_both ? "Donald + Goofy" : "Donald");
    if (g_fform == FORM_FINAL)  // (s89 b4-final)
        LOG("[final] in: LV%d (EXP %u), gauge %.0f bars, Donald + Goofy absorbed, Glide LV%d, combo %d/%d, run %.0f jump %.0f\n",
            fl_lv(), FL.exp, FM.total / FM_DRIVE_TIME, glide_lv_now(), FINAL_COMBO_GROUND, FINAL_COMBO_AIR, FINAL_RUN, FINAL_JUMP_H);
    const Mot *lm[6];  // (s68c: locomotion slots; in Valor's bake = idle 0 walk 4 run 8 jump 14 fall 18 land 22)
    for (int i = 0; i < 6; i++) lm[i] = mot_of(i);
    LOG("[form] loco slots: idle %d walk %d run %d jump %d fall %d land %d\n", lm[0] ? lm[0]->slot : -1, lm[1] ? lm[1]->slot : -1,
        lm[2] ? lm[2]->slot : -1, lm[3] ? lm[3]->slot : -1, lm[4] ? lm[4]->slot : -1, lm[5] ? lm[5]->slot : -1);
}

// every driven frame after the motion clock (sora.c step 2)
static void fm_tick(float kh) {
    fm_dim_tick(kh);  // (s71; before the early return: the dim fades out after a revert too)
    if (FM.st == FM_BASE) return;
    if (FM.away) {
        FM.away = false;
        if (FM.st == FM_FORM)
            FM.set_body(ofx_actor_form(g_fform)), g_fmots_on = true, LOG("[form] driven again: %s body back\n", FM_FORMS[g_fform].name);
    }
    if (FM.st == FM_CHANGE) {
        if ((FM.clock += kh) >= FM.swap_at) fm_enter();
        return;
    }
    bool battle = P.on && P.combat && P.battle;
    FM.t -= kh * (battle ? 1.0f : FM_RELAX);
    anti_life_tick();  // (s89: in Anti only - no heal, x1.5 damage taken)
    if (FM.t <= 0) fm_revert("form gauge empty");
}

// Sora not driven this frame (sora.c's Link-state branch): dead / loading = revert; else base Sora's body meanwhile
static int g_fm_away_base;  // (s70)
static void fm_other(bool hard) {
    if (FM.st == FM_BASE) return;
    if (hard || FM.st == FM_CHANGE) { fm_revert(hard ? "dead / loading" : "not driven during the change"); return; }
    // (s70 user: Valor keeps its body while climbing / swimming; the puppet retarget takes a form's 252 bones since
    // s69b) live "set fmaway 1" = the s68 rule (base body meanwhile)
    if (!FM.away && g_fm_away_base) {
        FM.away = true, g_fmots_on = false;
        if (FM.set_body) FM.set_body(ofx_actor_base());
        LOG("[form] not driven (climb / swim / puppet): base body meanwhile, gauge holds\n");
    }
}

static bool fm_live_set(const char *key, int v) {
    if (!strcmp(key, "form")) {
        if (v) fm_drive(v >= 1 && v <= FM_NFORMS ? v - 1 : FORM_VALOR, true);  // (s89 trial: form N = FM_FORMS row N - 1 = KH2's form id)
        else fm_revert("set form 0");
    } else if (!strcmp(key, "fmswap")) FM.swap_at = v < 0 ? 0 : (float)v;
    else if (!strcmp(key, "fmfx")) g_fmfx_on = v != 0;
    else if (!strcmp(key, "fmse")) g_fmse_on = v != 0;  // (s87c drive sfx)
    else if (!strcmp(key, "fmsevol")) g_fmse_vol = v < 0 ? 0 : v > 200 ? 200 : v, LOG("[form] drive SE volume %d%%\n", g_fmse_vol);  // (s89b)
    else if (!strcmp(key, "fmvoice")) g_fmvoice_once = v != 0, LOG("[form] entry voice %s\n", g_fmvoice_once ? "once (the motion's trigger)" : "old double call");  // (s88)
    else if (!strcmp(key, "fmblend")) g_fsw_len = v < 0 ? 0 : (float)v;
    else if (!strcmp(key, "fmdim")) g_fmdim_pct = v < 0 ? 0 : v > 100 ? 100 : v;
    else if (!strcmp(key, "fmaway")) g_fm_away_base = v != 0;
    else if (!strcmp(key, "fmlimit")) {  // (s89)
        g_fm_limit_on = v != 0, LOG("[form] Limit Form %s\n", g_fm_limit_on ? "on" : "OFF (greyed on the Drive page)");
        if (!g_fm_limit_on && fm_in_form() && g_fform == FORM_LIMIT) fm_revert("set fmlimit 0");
    }
    else if (!strcmp(key, "wmom")) g_wis_mom = v != 0;
    else if (!strcmp(key, "wis44")) g_wis44 = v != 0, LOG("[form] Wisdom combo limits %s\n", g_wis44 ? "4 / 4 (old, Valor's plrp row)" : "3 / 3 (Wisdom's plrp 0x82)");
    else if (!strcmp(key, "fmfinal")) {  // (s89 b4-final)
        g_fmfinal_on = v != 0;
        if (!g_fmfinal_on && fm_in_form() && g_fform == FORM_FINAL) fm_revert("set fmfinal 0");
        LOG("[final] Final Form %s\n", g_fmfinal_on ? "on" : "OFF (greyed)");
    } else if (!strcmp(key, "fmom")) g_fin_mom = v != 0;
    else if (!strcmp(key, "fglidelv")) g_fglide_lv = v < 0 ? 0 : v > 5 ? 5 : v, LOG("[final] Glide level %d (0 = auto)\n", g_fglide_lv);
    else if (!strcmp(key, "wshot")) g_wshot_on = v != 0;
    else if (!strcmp(key, "wramp")) g_wramp = v != 0, LOG("[wisdom] combo ramp %s\n", g_wramp ? "ON (s87c speed / bullets / power)" : "off (vanilla rate)");  // (s89b)
    else if (!strcmp(key, "wcombo")) g_wcombo = v != 0;  // (s87c wisdom combo: 0 = the old fixed burst per swing)
    else if (!strcmp(key, "wshotdmg")) g_wshot_dmg = v < 1 ? 1 : v > 400 ? 400 : v;
    else if (!strcmp(key, "fmtime")) { if (FM.st == FM_FORM) FM.t = (float)v; }
    else if (rox_live_set(key, v)) {}  // (s109: char)
    else return anti_live_set(key, v);  // (s89: fmanti / antipts / antiforce / ...)
    return true;
}

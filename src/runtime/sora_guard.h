// sora_guard.h: KH2 Guard (s88 b3-guard, recon/S88_HELPER_GUARD.md). Square with the stick neutral = Guard (Dodge Roll
// when the stick is pushed, today's roll). Included from sora.c after link_heal (needs S, play, mot_of, hit_slot,
// nearest_enemy, link_actor, LOG, sora_fx_event, sora_sfx_*).
//
// KH2 facts (VERIFIED from D:/KH2Extract via sorabake: P_EX100.mset slot 692 = A322 = motion 173 "Guard", 60 GFR frames,
// attack_hitbox ranges atkp 161 over GFR 6..37; slot 168 = A321 = motion 42 "AUTO_GUARD" = the guard reflect, atkp 162
// 0..40; 00battle atkp 161 / 162: Type Guard, RefactOther Reflect, ReflectRange 120 / 180 cm, Power 0). Ability 0x52
// "Guard" (bit 1, ptya Id 11 standing still -> re:0x3A9C70) is a story ability no KH2 table teaches (so it is not in
// AB_TAB): Sora has it from the start here. PHH hit_reaction_kh2.py (reference): a Guard that blocks plays motion 42 and
// sets +0xD70 = 80 invulnerable frames (re:0x3D61D0).
// GUESS: the block is frontal (nearest enemy in the front half plane; KH2's guard shape is a 120 cm sphere in front).
// Drive Forms: base Sora only. Valor's ptya Id 11 is a leaping slash (B330 = motion 181), not a guard; the other
// forms' Id 11 was not read (GUESS: none) - Square in a form stays as it was.
#pragma once

#define MOT_GUARD 173
#define MOT_GUARD_REFLECT 42
#define GUARD_INVULN_KH 80.0f   // PHH hit_reaction_kh2.py GUARD_INVULN (re:0x3D61D0)
#define GUARD_WIN_A 6           // atkp 161 window, GFR (sora_hits.bin slot 692), used if the hit bake is missing
#define GUARD_WIN_B 37

static int g_guard_gate = 0;  // (s91 b5-abil) live "set guardgate 1": Guard needs the ability (id 82) equipped on the Abilities page, as in KH2; default OFF = always; debugall ON = counts as equipped (s91 review)
static int g_guard = 1;  // live "set guard 0/1" (0 = old behaviour: Square always rolls)
// (s88 review) live "set guardinv 0/1/2": what the 80 frames after a block swallow. 0 = nothing (only the guard window
// blocks), 1 = enemy hits only (an enemy within the target radius; lava / fire / a lone hazard with no enemy near still
// lands), 2 = everything (PHH's +0xD70 reading). GUESS default 1: KH2's counter is general, BotW has hazards KH2 has not.
static int g_guard_inv = 1;
static struct {
    float inv;            // invulnerable KH frames left after a block
    int blocks_seen;      // linkpos_guard_blocks() last seen
    int life_prev;        // last frame's life (the DLL-only refund)
    uint64_t life_at;     // (s88 review) the present life_prev was read: a refund only from the frame just before
    bool exe_known, exe_ok;
    bool set_win, set_win_prev;  // (s88 review) the flag was set by the guard window (not the invulnerable frames)
    int refl_seen;               // (s90b) linkpos_guard_reflects() last seen
    uint64_t block_at;           // (s90b) vk_frame_counter() of the last guard_block by the window (one spark per hit)
} G = {0};
static void (*p_guard_set)(int);
static int (*p_guard_blocks)(void);
static int (*p_guard_dmg)(void);
// (s90b) Guard sends the Guardian beam back (recon/S90_HELPER_PARRY.md option B: the exe hooks the beam's reflect check
// and says "reflect" while this flag is set; BotW's own reflect then flies it back at 30 % speed into enemies).
// Live "set parry 0/1" (0 = the beam explodes on Sora as before; the damage is still blocked), "set parryfront 0/1/2"
// (1 = the beam must fly at Sora's front (exe, beam direction GUESS layout), 2 = the nearest enemy must be in front
// (guard_front), 0 = no direction test), "set parryrad <cm>" (hit point to Sora, default 250 GUESS).
static int g_parry = 1, g_parry_front = 1, g_parry_rad = 250;
// (s90b user test: the beam's hit came 1-2 frames after the press, before KH2's window (GFR 6) opened: 51 damage, dead.
// A BotW beam is too fast for KH2's 0.1 s startup.) The guard window (block + parry) opens at GFR g_guard_from instead
// of the atkp start. Live "set guardfrom N" (6 = KH2's own timing, 0 = from the press, default).
static int g_guard_from = 0;
static void (*p_refl_set)(int, const float *, float, float);
static int (*p_refl_n)(void);

// (s92) parry option A (recon/S90_HELPER_PARRY.md, DLL only): s91 showed the exe hook on the beam's reflect check
// (1.9.0 0x7000605338) is never called. Instead, while the guard window is live, raise Link's own guard bits at
// Link+0xc40: bit 4 = isGuard (Link vslot 166), bit 5 = isGuardJust (vslot 167); offset VERIFIED same on 1.9.0 by byte
// match. BotW's beam hit-record builder then sets flags 1|2 and the beam runs its vanilla REFLECT (x0.3 speed, back at
// enemies). The cone test uses Link's facing (= Sora's with linkface 1). Only bits WE raised are cleared (window end,
// parry off, Link actor change, unload). GUESS risks: BotW may clear the bits each frame (we re-raise; logged), Link's
// body sensor may skip the just bit (then the beam only ricochets sideways), isGuard may touch other Link logic
// (shield wear if Link has a shield equipped). Live "set parrylink 0/1" (default 1; 0 = never touch Link's flags).
#define LINK_GUARD_FLAGS 0xc40
#define LINK_GUARD_BITS 0x30u
static int g_parry_link = 1;
static struct { uint64_t actor; uint32_t own; int windows, relost; bool logged; } PL;
static void parry_link_clear(const char *why) {
    if (PL.own && PL.actor && rd_ok(PL.actor + LINK_GUARD_FLAGS, 4)) {
        uint32_t *f = (uint32_t *)(uintptr_t)(PL.actor + LINK_GUARD_FLAGS);
        *f &= ~PL.own;
        if (PL.logged) LOG("[sora] parrylink: cleared Link guard bits %#x (%s; BotW dropped them %d times this window)\n", PL.own, why, PL.relost);
    }
    PL.own = 0, PL.actor = 0, PL.relost = 0, PL.logged = false;
}
static void parry_link_frame(bool on) {
    uint64_t a = on && g_parry_link ? link_actor() : 0;
    if (PL.own && PL.actor != a) parry_link_clear(on && g_parry_link ? "Link actor changed" : "window end");
    if (!a || !rd_ok(a + LINK_GUARD_FLAGS, 4)) return;
    uint32_t *f = (uint32_t *)(uintptr_t)(a + LINK_GUARD_FLAGS), v = *f;
    if (PL.own & ~v) PL.relost++;                       // one of our bits was cleared by BotW since last frame
    PL.own |= LINK_GUARD_BITS & ~(v & ~PL.own);         // bits Link set himself stay his
    PL.actor = a, *f = v | PL.own;
    if (!PL.logged) {
        PL.logged = true, PL.windows++;
        LOG("[sora] parrylink: window #%d, Link+0xc40 %#x -> %#x (ours %#x)\n", PL.windows, v, *f, PL.own);
    }
}

static void guard_bind(void) {
    if (G.exe_known) return;
    G.exe_known = true;
    HMODULE m = GetModuleHandleA(NULL);
    p_guard_set = (void (*)(int))(void *)GetProcAddress(m, "linkpos_guard_set");
    p_guard_blocks = (int (*)(void))(void *)GetProcAddress(m, "linkpos_guard_blocks");
    p_guard_dmg = (int (*)(void))(void *)GetProcAddress(m, "linkpos_guard_last_damage");
    G.exe_ok = p_guard_set && p_guard_blocks && p_guard_dmg;
    LOG("[sora] guard: %s\n", G.exe_ok ? "exe hook present: a blocked hit is dropped before it lands"
                                       : "exe has no guard hook (NEEDS EXE RELINK): a blocked hit is refunded (BotW's flinch still plays)");
    if (G.exe_ok) G.blocks_seen = p_guard_blocks();
    p_refl_set = (void (*)(int, const float *, float, float))(void *)GetProcAddress(m, "linkpos_guard_reflect_set");
    p_refl_n = (int (*)(void))(void *)GetProcAddress(m, "linkpos_guard_reflects");
    if (!p_refl_set || !p_refl_n) p_refl_set = NULL, p_refl_n = NULL;
    LOG("[sora] guard: beam reflect %s\n", p_refl_n ? "hook present (set parry 0 = off)" : "not in this exe (NEEDS EXE RELINK)");
    if (p_refl_n) G.refl_seen = p_refl_n();
}

static bool guard_motion(void) { return S.special == SP_GUARD && S.mot; }

// the atkp 161 / 162 guard window of the playing guard motion (GFR, motion-relative)
static bool guard_window(void) {
    if (!guard_motion()) return false;
    const HitSlot *h = hit_slot(S.mot->slot, 0);
    int a = g_guard_from < GUARD_WIN_A ? g_guard_from : GUARD_WIN_A;
    if (!h) return S.motion == MOT_GUARD ? S.clock >= a && S.clock <= GUARD_WIN_B : S.motion == MOT_GUARD_REFLECT;
    for (int i = 0; i < h->nwin; i++) {
        int st = h->win[i].start;
        if (S.motion == MOT_GUARD && h->win[i].atkp == 161 && g_guard_from < st) st = g_guard_from;  // (s90b)
        if ((h->win[i].atkp == 161 || h->win[i].atkp == 162) && S.clock >= st && S.clock <= h->win[i].end) return true;
    }
    return false;
}

static bool guard_front(const float p[3]) {  // GUESS: the hit comes from the front half plane (nearest enemy; none = a shot)
    float e[2];
    return !nearest_enemy(p, e) || (e[0] - p[0]) * sinf(S.face) + (e[1] - p[2]) * cosf(S.face) >= 0;
}

// The Square press. true = the press became a Guard (the caller skips the roll).
static bool guard_press(uint64_t press, float sspeed, bool is_free, const float p[3]) {
    if (!g_guard || (g_guard_gate && g_abilfix && !g_dbgall && !ab_on(AB.on, AB_GUARD)) || !(press & BTN_ROLL) || sspeed > 0 || fm_in_form() || S.act != ACT_IDLE || S.takeoff > 0 ||
        S.air_frames || !is_free || !mot_of(MOT_GUARD))
        return false;
    play(MOT_GUARD), S.special = SP_GUARD;
    LOG("[sora] guard at %.3f %.3f %.3f face %.1f (window GFR %d..%d)\n", p[0], p[1], p[2], S.face * 180 / PI, g_guard_from < GUARD_WIN_A ? g_guard_from : GUARD_WIN_A, GUARD_WIN_B);
    return true;
}

// A block: KH2's reflect (re:0x3D61D0): motion 42 + the invulnerable frames, the Keyblade guard spark and se.
// (s88 review) by_win false = a hit swallowed by the invulnerable frames only: no new 80 frames (no endless chain while
// hits keep coming), no reflect motion over whatever Sora does by then (a combo, a roll), just the log.
static void guard_block(const float p[3], float to_u, int dmg, const char *how, bool by_win) {
    if (!by_win && mg_reflect_up()) {  // (s133 magic) Reflect's barrier swallowed it: counted, the flash element, no guard motion / invulnerable frames
        if (vk_frame_counter() > G.block_at + 2) G.block_at = vk_frame_counter(), mg_block(p, to_u, dmg, how);
        return;
    }
    if (!by_win) {
        static uint64_t n;
        if (n++ % 8 == 0) LOG("[sora] guard: hit (damage %d, %s) swallowed by the invulnerable frames (%.0f left; #%llu)\n", dmg, how,
                              G.inv, (unsigned long long)n);
        return;
    }
    if (vk_frame_counter() <= G.block_at + 2) return;  // (s90b: the beam's reflect + its dropped damage = one block)
    G.block_at = vk_frame_counter();
    G.inv = GUARD_INVULN_KH;
    float c[3] = {p[0] + sinf(S.face) * 0.7f, p[1] + 1.0f, p[2] + cosf(S.face) * 0.7f};
    sora_fx_event(kbfx_name("hit_guard"), c, S.face, to_u);  // w_ex010.3 (WALL_REPEL_RECON.md 10: the Keyblade's guarded-hit spark)
    static int se = -2;
    if (se == -2) se = sora_sfx_id("wall_hit");  // GUESS: the Keyblade clang (se500 #10); KH2's guard hit se not found
    if (se >= 0) sora_sfx_play_at(se, 1.0f, c);
    if (S.motion != MOT_GUARD_REFLECT && mot_of(MOT_GUARD_REFLECT)) play(MOT_GUARD_REFLECT), S.special = SP_GUARD;
    LOG("[sora] GUARD blocked a hit (damage %d, %s) at %.3f %.3f %.3f: reflect motion, %.0f invulnerable frames\n", dmg, how,
        p[0], p[1], p[2], G.inv);
}

// (s133 review) Reflect cancelled while Link has the pad / a load: guard_frame only runs while Sora is driven, so drop the
// exe hit drop, the beam reflect and Link's isGuard bits here (Mickey mode keeps its own drop)
static void guard_release(const char *why) {
    if (G.exe_ok && p_guard_set && !mk_drops_hits() && !g_dbm_god) p_guard_set(0);
    if (p_refl_set) { float z[3] = {0, 0, 0}; p_refl_set(0, z, NAN, 0); }
    parry_link_clear(why);
}

// every frame, before the hit check (step 0). Returns true when this frame's life drop was a blocked hit (the DLL-only
// refund): the caller then skips the flinch.
static bool guard_frame(const float p[3], float to_u, float kh, int *hits_seen, int link_hits) {
    guard_bind();
    if (G.inv > 0 && (G.inv -= kh) < 0) G.inv = 0;
    if (!g_guard) G.inv = 0;
    float e[2];  // (s88 review: the invulnerable frames swallow enemy hits only by default, see g_guard_inv)
    bool inv = G.inv > 0 && (g_guard_inv >= 2 || (g_guard_inv == 1 && nearest_enemy(p, e)));
    bool rf = mg_reflect_up();  // (s133 magic) Reflect's barrier is up: every hit is blocked like the guard window's, the Guardian beam is reflected
    bool win = g_guard && guard_window() && guard_front(p), want = win || inv || rf || pl_invuln();  // (s133 limits: a party Limit runs = invulnerable)
    bool refunded = false;
    int life = linkpos_link_life();
    G.set_win_prev = G.set_win, G.set_win = win;
    parry_link_frame((g_guard && g_parry && guard_window() && (g_parry_front != 2 || guard_front(p))) || (rf && g_parry));  // (s92 option A; s133: and Reflect's barrier)
    if (p_refl_n) {  // (s90b) the beam reflect: the guard window only (no reflect off the 80 invulnerable frames)
        bool gwin = g_guard && g_parry && guard_window() && (g_parry_front != 2 || guard_front(p)), rwin = gwin || (rf && g_parry);
        p_refl_set(rwin ? 1 : 0, p, rf && !gwin ? NAN : g_parry_front == 1 ? S.face : NAN, g_parry_rad / 100.0f);  // (s133: Reflect = all around, no direction test)
        int r = p_refl_n();
        if (r != G.refl_seen) {
            LOG("[sora] GUARD reflected a beam (#%d, %d since last frame)\n", r, r - G.refl_seen);
            G.refl_seen = r;
            guard_block(p, to_u, 0, "beam reflect", gwin || !rf);  // (skipped if its dropped damage already played one; s133: Reflect's barrier alone = its own block)
        }
    }
    if (G.exe_ok) {
        p_guard_set(want || mk_drops_hits() || g_dbm_god ? 1 : 0);  // (s110 review: this ran after mk_tick and cleared Mickey mode's drop every driving frame)
        int b = p_guard_blocks();  // (the drop ran in the game frame after last frame's set: its reason is set_win_prev)
        if (b != G.blocks_seen) { G.blocks_seen = b; if (!mk_drops_hits()) guard_block(p, to_u, p_guard_dmg(), "exe drop", G.set_win_prev || win); }  // (Mickey's drops: sora_mickey.h charges them)
    } else if (want && link_hits != *hits_seen && G.life_prev > 0 && life > 0 && life < G.life_prev &&
               vk_frame_counter() <= G.life_at + 2) {  // (life 0: BotW's death already ran; a stale life_prev would over-heal)
        uint64_t a = link_actor();
        if (a && rd_ok(a + 0x848, 4) && *(const int32_t *)(uintptr_t)(a + 0x848) == life) {
            *(int32_t *)(uintptr_t)(a + 0x848) = G.life_prev;
            *hits_seen = link_hits, refunded = true;
            guard_block(p, to_u, G.life_prev - life, "refund", win);
        }
    }
    G.life_prev = refunded ? G.life_prev : life, G.life_at = vk_frame_counter();
    return refunded;
}

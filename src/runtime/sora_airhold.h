// sora_airhold.h - s55 AIRHOLD (recon/S55_HELPER_AIRHOLD.md): KH2's blow and air-hop hit reactions on BotW enemies.
// Included once by sora.c (static functions only, like sora_levels.h). DLL only: no exe change needed.
//
// KH2 (VERIFIED, S55_HELPER_AIRHOLD.md 1): re:0x3D2FE0 maps the atkp EnemyReaction through re:0x3CE980 to a class;
//   class 3 on an AIRBORNE target = air flinch + re:0x3D4800: a 50 cm jump start (re:0x5B532C) from where it is, at
//   strength x 0.048 cm/frame along the hit (strength = KnockbackStrength1, 0x7FFF = sstm HitBackSmall 200);
//   class 4 = BLOW: rise = KnockbackStrength1, horizontal = KnockbackStrength2 (re:0x3CEA00; both 0: sstm BlowUp 24 /
//   BlowSpeed 40), then ACTION 4 physics re:0x422780 (per 2-frame step: horizontal x0.8 down to 10 %, rise x0.8, +g,
//   fall capped at 16 cm/frame) until it lands. Thunder Lv1 = atkp 250 (4, 30, 0): straight up ~1.9 m.
// BotW (S55_HELPER_AIRHOLD.md 2): an enemy leaves the ground only as a ragdoll (BlownOff, f54 >= 0x14 for the Sword
//   kind). While its ragdoll is in the world we set EVERY ragdoll body's linear velocity each frame through BotW's
//   own ksys::phys::RigidBody::setLinearVelocity, servoing the enemy's root to KH2's curve; Havok still collides.
//   Guest calls only from sora_frame (game thread, inside linkpos_frame: the window linkpos_raycast uses).
//
// Live (run/live_test.txt "set <key> <n>"): ah 0/1 (all of it), ahlog 0/1/2, ahhop 0/1 (air hits hop), ahblow 0/1
//   (blow-class hits take KH2's shape), ahheld <f54> (the damage type sent for hits on a held enemy, s57 default 0xf; 0x14 =
//   20: keeps BotW's BlownOff running; 15 = small to A/B), ahhoph <cm> (50), ahthr <cm/frame> (Thunder rise, 30),
//   ahvmax <u/s> (servo clamp, 40), ahair <cm> (floor gap that counts as airborne for a ragdoll we do not hold, 40),
//   ahcc 0/1 (exe slice 2: lift standing enemies by their controller), ahlf54 <f54> (Thunder's damage type, 0x14 =
//   BotW's BlownOff ragdoll; s57 default 15 + ahcc 1 = an animated launch with no ragdoll), ahlf40 (Thunder element, 0; -1 = electric),
//   ahreq 0/1 + ahsnap 0/1/2 (s57 exe fix knobs, recon/S57_HELPER_EVEL.md), ahrbreq 0/1 (DLL-side body request, for an older exe).
//   s58 lift fix (recon/S58_HELPER_LIFT.md): ahfull 0/1 (1: only a FULL ragdoll - blend state < 2 - counts as ragdolled; 0 =
//   any ragdoll in the world, the s55 rule), ahccheld <f54> (every hit on a controller-held enemy is sent as this, 7 =
//   BotW's Shock with no ragdoll; 0 = the ahheld rule), ahang 0/1 (zero every full-ragdoll body's spin each frame, 1), ahfrz <frames> (end a controller
//   lift whose enemy stops moving, 4; 0 = off).
//   Env SAVAGEWIND_SORA_AIRHOLD=0 starts with it off.

#define AH_RB_SET_LINVEL (sora_sites()->rb_set_linear_velocity)  // RigidBody::setLinearVelocity(x0 body, x1 const Vec3f*, s0 eps) (Ghidra 0x71011bf3c0)
#define AH_RB_SET_ANGVEL (sora_sites()->rb_set_angular_velocity)  // RigidBody::setAngularVelocity(x0 body, x1 const Vec3f*, s0 eps) (Ghidra 0x71011bf458,
                                          // S58_HELPER_LIFT 3: changeWorldState calls it right after setLinearVelocity)
#define AH_ACTOR_PHYSICS 0x578            // Actor -> phys::InstanceSet* (RAGDOLL_RECON 1, 45 sites)
#define AH_IS_RAGDOLL 0x90                // InstanceSet -> RagdollInstance* (RAGDOLL_RECON 1)
#define AH_IS_BLEND 0x112                 // InstanceSet: s8 ragdoll blend state (0 full_key_framed, 1 full_dynamic, >= 2 the
                                          // actor's .brgbw states, e.g. Bokoblin head_dynamic 2 / attack_dynamic 3: BotW's
                                          // powered hit flinch; S58_HELPER_LIFT 1)
#define AH_RAG_NBODY 0x28                 // RagdollInstance: s32 body count, +0x30 RagdollRigidBody** (VERIFIED 1.8.2)
#define AH_RAG_BODIES 0x30
#define AH_RAG_FLAGS 0xc0                 // u32, 0x10 = AddedToWorld (getWorldState 0x7101469624)
#define AH_RAG_IN_WORLD 0x10u
#define AH_MAX 8
#define AH_STEP 2.0f                      // KH2 ACTION physics sub-step, frames (re:0x3B89A0)
#define AH_BLOW_BRAKE 0.8f                // 03system 0x183B4 / 0x183BC (BlowBrakeXZ / BlowBrakeUp)
#define AH_BLOW_MIN 0.1f                  // 0x183B8 BlowMinXZ
#define AH_BLOW_UP 24.0f                  // 0x183C0 sstm BlowUp (both atkp strengths 0)
#define AH_BLOW_SPEED 40.0f               // 0x183C4 sstm BlowSpeed
#define AH_HIT_BACK_SMALL 200.0f          // 0x183D0 (KnockbackStrength1 0x7FFF on a class-3 hit)
#define AH_HOP_SPEED 0.048f               // 0x183D4 HitBackToJump: hop speed = strength x this
#define AH_AIR_BRAKE 0.95f                // ACTION_JUMP horizontal brake with no intent, per physics step (re:0x3B7AD0)
#define AH_PUSH_SPEED 0.1f                // re:0x3D4960 grounded hit-back: speed = strength x 0.1 cm/frame (S55_HELPER_AIRHOLD 1.1)
#define AH_PUSH_BRAKE 0.9f                //   x 0.9 per KH frame (A300 kb 125: 12.5 cm/frame, ~1.2 m in all)
#define AH_PUSH_END 0.5f                  //   cm/frame: the slide ends below this (GUESS: ~30 KH frames for 125)

enum { AH_FREE, AH_BLOW, AH_HOP, AH_SLIDE };  // (s66 SLIDE: the grounded class-3 hit-back)
typedef struct {
    uint64_t actor;
    int kind, frames, still, norag, stuck;
    float t;               // KH frames on the curve
    float base[3];         // world u: where the curve started (its feet)
    float dir[2];          // push direction (x, z), unit
    float cx, cy;          // curve offset so far, cm (along dir, up)
    float h, vy, hmin;     // BLOW state (cm/frame; vy KH-style: down positive)
    float jh, apex, hv;    // HOP: height, apex time, horizontal speed
    float last_y, last_req;
    bool cc;               // driven through linkpos_enemy_vel (exe slice 2) this frame, not the ragdoll
    bool flinch_seen;      // (s58 lift) BotW's partial hit-flinch ragdoll seen (logged once)
    int frozen;            // (s58 lift) frames the controller-lifted enemy did not move although the curve asked
    int dead_wait, dead_zero;  // (s60b ahdead) frames since the kill with no death ragdoll yet / frames zeroed
    float hold;                // (s60b ahthhold) KH frames left to hang at the BLOW's apex (Thunder)
    const char *what;
} AhEnt;
// (s60b, user: air combos "the bokoblins will still shake around in the air"; a Thunder kill "will still shake it
// around then send it rag doll body flying away") live_s60: the air hits went out as 0x14 (ahheld) -> BotW's full
// BlownOff ragdoll, then the body servo (16 bodies, up to ahvmax 40 u/s) shook it; a kill started BotW's death ragdoll
// and the servo kept pushing it ("stuck", v -40). Now: ahcchit 1 = a hit that starts a KH2 hop / blow on a small enemy
// in the air goes out as ahccheld (7, Shock: no ragdoll) and rides the controller lift like the Thunder pop
// (2 = grounded blows too: the ground finisher; 0 = s60). ahdead 1 = at life 0 let go (controller off), zero the
// death ragdoll's velocity for ahdeadn frames once it is in the world, then BotW's ragdoll falls on its own (2 = let go
// only; 0 = s60, servo on).
static int g_ah_cchit = 2, g_ah_dead = 1, g_ah_dead_n = 3;
// (s60b, user: "when i cast thunder they stay in the air for a little bit longer") a Thunder blow hangs at its apex for
// ahthhold KH frames (live "set ahthhold <n>", 0 = KH2's plain pop) - long enough for the ground press's jump-up attack
// (ptya id 0 / A331: a target centre 1.9-5 m above Sora) to catch it.
static float g_ah_th_hold = 30.0f;
static int g_ah_form_rise = 94;  // (s70) a Drive form's blow rise speed, % (height ~ square: 94 = ~12% lower) (user: Valor finisher a bit lower)
static AhEnt g_ah[AH_MAX];
static int g_ah_on = -1, g_ah_log = 1, g_ah_hop = 1, g_ah_blow = 1, g_ah_held_f54 = 0x14;  // (s61: ON by default - s58 LIFT fix merged, Thunder pop user verified s60)
static float g_ah_hop_h = 50.0f, g_ah_thunder_rise = 30.0f, g_ah_vmax = 40.0f, g_ah_air_cm = 40.0f;
static uint64_t g_ah_calls, g_ah_fail;
// exe slice 2 (linkpos.c linkpos_enemy_vel: a standing enemy's character controller velocity), looked up by name like
// warp_tint: an exe linked before it has none, then only ragdolls are driven. Live "set ahcc 0/1" (default 1).
static void (*g_ah_evel)(uint64_t, bool, float, float, float);
// (s57, user: "shake around and then sometimes get launched in a direction") Thunder's defaults with the controller
// lift: f54 0xf (a small flinch: no BlownOff ragdoll and its impulse) and f40 0 (no BotW electric shock / paralysis
// shake); KH2's Thunder is a plain straight-up blow. Without slice 2 it falls back to 0x14 + the spell's element.
// (s58 lift, S58_HELPER_LIFT 1: the s58 exe lifts the controller - live_s58 +1.1 u in one step; what "ragdolled" it was
// BotW's own hit reaction, chosen by f54 = the damage record's getField54: 0xf / 0x11 / 0x15 (2, 5) start the partial
// flinch ragdoll (0x7100a7fd4c), 10 / 0x14-0x17 / 0x1b the full BlownOff one (EnemyDefaultReaction 0x7100735e84). f54 7 =
// the reaction "Shock" (AS "Miss" stagger, 3-frame knockback the controller lift overrides) with NO ragdoll at all: an
// upright animated hurt pose. Live "set ahlf54 15" = small flinch + partial ragdoll, "set ahlf54 20" = the old default.)
static int g_ah_evel_tried, g_ah_cc = 1, g_ah_launch_f54 = 7, g_ah_launch_f40 = 0;
// (s66 audit #5, user: Bokoblins kept swinging through combos) KH2 class 3 on a GROUNDED target = flinch (re:0x3D43B0)
// + hit-back push (re:0x3D4960). A blade hit of class 3 on a small grounded enemy goes out as ahgf54 (7 = BotW's Shock
// stagger, upright, no partial ragdoll; 0 = keep BotW's f54 0xf) and slides it on its controller (AH_SLIDE, floor
// followed by ray). Live "set ahgfl 0" = the s65 rule (BotW's small flinch + BotW push only).
static int g_ah_gfl = 1, g_ah_gf54 = 7;
static int g_ah_slv = 100;  // (s80 fps A/B) the slide servo's velocity x ahslv % (0 = controller held still)
static int g_ah_sray = 1;   // (s80 fps A/B) 0 = the slide skips its per-frame floor ray (keeps the body's own height)
// (s80) 0 = the grounded flinch only sends ahgf54 (BotW's Shock stagger + its own knockback) and starts no controller
// slide: an enemy on the linkpos_enemy_vel override costs the game ~37 ms a frame (slow frame log, 70 vs 33 ms)
static int g_ah_slide = 1;  // (s81: on again, the override cost was readable() VirtualQuery, fixed in linkpos.c)
// s58 lift knobs (header): full-ragdoll rule, the f54 for hits on a controller-held enemy, spin zeroing, frozen end
static int g_ah_full = 1, g_ah_ccheld_f54 = 7, g_ah_ang = 1, g_ah_frz = 4;
static uint64_t g_ah_ang_calls;
// s57 evel fix (recon/S57_HELPER_EVEL.md): an exe with linkpos_enemy_vel_opts requests the velocity on the controller's
// body itself and snaps a body the game held down; live "set ahreq 0/1", "set ahsnap 0/1/2" pass through to it. On an
// exe without it, ahrbreq 1 (default there) makes the DLL do the request part: RigidBody::setLinearVelocity on the
// controller's body (cc + 0x398) every frame, like ah_body_vel on ragdoll bodies.
static void (*g_ah_evel_opts)(int, int);
static int g_ah_rbreq = -1;  // (-1: on when the exe has no linkpos_enemy_vel_opts)
static bool ah_cc_ok(void) {
    if (!g_ah_evel_tried) {
        g_ah_evel_tried = 1;
        g_ah_evel = (void (*)(uint64_t, bool, float, float, float))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_enemy_vel");
        g_ah_evel_opts = (void (*)(int, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_enemy_vel_opts");
        if (g_ah_rbreq < 0) g_ah_rbreq = !g_ah_evel_opts;
        LOG("[sora-ah] linkpos_enemy_vel %s; s57 fix %s\n", g_ah_evel ? "found (standing enemies can be lifted too)" : "not in this exe (ragdolls only)",
            g_ah_evel_opts ? "in the exe (request + snap)" : "not in the exe (DLL body request only)");
    }
    return g_ah_evel && g_ah_cc;
}

static bool rd_ok(uint64_t a, size_t n);  // (sora.c, below)

static bool ah_enabled(void) {
    if (g_ah_on < 0) { const char *e = getenv("SAVAGEWIND_SORA_AIRHOLD"); g_ah_on = !(e && *e == '0'); }
    return g_ah_on != 0;
}

// (s58, user: "only small/medium enemies like vanilla KH2" - the Stone Talus is a mini boss) the KH2 blow only takes on
// an enemy whose levels-table KH HP percent is <= g_ah_size (live "set ahsize", default 100; 100 = Bokoblin / Chuchu / Keese /
// Octorok / Pebblit, 150 = + Moblin / Lizalfos / Yiga / Wizzrobe, 200 = Hinox / Talus / Lynel / Guardian too);
// bosses (flag 1, pct 0) never. An enemy the table does not know is lifted (the old rule).
static int g_ah_size = 100;  // (s58 user: Moblins stay on the ground too)
static float g_ah_hgain = 100.0f;  // (s58 spin A/B, see ah_tick)
static bool ah_small(uint64_t actor) {
    for (int i = 0; i < SL_NC; i++)
        if (SL.c[i].used && SL.c[i].actor == actor && SL.c[i].named)
            return !(SL.c[i].flags & 1) && SL.c[i].pct && SL.c[i].pct <= g_ah_size;
    return true;
}

// Thunder's BotW damage type + element (live "set ahlf54 / ahlf40", ahlf40 -1 = the spell's own electric). A big enemy
// (ah_small) gets a plain hit (f54 0x11, as Fire / Blizzard): no BotW BlownOff either.
static void ah_thunder_dmg(uint64_t actor, int *f54, int *f40) {
    if (!ah_enabled()) return;
    if (g_ah_launch_f40 >= 0) *f40 = g_ah_launch_f40;
    if (!ah_small(actor)) { *f54 = 0x11; return; }
    bool cc = ah_cc_ok();
    // (s58 lift) with ahblow 0 (no KH2 pop) Thunder keeps the s58b hit: 0x14, BotW's own BlownOff
    int lf = g_ah_blow ? g_ah_launch_f54 : 0x14;
    if (lf > 0) *f54 = cc || lf >= 0x14 ? lf : 0x14;
}

// re:0x3CE980 (VERIFIED decompile, airhold_kh2a.c): atkp EnemyReaction -> reaction class (s194: the table moved to sora_net_proto.h
// net_react_class, so a NET_REACT receiver checks a reaction with the very same rule)
static int ah_class(int r) { return net_react_class(r); }

// The KnockbackStrength pairs of Sora's atkp rows (file:kh2_first/00battle.bin, 0x30-byte rows from 0x158; the mined
// sora_attacks.kh.json agrees). sora_hits.bin carries the EnemyReaction but not the strengths.
static const struct { uint16_t atkp; uint8_t react; int16_t kb1, kb2; } AH_ATKP[] = {
    {126, 3, 125, 0}, {127, 3, 125, 0}, {128, 3, 75, 0}, {147, 3, 125, 0}, {130, 7, 5, 45}, {129, 7, 3, 45},  // A300-A304
    {131, 3, 125, 0}, {132, 3, 125, 0}, {133, 7, 3, 45}, {134, 7, 5, 45},                                      // A330-A333
    {170, 4, 40, 10}, {890, 3, 125, 0}, {1007, 3, 0, 0}, {1898, 3, 0, 0}, {1899, 3, 125, 0}, {968, 3, 350, 0},
    {975, 3, 125, 0}, {876, 11, 20, 0}, {1896, 3, 300, 0}, {1897, 3, 125, 0}, {1582, 4, 20, 5}, {1583, 3, 125, 0},
    {909, 4, 10, -1}, {1131, 3, 75, 0}, {963, 4, 3, 8}, {967, 3, 200, 0}, {915, 9, 10, -1}, {916, 9, 5, -1},
    {917, 9, 12, -10}, {927, 11, 3, 45}, {1882, 3, 75, 0},
    // (s69) Valor Form B300-B304 / B330-B333, both blades (00battle.bin atkp SubId 0; without them the finishers got the
    // 0x7fff default and flew enemies sky high)
    {171, 3, 125, 0}, {172, 3, 75, 0}, {175, 3, 125, 0}, {176, 3, 125, 0}, {177, 3, 125, 0}, {178, 3, 125, 0},
    {179, 3, 0, 0}, {180, 3, 125, 0}, {187, 3, 75, 0}, {189, 8, 75, 0}, {190, 9, 5, 10}, {191, 9, 10, 10},
    {192, 9, 10, 10}, {197, 11, 60, 5}, {198, 11, 60, 5}, {199, 8, 125, 0}, {200, 11, 30, 0}, {201, 3, 75, 0},
    {202, 3, 125, 0}, {203, 8, 75, 0}, {204, 8, 75, 0}, {227, 3, 125, 0}, {228, 3, 75, 0}, {237, 8, 75, 0},
    {238, 8, 75, 0}, {526, 11, 5, 45}, {527, 11, 5, 45}, {528, 11, 3, 45}, {529, 11, 3, 45},
    // (s89 b4-limit) Limit Form (00battle.bin atkp SubId 0, VERIFIED): Zantetsuken 1870, Ripple Drive 1877, Hurricane
    // Period 1871 / 1872, Sonic Rave 1878 / 1879, Last Arcanum 1883-1895, Infinity 1857-1862, Strike Raid 1866 / 1867
    {1857, 9, 20, -2}, {1858, 9, 15, -2}, {1859, 9, 10, -2}, {1860, 9, 5, -2}, {1861, 9, 60, 15}, {1862, 10, 125, 0},
    {1866, 8, 0, 0}, {1867, 11, 50, 0}, {1870, 11, 40, 0}, {1871, 9, 8, -2}, {1872, 11, 45, 20}, {1877, 10, 0, 0},
    {1878, 8, 0, 0}, {1879, 11, 50, 0}, {1883, 8, 75, 0}, {1884, 8, 75, 0}, {1885, 8, 75, 0}, {1886, 8, 75, 0},
    {1887, 8, 75, 0}, {1888, 8, 75, 0}, {1889, 8, 125, 0}, {1890, 8, 75, 0}, {1891, 8, 75, 0}, {1892, 8, 75, 0},
    {1893, 8, 75, 0}, {1894, 8, 75, 0}, {1895, 11, 5, 60},
    // (s89 b4-master) Master Form D330-D334 / D370-D374, both blades + the D334 body windows (00battle.bin atkp SubId 0, read
    // s89: react 4 = KH2's small blow, kb1 = its rise; the D333 / air finisher slams: kb1 -100 = down)
    {1059, 4, 8, 0}, {1060, 4, 10, 0}, {1061, 4, 4, 0}, {1062, 4, 4, 0}, {1063, 4, 2, 0}, {1065, 4, 4, 0}, {1066, 4, 2, 0},
    {1076, 4, 10, 0}, {1077, 4, 12, 0}, {1078, 4, 3, 4}, {1096, 4, 6, -5}, {1097, 4, 20, 0}, {1098, 4, 5, 0}, {1099, 4, 1, 45},
    {1100, 5, 150, 250}, {1101, 11, 50, 20}, {1252, 9, 6, 5}, {1254, 9, 3, 0}, {1255, 9, 6, 0}, {1256, 11, -100, 45},
    {1257, 9, 6, 5}, {1356, 11, -100, 45}, {1366, 4, 10, 0}, {1368, 9, 3, 0}, {1369, 9, 3, 0}, {1370, 9, 15, 15},
    // (s89 b4-final) Final Form E300-E303 / E330-E332 both blades + its glide (1281/1282) and cast swings (1428/1429,
    // 1607/1608, 30/31): 00battle.bin atkp SubId 0 (parse checked vs 126 / 130 / 171 / 526 above)
    {1390, 3, 75, 0}, {1391, 3, 75, 0}, {1392, 3, 75, 0}, {1393, 3, -25, 0}, {1394, 3, -25, 0}, {1395, 3, 75, 0},
    {1396, 3, 75, 0}, {1397, 3, -25, 0}, {1398, 3, -25, 0}, {1399, 3, 50, 0}, {1400, 3, 50, 0}, {1401, 9, 8, 0},
    {1402, 9, 8, 0}, {1403, 9, 3, 0}, {1404, 9, 3, 0}, {1405, 4, 4, -1}, {1406, 4, 4, -1}, {1407, 4, 4, 0},
    {1408, 4, 4, 0}, {1409, 4, 4, 0}, {1410, 4, 6, 0}, {1411, 4, 6, 0}, {1412, 3, 75, 0}, {1413, 3, 75, 0},
    {1414, 9, 3, 0}, {1415, 9, 3, 0}, {1416, 9, 4, 0}, {1417, 9, 4, 0}, {1418, 9, 10, 0}, {1419, 9, 10, 0},
    {1420, 11, 3, 45}, {1421, 11, 3, 45}, {1422, 9, 6, 0}, {1423, 9, 6, 0}, {1424, 9, 6, 0}, {1425, 9, 6, 0},
    {1426, 11, 40, -20}, {1427, 11, 40, -20}, {1457, 17, 8, 20}, {1458, 17, 8, 20}, {1281, 4, 5, 45}, {1282, 4, 5, 45},
    {1428, 4, 3, 0}, {1429, 4, 3, 0}, {1607, 7, 2, -15}, {1608, 7, 2, -15}, {30, 7, 50, 10}, {31, 7, 50, 10},
    // (s92 ffin) E303's body windows (00battle.bin atkp SubId 0): 1456 spin sphere (react 17, kb 5 / 10), 1737 ground burst (react 11, kb 3 / 60)
    {1456, 17, 5, 10}, {1737, 11, 3, 60},
    // (s89 anti) Anti Form F300-F333 claws / kicks / bursts (00battle.bin atkp SubId 0, tools_s89/anti sorabake bodyhits;
    // Power 0 guard rows 124 / 454 / 455 left out: no hit)
    {456, 3, 75, 0}, {457, 3, 75, 0}, {458, 3, 75, 0}, {459, 3, 125, 0}, {460, 4, 50, 10}, {474, 9, 50, 0},
    {475, 3, 75, 0}, {476, 3, 75, 0}, {477, 9, 4, 0}, {478, 11, 50, 0}, {479, 4, 2, 0}, {480, 4, 2, 0}, {530, 3, 125, 0},
    {531, 4, 2, 0}, {532, 3, 125, 0}, {533, 3, 125, 0}, {534, 3, 75, 0}, {538, 11, 3, 45}, {542, 3, 125, 0},
    {543, 4, 2, 0}, {544, 4, 10, 0}, {545, 4, 2, 0}, {546, 4, 10, 0}, {547, 4, 2, 0}, {548, 3, 125, 0}, {549, 4, 10, 0},
    {550, 4, 2, 0}, {551, 3, 125, 0}, {552, 3, 125, 0}, {553, 9, 4, 0}, {554, 11, 3, 45}, {555, 9, 4, 0}, {556, 11, 5, 45},
    // (s147) Magnet Burst A346 (ability 561, body mset slot 788): 1901 = the pull hits f20..56 (Power 25, react 9 draw-in), 1903 = the
    // burst f64..70 (Power 150, react 11). 00battle.bin atkp SubId 0, read s147 (parse checked vs 126 / 130 / 1059). Without them
    // kb1 was the 0x7fff default = a 327 m rise (user s147: "send enemies flying super high")
    {1901, 9, 20, -2}, {1903, 11, 30, 40},
    // (s147) the other s106 combo-ability windows that had no row (same 0x7fff bug; 00battle.bin SubId 0): A120 125, A315 911 / 912,
    // A316 854, A317 1581, 1869. Power 0 guard rows 161 / 162 left out (no hit)
    {125, 9, 50, 0}, {911, 9, 3, -1}, {912, 11, 15, 30}, {854, 9, 50, -5}, {1581, 8, 75, 0}, {1869, 3, 150, 0},
};
static bool ah_atkp(int atkp, int *react, int *kb1, int *kb2) {
    for (size_t i = 0; i < sizeof AH_ATKP / sizeof *AH_ATKP; i++)
        if (AH_ATKP[i].atkp == atkp) { *react = AH_ATKP[i].react, *kb1 = AH_ATKP[i].kb1, *kb2 = AH_ATKP[i].kb2; return true; }
    return false;
}

static uint64_t ah_rag(uint64_t actor, int *n, uint64_t *arr, uint32_t *flags) {
    if (!actor || !rd_ok(actor + AH_ACTOR_PHYSICS, 8)) return 0;
    uint64_t is = *(const uint64_t *)(uintptr_t)(actor + AH_ACTOR_PHYSICS);
    if (!rd_ok(is + AH_IS_RAGDOLL, 8)) return 0;
    uint64_t rag = *(const uint64_t *)(uintptr_t)(is + AH_IS_RAGDOLL);
    if (!rd_ok(rag, AH_RAG_FLAGS + 4)) return 0;
    *flags = *(const uint32_t *)(uintptr_t)(rag + AH_RAG_FLAGS);
    *n = *(const int32_t *)(uintptr_t)(rag + AH_RAG_NBODY);
    *arr = *(const uint64_t *)(uintptr_t)(rag + AH_RAG_BODIES);
    if (*n <= 0 || *n > 48 || !rd_ok(*arr, (size_t)*n * 8)) return 0;
    return rag;
}
// (s58 lift, S58_HELPER_LIFT 1) The ragdoll's blend state while it is in the world (-1: not in the world). Every BotW hit
// on a Bokoblin starts a PARTIAL powered ragdoll (the enemy ragdoll helper 0x7100a8d160: Actor::startRagdoll, the hit
// impulse, then setRagdollBlendState(instSet, head_dynamic / attack_dynamic = 2 / 3); legs follow the animation, the
// controller keeps the actor) for SmallRagdollTime 8 + SmallRagdollRecoverTime 15 frames. Only BlownOff / death
// (0x7100a8d05c: state 1 full_dynamic, controller weight 0) is the limp ragdoll. An unreadable state counts as full.
static int ah_rag_state(uint64_t actor) {
    int n;
    uint64_t arr;
    uint32_t fl;
    if (!ah_rag(actor, &n, &arr, &fl) || !(fl & AH_RAG_IN_WORLD)) return -1;
    uint64_t is = *(const uint64_t *)(uintptr_t)(actor + AH_ACTOR_PHYSICS);
    return rd_ok(is + AH_IS_BLEND, 1) ? *(const int8_t *)(uintptr_t)(is + AH_IS_BLEND) : 1;
}
static bool ah_ragdolled(uint64_t actor) {  // (ahfull 1: a full ragdoll only; 0: any ragdoll in the world, the s55 rule)
    int s = ah_rag_state(actor);
    return s != -1 && (!g_ah_full || s < 2);
}

// One guest call: RigidBody::setLinearVelocity(body, &v, eps). The game passes eps 0x34000000 (changeWorldState).
static bool ah_body_vel(uint64_t body, const float v[3]) {
    Cpu *c = kernel_current_cpu();
    if (!c || !rd_ok(body, 0xc0) || !dispatch_lookup(AH_RB_SET_LINVEL)) return false;
    Cpu save = *c;
    uint64_t sp = (c->sp - 0x40) & ~0xfull;  // (scratch below the live guest sp, as linkpos_raycast)
    float *g = (float *)(uintptr_t)sp;
    g[0] = v[0], g[1] = v[1], g[2] = v[2], g[3] = 0;
    c->sp = sp, c->x[0] = body, c->x[1] = sp;
    float eps = 1.1920929e-7f;
    uint32_t eb;
    memcpy(&eb, &eps, 4);
    c->v[0] = eb;
    guest_call(c, AH_RB_SET_LINVEL);
    *c = save;
    g_ah_calls++;
    return true;
}
// (s58 lift) One guest call: RigidBody::setAngularVelocity(body, &zero, eps) - the same call shape as setLinearVelocity
// (VERIFIED: NaN check, then the motion accessor's vtable +0x60; RagdollInstance::changeWorldState makes it per body
// with the same body and eps right after setLinearVelocity). Every body moving at one linear velocity with no spin is
// a rigid translation: the ragdoll keeps its pose instead of tumbling (user s58: "spins in a circle really fast").
static bool ah_body_nospin(uint64_t body) {
    Cpu *c = kernel_current_cpu();
    if (!c || !rd_ok(body, 0xc0) || !dispatch_lookup(AH_RB_SET_ANGVEL)) return false;
    Cpu save = *c;
    uint64_t sp = (c->sp - 0x40) & ~0xfull;
    float *g = (float *)(uintptr_t)sp;
    g[0] = g[1] = g[2] = g[3] = 0;
    c->sp = sp, c->x[0] = body, c->x[1] = sp;
    float eps = 1.1920929e-7f;
    uint32_t eb;
    memcpy(&eb, &eps, 4);
    c->v[0] = eb;
    guest_call(c, AH_RB_SET_ANGVEL);
    *c = save;
    g_ah_ang_calls++;
    return true;
}
static int ah_rag_vel(uint64_t actor, const float v[3]) {  // every ragdoll body; the count set (0: not ragdolled)
    int n;
    uint64_t arr;
    uint32_t fl;
    if (!ah_rag(actor, &n, &arr, &fl) || !(fl & AH_RAG_IN_WORLD)) return 0;
    int done = 0;
    for (int i = 0; i < n; i++) {
        uint64_t b = *(const uint64_t *)(uintptr_t)(arr + 8ull * i);
        if (b && ah_body_vel(b, v)) {
            done++;
            if (g_ah_ang) ah_body_nospin(b);  // (s58 lift: ahang)
        } else g_ah_fail++;
    }
    return done;
}

// (s57, DLL-only part) The controller's body: actor -> InstanceSet +0x80 -> CC; its ksys RigidBody is at cc + 0x398 (the
// step call's x0, linkpos.c CC_STEP_LINK_ARG; VERIFIED for Link, the s57 exe log checks it per enemy against [cc+8]).
static int ah_cc_body_vel(uint64_t actor, const float v[3]) {
    if (!actor || !rd_ok(actor + AH_ACTOR_PHYSICS, 8)) return 0;
    uint64_t is = *(const uint64_t *)(uintptr_t)(actor + AH_ACTOR_PHYSICS);
    if (!rd_ok(is + 0x80, 8)) return 0;
    uint64_t cc = *(const uint64_t *)(uintptr_t)(is + 0x80);
    if (!rd_ok(cc + 0x398, 0x80)) return 0;
    return ah_body_vel(cc + 0x398, v) ? 1 : 0;
}

static AhEnt *ah_find(uint64_t actor) {
    for (int i = 0; i < AH_MAX; i++) if (g_ah[i].kind && g_ah[i].actor == actor) return &g_ah[i];
    return NULL;
}

// More than ahair above the floor (or no floor within 5 u).
static bool ah_off_floor(const float epos[3], float to_u) {
    float a[3] = {epos[0], epos[1] + 0.2f, epos[2]}, b[3] = {epos[0], epos[1] - 5.0f, epos[2]}, h[3], nr[3];
    return !linkpos_raycast(a, b, h, nr) || epos[1] - h[1] > g_ah_air_cm * to_u;
}

// Is this enemy airborne for KH2's re:0x3BA9A0 test: one we hold, or a ragdoll (BotW's own blow) off the floor.
static bool ah_airborne(uint64_t actor, const float epos[3], float to_u) {
    const AhEnt *e = ah_find(actor);
    if (e && e->kind != AH_SLIDE) return true;  // (s66: a sliding enemy is on the ground)
    return ah_ragdolled(actor) && ah_off_floor(epos, to_u);
}

// The damage type to send for a hit (blade / spell): a held enemy gets g_ah_held_f54 (BlownOff stays on) unless the
// hit is bigger already.
// (s58 lift) An enemy we hold on its controller would be turned into BotW's full BlownOff ragdoll by a 0x14+ hit (and
// into the partial flinch ragdoll by 0xf / 0x11): every hit on it goes out as ahccheld (7: BotW's Shock stagger, no
// ragdoll - S58_HELPER_LIFT 1); 0 = the old rule (ahheld).
static int ah_f54(uint64_t actor, const float epos[3], int f54, float to_u) {
    if (!ah_enabled() || !ah_airborne(actor, epos, to_u)) return f54;
    AhEnt *e = ah_find(actor);
    if (e && e->cc && g_ah_ccheld_f54 > 0) return g_ah_ccheld_f54;
    return f54 > g_ah_held_f54 ? f54 : g_ah_held_f54;
}
// (s60b ahcchit) The blade's damage type with the KH2 reaction it will start (react = the atkp row's EnemyReaction).
static int ah_hit_f54(uint64_t actor, const float epos[3], int f54, int react, float to_u) {
    int out = ah_f54(actor, epos, f54, to_u);
    if (!ah_enabled() || !g_ah_cchit || g_ah_ccheld_f54 <= 0 || !ah_cc_ok() || !ah_small(actor)) return out;
    int cls = ah_class(react);
    bool air = ah_airborne(actor, epos, to_u);
    bool kh2 = (cls == 3 && g_ah_hop && air) || (cls == 4 && g_ah_blow && (air || g_ah_cchit >= 2));
    if (!kh2 && cls == 3 && !air && g_ah_gfl && g_ah_gf54 > 0) return g_ah_gf54;  // (s66 grounded flinch)
    return kh2 ? g_ah_ccheld_f54 : out;
}

static void ah_start(uint64_t actor, int kind, const float epos[3], const float dir[2], const char *what) {
    AhEnt *e = ah_find(actor);
    for (int i = 0; i < AH_MAX && !e; i++) if (!g_ah[i].kind) e = &g_ah[i];
    if (!e) {  // (full: the oldest)
        e = &g_ah[0];
        for (int i = 1; i < AH_MAX; i++) if (g_ah[i].frames > e->frames) e = &g_ah[i];
    }
    int norag = e->kind && e->actor == actor ? e->norag : 0;  // (a restart keeps the wait for BotW's ragdoll)
    memset(e, 0, sizeof *e);
    e->actor = actor, e->kind = kind, e->what = what, e->norag = norag;
    memcpy(e->base, epos, sizeof e->base);
    e->dir[0] = dir[0], e->dir[1] = dir[1];
    e->last_y = epos[1];
}

// After every Sora hit on an enemy (blade_hits / spell_hit): KH2's reaction class on top of BotW's.
//   from = the attacker point (Sora's feet / the spell), react / kb1 / kb2 = the atkp row's, f54 = what was sent
//   (a BLOW only takes on an enemy BotW blows - f54 >= 0x14 - or one already in the air). form = the ATTACKER is in a Drive form
//   (s194: g_fmots_on for our own hits, the packet's bit for a NET_REACT another game sent; Valor's lower blow).
static void ah_hit_ex(uint64_t actor, const float epos[3], const float from[3], int react, int kb1, int kb2, int f54,
                      float to_u, const char *what, bool form) {
    if (!ah_enabled()) return;
    int cls = ah_class(react);
    float dx = epos[0] - from[0], dz = epos[2] - from[2], d = sqrtf(dx * dx + dz * dz), dir[2] = {0, 0};
    if (d > 1e-3f) dir[0] = dx / d, dir[1] = dz / d;
    bool air = ah_airborne(actor, epos, to_u);
    if (cls == 4 && g_ah_blow && !ah_small(actor)) {
        if (g_ah_log) LOG("[sora-ah] %llx %s: no blow (big enemy / boss, set ahsize %d)\n", (unsigned long long)actor, what, g_ah_size);
    } else if (cls == 4 && g_ah_blow && (air || f54 >= 0x14 || ah_cc_ok())) {  // re:0x3D2FE0 case 4 + re:0x3CEA00
        float rise = (float)kb1, sp = (float)kb2;
        if (!kb1 && !kb2) rise = AH_BLOW_UP, sp = AH_BLOW_SPEED;
        if (form && strcmp(what, "thunder")) rise *= g_ah_form_rise / 100.0f;
        ah_start(actor, AH_BLOW, epos, dir, what);
        AhEnt *e = ah_find(actor);
        e->h = sp, e->vy = -rise, e->hmin = fabsf(sp) * AH_BLOW_MIN;  // (re:0x4228B0)
        if (!strcmp(what, "thunder")) e->hold = g_ah_th_hold;  // (s60b: the hang at the top)
        if (g_ah_log) LOG("[sora-ah] %llx %s: BLOW rise %.0f speed %.0f cm/frame, apex hang %.0f KH frames (react %d, %s) at %.2f %.2f %.2f\n",
                          (unsigned long long)actor, what, rise, sp, e->hold, react, air ? "airborne" : "grounded", epos[0], epos[1], epos[2]);
    } else if (cls == 3 && air && g_ah_hop) {  // re:0x3D2FE0 case 3, airborne: re:0x3D3840 + re:0x3D4800
        float strength = kb1 == 0x7fff ? AH_HIT_BACK_SMALL : (float)kb1;
        ah_start(actor, AH_HOP, epos, dir, what);
        AhEnt *e = ah_find(actor);
        e->jh = g_ah_hop_h, e->apex = sqrtf(2 * g_ah_hop_h / KH_G), e->hv = strength * AH_HOP_SPEED;
        if (g_ah_log) LOG("[sora-ah] %llx %s: HOP %.0f cm, %.1f cm/frame (react %d kb %d) at %.2f %.2f %.2f\n",
                          (unsigned long long)actor, what, e->jh, e->hv, react, kb1, epos[0], epos[1], epos[2]);
    } else if (cls == 3 && !air && g_ah_gfl && !strcmp(what, "blade") && ah_small(actor) && ah_cc_ok()) {
        // (s66) grounded: re:0x3D2FE0 case 3 on the ground = flinch + re:0x3D4960 hit-back (blade only for now)
        float strength = kb1 == 0x7fff ? AH_HIT_BACK_SMALL : (float)kb1;
        if (strength * AH_PUSH_SPEED < AH_PUSH_END || !g_ah_slide) return;
        ah_start(actor, AH_SLIDE, epos, dir, what);
        AhEnt *e = ah_find(actor);
        e->hv = strength * AH_PUSH_SPEED;
        if (g_ah_log) LOG("[sora-ah] %llx %s: SLIDE %.1f cm/frame (react %d kb %d, f54 %#x) at %.2f %.2f %.2f\n",
                          (unsigned long long)actor, what, e->hv, react, kb1, f54, epos[0], epos[1], epos[2]);
    }
}

// (s194 netreact) the map HashId of a placed actor, as linkpos.c actor_hid: *(u32*)(*(u64*)(actor+0x7c8)+0x58); 0 = spawned /
// none (0xFFFFFFFF) / unreadable. Only read while enemy sync runs (net_esync_hit_wanted, s195; net_react_wanted, s194).
static uint32_t ah_actor_hid(uint64_t a) {
    if (!a || !rd_ok(a + 0x7c8, 8)) return 0;
    uint64_t mo = *(const uint64_t *)(uintptr_t)(a + 0x7c8);
    if (!mo || !rd_ok(mo + 0x58, 4)) return 0;
    uint32_t h = *(const uint32_t *)(uintptr_t)(mo + 0x58);
    return h == 0xFFFFFFFFu ? 0 : h;
}

// Every Sora hit's reaction (all call sites: blade, spells, Wisdom, Limit, Magnet / Reflect, Master, summons). (s194 netreact,
// sora_net_react.h) An enemy that is a PUPPET here (another game owns it, ESYNC) gets NO local reaction: ah_tick's servo fought the
// puppet apply (the jitter). Its owner is sent what ah_hit_ex would have used and plays it there. `set netreact 0` = the old way.
// (s195 eshit) Every Sora hit is also noted for the enemy sync's hit claim (net_esync_note_hit), before the airhold / netreact checks.
// (s195b) ORDER MATTERS: a hit that qualifies claims the enemy inside the note, so the netreact question after it sees owner == me
// (not a puppet: nr_puppet) and this claiming hit's reaction is played HERE (ah_hit_ex), no NET_REACT to the old owner. A hit that does
// not qualify (owner HOT, banned, not loaded, eshit 0) goes the s194 way. The hid is read once for both.
static void ah_hit(uint64_t actor, const float epos[3], const float from[3], int react, int kb1, int kb2, int f54,
                   float to_u, const char *what) {
    bool es = net_esync_hit_wanted(), nr = net_react_wanted();
    uint32_t hid = es || nr ? ah_actor_hid(actor) : 0;
    if (es) net_esync_note_hit(hid);
    if (!ah_enabled()) return;
    if (nr && net_react_hit(hid, from, react, kb1, kb2, f54, what, g_fmots_on)) return;
    ah_hit_ex(actor, epos, from, react, kb1, kb2, f54, to_u, what, g_fmots_on);
}

// (s194 netreact) The owner's half: the reactions other games sent for OUR enemies, played with their values in the frame body right
// before ah_tick (the same place and guest-call window as our own hits). The enemy list is read once, only when one is owned here.
typedef struct { float to_u; int n; uint64_t actor[64]; float pos[64][3]; int32_t life[64]; } AhNetPlay;
static bool ah_net_find(uint32_t hid, uint64_t *actor, float epos[3], int32_t *life, void *user) {
    AhNetPlay *p = (AhNetPlay *)user;
    if (p->n < 0) p->n = linkpos_enemies(p->actor, p->pos, p->life, 64);
    for (int i = 0; i < p->n && i < 64; i++)
        if (ah_actor_hid(p->actor[i]) == hid) {
            *actor = p->actor[i], *life = p->life[i];
            memcpy(epos, p->pos[i], sizeof p->pos[i]);
            return true;
        }
    return false;
}
static void ah_net_one(uint64_t actor, const float epos[3], const NetReactEnt *e, void *user) {
    const AhNetPlay *p = (const AhNetPlay *)user;
    ah_hit_ex(actor, epos, e->from, e->react, e->kb1, e->kb2, e->f54, p->to_u, net_react_what(e->kind), (e->flags & NRF_FORM) != 0);
}
static int ah_net_play(float to_u) {  // (s195 reactidle: returns how many were played; the driving path ignores it)
    if (!net_react_pending()) return 0;
    static AhNetPlay p;
    p.to_u = to_u, p.n = -1;
    return net_react_play(GetTickCount64(), ah_net_find, ah_net_one, &p);
}

// One KH frame-span of the curve: advances e->t by kh and returns the offset (cm) in e->cx / e->cy.
static void ah_curve(AhEnt *e, float kh) {
    if (e->kind == AH_BLOW) {  // re:0x422780 in AH_STEP sub-steps (PHH hit_reaction_kh2.Blow, constants VERIFIED in 03system)
        float left = kh;
        while (left > 1e-4f) {
            float dt = left < AH_STEP ? left : AH_STEP, k = dt / AH_STEP;
            if (e->hold > 0 && e->vy >= 0) {  // (s60b ahthhold) past the apex: hang there first (not KH2: user wish)
                float use = dt < e->hold ? dt : e->hold;
                e->hold -= use, left -= use;
                continue;
            }
            float brake = powf(AH_BLOW_BRAKE, k);
            e->h = fmaxf(e->h * brake, e->hmin);
            if (e->vy < 0) e->vy *= brake;
            e->vy = fminf(e->vy + KH_G * k, KH_FALL_MAX);  // (KH_G per sub-step, as PHH; KH y down)
            e->cx += e->h * dt, e->cy -= e->vy * dt;
            left -= dt;
        }
    } else if (e->kind == AH_SLIDE) {  // re:0x3D4960: per KH frame move hv, then hv x 0.9 (whole frames + the rest)
        float left = kh;
        while (left > 1e-4f) {
            float dt = left < 1.0f ? left : 1.0f;
            e->cx += e->hv * dt, e->hv *= powf(AH_PUSH_BRAKE, dt);
            left -= dt;
        }
    } else {  // HOP = ACTION_JUMP from here (sora.c arc_at's curve, re:0x3CB360): sin-sin rise, then g, FallMax
        float t1 = e->t + kh, y;
        if (t1 <= e->apex) y = e->jh * sinf(sinf(t1 / e->apex * PI * 0.5f) * PI * 0.5f);
        else {
            float tf = t1 - e->apex, tmax = KH_FALL_MAX / KH_G;  // (fall speed reaches FallMax after tmax frames)
            y = tf <= tmax ? e->jh - KH_G * tf * tf * 0.5f : e->jh - KH_G * tmax * tmax * 0.5f - KH_FALL_MAX * (tf - tmax);
        }
        e->cy = y;
        float steps = kh / AH_STEP;  // (horizontal: the jump start's speed, braked per physics step)
        e->cx += e->hv * kh, e->hv *= powf(AH_AIR_BRAKE, steps);
    }
    e->t += kh;
}

// Once a frame from sora_frame (after the hits): servo every held enemy's ragdoll to its curve.
static void ah_tick(float kh, float to_u) {
    if (!ah_enabled() || kh <= 0 || *(volatile uint32_t *)(uintptr_t)PAUSE_STATE) return;
    uint64_t actor[64];
    float pos[64][3];
    int32_t life[64];
    int n = -1;
    for (int k = 0; k < AH_MAX; k++) {
        AhEnt *e = &g_ah[k];
        if (!e->kind) continue;
        if (n < 0) n = linkpos_enemies(actor, pos, life, 64);
        int i = 0;
        while (i < n && actor[i] != e->actor) i++;
        const char *why = NULL;
        // (s58 lift) rs = the ragdoll's blend state (-1: none in the world). BotW's partial hit flinch (rs >= 2) stays on
        // the controller (it carries the flinching body up: KH2's hurt pose); only a full ragdoll (or any ragdoll with
        // no controller lift: ahcc 0 / old exe, or ahfull 0) gets the body servo.
        int rs = i < n ? ah_rag_state(e->actor) : -1;
        bool cc_ok = i < n && ah_cc_ok();
        bool rag = i < n && rs != -1 && (!g_ah_full || rs < 2 || !cc_ok), cc = i < n && !rag && cc_ok;
        if (cc && rs >= 2 && !e->flinch_seen && g_ah_log) {
            e->flinch_seen = true;
            LOG("[sora-ah] %llx: BotW hit flinch (partial ragdoll, blend state %d): staying on the controller\n",
                (unsigned long long)e->actor, rs);
        }
        if (i == n) why = "gone";
        else if (g_ah_dead && life[i] <= 0) {  // (s60b ahdead) killed: no more servo
            if (e->cc && g_ah_evel) g_ah_evel(e->actor, false, 0, 0, 0), e->cc = false;
            if (rs == -1 && ++e->dead_wait <= 10) { e->frames++; continue; }  // (BotW's death ragdoll not in yet)
            if (rs != -1 && g_ah_dead == 1 && e->dead_zero < g_ah_dead_n) {
                const float zero[3] = {0, 0, 0};
                int nb = ah_rag_vel(e->actor, zero);
                if (!e->dead_zero++ && g_ah_log)
                    LOG("[sora-ah] %llx killed: death ragdoll (state %d, %d bodies) zeroed for %d frames, then BotW's\n",
                        (unsigned long long)e->actor, rs, nb, g_ah_dead_n);
                e->frames++;
                continue;
            }
            why = rs == -1 ? "killed (no death ragdoll in 10 frames)" : "killed (BotW's death ragdoll falls on its own)";
        }
        else if (!rag && !cc && ++e->norag > 8) why = "no ragdoll (BotW did not blow it; exe slice 2 lifts standing enemies)";
        if (!why && (rag || cc) && e->t == 0) memcpy(e->base, pos[i], sizeof e->base), e->last_y = pos[i][1];  // (curve starts)
        if (!why && (rag || cc)) {
            e->norag = 0;
            float dt = kh / KH_FPS, y_now = pos[i][1];
            // landed: the curve asked to go down but the body stayed (as Sora's landing check, sora.c LAND_GOT)
            float got = y_now - e->last_y;
            // (s57: and the floor is near - a ragdoll lagging the servo after the apex looked "landed" 1.8 m up)
            if (e->last_req < -0.02f && got > e->last_req * LAND_GOT && !ah_off_floor(pos[i], to_u)) e->still++;
            else e->still = 0;
            if (e->still >= 2) why = "landed";
            // (s58 lift, live_s58 72ede678f8: a Chuchu killed by the Thunder stopped at the apex - BotW's die call stops
            // its controller, "7 own steps" - while the curve fell on, then "landed" by the clock) the controller-lifted
            // enemy has not moved for ahfrz frames although the curve asked for > 3 cm: BotW owns it now, let go
            if (e->cc && fabsf(e->last_req) > 0.03f && fabsf(got) < 0.005f) e->frozen++;
            else e->frozen = 0;
            if (!why && g_ah_frz > 0 && e->frozen >= g_ah_frz)
                why = life[i] <= 0 ? "frozen (killed: BotW's death stopped its controller)" : "frozen (the controller stopped moving)";
            float cy0 = e->cy;
            ah_curve(e, kh);
            float tgt[3] = {e->base[0] + e->dir[0] * e->cx * to_u, e->base[1] + e->cy * to_u, e->base[2] + e->dir[1] * e->cx * to_u};
            if (e->kind == AH_SLIDE) {  // (s66) on the floor under the target spot (the servo replaces the game's gravity)
                float a[3] = {tgt[0], pos[i][1] + 0.3f, tgt[2]}, b[3] = {tgt[0], pos[i][1] - 2.0f, tgt[2]}, h[3], nr[3];
                tgt[1] = !g_ah_sray ? pos[i][1] : linkpos_raycast(a, b, h, nr) ? h[1] : pos[i][1] - 0.3f;
                if (e->hv < AH_PUSH_END) why = "slid";
            }
            float v[3];
            for (int c = 0; c < 3; c++) {
                v[c] = (tgt[c] - pos[i][c]) / dt;
                // (s58, user: Thunder "spins them in a circle really fast" while ragdolled; log: x/z flipping -31..+30
                // u/s) the sideways servo on ragdolls x g_ah_hgain % (live "set ahhgain", 100 = the s55 servo)
                if (c != 1 && rag) v[c] *= g_ah_hgain / 100.0f;
                if (e->kind == AH_SLIDE) v[c] *= g_ah_slv / 100.0f;
                v[c] = fmaxf(-g_ah_vmax, fminf(g_ah_vmax, v[c]));
            }
            float err = hypotf(hypotf(tgt[0] - pos[i][0], tgt[2] - pos[i][2]), tgt[1] - pos[i][1]);
            if (err > 1.5f && ++e->stuck > 10) why = "stuck (servo error > 1.5 u for 10 frames)";
            else if (err <= 1.5f) e->stuck = 0;
            if (e->t > 240) why = "timeout";
            // (s57 live: a blow's ragdoll root stops ~1.5 u above its start while the curve falls on below it, then
            // "stuck"; KH2's blow ends at the floor = the start height) the curve back at / under the start: let go
            if (!why && e->kind == AH_BLOW && e->t > 4 && e->cy < 0) why = "landed (curve back at the start height)";
            if (!why) {
                int nb = -1;
                if (rag) {
                    if (e->cc) g_ah_evel(e->actor, false, 0, 0, 0), e->cc = false;  // (BotW ragdolled it meanwhile)
                    nb = ah_rag_vel(e->actor, v);
                } else {
                    g_ah_evel(e->actor, true, v[0], v[1], v[2]), e->cc = true;
                    if (g_ah_rbreq > 0) nb = ah_cc_body_vel(e->actor, v) ? -3 : -2;  // (s57 DLL request: -3 sent, -2 failed)
                }
                e->last_req = (e->cy - cy0) * to_u, e->last_y = y_now;
                if (g_ah_log >= 2 || (g_ah_log && e->frames % 6 == 0))
                    LOG("[sora-ah] %llx %s t %.1f: at %.2f %.2f %.2f target %.2f %.2f %.2f (curve %+.0f %+.0f cm) v %.1f %.1f %.1f "
                        "u/s, %d bodies, life %d, rag state %d\n", (unsigned long long)e->actor, e->kind == AH_BLOW ? "blow" : e->kind == AH_SLIDE ? "slide" : "hop",
                        e->t, pos[i][0], pos[i][1], pos[i][2], tgt[0], tgt[1], tgt[2], e->cx, e->cy, v[0], v[1], v[2], nb, life[i], rs);
            }
        }
        e->frames++;
        if (why) {
            if (g_ah_log) LOG("[sora-ah] %llx %s end: %s (t %.1f, %d frames; %llu body calls, %llu failed, %llu spin calls)\n",
                              (unsigned long long)e->actor, e->kind == AH_BLOW ? "blow" : e->kind == AH_SLIDE ? "slide" : "hop", why, e->t, e->frames,
                              (unsigned long long)g_ah_calls, (unsigned long long)g_ah_fail, (unsigned long long)g_ah_ang_calls);
            if (e->cc && g_ah_evel) g_ah_evel(e->actor, false, 0, 0, 0);
            e->kind = AH_FREE, e->cc = false;
        }
    }
}

// (s195 reactidle, sora_net_react.h nr_idle_decide) The frame body's NOT-driving branch (Link has the pad: our Sora dead / the Game
// Over screen / the post-Continue hold, climb, swim, glide, an event, Slate mode, the bed / wake-up puppet) returned before ah_net_play
// + ah_tick, so reactions other games sent expired "late" and a running servo stopped (its controller override kept its last
// velocity). With live `set reactidle 1` that branch calls this instead, once a frame, right before its return (the driving path's
// call at the end of the body is never reached on such a frame: never twice). Same game-thread window as the driving path
// (sora_mod_frame runs inside linkpos_frame either way); every actor touched comes from this frame's linkpos_enemies. Not while
// loading (g_loading), with no world frame / no Link body (those return earlier anyway), or BotW paused (ah_tick skips it too).
// Default 0 = the old behaviour exactly (nothing runs there).
static bool g_ah_react_idle = true;  // (s196: default 1, user verified)
static NrIdleLog g_ah_idle_log;
static void ah_idle_frame(bool world, bool loading, uint32_t sw, bool dead, float kh, float to_u) {
    if (nr_idle_decide(g_ah_react_idle, world, loading, *(volatile uint32_t *)(uintptr_t)PAUSE_STATE != 0) != NRI_RUN) return;
    int played = ah_net_play(to_u);
    if (nr_idle_log_due(&g_ah_idle_log, true, played) && (g_ah_log || g_net_react))
        LOG("[react] played %d reaction%s while not driving (Link state %#x%s): the reactidle path ran\n", played, played == 1 ? "" : "s", sw,
            dead ? ", Sora dead" : "");
    ah_tick(kh, to_u);
}
// the driving path, every frame: the not-driving stretch is over (the next one logs its first play again)
static void ah_idle_end(void) { nr_idle_log_due(&g_ah_idle_log, false, 0); }

static bool ah_live_set(const char *key, int v) {
    if (!strcmp(key, "ah")) g_ah_on = v != 0;
    else if (!strcmp(key, "reactidle")) {  // (s195 reactidle)
        if (g_ah_react_idle == (v != 0)) return true;
        g_ah_react_idle = v != 0;
        LOG("[react] play while not driving %s\n", g_ah_react_idle ? "on" : "off");
    }
    else if (!strcmp(key, "ahlog")) g_ah_log = v;
    else if (!strcmp(key, "ahhop")) g_ah_hop = v != 0;
    else if (!strcmp(key, "ahblow")) g_ah_blow = v != 0;
    else if (!strcmp(key, "ahheld")) g_ah_held_f54 = v;
    else if (!strcmp(key, "ahhoph")) g_ah_hop_h = (float)v;
    else if (!strcmp(key, "ahthr")) g_ah_thunder_rise = (float)v;
    else if (!strcmp(key, "ahvmax")) g_ah_vmax = (float)v;
    else if (!strcmp(key, "ahair")) g_ah_air_cm = (float)v;
    else if (!strcmp(key, "ahcc")) g_ah_cc = v != 0;
    else if (!strcmp(key, "ahlf54")) g_ah_launch_f54 = v;
    else if (!strcmp(key, "ahlf40")) g_ah_launch_f40 = v;
    else if (!strcmp(key, "ahsize")) g_ah_size = v;
    else if (!strcmp(key, "ahhgain")) g_ah_hgain = (float)v;
    else if (!strcmp(key, "ahfull")) g_ah_full = v != 0;  // (s58 lift)
    else if (!strcmp(key, "ahcchit")) g_ah_cchit = v;     // (s60b)
    else if (!strcmp(key, "ahdead")) g_ah_dead = v;
    else if (!strcmp(key, "ahdeadn")) g_ah_dead_n = v;
    else if (!strcmp(key, "ahthhold")) g_ah_th_hold = (float)v;
    else if (!strcmp(key, "ahformrise")) g_ah_form_rise = v;  // (s70)
    else if (!strcmp(key, "ahccheld")) g_ah_ccheld_f54 = v;
    else if (!strcmp(key, "ahang")) g_ah_ang = v != 0;
    else if (!strcmp(key, "ahfrz")) g_ah_frz = v;
    else if (!strcmp(key, "ahgfl")) g_ah_gfl = v;    // (s66 grounded flinch)
    else if (!strcmp(key, "ahgf54")) g_ah_gf54 = v;
    else if (!strcmp(key, "ahslv")) g_ah_slv = v;
    else if (!strcmp(key, "ahsray")) g_ah_sray = v;
    else if (!strcmp(key, "ahslide")) g_ah_slide = v;
    else if (!strcmp(key, "ahrbreq")) g_ah_rbreq = v != 0;
    else if (!strcmp(key, "ahreq") || !strcmp(key, "ahsnap")) {
        ah_cc_ok();
        if (!g_ah_evel_opts) { LOG("[sora-ah] %s: this exe has no linkpos_enemy_vel_opts (s57 exe fix not linked)\n", key); return true; }
        g_ah_evel_opts(!strcmp(key, "ahreq") ? v : -1, !strcmp(key, "ahsnap") ? v : -1);
    }
    else return false;
    if (!v && !strcmp(key, "ah")) {
        for (int k = 0; k < AH_MAX; k++) if (g_ah[k].kind && g_ah[k].cc && g_ah_evel) g_ah_evel(g_ah[k].actor, false, 0, 0, 0);
        memset(g_ah, 0, sizeof g_ah);
    }
    return true;
}

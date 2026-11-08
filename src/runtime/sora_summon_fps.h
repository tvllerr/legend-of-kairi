// (s137 fps) Chicken Little's summon Limit "FPS Mode" (KH2 cmd 529; the face-button commands 530 Balls / 531 Firecracker / 532 End).
// Included from sora_summon.h (after sm_enemies / sm_damage). Notes, sources, VERIFIED / GUESS list, tests: recon/S137_HELPER_FPS.md.
//
// KH2 truth used (re-checked this session against KH2's OWN files; "PHH" was only a hint where to look):
//   VERIFIED in KH2FM.exe (Ghidra, sha1 2548F119): kn::ChickenLittle start_limit rva 1CB6C0, think 1CB830, end_limit 1CB180, FPS camera (field cam mode 9)
//     178760 / 178810 / 1784A0 / 178580 / 1787E0; constants read from the exe image: eye height 100.0 (0x1CB6C0 stores 0x42C80000), yaw step 0.05236 rad/frame
//     (rva 5B22FC), screen margin 0.17453 (5B2300), pitch limits -0.5236 / +1.2217 (71AA80/84), look-at distance 100 (5B2318), no-target shoot point = view
//     eye + view z axis * 300 (623BF8), face-button menu kind 5 table (5CC488): 531 -> button 2, 532 -> 3, 530 -> 4 (the names Triangle / Square / decide
//     are PHH's: GUESS; for language 0 / 7 the exe uses 5CC478 instead: 530 -> 1, 532 -> 2, 531 -> 3).
//   VERIFIED in KH2's P_EX350.mdlx script data (D:/KH2Extract/kh2_sixth/original/obj): Balls cooldown 10 f, Firecracker cooldown 60 f, ball life 24 f, speed 50 -0.5/f to 30,
//     homing rate 0.087266 -0.008727/f to 0.017453, stop within 50 cm, wait 6 f, fade 8, pick box 120 x 60, intro aim 200 ahead / 160 up, firecracker count 3 + rand(6),
//     gap rand(4..12) f. Intro = Sora capture 253 + Chicken motion 241 (66 f each), bind 0x4002.
//   VERIFIED in 03system.bin / 00battle.bin: cmd 529 Cost 250 Time 300 (Execute 81), 530-532 Cost 0; sstm ChangeTimeAddRate 12.0 (@0x1840C) -> the cost is 250 * 12 = 3000
//     gauge frames; atkp 1444 (Balls) Power 25 reaction 8 effect-on-hit 1; atkp 1261 (Firecracker) Power 1 Interval 8 reaction 18; gravity 0.40816 cm/f^2, fall cap 16 (exe 5CE184 / sys 183B0).
// Keys: sfps 1 (ONE-SHOT start; remove from the file afterwards), sfcmd 530|531|532 (ONE-SHOT), fpslog 0|1, fpscam 0|1 (1 = KH2's first-person camera [needs the s137 exe
//   export linkpos_camera_set, else the BotW camera + a log line]; 0 = BotW camera, Sora + Chicken stay drawn), tuning: fpsrad (enemy hit radius cm, 80), fpsyaw (+1 / -1: the
//   right stick's left/right sign), fpspitch (+1 / -1), fpsturn (percent of KH2's 0.05236 rad/f, 100), fpstime (Limit Time frames, 300), fpsfov (camera horizontal fov for KH2's
//   aim clamp, 1.5), fpseye (eye height cm, 100), fpsretic (0|1).
#pragma once
#define SM_FPS_API  // (s137 merge) the real FPS API: runtime/sora_summon_fps_stub.h (smenu) steps aside
// (s138 ohana) Stitch's Limit (sora_summon_ohana.h, included after this file) answers through the same API: a few one-line hooks below, all inert while OH.st == 0 and for Chicken Little's row
static int oh_st(void);
static bool oh_late(float kh, float to_u, int st);
static bool oh_can(void);
static void oh_request(void);
static bool oh_cmd_ok(int cmd);
static void oh_abort(const char *why);
static void oh_input(uint64_t *press);
// (s139 neverland) Peter Pan's Limit (sora_summon_nl.h) answers through the same API
static int nl_st(void);
static bool nl_late(float kh, float to_u, int st);
static bool nl_can(void);
static void nl_request(void);
static bool nl_cmd_ok(int cmd);
static void nl_abort(const char *why);
static void nl_input(uint64_t *press);
static int gl_st(void);   // (s139 genielim)
static bool gl_late(float kh, float to_u, int st);
static bool gl_can(void);
static void gl_request(void);
static void gl_input(uint64_t *press);
static void gl_abort(const char *why);

#define FP_COST_UNITS 250        // VERIFIED cmd 529 Cost
#define FP_RATE 12.0f             // VERIFIED sstm ChangeTimeAddRate
#define FP_TIME_DEF 300.0f        // VERIFIED cmd 529 Time (frames, 1 per KH frame); GUESS (PHH's mset table, not re-parsed): the intro's range 45 on slot 253 holds it
#define FP_BALLS_COOL 10.0f
#define FP_FIRE_COOL 60.0f
#define FP_INTRO_FRAMES 66.0f     // 33 event frames * 2; motions 253 / 241 are 66 f
#define FP_BALL_LIFE 24.0f
#define FP_BALL_SPEED 50.0f
#define FP_BALL_ACCEL (-0.5f)
#define FP_BALL_MIN 30.0f
#define FP_BALL_HOMING 0.087266f
#define FP_BALL_HOMING_D (-0.008727f)
#define FP_BALL_HOMING_MIN 0.017453f
#define FP_BALL_STOP_CM 50.0f
#define FP_BALL_WAIT 6.0f
#define FP_BALL_FADE 8.0f
#define FP_BALL_FREE_AFTER 4.0f
#define FP_BALL_RADIUS_CM 40.0f   // trap_attack_set_radius(atk, 40) (PHH-read from the script; the data word is in the mdlx but its role was not re-derived: GUESS-ish)
#define FP_BALL_POWER 25
#define FP_PICK_X 120.0f
#define FP_PICK_Y 60.0f
#define FP_FOCAL 343.5f           // 320 / tan(1.5 / 2): KH2's screen units for the pick box (fov_h 1.5 on 640: PHH hint, exe rva 718C00 = 1.5 VERIFIED as a value)
#define FP_FIRE_POWER 1
#define FP_FIRE_INTERVAL 8.0f
#define FP_FIRE_REACH_CM 240.0f
#define FP_FIRE_BLAST 40.0f
#define FP_FIRE_AHEAD 200.0f
#define FP_FIRE_SPREAD 0.436332f
#define FP_GRAV 0.40816327929496765f
#define FP_FALL_MAX 16.0f
#define FP_EYE_DEF 100.0f
#define FP_TURN 0.05235987901687622f
#define FP_MARGIN 0.1745329201221466f
#define FP_PITCH_MIN (-0.5235987901687622f)
#define FP_PITCH_MAX 1.2217304706573486f
#define FP_LOOK_CM 100.0f
#define FP_SHOOT_AHEAD_CM 300.0f
#define BTN_R3_GUESS (1ull << 5)  // GUESS: Switch R-stick press (KH2 R3 = re-aim at the shoot target)
#define FP_NBALL 8
#define FP_NFIRE 16
#define FP_SEEN 32

typedef struct { bool on, homing, atk, hit; float p[3], d[3], speed, rate, wait, life, fade, free_in; uint64_t tgt; int fx, steps; } FpBall;
typedef struct {
    bool on, landed;
    float p[3], face, speed, accel, life, base, jh, air, tapex, apex;  // cm
    float blast;                                                         // -1 = not burst, else frames left
    int fx, bfx;
} FpFire;

static struct {
    int log, cam, retic, yawdir, pitchdir, turn_pct;
    float rad_cm, time0, fov_h, eye_cm;
    int st;                         // 0 off, 1 intro, 2 FPS
    int req, cmd_req;               // 1 = start asked; cmd_req = 530..532 pending
    bool end_req;
    float time, cool_b, cool_f, ic, acc, gauge_spent;
    float yaw, pitch;               // KH2's +0xC4 / +0xC8: yaw = atan2(x, z) in the mod's frame, pitch > 0 looks up
    bool cam_on, retic_on, hide_ok;
    uint64_t tgt;                   // the R1 target (0 = none)
    uint64_t seen[FP_SEEN];
    int nseen;
    float shoot[3];                 // world units
    int last_st;
    int n_balls, n_fire_spawned, n_hits;
    int fire_left;                  // firecrackers still to throw
    float fire_gap;
    FpBall ball[FP_NBALL];
    FpFire fc[FP_NFIRE];
    float fc_last[64];              // per-enemy firecracker timers (hash by actor)
    uint64_t fc_act[64];
    bool pr_r1, pr_atk, pr_tri, pr_sq, pr_r3;  // edges handed over by sm_fps_input (sora.c)
    float rsx, rsy;                 // the right stick, -1..1
    void (*camset)(bool, const float *, const float *, const float *);
    int (*camstate)(void);          // linkpos_camera_override_state: 0 off, 1 not confirmed, 2 kept by BotW's frame, 3 lost
    bool tried, cam_failed;         // cam_failed: the exe write did not survive BotW's frame -> BotW camera fallback (fpscam 1 retries)
    float last_feet[3];             // (S3: the abort puts Chicken Little back beside Sora)
    bool have_feet;
} FP ={.cam = 1, .retic = 1, .yawdir = 1, .pitchdir = 1, .turn_pct = 100, .rad_cm = 80.0f, .time0 = FP_TIME_DEF, .fov_h = 1.5f, .eye_cm = FP_EYE_DEF};

#define FLOG(...) do { if (FP.log) LOG(__VA_ARGS__); } while (0)

static bool sm_fps_on(void) { return FP.st == 2 || oh_st() == 2 || nl_st() == 2; }
static bool sm_fps_busy(void) { return FP.st != 0 || oh_st() != 0 || nl_st() != 0 || gl_st() != 0; }             // intro or FPS: the summon's end waits (KH2: the Limit holds the summon end cmd)
// (review M4) the first-person view is LIVE only when the exe confirmed that the write survives BotW's frame (state 2); until then / when lost,
// Sora + Chicken stay drawn and the right stick stays BotW's
static bool fp_cam_live(void) { return FP.cam_on && !FP.cam_failed && FP.camstate && FP.camstate() == 2; }
static bool sm_fps_hide_sora(void) { return FP.st == 2 && fp_cam_live(); }  // KH2 draws Sora + Chicken at alpha 0 in FPS Mode (re:0x1CB6C0 -> 3B7810)
static bool sm_fps_eat_x(void) { return FP.st == 2 || oh_st() == 2 || nl_st() == 2 || gl_st() != 0; }            // Triangle = Firecracker
static bool sm_fps_eat_rs(void) {  // the right stick is the FPS camera's, BotW's camera gets none (review M1: never in a pause / while Link has the pad)
    return FP.st == 2 && fp_cam_live() && !g_link_owns && !*(volatile uint32_t *)(uintptr_t)PAUSE_STATE;
}
static bool sm_fps_view(float cf[2]) {  // (review S4) sora.c's walk: camera-relative to the FPS view while it is live
    if (FP.st != 2 || !fp_cam_live()) return false;
    cf[0] = sinf(FP.yaw), cf[1] = cosf(FP.yaw);
    return true;
}

static float fp_wrap(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static float fp_rnd(void) { return (float)(rand() & 0x7fff) / 32768.0f; }
static float fp_rr(float a, float b) { return a + (b - a) * fp_rnd(); }
static float fp_len3(const float *a) { return sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }
static void fp_norm3(float *a) { float l = fp_len3(a); if (l > 1e-6f) a[0] /= l, a[1] /= l, a[2] /= l; }

static float fp_uptime(float h) { return sqrtf(fabsf(2.0f * h / FP_GRAV)); }  // trap_obj_jump_height_to_uptime

// ---- the camera (KH2 field camera mode 9, rva 178810 / 1784A0 / 178580): all in the mod's world frame, cm -> world by to_u
static void fp_fwd(float f[3]) {
    float cp = cosf(FP.pitch);
    f[0] = cp * sinf(FP.yaw), f[1] = sinf(FP.pitch), f[2] = cp * cosf(FP.yaw);
}
static void fp_basis(float fwd[3], float right[3], float up[3]) {
    fp_fwd(fwd);
    float wu[3] = {0, 1, 0};
    right[0] = fwd[1] * wu[2] - fwd[2] * wu[1], right[1] = fwd[2] * wu[0] - fwd[0] * wu[2], right[2] = fwd[0] * wu[1] - fwd[1] * wu[0];  // fwd x up = RIGHT in a y-up right-handed frame? verified below by a test, fpsyaw flips the stick only
    // fwd x up points to the viewer's LEFT in a right-handed y-up world (x right, z toward the viewer); the mod's frame has +x on Sora's LEFT at face 0 (left = (fz, -fx)), so
    // fwd x up is the mod's RIGHT: keep as is. GUESS: checked on paper, not in game (test list item 2).
    fp_norm3(right);
    up[0] = right[1] * fwd[2] - right[2] * fwd[1], up[1] = right[2] * fwd[0] - right[0] * fwd[2], up[2] = right[0] * fwd[1] - right[1] * fwd[0];
}
static void fp_eye(const float feet[3], float to_u, float e[3]) { e[0] = feet[0], e[1] = feet[1] + FP.eye_cm * to_u, e[2] = feet[2]; }

// 1784A0: look straight at a point (world units)
static void fp_aim_at(const float eye[3], const float p[3]) {
    float dx = p[0] - eye[0], dy = p[1] - eye[1], dz = p[2] - eye[2];
    FP.yaw = atan2f(dx, dz);
    FP.pitch = atan2f(dy, hypotf(dx, dz));
}

// 178580: keep the shoot target on screen (only matters with an R1 target: without one the shoot point follows the camera)
static void fp_clamp_to(const float eye[3], const float p[3]) {
    float dx = p[0] - eye[0], dy = p[1] - eye[1], dz = p[2] - eye[2];
    float ty = atan2f(dx, dz), tp = atan2f(dy, hypotf(dx, dz));
    float lim = FP.fov_h * 0.5f - FP_MARGIN, d = fp_wrap(FP.yaw - ty);
    if (d < -lim) FP.yaw = fp_wrap(ty - lim);
    else if (d > lim) FP.yaw = fp_wrap(ty + lim);
    // vertical half fov from KH2's 640 x 480 view: 2 atan(240 / focal) / 2 (GUESS: the exe reads the live camera's entry 1)
    float limv = atanf(240.0f / FP_FOCAL) - FP_MARGIN;
    d = fp_wrap(FP.pitch - tp);
    if (d < -limv) FP.pitch = fp_wrap(tp - limv);
    else if (d > limv) FP.pitch = fp_wrap(tp + limv);
}

static void fp_cam_off(void);
static void fp_cam_apply(const float feet[3], float to_u) {
    if (FP.cam && FP.camset && !FP.camstate && !FP.cam_failed)
        FP.cam_failed = true, LOG("[summon] fps: the exe has linkpos_camera_set but no linkpos_camera_override_state: BotW's camera is kept\n");
    if (!FP.cam || !FP.camset || FP.cam_failed) {
        if (FP.cam && !FP.camset && !FP.tried) FP.tried = true, LOG("[summon] fps: the exe has no linkpos_camera_set (s137 exe not linked): BotW's camera is kept\n");
        if (FP.cam_on) fp_cam_off();
        return;
    }
    if (FP.cam_on && FP.camstate() == 3) {  // (review M4) BotW rewrites the camera after every write of ours: BotW camera fallback, once per session
        FP.cam_failed = true;
        fp_cam_off();
        LOG("[summon] fps: the exe camera write does not survive BotW's frame (linkpos: LOST): BotW camera fallback, Sora + Chicken stay drawn, the right stick "
            "is BotW's (live 'set fpscam 1' retries)\n");
        return;
    }
    float e[3], f[3], r[3], u[3];
    fp_eye(feet, to_u, e);
    fp_basis(f, r, u);
    float k = FP_LOOK_CM * to_u, at[3] = {e[0] + f[0] * k, e[1] + f[1] * k, e[2] + f[2] * k}, up[3] = {0, 1, 0};
    FP.camset(true, e, at, up);
    FP.cam_on = true;
}
static void fp_cam_off(void) {
    if (FP.cam_on && FP.camset) FP.camset(false, NULL, NULL, NULL);
    FP.cam_on = false;
}

// KH2 screen units (640 wide, y up) of a point relative to the view centre (rva 1CB360: view x, y * focal / z)
static bool fp_screen(const float eye[3], const float p[3], float *sx, float *sy) {
    float f[3], r[3], u[3];
    fp_basis(f, r, u);
    float d[3] = {p[0] - eye[0], p[1] - eye[1], p[2] - eye[2]};
    float z = d[0] * f[0] + d[1] * f[1] + d[2] * f[2];
    if (z < 1e-4f) return false;
    *sx = (d[0] * r[0] + d[1] * r[1] + d[2] * r[2]) / z * FP_FOCAL, *sy = (d[0] * u[0] + d[1] * u[1] + d[2] * u[2]) / z * FP_FOCAL;
    return true;
}
static bool fp_on_screen(const float eye[3], const float p[3]) {  // "culled" (rva 3BA4A0: not drawn) = off the 640 x 480 view. GUESS: the exe asks the draw system
    float sx, sy;
    return fp_screen(eye, p, &sx, &sy) && fabsf(sx) <= 320.0f && fabsf(sy) <= 240.0f;
}

static void fp_center(const SmEn *e, float to_u, float c[3]) { c[0] = e->pos[0], c[1] = e->pos[1] + 90.0f * to_u, c[2] = e->pos[2]; }  // GUESS: chest height 90 cm

// ---- R1: the FPS think's target cycling (rva 1CB830): first visible, not-yet-seen part; else cycle the seen list from after the old target
static int fp_find(const SmEn *en, int n, uint64_t a) { for (int k = 0; k < n; k++) if (en[k].actor == a && en[k].life > 0) return k; return -1; }
static int fp_seen_idx(uint64_t a) { for (int i = 0; i < FP.nseen; i++) if (FP.seen[i] == a) return i; return -1; }
static void fp_seen_add(uint64_t a, const SmEn *en, int n) {
    int w = 0;
    for (int i = 0; i < FP.nseen; i++) if (FP.seen[i] != a && fp_find(en, n, FP.seen[i]) >= 0) FP.seen[w++] = FP.seen[i];  // 1CD830: the object moves to the end, dead ones drop
    FP.nseen = w;
    if (FP.nseen == FP_SEEN) memmove(FP.seen, FP.seen + 1, sizeof(uint64_t) * (FP_SEEN - 1)), FP.nseen--;
    FP.seen[FP.nseen++] = a;
}
static void fp_r1(const SmEn *en, int n, const float eye[3], float to_u) {
    uint64_t old = FP.tgt, nt = 0;
    for (int k = 0; k < n && !nt; k++) {
        float c[3];
        if (en[k].life <= 0) continue;
        fp_center(&en[k], to_u, c);
        if (fp_on_screen(eye, c) && fp_seen_idx(en[k].actor) < 0) nt = en[k].actor, fp_seen_add(nt, en, n);
    }
    if (!nt) {  // cycle the seen list, starting after the old target (culled ones count)
        int start = 0, cnt = FP.nseen;
        if (old) { int i = fp_seen_idx(old); start = i < 0 ? 0 : i + 1; }
        for (int k = start; k < start + cnt && cnt > 0; k++) {
            uint64_t a = FP.seen[((k % cnt) + cnt) % cnt];
            if (fp_find(en, n, a) >= 0) { nt = a; break; }
        }
    }
    FP.tgt = nt;
    sora_sfx_event("cursor", 1.0f);  // rva 1DD170(7): the system cursor sound, on every R1 press (GUESS: our "cursor")
    FLOG("[summon] fps R1: target %llx (seen %d)\n", (unsigned long long)nt, FP.nseen);
}

// pick (rva 1CB360): the enemy nearest the shoot point on screen within 120 x 60 screen units
static uint64_t fp_pick(const SmEn *en, int n, const float eye[3], const float shoot[3], float to_u) {
    float sx, sy, best = FP_PICK_X * FP_PICK_X + FP_PICK_Y * FP_PICK_Y;
    uint64_t b = 0;
    if (!fp_screen(eye, shoot, &sx, &sy)) return 0;
    for (int k = 0; k < n; k++) {
        float c[3], x, y;
        if (en[k].life <= 0) continue;
        fp_center(&en[k], to_u, c);
        if (!fp_screen(eye, c, &x, &y)) continue;
        float dx = x - sx, dy = y - sy;
        if (fabsf(dx) > FP_PICK_X || fabsf(dy) > FP_PICK_Y) continue;
        float d2 = dx * dx + dy * dy;
        if (!b || d2 < best) b = en[k].actor, best = d2;
    }
    return b;
}

// ---- shots
static int g_fp_react = 1;  // (s137c user: Bokoblins react to Balls like to the Keyblade) KH2's atkp reaction via ah_hit, as blade_hits_one; fpsreact 0 = damage only (old)
static void fp_hit_enemy(const SmEn *e, int power, const char *what, float to_u, int atkp, const float from_cm[3]) {
    int dmg = sm_damage(e->actor, e->life, power);
    float at[3] = {e->pos[0], e->pos[1] + 0.9f, e->pos[2]};
    if (g_fp_react) {  // the Keyblade's path (sora.c blade_hits_one): atkp row reaction + knock-back, airhold f54, ah_hit
        int react = atkp == 1444 ? 8 : 18, kb1 = 0x7fff, kb2 = 0;  // (no row in the table: the reaction from S137_HELPER_FPS.md, sstm kb defaults)
        ah_atkp(atkp, &react, &kb1, &kb2);
        float from[3] = {from_cm[0] * to_u, from_cm[1] * to_u, from_cm[2] * to_u};
        int f54 = ah_hit_f54(e->actor, e->pos, F54_SMALL, react, to_u);
        linkpos_damage_enemy(e->actor, dmg, f54);
        ah_hit(e->actor, e->pos, from, react, kb1, kb2, f54, to_u, what);
    } else linkpos_damage_enemy_ex(e->actor, dmg, F54_PARTY, 0);
    sora_fx_event("hit", at, SM.face, to_u);
    static int hit_se = -2;
    if (hit_se == -2) hit_se = sora_sfx_id("hit");
    if (hit_se >= 0) sora_sfx_play_at(hit_se, 1.0f, at);
    SM.hits++, FP.n_hits++;
    FLOG("[summon] fps HIT %s %llx (life %d) power %d: damage %d\n", what, (unsigned long long)e->actor, e->life, power, dmg);
}
static void fp_w(const float cm[3], float to_u, float w[3]) { w[0] = cm[0] * to_u, w[1] = cm[1] * to_u, w[2] = cm[2] * to_u; }

static FpBall *fp_ball_new(void) {
    for (int i = 0; i < FP_NBALL; i++) if (!FP.ball[i].on) return &FP.ball[i];
    return &FP.ball[0];  // all busy (cannot happen at one per 10 f and 24 f of life): recycle the oldest
}
static void fp_fire_balls(const float feet[3], const SmEn *en, int n, float to_u) {
    float eye[3], f[3];
    fp_eye(feet, to_u, eye);
    fp_fwd(f);
    float sh[3] = {FP.shoot[0], FP.shoot[1], FP.shoot[2]};
    FpBall *b = fp_ball_new();
    if (b->on && b->fx >= 0 && sora_fx_alive(b->fx)) sora_fx_kill(b->fx);
    memset(b, 0, sizeof *b);
    b->on = true, b->atk = true, b->speed = FP_BALL_SPEED, b->rate = FP_BALL_HOMING, b->wait = FP_BALL_WAIT, b->life = FP_BALL_LIFE, b->fx = -1;
    float cm_to_w = 1.0f / to_u;
    // from Chicken Little's position = Sora's feet (attached at 0x4000, offset 0, VERIFIED start_limit); with fpscam 0 the visible chicken's chest (GUESS 90 cm)
    float p[3] = {feet[0], feet[1] + (fp_cam_live() ? 0.0f : 90.0f * to_u), feet[2]};
    for (int c = 0; c < 3; c++) b->p[c] = p[c] * cm_to_w;
    float d[3] = {sh[0] - p[0], sh[1] - p[1], sh[2] - p[2]};
    fp_norm3(d);
    memcpy(b->d, d, sizeof d);
    b->tgt = fp_pick(en, n, eye, sh, to_u);
    int el = fp_rnd() < 0.5f ? 0 : 2;  // VERIFIED: random_getf(1) < 0.5 -> element 0 else 2
    char nm[16];
    snprintf(nm, sizeof nm, "cl_fx_%d", el);
    int id = sora_fx_id(nm);
    if (id >= 0) b->fx = sora_fx_spawn(id, p, atan2f(d[0], d[2]), to_u);
    int v = (int)(fp_rnd() * 5.0f);  // obj_voice(friend, random_get(5) + 0)
    snprintf(nm, sizeof nm, "cl_v%d", v);
    int vid = sora_sfx_id(nm);
    if (vid >= 0) sora_sfx_play_at(vid, 1.0f, feet);
    FP.n_balls++;
    FLOG("[summon] fps BALLS #%d: element %d (%s), voice cl_v%d, target %llx, shoot %.2f %.2f %.2f\n", FP.n_balls, el, id >= 0 ? "fx ok" : "fx NOT in the pack", v,
         (unsigned long long)b->tgt, sh[0], sh[1], sh[2]);
}

static void fp_ball_end(FpBall *b) {
    if (b->fx >= 0 && sora_fx_alive(b->fx)) sora_fx_kill(b->fx);
    b->on = false, b->fx = -1;
}
static void fp_ball_step(FpBall *b, const SmEn *en, int n, float to_u) {
    if (b->atk && b->hit) { fp_ball_end(b); return; }  // the script ends on is_hit
    if (b->homing) {
        int k = b->tgt ? fp_find(en, n, b->tgt) : -1;
        if (k < 0) b->homing = false;
        else {
            float c[3], pw[3];
            fp_center(&en[k], to_u, c);
            fp_w(b->p, to_u, pw);
            float to[3] = {c[0] - pw[0], c[1] - pw[1], c[2] - pw[2]};
            float dist = fp_len3(to) / to_u;
            if (dist - FP.rad_cm - FP_BALL_STOP_CM < 0) b->homing = false;  // surface distance - 50 < 0
            else {  // trap_vector_homing: turn toward `to` by at most `rate` rad
                fp_norm3(to);
                float dot = b->d[0] * to[0] + b->d[1] * to[1] + b->d[2] * to[2];
                dot = dot > 1 ? 1 : dot < -1 ? -1 : dot;
                float ang = acosf(dot);
                if (ang > 1e-5f) {
                    float t = ang <= b->rate ? 1.0f : b->rate / ang;
                    for (int c2 = 0; c2 < 3; c2++) b->d[c2] += (to[c2] - b->d[c2]) * t;
                    fp_norm3(b->d);
                }
                b->rate = fmaxf(b->rate + FP_BALL_HOMING_D, FP_BALL_HOMING_MIN);
            }
        }
    } else if (b->wait > 0) {
        b->wait -= 1.0f;
        if (b->wait <= 0) b->homing = b->tgt != 0;
    }
    float prev[3] = {b->p[0], b->p[1], b->p[2]};
    for (int c = 0; c < 3; c++) b->p[c] += b->d[c] * b->speed;
    if (b->fx >= 0 && sora_fx_alive(b->fx)) {
        float w[3];
        fp_w(b->p, to_u, w);
        sora_fx_move(b->fx, w, atan2f(b->d[0], b->d[2]));
    }
    if (b->atk) {
        float a[3], z[3], h[3], nn[3];
        fp_w(prev, to_u, a), fp_w(b->p, to_u, z);
        if (b->steps++ > 0 && linkpos_raycast(a, z, h, nn)) {  // attack_is_hit_bg (review S5: not on the first step, which starts on the floor)
            b->hit = true;
            FLOG("[summon] fps ball hit the background\n");
        }
        for (int k = 0; k < n; k++) {
            if (en[k].life <= 0) continue;
            float c[3];
            fp_center(&en[k], to_u, c);
            float ce[3] = {c[0] / to_u, c[1] / to_u, c[2] / to_u}, s[3] = {b->p[0] - prev[0], b->p[1] - prev[1], b->p[2] - prev[2]};
            float l2 = s[0] * s[0] + s[1] * s[1] + s[2] * s[2], t = 0;
            if (l2 > 1e-6f) t = ((ce[0] - prev[0]) * s[0] + (ce[1] - prev[1]) * s[1] + (ce[2] - prev[2]) * s[2]) / l2, t = t < 0 ? 0 : t > 1 ? 1 : t;
            float q[3] = {prev[0] + s[0] * t - ce[0], prev[1] + s[1] * t - ce[1], prev[2] + s[2] * t - ce[2]};
            if (fp_len3(q) <= FP_BALL_RADIUS_CM + FP.rad_cm) { fp_hit_enemy(&en[k], FP_BALL_POWER, "balls", to_u, 1444, prev); b->hit = true; break; }  // first enemy hit ends it (is_hit)
        }
    }
    b->speed = fmaxf(b->speed + FP_BALL_ACCEL, FP_BALL_MIN);
    if (b->free_in > 0) { b->free_in -= 1.0f; if (b->free_in <= 0) b->atk = false; }
    if (b->life > 0) {
        b->life -= 1.0f;
        if (b->life <= 0) {
            b->fade = FP_BALL_FADE, b->free_in = FP_BALL_FREE_AFTER;
            if (b->fx >= 0 && sora_fx_alive(b->fx)) sora_fx_fadeout(b->fx, FP_BALL_FADE);
        }
    } else if (b->fade > 0) {
        b->fade -= 1.0f;
        if (b->fade <= 0) fp_ball_end(b);
    }
}

// Firecracker object (P_EX360): thrown toward Sora's position + facing * 200 cm, turned by rand(+-0.436) about the vertical, jump rand(70..100), life rand(30..60)
static void fp_fire_throw(const float feet[3], float to_u) {
    int slot = -1;
    for (int i = 0; i < FP_NFIRE; i++) if (!FP.fc[i].on) { slot = i; break; }
    if (slot < 0) return;
    FpFire *f = &FP.fc[slot];
    memset(f, 0, sizeof *f);
    f->on = true, f->fx = f->bfx = -1, f->blast = -1.0f;
    float st[3] = {feet[0] / to_u, feet[1] / to_u + (fp_cam_live() ? 0.0f : 90.0f), feet[2] / to_u};
    float fx = sinf(S.face), fz = cosf(S.face);
    float dest[3] = {feet[0] / to_u + fx * FP_FIRE_AHEAD, feet[1] / to_u, feet[2] / to_u + fz * FP_FIRE_AHEAD};
    float dx = dest[0] - st[0], dz = dest[2] - st[2], a = fp_rr(-FP_FIRE_SPREAD, FP_FIRE_SPREAD), c = cosf(a), s = sinf(a);
    float rx = dx * c - dz * s, rz = dx * s + dz * c;
    memcpy(f->p, st, sizeof st);
    f->face = (rx != 0 || rz != 0) ? atan2f(rx, rz) : S.face;
    f->life = fp_rr(30.0f, 60.0f);
    f->jh = fp_rr(70.0f, 100.0f);
    float d3 = sqrtf(rx * rx + (dest[1] - st[1]) * (dest[1] - st[1]) + rz * rz);
    float apex = st[1] + f->jh, fall = apex - dest[1], t = fp_uptime(f->jh);
    if (fall > 0) t += fp_uptime(fall);
    float k = -0.01f, v = t > 0 ? d3 / (t + 0.5f * k * t * t) : 0;
    while (v < 0) k *= 0.7f, v = d3 / (t + 0.5f * k * t * t);
    f->speed = v, f->accel = v * k, f->apex = apex, f->base = st[1], f->air = 0, f->tapex = fp_uptime(f->jh);
    int fuse = (int)(fp_rnd() * 4.0f);
    char nm[16];
    snprintf(nm, sizeof nm, "clf_fx_%d", fuse);
    int id = sora_fx_id(nm);
    float w[3] = {f->p[0] * to_u, (f->p[1] + 15.0f) * to_u, f->p[2] * to_u};
    if (id >= 0) f->fx = sora_fx_spawn(id, w, f->face, to_u);
    FP.n_fire_spawned++;
    FLOG("[summon] fps FIRECRACKER #%d: life %.0f f, jump %.0f cm, speed %.2f, fuse fx %d (%s)\n", FP.n_fire_spawned, f->life, f->jh, v, fuse, id >= 0 ? "ok" : "NOT in the pack");
}
static void fp_fire_end(FpFire *f) {
    if (f->fx >= 0 && sora_fx_alive(f->fx)) sora_fx_kill(f->fx);
    if (f->bfx >= 0 && sora_fx_alive(f->bfx)) sora_fx_kill(f->bfx);
    f->on = false;
}
static float *fp_fc_timer(uint64_t a, bool make) {
    int free_i = -1;
    for (int i = 0; i < 64; i++) {
        if (FP.fc_act[i] == a) return &FP.fc_last[i];
        if (!FP.fc_act[i] && free_i < 0) free_i = i;
    }
    if (!make || free_i < 0) return NULL;
    FP.fc_act[free_i] = a, FP.fc_last[free_i] = -1e9f;
    return &FP.fc_last[free_i];
}
static float g_fp_clock;  // KH frames since the Limit began (the shared attack 1261's Interval clock)
static void fp_fire_step(FpFire *f, const SmEn *en, int n, float to_u) {
    float w[3];
    if (f->blast >= 0) {  // 40 frames of strikes while the burst lasts
        for (int k = 0; k < n; k++) {
            if (en[k].life <= 0) continue;
            float d[3] = {en[k].pos[0] / to_u - f->p[0], en[k].pos[1] / to_u - f->p[1], en[k].pos[2] / to_u - f->p[2]};
            if (fp_len3(d) > FP_FIRE_REACH_CM) continue;
            float *t = fp_fc_timer(en[k].actor, true);
            if (!t || g_fp_clock - *t < FP_FIRE_INTERVAL) continue;  // atkp 1261 Interval 8: one shared timer per enemy
            *t = g_fp_clock;
            fp_hit_enemy(&en[k], FP_FIRE_POWER, "firecracker", to_u, 1261, f->p);
        }
        f->blast -= 1.0f;
        if (f->blast <= 0) fp_fire_end(f);
        return;
    }
    if (!f->landed && f->life > 0) {
        f->p[0] += sinf(f->face) * f->speed, f->p[2] += cosf(f->face) * f->speed;
        f->speed = fmaxf(f->speed + f->accel, 0.0f);
        // ACTION_JUMP's arc: sin(sin) rise, then the gravity fall (capped 16 / f)
        f->air += 1.0f;
        float tgt;
        if (f->tapex <= f->air) { float t = f->air - f->tapex; tgt = f->base + f->jh - FP_GRAV * t * t * 0.5f; }
        else { float kk = f->tapex ? f->air / f->tapex : 1.0f; tgt = f->base + f->jh * sinf(sinf(kk * 1.5707963f) * 1.5707963f); }
        float dz = tgt - f->p[1];
        if (dz < -FP_FALL_MAX) dz = -FP_FALL_MAX;
        float ny = f->p[1] + dz;
        bool landed = false;
        if (dz < 0) {  // a floor between the old and the new height lands it (OBJ_LAND)
            float a[3] = {f->p[0] * to_u, (f->p[1] + 8.0f) * to_u, f->p[2] * to_u}, z[3] = {f->p[0] * to_u, (ny - 2.0f) * to_u, f->p[2] * to_u}, h[3], nn[3];
            if (linkpos_raycast(a, z, h, nn) && nn[1] > 0.3f) {
                float gy = h[1] / to_u;
                f->p[1] = gy;
                float bounce = (f->apex - gy) * 0.3f;
                if (bounce > 4.0f) {  // bounce: 0.3 of the fall height, speed * 0.97
                    f->base = gy, f->jh = bounce, f->air = 0, f->tapex = fp_uptime(bounce), f->speed *= 0.97f, f->apex = gy + bounce;
                } else f->landed = true;
                landed = true;
            }
        }
        if (!landed) f->p[1] = ny;
        f->life -= 1.0f;
        w[0] = f->p[0] * to_u, w[1] = (f->p[1] + 15.0f) * to_u, w[2] = f->p[2] * to_u;
        if (f->fx >= 0 && sora_fx_alive(f->fx)) sora_fx_move(f->fx, w, f->face);
        return;
    }
    // the burst: effect 5 on the ground, 4 in the air, where the fuse was
    if (f->fx >= 0 && sora_fx_alive(f->fx)) sora_fx_kill(f->fx);
    f->fx = -1;
    char nm[16];
    snprintf(nm, sizeof nm, "clf_fx_%d", f->landed ? 5 : 4);
    int id = sora_fx_id(nm);
    w[0] = f->p[0] * to_u, w[1] = (f->p[1] + 15.0f) * to_u, w[2] = f->p[2] * to_u;
    if (id >= 0) f->bfx = sora_fx_spawn(id, w, f->face, to_u);
    f->blast = FP_FIRE_BLAST;
    FLOG("[summon] fps firecracker bursts (%s)\n", f->landed ? "landed" : "in the air");
}

// ---- the commands (530 Balls / 531 Firecracker / 532 End) and their greying
static bool sm_fps_cmd_ok(int cmd) {
    if (SMD()->ohana) return oh_cmd_ok(cmd);  // (s138) the pane asks with Ohana!'s 525..527
    if (SMD()->neverland) return nl_cmd_ok(cmd);  // (s139) Never Land's 470 / 471 / 479 / 482
    if (FP.st != 2) return false;
    if (cmd == 530) return FP.cool_b <= 0;
    if (cmd == 531) return FP.cool_f <= 0;
    return cmd == 532;
}
static void sm_fps_cmd(int cmd) { if (cmd >= 530 && cmd <= 532) FP.cmd_req = cmd; }  // run in the next frame's update

static void fp_cmd_run(int cmd, const float feet[3], const SmEn *en, int n, float to_u) {
    if (!sm_fps_cmd_ok(cmd)) { FLOG("[summon] fps cmd %d refused (%s)\n", cmd, FP.st != 2 ? "not in FPS Mode" : "cooling down"); return; }
    if (cmd == 530) { fp_fire_balls(feet, en, n, to_u); FP.cool_b = FP_BALLS_COOL; }
    else if (cmd == 531) {
        int v = 4 + (int)(fp_rnd() * 2.0f);  // obj_voice(friend, random_get(2) + 4)
        char nm[16];
        snprintf(nm, sizeof nm, "cl_v%d", v);
        int vid = sora_sfx_id(nm);
        if (vid >= 0) sora_sfx_play_at(vid, 1.0f, feet);
        FP.fire_left = 3 + (int)(fp_rnd() * 6.0f);  // 3 + random_get(6) objects
        FP.fire_gap = fp_rr(4.0f, 12.0f);
        FP.cool_f = FP_FIRE_COOL;
        FLOG("[summon] fps FIRECRACKER command: %d objects, voice cl_v%d\n", FP.fire_left, v);
    } else {
        FP.end_req = true;
        FLOG("[summon] fps END command\n");
    }
}
#define FP_ATTACK_BTN (1ull << 0)
#define FP_TRI_BTN (1ull << 2)
#define FP_SQ_BTN (1ull << 3)
#define FP_R1_BTN (1ull << 8)

// sora.c, after rc_tick: while FPS Mode runs the face buttons are the Limit's menu (KH2 menu kind 5: Triangle Firecracker, Square End, decide Balls) and R1 cycles the target
static int g_fp_showcl = 1;  // (s137c user: "I don't see Chicken Little like I should in KH2" -> he stays drawn; KH2's 3B7810(+0xa10) call on him is a GUESS as alpha 0; fpscl 0 = hide him again)
static void sm_fps_input(uint64_t *press, uint64_t *held) {
    if (oh_st()) { oh_input(press); return; }  // (s138 ohana)
    if (nl_st()) { nl_input(press); return; }  // (s139 neverland)
    if (gl_st()) { gl_input(press); return; }  // (s139 genielim) Triangle = the stage-2 command
    if (FP.st && *press && FP.log) LOG("[summon] fps pad: state %d press 0x%llx held 0x%llx\n", FP.st, (unsigned long long)*press, (unsigned long long)*held);  // (s137c: which bit is Circle?)
    if (FP.st != 2) return;
    if (*press & FP_ATTACK_BTN) FP.pr_atk = true;
    if (*press & FP_TRI_BTN) FP.pr_tri = true;
    if (*press & FP_SQ_BTN) FP.pr_sq = true;
    if (*press & FP_R1_BTN) FP.pr_r1 = true;
    if (*press & BTN_R3_GUESS) FP.pr_r3 = true;
    *press &= ~(FP_ATTACK_BTN | FP_TRI_BTN | FP_SQ_BTN | FP_R1_BTN | BTN_R3_GUESS);  // (Jump = Circle stays Sora's: his own ACTION 62 jump)
    FP.rsx = kh_axis(g_rx_raw), FP.rsy = kh_axis(g_ry_raw);
}

// (review M3) the reticle = the KH2 lock-on sprite at the shoot point (sora_hud_target: 1 = the yellow AUTO icon while aiming free, 2 = the blue-white
// LOCK icon on an R1 target). KH2's own FPS sprites (lock-on kinds 3 / 0x19, rva 17C570) are not baked: GUESS look. NOT sora_hud_quads (the camp layer
// blanks the whole HUD while it shows). Called from sm_late, after sora.c's own lock-on icon call, so it wins the frame; sora.c sets its icon again next frame.
static void fp_retic(bool have_tgt) {
    if (!FP.retic || FP.st != 2) return;
    sora_hud_target(have_tgt ? 2 : 1, FP.shoot);
    FP.retic_on = true;
}
static void fp_retic_off(void) {
    if (FP.retic_on) sora_hud_target(0, NULL);
    FP.retic_on = false;
}

static void fp_clear_shots(void) {
    for (int i = 0; i < FP_NBALL; i++) if (FP.ball[i].on) fp_ball_end(&FP.ball[i]);
    for (int i = 0; i < FP_NFIRE; i++) if (FP.fc[i].on) fp_fire_end(&FP.fc[i]);
    FP.fire_left = 0;
}
static void sm_fps_abort(const char *why) {
    oh_abort(why);  // (s138 ohana)
    nl_abort(why);  // (s139 neverland)
    gl_abort(why);  // (s139 genielim)
    if (FP.st || FP.cam_on) LOG("[summon] fps abort/end (%s)\n", why);
    FP.st = 0, FP.req = 0, FP.cmd_req = 0, FP.end_req = false;
    fp_cam_off();
    fp_retic_off();
    fp_clear_shots();
}
// (review M1 / S3) sm_tick: Sora left the driven states (swim / climb / event / Link's own state) while the summon stays out: end the mode and leave
// Chicken Little as fp_end does (no intro pose / hold left behind); the summon then waits hidden as before
static void sm_fps_leave(const char *why) {
    bool busy = FP.st != 0 || oh_st() != 0 || nl_st() != 0 || gl_st() != 0;  // (s138 ohana: its abort is inside sm_fps_abort, the restore below is shared)
    sm_fps_abort(why);
    if (!busy || SM.st == SM_OFF) return;
    SM.hold = SM.call_on = false;
    if (SM.acall > 0 && SM.off) SM.off(SM.acall);
    SM.slot = -1, SM.atk = false;
    if (FP.have_feet) sm_place_beside(FP.last_feet, g_sm_u, "FPS Mode aborted");
    sm_play(SMS_IDLE);
    SM.band = PBAND_STAND, SM.pause = 30;
}

// ---- start (the menu calls sm_fps_request; sm_fps_can greys the entry)
static const char *fp_gate(int st) {
    if (!SMD()->fps) return "this summon's Limit is not built (Stitch: Ohana!)";
    if (SM.st != SM_ACTIVE) return "Chicken Little is not out and ready";
    if (FP.st) return "FPS Mode is already running";
    if (SM.hold || SM.call_on || SM.hidden) return "Sora is in the call / hidden";
    if (!(SM.t > 0)) return "the summon gauge is empty (KH2: the Limit is offered while the gauge > 0)";
    if (st != LINK_GROUND || S.act != ACT_IDLE || S.special != SP_NONE) return "Sora is not free on the ground";
    if (pl_hold_party()) return "a party Limit is running";
    return NULL;
}
static bool sm_fps_can(void) { return SMD()->genie ? gl_can() : SMD()->ohana ? oh_can() : SMD()->neverland ? nl_can() : fp_gate(FP.last_st) == NULL; }
static void sm_fps_request(void) { if (SMD()->genie) gl_request(); else if (SMD()->ohana) oh_request(); else if (SMD()->neverland) nl_request(); else FP.req = 1; }

static void fp_start(const float feet[3], float to_u) {
    float cost = (float)FP_COST_UNITS * FP_RATE, before = SM.t;
    SM.t -= cost;
    if (SM.t < 0) SM.t = 0;
    FP.gauge_spent = before - SM.t;
    FP.st = 1, FP.ic = 0, FP.end_req = false, FP.time = FP.time0, FP.cool_b = FP.cool_f = 0, FP.tgt = 0, FP.nseen = 0, FP.fire_left = 0, FP.pr_r1 = FP.pr_atk = FP.pr_tri = FP.pr_sq = FP.pr_r3 = false;
    g_fp_clock = 0;
    memset(FP.fc_act, 0, sizeof FP.fc_act), memset(FP.fc_last, 0, sizeof FP.fc_last);  // (review M2: last Limit's firecracker timers are stale)
    lock_release("FPS Mode");  // (review S2) KH2's start_limit puts the lock-on block in mode 3 (FUN_1403dd650(.., 3)); our R1 lock + its camera let go
    // L247 intro: Sora plays capture 253 on the call actor, Chicken Little 241 attached to Sora (0x4002); the hold is the generic call hold
    SM.hold = SM.call_on = true, SM.cslot = SMS_CALL_END, SM.cclock = 0;
    SM.atk = false;
    sm_play(241);
    SM.clock = 0, SM.blend = -1;
    if (SM.body) SM.off(SM.body());
    // the shoot target the intro aims at: Sora's position + facing * 200, 160 up
    float fx = sinf(S.face), fz = cosf(S.face), e[3];
    fp_eye(feet, to_u, e);
    float aim[3] = {feet[0] + fx * 200.0f * to_u, feet[1] + 160.0f * to_u, feet[2] + fz * 200.0f * to_u};
    fp_aim_at(e, aim);
    LOG("[summon] fps START: gauge %.0f -> %.0f frames (cost %d x %.0f = %.0f, spent %.0f), Limit time %.0f f, intro 253 / 241 (%.0f f), camera %s\n", before, SM.t, FP_COST_UNITS,
        FP_RATE, cost, FP.gauge_spent, FP.time, FP_INTRO_FRAMES, FP.cam && FP.camset ? "KH2 first person" : FP.cam ? "BotW (exe has no linkpos_camera_set)" : "BotW (fpscam 0)");
}

static void fp_end(const char *why, const float feet[3], float to_u) {
    LOG("[summon] fps END (%s): %.0f of %.0f Limit frames left, %d Balls, %d firecrackers, %d hits, gauge now %.0f\n", why, FP.time, FP.time0, FP.n_balls, FP.n_fire_spawned, FP.n_hits, SM.t);
    FP.st = 0;
    fp_cam_off();
    fp_retic_off();
    // friend_end_limit: his effect 4 bound to him (cl_fx_4: a 600 f loop; GUESS: fades out over 40 f since the object script's stop is not decoded)
    int id = sora_fx_id("cl_fx_4");
    if (id >= 0) { int h = sora_fx_spawn(id, feet, S.face, to_u); if (h >= 0) sora_fx_fadeout(h, 40.0f); }
    SM.slot = -1;
    sm_place_beside(feet, to_u, "FPS Mode ended");
    sm_play(SMS_IDLE);
    SM.band = PBAND_STAND, SM.pause = 30;
}

// every driven frame (top of sm_late). true = Chicken Little's draw is handled here (intro / hidden)
static bool sm_fps_late(float kh, float to_u, int st) {
    FP.last_st = st;
    if (SMD()->genie) return gl_late(kh, to_u, st);  // (s139 genielim) Genie's Limits
    if (SMD()->ohana) return oh_late(kh, to_u, st);  // (s138 ohana) Stitch's Limit; Chicken Little's row never takes this branch
    if (SMD()->neverland) return nl_late(kh, to_u, st);  // (s139 neverland) Peter Pan's Limit
    static bool looked;  // (once per DLL copy: the exe does not change under it)
    if (!looked) {
        looked = true;
        FP.camset = (void (*)(bool, const float *, const float *, const float *))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_camera_set");
        FP.camstate = (int (*)(void))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_camera_override_state");
    }
    float feet[3];
    if (SM.st == SM_OFF) {
        if (FP.st || FP.cam_on) sm_fps_abort("summon gone");
        if (FP.req) FP.req = 0, LOG("[summon] fps refused: no summon is out\n");
        return false;
    }
    bool has_feet = sm_feet(feet);
    if (has_feet) memcpy(FP.last_feet, feet, sizeof feet), FP.have_feet = true;
    if (FP.req) {
        FP.req = 0;
        const char *why = has_feet ? fp_gate(st) : "no Link matrix";
        if (why) { LOG("[summon] fps refused: %s\n", why); sora_sfx_event("error", 1.0f); }
        else fp_start(feet, to_u);
    }
    if (!FP.st) {
        bool shots = FP.fire_left > 0;
        for (int i = 0; i < FP_NBALL; i++) shots |= FP.ball[i].on;
        for (int i = 0; i < FP_NFIRE; i++) shots |= FP.fc[i].on;
        if (!shots || !has_feet) return false;
    }
    if (!has_feet) return false;  // (LINK_OTHER never gets here: sm_tick ends the mode, sm_fps_leave)
    // the right stick / buttons were handed over by sora.c this frame
    g_fp_clock += kh;
    SmEn en[64];
    int n = sm_enemies(en, 64);
    bool handled = false;
    float eye[3];
    fp_eye(feet, to_u, eye);
    if (FP.st == 1) {  // the intro: Sora's 253 on the call actor (generic hold / cues), Chicken 241 attached (Sora's frame: his feet - Sora's root T(t) + Chicken's root T(t))
        FP.ic += kh;
        M.dead = 8, M.sub = -1;
        const Mot *cm = mot_find(g_sm_cc, g_sm_ncc, SMS_CALL_END), *ch = sm_mot(241);
        float ts[3] = {0, 0, 0}, tg[3] = {0, 0, 0};
        if (cm && cm->root) mot_root(cm, SM.cclock, ts);
        if (ch && ch->root) mot_root(ch, FP.ic, tg);
        float rx = tg[0] - ts[0], ry = tg[1] - ts[1], rz = tg[2] - ts[2], fx = sinf(S.face), fz = cosf(S.face);
        SM.pos[0] = feet[0] + (fz * rx + fx * rz) * to_u, SM.pos[1] = feet[1] + ry * to_u, SM.pos[2] = feet[2] + (-fx * rx + fz * rz) * to_u;
        SM.floor_s = feet[1], SM.face = S.face;
        float c0 = SM.clock;
        SM.clock = FP.ic > sm_frames(241, FP_INTRO_FRAMES) ? sm_frames(241, FP_INTRO_FRAMES) : FP.ic;
        sm_cues(0, ch, c0, SM.clock, SM.pos);
        if (SM.ach > 0 && SM.pose && !SM.hidden) {
            SM.shadow(SM.ach, feet[1], blob_str(0.55f));
            SM.pose(SM.ach, 241, SM.clock, SM.pos, SM.face, to_u);
        }
        handled = true;
        // (review M5) no FPS camera here: KH2's start_limit (rva 1CB6C0) starts it after Sora's 253 ends; the intro's own camera event
        // (P_EX350.a.us#90) is not decoded, so BotW's camera shows the intro (GUESS)
        if (!SM.call_on) {  // Sora's 253 is over (or a hit cut it short)
            FP.st = 2;
            if (SM.acall > 0 && SM.off) SM.off(SM.acall);
            LOG("[summon] fps: intro over after %.0f f, FPS Mode (Limit time %.0f f)\n", FP.ic, FP.time);
        }
    }
    if (FP.st == 2) {
        M.dead = 8, M.sub = -1;  // the command menu is the Limit's; no Sora commands
        // cooldowns, then the Limit time (1 per KH frame)
        FP.cool_b = fmaxf(FP.cool_b - kh, 0), FP.cool_f = fmaxf(FP.cool_f - kh, 0);
        FP.time -= kh;
        // pad: commands (the face buttons) + R1 target cycling + R3 re-aim + the right stick
        if (FP.pr_sq) FP.cmd_req = 532;
        else if (FP.pr_tri) FP.cmd_req = 531;
        else if (FP.pr_atk) FP.cmd_req = 530;
        bool r1 = FP.pr_r1, r3 = FP.pr_r3;
        FP.pr_r1 = FP.pr_atk = FP.pr_tri = FP.pr_sq = FP.pr_r3 = false;
        int sh_tgt = fp_find(en, n, FP.tgt);
        if (FP.tgt && sh_tgt < 0) FP.tgt = 0;  // the target died
        if (r1) fp_r1(en, n, eye, to_u), sh_tgt = fp_find(en, n, FP.tgt);
        bool look = fabsf(FP.rsx) > 0.45f || fabsf(FP.rsy) > 0.45f;  // GUESS: the camera-stick bits (18..21) set past ~45 % deflection
        if (!r1 && look && FP.tgt) FP.tgt = 0, sh_tgt = -1;  // 1CB830: a held camera-stick bit drops the target (lock mode 3 without a target)
        float tp[3];
        if (sh_tgt >= 0) fp_center(&en[sh_tgt], to_u, tp);
        // camera (178810): R3 re-aims; else yaw by the left/right bit at 0.05236 rad/f, pitch by |stick y| * 0.05236 (the up bit raises it)
        bool own_cam = FP.cam && FP.camset && FP.camstate && !FP.cam_failed;  // (the DLL's camera maths + writes; shown / stick eaten only once fp_cam_live)
        if (!own_cam) {  // BotW's camera stays: the aim follows ITS view (GUESS: the point 300 cm beyond Sora's eye along the view axis from the camera)
            float m[12];
            if (linkpos_camera_view(m)) {
                float fw[3] = {-m[8], -m[9], -m[10]}, cp[3];
                for (int c = 0; c < 3; c++) cp[c] = -(m[c] * m[3] + m[4 + c] * m[7] + m[8 + c] * m[11]);
                FP.yaw = atan2f(fw[0], fw[2]), FP.pitch = asinf(fw[1] > 1 ? 1 : fw[1] < -1 ? -1 : fw[1]);
                float dc = hypotf(hypotf(cp[0] - eye[0], cp[1] - eye[1]), cp[2] - eye[2]);
                if (sh_tgt < 0) for (int c = 0; c < 3; c++) FP.shoot[c] = cp[c] + fw[c] * (dc + FP_SHOOT_AHEAD_CM * to_u);
            }
        } else if (r3 || r1) {
            if (sh_tgt >= 0) fp_aim_at(eye, tp);
            else { float a[3] = {feet[0] + sinf(S.face) * 200.0f * to_u, feet[1] + 160.0f * to_u, feet[2] + cosf(S.face) * 200.0f * to_u}; if (r3) fp_aim_at(eye, a); }
        } else {
            float step = FP_TURN * (float)FP.turn_pct / 100.0f * kh;
            if (fabsf(FP.rsx) > 0.45f) FP.yaw = fp_wrap(FP.yaw - (FP.rsx > 0 ? 1.0f : -1.0f) * (float)FP.yawdir * step);  // RS right = turn right; +yaw is a LEFT turn in the mod's frame
            if (fabsf(FP.rsy) > 0.45f) FP.pitch = fp_wrap(FP.pitch + (FP.rsy > 0 ? 1.0f : -1.0f) * (float)FP.pitchdir * fabsf(FP.rsy) * step);
        }
        if (own_cam) {
            if (sh_tgt >= 0) fp_clamp_to(eye, tp);
            if (FP.pitch > FP_PITCH_MAX) FP.pitch = FP_PITCH_MAX;
            if (FP.pitch < FP_PITCH_MIN) FP.pitch = FP_PITCH_MIN;
        }
        // the shoot point: the target's point, else the eye + the view axis * 300
        if (sh_tgt >= 0) memcpy(FP.shoot, tp, sizeof tp);
        else if (own_cam) { float f[3]; fp_fwd(f); for (int c = 0; c < 3; c++) FP.shoot[c] = eye[c] + f[c] * FP_SHOOT_AHEAD_CM * to_u; }
        fp_cam_apply(feet, to_u);
        fp_retic(sh_tgt >= 0);
        if (FP.cmd_req) { int c = FP.cmd_req; FP.cmd_req = 0; fp_cmd_run(c, feet, en, n, to_u); }
        if (FP.end_req) fp_end("End command", feet, to_u);
        else if (FP.time <= 0) fp_end("Limit time over", feet, to_u);
        else if (fp_cam_live()) {  // KH2: Chicken Little + Sora at alpha 0, attached to Sora (only once the exe confirmed the first-person view)
            if (!g_fp_showcl) {
                if (SM.ach > 0 && SM.off) SM.off(SM.ach);
                handled = true;
            }
        }
        if (FP.st != 2) handled = false;
    }
    // the shots fly on after the Limit ended too (the script threads / objects live on)
    FP.acc += kh;
    int steps = 0;
    while (FP.acc >= 1.0f && steps++ < 4) {
        FP.acc -= 1.0f;
        for (int i = 0; i < FP_NBALL; i++) if (FP.ball[i].on) fp_ball_step(&FP.ball[i], en, n, to_u);
        for (int i = 0; i < FP_NFIRE; i++) if (FP.fc[i].on) fp_fire_step(&FP.fc[i], en, n, to_u);
        if (FP.fire_left > 0 && (FP.fire_gap -= 1.0f) <= 0) {  // L3951: one object every rand(4..12) frames
            fp_fire_throw(feet, to_u);
            FP.fire_left--;
            FP.fire_gap = fp_rr(4.0f, 12.0f);
        }
    }
    if (FP.acc > 4.0f) FP.acc = 0;
    return handled;
}

static bool fp_live_set(const char *key, int v) {
    if (!strcmp(key, "sfps")) { if (v) sm_fps_request(); }
    else if (!strcmp(key, "sfcmd")) { if (v) sm_fps_cmd(v); }
    else if (!strcmp(key, "fpsreact")) g_fp_react = v != 0, LOG("[summon] fps: hit reactions %s\n", g_fp_react ? "KH2 atkp (as the Keyblade)" : "off (damage only)");
    else if (!strcmp(key, "fpscl")) g_fp_showcl = v != 0, LOG("[summon] fps: Chicken Little %s in FPS Mode\n", g_fp_showcl ? "shown" : "hidden");
    else if (!strcmp(key, "fpslog")) FP.log = v != 0, LOG("[summon] fps log %s\n", FP.log ? "on" : "off");
    else if (!strcmp(key, "fpscam")) { FP.cam = v != 0, FP.cam_failed = false; if (!FP.cam) fp_cam_off(); LOG("[summon] fps camera %s\n", FP.cam ? "KH2 first person (needs the s137 exe)" : "BotW's"); }
    else if (!strcmp(key, "fpsrad")) FP.rad_cm = v < 10 ? 10.0f : v > 400 ? 400.0f : (float)v, LOG("[summon] fps enemy radius %.0f cm\n", FP.rad_cm);
    else if (!strcmp(key, "fpsyaw")) FP.yawdir = v < 0 ? -1 : 1, LOG("[summon] fps yaw dir %d\n", FP.yawdir);
    else if (!strcmp(key, "fpspitch")) FP.pitchdir = v < 0 ? -1 : 1, LOG("[summon] fps pitch dir %d\n", FP.pitchdir);
    else if (!strcmp(key, "fpsturn")) FP.turn_pct = v < 10 ? 10 : v > 500 ? 500 : v, LOG("[summon] fps turn %d%%\n", FP.turn_pct);
    else if (!strcmp(key, "fpstime")) FP.time0 = v < 30 ? 30.0f : v > 6000 ? 6000.0f : (float)v, LOG("[summon] fps Limit time %.0f f\n", FP.time0);
    else if (!strcmp(key, "fpsfov")) FP.fov_h = v < 50 ? 0.5f : v > 300 ? 3.0f : (float)v / 100.0f, LOG("[summon] fps fov_h %.2f (give hundredths: 150 = 1.5)\n", FP.fov_h);
    else if (!strcmp(key, "fpseye")) FP.eye_cm = v < 20 ? 20.0f : v > 300 ? 300.0f : (float)v, LOG("[summon] fps eye %.0f cm\n", FP.eye_cm);
    else if (!strcmp(key, "fpsretic")) FP.retic = v != 0;
    else return false;
    return true;
}

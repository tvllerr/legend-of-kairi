// (s110 skate) KH2's skateboard (F_TT010) outside the first shrine (Oman Au) for Roxas and Sora. Notes + data: recon/S110_HELPER_SKATE.md.
//
// Slice 1b: the REAL KH2 board + rider motions. F_TT010.mdlx / F_TT010.mset (kh2_first) is the Twilight Town skateboard object (8 bones);
// its mset holds the board's own motions (slots 151..169) and, nested in slots 252..270, the RIDER's motions for Roxas' skeleton
// (F_TT010.mset, 229 bones) and for Sora's (F_TT010_SORA.mset, 228 bones). tools_s110s/skatebake `board` / `rider` bake three actors:
//   board  (parked: the board alone, rest pose)       data/forms/skate_board_{model,pose}.bin
//   Sora   (P_EX100 body + board bones, KH2 skate motions)  data/forms/skate_sora_{model,pose}.bin
//   Roxas  (P_EX110 body + board bones, KH2 skate motions)  data/forms/skate_rox_{model,pose}.bin
// While riding the rider actor replaces the normal body (one actor = rider + board, so they never drift apart); the physics stays the mod's.
// KH2's own physics numbers are NOT in the data (the skate missions are scripts; the numbers live in the exe), so speed / turn / jump height are GUESS.
//
// Motion map (board slot, KH2 names; the meaning is GUESS from the keyframes + their trigger ranges): 151 00C idle loop, 152 01C push-off,
// 153 02C cruise loop, 154 03C ollie take-off, 155 04C air loop, 156 05C landing, 160 09C / 161 09B lean (turning), 164..167 07D..07G flip tricks.
//
// Play (live "set skate 1", default 1): five boards lie in front of the Oman Au, Ja Baij, Owa Daim and Keh Namut Shrines and the Shrine of
// Resurrection's exit (or where you last got off one, per save; a save load puts them back, "skloadhome"). Only the nearest is drawn / rideable.
//   Triangle within SKT_R of it on the ground = get on; stick steers / pushes / coasts / brakes; B = jump (an ollie); Square (Y) in the air = a trick;
//   Triangle on the ground = get off, the board stays. Link's own states (swim, climb, glide, cutscene, a hit, a load, death) get you off by themselves.
// Integration (small hunks in sora.c): skt_tick (every frame, early: placement, parked-board draw, auto get-off), skt_input (presses),
// skt_ride (steering, in place of the stick's walk / run), skt_move (the board's velocity), skt_rider_draw (in place of the body draw),
// skt_eat_x (input thread), skt_live_set, skt_slot_load / skt_slot_saved (per-save position).
#pragma once
#include "sora_params_skate.h"  // SKB_SLOTS: {board slot, frames, loop, start} of the baked motions (generated)

#define SKT_DATA "D:/SoraBotW/data/forms/"
#define SKT_NEED_ACTORS 14  // the exe's table: Sora 0, Donald, Goofy, 6 forms, Roxas, Mickey = 11, + board, Sora rider, Roxas rider
// Oman Au Shrine (Dungeon038) door: DgnObj_DungeonEntrance_A_01 (-673.2, 172.1, 1513.0) rot 0.9 rad, BotW 1.9.0 E-6_Static; the
// board lies ~12 m out in front of it (front = (sin 0.9, cos 0.9)), 3 m to the side of the warp point, ground y ~171.7
#define SKT_HOME_X -665.5f
#define SKT_HOME_Y 171.7f
#define SKT_HOME_Z 1522.5f
#define SKT_HOME_YAW 2.4708f  // 0.9 + 90 deg: the board's long axis (model +z) runs along the shrine's wall
enum { SKT_BOARD, SKT_SORA, SKT_ROX, SKT_NW };
enum { SM_IDLE = 151, SM_PUSH = 152, SM_CRUISE = 153, SM_OLLIE = 154, SM_AIR = 155, SM_LAND = 156, SM_LEAN_A = 160, SM_LEAN_B = 161 };
static const int SKT_TRICKS[4] = {164, 165, 166, 167};  // 07D 07E 07F 07G; each starts with its own KH2 sound (se334 #21..#24)

static struct {
    int on;                      // live "skate": 0 off, 1 on (env SORA_SKATE sets the start value)
    bool init, riding, rider_on, failed, drawn, snapped, reach, was_near, have_last, trick_req, was_air, rdone;
    int act[SKT_NW];             // the draw actors (> 0 once loaded), by SKT_BOARD / SKT_SORA / SKT_ROX
    uint64_t try_at[SKT_NW];
    float pos[3], yaw;           // the parked board (BotW u); yaw as a face: model +z (its nose) = world (sin yaw, cos yaw)
    float heading, speed, turn;  // riding: travel heading (rad), cm per KH frame along it, heading change per KH frame
    float v[2];                  // the board's velocity, cm per KH frame (world x, z)
    float carry[2];              // the get-off slide (cm per KH frame, decays)
    int carry_n;
    int rw, rs, tn;              // the rider's actor kind, its playing board slot, the trick cycle
    float rc;                    // the playing slot's clock, 60 Hz frames
    int roll_voice, snd_state;   // the roll loop's sfx handle; -1 unchecked / 0 no pack / 1 ids ready
    int snd[11];
    float last_p[3], prev_p[3];  // Link's position this tick (live "skatehere") / when skt_ride last ran (the stall test)
    float safe[3];               // (s110 review) riding: the last dry ground spot under him (an automatic get-off in water / the air / a load leaves the board there)
    bool safe_ok, tick_ok;       // safe[] set this ride; last_p[] holds the previous tick's position
    uint64_t load_seen;          // g_load_end at the last tick (a load ended in between = get off)
    int logn, stall, slot;
    uint64_t cd_until, snap_at;
    volatile int eat;            // X belongs to the board (input thread)
    void (*pose)(int, int, float, const float *, float, float);
    void (*off)(int);
    void (*shadow)(int, float, float);
    int (*load)(const char *, const char *);
    int (*cap)(void);
    int (*body)(void);
} SKT = {.on = 1, .slot = -1, .roll_voice = -1, .snd_state = -1, .pos = {SKT_HOME_X, SKT_HOME_Y, SKT_HOME_Z}, .yaw = SKT_HOME_YAW};

// the numbers (live keys): cm per KH frame (60 Hz) like prty's RunSpeed 8; BotW u = KH cm * 0.01 * g_k
static float g_skt_max = 24.0f;     // top speed ("skmax" x10): (s110 user: "make the speed faster") 15 -> 24 = ~3 x Roxas' run (8)
static float g_skt_acc = 0.35f;     // push acceleration per KH frame ("skacc" x100) (s110: 0.22 -> 0.35, the same time to top speed)
static float g_skt_brake = 0.9f;    // stick pushed against the travel ("skbrake" x100)
static float g_skt_fric = 0.008f;   // coasting: speed *= (1 - fric) per KH frame ("skfric" x10000)
static float g_skt_turn = 0.07f;    // heading turn, rad per KH frame at a standstill (halved at top speed; x3 below 2 cm/frame) ("skturn" x1000)
static float g_skt_reach = 2.2f;    // get-on radius, u ("skr" x10)
static float g_skt_jump = 120.0f;   // the ollie's height, cm ("skjump"; base jump 185)
static int g_skt_flip, g_skt_log, g_skt_snd = 1;
static int g_skt_loadhome = 1;  // (s111 user: "whenever i load a save put the skateboards back at their spawn") live "set skloadhome 0" = keep the saved spots

static bool skt_eat_x(void) { return SKT.eat != 0; }

static void skt_roll_loop_stop(void);
static void skt_sidecar_path(int slot, char *p, size_t n) { snprintf(p, n, "%s/%d/skate.txt", sl_root(), slot); }
// (s110, user: "spawn another one in front of the Shrine of Resurrection where you walk outside for the first time") a second
// parked board; (s118, user wish: a board in front of every Great Plateau shrine) now five. Only one board draw actor exists (the exe
// table is full at 14), so SKT.pos / SKT.yaw / SKT.snapped are always the board NEAREST to Link (index SKB_CUR) and BRD[] holds the
// five boards' places (the active entry is stale until skt_store(): the live copy is SKT.*). skt_tick swaps the active one when another
// is nearer by SKB_HYST. The closest pair of spots is 449 u apart (Shrine of Resurrection - Keh Namut), so a swap happens >= 220 u from
// both boards (a 2 m board at that range: no visible pop).
// Spots (BotW 1.9.0 map data, DgnObj_DungeonEntrance_A_01 door + rot r; front = (sin r, cos r); the board = door + front * 8 + side * 3,
// side = (-cos r, sin r); yaw = r + 90 deg = the long axis along the wall; the shrine's LocationTag MessageID names it).
// Oman Au (Dungeon038) keeps its s110 spot (12 m out). The Shrine of Resurrection's is the user's (s110). y = the shrine platform's
// (PosAfterDemoSkip / warp point y); GUESS until walked: the floor ray (within 150 u) stands the board on the real ground.
// Owa Daim: 10 m out and 3 m to the other side (Banking_A_01 slabs lie on the +side). Details: recon/S118_HELPER_SKATE.md.
#define SKT_HOME2_X -1021.0f  // (s110: the user's spot in front of the Shrine of Resurrection, x -1021 y 252 z 1803; was -1049.2 251.3 1815.9)
#define SKT_HOME2_Y 252.0f
#define SKT_HOME2_Z 1803.0f
#define SKT_HOME2_YAW 2.16f
#define SKB_N 5
#define SKB_HYST 8.0f  // u: another board must be this much nearer before the drawn / rideable one changes (no flicker at the midpoint)
enum { SKB_OMAN, SKB_RESU, SKB_JABAIJ, SKB_OWADAIM, SKB_KEHNAMUT };
static const char *const SKB_NAME[SKB_N] = {"Oman Au", "Shrine of Resurrection", "Ja Baij", "Owa Daim", "Keh Namut"};
static const float SKB_HOME[SKB_N][4] = {  // x y z yaw
    {SKT_HOME_X, SKT_HOME_Y, SKT_HOME_Z, SKT_HOME_YAW},  // Oman Au: door (-673.2 172.1 1513.0) rot 0.900 (s110 spot)
    {SKT_HOME2_X, SKT_HOME2_Y, SKT_HOME2_Z, SKT_HOME2_YAW},  // Shrine of Resurrection exit (the user's spot)
    {-454.7f, 180.0f, 1987.2f, 0.0000f},  // Ja Baij (Dungeon041): door (-446.7 179.0 1990.2) rot -1.5708
    {-928.0f, 274.2f, 2311.2f, 4.7124f},  // Owa Daim (Dungeon009): door (-925.0 273.2 2321.2) rot 3.1416
    {-1427.8f, 337.6f, 1991.6f, 3.4307f}};  // Keh Namut (Dungeon065): door (-1436.3 336.6 1991.0) rot 1.8599
static struct { float pos[3], yaw; bool snapped; } BRD[SKB_N];
static bool BRD_ok;  // BRD[] filled (skt_brd_fill / skt_home ran)
static int SKB_CUR;  // the board SKT.pos / SKT.yaw / SKT.snapped stand for
static void skt_brd_fill(void) {  // BRD[] at the shrines once, WITHOUT touching the live copy SKT.* (a live key may have moved it already)
    if (BRD_ok) return;
    BRD_ok = true;
    for (int i = 0; i < SKB_N; i++) memcpy(BRD[i].pos, SKB_HOME[i], sizeof BRD[0].pos), BRD[i].yaw = SKB_HOME[i][3], BRD[i].snapped = false;
}
static void skt_store(void) {  // the live board (SKT.*) back into its BRD[] entry
    skt_brd_fill();
    memcpy(BRD[SKB_CUR].pos, SKT.pos, sizeof BRD[0].pos), BRD[SKB_CUR].yaw = SKT.yaw, BRD[SKB_CUR].snapped = SKT.snapped;
}
static void skt_use(int i) {  // board i becomes the live (drawn / rideable) one
    skt_store();
    SKB_CUR = i;
    memcpy(SKT.pos, BRD[i].pos, sizeof SKT.pos), SKT.yaw = BRD[i].yaw, SKT.snapped = BRD[i].snapped;
}
static void skt_home_one(int i) {
    memcpy(BRD[i].pos, SKB_HOME[i], sizeof BRD[0].pos), BRD[i].yaw = SKB_HOME[i][3], BRD[i].snapped = false;
    if (i == SKB_CUR) memcpy(SKT.pos, BRD[i].pos, sizeof SKT.pos), SKT.yaw = BRD[i].yaw, SKT.snapped = false;
}
static void skt_home(void) {
    BRD_ok = true;
    for (int i = 0; i < SKB_N; i++) skt_home_one(i);
}
// the live place of board i (the active one is SKT.*)
static const float *skt_bpos(int i) { return i == SKB_CUR ? SKT.pos : BRD[i].pos; }
// the board nearest to p (the active board counts SKB_HYST nearer: no flicker at a midpoint); *dist = the real distance to it
static int skt_nearest(const float p[3], float *dist) {
    int best = SKB_CUR;
    float bd = 1e30f;
    for (int i = 0; i < SKB_N; i++) {
        const float *q = skt_bpos(i);
        float d = hypotf(p[0] - q[0], p[2] - q[2]) - (i == SKB_CUR ? SKB_HYST : 0.0f);
        if (d < bd) bd = d, best = i;
    }
    if (dist) *dist = hypotf(p[0] - skt_bpos(best)[0], p[2] - skt_bpos(best)[2]);
    return best;
}
static void skt_slot_load(int slot) {   // (sl_slot_load) this save's boards; none = the shrines
    SKT.slot = slot;
#define SKT_SANE(v) (isfinite(v) && fabsf(v) < 1e6f)  // (review) no NaN / inf / absurd numbers from a sidecar (they would also overflow the writer)
    char p[1100], b[600];
    skt_sidecar_path(slot, p, sizeof p);
    FILE *f = fopen(p, "rb");
    size_t n = f ? fread(b, 1, sizeof b - 1, f) : 0;
    if (f) fclose(f);
    b[n] = 0;
    bool have = false;
    int nread = 0;
    if (n && !g_skt_loadhome) {
        if (b[0] == 'B') {  // (s118) "B<count> x y z yaw ..." in board order (Oman Au, Resurrection, Ja Baij, Owa Daim, Keh Namut)
            const char *c = b + 1;
            char *e;
            long cnt = strtol(c, &e, 10);
            c = e;
            skt_home();
            for (int i = 0; i < cnt && i < SKB_N; i++) {
                float t[4];
                int k, ok = 1;
                for (k = 0; k < 4; k++) {
                    t[k] = strtof(c, &e);
                    if (e == c || !SKT_SANE(t[k])) { ok = 0; break; }
                    c = e;
                }
                if (!ok) break;
                memcpy(BRD[i].pos, t, sizeof BRD[0].pos), BRD[i].yaw = t[3], BRD[i].snapped = true, nread++;
            }
            have = nread > 0;
        } else {  // old sidecars: 4 floats (the nearer board) or 8 (nearer, other); identity = the nearer of the Oman Au / Resurrection spots
            float w[8];
            int got = sscanf(b, "%f %f %f %f %f %f %f %f", w, w + 1, w + 2, w + 3, w + 4, w + 5, w + 6, w + 7);
            if (got >= 4 && SKT_SANE(w[0]) && SKT_SANE(w[1]) && SKT_SANE(w[2]) && SKT_SANE(w[3])) {
                skt_home();
                bool taken[2] = {false, false};
                for (int k = 0; k < (got >= 8 ? 2 : 1); k++) {
                    const float *q = w + k * 4;
                    if (!SKT_SANE(q[0]) || !SKT_SANE(q[1]) || !SKT_SANE(q[2]) || !SKT_SANE(q[3])) continue;
                    if (hypotf(q[0] + 1049.2f, q[2] - 1815.9f) < 1.0f) continue;  // (the first s110 spot of board 2 = the user's spot now)
                    int id = hypotf(q[0] - SKB_HOME[0][0], q[2] - SKB_HOME[0][2]) <= hypotf(q[0] - SKB_HOME[1][0], q[2] - SKB_HOME[1][2]) ? 0 : 1;
                    if (taken[id]) id = !id;
                    if (taken[id]) continue;
                    taken[id] = true, nread++;
                    memcpy(BRD[id].pos, q, sizeof BRD[0].pos), BRD[id].yaw = q[3], BRD[id].snapped = true;
                }
                have = true;
            }
        }
    }
    if (!have) skt_home();
    {  // BRD[] was just rewritten: the live copy = the entry of the board nearest to Link (SKT.last_p; before the first tick the Oman Au one)
        float d;
        SKB_CUR = 0;
        memcpy(SKT.pos, BRD[0].pos, sizeof SKT.pos), SKT.yaw = BRD[0].yaw, SKT.snapped = BRD[0].snapped;
        int nb = SKT.tick_ok ? skt_nearest(SKT.last_p, &d) : 0;
        if (nb != 0) skt_use(nb);
        if (SKT.drawn && SKT.off && SKT.act[SKT_BOARD] > 0) SKT.off(SKT.act[SKT_BOARD]);
        SKT.drawn = false;
    }
    if (have)
        LOG("[skate] slot %d: %d board(s) from its sidecar, the rest at their shrines; nearest = %s (%.1f %.1f %.1f)\n", slot, nread, SKB_NAME[SKB_CUR],
            SKT.pos[0], SKT.pos[1], SKT.pos[2]);
    else
        LOG("[skate] slot %d: %s, boards at the Oman Au, Ja Baij, Owa Daim and Keh Namut Shrines and the Shrine of Resurrection's exit; nearest = %s\n", slot,
            g_skt_loadhome ? "save loaded = boards back at their spawns (skloadhome 1)" : "no board sidecar", SKB_NAME[SKB_CUR]);
    if (SKT.riding) {  // (s110 review: a load read while riding: the rider actor must not stay drawn at its last pose)
        skt_roll_loop_stop();
        if (SKT.off && SKT.act[SKT.rw] > 0) SKT.off(SKT.act[SKT.rw]);
    }
    SKT.riding = SKT.rider_on = false, SKT.carry_n = 0, SKT.speed = 0, SKT.safe_ok = false;
}
static void skt_slot_saved(int slot) {  // (sl_slot_saved) BotW wrote slot N: the boards' places go next to it
    char p[1100], t[1110], b[600];
    skt_sidecar_path(slot, p, sizeof p);
    snprintf(t, sizeof t, "%s.tmp", p);
    skt_store();
    // (s118) "B5" + x y z yaw per board, in board order; older 4 / 8 float files still load. (review) every piece is checked: a huge
    // number ("%.3f" of 1e38 = 40+ chars) must not run n past b[] (the next snprintf would get b + n out of range and a wrapped size)
    int n = snprintf(b, sizeof b, "B%d", SKB_N);
    for (int i = 0; i <= SKB_N && n > 0; i++) {
        int k = i < SKB_N ? snprintf(b + n, sizeof b - (size_t)n, " %.3f %.3f %.3f %.4f", BRD[i].pos[0], BRD[i].pos[1], BRD[i].pos[2], BRD[i].yaw)
                          : snprintf(b + n, sizeof b - (size_t)n, "\n");
        n = k < 0 || (size_t)k >= sizeof b - (size_t)n ? -1 : n + k;
    }
    FILE *f = n > 0 ? fopen(t, "wb") : NULL;
    bool ok = f && fwrite(b, 1, (size_t)n, f) == (size_t)n;
    if (f) ok = !fclose(f) && ok;
    ok = ok && MoveFileExA(t, p, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileA(t);
    if (g_skt_log) LOG("[skate] slot %d: sidecar %s (%d boards; active = %s at %.1f %.1f %.1f)\n", slot, ok ? "written" : "NOT written", SKB_N, SKB_NAME[SKB_CUR], SKT.pos[0], SKT.pos[1], SKT.pos[2]);
}

static void skt_init(void) {
    if (SKT.init) return;
    SKT.init = true;
    skt_brd_fill();
    const char *e = getenv("SORA_SKATE");
    if (e && *e) SKT.on = atoi(e) != 0;
    HMODULE exe = GetModuleHandleA(NULL);
    SKT.load = (int (*)(const char *, const char *))(void *)GetProcAddress(exe, "sora_draw_actor_load");
    SKT.pose = (void (*)(int, int, float, const float *, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_pose");
    SKT.off = (void (*)(int))(void *)GetProcAddress(exe, "sora_draw_actor_off");
    SKT.shadow = (void (*)(int, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_shadow");
    SKT.cap = (int (*)(void))(void *)GetProcAddress(exe, "sora_draw_actor_capacity");
    SKT.body = (int (*)(void))(void *)GetProcAddress(exe, "sora_draw_body");
    LOG("[skate] %s; %d boards (Oman Au, Shrine of Resurrection, Ja Baij, Owa Daim, Keh Namut), first at %.1f %.1f %.1f; draw functions %s, exe actor table %d (needs %d)\n",
        SKT.on ? "on" : "off (set skate 1)", SKB_N, BRD[0].pos[0], BRD[0].pos[1], BRD[0].pos[2],
        SKT.load && SKT.pose && SKT.off && SKT.body ? "found" : "MISSING (the board stays off)", SKT.cap ? SKT.cap() : 0, SKT_NEED_ACTORS);
    if (SLS.slot >= 0) skt_slot_load(SLS.slot);  // (a hot reload: this save's board again)
}

// A draw actor of the three, loaded on demand (the parked board within 300 u, the rider of the current body within 60 u); an exe with fewer than
// 14 actors is refused (the board would take the last free one and Mickey's rescue would find the table full); files missing = logged, retried every 600 presents
static bool skt_actor(int w, uint64_t present) {
    if (SKT.act[w] > 0) return true;
    if (!SKT.load || !SKT.pose || !SKT.off || !SKT.body || !SKT.cap || SKT.cap() < SKT_NEED_ACTORS || (SKT.try_at[w] && present - SKT.try_at[w] < 600)) {
        if (!SKT.failed && SKT.init && (!SKT.cap || SKT.cap() < SKT_NEED_ACTORS))
            SKT.failed = true, LOG("[skate] no board: this exe has %d draw actors, the board needs %d (relink savagewind.exe)\n", SKT.cap ? SKT.cap() : 0, SKT_NEED_ACTORS);
        return false;
    }
    static const char *const F[SKT_NW][2] = {{SKT_DATA "skate_board_model.bin", SKT_DATA "skate_board_pose.bin"},
                                             {SKT_DATA "skate_sora_model.bin", SKT_DATA "skate_sora_pose.bin"},
                                             {SKT_DATA "skate_rox_model.bin", SKT_DATA "skate_rox_pose.bin"}};
    SKT.try_at[w] = present ? present : 1;
    int id = SKT.load(F[w][0], F[w][1]);
    if (id > 0) {
        SKT.act[w] = id;
        if (SKT.shadow) SKT.shadow(id, SKT.pos[1], w == SKT_BOARD ? 0.0f : blob_str(0.55f));  // (the parked board: no blob; a rider: Sora's)
        LOG("[skate] %s draw actor %d\n", w == SKT_BOARD ? "board" : w == SKT_SORA ? "Sora rider" : "Roxas rider", id);
    } else
        LOG("[skate] %s NOT loaded (%s installed? exe relinked with SORA_DRAW_ACTORS %d?): no riding with it, nothing else changes\n",
            w == SKT_BOARD ? "board" : w == SKT_SORA ? "Sora rider" : "Roxas rider", F[w][0], SKT_NEED_ACTORS);
    return id > 0;
}

static int skt_body_kind(void) { return IN_ROXAS ? SKT_ROX : SKT_SORA; }

static void skt_save_now(const char *why) {
    if (SKT.slot >= 0) skt_slot_saved(SKT.slot);
    LOG("[skate] %s: the %s board stays at %.1f %.1f %.1f\n", why, SKB_NAME[SKB_CUR], SKT.pos[0], SKT.pos[1], SKT.pos[2]);
}

// the board's sounds (se334 of F_TT010.a.us, sfxskate.py): ready only when the pack has them
static int skt_snd(int i) {  // SCD sound i (0 roll loop, 3 ollie, 4 land, 6..9 the 4 tricks) -> sfx id or -1
    if (SKT.snd_state < 0) {
        SKT.snd_state = g_skt_snd && sora_sfx_ready() && sora_sfx_id("skatese_v1") >= 0 ? 1 : 0;
        if (SKT.snd_state)
            for (int k = 0; k < 11; k++) {
                char key[48];
                snprintf(key, sizeof key, "se/se334.win32.scd#%d", 15 + k);
                SKT.snd[k] = sora_sfx_id(key);
            }
        LOG("[skate] board sounds %s\n", SKT.snd_state ? "in the pack" : "not in the pack (sfxskate.py not installed): the board is silent");
    }
    return SKT.snd_state && i >= 0 && i < 11 ? SKT.snd[i] : -1;
}
// (s172) the ollie / landing / trick sounds go to the remote too, at his feet there (net_note_sfx NULL); the roll loop stays local (GUESS: fine)
static void skt_play(int i, float gain) { int id = skt_snd(i); if (id >= 0) sora_sfx_play(id, gain), net_note_sfx(id, gain, NULL); }
static void skt_roll_loop(bool on) {
    if (on && (SKT.roll_voice < 0 || !sora_sfx_playing(SKT.roll_voice))) {
        int id = skt_snd(0);
        SKT.roll_voice = id >= 0 ? sora_sfx_play_pan(id, 0.55f, 0.0f, true) : -1;
    } else if (!on && SKT.roll_voice >= 0) sora_sfx_stop(SKT.roll_voice), SKT.roll_voice = -1;
}
static void skt_roll_loop_stop(void) { skt_roll_loop(false); }
// (s110 review) the frame body's early returns (BotW's pause, the KH2 camp, no world / no body): the roll loop must not drone on under a menu
// or a load (skt_rider_draw starts it again on the next ridden frame)
static void skt_quiet(void) { if (SKT.roll_voice >= 0) skt_roll_loop(false); }

// here_ok false (water, the air, a load / warp, death): the board goes back to the last dry ground spot of the ride, not where he is
// (s110, user: "I was on the skateboard and started climbing and it despawned" - it stayed on the ledge at the last ground spot,
// out of view below) a climb / swim / glide get-off puts the board in Roxas' pocket: not drawn, and back at his feet once he has
// stood on dry ground 0.5 s. Warps / loads / death / Triangle keep the old rule (the board stays). Live "set skpocket 0" = old.
static int g_skt_pocket = 1;
static struct { bool on; ULONGLONG since; } SKP;
static void skt_dismount(const char *why, bool slide, bool here_ok) {
    if (!SKT.riding) return;
    if (g_skt_pocket && (!strcmp(why, "Link's own state") || !strcmp(why, "glide")))
        SKP.on = true, SKP.since = 0, LOG("[skate] %s: the board goes in the pocket (back at your feet on dry ground)\n", why);
    if (!here_ok && SKT.safe_ok) memcpy(SKT.pos, SKT.safe, sizeof SKT.pos);
    SKT.riding = SKT.rider_on = false;
    skt_roll_loop(false);
    if (SKT.off && SKT.act[SKT.rw] > 0) SKT.off(SKT.act[SKT.rw]);
    SKT.yaw = SKT.heading;  // (the board keeps the way it rolled; the rider's face = the heading while riding)
    S.face = SKT.heading;
    float vl = hypotf(SKT.v[0], SKT.v[1]);
    SKT.carry_n = slide && vl > 2.0f && !ACT_AIR(S.act) ? 18 : 0;
    SKT.carry[0] = SKT.v[0] * 0.8f, SKT.carry[1] = SKT.v[1] * 0.8f;
    SKT.speed = 0, SKT.v[0] = SKT.v[1] = 0;
    SKT.cd_until = GetTickCount64() + 750;  // (s110 review: ms, not presents: the same 0.75 s at any fps)
    SKT.snapped = here_ok && !ACT_AIR(S.act);  // (else the floor ray finds the ground again)
    SKT.safe_ok = false;
    skt_save_now(why);
}

// every frame, early (after the Slate's state change, before Link's-own-states branch): auto get-off, the board's place, the parked board's draw
static void skt_tick(uint64_t present, int st, const float p[3], bool block) {
    bool first = !SKT.init;
    skt_init();
    if (first) SKT.load_seen = g_load_end;
    bool can = SKT.on && !block;
    bool jumped = SKT.tick_ok && hypotf(p[0] - SKT.last_p[0], p[2] - SKT.last_p[2]) + fabsf(p[1] - SKT.last_p[1]) > 40.0f;  // (a warp / goto)
    bool loaded = g_load_end != SKT.load_seen;  // (a load ended since the last tick: the frame body returned early through it)
    SKT.load_seen = g_load_end;
    bool safe_now = false;
    if (SKT.riding) {
        const char *why = !SKT.on ? "switched off" : block ? "dead / loading" : loaded ? "a load" : jumped ? "a warp"
                          : (st != LINK_GROUND && st != LINK_AIR) ? "Link's own state"
                          : g_link_owns || g_event ? "an event" : mk_body() ? "Mickey" : fm_in_form() ? "a Drive Form"
                          : skt_body_kind() != SKT.rw ? "a body switch" : NULL;
        bool dry = link_water_depth() <= 0.02f;
        if (why) skt_dismount(why, false, SKT.on && !block && !loaded && !jumped && st == LINK_GROUND && !ACT_AIR(S.act) && dry);
        safe_now = !why && st == LINK_GROUND && !ACT_AIR(S.act) && !S.air_frames && dry;
    }
    memcpy(SKT.last_p, p, sizeof SKT.last_p), SKT.tick_ok = true;
    if (!SKT.on) {
        if (SKT.drawn && SKT.act[SKT_BOARD] > 0) SKT.off(SKT.act[SKT_BOARD]);
        SKT.drawn = false, SKT.reach = false, SKT.eat = 0;
        return;
    }
    float lm[12];
    if (SKT.riding && linkpos_link_mtx(lm)) SKT.pos[0] = lm[3], SKT.pos[1] = lm[7], SKT.pos[2] = lm[11];
    else if (SKT.riding) memcpy(SKT.pos, p, sizeof SKT.pos);
    if (SKT.riding) SKT.snapped = true;  // (the board under his feet; where it stays when he gets off)
    if (SKT.riding && safe_now) memcpy(SKT.safe, SKT.pos, sizeof SKT.safe), SKT.safe_ok = true;
    if (SKP.on && !SKT.riding) {  // (s110 pocket) back at his feet after 0.5 s standing on dry ground (no event / load / death)
        bool ground = can && st == LINK_GROUND && !ACT_AIR(S.act) && !S.air_frames && link_water_depth() <= 0.02f && !g_event && !g_link_owns;
        ULONGLONG now = GetTickCount64();
        if (!ground) SKP.since = 0;
        else if (!SKP.since) SKP.since = now;
        else if (now - SKP.since >= 500) {
            float f[2] = {sinf(S.face), cosf(S.face)};
            SKT.pos[0] = p[0] + f[0] * 1.2f, SKT.pos[1] = p[1], SKT.pos[2] = p[2] + f[1] * 1.2f, SKT.yaw = S.face + PI / 2, SKT.snapped = false;
            SKP.on = false, SKT.cd_until = now + 300;
            LOG("[skate] the board is back from the pocket at %.1f %.1f %.1f (in front of you)\n", SKT.pos[0], SKT.pos[1], SKT.pos[2]);
            skt_save_now("pocket");
        }
    }
    if (!SKT.riding && !SKP.on) {  // (s110, s118: five boards) the nearest one (by SKB_HYST) is the drawn / rideable one
        float nd, od = hypotf(p[0] - SKT.pos[0], p[2] - SKT.pos[2]);
        int nb = skt_nearest(p, &nd);
        if (nb != SKB_CUR) {
            int from = SKB_CUR;
            if (SKT.drawn && SKT.act[SKT_BOARD] > 0) SKT.off(SKT.act[SKT_BOARD]);
            SKT.drawn = false, skt_use(nb);
            LOG("[skate] nearest board now %s (%.0f u away, %.1f %.1f %.1f); was %s (%.0f u away)\n", SKB_NAME[nb], nd, SKT.pos[0], SKT.pos[1], SKT.pos[2],
                SKB_NAME[from], od);
        }
    }
    float dx = p[0] - SKT.pos[0], dz = p[2] - SKT.pos[2], d = hypotf(dx, dz);
    // within 300 u: load / draw the parked board; on the ground (not riding) within 150 u: stand it on the floor (a ray down from just above)
    int bo = SKT.act[SKT_BOARD];
    if (d > 300.0f || !can || SKT.riding || SKP.on || !skt_actor(SKT_BOARD, present)) {  // (s110: + in the pocket)
        if (SKT.drawn && bo > 0) SKT.off(bo);
        SKT.drawn = false;
    } else {
        bo = SKT.act[SKT_BOARD];
        if (d < 150.0f && (!SKT.snapped ? present - SKT.snap_at >= 20 : present - SKT.snap_at >= 120)) {
            SKT.snap_at = present;
            float a[3] = {SKT.pos[0], SKT.pos[1] + (SKT.snapped ? 3.0f : 30.0f), SKT.pos[2]};
            float b[3] = {SKT.pos[0], SKT.pos[1] - (SKT.snapped ? 8.0f : 60.0f), SKT.pos[2]}, h[3] = {0}, n[3] = {0};
            if (linkpos_raycast(a, b, h, n) && fabsf(h[1] - SKT.pos[1]) < 40.0f && n[1] > 0.5f) {
                if (g_skt_log && fabsf(h[1] - SKT.pos[1]) > 0.05f) LOG("[skate] floor ray: board y %.2f -> %.2f\n", SKT.pos[1], h[1]);
                SKT.pos[1] = h[1], SKT.snapped = true;
            }
        }
        SKT.pose(bo, 0, 0.0f, SKT.pos, SKT.yaw, 0.01f * g_k);
        SKT.drawn = true;
    }
    // near the board: load the current body's rider (a 15 MB pose file: before the Triangle press, not at it)
    if (can && !SKT.riding && d < 60.0f) skt_actor(skt_body_kind(), present);
    // in reach of the board: X is its (the exe strips BotW's side through skt_eat_x)
    bool free = st == LINK_GROUND && S.act == ACT_IDLE && !S.special && S.takeoff <= 0 && !S.air_frames && !g_link_owns && !g_event &&
                !mk_body() && !fm_in_form();
    SKT.reach = can && !SKT.riding && !SKP.on && SKT.act[SKT_BOARD] > 0 && SKT.act[skt_body_kind()] > 0 && free && GetTickCount64() >= SKT.cd_until &&
                d <= g_skt_reach && fabsf(p[1] - SKT.pos[1]) < 1.8f;
    if (SKT.reach && !SKT.was_near) sora_sfx_event("cursor", 0.8f), LOG("[skate] the board is in reach (%.1f u): Triangle = ride\n", d);
    SKT.was_near = SKT.reach;
    SKT.eat = SKT.riding || SKT.reach;  // (s110 review: written once per tick, never 0 in between: the input thread reads it any time)
}

// the frame's presses (after press / held are known): Triangle gets on / off; the board eats the swing / roll / X presses (Square in the air = a trick)
static void skt_input(uint64_t *press, uint64_t *held, uint64_t present, int st) {
    (void)present;
    if (!SKT.on) return;
    if (SKT.riding) {
        if ((*press & BTN_CLIMB) && st == LINK_GROUND && S.act == ACT_IDLE && !S.special && S.takeoff <= 0 && !S.air_frames)
            skt_dismount("Triangle", true, true);
        if (SKT.riding && (*press & BTN_ROLL) && ACT_AIR(S.act)) SKT.trick_req = true;
        // (s110 review: + L, the shortcut menu: L + B cast a spell on the hidden body and Triangle could not get off until it ended)
        *press &= ~(uint64_t)(BTN_ATTACK | BTN_ROLL | BTN_CLIMB | BTN_SHORTCUT), *held &= ~(uint64_t)(BTN_CLIMB | BTN_SHORTCUT);
        if (ACT_AIR(S.act)) *press &= ~(uint64_t)BTN_JUMP;  // (no second B in the air: no Aerial Dodge / glide off the board)
    } else if (SKT.reach) {
        if (*press & BTN_CLIMB) {
            SKT.safe_ok = true, memcpy(SKT.safe, SKT.pos, sizeof SKT.safe);  // (the parked spot: dry ground, the reach test needs him on the ground next to it)
            SKT.riding = SKT.rider_on = true, SKT.heading = S.face, SKT.speed = 0, SKT.turn = 0, SKT.v[0] = SKT.v[1] = 0, SKT.stall = 0, SKT.have_last = false;
            SKT.carry_n = 0, SKT.rw = skt_body_kind(), SKT.rs = SM_IDLE, SKT.rc = 0, SKT.rdone = false, SKT.was_air = false, SKT.trick_req = false;
            if (SKT.off && SKT.act[SKT_BOARD] > 0) SKT.off(SKT.act[SKT_BOARD]);  // (the parked board = the rider actor's own board bones now)
            if (SKT.body && SKT.off) SKT.off(SKT.body());                         // (the normal body: not drawn while the rider is)
            SKT.drawn = false;
            LOG("[skate] ON the board at %.1f %.1f %.1f (%s rider, heading %.0f deg)\n", SKT.pos[0], SKT.pos[1], SKT.pos[2],
                SKT.rw == SKT_ROX ? "Roxas" : "Sora", SKT.heading * 180 / PI);
        }
        *press &= ~(uint64_t)BTN_CLIMB, *held &= ~(uint64_t)BTN_CLIMB;
    }
}

// riding, in place of the stick's walk / run (the caller zeroes `speed` for the locomotion pick afterwards): steer, push, coast, brake
static void skt_ride(const float sdir[2], float sspeed, float kh, float to_u, const float p[3]) {
    if (S.special == SP_DAMAGE || S.act == ACT_GLIDE || S.act == ACT_GLIDE_HOVER) {
        skt_dismount(S.special == SP_DAMAGE ? "hit" : "glide", false, S.special == SP_DAMAGE && !ACT_AIR(S.act) && !S.air_frames && link_water_depth() <= 0.02f);
        return;
    }
    bool air = ACT_AIR(S.act) || S.air_frames > 0;
    float vl = hypotf(SKT.v[0], SKT.v[1]);
    // the body's real move since last frame: a wall / a rock that stopped it slows the board down to it
    float actual = SKT.have_last ? hypotf(p[0] - SKT.prev_p[0], p[2] - SKT.prev_p[2]) / (to_u * kh) : vl;
    SKT.have_last = true;
    memcpy(SKT.prev_p, p, sizeof SKT.prev_p);
    SKT.stall = !air && vl > 2.0f && actual < 0.5f * vl ? SKT.stall + 1 : 0;
    if (SKT.stall >= 3) SKT.speed *= powf(0.5f, kh), SKT.stall = 3;
    float max = g_skt_max;
    float want = sspeed > 0 ? atan2f(sdir[0], sdir[1]) : SKT.heading, dh = wrap(want - SKT.heading);
    float sp = SKT.speed, h0 = SKT.heading;
    float rate = g_skt_turn * (1.0f - 0.5f * (sp > max ? 1.0f : sp / max)) * (sp < 2.0f ? 3.0f : 1.0f) * (air ? 0.3f : 1.0f);
    if (sspeed > 0) {
        float step = rate * kh;
        SKT.heading = wrap(SKT.heading + (dh > step ? step : dh < -step ? -step : dh));
    }
    SKT.turn = wrap(SKT.heading - h0) / (kh > 0.01f ? kh : 1.0f);
    if (!air) {
        if (sspeed > 0) {
            float cap = sspeed >= F_RUN ? max : 0.55f * max, al = cosf(dh);  // (a half push = a cruise at 55 %)
            if (al < -0.3f) sp -= g_skt_brake * kh;
            else if (sp < cap) sp += g_skt_acc * kh * (al > 0.5f ? 1.0f : 0.5f);
            else sp -= g_skt_brake * 0.25f * kh;
        } else sp *= powf(1.0f - g_skt_fric, kh);
    }
    SKT.speed = sp < 0 ? 0 : sp > max * 1.2f ? max * 1.2f : sp;
    SKT.v[0] = sinf(SKT.heading) * SKT.speed, SKT.v[1] = cosf(SKT.heading) * SKT.speed;
    S.face = SKT.heading;  // (the rider's model faces +z = the board's nose: the KH2 skate motions are made that way)
    if (g_skt_log && ++SKT.logn % 20 == 0) LOG("[skate] speed %.2f heading %.0f deg%s actual %.2f motion %d clock %.0f\n", SKT.speed, SKT.heading * 180 / PI, air ? " (air)" : "", actual, SKT.rs, SKT.rc);
}

// the horizontal move of this step (cm) in place of the ground / air branch's: the board's velocity while riding, the get-off slide after
static void skt_move(float move[2], float kh) {
    if (SKT.riding) {
        move[0] = SKT.v[0] * kh, move[1] = SKT.v[1] * kh;
        S.vel[0] = SKT.v[0], S.vel[1] = SKT.v[1];
        if (S.act == ACT_IDLE) S.ground_move = hypotf(move[0], move[1]), W.cmd[0] = move[0], W.cmd[1] = move[1];
    } else if (SKT.carry_n > 0 && S.act == ACT_IDLE && !S.special) {
        SKT.carry_n--;
        move[0] += SKT.carry[0] * kh, move[1] += SKT.carry[1] * kh;
        float k = powf(0.88f, kh);
        SKT.carry[0] *= k, SKT.carry[1] *= k;
        S.vel[0] = move[0] / kh, S.vel[1] = move[1] / kh, S.ground_move = hypotf(move[0], move[1]), W.cmd[0] = move[0], W.cmd[1] = move[1];
    } else SKT.carry_n = 0;
}

static bool skt_slot_info(int slot, int *frames, int *loop) {
    for (unsigned i = 0; i < sizeof SKB_SLOTS / sizeof SKB_SLOTS[0]; i++)
        if (SKB_SLOTS[i].slot == slot) return *frames = SKB_SLOTS[i].frames, *loop = SKB_SLOTS[i].loop, true;
    return false;
}
static void skt_play_slot(int slot) { if (SKT.rs != slot) SKT.rs = slot, SKT.rc = 0, SKT.rdone = false; }
static bool skt_is_trick(int slot) { for (int i = 0; i < 4; i++) if (SKT_TRICKS[i] == slot) return true; return false; }

// riding: pick the KH2 skate motion for what the board is doing, advance it, draw the rider actor (rider + board) at Link's feet in place of the body
static void skt_rider_draw(float kh, float to_u) {
    int ra = SKT.act[SKT.rw];
    if (ra <= 0 || !SKT.pose) return;
    bool air = ACT_AIR(S.act) || S.takeoff > 0;
    int frames = 1, loop = -1;
    // advance the clock (60 Hz frames; kh = the KH frames of this step)
    if (skt_slot_info(SKT.rs, &frames, &loop)) {
        SKT.rc += kh;
        if (SKT.rc >= (float)frames) {
            // (s110 review: rdone = "played to its end once", a loop too: the push-off 152 loops (ret 0) and stayed on forever, never cruise / lean)
            if (loop >= 0 && frames > loop) SKT.rc = (float)loop + fmodf(SKT.rc - (float)frames, (float)(frames - loop)), SKT.rdone = true;
            else SKT.rc = (float)frames, SKT.rdone = true;
        }
    }
    bool trick = skt_is_trick(SKT.rs);
    if (air) {
        if (!SKT.was_air && SKT.rs != SM_OLLIE) skt_play_slot(SM_OLLIE), skt_play(3, 0.9f);  // (the pop; KH2's board triggers se334 #18 here)
        if (SKT.trick_req && !trick && (SKT.rs == SM_AIR || (SKT.rs == SM_OLLIE && SKT.rc > 18.0f))) {
            int t = SKT.tn++ & 3;
            skt_play_slot(SKT_TRICKS[t]), skt_play(6 + t, 0.9f);  // (se334 #21..#24 = SCD sounds 6..9, the flip tricks' own)
            trick = true;
        }
        if (!trick && SKT.rs != SM_AIR && !(SKT.rs == SM_OLLIE && !SKT.rdone)) skt_play_slot(SM_AIR);
        if (trick && SKT.rdone) skt_play_slot(SM_AIR);
    } else {
        if (SKT.was_air) skt_play_slot(SM_LAND), skt_play(4, 0.9f);  // (se334 #19: the landing)
        else if (SKT.rs == SM_LAND || SKT.rs == SM_PUSH) {
            if (SKT.rdone) skt_play_slot(SKT.speed > 1.0f ? SM_CRUISE : SM_IDLE);
        } else if (SKT.speed < 0.4f) skt_play_slot(SM_IDLE);
        else {
            // (s110, user: "the animation will lean left to right very often") the raw heading change per frame jitters around 0
            // (the turn steps to the stick then stops; camera-relative stick wobble): smoothed (~0.25 s), a lean needs a real turn
            // (1.0 rad/s) to start and holds >= 0.4 s, it ends below 0.35 rad/s, and the other side only comes through the cruise
            static float lean_f;
            float lean = SKT.turn * 60.0f, k = 1.0f - powf(0.5f, kh / 9.0f);  // rad / s; half-life 9 KH frames
            lean_f += (lean - lean_f) * k;
            bool leaning = SKT.rs == SM_LEAN_A || SKT.rs == SM_LEAN_B, held = SKT.rdone || SKT.rc >= 24.0f;  // (a short looping lean clip: rdone after its first play)
            int lean_r = g_skt_flip ? SM_LEAN_B : SM_LEAN_A, lean_l = g_skt_flip ? SM_LEAN_A : SM_LEAN_B;
            if (SKT.rs == SM_IDLE) skt_play_slot(SM_PUSH), lean_f = 0;  // (pushing off from a standstill)
            else if (leaning) { if (held && (fabsf(lean_f) < 0.35f || SKT.speed <= 2.0f)) skt_play_slot(SM_CRUISE); }
            else if (lean_f > 1.0f && SKT.speed > 2.0f) skt_play_slot(lean_r);
            else if (lean_f < -1.0f && SKT.speed > 2.0f) skt_play_slot(lean_l);
            else if (SKT.rs != SM_CRUISE) skt_play_slot(SM_CRUISE);
        }
    }
    SKT.trick_req = false, SKT.was_air = air;
    skt_roll_loop(!air && SKT.speed > 2.0f);
    float lm[12];
    const float *feet = linkpos_link_mtx(lm) ? (SKT.pos[0] = lm[3], SKT.pos[1] = lm[7], SKT.pos[2] = lm[11], SKT.pos) : NULL;
    if (SKT.shadow) SKT.shadow(ra, S.ground_z * g_k / 100.0f, blob_str(0.55f));
    SKT.pose(ra, SKT.rs, SKT.rc, feet, S.face, to_u);
}

static bool skt_live_set(const char *key, int v) {
    if (!strcmp(key, "skate")) {
        SKT.on = v != 0;
        LOG("[skate] %s\n", SKT.on ? "on" : "off");
    } else if (!strcmp(key, "skatex")) SKT.pos[0] = (float)v, SKT.snapped = false, LOG("[skate] board x %d\n", v);
    else if (!strcmp(key, "skatey")) SKT.pos[1] = (float)v, SKT.snapped = false, LOG("[skate] board y %d\n", v);
    else if (!strcmp(key, "skatez")) SKT.pos[2] = (float)v, SKT.snapped = false, LOG("[skate] board z %d\n", v);
    else if (!strcmp(key, "skateyaw")) SKT.yaw = (float)v * PI / 180.0f, LOG("[skate] board yaw %d deg\n", v);
    else if (!strcmp(key, "skatehere")) {  // the board at Link's feet, facing as he does (saved with the next BotW save)
        if (SKT.riding) skt_dismount("live skatehere", false, true);
        memcpy(SKT.pos, SKT.last_p, sizeof SKT.pos), SKT.yaw = S.face, SKT.snapped = false;
        LOG("[skate] board put at %.1f %.1f %.1f\n", SKT.pos[0], SKT.pos[1], SKT.pos[2]);
    } else if (!strcmp(key, "skatehome")) {
        if (SKT.riding) skt_dismount("live skatehome", false, true);
        SKP.on = false;  // (review: else the pocket would put the live board back at Link's feet 0.5 s later)
        skt_home(), LOG("[skate] all %d boards back at their shrines (Oman Au, Shrine of Resurrection, Ja Baij, Owa Daim, Keh Namut)\n", SKB_N);
    } else if (!strcmp(key, "skatelog")) g_skt_log = v != 0;
    else if (!strcmp(key, "skatesnd")) g_skt_snd = v != 0, SKT.snd_state = -1;
    else if (!strcmp(key, "skloadhome")) g_skt_loadhome = v != 0, LOG("[skate] skloadhome %d (a save load puts all five boards back at their spawns)\n", g_skt_loadhome);
    else if (!strcmp(key, "skatehome2")) {  // (s110, s118) every board that is NOT the drawn one (the farther ones) back at its shrine
        if (v) {
            for (int k = 0; k < SKB_N; k++)
                if (k != SKB_CUR) skt_home_one(k);
            SKP.on = false;
            LOG("[skate] the %d farther boards are back at their shrines (the nearest, %s, stays)\n", SKB_N - 1, SKB_NAME[SKB_CUR]);
        }
    } else if (!strcmp(key, "skatehomen")) {  // (s118) "set skatehomen N": board N (0 Oman Au, 1 Resurrection, 2 Ja Baij, 3 Owa Daim, 4 Keh Namut) back at its shrine
        if (v >= 0 && v < SKB_N) {
            if (v == SKB_CUR && SKT.riding) skt_dismount("live skatehomen", false, true);
            if (v == SKB_CUR) SKP.on = false;  // (review: the pocket holds the live board)
            skt_home_one(v);
            LOG("[skate] board %d (%s) back at its shrine %.1f %.1f %.1f\n", v, SKB_NAME[v], SKB_HOME[v][0], SKB_HOME[v][1], SKB_HOME[v][2]);
        }
    } else if (!strcmp(key, "skatenear")) {  // (s118) logs all five boards: where, how far from Link, which one is the drawn / rideable one
        skt_store();
        const float *lp = SKT.last_p;
        for (int k = 0; k < SKB_N; k++)
            LOG("[skate] board %d %-22s %.1f %.1f %.1f yaw %.3f  %.0f u from you%s\n", k, SKB_NAME[k], BRD[k].pos[0], BRD[k].pos[1], BRD[k].pos[2], BRD[k].yaw,
                hypotf(lp[0] - BRD[k].pos[0], lp[2] - BRD[k].pos[2]), k == SKB_CUR ? "  <- drawn / rideable" : "");
    } else if (!strcmp(key, "skpocket")) g_skt_pocket = v != 0, LOG("[skate] pocket %d (a climb / swim / glide get-off keeps the board with you)\n", g_skt_pocket);
    else if (!strcmp(key, "skflip")) g_skt_flip = v != 0, LOG("[skate] lean motions %s\n", g_skt_flip ? "swapped" : "as baked");
    else if (!strcmp(key, "skjump")) g_skt_jump = v < 20 ? 20.0f : v > 600 ? 600.0f : (float)v, LOG("[skate] ollie height %.0f cm\n", g_skt_jump);
    else if (!strcmp(key, "skmax")) g_skt_max = v < 20 ? 2.0f : v > 600 ? 60.0f : (float)v / 10.0f, LOG("[skate] top speed %.1f cm/frame\n", g_skt_max);
    else if (!strcmp(key, "skacc")) g_skt_acc = v < 1 ? 0.01f : (float)v / 100.0f, LOG("[skate] acceleration %.2f\n", g_skt_acc);
    else if (!strcmp(key, "skbrake")) g_skt_brake = v < 1 ? 0.01f : (float)v / 100.0f, LOG("[skate] brake %.2f\n", g_skt_brake);
    else if (!strcmp(key, "skfric")) g_skt_fric = v < 0 ? 0 : v > 500 ? 0.05f : (float)v / 10000.0f, LOG("[skate] coast friction %.4f\n", g_skt_fric);
    else if (!strcmp(key, "skturn")) g_skt_turn = v < 5 ? 0.005f : (float)v / 1000.0f, LOG("[skate] turn %.3f rad/frame\n", g_skt_turn);
    else if (!strcmp(key, "skr")) g_skt_reach = v < 5 ? 0.5f : (float)v / 10.0f, LOG("[skate] get-on radius %.1f u\n", g_skt_reach);
    else return false;
    return true;
}

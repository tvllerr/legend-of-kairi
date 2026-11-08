// (s131 savept) KH2 save points at the shrines: a green ring on the ground (KH2's Cure ring fx as the stand-in) beside each Great Plateau
// shrine's skateboard spot (SKB_HOME, sora_skate.h; those 5 are the only shrine spots the mod knows). Stepping into it refills Sora's HP and
// MP like KH2's save point (the Drive gauge is NOT refilled: KH2's save point restores HP and MP only, believed - not checked in the exe).
// The BotW save stays BotW's (the mod adds no "Save?" panel). Sheikah Towers: the mod has no tower positions, so none yet.
// Live keys: savept 0/1 (default 1; 0 = no rings, no refill) | savefx 0/1 (the ring visuals) | saver N (trigger radius x10 u, default 18 = 1.8 m, GUESS)
//            | savemenu 0/1 (s131: stepping in opens BotW's pause menu) | saveside / savefwd N (x10 u from the door) | saveflip<i> 0/1
//            | saveoff N (distance from the board spot, x10 u, default 35, INVENTED) | savelog 0/1 | savehere (debug: put a point at Sora's feet, index 5)
// INVENTED: the ring is Cure's ring, not KH2's SAVE_POINT.a.us (obj/SAVE_POINT.a.us + SAVE_POINT.mdlx exist in the extract but are not in fx.bin);
// the heal sound is the Cure heal chime (KH2's save point SE is not in sfx.bin). The point stands beside the board (INVENTED: KH2 puts it at the entrance).
#pragma once
static uint64_t g_camp_skip_until;  // (s131) read by sora_camp.h camp_pad: no KH2 camp for this + (BotW's own menu)

#define SV_N (SKB_N + 1)  // the 5 shrines + the debug point
static struct {
    int on, fx, log, r, off, size, side, fwd, menu;
    bool flip[SV_N];
    uint64_t menu_at;  // (s131) when to press Plus (0 = none)
    int inside;        // (s131 user: Triangle in the ring opens the menu, like KH2) point index + 1 Sora stands in this frame, 0 = none
    uint64_t tri_cool; // no Save prompt before this (ms): after a use, the menu comes up first
    int tab_btn, tab_n, tab_delay, tab_gap;  // (s131 exe batch) after BotW's menu opens: press tab_btn (input.c bit) tab_n times -> the System tab
    int tab_left;      // presses still to do
    uint64_t tab_at;   // next press (ms)
    bool init, armed[SV_N], have_dbg;
    float pos[SV_N][3];
    int fxh[SV_N];  // fx handle + 1 (0 = none)
    uint32_t fxg[SV_N];  // g_fx_gen of that handle (fx_mine)
    uint64_t snap_ms[SV_N];
} SV = {.on = 1, .fx = 1, .log = 1, .r = 18, .off = 35, .size = 45, .side = 105, .fwd = 10, .menu = 1, .tab_btn = 7, .tab_n = 1, .tab_delay = 700, .tab_gap = 250};  // tab: R once, GUESS (tune live: savetabbtn/savetabn/savetabms)  // menu: 1 = Triangle "Save" prompt in the ring (KH2), 2 = Plus pressed by itself on entry (s131 first try), 0 = none  // (s131 user: half size: size 90 -> 45)

// (s131 user mark_223927_1 at Ja Baij: "to the side of the shrine, left or right") the shrine door (actor pos + rot, from the
// sora_skate.h comments); the point = door + forward x fwd + side x side (Ja Baij: 1.0 m out, 10.5 m to the side = the user's spot).
// No door known (Shrine of Resurrection exit) = beside the board as before. saveflip<i> 1 = the other side.
static const float SV_DOOR[SKB_N][5] = {  // x y z rot, known
    {-673.2f, 172.1f, 1513.0f, 0.900f, 1}, {0, 0, 0, 0, 0}, {-446.7f, 179.0f, 1990.2f, -1.5708f, 1},
    {-925.0f, 273.2f, 2321.2f, 3.1416f, 1}, {-1436.3f, 336.6f, 1991.0f, 1.8599f, 1}};
static void sv_place(int i) {  // pos of point i
    SV.snap_ms[i] = 0;
    if (i < SKB_N && SV_DOOR[i][4] > 0) {
        float r = SV_DOOR[i][3], f = (float)SV.fwd / 10.0f, sd = (float)SV.side / 10.0f * (SV.flip[i] ? -1.0f : 1.0f);
        SV.pos[i][0] = SV_DOOR[i][0] + sinf(r) * f - cosf(r) * sd, SV.pos[i][1] = SV_DOOR[i][1], SV.pos[i][2] = SV_DOOR[i][2] + cosf(r) * f + sinf(r) * sd;
        return;
    }
    float yaw = SKB_HOME[i][3], o = (float)SV.off / 10.0f;
    SV.pos[i][0] = SKB_HOME[i][0] + sinf(yaw) * o, SV.pos[i][1] = SKB_HOME[i][1], SV.pos[i][2] = SKB_HOME[i][2] + cosf(yaw) * o;
    SV.snap_ms[i] = 0;
}
static void sv_snap(int i, uint64_t now) {  // stand it on the floor (a ray down), at most every 3 s
    if (now - SV.snap_ms[i] < 3000) return;
    SV.snap_ms[i] = now;
    float a[3] = {SV.pos[i][0], SV.pos[i][1] + 6.0f, SV.pos[i][2]}, b[3] = {SV.pos[i][0], SV.pos[i][1] - 10.0f, SV.pos[i][2]}, h[3] = {0}, n[3] = {0};
    if (linkpos_raycast(a, b, h, n) && n[1] > 0.5f) SV.pos[i][1] = h[1] + 0.05f;
}
static void sv_off_all(void) {
    for (int i = 0; i < SV_N; i++) {
        if (SV.fxh[i] > 0 && fx_mine(SV.fxh[i] - 1, SV.fxg[i])) sora_fx_kill(SV.fxh[i] - 1);
        SV.fxh[i] = 0;
    }
}

static void sv_refill(const float p[3], int i) {
    int life = linkpos_link_life(), life2 = life;
    if (life > 0) life2 = link_heal(999);  // up to BotW's max hearts
    float mp0 = MP.mp;
    bool ch = mp_charging();
    MP.mp = MP.mp_max, MP.charge = MP.charge_total = 0;
    if (SV.log) LOG("[savept] %s save point: HP %d -> %d (max %d), MP %.0f%s -> %.0f; Drive untouched (%d bars + %d)\n", i < SKB_N ? SKB_NAME[i] : "debug", life, life2,
                    g_hud_life_max, mp0, ch ? " (charging)" : "", MP.mp, DG.bars, DG.fill);
    sora_sfx_event("cure_heal", 0.8f);  // GUESS / INVENTED SE
    int id = sora_fx_id("cure_sparkle");
    if (SV.fx && id >= 0) sora_fx_spawn(id, p, S.face, 0.01f * g_k);
    if (SV.menu == 2) SV.menu_at = GetTickCount64() + 700;  // (s131 user: open BotW's menu to save / load / options) after the sparkle, GUESS 0.7 s
}
// (s131) press Plus for the game = BotW's pause menu (the exe's own injector, hle/input.c g_inject_plus; movie skip uses it too)
static void sv_menu_now(void) {  // (s131) the Save prompt's Triangle (sora_react.h) or savemenu 2
    static volatile int *inj;
    static int tried;
    SV.tri_cool = GetTickCount64() + 1500;  // GUESS: the prompt waits until the menu is up (the game pauses) / closed again
    if (!tried) tried = 1, inj = (volatile int *)(void *)GetProcAddress(GetModuleHandleA(NULL), "g_inject_plus");
    if (inj && SV.tab_n > 0) SV.tab_left = SV.tab_n, SV.tab_at = GetTickCount64() + (uint64_t)SV.tab_delay;
    if (inj) g_camp_skip_until = GetTickCount64() + 1000, *inj = 1, LOG("[savept] Plus pressed: BotW's menu (save / load / options)\n");
    else LOG("[savept] no g_inject_plus in this exe: no menu\n");
}
static void sv_tab_tick(uint64_t now) {  // (s131) the System tab jump: needs the exe's input_inject_buttons (NULL = an older exe: no jump)
    static void (*inj)(uint64_t, int);
    static int tried;
    if (!SV.tab_left || now < SV.tab_at) return;
    if (!tried) tried = 1, inj = (void (*)(uint64_t, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "input_inject_buttons");
    if (!inj || SV.tab_btn < 0 || SV.tab_btn > 15) { SV.tab_left = 0; return; }
    inj(1ull << SV.tab_btn, 4);  // GUESS: 4 presented frames = one press
    SV.tab_left--, SV.tab_at = now + (uint64_t)SV.tab_gap;
    LOG("[savept] tab jump: button bit %d (%d left)\n", SV.tab_btn, SV.tab_left);
}
static void sv_menu_tick(uint64_t now, bool block) {
    sv_tab_tick(now);  // (runs while BotW's menu is up: sv_tick is called paused too - CHECK after the relink)
    if (!SV.menu_at || now < SV.menu_at) return;
    SV.menu_at = 0;
    if (!block) sv_menu_now();  // (block: a menu / cutscene came up meanwhile)
}
static bool sv_offer(void) { return SV.on && SV.menu == 1 && SV.inside && GetTickCount64() >= SV.tri_cool; }  // sora_react.h: show "Save"

static void sv_tick(uint64_t present, const float p[3], bool block) {
    (void)present;
    uint64_t now = GetTickCount64();
    if (!SV.init) {
        SV.init = true;
        for (int i = 0; i < SKB_N; i++) sv_place(i), SV.armed[i] = true;
        LOG("[savept] %d save points beside the shrine boards (Oman Au, Shrine of Resurrection, Ja Baij, Owa Daim, Keh Namut)\n", SKB_N);
    }
    sv_menu_tick(now, block);
    SV.inside = 0;
    if (!SV.on || block) {
        if (!SV.on) sv_off_all();
        return;
    }
    for (int i = 0; i < SV_N; i++) {
        if (i == SKB_N && !SV.have_dbg) continue;
        float dx = p[0] - SV.pos[i][0], dz = p[2] - SV.pos[i][2], d = hypotf(dx, dz);
        if (d < 60.0f) sv_snap(i, now);
        if (SV.fx && d < 40.0f) {  // the ring's fx only close by (32 fx slots are shared with the spells)
            bool alive = SV.fxh[i] > 0 && fx_mine(SV.fxh[i] - 1, SV.fxg[i]) && sora_fx_alive(SV.fxh[i] - 1);
            if (!alive) {
                int id = sora_fx_id("cure_ring");
                int h = id >= 0 ? sora_fx_spawn(id, SV.pos[i], 0, 0.01f * g_k * (float)SV.size / 100.0f) : -1;
                SV.fxh[i] = h >= 0 ? h + 1 : 0, SV.fxg[i] = h >= 0 && h < 32 ? g_fx_gen[h] : 0;
            }
        } else if (SV.fxh[i] > 0) {
            if (fx_mine(SV.fxh[i] - 1, SV.fxg[i])) sora_fx_kill(SV.fxh[i] - 1);
            SV.fxh[i] = 0;
        }
        float rr = (float)SV.r / 10.0f;  // trigger radius GUESS 1.8 m; re-arm 2.5 m further out GUESS; fx range 40 m GUESS
        bool in = d < rr && fabsf(p[1] - SV.pos[i][1]) < 2.0f;
        if (in) SV.inside = i + 1;
        if (SV.armed[i] && in) SV.armed[i] = false, sv_refill(p, i);
        else if (!SV.armed[i] && d > rr + 2.5f) SV.armed[i] = true;
    }
}

static bool sv_live_set(const char *key, int v) {
    if (!strcmp(key, "savept")) {
        SV.on = v != 0, LOG("[savept] %s\n", SV.on ? "on" : "off");
        if (!SV.on) sv_off_all();
    } else if (!strcmp(key, "savefx")) SV.fx = v != 0, LOG("[savept] ring visuals %s\n", SV.fx ? "on" : "off");
    else if (!strcmp(key, "savesize")) SV.size = v < 5 ? 5 : v > 300 ? 300 : v, LOG("[savept] ring size %d\n", SV.size);
    else if (!strcmp(key, "savetabbtn")) SV.tab_btn = v;  // (s131) input.c bit: 6 L, 7 R, 8 ZL, 9 ZR, 12 LEFT, 14 RIGHT
    else if (!strcmp(key, "savetabn")) SV.tab_n = v < 0 ? 0 : v > 6 ? 6 : v;
    else if (!strcmp(key, "savetabms")) SV.tab_delay = v < 100 ? 100 : v > 5000 ? 5000 : v;
    else if (!strcmp(key, "savemenu")) SV.menu = v < 0 ? 0 : v > 2 ? 2 : v;
    else if (!strcmp(key, "saveside") || !strcmp(key, "savefwd")) {
        *(key[4] == 's' ? &SV.side : &SV.fwd) = v < -300 ? -300 : v > 300 ? 300 : v;
        for (int i = 0; i < SKB_N; i++) sv_place(i);
    } else if (!strncmp(key, "saveflip", 8) && key[8] >= '0' && key[8] < '0' + SKB_N && !key[9]) SV.flip[key[8] - '0'] = v != 0, sv_place(key[8] - '0');
    else if (!strcmp(key, "saver")) SV.r = v < 5 ? 5 : v > 100 ? 100 : v;
    else if (!strcmp(key, "saveoff")) {
        SV.off = v < 0 ? 0 : v > 200 ? 200 : v;
        for (int i = 0; i < SKB_N; i++) sv_place(i);
    } else if (!strcmp(key, "savelog")) SV.log = v != 0;
    else if (!strcmp(key, "savehere")) {
        float p[3];
        if (linkpos_link_pos(p)) memcpy(SV.pos[SKB_N], p, sizeof p), SV.have_dbg = v != 0, SV.armed[SKB_N] = false, LOG("[savept] debug point at %.1f %.1f %.1f\n", p[0], p[1], p[2]);
    } else return false;
    return true;
}

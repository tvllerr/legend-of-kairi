// sora_camp_walk.h: (s92 b6-menuwalk) Sora / Donald / Goofy as live 3D models behind the KH2 camp menu.
// recon/S92_HELPER_MENUWALK.md. DLL only (hot reload): it rides on the exe's existing screen mode (sora_draw_screen, the
// KH2 Game Over's: own camera, no scene depth, a black rect first, drawn at present BEFORE the camp quads) and on
// sora_draw_actor_pose (s54 party draw). While the camp is up and the page is the top page (menuwalk 2: every page) the
// three actors are posed with a fixed menu camera; the camp's opaque black backdrop quad is dropped (g_cp_nobackdrop) so
// they show through the camp's own translucent chrome. Any failure (exe without the exports, no party bake) = the old
// black backdrop. Live: menuwalk 0 off / 1 idle / 2 walk in place (+ every page), mwd mwface mwyaw, (s93) mwsx mwdx mwgx mwfy mwin mwincm.
// Included by sora_camp.h (needs CAMP, PD, PARTY, g_party_id, mot_of, g_k).
#pragma once

static struct {
    bool tried, ok, on, logged;
    int mode;                // 0 off, 1 idle on the top page, 2 walk in place on every page
    int x_pct, y_pct, dist;  // (s92, unused since s93) group centre on screen; camera distance (cm)
    int face_deg;            // how far they turn (deg, + = toward the screen's right) once they stand at their plate
    int yaw_deg;             // camera yaw around the group (deg)
    int age;                 // frames drawn in a row (the backdrop goes only after the actors surely reached the screen)
    // (s93 user: "they are just standing off to the side instead of walking over where their names are": the top page's
    // three plates, left to right Goofy / Donald / Sora. Each stands above its own plate: feet at (px, fy) % of the game
    // image (x = the plate's centre, measured on the user's 2000 px screenshot; GUESS, tune live mwsx/mwdx/mwgx/mwfy),
    // after walking in from win_cm to its left over win KH frames (0 = no walk-in, they just stand there))
    int px[3], fy, win, win_cm;
    int idle;                // (s94) 1 = stand idle once at the plate (s93); 0 = keep walking in place (KH2)
    int layer;               // (s94) 1 = between the menu and the name plates (needs the s94 exe's sora_hud_quads2); 0 = behind it all
    uint64_t t0;
    void (*screen)(bool, const float *, const float *, const float *);
} MW = {.mode = 1, .x_pct = 26, .y_pct = 56, .dist = 620, .face_deg = 0, .yaw_deg = 0,
        .px = {786, 645, 505}, .fy = 790, .win = 0, .win_cm = 90, .layer = 1};  // (px / fy in 1/10 %: Sora, Donald, Goofy)

static void mw_load(void) {
    MW.tried = true;
    HMODULE exe = GetModuleHandleA(NULL);
    MW.screen = (void (*)(bool, const float *, const float *, const float *))(void *)GetProcAddress(exe, "sora_draw_screen");
    MW.ok = MW.screen && PD.pose;  // (s111 roxmenu: the party check moved to mw_frame; Roxas has none)
    LOG("[sora] menuwalk: sora_draw_screen %s, sora_draw_actor_pose %s, Donald actor %d, Goofy actor %d -> %s\n",
        MW.screen ? "found" : "NOT in this exe", PD.pose ? "found" : "missing", g_party_id[0], g_party_id[1],
        MW.ok ? "ready" : "OFF (the camp keeps its black backdrop)");
}

// (s94 user: "infront of the menu but behind their name icons") the exe draws the camp quads before the plates under them
static bool mw_under(void) { return MW.layer && MW.mode && MW.ok && MW.on && CAMP.quads2 && CAMP.page.page == 0; }

static void mw_off(void) {
    g_cp_nobackdrop = 0, MW.age = 0, MW.logged = false;
    if (!MW.on) return;
    MW.on = false;
    if (MW.screen) MW.screen(false, NULL, NULL, NULL);
    LOG("[sora] menuwalk off\n");
}

// a motion clock (KH frames, 60 Hz) looping over 'frames' (from 'loop' on once the first pass is over)
static float mw_clock(double kh, float frames, float loop) {
    if (frames < 2) return 0;
    if (kh < frames) return (float)kh;
    float l = loop > 0 && loop < frames - 1 ? loop : 0;
    return l + (float)fmod(kh - l, (double)(frames - l));
}

// Whether the next cp_tick may drop the black backdrop (called before it): the actors were drawn for a few frames in a row.
static void mw_pre(void) {
    bool want = MW.mode && MW.ok && MW.on && MW.age >= 3 && (CAMP.page.page == 0 || MW.mode >= 2) && !mw_under();  // (s94: under = the backdrop stays, under them)
    g_cp_nobackdrop = want ? 1 : 0;
}

// Every frame the camp is up (after cp_tick). True = the actors are posed (the caller must not sora_draw_off).
static bool mw_frame(void) {
    if (!MW.mode) { mw_off(); return false; }
    if (!MW.tried) mw_load();
    const int rid = rm_actor();  // (s111 roxmenu) > 0 = Roxas alone (his own draw actor); 0 = Sora + Donald + Goofy
    const bool party_ok = g_party_id[0] > 0 && g_party_id[1] > 0;
    if (!MW.ok || (!rid && !party_ok) || !(CAMP.page.page == 0 || MW.mode >= 2) || CAMP.page.state == CP_CLOSED) { mw_off(); return false; }
    uint64_t now = GetTickCount64();
    if (!MW.on) MW.on = true, MW.t0 = now, MW.age = 0, LOG("[sora] menuwalk on (mode %d: %s)\n", MW.mode, MW.mode >= 2 ? "walk in place" : "idle");
    const double kh = (double)(now - MW.t0) * 0.06;  // KH frames since the menu opened
    const float s = 0.01f * g_k, D2R = 3.14159265f / 180;
    // the camera (the Game Over's recipe, GUESS numbers): on a sphere around tg, +z side; (s93) no shift any more, each
    // actor is placed on the plane through tg so its feet land on its plate (MW.px / MW.fy)
    const float fov = 30 * D2R, dist = (float)MW.dist, yaw = (float)MW.yaw_deg * D2R, el = 6 * D2R;
    const float tg[3] = {0, 85, 0};  // cm: the camera looks here; the actors stand on the plane through it
    float bk[3] = {sinf(yaw) * cosf(el), sinf(el), cosf(yaw) * cosf(el)};
    float rt[3] = {bk[2], 0, -bk[0]};
    float rl = sqrtf(rt[0] * rt[0] + rt[2] * rt[2]);
    rt[0] /= rl, rt[2] /= rl;
    float up[3] = {bk[1] * rt[2] - bk[2] * rt[1], bk[2] * rt[0] - bk[0] * rt[2], bk[0] * rt[1] - bk[1] * rt[0]};
    const float half_h = dist * tanf(fov / 2), half_w = half_h * (16.0f / 9.0f);  // (GUESS: the game image is 16:9)
    float cam[3];
    for (int j = 0; j < 3; j++) cam[j] = (tg[j] + bk[j] * dist) * s;
    float view[12] = {rt[0], rt[1], rt[2], -(rt[0] * cam[0] + rt[1] * cam[1] + rt[2] * cam[2]),
                      up[0], up[1], up[2], -(up[0] * cam[0] + up[1] * cam[1] + up[2] * cam[2]),
                      bk[0], bk[1], bk[2], -(bk[0] * cam[0] + bk[1] * cam[1] + bk[2] * cam[2])};
    float proj[4] = {0.05f, 100.0f, fov, 0}, clear[4] = {0, 0, 1, 1};  // (aspect 0: the game image's; black behind them)
    if (mw_under()) clear[2] = clear[3] = 0;  // (s94: no black rect, the menu's own backdrop + layout are already under them)
    MW.screen(true, view, proj, clear);
    // (s93) the walk-in: 0..1 over MW.win KH frames, eased out; they face the way they walk (+90 deg = screen right),
    // then turn to MW.face_deg over the last third
    const float t = MW.win > 0 ? (float)(kh / MW.win) : 1, tt = t > 1 ? 1 : t, ease = 1 - (1 - tt) * (1 - tt);
    const float turn = tt < 0.66f ? 0 : (tt - 0.66f) / 0.34f;
    // (s94 user: "in kh2 they are just walking": KH2's menu party walks in place the whole time; mwidle 1 = the s93 stop)
    const int walking = MW.mode >= 2 || tt < 1 || !MW.idle;
    const float face = ((float)MW.face_deg + (1 - turn) * (90 - (float)MW.face_deg)) * D2R;
    struct { int id; float x, y, ph, f; int slot; float frames, loop; } w[3];
    for (int k = 0; k < 3; k++) {  // k 0 Sora, 1 Donald, 2 Goofy: feet on screen at their plate
        float X = ((float)MW.px[k] / 1000 - 0.5f) * 2 * half_w, Y = (0.5f - (float)MW.fy / 1000) * 2 * half_h;
        X -= (1 - ease) * (float)MW.win_cm;
        w[k].x = X, w[k].y = Y, w[k].f = tt < 1 ? face : (float)MW.face_deg * D2R;
        if (k == 0) {  // Sora: his own mot table (base Sora, actor 0; a Drive Form is not shown: GUESS); Roxas: his actor + table (mot_of is his while IN_ROXAS)
            const Mot *m = mot_of(walking ? MOT_WALK : MOT_IDLE);
            if (rid && (!m || m->slot < 0)) m = mot_of(MOT_IDLE);  // (s111: no walk slot in his table = stand)
            w[0].id = rid, w[0].ph = 0;
            w[0].slot = m ? m->slot : -1, w[0].frames = m ? m->frames : 0, w[0].loop = m ? m->loop : 0;
        } else {
            int i = k - 1;
            const PMotX *pm = pmot_find(i, PARTY[i].slot[walking ? 1 : 0]);
            w[k].id = g_party_id[i];
            w[k].ph = i == 0 ? 37.0f : 71.0f;  // (idle phases apart: they do not breathe in step)
            w[k].slot = PARTY[i].slot[walking ? 1 : 0];
            w[k].frames = pm ? pm->frames : PARTY[i].frames[walking ? 1 : 0];
            w[k].loop = pm ? pm->loop : 0;
        }
    }
    if (rid) {
        rm_weapon(rid);
        if (!MW.logged) MW.logged = true, LOG("[roxmenu] menuwalk: Roxas alone (actor %d, slot %d, %s), Donald + Goofy not drawn\n", rid, w[0].slot, walking ? "walk" : "idle");
        if (PD.off) for (int k = 0; k < 2; k++) if (g_party_id[k] > 0) PD.off(g_party_id[k]);
    }
    for (int k = 0; k < (rid ? 1 : 3); k++) {
        if (w[k].slot < 0) continue;
        float feet[3];
        for (int j = 0; j < 3; j++) feet[j] = (tg[j] + rt[j] * w[k].x + up[j] * w[k].y) * s;
        PD.pose(w[k].id, w[k].slot, mw_clock(kh + w[k].ph, w[k].frames, w[k].loop), feet, w[k].f, s);
    }
    if (MW.age < 1000000) MW.age++;
    return true;
}

// live: menuwalk 0/1/2, mwx / mwy (% of the screen, where the group's centre sits), mwd (camera distance cm),
// mwface (deg they turn toward the menu), mwyaw (deg the camera goes around them)
static bool mw_live_set(const char *key, int v) {
    if (!strcmp(key, "menuwalk")) {
        MW.mode = v < 0 ? 0 : v > 2 ? 2 : v;
        if (!MW.mode) mw_off();
        LOG("[sora] menuwalk mode %d (0 off, 1 idle on the top page, 2 walk in place on every page)\n", MW.mode);
    }
    else if (!strcmp(key, "mwx")) MW.x_pct = v;
    else if (!strcmp(key, "mwy")) MW.y_pct = v;
    else if (!strcmp(key, "mwd")) MW.dist = v < 100 ? 100 : v > 3000 ? 3000 : v;
    else if (!strcmp(key, "mwface")) MW.face_deg = v;
    else if (!strcmp(key, "mwyaw")) MW.yaw_deg = v;
    else if (!strcmp(key, "mwsx")) MW.px[0] = v;  // (s93: feet x of Sora / Donald / Goofy, 1/10 % of the image width)
    else if (!strcmp(key, "mwdx")) MW.px[1] = v;
    else if (!strcmp(key, "mwgx")) MW.px[2] = v;
    else if (!strcmp(key, "mwfy")) MW.fy = v;      // (feet y, 1/10 % from the top)
    else if (!strcmp(key, "mwin")) MW.win = v < 0 ? 0 : v, MW.t0 = GetTickCount64();  // (walk-in KH frames, 0 = none; replays it)
    else if (!strcmp(key, "mwincm")) MW.win_cm = v;
    else if (!strcmp(key, "mwlayer")) MW.layer = v != 0;  // (s94: 1 over the menu / under the plates, 0 = behind the whole menu)
    else if (!strcmp(key, "mwidle")) MW.idle = v != 0;  // (s94: 0 walk in place forever, KH2; 1 stand once there)
    else return false;
    return true;
}

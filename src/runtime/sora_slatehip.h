// (s94 b8-slatehip) The Sheikah Slate on Sora's hip. Included by sora.c after sora_slate.h. See recon/S94_HELPER_SLATEHIP.md.
// Link is hidden (his gear with him, linkpos.c is_link_equip), so his Slate (Item_Conductor, right hip) was invisible while Sora
// stood there. Sora is drawn on Link's skeleton, so the Slate's own attach already lands at Sora's hip: this only tells the exe
// to stop hiding it (linkpos_slate_hip, looked up by name = NEEDS EXE RELINK; an older exe: logged once, nothing else changes).
// Live keys (ints):
//   slatehip 0/1/2   off / cutscenes + talks only (g_event, default) / always
//   shsnap 0/1/2     0 = the game's own hip attach (default); 1 = the exe writes the Slate's ACTOR matrix = Link's matrix * offset
//                    below (the model does not follow it: kept for A/B); 2 = (s100) the Slate's MODEL matrix is put on Sora's right
//                    hip bone (exe hook linkpos_slate_model, NEEDS EXE RELINK; recon/S100_HELPER_SLATEHIP.md). Switching the mode
//                    loads that mode's default offset (mode 1: -20 90 0; mode 2: -9 6 -2).
//   shbone n         mode 2: the Sora bone the Slate hangs on (default 39 = the right hip joint; 3 = hip hub, 0 = root)
//   shkeep 0/1       mode 2: 1 (default) = only the position is replaced, the rotation stays the game's own attach rotation;
//                    0 = also the rotation: Sora's facing * shyaw / shpitch / shroll
//   shx shy shz      snap offset in cm; mode 1: Link's local frame (+x is Link's LEFT [GUESS], y up, z forward); mode 2: Sora's
//                    facing frame at the bone (KH model axes: +x = Sora's LEFT, so the right hip is negative x; y up; z forward)
//   shyaw shpitch shroll   snap rotation in degrees; default 0 0 0
//   shlog 1          log the mode + reason every time it changes (always on for changes, this adds a line each 300 presents)
// While Slate mode (runes) is live the snap is off, so the held Slate follows Link's hand like the game does it.
static int g_slatehip = 1, g_sh_snap = 0, g_sh_log = 0;
static int g_sh_off[3] = {-20, 90, 0}, g_sh_rot[3] = {0, 0, 0};
static int g_sh_bone = 39, g_sh_keep = 1;
// (s111 slatecut, recon/S111_HELPER_SLATECUT.md) Roxas + the Slate in HAND during cutscenes. Roxas' skeleton order is not Sora's (hip joint 21
// not 39, right palm 172 not 178, left 63 not 86), and shsnap 2 forced the Slate onto the hip even while the game had it in Link's hand
// (the pedestal "Sheikah Slate confirmed" animation: empty hand). Now the exe tells the DLL where the GAME put the Slate this frame
// (linkpos_slate_model_game, NEEDS EXE RELINK; an older exe: the old hip-only behaviour with the right bones) and the DLL picks hip /
// right hand / left hand by distance to that body's own bones. Live keys:
//   shhand 0/1       1 (default) = hip-or-hand by the game's Slate position; 0 = always hip (the pre-s111 behaviour)
//   shhr n           radius in cm: the game's Slate farther than this from the hip bone = in hand; farther than this from the tracked
//                    palm = not in that hand any more (default 50; s112: 30 was too tight - Link's hip is 31-42 cm from Roxas' hip bone,
//                    the Slate popped into his left hand for a moment; a real hand hold measured 85 cm) [GUESS]
//   shrbone n        Roxas' hip bone (default 21; Sora uses shbone)
//   shrpalm n n      (set shrpalmr / shrpalml) Roxas' palm bones (defaults 172 / 63); Sora's are 178 / 86 (shspalmr / shspalml)
//   shhx shhy shhz   Roxas only: the Slate centre from his palm in cm in his facing frame (+x = his LEFT, y up, z forward); default 0 4 6 [GUESS];
//                    Sora's hand = the game's own attach (his skeleton IS Link's), no override
//   shroxhand 0/1    1 (default) = Roxas' Slate in hand is put on his palm; 0 = the game's own attach (Link's hand, a few cm off Roxas')
//   shwr n           (s112) radius in cm: the game's Slate farther than this from the tracked bone = in the WORLD (Link put it into a
//                    pedestal: s112 log, Oman Au: 96-106 cm from Roxas' hip AND palm, it flipped hip <-> hand every 20 presents and never
//                    showed). In the world the DLL leaves the Slate where the game has it (no override). Default 75 [GUESS]
static int g_sh_hand = 1, g_sh_hr = 50, g_sh_rbone = 21, g_sh_rpalm[2] = {172, 63}, g_sh_spalm[2] = {178, 86}, g_sh_hoff[3] = {0, 4, 6}, g_sh_roxhand = 1;
static int g_sh_wr = 75;
static int g_sh_state, g_sh_state_t;  // 0 hip, 1 right hand, 2 left hand, 3 in the world (s112); present of the last switch
// (s102 bombhand, recon/S102_HELPER_BOMBHAND.md) held Remote Bombs drawn at Sora's hand. Live keys:
//   bombhand 0/1     (s102 live: default 0 - with the RemoteBomb name fix the held bomb already shows ABOVE Sora's hands, VERIFIED) 1 = a held Obj_RemoteBomb* model is put at Sora's hand (exe hooks linkpos_bomb_hand / linkpos_bomb_state:
//                    NEEDS EXE RELINK, an older exe: logged once, nothing changes)
//   bhbone n         Sora bone used as the hand (default 178 = the right hand; 86 = the left)
//   bhx bhy bhz      offset from that bone in cm, Sora's facing frame (+x = Sora's LEFT, y up, z forward); default 0 20 0
//   bhrad n          hold zone radius around Link's feet in cm (default 100)
static int g_bombhand = 0, g_bh_bone = 178, g_bh_off[3] = {0, 20, 0}, g_bh_radcm = 100;  // (s102 review: bhy 20 = the bomb above the hand, not around the wrist; GUESS)
static void bh_tick(void) {
    static void (*hand)(int, const float *, float);
    static int (*state)(void);
    static bool (*bw)(int, float *, float *);
    static int tried, was = -1;
    if (!tried) {
        HMODULE exe = GetModuleHandleA(NULL);
        tried = 1, hand = (void (*)(int, const float *, float))(void *)GetProcAddress(exe, "linkpos_bomb_hand");
        state = (int (*)(void))(void *)GetProcAddress(exe, "linkpos_bomb_state");
        bw = (bool (*)(int, float *, float *))(void *)GetProcAddress(exe, "sora_draw_bone_world");
        LOG("[sora] bombhand: exe exports linkpos_bomb_hand / linkpos_bomb_state / sora_draw_bone_world: %s / %s / %s (NEEDS EXE RELINK if NO)\n",
            hand ? "yes" : "NO", state ? "yes" : "NO", bw ? "yes" : "NO");
    }
    if (!hand || !state || !bw) return;
    int st = g_bombhand ? state() : 0;  // only track the bone while a bomb is near (it shares the one tracked-bone slot with shsnap 2)
    float h[12], fs[2], p[3] = {0, 0, 0};
    bool ok = false;
    if (st && (g_sh_snap != 2 || g_sh_bone == g_bh_bone)) {
        ok = bw(g_bh_bone, h, fs);
        if (ok) {
            float s = sinf(fs[0]), c = cosf(fs[0]), o[3] = {g_bh_off[0] * 0.01f, g_bh_off[1] * 0.01f, g_bh_off[2] * 0.01f};
            p[0] = h[3] + c * o[0] + s * o[2], p[1] = h[7] + o[1], p[2] = h[11] - s * o[0] + c * o[2];
        }
    }
    hand(ok, p, g_bh_radcm * 0.01f);
    if (st != was) LOG("[sora] bombhand: bomb state %d (0 none, 1 near, 2 held), hand %s\n", st, ok ? "tracked" : "not tracked"), was = st;
}
// (s100) mode 2: the Slate's model matrix on the body's hip bone (s111: or palm). 1 = placed this present.
static int sh_model_place(bool show, uint64_t present) {
    static void (*mdl)(int, const float *, int);
    static bool (*bw)(int, float *, float *);
    static bool (*gm)(float *);
    static int tried;
    if (!tried) {
        HMODULE exe = GetModuleHandleA(NULL);
        tried = 1, mdl = (void (*)(int, const float *, int))(void *)GetProcAddress(exe, "linkpos_slate_model");
        bw = (bool (*)(int, float *, float *))(void *)GetProcAddress(exe, "sora_draw_bone_world");
        gm = (bool (*)(float *))(void *)GetProcAddress(exe, "linkpos_slate_model_game");
        LOG("[sora] slatehip: exe exports linkpos_slate_model / sora_draw_bone_world: %s / %s (shsnap 2 needs both: NEEDS EXE RELINK)\n", mdl ? "yes" : "NO", bw ? "yes" : "NO");
        LOG("[slatecut] linkpos_slate_model_game: %s (hip-or-hand for the Slate needs it: NEEDS EXE RELINK if NO; shhand 0 = old)\n", gm ? "yes" : "NO");
    }
    if (!mdl || !bw) return 0;
    const bool rx = rox_on_now() != 0;
    const int hipb = rx ? g_sh_rbone : g_sh_bone;
    const int pal[2] = {rx ? g_sh_rpalm[0] : g_sh_spalm[0], rx ? g_sh_rpalm[1] : g_sh_spalm[1]};
    if (!g_sh_hand || !gm) g_sh_state = 0;  // (s111 review: "set shhand 0" while in a hand left the hand branch on with the hip bone's matrix)
    float h[12], fs[2];
    int bone = hipb;
    if (g_sh_hand && gm && (g_sh_state == 1 || g_sh_state == 2)) bone = pal[g_sh_state - 1];  // (world state 3 tracks the hip, like 0)
    if (!show || !bw(bone, h, fs)) {  // (the first call only starts tracking the bone)
        if (!show) bw(-1, h, fs), g_sh_state = 0;
        mdl(0, NULL, 0);
        return 0;
    }
    if (g_sh_hand && gm) {  // (s111) where does the game have the Slate: hip or in a hand?
        float g[12];
        if (gm(g)) {
            float dx = g[3] - h[3], dy = g[7] - h[7], dz = g[11] - h[11], d = sqrtf(dx * dx + dy * dy + dz * dz), r = g_sh_hr * 0.01f;
            float wr = g_sh_wr * 0.01f;
            if (wr < r) wr = r;
            int ns = g_sh_state;
            if (present - (uint64_t)g_sh_state_t > 20) {
                if ((g_sh_state == 0 || g_sh_state == 3) && d > wr) ns = 3;  // (s112) far from the body: in the world (pedestal)
                else if ((g_sh_state == 0 || g_sh_state == 3) && d > r) {  // lateral side of the Slate from the (right) hip joint, along Sora's facing +x = LEFT
                    float lat = dx * cosf(fs[0]) - dz * sinf(fs[0]);
                    ns = lat > 0.12f ? 2 : 1;
                } else if (g_sh_state == 3) ns = 0;  // (s112) back on the hip
                else if (g_sh_state && d > wr) ns = 3;  // (s112) the hand let go of it far away = put into the world: no hip stop
                else if (g_sh_state && d > r) ns = 0;  // not in that hand: back to the hip (the hip state re-picks a hand after the dwell).
                // (s111 review: was "the other hand, then the hip" = at the end of every hand moment Roxas' Slate sat in his LEFT palm for
                // 20 presents while the game had it back on the hip; a real hand swap now goes via the hip instead)
            }
            if (ns != g_sh_state) {
                LOG("[slatecut] game Slate %.2f %.2f %.2f is %.0f cm from bone %d (%s): %s -> %s\n", g[3], g[7], g[11], d * 100, bone, rx ? "Roxas" : "Sora",
                    g_sh_state == 0 ? "hip" : g_sh_state == 1 ? "right hand" : g_sh_state == 2 ? "left hand" : "world (game's spot)",
                    ns == 0 ? "hip" : ns == 1 ? "right hand" : ns == 2 ? "left hand" : "world (game's spot)");
                g_sh_state = ns, g_sh_state_t = (int)present;
                mdl(0, NULL, 0);
                return 0;
            }
        }
    }
    if (g_sh_state == 3) { mdl(0, NULL, 0); return 0; }  // (s112) in the world: the game's own spot (the pedestal)
    if (g_sh_state) {  // in a hand
        if (!rx || !g_sh_roxhand) { mdl(0, NULL, 0); return 0; }  // Sora: Link's own hand attach is exactly his hand
        float s = sinf(fs[0]), c = cosf(fs[0]), k = fs[1], o[3] = {g_sh_hoff[0] * k, g_sh_hoff[1] * k, g_sh_hoff[2] * k};
        float w[12] = {c, 0, s, h[3] + c * o[0] + s * o[2], 0, 1, 0, h[7] + o[1], -s, 0, c, h[11] - s * o[0] + c * o[2]};
        mdl(1, w, 1);  // translation only: the game's hand rotation stays
        return 1;
    }
    float s = sinf(fs[0]), c = cosf(fs[0]), k = fs[1], o[3] = {g_sh_off[0] * k, g_sh_off[1] * k, g_sh_off[2] * k};
    // Sora's facing frame in the world: columns X = (c, 0, -s), Y = (0, 1, 0), Z = (s, 0, c)
    float w[12] = {c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0};
    if (!g_sh_keep) {  // facing * Ry(yaw) Rx(pitch) Rz(roll) (the order linkpos.c slate_hip_apply uses)
        float cy = cosf(g_sh_rot[0] * 0.0174533f), sy = sinf(g_sh_rot[0] * 0.0174533f), cp = cosf(g_sh_rot[1] * 0.0174533f),
              sp = sinf(g_sh_rot[1] * 0.0174533f), cr = cosf(g_sh_rot[2] * 0.0174533f), sr = sinf(g_sh_rot[2] * 0.0174533f);
        float ry[9] = {cy, 0, sy, 0, 1, 0, -sy, 0, cy}, rx[9] = {1, 0, 0, 0, cp, -sp, 0, sp, cp}, rz[9] = {cr, -sr, 0, sr, cr, 0, 0, 0, 1};
        float t1[9], t2[9], lr[9] = {c, 0, s, 0, 1, 0, -s, 0, c};
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) t1[i * 3 + j] = ry[i * 3] * rx[j] + ry[i * 3 + 1] * rx[3 + j] + ry[i * 3 + 2] * rx[6 + j];
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) t2[i * 3 + j] = t1[i * 3] * rz[j] + t1[i * 3 + 1] * rz[3 + j] + t1[i * 3 + 2] * rz[6 + j];
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) w[i * 4 + j] = lr[i * 3] * t2[j] + lr[i * 3 + 1] * t2[3 + j] + lr[i * 3 + 2] * t2[6 + j];
    }
    // translation: the bone + the offset in the facing frame (X, Y, Z = the columns of the matrix above before the rotation edit)
    w[3] = h[3] + c * o[0] + s * o[2], w[7] = h[7] + o[1], w[11] = h[11] - s * o[0] + c * o[2];
    mdl(1, w, g_sh_keep);
    return 1;
}
// (s107 slateocc / slatedump, recon/S107_HELPER_SLATE.md) The held Slate looked see-through (Sora's hand drawn over it). The exe can
// draw an invisible box at the Slate model (sora_draw_slate_occ, NEEDS EXE RELINK) that hides every Sora fragment behind it. Live keys:
//   slatocc 0/1       box on while the Slate is held in rune mode (default 1; an exe without the export: logged once, nothing happens)
//   soccx soccy soccz box half sizes in mm along the Slate model's local x / y / z (default 140 / 40 / 90; the model's bbox is
//                     +-132 / +-24 / +-79 mm, y = the thin axis, padded) - bigger numbers hide more
//   slatedump 1       once the Slate has been held for ~1.2 s: logs the Slate model matrix + Sora's two hand bones in screen px
//                     ([slatedump] lines) and asks the exe for one F9 frame dump (build/pass_NNN.png in the game folder, which must
//                     exist); one shot, set it again for another
static int g_socc = 1, g_socc_mm[3] = {140, 40, 90}, g_sdump;
static void so_tick(bool held) {
    static void (*occ)(int, int, int, int);
    static bool (*sw)(float *);
    static bool (*bw)(int, float *, float *);
    static bool (*proj)(const float *, float *, float *);
    static int tried;
    if (!tried) {
        HMODULE exe = GetModuleHandleA(NULL);
        tried = 1, occ = (void (*)(int, int, int, int))(void *)GetProcAddress(exe, "sora_draw_slate_occ");
        sw = (bool (*)(float *))(void *)GetProcAddress(exe, "linkpos_slate_model_world");
        bw = (bool (*)(int, float *, float *))(void *)GetProcAddress(exe, "sora_draw_bone_world");
        proj = (bool (*)(const float *, float *, float *))(void *)GetProcAddress(exe, "sora_draw_project");
        LOG("[sora] slatocc: exe exports sora_draw_slate_occ / linkpos_slate_model_world / sora_draw_project: %s / %s / %s (NEEDS EXE RELINK if NO)\n",
            occ ? "yes" : "NO", sw ? "yes" : "NO", proj ? "yes" : "NO");
    }
    if (occ) occ(g_socc && held, g_socc_mm[0], g_socc_mm[1], g_socc_mm[2]);
    static ULONGLONG t0;
    static int ph;
    ULONGLONG now = GetTickCount64();
    if (!held || !g_sdump) { t0 = 0, ph = 0; return; }
    if (!t0) t0 = now;
    ULONGLONG dt = now - t0;
    static float hand[2][12];
    static bool hok[2];
    if (ph == 0 && dt > 600) ph = 1, bw ? (void)bw(178, hand[0], NULL) : (void)0;
    else if (ph == 1 && dt > 900) {
        hok[0] = bw && bw(178, hand[0], NULL), ph = 2;
        if (bw) bw(86, hand[1], NULL);
    } else if (ph == 2 && dt > 1200) {
        hok[1] = bw && bw(86, hand[1], NULL), ph = 3, g_sdump = 0;
        float m[12];
        bool ok = sw && sw(m);
        LOG("[slatedump] Slate model world: %s\n", ok ? "seen" : "NOT seen (exe export missing / Slate not drawn)");
        if (ok) {
            LOG("[slatedump]   axes (columns, incl. scale) x %.3f %.3f %.3f  y %.3f %.3f %.3f  z %.3f %.3f %.3f  centre %.3f %.3f %.3f\n",
                m[0], m[4], m[8], m[1], m[5], m[9], m[2], m[6], m[10], m[3], m[7], m[11]);
            for (int c = 0; c < 9; c++) {  // centre + the 8 box corners at the occluder size, in window px
                float sx = (c & 1) ? 1.f : -1.f, sy = (c & 2) ? 1.f : -1.f, sz = (c & 4) ? 1.f : -1.f;
                if (c == 8) sx = sy = sz = 0;
                float hx = sx * g_socc_mm[0] * 0.001f, hy = sy * g_socc_mm[1] * 0.001f, hz = sz * g_socc_mm[2] * 0.001f;
                float w[3] = {m[3] + m[0] * hx + m[1] * hy + m[2] * hz, m[7] + m[4] * hx + m[5] * hy + m[6] * hz, m[11] + m[8] * hx + m[9] * hy + m[10] * hz};
                float px, py;
                if (proj && proj(w, &px, &py)) LOG("[slatedump]   %s px %.0f %.0f\n", c == 8 ? "centre" : "corner", px, py);
                else if (c == 8) LOG("[slatedump]   centre not projectable (no draw yet / behind the camera)\n");
            }
        }
        for (int i = 0; i < 2; i++) {
            float px = 0, py = 0, hw[3] = {hand[i][3], hand[i][7], hand[i][11]};
            bool pr = proj && hok[i] && proj(hw, &px, &py);
            if (!hok[i]) LOG("[slatedump] hand bone %d not tracked\n", i ? 86 : 178);
            else LOG("[slatedump] hand bone %d at world %.3f %.3f %.3f -> px %s%.0f %.0f\n", i ? 86 : 178, hw[0], hw[1], hw[2], pr ? "" : "(no projection) ", px, py);
        }
        LOG("[slatedump]   boxes: slatocc %d, half sizes mm %d %d %d\n", g_socc, g_socc_mm[0], g_socc_mm[1], g_socc_mm[2]);
        volatile int *req = (volatile int *)(void *)GetProcAddress(GetModuleHandleA(NULL), "g_debug_frame_request");
        _putenv_s("SAVAGEWIND_DEBUG_MAIN_ONLY", "1");
        if (req) *req = 1;
        LOG("[slatedump] F9 frame dump: %s\n", req ? "requested" : "NO g_debug_frame_request export");
    }
}
static void sh_tick(uint64_t present) {
    static void (*fn)(int, int, const float *);
    static int tried, was = -1;
    if (!tried) {
        tried = 1, fn = (void (*)(int, int, const float *))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_slate_hip");
        LOG("[sora] slatehip: linkpos_slate_hip %s\n", fn ? "found" : "NOT in this exe (NEEDS EXE RELINK: the Slate stays hidden)");
    }
    if (!fn) return;
    bool cs = g_event;
    bool held = SLATE_LIVE(present);
    int show = g_slatehip == 2 || (g_slatehip == 1 && (cs || held));
    // (s113b) only while the Slate is IN USE (rune mode, or the game has it in a hand / a pedestal: slatecut state 1-3): bit 0 on also brings
    // back Link's other gear (user: an invisible shield on his back hid Roxas in some cutscenes); on the hip in a cutscene = not drawn
    int vis = show && (held || g_sh_state != 0);
    if (vis != g_sh_vis_now) LOG("[slvis] Slate %s: Link's view bit 0 %s (slvis %d, state %d)\n", vis ? "in use" : "not in use", vis && g_slvis ? "kept on" : "cleared", g_slvis, g_sh_state);
    g_sh_vis_now = vis;  // (s113 slvis: hide_link keeps Link's view bit 0 on while shown, else BotW copies it off onto the Slate)
    float p[6] = {g_sh_off[0] / 100.0f, g_sh_off[1] / 100.0f, g_sh_off[2] / 100.0f, (float)g_sh_rot[0], (float)g_sh_rot[1], (float)g_sh_rot[2]};
    fn(show, show && g_sh_snap == 1 && !held, p);
    so_tick(held);  // (s107)
    int placed = sh_model_place(show && g_sh_snap == 2 && !held, present);  // (s100)
    static int was_placed = -1;
    if (placed != was_placed && g_sh_snap == 2) LOG("[sora] slatehip: model on Sora's bone %d: %s\n", g_sh_bone, placed ? "placing" : "not placed (bone not tracked yet / exe export missing / hidden)"), was_placed = placed;
    if (show != was || (g_sh_log && present % 300 == 0)) {
        LOG("[sora] slatehip: mode %d, slate %s (cutscene/talk %d, rune mode %d, snap %d)\n", g_slatehip, show ? "shown" : "hidden", cs, held, g_sh_snap);
        was = show;
    }
}
static bool sh_live_set(const char *key, int v) {
    if (!strcmp(key, "slatehip")) g_slatehip = v < 0 ? 0 : v > 2 ? 2 : v, LOG("[sora] slatehip: mode %d (0 off, 1 cutscenes, 2 always)\n", g_slatehip);
    else if (!strcmp(key, "shsnap")) {
        int m = v < 0 ? 0 : v > 2 ? 2 : v;
        if (m != g_sh_snap && m == 2) g_sh_off[0] = 26, g_sh_off[1] = -4, g_sh_off[2] = -8;  // (s113: the LEFT hip, outside the shorts, 10 cm lower - the user, live on Roxas; s100's -9 6 -2 sat inside him)
        else if (m != g_sh_snap && g_sh_snap == 2) g_sh_off[0] = -20, g_sh_off[1] = 90, g_sh_off[2] = 0;
        g_sh_snap = m, LOG("[sora] slatehip: snap %d (offset %d %d %d)\n", g_sh_snap, g_sh_off[0], g_sh_off[1], g_sh_off[2]);
    }
    else if (!strcmp(key, "shbone")) g_sh_bone = v < 0 ? 0 : v, LOG("[sora] slatehip: bone %d\n", g_sh_bone);
    else if (!strcmp(key, "shkeep")) g_sh_keep = v != 0, LOG("[sora] slatehip: keep game rotation %d\n", g_sh_keep);
    else if (!strcmp(key, "shx")) g_sh_off[0] = v;
    else if (!strcmp(key, "shy")) g_sh_off[1] = v;
    else if (!strcmp(key, "shz")) g_sh_off[2] = v;
    else if (!strcmp(key, "shyaw")) g_sh_rot[0] = v;
    else if (!strcmp(key, "shpitch")) g_sh_rot[1] = v;
    else if (!strcmp(key, "shroll")) g_sh_rot[2] = v;
    else if (!strcmp(key, "shhand")) g_sh_hand = v != 0, LOG("[slatecut] shhand %d\n", g_sh_hand);
    else if (!strcmp(key, "shhr")) g_sh_hr = v < 5 ? 5 : v;
    else if (!strcmp(key, "shwr")) g_sh_wr = v < 5 ? 5 : v, LOG("[slatecut] shwr %d cm\n", g_sh_wr);
    else if (!strcmp(key, "shrbone")) g_sh_rbone = v < 0 ? 0 : v;
    else if (!strcmp(key, "shrpalmr")) g_sh_rpalm[0] = v < 0 ? 0 : v;
    else if (!strcmp(key, "shrpalml")) g_sh_rpalm[1] = v < 0 ? 0 : v;
    else if (!strcmp(key, "shspalmr")) g_sh_spalm[0] = v < 0 ? 0 : v;
    else if (!strcmp(key, "shspalml")) g_sh_spalm[1] = v < 0 ? 0 : v;
    else if (!strcmp(key, "shhx")) g_sh_hoff[0] = v;
    else if (!strcmp(key, "shhy")) g_sh_hoff[1] = v;
    else if (!strcmp(key, "shhz")) g_sh_hoff[2] = v;
    else if (!strcmp(key, "shroxhand")) g_sh_roxhand = v != 0, LOG("[slatecut] shroxhand %d\n", g_sh_roxhand);
    else if (!strcmp(key, "shlog")) g_sh_log = v != 0;
    else if (!strcmp(key, "slatocc")) g_socc = v != 0, LOG("[sora] slatocc %d\n", g_socc);
    else if (!strcmp(key, "soccx")) g_socc_mm[0] = v < 1 ? 1 : v;
    else if (!strcmp(key, "soccy")) g_socc_mm[1] = v < 1 ? 1 : v;
    else if (!strcmp(key, "soccz")) g_socc_mm[2] = v < 1 ? 1 : v;
    else if (!strcmp(key, "slatedump")) g_sdump = v != 0, LOG("[sora] slatedump %d (hold the Slate in rune mode for 1.2 s)\n", g_sdump);
    else if (!strcmp(key, "bombhand")) g_bombhand = v != 0, LOG("[sora] bombhand: %d\n", g_bombhand);
    else if (!strcmp(key, "bhbone")) g_bh_bone = v < 0 ? 0 : v, LOG("[sora] bombhand: bone %d\n", g_bh_bone);
    else if (!strcmp(key, "bhx")) g_bh_off[0] = v;
    else if (!strcmp(key, "bhy")) g_bh_off[1] = v;
    else if (!strcmp(key, "bhz")) g_bh_off[2] = v;
    else if (!strcmp(key, "bhrad")) g_bh_radcm = v < 20 ? 20 : v;
    else return false;
    return true;
}

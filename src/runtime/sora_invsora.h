// sora_invsora.h: (s102 b16-invsora) glitch #62: Sora instead of Link on BotW's inventory (pause menu) screen.
// recon/S102_HELPER_INVSORA.md. DLL only (hot reload). Included by sora.c after sora_camp.h (needs the dg_* helpers of the
// invdiag block, screen_on, mot_of, mw_clock, PD, g_k, SCREEN_MGR_PP).
// Live: "set invsora 0|1|2" (0 old = Link shown, 1 hide the menu Link + draw Sora, 2 hide only; default 0: untested live),
// placement: isx / isy (feet, 1/10 % of the game image from the left / top), iss (Sora's height, % of the image height),
// isface (deg), isyaw (1 = follow the inventory's own Link-rotate yaw, -1 reversed, 0 = fixed), ismask (vis mask written to the
// menu Link's units, default 0), isopa (1 = also the preview actor's opacity), isreq (1 = draw only on the plain item pages).
#pragma once

static int g_is_x = 790, g_is_y = 860, g_is_h = 58, g_is_face = 15, g_is_yaw = 1, g_is_opa = 1, g_is_req = 1;
// (s105 islayer) 1 = Sora is not drawn inside the description panel's rect (isl0..isl3 = x0 y0 x1 y1, 1/1000 of the game image),
// so he looks to stand behind it; 0 = old (over everything). Needs the exe export sora_draw_screen_mask (else logged once, = 0).
// (s105 menupuppet) invsora 3: Sora is drawn like the WORLD puppet (BotW's camera, scene depth, the ui-boundary pass under the 2D UI) at the
// preview actor's own matrix ([actor+0x398], feet = translation, facing = its yaw). iwx/iwy/iwz = offset in KH cm (BotW axes), iwf = facing
// offset deg, iwsc = scale % (100). DLL only. recon/S105_HELPER_MENUPUPPET.md
static int g_iw_off[3], g_iw_face, g_iw_sc = 100;
static int g_is_layer = 1, g_is_l[4] = {528, 666, 810, 894};
// (s106 invdump) live "set invdump 1": the next inventory frame after 1.5 s open asks the exe for one F9 frame dump
// (g_debug_frame_request: build/pass_NNN.png in the game folder + the draw log), to find the menu Link's pass / camera.
static int g_is_dump;
// (s107 invsora 3) ismenucam (1 = the menu scene's camera, 0 = BotW's world camera) + the exe's camera override exports
static int g_is_menucam = 1;
static void (*g_is_camfn)(bool, const float *, const float *);
static void (*g_is_shiftfn)(float, float);

static void (*g_is_maskfn)(float, float, float, float);
static bool g_is_maskfn_tried;
static uint32_t g_is_mask;                              // vis mask written to the menu Link's units (0 = hidden from every view)
static volatile uint64_t g_is_scan_unit;                // fallback: a unit found by the memory scan (thread)
static struct {
    bool open, hidden, drew, logged, scr_on, tried, cam_on, cam_warned;
    uint64_t actor;                                     // the preview actor ([[0x7001e15fb8]+0x48]), 0 = none
    uint64_t u[64];                                     // the units being hidden (verified Link-skeleton units)
    uint16_t vis0[64];                                  // their original visibility masks
    int nu;
    float opa0[2];
    uint8_t dirty0;
    bool opa_saved;
    uint64_t t0, next_resolve, last_scan;
    int scans;
    float yaw0;
    bool yaw0_ok;
    void (*screen)(bool, const float *, const float *, const float *);
} IS;

#define IS_PREVIEW_OWNER 0x7001e15fb8ull  // 1.9.0 (pc_cursor.c inventory_rotate: [[owner]+0x48] = the preview actor)
#define IS_PREVIEW_YAW 0x7001e15fa8ull    // 1.9.0 (two floats, deg)
// (s106 invcam) live "set invcam 1": once, 1.5 s into an open inventory, a background thread logs the preview actor's whole
// matrix and scans the preview owner (+ every object it points to, 1 level) and the pause globals around IS_PREVIEW_OWNER
// for a sead::LookAtCamera (view Matrix34f, then pos / at / up: up unit, 0.2 < |pos - at| < 40, view's back row = pos - at).
// The F9 dump (s106) showed the menu Link is a separate deferred scene composited into the final image right before the
// UI; invsora 3 needs that scene's camera. Lines "[invcam] ...".
static int g_is_cam;
static volatile LONG g_is_cam_thr;
static int is_cam_check(const float *f, uint64_t at_addr, char *out, size_t cap) {  // f = 12 view floats then pos/at/up
    const float *pos = f + 12, *at = f + 15, *up = f + 18;
    for (int i = 0; i < 21; i++) if (!(f[i] > -1e5f && f[i] < 1e5f)) return 0;
    int odd = 0;  // (s106b: the first scan was eaten by 0 / 0.5 / 1 lookup tables in the globals)
    for (int i = 12; i < 21; i++) { float a = fabsf(f[i]); odd += a != 0 && a != 0.5f && a != 1; }
    if (odd < 3) return 0;
    float al = sqrtf(f[15] * f[15] + f[16] * f[16] + f[17] * f[17]);  // the menu Link stands at his scene's origin (s106 log)
    if (al > 5) return 0;
    float ul = sqrtf(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
    float d[3] = {pos[0] - at[0], pos[1] - at[1], pos[2] - at[2]}, dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (!(ul > 0.98f && ul < 1.02f && dl > 0.2f && dl < 40)) return 0;
    int score = 1;
    float r0 = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]), r2 = sqrtf(f[8] * f[8] + f[9] * f[9] + f[10] * f[10]);
    if (r0 > 0.98f && r0 < 1.02f && r2 > 0.98f && r2 < 1.02f) {
        float dot = (f[8] * d[0] + f[9] * d[1] + f[10] * d[2]) / dl;
        if (dot > 0.99f || dot < -0.99f) score = 2;
    }
    snprintf(out, cap, "score %d at %llx: pos %.3f %.3f %.3f at %.3f %.3f %.3f up %.2f %.2f %.2f dist %.2f | view %.3f %.3f %.3f %.3f / %.3f %.3f %.3f %.3f / %.3f %.3f %.3f %.3f",
             score, (unsigned long long)at_addr, pos[0], pos[1], pos[2], at[0], at[1], at[2], up[0], up[1], up[2], dl, f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7],
             f[8], f[9], f[10], f[11]);
    return score;
}
static int is_cam_scan(uint64_t lo, size_t n, const char *what, int *budget) {
    static float buf[0x2000 / 4 + 32];
    if (n > 0x2000) n = 0x2000;
    if (!dg_rd(lo, buf, n)) return 0;
    int hits = 0;
    char line[512];
    for (size_t i = 0; i + 21 <= n / 4; i++) {
        int s = is_cam_check(buf + i, lo + (i + 12) * 4, line, sizeof line);
        if (s && *budget > 0) { (*budget)--, hits++; LOG("[invcam] %s +%#zx %s\n", what, (i + 12) * 4, line); }
    }
    return hits;
}
static DWORD WINAPI is_cam_thread(LPVOID arg) {
    uint64_t actor = (uint64_t)(uintptr_t)arg, owner = dg_q(IS_PREVIEW_OWNER);
    float m[12];
    if (actor && dg_rd(actor + 0x398, m, 48))
        LOG("[invcam] preview actor %llx mMtx %.3f %.3f %.3f %.3f / %.3f %.3f %.3f %.3f / %.3f %.3f %.3f %.3f, model %llx\n", (unsigned long long)actor,
            m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], (unsigned long long)dg_q(actor + 0x4e0));
    uint64_t mdl = actor ? dg_q(actor + 0x4e0) : 0;
    if (mdl && dg_rd(mdl + 0x58, m, 48))
        LOG("[invcam] preview model world %.3f %.3f %.3f %.3f / %.3f %.3f %.3f %.3f / %.3f %.3f %.3f %.3f\n", m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
            m[8], m[9], m[10], m[11]);
    int budget = 40, hits = 0;
    hits += is_cam_scan(IS_PREVIEW_OWNER - 0x1000, 0x2000, "globals", &budget);
    if (owner) {
        hits += is_cam_scan(owner, 0x2000, "owner", &budget);
        uint64_t p[0x2000 / 8];
        if (dg_rd(owner, p, sizeof p)) {
            int followed = 0;
            for (int i = 0; i < 0x2000 / 8 && followed < 200 && budget > 0; i++) {
                if (p[i] < 0x7000000000ull || p[i] > 0x7fffffffffull || p[i] == owner) continue;
                char what[64];
                snprintf(what, sizeof what, "owner+%#x -> %llx", i * 8, (unsigned long long)p[i]);
                followed++, hits += is_cam_scan(p[i], 0x800, what, &budget);
            }
        }
    }
    LOG("[invcam] scan done: owner %llx, %d candidate(s)%s\n", (unsigned long long)owner, hits, budget <= 0 ? " (log budget used up)" : "");
    InterlockedExchange(&g_is_cam_thr, 0);
    return 0;
}
#define IS_SCREEN_INV 0x2e                // screens[0x2e] = the inventory (pc_cursor.c); vtable below
#define IS_INV_VT 0x7001cf9ad8ull

// Link's own unit data: vtable, skeleton resource, his unit
static bool is_link_ref(uint64_t *vt, uint64_t *res, uint64_t *lunit) {
    uint64_t pi = dg_q(PLAYER_INFO_PTR), actor = pi ? dg_q(pi + 0x60) : 0, r = 0, sk = 0, b = 0, u = 0;
    if (!actor || !dg_link_unit(actor, &u, &r, &sk, &b)) return false;
    *vt = dg_q(u), *res = r, *lunit = u;
    return *vt != 0;
}
// a unit that is a Link-skeleton ModelUnit of the same class as his, but not his own
static bool is_unit_ok(uint64_t u, uint64_t vt, uint64_t res, uint64_t lunit) {
    return u && u != lunit && dg_q(u) == vt && res && dg_q(dg_q(u + 0x118)) == res;
}
// the units of an actor's model (the link_units chain: [[[model+0x40]+8i]]), safe reads
static int is_actor_units(uint64_t actor, uint64_t *out, int max) {
    uint64_t model = dg_q(actor + 0x4e0), arr = dg_q(model + 0x40);
    uint32_t n = 0;
    int k = 0;
    if (!model || !arr || !dg_rd(model + 0x38, &n, 4) || n == 0 || n > 64) return 0;
    for (uint32_t i = 0; i < n && k < max; i++) {
        uint64_t q = dg_q(dg_q(arr + 8ull * i));
        if (q) out[k++] = q;
    }
    return k;
}

static DWORD WINAPI is_scan_thread(LPVOID arg) {  // fallback: the invdiag scan, result cached in g_is_scan_unit
    (void)arg;
    uint64_t vt, res, lunit, sk[64], un[64], cand[64], pi = dg_q(PLAYER_INFO_PTR), actor = pi ? dg_q(pi + 0x60) : 0, r2, skel, b;
    if (actor && is_link_ref(&vt, &res, &lunit) && dg_link_unit(actor, &lunit, &r2, &skel, &b)) {
        int nsk = dg_scan(&res, 1, sk, 64), nc = 0, nu = 0;
        for (int i = 0; i < nsk; i++)
            if (sk[i] != skel) cand[nc++] = sk[i];
        if (nc) nu = dg_scan(cand, nc, un, 64);
        for (int i = 0; i < nu; i++)
            if (is_unit_ok(un[i] - 0x118, vt, res, lunit)) {
                g_is_scan_unit = un[i] - 0x118;
                LOG("[invsora] scan found the menu Link's unit %llx\n", (unsigned long long)g_is_scan_unit);
                break;
            }
        if (!g_is_scan_unit) LOG("[invsora] scan: %d holders of Link's skeleton, %d besides his own, no menu unit\n", nsk, nc);
    }
    InterlockedExchange(&g_is_thr, 0);
    return 0;
}

// restore what we changed (the inventory closed, the key went to 0, an unload)
static void is_restore(void) {
    if (IS.hidden) {
        uint64_t vt, res, lunit;
        bool ref = is_link_ref(&vt, &res, &lunit);
        for (int i = 0; i < IS.nu; i++)
            if (ref && is_unit_ok(IS.u[i], vt, res, lunit)) *(volatile uint16_t *)(uintptr_t)(IS.u[i] + 0xc) = IS.vis0[i];
        if (IS.opa_saved && IS.actor && ref && dg_q(IS.actor + 0x4e0)) {
            *(volatile float *)(uintptr_t)(IS.actor + 0x4f0) = IS.opa0[0];
            *(volatile float *)(uintptr_t)(IS.actor + 0x4ec) = IS.opa0[1];
            *(volatile uint8_t *)(uintptr_t)(IS.actor + 0x68e) = 1;
        }
        LOG("[invsora] inventory closed: menu Link restored (%d units)\n", IS.nu);
    }
    IS.hidden = IS.opa_saved = false, IS.nu = 0, IS.actor = 0, IS.logged = false, IS.yaw0_ok = false, IS.scans = 0;
}

static void is_off(void) {
    if (IS.cam_on) { IS.cam_on = false; if (g_is_camfn) g_is_camfn(false, NULL, NULL); }  // (s107: before the world poses Sora again)
    IS.cam_warned = false;
    if (IS.scr_on) {
        IS.scr_on = false;
        if (IS.screen) IS.screen(false, NULL, NULL, NULL);
        LOG("[invsora] Sora off\n");
    }
    if (IS.open || IS.hidden) {
        IS.open = false;
        is_restore();
    }
    IS.drew = false;
}

// (s107 invsora 3) the menu scene's camera: the preview owner's sead::LookAtCamera (+0x218; view Matrix34f at +0x220, pos +0x250,
// at +0x25c, up +0x268) and its PerspectiveProjection (near / far / fovy at +0x284, the 4x4 at +0x354: m00 +0x354, m02 +0x35c,
// m11 +0x368, m12 +0x36c). Found live in s106 (invcam): pos (-0.2, 1.2, 4.75) at (0, 0.95, 0), fovy 30, aspect 16:9, off-centre
// m02 -0.59 (Link right of centre). 1.9.0 offsets; checked for sanity every frame. ismenucam 0 = BotW's world camera (s105 mode 3).
static bool is_camfns(void) {
    static bool tried;
    if (!tried) {
        tried = true;
        HMODULE exe = GetModuleHandleA(NULL);
        g_is_camfn = (void (*)(bool, const float *, const float *))(void *)GetProcAddress(exe, "sora_draw_camera");
        g_is_shiftfn = (void (*)(float, float))(void *)GetProcAddress(exe, "sora_draw_camera_shift");
        LOG("[invsora] sora_draw_camera %s, sora_draw_camera_shift %s\n", g_is_camfn ? "found" : "NOT in this exe", g_is_shiftfn ? "found" : "NOT in this exe (s107 relink)");
    }
    return g_is_camfn && g_is_shiftfn;
}
static bool is_menu_cam(float view[12], float proj[4], float shift[2]) {
    uint64_t owner = dg_q(IS_PREVIEW_OWNER);
    float nf[3], m[8];
    if (!owner || !dg_rd(owner + 0x220, view, 48) || !dg_rd(owner + 0x284, nf, 12) || !dg_rd(owner + 0x354, m, 32)) return false;
    for (int i = 0; i < 12; i++) if (!isfinite(view[i])) return false;
    for (int r = 0; r < 3; r++) {  // rotation rows unit length
        float l = view[r * 4] * view[r * 4] + view[r * 4 + 1] * view[r * 4 + 1] + view[r * 4 + 2] * view[r * 4 + 2];
        if (!(l > 0.96f && l < 1.04f)) return false;
    }
    float m00 = m[0], m02 = m[2], m11 = m[5], m12 = m[6];
    if (!(nf[0] > 0.001f && nf[1] > nf[0] && nf[1] < 1e6f && m00 > 0.1f && m11 > 0.1f && m11 < 100 && fabsf(m02) < 3 && fabsf(m12) < 3)) return false;
    proj[0] = nf[0], proj[1] = nf[1], proj[2] = 2 * atanf(1 / m11), proj[3] = m11 / m00;
    if (fabsf(proj[2] - nf[2]) > 0.02f) return false;  // (the matrix and the fovy field agree, or the offsets moved)
    shift[0] = -m02, shift[1] = -m12;
    return true;
}

// Every PAUSED frame without the KH2 camp. True = Sora is posed for the inventory screen (the caller must not sora_draw_off).
static bool is_frame(void) {
    if (!g_invsora) { is_off(); return false; }
    // the inventory open? (1.9.0 only: the addresses are literals)
    uint64_t pp = rd_ok(SCREEN_MGR_PP, 8) ? GV(uint64_t, SCREEN_MGR_PP, 0) : 0;
    uint64_t mgr = pp && rd_ok(pp, 8) ? GV(uint64_t, pp, 0) : 0;
    bool open = false;
    uint32_t sub = 0;
    if (mgr && rd_ok(mgr, 0x38) && sora_sites()->pause_state == 0x7001e1611cull && GV(uint32_t, PAUSE_STATE, 0) == 2 &&
        screen_on(mgr, IS_SCREEN_INV)) {
        uint64_t s = GV(uint64_t, GV(uint64_t, mgr, 0x30), 8 * IS_SCREEN_INV);
        open = rd_ok(s, 0x3a50) && GV(uint64_t, s, 0) == IS_INV_VT && GV(uint8_t, s, 0xfe) == 2;
        if (open) sub = GV(uint32_t, s, 0x3a48);
    }
    if (!open && GV(uint32_t, PAUSE_STATE, 0) == 2) {  // (s102 live check: why the inventory isn't seen; every 2 s while paused)
        static ULONGLONG lastw;
        ULONGLONG nw = GetTickCount64();
        static int nlog;
        if (nw - lastw > 2000 && nlog < 6) {
            nlog++;
            lastw = nw;
            uint64_t arr = mgr && rd_ok(mgr, 0x38) ? GV(uint64_t, mgr, 0x30) : 0, s = arr && rd_ok(arr + 8 * IS_SCREEN_INV, 8) ? GV(uint64_t, arr, 8 * IS_SCREEN_INV) : 0;
            LOG("[invsora] not open: mgr %llx n %u arr %llx s %llx vt %llx fe %u\n", (unsigned long long)mgr, mgr && rd_ok(mgr, 0x38) ? GV(uint32_t, mgr, 0x28) : 0,
                (unsigned long long)arr, (unsigned long long)s, s && rd_ok(s, 8) ? (unsigned long long)GV(uint64_t, s, 0) : 0ull, s && rd_ok(s, 0x100) ? GV(uint8_t, s, 0xfe) : 0);
        }
    }
    if (!open) { is_off(); return false; }
    uint64_t now = GetTickCount64();
    if (!IS.open) {
        IS.open = true, IS.t0 = now, IS.next_resolve = 0, IS.scans = 0, IS.last_scan = 0;
        LOG("[invsora] inventory open (mode word %u), invsora %d\n", sub, g_invsora);
    }
    if (g_is_cam && now - IS.t0 > 1500 && !InterlockedExchange(&g_is_cam_thr, 1)) {
        g_is_cam = 0;
        uint64_t owner = dg_q(IS_PREVIEW_OWNER), actor = owner ? dg_q(owner + 0x48) : 0;
        HANDLE h = CreateThread(NULL, 0, is_cam_thread, (LPVOID)(uintptr_t)actor, 0, NULL);
        if (h) CloseHandle(h);
        else InterlockedExchange(&g_is_cam_thr, 0);
        LOG("[invcam] scan started (preview actor %llx)\n", (unsigned long long)actor);
    }
    if (g_is_dump && now - IS.t0 > 1500) {
        g_is_dump = 0;
        volatile int *req = (volatile int *)(void *)GetProcAddress(GetModuleHandleA(NULL), "g_debug_frame_request");
        _putenv_s("SAVAGEWIND_DEBUG_MAIN_ONLY", "1");  // (read by the exe at its first dump: one png per pass, not every target)
        if (req) *req = 1;
        LOG("[invsora] invdump: %s (inventory open %llu ms, invsora %d, preview actor %llx)\n", req ? "F9 frame dump requested" : "NO g_debug_frame_request export",
            (unsigned long long)(now - IS.t0), g_invsora, (unsigned long long)IS.actor);
    }
    uint64_t vt, res, lunit;
    if (!is_link_ref(&vt, &res, &lunit)) return false;
    // find the menu Link's units (every 250 ms until found, then every 1 s as a refresh)
    if (now >= IS.next_resolve) {
        IS.next_resolve = now + (IS.nu ? 1000 : 250);
        uint64_t owner = dg_q(IS_PREVIEW_OWNER), actor = owner ? dg_q(owner + 0x48) : 0, cu[64];
        int nc = 0;
        if (actor && actor != dg_q(dg_q(PLAYER_INFO_PTR) + 0x60)) {
            uint64_t all[64];
            int na = is_actor_units(actor, all, 64);
            for (int i = 0; i < na; i++)
                if (is_unit_ok(all[i], vt, res, lunit)) cu[nc++] = all[i];
            if (nc) IS.actor = actor;
        }
        uint64_t su = g_is_scan_unit;
        if (su && is_unit_ok(su, vt, res, lunit)) {
            bool have = false;
            for (int i = 0; i < nc; i++) have |= cu[i] == su;
            if (!have && nc < 64) cu[nc++] = su;
        }
        if (nc) {
            for (int i = 0; i < nc; i++) {  // keep the saved masks of units we already know
                int j = 0;
                while (j < IS.nu && IS.u[j] != cu[i]) j++;
                uint16_t v0 = j < IS.nu ? IS.vis0[j] : (uint16_t)dg_q(cu[i] + 0xc);
                if (j >= IS.nu) { IS.u[IS.nu] = cu[i], IS.vis0[IS.nu] = v0; IS.nu++; }
            }
            if (!IS.logged) {
                IS.logged = true;
                LOG("[invsora] menu Link: preview actor %llx, %d unit(s) hidden (first %llx, vis %#x) via %s%s\n",
                    (unsigned long long)IS.actor, IS.nu, (unsigned long long)IS.u[0], (unsigned)IS.vis0[0],
                    IS.actor ? "the preview actor" : "the memory scan", g_is_scan_unit ? " + scan" : "");
            }
        } else if (!IS.nu && !g_is_thr && now - IS.last_scan > 3000 && IS.scans < 3 && !g_is_scan_unit) {
            // the preview actor path gave nothing: one memory scan at a time (3 per pause at most, in its own thread)
            IS.last_scan = now, IS.scans++;
            HANDLE h = InterlockedExchange(&g_is_thr, 1) ? NULL : CreateThread(NULL, 0, is_scan_thread, NULL, 0, NULL);
            if (h) CloseHandle(h);
            else InterlockedExchange(&g_is_thr, 0);
            LOG("[invsora] preview actor path gave no unit (owner %llx actor %llx): memory scan %d started\n",
                (unsigned long long)owner, (unsigned long long)actor, IS.scans);
        }
    }
    // hide (every frame: cheap writes, the game may rewrite the mask)
    for (int i = 0; i < IS.nu; i++) {
        if (!is_unit_ok(IS.u[i], vt, res, lunit)) continue;
        volatile uint16_t *vis = (volatile uint16_t *)(uintptr_t)(IS.u[i] + 0xc);
        if (*vis != (uint16_t)g_is_mask) *vis = (uint16_t)g_is_mask;
        IS.hidden = true;
    }
    if (g_is_opa && IS.actor && IS.nu && dg_q(IS.actor + 0x4e0)) {
        volatile float *fo = (volatile float *)(uintptr_t)(IS.actor + 0x4f0), *fs = (volatile float *)(uintptr_t)(IS.actor + 0x4ec);
        if (!IS.opa_saved) IS.opa0[0] = *fo, IS.opa0[1] = *fs, IS.opa_saved = true;
        *fo = 0, *fs = 0;
        *(volatile uint8_t *)(uintptr_t)(IS.actor + 0x68e) = 1;
    }
    if (IS.cam_on && g_invsora != 3) { IS.cam_on = false; if (g_is_camfn) g_is_camfn(false, NULL, NULL); }
    if (g_invsora < 1 || g_invsora == 2 || !IS.nu || (g_is_req && sub != 0)) {
        if (IS.cam_on) { IS.cam_on = false; if (g_is_camfn) g_is_camfn(false, NULL, NULL); }
        if (IS.scr_on) { IS.scr_on = false; if (IS.screen) IS.screen(false, NULL, NULL, NULL); }
        return false;
    }
    if (g_invsora == 3 && g_is_menucam && !is_camfns() && !IS.cam_warned)  // (s107: an older exe = the screen-mode Sora below instead)
        IS.cam_warned = true, LOG("[invsora] world mode: the exe lacks sora_draw_camera / _shift (relink): screen mode (invsora 1) instead\n");
    if (g_invsora == 3 && (!g_is_menucam || is_camfns())) {  // (s105 world-puppet mode, s107: in the menu Link's own scene with its camera, under the UI)
        if (IS.scr_on) { IS.scr_on = false; if (IS.screen) IS.screen(false, NULL, NULL, NULL); }
        const Mot *m3 = mot_of(MOT_IDLE);
        if (!PD.pose || !m3 || m3->slot < 0 || !IS.actor) return false;
        float lm[12];
        if (!dg_rd(IS.actor + 0x398, lm, 48)) return false;
        for (int i = 0; i < 12; i++) if (!isfinite(lm[i])) return false;
        // (s106b: the menu Link stands at his scene's ORIGIN, so a zero translation is right; the old check bailed here)
        float cv[12] = {0}, cp[4] = {0}, csh[2] = {0};
        const bool cam_ok = g_is_menucam && is_menu_cam(cv, cp, csh);
        if (g_is_menucam && !cam_ok) {  // (revfix: Link is hidden already, so Sora must show: the screen mode below instead)
            if (!IS.cam_warned) IS.cam_warned = true, LOG("[invsora] world mode: the menu camera did not read sane: screen mode (invsora 1) instead\n");
            if (IS.cam_on) { IS.cam_on = false; if (g_is_camfn) g_is_camfn(false, NULL, NULL); }
            goto is_screen_mode;
        }
        if (cam_ok) g_is_shiftfn(csh[0], csh[1]), g_is_camfn(true, cv, cp), IS.cam_on = true;
        else if (IS.cam_on) g_is_camfn(false, NULL, NULL), IS.cam_on = false;
        if (PD.off) for (int k = 0; k < 2; k++) if (g_party_id[k] > 0) PD.off(g_party_id[k]);  // (the party isn't in the menu scene)
        const float D2R3 = 3.14159265f / 180, s3 = 0.01f * g_k * (float)g_iw_sc / 100;
        float feet3[3] = {lm[3] + g_iw_off[0] * 0.01f * g_k, lm[7] + g_iw_off[1] * 0.01f * g_k, lm[11] + g_iw_off[2] * 0.01f * g_k};
        float face3 = atan2f(lm[2], lm[10]) + (float)g_iw_face * D2R3;
        if (!IS.drew)
            LOG("[invsora] world mode: preview actor %llx at %.2f %.2f %.2f face %.1f deg, scale %.4f; camera %s (pos-at %.2f m, fovy %.1f deg, "
                "aspect %.3f, near %.2f far %.0f, shift %.3f %.3f)\n", (unsigned long long)IS.actor, lm[3], lm[7], lm[11], face3 / D2R3, s3,
                cam_ok ? "= the menu's (owner+0x218)" : "= BotW's world camera (ismenucam 0)", cam_ok ? -cv[11] : 0, cp[2] / D2R3, cp[3], cp[0], cp[1], csh[0], csh[1]);
        const double kh3 = (double)(now - IS.t0) * 0.06;
        const int rid3 = rm_actor();  // (s111 roxmenu: Roxas while he is the body)
        rm_weapon(rid3);
        PD.pose(rid3, m3->slot, mw_clock(kh3, m3->frames, m3->loop), feet3, face3, s3);
        if (rid3 && !IS.drew) LOG("[roxmenu] inventory: Roxas (actor %d) instead of Sora\n", rid3);
        IS.drew = true;
        return true;
    }
is_screen_mode:
    // Sora (screen mode: the Game Over's / menuwalk's exe path: own fixed camera, no scene depth, drawn over BotW's UI)
    if (!IS.tried || (!IS.screen && !(now & 0x3ff))) {
        IS.tried = true;
        IS.screen = (void (*)(bool, const float *, const float *, const float *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_screen");
        LOG("[invsora] sora_draw_screen %s, sora_draw_actor_pose %s\n", IS.screen ? "found" : "NOT in this exe", PD.pose ? "found" : "missing (party not loaded yet)");
    }
    const Mot *m = mot_of(MOT_IDLE);
    if (!IS.screen || !PD.pose || !m || m->slot < 0) return false;
    const float D2R = 3.14159265f / 180, s = 0.01f * g_k, fov = 30 * D2R, hcm = 165.0f;
    const float frac = (float)(g_is_h < 10 ? 10 : g_is_h > 150 ? 150 : g_is_h) / 100;
    const float half_h = hcm / frac * 0.5f, half_w = half_h * (16.0f / 9.0f), dist = half_h / tanf(fov / 2);
    const float ty = 85.0f;  // cm: the camera looks at (0, ty, 0) from +z
    float cam[3] = {0, ty * s, dist * s};
    float view[12] = {1, 0, 0, 0, 0, 1, 0, -cam[1], 0, 0, 1, -cam[2]};
    float proj[4] = {0.05f, 100.0f, fov, 0}, clear[4] = {0, 0, 0, 0};  // (aspect 0 = the game image's; no black rect: BotW's inventory stays)
    IS.screen(true, view, proj, clear);
    if (!g_is_maskfn_tried) {
        g_is_maskfn_tried = true;
        g_is_maskfn = (void (*)(float, float, float, float))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_screen_mask");
        LOG("[invsora] sora_draw_screen_mask %s\n", g_is_maskfn ? "found" : "NOT in this exe (islayer has no effect until the exe is relinked)");
    }
    if (g_is_maskfn) {  // after screen(true): that call keeps a mask set earlier, screen(false) clears it
        if (g_is_layer) g_is_maskfn(g_is_l[0] / 1000.0f, g_is_l[1] / 1000.0f, g_is_l[2] / 1000.0f, g_is_l[3] / 1000.0f);
        else g_is_maskfn(0, 0, 0, 0);
    }
    if (!IS.scr_on) LOG("[invsora] Sora drawn at feet %.1f %.1f %% of the screen, height %d %%, face %d deg\n", g_is_x / 10.0, g_is_y / 10.0, g_is_h, g_is_face);
    IS.scr_on = true;
    float yaw = (float)g_is_face;  // the inventory's own rotate gesture (pc_cursor.c): follow its change since the menu opened
    if (g_is_yaw && sora_sites()->pause_state == 0x7001e1611cull) {
        float y[2];
        if (dg_rd(IS_PREVIEW_YAW, y, 8) && isfinite(y[0])) {
            if (!IS.yaw0_ok) IS.yaw0 = y[0], IS.yaw0_ok = true;
            yaw += (float)g_is_yaw * remainderf(y[0] - IS.yaw0, 360.0f);
        }
    }
    float X = ((float)g_is_x / 1000 - 0.5f) * 2 * half_w, Y = (0.5f - (float)g_is_y / 1000) * 2 * half_h;
    float feet[3] = {X * s, (ty + Y) * s, 0};
    const double kh = (double)(now - IS.t0) * 0.06;
    const int rid1 = rm_actor();  // (s111 roxmenu)
    rm_weapon(rid1);
    PD.pose(rid1, m->slot, mw_clock(kh, m->frames, m->loop), feet, yaw * D2R, s);
    if (rid1 && !IS.drew) LOG("[roxmenu] inventory: Roxas (actor %d) instead of Sora\n", rid1);
    IS.drew = true;
    return true;
}

static bool is_live_set(const char *key, int v) {
    if (!strcmp(key, "invsora")) {
        g_invsora = v < 0 ? 0 : v > 3 ? 3 : v;
        LOG("[sora] invsora %d (0 old, 1 hide the menu Link + draw Sora, 2 hide only)\n", g_invsora);
    }
    else if (!strcmp(key, "isx")) g_is_x = v;
    else if (!strcmp(key, "isy")) g_is_y = v;
    else if (!strcmp(key, "iss")) g_is_h = v;
    else if (!strcmp(key, "isface")) g_is_face = v;
    else if (!strcmp(key, "isyaw")) g_is_yaw = v;
    else if (!strcmp(key, "ismask")) g_is_mask = (uint32_t)v & 0xffff;
    else if (!strcmp(key, "isopa")) g_is_opa = v != 0;
    else if (!strcmp(key, "isreq")) g_is_req = v != 0;
    else if (!strcmp(key, "iwx")) g_iw_off[0] = v;
    else if (!strcmp(key, "iwy")) g_iw_off[1] = v;
    else if (!strcmp(key, "iwz")) g_iw_off[2] = v;
    else if (!strcmp(key, "iwf")) g_iw_face = v;
    else if (!strcmp(key, "iwsc")) g_iw_sc = v < 10 ? 10 : v > 400 ? 400 : v;
    else if (!strcmp(key, "islayer")) g_is_layer = v != 0;
    else if (!strcmp(key, "ismenucam")) g_is_menucam = v != 0;
    else if (!strcmp(key, "invcam")) g_is_cam = v != 0, LOG("[sora] invcam %d: %s\n", v, v ? "the next inventory (after 1.5 s) logs the preview matrix + camera candidates" : "off");
    else if (!strcmp(key, "invdump"))g_is_dump = v != 0, LOG("[sora] invdump %d: %s\n", v, v ? "the next inventory frame (after 1.5 s open) dumps its passes" : "off");
    else if (!strcmp(key, "isl0")) g_is_l[0] = v;
    else if (!strcmp(key, "isl1")) g_is_l[1] = v;
    else if (!strcmp(key, "isl2")) g_is_l[2] = v;
    else if (!strcmp(key, "isl3")) g_is_l[3] = v;
    else return false;
    return true;
}

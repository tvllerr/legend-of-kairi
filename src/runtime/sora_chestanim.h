// sora_chestanim.h: (s106 b18-chestanim, SLICE 1) KH2 chest-open animation: preview + recon. recon/S105_HELPER_CHESTANIM.md section 4,
// recon/S106_HELPER_CHESTANIM1.md. Included from sora.c after sora_chestabil.h (needs CN / ca_load_names, SLX.gdt_bool, chev_measure,
// link_bones_now, mot_slot, S, sfx_slot). DLL ONLY, no exe relink.
//
//   set chestprev <slot>  Sora, standing and driven, plays that mset slot ONCE (550 A130, 554 A140, 522 A141, 38 A110 = the potion, for
//                         comparison) with the slot's own KH2 SE / voice triggers. Fires when the value CHANGES (set chestprev 0 to re-arm
//                         the same slot). Refused with a "[chestprev] refused" log line in the air, in a form / Limit, during an event,
//                         dead / loading, or while another motion owns Sora.
//   set chestanim 0..3    bit 0 (1) = SLICE 2 PLAYBACK on a chest open, bit 1 (2) = the recon log ("[chestanim]" lines). 0 off (default),
//                         1 play, 2 log only, 3 both (the log keeps running with the playback). See the slice 2 block at the end.
//
// Recon log (chestanim 2): change-only lines with ms since the key was set (QueryPerformanceCounter): Link state word, g_event, screens
// 0xb / 0x4a / 0x33, camera detach (chev_measure), Link root height (p[1]) and root / pelvis bone height above his feet (bones 0 / 1, 5 cm steps),
// and a FAST sweep of every "_TBox_" game-data flag (the chestabil name list, ~1611 names, CX_PER_FRAME a frame = ~40 frames), run only
// while an event stretch is on (g_event / screen 0xb / 0x4a) and CX_TAIL presents after it. A baseline sweep runs when the key is set;
// a flag edge is logged with the sweep number, the frame it was read on and the sweep's start time (the edge happened between the two reads).
#pragma once

static int g_chestprev_last = 0, g_chestanim = 0;
static bool g_loading;  // (tentative: defined with the load latch below)
static int g_cxp_cycle = 1, g_cxp_slot = 550, g_cxp_lead = 0, g_cxp_hold = 6000, g_cxp_drop = 8, g_cxp_fall = 900, g_cxp_n;
static int g_cxp_reach = 300;  // (s108: chestreach <cm>, a TBox_* actor this near Link during the event = the start)
static bool g_cxp_zero;  // (slice 2) set in sora.c: the state-0 event stretch (Link's word 0 held g_ev_zero presents) is on
static int g_cprev_req = -1;  // slot asked for by the key, taken by cprev_tick on the next driven frame
#define CX_PER_FRAME 40
#define CX_TAIL 180  // presents the sweep keeps going after the event stretch ends

static LARGE_INTEGER g_cx_t0;  // (review fix: reset when chestanim 2 is set again, so t= counts from the key)
static bool g_cx_reset;
static double cx_ms(void) {
    static LARGE_INTEGER f;
    LARGE_INTEGER n;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&n);
    if (!g_cx_t0.QuadPart) g_cx_t0 = n;
    return (double)(n.QuadPart - g_cx_t0.QuadPart) * 1000.0 / (double)f.QuadPart;
}

static bool cx_live_set(const char *key, int v) {
    if (!strcmp(key, "chestprev")) {
        if (v == 0) { g_chestprev_last = 0, LOG("[chestprev] re-armed\n"); return true; }
        if (v == g_chestprev_last) { LOG("[chestprev] slot %d was the last one played: set chestprev 0 first (or another slot)\n", v); return true; }
        g_chestprev_last = v, g_cprev_req = v;
        LOG("[chestprev] live chestprev %d: plays on the next driven frame\n", v);
    } else if (!strcmp(key, "chestanim")) {
        int nv = v < 0 ? 0 : v > 3 ? 3 : v;  // bit 0 = playback (slice 2), bit 1 = recon log
        if ((nv & 2) && !(g_chestanim & 2)) g_cx_reset = true, g_cx_t0.QuadPart = 0;  // (review fix: fresh baseline + clock)
        g_chestanim = nv;
        cx_ms();
        LOG("[chestanim] live chestanim %d (playback %s, recon log %s)\n", g_chestanim, (nv & 1) ? "ON" : "off", (nv & 2) ? "ON" : "off");
    } else if (!strcmp(key, "chestcycle")) g_cxp_cycle = v != 0, g_cxp_n = 0, LOG("[chestanim] live chestcycle %d (open counter reset)\n", g_cxp_cycle);
    else if (!strcmp(key, "chestslot")) g_cxp_slot = v, LOG("[chestanim] live chestslot %d (used when chestcycle 0)\n", v);
    else if (!strcmp(key, "chestlead")) g_cxp_lead = v < 0 ? 0 : v > 3000 ? 3000 : v, LOG("[chestanim] live chestlead %d ms\n", g_cxp_lead);
    else if (!strcmp(key, "chesthold")) g_cxp_hold = v < 0 ? 0 : v > 6000 ? 6000 : v, LOG("[chestanim] live chesthold %d ms\n", g_cxp_hold);
    else if (!strcmp(key, "chestdrop")) g_cxp_drop = v < 1 ? 1 : v > 60 ? 60 : v, LOG("[chestanim] live chestdrop %d cm\n", g_cxp_drop);
    else if (!strcmp(key, "chestreach")) g_cxp_reach = v < 50 ? 50 : v > 600 ? 600 : v, LOG("[chestanim] live chestreach %d cm\n", g_cxp_reach);
    else if (!strcmp(key, "chestfall")) g_cxp_fall = v < 0 ? 0 : v, LOG("[chestanim] live chestfall %d ms (0 = no fallback)\n", g_cxp_fall);
    else return false;
    return true;
}

// (a) the preview. Called once a frame from the player tick after g_event is known. st = Link's state class.
static void cprev_tick(int st, bool dead) {
    if (g_cprev_req < 0) return;
    int slot = g_cprev_req;
    g_cprev_req = -1;
    const char *why = NULL;
    const Mot *m = mot_slot(slot);
    if (!m) why = "slot not in the motion bake";
    else if (dead || g_loading) why = "dead / loading";
    else if (g_event) why = "an event / cutscene is on (Link's pad)";
    else if (st != LINK_GROUND || S.air_frames || S.takeoff > 0 || ACT_AIR(S.act)) why = "not standing on the ground (air / climb / other)";
    else if (fm_in_form() || IN_LIMIT) why = "in a Drive / Limit Form (its mset has its own slots)";
    else if (S.act != ACT_IDLE || S.special) why = "another motion owns Sora (stand still first)";
    if (why) {
        LOG("[chestprev] refused slot %d: %s\n", slot, why);
        g_chestprev_last = 0;  // (a refused key may be sent again)
        return;
    }
    // play(), but with the Mot picked by SLOT (play() takes a motion id and mot_of() prefers the id*4+3 variant)
    S.motion = slot / 4, S.clock = 0, S.mot = m, S.swing++;
    g_play_trig = LF_SE_OK(m) ? sora_sfx_motion(sfx_slot(m), -1, 0, NULL) : 0;
    S.special = SP_ITEM, S.next = 0;
    LOG("[chestprev] playing slot %d (%d frames = %.2f s), frame-0 sounds %d, its other SE / voice triggers fire as it plays\n", slot, m->frames,
        m->frames / 60.0, g_play_trig);
}

// (b) the recon log
static struct {
    bool had, base, win_was;
    uint32_t sw;
    int ev, scr_b, scr_4a, scr_33, det;
    int h_root, h_b0, h_b1;  // quantised cm
    double h_ms, sw_start;
    uint8_t flag[4096];  // 0 / 1 / 2 = not found
    int i, sweep, nopen, last_nopen;
    uint64_t tail_until;
} CX;

static void cx_tick(uint64_t present, const float p[3]) {
    if (!(g_chestanim & 2)) { CX.had = false; return; }
    if (g_cx_reset) g_cx_reset = false, memset(&CX, 0, sizeof CX);
    double now = cx_ms();
    uint32_t sw = linkpos_link_state_word();
    float dist = 0, ang = 0;
    bool have = chev_measure(p, &dist, &ang);
    bool det = have && (dist > (float)g_evcam || (ang > (float)g_evang && dist >= (float)g_evmin));
    bool active = g_event || g_item_screen || g_msg_screen;
    if (active) CX.tail_until = present + CX_TAIL;
    bool win = active || present < CX.tail_until;
    // heights above Link's feet: bone 0 / 1 world y minus the feet (bones 0 / 1 = GUESS root / pelvis)
    int hb0 = -9999, hb1 = -9999;
    {
        uint64_t w = link_bones_now();
        float lm[12];
        if (w && linkpos_link_mtx(lm)) {
            const float *bw = (const float *)(uintptr_t)w;
            hb0 = (int)lroundf((bw[0 * 16 + 13] - lm[7]) * 20.0f) * 5;  // (units are the game's metres: 5 cm steps)
            hb1 = (int)lroundf((bw[1 * 16 + 13] - lm[7]) * 20.0f) * 5;
        }
    }
    int hr = (int)lroundf(p[1] * 20.0f) * 5;
    // (review fix: the camera-detach edge has the same 100 ms gate as the heights, a camera hovering at g_evcam could flip every frame)
    bool det_edge = (int)det != CX.det && now - CX.h_ms >= 100.0;
    if ((int)det != CX.det && !det_edge) det = CX.det != 0;
    bool edge = !CX.had || sw != CX.sw || (int)g_event != CX.ev || (int)g_item_screen != CX.scr_b || (int)g_msg_screen != CX.scr_4a ||
                (int)g_scr33 != CX.scr_33 || det_edge;
    bool hchg = win && (hr != CX.h_root || hb0 != CX.h_b0 || hb1 != CX.h_b1) && now - CX.h_ms >= 100.0;
    if (edge || hchg) {
        LOG("[chestanim] t=%.0fms present=%llu state=%#x event=%d scr0xb=%d scr0x4a=%d scr0x33=%d camdetach=%d (%.1fm %.0fdeg) root_y=%dcm feet_dy[b0]=%dcm [b1]=%dcm win=%d\n",
            now, (unsigned long long)present, sw, g_event, g_item_screen, g_msg_screen, g_scr33, det, dist, ang, hr, hb0, hb1, win);
        CX.h_ms = now, CX.h_root = hr, CX.h_b0 = hb0, CX.h_b1 = hb1;
    }
    CX.had = true, CX.sw = sw, CX.ev = g_event, CX.scr_b = g_item_screen, CX.scr_4a = g_msg_screen, CX.scr_33 = g_scr33, CX.det = det;
    if (win && !CX.win_was) LOG("[chestanim] t=%.0fms sweep window ON (event %d, 0xb %d, 0x4a %d)\n", now, g_event, g_item_screen, g_msg_screen);
    if (!win && CX.win_was) LOG("[chestanim] t=%.0fms sweep window OFF (last sweep %d, %d chest flags open)\n", now, CX.sweep, CX.last_nopen);
    CX.win_was = win;
    // the TBox sweep: one baseline, then only inside the window
    if (!CN.loaded) ca_load_names();
    if (!CN.n || !SLX.gdt_bool || g_loading) return;  // (review fix: no sweep while loading, the getter refuses there)
    int n = CN.n < 4096 ? CN.n : 4096;
    if (CX.i == 0) {
        if (CX.base && !win) return;
        CX.sw_start = now, CX.nopen = 0;
    }
    for (int k = 0; k < CX_PER_FRAME && CX.i < n; k++, CX.i++) {
        int v = 0;
        uint8_t nv = SLX.gdt_bool(CN.buf + CN.off[CX.i], &v) ? (v != 0) : 2;
        if (nv == 2 && CX.base) nv = CX.flag[CX.i];  // (review fix: an unreadable flag is not an edge: keep the old value)
        if (nv == 1) CX.nopen++;
        if (CX.base && CX.flag[CX.i] != nv)
            LOG("[chestanim] t=%.0fms TBOX FLAG %s %d->%d (sweep %d read at present %llu, started t=%.0fms, state=%#x event=%d)\n", now, CN.buf + CN.off[CX.i],
                CX.flag[CX.i], nv, CX.sweep + 1, (unsigned long long)present, CX.sw_start, sw, g_event);
        CX.flag[CX.i] = nv;
    }
    if (CX.i >= n) {
        CX.sweep++, CX.i = 0, CX.last_nopen = CX.nopen;
        if (!CX.base) CX.base = true, LOG("[chestanim] t=%.0fms baseline sweep done: %d of %d chest flags open\n", now, CX.nopen, CN.n);
        else if (CX.sweep % 5 == 0) LOG("[chestanim] t=%.0fms sweep %d done (%.0f ms): %d open\n", now, CX.sweep, now - CX.sw_start, CX.nopen);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// SLICE 2 (s107 b19-chest): playback of a KH2 motion over Link's puppet while BotW's chest-open sequence plays. recon/S107_HELPER_CHEST.md
//
//   set chestanim 1|3     playback on (3 = with the recon log). Keys: chestcycle (1: each open uses the next of 550 A130 / 554 A140 / 522 A141),
//                         chestslot <slot> (fixed slot when chestcycle 0), chestlead <ms> (delay after the start edge, 0), chesthold <ms>
//                         (longest hold of the last frame, 6000; the whole thing is capped at 6 s), chestdrop <cm> (kneel gate, 8),
//                         chestfall <ms> (fallback start for a state-0 event with no kneel, 900; 0 = no fallback).
//
// START (GUESS, no flag needed: the full inventory keeps the TBox flag from flipping). Link is in an event (st == LINK_OTHER, so Sora is his
// puppet) that is not any known one (not 0x1 warp, not climb / glide / swim / Slate / wake / bed / camp-chest camera / freeze screen / dead /
// loading / message screen 0x4a up) and either
//   "kneel":  Link's pelvis (bone 1) is >= chestdrop cm lower than his standing height (measured on ground frames before the event), or
//   "event-timeout": the state-0 event stretch (g_cxp_zero) has run chestfall ms with no kneel and no camera detach.
// The log line "[chestanim] start via <reason>" names the edge. There is no TBox-actor read in the DLL (needs the exe, not done).
// PLAYBACK: ms by QueryPerformanceCounter, KH frame = ms * 0.06. From start + chestlead the slot pose (two whole frames slerped like
// pose_draw_sm) is blended over Link's puppet pose in over 150 ms, plays once, the last frame is held while the event lasts, blends out over
// 150 ms when the event has been over for 250 ms (the puppet is usually gone by then, so the out-blend is only seen when it lingers).
static struct {
    bool on;
    int phase;  // 1 playing / holding, 2 blending out
    int slot, n;
    const Mot *mot;
    double t_start, t_out, prev_f, ev_off_ms;
    float p[3];
} CP;
static double g_cxp_stand = -1;  // Link's standing pelvis height above his feet (m), from ground frames
static double g_cxp_ev_ms;       // when the current candidate event began (0 = none)
static bool g_cxp_armed = true;  // re-armed once an event is over
static float g_cxp_pose[POSE_BUF_BONES * 12], g_cxp_pose1[POSE_BUF_BONES * 12];

static double cxp_now(void) {
    static LARGE_INTEGER f;
    LARGE_INTEGER n;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&n);
    return (double)n.QuadPart * 1000.0 / (double)f.QuadPart;
}
static bool cxp_pelvis(double *h) {  // pelvis (bone 1) height above Link's feet, m
    uint64_t w = link_bones_now();
    float lm[12];
    if (!w || !linkpos_link_mtx(lm)) return false;
    const float *bw = (const float *)(uintptr_t)w;
    *h = bw[1 * 16 + 13] - lm[7];
    return true;
}
// called once a frame after st is final. excluded = the caller's list of known events (see above) is on.
static void cxp_tick(uint64_t present, int st, uint32_t sw, bool excluded, const float p[3]) {
    double now = cxp_now();
    bool ev = st == LINK_OTHER && !excluded && !g_loading;
    double h = 0;
    bool hv = cxp_pelvis(&h) && h > 0.4 && h < 1.6;
    if (st == LINK_GROUND && !g_event && hv) g_cxp_stand = g_cxp_stand < 0 ? h : g_cxp_stand + (h - g_cxp_stand) * 0.05;
    if (!(g_chestanim & 1)) { CP.on = false; g_cxp_ev_ms = 0; return; }
    if (CP.on) {
        double el = now - CP.t_start;
        if (!ev) {
            if (!CP.ev_off_ms) CP.ev_off_ms = now;
            if (CP.phase == 1 && now - CP.ev_off_ms >= 250.0) CP.phase = 2, CP.t_out = now, LOG("[chestanim] event over: blending out (%.0f ms after the start)\n", el);
        } else CP.ev_off_ms = 0;
        if (CP.phase == 2 && now - CP.t_out >= 150.0) CP.on = false, LOG("[chestanim] done (open #%d, slot %d)\n", CP.n, CP.slot);
        else if (el >= 6000.0) CP.on = false, LOG("[chestanim] hard cap 6 s reached (open #%d, slot %d): off\n", CP.n, CP.slot);
        if (!CP.on) g_cxp_armed = false;
        return;
    }
    if (!ev) { g_cxp_ev_ms = 0, g_cxp_armed = true; return; }
    if (!g_cxp_ev_ms) g_cxp_ev_ms = now;
    if (!g_cxp_armed) return;
    const char *why = NULL;
    double drop = g_cxp_stand > 0 && hv ? g_cxp_stand - h : 0;
    // (s108) START = a treasure chest (TBox_* actor, exe export linkpos_tbox_near) within chestreach of Link while he is in an
    // unknown event. s107's "kneel" fired on a CLIMB (state 0x10 after an X press at a wall) and its playback then held the
    // slot through the real chest event (0.58 s of state 0, too short for chestfall), so the chest never got its own start.
    // The old kneel / event-timeout rules stay only for an exe without the export.
    static float (*tbox)(float *, char *, int);
    static int tb_tried;
    if (!tb_tried) {
        tb_tried = 1, tbox = (float (*)(float *, char *, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_tbox_near");
        LOG("[chestanim] linkpos_tbox_near %s\n", tbox ? "found: start = event + chest within chestreach" : "MISSING (old exe): kneel / event-timeout guesses");
    }
    char tn[48] = "";
    float td = tbox ? tbox(NULL, tn, sizeof tn) : -1;
    if (tbox) {
        if (td >= 0 && td * 100.0f <= (float)g_cxp_reach) why = "chest";
    } else if (drop * 100.0 >= g_cxp_drop) why = "kneel";
    else if (g_cxp_fall && g_cxp_zero && !g_chev && now - g_cxp_ev_ms >= g_cxp_fall) {
        float d, a;
        if (!chev_measure(p, &d, &a) || d <= (float)g_evcam) why = "event-timeout";
    }
    if (!why) return;
    static const int slots[3] = {550, 554, 522};
    int slot = g_cxp_cycle ? slots[g_cxp_n % 3] : g_cxp_slot;
    const Mot *m = mot_slot(slot);
    if (!m) { LOG("[chestanim] start via %s but slot %d is not in the motion bake: nothing played\n", why, slot), g_cxp_armed = false; return; }
    g_cxp_n++;
    memset(&CP, 0, sizeof CP);
    CP.on = true, CP.phase = 1, CP.slot = slot, CP.n = g_cxp_n, CP.mot = m, CP.t_start = now, CP.prev_f = -1;
    memcpy(CP.p, p, sizeof CP.p);
    g_cxp_armed = false;
    if (td >= 0) LOG("[chestanim] chest %s %.2f m from Link\n", tn, td);
    LOG("[chestanim] start via %s (state %#x, pelvis drop %.0f cm of %d needed, event for %.0f ms, present %llu)\n", why, sw, drop * 100.0, g_cxp_drop,
        now - g_cxp_ev_ms, (unsigned long long)present);
    LOG("[chestanim] open #%d plays slot %d (%d frames = %.2f s), lead %d ms\n", g_cxp_n, slot, m->frames, m->frames / 60.0, g_cxp_lead);
}

// in the puppet block, after the retarget and the hand pose, before the Keyblade hide: blends the slot pose into pose (g_rpose).
static void cxp_blend(float *pose) {
    if (!CP.on || !(g_chestanim & 1) || !CP.mot) return;
    double now = cxp_now(), tm = now - CP.t_start - g_cxp_lead;
    if (tm < 0) return;
    int nb = sora_draw_bone_count(), fr = CP.mot->frames;
    if (nb <= 0 || nb > POSE_BUF_BONES || fr < 2) return;
    double f = tm * 0.06, last = fr - 1;
    if (f > last) f = last;  // (hold the last frame; chesthold is applied below, the 6 s cap in cxp_tick)
    float w = (float)(tm / 150.0);
    w = w > 1 ? 1 : w;
    w = w * w * (3 - 2 * w);
    if (tm * 0.06 > last && tm - last / 0.06 > g_cxp_hold) w = 0;  // (held longer than chesthold: back to Link's pose)
    if (CP.phase == 2) {
        float o = (float)((now - CP.t_out) / 150.0);
        o = o > 1 ? 1 : o;
        w *= 1 - o * o * (3 - 2 * o);
    }
    float f0 = floorf((float)f), u = (float)f - f0;
    if (!sora_draw_slot_pose(CP.slot, f0, g_cxp_pose)) return;
    if (u > 1e-3f && f0 + 1 <= (float)last && sora_draw_slot_pose(CP.slot, f0 + 1, g_cxp_pose1))
        for (int b = 0; b < nb; b++) sm_bone(g_cxp_pose + b * 12, g_cxp_pose1 + b * 12, u, g_cxp_pose + b * 12);
    if (w > 0)
        for (int b = 0; b < nb; b++) sm_bone(pose + b * 12, g_cxp_pose + b * 12, w, pose + b * 12);
    // the slot's own KH2 sounds / voice, once per new frame
    if (f > CP.prev_f && LF_SE_OK(CP.mot)) {
        sora_sfx_motion(sfx_slot(CP.mot), CP.prev_f < 0 ? -1.0f : (float)CP.prev_f, (float)f, CP.p);
        CP.prev_f = f;
    }
}

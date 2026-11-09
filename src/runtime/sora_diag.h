// (s183 kairi-diag) The on-screen "Legend of Kairi: Sora's model can't be drawn (...)" notice. A tester had the KH2 HUD and the
// title logo but no body and no log; now the screenshot says why. The decision is sora_diag_core.h (host-tested); the reasons come
// from the exe (sora_draw.c: sora_draw_diag = the first latched draw failure "D<n> text" or the latest skip) and from this DLL
// (E1 never posed, E2 no Link). The text is composed here with the camp menu's sys font (CAMP.seq) and handed to the exe's
// sora_hud_notice, which draws it in the window's bottom-left corner over everything (3 presents per call: it vanishes by itself).
// Included from sora.c after sora_net_avatar.h (needs CAMP, cs_text); the flags and macros it relies on sit near sora_draw_world's
// declaration in sora.c. Cost: one GetProcAddress'd call and a few compares a frame; the quads are composed only when the text changes.
#pragma once
#include "sora_diag_core.h"

#define DG_SCALE 0.5f          // sys font 24 px tall in KH2's 416 px frame -> 12
#define DG_MAX_W 490.0f        // virtual px per line (the frame is 512 wide at 4:3 and wider on wide screens)
#define DG_CHARS 300           // a letter is one quad (+ the box), <= the exe's 512
#define DG_NQ 512

typedef int (*DgExeDiag)(char *, int, uint64_t *);
typedef void (*DgNotice)(const SoraHudQuad *, int);
static struct {
    bool tried;
    DgExeDiag exe;
    DgNotice notice;
    DgCore core;
    int shown_kind, nlogged, nrecov;
    char text[320];           // the message the quads were composed for
    SoraHudQuad q[DG_NQ];
    int nq;
} KDG;

static void dg_bytes(const char *s, int len, uint8_t *out, int *n) {
    int k = 0;
    for (int i = 0; i < len && k < 250; i++) {
        uint8_t b = s[i] == ' ' ? 0x01 : cs_ascii(&CAMP.seq, s[i]);
        if (b) out[k++] = b;
    }
    *n = k;
}
static float dg_measure(void *ud, const char *s, int len) {
    (void)ud;
    uint8_t m[256];
    int n;
    dg_bytes(s, len, m, &n);
    float w = 0, h = 0;
    cs_text(&CAMP.seq, NULL, m, n, 0, 0, DG_SCALE, NULL, &w, &h);
    return w;
}
// the message as quads in the 512 x 416 frame, bottom-left; returns the count (0 = nothing: no font)
static int dg_compose(const char *msg, SoraHudQuad *q, int cap) {
    if (!CAMP.ok || cap < 8) return 0;
    int st[8], ln[8];
    int nl = dgc_wrap(msg, DG_MAX_W, dg_measure, NULL, st, ln, 8);
    if (nl <= 0) return 0;
    const float lh = 24.0f * DG_SCALE + 1.0f, x0 = 6.0f, bottom = 408.0f;
    const float y_top = bottom - (float)nl * lh;
    float wmax = 0;
    for (int i = 0; i < nl; i++) {
        float w = dg_measure(NULL, msg + st[i], ln[i]);
        if (w > wmax) wmax = w;
    }
    SoraHudQuad *b = &q[0];  // the dark box behind the text
    memset(b, 0, sizeof *b);
    const float bx0 = x0 - 4, bx1 = x0 + wmax + 4, by0 = y_top - 3, by1 = bottom + 2;
    b->x[0] = bx0, b->y[0] = by0, b->x[1] = bx1, b->y[1] = by0, b->x[2] = bx0, b->y[2] = by1, b->x[3] = bx1, b->y[3] = by1;
    for (int k = 0; k < 4; k++) b->col[k] = 0x60000000u;
    b->tex = 2;
    CsOut o = {q, 1, cap};
    const float tc[4] = {1.0f, 0.9f, 0.5f, 1};  // (no shadow pass: the dark box carries it, one quad a letter)
    for (int i = 0; i < nl; i++) {
        uint8_t m[256];
        int n;
        dg_bytes(msg + st[i], ln[i], m, &n);
        const float y = y_top + (float)i * lh;
        cs_text(&CAMP.seq, &o, m, n, x0, y, DG_SCALE, tc, NULL, NULL);
    }
    return o.n;
}

// Once per game frame (sora_mod_frame, after the body). now_ms = GetTickCount64().
static void dg_tick(uint64_t now_ms) {
    if (!KDG.tried) {
        KDG.tried = true;
        HMODULE exe = GetModuleHandleA(NULL);
        KDG.exe = (DgExeDiag)(void *)GetProcAddress(exe, "sora_draw_diag");
        KDG.notice = (DgNotice)(void *)GetProcAddress(exe, "sora_hud_notice");
        if (!CAMP.tried) camp_load();  // (the font table: the same file the pause menu uses)
        LOG("[kairi-diag] on-screen notice: exe diag %s, notice %s, font %s\n", KDG.exe ? "yes" : "NO (older exe)",
            KDG.notice ? "yes" : "NO (older exe)", CAMP.ok ? "yes" : "NO (log only)");
    }
    uint64_t drawn = now_ms;  // (no exe diag: "drawn" every tick, so the no-draw rule cannot fire)
    char why[200] = "";
    int latched = 0;
    if (KDG.exe) latched = KDG.exe(why, sizeof why, &drawn);
    int kind = dgc_tick(&KDG.core, now_ms, g_dg_posed && g_dg_playing && !g_rox_hold, g_dg_playing, g_dg_nolink, drawn);
    g_dg_posed = g_dg_playing = g_dg_nolink = false;
    if (kind == DG_OK) {
        if (KDG.shown_kind) {
            if (KDG.notice) KDG.notice(NULL, 0);
            if (KDG.nrecov++ < 3) LOG("[kairi-diag] recovered: the body is being drawn again\n");
            KDG.shown_kind = 0;
        }
        return;
    }
    char reason[240];
    if (kind == DG_NODRAW) {
        if (!KDG.exe) return;
        snprintf(reason, sizeof reason, "%s", why[0] ? why : "DZ the draw pass did not run");
    } else if (kind == DG_NOPOSE) {
        snprintf(reason, sizeof reason, "%s%s", "E1 the game reached the world but made no pose for Sora", latched ? "; " : "");
        if (latched) snprintf(reason + strlen(reason), sizeof reason - strlen(reason), "%s", why);
    } else {
        snprintf(reason, sizeof reason, "E2 BotW's world is running but Link's position was not found");
    }
    char msg[320];
    snprintf(msg, sizeof msg, "Legend of Kairi: Sora's model can't be drawn (%.*s). Please send logs/last.log or the bug report zip to tvllerr.",
             150, reason);
    if (strcmp(msg, KDG.text)) {  // new text: compose again, log it (a few times only)
        snprintf(KDG.text, sizeof KDG.text, "%s", msg);
        KDG.nq = dg_compose(msg, KDG.q, DG_CHARS + 1);
        if (KDG.nlogged++ < 5) LOG("[kairi-diag] %s | notice %s\n", reason, KDG.nq && KDG.notice ? "shown" : "NOT shown (no font / older exe)");
    }
    KDG.shown_kind = kind;
    if (KDG.nq && KDG.notice) KDG.notice(KDG.q, KDG.nq);
}

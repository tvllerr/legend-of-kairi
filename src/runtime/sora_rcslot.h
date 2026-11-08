// sora_rcslot.h (s134 rcslot): KH2's Reaction Command slot over the command menu, for every reaction command (Limit Triangle prompts, the
// react prompts: Reflect / Dodge / Save). The exe draws the slot art + the button icon (sora_hud_menu_rc, comw group 2 / 4, cmd.bin labels
// 42 Triangle / 43 Y); the DLL composes the name with the camp font (CAMP.seq) in the slot's text spot. Included from sora.c before sora_react.h
// (needs CAMP, camp_load, cs_text, g_mkpad, LOG). Live key: set rcslot 0/1 (default 1; 0 = the old camp-font boxes).
#pragma once

static int g_rcslot = 1;

// Draws the slot with <name> this frame (call every frame it shows; the exe hides it 3 presents after the last call).
// Returns false when the exe / cmd.bin / font cannot (rcslot 0, old exe, cmd.bin without group 2 + label 42): the caller draws its old box.
// flash: group 4 over the slot (GUESS: the use flash).
static bool rcs_show(const char *name, bool flash) {
    static int (*fn)(int, int, const SoraHudQuad *, int);
    static bool tried, warned;
    if (!g_rcslot) return false;
    if (!tried) {
        tried = true, fn = (int (*)(int, int, const SoraHudQuad *, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_rc");
        if (!fn) LOG("[rcslot] exe lacks sora_hud_menu_rc: the reaction commands keep the camp-font box until the next exe link\n");
    }
    if (!fn) return false;
    if (!CAMP.tried) camp_load();
    if (!CAMP.ok) return false;
    if (CAMP.drew) return true;  // the camp menu covers the screen: nothing to draw, nothing to fall back to
    static SoraHudQuad q[48];
    CsOut o = {q, 0, 48};
    uint8_t msg[32];
    int len = 0;
    for (int c = 0; name[c] && len < 31; c++) {
        uint8_t b = name[c] == ' ' ? 0x01 : cs_ascii(&CAMP.seq, name[c]);
        if (b) msg[len++] = b;
    }
    // the slot's text origin = slot + (10, -8) (hud_kh2.py TEXT_OFF), the name at pen x 2; same size / row box as the page rows (sub_text_rows).
    // Menu-local PS2 px for the 4-row page (slot y 289, text top = row y - 12); the exe moves it for a page with fewer rows.
    static const float W[4] = {1, 1, 1, 1};
    cs_text(&CAMP.seq, &o, msg, len, 36.0f + 10.0f + 2.0f, 377.0f - 22.0f * 4 - 12.0f, 0.8f, W, NULL, NULL);
    for (int i = 0; i < o.n; i++) for (int c = 0; c < 4; c++) q[i].col[c] = 0x80E6E6E6;
    int icon = g_mkpad == 2 ? 43 : 42;
    int ok = fn(flash ? 2 : 1, icon, q, o.n);
    if (ok == 0 && !warned) warned = true, LOG("[rcslot] cmd.bin has no RC slot art (groups 2..5 / labels 42, 43): reinstall data/hud/cmd.bin; the old box stays\n");
    return ok > 0;
}

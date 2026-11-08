// Multiplayer slice 1 (plan Task 7): the remote player's name plate as sys-font quads for the exe's sora_hud_label.
// Pure (host-tested in test/net_test.c); sora_net_avatar.h sends them every frame the avatar shows.
#pragma once
#include "sora_camp_seq.h"

#define PLATE_SCALE 0.75f  // sys font 24 px tall in KH2's 416 px frame -> 18 (GUESS: readable, smaller than the menu rows)
#define PLATE_MAX 64       // quads: shadow + text per letter (NET_NAME_MAX = 15 letters -> 30)

// <name>'s quads in KH2 virtual px around the plate's anchor: centred on x 0, bottom on y 0; first the shadow pass
// (black, 3/4 alpha, 1.5 px down-right, as cs_draw_node_), then the white text; alpha 0..1 scales both. A space = a gap,
// a char without a glyph is skipped. Returns the count (<= cap); 0 = nothing to draw.
static int plate_quads(const CampSeq *s, const char *name, float alpha, SoraHudQuad *q, int cap) {
    uint8_t msg[32];
    int len = 0;
    for (int c = 0; s && name && name[c] && len < (int)sizeof msg; c++) {
        uint8_t b = name[c] == ' ' ? 0x01 : cs_ascii(s, name[c]);
        if (b) msg[len++] = b;
    }
    if (alpha > 1) alpha = 1;
    if (!len || cap <= 0 || !q || alpha <= 0) return 0;
    float w = 0, h = 0;
    cs_text(s, NULL, msg, len, 0, 0, PLATE_SCALE, NULL, &w, &h);
    float x = -w * 0.5f, y = -h;
    const float sh[4] = {0, 0, 0, 0.75f * alpha}, tc[4] = {1, 1, 1, alpha};
    CsOut o = {q, 0, cap};
    cs_text(s, &o, msg, len, x + 1.5f, y + 1.5f, PLATE_SCALE, sh, NULL, NULL);
    cs_text(s, &o, msg, len, x, y, PLATE_SCALE, tc, NULL, NULL);
    return o.n;
}

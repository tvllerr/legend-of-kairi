// sora_camp_pic.h: the Equipment page's Keyblade picture quad (s120 itempic). Pure: host-tested by runtime/test/camp_pic_test.c.
// Art = D:/SoraBotW/data/hud/itempic.bin (tools_s120/itempic_bake.py; the exe uploads it as texture 13, SoraHudQuad.tex == 3,
// u / v in atlas texels). The atlas is a FIXED grid, so the DLL needs no table or file: cell i = KB_TAB[i] (sora_kb.h index),
// CP_COLS per row, CP_CELL px square. KH2 draws the picture ~128 virtual px square (the PS2 IMGD size) in the right panel.
#pragma once
#include "sora_camp_seq.h"  // SoraHudQuad

#define CP_COLS 6
#define CP_CELL 256
#define CP_N 26  // = KB_N (sora_kb.h; s214: + our own two, cells 24 / 25 = our renders)
#define CP_ATLAS_W (CP_COLS * CP_CELL)
#define CP_LOGICAL 128.0f

// Fills *q with the picture of KB_TAB[kb_index] at (x, y) size w x h (virtual px, the page's frame), alpha 0..255 (255 = opaque).
// Returns 1 on success, 0 (q untouched) for an index outside 0..CP_N-1.
static int cp_pic_quad(SoraHudQuad *q, int kb_index, float x, float y, float w, float h, uint8_t alpha) {
    if (!q || kb_index < 0 || kb_index >= CP_N) return 0;
    float u0 = (float)((kb_index % CP_COLS) * CP_CELL), v0 = (float)((kb_index / CP_COLS) * CP_CELL);
    float u1 = u0 + CP_CELL, v1 = v0 + CP_CELL;
    memset(q, 0, sizeof *q);
    q->x[0] = q->x[2] = x, q->x[1] = q->x[3] = x + w;  // corners TL TR BL BR
    q->y[0] = q->y[1] = y, q->y[2] = q->y[3] = y + h;
    q->u[0] = q->u[2] = u0, q->u[1] = q->u[3] = u1;
    q->v[0] = q->v[1] = v0, q->v[2] = q->v[3] = v1;
    uint32_t a = (uint32_t)alpha * 0x80u / 255u;  // 0x80 = 1.0
    for (int k = 0; k < 4; k++) q->col[k] = a << 24 | 0x808080u;
    q->tex = 3, q->blend = 0;
    return 1;
}

// sora_hud_label's placement (pure, host-tested in test/net_test.c): camp_draw maps KH2 virtual px (qx, qy) to window
// px (cx + (qx - 256) * sx, vy + qy * sy) with sy = vh / 416, sx = sy * 416 / 384, cx = vx + vw / 2 (no game image:
// the window W x H). hud_label_origin gives the virtual point that lands on window px (px, py): the label's quads,
// built around 0, 0, are moved by it.
#pragma once

static inline void hud_label_origin(float px, float py, float vx, float vy, float vw, float vh, float W, float H, float *ox, float *oy) {
    if (vw <= 0 || vh <= 0) vx = vy = 0, vw = W, vh = H;
    float sy = vh / 416.0f, sx = sy * 416.0f / 384.0f, cx = vx + vw * 0.5f;
    *ox = 256.0f + (px - cx) / sx, *oy = (py - vy) / sy;
}

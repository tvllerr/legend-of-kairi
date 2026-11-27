// sora_hud_bi.h - (botwpic, PR #62; s289 b9fix) the one-picture state behind sora_hud_botw_icon / bi_upload (sora_hud.c).
// Pure: no Vulkan, no lock. sora_hud.c calls every function here with g_bi_lock held; runtime/test/hud_bi_test.c tests it.
//   want  = the last picture the DLL asked for (decoded or failed), px / w / h / pend = its decoded pixels waiting for the
//           render thread, failed = want has no picture
//   have  = the picture texture 15 holds right now ("" = none, or one being uploaded)
// sora_hud_botw_icon answers: 1 = have (draw it), 0 = decoded, not uploaded yet, -1 = no picture, 2 = new name (decode it)
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char want[72], have[72], up[72]; uint8_t *px; int w, h, pend, failed; int tw, th; } BiState;

// the DLL asks for actor
static int bi_ask_(BiState *b, const char *actor) {
    int r = !strcmp(actor, b->have) ? 1 : !strcmp(actor, b->want) ? (b->failed ? -1 : 0) : 2;
    if (r == 1 && b->pend) {  // (s288 port) back to the picture in texture 15 while another one waits: drop that one, else the next
        free(b->px);          // camp draw would upload it under this frame's quad (one frame of the wrong picture)
        b->px = NULL, b->pend = 0, b->failed = 0;
        snprintf(b->want, sizeof b->want, "%s", b->have);
    }
    return r;
}
// the asking thread decoded actor (px NULL = no picture): it replaces any picture still waiting
static void bi_decoded_(BiState *b, const char *actor, uint8_t *px, int w, int h) {
    free(b->px);
    b->px = px, b->w = w, b->h = h, b->pend = px != NULL, b->failed = px == NULL;
    snprintf(b->want, sizeof b->want, "%s", actor);
}
// (render thread) take the waiting picture for upload: NULL = nothing waits. (s289 b9fix) Taking one clears have at once:
// texture 15 is about to be overwritten, so a request for the old picture during the upload answers "not uploaded"
// (it is decoded again) instead of 1, which drew the new picture under the old one's quad for a frame or two.
static uint8_t *bi_take_(BiState *b, char name[72], int *w, int *h) {
    uint8_t *px = b->pend ? b->px : NULL;
    *w = b->w, *h = b->h;
    snprintf(name, 72, "%s", b->want);
    if (px) b->px = NULL, b->pend = 0, b->have[0] = 0;
    return px;
}
// (render thread) name is in texture 15
static void bi_done_(BiState *b, const char *name) { snprintf(b->have, sizeof b->have, "%s", name); }
// (render thread, s289 b9fix) name's image / view could not be made (the old image may be gone already): texture 15 holds no
// picture, and name answers -1 from now on instead of 0 forever. A newer picture already waiting keeps its own state.
static void bi_fail_(BiState *b, const char *name) {
    b->have[0] = 0;
    if (!b->pend && !strcmp(b->want, name)) b->failed = 1;
}

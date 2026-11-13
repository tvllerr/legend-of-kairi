// (s209) Melty's lab notes, the game's side: what the FRAME note says about the other players' puppets, and who this
// add-on is for the HELLO note. The notes themselves are made and sent by the kit's add-on helper inside the connector
// (runtime/melty/sora_melty.cpp: sora_melty_addon_setup / _frame / _puppet / _forget, Melty's melty_addon.h Lab); outside a
// Melty room (no connector, the solo build, a UDP game) those calls do nothing.
//   HELLO   once per welcome: add-on "savage-wind/legend-of-kairi", version = the release id Melty's setup wrote into the game
//           folder (.melty-ready.json, its "ready" text's 16 hex) else "dev", "+net<NET_VERSION>"; payload "kairi-sn<NET_VERSION>"
//           (our SNAP: 'S' 'N' version type ..., sora_net_proto.h); 1 m a unit (BotW's world is in metres: sora_melty.h); y up.
//   FRAME   every frame (net_note_frame, sora.c after the puppets are drawn), a note once a second. The counts are this
//           frame's, made where the puppets are drawn (sora_net_avatar.h av_slot_tick):
//             drawn     a remote Sora was posed this frame (sora_draw_actor_pose / the bone pose asked of the exe)
//             animated  ... with a real animation: his slot found in that body's motion table, his skate board slot, or
//                       his bone pose (ragdoll / Link puppet). A slot this copy has no motion for is drawn, not animated.
//             on screen ... and a 1 m ball at his chest (0.9 m over his feet) is inside the camera's view (the exe's
//                       linkpos_camera_view / _proj, the same camera cam_classify reads); no camera this frame: 0.
//             room entities drawn: 0 (the mod draws nothing of the room's own entities: enemies are the game's own).
//           Each drawn puppet also goes to the helper's puppet meter (frozen frames, jumps): moving = his two SNAPs around
//           the time drawn moved over 0.1 m/s (by their arrival times), behind = unknown (-1: a SNAP has no send time),
//           delay = NET_INTERP_MS (the fixed interpolation delay he is drawn at).
// Pure parts (no game state) here, host-tested by runtime/test/net_note_test.c; the glue (net_note_setup / net_note_frame)
// is in sora_net.h, the counting in sora_net_avatar.h.
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sora_net_peer.h"

#define NNOTE_ADDON "savage-wind/legend-of-kairi"
#define NNOTE_CHEST_M 0.9f    // the ball tested against the view: its centre over his feet (m)
#define NNOTE_BALL_M 1.0f     // ... and its radius (m): a 1.7 m Sora fits in it
#define NNOTE_MOVING_MPS 0.1f // the sender moves: faster than this between his two SNAPs around the time drawn (the kit's 0.1 m/s)

// this frame's puppets, counted where they are drawn (sora_net_avatar.h), sent and cleared by net_note_frame
typedef struct { int on_screen, drawn, animated; } NetNoteCount;
static NetNoteCount g_nnote;

// A ball (centre c, radius r, world units) inside the camera's view: v = the view matrix (3 rows right / up / back, the
// translation in column 3: eye = v * (p, 1); the view looks down -back), pr = {near, far, fovy (rad), aspect}.
// Conservative at the corners (a ball near a corner outside both side planes by less than r counts as in).
static inline bool nnote_on_screen(const float v[12], const float pr[4], const float c[3], float r) {
    if (!v || !pr || !c || !(r >= 0)) return false;
    float ex = v[0] * c[0] + v[1] * c[1] + v[2] * c[2] + v[3], ey = v[4] * c[0] + v[5] * c[1] + v[6] * c[2] + v[7],
          ez = v[8] * c[0] + v[9] * c[1] + v[10] * c[2] + v[11];
    float d = -ez;  // depth along the view axis
    float nearp = pr[0], farp = pr[1], fovy = pr[2], asp = pr[3];
    if (!isfinite(d) || !isfinite(ex) || !isfinite(ey) || !(fovy > 0 && fovy < 3.1f) || !(asp > 0) || !(farp > nearp)) return false;
    if (d + r < nearp || d - r > farp) return false;
    float ty = tanf(fovy * 0.5f), tx = ty * asp;
    // side planes through the eye: a point is inside while |x| <= d * tx; the ball while |x| - r * sqrt(1 + tx^2) <= d * tx
    if (fabsf(ex) - r * sqrtf(1 + tx * tx) > d * tx) return false;
    if (fabsf(ey) - r * sqrtf(1 + ty * ty) > d * ty) return false;
    return true;
}

// Was his sender moving at the time he is drawn now (now_ms - NET_INTERP_MS, as peer_sample draws him)? The two SNAPs
// around that time (the newest two when it is past them), by their arrival times: faster than min_mps (metres a second;
// to_m = metres a world unit). Fewer than 2 SNAPs, or two that came together: not moving (unknown is never "moving").
static inline bool nnote_peer_moving(const NetPeer *p, uint64_t now_ms, float to_m, float min_mps) {
    if (!p || p->st == PEER_NONE || p->n < 2) return false;
    int64_t render = (int64_t)now_ms - NET_INTERP_MS;
    int i = p->n - 2;
    while (i > 0 && (int64_t)p->at[i] > render) i--;
    const NetSnap *a = &p->ring[i], *b = &p->ring[i + 1];
    double dt = ((double)p->at[i + 1] - (double)p->at[i]) / 1000.0;
    if (!(dt > 0.001)) return false;
    double dx = b->feet[0] - a->feet[0], dy = b->feet[1] - a->feet[1], dz = b->feet[2] - a->feet[2];
    double mps = sqrt(dx * dx + dy * dy + dz * dz) * to_m / dt;
    return isfinite(mps) && mps > min_mps;
}

// The release id in Melty's setup marker (the game folder's .melty-ready.json, melty-setup/build.py:
// {"ready": "<what>:<16 hex>", "at": ...}): out = the 16 hex (lowercase), true; anything else: false, out "".
static inline bool nnote_ready_id(const char *text, char out[17]) {
    if (!out) return false;
    out[0] = 0;
    if (!text) return false;
    const char *k = strstr(text, "\"ready\"");
    if (!k) return false;
    k += 7;
    while (*k == ' ' || *k == '\t' || *k == '\r' || *k == '\n') k++;
    if (*k++ != ':') return false;
    while (*k == ' ' || *k == '\t' || *k == '\r' || *k == '\n') k++;
    if (*k++ != '"') return false;
    const char *end = strchr(k, '"'), *colon = NULL;
    if (!end || end - k > 200) return false;
    for (const char *q = k; q < end; q++)
        if (*q == ':') colon = q;
    if (!colon || end - colon - 1 != 16) return false;
    for (int j = 0; j < 16; j++) {
        char ch = colon[1 + j];
        if (ch >= 'A' && ch <= 'F') ch = (char)(ch - 'A' + 'a');
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) { out[0] = 0; return false; }
        out[j] = ch;
    }
    out[16] = 0;
    return true;
}

// HELLO's version: "<release id>+net<v>" (Melty's install) or "dev+net<v>" (no marker: a dev or GitHub copy)
static inline void nnote_version(char *out, int cap, const char *marker_text, int net_version) {
    if (!out || cap <= 0) return;
    char id[17];
    snprintf(out, (size_t)cap, "%s+net%d", nnote_ready_id(marker_text, id) ? id : "dev", net_version);
}

// (s209 step 2) HURT: Link's life (quarter hearts, linkpos_link_life BEFORE Second Chance's clamp is applied by the mod)
// on driven frames. A drop since the last counted frame is a hurt by an enemy or the world (co-op: no player hurts
// another), its amount in quarter hearts. skip: a frame that must not count (a load, the Game Over, Mickey's rescue
// running, Melty off): the next frame starts over, so a heal, a load's new life or the rescue's life never reads as a hurt.
// -> the drop (> 0), else 0. *prev < 0 = nothing to compare with yet.
static inline int nnote_hurt_step(int *prev, int life, bool skip) {
    if (skip || life < 0) { *prev = -1; return 0; }
    int drop = *prev >= 0 && life < *prev ? *prev - life : 0;
    *prev = life;
    return drop;
}

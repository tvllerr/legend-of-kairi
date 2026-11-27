// sora_labhorse_core.h (s291 labhorse): the pure parts of the LAB-ONLY `labhorse` live line (sora_labhorse.h): the line's
// parse, which actor names it takes, the spot at Link's side, the nearest horse, a request's wait. No game, no clock inside
// (times come in as ms): host-tested by runtime/test/labhorse_test.c.
// Why: nethorse's lab tests F2-F7 (sb/s289/objs/LAB_STEPS.md "F") need our Link on a horse; neither lab save owns one (s290 /
// s291 LAB_RESULTS: the Outskirt Stable stall horses are fenced Animal_Horse_A decor, wild ones need a sneak + mount by hand).
#pragma once
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sora_net_objs_core.h"  // (nh_horse: the names nethorse sends as "the horse our Link rides")

#define LH_NAME_MAX 40           // a horse actor's name (the longest, GameRomHorseBone_AllDay, is 23)
#define LH_DEFAULT "GameRomHorse01"  // romfs bgparamlist: Nature 0, ASVariation Slow, HorseUnit CalmDownNum 10 (the lowest)
#define LH_NEAR_M 150.0f         // `labhorse near`: how far a loaded horse may be (default)
#define LH_NEAR_MAX 2000.0f
#define LH_SIDE_M 1.6f           // the spot: this far to the side of Link (+X of his yaw; which side that is = GUESS) ...
#define LH_FWD_M 0.4f            // ... this far ahead ...
#define LH_UP_M 0.3f             // ... and this high (it drops onto the ground: GUESS, no ground probe)
#define LH_SLOW_MS 15000         // a request still loading after this: said once
#define LH_FRAME_MS 3000         // a command that never met a game-thread frame in this long: refused

typedef enum { LH_NONE, LH_SPAWN, LH_HERE, LH_NEAR, LH_OFF, LH_STAM, LH_DIAG, LH_BAD } LhCmd;
static const char *const LH_CMD_TXT[] = {"none", "spawn", "here", "near", "off", "stam", "diag", "bad"};

// a horse nethorse counts (nh_horse: GameRomHorse* without Saddle / Reins, GameROMMotorcycle), plain letters only
static inline bool lh_name_ok(const char *n) {
    if (!n || !n[0] || strlen(n) >= LH_NAME_MAX) return false;
    for (const char *c = n; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '_') return false;
    return nh_horse(n);
}

// "labhorse ..." -> the command (LH_NONE = not this line). name gets the actor (LH_SPAWN), arg the number (LH_NEAR metres,
// LH_STAM 0 / 1). Unknown words / bad names / bad numbers = LH_BAD (said in the log, nothing done).
static inline LhCmd lh_parse(const char *ln, char name[LH_NAME_MAX], float *arg) {
    name[0] = 0, *arg = 0;
    if (!ln || strncmp(ln, "labhorse", 8) || (ln[8] && ln[8] != ' ')) return LH_NONE;
    const char *p = ln + 8;
    while (*p == ' ') p++;
    char w[64] = "", x[64] = "", extra[8] = "";
    int n = sscanf(p, "%63s %63s %7s", w, x, extra);
    if (n <= 0) return snprintf(name, LH_NAME_MAX, "%s", LH_DEFAULT), LH_SPAWN;
    if (n == 3) return LH_BAD;
    char *end = NULL;
    if (!strcmp(w, "here")) return n == 1 ? LH_HERE : LH_BAD;
    if (!strcmp(w, "off")) return n == 1 ? LH_OFF : LH_BAD;
    if (!strcmp(w, "diag")) return n == 1 ? LH_DIAG : LH_BAD;
    if (!strcmp(w, "near")) {
        if (n == 1) return *arg = LH_NEAR_M, LH_NEAR;
        float m = strtof(x, &end);
        if (end == x || *end || !(m > 0) || m > LH_NEAR_MAX) return LH_BAD;
        return *arg = m, LH_NEAR;
    }
    if (!strcmp(w, "stam")) {
        if (n != 2 || (strcmp(x, "0") && strcmp(x, "1"))) return LH_BAD;
        return *arg = (float)(x[0] - '0'), LH_STAM;
    }
    if (n != 1 || !lh_name_ok(w)) return LH_BAD;
    return snprintf(name, LH_NAME_MAX, "%s", w), LH_SPAWN;
}

// the spot at Link's side from his 3x4 matrix (row-major, translation in m[3] m[7] m[11], as no_mtx reads +0x398): upright,
// facing his yaw (his forward = the matrix's 3rd column, flattened), LH_SIDE_M along the yaw's +X, LH_FWD_M ahead, LH_UP_M up
static inline void lh_side_mtx(const float lm[12], float out[12]) {
    float fx = lm[2], fz = lm[10], l = sqrtf(fx * fx + fz * fz);
    if (!(l > 1e-3f) || !isfinite(l)) fx = 0, fz = 1, l = 1;
    fx /= l, fz /= l;
    // columns: c0 = (fz, 0, -fx) (his yaw's +X), c1 = up, c2 = (fx, 0, fz) (his forward): a proper rotation (det 1)
    out[0] = fz, out[1] = 0, out[2] = fx;
    out[4] = 0, out[5] = 1, out[6] = 0;
    out[8] = -fx, out[9] = 0, out[10] = fz;
    out[3] = lm[3] + fz * LH_SIDE_M + fx * LH_FWD_M;
    out[7] = lm[7] + LH_UP_M;
    out[11] = lm[11] - fx * LH_SIDE_M + fz * LH_FWD_M;
}

// the nearest of n horses (pos, names; skip[i] = not this one: Link himself, being deleted, ...) within maxd of me. -1 = none
static inline int lh_nearest(const float me[3], const float (*pos)[3], const char *const *names, const bool *skip, int n, float maxd,
                             float *dist) {
    int bi = -1;
    float bd = maxd;
    for (int i = 0; i < n; i++) {
        if ((skip && skip[i]) || !names[i] || !nh_horse(names[i])) continue;
        float dx = pos[i][0] - me[0], dy = pos[i][1] - me[1], dz = pos[i][2] - me[2], d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (isfinite(d) && d < bd) bd = d, bi = i;
    }
    if (dist) *dist = bi >= 0 ? bd : 0;
    return bi;
}

// a request's wait: the unit's state (-1 none yet, 2 ready, 3 / 5 failed: the PR's / loot's reading, VERIFIED in game for loot
// items s289-s290) and how long it has waited
typedef enum { LHW_WAIT, LHW_TAKE, LHW_FAILED, LHW_SLOW } LhWait;
static inline LhWait lh_wait(int unit_state, uint64_t waited_ms, bool said_slow) {
    if (unit_state == 2) return LHW_TAKE;
    if (unit_state == 3 || unit_state == 5) return LHW_FAILED;
    return waited_ms >= LH_SLOW_MS && !said_slow ? LHW_SLOW : LHW_WAIT;
}

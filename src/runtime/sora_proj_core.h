// (s183 amd FIX 2) Which projection Sora's body pass uses (pure; host-tested in test/proj_test.c). sora_draw.c's
// skin_publish asks it once per publish for BotW's world camera (not the KH2 Game Over screen camera, not the DLL's
// camera override, which bring their own):
//   scan      linkpos_camera_proj: the sead PerspectiveProjection the camera scan picked {near, far, fovy rad, aspect}.
//             Its rule (linkpos.c proj_gameplay) wants aspect 1.45..2.5, so on a 4:3 / 5:4 / 32:9 window (the world
//             camera takes the window's shape) it finds none.
//   exe       output_world_proj (nvn/output_scale.c rt_persp): the world camera's projection as Savage Wind built it
//             {tan(fovy / 2), aspect, near, far}, the widened values that are drawn, any window shape.
// mode (SAVAGEWIND_SORA_PROJ_SRC): PROJ_AUTO = the scan when it has one (the old behaviour, byte for byte: 16:9 PC),
// unless its aspect is not the exe's (> 1% apart: a projection of another shape than the image drawn, e.g. a 16:9 view
// picked on a 4:3 window = a stretched body), else the exe's; PROJ_EXE = the exe's first (test: =exe on 16:9 must look
// the same as auto); PROJ_SCAN = the scan only (s182's choice; none without it). Neither = PROJ_SRC_NONE: the pass
// keeps its guessed projection for placing the body but turns the scene-depth occlusion off for that frame (a guessed
// near / far hid the whole body).
#pragma once
#include <math.h>
#include <stdbool.h>

enum { PROJ_AUTO = 0, PROJ_EXE = 1, PROJ_SCAN = 2 };
enum { PROJ_SRC_NONE = 0, PROJ_SRC_SCAN = 1, PROJ_SRC_EXE = 2 };

// exe = {tan(fovy / 2), aspect, near, far}: a plausible perspective (zeros before the first world camera)
static inline bool proj_exe_sane(const float e[4]) {
    return isfinite(e[0]) && isfinite(e[1]) && isfinite(e[2]) && isfinite(e[3]) && e[0] > 0.05f && e[0] < 20 &&
           e[1] > 0.2f && e[1] < 10 && e[2] > 0 && e[3] > e[2] && e[3] < 1e7f;
}
// exe -> the pass's {near, far, fovy rad, aspect}
static inline void proj_from_exe(const float e[4], float out[4]) {
    out[0] = e[2], out[1] = e[3], out[2] = 2 * atanf(e[0]), out[3] = e[1];
}
// scan (as linkpos gives it) and exe describe the same projection (log only: aspect / far 1%, tan(fovy / 2) 3%, near 10%)
static inline bool proj_agree(const float s[4], const float e[4]) {
    float t = tanf(s[2] * 0.5f);
    return fabsf(s[3] / e[1] - 1) < 0.01f && fabsf(t / e[0] - 1) < 0.03f && fabsf(s[1] / e[3] - 1) < 0.01f &&
           fabsf(s[0] / e[2] - 1) < 0.10f;
}
// The pick: out = {near, far, fovy rad, aspect} of the source returned (untouched for PROJ_SRC_NONE).
static inline int proj_pick(int mode, bool scan_ok, const float scan[4], bool exe_ok, const float exe[4], float out[4]) {
    exe_ok = exe_ok && proj_exe_sane(exe);
    if (mode == PROJ_EXE && exe_ok) return proj_from_exe(exe, out), PROJ_SRC_EXE;
    if (scan_ok && !(mode == PROJ_AUTO && exe_ok && fabsf(scan[3] / exe[1] - 1) >= 0.01f)) {
        for (int i = 0; i < 4; i++) out[i] = scan[i];
        return PROJ_SRC_SCAN;
    }
    if (mode != PROJ_SCAN && exe_ok) return proj_from_exe(exe, out), PROJ_SRC_EXE;
    return PROJ_SRC_NONE;
}

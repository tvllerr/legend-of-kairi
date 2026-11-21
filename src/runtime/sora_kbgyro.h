// sora_kbgyro.h (s263 kbthrowgyro, live "set kbthrowgyro 0 / 1", default 1: verified by the user in s263, "gyro aim now works"): gyro aim while the Keyblade
// throw is aimed (sora_kbthrow.h KT_AIM). User s262: "gyro aim for the Keyblade throw like the botw throw weapons". NOT KH2: the
// mod's own, his call. BotW adds the pad's gyro to the camera only in its own aim modes (Link has the pad there); in the Keyblade
// aim Sora drives, so the mod does it. The easy way (s263): the gyro becomes MOUSE motion. The exe has a native mouse camera
// (hle/input.c input_cam_frame: 0.022 degrees per count x Mouse sensitivity, no dead zone, no curve, both axes alike), fed by
// SDL_EVENT_MOUSE_MOTION events. So each frame of the aim: the pad's new six-axis samples (the exe's own reader, the HLE of
// nn::hid::GetSixAxisSensorStates: 200 Hz, turns per second, his Motion sensitivity / inversion already in them) -> degrees ->
// counts -> mouse-motion events pushed into SDL's queue (SDL_PushEvent is thread safe). DLL only.
// NEEDS: Controls > Mouse camera ON and the game window focused (the exe takes mouse motion only then), a pad with a gyroscope.
// A lock-on target aims by itself: no gyro then. GUESSES until he tries it: the two signs, the feel (1 degree of the pad = 1
// degree of the camera at kbthrowgyrosens 100).
// Live: kbthrowgyro 0/1 | kbthrowgyrosens <% left / right, negative = the other way> | kbthrowgyroy <% up / down, same> |
// kbthrowgyrolog 0/1.
#pragma once
#include "settings.h"

typedef struct {  // nn::hid::SixAxisSensorState as hle/motion.c fills it
    int64_t delta_time, sampling_number;
    float acc[3], gyro[3], angle[3];  // gyro: turns per second, the controller's axes (X right, Y forward, Z up out of its face)
    float dir[3][3];                  // the controller's X, Y and Z axes in a world whose Z is up
    uint32_t attributes, reserved;
} KgState;
_Static_assert(sizeof(KgState) == 0x60, "nn::hid::SixAxisSensorState");
typedef union {  // SDL3's SDL_Event with its SDL_MouseMotionEvent (checked against SDL3/SDL_events.h in s263)
    struct {
        uint32_t type, reserved;
        uint64_t timestamp;  // 0: SDL stamps it
        uint32_t windowID, which, state;
        float x, y, xrel, yrel;
    } motion;
    uint8_t pad[128];
} KgEvent;
_Static_assert(sizeof(KgEvent) == 128, "SDL_Event");
#define KG_MOUSE_MOTION 0x400  // SDL_EVENT_MOUSE_MOTION
#define KG_PIECE 3.9f          // counts per event: one over 4 makes the exe show keyboard button pictures (input.c last_device)
#define KG_PIECES 64

static int g_kg_on = 1, g_kg_sens = 100, g_kg_sensy = 100, g_kg_log = 1;
// (kbthrowstick) the right stick aims too. The exe's camera takes the mouse OR the stick (hle/input.c input_cam_frame: any mouse
// motion in a frame and the cameras read it instead of the stick), and the gyro above sends mouse motion every frame of the aim
// (a still pad still reads a little), so the right stick never turned the aim (user report; epictest log: the camera turned only
// what the gyro asked, 0.7 / -5.2 deg). With this on, a frame whose right stick is past g_kg_dead % sends no gyro motion: BotW's
// own stick camera turns the aim at its own speed; the gyro's samples of that frame are used up (no jump on letting go).
// On by default (tested in game); SAVAGEWIND_SORA_KBTHROW_STICK=0 / live "set kbthrowstick 0" = gyro only (as before);
// "set kbthrowstickdz <%>" the stick's dead zone (GUESS 25).
static int g_kg_stick = 1, g_kg_dead = 25;
static struct {
    bool tried, was, warned;
    HostFn read;
    bool (*push)(void *);
    int64_t last;                    // the newest sample already used
    double yaw, pitch, cam0;         // this aim: degrees asked (left + / up +), the camera's heading at its start
    int frames, events, samples;
} KG;

static float kg_cam_yaw(void) {
    float cf[2];
    return linkpos_camera_fwd(cf) ? atan2f(cf[0], cf[1]) * 57.29578f : 0;
}

// sora.c, right after kt_tick_press. aim: the Keyblade aim is held and follows the camera (no lock-on target)
static void kg_tick(bool aim) {
    if (!g_kg_on || !aim) {
        if (KG.was) {
            KG.was = false;
            float d = kg_cam_yaw() - (float)KG.cam0;
            d -= 360.0f * floorf((d + 180.0f) / 360.0f);
            if (g_kg_log)
                LOG("[kbgyro] aim over: %d frames, %d samples, asked %.1f deg left / %.1f deg up in %d mouse events; the camera turned %.1f deg (the right stick turns it too)\n",
                    KG.frames, KG.samples, KG.yaw, KG.pitch, KG.events, d);
        }
        return;
    }
    if (!KG.tried) {
        KG.tried = true;
        KG.read = hle_lookup_impl("_ZN2nn3hid22GetSixAxisSensorStatesEPNS0_18SixAxisSensorStateEiRKNS0_19SixAxisSensorHandleE");
#ifdef _WIN32
        HMODULE sdl = GetModuleHandleA("SDL3.dll");
        KG.push = sdl ? (bool (*)(void *))(void *)GetProcAddress(sdl, "SDL_PushEvent") : NULL;
#endif
        LOG("[kbgyro] six-axis reader %s, SDL_PushEvent %s\n", KG.read ? "found" : "NOT in this exe", KG.push ? "found" : "NOT found (no gyro aim in this build)");
    }
    if (!KG.read || !KG.push) return;
    static Cpu c;
    static KgState st[16];
    static uint32_t handle = 3 | 2 << 16;  // (FullKey, npad 0: player 1's controller)
    c.x[0] = (uint64_t)(uintptr_t)st, c.x[1] = 16, c.x[2] = (uint64_t)(uintptr_t)&handle;
    KG.read(&c);
    int n = (int)c.x[0];
    if (n <= 0 || n > 16) return;
    if (!KG.was) {  // the aim's first frame: from this sample on
        KG.was = true, KG.last = st[0].sampling_number;
        KG.yaw = KG.pitch = 0, KG.frames = KG.events = KG.samples = 0, KG.cam0 = kg_cam_yaw();
        if (!g_settings.mouse_camera && !KG.warned) KG.warned = true, LOG("[kbgyro] Controls > Mouse camera is OFF: the exe takes no mouse motion, so no gyro aim (turn it on)\n");
        if (g_kg_log)
            LOG("[kbgyro] aim on: sample %lld, mouse camera %d, sensitivity %.2f (vertical x %.2f%s), gyro sens %d / %d %%\n", (long long)KG.last, (int)g_settings.mouse_camera,
                g_settings.mouse_sens, g_settings.mouse_sens_y, g_settings.mouse_invert_y ? ", inverted" : "", g_kg_sens, g_kg_sensy);
        return;
    }
    // The turn since the last frame, on the world's axes: left / right = about the world's up, up / down = about the controller's
    // right axis laid level (so a pad held tilted or rolled a little still aims straight)
    double yaw = 0, pitch = 0;
    for (int i = 0; i < n && st[i].sampling_number > KG.last; i++) {
        const KgState *s = &st[i];
        double dt = s->delta_time > 0 && s->delta_time < 100000000 ? (double)s->delta_time / 1e9 : 0.005, w[3];
        for (int j = 0; j < 3; j++) w[j] = s->gyro[0] * s->dir[0][j] + s->gyro[1] * s->dir[1][j] + s->gyro[2] * s->dir[2][j];
        double hx = s->dir[0][0], hy = s->dir[0][1], hl = sqrt(hx * hx + hy * hy);
        yaw += w[2] * 360.0 * dt;
        pitch += (hl > 0.2 ? (w[0] * hx + w[1] * hy) / hl : s->gyro[0]) * 360.0 * dt;
        KG.samples++;
    }
    KG.last = st[0].sampling_number;
    KG.frames++;
    yaw *= g_kg_sens / 100.0, pitch *= g_kg_sensy / 100.0;
    if (g_kg_stick) {  // (kbthrowstick) the stick pushed: this frame is the stick's (no gyro mouse motion, its samples dropped)
        int32_t dz = (int32_t)(32767 * g_kg_dead / 100);
        bool stick = g_rx_raw > dz || g_rx_raw < -dz || g_ry_raw > dz || g_ry_raw < -dz;
        static bool was_stick;
        if (stick != was_stick && g_kg_log) LOG("[kbgyro] right stick %s\n", stick ? "pushed: it aims (no gyro motion while it is)" : "let go: the gyro aims again");
        was_stick = stick;
        if (stick) return;
    }
    if (yaw == 0 && pitch == 0) return;
    KG.yaw += yaw, KG.pitch += pitch;
    // degrees -> mouse counts (input_cam_frame: a count = 0.022 deg x Mouse sensitivity on both axes; the vertical part also takes
    // "Mouse vertical sensitivity" and "Invert mouse Y", taken out again here: the gyro is not the mouse). Mouse right = +x,
    // mouse up = -y; the pad turned left = +yaw, its front up = +pitch.
    double per = 0.022 * (g_settings.mouse_sens > 0.01f ? g_settings.mouse_sens : 1.0f);
    double sy = g_settings.mouse_sens_y > 0.01f ? g_settings.mouse_sens_y : 1.0f;
    float dx = (float)(-yaw / per), dy = (float)(-pitch / (per * sy) * (g_settings.mouse_invert_y ? -1 : 1));
    int pieces = (int)ceilf((fabsf(dx) + fabsf(dy)) / KG_PIECE);
    if (pieces < 1) pieces = 1;
    if (pieces > KG_PIECES) pieces = KG_PIECES;  // (a wild swing: bigger pieces, the button pictures may flip for a moment)
    for (int i = 0; i < pieces; i++) {
        KgEvent e;
        memset(&e, 0, sizeof e);
        e.motion.type = KG_MOUSE_MOTION, e.motion.xrel = dx / (float)pieces, e.motion.yrel = dy / (float)pieces;
        if (KG.push(&e)) KG.events++;
    }
}

static bool kg_live_set(const char *key, int v) {
    if (!strcmp(key, "kbthrowstick")) { g_kg_stick = v != 0; LOG("[kbgyro] kbthrowstick %d: %s\n", g_kg_stick, g_kg_stick ? "the right stick aims the throw (no gyro while it is pushed)" : "gyro only (the stick is drowned out)"); return true; }
    if (!strcmp(key, "kbthrowstickdz")) { g_kg_dead = v < 1 ? 1 : v > 90 ? 90 : v; return true; }
    if (!strcmp(key, "kbthrowgyro")) g_kg_on = v != 0, LOG("[kbgyro] kbthrowgyro %d: %s\n", g_kg_on, g_kg_on ? "the pad's gyro aims the Keyblade throw (needs Mouse camera on)" : "off");
    else if (!strcmp(key, "kbthrowgyrosens")) g_kg_sens = v < -1000 ? -1000 : v > 1000 ? 1000 : v, LOG("[kbgyro] left / right %d %%\n", g_kg_sens);
    else if (!strcmp(key, "kbthrowgyroy")) g_kg_sensy = v < -1000 ? -1000 : v > 1000 ? 1000 : v, LOG("[kbgyro] up / down %d %%\n", g_kg_sensy);
    else if (!strcmp(key, "kbthrowgyrolog")) g_kg_log = v != 0;
    else return false;
    return true;
}

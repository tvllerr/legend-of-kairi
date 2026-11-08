// sora_light.h (s121 a1, hot-reload DLL side): time-of-day / weather / indoor light for Sora and the party.
// Included by sora.c (static functions only). Feeds the exe's sora_draw_light(p, 16 floats) every frame while on:
//   L0 = (sun dir x,y,z, style)   L1 = (sun r,g,b, amount 0..1)   L2 = (ambient top r,g,b, ambient floor 0..1)
//   L3 = (ambient bottom r,g,b, shadow 0..1 = 0 for now).  style 0 = off (today's look), 1 KH2 soft, 2 toon, 3 BotW soft.
// Sun dir = unit vector FROM the model TOWARD the sun, BotW world space (y up). Colours = linear multipliers around 1.
// Keys (sora.c live "set <key> <v>", ints only): light 0/1 (default 0), lightstyle 1/2/3 (1), lightamt <pct> (70),
// lightfloor <pct> (65), lighthour <h> (-1 = real time), lighthm <tenths of an hour> (e.g. 125 = 12:30; wins over
// lighthour), lightyaw <deg> (0), lightlog 1.   Log tag "[light]".
// The pure math (light_sun_dir, light_sun_rgb, ...) has no windows/LOG dependency: tests define LIGHT_TEST_ONLY.
// ALL numbers are GUESSES, tune by eye (S121_HELPER_LIGHTDLL.md lists them); BotW's real arc/colours come with a2.
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

static float light_smooth(float a, float b, float x) {
    float u = (x - a) / (b - a);
    u = u < 0 ? 0 : u > 1 ? 1 : u;
    return u * u * (3 - 2 * u);
}
static float light_wrap24(float h) {
    h = fmodf(h, 24.0f);
    return h < 0 ? h + 24.0f : h;
}
static void light_norm3(float *v) {
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l < 1e-6f) v[0] = 0, v[1] = 1, v[2] = 0;
    else v[0] /= l, v[1] /= l, v[2] /= l;
}

// Sun direction from the hour (0..24). GUESS: sun up 05:00..19:00, rises east (+x), peak 60 deg elevation at 12:00
// toward the south (+z; -z = north is a GUESS), sets west (-x). Night: a moon light from the west, 55 deg up.
// yaw_deg rotates the whole arc around y (key lightyaw) to fix a wrong east/north guess.
static void light_sun_dir(float hour, float yaw_deg, float out[3]) {
    const float PI = 3.14159265f;
    float h = light_wrap24(hour);
    float t = (h - 5.0f) / 14.0f;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    float elev = 60.0f * sinf(PI * t) * PI / 180.0f;  // 0 at the horizon, 60 deg at noon
    float phi = PI * t;                               // 0 = east, pi/2 = south, pi = west
    float s[3] = {cosf(elev) * cosf(phi), sinf(elev), cosf(elev) * sinf(phi)};
    float me = 55.0f * PI / 180.0f;
    float m[3] = {-cosf(me), sinf(me), 0.15f * cosf(me)};  // moon: west-ish, high
    float w = light_smooth(4.5f, 5.5f, h) * (1.0f - light_smooth(19.0f, 20.0f, h));
    float d[3];
    for (int i = 0; i < 3; i++) d[i] = s[i] * w + m[i] * (1.0f - w);
    light_norm3(d);
    float yw = yaw_deg * PI / 180.0f, c = cosf(yw), sn = sinf(yw);
    out[0] = d[0] * c + d[2] * sn, out[1] = d[1], out[2] = -d[0] * sn + d[2] * c;
}

// Sun colour keyframes per hour, wrapping 24 -> 0 (the last row repeats the first). GUESS, tune by eye.
static const float LIGHT_KEYS[][4] = {
    {0.0f, 0.22f, 0.30f, 0.55f},   // night: dim blue (moon)
    {4.0f, 0.22f, 0.30f, 0.55f},
    {5.5f, 1.15f, 0.70f, 0.45f},   // dawn: orange
    {7.0f, 1.10f, 0.92f, 0.78f},
    {8.5f, 1.05f, 1.00f, 0.95f},   // day: warm white
    {12.0f, 1.05f, 1.00f, 0.92f},
    {16.0f, 1.05f, 0.98f, 0.88f},
    {17.5f, 1.20f, 0.72f, 0.42f},  // dusk: red-orange
    {19.0f, 1.10f, 0.50f, 0.35f},
    {20.5f, 0.22f, 0.30f, 0.55f},  // night
    {24.0f, 0.22f, 0.30f, 0.55f},
};
static void light_sun_rgb(float hour, float out[3]) {
    float h = light_wrap24(hour);
    int n = (int)(sizeof LIGHT_KEYS / sizeof LIGHT_KEYS[0]);
    for (int i = 0; i + 1 < n; i++) {
        if (h >= LIGHT_KEYS[i][0] && h <= LIGHT_KEYS[i + 1][0]) {
            float u = (h - LIGHT_KEYS[i][0]) / (LIGHT_KEYS[i + 1][0] - LIGHT_KEYS[i][0]);
            u = u * u * (3 - 2 * u);
            for (int k = 0; k < 3; k++) out[k] = LIGHT_KEYS[i][1 + k] + (LIGHT_KEYS[i + 1][1 + k] - LIGHT_KEYS[i][1 + k]) * u;
            return;
        }
    }
    out[0] = out[1] = out[2] = 1.0f;
}
// Ambient from the sun colour: top = a bluer, slightly darker version; bottom = darker still (GUESS).
static void light_amb(const float sun[3], float top[3], float bot[3]) {
    static const float SKY[3] = {0.70f, 0.82f, 1.00f};
    for (int k = 0; k < 3; k++) top[k] = (sun[k] * 0.5f + SKY[k] * 0.5f) * 0.90f, bot[k] = top[k] * 0.55f;
}

// Weather byte -> (rain 0..1, snow 0..1). GUESS mapping: 0 sunny, 1 cloudy, 2 rain, 3 heavy rain, 4 snow,
// 5 heavy snow, 6 thunder (storm). The raw value is logged; fix the mapping once seen.
static void light_weather(int w, float *rain, float *snow) {
    // (review: decomp worldDefines.h WeatherType: 7 ThunderRain, 8 BlueskyRain = sun shower; were mapped to dry)
    *rain = (w == 2 || w == 8) ? 0.7f : (w == 3 || w == 6 || w == 7) ? 1.0f : (w == 1) ? 0.25f : 0.0f;
    *snow = (w == 4) ? 0.7f : (w == 5) ? 1.0f : 0.0f;
}

// Full parameter set from the blended state. rain/snow/indoor are 0..1 (already smoothed by the caller).
static void light_compose(float hour, float yaw, float rain, float snow, float indoor, int style, float amt, float floor_,
                          float p[16]) {
    float d[3], sun[3], top[3], bot[3];
    light_sun_dir(hour, yaw, d);
    light_sun_rgb(hour, sun);
    // rain: greyer (towards its own luminance) and sun x(1 - 0.5 rain); snow: cooler
    float lum = 0.3f * sun[0] + 0.59f * sun[1] + 0.11f * sun[2];
    for (int k = 0; k < 3; k++) {
        sun[k] += (lum - sun[k]) * 0.6f * rain;
        sun[k] *= 1.0f - 0.5f * rain;
    }
    sun[0] *= 1.0f - 0.10f * snow, sun[1] *= 1.0f - 0.03f * snow, sun[2] *= 1.0f + 0.10f * snow;
    light_amb(sun, top, bot);
    // indoor: no time of day; fixed neutral light from above-front (+z toward the camera side is a GUESS)
    static const float IS[3] = {1.0f, 0.97f, 0.92f}, IT[3] = {0.85f, 0.87f, 0.92f};
    float idn[3] = {0.25f, 0.80f, 0.55f};
    light_norm3(idn);
    for (int k = 0; k < 3; k++) {
        d[k] += (idn[k] - d[k]) * indoor;
        sun[k] += (IS[k] - sun[k]) * indoor;
        top[k] += (IT[k] - top[k]) * indoor;
        bot[k] += (IT[k] * 0.6f - bot[k]) * indoor;
    }
    light_norm3(d);
    p[0] = d[0], p[1] = d[1], p[2] = d[2], p[3] = (float)style;
    p[4] = sun[0], p[5] = sun[1], p[6] = sun[2], p[7] = amt;
    p[8] = top[0], p[9] = top[1], p[10] = top[2], p[11] = floor_;
    p[12] = bot[0], p[13] = bot[1], p[14] = bot[2], p[15] = 0.0f;  // shadow 0 until step b/c
}

// ---- (s123 a2) BotW's own light words -> the 16-float block. c4 = the first 144 floats of the sun pass's fragment cbuf 4
// (exe export sora_botw_light; layout: recon/S123_LPROBE_READ.md). Overwrites sun dir, sun rgb, ambient top / bottom in p
// (style, amount, floor, shadow stay as light_compose set them). False (p untouched) if a word is not finite or the sun is below
// the horizon (then a1's moon light stays: GUESS). sunk / ambk = multipliers (live keys lsunk / lambk, percent / 100).
//   sun dir toward the sun = -c4[92..94] (HIGH), sun rgb = c4[20..22] * sunk (HDR, GUESS), ambient top = c4[40..42] * ambk (GUESS)
static bool light_from_botw(const float *c4, float sunk, float ambk, float floor_, float p[16]) {
    static const unsigned char used[] = {19, 20, 21, 22, 40, 41, 42, 92, 93, 94};  // (review: only the words read; unknown ones may be inf)
    for (int i = 0; i < (int)sizeof used; i++) if (!isfinite(c4[used[i]])) return false;
    float d[3] = {-c4[92], -c4[93], -c4[94]};
    if (d[1] < 0.0f) return false;  // sun below the horizon: keep a1's moon (GUESS)
    float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (l < 0.5f || l > 2.0f) return false;  // not a unit light direction: do not trust the block
    p[0] = d[0] / l, p[1] = d[1] / l, p[2] = d[2] / l;
    for (int k = 0; k < 3; k++) {
        p[4 + k] = c4[20 + k] * sunk;
        p[8 + k] = c4[40 + k] * ambk;
        p[12 + k] = p[8 + k] * floor_;
    }
    return true;
}

#ifndef LIGHT_TEST_ONLY
// ---- runtime part (needs sora.c's LOG and windows.h) ----
static int g_light = 1;  // (s121 user: style 1 picked -> light on by default; set light 0 = off)
static int g_lightstyle = 1, g_lightamt = 70, g_lightfloor = 65, g_lighthour = -1, g_lighthm = -1;
static int g_lightyaw = 0, g_lightlog = 0;
// (s123 a2) lightsrc 1 = BotW's real sun words when fresh, else a1 (default); 0 = a1 only. lsunk: sun rgb = HDR word * pct/100
// (GUESS 18: BotW 6.0 intensity -> ~1.1, a1's noon 1.05); lambk: ambient top = word * pct/100 (GUESS 140: dawn word ~0.55 -> ~0.77, a1 ambient ~0.8).
static int g_lightsrc = 1, g_lsunk = 18, g_lambk = 140;
// (s124 b) BotW's sun shadow on Sora (exe export sora_draw_sun_shadow, sora_draw.c): shmode 0 off / 1 one lookup at the chest
// (b0, default) / 2 per pixel / 3, 4 = 1, 2 with debug colours (red shade, green sun, blue outside the cascade). shstr <pct>
// (100), shrad <cm> chest disc (30), shbias <mm> (30), shchest <cm> above the feet (100), shoff <cm> toward the sun per pixel
// (5), shprad <mm> per-pixel disc (20), shflags -1 auto / bits 1 flip y, 2 depth 0..1, 4 greater-is-nearer. ALL GUESS.
static int g_shmode = 2, g_shstr = 100, g_shrad = 30, g_shbias = 30, g_shchest = 100, g_shoff = 5, g_shprad = 20, g_shflags = -1;
// (s124b) the shade's own colour: shdark <pct> brightness of the shade in shadow (60, GUESS), shcool <pct> toward BotW's sky
// hue (60, GUESS; hue = sun-pass words +100..102 'sky irradiance', S123 probe GUESS, else a cool default).
static int g_shdark = 60, g_shcool = 60;
static float g_shsky[3] = {0.79f, 0.84f, 1.0f};
// (s124d user: in the sun his skin looks white) lsunmax <pct>: the sun colour's brightest channel is capped here (BotW's HDR
// sun x lsunk reached 1.4..1.6 = bleached skin; KH2's own colours = 1.0). The floor is raised by the same factor so the
// shade (shader: ambient hue x floor x sun max) and the in-shadow colour B stay exactly as before. 0 = no cap. GUESS 105.
// lwarm <pct>: warmer sun (r x (1 + w), b x (1 - w)) for a tanner skin. GUESS 0.
static int g_lsunmax = 105, g_lwarm = 0;
// (s125 c) Sora's own shadow cast into BotW's sun cascades (exe sora_draw_cast, needs the s125 exe): shcast 0 off / 1 every
// actor (default) / 2 Sora only / 3 every actor + they shade themselves (no receive copy); shcastl <mask> cascade layers (7 = all three; 1 = the near one only); shcastp <mm> casters
// pushed away from the sun (0, GUESS: raise it if Sora's arms leave stripes on his own body). Sent with the shadow block.
static int g_shcast = 1, g_shcastl = 7, g_shcastp = 0;
// (s126) lflat 1 (default) = a vertex whose smooth normal cancelled (two-sided cards: Roxas' hair spikes, the keychain) is lit
// by its triangle's own facing; 0 = the old unlit look (stayed day-bright at night). Needs the s126 exe (sent with the shadow block).
static int g_lflat = 1;
// (s126) the KH2 blob under Sora + party: blob 0 off / 1 always (the old look) / 2 auto (default, GUESS) = only while BotW
// draws no sun / moon shadow map (exe vk_sun_cascade_age > 2 frames: caves under rock still have one, shrines / menus GUESS)
// or the cast shadow is off (shcast 0); else the actors' own cast shadow is the shadow, like Link's. Eased ~0.25 s.
static int g_blob = 2;
static float g_blob_k = 1;
static void blob_tick(void) {
    static long long (*age)(void);
    static int tried, was = -1;
    if (!tried) {
        tried = 1;
        age = (long long (*)(void))(void *)GetProcAddress(GetModuleHandleA(NULL), "vk_sun_cascade_age");
        LOG("[light] vk_sun_cascade_age %s\n", age ? "found" : "not in this exe (blob 2 acts as 1 until the s126 exe)");
    }
    long long a = age ? age() : -1;
    bool want = g_blob == 1 || (g_blob == 2 && (!age || !g_shcast || a < 0 || a > 2));
    if ((int)want != was) {
        LOG("[light] blob %s (mode %d, shadow map age %lld, shcast %d)\n", want ? "on" : "off", g_blob, a, g_shcast);
        was = want;
    }
    const float t = want ? 1.0f : 0.0f, st = 1.0f / 15.0f;
    g_blob_k = t > g_blob_k ? g_blob_k + fminf(st, t - g_blob_k) : g_blob_k - fminf(st, g_blob_k - t);
}
static float blob_str(float s) { return s * g_blob_k; }
static void light_shadow_send(int on);

static bool light_set_key(const char *key, int v) {
    if (!strcmp(key, "light")) g_light = v != 0;
    else if (!strcmp(key, "lightstyle")) g_lightstyle = v < 1 ? 1 : v > 3 ? 3 : v;
    else if (!strcmp(key, "lightamt")) g_lightamt = v;
    else if (!strcmp(key, "lightfloor")) g_lightfloor = v;
    else if (!strcmp(key, "lighthour")) g_lighthour = v, g_lighthm = -1;  // -1 = real time
    else if (!strcmp(key, "lighthm")) g_lighthm = v;                       // tenths of an hour, -1 = off
    else if (!strcmp(key, "lightyaw")) g_lightyaw = v;
    else if (!strcmp(key, "lightlog")) g_lightlog = v != 0;
    else if (!strcmp(key, "lightsrc")) g_lightsrc = v != 0;
    else if (!strcmp(key, "lsunk")) g_lsunk = v;
    else if (!strcmp(key, "lambk")) g_lambk = v;
    else if (!strcmp(key, "shmode")) g_shmode = v < 0 ? 0 : v > 4 ? 4 : v;
    else if (!strcmp(key, "shstr")) g_shstr = v < 0 ? 0 : v > 100 ? 100 : v;
    else if (!strcmp(key, "shrad")) g_shrad = v < 0 ? 0 : v;
    else if (!strcmp(key, "shbias")) g_shbias = v;
    else if (!strcmp(key, "shchest")) g_shchest = v;
    else if (!strcmp(key, "shoff")) g_shoff = v;
    else if (!strcmp(key, "shprad")) g_shprad = v < 0 ? 0 : v;
    else if (!strcmp(key, "shflags")) g_shflags = v < -1 ? -1 : v > 7 ? 7 : v;
    else if (!strcmp(key, "shdark")) g_shdark = v < 0 ? 0 : v > 200 ? 200 : v;
    else if (!strcmp(key, "shcool")) g_shcool = v < 0 ? 0 : v > 100 ? 100 : v;
    else if (!strcmp(key, "shcast") || !strcmp(key, "shcastl") || !strcmp(key, "shcastp")) {
        if (key[6] == 0) g_shcast = v < 0 ? 0 : v > 3 ? 3 : v;
        else if (key[6] == 'l') g_shcastl = v & 7;
        else g_shcastp = v;
        light_shadow_send(g_light);  // (also while the light is off: the cast does not need it)
    }
    else if (!strcmp(key, "lflat")) g_lflat = v != 0, light_shadow_send(g_light);
    else if (!strcmp(key, "blob")) g_blob = v < 0 ? 0 : v > 2 ? 2 : v;
    else if (!strcmp(key, "lsunmax")) g_lsunmax = v < 0 ? 0 : v;
    else if (!strcmp(key, "lwarm")) g_lwarm = v < -50 ? -50 : v > 50 ? 50 : v;
    else if (!strcmp(key, "lprobe")) {  // (s121) the exe's log-only light probe (exec.c "[lprobe]", every 60th frame while on)
        void (*fn)(int) = (void (*)(int))(void *)GetProcAddress(GetModuleHandleA(NULL), "vk_light_probe");
        if (fn) fn(v != 0);
        else LOG("[light] lprobe: vk_light_probe not in this exe (until the next exe link)\n");
    } else return false;
    LOG("[light] live: %s %d\n", key, v);
    return true;
}

static float light_ease(float cur, float tgt, float k) {
    return tgt > cur ? cur + fminf(k, tgt - cur) : cur - fminf(k, cur - tgt);
}

// (review) sora_botw_light returns 0 when its seqlock read lands on the render thread's copy (4 quick tries): keep the last
// good copy (same frame number, so the age check still ages it out) instead of a one-frame a1 pop.
static int light_botw_get(int (*botw)(float *, int, float *, int, unsigned long long *), float c4[144], float c3[256],
                          unsigned long long *bf) {
    static float last4[144];
    static unsigned long long last_bf;
    static int have;
    if (botw(c4, 144, c3, 256, bf)) {
        memcpy(last4, c4, sizeof last4), last_bf = *bf, have = 1;
        return 1;
    }
    if (!have) return 0;
    memcpy(c4, last4, sizeof last4), *bf = last_bf;
    return 1;
}

// (s124 b) the sun shadow block for the exe (mode 0 while the light is off). Looked up once; an exe before s124 lacks it.
static void light_shadow_send(int on) {
    static void (*fn)(const float *, int);
    static int tried;
    if (!tried) {
        tried = 1;
        fn = (void (*)(const float *, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_sun_shadow");
        LOG("[light] sora_draw_sun_shadow %s\n", fn ? "found" : "not in this exe (no sun shadow until the next exe link)");
    }
    if (!fn) return;
    float q[17] = {on ? (float)g_shmode : 0, g_shstr / 100.0f, g_shrad / 100.0f, g_shbias / 1000.0f, g_shchest / 100.0f,
                   g_shoff / 100.0f, g_shprad / 1000.0f, (float)g_shflags, g_shsky[0], g_shsky[1], g_shsky[2],
                   g_shdark / 100.0f, g_shcool / 100.0f, (float)g_shcast, (float)g_shcastl, g_shcastp / 1000.0f,
                   (float)g_lflat};
    fn(q, 17);  // (an exe before s125 reads the first 13, before s126 the first 16)
}

// per frame (sora.c's frame driver). Looked up by name once: an exe linked before s121 lacks both exports.
static void light_tick(void) {
    static void (*draw)(const float *, int);
    static int (*botw)(float *, int, float *, int, unsigned long long *);
    static bool (*env)(float *);
    static int tried, was_on, frame;
    static float rain, snow, indoor, last_tod = 180.0f;
    if (!tried) {
        tried = 1;
        HMODULE m = GetModuleHandleA(NULL);
        draw = (void (*)(const float *, int))(void *)GetProcAddress(m, "sora_draw_light");
        env = (bool (*)(float *))(void *)GetProcAddress(m, "sora_world_env");
        botw = (int (*)(float *, int, float *, int, unsigned long long *))(void *)GetProcAddress(m, "sora_botw_light");
        if (!botw) LOG("[light] sora_botw_light not in this exe (a1 hour table until the next exe link)\n");
        LOG("[light] sora_draw_light %s, sora_world_env %s\n", draw ? "found" : "not in this exe (no light until the next exe link)",
            env ? "found" : "not in this exe (no time/weather until the next exe link)");
        // (s121 review) after a hot reload the exe still holds the old DLL's light: start from off (style 0) like the key
        if (draw) { float z[16] = {0}; draw(z, 16); }
    }
    if (!g_light) {
        if (was_on) {  // turned off: style 0 once, all 16 floats
            float z[16] = {0};
            if (draw) draw(z, 16);
            light_shadow_send(0);
            was_on = 0, frame = 0;
            LOG("[light] off\n");
        }
        return;
    }
    // world state. e[]: time of day 0..360, stage type (2 = Indoor/shrine: GUESS), weather byte, climate.
    float e[4] = {last_tod, 0, 0, 0};
    bool ok = env && env(e);
    if (!ok) e[0] = last_tod, e[1] = 0, e[2] = 0;
    else last_tod = e[0];
    int stage = (int)(e[1] + 0.5f), weather = (int)(e[2] + 0.5f);
    float hour = e[0] / 15.0f;
    if (g_lighthm >= 0) hour = g_lighthm / 10.0f;
    else if (g_lighthour >= 0) hour = (float)g_lighthour;
    float rt, st, in = (ok && stage == 2) ? 1.0f : 0.0f;
    light_weather(weather, &rt, &st);
    const float k = 1.0f / 60.0f;  // ~1 s blend at 60 fps
    if (frame == 0) rain = rt, snow = st, indoor = in;  // first frame on: no blend from stale values
    else rain = light_ease(rain, rt, k), snow = light_ease(snow, st, k), indoor = light_ease(indoor, in, k);
    float p[16];
    light_compose(hour, (float)g_lightyaw, rain, snow, indoor, g_lightstyle, g_lightamt / 100.0f, g_lightfloor / 100.0f, p);
    // (s123 a2) BotW's real sun + ambient when the exe has a fresh copy of its sun-pass words (age <= 10 frames)
    const char *why = "lightsrc 0";
    int usedbotw = 0;
    unsigned long long bf = 0;
    float c4[144], c3[256];
    if (!g_lightsrc) why = "lightsrc 0";
    else if (!botw) why = "no sora_botw_light export";
    else if (!light_botw_get(botw, c4, c3, &bf)) why = "no snapshot yet";
    else {
        // age = DLL ticks (one per game frame) since the exe's snapshot frame number last changed
        static unsigned long long last_bf, tick_at_bf, tick;
        tick++;
        if (bf != last_bf) last_bf = bf, tick_at_bf = tick;
        unsigned long long age = tick - tick_at_bf;
        if (age > 10) why = "snapshot stale";
        else if (light_from_botw(c4, g_lsunk / 100.0f, g_lambk / 100.0f, g_lightfloor / 100.0f, p)) usedbotw = 1, why = "";
        else why = "sun below horizon or bad words (a1 moon)";
        if (usedbotw && isfinite(c4[100]) && isfinite(c4[101]) && isfinite(c4[102])) {  // (s124b) sky hue, max = 1
            float mx = fmaxf(c4[100], fmaxf(c4[101], c4[102]));
            if (mx > 1e-3f && fminf(c4[100], fminf(c4[101], c4[102])) >= 0)
                for (int k = 0; k < 3; k++) g_shsky[k] = g_shsky[k] + (c4[100 + k] / mx - g_shsky[k]) * 0.05f;
        }
        if (usedbotw && (!was_on || frame % 600 == 0))
            LOG("[light] src botw age %llu sun %.2f %.2f %.2f rgb %.2f %.2f %.2f amb %.2f %.2f %.2f (raw intensity %.2f, ambient word %.2f %.2f %.2f)\n", age,
                p[0], p[1], p[2], p[4], p[5], p[6], p[8], p[9], p[10], c4[19], c4[40], c4[41], c4[42]);
    }
    if (!usedbotw && (!was_on || frame % 600 == 0)) LOG("[light] src a1 (%s)\n", why);
    {   // (s124d) sun cap + warmth; the floor compensates so the shade keeps its brightness
        float mx = fmaxf(p[4], fmaxf(p[5], p[6])), cap = g_lsunmax / 100.0f;
        if (cap > 0 && mx > cap) {
            float k = cap / mx;
            p[4] *= k, p[5] *= k, p[6] *= k, p[11] /= k;
        }
        float w = g_lwarm / 100.0f;
        p[4] *= 1 + w, p[6] *= 1 - w;
        if (!was_on || frame % 600 == 0)
            LOG("[light] sun after cap %d warm %d: rgb %.2f %.2f %.2f floor %.2f\n", g_lsunmax, g_lwarm, p[4], p[5], p[6], p[11]);
    }
    if (draw) draw(p, 16);
    light_shadow_send(1);
    int every = g_lightlog ? 60 : 600;
    if (!was_on || frame % every == 0)
        LOG("[light] %s hour %.2f stage %d weather %d (raw) ok %d rain %.2f snow %.2f indoor %.2f sun dir %.2f %.2f %.2f rgb %.2f %.2f %.2f style %d amount %.2f floor %.2f\n",
            was_on ? "on" : "ON", hour, stage, weather, ok, rain, snow, indoor, p[0], p[1], p[2], p[4], p[5], p[6], g_lightstyle, p[7], p[11]);
    was_on = 1, frame++;
}
#endif

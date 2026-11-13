// sora_world.h (s191, DLL side, hot-reloadable; Melty spec 4.4 "room clock"): BotW's weather and Blood Moons for the DLL.
//   sora_world_weather()            the weather type BotW shows now (0..8), -1 = not known (no world)
//   sora_world_weather_force(t)     force that weather (t < 0 releases); false = could not (bad type / no world)
//   sora_world_bloodmoon_count()    Blood Moons this game has played (+1 each time one plays), -1 = not known yet
//   sora_world_bloodmoon_play()     make the real Blood Moon play now (BotW's own midnight path); false = could not
//   (s191 task 2, the room clock) sora_world_bloodmoon_busy() one of ours is on its way (no hour holds then: a hold would
//   cancel the 24:00 push); sora_world_open() 1 = open world (stage 1); sora_world_climate() the climate id the game's
//   weather getter uses (world::Manager +0x780: the index into its climate table, 0x600 bytes each); sora_world_clock_gate()
//   / _put() TimeMgr +0x14e, the paraglider clock gate (0 = the game re-reads its flag each update and stops at 11:00).
// Live: set wxforce <0..8> / -1 (release), set bmplay 1 (one shot: remove the line again), set worldlog 0|1.
// A "[world]" line: weather + count + hour, on change, at most one every 10 s.
//
// How it is found (1.9.0 main, Ghidra base 0x7100000000 = runtime 0x7000000000 + 0x100000000; recon/s191/S191_WORLD.md,
// decompiles recon/ghidra/out/s191_*.c). No pinned address: the world::Manager pointer is the exe's game_sites row
// "world_mgr" and the event manager is "event_mgr" (sw_api->site), the gdt::Manager pointer is sora_sites()->gdt_mgr.
//   world::Manager  +0x5c0 u32 manager count, +0x5c8 mMgrs: [0] TimeMgr, [3] WeatherMgr
//     +0x6b8 u32 stage type (1 = open world), +0x7c0 / +0x7c4 u32 (both 1 in the normal overworld: GUESS what they are),
//     +0x798 s32 a second weather override (>= 0: 0 -> 7, 2 -> 5, else 0; GUESS: the Divine Beast approaches),
//     +0x7d1 u8 the manual weather request (0xff = none), +0x794 s32 its frame timer (the game's setter 0x710143162c writes 4;
//     Manager::calc 0x710143580c counts it down and puts 0xff back at 0), +0x7d5 u8 (setter arg 3), +0x7d6 u8 "for an event"
//     (an event's own weather: we never overwrite it), +0x7d9 u8 (0 = no Blood Moon here).
//     The game's weather getter = 0x7101431c3c (getWeatherType(+0x780)): weather_now() below is the same branches.
//   WeatherMgr +0x28 u8 = the climate's current weather (what the getter returns in the overworld with no request).
//   TimeMgr (vtable 0x7101d50e08, calc 0x7101426054): +0xb8 f32 time 0..360 (= WM_Time), +0xc8 f32 Blood Moon timer
//     (WM_BloodyMoonTimer, +speed*dt a frame, 2520 = 7 days), +0xd4 u32 WM_Time flag handle, +0x11c WM_BloodyDay handle,
//     +0x13c s32 day count (WM_NumberOfDays), +0x144 s32 bloodyEndReserveTimer, +0x149 u8 fixed-time mode (0 = clock runs),
//     +0x14a u8 Blood Moon force (1: time 23:29 + WM_BloodyDay + timer 0, then 0 again; 2: the same at 21:30; 99 = an event's
//     "Blood Moon midnight"), +0x14d u8 the intro played (IsPlayed_Demo103_0), +0x152 u8 bloody-day copy.
//     Midnight: calc adds speed*dt to the time; at >= 360 it calls the new-day function 0x7101425e98(this, 0): days + 1, and if
//     it is a Blood Moon night (WM_BloodyDay, or +0x14a, or +0x152 before 1:00) and not prohibited (castle area, final boss,
//     Divine Beast battles, BloodyMoonProhibition, stage != 1 or +0x7d9 == 0) it calls the event "Demo011_0" (the Blood Moon
//     cutscene, 0x7101425c5c) and resets the timer.
// bloodmoon_play = the game's own route: +0x14a = 1 (the game itself sets WM_BloodyDay), then WM_Time = 360 in both gdt
// buffers (as runtime/services/test_warp.c writes the hour) so the next TimeMgr::calc crosses midnight even with the clock
// held at speed 0, and the new-day function plays Demo011_0 as at a real midnight.
// The count: the event "Demo011_0" starting (evt::Manager +0x1d2b8 active event, its name at +0x68), or the new-day
// signature (days + 1 with the timer back at 0 and the end-reserve timer just set to 150), one count per 60 s. It lives in
// this process's environment (SORA_WORLD_BMCOUNT) so a hot reload keeps it; a game start begins at 0. Not in the save.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "sw_api.h"

#define WX_TYPES 9
static const char *const WX_NAME[WX_TYPES] = {"Bluesky", "Cloudy", "Rain", "HeavyRain", "Snow", "HeavySnow", "ThunderStorm", "ThunderRain", "BlueskyRain"};

static struct {
    int bound;
    uint64_t wm_site, ev_site;  // pointer sites (guest addresses holding the manager pointers)
    int force;                  // the forced weather type, -1 = none
    int count;                  // Blood Moons played, -1 = not known yet
    int bm;                     // bloodmoon_play state (0 idle)
    uint64_t bm_t0;
    int bm_ticks;
    uint64_t ev_last;           // the Demo011_0 event object last counted
    uint64_t last_play_ms;      // the last counted Blood Moon (de-dupe of the two signals)
    int32_t days_was;
    int log_on;
    int lw, lc, lf, lcl;        // last logged weather / count / force / climate id
    uint64_t log_next;
} WORLD = {.force = -1, .count = -1, .log_on = 1, .lw = -2, .lc = -2, .lf = -2, .lcl = -2, .days_was = -1};

static bool w_rd(uint64_t a, void *out, size_t n) {
    if (!a || !rd_ok(a, n)) return false;
    memcpy(out, (const void *)(uintptr_t)a, n);
    return true;
}
static uint64_t w_ptr(uint64_t a) {
    uint64_t p = 0;
    return w_rd(a, &p, 8) ? p : 0;
}

static void world_bind(void) {
    if (WORLD.bound) return;
    WORLD.bound = 1;
    const SwApi *(*get)(uint32_t) = (const SwApi *(*)(uint32_t))(void *)GetProcAddress(GetModuleHandleA(NULL), SW_API_EXPORT);
    const SwApi *api = get ? get(1) : NULL;
    if (SW_HAS(api, site)) WORLD.wm_site = api->site("world_mgr"), WORLD.ev_site = api->site("event_mgr");
    char b[16];
    DWORD n = GetEnvironmentVariableA("SORA_WORLD_BMCOUNT", b, sizeof b);
    if (n > 0 && n < sizeof b) WORLD.count = atoi(b);  // (kept across a hot reload)
    LOG("[world] world::Manager site %#llx, event manager site %#llx%s%s\n", (unsigned long long)WORLD.wm_site,
        (unsigned long long)WORLD.ev_site, api ? "" : " (no sw_api in this exe: no weather / Blood Moon)",
        WORLD.count >= 0 ? " (Blood Moon count kept from before the reload)" : "");
}

// The world::Manager, NULL-ish (0) on the title screen / before the world exists.
static uint64_t world_mgr(void) {
    world_bind();
    uint64_t wm = w_ptr(WORLD.wm_site);
    return wm && rd_ok(wm, 0x800) ? wm : 0;
}
// mMgrs[i] (0 TimeMgr, 3 WeatherMgr) checked readable for n bytes.
static uint64_t world_sub(uint64_t wm, int i, size_t n) {
    uint32_t cnt = 0;
    if (!wm || !w_rd(wm + 0x5c0, &cnt, 4) || cnt <= (uint32_t)i || cnt > 64) return 0;
    uint64_t m = w_ptr(w_ptr(wm + 0x5c8) + 8ull * i);
    return m && rd_ok(m, n) ? m : 0;
}
static uint64_t world_time_mgr(uint64_t wm) {
    uint64_t tm = world_sub(wm, 0, 0x160);
    float t;
    return tm && w_rd(tm + 0xb8, &t, 4) && t >= 0 && t <= 361 ? tm : 0;  // (a sane hour, or it is not the TimeMgr)
}

#define WM_U8(wm, o) (*(volatile uint8_t *)(uintptr_t)((wm) + (o)))
#define WM_I32(wm, o) (*(volatile int32_t *)(uintptr_t)((wm) + (o)))

// The same branches as the game's getter 0x7101431c3c.
static int weather_now(uint64_t wm) {
    uint64_t wx = world_sub(wm, 3, 0x30);
    if (!wm || !wx) return -1;
    int32_t stage = WM_I32(wm, 0x6b8), o798 = WM_I32(wm, 0x798);
    int req = WM_U8(wm, 0x7d1), live = *(volatile uint8_t *)(uintptr_t)(wx + 0x28), w;
    if (o798 >= 0) w = o798 == 0 ? 7 : o798 == 2 ? 5 : 0;
    else if (req <= 8) w = req;
    else if (stage != 1) w = 0;  // (a shrine / indoors: the getter gives Bluesky without a request)
    else w = live;               // (overworld; with +0x7c0 / +0x7c4 != 1 the getter picks from the climate table: +0x28 is GUESS there)
    return w >= 0 && w < WX_TYPES ? w : -1;
}

static int sora_world_weather(void) { return weather_now(world_mgr()); }

// (s191 task 2) for the room clock (sora_clock.h)
static bool sora_world_bloodmoon_busy(void) { return WORLD.bm != 0; }
static int sora_world_open(void) {
    uint64_t wm = world_mgr();
    return wm ? WM_I32(wm, 0x6b8) == 1 : -1;
}
static int sora_world_climate(void) {  // -1 = not known / not in the open world
    uint64_t wm = world_mgr();
    if (!wm || WM_I32(wm, 0x6b8) != 1) return -1;
    int32_t c = WM_I32(wm, 0x780);
    return c >= 0 && c < 0xff ? c : -1;
}
static int sora_world_clock_gate(void) {
    uint64_t tm = world_time_mgr(world_mgr());
    return tm ? WM_U8(tm, 0x14e) : -1;
}
static bool sora_world_clock_gate_put(int v) {
    uint64_t tm = world_time_mgr(world_mgr());
    if (!tm) return false;
    WM_U8(tm, 0x14e) = (uint8_t)v;
    return true;
}

static bool sora_world_weather_force(int type) {
    if (type < 0) {
        if (WORLD.force >= 0) LOG("[world] weather force released (was %d %s): BotW's own weather comes back\n", WORLD.force, WX_NAME[WORLD.force]);
        WORLD.force = -1;  // (the game's Manager::calc clears its request 4 frames later)
        return true;
    }
    if (type >= WX_TYPES || !world_sub(world_mgr(), 3, 0x30)) return false;
    if (WORLD.force != type) LOG("[world] weather forced to %d %s\n", type, WX_NAME[type]);
    WORLD.force = type;
    return true;
}

static void world_count_set(int c) {
    WORLD.count = c;
    char b[16];
    snprintf(b, sizeof b, "%d", c);
    SetEnvironmentVariableA("SORA_WORLD_BMCOUNT", b);
}

static int sora_world_bloodmoon_count(void) {
    if (WORLD.count < 0 && world_time_mgr(world_mgr())) world_count_set(0);
    return WORLD.count;
}

static uint32_t w_crc32(const char *s) {
    uint32_t c = 0xFFFFFFFFu;
    for (; *s; s++) {
        c ^= (uint8_t)*s;
        for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xEDB88320u & -(c & 1));
    }
    return ~c;
}

// WM_Time in both gdt buffers (the pending-write proxy and the current-read primary): test_warp.c time_flags, DLL copy.
static bool world_write_time(uint64_t tm, float tod) {
    const SoraSites *ss = sora_sites();
    uint64_t mgr = ss && ss->gdt_mgr ? w_ptr(ss->gdt_mgr) : 0;
    uint32_t handle;
    if (!mgr || !w_rd(tm + 0xd4, &handle, 4) || handle == UINT32_MAX) return false;
    handle &= 0xffffff;
    uint64_t at[2];
    for (int i = 0; i < 2; i++) {
        uint64_t buf = w_ptr(w_ptr(mgr + 0xbd8 + 8 * i));
        uint32_t cnt, code[2], hash;
        if (!buf || !w_rd(buf + 0x28, &cnt, 4) || handle >= cnt) return false;
        uint64_t flag = w_ptr(w_ptr(buf + 0x30) + 8ull * handle), primary = flag, off;
        uint64_t getter = w_ptr(w_ptr(flag) + 0x88);
        if (!getter || !w_rd(getter, code, 8) || code[1] != 0xd65f03c0) return false;
        if (code[0] == 0xbd401800) off = 0x18;                                  // ldr s0, [x0, #0x18]; ret
        else if (code[0] == 0xbd401000) off = 0x10, primary = w_ptr(flag + 8);  // ldr s0, [x0, #0x10]; ret
        else return false;
        if (!w_rd(primary + 0x1c, &hash, 4) || hash != w_crc32("WM_Time") || !rd_ok(flag + off, 4)) return false;
        at[i] = flag + off;
    }
    for (int i = 0; i < 2; i++) *(volatile float *)(uintptr_t)at[i] = tod;
    return true;
}

static bool sora_world_bloodmoon_play(void) {
    uint64_t wm = world_mgr(), tm = world_time_mgr(wm);
    const char *why = !tm ? "no world / TimeMgr" : WORLD.bm ? "one is already on its way"
                    : WM_U8(tm, 0x149) ? "the clock is in a fixed-time mode here" : !WM_U8(tm, 0x14d) ? "the intro is not played yet"
                    : WM_I32(wm, 0x6b8) != 1 || !WM_U8(wm, 0x7d9) ? "not in the open world (no Blood Moon here)" : NULL;
    if (why) { LOG("[world] Blood Moon: could not (%s)\n", why); return false; }
    sora_world_bloodmoon_count();
    WORLD.bm = 1, WORLD.bm_t0 = GetTickCount64(), WORLD.bm_ticks = 0;
    LOG("[world] Blood Moon requested (count %d, hour %.2f)\n", WORLD.count, *(volatile float *)(uintptr_t)(tm + 0xb8) / 15);
    return true;
}

static void world_played(const char *how) {
    uint64_t now = GetTickCount64();
    if (WORLD.last_play_ms && now - WORLD.last_play_ms < 60000) return;  // (the other signal of the same Blood Moon)
    WORLD.last_play_ms = now;
    world_count_set((WORLD.count < 0 ? 0 : WORLD.count) + 1);
    LOG("[world] Blood Moon played (%s): count %d\n", how, WORLD.count);
    if (WORLD.bm) WORLD.bm = 0, LOG("[world] Blood Moon request done in %llu ms\n", (unsigned long long)(now - WORLD.bm_t0));
}

// The active event's name ("" = none / unreadable)
static const char *world_event(uint64_t *ev_out) {
    uint64_t evt = w_ptr(WORLD.ev_site), ev = evt ? w_ptr(evt + 0x1d2b8) : 0, s = ev ? w_ptr(ev + 0x68) : 0;
    *ev_out = ev;
    if (!s || !rd_ok(s, 32)) return "";
    const char *p = (const char *)(uintptr_t)s;
    int n = 0;
    while (n < 32 && p[n] >= 0x20 && p[n] < 0x7f) n++;
    return n && n < 32 && !p[n] ? p : "";
}

// Every frame (sora_mod_frame, Sora mode on): the weather request, the Blood Moon steps and counting, the [world] line.
static void world_tick(void) {
    uint64_t wm = world_mgr(), tm = world_time_mgr(wm), now = GetTickCount64();
    if (!tm) { WORLD.days_was = -1; return; }
    sora_world_bloodmoon_count();
    // weather: the same fields the game's own setter writes, refreshed before its 4-frame timer runs out
    // (s191 task 2: never outside the open world; a guest's room clock also releases it there)
    if (WORLD.force >= 0 && !WM_U8(wm, 0x7d6) && WM_I32(wm, 0x6b8) == 1) WM_U8(wm, 0x7d1) = (uint8_t)WORLD.force, WM_I32(wm, 0x794) = 4, WM_U8(wm, 0x7d5) = 0;
    // Blood Moon counting: the cutscene event, or the new-day signature
    uint64_t ev;
    const char *en = world_event(&ev);
    if (!strcmp(en, "Demo011_0")) {
        if (ev != WORLD.ev_last) WORLD.ev_last = ev, world_played("event Demo011_0");
    } else WORLD.ev_last = 0;
    int32_t days = WM_I32(tm, 0x13c), reserve = WM_I32(tm, 0x144);
    float timer = *(volatile float *)(uintptr_t)(tm + 0xc8);
    if (WORLD.days_was >= 0 && days == WORLD.days_was + 1 && timer < 1.0f && reserve > 100 && reserve <= 150) world_played("midnight: new day with the timer reset");
    WORLD.days_was = days;
    // Blood Moon request steps
    if (WORLD.bm == 1) WM_U8(tm, 0x14a) = 1, WORLD.bm = 2, WORLD.bm_t0 = now;  // (the game: 23:29, WM_BloodyDay, timer 0)
    else if (WORLD.bm == 2) {
        if (!WM_U8(tm, 0x14a)) WORLD.bm = 3, WORLD.bm_ticks = 0;
        else if (now - WORLD.bm_t0 > 3000) WORLD.bm = 0, WM_U8(tm, 0x14a) = 0, LOG("[world] Blood Moon: TimeMgr did not take the request (3 s)\n");
    } else if (WORLD.bm == 3 && ++WORLD.bm_ticks >= 10) {  // (a few frames for the gdt to publish WM_BloodyDay)
        bool ok = world_write_time(tm, 360.0f);
        LOG("[world] Blood Moon: hour pushed to 24:00 %s\n", ok ? "(the next TimeMgr calc crosses midnight)" : "FAILED (no WM_Time flags): it plays at the next real midnight");
        WORLD.bm = 4, WORLD.bm_t0 = now;
    } else if (WORLD.bm == 4 && now - WORLD.bm_t0 > 20000)
        WORLD.bm = 0, LOG("[world] Blood Moon: none played within 20 s (prohibited here? an event running?)\n");
    // the [world] line
    int w = weather_now(wm), clim = WM_I32(wm, 0x780);
    if (WORLD.log_on && (w != WORLD.lw || WORLD.count != WORLD.lc || WORLD.force != WORLD.lf || clim != WORLD.lcl) && now >= WORLD.log_next) {
        int bd = -1, (*gb)(const char *, int *) = (int (*)(const char *, int *))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_gdt_bool");
        if (gb && !gb("WM_BloodyDay", &bd)) bd = -1;
        uint64_t wx = world_sub(wm, 3, 0x30);
        LOG("[world] weather %d %s (request %d, climate %d), forced %d, Blood Moon count %d, hour %.2f, day %d, bloody day %d, timer %.0f, stage %d, climate id %d, gate %d\n",
            w, w >= 0 ? WX_NAME[w] : "?", WM_U8(wm, 0x7d1) == 0xff ? -1 : WM_U8(wm, 0x7d1),
            wx ? *(volatile uint8_t *)(uintptr_t)(wx + 0x28) : -1, WORLD.force, WORLD.count,
            *(volatile float *)(uintptr_t)(tm + 0xb8) / 15, days, bd, timer, WM_I32(wm, 0x6b8), clim, WM_U8(tm, 0x14e));
        WORLD.lw = w, WORLD.lc = WORLD.count, WORLD.lf = WORLD.force, WORLD.lcl = clim, WORLD.log_next = now + 10000;
    }
}

static bool world_set_key(const char *key, int v) {
    if (!strcmp(key, "wxforce")) {
        bool ok = sora_world_weather_force(v);
        LOG("[world] live: wxforce %d%s\n", v, ok ? "" : ": could not (type 0..8 / no world yet)");
    } else if (!strcmp(key, "bmplay")) {
        if (v) LOG("[world] live: bmplay -> %s\n", sora_world_bloodmoon_play() ? "requested" : "could not");
    } else if (!strcmp(key, "worldlog")) WORLD.log_on = v != 0, WORLD.lw = -2;
    else return false;
    return true;
}

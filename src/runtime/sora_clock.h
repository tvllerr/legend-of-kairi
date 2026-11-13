// sora_clock.h (s191, DLL side; Melty spec 4.4): one time of day, weather and Blood Moon count for everyone in a Melty room.
// The story key botw.clock:world is written by the room's leader (its game's clock runs as ever) and every guest holds its
// own game's hour where the room says. Two layers:
//   1. a pure core (Clock + ClockIO): no Windows / game / Melty call, everything comes through ClockIO's function
//      pointers, so a host test runs several cores against one fake story (runtime/test/clock_test.c);
//   2. the game binding (CK, below the SORA_CLOCK_TEST guard): the hour through the exe's exports test_warp_hour /
//      test_warp_hold_hour (runtime/services/test_warp.c; a missing export = the clock stays off), the shown hour through
//      sora_world_env, weather / climate / Blood Moon / the paraglider clock gate through sora_world.h (sora.c defines
//      SORA_WORLD_HOOKS), the room through sora_melty_*. The glue (sora_net.h net_clock_frame) drains the story and calls it.
//
// Wire value (little-endian, as Melty's connector test writes it: connector/test/standin.cpp):
//   f64 at (room clock ms), f64 hour (0..24 at `at`), f64 rate (hours per room ms; BotW = 1 / 60000), u8 weather (0xff =
//   not known / not in the open world: do not force), u8 Blood Moon count (wraps), u8 the leader's climate (world::Manager
//   +0x780, the index the game's weather getter uses into its climate table; 0xff = not known). 27 bytes; a reader takes
//   >= 24: 24 = weather 0xff, count not known; 26 = no climate. NaN / inf / hour outside 0..24 / |rate| > 1 hour a second =
//   a bad value: logged once, ignored.
// LEADER: writes at once when the hour is off what its last write predicts by > 0.05 h (a sleep, a time skip), when its
//   game's clock stops (menus, cutscenes: rate 0) or runs again (the normal rate), the weather / climate or the count
//   changes, and every 5 s; never more than 4 writes a second. Never holds its own clock. Weather + climate = 0xff outside
//   the open world (a shrine reads "0 Bluesky": never sent as the room's weather). Its game playing a Blood Moon = count + 1
//   and story_clear("botw.dead:") (spec 4.3).
// GUEST: holds its hour at hour + (now - at) * rate (mod 24), re-held only when that moved >= 1 game minute (so once a
//   real second at the normal rate), or when the game lost / changed the hold. No hold while its Blood Moon plays (the
//   hold would cancel the game's own 24:00 push); held again at once when it is done. WEATHER IS PER REGION in BotW:
//   "set mclockwx" 1 (default) = the room's weather only while this game is in the open world AND (no climate byte, or the
//   same climate as the leader's), else BotW's own; 2 = in the open world whatever the climate (the spec's literal rule);
//   0 = never. Decided again once a second as the player walks. The first count after a join is adopted; each later
//   change owes ONE Blood Moon, tried every 5 s until the game takes it (shrines / Divine Beasts / events say no).
//   A held guest never gets its own midnight (speed 0; a 23.99 -> 0.01 step skips the game's new day: S191_WORLD.md), so its
//   own Blood Moon / day count never runs: nothing to block, the room's count is the only one it plays.
// NEW LEADER (s198, "set mclocktake 1" default; 0 = the s191 behaviour): Melty hands the lead to the room host's game the
//   moment it is back (kit story.ts chooseLeader, why BACK), and a joiner gets the room's whole story WITH who leads in one
//   snapshot (melty_net.h STORY_S_SNAPSHOT fills storyGet's map, sets the leader, then onStorySnapshot = story_ready), so
//   once story_ready says 1 the room's copy (story_get) is complete: a stored clock is known, or there is none. A leader
//   (1) writes nothing while its game is not in the world (in_world 0: the title screen, a load; the guests run on from the
//   last value meanwhile); (2) once in the world, with a room value: TAKES it into its own game first (hour = the value
//   run on by the room time since its `at`, so an old value is advanced; hold + release = its own clock runs on from there;
//   the count goes on from the value's as before), then leads from there; (3) no value by its first write (a fresh room,
//   1 s after it is in the world): its own hour starts the room's clock, as ever. Weather is not taken (the leader's game
//   decides its own weather; the value carries no day count). The room does not echo the leader's own writes back
//   (server.ts: others()), so the leader keeps its last write as the room's value (a later guest role starts from it).
// LEADER LOADS A SAVE (s198 b, same key): BotW read a game_data.sav (sora_levels.h's exe fs event, kind 0: the title's
//   Continue, the Game Over's Continue, the pause menu's Load; 68 of 68 reads in 299 logs were one of those, none came with
//   an autosave or anything else) = the save's hour is coming. The binding reads the exe's fs_save_event ring itself, every
//   frame (its own sequence number; sora_levels.h's reader runs on driven frames only, so it can be late). From that read the leader writes nothing; once its game has
//   been out of the world (in_world 0 / no hour) and is back, it takes the room's clock again (its own last write run on by
//   the room time) instead of writing the save's hour. A read with no trip out of the world in CLOCK_RELOAD_MS = nothing
//   taken. A sleep (fire / bed / inn) reads no save, so its hour jump is written as ever. Fast travel reads none either (its
//   hour is the room's anyway).
// RELEASE (leaving the guest role: room gone, key off, became leader, DLL unloading): only what this module set. A hold
//   that was there before us (SAVAGEWIND_TEST_TIME, the control `time freeze` command) is left as it is. Every hold
//   (test_warp.c write_time) sets TimeMgr +0x14e = 1, the paraglider clock gate: while it is 0 the game re-reads it from
//   its flag each update and stops the clock at 11:00 (recon/ghidra/out/s191_tm_calc.c). It is read before our first
//   hold and a 0 is put back after our release, so the game reads its own flag again (pre-paraglider: the clock goes back
//   to its 11:00 stop, as vanilla).
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define CLOCK_KEY "botw.clock:world"
#define CLOCK_DEAD_PREFIX "botw.dead:"
#define CLOCK_WIRE_LEN 27
#define CLOCK_WIRE_MIN 24
#define CLOCK_RATE_NORMAL (1.0 / 60000.0)  // BotW: an in-game hour a real minute
#define CLOCK_RATE_MAX (1.0 / 1000.0)      // more than an hour a real second = a bad value
#define CLOCK_PERIOD_MS 5000.0             // the leader's regular write
#define CLOCK_GAP_MS 250.0                 // at most 4 writes a second
#define CLOCK_MEASURE_MS 1000.0            // the leader measures its game's clock rate over this
#define CLOCK_JUMP_H 0.05                  // hour off the last write's prediction by more = write now
#define CLOCK_STEP_H (1.0 / 60.0)          // a guest re-holds when its target moved one game minute
#define CLOCK_DRIFT_H (2.0 / 60.0)         // the game shows an hour this far off our hold = hold again (at most every 5 s)
#define CLOCK_DRIFT_MS 5000.0
#define CLOCK_BIG_H 0.25                   // a guest apply that jumps more is logged
#define CLOCK_PERIOD_LOG_MS 60000.0        // the 5 s writes are logged once a minute
#define CLOCK_WX_MS 1000.0                 // a guest decides its weather force again this often
#define CLOCK_OWED_MS 5000.0               // an owed Blood Moon is tried again this often
#define CLOCK_TAKE_MAX 3                   // (s198) a new leader sets the room's hour again if its game moved it before the first write
#define CLOCK_RELOAD_MS 30000.0            // (s198 b) a save read with no trip out of the world in this long: nothing to take

enum { CLOCK_OFF, CLOCK_GUEST, CLOCK_LEADER };

// weather / climate -1 = 0xff; moons -1 = not known (24 bytes)
typedef struct { double at, hour, rate; int weather, moons, climate; } ClockVal;

typedef struct {
    void *ctx;
    double (*now_ms)(void *ctx);                                         // the room clock
    int (*story_ready)(void *ctx);
    int (*story_leader)(void *ctx);
    int (*story_set)(void *ctx, const char *key, const void *v, int len);  // 1 taken
    int (*story_get)(void *ctx, const char *key, void *buf, int cap);    // length, -1 none
    int (*story_clear)(void *ctx, const char *prefix);
    float (*hour_get)(void *ctx, bool *held);                            // 0..24, -1 = no world yet; *held = someone holds it
    float (*hour_shown)(void *ctx);                                      // the hour the game shows (-1 not known); may be NULL
    void (*hour_hold)(void *ctx, float hour);
    void (*hour_release)(void *ctx);
    int (*weather_get)(void *ctx);                                       // -1 not known; NULL = no weather hook
    bool (*weather_force)(void *ctx, int type);                          // type < 0 releases; NULL = no hook
    int (*bloodmoon_count)(void *ctx);                                   // -1 not known; NULL = no hook
    bool (*bloodmoon_play)(void *ctx);                                   // NULL = no hook
    void (*log)(void *ctx, const char *line);
    // (s191 task 2; all may be NULL)
    int (*world_open)(void *ctx);                                        // 1 open world, 0 a shrine / Divine Beast / indoors, -1 not known; NULL = open
    int (*climate_get)(void *ctx);                                       // -1 not known
    bool (*bloodmoon_busy)(void *ctx);                                   // a Blood Moon of ours is on its way: no holds
    int (*gate_get)(void *ctx);                                          // the paraglider clock gate byte, -1 not known
    void (*gate_put)(void *ctx, int v);
    // (s198; may be NULL = always in the world)
    int (*in_world)(void *ctx);                                          // 1 Link stands in the world, 0 the title / a load
} ClockIO;

typedef struct {
    ClockIO io;
    int role, wxmode;                // wxmode: set mclockwx 0 never / 1 same climate (default) / 2 open world
    bool have, pending, bad_logged;  // the room's latest value; pending = not acted on yet (weather / Blood Moon)
    ClockVal val;
    float last_h;                    // the hour last read (-1: none)
    // guest
    bool applied_any, ours, moons_known, busy_was, owed, owed_logged;
    double applied, drift_at, wx_at, owed_at;
    int forced, moons, wx_failed, gate_old;  // forced = the weather we forced (-1 none); gate_old -1 = not read
    // leader
    bool wrote, m_have, set_fail_logged;
    bool take_on, need_take, wait_logged;  // (s198) set mclocktake; a new leader still has to take / start the room's clock
    int take_n;                      // (s198) times this leader set the room's hour into its game (at most CLOCK_TAKE_MAX)
    bool guard;                      // (s198) a take set our hour and nothing was written since: a move off it = take again
    bool reload, reload_drop;        // (s198 b) our game read a save while leading / it has been out of the world since
    double reload_at;
    ClockVal w;                      // the last write
    double w_try, w_log, m_t0, m_h0;
    int rate_cls, game_moons, room_moons;  // rate_cls -1 not measured yet, 0 stopped, 1 running
    struct { unsigned writes, holds, plays, forces, bad, releases, takes, reloads; } st;
} Clock;

// ---- the wire ----
static int clock_encode(const ClockVal *v, uint8_t out[CLOCK_WIRE_LEN]) {
    memcpy(out, &v->at, 8), memcpy(out + 8, &v->hour, 8), memcpy(out + 16, &v->rate, 8);
    out[24] = v->weather >= 0 && v->weather < 0xff ? (uint8_t)v->weather : 0xff;
    out[25] = (uint8_t)(v->moons < 0 ? 0 : v->moons);
    out[26] = v->climate >= 0 && v->climate < 0xff ? (uint8_t)v->climate : 0xff;
    return CLOCK_WIRE_LEN;
}
static bool clock_decode(const void *b, int len, ClockVal *v) {
    if (!b || len < CLOCK_WIRE_MIN) return false;
    const uint8_t *p = (const uint8_t *)b;
    double at, hour, rate;
    memcpy(&at, p, 8), memcpy(&hour, p + 8, 8), memcpy(&rate, p + 16, 8);
    if (!isfinite(at) || !isfinite(hour) || !isfinite(rate) || hour < 0 || hour > 24 || fabs(rate) > CLOCK_RATE_MAX) return false;
    v->at = at, v->hour = hour >= 24 ? 0 : hour, v->rate = rate;
    v->weather = len >= 25 && p[24] != 0xff ? p[24] : -1;
    v->moons = len >= 26 ? p[25] : -1;
    v->climate = len >= 27 && p[26] != 0xff ? p[26] : -1;
    return true;
}

// ---- hours on a 24 h circle ----
static double clock_wrap24(double h) {
    h = fmod(h, 24.0);
    if (h < 0) h += 24.0;
    return h >= 24.0 ? 0 : h;
}
static double clock_delta(double from, double to) { double d = clock_wrap24(to - from); return d > 12 ? d - 24 : d; }  // -12..12
static double clock_dist(double a, double b) { return fabs(clock_delta(a, b)); }
static double clock_at(const ClockVal *v, double now) { return clock_wrap24(v->hour + (now - v->at) * v->rate); }

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static void clock_logf(Clock *c, const char *f, ...) {
    if (!c->io.log) return;
    char b[256];
    va_list a;
    va_start(a, f);
    vsnprintf(b, sizeof b, f, a);
    va_end(a);
    c->io.log(c->io.ctx, b);
}
static const char *clock_role_name(int r) { return r == CLOCK_LEADER ? "leader" : r == CLOCK_GUEST ? "guest" : "off"; }

// ---- the core ----
static void clock_guest_reset(Clock *c) {
    c->ours = c->applied_any = c->busy_was = c->owed = c->owed_logged = false;
    c->forced = c->wx_failed = c->gate_old = -1, c->drift_at = 0, c->wx_at = c->owed_at = -1e300;
}
// Our hold / weather force go (only ours), the paraglider gate back to 0 if it was 0. The guest state starts over.
static void clock_release(Clock *c, const char *why) {
    bool hour = c->ours && c->applied_any && c->io.hour_release, wx = c->forced >= 0 && c->io.weather_force;
    if (hour) c->io.hour_release(c->io.ctx);
    bool gate = hour && c->gate_old == 0 && c->io.gate_put;  // (after the release: its write_time sets the gate to 1 again)
    if (gate) c->io.gate_put(c->io.ctx, 0);
    if (wx) c->io.weather_force(c->io.ctx, -1);
    if (hour || wx)
        c->st.releases++, clock_logf(c, "[clock] released the %s%s%s%s (%s)", hour ? "hour" : "", hour && wx ? " and the " : "", wx ? "weather" : "",
                                     gate ? ", paraglider clock gate back to 0" : "", why);
    else if (c->applied_any && !c->ours)
        clock_logf(c, "[clock] the hour stays held: it was held before the room's clock (SAVAGEWIND_TEST_TIME / time freeze) (%s)", why);
    if (c->owed) clock_logf(c, "[clock] the owed Blood Moon is dropped (%s)", why);
    clock_guest_reset(c);
}

// A value of the story key as the room handed it (deleted: the key went).
static void clock_on_value(Clock *c, const void *v, int len, bool deleted) {
    if (deleted) {
        c->have = c->pending = false;
        if (c->role == CLOCK_GUEST) clock_release(c, "the room's clock key went");
        return;
    }
    ClockVal x;
    if (!clock_decode(v, len, &x)) {
        c->st.bad++;
        if (!c->bad_logged) c->bad_logged = true, clock_logf(c, "[clock] a bad %s value (%d bytes): ignored (logged once)", CLOCK_KEY, len);
        return;
    }
    c->val = x, c->have = c->pending = true;
}

static void clock_set_role(Clock *c, int role, const char *why) {
    if (role == c->role) return;
    if (c->role == CLOCK_GUEST) clock_release(c, why);
    clock_logf(c, "[clock] %s -> %s (%s)", clock_role_name(c->role), clock_role_name(role), why);
    c->role = role;
    // (the key came before we listened: the room's copy. s198 mclocktake: always read, the room's copy is the newest; a
    // new leader whose room has none has none, whatever an older room / role left in c->val)
    if (role != CLOCK_OFF && (!c->have || c->take_on) && c->io.story_get) {
        uint8_t b[64];
        int n = c->io.story_get(c->io.ctx, CLOCK_KEY, b, (int)sizeof b);
        if (n >= CLOCK_WIRE_MIN) clock_on_value(c, b, n, false);
        else if (n == -1 && c->take_on && role == CLOCK_LEADER) c->have = c->pending = false;
    }
    if (role == CLOCK_GUEST) {
        c->pending = c->have, c->moons_known = false;  // (a join: the first count is adopted, never played)
        clock_guest_reset(c);
    } else if (role == CLOCK_LEADER) {
        c->wrote = c->m_have = c->set_fail_logged = false, c->rate_cls = -1, c->game_moons = -1;
        c->room_moons = c->have && c->val.moons >= 0 ? c->val.moons : 0;  // (the count goes on from the old leader's)
        c->w_try = c->w_log = -1e300;
        c->need_take = c->take_on, c->wait_logged = false, c->take_n = 0, c->guard = c->reload = c->reload_drop = false;
    }
}

// The guest's weather: the room's while the rule says so, else BotW's own. now = the room clock.
static void clock_guest_weather(Clock *c, double now, bool at_once) {
    if (!c->io.weather_force || (!at_once && now - c->wx_at < CLOCK_WX_MS && now >= c->wx_at)) return;
    c->wx_at = now;
    int want = -1, open = c->io.world_open ? c->io.world_open(c->io.ctx) : 1, mine = -1;
    char why[96];
    snprintf(why, sizeof why, "the room's weather is not known");
    if (c->wxmode == 0) snprintf(why, sizeof why, "set mclockwx 0");
    else if (c->val.weather < 0) snprintf(why, sizeof why, "the room's weather is not known (0xff)");
    else if (open != 1) snprintf(why, sizeof why, "not in the open world here");
    else if (c->wxmode == 2 || c->val.climate < 0) want = c->val.weather;
    else if ((mine = c->io.climate_get ? c->io.climate_get(c->io.ctx) : -1) == c->val.climate) want = c->val.weather;
    else snprintf(why, sizeof why, "another climate here (%d, the leader's %d)", mine, c->val.climate);
    if (want == c->forced) return;
    if (want < 0) {
        c->io.weather_force(c->io.ctx, -1);
        clock_logf(c, "[clock] guest: weather force released: %s (BotW's own weather)", why);
        c->forced = -1, c->wx_failed = -1;
        return;
    }
    if (c->io.weather_force(c->io.ctx, want)) {
        clock_logf(c, "[clock] guest: weather %d from the room%s", want, c->wxmode == 2 ? " (set mclockwx 2)" : c->val.climate < 0 ? " (no climate in the value)" : " (same climate)");
        c->forced = want, c->wx_failed = -1, c->st.forces++;
    } else if (c->wx_failed != want) c->wx_failed = want, clock_logf(c, "[clock] guest: weather %d could not be forced (tries again)", want);
}

static void clock_guest(Clock *c) {
    bool held = false;
    float h = c->io.hour_get(c->io.ctx, &held);
    c->last_h = h;
    if (!(h >= 0) || !c->have) return;  // (no world yet: next frame)
    double now = c->io.now_ms(c->io.ctx), t = clock_at(&c->val, now);
    bool busy = c->io.bloodmoon_busy && c->io.bloodmoon_busy(c->io.ctx);
    if (busy) c->busy_was = true;  // (our Blood Moon is on its way: a hold now would undo its 24:00 push)
    else {
        bool need = !c->applied_any || c->busy_was || clock_dist(t, c->applied) >= CLOCK_STEP_H || !held || clock_dist(h, c->applied) >= CLOCK_STEP_H;
        if (!need && c->io.hour_shown && (now - c->drift_at >= CLOCK_DRIFT_MS || now < c->drift_at)) {  // (the game's clock runs under our hold?)
            float s = c->io.hour_shown(c->io.ctx);
            if (s >= 0 && clock_dist(s, c->applied) >= CLOCK_DRIFT_H) need = true, c->drift_at = now;
        }
        if (need) {
            if (!c->applied_any) clock_logf(c, "[clock] guest: hour %.2f from the room (was %.2f%s)", t, h, held ? ", held" : "");
            else if (c->busy_was) clock_logf(c, "[clock] guest: Blood Moon done: hour back to the room's %.2f", t);
            else if (clock_dist(t, c->applied) > CLOCK_BIG_H) clock_logf(c, "[clock] guest: hour %.2f -> %.2f (the room's clock jumped)", c->applied, t);
            if (!held && !c->ours) {  // (a hold that was there before our first one is not ours to release)
                c->ours = true;
                c->gate_old = c->io.gate_get ? c->io.gate_get(c->io.ctx) : -1;
            }
            c->io.hour_hold(c->io.ctx, (float)t);
            c->applied = t, c->applied_any = true, c->busy_was = false, c->st.holds++, c->last_h = (float)t;
        }
    }
    bool fresh = c->pending;
    if (fresh) {
        c->pending = false;
        if (c->val.moons >= 0) {
            if (!c->moons_known) c->moons_known = true, c->moons = c->val.moons;
            else if (c->val.moons != c->moons) {
                c->moons = c->val.moons;
                if (c->io.bloodmoon_play && !c->owed) c->owed = true, c->owed_logged = false, c->owed_at = -1e300;
            }
        }
    }
    clock_guest_weather(c, now, fresh);
    if (c->owed && !busy && (now - c->owed_at >= CLOCK_OWED_MS || now < c->owed_at)) {
        c->owed_at = now;
        if (c->io.bloodmoon_play(c->io.ctx)) {
            c->owed = false, c->st.plays++;
            clock_logf(c, "[clock] guest: Blood Moon from the room (count %d)%s", c->moons, c->owed_logged ? ": the owed one plays now" : "");
        } else if (!c->owed_logged)
            c->owed_logged = true, clock_logf(c, "[clock] guest: Blood Moon owed (count %d): the game cannot play it here, tries every 5 s", c->moons);
    }
}

// (s198) A new leader in the world, the room has a clock: its hour (run on from the value's `at` at the value's rate) goes
// into our game. hold + release = set it and let our clock run (a hold that was there before us stays: only held). The
// paraglider gate goes back to 0 if it was 0, as a guest's release does. The rate is measured again from the next frame.
// reload (s198 b): after a load of ours, always set (the guard then watches the first second) and say so in its own words.
static void clock_take(Clock *c, float h, bool held, double now, bool reload) {
    double t = clock_at(&c->val, now), age = (now - c->val.at) / 1000.0;
    c->need_take = false, c->st.takes++;
    if (c->val.moons >= 0) c->room_moons = c->val.moons;  // (a value that came after the lead: its count goes on too)
    if (!reload && clock_dist(h, t) < CLOCK_STEP_H) {
        clock_logf(c, "[clock] took the room's clock %.2f as the new leader (our game is there already; the value is %.1f s old)", t, age);
        return;
    }
    int gate = !held && c->io.gate_get ? c->io.gate_get(c->io.ctx) : -1;
    c->io.hour_hold(c->io.ctx, (float)t);
    if (!held) c->io.hour_release(c->io.ctx);
    if (gate == 0 && c->io.gate_put) c->io.gate_put(c->io.ctx, 0);
    c->m_have = false, c->rate_cls = -1, c->last_h = (float)t, c->take_n++, c->guard = true;
    if (reload)
        clock_logf(c, "[clock] leader: back in the world after a load: took the room's clock %.2f again (the save says %.2f; our last write is %.1f s old, %s%s)", t,
                   h, age, c->val.rate ? "run on at the normal rate" : "the room's clock was stopped", held ? "; the hour stays held: it was held before" : "");
    else
        clock_logf(c, "[clock] took the room's clock %.2f as the new leader (our game was at %.2f; the value is %.1f s old, %s%s)", t, h, age,
                   c->val.rate ? "run on at the normal rate" : "the room's clock was stopped", held ? "; the hour stays held: it was held before" : "");
}

static void clock_leader(Clock *c) {
    bool held = false;
    float h = c->io.hour_get(c->io.ctx, &held);
    c->last_h = h;
    if (!(h >= 0)) { c->m_have = false, c->reload_drop |= c->reload; return; }  // (loading: no writes; the guests run on)
    double now = c->io.now_ms(c->io.ctx);
    if (c->take_on && c->io.in_world && c->io.in_world(c->io.ctx) != 1) {  // (s198: the title screen's / a load's hour is not the room's)
        c->m_have = false, c->reload_drop |= c->reload;
        if (!c->wait_logged)
            c->wait_logged = true, clock_logf(c, "[clock] leader: not leading yet: not in the world (title screen / loading; our hour %.2f is not written)%s", h,
                                              c->have ? ", the guests run on from the room's last value" : "");
        return;
    }
    if (c->take_on && c->reload) {  // (s198 b: our game read a save: nothing written until it has been out of the world and back)
        if (!c->reload_drop && now - c->reload_at < CLOCK_RELOAD_MS && now >= c->reload_at) { c->m_have = false; return; }
        c->reload = false;
        if (!c->reload_drop) clock_logf(c, "[clock] leader: our game read a save but never left the world in %.0f s: nothing taken", CLOCK_RELOAD_MS / 1000);
        else if (!c->need_take && c->have) { c->take_n = 0, clock_take(c, h, held, now, true); return; }
    }
    if (c->take_on && c->need_take && c->have) { clock_take(c, h, held, now, false); return; }
    if (!c->m_have || now < c->m_t0) c->m_have = true, c->m_t0 = now, c->m_h0 = h;
    else if (now - c->m_t0 >= CLOCK_MEASURE_MS) {
        double r = clock_delta(c->m_h0, h) / (now - c->m_t0);
        c->rate_cls = fabs(r) < 0.25 * CLOCK_RATE_NORMAL ? 0 : 1;  // (a sleep / time skip in the window: running; the jump writes)
        c->m_t0 = now, c->m_h0 = h;
    }
    if (c->rate_cls < 0) return;  // (the first second: the rate is not known yet)
    bool open = !c->io.world_open || c->io.world_open(c->io.ctx) == 1;  // (a shrine reads "0 Bluesky": not the room's weather)
    int wx = open && c->io.weather_get ? c->io.weather_get(c->io.ctx) : -1, cl = open && c->io.climate_get ? c->io.climate_get(c->io.ctx) : -1;
    if (wx < 0 || wx >= 0xff) wx = -1;
    if (cl < 0 || cl >= 0xff) cl = -1;
    if (c->io.bloodmoon_count) {
        int g = c->io.bloodmoon_count(c->io.ctx);
        if (g >= 0) {
            if (c->game_moons >= 0 && g == c->game_moons + 1) {  // (our game played one; any other change = another save loaded)
                c->room_moons = (c->room_moons + 1) & 0xff;
                if (c->io.story_clear) c->io.story_clear(c->io.ctx, CLOCK_DEAD_PREFIX);
                clock_logf(c, "[clock] leader: Blood Moon in our game: room count %d, %s cleared", c->room_moons, CLOCK_DEAD_PREFIX);
            }
            c->game_moons = g;
        }
    }
    // (s198, GUESS that a load can still put its save's hour back once Link stands: our take is one write, not a hold) the
    // game moved the hour off the taken one before our first write: set it again, at most CLOCK_TAKE_MAX times in all
    if (c->guard && c->take_on && c->have && c->take_n < CLOCK_TAKE_MAX && clock_dist(h, clock_at(&c->val, now)) > CLOCK_JUMP_H) {
        clock_logf(c, "[clock] leader: our game moved the hour to %.2f after the take (the room's %.2f): taken again", h, clock_at(&c->val, now));
        clock_take(c, h, held, now, false);
        return;
    }
    double rate = c->rate_cls ? CLOCK_RATE_NORMAL : 0;
    const char *why = NULL;
    bool periodic = false;
    if (!c->wrote) {
        why = "first";
        if (c->need_take)  // (s198: still no value a second into the world = a fresh room)
            c->need_take = false, clock_logf(c, "[clock] leader: the room has no clock yet: our hour %.2f starts it", h);
    } else if (c->room_moons != c->w.moons) why = "Blood Moon";
    else if (rate != c->w.rate) why = rate ? "our clock runs again" : "our clock stopped";
    else if (wx != c->w.weather) why = "weather";
    else if (cl != c->w.climate) why = "climate";
    else if (clock_dist(h, clock_at(&c->w, now)) > CLOCK_JUMP_H) why = "hour jump";
    else if (now - c->w.at >= CLOCK_PERIOD_MS || now < c->w.at) why = "every 5 s", periodic = true;
    if (!why || (now - c->w_try < CLOCK_GAP_MS && now >= c->w_try)) return;
    ClockVal v = {now, h, rate, wx, c->room_moons, cl};
    uint8_t b[CLOCK_WIRE_LEN];
    clock_encode(&v, b);
    c->w_try = now;
    if (!c->io.story_set(c->io.ctx, CLOCK_KEY, b, CLOCK_WIRE_LEN)) {
        if (!c->set_fail_logged) c->set_fail_logged = true, clock_logf(c, "[clock] leader: the room did not take the write (tries again)");
        return;
    }
    c->w = v, c->wrote = true, c->st.writes++, c->guard = false;
    if (c->take_on) c->val = v, c->have = true;  // (s198: the room's value now; it is not echoed back to its writer)
    if (!periodic || now - c->w_log >= CLOCK_PERIOD_LOG_MS) {
        if (periodic) c->w_log = now;
        clock_logf(c, "[clock] leader: hour %.3f rate %s weather %d climate %d count %d (%s%s)", h, rate ? "normal" : "0", wx, cl, c->room_moons, why,
                   periodic ? ", logged once a minute" : "");
    }
}

// (s198 b) BotW read a game_data.sav (sora_levels.h: Continue / Load). A leader keeps the room's clock through the load.
static void clock_on_load(Clock *c, int slot) {
    if (c->role != CLOCK_LEADER || !c->take_on) return;
    c->reload = true, c->reload_drop = false, c->reload_at = c->io.now_ms(c->io.ctx), c->st.reloads++;
    clock_logf(c, "[clock] leader: our game reads a save (slot %d): its hour is not written; the room's clock is taken again once we are back in the world", slot);
}

// Once a frame. active: in a Melty room, connected and the feature key on. dev: a dev aid's role without a room
// (CLOCK_GUEST = set mclockfake, CLOCK_LEADER = set mclocklead), CLOCK_OFF = none.
static void clock_frame(Clock *c, bool active, int dev) {
    int role = CLOCK_OFF;
    const char *why = "not in a room, or set mclock 0";
    if (dev == CLOCK_GUEST) role = CLOCK_GUEST, why = "set mclockfake: a fake room clock";
    else if (dev == CLOCK_LEADER) role = CLOCK_LEADER, why = "set mclocklead: the leader against a loopback";
    else if (active && !c->io.story_ready(c->io.ctx)) why = "waiting for the room's story";
    else if (active) role = c->io.story_leader(c->io.ctx) ? CLOCK_LEADER : CLOCK_GUEST, why = role == CLOCK_LEADER ? "we lead the room's story" : "the room's clock";
    clock_set_role(c, role, why);
    if (role == CLOCK_GUEST) clock_guest(c);
    else if (role == CLOCK_LEADER) clock_leader(c);
    else c->last_h = -1;
}

static void clock_init(Clock *c, const ClockIO *io) {
    memset(c, 0, sizeof *c);
    c->io = *io, c->last_h = -1, c->rate_cls = -1, c->game_moons = -1, c->wxmode = 1, c->take_on = true;
    clock_guest_reset(c);
}
// "9.50G" / "9.50Gh" (held by us) / "14.02L" / "-" (off)
static void clock_state_txt(const Clock *c, char *out, int cap) {
    if (c->role == CLOCK_OFF || !(c->last_h >= 0)) snprintf(out, (size_t)cap, "-");
    else snprintf(out, (size_t)cap, "%.2f%c%s", c->last_h, c->role == CLOCK_LEADER ? 'L' : 'G', c->ours && c->applied_any ? "h" : "");
}

#ifndef SORA_CLOCK_TEST
// ---- the game binding (sora_net.h includes this after sora_net_sock.h: LOG, sora_melty_*, GetTickCount64) ----
#include "kairi_env.h"
static struct {
    Clock c;
    bool io_set;
    int on;        // live "set mclock 0|1"; -1 = not decided (env SAVAGEWIND_SORA_MCLOCK, else on when Melty started the game)
    int wxmode;    // live "set mclockwx 0|1|2" (default 1)
    float fake;    // live "set mclockfake <hour>": -1 = off
    int fake_wx, fake_bm, fake_cl;  // live "set mclockfakewx / mclockfakebm 1 / mclockfakecl": the fake value's bytes
    bool lead;     // live "set mclocklead 1": the leader against a loopback
    int take;      // (s198) live "set mclocktake 0|1" (default 1): a new leader takes the room's clock, no write off the world
    int resolved;  // 0 not yet, 1 the hour exports are there, -1 not in this exe
    float (*hour)(bool *held);       // exe test_warp_hour
    void (*hold)(float hour);        // exe test_warp_hold_hour (< 0 releases)
    bool (*env)(float out[4]);       // exe sora_world_env (out[0] = time of day 0..360)
    int (*weather)(void);            // sora_world.h
    bool (*weather_force)(int type);
    int (*moons)(void);
    bool (*moons_play)(void);
    int (*open)(void);
    int (*climate)(void);
    bool (*busy)(void);
    int (*gate)(void);
    bool (*gate_put)(int v);
    int (*inworld)(void);            // (s198) sora.c sora_clock_in_world
    int (*save_ev)(uint64_t *seq, int *slot, int *kind, char *dir, int n);  // (s198 b) exe fs_save_event (services/fs.c)
    uint64_t sev_seq;                // its last event seen by us
    bool sev_live;                   // false: the events so far are old (first look, or after a time outside a room)
    uint64_t sev_pend[16];           // when a slot's write began (kind 1; 0 = written out (2)): a read within 10 s is a check
} CK = {.on = -1, .wxmode = 1, .fake = -1, .fake_wx = -1, .fake_cl = -1, .take = 1};

static bool ck_dev(void) { return CK.fake >= 0 || CK.lead; }
static double ck_now(void *x) { (void)x; return ck_dev() ? (double)GetTickCount64() : sora_melty_now_ms(); }
static int ck_ready(void *x) { (void)x; return CK.lead || sora_melty_story_ready(); }
static int ck_leader(void *x) { (void)x; return CK.lead || sora_melty_story_leader(); }
static int ck_set(void *x, const char *k, const void *v, int n) {
    (void)x;
    if (!CK.lead) return sora_melty_story_set(k, v, n);
    ClockVal d;
    if (!clock_decode(v, n, &d)) LOG("[clock] would write: %s, %d bytes that do not decode\n", k, n);
    else LOG("[clock] would write: %s at %.0f hour %.3f rate %s weather %d count %d climate %d (%d bytes; set mclocklead: nothing sent)\n", k, d.at, d.hour,
             d.rate ? "normal" : "0", d.weather, d.moons, d.climate, n);
    return 1;
}
static int ck_get(void *x, const char *k, void *b, int cap) { (void)x; return ck_dev() ? -1 : sora_melty_story_get(k, b, cap); }
static int ck_clear(void *x, const char *p) {
    (void)x;
    if (CK.lead) { LOG("[clock] would clear: %s* (set mclocklead: nothing sent)\n", p); return 1; }
    return sora_melty_story_clear(p);
}
static float ck_hour(void *x, bool *held) { (void)x; *held = false; return CK.hour ? CK.hour(held) : -1; }
static float ck_shown(void *x) { (void)x; float e[4]; return CK.env && CK.env(e) && e[0] >= 0 ? e[0] / 15.0f : -1; }
static void ck_hold(void *x, float h) { (void)x; if (CK.hold) CK.hold(h); }
static void ck_release(void *x) { (void)x; if (CK.hold) CK.hold(-1); }
static int ck_weather(void *x) { (void)x; return CK.weather(); }
static bool ck_wforce(void *x, int t) { (void)x; return CK.weather_force(t); }
static int ck_moons(void *x) { (void)x; return CK.moons(); }
static bool ck_mplay(void *x) { (void)x; return CK.moons_play(); }
static int ck_open(void *x) { (void)x; return CK.open(); }
static int ck_climate(void *x) { (void)x; return CK.climate(); }
static bool ck_busy(void *x) { (void)x; return CK.busy(); }
static int ck_gate(void *x) { (void)x; return CK.gate(); }
static void ck_gate_put(void *x, int v) { (void)x; CK.gate_put(v); }
static int ck_inworld(void *x) { (void)x; return CK.inworld(); }
static void ck_log(void *x, const char *s) { (void)x; LOG("%s\n", s); }

static void ck_io(void) {  // (every hook the core may call; the optional ones only when there is one)
    ClockIO io = {NULL, ck_now, ck_ready, ck_leader, ck_set, ck_get, ck_clear, ck_hour, CK.env ? ck_shown : NULL, ck_hold, ck_release,
                  CK.weather ? ck_weather : NULL, CK.weather_force ? ck_wforce : NULL, CK.moons ? ck_moons : NULL,
                  CK.moons_play ? ck_mplay : NULL, ck_log, CK.open ? ck_open : NULL, CK.climate ? ck_climate : NULL,
                  CK.busy ? ck_busy : NULL, CK.gate ? ck_gate : NULL, CK.gate_put ? ck_gate_put : NULL, CK.inworld ? ck_inworld : NULL};
    if (!CK.io_set) clock_init(&CK.c, &io), CK.io_set = true;
    else CK.c.io = io;
}
static bool ck_resolve(void) {
    if (CK.resolved) return CK.resolved > 0;
#if !defined(SORA_CLOCK_NO_EXE) && defined(_WIN32)  // (host tests set the pointers themselves)
    HMODULE exe = GetModuleHandleA(NULL);
    CK.hour = (float (*)(bool *))(void *)GetProcAddress(exe, "test_warp_hour");
    CK.hold = (void (*)(float))(void *)GetProcAddress(exe, "test_warp_hold_hour");
    CK.save_ev = (int (*)(uint64_t *, int *, int *, char *, int))(void *)GetProcAddress(exe, "fs_save_event");
    CK.env = (bool (*)(float *))(void *)GetProcAddress(exe, "sora_world_env");
#ifdef SORA_WORLD_HOOKS  // (sora.c: sora_world.h, declared before sora_net.h and defined after it)
    CK.weather = sora_world_weather, CK.weather_force = sora_world_weather_force;
    CK.moons = sora_world_bloodmoon_count, CK.moons_play = sora_world_bloodmoon_play, CK.busy = sora_world_bloodmoon_busy;
    CK.open = sora_world_open, CK.climate = sora_world_climate, CK.gate = sora_world_clock_gate, CK.gate_put = sora_world_clock_gate_put;
    CK.inworld = sora_clock_in_world;
#endif
#endif
    CK.resolved = CK.hour && CK.hold ? 1 : -1;
    if (CK.resolved < 0) LOG("[clock] this exe has no test_warp_hour / test_warp_hold_hour: the room clock stays off\n");
    else LOG("[clock] room clock ready (hour + hold%s; weather %s, Blood Moon %s, climate %s, in-the-world test %s, save reads %s; new leader %s)\n",
             CK.env ? ", shown hour" : "", CK.weather && CK.weather_force ? "hooked" : "not hooked (0xff)", CK.moons && CK.moons_play ? "hooked" : "not hooked",
             CK.climate ? "hooked" : "not hooked", CK.inworld ? "hooked" : "not hooked", CK.save_ev ? "hooked" : "not hooked (a load's hour is written)",
             CK.take ? "takes the room's clock" : "writes its own hour (mclocktake 0)");
    ck_io();
    return CK.resolved > 0;
}
static const char *ck_getenv(const char *k) { return getenv(k); }
// (s198 b) BotW's save reads / writes as the exe's fs layer saw them (services/fs.c fs_save_event: kind 0 the first read of an
// opened game_data.sav, 1 its first write, 2 written out), in order, every frame. A read = Continue / Load (as sora_levels.h
// takes it, also its rule: a read while that slot's write is not written out is a check, not a load). Old events (before
// our first look, or from a time outside a room) are passed over.
static void ck_save_events(void) {
    if (!CK.save_ev) return;
    int slot = 0, kind = 0;
    for (int i = 0; i < 64 && CK.save_ev(&CK.sev_seq, &slot, &kind, NULL, 0); i++) {
        if (!CK.sev_live || slot < 0 || slot >= (int)(sizeof CK.sev_pend / sizeof CK.sev_pend[0])) continue;  // (16 slots, not the 128 bytes)
        uint64_t t = GetTickCount64();
        if (kind == 1) CK.sev_pend[slot] = t;
        else if (kind == 2) CK.sev_pend[slot] = 0;
        else if (kind == 0 && !(CK.sev_pend[slot] && t - CK.sev_pend[slot] < 10000)) clock_on_load(&CK.c, slot);
    }
    CK.sev_live = true;
}
// Once a frame (sora_net.h net_clock_frame). in_room: a Melty room, connected. Outside a room with nothing held and no dev
// aid it returns at once: no exe lookup, no call (UDP and solo play as before).
static void ck_frame(bool in_room) {
    if (CK.on < 0 && in_room) {
        const char *e = getenv("SAVAGEWIND_SORA_MCLOCK");
        CK.on = e && *e ? atoi(e) != 0 : kairi_started_by_melty(ck_getenv);
        LOG("[clock] room clock %s (%s)\n", CK.on ? "on" : "off", e && *e ? "SAVAGEWIND_SORA_MCLOCK" : "default: on when Melty started the game");
    }
    int dev = CK.lead ? CLOCK_LEADER : CK.fake >= 0 ? CLOCK_GUEST : CLOCK_OFF;
    bool want = dev != CLOCK_OFF || (in_room && CK.on > 0);
    if (!want && CK.c.role == CLOCK_OFF) { CK.sev_live = false; return; }
    if (want && !ck_resolve()) return;  // (no hour exports: never left OFF)
    if (!CK.io_set) ck_io();
    CK.c.wxmode = CK.wxmode, CK.c.take_on = CK.take != 0;
    ck_save_events();
    clock_frame(&CK.c, want && dev == CLOCK_OFF, dev);
}
// The story's value of CLOCK_KEY (the glue's drain). The dev aids ignore the room.
static void ck_value(const void *v, int len, int deleted) {
    if (ck_dev()) return;
    if (!CK.io_set) ck_io();
    clock_on_value(&CK.c, v, len, deleted != 0);
}
// the fake guest value (at = now, that hour, the normal rate, the fake weather / count / climate), 27 bytes
static void ck_fake_feed(double hour) {
    if (!CK.io_set) ck_io();
    ClockVal v = {(double)GetTickCount64(), clock_wrap24(hour), CLOCK_RATE_NORMAL, CK.fake_wx, CK.fake_bm, CK.fake_cl};
    uint8_t b[CLOCK_WIRE_LEN];
    clock_encode(&v, b);
    clock_on_value(&CK.c, b, CLOCK_WIRE_LEN, false);
}
static double ck_fake_now_hour(void) {  // (a fake value change keeps the hour where the fake clock is now)
    return CK.c.have && CK.c.role == CLOCK_GUEST ? clock_at(&CK.c.val, (double)GetTickCount64()) : CK.fake;
}
// live "set mclockfake <hour>": the guest path with a fake value from a local ms timer, also outside a room: shows one game
// set + hold the hour. -1 = off (our hold is released). The same hour again (a live-file rewrite) changes nothing.
static void ck_fake(float hour) {
    if (!(hour >= 0)) {
        if (CK.fake < 0) return;
        CK.fake = -1;
        if (CK.io_set) clock_set_role(&CK.c, CLOCK_OFF, "set mclockfake -1"), CK.c.have = CK.c.pending = false;
        LOG("[clock] fake room clock off\n");
        return;
    }
    if (hour > 24) { LOG("[clock] mclockfake %.2f refused (0..24, -1 = off)\n", hour); return; }
    if (CK.fake == hour) return;
    if (CK.lead) CK.lead = false, LOG("[clock] mclocklead off (mclockfake is on)\n");
    CK.fake = hour;
    ck_fake_feed(hour);
    LOG("[clock] fake room clock: hour %.2f at the normal rate, weather %d, count %d, climate %d (set mclockfake -1 = off)\n", hour, CK.fake_wx, CK.fake_bm, CK.fake_cl);
}
static bool ck_live_set(const char *key, int v) {
    if (!strcmp(key, "mclock")) {
        if (CK.on == (v != 0)) return true;
        CK.on = v != 0;
        LOG("[clock] room clock %s\n", CK.on ? "on (in a Melty room)" : "off (set mclock 0: our own clock; a hold of ours is released)");
    } else if (!strcmp(key, "mclockwx")) {
        int m = v < 0 ? 0 : v > 2 ? 2 : v;
        if (CK.wxmode == m) return true;
        CK.wxmode = m;
        LOG("[clock] room weather: %s\n", m == 0 ? "never forced (set mclockwx 0)" : m == 1 ? "forced in the open world with the leader's climate only (default)"
                                                                                      : "forced in the open world whatever the climate (set mclockwx 2)");
    } else if (!strcmp(key, "mclockfakewx") || !strcmp(key, "mclockfakecl")) {
        bool wx = key[10] == 'w';
        int n = v < 0 || v >= 0xff ? -1 : v;
        if (wx ? CK.fake_wx == n : CK.fake_cl == n) return true;
        if (wx) CK.fake_wx = n;
        else CK.fake_cl = n;
        LOG("[clock] fake room clock: %s %d%s\n", wx ? "weather" : "climate", n, CK.fake >= 0 ? "" : " (with set mclockfake <hour>)");
        if (CK.fake >= 0) ck_fake_feed(ck_fake_now_hour());
    } else if (!strcmp(key, "mclockfakebm")) {  // (one shot: remove the line again)
        if (!v) return true;
        CK.fake_bm = (CK.fake_bm + 1) & 0xff;
        LOG("[clock] fake room clock: Blood Moon count %d%s\n", CK.fake_bm, CK.fake >= 0 ? "" : " (with set mclockfake <hour>)");
        if (CK.fake >= 0) ck_fake_feed(ck_fake_now_hour());
    } else if (!strcmp(key, "mclocktake")) {
        if (CK.take == (v != 0)) return true;
        CK.take = v != 0;
        LOG("[clock] new leader: %s\n", CK.take ? "takes the room's clock first, no write off the world (default)"
                                                : "writes its own hour at once, also from the title screen (set mclocktake 0: the s191 behaviour)");
    } else if (!strcmp(key, "mclocklead")) {
        if (CK.lead == (v != 0)) return true;
        if (v && CK.fake >= 0) ck_fake(-1);
        CK.lead = v != 0;
        if (!CK.lead && CK.io_set) clock_set_role(&CK.c, CLOCK_OFF, "set mclocklead 0"), CK.c.have = CK.c.pending = false;
        LOG("[clock] mclocklead %s\n", CK.lead ? "on: the leader core against a loopback ([clock] would write: ...; nothing sent)" : "off");
    } else return false;
    return true;
}
static void ck_unload(void) {
    CK.fake = -1, CK.lead = false;
    if (CK.io_set && CK.c.role != CLOCK_OFF) clock_set_role(&CK.c, CLOCK_OFF, "this copy of the mod is going");
}
static void ck_state(char *out, int cap) {
    if (!CK.io_set) snprintf(out, (size_t)cap, "-");
    else clock_state_txt(&CK.c, out, cap);
}
#endif

// sora_nopause.h (s289 nopause, DLL side): "the world runs in menus". The user's pick from Savage Wind PR #294 "Online mode"
// (byluki, fixes by mstampfli; MIT; this is a port after it, docs: sb/s289/pr294/ONLINE.md "The menus don't pause"):
// "Opening a menu doesn't pause the world while a friend is in the room (in theirs, enemies keep moving and can still hit you)."
//
// Live keys (sora_net.h net_live_set; all compiled default OFF; key off = the old frames: nothing read, called or written):
//   set netnopause 0|1     1 = in a Melty room with another player, BotW's pause menu, its map, the quick menus and the KH2 camp
//                          (which opens over BotW's own pause: sora_camp_page.h CP_WAIT_PAUSE -> CP_OPEN at pause state 2) leave the
//                          world running: enemies, physics, time of day. One log line when it changes.
//   set netnopauselog 0|1  the per-event lines ([nopause] on / off / why, other UI pause values, the enemies' AS behind the menu,
//                          every blow) and a line every second while the world runs (the hour, the nearest enemy's walk, the
//                          game's VFR updates per present).
//   set netnopausedmg <q>  quarter hearts one blow behind the menu takes (default 4 = 1 heart; Moblin / Lynel / Hinox / Talus x2).
//                          GUESS numbers: PR #294 uses its own weapon / armour tables (online_mod/power.txt, defense.txt).
//   set netnopausestep 1|2 (default 1 = the game's paused step as ever) the paused game updates VFR twice a present
//                          (hle/framepace.c); 2 halves the step the exe hands it while the world runs behind a menu, for when
//                          the 1 s line shows the hour / enemies at twice the speed of play (not known without the game).
//
// How BotW stops the world for a menu (PR #294, VERIFIED there live; the addresses checked here statically in the 1.9.0 main,
// sb/s289/nopause/words.py; the scene state's readers, writer and frame timing in recon/s289/S289_HELPER_NOPAUSE.md):
//   the UI's pause state (sites row pause_state, 1.9.0 0x7001e1611c) is 2 for the pause menu / map (and so the KH2 camp), 4 for the
//   quick menus, 0x200 a load's bit; the menu sets the actors' pause bits 0x7f (BaseProcMgr [np_bpm_ptr], its u16 +0x168; bit 0
//   skips the world manager's update), pauses the physics (np_phys_pause(system, 1, 0), system = [[phys_system_got]]), sets the
//   game scene's paused byte ([[np_scene_got]] +0x8b8) and its state 1 (the frame skips the world's jobs while it is not 0).
// Each mod frame while it is on, the menu's own resume (1.9.0 0x7000d462d0) for the world alone, as the PR does: the exe
//   writes the scene state's 1 as 0 at its VFR update, right after this frame (runtime/nopause.c rt_nopause_vfr, no game-code
//   patch: permission from sw_nopause_set, good for 2 presents; the game's value back at once when it ends), and when bit 0
//   is set: np_phys_pause(system, 0, 0), np_bits_clear(mgr, 0x7f), np_procs(mgr), the scene's paused byte 0. The menu's own
//   screens, sounds and input are left alone.
// The pouch rule (PR): A or X down with the pause menu / map up (BotW's own menu, not while the KH2 camp is up: the camp eats
//   its buttons before BotW sees them; s290 labfix: also behind the camp's "BotW Menu", CP_HANDOVER, np_pad_newly) stops the world again as the menu stopped it (np_phys_pause 1, np_bits_set 0x7f, the
//   manager's +0x163 = 1 as the game writes it after its bits set, the byte 1) until the menu goes: a thing taken out of the
//   pouch while the world runs hangs in the air (PR). The menu's own resume undoes it as it closes.
// Blows behind the menu (PR own_strikes_in_menu): BotW takes no damage on its Link while its menu is up (PR, measured: 12 -> 12
//   quarter hearts in 14 s next to Bokoblins), so an enemy near him that starts an AS with "Attack" / "Kick" in its name hits
//   NP_STRIKE_MS later when Link is within its reach in front of it and not guarding; an archer's "ArrowShoot" facing him within
//   40 m lands d / 40 s later. Every enemy of linkpos's list counts: one whose brain is the friend's (an esync puppet) plays the
//   owner's attack here and would hit him the normal way in play. GUESS (safety, not in the PR): a blow behind the menu never
//   takes the last quarter heart (no death inside a menu: our Game Over / revive were never tried with a BotW menu up).
// Our own frames while it is on: the servo lets go of Link (drive_off each frame: vel_step would push him at Sora's last speed
//   while the physics runs), the net ticks with Link's real body position (SNAPs without NF_LOADING: the friend sees him in his
//   last pose, esync keeps him as a player, so the enemies near him stay his and keep moving for the friend), noauto keeps its
//   hold (the AutoPlacementMgr runs too). Sora / the party / the avatars stay hidden as in any menu. The live file is still not
//   read while the KH2 camp is up (unchanged: the camp branch returns before live_poll); BotW's own menus read it as before.
// Off again (key 0, the friend gone, Sora mode off, a refused call, Game Over / DOWNED) with a menu value (2 / 4) up: the world
//   is stopped again as the menu stopped it, and the menu's own resume undoes that as it closes, as ever. A DLL unload does the
//   same (np_unload). Midnight (not in the PR; GUESS safety): BotW never crosses midnight with a menu up, and its midnight is
//   where the new day and the Blood Moon start (TimeMgr::calc at time >= 360 -> the new-day function -> the event Demo011_0,
//   sora_world.h / S191_WORLD.md, VERIFIED there by Ghidra). From 23:57 to 0:03 (NP_NIGHT_H), or while a Blood Moon of the
//   room's is on its way here (sora_world_bloodmoon_busy: its +0x14a / WM_Time 360 steps need TimeMgr::calc, which a stopped
//   world never runs), the world stays stopped behind that menu, as today, until the menu goes.
//   Other UI pause values (a new item's window, a talk) never start it (narrower than the PR, which runs the world for every
//   UI pause but a load's); one that comes while it runs keeps it running, the PR's way, and if it has to stop
//   there the stop waits (np_owed) until a menu value is back: a stop is never laid on a pause the menu's resume won't undo.
//   No UI pause (the menu closed: its own resume ran) or a load (the world waits for the map): nothing to stop, the game's way.
// Needs: an exe with runtime/nopause.c (sw_nopause_set + sw_nopause_counts) + the np_* rows of sora_sites.h, on 1.9.0 (the
// game library as Savage Wind builds it, nothing patched); else the key is refused with one log line and nothing else happens.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// ------------------------------------------------------------------ pure core (host test: runtime/test/nopause_test.c)
#define NP_PS_MENU 2u         // the UI's pause state: BotW's pause menu / map (the KH2 camp opens over it)
#define NP_PS_QUICK 4u        // the quick menus
#define NP_PS_LOAD 0x200u     // a load's bit: the world waits for the map
#define NP_STRIKE_MS 167      // an attack AS's start to its blow (PR: STRIKE_FRAMES 10, its frames at 60 fps; GUESS)
#define NP_ARROW_MPS 40.0f    // (PR) an arrow's speed
#define NP_ARROW_M 40.0f      // (PR) an archer this near, facing him
#define NP_NEAR_M 45.0f       // enemies farther than this from Link are not followed (a blow needs < 6.5 m, an arrow < 40 m)
#define NP_DMG_DEFAULT 4      // quarter hearts a blow takes by default (GUESS)
#define NP_AX_MASK ((1ull << 0) | (1ull << 2))  // A (bit 0), X (bit 2): the pouch's take
#define NP_NIGHT_H 0.05f      // game hours each side of midnight with no world behind a menu (3 real s at the speed of play; GUESS)

typedef enum {
    NP_ON,       // the world runs behind the menu
    NP_KEY,      // set netnopause 0
    NP_NOEXE,    // the exe has no nopause half (or this game version no rows)
    NP_NOROOM,   // not in a Melty room with another player
    NP_NOMENU,   // no UI pause (playing) or a load
    NP_OTHERUI,  // a UI pause that is not the pause menu / map / a quick menu
    NP_DEAD,     // Game Over / DOWNED
    NP_NIGHT,    // midnight / a Blood Moon of the room's on its way, until the menu goes
    NP_HELD,     // the pouch rule: A or X in the pause menu, until it goes
    NP_FAILED,   // a guest call was refused (or no Link body) during this menu
} NpWhy;
static const char *const NP_WHY_TXT[] = {"on", "the key is 0", "no nopause exe", "no friend in the Melty room", "no menu",
                                         "another UI pause", "Game Over / DOWNED", "midnight / a Blood Moon on its way",
                                         "A / X in the pouch", "a call was refused / no Link"};
typedef struct {
    bool key, exe_ok, room, dead, night, held, failed;
    bool active;  // it ran at the last mod frame
    uint32_t ps;  // the UI's pause state now
} NpIn;

static inline bool np_menu_ps(uint32_t ps) { return ps == NP_PS_MENU || ps == NP_PS_QUICK; }
// Only a menu value starts it. Another UI value that comes while it runs (over the menu, or as the menu closes) keeps it
// running as PR #294 runs the world for every UI pause but a load's: a stop laid on that pause might outlast it (np_owed).
static NpWhy np_decide(const NpIn *in) {
    if (!in->key) return NP_KEY;
    if (!in->exe_ok) return NP_NOEXE;
    if (!in->room) return NP_NOROOM;
    if (!in->ps || (in->ps & NP_PS_LOAD)) return NP_NOMENU;
    if (!np_menu_ps(in->ps) && !in->active) return NP_OTHERUI;
    if (in->dead) return NP_DEAD;
    if (in->night) return NP_NIGHT;
    if (in->held) return NP_HELD;
    if (in->failed) return NP_FAILED;
    return NP_ON;
}
// A world let run during this menu that no longer runs: 1 = stop it again now, as the menu stopped it (a menu value is up: the
// menu's own resume undoes it as it closes, as ever); 0 = the stop stays owed (another UI value is up: a stop laid on a pause the
// menu's resume never sees could outlast it); -1 = settled (no UI pause: the menu's own resume ran; a load: the game's own way)
static inline int np_owed(uint32_t ps) { return !ps || (ps & NP_PS_LOAD) ? -1 : np_menu_ps(ps) ? 1 : 0; }
// the hour (0..24, -1 = not known) is within NP_NIGHT_H of midnight
static inline bool np_midnight(float h) { return h >= 24.0f - NP_NIGHT_H || (h >= 0 && h < NP_NIGHT_H); }
// the pouch rule's buttons: down now, not down at the last mod frame
static inline uint64_t np_newly(uint64_t now, uint64_t was) { return now & ~was & NP_AX_MASK; }
// (s290 labfix) who has the pad behind BotW's pause menu, from the KH2 camp's state (sora_camp_page.h): the camp (open / opening /
// closing: it eats the buttons), BotW (the camp closed: the map, a menu opened without the camp), or BotW after the camp's
// "BotW Menu" (CP_HANDOVER: the camp draws and eats nothing until the pause ends; the only way to BotW's pouch in this mod).
// Before s290 only "closed" counted, so the pouch rule never fired behind "BotW Menu" (lab s290 test 1 step 5).
enum { NP_PAD_CAMP, NP_PAD_BOTW, NP_PAD_HANDOVER };
// the pouch rule's new A / X this mod frame: raw = the pad's buttons now, pad / pad_was = who has the pad now / at the last mod
// frame, *was = the buttons kept at the last mod frame (updated). A button still down from the camp's own pick of "BotW Menu"
// (the edge from the camp into the handover) is the camp's, never a take; the camp's other ways out keep the old safe side (a
// still-held A given back to BotW's menu = a new press there: the world stops).
static inline uint64_t np_pad_newly(uint64_t raw, int pad, int pad_was, uint64_t *was) {
    uint64_t btn = pad != NP_PAD_CAMP ? raw & NP_AX_MASK : 0;
    if (pad == NP_PAD_HANDOVER && pad_was == NP_PAD_CAMP) *was = btn;
    uint64_t n = np_newly(btn, *was);
    *was = btn;
    return n;
}
// an enemy AS that starts a blow (PR: "Attack" or "Kick" in its name)
static inline bool np_attack_as(const char *as) { return as && (strstr(as, "Attack") || strstr(as, "Kick")); }
static inline bool np_big(const char *name) {
    return name && (strstr(name, "Moriblin") || strstr(name, "Lynel") || strstr(name, "Giant") || strstr(name, "Golem"));
}
// PR strike(): Link at `link` within the enemy's reach in front of it (m = its 3x4 row-major mMtx: translation m[3] m[7] m[11],
// facing column m[2] m[10]); d = the flat distance. Reach 2.2 m (big 3.5) + 0.8 for a weapon (GUESS: assumed held; the PR adds
// 1.5 for spears / long swords, 0.8 for any other weapon, 0 without one)
static bool np_in_reach(const float link[3], const float m[12], bool big, float *d_out) {
    float dx = link[0] - m[3], dy = link[1] - m[7], dz = link[2] - m[11];
    float d = sqrtf(dx * dx + dz * dz), fx = m[2], fz = m[10], fl = sqrtf(fx * fx + fz * fz);
    if (d_out) *d_out = d;
    float reach = (big ? 3.5f : 2.2f) + 0.8f;
    if (!(d <= reach) || dy > 2.5f || dy < -2.5f) return false;
    if (d > 0.8f && fl > 0 && (dx * fx + dz * fz) / (d * fl) < 0.3f) return false;  // (behind it, or to its side)
    return true;
}
// PR: an archer shooting at him: within 40 m, more than 0.5 m, facing him (cos > 0.9); returns the ms until it lands (1 frame
// + d at 40 m/s), 0 = not at him
static int np_arrow_ms(const float link[3], const float m[12], float *d_out) {
    float dx = link[0] - m[3], dz = link[2] - m[11], d = sqrtf(dx * dx + dz * dz);
    float fx = m[2], fz = m[10], fl = sqrtf(fx * fx + fz * fz);
    if (d_out) *d_out = d;
    if (!(d < NP_ARROW_M) || d <= 0.5f || !(fl > 0) || (dx * fx + dz * fz) / (d * fl) <= 0.9f) return 0;
    return 17 + (int)(d / NP_ARROW_MPS * 1000.0f);
}
// a blow's new life: down by dmg, never below 1 (no death inside a menu); 1 or less = no change
static inline int np_blow(int life, int dmg) {
    if (life <= 1 || dmg <= 0) return life;
    return life - dmg >= 1 ? life - dmg : 1;
}
// (s290 rv3fix, review 3 #6) the enemy we follow is still the one we saw: BotW's proc unit of an actor (actor +0x68) points back at
// it (unit +0x48) and holds its id (unit +0x40 == actor +0x60, the id the entry was made with), and it is not being deleted
// (actor +0x64 != 3): the same 4 tests as no_alive in sora_net_objs.h. An actor deleted behind the menu (a friend's kill) whose
// memory is used again fails them (another id / unit / nothing), and its entry goes.
static inline bool np_unit_ok(uint64_t actor, uint64_t unit_actor, uint32_t unit_id, uint32_t actor_id, uint32_t watch_id, uint8_t state) {
    return actor && unit_actor == actor && unit_id == actor_id && actor_id == watch_id && state != 3;
}
// (s290 rv3fix, review 3 #6) an actor's name copied with a bound: at most NP_NAME_MAX - 1 letters, the first byte and every
// 4 KB page after it checked by ok(addr, 1) (the exe's name is read up to its NUL otherwise: a name with no NUL up to an
// unmapped page would fault). No name / unreadable = "".
#define NP_NAME_MAX 48
static inline void np_copy_name(const char *p, char out[NP_NAME_MAX], bool (*ok)(uint64_t, size_t)) {
    out[0] = 0;
    uint64_t a = (uint64_t)(uintptr_t)p;
    if (!p || !ok) return;
    int i = 0;
    for (; i < NP_NAME_MAX - 1; i++) {
        if ((i == 0 || ((a + (uint64_t)i) & 0xfff) == 0) && !ok(a + (uint64_t)i, 1)) break;
        char ch = p[i];
        if (!ch) break;
        out[i] = ch;
    }
    out[i] = 0;
}
// a live key's value as kept: netnopausedmg 0..120 quarter hearts, netnopausestep 1..2
static inline int np_clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

#ifndef NP_HOST
// ------------------------------------------------------------------ game glue (sora.c, after sora_camp.h; decls in sora_net.h)
// Uses: SwApi (sw_api.h), NET / PEER_LIVE / sora_melty_connected (sora_net.h), CAMP (sora_camp.h), g_held, PAUSE_STATE via
// sora_sites(), vk_frame_counter, drive_off, link_actor, rd_ok, linkpos_link_life, linkpos_link_pos, linkpos_enemies, LOG (sora.c),
// sora_world_bloodmoon_busy (sora_world.h, declared at the top of sora.c; read only).
#include "sw_api.h"
static int g_np_key = 1;               // live netnopause (default ON since s291: two-copy lab s290 + s291 PASS, user)
static int g_np_log = 0;               // live netnopauselog
static int g_np_dmg = NP_DMG_DEFAULT;  // live netnopausedmg
static int g_np_step = 1;              // live netnopausestep

#define NP_TRACK 64
typedef struct {
    uint64_t actor, seen;        // the enemy, the present it was last followed
    uint32_t id;                 // (s290) its watch id: actor +0x60 when it was first followed (np_unit_ok)
    char as[32];                 // its slot-0 AS as last read
    uint64_t blow_at, arrow_at;  // ms its blow / its arrow lands (0 none)
    float arrow_d;
    bool shot;                   // its ArrowShoot already landed once
} NpEn;
static struct {
    bool tried, bound;
    const SwApi *api;
    uint64_t bpm_ptr, bits_set, bits_clear, procs, phys_pause, phys_got, scene_got;
    void (*exe_set)(int world, int live);
    void (*exe_step)(int div);
    void (*exe_counts)(uint64_t out[3]);  // the exe's writes of the scene state: zeroed / held / back (runtime/nopause.c)
    float (*hour)(bool *held);
    uint64_t (*vfr_count)(void);
    bool active, held, failed, night; // the world runs now / the pouch rule / a refused call / midnight (all until the menu goes)
    bool owed;                        // it ran during this menu and is not stopped again yet (np_owed)
    uint64_t btn_was;                 // A / X at the last mod frame
    int pad_was;                      // (s290 labfix) who had the pad at the last mod frame (NP_PAD_*; 0 = the camp)
    uint32_t ps_said[8], ps_ran[8];   // other UI pause values already logged (not started there / kept running there)
    uint64_t last_present;            // of the last blow scan
    uint32_t resumes, stops, blows, hits, menus, gone;  // (gone, s290: enemies dropped, the unit tests failed)
    uint64_t next_sec, sec_vfr, sec_present, sec_cnt[3];  // the 1 s line
    float hour_was, sec_pos[3];
    uint64_t sec_actor, near_actor;
    int listed;                       // linkpos's enemy list at the last blow scan (fed by the drawn actors' opacity hook)
    NpEn en[NP_TRACK];
} NOPA;

static bool np_bind(void) {
    if (NOPA.tried) return NOPA.bound;
    NOPA.tried = true;
    HMODULE exe = GetModuleHandleA(NULL);
    const SwApi *(*get)(uint32_t) = (const SwApi *(*)(uint32_t))(void *)GetProcAddress(exe, SW_API_EXPORT);
    const SwApi *a = NOPA.api = get ? get(1) : NULL;
    NOPA.exe_set = (void (*)(int, int))(void *)GetProcAddress(exe, "sw_nopause_set");
    NOPA.exe_step = (void (*)(int))(void *)GetProcAddress(exe, "sw_nopause_step");
    NOPA.exe_counts = (void (*)(uint64_t *))(void *)GetProcAddress(exe, "sw_nopause_counts");
    NOPA.hour = (float (*)(bool *))(void *)GetProcAddress(exe, "test_warp_hour");
    NOPA.vfr_count = (uint64_t (*)(void))(void *)GetProcAddress(exe, "framepace_game_frame");
    bool api_ok = SW_HAS(a, call) && SW_HAS(a, site) && SW_HAS(a, in_frame) && SW_HAS(a, actor_name);
    if (api_ok) {
        NOPA.bpm_ptr = a->site("np_bpm_ptr"), NOPA.bits_set = a->site("np_bits_set"), NOPA.bits_clear = a->site("np_bits_clear");
        NOPA.procs = a->site("np_procs"), NOPA.phys_pause = a->site("np_phys_pause"), NOPA.scene_got = a->site("np_scene_got");
        NOPA.phys_got = a->site("phys_system_got");
    }
    // (the s289 nopause exe: it writes the scene state at its VFR update, runtime/nopause.c; the game library is stock)
    bool exe_ok = NOPA.exe_set && NOPA.exe_counts;
    bool state = api_ok && a->site("np_scene_state") && a->site("np_pause_mgr");  // (the exe's rows for this game version)
    NOPA.bound = api_ok && exe_ok && state && NOPA.bpm_ptr && NOPA.bits_set && NOPA.bits_clear && NOPA.procs && NOPA.phys_pause &&
               NOPA.phys_got && NOPA.scene_got && sora_sites()->pause_state;
    if (!NOPA.bound)
        LOG("[nopause] refused: %s: the world stops in menus as ever\n",
            !api_ok ? "this exe has no sw_api (before s175)"
            : !exe_ok ? "NEEDS THE s289 nopause EXE (runtime/nopause.c: sw_nopause_set + sw_nopause_counts, the np_* rows of sora_sites.h)"
                      : "a site is 0 on this game version (1.9.0 only)");
    return NOPA.bound;
}

// a Melty room with another player in it (his slot used, his peer live)
static bool np_room(void) {
    if (!NET.on || !NET.melty || !sora_melty_connected()) return false;
    for (int k = 0; k < NET_MAX_PEERS; k++)
        if (NET.slot[k].used && NET.slot[k].p.st == PEER_LIVE) return true;
    return false;
}

static uint64_t np_u64(uint64_t a) { return a && rd_ok(a, 8) ? *(const uint64_t *)(uintptr_t)a : 0; }
static uint64_t np_bpm(void) { return np_u64(NOPA.bpm_ptr); }
static uint64_t np_phys(void) { return np_u64(np_u64(NOPA.phys_got)); }
static uint64_t np_scene(void) { return np_u64(np_u64(NOPA.scene_got)); }
static uint16_t np_bits(uint64_t mgr) { return mgr && rd_ok(mgr + 0x168, 2) ? *(const uint16_t *)(uintptr_t)(mgr + 0x168) : 0; }
static bool np_call(uint64_t fn, uint64_t x0, uint64_t x1, uint64_t x2, int nx) {
    SwArgs a = {.x = {x0, x1, x2}, .nx = (uint8_t)nx};
    SwRet r = {0};
    return NOPA.api->in_frame() && NOPA.api->call(NULL, fn, &a, &r);
}
static void np_scene_byte(uint8_t v) {
    uint64_t s = np_scene();
    if (s && rd_ok(s + 0x8b8, 1)) *(volatile uint8_t *)(uintptr_t)(s + 0x8b8) = v;
}
// the menu's own resume for the world alone (PR, 1.9.0 0x7000d462d0's world part); false = a call refused
static bool np_resume_world(void) {
    uint64_t mgr = np_bpm(), phys = np_phys();
    bool ok = mgr != 0;
    if (ok && phys) ok = np_call(NOPA.phys_pause, phys, 0, 0, 3);
    if (ok) ok = np_call(NOPA.bits_clear, mgr, 0x7f, 0, 2) && np_call(NOPA.procs, mgr, 0, 0, 1);
    if (ok) np_scene_byte(0), NOPA.resumes++;
    return ok;
}
// stop it again as the menu stopped it as it opened: the physics (1.9.0 0x7000c40454: (system, 1, 0)), the bits (0x7000d46240:
// (mgr, 0x7f)), the manager's +0x163 = 1 (0x7000d46248: the game writes it after every bits set, 1 for the menus; the PR called
// 0x7001519720 here, which only clears it: VERIFIED static, sb/s289/nopause/call_args.py), the scene's paused byte 1
static void np_stop_world(const char *why) {
    uint64_t mgr = np_bpm(), phys = np_phys();
    bool ok = true;
    if (phys) ok = np_call(NOPA.phys_pause, phys, 1, 0, 3);
    if (mgr) {
        bool set = np_call(NOPA.bits_set, mgr, 0x7f, 0, 2);
        if (set && rd_ok(mgr + 0x163, 1)) *(volatile uint8_t *)(uintptr_t)(mgr + 0x163) = 1;
        ok = set && ok;
    }
    np_scene_byte(1);
    NOPA.stops++;
    if (g_np_log || !ok) LOG("[nopause] the world stops again behind the menu (%s)%s\n", why, ok ? "" : ": a call was REFUSED");
}

// ---- blows behind the menu (PR own_strikes_in_menu) ----
// the AS an actor plays in an ASList slot (its first bank with one: linkpos_esync_block.inc esx_as_read's walk, guest memory
// only, no call); false = none / unreadable
static bool np_as_name(uint64_t actor, int slot, char out[32]) {
    out[0] = 0;
    uint64_t asl = np_u64(actor + 0x560), none_got = sora_sites()->aslist_none_got;
    if (!asl || (none_got && asl == np_u64(none_got)) || !rd_ok(asl, 0xd0)) return false;
    int32_t ns = *(const int32_t *)(uintptr_t)(asl + 0xb8);
    uint64_t sp = *(const uint64_t *)(uintptr_t)(asl + 0xc0) + (uint64_t)slot * 0x50;
    if (slot < 0 || slot >= ns || ns > 16 || !rd_ok(sp, 0x30)) return false;
    int32_t nb = *(const int32_t *)(uintptr_t)(sp + 0x20);
    uint64_t banks = *(const uint64_t *)(uintptr_t)(sp + 0x28);
    if (nb <= 0 || nb > 16) return false;
    for (int b = 0; b < nb; b++) {
        uint64_t be = banks + (uint64_t)b * 0x98;
        if (!rd_ok(be, 0x48)) return false;
        if (!*(const uint64_t *)(uintptr_t)(be + 0x18)) continue;
        uint64_t np = np_u64(np_u64(be) + 0x10);
        if (!np) continue;
        bool whole = rd_ok(np, 32);
        int i = 0;
        for (; i < 31 && (whole || rd_ok(np + (uint64_t)i, 1)); i++) {
            char ch = *(const char *)(uintptr_t)(np + (uint64_t)i);
            if (!ch) break;
            out[i] = ch;
        }
        out[i] = 0;
        if (i > 0) return true;
    }
    return false;
}
static bool np_guarding(void) {  // (PR: Link's own AS, upper body or whole, with "Guard" in it: his shield takes the blow)
    uint64_t a = link_actor();
    char as[32];
    for (int s = 0; s < 2 && a; s++)
        if (np_as_name(a, s, as) && strstr(as, "Guard")) return true;
    return false;
}
static uint32_t np_u32(uint64_t a) { return a && rd_ok(a, 4) ? *(const uint32_t *)(uintptr_t)a : 0; }
static uint8_t np_u8(uint64_t a) { return a && rd_ok(a, 1) ? *(const uint8_t *)(uintptr_t)a : 0; }
// (s290) the enemy's proc unit and id read now (every read checked) -> its id (0 = none) and whether the unit tests hold at once
static bool np_unit_now(uint64_t actor, uint32_t *id) {
    uint64_t unit = np_u64(actor + 0x68);
    *id = np_u32(actor + 0x60);
    return np_unit_ok(actor, np_u64(unit + 0x48), np_u32(unit + 0x40), *id, *id, np_u8(actor + 0x64));
}
static bool np_ok_adapter(uint64_t a, size_t n) { return rd_ok(a, n); }
static NpEn *np_en(uint64_t actor, uint64_t f, uint32_t id) {
    NpEn *free_e = NULL, *old = &NOPA.en[0];
    for (int i = 0; i < NP_TRACK; i++) {
        NpEn *e = &NOPA.en[i];
        if (e->actor == actor && e->id == id) return e;
        if (e->actor == actor) { memset(e, 0, sizeof *e); free_e = e; break; }  // (same memory, another enemy: afresh)
        if (!e->actor || f - e->seen > 30) { if (!free_e) free_e = e; }
        else if (e->seen < old->seen) old = e;
    }
    NpEn *e = free_e ? free_e : old;
    memset(e, 0, sizeof *e);
    e->actor = actor, e->id = id;
    return e;
}
// the blow lands: Link's life (+0x848, quarter hearts) down, never below 1 (np_blow), only while it is what linkpos read
static void np_hit(const char *who, int dmg, const char *how, float d) {
    NOPA.blows++;
    if (np_guarding()) {
        if (g_np_log) LOG("[nopause] %s's %s behind the menu at %.1f m: blocked (Link guards)\n", who, how, d);
        return;
    }
    int cur = linkpos_link_life(), v = np_blow(cur, dmg);
    uint64_t a = link_actor();
    if (v == cur || !a || !rd_ok(a + 0x848, 4) || *(const int32_t *)(uintptr_t)(a + 0x848) != cur) {
        if (g_np_log) LOG("[nopause] %s's %s behind the menu at %.1f m: no damage (life %d: never the last quarter heart here)\n", who, how, d, cur);
        return;
    }
    *(int32_t *)(uintptr_t)(a + 0x848) = v;
    NOPA.hits++;
    if (g_np_log) LOG("[nopause] %s's %s hit us behind the menu at %.1f m: -%d (life %d -> %d)\n", who, how, d, cur - v, cur, v);
}
static void np_blows(uint64_t f, uint64_t now) {
    float lp[3];
    if (f == NOPA.last_present || !linkpos_link_pos(lp)) return;  // (once a present: the paused game updates twice a present)
    NOPA.last_present = f;
    static uint64_t actor[64];
    static float pos[64][3];
    static int32_t life[64];
    int n = linkpos_enemies(actor, pos, life, 64);
    float best = 1e9f;
    uint64_t nearest = 0;
    for (int i = 0; i < n; i++) {
        if (life[i] <= 0 || !actor[i]) continue;
        float dx = pos[i][0] - lp[0], dz = pos[i][2] - lp[2], d2 = dx * dx + dz * dz;
        if (!(d2 <= NP_NEAR_M * NP_NEAR_M) || !rd_ok(actor[i] + 0x398, 48)) continue;
        uint32_t id = 0;
        if (!np_unit_now(actor[i], &id)) {  // (s290: not the enemy we saw / being deleted: its entry goes, nothing is read of it)
            for (int j = 0; j < NP_TRACK; j++)
                if (NOPA.en[j].actor == actor[i]) memset(&NOPA.en[j], 0, sizeof NOPA.en[j]);
            NOPA.gone++;
            continue;
        }
        if (d2 < best) best = d2, nearest = actor[i];
        NpEn *e = np_en(actor[i], f, id);
        e->seen = f;
        char as[32];
        if (!np_as_name(actor[i], 0, as)) continue;
        char nm[NP_NAME_MAX];
        np_copy_name(NOPA.api->actor_name(actor[i]), nm, np_ok_adapter);
        float m[12];
        memcpy(m, (const void *)(uintptr_t)(actor[i] + 0x398), sizeof m);
        if (strcmp(as, e->as)) {
            if (g_np_log) LOG("[nopause] behind the menu: %s plays %s\n", nm, as);
            if (np_attack_as(as)) e->blow_at = now + NP_STRIKE_MS;
            snprintf(e->as, sizeof e->as, "%s", as);
        }
        if (e->blow_at && now >= e->blow_at) {
            float d = 0;
            bool big = np_big(nm);
            e->blow_at = 0;
            if (np_in_reach(lp, m, big, &d)) np_hit(nm, big ? 2 * g_np_dmg : g_np_dmg, "blow", d);
            else if (g_np_log) LOG("[nopause] %s's blow behind the menu missed (%.1f m, or not in front of it)\n", nm, d);
        }
        bool shoot = !strcmp(as, "ArrowShoot");
        if (!shoot) e->shot = false;
        float ad = 0;
        int ms = shoot && !e->arrow_at ? np_arrow_ms(lp, m, &ad) : 0;
        if (ms) e->arrow_at = now + (uint64_t)ms, e->arrow_d = ad;
        if (e->arrow_at && now >= e->arrow_at) {
            e->arrow_at = 0;
            if (!e->shot) e->shot = true, np_hit(nm, g_np_dmg, "arrow", e->arrow_d);
        }
    }
    NOPA.near_actor = nearest, NOPA.listed = n;
}

// the 1 s line while the world runs (netnopauselog): the hour going on, the nearest enemy moving, the VFR updates per present
static void np_second(uint64_t now, uint64_t f) {
    if (now < NOPA.next_sec) return;
    bool first = !NOPA.next_sec;
    NOPA.next_sec = now + 1000;
    bool held = false;
    float h = NOPA.hour ? NOPA.hour(&held) : -1, moved = -1;
    uint64_t vfr = NOPA.vfr_count ? NOPA.vfr_count() : 0, c[3] = {0, 0, 0};
    if (NOPA.exe_counts) NOPA.exe_counts(c);  // (the exe's scene state writes: zeroed about as often as held = the world's reads saw 0)
    if (!first && NOPA.sec_actor && rd_ok(NOPA.sec_actor + 0x398, 48)) {
        const float *m = (const float *)(uintptr_t)(NOPA.sec_actor + 0x398);
        float dx = m[3] - NOPA.sec_pos[0], dz = m[11] - NOPA.sec_pos[2];
        moved = sqrtf(dx * dx + dz * dz);
    }
    char secnm[NP_NAME_MAX] = "no enemy near";
    if (NOPA.sec_actor) np_copy_name(NOPA.api->actor_name(NOPA.sec_actor), secnm, np_ok_adapter);
    if (g_np_log && !first) {
        float dh = h - NOPA.hour_was;
        if (dh < -12) dh += 24;  // (midnight)
        LOG("[nopause] 1 s behind the menu: hour %.3f (%+.4f; play = +0.0167%s), %s moved %.2f m, %d enemies listed, VFR updates %.2f a present (step /%d), pause bits %#x, resumes %u, blows %u, hits %u, scene state zeroed %llu held %llu\n",
            h, dh, held ? ", HELD by the room clock: a guest" : "", secnm,
            moved, NOPA.listed, f > NOPA.sec_present ? (double)(vfr - NOPA.sec_vfr) / (double)(f - NOPA.sec_present) : 0.0, g_np_step,
            np_bits(np_bpm()), NOPA.resumes, NOPA.blows, NOPA.hits, (unsigned long long)(c[0] - NOPA.sec_cnt[0]),
            (unsigned long long)(c[1] - NOPA.sec_cnt[1]));
    }
    NOPA.hour_was = h, NOPA.sec_vfr = vfr, NOPA.sec_present = f;
    memcpy(NOPA.sec_cnt, c, sizeof c);
    NOPA.sec_actor = NOPA.near_actor;
    if (NOPA.sec_actor && rd_ok(NOPA.sec_actor + 0x398, 48)) {
        const float *m = (const float *)(uintptr_t)(NOPA.sec_actor + 0x398);
        NOPA.sec_pos[0] = m[3], NOPA.sec_pos[1] = m[7], NOPA.sec_pos[2] = m[11];
    } else NOPA.sec_actor = 0;
}

// once a mod frame (sora_net.h net_frame; net_sora_off with feet NULL): true = the world runs behind a menu now and feet =
// Link's body position (the net ticks with it). Key 0 and not running: returns at once.
static bool np_frame(uint64_t now, float feet[3]) {
    int pad = CAMP.page.state == CP_CLOSED ? NP_PAD_BOTW : CAMP.page.state == CP_HANDOVER ? NP_PAD_HANDOVER : NP_PAD_CAMP;  // (s290 labfix)
    uint64_t newly = np_pad_newly(g_held, pad, NOPA.pad_was, &NOPA.btn_was);
    NOPA.pad_was = pad;
    if (!g_np_key && !NOPA.active && !NOPA.owed) return false;  // (key 0: nothing read, called or written)
    uint32_t ps = sora_sites()->pause_state ? *(volatile uint32_t *)(uintptr_t)sora_sites()->pause_state : 0;
    if (!ps) NOPA.held = NOPA.failed = NOPA.night = false;  // (the menu went: the next one starts afresh)
    NpIn in = {.key = g_np_key != 0, .exe_ok = np_bind(), .room = feet && np_room(), .dead = NET.dead || NET.downed,
               .held = NOPA.held, .failed = NOPA.failed, .active = NOPA.active, .ps = ps};
    if (!NOPA.night && in.key && in.exe_ok && in.room && (np_menu_ps(ps) || NOPA.active)) {  // (midnight: see the top)
        bool hh = false;
        float hr = NOPA.hour ? NOPA.hour(&hh) : -1;
        bool bm = sora_world_bloodmoon_busy();
        if (bm || np_midnight(hr)) {
            NOPA.night = true;
            if (g_np_log) LOG("[nopause] %s (hour %.3f): the world stays stopped behind this menu (BotW never crosses midnight in one)\n",
                              bm ? "a Blood Moon of the room's on its way" : "midnight", hr);
        }
    }
    in.night = NOPA.night;
    NpWhy why = np_decide(&in);
    if (why == NP_ON && ps == NP_PS_MENU && newly) {
        NOPA.held = true, why = NP_HELD;
        if (g_np_log) LOG("[nopause] A or X with the pause menu up (the pouch's take): the world stops until the menu goes\n");
    }
    if (ps && !(ps & NP_PS_LOAD) && !np_menu_ps(ps) && in.key && in.exe_ok && in.room && g_np_log) {  // (each other UI value once)
        bool run = why == NP_ON;
        uint32_t *said = run ? NOPA.ps_ran : NOPA.ps_said;
        int q = 0;
        while (q < 8 && said[q] && said[q] != ps) q++;
        if (q < 8 && !said[q]) {
            said[q] = ps;
            LOG(run ? "[nopause] the UI's pause %#x came while the world runs behind the menu: it keeps running (PR #294's way)\n"
                    : "[nopause] the UI's pause %#x is not a menu: the world stops there as ever\n", ps);
        }
    }
    if (why == NP_ON && !linkpos_link_pos(feet)) why = NP_FAILED, NOPA.failed = true;  // (no Link body: nothing to stand for him)
    uint64_t f = vk_frame_counter();
    bool ran = NOPA.active;  // (the world was let run: this frame's resume counts too, even a half-refused one)
    if (why == NP_ON) {
        if (!NOPA.active) {
            NOPA.menus++, NOPA.next_sec = 0, NOPA.near_actor = NOPA.sec_actor = 0, NOPA.resumes = NOPA.blows = NOPA.hits = 0;
            memset(NOPA.en, 0, sizeof NOPA.en);
            if (g_np_log) LOG("[nopause] on: the UI's pause %#x, a friend in the room: the world runs behind the menu (menu %u)\n", ps, NOPA.menus);
        }
        drive_off();  // (the servo lets go: the physics runs, vel_step would push him at Sora's last speed)
        NOPA.exe_set(1, 0);
        ran = true;
        if ((np_bits(np_bpm()) & 1) && !np_resume_world()) {
            NOPA.failed = true, why = NP_FAILED;
            LOG("[nopause] a resume call was REFUSED: the world stops again for this menu\n");
        }
    }
    if (why == NP_ON) {
        NOPA.active = true;
        np_blows(f, now);
        np_second(now, f);
        return true;
    }
    if (NOPA.exe_set) NOPA.exe_set(0, 0);
    if (ran) NOPA.owed = true;
    int o = NOPA.owed ? np_owed(ps) : -1;
    if (o > 0) np_stop_world(NP_WHY_TXT[why]);
    else if (!o && ran && g_np_log) LOG("[nopause] the UI's pause %#x is up: the world is stopped again once a menu value is back\n", ps);
    if (o) NOPA.owed = false;
    if (ran && g_np_log) LOG("[nopause] off: %s (%u resumes, %u blows, %u hits this menu)\n", NP_WHY_TXT[why], NOPA.resumes, NOPA.blows, NOPA.hits);
    NOPA.active = false;
    return false;
}

// sora_mod_unload (inside linkpos_frame): never leave the world running for a DLL copy that is gone
static void np_unload(void) {
    if (NOPA.exe_step) NOPA.exe_step(1);
    if (!NOPA.active && !NOPA.owed) return;
    uint32_t ps = sora_sites()->pause_state ? *(volatile uint32_t *)(uintptr_t)sora_sites()->pause_state : 0;
    if (NOPA.exe_set) NOPA.exe_set(0, 0);  // (the exe's permission also ends by itself 2 presents on)
    int o = np_owed(ps);
    if (o > 0) np_stop_world("unload");
    else if (!o) LOG("[nopause] unload with the UI's pause %#x up: the world is left as it is, the menu's own resume ends it\n", ps);
    NOPA.active = NOPA.owed = false;
}

// live keys (sora_net.h net_live_set); true = ours. Only a change logs (the live file re-applies every line on each rewrite).
static bool np_live_set(const char *key, int v) {
    if (!strcmp(key, "netnopause")) {
        if (g_np_key == (v != 0)) return true;
        g_np_key = v != 0;
        bool ok = !g_np_key || np_bind();
        LOG("[nopause] netnopause %d: %s\n", g_np_key,
            !g_np_key ? "BotW's and KH2's menus stop the world as ever"
            : ok ? "in a Melty room with a friend, the pause menu / map / quick menus / KH2 camp leave the world running"
                 : "REFUSED (see the line above): nothing changes");
    } else if (!strcmp(key, "netnopauselog")) {
        if (g_np_log != (v != 0)) LOG("[nopause] netnopauselog %d\n", v != 0);
        g_np_log = v != 0;
    } else if (!strcmp(key, "netnopausedmg")) {
        int q = np_clamp(v, 0, 120);
        if (q != g_np_dmg) LOG("[nopause] netnopausedmg %d: a blow behind the menu takes %d quarter heart(s), big enemies %d\n", q, q, 2 * q);
        g_np_dmg = q;
    } else if (!strcmp(key, "netnopausestep")) {
        int s = np_clamp(v, 1, 2);
        if (s == g_np_step) return true;
        np_bind();
        if (!NOPA.exe_step) { LOG("[nopause] netnopausestep %d REFUSED: this exe has no sw_nopause_step\n", s); return true; }
        g_np_step = s;
        NOPA.exe_step(s);
        LOG("[nopause] netnopausestep %d: %s\n", s, s > 1 ? "the paused step halved while the world runs behind a menu" : "the game's paused step as ever");
    } else return false;
    return true;
}
#endif  // NP_HOST

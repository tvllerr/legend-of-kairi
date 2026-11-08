// (s67 drive) KH2's Drive gauge for Sora in BotW, Drive Forms slice 1: the gauge logic (DLL-only, hot reload).
// Design: D:/SoraBotW/recon/S64_HELPER_DRIVE_RECON.md sec 2 (rules) + 6 (slice 1); KH2's rules as PHH's status_kh2.py
// ports them (re:0x3C0AD0 drive_add, re:0x3A9CF0 drive_gain, re:0x3A8800 hit_dealt, re:0x3DCA50 prize, re:0x3C2780
// spend); constants CONFIRMED in kh2_first/original/03system.bin (DriveEnemy 0.75 @0x183FC, MpDriveRate 2 @0x18414).
//
// Gauge (normal state, drive_state 1): whole bars + fill 0..99 of the next one (100 points = 1 bar); a full gauge =
// bars == max and fill 100. Max = 3 bars (KH2's new-game gauge; growth to 9 = a later slice), 9 under the levels'
// dev override (live "set devlv N", sora_levels.h).
// (s89 trial, USER DECISION 2026-11-02) the max grows KH2-style by the Drive Forms: base 3 + 1 per form level gained
// (Valor / Wisdom / Limit / Master / Final LV2..7 = sum of (LV - 1)), cap 9 (KH2 BONUS_DRIVE_MAX, re:0x3EDF90). Derived
// from the saved form EXPs (no new save field). Source check: in KH2 itself the max grows only by the Get Bonus rows'
// DriveGaugeUpgrade (00battle.bin bons, PHH status_kh2 re:0x3EDF90); fmlv rows (8 B: form<<4|lv, growth lv, ability,
// EXP; VERIFIED s89 review) carry no Drive field - BotW has no bonus levels, so the form levels stand in (the user's
// rule). A max that rises re-runs drive_add(0) as KH2 does after a bonus (a full gauge stays full). Live "set dggrow 0" =
// the old fixed 3, LOG "[drive] max ... bars".
// Gains (drive_gain): x MpDriveRate 2 during MP Charge (x Drive Boost 1.0: no ability yet), x the area rate 1.0,
// x g_dg_mul (live "set drivemul <x100>", default 100 = KH2), truncated to whole points as KH2 does.
//   Hit (hit_dealt): the atkp row's DriveDrain (file:00battle.bin atkp, byte +41, rows by Id; table DG_DRAIN below),
//   x DriveEnemy 0.75 for a common enemy (KH2's ACTOR_LESS_DRIVE; every BotW enemy but a boss here). Blade + spells.
//   Kill: Drive orbs through KH2's prize rule (a small orb = 1 point, a big one 3: status_kh2.PRIZE kinds 7 / 8);
//   KH2's przt rows drop 0-4 per enemy, so per BotW kill (GUESS, by the levels table's KH HP percent): small
//   (pct <= 100) 2 small orbs = 2, medium (<= 150) 2 big = 6, big / boss 4 big = 12, animals that do not fight 0.
// HUD: the exe's sora_hud_drive (KH2's gauge, data/hud/drive.bin) by GetProcAddress; an exe without it = log only.
// SEs: drive_bar (se000 #32) when a bar fills, drive_full (#33) when the gauge becomes full.
// Saved: the levels sidecar's "drive" (bars x 100 + fill) per BotW slot; hot reload: the process env (DG_ENV).
// Live: drive <bars> (fill 0; 3 at max = full) | drivept <points> (add, may be < 0) | drivemul <x100> | drivelog 0/1.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sora_win.h"  // (<windows.h> on Windows; Linux shims: Deck port)

#define DG_ENV "SORA_KH2_DRIVE_STATE"  // (process env: survives the DLL hot reload, not a game restart)
#define DG_MAX_BASE 3                   // KH2 new game (PHH START_DRIVE_MAX)
#define DG_MAX_DEV 9                    // KH2's BONUS_DRIVE_MAX (re:0x3EDF90)
#define DG_DRIVE_ENEMY 0.75f            // sstm DriveEnemy (03system.bin @0x183FC)
#define DG_MP_DRIVE_RATE 2.0f           // sstm MpDriveRate (@0x18414)

static bool mp_charging(void);  // (sora.c's MP Charge, defined after this header)
static bool mk_gauge(int *bars, int *fill, int *max);  // (s110m mickeyfx: sora_mickey.h; his own Drive gauge while he is out)
static bool fm_gauge(int *bars, int *fill, int *max);
static int smn_gauge_state(void);  // (s137 smenu)  // (s68 sora_form.h: the form gauge while in a Drive Form)
static bool fm_in_form(void);

// DriveDrain of the atkp rows Sora's blade (sora_hits.bin windows) and spells use (file:kh2_first/original/00battle.bin
// atkp @0x150: v6, 2713 rows of 0x30, SubId 0; read s67). A row not here gives no Drive (logged once per row).
static const struct { uint16_t atkp; uint8_t drain; } DG_DRAIN[] = {
    {126, 4}, {127, 3}, {128, 3}, {147, 1}, {130, 3}, {129, 4},  // A300-A304 (+ 147)
    {131, 4}, {132, 4}, {133, 4}, {134, 3},                      // A330-A333
    {170, 3}, {890, 4}, {1007, 1}, {1898, 1}, {1899, 3}, {968, 3}, {975, 3}, {876, 4}, {1896, 1}, {1897, 3},
    {1582, 3}, {1583, 3}, {909, 3}, {1131, 3}, {963, 1}, {967, 3}, {915, 3}, {916, 1}, {917, 1}, {927, 3}, {1882, 3},
    {248, 6}, {250, 5}, {502, 4},                                // Blizzard, Thunder, Fire (Lv1 rows)
    // (s88 spell tiers) the Lv2 / Lv3 rows (SubId = level << 9): Blizzard 248 (512 / 1024) = 6 and Thunder 250 = 5 at
    // EVERY level (same as Lv1: no extra keys needed), Fire 502: Lv2 ticks 512 / 513 = 4 / 3 (514 = 1 exists too), last
    // 515 = 4; Lv3 ticks 1024 (two rows, kb 1 / kb 5) = 3 / 1, last 1028 = 4. Which row KH2's script picks per wave is
    // not decoded: GUESS = the k-th wave takes the k-th row. Pseudo keys 50200 + (Lv - 2) * 10 + wave (sora.c spell_hit):
    {50200, 4}, {50201, 3}, {50202, 4},                          // Fira wave 0 / 1 / last
    {50210, 3}, {50211, 1}, {50212, 4},                          // Firaga wave 0 / 1 / last
    // (s133 magic) Magnet 658 and Reflect's burst rows 574 / 576 / 578 (Power 50) + 667 / 668 / 669 (Power 550), DriveDrain byte
    // (+41) of 00battle.bin atkp SubId 0 / 512 / 1024 (all three the same): 658 = 5, the Reflect rows = 8
    {658, 5}, {574, 8}, {576, 8}, {578, 8}, {667, 8}, {668, 8}, {669, 8},
};

static struct {
    bool init;
    int bars, fill;   // KH2 status +0x1B1 / +0x1B0
    int mul;          // gain x mul / 100 (live drivemul)
    int log;          // per-gain log lines
    uint32_t points;  // total points gained (log / test)
} DG = {.mul = 100, .log = 1};

#define DG_MAX_CAP 9                    // (s89 trial) the growth cap = KH2's BONUS_DRIVE_MAX
static int g_dg_grow = 1;               // (s89 trial) live "set dggrow 0" = the old fixed DG_MAX_BASE
static int fm_lv_bonus(void);           // (s89 trial, sora_form.h) sum of (form LV - 1), Valor .. Final
static int dg_max(void) {
    if (SL.devlv > 0) return DG_MAX_DEV;
    if (!g_dg_grow) return DG_MAX_BASE;
    int m = DG_MAX_BASE + fm_lv_bonus();
    return m > DG_MAX_CAP ? DG_MAX_CAP : m;
}
static bool dg_full(void) { return DG.bars >= dg_max() && DG.fill >= 100; }
static int dg_points(void) { return DG.bars * 100 + DG.fill; }  // (the sidecar's "drive")

static void dg_persist(void) {
    char b[96];
    snprintf(b, sizeof b, "v1 bars=%d fill=%d mul=%d log=%d points=%u", DG.bars, DG.fill, DG.mul, DG.log, DG.points);
    SetEnvironmentVariableA(DG_ENV, b);
}
static void dg_init(void) {
    if (DG.init) return;
    DG.init = true;
    char b[96];
    DWORD n = GetEnvironmentVariableA(DG_ENV, b, sizeof b);
    int ba = 0, fi = 0, mu = 100, lg = 1;
    unsigned pt = 0;
    if (n && n < sizeof b && sscanf(b, "v1 bars=%d fill=%d mul=%d log=%d points=%u", &ba, &fi, &mu, &lg, &pt) == 5) {
        DG.bars = ba, DG.fill = fi, DG.mul = mu, DG.log = lg, DG.points = pt;
        LOG("[drive] hot reload: %d bars + %d (max %d), mul %d%%\n", DG.bars, DG.fill, dg_max(), DG.mul);
    }
}

// a bar filled / the gauge became full: KH2's SEs (the HUD plays its own bar-up and full animations)
static void dg_changed(int bars0, bool full0, const char *why) {
    bool full = dg_full();
    if (full && !full0) sora_sfx_event("drive_full", 1.0f);
    else if (DG.bars > bars0) sora_sfx_event("drive_bar", 1.0f);
    if (DG.log || DG.bars != bars0 || full != full0)
        LOG("[drive] %s -> %d bars + %d (max %d)%s\n", why, DG.bars, DG.fill, dg_max(), full && !full0 ? " FULL" : DG.bars > bars0 ? " BAR" : "");
    dg_persist();
}

// re:0x3C0AD0 drive_add: fill += points with the bar carry; a full gauge holds fill 100
static void dg_add(int points, const char *why) {
    dg_init();
    int bars0 = DG.bars, v = DG.fill + points;
    bool full0 = dg_full();
    if (points < 0) {
        if (v < 0) {
            if (DG.bars > 0) DG.bars--, DG.fill = v + 100;
            else DG.fill = 0;
        } else DG.fill = v;
    } else {
        while (v > 99) {
            if (DG.bars >= dg_max()) { v = 100; break; }
            DG.bars++;
            if (DG.bars >= dg_max()) { v = 100; break; }
            v -= 100;
        }
        DG.fill = v;
        DG.points += (uint32_t)points;
    }
    dg_changed(bars0, full0, why);
}

// re:0x3A9CF0 drive_gain (normal state): x MpDriveRate during MP Charge, x the live mul, truncated
static void dg_gain(float x, const char *why) {
    if (fm_in_form()) return;  // (s68: in a form KH2's gains go to the form gauge = slice 3; none yet)
    if (sm_out()) return;      // (s136 review S2: a summon is out = KH2 drive state 3: no hidden Drive refill under the summon gauge; GUESS: KH2 may feed the summon gauge)
    if (mp_charging()) x *= DG_MP_DRIVE_RATE;
    x *= (float)DG.mul / 100.0f;
    int p = (int)x;
    if (p > 0) dg_add(p, why);
    else if (DG.log) LOG("[drive] %s: %.2f points -> 0 (truncated)\n", why, x);
}

// re:0x3A8800 hit_dealt: a hit Sora landed on actor (atkp row of the window / spell); a boss (the levels table's flag 1)
// is not "common": no DriveEnemy cut
static void dg_hit(int atkp, uint64_t actor) {
    bool boss = false;
    for (int i = 0; i < SL_NC; i++)
        if (SL.c[i].used && SL.c[i].actor == actor && SL.c[i].named) boss = (SL.c[i].flags & 1) != 0;
    for (size_t i = 0; i < sizeof DG_DRAIN / sizeof *DG_DRAIN; i++) {
        if (DG_DRAIN[i].atkp != atkp) continue;
        char why[48];
        snprintf(why, sizeof why, "hit atkp %d drain %d%s", atkp, DG_DRAIN[i].drain, boss ? " (boss)" : "");
        if (sm_out()) { sm_hit_refill((float)DG_DRAIN[i].drain * (boss ? 1.0f : DG_DRIVE_ENEMY)); return; }  // (s148 sumlv: KH2 state 3 = the hit feeds the summon gauge)
        dg_gain((float)DG_DRAIN[i].drain * (boss ? 1.0f : DG_DRIVE_ENEMY), why);
        return;
    }
    static int unknown[16], nunk;
    for (int i = 0; i < nunk; i++) if (unknown[i] == atkp) return;
    if (nunk < 16) unknown[nunk++] = atkp;
    LOG("[drive] atkp %d not in DG_DRAIN: no Drive for it\n", atkp);
}

// a kill (sora_levels.h sl_kill): Drive orbs by size (GUESS amounts, see the top); flags 1 boss, 2 animal
static void dg_kill(int pct, int flags) {
    if (flags & 2) return;
    int pts = (flags & 1) || pct > 150 ? 12 : pct > 100 ? 6 : 2;
    char why[48];
    snprintf(why, sizeof why, "kill orbs (pct %d%s)", pct, flags & 1 ? " boss" : "");
    dg_gain((float)pts, why);
}

// the sidecar's value (sl_slot_load): -1 = no "drive" in it (an old sidecar / a save without one) -> empty
static void dg_load_points(int p, const char *why) {
    dg_init();
    if (p < 0) p = 0;
    int mx = dg_max();
    DG.bars = p / 100, DG.fill = p % 100;
    if (DG.bars >= mx) DG.bars = mx, DG.fill = 100;
    LOG("[drive] %s: %d bars + %d (max %d)\n", why, DG.bars, DG.fill, mx);
    dg_persist();
}

// every driven frame with the HP gauge (sora.c hud_tick): the exe draws KH2's gauge
static bool roxcmd_on(void);  // (s111 roxcmd, sora.c)
static void dg_hud(void) {
    dg_init();
    static void (*fn)(int, int, int, int);
    static int tried;
    if (!tried) {
        tried = 1, fn = (void (*)(int, int, int, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_drive");
        LOG("[drive] HUD: sora_hud_drive %s\n", fn ? "found" : "not in this exe (log only until the next exe link)");
    }
    {  // (s89 trial) the max moved: a form level-up / a load / a New Game / dggrow / devlv
        static int last;
        int mx = dg_max();
        if (last && mx != last) {
            LOG("[drive] max %d -> %d bars (form levels +%d, dggrow %d, devlv %d)\n", last, mx, fm_lv_bonus(), g_dg_grow, SL.devlv);
            if (mx > last) dg_add(0, "drive max up");  // (KH2 re:0x3EDF90: drive_add(0) after the max rises)
        }
        last = mx;
    }
    if (DG.bars > dg_max()) DG.bars = dg_max(), DG.fill = 100;  // (devlv dropped back)
    int fb, ff, fmx;
    if (fn && roxcmd_on()) fn(0, DG.bars, DG.fill, dg_max());  // (s111 roxcmd: KH2's Roxas has no Drive gauge; the values keep running)
    else if (fn && mk_gauge(&fb, &ff, &fmx)) fn(1, fb, ff, fmx);  // (s110m: Mickey's gauge)
    else if (fn && fm_gauge(&fb, &ff, &fmx)) fn(!fm_in_form() && sm_out() ? smn_gauge_state() : 1, fb, ff, fmx);  // (s68: the form gauge on the plate; s137: state 2 = KH2's summon plate while Chicken Little is out)
    else if (fn) fn(1, DG.bars, DG.fill, dg_max());
}

static bool dg_live_set(const char *key, int v) {
    dg_init();
    if (!strcmp(key, "drive")) {
        int mx = dg_max(), b0 = DG.bars;
        bool f0 = dg_full();
        DG.bars = v < 0 ? 0 : v > mx ? mx : v, DG.fill = DG.bars >= mx ? 100 : 0;
        dg_changed(b0, f0, "set drive");
    } else if (!strcmp(key, "drivept")) dg_add(v, "set drivept");
    else if (!strcmp(key, "drivemul")) DG.mul = v < 0 ? 0 : v, LOG("[drive] gains x %d%%\n", DG.mul);
    else if (!strcmp(key, "drivelog")) DG.log = v != 0;
    else if (!strcmp(key, "dggrow")) g_dg_grow = v != 0, LOG("[drive] max %s: now %d bars\n", g_dg_grow ? "grows with the form levels (KH2-style)" : "fixed 3 (old)", dg_max());
    else return false;
    dg_persist();
    return true;
}

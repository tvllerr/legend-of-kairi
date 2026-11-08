// sora_wpn.h (s128): Sora swings Link's equipped BotW melee weapon with the KH2 combos (plan recon/S128_PLAN.md,
// research recon/S128_HELPER_EQUIP.md + S128_HELPER_DMG.md). Included by sora.c.
//
// User picks (s128): automatic (the weapon equipped in BotW) + a KH2 Weapons-picker entry for it (pick a Keyblade = Keyblade;
// equipping a NEW weapon in BotW switches back to the weapon); melee only; Drive Forms keep their Keyblades; base-form
// abilities use the weapon; every type uses the normal KH2 combo, damage + reach from the weapon.
//
// API (used by the menu code, sora_camp*.h, and the hit / draw code):
//   wpn_equipped(&cur)  true = Link has a usable BotW melee weapon equipped right now (whatever g_wpn_use says)
//   wpn_active()        true = Sora uses it now (g_wpn_use, equipped, base form / not Roxas / not Mickey)
//   wpn_menu_pick(use)  the KH2 Weapons picker: 1 = the BotW weapon entry picked, 0 = a Keyblade picked
//   g_wpn_use           1 = the BotW weapon (default), 0 = the Keyblade; saved per slot in the levels sidecar ("wpnuse")
// Live keys: wpn 0 = feature off (old: Keyblade always), wpnfake <table index + 1> = pretend that weapon is equipped (testing
// without the s128 exe; 0 = off).
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    const char *actor;  // BotW actor name, e.g. "Weapon_Sword_001"
    const char *name;   // English name (USen msbt), e.g. "Traveler's Sword"
    uint8_t type;       // 0 one-handed (WeaponSmallSword), 1 two-handed (WeaponLargeSword), 2 spear (WeaponSpear)
    uint8_t keyblade;   // 1 = not a blade Sora should swing (rods, boomerangs, torches, ...): keep the Keyblade
    int16_t power;      // Attack.Power (bgparamlist)
    int16_t life;       // durability (General.Life), hits
    float zmin, zmax;   // model extent along the blade (+Z, metres): grip side .. tip
} WpnDef;

#include "sora_wpn_tab.h"  // static const WpnDef WPN_TAB[]; #define WPN_N (generated: tools_s128/gen_wpn_tab.py)

typedef struct {
    const WpnDef *def;
    int life, life_max;  // durability now / max, in hits (BotW stores x100)
    int atk_up;          // the "Attack up" modifier value (0 = none)
} WpnCur;

static int g_wpn_on = 1, g_wpn_use = 1, g_wpn_fake;
static char g_wpn_last[48];  // the last equipped weapon's actor name (a NEW one switches g_wpn_use back to 1)

static const WpnDef *wpn_find(const char *actor) {
    if (!actor || !*actor) return NULL;
    for (int i = 0; i < WPN_N; i++)
        if (!strcmp(WPN_TAB[i].actor, actor)) return &WPN_TAB[i];
    return NULL;
}

// filled in by the main session (exe export linkpos_weapon_info, s128 relink)
static bool wpn_read_exe(char *actor, int cap, int *life, int *atk_up);

// (s128b, live Frida read 20:2x) mEquippedWeapons[] is filled only while the pause menu is open (all 4 slots 0 in play), so
// the DLL walks the pouch list itself: PauseMenuDataMgr +0x68 = mItemLists.list1 (sead::OffsetList: head node prev/next,
// +0x78 count, +0x7c node offset 8 = PouchItem +0x08), first item with type +0x18 == 0 (sword slot) and equipped +0x24.
// Read: Weapon_Sword_001 eq 1 val 2000 (+ the shield, type 3). Plain reads, checked with rd_ok.
static bool rd_ok(uint64_t a, size_t n);
static bool wpn_read_list(char *actor, int cap, int *life, int *atk_up) {
    uint64_t got = sora_sites()->pmdm_got;
    if (!got || !rd_ok(got, 8)) return false;
    uint64_t pp = *(const uint64_t *)(uintptr_t)got;
    if (!rd_ok(pp, 8)) return false;
    uint64_t mgr = *(const uint64_t *)(uintptr_t)pp;
    if (!rd_ok(mgr + 0x68, 0x18) || *(const int32_t *)(uintptr_t)(mgr + 0x7c) != 8) return false;
    uint64_t head = mgr + 0x68, n = *(const uint64_t *)(uintptr_t)(head + 8);
    for (int k = 0; k < 500 && n && n != head; k++) {
        uint64_t it = n - 8;
        if (!rd_ok(it, 0x90)) return false;
        if (*(const int32_t *)(uintptr_t)(it + 0x18) == 0 && *(const uint8_t *)(uintptr_t)(it + 0x24)) {
            uint64_t sp = *(const uint64_t *)(uintptr_t)(it + 0x30);
            if (!rd_ok(sp, 1)) return false;
            const char *p = (const char *)(uintptr_t)sp;
            int i;
            for (i = 0; i < cap - 1 && p[i] && rd_ok(sp + (uint64_t)i + 1, 1); i++) actor[i] = p[i];
            actor[i] = 0;
            *life = *(const int32_t *)(uintptr_t)(it + 0x20);
            *atk_up = (*(const uint32_t *)(uintptr_t)(it + 0x88) & 1) ? (int)*(const uint32_t *)(uintptr_t)(it + 0x80) : 0;
            return i > 0;
        }
        n = *(const uint64_t *)(uintptr_t)(n + 8);
    }
    return false;
}

static bool wpn_equipped(WpnCur *out) {
    WpnCur c = {0};
    if (!g_wpn_on) return false;
    if (g_wpn_fake > 0 && g_wpn_fake <= WPN_N) {
        c.def = &WPN_TAB[g_wpn_fake - 1], c.life = c.life_max = c.def->life;
    } else {
        char actor[48];
        int life = 0, up = 0;
        if (!wpn_read_exe(actor, sizeof actor, &life, &up) && !wpn_read_list(actor, sizeof actor, &life, &up)) return false;
        // (s128 S128_HELPER_BREAK.md 3) after a break BotW leaves mEquippedWeapons[0] pointing at a recycled, emptied PouchItem
        // until the pause menu updates it (Master Sword: value 0 + unequipped): life 0 = no weapon = the Keyblade
        if (life <= 0) return false;
        if (!(c.def = wpn_find(actor))) return false;
        c.life = life / 100, c.life_max = c.def->life, c.atk_up = up;
    }
    if (c.def->keyblade) return false;
    if (strcmp(g_wpn_last, c.def->actor)) {  // (user s128: equipping a NEW weapon in BotW switches Sora to it)
        if (g_wpn_last[0]) g_wpn_use = 1;
        snprintf(g_wpn_last, sizeof g_wpn_last, "%s", c.def->actor);
    }
    if (out) *out = c;
    return true;
}

static void wpn_menu_pick(int use) { g_wpn_use = use != 0; }

// exe export (s128 linkpos.c): int linkpos_weapon_info(char *actor, int cap, int *life_x100, int *atk_up)
// = 1 when Link's sword slot (PauseMenuDataMgr mEquippedWeapons[0]) holds a weapon; older exe = no symbol = never.
typedef int (*WpnInfoFn)(char *, int, int *, int *);
static bool wpn_read_exe(char *actor, int cap, int *life, int *atk_up) {
    static WpnInfoFn fn;
    static int tried;
    if (!tried) {
        tried = 1, fn = (WpnInfoFn)(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_weapon_info");
        LOG("[wpn] exe export linkpos_weapon_info: %s\n", fn ? "found" : "NOT in this exe (needs the s128 relink)");
    }
    return fn && fn(actor, cap, life, atk_up) == 1;
}

// ------------------------------------------------------------------ (s128) use, hand, damage, reach, wear (main session)
// Base form only: Drive Forms keep their Keyblades (user s128), Roxas / Mickey / Anti too.
// (s137 roxwpn) Roxas (body Roxas, no Drive Form, not Mickey) swings it too: his Keyblade chain sits on bone 229 of his 241-bone
// bake (Sora 228 of 240; VERIFIED recon/S137_HELPER_ROXWPN.md: same frame, grip = hit-bake grip, blade = local -Y). Needs the s137 exe
// (per-body grip bone, export linkpos_weapon_rox); live key wpnrox 0 = old (Roxas = Keyblade only).
static int g_wpn_rox = 1;
static bool wpn_rox_exe(void) {
    static int ok = -1;
    if (ok < 0) {
        ok = GetProcAddress(GetModuleHandleA(NULL), "linkpos_weapon_rox") != NULL;
        LOG("[wpn] exe export linkpos_weapon_rox: %s\n", ok ? "found" : "NOT in this exe (needs the s137 relink): Roxas keeps the Keyblade");
    }
    return ok;
}
static bool wpn_body_ok(void) {  // base Sora, or Roxas (wpnrox 1 + the s137 exe)
    if (!g_fmots_on) return true;  // (g_fmots_on = any form's own motions: Drive Forms, Roxas, Mickey)
    return g_wpn_rox && IN_ROXAS && wpn_rox_exe();
}
static bool wpn_active(WpnCur *out) {
    if (!g_wpn_use || !wpn_body_ok()) return false;
    return wpn_equipped(out);
}
// GUESS keys (live "set ..."): wgz <cm> per type = how far the model origin sits past the grip along the blade (helper:
// the hands are on the grip, ~0.1 / 0.2 m below the model origin); wroll / wpitch / wyaw <deg> turn it in the hand;
// wpace <pct> = the damage pace (Sora hits more often than Link: GUESS 65).
// (s130 user: wroll 180 = the axe's edge leads, every other weapon tested still looks good -> default 180)
static int g_wgz[3] = {10, 20, 15}, g_wroll = 180, g_wpitch, g_wyaw, g_wpace = 65;
// (s131) the 3 monster arms (Stal arm models: they run along model +X, knob = shoulder ball at the origin; swords run
// along +Z) -> recon/S130_HELPER_STALARM.md. Each gets: extra yaw (wyarm, all 3: -90 sends +X to the blade axis; s130 shot
// s130arm7 at +90 = claws UP past his head), its own scale (BotW PlayerEqScale, wsarm<n> pct) and a grip point on the shaft
// in model space (wgxarm<n> cm along the arm, y/z = the shaft centreline, helper). n: 0 Bokoblin, 1 Lizalfos, 2 Moblin.
// wrarm = extra roll (spins the claws about the arm), wparm = extra pitch. Needs the s131 exe (linkpos_weapon_hand2).
// xmax = how far the claw tip reaches along the arm (unscaled m): reach = (xmax - gx) x scale. s131 helper
// (recon/S131_HELPER_ARMPOSE.md, held pose Arm_L_Weapon): Lizalfos Arm is BENT 64 deg at the elbow (BotW's boomerang shape,
// kept as is: grip on the upper arm) -> its xmax = grip + the straight 1.49 m grip -> claw tip; Moblin claws reach x 3.877.
typedef struct { const char *actor; int s_pct, gx_cm; float gy, gz, xmax; } WpnArm;
static WpnArm g_warm[3] = {
    {"Weapon_Sword_019", 70, 25, -0.02f, 0.03f, 1.688f},   // Bokoblin Arm
    {"Weapon_Sword_020", 70, 30, -0.013f, -0.006f, 1.79f},  // Lizalfos Arm (bent: reach 1.04 m)
    {"Weapon_Lsword_019", 50, 45, 0.004f, 0.048f, 3.877f},  // Moblin Arm (two-handed, reach 1.71 m)
};
// (s137 roxwpn) extra turns/back for ROXAS only, added to the Sora values (GUESS 0 = the same frame as Sora's; live keys wgzrox wrollrox wpitchrox wyawrox)
static int g_wgz_rox, g_wroll_rox, g_wpitch_rox, g_wyaw_rox;
static int g_wyaw_arm = -90, g_wroll_arm = 0, g_wpitch_arm = 0;
static const WpnArm *wpn_arm(const WpnDef *d) {
    if (d) for (int i = 0; i < 3; i++) if (!strcmp(d->actor, g_warm[i].actor)) return &g_warm[i];
    return NULL;
}
static int wpn_gz(const WpnDef *d) {  // cm: the model origin past the grip, for this weapon (arms: 0, the grip point does it)
    if (wpn_arm(d)) return 0;
    return g_wgz[d && d->type < 3 ? d->type : 0] + (IN_ROXAS ? g_wgz_rox : 0);
}
// grip -> tip (m = u). (s128 review) a few table rows have tiny zmax (glowing / elemental blades not in the mesh size,
// Ancient Battle Axe 0.01, Guardian Sword 0.116) or look modelled backwards (tridents Spear_049/050: zmin -2.03): use the
// longer side, never shorter than the type's floor (GUESS: Keyblade 0.78, two-hander 1.2, spear 1.8 m).
static float wpn_len(const WpnDef *d) {
    static const float floor_m[3] = {0.78f, 1.2f, 1.8f};
    int t = d->type < 3 ? d->type : 0;
    const WpnArm *a = wpn_arm(d);
    if (a) return (a->xmax - (float)a->gx_cm / 100.0f) * (float)a->s_pct / 100.0f;  // (s131) claw tip - grip point, scaled
    float z = d->zmax > -d->zmin ? d->zmax : -d->zmin, l = z + (float)wpn_gz(d) / 100.0f;
    return l < floor_m[t] ? floor_m[t] : l;
}

typedef void (*WpnHandFn)(int, const float *);  // (s131 hand2 takes off[8], same type)
typedef void (*WpnWearFn)(int);
static int g_wlead = 17;  // (s130 tuned live: glide + fall good; s129 live key wlead <ms>: the exe leads the weapon by Sora's velocity x this; 0 = old)
typedef void (*WpnNameFn)(const char *);
static WpnNameFn g_wpn_name_fn;  // (s128b exe) NULL = an older exe: then sora.c keeps LINK's view bit on instead (wpnvis)
static WpnHandFn g_wpn_hand_fn, g_wpn_hand2_fn;
static WpnWearFn g_wpn_wear_fn, g_wpn_lead_fn;
static WpnCur g_wpn_cur;  // this frame's (def NULL = the Keyblade)
static void wpn_tick(bool drawn) {
    static int tried;
    if (!tried) {
        HMODULE exe = GetModuleHandleA(NULL);
        tried = 1, g_wpn_hand_fn = (WpnHandFn)(void *)GetProcAddress(exe, "linkpos_weapon_hand");
        g_wpn_wear_fn = (WpnWearFn)(void *)GetProcAddress(exe, "linkpos_weapon_wear");
        g_wpn_name_fn = (WpnNameFn)(void *)GetProcAddress(exe, "linkpos_weapon_name");
        g_wpn_lead_fn = (WpnWearFn)(void *)GetProcAddress(exe, "linkpos_weapon_lead");  // (s129; NULL = an older exe)
        g_wpn_hand2_fn = (WpnHandFn)(void *)GetProcAddress(exe, "linkpos_weapon_hand2");  // (s131; NULL = an older exe: arms at scale 1)
    }
    WpnCur c = {0};
    bool on = drawn && wpn_active(&c);
    static const WpnDef *last;
    if (c.def != last) {
        if (c.def) LOG("[wpn] %s holds %s (%s, %s, Attack %d%+d, life %d/%d, reach %.2f m)\n", IN_ROXAS ? "Roxas" : "Sora", c.def->name, c.def->actor,
                       c.def->type == 1 ? "two-handed" : c.def->type == 2 ? "spear" : "one-handed", c.def->power, c.atk_up, c.life, c.life_max, wpn_len(c.def));
        else LOG("[wpn] %s holds the Keyblade (%s)\n", IN_ROXAS ? "Roxas" : "Sora", !g_wpn_on ? "wpn 0" : !g_wpn_use ? "Keyblade picked in the KH2 menu" : "no usable BotW weapon / Drive Form");
        last = c.def;
    }
    static int last_life = -1;  // (s129 test log: the durability as BotW counts it)
    if (c.def && c.life != last_life) { if (last_life >= 0) LOG("[wpn] %s life %d/%d\n", c.def->name, c.life, c.life_max); last_life = c.life; }
    else if (!c.def) last_life = -1;
    g_wpn_cur = on ? c : (WpnCur){0};
    if (on && g_wpn_name_fn) g_wpn_name_fn(c.def->actor);
    if (g_wpn_lead_fn) g_wpn_lead_fn(g_wlead);
    if (g_wpn_hand_fn || g_wpn_hand2_fn) {
        const WpnArm *a = wpn_arm(c.def);
        const int rx = IN_ROXAS;
        float off[8] = {c.def ? -(float)wpn_gz(c.def) / 100.0f : 0,  // (minus: the exe puts the origin at grip - Z*off; we want grip + Z*wgz, hands on the handle)
                        (float)(g_wroll + (rx ? g_wroll_rox : 0) + (a ? g_wroll_arm : 0)), (float)(g_wpitch + (rx ? g_wpitch_rox : 0) + (a ? g_wpitch_arm : 0)), (float)(g_wyaw + (rx ? g_wyaw_rox : 0) + (a ? g_wyaw_arm : 0)),
                        a ? (float)a->s_pct / 100.0f : 1.0f, a ? (float)a->gx_cm / 100.0f : 0, a ? a->gy : 0, a ? a->gz : 0};
        if (g_wpn_hand2_fn) g_wpn_hand2_fn(on && !g_wpn_fake, off);
        else g_wpn_hand_fn(on && !g_wpn_fake, off);
    }
}
// the damage of one hit with the weapon (user s128 option a): (Attack + Attack up) x the KH2 window's power % x wpace %
static int wpn_dmg(int power_pct) {
    if (!g_wpn_cur.def) return 0;
    float d = (float)(g_wpn_cur.def->power + g_wpn_cur.atk_up) * (float)power_pct / 100.0f * (float)g_wpace / 100.0f;
    int n = (int)lroundf(d);
    return n < 1 ? 1 : n;
}
static void wpn_wear(int hits) { if (g_wpn_cur.def && hits > 0 && g_wpn_wear_fn && !g_wpn_fake) g_wpn_wear_fn(hits); }
// the swept blade (grip xyz, tip xyz) stretched from the Keyblade's 78 cm to the weapon's grip -> tip
static void wpn_reach(float seg[6]) {
    if (!g_wpn_cur.def) return;
    float d[3] = {seg[3] - seg[0], seg[4] - seg[1], seg[5] - seg[2]}, l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (l < 1e-4f) return;
    float k = wpn_len(g_wpn_cur.def) / l;
    for (int i = 0; i < 3; i++) seg[3 + i] = seg[i] + d[i] * k;
}

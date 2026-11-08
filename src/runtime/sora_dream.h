// Sora-in-BotW (s54 dream): KH2's Dream Weapon picker, the mod-DLL side (included by runtime/sora.c BEFORE
// sora_levels.h, which then uses these two variables instead of its own). Recon + design: recon/S54_HELPER_DREAM.md.
//
// When: Sora mode, no pick yet (dream_weapon < 0), Link standing (Sora idle on the ground, life > 0) for DREAM_WAIT
// presents -> the picker opens. The world keeps running; Sora stands (sora.c zeroes his pad and stick while it is up)
// and is kept alive: a life drop is healed back to the life at opening (linkpos_link_heal, like SAVAGEWIND_LINK_GODMODE;
// a single hit from full life to 0 still kills - BotW's death fires on the hit itself, DAMAGE_ARGS_RECON.md).
// Look: the exe's sora_hud_dream (recon/wip_s54_dream/sora_hud.c, next exe link) draws KH2's own Station of Awakening
// lines (KH2 tt.bar / sys.bar, data/dream/dream_text.json) over the command menu's slots. It is looked up by name: on an
// exe without it the picker never opens and logs "picker needs the next exe" once (live "set dream N" still works).
// Flow (KH2's lines, KH2 has no give-up step - see the recon 2):
//   LIST  rows Sword / Shield / Staff, the message = the cursor weapon's examine lines (tt 0x43c0 / 0x43c4 / 0x43c2);
//         D-pad up / down (SE cursor), A = ask (SE decide); B = SE error (the choice can't be skipped)
//   ASK   "Is strength / protection / wisdom important?" rows "Not really." / "You bet it is!" (tt 0x43c1 / 5 / 3);
//         You bet it is! = picked -> GOT; Not really. or B = back to LIST (SE cancel)
//   GOT   "Received the power of the warrior / guardian / mystic." (tt 0x45fc..e), A or DREAM_GOT_HOLD presents
//   GLIST (give-up step, live "set dreamgu 0" = none) rows = the two others, their examine lines; A = ask
//   GASK  the same KH2 question; "Not really." = give that one up -> done; "You bet it is!" or B = back to GLIST
// Memory (this slice): the process environment (SORA_DREAM_PICK = "<pick>,<giveup>"), so a hot reload of the DLL keeps
// the pick and a new game process forgets it. dream_mem_load / dream_mem_save = the hook for the per-slot sidecar slice
// (S54_HELPER_LV_LEVELS.md 4.3: <save>/sora_kh2.json "dream"); that loader must run before the picker's DREAM_WAIT.
// Live (run/live_test.txt): "set dream -1" forget (the picker comes back once he stands), "set dream 0/1/2" pick
// without the picker (closes it), "set giveup -1/0/1/2", "set dreamui 1/0" open the picker now / close it, "set dreamauto
// 0/1" the automatic opening, "set dreamgu 0/1" the give-up step, "set dreamwait <presents>".
// Log: "[sora] dream weapon: picked sword, gave up staff (picker)".
#ifndef SORA_DREAM_H
#define SORA_DREAM_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "sora_win.h"  // (<windows.h> on Windows; Linux shims: Deck port)
#include "sora_hud.h"  // (SoraHudDream: recon/wip_s54_dream/sora_hud.h until merged)
#include "sora_sfx.h"

#define SORA_HAVE_DREAM_WEAPON
static int dream_weapon = -1, dream_giveup = -1;  // 0 sword, 1 shield, 2 staff; -1 = none yet (sora_levels.h: sword)

// data/hud/dream.bin label ids (= data/dream/dream_labels.txt order, tools/kh2_dream_text.py LABELS)
enum {
    SORA_DREAM_L_ROW = 0,    // + weapon: "Sword" "Shield" "Staff" (words cut from KH2 item names, sys.bar)
    SORA_DREAM_L_DESC = 3,   // + 2 x weapon + line: the examine text, 2 lines each
    SORA_DREAM_L_ASK = 9,    // + weapon: "Is strength / protection / wisdom important?"
    SORA_DREAM_L_NO = 12,    // "Not really."
    SORA_DREAM_L_YES = 13,   // "You bet it is!"
    SORA_DREAM_L_GOT = 14,   // + weapon: "Received the power of the warrior / guardian / mystic."
};
static const char *const DREAM_NAME[3] = {"sword", "shield", "staff"};
#define DREAM_BTN_A (1ull << 0)   // (sora.c BTN_ATTACK: Switch A = KH2's Circle, confirm)
#define DREAM_BTN_B (1ull << 1)   // (BTN_JUMP: Switch B = KH2's Cross, back)
#define DREAM_BTN_UP (1ull << 13)
#define DREAM_BTN_DOWN (1ull << 15)
#define DREAM_GOT_HOLD 90         // presents the "Received ..." line stays without A (GUESS)
enum { DR_OFF, DR_LIST, DR_ASK, DR_GOT, DR_GLIST, DR_GASK };
static const char *const DR_STATE[] = {"off", "list", "ask", "got", "give-up list", "give-up ask"};

static struct {
    int st, cur, w;           // state, cursor row, the weapon being asked about
    int rows[3], nrows;       // the list's weapons (GLIST: the two not picked)
    int auto_on, gu_on, wait; // live dreamauto / dreamgu / dreamwait
    int ground_n, life0, force;
    uint64_t got_at;
    bool loaded, tried, said_exe;
    void (*fn)(const SoraHudDream *);
} DR = {.auto_on = 1, .gu_on = 0, .wait = 90};  // (s56 user: KH2-exact single pick; "set dreamgu 1" = KH1's give-up)

static void dream_mem_load(void) {  // (the hook: the sidecar slice reads the save's slot here)
    char b[32];
    DWORD n = GetEnvironmentVariableA("SORA_DREAM_PICK", b, sizeof b);
    int p = -1, g = -1;
    if (n > 0 && n < sizeof b && sscanf(b, "%d,%d", &p, &g) >= 1) {
        dream_weapon = p >= 0 && p <= 2 ? p : -1, dream_giveup = g >= 0 && g <= 2 && g != p ? g : -1;
        LOG("[sora] dream weapon: picked %s, gave up %s (kept across the reload)\n",
            dream_weapon >= 0 ? DREAM_NAME[dream_weapon] : "-", dream_giveup >= 0 ? DREAM_NAME[dream_giveup] : "-");
    }
}
static void dream_mem_save(void) {  // (the hook: the sidecar slice writes the save's slot here)
    char b[32];
    snprintf(b, sizeof b, "%d,%d", dream_weapon, dream_giveup);
    SetEnvironmentVariableA("SORA_DREAM_PICK", dream_weapon < 0 && dream_giveup < 0 ? NULL : b);
}

static void dream_done(const char *how) {
    DR.st = DR_OFF, DR.force = 0;
    dream_mem_save();
    LOG("[sora] dream weapon: picked %s, gave up %s (%s)\n", dream_weapon >= 0 ? DREAM_NAME[dream_weapon] : "-",
        dream_giveup >= 0 ? DREAM_NAME[dream_giveup] : "-", how);
}

static void dream_list(int st) {  // LIST: all three; GLIST: the two not picked
    DR.st = st, DR.cur = 0, DR.nrows = 0;
    for (int w = 0; w < 3; w++)
        if (st == DR_LIST || w != dream_weapon) DR.rows[DR.nrows++] = w;
}

// Live "set <key> <v>" (sora.c live_poll's chain): true = the key was the picker's.
static bool dream_live_set(const char *key, int v) {
    if (!strcmp(key, "dream")) {
        DR.st = DR_OFF, DR.force = 0, DR.ground_n = 0;
        dream_weapon = v >= 0 && v <= 2 ? v : -1;
        if (dream_weapon < 0 || dream_giveup == dream_weapon) dream_giveup = -1;
        dream_done(dream_weapon < 0 ? "forgotten: the picker opens when he stands" : "set dream");
    } else if (!strcmp(key, "giveup")) {
        dream_giveup = v >= 0 && v <= 2 && v != dream_weapon ? v : -1;
        dream_done("set giveup");
    } else if (!strcmp(key, "dreamui")) DR.force = v != 0, DR.st = v ? DR.st : DR_OFF;
    else if (!strcmp(key, "dreamauto")) DR.auto_on = v != 0;
    else if (!strcmp(key, "dreamgu")) DR.gu_on = v != 0;
    else if (!strcmp(key, "dreamwait")) DR.wait = v < 0 ? 0 : v;
    else return false;
    return true;
}

// sora.c's driving frame, after the pad is read: ground_idle = Sora idle on the ground (no special motion), life = Link's.
// While the picker is up it takes this frame's presses, clears *press / *held and returns true (sora.c then zeroes the
// stick and marks the next frame fresh: no attack from the A that closes it, no flinch from a hit meanwhile).
static bool dream_tick(uint64_t present, bool ground_idle, int life, uint64_t *press, uint64_t *held) {
    if (!DR.loaded) DR.loaded = true, dream_mem_load();
    if (!DR.tried) {
        DR.tried = true;
        DR.fn = (void (*)(const SoraHudDream *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_dream");
        LOG("[sora] dream weapon: sora_hud_dream %s\n", DR.fn ? "found" : "not in this exe (picker needs the next exe)");
    }
    if (DR.st == DR_OFF) {
        DR.ground_n = ground_idle && life > 0 ? DR.ground_n + 1 : 0;
        bool want = DR.force || (DR.auto_on && dream_weapon < 0 && DR.ground_n >= DR.wait);
        if (!want) return false;
        if (!DR.fn) {
            if (!DR.said_exe) DR.said_exe = true, LOG("[sora] dream weapon: picker needs the next exe (waiting)\n");
            return false;
        }
        if (!ground_idle || life <= 0) return false;  // (a forced open waits for him to stand too)
        DR.force = 0, DR.life0 = life;
        dream_list(DR_LIST);
        sora_sfx_event("page", 1);
        LOG("[sora] dream weapon: picker open at present %llu (life %d)\n", (unsigned long long)present, life);
    }
    // kept alive while it is up (see the top)
    if (life > DR.life0) DR.life0 = life;
    else if (life > 0 && life < DR.life0) {
        int v = linkpos_link_heal(DR.life0 - life);
        LOG("[sora] dream weapon: hit while the picker is up: life %d -> %d\n", life, v);
    }
    uint64_t p = *press;
    {   // (s56 debug: user saw no cursor move / no select)
        static uint64_t beat;
        if (p) LOG("[sora] dream weapon: press %#llx held %#llx (st %d cur %d)\n", (unsigned long long)p,
                   (unsigned long long)*held, DR.st, DR.cur);
        if (present - beat >= 300) beat = present, LOG("[sora] dream weapon: tick at %llu held %#llx st %d\n",
                                                       (unsigned long long)present, (unsigned long long)*held, DR.st);
    }
    *press = 0, *held = 0;
    int st0 = DR.st, n = DR.st == DR_LIST || DR.st == DR_GLIST ? DR.nrows : DR.st == DR_GOT ? 0 : 2;
    if (n && (p & DREAM_BTN_UP)) DR.cur = (DR.cur + n - 1) % n, sora_sfx_event("cursor", 1);
    if (n && (p & DREAM_BTN_DOWN)) DR.cur = (DR.cur + 1) % n, sora_sfx_event("cursor", 1);
    if (DR.st == DR_LIST || DR.st == DR_GLIST) {
        if (p & DREAM_BTN_A) DR.w = DR.rows[DR.cur], DR.st = DR.st == DR_LIST ? DR_ASK : DR_GASK, DR.cur = 0, sora_sfx_event("decide", 1);
        else if (p & DREAM_BTN_B) sora_sfx_event("error", 1);
    } else if (DR.st == DR_ASK || DR.st == DR_GASK) {
        bool yes = (p & DREAM_BTN_A) && DR.cur == 1, no = ((p & DREAM_BTN_A) && DR.cur == 0) || (p & DREAM_BTN_B);
        bool take = DR.st == DR_ASK ? yes : no;  // (ASK: "You bet it is!" picks it; GASK: "Not really." gives it up)
        if (take) {
            sora_sfx_event("decide", 1);
            if (DR.st == DR_ASK) {
                dream_weapon = DR.w;
                if (dream_giveup == dream_weapon) dream_giveup = -1;
                DR.st = DR_GOT, DR.got_at = present;
                LOG("[sora] dream weapon: picked %s\n", DREAM_NAME[dream_weapon]);
            } else {
                dream_giveup = DR.w;
                dream_done("picker");
                return true;
            }
        } else if (yes || no) {  // back to the list, on the weapon just asked about
            int w = DR.w;
            dream_list(DR.st == DR_ASK ? DR_LIST : DR_GLIST);
            for (int i = 0; i < DR.nrows; i++)
                if (DR.rows[i] == w) DR.cur = i;
            sora_sfx_event("cancel", 1);
        }
    } else if (DR.st == DR_GOT && ((p & DREAM_BTN_A) || present - DR.got_at >= DREAM_GOT_HOLD)) {
        if (DR.gu_on) dream_list(DR_GLIST);
        else {
            dream_done("picker, no give-up step");
            return true;
        }
    }
    if (DR.st != st0) LOG("[sora] dream weapon: %s -> %s\n", DR_STATE[st0], DR_STATE[DR.st]);
    // the panel for the exe (every frame while up; it hides by itself when these calls stop)
    SoraHudDream d = {.on = true, .cursor = DR.cur};
    if (DR.st == DR_LIST || DR.st == DR_GLIST) {
        int w = DR.rows[DR.cur];
        d.n_text = 2, d.text[0] = (int16_t)(SORA_DREAM_L_DESC + 2 * w), d.text[1] = (int16_t)(SORA_DREAM_L_DESC + 2 * w + 1);
        d.n_rows = DR.nrows;
        for (int i = 0; i < DR.nrows; i++) d.row[i] = (int16_t)(SORA_DREAM_L_ROW + DR.rows[i]);
    } else if (DR.st == DR_ASK || DR.st == DR_GASK) {
        d.n_text = 1, d.text[0] = (int16_t)(SORA_DREAM_L_ASK + DR.w);
        d.n_rows = 2, d.row[0] = SORA_DREAM_L_NO, d.row[1] = SORA_DREAM_L_YES;
    } else d.n_text = 1, d.text[0] = (int16_t)(SORA_DREAM_L_GOT + dream_weapon), d.cursor = -1;
    DR.fn(&d);
    return true;
}
#endif

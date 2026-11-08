// sora_cust.h: Sora's 4 command-menu shortcuts (L + Triangle / Square / Cross / Circle), the KH2 pause menu Customize
// page model (slice 4, plan docs/plans/2026-10-30-kh2-pause-menu-slice4-customize.md). Pure, host-tested
// (runtime/test/cust_test.c). Slot order = M.shortcuts in sora.c: 0 X (Triangle), 1 Y (Square), 2 B (Cross), 3 A (Circle).
// Spell = SPELL_FIRE 0 / BLIZZARD 1 / THUNDER 2 / CURE 3 / MAGNET 4 / REFLECT 5 (s147), or CUST_EMPTY.
// Duplicates ALLOWED: KH2 FM's apply FUN_140359b70 writes save+0x36f8 + slot * 2 with no check (recon/S84_CUST_PAGE.md).
// Sidecar pack: byte i = slot i, 0xFF = empty (the default packs to 0x03020100).
#pragma once
#include <stdint.h>

#define CUST_SLOTS 4
#define CUST_EMPTY -1
#define CUST_NSPELL 6  // (s147: + Magnet 4 / Reflect 5; an old save's bytes 0..3 unpack the same)

static void cust_default(int sc[CUST_SLOTS]) { for (int i = 0; i < CUST_SLOTS; i++) sc[i] = i; }

static uint32_t cust_pack(const int sc[CUST_SLOTS]) {
    uint32_t v = 0;
    for (int i = 0; i < CUST_SLOTS; i++) v |= (uint32_t)(sc[i] >= 0 && sc[i] < CUST_NSPELL ? sc[i] : 0xFF) << (8 * i);
    return v;
}

// returns the number of bytes that were neither a spell nor 0xFF (those slots become empty, the others kept)
static int cust_unpack(uint32_t v, int sc[CUST_SLOTS]) {
    int bad = 0;
    for (int i = 0; i < CUST_SLOTS; i++) {
        int b = (int)((v >> (8 * i)) & 0xFF);
        if (b < CUST_NSPELL) sc[i] = b;
        else { sc[i] = CUST_EMPTY; bad += b != 0xFF; }
    }
    return bad;
}

// 1 changed, 0 already so, -1 refused (slot or spell out of range)
static int cust_set(int sc[CUST_SLOTS], int slot, int spell) {
    if (slot < 0 || slot >= CUST_SLOTS || spell < CUST_EMPTY || spell >= CUST_NSPELL) return -1;
    if (sc[slot] == spell) return 0;
    sc[slot] = spell;
    return 1;
}

// log names (KH_SPELLS order in sora.c)
static const char *cust_str(int sp) {
    static const char *const N[CUST_NSPELL] = {"fire", "blizzard", "thunder", "cure", "magnet", "reflect"};
    return sp == CUST_EMPTY ? "empty" : sp >= 0 && sp < CUST_NSPELL ? N[sp] : "?";
}

// the levels sidecar "shortcuts" key: missing (old saves / a new slot) -> the default, else unpack; returns the bad bytes
static int cust_load(int found, uint32_t v, int sc[CUST_SLOTS]) {
    if (!found) { cust_default(sc); return 0; }
    return cust_unpack(v, sc);
}

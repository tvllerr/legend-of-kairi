// sora_armor.h (s120): Link's worn BotW armor for the KH2 Equipment page's Armor rows (user ruling s120: option A = SHOW ONLY;
// change armor in BotW's own menu; BotW already lowers the hits by Link's armor, so the Defense is shown, never added again).
// Source: the exe export linkpos_link_armor (s120 exe: the Armor_* actors that carry Link's BaseProcLinkData, seen in the
// last 1 s) + sora_armor_tab.h (BotW's own names + armorDefenceAddLevel). Old exe = no export = no Armor rows (logged once).
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sora_armor_tab.h"

static int armor_cmp_(const void *k, const void *e) { return strcmp((const char *)k, ((const SoraArmor *)e)->actor); }
static const SoraArmor *armor_find(const char *actor) {
    return (const SoraArmor *)bsearch(actor, SORA_ARMOR, SORA_ARMOR_N, sizeof *SORA_ARMOR, armor_cmp_);
}

// Pure: worn actor names (any order, unknown ones skipped) -> rows in KH2-ish order Head, Body, Legs (one per slot, the
// first seen wins). out_name[i] = BotW's name, out_def[i] = its Defense. -> row count (0..3).
static int armor_rows(const char (*worn)[40], int nw, char out_name[3][24], int out_def[3]) {
    const SoraArmor *by[3] = {0};
    for (int i = 0; i < nw; i++) {
        const SoraArmor *a = armor_find(worn[i]);
        if (a && a->slot < 3 && !by[a->slot]) by[a->slot] = a;
    }
    int n = 0;
    for (int s = 0; s < 3; s++) {
        if (!by[s]) continue;
        snprintf(out_name[n], 24, "%s", by[s]->name);
        out_def[n++] = by[s]->def;
    }
    return n;
}

#ifndef SORA_ARMOR_PURE  // (host tests define it)
// Game side: -> row count; 0 when the exe lacks the export or Link wears nothing named (Armor_Default_* are not in the table).
static int armor_worn(char out_name[3][24], int out_def[3]) {
    static int (*get)(char (*)[40], int);
    static int bound;
    if (!bound) {
        bound = 1;
        get = (int (*)(char (*)[40], int))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_link_armor");
        LOG("[armor] %s\n", get ? "exe linkpos_link_armor found" : "linkpos_link_armor not in this exe: no Armor rows");
    }
    if (!get) return 0;
    char worn[8][40];
    int nw = get(worn, 8);
    return armor_rows((const char (*)[40])worn, nw < 0 ? 0 : nw > 8 ? 8 : nw, out_name, out_def);
}
#endif

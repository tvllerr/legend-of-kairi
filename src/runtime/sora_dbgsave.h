// sora_dbgsave.h: (dbgsave, PR #60; s288 port) the Debug Menu rows as one line of the slot sidecar sora_kh2.json. Pure text in /
// text out, no game state (host test: runtime/test/dbgsave_test.c). The glue (what is written when, what is done with what is
// read) is sora_dbgmenu.h dbm_save_str / dbm_slot_load; the sidecar itself is sora_levels.h sl_sidecar_write / sl_slot_load.
//
// The line (between "summon" and "dream" in the sidecar):
//     "dbgmenu": "lv=0,kb=1,forms=0,smn=0,ab=0,god=0,clip=0,sora=0,outfit=0,autoout=0,party1=0,party2=1,drive=0,form=0,mickey=0,mp=0,hjump=0,adodge=0,gyro=1,qrun=0",
// name = the row's live key without "dbm" (dbmlv, dbmkb, ... dbmqrun), value = the number the row has (sora_dbgmenu.h dbm_state).
// Names, not positions (PR #60 wrote bare positions "0,1,0,..."): a row added or moved later still reads right, a name this DLL
// does not know is skipped. No quote inside the value, so the sidecar's other flat-key reads (sl_json_u32 "\"drive\"" ...) never
// match in it.
#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DBS_NROWS 20  // = sora_dbgmenu.h DBM_NROWS (checked there), same order
static const char *const DBS_NAME[] = {"lv", "kb", "forms", "smn", "ab", "god", "clip", "sora", "outfit", "autoout", "party1",
                                       "party2", "drive", "form", "mickey", "mp", "hjump", "adodge", "gyro", "qrun"};
_Static_assert(sizeof DBS_NAME / sizeof *DBS_NAME == DBS_NROWS, "one name per Debug Menu row");

// The whole line into o (2 spaces, the key, the value, ",\n"). -> its length; 0 = it does not fit in cap (o is "" then, so the
// sidecar never gets half a line). Worst case (20 rows, every value 11 characters) = 362 characters.
static int dbs_write(char *o, size_t cap, const int v[DBS_NROWS]) {
    if (!o || !cap) return 0;
    o[0] = 0;
    size_t n = 0;
    for (int r = -1; r <= DBS_NROWS; r++) {  // -1 = the key, 0..19 = the rows, 20 = the closing quote
        int k = r < 0 ? snprintf(o + n, cap - n, "  \"dbgmenu\": \"")
              : r < DBS_NROWS ? snprintf(o + n, cap - n, "%s%s=%d", r ? "," : "", DBS_NAME[r], v[r])
              : snprintf(o + n, cap - n, "\",\n");
        if (k < 0 || (size_t)k >= cap - n) { o[0] = 0; return 0; }
        n += (size_t)k;
    }
    return (int)n;
}

// (s289 fix3b, recon/s289/S289_HELPER_COOPCHECK.md 3b) a Party row (party1 / party2) under the co-op room's one-member rule. The
// rule empties a slot with the Debug Menu's None (sora_dbgmenu.h dbm_coop_drop_, marked in sora_party.h g_coop_auto) and leaving
// the room fills it again with the member the slot kept under it (dbm_coop_tick_: nobody is swapped in). Written as None (-1), a
// save made in the room (an autosave too) took that member away from single player: the next solo Continue (dbm_slot_load) ran
// the -1 as the player's OWN None, which the room's end never gives back.
// row = the row's value as the line had it before (a character id, -1 = None), rule = the rule made this None (g_coop_auto[slot]),
// under = the character the slot keeps (g_pchar[slot]). -> the value for the line: the rule's None = under; else row, unchanged
// (the player's own None stays -1; single player never sets rule, so its line is byte for byte the old one).
static int dbs_party_val(int row, bool rule, int under) { return rule ? under : row; }

// The line out of the sidecar text b (NUL-terminated). -> -1 = no "dbgmenu" key (a save from before dbgsave, or written with it
// off), -2 = the key is there but its value is not a closed "..." string on that line (a broken file: nothing of it is used),
// else the number of rows read (have[r] = true, v[r] = the value). *bad = items not understood (an unknown name, no number).
static int dbs_parse(const char *b, int v[DBS_NROWS], bool have[DBS_NROWS], int *bad) {
    for (int r = 0; r < DBS_NROWS; r++) have[r] = false, v[r] = 0;
    *bad = 0;
    const char *p = b ? strstr(b, "\"dbgmenu\"") : NULL;
    if (!p) return -1;
    p += 9;
    while (*p == ' ' || *p == '\t') p++;
    if (*p++ != ':') return -2;
    while (*p == ' ' || *p == '\t') p++;
    if (*p++ != '"') return -2;
    const char *end = p;
    while (*end && *end != '"' && *end != '\n' && *end != '\r') end++;
    if (*end != '"') return -2;
    int got = 0;
    while (p < end) {
        const char *item = p, *eq = NULL;
        while (p < end && *p != ',') { if (*p == '=' && !eq) eq = p; p++; }  // one item = item .. p
        int r = -1;
        if (eq)
            for (int i = 0; i < DBS_NROWS && r < 0; i++)
                if ((size_t)(eq - item) == strlen(DBS_NAME[i]) && !strncmp(item, DBS_NAME[i], (size_t)(eq - item))) r = i;
        char *e = NULL;
        long x = eq ? strtol(eq + 1, &e, 10) : 0;
        if (r < 0 || e == eq + 1 || e != p) (*bad)++;  // unknown name / no number / junk after the number
        else {
            if (!have[r]) got++;
            have[r] = true, v[r] = (int)x;
        }
        if (p < end) p++;  // the ","
    }
    return got;
}

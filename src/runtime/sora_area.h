// sora_area.h (s190, DLL side; Melty spec 4.1): which map the game is on, as one short text that is the same in every copy:
//   the overworld = "MainField"; a shrine = its name ("Dungeon012"); a Divine Beast = its name ("RemainsWater"); "" = not known.
// Where it lives: two sead::FixedSafeString<32> statics side by side in main's .bss, the current map TYPE ("MainField",
// "CDungeon", "MainFieldDungeon", "AocField") and the current map NAME (overworld: the section, e.g. "F-6"; else the
// shrine's / beast's name). VERIFIED s190 on 1.9.0 by reading the running game: 0x7001dae3a0 / 0x7001dae3d8 held
// MainField / F-6 on the Great Plateau side (x 153, z 1541 = section F-6) and MainFieldDungeon / RemainsWater inside Vah
// Ruta (sb/s190/scan_*.txt). The decomp's name for them (ksys::StageInfo sCurrentMapType / sCurrentMapName) is a GUESS.
// No pinned address: the pair is found by its shape (area_scan), so another game version needs no new site row.
//   object layout: +0 vtable, +8 char *top (= this + 0x14), +0x10 s32 size (32), +0x14 the text; the next one at +0x38.
// A second pair of the same shape exists (1.9.0: 0x7001e11360, MainField / A-1, did not change inside Vah Ruta): a default,
// not the live one. The live pair is the LOWEST address (GUESS that this order holds on other versions; all are logged).
// The pure part (area_scan, area_text) has a host test: runtime/test/area_test.c.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define AREA_FSS_STEP 0x38  // sizeof(sead::FixedSafeString<32>) (0x14 + 32, padded to 8)
#define AREA_FSS_TEXT 0x14
#define AREA_CAP 32

// The text of the FixedSafeString<32> at img[off] (the image starts at guest address base), or NULL when there is none.
static const char *area_fss32(const uint8_t *img, uint64_t base, size_t size, size_t off) {
    if ((off & 7) || off + AREA_FSS_STEP > size) return NULL;
    uint64_t top;
    int32_t cap;
    memcpy(&top, img + off + 8, 8), memcpy(&cap, img + off + 0x10, 4);
    if (top != base + off + AREA_FSS_TEXT || cap != 32) return NULL;
    const char *s = (const char *)img + off + AREA_FSS_TEXT;
    int n = 0;
    while (n < 32 && s[n]) {
        if ((unsigned char)s[n] < 0x20 || (unsigned char)s[n] > 0x7e) return NULL;
        n++;
    }
    return n < 32 ? s : NULL;  // (ends inside its buffer)
}

static int area_is_map_type(const char *s) {
    return !strcmp(s, "MainField") || !strcmp(s, "MainFieldDungeon") || !strcmp(s, "CDungeon") || !strcmp(s, "AocField");
}

// Every (map type, map name) pair in the image, lowest address first: out[] = the guest address of the TYPE string
// object. Returns how many were found (it keeps counting past cap).
static int area_scan(const uint8_t *img, uint64_t base, size_t size, uint64_t *out, int cap) {
    int n = 0;
    for (size_t off = 0; off + 2 * AREA_FSS_STEP <= size; off += 8) {
        const char *type = area_fss32(img, base, size, off);
        if (!type || !area_is_map_type(type)) continue;
        const char *name = area_fss32(img, base, size, off + AREA_FSS_STEP);
        if (!name || !*name || memcmp(img + off, img + off + AREA_FSS_STEP, 8)) continue;  // (same class = same vtable)
        if (n < cap) out[n] = base + off;
        n++;
    }
    return n;
}

// The area text of a map type + name: letters, digits, '-' and '_' only (anything else = not known).
static void area_text(const char *type, const char *name, char out[AREA_CAP]) {
    out[0] = 0;
    if (!type || !*type) return;
    const char *s = !strcmp(type, "MainField") ? type : name && *name ? name : type;
    size_t n = strlen(s);
    if (n >= AREA_CAP) return;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return;
    }
    memcpy(out, s, n + 1);
}

#ifndef SORA_AREA_TEST
// ---- the game side (sora.c: LOG, rd_ok, sora_sites) ----
static struct {
    uint64_t pair;       // the live TYPE string object (0 = not found)
    int tries;
    uint64_t next_try;
    char now[AREA_CAP];  // the last area text
    int on;              // live "set area 0" = always "" (one world, the s189 behavior)
} AREA = {.on = 1};

static void area_find(uint64_t now_ms) {
    if (AREA.pair || AREA.tries >= 5 || now_ms < AREA.next_try) return;
    AREA.tries++, AREA.next_try = now_ms + 2000;
    uint64_t lo = sora_sites()->main_data, hi = sora_sites()->main_end;
    if (!lo || hi <= lo) { AREA.tries = 5; return; }
    for (uint64_t a = lo; a < hi; a += 0x1000)
        if (!rd_ok(a, 0x1000)) { LOG("[area] main's data is not readable at %#llx: no area (one world)\n", (unsigned long long)a); AREA.tries = 5; return; }
    uint64_t c[4];
    int n = area_scan((const uint8_t *)(uintptr_t)lo, lo, (size_t)(hi - lo), c, 4);
    for (int i = 0; i < n && i < 4; i++)
        LOG("[area] map strings at %#llx: %s / %s%s\n", (unsigned long long)c[i], (const char *)(uintptr_t)(c[i] + AREA_FSS_TEXT),
            (const char *)(uintptr_t)(c[i] + AREA_FSS_STEP + AREA_FSS_TEXT), i == 0 ? " (used)" : "");
    if (n > 0) AREA.pair = c[0];
    else LOG("[area] no map strings found (try %d of 5)%s\n", AREA.tries, AREA.tries >= 5 ? ": no area (one world)" : "");
}

// The area we are on right now ("" = not known / off). Call while Link is in the world.
static const char *area_now(void) {
    if (!AREA.on) return "";
    area_find(GetTickCount64());
    if (!AREA.pair) return "";
    char type[33], name[33], t[AREA_CAP];
    memcpy(type, (const void *)(uintptr_t)(AREA.pair + AREA_FSS_TEXT), 32), type[32] = 0;
    memcpy(name, (const void *)(uintptr_t)(AREA.pair + AREA_FSS_STEP + AREA_FSS_TEXT), 32), name[32] = 0;
    area_text(type, name, t);
    if (strcmp(t, AREA.now)) {
        LOG("[area] now \"%s\" (%s / %s)\n", t, type, name);
        memcpy(AREA.now, t, sizeof t);
    }
    return AREA.now;
}

static bool area_set_key(const char *key, int v) {
    if (strcmp(key, "area")) return false;
    AREA.on = v != 0;
    LOG("[area] %s\n", AREA.on ? "on" : "off (one world)");
    return true;
}
#endif

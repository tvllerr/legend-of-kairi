// sora_meals.h (s93 b7-meals): cooked meals in the command menu's Items page (recon/S93_HELPER_MEALS.md).
// Behind the live key "meals" (default 0): off = Items stays greyed exactly as before.
//   set meals 1      Items lists Link's cooked dishes (PorchItem names "Item_Cook_*"), d-pad up / down, A eats, B / left back
//   set mealq <n>    quarter hearts a dish heals when its own value is not read (default 8 = 2 hearts)
//   set mealhp 0/1   (s98, default 1) each dish heals its own CookEffect0 value (quarter hearts, VERIFIED live read)
//   set mealtp 0/1   (s98, default 1) gdt TriggerParam: 1 = [mgr+0xbd8] (live pouch, VERIFIED), 0 = [mgr+0xbe0] (empty)
//   set mealdel 0/1  (s98 b13, default 0) 1 = eating also removes the dish from BotW's real pouch (exe linkpos_pouch_remove)
//   set mealfind 1   (s98) one-shot memory search for "Item_Cook_" names (recon)
//   set meallog 1    log every scan / use
// The inventory is read straight out of guest memory (the DLL shares the process): gdt::Manager -> TriggerParam -> the
// string64 array group (+0xd8 count, +0xe0 array flags; s98: the TriggerParam at [mgr+0xbd8], NOT +0xbe0) -> the 420-entry array "PorchItem" -> each element flag's
// FixedSafeString (found by its self-pointing mStringTop). Chain VERIFIED STATIC on 1.8.2 (Ghidra getters 0x7101024030 /
// 0x7101023c2c), field offsets same on 1.9.0 (S61); the string scan is a GUESS until the first live test.
// Included by sora.c after sora_camp.h (needs rd_ok, CAMP, link_heal, M, LOG).
#pragma once

#define MEAL_MAX 48
#define MEAL_ROWS 6
#define MEAL_SUB 4  // M.sub value of this page

#define MEAL_HP 16  // per-dish heal values kept per name (dishes of one name can differ by ingredients)
static int g_meals = 0, g_meal_q = 8, g_meal_log = 0, g_meal_tp = 1, g_meal_hp = 1;
#include "sora_items.h"  // (s131 kh2item: dish -> KH2 item table; after g_meal_tp, which it reads)
static struct {
    int n;
    struct { char name[40]; int count, seen, used, nh; short hp[MEAL_HP]; uint8_t ki[MEAL_HP]; } e[MEAL_MAX];  // count = listed (gdt count - eaten here)
    int cursor, scroll;
    int tick;
    uint64_t elems;  // the PorchItem element array (cached; re-found when it stops looking right)
    int ncnt;
} MEAL;

static bool meal_porch(uint64_t *elems, int *cnt) {
    const SoraSites *ss = sora_sites();
    if (!ss || !ss->gdt_mgr || !rd_ok(ss->gdt_mgr, 8)) return false;
    uint64_t mgr = *(const uint64_t *)(uintptr_t)ss->gdt_mgr;
    if (!rd_ok(mgr + 0xbe0, 8)) return false;
    // (s98) gdt Manager holds two TriggerParam refs: +0xbd8 (mParam1?) and +0xbe0 (s93 pick: its PorchItem strings are
    // all empty with dishes in the pouch). live "mealtp 0/1" picks +0xbe0 / +0xbd8 (default 1 = +0xbd8, the s98 guess)
    uint64_t tp = *(const uint64_t *)(uintptr_t)(mgr + (g_meal_tp ? 0xbd8 : 0xbe0));
    if (!rd_ok(tp, 8)) return false;
    uint64_t param = *(const uint64_t *)(uintptr_t)tp;
    if (!rd_ok(param + 0xd8, 16)) return false;
    uint32_t n = *(const uint32_t *)(uintptr_t)(param + 0xd8);
    uint64_t arr = *(const uint64_t *)(uintptr_t)(param + 0xe0);
    if (n == 0 || n > 4096 || !rd_ok(arr, 8ull * n)) return false;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t af = ((const uint64_t *)(uintptr_t)arr)[i];
        if (!rd_ok(af, 16)) continue;
        uint32_t c = *(const uint32_t *)(uintptr_t)af;
        uint64_t el = *(const uint64_t *)(uintptr_t)(af + 8);
        if (c == 420 && rd_ok(el, 8ull * c)) { *elems = el, *cnt = (int)c; return true; }  // PorchItem (the only 420 string64 array)
    }
    return false;
}

// (s98 VERIFIED LIVE) CookEffect0 = the FIRST 60-entry vec2 array (TriggerParam +0xf8 count / +0x100 ptr; element = vptr,
// twin, +0x10 f32 x, y): x = heal in quarter hearts, y = effect seconds; index = the dish's place among the pouch's FOOD
// entries (an Item_Roast_01 ahead of 4 dishes took [0], the dishes [1..4]: 12 4 12 8). CookEffect1 (2nd: effect id,
// level) and the sell price (3rd) are not used. Returns the CookEffect0 element array, or 0.
static uint64_t meal_cook0(void) {
    const SoraSites *ss = sora_sites();
    if (!ss || !ss->gdt_mgr || !rd_ok(ss->gdt_mgr, 8)) return 0;
    uint64_t mgr = *(const uint64_t *)(uintptr_t)ss->gdt_mgr;
    if (!rd_ok(mgr + 0xbd8, 16)) return 0;
    uint64_t tp = *(const uint64_t *)(uintptr_t)(mgr + (g_meal_tp ? 0xbd8 : 0xbe0));
    if (!rd_ok(tp, 8)) return 0;
    uint64_t param = *(const uint64_t *)(uintptr_t)tp;
    if (!rd_ok(param + 0xf8, 16)) return 0;
    uint32_t n = *(const uint32_t *)(uintptr_t)(param + 0xf8);
    uint64_t arr = *(const uint64_t *)(uintptr_t)(param + 0x100);
    if (n == 0 || n > 64 || !rd_ok(arr, 8ull * n)) return 0;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t af = ((const uint64_t *)(uintptr_t)arr)[i];
        if (!rd_ok(af, 16)) continue;
        uint64_t el = *(const uint64_t *)(uintptr_t)(af + 8);
        if (*(const uint32_t *)(uintptr_t)af == 60 && rd_ok(el, 8 * 60)) return el;
    }
    return 0;
}
static int meal_cook_hp(uint64_t c0, int fi) {  // quarter hearts, or -1
    if (!c0 || fi < 0 || fi >= 60) return -1;
    uint64_t o = ((const uint64_t *)(uintptr_t)c0)[fi];
    if (!rd_ok(o, 0x18)) return -1;
    float x = *(const float *)(uintptr_t)(o + 0x10);
    return x >= 0 && x <= 400 ? (int)(x + 0.5f) : -1;  // (-1 = no cook data in that slot)
}
// the Food tab's actor names (BotW PouchItemType Food: dishes, roasted / frozen / boiled items)
static bool meal_is_food(const char *nm) {
    return !strncmp(nm, "Item_Cook_", 10) || !strncmp(nm, "Item_Roast", 10) || !strncmp(nm, "Item_Chilled", 12) ||
           !strncmp(nm, "Item_Boiled", 11);
}

// one element flag's string: the self-pointing mStringTop inside the element. s98 VERIFIED LIVE: elements are 0x68 apart
// (vptr, +8 = the twin element in the other TriggerParam, +0x10 FixedSafeString<64>: top +0x18 = obj+0x24, size 0x40),
// so the search stays inside 0x68 bytes (a wider window reads the NEXT element's name).
#define MEAL_OBJ 0x68
static int meal_str_n;  // (s98) string tops found in the last object (meallog 2 dump)
static bool meal_str(uint64_t obj, char *out, int cap) {
    if (!obj || !rd_ok(obj, MEAL_OBJ)) return false;
    meal_str_n = 0;
    for (int off = 8; off < MEAL_OBJ; off += 8) {
        uint64_t p = *(const uint64_t *)(uintptr_t)(obj + off);
        if (p < obj + (uint64_t)off || p >= obj + MEAL_OBJ || !rd_ok(p, 1)) continue;
        int k = 0;
        const char *s = (const char *)(uintptr_t)p;
        while (k < cap - 1 && p + (uint64_t)k < obj + MEAL_OBJ && s[k] >= 0x20 && s[k] < 0x7f) out[k] = s[k], k++;
        if (k < cap - 1 && p + (uint64_t)k < obj + MEAL_OBJ && s[k] != 0) continue;  // not text
        out[k] = 0;
        meal_str_n++;
        return true;
    }
    return false;
}

// (s98 recon) "set mealfind 1": one-shot search of guest memory for "Item_Cook_" text, to find where BotW keeps the
// live pouch (both PorchItem strings were empty with dishes in the pouch). Logs each hit + the words before it
// (a FixedSafeString has its self-pointing mStringTop 0xc bytes before the text) + the gdt Manager words near +0xbe0.
static int g_meal_find = 0;
static void meal_find(void) {
    ULONGLONG t0 = GetTickCount64();
    int hits = 0, regions = 0;
    MEMORY_BASIC_INFORMATION mbi;
    const SoraSites *ss = sora_sites();
    if (ss && ss->gdt_mgr && rd_ok(ss->gdt_mgr, 8)) {
        uint64_t mgr = *(const uint64_t *)(uintptr_t)ss->gdt_mgr;
        if (rd_ok(mgr + 0xbc0, 0x60)) {
            const uint64_t *w = (const uint64_t *)(uintptr_t)(mgr + 0xbc0);
            for (int i = 0; i < 12; i++) {
                uint64_t d = rd_ok(w[i], 8) ? *(const uint64_t *)(uintptr_t)w[i] : 0;
                LOG("[mealfind] mgr+%#x = %#llx -> %#llx\n", 0xbc0 + 8 * i, (unsigned long long)w[i], (unsigned long long)d);
            }
        }
    }
    for (uint64_t r = 0x7100000000ull; r < 0x7400000000ull && hits < 48 && GetTickCount64() - t0 < 6000 &&
         VirtualQuery((void *)(uintptr_t)r, &mbi, sizeof mbi); r = (uint64_t)(uintptr_t)mbi.BaseAddress + mbi.RegionSize) {
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            !(mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY))) continue;
        regions++;
        const char *b = (const char *)mbi.BaseAddress, *e = b + mbi.RegionSize - 16;
        for (const char *c = b + 0x20; c < e && hits < 48; c++) {
            c = memchr(c, 'I', (size_t)(e - c));
            if (!c) break;
            if (memcmp(c, "Item_Cook_", 10)) continue;
            if (c[-1] != 0) continue;  // (s98: whole names only, not "IsGet_Item_Cook_*" flag names)
            uint64_t a = (uint64_t)(uintptr_t)c;
            const uint64_t *w = (const uint64_t *)(uintptr_t)((a - 0x20) & ~7ull);
            uint64_t top = *(const uint64_t *)(uintptr_t)(a - 0xc);  // (unaligned read is fine on x64)
            char nm[24];
            int k = 0;
            while (k < 23 && c[k] >= 0x20 && c[k] < 0x7f) nm[k] = c[k], k++;
            nm[k] = 0;
            LOG("[mealfind] %#llx \"%s\" top%s | %016llx %016llx %016llx %016llx\n", (unsigned long long)a, nm, top == a ? "=self" : "",
                (unsigned long long)w[0], (unsigned long long)w[1], (unsigned long long)w[2], (unsigned long long)w[3]);
            hits++;
            c += 9;
        }
    }
    LOG("[mealfind] %d hits in %d regions, %llu ms\n", hits, regions, (unsigned long long)(GetTickCount64() - t0));
}

// (s98) eaten-here tally per dish name (BotW's pouch is not changed yet: removal needs a guest call = exe relink)
static struct { int n; struct { char name[40]; int used, seen; } e[MEAL_MAX]; bool loaded; } EAT;
static int meal_eat_find(const char *nm, bool add) {
    for (int k = 0; k < EAT.n; k++) if (!strcmp(EAT.e[k].name, nm)) return k;
    if (!add || EAT.n >= MEAL_MAX) return -1;
    snprintf(EAT.e[EAT.n].name, sizeof EAT.e[0].name, "%s", nm), EAT.e[EAT.n].used = EAT.e[EAT.n].seen = 0;
    return EAT.n++;
}
static void meal_eat_save(void) {
    char b[2048];
    int o = 0;
    for (int k = 0; k < EAT.n && o < (int)sizeof b - 64; k++)
        if (EAT.e[k].used > 0) o += snprintf(b + o, sizeof b - (size_t)o, "%s:%d:%d;", EAT.e[k].name, EAT.e[k].used, EAT.e[k].seen);
    b[o] = 0;
    SetEnvironmentVariableA("SORA_MEAL_EATEN", o ? b : NULL);
}
static void meal_eat_load(void) {
    if (EAT.loaded) return;
    EAT.loaded = true;
    char b[2048];
    DWORD n = GetEnvironmentVariableA("SORA_MEAL_EATEN", b, sizeof b);
    if (!n || n >= sizeof b) return;
    for (char *s = b, *e; *s && (e = strchr(s, ';')); s = e + 1) {
        *e = 0;
        char nm[40];
        int u, sn;
        if (sscanf(s, "%39[^:]:%d:%d", nm, &u, &sn) == 3) {
            int k = meal_eat_find(nm, true);
            if (k >= 0) EAT.e[k].used = u, EAT.e[k].seen = sn;
        }
    }
    LOG("[meals] eaten tally restored: %d dish names\n", EAT.n);
}

static void meal_scan(void) {
    if (g_meal_find) g_meal_find = 0, meal_find();
    uint64_t el = 0;
    int cnt = 0;
    if (!meal_porch(&el, &cnt)) { if (g_meal_log) LOG("[meals] PorchItem not found\n"); MEAL.n = 0; return; }
    struct { char name[40]; int count, nh; short hp[MEAL_HP]; uint8_t ki[MEAL_HP]; } t[MEAL_MAX];
    int nt = 0, nstr = 0, fdish = -1, food = 0;
    uint64_t c0 = g_meal_hp ? meal_cook0() : 0;
    for (int i = 0; i < cnt; i++) {
        char nm[64];
        uint64_t obj = ((const uint64_t *)(uintptr_t)el)[i];
        if (!meal_str(obj, nm, sizeof nm)) continue;
        nstr++;
        int fi = meal_is_food(nm) ? food++ : -1;  // (s98) this entry's CookEffect0 index
        if (g_meal_log >= 2 && nm[0]) {  // (s97: 0 dishes found with 420 strings read -> dump what the strings are, once)
            static char dumped[64][48];
            static int nd;
            int d = 0;
            while (d < nd && strncmp(dumped[d], nm, 47)) d++;
            if (d == nd && nd < 64) strncpy(dumped[nd++], nm, 47), LOG("[meals] porch[%d] = \"%s\" (obj %#llx)\n", i, nm, (unsigned long long)obj);
        }
        // (s131 user: "I can't open Items with cooked food" = campfire food Item_Roast_03 / Item_RoastFish_01, not pot dishes)
        if (!meal_is_food(nm) || strlen(nm) >= 40) continue;
        if (fdish < 0) fdish = i;
        int j = 0;
        while (j < nt && strcmp(t[j].name, nm)) j++;
        if (j == nt) { if (nt >= MEAL_MAX) continue; strcpy(t[nt].name, nm), t[nt].count = 0, t[nt].nh = 0, nt++; }
        t[j].count++;
        if (t[j].nh < MEAL_HP) {
            int hq = meal_cook_hp(c0, fi);
            float ef = -1, lv = 0;
            if (!ki_cook1(fi, &ef, &lv)) ef = -1;
            t[j].ki[t[j].nh] = (uint8_t)ki_pick(hq, ef, lv);  // (s131 kh2item)
            if (g_kh2item_log) LOG("[kh2item] %s food #%d: %d quarter hearts, effect %g level %g -> %s\n", nm, fi, hq, ef, lv, KI[t[j].ki[t[j].nh]].name);
            t[j].hp[t[j].nh++] = (short)hq;
        }
        if (g_meal_log >= 2) LOG("[meals] dish porch[%d] %s food #%d heal %d quarter hearts\n", i, nm, fi, meal_cook_hp(c0, fi));
    }
    // new list, keeping the eaten-here counts per name; a drop in the game's own count (eaten / dropped in BotW's menu)
    // takes the same off our tally. (s98 fix: the tally lives in EAT, not in the listed rows - a dish eaten down to 0
    // left the list, so the next scan forgot it and it came back.) EAT survives hot reloads via env SORA_MEAL_EATEN.
    meal_eat_load();
    MEAL.n = 0;
    for (int j = 0; j < nt; j++) {
        int seen = t[j].count, k = meal_eat_find(t[j].name, false), used = 0;
        if (k >= 0) {
            if (t[j].count < EAT.e[k].seen) EAT.e[k].used -= EAT.e[k].seen - t[j].count;
            if (EAT.e[k].used < 0) EAT.e[k].used = 0;
            EAT.e[k].seen = seen, used = EAT.e[k].used;
        }
        if (t[j].count - used <= 0) continue;
        strcpy(MEAL.e[MEAL.n].name, t[j].name);
        MEAL.e[MEAL.n].count = t[j].count - used, MEAL.e[MEAL.n].seen = seen, MEAL.e[MEAL.n].used = used;
        MEAL.e[MEAL.n].nh = t[j].nh;
        memcpy(MEAL.e[MEAL.n].hp, t[j].hp, sizeof t[j].hp), memcpy(MEAL.e[MEAL.n].ki, t[j].ki, sizeof t[j].ki);
        MEAL.n++;
    }
    if (g_meal_log || MEAL.ncnt != MEAL.n) LOG("[meals] porch %d entries read (%d strings), %d kinds of dish listed\n", cnt, nstr, MEAL.n);
    if (g_meal_log >= 2 && fdish >= 0) {  // (s98 recon) PorchItem_Value1 = the s32 array of 420 (+0xa8 group): dump the first dish's element
        const SoraSites *ss = sora_sites();
        uint64_t mgr = *(const uint64_t *)(uintptr_t)ss->gdt_mgr;
        uint64_t param = *(const uint64_t *)(uintptr_t) * (const uint64_t *)(uintptr_t)(mgr + (g_meal_tp ? 0xbd8 : 0xbe0));
        uint32_t n = *(const uint32_t *)(uintptr_t)(param + 0xa8);
        uint64_t arr = *(const uint64_t *)(uintptr_t)(param + 0xb0);
        LOG("[meals] s32 arrays: %u (first dish porch[%d])\n", n, fdish);
        uint32_t nv = *(const uint32_t *)(uintptr_t)(param + 0xf8);  // (s98 recon) vec2 arrays: CookEffect0/1 = 120 each
        uint64_t av = *(const uint64_t *)(uintptr_t)(param + 0x100);
        LOG("[meals] vec2 arrays: %u\n", nv);
        for (uint32_t i = 0; nv < 4096 && rd_ok(av, 8ull * nv) && i < nv; i++) {
            uint64_t af = ((const uint64_t *)(uintptr_t)av)[i];
            if (!rd_ok(af, 16)) continue;
            uint32_t c = *(const uint32_t *)(uintptr_t)af;
            uint64_t el2 = *(const uint64_t *)(uintptr_t)(af + 8);
            LOG("[meals] vec2 array #%u: %u entries\n", i, c);
            if (c > 512 || !rd_ok(el2, 8ull * c)) continue;
            for (uint32_t d = 0, shown = 0; d < c && shown < 8; d++) {
                uint64_t o = ((const uint64_t *)(uintptr_t)el2)[d];
                if (!rd_ok(o, 0x20)) continue;
                const float *f = (const float *)(uintptr_t)(o + 0x10);
                if (f[0] == 0 && f[1] == 0) continue;
                shown++;
                LOG("[meals]   [%u] %g %g (next %g %g)\n", d, f[0], f[1], f[2], f[3]);
            }
        }
        for (uint32_t i = 0; n < 4096 && rd_ok(arr, 8ull * n) && i < n; i++) {
            uint64_t af = ((const uint64_t *)(uintptr_t)arr)[i];
            if (!rd_ok(af, 16) || *(const uint32_t *)(uintptr_t)af != 420) continue;
            uint64_t el2 = *(const uint64_t *)(uintptr_t)(af + 8);
            if (!rd_ok(el2, 8ull * 420)) continue;
            for (int d = fdish; d < fdish + 4 && d < 420; d++) {
                uint64_t o = ((const uint64_t *)(uintptr_t)el2)[d];
                if (!rd_ok(o, 0x30)) continue;
                const uint32_t *w = (const uint32_t *)(uintptr_t)o;
                LOG("[meals] s32 array #%u [%d] obj %#llx: %08x %08x %08x %08x | %d %d %d %d\n", i, d, (unsigned long long)o, w[0], w[1], w[2], w[3],
                    (int)w[4], (int)w[5], (int)w[6], (int)w[7]);
            }
        }
    }
    if (g_meal_log >= 2) {  // (s97) the dump also shows raw bytes of the first element (the string scan is a GUESS)
        uint64_t o0 = ((const uint64_t *)(uintptr_t)el)[0];
        if (rd_ok(o0, 0x40)) {
            const uint64_t *w = (const uint64_t *)(uintptr_t)o0;
            LOG("[meals] porch[0] obj %#llx: %016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n", (unsigned long long)o0,
                (unsigned long long)w[0], (unsigned long long)w[1], (unsigned long long)w[2], (unsigned long long)w[3], (unsigned long long)w[4],
                (unsigned long long)w[5], (unsigned long long)w[6], (unsigned long long)w[7]);
            if (rd_ok(o0, 0x100))  // (s98) +0x60.. = the second string (mValue) header
                LOG("[meals] porch[0] +0x60: %016llx %016llx %016llx %016llx (%d string tops)\n", (unsigned long long)w[12],
                    (unsigned long long)w[13], (unsigned long long)w[14], (unsigned long long)w[15], (meal_str(o0, (char[64]){0}, 64), meal_str_n));
        }
        g_meal_log = 1;
    }
    MEAL.ncnt = MEAL.n;
    if (MEAL.cursor >= MEAL.n) MEAL.cursor = MEAL.n ? MEAL.n - 1 : 0;
}

static bool meal_owns_items(void) { return g_meals != 0; }  // (s93 review: sora_slate.h keeps off the Items slot while on)
static int meal_top_state(void) {  // the Items entry: 5 greyed (as before) unless "meals" is on and a dish is in the pouch
    if (!g_meals) return 5;
    if (M.sub != MEAL_SUB && (MEAL.tick++ % 30) == 0) meal_scan();
    return MEAL.n > 0 ? 0 : 5;
}

// (s98 b13) exe export (NULL on an exe linked before this: the tally alone, as before). int linkpos_pouch_remove(name):
// 1 = BotW's PauseMenuDataMgr::removeItem was called, < 0 = refused (recon/S98_HELPER_POUCH.md).
static int g_meal_del = 0;  // live "mealdel": 0 = tally only (default until the user tests), 1 = remove from BotW's pouch on eat
static struct { bool bound; int (*remove)(const char *); } MEALX;
static void meal_pouch_bind(void) {
    if (MEALX.bound) return;
    MEALX.bound = true;
    MEALX.remove = (int (*)(const char *))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_pouch_remove");
    LOG("[meals] exe export linkpos_pouch_remove: %s\n", MEALX.remove ? "yes" : "NO (mealdel falls back to the tally)");
}

// ---- (s98 potion) KH2's item use. Vanilla KH2: the Item command queues Sora's ITEM motion (motion id 9 = A110, mset slot 38,
// 50 frames at 60 fps; PHH kh2ctl.py ITEM = 9, use_item(): ground + idle only, blend 8, idle queued after) and the motion's
// frame trigger 6 (= "use_item", frame 18 in the bake, P_EX100.mset slot 38) takes the item and starts an ItemTask (timer
// 30 KH frames): its first run plays the user's effect SE (00effect.bar system element 0 = se000 #100), the run that brings
// the timer to 0 plays the item's own effect SE (system element 1 = Potion = se000 #101) and applies the item
// (PHH items.py ItemTask, re:0x3F6B30 / re:0x3F6D00). The motion's own frame 0 voice row (vtbl 3) and frame 10 SE (se000 #305)
// come with play() from the baked sfx.bin triggers. Live "set mealanim 0" = the old instant heal.
// GUESS: a cooked dish uses the Potion's effect SE (#101); the visual 00effect elements 0 / 1 are not baked (SE only).
// GUESS: in the air / in a form / in Limit KH2 "still lets the command through" (PHH note): the same task without the motion.
static int g_meal_anim = 1;
#define MIT_KH 30.0f  // ItemTask timer (PHH items.py ITEM_TIMER, KH re:0x3F6790)
static struct { int phase; float use_at, timer; int q; bool own; char name[40]; int ki; } MIT;  // phase 0 idle, 1 motion (waiting for use_item), 2 task running

static void meal_heal_now(const char *nm, int q, bool own) {
    extern int g_hud_life_max;
    int before = linkpos_link_life();
    if (g_kh2item && MIT.ki > KI_NONE) q = ki_apply(MIT.ki, q), MIT.ki = KI_NONE;  // (s131 kh2item: the KH2 item's MP / Drive extras, or its HP percent)
    int v = q > 0 && before > 0 ? link_heal(q) : before;  // (an elixir heals 0; a Sora who died on the way gets nothing)
    LOG("[meals] ate %s: life %d -> %d (max %d, +%d quarter hearts %s)\n", nm, before, v, g_hud_life_max, q,
        own ? "= the dish's CookEffect0" : "GUESS mealq");
}

// (s98 merge b13-pouch + b13-potion) the dish leaves BotW's real pouch at the moment KH2 takes the item (the use_item
// frame; at once without the motion). mealdel 1 = exe export linkpos_pouch_remove (PauseMenuDataMgr::removeItem); the
// tally (EAT) stays: the next scan sees the pouch count drop (EAT.used -= seen - count), so nothing counts twice.
static void meal_take(const char *nm) {
    if (!g_meal_del) { LOG("[meals] %s taken: BotW's pouch NOT changed (mealdel 0), tally only\n", nm); return; }
    meal_pouch_bind();
    int rm = MEALX.remove ? MEALX.remove(nm) : -9;
    LOG("[meals] %s taken: BotW's pouch %s\n", nm, rm == 1 ? "removeItem called" : rm == -9 ? "NOT changed: this exe has no linkpos_pouch_remove, tally only"
                                                                                        : "NOT changed: remove refused, tally only");
    MEAL.tick = 0;
}

static void meal_untake(const char *nm) {  // the motion was cut before the use_item frame: KH2 never took the item
    int k = meal_eat_find(nm, false);
    if (k >= 0 && EAT.e[k].used > 0) EAT.e[k].used--, meal_eat_save();
    MEAL.tick = 0;
}

// (s99 potfx) KH2's item-use visuals: 00effect.bar system element 0 (the user effect, task first run) and element 1 (Potion's own,
// timer end), packed as fx "item_user" / "item_potion" (tools_s99/potfx). Spawned from sora.c's per-frame meal_fx_show (it has the
// feet), bound to his centre like the level-up burst (system.15; element 1 has bind 0x4009 itself; element 0 has none = GUESS, same
// centre). Missing pack entry: logged once, nothing else changes. Live: set mealfx 0/1 (default 1).
static int g_meal_fx = 1, MFX_req, MFX_h[2] = {-1, -1};
static bool MFX_warned[2];
static void meal_fx_show(const float feet[3], float face, float to_u) {
    static const char *nm[2] = {"item_user", "item_potion"};
    float ctr[3] = {feet[0], feet[1] + 90 * to_u, feet[2]};  // (GUESS 90 cm, as sl_lvup_show / Cure)
    for (int k = 0; k < 2; k++) {
        if (MFX_h[k] >= 0) { if (sora_fx_alive(MFX_h[k])) sora_fx_move(MFX_h[k], ctr, face); else MFX_h[k] = -1; }
        if (!(MFX_req & (1 << k))) continue;
        MFX_req &= ~(1 << k);
        if (!g_meal_fx) continue;
        if (sora_fx_id(nm[k]) < 0) {
            if (!MFX_warned[k]) MFX_warned[k] = true, LOG("[meals] fx \"%s\" not in fx.bin (install fx_s99.bin): no item visual\n", nm[k]);
            continue;
        }
        MFX_h[k] = sora_fx_event(nm[k], ctr, face, to_u);
        LOG("[meals] item fx %s spawned (handle %d)\n", nm[k], MFX_h[k]);
    }
}

static void meal_item_tick(float kh) {
    if (MIT.phase == 1) {
        if (S.special != SP_ITEM || S.motion != 9) {
            LOG("[meals] item motion cut before the use_item frame: %s not taken\n", MIT.name);
            meal_untake(MIT.name), MIT.phase = 0;
            meal_scan();
            return;
        }
        if (S.clock < MIT.use_at) return;
        MIT.phase = 2, MIT.timer = MIT_KH;
        LOG("[meals] use_item frame (clock %.1f): %s taken\n", S.clock, MIT.name);
        meal_take(MIT.name);
    }
    if (MIT.phase != 2) return;
    if (MIT.timer >= MIT_KH) sora_sfx_event("item_user", 1.0f), MFX_req |= 1;  // (the task's first run: system element 0's SE, se000 #100)
    MIT.timer -= kh;
    if (MIT.timer > 0) return;
    sora_sfx_event("item_potion", 1.0f), MFX_req |= 2;  // (the item's own element: Potion's system element 1, se000 #101; GUESS for a dish)
    meal_heal_now(MIT.name, MIT.q, MIT.own);
    MIT.phase = 0;
}

static void meal_use(int i) {
    // (s98) the dish's own heal (CookEffect0 x, quarter hearts) when read; else the flat mealq guess
    int u = MEAL.e[i].used, hp = u < MEAL.e[i].nh ? MEAL.e[i].hp[u] : MEAL.e[i].nh ? MEAL.e[i].hp[MEAL.e[i].nh - 1] : -1;
    bool own = g_meal_hp && hp >= 0;
    int q = own ? hp : g_meal_q > 0 ? g_meal_q : 1;
    MEAL.e[i].used++, MEAL.e[i].count--;
    int k = meal_eat_find(MEAL.e[i].name, true);  // (s98 fix) the tally that outlives the row
    if (k >= 0) EAT.e[k].used = MEAL.e[i].used, EAT.e[k].seen = MEAL.e[i].seen, meal_eat_save();
    if (MIT.phase == 1) meal_take(MIT.name);  // (s98 review) a second dish mid-use: the first one is taken + healed now, not lost
    if (MIT.phase) meal_heal_now(MIT.name, MIT.q, MIT.own), MIT.phase = 0;
    // (s131 kh2item; review 5/6) the same fallback as meal_title (more than MEAL_HP of one dish: the last known item), for both paths
    int kit = g_kh2item ? (u < MEAL.e[i].nh ? MEAL.e[i].ki[u] : MEAL.e[i].nh ? MEAL.e[i].ki[MEAL.e[i].nh - 1] : KI_NONE) : KI_NONE;
    MIT.ki = kit;
    if (kit > KI_NONE) LOG("[kh2item] %s used as %s\n", MEAL.e[i].name, KI[kit].name);
    if (!g_meal_anim) { snprintf(MIT.name, sizeof MIT.name, "%s", MEAL.e[i].name), meal_take(MEAL.e[i].name), meal_heal_now(MEAL.e[i].name, q, own); return; }
    snprintf(MIT.name, sizeof MIT.name, "%s", MEAL.e[i].name), MIT.q = q, MIT.own = own;
    // PHH use_item(): the motion only on the ground with nothing else playing (act idle, no special); base Sora (a form's mset
    // has its own ITEM slot this bake does not carry)
    bool ground = S.act == ACT_IDLE && S.takeoff <= 0 && !S.air_frames && !S.special && !fm_in_form() && !IN_LIMIT && mot_of(9);
    if (ground) {
        play(9), S.special = SP_ITEM, S.next = 0;
        MIT.phase = 1, MIT.use_at = mot_frame_trigger(S.mot, 6, 18);  // trigger 6 = use_item (slot 38: frame 18)
        LOG("[meals] item motion A110 (slot %d, %d frames), use_item at %.0f\n", S.mot->slot, S.mot->frames, MIT.use_at);
    } else {
        MIT.phase = 2, MIT.timer = MIT_KH;  // the item still works: the task alone
        LOG("[meals] no item motion (air / busy / form): the task alone\n");
        meal_take(MIT.name);
    }
}

// the page's input; returns -1 (a use is done here)
static int meal_menu_tick(uint64_t *press) {
    if (!g_meals || MEAL.n <= 0) { M.sub = -1, M.cursor = 2; return -1; }
    if (*press & BTN_UP) MEAL.cursor = (MEAL.cursor + MEAL.n - 1) % MEAL.n, sora_sfx_event("cursor", 1);
    if (*press & BTN_DOWN) MEAL.cursor = (MEAL.cursor + 1) % MEAL.n, sora_sfx_event("cursor", 1);
    if (MEAL.cursor < MEAL.scroll) MEAL.scroll = MEAL.cursor;
    if (MEAL.cursor >= MEAL.scroll + MEAL_ROWS) MEAL.scroll = MEAL.cursor - MEAL_ROWS + 1;
    if (*press & (BTN_JUMP | BTN_LEFT)) {
        *press &= ~BTN_JUMP;
        M.sub = -1, M.cursor = 2;
        sora_sfx_event("cancel", 1), LOG("[meals] Items closed\n");
        return -1;
    }
    if (*press & BTN_ATTACK) {
        *press &= ~BTN_ATTACK;
        // (s93 review: no eating at 0 life - link_heal's fallback is linkpos_link_heal, which must not revive a dying Link)
        if (M.dead > 0 || linkpos_link_life() <= 0 || MEAL.cursor < 0 || MEAL.cursor >= MEAL.n) { sora_sfx_event("error", 1); return -1; }
        meal_use(MEAL.cursor);  // (s98 potion: KH2's ITEM motion A110 + item SEs, mealanim)
        M.dead = 8, M.sub = -1, M.cursor = 0, M.sub_cursor = 0;  // re:0x3B21D0: executed
        meal_scan();
    }
    return -1;
}

static void meal_open(void) {
    meal_scan();
    MEAL.cursor = MEAL.scroll = 0;
    M.sub = MEAL_SUB, M.sub_cursor = 0;
    sora_sfx_event("submenu", 1);
    LOG("[meals] Items open: %d kinds\n", MEAL.n);
}

// "Item_Cook_C_17" -> "Dish C-17" (BotW's own display names live in its message files: NEEDS NAME BAKE)
static void meal_title(const char *nm, int count, char *out, int cap) {
    char b[40];
    int ki = KI_NONE;  // (s131 kh2item) the KH2 item name replaces "Dish C-17"; a second row mapping to the same item keeps the dish id
    if (g_kh2item) for (int r = 0; r < MEAL.n; r++) if (!strcmp(MEAL.e[r].name, nm)) {
        int u = MEAL.e[r].used;
        ki = u < MEAL.e[r].nh ? MEAL.e[r].ki[u] : MEAL.e[r].nh ? MEAL.e[r].ki[MEAL.e[r].nh - 1] : KI_NONE;
        if (ki > KI_NONE) {
            bool dup = false;
            for (int r2 = 0; r2 < MEAL.n; r2++) if (r2 != r && MEAL.e[r2].nh && MEAL.e[r2].ki[0] == MEAL.e[r].ki[0]) dup = true;
            const char *s0 = strncmp(nm, "Item_Cook_", 10) == 0 ? nm + 10 : nm;
            char id[24];
            int k0 = 0;
            for (; s0[k0] && k0 < 20; k0++) id[k0] = s0[k0] == '_' ? '-' : s0[k0];
            id[k0] = 0;
            if (dup && count > 1) snprintf(out, (size_t)cap, "%s (%s) x%d", KI[ki].name, id, count);
            else if (dup) snprintf(out, (size_t)cap, "%s (%s)", KI[ki].name, id);
            else if (count > 1) snprintf(out, (size_t)cap, "%s  x%d", KI[ki].name, count);
            else snprintf(out, (size_t)cap, "%s", KI[ki].name);
            return;
        }
        break;
    }
    const char *s = strncmp(nm, "Item_Cook_", 10) == 0 ? nm + 10 : nm;
    int k = 0;
    for (; s[k] && k < 20; k++) b[k] = s[k] == '_' ? '-' : s[k];
    b[k] = 0;
    if (count > 1) snprintf(out, (size_t)cap, "Dish %s  x%d", b, count);
    else snprintf(out, (size_t)cap, "Dish %s", b);
}

// fills the SoraHudMenu for the page (called from hud_update when M.sub == MEAL_SUB) and sends the text quads
static void meal_hud(SoraHudMenu *m) {
    static SoraHudQuad mq[256];
    static void (*text)(const SoraHudQuad *, int);
    static bool tried;
    if (!tried) {
        tried = true;
        text = (void (*)(const SoraHudQuad *, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_text");
        if (!text) LOG("[meals] sora_hud_menu_text not in this exe (NEEDS EXE RELINK): rows show the generic Items label\n");
        if (!CAMP.tried) camp_load();
        if (!CAMP.ok) LOG("[meals] camp_seq.bin not loaded: no font table, rows show the generic Items label\n");
    }
    m->sub = 2;  // the Drive page's look (no title)
    int ns = MEAL.n - MEAL.scroll < MEAL_ROWS ? MEAL.n - MEAL.scroll : MEAL_ROWS;
    if (ns < 1) ns = 1;
    m->n_sub = ns;
    m->sub_cursor = MEAL.cursor - MEAL.scroll;
    bool txt = text && CAMP.ok;
    CsOut o = {mq, 0, 256};
    for (int i = 0; i < ns; i++) {
        m->sub_label[i] = txt ? 255 : 2, m->sub_state[i] = 0, m->sub_cost[i] = 0;  // 255 = no atlas strip (the text quads draw)
        int k = MEAL.scroll + i;
        if (!txt || k >= MEAL.n) continue;
        char t[64];
        uint8_t msg[64];
        meal_title(MEAL.e[k].name, MEAL.e[k].count, t, sizeof t);
        int len = 0;
        for (int c = 0; t[c] && len < 63; c++) {
            uint8_t b = t[c] == ' ' ? 0x01 : cs_ascii(&CAMP.seq, t[c]);
            if (b) msg[len++] = b;
        }
        bool sel = i == m->sub_cursor;
        float y = 410.0f - 22.0f * (float)ns + 22.0f * (float)i, x = 28.0f + 16.0f + (sel ? 8.0f : 0.0f) + 10.0f;
        int q0 = o.n;
        static const float W[4] = {1, 1, 1, 1};
        cs_text(&CAMP.seq, &o, msg, len, x, y - 12.0f, 0.8f, W, NULL, NULL);
        uint32_t col = sel ? 0x80E6E6E6 : 0x80B4B4B4;  // KH2's selected / plain row text (sora_hud.c COL_ST / SEL_ST state 0)
        for (int q = q0; q < o.n; q++) for (int c = 0; c < 4; c++) mq[q].col[c] = col;
    }
    if (txt) text(mq, o.n);
}

// (s98) the same text rows for another sub page (the Slate page: "Magnesis" instead of the cmd.bin placeholder label
// "Thunder"). Returns false when the exe export / font table is missing (the caller keeps its atlas labels).
static bool sub_text_rows(SoraHudMenu *m, const char *const *rows, int ns, int sel_row) {
    static SoraHudQuad sq[256];
    static void (*text)(const SoraHudQuad *, int);
    static bool tried;
    if (!tried) {
        tried = true;
        text = (void (*)(const SoraHudQuad *, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_text");
        if (!CAMP.tried) camp_load();
    }
    if (!text || !CAMP.ok) return false;
    CsOut o = {sq, 0, 256};
    for (int i = 0; i < ns; i++) {
        m->sub_label[i] = 255;
        uint8_t msg[64];
        int len = 0;
        for (int c = 0; rows[i][c] && len < 63; c++) {
            uint8_t b = rows[i][c] == ' ' ? 0x01 : cs_ascii(&CAMP.seq, rows[i][c]);
            if (b) msg[len++] = b;
        }
        bool sel = i == sel_row;
        float y = 410.0f - 22.0f * (float)ns + 22.0f * (float)i, x = 28.0f + 16.0f + (sel ? 8.0f : 0.0f) + 10.0f;
        int q0 = o.n;
        static const float W[4] = {1, 1, 1, 1};
        cs_text(&CAMP.seq, &o, msg, len, x, y - 12.0f, 0.8f, W, NULL, NULL);
        uint32_t col = m->sub_state[i] >= 4 ? 0x80787878 : sel ? 0x80E6E6E6 : 0x80B4B4B4;  // (greyed rune: dim, GUESS shade)
        for (int q = q0; q < o.n; q++) for (int c = 0; c < 4; c++) sq[q].col[c] = col;
    }
    text(sq, o.n);
    return true;
}

static bool meal_live_set(const char *key, int v) {
    if (!strcmp(key, "meals")) {
        g_meals = v != 0;
        LOG("[meals] %s\n", g_meals ? "on: Items lists cooked dishes" : "off: Items greyed as before");
        if (!g_meals && M.sub == MEAL_SUB) M.sub = -1, M.cursor = 2;
    } else if (!strcmp(key, "kh2item")) g_kh2item = v != 0, LOG("[kh2item] %s\n", g_kh2item ? "on: dishes shown / used as KH2 items" : "off: Dish rows, BotW heal only");
    else if (!strcmp(key, "kh2itemhp")) g_kh2item_hp = v != 0, LOG("[kh2item] kh2itemhp %d: %s\n", g_kh2item_hp, g_kh2item_hp ? "HP = the KH2 item's percent of max" : "HP = the dish's own value");
    else if (!strcmp(key, "kh2itemlog")) g_kh2item_log = v != 0;
    else if (!strcmp(key, "mealq")) g_meal_q = v < 1 ? 1 : v > 120 ? 120 : v;
    else if (!strcmp(key, "meallog")) g_meal_log = v < 0 ? 0 : v;  // (s97: 2 = dump the porch names once)
    else if (!strcmp(key, "mealfx")) g_meal_fx = v != 0, LOG("[meals] mealfx %d: KH2 item-use visual effects (system.0 / .1)\n", g_meal_fx);  // (s99 potfx)
    else if (!strcmp(key, "mealanim")) g_meal_anim = v != 0, LOG("[meals] mealanim %d: %s\n", g_meal_anim, g_meal_anim ? "KH2 item motion + SE, heal 30 frames after the use_item frame" : "instant heal");  // (s98 potion)
    else if (!strcmp(key, "mealhp")) g_meal_hp = v != 0;  // (s98: 1 = each dish heals its own CookEffect0 value, 0 = flat mealq)
    else if (!strcmp(key, "mealtp")) g_meal_tp = v != 0, MEAL.elems = 0;  // (s98: which gdt TriggerParam)
    else if (!strcmp(key, "mealfind")) g_meal_find = v != 0;  // (s98: one-shot memory search for "Item_Cook_")
    else if (!strcmp(key, "mealdel")) {  // (s98 b13: 1 = eating also removes the dish from BotW's pouch; needs the relinked exe)
        g_meal_del = v != 0;
        if (g_meal_del) meal_pouch_bind();
        LOG("[meals] mealdel %d: %s\n", g_meal_del, g_meal_del ? (MEALX.remove ? "eating removes the dish from BotW's pouch" : "exe has no linkpos_pouch_remove: tally only") : "tally only");
    } else return false;
    return true;
}

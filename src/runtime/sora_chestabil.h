// sora_chestabil.h: (s102 b16-chestabil, s103 review) BotW treasure chests teach Sora KH2 support abilities.
// recon/S102_HELPER_CHESTABIL.md. Included from sora.c after sora_chestev.h (needs SLX gdt getters, UL, SLS, ul_kb_label, ab_*, CA, AB).
//
// s103 (review): DLL ONLY. Chests are COUNTED from BotW's own game data: every chest has a bool flag named after its map
// object (MainField_TBox_Field_Iron_<hash>, CDungeon_TBox_Dungeon_Stone_<hash>, ... 1621 names with "_TBox_" in the 1.9.0
// Bootup.pack gamedata = data/levels/gamedata_flag_names.txt; GUESS until the live log: the flag is true once that chest
// is open). ca_tick reads CA_PER_FRAME names a frame through linkpos_gdt_bool (game thread, inside the frame, as sora_unlock.h)
// = one full sweep every ~1 s, then waits CA_EVERY s. So the chests a save opened before this mod count too, nothing needs the
// "item got" moment, and a chest is never counted twice. (The s102 helper's TBox-actor-near-Link + item-got edge detection
// needed an exe relink and could count a talk next to a chest: dropped.)
//
// Reward rule: reward k (k = 1, 2, ...) comes at CA_T[k - 1] chests opened (1, 3, 6, 10, 15, ... = sooner at the start, then
// every ~20-50). Rewards given = popcount(CA.mask). Each reward = the next pool ability Sora does not know yet (a level-taught
// one then comes early; once the level teaches it, the bit stays and nothing is lost). Awards only once sora_unlock.h says the
// live flags are THIS save's (UL.synced): a load must not give abilities for the last world's chests.
// Live keys: set chestabil 0/1 (default 1), chestlog 1 (log each sweep), chestfake N (pretend N chests are open; -1 = real),
// chestgive 1 (the next pool ability now; it counts as one chest reward), chestreset 1 (forget the chest abilities: the next
// sweep gives back what the count earns), chestfx 0/1 (Second Chance / Once More, default 1 since s104). Log lines "[chestabil] ...".
//
// Effects (chestfx, s104 default ON): Second Chance (415) / Once More (416) with KH2 rules, clamped in the exe's player
// addDamage hook (linkpos.c survive_clamp) before BotW takes the life off - see ca_survive.
// The other chest abilities are only learnable (no effect yet), as the Abilities page already lists them.
#pragma once

static int g_chestabil = 1, g_chestlog = 0, g_chestfx = 1, g_chestfake = -1;  // (s104: chestfx default 1, the exe clamp is safe)

static const uint16_t CA_FIRST[] = {415, 416, 402, 403, 405, 406, 411, 542, 414, 540};  // Second Chance, Once More, Leaf Bracer, Magic Lock-On, Draw, Jackpot, Item Boost, Damage Control, Defender, Drive Converter
static const uint16_t CA_T[] = {1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 70, 85, 100, 120, 140, 160, 180, 200, 230, 260, 300, 350, 400, 450, 500};
#define CA_NT ((int)(sizeof CA_T / sizeof CA_T[0]))
#define CA_PER_FRAME 32
#define CA_EVERY 3.0f  // game s between sweeps

static int ca_pool(uint16_t out[AB_MAX_LEARN]) {
    int n = 0;
    if (g_chestfirst) out[n++] = AB_GUARD;  // (s110: chest #1 Guard, then CA_FIRST: #2 Second Chance, #3 Once More, ...)
    for (size_t i = 0; i < sizeof CA_FIRST / sizeof *CA_FIRST; i++) out[n++] = CA_FIRST[i];
    for (int i = 0; i < AB_N && n < AB_MAX_LEARN; i++) {  // then the other support rows (kind 0), AB_TAB order
        int id = AB_TAB[i].id;
        if (AB_TAB[i].kind != 0) continue;
        bool dup = false;
        for (int k = 0; k < n; k++) dup |= out[k] == id;
        if (!dup) out[n++] = (uint16_t)id;
    }
    return n;
}
// the next pool id not learned yet and not given by a chest yet (0 = the pool is empty)
static int ca_next(void) {
    uint16_t pool[AB_MAX_LEARN], o[AB_MAX_LEARN];
    int np = ca_pool(pool);
    abm_t have = ab_mask(o, ab_order(o)) | CA.mask;
    for (int i = 0; i < np; i++) if (ab_bit(pool[i]) && !(have & ab_bit(pool[i]))) return pool[i];
    return 0;
}

static void ca_popup(int id) {
    static void (*fn)(const SoraHudLevelUp *);
    static int tried;
    if (!g_kbpop) return;
    if (!tried) tried = 1, fn = (void (*)(const SoraHudLevelUp *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_levelup");
    if (!fn) { LOG("[chestabil] popup: sora_hud_levelup not in this exe: log line only\n"); return; }
    if (!ul_kb_label(id)) { LOG("[chestabil] popup: no label ab%d in lvup.bin: log line only\n", id); return; }
    SoraHudLevelUp u = {.level = SL.lv < 1 ? 1 : SL.lv > 99 ? 99 : SL.lv, .n_lines = 1, .line = {SORA_LVUP_ABILITY((uint32_t)id)}, .name = SORA_LVUP_NAME_SORA};
    fn(&u);
    LOG("[chestabil] popup shown for %s (SE %d)\n", ab_name(id), sora_sfx_event("bonus_up", 1.0f));
}

// give the next pool ability (a chest reward: CA.mask); returns its id (0 = none left)
static int ca_award(const char *why, bool popup) {
    int id = ca_next();
    if (!id) return 0;
    CA.mask |= ab_bit(id), AB.dirty = true;
    ab_tick();  // (learn + auto-equip now, so the Abilities page and the log agree)
    // (s110 live log: a new game's Guard came "NOT equipped (AP short); AP 2/2" - Scan (2 AP, LV 1) had the AP) Guard is
    // Roxas' only defence: make room by turning off other equipped support abilities from the end of the table
    if (id == AB_GUARD && g_chestfirst && !ab_on(AB.on, id))
        for (int i = AB_N - 1; i >= 0 && ab_used(AB.on) + AB_TAB[ab_idx(id)].ap > ab_ap_max(); i--)
            if ((AB.on >> i & 1) && AB_TAB[i].id != AB_GUARD && AB_TAB[i].ap > 0)
                AB.on &= ~((abm_t)1 << i), LOG("[chestabil] Guard needs the AP: %s turned off\n", ab_name(AB_TAB[i].id));
    if (!ab_on(AB.on, id) && ab_used(AB.on) + AB_TAB[ab_idx(id)].ap <= ab_ap_max()) AB.on |= ab_bit(id);  // (s110: Guard was "seen" at LV 1 under the old rule: ab_sync would leave it off)
    sl_persist_save();
    LOG("[chestabil] %s: Sora learned %s (id %d)%s; AP %d/%d\n", why, ab_name(id), id, ab_on(AB.on, id) ? ", equipped" : ", NOT equipped (AP short)", ab_used(AB.on), ab_ap_max());
    if (popup) ca_popup(id);
    return id;
}
static int ca_popcount(abm_t m) { int n = 0; for (; m; m &= m - 1) n++; return n; }
// (s110 chestfirst) one more reward at 2 chests: 1, 2, 3, 6, 10, ... (Guard, Second Chance, Once More, then the old order one step later)
static int ca_rewards_for(int chests) { int k = 0; while (k < CA_NT && chests >= CA_T[k]) k++; return k + (g_chestfirst && chests >= 2); }

// the chest flag names (once, from the gamedata name list)
static struct {
    char *buf;
    uint32_t *off;
    int n, i, open, found, last_open, last_found;  // i = the sweep's next name; open / found = this sweep so far; last_* = the last full sweep
    float next;
    bool loaded, have;
} CN;
static void ca_load_names(void) {
    CN.loaded = true;
    const char *p = getenv("SAVAGEWIND_SORA_GDT_NAMES");
    if (!p || !*p) p = "D:/SoraBotW/data/levels/gamedata_flag_names.txt";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[chestabil] no flag name list %s: chests are not counted\n", p); return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *all = sz > 0 ? (char *)malloc((size_t)sz + 1) : NULL;
    if (!all || fread(all, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(all); LOG("[chestabil] could not read %s\n", p); return; }
    fclose(f);
    all[sz] = 0;
    CN.buf = (char *)malloc((size_t)sz + 1), CN.off = (uint32_t *)malloc(sizeof(uint32_t) * 4096);
    if (!CN.buf || !CN.off) { free(all); return; }
    size_t w = 0;
    for (char *s = all, *e; *s; s = e) {
        e = s + strcspn(s, "\r\n");
        char c = *e;
        *e = 0;
        // chests only: "<map>_TBox_<kind>_<hash>"; not "Wind_Relic_TBox_Appear" (a demo flag) nor the test map
        if (strstr(s, "_TBox_") && !strstr(s, "_Appear") && !strstr(s, "MarioClubTest") && e - s < 90 && CN.n < 4096) {
            CN.off[CN.n++] = (uint32_t)w;
            memcpy(CN.buf + w, s, (size_t)(e - s) + 1), w += (size_t)(e - s) + 1;
        }
        if (c) e++;
        while (*e == '\r' || *e == '\n') e++;
    }
    free(all);
    LOG("[chestabil] %d chest flags from %s (one sweep every %.0f s, %d a frame)\n", CN.n, p, CA_EVERY, CA_PER_FRAME);
}

// per frame, after ul_tick (the gdt getters work only in the game's frame)
static void ca_tick(void) {
    if (!g_chestabil) return;
    if (!CN.loaded) ca_load_names();
    if (!CN.n || !SLX.gdt_bool) return;
    if (CN.i == 0 && SL.clock < CN.next) return;
    for (int k = 0; k < CA_PER_FRAME && CN.i < CN.n; k++, CN.i++) {
        int v = 0;
        if (SLX.gdt_bool(CN.buf + CN.off[CN.i], &v)) CN.found++, CN.open += v != 0;
    }
    if (CN.i < CN.n) return;
    bool ch = !CN.have || CN.open != CN.last_open || CN.found != CN.last_found;
    CN.last_open = CN.open, CN.last_found = CN.found, CN.have = true;
    CN.i = CN.open = CN.found = 0, CN.next = SL.clock + CA_EVERY;
    if (!CN.last_found) {
        static int said;
        if (!said++) LOG("[chestabil] sweep: none of the %d chest flags found (gdt refused / not loaded yet)\n", CN.n);
        return;
    }
    int chests = g_chestfake >= 0 ? g_chestfake : CN.last_open, want = ca_rewards_for(chests), got = ca_popcount(CA.mask);
    bool ok = SLS.slot >= 0 && SLS.slot == UL.slot_seen && UL.synced;
    if (ch || g_chestlog)
        LOG("[chestabil] sweep: %d of %d chest flags open (%d found)%s: %d reward(s) earned, %d given%s\n", CN.last_open, CN.n, CN.last_found,
            g_chestfake >= 0 ? " (chestfake)" : "", want, got, ok ? "" : " (waiting: the live flags are not this save's yet)");
    if (!ok) return;
    {   // (s110 chestfirst) a save whose chest rewards were dealt in the old order (no Guard from a chest, Guard not known): deal them again once
        uint16_t o[AB_MAX_LEARN];
        bool guard_known = (ab_mask(o, ab_order(o)) & ab_bit(AB_GUARD)) != 0;
        if (g_chestfirst && CA.mask && !guard_known && want >= 1) {
            LOG("[chestabil] chestfirst: re-dealing %d chest reward(s) in the new order (Guard, Second Chance, ...)\n", got);
            CA.mask = 0, got = 0, AB.dirty = true, ab_tick();
        }
    }
    int gave = 0;
    while (got < want) {
        int id = ca_award("chest", gave < 4);  // (a save with many chests opened: the first 4 popups, the rest in the log)
        if (!id) {
            static int said;
            if (!said++) LOG("[chestabil] the chest pool is empty: %d more reward(s) give nothing\n", want - got);
            break;
        }
        got++, gave++;
    }
}

// (s104) Second Chance (415) / Once More (416) are clamped in the EXE's player addDamage hook (linkpos.c survive_clamp,
// before BotW takes the life off = no death started): each frame this says which ones are equipped. KH2 rules: Second
// Chance = a fatal hit from > 1 quarter heart leaves 1; Once More = at 1 quarter heart, a hit inside a combo leaves 1.
// An exe without the export: no effect (the s103 write of life after the hit could land mid-death, removed).
// life: Link's quarter hearts this frame, returned unchanged.
static int ca_survive(int life) {
    static bool bound;
    static void (*set)(int);
    static int (*count)(int *);
    static int seen;
    if (!bound) {
        bound = true;
        HMODULE m = GetModuleHandleA(NULL);
        set = (void (*)(int))(void *)GetProcAddress(m, "linkpos_survive_set");
        count = (int (*)(int *))(void *)GetProcAddress(m, "linkpos_survive_count");
        LOG("[chestabil] Second Chance / Once More: %s\n", set ? "exe clamp found" : "not in this exe (no effect)");
        if (count) seen = count(NULL);
    }
    if (!set) return life;
    int mask = 0;
    if (g_chestfx && life >= 0) mask = (ab_on(AB.on, 415) ? 1 : 0) | (ab_on(AB.on, 416) ? 2 : 0);
    mask |= mk_survive_mask();  // (s109 b22-mickey: armed in a boss fight)
    set(mask);
    if (count) {
        int what = 0, n = count(&what);
        if (n != seen) {
            seen = n;
            if (what != 3) LOG("[chestabil] %s saved Sora (%d so far)\n", what == 2 ? "Once More" : "Second Chance", n);  // (s110: what 3 = the Mickey rescue clamp, logged by sora_mickey.h)
        }
    }
    return life;
}

static bool ca_live_set(const char *key, int v) {
    if (!strcmp(key, "chestabil")) g_chestabil = v != 0, LOG("[chestabil] live chestabil %d\n", g_chestabil);
    else if (!strcmp(key, "chestlog")) g_chestlog = v != 0, LOG("[chestabil] live chestlog %d\n", g_chestlog);
    else if (!strcmp(key, "chestfirst")) g_chestfirst = v != 0, AB.dirty = true, CN.next = 0, LOG("[chestabil] live chestfirst %d (1 = chest #1 Guard, #2 Second Chance, Dodge Roll with the paraglider)\n", g_chestfirst);
    else if (!strcmp(key, "chestfx")) g_chestfx = v != 0, LOG("[chestabil] live chestfx %d (Second Chance / Once More)\n", g_chestfx);
    else if (!strcmp(key, "chestfake")) g_chestfake = v < 0 ? -1 : v, CN.next = 0, LOG("[chestabil] live chestfake %d\n", g_chestfake);
    else if (!strcmp(key, "chestgive")) { if (v && !ca_award("chestgive", true)) LOG("[chestabil] chestgive: the pool is empty\n"); }
    else if (!strcmp(key, "chestreset")) {
        if (v) CA.mask = 0, AB.dirty = true, ab_tick(), sl_persist_save(), CN.next = 0, LOG("[chestabil] reset: no chest abilities (the next sweep gives back what the count earns)\n");
    } else return false;
    return true;
}

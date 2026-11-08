// sora_unlock.h: (s89 kb) BotW milestones -> Sora's Keyblades (KH2 gives them per world; here per BotW milestone) and
// the Drive Form gate. Plan: recon/S89_PLAN_KEYBLADES_FORMS.md item 1; notes: recon/S89_HELPER_KB.md.
// Included from sora.c right after sora_levels.h (needs SLX gdt getters, SLS / SL.clock, KB, sl_story, sl_persist_save).
//
// Milestones are read LIVE through the exe's gdt getters (sora_levels.h "gdt getters": linkpos_gdt_bool / _s32, game
// thread only = from sl_tick's frame, as the levels' own check), every UL_EVERY game seconds. Flag names: BotW 1.9.0
// Bootup.pack gamedata (data/levels/gamedata_flag_names.txt; S54_HELPER_LV_LEVELS.md 4.1). An old exe without the
// getters: the save file's counts (sl_story: paraglider, beasts, towers, shrines) at each slot change, no kills.
// Slot changes (load / autosave to another slot): the live flags may still be the OLD world's for a while, so nothing
// unlocks until they match that save's counts exactly (or UL_SYNC_MAX s pass) = no Keyblade from another playthrough.
//
// Owned set = KB.own (sora_levels.h, sidecar "kb_owned", env over the hot reload); a milestone only ever ADDS. The
// "Obtained <Keyblade>" popup = LOG only: the HUD has no KH2 item-get panel (the s85h #6 popup is BotW's own new-item
// screen 0xb, which the DLL can only wait for, not raise). Abilities of the new Keyblades stay menu-only (user, s89).
//
// Drive Forms: ul_form_unlocked(FM_FORMS[f].name), Divine Beasts cleared (user's s87c idea): Valor + Wisdom always,
// Limit >= 1, Master >= 2, Final >= 4, Anti = any Drive Form owned (s89 trial, user: KH2 gives Anti with Valor).
// Unknown names = unlocked (a form this table does not know is never hidden by it).
//
// Live: "set kball 1" every Keyblade usable (not saved), "set kbown 0" the owned-only filter off (picker lists all 24,
// equip allowed), "set beasts N" fake Divine Beast count for the form gate (-1 = the real one; not saved, Keyblades not
// given), "set ullog 1" log the milestone state now.
#pragma once

#define UL_EVERY 2.0f      // game s between milestone reads
#define UL_SYNC_MAX 60.0f  // game s after a slot change before the live flags are trusted anyway
enum { UL_ALWAYS, UL_GLIDER, UL_BEAST, UL_BEASTS, UL_TOWERS, UL_SHRINES, UL_KILLS, UL_BOOL, UL_S32 };
enum { UL_K_HINOX, UL_K_TALUS, UL_K_LYNEL, UL_K_MOLDUGA, UL_K_GUARDIAN, UL_NKILL };
static const char *const UL_BEAST_FLAG[4] = {"Clear_RemainsWater", "Clear_RemainsWind", "Clear_RemainsFire", "Clear_RemainsElectric"};
// BotW's Defeated_<actor>_Num counters (+1 per death up to 10, no attacker check: Sora's kills count too = GUESS until
// live, S54_HELPER_LV_LEVELS.md 3.2). Mini Guardians (scouts) and Pebblits left out.
static const char *const UL_KILL_FLAGS[UL_NKILL][6] = {
    {"Defeated_Enemy_Giant_Junior_Num", "Defeated_Enemy_Giant_Middle_Num", "Defeated_Enemy_Giant_Senior_Num", "Defeated_Enemy_Giant_Bone_Num"},
    {"Defeated_Enemy_Golem_Junior_Num", "Defeated_Enemy_Golem_Middle_Num", "Defeated_Enemy_Golem_Senior_Num", "Defeated_Enemy_Golem_Fire_Num",
     "Defeated_Enemy_Golem_Ice_Num", "Defeated_Enemy_Golem_Fire_R_Num"},
    {"Defeated_Enemy_Lynel_Junior_Num", "Defeated_Enemy_Lynel_Middle_Num", "Defeated_Enemy_Lynel_Senior_Num", "Defeated_Enemy_Lynel_Dark_Num",
     "Defeated_Enemy_Lynel_Gold_Num"},
    {"Defeated_Enemy_Sandworm_Num", "Defeated_Enemy_SandwormR_Num"},
    {"Defeated_Enemy_Guardian_A_Num", "Defeated_Enemy_Guardian_B_Num", "Defeated_Enemy_Guardian_C_Num", "Defeated_Enemy_Guardian_A_Fixed_Moss_Num"},
};

// The milestone -> Keyblade table (proposal, FYI for the user in recon/S89_HELPER_KB.md): KH2's rough acquisition
// order mapped onto BotW's progress; the four elemental Keyblades go to the matching Divine Beast.
typedef struct { uint16_t kb; uint8_t kind; int16_t n; const char *flag; const char *what; } UlRule;
static const UlRule UL_RULES[] = {
    {0x29, UL_ALWAYS, 0, NULL, "start"},                             // Kingdom Key
    {0x1e0, UL_ALWAYS, 0, NULL, "start"},                            // Star Seeker (KH2: Yen Sid's tower, before world 1)
    {0x1e1, UL_GLIDER, 0, NULL, "paraglider (Great Plateau done)"},  // Hidden Dragon (KH2 world 1, Land of Dragons)
    {0x1e4, UL_TOWERS, 3, NULL, "3 towers activated"},               // Hero's Crest
    {0x1e5, UL_SHRINES, 10, NULL, "10 shrines cleared"},             // Monochrome
    {0x1e7, UL_TOWERS, 6, NULL, "6 towers activated"},               // Circle of Life
    {0x1ec, UL_KILLS, UL_K_HINOX, NULL, "first Hinox defeated"},     // Wishing Lamp
    {0x1e6, UL_BEAST, 1, NULL, "Vah Medoh (wind)"},                  // Follow the Wind
    {0x1f0, UL_BEAST, 0, NULL, "Vah Ruta (water)"},                  // Mysterious Abyss (Blizzard Boost)
    {0x1f2, UL_BEAST, 2, NULL, "Vah Rudania (fire)"},                // Bond of Flame (Fire Boost)
    {0x1e8, UL_BEAST, 3, NULL, "Vah Naboris (lightning)"},           // Photon Debugger (Thunder Boost)
    {0x1e9, UL_SHRINES, 20, NULL, "20 shrines cleared"},             // Gull Wing
    {0x1ea, UL_KILLS, UL_K_TALUS, NULL, "first Stone Talus defeated"},  // Rumbling Rose
    {0x1eb, UL_KILLS, UL_K_GUARDIAN, NULL, "first Guardian destroyed"}, // Guardian Soul
    {0x1ed, UL_KILLS, UL_K_MOLDUGA, NULL, "first Molduga defeated"},    // Decisive Pumpkin
    {0x1ef, UL_S32, 30, "HiddenKorok_Number", "30 Korok seeds"},     // Sweet Memories (100 Acre Wood ~ the Koroks)
    {0x1ee, UL_TOWERS, 10, NULL, "10 towers activated"},             // Sleeping Lion
    {0x2a, UL_BEASTS, 2, NULL, "2 Divine Beasts freed"},             // Oathkeeper
    {0x2b, UL_BEASTS, 3, NULL, "3 Divine Beasts freed"},             // Oblivion
    {0x1f1, UL_SHRINES, 60, NULL, "60 shrines cleared"},             // Fatal Crest
    {0x1f3, UL_KILLS, UL_K_LYNEL, NULL, "first Lynel defeated"},     // Fenrir
    {0x21f, UL_BOOL, 1, "IsGet_Weapon_Sword_070", "the Master Sword"},  // Two Become One
    {0x1f4, UL_SHRINES, 100, NULL, "100 shrines cleared"},           // Ultima Weapon
    {0x220, UL_BOOL, 1, "GameClear", "Calamity Ganon defeated"},     // Winner's Proof
};
#define UL_NRULES ((int)(sizeof UL_RULES / sizeof UL_RULES[0]))
_Static_assert(sizeof UL_RULES / sizeof UL_RULES[0] == sizeof KB_TAB / sizeof KB_TAB[0], "one rule per Keyblade");

typedef struct { int glider, beast[4], beasts, towers, shrines, kills[UL_NKILL], korok, msword, ganon; } UlState;
static struct {
    int kball, filter, beasts_fake, slot_seen, synced, logq;
    float next, sync_by;
    SlStory want;     // the slot's save-file counts at the slot change (the sync check)
    UlState st;       // the last read
    bool have, src_file, popup_said;
    int forms_said;   // bit per UL_FORMS entry: logged as available
    int spells_said;  // (s133 magic) bit per UL_SPELL_RULES entry: logged as obtained
    int forms_base;   // (s89 trial) Limit / Master / Final owned by the real beasts since this slot synced
    bool base_ok;
} UL = {.filter = 1, .beasts_fake = -1, .slot_seen = -2};

static int g_ul_seed = 1;  // (s89 review) live "set ulseed 0" = the old rule (the last read kept over a slot change)

static bool ul_b(const char *nm) { int v = 0; return SLX.gdt_bool && SLX.gdt_bool(nm, &v) && v; }
static int ul_i(const char *nm) { int32_t v = 0; return SLX.gdt_s32 && SLX.gdt_s32(nm, &v) ? v : 0; }
// the live flags -> st; false = the getters are not there / refused (not in a game frame, no gdt manager yet)
static bool ul_read(UlState *st) {
    int probe = 0;
    memset(st, 0, sizeof *st);
    if (!SLX.gdt_bool || !SLX.gdt_s32 || !SLX.gdt_bool("IsGet_PlayerStole2", &probe)) return false;
    st->glider = probe != 0;
    for (int i = 0; i < 4; i++) st->beasts += st->beast[i] = ul_b(UL_BEAST_FLAG[i]);
    char nm[40];
    for (int i = 1; i <= 15; i++) snprintf(nm, sizeof nm, "MapTower_%02d", i), st->towers += ul_b(nm);
    for (int i = 0; i < 160; i++) snprintf(nm, sizeof nm, "Clear_Dungeon%03d", i), st->shrines += ul_b(nm);  // (as sl_story)
    for (int k = 0; k < UL_NKILL; k++)
        for (int j = 0; j < 6 && UL_KILL_FLAGS[k][j]; j++) st->kills[k] += ul_i(UL_KILL_FLAGS[k][j]);
    st->korok = ul_i("HiddenKorok_Number"), st->msword = ul_b("IsGet_Weapon_Sword_070"), st->ganon = ul_b("GameClear");
    return true;
}
static bool ul_met(const UlRule *r, const UlState *s) {
    switch (r->kind) {
    case UL_ALWAYS: return true;
    case UL_GLIDER: return s->glider;
    case UL_BEAST: return r->n >= 0 && r->n < 4 && s->beast[r->n];
    case UL_BEASTS: return s->beasts >= r->n;
    case UL_TOWERS: return s->towers >= r->n;
    case UL_SHRINES: return s->shrines >= r->n;
    case UL_KILLS: return r->n >= 0 && r->n < UL_NKILL && s->kills[r->n] >= 1;
    case UL_BOOL: return !strcmp(r->flag, "GameClear") ? s->ganon : s->msword;
    case UL_S32: return s->korok >= r->n;
    }
    return false;
}
static void ul_log_state(const char *why) {
    const UlState *s = &UL.st;
    LOG("[sora] unlock: milestones (%s%s): paraglider %d, beasts %d (Ruta %d Medoh %d Rudania %d Naboris %d), towers %d, "
        "shrines %d, koroks %d, kills Hinox %d Talus %d Lynel %d Molduga %d Guardian %d, Master Sword %d, Ganon %d; "
        "owned 0x%06x, beasts for forms %d%s%s\n", why, UL.src_file ? ", from the save file" : "", s->glider, s->beasts,
        s->beast[0], s->beast[1], s->beast[2], s->beast[3], s->towers, s->shrines, s->korok, s->kills[0], s->kills[1],
        s->kills[2], s->kills[3], s->kills[4], s->msword, s->ganon, KB.own, UL.beasts_fake >= 0 ? UL.beasts_fake : s->beasts,
        UL.beasts_fake >= 0 ? " (fake: set beasts)" : "", UL.kball ? ", kball ON" : !UL.filter ? ", owned filter OFF" : "");
}

// ---- the Drive Form gate
static int ul_beasts(void) { return UL.beasts_fake >= 0 ? UL.beasts_fake : UL.have ? UL.st.beasts : 0; }
// (s89 trial) the rule for b Divine Beasts; Anti = any Drive Form owned (KH2 gives Antiform with Valor at the Mysterious
// Tower, khwiki / S89_HELPER_ANTI.md: no later story gate) - Valor + Wisdom always are, so Anti is always allowed here
static const char *const UL_FORM_ORDER[] = {"Valor", "Wisdom", "Limit", "Master", "Final"};
static bool ul_form_unlocked_b(const char *form_name, int b) {
    if (!form_name) return true;
    if (!strcmp(form_name, "Limit")) return b >= 1;
    if (!strcmp(form_name, "Master")) return b >= 2;
    if (!strcmp(form_name, "Final")) return b >= 4;
    if (!strcmp(form_name, "Anti")) {
        for (int i = 0; i < 5; i++)
            if (ul_form_unlocked_b(UL_FORM_ORDER[i], b)) return true;
        return false;
    }
    return true;  // Valor, Wisdom, and any name not listed here
}
static bool ul_form_unlocked(const char *form_name) { return ul_form_unlocked_b(form_name, ul_beasts()); }
static const char *const UL_FORMS[] = {"Limit", "Master", "Final", "Anti"};
// (s133 magic) the spells that come with a BotW milestone (not learned by level): Magnet after the 2nd Divine Beast, Reflect after the 3rd
// (the same counts as Oathkeeper / Oblivion above). GUESS: the user may change these. The "kb" field = the SPELL_* id (sora.c: 4 / 5).
// Derived from ul_beasts() each time (it follows "set beasts" and debugall's fake 4), so nothing is saved: a milestone never un-happens in a save.
static const UlRule UL_SPELL_RULES[] = {
    {4, UL_BEASTS, 2, NULL, "2 Divine Beasts freed"},  // Magnet
    {5, UL_BEASTS, 3, NULL, "3 Divine Beasts freed"},  // Reflect
};
#define UL_NSPELL_RULES ((int)(sizeof UL_SPELL_RULES / sizeof UL_SPELL_RULES[0]))
static bool ul_spell_owned(int spell) {
    UlState s;
    memset(&s, 0, sizeof s);
    s.beasts = ul_beasts();
    for (int i = 0; i < UL_NSPELL_RULES; i++)
        if (UL_SPELL_RULES[i].kb == spell) return ul_met(&UL_SPELL_RULES[i], &s);
    return true;  // a spell with no rule is always owned
}
static void ul_spells_log(void) {
    static const char *const nm[2] = {"Magnet", "Reflect"};
    for (int i = 0; i < UL_NSPELL_RULES; i++) {
        bool on = ul_spell_owned(UL_SPELL_RULES[i].kb), was = (UL.spells_said >> i & 1) != 0;
        if (on == was) continue;
        UL.spells_said ^= 1 << i;
        LOG("[sora] unlock: spell %s %s (%s%s)\n", nm[i], on ? "OBTAINED" : "locked again", UL_SPELL_RULES[i].what, UL.beasts_fake >= 0 ? ", fake beasts" : "");
    }
}
// (s89 trial) KH2 (khwiki "Antiform"): learning a new Drive Form sets the Anti points to 0 = sora_anti.h
// anti_points_reset. Only a REAL new form (live beasts, not "set beasts"), only after this slot's flags synced (a load
// is not "learning"). Live "set ulantireset 0" = off.
static void anti_points_reset(const char *form);
static int g_ul_antireset = 1;
static void ul_new_forms(void) {
    int real = 0;
    for (int i = 0; i < 3; i++) real |= ul_form_unlocked_b(UL_FORMS[i], UL.st.beasts) << i;
    if (!UL.base_ok) { UL.forms_base = real, UL.base_ok = true; return; }
    int add = real & ~UL.forms_base;
    UL.forms_base |= real;
    for (int i = 0; i < 3; i++)
        if (add >> i & 1) {
            LOG("[sora] unlock: NEW Drive Form %s learned%s\n", UL_FORMS[i], g_ul_antireset ? ": Anti points reset (KH2)" : " (ulantireset 0: Anti points kept)");
            if (g_ul_antireset) anti_points_reset(UL_FORMS[i]);
        }
}
static void ul_forms_log(void) {
    for (int i = 0; i < 4; i++) {
        bool on = ul_form_unlocked(UL_FORMS[i]), was = (UL.forms_said >> i & 1) != 0;
        if (on == was) continue;
        UL.forms_said ^= 1 << i;
        LOG("[sora] unlock: Drive Form %s %s (Divine Beasts %d%s)\n", UL_FORMS[i], on ? "available" : "locked", ul_beasts(),
            UL.beasts_fake >= 0 ? ", fake" : "");
    }
}

// ---- the Keyblades
static uint32_t ul_all(void) { return KB_N >= 32 ? 0xffffffffu : (1u << KB_N) - 1; }
static uint32_t ul_kb_own_mask(void) { return UL.kball || !UL.filter ? ul_all() : kb_own_fix(KB.own, KB.kb); }  // (the camp)
static bool ul_kb_ok(int id) { return (ul_kb_own_mask() & kb_own_bit(id)) != 0; }
// (s92 b6-formfx) the "Keyblade obtained" panel. KH2's own item-get window ("OBTAINED" + the item, jm.bar 0x50d0 = the Jiminy puzzle
// reward; the chest / treasure panel's art is not in the HUD bakes) is not reusable: the s85h item popup is BotW's own screen 0xb. What the
// HUD has is KH2's level-up panel (sora_hud_levelup, lvup.bin) with its "<icon><name>!" ability line (sys.bar 0xae8, the line KH2 shows
// for a learned ability), so the new Keyblade goes there: lvup.bin s92 label ab<item id> (24 Keyblades baked, tools_s92/formfx),
// at Sora's current LV, name "Sora" (GUESS: the red "LEVEL UP!" title is the panel's art). SE = KH2's Get Bonus / level-up element
// SE (sfx "bonus_up", se000 #25; GUESS that this panel carries it). Only with a lvup.bin that has the label (else the panel would show
// the level alone): the file is read once per Keyblade. Live "set kbpop 0" = the log line only (s89).
static bool ul_kb_label(int id) {
    const char *p = getenv("SAVAGEWIND_SORA_LVUP");
    if (!p || !*p || !strcmp(p, "1")) p = "D:/SoraBotW/data/hud/lvup.bin";
    p = sora_path(p);
    if (!strcmp(p, "0")) return false;
    FILE *f = fopen(p, "rb");
    if (!f) return false;
    uint32_t h[8];  // SKLV header {magic, ver, n_grp, n_frames, w, h, n_label, as}
    bool ok = fread(h, 4, 8, f) == 8 && !memcmp(h, "SKLV", 4) && h[6] <= 4096;
    for (uint32_t i = 0; ok && i < h[6]; i++) {
        uint8_t rec[24];  // VLabel: x y w h u16, ox oy adv f32, key u32 @20
        if (fseek(f, (long)(32 + 16ull * h[2] + 20ull * h[3] + 24ull * i), SEEK_SET) || fread(rec, 1, 24, f) != 24) { ok = false; break; }
        uint32_t key;
        memcpy(&key, rec + 20, 4);
        if (key == SORA_LVUP_ABILITY((uint32_t)id)) { fclose(f); return true; }
    }
    fclose(f);
    return false;
}
static void ul_kb_panel(int id) {
    static void (*fn)(const SoraHudLevelUp *);
    static int tried;
    if (!g_kbpop) return;
    if (!tried) {
        tried = 1;
        fn = (void (*)(const SoraHudLevelUp *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_levelup");
    }
    if (!fn) { LOG("[sora] unlock: Keyblade panel: sora_hud_levelup not in this exe\n"); return; }
    if (!ul_kb_label(id)) { LOG("[sora] unlock: Keyblade panel: no label ab%d in lvup.bin (install lvup_s92_kb.bin + restart): log line only\n", id); return; }
    SoraHudLevelUp u = {.level = SL.lv < 1 ? 1 : SL.lv > 99 ? 99 : SL.lv, .n_lines = 1, .line = {SORA_LVUP_ABILITY((uint32_t)id)}, .name = SORA_LVUP_NAME_SORA};
    fn(&u);
    int h = sora_sfx_event("bonus_up", 1.0f);
    LOG("[sora] unlock: Keyblade panel for %s (item %d), SE %d\n", kb_name(id), id, h);
}
static void ul_apply(const char *why) {
    uint32_t add = 0;
    for (int i = 0; i < UL_NRULES; i++) {
        const UlRule *r = &UL_RULES[i];
        uint32_t bit = kb_own_bit(r->kb);
        if (!bit || (KB.own & bit) || !ul_met(r, &UL.st)) continue;
        add |= bit;
        LOG("[sora] unlock: Obtained %s! (%s; %s)\n", kb_name(r->kb), r->what, why);
        ul_kb_panel(r->kb);  // (s92 b6-formfx)
    }
    if (!add) return;
    KB.own |= add;
    if (!UL.popup_said) UL.popup_said = true, LOG("[sora] unlock: Obtained panel = the level-up panel's ability line (set kbpop 0 = log only)\n");
    sl_persist_save();  // (env now; the sidecar at BotW's next save, as every levels value)
}

// per frame, after sl_tick (the gdt getters work only in the game's frame)
static void ul_tick(void) {
    if (UL.logq) UL.logq = 0, ul_log_state("set ullog");
    if (SL.clock < UL.next) return;
    UL.next = SL.clock + UL_EVERY;
    if (SLS.slot != UL.slot_seen) {  // a load / a save to another slot: wait for the live flags to be that save's
        UL.slot_seen = SLS.slot, UL.synced = 0, UL.sync_by = SL.clock + UL_SYNC_MAX, UL.base_ok = false;
        UL.want = SLS.slot >= 0 ? sl_story(SLS.slot) : (SlStory){0};
        // (s89 review) until the live flags sync, the form gate counts THIS save's Divine Beasts (its file), not the
        // last world's (UL.st kept them: a load from a 4-beast save into a 0-beast one left Final usable for up to 60 s)
        if (g_ul_seed) {
            if (UL.want.ok) UL.st.beasts = UL.want.beasts, UL.have = true;
            else UL.have = false;
            LOG("[sora] unlock: slot %d: form gate from its save file until the live flags sync (beasts %d)\n", SLS.slot,
                UL.have ? UL.st.beasts : 0);
        }
    }
    ul_forms_log();
    ul_spells_log();  // (s133 magic)
    if (SLS.slot < 0) return;  // no save of this world yet (title / first boot before the first save event)
    UlState st;
    bool live = ul_read(&st);
    if (!live) {  // old exe / refused: the save file's counts (no kills, no single beasts, no Master Sword)
        if (SLX.gdt_bool || !UL.want.ok || UL.synced) return;
        memset(&st, 0, sizeof st);
        st.glider = UL.want.glider, st.beasts = UL.want.beasts, st.towers = UL.want.towers, st.shrines = UL.want.shrines;
    }
    if (!UL.synced) {
        bool same = !UL.want.ok || (st.glider == UL.want.glider && st.beasts == UL.want.beasts && st.towers == UL.want.towers &&
                                    st.shrines == UL.want.shrines);
        if (!same && SL.clock < UL.sync_by) return;
        UL.synced = 1;
        LOG("[sora] unlock: slot %d %s\n", SLS.slot, !live ? "(no gdt getters: the save file's counts)" : same ? "in sync with its save"
                                                                                                          : "not in sync after 60 s: trusting the live flags");
    }
    bool changed = !UL.have || memcmp(&st, &UL.st, sizeof st);
    UL.st = st, UL.have = true, UL.src_file = !live;
    if (changed) ul_log_state(live ? "live" : "save"), ul_forms_log();
    ul_new_forms();  // (s89 trial: Anti points)
    ul_apply(live ? "live flags" : "save file");
}

// live "set <key> <v>" keys of this module -> true = handled
static bool ul_set_key(const char *key, int v) {
    if (!strcmp(key, "kball")) UL.kball = v != 0;
    else if (!strcmp(key, "kbown")) UL.filter = v != 0;
    else if (!strcmp(key, "beasts")) UL.beasts_fake = v < 0 ? -1 : v;
    else if (!strcmp(key, "ullog")) UL.logq = v != 0;
    else if (!strcmp(key, "kbpoptest")) { ul_kb_panel(v); return true; }  // (s92) show the panel for a Keyblade item id, e.g. 43 = Oblivion (not saved, not given)
    else if (!strcmp(key, "ulseed")) g_ul_seed = v != 0;            // (s89 review)
    else if (!strcmp(key, "ulnoside")) g_ul_noside_reset = v != 0;  // (s89 review, sora_levels.h)
    else if (!strcmp(key, "ulantireset")) g_ul_antireset = v != 0;  // (s89 trial)
    else return false;
    LOG("[sora] unlock: live %s = %d (kball %d, owned filter %d, beasts for forms %d%s)\n", key, v, UL.kball, UL.filter,
        ul_beasts(), UL.beasts_fake >= 0 ? " fake" : "");
    ul_forms_log();
    ul_spells_log();  // (s133 magic: "set beasts" moves the spell gate too)
    return true;
}

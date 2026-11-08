// (s89 anti) KH2 FM Anti Form for Sora in BotW (DLL, hot reload): the Anti points, the random Anti roll on a Drive,
// and Anti's own rules. Notes + sources: D:/SoraBotW/recon/S89_HELPER_ANTI.md. Included right after sora_form.h.
//
// The roll = KH2's form_choice (re:0x421D60, as Project Heart Hunters ported it - read for the rule, no code taken), with
// KH2 FM's numbers from the game files:
//   - 03system.bin pref/sstm (OpenKH Sstm): AntiFormDriveCount 5 (the form gauge = 5 - 2 + 3 = 6 bars, re:0x3C17C0),
//     AntiFormSubCount 6 (points taken by an Anti), AntiFormDamageRate 1.5, FinalFormSubCount 10 (taken by a Final pick).
//   - 00battle.bin fmlv, form 6 rows (key 0x61..0x63: chance %, points): 25 % from 10 points, 10 % from 5, else 0 %.
//   - Each Drive into another form (not Final): the roll uses the points BEFORE this Drive, then points + 1; a hit in
//     battle = Anti (points - 6, floor 0). A Final pick = points - 10 (floor 0), no roll. The party check comes first: a
//     Drive without both Donald and Goofy able = no roll and no point (KH2 needs both for Anti).
//   - khwiki "Antiform": +1 per Valor / Wisdom / Limit / Master, -10 per Final, learning a new form resets to 0
//     (anti_points_reset: b4-kb's unlock should call it), the wiki says -4 per Anti (the exe + data say -6 after the
//     +1; live "set antisub 4" if the user prefers the wiki); x2 / x4 / x0 area multipliers (scripted / Org XIII / some
//     bosses) have no BotW match (GUESS: x1 everywhere).
//   - When: KH2 gives Antiform with Valor at the Mysterious Tower (khwiki), i.e. from the first Drive Form on (0 % until
//     5 points). In the mod Valor + Wisdom are always there -> anti_allowed() = true.
// Anti's rules: no Revert in battle (KH2 greys it, re:0x3F36C0; outside battle it is allowed, khwiki), no healing
// (khwiki: no HP orbs = none at all: BotW food / fairies here, undone by the life watch), damage taken x1.5
// (AntiFormDamageRate), no EXP (plrp 0x86 equips 404 No Experience; khwiki), no Magic / Items (cmd DisableForm bit 6),
// absorbs both (FM_ABSORB_BOTH), 6 bars, never a Drive page row.
// Live: set fmanti 0 (no Anti, no roll, no points) | antipts N | antiforce 1 (the next Drive -> Anti; also the live
// "form N") | antisub N | antidmg <pct> (150; 100 = off) | antiheal 1 (allow heals) | antiexp 1 (allow EXP) |
// antirevert 0 never / 1 KH2 (outside battle) / 2 always | rehit 0 (sora.c: one hit per window, no Interval re-hits).
#pragma once

// (s89 trial) KH2: Anti comes with the first form (Valor, Mysterious Tower) = sora_unlock.h's "Anti" rule: any Drive
// Form owned (always true while Valor / Wisdom are always owned).
static bool anti_allowed(void) { return ul_form_unlocked("Anti"); }

#define ANTI_ENV "SORA_KH2_ANTI_STATE"
static const struct { int pct, pts; } ANTI_CHANCE[] = {{25, 10}, {10, 5}, {0, 0}};  // 00battle fmlv 0x61..0x63
static int g_fmanti = 1, g_antiforce, g_anti_sub = 6, g_anti_final_sub = 10, g_anti_drive_count = 5;
static int g_anti_dmg_pct = 150, g_anti_heal, g_anti_exp, g_anti_revert = 1;
static struct {
    bool init;
    int pts;
    uint32_t rng;
    int cap;      // the life Sora may have in Anti (-1 = read it next tick)
    float frac;   // the x1.5's fraction of a quarter heart, carried
    int blocked;  // heals undone this form (log)
} AN = {.cap = -1};

static void anti_persist(void) {
    char b[48];
    snprintf(b, sizeof b, "v1 pts=%d", AN.pts);
    SetEnvironmentVariableA(ANTI_ENV, b);
}
static void anti_init(void) {
    if (AN.init) return;
    AN.init = true;
    AN.rng = GetTickCount() * 2654435761u | 1;
    char b[48];
    DWORD n = GetEnvironmentVariableA(ANTI_ENV, b, sizeof b);
    int p = 0;
    if (n && n < sizeof b && sscanf(b, "v1 pts=%d", &p) == 1) AN.pts = p < 0 ? 0 : p, LOG("[anti] hot reload: %d anti points\n", AN.pts);
}
static uint32_t anti_pts_get(void) { anti_init(); return (uint32_t)AN.pts; }  // (the levels sidecar's "anti")
static void anti_load(int pts, const char *why) {  // (sl_slot_load: -1 = none in the sidecar = 0)
    anti_init();
    AN.pts = pts < 0 ? 0 : pts;
    LOG("[anti] %s: %d anti points\n", why, AN.pts);
    anti_persist();
}
static void anti_set_pts(int v, const char *why) {
    anti_init();
    int old = AN.pts;
    AN.pts = v < 0 ? 0 : v;
    LOG("[anti] points %d -> %d (%s)\n", old, AN.pts, why);
    anti_persist();
}
// khwiki: learning a new Drive Form resets the counter (b4-kb's unlock: call this when a form becomes owned)
static void anti_points_reset(const char *form) {
    char why[64];
    snprintf(why, sizeof why, "new form %s learned", form);
    anti_set_pts(0, why);
}
static bool anti_battle(void) { return P.on && P.combat && P.battle; }
// KH2's party check for Anti (re:0x4005E0, both slots): Donald and Goofy in the party, loaded, standing, not absorbed
static bool anti_party_ok(void) {
    if (!P.on || !P.ticks) return false;
    for (int mb = 0; mb < 2; mb++)
        if (g_party_id[mb] <= 0 || P.m[mb].hp <= 0 || g_party_absorbed[mb]) return false;
    return true;
}
static int anti_rand100(void) {
    anti_init();
    AN.rng = AN.rng * 1664525u + 1013904223u;
    return (int)((AN.rng >> 8) % 100);
}

// fm_drive: the form a Drive gives (re:0x421D60). free_cost (the live "form N") = only antiforce applies.
static int anti_choice(int f, bool free_cost) {
    anti_init();
    if (f == FORM_ANTI || !g_fmanti) return f;
    if (g_antiforce) {
        g_antiforce = 0;
        if (!fm_ready(FORM_ANTI)) {
            LOG("[anti] antiforce: Anti's draw actor / bake is not loaded (actor %d, %d motion slots) - %s instead\n",
                FM.actor[FORM_ANTI], g_nfmots_tab[FORM_ANTI], FM_FORMS[f].name);
            return f;
        }
        anti_set_pts(AN.pts + 1 - g_anti_sub, "antiforce: Anti (+1 - AntiFormSubCount)");
        LOG("[anti] ROLL forced (antiforce 1): %s -> Anti\n", FM_FORMS[f].name);
        return FORM_ANTI;
    }
    if (free_cost) return f;
    if (!strcmp(FM_FORMS[f].name, "Final")) {  // (by name: FORM_FINAL is another branch's enum)
        anti_set_pts(AN.pts - g_anti_final_sub, "Final Form picked (FinalFormSubCount)");
        return f;
    }
    if (!anti_allowed()) { LOG("[anti] no roll: Anti not unlocked yet (%d points)\n", AN.pts); return f; }
    if (!anti_party_ok()) {
        LOG("[anti] no roll, no point: KH2 needs Donald and Goofy both able (party on %d, ids %d %d) - %d points\n", P.on,
            g_party_id[0], g_party_id[1], AN.pts);
        return f;
    }
    int i = 0;
    while (ANTI_CHANCE[i].pts && AN.pts < ANTI_CHANCE[i].pts) i++;
    int chance = ANTI_CHANCE[i].pct, r = anti_rand100();
    bool battle = anti_battle(), hit = r < chance && battle, ready = fm_ready(FORM_ANTI);
    int before = AN.pts;
    AN.pts++;
    LOG("[anti] ROLL on %s: %d points -> chance %d%%, roll %d, battle %d -> %s (points now %d)\n", FM_FORMS[f].name, before,
        chance, r, battle, hit ? (ready ? "ANTI" : "Anti, but its actor is not loaded: no") : "no", AN.pts);
    if (hit && ready) {
        anti_set_pts(AN.pts - g_anti_sub, "Anti (AntiFormSubCount)");
        return FORM_ANTI;
    }
    anti_persist();
    return f;
}

static float anti_gauge_bars(void) { return (float)(g_anti_drive_count - 2 + 3); }  // (re:0x3C17C0: n = 5 - 2; 6 bars)

static bool anti_revert_ok(void) { return g_anti_revert == 2 || (g_anti_revert == 1 && !anti_battle()); }

static void anti_on_enter(void) {
    AN.cap = -1, AN.frac = 0, AN.blocked = 0;
    LOG("[anti] in Anti Form: %.0f bars, no heal %d, damage taken %d%%, no EXP %d, Revert %s\n", anti_gauge_bars(),
        !g_anti_heal, g_anti_dmg_pct, !g_anti_exp, g_anti_revert == 2 ? "always" : g_anti_revert ? "outside battle" : "never");
}

// every form frame (fm_tick): in Anti a life rise (food, fairy, Cure) is undone and a life drop gets its x1.5. Life =
// Link's actor +0x848 (the field link_heal writes), quarter hearts. The extra damage never takes the last quarter
// (GUESS: a death by the write itself might skip BotW's own death path).
static void anti_life_tick(void) {
    if (!IN_ANTI || FM.st != FM_FORM) { AN.cap = -1; return; }
    int cur = linkpos_link_life();
    if (cur <= 0) return;
    if (AN.cap < 0) { AN.cap = cur; return; }
    uint64_t a = link_actor();
    bool can = a && rd_ok(a + 0x848, 4) && *(const int32_t *)(uintptr_t)(a + 0x848) == cur;
    if (cur > AN.cap) {
        if (!g_anti_heal && can) {
            *(int32_t *)(uintptr_t)(a + 0x848) = AN.cap;
            if (AN.blocked++ < 20) LOG("[anti] heal undone: life %d -> %d (Anti can't heal)\n", cur, AN.cap);
            return;
        }
        AN.cap = cur;
    } else if (cur < AN.cap) {
        int d = AN.cap - cur, v = cur;
        if (g_anti_dmg_pct != 100 && can) {
            AN.frac += d * (g_anti_dmg_pct - 100) / 100.0f;
            int extra = (int)AN.frac;
            AN.frac -= (float)extra;
            v = cur - extra < 1 ? (cur < 1 ? cur : 1) : cur - extra;
            if (v != cur) *(int32_t *)(uintptr_t)(a + 0x848) = v;
            LOG("[anti] damage taken %d quarter hearts x%.2f: life %d -> %d\n", d, g_anti_dmg_pct / 100.0f, AN.cap, v);
        }
        AN.cap = v;
    }
}

// sora_levels.h sl_kill: KH2's No Experience (plrp 0x86 item 404) while in Anti
static bool anti_no_exp(void) { return IN_ANTI && !g_anti_exp; }

static bool anti_live_set(const char *key, int v) {
    if (!strcmp(key, "fmanti")) g_fmanti = v != 0, LOG("[anti] Anti Form %s\n", g_fmanti ? "ON (KH2's roll)" : "OFF (no roll, no points)");
    else if (!strcmp(key, "antipts")) anti_set_pts(v, "set antipts");
    else if (!strcmp(key, "antiforce")) g_antiforce = v != 0, LOG("[anti] antiforce %d: the next Drive %s\n", g_antiforce, g_antiforce ? "turns into Anti" : "rolls as KH2");
    else if (!strcmp(key, "antisub")) g_anti_sub = v < 0 ? 0 : v, LOG("[anti] points taken by an Anti: %d\n", g_anti_sub);
    else if (!strcmp(key, "antidmg")) g_anti_dmg_pct = v < 100 ? 100 : v > 400 ? 400 : v, AN.frac = 0;
    else if (!strcmp(key, "antiheal")) g_anti_heal = v != 0;
    else if (!strcmp(key, "antiexp")) g_anti_exp = v != 0;
    else if (!strcmp(key, "antirevert")) g_anti_revert = v < 0 ? 0 : v > 2 ? 2 : v;
    else if (!strcmp(key, "rehit")) g_rehit = v != 0, LOG("[anti] Interval re-hits %s\n", g_rehit ? "on" : "off");
    else return false;
    return true;
}

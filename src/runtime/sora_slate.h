// (s93 b7-runes) The Sheikah Slate: Link's runes in Sora's KH2 command menu (plan docs/plans/2026-11-01-sheikah-slate-runes.md,
// recon/S93_HELPER_RUNES.md). Included by sora.c after sora_limit.h. Everything is off by default: live "set slate 1" turns the row on.
//
// Slate row: the top page's slot SK_TOP_SLOT (the greyed "Items" row - a 5th row needs sora_hud.c's n = 4 / ICON[] and a baked
// "Slate" label = NEEDS EXE RELINK + NEEDS BAKE; SK_TOP_SLOT 2 / the 3 placeholders below are the DLL-only fallback). A / Right
// opens the Slate page (M.sub SK_SUB = 5, drawn as the Drive page's look: no title), one row per rune (s99: Remote Bomb, Remote Bomb Cube, Stasis, Cryonis, Magnesis, Camera).
// A on a rune = Slate mode: Link gets the pad (st = LINK_OTHER like the swim latch, so the existing puppet draws Sora in
// Link's pose), pad_tap keeps L + the d-pad for BotW, and one L tap (2 presents) uses BotW's SELECTED rune.
// s99 R2 SOLVED statically (recon/S99_HELPER_RUNES.md): the selected rune is the gdt s32 PlayerEquipItemType (ids 0 round bomb, 1 cube bomb,
// 2 Magnesis, 3 Stasis, 4 Cryonis, 5 Camera) and the wheel object's requested id; live "set slaterune 1" (needs the relinked exe) writes it
// before the L tap. With slaterune 0 (default) the tap uses whatever rune BotW's own wheel (d-pad up) has selected, as before.
// Slate mode ends: Link's state word back to ground for SK_STAND presents after the hold time, "set slateend 1", the timeout
// ("set slatemax", presents), a death / a load. Live keys: slate, slateforce, slatepulse, slatehold, slatemax, slatestay,
// slateend (Slate mode); slatepass (recon: L + d-pad reach BotW), slaterec (recon logs).
#pragma once

#define SK_N 6
#define SK_TOP_SLOT 2        // the top page slot the row sits on (2 = Items' greyed row; the 5th row would be 4)
#define SK_PLACEHOLDER_TOP 2 // cmd.bin label for the top slot: GUESS no "Slate" label baked -> "Items" (NEEDS BAKE for the real one)
#define SK_STAND 45          // presents Link must stand again before Slate mode ends

typedef struct { const char *name, *flag; int label, id; } SkRune;
// VERIFIED: the flag names are in the 1.9.0 gamedata (data/levels/gamedata_flag_names.txt) and in the exe's flag name table.
// label = cmd.bin label id for the row: 6 "Thunder" is a placeholder (no rune labels baked; NEEDS BAKE).
// id = BotW's rune id (VERIFIED static, recon/S99_HELPER_RUNES.md: the wheel's id order 0 round bomb, 1 cube bomb, 2 Magnesis, 3 Stasis,
// 4 Cryonis, 5 Camera). flag = the unlock flag BotW's own rune table checks: BOTH bombs use IsGet_Obj_RemoteBomb (IsGet_Obj_RemoteBombCube
// is not read by that table).
static const SkRune SK_RUNES[SK_N] = {
    {"Remote Bomb", "IsGet_Obj_RemoteBomb", 6, 0},
    {"Remote Bomb Cube", "IsGet_Obj_RemoteBomb", 6, 1},
    {"Stasis", "IsGet_Obj_StopTimer", 6, 3},
    {"Cryonis", "IsGet_Obj_IceMaker", 6, 4},
    {"Magnesis", "IsGet_Obj_Magnetglove", 6, 2},
    {"Camera", "IsGet_Obj_Camera", 6, 5},
};
// R1 recon: every rune-related unlock flag name found in the exe (VERIFIED strings) - the first five are the plan's.
static const char *const SK_R1_FLAGS[] = {
    "IsGet_Obj_Magnetglove", "IsGet_Obj_RemoteBomb", "IsGet_Obj_RemoteBombCube", "IsGet_Obj_StopTimer", "IsGet_Obj_IceMaker",
    "IsGet_Obj_Camera", "IsGet_Obj_RemoteBombLv2", "IsGet_Obj_StopTimerLv2", "IsGet_Obj_RemoteBombBall",
    "IsGet_Obj_RemoteBombBall2", "IsGet_Obj_RemoteBombCube2", "IsGet_PlayerStole2", "IsGet_PlayerStole2_Vagrant",
    "Guide_ItemMagnetglove", "IsHelp_App_Magnetglove", "IsHelp_App_RemoteBomb", "IsHelp_App_Stoptimer", "IsHelp_App_IceMaker",
};

static int g_sk = 0;          // live "set slate 1": the Slate row on the Items slot
static int g_sk_force = 0;    // live "set slateforce 1": the row is usable even if the unlock flag reads 0 / unknown
static int g_sk_pulse = 1;    // live "set slatepulse 0": no automatic L tap (press L yourself)
static int g_sk_hold = 90;    // live "set slatehold <presents>": Slate mode lasts at least this long
static int g_sk_max = 3600;   // live "set slatemax <presents>": ... and at most this long
static int g_sk_stay = 0;     // live "set slatestay 1": no natural end (only slateend / the timeout)
// (s100) live "set slateb 1" (default): holding a rune does NOT change Link's state word (s100 log: every rune ended "Link stands
// again" after 90 presents while the user still held it), so Slate mode now ends only when you press B (BotW's own rune cancel,
// PS5 Cross; the press also reaches Link, so BotW puts the rune away), the timeout, a death or a load. "set slateb 0" = old end rule.
static int g_sk_bend = 1;
static int g_sk_rec = 0;      // live "set slaterec <n>": 1 = R1 flags (now + each load), 2 = + state word log, 3 = + the actor diff probe (R2)
static struct { bool on; uint64_t t0; uint32_t sw0, last_sw; int stand, rune; bool moved; bool wait; int want; } SK;
// (s99 b14-runes) live "set slaterune 1": Slate mode first chooses the row's rune in BotW (exe export linkpos_rune_select, NEEDS EXE RELINK),
// then taps L once the game has applied it. 0 (default) = as before: BotW's own selected rune is used.
static int g_sk_rune = 0;
static struct { bool tried; int (*sel)(int); int (*get)(int *, int *); } SKR;
static void sk_rune_bind(void) {
    if (SKR.tried) return;
    SKR.tried = true;
    HMODULE exe = GetModuleHandleA(NULL);
    SKR.sel = (int (*)(int))(void *)GetProcAddress(exe, "linkpos_rune_select");
    SKR.get = (int (*)(int *, int *))(void *)GetProcAddress(exe, "linkpos_rune_get");
    LOG("[slate] exe exports linkpos_rune_select / linkpos_rune_get: %s / %s\n", SKR.sel ? "yes" : "NO (relink the exe: slaterune is ignored)", SKR.get ? "yes" : "NO");
}

static bool sk_flag(const char *name, int *val) {  // 1 = flag known (*val = its value)
    int v = 0;
    if (!SLX.gdt_bool || !SLX.gdt_bool(name, &v)) return false;
    *val = v;
    return true;
}
static bool sk_unlocked(int i) {
    int v = 0;
    if (i < 0 || i >= SK_N) return false;
    return g_sk_force || (sk_flag(SK_RUNES[i].flag, &v) && v);
}
static bool sk_any_unlocked(void) {
    for (int i = 0; i < SK_N; i++) if (sk_unlocked(i)) return true;
    return false;
}
static int sk_first_unlocked(void) {  // the page opens on the first usable rune
    for (int i = 0; i < SK_N; i++) if (sk_unlocked(i)) return i;
    return 0;
}
static int sk_top_state(void) { return g_sk && sk_any_unlocked() && !SK.on && !fm_in_form() ? 0 : 5; }

// (s98 slateleft) live "set slateleft 0": d-pad Left on the top page no longer opens the Slate page (1 = it does, whatever "meals")
static int g_sk_left = 1;
static bool sk_via_left = false;  // the page was opened by d-pad Left: it works with "meals" on, and Back returns to sk_ret
static int sk_ret = 0;            // the top cursor row Left was pressed on

// menu_tick, top page: A / Right on the Slate slot
static bool sk_open(void) {
    if (!g_sk || meal_owns_items() || M.cursor != SK_TOP_SLOT) return false;  // (s93 review: "meals 1" owns the Items slot)
    if (sk_top_state() != 0) return false;  // (greyed: the caller's error path)
    sk_via_left = false;
    M.sub = SK_SUB, M.sub_cursor = sk_first_unlocked(), sora_sfx_event("submenu", 1), LOG("[slate] menu: Slate page open\n");
    return true;
}
// menu_tick, top page: d-pad Left (unused on the top page in KH2 / here before) opens the Slate page from any row, "meals" on or off.
// Same gating as sk_open (slate on, rune unlocked, not in a form / Slate mode); otherwise it does nothing (no SE).
static bool sk_open_left(void) {
    if (!g_sk_left || !g_sk || sk_top_state() != 0) return false;
    sk_via_left = true, sk_ret = M.cursor;
    M.sub = SK_SUB, M.sub_cursor = sk_first_unlocked(), sora_sfx_event("submenu", 1), LOG("[slate] menu: Slate page open (d-pad Left, row %d)\n", sk_ret);
    return true;
}
// menu_tick, page 4: one row per rune (Remote Bomb, Remote Bomb Cube, Stasis, Cryonis, Magnesis, Camera)
static int sk_menu_tick(uint64_t *press) {
    if (!g_sk || (meal_owns_items() && !sk_via_left)) { M.sub = -1, M.cursor = sk_via_left ? sk_ret : SK_TOP_SLOT; sk_via_left = false; return -1; }
    if (M.sub_cursor < 0 || M.sub_cursor >= SK_N) M.sub_cursor = 0;
    if (*press & BTN_UP) M.sub_cursor = (M.sub_cursor + SK_N - 1) % SK_N, sora_sfx_event("cursor", 1);
    if (*press & BTN_DOWN) M.sub_cursor = (M.sub_cursor + 1) % SK_N, sora_sfx_event("cursor", 1);
    if (*press & (BTN_JUMP | BTN_LEFT)) {
        *press &= ~BTN_JUMP;
        M.sub = -1, M.cursor = sk_via_left ? sk_ret : SK_TOP_SLOT, sk_via_left = false;
        sora_sfx_event("cancel", 1), LOG("[slate] menu: Slate page closed\n");
    } else if (*press & BTN_ATTACK) {
        *press &= ~BTN_ATTACK;
        int r = M.sub_cursor;
        if (M.dead > 0 || !sk_unlocked(r)) { sora_sfx_event("error", 1); LOG("[slate] menu: %s greyed\n", SK_RUNES[r].name); return -1; }
        return CMD_SLATE + r;
    }
    return -1;
}

// hud_update: the top slot's state + label, the page's rows. Slot 2's label goes back to "Items" when the key is off.
static void sk_hud(SoraHudMenu *m) {
    static void (*top)(int, int);
    static int tried, was_on;
    if (!tried) tried = 1, top = (void (*)(int, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_top");
    if (g_sk != was_on && top) top(SK_TOP_SLOT, g_sk ? SK_PLACEHOLDER_TOP : SK_TOP_SLOT), was_on = g_sk;
    bool page_left = M.sub == SK_SUB && sk_via_left;  // (s98) opened by d-pad Left: drawn even with "meals" on
    if (!g_sk || (meal_owns_items() && !page_left)) return;  // (s93 review: meals on = the slot is the meals page's)
    if (!meal_owns_items()) m->top_state[SK_TOP_SLOT] = (uint8_t)sk_top_state();
    if (M.sub == SK_SUB) {
        m->sub = 2, m->n_sub = SK_N;
        for (int i = 0; i < SK_N; i++) m->sub_label[i] = (uint8_t)SK_RUNES[i].label, m->sub_state[i] = sk_unlocked(i) ? 0 : 5, m->sub_cost[i] = 0;
        // (s98) the rune names as text rows (user: the row said "Thunder" = the cmd.bin placeholder label)
        const char *rows[SK_N];
        for (int i = 0; i < SK_N; i++) rows[i] = SK_RUNES[i].name;
        sub_text_rows(m, rows, SK_N, M.sub_cursor);
    }
}

static void sk_end(const char *why, uint64_t present) {
    LOG("[slate] Slate mode off (%s) after %llu presents, state %#x\n", why, (unsigned long long)(present - SK.t0), linkpos_link_state_word());
    SK.on = false, SK.wait = false, g_slate_mode = 0, g_slate_l_until = 0;
    S.fresh = true;  // (the press that ended it is not an attack)
}

// the command: enter Slate mode (Sora idle on the ground, not in a Drive Form / Limit)
static void sk_request(int i, int st, uint64_t present) {
    if (i < 0 || i >= SK_N || SK.on) return;
    bool ok = st == LINK_GROUND && S.act == ACT_IDLE && !S.special && !fm_in_form() && !g_loading;
    if (!ok) { sora_sfx_event("error", 1); LOG("[slate] %s refused: Sora is busy (state %d, act %d, special %d)\n", SK_RUNES[i].name, st, S.act, S.special); return; }
    M.sub = -1, M.cursor = sk_via_left ? sk_ret : SK_TOP_SLOT, sk_via_left = false;
    memset(&SK, 0, sizeof SK);
    SK.on = true, SK.t0 = present, SK.rune = i, SK.sw0 = SK.last_sw = linkpos_link_state_word();
    g_slate_mode = 1, g_slate_seen = present;
    // L pulse 3 presents later (Link has the pad from the next tick on), held 2 presents
    g_slate_l_from = present + 3, g_slate_l_until = g_sk_pulse ? present + 5 : 0;
    if (g_sk_rune) {  // (s99) choose the rune in BotW first; the L tap waits until the game has applied it (sk_tick)
        sk_rune_bind();
        int oldv = -99, rq = -99, cu = -99, rc = -9;
        if (SLX.gdt_s32) SLX.gdt_s32("PlayerEquipItemType", &oldv);
        if (SKR.get) SKR.get(&rq, &cu);
        if (SKR.sel) rc = SKR.sel(SK_RUNES[i].id);
        LOG("[slate] slaterune: %s id %d: PlayerEquipItemType was %d, wheel req/cur %d/%d -> request rc %d%s\n", SK_RUNES[i].name, SK_RUNES[i].id, oldv, rq, cu, rc,
            rc == 1 ? "" : " (NOT written: BotW's selected rune will be used)");
        if (rc == 1 && g_sk_pulse) SK.wait = true, SK.want = SK_RUNES[i].id, g_slate_l_from = ~0ull, g_slate_l_until = 0;
    }
    sora_sfx_event("submenu", 1);
    LOG("[slate] %s: Slate mode on at present %llu (state %#x, L tap %s; %s)\n", SK_RUNES[i].name,
        (unsigned long long)present, SK.sw0, g_sk_pulse ? "yes" : "no, press L",
        SK.wait ? "rune written, tap waits for the game to apply it" : "BotW's selected rune is used (slaterune off / no export)");
}

// sora.c tick, after the event stretch: Slate mode makes the frame Link's (st = LINK_OTHER)
static int sk_tick(uint64_t present, int st, uint32_t sw) {
    if (!SK.on) return st;
    g_slate_seen = present;
    uint64_t age = present - SK.t0;
    if (sw != SK.last_sw) {  // R3: the state words of the rune's life, with the pad
        LOG("[slate] t+%llu Link state %#x -> %#x (held %#llx)\n", (unsigned long long)age, SK.last_sw, sw, (unsigned long long)g_held);
        SK.last_sw = sw;
        if (sw != SK.sw0) SK.moved = true;
    }
    if (SK.wait) {  // (s99) the L tap follows the game's own update of the wheel object (current id == the requested one), max 45 presents
        int rq = -9, cu = -9;
        if (SKR.get) SKR.get(&rq, &cu);
        if (cu == SK.want || age >= 45) {
            SK.wait = false, g_slate_l_from = present + 2, g_slate_l_until = present + 4;
            int fl = -99;
            if (SLX.gdt_s32) SLX.gdt_s32("PlayerEquipItemType", &fl);
            LOG("[slate] slaterune: %s after %llu presents (wheel req/cur %d/%d, PlayerEquipItemType %d): L tap now\n", cu == SK.want ? "applied" : "NOT applied, tapping anyway",
                (unsigned long long)age, rq, cu, fl);
        }
    }
    SK.stand = st == LINK_GROUND ? SK.stand + 1 : 0;
    if (g_loading) sk_end("load", present);
    else if (linkpos_link_life() <= 0) sk_end("died", present);
    else if (age >= (uint64_t)g_sk_max) sk_end("timeout", present);
    else if (g_sk_bend && !g_sk_stay) {  // (s100) B ends it (edge, after the first 10 presents so the menu's own press doesn't count)
        static uint64_t prevb;
        uint64_t b = g_held & BTN_JUMP;
        if (age >= 10 && b && !prevb) sk_end("B pressed", present);
        prevb = b;
    }
    else if (!g_sk_stay && age >= (uint64_t)g_sk_hold && SK.stand >= SK_STAND) sk_end(SK.moved ? "Link stands again" : "no rune state seen, Link stands", present);
    return SK.on ? LINK_OTHER : st;
}

// R1 / R2 / R3 recon (logs only): "set slaterec 1" = the unlock flags now and after each load; 2 = + Link's state word changes
// with the d-pad / L bits; 3 = + the 900-present actor diff probe (switch runes with BotW's wheel during it: the changed
// words in Link's actor / +0x578 struct are the selected-rune candidates; needs "set slatepass 1" for the wheel)
static void sk_r1(const char *tag) {
    int n = 0;
    for (size_t i = 0; i < sizeof SK_R1_FLAGS / sizeof *SK_R1_FLAGS; i++) {
        int v = -1;
        bool f = sk_flag(SK_R1_FLAGS[i], &v);
        LOG("[slate] R1 %s %s = %s\n", tag, SK_R1_FLAGS[i], f ? (v ? "1" : "0") : "unknown (getter failed)");
        n += f;
    }
    int sel = -99;
    if (SLX.gdt_s32) SLX.gdt_s32("PlayerEquipItemType", &sel);
    LOG("[slate] R2 %s PlayerEquipItemType (BotW's selected rune id: 0 round bomb, 1 cube, 2 Magnesis, 3 Stasis, 4 Cryonis, 5 Camera) = %d\n", tag, sel);
    LOG("[slate] R1 %s: %d of %d flags readable\n", tag, n, (int)(sizeof SK_R1_FLAGS / sizeof *SK_R1_FLAGS));
}
static void sk_recon_tick(uint64_t present) {
    if (!g_sk_rec) return;
    static uint64_t last_load, next_try;
    static bool pending = true;
    static uint32_t last_sw = 0xffffffffu;
    static int probe_was = -1;
    if (g_load_end != last_load) last_load = g_load_end, pending = true;
    if (pending && present >= next_try) {
        next_try = present + 30;
        int v;
        if (sk_flag(SK_R1_FLAGS[0], &v)) sk_r1("load"), pending = false;  // (the getter works only with a world up)
    }
    if (g_sk_rec >= 2) {
        uint32_t sw = linkpos_link_state_word();
        if (sw != last_sw) {
            LOG("[slate] R3 present %llu Link state %#x -> %#x (held %#llx, d-pad up %d, L %d)\n", (unsigned long long)present, last_sw, sw,
                (unsigned long long)g_held, (g_held & BTN_UP) != 0, (g_held & BTN_SHORTCUT) != 0);
            last_sw = sw;
        }
    }
    if (g_sk_rec >= 3) {
        // (s93 review: ONE probe per "set slaterec 3" - it restarted every 900 presents forever; back to level 2 when done)
        if (probe_was < 0) {
            probe_was = g_probe_on, g_probe_on = 1, LOG("[slate] R2: actor diff probe on - switch runes with the wheel now (900 presents)\n");
            probe_start(present);
            if (!PR.on) g_probe_on = probe_was, probe_was = -1, g_sk_rec = 2, LOG("[slate] R2: the probe did not start (no Link actor?): slaterec 2\n");
        } else if (!PR.on) g_probe_on = probe_was, probe_was = -1, g_sk_rec = 2, LOG("[slate] R2: probe done (lines above), slaterec back to 2\n");
    } else if (probe_was >= 0 && !PR.on) g_probe_on = probe_was, probe_was = -1;
}

static bool sk_live_set(const char *key, int v) {
    if (!strcmp(key, "slate")) {
        g_sk = v != 0;
        if (!g_sk && M.sub == SK_SUB) M.sub = -1, M.cursor = 0, sk_via_left = false;
        LOG("[slate] Slate row %s (Items slot, placeholder label)\n", g_sk ? "on" : "off");
    } else if (!strcmp(key, "slateleft")) {
        g_sk_left = v != 0;
        if (!g_sk_left && M.sub == SK_SUB && sk_via_left) M.sub = -1, M.cursor = sk_ret, sk_via_left = false;
        LOG("[slate] slateleft %d: d-pad Left on the top page %s\n", g_sk_left, g_sk_left ? "opens the Slate page (needs slate 1)" : "does nothing");
    } else if (!strcmp(key, "slaterune")) {
        g_sk_rune = v != 0;
        if (g_sk_rune) sk_rune_bind();
        LOG("[slate] slaterune %d: Slate mode %s\n", g_sk_rune, g_sk_rune ? "chooses the row's rune in BotW first (needs the relinked exe)" : "uses BotW's selected rune");
    } else if (!strcmp(key, "slateforce")) g_sk_force = v != 0;
    else if (!strcmp(key, "slatepulse")) g_sk_pulse = v != 0;
    else if (!strcmp(key, "slatehold")) g_sk_hold = v < 0 ? 0 : v;
    else if (!strcmp(key, "slatemax")) g_sk_max = v < 60 ? 60 : v;
    else if (!strcmp(key, "slatestay")) g_sk_stay = v != 0;
    else if (!strcmp(key, "slateb")) g_sk_bend = v != 0, LOG("[slate] slateb %d: Slate mode ends %s\n", g_sk_bend, g_sk_bend ? "on B" : "when Link stands again (old)");
    else if (!strcmp(key, "slateend")) { if (SK.on) sk_end("slateend", vk_frame_counter()); }
    else if (!strcmp(key, "slatepass")) g_slate_pass = v != 0, LOG("[slate] slatepass %d: L + d-pad %s\n", g_slate_pass, g_slate_pass ? "reach BotW (KH2 menu keys dead)" : "are KH2's menu again");
    else if (!strcmp(key, "slaterec")) {
        g_sk_rec = v < 0 ? 0 : v > 3 ? 3 : v;
        LOG("[slate] recon level %d\n", g_sk_rec);
        if (g_sk_rec) sk_r1("now");
    } else return false;
    return true;
}

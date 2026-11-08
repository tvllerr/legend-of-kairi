// sora_outfit.h: (s146 b146-outfit) Sora's KH2 world outfits as draw-actor bodies, picked by the Debug Menu (sora_dbgmenu.h rows
// Outfit / Auto Outfits). DLL only. Plan + bake: recon/S146_HELPER_OUTFIT.md.
//
// KH2 has Sora's per-world suits as extra models of the same skeleton: P_EX100_NM (Halloween Town), _XM (Christmas Town), _TR (Space
// Paranoids), _WI (Timeless River) and one per Drive Form (_BTLF Valor, _MAGF Wisdom, _TRIF Master, _ULTF Final, _KH1F Limit,
// _HTLF Anti), e.g. P_EX100_NM_BTLF = Valor in Halloween Town. VERIFIED (tools/outfitbake bonecmp): all 28 have 228 bones and the same
// parent / scale / rotation / translation as the matching normal body (only a per-bone Flags field differs), so the normal pose / motion
// bakes drive them. tools/outfitbake/bake_outfits.sh bakes data/outfits/<nm|xm|tr|wi>_<sora|valor|wisdom|limit|master|final|anti>_model.bin
// with the form's own bake env (weapon chains + alt blades, so Keyblade, Keyblade skins, Valor's second blade all still draw).
//
// How it plugs in: an outfit body = one more draw actor loaded from <outfit>_<body>_model.bin + the body's OWN pose bake (a model /
// pose pair = one actor, sora_draw_actor_load), loaded lazily the first time it is needed (a 5-80 MB read on the game thread: one
// short hitch). The places that put a body on (sora_form.h fm_enter / fm_revert / driven again, Roxas / Mickey leaving) ask
// ofx_actor_base() / ofx_actor_form(f) which return the normal actor (0 / FM.actor[f]) while the outfit is Normal, and ofx_tick() swaps an
// already shown body when the wanted outfit changes (also drive-form <-> base). Roxas and Mickey have no outfit. Everything the exe
// keeps per body (shadow, ghost, tint, hidden weapon, blade ids) is carried over by sora_draw_set_body.
// Draw actors are a fixed table (exe SORA_DRAW_ACTORS, 24 before s146): an outfit only takes a slot while OF_RESERVE stay free (Roxas,
// Mickey, the skateboard and the summons load theirs later); a refused one logs "[outfit] no free draw actor" and that body stays Normal.
// Nothing here is saved: no sidecar field, no BotW save data.
//
// Auto Outfits (GUESS, KH2 itself never changes clothes by time): night (BotW time of day via the exe's sora_world_env, the same read
// the light uses; night = 19:00..05:00 like sora_light.h's sun) -> Halloween Town; a cold place -> Christmas Town; else Normal. Cold =
// the weather byte is snow (4 / 5, the light's GUESS mapping) OR the climate field equals dbmcold's climate (default none; the
// field has read 0 everywhere so far, GUESS offset) OR dbmcold 1 forces it (test). Cold wins over night. A change must hold
// OF_HOLD_MS before it applies (a load briefly reads garbage). The Outfit row, when not Normal, overrides Auto.
// Live keys (sora_dbgmenu.h dbm_set_key): dbmoutfit 0..4, dbmautoout 0|1, dbmcold -1 (auto) | 0 | 1 | climate id + 2, dbmofres N (reserve).
// Log tag "[outfit]".
#pragma once

#define OF_N 5                 // 0 Normal, 1 Halloween Town (NM), 2 Christmas Town (XM), 3 Space Paranoids (TR), 4 Timeless River (WI)
#define OF_DIR "D:/SoraBotW/data/outfits/"
#define OF_HOLD_MS 2000
static const char *const OF_FILE[OF_N] = {"", "nm", "xm", "tr", "wi"};
static const char *const OF_NAME[OF_N] = {"Normal", "Halloween Town", "Christmas Town", "Space Paranoids", "Timeless River"};
static const char *const OF_BODYNAME[FORM_ANTI + 2] = {"valor", "wisdom", "limit", "master", "final", "anti", "sora"};  // by form id; [FORM_ANTI + 1] = base Sora

static int g_outfit;            // Debug Menu "Outfit" 0..4
static int g_outfit_auto;       // Debug Menu "Auto Outfits"
static int g_of_cold = -1;      // dbmcold: -1 snow weather only, 0 never cold, 1 always cold, >= 2: climate id (value - 2) means cold
static int g_of_reserve = 12;   // free draw actors an outfit must leave (s146b: Roxas, Mickey, skate 3, summons ~9)
#define OF_MIN_CAP 48           // (s146b) no outfits below this many draw actors (the 24-slot exe)
#define OF_ENV "SORA_OUTFIT_ACT"  // (s146b) the loaded actor ids ride the DLL hot reload

static struct {
    bool init, tried_dll;
    int (*load)(const char *, const char *);
    int (*cap)(void);
    int (*bones)(int);
    int act[OF_N][FORM_ANTI + 2];       // the loaded actor of (outfit, body) or 0
    char pose[OF_N][FORM_ANTI + 2][260];  // the pose file it was loaded with (Final's changes with ffps)
    ULONGLONG fail_at[OF_N][FORM_ANTI + 2];
    int fails[OF_N][FORM_ANTI + 2];     // (s146b) failed loads; 3 = given up for the session
    int auto_now, auto_want;            // the Auto outfit in force / the one waiting out OF_HOLD_MS
    ULONGLONG auto_since;
    int shown;                          // the outfit of the body last put on (log only)
    unsigned n;
} OF;

static const char *ofx_pose_path(int bi);
static void ofx_init(void) {
    if (OF.init) return;
    OF.init = true;
    HMODULE exe = GetModuleHandleA(NULL);
    OF.load = (int (*)(const char *, const char *))(void *)GetProcAddress(exe, "sora_draw_actor_load");
    OF.cap = (int (*)(void))(void *)GetProcAddress(exe, "sora_draw_actor_capacity");
    OF.bones = (int (*)(int))(void *)GetProcAddress(exe, "sora_draw_actor_bone_count");
    LOG("[outfit] exe calls: actor_load %s, capacity %s (%d), bone_count %s; reserve %d actors\n", OF.load ? "ok" : "MISSING", OF.cap ? "ok" : "MISSING (old exe)",
        OF.cap ? OF.cap() : 0, OF.bones ? "ok" : "MISSING", g_of_reserve);
    {   // (s146b) a hot reload: the exe still holds the outfit actors; take their ids back (pose path as now) so they are reused and swapped back
        char b[600];
        if (GetEnvironmentVariableA(OF_ENV, b, sizeof b) && b[0] && OF.bones) {
            int n = 0;
            for (const char *p = b; *p;) {
                int o, k, id, c = 0;
                if (sscanf(p, "%d:%d:%d,%n", &o, &k, &id, &c) < 3 || c <= 0) break;
                p += c;
                if (o > 0 && o < OF_N && k >= 0 && k <= FORM_ANTI + 1 && OF.bones(id) > 0)
                    OF.act[o][k] = id, snprintf(OF.pose[o][k], sizeof OF.pose[o][k], "%s", ofx_pose_path(k)), n++;
            }
            if (n) LOG("[outfit] hot reload: %d outfit actors taken back\n", n);
        }
    }
}

// loaded draw actors now (ids 0.. while the exe's bone count says there is one)
static void ofx_persist(void) {
    char b[600] = "";
    int n = 0;
    for (int o = 1; o < OF_N; o++)
        for (int k = 0; k <= FORM_ANTI + 1; k++)
            if (OF.act[o][k] > 0) n += snprintf(b + n, sizeof b - (size_t)n, "%d:%d:%d,", o, k, OF.act[o][k]);
    SetEnvironmentVariableA(OF_ENV, n ? b : NULL);
}
static int ofx_used(void) {
    int cap = OF.cap ? OF.cap() : 0, n = 0;
    for (int i = 0; i < cap && OF.bones && OF.bones(i) > 0; i++) n++;
    return n;
}

static const char *ofx_pose_path(int bi) {
    if (bi == FORM_ANTI + 1) {
        const char *e = getenv("SAVAGEWIND_SORA_POSE");
        return sora_path(e && *e ? e : "D:/SoraBotW/data/sora_pose.bin");  // actor 0's default pose bake (Deck: SORA_ROOT)
    }
    return ffps_path(FM_FORMS[bi].pose);
}

// the draw actor of outfit <o> on body <bi> (a form id 0..FORM_ANTI, or FORM_ANTI + 1 = base Sora); 0 = not available (use the normal one)
static int ofx_get(int o, int bi) {
    if (o <= 0 || o >= OF_N || bi < 0 || bi > FORM_ANTI + 1) return 0;
    ofx_init();
    if (!OF.load) return 0;
    const char *pp = ofx_pose_path(bi);
    if (OF.act[o][bi] > 0 && !strcmp(OF.pose[o][bi], pp)) return OF.act[o][bi];
    ULONGLONG now = GetTickCount64();
    if (OF.fail_at[o][bi] && now - OF.fail_at[o][bi] < 10000) return 0;  // (a refused / failed one is retried every 10 s)
    if (OF.fails[o][bi] >= 3) return 0;  // (s146b) given up for the session
    int cap = OF.cap ? OF.cap() : 0, used = ofx_used();
    if (cap < OF_MIN_CAP) {  // (s146b) the 24-slot exe has no room for outfits
        OF.fails[o][bi] = 3;
        static bool said;
        if (!said) said = true, LOG("[outfit] this exe has %d draw actors (< %d): no outfits (relink with SORA_DRAW_ACTORS 48)\n", cap, OF_MIN_CAP);
        return 0;
    }
    char mp[300];
    snprintf(mp, sizeof mp, "%s", sora_path(OF_DIR));  // (Deck port: D:/SoraBotW -> SORA_ROOT before the exists check)
    snprintf(mp + strlen(mp), sizeof mp - strlen(mp), "%s_%s_model.bin", OF_FILE[o], OF_BODYNAME[bi]);
    if (GetFileAttributesA(mp) == INVALID_FILE_ATTRIBUTES) {  // (s146b) missing file: never read the pose bake for it
        OF.fails[o][bi] = 3;
        LOG("[outfit] %s %s: %s missing (install_outfits.sh); stays Normal for this session\n", OF_NAME[o], OF_BODYNAME[bi], mp);
        return 0;
    }
    if (cap && cap - used <= g_of_reserve) {
        OF.fail_at[o][bi] = now;
        LOG("[outfit] no free draw actor for %s %s: %d of %d used, %d must stay free (relink the exe with a bigger SORA_DRAW_ACTORS, or dbmofres lower); stays Normal\n",
            OF_NAME[o], OF_BODYNAME[bi], used, cap, g_of_reserve);
        return 0;
    }
    ULONGLONG t0 = GetTickCount64();
    int id = OF.load(mp, pp);
    if (id <= 0) {
        OF.fail_at[o][bi] = now, OF.fails[o][bi]++;
        LOG("[outfit] %s %s: actor load failed (%s installed? install_outfits.sh); stays Normal\n", OF_NAME[o], OF_BODYNAME[bi], mp);
        return 0;
    }
    OF.act[o][bi] = id;
    ofx_persist();
    snprintf(OF.pose[o][bi], sizeof OF.pose[o][bi], "%s", pp);
    LOG("[outfit] %s %s: draw actor %d loaded in %llu ms (%d of %d actors used)\n", OF_NAME[o], OF_BODYNAME[bi], id, (unsigned long long)(GetTickCount64() - t0),
        ofx_used(), cap);
    return id;
}

// ---- which outfit is wanted
static bool ofx_night(float hour) { return hour >= 19.0f || hour < 5.0f; }  // (sora_light.h's sun is up 05:00..19:00, GUESS)
static int ofx_auto_pick(void) {
    static bool (*env)(float *);
    static int tried;
    if (!tried) tried = 1, env = (bool (*)(float *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_world_env");
    float e[4] = {0, 0, 0, 0};
    if (!env || !env(e)) return -1;  // no world manager (title / loading): keep what is on
    float hour = e[0] / 15.0f;
    if (g_lighthm >= 0) hour = g_lighthm / 10.0f;  // (the light's test overrides, so lighthour 22 shows the night outfit too)
    else if (g_lighthour >= 0) hour = (float)g_lighthour;
    int weather = (int)(e[2] + 0.5f), climate = (int)(e[3] + 0.5f);
    bool cold = g_of_cold == 1 || (g_of_cold == -1 && (weather == 4 || weather == 5)) || (g_of_cold >= 2 && climate == g_of_cold - 2);
    if (OF.n % 300 == 0) LOG("[outfit] auto env: hour %.2f weather %d climate %d stage %d -> %s\n", hour, weather, climate, (int)(e[1] + 0.5f),
                             cold ? "cold (Christmas Town)" : ofx_night(hour) ? "night (Halloween Town)" : "day (Normal)");
    return cold ? 2 : ofx_night(hour) ? 1 : 0;
}
static int ofx_want(void) {
    if (g_outfit) return g_outfit;
    if (!g_outfit_auto) return 0;
    return OF.auto_now;
}

static int ofx_peek(int o, int bi) { return o > 0 && o < OF_N && bi >= 0 && bi <= FORM_ANTI + 1 ? OF.act[o][bi] : 0; }
// the Limit body that is drawn now: FM.body() when it is one of the Limit actors, else the normal one (no load)
static int ofx_shown_limit(void) {
    int cur = FM.body ? FM.body() : 0;
    if (cur > 0 && cur == FM.actor[FORM_LIMIT]) return cur;
    for (int o = 1; o < OF_N; o++)
        if (cur > 0 && cur == OF.act[o][FORM_LIMIT]) return cur;
    return FM.actor[FORM_LIMIT];
}
// ---- the actors the body switches ask for
static int ofx_actor_base(void) { return ofx_get(ofx_want(), FORM_ANTI + 1); }
static int ofx_actor_form(int f) {
    int id = f >= FORM_VALOR && f <= FORM_ANTI ? ofx_get(ofx_want(), f) : 0;
    return id > 0 ? id : FM.actor[f];
}
static bool ofx_is_sora_actor(int id) {
    if (id == 0) return true;
    for (int f = 0; f < FM_NFORMS; f++)
        if (f != FORM_ROXAS && f != FORM_MICKEY && FM.actor[f] > 0 && FM.actor[f] == id) return true;
    for (int o = 1; o < OF_N; o++)
        for (int b = 0; b <= FORM_ANTI + 1; b++)
            if (OF.act[o][b] > 0 && OF.act[o][b] == id) return true;
    return false;
}
// kb_send (sora.c) asks for every Sora body actor so the Keyblade skin ids reach the outfit bodies too
static int ofx_blade_ids(int *ids, int *fs, int n) {
    for (int o = 1; o < OF_N; o++)
        for (int b = 0; b <= FORM_ANTI + 1; b++)
            if (OF.act[o][b] > 0) ids[n] = OF.act[o][b], fs[n++] = b == FORM_ANTI + 1 ? FORM_VALOR : b;
    return n;
}

// per driven frame (sora.c, next to rox_tick): the Auto outfit, then swap the body on screen if its outfit is not the wanted one
static void ofx_tick(void) {
    ofx_init();
    OF.n++;
    if (g_outfit_auto && !g_outfit) {
        int a = ofx_auto_pick();
        ULONGLONG now = GetTickCount64();
        if (a >= 0 && a != OF.auto_now) {
            if (OF.auto_want != a) OF.auto_want = a, OF.auto_since = now;
            if (now - OF.auto_since >= OF_HOLD_MS) {
                LOG("[outfit] auto: %s -> %s\n", OF_NAME[OF.auto_now], OF_NAME[a]);
                OF.auto_now = a;
            }
        } else OF.auto_want = OF.auto_now;
    } else if (OF.auto_now) OF.auto_now = 0, OF.auto_want = 0;
    if (!FM.set_body || !FM.body || g_loading) return;
    if (IN_ROXAS || mk_body() || g_rox_hold) return;  // (Roxas / Mickey have no outfit)
    bool in_form = FM.st == FM_FORM && !FM.away && g_fform <= FORM_ANTI;
    int id = in_form ? ofx_actor_form(g_fform) : ofx_actor_base();
    int cur = FM.body();
    if (cur == id || !ofx_is_sora_actor(cur)) return;  // (a body of another module - skate rider, summon call - is left alone)
    FM.set_body(id);
    OF.shown = ofx_want();
    LOG("[outfit] %s on %s: body actor %d -> %d\n", OF_NAME[OF.shown], in_form ? FM_FORMS[g_fform].name : "base Sora", cur, id);
}

static bool ofx_set(int o, const char *why) {
    o = o < 0 ? 0 : o >= OF_N ? OF_N - 1 : o;
    g_outfit = o;
    LOG("[outfit] outfit %d (%s) (%s)\n", o, OF_NAME[o], why);
    return true;
}

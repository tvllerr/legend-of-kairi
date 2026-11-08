// sora_react.h (s131 helper B, feature 5): KH2-style Reaction Commands (a Triangle prompt) on BotW threats.
// Included from sora.c after sora_camp.h / sora_guard.h / sora_mkchoice.h (needs CAMP, S, M, MC, G, play, mot_of, LOG).
// Look (s134 rcslot, default): KH2's Reaction Command slot over the command menu (sora_rcslot.h). Look with `set rcslot 0` / an old exe: a box lower left,
// a Triangle icon + the command's name in the sys font. The old box art is a GUESS (plain dim rect + drawn triangle; KH2's real REACT layout was not extracted).
// WHAT IS KH2 AND WHAT IS INVENTED:
//   KH2: the Triangle prompt that appears for a short window, one press runs the command, the motions (Guard reflect A321
//        = motion 42, Dodge Roll A180 = motion 209) are Sora's real KH2 motions.
//   INVENTED: the triggers (KH2 has no Guardians or Lynels), the names "Reflect" / "Dodge", the windows, the dodge's
//        invulnerable frames (40, half the guard's 80), the threat detection thresholds.
// Live keys (default ON, 0 = no prompt, Triangle as before):
//   set react 0/1      the whole feature
//   set reactauto 0/1  automatic threat detection (1 default); 0 = only the test keys
//   set reacttest 1    show the Guardian "Reflect" prompt now (no Guardian needed); Triangle = Sora's reflect motion + the guard window
//   set reacttest 2    show the Lynel "Dodge" prompt now; Triangle = a sidestep roll + invulnerable frames
//   set reactlog 0/1   log detection, and print the AI leaf path of Guardians / Lynels within 40 m (to calibrate the Guardian beam match)
// Detection:
//   Lynel: DLL only. A live "Enemy_Lynel*" 3..14 m away closing at more than 6 m/s (GUESS) = a charge. (A "big swing"
//          is NOT detected: it needs the AI state, see the leaf note.)
//   Guardian: needs the enemy AI leaf path (exe export linkpos_enemy_leaf, callable only inside linkpos_frame) and a match
//          of the beam-charge node name, which is NOT known (RC_BEAM below is a GUESS list). Until reactlog shows the real names,
//          the Guardian prompt is reachable by the test key only. See recon/S131_HELPER_ITEMS.md for the exe need.
#pragma once

static int g_react = 1, g_react_auto = 1, g_react_log = 0, g_react_beam = 0;  // reactbeam 0: the Guardian auto prompt (and its AI-leaf guest calls) stay off until the real node names are known
enum { REACT_NONE, REACT_REFLECT, REACT_DODGE, REACT_SAVE };  // (s131 user) SAVE: standing in a save point ring, Triangle = BotW's menu
static struct {
    int kind;                 // active prompt
    uint64_t t0, until, cool; // ms
    float tx, tz;             // the threat's position (BotW units)
    int shown;                // drawn last frame (clear once at the end)
    int frame;
    struct { uint64_t actor; float x, z; uint64_t ms; } prev[8];
    char last_leaf[8][96];
    SoraHudQuad q[48];
} REACT;
static const char *const RC_BEAM[] = {"\xe3\x83\x93\xe3\x83\xbc\xe3\x83\xa0", "\xe3\x83\xac\xe3\x83\xbc\xe3\x82\xb6\xe3\x83\xbc", "Beam", "Laser"};  // GUESS: UTF-8 "ビーム", "レーザー" AI node names, unverified
#define RC_COOL_MS 5000        // INVENTED
#define RC_CLOSE_COOL_MS 1500  // GUESS: after any close but a use (no flashing re-fire)
#define RC_WIN_REFLECT_MS 1500 // INVENTED
#define RC_WIN_DODGE_MS 800    // INVENTED
#define RC_CHARGE_SPEED 6.0f  // m/s: the LYNEL's own speed toward Sora (GUESS); the 3..14 m range is a GUESS too
#define RC_DODGE_INV_KH 40.0f // invulnerable KH frames of the dodge (INVENTED: half the guard's 80)

static bool rc_eat_x(void) { return g_react && REACT.kind != REACT_NONE && GetTickCount64() <= REACT.until; }  // Triangle is the prompt's while it is up (no climb try / hop)

static void rc_close(const char *why) {
    if (REACT.kind) LOG("[react] prompt %d closed: %s\n", REACT.kind, why);
    if (REACT.kind && strcmp(why, "used")) {  // (review 2) every close but a use: a short cooldown, else detection re-fires 6 frames later with the SE
        uint64_t c = GetTickCount64() + RC_CLOSE_COOL_MS;
        if (c > REACT.cool) REACT.cool = c;
    }
    REACT.kind = REACT_NONE;
    if (REACT.shown && CAMP.quads && !CAMP.drew) CAMP.quads(NULL, 0);
    REACT.shown = 0;
}
static void rc_open(int kind, float tx, float tz, const char *why) {
    uint64_t now = GetTickCount64();
    if (!CAMP.tried) camp_load();
    if (!CAMP.ok || !CAMP.quads) { LOG("[react] camp font / sora_hud_quads missing: no prompt\n"); return; }
    REACT.kind = kind, REACT.t0 = now, REACT.until = now + (kind == REACT_REFLECT ? RC_WIN_REFLECT_MS : kind == REACT_SAVE ? 400 : RC_WIN_DODGE_MS), REACT.tx = tx, REACT.tz = tz;
    sora_sfx_event("submenu", 1.0f);  // GUESS: KH2's REACT appear SE was not found; the menu's submenu SE
    LOG("[react] Triangle prompt %d (%s) for %lu ms\n", kind, why, (unsigned long)(REACT.until - now));
}

static void rc_draw(void) {
    if (!CAMP.ok || !CAMP.quads || CAMP.drew) return;
    const char *txt = REACT.kind == REACT_REFLECT ? "Reflect" : REACT.kind == REACT_SAVE ? "Save" : "Dodge";
    if (rcs_show(txt, false)) {  // (s134 rcslot) KH2's slot over the command menu; an old camp box that was up is cleared once
        if (REACT.shown) CAMP.quads(NULL, 0), REACT.shown = 0;
        return;
    }
    uint8_t msg[24];
    int len = 0;
    for (int c = 0; txt[c] && len < 23; c++) {
        uint8_t b = cs_ascii(&CAMP.seq, txt[c]);
        if (b) msg[len++] = b;
    }
    uint64_t age = GetTickCount64() - REACT.t0;
    float fade = age < 150 ? (float)age / 150.0f : 1.0f;
    CsOut o = {REACT.q, 0, 48};
    float w = 0, h = 0;
    cs_text(&CAMP.seq, NULL, msg, len, 0, 0, 1.0f, NULL, &w, &h);
    const float x0 = 28.0f, y0 = 318.0f, bw = 58.0f + w, bh = h + 14.0f;
    SoraHudQuad *q = &REACT.q[o.n++];  // the dim box
    memset(q, 0, sizeof *q);
    q->x[0] = q->x[2] = x0, q->x[1] = q->x[3] = x0 + bw, q->y[0] = q->y[1] = y0, q->y[2] = q->y[3] = y0 + bh;
    const float bc[4] = {0.05f, 0.05f, 0.08f, 0.7f * fade};
    for (int k = 0; k < 4; k++) q->col[k] = cs_pack_(bc);
    q->tex = 2;
    q = &REACT.q[o.n++];  // the Triangle icon: a quad with the top edge collapsed to a point
    memset(q, 0, sizeof *q);
    float cx = x0 + 22.0f, cy = y0 + bh * 0.5f, r = 11.0f;
    q->x[0] = q->x[1] = cx, q->y[0] = q->y[1] = cy - r;
    q->x[2] = cx - r, q->x[3] = cx + r, q->y[2] = q->y[3] = cy + r * 0.8f;
    const float tc[4] = {0.35f, 0.9f, 0.55f, fade};  // KH2's Triangle is green
    for (int k = 0; k < 4; k++) q->col[k] = cs_pack_(tc);
    q->tex = 2;
    const float sh[4] = {0, 0, 0, 0.75f * fade}, col[4] = {1, 1, 1, fade};
    cs_text(&CAMP.seq, &o, msg, len, x0 + 46.5f, y0 + 7.5f, 1.0f, sh, NULL, NULL);
    cs_text(&CAMP.seq, &o, msg, len, x0 + 45.0f, y0 + 6.0f, 1.0f, col, NULL, NULL);
    CAMP.quads(REACT.q, o.n);
    REACT.shown = 1;
}

// Triangle pressed during the prompt
static void rc_run(const float p[3]) {
    int kind = REACT.kind;
    if (kind == REACT_SAVE) {  // (s131) KH2's save point: Triangle opens the menu (here BotW's: save / load / options)
        sora_sfx_event("decide", 1.0f);
        LOG("[react] SAVE: BotW's menu\n");
        rc_close("used"), sv_menu_now();
        return;
    }
    float dx = REACT.tx - p[0], dz = REACT.tz - p[2];
    bool have = isfinite(REACT.tx) && isfinite(REACT.tz) && dx * dx + dz * dz > 0.01f;  // (review 7: the test key has no threat position: Sora's facing)
    bool free_ = S.act == ACT_IDLE && !S.special && S.takeoff <= 0 && !S.air_frames && !fm_in_form();
    if (!free_) { sora_sfx_event("error", 1.0f); LOG("[react] Triangle but Sora is busy: nothing\n"); rc_close("busy"); return; }
    sora_sfx_event("decide", 1.0f);
    if (kind == REACT_REFLECT) {
        if (have) S.face = atan2f(dx, dz);
        // the Guard motion (173) opens the guard window (beam reflect + block); NOT motion 42 directly, which t0_reaction would offer Counterguard on without a block
        if (!mot_of(MOT_GUARD)) { sora_sfx_event("error", 1.0f); rc_close("no guard motion"); return; }
        play(MOT_GUARD), S.special = SP_GUARD;
        LOG("[react] REFLECT: Guard motion %d (the guard window sends the Guardian beam back)\n", MOT_GUARD);
    } else {
        if (!mot_of(MOT_ROLL)) { sora_sfx_event("error", 1.0f); rc_close("no roll motion"); return; }  // (review 9: no i-frames without the roll)
        float away = have ? atan2f(dx, dz) : S.face;
        away += have ? (float)(PI * 0.5) : 0.0f;  // INVENTED: a sidestep across the charge line, always to the same (+90 deg) side; no threat position = straight ahead
        S.face = away;
        play(MOT_ROLL), S.special = SP_ROLL;  // INVENTED: no roll_learned() / stamina cost, unlike the normal roll
        G.inv = RC_DODGE_INV_KH;  // swallows enemy hits for those frames (guardinv 1 default: only with an enemy near)
        LOG("[react] DODGE: Dodge Roll motion %d, %.0f invulnerable frames\n", MOT_ROLL, G.inv);
    }
    REACT.cool = GetTickCount64() + RC_COOL_MS;
    rc_close("used");
}

static bool rc_leaf_beam(const char *leaf) {
    for (size_t i = 0; i < sizeof RC_BEAM / sizeof *RC_BEAM; i++) if (strstr(leaf, RC_BEAM[i])) return true;
    return false;
}

// the detection; looks at the enemies a few times a second
static void rc_detect(const float p[3], uint64_t now) {
    static int (*leaf)(uint64_t, char *, int);
    static bool bound;
    if (!bound) bound = true, leaf = (int (*)(uint64_t, char *, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_enemy_leaf");
    uint64_t actor[64];
    float pos[64][3];
    int32_t life[64];
    int n = linkpos_enemies(actor, pos, life, 64);
    for (int i = 0; i < n; i++) {
        if (life[i] <= 0) continue;
        char nm[64];
        if (!linkpos_enemy_name(actor[i], nm, sizeof nm)) continue;
        float dx = pos[i][0] - p[0], dz = pos[i][2] - p[2], d = sqrtf(dx * dx + dz * dz);
        bool lyn = strstr(nm, "Lynel") != NULL, gua = strstr(nm, "Guardian") && !strstr(nm, "Mini");
        if (!lyn && !gua) continue;
        if (d < 40.0f && leaf && g_react_log && (lyn || gua)) {  // (review 3: the guest call into the AI only while calibrating)  // calibration: the AI leaf path whenever it changes
            char path[192];
            int k = (int)((actor[i] >> 4) % 8);  // (review 11)
            if (leaf(actor[i], path, sizeof path) > 0 && strncmp(path, REACT.last_leaf[k], 95)) {
                strncpy(REACT.last_leaf[k], path, 95), REACT.last_leaf[k][95] = 0;
                LOG("[react] %s %.1f m leaf: %s\n", nm, d, path);
            }
        }
        if (lyn) {  // closing speed from the last sample of this actor
            int s = -1;
            for (int j = 0; j < 8; j++) if (REACT.prev[j].actor == actor[i]) s = j;
            if (s < 0) for (int j = 0; j < 8 && s < 0; j++) if (!REACT.prev[j].actor || now - REACT.prev[j].ms > 3000) s = j;
            if (s < 0) continue;
            if (REACT.prev[s].actor == actor[i] && now - REACT.prev[s].ms >= 80) {
                float dt = (float)(now - REACT.prev[s].ms) / 1000.0f, odx = REACT.prev[s].x - p[0], odz = REACT.prev[s].z - p[2];
                (void)odx, (void)odz;
                // (review 1) the Lynel's OWN motion toward Sora: Sora running at a standing Lynel must not count
                float ux = d > 0.01f ? -dx / d : 0.0f, uz = d > 0.01f ? -dz / d : 0.0f;
                float closing = ((pos[i][0] - REACT.prev[s].x) * ux + (pos[i][2] - REACT.prev[s].z) * uz) / dt;
                if (g_react_log && closing > 3.0f) LOG("[react] %s %.1f m closing %.1f m/s\n", nm, d, closing);
                if (REACT.kind == REACT_NONE && now >= REACT.cool && closing > RC_CHARGE_SPEED && d > 3.0f && d < 14.0f)
                    rc_open(REACT_DODGE, pos[i][0], pos[i][2], "Lynel charge");
            }
            if (REACT.prev[s].actor != actor[i] || now - REACT.prev[s].ms >= 80)
                REACT.prev[s].actor = actor[i], REACT.prev[s].x = pos[i][0], REACT.prev[s].z = pos[i][2], REACT.prev[s].ms = now;
        } else if (gua && leaf && g_react_beam && REACT.kind == REACT_NONE && now >= REACT.cool && d > 4.0f && d < 40.0f) {
            char path[192];
            if (leaf(actor[i], path, sizeof path) > 0 && rc_leaf_beam(path)) rc_open(REACT_REFLECT, pos[i][0], pos[i][2], "Guardian beam charge");
        }
    }
}

// every driving frame, after mc_input. press / held: Triangle (BTN_CLIMB) is eaten while the prompt is up and pressed.
// can = Sora stands free on the ground (the caller's idle test).
static void rc_tick(const float p[3], uint64_t *press, uint64_t *held, bool can) {
    can = can && !fm_in_form() && !skt_eat_x() && !IN_MICKEY && DR.st == DR_OFF && !pl_eat_x();  // (s134 review: no Dodge while a Limit's Triangle window is up)  // (review 2 / 12: forms, the board (its Triangle is never eaten), Mickey, the dream picker)
    if (!g_react) { if (REACT.kind) rc_close("react 0"); return; }
    uint64_t now = GetTickCount64();
    {   // (s131) the Save prompt: up while Sora stands in a save ring (any form; not on the board / as Mickey / in a menu / in the air)
        bool menu0 = M.sub >= 0 || M.shortcut || MC.on || CAMP.drew || DR.st != DR_OFF;
        bool sok = sv_offer() && !menu0 && !skt_eat_x() && !IN_MICKEY && !S.air_frames && !pl_eat_x();  // (s134 review: no Save during a Limit prompt)
        if (REACT.kind == REACT_SAVE) {
            if (!sok) {  // (no cooldown: the threat prompts stay ready)
                REACT.kind = REACT_NONE;
                if (REACT.shown && CAMP.quads && !CAMP.drew) CAMP.quads(NULL, 0);
                REACT.shown = 0;
                LOG("[react] Save prompt closed\n");
                return;
            }
            REACT.until = now + 400;
            if (*press & BTN_CLIMB) { *press &= ~(uint64_t)BTN_CLIMB, *held &= ~(uint64_t)BTN_CLIMB; rc_run(p); return; }
            *held &= ~(uint64_t)BTN_CLIMB;
            rc_draw();
            return;
        }
        if (sok && REACT.kind == REACT_NONE) { rc_open(REACT_SAVE, NAN, NAN, "save point"); if (REACT.kind == REACT_SAVE) rc_draw(); return; }
    }
    if (REACT.kind) {
        bool menu = M.sub >= 0 || M.shortcut || MC.on || CAMP.drew || DR.st != DR_OFF;
        if (now > REACT.until) { rc_close("window over"); return; }
        if (!can) { rc_close("Sora busy"); return; }
        if (menu) { rc_close("a menu opened"); return; }
        if (*press & BTN_CLIMB) { *press &= ~(uint64_t)BTN_CLIMB, *held &= ~(uint64_t)BTN_CLIMB; rc_run(p); return; }
        *held &= ~(uint64_t)BTN_CLIMB;
        rc_draw();
        return;
    }
    if (!g_react_auto || !can || now < REACT.cool || MC.on || M.sub >= 0 || M.shortcut || CAMP.drew) return;
    if (++REACT.frame % 6 == 0) rc_detect(p, now);
}

static bool rc_live_set(const char *key, int v) {
    if (!strcmp(key, "react")) g_react = v != 0, LOG("[react] %s\n", g_react ? "on" : "off");
    else if (!strcmp(key, "reactauto")) g_react_auto = v != 0, LOG("[react] auto detection %d\n", g_react_auto);
    else if (!strcmp(key, "rcslot")) g_rcslot = v != 0, LOG("[rcslot] %s\n", g_rcslot ? "on: KH2's Reaction Command slot over the command menu" : "off: the camp-font boxes");
    else if (!strcmp(key, "reactlog")) g_react_log = v != 0;
    else if (!strcmp(key, "reactbeam")) g_react_beam = v != 0, LOG("[react] Guardian beam auto prompt %d (needs the real AI node names in RC_BEAM)\n", g_react_beam);
    else if (!strcmp(key, "reacttest")) {
        REACT.cool = 0;
        if (v == 1) rc_open(REACT_REFLECT, NAN, NAN, "test key");
        else if (v == 2) rc_open(REACT_DODGE, NAN, NAN, "test key");
    } else return false;
    return true;
}

// sora_mkchoice.h (s117 mkchoice): KH2's "I won't give up!" / "It's all over..." choice when Sora is KO'd in a Mickey fight.
// Included from sora.c after sora_camp.h (it draws with the camp menu's sys-font table + the exe's sora_hud_quads, both already
// in the shipped exe: NO exe relink). The state machine is in sora_mickey.h mk_tick; this file is the screen. Recon: recon/S117_HELPER_MKCHOICE.md.
//
// KH2 (VERIFIED khwiki King Mickey: "If Sora is defeated in certain boss battles, the Game Over menu changes. Sora gets two
// options: 'I won't give up!' ... and 'It's all over...'"; texts VERIFIED in sys.bar ids 2741 / 2742 = 0xab5 / 0xab6).
// Look = GUESS (the real layout art of that menu was not found; the text is KH2's sys font + a dim, two rows, a highlight bar).
// While it is up: Link's hits are dropped (guard drop) and the clamp holds, the pad is the screen's (up / down / A), the
// world is NOT paused (no pause hook; GUESS vs KH2's stopped fight).
#ifndef SORA_MKCHOICE_H
#define SORA_MKCHOICE_H

static struct {
    bool on;
    int cur, res;        // cursor 0 = "I won't give up!", 1 = "It's all over..."; res: 0 choosing, 1 / 2 / 3 as mc_result
    uint64_t t0;
    uint64_t hprev;      // (s117 review) the raw pad last frame: mc_other's own presses (the driving section's are not made there)
    uint64_t kill_ms;    // (s117 review) when mc_kill_link wrote life 0 (mc_kill_watch: no BotW death in MC_KILL_WAIT_MS = life 1 back)
    SoraHudQuad q[96];
} MC;
#define MC_KILL_WAIT_MS 10000

// sys.bar ids 2741 / 2742 (kmsg.py dump, US)
static const uint8_t MC_T0[] = {0x36, 0x01, 0xb0, 0xa8, 0xa7, 0xec, 0xad, 0x01, 0xa0, 0xa2, 0xaf, 0x9e, 0x01, 0xae, 0xa9, 0x48};
static const uint8_t MC_T1[] = {0x36, 0xad, 0xec, 0xac, 0x01, 0x9a, 0xa5, 0xa5, 0x01, 0xa8, 0xaf, 0x9e, 0xab, 0x4f, 0x4f, 0x4f};

static bool mc_open(void) {
    if (!CAMP.tried) camp_load();
    if (!CAMP.ok || !CAMP.quads) {
        LOG("[mickey] choice: camp font / sora_hud_quads missing -> the automatic roll\n");
        return false;
    }
    MC.on = true, MC.cur = 0, MC.res = 0, MC.t0 = GetTickCount64(), MC.hprev = g_held;
    sora_sfx_event("page", 1);
    return true;
}
static int mc_result(void) { return MC.res; }
static void mc_close(void) {
    if (MC.on && CAMP.quads && !CAMP.drew) CAMP.quads(NULL, 0);
    MC.on = false, g_mc_pad_ms = 0;
}

// every frame from the input section: eats the pad while the screen is up, draws it. true = the screen owns the pad.
static bool mc_input(uint64_t *press, uint64_t *held) {
    if (!MC.on) return false;
    uint64_t p = *press;
    MC.hprev = *held;
    *press = 0, *held = 0;
    if (MC.res) return true;
    g_mc_pad_ms = GetTickCount64();  // (s117 review: pad_tap keeps the pad from BotW while this is fresh)
    uint64_t age = GetTickCount64() - MC.t0;
    if (age > 250) {  // (the hit that KO'd him may still have A / a swing in it)
        if (p & BTN_UP) MC.cur = 1 - MC.cur, sora_sfx_event("cursor", 1);
        if (p & BTN_DOWN) MC.cur = 1 - MC.cur, sora_sfx_event("cursor", 1);
        if (p & BTN_ATTACK) MC.res = MC.cur ? 2 : 1, sora_sfx_event("decide", 1);
        else if (p & BTN_JUMP) sora_sfx_event("error", 1);  // (GUESS: no cancel, like the Dream picker's list)
    }
    // the screen: a dim over everything (virtual 512 x 416 frame, 16:9 = x -85..597), two rows centred, a bar behind the cursor row
    CsOut o = {MC.q, 0, 96};
    float fade = age < 300 ? (float)age / 300.0f : 1.0f;
    {
        SoraHudQuad *q = &MC.q[o.n++];
        memset(q, 0, sizeof *q);
        q->x[0] = q->x[2] = -2048, q->x[1] = q->x[3] = 2560, q->y[0] = q->y[1] = -1024, q->y[2] = q->y[3] = 1440;  // (s117 review: the camp backdrop's extents, any aspect)
        const float c[4] = {0, 0, 0, 0.62f * fade};
        for (int k = 0; k < 4; k++) q->col[k] = cs_pack_(c);
        q->tex = 2;
    }
    const uint8_t *tx[2] = {MC_T0, MC_T1};
    const int tl[2] = {(int)sizeof MC_T0, (int)sizeof MC_T1};
    for (int i = 0; i < 2; i++) {
        float w, h, y = 188.0f + 44.0f * (float)i;
        cs_text(&CAMP.seq, NULL, tx[i], tl[i], 0, 0, 1.0f, NULL, &w, &h);
        float x = 256.0f - w / 2;
        bool sel = i == MC.cur;
        if (sel) {
            SoraHudQuad *q = &MC.q[o.n++];
            memset(q, 0, sizeof *q);
            q->x[0] = q->x[2] = x - 22, q->x[1] = q->x[3] = x + w + 22, q->y[0] = q->y[1] = y - 4, q->y[2] = q->y[3] = y + h + 2;
            const float c[4] = {0.30f, 0.30f, 0.34f, 0.55f * fade};
            for (int k = 0; k < 4; k++) q->col[k] = cs_pack_(c);
            q->tex = 2;
        }
        const float sh[4] = {0, 0, 0, 0.75f * fade}, col[4] = {sel ? 1.0f : 0.55f, sel ? 1.0f : 0.55f, sel ? 1.0f : 0.55f, fade};
        cs_text(&CAMP.seq, &o, tx[i], tl[i], x + 1.5f, y + 1.5f, 1.0f, sh, NULL, NULL);
        cs_text(&CAMP.seq, &o, tx[i], tl[i], x, y, 1.0f, col, NULL, NULL);
    }
    if (!CAMP.drew) CAMP.quads(MC.q, o.n);  // (s117 review: never over the KH2 pause menu's own quads)
    return true;
}

// (s117 review) the not-driving branch of sora_mod_frame_body (Link has the pad: climbing, swimming, a knock-down): the screen
// is drawn and read there too, with its own presses from the raw pad; pad_tap keeps that pad from BotW meanwhile.
static void mc_other(void) {
    if (!MC.on) return;
    uint64_t h = g_held, p = h & ~MC.hprev;
    mc_input(&p, &h);
}

// "It's all over...": Link dies for real. The clamp is already off (MK.doomed); his life goes to 0 on the actor (GUESS that BotW's
// death check reads it, like the Dream/anti writes on +0x848 work in the other direction); if he lives, the next hit kills him.
static void mc_kill_link(void) {
    uint64_t a = link_actor();
    int cur = linkpos_link_life();
    if (a && cur > 0 && rd_ok(a + 0x848, 4) && *(const int32_t *)(uintptr_t)(a + 0x848) == cur) {
        *(int32_t *)(uintptr_t)(a + 0x848) = 0;
        MC.kill_ms = GetTickCount64();
        LOG("[mickey] choice: Link's life %d -> 0 (BotW's death -> the KH2 Game Over screen, s59)\n", cur);
    } else LOG("[mickey] choice: could not write Link's life (actor %s, life %d): the next hit kills him\n", a ? "life differs" : "none", cur);
}

// (s117 review) mk_tick, every frame: sora_anti.h avoids a death by the write itself (GUESS that BotW's own death may need its
// damage path). If no death follows the write (no Game Over screen 0x30, no dead ragdoll, life still 0 after MC_KILL_WAIT_MS),
// Link would stand with 0 hearts and Sora hidden (sora.c's "dead"): 1 quarter heart goes back, the clamp stays off
// (MK.doomed), so the next hit kills him through BotW's own path, as the lost roll did before s117.
static void mc_kill_watch(void) {
    if (!MC.kill_ms) return;
    int life = linkpos_link_life();
    if (life != 0 || g_go_screen || g_loading || linkpos_link_ragdoll() == 2) {
        LOG("[mickey] choice: the life write %s\n", life != 0 ? "is over (life back / no Link)" : "led to BotW's death");
        MC.kill_ms = 0;
        return;
    }
    if (GetTickCount64() - MC.kill_ms < MC_KILL_WAIT_MS) return;
    MC.kill_ms = 0;
    uint64_t a = link_actor();
    if (a && rd_ok(a + 0x848, 4) && *(const int32_t *)(uintptr_t)(a + 0x848) == 0) {
        *(int32_t *)(uintptr_t)(a + 0x848) = 1;
        LOG("[mickey] choice: life 0 for %d ms and no BotW death -> life 1 back (the next hit kills him)\n", MC_KILL_WAIT_MS);
    }
}
#endif

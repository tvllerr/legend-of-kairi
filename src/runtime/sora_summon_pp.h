// (s138 ppgenie) KH2 Summons, rows 2 / 3: GENIE (P_AL010) and PETER PAN (P_EX330) + TINKER BELL (P_EX340). Notes, sources, GUESS list, tests: recon/S138_HELPER_PPGENIE.md.
// The shared state machine (sora_summon.h) runs both rows; this file holds only what is specific to them, behind the smx_* hooks the state machine calls:
//   * Peter Pan's intro has Tinker Bell (s138 review, VERIFIED KH2 object scripts): Peter's act "appear" (P_EX330 bdx 5728) attaches him to Sora, plays his 231, makes Sora play
//     A800 (252, 330 f), spawns Tinker Bell (objentry 1268) and starts HER act "appear" - all in the same frame. Her "appear" (P_EX340 bdx 1657 / 1631): obj_attach to Sora, her
//     231 (330 f), wait for its end, obj_leave. So she flies in WITH Peter during the call and is gone when it ends. One draw actor (her body), no Sora hold of her own.
//     Her 232 + Sora's A802 (340 f) are her act "reraise" (bdx 1695: obj_reraise on Sora = she REVIVES a KO'd Sora): not built (needs a 2nd actor, tinkcall_pose.bin).
//     After the intro KH2 shows her as a light: Peter's thread (P_EX330 bdx 5179 / 5303, started at the end of "appear", "appear_skip", "appear_area_start") spawns (ex330.8 only from
//     appear_skip) ex330.7 (= pp_fx_7, orbiting Sora) and heals Sora (obj_add_hp 5, effect ex330.5 bound on Sora) every 300 frames while summon_is_tink_exist(): BUILT s139 (light + heal, sora_summon_tink.h; the reraise is not).
//     NO sparkle effect (user ruling 2026-11-05: until KH2's own effect is found). Her 231 has no sound trigger.
//   * Genie: his four Drive-Form versions are one mesh (genie_model.bin's 12 variant groups carry alt ids 1 Valor / 2 Wisdom / 3 Master / 4 Final, tools_s137sumdata/genie_alt.py),
//     drawn with sora_draw_actor_blade(actor, 0, version). The cast picks a RANDOM one of the owned forms (KH2 exe FUN_1403de460: forms 1..5 without 3 = Limit, an LCG
//     over the owned ones); the version commands 535-538 "{Valor,Wisdom,Master,Final} Genie" (cmd.bin labels 64..67) switch it: Genie plays 90A (241) and the parts swap
//     (genie_change_form, exe rva 1CF2D0). Menu entries for them come with Genie's Limit commands (not built); live key `sgver <1-4>` meanwhile.
// Live keys: sgver 1|2|3|4 (Genie out: switch to that version now; else: force it for the next cast), sgver 0 (random again).
#pragma once

static void gl_aura_reset(void);   // (s139 genielim, sora_summon_gl.h)
static void gl_reset(void);
#define SMX_TINK_IN 231  // her 80A (330 f): attached to Sora, during Peter's 231 + Sora's A800
static struct {
    int ach;                 // Tinker Bell's draw actor (P_EX340)
    Mot ch[8];
    int nch;
    int stage;               // 0 = not drawn, 1 = her 231 (the intro)
    float clock, origin[3], face;
    int ver, force, pend;    // Genie: the version on screen 1..4, the forced next one (sgver), the one a running 90A is about to show
} SMX;

static const char *const SMX_VER_NAME[4] = {"Valor", "Wisdom", "Master", "Final"};
static const int SMX_VER_CMD[4] = {535, 536, 537, 538};       // KH2 cmd ids (03system cmd, exec 23..26; VERIFIED)
static const int SMX_VER_LIMIT_LABEL[4] = {68, 70, 72, 27};   // cmd.bin labels of the version's first Limit (Sonic / Strike / Arcana / Infinity)

static bool smx_ready(int d) { return !SM_DEF[d].tink || SMX.ach > 0; }

static bool smx_actors(int d) {  // (sm_actors, after the row's own two actors) Tinker Bell's body + Genie's default version
    const SmDef *D = &SM_DEF[d];
    if (D->genie && SM.blade && SM.ach_d[d] > 0) SM.blade(SM.ach_d[d], 0, SMX.ver ? SMX.ver : 1);
    if (!D->tink || smx_ready(d)) return true;
    char m[160], p[160];
    if (!SMX.nch) {  // (frame counts only; missing = 330 f)
        snprintf(m, sizeof m, SM_DATA "tink_motion.bin");
        SMX.nch = mot_file_load(m, SMX.ch, 8);
        if (SMX.nch < 0) SMX.nch = 0;
    }
    snprintf(m, sizeof m, SM_DATA "tink_model.bin"), snprintf(p, sizeof p, SM_DATA "tink_pose.bin");
    SMX.ach = SM.load(m, p);
    if (SMX.ach > 0 && SM.shape) SM.shape(SMX.ach, 0.12f, 0, 0, 0);
    LOG("[summon] draw actor: Tinker Bell %d%s\n", SMX.ach, SMX.ach > 0 ? "" : " (tink_model / tink_pose.bin missing: no Peter Pan)");
    return smx_ready(d);
}
static void smx_env(char *b, size_t n) {  // the hot-reload list (SORA_SM_ACT): append her actor
    if (SMX.ach <= 0) return;
    char t[16];
    snprintf(t, sizeof t, ",%d", SMX.ach);
    strncat(b, t, n - strlen(b) - 1);
}
static void smx_body_off(void) {
    if (SM.off && SMX.ach > 0) SM.off(SMX.ach);
}
static void smx_off(void) {
    smx_body_off();
    smt_hide();  // (s139 tink) hidden stretch / end: the light goes too (it respawns when Sora is driven again)
}
static void smx_reset(void) { SMX.stage = 0, SMX.pend = 0, smt_stop("summon over"); gl_reset(); }

// ---- Genie: the version
static bool smx_ver_owned(int v) {  // 1 Valor, 2 Wisdom (always), 3 Master (2 beasts), 4 Final (4 beasts): ul_form_unlocked, the same gate as the Drive Form menu
    return v >= 1 && v <= 4 && ul_form_unlocked(SMX_VER_NAME[v - 1]);
}
static void smx_set_ver(int v) {
    SMX.ver = v;
    gl_aura_reset();  // (s139) the version aura is restarted with the new element
    if (SM.blade && SM.ach > 0) SM.blade(SM.ach, 0, v);
}
// the version command (535-538): Genie plays 90A (241, 70 f); the parts swap at f20 (GUESS: KH2 calls genie_change_form from the same script branch, the effect gn_fx_4 covers it)
static bool sm_genie_change(int v) {
    const SmDef *D = SMD();
    if (!D->genie || SM.st != SM_ACTIVE || SM.hold || SM.call_on) { LOG("[summon] Genie version change refused: Genie is not out and idle\n"); return false; }
    if (!smx_ver_owned(v)) { LOG("[summon] Genie version %d (%s Genie, cmd %d) refused: that Drive Form is not owned\n", v, SMX_VER_NAME[v - 1], SMX_VER_CMD[v - 1]); return false; }
    if (v == SMX.ver || SMX.pend) return false;
    if (SM.atk) return false;
    SMX.pend = v;
    SM.atk_i = 0, SM.atk = true, SM.nhit = 0, SM.win = -1, SM.idle_t = 0;
    sm_play(241);
    sm_cues(0, sm_mot(241), -1, 0, SM.pos);
    LOG("[summon] Genie: %s Genie (cmd %d): 90A plays, the parts swap at f20\n", SMX_VER_NAME[v - 1], SMX_VER_CMD[v - 1]);
    return true;
}
static void smx_atk_tick(float c0, float c1) {
    if (!SMD()->genie || SM.slot != 241 || !SMX.pend) return;
    if (c0 < 20.0f && c1 >= 20.0f) smx_set_ver(SMX.pend), LOG("[summon] Genie: now %s\n", SMX_VER_NAME[SMX.ver - 1]);
    if (c1 >= sm_frames(241, 70.0f) - 0.01f) {
        if (SMX.pend && SMX.ver != SMX.pend) smx_set_ver(SMX.pend);  // (a fast clock jumped past f20)
        SMX.pend = 0;
    }
}
static int smx_limit_label(void) {  // the page-2 Limit slot's label while a row with its own Limit names is out; 0 = the menu's old rule
    const SmDef *D = SMD();
    if (D->genie) return SMX_VER_LIMIT_LABEL[(SMX.ver >= 1 && SMX.ver <= 4 ? SMX.ver : 1) - 1];
    return D->limit_label;
}

// ---- the cast hook
static void smx_cast(int d, const float feet[3]) {
    const SmDef *D = &SM_DEF[d];
    smx_reset();
    if (D->genie) {
        int own[4], n = 0;
        for (int v = 1; v <= 4; v++)
            if (smx_ver_owned(v)) own[n++] = v;
        int v = SMX.force >= 1 && SMX.force <= 4 ? SMX.force : n > 0 ? own[rand() % n] : 1;
        smx_set_ver(v);
        LOG("[summon] Genie: %s Genie (cmd %d) appears (%s; %d owned form(s): KH2 FUN_1403de460 picks one at random)\n", SMX_VER_NAME[v - 1], SMX_VER_CMD[v - 1],
            SMX.force ? "sgver forced" : "random", n);
    }
    if (D->tink && SMX.ach > 0) {  // (P_EX330 bdx 5815-5826: spawned + "appear" in the same frame as Peter's 231 / Sora's A800)
        SMX.stage = 1, SMX.clock = 0, SMX.face = S.face;
        memcpy(SMX.origin, feet, sizeof SMX.origin);
        LOG("[summon] Tinker Bell flies in with Peter (her 231, attached to Sora)\n");
    }
}
static void smx_dismiss(void) {
    smt_stop("dismissed");  // (s139 tink) KH2 summon_is_tink_exist() is false from the dismiss on
    if (!SMD()->tink || SMX.stage == 0) return;
    SMX.stage = 0;
    smx_off();
    LOG("[summon] Tinker Bell goes with the dismiss\n");
}
static void smx_late(float kh, float to_u) {  // every driven frame: her body
    smt_tick(kh, to_u);  // (s139 tink) the light + heal after the entry
    if (!SMD()->tink || SMX.stage == 0 || SMX.ach <= 0 || !SM.pose) return;
    float f[3];
    if (sm_feet(f)) memcpy(SMX.origin, f, sizeof f), SMX.face = S.face;  // attached to Sora (obj_attach 0x4002): the pose is relative to him
    const Mot *m = mot_find(SMX.ch, SMX.nch, SMX_TINK_IN);
    float fr = m && m->frames ? (float)m->frames : 330.0f;
    SMX.clock += kh;
    if (SMX.clock >= fr) {  // (bdx 1631: her motion ended -> obj_leave)
        SMX.stage = 0;
        smx_body_off();  // (s139 tink: only her body; the light goes on)
        LOG("[summon] Tinker Bell leaves (end of her 231)\n");
        return;
    }
    if (SM.shadow) SM.shadow(SMX.ach, SMX.origin[1], blob_str(0.30f));
    SM.pose(SMX.ach, SMX_TINK_IN, SMX.clock, SMX.origin, SMX.face, to_u);
}

// ---- Peter Pan's attack (s138 review, VERIFIED P_EX330 bdx: battle act 8995 -> decision 6099 -> attack_empty 202 / attack_small 203 / attack_blow 201): in reach he plays
// 203 (attack_small: KH2 needs Sora's target <= 300 cm), and 201 (attack_blow) when that target is taking Sora's finishing blow (sysobj_is_finish_blow + enemy_is_attacked_from:
// here = Sora's last finisher hit it within 45 presents, GUESS window). 202 (attack_empty: target gone or > 300 cm, a lunge) is not used: he walks into reach instead.
// Other rows: the old cycle combo % natk.
static int smx_pick_atk(uint64_t tgt) {
    const SmDef *D = SMD();
    if (!D->tink || D->natk < 3) return D->natk > 0 ? SM.combo % D->natk : 0;
    bool fin = tgt && tgt == g_fin_hit_actor && vk_frame_counter() - g_fin_hit_at <= 45;
    return fin ? 0 : 2;
}

static bool smx_live_set(const char *key, int v) {
    if (smt_live_set(key, v)) return true;  // (s139 tink) tinkfx tinkheal tinklog
    if (!strcmp(key, "sgver")) {
        v = v < 0 ? 0 : v > 4 ? 4 : v;
        if (SMD()->genie && SM.st == SM_ACTIVE && v > 0) sm_genie_change(v);
        else SMX.force = v, LOG("[summon] sgver %d: %s\n", v, v ? "Genie's next cast shows that version" : "Genie's next cast picks a random owned version");
        return true;
    }
    return false;
}

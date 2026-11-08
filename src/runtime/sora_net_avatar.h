// Multiplayer slice 1: the remote Sora (spec docs/specs/2026-11-05-multiplayer-slice1-design.md section 5, plan Task 6).
// Included once by sora.c after sora_camp.h (needs FM_FORMS / ffps_path / g_mots / g_fmots_tab / mk_se_ok / g_k, and
// CAMP.seq = the sys font's table for the name plate).
// Each frame sora_mod_frame sets g_av_local_ok (this frame drove the world: Link's pos read, no load screen, no Game
// Over) and calls avatar_tick(now) right after net_frame. The avatar = one draw actor per remote body, loaded the first
// time that body is sampled from the local body's bake pair with "/./" put before the file name (the exe dedupes
// actors by path string: the same spelling would hand back Sora's own actor; the same strings again after a hot reload
// give back the same ids, so the table never grows). A body that fails -> base Sora's copy; that one failing too ->
// avatar off for the session (logged once).
// (mp4, slice 4) per peer slot k: its own AVS[k] state, actors loaded with k + 1 "/./" inserts ("/./", "/././", ...) so each
// slot gets actors of its own; plate / ribbon / far cull / events per slot; a refused base Sora = only that slot not drawn.
//   draw: sora_draw_actor_pose at the sampled feet / yaw (loopback: + 2 m to his right), weapon / blades / ghost
//   hidden: no sample, faded out, remote NF_LOADING, local load / Game Over / menu (g_av_local_ok), > 150 m away
//   events: NET.inq (sora_net.h) replayed NET_INTERP_MS after arrival (in step with the pose), only while shown
//   sounds of his motions: sora_sfx_motion over the sampled slot's clock (peer_trigger_ranges), at his feet
//   name plate (Task 7): his HELLO name in KH2's sys font AV_PLATE_CM over his feet, through the exe's sora_hud_label
//   (projected at present, hidden behind the camera and under the pause menu); hidden with the avatar
#pragma once
#include "sora_net_plate.h"
#include "sora_net_ribbon.h"  // (mp2-rib: the remote swing ribbon core)

#define AV_BODIES 9          // 0 Sora, 1..6 Valor..Anti, 7 Roxas, 8 Mickey (net_fill)
#define AV_RESERVE 4         // draw actors a remote FORM body must leave free (base Sora's copy may take any free one)
#define AV_LOOP_SIDE_CM 200  // loopback: the copy stands 2 m to Sora's right
#define AV_OBODIES 7         // (mp4) bodies that have outfits: 0 Sora, 1..6 the forms (Roxas 7 / Mickey 8 never)
#define AV_FAR_CM 15000      // farther than 150 m from the local Link: not drawn
#define AV_PLATE_CM 180      // the name plate's anchor (its bottom) over his feet

static bool g_av_local_ok;  // (sora_mod_frame, each frame before avatar_tick)
// (slice 3) avatar_tick's verdict for the remote party: his Sora is drawn this frame, fully opaque, in range; the party
// shares her loopback side shift (x / z, BotW units)
static bool g_av_party_ok[NET_MAX_PEERS];   // (mp4: per slot)
static float g_av_shift[NET_MAX_PEERS][2];

static struct {
    bool tried, ok, dead;
    int (*load)(const char *, const char *);
    int (*cap)(void);
    int (*bones)(int);
    void (*pose)(int, int, float, const float *, float, float);
    void (*off)(int);
    void (*ghost)(int, float);
    void (*weapon)(int, bool);
    void (*blade)(int, int, int);
    bool (*slot_pose)(int, int, float, float *);  // (s170 bonedraw) sora_draw_actor_slot_pose / _world: optional (no bone pose without them)
    void (*world)(int, const float *, const float *, float, float);
    int (*sfx_at)(int, float, const float *);  // (the exe's own: the DLL's sora_sfx_play_at macro would send it back out)
    int formse, roxse;      // sfx.bin markers (-2 = not looked up)
} AV = {.formse = -2, .roxse = -2};
// (mp4) one remote player's avatar state
static struct {
    bool dead, dead_logged, shown, plate_shown;
    int actor[AV_BODIES];   // 0 = not tried, > 0 = loaded, -1 = failed
    int cur;                // actor posed last frame (0 = none; ids are > 0)
    int w, kb0, kb1;        // weapon shown / blades last set on cur (-1 = unset)
    float gh;
    int trig_slot; float trig_prev;
    int outfit, oact;       // (mp4) the outfit his SNAP says / the outfit actor drawn (0 = his normal body)
    int oid[OF_N][AV_OBODIES];  // outfit actor of (outfit, body 0..6): 0 untried, > 0 loaded, -1 given up
    int ofails[OF_N][AV_OBODIES]; ULONGLONG ofail_at[OF_N][AV_OBODIES];
    int skact[2], skfails[2]; ULONGLONG skfail_at[2];  // (s172) his skate rider actor: 0 Sora's, 1 Roxas'; 0 untried, > 0 loaded, -1 given up
} AVS[NET_MAX_PEERS];
_Static_assert(OF_N == NET_OUTFITS, "mp4: sora_outfit.h OF_N must match the wire's NET_OUTFITS");

static bool av_init(void) {
    if (AV.tried) return AV.ok;
    AV.tried = true;
    HMODULE exe = GetModuleHandleA(NULL);
    AV.load = (int (*)(const char *, const char *))(void *)GetProcAddress(exe, "sora_draw_actor_load");
    AV.cap = (int (*)(void))(void *)GetProcAddress(exe, "sora_draw_actor_capacity");
    AV.bones = (int (*)(int))(void *)GetProcAddress(exe, "sora_draw_actor_bone_count");
    AV.pose = (void (*)(int, int, float, const float *, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_pose");
    AV.off = (void (*)(int))(void *)GetProcAddress(exe, "sora_draw_actor_off");
    AV.ghost = (void (*)(int, float))(void *)GetProcAddress(exe, "sora_draw_actor_ghost");
    AV.weapon = (void (*)(int, bool))(void *)GetProcAddress(exe, "sora_draw_actor_weapon");
    AV.blade = (void (*)(int, int, int))(void *)GetProcAddress(exe, "sora_draw_actor_blade");
    AV.sfx_at = (int (*)(int, float, const float *))(void *)GetProcAddress(exe, "sora_sfx_play_at");
    AV.slot_pose = (bool (*)(int, int, float, float *))(void *)GetProcAddress(exe, "sora_draw_actor_slot_pose");
    AV.world = (void (*)(int, const float *, const float *, float, float))(void *)GetProcAddress(exe, "sora_draw_actor_world");
    AV.ok = AV.load && AV.pose && AV.off && AV.ghost && AV.weapon && AV.blade && AV.sfx_at;
    LOG("[net] avatar: draw exports %s\n", AV.ok ? "found" : "MISSING (no remote Sora in this exe)");
    return AV.ok;
}

// p with `dots` x "/./" before its file name (the separator p uses), into out (mp4: slot k uses k + 1)
static void av_dot_pathn(const char *p, char *out, size_t n, int dots) {
    const char *s = strrchr(p, '/'), *b = strrchr(p, '\\');
    if (b > s) s = b;
    char sep = s ? *s : '/', mid[16] = {0};
    for (int i = 0; i < dots && i < 6; i++) mid[2 * i] = '.', mid[2 * i + 1] = sep;
    if (!s) snprintf(out, n, "%s%s", mid, p);
    else snprintf(out, n, "%.*s%c%s%s", (int)(s - p), p, sep, mid, s + 1);
}
static void av_dot_path(const char *p, char *out, size_t n) { av_dot_pathn(p, out, n, 1); }

static int av_used(void) {
    int cap = AV.cap ? AV.cap() : 0, n = 0;
    for (int i = 0; i < cap && AV.bones && AV.bones(i) > 0; i++) n++;
    return n;
}

// slot k's remote draw actor for body <body> (lazy load, cached for the session); -1 = failed / refused
static int avatar_actor_for_body(int k, int body) {
    if (k < 0 || k >= NET_MAX_PEERS || body < 0 || body >= AV_BODIES || !av_init()) return -1;
    if (AVS[k].actor[body]) return AVS[k].actor[body];
    char mp[600], pp[600];
    if (body == 0) {
        const char *m = getenv("SAVAGEWIND_SORA_MODEL"), *q = getenv("SAVAGEWIND_SORA_POSE");
        av_dot_pathn(m && *m ? m : "D:/SoraBotW/data/sora_model.bin", mp, sizeof mp, k + 1);
        av_dot_pathn(q && *q ? q : "D:/SoraBotW/data/sora_pose.bin", pp, sizeof pp, k + 1);
    } else {
        int f = body - 1;
        int cap = AV.cap ? AV.cap() : 0;
        if (cap && cap - av_used() <= AV_RESERVE) {
            AVS[k].actor[body] = -1;
            LOG("[net] slot %d body %d (%s): %d of %d draw actors in use, base Sora drawn instead\n", k + 1, body, FM_FORMS[f].name, av_used(), cap);
            return -1;
        }
        av_dot_pathn(FM_FORMS[f].model, mp, sizeof mp, k + 1);
        av_dot_pathn(ffps_path(FM_FORMS[f].pose), pp, sizeof pp, k + 1);
    }
    uint64_t t = GetTickCount64();
    int id = AV.load(mp, pp);
    AVS[k].actor[body] = id > 0 ? id : -1;
    if (id > 0) LOG("[net] slot %d body %d loaded (%llu ms): actor %d (%s)\n", k + 1, body, (unsigned long long)(GetTickCount64() - t), id, mp);
    else LOG("[net] slot %d body %d: load failed (%s)%s\n", k + 1, body, mp, body ? ", base Sora drawn instead" : "");
    return AVS[k].actor[body];
}

// (mp4) slot k's outfit actor <o> (1..4) for body <body> (0..6), loaded from the same file pair as sora_outfit.h's ofx_get with
// the slot's "/./" spelling (never the local player's outfit actor, never another slot's). 0 = draw his normal body: no file,
// no free actor (g_of_reserve / AV_RESERVE stay free), load failed (retried after 10 s, 3 fails = given up).
static int avatar_outfit_actor(int k, int o, int body) {
    if (o <= 0 || o >= OF_N || body < 0 || body >= AV_OBODIES || !av_init() || !AV.cap) return 0;
    int *id = &AVS[k].oid[o][body];
    if (*id) return *id > 0 ? *id : 0;
    ULONGLONG now = GetTickCount64();
    if (AVS[k].ofail_at[o][body] && now - AVS[k].ofail_at[o][body] < 10000) return 0;
    int bi = body ? body - 1 : FORM_ANTI + 1, cap = AV.cap(), used = av_used(), res = g_of_reserve > AV_RESERVE ? g_of_reserve : AV_RESERVE;
    if (cap < OF_MIN_CAP) { *id = -1; return 0; }
    char mp[300], pp[300], dm[600], dp[600];
    snprintf(mp, sizeof mp, "%s%s_%s_model.bin", sora_path(OF_DIR), OF_FILE[o], OF_BODYNAME[bi]);
    snprintf(pp, sizeof pp, "%s", ofx_pose_path(bi));
    if (GetFileAttributesA(mp) == INVALID_FILE_ATTRIBUTES) {
        *id = -1;
        LOG("[net] slot %d outfit %s body %d: %s missing, his normal body is drawn\n", k + 1, OF_NAME[o], body, mp);
        return 0;
    }
    if (cap - used <= res) {
        AVS[k].ofail_at[o][body] = now;
        int nf = ++AVS[k].ofails[o][body];  // (mp4 review: 3 refusals = given up for the session; logged on the 1st and on giving up)
        if (nf >= 3) *id = -1;
        if (nf == 1 || nf >= 3)
            LOG("[net] slot %d outfit %s body %d: %d of %d draw actors in use (%d must stay free), his normal body is drawn%s\n", k + 1, OF_NAME[o], body, used, cap, res,
                nf >= 3 ? " (given up for this session)" : "");
        return 0;
    }
    av_dot_pathn(mp, dm, sizeof dm, k + 1), av_dot_pathn(pp, dp, sizeof dp, k + 1);
    uint64_t t = GetTickCount64();
    int a = AV.load(dm, dp);
    if (a <= 0) {
        AVS[k].ofail_at[o][body] = now;
        if (++AVS[k].ofails[o][body] >= 3) *id = -1;
        LOG("[net] slot %d outfit %s body %d: load failed (%s), his normal body is drawn\n", k + 1, OF_NAME[o], body, dm);
        return 0;
    }
    *id = a;
    LOG("[net] slot %d outfit %s body %d loaded (%llu ms): actor %d\n", k + 1, OF_NAME[o], body, (unsigned long long)(GetTickCount64() - t), a);
    return a;
}

// the remote body's motion of mset slot <slot>: its form table first, then base Sora's (a guest body never falls back)
static const Mot *av_mot(int body, int slot) {
    const Mot *m = NULL;
    int f = body - 1;
    if (body > 0 && body < AV_BODIES) m = mot_find(g_fmots_tab[f], g_nfmots_tab[f], slot);
    if (m || f == FORM_ROXAS || f == FORM_MICKEY) return m;
    return mot_find(g_mots, g_nmots, slot);
}
static const Mot *av_mot_id(int body, int id) {  // (mot_of's variant order, per body)
    static const int var[] = {3, 2, 0, 1};
    for (int i = 0; i < 4; i++) {
        const Mot *m = av_mot(body, id * 4 + var[i]);
        if (m) return m;
    }
    return NULL;
}

// sfx_slot() for the remote's body: -1 = his motion sounds are off
static int av_sfx_slot(int body, int slot) {
    if (body <= 0 || body >= AV_BODIES) return slot;
    int f = body - 1;
    if (AV.formse == -2) AV.formse = sora_sfx_id("formse_v1"), AV.roxse = sora_sfx_id("roxse_v1");
    if (f == FORM_MICKEY) return mk_se_ok() ? slot + 10000 * (f + 1) : -1;
    if (f == FORM_ROXAS) return g_roxsfx && AV.roxse >= 0 ? slot + 10000 * (f + 1) : -1;
    if (f == FORM_LIMIT && slot >= 844 && !g_lf_on) return -1;  // (LF_SE_OK)
    if (!g_formse_on || (f == FORM_LIMIT && slot >= 844) || AV.formse < 0) return slot;
    return slot + 10000 * (f + 1);
}

// (Task 7) the name plate: sora_hud_label by GetProcAddress (an older exe = no plate, logged once).
// (mp4) sora_hud_label_slot(slot, world, q, n) = one plate per slot 0..3; an exe without it keeps only slot 0 (through the
// old sora_hud_label, logged once)
typedef void (*AvLabel)(const float *, const SoraHudQuad *, int);
typedef void (*AvLabelSlot)(int, const float *, const SoraHudQuad *, int);
static struct { bool tried, slot_logged; AvLabel label; AvLabelSlot label_slot; } AVPL;

static void avatar_plate_off(int k) {
    if (!AVS[k].plate_shown) return;
    if (AVPL.label_slot) AVPL.label_slot(k, NULL, NULL, 0);
    else if (AVPL.label && k == 0) AVPL.label(NULL, NULL, 0);
    AVS[k].plate_shown = false;
}

// <name> over head_world (BotW units) this frame for slot k, alpha 0..1; an empty name = "Sora"
static void avatar_plate(int k, const float head_world[3], const char *name, float alpha) {
    if (!AVPL.tried) {
        AVPL.tried = true;
        HMODULE exe = GetModuleHandleA(NULL);
        AVPL.label_slot = (AvLabelSlot)(void *)GetProcAddress(exe, "sora_hud_label_slot");
        AVPL.label = (AvLabel)(void *)GetProcAddress(exe, "sora_hud_label");
        if ((AVPL.label || AVPL.label_slot) && !CAMP.tried) camp_load();
        LOG("[net] name plate: %s\n", AVPL.label_slot ? (!CAMP.ok ? "no camp_seq.bin font table, no plate" : "on (sora_hud_label_slot, one per slot)")
                                    : AVPL.label ? "sora_hud_label_slot not in this exe (NEEDS EXE RELINK): only slot 0 gets a plate"
                                                 : "sora_hud_label not in this exe (NEEDS EXE RELINK), no plate");
    }
    if (!(AVPL.label_slot || AVPL.label) || !CAMP.ok) return;
    if (!AVPL.label_slot && k > 0) {
        if (!AVPL.slot_logged) AVPL.slot_logged = true, LOG("[net] slot %d: no name plate (exe has only sora_hud_label)\n", k + 1);
        return;
    }
    if (g_net_noplate) { avatar_plate_off(k); return; }
    static SoraHudQuad q[PLATE_MAX];
    int n = plate_quads(&CAMP.seq, name && *name ? name : "Sora", alpha, q, PLATE_MAX);
    if (n <= 0) { avatar_plate_off(k); return; }
    if (AVPL.label_slot) AVPL.label_slot(k, head_world, q, n);
    else AVPL.label(head_world, q, n);
    AVS[k].plate_shown = true;
}

// (mp2-rib) his swing ribbon: sora_net_ribbon.h's core fed with the tables of sora.c (hit_slot_form / ffx_trail_win_form /
// blade_world), drawn on the exe's ribbon slots 2 + 2k + blade (slots 0 / 1 are the local player's; mp4: slot k of the peers
// uses 2 + 2k, 3 + 2k, and only while that index < sora_fx_trail_slots()). Old exe = none, logged once.
typedef void (*AvTrailEx)(int, const float *, const float *, bool, int, int);
static struct { NetRib r[NET_MAX_PEERS]; bool inited, tried; AvTrailEx ex; int nslots; } AVRIB;

static const void *avrib_hit(void *ud, int form, int slot, int blade) { (void)ud; return hit_slot_form(form, slot, blade); }
static bool avrib_win(void *ud, int form, int slot, float clock) { (void)ud; return ffx_trail_win_form(form, slot, clock); }
static void avrib_blade(void *ud, const void *row, float clock, const float feet[3], float yaw, float to_u, float out[6]) {
    (void)ud;
    blade_world((const HitSlot *)row, clock, feet, yaw, to_u, out);
}

static bool avrib_ready(void) {
    if (!AVRIB.tried) {
        AVRIB.tried = true;
        int (*slots)(void) = (int (*)(void))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_fx_trail_slots");
        AVRIB.ex = (AvTrailEx)(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_fx_trail_ex");
        AVRIB.nslots = slots ? slots() : 0;
        if (!slots || !AVRIB.ex || AVRIB.nslots < 4) AVRIB.ex = NULL, LOG("[net] remote ribbon: NEEDS EXE RELINK (sora_fx_trail_slots)\n");
        else LOG("[net] remote ribbons: exe has %d trail slots (peer slots with 2 + 2k + blade < %d get one)\n", AVRIB.nslots, AVRIB.nslots);
    }
    return AVRIB.ex != NULL;
}

// active: he is shown with a body pose; !active = hidden / bone pose / no blade -> only the closing on = false
static void avrib_tick(int k, bool active, int body, int slot, float clock, const float feet[3], float yaw, float to_u,
                       int kb0, int kb1, uint64_t now) {
    if (!AVRIB.inited) {
        for (int i = 0; i < NET_MAX_PEERS; i++) netrib_init(&AVRIB.r[i]);
        AVRIB.inited = true;
    }
    if (!avrib_ready()) return;
    static const NetRibCb cb = {avrib_hit, avrib_win, avrib_blade, NULL};
    NetRibIn in = {active && body - 1 != FORM_ANTI, body - 1, slot, clock, {feet[0], feet[1], feet[2]}, yaw, to_u, now};
    NetRibOut out[NETRIB_BLADES];
    if (!netrib_step(&AVRIB.r[k], &cb, &in, out)) return;
    for (int b = 0; b < NETRIB_BLADES; b++) {
        if (!out[b].push) continue;
        const int ex_slot = 2 + 2 * k + b;
        if (ex_slot >= AVRIB.nslots) continue;  // (mp4: the exe has no ribbon slot for this one)
        // (the SNAP's kb ids are kb_alt() values = KB_TAB index + 1, not item ids; 0 / bad = the Kingdom Key's row)
        const int ka = b ? kb1 : kb0, item = ka > 0 && ka <= KB_N ? KB_TAB[ka - 1].id : 0;
        const KbfxIds *d = g_kbfx ? kbfx_ids(kbfx_row(item)) : NULL;
        AVRIB.ex(ex_slot, out[b].grip, out[b].tip, out[b].on, d ? d->trail : -1, g_kbfx_vflip ? 1 : 0);
    }
}

// (s172) slot k's skate rider actor (w 0 Sora's, 1 Roxas'): the local rider's file pair (sora_skate.h skt_actor) with the slot's "/./"
// spelling, so never the local rider's actor. Refused (AV_RESERVE must stay free) / failed = retried after 10 s, 3 fails = given up;
// <= 0 = the caller draws the old idle pose on his body.
static int avatar_skate_actor(int k, int w) {
    if (k < 0 || k >= NET_MAX_PEERS || w < 0 || w > 1 || !av_init() || !AV.cap) return 0;
    int *id = &AVS[k].skact[w];
    if (*id) return *id;
    ULONGLONG now = GetTickCount64();
    if (AVS[k].skfail_at[w] && now - AVS[k].skfail_at[w] < 10000) return 0;
    static const char *const F[2][2] = {{SKT_DATA "skate_sora_model.bin", SKT_DATA "skate_sora_pose.bin"},
                                        {SKT_DATA "skate_rox_model.bin", SKT_DATA "skate_rox_pose.bin"}};
    int cap = AV.cap(), used = av_used();
    if (cap - used <= AV_RESERVE) {
        AVS[k].skfail_at[w] = now;
        if (++AVS[k].skfails[w] >= 3) *id = -1;
        LOG("[net] slot %d %s skate rider: %d of %d draw actors in use, idle pose instead%s\n", k + 1, w ? "Roxas'" : "Sora's", used, cap, *id < 0 ? " (given up)" : "");
        return *id;
    }
    char mp[600], pp[600];
    av_dot_pathn(F[w][0], mp, sizeof mp, k + 1);
    av_dot_pathn(F[w][1], pp, sizeof pp, k + 1);
    int got = AV.load(mp, pp);
    if (got > 0) {
        *id = got;
        LOG("[net] slot %d %s skate rider loaded: actor %d (%s)\n", k + 1, w ? "Roxas'" : "Sora's", got, mp);
    } else {
        AVS[k].skfail_at[w] = now;
        if (++AVS[k].skfails[w] >= 3) *id = -1;
        LOG("[net] slot %d %s skate rider: load failed (%s), idle pose instead%s\n", k + 1, w ? "Roxas'" : "Sora's", mp, *id < 0 ? " (given up)" : "");
    }
    return *id;
}

// slot k hidden: actor, plate, ribbon closing, party verdict
static void avatar_slot_off(int k) {
    g_av_party_ok[k] = false;
    avrib_tick(k, false, 0, -1, 0, (float[3]){0, 0, 0}, 0, 1, 0, 0, 0);
    if (AVS[k].shown && AV.off && AVS[k].cur > 0) AV.off(AVS[k].cur);
    AVS[k].shown = false;
    avatar_plate_off(k);
}
static void avatar_off(void) { for (int k = 0; k < NET_MAX_PEERS; k++) avatar_slot_off(k); }

// hidden: queued events dropped, his moving fx copies gone (they would freeze where they are)
static void av_drop_events(void) { for (int k = 0; k < NET_MAX_PEERS; k++) net_slot_drop(k); }

// (s170 bonedraw) the remote's ragdoll / Link-puppet pose: the base motion of his bone block, bent onto the joints it carries by the
// local retarget (rag_retarget_core, the rig of his body), drawn with sora_draw_actor_world. False = nothing drawn (no exports, no
// slot, bone count not the rig's): the caller draws today's idle / fall pose. No RIG_CUR / IN_GUEST here: they are the LOCAL body.
static float g_av_bpose[POSE_BUF_BONES * 12];
static bool av_bone_draw(int id, int body, const PeerPose *q, const float feet[3], uint64_t now) {
    (void)now;
    if (!AV.slot_pose || !AV.world || !AV.bones) return false;
    RagRig *rg = body == 7 ? &g_rig_rox : body == 8 ? &g_rig_mk : &g_rig_sora;
    if (!rg->ok) return false;  // (s182 no data/ragmap file: the caller draws today's idle / fall pose)
    int nb = AV.bones(id);
    bool form = body >= 1 && body <= 6 && nb > SORA_RAG_BONES && nb <= POSE_BUF_BONES;
    if (nb <= 0 || nb > POSE_BUF_BONES || (nb != rg->nbones && !form)) return false;
    float clock = q->bones.clock;
    const Mot *bm = av_mot(body, q->bones.slot);
    if (bm) clock = peer_clock_wrap(clock, bm->frames, bm->loop, (bm->flags & 1) != 0);
    if (!AV.slot_pose(id, q->bones.slot, clock, g_av_bpose)) return false;
    static float L[LINK_BONES][3];
    memset(L, 0, sizeof L);
    for (int i = 0; i < NET_BONE_J; i++) memcpy(L[NET_BONE_LINK[i]], q->bones.j[i], sizeof L[0]);
    float hu[3], hl[3];
    memcpy(hu, q->bones.hu, sizeof hu), memcpy(hl, q->bones.hl, sizeof hl);
    float ysc = rg->bind && sora_rag_bind_pos[SORA_J_ROOT][1] > 0 ? rg->bind[rg->root][1] / sora_rag_bind_pos[SORA_J_ROOT][1] : 1;
    if (body == 8 && (q->flags & NF_GHOST)) ysc = (float)g_mkswim / 100;  // (s170 review SF-4: a swimming Mickey, as the local mkswim)
    rag_retarget_core(rg, (const float (*)[3])L, hu, hl, g_av_bpose, nb, form, ysc, true, NULL, NULL);
    if (!(q->flags & NF_WEAPON)) {  // (the local puppet's keyblade_hide_rig + keyblade2_hide, on his rig / bone count)
        keyblade_hide_rig(g_av_bpose, rg);
        if (form) {
            const float *palm = g_av_bpose + RAG_KB2_PARENT * 12 + 9;
            for (int b = SORA_RAG_BONES; b < nb; b++) {
                float *m = g_av_bpose + b * 12;
                memset(m, 0, 9 * sizeof(float));
                m[9] = palm[0], m[10] = palm[1], m[11] = palm[2];
            }
        }
    }
    AV.world(id, g_av_bpose, feet, q->yaw, 0.01f * g_k);
    return true;
}

// one remote player (slot k): pose, plate, ribbon, sounds, events, moving fx
static void av_slot_tick(int k, uint64_t now) {
    NetPeerSlot *sl = &NET.slot[k];
    PeerPose q = sl->used ? peer_sample(&sl->p, now) : (PeerPose){0};
    bool show = q.valid && q.alpha > 0 && !(q.flags & NF_LOADING) && !AVS[k].dead;
    if (show) {
        float dx = q.feet[0] - g_body[0], dy = q.feet[1] - g_body[1], dz = q.feet[2] - g_body[2];
        float far_u = AV_FAR_CM * 0.01f * g_k;
        show = dx * dx + dy * dy + dz * dz <= far_u * far_u;
    }
    int body = q.body < AV_BODIES ? q.body : 0, id = -1;
    if (show) {
        id = avatar_actor_for_body(k, body);
        if (id < 0) body = 0, id = avatar_actor_for_body(k, 0);
        if (id < 0) {  // (a refused base Sora: this slot is not drawn; the others still are)
            AVS[k].dead = true;
            if (!AVS[k].dead_logged) AVS[k].dead_logged = true, LOG("[net] slot %d not drawn: base Sora's copy did not load\n", k + 1);
            show = false;
        }
    }
    if (!show) { avatar_slot_off(k); net_slot_drop(k); return; }
    AVS[k].outfit = q.outfit, AVS[k].oact = 0;
    if (q.outfit > 0 && body < AV_OBODIES) {  // (mp4: his outfit; a refused / missing one = his normal body, same motions)
        int oa = avatar_outfit_actor(k, q.outfit, body);
        if (oa > 0) id = oa, AVS[k].oact = oa;
    }
    // (s172) riding: his own copy of the rider actor (rider + board) plays the SNAP's board slot; no rider actor = the old idle pose
    bool skflag = (q.flags & NF_SKATE) != 0, skate = false;
    if (skflag && g_net_skate) {
        int sa = avatar_skate_actor(k, body == 7 ? 1 : 0);
        if (sa > 0) id = sa, skate = true;
    }
    if (skflag) { if (skate) g_net_skate_drawn++; else g_net_skate_fb++; }

    float feet[3] = {q.feet[0], q.feet[1], q.feet[2]};
    if (g_net_loop) {  // (right of his facing (sin, cos) in x, z = (-cos, sin))
        float side = AV_LOOP_SIDE_CM * 0.01f * g_k;
        feet[0] += -cosf(q.yaw) * side, feet[2] += sinf(q.yaw) * side;
    }
    g_av_party_ok[k] = q.alpha >= 1, g_av_shift[k][0] = feet[0] - q.feet[0], g_av_shift[k][1] = feet[2] - q.feet[2];  // (slice 3)
    // slot + clock: his own, wrapped by its motion (sora.c's motion end); a bone pose (ragdoll / Link puppet / skate
    // rider without a rider actor here) = the body's fall (air) or idle slot, looping on our clock; (s172) NF_SKATE = his rider actor's board slot
    int slot = q.slot;
    float clock = q.clock;
    const Mot *m;
    bool bone = (q.flags & NF_BONEPOSE) != 0 || (skflag && !skate);  // (s172: a rider not drawn as one = the idle pose too)
    if (skate) {
        int fr = 0, lp = -1;
        m = NULL;
        if (skt_slot_info(slot, &fr, &lp)) clock = peer_clock_wrap(clock, fr, lp, lp >= 0);
    } else if (bone) {
        m = av_mot_id(body, (q.flags & NF_AIR) ? MOT_FALL : MOT_IDLE);
        if (m) slot = m->slot, clock = m->frames ? fmodf((float)(now % 600000) * 0.06f, (float)m->frames) : 0;
    } else m = av_mot(body, slot);
    if (m && !skate) clock = peer_clock_wrap(clock, m->frames, m->loop, (m->flags & 1) != 0);

    bool fresh = !AVS[k].shown || id != AVS[k].cur;
    if (id != AVS[k].cur) {
        if (AVS[k].cur > 0 && AVS[k].shown) AV.off(AVS[k].cur);
        AVS[k].cur = id, AVS[k].w = AVS[k].kb0 = AVS[k].kb1 = -1, AVS[k].gh = -1;
    }
    float gh = q.alpha < 1 ? q.alpha : (q.flags & NF_GHOST) ? g_swim_ghost / 100.0f : 0;
    if (gh != AVS[k].gh) AV.ghost(id, gh), AVS[k].gh = gh;
    int w = (q.flags & NF_WEAPON) != 0;
    if (skate) {}  // (s172: the rider actor has no Keyblade; the local rider never sets one either)
    else if (w != AVS[k].w) AV.weapon(id, w != 0), AVS[k].w = w;
    if (!skate && (q.kb_chain != AVS[k].kb0 || q.kb_alt != AVS[k].kb1)) {
        AV.blade(id, 0, q.kb_chain), AV.blade(id, 1, q.kb_alt);
        AVS[k].kb0 = q.kb_chain, AVS[k].kb1 = q.kb_alt;
    }
    bool bdrawn = false;
    if ((q.flags & NF_BONEPOSE) && g_net_bone) {  // (s170 bonedraw) a ragdoll / puppet pose with his joints: retargeted here; else the fallback below
        if (q.bones_ok) g_net_bone_recv++, bdrawn = av_bone_draw(id, body, &q, feet, now);
        if (bdrawn) g_net_bone_drawn++;
        else g_net_bone_fb++;
    }
    if (!bdrawn) AV.pose(id, slot, clock, feet, q.yaw, 0.01f * g_k);
    avrib_tick(k, !bone && !skate, body, slot, clock, feet, q.yaw, 0.01f * g_k, q.kb_chain, q.kb_alt, now);
    AVS[k].shown = true;
    {
        float head[3] = {feet[0], feet[1] + AV_PLATE_CM * 0.01f * g_k, feet[2]};
        avatar_plate(k, head, sl->p.name, q.alpha);
    }

    // his motion sounds (swings, steps, voice rows): from where the clock was, not re-fired on (re)appearing
    if (fresh) AVS[k].trig_slot = slot, AVS[k].trig_prev = clock;
    else {
        PeerRange r[2]; float keep;
        int n = peer_trigger_ranges(AVS[k].trig_slot, AVS[k].trig_prev, slot, clock, m ? m->frames : 0, m ? m->loop : 0,
                                    m && (m->flags & 1), r, &keep);
        int ss = bone || skate ? -1 : av_sfx_slot(body, slot);
        for (int i = 0; i < n && ss >= 0; i++) sora_sfx_motion(ss, r[i].from, r[i].to, feet);
        AVS[k].trig_slot = slot, AVS[k].trig_prev = keep;
    }

    // (slice 3) the remote's bodies as drawn here this frame, for the anchored fx: Sora = the pose just drawn (feet / q.yaw,
    // loopback shift included); Donald / Goofy = net_party_remote_pose (false = not shown)
    NetBody rb[3];
    memset(rb, 0, sizeof rb);
    rb[0].ok = true, memcpy(rb[0].feet, feet, sizeof feet), rb[0].yaw = q.yaw;
    for (int w2 = 0; w2 < NET_PARTY_N; w2++) rb[1 + w2].ok = net_party_remote_pose(k, w2, rb[1 + w2].feet, &rb[1 + w2].yaw);

    // fx / sfx events, NET_INTERP_MS after they came (the pose shows that moment now)
    int kept = 0;
    for (int i = 0; i < sl->ninq; i++) {
        NetInEvent e = sl->inq[i];
        if (now < e.at + NET_INTERP_MS) { sl->inq[kept++] = e; continue; }
        float pos[3] = {e.ev.pos[0] + feet[0] - q.feet[0], e.ev.pos[1] + feet[1] - q.feet[1], e.ev.pos[2] + feet[2] - q.feet[2]};
        float eyaw = e.ev.yaw;
        const NetBody *ab = e.ev.anchor ? net_anchor_body(rb, e.ev.anchor) : NULL;
        if (ab) {  // (slice 3) at the body as drawn now; a party member not shown = the remote Sora (GUESS)
            net_anchor_dec(ab, e.ev.pos, e.ev.yaw, pos, &eyaw);
            NET.st.anc_guess += ab != &rb[e.ev.anchor - 1];
        }
        if (e.ev.kind == 0) {
            net_rfx_room();  // (mp2: his copies hold at most NET_RFX_MAX fx slots; mp4: in total over all peers)
            int h = sora_fx_spawn(e.ev.id, pos, eyaw, e.ev.scale);  // (fx_spawn_g: the slot generation bump, not sent)
            NetRemoteFx *r = &sl->rfx[e.ev.seq];  // (Task 8: its later places / kill move our copy)
            if (h >= 0 && h < 32 && !r->kill) r->h1 = h + 1, r->gen = g_fx_gen[h];
            else if (h >= 0 && h < 32) sora_fx_kill(h);  // (killed before it showed here)
        } else AV.sfx_at(e.ev.id, e.ev.scale, pos);
        NET.st.ev_played++, sl->st.ev_played++;
    }
    sl->ninq = kept;

    // (Task 8) his moving fx (Blizzard shards, bound rings, bullets): on the track, NET_INTERP_MS behind like the pose
    uint64_t at = now > NET_INTERP_MS ? now - NET_INTERP_MS : 0;
    for (int i = 0; i < NET_RFX_N; i++) {
        NetRemoteFx *r = &sl->rfx[i];
        if (!r->h1) continue;
        int h = r->h1 - 1;
        if (!fx_mine(h, r->gen)) { r->h1 = 0; continue; }  // (ended here on its own, slot reused)
        if (r->kill && at >= r->kill_at) {
            if (r->kflags & NMV_FADE) sora_fx_fadeout(h, r->fade);
            else sora_fx_kill(h);
            r->h1 = 0;
            continue;
        }
        float p[3], y;
        if (fxtrack_sample(&r->tr, at, p, &y)) {
            const NetBody *ab = r->tr.anc ? net_anchor_body(rb, r->tr.anc) : NULL;
            if (ab) {  // (slice 3) the sampled offset on the body as drawn THIS frame: a bound fx needs no moves
                float wv[3], wy;
                net_anchor_dec(ab, p, y, wv, &wy);
                memcpy(p, wv, sizeof p), y = wy;
            } else if (r->tr.anc) continue;  // (an anchored track and no body at all: cannot happen, rb[0] is always ok)
            else for (int c = 0; c < 3; c++) p[c] += feet[c] - q.feet[c];
            sora_fx_move(h, p, y);
        }
    }
}

static void avatar_tick(uint64_t now) {
    if (!NET.on || !g_av_local_ok || AV.dead) { avatar_off(); av_drop_events(); return; }
    if (!av_init()) { AV.dead = true; avatar_off(); av_drop_events(); return; }
    for (int k = 0; k < NET_MAX_PEERS; k++) av_slot_tick(k, now);
}

static void avatar_diag(void) {
    LOG("[net] avatar: %s, %d slot(s)\n", !AV.tried ? "not started" : AV.dead || !AV.ok ? "OFF" : "ok", NET_MAX_PEERS);
    for (int k = 0; k < NET_MAX_PEERS; k++)
        LOG("[net]   slot %d: %s%s, shown %d, actor %d, plate %d, outfit %s actor %d; body actors 0..8: %d %d %d %d %d %d %d %d %d\n", k + 1,
            NET.slot[k].used ? "peer" : "free", AVS[k].dead ? " (NOT DRAWN)" : "", AVS[k].shown, AVS[k].cur, AVS[k].plate_shown, OF_NAME[AVS[k].outfit < OF_N ? AVS[k].outfit : 0], AVS[k].oact,
            AVS[k].actor[0], AVS[k].actor[1], AVS[k].actor[2], AVS[k].actor[3], AVS[k].actor[4], AVS[k].actor[5], AVS[k].actor[6],
            AVS[k].actor[7], AVS[k].actor[8]);
    for (int k = 0; k < NET_MAX_PEERS; k++)
        if (AVS[k].skact[0] || AVS[k].skact[1]) LOG("[net]   slot %d skate rider actors Sora %d Roxas %d\n", k + 1, AVS[k].skact[0], AVS[k].skact[1]);
}

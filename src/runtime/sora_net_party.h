// Multiplayer slice 3: the remote Donald + Goofy (spec D:/SoraBotW/docs/specs/2026-11-06-multiplayer-slice3-design.md, unit A).
// Included once by sora.c right after sora_net_avatar.h (needs AV / av_dot_path / g_av_party_ok from it, PD / pwf_sample from
// sora_party.h). The sender side is sora_net_pcap.h (what party_draw_member / pl_draw drew -> SNAP pm).
//   draw: two draw actors, loaded lazily the first time a member is shown, from the files the local party loads with "/./"
//         before the file name (the exe dedupes actors by path string: the plain spelling would hand back our own Donald).
//         Poses = the 8-frame blend (PD.slot_pose from + current, per-bone lerp, PD.world) when NPM_BLEND, else PD.pose;
//         shadow at feet y + floor_dy; water ghost; weapon on / off; Goofy Bash's flying shield (PD.weapon_at + pwf_sample).
//   (mp4: per peer slot k, see NP[k])
//   hidden: netparty 0, net off, !g_av_local_ok, his Sora not drawn / faded / far (g_av_party_ok), the member absent from
//         the bracketing SNAP (absorbed by a Drive Form, party off, ...), the actor failed to load.
//   loopback: the same side shift as his Sora (g_av_shift), so the party keeps its formation with her.
//   sounds: NONE here on purpose: the local party's sounds go through sora_sfx_play_at (the net_note_sfx macro), i.e. they
//         already reach the peer as sfx events and play at the world pos; a second sound path here would double them (and the
//         macro would send ours back out).
#pragma once

// (mp4, slice 4) per peer slot k: NP[k] = that peer's two actors (path spelling with k + 1 "/./" inserts, so each slot has its own).
#define NP_MAX_BONES 256

static struct {
    int id[NET_PARTY_N];       // 0 = not tried, > 0 = loaded, -1 = failed (that member is off for the session)
    bool shown[NET_PARTY_N], vis[NET_PARTY_N], sh_on[NET_PARTY_N], init;
    int wp[NET_PARTY_N], slot[NET_PARTY_N];  // weapon shown last set (-1 unset), slot drawn last
    float gh[NET_PARTY_N], feet[NET_PARTY_N][3], face[NET_PARTY_N];
} NP[NET_MAX_PEERS];
static bool np_notready_logged;
static float np_from[NP_MAX_BONES * 12], np_cur[NP_MAX_BONES * 12];  // (scratch: a blend is built and drawn at once)
static void np_init(int k) {  // (the zero state wants wp = 1 (shown), gh = -1)
    if (NP[k].init) return;
    NP[k].init = true;
    for (int w = 0; w < NET_PARTY_N; w++) NP[k].wp[w] = 1, NP[k].gh[w] = -1;
}

static bool net_party_local_pose(int who, float feet[3], float *face) { return pcap_get(who, feet, face); }
static bool net_party_remote_pose(int k, int who, float feet[3], float *face) {
    if (k < 0 || k >= NET_MAX_PEERS || who < 0 || who >= NET_PARTY_N) return false;
    if (NP[k].vis[who]) {
        memcpy(feet, NP[k].feet[who], 3 * sizeof(float)), *face = NP[k].face[who];
        return true;
    }
    // (review I2) not drawn HERE (netparty 0, no actor, his Sora fading, first frame) but in his SNAP: his sampled place,
    // so his spell fx still land where he is; false only when his game did not draw him either
    PartyPose q = peer_party_sample(&NETP(k), who, GetTickCount64());
    if (!q.valid) return false;
    feet[0] = q.feet[0] + (g_net_loop ? g_av_shift[k][0] : 0), feet[1] = q.feet[1], feet[2] = q.feet[2] + (g_net_loop ? g_av_shift[k][1] : 0);
    *face = q.face;
    return true;
}

// slot k's member `who` remote actor (lazy load, cached for the session); 0 = not ready yet (the local party has not loaded
// the draw API), -1 = failed
static int np_actor(int k, int who) {
    np_init(k);
    if (NP[k].id[who]) return NP[k].id[who];
    if (!PD.load || !PD.pose || !PD.shadow || !PD.shape || !PD.off) {
        if (!np_notready_logged) np_notready_logged = true, LOG("[net] remote party: the party draw API is not loaded (party off / old exe), waiting\n");
        return 0;
    }
    if (!av_init()) { NP[k].id[who] = -1; return -1; }
    int cap = AV.cap ? AV.cap() : 0;
    if (cap && cap - av_used() <= AV_RESERVE) {  // (review I1: as the remote forms, AV_RESERVE stay free for local summons / skate / Roxas)
        NP[k].id[who] = -1;
        LOG("[net] slot %d remote %s: %d of %d draw actors in use, not drawn (this session)\n", k + 1, PARTY[who].name, av_used(), cap);
        return -1;
    }
    const char *dir = getenv("SAVAGEWIND_PARTY_DIR");
    if (!dir || !*dir) dir = "D:/SoraBotW/data/party";
    dir = sora_path(dir);
    char mp[512], pp[512], dm[600], dp[600];
    snprintf(mp, sizeof mp, "%s/%s_model.bin", dir, PARTY[who].name);
    snprintf(pp, sizeof pp, "%s/%s_pose.bin", dir, PARTY[who].name);
    av_dot_pathn(mp, dm, sizeof dm, k + 1), av_dot_pathn(pp, dp, sizeof dp, k + 1);
    uint64_t t = GetTickCount64();
    int id = PD.load(dm, dp);
    NP[k].id[who] = id > 0 ? id : -1;
    if (id > 0) {
        PD.shape(id, PARTY[who].shadow_r, 0, 0, 0);
        if (PD.weapon) PD.weapon(id, true);
        if (PD.weapon_at) PD.weapon_at(id, NULL, 0, NULL, 0, 0);
        LOG("[net] slot %d remote %s loaded (%llu ms): actor %d (%s)\n", k + 1, PARTY[who].name, (unsigned long long)(GetTickCount64() - t), id, dm);
    } else LOG("[net] slot %d remote %s: load failed (%s), not drawn (this session)\n", k + 1, PARTY[who].name, dm);
    return NP[k].id[who];
}

static void np_hide(int k, int who) {
    if (NP[k].shown[who] && PD.off && NP[k].id[who] > 0) PD.off(NP[k].id[who]);
    NP[k].shown[who] = NP[k].vis[who] = false;
}
static void net_party_slot_off(int k) { for (int w = 0; w < NET_PARTY_N; w++) np_hide(k, w); }
static void net_party_off(void) { for (int k = 0; k < NET_MAX_PEERS; k++) net_party_slot_off(k); }

static void np_slot_tick(int k, uint64_t now) {
    np_init(k);
    bool on = !g_net_noparty && NET.on && g_av_local_ok && g_av_party_ok[k] && NET.slot[k].used;
    float to_u = 0.01f * g_k;
    for (int w = 0; w < NET_PARTY_N; w++) {
        PartyPose q;
        int id = 0;
        memset(&q, 0, sizeof q);
        if (on) q = peer_party_sample(&NETP(k), w, now);
        if (!on || !q.valid || (id = np_actor(k, w)) <= 0) { np_hide(k, w); continue; }
        float feet[3] = {q.feet[0] + (g_net_loop ? g_av_shift[k][0] : 0), q.feet[1], q.feet[2] + (g_net_loop ? g_av_shift[k][1] : 0)};
        const PMotX *mo = pmot_find(w, q.slot);  // (the clock past its motion's end, as the local party's)
        float clock = mo ? peer_clock_wrap(q.clock, mo->frames, mo->loop, mo->loop < mo->frames) : q.clock;
        PD.shadow(id, feet[1] + q.floor_dy, blob_str(0.55f));
        if (PD.ghost) {
            float gh = (q.flags & NPM_GHOST) ? g_swim_ghost / 100.0f : 0.0f;
            if (gh != NP[k].gh[w]) PD.ghost(id, gh), NP[k].gh[w] = gh;
        }
        int wp = (q.flags & NPM_WEAPON) != 0;
        if (PD.weapon && wp != NP[k].wp[w]) PD.weapon(id, wp != 0), NP[k].wp[w] = wp;
        if (PD.weapon_at) {  // (Goofy Bash's shield in flight; the older exe has no weapon_at: not drawn, as local)
            if ((q.flags & NPM_SHIELD) && w < PARTY_N) {
                const float *wb = pwf_sample(w, q.shield_t);
                float sp[3] = {q.shield[0] + (feet[0] - q.feet[0]), q.shield[1], q.shield[2] + (feet[2] - q.feet[2])};
                PD.weapon_at(id, wb, wb ? g_pwf[w].nb : 0, sp, q.shield_yaw, to_u), NP[k].sh_on[w] = true;
            } else if (NP[k].sh_on[w]) PD.weapon_at(id, NULL, 0, NULL, 0, 0), NP[k].sh_on[w] = false;
        }
        int nb = PD.bones ? PD.bones(id) : 0;
        bool drawn = false;
        if ((q.flags & NPM_BLEND) && nb > 0 && nb <= NP_MAX_BONES && PD.slot_pose && PD.world &&
            PD.slot_pose(id, q.from_slot, q.from_clock, np_from) && PD.slot_pose(id, q.slot, clock, np_cur)) {
            float wgt = q.blend;  // (party_draw_member's per-bone lerp; all bones: the wire has no catch-phase flag, GUESS)
            for (int bi = 0; bi < nb; bi++) {
                float *d = np_cur + bi * 12, *a = np_from + bi * 12, len[3];
                for (int r = 0; r < 3; r++) {
                    float la = sqrtf(v3dot(a + 3 * r, a + 3 * r)), lb = sqrtf(v3dot(d + 3 * r, d + 3 * r));
                    len[r] = la + (lb - la) * wgt;
                }
                for (int j = 0; j < 12; j++) d[j] = a[j] + (d[j] - a[j]) * wgt;
                ortho_rows(d, len);
            }
            PD.world(id, np_cur, feet, q.face, to_u);
            drawn = true;
        }
        if (!drawn) PD.pose(id, q.slot, clock, feet, q.face, to_u);
        NP[k].slot[w] = q.slot, NP[k].shown[w] = NP[k].vis[w] = true;
        memcpy(NP[k].feet[w], feet, sizeof feet), NP[k].face[w] = q.face;
    }
}

static void net_party_tick(uint64_t now) { for (int k = 0; k < NET_MAX_PEERS; k++) np_slot_tick(k, now); }

static void net_party_diag(void) {
    LOG("[net] remote party: %s\n", g_net_noparty ? "OFF (set netparty 0)" : "on");
    for (int k = 0; k < NET_MAX_PEERS; k++)
        LOG("[net]   slot %d%s: Donald %s actor %d slot %d, Goofy %s actor %d slot %d\n", k + 1,
            g_av_party_ok[k] ? "" : " (his Sora not drawn: party hidden)", NP[k].shown[0] ? "shown" : "hidden", NP[k].id[0], NP[k].slot[0],
            NP[k].shown[1] ? "shown" : "hidden", NP[k].id[1], NP[k].slot[1]);
}

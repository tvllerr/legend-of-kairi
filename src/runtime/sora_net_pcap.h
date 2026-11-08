// Multiplayer slice 3 (unit A, mp3-party): pure parts of the remote Donald + Goofy, host-tested by runtime/test/net_test.c.
//  sender: a per-frame record of what the LOCAL game drew for each party member (written at the draw sites in sora_party.h /
//          sora_plimit.h, read by net_fill -> SNAP s->pm). A member not drawn THIS frame is absent: every frame starts with
//          pcap_begin() (sora_mod_frame, before the frame body), a record is only valid with the current frame's stamp.
//  receiver: peer_party_sample = one member sampled from g_net_peer's ring NET_INTERP_MS behind, like peer_sample.
// Included by sora.c before sora_party.h (the draw sites call it); needs nothing from the game.
#pragma once
#include "sora_net_peer.h"

static struct {
    uint64_t frame, stamp[NET_PARTY_N];
    bool weapon[NET_PARTY_N];  // PD.weapon state (default shown, as the exe loads him)
    NetPartyMember m[NET_PARTY_N];
} PCAP = {.frame = 1, .weapon = {true, true}};

static inline void pcap_begin(void) { PCAP.frame++; }
static inline void pcap_weapon(int who, bool shown) { if (who >= 0 && who < NET_PARTY_N) PCAP.weapon[who] = shown; }
// member `who` as drawn this frame. floor_y = the y his shadow got; blend: the 8-frame blend ran (from_slot / from_clock /
// weight 0..1); sh = Goofy Bash's flying shield {x, y, z, yaw, KH clock} or NULL
static inline void pcap_commit(int who, const float feet[3], float face, int slot, float clock, float floor_y, bool ghost,
                               bool blend, int from_slot, float from_clock, float weight, const float *sh) {
    if (who < 0 || who >= NET_PARTY_N) return;
    NetPartyMember *m = &PCAP.m[who];
    memset(m, 0, sizeof *m);
    m->who = (uint8_t)who, m->slot = (uint16_t)slot, m->clock = clock, m->face = face;
    memcpy(m->feet, feet, sizeof m->feet);
    m->floor_dy = floor_y - feet[1];
    m->flags = (uint8_t)((ghost ? NPM_GHOST : 0) | (PCAP.weapon[who] ? NPM_WEAPON : 0));
    if (blend) m->flags |= NPM_BLEND, m->from_slot = (uint16_t)from_slot, m->from_clock = from_clock, m->blend = weight;
    if (sh) {
        m->flags |= NPM_SHIELD;
        memcpy(m->shield, sh, sizeof m->shield), m->shield_yaw = sh[3], m->shield_t = sh[4];
    }
    PCAP.stamp[who] = PCAP.frame;
}
// net_fill: the members drawn this frame, in `who` order
static inline int pcap_fill(NetPartyMember out[NET_PARTY_N]) {
    int n = 0;
    for (int i = 0; i < NET_PARTY_N; i++)
        if (PCAP.stamp[i] == PCAP.frame) out[n++] = PCAP.m[i];
    return n;
}
static inline bool pcap_get(int who, float feet[3], float *face) {
    // (merge fix: the fx anchors ask at any point of the frame, often BEFORE this frame's party draw: last frame's
    // record counts too; net_fill / pcap_fill still sends only this frame's)
    if (who < 0 || who >= NET_PARTY_N || !PCAP.stamp[who] || PCAP.stamp[who] + 1 < PCAP.frame) return false;  // (review M5: 0 = never drawn)
    memcpy(feet, PCAP.m[who].feet, 3 * sizeof(float)), *face = PCAP.m[who].face;
    return true;
}

typedef struct {
    bool valid; uint8_t flags; uint16_t slot; float clock, feet[3], face, floor_dy;
    uint16_t from_slot; float from_clock, blend;
    float shield[3], shield_yaw, shield_t;
} PartyPose;

static inline const NetPartyMember *net_snap_pm(const NetSnap *s, int who) {
    for (int i = 0; i < s->npm && i < NET_PARTY_N; i++)
        if (s->pm[i].who == who) return &s->pm[i];
    return NULL;
}
static inline float peer_arc(float a, float b, double t) {  // shortest-arc lerp, [0, 2 pi)
    double d = fmod((double)b - a, NET_TWO_PI);
    if (d > NET_TWO_PI / 2) d -= NET_TWO_PI;
    if (d < -NET_TWO_PI / 2) d += NET_TWO_PI;
    return peer_wrap_2pi(a + d * t);
}
// member `who` NET_INTERP_MS behind (peer_sample's bracket). The older SNAP of the bracket must carry him (else not shown:
// absorbed / not drawn there); lerped to the newer one only when it carries him too, else held. Clock = the older SNAP's,
// advanced at NET_KH_FPS (the caller wraps it by the motion). valid = false: not shown.
static inline PartyPose peer_party_sample(const NetPeer *p, int who, uint64_t now_ms) {
    PartyPose q;
    memset(&q, 0, sizeof q);
    if (p->st == PEER_NONE || p->n == 0) return q;
    int64_t render = (int64_t)now_ms - NET_INTERP_MS;
    int i = -1;
    for (int k = p->n - 1; k >= 0; k--)
        if ((int64_t)p->at[k] <= render) { i = k; break; }
    int ia = i < 0 ? 0 : i;
    const NetPartyMember *a = net_snap_pm(&p->ring[ia], who);
    if (!a) return q;
    int64_t a_at = (int64_t)p->at[ia];
    double since = render > a_at ? (double)(render - a_at) : 0.0;
    q.valid = true, q.flags = a->flags, q.slot = a->slot, q.floor_dy = a->floor_dy;
    q.clock = (float)(a->clock + since * NET_KH_FPS / 1000.0);
    q.from_slot = a->from_slot, q.from_clock = a->from_clock, q.blend = a->blend;
    memcpy(q.feet, a->feet, sizeof q.feet), q.face = peer_wrap_2pi(a->face);
    memcpy(q.shield, a->shield, sizeof q.shield), q.shield_yaw = a->shield_yaw, q.shield_t = a->shield_t;
    if (a->flags & NPM_SHIELD) q.shield_t = (float)(a->shield_t + since * NET_KH_FPS / 1000.0);
    const NetPartyMember *b = ia + 1 < p->n && i >= 0 ? net_snap_pm(&p->ring[ia + 1], who) : NULL;
    if (b) {
        double span = (double)(p->at[ia + 1] - p->at[ia]), t = span > 0 ? since / span : 1.0;
        if (t > 1) t = 1;
        for (int k = 0; k < 3; k++) q.feet[k] = (float)(a->feet[k] + (b->feet[k] - a->feet[k]) * t);
        q.face = peer_arc(a->face, b->face, t);
        q.floor_dy = (float)(a->floor_dy + (b->floor_dy - a->floor_dy) * t);
        if ((a->flags & NPM_BLEND) && (b->flags & NPM_BLEND) && a->from_slot == b->from_slot)
            q.blend = (float)(a->blend + (b->blend - a->blend) * t);
        if ((a->flags & NPM_SHIELD) && (b->flags & NPM_SHIELD)) {
            for (int k = 0; k < 3; k++) q.shield[k] = (float)(a->shield[k] + (b->shield[k] - a->shield[k]) * t);
            q.shield_yaw = peer_arc(a->shield_yaw, b->shield_yaw, t);
        }
    }
    return q;
}

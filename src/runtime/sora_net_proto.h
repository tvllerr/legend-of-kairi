// Multiplayer slice 1 wire protocol (spec docs/specs/2026-11-05-multiplayer-slice1-design.md section 3).
// Header-only, pure C, no game includes: host-tested by runtime/test/net_test.c.
// Packet = 'S' 'N' version type player seq(u16 LE) + payload, all little-endian.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

#define NET_MAGIC0 'S'
#define NET_MAGIC1 'N'
#define NET_VERSION 11  // (11: s182 sfx pack ids shift (Roxas run se000 #306) = 0.1.1, 2: log scale byte, 3: fx moves, Task 8, 4: party block + fx anchors, slice 3, 5: 12 events per SNAP + HELLO peer list, slice 4 mp4, 6: NET_EDMG shared enemy HP, s166, 7: NET_ESYNC enemy sync, s168, 8: NF_BONES bone-pose block, s170, 9: NF_SKATE skate rider, s172, 10: NET_FIRE fire sync, s176)
#define NET_PORT 7790
#define NET_MAX_EVENTS 12  // (mp4: 4 -> 12, a party-Limit burst no longer overflows the queue)
#define NET_EVENT_REPEAT 3
#define NET_MAX_MOVES 6  // moving fx per SNAP (Blizzard shards, bound rings, bullets)
#define NET_NAME_MAX 15
#define NET_EDMG_ENT 12  // (v6, see the end of this file)
#define NET_EDMG_MAX 16
#define NET_FIRE_ENT 18   // (v10, see the end of this file)
#define NET_FIRE_MAX 4
#define NET_ESYNC_ENT 40   // (v7, see the end of this file)
#define NET_ESYNC_EV 26
#define NET_ESYNC_MAX 8
#define NET_ESYNC_EVMAX 6
#define NET_PKT_MAX 768  // (v8: 512 -> 768 for the NF_BONES block;  slice 3: 256 -> 512; a full SNAP with both party members + a shield is 309)

enum { NET_HELLO = 1, NET_SNAP = 2, NET_BYE = 3, NET_EDMG = 4, NET_ESYNC = 5, NET_FIRE = 6, NET_AREA = 7, NET_REACT = 8, NET_KILLS = 9 };  // (s191: AREA, s194: REACT, s196: KILLS, no NET_VERSION change: see the end of this file)
#define NET_REACT_ENT 24  // (s194, see the end of this file)
#define NET_REACT_MAX 8
#define NET_KILLS_CHUNK 106       // (s197) kills in one NET_KILLS part: 6 B each (u32 hid + u16 age), 17 + 636 = 653 <= the s196 657 (see the end of this file)
#define NET_KILLS_CHUNKS_MAX 64   // parts of one answer (a u64 bitmap)
#define NET_KILLS_ENT 6           // (s197) one kill in a part: u32 hid, u16 age (s)
#define NET_KILLS_ASK_LEN (NET_HDR + 17)   // 24 (s197: + u16 moon age)
#define NET_KILLS_PART_HDR (NET_HDR + 10)  // 17
enum { NF_WEAPON = 1, NF_AIR = 2, NF_GHOST = 4, NF_GUARD = 8, NF_DEAD = 16, NF_LOADING = 32, NF_BONEPOSE = 64, NF_BONES = 128, NF_SKATE = 256 };
// (v9, s172) NF_SKATE = he rides a skateboard: the SNAP's slot / clock are the KH2 skate rider's board slot (151..170, SKB_SLOTS) and
// its clock (60 Hz frames), body 7 = Roxas' rider, any other = Sora's; the receiver draws its own copy of the rider actor (rider + board).
// Never together with NF_BONEPOSE (a puppet / ragdoll gets him off the board).
// (v8, s170 bone-pose sync, spec docs/specs/2026-11-07-multiplayer-bonepose-sync-design.md) NF_BONES = a NetBones block follows
// the SNAP's party part (only with NF_BONEPOSE): Link's joints the sender's rag_retarget bent Sora onto, in the model space of
// THIS SNAP (relative to its feet, RotY(its yaw), KH cm; x left, y up, z forward) + Link's Head axes, + the base motion.
#define NET_BONE_J 24
#define NET_BONE_SIZE (2 + 4 + NET_BONE_J * 6 + 12)  // u16 slot, f32 clock, 24 x 3 x i16 mm, 2 x 3 x i16 axes x 32767 = 162
// the Link.sbfres bone ids of the 24 joints, wire order (sora_ragmap*.h LINK_J_*: every joint the Sora / Roxas / Mickey maps
// read, + SPINE_2 = the pelvis fallback)
static const uint8_t NET_BONE_LINK[NET_BONE_J] = {1, 2, 3, 4, 5, 7, 10, 17, 28, 29, 31, 34, 41, 52, 53, 90, 106, 108, 110, 111, 112, 114, 116, 117};
typedef struct { uint16_t slot; float clock; float j[NET_BONE_J][3]; float hu[3], hl[3]; } NetBones;  // j / hu / hl: model space, cm / unit

// (slice 3) fx anchors: an fx event / move may be placed relative to a body of the sender: pos = the offset in that body's
// frame (net_to_local: +z = his facing, y up), yaw = relative to his yaw. The receiver adds it to the body as drawn there
// (net_from_local), so a bound fx stays on the drawn body between SNAPs. NANC_WORLD = world pos / yaw as before.
enum { NANC_WORLD = 0, NANC_SORA = 1, NANC_DONALD = 2, NANC_GOOFY = 3 };  // (2 bits on the wire)
typedef struct { uint8_t seq, kind; uint16_t id; float pos[3]; float yaw; float scale; uint8_t anchor; } NetEvent;  // kind 0 fx, 1 sfx
// (Task 8) the fx an event spawned (key: that event's seq) moved / was killed / fades out (fade = frames)
enum { NMV_KILL = 1, NMV_FADE = 2 };
typedef struct { uint8_t ev, flags; float pos[3]; float yaw; float fade; uint8_t anchor; } NetFxMove;  // (flags: NMV_*)
// (slice 3) a party member as the sender DREW it this frame (absent = not drawn there: absorbed, hidden, no party)
#define NET_PARTY_N 2
enum { NPM_GHOST = 1, NPM_WEAPON = 2, NPM_BLEND = 4, NPM_SHIELD = 8 };
typedef struct {
    uint8_t who, flags;           // who 0 Donald, 1 Goofy; flags NPM_*
    uint16_t slot; float clock;   // the drawn motion (mset slot, KH clock)
    float feet[3], face;          // BotW units, radians
    float floor_dy;               // his shadow's floor y - feet y (u; < 0 in the air of a knockdown), +-327 u
    uint16_t from_slot; float from_clock, blend;  // NPM_BLEND: the 8-frame blend from this pose, weight 0..1 (u8)
    float shield[3], shield_yaw, shield_t;        // NPM_SHIELD: Goofy Bash's flying shield (pos, yaw, KH clock)
} NetPartyMember;
typedef struct {
    uint8_t player; uint16_t seq;
    uint8_t body, kb_chain, kb_alt; uint16_t slot; float clock; float feet[3]; float yaw;
    uint16_t flags; uint8_t hp, mp; uint8_t nev; uint8_t outfit; NetEvent ev[NET_MAX_EVENTS];  // (mp4: outfit 0 Normal, 1..4)
    uint8_t nmv; NetFxMove mv[NET_MAX_MOVES];
    uint8_t npm; NetPartyMember pm[NET_PARTY_N];  // (slice 3)
    NetBones bones;  // (v8) valid only when flags & NF_BONES
} NetSnap;

// sender side, per local fx handle: the event that spawned it + its newest move, or a kill / fade riding `rep` SNAPs
// (slice 3) anchor = the body the pos / yaw below are relative to (NANC_*, chosen by the spawn event, sticky); `dirty`
// = a move waits to go out, `mrep` = how many more SNAPs it rides after the first (loss safety)
typedef struct { bool on, dirty, moved; uint8_t ev, rep, flags, anchor, mrep; float pos[3], yaw, fade; } NetFxOut;  // (moved: a move was queued once)
// up to max moves for this SNAP, round robin from *rr (a burst of more than max moving fx shares the SNAPs)
static inline int net_pick_moves(NetFxOut *t, int n, int *rr, NetFxMove *out, int max) {
    int k = 0, last = -1;
    for (int j = 0; j < n && k < max; j++) {
        int i = (*rr + j) % n;
        NetFxOut *o = &t[i];
        if (!o->on || !(o->dirty || o->rep)) continue;
        NetFxMove *m = &out[k++];
        memset(m, 0, sizeof *m);
        m->ev = o->ev, m->flags = o->rep ? o->flags : 0, m->yaw = o->yaw, m->fade = o->fade;
        m->anchor = o->rep ? NANC_WORLD : o->anchor;  // (a kill / fade reads no place)
        memcpy(m->pos, o->pos, sizeof m->pos);
        if (o->mrep) o->mrep--;
        else o->dirty = false;
        if (o->rep && !--o->rep) o->on = false;
        last = i;
    }
    if (last >= 0) *rr = (last + 1) % n;
    return k;
}

// the sender's table: NET_FXO_HANDLES entries by fx handle + a spill for kills / fades still riding their SNAPs when
// the exe hands their slot to a new fx in the same tick (final review I1: the kill was wiped = a looping copy stuck)
#define NET_FXO_HANDLES 32
#define NET_FXO_N (NET_FXO_HANDLES + 8)
// slot h now holds a new fx (ev = its event seq, -1 = not sent): a pending kill / fade moves to the spill first
// (spill full: it takes the entry with the fewest repeats left)
static inline void net_fxo_reuse(NetFxOut *t, int h, int ev) {
    if (h < 0 || h >= NET_FXO_HANDLES) return;
    NetFxOut *o = &t[h];
    if (o->on && o->rep) {
        int best = NET_FXO_HANDLES;
        for (int i = NET_FXO_HANDLES; i < NET_FXO_N; i++) {
            if (!t[i].on) { best = i; break; }
            if (t[i].rep < t[best].rep) best = i;
        }
        t[best] = *o;
    }
    memset(o, 0, sizeof *o);
    if (ev >= 0) o->on = true, o->ev = (uint8_t)ev;
}

// a SNAP's flags word from the local state (sora_net.h net_fill)
static inline uint16_t net_flags(bool weapon, bool air, bool ghost, bool guard, bool dead, bool loading, bool bonepose) {
    return (uint16_t)((weapon ? NF_WEAPON : 0) | (air ? NF_AIR : 0) | (ghost ? NF_GHOST : 0) | (guard ? NF_GUARD : 0) |
                      (dead ? NF_DEAD : 0) | (loading ? NF_LOADING : 0) | (bonepose ? NF_BONEPOSE : 0));
}

// event seqs played, per peer: bitmap of the last 128 seqs behind `last` (ahead of it = always new)
typedef struct { uint8_t seen[32]; uint8_t last, any; } NetEvSeen;

#define NET_HDR 7
#define NET_SNAP_BODY 29     // (mp4: +1 = the outfit byte at p[28])
#define NET_OUTFITS 5        // (mp4) outfit ids 0 Normal .. 4 (sora_outfit.h OF_N); a byte >= this reads as 0
#define NET_EV_SIZE 19
#define NET_MV_SIZE 16
#define NET_PM_SIZE 29       // (slice 3) + NET_PM_SHIELD when NPM_SHIELD
#define NET_PM_SHIELD 16
#define NET_TWO_PI 6.283185307179586
// (mp4) the biggest SNAP: header + body + 12 events + 6 moves + 2 party members with a shield = 450 B must fit a packet
#define NET_SNAP_WORST (NET_HDR + NET_SNAP_BODY + NET_MAX_EVENTS * NET_EV_SIZE + NET_MAX_MOVES * NET_MV_SIZE + NET_PARTY_N * (NET_PM_SIZE + NET_PM_SHIELD))
_Static_assert(NET_SNAP_WORST == 450 && NET_SNAP_WORST <= NET_PKT_MAX, "mp4: worst SNAP must fit NET_PKT_MAX");
// (v8) ... and the same SNAP with the NF_BONES block: 450 + 162 = 612
_Static_assert(NET_SNAP_WORST + NET_BONE_SIZE == 612 && NET_SNAP_WORST + NET_BONE_SIZE <= NET_PKT_MAX, "v8: worst SNAP + bone block must fit NET_PKT_MAX");
// (mp4) HELLO v5: name + up to NET_HELLO_PEERS live peer addresses (ip as in sockaddr_in = network order, port) for the gossip
#define NET_HELLO_PEERS 3
#define NET_ADDR_SIZE 6
typedef struct { uint32_t ip; uint16_t port; } NetAddr;

static inline void net_w16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static inline uint16_t net_r16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline void net_wf(uint8_t *p, float f) {
    uint32_t v; memcpy(&v, &f, 4);
    p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24);
}
static inline float net_rf(const uint8_t *p) {
    uint32_t v = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
    float f; memcpy(&f, &v, 4); return f;
}
static inline uint16_t net_yaw_enc(float yaw) {
    double t = fmod((double)yaw, NET_TWO_PI);
    if (t < 0) t += NET_TWO_PI;
    return (uint16_t)((long)floor(t / NET_TWO_PI * 65536.0 + 0.5) & 0xffff);
}
static inline float net_yaw_dec(uint16_t v) { return (float)(v / 65536.0 * NET_TWO_PI); }
// (slice 3) world point w -> the frame of a body at feet / yaw (facing = (sin yaw, cos yaw) in x, z) and back
static inline void net_to_local(const float feet[3], float yaw, const float w[3], float out[3]) {
    float c = cosf(yaw), s = sinf(yaw), dx = w[0] - feet[0], dz = w[2] - feet[2];
    out[0] = dx * c - dz * s, out[1] = w[1] - feet[1], out[2] = dx * s + dz * c;
}
static inline void net_from_local(const float feet[3], float yaw, const float l[3], float out[3]) {
    float c = cosf(yaw), s = sinf(yaw);
    out[0] = feet[0] + l[0] * c + l[2] * s, out[1] = feet[1] + l[1], out[2] = feet[2] - l[0] * s + l[2] * c;
}
// (slice 3) fx anchors, sender side. A body = a drawn character (Sora 0, Donald 1, Goofy 2: NANC_* = index + 1).
#define NET_ANCHOR_CM 500  // an fx within 5 m of a body is placed relative to the nearest one
typedef struct { bool ok; float feet[3], yaw; } NetBody;
// nearest ok body within max_u (BotW units) of pos, 3D distance -> its index, -1 = none (NANC_WORLD)
static inline int net_anchor_pick(const NetBody *b, int n, const float pos[3], float max_u) {
    int best = -1;
    float bd = max_u * max_u;
    for (int i = 0; i < n; i++) {
        if (!b[i].ok) continue;
        float dx = pos[0] - b[i].feet[0], dy = pos[1] - b[i].feet[1], dz = pos[2] - b[i].feet[2];
        float d = dx * dx + dy * dy + dz * dz;
        if (d <= bd) bd = d, best = i;
    }
    return best;
}
static inline float net_wrap_2pi(double a) {
    a = fmod(a, NET_TWO_PI);
    if (a < 0) a += NET_TWO_PI;
    return (float)(a >= NET_TWO_PI ? 0 : a);
}
// the world place (w, wyaw) as an offset from body b (net_to_local) + yaw relative to his
static inline void net_anchor_enc(const NetBody *b, const float w[3], float wyaw, float pos[3], float *yaw) {
    net_to_local(b->feet, b->yaw, w, pos);
    *yaw = net_wrap_2pi((double)wyaw - b->yaw);
}
// receiver side: the offset (pos, yaw) from a body as drawn here -> the world place
static inline void net_anchor_dec(const NetBody *b, const float pos[3], float yaw, float out[3], float *oyaw) {
    net_from_local(b->feet, b->yaw, pos, out);
    *oyaw = net_wrap_2pi((double)yaw + b->yaw);
}
// receiver: the body an anchor refers to, among the remote's three as drawn here (rb[0] Sora is always ok). NULL = world.
// A Donald / Goofy anchor whose member is not shown falls back to the remote Sora (GUESS: nearest thing we know).
static inline const NetBody *net_anchor_body(const NetBody rb[3], int anchor) {
    if (anchor < NANC_SORA || anchor > NANC_GOOFY) return NULL;
    return rb[anchor - 1].ok ? &rb[anchor - 1] : rb[0].ok ? &rb[0] : NULL;
}
// sender: queue the fx's place (pos / yaw already in the anchor's frame). Changed by > eps_u / eps_yaw or the anchor
// changed (or always) = true: dirty, riding NET_EVENT_REPEAT SNAPs (always = 1, the old send-every-move). false = skipped.
static inline bool net_fxo_place(NetFxOut *o, int anchor, const float pos[3], float yaw, float eps_u, float eps_yaw, bool always) {
    float dx = pos[0] - o->pos[0], dy = pos[1] - o->pos[1], dz = pos[2] - o->pos[2];
    double dy_ = fmod((double)yaw - o->yaw, NET_TWO_PI);
    if (dy_ > NET_TWO_PI / 2) dy_ -= NET_TWO_PI;
    if (dy_ < -NET_TWO_PI / 2) dy_ += NET_TWO_PI;
    // (merge fix: the FIRST move always goes out: a bound fx's first anchored place equals its spawn's, and the receiver only
    // follows a body once a move came - without it a bound fx stayed where it spawned)
    bool ch = always || !o->moved || anchor != o->anchor || dx * dx + dy * dy + dz * dz > eps_u * eps_u || fabs(dy_) > eps_yaw;
    if (!ch) return false;
    memcpy(o->pos, pos, sizeof o->pos), o->yaw = yaw, o->anchor = (uint8_t)anchor, o->moved = true;
    o->dirty = true, o->mrep = always ? 0 : NET_EVENT_REPEAT - 1;
    return true;
}
// the fx's spawn event carried this place: later moves are compared with it (an unmoved fx sends nothing)
static inline void net_fxo_base(NetFxOut *o, int anchor, const float pos[3], float yaw) {
    memcpy(o->pos, pos, sizeof o->pos), o->yaw = yaw, o->anchor = (uint8_t)anchor;
}

static inline uint16_t net_clock_enc(float c) {  // KH frames x 10, clamped to 0..6553.5
    double t = (double)c * 10.0 + 0.5;
    return t <= 0 ? 0 : t >= 65535.0 ? 65535 : (uint16_t)t;
}
// fx scale / sfx gain: 0 = 0, else 2^((b-160)/20) = 0.004..26.9 in 3.5% steps (gain 1 exact). (Task 8: fx spawn at
// to_u = 0.01 * g_k, the old linear x32 byte sent them as 0 = invisible on the remote)
static inline uint8_t net_scale_enc(float s) {
    if (!(s > 0)) return 0;
    double t = 160.0 + 20.0 * log2((double)s) + 0.5;
    return t < 1.0 ? 1 : t >= 255.0 ? 255 : (uint8_t)t;
}
#define NET_GAIN_MAX 4.0f  // (mp2) the most any local sfx sends is ~2 (g_fmse_vol / 100); the byte reaches 26.9
static inline float net_scale_dec(uint8_t b) { return b ? (float)exp2((b - 160) / 20.0) : 0.0f; }

static inline int net_hdr(uint8_t *out, int type, uint8_t player, uint16_t seq) {
    out[0] = NET_MAGIC0, out[1] = NET_MAGIC1, out[2] = NET_VERSION, out[3] = (uint8_t)type, out[4] = player;
    net_w16(out + 5, seq);
    return NET_HDR;
}

// (v8) round(v * scale) clamped to [-32767, 32767] as the int16's wire bits; a NaN = 0
static inline uint16_t net_i16_round(float v, double scale) {
    double t = floor((double)v * scale + 0.5);
    if (!(t == t)) t = 0;
    return (uint16_t)(int16_t)(t < -32767 ? -32767 : t > 32767 ? 32767 : t);
}
static inline int net_pack_snap(const NetSnap *s, uint8_t *out, int cap) {
    if (s->nev > NET_MAX_EVENTS || s->nmv > NET_MAX_MOVES || s->npm > NET_PARTY_N) return -1;
    int n = NET_HDR + NET_SNAP_BODY + s->nev * NET_EV_SIZE + s->nmv * NET_MV_SIZE;
    for (int i = 0; i < s->npm; i++) n += NET_PM_SIZE + (s->pm[i].flags & NPM_SHIELD ? NET_PM_SHIELD : 0);
    bool bones = (s->flags & NF_BONES) != 0;
    if (bones) n += NET_BONE_SIZE;
    if (cap < n) return -1;
    uint8_t *p = out + net_hdr(out, NET_SNAP, s->player, s->seq);
    p[0] = s->body, p[1] = s->kb_chain, p[2] = s->kb_alt;
    net_w16(p + 3, s->slot);
    net_w16(p + 5, net_clock_enc(s->clock));
    for (int i = 0; i < 3; i++) net_wf(p + 7 + 4 * i, s->feet[i]);
    net_w16(p + 19, net_yaw_enc(s->yaw));
    net_w16(p + 21, s->flags);
    p[23] = s->hp, p[24] = s->mp, p[25] = s->nev;
    p[26] = s->nmv, p[27] = s->npm;  // (slice 3: 27 = party members)
    p[28] = s->outfit < NET_OUTFITS ? s->outfit : 0;  // (mp4)
    p += NET_SNAP_BODY;
    for (int e = 0; e < s->nev; e++, p += NET_EV_SIZE) {
        const NetEvent *v = &s->ev[e];
        p[0] = v->seq, p[1] = (uint8_t)((v->kind & 15) | (v->anchor & 3) << 4);
        net_w16(p + 2, v->id);
        for (int i = 0; i < 3; i++) net_wf(p + 4 + 4 * i, v->pos[i]);
        net_w16(p + 16, net_yaw_enc(v->yaw));
        p[18] = net_scale_enc(v->scale);
    }
    for (int e = 0; e < s->nmv; e++, p += NET_MV_SIZE) {
        const NetFxMove *m = &s->mv[e];
        p[0] = m->ev, p[1] = (uint8_t)((m->flags & 15) | (m->anchor & 3) << 4);
        for (int i = 0; i < 3; i++) net_wf(p + 2 + 4 * i, m->pos[i]);
        net_w16(p + 14, m->flags & NMV_FADE ? net_clock_enc(m->fade) : net_yaw_enc(m->yaw));
    }
    for (int e = 0; e < s->npm; e++) {  // (slice 3) who flags slot clock feet[3] face floor_dy from_slot from_clock blend
        const NetPartyMember *m = &s->pm[e];
        p[0] = m->who, p[1] = m->flags;
        net_w16(p + 2, m->slot), net_w16(p + 4, net_clock_enc(m->clock));
        for (int i = 0; i < 3; i++) net_wf(p + 6 + 4 * i, m->feet[i]);
        net_w16(p + 18, net_yaw_enc(m->face));
        {
            double t = floor((double)m->floor_dy * 100.0 + 0.5);  // (1/100 u, clamped to int16)
            net_w16(p + 20, (uint16_t)(int16_t)(t < -32768 ? -32768 : t > 32767 ? 32767 : t));
        }
        net_w16(p + 22, m->from_slot), net_w16(p + 24, net_clock_enc(m->from_clock));
        float b = m->blend > 0 ? (m->blend < 1 ? m->blend : 1) : 0;
        p[26] = (uint8_t)(b * 255.0f + 0.5f), p[27] = p[28] = 0;  // (27-28 reserved)
        p += NET_PM_SIZE;
        if (m->flags & NPM_SHIELD) {
            for (int i = 0; i < 3; i++) net_wf(p + 4 * i, m->shield[i]);
            net_w16(p + 12, net_yaw_enc(m->shield_yaw)), net_w16(p + 14, net_clock_enc(m->shield_t));
            p += NET_PM_SHIELD;
        }
    }
    if (bones) {  // (v8) u16 slot, f32 clock, 24 x 3 x i16 mm, hu / hl 3 x i16 x 32767
        const NetBones *b = &s->bones;
        net_w16(p, b->slot), net_wf(p + 2, b->clock);
        p += 6;
        for (int k = 0; k < NET_BONE_J; k++)
            for (int i = 0; i < 3; i++, p += 2) net_w16(p, net_i16_round(b->j[k][i], 10.0));
        for (int i = 0; i < 3; i++, p += 2) net_w16(p, net_i16_round(b->hu[i], 32767.0));
        for (int i = 0; i < 3; i++, p += 2) net_w16(p, net_i16_round(b->hl[i], 32767.0));
    }
    return n;
}

// (mp4) HELLO = hdr, name len, name, peer count, count x (ip 4 raw, port u16 LE); entries past NET_HELLO_PEERS are cut
static inline int net_pack_hello_peers(uint8_t player, uint16_t seq, const char *name, const NetAddr *pa, int npa,
                                       uint8_t *out, int cap) {
    size_t len = name ? strlen(name) : 0;
    if (len > NET_NAME_MAX) len = NET_NAME_MAX;
    if (npa < 0 || !pa) npa = 0;
    if (npa > NET_HELLO_PEERS) npa = NET_HELLO_PEERS;
    int n = NET_HDR + 1 + (int)len + 1 + npa * NET_ADDR_SIZE;
    if (cap < n) return -1;
    net_hdr(out, NET_HELLO, player, seq);
    out[NET_HDR] = (uint8_t)len;
    if (len) memcpy(out + NET_HDR + 1, name, len);
    uint8_t *p = out + NET_HDR + 1 + len;
    *p++ = (uint8_t)npa;
    for (int i = 0; i < npa; i++, p += NET_ADDR_SIZE) memcpy(p, &pa[i].ip, 4), net_w16(p + 4, pa[i].port);
    return n;
}
static inline int net_pack_hello(uint8_t player, uint16_t seq, const char *name, uint8_t *out, int cap) {
    return net_pack_hello_peers(player, seq, name, NULL, 0, out, cap);
}

static inline int net_pack_bye(uint8_t player, uint16_t seq, uint8_t *out, int cap) {
    if (cap < NET_HDR) return -1;
    return net_hdr(out, NET_BYE, player, seq);
}

// (s191) NET_AREA text rules (used by net_unpack_ex; the packet itself is at the end of this file)
#define NET_AREA_MAX 31  // (sora_area.h AREA_CAP 32 with its 0)
static inline bool net_area_chars(const char *s, int n) {  // letters, digits, '-', '_' only (sora_area.h area_text's set)
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}

// false on bad magic/version/type/size; snap filled for NET_SNAP, name for NET_HELLO
// (mp4) pa[NET_HELLO_PEERS] / *npa get a HELLO's peer list (entries with ip 0 or port 0 are dropped); NULL = not wanted
static inline bool net_unpack_ex(const uint8_t *in, int len, int *type, uint8_t *player, uint16_t *seq, NetSnap *snap,
                                 char name[NET_NAME_MAX + 1], NetAddr *pa, int *npa) {
    if (npa) *npa = 0;
    if (!in || len < NET_HDR || in[0] != NET_MAGIC0 || in[1] != NET_MAGIC1 || in[2] != NET_VERSION) return false;
    int t = in[3];
    const uint8_t *p = in + NET_HDR;
    if (t == NET_SNAP) {
        if (len < NET_HDR + NET_SNAP_BODY) return false;
        int nev = p[25], nmv = p[26], npm = p[27];
        if (nev > NET_MAX_EVENTS || nmv > NET_MAX_MOVES || npm > NET_PARTY_N) return false;
        int need = NET_HDR + NET_SNAP_BODY + nev * NET_EV_SIZE + nmv * NET_MV_SIZE;
        for (int e = 0; e < npm; e++) {  // (slice 3) a party entry's size depends on its shield flag
            if (len < need + NET_PM_SIZE) return false;
            need += NET_PM_SIZE + (in[need + 1] & NPM_SHIELD ? NET_PM_SHIELD : 0);
        }
        bool wbones = (in[7 + 21] | in[7 + 22] << 8) & NF_BONES;  // flags at p[21..22]
        if (len != need + (wbones ? NET_BONE_SIZE : 0)) return false;
        if (snap) {
            memset(snap, 0, sizeof *snap);
            snap->player = in[4], snap->seq = net_r16(in + 5);
            snap->body = p[0], snap->kb_chain = p[1], snap->kb_alt = p[2];
            snap->slot = net_r16(p + 3);
            snap->clock = net_r16(p + 5) / 10.0f;
            for (int i = 0; i < 3; i++) snap->feet[i] = net_rf(p + 7 + 4 * i);
            snap->yaw = net_yaw_dec(net_r16(p + 19));
            snap->flags = net_r16(p + 21);
            snap->hp = p[23], snap->mp = p[24], snap->nev = (uint8_t)nev;
            snap->outfit = p[28] < NET_OUTFITS ? p[28] : 0;  // (mp4: a byte out of range = Normal)
            const uint8_t *q = p + NET_SNAP_BODY;
            for (int e = 0; e < nev; e++, q += NET_EV_SIZE) {
                NetEvent *v = &snap->ev[e];
                v->seq = q[0], v->kind = q[1] & 15, v->anchor = q[1] >> 4 & 3, v->id = net_r16(q + 2);
                for (int i = 0; i < 3; i++) v->pos[i] = net_rf(q + 4 + 4 * i);
                v->yaw = net_yaw_dec(net_r16(q + 16));
                v->scale = net_scale_dec(q[18]);
                if (v->kind == 1 && v->scale > NET_GAIN_MAX) v->scale = NET_GAIN_MAX;  // (mp2: a sane sfx gain)
            }
            snap->nmv = (uint8_t)nmv;
            for (int e = 0; e < nmv; e++, q += NET_MV_SIZE) {
                NetFxMove *m = &snap->mv[e];
                m->ev = q[0], m->flags = q[1] & 15, m->anchor = q[1] >> 4 & 3;
                for (int i = 0; i < 3; i++) m->pos[i] = net_rf(q + 2 + 4 * i);
                if (m->flags & NMV_FADE) m->fade = net_r16(q + 14) / 10.0f;
                else m->yaw = net_yaw_dec(net_r16(q + 14));
            }
            int kp = 0;
            for (int e = 0; e < npm; e++) {  // (slice 3) a member with a bad / repeated who or a non-finite place is dropped alone
                NetPartyMember *m = &snap->pm[kp];
                memset(m, 0, sizeof *m);
                m->who = q[0], m->flags = q[1];
                m->slot = net_r16(q + 2), m->clock = net_r16(q + 4) / 10.0f;
                for (int i = 0; i < 3; i++) m->feet[i] = net_rf(q + 6 + 4 * i);
                m->face = net_yaw_dec(net_r16(q + 18));
                m->floor_dy = (float)(int16_t)net_r16(q + 20) / 100.0f;
                m->from_slot = net_r16(q + 22), m->from_clock = net_r16(q + 24) / 10.0f, m->blend = q[26] / 255.0f;
                q += NET_PM_SIZE;
                if (m->flags & NPM_SHIELD) {
                    for (int i = 0; i < 3; i++) m->shield[i] = net_rf(q + 4 * i);
                    m->shield_yaw = net_yaw_dec(net_r16(q + 12)), m->shield_t = net_r16(q + 14) / 10.0f;
                    q += NET_PM_SHIELD;
                    if (!(isfinite(m->shield[0]) && isfinite(m->shield[1]) && isfinite(m->shield[2]))) m->flags &= (uint8_t)~NPM_SHIELD;
                }
                bool dup = false;
                for (int k = 0; k < kp; k++) dup = dup || snap->pm[k].who == m->who;
                if (m->who < NET_PARTY_N && !dup && isfinite(m->feet[0]) && isfinite(m->feet[1]) && isfinite(m->feet[2])) kp++;
            }
            snap->npm = (uint8_t)kp;
            if (wbones) {  // (v8) the block sits after the party members; q is there
                NetBones *b = &snap->bones;
                b->slot = net_r16(q), b->clock = net_rf(q + 2);
                q += 6;
                for (int k = 0; k < NET_BONE_J; k++)
                    for (int i = 0; i < 3; i++, q += 2) b->j[k][i] = (float)(int16_t)net_r16(q) / 10.0f;
                for (int i = 0; i < 3; i++, q += 2) b->hu[i] = (float)(int16_t)net_r16(q) / 32767.0f;
                for (int i = 0; i < 3; i++, q += 2) b->hl[i] = (float)(int16_t)net_r16(q) / 32767.0f;
                if (!isfinite(b->clock)) return false;  // a non-finite clock = a bad packet
                // no NF_BONEPOSE = the block means nothing (ignored, packet kept)
                if (!(snap->flags & NF_BONEPOSE)) snap->flags &= (uint16_t)~NF_BONES, memset(b, 0, sizeof *b);
            }
            // (mp2) NaN / Inf from the wire never reaches sora_fx_spawn / move or the pose: bad feet = the packet is
            // bad; a bad event / move entry is dropped alone (one stuck NaN fx must not freeze the whole avatar)
            if (!(isfinite(snap->feet[0]) && isfinite(snap->feet[1]) && isfinite(snap->feet[2]))) return false;
            // (mp3 review M1) huge finite places (a bad peer) would overflow to Inf in net_from_local: |x| > 1e6 u = not finite
            for (int e = 0; e < nev; e++)
                for (int i = 0; i < 3; i++) if (fabsf(snap->ev[e].pos[i]) > 1e6f) snap->ev[e].pos[i] = NAN;
            for (int e = 0; e < nmv; e++)
                for (int i = 0; i < 3; i++) if (fabsf(snap->mv[e].pos[i]) > 1e6f) snap->mv[e].pos[i] = NAN;
            int ke = 0, km = 0;
            for (int e = 0; e < nev; e++)
                if (isfinite(snap->ev[e].pos[0]) && isfinite(snap->ev[e].pos[1]) && isfinite(snap->ev[e].pos[2]))
                    snap->ev[ke++] = snap->ev[e];
            for (int e = 0; e < nmv; e++)  // (a kill / fade never reads pos: kept, so the fx still dies)
                if ((snap->mv[e].flags & (NMV_KILL | NMV_FADE)) ||
                    (isfinite(snap->mv[e].pos[0]) && isfinite(snap->mv[e].pos[1]) && isfinite(snap->mv[e].pos[2])))
                    snap->mv[km++] = snap->mv[e];
            snap->nev = (uint8_t)ke, snap->nmv = (uint8_t)km;
        }
    } else if (t == NET_HELLO) {
        if (len < NET_HDR + 2) return false;
        int n = p[0];
        if (n > NET_NAME_MAX || len < NET_HDR + 2 + n) return false;
        int np = p[1 + n];
        if (np > NET_HELLO_PEERS || len != NET_HDR + 2 + n + np * NET_ADDR_SIZE) return false;
        if (name) {
            memcpy(name, p + 1, (size_t)n);
            name[n] = 0;
        }
        const uint8_t *q = p + 2 + n;
        int k = 0;
        for (int i = 0; i < np; i++, q += NET_ADDR_SIZE) {
            NetAddr a;
            memcpy(&a.ip, q, 4), a.port = net_r16(q + 4);
            if (a.ip && a.port && pa) pa[k++] = a;
        }
        if (npa) *npa = k;
    } else if (t == NET_BYE) {
        if (len != NET_HDR) return false;
    } else if (t == NET_EDMG) {  // (s166 v6) the entries are read by net_unpack_edmg; here only the size is checked
        if (len < NET_HDR + NET_EDMG_ENT || (len - NET_HDR) % NET_EDMG_ENT || (len - NET_HDR) / NET_EDMG_ENT > NET_EDMG_MAX) return false;
    } else if (t == NET_FIRE) {  // (s176 v10) the entries are read by net_unpack_fire; here only the size is checked
        if (len < NET_HDR + NET_FIRE_ENT || (len - NET_HDR) % NET_FIRE_ENT || (len - NET_HDR) / NET_FIRE_ENT > NET_FIRE_MAX) return false;
    } else if (t == NET_ESYNC) {  // (s168 v7) entries are read by net_unpack_esync; here only the exact size is checked
        if (len < NET_HDR + 2 || len != NET_HDR + 2 + p[0] * NET_ESYNC_ENT + p[1] * NET_ESYNC_EV || p[0] > NET_ESYNC_MAX || p[1] > NET_ESYNC_EVMAX || !(p[0] + p[1])) return false;
    } else if (t == NET_AREA) {  // (s191) the text is read by net_unpack_area; here the exact size and the letters are checked
        if (len < NET_HDR + 1 || p[0] > NET_AREA_MAX || len != NET_HDR + 1 + p[0] || !net_area_chars((const char *)p + 1, p[0])) return false;
    } else if (t == NET_REACT) {  // (s194) the entries are read by net_unpack_react; here only the size is checked
        if (len < NET_HDR + NET_REACT_ENT || (len - NET_HDR) % NET_REACT_ENT || (len - NET_HDR) / NET_REACT_ENT > NET_REACT_MAX) return false;
    } else if (t == NET_KILLS) {  // (s196) the body is read by net_unpack_kills; here the op, the exact size and the part numbers are checked
        if (len < NET_HDR + 1) return false;
        if (p[0] == 1) { if (len != NET_KILLS_ASK_LEN) return false; }  // (NKO_ASK)
        else if (p[0] == 2) {  // (NKO_PART: i < n <= 64, k <= 106, length exact)
            if (len < NET_KILLS_PART_HDR || p[8] < 1 || p[8] > NET_KILLS_CHUNKS_MAX || p[7] >= p[8] || p[9] > NET_KILLS_CHUNK || len != NET_KILLS_PART_HDR + NET_KILLS_ENT * p[9]) return false;
        } else return false;
    } else {
        return false;
    }
    if (type) *type = t;
    if (player) *player = in[4];
    if (seq) *seq = net_r16(in + 5);
    return true;
}

static inline bool net_unpack(const uint8_t *in, int len, int *type, uint8_t *player, uint16_t *seq, NetSnap *snap,
                              char name[NET_NAME_MAX + 1]) {
    return net_unpack_ex(in, len, type, player, seq, snap, name, NULL, NULL);
}

// (mp2) packets of a wrong NET_VERSION: is `in` an SN packet of another version (-> its version byte, else -1)
static inline int net_other_version(const uint8_t *in, int len) {
    return in && len >= 3 && in[0] == NET_MAGIC0 && in[1] == NET_MAGIC1 && in[2] != NET_VERSION ? in[2] : -1;
}
// the last few peer addresses already logged for it: true = first time for this ip (-> log it once)
typedef struct { uint32_t ip[8]; int n; } NetVerSeen;
static inline bool net_ver_first(NetVerSeen *t, uint32_t ip) {
    int cnt = t->n < 8 ? t->n : 8;
    for (int i = 0; i < cnt; i++) if (t->ip[i] == ip) return false;
    t->ip[t->n++ & 7] = ip;
    return true;
}

// serial-number arithmetic: a is newer than b (half the u16 range ahead)
static inline bool net_seq_newer(uint16_t a, uint16_t b) {
    uint16_t d = (uint16_t)(a - b);
    return d != 0 && d < 0x8000;
}

static inline bool net_ev_bit(const NetEvSeen *s, uint8_t q) { return s->seen[q >> 3] >> (q & 7) & 1; }
static inline void net_ev_set(NetEvSeen *s, uint8_t q, bool on) {
    if (on) s->seen[q >> 3] |= (uint8_t)(1 << (q & 7));
    else s->seen[q >> 3] &= (uint8_t)~(1 << (q & 7));
}

// true exactly once per event seq: ahead of the newest = new (the seqs skipped become "not played yet"),
// up to 127 behind = new if not played yet, 128 = too old
static inline bool net_event_fresh(NetEvSeen *s, uint8_t q) {
    if (!s->any) {
        memset(s->seen, 0, sizeof s->seen);
        s->any = 1, s->last = q;
        net_ev_set(s, q, true);
        return true;
    }
    uint8_t ahead = (uint8_t)(q - s->last);
    if (ahead == 0) return false;
    if (ahead < 128) {
        for (uint8_t k = (uint8_t)(s->last + 1); k != q; k++) net_ev_set(s, k, false);
        net_ev_set(s, q, true);
        s->last = q;
        return true;
    }
    if (ahead == 128) return false;
    if (net_ev_bit(s, q)) return false;
    net_ev_set(s, q, true);
    return true;
}

// ---- (s166 v6) NET_EDMG: shared enemy HP. Hits the sender's game saw on placed enemies (mubin HashId), as damage DELTAS ----
// Packet = hdr (player, seq = this packet kind's OWN u16 counter) + n x 12 B (n = (len - 7) / 12, 1..16, no count byte):
//   u32 hid, u16 dealt (clamped 65535), u16 after (the sender's life after, 0 = dead), u8 f54, u8 f40, u8 flags, u8 pad.
// Each packet is sent NET_EVENT_REPEAT times with the SAME seq; the receiver de-dupes per peer with a 16-seq window.
#define NET_EDMG_PKT (NET_HDR + NET_EDMG_MAX * NET_EDMG_ENT)  // 199
enum { NEDF_KILL = 1, NEDF_SORA = 2 };  // (SORA: reserved, the hit came from Sora; 0 for now)
typedef struct { uint32_t hid; uint16_t dealt, after; uint8_t f54, f40, flags; } NetEdEnt;

static inline void net_w32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }
static inline uint32_t net_r32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static inline int net_pack_edmg(uint8_t player, uint16_t seq, const NetEdEnt *e, int n, uint8_t *out, int cap) {
    if (!e || n < 1 || n > NET_EDMG_MAX) return -1;
    int len = NET_HDR + n * NET_EDMG_ENT;
    if (cap < len) return -1;
    net_hdr(out, NET_EDMG, player, seq);
    uint8_t *p = out + NET_HDR;
    for (int i = 0; i < n; i++, p += NET_EDMG_ENT) {
        net_w32(p, e[i].hid), net_w16(p + 4, e[i].dealt), net_w16(p + 6, e[i].after);
        p[8] = e[i].f54, p[9] = e[i].f40, p[10] = e[i].flags, p[11] = 0;
    }
    return len;
}
// -> the entry count (0 = not a valid NET_EDMG packet); entries with hid 0 are dropped (never valid on the wire)
static inline int net_unpack_edmg(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetEdEnt out[NET_EDMG_MAX]) {
    if (!in || len < NET_HDR + NET_EDMG_ENT || in[0] != NET_MAGIC0 || in[1] != NET_MAGIC1 || in[2] != NET_VERSION || in[3] != NET_EDMG) return 0;
    if ((len - NET_HDR) % NET_EDMG_ENT || (len - NET_HDR) / NET_EDMG_ENT > NET_EDMG_MAX) return 0;
    int n = (len - NET_HDR) / NET_EDMG_ENT, k = 0;
    const uint8_t *p = in + NET_HDR;
    for (int i = 0; i < n; i++, p += NET_EDMG_ENT) {
        NetEdEnt *e = &out[k];
        e->hid = net_r32(p), e->dealt = net_r16(p + 4), e->after = net_r16(p + 6), e->f54 = p[8], e->f40 = p[9], e->flags = p[10];
        if (e->hid) k++;
    }
    if (player) *player = in[4];
    if (seq) *seq = net_r16(in + 5);
    return k;
}

// receiver, per peer: u16 packet seqs already applied = the newest + a bitmap of the 16 before it (bit i = seq last - 1 - i).
// true exactly once per seq while it is within 16 of the newest; older than that = dropped (the repeats ride a few ticks only).
typedef struct { uint16_t last, bits; uint8_t any; } NetEdSeen;
static inline bool net_edseq_fresh(NetEdSeen *s, uint16_t q) {
    if (!s->any) { s->any = 1, s->last = q, s->bits = 0; return true; }
    uint16_t d = (uint16_t)(q - s->last);
    if (d == 0) return false;
    if (d < 0x8000) {  // newer
        if (d <= 16) s->bits = (uint16_t)((d == 16 ? 0u : (uint32_t)s->bits << d) | (1u << (d - 1)));
        else s->bits = 0;
        s->last = q;
        return true;
    }
    uint16_t back = (uint16_t)(s->last - q);  // 1..32768 behind
    if (back > 256) { s->last = q, s->bits = 0; return true; }  // (s167 review SF-3: far behind = the peer restarted net, its BYE lost; a real repeat is <~20 behind)
    if (back > 16) return false;
    if (s->bits >> (back - 1) & 1) return false;
    s->bits |= (uint16_t)(1u << (back - 1));
    return true;
}

// sender: finished packets waiting for their repeats (each goes out NET_EVENT_REPEAT times, NET_EDMG_REP_MS apart)
#define NET_EDMG_REP_MS 40
#define NET_EDMG_OUTQ 8
typedef struct { uint8_t b[NET_EDMG_PKT]; int len, rep; uint64_t next_ms; } NetEdOut;
// queue a packet (the oldest is dropped when the queue is full: its remaining repeats are lost); -> 1 if one was dropped
static inline int net_edout_push(NetEdOut *q, int *nq, const uint8_t *pkt, int len, uint64_t now) {
    int dropped = 0;
    if (*nq == NET_EDMG_OUTQ) { memmove(q, q + 1, sizeof *q * (NET_EDMG_OUTQ - 1)), (*nq)--, dropped = 1; }
    NetEdOut *o = &q[(*nq)++];
    memcpy(o->b, pkt, (size_t)len), o->len = len, o->rep = NET_EVENT_REPEAT, o->next_ms = now;
    return dropped;
}
// the next packet due at `now` (its repeat counted off, the next one NET_EDMG_REP_MS later; a finished packet leaves the queue): index into
// the copy `out`, 0 = none due. Call until it returns 0.
static inline int net_edout_due(NetEdOut *q, int *nq, uint64_t now, NetEdOut *out) {
    for (int i = 0; i < *nq; i++)
        if (q[i].rep > 0 && q[i].next_ms <= now) {
            *out = q[i];
            q[i].rep--, q[i].next_ms = now + NET_EDMG_REP_MS;
            if (!q[i].rep) { memmove(q + i, q + i + 1, sizeof *q * (size_t)(*nq - i - 1)), (*nq)--; }
            return 1;
        }
    return 0;
}

// ---- (s168 v7) NET_ESYNC: enemies act the same on every game. The owner (nearest player) sends its enemies' state at 20 Hz. ----
// Packet = hdr (player, seq = this kind's OWN u16 counter, NOT repeated: newest state wins) + u8 n + u8 m + n x 40 B state + m x 26 B sensor event:
//   state: u32 hid, f32 pos[3], f32 yaw, u32 anim_hash[2], u8 bank[2], u8 chg[2] (anim restart counters), f32 frame (0 = unknown), u16 action (opaque), u16 flags (bit0 sensor on, bit1 dead)
//   event: u8 evseq, u8 on (0/1), u32 hid, u32 sensor name hash (0 = all, off only), u32 params[4] (opaque); each event rides NET_EVENT_REPEAT packets
// n <= 8, m <= 6, n + m >= 1, length exact: 7 + 2 + 40n + 26m <= 485.
#define NET_ESYNC_PKT (NET_HDR + 2 + NET_ESYNC_MAX * NET_ESYNC_ENT + NET_ESYNC_EVMAX * NET_ESYNC_EV)  // 485
enum { NESF_SENSOR = 1, NESF_DEAD = 2 };
typedef struct {
    uint32_t hid; float pos[3], yaw; uint32_t anim_hash[2]; uint8_t anim_bank[2], anim_chg[2]; float frame; uint16_t action, flags;
} NetEsEnt;  // = the exe interface's state struct too
typedef struct { uint8_t seq, on; uint32_t hid, name, params[4]; } NetEsEv;
_Static_assert(sizeof(NetEsEnt) == NET_ESYNC_ENT, "NetEsEnt is the 40 B wire entry");

#define net_rf32 net_rf

static inline int net_pack_esync(uint8_t player, uint16_t seq, const NetEsEnt *e, int n, const NetEsEv *v, int m, uint8_t *out, int cap) {
    if (n < 0 || m < 0 || n > NET_ESYNC_MAX || m > NET_ESYNC_EVMAX || n + m < 1 || (n && !e) || (m && !v)) return -1;
    int len = NET_HDR + 2 + n * NET_ESYNC_ENT + m * NET_ESYNC_EV;
    if (cap < len) return -1;
    net_hdr(out, NET_ESYNC, player, seq);
    out[NET_HDR] = (uint8_t)n, out[NET_HDR + 1] = (uint8_t)m;
    uint8_t *p = out + NET_HDR + 2;
    for (int i = 0; i < n; i++, p += NET_ESYNC_ENT) {
        net_w32(p, e[i].hid);
        for (int k = 0; k < 3; k++) net_wf(p + 4 + 4 * k, e[i].pos[k]);
        net_wf(p + 16, e[i].yaw), net_w32(p + 20, e[i].anim_hash[0]), net_w32(p + 24, e[i].anim_hash[1]);
        p[28] = e[i].anim_bank[0], p[29] = e[i].anim_bank[1], p[30] = e[i].anim_chg[0], p[31] = e[i].anim_chg[1];
        net_wf(p + 32, e[i].frame), net_w16(p + 36, e[i].action), net_w16(p + 38, e[i].flags);
    }
    for (int i = 0; i < m; i++, p += NET_ESYNC_EV) {
        p[0] = v[i].seq, p[1] = v[i].on, net_w32(p + 2, v[i].hid), net_w32(p + 6, v[i].name);
        for (int k = 0; k < 4; k++) net_w32(p + 10 + 4 * k, v[i].params[k]);
    }
    return len;
}
static inline bool net_es_fin(float f) { return isfinite(f) && fabsf(f) <= 1e6f; }
// -> true = a structurally valid NET_ESYNC packet; *ne / *nv = the entries that survived (hid 0, non-finite / huge numbers, on > 1 are dropped alone)
static inline bool net_unpack_esync(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetEsEnt e[NET_ESYNC_MAX], int *ne, NetEsEv v[NET_ESYNC_EVMAX], int *nv) {
    if (ne) *ne = 0;
    if (nv) *nv = 0;
    if (!in || len < NET_HDR + 2 || in[0] != NET_MAGIC0 || in[1] != NET_MAGIC1 || in[2] != NET_VERSION || in[3] != NET_ESYNC) return false;
    int n = in[NET_HDR], m = in[NET_HDR + 1];
    if (n > NET_ESYNC_MAX || m > NET_ESYNC_EVMAX || n + m < 1 || len != NET_HDR + 2 + n * NET_ESYNC_ENT + m * NET_ESYNC_EV) return false;
    const uint8_t *p = in + NET_HDR + 2;
    int k = 0;
    for (int i = 0; i < n; i++, p += NET_ESYNC_ENT) {
        NetEsEnt *o = &e[k];
        o->hid = net_r32(p);
        for (int j = 0; j < 3; j++) o->pos[j] = net_rf32(p + 4 + 4 * j);
        o->yaw = net_rf32(p + 16), o->anim_hash[0] = net_r32(p + 20), o->anim_hash[1] = net_r32(p + 24);
        o->anim_bank[0] = p[28], o->anim_bank[1] = p[29], o->anim_chg[0] = p[30], o->anim_chg[1] = p[31];
        o->frame = net_rf32(p + 32), o->action = net_r16(p + 36), o->flags = net_r16(p + 38);
        if (o->hid && net_es_fin(o->pos[0]) && net_es_fin(o->pos[1]) && net_es_fin(o->pos[2]) && net_es_fin(o->yaw) && net_es_fin(o->frame)) k++;
    }
    int kv = 0;
    for (int i = 0; i < m; i++, p += NET_ESYNC_EV) {
        NetEsEv *o = &v[kv];
        o->seq = p[0], o->on = p[1], o->hid = net_r32(p + 2), o->name = net_r32(p + 6);
        for (int j = 0; j < 4; j++) o->params[j] = net_r32(p + 10 + 4 * j);
        if (o->hid && o->on <= 1) kv++;
    }
    if (ne) *ne = k;
    if (nv) *nv = kv;
    if (player) *player = in[4];
    if (seq) *seq = net_r16(in + 5);
    return true;
}

// receiver, per peer: ESYNC packets carry the newest state, so only a NEWER seq is applied (a repeat / reordered older one is dropped);
// a seq more than 256 behind = the peer restarted (BYE lost): taken as the new base
typedef struct { uint16_t last; uint8_t any; } NetEsSeen;
static inline bool net_esseq_fresh(NetEsSeen *s, uint16_t q) {
    if (!s->any) { s->any = 1, s->last = q; return true; }
    uint16_t d = (uint16_t)(q - s->last);
    if (d == 0) return false;
    if (d < 0x8000) { s->last = q; return true; }
    if ((uint16_t)(s->last - q) > 256) { s->last = q; return true; }
    return false;
}

// ---- (s176 v10) NET_FIRE: fire sync. A Fire tick the sender's Sora / Donald cast, so the other games' world burns too ----
// Packet = hdr (player, seq = this kind's OWN u16 counter) + n x 18 B (n = (len - 7) / 18, 1..4, no count byte):
//   f32 x, y, z (the caster's feet as passed to fire_world, BotW world units), u16 r, u16 h (1/100 u, clamped 65535), u8 flags, u8 pad.
//   flags: bit0 = last tick, bits1-2 = who (0 sora, 1 donald, 2 goofy / other), bit3 = bursts (the sender has fireworld 2).
// Sent NET_EVENT_REPEAT times with the SAME seq (net_edout_*); the receiver de-dupes per peer (net_edseq_fresh).
#define NET_FIRE_PKT (NET_HDR + NET_FIRE_MAX * NET_FIRE_ENT)  // 79
enum { NFRF_LAST = 1, NFRF_WHO_SHIFT = 1, NFRF_WHO_MASK = 3, NFRF_BURST = 8 };  // (s176 review: BURST = the sender has fireworld 2: its last tick bursts the grass)
typedef struct { float pos[3]; uint16_t r, h; uint8_t flags; } NetFireEnt;  // r, h in 1/100 u

static inline uint16_t net_fire_q(float u) { return (uint16_t)(!(u > 0) ? 0 : u >= 655.35f ? 65535 : (int)(u * 100.0f + 0.5f)); }
static inline void net_w32f(uint8_t *p, float f) { uint32_t u; memcpy(&u, &f, 4); net_w32(p, u); }
static inline float net_r32f(const uint8_t *p) { uint32_t u = net_r32(p); float f; memcpy(&f, &u, 4); return f; }
static inline int net_pack_fire(uint8_t player, uint16_t seq, const NetFireEnt *e, int n, uint8_t *out, int cap) {
    if (!e || n < 1 || n > NET_FIRE_MAX) return -1;
    int len = NET_HDR + n * NET_FIRE_ENT;
    if (cap < len) return -1;
    net_hdr(out, NET_FIRE, player, seq);
    uint8_t *p = out + NET_HDR;
    for (int i = 0; i < n; i++, p += NET_FIRE_ENT) {
        net_w32f(p, e[i].pos[0]), net_w32f(p + 4, e[i].pos[1]), net_w32f(p + 8, e[i].pos[2]);
        net_w16(p + 12, e[i].r), net_w16(p + 14, e[i].h), p[16] = e[i].flags, p[17] = 0;
    }
    return len;
}
// -> the entry count (0 = not a valid NET_FIRE packet); entries with a non-finite / huge (> 1e6) position are dropped alone
static inline int net_unpack_fire(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetFireEnt out[NET_FIRE_MAX]) {
    if (!in || len < NET_HDR + NET_FIRE_ENT || in[0] != NET_MAGIC0 || in[1] != NET_MAGIC1 || in[2] != NET_VERSION || in[3] != NET_FIRE) return 0;
    if ((len - NET_HDR) % NET_FIRE_ENT || (len - NET_HDR) / NET_FIRE_ENT > NET_FIRE_MAX) return 0;
    int n = (len - NET_HDR) / NET_FIRE_ENT, k = 0;
    const uint8_t *p = in + NET_HDR;
    for (int i = 0; i < n; i++, p += NET_FIRE_ENT) {
        NetFireEnt *e = &out[k];
        bool ok = true;
        for (int j = 0; j < 3; j++) {
            e->pos[j] = net_r32f(p + 4 * j);
            if (!(e->pos[j] > -1e6f && e->pos[j] < 1e6f)) ok = false;  // (also false for NaN)
        }
        e->r = net_r16(p + 12), e->h = net_r16(p + 14), e->flags = p[16];
        if (ok) k++;
    }
    if (player) *player = in[4];
    if (seq) *seq = net_r16(in + 5);
    return k;
}

// ---- (s191, Melty spec 4.1) NET_AREA: the map this player is on ("MainField", "Dungeon012", "RemainsWater"; "" = not known) ----
// Packet = hdr (player, seq = this kind's OWN u16 counter) + u8 n (0..NET_AREA_MAX) + n letters (no 0). A NEW type, NET_VERSION
// unchanged: a 0.1.1-0.1.4 game (NET v11) finds no such type in net_unpack_ex, counts the packet "bad" and goes on (no log: the
// version byte is its own, no slot touched; recon/s191/S191_ESAREA.md task 2). Sent on every change, on a join and every
// NET_AREA_MS (Melty: NET_AREA_MELTY_MS); the receiver keeps the newest (net_esseq_fresh) and forgets it after 5 periods of silence.
#define NET_AREA_PKT (NET_HDR + 1 + NET_AREA_MAX)  // 39
static inline int net_pack_area(uint8_t player, uint16_t seq, const char *area, uint8_t *out, int cap) {
    int n = area ? (int)strlen(area) : 0;
    if (n > NET_AREA_MAX || !net_area_chars(area ? area : "", n) || cap < NET_HDR + 1 + n) return -1;
    net_hdr(out, NET_AREA, player, seq);
    out[NET_HDR] = (uint8_t)n;
    if (n) memcpy(out + NET_HDR + 1, area, (size_t)n);
    return NET_HDR + 1 + n;
}
// -> true = a valid NET_AREA packet; out = its text with a 0 (cap NET_AREA_MAX + 1)
static inline bool net_unpack_area(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, char out[NET_AREA_MAX + 1]) {
    int t = 0;
    if (!in || len < NET_HDR + 1 || in[3] != NET_AREA || !net_unpack_ex(in, len, &t, player, seq, NULL, NULL, NULL, NULL)) return false;
    memcpy(out, in + NET_HDR + 1, in[NET_HDR]), out[in[NET_HDR]] = 0;
    return true;
}
// two players are in different areas only when both say where they are and the texts differ ("" = not known = one world)
static inline bool net_area_apart_txt(const char *a, const char *b) { return a && b && a[0] && b[0] && strcmp(a, b) != 0; }

// ---- (s194) NET_REACT: "play this KH2 hit reaction on your enemy" (recon/s194/S194_REACT_FACTS.md). Sora hit an enemy that is a
// PUPPET on his game (another game owns it, ESYNC): instead of starting the KH2 slide / hop / blow there (it fought the puppet apply
// = the jitter) he sends what his ah_hit would have used, and the OWNER runs that ah_hit; its pose stream carries the result.
// Packet = hdr (player, seq = this kind's OWN u16 counter) + n x 24 B (n = (len - 7) / 24, 1..8, no count byte):
//   u32 hid, u8 react (atkp EnemyReaction), u8 kind (NRK_*: ah_hit's `what`), i16 kb1, i16 kb2 (atkp KnockbackStrength 1 / 2),
//   u8 f54 (the BotW damage type the hit was sent with), u8 flags (NRF_FORM: the attacker is in a Drive form), f32 from[3] (the attack
//   point, world units: Sora's feet / the spell / the shot). A NEW type, NET_VERSION unchanged: a 0.1.1-0.1.4 game (NET v11) finds no
//   such type in net_unpack_ex, counts the packet "bad" and goes on (no log). Sent NET_EVENT_REPEAT times with the SAME seq over UDP
//   (net_edout_*), once in a Melty room; the receiver de-dupes per peer (net_edseq_fresh).
#define NET_REACT_PKT (NET_HDR + NET_REACT_MAX * NET_REACT_ENT)  // 199
enum { NRK_BLADE = 0, NRK_THUNDER = 1, NRK_OTHER = 2, NRK_N = 3 };  // (ah_hit tells only "blade" (the grounded slide) and "thunder" (the apex hang) apart)
enum { NRF_FORM = 1 };
#define NET_REACT_KB_MAX 1000      // |kb| above this is refused (garbage), except the 0x7fff "no row" default
// (s197 review S5) what is accepted is clamped into the span of Sora's atkp KnockbackStrength rows (sora_airhold.h AH_ATKP:
// -100 (the Master Form slams) .. 350 (atkp 968)): a peer can no longer fling an enemy we own harder than any real hit would
#define NET_REACT_KB_LO (-100)
#define NET_REACT_KB_HI 350
#define NET_REACT_KB_DEFAULT 0x7fff
typedef struct { uint32_t hid; uint8_t react, kind, f54, flags; int16_t kb1, kb2; float from[3]; } NetReactEnt;

// re:0x3CE980 (VERIFIED decompile, airhold_kh2a.c): atkp EnemyReaction -> KH2 reaction class (sora_airhold.h ah_class uses this one)
static inline int net_react_class(int r) {
    switch (r & 0xff) {
    case 1: case 0xc: return 1;
    case 3: case 6: case 8: case 10: case 0xd: case 0x10: case 0x12: return 3;
    case 4: case 7: case 9: case 0xb: case 0xf: case 0x11: return 4;
    default: return r & 0xff;
    }
}
// ah_hit only starts something for class 3 (hop / slide) and class 4 (blow): any other reaction is never sent
static inline bool net_react_plays(int r) { int c = net_react_class(r); return c == 3 || c == 4; }
// ah_hit's `what` <-> the wire kind; the strings handed back are the SAME literals ah_hit compares against (static: AhEnt keeps them)
static inline int net_react_kind(const char *what) { return !what ? NRK_OTHER : !strcmp(what, "blade") ? NRK_BLADE : !strcmp(what, "thunder") ? NRK_THUNDER : NRK_OTHER; }
static inline const char *net_react_what(int kind) { return kind == NRK_BLADE ? "blade" : kind == NRK_THUNDER ? "thunder" : "other"; }
// (s197 S5) the sender clamps into the table span too (an honest one never has to: every AH_ATKP row is inside it)
static inline int16_t net_react_kb(int kb) { return (int16_t)(kb == NET_REACT_KB_DEFAULT ? kb : kb > NET_REACT_KB_HI ? NET_REACT_KB_HI : kb < NET_REACT_KB_LO ? NET_REACT_KB_LO : kb); }
static inline bool net_react_kb_ok(int kb) { return kb == NET_REACT_KB_DEFAULT || (kb >= -NET_REACT_KB_MAX && kb <= NET_REACT_KB_MAX); }
// one entry is good: hid set, a known kind, a reaction ah_hit plays, kb in range, a finite attack point (< 1e6 u)
static inline bool net_react_ent_ok(const NetReactEnt *e) {
    if (!e->hid || e->kind >= NRK_N || !net_react_plays(e->react) || !net_react_kb_ok(e->kb1) || !net_react_kb_ok(e->kb2)) return false;
    for (int j = 0; j < 3; j++) if (!(e->from[j] > -1e6f && e->from[j] < 1e6f)) return false;  // (also false for NaN)
    return true;
}

static inline int net_pack_react(uint8_t player, uint16_t seq, const NetReactEnt *e, int n, uint8_t *out, int cap) {
    if (!e || n < 1 || n > NET_REACT_MAX) return -1;
    int len = NET_HDR + n * NET_REACT_ENT;
    if (cap < len) return -1;
    net_hdr(out, NET_REACT, player, seq);
    uint8_t *p = out + NET_HDR;
    for (int i = 0; i < n; i++, p += NET_REACT_ENT) {
        net_w32(p, e[i].hid), p[4] = e[i].react, p[5] = e[i].kind;
        net_w16(p + 6, (uint16_t)e[i].kb1), net_w16(p + 8, (uint16_t)e[i].kb2), p[10] = e[i].f54, p[11] = e[i].flags;
        for (int j = 0; j < 3; j++) net_w32f(p + 12 + 4 * j, e[i].from[j]);
    }
    return len;
}
// -> the entry count (0 = not a valid NET_REACT packet, or no good entry in it); a bad entry (net_react_ent_ok) is dropped alone,
// *bad (optional) = how many
static inline int net_unpack_react(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetReactEnt out[NET_REACT_MAX], int *bad) {
    if (bad) *bad = 0;
    if (!in || len < NET_HDR + NET_REACT_ENT || in[0] != NET_MAGIC0 || in[1] != NET_MAGIC1 || in[2] != NET_VERSION || in[3] != NET_REACT) return 0;
    if ((len - NET_HDR) % NET_REACT_ENT || (len - NET_HDR) / NET_REACT_ENT > NET_REACT_MAX) return 0;
    int n = (len - NET_HDR) / NET_REACT_ENT, k = 0;
    const uint8_t *p = in + NET_HDR;
    for (int i = 0; i < n; i++, p += NET_REACT_ENT) {
        NetReactEnt *e = &out[k];
        e->hid = net_r32(p), e->react = p[4], e->kind = p[5];
        e->kb1 = (int16_t)net_r16(p + 6), e->kb2 = (int16_t)net_r16(p + 8), e->f54 = p[10], e->flags = p[11];
        for (int j = 0; j < 3; j++) e->from[j] = net_r32f(p + 12 + 4 * j);
        if (net_react_ent_ok(e)) e->kb1 = net_react_kb(e->kb1), e->kb2 = net_react_kb(e->kb2), k++;  // (s197 S5: into -100..350)
        else if (bad) (*bad)++;
    }
    if (player) *player = in[4];
    if (seq) *seq = net_r16(in + 5);
    return k;
}

// ---- (s196 killsync) NET_KILLS: "which placed enemies did you see die since your last Blood Moon?" (sora_net_kills.h) ----
// A game that loads a save, or sees a new peer, ASKS each peer for his kill list; the peer ANSWERS with PARTS of up to 106 kills.
// (s197 review M1) Every kill carries its AGE, and the ask says how long ago the asker's own last Blood Moon was: over plain UDP
// each game has its own Blood Moons, so a kill from before MY Blood Moon must never come back to me (its enemy respawned here).
// Packet = hdr (player, seq = this kind's OWN u16 counter, not used to de-dupe: the nonce + part bitmap do that) + u8 op + body:
//   op 1 ASK  (len 24): u32 nonce (the asker's, never 0), u16 tag (the answer snapshot the bitmap is for, 0 = none yet),
//                       u64 have (bit i = part i of that snapshot already here; 0 = send all),
//                       u16 moon (s197: seconds since the asker's last Blood Moon, saturating at 0xFFFE; 0xFFFF = NET_KILLS_MOON_NONE
//                       = none seen in this session (a fresh start / join / DLL hot reload) = send every kill)
//   op 2 PART (len 17 + 6k): u32 nonce (the ask's), u16 tag (the answerer's snapshot, never 0), u8 i (0..n-1), u8 n (1..64),
//                       u8 k (0..106), k x { u32 hid (map HashId; a 0 is dropped alone), u16 age (s197: seconds since that kill,
//                       saturating: 0xFFFF = NET_KILLS_AGE_OLD = that old or older / not known) }. An empty list = one part, k 0.
//   The answerer sends a kill only when it is YOUNGER than the asker's moon (sora_net_kills.h ks_cut); an OLD age only when the
//   asker saw no moon. The asker drops what is not younger than its own moon again on receive (an answer started before it).
//   Ages are relative, so the two clocks never have to agree; a forwarded kill keeps its true age (game C hears B's catch-up
//   kills from A with A's age, plus the transit, never "received now").
// A NEW type, NET_VERSION unchanged: a 0.1.1-0.1.4 / 7be3a1d game (NET v11) finds no such type in net_unpack_ex, counts the
// packet "bad" and goes on (no log: the version byte is its own, no slot touched). A b020355 (s196/s197 killsync, never shipped)
// game refuses the new lengths as "bad" too. UDP: an ASK is re-sent until every part is in (or a timeout), each part goes once
// per ask; in a Melty room (reliable events) the same, the re-ask is only a late safety.
enum { NKO_ASK = 1, NKO_PART = 2 };
#define NET_KILLS_PKT (NET_KILLS_PART_HDR + NET_KILLS_ENT * NET_KILLS_CHUNK)  // 653
#define NET_KILLS_AGE_OLD 0xFFFF   // a part's age: 18.2 h or more / not known
#define NET_KILLS_MOON_NONE 0xFFFF // an ask's moon: no Blood Moon seen in this session (the largest real value is 0xFFFE)
typedef struct { uint8_t op, i, n, k; uint32_t nonce; uint16_t tag, moon; uint64_t have; uint32_t hid[NET_KILLS_CHUNK]; uint16_t age[NET_KILLS_CHUNK]; } NetKillsMsg;

static inline int net_pack_kills_ask(uint8_t player, uint16_t seq, uint32_t nonce, uint16_t tag, uint64_t have, uint16_t moon, uint8_t *out, int cap) {
    if (!out || !nonce || cap < NET_KILLS_ASK_LEN) return -1;
    net_hdr(out, NET_KILLS, player, seq);
    uint8_t *p = out + NET_HDR;
    p[0] = NKO_ASK, net_w32(p + 1, nonce), net_w16(p + 5, tag), net_w32(p + 7, (uint32_t)have), net_w32(p + 11, (uint32_t)(have >> 32));
    net_w16(p + 15, moon);
    return NET_KILLS_ASK_LEN;
}
// age NULL = every kill NET_KILLS_AGE_OLD
static inline int net_pack_kills_part(uint8_t player, uint16_t seq, uint32_t nonce, uint16_t tag, int i, int n, const uint32_t *hid,
                                      const uint16_t *age, int k, uint8_t *out, int cap) {
    if (!out || !nonce || !tag || n < 1 || n > NET_KILLS_CHUNKS_MAX || i < 0 || i >= n || k < 0 || k > NET_KILLS_CHUNK || (k && !hid)) return -1;
    int len = NET_KILLS_PART_HDR + NET_KILLS_ENT * k;
    if (cap < len) return -1;
    net_hdr(out, NET_KILLS, player, seq);
    uint8_t *p = out + NET_HDR;
    p[0] = NKO_PART, net_w32(p + 1, nonce), net_w16(p + 5, tag), p[7] = (uint8_t)i, p[8] = (uint8_t)n, p[9] = (uint8_t)k;
    for (int j = 0; j < k; j++) net_w32(p + 10 + NET_KILLS_ENT * j, hid[j]), net_w16(p + 14 + NET_KILLS_ENT * j, age ? age[j] : NET_KILLS_AGE_OLD);
    return len;
}
// -> true = a valid NET_KILLS packet (nonce 0, or a part's tag 0 = not valid); a part's hid 0 entries are dropped (m->k = the rest)
static inline bool net_unpack_kills(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetKillsMsg *m) {
    int t = 0;
    if (!in || !m || len < NET_HDR + 1 || in[3] != NET_KILLS || !net_unpack_ex(in, len, &t, player, seq, NULL, NULL, NULL, NULL)) return false;
    const uint8_t *p = in + NET_HDR;
    memset(m, 0, offsetof(NetKillsMsg, hid));
    m->op = p[0], m->nonce = net_r32(p + 1), m->tag = net_r16(p + 5);
    if (!m->nonce) return false;
    if (m->op == NKO_ASK) {
        m->have = (uint64_t)net_r32(p + 7) | (uint64_t)net_r32(p + 11) << 32;
        m->moon = net_r16(p + 15);
        return true;
    }
    if (!m->tag) return false;
    m->i = p[7], m->n = p[8];
    int k = 0;
    for (int j = 0; j < p[9]; j++) {
        uint32_t h = net_r32(p + 10 + NET_KILLS_ENT * j);
        if (h) m->hid[k] = h, m->age[k++] = net_r16(p + 14 + NET_KILLS_ENT * j);
    }
    m->k = (uint8_t)k;
    return true;
}

// (s197 review S3 / S5) a log line limiter for receive paths a peer can drive: at most NET_LOGRATE_BURST lines per NET_LOGRATE_MS
// window; the rest is only counted (quiet, shown in `net diag`). ok -> true = write the line now; *first_quiet (optional) = this is
// the first line refused in this window (the caller may say "more lines are not written" once).
#define NET_LOGRATE_MS 2000   // GUESS
#define NET_LOGRATE_BURST 8   // GUESS
typedef struct { uint64_t t0; uint32_t n, quiet; } NetLogRate;
static inline bool net_lograte_ok(NetLogRate *r, uint64_t now, bool *first_quiet) {
    if (first_quiet) *first_quiet = false;
    if (!r->t0 || now < r->t0 || now - r->t0 >= NET_LOGRATE_MS) r->t0 = now ? now : 1, r->n = 0;
    if (r->n < NET_LOGRATE_BURST) { r->n++; return true; }
    if (r->n == NET_LOGRATE_BURST && first_quiet) *first_quiet = true;
    r->n = NET_LOGRATE_BURST + 1, r->quiet++;
    return false;
}

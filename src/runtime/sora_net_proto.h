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
#define NET_VERSION 12  // (12: s267 co-op item 2, the party member's character in byte 27 of its record (Riku / Auron / ... drawn as themselves); 11: s182 sfx pack ids shift (Roxas run se000 #306) = 0.1.1, 2: log scale byte, 3: fx moves, Task 8, 4: party block + fx anchors, slice 3, 5: 12 events per SNAP + HELLO peer list, slice 4 mp4, 6: NET_EDMG shared enemy HP, s166, 7: NET_ESYNC enemy sync, s168, 8: NF_BONES bone-pose block, s170, 9: NF_SKATE skate rider, s172, 10: NET_FIRE fire sync, s176)
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

enum { NET_HELLO = 1, NET_SNAP = 2, NET_BYE = 3, NET_EDMG = 4, NET_ESYNC = 5, NET_FIRE = 6, NET_AREA = 7, NET_REACT = 8, NET_KILLS = 9, NET_WARP = 10, NET_DUEL = 11, NET_GEAR = 12 };  // (s191: AREA, s194: REACT, s196: KILLS, s269: WARP, s272: DUEL, s289: GEAR, no NET_VERSION change: see the end of this file)
enum { NET_LOOT = 13, NET_WORLD = 14 };  // (s289 loot: the room's loot list, layout at the end of this file; s290 networld: NET_WORLD = the room's world flags, `set networld`, layout after the NET_LOOT block; no NET_VERSION change)
enum { NET_OBJS = 17, NET_OBJEV = 18 };  // (s289 objs: map objects that move / break the same for everyone, sora_net_objs.h; layouts at the end of this file, no NET_VERSION change)
// (s269 warpcore, co-op item 4) NET_WARP sizes (the packet is at the end of this file; net_unpack_ex checks them)
enum { NWO_ASK = 1, NWO_SPOT = 2, NWO_NOTE = 3 };
#define NET_WARP_HEAD (NET_HDR + 6)        // 13: op, target player, u32 nonce (= ASK / NOTE)
#define NET_WARP_SPOT_LEN (NET_WARP_HEAD + 33)  // 46: + flags, live xyz, live deg, save xyz, save deg
// (s272 duel, co-op item 6) NET_DUEL sizes (the packet is at the end of this file; net_unpack_ex checks them)
enum { NDO_ASK = 1, NDO_ANSWER = 2, NDO_HIT = 3, NDO_END = 4, NDO_TIME = 5, NDO_RING = 6, NDO_PULL = 7 };
#define NET_DUEL_HEAD (NET_HDR + 6)               // 13: op, target player, u32 duel id (= ASK)
#define NET_DUEL_ANSWER_LEN (NET_DUEL_HEAD + 16)  // 29: + answer, why, centre xyz, radius cm
#define NET_DUEL_HIT_LEN (NET_DUEL_HEAD + 38)     // 51: + hseq, power, stat, flags, src, boost, react, pad, from xyz, at xyz, atkp
#define NET_DUEL_END_LEN (NET_DUEL_HEAD + 4)      // 17: + reason, result, hp permille
#define NET_DUEL_TIME_LEN (NET_DUEL_HEAD + 2)     // 15: + hp permille
#define NET_DUEL_RING_LEN (NET_DUEL_HEAD + 18)    // 31: + player a, player b, phase, pad, centre xyz, radius cm
#define NET_DUEL_PULL_LEN (NET_DUEL_HEAD + 24)    // 37: (s289) + hseq, centre xyz, radius cm, hold cm, swirl, frames, tier, pad
#define NET_REACT_ENT 24  // (s194, see the end of this file)
#define NET_REACT_MAX 8
#define NET_KILLS_CHUNK 106       // (s197) kills in one NET_KILLS part: 6 B each (u32 hid + u16 age), 17 + 636 = 653 <= the s196 657 (see the end of this file)
#define NET_KILLS_CHUNKS_MAX 64   // parts of one answer (a u64 bitmap)
#define NET_KILLS_ENT 6           // (s197) one kill in a part: u32 hid, u16 age (s)
#define NET_KILLS_ASK_LEN (NET_HDR + 17)   // 24 (s197: + u16 moon age)
#define NET_KILLS_PART_HDR (NET_HDR + 10)  // 17
enum { NF_WEAPON = 1, NF_AIR = 2, NF_GHOST = 4, NF_GUARD = 8, NF_DEAD = 16, NF_LOADING = 32, NF_BONEPOSE = 64, NF_BONES = 128, NF_SKATE = 256, NF_DOWNED = 512, NF_REVIVING = 1024, NF_DUEL = 2048 };  // (s272 duel: NF_DUEL = he is in a duel (countdown / fight); no NET_VERSION change: an older game copies the flags raw and tests no bit above 256; s289 revive: NF_DOWNED = he is DOWNED (sora_revive.h, a friend standing by revives him); NF_REVIVING kept for a button revive (design R2), never sent)
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
    uint8_t who, flags;           // who = the party SLOT: 0 Donald's place, 1 Goofy's; flags NPM_*
    uint8_t chr;                  // (v12, s267) who stands there: sora_pchar.h PCH_* + 1; 0 = the slot's own member (Donald / Goofy)
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
#define NET_OUTFITS 8        // (mp4) outfit ids 0 Normal .. 7 (sora_outfit.h OF_N; s263: 5 Roxas, 6 Kairi, 7 Green Tunic); a byte >= this reads as 0
                             // (an older peer, NET_OUTFITS 5, draws a 5..7 sender in his Normal body)
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
        p[26] = (uint8_t)(b * 255.0f + 0.5f), p[27] = m->chr, p[28] = 0;  // (27: v12 character; 28 reserved)
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
// (s289 objs) NET_OBJS / NET_OBJEV sizes and their body check (used by net_unpack_ex; the packets are at the end of this file)
#define NET_OBJS_N 12                                          // records in one NET_OBJS
#define NET_OBJS_REC 58                                        // u8 kind, u8 flags, u32 h, then f32 m[12] + u32 extra (kind 1) or a pose + AS name (2, 3)
#define NET_OBJS_PKT (NET_HDR + 1 + NET_OBJS_N * NET_OBJS_REC)  // 704
#define NET_OBJEV_LEN (NET_HDR + 22)                           // 29
_Static_assert(NET_OBJS_PKT == 704 && NET_OBJS_PKT <= NET_PKT_MAX, "s289 objs: a full NET_OBJS must fit NET_PKT_MAX");
static inline bool net_objs_body_ok(int t, const uint8_t *p, int n) {
    if (!p) return false;
    if (t == NET_OBJEV) return n == NET_OBJEV_LEN - NET_HDR && (p[0] == 1 || p[0] == 2) && (p[1] == 1 || p[1] == 2);
    return n >= 1 && p[0] >= 1 && p[0] <= NET_OBJS_N && n == 1 + p[0] * NET_OBJS_REC;
}
// (s289 netgear) NET_GEAR sizes and its body check (used by net_unpack_ex; the packet itself is right after NET_AREA's)
#define NET_GEAR_N 4          // entries in one packet (0 = nothing equipped / in his hands)
#define NET_GEAR_NAME_MAX 31  // an actor name ("Weapon_Sword_001"), net_area_chars' letters only
// p / n = the bytes after the header: u8 count (<= NET_GEAR_N), then per entry u8 kind, u8 flags, u8 len (1..31), len letters;
// nothing after the last entry
static inline bool net_gear_body_ok(const uint8_t *p, int n) {
    if (!p || n < 1 || p[0] > NET_GEAR_N) return false;
    int at = 1;
    for (int i = 0; i < p[0]; i++) {
        if (at + 3 > n) return false;
        int l = p[at + 2];
        if (l < 1 || l > NET_GEAR_NAME_MAX || at + 3 + l > n || !net_area_chars((const char *)p + at + 3, l)) return false;
        at += 3 + l;
    }
    return at == n;
}
// (s289 shots) NET_SHOTS / NET_RUNES (sora_net_shots.h): the types here for net_unpack_ex, the layouts at the END of this file
enum { NET_SHOTS = 15, NET_RUNES = 16 };
static inline bool net_shots_body_ok(int t, const uint8_t *p, int n);

// false on bad magic/version/type/size; snap filled for NET_SNAP, name for NET_HELLO
// (mp4) pa[NET_HELLO_PEERS] / *npa get a HELLO's peer list (entries with ip 0 or port 0 are dropped); NULL = not wanted
static inline bool net_loot_body_ok(const uint8_t *p, int n);  // (s289 loot: NET_LOOT's op / counts / names / exact size, at the end of this file)
static inline bool net_world_body_ok(const uint8_t *p, int n);  // (s290 networld: NET_WORLD's op / counts / pages / exact size, after the NET_LOOT block)
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
                m->chr = q[27];  // (v12: the receiver checks the id against its own character table)
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
    } else if (t == NET_GEAR) {  // (s289) the entries are read by net_unpack_gear; here the count, each name's letters and the exact size
        if (!net_gear_body_ok(p, len - NET_HDR)) return false;
    } else if (t == NET_SHOTS || t == NET_RUNES) {  // (s289 shots) read by net_unpack_shots / _runes; here counts, kinds, finite floats, exact size
        if (!net_shots_body_ok(t, p, len - NET_HDR)) return false;
    } else if (t == NET_OBJS || t == NET_OBJEV) {  // (s289 objs) read by net_unpack_objs / net_unpack_objev; here the count / op / kind and the exact size
        if (!net_objs_body_ok(t, p, len - NET_HDR)) return false;
    } else if (t == NET_REACT) {  // (s194) the entries are read by net_unpack_react; here only the size is checked
        if (len < NET_HDR + NET_REACT_ENT || (len - NET_HDR) % NET_REACT_ENT || (len - NET_HDR) / NET_REACT_ENT > NET_REACT_MAX) return false;
    } else if (t == NET_KILLS) {  // (s196) the body is read by net_unpack_kills; here the op, the exact size and the part numbers are checked
        if (len < NET_HDR + 1) return false;
        if (p[0] == 1) { if (len != NET_KILLS_ASK_LEN) return false; }  // (NKO_ASK)
        else if (p[0] == 2) {  // (NKO_PART: i < n <= 64, k <= 106, length exact)
            if (len < NET_KILLS_PART_HDR || p[8] < 1 || p[8] > NET_KILLS_CHUNKS_MAX || p[7] >= p[8] || p[9] > NET_KILLS_CHUNK || len != NET_KILLS_PART_HDR + NET_KILLS_ENT * p[9]) return false;
        } else return false;
    } else if (t == NET_WARP) {  // (s269) the body is read by net_unpack_warp; here the op and the exact size are checked
        if (len < NET_WARP_HEAD) return false;
        if (p[0] == NWO_ASK || p[0] == NWO_NOTE) { if (len != NET_WARP_HEAD) return false; }
        else if (p[0] == NWO_SPOT) { if (len != NET_WARP_SPOT_LEN) return false; }
        else return false;
    } else if (t == NET_DUEL) {  // (s272) the body is read by net_unpack_duel; here the op and the exact size are checked
        static const uint8_t DL[8] = {0, NET_DUEL_HEAD, NET_DUEL_ANSWER_LEN, NET_DUEL_HIT_LEN, NET_DUEL_END_LEN, NET_DUEL_TIME_LEN, NET_DUEL_RING_LEN,
                                      NET_DUEL_PULL_LEN};
        if (len < NET_DUEL_HEAD || p[0] < NDO_ASK || p[0] > NDO_PULL || len != DL[p[0]]) return false;
    } else if (t == NET_LOOT) {  // (s289 loot) the entries are read by net_unpack_loot; here the op, the counts, the names and the exact size
        if (!net_loot_body_ok(p, len - NET_HDR)) return false;
    } else if (t == NET_WORLD) {  // (s290 networld) read by net_unpack_world; here the op, the counts, the pages and the exact size
        if (!net_world_body_ok(p, len - NET_HDR)) return false;
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

// ---- (s289 netgear) NET_GEAR: what this player has in his hands, so the other games draw it on his puppet (sora_net_gear.h) ----
// Packet = hdr (player, seq = this kind's OWN u16 counter) + u8 n (0..NET_GEAR_N) + n x {u8 kind, u8 flags, u8 len (1..31), len
// letters (no 0)}. kind NGK_MELEE = his BotW melee weapon (the actor name, "Weapon_Sword_001"); NGK_SHIELD / NGK_BOW are kept for
// later (no build sends them yet); a receiver skips any kind it does not draw. flags NGF_HAND = in his hand now (else equipped
// and put away). A NEW type, NET_VERSION unchanged: a game without it (0.2.3 and older) finds no such type in net_unpack_ex,
// counts the packet "bad" and goes on (no log: the version byte is its own, no slot touched), exactly as for AREA. Sent only
// while `set netgear 1`: on a change (NET_GEAR_GAP_MS apart at least), on a join and every NET_GEAR_MS (Melty:
// NET_GEAR_MELTY_MS); the receiver keeps the newest (net_esseq_fresh) and forgets it after NET_GEAR_FORGET quiet periods.
enum { NGK_MELEE = 0, NGK_SHIELD = 1, NGK_BOW = 2 };
enum { NGF_HAND = 1 };
typedef struct { uint8_t kind, flags; char name[NET_GEAR_NAME_MAX + 1]; } NetGearItem;
#define NET_GEAR_PKT (NET_HDR + 1 + NET_GEAR_N * (3 + NET_GEAR_NAME_MAX))  // 144
static inline int net_pack_gear(uint8_t player, uint16_t seq, const NetGearItem *it, int n, uint8_t *out, int cap) {
    if (n < 0 || n > NET_GEAR_N || (n && !it) || !out || cap < NET_HDR + 1) return -1;
    int at = NET_HDR + 1;
    for (int i = 0; i < n; i++) {
        int l = 0;
        while (l <= NET_GEAR_NAME_MAX && it[i].name[l]) l++;
        if (l < 1 || l > NET_GEAR_NAME_MAX || !net_area_chars(it[i].name, l) || at + 3 + l > cap) return -1;
        out[at] = it[i].kind, out[at + 1] = it[i].flags, out[at + 2] = (uint8_t)l;
        memcpy(out + at + 3, it[i].name, (size_t)l);
        at += 3 + l;
    }
    net_hdr(out, NET_GEAR, player, seq);
    out[NET_HDR] = (uint8_t)n;
    return at;
}
// -> true = a valid NET_GEAR packet; it[0..*n) = its entries (names with their 0)
static inline bool net_unpack_gear(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetGearItem it[NET_GEAR_N], int *n) {
    int t = 0;
    if (!in || !it || !n || len < NET_HDR + 1 || in[3] != NET_GEAR || !net_unpack_ex(in, len, &t, player, seq, NULL, NULL, NULL, NULL)) return false;
    const uint8_t *p = in + NET_HDR;
    int at = 1;
    *n = p[0];
    for (int i = 0; i < *n; i++) {
        int l = p[at + 2];
        it[i].kind = p[at], it[i].flags = p[at + 1];
        memcpy(it[i].name, p + at + 3, (size_t)l), it[i].name[l] = 0;
        at += 3 + l;
    }
    return true;
}

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

// ---- (s269 warpcore, co-op item 4) NET_WARP: "warp to a friend" with BotW's own fast travel (sora_net_warp.h, sora_warp.h) ----
// Packet = hdr (player, seq = this kind's OWN u16 counter) + u8 op + u8 target (the player id it is for; others drop it)
//          + u32 nonce (the warper's, never 0) + body:
//   op 1 ASK  (len 13): "where are you?"  (warper -> friend; re-sent until a SPOT with the same nonce comes back)
//   op 2 SPOT (len 46): the answer (friend -> warper): u8 flags (NWF_*), f32 live x y z (his feet, BotW world units),
//                       f32 live deg (his facing: Sora's yaw in degrees, BotW's RotY sense: GUESS that it is the same sense
//                       as PlayerSavePosAngleYDegree), f32 save x y z (his gdt PlayerSavePos = his last safe ground spot),
//                       f32 save deg (PlayerSavePosAngleYDegree)
//   op 3 NOTE (len 13): "I warped to you" (warper -> friend, once the game took the warp call): his game shows a small text
// A NEW type, NET_VERSION unchanged: an older game (no NET_WARP in its net_unpack_ex) counts the packet "bad" and goes on (no
// log, no slot touched), exactly as NET_AREA / NET_KILLS did for theirs; the warper then never gets a SPOT and gives up
// after its ask timeout ("no answer: older build?"). UDP: the ASK is repeated, SPOT / NOTE go once per ask / warp; in a Melty
// room every one is a reliable event.
enum { NWF_GROUND = 1, NWF_SAVE = 2, NWF_MAINFIELD = 4, NWF_DEAD = 8, NWF_LOADING = 16 };
typedef struct { uint8_t op, target; uint32_t nonce; uint8_t flags; float live[3], live_deg, save[3], save_deg; } NetWarpMsg;

static inline int net_pack_warp(uint8_t player, uint16_t seq, const NetWarpMsg *m, uint8_t *out, int cap) {
    if (!m || !out || !m->nonce || (m->op != NWO_ASK && m->op != NWO_SPOT && m->op != NWO_NOTE)) return -1;
    int len = m->op == NWO_SPOT ? NET_WARP_SPOT_LEN : NET_WARP_HEAD;
    if (cap < len) return -1;
    net_hdr(out, NET_WARP, player, seq);
    uint8_t *p = out + NET_HDR;
    p[0] = m->op, p[1] = m->target, net_w32(p + 2, m->nonce);
    if (m->op == NWO_SPOT) {
        p[6] = m->flags;
        for (int j = 0; j < 3; j++) net_w32f(p + 7 + 4 * j, m->live[j]), net_w32f(p + 23 + 4 * j, m->save[j]);
        net_w32f(p + 19, m->live_deg), net_w32f(p + 35, m->save_deg);
    }
    return len;
}
// -> true = a valid NET_WARP packet (nonce 0 = not valid). A SPOT's numbers are passed on as they came (the receiver checks them:
// sora_warp_core.h wsp_choose drops a NaN / out-of-range spot)
static inline bool net_unpack_warp(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetWarpMsg *m) {
    int t = 0;
    if (!in || !m || in[3] != NET_WARP || !net_unpack_ex(in, len, &t, player, seq, NULL, NULL, NULL, NULL)) return false;
    const uint8_t *p = in + NET_HDR;
    memset(m, 0, sizeof *m);
    m->op = p[0], m->target = p[1], m->nonce = net_r32(p + 2);
    if (!m->nonce) return false;
    if (m->op == NWO_SPOT) {
        m->flags = p[6];
        for (int j = 0; j < 3; j++) m->live[j] = net_r32f(p + 7 + 4 * j), m->save[j] = net_r32f(p + 23 + 4 * j);
        m->live_deg = net_r32f(p + 19), m->save_deg = net_r32f(p + 35);
    }
    return true;
}

// ---- (s272 duel, co-op item 6) NET_DUEL: two players' duel (sora_net_duel.h, sora_duel.h; pure parts sora_duel_core.h) ----
// Packet = hdr (player, seq = this kind's OWN u16 counter) + u8 op + u8 target (the player id it is for; RING: 0 = everyone)
//          + u32 duel id (the asker's nonce, never 0) + body:
//   op 1 ASK    (len 13): "duel?" (asker -> friend)
//   op 2 ANSWER (len 29): u8 answer (1 accept, 2 decline / no answer, 3 busy), u8 why (busy: DUG_*), f32 centre x y z (the ring,
//                         between the two at the accept, BotW units), u16 radius cm (friend -> asker)
//   op 3 HIT    (len 51): u16 hit seq, u16 Power (atkp / spell power, 100 = a plain hit), u16 S (the attacker's Str + Keyblade Str,
//                         or Mag + Keyblade Mag for a spell), u8 flags (1 spell, 2 finisher), u8 src (0 blade, 1 Fire, 2 Blizzard,
//                         3 Thunder; s289: 4 Keyblade throw, 6 Magnet, 7 Reflect's burst; RESERVED 5 BotW weapon throw: the list
//                         and the names are sora_duel_core.h DU_SRC_* / du_src_name; an older game takes any src as damage, its
//                         log calls src >= 3 "Thunder"), u16 boost % (Combo / element Boost, 0 none), u8 reaction class, u8 0, f32 from x y z (his
//                         feet), f32 at x y z (where it met), u16 atkp row (its hit sound; spells: their row) (attacker ->
//                         victim: the victim's game takes the damage)
//   op 4 END    (len 17): u8 reason (DUE_*), u8 result as the SENDER sees it (DUR_*), u16 his HP permille (either -> other)
//   op 5 TIME   (len 15): u16 HP permille at his time limit (either -> other)
//   op 6 RING   (len 31): u8 player a, u8 player b, u8 phase (0 gone, 1 countdown, 2 fight), u8 0, f32 centre x y z, u16 radius cm
//                         (each duelist -> every peer, every 2 s: a third player draws the ring)
//   op 7 PULL   (len 37): (s289 duelmagnet) u16 the hit seq of the Magnet HIT it follows, f32 centre x y z (the sphere's, BotW
//                         units), u16 sphere radius cm, u16 hold radius cm (rolled at the grab), i16 swirl (cm per KH frame), u16 KH
//                         frames left in the cast's pull window, u8 tier (0..2), u8 0 (attacker -> victim, right after that HIT;
//                         sora_duel_magnet.h). An older game counts it "bad" (net_unpack_ex's op range) and still takes the HIT.
// A NEW type, NET_VERSION unchanged (as NET_WARP): an older game counts it "bad" and goes on; the asker then gets no ANSWER and
// gives up after 12.5 s ("no answer: duels off there, or an older build"). UDP: ANSWER / HIT / END / TIME / PULL go DU_RESENDS
// more times with the same seq (the receiver drops the copies: per duel id / hit seq; PULL by its own seen list); in a Melty room
// every one is a reliable event.
typedef struct {
    uint8_t op, target;
    uint32_t id;
    uint8_t ans, why;                   // ANSWER
    float c[3];                         // ANSWER / RING: the ring's centre; PULL: the sphere's
    uint16_t radius_cm;                 // ANSWER / RING; PULL: the sphere's
    uint16_t hseq, power, stat, boost, atkp;  // HIT (PULL: hseq = its HIT's)
    uint8_t flags, src, react;          // HIT
    float from[3], at[3];               // HIT
    uint8_t reason, result;             // END
    uint16_t pm;                        // END / TIME
    uint8_t a, b, phase;                // RING
    uint16_t hold_cm, frames;           // (s289) PULL: the hold radius, the KH frames left in the window
    int16_t swirl;                      // PULL: cm per KH frame
    uint8_t tier;                       // PULL: 0..2
} NetDuelMsg;
enum { NDH_SPELL = 1, NDH_FINISH = 2 };
static const uint8_t NET_DUEL_LEN[8] = {0, NET_DUEL_HEAD, NET_DUEL_ANSWER_LEN, NET_DUEL_HIT_LEN, NET_DUEL_END_LEN, NET_DUEL_TIME_LEN, NET_DUEL_RING_LEN,
                                        NET_DUEL_PULL_LEN};
static inline int net_pack_duel(uint8_t player, uint16_t seq, const NetDuelMsg *m, uint8_t *out, int cap) {
    if (!m || !out || !m->id || m->op < NDO_ASK || m->op > NDO_PULL) return -1;
    int len = NET_DUEL_LEN[m->op];
    if (cap < len) return -1;
    net_hdr(out, NET_DUEL, player, seq);
    uint8_t *p = out + NET_HDR;
    memset(p, 0, (size_t)(len - NET_HDR));
    p[0] = m->op, p[1] = m->target, net_w32(p + 2, m->id);
    uint8_t *b = p + 6;
    if (m->op == NDO_ANSWER) {
        b[0] = m->ans, b[1] = m->why;
        for (int j = 0; j < 3; j++) net_w32f(b + 2 + 4 * j, m->c[j]);
        net_w16(b + 14, m->radius_cm);
    } else if (m->op == NDO_HIT) {
        net_w16(b, m->hseq), net_w16(b + 2, m->power), net_w16(b + 4, m->stat);
        b[6] = m->flags, b[7] = m->src, net_w16(b + 8, m->boost), b[10] = m->react;
        for (int j = 0; j < 3; j++) net_w32f(b + 12 + 4 * j, m->from[j]), net_w32f(b + 24 + 4 * j, m->at[j]);
        net_w16(b + 36, m->atkp);
    } else if (m->op == NDO_END) {
        b[0] = m->reason, b[1] = m->result, net_w16(b + 2, m->pm);
    } else if (m->op == NDO_TIME) {
        net_w16(b, m->pm);
    } else if (m->op == NDO_RING) {
        b[0] = m->a, b[1] = m->b, b[2] = m->phase;
        for (int j = 0; j < 3; j++) net_w32f(b + 4 + 4 * j, m->c[j]);
        net_w16(b + 16, m->radius_cm);
    } else if (m->op == NDO_PULL) {
        net_w16(b, m->hseq);
        for (int j = 0; j < 3; j++) net_w32f(b + 2 + 4 * j, m->c[j]);
        net_w16(b + 14, m->radius_cm), net_w16(b + 16, m->hold_cm), net_w16(b + 18, (uint16_t)m->swirl), net_w16(b + 20, m->frames);
        b[22] = m->tier;
    }
    return len;
}
// -> true = a valid NET_DUEL packet: id != 0, the answer code in range, every float finite (a NaN centre / hit point is dropped)
static inline bool net_unpack_duel(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetDuelMsg *m) {
    int t = 0;
    if (!in || !m || len < NET_HDR || in[3] != NET_DUEL || !net_unpack_ex(in, len, &t, player, seq, NULL, NULL, NULL, NULL)) return false;
    const uint8_t *p = in + NET_HDR, *b = p + 6;
    memset(m, 0, sizeof *m);
    m->op = p[0], m->target = p[1], m->id = net_r32(p + 2);
    if (!m->id) return false;
    bool ok = true;
    if (m->op == NDO_ANSWER) {
        m->ans = b[0], m->why = b[1];
        for (int j = 0; j < 3; j++) m->c[j] = net_r32f(b + 2 + 4 * j), ok = ok && isfinite(m->c[j]);
        m->radius_cm = net_r16(b + 14);
        ok = ok && m->ans >= 1 && m->ans <= 3;  // (DUA_ACCEPT .. DUA_BUSY, sora_duel_core.h)
    } else if (m->op == NDO_HIT) {
        m->hseq = net_r16(b), m->power = net_r16(b + 2), m->stat = net_r16(b + 4);
        m->flags = b[6], m->src = b[7], m->boost = net_r16(b + 8), m->react = b[10], m->atkp = net_r16(b + 36);
        for (int j = 0; j < 3; j++) {
            m->from[j] = net_r32f(b + 12 + 4 * j), m->at[j] = net_r32f(b + 24 + 4 * j);
            ok = ok && isfinite(m->from[j]) && isfinite(m->at[j]);
        }
    } else if (m->op == NDO_END) {
        m->reason = b[0], m->result = b[1], m->pm = net_r16(b + 2);
    } else if (m->op == NDO_TIME) {
        m->pm = net_r16(b);
    } else if (m->op == NDO_RING) {
        m->a = b[0], m->b = b[1], m->phase = b[2];
        for (int j = 0; j < 3; j++) m->c[j] = net_r32f(b + 4 + 4 * j), ok = ok && isfinite(m->c[j]);
        m->radius_cm = net_r16(b + 16);
    } else if (m->op == NDO_PULL) {  // (s289 duelmagnet: the ranges are judged by sora_duel_magnet.h du_mag_accept)
        m->hseq = net_r16(b);
        for (int j = 0; j < 3; j++) m->c[j] = net_r32f(b + 2 + 4 * j), ok = ok && isfinite(m->c[j]);
        m->radius_cm = net_r16(b + 14), m->hold_cm = net_r16(b + 16), m->swirl = (int16_t)net_r16(b + 18), m->frames = net_r16(b + 20);
        m->tier = b[22];
    }
    return ok;
}

// ---- (s289 loot) NET_LOOT: the room's loot list (runtime/sora_net_loot.h; after Savage Wind PR #294 by byluki, MIT) ----
// A NEW type (13, enum at the top): no NET_VERSION change; an older game drops it as a "bad" packet without a log line.
// Reliable path only: a Melty room event (any type but SNAP / ESYNC: reliable, in order, once). Melty rooms only: on UDP netloot
// sends nothing and drops what comes. Never per frame: an op goes out when something happened (a drop, a pickup, a join).
// Packet = hdr (player, seq = this kind's own u16 counter) + u8 op + u8 count + count entries:
//   NLO_ADD (1..8)      u32 gid (its top byte = the player id of the game it came from), u32 src (the HashId of the enemy it dropped
//                       from, 0 = none), u8 kind (NLK_*), u8 name length (1..47), the actor name (net_area_chars' letters),
//                       f32 x y z (where it lies), i16 x 9 its rotation rows (x 32767; all 0 = upright)
//   NLO_TAKE (1..64)    u32 gid: picked up (or taken back by its game): gone in every game
//   NLO_MAPTAKE (1..12) u32 HashId, u8 name length (1..47), the actor name: an item the MAP placed, picked up in the sender's game
//   NLO_STATE (1)       u8 bits: 1 = on (the sender's `set netloot 1`), 2 = ask (answer with yours). On + ask goes to each joiner
//                       and to the room when the key turns on; the answer is on without ask (no ping-pong); 0 when it turns off.
//                       A friend's kill takes our drops of it away only when his game said on (it shares its own); an older
//                       build never says it: its kills leave every game its own drops, as before netloot.
// (NET_WORLD = 14 is s290 networld's (`set networld`): its own block right after this one.)
enum { NLO_ADD = 1, NLO_TAKE = 2, NLO_MAPTAKE = 3, NLO_STATE = 4 };
enum { NLK_DROP = 1, NLK_FELL = 2, NLK_THROWN = 3 };  // an enemy's drop / fell to the ground by a player (pouch, ore, tree) / a thrown weapon that landed
#define NET_LOOT_HEAD (NET_HDR + 2)  // 9: op, count
#define NET_LOOT_NAME_MAX 47
#define NET_LOOT_ADD_FIX 40          // one ADD entry without its name's letters
#define NET_LOOT_ADD_MAX 8
#define NET_LOOT_TAKE_MAX 64
#define NET_LOOT_MAP_FIX 5           // one MAPTAKE entry without its name's letters
#define NET_LOOT_MAP_MAX 12
#define NET_LOOT_PKT (NET_LOOT_HEAD + NET_LOOT_ADD_MAX * (NET_LOOT_ADD_FIX + NET_LOOT_NAME_MAX))  // 705, the biggest of the three
_Static_assert(NET_LOOT_PKT <= NET_PKT_MAX && NET_LOOT_HEAD + 4 * NET_LOOT_TAKE_MAX <= NET_PKT_MAX &&
                   NET_LOOT_HEAD + NET_LOOT_MAP_MAX * (NET_LOOT_MAP_FIX + NET_LOOT_NAME_MAX) <= NET_PKT_MAX,
               "a full NET_LOOT packet fits a packet and the Melty connector's 1024");
typedef struct { uint32_t gid, src; uint8_t kind; char name[NET_LOOT_NAME_MAX + 1]; float pos[3]; int16_t rot[9]; } NetLootAdd;
typedef struct { uint32_t hid; char name[NET_LOOT_NAME_MAX + 1]; } NetLootMap;
typedef struct {
    uint8_t op;
    int n;  // entries in the op's array
    NetLootAdd add[NET_LOOT_ADD_MAX];
    uint32_t gid[NET_LOOT_TAKE_MAX];
    NetLootMap map[NET_LOOT_MAP_MAX];
    bool on, ask;  // (NLO_STATE)
} NetLootMsg;

// the body after the header (p = in + NET_HDR, n = its length): op 1..4, count in range, every name 1..47 letters, the length exact
static inline bool net_loot_body_ok(const uint8_t *p, int n) {
    if (!p || n < 2) return false;
    int op = p[0], c = p[1], at = 2;
    if (op == NLO_ADD) {
        if (c < 1 || c > NET_LOOT_ADD_MAX) return false;
        for (int i = 0; i < c; i++) {
            if (at + NET_LOOT_ADD_FIX > n) return false;
            int l = p[at + 9];
            if (l < 1 || l > NET_LOOT_NAME_MAX || at + NET_LOOT_ADD_FIX + l > n || !net_area_chars((const char *)p + at + 10, l)) return false;
            at += NET_LOOT_ADD_FIX + l;
        }
    } else if (op == NLO_TAKE) {
        if (c < 1 || c > NET_LOOT_TAKE_MAX) return false;
        at += 4 * c;
    } else if (op == NLO_MAPTAKE) {
        if (c < 1 || c > NET_LOOT_MAP_MAX) return false;
        for (int i = 0; i < c; i++) {
            if (at + NET_LOOT_MAP_FIX > n) return false;
            int l = p[at + 4];
            if (l < 1 || l > NET_LOOT_NAME_MAX || at + NET_LOOT_MAP_FIX + l > n || !net_area_chars((const char *)p + at + 5, l)) return false;
            at += NET_LOOT_MAP_FIX + l;
        }
    } else if (op == NLO_STATE) {
        if (c != 1 || n < 3 || p[2] > 3) return false;
        at += 1;
    } else
        return false;
    return at == n;
}

static inline int net_loot_namelen(const char *s) {
    int l = 0;
    while (s && l <= NET_LOOT_NAME_MAX && s[l]) l++;
    return l;
}
// -> the packet length, -1 = refused (no entries / too many / a name of no letters or too long / gid 0 / a float not finite)
static inline int net_pack_loot_add(uint8_t player, uint16_t seq, const NetLootAdd *a, int n, uint8_t *out, int cap) {
    if (!a || !out || n < 1 || n > NET_LOOT_ADD_MAX) return -1;
    int len = NET_LOOT_HEAD;
    for (int i = 0; i < n; i++) {
        int l = net_loot_namelen(a[i].name);
        if (!a[i].gid || l < 1 || l > NET_LOOT_NAME_MAX || !net_area_chars(a[i].name, l)) return -1;
        for (int j = 0; j < 3; j++) if (!isfinite(a[i].pos[j])) return -1;
        len += NET_LOOT_ADD_FIX + l;
    }
    if (cap < len) return -1;
    net_hdr(out, NET_LOOT, player, seq);
    uint8_t *p = out + NET_HDR;
    p[0] = NLO_ADD, p[1] = (uint8_t)n;
    p += 2;
    for (int i = 0; i < n; i++) {
        int l = net_loot_namelen(a[i].name);
        net_w32(p, a[i].gid), net_w32(p + 4, a[i].src), p[8] = a[i].kind, p[9] = (uint8_t)l;
        memcpy(p + 10, a[i].name, (size_t)l);
        for (int j = 0; j < 3; j++) net_w32f(p + 10 + l + 4 * j, a[i].pos[j]);
        for (int j = 0; j < 9; j++) net_w16(p + 22 + l + 2 * j, (uint16_t)a[i].rot[j]);
        p += NET_LOOT_ADD_FIX + l;
    }
    return len;
}
static inline int net_pack_loot_take(uint8_t player, uint16_t seq, const uint32_t *gid, int n, uint8_t *out, int cap) {
    if (!gid || !out || n < 1 || n > NET_LOOT_TAKE_MAX) return -1;
    int len = NET_LOOT_HEAD + 4 * n;
    if (cap < len) return -1;
    for (int i = 0; i < n; i++) if (!gid[i]) return -1;
    net_hdr(out, NET_LOOT, player, seq);
    uint8_t *p = out + NET_HDR;
    p[0] = NLO_TAKE, p[1] = (uint8_t)n;
    for (int i = 0; i < n; i++) net_w32(p + 2 + 4 * i, gid[i]);
    return len;
}
static inline int net_pack_loot_map(uint8_t player, uint16_t seq, const NetLootMap *m, int n, uint8_t *out, int cap) {
    if (!m || !out || n < 1 || n > NET_LOOT_MAP_MAX) return -1;
    int len = NET_LOOT_HEAD;
    for (int i = 0; i < n; i++) {
        int l = net_loot_namelen(m[i].name);
        if (!m[i].hid || l < 1 || l > NET_LOOT_NAME_MAX || !net_area_chars(m[i].name, l)) return -1;
        len += NET_LOOT_MAP_FIX + l;
    }
    if (cap < len) return -1;
    net_hdr(out, NET_LOOT, player, seq);
    uint8_t *p = out + NET_HDR;
    p[0] = NLO_MAPTAKE, p[1] = (uint8_t)n;
    p += 2;
    for (int i = 0; i < n; i++) {
        int l = net_loot_namelen(m[i].name);
        net_w32(p, m[i].hid), p[4] = (uint8_t)l;
        memcpy(p + 5, m[i].name, (size_t)l);
        p += NET_LOOT_MAP_FIX + l;
    }
    return len;
}
// NLO_STATE: our `set netloot` now (on / off); ask = the receivers answer with theirs
static inline int net_pack_loot_state(uint8_t player, uint16_t seq, bool on, bool ask, uint8_t *out, int cap) {
    if (!out || cap < NET_LOOT_HEAD + 1) return -1;
    net_hdr(out, NET_LOOT, player, seq);
    out[NET_HDR] = NLO_STATE, out[NET_HDR + 1] = 1, out[NET_HDR + 2] = (uint8_t)((on ? 1 : 0) | (ask ? 2 : 0));
    return NET_LOOT_HEAD + 1;
}
// -> true = a valid NET_LOOT packet, m filled; false: not one / a gid or HashId 0 / a kind out of range / a float not finite
static inline bool net_unpack_loot(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetLootMsg *m) {
    int t = 0;
    if (!in || !m || len < NET_LOOT_HEAD || in[3] != NET_LOOT || !net_unpack_ex(in, len, &t, player, seq, NULL, NULL, NULL, NULL)) return false;
    const uint8_t *p = in + NET_HDR;
    memset(m, 0, sizeof *m);
    m->op = p[0], m->n = p[1];
    p += 2;
    for (int i = 0; i < m->n; i++) {
        if (m->op == NLO_ADD) {
            NetLootAdd *a = &m->add[i];
            int l = p[9];
            a->gid = net_r32(p), a->src = net_r32(p + 4), a->kind = p[8];
            memcpy(a->name, p + 10, (size_t)l), a->name[l] = 0;
            for (int j = 0; j < 3; j++) a->pos[j] = net_r32f(p + 10 + l + 4 * j);
            for (int j = 0; j < 9; j++) a->rot[j] = (int16_t)net_r16(p + 22 + l + 2 * j);
            if (!a->gid || a->kind < NLK_DROP || a->kind > NLK_THROWN || !isfinite(a->pos[0]) || !isfinite(a->pos[1]) || !isfinite(a->pos[2])) return false;
            p += NET_LOOT_ADD_FIX + l;
        } else if (m->op == NLO_TAKE) {
            m->gid[i] = net_r32(p + 4 * i);
            if (!m->gid[i]) return false;
        } else if (m->op == NLO_STATE) {
            m->on = (p[0] & 1) != 0, m->ask = (p[0] & 2) != 0;
        } else {
            NetLootMap *e = &m->map[i];
            int l = p[4];
            e->hid = net_r32(p);
            memcpy(e->name, p + 5, (size_t)l), e->name[l] = 0;
            if (!e->hid) return false;
            p += NET_LOOT_MAP_FIX + l;
        }
    }
    return true;
}

// ---- (s290 networld) NET_WORLD: the room's world flags (runtime/sora_net_world.h; after Savage Wind PR #294 by byluki, MIT) ----
// The type was reserved by s289 loot (14, enum at the top): no NET_VERSION change; an older game (no NET_WORLD in its
// net_unpack_ex) drops it as a "bad" packet. Reliable path only (a Melty room event: in order, once). Never per frame: STATE +
// PAGEs on a join / an ask / a Blood Moon there, SETs when the sender's own flags changed (looked at every 2 s).
// An index is a place in the flag list each game builds ITSELF from its own game files (sora_net_world.h nwd_list_build: the
// GameData bool flags of the allowed families, sorted by hash); fp = that list's fingerprint (crc32 over every entry's hash + family,
// in order). The receiver refuses a packet whose fp or count is not its own whole, so an index can only ever name a flag of the
// receiver's own list of allowed families: a packet never picks a flag by name or hash.
// Packet = hdr (player, seq = this kind's own u16 counter) + u8 op + the op's body:
//   NWO_STATE (1)  u8 bits (1 = on, 2 = ask: answer with your state + pages), u32 fp, u32 list count, u16 the sender's families
//                  (NWF_* bits it shares). On + ask goes to the room on a join and when the key turns on; the answer is on without
//                  ask (no ping-pong); 0 (off) when the key turns off. 14 bytes after the header.
//   NWO_PAGE  (2)  u32 fp, u16 gen (the sender's snapshot number), u8 page, u8 pages, u32 list count, u16 nbytes, nbytes of bits:
//                  bit i of byte j = list entry page * NET_WORLD_PAGE_BITS + 8 * j + i is set in the sender's game. pages =
//                  ceil(count / NET_WORLD_PAGE_BITS); nbytes = NET_WORLD_PAGE_BITS / 8, the last page's ceil(its bits / 8).
//                  The pages of one gen are the sender's whole set at that moment: they replace what the receiver knew of him
//                  (a Blood Moon in his game cleared some). 15 + nbytes bytes after the header.
//   NWO_SET   (3)  u32 fp, u8 n (1..NET_WORLD_SET_MAX), n x u16 index: flags that became set in the sender's game since his last
//                  pages (his own: not the ones he took from a friend). 6 + 2n bytes after the header.
enum { NWO_STATE = 1, NWO_PAGE = 2, NWO_SET = 3 };
#define NET_WORLD_PAGE_BITS 5120    // 640 bytes of bits a page
#define NET_WORLD_PAGES_MAX 13      // ceil(65535 / 5120): a list holds at most 65535 flags (u16 indices)
#define NET_WORLD_COUNT_MAX 65535
#define NET_WORLD_SET_MAX 250
#define NET_WORLD_STATE_LEN (NET_HDR + 15)
#define NET_WORLD_PAGE_HEAD (NET_HDR + 15)
#define NET_WORLD_SET_HEAD (NET_HDR + 6)
_Static_assert(NET_WORLD_PAGE_HEAD + NET_WORLD_PAGE_BITS / 8 <= NET_PKT_MAX && NET_WORLD_SET_HEAD + 2 * NET_WORLD_SET_MAX <= NET_PKT_MAX,
               "a full NET_WORLD packet fits a packet and the Melty connector's 1024");
_Static_assert((NET_WORLD_COUNT_MAX + NET_WORLD_PAGE_BITS - 1) / NET_WORLD_PAGE_BITS == NET_WORLD_PAGES_MAX, "pages for the biggest list");
typedef struct {
    uint8_t op;
    bool on, ask;                  // (STATE)
    uint32_t fp, count;            // (STATE, PAGE: count; SET: fp only)
    uint16_t fams;                 // (STATE)
    uint16_t gen;                  // (PAGE)
    uint8_t page, pages;           // (PAGE)
    uint16_t nbytes;               // (PAGE)
    uint8_t bits[NET_WORLD_PAGE_BITS / 8];  // (PAGE)
    int n;                         // (SET)
    uint16_t idx[NET_WORLD_SET_MAX];  // (SET)
} NetWorldMsg;
static inline int net_world_pages(uint32_t count) { return (int)((count + NET_WORLD_PAGE_BITS - 1) / NET_WORLD_PAGE_BITS); }
// the page's bytes: NET_WORLD_PAGE_BITS / 8, the last page's what is left (rounded up); 0 = no such page
static inline int net_world_page_bytes(uint32_t count, int page) {
    if (!count || count > NET_WORLD_COUNT_MAX || page < 0 || page >= net_world_pages(count)) return 0;
    uint32_t left = count - (uint32_t)page * NET_WORLD_PAGE_BITS;
    return left >= NET_WORLD_PAGE_BITS ? NET_WORLD_PAGE_BITS / 8 : (int)((left + 7) / 8);
}
// the body after the header (p = in + NET_HDR, n = its length): op 1..3, the exact length for the op, counts and pages in range
static inline bool net_world_body_ok(const uint8_t *p, int n) {
    if (!p || n < 1) return false;
    if (p[0] == NWO_STATE) return n == 15 && p[1] <= 3 && net_r32(p + 6) >= 1 && net_r32(p + 6) <= NET_WORLD_COUNT_MAX;
    if (p[0] == NWO_PAGE) {
        if (n < 16) return false;
        uint32_t count = net_r32(p + 9);
        int page = p[7], pages = p[8], nb = net_r16(p + 13);
        return count >= 1 && count <= NET_WORLD_COUNT_MAX && pages == net_world_pages(count) && page < pages &&
               nb == net_world_page_bytes(count, page) && n == 15 + nb;
    }
    if (p[0] == NWO_SET) return n >= 8 && p[5] >= 1 && p[5] <= NET_WORLD_SET_MAX && n == 6 + 2 * p[5];
    return false;
}
static inline int net_pack_world_state(uint8_t player, uint16_t seq, bool on, bool ask, uint32_t fp, uint32_t count, uint16_t fams,
                                       uint8_t *out, int cap) {
    if (!out || cap < NET_WORLD_STATE_LEN || !count || count > NET_WORLD_COUNT_MAX) return -1;
    net_hdr(out, NET_WORLD, player, seq);
    uint8_t *p = out + NET_HDR;
    p[0] = NWO_STATE, p[1] = (uint8_t)((on ? 1 : 0) | (ask ? 2 : 0));
    net_w32(p + 2, fp), net_w32(p + 6, count), net_w16(p + 10, fams);
    p[12] = p[13] = p[14] = 0;
    return NET_WORLD_STATE_LEN;
}
// bits = the page's own bytes (net_world_page_bytes of them); -> the packet length, -1 = refused
static inline int net_pack_world_page(uint8_t player, uint16_t seq, uint32_t fp, uint16_t gen, int page, uint32_t count,
                                      const uint8_t *bits, uint8_t *out, int cap) {
    int nb = net_world_page_bytes(count, page);
    if (!out || !bits || !nb || cap < NET_WORLD_PAGE_HEAD + nb) return -1;
    net_hdr(out, NET_WORLD, player, seq);
    uint8_t *p = out + NET_HDR;
    p[0] = NWO_PAGE;
    net_w32(p + 1, fp), net_w16(p + 5, gen);
    p[7] = (uint8_t)page, p[8] = (uint8_t)net_world_pages(count);
    net_w32(p + 9, count), net_w16(p + 13, (uint16_t)nb);
    memcpy(p + 15, bits, (size_t)nb);
    return NET_WORLD_PAGE_HEAD + nb;
}
static inline int net_pack_world_set(uint8_t player, uint16_t seq, uint32_t fp, const uint16_t *idx, int n, uint8_t *out, int cap) {
    if (!out || !idx || n < 1 || n > NET_WORLD_SET_MAX || cap < NET_WORLD_SET_HEAD + 2 * n) return -1;
    net_hdr(out, NET_WORLD, player, seq);
    uint8_t *p = out + NET_HDR;
    p[0] = NWO_SET;
    net_w32(p + 1, fp);
    p[5] = (uint8_t)n;
    for (int i = 0; i < n; i++) net_w16(p + 6 + 2 * i, idx[i]);
    return NET_WORLD_SET_HEAD + 2 * n;
}
// -> true = a valid NET_WORLD packet, m filled (the indices / pages are checked against OUR list by the receiver: net_world_fits)
static inline bool net_unpack_world(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetWorldMsg *m) {
    int t = 0;
    if (!in || !m || len < NET_HDR + 1 || in[3] != NET_WORLD || !net_unpack_ex(in, len, &t, player, seq, NULL, NULL, NULL, NULL)) return false;
    const uint8_t *p = in + NET_HDR;
    memset(m, 0, sizeof *m);
    m->op = p[0];
    if (m->op == NWO_STATE) {
        m->on = (p[1] & 1) != 0, m->ask = (p[1] & 2) != 0;
        m->fp = net_r32(p + 2), m->count = net_r32(p + 6), m->fams = net_r16(p + 10);
    } else if (m->op == NWO_PAGE) {
        m->fp = net_r32(p + 1), m->gen = net_r16(p + 5), m->page = p[7], m->pages = p[8];
        m->count = net_r32(p + 9), m->nbytes = net_r16(p + 13);
        memcpy(m->bits, p + 15, m->nbytes);
    } else {
        m->fp = net_r32(p + 1), m->n = p[5];
        for (int i = 0; i < m->n; i++) m->idx[i] = net_r16(p + 6 + 2 * i);
    }
    return true;
}
// the receiver's own check: the sender's list is ours (fp, count) and every index is in it -> true; false = refused whole
static inline bool net_world_fits(const NetWorldMsg *m, uint32_t fp, uint32_t count) {
    if (!m || !count || m->fp != fp) return false;
    if (m->op == NWO_STATE || m->op == NWO_PAGE) return m->count == count;
    for (int i = 0; i < m->n; i++)
        if (m->idx[i] >= count) return false;
    return m->op == NWO_SET;
}

// ---- (s289 shots) NET_SHOTS: the sender's arrows, Remote Bombs and thrown weapon, where they are in his world now (sora_net_shots.h) ----
// After Savage Wind PR #294 "Online mode" by byluki (fixes by mstampfli), MIT: its OnlineShots (sample_shots), sent as our own packet.
// Packet = hdr (player, seq = this kind's OWN u16 counter) + u8 n (0..NET_SHOTS_N) + u8 nb (0..NET_SHOTS_BOOMS)
//   + n x 24 B {u16 id (!= 0, the sender's track number, unique in a packet), u8 kind (NSK_*), u8 flags (NSF_*), f32 x y z (world,
//     BotW units), i16 qx qy qz qw (its rotation, x 32767; not all 0)}
//   + nb x 16 B {u16 seq (!= 0), u8 kind (a bomb NSK_*), u8 0, f32 x y z}: a Remote Bomb that went off, in every packet for
//     NSHOT_BOOM_MS, played once by its seq
//   + for each entry of kind NSK_THROWN, in entry order: u8 len (1..NET_GEAR_NAME_MAX) + len letters = that weapon's actor name
// A NEW type, NET_VERSION unchanged: a game without it finds no such type in net_unpack_ex and counts the packet "bad" (as
// NET_GEAR). 20 Hz state: in a Melty room it rides the snapshot lanes (sora_melty.cpp, the ESYNC case).
enum { NSK_NORMAL_ARROW = 0, NSK_FIRE_ARROW = 1, NSK_ICE_ARROW = 2, NSK_ELECTRIC_ARROW = 3, NSK_BOMB_ARROW = 4,
       NSK_ANCIENT_ARROW = 5, NSK_BRIGHT_ARROW = 6, NSK_REMOTE_BOMB = 7, NSK_REMOTE_BOMB_CUBE = 8, NSK_REMOTE_BOMB2 = 9,
       NSK_REMOTE_BOMB_CUBE2 = 10, NSK_THROWN = 11, NSK_N = 12 };
enum { NSF_STILL = 1 };  // it has not moved for NSHOT_STILL_HOLD_MS: the receiver holds it there (no extrapolation)
#define NET_SHOTS_N 8
#define NET_SHOTS_BOOMS 4
#define NET_SHOTS_ENT 24
#define NET_SHOTS_BOOM_ENT 16
#define NET_SHOTS_PKT (NET_HDR + 2 + NET_SHOTS_N * (NET_SHOTS_ENT + 1 + NET_GEAR_NAME_MAX) + NET_SHOTS_BOOMS * NET_SHOTS_BOOM_ENT)  // 521
typedef struct { uint16_t id; uint8_t kind, flags; float pos[3], q[4]; char name[NET_GEAR_NAME_MAX + 1]; } NetShot;  // (name: NSK_THROWN only)
typedef struct { uint16_t seq; uint8_t kind; float pos[3]; } NetBoom;
static inline bool nshot_kind_arrow(int k) { return k >= NSK_NORMAL_ARROW && k <= NSK_BRIGHT_ARROW; }
static inline bool nshot_kind_bomb(int k) { return k >= NSK_REMOTE_BOMB && k <= NSK_REMOTE_BOMB_CUBE2; }

// a rotation on the wire: 4 x i16 (q x 32767, rounded, clamped); read back normalised. False = not finite / all 0
static inline bool net_wq(uint8_t *b, const float q[4]) {
    float l = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (!(l > 1e-6f) || !isfinite(l)) return false;
    for (int i = 0; i < 4; i++) {
        float v = q[i] / l * 32767.0f;
        v = v > 32767.0f ? 32767.0f : v < -32767.0f ? -32767.0f : v;
        net_w16(b + 2 * i, (uint16_t)(int16_t)lrintf(v));
    }
    return net_r16(b) | net_r16(b + 2) | net_r16(b + 4) | net_r16(b + 6);
}
static inline bool net_rq(const uint8_t *b, float q[4]) {
    float l2 = 0;
    for (int i = 0; i < 4; i++) q[i] = (float)(int16_t)net_r16(b + 2 * i) / 32767.0f, l2 += q[i] * q[i];
    if (!(l2 > 0.25f)) return false;  // (|q| ~ 1 on the wire; far below = garbage)
    float l = sqrtf(l2);
    for (int i = 0; i < 4; i++) q[i] /= l;
    return true;
}
static inline bool net_f3ok(const uint8_t *b) { return isfinite(net_r32f(b)) && isfinite(net_r32f(b + 4)) && isfinite(net_r32f(b + 8)); }

// ---- (s289 shots) NET_RUNES: what the sender holds with Magnesis / stopped with Stasis, and his Cryonis pillars ----
// Packet = hdr (player, seq = this kind's OWN u16 counter) + u8 flags (NRF_*) + u8 nice (0..NET_RUNES_ICE)
//   + (NRF_MAG) 24 B {u32 hashid (the object's map HashId, actor +0x640; 0 = not a map object), f32 x y z, i16 q[4]}
//   + (NRF_STOP) 24 B {the same, the object Stasis stopped}
//   + nice x 20 B {f32 x y z, i16 q[4]}: his Cryonis pillars (Obj_IceMakerBlock), nearest first
// The KH2 Magnet SPELL (duels, sora_duel_magnet.h) is NOT this: these are BotW's runes.
enum { NRF_MAG = 1, NRF_STOP = 2 };
#define NET_RUNES_ICE 3
#define NET_RUNES_OBJ 24
#define NET_RUNES_ICE_ENT 20
#define NET_RUNES_PKT (NET_HDR + 2 + 2 * NET_RUNES_OBJ + NET_RUNES_ICE * NET_RUNES_ICE_ENT)  // 117
typedef struct { uint32_t hashid; float pos[3], q[4]; } NetRuneObj;
typedef struct { float pos[3], q[4]; } NetRuneIce;
typedef struct { uint8_t flags, nice; NetRuneObj mag, stop; NetRuneIce ice[NET_RUNES_ICE]; } NetRunes;

static inline bool net_shots_body_ok(int t, const uint8_t *p, int n) {
    if (!p || n < 2) return false;
    float q[4];
    if (t == NET_RUNES) {
        if ((p[0] & ~(NRF_MAG | NRF_STOP)) || p[1] > NET_RUNES_ICE) return false;
        int at = 2;
        for (int f = 0; f < 2; f++) {
            if (!(p[0] & (f ? NRF_STOP : NRF_MAG))) continue;
            if (at + NET_RUNES_OBJ > n || !net_f3ok(p + at + 4) || !net_rq(p + at + 16, q)) return false;
            at += NET_RUNES_OBJ;
        }
        for (int i = 0; i < p[1]; i++, at += NET_RUNES_ICE_ENT)
            if (at + NET_RUNES_ICE_ENT > n || !net_f3ok(p + at) || !net_rq(p + at + 12, q)) return false;
        return at == n;
    }
    if (t != NET_SHOTS || p[0] > NET_SHOTS_N || p[1] > NET_SHOTS_BOOMS) return false;
    int ns = p[0], nb = p[1], at = 2 + ns * NET_SHOTS_ENT + nb * NET_SHOTS_BOOM_ENT, nthr = 0;
    if (at > n) return false;
    for (int i = 0; i < ns; i++) {
        const uint8_t *e = p + 2 + i * NET_SHOTS_ENT;
        uint16_t id = net_r16(e);
        if (!id || e[2] >= NSK_N || !net_f3ok(e + 4) || !net_rq(e + 16, q)) return false;
        for (int j = 0; j < i; j++)
            if (net_r16(p + 2 + j * NET_SHOTS_ENT) == id) return false;  // (one track, one entry)
        nthr += e[2] == NSK_THROWN;
    }
    for (int i = 0; i < nb; i++) {
        const uint8_t *e = p + 2 + ns * NET_SHOTS_ENT + i * NET_SHOTS_BOOM_ENT;
        if (!net_r16(e) || !nshot_kind_bomb(e[2]) || e[3] || !net_f3ok(e + 4)) return false;
    }
    for (int i = 0; i < nthr; i++) {
        if (at >= n) return false;
        int l = p[at];
        if (l < 1 || l > NET_GEAR_NAME_MAX || at + 1 + l > n || !net_area_chars((const char *)p + at + 1, l)) return false;
        at += 1 + l;
    }
    return at == n;
}

// -> bytes written, -1 = bad input (n / nb out of range, an id 0 or twice, a bad kind, a thrown entry without a name the wire takes,
// a float not finite) or no room
static inline int net_pack_shots(uint8_t player, uint16_t seq, const NetShot *s, int n, const NetBoom *b, int nb, uint8_t *out, int cap) {
    if (n < 0 || n > NET_SHOTS_N || nb < 0 || nb > NET_SHOTS_BOOMS || (n && !s) || (nb && !b) || !out) return -1;
    int at = NET_HDR + 2 + n * NET_SHOTS_ENT + nb * NET_SHOTS_BOOM_ENT, need = at;
    if (cap < at) return -1;
    for (int i = 0; i < n; i++)  // (an id twice: refused, as net_shots_body_ok would refuse it)
        for (int j = 0; j < i; j++)
            if (s[i].id == s[j].id) return -1;
    for (int i = 0; i < n; i++) {
        uint8_t *e = out + NET_HDR + 2 + i * NET_SHOTS_ENT;
        if (!s[i].id || s[i].kind >= NSK_N) return -1;
        net_w16(e, s[i].id), e[2] = s[i].kind, e[3] = s[i].flags;
        for (int j = 0; j < 3; j++) {
            if (!isfinite(s[i].pos[j])) return -1;
            net_w32f(e + 4 + 4 * j, s[i].pos[j]);
        }
        if (!net_wq(e + 16, s[i].q)) return -1;
        if (s[i].kind != NSK_THROWN) continue;
        int l = 0;
        while (l <= NET_GEAR_NAME_MAX && s[i].name[l]) l++;
        if (l < 1 || l > NET_GEAR_NAME_MAX || !net_area_chars(s[i].name, l) || (need += 1 + l) > cap) return -1;  // (all the names: room)
    }
    for (int i = 0; i < nb; i++) {
        uint8_t *e = out + NET_HDR + 2 + n * NET_SHOTS_ENT + i * NET_SHOTS_BOOM_ENT;
        if (!b[i].seq || !nshot_kind_bomb(b[i].kind)) return -1;
        net_w16(e, b[i].seq), e[2] = b[i].kind, e[3] = 0;
        for (int j = 0; j < 3; j++) {
            if (!isfinite(b[i].pos[j])) return -1;
            net_w32f(e + 4 + 4 * j, b[i].pos[j]);
        }
    }
    for (int i = 0; i < n; i++) {
        if (s[i].kind != NSK_THROWN) continue;
        int l = (int)strlen(s[i].name);
        out[at] = (uint8_t)l, memcpy(out + at + 1, s[i].name, (size_t)l), at += 1 + l;
    }
    net_hdr(out, NET_SHOTS, player, seq);
    out[NET_HDR] = (uint8_t)n, out[NET_HDR + 1] = (uint8_t)nb;
    return at;
}
static inline bool net_unpack_shots(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetShot s[NET_SHOTS_N], int *n,
                                    NetBoom b[NET_SHOTS_BOOMS], int *nb) {
    int t = 0;
    if (!in || !s || !n || !b || !nb || len < NET_HDR + 2 || in[3] != NET_SHOTS || !net_unpack_ex(in, len, &t, player, seq, NULL, NULL, NULL, NULL))
        return false;
    const uint8_t *p = in + NET_HDR;
    *n = p[0], *nb = p[1];
    int at = 2 + *n * NET_SHOTS_ENT + *nb * NET_SHOTS_BOOM_ENT;
    for (int i = 0; i < *n; i++) {
        const uint8_t *e = p + 2 + i * NET_SHOTS_ENT;
        memset(&s[i], 0, sizeof s[i]);
        s[i].id = net_r16(e), s[i].kind = e[2], s[i].flags = e[3];
        for (int j = 0; j < 3; j++) s[i].pos[j] = net_r32f(e + 4 + 4 * j);
        net_rq(e + 16, s[i].q);
    }
    for (int i = 0; i < *nb; i++) {
        const uint8_t *e = p + 2 + *n * NET_SHOTS_ENT + i * NET_SHOTS_BOOM_ENT;
        b[i].seq = net_r16(e), b[i].kind = e[2];
        for (int j = 0; j < 3; j++) b[i].pos[j] = net_r32f(e + 4 + 4 * j);
    }
    for (int i = 0; i < *n; i++) {
        if (s[i].kind != NSK_THROWN) continue;
        int l = p[at];
        memcpy(s[i].name, p + at + 1, (size_t)l), s[i].name[l] = 0, at += 1 + l;
    }
    return true;
}

static inline int net_pack_runes(uint8_t player, uint16_t seq, const NetRunes *r, uint8_t *out, int cap) {
    if (!r || !out || (r->flags & ~(NRF_MAG | NRF_STOP)) || r->nice > NET_RUNES_ICE || cap < NET_RUNES_PKT) return -1;
    uint8_t *b = out + NET_HDR + 2;
    for (int f = 0; f < 2; f++) {
        if (!(r->flags & (f ? NRF_STOP : NRF_MAG))) continue;
        const NetRuneObj *o = f ? &r->stop : &r->mag;
        net_w32(b, o->hashid);
        for (int j = 0; j < 3; j++) {
            if (!isfinite(o->pos[j])) return -1;
            net_w32f(b + 4 + 4 * j, o->pos[j]);
        }
        if (!net_wq(b + 16, o->q)) return -1;
        b += NET_RUNES_OBJ;
    }
    for (int i = 0; i < r->nice; i++, b += NET_RUNES_ICE_ENT) {
        for (int j = 0; j < 3; j++) {
            if (!isfinite(r->ice[i].pos[j])) return -1;
            net_w32f(b + 4 * j, r->ice[i].pos[j]);
        }
        if (!net_wq(b + 12, r->ice[i].q)) return -1;
    }
    net_hdr(out, NET_RUNES, player, seq);
    out[NET_HDR] = r->flags, out[NET_HDR + 1] = r->nice;
    return (int)(b - out);
}
static inline bool net_unpack_runes(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetRunes *r) {
    int t = 0;
    if (!in || !r || len < NET_HDR + 2 || in[3] != NET_RUNES || !net_unpack_ex(in, len, &t, player, seq, NULL, NULL, NULL, NULL)) return false;
    const uint8_t *p = in + NET_HDR, *b = p + 2;
    memset(r, 0, sizeof *r);
    r->flags = p[0], r->nice = p[1];
    for (int f = 0; f < 2; f++) {
        if (!(r->flags & (f ? NRF_STOP : NRF_MAG))) continue;
        NetRuneObj *o = f ? &r->stop : &r->mag;
        o->hashid = net_r32(b);
        for (int j = 0; j < 3; j++) o->pos[j] = net_r32f(b + 4 + 4 * j);
        net_rq(b + 16, o->q);
        b += NET_RUNES_OBJ;
    }
    for (int i = 0; i < r->nice; i++, b += NET_RUNES_ICE_ENT) {
        for (int j = 0; j < 3; j++) r->ice[i].pos[j] = net_r32f(b + 4 * j);
        net_rq(b + 12, r->ice[i].q);
    }
    return true;
}

// ---- (s289 objs) NET_OBJS 17 / NET_OBJEV 18: map objects that move / break the same in every world (sora_net_objs.h) ----
// NET_OBJS (state: 20 Hz while something moves; a Melty room carries it on the ESYNC lanes as a snapshot, latest wins):
//   u8 n (1..12), n x 58 bytes, by kind:
//   kind 1 (an object): u8 kind, u8 flags (1 REST = the last one: it came to rest, 2 HELD = the sender holds it), u32 map
//     HashId, f32 m[12] (its actor's matrix, BotW units, row-major 3x4 as actor +0x398), u32 extra (0)
//   kind 2 (a mover: netmovers) / kind 3 (a horse: nethorse), "pose records" (s289 objs parts 3-4): u8 kind, u8 flags,
//     u32 h, f32 qx qy qz (its rotation as a unit quaternion with w >= 0, w = sqrt(1 - x2 - y2 - z2)), f32 tx ty tz (its
//     place), char as[28] (the AS its actor plays in slot 0, NUL-padded, "" = none / not known)
//     kind 2: h = its map HashId, flags 1 REST
//     kind 3: h = crc32 of its actor's name (GameRomHorse12, GameRomHorseSaddle_00, GameROMMotorcycle, ...), flags & 0x0c:
//       0 the horse the sender rides now (= he rides), 4 its saddle, 8 its reins (their as "")
//     (s291 horsefix) kind 3, the horse: as[27] (the AS field's last byte: no older build writes it (0) or reads it, a name has
//       27 letters at most) = its coat (sora_net_objs_core.h nh_coat_byte: 0 not known / live nethorsecoat 0)
// NET_OBJEV (one-offs: a Melty room's reliable event "sora.18"): u8 op (1 BREAK = it broke in the sender's world just now,
//   2 TOLD = one of the session's broken list, re-told for a late joiner), u8 kind (1 breakable, 2 tree), u32 HashId,
//   f32 x y z (where it stood), i8 fall x, z (a tree's, x 127; 0 0 = not known), u16 0
// NEW types, NET_VERSION unchanged (as NET_WARP / NET_GEAR): an older game counts them "bad" (net_unpack_ex) and goes on.
enum { NOR_OBJ = 1, NOR_MOVER = 2, NOR_HORSE = 3 };
enum { NORF_REST = 1, NORF_HELD = 2 };
enum { NORH_PART = 0x0c, NORH_HORSE = 0, NORH_SADDLE = 4, NORH_REINS = 8 };  // (kind 3: flags & NORH_PART)
enum { NOE_BREAK = 1, NOE_TOLD = 2 };
#define NET_OBJS_AS 28  // (kinds 2 / 3: the AS name's bytes, its NUL included: 27 letters at most)
typedef struct { uint8_t kind, flags; uint32_t h; float m[12]; uint32_t extra; char as[NET_OBJS_AS]; uint8_t coat; } NetObjRec;  // (as: kinds 2, 3; coat: kind 3, s291)
typedef struct { uint8_t op, kind; uint32_t h; float pos[3]; int8_t dir[2]; } NetObjEv;

// (s289 objs parts 3-4) a row-major 3x4 matrix's rotation <-> a unit quaternion x y z w (w >= 0); the rows are BotW's
// Matrix34f (actor +0x398: v' = R v + t, t at 3, 7, 11). -> false = not finite (q = the identity then)
static inline bool net_q_from_m(const float m[12], float q[4]) {
    double r00 = m[0], r01 = m[1], r02 = m[2], r10 = m[4], r11 = m[5], r12 = m[6], r20 = m[8], r21 = m[9], r22 = m[10];
    double tr = r00 + r11 + r22, x, y, z, w, s;
    if (tr > 0) s = sqrt(tr + 1) * 2, w = s / 4, x = (r21 - r12) / s, y = (r02 - r20) / s, z = (r10 - r01) / s;
    else if (r00 > r11 && r00 > r22) s = sqrt(1 + r00 - r11 - r22) * 2, w = (r21 - r12) / s, x = s / 4, y = (r01 + r10) / s, z = (r02 + r20) / s;
    else if (r11 > r22) s = sqrt(1 + r11 - r00 - r22) * 2, w = (r02 - r20) / s, x = (r01 + r10) / s, y = s / 4, z = (r12 + r21) / s;
    else s = sqrt(1 + r22 - r00 - r11) * 2, w = (r10 - r01) / s, x = (r02 + r20) / s, y = (r12 + r21) / s, z = s / 4;
    double L = sqrt(x * x + y * y + z * z + w * w);
    if (!(L > 1e-6) || !isfinite(L)) { q[0] = q[1] = q[2] = 0, q[3] = 1; return false; }
    if (w < 0) L = -L;
    q[0] = (float)(x / L), q[1] = (float)(y / L), q[2] = (float)(z / L), q[3] = (float)(w / L);
    return true;
}
static inline void net_m_from_q(const float q[4], const float t[3], float m[12]) {
    double x = q[0], y = q[1], z = q[2], w = q[3];
    m[0] = (float)(1 - 2 * (y * y + z * z)), m[1] = (float)(2 * (x * y - z * w)), m[2] = (float)(2 * (x * z + y * w)), m[3] = t[0];
    m[4] = (float)(2 * (x * y + z * w)), m[5] = (float)(1 - 2 * (x * x + z * z)), m[6] = (float)(2 * (y * z - x * w)), m[7] = t[1];
    m[8] = (float)(2 * (x * z - y * w)), m[9] = (float)(2 * (y * z + x * w)), m[10] = (float)(1 - 2 * (x * x + y * y)), m[11] = t[2];
}

static inline int net_pack_objs(uint8_t player, uint16_t seq, const NetObjRec *r, int n, uint8_t *out, int cap) {
    if (!r || !out || n < 1 || n > NET_OBJS_N) return -1;
    int len = NET_HDR + 1 + n * NET_OBJS_REC;
    if (cap < len) return -1;
    net_hdr(out, NET_OBJS, player, seq);
    uint8_t *p = out + NET_HDR, *q = p + 1;
    p[0] = (uint8_t)n;
    for (int i = 0; i < n; i++, q += NET_OBJS_REC) {
        q[0] = r[i].kind, q[1] = r[i].flags;
        net_w32(q + 2, r[i].h);
        if (r[i].kind == NOR_MOVER || r[i].kind == NOR_HORSE) {  // (a pose record)
            float qq[4];
            net_q_from_m(r[i].m, qq);
            for (int j = 0; j < 3; j++) net_w32f(q + 6 + 4 * j, qq[j]), net_w32f(q + 18 + 4 * j, r[i].m[3 + 4 * j]);
            memset(q + 30, 0, NET_OBJS_AS);
            for (int j = 0; j < NET_OBJS_AS - 1 && r[i].as[j]; j++) q[30 + j] = (uint8_t)r[i].as[j];
            if (r[i].kind == NOR_HORSE) q[30 + NET_OBJS_AS - 1] = r[i].coat;  // (s291 horsefix: 0 = as before)
            continue;
        }
        for (int j = 0; j < 12; j++) net_w32f(q + 6 + 4 * j, r[i].m[j]);
        net_w32(q + 54, r[i].extra);
    }
    return len;
}
// (s290 rv3fix, review 3 #3) a row-major 3x4 matrix whose 3x3 part (m[0..2] / m[4..6] / m[8..10]) is a rotation: every row's length
// within NET_ROT_LEN of 1, every pair of rows within NET_ROT_DOT of orthogonal (dot product), the determinant within NET_ROT_DET
// of +1. The tolerances are a GUESS (a game matrix drifts ~1e-6; these allow a few percent of rounding / a slight scale and
// still refuse a zero or flipped or sheared one, which a body asked to reach in one step turns into NaN speed).
#define NET_ROT_LEN 0.1f
#define NET_ROT_DOT 0.1f
#define NET_ROT_DET 0.2f
static inline bool net_rot_ok(const float m[12]) {
    const float *r0 = m, *r1 = m + 4, *r2 = m + 8;
    const float *r[3] = {r0, r1, r2};
    for (int i = 0; i < 3; i++) {
        float l = sqrtf(r[i][0] * r[i][0] + r[i][1] * r[i][1] + r[i][2] * r[i][2]);
        if (!(fabsf(l - 1.0f) <= NET_ROT_LEN)) return false;  // (NaN fails too)
    }
    for (int i = 0; i < 3; i++)
        for (int j = i + 1; j < 3; j++)
            if (!(fabsf(r[i][0] * r[j][0] + r[i][1] * r[j][1] + r[i][2] * r[j][2]) <= NET_ROT_DOT)) return false;
    float det = r0[0] * (r1[1] * r2[2] - r1[2] * r2[1]) - r0[1] * (r1[0] * r2[2] - r1[2] * r2[0]) + r0[2] * (r1[0] * r2[1] - r1[1] * r2[0]);
    return fabsf(det - 1.0f) <= NET_ROT_DET;
}
// -> false = not a good NET_OBJS. A record with HashId 0, a kind this build does not take (not 1..3; kind 3 with part 12), or a
// non-finite / huge (> 1e6) value or (kind 1, s290) a 3x3 part that is not a rotation (net_rot_ok) is dropped alone; a pose record's AS name with a letter outside ' '..'~' is emptied (its pose
// kept); *n = the good ones (in r[0..*n-1], at most NET_OBJS_N)
static inline bool net_unpack_objs(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetObjRec *r, int *n) {
    int t = 0;
    if (n) *n = 0;
    if (!in || !r || !n || len < NET_HDR || in[3] != NET_OBJS || !net_unpack_ex(in, len, &t, player, seq, NULL, NULL, NULL, NULL)) return false;
    const uint8_t *p = in + NET_HDR, *q = p + 1;
    int k = 0;
    for (int i = 0; i < p[0]; i++, q += NET_OBJS_REC) {
        NetObjRec *o = &r[k];
        memset(o, 0, sizeof *o);
        o->kind = q[0], o->flags = q[1], o->h = net_r32(q + 2);
        bool ok = o->h != 0;
        if (o->kind == NOR_OBJ) {
            o->extra = net_r32(q + 54);
            for (int j = 0; j < 12; j++) o->m[j] = net_r32f(q + 6 + 4 * j), ok = ok && isfinite(o->m[j]) && fabsf(o->m[j]) < 1e6f;
            ok = ok && net_rot_ok(o->m);  // (s290 rv3fix: NaN fails it, so the finite test above stays the first)
        } else if (o->kind == NOR_MOVER || o->kind == NOR_HORSE) {
            if (o->kind == NOR_HORSE && (o->flags & NORH_PART) == NORH_PART) ok = false;
            float qq[4], tt[3];
            double s = 0;
            for (int j = 0; j < 3; j++) {
                qq[j] = net_r32f(q + 6 + 4 * j), tt[j] = net_r32f(q + 18 + 4 * j);
                ok = ok && isfinite(qq[j]) && isfinite(tt[j]) && fabsf(tt[j]) < 1e6f;
                s += (double)qq[j] * qq[j];
            }
            ok = ok && s <= 1.001;
            if (ok) {
                qq[3] = (float)sqrt(s < 1 ? 1 - s : 0);
                double L = sqrt(s + (double)qq[3] * qq[3]);
                if (!(L > 0.5)) ok = false;
                else for (int j = 0; j < 4; j++) qq[j] = (float)(qq[j] / L);
            }
            if (ok) net_m_from_q(qq, tt, o->m);
            bool name_ok = true;
            for (int j = 0; j < NET_OBJS_AS - 1; j++) {
                uint8_t c = q[30 + j];
                if (!c) break;
                if (c < 0x20 || c > 0x7e) name_ok = false;
                o->as[j] = (char)c;
            }
            if (!name_ok) memset(o->as, 0, sizeof o->as);
            if (o->kind == NOR_HORSE) o->coat = q[30 + NET_OBJS_AS - 1];  // (s291 horsefix: the coat byte, 0 from an older build)
        } else ok = false;
        if (ok) k++;
    }
    *n = k;
    return true;
}
static inline int net_pack_objev(uint8_t player, uint16_t seq, const NetObjEv *e, uint8_t *out, int cap) {
    if (!e || !out || cap < NET_OBJEV_LEN || !e->h || (e->op != NOE_BREAK && e->op != NOE_TOLD) || (e->kind != 1 && e->kind != 2)) return -1;
    net_hdr(out, NET_OBJEV, player, seq);
    uint8_t *p = out + NET_HDR;
    memset(p, 0, NET_OBJEV_LEN - NET_HDR);
    p[0] = e->op, p[1] = e->kind;
    net_w32(p + 2, e->h);
    for (int j = 0; j < 3; j++) net_w32f(p + 6 + 4 * j, e->pos[j]);
    p[18] = (uint8_t)e->dir[0], p[19] = (uint8_t)e->dir[1];
    return NET_OBJEV_LEN;
}
// -> false = not a good NET_OBJEV (HashId 0, a non-finite / huge place)
static inline bool net_unpack_objev(const uint8_t *in, int len, uint8_t *player, uint16_t *seq, NetObjEv *e) {
    int t = 0;
    if (!in || !e || len < NET_HDR || in[3] != NET_OBJEV || !net_unpack_ex(in, len, &t, player, seq, NULL, NULL, NULL, NULL)) return false;
    const uint8_t *p = in + NET_HDR;
    memset(e, 0, sizeof *e);
    e->op = p[0], e->kind = p[1], e->h = net_r32(p + 2);
    bool ok = e->h != 0;
    for (int j = 0; j < 3; j++) e->pos[j] = net_r32f(p + 6 + 4 * j), ok = ok && isfinite(e->pos[j]) && fabsf(e->pos[j]) < 1e6f;
    e->dir[0] = (int8_t)p[18], e->dir[1] = (int8_t)p[19];
    return ok;
}

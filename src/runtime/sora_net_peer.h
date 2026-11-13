// Multiplayer slice 1: one remote peer's state + snapshot interpolation (spec section 5).
// Header-only, pure C, time passed in (no clocks inside): host-tested by runtime/test/net_test.c.
#pragma once
#include "sora_net_proto.h"

#define NET_RING 8
#define NET_INTERP_MS 120
#define NET_FADE_MS 2000
#define NET_DROP_MS 10000
#define NET_FADE_OUT_MS 500
#define NET_KH_FPS 60.0

typedef enum { PEER_NONE, PEER_LIVE, PEER_FADING } PeerState;

typedef struct {
    PeerState st; uint8_t player; char name[16]; uint64_t last_ms; uint16_t last_seq;
    NetSnap ring[NET_RING]; uint64_t at[NET_RING]; int n;  // ring[0] oldest .. ring[n-1] newest, at = arrival ms
    NetEvSeen evs;
} NetPeer;

typedef struct {
    bool valid; uint8_t body, kb_chain, kb_alt; uint16_t slot;
    float clock, feet[3], yaw, alpha; uint16_t flags; uint8_t hp, mp; uint8_t outfit;  // (mp4: outfit)
    bool bones_ok; NetBones bones;  // (v8) the sampled bone block: valid when bones_ok (clock = the extrapolated base clock)
} PeerPose;

static inline void peer_reset(NetPeer *p, uint8_t player) {
    char name[16];
    memcpy(name, p->name, sizeof name);
    memset(p, 0, sizeof *p);
    memcpy(p->name, name, sizeof name);
    p->player = player;
}

static inline void peer_set_name(NetPeer *p, const char *name) {
    if (!name) return;
    size_t n = strlen(name);
    if (n > sizeof p->name - 1) n = sizeof p->name - 1;
    memcpy(p->name, name, n);
    p->name[n] = 0;
}

// HELLO with its packet seq: a seq older than the newest SNAP from the same player = that side restarted -> reset
static inline void peer_on_hello_seq(NetPeer *p, uint8_t player, const char *name, int seq, uint64_t now_ms) {
    bool restarted = p->st == PEER_NONE || player != p->player ||
                     (seq >= 0 && p->n > 0 && !net_seq_newer((uint16_t)seq, p->last_seq));
    if (restarted) peer_reset(p, player);
    peer_set_name(p, name);
    p->st = PEER_LIVE;
    p->last_ms = now_ms;
}

static inline void peer_on_hello(NetPeer *p, uint8_t player, const char *name, uint64_t now_ms) {
    peer_on_hello_seq(p, player, name, -1, now_ms);
}

// false = stale (not newer than the newest kept) and dropped
static inline bool peer_on_snap(NetPeer *p, const NetSnap *s, uint64_t now_ms) {
    if (p->st == PEER_NONE || s->player != p->player) peer_reset(p, s->player);
    else if (p->n > 0 && !net_seq_newer(s->seq, p->last_seq)) return false;
    if (p->n == NET_RING) {
        memmove(p->ring, p->ring + 1, sizeof p->ring[0] * (NET_RING - 1));
        memmove(p->at, p->at + 1, sizeof p->at[0] * (NET_RING - 1));
        p->n--;
    }
    p->ring[p->n] = *s;
    p->at[p->n] = now_ms;
    p->n++;
    p->last_seq = s->seq;
    p->last_ms = now_ms;
    p->st = PEER_LIVE;
    return true;
}

static inline void peer_on_bye(NetPeer *p) { peer_reset(p, p->player); }

static inline void peer_tick(NetPeer *p, uint64_t now_ms) {
    if (p->st == PEER_NONE || now_ms < p->last_ms) return;
    uint64_t quiet = now_ms - p->last_ms;
    if (quiet > NET_DROP_MS) peer_reset(p, p->player);
    else if (quiet > NET_FADE_MS) p->st = PEER_FADING;
}

static inline float peer_wrap_2pi(double a) {
    a = fmod(a, NET_TWO_PI);
    if (a < 0) a += NET_TWO_PI;
    return (float)(a >= NET_TWO_PI ? 0 : a);
}

// (Task 8) a remote moving fx: its last FXTRACK_N places by arrival ms; sampled NET_INTERP_MS behind, as the pose
#define FXTRACK_N 3
typedef struct { int n; uint8_t anc; uint64_t at[FXTRACK_N]; float pos[FXTRACK_N][3], yaw[FXTRACK_N]; } FxTrack;  // (slice 3: anc = NANC_* the places are relative to)
static inline void fxtrack_push(FxTrack *t, uint64_t at, const float pos[3], float yaw) {
    if (t->n == FXTRACK_N) {
        memmove(t->at, t->at + 1, sizeof t->at[0] * (FXTRACK_N - 1));
        memmove(t->pos, t->pos + 1, sizeof t->pos[0] * (FXTRACK_N - 1));
        memmove(t->yaw, t->yaw + 1, sizeof t->yaw[0] * (FXTRACK_N - 1));
        t->n--;
    }
    t->at[t->n] = at, t->yaw[t->n] = yaw;
    memcpy(t->pos[t->n], pos, sizeof t->pos[0]);
    t->n++;
}
// (slice 3) a place under anchor `anc`: another anchor than the track's = its old places mean nothing now -> start over
static inline void fxtrack_push_anc(FxTrack *t, int anc, uint64_t at, const float pos[3], float yaw) {
    if (t->n && t->anc != anc) t->n = 0;
    t->anc = (uint8_t)anc;
    fxtrack_push(t, at, pos, yaw);
}
// false = no place yet at time t (before the first: the fx stays where it spawned); after the last: hold it
static inline bool fxtrack_sample(const FxTrack *t, uint64_t at, float pos[3], float *yaw) {
    if (!t->n || at < t->at[0]) return false;
    int i = 0;
    while (i + 1 < t->n && t->at[i + 1] <= at) i++;
    if (i + 1 >= t->n) {
        memcpy(pos, t->pos[i], sizeof t->pos[0]), *yaw = t->yaw[i];
        return true;
    }
    double u = (double)(at - t->at[i]) / (double)(t->at[i + 1] - t->at[i]);
    for (int k = 0; k < 3; k++) pos[k] = (float)(t->pos[i][k] + (t->pos[i + 1][k] - t->pos[i][k]) * u);
    double d = fmod((double)t->yaw[i + 1] - t->yaw[i], NET_TWO_PI);
    if (d > NET_TWO_PI / 2) d -= NET_TWO_PI;
    if (d < -NET_TWO_PI / 2) d += NET_TWO_PI;
    *yaw = peer_wrap_2pi(t->yaw[i] + d * u);
    return true;
}

static inline PeerPose peer_sample(const NetPeer *p, uint64_t now_ms) {
    PeerPose q;
    memset(&q, 0, sizeof q);
    if (p->st == PEER_NONE || p->n == 0) return q;
    int64_t render = (int64_t)now_ms - NET_INTERP_MS;
    int i = -1;  // newest SNAP at or before render time
    for (int k = p->n - 1; k >= 0; k--)
        if ((int64_t)p->at[k] <= render) { i = k; break; }
    const NetSnap *a = &p->ring[i < 0 ? 0 : i];
    int64_t a_at = (int64_t)p->at[i < 0 ? 0 : i];
    q.valid = true;
    q.body = a->body, q.kb_chain = a->kb_chain, q.kb_alt = a->kb_alt, q.slot = a->slot;
    q.flags = a->flags, q.hp = a->hp, q.mp = a->mp, q.outfit = a->outfit;
    double since = render > a_at ? (double)(render - a_at) : 0.0;
    q.clock = (float)(a->clock + since * NET_KH_FPS / 1000.0);
    if (a->flags & NF_BONES) {
        q.bones_ok = true, q.bones = a->bones;
        q.bones.clock = (float)(a->bones.clock + since * NET_KH_FPS / 1000.0);
    }
    if (i >= 0 && i < p->n - 1) {
        const NetSnap *b = &p->ring[i + 1];
        double span = (double)(p->at[i + 1] - p->at[i]);
        double t = span > 0 ? since / span : 1.0;
        if (t > 1) t = 1;
        for (int k = 0; k < 3; k++) q.feet[k] = (float)(a->feet[k] + (b->feet[k] - a->feet[k]) * t);
        double d = fmod((double)b->yaw - a->yaw, NET_TWO_PI);
        if (d > NET_TWO_PI / 2) d -= NET_TWO_PI;
        if (d < -NET_TWO_PI / 2) d += NET_TWO_PI;
        q.yaw = peer_wrap_2pi(a->yaw + d * t);
        if (q.bones_ok && (b->flags & NF_BONES) && b->bones.slot == a->bones.slot) {  // (v8) same base motion: lerp the joints
            // (s170 review SF-1) the base clock between the two SNAPs, not pushed ahead: the puppet's held clock 0 stays 0 (no jitter)
            if (b->bones.clock >= a->bones.clock) q.bones.clock = (float)(a->bones.clock + (b->bones.clock - a->bones.clock) * t);
            for (int k = 0; k < NET_BONE_J; k++)
                for (int c = 0; c < 3; c++) q.bones.j[k][c] = (float)(a->bones.j[k][c] + (b->bones.j[k][c] - a->bones.j[k][c]) * t);
            for (int w = 0; w < 2; w++) {
                const float *pa = w ? a->bones.hl : a->bones.hu, *pb = w ? b->bones.hl : b->bones.hu;
                float *o = w ? q.bones.hl : q.bones.hu, v[3];
                for (int c = 0; c < 3; c++) v[c] = (float)(pa[c] + (pb[c] - pa[c]) * t);
                double L = sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);
                if (L > 1e-4) for (int c = 0; c < 3; c++) o[c] = (float)(v[c] / L);  // else o keeps a's axis
            }
        }
    } else {
        for (int k = 0; k < 3; k++) q.feet[k] = a->feet[k];
        q.yaw = peer_wrap_2pi(a->yaw);
    }
    if (p->st == PEER_LIVE) q.alpha = 1.0f;
    else {
        double f = 1.0 - (double)((int64_t)now_ms - (int64_t)(p->last_ms + NET_FADE_MS)) / NET_FADE_OUT_MS;
        q.alpha = (float)(f > 1 ? 1 : f < 0 ? 0 : f);
    }
    return q;
}

// (Task 6) a sampled clock past its motion's end, as sora.c's own motion end: a loop (flags & 1, frames > loop) wraps
// into [loop, frames), anything else holds at frames; frames <= 0 (motion unknown here) = as it is
static inline float peer_clock_wrap(float clock, int frames, int loop, bool loops) {
    if (clock < 0) return 0;
    if (frames <= 0 || clock < (float)frames) return clock;
    if (loops && frames > loop) return (float)loop + fmodf(clock - (float)frames, (float)(frames - loop));
    return (float)frames;
}

// (Task 6) the remote's mset trigger ranges for this frame (sora_sfx_motion fires prev < frame <= to): a new slot or the
// same motion restarted = (-1, clock]; a small step back (a fresh SNAP's clock a little behind the extrapolation, < 10
// KH frames) = nothing, prev kept; a loop wrap = (prev, frames] + (loop - eps, clock]. *keep = the prev to store.
#define PEER_TRIG_JITTER 10.0f
typedef struct { float from, to; } PeerRange;
static inline int peer_trigger_ranges(int prev_slot, float prev, int slot, float clock, int frames, int loop, bool loops,
                                      PeerRange out[2], float *keep) {
    *keep = clock;
    if (prev_slot != slot) { out[0].from = -1, out[0].to = clock; return 1; }
    if (clock >= prev) {
        if (clock == prev) return 0;
        out[0].from = prev, out[0].to = clock;
        return 1;
    }
    if (prev - clock < PEER_TRIG_JITTER) { *keep = prev; return 0; }
    if (loops && frames > loop && clock >= (float)loop) {
        out[0].from = prev, out[0].to = (float)frames;
        out[1].from = (float)loop - 0.001f, out[1].to = clock;
        return 2;
    }
    out[0].from = -1, out[0].to = clock;
    return 1;
}

// fresh (never played) events of one SNAP, each event seq exactly once per peer
static inline int peer_take_events(NetPeer *p, const NetSnap *s, NetEvent out[NET_MAX_EVENTS]) {
    int n = 0;
    for (int e = 0; e < s->nev && e < NET_MAX_EVENTS; e++)
        if (net_event_fresh(&p->evs, s->ev[e].seq)) out[n++] = s->ev[e];
    return n;
}

// ---- (mp4) the peer table: up to NET_MAX_PEERS remote players, one slot each (spec 2026-11-06-multiplayer-slice4 2.2) ----
#ifdef __linux__
#define NET_MAX_PEERS 1  // the Deck build stays a 2-player game
#else
#define NET_MAX_PEERS 3
#endif
#define NET_INQ 64       // incoming fresh events per peer waiting for the avatar's NET_INTERP_MS delay (Valor drive bursts)
#define NET_RFX_N 256    // his moving fx by event seq

typedef struct { NetEvent ev; uint64_t at; } NetInEvent;  // a fresh remote event + its arrival ms (avatar delays it)
// (Task 8) a remote fx spawned by event seq k: our copy's handle + 1 (0 = none), its places, a pending kill / fadeout
typedef struct { int h1; uint32_t gen; FxTrack tr; bool kill; uint8_t kflags; float fade; uint64_t kill_at; uint32_t born; } NetRemoteFx;
typedef struct {
    bool used;
    uint32_t ip; uint16_t port;     // where he sends from (loopback slot 0: 0 / 0)
    NetPeer p;                      // ring, event seqs seen, name, player id
    NetEdSeen fi;                   // (s176 v10) NET_FIRE packet seqs already applied
    NetEdSeen ed;                   // (s166 v6) NET_EDMG packet seqs already applied
    NetEdSeen rc;                   // (s194) NET_REACT packet seqs already taken
    NetEsSeen es; NetEvSeen esev;   // (s168 v7) NET_ESYNC: newest packet seq, sensor-event seqs already applied
    // (s191, Melty spec 4.1) NET_AREA: the map he says he is on (area_ms 0 = he never said: an old build = one world), its newest
    // seq; hidden = we are both in known, different areas (avatar / party / fx / FIRE / enemy election skip him); the last logged state
    char area[NET_AREA_MAX + 1]; uint64_t area_ms; NetEsSeen ars; bool hidden, area_said; char area_said_txt[NET_AREA_MAX + 1];
    NetInEvent inq[NET_INQ]; int ninq;
    NetRemoteFx rfx[NET_RFX_N];
    uint64_t joined_ms, last_recv_ms, last_snap_ms;  // (s187: last_snap_ms + st.snap = the "net diag" slot line)
    struct { uint32_t recv, stale, ev_fresh, ev_played, lost_in, snap; } st;
} NetPeerSlot;

// slot of (ip, port), else (same ip, same player id: his port changed), else -1
static inline int netslot_find(const NetPeerSlot *t, int n, uint32_t ip, uint16_t port, int player) {
    for (int i = 0; i < n; i++)
        if (t[i].used && t[i].ip == ip && t[i].port == port) return i;
    for (int i = 0; i < n && player >= 0; i++)
        if (t[i].used && t[i].ip == ip && t[i].p.st != PEER_NONE && t[i].p.player == player) return i;
    return -1;
}
// a free slot for (ip, port, player, name); -1 = full. The slot is wiped first (the glue freed his fx copies on leave).
static inline int netslot_join(NetPeerSlot *t, int n, uint32_t ip, uint16_t port, int player, const char *name, uint64_t now) {
    for (int i = 0; i < n; i++) {
        if (t[i].used) continue;
        memset(&t[i], 0, sizeof t[i]);
        t[i].used = true, t[i].ip = ip, t[i].port = port, t[i].joined_ms = t[i].last_recv_ms = now;
        peer_reset(&t[i].p, (uint8_t)player);
        peer_set_name(&t[i].p, name);
        return i;
    }
    return -1;
}
static inline int netslot_count(const NetPeerSlot *t, int n) {
    int c = 0;
    for (int i = 0; i < n; i++) c += t[i].used;
    return c;
}

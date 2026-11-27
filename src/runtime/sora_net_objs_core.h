// sora_net_objs_core.h (s289 objs): the pure parts of netbump / netobjs. No game, no clock, no socket inside (times come in
// as ms): host-tested by runtime/test/net_objs_test.c. The glue: sora_net_objs.h; the wire: NET_OBJS / NET_OBJEV in
// sora_net_proto.h.
// After Savage Wind PR #294 "Online mode" by byluki (fixes by mstampfli), MIT: the bump capsule (players_collide), the
// "whose an object is" rules (objs_update), the object kinds (obj_kind) and the session's broken list (world_add /
// world_rotate) of its runtime/online_actors.c, re-written for our transport (Melty rooms only) and our exe's lists.
// Parts 3-4: the level's mechanisms (movers_update: mover_kind) and the horse a friend rides (its "horse" piece).
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>   // (s291 coat: nh_slots_line's snprintf)
#include <string.h>
#include "sora_net_proto.h"  // (header-only, pure: net_q_from_m / net_m_from_q)

// ---------------- netbump: players bump into each other instead of walking through ----------------
#define NB_R 0.7f      // centres closer than this (BotW m) push apart (PR COLLIDE_R)
#define NB_H 1.7f      // only within this height of each other (PR COLLIDE_H)
#define NB_STEP 0.5f   // at most this far in one frame (PR)
#define NB_TIE 0.001f  // closer than this = on top of each other: the lower player id goes +x, the other -x
#define NB_FAST 0.25f  // our Link moved more than this in one frame (a knock-back, a warp, a fall): no push ...
#define NB_FAST_MS 500 // ... for this long after (GUESS: a duel's knock-down without ragdoll flies ~10 m/s)

// me = our Link's feet; at[i] = friend i's drawn feet (n of them), id[i] his player id, my_id ours. out = this frame's push
// (x, z) for OUR Link only (his own game pushes his). -> false = none (no friend inside the capsule)
// (PR: a tie set d = 1, which pushed 0.3 m the wrong way on both sides; here the whole radius, the same sides)
static inline bool nb_push(const float me[3], const float (*at)[3], const uint8_t *id, int n, uint8_t my_id, float out[2]) {
    double px = 0, pz = 0;
    out[0] = out[1] = 0;
    for (int i = 0; i < n; i++) {
        double dx = (double)me[0] - at[i][0], dy = (double)me[1] - at[i][1], dz = (double)me[2] - at[i][2];
        if (!(fabs(dy) <= NB_H)) continue;  // (NaN: no)
        double d = sqrt(dx * dx + dz * dz);
        if (!(d < NB_R)) continue;
        if (d < NB_TIE) {
            if (id[i] != my_id) px += my_id < id[i] ? NB_R : -NB_R;  // (the same id: no side to pick)
            continue;
        }
        double in = NB_R - d;
        px += dx / d * in, pz += dz / d * in;
    }
    double L = sqrt(px * px + pz * pz);
    if (!(L >= 0.001)) return false;
    if (L > NB_STEP) px *= NB_STEP / L, pz *= NB_STEP / L;
    out[0] = (float)px, out[1] = (float)pz;
    return true;
}

// why our Link is not pushed this frame (0 = he may be). Link's state word bit 0x100000 = riding (sora.c LINK_SW_RIDING, GUESS
// there); state word 0 / 0x1 = a talk / pickup / scripted stretch / warp / story event (the core of sora.c's g_event)
enum { NBW_OK, NBW_DOWNED, NBW_DEAD, NBW_LOADING, NBW_RIDING, NBW_EVENT, NBW_RAGDOLL, NBW_FAST, NBW_N };
static const char *const NBW_TXT[NBW_N] = {"ok", "DOWNED", "dead", "loading", "riding", "event", "ragdoll / knock-down", "flying (knock-back / warp)"};
#define NB_SW_RIDING 0x100000u
static inline int nb_why(bool downed, bool dead, bool loading, uint32_t state_word, int ragdoll, bool fast) {
    if (downed) return NBW_DOWNED;
    if (dead) return NBW_DEAD;
    if (loading) return NBW_LOADING;
    if (state_word & NB_SW_RIDING) return NBW_RIDING;
    if (state_word == 0 || state_word == 1) return NBW_EVENT;
    if (ragdoll > 0) return NBW_RAGDOLL;
    if (fast) return NBW_FAST;
    return NBW_OK;
}

// ---------------- netobjs: map objects that move or break the same in every world ----------------
#define NO_NEAR 20.0f      // objects this near our Link are watched for our moves (PR OBJ_NEAR)
#define NO_FAR 100.0f      // his records are matched to ours this near (PR OBJ_FAR)
#define NO_TRACK 8         // at most this many of ours watched (the nearest; PR)
#define NO_MOVE_M 0.02f    // moved more than this since the last sample = moving (PR: 2 cm)
#define NO_STILL 20        // still samples (~1 s at 20 Hz) and ours is sent no more (PR)
#define NO_TIE_MS 300      // both began within this: the lower player id keeps it (PR OBJ_TIE_NS 0.3 s)
#define NO_GAP_MS 500      // his records this far apart = a new stream of his (PR)
#define NO_QUIET_MS 700    // after we took it to his place: its moves are his, not ours to send (PR OBJ_QUIET_NS)
#define NO_HOLD 90         // frames we keep taking it to his last place after his last record (PR OBJ_HOLD)
#define NO_HELD_MS 4000    // one he holds (Magnesis, Stasis) is his this long after his last word of it (PR: 4 s after a release)
#define NO_SNAP_M 0.5f     // at the end of a hold, farther than this from his place = put there (PR)

// which actors are map objects that may move (PR obj_kind): no enemy / NPC / item / weapon / animal / player / the level's
// mechanisms (netmovers' kinds) / water / walls
static inline bool no_kind(const char *n) {
    if (!n || !n[0]) return false;
    if (!strncmp(n, "Enemy_", 6) || !strncmp(n, "Npc", 3) || !strncmp(n, "Online", 6) || !strncmp(n, "Item_", 5) ||
        !strncmp(n, "Weapon_", 7) || !strncmp(n, "Animal_", 7) || !strncmp(n, "GameRom", 7) || !strcmp(n, "PlayerObj"))
        return false;
    static const char *const fixed[] = {"Lift", "Elevator", "Gear", "Door", "Shutter", "Water", "Fence", "Gate", "Bridge",
                                        "Rotate", "Slide", "Move", "Floor", "Wall", "Propeller", "Windmill"};
    for (size_t k = 0; k < sizeof fixed / sizeof *fixed; k++)
        if (strstr(n, fixed[k])) return false;
    return true;
}

// one object's "whose is it" state, per HashId
typedef struct {
    bool sending; uint64_t send_t; int still;  // ours: sent since send_t; still samples in a row
    uint64_t his_start, his_seen;              // his stream: began, his last record (0 = never)
    uint64_t quiet_until, his_until;           // its moves are his until quiet_until; his (he holds it) until his_until
    int hold;                                  // frames left taking it to his place
    uint8_t his_id;                            // whose records we follow
    float his_m[12];                           // his last matrix (row-major 3x4, translation at 3, 7, 11)
} NoOwn;

// his record of it came (now ms); held = he holds it (netshots_friend_holds, or his record's HELD flag).
// -> true = his: we follow it (his_m kept, hold refilled, ours stops); false = ours: his record dropped (we began first, or
// both within NO_TIE_MS and our player id is lower)
static inline bool no_his(NoOwn *o, uint64_t now, uint8_t my_id, uint8_t his_id, bool held, const float m[12]) {
    if (!o->his_seen || now - o->his_seen > NO_GAP_MS) o->his_start = now;
    o->his_seen = now ? now : 1;
    if (held) o->his_until = now + NO_HELD_MS;
    bool his_hold = held || now < o->his_until;
    if (!his_hold && o->sending && (o->send_t + NO_TIE_MS < o->his_start || (o->send_t < o->his_start + NO_TIE_MS && my_id < his_id)))
        return false;
    o->sending = false, o->still = 0, o->hold = NO_HOLD, o->quiet_until = now + NO_QUIET_MS, o->his_id = his_id;
    memcpy(o->his_m, m, sizeof o->his_m);
    return true;
}

// (s290 rv3fix, review 3 #3) a kind-1 record of his must be near where our copy of the object is (or, in a stream, near his
// last record of it, which our copy is following): a HashId of another map / a bad sender / a lost chain of packets is refused,
// a real push or fall is not (20 Hz records: a 50 m/s fall = 2.5 m a record; the breaks' identity test NO_POS_M is 5 m).
// NO_REC_MAX_M = 10 m is a GUESS (twice NO_POS_M: a few lost records of a fast fall still pass).
#define NO_REC_MAX_M 10.0f
static inline bool no_rec_within(const float ref[3], const float m[12], float r) {
    float dx = m[3] - ref[0], dy = m[7] - ref[1], dz = m[11] - ref[2];
    return dx * dx + dy * dy + dz * dz <= r * r;  // (NaN = not near)
}
static inline bool no_rec_near(const float ref[3], const float m[12]) { return no_rec_within(ref, m, NO_REC_MAX_M); }
// (s290 revfix, review F3) which of his kind-1 records are taken. The 10 m rule above kept an object apart for good: a boulder he
// rolled 30 m while we were farther than NO_FAR stood at its old place here; his next push began 30 m from our copy and every
// record of it was refused. Now: inside a running stream (his last record that passed, NO_GAP_MS ago at most) a record must be
// within NO_REC_MAX_M of that one (a jump inside a stream = refused); the first record of a stream (none passed for NO_GAP_MS)
// must be within NO_FAR of our copy (the distance his records are matched to ours at all; ours = NULL: unreadable = refused).
// A refused record does not keep the stream going: after NO_GAP_MS of refusals the next one is a first record again.
typedef struct { float p[3]; uint64_t at; } NoRxNear;  // (his last record that passed, when: 0 = none)
static inline bool no_rec_take(NoRxNear *s, uint64_t now, const float *ours, const float m[12], bool *in_stream) {
    bool stream = s->at && now >= s->at && now - s->at <= NO_GAP_MS;
    if (in_stream) *in_stream = stream;
    bool ok = stream ? no_rec_within(s->p, m, NO_REC_MAX_M) : ours && no_rec_within(ours, m, NO_FAR);
    if (ok) s->p[0] = m[3], s->p[1] = m[7], s->p[2] = m[11], s->at = now ? now : 1;
    return ok;
}
// (s290 rv3fix, review 3 #4) NET_OBJS' seq per sender slot: one NetEsSeen (net_esseq_fresh, as ESYNC / shots). The two packets of a
// tick ride two lanes and hold different objects, and an object's REST record is sent once, so a stale packet is not dropped whole:
// no_rx_rec says, per kind-1 record, whether the object was already heard of in a NEWER (or this) packet (-> drop it) or not
// (-> it is still his last word of it: take it). A table of the last NO_RXH_N HashIds with their newest seq; a slot's table starts
// over when the slot is another peer (joined) or he restarted (seq far behind, net_esseq_fresh's own rule).
#define NO_RXH_N 32
typedef struct { NetEsSeen seen; uint64_t joined; uint32_t h[NO_RXH_N]; uint16_t sq[NO_RXH_N]; int next; } NoRx;
static inline bool no_rx_begin(NoRx *r, uint64_t joined, uint16_t sq) {  // -> the packet is the newest so far
    if (r->joined != joined) memset(r, 0, sizeof *r), r->joined = joined;
    bool had = r->seen.any != 0;
    uint16_t was = r->seen.last;
    bool fresh = net_esseq_fresh(&r->seen, sq);
    if (fresh && had && (uint16_t)(sq - was) >= 0x8000) memset(r->h, 0, sizeof r->h), memset(r->sq, 0, sizeof r->sq), r->next = 0;  // (restart)
    return fresh;
}
static inline bool no_rx_rec(NoRx *r, uint32_t h, uint16_t sq, bool fresh) {  // -> take this record
    int at = -1;
    for (int i = 0; i < NO_RXH_N && at < 0; i++) if (r->h[i] == h) at = i;
    if (!fresh && at >= 0 && (uint16_t)(r->sq[at] - sq) < 0x8000) return false;  // (heard in a newer / this packet already)
    if (at < 0) at = r->next, r->next = (r->next + 1) % NO_RXH_N, r->h[at] = h;
    r->sq[at] = sq;
    return true;
}

// our sample of it (20 Hz): moved = metres since the last sample; held_by_him = netshots_friend_holds now.
// -> true = send our record of it now (while it moves, then until NO_STILL still samples: the last one = where it rests)
static inline bool no_ours(NoOwn *o, uint64_t now, float moved, bool held_by_him) {
    if (held_by_him || now < o->his_until || now < o->quiet_until || o->hold > 0) { o->sending = false, o->still = 0; return false; }
    if (moved > NO_MOVE_M) {
        if (!o->sending) o->sending = true, o->send_t = now;
        o->still = 0;
        return true;
    }
    if (!o->sending) return false;
    if (++o->still >= NO_STILL) o->sending = false;
    return true;
}

// every frame: -> 0 nothing, 1 take it towards his place (through its body), 2 the same, the hold's last frame (then: put it
// at his place when farther than NO_SNAP_M). While it follows, its own moves are his (quiet)
static inline int no_follow(NoOwn *o, uint64_t now) {
    if (o->hold <= 0) return 0;
    o->quiet_until = now + NO_QUIET_MS;
    return --o->hold ? 1 : 2;
}

// (s289 objs fix, lab 2:30 am: an old barrel broke in copy2's world as it followed PC's push, and copy2 told the room) the
// objects we took towards a friend's place lately, by HashId: one that goes while we follow it or within NO_FOLLOW_GRACE_MS
// after did not break by our hand (our following, or something of his): never told to the room as our break.
#define NO_FOLLOWED_N 32
#define NO_FOLLOW_GRACE_MS 1000
typedef struct { uint32_t h[NO_FOLLOWED_N]; uint64_t until[NO_FOLLOWED_N]; } NoFollowed;
static inline void nf_note(NoFollowed *f, uint32_t h, uint64_t now) {  // (every frame we follow it / put it there)
    if (!h) return;
    int at = -1;
    for (int i = 0; i < NO_FOLLOWED_N && at < 0; i++) if (f->h[i] == h) at = i;
    for (int i = 0; i < NO_FOLLOWED_N && at < 0; i++) if (!f->h[i] || now >= f->until[i]) at = i;
    if (at < 0) {  // (full: the one that ends first goes)
        at = 0;
        for (int i = 1; i < NO_FOLLOWED_N; i++) if (f->until[i] < f->until[at]) at = i;
    }
    f->h[at] = h, f->until[at] = now + NO_FOLLOW_GRACE_MS;
}
static inline bool nf_recent(const NoFollowed *f, uint32_t h, uint64_t now) {
    for (int i = 0; h && i < NO_FOLLOWED_N; i++) if (f->h[i] == h && now < f->until[i]) return true;
    return false;
}
// A breakable of BotW's (its DamageParam's Breakable set) is not taken through its body any more: the lab's old barrel (GParam
// Life 1) broke at once; Barrel (Life 20) and Kibako_Contain_01 (40) take the same contact-impulse damage (the player's romfs:
// DamageRate Impulse 1.0, IsCommonCalcImpuleDamage, no fall damage). It stays where it is while his moves, and is put at his
// place once his rests (his last record, NORF_REST) or the hold ends, when ours is this far off his (not for a jitter).
#define NO_REST_PUT_M 0.05f
#define NO_REST_PUT_ROT 0.05f  // (a rotation entry: about 3 degrees)
static inline bool no_put_off(const float cur[12], const float his[12]) {
    for (int k = 0; k < 12; k++)
        if (!isfinite(his[k])) return false;  // (never put at a bad place; net_unpack_objs drops those already)
    float dx = cur[3] - his[3], dy = cur[7] - his[7], dz = cur[11] - his[11];
    if (!(dx * dx + dy * dy + dz * dz <= NO_REST_PUT_M * NO_REST_PUT_M)) return true;  // (NaN = off)
    for (int k = 0; k < 12; k++) {
        if (k == 3 || k == 7 || k == 11) continue;
        float d = cur[k] - his[k];
        if (!(d <= NO_REST_PUT_ROT && d >= -NO_REST_PUT_ROT)) return true;
    }
    return false;
}
// (s289 objs fix 2, lab D2 3:05 am: the put moved the actor's matrix only, its resting body put it back) the put is BotW's
// own warp now (sora_net_objs.h no_warp). Never into something: while our Link (NO_PUT_CLEAR in x z, NO_PUT_LINK_Y up or
// down) or another bodied map object (NO_PUT_CLEAR between the two places) stands where it would go, it waits (GUESS: a
// warp into another body = a contact impulse, and an old barrel breaks at Life 1), at most NO_PUT_WAIT_MS, then it is not put.
// -> -1 clear, 0 our Link, 1 + j = o[j] (a NaN place is nothing in the way)
#define NO_PUT_CLEAR 0.9f     // m (GUESS: an old barrel is about 0.7 m across)
#define NO_PUT_LINK_Y 2.0f
#define NO_PUT_WAIT_MS 10000
#define NO_PUT_CHECK_MS 500   // where it is this long after the put (one line for the lab)
#define NO_PUT_STAYED_M 0.1f  // that near his place then = the warp held
static inline int no_put_block(const float to[3], const float *link, const float (*o)[3], int n) {
    if (link) {
        float dx = link[0] - to[0], dy = link[1] - to[1], dz = link[2] - to[2];
        if (dx * dx + dz * dz < NO_PUT_CLEAR * NO_PUT_CLEAR && dy < NO_PUT_LINK_Y && dy > -NO_PUT_LINK_Y) return 0;
    }
    for (int j = 0; j < n; j++) {
        float dx = o[j][0] - to[0], dy = o[j][1] - to[1], dz = o[j][2] - to[2];
        if (dx * dx + dy * dy + dz * dz < NO_PUT_CLEAR * NO_PUT_CLEAR) return 1 + j;
    }
    return -1;
}
// (s289 objs fix 3, lab room s289p 3:49 am: Barrel 0x7f4657de put at PC2's last place, 65 m down a cliff, landed on a slope and
// rolled 22 m on; then it went to the room as ours, while PC2's was gone: the worlds ended 22 m apart) after our warp-put its own
// moves are part of the put: not ours, not sent, until it rests (NO_STILL still samples), at most NO_SETTLE_MS, or until our
// Link comes within NO_SETTLE_LINK_M of it (then its moves are ours again: our hand may move it). At rest within NO_SETTLE_OK_M
// of his place = kept there; farther, and his place was where his rests (his record said so): put there once more; else kept
// where it rests, not sent (his last record was no rest: his may be gone, a break far from his Link is not told).
#define NO_SETTLE_MS 8000
#define NO_SETTLE_LINK_M 2.0f  // (GUESS: a Keyblade swing reaches about 2 m)
#define NO_SETTLE_OK_M 1.0f    // (GUESS: a barrel is about 0.7 m across)
#define NO_REPUTS 1
typedef struct { bool on; uint64_t until; int still; } NoSettle;
enum { NOS_ON, NOS_RESTS, NOS_LATE, NOS_LINK };
static inline void nos_start(NoSettle *s, uint64_t now) { s->on = true, s->until = now + NO_SETTLE_MS, s->still = 0; }
// one 20 Hz sample while it settles: moved = metres since the last sample (NaN = moved), link_d = our Link to it (NaN = not
// known: not near). -> NOS_ON still settling; NOS_RESTS at rest now; NOS_LATE still moving at NO_SETTLE_MS; NOS_LINK our Link
// at it (each but NOS_ON ends it)
static inline int nos_sample(NoSettle *s, uint64_t now, float moved, float link_d) {
    if (!s->on) return NOS_LATE;
    if (link_d < NO_SETTLE_LINK_M) { s->on = false; return NOS_LINK; }
    if (now >= s->until) { s->on = false; return NOS_LATE; }
    if (!(moved <= NO_MOVE_M)) s->still = 0;
    else if (++s->still >= NO_STILL) { s->on = false; return NOS_RESTS; }
    return NOS_ON;
}
// at rest d m from his place: 0 kept (near enough), 1 put there once more (his place is where his rests, not put again yet),
// 2 kept where it rests (NaN d: not near)
static inline int nos_pick(float d, bool his_rest, int reputs) {
    if (d <= NO_SETTLE_OK_M) return 0;
    return his_rest && reputs < NO_REPUTS ? 1 : 2;
}

// is a watched object that is gone now one that broke here? gone = its actor was deleted (its proc unit no longer holds it);
// d_link = our Link to where it was, travel = how far our Link went since it was last seen; a tree also needs a fresh trunk
// (BotW's felled one) at its foot. (PR breaks_update: 15 m, 10 m; its stump test is our trunk's root end)
#define NO_BREAK_NEAR 15.0f
#define NO_BREAK_TRAVEL 10.0f
#define NO_TRUNK_FOOT 1.5f   // a fresh trunk's root end this near the tree's place (GUESS: the PR's stump test was 1 m)
#define NO_TRUNK_FRESH 4000  // ms: "fresh" = the trunk list saw it first this recently
static inline bool no_broke(bool loading, bool tree, float d_link, float travel, bool trunk_at_foot) {
    if (loading || !(d_link < NO_BREAK_NEAR) || !(travel < NO_BREAK_TRAVEL)) return false;
    return !tree || trunk_at_foot;
}

// ---------------- the session's broken list (PR "world"): what broke in the room while it lasts ----------------
#define NW_MAX 2048
#define NW_IDX 4096           // open-address slots (a power of two, > 2 x NW_MAX)
#define NW_ROTATE_MS 1500     // one entry re-told this often (PR WORLD_ROTATE_NS) ...
#define NW_ROTATE_QUIET 3     // ... once no new break came for 3 x that (PR)
enum { NOK_BREAK = 1, NOK_TREE = 2 };
typedef struct { uint32_t h; uint8_t kind; int8_t dir[2]; float pos[3]; } NwEnt;  // dir: a tree's fall (x, z) x 127
typedef struct { NwEnt e[NW_MAX]; int n, next; uint16_t idx[NW_IDX]; uint64_t new_t, rot_t; } NoWorld;

static inline uint32_t nw_slot(uint32_t h) { return (uint32_t)(h * 2654435761u) >> 20; }  // (12 bits: 0..4095)
static inline int nw_find(const NoWorld *w, uint32_t h) {
    if (!h) return -1;
    uint32_t s = nw_slot(h);
    for (int k = 0; k < NW_IDX; k++, s = (s + 1) & (NW_IDX - 1)) {
        int i = (int)w->idx[s] - 1;
        if (i < 0) return -1;
        if (w->e[i].h == h) return i;
    }
    return -1;
}
// -> 1 added (a new one), 0 already on the list, -1 full / HashId 0
static inline int nw_add(NoWorld *w, const NwEnt *e, uint64_t now) {
    if (!e || !e->h) return -1;
    if (nw_find(w, e->h) >= 0) return 0;
    if (w->n >= NW_MAX) return -1;
    uint32_t s = nw_slot(e->h);
    while (w->idx[s]) s = (s + 1) & (NW_IDX - 1);
    w->e[w->n] = *e, w->idx[s] = (uint16_t)(w->n + 1), w->n++;
    w->new_t = now;
    return 1;
}
// -> the entry to re-tell now (a late joiner learns the list this way), NULL = not now
static inline const NwEnt *nw_rotate(NoWorld *w, uint64_t now) {
    if (!w->n || now - w->new_t < (uint64_t)NW_ROTATE_QUIET * NW_ROTATE_MS || now - w->rot_t < NW_ROTATE_MS) return NULL;
    w->rot_t = now;
    if (w->next >= w->n || w->next < 0) w->next = 0;
    return &w->e[w->next++];
}
static inline void nw_clear(NoWorld *w) { memset(w, 0, sizeof *w); }

// a tree's fall direction (x, z, any length) <-> 2 signed bytes; (0, 0) = not known (the receiver fells it away from his Sora)
static inline void no_dir_enc(float x, float z, int8_t out[2]) {
    double L = sqrt((double)x * x + (double)z * z);
    if (!(L > 1e-4)) { out[0] = out[1] = 0; return; }
    out[0] = (int8_t)lrint(x / L * 127), out[1] = (int8_t)lrint(z / L * 127);
}

// ---------------- (part 3) netmovers: the level's mechanisms move the same in every world ----------------
// PR movers_update: what a level moves itself (lifts, elevators, doors, shutters, gears, bridges, turning / sliding floors,
// propellers, gates, a Divine Beast's parts...), each game moving its own by its own logic; one a player set going moves in
// his world alone. Ours that moves (or changes its AS) goes to the room; theirs is taken where his is and plays his AS.
#define NM_NEAR 80.0f        // mechanisms this near our Link are watched (PR MOVER_NEAR)
#define NM_TRACK 24          // at most this many watched (PR MOVER_TRACK 48)
#define NM_SEND 6            // ours sent: the nearest this many that move (PR ONLINE_MOVERS)
#define NM_QUIET 30          // samples (20 Hz) without a move / an AS change and ours is sent no more (PR: 90 frames = 1.5 s)
#define NM_THEIRS_MS 700     // his took it here this recently: its moves are his, not ours to send (PR MOVER_THEIRS_NS)
#define NM_INTERP_MS 120     // his records are drawn this far behind (= NET_INTERP_MS, his Sora's: they keep in step)
#define NM_HOLD_MS 1500      // after his last record: held at his last place this long, then it is ours again
#define NM_LINK_R 6.0f       // "our Link is at it": within this in x z of its place (or where his record takes it) ...
#define NM_LINK_BELOW 2.0f   // ... from this far under it ...
#define NM_LINK_ABOVE 6.0f   // ... to this far over it (GUESS: a lift's place is its platform's; most are < 12 m wide)
#define NM_RIDE_STEP 0.05f   // netmoverride 1: one our Link is at moves at most this far a frame (3 m/s at 60 fps) ...
#define NM_RIDE_RAD 0.03f    // ... and turns at most this much a frame (1.7 deg), through its body only (never put there)
#define NM_LINK_R_BIG 220.0f // a Divine Beast and its parts (Remains*, Link inside them; 1.9.0 ActorInfo aabbs: RemainsWind 282 x 91
#define NM_LINK_BIG_Y 150.0f //   x 219 m, its barrier 345 x 104 x 245 m): at it within 220 m x z, 150 m under / over (GUESS: place ~ middle)
#define NM_SAFE_R 30.0f      // our Link this near one he moves (not at it): his moves of it never jump, they step ...
#define NM_NEAR_STEP 0.2f    // ... at most this far a frame (12 m/s at 60 fps; a lift on its way moves a few cm a frame) ...
#define NM_NEAR_RAD 0.05f    // ... and turn at most this much (2.9 deg); farther than NM_SAFE_R: his place at once (PR)
#define NM_RING 4

// which actors are the level's mechanisms (PR mover_kind, as is)
static inline bool nm_kind(const char *n) {
    if (!n || !n[0]) return false;
    if (!strncmp(n, "Enemy_", 6) || !strncmp(n, "Npc", 3) || !strncmp(n, "Online", 6) || !strncmp(n, "Item_", 5) ||
        !strncmp(n, "Weapon_", 7) || !strncmp(n, "Animal_", 7) || !strncmp(n, "GameRom", 7) || !strcmp(n, "PlayerObj") ||
        strstr(n, "Far") || strstr(n, "_Stump") || strstr(n, "_Trunk") || strstr(n, "Tag") || strstr(n, "LiftRock") ||
        strstr(n, "StoneLift"))  // (PR: tags do not move; the rocks to lift are the objects')
        return false;
    static const char *const kinds[] = {"Lift", "Elevator", "Gear", "Door", "Shutter", "Bridge", "Rotate", "Slide", "Move", "Floor",
                                        "Propeller", "Windmill", "Gate", "Remains", "Wheel", "Crane", "Piston", "Seesaw", "Turn",
                                        "Lever", "Swing", "Pendulum", "Waterwheel", "Fan"};
    for (size_t k = 0; k < sizeof kinds / sizeof *kinds; k++)
        if (strstr(n, kinds[k])) return true;
    return false;
}
// moved between two of its samples (PR mtx_moved: 1 cm, 0.002 of a rotation entry); NaN = moved
static inline bool nm_moved(const float a[12], const float b[12]) {
    for (int k = 0; k < 12; k++) {
        float d = a[k] - b[k], tol = (k == 3 || k == 7 || k == 11) ? 0.01f : 0.002f;
        if (!(d <= tol && d >= -tol)) return true;
    }
    return false;
}
// a Divine Beast or one of its parts (big: our Link is at it from much farther)
static inline bool nm_big(const char *n) { return n && strstr(n, "Remains") != NULL; }
// our Link is at a mechanism whose matrix is m (on it, under it, beside it)
static inline bool nm_link_at(const float L[3], const float m[12], bool big) {
    float dx = L[0] - m[3], dy = L[1] - m[7], dz = L[2] - m[11];
    float r = big ? NM_LINK_R_BIG : NM_LINK_R, lo = big ? NM_LINK_BIG_Y : NM_LINK_BELOW, hi = big ? NM_LINK_BIG_Y : NM_LINK_ABOVE;
    return dx * dx + dz * dz < r * r && dy > -lo && dy < hi;  // (NaN: not at it)
}
// our Link is within NM_SAFE_R of its place here or of where his record takes it (NaN: near, the safe side)
static inline bool nm_link_near(const float L[3], const float a[12], const float b[12]) {
    float da = (L[0] - a[3]) * (L[0] - a[3]) + (L[1] - a[7]) * (L[1] - a[7]) + (L[2] - a[11]) * (L[2] - a[11]);
    float db = (L[0] - b[3]) * (L[0] - b[3]) + (L[1] - b[7]) * (L[1] - b[7]) + (L[2] - b[11]) * (L[2] - b[11]);
    return !(da >= NM_SAFE_R * NM_SAFE_R) || !(db >= NM_SAFE_R * NM_SAFE_R);
}

// one mechanism's "whose is it" state (PR: quiet, theirs_t; the lower player number keeps one both move)
typedef struct { int quiet; uint64_t theirs_ms; uint8_t his_id; } NmOwn;
// his record of it came: -> true = take it (his place, his AS), false = ours: we move it now and our player id is lower
static inline bool nm_take(NmOwn *o, uint64_t now, uint8_t my_id, uint8_t his_id) {
    if (o->quiet < NM_QUIET && my_id < his_id) return false;
    o->theirs_ms = now ? now : 1, o->quiet = NM_QUIET, o->his_id = his_id;
    return true;
}
// our 20 Hz sample of it: changed = it moved / changed its AS since the last sample. -> true = send ours now
static inline bool nm_ours(NmOwn *o, uint64_t now, bool changed) {
    if (o->theirs_ms && now - o->theirs_ms < NM_THEIRS_MS) return false;  // (his: our game shows his moves)
    o->theirs_ms = 0;
    o->quiet = changed ? 0 : o->quiet < NM_QUIET ? o->quiet + 1 : NM_QUIET;
    return o->quiet < NM_QUIET;
}

// his records of it, newest last (receive times, ms): drawn NM_INTERP_MS behind, as his Sora
typedef struct { uint64_t t[NM_RING]; float q[NM_RING][4], p[NM_RING][3]; int n; } NmRing;
static inline void nm_ring_push(NmRing *r, uint64_t now, const float m[12]) {
    if (r->n && now < r->t[r->n - 1]) r->n = 0;  // (a clock that went back: start over)
    if (r->n == NM_RING) {
        memmove(&r->t[0], &r->t[1], sizeof r->t[0] * (NM_RING - 1)), memmove(&r->q[0], &r->q[1], sizeof r->q[0] * (NM_RING - 1));
        memmove(&r->p[0], &r->p[1], sizeof r->p[0] * (NM_RING - 1)), r->n--;
    }
    net_q_from_m(m, r->q[r->n]);
    r->p[r->n][0] = m[3], r->p[r->n][1] = m[7], r->p[r->n][2] = m[11], r->t[r->n] = now;
    r->n++;
}
// where it is at time t: between the two records around t (place lerp, rotation nlerp), the first before it, the last after
static inline bool nm_ring_at(const NmRing *r, uint64_t t, float out[12]) {
    if (!r->n) return false;
    int i = 0;
    while (i + 1 < r->n && r->t[i + 1] <= t) i++;
    if (i + 1 >= r->n || t <= r->t[i]) { net_m_from_q(r->q[i], r->p[i], out); return true; }
    double u = (double)(t - r->t[i]) / (double)(r->t[i + 1] - r->t[i]);
    const float *a = r->q[i], *b = r->q[i + 1];
    double s = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3] < 0 ? -1 : 1, q[4], L = 0;
    for (int k = 0; k < 4; k++) q[k] = a[k] + (s * b[k] - a[k]) * u, L += q[k] * q[k];
    L = sqrt(L);
    float qf[4], pf[3];
    for (int k = 0; k < 4; k++) qf[k] = (float)(L > 1e-9 ? q[k] / L : (k == 3));
    for (int k = 0; k < 3; k++) pf[k] = (float)(r->p[i][k] + (r->p[i + 1][k] - r->p[i][k]) * u);
    net_m_from_q(qf, pf, out);
    return true;
}
// one frame from cur towards want: its place at most max_m on, its rotation at most max_rad on (netmoverride: one our Link
// is at never jumps; one he is near never jumps either). -> true = held back (want not reached this frame). A non-finite cur
// (not seen; want is a checked record's): want at once
static inline bool nm_step(const float cur[12], const float want[12], float max_m, float max_rad, float out[12]) {
    for (int j = 0; j < 12; j++)
        if (!isfinite(cur[j])) { memcpy(out, want, sizeof(float) * 12); return false; }
    float qa[4], qb[4], t[3];
    net_q_from_m(cur, qa), net_q_from_m(want, qb);
    double dx = want[3] - cur[3], dy = want[7] - cur[7], dz = want[11] - cur[11], d = sqrt(dx * dx + dy * dy + dz * dz);
    double k = d > max_m ? max_m / d : 1;
    t[0] = (float)(cur[3] + dx * k), t[1] = (float)(cur[7] + dy * k), t[2] = (float)(cur[11] + dz * k);
    double dot = qa[0] * qb[0] + qa[1] * qb[1] + qa[2] * qb[2] + qa[3] * qb[3], s = dot < 0 ? -1 : 1;
    dot = fabs(dot);
    double ang = 2 * acos(dot > 1 ? 1 : dot), u = ang > max_rad ? max_rad / ang : 1, q[4], L = 0;
    for (int j = 0; j < 4; j++) q[j] = qa[j] + (s * qb[j] - qa[j]) * u, L += q[j] * q[j];
    L = sqrt(L);
    float qf[4];
    for (int j = 0; j < 4; j++) qf[j] = (float)(L > 1e-9 ? q[j] / L : (j == 3));
    net_m_from_q(qf, t, out);
    return k < 1 || u < 1;
}

// ---------------- (part 4) nethorse: the horse a friend rides, seen under him; netbump leaves a rider alone ----------------
// PR: while his Link's AS is a horse's, the player's horse nearest him (3 m), its saddle and reins (1.5 m from it); the others'
// games make a model-only actor of it (OnlineHorse_<horse>, from the player's own game files at install) where his is,
// playing the AS his plays.
#define NH_NEAR 3.0f         // the ridden horse: the nearest horse actor this near our Link (PR)
#define NH_TACK 1.5f         // its saddle / reins: the nearest this near it (PR)
#define NH_LOOK_MS 333       // looked for this often while riding (PR: every 20 frames), at once when lost
#define NH_FRESH_MS 600      // a friend's horse record this recent = he rides (netbump leaves him alone)
#define NH_GONE_MS 1500      // no record of a part this long: his puppet of it is put away (his pool)
#define NH_PARTS 3           // the horse, its saddle, its reins
#define NH_MAP_N 64

// a horse one rides (PR: GameRomHorse*, not the saddles / reins; the Master Cycle)
static inline bool nh_horse(const char *n) {
    return n && ((!strncmp(n, "GameRomHorse", 12) && !strstr(n, "Reins") && !strstr(n, "Saddle")) || !strcmp(n, "GameROMMotorcycle"));
}
// 0 a saddle, 1 reins, -1 neither (PR)
static inline int nh_tack(const char *n) {
    return !n ? -1 : !strncmp(n, "GameRomHorseSaddle_", 19) ? 0 : !strncmp(n, "GameRomHorseReins_", 18) ? 1 : -1;
}
// his Link's AS (slot 0) is a rider's (PR: "Horse" or "Motorcycle" in its name)
static inline bool nh_riding_as(const char *as) { return as && (strstr(as, "Horse") || strstr(as, "Motorcycle")); }
// zlib's crc32 (python's zlib.crc32 = the bake's horse.txt and BotW's own name hashes)
static inline uint32_t nh_crc32(const char *s) {
    uint32_t c = 0xFFFFFFFFu;
    for (; s && *s; s++) {
        c ^= (uint8_t)*s;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}
// part of a kind-3 record's flags <-> 0 horse, 1 saddle, 2 reins
static inline int nh_part(uint8_t flags) { int p = (flags & NORH_PART) >> 2; return p < NH_PARTS ? p : -1; }
// (s292 coopfix) a friend slot's horse records (have = any kept: his player byte / a part's time) are stale once that slot is
// free or someone else's: forgotten then (lab s291d F6: a "slot 0 (player 2) ... last record 8343 ms ago" diag row stayed for a
// friend who had left)
static inline bool nh_rx_stale(bool have, uint8_t rx_player, bool slot_used, uint8_t slot_player) {
    return have && (!slot_used || slot_player != rx_player);
}
// (s292 coopfix, live `set nethorsefar N`, 0 = off = as before) lab s292a: while the rider was far (~1840 m, again ~330 m) our game
// removed his horse puppet about once a second and we made it again at once ("made 103 ... removed by the game 96"). GUESS: BotW
// deletes an actor that stands outside its loaded area. Two rules: (1) his horse farther than N m from our Link: no puppet (to his
// pool), back once nearer than NH_FAR_IN of N (no flicker at the edge); (2) a puppet the game removed is asked for again only after
// a wait that doubles each time in a row (NH_RM_WAIT_MS .. NH_RM_WAIT_MAX_MS); a puppet that lives NH_RM_STABLE_MS starts the count over
#define NH_FAR_IN 0.8f
#define NH_RM_WAIT_MS 2000
#define NH_RM_WAIT_MAX_MS 30000
#define NH_RM_STABLE_MS 10000
// far now? was = far at the last look, d = his horse's metres from our Link (NaN / not known: as it was), far_m = N (<= 0: never)
static inline bool nh_far(bool was, float d, float far_m) {
    if (!(far_m > 0)) return false;
    if (!(d == d)) return was;
    return was ? !(d < far_m * NH_FAR_IN) : d > far_m;
}
// the wait after the n-th removal in a row (0: none)
static inline uint32_t nh_rm_wait_ms(int n) {
    if (n <= 0) return 0;
    uint32_t w = NH_RM_WAIT_MS;
    for (int i = 1; i < n && w < NH_RM_WAIT_MAX_MS; i++) w *= 2;
    return w > NH_RM_WAIT_MAX_MS ? NH_RM_WAIT_MAX_MS : w;
}

// the bake's horse.txt ("GameRomHorse12=OnlineHorse_GameRomHorse12" a line): which puppet stands for which horse
typedef struct { uint32_t h; char online[48]; } NhMapEnt;
typedef struct { NhMapEnt e[NH_MAP_N]; int n; } NhMap;
static inline int nh_map_parse(const char *text, NhMap *m) {
    memset(m, 0, sizeof *m);
    const char *s = text;
    while (s && *s && m->n < NH_MAP_N) {
        const char *e = s;
        while (*e && *e != '\n') e++;
        const char *eq = s;
        while (eq < e && *eq != '=') eq++;
        int ln = (int)(eq - s), rn = (int)(e - eq) - 1;
        while (rn > 0 && (eq[rn] == '\r' || eq[rn] == ' ')) rn--;
        if (eq < e && ln > 0 && ln < 48 && rn > 0 && rn < 48) {
            char key[48];
            memcpy(key, s, (size_t)ln), key[ln] = 0;
            bool ok = true;
            for (int k = 0; k < rn && ok; k++) ok = eq[1 + k] > 0x20 && eq[1 + k] < 0x7f;
            if (ok) {
                NhMapEnt *x = &m->e[m->n++];
                x->h = nh_crc32(key), memcpy(x->online, eq + 1, (size_t)rn), x->online[rn] = 0;
            }
        }
        s = *e ? e + 1 : e;
    }
    return m->n;
}
static inline const char *nh_map_find(const NhMap *m, uint32_t h) {
    for (int i = 0; m && i < m->n; i++)
        if (m->e[i].h == h) return m->e[i].online;
    return NULL;
}

// (s291 horsefix, live nethorsecoat) a horse's coat. BotW rolls it per HORSE, not per actor name (VERIFIED, 1.9.0 code at
// 0x71011a7e0c, static only): the Horse plays Collar_Shader / Collar_Shader_Coward / Collar_Shader_Impatient (by its GParam
// Horse.Nature 0 / 1 / 2; Epona / Zelda their own) in AS slot 4 at Rate 0 and sets that slot's frame to the int at actor
// +0xb78, rolled once (random 0..the AS's frame count) while it is < 0; a registered horse's comes from GameData
// Horse_CollarType. Those AS play the material anims Horse_Link_Collar_<Docile|Touchy|Epona|Zelda>_fsp / _ftp (the coat
// colours + the Horse_Link_Body_Alb.00-04 pattern). The model-only puppet played none of them: Horse_Link with no Collar anim
// = the dark coat with white socks the s291 lab saw under a cream horse. The coat byte (a kind-3 horse record: the AS field's
// last byte, which no older build writes (0) or reads): 0 = not known, else 1 + family * 32 + frame (frame 0..31). The puppet
// plays the bake's AS Coat_<D|T|E|Z>_<nn> (tools/sorabuild/nethorse/horse_bake.py) in slot 4.
#define NH_COAT_SLOT 4       // (the Horse's own slot for its Collar AS: 0x71011a7e1c w3 = 4)
#define NH_COAT_OFF 0xb78    // (the Horse's coat frame, int32: zeroed at 0x71011a7dc4 for Epona, rolled in 0x71011a7e54's code)
#define NH_COAT_FRAMES 32    // (frames 0..31 fit the byte; Docile / Touchy have 30, Epona / Zelda 0: the bake's horse_list.txt)
enum { NHC_DOCILE, NHC_TOUCHY, NHC_EPONA, NHC_ZELDA, NHC_N };
static const char NHC_LETTER[NHC_N] = {'D', 'T', 'E', 'Z'};
// the AS his horse plays in slot 4 (its ASList name; its file name Horse_Link_<name> too) -> its family, -1 not a coat AS
static inline int nh_coat_family(const char *as) {
    if (!as) return -1;
    if (!strncmp(as, "Horse_Link_", 11)) as += 11;
    if (!strcmp(as, "Collar_Shader")) return NHC_DOCILE;
    if (!strcmp(as, "Collar_Shader_Coward") || !strcmp(as, "Collar_Shader_Impatient")) return NHC_TOUCHY;
    if (!strcmp(as, "Collar_Shader_EponaHorse")) return NHC_EPONA;
    if (!strcmp(as, "Collar_Shader_ZeldaHorse")) return NHC_ZELDA;
    return -1;
}
// -> the coat byte, 0 = not known (a family / frame out of range; frame < 0 = not rolled yet)
static inline uint8_t nh_coat_byte(int family, int32_t frame) {
    if (family < 0 || family >= NHC_N || frame < 0 || frame >= NH_COAT_FRAMES) return 0;
    return (uint8_t)(1 + family * NH_COAT_FRAMES + frame);
}
// the coat byte -> the puppet's AS name ("Coat_D_07", the bake's); false (out "") = not known
static inline bool nh_coat_name(uint8_t coat, char out[16]) {
    out[0] = 0;
    if (!coat || coat > NHC_N * NH_COAT_FRAMES) return false;
    int f = (coat - 1) / NH_COAT_FRAMES, fr = (coat - 1) % NH_COAT_FRAMES;
    memcpy(out, "Coat_", 5);
    out[5] = NHC_LETTER[f], out[6] = '_', out[7] = (char)('0' + fr / 10), out[8] = (char)('0' + fr % 10), out[9] = 0;
    return true;
}

// (s291 coat) where an AS plays: BotW 1.9.0's AS manager (actor +0x560) has one AnmTarget per ModelList AnmTarget_<t> (mgr +0xb8
// count, +0xc0 array, 0x50 each; the count = ModelList +0x770, 0x7101502374) and, in each, NumASSlot AS slots (the target's
// +0x20 count, +0x28 array, 0x98 each; 0x71014bd7a4 from 0x7101502c14 = AnmTarget_<t> Base.NumASSlot). Every horse's ModelList
// (EngineRom_Horse_Link, the puppets' too) has AnmTarget_0 (6 slots) + AnmTarget_1 (3). Disassembled (s291 coat, VERIFIED):
//   objs_as_in_slot 0x71014a0040 (mgr, w1 = TARGET, w2 = SLOT)   (w1 checked against mgr +0xb8, w2 against the target's +0x20)
//   objs_play_as 0x710151e42c (action, name, w2 0 = again, w3 = TARGET, w4 = SLOT) -> 0x710149f24c(mgr, name, w2 TARGET, w3 SLOT, 1)
//   the Horse's Collar AS: 0x71011a7e14 / e1c 0x710149f24c(mgr, name, w2 = 0, w3 = 4) = TARGET 0, SLOT 4 (its frame: 0x71014a2424
//   (mgr, 0, 4, ..), the frame count 0x71014a25b0(mgr, 0, 4, ..)).
// s291 horsefix passed (4, 0) to both = AnmTarget 4, which no horse has: as_in_slot gave its empty default (0x7101dad0b8), play_as
// returned without playing (0x710149f2a0) -> "coat -" / no "our horse's coat" line in the s291e lab. These two fill the calls.
#define NO_AS_TARGETS_MAX 4  // (the diag reads at most this many AnmTargets ...
#define NO_AS_SLOTS_MAX 8    // ... and slots in each: a horse has 2 x (6, 3); a bad count = a bad read, capped)
static inline void no_as_in_slot_args(uint64_t x[3], uint64_t mgr, int target, int slot) {
    x[0] = mgr, x[1] = (uint64_t)(uint32_t)target, x[2] = (uint64_t)(uint32_t)slot;
}
static inline void no_play_as_args(uint64_t x[5], uint64_t action, uint64_t name, bool again, int target, int slot) {
    x[0] = action, x[1] = name, x[2] = again ? 0 : 1, x[3] = (uint64_t)(uint32_t)target, x[4] = (uint64_t)(uint32_t)slot;
}
// (s291 coat, lab diag) one line of every AS slot's playing AS: nt targets, ns[t] slots each, names[t][s] ("" = none / not read):
// "t0 (6): 0 Wait, 1 -, ..; t1 (3): .." (a count over the caps: "(9, read 8)"); -> out
static inline const char *nh_slots_line(char *out, size_t n, int nt, const int *ns, const char (*names)[NO_AS_SLOTS_MAX][NET_OBJS_AS]) {
    size_t at = 0;
    if (!n) return out;
    out[0] = 0;
    if (nt <= 0) { snprintf(out, n, "no AnmTarget"); return out; }
    int tn = nt > NO_AS_TARGETS_MAX ? NO_AS_TARGETS_MAX : nt;
    for (int t = 0; t < tn && at < n; t++) {
        int sn = ns[t] < 0 ? 0 : ns[t] > NO_AS_SLOTS_MAX ? NO_AS_SLOTS_MAX : ns[t];
        int w = ns[t] == sn ? snprintf(out + at, n - at, "%st%d (%d):", t ? "; " : "", t, ns[t])
                            : snprintf(out + at, n - at, "%st%d (%d, read %d):", t ? "; " : "", t, ns[t], sn);
        at += w > 0 ? (size_t)w : 0;
        for (int s = 0; s < sn && at < n; s++) {
            w = snprintf(out + at, n - at, "%s %d %s", s ? "," : "", s, names[t][s][0] ? names[t][s] : "-");
            at += w > 0 ? (size_t)w : 0;
        }
    }
    if (nt > tn && at < n) snprintf(out + at, n - at, "; (%d targets, read %d)", nt, tn);
    return out;
}

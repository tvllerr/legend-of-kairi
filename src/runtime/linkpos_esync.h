// s168 ESYNC exe side ("enemies act the same on every game", recon/S168_HELPER_ESYNC_EXE.md): the pure-C parts of the
// linkpos_esync_* exports - no guest memory, no locks, no clocks (time = game frames passed in), host-tested by
// runtime/test/esync_exe_test.c. linkpos.c holds the guest reads / calls and the hooks.
//   - esx_hash: the exe-owned name hash (FNV-1a 32; 0 is "none", so a real hash is never 0) + EsxNameTab: hash -> name
//   - EsxOff: the HashIds whose brain the DLL turned off (the "puppets")
//   - EsxHook: the ACTOR pointers whose RootAi::calc the hook skips (lock-free for the reader: the hook can run on any thread)
//   - EsxEvQ: ring of attack-sensor ON / OFF events the hooks saw, drained by linkpos_esync_events
//   - EsxTrk: owner side, per hid: change counters of the 2 AS slots + "a sensor is on"
//   - EsxPup: puppet side, per hid: what was last applied (AS hash/chg per slot, sensors we switched on)
#pragma once
#include <stdint.h>
#include <string.h>

// ---- the hash and the hash -> name table ----
static inline uint32_t esx_hash(const char *s) {
    uint32_t h = 2166136261u;
    for (int i = 0; s && s[i] && i < 255; i++) h = (h ^ (uint8_t)s[i]) * 16777619u;
    return h ? h : 1;
}

#define ESX_NAMES 512  // power of two
#define ESX_NAME_LEN 64
typedef struct { uint32_t h; char s[ESX_NAME_LEN]; } EsxName;
typedef struct { EsxName e[ESX_NAMES]; int n; uint64_t full, toolong; } EsxNameTab;

// 1 = new, 0 = known, -1 = refused (empty / too long / table full)
static inline int esx_name_put(EsxNameTab *t, const char *s) {
    size_t len = s ? strlen(s) : 0;
    if (!len) return -1;
    if (len >= ESX_NAME_LEN) return t->toolong++, -1;
    uint32_t h = esx_hash(s);
    for (uint32_t i = 0; i < ESX_NAMES; i++) {
        EsxName *e = &t->e[(h + i) & (ESX_NAMES - 1)];
        if (!e->h) {
            if (t->n >= ESX_NAMES - 8) return t->full++, -1;  // (keeps probe chains short)
            e->h = h, memcpy(e->s, s, len + 1), t->n++;
            return 1;
        }
        if (e->h == h) return strcmp(e->s, s) ? -1 : 0;  // (a real collision: the first name keeps the slot)
    }
    return t->full++, -1;
}
static inline const char *esx_name_get(const EsxNameTab *t, uint32_t h) {
    if (!h) return NULL;
    for (uint32_t i = 0; i < ESX_NAMES; i++) {
        const EsxName *e = &t->e[(h + i) & (ESX_NAMES - 1)];
        if (!e->h) return NULL;
        if (e->h == h) return e->s;
    }
    return NULL;
}

// ---- puppets: the HashIds whose brain is off (wanted by the DLL, whether or not the enemy is loaded right now) ----
#define ESX_OFF_MAX 64
typedef struct { uint32_t hid[ESX_OFF_MAX]; int n; } EsxOff;
static inline int esx_off_has(const EsxOff *o, uint32_t hid) {
    for (int i = 0; i < o->n; i++) if (o->hid[i] == hid) return 1;
    return 0;
}
static inline int esx_off_add(EsxOff *o, uint32_t hid) {  // 1 added, 0 already there, -1 full
    if (esx_off_has(o, hid)) return 0;
    if (o->n == ESX_OFF_MAX) return -1;
    o->hid[o->n++] = hid;
    return 1;
}
static inline int esx_off_del(EsxOff *o, uint32_t hid) {
    for (int i = 0; i < o->n; i++)
        if (o->hid[i] == hid) return o->hid[i] = o->hid[--o->n], 1;
    return 0;
}

// ---- the hook's table: actor pointers (0 = free). Readers take no lock: a stale read costs one frame at most. ----
#define ESX_HOOK_MAX 64
typedef struct { volatile uint64_t a[ESX_HOOK_MAX]; volatile int live; } EsxHook;
static inline int esx_hook_has(const EsxHook *h, uint64_t actor) {
    if (!h->live || !actor) return 0;
    for (int i = 0; i < ESX_HOOK_MAX; i++) if (h->a[i] == actor) return 1;
    return 0;
}
static inline int esx_hook_add(EsxHook *h, uint64_t actor) {  // 1 added, 0 there, -1 full
    int free_i = -1;
    for (int i = 0; i < ESX_HOOK_MAX; i++) {
        if (h->a[i] == actor) return 0;
        if (!h->a[i] && free_i < 0) free_i = i;
    }
    if (free_i < 0) return -1;
    h->a[free_i] = actor;
    h->live++;
    return 1;
}
static inline int esx_hook_del(EsxHook *h, uint64_t actor) {
    for (int i = 0; i < ESX_HOOK_MAX; i++)
        if (h->a[i] == actor) return h->a[i] = 0, h->live--, 1;
    return 0;
}

// ---- sensor events (owner side) ----
typedef struct { uint8_t seq, on; uint32_t hid, name, params[4]; } EsxEv;  // = the DLL's EsEvent (NetEsEv), seq filled by the DLL
#define ESX_EVQ 64
typedef struct { EsxEv e[ESX_EVQ]; int n; uint64_t dropped; } EsxEvQ;
static inline void esx_ev_push(EsxEvQ *q, const EsxEv *ev) {
    if (q->n == ESX_EVQ) memmove(q->e, q->e + 1, sizeof q->e[0] * (ESX_EVQ - 1)), q->n--, q->dropped++;
    q->e[q->n++] = *ev;
}
static inline int esx_ev_take(EsxEvQ *q, EsxEv *out, int max) {
    int n = q->n < max ? q->n : max;
    if (n <= 0) return 0;
    memcpy(out, q->e, sizeof *out * (size_t)n);
    memmove(q->e, q->e + n, sizeof q->e[0] * (size_t)(q->n - n));
    q->n -= n;
    return n;
}

// ---- owner side: per hid the AS change counters ----
#define ESX_TRK_MAX 64
// son = our own brain has an attack sensor on (son_frame / son_name = when + which, S168 MF-2: no freeze mid-swing); action = cached leaf action (S168 SF-4)
typedef struct { uint32_t hid; uint64_t seen; uint32_t hash[2]; uint64_t obj[2]; uint8_t chg[2], fin[2], son; uint32_t son_name; uint64_t son_frame, act_frame; uint16_t action; } EsxTrkEnt;
typedef struct { EsxTrkEnt e[ESX_TRK_MAX]; int n; } EsxTrk;
static inline EsxTrkEnt *esx_trk_get(EsxTrk *t, uint32_t hid, uint64_t now) {
    for (int i = 0; i < t->n; i++) if (t->e[i].hid == hid) return t->e[i].seen = now, &t->e[i];
    if (t->n == ESX_TRK_MAX) {  // evict the stalest
        int w = 0;
        for (int i = 1; i < t->n; i++) if (t->e[i].seen < t->e[w].seen) w = i;
        t->e[w] = t->e[--t->n];
    }
    EsxTrkEnt *e = &t->e[t->n++];
    memset(e, 0, sizeof *e);
    e->hid = hid, e->seen = now;
    return e;
}
static inline EsxTrkEnt *esx_trk_find(EsxTrk *t, uint32_t hid) {  // (no create, no stamp)
    for (int i = 0; i < t->n; i++) if (t->e[i].hid == hid) return &t->e[i];
    return NULL;
}
// may this hid's brain be frozen now? not while our own sensor is on, up to `cap` frames (then the caller forces it off). 1 yes, 0 wait, -1 cap passed
static inline int esx_trk_may_freeze(const EsxTrkEnt *t, uint64_t now, uint64_t cap) {
    if (!t || !t->son) return 1;
    return now - t->son_frame > cap ? -1 : 0;
}
// slot's running AS now: hash (0 = nothing), obj = the running AS object's address (0 = nothing), fin = finished flag. Returns the new chg.
// The counter goes up when the AS name changes, the AS object changes (a restart of the same name usually makes a new one), or a finished AS plays again.
static inline uint8_t esx_trk_slot(EsxTrkEnt *t, int slot, uint32_t hash, uint64_t obj, int fin) {
    int changed = hash != t->hash[slot] || obj != t->obj[slot] || (t->fin[slot] && !fin && hash);
    if (changed) t->chg[slot]++;
    t->hash[slot] = hash, t->obj[slot] = obj, t->fin[slot] = (uint8_t)(fin != 0);
    return t->chg[slot];
}

// ---- puppet side: what was applied ----
#define ESX_PUP_MAX 64
#define ESX_PUP_SENS 8
typedef struct {
    uint32_t hid;
    uint64_t actor;                 // the actor the state below belongs to (a reload = a new actor = state reset)
    uint32_t hash[2];
    uint8_t chg[2], have[2];
    uint8_t defs_learned, sensors_learned;
    uint32_t sens[ESX_PUP_SENS];    // sensor name hashes this side switched on
    int nsens;
    uint8_t off_pending;            // an OFF came while the body was not fresh (seen > 3 frames ago): end every sensor when it is (S168 SF-1)
#define ESX_PUP_NODES 16
    uint32_t nh[ESX_PUP_NODES];     // this actor's own ActivateAttackSensor nodes by name hash (their params beat the wire's, S168 SF-2)
    uint64_t node[ESX_PUP_NODES];
    int nnode;
    // s174 R2: the owner's attack action run locally on this puppet (its club / body hit switches itself from its own AS window)
#define ESX_PUP_ATK 24
    uint8_t atk_walked;             // its AI tree was searched for attack-class nodes (once per body)
    int natk;
    uint16_t aidx[ESX_PUP_ATK];     // definition index (node+0x18) -> node
    uint64_t anode[ESX_PUP_ATK];
    uint64_t atk_node;              // the attack node running now (0 = none)
    uint16_t atk_idx, atk_last;     // its index; the index of the last swing that ran here (finished / left)
    uint8_t atk_lchg;               // the owner's AS slot-0 change counter when that swing ended (absorbed while it ran)
    uint64_t atk_frame, atk_seen, atk_end;  // entered / last puppet update / ended (game frames)
} EsxPupEnt;
typedef struct { EsxPupEnt e[ESX_PUP_MAX]; int n; } EsxPup;
static inline EsxPupEnt *esx_pup_get(EsxPup *p, uint32_t hid, uint64_t actor, int create) {
    for (int i = 0; i < p->n; i++)
        if (p->e[i].hid == hid) {
            if (p->e[i].actor != actor) {
                if (!create) return NULL;
                memset(&p->e[i], 0, sizeof p->e[i]), p->e[i].hid = hid, p->e[i].actor = actor;
            }
            return &p->e[i];
        }
    if (!create || p->n == ESX_PUP_MAX) return NULL;
    EsxPupEnt *e = &p->e[p->n++];
    memset(e, 0, sizeof *e);
    e->hid = hid, e->actor = actor;
    return e;
}
static inline void esx_pup_drop(EsxPup *p, uint32_t hid) {
    for (int i = 0; i < p->n; i++) if (p->e[i].hid == hid) { p->e[i] = p->e[--p->n]; return; }
}
// does this slot need a (re)start? hash 0 = nothing wanted: no (and the state forgets), else when never applied / hash or chg differs
static inline int esx_pup_need(EsxPupEnt *p, int slot, uint32_t hash, uint8_t chg) {
    if (!hash) return p->have[slot] = 0, p->hash[slot] = 0, 0;
    return !p->have[slot] || p->hash[slot] != hash || p->chg[slot] != chg;
}
static inline void esx_pup_done(EsxPupEnt *p, int slot, uint32_t hash, uint8_t chg) { p->have[slot] = 1, p->hash[slot] = hash, p->chg[slot] = chg; }
static inline int esx_pup_sens_add(EsxPupEnt *p, uint32_t h) {  // 1 added, 0 there, -1 full
    for (int i = 0; i < p->nsens; i++) if (p->sens[i] == h) return 0;
    if (p->nsens == ESX_PUP_SENS) return -1;
    p->sens[p->nsens++] = h;
    return 1;
}
static inline int esx_pup_has_sens(const EsxPupEnt *p, uint32_t h) {
    for (int i = 0; i < p->nsens; i++) if (p->sens[i] == h) return 1;
    return 0;
}
static inline void esx_pup_node_put(EsxPupEnt *p, uint32_t h, uint64_t node) {  // first node of a name wins
    for (int i = 0; i < p->nnode; i++) if (p->nh[i] == h) return;
    if (p->nnode < ESX_PUP_NODES) p->nh[p->nnode] = h, p->node[p->nnode++] = node;
}
static inline uint64_t esx_pup_node_get(const EsxPupEnt *p, uint32_t h) {
    for (int i = 0; i < p->nnode; i++) if (p->nh[i] == h) return p->node[i];
    return 0;
}
static inline uint64_t esx_pup_atk_node(const EsxPupEnt *p, uint16_t idx) {
    for (int i = 0; i < p->natk; i++) if (p->aidx[i] == idx) return p->anode[i];
    return 0;
}
static inline void esx_pup_atk_put(EsxPupEnt *p, uint16_t idx, uint64_t node) {  // first node of an index wins
    if (!idx || esx_pup_atk_node(p, idx)) return;
    if (p->natk < ESX_PUP_ATK) p->aidx[p->natk] = idx, p->anode[p->natk++] = node;
}
// s174: this frame's step for the puppet's local attack. want = the owner's action index (0 none), want_atk = it is an attack node
// of this body, chg0 = the owner's AS slot-0 change counter. 0 nothing / keep running, 1 enter want, 2 leave, 3 leave then enter want.
// After a swing ended here the same index runs again only when the owner's slot-0 AS changed since AND >= gap frames passed (GUESS:
// a back-to-back repeat of the same action sends no index change).
#define ESX_ATK_STEP_NONE 0
#define ESX_ATK_STEP_ENTER 1
#define ESX_ATK_STEP_LEAVE 2
#define ESX_ATK_STEP_SWAP 3
static inline int esx_atk_step(const EsxPupEnt *p, uint16_t want, int want_atk, uint8_t chg0, uint64_t now, uint64_t cap, uint64_t gap, int alive) {
    if (p->atk_node) {
        if (!alive || now - p->atk_frame > cap) return ESX_ATK_STEP_LEAVE;
        if (want != p->atk_idx) return want_atk ? ESX_ATK_STEP_SWAP : ESX_ATK_STEP_LEAVE;
        return ESX_ATK_STEP_NONE;
    }
    if (!alive || !want || !want_atk) return ESX_ATK_STEP_NONE;
    if (p->atk_last == want && (p->atk_lchg == chg0 || now - p->atk_end < gap)) return ESX_ATK_STEP_NONE;
    return ESX_ATK_STEP_ENTER;
}
static inline int esx_pup_sens_del(EsxPupEnt *p, uint32_t h) {
    for (int i = 0; i < p->nsens; i++) if (p->sens[i] == h) return p->sens[i] = p->sens[--p->nsens], 1;
    return 0;
}

// ---- the puppet's matrix: pure rotation about y by yaw + translation, BotW Matrix34f rows (c 0 s tx)(0 1 0 ty)(-s 0 c tz) ----
#include <math.h>
static inline void esx_mtx(float m[12], const float pos[3], float yaw) {
    float s = sinf(yaw), c = cosf(yaw);
    m[0] = c,  m[1] = 0, m[2] = s,  m[3] = pos[0];
    m[4] = 0,  m[5] = 1, m[6] = 0,  m[7] = pos[1];
    m[8] = -s, m[9] = 0, m[10] = c, m[11] = pos[2];
}
static inline float esx_yaw(const float m[12]) { return atan2f(m[2], m[10]); }

// s175 "idea 1": the PURE bookkeeping behind runtime/sw_api.h (hook table, tap table, guest-call argument packing). No Windows, no
// guest memory: host-tested by runtime/test/sw_api_test.c. The exe side (runtime/linkpos_api_block.inc) owns the stubs, the guest
// memory and the locks; this header only does the arithmetic and the table.
// Threading: fn / user / taps are read by the stubs on ANY guest thread (atomic loads); everything else is written under the exe's
// g_sw_lock. A reader that races a replace of the SAME hook may see the new fn with the old user (or the reverse): the DLL keeps
// `user` stable across a re-hook (it only ever passes its own state pointer).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sw_api.h"

// ---- hook table ----
#define SW_SLOT_MAX 512   // vtable slots 0..511
typedef struct {
    uint64_t vt, orig;      // the vtable, the guest function the slot held before (0 for an unused entry)
    void *fn, *user;        // SwHookFn / user (atomic)
    void *origfn;           // GuestFn of orig (kept after a free: a stub already running on another thread still needs it)
    int slot;
    uint8_t used, ever;     // in use now / allocated at least once since the last reset (a never-used id is preferred over a freed one)
    char name[24];
} SwHookEnt;
typedef struct {
    SwHookEnt h[SW_HOOKS];
    void *tap[SW_TAPS];     // atomic
} SwCore;

static inline void *sw_ld(void *const *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }
static inline void sw_st(void **p, void *v) { __atomic_store_n(p, v, __ATOMIC_RELEASE); }

static inline void sw_core_reset(SwCore *k) { memset(k, 0, sizeof *k); }
static inline void sw_core_hooks_clear(SwCore *k) { memset(k->h, 0, sizeof k->h); }  // (a reboot: the taps are kept)

static inline int sw_core_hook_find(const SwCore *k, uint64_t vt, int slot) {
    for (int i = 0; i < SW_HOOKS; i++)
        if (k->h[i].used && k->h[i].vt == vt && k->h[i].slot == slot) return i;
    return -1;
}
// the same vt+slot -> the same id (*isnew = 0, entry untouched); else a never-used entry, only then a freed one (a stub still running
// on another thread may use a freed id's origfn: reuse it as late as possible) (*isnew = 1); -1 = bad args / all used
static inline int sw_core_hook_alloc(SwCore *k, uint64_t vt, int slot, uint64_t orig, void *origfn, const char *name, int *isnew) {
    if (isnew) *isnew = 0;
    if (!vt || slot < 0 || slot >= SW_SLOT_MAX) return -1;
    int id = sw_core_hook_find(k, vt, slot);
    if (id >= 0) return id;
    for (id = 0; id < SW_HOOKS && (k->h[id].used || k->h[id].ever); id++) {}
    if (id == SW_HOOKS)
        for (id = 0; id < SW_HOOKS && k->h[id].used; id++) {}
    if (id == SW_HOOKS) return -1;
    SwHookEnt *e = &k->h[id];
    sw_st(&e->fn, NULL), sw_st(&e->user, NULL);
    e->vt = vt, e->slot = slot, e->orig = orig, e->origfn = origfn, e->used = 1, e->ever = 1;
    memset(e->name, 0, sizeof e->name);
    if (name) strncpy(e->name, name, sizeof e->name - 1);
    if (isnew) *isnew = 1;
    return id;
}
static inline void sw_core_hook_set(SwCore *k, int id, SwHookFn fn, void *user) {
    if (id < 0 || id >= SW_HOOKS) return;
    sw_st(&k->h[id].user, user);
    sw_st(&k->h[id].fn, (void *)fn);
}
static inline SwHookFn sw_core_hook_fn(const SwCore *k, int id, void **user) {
    if (id < 0 || id >= SW_HOOKS) return NULL;
    SwHookFn fn = (SwHookFn)sw_ld(&k->h[id].fn);
    if (user) *user = sw_ld(&k->h[id].user);
    return fn;
}
static inline void sw_core_hook_free(SwCore *k, int id) {  // (origfn stays: see SwHookEnt)
    if (id < 0 || id >= SW_HOOKS || !k->h[id].used) return;
    sw_st(&k->h[id].fn, NULL), sw_st(&k->h[id].user, NULL);
    k->h[id].used = 0, k->h[id].vt = 0, k->h[id].orig = 0, k->h[id].slot = 0;
}
static inline int sw_core_hooks_used(const SwCore *k) {
    int n = 0;
    for (int i = 0; i < SW_HOOKS; i++) n += k->h[i].used;
    return n;
}

// ---- taps ----
static inline bool sw_core_tap_set(SwCore *k, int tap, void *fn) {
    if (tap < 0 || tap >= SW_TAPS) return false;
    sw_st(&k->tap[tap], fn);
    return true;
}
static inline void *sw_core_tap_get(const SwCore *k, int tap) { return tap < 0 || tap >= SW_TAPS ? NULL : sw_ld(&k->tap[tap]); }
static inline void sw_core_taps_clear(SwCore *k) {
    for (int i = 0; i < SW_TAPS; i++) sw_st(&k->tap[i], NULL);
}

// ---- guest call argument packing ----
// Layout below the live guest sp (grows down): [sp - SW_REDZONE .. ) untouched red zone, then the scratch (rounded to 16, its start
// 16-aligned = `base`), then SW_CALLEE_GAP bytes, and the callee's sp (`newsp`) sits at the bottom of that gap, 16-aligned.
#define SW_REDZONE 0x100
#define SW_CALLEE_GAP 0x40
typedef struct {
    uint32_t rs;        // scratch bytes, rounded up to 16
    uint64_t base;      // scratch start (guest address)
    uint64_t newsp;     // sp for the callee
    uint64_t x[8];      // the x registers to load (x_scratch bits already turned into addresses)
} SwPack;
static inline bool sw_core_pack(const SwArgs *a, uint64_t cur_sp, SwPack *p) {
    if (!a || !p) return false;
    if (a->scratch > SW_SCRATCH_MAX || a->init_len > a->scratch || a->out_len > a->scratch) return false;
    if (a->nx > 8 || a->nv > 4) return false;
    if (a->init_len && !a->init) return false;
    if (a->out_len && !a->out) return false;
    uint32_t rs = ((uint32_t)a->scratch + 15u) & ~15u;
    if (cur_sp < (uint64_t)SW_REDZONE + rs + SW_CALLEE_GAP + 0x1000) return false;  // (not a real guest sp)
    p->rs = rs;
    p->base = (cur_sp - SW_REDZONE - rs) & ~0xfull;
    p->newsp = (p->base - SW_CALLEE_GAP) & ~0xfull;
    for (int i = 0; i < 8; i++) {
        p->x[i] = a->x[i];
        if (i < a->nx && (a->x_scratch >> i & 1)) {
            if (a->x[i] > a->scratch) return false;  // (an offset past the scratch)
            p->x[i] = p->base + a->x[i];
        }
    }
    return true;
}

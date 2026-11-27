// linkpos_ercache.h - (mgelem, PR #61; s289 b9fix) where each enemy class keeps its element-reaction object, cached by the
// actor's vtable (linkpos.c er_obj). Pure: no guest reads; runtime/test/ercache_test.c tests it.
// One entry per vtable: found = {base 0 actor / 1 damage manager, ptr 1 pointer at / 0 inside, off, vtable slot of react},
// or slot -1 = "this class has none" (searched once, never again while the entry lives). A fixed table of ERC_N entries; when
// it is full the least recently used entry is replaced (that class is searched again on its next hit).
#pragma once
#include <stdint.h>

#define ERC_N 32
typedef struct { uint64_t vt; int base, ptr, off, slot; uint32_t used; } ErcEnt;
typedef struct { ErcEnt e[ERC_N]; int n; uint32_t tick; } ErCache;

static ErcEnt *erc_find(ErCache *c, uint64_t vt) {
    if (!vt) return 0;
    for (int i = 0; i < c->n; i++)
        if (c->e[i].vt == vt) { c->e[i].used = ++c->tick; return &c->e[i]; }
    return 0;
}
// a new entry (or vt's own entry again); *evicted = the vtable it replaced (0 = a free entry was used)
static ErcEnt *erc_put(ErCache *c, uint64_t vt, int base, int ptr, int off, int slot, uint64_t *evicted) {
    ErcEnt *d = 0;
    if (evicted) *evicted = 0;
    for (int i = 0; i < c->n && !d; i++)
        if (c->e[i].vt == vt) d = &c->e[i];
    if (!d && c->n < ERC_N) d = &c->e[c->n++];
    if (!d) {
        d = &c->e[0];
        for (int i = 1; i < ERC_N; i++)
            if (c->e[i].used < d->used) d = &c->e[i];
        if (evicted) *evicted = d->vt;
    }
    d->vt = vt, d->base = base, d->ptr = ptr, d->off = off, d->slot = slot, d->used = ++c->tick;
    return d;
}

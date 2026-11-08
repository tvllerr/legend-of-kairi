// s175 "idea 1": the exe's primitives for the hot-reloaded Sora DLL (runtime/sw_api.h is the contract; the pure bookkeeping is in
// runtime/sw_api_core.h, host-tested). #included at the END of linkpos.c (it uses linkpos.c's statics and the esync block).
// Plan: D:/SoraBotW/docs/specs/2026-11-07-idea1-exe-primitives-plan.md
#include "sw_api_core.h"

// (no __declspec(dllexport) on Windows: sw_api.h declares it plainly, clang warns on a dllexport redeclaration, and the exe links with
// --export-all-symbols anyway. A dllexport would also be the first one: it would switch off the automatic export of everything else.)
#define SW_EXPORT __attribute__((used, visibility("default")))

static SwCore g_sw;
static SRWLOCK g_sw_lock = SRWLOCK_INIT;   // hook table writes (vt_hook / unhook / switch); never held across a guest call
static uint64_t g_sw_stub[SW_HOOKS];       // the hle_register()ed stub addresses (set at boot, kept over resets of the table)

// ---- the hook stubs ----
static void sw_hook_run(Cpu *c, int id) {
    void *user = NULL;
    SwHookFn fn = sw_core_hook_fn(&g_sw, id, &user);
    if (fn) { fn(c, id, user); return; }
    GuestFn of = (GuestFn)sw_ld(&g_sw.h[id].origfn);
    if (of) of(c);
}
#define SW_STUB(i) static void sw_stub##i(Cpu *c) { sw_hook_run(c, i); }
SW_STUB(0) SW_STUB(1) SW_STUB(2) SW_STUB(3) SW_STUB(4) SW_STUB(5) SW_STUB(6) SW_STUB(7)
SW_STUB(8) SW_STUB(9) SW_STUB(10) SW_STUB(11) SW_STUB(12) SW_STUB(13) SW_STUB(14) SW_STUB(15)
SW_STUB(16) SW_STUB(17) SW_STUB(18) SW_STUB(19) SW_STUB(20) SW_STUB(21) SW_STUB(22) SW_STUB(23)
SW_STUB(24) SW_STUB(25) SW_STUB(26) SW_STUB(27) SW_STUB(28) SW_STUB(29) SW_STUB(30) SW_STUB(31)
#undef SW_STUB
static const HostFn g_sw_stubfn[SW_HOOKS] = {
    sw_stub0, sw_stub1, sw_stub2, sw_stub3, sw_stub4, sw_stub5, sw_stub6, sw_stub7, sw_stub8, sw_stub9, sw_stub10, sw_stub11,
    sw_stub12, sw_stub13, sw_stub14, sw_stub15, sw_stub16, sw_stub17, sw_stub18, sw_stub19, sw_stub20, sw_stub21, sw_stub22, sw_stub23,
    sw_stub24, sw_stub25, sw_stub26, sw_stub27, sw_stub28, sw_stub29, sw_stub30, sw_stub31};

static void *sw_tap_get(int tap) { return sw_core_tap_get(&g_sw, tap); }

static void sw_api_init(void) {  // linkpos_init (startup thread, before any guest code): register the stubs, forget the old table
    AcquireSRWLockExclusive(&g_sw_lock);
    sw_core_reset(&g_sw);
    ReleaseSRWLockExclusive(&g_sw_lock);
    int n = 0;
    for (int i = 0; i < SW_HOOKS; i++) {
        char nm[32];
        snprintf(nm, sizeof nm, "sw hook %d", i);
        g_sw_stub[i] = hle_register(nm, g_sw_stubfn[i]);  // (copies the name; the same name returns the same address)
        n += g_sw_stub[i] != 0;
    }
    LOG("[swapi] api v%u ready (%u bytes, %d hook stubs)\n", (unsigned)SW_API_VERSION, (unsigned)sizeof(SwApi), n);
}

// ---- memory ----
static bool swa_in_guest(uint64_t a, size_t n) { return n <= GUEST_HI && in_guest(a) && a + n <= GUEST_HI && a + n >= a; }
static size_t swa_readable(uint64_t a, size_t n) { return n && in_guest(a) ? readable(a, n) : 0; }
static bool swa_read(uint64_t a, void *out, size_t n) {
    if (!n) return true;
    if (!out || !swa_in_guest(a, n) || readable(a, n) != n) return false;
    memcpy(out, (const void *)(uintptr_t)a, n);
    return true;
}
static bool swa_write(uint64_t a, const void *in, size_t n) {
    if (!n) return true;
    if (!in || !swa_in_guest(a, n) || readable(a, n) != n) return false;
    for (size_t ok = 0; ok < n;) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((const void *)(uintptr_t)(a + ok), &mbi, sizeof mbi) || mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD) ||
            (mbi.Protect & 0xff) != PAGE_READWRITE)
            return false;
        uint64_t end = (uint64_t)(uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        ok = end - a >= n ? n : (size_t)(end - a);
    }
    memcpy((void *)(uintptr_t)a, in, n);
    return true;
}

// ---- game state ----
static bool swa_in_frame(void) { return g_in_frame; }
static uint64_t swa_frame_id(void) { return g_vel_frame_id; }
static uint64_t swa_site(const char *name) {
    const GameLibTables *t = name ? gamelib_tables() : NULL;
    for (uint32_t i = 0; t && t->sites && t->nsites && i < *t->nsites; i++)
        if (t->sites[i].name && !strcmp(t->sites[i].name, name)) return t->sites[i].addr;
    if (!name) return 0;
    // then the Sora sites (sora_sites.h, the running game version's row in g_ss) by field name: atkact_*_vt live there
#define SWA_SORA_SITE(n, a, b) if (!strcmp(name, #n)) return g_ss.n;
    SORA_SITES(SWA_SORA_SITE)
#undef SWA_SORA_SITE
    return 0;
}
static uint64_t swa_fn_at(uint64_t guest) { return guest ? guest_fn_at(guest) : 0; }
static const char *swa_actor_name(uint64_t a) {
    if (!swa_in_guest(a, BASEPROC_NAME + 8) || readable(a + BASEPROC_NAME, 8) != 8) return "";
    const char *n = actor_name(a);
    return n && readable((uint64_t)(uintptr_t)n, 1) == 1 ? n : "";
}

// ---- guest calls ----
static Cpu *swa_cpu(Cpu *c) { return c ? c : g_in_frame ? kernel_current_cpu() : NULL; }
static bool swa_invoke(Cpu *c, uint64_t fn, const SwArgs *a, SwRet *r) {
    c = swa_cpu(c);
    if (!c || !a || !fn || !dispatch_lookup(fn)) return false;
    SwPack pk;
    if (!sw_core_pack(a, c->sp, &pk)) return false;
    if (pk.rs && readable(pk.base, pk.rs) != pk.rs) return false;
    Cpu save = *c;
    if (pk.rs) {
        memset((void *)(uintptr_t)pk.base, 0, pk.rs);
        if (a->init_len) memcpy((void *)(uintptr_t)pk.base, a->init, a->init_len);
    }
    c->sp = pk.newsp;
    for (int i = 0; i < a->nx; i++) c->x[i] = pk.x[i];
    for (int i = 0; i < a->nv; i++) c->v[i] = (__uint128_t)a->s[i];
    guest_call(c, fn);
    if (a->out_len) memcpy(a->out, (const void *)(uintptr_t)pk.base, a->out_len);
    if (r) r->x0 = c->x[0], r->x1 = c->x[1], r->s0 = (uint32_t)c->v[0];
    *c = save;
    return true;
}
static bool swa_call(Cpu *c, uint64_t fn, const SwArgs *a, SwRet *r) { return swa_invoke(c, fn, a, r); }
static bool swa_vcall(Cpu *c, uint64_t obj, int slot, const SwArgs *a, SwRet *r) {
    if (!a || slot < 0 || slot >= SW_SLOT_MAX || !swa_in_guest(obj, 8) || readable(obj, 8) != 8) return false;
    uint64_t vt = *(const uint64_t *)(uintptr_t)obj;
    if (!in_guest(vt) || readable(vt + 8ull * (uint32_t)slot, 8) != 8) return false;
    uint64_t fn = *(const uint64_t *)(uintptr_t)(vt + 8ull * (uint32_t)slot);
    SwArgs aa = *a;
    aa.x[0] = obj, aa.x_scratch &= (uint8_t)~1u;
    if (aa.nx < 1) aa.nx = 1;
    return swa_invoke(c, fn, &aa, r);
}

// ---- AI tree (same algorithms as esx_leaf_action / esx_atk_walk; those stay untouched) ----
// Thread safety: each call allocates its own dedupe set + stack (no shared static buffers), so concurrent calls on different Cpus
// are fine. They need a Cpu (getType is a guest call).
static bool swa_ai_gettype(Cpu *c, uint64_t sp, uint64_t node, uint64_t vt, bool *leaf) {
    uint64_t gt = in_guest(vt) && readable(vt + 8 * VT_AI_GET_TYPE, 8) == 8 ? *(const uint64_t *)(uintptr_t)(vt + 8 * VT_AI_GET_TYPE) : 0;
    if (!gt || !dispatch_lookup(gt)) return false;
    c->sp = sp, c->x[0] = node;
    guest_call(c, gt);
    *leaf = (int32_t)c->x[0] != 0;
    return true;
}
static bool swa_ai_leaf(Cpu *c, uint64_t actor, SwAiNode *out) {
    c = swa_cpu(c);
    uint64_t node;
    if (!c || !out || !esx_rd64(actor + ACTOR_ROOT_AI, &node)) return false;
    bool found = false;
    Cpu save = *c;
    uint64_t sp = (c->sp - 0x40) & ~0xfull;
    for (int depth = 0; depth < ESX_LEAF_DEPTH && in_guest(node) && readable(node, 0x40) >= 0x40; depth++) {
        if (*(const uint64_t *)(uintptr_t)(node + ACTION_ACTOR) != actor) break;
        uint64_t vt = *(const uint64_t *)(uintptr_t)node;
        bool leaf;
        if (!swa_ai_gettype(c, sp, node, vt, &leaf)) break;
        if (leaf) {
            out->node = node, out->vt = vt, out->idx = *(const uint16_t *)(uintptr_t)(node + 0x18), out->depth = (uint8_t)depth, out->leaf = 1;
            found = true;
            break;
        }
        uint32_t idx = *(const uint16_t *)(uintptr_t)(node + AI_CHILD_IDX), cnt = *(const uint32_t *)(uintptr_t)(node + AI_NCHILD);
        uint64_t kids = *(const uint64_t *)(uintptr_t)(node + AI_CHILDREN);
        if (idx == 0xffff || !cnt || cnt > 64 || !in_guest(kids) || readable(kids, 8ull * cnt) < 8ull * cnt) break;
        node = *(const uint64_t *)(uintptr_t)(kids + 8ull * (idx < cnt ? idx : 0));
    }
    *c = save;
    return found;
}
#define SWA_SET 8192
#define SWA_STACK 2048
static bool swa_set_mark(uint64_t *set, uint64_t n) {  // true = first time
    uint32_t h = (uint32_t)((n >> 3) * 2654435761u) & (SWA_SET - 1);
    for (int i = 0; i < SWA_SET; i++) {
        uint64_t *e = &set[(h + (uint32_t)i) & (SWA_SET - 1)];
        if (*e == n) return false;
        if (!*e) return *e = n, true;
    }
    return false;
}
static bool swa_set_has(const uint64_t *set, uint64_t n) {
    uint32_t h = (uint32_t)((n >> 3) * 2654435761u) & (SWA_SET - 1);
    for (int i = 0; i < SWA_SET; i++) {
        uint64_t e = set[(h + (uint32_t)i) & (SWA_SET - 1)];
        if (e == n) return true;
        if (!e) return false;
    }
    return false;
}
static int swa_ai_walk(Cpu *c, uint64_t actor, SwAiNode *out, int max) {
    c = swa_cpu(c);
    uint64_t root;
    if (!c || !out || max <= 0 || !esx_rd64(actor + ACTOR_ROOT_AI, &root) || !in_guest(root)) return 0;
    uint64_t *set = calloc(SWA_SET, sizeof *set);
    uint64_t *stack = malloc(SWA_STACK * sizeof *stack);
    uint8_t *sdepth = malloc(SWA_STACK);
    if (!set || !stack || !sdepth) { free(set), free(stack), free(sdepth); return 0; }
    int sp = 0, visited = 0, n = 0;
    Cpu save = *c;
    uint64_t gsp = (c->sp - 0x40) & ~0xfull;
    stack[sp] = root, sdepth[sp++] = 0;
    while (sp && visited < 4000 && n < max) {
        sp--;
        uint64_t node = stack[sp];
        uint8_t depth = sdepth[sp];
        if (!swa_set_mark(set, node)) continue;
        visited++;
        if (!in_guest(node) || readable(node, 0x40) < 0x40 || *(const uint64_t *)(uintptr_t)(node + ACTION_ACTOR) != actor) continue;
        uint64_t vt = *(const uint64_t *)(uintptr_t)node;
        bool leaf = false, typed = swa_ai_gettype(c, gsp, node, vt, &leaf);
        SwAiNode *o = &out[n++];
        o->node = node, o->vt = vt, o->idx = *(const uint16_t *)(uintptr_t)(node + 0x18), o->depth = depth, o->leaf = typed && leaf;
        if (!typed || leaf) continue;
        uint32_t cnt = *(const uint32_t *)(uintptr_t)(node + AI_NCHILD);
        uint64_t kids = *(const uint64_t *)(uintptr_t)(node + AI_CHILDREN);
        if (!cnt || cnt > 64 || !in_guest(kids) || readable(kids, 8ull * cnt) < 8ull * cnt) continue;
        for (uint32_t k = 0; k < cnt; k++) {
            uint64_t kid = *(const uint64_t *)(uintptr_t)(kids + 8ull * k);
            if (swa_set_has(set, kid)) continue;
            if (sp < SWA_STACK) stack[sp] = kid, sdepth[sp++] = depth < 255 ? (uint8_t)(depth + 1) : 255;
        }
    }
    *c = save;
    free(set), free(stack), free(sdepth);
    return n;
}

// ---- hooks ----
static bool swa_is_foreign_stub(uint64_t fn, int id) {
    for (int i = 0; i < SW_HOOKS; i++)
        if (g_sw_stub[i] && g_sw_stub[i] == fn && i != id) return true;
    for (int i = 0; i < DMG_STUBS; i++)
        if (g_dmg_stub[i].stub && g_dmg_stub[i].stub == fn) return true;
    return false;
}
static bool swa_slot_write(uint64_t vt, int slot, uint64_t val) {
    uint64_t *p = (uint64_t *)(uintptr_t)(vt + 8ull * (uint32_t)slot);
    DWORD old;
    if (!VirtualProtect(p, 8, PAGE_READWRITE, &old)) return false;
    *p = val;
    VirtualProtect(p, 8, old, &old);
    return true;
}
static int swa_vt_hook(uint64_t vt, int slot, SwHookFn fn, void *user, const char *name) {
    if (!fn || slot < 0 || slot >= SW_SLOT_MAX || !in_guest(vt) || readable(vt + 8ull * (uint32_t)slot, 8) != 8) return -1;
    AcquireSRWLockExclusive(&g_sw_lock);
    int id = sw_core_hook_find(&g_sw, vt, slot), res = -1;
    uint64_t cur = *(const uint64_t *)(uintptr_t)(vt + 8ull * (uint32_t)slot);
    if (id >= 0) {  // (again: same id, fn / user replaced; the slot already holds our stub)
        sw_core_hook_set(&g_sw, id, fn, user);
        res = id;
    } else if (swa_is_foreign_stub(cur, -1)) {
        LOG("[swapi] hook %s: vtable %llx slot %d holds another stub (%llx): refused\n", name ? name : "", (unsigned long long)vt, slot, (unsigned long long)cur);
    } else {
        GuestFn of = dispatch_lookup(cur);
        int isnew;
        if (!of) LOG("[swapi] hook %s: vtable %llx slot %d %llx: no function there\n", name ? name : "", (unsigned long long)vt, slot, (unsigned long long)cur);
        else if ((id = sw_core_hook_alloc(&g_sw, vt, slot, cur, (void *)of, name, &isnew)) < 0) LOG("[swapi] hook %s: all %d hooks used\n", name ? name : "", SW_HOOKS);
        else if (!g_sw_stub[id]) sw_core_hook_free(&g_sw, id), LOG("[swapi] hook %s: no stub %d registered\n", name ? name : "", id);
        else {
            sw_core_hook_set(&g_sw, id, fn, user);  // (before the slot points at the stub)
            if (!swa_slot_write(vt, slot, g_sw_stub[id])) {
                sw_core_hook_free(&g_sw, id);
                LOG("[swapi] hook %s: VirtualProtect failed (%lu)\n", name ? name : "", (unsigned long)GetLastError());
            } else {
                res = id;
                LOG("[swapi] hook %d %s: vtable %llx slot %d %llx -> stub\n", id, name ? name : "", (unsigned long long)vt, slot, (unsigned long long)cur);
            }
        }
    }
    ReleaseSRWLockExclusive(&g_sw_lock);
    return res;
}
static void swa_unhook_locked(int id, bool write_back) {
    SwHookEnt *e = &g_sw.h[id];
    if (!e->used) return;
    uint64_t vt = e->vt, orig = e->orig;
    int slot = e->slot;
    sw_core_hook_set(&g_sw, id, NULL, NULL);  // (the stub now runs the original)
    if (write_back && in_guest(vt) && readable(vt + 8ull * (uint32_t)slot, 8) == 8) {
        if (*(const uint64_t *)(uintptr_t)(vt + 8ull * (uint32_t)slot) == g_sw_stub[id]) {
            if (!swa_slot_write(vt, slot, orig)) {  // (the slot still holds our stub: keep the entry in use, fn NULL = the stub runs the original)
                LOG("[swapi] unhook %d: vtable %llx slot %d write-back failed (%lu): entry kept, stub passes through\n", id, (unsigned long long)vt, slot, (unsigned long)GetLastError());
                return;
            }
        } else LOG("[swapi] unhook %d: vtable %llx slot %d no longer holds our stub: left alone\n", id, (unsigned long long)vt, slot);
    }
    sw_core_hook_free(&g_sw, id);
}
static void swa_vt_unhook(int id) {
    if (id < 0 || id >= SW_HOOKS) return;
    AcquireSRWLockExclusive(&g_sw_lock);
    swa_unhook_locked(id, true);
    ReleaseSRWLockExclusive(&g_sw_lock);
}
static void swa_hook_orig(Cpu *c, int id) {
    if (!c || id < 0 || id >= SW_HOOKS) return;
    GuestFn of = (GuestFn)sw_ld(&g_sw.h[id].origfn);
    if (of) of(c);
}

// ---- taps ----
static bool swa_tap_set(int tap, void *fn) { return sw_core_tap_set(&g_sw, tap, fn); }

// ---- esync ----
static bool swa_esync_actor(uint32_t hid, uint64_t *actor, int *puppet) {
    uint64_t a;
    int32_t life;
    if (!g_esync_on || !hid || !esx_find(hid, 3, &a, &life) || !esx_actor_ok(a, hid, &life)) return false;
    AcquireSRWLockExclusive(&g_esx_lock);
    int pup = esx_off_has(&g_esx_off, hid) && esx_hook_has(&g_esx_hook, a);
    ReleaseSRWLockExclusive(&g_esx_lock);
    if (actor) *actor = a;
    if (puppet) *puppet = pup;
    return true;
}

static const SwApi g_swapi = {
    .version = SW_API_VERSION, .size = sizeof(SwApi),
    .in_guest = swa_in_guest, .readable = swa_readable, .read = swa_read, .write = swa_write,
    .in_frame = swa_in_frame, .frame_id = swa_frame_id, .site = swa_site, .fn_at = swa_fn_at, .actor_name = swa_actor_name,
    .call = swa_call, .vcall = swa_vcall, .ai_walk = swa_ai_walk, .ai_leaf = swa_ai_leaf,
    .vt_hook = swa_vt_hook, .vt_unhook = swa_vt_unhook, .hook_orig = swa_hook_orig,
    .tap_set = swa_tap_set, .esync_actor = swa_esync_actor,
    .taps_called = (1u << SW_TAP_PUPPET_ATTACK) | (1u << SW_TAP_PUPPET_AI),
};

SW_EXPORT const SwApi *sw_api(uint32_t want) { return want > SW_API_VERSION ? NULL : &g_swapi; }

void sw_api_dll_switch(void) {
    int n = 0;
    AcquireSRWLockExclusive(&g_sw_lock);
    for (int i = 0; i < SW_HOOKS; i++)
        if (g_sw.h[i].used) swa_unhook_locked(i, true), n++;
    sw_core_taps_clear(&g_sw);
    ReleaseSRWLockExclusive(&g_sw_lock);
    LOG("[swapi] dll switch: %d hooks unhooked, taps cleared\n", n);
}

RT_ON_RESET(sw_api_reset) {  // reboot: guest memory is wiped; forget the HOOK table WITHOUT writing slots. The taps stay: they point into the DLL, which survives a reboot
    AcquireSRWLockExclusive(&g_sw_lock);
    sw_core_hooks_clear(&g_sw);
    ReleaseSRWLockExclusive(&g_sw_lock);
}

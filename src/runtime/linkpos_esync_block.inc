// ======================================================================================================================
// s168 ESYNC (exe side): "enemies act the same on every game". Spec docs/specs/2026-11-07-multiplayer-enemy-sync-design.md, R1 =
// recon/S168_HELPER_ESYNC_R1.md, report recon/S168_HELPER_ESYNC_EXE.md. The DLL (sora_net_esync.h) decides WHO owns an enemy and
// calls the six linkpos_esync_* exports below (GetProcAddress); this is the hands. (Included by linkpos.c near its end.)
//   brain    RootAi::calc hook: an actor in g_esx_hook returns at once (physics, animation, damage keep running)
//   read     the owner's body: pos / yaw / the 2 AS slots (name hash, bank, change counter) / leaf action id / flags
//   puppet   Actor vslot 85 setMtx (pos+yaw, upright) + ASList startAnimation when the owner's AS changed
//   sensors  owner: enter_ / calc_ / leave_ of ActivateAttackSensor queue ON / OFF events; puppet: the same activate / deactivate helpers
// Gate: SAVAGEWIND_LINK_ESYNC=1 (off = no hook installed, exports do nothing). SAVAGEWIND_LINK_ESYNC_LOG=1 = log lines.
//   SAVAGEWIND_LINK_ESYNC_BLEND=<f>  startAnimation's s0 (GUESS: blend-in, default 0)
// Exports run on the game thread inside linkpos_frame (the DLL ticks from sora_frame); anything that calls guest code refuses
// outside it. Every pointer is checked (in_guest + readable), the actor must be in the enemy list (seen lately) and its HashId must still
// match; a failed check = skip + counter. The owner path (brain on) is only observed, never changed.
// ======================================================================================================================
#include "linkpos_esync.h"
#define ES_ASLIST 0x560       // Actor mASList (R1 (c)); == [aslist_none_got] means none
#define ES_ATK_NAME 0x88      // ActivateAttackSensor: SafeString AtkSensorName at +0x80, its char* at +0x88 (enter_ passes action+0x80 as the name)
#define ES_ATK_DONE 0xa4      // u8: enter_ writes 0, calc_ writes 1 when it ended the sensor
#define ES_SLOTS 2
#define ES_FL_SENSOR 1
#define ES_FL_DEAD 2

typedef struct {
    uint32_t hid; float pos[3], yaw; uint32_t anim_hash[2]; uint8_t anim_bank[2], anim_chg[2]; float frame; uint16_t action, flags;
} LinkposEsState;  // = the DLL's NetEsEnt (40 B)
_Static_assert(sizeof(LinkposEsState) == 40, "LinkposEsState is the DLL's 40 B state");
_Static_assert(sizeof(EsxEv) == 28, "EsxEv is the DLL's NetEsEv");

static SRWLOCK g_esx_lock = SRWLOCK_INIT;  // names, off set, hook-table writes, events, trk, pup (never held across a guest call)
static EsxNameTab g_esx_names;
static EsxOff g_esx_off;
static EsxHook g_esx_hook;
static EsxEvQ g_esx_evq;
static EsxTrk g_esx_trk;
static EsxPup g_esx_pup;
static float g_esx_blend;
static GuestFn g_esx_rootai, g_esx_enter, g_esx_calc, g_esx_leave;
static struct {
    uint64_t enum_calls, reads, read_fail, puppets, p_nocpu, p_unloaded, p_notpup, p_bad, p_dead, p_vt, p_mtx, p_snap, a_start, a_same, a_unknown,
        a_defs, a_defs_fail, brain_off, brain_on, brain_skip, brain_full, brain_unloaded, ev_on, ev_off, ev_nolist, ev_nohid, ev_noname,
        s_on, s_off, s_unknown, s_forgot, s_dead, s_learn, s_fail, names_cur,
        brain_delay, brain_force, off_drop, s_local, s_wire, s_dup, s_pend, act_cached,  // (S168 review fixes)
        brain_tap;  // (s181: SW_TAP_PUPPET_AI calls)
} g_esx_st;
#define ESX_SON_CAP 120       // frames a freeze waits for our own attack sensor to end (S168 MF-2); then esync_frame switches it off itself

#define ESX_LOG(...) do { if (g_esync_log) LOG(__VA_ARGS__); } while (0)

static bool esx_rd64(uint64_t a, uint64_t *v) {
    if (!in_guest(a) || readable(a, 8) < 8) return false;
    *v = *(const uint64_t *)(uintptr_t)a;
    return true;
}
static bool esx_rd32(uint64_t a, int32_t *v) {
    if (!in_guest(a) || readable(a, 4) < 4) return false;
    *v = *(const int32_t *)(uintptr_t)a;
    return true;
}
// a NUL-terminated printable-ASCII name of 1..cap-1 bytes at guest address p (BotW's AS / sensor / define names are ASCII)
static bool esx_cstr(uint64_t p, char *out, size_t cap) {
    if (!in_guest(p)) return false;
    size_t n = readable(p, cap);
    if (n < 2) return false;
    const char *s = (const char *)(uintptr_t)p;
    size_t i = 0;
    for (; i < n && i < cap - 1; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (!ch) break;
        if (ch < 0x20 || ch > 0x7e) return false;
        out[i] = (char)ch;
    }
    if (!i || i >= cap - 1 || (i < n && s[i])) return false;
    out[i] = 0;
    return true;
}

// the enemy-list row of a HashId (newest sighting within maxage frames)
static bool esx_find(uint32_t hid, uint64_t maxage, uint64_t *actor, int32_t *life) {
    bool ok = false;
    uint64_t best = 0;
    AcquireSRWLockExclusive(&g_enemy_lock);
    for (int i = 0; i < g_nenemy; i++)
        if (g_enemy[i].hid == hid && g_vel_frame_id - g_enemy[i].seen <= maxage && (!ok || g_enemy[i].seen > best))
            ok = true, best = g_enemy[i].seen, *actor = g_enemy[i].actor, *life = g_enemy[i].life;
    ReleaseSRWLockExclusive(&g_enemy_lock);
    return ok;
}
// ... plus the checks that the object is still the one we think: readable, HashId unchanged, not being deleted
static bool esx_actor_ok(uint64_t a, uint32_t hid, int32_t *life) {
    if (!in_guest(a) || readable(a, 0x850) < 0x850) return false;
    if (actor_hid(a) != hid) return false;
    if (*(const uint8_t *)(uintptr_t)(a + BASEPROC_STATE_OFF) == 3) return false;
    if (life) *life = *(const int32_t *)(uintptr_t)(a + ACTOR_LIFE);
    return true;
}

// ---- brain ----
static void esx_rootai_hook(Cpu *c) {  // RootAi::calc(RootAi*): per enemy per frame, any thread
    uint64_t ai = c->x[0], actor;
    if (g_esx_hook.live && in_guest(ai) && *(const uint64_t *)(uintptr_t)ai == SS->rootai_vptr) {
        actor = *(const uint64_t *)(uintptr_t)(ai + 8);
        if (esx_hook_has(&g_esx_hook, actor)) {
            g_esx_st.brain_skip++;
            // s181 R2: the DLL runs the copy's attack action here, in the game's own AI phase (from the net tick its calc_ took 70-100 ms)
            SwTapPuppetAiFn tap = (SwTapPuppetAiFn)sw_tap_get(SW_TAP_PUPPET_AI);
            if (tap) g_esx_st.brain_tap++, tap(c, actor, ai);
            return;
        }
    }
    g_esx_rootai(c);
}

// resolve the wanted-off HashIds to the actors now loaded here, and make the hook table match (game thread: esync_frame / brain)
static void esx_resolve(void) {
    uint32_t hids[ESX_OFF_MAX], gone[ESX_OFF_MAX];
    int n, ng = 0;
    AcquireSRWLockExclusive(&g_esx_lock);
    n = g_esx_off.n;
    memcpy(hids, g_esx_off.hid, sizeof hids[0] * (size_t)n);
    ReleaseSRWLockExclusive(&g_esx_lock);
    uint64_t want[ESX_OFF_MAX];
    uint32_t whid[ESX_OFF_MAX];
    int nw = 0;
    for (int i = 0; i < n; i++) {
        uint64_t a, ai = 0;
        int32_t life = 0;
        // S168 MF-1: not seen for ENEMY_STALE frames = unloaded here: the exe forgets it (never trusts a DLL that may be swapped / stop ticking);
        // the DLL tells brain(0) again when it reloads (and every ES_BRAIN_REFRESH_MS)
        if (!esx_find(hids[i], ENEMY_STALE, &a, &life)) { gone[ng++] = hids[i]; continue; }
        if (!esx_actor_ok(a, hids[i], &life)) continue;
        if (life <= 0) continue;  // (dying: its brain must run the death; E2 already took the life)
        if (!esx_rd64(a + ACTOR_ROOT_AI, &ai) || !in_guest(ai) || readable(ai, 16) < 16 || *(const uint64_t *)(uintptr_t)ai != SS->rootai_vptr) continue;
        want[nw] = a, whid[nw++] = hids[i];
    }
    AcquireSRWLockExclusive(&g_esx_lock);
    for (int i = 0; i < ng; i++)
        if (esx_off_del(&g_esx_off, gone[i])) { g_esx_st.off_drop++; ESX_LOG("[linkpos] esync hid=%08x unloaded: brain-off forgotten\n", gone[i]); }
    for (int i = 0; i < ESX_HOOK_MAX; i++) {
        uint64_t a = g_esx_hook.a[i];
        if (!a) continue;
        bool keep = false;
        for (int k = 0; k < nw; k++) keep |= want[k] == a;
        if (!keep) { esx_hook_del(&g_esx_hook, a); ESX_LOG("[linkpos] esync brain ON again for actor %llx\n", (unsigned long long)a); }
    }
    for (int k = 0; k < nw; k++) {
        // S168 MF-2: never freeze a brain whose own attack sensor is on (calc_ / leave_ would not run: the hitbox stays live); wait for its OFF
        if (!esx_hook_has(&g_esx_hook, want[k]) && esx_trk_may_freeze(esx_trk_find(&g_esx_trk, whid[k]), g_vel_frame_id, ESX_SON_CAP) != 1) { g_esx_st.brain_delay++; continue; }
        int r = esx_hook_add(&g_esx_hook, want[k]);
        if (r == 1) ESX_LOG("[linkpos] esync brain OFF hid=%08x actor %llx\n", whid[k], (unsigned long long)want[k]);
        else if (r < 0) g_esx_st.brain_full++;
    }
    ReleaseSRWLockExclusive(&g_esx_lock);
}

void linkpos_esync_brain(uint32_t hid, int on) {
    if (!g_esync_on || !hid) return;
    AcquireSRWLockExclusive(&g_esx_lock);
    int r = on ? esx_off_del(&g_esx_off, hid) : esx_off_add(&g_esx_off, hid);
    ReleaseSRWLockExclusive(&g_esx_lock);
    if (r < 0) { g_esx_st.brain_full++; return; }
    if (r) { if (on) g_esx_st.brain_on++; else g_esx_st.brain_off++; }
    uint64_t a;
    int32_t life;
    if (!on && !esx_find(hid, ENEMY_STALE, &a, &life)) g_esx_st.brain_unloaded++;  // (not loaded: esx_resolve forgets it at once; the DLL asks again on reload)
    esx_resolve();
}

// ---- owner: reading the body ----
// ASList slot -> the running AS. 1 = running (name, bank, obj, fin set), 0 = list ok but nothing runs, -1 = no / invalid list
static int esx_as_read(uint64_t actor, int slot, uint8_t *bank, uint64_t *obj, int *fin, char *name) {
    *bank = 0, *obj = 0, *fin = 0, name[0] = 0;
    uint64_t asl, none = 0, slots;
    int32_t ns;
    if (!esx_rd64(actor + ES_ASLIST, &asl) || !asl || !in_guest(asl)) return -1;
    if (SS->aslist_none_got && esx_rd64(SS->aslist_none_got, &none) && asl == none) return -1;
    if (readable(asl, 0xd0) < 0xd0) return -1;
    ns = *(const int32_t *)(uintptr_t)(asl + 0xb8), slots = *(const uint64_t *)(uintptr_t)(asl + 0xc0);
    if (slot < 0 || slot >= ns || ns > 16 || !in_guest(slots)) return -1;
    uint64_t sp = slots + (uint64_t)slot * 0x50, banks;
    int32_t nb;
    if (readable(sp, 0x30) < 0x30) return -1;
    nb = *(const int32_t *)(uintptr_t)(sp + 0x20), banks = *(const uint64_t *)(uintptr_t)(sp + 0x28);
    if (nb <= 0 || nb > 16 || !in_guest(banks)) return -1;
    for (int b = 0; b < nb; b++) {
        uint64_t be = banks + (uint64_t)b * 0x98, o, sb, np;
        if (readable(be, 0x48) < 0x48) return -1;
        o = *(const uint64_t *)(uintptr_t)(be + 0x18);
        if (!o) continue;
        sb = *(const uint64_t *)(uintptr_t)be;
        if (!in_guest(sb) || !esx_rd64(sb + 0x10, &np) || !esx_cstr(np, name, ESX_NAME_LEN)) { name[0] = 0; continue; }
        *bank = (uint8_t)b, *obj = o, *fin = *(const uint16_t *)(uintptr_t)(be + 0x40) & 1;
        return 1;
    }
    return 0;
}

// ---- s174 R2: a puppet runs its OWN node of the owner's attack action (recon/s174/S174_ATK_VPTRS.md) ----
// Bokoblin clubs have no ActivateAttackSensor: the attack action's calc_ switches the weapon hit from its own AS window. So the
// puppet enters / calcs / leaves its own node of that action (vslots 12 / 31 / 14) and that game's Link is hit the normal way.
#define ESX_ATK_VT_MAX 12
#define ESX_LEAF_DEPTH 32  // (the owner leaf walk: one get_type call per level, every 3rd frame per owned enemy)
#define ESX_ATK_CAP 180    // frames (3 s) one local attack may run
#define ESX_ATK_GAP 20     // frames before the same index may run again here (GUESS)
#define ESX_ATK_STALE 30   // no puppet update for this many frames: leave it
static uint64_t g_esx_atk_vt[ESX_ATK_VT_MAX];
static volatile int g_esx_atk_nvt, g_esx_atk_on;
static struct { uint64_t enter, leave, fin, cap, swap, nonode, walk, walk_nodes, skip_as, stale, bad_fn, forgot; } g_esx_as;
static void esx_atk_log(void);
static bool esx_atk_vt_is(uint64_t vt) {
    if (!vt) return false;
    for (int i = 0; i < g_esx_atk_nvt; i++) if (g_esx_atk_vt[i] == vt) return true;
    return false;
}
static void esx_atk_defaults(void) {
    const uint64_t d[] = {SS->atkact_attack_vt, SS->atkact_stepdouble_vt, SS->atkact_large_vt, SS->atkact_jump_vt, SS->atkact_forkweapon_vt, SS->atkact_animtiming_vt};
    int n = 0;
    for (size_t i = 0; i < sizeof d / sizeof *d; i++) if (d[i]) g_esx_atk_vt[n++] = d[i];
    g_esx_atk_nvt = n;
}
// the DLL: on 0 / 1 (-1 = keep); vts + n > 0 = replace the class set (a hot fix without a relink). Returns the classes in use (0 = off), -1 = esync off.
int linkpos_esync_atk(int on, const uint64_t *vts, int n) {
    if (vts && n > 0) {
        int k = 0;
        for (int i = 0; i < n && k < ESX_ATK_VT_MAX; i++) if (vts[i]) g_esx_atk_vt[k++] = vts[i];
        g_esx_atk_nvt = k;
    }
    if (on >= 0) g_esx_atk_on = on != 0 && g_esx_atk_nvt > 0;
    LOG("[linkpos] esync attack actions %s (%d classes)\n", g_esx_atk_on ? "on" : "off", g_esx_atk_nvt);
    return g_esync_on ? (g_esx_atk_on ? g_esx_atk_nvt : 0) : -1;
}
// owner-side diag (esynclog): each new (action index, class vptr) pair once
static uint64_t g_esx_leafseen[256];
static int g_esx_nleafseen;
static void esx_leaf_note(uint16_t idx, uint64_t vt) {
    if (!g_esync_log || !idx) return;
    uint64_t k = (vt << 16) | idx;
    for (int i = 0; i < g_esx_nleafseen; i++) if (g_esx_leafseen[i] == k) return;
    if (g_esx_nleafseen >= 256) return;
    g_esx_leafseen[g_esx_nleafseen++] = k;
    LOG("[linkpos] esync leaf idx %u vptr %llx%s\n", (unsigned)idx, (unsigned long long)vt, esx_atk_vt_is(vt) ? " (attack)" : "");
}

// the leaf action's definition index (R1: node+0x18), walking the RootAi tree like linkpos_enemy_leaf; 0 = not found / not now
static uint16_t esx_leaf_action(uint64_t actor, Cpu *c) {
    if (!c) return 0;
    uint64_t node;
    if (!esx_rd64(actor + ACTOR_ROOT_AI, &node)) return 0;
    uint16_t res = 0;
    Cpu save = *c;
    uint64_t sp = (c->sp - 0x40) & ~0xfull;
    // (s174: Bokoblin attacks sit 5-15 deep under Root (recon/s173/bok_aiprog.yml); AI_DEPTH_MAX 8 sent act 0 for every swing)
    for (int depth = 0; depth < ESX_LEAF_DEPTH && in_guest(node) && readable(node, 0x40) >= 0x40; depth++) {
        if (*(const uint64_t *)(uintptr_t)(node + ACTION_ACTOR) != actor) break;
        uint64_t vt = *(const uint64_t *)(uintptr_t)node, gt = in_guest(vt) ? *(const uint64_t *)(uintptr_t)(vt + 8 * VT_AI_GET_TYPE) : 0;
        if (!gt || !dispatch_lookup(gt)) break;
        c->sp = sp, c->x[0] = node;
        guest_call(c, gt);
        if ((int32_t)c->x[0] != 0) { res = *(const uint16_t *)(uintptr_t)(node + 0x18); esx_leaf_note(res, vt); break; }  // (an Action / Behavior: the leaf)
        uint32_t idx = *(const uint16_t *)(uintptr_t)(node + AI_CHILD_IDX), cnt = *(const uint32_t *)(uintptr_t)(node + AI_NCHILD);
        uint64_t kids = *(const uint64_t *)(uintptr_t)(node + AI_CHILDREN);
        if (idx == 0xffff || !cnt || cnt > 64 || !in_guest(kids) || readable(kids, 8ull * cnt) < 8ull * cnt) break;
        node = *(const uint64_t *)(uintptr_t)(kids + 8ull * (idx < cnt ? idx : 0));
    }
    *c = save;
    return res;
}

int linkpos_esync_enum(uint32_t *hids, int max) {
    if (!g_esync_on || !hids || max <= 0) return 0;
    g_esx_st.enum_calls++;
    int n = 0;
    AcquireSRWLockExclusive(&g_enemy_lock);
    for (int i = 0; i < g_nenemy && n < max; i++) {
        uint32_t h = g_enemy[i].hid;
        if (!h || g_enemy[i].life <= 0 || g_vel_frame_id - g_enemy[i].seen > 3) continue;
        int k = 0;
        while (k < n && hids[k] != h) k++;
        if (k == n) hids[n++] = h;
    }
    ReleaseSRWLockExclusive(&g_enemy_lock);
    return n;
}

int linkpos_esync_read(uint32_t hid, LinkposEsState *out) {
    if (!g_esync_on || !out || !hid) return 0;
    uint64_t a;
    int32_t life;
    if (!esx_find(hid, 3, &a, &life) || !esx_actor_ok(a, hid, &life) || readable(a + ACTOR_MTX, 48) < 48) { g_esx_st.read_fail++; return 0; }
    const float *m = (const float *)(uintptr_t)(a + ACTOR_MTX);
    if (!isfinite(m[3]) || !isfinite(m[7]) || !isfinite(m[11]) || !isfinite(m[2]) || !isfinite(m[10])) { g_esx_st.read_fail++; return 0; }
    memset(out, 0, sizeof *out);
    out->hid = hid, out->pos[0] = m[3], out->pos[1] = m[7], out->pos[2] = m[11], out->yaw = esx_yaw(m);
    char nm[ESX_NAME_LEN];
    AcquireSRWLockExclusive(&g_esx_lock);
    EsxTrkEnt *t = esx_trk_get(&g_esx_trk, hid, g_vel_frame_id);
    for (int s = 0; s < ES_SLOTS; s++) {
        uint8_t bank;
        uint64_t obj;
        int fin;
        // (guest memory reads only, no guest call: safe under the lock)
        int r = esx_as_read(a, s, &bank, &obj, &fin, nm);
        uint32_t h = 0;
        if (r == 1) { esx_name_put(&g_esx_names, nm); h = esx_hash(nm); }
        else obj = 0, fin = 0, bank = 0;
        out->anim_hash[s] = h, out->anim_bank[s] = bank;
        out->anim_chg[s] = esx_trk_slot(t, s, h, obj, fin);
    }
    out->flags = (uint16_t)((t->son ? ES_FL_SENSOR : 0) | (life <= 0 ? ES_FL_DEAD : 0));
    // S168 SF-4: the leaf action costs up to 8 guest calls; puppets do not need it, owned ones get it every 3rd frame
    bool pup = esx_off_has(&g_esx_off, hid) != 0, need_act = !pup && (!t->act_frame || g_vel_frame_id - t->act_frame >= 3);
    uint16_t act = pup ? 0 : t->action;
    ReleaseSRWLockExclusive(&g_esx_lock);
    out->frame = 0;  // (the AS phase is not found yet: R1 "not resolved", R2)
    if (need_act) {
        act = esx_leaf_action(a, g_in_frame ? kernel_current_cpu() : NULL);
        AcquireSRWLockExclusive(&g_esx_lock);
        EsxTrkEnt *t2 = esx_trk_find(&g_esx_trk, hid);  // (t may have been evicted by a hook thread meanwhile)
        if (t2) t2->action = act, t2->act_frame = g_vel_frame_id ? g_vel_frame_id : 1;
        ReleaseSRWLockExclusive(&g_esx_lock);
    } else if (!pup) g_esx_st.act_cached++;
    out->action = act;
    g_esx_st.reads++;
    return 1;
}

// ---- the attack-sensor events (owner side) ----
// The params enter_ passes to the AttackInfo filler 0x7000c35ed0: w1 = *[act+0x48], w2 = *[act+0x50], w3 = *[act+0x20] (or +0x90 when *[act+0x78] != 0),
// w4 = *[act+0x28]. Those four are what travels (the other four -- w5 w6 w7 and the float -- stay what the puppet's own record holds, GUESS enough).
static bool esx_atk_params(uint64_t act, uint32_t p[4]) {
    uint64_t pa[4], flag = 0;
    if (!esx_rd64(act + 0x48, &pa[0]) || !esx_rd64(act + 0x50, &pa[1]) || !esx_rd64(act + 0x28, &pa[3]) || !esx_rd64(act + 0x78, &flag)) return false;
    if (!in_guest(flag) || readable(flag, 1) < 1) return false;
    uint8_t fb = *(const uint8_t *)(uintptr_t)flag;
    if (!esx_rd64(act + (fb ? 0x90 : 0x20), &pa[2])) return false;
    for (int i = 0; i < 4; i++) {
        int32_t v;
        if (!esx_rd32(pa[i], &v)) return false;
        p[i] = (uint32_t)v;
    }
    return true;
}

static void esx_atk_event(uint64_t act, int on) {  // an ActivateAttackSensor action turned its sensor on / off (hook threads)
    uint64_t actor, np;
    char nm[ESX_NAME_LEN];
    if (!esx_rd64(act + 8, &actor) || !in_guest(actor) || readable(actor, 0x850) < 0x850) return;
    if (esx_hook_has(&g_esx_hook, actor)) return;  // (a puppet: not ours to report)
    const char *an = actor_name(actor);
    if (!an || !enemy_name_ok(an) || !enemy_listed(actor, ENEMY_STALE)) { g_esx_st.ev_nolist++; return; }
    uint32_t hid = actor_hid(actor);
    if (!hid) { g_esx_st.ev_nohid++; return; }
    if (readable(act, ES_ATK_NAME + 8) < ES_ATK_NAME + 8 || !esx_rd64(act + ES_ATK_NAME, &np) || !esx_cstr(np, nm, ESX_NAME_LEN)) { g_esx_st.ev_noname++; return; }
    EsxEv ev;
    memset(&ev, 0, sizeof ev);
    ev.on = (uint8_t)on, ev.hid = hid, ev.name = esx_hash(nm);
    if (on && !esx_atk_params(act, ev.params)) { g_esx_st.ev_noname++; return; }
    AcquireSRWLockExclusive(&g_esx_lock);
    esx_name_put(&g_esx_names, nm);
    esx_ev_push(&g_esx_evq, &ev);
    EsxTrkEnt *t = esx_trk_get(&g_esx_trk, hid, g_vel_frame_id);
    t->son = (uint8_t)on;
    if (on) t->son_frame = g_vel_frame_id, t->son_name = ev.name;
    ReleaseSRWLockExclusive(&g_esx_lock);
    if (on) g_esx_st.ev_on++; else g_esx_st.ev_off++;
    ESX_LOG("[linkpos] esync sensor %s hid=%08x %s (%08x) p=%d %d %d %d\n", on ? "ON" : "OFF", hid, nm, ev.name, (int)ev.params[0], (int)ev.params[1],
            (int)ev.params[2], (int)ev.params[3]);
}
static void esx_enter_hook(Cpu *c) { uint64_t act = c->x[0]; g_esx_enter(c); esx_atk_event(act, 1); }
static void esx_calc_hook(Cpu *c) {  // per action per frame: only the 0 -> 1 change of the "done" byte matters
    uint64_t act = c->x[0];
    uint8_t was = in_guest(act) ? *(const volatile uint8_t *)(uintptr_t)(act + ES_ATK_DONE) : 1;
    g_esx_calc(c);
    if (!was && *(const volatile uint8_t *)(uintptr_t)(act + ES_ATK_DONE)) esx_atk_event(act, 0);
}
static void esx_leave_hook(Cpu *c) {
    uint64_t act = c->x[0];
    uint8_t was = in_guest(act) ? *(const volatile uint8_t *)(uintptr_t)(act + ES_ATK_DONE) : 1;
    g_esx_leave(c);
    if (!was) esx_atk_event(act, 0);  // (its sensor was still on: leave_ switched it off)
}

int linkpos_esync_events(EsxEv *out, int max) {
    if (!g_esync_on || !out || max <= 0) return 0;
    AcquireSRWLockExclusive(&g_esx_lock);
    int n = esx_ev_take(&g_esx_evq, out, max);
    ReleaseSRWLockExclusive(&g_esx_lock);
    return n;
}

// ---- puppet: names, scratch, calls ----
// guest scratch below the live sp: +0 SafeString {vt+0x10, +0x40}, +0x40 name (<= 63), +0x100 the Matrix34f
static uint64_t esx_scratch(Cpu *c, uint64_t vt, const char *name) {
    uint64_t sp = (c->sp - 0x200) & ~0xfull;
    uint8_t *g = (uint8_t *)(uintptr_t)sp;
    memset(g, 0, 0x200);
    uint64_t ss[2] = {vt + 0x10, sp + 0x40};
    memcpy(g, ss, 16);
    if (name) strncpy((char *)g + 0x40, name, ESX_NAME_LEN - 1);
    return sp;
}
static uint64_t esx_ss_vt(void) {
    uint64_t vt = 0;
    return esx_rd64(SS->safestring_vt_got, &vt) && in_guest(vt) ? vt : 0;
}

// What the actor's AS define table calls its animations. Layout (R1 0x710149ed3c): [[actor+0x570]+0x98] = A; A+0x2b8 u32 count,
// A+0x2c0 array (stride 0x88). The lookup passes entry+0x18 to SafeString::operator= (vslot 2): entry+0x18 is a sead FixedSafeString
// (+0 vtable, +8 char* mStringTop, +0x10 size, +0x14 text), entry+0x80 the next field. s168 read the text inline at +0x18 = the vtable
// bytes -> `defs fail` in the s169 test. Now: char* at entry+0x20 first, then inline at +0x2c, then +0x18 (each validated).
static void esx_learn_defs(uint64_t actor) {
    uint64_t pa = 0, a = 0, arr = 0;
    int32_t n = 0;
    int learned = 0, how[3] = {0, 0, 0};
    const char *why = NULL;
    if (!esx_rd64(actor + 0x570, &pa) || !in_guest(pa)) why = "actor+0x570";
    else if (!esx_rd64(pa + 0x98, &a) || !in_guest(a)) why = "+0x98";
    else if (!esx_rd32(a + 0x2b8, &n) || n <= 0 || n > 2048) why = "count +0x2b8";
    else if (!esx_rd64(a + 0x2c0, &arr) || !in_guest(arr)) why = "array +0x2c0";
    if (!why) {
        char nm[ESX_NAME_LEN];
        AcquireSRWLockExclusive(&g_esx_lock);
        for (int i = 0; i < n; i++) {
            uint64_t e = arr + (uint64_t)i * 0x88, sp = 0;
            int k = -1;
            if (esx_rd64(e + 0x20, &sp) && esx_cstr(sp, nm, ESX_NAME_LEN)) k = 0;
            else if (esx_cstr(e + 0x2c, nm, ESX_NAME_LEN)) k = 1;
            else if (esx_cstr(e + 0x18, nm, ESX_NAME_LEN)) k = 2;
            if (k < 0) continue;
            how[k]++;
            if (esx_name_put(&g_esx_names, nm) == 1) learned++;
        }
        ReleaseSRWLockExclusive(&g_esx_lock);
        if (!how[0] && !how[1] && !how[2]) why = "no readable names";
    }
    if (!why) g_esx_st.a_defs += (uint64_t)learned; else g_esx_st.a_defs_fail++;
    ESX_LOG("[linkpos] esync define table of %llx: %d entries, names via ptr %d / inline+0x2c %d / inline+0x18 %d, %d new (names known: %d)%s%s\n",
            (unsigned long long)actor, n, how[0], how[1], how[2], learned, g_esx_names.n, why ? ", FAILED at " : "", why ? why : "");
}
// ActivateAttackSensor names of this actor's own AI tree (the puppet's brain never ran them, so enter_ never told us)
static void esx_learn_sensors(uint64_t actor, Cpu *c, EsxPupEnt *p) {  // (p: game thread only, like every g_esx_pup user)
    uint64_t stack[128], root;
    int sp = 0, visited = 0, learned = 0;
    if (!esx_rd64(actor + ACTOR_ROOT_AI, &root) || !in_guest(root)) { g_esx_st.s_fail++; return; }
    Cpu save = *c;
    uint64_t gsp = (c->sp - 0x40) & ~0xfull;
    stack[sp++] = root;
    while (sp && visited < 200) {
        uint64_t node = stack[--sp], vt;
        visited++;
        if (!in_guest(node) || readable(node, 0x90) < 0x90 || *(const uint64_t *)(uintptr_t)(node + ACTION_ACTOR) != actor) continue;
        vt = *(const uint64_t *)(uintptr_t)node;
        if (vt == SS->atk_vptr) {
            char nm[ESX_NAME_LEN];
            uint64_t np;
            if (esx_rd64(node + ES_ATK_NAME, &np) && esx_cstr(np, nm, ESX_NAME_LEN)) {
                AcquireSRWLockExclusive(&g_esx_lock);
                if (esx_name_put(&g_esx_names, nm) == 1) learned++;
                if (p) esx_pup_node_put(p, esx_hash(nm), node);
                ReleaseSRWLockExclusive(&g_esx_lock);
            }
            continue;
        }
        uint64_t gt = in_guest(vt) ? *(const uint64_t *)(uintptr_t)(vt + 8 * VT_AI_GET_TYPE) : 0;
        if (!gt || !dispatch_lookup(gt)) continue;
        c->sp = gsp, c->x[0] = node;
        guest_call(c, gt);
        if ((int32_t)c->x[0] != 0) continue;  // (a leaf that is not a sensor action)
        uint32_t cnt = *(const uint32_t *)(uintptr_t)(node + AI_NCHILD);
        uint64_t kids = *(const uint64_t *)(uintptr_t)(node + AI_CHILDREN);
        if (!cnt || cnt > 48 || !in_guest(kids) || readable(kids, 8ull * cnt) < 8ull * cnt) continue;
        for (uint32_t k = 0; k < cnt && sp < 128; k++) stack[sp++] = *(const uint64_t *)(uintptr_t)(kids + 8ull * k);
    }
    *c = save;
    g_esx_st.s_learn += (uint64_t)learned;
    ESX_LOG("[linkpos] esync learned %d sensor names from the AI tree of %llx (%d nodes)\n", learned, (unsigned long long)actor, visited);
}

// s174: this body's attack-class nodes by definition index (once per body, one get_type call per Ai node). The tree SHARES nodes
// (one instance per AIProgram entry, ~1086 for a Bokoblin, many parents each): s174 first test = 1500 visits for 1 attack node
// -> every node once (pointer set), up to 4000 distinct nodes, a 2048-deep stack. Game thread only (static buffers).
#define ESX_WALK_SET 8192
static uint64_t g_esx_walk_seen[ESX_WALK_SET], g_esx_walk_stack[2048];
static bool esx_walk_mark(uint64_t n) {  // true = first time
    uint32_t h = (uint32_t)((n >> 3) * 2654435761u) & (ESX_WALK_SET - 1);
    for (int i = 0; i < ESX_WALK_SET; i++) {
        uint64_t *e = &g_esx_walk_seen[(h + (uint32_t)i) & (ESX_WALK_SET - 1)];
        if (*e == n) return false;
        if (!*e) return *e = n, true;
    }
    return false;
}
static bool esx_walk_has(uint64_t n) {
    uint32_t h = (uint32_t)((n >> 3) * 2654435761u) & (ESX_WALK_SET - 1);
    for (int i = 0; i < ESX_WALK_SET; i++) {
        uint64_t e = g_esx_walk_seen[(h + (uint32_t)i) & (ESX_WALK_SET - 1)];
        if (e == n) return true;
        if (!e) return false;
    }
    return false;
}
static void esx_atk_walk(uint64_t actor, Cpu *c, EsxPupEnt *p) {
    uint64_t *stack = g_esx_walk_stack, root, fnode[ESX_PUP_ATK];
    uint16_t fidx[ESX_PUP_ATK];
    int sp = 0, visited = 0, nf = 0, dup = 0, full = 0;
    memset(g_esx_walk_seen, 0, sizeof g_esx_walk_seen);
    if (!esx_rd64(actor + ACTOR_ROOT_AI, &root) || !in_guest(root)) root = 0;
    Cpu save = *c;
    uint64_t gsp = (c->sp - 0x40) & ~0xfull;
    if (root) stack[sp++] = root;
    while (sp && visited < 4000) {
        uint64_t node = stack[--sp], vt;
        if (!esx_walk_mark(node)) { dup++; continue; }
        visited++;
        if (!in_guest(node) || readable(node, 0x40) < 0x40 || *(const uint64_t *)(uintptr_t)(node + ACTION_ACTOR) != actor) continue;
        vt = *(const uint64_t *)(uintptr_t)node;
        if (esx_atk_vt_is(vt)) {
            uint16_t ix = *(const uint16_t *)(uintptr_t)(node + 0x18);
            int k = 0;
            while (k < nf && fidx[k] != ix) k++;
            if (k == nf && nf < ESX_PUP_ATK && ix) fidx[nf] = ix, fnode[nf++] = node;
            continue;
        }
        uint64_t gt = in_guest(vt) && readable(vt + 8 * VT_AI_GET_TYPE, 8) == 8 ? *(const uint64_t *)(uintptr_t)(vt + 8 * VT_AI_GET_TYPE) : 0;
        if (!gt || !dispatch_lookup(gt)) continue;
        c->sp = gsp, c->x[0] = node;
        guest_call(c, gt);
        if ((int32_t)c->x[0] != 0) continue;  // (another leaf)
        uint32_t cnt = *(const uint32_t *)(uintptr_t)(node + AI_NCHILD);
        uint64_t kids = *(const uint64_t *)(uintptr_t)(node + AI_CHILDREN);
        if (!cnt || cnt > 64 || !in_guest(kids) || readable(kids, 8ull * cnt) < 8ull * cnt) continue;
        for (uint32_t k = 0; k < cnt; k++) {
            uint64_t kid = *(const uint64_t *)(uintptr_t)(kids + 8ull * k);
            if (esx_walk_has(kid)) { dup++; continue; }  // (shared node already done: do not even push it)
            if (sp < 2048) stack[sp++] = kid; else full++;
        }
    }
    *c = save;
    AcquireSRWLockExclusive(&g_esx_lock);
    p->atk_walked = 1;
    for (int i = 0; i < nf; i++) esx_pup_atk_put(p, fidx[i], fnode[i]);
    ReleaseSRWLockExclusive(&g_esx_lock);
    g_esx_as.walk++, g_esx_as.walk_nodes += (uint64_t)visited;
    ESX_LOG("[linkpos] esync attack walk of %llx: %d attack nodes in %d distinct nodes (%d shared skips, %d stack-full)\n", (unsigned long long)actor, nf, visited, dup, full);
    for (int i = 0; i < nf; i++) ESX_LOG("[linkpos] esync   attack node idx %u = %llx vptr %llx\n", (unsigned)fidx[i], (unsigned long long)fnode[i], (unsigned long long)*(const uint64_t *)(uintptr_t)fnode[i]);
}
// one virtual of an action node (12 enter_ with a zeroed InlineParamPack, 31 calc_, 14 leave_)
static bool esx_atk_vcall(Cpu *c, uint64_t node, int slot) {
    uint64_t vt = in_guest(node) && readable(node, 0x20) >= 0x20 ? *(const uint64_t *)(uintptr_t)node : 0;
    uint64_t fn = in_guest(vt) && readable(vt + 8ull * slot, 8) == 8 ? *(const uint64_t *)(uintptr_t)(vt + 8ull * slot) : 0;
    if (!fn || !dispatch_lookup(fn)) { g_esx_as.bad_fn++; return false; }
    Cpu save = *c;
    uint64_t sp = (c->sp - 0x800) & ~0xfull;
    memset((void *)(uintptr_t)(sp + 0x40), 0, 0x600);
    c->sp = sp, c->x[0] = node, c->x[1] = sp + 0x40;
    guest_call(c, fn);
    *c = save;
    return true;
}
static void esx_atk_end(Cpu *c, EsxPupEnt *p, uint8_t chg0, const char *why) {
    uint64_t node, actor;
    uint16_t idx;
    uint32_t hid;
    AcquireSRWLockExclusive(&g_esx_lock);
    node = p->atk_node, actor = p->actor, idx = p->atk_idx, hid = p->hid;
    p->atk_node = 0, p->atk_last = p->atk_idx, p->atk_lchg = chg0, p->atk_end = g_vel_frame_id;
    ReleaseSRWLockExclusive(&g_esx_lock);
    if (!node) return;
    if (c && readable(node, 0x40) >= 0x40 && *(const uint64_t *)(uintptr_t)(node + ACTION_ACTOR) == actor) esx_atk_vcall(c, node, 14);
    g_esx_as.leave++;
    ESX_LOG("[linkpos] esync puppet hid=%08x attack idx %u left (%s)\n", hid, (unsigned)idx, why);
}
// per puppet frame (linkpos_esync_puppet): true while a local attack runs (its enter_ started the slot-0 AS itself)
static bool esx_atk_frame(Cpu *c, uint64_t a, EsxPupEnt *p, const LinkposEsState *t) {
    AcquireSRWLockExclusive(&g_esx_lock);
    bool walk = g_esx_atk_on && t->action && !p->atk_walked;
    bool any = g_esx_atk_on || p->atk_node;
    ReleaseSRWLockExclusive(&g_esx_lock);
    if (!any) return false;
    if (walk) esx_atk_walk(a, c, p);
    AcquireSRWLockExclusive(&g_esx_lock);
    uint64_t want = g_esx_atk_on && t->action ? esx_pup_atk_node(p, t->action) : 0;
    int step = esx_atk_step(p, t->action, want != 0, t->anim_chg[0], g_vel_frame_id, ESX_ATK_CAP, ESX_ATK_GAP, 1);
    bool capped = p->atk_node && g_vel_frame_id - p->atk_frame > ESX_ATK_CAP;
    p->atk_seen = g_vel_frame_id;
    ReleaseSRWLockExclusive(&g_esx_lock);
    if (step == ESX_ATK_STEP_LEAVE || step == ESX_ATK_STEP_SWAP) {
        if (capped) g_esx_as.cap++;
        if (step == ESX_ATK_STEP_SWAP) g_esx_as.swap++;
        esx_atk_end(c, p, t->anim_chg[0], capped ? "3 s cap" : step == ESX_ATK_STEP_SWAP ? "owner swapped attack" : "owner's action changed");
    }
    if (step == ESX_ATK_STEP_ENTER || step == ESX_ATK_STEP_SWAP) {
        if (!want || readable(want, 0x40) < 0x40 || *(const uint64_t *)(uintptr_t)(want + ACTION_ACTOR) != a) { g_esx_as.nonode++; return false; }
        *(volatile uint8_t *)(uintptr_t)(want + 0x1b) &= 0xfc;  // (finished / failed: ActionBase::enter clears them; we call enter_ itself. GUESS bits)
        if (!esx_atk_vcall(c, want, 12)) return false;
        AcquireSRWLockExclusive(&g_esx_lock);
        p->atk_node = want, p->atk_idx = t->action, p->atk_frame = g_vel_frame_id;
        ReleaseSRWLockExclusive(&g_esx_lock);
        g_esx_as.enter++;
        ESX_LOG("[linkpos] esync puppet hid=%08x runs its own attack idx %u (node %llx vptr %llx)\n", p->hid, (unsigned)t->action, (unsigned long long)want,
                (unsigned long long)*(const uint64_t *)(uintptr_t)want);
    }
    uint64_t node = p->atk_node;
    if (!node) return false;
    if (!esx_atk_vcall(c, node, 31)) { esx_atk_end(c, p, t->anim_chg[0], "calc_ missing"); return false; }
    if (*(const volatile uint8_t *)(uintptr_t)(node + 0x1b) & 1) { g_esx_as.fin++; esx_atk_end(c, p, t->anim_chg[0], "finished"); return false; }
    return true;
}

// ---- puppet: the per-frame body + animation ----
void linkpos_esync_puppet(uint32_t hid, const LinkposEsState *t, int snap) {
    if (!g_esync_on || !t || !hid) return;
    Cpu *c = g_in_frame ? kernel_current_cpu() : NULL;
    if (!c) { g_esx_st.p_nocpu++; return; }
    if (!isfinite(t->pos[0]) || !isfinite(t->pos[1]) || !isfinite(t->pos[2]) || !isfinite(t->yaw)) { g_esx_st.p_bad++; return; }
    bool off;
    AcquireSRWLockExclusive(&g_esx_lock);
    off = esx_off_has(&g_esx_off, hid) != 0;
    ReleaseSRWLockExclusive(&g_esx_lock);
    if (!off) { g_esx_st.p_notpup++; return; }  // (never touch an enemy whose brain we did not stop: the owner path stays untouched)
    uint64_t a, vt, fn;
    int32_t life;
    if (!esx_find(hid, 3, &a, &life)) { g_esx_st.p_unloaded++; return; }
    if (!esx_actor_ok(a, hid, &life)) { g_esx_st.p_bad++; return; }
    if (life <= 0) { g_esx_st.p_dead++; return; }
    if (!esx_hook_has(&g_esx_hook, a)) { g_esx_st.p_notpup++; return; }  // (S168 SF-3: brain not really frozen yet (delayed / table full / other vptr): never fight it)
    uint64_t ssvt = esx_ss_vt();
    vt = *(const uint64_t *)(uintptr_t)a;
    fn = in_guest(vt) ? *(const uint64_t *)(uintptr_t)(vt + 8 * 85) : 0;
    if (!ssvt || fn != SS->actor_set_mtx || !dispatch_lookup(fn)) { g_esx_st.p_vt++; return; }
    g_esx_st.puppets++;

    // 1. body: Actor::setMtx(actor, mtx, 1 = model too, snap). Upright (pitch / roll 0). The leftover controller velocity is NOT zeroed (R1: only a GUESS that it is enough).
    Cpu save = *c;
    uint64_t sp = esx_scratch(c, ssvt, NULL);
    esx_mtx((float *)(uintptr_t)(sp + 0x100), t->pos, t->yaw);
    c->sp = sp, c->x[0] = a, c->x[1] = sp + 0x100, c->x[2] = 1, c->x[3] = snap ? 1 : 0;
    guest_call(c, fn);
    *c = save;
    g_esx_st.p_mtx++;
    if (snap) g_esx_st.p_snap++;

    // 2. animation: per slot, when the owner's AS (hash or change counter) differs from what we applied
    AcquireSRWLockExclusive(&g_esx_lock);
    EsxPupEnt *p = esx_pup_get(&g_esx_pup, hid, a, 1);
    bool learn = p && !p->defs_learned;
    if (p) p->defs_learned = 1;
    ReleaseSRWLockExclusive(&g_esx_lock);
    if (!p) return;
    if (learn) esx_learn_defs(a);
    // (s175: the DLL's attack runner, when it set the tap; else the exe runner, unchanged)
    bool atk_run;
    SwTapPuppetAttackFn tapfn = (SwTapPuppetAttackFn)sw_tap_get(SW_TAP_PUPPET_ATTACK);
    if (tapfn) {
        if (p->atk_node) esx_atk_end(c, p, t->anim_chg[0], "dll runner took over");
        atk_run = tapfn(c, hid, a, t, life > 0) != 0;
    } else
        atk_run = esx_atk_frame(c, a, p, t);  // (s174 R2)
    for (int s = 0; s < ES_SLOTS; s++) {
        uint32_t h = t->anim_hash[s];
        char name[ESX_NAME_LEN];
        bool known = false, first;
        AcquireSRWLockExclusive(&g_esx_lock);
        int need = esx_pup_need(p, s, h, t->anim_chg[s]);
        if (need) {
            const char *nm = esx_name_get(&g_esx_names, h);
            if (nm) memcpy(name, nm, sizeof name), known = true;
        }
        first = !p->have[s];
        ReleaseSRWLockExclusive(&g_esx_lock);
        if (!need) continue;
        if (s == 0 && atk_run) {  // (s174: the local attack's enter_ started its own AS: mark the owner's as applied, do not restart it)
            AcquireSRWLockExclusive(&g_esx_lock);
            esx_pup_done(p, s, h, t->anim_chg[s]);
            ReleaseSRWLockExclusive(&g_esx_lock);
            g_esx_as.skip_as++;
            continue;
        }
        if (!known) { g_esx_st.a_unknown++; continue; }
        uint64_t asl, obj;
        char cur[ESX_NAME_LEN];
        uint8_t bank;
        int fin;
        if (first && esx_as_read(a, s, &bank, &obj, &fin, cur) == 1 && !strcmp(cur, name)) {  // (hand-over / first sample: already playing it, do not restart)
            AcquireSRWLockExclusive(&g_esx_lock);
            esx_pup_done(p, s, h, t->anim_chg[s]);
            ReleaseSRWLockExclusive(&g_esx_lock);
            g_esx_st.a_same++;
            continue;
        }
        uint64_t none = 0;
        if (!esx_rd64(a + ES_ASLIST, &asl) || !in_guest(asl) || (SS->aslist_none_got && esx_rd64(SS->aslist_none_got, &none) && asl == none)) { g_esx_st.p_bad++; continue; }
        Cpu sv = *c;
        uint64_t sp2 = esx_scratch(c, ssvt, name);
        float blend = g_esx_blend, phase = -1.0f;
        c->v[0] = 0, c->v[1] = 0;
        memcpy(&c->v[0], &blend, 4), memcpy(&c->v[1], &phase, 4);
        c->sp = sp2, c->x[0] = asl, c->x[1] = sp2, c->x[2] = (uint64_t)s, c->x[3] = t->anim_bank[s], c->x[4] = 1;
        guest_call(c, SS->as_start);
        *c = sv;
        AcquireSRWLockExclusive(&g_esx_lock);
        esx_pup_done(p, s, h, t->anim_chg[s]);
        ReleaseSRWLockExclusive(&g_esx_lock);
        g_esx_st.a_start++;
        ESX_LOG("[linkpos] esync puppet hid=%08x slot %d bank %d plays %s (hash %08x chg %d)\n", hid, s, (int)t->anim_bank[s], name, h, (int)t->anim_chg[s]);
    }
}

// ---- puppet: sensors ----
static void esx_sensor_off_one(uint64_t actor, Cpu *c, uint64_t ssvt, const char *name) {  // guest calls: deactivate + reset the layer
    Cpu save = *c;
    uint64_t sp = esx_scratch(c, ssvt, name);
    c->sp = sp, c->x[0] = actor, c->x[1] = sp;
    guest_call(c, SS->atk_off);
    c->sp = sp, c->x[0] = actor, c->x[1] = sp, c->x[2] = 0;
    guest_call(c, SS->atk_layer_off);
    *c = save;
}

void linkpos_esync_sensor(uint32_t hid, int on, uint32_t name, const uint32_t params[4]) {
    if (!g_esync_on || !hid) return;
    Cpu *c = g_in_frame ? kernel_current_cpu() : NULL;
    uint64_t ssvt = esx_ss_vt(), a = 0;
    int32_t life = 0;
    if (!c || !ssvt) { g_esx_st.p_nocpu++; return; }
    bool off;
    AcquireSRWLockExclusive(&g_esx_lock);
    off = esx_off_has(&g_esx_off, hid) != 0;
    ReleaseSRWLockExclusive(&g_esx_lock);
    if (!esx_find(hid, ENEMY_STALE, &a, &life) || !esx_actor_ok(a, hid, &life)) {  // (not loaded / replaced: nothing to switch, and nothing to leave on)
        AcquireSRWLockExclusive(&g_esx_lock);
        for (int k = 0; k < g_esx_pup.n; k++)
            if (g_esx_pup.e[k].hid == hid) g_esx_st.s_forgot += (uint64_t)g_esx_pup.e[k].nsens, g_esx_pup.e[k].nsens = 0;
        ReleaseSRWLockExclusive(&g_esx_lock);
        if (on) g_esx_st.p_unloaded++;
        return;
    }
    uint64_t a3 = 0;
    int32_t l3;
    bool fresh = esx_find(hid, 3, &a3, &l3) && a3 == a;  // (S168 SF-1: guest calls only on a body seen in the last 3 frames, like E1 / E2)
    AcquireSRWLockExclusive(&g_esx_lock);
    EsxPupEnt *p = esx_pup_get(&g_esx_pup, hid, a, on && off);
    if (p && !on && !fresh && p->nsens) p->off_pending = 1;  // (esync_frame ends them all once it is fresh again)
    ReleaseSRWLockExclusive(&g_esx_lock);
    if (!on) {
        if (!p) return;
        if (!fresh) { if (p->nsens) g_esx_st.s_pend++; return; }
        uint32_t list[ESX_PUP_SENS];
        int n = 0;
        AcquireSRWLockExclusive(&g_esx_lock);
        for (int i = 0; i < p->nsens; i++) if (!name || p->sens[i] == name) list[n++] = p->sens[i];
        for (int i = 0; i < n; i++) esx_pup_sens_del(p, list[i]);
        ReleaseSRWLockExclusive(&g_esx_lock);
        for (int i = 0; i < n; i++) {
            char buf[ESX_NAME_LEN];
            bool known = false;
            AcquireSRWLockExclusive(&g_esx_lock);
            const char *nm = esx_name_get(&g_esx_names, list[i]);
            if (nm) memcpy(buf, nm, sizeof buf), known = true;
            ReleaseSRWLockExclusive(&g_esx_lock);
            if (!known) { g_esx_st.s_unknown++; continue; }
            esx_sensor_off_one(a, c, ssvt, buf);
            g_esx_st.s_off++;
            ESX_LOG("[linkpos] esync puppet sensor OFF hid=%08x %s\n", hid, buf);
        }
        return;
    }
    if (!off || !p || !params || !name) { g_esx_st.p_notpup++; return; }  // (ON only for enemies whose brain we stopped)
    if (life <= 0) { g_esx_st.p_dead++; return; }
    if (!fresh) { g_esx_st.p_unloaded++; return; }
    char buf[ESX_NAME_LEN];
    bool known = false, learn, dup;
    AcquireSRWLockExclusive(&g_esx_lock);
    dup = esx_pup_has_sens(p, name) != 0;
    learn = !dup && !p->sensors_learned;  // (once per puppet body: names AND this actor's own sensor nodes, S168 SF-2)
    if (learn) p->sensors_learned = 1;
    ReleaseSRWLockExclusive(&g_esx_lock);
    if (dup) { g_esx_st.s_dup++; return; }  // (S168 SF-6: a second ON (the OFF was lost) does not run the activate calls again)
    if (learn) esx_learn_sensors(a, c, p);
    uint64_t node;
    AcquireSRWLockExclusive(&g_esx_lock);
    const char *nm = esx_name_get(&g_esx_names, name);
    if (nm) memcpy(buf, nm, sizeof buf), known = true;
    node = esx_pup_node_get(p, name);
    ReleaseSRWLockExclusive(&g_esx_lock);
    if (!known) { g_esx_st.s_unknown++; return; }
    // S168 SF-2: the puppet runs the same AI program, so its own node of that name holds the real params; the wire's 4 values are only the fallback
    uint32_t lp[4];
    const uint32_t *use = params;
    uint64_t np2;
    char nn[ESX_NAME_LEN];
    if (node && in_guest(node) && readable(node, 0x98) >= 0x98 && *(const uint64_t *)(uintptr_t)node == SS->atk_vptr &&
        *(const uint64_t *)(uintptr_t)(node + ACTION_ACTOR) == a && esx_rd64(node + ES_ATK_NAME, &np2) && esx_cstr(np2, nn, ESX_NAME_LEN) &&
        esx_hash(nn) == name && esx_atk_params(node, lp))
        use = lp, g_esx_st.s_local++;
    else g_esx_st.s_wire++;
    uint64_t rec = 0;
    {  // the AttackInfo record: atk_rec(actor)
        Cpu save = *c;
        c->sp = (c->sp - 0x40) & ~0xfull, c->x[0] = a;
        guest_call(c, SS->atk_rec);
        rec = c->x[0];
        *c = save;
    }
    if (!in_guest(rec) || readable(rec, 0x50) < 0x50) { g_esx_st.s_fail++; return; }
    uint32_t *r = (uint32_t *)(uintptr_t)rec;
    r[0x18 / 4] = use[0], r[0x1c / 4] = use[1], r[0x24 / 4] = use[2], r[0x28 / 4] = use[3];  // (enter_'s AtDamage etc.; the rest stays)
    Cpu save = *c;
    uint64_t sp = esx_scratch(c, ssvt, buf);
    c->sp = sp, c->x[0] = a, c->x[1] = sp, c->x[2] = 0;
    guest_call(c, SS->atk_on);
    c->sp = sp, c->x[0] = a, c->x[1] = sp, c->x[2] = 0;
    guest_call(c, SS->atk_layer_on);
    *c = save;
    AcquireSRWLockExclusive(&g_esx_lock);
    esx_pup_sens_add(p, name);
    ReleaseSRWLockExclusive(&g_esx_lock);
    g_esx_st.s_on++;
    ESX_LOG("[linkpos] esync puppet sensor ON hid=%08x %s p=%d %d %d %d (%s)\n", hid, buf, (int)use[0], (int)use[1], (int)use[2], (int)use[3], use == lp ? "own node" : "wire");
}

// ---- upkeep (linkpos_frame, inside the guest window, game thread) ----
static void esync_frame(void) {
    if (!g_esync_on) return;
    Cpu *c = kernel_current_cpu();
    uint64_t ssvt = esx_ss_vt();
    // S168 MF-2 cap: a wanted puppet whose own sensor stayed on > ESX_SON_CAP frames (its OFF got lost): switch it off here, so this resolve can freeze it
    if (c && ssvt) {
        uint32_t fh[8], fnm[8];
        int nf = 0;
        AcquireSRWLockExclusive(&g_esx_lock);
        for (int i = 0; i < g_esx_trk.n && nf < 8; i++) {
            const EsxTrkEnt *t = &g_esx_trk.e[i];
            if (esx_trk_may_freeze(t, g_vel_frame_id, ESX_SON_CAP) < 0 && esx_off_has(&g_esx_off, t->hid)) fh[nf] = t->hid, fnm[nf++] = t->son_name;
        }
        ReleaseSRWLockExclusive(&g_esx_lock);
        for (int i = 0; i < nf; i++) {
            uint64_t a = 0;
            int32_t life = 0;
            if (!esx_find(fh[i], 3, &a, &life) || !esx_actor_ok(a, fh[i], &life) || esx_hook_has(&g_esx_hook, a)) continue;  // (not fresh: next frame)
            char buf[ESX_NAME_LEN];
            bool known = false;
            AcquireSRWLockExclusive(&g_esx_lock);
            const char *nm = esx_name_get(&g_esx_names, fnm[i]);
            if (nm) memcpy(buf, nm, sizeof buf), known = true;
            EsxTrkEnt *t = esx_trk_find(&g_esx_trk, fh[i]);
            if (t) t->son = 0;
            ReleaseSRWLockExclusive(&g_esx_lock);
            if (known) esx_sensor_off_one(a, c, ssvt, buf);
            g_esx_st.brain_force++;
            ESX_LOG("[linkpos] esync hid=%08x own sensor %s still on after %d frames: switched off, freezing now\n", fh[i], known ? buf : "?", ESX_SON_CAP);
        }
    }
    esx_resolve();
    // puppets with a sensor still on: end them when the enemy died / is not a puppet any more; forget them when it unloaded
    struct { uint32_t hid; bool off, pend; } pl[ESX_PUP_MAX];
    int np = 0;
    AcquireSRWLockExclusive(&g_esx_lock);
    for (int i = 0; i < g_esx_pup.n; i++)
        if (g_esx_pup.e[i].nsens > 0)
            pl[np].hid = g_esx_pup.e[i].hid, pl[np].pend = g_esx_pup.e[i].off_pending != 0, pl[np++].off = esx_off_has(&g_esx_off, g_esx_pup.e[i].hid) != 0;
    ReleaseSRWLockExclusive(&g_esx_lock);
    for (int i = 0; i < np; i++) {
        uint64_t a = 0, a3 = 0;
        int32_t life = 1, l3;
        bool ok = esx_find(pl[i].hid, ENEMY_STALE, &a, &life) && esx_actor_ok(a, pl[i].hid, &life);
        if (ok && pl[i].off && life > 0 && !pl[i].pend) continue;
        if (ok && !(esx_find(pl[i].hid, 3, &a3, &l3) && a3 == a)) continue;  // (S168 SF-1: not seen in the last 3 frames: no guest call, try again next frame)
        if (ok && !(c && ssvt)) continue;
        if (ok) {
            uint32_t list[ESX_PUP_SENS];
            int n = 0;
            AcquireSRWLockExclusive(&g_esx_lock);
            for (int k = 0; k < g_esx_pup.n; k++)
                if (g_esx_pup.e[k].hid == pl[i].hid) {
                    for (int j = 0; j < g_esx_pup.e[k].nsens; j++) list[n++] = g_esx_pup.e[k].sens[j];
                    g_esx_pup.e[k].nsens = 0, g_esx_pup.e[k].off_pending = 0;
                }
            ReleaseSRWLockExclusive(&g_esx_lock);
            for (int k = 0; k < n; k++) {
                char buf[ESX_NAME_LEN];
                bool known = false;
                AcquireSRWLockExclusive(&g_esx_lock);
                const char *nm = esx_name_get(&g_esx_names, list[k]);
                if (nm) memcpy(buf, nm, sizeof buf), known = true;
                ReleaseSRWLockExclusive(&g_esx_lock);
                if (known) esx_sensor_off_one(a, c, ssvt, buf), g_esx_st.s_dead++;
            }
            ESX_LOG("[linkpos] esync ended %d puppet sensors of hid=%08x (%s)\n", n, pl[i].hid, life <= 0 ? "dead" : pl[i].pend ? "OFF came while not drawn" : "no longer a puppet");
        } else {  // unloaded: its bodies went with it
            AcquireSRWLockExclusive(&g_esx_lock);
            for (int k = 0; k < g_esx_pup.n; k++)
                if (g_esx_pup.e[k].hid == pl[i].hid) g_esx_st.s_forgot += (uint64_t)g_esx_pup.e[k].nsens, g_esx_pup.e[k].nsens = 0, g_esx_pup.e[k].off_pending = 0;
            ReleaseSRWLockExclusive(&g_esx_lock);
        }
    }
    // s174: local attacks end when the feature is off / the enemy is no longer a frozen puppet / died / got no puppet update; forgotten when unloaded
    {
        struct { uint32_t hid; uint64_t actor; bool off, stale; } al[ESX_PUP_MAX];
        int na = 0;
        AcquireSRWLockExclusive(&g_esx_lock);
        for (int i = 0; i < g_esx_pup.n; i++)
            if (g_esx_pup.e[i].atk_node)
                al[na].hid = g_esx_pup.e[i].hid, al[na].actor = g_esx_pup.e[i].actor, al[na].off = esx_off_has(&g_esx_off, g_esx_pup.e[i].hid) != 0,
                al[na++].stale = g_vel_frame_id - g_esx_pup.e[i].atk_seen > ESX_ATK_STALE;
        ReleaseSRWLockExclusive(&g_esx_lock);
        for (int i = 0; i < na; i++) {
            uint64_t a = 0, a3 = 0;
            int32_t life = 0, l3;
            bool found = esx_find(al[i].hid, ENEMY_STALE, &a, &life) && a == al[i].actor && esx_actor_ok(a, al[i].hid, &life);
            bool fresh = found && esx_find(al[i].hid, 3, &a3, &l3) && a3 == a;
            EsxPupEnt *pe = NULL;
            AcquireSRWLockExclusive(&g_esx_lock);
            for (int k = 0; k < g_esx_pup.n; k++) if (g_esx_pup.e[k].hid == al[i].hid && g_esx_pup.e[k].actor == al[i].actor) pe = &g_esx_pup.e[k];
            if (pe && !found) pe->atk_node = 0, g_esx_as.forgot++;  // (unloaded / replaced: its nodes went with it)
            ReleaseSRWLockExclusive(&g_esx_lock);
            if (!pe || !found || !fresh || !c) continue;  // (not drawn this frame: try again next frame)
            const char *why = !g_esx_atk_on ? "feature off" : !al[i].off ? "not a puppet any more" : !esx_hook_has(&g_esx_hook, a) ? "brain back on" :
                              life <= 0 ? "dead" : al[i].stale ? "no puppet update" : NULL;
            if (!why) continue;
            if (al[i].stale) g_esx_as.stale++;
            esx_atk_end(c, pe, pe->chg[0], why);
        }
    }
    // no puppet entry outlives its brain-off state
    AcquireSRWLockExclusive(&g_esx_lock);
    for (int i = 0; i < g_esx_pup.n;)
        if (!esx_off_has(&g_esx_off, g_esx_pup.e[i].hid) && !g_esx_pup.e[i].nsens && !g_esx_pup.e[i].atk_node) g_esx_pup.e[i] = g_esx_pup.e[--g_esx_pup.n];
        else i++;
    ReleaseSRWLockExclusive(&g_esx_lock);
    // keep the name table fed from our own enemies' running AS (every 10 frames)
    if (g_vel_frame_id % 10 == 0) {
        uint64_t as[16];
        int na = 0;
        AcquireSRWLockExclusive(&g_enemy_lock);
        for (int i = 0; i < g_nenemy && na < 16; i++)
            if (g_enemy[i].hid && g_vel_frame_id - g_enemy[i].seen <= 1 && g_enemy[i].life > 0) as[na++] = g_enemy[i].actor;
        ReleaseSRWLockExclusive(&g_enemy_lock);
        for (int i = 0; i < na; i++) {
            char nm[ESX_NAME_LEN];
            uint8_t bank;
            uint64_t obj;
            int fin;
            if (!in_guest(as[i]) || readable(as[i], 0x850) < 0x850) continue;
            for (int s = 0; s < ES_SLOTS; s++)
                if (esx_as_read(as[i], s, &bank, &obj, &fin, nm) == 1) {
                    AcquireSRWLockExclusive(&g_esx_lock);
                    if (esx_name_put(&g_esx_names, nm) == 1) g_esx_st.names_cur++;
                    ReleaseSRWLockExclusive(&g_esx_lock);
                }
        }
    }
    if (g_esync_log && g_vel_frame_id % 600 == 0) {
        LOG("[linkpos] esync (frame %llu): off %d, hook %d; brain skipped %llu; reads %llu/%llu fail; puppet %llu (mtx %llu snap %llu; not-puppet %llu unloaded %llu bad %llu dead %llu vt %llu nocpu %llu); "
            "anim start %llu same %llu unknown %llu (defs learned %llu, fail %llu, cur %llu); sensor ev on %llu off %llu (no-list %llu no-hid %llu no-name %llu), puppet on %llu off %llu "
            "unknown %llu forgot %llu dead %llu learn %llu fail %llu; names %d, ev queued %d dropped %llu; "
            "freeze delayed %llu forced %llu, unloaded-forgot %llu, params own %llu wire %llu, sensor dup %llu pending %llu, action cached %llu\n",
            (unsigned long long)g_vel_frame_id, g_esx_off.n, g_esx_hook.live, (unsigned long long)g_esx_st.brain_skip, (unsigned long long)g_esx_st.reads,
            (unsigned long long)g_esx_st.read_fail, (unsigned long long)g_esx_st.puppets, (unsigned long long)g_esx_st.p_mtx, (unsigned long long)g_esx_st.p_snap,
            (unsigned long long)g_esx_st.p_notpup, (unsigned long long)g_esx_st.p_unloaded, (unsigned long long)g_esx_st.p_bad, (unsigned long long)g_esx_st.p_dead,
            (unsigned long long)g_esx_st.p_vt, (unsigned long long)g_esx_st.p_nocpu, (unsigned long long)g_esx_st.a_start, (unsigned long long)g_esx_st.a_same,
            (unsigned long long)g_esx_st.a_unknown, (unsigned long long)g_esx_st.a_defs, (unsigned long long)g_esx_st.a_defs_fail, (unsigned long long)g_esx_st.names_cur,
            (unsigned long long)g_esx_st.ev_on, (unsigned long long)g_esx_st.ev_off, (unsigned long long)g_esx_st.ev_nolist, (unsigned long long)g_esx_st.ev_nohid,
            (unsigned long long)g_esx_st.ev_noname, (unsigned long long)g_esx_st.s_on, (unsigned long long)g_esx_st.s_off, (unsigned long long)g_esx_st.s_unknown,
            (unsigned long long)g_esx_st.s_forgot, (unsigned long long)g_esx_st.s_dead, (unsigned long long)g_esx_st.s_learn, (unsigned long long)g_esx_st.s_fail,
            g_esx_names.n, g_esx_evq.n, (unsigned long long)g_esx_evq.dropped, (unsigned long long)g_esx_st.brain_delay, (unsigned long long)g_esx_st.brain_force,
            (unsigned long long)g_esx_st.off_drop, (unsigned long long)g_esx_st.s_local, (unsigned long long)g_esx_st.s_wire, (unsigned long long)g_esx_st.s_dup,
            (unsigned long long)g_esx_st.s_pend, (unsigned long long)g_esx_st.act_cached);
        esx_atk_log();
    }
}

// (s174) the attack counters: their own line next to the big one
static void esx_atk_log(void) {
    LOG("[linkpos] esync attack: %s, %d classes; enter %llu leave %llu (finished %llu, cap %llu, swap %llu, stale %llu, forgot %llu), no-node %llu, "
        "walks %llu (%llu nodes), owner AS skipped %llu, bad fn %llu, dll ai tap %llu\n", g_esx_atk_on ? "on" : "off", g_esx_atk_nvt,
        (unsigned long long)g_esx_as.enter, (unsigned long long)g_esx_as.leave, (unsigned long long)g_esx_as.fin, (unsigned long long)g_esx_as.cap,
        (unsigned long long)g_esx_as.swap, (unsigned long long)g_esx_as.stale, (unsigned long long)g_esx_as.forgot, (unsigned long long)g_esx_as.nonode,
        (unsigned long long)g_esx_as.walk, (unsigned long long)g_esx_as.walk_nodes, (unsigned long long)g_esx_as.skip_as, (unsigned long long)g_esx_as.bad_fn,
        (unsigned long long)g_esx_st.brain_tap);
}
// counters for tests / the DLL's diag (out[0..n)): the order below; -1 = esync off
int linkpos_esync_stats(uint64_t *out, int n) {
    const uint64_t v[] = {g_esx_st.enum_calls, g_esx_st.reads, g_esx_st.read_fail, g_esx_st.puppets, g_esx_st.p_mtx, g_esx_st.a_start, g_esx_st.a_unknown,
                          g_esx_st.brain_skip, g_esx_st.ev_on, g_esx_st.ev_off, g_esx_st.s_on, g_esx_st.s_off, g_esx_st.s_unknown, (uint64_t)g_esx_names.n,
                          (uint64_t)g_esx_hook.live, (uint64_t)g_esx_off.n};
    int k = (int)(sizeof v / sizeof *v);
    if (out) for (int i = 0; i < n && i < k; i++) out[i] = v[i];
    return g_esync_on ? k : -1;
}

static void esync_init(void) {  // linkpos_init, after dmg_init (g_esync_on is read by enemy_note, so before any guest code runs)
    AcquireSRWLockExclusive(&g_esx_lock);
    memset(&g_esx_names, 0, sizeof g_esx_names), memset(&g_esx_off, 0, sizeof g_esx_off), memset(&g_esx_evq, 0, sizeof g_esx_evq);
    memset(&g_esx_trk, 0, sizeof g_esx_trk), memset(&g_esx_pup, 0, sizeof g_esx_pup);
    for (int i = 0; i < ESX_HOOK_MAX; i++) g_esx_hook.a[i] = 0;
    g_esx_hook.live = 0;
    ReleaseSRWLockExclusive(&g_esx_lock);
    memset(&g_esx_st, 0, sizeof g_esx_st);
    g_esync_on = env_int("SAVAGEWIND_LINK_ESYNC", 0) != 0, g_esync_log = env_int("SAVAGEWIND_LINK_ESYNC_LOG", 0) != 0;
    const char *bl = getenv("SAVAGEWIND_LINK_ESYNC_BLEND");
    g_esx_blend = bl && *bl ? (float)atof(bl) : 0.0f;
    if (!g_esync_on) return;
    const struct { const char *n; uint64_t a; } need[] = {
        {"rootai_calc", SS->rootai_calc}, {"rootai_vptr", SS->rootai_vptr}, {"actor_set_mtx", SS->actor_set_mtx}, {"as_start", SS->as_start},
        {"atk_enter", SS->atk_enter}, {"atk_calc", SS->atk_calc}, {"atk_leave", SS->atk_leave}, {"atk_rec", SS->atk_rec}, {"atk_on", SS->atk_on},
        {"atk_off", SS->atk_off}, {"atk_layer_on", SS->atk_layer_on}, {"atk_layer_off", SS->atk_layer_off}, {"atk_vptr", SS->atk_vptr},
        {"aslist_none_got", SS->aslist_none_got}, {"safestring_vt_got", SS->safestring_vt_got}};
    for (size_t i = 0; i < sizeof need / sizeof *need; i++)
        if (!need[i].a) { LOG("[linkpos] esync: no site '%s' for game version %s: enemy sync OFF\n", need[i].n, g_game_version); g_esync_on = 0; return; }
    const struct { const char *n; uint64_t a; } fns[] = {{"rootai_calc", SS->rootai_calc}, {"actor_set_mtx", SS->actor_set_mtx}, {"as_start", SS->as_start},
        {"atk_enter", SS->atk_enter}, {"atk_calc", SS->atk_calc}, {"atk_leave", SS->atk_leave}, {"atk_rec", SS->atk_rec}, {"atk_on", SS->atk_on},
        {"atk_off", SS->atk_off}, {"atk_layer_on", SS->atk_layer_on}, {"atk_layer_off", SS->atk_layer_off}};
    for (size_t i = 0; i < sizeof fns / sizeof *fns; i++)
        if (!dispatch_lookup(fns[i].a)) { LOG("[linkpos] esync: no function at %llx ('%s'): enemy sync OFF\n", (unsigned long long)fns[i].a, fns[i].n); g_esync_on = 0; return; }
    g_esx_rootai = dispatch_lookup(SS->rootai_calc), g_esx_enter = dispatch_lookup(SS->atk_enter);
    g_esx_calc = dispatch_lookup(SS->atk_calc), g_esx_leave = dispatch_lookup(SS->atk_leave);
    memset(&g_esx_as, 0, sizeof g_esx_as);
    esx_atk_defaults();
    g_esx_atk_on = env_int("SAVAGEWIND_LINK_ESYNC_ATK", 0) != 0 && g_esx_atk_nvt > 0;
    hle_override(SS->rootai_calc, esx_rootai_hook, "linkpos esync RootAi::calc");
    hle_override(SS->atk_enter, esx_enter_hook, "linkpos esync ActivateAttackSensor::enter_");
    hle_override(SS->atk_calc, esx_calc_hook, "linkpos esync ActivateAttackSensor::calc_");
    hle_override(SS->atk_leave, esx_leave_hook, "linkpos esync ActivateAttackSensor::leave_");
    LOG("[linkpos] esync on (RootAi::calc %llx, setMtx %llx, startAnimation %llx, sensor enter_/calc_/leave_ %llx/%llx/%llx)%s, blend %.2f\n",
        (unsigned long long)SS->rootai_calc, (unsigned long long)SS->actor_set_mtx, (unsigned long long)SS->as_start, (unsigned long long)SS->atk_enter,
        (unsigned long long)SS->atk_calc, (unsigned long long)SS->atk_leave, g_esync_log ? ", log on" : "", g_esx_blend);
    LOG("[linkpos] esync attack actions %s (%d classes; SAVAGEWIND_LINK_ESYNC_ATK, live set esyncatk)\n", g_esx_atk_on ? "on" : "off", g_esx_atk_nvt);
}

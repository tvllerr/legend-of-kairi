// sora_bbomb.h (s217 world, feature 1): a Keyblade hit, Fire or Thunder sets off BotW's explosive barrels (BarrelBomb, BarrelBomb2 / 3,
// BarrelBomb_Balloon: all four use the AI class BarrelBomb). Study: recon/s217/S217_HELPER_WORLD.md section 1; report:
// recon/s217/S217_HELPER_WORLD_BUILD.md. DLL only (sw_api, exe s175+), hot reloads, no exe change.
//
// Why: the barrel is not "Breakable" (its DamageParam says false), so it is in none of the exe's lists and the Keyblade passed
// through it. BotW's own ai::BarrelBomb::calc_ (1.9.0 0x71007eb98c, decompile sb/s217/world/dec_bb.c) asks the barrel's chemical
// to explode as soon as the actor's life (+0x848) is below 1. So: learn where the barrels are from that calc (a sw_api vtable
// hook, as sora_esa_probe.h does), and on a hit write the life down to 0 from the mod's frame. BotW then explodes it itself
// (its effect, sound and blast: the blast hurts enemies AND Sora, Lv2 / Lv3 barrels hit hard: GParam power 40 / 80).
//
// The hook (BotW 1.9.0 only): vtable 0x7001c47500 (runtime address) slot 32 must hold 0x70007eb98c, else it is refused (one log
// line, the feature stays idle). It runs on AI worker threads: it only READS (api->read: never faults) and RECORDS a row
// {actor, node, the actor's vtable, frame id, position, life, generation} into a 32-row table under one SRWLOCK, then runs the
// original. No guest call, no LOG, no write there. Every write happens on the mod's frame (blade_hits_one / spell_tick).
// Validity before a write (bb_check, host-tested): the row has this hook generation (bumped at every hook on / off: rows from an
// older install, or recorded by a worker that was mid-call during an unhook, never count), it was recorded in the last BB_FRESH
// linkpos frames (a barrel that exploded / unloaded / a reload stops calling calc, so its row goes stale at once), and a fresh
// read NOW still shows the node's vtable == the BarrelBomb AI vtable, the node's actor == the row's actor, the actor's vtable ==
// the one recorded, its BaseProc state (+0x64) != 3 (being deleted), its name starting with "BarrelBomb" and its life >= 1.
// A hot reload = a new DLL copy: new empty table, the exe unhooked the old copy's slot (sw_api_dll_switch), we hook again.
// A load (g_loading) empties the table.
// Rules (user s217): ONE Keyblade hit (any live hit window with power, swings and finishers alike) = life 0 = it explodes; Fire
// (every tick of the ring) and Thunder (the bolt's burst) too. Blizzard / Magnet / Reflect / the beam do NOT (BotW: Arrow rate 0
// for ice; the others were not asked for). A Stasis'd barrel is left to the Stasis path (world_blade stores the hit there).
// Live: barrelboom 0/1 (DEFAULT = BB_DEFAULT_ON = 1 since s218: the user verified Keyblade, Fire and Thunder; 0 = the hook is not even installed),
// barrelboomlog 0/1 ([barrel] lines: barrels seen, a 2 s summary, each hit / skip, the barrel's disappearance after a hit).
// Multiplayer: no packets. The explosion happens on this game only; another player's game keeps its barrel.
//
// The pure part (above BBOMB_HOST's #ifndef) is host-tested: runtime/test/bbomb_test.c.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define BB_DEFAULT_ON 1            // (s218: verified by the user on the 0.5.3 trial -> default on; set barrelboom 0 = off)
#define BB_VT 0x7001c47500ull      // ai::BarrelBomb vtable (Ghidra 0x7101c47500, sb/s217/world/facvt190.txt)
#define BB_SLOT 32                 // calc_
#define BB_CALC 0x70007eb98cull    // what slot 32 holds on 1.9.0 (sb/s217/world/vt_barrelbomb.txt)
#define BB_N 32                    // table rows
#define BB_FRESH 3                 // a row counts if its calc ran within this many linkpos frames
#define BB_REUSE 120               // a row not seen for this many frames may be taken by another barrel
#define BB_GONE 60                 // a hit barrel with no calc for this many frames = exploded / deleted (log line)
#define BB_R 0.5f                  // the barrel's body: cylinder radius / height, u (BarrelBomb.bphysics TgtBody, study 1.5)
#define BB_H 1.0f

// ------------------------------------------------------------------ pure logic (host-tested)
typedef struct {
    uint64_t actor, node, vt;  // the barrel actor, its AI node, the actor's vtable when recorded
    uint64_t frame;            // linkpos frame id of the last calc
    uint32_t gen;              // hook generation it was recorded under
    float pos[3];              // actor matrix translation (BotW u)
    int32_t life;              // +0x848 when recorded
    uint8_t used, logged, hit; // row in use / "seen" line printed / we wrote its life (watch for its end)
} BbRow;

// The row a calc of `actor` at frame `now` goes into: its own row, else a free one, else the stalest row not seen for BB_REUSE
// frames; -1 = full (the calc is not recorded).
static int bb_row_pick(const BbRow *t, int n, uint64_t actor, uint64_t now) {
    int free_i = -1, old_i = -1;
    uint64_t old_f = ~0ull;
    for (int i = 0; i < n; i++) {
        if (t[i].used && t[i].actor == actor) return i;
        if (!t[i].used) { if (free_i < 0) free_i = i; continue; }
        if (t[i].frame <= now && now - t[i].frame >= BB_REUSE && t[i].frame < old_f) old_f = t[i].frame, old_i = i;
    }
    return free_i >= 0 ? free_i : old_i;
}

enum { BB_OK, BB_UNUSED, BB_GEN, BB_STALE, BB_UNREAD, BB_NODE, BB_VTABLE, BB_DELETING, BB_NAME, BB_SPENT };
static const char *const BB_WHY[] = {"ok", "unused row", "older hook generation", "stale (no calc lately)", "unreadable now",
                                     "AI node changed", "actor vtable changed", "being deleted", "not a BarrelBomb", "life already < 1"};
typedef struct {     // what the mod's frame reads NOW (the glue fills it; read_ok = every read worked)
    uint64_t now_frame;
    uint32_t gen;    // the current hook generation
    bool read_ok;
    uint64_t node_vt, node_actor, actor_vt;
    uint8_t state;   // BaseProc state +0x64 (3 = being deleted)
    const char *name;
    int32_t life;
} BbLive;
static bool bb_name_ok(const char *n) { return n && !strncmp(n, "BarrelBomb", 10); }
static int bb_check(const BbRow *r, const BbLive *l) {
    if (!r->used) return BB_UNUSED;
    if (r->gen != l->gen) return BB_GEN;
    if (r->frame > l->now_frame || l->now_frame - r->frame > BB_FRESH) return BB_STALE;  // (a frame id from before a reboot = stale)
    if (!l->read_ok) return BB_UNREAD;
    if (l->node_vt != BB_VT || l->node_actor != r->actor) return BB_NODE;
    if (l->actor_vt != r->vt) return BB_VTABLE;
    if (l->state == 3) return BB_DELETING;
    if (!bb_name_ok(l->name)) return BB_NAME;
    if (l->life < 1) return BB_SPENT;
    return BB_OK;
}
// The life to write for a hit (user s217: one hit sets it off): 0, or -1 = nothing to do.
static int32_t bb_hit_life(int32_t life) { return life >= 1 ? 0 : -1; }
// A spell's area (as brk_spell): a cylinder of radius r at c, from c.y - below to c.y + above, vs the barrel's feet.
static bool bb_in_cyl(const float pos[3], const float c[3], float r, float below, float above) {
    float dx = pos[0] - c[0], dz = pos[2] - c[2], dy = pos[1] - c[1];
    return dx * dx + dz * dz < r * r && dy > -below && dy < above;
}

#ifndef BBOMB_HOST
// ------------------------------------------------------------------ game glue (sora.c, after brk_spell)
// Needs: SwApi (sw_api.h), S, HitWin, ATKP_KIND_NORMAL, sweep_hits_rh, st_frozen, kbfx_name, kbse_hit, sora_fx_event, LOG.
#include "sw_api.h"
enum { BBHIT_SWING, BBHIT_FINISH, BBHIT_FIRE, BBHIT_THUNDER };
static const char *const BB_HOW[] = {"swing", "finisher", "fire", "thunder"};
static int g_bb_on = BB_DEFAULT_ON;  // live barrelboom
static int g_bb_log = 0;             // live barrelboomlog
static struct {
    const SwApi *api;
    int id, hooked, refused;  // hook id (-1 none); refused = the check failed once (retried after barrelboom 0 -> 1)
    uint32_t gen;             // bumped at every hook on / off (written on the frame, read by the hook)
    SRWLOCK lock;             // the table (hook threads + the frame)
    BbRow row[BB_N];
    long calls, full;         // calc calls seen / not recorded because the table was full (atomics)
    long calls_seen;          // (the summary's last value)
    uint64_t on_at, next_sum; // linkpos frame of the hook install / the next 2 s summary
    int zero_said;
} BB = {.id = -1};

static bool bb_bind(void) {
    if (!BB.api) {
        const SwApi *(*get)(uint32_t) = (const SwApi *(*)(uint32_t))(void *)GetProcAddress(GetModuleHandleA(NULL), SW_API_EXPORT);
        BB.api = get ? get(1) : NULL;
    }
    const SwApi *a = BB.api;
    return SW_HAS(a, read) && SW_HAS(a, write) && SW_HAS(a, frame_id) && SW_HAS(a, actor_name) && SW_HAS(a, vt_hook) &&
           SW_HAS(a, vt_unhook) && SW_HAS(a, hook_orig) && SW_HAS(a, in_frame);
}
// AI worker threads: read + record only (no guest call, no LOG, no write), then BotW's own calc
static void bb_calc(Cpu *c, int id, void *u) {
    (void)u;
    const SwApi *a = BB.api;
    uint64_t node = c->x[0], actor = 0, vt = 0;
    float m[12];
    int32_t life = 0;
    __atomic_add_fetch(&BB.calls, 1, __ATOMIC_RELAXED);
    if (a->read(node + 8, &actor, 8) && actor && a->read(actor, &vt, 8) && a->read(actor + 0x398, m, sizeof m) &&
        a->read(actor + 0x848, &life, 4)) {
        uint64_t f = a->frame_id();
        uint32_t gen = __atomic_load_n(&BB.gen, __ATOMIC_ACQUIRE);
        AcquireSRWLockExclusive(&BB.lock);
        int i = bb_row_pick(BB.row, BB_N, actor, f);
        if (i >= 0) {
            BbRow *r = &BB.row[i];
            if (!r->used || r->actor != actor || r->gen != gen) memset(r, 0, sizeof *r), r->actor = actor;
            r->node = node, r->vt = vt, r->frame = f, r->gen = gen, r->life = life, r->used = 1;
            r->pos[0] = m[3], r->pos[1] = m[7], r->pos[2] = m[11];
        } else __atomic_add_fetch(&BB.full, 1, __ATOMIC_RELAXED);
        ReleaseSRWLockExclusive(&BB.lock);
    }
    a->hook_orig(c, id);
}
static void bb_clear(void) {
    AcquireSRWLockExclusive(&BB.lock);
    memset(BB.row, 0, sizeof BB.row);
    ReleaseSRWLockExclusive(&BB.lock);
}
static void bb_hook(void) {
    if (BB.hooked || BB.refused) return;
    if (!bb_bind()) { BB.refused = 1; LOG("[barrel] refused: this exe has no sw_api hooks (needs the s175+ exe): barrelboom idle\n"); return; }
    const SwApi *a = BB.api;
    uint64_t s = 0;
    if (!a->read(BB_VT + BB_SLOT * 8, &s, 8) || s != BB_CALC) {
        BB.refused = 1;
        LOG("[barrel] refused: BarrelBomb AI vtable %llx slot %d holds %llx, not %llx (other game version / already hooked): barrelboom idle\n",
            (unsigned long long)BB_VT, BB_SLOT, (unsigned long long)s, (unsigned long long)BB_CALC);
        return;
    }
    __atomic_add_fetch(&BB.gen, 1, __ATOMIC_RELEASE);
    bb_clear();
    BB.id = a->vt_hook(BB_VT, BB_SLOT, bb_calc, NULL, "barrelboom calc");
    if (BB.id < 0) { BB.refused = 1; LOG("[barrel] refused: vt_hook said %d (all sw_api hook slots used / bad slot): barrelboom idle\n", BB.id); return; }
    uint64_t now_s = 0;
    a->read(BB_VT + BB_SLOT * 8, &now_s, 8);
    BB.hooked = 1, BB.on_at = a->frame_id(), BB.next_sum = BB.on_at + 120, BB.zero_said = 0;
    BB.calls = BB.full = BB.calls_seen = 0;
    LOG("[barrel] hook on: BarrelBomb AI calc (vtable %llx slot %d, was %llx, now %llx), hook id %d, generation %u (log %d)\n",
        (unsigned long long)BB_VT, BB_SLOT, (unsigned long long)s, (unsigned long long)now_s, BB.id, BB.gen, g_bb_log);
}
static void bb_unhook(const char *why) {
    if (!BB.hooked) return;
    BB.api->vt_unhook(BB.id);
    BB.id = -1, BB.hooked = 0;
    __atomic_add_fetch(&BB.gen, 1, __ATOMIC_RELEASE);  // (a worker still inside bb_calc records under the old generation: ignored)
    bb_clear();
    LOG("[barrel] hook off (%s)\n", why);
}
static float bb_dist(const float a[3], const float b[3]) { return sqrtf((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2])); }
static int bb_snap(BbRow *out) {  // (the frame: a copy of the used rows)
    int n = 0;
    AcquireSRWLockExclusive(&BB.lock);
    for (int i = 0; i < BB_N; i++) if (BB.row[i].used) out[n++] = BB.row[i];
    ReleaseSRWLockExclusive(&BB.lock);
    return n;
}
static void bb_mark_hit(uint64_t actor) {
    AcquireSRWLockExclusive(&BB.lock);
    for (int i = 0; i < BB_N; i++) if (BB.row[i].used && BB.row[i].actor == actor) BB.row[i].hit = 1;
    ReleaseSRWLockExclusive(&BB.lock);
}
// one barrel hit (the frame): the validity check, then life -> 0. Returns the life after (0) or -1 = no write.
static int bb_strike(const BbRow *r, int how, const float from[3], bool quiet_spent) {
    const SwApi *a = BB.api;
    BbLive l = {.now_frame = a->frame_id(), .gen = __atomic_load_n(&BB.gen, __ATOMIC_ACQUIRE), .name = ""};
    l.read_ok = a->in_frame() && a->read(r->node, &l.node_vt, 8) && a->read(r->node + 8, &l.node_actor, 8) && a->read(r->actor, &l.actor_vt, 8) &&
                a->read(r->actor + 0x64, &l.state, 1) && a->read(r->actor + 0x848, &l.life, 4);
    if (l.read_ok) l.name = a->actor_name(r->actor);
    int why = bb_check(r, &l);
    float d = bb_dist(r->pos, from);
    if (why != BB_OK) {
        if (g_bb_log && !(quiet_spent && why == BB_SPENT))
            LOG("[barrel] skip %llx %s by %s at %.2f u: %s (seen %lld frames ago, life %d)\n", (unsigned long long)r->actor, l.name, BB_HOW[how], d,
                BB_WHY[why], (long long)(l.now_frame - r->frame), l.life);
        return -1;
    }
    int32_t want = bb_hit_life(l.life), after = -99;
    if (!a->write(r->actor + 0x848, &want, 4)) {
        LOG("[barrel] HIT %llx %s by %s at %.2f u: life write REFUSED (not a read-write page)\n", (unsigned long long)r->actor, l.name, BB_HOW[how], d);
        return -1;
    }
    a->read(r->actor + 0x848, &after, 4);
    bb_mark_hit(r->actor);
    if (g_bb_log)
        LOG("[barrel] HIT %llx %s by %s at %.2f u from Sora: life %d -> %d (BotW's calc explodes it), at %.2f %.2f %.2f\n", (unsigned long long)r->actor,
            l.name, BB_HOW[how], d, l.life, after, r->pos[0], r->pos[1], r->pos[2]);
    return after;
}
// blade_hits_one, next to brk_blade: the Keyblade sweep vs the recorded barrels (one try per barrel per window)
static int bb_blade(const HitWin *win, const float prev[6], const float now[6], const float feet[3], float to_u) {
    if (!BB.hooked) return 0;
    BbRow t[BB_N];
    int n = bb_snap(t), hits = 0;
    bool fin = win->kind != ATKP_KIND_NORMAL;
    for (int i = 0; i < n; i++) {
        int k = 0;
        while (k < S.nhit && S.hit_actor[k] != t[i].actor) k++;
        if (k < S.nhit || !sweep_hits_rh(prev, now, t[i].pos, BB_R, BB_H)) continue;
        if (st_frozen(t[i].actor)) {  // (Stasis'd: world_blade stores the hit, BotW launches it; NOT recorded in S.hit_actor, or
                                      //  world_blade's world_seen would skip it)
            static uint64_t said;
            if (g_bb_log && said != t[i].actor) said = t[i].actor, LOG("[barrel] skip %llx: Stasis'd (the Stasis path takes the hit)\n", (unsigned long long)t[i].actor);
            continue;
        }
        if (S.nhit < 16) S.hit_actor[S.nhit++] = t[i].actor;
        if (bb_strike(&t[i], fin ? BBHIT_FINISH : BBHIT_SWING, feet, false) < 0) continue;
        sora_fx_event(kbfx_name(fin ? "hit_finish" : "hit"), now + 3, S.face, to_u);
        kbse_hit(win->atkp, 1.0f, t[i].pos);
        S.hit = true, hits++;  // (as a box hit: counts for the combo)
    }
    return hits;
}
// spell_tick, next to brk_spell: Fire (each tick) / Thunder (the burst). Blizzard is never passed here.
static int bb_spell(int how, const float c[3], float r, float below, float above, const float from[3]) {
    if (!BB.hooked || (how != BBHIT_FIRE && how != BBHIT_THUNDER)) return 0;
    BbRow t[BB_N];
    int n = bb_snap(t), hits = 0;
    for (int i = 0; i < n; i++)
        if (bb_in_cyl(t[i].pos, c, r, below, above) && !st_frozen(t[i].actor) && bb_strike(&t[i], how, from, true) >= 0) hits++;
    return hits;
}
// every world frame (sora.c, next to sh_tick): hook on / off by barrelboom, the load reset, the log
static void bb_tick(bool loading, const float feet[3]) {
    if (!g_bb_on) { bb_unhook("barrelboom 0"); return; }  // (default: nothing is bound or hooked)
    if (!BB.hooked) bb_hook();
    if (!BB.hooked) return;
    if (loading) { bb_clear(); return; }
    uint64_t f = BB.api->frame_id();
    long calls = __atomic_load_n(&BB.calls, __ATOMIC_RELAXED);
    if (!calls && !BB.zero_said && f > BB.on_at + 300) {
        BB.zero_said = 1;
        LOG("[barrel] no BarrelBomb calc in %llu frames since the hook (none loaded near here, or the calc is not called through the vtable: GUESS)\n",
            (unsigned long long)(f - BB.on_at));
    }
    BbRow gone[BB_N];
    int ng = 0;
    AcquireSRWLockExclusive(&BB.lock);
    for (int i = 0; i < BB_N; i++) {
        BbRow *r = &BB.row[i];
        if (!r->used) continue;
        if (r->hit && r->frame <= f && f - r->frame >= BB_GONE) gone[ng++] = *r, memset(r, 0, sizeof *r);
    }
    ReleaseSRWLockExclusive(&BB.lock);
    for (int i = 0; g_bb_log && i < ng; i++)
        LOG("[barrel] %llx: no AI calc for %d frames since our hit (exploded and deleted, or unloaded)\n", (unsigned long long)gone[i].actor, BB_GONE);
    if (!g_bb_log) return;
    BbRow t[BB_N];
    int n = bb_snap(t);
    for (int i = 0; i < n; i++) {
        if (t[i].logged || t[i].gen != BB.gen) continue;
        LOG("[barrel] seen %llx %s life %d at %.2f %.2f %.2f (%.1f u from Sora), node %llx\n", (unsigned long long)t[i].actor, BB.api->actor_name(t[i].actor),
            t[i].life, t[i].pos[0], t[i].pos[1], t[i].pos[2], bb_dist(t[i].pos, feet), (unsigned long long)t[i].node);
        AcquireSRWLockExclusive(&BB.lock);
        for (int j = 0; j < BB_N; j++) if (BB.row[j].used && BB.row[j].actor == t[i].actor) BB.row[j].logged = 1;
        ReleaseSRWLockExclusive(&BB.lock);
    }
    if (f < BB.next_sum) return;
    BB.next_sum = f + 120;
    if (calls == BB.calls_seen) return;
    int fresh = 0, best = -1;
    float bd = 1e9f;
    for (int i = 0; i < n; i++) {
        if (t[i].gen != BB.gen || t[i].frame > f || f - t[i].frame > BB_FRESH) continue;
        fresh++;
        float d = bb_dist(t[i].pos, feet);
        if (d < bd) bd = d, best = i;
    }
    LOG("[barrel] calc calls %ld (+%ld), rows %d, fresh %d, nearest %s life %d at %.1f u, table full %ld\n", calls, calls - BB.calls_seen, n, fresh,
        best >= 0 ? BB.api->actor_name(t[best].actor) : "-", best >= 0 ? t[best].life : 0, best >= 0 ? bd : -1.0f, __atomic_load_n(&BB.full, __ATOMIC_RELAXED));
    BB.calls_seen = calls;
}
static bool bb_live_set(const char *key, int v) {
    if (!strcmp(key, "barrelboom")) {
        g_bb_on = v != 0, BB.refused = 0;  // (a new "1" retries a refused hook once)
        LOG("[barrel] barrelboom %d: %s\n", g_bb_on, g_bb_on ? "a Keyblade hit / Fire / Thunder sets off explosive barrels" : "off (hook removed)");
    } else if (!strcmp(key, "barrelboomlog")) g_bb_log = v != 0, LOG("[barrel] log %s\n", g_bb_log ? "on" : "off");
    else return false;
    return true;
}
#endif  // BBOMB_HOST

// sora_carry.h (s217 world, feature 2): carrying a barrel / rock / pot works like the Remote Bomb. Study:
// recon/s217/S217_HELPER_WORLD.md section 2; report: recon/s217/S217_HELPER_WORLD_BUILD.md. DLL only, hot reloads.
//
// BotW has ONE carry machine for the Remote Bomb, barrels, explosive barrels, rocks, pots, shrine orbs ...: Link's AI node
// "持上げ" (class PlayerGrab, 1.9.0 vtable 0x7001cd5c20) with the children GrabReady / GrabUp / GrabUpAnmStop / GrabWait /
// GrabThrow / GrabPut / GrabStand; each object's LiftType picks the animation. The bomb looks right because Slate mode gives
// Link the frame (LINK_OTHER: Link's pad, Sora copies his bones, Link + Keyblade hidden). A barrel lifted from the Triangle
// pick-up window did not: Sora drove again in his own KH2 pose with the barrel floating over him and no throw.
// Carry mode: every frame, a plain-read walk of Link's AI chain (no guest call: RootAi = actor +0x558, each node's +8 must be
// Link, the running child = children(+0x30)[index(+0x1c)], count +0x28, <= 32 deep; rd_ok() before every read) looks for
// the PlayerGrab node. While it is there the frame is Link's through the same branch as Slate mode, so the existing puppet
// copies BotW's lift / carry walk / throw / put down (BotW's buttons: R throws, A puts down GUESS; heavy rocks slow him: BotW's
// own rules). Donald + Goofy are hidden meanwhile (party_other, as with the bomb: OK'd by the user for now).
// The state machine (carry_step, host-tested): on after CARRY_IN_N frames in PlayerGrab (never while loading, dead,
// swimming or as Mickey, nor while "blocked"). Off when:
//   - the grab ended: CARRY_OFF_N frames without PlayerGrab and Link stands (ground), or CARRY_OFF_AIR frames without it
//     anywhere (thrown, put down, dropped by a hit, the object broke / exploded, a cutscene took it, the walk failed);
//   - at once: carry 0 / a mode change, a load, a death, swimming, Mickey, a Drive Form / Roxas body change, the timeout
//     (carrymax presents). If Link is STILL in PlayerGrab then, it is "blocked": no new latch until the grab node is gone
//     (Sora drives again as before carry mode existed; nothing is left in Link's control).
// Every end sets S.fresh (the throw / put-down press is no attack) and closes the pick-up window.
// (s217 follow-up 2) PlayerGrab only runs for the lift / put down / throw; the HOLD between them is plain PlayerWait / PlayerMove.
// So "in it" = PlayerGrab OR Link +0x88 still holding the object first seen during this carry's PlayerGrab (carry_held_now: BotW's
// own grab actions read the carried object there). Entry still needs PlayerGrab; the ends above count frames without either.
// While carry mode is on, Link's BotW weapon is not put in Sora's hand (wpn_tick drawn = false), as his hands hold the object.
// The refusals (one log line, the feature idle until carry is set again): PlayerGrab's vtable slots 12 / 14 differ from
// 1.9.0 (other game version), or Link's RootAi never passes the node+8 == Link check for CARRY_BAD_N world frames.
// Live: carry 0/1/2 (DEFAULT CARRY_DEFAULT = 1 since s218: the user verified barrel, rock and pot; 0 = no scan at all, 2 = detect + log only, change
// nothing), carrylog 0/1 ([carry] chain walk result, leaf changes; enter / leave lines print whenever carry != 0),
// carrymax <presents> (the safety timeout, default CARRY_MAX_DEF), carrydump 1 / 2 (s217 follow-up, ONE-SHOT: Link's whole AI tree
// into the log now / at +30 and +120 presents after the next no-wall Triangle; plain reads; remove the line after it fired).
// (s217 follow-up) carrylog prints the walk whenever its result changes (path + leaf), at most one line per CARRY_LOG_GAP presents.
// Multiplayer: no packets. A second player sees what the SNAP shows today in Link's own states (Sora's puppet pose).
//
// The pure part (above CARRY_HOST's #ifndef) is host-tested: runtime/test/carry_test.c.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#define CARRY_DEFAULT 1          // (s218: verified by the user on the 0.5.3 trial -> default on; set carry 0 = off)
#define CARRY_IN_N 2             // frames in PlayerGrab before carry mode starts
#define CARRY_OFF_N 10           // frames out of it (and standing) before it ends
#define CARRY_OFF_AIR 60         // ... or this many out of it, standing or not
#define CARRY_MAX_DEF 36000      // presents: the safety timeout (10 min at 60, 5 at 120; s226 review R1: was 7200 = a shrine orb or a Korok rock carried over 2 min went "blocked")

// ------------------------------------------------------------------ pure logic (host-tested)
enum { CR_NONE, CR_ENTER, CR_LEAVE };
enum { CRW_NONE, CRW_ENDED, CRW_OFF, CRW_MODE, CRW_LOAD, CRW_DEATH, CRW_SWIM, CRW_MICKEY, CRW_FORM, CRW_TIMEOUT };
static const char *const CRW_NAME[] = {"-", "grab + hold over", "carry 0", "carry mode changed", "load", "death", "swimming", "Mickey",
                                       "Drive Form / body change", "timeout"};
typedef struct {
    int mode;        // live carry: 0 off, 1 on, 2 log only
    bool grab;       // Link's AI chain holds a PlayerGrab node this frame (lift / put down / throw)
    bool hold;       // (s217 follow-up 2) the object picked up in this carry is still in his hands (Link +0x88, see the glue)
    bool ground;     // Link's raw state is ground
    bool dead, loading, swim, mickey;
    int form;        // the body / form key (any change while on = an end)
    uint64_t present;
    int max;         // the timeout, presents
} CarryIn;
typedef struct {
    bool on, block;  // carry mode on; blocked = ended by force while Link still grabbed: no latch until the grab ends
    int mode0, form0;
    int in_n, out_n; // frames in / out of PlayerGrab in a row
    uint64_t t0;
    int why;         // the last end's reason (CRW_*)
} CarrySm;
static int carry_step(CarrySm *m, const CarryIn *in) {
    bool active = in->grab || in->hold;  // (s217 follow-up 2: the hold between the lift and the put down / throw counts too)
    m->in_n = in->grab ? m->in_n + 1 : 0;  // (entry: only a PlayerGrab episode starts carry mode)
    m->out_n = active ? 0 : m->out_n + 1;
    if (m->block && !active) m->block = false;
    if (m->on) {
        int why = !in->mode ? CRW_OFF : in->mode != m->mode0 ? CRW_MODE : in->loading ? CRW_LOAD : in->dead ? CRW_DEATH
                : in->swim ? CRW_SWIM : in->mickey ? CRW_MICKEY : in->form != m->form0 ? CRW_FORM
                : in->present < m->t0 || in->present - m->t0 >= (uint64_t)(in->max > 0 ? in->max : 1) ? CRW_TIMEOUT
                : m->out_n >= CARRY_OFF_N && (in->ground || m->out_n >= CARRY_OFF_AIR) ? CRW_ENDED : CRW_NONE;
        if (!why) return CR_NONE;
        m->on = false, m->why = why;
        if (active) m->block = true;  // (forced end mid-grab / mid-hold: Sora drives, BotW keeps the object until it is put down)
        return CR_LEAVE;
    }
    if (in->mode && !m->block && m->in_n >= CARRY_IN_N && !in->loading && !in->dead && !in->swim && !in->mickey) {
        m->on = true, m->t0 = in->present, m->mode0 = in->mode, m->form0 = in->form, m->why = CRW_NONE;
        return CR_ENTER;
    }
    return CR_NONE;
}

// ---- the AI chain walk (pure: plain reads through the caller's rd_ok, host-tested on a fake tree)
#define CARRY_VT_GRAB 0x7001cd5c20ull      // PlayerGrab (Ghidra 0x7101cd5c20, sb/s217/world/facvt_grab.txt)
#define CARRY_VT_GRAB_S12 0x7000ca5af8ull  // ... its slot 12 / 14 on 1.9.0 (the version gate)
#define CARRY_VT_GRAB_S14 0x7000ca5d98ull
#define CARRY_DEPTH 32
#define CARRY_BAD_N 600                    // world frames with a bad RootAi before the feature refuses
// (s217 follow-up, live last53.log: "chain walk: ok, no PlayerGrab (3 nodes: 7001d1dec0 7001cd62e8 7001cd60a0)" while lifting)
// Those 3 = the RootAi, PlayerRoot, PlayerNormal (sb/s217/world/fu_classes_vt.txt, every class of Player_Link.baiprog -> its
// 1.9.0 vtable). PlayerGrab (持上げ) is a direct child of PlayerNormal, which has 89 children: the old cap of 64 (as swa_ai_leaf)
// refused PlayerNormal's child array, so the walk always ended there. The cap is now CARRY_KIDS_MAX.
#define CARRY_KIDS_MAX 512
typedef struct { int res; int n, grab_at; uint64_t vt[CARRY_DEPTH]; uint64_t leaf_vt; uint16_t leaf_idx; uint64_t root, root_actor; } CarryWalk;
// res: 1 PlayerGrab found, 0 chain ok without it, -1 no Link actor, -2 RootAi unreadable / not Link's
static void carry_walk(uint64_t a, CarryWalk *w, bool (*rd_ok)(uint64_t, size_t)) {  // (rd_ok = the glue's; the host test passes its own)
    memset(w, 0, sizeof *w);
    w->grab_at = -1;
    if (!a) { w->res = -1; return; }
    uint64_t node = rd_ok(a + 0x558, 8) ? *(const uint64_t *)(uintptr_t)(a + 0x558) : 0;
    w->root = node;
    if (!node || !rd_ok(node, 0x38) || *(const uint64_t *)(uintptr_t)(node + 8) != a) {
        w->root_actor = node && rd_ok(node, 0x10) ? *(const uint64_t *)(uintptr_t)(node + 8) : 0;
        w->res = -2;
        return;
    }
    w->root_actor = a;
    for (int d = 0; d < CARRY_DEPTH; d++) {
        if (!rd_ok(node, 0x38) || *(const uint64_t *)(uintptr_t)(node + 8) != a) break;
        uint64_t vt = *(const uint64_t *)(uintptr_t)node;
        w->vt[w->n++] = vt;
        uint32_t idx = *(const uint16_t *)(uintptr_t)(node + 0x1c), cnt = *(const uint32_t *)(uintptr_t)(node + 0x28);
        uint64_t kids = *(const uint64_t *)(uintptr_t)(node + 0x30), next = 0;
        if (idx != 0xffff && cnt && cnt <= CARRY_KIDS_MAX && idx < cnt && rd_ok(kids, 8ull * cnt)) next = *(const uint64_t *)(uintptr_t)(kids + 8ull * idx);
        if (vt == CARRY_VT_GRAB) {
            w->grab_at = d, w->res = 1;
            if (next && rd_ok(next, 0x20) && *(const uint64_t *)(uintptr_t)(next + 8) == a)
                w->leaf_vt = *(const uint64_t *)(uintptr_t)next, w->leaf_idx = *(const uint16_t *)(uintptr_t)(next + 0x18);
            return;
        }
        if (!next) break;
        node = next;
    }
    w->res = 0;
}

#ifndef CARRY_HOST
// ------------------------------------------------------------------ game glue (sora.c, after sora_slate.h; carry_tick after sk_tick)
// Needs: link_actor, rd_ok, linkpos_link_state, linkpos_link_life, g_climb_until, S, g_fmots_on / g_fform, fm_in_form, IN_MICKEY, LOG.
#define CARRY_LOG_GAP 15                   // presents between two carrylog walk lines (the newest change prints after the gap)
#define CARRY_DUMP_ROWS 400                // carrydump: nodes printed at most
#define CARRY_DUMP_DEPTH 12
#define CARRY_DUMP_STACK 1024
static const struct { uint64_t vt; const char *name; } CARRY_LEAF[] = {
    {0x7001ccd0a0ull, "GrabReady"}, {0x7001ccd3e8ull, "GrabUp"}, {0x7001ccd500ull, "GrabUpAnmStop"}, {0x7001ccd618ull, "GrabWait"},
    {0x7001ccd2d0ull, "GrabThrow"}, {0x7001cccf88ull, "GrabPut"}, {0x7001ccd1b8ull, "GrabStand"},
    // (not leaves: names for the walk / dump lines only; the rest: sb/s217/world/fu_classes_vt.txt)
    {0x7001d1dec0ull, "RootAi"}, {0x7001cd62e8ull, "PlayerRoot"}, {0x7001cd60a0ull, "PlayerNormal"}, {0x7001cd5c20ull, "PlayerGrab"},
    {0x7001cd0460ull, "PlayerPickUp"},
};
static int g_carry = CARRY_DEFAULT;  // live carry
static int g_carry_log = 0;          // live carrylog
static int g_carry_max = CARRY_MAX_DEF;
static struct {
    int gate;        // 0 not checked, 1 ok, -1 refused (other version), -2 refused (chain)
    int bad_n;
    CarrySm m;
    int last_res;
    uint64_t last_leaf;
    const char *last_name;
    uint64_t sig_logged, log_at;  // (carrylog: the walk signature last printed, when)
    int dump;                     // live carrydump: 1 = dump now, 2 = armed: two dumps after the next pick-up try starts
    bool nox_was;
    uint64_t dump_at[2];
    uint64_t held;                // (s217 follow-up 2) the object Link +0x88 held during this carry's PlayerGrab episode (0 = none yet)
    bool hold_said;
} CARRY = {.last_res = 99};
static bool carry_on(void) { return g_carry == 1 && CARRY.m.on; }
static const char *carry_leaf_name(uint64_t vt) {
    for (size_t i = 0; i < sizeof CARRY_LEAF / sizeof *CARRY_LEAF; i++) if (CARRY_LEAF[i].vt == vt) return CARRY_LEAF[i].name;
    return vt ? "other" : "none";
}
static uint64_t carry_sig(const void *p, size_t n, uint64_t h) {  // (FNV-1a: the walk's result in one word, for the change log)
    for (size_t i = 0; i < n; i++) h = (h ^ ((const uint8_t *)p)[i]) * 0x100000001b3ull;
    return h;
}
// (s217 follow-up) live carrydump: Link's whole AI tree with plain reads (rd_ok before every read, no guest call, no write).
// One line per node (depth, child index under its parent, address, vtable + name when known, +8 actor, the words +0x10 .. +0x38:
// +0x18 AIProgram index, +0x1c running-child index, +0x28 child count, +0x30 children, the running child), PlayerGrab's next
// 0x100 bytes, a node's children are walked only when it is Link's (+8) and its first child is Link's too (an Action's words
// there are not a child list). Shared nodes print once. Caps: CARRY_DUMP_ROWS nodes, CARRY_DUMP_DEPTH deep.
static const char *carry_actor_name(uint64_t h);  // (below)
static void carry_dump(uint64_t a, const char *why) {
    uint64_t root = a && rd_ok(a + 0x558, 8) ? *(const uint64_t *)(uintptr_t)(a + 0x558) : 0;
    uint64_t p80 = a && rd_ok(a + 0x80, 16) ? *(const uint64_t *)(uintptr_t)(a + 0x80) : 0, p88 = a && rd_ok(a + 0x88, 8) ? *(const uint64_t *)(uintptr_t)(a + 0x88) : 0;
    LOG("[carrydump] begin (%s): Link %llx, RootAi (+0x558) %llx, state %#x, +0x80 %llx, +0x88 %llx %s\n", why, (unsigned long long)a, (unsigned long long)root,
        linkpos_link_state_word(), (unsigned long long)p80, (unsigned long long)p88, p88 ? carry_actor_name(p88) : "-");
    if (!a || !root) { LOG("[carrydump] end: no Link actor / RootAi\n"); return; }
    static uint64_t seen[CARRY_DUMP_ROWS];
    static struct { uint64_t node; uint16_t ci; uint8_t d; } stk[CARRY_DUMP_STACK];
    int ns = 0, sp = 0, rows = 0, shared = 0, capped = 0;
    stk[sp].node = root, stk[sp].ci = 0xffff, stk[sp].d = 0, sp++;
    while (sp) {
        sp--;
        uint64_t node = stk[sp].node;
        int d = stk[sp].d, ci = stk[sp].ci;
        int k = 0;
        while (k < ns && seen[k] != node) k++;
        if (k < ns) { shared++; continue; }
        if (rows >= CARRY_DUMP_ROWS) { capped++; continue; }
        seen[ns++] = node, rows++;
        if (!rd_ok(node, 0x40)) { LOG("[carrydump] %*sd%d [%d] %llx unreadable\n", 2 * d, "", d, ci == 0xffff ? -1 : ci, (unsigned long long)node); continue; }
        const uint8_t *b = (const uint8_t *)(uintptr_t)node;
        uint64_t vt, act, kids, w38;
        uint32_t w10, w14, w20, w24, cnt, w2c;
        uint16_t i18, i1a, i1c, i1e;
        memcpy(&vt, b, 8), memcpy(&act, b + 8, 8), memcpy(&w10, b + 0x10, 4), memcpy(&w14, b + 0x14, 4), memcpy(&i18, b + 0x18, 2);
        memcpy(&i1a, b + 0x1a, 2), memcpy(&i1c, b + 0x1c, 2), memcpy(&i1e, b + 0x1e, 2), memcpy(&w20, b + 0x20, 4), memcpy(&w24, b + 0x24, 4);
        memcpy(&cnt, b + 0x28, 4), memcpy(&w2c, b + 0x2c, 4), memcpy(&kids, b + 0x30, 8), memcpy(&w38, b + 0x38, 8);
        bool mine = act == a, list = mine && cnt && cnt <= CARRY_KIDS_MAX && rd_ok(kids, 8ull * cnt);
        uint64_t first = list ? *(const uint64_t *)(uintptr_t)kids : 0, cur = list && i1c < cnt ? *(const uint64_t *)(uintptr_t)(kids + 8ull * i1c) : 0;
        if (list && !(first && rd_ok(first, 0x10) && *(const uint64_t *)(uintptr_t)(first + 8) == a)) list = false;
        uint64_t cur_vt = cur && rd_ok(cur, 8) ? *(const uint64_t *)(uintptr_t)cur : 0;
        LOG("[carrydump] %*sd%d [%d] %llx vt %llx %s | +8 %llx%s | +10 %08x %08x +18 %04x +1a %04x +1c %u +1e %04x +20 %08x %08x +28 %u +2c %08x "
            "+30 %llx +38 %llx | cur %llx %s%s\n", 2 * d, "", d, ci == 0xffff ? -1 : ci, (unsigned long long)node, (unsigned long long)vt, carry_leaf_name(vt),
            (unsigned long long)act, mine ? " (Link)" : " (NOT Link)", w10, w14, i18, i1a, i1c, i1e, w20, w24, cnt, w2c, (unsigned long long)kids,
            (unsigned long long)w38, (unsigned long long)cur, cur ? carry_leaf_name(cur_vt) : "-", list ? "" : mine && cnt ? " [children not walked]" : "");
        if (vt == CARRY_VT_GRAB && rd_ok(node + 0x40, 0x100))
            for (int r = 0; r < 4; r++) {
                const uint64_t *q = (const uint64_t *)(uintptr_t)(node + 0x40 + 0x40 * r);
                LOG("[carrydump] %*s  PlayerGrab +%03x: %llx %llx %llx %llx %llx %llx %llx %llx\n", 2 * d, "", 0x40 + 0x40 * r, (unsigned long long)q[0],
                    (unsigned long long)q[1], (unsigned long long)q[2], (unsigned long long)q[3], (unsigned long long)q[4], (unsigned long long)q[5],
                    (unsigned long long)q[6], (unsigned long long)q[7]);
            }
        if (!list || d + 1 >= CARRY_DUMP_DEPTH) continue;
        for (int i = (int)cnt - 1; i >= 0; i--) {  // (reversed: printed in index order)
            if (sp >= CARRY_DUMP_STACK) { capped++; continue; }
            stk[sp].node = *(const uint64_t *)(uintptr_t)(kids + 8ull * (uint32_t)i), stk[sp].ci = (uint16_t)i, stk[sp].d = (uint8_t)(d + 1), sp++;
        }
    }
    LOG("[carrydump] end (%s): %d nodes, %d shared (printed once), %d over the caps\n", why, rows, shared, capped);
}
// every frame (carry_tick's first line): carrydump 1 = now; 2 = at +CARRY_DUMP_T0 / +CARRY_DUMP_T1 presents after the next no-wall
// Triangle (the pick-up try that lifts), i.e. once in the lift and once while he holds it
#define CARRY_DUMP_T0 30
#define CARRY_DUMP_T1 120
static void carry_dump_tick(uint64_t present, bool loading) {
    bool nox = g_climb_nox && present < g_climb_until;
    if (CARRY.dump == 1 && !loading) carry_dump(link_actor(), "carrydump 1"), CARRY.dump = 0;
    else if (CARRY.dump == 2) {
        if (nox && !CARRY.nox_was && !CARRY.dump_at[0])
            CARRY.dump_at[0] = present + CARRY_DUMP_T0, CARRY.dump_at[1] = present + CARRY_DUMP_T1,
            LOG("[carrydump] pick-up try at present %llu: dumps at +%d and +%d presents\n", (unsigned long long)present, CARRY_DUMP_T0, CARRY_DUMP_T1);
        for (int i = 0; i < 2; i++)
            if (CARRY.dump_at[i] && present >= CARRY.dump_at[i]) {
                CARRY.dump_at[i] = 0;
                if (!loading) carry_dump(link_actor(), i ? "lift +120 (holding)" : "lift +30");
                if (i) CARRY.dump = 0;
            }
    }
    CARRY.nox_was = nox;
}
static bool carry_gate(void) {
    if (CARRY.gate) return CARRY.gate > 0;
    uint64_t s12 = rd_ok(CARRY_VT_GRAB + 12 * 8, 8) ? *(const uint64_t *)(uintptr_t)(CARRY_VT_GRAB + 12 * 8) : 0;
    uint64_t s14 = rd_ok(CARRY_VT_GRAB + 14 * 8, 8) ? *(const uint64_t *)(uintptr_t)(CARRY_VT_GRAB + 14 * 8) : 0;
    if (s12 != CARRY_VT_GRAB_S12 || s14 != CARRY_VT_GRAB_S14) {
        CARRY.gate = -1;
        LOG("[carry] refused: PlayerGrab vtable %llx slots 12 / 14 = %llx / %llx, not %llx / %llx (other game version): carry idle\n",
            (unsigned long long)CARRY_VT_GRAB, (unsigned long long)s12, (unsigned long long)s14, (unsigned long long)CARRY_VT_GRAB_S12,
            (unsigned long long)CARRY_VT_GRAB_S14);
        return false;
    }
    CARRY.gate = 1;
    LOG("[carry] PlayerGrab vtable %llx checked (1.9.0): carry %d (%s)\n", (unsigned long long)CARRY_VT_GRAB, g_carry,
        g_carry == 2 ? "detect + log only" : "Link's frame while he carries");
    return true;
}
static void carry_end_effects(uint64_t present) {  // (mode 1 only: the frame goes back to Sora cleanly)
    S.fresh = true;            // (the throw / put-down press is not an attack)
    g_climb_until = present;   // (no pick-up window left open)
}
// (s217 follow-up 2) What Link holds. Live s217: the PlayerGrab node ENDS when the lift is done; while he holds and walks, PlayerNormal
// runs plain PlayerWait / PlayerMove (dump +120: PlayerNormal +0x1c = 47 = PlayerWait, AIProgram Action_93 = the ordinary 待機; the AI
// tree has no hold node). BotW's own grab actions (1.9.0 decompile sb/s217/world/fu2/dec_grab.c + dec_link.c) find the carried object
// with FUN_7101516384(Link) = *(Link + 0x88) unless its BaseProc state (+0x64) is 3 (being deleted): GrabUp's calc places it with
// it and ends the lift when it is 0, GrabPut / GrabThrow release it through it. So: the object = Link +0x88 (read here with rd_ok, the
// same state-3 rule), and only the one first seen while PlayerGrab ran counts as "held" (an unrelated +0x88 never starts or keeps
// carry mode). Plain reads only.
#define CARRY_HELD_DIST 6.0f  // u: the held object's origin to Link's (sanity; GUESS: a carried object is always within 3)
static uint64_t carry_held_now(uint64_t a, float *dist) {
    *dist = -1;
    if (!a || !rd_ok(a + 0x88, 8) || !rd_ok(a + 0x398, 0x30)) return 0;
    uint64_t h = *(const uint64_t *)(uintptr_t)(a + 0x88);
    if (!h || h == a || !rd_ok(h, 0x3c8) || *(const uint8_t *)(uintptr_t)(h + 0x64) == 3) return 0;
    const float *m = (const float *)(uintptr_t)(h + 0x398), *l = (const float *)(uintptr_t)(a + 0x398);
    float dx = m[3] - l[3], dy = m[7] - l[7], dz = m[11] - l[11];
    *dist = sqrtf(dx * dx + dy * dy + dz * dz);
    return *dist <= CARRY_HELD_DIST ? h : 0;
}
static const char *carry_actor_name(uint64_t h) {  // (BaseProc +0x10 = mName's string; bounded, printable or "?")
    static char nb[48];
    uint64_t p = h && rd_ok(h + 0x10, 8) ? *(const uint64_t *)(uintptr_t)(h + 0x10) : 0;
    int k = 0;
    if (p && rd_ok(p, 1))
        for (; k < (int)sizeof nb - 1 && rd_ok(p + (uint64_t)k, 1); k++) {
            char ch = *(const char *)(uintptr_t)(p + (uint64_t)k);
            if (ch <= ' ' || ch >= 127) break;
            nb[k] = ch;
        }
    nb[k] = 0;
    return k ? nb : "?";
}
// sora.c tick, right after sk_tick: returns LINK_OTHER while carry mode is on (carry 1)
static int carry_tick(uint64_t present, int st, uint32_t sw, bool dead, bool loading, bool swim) {
    if (CARRY.dump) carry_dump_tick(present, loading);  // (s217 follow-up: live carrydump, reads only)
    if (!g_carry && !CARRY.m.on) return st;  // (default: no scan at all)
    CarryWalk w;
    memset(&w, 0, sizeof w);
    bool scan = g_carry && carry_gate() && CARRY.gate > 0 && !loading;
    if (scan) {
        carry_walk(link_actor(), &w, rd_ok);
        if (w.res == -2 && !dead) {
            if (++CARRY.bad_n >= CARRY_BAD_N) {
                CARRY.gate = -2, scan = false;
                LOG("[carry] refused: Link's RootAi (actor +0x558 = %llx) never points back at Link (%llx, node +8 = %llx) in %d frames: carry idle\n",
                    (unsigned long long)w.root, (unsigned long long)link_actor(), (unsigned long long)w.root_actor, CARRY_BAD_N);
            }
        } else if (w.res >= 0) CARRY.bad_n = 0;
        // (s217 follow-up) a line whenever the walk's result changes (result, every vtable on the path, the grab's leaf), at most one
        // per CARRY_LOG_GAP presents: after the gap the newest state prints if it still differs from the last printed one
        uint64_t sig = carry_sig(&w.res, sizeof w.res, 0xcbf29ce484222325ull);
        sig = carry_sig(w.vt, sizeof *w.vt * (size_t)w.n, sig), sig = carry_sig(&w.leaf_vt, sizeof w.leaf_vt, sig);
        if (g_carry_log && sig != CARRY.sig_logged && present >= CARRY.log_at + CARRY_LOG_GAP) {
            char path[CARRY_DEPTH * 28 + 1] = "";
            int k = 0;
            for (int i = 0; i < w.n && k < (int)sizeof path - 28; i++) {
                const char *nm = carry_leaf_name(w.vt[i]);
                k += strcmp(nm, "other") ? snprintf(path + k, sizeof path - k, " %s", nm) : snprintf(path + k, sizeof path - k, " %llx", (unsigned long long)w.vt[i]);
            }
            LOG("[carry] chain walk: %s (root %llx, %d nodes:%s)%s%s at present %llu, state %#x\n",
                w.res == 1 ? "PlayerGrab found" : w.res == 0 ? "ok, no PlayerGrab" : w.res == -1 ? "no Link actor" : "RootAi is not Link's",
                (unsigned long long)w.root, w.n, path, w.res == 1 ? ", leaf " : "", w.res == 1 ? carry_leaf_name(w.leaf_vt) : "",
                (unsigned long long)present, sw);
            CARRY.sig_logged = sig, CARRY.log_at = present;
        }
        CARRY.last_res = w.res;
    }
    bool grab = scan && w.res == 1;
    float hd = -1;
    uint64_t h = scan ? carry_held_now(link_actor(), &hd) : 0;
    if (CARRY.m.on && grab && h && !CARRY.held) {  // (the object attached during the lift: this carry's object)
        CARRY.held = h;
        LOG("[carry] holds %llx %s (Link +0x88, %.2f u away) at t+%llu\n", (unsigned long long)h, carry_actor_name(h), hd, (unsigned long long)(present - CARRY.m.t0));
    }
    bool hold = CARRY.held && h == CARRY.held;
    if (CARRY.held && !hold && !CARRY.hold_said) {
        CARRY.hold_said = true;
        LOG("[carry] %llx left his hands (Link +0x88 now %llx) at present %llu\n", (unsigned long long)CARRY.held, (unsigned long long)h, (unsigned long long)present);
    }
    CarryIn in = {.mode = CARRY.gate > 0 ? g_carry : 0, .grab = grab, .hold = hold, .ground = linkpos_link_state() == LINK_GROUND,
                  .dead = dead || linkpos_link_life() <= 0, .loading = loading, .swim = swim || sw == 0x410, .mickey = IN_MICKEY,
                  .form = (g_fmots_on ? g_fform + 1 : 0) * 2 + (fm_in_form() ? 1 : 0), .present = present, .max = g_carry_max};
    int mode_was = CARRY.m.mode0;
    int ev = carry_step(&CARRY.m, &in);
    if (ev == CR_ENTER) {
        CARRY.last_leaf = w.leaf_vt, CARRY.last_name = carry_leaf_name(w.leaf_vt);
        CARRY.held = 0, CARRY.hold_said = false;
        LOG("[carry] on at present %llu (Link in PlayerGrab, leaf %s idx %u, state %#x): %s\n", (unsigned long long)present, CARRY.last_name, w.leaf_idx, sw,
            g_carry == 1 ? "Link has the pad, Sora takes his pose" : "carry 2: log only, nothing changed");
        if (g_carry == 1) g_climb_until = present;  // (the pick-up window that lifted it is over)
    } else if (ev == CR_LEAVE) {
        LOG("[carry] off (%s, last leaf %s) after %llu presents, state %#x%s\n", CRW_NAME[CARRY.m.why], CARRY.last_name ? CARRY.last_name : "none",
            (unsigned long long)(present - CARRY.m.t0), sw, CARRY.m.block ? "; Link still grabs / holds: no carry mode until that ends" : "");
        if (mode_was == 1) carry_end_effects(present);
    } else if (CARRY.m.on && grab && w.leaf_vt != CARRY.last_leaf) {
        CARRY.last_leaf = w.leaf_vt, CARRY.last_name = carry_leaf_name(w.leaf_vt);
        if (g_carry_log) LOG("[carry] leaf %s (idx %u, vt %llx) at t+%llu\n", CARRY.last_name, w.leaf_idx, (unsigned long long)w.leaf_vt, (unsigned long long)(present - CARRY.m.t0));
    }
    if (!CARRY.m.on && !CARRY.m.block && !grab) CARRY.held = 0, CARRY.hold_said = false;  // (that carry is over: the next lift captures anew)
    return carry_on() ? LINK_OTHER : st;
}
static bool carry_live_set(const char *key, int v) {
    if (!strcmp(key, "carry")) {
        int was = g_carry;
        g_carry = v < 0 || v > 2 ? 0 : v;
        if (CARRY.gate < 0 && g_carry != was) CARRY.gate = 0, CARRY.bad_n = 0;  // (a new value retries a refused gate)
        LOG("[carry] carry %d: %s\n", g_carry, g_carry == 1 ? "barrels / rocks / pots are carried with BotW's Link moves" : g_carry == 2 ? "detect + log only" : "off");
    } else if (!strcmp(key, "carrylog")) g_carry_log = v != 0, CARRY.last_res = 99, CARRY.sig_logged = 0, CARRY.log_at = 0, LOG("[carry] log %s\n", g_carry_log ? "on" : "off");
    else if (!strcmp(key, "carrydump")) {  // (s217 follow-up, ONE-SHOT: remove the line after it fired)
        CARRY.dump = v == 1 || v == 2 ? v : 0, CARRY.dump_at[0] = CARRY.dump_at[1] = 0, CARRY.nox_was = false;
        LOG("[carrydump] %s\n", CARRY.dump == 1 ? "Link's AI tree now (next frame)" : CARRY.dump == 2 ? "armed: two dumps after the next no-wall Triangle (lift +30, +120 presents)" : "off");
    }
    else if (!strcmp(key, "carrymax")) g_carry_max = v < 60 ? 60 : v, LOG("[carry] timeout %d presents\n", g_carry_max);
    else return false;
    return true;
}
#endif  // CARRY_HOST

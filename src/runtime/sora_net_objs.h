// sora_net_objs.h (s289 objs): two co-op features from Savage Wind PR #294 "Online mode" (byluki, fixes by mstampfli; MIT),
// re-written for our Melty rooms and our exe's lists. Pure parts: sora_net_objs_core.h (host test runtime/test/net_objs_test.c);
// the wire: NET_OBJS / NET_OBJEV in sora_net_proto.h. Included by sora_net.h after sora_net_noauto.h; net_objs_frame from
// net_frame (game thread, inside linkpos_frame), net_objs_recv from net_recv_all, net_objs_live_set from net_live_set.
//   netbump 0|1   players bump into each other instead of walking through: our Link is pushed out of a friend's drawn Sora
//                 (capsule 0.7 m, 1.7 m tall, at most 0.5 m a frame) with linkpos_move (the goto / noclip path). Nothing
//                 sent for the push: his game pushes his Link. No push while we are DOWNED / dead / loading / riding / in an
//                 event / ragdolled (a duel's knock-down with duelrag) / flying (moved > 0.25 m in a frame: a knock-back, a
//                 warp); a friend loading / dead / DOWNED / on another map / fading is not bumped into. DLL only.
//                 (s289 objs part 4) nor a friend who rides: while our Link rides, his horse goes to a Melty room (kind-3
//                 records, sora_net_horse.h) when netbump or nethorse is on; a friend whose horse record is fresh rides.
//   netobjs 0|1   map objects move and break the same in every world of a Melty room:
//                 breaks (DLL only: the exe's linkpos_breakables / linkpos_trees / linkpos_trunks lists, both game versions):
//                 a crate / barrel / ore / pot / ... (BotW's Breakable list) or a tree we were near that went (its actor
//                 deleted, our Link within 15 m and moved < 10 m since; a tree also needs BotW's fresh trunk at its foot) is
//                 one we broke: told once (NET_OBJEV BREAK, reliable). Theirs is broken here with BotW's own step
//                 (linkpos_break_hit: life to 0, BotW breaks it with its drops and effects; a tree: linkpos_tree_hit, BotW's
//                 fell, the same way as his fell). The room's broken list (2048) is re-applied while the room lasts (an object
//                 on it that comes near = broken then) and re-told one entry every 1.5 s after 4.5 s without a new break (a
//                 late joiner learns it). Revival flags (a break remembered after a reload) are NOT ours: the loot helper's
//                 networld.
//                 moves (needs the exe rows objs_body_to / objs_set_mtx / objs_wake = an exe relink with the s289 sora_sites.h;
//                 1.9.0 only): the nearest 8 bodied map objects within 20 m (no_kind) are sampled at 20 Hz; one that moves
//                 (> 2 cm) is sent while it moves and 1 s after (NET_OBJS records, on the ESYNC lanes in a room). His records:
//                 whose it is by the PR's rules (no_his: who began first, a tie = the lower player id, one he holds is his);
//                 his are taken to his place through their body every frame (woken first) and 90 frames after his last, then
//                 put there if still > 0.5 m off. A breakable of BotW's (DamageParam Breakable: a barrel, a crate, a pot...) is
//                 NOT taken through its body (s289 lab: an old barrel, Life 1, broke in the follower's world): it stays, and is
//                 put at his place once his rests (his last record / the hold's end) with BotW's own warp (s289 fix 2: its
//                 class's Actor::setMtx, vtable slot 85 = the actor, its model and its body's queued transform, no velocity;
//                 DLL only, the actor_set_mtx row every exe has); never into our Link / another map object (it waits, 10 s at
//                 most). One that goes while we follow it (or
//                 within 1 s after) is never told to the room as our break. One the friend holds with Magnesis / stopped with Stasis is his
//                 (netshots_friend_holds, the shots helper's: sora_net_shots.h, included before this file; s289 shots merge).
//                 At most 2 NET_OBJS packets per 50 ms (they share the 16 lanes with ESYNC / shots); NET_OBJEV is rare.
//   netbumplog 0|1, netobjslog 0|1   one line per bump start / per break, follow, put-there; netobjsdiag 1 = one status line.
//   netobjsbrkfol 0|1   (lab only, s289 fix) 1 = breakables are taken through their body again as before the fix, to
//                 measure why one broke (netobjslog's "following ... over" line: ours' biggest move in a frame, how far off /
//                 over his place); they may break then. Its breaks are still not told. (Live keys are 15 letters at most:
//                 sora.c "set %15s %d"; the first name, netobjsbreakfollow, was never read.)
//   netmovers 0|1 (s289 objs part 3, sora_net_movers.h)   the level's mechanisms (lifts, elevators, doors, gates, gears,
//                 bridges, turning floors...) move the same in every world: ours that moves by itself goes to the room, his
//                 are taken where his are, playing his AS. netmoverride 0 (default): his moves of one our Link is at (on /
//                 under / beside it) are not shown while he is; 1 (lab) = slow steps through its body. netmoverslog 0|1|2.
//                 Needs the s289 exe rows (an exe relink, 1.9.0 only).
//   nethorse 0|1 (s289 objs part 4, sora_net_horse.h; default 1 since s291)   the horse a friend rides shows under his Sora: model-only puppets
//                 (OnlineHorse_*) the s289 horse step bakes into the RomFS overlay from the player's own game files (NOT
//                 installed by this build: tools/sorabuild/nethorse). nethorselog 0|1|2.
// Keys 0 (default; nethorse is default 1 since s291) = today exactly: nothing read, sent or moved; a received NET_OBJS / NET_OBJEV is dropped and counted.
// UDP (no Melty room): netobjs sends nothing (MULTIPLAYER IS MELTY ONLY); netbump works on any transport (it sends nothing).
#pragma once
#include "sora_net_objs_core.h"
#include "linkpos_break.h"
#include "linkpos_world.h"
#include "sw_api.h"
#include "sora_sites.h"
// (every exe function through GetProcAddress / the SwApi at bind time, as sora_net_noauto.h: a host test that includes
// sora_net.h links without them, and an exe without one = that part stays off)
static int netshots_friend_holds(uint32_t map_hashid);  // (the shots helper's, s289: 1 = a friend holds it / stopped it now)

static int g_net_bump, g_net_bump_log;      // live "set netbump 1", "set netbumplog 1"
static int g_net_objs = 1, g_net_objs_log;  // live "set netobjs 0|1", "set netobjslog 1". (s290 flip: netobjs default ON, the user: "Flip the passes ON"; its room line: sora_net_flip.h)
static int g_net_objs_brkfol;               // live "set netobjsbrkfol 1" (lab: breakables through their body, as before)
// (s289 objs fix 2) a live key's name: 15 letters at most (sora.c: char key[16], "set %15s %d"), else it does not compile
#define NO_KEYIS(key, lit) (sizeof(char[sizeof(lit) <= 16 ? 1 : -1]) && !strcmp((key), (lit)))
static int g_net_movers, g_net_movers_log, g_net_mover_ride;  // (s289 objs part 3) live "set netmovers 1", "netmoverslog 1|2", "netmoverride 1"
static int g_net_horse = 1, g_net_horse_log;                  // (s289 objs part 4) live "set nethorse 0|1" (default ON since s291: two-copy lab F0-F7 PASS, user), "nethorselog 1|2"
static int g_net_horse_coat;  // (s291 horsefix) live "set nethorsecoat 1": his horse's coat goes with it and his puppet wears it (default OFF)
static int g_net_horse_far;   // (s292 coopfix) live "set nethorsefar N" (m): no puppet of a horse farther than N + a wait after the game removed one (0 = OFF, as before)
static bool nh_rides(int k, uint64_t now);                    // (sora_net_horse.h: friend slot k rides now; netbump leaves him alone)
static void nm_scan_see(uint64_t a, const char *name, const float L[3]);  // (sora_net_movers.h: the actors' walk found a mechanism)
static void nm_scan_end(const float L[3]);

#define NO_VALID(p) ((p) >= 0x7000000000ull && (p) < 0x7400000000ull)
#define NO_IN_IMAGE(p) ((p) >= 0x7000000000ull && (p) < 0x7004000000ull)
#define NO_NEAR_N 160        // watched breakables / trees (drawn near us)
#define NO_FORGET_MS 5000    // one not drawn this long is no longer watched (seen again = watched again)
#define NO_SCAN_MS 250       // breakables / trees looked at this often
#define NO_ENFORCE_MS 1000   // one on the room's list, still standing here: broken again at most this often
#define NO_POS_M 5.0f        // his object of a HashId must stand this near ours (a HashId of another map = not it)
#define NO_ACT_MS 500        // all actors looked through this often (moves)
#define NO_MAX_ACTORS 8192
#define NO_KNOWN_N 512       // bodied map objects within NO_FAR (his records matched to them)
#define NO_TRK_N 24          // watched for moves: our nearest NO_TRACK + the ones of his we follow
#define NO_TRK_KEEP_MS 5000
#define NO_SAMPLE_MS 50
#define NO_PKTS_PER_TICK 2   // (lanes: shared with ESYNC / shots, 16 per 50 ms)

typedef struct {
    uint32_t h; uint8_t kind; bool seen_now;
    uint64_t actor, unit; uint32_t id;
    float pos[3], link_at[3];
    int32_t life;
    uint64_t seen_ms, enforce_ms;
    char name[40];
} NoNear;
typedef struct { uint32_t h; uint64_t actor, unit; uint32_t id; } NoKnown;
typedef struct {
    uint32_t h; uint64_t actor, unit; uint32_t id;
    float last[3]; bool sampled, followed_said;
    uint64_t touch_ms;
    NoOwn o;
    char name[40];
    bool fragile, his_rest;  // (s289 objs fix) a breakable: never through its body; his last record came (put it there)
    bool put_wait, put_said; float put_m[12]; uint64_t put_until, put_check_at;  // (s289 objs fix 2) a warp to his place to do
    NoSettle settle; bool put_rest; int reputs;  // (s289 objs fix 3) after the warp: its own moves are the put's; put_m = his rest?
    // (s289 objs fix) the lab's numbers for one follow: frames, his records, his biggest step between two records, ours'
    // biggest move in a frame, farthest off his place, highest over it
    int fol_frames, fol_recs;
    float fol_rec_jump, fol_step, fol_off, fol_rise, fol_prev[3], his_prev[3];
    bool fol_prev_ok, his_prev_ok;
    NoRxNear rxn;  // (s290 revfix F3) his last kind-1 record that passed the near check
} NoTrk;

static struct {
    bool bound, moves;
    const SwApi *api;
    const SoraSites *ss;
    uint64_t body_to, set_mtx, wake;
    uint64_t warp_base;  // (s289 objs fix 2) Actor::setMtx, the base class's (row actor_set_mtx): only to tell an own one apart
    uint64_t as_in_slot, play_as, req, take, release, del, creator_p, heap_p;  // (s289 objs parts 3-4: the movers' / horses' rows)
    bool as_ok, movers_ok, horse_ok;                                          // (each 0 = that part stays off here)
    LinkposBreakablesFn brk; LinkposBreakHitFn brk_hit; LinkposTreesFn trees; LinkposTreeHitFn tree_hit; LinkposTrunksFn trunks;
    void (*move)(float dx, float dy, float dz);  // linkpos_move: added to Link's body once a frame (sora.c goto / noclip)
    uint32_t (*state_word)(void);                // linkpos_link_state_word
    int (*ragdoll)(void);                        // linkpos_link_ragdoll
    bool (*link_pos)(float p[3]);                // linkpos_link_pos
    bool room, udp_said;
    uint16_t seq;
    NoRx rx[NET_MAX_PEERS];  // (s290 rv3fix #4: NET_OBJS seq per sender slot)
    NoWorld world;
    NoNear nr[NO_NEAR_N]; int nnr;
    NoKnown kn[NO_KNOWN_N]; int nkn;
    NoTrk tk[NO_TRK_N];
    uint64_t scan_at, act_at, sample_at;
    NoFollowed followed;  // (s289 objs fix: what went while we followed it was not our break)
    struct { uint32_t bumps, tx_pkt, tx_rec, tx_ev, tx_told, rx_pkt, rx_rec, rx_ev, rx_off, rx_bad, rx_hidden, broke_here, applied,
             enforced, wrong_pos, follows, put_there, no_actor, tick_cap, follow_broke, rest_puts,
             put_waits, put_gave_up, put_refused, put_ours, put_stayed, put_back,
             settle_near, settle_reput, settle_kept, settle_late, settle_link,  // (s289 objs fix 3)
             riders,
             rx_dup, rx_far, rx_unp; } st;  // (s289 objs fix 2: the warps; s290 rv3fix: kind-1 records dropped as older / too far from our copy / by net_unpack_objs)
} NOB;
static uint64_t g_no_act[NO_MAX_ACTORS];

// ---- game memory (read-only through the exe's readable(), never faults; nothing here writes) ----
static bool rd_ok_no(uint64_t a, size_t n) { return NOB.api && NO_VALID(a) && NOB.api->readable(a, n) == n; }
static uint64_t no_rd64(uint64_t a) { return rd_ok_no(a, 8) ? *(const uint64_t *)(uintptr_t)a : 0; }
static uint32_t no_rd32(uint64_t a) { return rd_ok_no(a, 4) ? *(const uint32_t *)(uintptr_t)a : 0; }
static uint8_t no_rd8(uint64_t a) { return rd_ok_no(a, 1) ? *(const uint8_t *)(uintptr_t)a : 0; }
// Link's actor (sora.c link_actor: PlayerInfo +0x60), 0 = none
static uint64_t no_link_actor(void) {
    uint64_t pi = NOB.ss ? no_rd64(NOB.ss->player_info) : 0;
    return pi ? no_rd64(pi + 0x60) : 0;
}
// a map object's HashId (linkpos.c actor_hid: actor +0x7c8 its map object, +0x58 the HashId; 0xFFFFFFFF = none), 0 = none
static uint32_t no_hid(uint64_t a) {
    uint64_t mo = no_rd64(a + 0x7c8);
    if (!mo) return 0;
    uint32_t h = no_rd32(mo + 0x58);
    return h == 0xFFFFFFFFu ? 0 : h;
}
// the actor still lives: its proc unit (+0x68) holds its id (+0x40 = actor +0x60) and it (+0x48), not being deleted (+0x64 3)
static bool no_alive(uint64_t a, uint64_t unit, uint32_t id) {
    return a && unit && no_rd64(unit + 0x48) == a && no_rd32(unit + 0x40) == id && no_rd8(a + 0x64) != 3;
}
// a physical object: its body (+0x190) is a real physics object, its vtable in the game's image (PR has_body)
static bool no_has_body(uint64_t a) {
    uint64_t body = no_rd64(a + 0x190);
    return body && !(body & 7) && NO_IN_IMAGE(no_rd64(body));
}
// (s289 objs fix) a breakable of BotW's: ActorParam (+0x570) -> DamageParam (+0xd8) -> Breakable (+0x418) set, the 3 loads
// the exe's breakables list makes (linkpos.c brk_dp, s55: VERIFIED there, both game versions)
static bool no_fragile(uint64_t a) {
    uint64_t ap = no_rd64(a + 0x570), dp = ap ? no_rd64(ap + 0xd8) : 0;
    return dp && no_rd8(dp + 0x418) != 0;
}
// (s289 objs fix 2 nit) a breakable kept out of its body (put there at rest): not with the lab key netobjsbrkfol 1
static bool no_put_kind(const NoTrk *t) { return t->fragile && !g_net_objs_brkfol; }
static const char *no_who(uint8_t player) {  // (a friend's name by his player id, for the log)
    for (int k = 0; k < NET_MAX_PEERS; k++)
        if (NET.slot[k].used && NET.slot[k].p.player == player && NET.slot[k].p.name[0]) return NET.slot[k].p.name;
    return "a friend";
}
static bool no_mtx(uint64_t a, float m[12]) {
    if (!NO_VALID(a) || !rd_ok_no(a + 0x398, 48)) return false;
    memcpy(m, (const void *)(uintptr_t)(a + 0x398), 48);
    return isfinite(m[3]) && isfinite(m[7]) && isfinite(m[11]);
}
static float no_d3(const float a[3], const float b[3]) {
    float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return sqrtf(x * x + y * y + z * z);
}
// BotW's actor list from one actor's proc unit, both ways (PR all_actors, ported as is, every read checked)
static int no_all_actors(uint64_t from, uint64_t *out, int max) {
    if (!NO_VALID(from) || !rd_ok_no(from, 0x50)) return 0;
    uint64_t vt = no_rd64(from);
    int n = 0;
    for (int dir = 0; dir < 2; dir++) {
        uint64_t u = from;
        for (int k = 0; k < 16384 && n < max; k++) {
            if (dir == 1 || k > 0) {
                uint64_t a = no_rd64(u + 0x48);
                if (NO_VALID(a) && no_rd32(a + 0x60) == no_rd32(u + 0x40)) out[n++] = a;
            }
            uint64_t nx = no_rd64(u + (dir ? 0x10 : 0x18));
            if (!NO_VALID(nx) || nx - 0x10 == from) break;
            u = nx - 0x10;
            if (!rd_ok_no(u, 0x50) || no_rd64(u) != vt) break;
        }
        if (dir == 0 && n) n--;  // (PR: the first one again, going back)
    }
    return n;
}

static void no_bind(void) {
    if (NOB.bound) return;
    NOB.bound = true;
    HMODULE exe = GetModuleHandleA(NULL);
    NOB.brk = (LinkposBreakablesFn)(void *)GetProcAddress(exe, "linkpos_breakables");
    NOB.brk_hit = (LinkposBreakHitFn)(void *)GetProcAddress(exe, "linkpos_break_hit");
    NOB.trees = (LinkposTreesFn)(void *)GetProcAddress(exe, "linkpos_trees");
    NOB.tree_hit = (LinkposTreeHitFn)(void *)GetProcAddress(exe, "linkpos_tree_hit");
    NOB.trunks = (LinkposTrunksFn)(void *)GetProcAddress(exe, "linkpos_trunks");
    NOB.move = (void (*)(float, float, float))(void *)GetProcAddress(exe, "linkpos_move");
    NOB.state_word = (uint32_t (*)(void))(void *)GetProcAddress(exe, "linkpos_link_state_word");
    NOB.ragdoll = (int (*)(void))(void *)GetProcAddress(exe, "linkpos_link_ragdoll");
    NOB.link_pos = (bool (*)(float *))(void *)GetProcAddress(exe, "linkpos_link_pos");
    const SoraSites *(*ss)(void) = (const SoraSites *(*)(void))(void *)GetProcAddress(exe, "sora_sites");
    NOB.ss = ss ? ss() : NULL;
    const SwApi *(*get)(uint32_t) = (const SwApi *(*)(uint32_t))(void *)GetProcAddress(exe, SW_API_EXPORT);
    NOB.api = get ? get(1) : NULL;
    const SwApi *a = NOB.api;
    if (!SW_HAS(a, readable)) NOB.api = a = NULL;  // (every read goes through it)
    if (SW_HAS(a, site)) NOB.body_to = a->site("objs_body_to"), NOB.set_mtx = a->site("objs_set_mtx"), NOB.wake = a->site("objs_wake");
    if (SW_HAS(a, site)) NOB.warp_base = a->site("actor_set_mtx");  // (s289 objs fix 2: a row since s168, both game versions)
    NOB.moves = NOB.body_to && NOB.set_mtx && NOB.wake && NOB.ss && SW_HAS(a, call) && SW_HAS(a, in_frame) && SW_HAS(a, actor_name);
    if (SW_HAS(a, site)) {  // (s289 objs parts 3-4)
        NOB.as_in_slot = a->site("objs_as_in_slot"), NOB.play_as = a->site("objs_play_as"), NOB.req = a->site("loot_request");
        NOB.take = a->site("loot_handle_take"), NOB.release = a->site("loot_handle_release"), NOB.del = a->site("loot_delete");
        NOB.creator_p = a->site("loot_creator_got"), NOB.heap_p = a->site("loot_heap_got");  // (s289 loot's rows: the same functions)
    }
    NOB.as_ok = NOB.moves && NOB.as_in_slot && NOB.play_as;
    NOB.movers_ok = NOB.moves;  // (body_to / set_mtx / wake; without the AS rows his AS is not played, his moves are)
    NOB.horse_ok = NOB.as_ok && NOB.req && NOB.take && NOB.release && NOB.del && NOB.creator_p && NOB.heap_p;
}
// (s289 objs parts 3-4) why a part is off here, for its diag / first line: "movers" or "horse"
static const char *no_rows_txt(const char *part) {
    static char t[200];
    bool v190 = NOB.ss && NOB.ss->pause_state == 0x7001e1611cull;
    if (!NOB.api) snprintf(t, sizeof t, "%s OFF: this exe has no sw_api", part);
    else if (!v190) snprintf(t, sizeof t, "%s OFF: not found for this game version (1.9.0 only)", part);
    else snprintf(t, sizeof t, "%s OFF: NO s289 objs / loot rows (%s): NEEDS EXE RELINK with the s289 sora_sites.h", part,
                  !strcmp(part, "movers") ? "objs_body_to / objs_set_mtx / objs_wake" : "objs_as_in_slot / objs_play_as / loot_request / loot_handle_* / loot_delete / loot_creator_got / loot_heap_got");
    return t;
}
static const char *no_moves_txt(void) {
    if (NOB.moves) return "moves on";
    if (!NOB.api) return "moves OFF: this exe has no sw_api";
    // (1.9.0 = sora_sites' pause_state row 0x7001e1611c, as sora_invsora.h tells them apart)
    return NOB.ss && NOB.ss->pause_state == 0x7001e1611cull ? "moves OFF: NO objs_body_to / objs_set_mtx / objs_wake row: NEEDS EXE RELINK with the s289 sora_sites.h"
                                                        : "moves OFF: not found for this game version (1.9.0 only)";
}
static bool no_call(uint64_t fn, uint64_t x0, const float m[12]) {  // (only inside linkpos_frame: net_frame is)
    if (!fn || !NOB.api || !NOB.api->in_frame()) return false;
    SwArgs a;
    memset(&a, 0, sizeof a);
    a.x[0] = x0, a.nx = 6;  // (x1..x5 0, as the PR's call(): unloaded ones would keep the caller's values)
    if (m) a.x[1] = 0, a.x_scratch = 2, a.scratch = 64, a.init = m, a.init_len = 48;  // (x1 = a Matrix34f in the scratch)
    SwRet r;
    return NOB.api->call(NULL, fn, &a, &r);
}
// (s289 objs fix 2) BotW's own warp of an actor: its class's Actor::setMtx (vtable slot 85; the base one, 0x70015337ac on
// 1.9.0, disassembled s289 from main190_flat.bin, = s168's actor_set_mtx row). w2 1: the matrix into the actor (+0x398) and
// its model (+0x4e0) now; then into its body: RigidBody::setTransform(+0x190 body, m, 1) = its motion's queued transform
// (flag 0x20), applied at the physics step, no velocity (a character controller's own setter for one that has one); the
// actor told (slot 42). w3 1: its InstanceSet (+0x578) flagged as warped (+0x20 |= 2 / 6, the 0x400000 state), as BotW's
// warps do (GUESS: no contact history from the old place). The PR's FN_SET_MTX (row objs_set_mtx, 0x700152261c) writes
// +0x398 only: a resting body put the actor back (lab D2, 3:05 am: 0.78 m and 3.2 m off, twice, never moved).
// own = its class has its own setMtx (not the base one): still called (the game calls it so), said in the log.
#define NO_SETMTX_SLOT 85
static bool no_warp(uint64_t a, const float m[12], bool *own) {
    *own = false;
    if (!NOB.api || !SW_HAS(NOB.api, vcall) || !NOB.api->in_frame()) return false;
    uint64_t vt = no_rd64(a), fn = NO_IN_IMAGE(vt) ? no_rd64(vt + 8 * NO_SETMTX_SLOT) : 0;
    if (!NO_IN_IMAGE(fn)) return false;
    *own = NOB.warp_base && fn != NOB.warp_base;
    SwArgs x;
    memset(&x, 0, sizeof x);
    x.x[0] = a, x.x[1] = 0, x.x[2] = 1, x.x[3] = 1, x.nx = 4;
    x.x_scratch = 2, x.scratch = 64, x.init = m, x.init_len = 48;  // (x1 = the Matrix34f in the scratch)
    SwRet r;
    return NOB.api->vcall(NULL, a, NO_SETMTX_SLOT, &x, &r);
}
// (s289 objs parts 3-4) the AS actor a plays in slot 0, bank 0 (PR playing: its AS manager = *(a +0x560), objs_as_in_slot(mgr, 0,
// 0) -> the AS there, its name char* at +8). out gets NET_OBJS_AS bytes ("" none / not known / a name not plain letters); -> out
// (s291 horsefix) no_playing_in: the same in slot `slot` (nethorsecoat reads his horse's slot 4: its Collar AS)
// (s291 coat) no_playing_at: AnmTarget `target`, slot `slot` (sora_net_objs_core.h no_as_in_slot_args: the call is (mgr, TARGET,
// SLOT); horsefix had them swapped). no_playing_in = target 0 (the Horse's Collar AS: target 0, slot 4); slot 0 = the same call
static const char *no_playing_at(uint64_t a, int target, int slot, char out[NET_OBJS_AS]) {
    out[0] = 0;
    if (!NOB.as_ok || !NOB.api->in_frame()) return out;
    uint64_t mgr = no_rd64(a + 0x560);
    if (!NO_VALID(mgr)) return out;
    SwArgs x;
    memset(&x, 0, sizeof x);
    no_as_in_slot_args(x.x, mgr, target, slot), x.nx = 6;  // (x3..x5 0, as the PR's call())
    SwRet r = {0};
    if (!NOB.api->call(NULL, NOB.as_in_slot, &x, &r) || !NO_VALID(r.x0)) return out;
    uint64_t nm = no_rd64(r.x0 + 8);
    char b[NET_OBJS_AS] = {0};
    size_t ok = NO_VALID(nm) ? NOB.api->readable(nm, NET_OBJS_AS - 1) : 0;
    if (ok) memcpy(b, (const void *)(uintptr_t)nm, ok);
    for (int k = 0; k < NET_OBJS_AS - 1 && b[k]; k++)
        if ((uint8_t)b[k] < 0x20 || (uint8_t)b[k] > 0x7e) return out;
    if (memchr(b, 0, sizeof b) == NULL) return out;  // (never: b ends in 0)
    memcpy(out, b, NET_OBJS_AS);
    out[NET_OBJS_AS - 1] = 0;
    return out;
}
static const char *no_playing_in(uint64_t a, int slot, char out[NET_OBJS_AS]) { return no_playing_at(a, 0, slot, out); }
static const char *no_playing(uint64_t a, char out[NET_OBJS_AS]) { return no_playing_at(a, 0, 0, out); }
// (s289 objs parts 3-4) play AS `name` on actor a in slot 0, bank 0 (PR play_as: a fake AI action {0, actor}, the name, w2 = again
// ? 0 : 1 (1 = not again when it plays it already), the slot, the bank, s0 the blend, -1 = the AS's own); -> it plays it now
// (s291 horsefix) no_play_as_in: the same in slot `slot` (x3; bank 0), checked in that slot (nethorsecoat: slot 4)
// (s291 coat) the call is (action, name, w2, w3 = TARGET, w4 = SLOT) (no_play_as_args; horsefix put the slot in w3 = target 4:
// nothing played). no_play_as_in = target 0, `slot` (x4); slot 0 = the same call as before
static bool no_play_as_in(uint64_t a, const char *name, bool again, int slot) {
    if (!NOB.as_ok || !name || !name[0] || !NOB.api->in_frame()) return false;
    uint64_t fake[2] = {0, a};
    SwArgs x;
    memset(&x, 0, sizeof x);
    no_play_as_args(x.x, (uint64_t)(uintptr_t)fake, (uint64_t)(uintptr_t)name, again, 0, slot), x.nx = 7;
    float blend = -1.0f;
    memcpy(&x.s[0], &blend, 4), x.nv = 1;
    SwRet r = {0};
    if (!NOB.api->call(NULL, NOB.play_as, &x, &r)) return false;
    char now[NET_OBJS_AS];
    return !strcmp(no_playing_at(a, 0, slot, now), name);
}
static bool no_play_as(uint64_t a, const char *name, bool again) { return no_play_as_in(a, name, again, 0); }

// ---- netbump ----
static void net_bump_frame(bool driven, const float L[3], uint64_t now) {
    static float last[3];
    static bool have_last;
    static uint64_t fast_until, log_at;
    static int was = -1;
    if (!g_net_bump || !driven || !L) { have_last = false, was = -1; return; }
    no_bind();
    if (!NOB.move || !NOB.state_word || !NOB.ragdoll) return;  // (an exe without them: no bump)
    float mv = have_last ? no_d3(L, last) : 0;
    memcpy(last, L, sizeof last), have_last = true;
    if (mv > NB_FAST) fast_until = now + NB_FAST_MS;
    float at[NET_MAX_PEERS][3];
    uint8_t id[NET_MAX_PEERS];
    const char *nm[NET_MAX_PEERS];
    int n = 0;
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        const NetPeerSlot *sl = &NET.slot[k];
        if (!sl->used || sl->hidden || sl->p.st != PEER_LIVE) continue;
        PeerPose q = peer_sample(&sl->p, now);  // (where his Sora is drawn: NET_INTERP_MS behind his SNAPs)
        if (!q.valid || (q.flags & (NF_LOADING | NF_DEAD | NF_DOWNED))) continue;
        if (nh_rides(k, now)) {  // (s289 objs part 4: a friend who rides is not bumped into, PR players_collide; his horse records)
            static bool said[NET_MAX_PEERS];
            float dx = L[0] - q.feet[0], dz = L[2] - q.feet[2];
            bool close_by = dx * dx + dz * dz < 4 * NB_R * NB_R;  // (not `near`: a Windows macro)
            if (close_by) NOB.st.riders++;
            if (g_net_bump_log && close_by && !said[k]) LOG("[net] bump: %s rides: not bumped into\n", sl->p.name[0] ? sl->p.name : "a friend");
            said[k] = close_by ? true : said[k] && dx * dx + dz * dz < 16 * NB_R * NB_R;
            continue;
        }
        memcpy(at[n], q.feet, sizeof at[n]), id[n] = sl->p.player, nm[n] = sl->p.name[0] ? sl->p.name : "a friend", n++;
    }
    float push[2];
    if (!n || !nb_push(L, (const float (*)[3])at, id, n, NET.me, push)) { was = -1; return; }
    int why = nb_why(NET.downed, NET.dead, NET.loading, NOB.state_word(), NOB.ragdoll(), now < fast_until);
    if (why != NBW_OK) {
        if (g_net_bump_log && why != was) LOG("[net] bump: a friend is inside our Link's 0.7 m, no push while %s\n", NBW_TXT[why]);
        was = why;
        return;
    }
    NOB.move(push[0], 0, push[1]);
    NOB.st.bumps++;
    if (g_net_bump_log && (was != NBW_OK || now >= log_at)) {
        float d = sqrtf((L[0] - at[0][0]) * (L[0] - at[0][0]) + (L[2] - at[0][2]) * (L[2] - at[0][2]));
        LOG("[net] bump: Link pushed %.2f m (x %+.2f, z %+.2f) out of %s's Sora (%.2f m apart, %u pushes so far)\n", sqrtf(push[0] * push[0] + push[1] * push[1]),
            push[0], push[1], nm[0], d, NOB.st.bumps);
        log_at = now + 2000;
    }
    was = NBW_OK;
}

// ---- netobjs: breaks ----
static void no_send(const uint8_t *b, int n) {
    if (n <= 0) return;
    NetAddr d[NET_DESTS_MAX];
    net_send_all(d, 0, b, n);  // (Melty only: the room hands it to everyone; d unused there)
}
static void no_tell(int op, const NwEnt *w) {
    if (!NOB.room || !netslot_count(NET.slot, NET_MAX_PEERS)) return;
    NetObjEv e = {.op = (uint8_t)op, .kind = w->kind, .h = w->h, .pos = {w->pos[0], w->pos[1], w->pos[2]}, .dir = {w->dir[0], w->dir[1]}};
    uint8_t b[NET_OBJEV_LEN];
    int n = net_pack_objev(NET.me, ++NOB.seq, &e, b, (int)sizeof b);
    if (n <= 0) return;
    no_send(b, n);
    if (op == NOE_BREAK) NOB.st.tx_ev++;
    else NOB.st.tx_told++;
}
// break our copy of one on the room's list (BotW's own step: its drops, effects, a tree's fall)
static void no_apply(NoNear *e, const NwEnt *w, const float L[3], uint64_t now, const char *why) {
    e->enforce_ms = now + NO_ENFORCE_MS;
    if (!no_alive(e->actor, e->unit, e->id)) return;
    if (no_d3(e->pos, w->pos) > NO_POS_M) {  // (the same HashId elsewhere: not this one)
        NOB.st.wrong_pos++;
        if (g_net_objs_log) LOG("[net] objs: %s %#x is %.1f m from where his stood: not it, left alone\n", e->name, e->h, no_d3(e->pos, w->pos));
        return;
    }
    net_loot_hush(e->pos, now);  // (s289 loot: what BotW drops from this break here duplicates his shared drops: removed; netloot 0 = nothing)
    if (NOB.wake && no_rd8(e->actor + 0x64) != 1) no_call(NOB.wake, e->actor, NULL);  // (asleep: a life of 0 does nothing until woken; PR)
    int life = e->life > 0 ? e->life : 1, r;
    if (e->kind == NOK_TREE) {
        float dx = w->dir[0] / 127.f, dz = w->dir[1] / 127.f;
        if (!w->dir[0] && !w->dir[1]) dx = e->pos[0] - L[0], dz = e->pos[2] - L[2];  // (not known: away from our Sora)
        r = NOB.tree_hit ? NOB.tree_hit(e->actor, life, dx, dz) : -1;
    } else r = NOB.brk_hit ? NOB.brk_hit(e->actor, life) : -1;
    NOB.st.applied++;
    if (g_net_objs_log) LOG("[net] objs: %s %#x broken here too (%s): life %d -> %d%s\n", e->name, e->h, why, life, r, r < 0 ? " (REFUSED by the exe)" : "");
}
static NoNear *no_near_of(uint32_t h) {
    for (int i = 0; i < NOB.nnr; i++)
        if (NOB.nr[i].h == h) return &NOB.nr[i];
    return NULL;
}
static void no_near_see(uint64_t a, int kind, const float pos[3], int32_t life, const char *name, const float L[3], uint64_t now) {
    uint32_t h = no_hid(a);
    if (!h) return;
    NoNear *e = no_near_of(h);
    if (!e) {
        if (NOB.nnr >= NO_NEAR_N) return;
        e = &NOB.nr[NOB.nnr++];
        memset(e, 0, sizeof *e);
        e->h = h;
    }
    e->kind = (uint8_t)kind, e->seen_now = true, e->actor = a, e->unit = no_rd64(a + 0x68), e->id = no_rd32(a + 0x60), e->life = life;
    memcpy(e->pos, pos, sizeof e->pos), memcpy(e->link_at, L, sizeof e->link_at), e->seen_ms = now;
    snprintf(e->name, sizeof e->name, "%s", name);
    int w = nw_find(&NOB.world, h);
    if (w >= 0 && now >= e->enforce_ms) {
        if (e->enforce_ms) NOB.st.enforced++;
        no_apply(e, &NOB.world.e[w], L, now, e->enforce_ms ? "again: it still stands" : "on the room's list, near now");
    }
}
static void no_breaks_scan(const float L[3], uint64_t now) {
    static LinkposBreakable b[64];
    static LinkposTree t[48];
    static LinkposTrunk tr[24];
    int nb = NOB.brk ? NOB.brk(b, 64) : 0, nt = NOB.trees ? NOB.trees(t, 48) : 0, ntr = NOB.trunks ? NOB.trunks(tr, 24) : 0;
    for (int i = 0; i < NOB.nnr; i++) NOB.nr[i].seen_now = false;
    for (int i = 0; i < nb; i++) no_near_see(b[i].actor, NOK_BREAK, b[i].pos, b[i].life, b[i].name, L, now);
    for (int i = 0; i < nt; i++) no_near_see(t[i].actor, NOK_TREE, t[i].pos, t[i].life, t[i].name, L, now);
    for (int i = 0; i < NOB.nnr; i++) {
        NoNear *e = &NOB.nr[i];
        if (e->seen_now) continue;
        bool gone = !no_alive(e->actor, e->unit, e->id);
        if (!gone && now - e->seen_ms <= NO_FORGET_MS) continue;  // (not drawn now: the camera, a wall; still watched a while)
        if (gone) {
            bool trunk = false;
            float fall[2] = {0, 0};
            for (int j = 0; j < ntr && e->kind == NOK_TREE && !trunk; j++) {
                float dx = tr[j].pos[0] - e->pos[0], dz = tr[j].pos[2] - e->pos[2];
                if (tr[j].age_ms < NO_TRUNK_FRESH && dx * dx + dz * dz < NO_TRUNK_FOOT * NO_TRUNK_FOOT) trunk = true, fall[0] = tr[j].axis[0], fall[1] = tr[j].axis[2];
            }
            float dl = no_d3(L, e->pos), tv = no_d3(L, e->link_at);
            if (no_broke(NET.loading, e->kind == NOK_TREE, dl, tv, trunk) && nf_recent(&NOB.followed, e->h, now)) {
                // (s289 objs fix: it went while we took it to a friend's place, or just after: not our break, the room is not told)
                NOB.st.follow_broke++;
                net_loot_broke_followed(e->pos, now);  // (s289 loot: what it drops here duplicates his break's: removed; netloot 0 = nothing)
                if (g_net_objs_log) LOG("[net] objs: %s %#x broke while following a friend's (%.1f m from Link): not told\n", e->name, e->h, dl);
            } else if (no_broke(NET.loading, e->kind == NOK_TREE, dl, tv, trunk)) {
                NwEnt w = {.h = e->h, .kind = e->kind, .pos = {e->pos[0], e->pos[1], e->pos[2]}};
                no_dir_enc(fall[0], fall[1], w.dir);
                int added = nw_add(&NOB.world, &w, now);
                if (added == 1) NOB.st.broke_here++, no_tell(NOE_BREAK, &w);
                if (added == 1) net_loot_broke_here(e->pos, now);  // (s289 loot: its drops here are ours, shared, rupees too; netloot 0 = nothing)
                if (g_net_objs_log && added == 1) LOG("[net] objs: %s %#x broke here (%.1f m from Link): told the room\n", e->name, e->h, dl);
                else if (g_net_objs_log && added < 0) LOG("[net] objs: %s %#x broke here: the room's list is FULL (%d)\n", e->name, e->h, NW_MAX);
            } else if (g_net_objs_log)
                LOG("[net] objs: %s %#x went, not a break here (%.1f m from Link, Link went %.1f m%s)\n", e->name, e->h, dl, tv,
                    e->kind == NOK_TREE && !trunk ? ", no fresh trunk at its foot" : "");
        }
        NOB.nr[i--] = NOB.nr[--NOB.nnr];
    }
}

// ---- netobjs: moves ----
static NoTrk *no_trk_of(uint32_t h) {
    for (int i = 0; i < NO_TRK_N; i++)
        if (NOB.tk[i].h == h) return &NOB.tk[i];
    return NULL;
}
static NoTrk *no_trk_add(uint32_t h, uint64_t a, uint64_t unit, uint32_t id, uint64_t now) {
    NoTrk *t = no_trk_of(h);
    if (t) { t->actor = a, t->unit = unit, t->id = id, t->touch_ms = now; return t; }
    for (int i = 0; i < NO_TRK_N && !t; i++)
        if (!NOB.tk[i].h) t = &NOB.tk[i];
    for (int i = 0; i < NO_TRK_N && !t; i++)  // (full: the one untouched longest that we do not follow)
        if (NOB.tk[i].o.hold <= 0 && !NOB.tk[i].o.sending && (!t || NOB.tk[i].touch_ms < t->touch_ms)) t = &NOB.tk[i];
    if (!t) return NULL;
    memset(t, 0, sizeof *t);
    t->h = h, t->actor = a, t->unit = unit, t->id = id, t->touch_ms = now;
    t->fragile = no_fragile(a);
    const char *nm = NOB.api ? NOB.api->actor_name(a) : NULL;
    snprintf(t->name, sizeof t->name, "%s", nm && nm[0] ? nm : "?");
    return t;
}
// (s289 objs fix) one follow over (the hold ran out / ours went): the lab's numbers, one line (netobjslog)
static void no_fol_end(NoTrk *t, const char *why) {
    if (g_net_objs_log && t->fol_frames)
        LOG("[net] objs: %s %#x: following %s's %s after %d frames%s: %d records of his, his biggest step %.2f m; ours moved at most "
            "%.3f m in a frame, was %.2f m off his place at most and %.2f m over it at most\n", t->name, t->h, no_who(t->o.his_id), why,
            t->fol_frames, no_put_kind(t) ? " (a breakable: not through its body)" : t->fragile ? " (a breakable: through its body, netobjsbrkfol 1)" : "", t->fol_recs, t->fol_rec_jump, t->fol_step, t->fol_off,
            t->fol_rise);
    t->fol_frames = t->fol_recs = 0, t->fol_rec_jump = t->fol_step = t->fol_off = t->fol_rise = 0;
    t->fol_prev_ok = t->his_prev_ok = false, t->followed_said = false;
}
// mv = netobjs' moves (bodied map objects), mech = netmovers (the level's mechanisms, s289 objs part 3): one walk for both; a
// mechanism (nm_kind) is the movers' while netmovers is on, never an object then
static void no_actors_scan(const float L[3], uint64_t now, bool mv, bool mech) {
    uint64_t la = no_link_actor();
    int n = la ? no_all_actors(no_rd64(la + 0x68), g_no_act, NO_MAX_ACTORS) : 0;
    struct { int i; float d; } nearest[NO_TRACK];
    int nn = 0;
    if (mv) NOB.nkn = 0;
    for (int i = 0; i < n && (mech || NOB.nkn < NO_KNOWN_N); i++) {
        uint64_t a = g_no_act[i];
        if (a == la || no_rd8(a + 0x64) == 3) continue;
        const char *an = NOB.api->actor_name(a);
        if (mech && nm_kind(an)) { nm_scan_see(a, an, L); continue; }
        if (!mv || NOB.nkn >= NO_KNOWN_N || !no_kind(an) || !no_has_body(a)) continue;
        uint32_t h = no_hid(a);
        float m[12];
        if (!h || !no_mtx(a, m)) continue;
        float p[3] = {m[3], m[7], m[11]}, d = no_d3(L, p);
        if (!(d < NO_FAR)) continue;
        NoKnown *k = &NOB.kn[NOB.nkn++];
        k->h = h, k->actor = a, k->unit = no_rd64(a + 0x68), k->id = no_rd32(a + 0x60);
        if (!(d < NO_NEAR)) continue;
        int at = nn < NO_TRACK ? nn++ : (nearest[NO_TRACK - 1].d > d ? NO_TRACK - 1 : -1);
        if (at < 0) continue;
        while (at > 0 && nearest[at - 1].d > d) nearest[at] = nearest[at - 1], at--;
        nearest[at].i = NOB.nkn - 1, nearest[at].d = d;
    }
    if (mech) nm_scan_end(L);
    if (!mv) return;
    for (int j = 0; j < nn; j++) {
        const NoKnown *k = &NOB.kn[nearest[j].i];
        no_trk_add(k->h, k->actor, k->unit, k->id, now);
    }
    for (int i = 0; i < NO_TRK_N; i++) {  // (gone, or not near / not his for a while: forgotten)
        NoTrk *t = &NOB.tk[i];
        if (!t->h) continue;
        bool alive = no_alive(t->actor, t->unit, t->id);
        if (!alive || (t->o.hold <= 0 && !t->o.sending && now - t->touch_ms > NO_TRK_KEEP_MS)) {
            if (t->fol_frames) no_fol_end(t, alive ? "over: forgotten" : "over: it went (deleted here)");  // (s289 objs fix)
            memset(t, 0, sizeof *t);
        }
    }
}
static void no_flush(NetObjRec *r, int *n, int *pk) {
    if (!*n) return;
    if (*pk >= NO_PKTS_PER_TICK) { NOB.st.tick_cap++, *n = 0; return; }
    uint8_t b[NET_OBJS_PKT];
    int len = net_pack_objs(NET.me, ++NOB.seq, r, *n, b, (int)sizeof b);
    if (len > 0) no_send(b, len), NOB.st.tx_pkt++, NOB.st.tx_rec += (uint32_t)*n, (*pk)++;
    *n = 0;
}
// (s289 objs fix 3) one 20 Hz sample of a breakable settling after our warp-put (t->settle.on): -> true = its moves are still
// the put's (quiet: nothing sent); false = ours again from now (the usual rule)
static bool no_settle_sample(NoTrk *t, const float p[3], float moved, const float *L, uint64_t now) {
    int s = nos_sample(&t->settle, now, moved, L ? no_d3(L, p) : NAN);
    t->touch_ms = now;  // (watched while it settles, wherever it rolls)
    float to[3] = {t->put_m[3], t->put_m[7], t->put_m[11]}, d = no_d3(p, to);
    if (s == NOS_ON) { t->o.quiet_until = now + NO_QUIET_MS; return true; }
    if (s == NOS_RESTS) {
        t->o.quiet_until = now + NO_QUIET_MS;  // (its rest is the put's too: not sent)
        int pick = nos_pick(d, t->put_rest, t->reputs);
        if (pick == 1) {  // (his rests there: put once more, no_puts, when clear)
            t->put_wait = true, t->put_until = now + NO_PUT_WAIT_MS, t->put_said = false, t->reputs++, NOB.st.settle_reput++;
            if (g_net_objs_log) LOG("[net] objs: %s %#x rests %.2f m from %s's rest after the put: put there once more\n", t->name, t->h, d, no_who(t->o.his_id));
        } else {
            if (pick == 0) NOB.st.settle_near++;
            else NOB.st.settle_kept++;
            if (g_net_objs_log)
                LOG("[net] objs: %s %#x rests %.2f m from %s's place after the put, at %.1f %.1f %.1f: kept there%s\n", t->name, t->h, d,
                    no_who(t->o.his_id), p[0], p[1], p[2], pick == 0 ? "" : t->put_rest ? " (put twice already), not sent" : " (his last record was no rest: his may be gone), not sent");
        }
        return true;
    }
    t->o.quiet_until = 0;  // (its moves are ours from now: it moves = sent)
    if (s == NOS_LINK) NOB.st.settle_link++;
    else NOB.st.settle_late++;
    if (g_net_objs_log)
        LOG("[net] objs: %s %#x %.2f m from %s's place after the put: %s: its moves are ours again\n", t->name, t->h, d, no_who(t->o.his_id),
            s == NOS_LINK ? "our Link came to it" : "still moving 8 s after the put");
    return false;
}
// our objects' 20 Hz samples into r (the send tick's packets: no_send_tick)
static void no_moves_sample(NetObjRec *r, int *pn, int *pk, bool anyone, const float *L, uint64_t now) {
    int n = *pn;
    for (int i = 0; i < NO_TRK_N; i++) {
        NoTrk *t = &NOB.tk[i];
        float m[12];
        if (!t->h || !no_alive(t->actor, t->unit, t->id) || !no_mtx(t->actor, m)) continue;
        float p[3] = {m[3], m[7], m[11]}, moved = t->sampled ? no_d3(p, t->last) : 0;
        memcpy(t->last, p, sizeof t->last), t->sampled = true;
        if (t->settle.on && no_settle_sample(t, p, moved, L, now)) continue;  // (s289 objs fix 3: the put's, not ours)
        bool was = t->o.sending;
        if (!no_ours(&t->o, now, moved, netshots_friend_holds(t->h) != 0) || !anyone) continue;
        if (g_net_objs_log && !was && t->o.sending) LOG("[net] objs: %#x moves here (%.2f m in 50 ms) at %.1f %.1f %.1f: ours, sent\n", t->h, moved, p[0], p[1], p[2]);
        if (g_net_objs_log && was && !t->o.sending) LOG("[net] objs: %#x at rest here at %.1f %.1f %.1f: sent its last\n", t->h, p[0], p[1], p[2]);
        memset(&r[n], 0, sizeof r[n]);
        r[n].kind = NOR_OBJ, r[n].flags = t->o.sending ? 0 : NORF_REST, r[n].h = t->h, r[n].extra = 0;
        memcpy(r[n].m, m, sizeof r[n].m);
        if (++n == NET_OBJS_N) no_flush(r, &n, pk);
    }
    *pn = n;
}
static void no_moves_follow(uint64_t now) {
    for (int i = 0; i < NO_TRK_N; i++) {
        NoTrk *t = &NOB.tk[i];
        if (!t->h || t->o.hold <= 0) continue;
        if (!no_alive(t->actor, t->unit, t->id)) { t->o.hold = 0, no_fol_end(t, "over: it went (deleted here)"); continue; }
        int step = no_follow(&t->o, now);
        nf_note(&NOB.followed, t->h, now);  // (s289 objs fix: should it go now or within 1 s, it was no break of ours)
        float m[12], hp[3] = {t->o.his_m[3], t->o.his_m[7], t->o.his_m[11]};
        bool have = no_mtx(t->actor, m);
        if (have) {  // (the lab's numbers)
            float p[3] = {m[3], m[7], m[11]}, off = no_d3(p, hp), rise = p[1] - hp[1];
            if (t->fol_prev_ok && no_d3(p, t->fol_prev) > t->fol_step) t->fol_step = no_d3(p, t->fol_prev);
            memcpy(t->fol_prev, p, sizeof t->fol_prev), t->fol_prev_ok = true;
            if (off > t->fol_off) t->fol_off = off;
            if (rise > t->fol_rise) t->fol_rise = rise;
        }
        t->fol_frames++;
        if (no_put_kind(t)) {  // (s289 objs fix: a breakable is not taken through its body; put at his place once his rests)
            // (fix 2: a warp to do, no_puts, at once when clear; fix 3: while one settles after a put, the end of the hold
            // puts nothing: the settle decides; a fresh rest of his is a new place)
            bool rest = t->his_rest;
            if ((rest || (step == 2 && !t->settle.on)) && have && no_put_off(m, t->o.his_m)) {
                if (!t->put_wait) t->put_until = now + NO_PUT_WAIT_MS, t->put_said = false;
                t->put_rest = rest || (t->put_wait && t->put_rest);
                if (rest) t->reputs = 0, t->settle.on = false;
                memcpy(t->put_m, t->o.his_m, sizeof t->put_m), t->put_wait = true;
            }
            t->his_rest = false;
            if (step == 2) no_fol_end(t, "over");
            continue;
        }
        if (no_rd8(t->actor + 0x64) != 1) no_call(NOB.wake, t->actor, NULL);  // (asleep: its body takes nothing; PR)
        if (step == 2 && have) {  // (the last: still apart, caught on something on the way = put there; PR)
            float p[3] = {m[3], m[7], m[11]};
            if (no_d3(p, hp) > NO_SNAP_M) {
                no_call(NOB.set_mtx, t->actor, t->o.his_m), NOB.st.put_there++;
                if (g_net_objs_log) LOG("[net] objs: %#x %.1f m off his at rest: put there\n", t->h, no_d3(p, hp));
            }
        }
        uint64_t body = no_rd64(t->actor + 0x190);
        if (body && no_has_body(t->actor)) no_call(NOB.body_to, body, t->o.his_m);
        if (step == 2) no_fol_end(t, "over");
    }
}
// (s289 objs fix 2) every frame, after no_moves_follow: the breakables to put at his place (BotW's warp, no_warp) once
// nothing stands there (no_put_block: our Link, the bodied map objects of the last walk); 0.5 s later one line: did it stay?
static void no_puts(const float L[3], uint64_t now) {
    for (int i = 0; i < NO_TRK_N; i++) {
        NoTrk *t = &NOB.tk[i];
        if (!t->h) continue;
        float m[12];
        if (t->put_check_at && now >= t->put_check_at) {
            t->put_check_at = 0;
            if (no_mtx(t->actor, m)) {
                float p[3] = {m[3], m[7], m[11]}, to[3] = {t->put_m[3], t->put_m[7], t->put_m[11]}, d = no_d3(p, to);
                bool stayed = d < NO_PUT_STAYED_M;
                if (stayed) NOB.st.put_stayed++;
                else NOB.st.put_back++;
                if (g_net_objs_log) LOG("[net] objs: %s %#x 0.5 s after the warp: %.2f m from his place at %.2f %.2f %.2f (%s)\n", t->name, t->h, d,
                                        p[0], p[1], p[2], stayed ? "it stayed" : "NOT THERE: moved back / away");
            }
        }
        if (!t->put_wait) continue;
        if (!no_alive(t->actor, t->unit, t->id) || !no_mtx(t->actor, m) || !no_put_off(m, t->put_m)) { t->put_wait = false; continue; }  // (gone / there)
        if (t->o.sending) {  // (ours moves now, by our hand: ours, not put)
            t->put_wait = false, NOB.st.put_ours++;
            if (g_net_objs_log) LOG("[net] objs: %s %#x moves here by our hand: not put at %s's place\n", t->name, t->h, no_who(t->o.his_id));
            continue;
        }
        static float at[NO_KNOWN_N][3];  // (game thread only)
        static uint64_t who[NO_KNOWN_N];
        float to[3] = {t->put_m[3], t->put_m[7], t->put_m[11]};
        int n = 0;
        for (int j = 0; j < NOB.nkn; j++) {
            float km[12];
            const NoKnown *k = &NOB.kn[j];
            if (k->actor == t->actor || k->h == t->h || !no_alive(k->actor, k->unit, k->id) || !no_mtx(k->actor, km)) continue;
            at[n][0] = km[3], at[n][1] = km[7], at[n][2] = km[11], who[n++] = k->actor;
        }
        int b = no_put_block(to, L, (const float (*)[3])at, n);
        if (b >= 0) {
            const char *by = b == 0 ? "our Link" : NOB.api->actor_name(who[b - 1]);
            if (now >= t->put_until) {
                t->put_wait = false, NOB.st.put_gave_up++;
                if (g_net_objs_log) LOG("[net] objs: %s %#x NOT put at %s's place: %s stood there for %d s\n", t->name, t->h, no_who(t->o.his_id), by && by[0] ? by : "?", NO_PUT_WAIT_MS / 1000);
            } else if (!t->put_said) {
                t->put_said = true, NOB.st.put_waits++;
                if (g_net_objs_log) LOG("[net] objs: %s %#x waits to be put at %s's place: %s stands there\n", t->name, t->h, no_who(t->o.his_id), by && by[0] ? by : "?");
            }
            continue;
        }
        if (no_rd8(t->actor + 0x64) != 1) {  // (asleep: woken first, as the PR; then maybe on another proc unit: watched there)
            no_call(NOB.wake, t->actor, NULL);
            uint64_t u = no_rd64(t->actor + 0x68);
            if (NO_VALID(u) && no_rd64(u + 0x48) == t->actor && no_rd32(t->actor + 0x60) == t->id) t->unit = u;
        }
        bool own = false, ok = no_warp(t->actor, t->put_m, &own);
        nf_note(&NOB.followed, t->h, now);  // (should the warp break it, within 1 s: not our break, not told)
        t->o.quiet_until = now + NO_QUIET_MS;  // (the jump is his, not ours to send)
        t->put_wait = false;
        float p[3] = {m[3], m[7], m[11]};
        if (ok) NOB.st.rest_puts++, t->put_check_at = now + NO_PUT_CHECK_MS, nos_start(&t->settle, now);  // (fix 3: it settles)
        else NOB.st.put_refused++;
        if (g_net_objs_log)
            LOG("[net] objs: %s %#x (a breakable) %s at %s's place, his at rest (%.2f m off; BotW's warp, %s setMtx)\n", t->name, t->h,
                ok ? "put" : "NOT put (the call was refused)", no_who(t->o.his_id), no_d3(p, to), own ? "its class's own" : "Actor's");
    }
}

// ---- (s289 objs parts 3-4) the level's mechanisms, the horse a friend rides ----
#include "sora_net_movers.h"
#include "sora_net_horse.h"

// the 20 Hz send: the horse our Link rides first, then our mechanisms, then our objects; at most NO_PKTS_PER_TICK packets
// (records past the cap are dropped this tick and counted: the next tick has newer ones)
static void no_send_tick(const float L[3], uint64_t now, bool mv, bool mech, bool ride) {
    NetObjRec r[NET_OBJS_N];
    int n = 0, pk = 0;
    bool anyone = NOB.room && netslot_count(NET.slot, NET_MAX_PEERS) > 0;
    if (ride) nh_sample(r, &n, &pk, anyone, now);
    if (mech) nm_sample(r, &n, &pk, L, anyone, now);
    if (mv) no_moves_sample(r, &n, &pk, anyone, L, now);
    no_flush(r, &n, &pk);
}

// ---- receive (net_recv_all: after net_unpack_ex) ----
static void net_objs_recv(int k, int type, const uint8_t *buf, int n, uint64_t now) {
    // (s289 objs parts 3-4) per kind: objects + breaks need netobjs, mechanisms netmovers, a friend's horse nethorse or netbump
    bool objs = g_net_objs != 0, mv = g_net_movers != 0, horse = g_net_horse || g_net_bump;
    if (!objs && (type == NET_OBJEV || (!mv && !horse))) { NOB.st.rx_off++; return; }  // (keys 0: dropped, nothing moved = as before)
    if (k < 0 || k >= NET_MAX_PEERS || NET.slot[k].hidden) { NOB.st.rx_hidden++; return; }  // (on another map: not ours to show)
    uint8_t pl = 0;
    uint16_t sq = 0;
    const char *who = NET.slot[k].p.name[0] ? NET.slot[k].p.name : "a friend";
    if (type == NET_OBJEV) {
        NetObjEv e;
        if (!net_unpack_objev(buf, n, &pl, &sq, &e)) { NOB.st.rx_bad++; return; }
        NOB.st.rx_ev++;
        NwEnt w = {.h = e.h, .kind = e.kind, .dir = {e.dir[0], e.dir[1]}, .pos = {e.pos[0], e.pos[1], e.pos[2]}};
        int added = nw_add(&NOB.world, &w, now);
        if (added != 1) return;  // (already on our list / full)
        NoNear *ne = no_near_of(e.h);
        if (g_net_objs_log) LOG("[net] objs: %s %s %#x (%s) at %.1f %.1f %.1f%s\n", who, e.op == NOE_BREAK ? "broke" : "told us of", e.h,
                                e.kind == NOK_TREE ? "a tree" : "a breakable", e.pos[0], e.pos[1], e.pos[2], ne ? "" : ": not near us, broken when it is");
        if (ne) {
            float L[3];
            if (NOB.link_pos && NOB.link_pos(L)) no_apply(ne, &NOB.world.e[nw_find(&NOB.world, e.h)], L, now, "his break");
        }
        return;
    }
    NetObjRec r[NET_OBJS_N];
    int nr = 0;
    if (!net_unpack_objs(buf, n, &pl, &sq, r, &nr)) { NOB.st.rx_bad++; return; }
    NOB.st.rx_pkt++, NOB.st.rx_rec += (uint32_t)nr;
    NOB.st.rx_unp += (uint32_t)(buf[NET_HDR] - nr);  // (s290: records net_unpack_objs dropped: HashId 0, a kind, a bad value, a non-rotation)
    bool fresh = no_rx_begin(&NOB.rx[k], NET.slot[k].joined_ms, sq);  // (s290 #4: kind 1 only; movers / horses as before)
    for (int i = 0; i < nr; i++) {
        if (r[i].kind == NOR_MOVER) { if (mv && NOB.movers_ok) nm_recv(k, &r[i], now); continue; }  // (s289 objs part 3)
        if (r[i].kind == NOR_HORSE) { if (horse) nh_recv(k, &r[i], now); continue; }              // (s289 objs part 4)
        if (!objs || !NOB.moves) continue;  // (kind 1, as before)
        if (!no_rx_rec(&NOB.rx[k], r[i].h, sq, fresh)) { NOB.st.rx_dup++; continue; }  // (s290 #4: his newer word of it came first)
        NoTrk *t = no_trk_of(r[i].h);
        if (!t)
            for (int j = 0; j < NOB.nkn && !t; j++)
                if (NOB.kn[j].h == r[i].h) t = no_trk_add(r[i].h, NOB.kn[j].actor, NOB.kn[j].unit, NOB.kn[j].id, now);
        if (!t) { NOB.st.no_actor++; continue; }  // (not loaded here / farther than NO_FAR from our Link)
        {  // (s290 #3, revfix F3: in his stream within 10 m of his last record that passed; a stream's first within NO_FAR of our copy)
            float cm[12], ours[3] = {0, 0, 0};
            bool have = no_mtx(t->actor, cm), stream = false;
            if (have) ours[0] = cm[3], ours[1] = cm[7], ours[2] = cm[11];
            if (!no_rec_take(&t->rxn, now, have ? ours : NULL, r[i].m, &stream)) {
                NOB.st.rx_far++;
                if (g_net_objs_log) LOG("[net] objs: %s's record of %#x is too far from %s: refused\n", who, r[i].h, stream ? "his last (a jump in his stream)" : "ours (or ours unreadable)");
                continue;
            }
        }
        t->touch_ms = now;
        bool held = netshots_friend_holds(r[i].h) != 0 || (r[i].flags & NORF_HELD);
        if (!no_his(&t->o, now, NET.me, NET.slot[k].p.player, held, r[i].m)) continue;  // (ours: we began first / the tie)
        NOB.st.follows++;
        if (r[i].flags & NORF_REST) t->his_rest = true;  // (s289 objs fix: his rests there now; a breakable is put there)
        else t->put_wait = false, t->settle.on = false;  // (fix 2: his moves again: no warp to his old place; his next rest; fix 3)
        float hp[3] = {r[i].m[3], r[i].m[7], r[i].m[11]};  // (the lab's numbers)
        if (t->his_prev_ok && no_d3(hp, t->his_prev) > t->fol_rec_jump) t->fol_rec_jump = no_d3(hp, t->his_prev);
        memcpy(t->his_prev, hp, sizeof t->his_prev), t->his_prev_ok = true, t->fol_recs++;
        if (g_net_objs_log && !t->followed_said)
            LOG("[net] objs: %s %#x moves in %s's world: %s\n", t->name, r[i].h, who, no_put_kind(t) ? "a breakable: ours stays, put there once his rests"
                                                                                     : t->fragile ? "a breakable, ours follows (netobjsbrkfol 1)" : "ours follows");
        t->followed_said = true;
    }
}

// ---- the frame (net_frame, every mod frame) ----
static void net_objs_frame(bool driven, const float link_pos[3], uint64_t now) {
    net_bump_frame(driven, link_pos, now);
    // (s289 objs part 4) puppets to delete (an earlier DLL copy's: looked for once per copy; ours once the key / the room
    // went) and requests still under way: here, every key 0 too (the calls need the frame). Nothing when nethorse never ran
    // in this process (no mapping: one OpenFileMapping per DLL copy).
    nh_old_copy_probe();
    if (NHS.sh && !NET.loading && (NHS.purge || ((!g_net_horse || !NOB.room) && nh_orphans_left(NHS.sh)))) {  // (nethorse 1 in a room: nh_frame sees to orphans; s291 review2 #1: a room over = here too)
        no_bind();
        if (NOB.horse_ok && NOB.api->in_frame()) {
            if (NHS.purge) NHS.purge = false, nh_purge();
            nh_orphans_update(now);
        }
    }
    bool ride = g_net_horse || g_net_bump;  // (part 4: the horse our Link rides goes to the room: nethorse's puppets, the bump's riding flag)
    if (!g_net_objs && !g_net_movers && !ride) return;  // (every key 0: nothing read, sent or moved)
    no_bind();
    bool room = NET.on && NET.melty && sora_melty_connected();
    if (NOB.room && !room) {  // (the room is over: its list with it)
        if (g_net_objs_log) LOG("[net] objs: left the room: its broken list (%d) forgotten\n", NOB.world.n);
        nw_clear(&NOB.world), memset(NOB.tk, 0, sizeof NOB.tk), NOB.nnr = 0, memset(NOB.rx, 0, sizeof NOB.rx);  // (s290: his seqs start over)
        nm_forget();                                                         // (part 3)
        memset(NHS.rx, 0, sizeof NHS.rx), NHS.riding = false, NHS.horse = 0;  // (part 4: his puppets go at the next frame)
        if (NHS.sh) NHS.purge = true;
    }
    NOB.room = room;
    if (NET.on && !NET.melty && !NOB.udp_said && (g_net_objs || g_net_movers || g_net_horse))
        NOB.udp_said = true, LOG("[net] objs: Melty rooms only: nothing sent on UDP\n");
    if (!room) return;
    nh_rx_prune();  // (s292 coopfix: a friend who left keeps no horse records / diag row)
    if (g_net_objs) {
        const NwEnt *w = nw_rotate(&NOB.world, now);
        if (w) no_tell(NOE_TOLD, w);
    }
    if (NET.loading && g_net_horse) NHS.saw_loading = true;  // (part 4: a load = the game's actors go)
    if (!driven || NET.loading || !link_pos) return;
    if (g_net_objs && now >= NOB.scan_at) NOB.scan_at = now + NO_SCAN_MS, no_breaks_scan(link_pos, now);
    bool mv = g_net_objs && NOB.moves, mech = g_net_movers && NOB.movers_ok;
    if ((mv || mech) && now >= NOB.act_at) NOB.act_at = now + NO_ACT_MS, no_actors_scan(link_pos, now, mv, mech);
    if (mv) no_moves_follow(now), no_puts(link_pos, now);  // (no_puts: s289 objs fix 2)
    if (mech) nm_drive(link_pos, now);                     // (part 3)
    if (g_net_horse) nh_frame(link_pos, now);              // (part 4)
    if (now >= NOB.sample_at) NOB.sample_at = now + NO_SAMPLE_MS, no_send_tick(link_pos, now, mv, mech, ride);
}

static void net_objs_diag(void) {
    no_bind();
    LOG("[net] objs: bump %s (%u pushes); objs %s, %s; room %s, broken list %d; watched %d near, %d known; sent %u state packets "
        "(%u records, %u held back by the %d-per-tick cap), %u breaks, %u re-told; received %u packets (%u records), %u breaks, "
        "%u bad, %u dropped (netobjs 0), %u from another map; broke here %u, applied %u (again %u, not it %u), follows %u, put there %u, "
        "his not here %u; went while following his (not told) %u, breakables put at his place at rest %u (BotW's warp; "
        "%u waited, %u not put: something stood there 10 s, %u refused, %u ours moved first; 0.5 s after: %u stayed, %u NOT there; "
        "settled after: %u near his place, %u put once more, %u kept away from it, %u still moving 8 s, %u our Link came); "
        "kind-1 records refused: %u dropped by unpack (not a rotation / bad value), %u too far from our copy (> %.0f m), %u older than one already heard (seq)\n",
        g_net_bump ? "on" : "OFF", NOB.st.bumps, g_net_objs ? "on" : "OFF", no_moves_txt(), NOB.room ? "yes" : "no", NOB.world.n, NOB.nnr,
        NOB.nkn, NOB.st.tx_pkt, NOB.st.tx_rec, NOB.st.tick_cap, NO_PKTS_PER_TICK, NOB.st.tx_ev, NOB.st.tx_told, NOB.st.rx_pkt, NOB.st.rx_rec,
        NOB.st.rx_ev, NOB.st.rx_bad, NOB.st.rx_off, NOB.st.rx_hidden, NOB.st.broke_here, NOB.st.applied, NOB.st.enforced, NOB.st.wrong_pos,
        NOB.st.follows, NOB.st.put_there, NOB.st.no_actor, NOB.st.follow_broke, NOB.st.rest_puts, NOB.st.put_waits, NOB.st.put_gave_up,
        NOB.st.put_refused, NOB.st.put_ours, NOB.st.put_stayed, NOB.st.put_back, NOB.st.settle_near, NOB.st.settle_reput, NOB.st.settle_kept,
        NOB.st.settle_late, NOB.st.settle_link, NOB.st.rx_unp, NOB.st.rx_far, (double)NO_REC_MAX_M, NOB.st.rx_dup);
    for (int i = 0; i < NO_TRK_N; i++) {  // (the lab: the same HashId in both games' lines = the same object, compare the places)
        const NoTrk *t = &NOB.tk[i];
        float m[12];
        if (!t->h || !no_mtx(t->actor, m)) continue;
        LOG("[net] objs:   tracked: %s %#x at %.2f %.2f %.2f (%s%s)\n", t->name, t->h, m[3], m[7], m[11],
            t->o.hold > 0 ? "his: following" : t->settle.on ? "settling after the put (not sent)" : t->o.sending ? "ours: sending" : "still",
            no_put_kind(t) ? "; a breakable: put there at rest" : t->fragile ? "; a breakable, through its body: netobjsbrkfol 1" : "");
    }
    float L[3];  // (the lab: the nearest watched breakables / trees, to walk to one)
    bool have_L = NOB.link_pos && NOB.link_pos(L);
    bool said[NO_NEAR_N] = {0};
    for (int k = 0; k < 6 && have_L; k++) {
        int best = -1;
        for (int i = 0; i < NOB.nnr; i++)
            if (!said[i] && (best < 0 || no_d3(L, NOB.nr[i].pos) < no_d3(L, NOB.nr[best].pos))) best = i;
        if (best < 0) break;
        said[best] = true;
        const NoNear *e = &NOB.nr[best];
        LOG("[net] objs:   near: %s %#x (%s, life %d) at %.1f %.1f %.1f, %.1f m%s\n", e->name, e->h, e->kind == NOK_TREE ? "tree" : "breakable", e->life,
            e->pos[0], e->pos[1], e->pos[2], no_d3(L, e->pos), nw_find(&NOB.world, e->h) >= 0 ? " (on the room's broken list)" : "");
    }
    nm_diag(have_L ? L : NULL);  // (s289 objs part 3)
    nh_diag();                   // (s289 objs part 4)
}

// live keys (one line when a value changes: a live rewrite re-applies every line)
static bool net_objs_live_set(const char *key, int v) {
    static int diag_v;
    if (NO_KEYIS(key, "netbump")) {
        if (g_net_bump == (v != 0)) return true;
        g_net_bump = v != 0;
        LOG("[net] bump %s\n", g_net_bump ? "on: our Link is pushed out of a friend's Sora (0.7 m), not out of a riding friend's (s289 objs part 4: "
                                            "while we ride, our horse goes to a Melty room for the same)"
                                          : "off (set netbump 0: players walk through each other, as before)");
    } else if (NO_KEYIS(key, "netbumplog")) {
        if (g_net_bump_log == (v != 0)) return true;
        g_net_bump_log = v != 0;
        LOG("[net] bump log %s\n", g_net_bump_log ? "on (a line per bump, at most one per 2 s; why none while a friend is inside)" : "off");
    } else if (NO_KEYIS(key, "netobjs")) {
        if (g_net_objs == (v != 0)) return true;
        g_net_objs = v != 0;
        no_bind();
        if (!g_net_objs) memset(NOB.tk, 0, sizeof NOB.tk), NOB.nnr = 0;  // (nothing followed any more; the room's list is kept)
        LOG("[net] objs %s\n", g_net_objs ? (NOB.moves ? "on: breaks + moves of map objects go to the room (Melty only)"
                                                       : "on: breaks of map objects go to the room (Melty only); moves stay off (see net diag)")
                                          : "off (set netobjs 0: crates / trees / boulders are each game's own, as before)");
        if (g_net_objs) LOG("[net] objs: %s\n", no_moves_txt());
    } else if (NO_KEYIS(key, "netobjslog")) {
        if (g_net_objs_log == (v != 0)) return true;
        g_net_objs_log = v != 0;
        LOG("[net] objs log %s\n", g_net_objs_log ? "on (a line per break here / theirs applied / follow / put there)" : "off");
    } else if (NO_KEYIS(key, "netobjsbrkfol")) {  // (s289 objs fix: lab only; fix 2: 15 letters at most)
        if (g_net_objs_brkfol == (v != 0)) return true;
        g_net_objs_brkfol = v != 0;
        LOG("[net] objs: netobjsbrkfol %d: breakables %s\n", g_net_objs_brkfol,
            g_net_objs_brkfol ? "taken through their body again, as before the s289 fix (lab: to measure why one broke; they may break, their breaks are not told)"
                              : "not taken through their body: put at the friend's place once his rests (the s289 fix, BotW's warp)");
    } else if (NO_KEYIS(key, "netobjsdiag")) {
        if (v && v != diag_v) net_objs_diag();
        diag_v = v;
    } else if (NO_KEYIS(key, "netmovers")) {  // (s289 objs part 3)
        if (g_net_movers == (v != 0)) return true;
        g_net_movers = v != 0;
        no_bind();
        if (!g_net_movers) nm_forget();  // (nothing driven any more: each game's own mechanisms, as before)
        if (!g_net_movers) LOG("[net] movers off (set netmovers 0: each game moves its own lifts / doors / gears, as before)\n");
        else if (NOB.movers_ok) LOG("[net] movers on: lifts / doors / gears / gates ... move the same in every world of a Melty room (netmoverride %d)\n", g_net_mover_ride);
        else LOG("[net] movers on, but %s\n", no_rows_txt("movers"));
    } else if (NO_KEYIS(key, "netmoverslog")) {
        int nv = v < 0 ? 0 : v > 2 ? 2 : v;
        if (g_net_movers_log == nv) return true;
        g_net_movers_log = nv;
        LOG("[net] movers log %d%s\n", nv, nv == 2 ? " (a line per one his / ours / our Link at it, and each one watched or no longer)"
                                       : nv ? " (a line per one that goes a friend's way / is ours / our Link is at)" : "");
    } else if (NO_KEYIS(key, "netmoverride")) {
        if (g_net_mover_ride == (v != 0)) return true;
        g_net_mover_ride = v != 0;
        LOG("[net] movers: netmoverride %d: %s\n", g_net_mover_ride, g_net_mover_ride
            ? "one our Link is at goes a friend's way in slow steps through its body (lab, LAB_STEPS E1)"
            : "a friend's moves of one our Link is at are not shown while he is there (default)");
    } else if (NO_KEYIS(key, "nethorse")) {  // (s289 objs part 4)
        if (g_net_horse == (v != 0)) return true;
        g_net_horse = v != 0;
        no_bind();
        if (!g_net_horse) {
            if (NHS.sh) NHS.purge = true;  // (his puppets go at the next frame; the bump still reads his riding flag)
            LOG("[net] horse off (set nethorse 0: a friend's horse is not shown, as before)%s\n", NHS.sh ? ": the puppets go" : "");
        } else {
            NHS.have_last = false, NHS.saw_loading = false;
            LOG("[net] horse on: a friend's horse shows under his Sora (Melty rooms only)\n");
            if (!nh_ready()) LOG("[net] horse: no puppets here (the line above says why): our horse still goes to the room while we ride\n");
        }
    } else if (NO_KEYIS(key, "nethorsecoat")) {  // (s291 horsefix)
        if (g_net_horse_coat == (v != 0)) return true;
        g_net_horse_coat = v != 0;
        if (!g_net_horse_coat) memset(NHS.coat, 0, sizeof NHS.coat), NHS.hcoat = 0;
        LOG("[net] horse coat %d%s\n", g_net_horse_coat, g_net_horse_coat
            ? " (our horse's coat goes with it; a friend's puppet plays the bake's Coat_<family>_<frame> AS in slot 4)"
            : " (off: no coat sent; a puppet that wears one keeps it until it is made again)");
    } else if (NO_KEYIS(key, "nethorsefar")) {  // (s292 coopfix)
        int nv = v < 0 ? 0 : v > 5000 ? 5000 : v;
        if (g_net_horse_far == nv) return true;
        g_net_horse_far = nv;
        memset(NHS.rm, 0, sizeof NHS.rm), memset(NHS.toofar, 0, sizeof NHS.toofar);
        if (nv) LOG("[net] horse: nethorsefar %d: no puppet of a friend's horse farther than %d m (back under %d m); one the game removed is asked for again after %d s, doubling up to %d s\n",
                    nv, nv, (int)(nv * NH_FAR_IN), NH_RM_WAIT_MS / 1000, NH_RM_WAIT_MAX_MS / 1000);
        else LOG("[net] horse: nethorsefar 0: puppets at any distance, made again at once when the game removes one (as before)\n");
    } else if (NO_KEYIS(key, "nethorselog")) {
        int nv = v < 0 ? 0 : v > 2 ? 2 : v;
        if (g_net_horse_log == nv) return true;
        g_net_horse_log = nv;
        LOG("[net] horse log %d%s\n", nv, nv == 2 ? " (also each AS played on a puppet)" : nv ? " (a line per puppet made / pooled / removed, rides / rides no more)" : "");
    } else return false;
    return true;
}


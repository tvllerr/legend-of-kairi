// sora_net_shots_core.h (s289 shots): the pure parts of `set netshots 1` / `set netrunes 1` (no game, no socket, no exe; host
// tested by runtime/test/netshots_test.c). After Savage Wind PR #294 "Online mode" by byluki (fixes by mstampfli), MIT:
// runtime/online_actors.c all_actors / linked / alive / actor_name_of / has_body and sample_shots' rules, ported to reads through
// a readable-check callback (the DLL passes rd_ok) instead of the PR's exe patch sites (arrow_move / remote_bomb_made).
//   walk      every actor the game has, from one actor's proc unit (BaseProcUnit list: +0x10 / +0x18 links, +0x40 id, +0x48 actor)
//   AS        the AS an actor plays in a slot / bank: a read-only copy of ASList's slot lookup (1.8.2 0x700138bee0 -> 0x70013a6f5c,
//             1.9.0 0x70014a0040 -> 0x70014bb0bc: the same code in both, read s289), so no guest call
//   local     which arrows / bombs / thrown weapon are HIS (an arrow on his bow, or the PR's check: first seen within NSHOT_NEAR_M
//             of Link while one of his AS channels plays a Bow AS now or in the last NSHOT_BOW_MS; else remembered as not his),
//             followed until gone / still; an arrow is sent once it flies (never while it waits on his string); a bomb's
//             "Explode" AS = it went off (sent NSHOT_BOOM_MS, played once)
//   remote    a friend's shots on a track NET_INTERP_MS behind (as his puppet): lerp + nlerp between samples, a short
//             extrapolation, hidden once his newest packet no longer lists them and the track has played out
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "sora_net_proto.h"

#define NSHOT_MAX NET_SHOTS_N   // shots followed / sent at once (PR ONLINE_SHOTS)
#define NSHOT_NEAR_M 4.0f       // an arrow first seen this close to Link while he uses a bow is his (PR)
#define NSHOT_BOW_MS 1000       // "uses a bow" = a Bow AS now or this long ago (PR)
#define NSHOT_NOT_HIS 64        // arrows remembered as not his (actor + id; PR)
#define NSHOT_STILL_M 0.01f     // moved less than this since the last look = still (PR, the thrown weapon)
#define NSHOT_STILL_MS 3000     // an arrow / thrown weapon still this long is no longer shown (landed, stuck; PR: 3 s)
#define NSHOT_STILL_HOLD_MS 500 // still this long = NSF_STILL (the receiver holds it; the sender slows to NSHOT_SLOW_MS)
#define NSHOT_BOOMS NET_SHOTS_BOOMS
#define NSHOT_BOOM_MS 2000      // a boom is in every packet this long (PR: 120 frames)
#define NSHOT_MS 50             // send period while something moves (20 Hz, the SNAP's)
#define NSHOT_SLOW_MS 250       // send period while everything lies still and no boom is due
#define NSHOT_EMPTY 3           // "nothing any more" is sent this many times (UDP: one may be lost)
#define NSHOT_LOCAL_MS 250      // a local list older than this is not sent (the frame stopped sampling: load, menu, Sora off)
#define NSHOT_QUIET_MS 1000     // a friend's track not in a packet for this long has ended
#define NSHOT_EXTRA_MS 100      // past his newest sample a moving shot goes on this long on its last speed, then holds
#define NST_SAMPLES 8           // samples kept per remote track (> 120 ms of 20 Hz with room for jitter)
#define NST_TRACKS (2 * NSHOT_MAX)  // remote tracks per friend (the ended ones play out while new ones start)
#define NSHOT_WALK_MAX 8192     // actors one walk takes (PR MAX_ACTORS)

// ---- the kinds: their BotW actor names (TitleBG.pack), the model file each is drawn with (data/gear/<name>.bin) ----
static const char *const NSK_NAME[NSK_N] = {"NormalArrow", "FireArrow", "IceArrow", "ElectricArrow", "BombArrow_A", "AncientArrow",
                                            "BrightArrow", "RemoteBomb", "RemoteBombCube", "RemoteBomb2", "RemoteBombCube2", "(thrown)"};
// an actor name -> its NSK_* (arrows and bombs; never NSK_THROWN), -1 = not a shot
static inline int nshot_kind_of(const char *n) {
    if (!n || !n[0] || !strchr("NFIEBAR", n[0])) return -1;
    for (int k = 0; k < NSK_THROWN; k++)
        if (!strcmp(n, NSK_NAME[k])) return k;
    return -1;
}

// ---- rotations: BotW's Matrix34f (row-major, columns = the object's axes in the world) <-> a unit quaternion (x y z w) ----
static inline void nshot_q_from_m(const float m[12], float q[4]) {
    float R[3][3];
    for (int c = 0; c < 3; c++) {  // (the columns made unit length: a scaled actor keeps its turn)
        float x = m[c], y = m[4 + c], z = m[8 + c], l = sqrtf(x * x + y * y + z * z);
        if (!(l > 1e-6f)) l = 1;
        R[0][c] = x / l, R[1][c] = y / l, R[2][c] = z / l;
    }
    float tr = R[0][0] + R[1][1] + R[2][2], s;
    if (tr > 0) s = sqrtf(tr + 1) * 2, q[3] = s / 4, q[0] = (R[2][1] - R[1][2]) / s, q[1] = (R[0][2] - R[2][0]) / s, q[2] = (R[1][0] - R[0][1]) / s;
    else if (R[0][0] > R[1][1] && R[0][0] > R[2][2])
        s = sqrtf(1 + R[0][0] - R[1][1] - R[2][2]) * 2, q[3] = (R[2][1] - R[1][2]) / s, q[0] = s / 4, q[1] = (R[0][1] + R[1][0]) / s, q[2] = (R[0][2] + R[2][0]) / s;
    else if (R[1][1] > R[2][2])
        s = sqrtf(1 + R[1][1] - R[0][0] - R[2][2]) * 2, q[3] = (R[0][2] - R[2][0]) / s, q[0] = (R[0][1] + R[1][0]) / s, q[1] = s / 4, q[2] = (R[1][2] + R[2][1]) / s;
    else s = sqrtf(1 + R[2][2] - R[0][0] - R[1][1]) * 2, q[3] = (R[1][0] - R[0][1]) / s, q[0] = (R[0][2] + R[2][0]) / s, q[1] = (R[1][2] + R[2][1]) / s, q[2] = s / 4;
    float l = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (!(l > 1e-6f)) { q[0] = q[1] = q[2] = 0, q[3] = 1; return; }
    for (int i = 0; i < 4; i++) q[i] /= l;
}
static inline void nshot_r_from_q(const float q[4], float R[3][3]) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    R[0][0] = 1 - 2 * (y * y + z * z), R[0][1] = 2 * (x * y - z * w), R[0][2] = 2 * (x * z + y * w);
    R[1][0] = 2 * (x * y + z * w), R[1][1] = 1 - 2 * (x * x + z * z), R[1][2] = 2 * (y * z - x * w);
    R[2][0] = 2 * (x * z - y * w), R[2][1] = 2 * (y * z + x * w), R[2][2] = 1 - 2 * (x * x + y * y);
}
// the bone rows of a 1-bone gear bake (vertices in cm, the BotW model's own frame) so sora_draw_actor_world(id, rows, pos, 0, 0.01)
// puts the model where BotW draws an actor with this rotation at pos: row r = the object's axis r in the world (sora_draw.c
// skin_verts: q = sum p[r] row r; face 0 = no turn; x 0.01 = cm -> BotW units, which are metres)
static inline void nshot_rows(const float q[4], float rows[12]) {
    float R[3][3];
    nshot_r_from_q(q, R);
    for (int r = 0; r < 3; r++)
        for (int j = 0; j < 3; j++) rows[r * 3 + j] = R[j][r];
    rows[9] = rows[10] = rows[11] = 0;
}
static inline void nshot_nlerp(const float a[4], const float b[4], float u, float out[4]) {
    float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3], sg = d < 0 ? -1.0f : 1.0f, l = 0;
    for (int i = 0; i < 4; i++) out[i] = a[i] + (sg * b[i] - a[i]) * u, l += out[i] * out[i];
    l = sqrtf(l);
    if (!(l > 1e-6f)) { memcpy(out, b, 4 * sizeof(float)); return; }
    for (int i = 0; i < 4; i++) out[i] /= l;
}

// ---- guest reads (PR helpers), each pointer checked with rd(address, bytes) before it is read ----
typedef bool (*NsRd)(uint64_t a, size_t n);
static inline bool ns_u64(NsRd rd, uint64_t a, uint64_t *v) {
    if (!a || !rd(a, 8)) return false;
    *v = *(const uint64_t *)(uintptr_t)a;
    return true;
}
static inline bool ns_u32(NsRd rd, uint64_t a, uint32_t *v) {
    if (!a || !rd(a, 4)) return false;
    *v = *(const uint32_t *)(uintptr_t)a;
    return true;
}
static inline bool ns_u8(NsRd rd, uint64_t a, uint8_t *v) {
    if (!a || !rd(a, 1)) return false;
    *v = *(const uint8_t *)(uintptr_t)a;
    return true;
}
// an actor's name (BaseProc +0x10: its SafeString's text) when it is one: printable, < 48 letters ("" else). (rd once per page:
// a readable region is whole pages, so the walk's thousands of names cost one check each)
static inline void nshot_name(NsRd rd, uint64_t a, char out[48]) {
    memset(out, 0, 48);
    uint64_t n = 0;
    if (!ns_u64(rd, a + 0x10, &n) || !rd(n, 1)) return;
    const char *s = (const char *)(uintptr_t)n;
    int l = 0;
    for (;; l++) {
        if (l && !((n + (uint64_t)l) & 0xfff) && !rd(n + (uint64_t)l, 1)) return;
        if (l >= 47 || !(s[l] > ' ' && s[l] < 127)) break;
    }
    if (!s[l]) memcpy(out, s, (size_t)l), out[l] = 0;
}
// the actor a BaseProcLink at `at` points to (u64 unit, u32 id): alive while the unit holds that id; 0 = none / gone (PR linked)
static inline uint64_t nshot_linked(NsRd rd, uint64_t at) {
    uint64_t u = 0, a = 0;
    uint32_t id = 0, uid = 0;
    uint8_t st = 0;
    if (!ns_u64(rd, at, &u) || !ns_u32(rd, at + 8, &id) || !ns_u32(rd, u + 0x40, &uid) || uid != id || !ns_u64(rd, u + 0x48, &a)) return 0;
    return ns_u8(rd, a + 0x64, &st) && st != 3 ? a : 0;
}
// an actor watched as the game's BaseProcLink does: its unit (+0x68) and id (+0x60) as first seen; alive while the unit holds
// that id and that actor and the actor is not being deleted (+0x64 state 3) (PR watch / alive)
typedef struct { uint64_t unit; uint32_t id; } NsWatch;
static inline bool nshot_watch(NsRd rd, uint64_t a, NsWatch *w) { return ns_u64(rd, a + 0x68, &w->unit) && ns_u32(rd, a + 0x60, &w->id); }
static inline bool nshot_alive(NsRd rd, uint64_t a, NsWatch w) {
    uint32_t uid = 0;
    uint64_t ua = 0;
    uint8_t st = 0;
    return a && ns_u32(rd, w.unit + 0x40, &uid) && uid == w.id && ns_u64(rd, w.unit + 0x48, &ua) && ua == a && ns_u8(rd, a + 0x64, &st) && st != 3;
}
// (s290 arrow, netshotsstuck) the actor still has this id (+0x60) and is not being deleted (+0x64 state 3)
static inline bool nshot_same(NsRd rd, uint64_t a, uint32_t id) {
    uint32_t aid = 0;
    uint8_t st = 0;
    return a && ns_u32(rd, a + 0x60, &aid) && aid == id && ns_u8(rd, a + 0x64, &st) && st != 3;
}
// (s290 arrow, after PR #294's alive_rewatch: "the same actor gone to another proc unit ... its id the same, not deleted; then
// watched again") its watch failed, but it is the same actor (nshot_same) and its unit now (+0x68) holds it (+0x40 its id, +0x48
// it): *w = that unit, true. (s289 lab s289q: each IceArrow's watch failed at its hit while it stayed stuck in the shooter's
// world, and the walk found the same actor, same address and id, again: GUESS, a stuck arrow gets a new proc unit)
static inline bool nshot_rewatch(NsRd rd, uint64_t a, NsWatch *w) {
    NsWatch n = {0, w->id};
    if (!nshot_same(rd, a, w->id) || !ns_u64(rd, a + 0x68, &n.unit) || !n.unit || !nshot_alive(rd, a, n)) return false;
    *w = n;
    return true;
}
// every actor the game has, from one actor's proc unit (actor +0x68): the units are one list (+0 their vtable, +0x10 / +0x18 the
// previous / next one's +0x10; +0x40 the actor's id, +0x48 the actor), its head no unit (PR all_actors, the same walk, but the
// forward run's last actor is KEPT: the PR drops it (its "n--"), and the newest actor, an arrow just shot, may be that one; the
// PR finds arrows with an exe patch instead. A duplicate or an odd entry costs nothing here: names are matched, reads checked)
// (units: NULL, or the unit each actor was found through, for nsl_found)
static inline int nshot_walk_u(NsRd rd, uint64_t from, uint64_t *out, uint64_t *units, int max) {
    uint64_t vt = 0;
    if (!ns_u64(rd, from, &vt)) return 0;
    int n = 0;
    for (int dir = 0; dir < 2; dir++) {
        uint64_t u = from;
        for (int k = 0; k < 16384 && n < max; k++) {
            if (dir == 1 || k > 0) {
                uint64_t a = 0;
                uint32_t aid = 0, uid = 0;
                if (ns_u64(rd, u + 0x48, &a) && ns_u32(rd, a + 0x60, &aid) && ns_u32(rd, u + 0x40, &uid) && aid == uid) {
                    if (units) units[n] = u;
                    out[n++] = a;
                }
            }
            uint64_t nx = 0, v = 0;
            if (!ns_u64(rd, u + (dir ? 0x10 : 0x18), &nx) || nx < 0x10 || nx - 0x10 == from) break;
            u = nx - 0x10;
            if (!ns_u64(rd, u, &v) || v != vt) break;
        }
    }
    return n;
}
static inline int nshot_walk(NsRd rd, uint64_t from, uint64_t *out, int max) { return nshot_walk_u(rd, from, out, NULL, max); }
// the AS an actor plays in slot / bank (its +0x560 AS manager): "" = none / unreadable. ASList's lookup (header): slots at +0xb8,
// table +0xc0 (0x50 per slot: +0x20 banks, +0x28 the bank array, 0x98 per bank), a bank's +0x18 = something plays, *bank = the
// AS, whose +0x10 is its name
static inline const char *nshot_as_name(NsRd rd, uint64_t actor, int slot, int bank) {
    uint64_t mgr = 0, tab = 0, arr = 0, used = 0, as = 0, nm = 0;
    uint32_t nslot = 0, nbank = 0;
    if (slot < 0 || bank < 0 || !ns_u64(rd, actor + 0x560, &mgr) || !ns_u32(rd, mgr + 0xb8, &nslot) || (int32_t)nslot <= slot ||
        !ns_u64(rd, mgr + 0xc0, &tab) || !ns_u32(rd, tab + (uint64_t)slot * 0x50 + 0x20, &nbank) || (int32_t)nbank <= bank ||
        !ns_u64(rd, tab + (uint64_t)slot * 0x50 + 0x28, &arr) || !arr)
        return "";
    uint64_t b = arr + (uint64_t)bank * 0x98;
    if (!ns_u64(rd, b + 0x18, &used) || !used || !ns_u64(rd, b, &as) || !ns_u64(rd, as + 8 + 8, &nm) || !rd(nm, 1)) return "";
    for (int l = 0; l < 64; l++)  // (a name ends within 64 readable letters, else it is no name)
        if (!rd(nm + (uint64_t)l, 1)) return "";
        else if (!((const char *)(uintptr_t)nm)[l]) return (const char *)(uintptr_t)nm;
    return "";
}
// his AS channels the PR reads ({slot, bank}): the body, the upper body's two, the third
static const int NSHOT_AS_CH[4][2] = {{0, 0}, {1, 0}, {1, 1}, {2, 0}};
static inline bool nshot_bow_as(NsRd rd, uint64_t player) {
    for (int c = 0; c < 4; c++)
        if (strstr(nshot_as_name(rd, player, NSHOT_AS_CH[c][0], NSHOT_AS_CH[c][1]), "Bow")) return true;
    return false;
}
// how often the game side walks, from what its walks cost lately (avg_us, an average): 0 = every frame while he uses a bow,
// 1 = every 6th frame, 2 = once a second. Up once the average passes a line, down only once it is under half of that line (one
// slow walk alone changes nothing; s289 lab: one walk of 3297 us among 360 us ones had stopped the bowing walks for the session)
static inline int nshot_walk_level(int cur, float avg_us, float slow_us, float slower_us) {
    int up = avg_us > slower_us ? 2 : avg_us > slow_us ? 1 : 0;
    if (up >= cur) return up;
    int keep = avg_us > slower_us / 2 ? 2 : avg_us > slow_us / 2 ? 1 : 0;
    return keep < cur ? keep : cur;
}
// a physical object: its body (+0x190) a real physics object (8-aligned, its vtable in the game's image) (PR has_body)
static inline bool nshot_has_body(NsRd rd, uint64_t a) {
    uint64_t body = 0, vt = 0;
    return ns_u64(rd, a + 0x190, &body) && !(body & 7) && ns_u64(rd, body, &vt) && vt >= 0x7000000000ull && vt < 0x7004000000ull;
}
// an actor's parent (BaseProc +0x80) while it is not being deleted (+0x64 state 3); 0 = none (1.9.0 BaseProc::getParent, Ghidra
// 0x71015150f8, read s289). How BotW shoots an arrow (1.9.0 code, read s289 in Ghidra): ONE actor from his string to its flight.
// The arrow ("NormalArrow" ... "AncientArrow", the item's name, in the proc list every actor is in) is made as a child of the
// bow; its root AI "Arrow" (vtable Ghidra 0x7101c6e700) starts in 所持前, goes to 所持 (EquipedWeaponChild, ParentActor =
// that parent, at the parent's arrow bone: the arrow on his string), and when he lets go the same arrow takes its speed and aim
// from the bow, asks to be cut from it (0x7101516610) and turns to 発射 / 上空撃ち / ホーミング (ArrowShootMove ...) (Arrow
// vt[34], 0x7100931364). An arrow made to be shot at once (its +0xc7a set by whoever makes it) skips the string (0x710092ce7c,
// from enter / 所持前).
static inline uint64_t nshot_parent(NsRd rd, uint64_t a) {
    uint64_t p = 0;
    uint8_t st = 0;
    return ns_u64(rd, a + 0x80, &p) && p && ns_u8(rd, p + 0x64, &st) && st != 3 ? p : 0;
}
// an arrow on his bow / in his hands: its parent is him, his bow (0 = none known) or a child of his
static inline bool nshot_on_him(NsRd rd, uint64_t a, uint64_t player, uint64_t bow) {
    uint64_t p = nshot_parent(rd, a);
    return p && player && (p == player || (bow && p == bow) || nshot_parent(rd, p) == player);
}

// ---- local: his shots (sender) ----
// What is sent, and when (the PR follows an arrow from its first move: its exe patch on the arrow's move action fills a ring;
// here the walk sees an arrow from its first frame, so these stand in for "it moves"):
//   arrows   his when on his bow while he uses it (nshot_on_him, whatever was said of it before) or by the PR's check when
//            first seen. The one on his string IS the one that flies (nshot_parent's note), so it is followed while it waits
//            there, never dropped for lying still on his bow, and never sent before it flies: cut from his bow (two looks) and
//            moving NSHOT_CUT_M farther from his feet than on it (also slow: bullet time), or, never seen on his bow, NSHOT_FLY_MPS
//            fast at NSHOT_FLY_M or more from his feet. (s289 lab: one was sent at 2.01 m while still on his string; the next
//            ones waited 3 s on the string, were dropped as lying still and remembered as not his, and flew unsent.) Dropped
//            after NSHOT_STILL_MS still (remembered as not his only once it flew: stuck); never moved and not on his bow, once
//            he stops using his bow (one lying near him as he aimed: remembered unless it was on his bow); on his bow while he
//            does not use it, after NSHOT_STILL_MS (not remembered; not taken while so)
//   bombs    every Remote Bomb in this world (only his rune makes them [GUESS]): sent from the first look (held, thrown, rolling),
//            no longer once it starts "Explode" (the boom goes out instead) or goes (put away)
//   thrown   his weapon let go of (nsl_offer from the game side): sent once moved, dropped after NSHOT_STILL_MS still or when it is
//            his again (nsl_drop: back in his hand / on his back)
#define NSHOT_AWAY_M 2.0f       // (the trace's "away": this far from his feet; s289 lab: the arrow on his string at full draw, 2.01 m)
#define NSHOT_FLY_M 3.5f        // an arrow flies once it is this far from his feet ... (on his string: 2.0 - 2.5 m [GUESS for the top])
#define NSHOT_FLY_MPS 25.0f     // ... and moved this fast since the look before (a shot leaves at tens of m/s [GUESS]; his hands, his
                                // gallop: slower)
#define NSHOT_JUMP_M 30.0f      // farther than this between two looks: a jump (load, warp, a reused actor), never a flight
#define NSHOT_CUT_M 0.25f       // cut from his bow: it flies once this much farther from his feet than at its last look on the bow
// (s290 arrow) `set netshotsstuck N` (NsLocal.stuck_ms = N s; 0 = as s289): an arrow of his that flew and then lies (stuck, landed)
// is followed and sent (NSF_STILL, 4 Hz) until his game removes it (its watch fails, the re-watch too, and its +0x60 id is no
// longer there), not dropped NSHOT_STILL_MS after it lies; at most N s still (the cap). BotW 1.9.0 (read s290 in Ghidra): the
// Arrow AI (vt[15] loadParams 0x710092c9a8: StickTime at +0xa0, GroundHitTime at +0xa8) sets a frame timer as it sticks / lands
// (0x7100930818 / 0x71009311a8: the actor's GeneralParamList Arrow int if not 0, else StickTime / GroundHitTime) and counts it
// down by the frame delta (0x710151cfe8); the data: StickTime = GroundHitTime = 1800.0 in every arrow's AIProgram (60 s at 30
// frames a second [GUESS: frames]); Arrow DeleteTime: Ice / Electric 30, Fire 90, the rest -1 [which int the timer takes is a GUESS]
#define NSHOT_STUCK_S 600       // the cap's default (s291 user: the max; lab s291: a stuck NormalArrow outlived 600 s, an IceArrow 1.0 s; full tracks: the oldest stuck one gives way)
#define NSHOT_LOST_MS 2000      // watch + re-watch failed but its id is still there: kept (its last pose) this long for the walk to
                                // find it on the unit it is on now (nsl_found) [GUESS: the walk runs at least once a second]
#define NSHOT_STUCK_SLACK_MS 5000  // a friend's arrow still past netshotsstuck + this, with no end from him: hidden here (nsp_cap)
typedef struct {
    uint64_t actor;
    NsWatch w;
    uint16_t id;
    uint8_t kind;
    bool boomed, moved, away, said;  // (said: the game side's log line "flies" is out)
    bool held, was_held, flew;       // (arrows: on his bow at the last look / ever; it flew: sent from then on)
    uint8_t off;                     // (arrows: looks off his bow in a row since it was on it)
    float m[12];
    float dist, speed, held_dist;    // (the last look: m from his feet, m/s since the look before; m from his feet on the bow)
    uint64_t seen_at, still_since, at, idle_since;  // (at: the last look; idle_since: on his bow while he uses none)
    uint64_t lost_since;             // (s290: its watch failed, its id still there: nsl_lose; 0 = not lost)
    bool stuck_said;                 // (s290: the game side's log line "lies still ... kept" is out)
    char name[NET_GEAR_NAME_MAX + 1];  // (NSK_THROWN: its actor name)
} NsTrack;
// why nsl_offer took an actor or not (NsLocal.why after each call; the trace's words NSW_TXT), why nsl_update let one go (.end)
enum { NSW_NONE, NSW_ON_BOW, NSW_NEAR, NSW_BOMB, NSW_THROWN, NSW_REJECTED, NSW_NOT_BOWING, NSW_TOO_FAR, NSW_FULL, NSW_IDLE_BOW, NSW_N };
static const char *const NSW_TXT[NSW_N] = {"", "taken: on our bow", "taken: near our feet while we use a bow", "taken: a Remote Bomb",
                                           "taken: our thrown weapon", "not ours (remembered)", "not ours: we use no bow",
                                           "not ours: too far from our feet", "not followed: 8 followed, none lying still",
                                           "not yet: on our bow, but we use no bow (not remembered)"};
enum { NSE_NONE, NSE_GONE, NSE_STILL, NSE_UNMOVED, NSE_IDLE, NSE_CAP, NSE_N };
static const char *const NSE_TXT[NSE_N] = {"", "gone", "lay still", "never moved", "on our bow while we use none", "still past netshotsstuck (the cap)"};
typedef struct {
    NsTrack tr[NSHOT_MAX];
    struct { uint64_t a; uint32_t id; } not_his[NSHOT_NOT_HIS];
    int not_his_next;
    uint16_t next_id, boom_seq;
    struct { uint16_t seq; uint8_t kind; float pos[3]; uint64_t at; } boom[NSHOT_BOOMS];
    uint64_t bow_at;
    uint8_t why, end;     // (the last nsl_offer's NSW_*; the NSE_* of the last nsl_update that let one go)
    uint16_t evicted_id;  // (the last nsl_offer took the place of this track; 0 = none)
    uint32_t stuck_ms;    // (s290: netshotsstuck in ms, set by the game side each frame; 0 = as s289)
    uint32_t end_still_ms;  // (s290: the track let go last had lain still this long; 0 = it moved)
    struct { uint32_t tracked, not_his, full, evicted, gone, still, unmoved, booms, put_away, his_again, on_bow, flew, idle;
             uint32_t rewatched, lost, found, stuck_gone, capped; } st;  // (s290: the netshotsstuck ones)
} NsLocal;
enum { NSO_KNOWN = 0, NSO_TAKEN = 1, NSO_NOT_HIS = 2, NSO_FULL = 3, NSO_BAD = 4 };

// first_id: a number of this run (GetTickCount64 based): track ids and boom seqs never start again where a hot reload left them,
// so a friend's game never takes a new shot for an old one
static inline void nsl_init(NsLocal *L, uint16_t first_id) {
    memset(L, 0, sizeof *L);
    L->next_id = first_id ? first_id : 1, L->boom_seq = (uint16_t)(first_id * 7u + 3u);
}
static inline void nsl_not_his(NsLocal *L, uint64_t a, uint32_t id) {
    L->not_his[L->not_his_next].a = a, L->not_his[L->not_his_next].id = id, L->not_his_next = (L->not_his_next + 1) % NSHOT_NOT_HIS;
}
// bow_now = one of his AS channels plays a Bow AS this frame (or our bow mimic is on); -> he uses a bow now or did NSHOT_BOW_MS ago
static inline bool nsl_bowing(NsLocal *L, uint64_t now, bool bow_now) {
    if (bow_now) L->bow_at = now ? now : 1;
    return L->bow_at && now >= L->bow_at && now - L->bow_at < NSHOT_BOW_MS;
}
static inline int nsl_find(const NsLocal *L, uint64_t a, uint32_t id) {
    for (int i = 0; i < NSHOT_MAX; i++)
        if (L->tr[i].actor == a && L->tr[i].w.id == id) return i;
    return -1;
}
static inline bool nsl_rejected(const NsLocal *L, uint64_t a, uint32_t id) {
    for (int i = 0; i < NSHOT_NOT_HIS; i++)
        if (L->not_his[i].a == a && L->not_his[i].id == id) return true;
    return false;
}
static inline void nsl_unreject(NsLocal *L, uint64_t a, uint32_t id) {
    for (int i = 0; i < NSHOT_NOT_HIS; i++)
        if (L->not_his[i].a == a && L->not_his[i].id == id) L->not_his[i].a = 0, L->not_his[i].id = 0;
}
// an actor the walk found (kind = nshot_kind_of its name, or NSK_THROWN for his let-go weapon), m = its matrix, link = his feet,
// held = an arrow on his bow now (nshot_on_him). Arrows: his when held while bowing (whatever was said of it before; held while
// not bowing: not yet, not remembered), else only when first seen within NSHOT_NEAR_M while bowing; any other is remembered as
// not his (it becomes his later only on his bow; PR). Bombs and the thrown weapon: his (only his rune makes Remote Bombs in this
// world; the weapon comes from his hand). L->why says which.
static inline int nsl_offer(NsLocal *L, uint64_t now, uint64_t a, NsWatch w, int kind, const float m[12], const float link[3], bool bowing,
                            bool held, const char *name) {
    L->why = NSW_NONE, L->evicted_id = 0;
    if (!a || kind < 0 || kind >= NSK_N || (kind == NSK_THROWN && (!name || !name[0]))) return NSO_BAD;
    if (nsl_find(L, a, w.id) >= 0) return NSO_KNOWN;
    bool arrow = nshot_kind_arrow(kind);
    float dx = m[3] - link[0], dy = m[7] - link[1], dz = m[11] - link[2], d2 = dx * dx + dy * dy + dz * dz;
    held = held && arrow;
    if (held) {
        if (!bowing) { L->why = NSW_IDLE_BOW; return NSO_NOT_HIS; }  // (on his back [GUESS]: taken once he uses his bow)
        nsl_unreject(L, a, w.id);
        L->why = NSW_ON_BOW;
    } else if (arrow) {
        if (nsl_rejected(L, a, w.id)) { L->why = NSW_REJECTED; return NSO_NOT_HIS; }
        if (!bowing || d2 > NSHOT_NEAR_M * NSHOT_NEAR_M) {
            nsl_not_his(L, a, w.id);
            L->st.not_his++;
            L->why = bowing ? NSW_TOO_FAR : NSW_NOT_BOWING;
            return NSO_NOT_HIS;
        }
        L->why = NSW_NEAR;
    } else L->why = kind == NSK_THROWN ? NSW_THROWN : NSW_BOMB;
    int k = -1;
    for (int i = 0; i < NSHOT_MAX && k < 0; i++)
        if (!L->tr[i].actor) k = i;
    if (k < 0) {  // (all in use: the one lying still the longest gives way, never one in flight, on his bow, or a bomb)
        uint64_t best = 0;
        for (int i = 0; i < NSHOT_MAX; i++)
            if (!nshot_kind_bomb(L->tr[i].kind) && !L->tr[i].held && L->tr[i].still_since && (!best || L->tr[i].still_since < best))
                best = L->tr[i].still_since, k = i;
        if (k < 0) { L->st.full++; L->why = NSW_FULL; return NSO_FULL; }
        L->st.evicted++;
        L->evicted_id = L->tr[k].id;
        const NsTrack *e = &L->tr[k];  // (a stuck shot, an arrow that never moved: never taken again; one that only waited: may be)
        if (nshot_kind_arrow(e->kind) && (e->flew || !e->moved)) nsl_not_his(L, e->actor, e->w.id);
    }
    NsTrack *t = &L->tr[k];
    memset(t, 0, sizeof *t);
    t->actor = a, t->w = w, t->kind = (uint8_t)kind, t->seen_at = t->at = now ? now : 1;
    t->held = t->was_held = held, t->dist = sqrtf(d2), t->held_dist = held ? t->dist : 0;
    if (held) L->st.on_bow++;
    memcpy(t->m, m, sizeof t->m);
    if (kind == NSK_THROWN) snprintf(t->name, sizeof t->name, "%s", name);
    t->id = L->next_id++;
    if (!L->next_id) L->next_id = 1;
    L->st.tracked++;
    return NSO_TAKEN;
}
// track i no longer followed (not_his: an arrow, remembered so the walk never takes it again)
static inline void nsl_drop(NsLocal *L, int i, bool not_his) {
    NsTrack *t = &L->tr[i];
    if (!t->actor) return;
    if (not_his) nsl_not_his(L, t->actor, t->w.id);
    memset(t, 0, sizeof *t);
}
// (s292 coopfix) `set netshots` changed: every track goes. While the key was 0 nothing looked at them (nsg_sample skips them), so
// their watch, place and timers are stale; lab s291 (h): after 0 -> 1 the old #38486 was sent again 21 m from where it stuck.
// Its arrows are remembered as not his (a stuck one is never taken again; a Remote Bomb still out is taken again by the next
// walk, under a new id, where it is now); ids and boom seqs go on where they were (a friend never takes a new shot for an old
// one); booms still due are dropped. -> how many tracks went
static inline int nsl_restart(NsLocal *L) {
    int n = 0;
    for (int i = 0; i < NSHOT_MAX; i++) {
        if (!L->tr[i].actor) continue;
        nsl_drop(L, i, nshot_kind_arrow(L->tr[i].kind));
        n++;
    }
    memset(L->boom, 0, sizeof L->boom);
    L->bow_at = 0, L->why = NSW_NONE, L->end = NSE_NONE, L->evicted_id = 0, L->end_still_ms = 0;
    return n;
}
// a track that is sent now: not gone off, and (bombs) at once / (arrows) once it flew / (his thrown weapon) once it moved
static inline bool nsl_sent(const NsTrack *t) {
    return t->actor && !t->boomed && (nshot_kind_bomb(t->kind) || (nshot_kind_arrow(t->kind) ? t->flew : t->moved));
}
// track i this frame: alive (the watch holds), m = its matrix now, explode = a bomb whose AS slot 0 is "Explode", link = his feet,
// bowing = nsl_bowing this frame, held = an arrow on his bow now (nshot_on_him). -> false = no longer followed (why: L->end; see
// the notes above NsTrack). A bomb gone without Explode was put away.
static inline bool nsl_update(NsLocal *L, int i, uint64_t now, bool alive, const float m[12], bool explode, const float link[3], bool bowing,
                              bool held) {
    NsTrack *t = &L->tr[i];
    L->end = NSE_NONE, L->end_still_ms = 0;
    if (!t->actor) return false;
    if (!alive) {
        if (nshot_kind_bomb(t->kind) && !t->boomed) L->st.put_away++;
        L->st.gone++;
        L->end = NSE_GONE;
        if (t->still_since && now >= t->still_since) L->end_still_ms = (uint32_t)(now - t->still_since);
        if (L->stuck_ms && nshot_kind_arrow(t->kind) && t->flew && L->end_still_ms >= NSHOT_STILL_HOLD_MS) L->st.stuck_gone++;  // (the game removed it)
        nsl_drop(L, i, false);
        return false;
    }
    t->lost_since = 0;
    bool arrow = nshot_kind_arrow(t->kind);
    float dx = m[3] - t->m[3], dy = m[7] - t->m[7], dz = m[11] - t->m[11], step = sqrtf(dx * dx + dy * dy + dz * dz);
    if (step > NSHOT_STILL_M) t->still_since = 0, t->moved = true;
    else if (!t->still_since) t->still_since = now ? now : 1;
    uint64_t dt = now > t->at ? now - t->at : 0;
    t->speed = dt ? step * 1000.0f / (float)dt : 0;
    memcpy(t->m, m, sizeof t->m);
    t->at = now ? now : 1;
    float lx = m[3] - link[0], ly = m[7] - link[1], lz = m[11] - link[2];
    t->dist = sqrtf(lx * lx + ly * ly + lz * lz);
    if (t->dist > NSHOT_AWAY_M) t->away = true;
    t->held = held && arrow;
    if (t->held) t->was_held = true, t->off = 0, t->held_dist = t->dist;
    else if (t->was_held && t->off < 255) t->off++;
    // it flies: (cut) off his bow two looks in a row after being on it, moving, NSHOT_CUT_M farther from his feet than on it (the
    // game cuts the nocked arrow as he lets go, a request it carries out a little later; slow in bullet time), or (fast)
    // NSHOT_FLY_MPS fast at NSHOT_FLY_M or more from his feet (one never seen on his bow, or before the cut shows)
    if (arrow && !t->flew && dt && step <= NSHOT_JUMP_M &&
        ((t->was_held && t->off >= 2 && step > NSHOT_STILL_M && t->dist >= t->held_dist + NSHOT_CUT_M) ||
         (t->speed >= NSHOT_FLY_MPS && t->dist >= NSHOT_FLY_M)))
        t->flew = true, L->st.flew++;
    if (nshot_kind_bomb(t->kind) && !t->boomed && explode) {  // (it goes some frames later: no longer shown as a bomb from now)
        memmove(&L->boom[1], &L->boom[0], sizeof L->boom[0] * (NSHOT_BOOMS - 1));
        if (!++L->boom_seq) L->boom_seq = 1;
        L->boom[0].seq = L->boom_seq, L->boom[0].kind = t->kind, L->boom[0].at = now ? now : 1;
        L->boom[0].pos[0] = m[3], L->boom[0].pos[1] = m[7], L->boom[0].pos[2] = m[11];
        t->boomed = true, L->st.booms++;
    }
    if (arrow && t->held && !t->flew && !bowing) {  // (on his bow while he uses none, on his back [GUESS]: let go, not remembered)
        if (!t->idle_since) t->idle_since = now ? now : 1;
        if (now >= t->idle_since && now - t->idle_since >= NSHOT_STILL_MS) {
            L->st.idle++;
            L->end = NSE_IDLE;
            nsl_drop(L, i, false);
            return false;
        }
    } else t->idle_since = 0;
    if (arrow && !t->moved && !bowing && !t->held) {  // (never moved, not on his bow, and his bow is away: not a shot of his)
        L->st.unmoved++;
        L->end = NSE_UNMOVED;
        nsl_drop(L, i, !t->was_held);  // (remembered, unless it was on his bow)
        return false;
    }
    // (s290 netshotsstuck: an arrow that flew and lies is kept until the game removes it, at most L->stuck_ms still)
    bool hold = L->stuck_ms && arrow && t->flew;
    if (!nshot_kind_bomb(t->kind) && t->moved && t->still_since && now - t->still_since >= (hold ? L->stuck_ms : NSHOT_STILL_MS) && !t->held) {
        if (hold) L->st.capped++, L->end = NSE_CAP;
        else L->st.still++, L->end = NSE_STILL;  // (an arrow that flew and lies: stuck, remembered; one that only waited in his hands: may come again)
        L->end_still_ms = (uint32_t)(now - t->still_since);
        nsl_drop(L, i, arrow && t->flew);
        return false;
    }
    if (t->kind == NSK_THROWN && !t->moved && now - t->seen_at >= NSHOT_STILL_MS) {  // (let go of and never moved: not thrown)
        L->st.unmoved++;
        L->end = NSE_UNMOVED;
        nsl_drop(L, i, false);
        return false;
    }
    return true;
}
// (s290 netshotsstuck) track i this frame when its watch AND its re-watch (nshot_rewatch) failed while its actor still has its id
// (nshot_same): kept at its last pose (sent as before: the friends hold it) for NSHOT_LOST_MS, for the walk to find it again
// (nsl_found); -> false = no longer followed (L->end NSE_GONE, as nsl_update's not alive)
static inline bool nsl_lose(NsLocal *L, int i, uint64_t now) {
    NsTrack *t = &L->tr[i];
    L->end = NSE_NONE, L->end_still_ms = 0;
    if (!t->actor) return false;
    if (!t->lost_since) {
        t->lost_since = now ? now : 1;
        L->st.lost++;
        return true;
    }
    if (now >= t->lost_since && now - t->lost_since < NSHOT_LOST_MS) return true;
    return nsl_update(L, i, now, false, t->m, false, t->m, false, false);  // (gone: not found again; m / link unused when not alive)
}
// (s290 netshotsstuck) the walk found actor a (its +0x60 id) on `unit`: a track of it that is lost (nsl_lose) on another unit
// watches that unit from now (the next look's nshot_alive checks it; only a look that holds, nsl_update alive, ends the lost
// time: one found but still failing, an unreadable matrix, goes when NSHOT_LOST_MS is over) -> its index, -1 = none
static inline int nsl_found(NsLocal *L, uint64_t a, uint32_t id, uint64_t unit) {
    int i = nsl_find(L, a, id);
    if (i < 0 || !L->tr[i].lost_since || !unit || L->tr[i].w.unit == unit) return -1;
    L->tr[i].w.unit = unit;
    L->st.found++;
    return i;
}
// the actor of a track that is sent as a shot now (for the loot helper: a weapon / arrow in flight is not a drop yet); -1 = not one
static inline int nsl_flying(const NsLocal *L, uint64_t actor) {
    for (int i = 0; actor && i < NSHOT_MAX; i++)
        if (L->tr[i].actor == actor && nsl_sent(&L->tr[i])) return i;
    return -1;
}
// the shots to send now (bombs that went off: no longer); *moving = one of them moved within NSHOT_STILL_HOLD_MS
static inline int nsl_list(const NsLocal *L, uint64_t now, NetShot out[NSHOT_MAX], bool *moving) {
    int n = 0;
    if (moving) *moving = false;
    for (int i = 0; i < NSHOT_MAX; i++) {
        const NsTrack *t = &L->tr[i];
        if (!nsl_sent(t)) continue;
        NetShot *s = &out[n++];
        memset(s, 0, sizeof *s);
        s->id = t->id, s->kind = t->kind;
        bool still = t->still_since && now - t->still_since >= NSHOT_STILL_HOLD_MS;
        s->flags = still ? NSF_STILL : 0;
        if (!still && moving) *moving = true;
        s->pos[0] = t->m[3], s->pos[1] = t->m[7], s->pos[2] = t->m[11];
        nshot_q_from_m(t->m, s->q);
        if (t->kind == NSK_THROWN) snprintf(s->name, sizeof s->name, "%s", t->name);
    }
    return n;
}
static inline int nsl_booms(const NsLocal *L, uint64_t now, NetBoom out[NSHOT_BOOMS]) {
    int n = 0;
    for (int i = 0; i < NSHOT_BOOMS; i++)
        if (L->boom[i].seq && now - L->boom[i].at < NSHOT_BOOM_MS) {
            out[n].seq = L->boom[i].seq, out[n].kind = L->boom[i].kind;
            memcpy(out[n].pos, L->boom[i].pos, sizeof out[n].pos);
            n++;
        }
    return n;
}

// the send decision (once per net tick): state = shots or booms to say; -> send now. empties = "nothing" packets still owed after
// the last state packet (set to NSHOT_EMPTY by the caller each time it sends state).
static inline bool nshot_due(bool state, bool moving_or_boom, int empties, uint64_t now, uint64_t last_tx) {
    if (!state && empties <= 0) return false;
    uint64_t period = state && !moving_or_boom ? NSHOT_SLOW_MS : NSHOT_MS;
    return !last_tx || now < last_tx || now - last_tx >= period;
}

// ---- remote: a friend's shots (receiver) ----
typedef struct { uint64_t t; float p[3], q[4]; } NstSample;
typedef struct {
    bool used, ended;
    uint16_t id;
    uint8_t kind, flags;
    char name[NET_GEAR_NAME_MAX + 1];
    NstSample s[NST_SAMPLES];
    int ns;
    uint64_t last_at;  // when a packet last listed it
    uint64_t still_at;            // (s290: since when his packets say NSF_STILL; 0 = moving)
    bool stuck_said, end_said;    // (s290: the glue's log lines "stuck here" / its end are out)
} NstTrack;
typedef struct {
    NstTrack tr[NST_TRACKS];
    uint16_t boom_seen[16];
    int boom_next;
    struct { uint16_t seq; uint8_t kind; float pos[3]; uint64_t at; bool due; } bq[NSHOT_BOOMS * 2];
    struct { uint32_t new_tracks, full, booms, boom_dups, ended, capped; } st;
    uint16_t capped_id[8];  // (s290: the ids nsp_cap ended: still listed by him, never shown again; 0 = a free entry)
    int capped_next;
} NsPeer;

static inline NstTrack *nsp_track(NsPeer *P, uint16_t id) {
    for (int i = 0; i < NST_TRACKS; i++)
        if (P->tr[i].used && P->tr[i].id == id) return &P->tr[i];
    return NULL;
}
// one packet of his (fresh: net_esseq_fresh said so) taken at now
static inline void nsp_take(NsPeer *P, uint64_t now, const NetShot *s, int n, const NetBoom *b, int nb) {
    for (int i = 0; i < NST_TRACKS; i++)
        if (P->tr[i].used && !P->tr[i].ended) {
            bool listed = false;
            for (int j = 0; j < n && !listed; j++) listed = s[j].id == P->tr[i].id;
            if (!listed) P->tr[i].ended = true, P->st.ended++;
        }
    for (int j = 0; j < n; j++) {
        bool capped = false;  // (s290: one the cap hid stays hidden while he lists it)
        for (int c = 0; c < 8 && !capped; c++) capped = P->capped_id[c] && P->capped_id[c] == s[j].id;
        if (capped) continue;
        NstTrack *t = nsp_track(P, s[j].id);
        if (t && (t->ended || t->kind != s[j].kind)) t->used = false, t = NULL;  // (an id again after it ended: a new shot)
        if (!t) {
            for (int i = 0; i < NST_TRACKS && !t; i++)
                if (!P->tr[i].used) t = &P->tr[i];
            if (!t) {  // (all in use: the ended one that ended first gives way)
                uint64_t best = 0;
                for (int i = 0; i < NST_TRACKS; i++)
                    if (P->tr[i].ended && (!best || P->tr[i].last_at < best)) best = P->tr[i].last_at, t = &P->tr[i];
            }
            if (!t) { P->st.full++; continue; }
            memset(t, 0, sizeof *t);
            t->used = true, t->id = s[j].id, t->kind = s[j].kind;
            snprintf(t->name, sizeof t->name, "%s", s[j].kind == NSK_THROWN ? s[j].name : NSK_NAME[s[j].kind]);
            P->st.new_tracks++;
        }
        if (t->ns == NST_SAMPLES) memmove(&t->s[0], &t->s[1], sizeof t->s[0] * (NST_SAMPLES - 1)), t->ns--;
        NstSample *e = &t->s[t->ns++];
        e->t = now, memcpy(e->p, s[j].pos, sizeof e->p), memcpy(e->q, s[j].q, sizeof e->q);
        t->flags = s[j].flags, t->last_at = now;
        if (!(s[j].flags & NSF_STILL)) t->still_at = 0;
        else if (!t->still_at) t->still_at = now ? now : 1;
    }
    for (int j = 0; j < nb; j++) {
        bool seen = false;
        for (int i = 0; i < 16 && !seen; i++) seen = P->boom_seen[i] == b[j].seq;
        if (seen) { P->st.boom_dups++; continue; }
        P->boom_seen[P->boom_next] = b[j].seq, P->boom_next = (P->boom_next + 1) % 16;
        int k = -1;
        for (int i = 0; i < NSHOT_BOOMS * 2 && k < 0; i++)
            if (!P->bq[i].due) k = i;
        if (k < 0) k = 0;  // (a burst of more than 8: the oldest queued gives way)
        P->bq[k].seq = b[j].seq, P->bq[k].kind = b[j].kind, P->bq[k].at = now, P->bq[k].due = true;
        memcpy(P->bq[k].pos, b[j].pos, sizeof P->bq[k].pos);
        P->st.booms++;
    }
}
// where track t is at time `at` (his time line as the puppet plays it: now - NET_INTERP_MS). False = not shown (not started yet,
// or ended and played out: the caller frees it with nsp_reap).
static inline bool nsp_sample(const NstTrack *t, uint64_t at, float p[3], float q[4]) {
    if (!t->used || !t->ns || at < t->s[0].t) return false;
    for (int i = 0; i + 1 < t->ns; i++) {
        const NstSample *a = &t->s[i], *b = &t->s[i + 1];
        if (at <= b->t) {
            float u = b->t > a->t ? (float)(at - a->t) / (float)(b->t - a->t) : 1.0f;
            for (int c = 0; c < 3; c++) p[c] = a->p[c] + (b->p[c] - a->p[c]) * u;
            nshot_nlerp(a->q, b->q, u, q);
            return true;
        }
    }
    const NstSample *z = &t->s[t->ns - 1];
    if (t->ended) return false;  // (his newest packet no longer lists it and its last place has been shown)
    memcpy(p, z->p, sizeof z->p), memcpy(q, z->q, sizeof z->q);
    if (t->ns >= 2 && !(t->flags & NSF_STILL)) {  // (past his newest sample: on at its last speed for a short while, then held)
        const NstSample *y = &t->s[t->ns - 2];
        uint64_t dt = z->t - y->t, ex = at - z->t;
        if (ex > NSHOT_EXTRA_MS) ex = NSHOT_EXTRA_MS;
        if (dt > 0 && dt < 1000)
            for (int c = 0; c < 3; c++) p[c] += (z->p[c] - y->p[c]) * (float)ex / (float)dt;
    }
    return true;
}
// end the tracks a quiet friend no longer speaks of, free the ones played out at `at`; -> tracks still in use
static inline int nsp_reap(NsPeer *P, uint64_t now, uint64_t at) {
    int n = 0;
    for (int i = 0; i < NST_TRACKS; i++) {
        NstTrack *t = &P->tr[i];
        if (!t->used) continue;
        if (!t->ended && now > t->last_at && now - t->last_at > NSHOT_QUIET_MS) t->ended = true, P->st.ended++;
        if (t->ended && (!t->ns || at > t->s[t->ns - 1].t)) { t->used = false; continue; }
        n++;
    }
    return n;
}
// (s290 netshotsstuck, the receiver's cap) his arrows still (NSF_STILL) for cap_ms or more with no end from him (his game kept
// listing them: a stale read there [GUESS], or an arrow BotW never removes): ended here (nsp_reap frees them once played out);
// cap_ms 0 = none. -> how many; their ids in ids[0 .. max-1]
static inline int nsp_cap(NsPeer *P, uint64_t now, uint64_t cap_ms, uint16_t *ids, int max) {
    int n = 0;
    for (int i = 0; cap_ms && i < NST_TRACKS; i++) {
        NstTrack *t = &P->tr[i];
        if (!t->used || t->ended || !nshot_kind_arrow(t->kind) || !t->still_at || now < t->still_at || now - t->still_at < cap_ms) continue;
        t->ended = true, P->st.ended++, P->st.capped++;
        P->capped_id[P->capped_next] = t->id, P->capped_next = (P->capped_next + 1) % 8;
        if (ids && n < max) ids[n] = t->id;
        n++;
    }
    return n;
}
// the next boom of his to play at `at` (it came NET_INTERP_MS before: the bomb's last place has just been shown) -> true + *out
static inline bool nsp_boom_due(NsPeer *P, uint64_t at, NetBoom *out) {
    for (int i = 0; i < NSHOT_BOOMS * 2; i++)
        if (P->bq[i].due && at >= P->bq[i].at) {
            out->seq = P->bq[i].seq, out->kind = P->bq[i].kind;
            memcpy(out->pos, P->bq[i].pos, sizeof out->pos);
            P->bq[i].due = false;
            return true;
        }
    return false;
}

// ---- runes (NET_RUNES): what a friend holds with Magnesis / stopped with Stasis, by map HashId ----
// -> NRF_MAG / NRF_STOP when r (a friend's newest, fresh) names this object, 0 = not his
static inline int nrune_holds(const NetRunes *r, uint32_t hashid) {
    if (!r || !hashid) return 0;
    return ((r->flags & NRF_MAG) && r->mag.hashid == hashid ? NRF_MAG : 0) | ((r->flags & NRF_STOP) && r->stop.hashid == hashid ? NRF_STOP : 0);
}

// sora_net_loot.h (s289 loot): shared loot for the room. User (2026-11-26, the "Shared loot + world" pick from Savage Wind PR #294):
// "Enemy drops and dropped items show up in every game and vanish for everyone when one player picks them up."
// Ported after Savage Wind PR #294 "Online mode" by byluki, MIT (runtime/online_actors.c: the loot list, the death watch, what falls
// near Link, the pickup rules, the actor requests, take_out and the GameData flag setter are theirs). The owner model, the
// duplicate rules and the wire are ours: we have no host, and every game has its own copy of each shared enemy.
// MULTIPLAYER IS MELTY ONLY: NET_LOOT rides the room's reliable events (sora_melty.cpp's default case); on UDP it does nothing.
//
// Today (VERIFIED from the code, s289): when a shared (placed, HashId) enemy dies, EVERY game drops its own loot: NET_EDMG replays
// the kill through each game's own damage pipeline (linkpos.c linkpos_edmg_apply: "life, flinch, death, drops"), an esync puppet
// at life 0 gets its brain back and runs its own death (linkpos_esync_block.inc), and killsync's re-kill on a reload runs the
// game's death again (sora_net_kills.h). That is the recorded Melty answer 1 "Everyone gets their own"; `set netloot 1` changes it.
//
// The rules (no host, no leader for live items):
//   * Every item on the room's list belongs to the game it came from (the top byte of its gid = that game's player id). That game
//     has the real one; the others make a copy where it lies (a real item, by name, with the game's own actor request).
//   * A shared enemy's drops belong to the game whose OWN hit killed it. Every other game removes its own drops of that death
//     (the death watch: items that came within LT_DEATH_R of the body (it follows the body while it is listed) for LT_DEATH_MS).
//     Who killed it (lt_death_kind): our own fresh kill on the kill list = ours; a friend's NET_EDMG kill of that HashId in the last
//     LT_REMOTE_KILL_MS, or the kill list has it as a friend's = his; our own kill from longer ago = killsync's re-kill on a reload
//     ("old": its drops were shared then). Two games that both saw their own kill (the same instant): the LOWER player id keeps
//     its drops, the other takes its back. A spawned enemy (HashId 0) lives in one game only: its drops are always that game's.
//     A friend's kill takes our drops away only when HIS game shares loot (its NLO_STATE said netloot 1; asked on each join and
//     when the key turns on): a friend with netloot 0 or an older build leaves every game its own drops of his kills, as today.
//     Only what is plainly the enemy's is removed or shared: monster parts (Item_Enemy_*), an animal's meat (Item_Meat_*), rupees
//     (PutRupee*) and what it held when it died (its weapon). Other new items near a death are left as they are, unshared. An item
//     that lay there before the death is never its drop (the first time this game saw it is before the death began).
//   * What falls to the ground near OUR Link that the map did not place (his pouch drop, his thrown weapon that landed, an ore /
//     crate / tree he broke) is ours once it lies still LT_SETTLE_MS (PR: DROP_SETTLE_NS), unless it was first seen within
//     LT_ENEMY_NEAR of an enemy (alive, dead or gone in the last LT_GONE_EN_MS: each game has its own copy of that enemy) or it is
//     a monster part / meat / rupee (an enemy's, whose death this game did not see). What left his hands, what he held over his
//     head and what he dropped from his pouch (first seen at his feet right after the menu) are his: never taken for an enemy's.
//   * Picked up anywhere (gone with that player's Link within LT_PICK_R, or now in his hand): TAKE -> gone in every game (the real
//     one in its own game too). Gone far from Link (an unload) = gone in that game only; it stays on the list for the others and
//     is made again there when that Link was LT_REMAKE_R away and comes back.
//   * An item the MAP placed (a HashId: a mushroom, a rupee, a weapon lying in the world) picked up here: MAPTAKE -> the others
//     take their copy out as the game takes out a picked object (woken, its map object marked gone, take_out) as it is or as it
//     loads, and its revival flag "<map>_<name>_<HashId>" is set the game's own way (GameDataMgr setBool, EventFlagONAction's path)
//     when the game has that flag, so the next load does not bring it back. A Blood Moon here forgets that session list.
//   * A late joiner: every game sends him its own items on the list; the room leader also sends the items whose game left (they
//     still lie in the other worlds). The same for the map items taken this session.
//   Race (GUESS, as the PR): two players picking up their copies in the same instant both get it (a pickup cannot wait).
//   With the other s289 PR #294 ports: netobjs breaks a friend's crate / ore / tree here with BotW's own step, so it drops its own
//   items here: no_apply calls net_loot_hush(pos) first and those are removed (his shared ones come). Our own break that netobjs
//   tells the room calls net_loot_broke_here(pos): what it drops is ours to share, rupees too. netshots draws our arrow /
//   thrown weapon in flight for the friends: one it still draws (netshots_local_flying) is not "lying still" yet, so the landed
//   weapon is shared once, after netshots hides it there.
//
// Live keys (DLL side, sora_net.h net_live_set):
//   set netloot 0|1     compiled default 0 = today exactly: nothing scanned, sent, made or deleted; a received NET_LOOT is
//                       dropped and counted. 1 = the rules above while in a Melty room with a live friend (driven frames,
//                       one look every LT_SCAN_MS; nothing while the pause menu / map is open).
//   set netlootlog 0|1  one line per event (a death and who it is, an item ours / removed / made here / picked up, a tie, a map
//                       item); "net diag" gives one [loot] line either way (with what a look costs, in us).
// Exe side: the (s289 loot) rows of sora_sites.h (loot_*, plus s289 objs' objs_set_mtx / objs_wake and s288's drop_request; 1.9.0
// only, 1.8.2 = 0 = netloot stays off there) need an EXE RELINK;
// an exe without them says so in the key's line and does nothing. Everything else is this DLL (hot reload).
//
// Offsets (1.9.0, from the PR; GUESS that 1.8.2 has the same: no 1.8.2 rows anyway): actor +0x60 its id, +0x64 its state (3 =
// being deleted, 1 = awake), +0x68 its proc unit (unit +0x40 the id, +0x48 the actor, +0x10 / +0x18 the list links), +0x398 its
// 3x4 matrix, +0x4d4 != 0 linked to someone (held), +0x958 a weapon's sheath / a bow's quiver link, +0x640 its map HashId (the
// PR's); the player's +0xb98 / +0xbb0 / +0xbc8 weapon / shield / bow links (unit, id), an enemy's weapon / shield the same way
// (PR Own.held). The HashId: OUR path
// *(u32*)(*(u64*)(actor+0x7c8)+0x58) (linkpos.c actor_hid, VERIFIED for enemies), the PR's +0x640 when ours is 0 (GUESS that they
// agree; netlootlog prints both for the first items). The actor list walk is the PR's all_actors without its last "n--" (that
// dropped the last unit of the first direction = the newest actor, a fresh drop: GUESS, a duplicate entry is harmless here).
//
// Pure core first (host test runtime/test/netloot_test.c); the glue (NET, LOG, sw_api, NETK) under NET_LOOT_GLUE, which sora_net.h
// defines before the include.
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sora_net_proto.h"

#define LT_LIST_MAX 96          // GUESS: items on the room's list at once (PR: ONLINE_LOOT); full = a new one stays unshared
#define LT_TOMB 128             // taken gids remembered: an ADD that comes after its TAKE (another sender's order) is dropped
#define LT_MAP_MAX 256          // map items taken this session (HashIds)
#define LT_DEATHS 32            // death watches at once (PR: 8; a reload's killsync re-kills come many at once)
#define LT_BEFORE 48            // items already near a death when it was seen (PR: before[48])
#define LT_HELD 8               // what was held near it, nearest to it, when it died (its weapon)
#define LT_DEATH_MS 10000       // PR: 300 frames = 10 s
#define LT_DEATH_R 8.0f         // PR: 6 m (the scan) / 8 m (before); 8 for both (a ragdoll rolls before its loot comes)
#define LT_REMOTE_KILL_MS 5000  // GUESS: a friend's EDMG kill this recent makes a death here his
#define LT_OLD_KILL_MS 4000     // GUESS: our own kill on the kill list older than this = killsync's re-kill on a reload
#define LT_TIE_MS 15000         // GUESS: drops of the same enemy announced this close together are one death's (the tie rule)
#define LT_TIE_WAIT_MS 3000     // GUESS: a higher id's drops of a tie wait this long (at least) before they are made here
#define LT_SETTLE_MS 400        // PR: DROP_SETTLE_NS
#define LT_NEAR_DROP 15.0f      // PR: what falls near our Link (15 m)
#define LT_SEEN_R 250.0f        // dynamic items further than this are not looked at
#define LT_ENEMY_NEAR 4.0f      // GUESS: first seen this close to an enemy = that enemy's (each game has its own copy)
#define LT_GONE_EN_MS 4000      // GUESS: an enemy gone from the list (its body gone in smoke) still counts this long, where it was
#define LT_MAKE_R 150.0f        // PR: theirs made within 150 m of our Link
#define LT_REMAKE_R 180.0f      // a copy gone here (unloaded) is made again after Link was this far from it and came back
#define LT_PICK_R 4.0f          // PR: gone with Link within 4 m = picked up
#define LT_MAP_R 18.0f          // PR: map objects watched within 18 m
#define LT_MAP_PICK_R 3.5f      // PR: a map item gone with Link within 3.5 m = picked up
#define LT_JUMP_R 30.0f         // PR: Link moved this far between two looks = a load / a warp (what is there now is not new)
#define LT_HUSH_R 5.0f          // PR: DROP_HUSH: what our game breaks for a friend drops its own here: removed (his comes)
#define LT_HUSH_MS 5000         // PR: DROP_HUSH_NS
#define LT_HUSHES 8
#define LT_BROKE_BACK_MS 1500   // GUESS: objs tells our own break up to ~250 ms (its look) after the drops came: 1.5 s back is ample
#define LT_MENU_MS 1500         // GUESS: a new item at Link's feet this soon after the pause menu = dropped from his pouch
#define LT_AT_LINK_R 1.5f       // (horizontal) "at his feet" / over his head (PR: the held materials, 1.5 m)
#define LT_RING 16              // Link's own items remembered (what left his hands, what he dropped from his pouch)
#define LT_SEEN 512             // dynamic items remembered (more than a look keeps: LT_ITEMS)
#define LT_LIST_AGE_MS (30ull * 60 * 1000)  // GUESS: nobody took it in 30 min: off the list here (it stays where it lies)
#define LT_SCAN_MS 150          // GUESS: one look at the world this often (PR: every 3 / 9 frames)
#define LT_MAKE_TRIES 5         // PR: a copy the game didn't make is asked for again, 5 times
#define LT_ASK_PER_LOOK 8       // GUESS: actor requests per look (a join's 50 items load over a few looks)

// ---- names (PR is_loot / players_own_item; map items: PR map_item, plus weapons lying in the world) ----
static inline bool lt_players_own(const char *n) { return !strcmp(n, "Item_Conductor") || !strncmp(n, "Item_Magnetglove", 16); }
static inline bool lt_is_loot(const char *n) {
    return n && (!strncmp(n, "Item_", 5) || !strncmp(n, "Weapon_", 7) || !strncmp(n, "PutRupee", 8)) && !lt_players_own(n);
}
// plainly an enemy's drop: its parts, an animal's meat, rupees, or what it held when it died (GUESS: the names)
static inline bool lt_drop_class(const char *n, bool held_by_it) {
    return held_by_it || (n && (!strncmp(n, "Item_Enemy_", 11) || !strncmp(n, "Item_Meat_", 10) || !strncmp(n, "PutRupee", 8)));
}

// ---- ids: gid = the game's player id (top byte) + its own counter (never 0; the counter starts random at net on) ----
static inline uint32_t lt_gid_next(uint8_t me, uint32_t *ctr) {
    *ctr = (*ctr + 1) & 0xffffffu;
    if (!*ctr) *ctr = 1;
    return (uint32_t)me << 24 | *ctr;
}
static inline uint8_t lt_gid_origin(uint32_t gid) { return (uint8_t)(gid >> 24); }
static inline int lt_ring_next(int n, int cap) { return n >= 2 * cap ? cap + (n + 1) % cap : n + 1; }  // (a ring's count: never overflows)

// ---- the list (one per game; every game ends with the same entries: ADDs and TAKEs go to everyone) ----
enum { LTE_WAIT = 0, LTE_ASKED = 1, LTE_HERE = 2, LTE_GONE = 3 };  // to make here / asked for / here (ours or our copy) / gone here
typedef struct {
    NetLootAdd w;       // as on the wire (w.gid's top byte = its game)
    uint64_t at;        // when it came on this game's list (ms)
    uint64_t actor;     // its actor here (ours: the real one; theirs: our copy), 0 = none
    uint64_t unit;      // (watched as the game's BaseProcLink does: its proc unit and id)
    uint32_t id;
    float here[3];      // where it is now (pickups are judged by it)
    int hix;            // its request's handle (ring slot) while asked, -1 = none
    uint8_t state, tries;
    uint64_t retry_at, made_at;
} LtEnt;
typedef struct {
    LtEnt e[LT_LIST_MAX];
    int n;
    uint32_t tomb[LT_TOMB];
    int ntomb;  // (a ring: lt_ring_next)
} LtList;
enum { LTR_FULL = -2, LTR_BAD = -1, LTR_DUP = 0, LTR_NEW = 1, LTR_TOMB = 2 };
static inline int lt_find(const LtList *L, uint32_t gid) {
    for (int i = 0; gid && i < L->n; i++)
        if (L->e[i].w.gid == gid) return i;
    return -1;
}
static inline bool lt_tombed(const LtList *L, uint32_t gid) {
    int c = L->ntomb < LT_TOMB ? L->ntomb : LT_TOMB;
    for (int i = 0; gid && i < c; i++)
        if (L->tomb[i] == gid) return true;
    return false;
}
static inline void lt_tomb(LtList *L, uint32_t gid) {
    if (!gid || lt_tombed(L, gid)) return;
    L->tomb[L->ntomb % LT_TOMB] = gid;
    L->ntomb = lt_ring_next(L->ntomb, LT_TOMB);
}
// -> LTR_NEW (*at = its index), LTR_DUP (*at = the one there), LTR_TOMB (taken before: dropped), LTR_FULL, LTR_BAD (gid 0 / no name)
static inline int lt_add(LtList *L, const NetLootAdd *w, uint64_t now, int *at) {
    if (at) *at = -1;
    if (!w || !w->gid || !w->name[0]) return LTR_BAD;
    int i = lt_find(L, w->gid);
    if (i >= 0) {
        if (at) *at = i;
        return LTR_DUP;
    }
    if (lt_tombed(L, w->gid)) return LTR_TOMB;
    if (L->n >= LT_LIST_MAX) return LTR_FULL;
    LtEnt *e = &L->e[L->n];
    memset(e, 0, sizeof *e);
    e->w = *w, e->at = now ? now : 1, e->hix = -1;
    memcpy(e->here, w->pos, sizeof e->here);
    if (at) *at = L->n;
    L->n++;
    return LTR_NEW;
}
// off the list (its gid remembered: a late ADD of it is dropped); true = it was on it (copied to *out)
static inline bool lt_take(LtList *L, uint32_t gid, LtEnt *out) {
    lt_tomb(L, gid);
    int i = lt_find(L, gid);
    if (i < 0) return false;
    if (out) *out = L->e[i];
    L->e[i] = L->e[--L->n];
    return true;
}
// up to max entries nobody took for LT_LIST_AGE_MS leave the list (no TAKE, no tomb: where they lie they stay); -> how many
static inline int lt_expire(LtList *L, uint64_t now, LtEnt *out, int max) {
    int k = 0;
    for (int i = 0; i < L->n && k < max; i++)
        if (now > L->e[i].at && now - L->e[i].at > LT_LIST_AGE_MS) {
            if (out) out[k] = L->e[i];
            k++;
            L->e[i--] = L->e[--L->n];
        }
    return k;
}
// an enemy drop of src that came on the list in the last LT_TIE_MS from a LOWER player id than ours (the tie rule: his are the room's)
static inline bool lt_has_src_below(const LtList *L, uint32_t src, uint8_t me, uint64_t now) {
    for (int i = 0; src && i < L->n; i++) {
        const LtEnt *e = &L->e[i];
        if (e->w.src == src && e->w.kind == NLK_DROP && lt_gid_origin(e->w.gid) < me && now >= e->at && now - e->at <= LT_TIE_MS) return true;
    }
    return false;
}
// a late joiner gets this entry from us: ours, or (the room leader only) one whose game is not in the room any more
static inline bool lt_join_sends(uint8_t origin, uint8_t me, bool origin_here, bool leader) { return origin == me || (!origin_here && leader); }

// ---- deaths ----
enum { LTD_OWN = 0, LTD_REMOTE = 1, LTD_OLD = 2 };
// a friend's kill here: our drops of it stay ours (unshared, as before netloot) when the killer's game does not share loot (it said
// netloot 0, or an older build never said 1). killer_known: the kill's NET_EDMG named him; else (the kill list's) any friend sharing
static inline bool lt_remote_keeps(bool killer_known, bool killer_shares, bool any_shares) { return killer_known ? !killer_shares : !any_shares; }
// a death here of hid: remote_at = when a friend's EDMG kill of it came (0 = none); list = the kill list has it: 0 no, 1 as our own
// kill, 2 as a friend's (EDMG / catch-up); kt = when that kill was (kt_known false = not known / very old)
static inline int lt_death_kind(uint32_t hid, uint64_t now, uint64_t remote_at, int list, bool kt_known, int64_t kt) {
    if (!hid) return LTD_OWN;  // (a spawned enemy lives in this game only)
    if (list == 1 && kt_known && (int64_t)now - kt <= (int64_t)LT_OLD_KILL_MS) return LTD_OWN;  // (our own hit killed it: a friend's
                                                                                                // packet of the same instant = the tie rule's)
    if (remote_at && now >= remote_at && now - remote_at <= LT_REMOTE_KILL_MS) return LTD_REMOTE;
    if (list == 2) return LTD_REMOTE;
    if (list == 1) return LTD_OLD;
    return LTD_OWN;
}
typedef struct {
    float pos[3];
    uint64_t actor, start, until;  // (the dead enemy: its watch follows its body while it is listed)
    uint32_t hid, before[LT_BEFORE], held[LT_HELD];
    uint8_t kind;
    int nbefore, nheld;
} LtDeath;
static inline float lt_d2(const float a[3], const float b[3]) {
    float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return x * x + y * y + z * z;
}
static inline int lt_death_start(LtDeath *d, int nd, const float pos[3], uint64_t actor, uint32_t hid, int kind, uint64_t now) {
    int k = -1;
    for (int i = 0; i < nd && k < 0; i++)
        if (!d[i].until || d[i].until <= now) k = i;
    if (k < 0) {  // (all busy: the one that ends first goes)
        k = 0;
        for (int i = 1; i < nd; i++)
            if (d[i].until < d[k].until) k = i;
    }
    memset(&d[k], 0, sizeof d[k]);
    memcpy(d[k].pos, pos, sizeof d[k].pos);
    d[k].actor = actor, d[k].start = now, d[k].until = now + LT_DEATH_MS, d[k].hid = hid, d[k].kind = (uint8_t)kind;
    return k;
}
static inline void lt_death_before(LtDeath *d, uint32_t id) {
    if (d->nbefore < LT_BEFORE) d->before[d->nbefore++] = id;
}
static inline void lt_death_held_add(LtDeath *d, uint32_t id) {
    if (d->nheld < LT_HELD) d->held[d->nheld++] = id;
}
static inline bool lt_death_is_held(const LtDeath *d, uint32_t id) {
    for (int i = 0; i < d->nheld; i++)
        if (d->held[i] == id) return true;
    return false;
}
// the dead body moved (a ragdoll): its running watch moves with it
static inline void lt_death_follow(LtDeath *d, int nd, uint64_t actor, const float pos[3], uint64_t now) {
    for (int i = 0; actor && i < nd; i++)
        if (d[i].actor == actor && d[i].until > now) memcpy(d[i].pos, pos, sizeof d[i].pos);
}
// the watch an item (id at p, not held now, first seen here at `first` ms) belongs to: one whose enemy held it when it died, else
// the nearest within LT_DEATH_R whose death began before the item came; -1 = none (not near a death, or it lay there before)
static inline int lt_death_match(const LtDeath *d, int nd, const float p[3], uint32_t id, uint64_t first, uint64_t now) {
    int best = -1;
    float bd = 0;
    for (int i = 0; i < nd; i++) {
        if (!d[i].until || now >= d[i].until) continue;
        float q = lt_d2(d[i].pos, p);
        if (q > LT_DEATH_R * LT_DEATH_R) continue;
        bool held = lt_death_is_held(&d[i], id), old = false;
        if (!held && first < d[i].start) continue;
        for (int b = 0; b < d[i].nbefore && !old; b++) old = d[i].before[b] == id;
        if (old) continue;
        float rank = held ? -1.0f : q;  // (what it held is its own, whatever is nearer)
        if (best < 0 || rank < bd) best = i, bd = rank;
    }
    return best;
}
// our own death watch of hid still running: when it ends (0 = none)
static inline uint64_t lt_death_own_until(const LtDeath *d, int nd, uint32_t hid, uint64_t now) {
    for (int i = 0; hid && i < nd; i++)
        if (d[i].hid == hid && d[i].kind == LTD_OWN && d[i].until > now) return d[i].until;
    return 0;
}
// a friend's drops of an enemy we also have (or will have) drops of: the lower player id keeps its drops
enum { LTT_NONE = 0, LTT_RETRACT_OURS = 1, LTT_DELAY_THEIRS = 2 };
static inline int lt_tie(uint8_t me, uint8_t them, bool we_have) { return !we_have ? LTT_NONE : them < me ? LTT_RETRACT_OURS : LTT_DELAY_THEIRS; }

// ---- what falls near Link (PR drops_update): first seen -> watched (or "old"); still for LT_SETTLE_MS -> ours, once ----
enum { LTS_FREE = 0, LTS_OLD = 1, LTS_WATCH = 2, LTS_JUDGED = 3, LTS_MINED = 4, LTS_HUSH = 5 };  // (JUDGED: a death watch took it: never
// looked at again; MINED: it settled and went on the list as ours (or the list was full): never again, a break near it too; HUSH: a
// duplicate of a friend's pushed one that broke here while it followed his (objs): removed at the next look)
typedef struct { uint32_t id; uint8_t st; uint64_t first, t, last; float pos[3]; } LtSeen;
static inline int lt_seen_find(const LtSeen *s, int n, uint32_t id) {
    for (int i = 0; i < n; i++)
        if (s[i].st && s[i].id == id) return i;
    return -1;
}
// a new one: watched (watch) or old (never ours); first = when this game first saw it (0 = before our first look). A full table
// forgets the one seen least lately (one seen every look stays).
static inline int lt_seen_add(LtSeen *s, int n, uint32_t id, const float p[3], bool watch, uint64_t first, uint64_t now) {
    int k = -1;
    for (int i = 0; i < n && k < 0; i++)
        if (!s[i].st) k = i;
    if (k < 0) {
        k = 0;
        for (int i = 1; i < n; i++)
            if (s[i].last < s[k].last) k = i;
    }
    s[k].id = id, s[k].st = watch ? LTS_WATCH : LTS_OLD, s[k].first = first, s[k].t = now, s[k].last = now;
    memcpy(s[k].pos, p, sizeof s[k].pos);
    return k;
}
// this look at a seen one: -> true once, when a watched one has lain still LT_SETTLE_MS (moved > 5 cm or held = the clock restarts)
static inline bool lt_seen_step(LtSeen *s, const float p[3], bool held, uint64_t now) {
    s->last = now;
    if (s->st != LTS_WATCH) return false;
    if (held || lt_d2(s->pos, p) > 0.05f * 0.05f) {
        memcpy(s->pos, p, sizeof s->pos);
        s->t = now;
        return false;
    }
    if (now - s->t < LT_SETTLE_MS) return false;
    s->st = LTS_MINED;
    return true;
}
// a new item near Link can be "fell near Link" (ours): his own (the ring), or not an enemy's kind and not first seen by an enemy
static inline bool lt_fell_ok(const char *name, bool his, bool near_enemy) { return his || (!lt_drop_class(name, false) && !near_enemy); }

// ---- Link's own items (what left his hands, what he held over his head, what he dropped from his pouch): by actor id ----
typedef struct { uint32_t id[LT_RING]; uint8_t thrown[LT_RING]; int n; } LtRing;
static inline int lt_ring_find(const LtRing *r, uint32_t id) {
    int c = r->n < LT_RING ? r->n : LT_RING;
    for (int i = 0; id && i < c; i++)
        if (r->id[i] == id) return i;
    return -1;
}
static inline int lt_ring_add(LtRing *r, uint32_t id, bool thrown) {
    int i = lt_ring_find(r, id);
    if (i < 0) {
        i = r->n % LT_RING;
        r->id[i] = id, r->thrown[i] = 0;
        r->n = lt_ring_next(r->n, LT_RING);
    }
    if (thrown) r->thrown[i] = 1;
    return i;
}
static inline bool lt_at_link(const float p[3], const float link[3]) {  // at his feet / in his hands (GUESS: 1.5 m, -1..+2.5 m)
    float x = p[0] - link[0], y = p[1] - link[1], z = p[2] - link[2];
    return x * x + z * z < LT_AT_LINK_R * LT_AT_LINK_R && y > -1.0f && y < 2.5f;
}

// ---- pickups ----
// one of the list's actors here: in Link's hand = picked up here; gone with Link within LT_PICK_R = picked up here; else gone here only
enum { LTP_THERE = 0, LTP_PICKED = 1, LTP_GONE = 2 };
static inline int lt_pick(bool alive, bool in_hand, float d2_link) {
    if (in_hand) return LTP_PICKED;
    if (alive) return LTP_THERE;
    return d2_link < LT_PICK_R * LT_PICK_R ? LTP_PICKED : LTP_GONE;
}
// a watched map item: in his hand, or gone with Link within LT_MAP_PICK_R (not right after a load / warp) = picked up
static inline bool lt_map_picked(bool alive, bool in_hand, float d2_link, bool jumped) {
    if (alive) return in_hand;
    return !jumped && d2_link < LT_MAP_PICK_R * LT_MAP_PICK_R;
}

// ---- map items taken this session (HashIds, the game that took each, its name for the flag) ----
typedef struct { uint32_t hid[LT_MAP_MAX]; uint8_t origin[LT_MAP_MAX]; char name[LT_MAP_MAX][NET_LOOT_NAME_MAX + 1]; int n; } LtMapList;
static inline int lt_map_find(const LtMapList *m, uint32_t hid) {
    for (int i = 0; hid && i < m->n; i++)
        if (m->hid[i] == hid) return i;
    return -1;
}
static inline int lt_map_add(LtMapList *m, uint32_t hid, uint8_t origin, const char *name) {  // 1 new, 0 known, -1 full / hid 0
    if (!hid) return -1;
    if (lt_map_find(m, hid) >= 0) return 0;
    if (m->n >= LT_MAP_MAX) return -1;
    m->hid[m->n] = hid, m->origin[m->n] = origin;
    snprintf(m->name[m->n], sizeof m->name[m->n], "%s", name ? name : "");
    m->n++;
    return 1;
}

// ---- where our game breaks something for a friend (objs' break_here): what drops there is a duplicate (PR drop_hush) ----
typedef struct { float pos[3]; uint64_t until; } LtHush;
static inline void lt_hush_add(LtHush *h, int n, const float p[3], uint64_t now) {
    int k = 0;
    for (int i = 1; i < n; i++)
        if (h[i].until < h[k].until) k = i;
    memcpy(h[k].pos, p, sizeof h[k].pos);
    h[k].until = now + LT_HUSH_MS;
}
static inline bool lt_hushed(const LtHush *h, int n, const float p[3], uint64_t now) {
    for (int i = 0; i < n; i++)
        if (now < h[i].until && lt_d2(h[i].pos, p) < LT_HUSH_R * LT_HUSH_R) return true;
    return false;
}
// (s289 loot + objs) OUR own break, told to the room (objs: "broke here: told the room"): the friends' copies break with objs and
// hush their own drops, so what it dropped here must be shared, rupees too (a rupee is an enemy's kind: never "fell near Link" on
// its own). Items first seen within LT_HUSH_R of it from LT_BROKE_BACK_MS before (objs looks every 250 ms: the drops can come
// first) that were left "old" are watched now, their settle clock restarted. -> how many. (Later ones: lt_dynamic's zone check.)
static inline int lt_seen_broke(LtSeen *s, int n, const float p[3], uint64_t now) {
    int k = 0;
    for (int i = 0; i < n; i++)
        if (s[i].st == LTS_OLD && s[i].first && s[i].first <= now && now - s[i].first <= LT_BROKE_BACK_MS && lt_d2(s[i].pos, p) < LT_HUSH_R * LT_HUSH_R)
            s[i].st = LTS_WATCH, s[i].t = now, k++;
    return k;
}
// (s289 loot + objs fix 2) a friend's pushed one broke HERE while it followed his (objs: not our break, the room is not told): what
// it dropped here duplicates his break's (his game shares those) or, when only ours broke, his to come. Items first seen within
// LT_HUSH_R of it from LT_BROKE_BACK_MS before, not ours on the list yet (old or watched), are removed at the next look. -> how many
static inline int lt_seen_hush_back(LtSeen *s, int n, const float p[3], uint64_t now) {
    int k = 0;
    for (int i = 0; i < n; i++)
        if ((s[i].st == LTS_OLD || s[i].st == LTS_WATCH) && s[i].first && s[i].first <= now && now - s[i].first <= LT_BROKE_BACK_MS &&
            lt_d2(s[i].pos, p) < LT_HUSH_R * LT_HUSH_R)
            s[i].st = LTS_HUSH, k++;
    return k;
}

// ---- where a copy goes: its rotation as it lies there (rows x 32767), or upright a little above (PR) ----
static inline void lt_rot_pack(const float m[12], int16_t q[9]) {
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            float v = m[4 * r + c];
            v = !isfinite(v) ? 0.0f : v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
            q[3 * r + c] = (int16_t)lrintf(v * 32767.0f);
        }
}
static inline void lt_place_mtx(const NetLootAdd *a, float m[12]) {
    bool rot = false;
    for (int j = 0; j < 9; j++) rot = rot || a->rot[j] != 0;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) m[4 * r + c] = rot ? a->rot[3 * r + c] / 32767.0f : (r == c ? 1.0f : 0.0f);
        m[4 * r + 3] = a->pos[r] + (r == 1 ? (rot ? 0.02f : 0.2f) : 0.0f);
    }
}

// ---- the revival flag of a map object (PR: "<map type>_<actor name>_<HashId decimal>", its GameData hash = crc32 of it) ----
static const char *const LT_FLAG_MAPS[4] = {"MainField", "AocField", "MainFieldDungeon", "CDungeon"};  // (PR mark_dead's list)
static inline uint32_t lt_crc32(const char *s) {
    uint32_t h = 0xFFFFFFFFu;
    for (; *s; s++) {
        h ^= (uint8_t)*s;
        for (int b = 0; b < 8; b++) h = h >> 1 ^ (0xEDB88320u & (0u - (h & 1)));
    }
    return ~h;
}
static inline int lt_flag_name(char *out, size_t cap, const char *map, const char *name, uint32_t hid) {
    return snprintf(out, cap, "%s_%s_%u", map, name, hid);
}
// the bool flag with hash h in GameDataMgr's bool buffer (PR gdt_value: buf +8 the count, +0x10 an array of pointers to the
// flags, sorted by hash; a flag +8 its definition (its name's crc32 at +0xc), +0x10 its value) -> the value's address, 0 = none.
// Reads only (rd = the exe's sw_api read); an unreadable step = none.
typedef bool (*LtRead)(uint64_t a, void *out, size_t n);
static inline bool lt_gdt_hash_at(LtRead rd, uint64_t arr, uint32_t i, uint64_t *f, uint32_t *h) {
    uint64_t pr = 0;
    return rd(arr + 8ull * i, f, 8) && *f && rd(*f + 8, &pr, 8) && pr && rd(pr + 0xc, h, 4);
}
static inline uint64_t lt_gdt_find(LtRead rd, uint64_t buf, uint32_t h) {
    uint32_t n = 0, mh = 0;
    uint64_t arr = 0, f = 0;
    if (!rd || !buf || !rd(buf + 8, &n, 4) || !rd(buf + 0x10, &arr, 8) || !n || n > 200000 || !arr) return 0;
    uint32_t lo = 0, hi = n;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (!lt_gdt_hash_at(rd, arr, mid, &f, &mh)) return 0;
        if (mh < h) lo = mid + 1;
        else hi = mid;
    }
    if (lo >= n || !lt_gdt_hash_at(rd, arr, lo, &f, &mh) || mh != h) return 0;
    return f + 0x10;
}

#ifdef NET_LOOT_GLUE
// ===================== GLUE (sora_net.h: NET, PEER_LIVE, net_dests, net_send_all, g_net_loop, LOG; NETK / ks_find from
// sora_net_kills.h; the room's sora_melty_connected / sora_melty_story_leader; the exe's sw_api + linkpos_enemies) =====================
#include "sw_api.h"

static bool g_net_loot = true;  // live "set netloot 0|1". (s290: passed the two-copy lab, sb/s290/LAB_RESULTS.md; the user: "Flip the passes ON" -> default ON). The live "on" step's work (lt_bind, its line, the join send) is done by net_loot_tick's first tick in a net session (NETL.said) and its joined[] edges
static bool g_net_loot_log;  // live "set netlootlog"

#define LT_ACTORS 8192   // PR: MAX_ACTORS
#define LT_ITEMS 384     // loot actors kept per look (dynamic within LT_SEEN_R; map ones near Link or taken by a friend)
#define LT_HANDLES 256   // actor requests: a ring, a slot used again only much later (PR: HANDLES)
#define LT_ENEMIES 128   // linkpos_enemies' copy
#define LT_GONE_EN 32    // enemies gone from the list lately (where they were)
#define LT_MAPW 64       // map items watched near Link
#define LT_PEEK 32       // friends' EDMG kills remembered
#define LT_OWNSRC 16     // our recent enemy drops (the tie rule)
#define LT_FLAGQ 32      // map flags waiting for a driven frame
#define LT_KILLQ 64      // actors to delete on the next driven frame (a receive never calls the game)
#define LT_OUTS 32       // map items taken out lately (not asked again every look)

static const char *const LTD_TXT[3] = {"our kill: its drops are the room's", "a friend's kill: our drops of it are removed",
                                       "our old kill again (a reload): our drops of it are removed"};
static const char *const LT_KIND_TXT[4] = {"?", "enemy drop", "fell", "thrown"};

typedef struct { uint64_t actor; uint32_t id, hid; float pos[3]; bool held; char name[NET_LOOT_NAME_MAX + 1]; float m[12]; } LtItem;
// A request's BaseProcHandle (unit, flag) and the name it was made with: the unit keeps the handle's address until the actor is
// taken or the handle released (PR Handle). Allocated once and never freed: a DLL hot reload with a request in flight leaves
// the game a valid handle (the new DLL starts its own block).
typedef struct { uint64_t unit, flag; } LtHandle;
typedef struct { LtHandle h[LT_HANDLES]; char name[LT_HANDLES][NET_LOOT_NAME_MAX + 1]; } LtHandles;
typedef struct { uint64_t actor; uint32_t hid, held_id[2]; float pos[3]; bool alive; } LtEnemy;  // (held: its weapon / shield, PR Own.held)
// (s289 shots merge) the shots helper's netshots_local_flying (sora_net_shots_game.h, which sora.c includes later through
// sora_net_avatar.h; SORA_WORLD_HOOKS = that sora.c build): 1 = one of our shots it still draws in flight for the friends. The
// host tests have no shots game side: never flying there.
#ifdef SORA_WORLD_HOOKS
static int netshots_local_flying(uint64_t actor);
#define LT_SHOT_FLYING(a) netshots_local_flying(a)
#else
#define LT_SHOT_FLYING(a) 0
#endif

static struct {
    bool tried, sites, said, said_udp, en_base;
    const SwApi *api;
    uint64_t s_req, s_take, s_rel, s_mtx, s_del, s_creator, s_heap, s_wake, s_out, s_gone, s_pinfo, s_gdt, s_gdt_handle, s_gdt_set, s_pause;
    int (*enemies)(uint64_t *, float (*)[3], int32_t *, int);  // the exe's linkpos_enemies
    int (*moons_fn)(void);                                     // sora_world_bloodmoon_count (NULL = no Blood Moon clear)
    LtHandles *H;
    bool hbusy[LT_HANDLES];
    int hnext;
    LtList L;
    LtMapList M;
    LtDeath death[LT_DEATHS];
    LtSeen seen[LT_SEEN];
    LtRing ring;
    LtHush hush[LT_HUSHES];
    LtHush broke[LT_HUSHES];  // (our own breaks told to the room: what drops there is ours)
    struct { uint64_t actor, unit; uint32_t id, hid; float pos[3]; char name[NET_LOOT_NAME_MAX + 1]; } mapw[LT_MAPW];
    struct { uint32_t id; uint64_t at; } outs[LT_OUTS];
    int nouts;
    LtEnemy en[LT_ENEMIES], en2[LT_ENEMIES];
    int nen;
    struct { float pos[3]; uint64_t at; } gone[LT_GONE_EN];
    int ngone;
    uint64_t ea[LT_ENEMIES];
    float ep[LT_ENEMIES][3];
    int32_t el[LT_ENEMIES];
    struct { uint32_t hid; uint64_t at; uint8_t pl; } peek[LT_PEEK];  // (pl: the killer's player id, from its NET_EDMG header)
    struct { uint32_t hid; uint64_t at; } own_src[LT_OWNSRC];
    bool share[NET_MAX_PEERS], state_due;  // (slot k's game said netloot 1 (NLO_STATE); state_due: ours goes out next tick)
    uint64_t share_j[NET_MAX_PEERS];       // (the slot's joined_ms + 1 when it said so: someone new in the slot = not known)
    struct { uint32_t hid; char name[NET_LOOT_NAME_MAX + 1]; } flagq[LT_FLAGQ];
    int nflagq;
    struct { uint64_t actor, unit; uint32_t id; } killq[LT_KILLQ];
    int nkillq;
    int orphan[LT_HANDLES], norphan;  // requests whose entry went: what they make is deleted once made (PR orphans)
    uint64_t pend[LT_HANDLES];        // the actors our requests in flight made already (never taken for new items)
    int npend;
    LtItem item[LT_ITEMS], prev[LT_ITEMS];
    int nitem, nprev;
    uint64_t actors[LT_ACTORS];
    uint64_t carried[5], carried_prev[5];
    uint32_t ctr;
    uint16_t seq;
    bool base, have_link, said_player;
    int said_hid;
    float link_was[3];
    uint64_t next_scan, next_moon, menu_at, joined[NET_MAX_PEERS];
    bool join_due;
    int moons;
    bool moons_known;
    NetLootAdd addq[NET_LOOT_ADD_MAX];
    int naddq;
    uint32_t takeq[NET_LOOT_TAKE_MAX];
    int ntakeq;
    NetLootMap mapq[NET_LOOT_MAP_MAX];
    int nmapq;
    uint32_t cost_last, cost_max, cost_n;
    uint64_t cost_sum;
    struct { uint32_t scans, deaths[3], ours, dups, left, hushed, fell, thrown, add_tx, add_rx, add_dup, add_tomb, full, take_tx, take_rx,
             take_hit, map_tx, map_rx, map_out, flags_ok, flags_had, flags_none, flags_fail, flagq_full, made, make_fail, lost_made, picked,
             gone_here, ties_lost, ties_wait, rx_bad, rx_off, pkt_tx, unsent, expired, moon_clr, not_frame, paused, killq_full,
             no_player, hid_alt, flying, broke, broke_items, kept, state_rx, follow, follow_items; } st;
} NETL;

static bool lt_rd(uint64_t a, void *out, size_t n) { return a && NETL.api && NETL.api->read(a, out, n); }
static uint64_t lt_q(uint64_t a) { uint64_t v = 0; return lt_rd(a, &v, 8) ? v : 0; }
static uint32_t lt_d(uint64_t a) { uint32_t v = 0; return lt_rd(a, &v, 4) ? v : 0; }
static uint8_t lt_b(uint64_t a, uint8_t dflt) { uint8_t v = dflt; return lt_rd(a, &v, 1) ? v : dflt; }

// once, on the first use (key 0 never gets here)
static void lt_bind(void) {
    if (NETL.tried) return;
    NETL.tried = true;
    HMODULE exe = GetModuleHandleA(NULL);
    const SwApi *(*get)(uint32_t) = (const SwApi *(*)(uint32_t))(void *)GetProcAddress(exe, SW_API_EXPORT);
    const SwApi *a = NETL.api = get ? get(1) : NULL;
    NETL.enemies = (int (*)(uint64_t *, float (*)[3], int32_t *, int))(void *)GetProcAddress(exe, "linkpos_enemies");
#ifdef SORA_WORLD_HOOKS  // (sora.c: sora_world.h)
    NETL.moons_fn = sora_world_bloodmoon_count;
#endif
    if (SW_HAS(a, site) && SW_HAS(a, call) && SW_HAS(a, read) && SW_HAS(a, in_frame) && SW_HAS(a, actor_name)) {
        NETL.s_req = a->site("loot_request"), NETL.s_take = a->site("loot_handle_take"), NETL.s_rel = a->site("loot_handle_release");
        NETL.s_mtx = a->site("objs_set_mtx"), NETL.s_del = a->site("loot_delete"), NETL.s_creator = a->site("loot_creator_got");  // (s289 objs'
        NETL.s_heap = a->site("loot_heap_got"), NETL.s_wake = a->site("objs_wake"), NETL.s_out = a->site("loot_take_out");      //  rows: the same functions)
        NETL.s_gone = a->site("drop_request");  // (s288 logchop's row = the PR's FN_MAPOBJ_GONE: the map object marked gone)
        NETL.s_gdt_handle = a->site("loot_gdt_handle"), NETL.s_gdt_set = a->site("loot_gdt_set_bool");
        NETL.s_pinfo = a->site("player_info"), NETL.s_gdt = a->site("gdt_mgr"), NETL.s_pause = a->site("pause_state");
    }
    if (NETL.s_req) NETL.H = (LtHandles *)calloc(1, sizeof(LtHandles));  // (the CRT's heap: outlives this DLL; never freed)
    NETL.sites = NETL.s_req && NETL.s_take && NETL.s_rel && NETL.s_mtx && NETL.s_del && NETL.s_creator && NETL.s_heap && NETL.s_wake &&
                 NETL.s_out && NETL.s_pinfo && NETL.enemies && NETL.H;
}
static const char *lt_sites_txt(void) {
    return NETL.sites ? "sites ok"
           : !NETL.api ? "this exe has no sw_api: netloot does nothing"
           : !NETL.s_req ? "NO loot_* sites rows: NEEDS EXE RELINK with the s289 loot sora_sites.h (game 1.9.0 only; 1.8.2 has no rows): "
                           "netloot does nothing"
                         : "a site / linkpos_enemies / memory missing: netloot does nothing";
}

// ---- actors (all reads through the exe's sw_api: never faults) ----
static void lt_watch_of(uint64_t a, uint64_t *unit, uint32_t *id) { *unit = lt_q(a + 0x68), *id = lt_d(a + 0x60); }
static bool lt_alive(uint64_t actor, uint64_t unit, uint32_t id) {
    if (!actor || !unit || lt_b(actor + 0x64, 3) == 3) return false;
    return lt_d(unit + 0x40) == id && lt_q(unit + 0x48) == actor;
}
// alive, or the same actor moved to another proc unit (woken: PR alive_rewatch): then watched there
static bool lt_alive_w(uint64_t actor, uint64_t *unit, uint32_t *id) {
    if (lt_alive(actor, *unit, *id)) return true;
    if (!actor || lt_d(actor + 0x60) != *id || lt_b(actor + 0x64, 3) == 3) return false;
    uint64_t u = lt_q(actor + 0x68);
    if (!u || lt_q(u + 0x48) != actor || lt_d(u + 0x40) != *id) return false;
    *unit = u;
    return true;
}
static uint64_t lt_linked(uint64_t at) {  // a BaseProcLink (unit, id) -> its actor while it lives (PR linked)
    uint64_t u = lt_q(at);
    uint32_t id = lt_d(at + 8);
    if (!u || lt_d(u + 0x40) != id) return 0;
    uint64_t a = lt_q(u + 0x48);
    return a && lt_b(a + 0x64, 3) != 3 ? a : 0;
}
static uint32_t lt_hid(uint64_t a) {  // the map HashId (linkpos.c actor_hid's path), 0 = spawned / none
    uint64_t mo = lt_q(a + 0x7c8);
    uint32_t h = mo ? lt_d(mo + 0x58) : 0;
    return h == 0xffffffffu ? 0 : h;
}
// every actor the game has, from one actor's proc unit (PR all_actors: the units are one list, +0x10 / +0x18 the previous / next
// one's +0x10, +0x40 the actor's id, +0x48 the actor; the list's head has another vtable)
static int lt_all_actors(uint64_t from, uint64_t *out, int max) {
    uint64_t vt = lt_q(from);
    if (!vt) return 0;
    int n = 0;
    for (int dir = 0; dir < 2; dir++) {
        uint64_t u = from;
        for (int k = 0; k < 16384 && n < max; k++) {
            if (dir == 1 || k > 0) {
                uint64_t a = lt_q(u + 0x48);
                if (a && lt_d(a + 0x60) == lt_d(u + 0x40)) out[n++] = a;
            }
            uint64_t nx = lt_q(u + (dir ? 0x10 : 0x18));
            if (!nx || nx - 0x10 == from) break;
            u = nx - 0x10;
            if (lt_q(u) != vt) break;
        }
    }
    return n;
}
static uint64_t lt_player(void) {
    uint64_t pi = NETL.s_pinfo ? lt_q(NETL.s_pinfo) : 0;
    return pi ? lt_q(pi + 0x60) : 0;  // (sora.c link_actor's chain)
}
static bool lt_in_hand(uint64_t a) {
    for (int k = 0; a && k < 5; k++)
        if (NETL.carried[k] == a || NETL.carried_prev[k] == a) return true;
    return false;
}

// ---- guest calls (only from lt_scan / lt_idle: a driven frame inside the mod frame, api->in_frame, c = NULL) ----
// x0..x5 always loaded (the PR's call() zeroes the ones it does not use: the others would keep the caller's values)
static bool lt_call(uint64_t fn, const SwArgs *a, SwRet *r) {
    SwRet t;
    return fn && NETL.api->call(NULL, fn, a, r ? r : &t);
}
static bool lt_call2(uint64_t fn, uint64_t x0, uint64_t x1, SwRet *r) {
    SwArgs a = {.x = {x0, x1}, .nx = 6};
    return lt_call(fn, &a, r);
}
static void lt_delete(uint64_t a) { lt_call2(NETL.s_del, a, 0, NULL); }  // (PR FN_DELETE(actor, 0))
// an actor deleted on the next look (a receive never calls the game): not if it is in Link's hand then (his now)
static void lt_kill_later(uint64_t actor, uint64_t unit, uint32_t id) {
    if (!actor) return;
    if (NETL.nkillq >= LT_KILLQ) { NETL.st.killq_full++; return; }
    NETL.killq[NETL.nkillq].actor = actor, NETL.killq[NETL.nkillq].unit = unit, NETL.killq[NETL.nkillq].id = id;
    NETL.nkillq++;
}
static void lt_kills_run(void) {
    for (int i = 0; i < NETL.nkillq; i++)
        if (lt_alive_w(NETL.killq[i].actor, &NETL.killq[i].unit, &NETL.killq[i].id) && !lt_in_hand(NETL.killq[i].actor)) lt_delete(NETL.killq[i].actor);
    NETL.nkillq = 0;
}
static int lt_handle_new(void) {
    for (int t = 0; NETL.H && t < LT_HANDLES; t++) {
        int i = (NETL.hnext + t) % LT_HANDLES;
        if (NETL.hbusy[i]) continue;
        NETL.hbusy[i] = true, NETL.hnext = (i + 1) % LT_HANDLES;
        memset(&NETL.H->h[i], 0, sizeof NETL.H->h[i]);
        return i;
    }
    return -1;
}
// the creator + heap the game makes actors with (PR request_p), then the request (its resources load on the game's own threads):
// x0 creator, x1 the name, x2 the heap, x3 the handle, x4 params 0, x5 0, w6 1, s0 0 -> true when asked
static bool lt_request(int hix, const char *name) {
    uint64_t creator = lt_q(lt_q(NETL.s_creator)), owner = lt_q(lt_q(NETL.s_heap)), heap = owner ? lt_q(owner + 0x28) : 0;
    if (!creator || !heap) return false;
    snprintf(NETL.H->name[hix], sizeof NETL.H->name[hix], "%s", name);
    SwArgs a = {.x = {creator, (uint64_t)(uintptr_t)NETL.H->name[hix], heap, (uint64_t)(uintptr_t)&NETL.H->h[hix], 0, 0, 1}, .nx = 7, .nv = 1};
    SwRet r = {0};
    return lt_call(NETL.s_req, &a, &r) && (r.x0 & 1);
}
static int lt_handle_state(int hix) {  // the unit's state: 2 ready, 3 / 5 failed, other = still loading, -1 = no unit yet
    uint64_t u = NETL.H->h[hix].unit;
    if (!u) return -1;
    int32_t s = -1;
    return lt_rd(u, &s, 4) ? s : -1;
}
static uint64_t lt_handle_take(int hix) {  // (PR FN_HANDLE_TAKE: its actor, woken)
    SwRet r = {0};
    bool ok = lt_call2(NETL.s_take, (uint64_t)(uintptr_t)&NETL.H->h[hix], 0, &r);
    NETL.hbusy[hix] = false;
    return ok ? r.x0 : 0;
}
static void lt_handle_release(int hix) {
    lt_call2(NETL.s_rel, (uint64_t)(uintptr_t)&NETL.H->h[hix], 0, NULL);
    NETL.hbusy[hix] = false;
}
static void lt_orphan(int hix) {
    if (hix < 0) return;
    if (NETL.norphan < LT_HANDLES) NETL.orphan[NETL.norphan++] = hix;
}
static void lt_orphans_tick(void) {
    for (int i = 0; i < NETL.norphan; i++) {
        int h = NETL.orphan[i], s = lt_handle_state(h);
        if (s == 2) {
            uint64_t a = lt_handle_take(h);
            if (a) lt_delete(a);
        } else if (s == 3 || s == 5)
            lt_handle_release(h);
        else
            continue;
        NETL.orphan[i--] = NETL.orphan[--NETL.norphan];
    }
}
static void lt_place(uint64_t actor, const NetLootAdd *w) {  // (PR FN_SET_MTX(actor, Matrix34f *, 0))
    float m[12];
    lt_place_mtx(w, m);
    SwArgs a = {.x = {actor, 0, 0}, .nx = 6, .x_scratch = 2, .scratch = 48, .init_len = 48, .init = m};
    lt_call(NETL.s_mtx, &a, NULL);
}
// a map item taken out of the world here, as the game takes out a picked / broken object (PR take_out): woken first when asleep,
// its map object marked gone (x1 16 bytes it fills, x2 the place), then take_out
static bool lt_take_out(uint64_t a, const float at[3]) {
    if (lt_b(a + 0x64, 1) != 1) lt_call2(NETL.s_wake, a, 0, NULL);
    if (NETL.s_gone) {
        uint8_t buf[28] = {0};
        memcpy(buf + 16, at, 12);
        SwArgs g = {.x = {a, 0, 16}, .nx = 6, .x_scratch = 6, .scratch = 32, .init_len = 28, .init = buf};
        lt_call(NETL.s_gone, &g, NULL);
    }
    return lt_call2(NETL.s_out, a, 0, NULL);
}
// its revival flag the game's own way (PR set_bool_flag = EventFlagONAction's path: the flag's handle (the bool buffer, the hash)
// with the manager's +0xc28 in its top byte, then the manager's setBool(mgr, 1, handle); a value written in memory alone is seen by
// nothing). Only a flag the game has (looked up first, read only); one already set is left. -> 1 set, 2 was set, 0 none, -1 refused
static int lt_flag_set(const char *name, uint32_t hid, char *which, size_t cap) {
    uint64_t mgr = NETL.s_gdt ? lt_q(NETL.s_gdt) : 0, pb = mgr ? lt_q(mgr + 0xbd8) : 0, buf = pb ? lt_q(pb) : 0;
    if (which && cap) which[0] = 0;
    if (!lt_is_loot(name)) return -1;  // (s289 review3 #1: a loot item's flag only)
    if (!buf || !name || !name[0]) return 0;
    for (int k = 0; k < 4; k++) {
        char flag[128];
        lt_flag_name(flag, sizeof flag, LT_FLAG_MAPS[k], name, hid);
        uint32_t h = lt_crc32(flag);
        uint64_t v = lt_gdt_find(lt_rd, buf, h);
        if (!v) continue;
        if (which && cap) snprintf(which, cap, "%s", flag);
        if (lt_d(v) & 1) return 2;
        if (!NETL.s_gdt_handle || !NETL.s_gdt_set) return -1;
        SwRet r = {0};
        if (!lt_call2(NETL.s_gdt_handle, buf, h, &r)) return -1;  // (x2 = 0)
        uint32_t handle = (uint32_t)r.x0 | lt_d(mgr + 0xc28) << 24;
        if (handle == 0xffffffffu) return -1;
        SwArgs a2 = {.x = {mgr, 1, handle}, .nx = 6};
        return lt_call(NETL.s_gdt_set, &a2, &r) && (r.x0 & 1) ? 1 : -1;
    }
    return 0;
}

// ---- the wire out (the room's reliable events; Melty only) ----
static bool lt_anyone(void) {
    if (g_net_loop || !NET.on || !NET.melty || !sora_melty_connected()) return false;
    for (int k = 0; k < NET_MAX_PEERS; k++)
        if (NET.slot[k].used && NET.slot[k].p.st == PEER_LIVE) return true;
    return false;
}
static void lt_send(const uint8_t *b, int n) {
    if (n <= 0) return;
    NetAddr d[NET_DESTS_MAX];
    int nd = net_dests(d, false);
    net_send_all(d, nd, b, n);  // (a Melty room takes it once and hands it to the others as a reliable event)
    NETL.st.pkt_tx++;
}
// (NLO_STATE) does slot k's game share loot (it said netloot 1, and it is still the same player in that slot)?
static bool lt_shares(int k) {
    return k >= 0 && k < NET_MAX_PEERS && NET.slot[k].used && NET.slot[k].p.st == PEER_LIVE && NETL.share[k] && NETL.share_j[k] == NET.slot[k].joined_ms + 1;
}
static int lt_slot_of(uint8_t pl) {
    for (int k = 0; k < NET_MAX_PEERS; k++)
        if (NET.slot[k].used && NET.slot[k].p.player == pl) return k;
    return -1;
}
static int lt_sharing(void) {
    int n = 0;
    for (int k = 0; k < NET_MAX_PEERS; k++) n += lt_shares(k);
    return n;
}
static void lt_send_state(bool on, bool ask) {
    uint8_t b[NET_LOOT_HEAD + 1];
    lt_send(b, net_pack_loot_state(NET.me, ++NETL.seq, on, ask, b, (int)sizeof b));
}
static void lt_flush(void) {
    if (!NETL.naddq && !NETL.ntakeq && !NETL.nmapq) return;
    if (!lt_anyone()) {  // (nobody to tell: a joiner gets the list itself)
        NETL.st.unsent += (uint32_t)(NETL.naddq + NETL.ntakeq + NETL.nmapq);
        NETL.naddq = NETL.ntakeq = NETL.nmapq = 0;
        return;
    }
    uint8_t b[NET_PKT_MAX];
    if (NETL.naddq) lt_send(b, net_pack_loot_add(NET.me, ++NETL.seq, NETL.addq, NETL.naddq, b, (int)sizeof b)), NETL.st.add_tx += (uint32_t)NETL.naddq;
    if (NETL.ntakeq) lt_send(b, net_pack_loot_take(NET.me, ++NETL.seq, NETL.takeq, NETL.ntakeq, b, (int)sizeof b)), NETL.st.take_tx += (uint32_t)NETL.ntakeq;
    if (NETL.nmapq) lt_send(b, net_pack_loot_map(NET.me, ++NETL.seq, NETL.mapq, NETL.nmapq, b, (int)sizeof b)), NETL.st.map_tx += (uint32_t)NETL.nmapq;
    NETL.naddq = NETL.ntakeq = NETL.nmapq = 0;
}
static void lt_q_add(const NetLootAdd *w) {
    if (NETL.naddq == NET_LOOT_ADD_MAX) lt_flush();
    NETL.addq[NETL.naddq++] = *w;
}
static void lt_q_take(uint32_t gid) {
    if (NETL.ntakeq == NET_LOOT_TAKE_MAX) lt_flush();
    NETL.takeq[NETL.ntakeq++] = gid;
}
static void lt_q_map(uint32_t hid, const char *name) {
    if (NETL.nmapq == NET_LOOT_MAP_MAX) lt_flush();
    NETL.mapq[NETL.nmapq].hid = hid;
    snprintf(NETL.mapq[NETL.nmapq].name, sizeof NETL.mapq[NETL.nmapq].name, "%s", name && name[0] ? name : "Item_");
    NETL.nmapq++;
}

// an entry that leaves our list: its actor here goes on the next look (not if it is in Link's hand then), a request is orphaned
static void lt_drop_actor(LtEnt *e) {
    if (e->state == LTE_ASKED) lt_orphan(e->hix), e->hix = -1;
    if (e->actor && e->state == LTE_HERE) lt_kill_later(e->actor, e->unit, e->id);
    e->actor = 0;
}
static void lt_forget(const char *why) {
    for (int i = 0; i < NETL.L.n; i++)
        if (NETL.L.e[i].state == LTE_ASKED) lt_orphan(NETL.L.e[i].hix);
    if (NETL.L.n || NETL.M.n) LOG("[loot] list forgotten (%s): %d items, %d map items (none removed: they stay where they lie)\n", why, NETL.L.n, NETL.M.n);
    memset(&NETL.L, 0, sizeof NETL.L), memset(&NETL.M, 0, sizeof NETL.M), memset(&NETL.ring, 0, sizeof NETL.ring);
    memset(NETL.death, 0, sizeof NETL.death), memset(NETL.seen, 0, sizeof NETL.seen), memset(NETL.mapw, 0, sizeof NETL.mapw);
    memset(NETL.hush, 0, sizeof NETL.hush), memset(NETL.peek, 0, sizeof NETL.peek), memset(NETL.own_src, 0, sizeof NETL.own_src);
    memset(NETL.gone, 0, sizeof NETL.gone), memset(NETL.broke, 0, sizeof NETL.broke);
    NETL.nen = NETL.nflagq = NETL.nitem = NETL.nprev = NETL.nkillq = NETL.nouts = NETL.ngone = 0;
    NETL.naddq = NETL.ntakeq = NETL.nmapq = 0;
    NETL.base = NETL.have_link = NETL.en_base = false;
    memset(NETL.carried, 0, sizeof NETL.carried), memset(NETL.carried_prev, 0, sizeof NETL.carried_prev);
}

// sora_net.h, a NET_EDMG packet before net_edmg_recv (reads only): a friend's kills (flag KILL, or life after 0), by HashId
static void net_loot_edmg_peek(const uint8_t *buf, int n, uint64_t now) {
    if (!g_net_loot) return;
    NetEdEnt e[NET_EDMG_MAX];
    uint8_t pl = 0;
    int c = net_unpack_edmg(buf, n, &pl, NULL, e);
    for (int i = 0; i < c; i++) {
        if (!e[i].hid || (!(e[i].flags & NEDF_KILL) && !(e[i].after == 0 && e[i].dealt))) continue;
        int k = 0;
        for (int j = 0; j < LT_PEEK; j++) {
            if (NETL.peek[j].hid == e[i].hid) { k = j; break; }
            if (NETL.peek[j].at < NETL.peek[k].at) k = j;
        }
        NETL.peek[k].hid = e[i].hid, NETL.peek[k].at = now ? now : 1, NETL.peek[k].pl = pl;
    }
}
static uint64_t lt_peek_at(uint32_t hid) {
    for (int j = 0; hid && j < LT_PEEK; j++)
        if (NETL.peek[j].hid == hid) return NETL.peek[j].at;
    return 0;
}
static int lt_peek_slot(uint32_t hid) {  // (the slot of the friend whose NET_EDMG killed it; -1 = none / not in the room)
    for (int j = 0; hid && j < LT_PEEK; j++)
        if (NETL.peek[j].hid == hid) return lt_slot_of(NETL.peek[j].pl);
    return -1;
}
static bool lt_own_src_recent(uint32_t hid, uint64_t now) {
    for (int j = 0; hid && j < LT_OWNSRC; j++)
        if (NETL.own_src[j].hid == hid && now - NETL.own_src[j].at <= LT_TIE_MS) return true;
    return false;
}
static void lt_own_src_note(uint32_t hid, uint64_t now) {
    if (!hid) return;
    int k = 0;
    for (int j = 0; j < LT_OWNSRC; j++) {
        if (NETL.own_src[j].hid == hid) { k = j; break; }
        if (NETL.own_src[j].at < NETL.own_src[k].at) k = j;
    }
    NETL.own_src[k].hid = hid, NETL.own_src[k].at = now;
}
// a friend with a LOWER player id has drops of enemy src too: ours of it are taken back everywhere, its watch removes ours from now
static void lt_retract_src(uint32_t src, uint8_t them, uint64_t now) {
    int n = 0;
    for (int i = 0; i < NETL.L.n; i++) {
        LtEnt *e = &NETL.L.e[i];
        if (lt_gid_origin(e->w.gid) != NET.me || e->w.src != src || e->w.kind != NLK_DROP) continue;
        uint32_t gid = e->w.gid;
        lt_drop_actor(e);
        lt_q_take(gid);
        lt_take(&NETL.L, gid, NULL);
        i--, n++;
    }
    for (int d = 0; d < LT_DEATHS; d++)
        if (NETL.death[d].hid == src && NETL.death[d].until > now) NETL.death[d].kind = LTD_REMOTE;
    for (int j = 0; j < LT_OWNSRC; j++)
        if (NETL.own_src[j].hid == src) NETL.own_src[j].hid = 0;
    NETL.st.ties_lost++;
    LOG("[loot] tie: player %u (lower id) has drops of enemy hid=%08x too: our %d taken back, his are the room's\n", (unsigned)them, src, n);
}

static const char *lt_who(int k) { return k >= 0 && k < NET_MAX_PEERS && NET.slot[k].p.name[0] ? NET.slot[k].p.name : "a friend"; }

// slot k's NET_LOOT (its header checked by net_unpack_ex). Lists only: the game is never called from here. (Melty's events come
// reliable, in order and once: no seq de-dupe.)
static void net_loot_recv(int k, const uint8_t *buf, int n, uint64_t now) {
    if (!g_net_loot || !NET.melty) { NETL.st.rx_off++; return; }  // (key 0: dropped and counted = today; UDP: Melty only)
    static NetLootMsg m;  // (~3 KB: off the stack; the receive is one thread)
    uint8_t pl = 0;
    uint16_t sq = 0;
    if (!net_unpack_loot(buf, n, &pl, &sq, &m)) { NETL.st.rx_bad++; return; }
    const char *who = lt_who(k);
    if (m.op == NLO_STATE) {
        NETL.st.state_rx++;
        bool was = lt_shares(k);
        if (k >= 0 && k < NET_MAX_PEERS) NETL.share[k] = m.on, NETL.share_j[k] = NET.slot[k].joined_ms + 1;
        if (m.on && m.ask) NETL.state_due = true;  // (he asks: ours goes back once, without ask)
        if (m.on != was) LOG("[loot] %s's game %s\n", who, m.on ? "shares loot (netloot 1)" : "does not share loot (netloot 0): his kills leave our drops ours");
        return;
    }
    if (m.op == NLO_ADD) {
        for (int i = 0; i < m.n; i++) {
            const NetLootAdd *a = &m.add[i];
            if (!lt_is_loot(a->name)) {  // (s289 review3 #2: a friend's packet names loot only, never another actor for our game to make)
                if (!NETL.st.rx_bad++ || g_net_loot_log) LOG("[loot] %s's %s is not loot: not made here (counted as bad)\n", who, a->name);
                continue;
            }
            uint8_t org = lt_gid_origin(a->gid);
            uint64_t wait = 0;
            if (org != NET.me && a->src && a->kind == NLK_DROP) {
                uint64_t own_until = lt_death_own_until(NETL.death, LT_DEATHS, a->src, now);
                int t = lt_tie(NET.me, org, lt_own_src_recent(a->src, now) || own_until);
                if (t == LTT_RETRACT_OURS) lt_retract_src(a->src, org, now);
                else if (t == LTT_DELAY_THEIRS) {  // (he takes his back when ours reach him; made here later if he does not)
                    wait = own_until > now + LT_TIE_WAIT_MS ? own_until : now + LT_TIE_WAIT_MS;
                    NETL.st.ties_wait++;
                    if (g_net_loot_log) LOG("[loot] tie: %s's %s (enemy hid=%08x) waits: we have drops of it too and the lower id\n", who, a->name, a->src);
                }
            }
            int at = -1, r = lt_add(&NETL.L, a, now, &at);
            if (r == LTR_NEW) {
                NETL.st.add_rx++;
                NETL.L.e[at].retry_at = wait;  // (state WAIT: made here when near; ours from an earlier session sent back = a copy too)
                if (g_net_loot_log) LOG("[loot] + %s's %s (gid %08x, %s, hid=%08x) at %.1f %.1f %.1f: on the list, made here when near\n", who, a->name, a->gid,
                                        LT_KIND_TXT[a->kind & 3], a->src, a->pos[0], a->pos[1], a->pos[2]);
            } else if (r == LTR_FULL) {
                if (!NETL.st.full++) LOG("[loot] the list is full (%d): %s's %s and later new items are not made here\n", LT_LIST_MAX, who, a->name);
            } else if (r == LTR_TOMB)
                NETL.st.add_tomb++;
            else
                NETL.st.add_dup++;
        }
    } else if (m.op == NLO_TAKE) {
        for (int i = 0; i < m.n; i++) {
            LtEnt e;
            NETL.st.take_rx++;
            if (!lt_take(&NETL.L, m.gid[i], &e)) continue;
            NETL.st.take_hit++;
            bool had = e.actor && e.state == LTE_HERE;
            lt_drop_actor(&e);
            if (g_net_loot_log) LOG("[loot] - %s (gid %08x) picked up by %s: %s\n", e.w.name, e.w.gid, who, had ? "removed here too" : "(none here)");
        }
    } else {
        for (int i = 0; i < m.n; i++) {
            NETL.st.map_rx++;
            if (!lt_is_loot(m.map[i].name)) {  // (s289 review3 #1: only a loot name may set a revival flag in our save)
                if (!NETL.st.rx_bad++ || g_net_loot_log) LOG("[loot] %s's map item %s hid=%08x is not loot: no flag set here (counted as bad)\n", who, m.map[i].name, m.map[i].hid);
                continue;
            }
            if (lt_map_add(&NETL.M, m.map[i].hid, pl, m.map[i].name) != 1) continue;
            if (NETL.nflagq < LT_FLAGQ) {
                NETL.flagq[NETL.nflagq].hid = m.map[i].hid;
                snprintf(NETL.flagq[NETL.nflagq].name, sizeof NETL.flagq[NETL.nflagq].name, "%s", m.map[i].name);
                NETL.nflagq++;
            } else
                NETL.st.flagq_full++;
            if (g_net_loot_log) LOG("[loot] map item %s hid=%08x picked up by %s: taken out here as it is / as it loads\n", m.map[i].name, m.map[i].hid, who);
        }
    }
}

// a new friend (or a slot with someone new in it): our items (+ the orphans, when we lead the room) go out to everyone
static bool lt_player_here(uint8_t o) {
    if (o == NET.me) return true;
    for (int k = 0; k < NET_MAX_PEERS; k++)
        if (NET.slot[k].used && NET.slot[k].p.player == o) return true;
    return false;
}
static void lt_join_send(void) {
    lt_send_state(true, true);  // (ours first, asking theirs back: a kill takes the drops away only where the killer's game shares)
    bool leader = sora_melty_story_leader() == 1;
    int na = 0, nm = 0;
    for (int i = 0; i < NETL.L.n; i++) {
        uint8_t o = lt_gid_origin(NETL.L.e[i].w.gid);
        if (!lt_join_sends(o, NET.me, lt_player_here(o), leader)) continue;
        NetLootAdd w = NETL.L.e[i].w;
        memcpy(w.pos, NETL.L.e[i].here, sizeof w.pos);
        lt_q_add(&w), na++;
    }
    for (int i = 0; i < NETL.M.n; i++) {
        uint8_t o = NETL.M.origin[i];
        if (!lt_join_sends(o, NET.me, lt_player_here(o), leader)) continue;
        lt_q_map(NETL.M.hid[i], NETL.M.name[i]), nm++;
    }
    lt_flush();
    if (g_net_loot_log || na || nm) LOG("[loot] a friend joined: %d items, %d map items sent (%s)\n", na, nm, leader ? "ours + the orphans: we lead the room" : "ours");
}

// ---- one look at the world (lt_scan: driven frames, inside the mod frame) ----
static bool lt_pending(uint64_t a) {
    for (int i = 0; i < NETL.npend; i++)
        if (NETL.pend[i] == a) return true;
    return false;
}
static void lt_read_items(uint64_t player, const float link[3]) {
    NETL.npend = 0;  // (what our requests in flight made already: never a "new" item)
    for (int i = 0; NETL.H && i < LT_HANDLES; i++) {
        uint64_t a = NETL.hbusy[i] && NETL.H->h[i].unit ? lt_q(NETL.H->h[i].unit + 0x48) : 0;
        if (a) NETL.pend[NETL.npend++] = a;
    }
    int na = lt_all_actors(lt_q(player + 0x68), NETL.actors, LT_ACTORS);
    NETL.nitem = 0;
    for (int i = 0; i < na && NETL.nitem < LT_ITEMS; i++) {
        uint64_t a = NETL.actors[i];
        if (lt_b(a + 0x64, 3) == 3) continue;  // (being deleted: PR, it stays in the list a few frames)
        const char *nm = NETL.api->actor_name(a);
        if (!nm || !lt_is_loot(nm)) continue;
        int l = net_loot_namelen(nm);
        if (l > NET_LOOT_NAME_MAX || !net_area_chars(nm, l) || lt_pending(a)) continue;
        float m[12];
        if (!lt_rd(a + 0x398, m, sizeof m) || !isfinite(m[3]) || !isfinite(m[7]) || !isfinite(m[11])) continue;
        float p[3] = {m[3], m[7], m[11]}, d2 = lt_d2(p, link);
        uint32_t h1 = lt_hid(a), h2 = lt_d(a + 0x640), hid = h1 ? h1 : h2;  // (ours; the PR's when ours has none)
        if (hid ? d2 > (LT_MAP_R + 2) * (LT_MAP_R + 2) && lt_map_find(&NETL.M, hid) < 0 : d2 > LT_SEEN_R * LT_SEEN_R) continue;
        LtItem *it = &NETL.item[NETL.nitem++];
        it->actor = a, it->id = lt_d(a + 0x60), it->hid = hid, it->held = lt_d(a + 0x4d4) != 0;
        memcpy(it->pos, p, sizeof p), memcpy(it->m, m, sizeof m);
        snprintf(it->name, sizeof it->name, "%s", nm);
        if (h1 != h2) NETL.st.hid_alt++;
        if (g_net_loot_log && hid && NETL.said_hid < 6) {
            NETL.said_hid++;
            LOG("[loot] HashId check %s: %08x by +0x7c8 -> +0x58 (ours), %08x at +0x640 (the PR's)%s\n", nm, h1, h2, h1 == h2 ? ": the same" : ": DIFFERENT");
        }
    }
}
static bool lt_is_listed(uint64_t a, uint32_t id) {
    for (int i = 0; i < NETL.L.n; i++)
        if (NETL.L.e[i].actor == a && NETL.L.e[i].id == id) return true;
    return false;
}
static bool lt_near_enemy(const float p[3], uint64_t now) {  // (alive or dead in the list, or gone from it lately)
    for (int i = 0; i < NETL.nen; i++)
        if (lt_d2(NETL.en[i].pos, p) < LT_ENEMY_NEAR * LT_ENEMY_NEAR) return true;
    for (int i = 0; i < LT_GONE_EN; i++)
        if (NETL.gone[i].at && now - NETL.gone[i].at < LT_GONE_EN_MS && lt_d2(NETL.gone[i].pos, p) < LT_ENEMY_NEAR * LT_ENEMY_NEAR) return true;
    return false;
}
static bool lt_enemy_holds(uint32_t id) {  // (an enemy's weapon / shield this look: never Link's, however near his head it is)
    for (int i = 0; id && i < NETL.nen; i++)
        if (NETL.en[i].held_id[0] == id || NETL.en[i].held_id[1] == id) return true;
    return false;
}
static bool lt_out_lately(uint32_t id, uint64_t now) {  // (a map item we took out lately: not asked again every look)
    for (int i = 0; i < LT_OUTS; i++)
        if (NETL.outs[i].id == id && now - NETL.outs[i].at < 5000) return true;
    NETL.outs[NETL.nouts % LT_OUTS].id = id, NETL.outs[NETL.nouts % LT_OUTS].at = now;
    NETL.nouts = lt_ring_next(NETL.nouts, LT_OUTS);
    return false;
}
// one of OUR items onto the list and out to everyone
static void lt_mine(const LtItem *it, uint8_t kind, uint32_t src, uint64_t now) {
    NetLootAdd w;
    memset(&w, 0, sizeof w);
    w.gid = lt_gid_next(NET.me, &NETL.ctr), w.src = src, w.kind = kind;
    snprintf(w.name, sizeof w.name, "%s", it->name);
    memcpy(w.pos, it->pos, sizeof w.pos);
    lt_rot_pack(it->m, w.rot);
    int at = -1, r = lt_add(&NETL.L, &w, now, &at);
    if (r != LTR_NEW) {
        if (r == LTR_FULL && !NETL.st.full++) LOG("[loot] the list is full (%d): our %s stays ours alone\n", LT_LIST_MAX, it->name);
        return;
    }
    LtEnt *e = &NETL.L.e[at];
    e->actor = it->actor, e->state = LTE_HERE;
    lt_watch_of(it->actor, &e->unit, &e->id);
    lt_q_add(&w);
    if (kind == NLK_DROP) NETL.st.ours++, lt_own_src_note(src, now);
    else if (kind == NLK_THROWN) NETL.st.thrown++;
    else NETL.st.fell++;
    if (g_net_loot_log) LOG("[loot] ours: %s (gid %08x, %s, hid=%08x) at %.1f %.1f %.1f: on the list, sent\n", w.name, w.gid,
                            kind == NLK_DROP ? "our kill's drop" : kind == NLK_THROWN ? "his weapon, out of his hands" : "fell near Link", src, w.pos[0], w.pos[1], w.pos[2]);
}

// the enemies' deaths since the last look (linkpos_enemies: actor, feet, life as last noted; an entry goes ~1 s after its body)
static void lt_deaths(uint64_t now) {
    int n = NETL.enemies(NETL.ea, NETL.ep, NETL.el, LT_ENEMIES);
    if (n < 0) n = 0;
    if (n > LT_ENEMIES) n = LT_ENEMIES;
    for (int j = 0; j < NETL.nen; j++) {  // (gone from the list since the last look: where it was, for a few seconds)
        bool still = false;
        for (int i = 0; i < n && !still; i++) still = NETL.ea[i] == NETL.en[j].actor;
        if (still) continue;
        memcpy(NETL.gone[NETL.ngone % LT_GONE_EN].pos, NETL.en[j].pos, sizeof NETL.en[j].pos);
        NETL.gone[NETL.ngone % LT_GONE_EN].at = now;
        NETL.ngone = lt_ring_next(NETL.ngone, LT_GONE_EN);
    }
    int nn = 0;
    for (int i = 0; i < n; i++) {
        int f = -1;
        for (int j = 0; j < NETL.nen && f < 0; j++)
            if (NETL.en[j].actor == NETL.ea[i]) f = j;
        bool alive = NETL.el[i] > 0;
        uint32_t hid = f >= 0 ? NETL.en[f].hid : lt_hid(NETL.ea[i]);
        LtEnemy *x = &NETL.en2[nn++];
        x->actor = NETL.ea[i], x->hid = hid, x->alive = alive, x->held_id[0] = x->held_id[1] = 0;
        memcpy(x->pos, NETL.ep[i], sizeof x->pos);
        if (alive) {  // (what it holds now, as the player links his: +0xb98 its weapon, +0xbb0 its shield; they fall as it dies: PR Own.held)
            for (int h = 0; h < 2; h++) {
                uint64_t w = lt_linked(NETL.ea[i] + 0xb98 + 0x18 * (uint64_t)h);
                x->held_id[h] = w ? lt_d(w + 0x60) : 0;
            }
            continue;
        }
        if (f >= 0 && !NETL.en[f].alive) {  // (dead in the last look already: its watch follows the body)
            lt_death_follow(NETL.death, LT_DEATHS, NETL.ea[i], NETL.ep[i], now);
            continue;
        }
        // died since the last look, or first seen dead (loaded and killed again at once: killsync's re-kill on a reload)
        uint64_t peek = lt_peek_at(hid);
        int list = 0;
        bool kt_known = false;
        int64_t kt = 0;
        if (hid) {
            int ki = ks_find(&NETK.L, hid);
            if (ki >= 0) list = NETK.L.src[ki] == KSRC_OWN ? 1 : 2, kt_known = NETK.L.kt[ki] != KS_KT_OLD, kt = NETK.L.kt[ki];
        }
        if (f < 0 && (!NETL.en_base || (!list && !peek))) continue;  // (first seen dead, nobody's kill / our first look: not ours to judge)
        int kind = lt_death_kind(hid, now, peek, list, kt_known, kt);
        if (kind == LTD_REMOTE) {  // (a friend's kill: his game must share its drops, or ours stay ours)
            int ks = peek && now - peek <= LT_REMOTE_KILL_MS ? lt_peek_slot(hid) : -1;
            if (lt_remote_keeps(ks >= 0, lt_shares(ks), lt_sharing() > 0)) {
                NETL.st.kept++;
                if (g_net_loot_log) LOG("[loot] died here: hid=%08x: %s's kill, but his game does not share loot (netloot 0 / an older build): our drops of it stay ours\n",
                                        hid, ks >= 0 ? lt_who(ks) : "a friend");
                continue;
            }
        }
        int d = lt_death_start(NETL.death, LT_DEATHS, NETL.ep[i], NETL.ea[i], hid, kind, now);
        // what it held in the look before (not Link's thrown weapon it picked up): its own; what else lay near it then: not its loot
        for (int h = 0; f >= 0 && h < 2; h++)
            if (NETL.en[f].held_id[h] && lt_ring_find(&NETL.ring, NETL.en[f].held_id[h]) < 0) lt_death_held_add(&NETL.death[d], NETL.en[f].held_id[h]);
        const float *was = f >= 0 ? NETL.en[f].pos : NETL.ep[i];
        for (int k = 0; k < NETL.nprev; k++) {
            const LtItem *p = &NETL.prev[k];
            if (!p->hid && lt_d2(p->pos, was) < LT_DEATH_R * LT_DEATH_R && !lt_death_is_held(&NETL.death[d], p->id)) lt_death_before(&NETL.death[d], p->id);
        }
        NETL.st.deaths[kind]++;
        if (g_net_loot_log)
            LOG("[loot] died here: hid=%08x at %.1f %.1f %.1f: %s (%d items lay near it, %d held by it)\n", hid, NETL.ep[i][0], NETL.ep[i][1], NETL.ep[i][2], LTD_TXT[kind],
                NETL.death[d].nbefore, NETL.death[d].nheld);
    }
    memcpy(NETL.en, NETL.en2, sizeof NETL.en[0] * (size_t)nn), NETL.nen = nn;
    NETL.en_base = true;
}

// a loot item of no HashId this look: an enemy's drop (ours / a duplicate), a friend's break's duplicate, or what fell near Link
static void lt_dynamic(LtItem *it, const float link[3], const float *lm, bool menu, uint64_t now) {
    bool over = false;
    if (lm) {
        float hx = it->pos[0] - lm[3], hz = it->pos[2] - lm[11];
        over = hx * hx + hz * hz < LT_AT_LINK_R * LT_AT_LINK_R && it->pos[1] > lm[7] + 0.5f;  // (PR: what he holds from his pouch)
    }
    bool held = it->held || over;
    int s = lt_seen_find(NETL.seen, LT_SEEN, it->id), r = lt_ring_find(&NETL.ring, it->id);
    if (r < 0 && ((it->held && over && !lt_enemy_holds(it->id)) || (s < 0 && NETL.base && menu && lt_at_link(it->pos, link))))
        r = lt_ring_add(&NETL.ring, it->id, false);  // (his: held over his head, or at his feet right after the menu)
    uint64_t first = s >= 0 ? NETL.seen[s].first : NETL.base ? now : 0;  // (0: there before our first look / a warp)
    if (r < 0 && !held && (s < 0 || NETL.seen[s].st != LTS_JUDGED)) {
        if (s >= 0 && NETL.seen[s].st == LTS_HUSH) {  // (s289 loot + objs fix 2: marked by net_loot_broke_followed)
            lt_delete(it->actor), NETL.st.hushed++, NETL.st.follow_items++;
            NETL.seen[s].st = LTS_JUDGED;
            if (g_net_loot_log) LOG("[loot] %s dropped here by a friend's pushed one that broke here while it followed his: removed (his comes)\n", it->name);
            return;
        }
        int d = lt_death_match(NETL.death, LT_DEATHS, it->pos, it->id, first, now);
        if (d >= 0) {
            LtDeath *dw = &NETL.death[d];
            bool cls = lt_drop_class(it->name, lt_death_is_held(dw, it->id));
            if (cls && dw->kind == LTD_OWN && lt_has_src_below(&NETL.L, dw->hid, NET.me, now)) {  // (a lower id announced this enemy's first)
                dw->kind = LTD_REMOTE, NETL.st.ties_lost++;
                LOG("[loot] tie: a lower id already has drops of enemy hid=%08x: ours of it are removed\n", dw->hid);
            }
            if (!cls) {
                NETL.st.left++;
                if (g_net_loot_log) LOG("[loot] %s near enemy hid=%08x's death: not plainly its drop, left as it is (unshared)\n", it->name, dw->hid);
            } else if (dw->kind == LTD_OWN)
                lt_mine(it, NLK_DROP, dw->hid, now);
            else {
                lt_delete(it->actor), NETL.st.dups++;
                if (g_net_loot_log) LOG("[loot] duplicate %s of enemy hid=%08x (%s) removed here: the killer's game shares its own\n", it->name, dw->hid,
                                        dw->kind == LTD_REMOTE ? "a friend's kill" : "our old kill again");
            }
            if (s < 0) s = lt_seen_add(NETL.seen, LT_SEEN, it->id, it->pos, false, first, now);
            NETL.seen[s].st = LTS_JUDGED;
            return;
        }
        if (s < 0 && NETL.base && lt_hushed(NETL.hush, LT_HUSHES, it->pos, now)) {
            lt_delete(it->actor), NETL.st.hushed++;
            lt_seen_add(NETL.seen, LT_SEEN, it->id, it->pos, false, first, now);
            if (g_net_loot_log) LOG("[loot] %s dropped here by what we broke for a friend: removed (his comes)\n", it->name);
            return;
        }
    }
    if (s < 0) {
        bool close = r >= 0 || lt_d2(it->pos, link) < LT_NEAR_DROP * LT_NEAR_DROP;  // (his: wherever it landed)
        bool brk = lt_hushed(NETL.broke, LT_HUSHES, it->pos, now);  // (s289 loot + objs: near our own break told to the room: its drop)
        bool watch = NETL.base && close && (brk || lt_fell_ok(it->name, r >= 0, r < 0 && lt_near_enemy(it->pos, now)));
        if (watch && brk) NETL.st.broke_items++;
        s = lt_seen_add(NETL.seen, LT_SEEN, it->id, it->pos, watch, first, now);
    }
    // (s289 shots merge: one netshots still draws in flight for the friends (an arrow / his thrown weapon, < its 500 ms still) is not
    // settled: it becomes a shared drop once landed, when shots hides it there, so it never shows twice)
    bool flying = LT_SHOT_FLYING(it->actor) != 0;
    if (flying) NETL.st.flying++;
    if (lt_seen_step(&NETL.seen[s], it->pos, held || flying, now)) lt_mine(it, r >= 0 && NETL.ring.thrown[r] ? NLK_THROWN : NLK_FELL, 0, now);
}

static void lt_scan(const float link[3], uint64_t now, bool share) {
    uint64_t player = lt_player();
    if (!player) { NETL.st.no_player++; return; }
    if (!NETL.said_player) NETL.said_player = true, LOG("[loot] the player actor: %s\n", NETL.api->actor_name(player));
    NETL.st.scans++;
    float jx = link[0] - NETL.link_was[0], jz = link[2] - NETL.link_was[2];
    bool jumped = !NETL.have_link || jx * jx + jz * jz > LT_JUMP_R * LT_JUMP_R;
    memcpy(NETL.link_was, link, sizeof NETL.link_was), NETL.have_link = true;
    if (jumped) NETL.base = false, memset(NETL.seen, 0, sizeof NETL.seen), memset(NETL.mapw, 0, sizeof NETL.mapw), NETL.nprev = 0;
    if (!share) NETL.base = NETL.en_base = false, NETL.nen = 0;  // (alone: the first look with a friend again is a baseline)
    // Link's hands (PR link_carried): weapon / shield / bow, the weapon's sheath and the bow's quiver
    memcpy(NETL.carried_prev, NETL.carried, sizeof NETL.carried);
    for (int k = 0; k < 3; k++) NETL.carried[k] = lt_linked(player + 0xb98 + 0x18 * (uint64_t)k);
    NETL.carried[3] = NETL.carried[0] ? lt_linked(NETL.carried[0] + 0x958) : 0;
    NETL.carried[4] = NETL.carried[2] ? lt_linked(NETL.carried[2] + 0x958) : 0;
    lt_kills_run();
    lt_orphans_tick();
    lt_read_items(player, link);
    for (int k = 0; k < 3; k++) {  // (one that left his hands and still lies in the world: his, thrown / knocked out; watched anew)
        uint64_t a = NETL.carried_prev[k];
        if (!a || a == NETL.carried[0] || a == NETL.carried[1] || a == NETL.carried[2]) continue;
        for (int i = 0; i < NETL.nitem; i++) {
            if (NETL.item[i].actor != a) continue;
            lt_ring_add(&NETL.ring, NETL.item[i].id, true);
            int s = lt_seen_find(NETL.seen, LT_SEEN, NETL.item[i].id);
            if (s >= 0) NETL.seen[s].st = LTS_FREE;
        }
    }
    memset(NETL.carried_prev, 0, sizeof NETL.carried_prev);  // (from here: "in his hand" = this look's)
    if (share) lt_deaths(now);
    float lm[12];
    bool have_lm = lt_rd(player + 0x398, lm, sizeof lm) && isfinite(lm[3]) && isfinite(lm[7]) && isfinite(lm[11]);
    bool menu = NETL.menu_at && now - NETL.menu_at < LT_MENU_MS;
    for (int i = 0; i < NETL.nitem; i++) {
        LtItem *it = &NETL.item[i];
        if (lt_in_hand(it->actor) || lt_is_listed(it->actor, it->id)) continue;
        if (!it->hid) {
            if (share) lt_dynamic(it, link, have_lm ? lm : NULL, menu, now);
            continue;
        }
        int mi = lt_map_find(&NETL.M, it->hid);
        if (mi >= 0) {
            if (NETL.M.origin[mi] == NET.me) continue;  // (our own pickup: the game's own rules bring it back, or not)
            if (lt_out_lately(it->id, now)) continue;
            bool ok = lt_take_out(it->actor, it->pos);
            NETL.st.map_out += ok;
            if (g_net_loot_log) LOG("[loot] map item %s hid=%08x here: a friend took it, %s\n", it->name, it->hid, ok ? "taken out" : "take_out refused");
            continue;
        }
        if (!share || lt_d2(it->pos, link) > LT_MAP_R * LT_MAP_R) continue;
        int f = -1, fr = -1;
        for (int w = 0; w < LT_MAPW && f < 0; w++) {
            if (NETL.mapw[w].actor == it->actor && NETL.mapw[w].id == it->id) f = w;
            else if (fr < 0 && !NETL.mapw[w].actor) fr = w;
        }
        if (f < 0 && fr >= 0) {
            f = fr;
            NETL.mapw[f].actor = it->actor, NETL.mapw[f].hid = it->hid;
            lt_watch_of(it->actor, &NETL.mapw[f].unit, &NETL.mapw[f].id);
            snprintf(NETL.mapw[f].name, sizeof NETL.mapw[f].name, "%s", it->name);
        }
        if (f >= 0) memcpy(NETL.mapw[f].pos, it->pos, sizeof it->pos);
    }
    NETL.base = share;
    // the map items watched near Link: picked up here -> MAPTAKE (its revival flag is the game's own here: it set it)
    for (int w = 0; w < LT_MAPW; w++) {
        if (!NETL.mapw[w].actor) continue;
        bool alive = lt_alive_w(NETL.mapw[w].actor, &NETL.mapw[w].unit, &NETL.mapw[w].id), hand = alive && lt_in_hand(NETL.mapw[w].actor);
        float d2 = lt_d2(NETL.mapw[w].pos, link);
        if (alive && !hand && d2 < (LT_MAP_R + 2) * (LT_MAP_R + 2)) continue;
        if (lt_map_picked(alive, hand, d2, jumped)) {
            if (lt_map_add(&NETL.M, NETL.mapw[w].hid, NET.me, NETL.mapw[w].name) == 1) lt_q_map(NETL.mapw[w].hid, NETL.mapw[w].name);
            if (g_net_loot_log) LOG("[loot] map item %s hid=%08x picked up here: taken out in the others' worlds too\n", NETL.mapw[w].name, NETL.mapw[w].hid);
        }
        memset(&NETL.mapw[w], 0, sizeof NETL.mapw[w]);
    }
    // the list's actors here: ours and the copies we made; the ones to make
    int asked = 0;
    for (int i = 0; i < NETL.L.n; i++) {
        LtEnt *e = &NETL.L.e[i];
        bool mine = lt_gid_origin(e->w.gid) == NET.me;
        float dl = lt_d2(e->here, link);
        if (e->state == LTE_GONE) {
            if (dl > LT_REMAKE_R * LT_REMAKE_R) e->state = LTE_WAIT, e->tries = 0, e->retry_at = 0, e->made_at = 0;  // (made again when he comes back)
            continue;
        }
        if (e->state == LTE_WAIT) {
            if (now < e->retry_at || dl > LT_MAKE_R * LT_MAKE_R || asked >= LT_ASK_PER_LOOK) continue;
            int h = lt_handle_new();
            if (h < 0) continue;
            asked++;
            if (lt_request(h, e->w.name)) e->state = LTE_ASKED, e->hix = h;
            else {
                NETL.hbusy[h] = false;
                e->retry_at = now + 1000;
                if (++e->tries >= LT_MAKE_TRIES) e->state = LTE_GONE, NETL.st.make_fail++, LOG("[loot] %s (gid %08x) could not be asked for: not made here\n", e->w.name, e->w.gid);
            }
            continue;
        }
        if (e->state == LTE_ASKED) {
            int s = lt_handle_state(e->hix);
            if (s == 2) {
                uint64_t a = lt_handle_take(e->hix);
                e->hix = -1;
                if (a) {
                    lt_place(a, &e->w);
                    e->actor = a, e->state = LTE_HERE, e->made_at = now;
                    lt_watch_of(a, &e->unit, &e->id);
                    memcpy(e->here, e->w.pos, sizeof e->here);
                    NETL.st.made++;
                    if (g_net_loot_log) LOG("[loot] made here: %s (gid %08x) at %.1f %.1f %.1f\n", e->w.name, e->w.gid, e->w.pos[0], e->w.pos[1], e->w.pos[2]);
                } else
                    e->state = ++e->tries >= LT_MAKE_TRIES ? LTE_GONE : LTE_WAIT, e->retry_at = now + 1000;
            } else if (s == 3 || s == 5) {
                lt_handle_release(e->hix);
                e->hix = -1;
                e->state = ++e->tries >= LT_MAKE_TRIES ? LTE_GONE : LTE_WAIT, e->retry_at = now + 1000;
                if (e->state == LTE_GONE) NETL.st.make_fail++, LOG("[loot] %s (gid %08x): the game could not make it here (state %d)\n", e->w.name, e->w.gid, s);
            }
            continue;
        }
        bool alive = lt_alive_w(e->actor, &e->unit, &e->id), hand = alive && lt_in_hand(e->actor);
        if (!alive && e->made_at && now - e->made_at < 1500 && e->tries < LT_MAKE_TRIES) {  // (PR: lost as it was made: made again)
            e->tries++, e->state = LTE_WAIT, e->actor = 0, e->retry_at = now + 300, NETL.st.lost_made++;
            continue;
        }
        if (alive && !hand) {
            float m[12];
            if (lt_rd(e->actor + 0x398, m, sizeof m) && isfinite(m[3]) && isfinite(m[7]) && isfinite(m[11])) e->here[0] = m[3], e->here[1] = m[7], e->here[2] = m[11];
            continue;
        }
        int pk = lt_pick(alive, hand, lt_d2(e->here, link));
        if (pk == LTP_PICKED) {
            uint32_t gid = e->w.gid;
            if (g_net_loot_log) LOG("[loot] picked up here: %s (gid %08x, %s)%s: gone for everyone\n", e->w.name, gid, mine ? "ours" : "a friend's", hand ? ", into his hand" : "");
            NETL.st.picked++;
            lt_q_take(gid);
            lt_take(&NETL.L, gid, NULL);
            i--;
        } else if (pk == LTP_GONE) {
            e->state = LTE_GONE, e->actor = 0, NETL.st.gone_here++;
            if (g_net_loot_log) LOG("[loot] gone here (%.0f m from Link: unloaded): %s (gid %08x) stays on the list for the others\n", sqrtf(lt_d2(e->here, link)), e->w.name, e->w.gid);
        }
    }
    // the friends' map items waiting for their flag
    for (int q = 0; q < NETL.nflagq; q++) {
        char which[128];
        int r = lt_flag_set(NETL.flagq[q].name, NETL.flagq[q].hid, which, sizeof which);
        if (r == 1) NETL.st.flags_ok++;
        else if (r == 2) NETL.st.flags_had++;
        else if (r == 0) NETL.st.flags_none++;
        else NETL.st.flags_fail++;
        if (g_net_loot_log) LOG("[loot] revival flag of %s hid=%u: %s %s\n", NETL.flagq[q].name, NETL.flagq[q].hid, which[0] ? which : "(none of the 4 maps' names)",
                                r == 1 ? "set" : r == 2 ? "was set already" : r == 0 ? "is not a flag of this game: only taken out" : "NOT set (the setter refused)");
    }
    NETL.nflagq = 0;
    memcpy(NETL.prev, NETL.item, sizeof NETL.item[0] * (size_t)NETL.nitem), NETL.nprev = NETL.nitem;
}

// netloot 0 with requests still in flight (turned off while a copy was loading): they are still taken and deleted (nothing else)
static void lt_idle(const float *link_pos) {
    if (!NETL.norphan || !NETL.sites || !link_pos || NET.loading || !NETL.api->in_frame()) return;
    uint32_t ps = 0;
    if (NETL.s_pause && lt_rd(NETL.s_pause, &ps, 4) && ps != 0) return;
    lt_orphans_tick();
}

// every net_tick (after the receive and net_kills_tick): link_pos NULL = not driven (nothing looked at, nothing called)
static void net_loot_tick(const float *link_pos, uint64_t now) {
    if (!g_net_loot) { lt_idle(link_pos); return; }
    lt_bind();
    if (!NETL.said) NETL.said = true, LOG("[loot] netloot 1: %s\n", lt_sites_txt());
    if (!NETL.sites) return;
    if (!NET.melty) {
        if (!NETL.said_udp) NETL.said_udp = true, LOG("[loot] netloot: Melty rooms only (this net session is not one): nothing shared\n");
        return;
    }
    for (int k = 0; k < NET_MAX_PEERS; k++) {  // (someone new in a slot: our list goes out to him)
        uint64_t j = NET.slot[k].used ? NET.slot[k].joined_ms + 1 : 0;
        if (j != NETL.joined[k]) NETL.join_due = NETL.join_due || j, NETL.joined[k] = j;
    }
    if (NETL.moons_fn && now >= NETL.next_moon) {  // (a Blood Moon here: the session's taken map items come back by the game's own rules)
        NETL.next_moon = now + 2000;
        int m = NETL.moons_fn();
        if (m >= 0) {
            if (NETL.moons_known && m > NETL.moons && NETL.M.n) LOG("[loot] Blood Moon here: %d taken map items forgotten\n", NETL.M.n), NETL.st.moon_clr++, memset(&NETL.M, 0, sizeof NETL.M);
            NETL.moons = m, NETL.moons_known = true;
        }
    }
    bool share = lt_anyone();
    if (share && NETL.join_due) NETL.join_due = false, lt_join_send();
    if (share && NETL.state_due) NETL.state_due = false, lt_send_state(true, false);  // (an answer: no ask, so no ping-pong)
    {
        LtEnt old[8];
        int x = lt_expire(&NETL.L, now, old, 8);
        for (int i = 0; i < x; i++) {
            if (old[i].state == LTE_ASKED) lt_orphan(old[i].hix);
            NETL.st.expired++;
        }
    }
    uint32_t ps = 0;
    if (NETL.s_pause && lt_rd(NETL.s_pause, &ps, 4) && ps != 0) NETL.menu_at = now;  // (0 playing, 2 the pause menu / map)
    bool work = share || NETL.L.n || NETL.M.n || NETL.nflagq || NETL.norphan || NETL.nkillq;  // (alone: our pickups of listed items still count)
    if (!work || !link_pos || NET.loading || now < NETL.next_scan) { lt_flush(); return; }
    if (ps != 0) { NETL.st.paused++; lt_flush(); return; }
    if (!NETL.api->in_frame()) { NETL.st.not_frame++; lt_flush(); return; }
    NETL.next_scan = now + LT_SCAN_MS;
    LARGE_INTEGER t0, t1, fq;
    QueryPerformanceCounter(&t0);
    lt_scan(link_pos, now, share);
    QueryPerformanceCounter(&t1);
    QueryPerformanceFrequency(&fq);
    uint32_t us = fq.QuadPart > 0 ? (uint32_t)((t1.QuadPart - t0.QuadPart) * 1000000 / fq.QuadPart) : 0;
    NETL.cost_last = us, NETL.cost_max = us > NETL.cost_max ? us : NETL.cost_max, NETL.cost_sum += us, NETL.cost_n++;
    lt_flush();
}

// (s289 loot) for objs' break_here: where OUR game breaks something for a friend (his ore, his crate), what it drops here is a
// duplicate (his own comes on the list from his game): new loot first seen within LT_HUSH_R for LT_HUSH_MS is removed (PR drop_hush)
static void net_loot_hush(const float pos[3], uint64_t now) {
    if (g_net_loot && pos) lt_hush_add(NETL.hush, LT_HUSHES, pos, now);
}
// (s289 loot) for objs' break detection: OUR Link broke this one and objs told the room (the friends break their copies and hush
// what those drop), so its drops here are ours to share, rupees too: the ones seen already (lt_seen_broke) and the next LT_HUSH_MS
static void net_loot_broke_here(const float pos[3], uint64_t now) {
    if (!g_net_loot || !pos) return;
    lt_hush_add(NETL.broke, LT_HUSHES, pos, now);
    int n = NETL.base ? lt_seen_broke(NETL.seen, LT_SEEN, pos, now) : 0;
    NETL.st.broke++, NETL.st.broke_items += (uint32_t)n;
    if (g_net_loot_log) LOG("[loot] our break at %.1f %.1f %.1f (told the room by netobjs): what it drops here is ours, shared (%d seen already)\n", pos[0], pos[1], pos[2], n);
}
// (s289 loot) for objs fix 2's follow_broke: a friend's pushed one broke HERE while it followed his (objs: not our break, the room is
// not told). Its drops here duplicate his (his break shares them) or, when only ours broke, his to come: removed, the ones seen
// already (lt_seen_hush_back: at the next look) and what comes there the next LT_HUSH_MS (the hush, as for no_apply)
static void net_loot_broke_followed(const float pos[3], uint64_t now) {
    if (!g_net_loot || !pos) return;
    lt_hush_add(NETL.hush, LT_HUSHES, pos, now);
    int n = NETL.base ? lt_seen_hush_back(NETL.seen, LT_SEEN, pos, now) : 0;
    NETL.st.follow++;
    if (g_net_loot_log)
        LOG("[loot] a friend's pushed one broke here while it followed his (netobjs: not our break) at %.1f %.1f %.1f: what it drops here is "
            "removed, his comes (%d seen already)\n", pos[0], pos[1], pos[2], n);
}

// net on: a fresh seq + gid counter (a restarted game never reuses an old session's gids), no list
static void net_loot_reset(uint64_t now) {
    if (NETL.L.n || NETL.M.n) lt_forget("net on");
    NETL.seq = (uint16_t)(now * 2654435761u >> 9);
    NETL.ctr = (uint32_t)(now * 2246822519u >> 7) & 0xffffffu;
    memset(NETL.joined, 0, sizeof NETL.joined), memset(NETL.share, 0, sizeof NETL.share), memset(NETL.share_j, 0, sizeof NETL.share_j);
    NETL.said_udp = false, NETL.state_due = false;
}

static bool net_loot_live_set(const char *key, int v) {
    if (!strcmp(key, "netloot")) {
        bool on = v != 0;
        if (on == g_net_loot) return true;  // (a live rewrite re-applies every line: only a change counts)
        g_net_loot = on;
        if (!on && lt_anyone()) lt_send_state(false, false);  // (the friends keep their drops of our kills from now)
        if (!on) lt_forget("set netloot 0"), memset(NETL.share, 0, sizeof NETL.share), NETL.state_due = false;  // (off: no states kept;
                                                                                                                 //  on again asks anew)
        else {
            NETL.said = false, NETL.join_due = true;
            if (!NETL.ctr) NETL.ctr = (uint32_t)(GetTickCount64() * 2246822519u >> 7) & 0xffffffu;
        }
        lt_bind();
        LOG("[loot] netloot %d: %s (%s)\n", on, on ? "the room shares enemy drops, dropped items and map item pickups (Melty rooms)" : "off: every game keeps its own loot (today)",
            lt_sites_txt());
        return true;
    }
    if (!strcmp(key, "netlootlog")) { g_net_loot_log = v != 0; return true; }
    return false;
}

static void net_loot_diag(void) {
    int mine = 0, made = 0, wait = 0;
    for (int i = 0; i < NETL.L.n; i++) {
        if (NETL.L.e[i].state == LTE_HERE && lt_gid_origin(NETL.L.e[i].w.gid) == NET.me && !NETL.L.e[i].made_at) mine++;
        else if (NETL.L.e[i].state == LTE_HERE) made++;
        else if (NETL.L.e[i].state != LTE_GONE) wait++;
    }
    LOG("[loot] %s, %s: list %d (ours here %d, copies here %d, to make %d), map items taken %d; friends sharing %d (states in %u); "
        "deaths own %u friend %u old %u, friends' kills kept (his game does not share) %u; ours %u, "
        "duplicates removed %u, left %u, hushed %u, fell %u, thrown %u (looks still in flight for netshots %u), our breaks %u (their drops taken as ours %u), "
        "friends' pushed ones that broke here %u (their drops removed %u); adds out %u in %u (dup %u, taken %u), takes out %u in %u (hit %u), "
        "map out %u in %u taken out %u, flags set %u had %u none %u refused %u (queue full %u); made %u, make fails %u, lost %u, picked %u, "
        "gone here %u; ties lost %u waited %u; bad %u, off %u, pkts %u, unsent %u, expired %u, moon %u, paused %u, not in frame %u, "
        "kill queue full %u, no player %u, HashIds that differ %u; scans %u, a look %u us (max %u, avg %u)\n",
        g_net_loot ? "ON" : "off (set netloot 1)", NETL.tried ? lt_sites_txt() : "not bound yet", NETL.L.n, mine, made, wait, NETL.M.n, lt_sharing(),
        NETL.st.state_rx, NETL.st.deaths[0], NETL.st.deaths[1], NETL.st.deaths[2], NETL.st.kept, NETL.st.ours, NETL.st.dups, NETL.st.left, NETL.st.hushed, NETL.st.fell, NETL.st.thrown, NETL.st.flying, NETL.st.broke, NETL.st.broke_items,
        NETL.st.follow, NETL.st.follow_items, NETL.st.add_tx,
        NETL.st.add_rx, NETL.st.add_dup, NETL.st.add_tomb, NETL.st.take_tx, NETL.st.take_rx, NETL.st.take_hit, NETL.st.map_tx, NETL.st.map_rx,
        NETL.st.map_out, NETL.st.flags_ok, NETL.st.flags_had, NETL.st.flags_none, NETL.st.flags_fail, NETL.st.flagq_full, NETL.st.made,
        NETL.st.make_fail, NETL.st.lost_made, NETL.st.picked, NETL.st.gone_here, NETL.st.ties_lost, NETL.st.ties_wait, NETL.st.rx_bad, NETL.st.rx_off,
        NETL.st.pkt_tx, NETL.st.unsent, NETL.st.expired, NETL.st.moon_clr, NETL.st.paused, NETL.st.not_frame, NETL.st.killq_full, NETL.st.no_player,
        NETL.st.hid_alt, NETL.st.scans, NETL.cost_last, NETL.cost_max, NETL.cost_n ? (uint32_t)(NETL.cost_sum / NETL.cost_n) : 0);
}
#endif  // NET_LOOT_GLUE

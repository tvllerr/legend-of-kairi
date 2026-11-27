// s289 noauto (Option A of recon/s289/S289_HELPER_SPAWNED.md): in a net session with a friend, BotW's own ENEMY
// auto-placement is held off, so every copy shows only the placed (HashId) enemies ESYNC already shares. The user's two-copy
// Melty room test: "enemy is only showing on the game on the right but not on the game on the left" = the auto-placed
// enemies (made with map object 0 = hid 0: Chuchu, Keese, night Stal, ...) are made by each copy around its own Link.
//
// Live keys (DLL side, sora_net.h net_live_set):
//   set roomnoauto 0|1  compiled default 1 since s290 (the user: "Flip the passes ON"). 1 = while the net session is on
//                       with at least one live peer (the rule of party_mp_live, sora_party.h: NET.on + a used slot whose peer is
//                       PEER_LIVE; netloop's own slot 0 counts too), every DRIVEN frame (Link's position read this frame, no
//                       load) stores 5 into BotW's type-0 (Enemy) hold byte of the AutoPlacementMgr. 0, net off or no peer:
//                       nothing is read or written (the old frames exactly). One log line when the key changes.
//   set noautolog 0|1   the per-event lines: hold started / stopped (with the reason), the mgr pointer found / lost / not
//                       writable, and every 10 s (driven frames; also with roomnoauto 0, for the "before" numbers) the count
//                       of auto-placed (hid 0) enemies loaded near Link.
//   net diag            one [noauto] line: the state and why, frame counts, the mgr, its type-0 hold byte, BotW's own counter.
//
// BotW (S289_HELPER_SPAWNED.md Q2 + this helper's static re-check; 1.9.0 and 1.8.2 VERIFIED static, the same offsets):
//   AutoPlacementMgr = *[sites row autoplace_mgr_ptr] (a sead singleton of 0x189e38 bytes; its disposer clears the pointer),
//   its vtable at +0 = sites row autoplace_mgr_vt (the ctor's store). The setter (mgr, type, on) (row autoplace_forbid, NOT
//   called here): on = an atomic +1 of the u32 counter at +0x171e4c + 4 * type and the s8 hold byte at +0x171e68 + type = 5;
//   off = the counter -1 only. The calc lowers each hold byte by 1 per run while that type's counter is 0 (and a game-state
//   byte is not 5); the spawn checks (0x7100b18a4c) say "forbidden" while the byte is above 0. Type 0 = Enemy (every flow
//   name that is not Animal / Insect / Fish / Bird / Material). Names with "Enemy_Dragon" are exempt (dragons keep coming).
//   So our store keeps the byte at 4..5 (GUESS: the calc runs about once a frame); when we stop, the game's own countdown
//   clears it about 5 calc runs later: nothing can leak (we keep no counter). BotW's own holds (NoAutoPlacementEnemyDeadlyQuest,
//   the area tags, the DLC trial) work as before: its counter is never touched and a byte already at 5 or more is left alone.
//   Stops: Chuchu, Keese, night Stal, auto Octoroks / Lizalfos / Pebblits / Wizzrobes, disguised Yiga. Stays: animals,
//   insects, fish, birds, materials, every placed enemy (HashId rows, ESYNC), the dragons; enemies already spawned stay.
// Before every store the pointer is re-read and checked (in guest memory, readable through the exe's sw_api, its vtable);
// the first store at a new mgr address goes through api->write (committed READWRITE pages only), later ones are plain byte
// stores (api->write's VirtualQuery costs ms). An exe without sw_api / without the s289 rows (NEEDS EXE RELINK) or another
// game version: nothing is written, said once in the key's line.
// GUESS: frames that are not driven (menus, loading, Sora's body skipped) write nothing; BotW's calc pauses in menus and
// loads (the pause counter at +0x171e48), but a long stretch where the game runs and the mod frame is not driven could let
// the byte run out and one spawn through. "net diag" counts those frames ("wanted but not in the world").
//
// Pure core first (host test runtime/test/noauto_test.c); the glue (NET, LOG, sw_api) under NET_NOAUTO_GLUE, which sora_net.h
// defines before the include (as sora_net_kills.h).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define NA_TYPE_ENEMY 0
#define NA_HOLD_OFF 0x171e68u   // s8 hold byte per type (+ type)
#define NA_CNT_OFF 0x171e4cu    // u32 forbid counter per type (+ 4 * type)
#define NA_PAUSE_OFF 0x171e48u  // u32: the calc returns early while it is not 0 (GUESS: the meaning "pause"; S289 report Q2)
#define NA_SPAN 0x28u           // NA_PAUSE_OFF .. past the 7 hold bytes (0x171e70): checked readable before a read / store
#define NA_MGR_SIZE 0x189e38u   // sizeof(AutoPlacementMgr) (its createInstance's alloc)
#define NA_HOLD_VAL 5           // what BotW's own setter stores
#define NA_NEAR_M 60.0f         // the count's "near Link", horizontal metres (GUESS: about the near spawn ring)
#define NA_COUNT_MS 10000u      // the noautolog count line
#define NA_RETRY_MS 5000u       // a hold byte api->write refused: tried again after this

// why the hold is not written this frame, in check order; NA_HOLD = write it. (zero = key off = the compiled default)
typedef enum { NA_KEY, NA_NET, NA_PEER, NA_MGR, NA_HOLD, NA_N } NaWhy;
static const char *const NA_STATE_TXT[NA_N] = {"off (set roomnoauto 1)", "waiting: net off", "waiting: no friend connected",
                                               "waiting: no AutoPlacementMgr (title / loading / not writable / old exe)",
                                               "HOLDING: BotW's enemy auto-placement is held off"};
static const char *const NA_START_TXT[NA_N] = {"key on", "net on", "peer joined", "AutoPlacementMgr found again", "?"};
static const char *const NA_STOP_TXT[NA_N] = {"key off", "net off", "peer left", "AutoPlacementMgr pointer missing", "?"};

// the frame's answer: key + session + mgr (mgr_ok = the pointer passed every check this frame)
static inline NaWhy na_block(bool key, bool net_on, int peers, bool mgr_ok) {
    if (!key) return NA_KEY;
    if (!net_on) return NA_NET;
    if (peers <= 0) return NA_PEER;
    if (!mgr_ok) return NA_MGR;
    return NA_HOLD;
}
// a store this frame: holding AND driven (a frame that is not driven writes nothing: no checks there)
static inline bool na_write_now(NaWhy w, bool driven) { return w == NA_HOLD && driven; }
// the value to store over cur: 5 below 5, else -1 = leave it (BotW's own hold, or a value we never lower)
static inline int na_store_val(int cur) { return cur < NA_HOLD_VAL ? NA_HOLD_VAL : -1; }
// the pointer checks' verdict: non-zero, in guest memory, readable (the vtable + NA_SPAN), the vtable = the row's (non-zero)
static inline bool na_mgr_ok(uint64_t mgr, bool in_guest, bool readable, uint64_t vt, uint64_t vt_want) {
    return mgr && in_guest && readable && vt_want && vt == vt_want;
}

// the logged state. from = what a wait for the mgr came after (a start from NA_MGR names that: "peer joined" when the mgr
// was only checked at the next driven frame; NA_HOLD = the pointer came back with the session unchanged)
typedef struct { NaWhy last, from; } NaState;
// +1 = the hold started (*why = what blocked it before: NA_START_TXT), -1 = stopped (*why = what blocks it now: NA_STOP_TXT)
static inline int na_step(NaState *s, NaWhy now, NaWhy *why) {
    NaWhy was = s->last;
    if (now == NA_MGR && was != NA_MGR) s->from = was;
    s->last = now;
    if (was != NA_HOLD && now == NA_HOLD) {
        *why = was != NA_MGR ? was : s->from != NA_HOLD ? s->from : NA_MGR;
        return 1;
    }
    if (was == NA_HOLD && now != NA_HOLD) {
        *why = now;
        return -1;
    }
    return 0;
}

// the count line: the exe's enemy list (Enemy_* and non-insect Animal_*), each with its map HashId (0 = no map object =
// auto-placed, as ah_actor_hid), feet position and life
typedef struct { const char *name; uint32_t hid; float pos[3]; int32_t life; } NaEnemy;
typedef struct { int near0, all0, placed, dragons; } NaCount;
static inline bool na_is_dragon(const char *n) { return n && strstr(n, "Enemy_Dragon"); }
// living Enemy_* only (the list also has animals; a dead one lingers); dragons on their own (auto-placed but exempt); near =
// within near_m of link on the ground plane (x, z)
static inline void na_count(const NaEnemy *e, int n, const float link[3], float near_m, NaCount *c) {
    memset(c, 0, sizeof *c);
    for (int i = 0; e && i < n; i++) {
        const char *nm = e[i].name;
        if (!nm || strncmp(nm, "Enemy_", 6) || e[i].life <= 0) continue;
        if (na_is_dragon(nm)) {
            c->dragons++;
            continue;
        }
        if (e[i].hid) {
            c->placed++;
            continue;
        }
        c->all0++;
        if (link) {
            float dx = e[i].pos[0] - link[0], dz = e[i].pos[2] - link[2];
            if (dx * dx + dz * dz <= near_m * near_m) c->near0++;
        }
    }
}

#ifdef NET_NOAUTO_GLUE
// ---- glue (sora_net.h: NET, PEER_LIVE, LOG, GetProcAddress) ----
#include "sw_api.h"

static bool g_net_noauto = true;  // live "set roomnoauto 0|1". (s290: passed the two-copy lab, sb/s290/LAB_RESULTS.md; the user: "Flip the passes ON" -> default ON). Its sites line in a room: sora_net_flip.h
static bool g_net_noauto_log;  // live "set noautolog"
static struct {
    bool tried, sites;
    const SwApi *api;
    uint64_t ptr_site, vt_site;                             // sites rows autoplace_mgr_ptr / autoplace_mgr_vt (0 = none)
    int (*enemies)(uint64_t *, float (*)[3], int32_t *, int);  // the exe's linkpos_enemies (the count only)
    NaState s;
    bool mgr_ok;                                            // the last driven frame's checks (not driven: kept, no reads)
    bool said;                                              // this want's first check logged the mgr (found or missing)
    uint64_t mgr;                                           // that frame's checked mgr (0 = none)
    uint64_t mgr_rw, mgr_bad, bad_retry;                    // the mgr whose first store api->write took / refused
    uint64_t next_count;
    float link[3];
    bool have_link;
    struct { uint32_t held, stores, idle, miss, refused, starts, stops; } st;
} NA;

// once, on the first use (key 0 and noautolog 0 never get here)
static void na_bind(void) {
    if (NA.tried) return;
    NA.tried = true;
    HMODULE exe = GetModuleHandleA(NULL);
    const SwApi *(*get)(uint32_t) = (const SwApi *(*)(uint32_t))(void *)GetProcAddress(exe, SW_API_EXPORT);
    NA.api = get ? get(1) : NULL;
    NA.enemies = (int (*)(uint64_t *, float (*)[3], int32_t *, int))(void *)GetProcAddress(exe, "linkpos_enemies");
    const SwApi *a = NA.api;
    if (SW_HAS(a, site) && SW_HAS(a, in_guest) && SW_HAS(a, readable) && SW_HAS(a, write))
        NA.ptr_site = a->site("autoplace_mgr_ptr"), NA.vt_site = a->site("autoplace_mgr_vt");
    NA.sites = NA.ptr_site && NA.vt_site;
}
static const char *na_sites_txt(void) {
    return NA.sites ? "sites ok"
           : !NA.api ? "this exe has no sw_api: nothing will be written"
                     : "NO autoplace_mgr_ptr / autoplace_mgr_vt row: NEEDS EXE RELINK with the s289 sora_sites.h (or another game "
                       "version): nothing will be written";
}

// live peers, the rule of party_mp_live (sora_party.h): a used slot whose peer is PEER_LIVE (NET.on is asked on its own)
static int na_live_peers(void) {
    int n = 0;
    for (int k = 0; k < NET_MAX_PEERS; k++) n += NET.slot[k].used && NET.slot[k].p.st == PEER_LIVE;
    return n;
}

// the AutoPlacementMgr, read-only checks: the singleton pointer re-read, in guest memory, readable for the vtable and NA_SPAN,
// its vtable = the row's. 0 = none (before the singleton exists / after its disposer, a reboot, another object, no rows).
static uint64_t na_mgr_peek(void) {
    const SwApi *a = NA.api;
    if (!NA.sites || a->readable(NA.ptr_site, 8) != 8) return 0;
    uint64_t m = *(const volatile uint64_t *)(uintptr_t)NA.ptr_site;
    bool ig = m && a->in_guest(m, NA_MGR_SIZE);
    bool rd = ig && a->readable(m, 8) == 8 && a->readable(m + NA_PAUSE_OFF, NA_SPAN) == NA_SPAN;
    uint64_t vt = rd ? *(const volatile uint64_t *)(uintptr_t)m : 0;
    return na_mgr_ok(m, ig, rd, vt, NA.vt_site) ? m : 0;
}

// a driven frame while the session wants the hold: the checks before the store; at a new mgr address the first store goes
// through api->write (committed READWRITE pages only). true = NA.mgr may be stored to this frame.
static bool na_mgr_ready(uint64_t now) {
    na_bind();
    uint64_t m = na_mgr_peek();
    if ((m != NA.mgr || !NA.said) && g_net_noauto_log) {
        if (m) LOG("[noauto] AutoPlacementMgr at %#llx (vtable ok)\n", (unsigned long long)m);
        else LOG("[noauto] AutoPlacementMgr pointer missing (%s): not writing\n", NA.sites ? "0 / not readable / another vtable" : na_sites_txt());
    }
    NA.mgr = m, NA.said = true;
    if (!m) return false;
    if (m == NA.mgr_rw) return true;
    if (m == NA.mgr_bad && now < NA.bad_retry) return false;
    uint64_t at = m + NA_HOLD_OFF + NA_TYPE_ENEMY;
    int cur = *(const volatile int8_t *)(uintptr_t)at;
    int v = na_store_val(cur);
    int8_t s = (int8_t)(v < 0 ? cur : v);  // (5, or BotW's own value written back: the write is the RW check)
    if (!NA.api->write(at, &s, 1)) {
        NA.mgr_bad = m, NA.bad_retry = now + NA_RETRY_MS, NA.st.refused++;
        if (g_net_noauto_log) LOG("[noauto] the hold byte at %#llx is not writable (api->write refused): not writing, again in 5 s\n", (unsigned long long)at);
        return false;
    }
    if (v >= 0) NA.st.stores++;
    NA.mgr_rw = m, NA.mgr_bad = 0;
    return true;
}

// the store (only right after na_mgr_ready said yes this frame)
static void na_store(void) {
    volatile int8_t *b = (volatile int8_t *)(uintptr_t)(NA.mgr + NA_HOLD_OFF + NA_TYPE_ENEMY);
    int v = na_store_val(*b);
    if (v >= 0) *b = (int8_t)v, NA.st.stores++;
    NA.st.held++;
}

// the mgr's numbers for a log line (read-only)
static void na_mgr_txt(char *b, size_t cap) {
    na_bind();
    uint64_t m = na_mgr_peek();
    if (m)
        snprintf(b, cap, "mgr %#llx: type-0 hold byte %d, BotW counter %u, pause counter %u", (unsigned long long)m,
                 (int)*(const volatile int8_t *)(uintptr_t)(m + NA_HOLD_OFF + NA_TYPE_ENEMY),
                 (unsigned)*(const volatile uint32_t *)(uintptr_t)(m + NA_CNT_OFF + 4u * NA_TYPE_ENEMY),
                 (unsigned)*(const volatile uint32_t *)(uintptr_t)(m + NA_PAUSE_OFF));
    else snprintf(b, cap, "no mgr (%s)", NA.sites ? "title / loading / another vtable" : na_sites_txt());
}

// the map HashId of a listed actor, as ah_actor_hid (sora_airhold.h) but through the exe's readable: 0 = no map object
// (auto-placed) / 0xFFFFFFFF / unreadable
static uint32_t na_actor_hid(uint64_t a) {
    if (!a || NA.api->readable(a + 0x7c8, 8) != 8) return 0;
    uint64_t mo = *(const volatile uint64_t *)(uintptr_t)(a + 0x7c8);
    if (!mo || NA.api->readable(mo + 0x58, 4) != 4) return 0;
    uint32_t h = *(const volatile uint32_t *)(uintptr_t)(mo + 0x58);
    return h == 0xFFFFFFFFu ? 0 : h;
}
// the enemies loaded near Link now (the exe's list: drawn actors, gone 60 frames after their last draw). false = no list.
static bool na_count_now(NaCount *c) {
    na_bind();
    if (!NA.enemies || !SW_HAS(NA.api, actor_name) || !SW_HAS(NA.api, readable)) return false;
    uint64_t a[64];
    float p[64][3];
    int32_t l[64];
    NaEnemy e[64];
    int n = NA.enemies(a, p, l, 64);
    if (n < 0) n = 0;
    if (n > 64) n = 64;
    for (int i = 0; i < n; i++) {
        e[i].name = NA.api->actor_name(a[i]);
        e[i].hid = na_actor_hid(a[i]);
        memcpy(e[i].pos, p[i], sizeof e[i].pos);
        e[i].life = l[i];
    }
    na_count(e, n, NA.have_link ? NA.link : NULL, NA_NEAR_M, c);
    return true;
}
static void na_count_txt(char *b, size_t cap) {
    NaCount c;
    if (!na_count_now(&c)) snprintf(b, cap, "no enemy list (this exe has no linkpos_enemies / sw_api)");
    else snprintf(b, cap, "%d auto-placed (hid 0) within %d m of Link, %d in the whole list; placed %d, dragons %d", c.near0, (int)NA_NEAR_M,
                  c.all0, c.placed, c.dragons);
}

// once a frame, sora_net.h: net_frame after net_tick (driven = Link's position read this frame and no load), and net_sora_off
// (driven false). Key 0 + noautolog 0: nothing but this compare chain.
static void net_noauto_frame(bool driven, const float link_pos[3], uint64_t now) {
    if (driven && link_pos) memcpy(NA.link, link_pos, sizeof NA.link), NA.have_link = true;
    int peers = g_net_noauto ? na_live_peers() : 0;
    NaWhy w = na_block(g_net_noauto, NET.on, peers, true);  // (key + session first: off / no friend = no read, no write)
    if (w != NA_HOLD) NA.mgr_ok = false, NA.said = false;   // (a new hold checks the mgr at its first driven frame)
    else {
        if (driven) NA.mgr_ok = na_mgr_ready(now);
        w = na_block(true, true, peers, NA.mgr_ok);
    }
    NaWhy why = NA_KEY;
    int ev = na_step(&NA.s, w, &why);
    if (ev > 0) {
        NA.st.starts++;
        if (g_net_noauto_log) LOG("[noauto] hold started (%s): %d friend(s) connected, mgr %#llx, storing %d into its type-0 hold byte each world frame\n",
                                  NA_START_TXT[why], peers, (unsigned long long)NA.mgr, NA_HOLD_VAL);
    } else if (ev < 0) {
        NA.st.stops++;
        if (g_net_noauto_log) LOG("[noauto] hold stopped (%s): nothing written now; BotW's own countdown clears the byte in about 5 frames\n", NA_STOP_TXT[why]);
    }
    if (na_write_now(w, driven)) na_store();
    else if (w == NA_HOLD) NA.st.idle++;
    else if (w == NA_MGR && driven) NA.st.miss++;
    if (g_net_noauto_log && driven && link_pos && now >= NA.next_count) {
        NA.next_count = now + NA_COUNT_MS;
        char ct[200], mg[160];
        na_count_txt(ct, sizeof ct);
        na_mgr_txt(mg, sizeof mg);
        LOG("[noauto] count: %s; %s; %s\n", ct, NA_STATE_TXT[w], mg);
    }
}

// live keys (net_live_set): true = handled
static bool net_noauto_live_set(const char *key, int v) {
    if (!strcmp(key, "roomnoauto")) {
        if (g_net_noauto == (v != 0)) return true;  // (a live rewrite re-applies every line: only a change logs)
        g_net_noauto = v != 0;
        if (g_net_noauto) {
            na_bind();
            LOG("[noauto] roomnoauto 1: BotW's own enemy auto-placement (Chuchu, Keese, Stal, Octoroks, Lizalfos, Pebblits, Wizzrobes, "
                "Yiga) is held off while a friend is connected; placed enemies and dragons stay (%s)\n", na_sites_txt());
        } else
            LOG("[noauto] roomnoauto 0: BotW places its own enemies again (nothing written; a held byte runs out by the game's own "
                "countdown in about 5 frames)\n");
    } else if (!strcmp(key, "noautolog")) {
        if (g_net_noauto_log == (v != 0)) return true;
        g_net_noauto_log = v != 0;
        NA.next_count = 0;  // (the first count line at the next world frame)
        LOG("[noauto] log %s\n", g_net_noauto_log ? "on (hold start / stop, mgr found / lost, a count line every 10 s)" : "off");
    } else return false;
    return true;
}

// "net diag" (read-only, also with the key 0: the "before" numbers)
static void net_noauto_diag(void) {
    char mg[160], ct[200];
    na_mgr_txt(mg, sizeof mg);
    na_count_txt(ct, sizeof ct);
    LOG("[noauto] roomnoauto %d, noautolog %d: %s; live peers %d; frames held %u (stores %u), wanted but not in the world %u, "
        "mgr missing %u, not writable %u; starts %u, stops %u; %s; enemies: %s\n",
        (int)g_net_noauto, (int)g_net_noauto_log, NA_STATE_TXT[NA.s.last], na_live_peers(), NA.st.held, NA.st.stores, NA.st.idle,
        NA.st.miss, NA.st.refused, NA.st.starts, NA.st.stops, mg, ct);
}
#endif

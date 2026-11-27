// sora_net_horse.h (s289 objs part 4): nethorse, the horse a friend rides seen under his Sora in our world, and the riding
// flag netbump reads (a friend on a horse is not bumped into). After Savage Wind PR #294 "Online mode" (byluki, fixes by
// mstampfli; MIT): its "horse" / "tack" pieces (OnlineHorse_* model-only actors) with their requests, pool and orphans.
// Included by sora_net_objs.h (NOB, the reads, the guest calls); the pure parts: sora_net_objs_core.h nh_* (host-tested).
//   sent (nethorse 1 or netbump 1, a Melty room): while our Link rides (his state word bit 0x100000 = sora.c LINK_SW_RIDING,
//     GUESS there; or his AS a rider's, the PR's test), the nearest horse actor within 3 m (GameRomHorse*, the Master Cycle)
//     and its saddle / reins within 1.5 m of it: kind-3 pose records at 20 Hz (h = crc32 of the actor's name; the horse's AS).
//   received: a friend whose horse record is younger than 600 ms rides: netbump leaves him alone (nh_rides).
//     nethorse 1: his horse, saddle and reins as OnlineHorse_<actor> actors - their model and AS only: no AI, no physics,
//     nothing anyone can ride, hit or pick up - built at install from the player's OWN game files (tools/sorabuild/nethorse/
//     horse_bake.py, into data/overlay = the RomFS overlay; its horse.txt says which is which), where his are, 120 ms
//     behind as his Sora, playing his horse's AS.
//   A puppet he no longer uses goes to his pool (5000 m under the world; PR: "a second actor of a piece made for a player is
//   never drawn", so each is made once and kept); his pool goes when he leaves the room, the key goes off or the room ends
//   (deleted at the next frame: the calls need the game's frame). A request still under way then is deleted once made (an
//   orphan; orphans are seen to even with every key 0, until none is left).
//   A load (NET.loading, another Link actor, our Link 100 m away in a frame; PR load_update): puppets the game still has are
//   kept, else all are forgotten (the game deleted them; requests under way = orphans) and nothing is asked for 1 s; dead:
//   nothing asked for, then 3 s (PR: 60 / 180 frames).
//   The pieces and the request handles (the game writes its unit into them later, from its own threads) live in a named file
//   mapping of this process; the first view is kept (the game keeps the handles' addresses): a DLL hot reload looks for it
//   once (also with every key 0) and deletes the old copy's puppets instead of leaving them standing.
// Needs: the exe rows objs_set_mtx / objs_as_in_slot / objs_play_as / loot_request / loot_handle_take / loot_handle_release /
// loot_delete / loot_creator_got / loot_heap_got (an exe relink with the s289 sora_sites.h; 1.9.0 only), the RomFS overlay on
// (SAVAGEWIND_ROMFS_OVERLAY) and its horse.txt. Without one of them: nothing requested, one line why.
#pragma once

#define NH_POOL 6
#define NH_HANDLES 64
#define NH_ORPHANS 24
#define NH_FAILS 5          // a puppet that could not be made this often: not asked for again this session (PR)
#define NH_RETRY_MS 2000    // ... asked for again this much later after a failure (PR: 2 s)
#define NH_HANDLE_REST_MS 10000  // a handle the game let go is not handed out again this soon (PR: "not used again for a long while")
#define NH_HIDE_DY 5000.0f  // a pooled puppet: this far under our Link (PR)
#define NH_MAGIC 0x31484E53u

typedef struct { uint64_t unit, flag; } NhHandle;  // a BaseProcHandle (PR Handle): the game keeps its address until taken / released
typedef struct {
    char name[48];          // the puppet's actor, "" = none
    uint64_t actor, unit; uint32_t id;
    int16_t hd;             // its request's handle while pending, -1 none
    bool pending;
    uint8_t fails;
    uint64_t retry_at;
    char as[NET_OBJS_AS];   // the AS last played on it
} NhPiece;
typedef struct {
    uint32_t magic, size;
    uint64_t self;  // the first view's address (the game keeps &hd[i] of the requests it was given)
    NhHandle hd[NH_HANDLES]; uint64_t hd_free_at[NH_HANDLES]; uint32_t next_hd;
    NhPiece part[NET_MAX_PEERS][NH_PARTS], pool[NET_MAX_PEERS][NH_POOL], orphan[NH_ORPHANS];
    char names[NH_MAP_N][48];  // the names the requests were given (kept: GUESS that the game may read one after the call)
    uint8_t who[NET_MAX_PEERS];  // whose pieces a slot's are (his player id, 0 none)
} NhShared;
typedef struct { uint8_t n; uint64_t until, alive_since; } NhRm;  // (s292 coopfix, nethorsefar: removals by the game in a row, the wait)

static struct {
    NhShared *sh;
    bool sh_tried, map_tried, said_rows, said_overlay, said_map;
    NhMap map;
    char map_path[600];
    // ours (the sender)
    uint64_t horse, hunit; uint32_t hid; uint64_t tack[2], tunit[2]; uint32_t tid[2];
    uint64_t look_at;
    bool riding;
    // his (per friend slot)
    struct { uint8_t player; uint32_t h[NH_PARTS]; uint64_t at[NH_PARTS]; NmRing ring[NH_PARTS]; char as[NET_OBJS_AS]; bool said, said_none; uint8_t coat; } rx[NET_MAX_PEERS];
    // (s291 horsefix, nethorsecoat) ours: the coat byte of the horse we ride (nh_coat_read); his: the coat his horse puppet wears
    // (per DLL copy: a hot reload plays it again); a refused one is asked again NH_RETRY_MS later, NH_FAILS times at most
    uint8_t hcoat;
    struct { uint64_t actor; uint8_t coat, tried, fails; uint64_t retry_at; } coat[NET_MAX_PEERS];
    bool purge;  // (key off / the room over / an earlier DLL copy's: every puppet goes at the next frame)
    bool probed;  // (once per DLL copy: an earlier copy's mapping looked for)
    // loads / death (PR load_update): nothing asked for until settle_until
    bool saw_loading, have_last, was_dead; float last_L[3]; uint64_t last_la, settle_until;
    // (s292 coopfix, nethorsefar N) per friend slot: his horse far (no puppet); per piece: removals by the game in a row + the wait
    // (per DLL copy, never in the shared mapping: its size names it)
    bool toofar[NET_MAX_PEERS];
    NhRm rm[NET_MAX_PEERS][NH_PARTS];
    struct { uint32_t tx_rec, rx_rec, requested, made, failed, ask_refused, pooled, reused, deleted, orphaned, as_played, removed, loads, kept, far_on, rm_waits; } st;
} NHS;

static bool nh_rides(int k, uint64_t now) {  // (netbump: friend slot k rides now)
    return k >= 0 && k < NET_MAX_PEERS && NHS.rx[k].at[0] && NHS.rx[k].player == NET.slot[k].p.player && now - NHS.rx[k].at[0] < NH_FRESH_MS;
}

// ---- the shared state (a named file mapping: it outlives this DLL copy) ----
// anything in it: a puppet (made / being made) or an orphan request
static bool nh_any_left(const NhShared *s) {
    for (int k = 0; s && k < NET_MAX_PEERS; k++) {
        for (int p = 0; p < NH_PARTS; p++) if (s->part[k][p].actor || s->part[k][p].pending) return true;
        for (int p = 0; p < NH_POOL; p++) if (s->pool[k][p].actor || s->pool[k][p].pending) return true;
    }
    for (int i = 0; s && i < NH_ORPHANS; i++) if (s->orphan[i].pending) return true;
    return false;
}
static bool nh_orphans_left(const NhShared *s) {
    for (int i = 0; s && i < NH_ORPHANS; i++) if (s->orphan[i].pending) return true;
    return false;
}
// create = made when not there (nethorse makes puppets); else opened only when an earlier copy of this process made it
#ifndef _WIN32
// (s292, Deck build fix) Linux: no named mapping (CreateFileMapping is Windows-only) and no hot-reload DLL copies to share
// with, so the state is plain process memory, made once; "open an earlier copy's" finds none.
static NhShared *nh_attach(bool create) {
    static NhShared nh_mem; static bool nh_mem_init;
    if (!create) return NULL;
    if (!nh_mem_init) {
        memset(&nh_mem, 0, sizeof nh_mem);
        nh_mem.magic = NH_MAGIC, nh_mem.size = sizeof nh_mem, nh_mem.self = (uint64_t)(uintptr_t)&nh_mem;
        for (int k = 0; k < NET_MAX_PEERS; k++) {
            for (int p = 0; p < NH_PARTS; p++) nh_mem.part[k][p].hd = -1;
            for (int p = 0; p < NH_POOL; p++) nh_mem.pool[k][p].hd = -1;
        }
        for (int i = 0; i < NH_ORPHANS; i++) nh_mem.orphan[i].hd = -1;
        nh_mem_init = true;
    }
    return &nh_mem;
}
#else
static NhShared *nh_attach(bool create) {
    char nm[96];
    snprintf(nm, sizeof nm, "Local\\sora_s289_nethorse_%lu_%u", (unsigned long)GetCurrentProcessId(), (unsigned)sizeof(NhShared));
    HANDLE h = create ? CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)sizeof(NhShared), nm)
                      : OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, nm);
    bool existed = !create || (h && GetLastError() == ERROR_ALREADY_EXISTS);
    if (!h) {
        if (create) LOG("[net] horse: no shared state (CreateFileMapping %lu): no puppets\n", (unsigned long)GetLastError());
        return NULL;  // (opening: none = nethorse never made puppets in this process)
    }
    NhShared *s = (NhShared *)MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(NhShared));
    if (!s) { LOG("[net] horse: no shared state (MapViewOfFile %lu): no puppets\n", (unsigned long)GetLastError()); CloseHandle(h); return NULL; }
    // (the handle stays open and the view mapped for the process: the game may write a handle any time)
    if (existed && s->magic == NH_MAGIC && s->size == sizeof(NhShared)) {
        // the first view, when it is still mapped here (a DLL copy's views outlive it): the game keeps its handles' addresses
        NhShared *first = (NhShared *)(uintptr_t)s->self;
        MEMORY_BASIC_INFORMATION mi;
        if (first && first != s && VirtualQuery(first, &mi, sizeof mi) == sizeof mi && mi.State == MEM_COMMIT && mi.Type == MEM_MAPPED &&
            mi.AllocationBase == (void *)first && first->magic == NH_MAGIC && first->self == (uint64_t)(uintptr_t)first)
            UnmapViewOfFile(s), s = first;
        if (nh_any_left(s)) NHS.purge = true, LOG("[net] horse: an earlier DLL copy's puppets / requests found: deleted at the next frame\n");
    } else if (create) {
        memset(s, 0, sizeof *s);
        s->magic = NH_MAGIC, s->size = sizeof *s, s->self = (uint64_t)(uintptr_t)s;
        for (int k = 0; k < NET_MAX_PEERS; k++) {
            for (int p = 0; p < NH_PARTS; p++) s->part[k][p].hd = -1;
            for (int p = 0; p < NH_POOL; p++) s->pool[k][p].hd = -1;
        }
        for (int i = 0; i < NH_ORPHANS; i++) s->orphan[i].hd = -1;
    } else {  // (opened, but not ours / another layout: left alone)
        UnmapViewOfFile(s), CloseHandle(h);
        return NULL;
    }
    return s;
}
#endif  // _WIN32
static NhShared *nh_shared(void) {
    if (NHS.sh || NHS.sh_tried) return NHS.sh;
    NHS.sh_tried = true;
    NHS.sh = nh_attach(true);
    return NHS.sh;
}
// once per DLL copy, every key 0 too (net_objs_frame): an earlier copy's puppets / requests are deleted at the next frame
static void nh_old_copy_probe(void) {
    if (NHS.probed) return;
    NHS.probed = true;
    if (NHS.sh) return;
    NhShared *s = nh_attach(false);
    if (s) NHS.sh = s, NHS.sh_tried = true;
}

// ---- may we make puppets here? (rows, the overlay, horse.txt, the shared state) ----
static bool nh_ready(void) {
    if (!NOB.horse_ok) {
        if (!NHS.said_rows) NHS.said_rows = true, LOG("[net] horse: no puppets: %s\n", no_rows_txt("horse"));
        return false;
    }
    const char *ov = getenv("SAVAGEWIND_ROMFS_OVERLAY");
    if (!ov || !*ov) {
        if (!NHS.said_overlay) NHS.said_overlay = true, LOG("[net] horse: no puppets: the RomFS overlay is off (SAVAGEWIND_ROMFS_OVERLAY unset: the game cannot load OnlineHorse_*)\n");
        return false;
    }
    if (!NHS.map_tried) {
        NHS.map_tried = true;
        snprintf(NHS.map_path, sizeof NHS.map_path, "%s/horse.txt", ov);
        FILE *f = fopen(NHS.map_path, "rb");
        static char txt[16384];
        size_t n = f ? fread(txt, 1, sizeof txt - 1, f) : 0;
        if (f) fclose(f);
        txt[n] = 0;
        nh_map_parse(txt, &NHS.map);
        LOG("[net] horse: %s: %d puppets%s\n", NHS.map_path, NHS.map.n, NHS.map.n ? "" : " (missing / empty: the s289 horse step makes it, tools/sorabuild/nethorse/horse_install.py)");
    }
    if (!NHS.map.n) return false;
    return nh_shared() != NULL;
}

// ---- guest calls for the puppets (only inside linkpos_frame) ----
static uint64_t nh_x0(uint64_t fn, uint64_t x0) {  // fn(x0) -> x0
    if (!fn || !NOB.api || !NOB.api->in_frame()) return 0;
    SwArgs a;
    memset(&a, 0, sizeof a);
    a.x[0] = x0, a.nx = 6;  // (x1..x5 0, as the PR's call(): delete(actor, 0))
    SwRet r = {0};
    return NOB.api->call(NULL, fn, &a, &r) ? r.x0 : 0;
}
static bool nh_handle_busy(const NhShared *s, int i) {
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        for (int p = 0; p < NH_PARTS; p++) if (s->part[k][p].pending && s->part[k][p].hd == i) return true;
        for (int p = 0; p < NH_POOL; p++) if (s->pool[k][p].pending && s->pool[k][p].hd == i) return true;
    }
    for (int p = 0; p < NH_ORPHANS; p++) if (s->orphan[p].pending && s->orphan[p].hd == i) return true;
    return false;
}
static int nh_new_handle(NhShared *s, uint64_t now) {
    for (int k = 0; k < NH_HANDLES; k++) {
        int i = (int)(s->next_hd++ % NH_HANDLES);
        if (nh_handle_busy(s, i) || (s->hd_free_at[i] && now - s->hd_free_at[i] < NH_HANDLE_REST_MS)) continue;
        memset(&s->hd[i], 0, sizeof s->hd[i]);
        return i;
    }
    return -1;
}
static const char *nh_name_kept(NhShared *s, const char *name) {  // (a copy that stays: one per map entry)
    for (int i = 0; i < NH_MAP_N; i++)
        if (!strcmp(s->names[i], name)) return s->names[i];
    for (int i = 0; i < NH_MAP_N; i++)
        if (!s->names[i][0]) { snprintf(s->names[i], sizeof s->names[i], "%s", name); return s->names[i]; }
    return NULL;
}
// PR request_p: x0 the creator (*P_CREATOR's), x1 the name, x2 the heap (*P_HEAP_OWNER's + 0x28), x3 a BaseProcHandle, x4 0
// (no params), x5 0, w6 1, s0 0 -> true when asked; its resources load on the game's threads, then the unit's state is 2
static bool nh_request(NhPiece *p, const char *name, uint64_t now) {
    NhShared *s = NHS.sh;
    uint64_t cr = no_rd64(NOB.creator_p), ho = no_rd64(NOB.heap_p);
    cr = cr ? no_rd64(cr) : 0, ho = ho ? no_rd64(ho) : 0;
    uint64_t heap = ho ? no_rd64(ho + 0x28) : 0;
    const char *kept = nh_name_kept(s, name);
    int i = cr && heap && kept ? nh_new_handle(s, now) : -1;
    if (i < 0 || !NOB.api->in_frame()) return false;
    SwArgs a;
    memset(&a, 0, sizeof a);
    a.x[0] = cr, a.x[1] = (uint64_t)(uintptr_t)kept, a.x[2] = heap, a.x[3] = (uint64_t)(uintptr_t)&s->hd[i], a.x[4] = 0, a.x[5] = 0, a.x[6] = 1, a.nx = 7;
    a.s[0] = 0, a.nv = 1;  // (s0 0 as the PR's call_f: unset, it would keep the caller's s0)
    SwRet r = {0};
    if (!NOB.api->call(NULL, NOB.req, &a, &r) || !(r.x0 & 1)) return false;
    p->hd = (int16_t)i, p->pending = true;
    return true;
}
// a pending request's unit: 2 ready, 3 / 5 failed (PR), else still loading (-1)
static int nh_unit_state(const NhPiece *p) {
    if (!p->pending || p->hd < 0) return -1;
    uint64_t unit = *(volatile uint64_t *)&NHS.sh->hd[p->hd].unit;
    return unit ? (int)no_rd32(unit) : -1;
}
static void nh_handle_done(NhPiece *p, uint64_t now) {
    if (p->hd >= 0) NHS.sh->hd_free_at[p->hd] = now ? now : 1;
    p->hd = -1, p->pending = false;
}
// took it: its actor (woken by the take, PR FN_HANDLE_TAKE), or failed: released (FN_HANDLE_RELEASE). -> true = took one
static bool nh_take(NhPiece *p, uint64_t now) {
    int st = nh_unit_state(p);
    if (st == 2) {
        uint64_t a = nh_x0(NOB.take, (uint64_t)(uintptr_t)&NHS.sh->hd[p->hd]);
        nh_handle_done(p, now);
        if (!a || !NO_VALID(a)) {  // (s289 review3 #5: a failure like a release: asked again NH_RETRY_MS later, NH_FAILS at most)
            p->fails++, p->retry_at = now + NH_RETRY_MS, NHS.st.failed++;
            return false;
        }
        p->actor = a, p->unit = no_rd64(a + 0x68), p->id = no_rd32(a + 0x60), p->as[0] = 0;
        return true;
    }
    if (st == 3 || st == 5) nh_x0(NOB.release, (uint64_t)(uintptr_t)&NHS.sh->hd[p->hd]), nh_handle_done(p, now), p->fails++, p->retry_at = now + NH_RETRY_MS, NHS.st.failed++;
    return false;
}
static void nh_hide(const NhPiece *p, const float L[3]) {
    float m[12] = {1, 0, 0, L ? L[0] : 0, 0, 1, 0, (L ? L[1] : 0) - NH_HIDE_DY, 0, 0, 1, L ? L[2] : 0};
    if (p->actor && no_alive(p->actor, p->unit, p->id)) no_call(NOB.set_mtx, p->actor, m);
}
// a piece goes for good: made = deleted (FN_DELETE); still under way = an orphan, deleted once made (PR: a release does not
// stop a request under way)
static void nh_delete(NhPiece *p) {
    if (p->actor && no_alive(p->actor, p->unit, p->id)) nh_x0(NOB.del, p->actor), NHS.st.deleted++;
    if (p->pending) {
        NhShared *s = NHS.sh;
        NhPiece *o = NULL;
        for (int i = 0; i < NH_ORPHANS && !o; i++)
            if (!s->orphan[i].pending) o = &s->orphan[i];
        if (o) *o = *p, o->actor = 0, NHS.st.orphaned++;
        // (no orphan slot left: its handle is kept busy by nobody now; the actor it makes would stay unmoved - logged)
        else LOG("[net] horse: no orphan slot left for %s's request: it may stand unmoved once made\n", p->name);
    }
    memset(p, 0, sizeof *p);
    p->hd = -1;
}
static void nh_orphans_update(uint64_t now) {
    for (int i = 0; i < NH_ORPHANS; i++) {
        NhPiece *q = &NHS.sh->orphan[i];
        if (!q->pending) continue;
        int st = nh_unit_state(q);
        if (st == 2) {
            uint64_t a = nh_x0(NOB.take, (uint64_t)(uintptr_t)&NHS.sh->hd[q->hd]);
            if (a && NO_VALID(a)) nh_x0(NOB.del, a), NHS.st.deleted++;
            nh_handle_done(q, now), memset(q, 0, sizeof *q), q->hd = -1;
        } else if (st == 3 || st == 5) {
            nh_x0(NOB.release, (uint64_t)(uintptr_t)&NHS.sh->hd[q->hd]);
            nh_handle_done(q, now), memset(q, 0, sizeof *q), q->hd = -1;
        }
    }
}
// his piece he stopped using: to his pool, out of sight (PR pool_piece); a full pool: its oldest goes
static void nh_pool_piece(int k, NhPiece *p, const float L[3]) {
    if (!p->pending && !(p->actor && no_alive(p->actor, p->unit, p->id))) { memset(p, 0, sizeof *p), p->hd = -1; return; }
    nh_hide(p, L);
    NhPiece *slot = NULL;
    for (int i = 0; i < NH_POOL && !slot; i++)
        if (!NHS.sh->pool[k][i].actor && !NHS.sh->pool[k][i].pending) slot = &NHS.sh->pool[k][i];
    if (!slot) slot = &NHS.sh->pool[k][0], nh_delete(slot);
    *slot = *p;
    memset(p, 0, sizeof *p), p->hd = -1;
    NHS.st.pooled++;
}
static bool nh_from_pool(int k, NhPiece *p, const char *want) {
    for (int i = 0; i < NH_POOL; i++) {
        NhPiece *q = &NHS.sh->pool[k][i];
        if ((!q->actor && !q->pending) || strcmp(q->name, want)) continue;
        if (q->actor && !no_alive(q->actor, q->unit, q->id)) { memset(q, 0, sizeof *q), q->hd = -1; continue; }
        *p = *q, p->as[0] = 0;
        memset(q, 0, sizeof *q), q->hd = -1;
        NHS.st.reused++;
        return p->actor != 0;
    }
    return false;
}
static void nh_pool_update(const float L[3], uint64_t now) {  // (PR pool_update: the pool's pieces still being made, as they come)
    for (int k = 0; k < NET_MAX_PEERS; k++)
        for (int i = 0; i < NH_POOL; i++) {
            NhPiece *q = &NHS.sh->pool[k][i];
            if (!q->pending) continue;
            if (nh_take(q, now)) nh_hide(q, L), NHS.st.made++;
            else if (!q->pending && !q->actor) memset(q, 0, sizeof *q), q->hd = -1;
        }
}
// p should be `want` ("" none): a piece that changed goes to the pool, the new one comes from it or is asked for (PR
// piece_update; may_ask false = loading / dead / settling: nothing new asked for). -> true = it is there just now (made /
// from the pool): its AS is played again
static bool nh_piece_update(int k, NhPiece *p, const char *want, const float L[3], uint64_t now, const char *who, bool may_ask) {
    if ((p->actor || p->pending || p->name[0]) && strcmp(p->name, want)) nh_pool_piece(k, p, L);
    if (!p->actor && !p->pending && want[0] && nh_from_pool(k, p, want)) {
        if (g_net_horse_log) LOG("[net] horse: %s's %s again (from his pool)\n", who, want);
        return true;
    }
    snprintf(p->name, sizeof p->name, "%s", want);
    if (p->actor && !no_alive(p->actor, p->unit, p->id)) {
        if (g_net_horse_log) LOG("[net] horse: the game removed %s's %s: made again\n", who, want);
        p->actor = 0, p->fails = 0, NHS.st.removed++;
    }
    if (!want[0] || p->actor) return false;
    if (p->pending) {
        bool took = nh_take(p, now);
        if (took) NHS.st.made++;
        if (g_net_horse_log && took) LOG("[net] horse: %s's %s made (actor %#llx)\n", who, want, (unsigned long long)p->actor);
        if (g_net_horse_log && !took && !p->pending) LOG("[net] horse: %s's %s could not be made (%d of %d tries)\n", who, want, p->fails, NH_FAILS);
        return took;
    }
    if (p->fails >= NH_FAILS || now < p->retry_at || !may_ask) return false;
    if (nh_request(p, want, now)) NHS.st.requested++;
    else {
        p->fails++, p->retry_at = now + NH_RETRY_MS, NHS.st.ask_refused++;
        if (p->fails == 1) LOG("[net] horse: can't ask the game for a %s (refused)\n", want);
    }
    return false;
}

// every puppet goes (the key off, the room over, an earlier DLL copy's)
static void nh_purge(void) {
    NhShared *s = NHS.sh;
    if (!s) return;
    int n = 0;
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        for (int p = 0; p < NH_PARTS; p++) n += s->part[k][p].actor || s->part[k][p].pending, nh_delete(&s->part[k][p]);
        for (int p = 0; p < NH_POOL; p++) n += s->pool[k][p].actor || s->pool[k][p].pending, nh_delete(&s->pool[k][p]);
        s->who[k] = 0;
    }
    if (n && g_net_horse_log) LOG("[net] horse: %d puppets / requests gone\n", n);
}

// ---- ours: the horse our Link rides (20 Hz, with the other records) ----
// (s291 horsefix, nethorsecoat) the coat of horse a: its slot-4 AS (Collar_Shader...: the family) + its int at +0xb78 (the
// frame), read with each look (NH_LOOK_MS); -> NHS.hcoat (0 = not known: not rolled yet / another AS there)
// (s291 coat, lab diag, nethorselog 1) ONE line per horse actor (once per DLL copy): every AnmTarget / AS slot's playing AS (the
// AS manager's own counts, sora_net_objs_core.h no_as_in_slot_args) and, for a real horse (`real`), its coat frames +0xb78 /
// +0xb7c (a puppet is a DemoNPC: no such fields). Reads only (objs_as_in_slot returns a pointer, changes nothing).
static void nh_slots_say(uint64_t a, const char *what, bool real) {
    static uint64_t said[16];
    static int said_n;
    if (!g_net_horse_log || !a || !NOB.as_ok || !NOB.api->in_frame()) return;
    for (int i = 0; i < said_n && i < 16; i++)
        if (said[i] == a) return;
    said[said_n++ % 16] = a;
    uint64_t mgr = no_rd64(a + 0x560);
    if (!NO_VALID(mgr)) { LOG("[net] horse: AS slots of %s %s (actor %#llx): no AS manager\n", what, NOB.api->actor_name(a), (unsigned long long)a); return; }
    int nt = (int)no_rd32(mgr + 0xb8), ns[NO_AS_TARGETS_MAX] = {0};
    uint64_t arr = no_rd64(mgr + 0xc0);
    static char nm[NO_AS_TARGETS_MAX][NO_AS_SLOTS_MAX][NET_OBJS_AS];
    memset(nm, 0, sizeof nm);
    for (int t = 0; t < nt && t < NO_AS_TARGETS_MAX && NO_VALID(arr); t++) {
        ns[t] = (int)no_rd32(arr + (uint64_t)t * 0x50 + 0x20);
        for (int s = 0; s < ns[t] && s < NO_AS_SLOTS_MAX; s++) no_playing_at(a, t, s, nm[t][s]);
    }
    char line[700];
    nh_slots_line(line, sizeof line, NO_VALID(arr) ? nt : 0, ns, (const char (*)[NO_AS_SLOTS_MAX][NET_OBJS_AS])nm);
    if (real)
        LOG("[net] horse: AS slots of %s %s (actor %#llx): %s; +%#x = %d, +%#x = %d\n", what, NOB.api->actor_name(a), (unsigned long long)a, line,
            NH_COAT_OFF, (int)(int32_t)no_rd32(a + NH_COAT_OFF), NH_COAT_OFF + 4, (int)(int32_t)no_rd32(a + NH_COAT_OFF + 4));
    else LOG("[net] horse: AS slots of %s %s (actor %#llx): %s\n", what, NOB.api->actor_name(a), (unsigned long long)a, line);
}
static void nh_coat_read(uint64_t a) {
    static uint64_t said_for;  // (s291 coat: the first read of each horse is said too, "not known" included)
    char as[NET_OBJS_AS];
    int fam = nh_coat_family(no_playing_in(a, NH_COAT_SLOT, as));
    int32_t fr = (int32_t)no_rd32(a + NH_COAT_OFF);
    uint8_t c = nh_coat_byte(fam, fr);
    if ((c != NHS.hcoat || said_for != a) && g_net_horse_log) {
        said_for = a;
        char nm[16];
        nh_coat_name(c, nm);
        LOG("[net] horse: our horse's coat %s (slot %d AS %s, +%#x = %d)%s\n", c ? nm : "not known", NH_COAT_SLOT, as[0] ? as : "-",
            NH_COAT_OFF, (int)fr, c ? ": it goes to the room" : "");
    }
    NHS.hcoat = c;
}
static void nh_look(uint64_t la, uint64_t now) {
    NHS.look_at = now + NH_LOOK_MS;
    NHS.horse = 0, NHS.tack[0] = NHS.tack[1] = 0;
    float lm[12];
    if (!no_mtx(la, lm)) return;
    int n = no_all_actors(no_rd64(la + 0x68), g_no_act, NO_MAX_ACTORS);
    float bd = NH_NEAR, td[2] = {NH_TACK, NH_TACK};
    for (int i = 0; i < n; i++) {
        uint64_t a = g_no_act[i];
        float m[12];
        if (a == la || no_rd8(a + 0x64) == 3 || !nh_horse(NOB.api->actor_name(a)) || !no_mtx(a, m)) continue;
        float d = no_d3((const float[3]){lm[3], lm[7], lm[11]}, (const float[3]){m[3], m[7], m[11]});
        if (d < bd) bd = d, NHS.horse = a;
    }
    float hm[12];
    if (!NHS.horse || !no_mtx(NHS.horse, hm)) { NHS.horse = 0; return; }
    NHS.hunit = no_rd64(NHS.horse + 0x68), NHS.hid = no_rd32(NHS.horse + 0x60);
    if (g_net_horse_coat) nh_coat_read(NHS.horse);  // (s291 horsefix)
    nh_slots_say(NHS.horse, "our horse", true);      // (s291 coat: nethorselog 1, once per horse)
    for (int i = 0; i < n; i++) {
        uint64_t a = g_no_act[i];
        float m[12];
        int w = nh_tack(NOB.api->actor_name(a));
        if (w < 0 || no_rd8(a + 0x64) == 3 || !no_mtx(a, m)) continue;
        float d = no_d3((const float[3]){hm[3], hm[7], hm[11]}, (const float[3]){m[3], m[7], m[11]});
        if (d < td[w]) td[w] = d, NHS.tack[w] = a, NHS.tunit[w] = no_rd64(a + 0x68), NHS.tid[w] = no_rd32(a + 0x60);
    }
}
// (20 Hz) anyone = someone in the room gets them: else nothing is read or sent
static void nh_sample(NetObjRec *r, int *n, int *pk, bool anyone, uint64_t now) {
    if (!anyone) { NHS.riding = false, NHS.horse = 0; return; }  // (alone)
    uint64_t la = no_link_actor();
    char as[NET_OBJS_AS];
    uint32_t sw = NOB.state_word ? NOB.state_word() : 0;
    // (s289 review3 #7: the AS test is a game call: nethorse 1 only; netbump 1 alone reads the state word, no call)
    bool ride = la && !NET.dead && ((sw & NB_SW_RIDING) || (g_net_horse && NOB.as_ok && nh_riding_as(no_playing(la, as))));
    if (ride != NHS.riding && g_net_horse_log) LOG("[net] horse: our Link %s (state %#x)\n", ride ? "rides: his horse goes to the room" : "rides no more", sw);
    NHS.riding = ride;
    if (!ride) { NHS.horse = 0; return; }
    if (!NHS.horse || !no_alive(NHS.horse, NHS.hunit, NHS.hid) || now >= NHS.look_at) {
        uint64_t was = NHS.horse;
        nh_look(la, now);
        if (g_net_horse_log && NHS.horse != was && NHS.horse) LOG("[net] horse: riding %s (saddle %s, reins %s)\n", NOB.api->actor_name(NHS.horse),
                                                                   NHS.tack[0] ? NOB.api->actor_name(NHS.tack[0]) : "-", NHS.tack[1] ? NOB.api->actor_name(NHS.tack[1]) : "-");
    }
    if (!NHS.horse) return;
    for (int part = 0; part < NH_PARTS; part++) {
        uint64_t a = part ? NHS.tack[part - 1] : NHS.horse;
        if (!a || (part && !no_alive(a, NHS.tunit[part - 1], NHS.tid[part - 1]))) continue;
        NetObjRec *o = &r[*n];
        memset(o, 0, sizeof *o);
        if (!no_mtx(a, o->m)) continue;
        o->kind = NOR_HORSE, o->flags = (uint8_t)(part == 0 ? NORH_HORSE : part == 1 ? NORH_SADDLE : NORH_REINS);
        o->h = nh_crc32(NOB.api->actor_name(a));
        if (!part && NOB.as_ok) no_playing(a, o->as);
        if (!part && g_net_horse_coat) o->coat = NHS.hcoat;  // (s291 horsefix: else 0, as before)
        NHS.st.tx_rec++;
        if (++*n == NET_OBJS_N) no_flush(r, n, pk);
    }
}

// ---- his: a kind-3 record (net_objs_recv) ----
static void nh_recv(int k, const NetObjRec *r, uint64_t now) {
    int part = nh_part(r->flags);
    if (part < 0 || k < 0 || k >= NET_MAX_PEERS) return;
    NHS.st.rx_rec++;
    if (NHS.rx[k].player != NET.slot[k].p.player) memset(&NHS.rx[k], 0, sizeof NHS.rx[k]), NHS.rx[k].player = NET.slot[k].p.player;
    if (NHS.rx[k].h[part] != r->h) NHS.rx[k].ring[part].n = 0;  // (another horse: its own stream)
    NHS.rx[k].h[part] = r->h, NHS.rx[k].at[part] = now ? now : 1;
    nm_ring_push(&NHS.rx[k].ring[part], now, r->m);
    if (!part) memcpy(NHS.rx[k].as, r->as, sizeof NHS.rx[k].as), NHS.rx[k].coat = r->coat;
    if (!part && g_net_horse_log && !NHS.rx[k].said) {
        const char *on = nh_map_find(&NHS.map, r->h);
        LOG("[net] horse: %s rides (%s, AS %s)\n", NET.slot[k].p.name[0] ? NET.slot[k].p.name : "a friend", on ? on : "a horse not in horse.txt / not loaded yet", r->as[0] ? r->as : "-");
    }
    if (!part) NHS.rx[k].said = true;
}

// a load / another Link (PR load_update): puppets the game still has are kept; none = the game deleted them: forgotten (not
// deleted: their actors are gone; requests under way = orphans) and nothing asked for NH_SETTLE_LOAD_MS. -> may we ask now?
#define NH_SETTLE_LOAD_MS 1000   // (PR: 60 frames)
#define NH_SETTLE_DEAD_MS 3000   // (PR: 180 frames, after a death / Link removed)
#define NH_JUMP_M 100.0f         // our Link this far in a frame = a load (PR)
static bool nh_settle(const float L[3], uint64_t now) {
    uint64_t la = no_link_actor();
    bool jump = NHS.have_last && L && no_d3(L, NHS.last_L) > NH_JUMP_M, new_link = NHS.have_last && la != NHS.last_la;
    bool load = NHS.saw_loading || new_link || jump;
    NHS.saw_loading = false;
    if (L) memcpy(NHS.last_L, L, sizeof NHS.last_L);
    NHS.have_last = L != NULL, NHS.last_la = la;
    if (load) {
        NhShared *s = NHS.sh;
        bool alive = false;
        for (int k = 0; k < NET_MAX_PEERS && !alive; k++) {
            for (int p = 0; p < NH_PARTS; p++) alive |= s->part[k][p].actor && no_alive(s->part[k][p].actor, s->part[k][p].unit, s->part[k][p].id);
            for (int p = 0; p < NH_POOL; p++) alive |= s->pool[k][p].actor && no_alive(s->pool[k][p].actor, s->pool[k][p].unit, s->pool[k][p].id);
        }
        NHS.st.loads++;
        if (alive) NHS.st.kept++;
        else {
            for (int k = 0; k < NET_MAX_PEERS; k++) {  // (nh_delete: an actor not alive is not called; pending = an orphan)
                for (int p = 0; p < NH_PARTS; p++) nh_delete(&s->part[k][p]);
                for (int p = 0; p < NH_POOL; p++) nh_delete(&s->pool[k][p]);
            }
            uint64_t until = now + (new_link ? NH_SETTLE_DEAD_MS : NH_SETTLE_LOAD_MS);
            if (until > NHS.settle_until) NHS.settle_until = until;
        }
        if (g_net_horse_log) LOG("[net] horse: a load (%s): %s\n", jump ? "our Link moved > 100 m" : new_link ? "another Link actor" : "loading",
                                 alive ? "the puppets are still there: kept" : "puppets forgotten (the game removed them), none asked for a while");
    }
    if (NET.dead || !la) {  // (dead / no Link: nothing asked for; then NH_SETTLE_DEAD_MS)
        NHS.was_dead = true;
        return false;
    }
    if (NHS.was_dead) NHS.was_dead = false, NHS.settle_until = now + NH_SETTLE_DEAD_MS;
    return now >= NHS.settle_until;
}

// (s291 horsefix, nethorsecoat) his horse puppet p wears his horse's coat: the bake's Coat_<family>_<frame> AS in slot 4, played
// once per (puppet, coat); refused (an older bake has no Coat_* AS): asked again NH_RETRY_MS later, NH_FAILS times at most
static void nh_coat_wear(int k, NhPiece *p, uint64_t now, const char *who) {
    uint8_t c = NHS.rx[k].coat;
    char nm[16];
    if (!nh_coat_name(c, nm) || no_rd8(p->actor + 0x64) != 1) return;
    if (NHS.coat[k].actor == p->actor && NHS.coat[k].coat == c) return;  // (wears it)
    if (NHS.coat[k].actor != p->actor || NHS.coat[k].tried != c) NHS.coat[k].fails = 0, NHS.coat[k].retry_at = 0, NHS.coat[k].coat = 0;  // (a new try)
    if (NHS.coat[k].fails >= NH_FAILS || now < NHS.coat[k].retry_at) return;
    bool ok = no_play_as_in(p->actor, nm, false, NH_COAT_SLOT);
    NHS.coat[k].actor = p->actor, NHS.coat[k].tried = c;
    if (ok) NHS.coat[k].coat = c, NHS.coat[k].fails = 0;
    else NHS.coat[k].fails++, NHS.coat[k].retry_at = now + NH_RETRY_MS;
    if (g_net_horse_log && (ok || NHS.coat[k].fails == 1 || NHS.coat[k].fails >= NH_FAILS))
        LOG("[net] horse: %s's horse puppet %s %s (AS in slot %d)%s\n", who, ok ? "wears coat" : "could NOT wear coat", nm, NH_COAT_SLOT,
            ok ? "" : NHS.coat[k].fails >= NH_FAILS ? ": not asked again (no Coat_* AS in its pack? the s291 horse_bake.py makes them)" : ": asked again later");
}

// ---- every frame (nethorse 1): his puppets where his are ----
static void nh_frame(const float L[3], uint64_t now) {
    if (!nh_ready()) return;
    NhShared *s = NHS.sh;
    nh_orphans_update(now);
    bool may_ask = nh_settle(L, now);
    nh_pool_update(L, now);
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        const NetPeerSlot *sl = &NET.slot[k];
        const char *who = sl->p.name[0] ? sl->p.name : "a friend";
        if (s->who[k] && (!sl->used || sl->p.player != s->who[k])) {  // (he left: his pieces and pool go; PR drop(left))
            for (int p = 0; p < NH_PARTS; p++) nh_delete(&s->part[k][p]);
            for (int p = 0; p < NH_POOL; p++) nh_delete(&s->pool[k][p]);
            if (g_net_horse_log) LOG("[net] horse: friend slot %d left: his puppets gone\n", k);
        }
        s->who[k] = sl->used ? sl->p.player : 0;
        bool here = sl->used && !sl->hidden && sl->p.st == PEER_LIVE && NHS.rx[k].player == sl->p.player;
        bool toofar = false;  // (s292 coopfix, nethorsefar N: his horse farther than N m = no puppet; key 0 = never)
        if (g_net_horse_far > 0 && here && NHS.rx[k].at[0]) {
            float hm[12], d = NAN;
            if (L && nm_ring_at(&NHS.rx[k].ring[0], now - NET_INTERP_MS, hm)) d = no_d3(L, (const float[3]){hm[3], hm[7], hm[11]});
            toofar = nh_far(NHS.toofar[k], d, (float)g_net_horse_far);
            if (toofar && !NHS.toofar[k]) NHS.st.far_on++;
            if (toofar != NHS.toofar[k] && g_net_horse_log)
                LOG("[net] horse: %s's horse is %.0f m away: %s (nethorsefar %d)\n", who, d, toofar ? "no puppet while that far (his pool)" : "near again: his puppet back",
                    g_net_horse_far);
        }
        NHS.toofar[k] = toofar;
        for (int part = 0; part < NH_PARTS; part++) {
            const char *want = "";
            if (here && !toofar && NHS.rx[k].at[part] && now - NHS.rx[k].at[part] < NH_GONE_MS) {
                const char *on = nh_map_find(&NHS.map, NHS.rx[k].h[part]);
                if (on) want = on;
                else if (!NHS.rx[k].said_none && g_net_horse_log) NHS.rx[k].said_none = true, LOG("[net] horse: %s's %s %#x has no puppet in horse.txt\n", who, part ? "tack" : "horse", NHS.rx[k].h[part]);
            }
            NhPiece *p = &s->part[k][part];
            bool ask = may_ask;
            if (g_net_horse_far > 0) {  // (s292 coopfix: one the game removed is asked for again after a wait, doubling while it goes on)
                NhRm *r = &NHS.rm[k][part];
                if (p->actor && !no_alive(p->actor, p->unit, p->id)) {
                    r->n = r->n < 255 ? (uint8_t)(r->n + 1) : 255, r->until = now + nh_rm_wait_ms(r->n), r->alive_since = 0, NHS.st.rm_waits++;
                    if (g_net_horse_log && want[0]) LOG("[net] horse: the game removed %s's %s (%d in a row): asked for again in %u s\n", who, p->name, r->n, nh_rm_wait_ms(r->n) / 1000);
                } else if (p->actor) {
                    if (!r->alive_since) r->alive_since = now ? now : 1;
                    else if (now - r->alive_since >= NH_RM_STABLE_MS) r->n = 0;
                }
                if (now < r->until) ask = false;
            }
            bool fresh = nh_piece_update(k, p, want, L, now, who, ask);
            if (!p->actor) continue;
            float m[12];
            if (nm_ring_at(&NHS.rx[k].ring[part], now - NET_INTERP_MS, m)) no_call(NOB.set_mtx, p->actor, m);
            const char *as = part ? "" : NHS.rx[k].as;
            if (as[0] && (fresh || strcmp(p->as, as)) && no_rd8(p->actor + 0x64) == 1) {  // (PR: played when it changes, the actor running)
                bool ok = no_play_as(p->actor, as, true);
                snprintf(p->as, sizeof p->as, "%s", as);
                NHS.st.as_played += ok;
                if (g_net_horse_log > 1 || (g_net_horse_log && !ok)) LOG("[net] horse: %s's horse AS %s %s\n", who, as, ok ? "played" : "NOT played");
            }
            if (!part && g_net_horse_coat) nh_coat_wear(k, p, now, who);  // (s291 horsefix)
            // (s291 coat: nethorselog 1, once per puppet: after its first coat try / its first AS)
            if (!part && g_net_horse_log && (g_net_horse_coat ? NHS.coat[k].actor == p->actor : p->as[0] != 0)) nh_slots_say(p->actor, "his horse puppet", false);
        }
    }
}

// (s291 duelride) the nearest shown horse puppet (a friend's, nethorse 1) to `at`, flat metres; -1 none (sora_duel.h: a horse)
static float nh_puppet_near(const float at[3]) {
    float best = -1;
    for (int k = 0; g_net_horse && NHS.sh && at && k < NET_MAX_PEERS; k++) {
        const NhPiece *p = &NHS.sh->part[k][0];
        float m[12];
        if (!p->actor || p->pending || !no_alive(p->actor, p->unit, p->id) || !no_mtx(p->actor, m)) continue;
        float d = hypotf(m[3] - at[0], m[11] - at[2]);
        if (best < 0 || d < best) best = d;
    }
    return best;
}

// (s292 coopfix) every frame in a room (net_objs_frame) and before the diag: a friend's horse records go with his slot (he left /
// the slot is someone else's), and the coat his puppet wore; his puppets themselves go in nh_frame (s->who) as before
static void nh_rx_prune(void) {
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        bool have = NHS.rx[k].player || NHS.rx[k].at[0] || NHS.rx[k].at[1] || NHS.rx[k].at[2];
        if (!nh_rx_stale(have, NHS.rx[k].player, NET.slot[k].used, NET.slot[k].p.player)) continue;
        if (g_net_horse_log && NHS.rx[k].at[0]) LOG("[net] horse: friend slot %d (player %u) gone: his horse records forgotten\n", k, NHS.rx[k].player);
        memset(&NHS.rx[k], 0, sizeof NHS.rx[k]), memset(&NHS.coat[k], 0, sizeof NHS.coat[k]);
        memset(NHS.rm[k], 0, sizeof NHS.rm[k]), NHS.toofar[k] = false;  // (nethorsefar: the next friend there starts with no wait)
    }
}

static void nh_diag(void) {
    nh_rx_prune();
    int made = 0, pend = 0, pooled = 0, orph = 0;
    for (int k = 0; NHS.sh && k < NET_MAX_PEERS; k++) {
        for (int p = 0; p < NH_PARTS; p++) made += NHS.sh->part[k][p].actor != 0, pend += NHS.sh->part[k][p].pending;
        for (int p = 0; p < NH_POOL; p++) pooled += NHS.sh->pool[k][p].actor != 0 || NHS.sh->pool[k][p].pending;
    }
    for (int i = 0; NHS.sh && i < NH_ORPHANS; i++) orph += NHS.sh->orphan[i].pending;
    LOG("[net] horse: %s; our Link %s%s; sent %u records, received %u; puppets: %d shown, %d being made, %d pooled, %d orphans; "
        "asked %u (refused %u), made %u, failed %u, reused %u, removed by the game %u, deleted %u; AS played %u; loads %u (puppets kept %u); "
        "bump left a riding friend alone %u frames\n",
        g_net_horse ? (NOB.horse_ok ? "on" : "on but OFF here") : "OFF", NHS.riding ? "rides " : "does not ride",
        NHS.riding && NHS.horse && NOB.api ? NOB.api->actor_name(NHS.horse) : "", NHS.st.tx_rec, NHS.st.rx_rec, made, pend, pooled, orph,
        NHS.st.requested, NHS.st.ask_refused, NHS.st.made, NHS.st.failed, NHS.st.reused, NHS.st.removed, NHS.st.deleted, NHS.st.as_played,
        NHS.st.loads, NHS.st.kept, NOB.st.riders);
    if (g_net_horse && !NOB.horse_ok) LOG("[net] horse: %s\n", no_rows_txt("horse"));
    if (g_net_horse_far) {  // (s292 coopfix)
        int nfar = 0, wait = 0;
        uint64_t now = GetTickCount64();
        for (int k = 0; k < NET_MAX_PEERS; k++) {
            nfar += NHS.toofar[k];
            for (int p = 0; p < NH_PARTS; p++) wait += now < NHS.rm[k][p].until;
        }
        LOG("[net] horse: nethorsefar %d: %d friend(s) too far now (%u times in all), %d piece(s) waiting after the game removed them (%u removals waited)\n",
            g_net_horse_far, nfar, NHS.st.far_on, wait, NHS.st.rm_waits);
    }
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        if (!NHS.rx[k].at[0]) continue;
        const char *on = nh_map_find(&NHS.map, NHS.rx[k].h[0]);
        char cn[16];
        nh_coat_name(NHS.rx[k].coat, cn);
        bool worn = cn[0] && NHS.sh && NHS.coat[k].coat == NHS.rx[k].coat && NHS.coat[k].actor && NHS.coat[k].actor == NHS.sh->part[k][0].actor;
        LOG("[net] horse:   slot %d (player %u): horse %s %#x, AS %s, coat %s%s, last record %llu ms ago%s\n", k, NHS.rx[k].player, on ? on : "?",
            NHS.rx[k].h[0], NHS.rx[k].as[0] ? NHS.rx[k].as : "-", cn[0] ? cn : "-", !cn[0] ? "" : worn ? " (worn)" : " (not worn)",
            (unsigned long long)(GetTickCount64() - NHS.rx[k].at[0]), NHS.sh && NHS.sh->part[k][0].actor ? " (puppet shown)" : "");
    }
}

// sora_net_shots_game.h (s289 shots): the game side of `set netshots 1` / `set netrunes 1` (the wish, the keys and the wire:
// sora_net_shots.h). After Savage Wind PR #294 "Online mode" by byluki (fixes by mstampfli), MIT: sample_shots' and
// sample_pose's rules for his shots and his thrown weapon, the rune state of its pose, through reads only.
// Included by sora_net_avatar.h after sora_net_gear_draw.h: uses sora.c's link_actor / rd_ok / g_bow_on / g_k, AV / AVS /
// av_dot_pathn / av_used / AV_RESERVE / g_of_reserve / g_av_shift / g_av_local_ok / g_body, netgear's ngd_get / NGD / NGD_DIR,
// wpn_find, sora_fx_spawn / sora_fx_id / sora_sfx_id and net_rfx_room.
// DLL-side only: no guest call, no hook, no exe patch, nothing written into the game (a hot reload; no relink).
//   sample   nsg_sample (top of avatar_tick, every frame while Sora is on and his world is up): his tracked shots followed; a walk
//            of every actor (sora_net_shots_core.h nshot_walk) every frame while he uses his bow, else every NSG_WALK_EVERY frames,
//            finds new arrows / Remote Bombs (and, netrunes 1, his Magnesis / Stasis object and Cryonis pillars); his weapon let
//            go of = the PR's check on his equipment links. The list goes to net_shots_local / net_runes_local.
//   draw     nsg_draw (end of avatar_tick): each friend drawn here (AVS[k].shown) gets his shots NET_INTERP_MS behind, with his
//            loopback shift: arrows / bombs as draw actors of data/gear/<actor>.bin (tools/sorabuild/netshots/shots_bake.py bakes
//            them at install from the user's own romfs; one model per kind and instance, netshotsmax per session, the draw
//            actor reserve kept free); his thrown weapon as netgear's model of it with netgear 1 (ngd_get: the same draw actor
//            his hand used), else one of ours (data/gear/<weapon>.bin, which netgear's install bake writes); a landed one is
//            hidden (a dropped item: loot's). A boom = KH2's last Fire hit + its sound at his bomb's place (placeholder: BotW's
//            own blast is a guest call). netrunes 1: his Cryonis pillars as draw actors of data/gear/Obj_IceMakerBlock.bin, from
//            the same pool and budget. nsg_off(k) from avatar_slot_off hides slot k's.
#pragma once
#include "sora_net_shots.h"

#define NSG_WALK_EVERY 6    // frames between walks while he does not use a bow (a new Remote Bomb / Stasis object found within 0.2 s at 30)
#define NSG_POOL 16         // shot models one session may know (loaded or refused)
#define NSG_STOP_M 60.0f    // (PR) his Stasis object within this, x / z
#define NSG_ICE_M 100.0f    // (PR) his Cryonis pillars within this, x / z
#define NSG_LINK_CARRIED 5  // (PR link_carried) weapon, shield, bow, the weapon's sheath, the bow's quiver
#define NSG_SLOW_US 3000    // walks slower than this on average (NSG.walk_avg): no walk every frame while bowing (every NSG_WALK_EVERY)
#define NSG_SLOWER_US 12000 // slower than this on average: every 30th frame only (a second at 30 fps). Back down under half of each;
                            // each change logged. (s289 lab: one walk of 3297 us, 360 us average, had stopped the bowing walks)
#define NSG_AVG_N 8         // the average: each walk moves it 1/NSG_AVG_N of the way
#define NSG_TRACE_FULL_MS 1000  // netshotstrace: "8 followed" said at most this often
#define NSG_TRACE_SAID 128      // netshotstrace: "not ours" said once per actor (the last this many), until it is taken

static bool nsg_rd(uint64_t a, size_t n) { return rd_ok(a, n); }

typedef struct {
    char name[NET_GEAR_NAME_MAX + 1];
    int inst, id, fails, owner;  // id 0 not loaded yet, > 0 the draw actor, -1 given up for the session
    uint64_t retry_at;
    bool said, shown, used;
} NsgModel;

static struct {
    bool init;
    NsLocal L;
    unsigned frame;
    uint64_t actors[NSHOT_WALK_MAX];
    uint64_t units[NSHOT_WALK_MAX];  // (s290 netshotsstuck: the unit each actor was found through, for nsl_found)
    uint64_t held;  // (PR) his weapon last frame (+0xb98) and its watch
    NsWatch hw;
    // runes (netrunes 1)
    uint64_t mag, stop;
    NsWatch mw, sw;
    uint32_t mag_hid, stop_hid;
    float ice[NET_RUNES_ICE][12];
    int nice;
    // drawing
    NsgModel m[NSG_POOL];
    int nm, total;
    bool tried, pool_full_said;
    int slow;  // (1 = walks slower than NSG_SLOW_US on average, 2 = slower than NSG_SLOWER_US)
    float walk_avg;
    uint64_t trace_full_at;
    struct { uint64_t a; uint32_t id; } tsaid[NSG_TRACE_SAID];
    int tsaid_next;
    void (*shadow)(int, float, float);
    int thr[NET_MAX_PEERS];  // netgear's draw actor showing slot k's thrown weapon now (0 = none)
    int fx_boom, se_boom;    // (-2 = not looked up)
    struct { uint32_t walks, walked_max, walk_us_max, taken, booms_played, drawn, nomodel; double walk_us; } st;
} NSG = {.fx_boom = -2, .se_boom = -2};

static void nsg_diag(void);
// (s292 coopfix) `set netshots` changed (net_shots_live_set): our tracks go (sora_net_shots_core.h nsl_restart), the next walk
// takes what is ours from where it is now; one line when any went
static void nsg_restart(void) {
    if (!NSG.init) return;
    uint16_t next = NSG.L.next_id;
    int n = nsl_restart(&NSG.L);
    NSG.held = 0, memset(&NSG.hw, 0, sizeof NSG.hw);  // (his let-go weapon is looked at again from scratch)
    if (n) LOG("[net] shots: netshots changed: our %d followed shot(s) dropped (stale while the key was 0); new ones go on from #%u\n", n, next);
}
static void nsg_init(uint64_t now) {
    if (NSG.init) return;
    NSG.init = true;
    nsl_init(&NSG.L, (uint16_t)(now * 2654435761u >> 13));
    g_nsg_diag = nsg_diag;
    g_nsg_restart = nsg_restart;
}

static const char *nsg_track_name(const NsTrack *t) { return t->kind == NSK_THROWN ? t->name : net_shots_kind_txt(t->kind); }
static bool nsg_matrix(uint64_t a, float m[12]) {
    if (!rd_ok(a + 0x398, 12 * sizeof(float))) return false;
    memcpy(m, (const void *)(uintptr_t)(a + 0x398), 12 * sizeof(float));
    for (int i = 0; i < 12; i++)
        if (!isfinite(m[i])) return false;
    return true;
}
static bool nsg_carries(const uint64_t c[NSG_LINK_CARRIED], uint64_t a) {
    for (int k = 0; a && k < NSG_LINK_CARRIED; k++)
        if (c[k] == a) return true;
    return false;
}

// ---- his weapon let go of (PR sample_pose): last frame's weapon, alive, within NSHOT_NEAR_M of him, no longer his gear ----
static void nsg_thrown(uint64_t pl, const float link[3], bool bowing, uint64_t now) {
    uint64_t c[NSG_LINK_CARRIED];
    for (int k = 0; k < 3; k++) c[k] = nshot_linked(nsg_rd, pl + 0xb98 + 0x18 * (uint64_t)k);
    c[3] = c[0] ? nshot_linked(nsg_rd, c[0] + 0x958) : 0, c[4] = c[2] ? nshot_linked(nsg_rd, c[2] + 0x958) : 0;
    float hm[12];
    if (NSG.held && c[0] != NSG.held && nshot_alive(nsg_rd, NSG.held, NSG.hw) && !nsg_carries(c, NSG.held) && nsg_matrix(NSG.held, hm)) {
        float dx = hm[3] - link[0], dy = hm[7] - link[1], dz = hm[11] - link[2];
        char nm[48];
        nshot_name(nsg_rd, NSG.held, nm);
        int l = (int)strlen(nm);
        if (dx * dx + dy * dy + dz * dz < NSHOT_NEAR_M * NSHOT_NEAR_M && !strncmp(nm, "Weapon_", 7) && l <= NET_GEAR_NAME_MAX && net_area_chars(nm, l)) {
            int r = nsl_offer(&NSG.L, now, NSG.held, NSG.hw, NSK_THROWN, hm, link, bowing, false, nm);
            if (r == NSO_TAKEN) NSG.st.taken++;
            if (g_net_shots_log && r != NSO_KNOWN) LOG("[net] shots: our %s left our hand: %s\n", nm, r == NSO_TAKEN ? "followed while it flies" : "not followed (8 shots already)");
        }
    }
    NSG.held = c[0];
    if (c[0]) nshot_watch(nsg_rd, c[0], &NSG.hw);
    for (int i = 0; i < NSHOT_MAX; i++)  // (his again: back in his hand / on his back)
        if (NSG.L.tr[i].actor && NSG.L.tr[i].kind == NSK_THROWN && nsg_carries(c, NSG.L.tr[i].actor)) {
            if (g_net_shots_log && NSG.L.tr[i].said) LOG("[net] shots: our %s #%u is ours again (caught / picked up)\n", NSG.L.tr[i].name, NSG.L.tr[i].id);
            NSG.L.st.his_again++;
            nsl_drop(&NSG.L, i, false);
        }
}

// netshotstrace: an arrow / bomb the walk offered, taken or why not ("not ours ..." once per actor until it is taken)
static void nsg_trace_offer(const char *nm, uint64_t a, NsWatch w, const float m[12], const float link[3], int r, uint64_t now) {
    const NsLocal *L = &NSG.L;
    if (!g_net_shots_trace || r == NSO_KNOWN || r == NSO_BAD || L->why >= NSW_N) return;
    int k = -1;
    for (int i = 0; i < NSG_TRACE_SAID && k < 0; i++)
        if (NSG.tsaid[i].a == a && NSG.tsaid[i].id == w.id) k = i;
    if (r == NSO_NOT_HIS) {
        if (k >= 0) return;
        NSG.tsaid[NSG.tsaid_next].a = a, NSG.tsaid[NSG.tsaid_next].id = w.id, NSG.tsaid_next = (NSG.tsaid_next + 1) % NSG_TRACE_SAID;
    } else if (k >= 0) NSG.tsaid[k].a = 0, NSG.tsaid[k].id = 0;
    if (r == NSO_FULL) {
        if (NSG.trace_full_at && now >= NSG.trace_full_at && now - NSG.trace_full_at < NSG_TRACE_FULL_MS) return;
        NSG.trace_full_at = now ? now : 1;
    }
    float dx = m[3] - link[0], dy = m[7] - link[1], dz = m[11] - link[2];
    char pn[48] = "";
    uint64_t par = nshot_parent(nsg_rd, a);
    if (par) nshot_name(nsg_rd, par, pn);
    char ev[24] = "";
    if (r == NSO_TAKEN && L->evicted_id) snprintf(ev, sizeof ev, " (in place of #%u)", L->evicted_id);
    LOG("[net] shots trace: new %s %#llx (id %u) %.2f m from our feet, parent %s: %s%s\n", nm, (unsigned long long)a, w.id,
        sqrtf(dx * dx + dy * dy + dz * dz), par ? (pn[0] ? pn : "(no name)") : "none", NSW_TXT[L->why], ev);
}

// ---- (s290 netshotsstuck) one of our arrows this look: its watch; failed: the same actor on another proc unit (nshot_rewatch);
// failed too while its +0x60 id is still there: lost (nsl_lose: kept at its last pose for NSHOT_LOST_MS, the walk finds it on its
// unit: nsl_found). -> true = done with it for this look (kept lost, or let go: gone, said here), false = *alive (+ m) for
// nsl_update (alive, or really gone: its id no longer there / being deleted)
static bool nsg_follow(NsTrack *t, int i, float m[12], uint64_t now, bool *alive) {
    uint64_t was = t->w.unit;
    bool ok = nshot_alive(nsg_rd, t->actor, t->w);
    if (!ok && nshot_rewatch(nsg_rd, t->actor, &t->w)) {
        ok = true, NSG.L.st.rewatched++;
        if (g_net_shots_log || g_net_shots_trace)
            LOG("[net] shots: our %s #%u is on another proc unit now (%#llx -> %#llx, id %u the same): followed on\n", nsg_track_name(t), t->id,
                (unsigned long long)was, (unsigned long long)t->w.unit, t->w.id);
    }
    *alive = ok && nsg_matrix(t->actor, m);
    if (*alive || !nshot_same(nsg_rd, t->actor, t->w.id)) return false;
    bool first = !t->lost_since, said = t->said;
    uint16_t id = t->id;
    uint32_t wid = t->w.id;
    uint64_t actor = t->actor;
    char nm[NET_GEAR_NAME_MAX + 1];
    snprintf(nm, sizeof nm, "%s", nsg_track_name(t));
    if (nsl_lose(&NSG.L, i, now)) {
        if (first && (g_net_shots_log || g_net_shots_trace))
            LOG("[net] shots: our %s #%u: its watch failed (unit %#llx), id %u still there: kept %d ms at its last place for the walk to find it\n", nm, id,
                (unsigned long long)was, wid, NSHOT_LOST_MS);
        return true;
    }
    if (g_net_shots_log && said) LOG("[net] shots: our %s #%u ended (gone: not found again in %d ms)\n", nm, id, NSHOT_LOST_MS);
    if (g_net_shots_trace) LOG("[net] shots trace: #%u %s %#llx ended: gone (lost, not found again) (alive 0)\n", id, nm, (unsigned long long)actor);
    return true;
}

// ---- one walk over every actor: new arrows / bombs; (netrunes 1) his Magnesis / Stasis object, his Cryonis pillars ----
static void nsg_walk(uint64_t pl, uint64_t bow, const float link[3], bool bowing, uint64_t now) {
    uint64_t unit = 0;
    if (!ns_u64(nsg_rd, pl + 0x68, &unit)) return;
    LARGE_INTEGER t0, t1, fq;
    QueryPerformanceCounter(&t0);
    int n = nshot_walk_u(nsg_rd, unit, NSG.actors, NSG.L.stuck_ms ? NSG.units : NULL, NSHOT_WALK_MAX), nice = 0;
    bool want_mag = g_net_runes && !NSG.mag, want_stop = g_net_runes && !NSG.stop;
    for (int i = 0; i < n; i++) {
        uint64_t a = NSG.actors[i];
        if (a == pl) continue;
        char nm[48];
        nshot_name(nsg_rd, a, nm);
        int kind = g_net_shots ? nshot_kind_of(nm) : -1;
        NsWatch w;
        float m[12];
        if (kind >= 0) {
            uint32_t aid = 0;  // (s290 netshotsstuck: one of ours that was lost, watched from now on the unit the walk found it through)
            int f = NSG.L.stuck_ms && nshot_kind_arrow(kind) && ns_u32(nsg_rd, a + 0x60, &aid) ? nsl_found(&NSG.L, a, aid, NSG.units[i]) : -1;
            if (f >= 0) {
                if (g_net_shots_log || g_net_shots_trace)
                    LOG("[net] shots: our %s #%u found again by the walk on unit %#llx (id %u): followed on\n", nm, NSG.L.tr[f].id, (unsigned long long)NSG.units[i], aid);
                continue;
            }
            if (!nshot_watch(nsg_rd, a, &w) || !nsg_matrix(a, m)) continue;
            bool held = nshot_kind_arrow(kind) && nsl_find(&NSG.L, a, w.id) < 0 && nshot_on_him(nsg_rd, a, pl, bow);
            int r = nsl_offer(&NSG.L, now, a, w, kind, m, link, bowing, held, NULL);
            if (r == NSO_TAKEN) NSG.st.taken++;
            if (g_net_shots_log && !g_net_shots_trace && r == NSO_FULL) LOG("[net] shots: %s not followed: 8 shots already\n", nm);
            nsg_trace_offer(nm, a, w, m, link, r, now);
            continue;
        }
        if (!g_net_runes) continue;
        if (!strcmp(nm, "Obj_IceMakerBlock")) {  // (PR: his pillars; the first NET_RUNES_ICE within NSG_ICE_M)
            if (nice < NET_RUNES_ICE && nsg_matrix(a, m) && (m[3] - link[0]) * (m[3] - link[0]) + (m[11] - link[2]) * (m[11] - link[2]) <= NSG_ICE_M * NSG_ICE_M)
                memcpy(NSG.ice[nice++], m, sizeof m);
            continue;
        }
        if (!want_mag && !want_stop) continue;
        uint32_t look = 0, hid = 0;  // (PR: +0x710 how it is drawn: 2 held by Magnesis, 8 stopped by Stasis; the cheap checks first)
        uint64_t mdl = 0;
        if (!ns_u32(nsg_rd, a + 0x710, &look) || !(look & ((want_mag ? 2u : 0u) | (want_stop ? 8u : 0u)))) continue;
        if (!ns_u64(nsg_rd, a + 0x4e0, &mdl) || !mdl || !nshot_has_body(nsg_rd, a) || !nsg_matrix(a, m) || !nshot_watch(nsg_rd, a, &w)) continue;
        if ((m[3] - link[0]) * (m[3] - link[0]) + (m[11] - link[2]) * (m[11] - link[2]) > NSG_STOP_M * NSG_STOP_M) continue;
        ns_u32(nsg_rd, a + 0x640, &hid);
        if (hid && !g_net_loop && netshots_friend_holds(hid)) continue;  // (one a friend holds, moved here by objs: not ours; loopback: slot 0 is us)
        if (want_mag && (look & 2)) {
            NSG.mag = a, NSG.mw = w, NSG.mag_hid = hid, want_mag = false;
            if (g_net_shots_log) LOG("[net] runes: our Magnesis holds %s (hashid %#x)\n", nm, hid);
        } else if (want_stop && (look & 8)) {
            NSG.stop = a, NSG.sw = w, NSG.stop_hid = hid, want_stop = false;
            if (g_net_shots_log) LOG("[net] runes: our Stasis stopped %s (hashid %#x)\n", nm, hid);
        }
    }
    if (g_net_runes) NSG.nice = nice;
    QueryPerformanceCounter(&t1), QueryPerformanceFrequency(&fq);
    uint32_t us = fq.QuadPart ? (uint32_t)((t1.QuadPart - t0.QuadPart) * 1000000 / fq.QuadPart) : 0;
    NSG.st.walks++, NSG.st.walk_us += us;
    if (us > NSG.st.walk_us_max) NSG.st.walk_us_max = us;
    NSG.walk_avg += ((float)us - NSG.walk_avg) / NSG_AVG_N;  // (from 0: one slow walk alone changes nothing, one of 24 ms or more does)
    int slow = nshot_walk_level(NSG.slow, NSG.walk_avg, NSG_SLOW_US, NSG_SLOWER_US);
    if (slow != NSG.slow) {  // (a safety net: the frame comes first; "net diag" says how long they take)
        NSG.slow = slow;
        LOG("[net] shots: walks over %d actors take %.0f us on average now (this one %u us): walking %s\n", n, NSG.walk_avg, us,
            slow == 2 ? "once a second" : slow ? "every 6th frame, also while bowing" : "every frame while bowing again");
    }
    if ((uint32_t)n > NSG.st.walked_max) NSG.st.walked_max = (uint32_t)n;
}

// ---- top of avatar_tick, every frame Sora is on ----
static void nsg_sample(uint64_t now) {
    if ((!g_net_shots && !g_net_runes) || !NET.on || !g_av_local_ok) return;  // (load / death / not driven: the list goes stale, the tick stops)
    nsg_init(now);
    uint64_t pl = link_actor();
    float pm[12];
    if (!pl || !nsg_matrix(pl, pm)) return;
    const float link[3] = {pm[3], pm[7], pm[11]};
    NSG.frame++;
    bool bowing = false;
    uint64_t bow = 0;
    if (g_net_shots) {
        bowing = nsl_bowing(&NSG.L, now, g_bow_on || nshot_bow_as(nsg_rd, pl));
        bow = nshot_linked(nsg_rd, pl + 0xbc8);  // (his bow: an arrow on it is his)
        nsg_thrown(pl, link, bowing, now);
        NSG.L.stuck_ms = g_net_shots_stuck > 0 ? (uint32_t)g_net_shots_stuck * 1000u : 0;  // (s290)
        for (int i = 0; i < NSHOT_MAX; i++) {  // (the ones followed: where they are now)
            NsTrack *t = &NSG.L.tr[i];
            if (!t->actor) continue;
            float m[12];
            bool alive;
            if (!NSG.L.stuck_ms || !nshot_kind_arrow(t->kind)) alive = nshot_alive(nsg_rd, t->actor, t->w) && nsg_matrix(t->actor, m);
            else if (nsg_follow(t, i, m, now, &alive)) continue;  // (s290 netshotsstuck: re-watched / lost / gone: nsg_follow)
            if (!alive) memcpy(m, t->m, sizeof m);
            bool ex = alive && nshot_kind_bomb(t->kind) && !t->boomed && !strcmp(nshot_as_name(nsg_rd, t->actor, 0, 0), "Explode");
            bool held = alive && nshot_kind_arrow(t->kind) && nshot_on_him(nsg_rd, t->actor, pl, bow);
            bool was = nsl_sent(t), said = t->said, arrow = nshot_kind_arrow(t->kind);
            uint16_t id = t->id;
            uint64_t actor = t->actor;
            char nm[NET_GEAR_NAME_MAX + 1];
            snprintf(nm, sizeof nm, "%s", nsg_track_name(t));
            if (!nsl_update(&NSG.L, i, now, alive, m, ex, link, bowing, held)) {
                if (g_net_shots_log && said && NSG.L.stuck_ms && arrow)  // (s290)
                    LOG("[net] shots: our %s #%u ended (%s, after %.1f s lying still)\n", nm, id,
                        NSG.L.end == NSE_CAP ? "still past netshotsstuck: the cap" : alive ? "lay still / not a shot" : "gone: the game removed it",
                        NSG.L.end_still_ms / 1000.0);
                else if (g_net_shots_log && said) LOG("[net] shots: our %s #%u ended (%s)\n", nm, id, alive ? "lay still / not a shot" : "gone");
                if (g_net_shots_trace && arrow)
                    LOG("[net] shots trace: #%u %s %#llx ended: %s (alive %d)\n", id, nm, (unsigned long long)actor, NSE_TXT[NSG.L.end < NSE_N ? NSG.L.end : 0], alive);
                continue;
            }
            if (g_net_shots_trace && arrow)
                LOG("[net] shots trace: #%u %s %#llx %.2f m from our feet, %.1f m/s, moved %d away %d still %u ms, on our bow %d (off it %u looks), flies %d, "
                    "alive 1\n", id, nm, (unsigned long long)actor, t->dist, t->speed, t->moved, t->away,
                    t->still_since && now >= t->still_since ? (unsigned)(now - t->still_since) : 0u, t->held, t->off, t->flew);
            if (!was && nsl_sent(t) && !t->said) {
                t->said = true;
                if (g_net_shots_log) LOG("[net] shots: our %s #%u %s at %.1f %.1f %.1f\n", nm, id, nshot_kind_bomb(t->kind) ? "is out" : "flies", m[3], m[7], m[11]);
            }
            if (NSG.L.stuck_ms && arrow && t->flew && !t->stuck_said && t->still_since && now - t->still_since >= NSHOT_STILL_HOLD_MS) {  // (s290)
                t->stuck_said = true;
                if (g_net_shots_log)
                    LOG("[net] shots: our %s #%u lies still at %.1f %.1f %.1f (stuck / landed): sent until the game removes it (at most %d s)\n", nm, id, m[3], m[7],
                        m[11], g_net_shots_stuck);
            }
        }
    }
    if (NSG.slow == 2 ? NSG.frame % 30 == 0 : (g_net_shots && bowing && !NSG.slow) || NSG.frame % NSG_WALK_EVERY == 0) nsg_walk(pl, bow, link, bowing, now);
    if (g_net_shots) {
        NetShot s[NSHOT_MAX];
        NetBoom b[NSHOT_BOOMS];
        bool moving = false;
        int n = nsl_list(&NSG.L, now, s, &moving), nb = nsl_booms(&NSG.L, now, b);
        net_shots_local(s, n, b, nb, moving, now);
    }
    if (g_net_runes) {
        NetRunes r;
        memset(&r, 0, sizeof r);
        uint32_t look = 0;
        float m[12];
        uint64_t *objs[2] = {&NSG.mag, &NSG.stop};
        NsWatch *ws[2] = {&NSG.mw, &NSG.sw};
        for (int f = 0; f < 2; f++) {  // (followed while alive and still drawn so: Magnesis 2 / Stasis 8 at +0x710)
            uint64_t a = *objs[f];
            if (!a) continue;
            if (!nshot_alive(nsg_rd, a, *ws[f]) || !ns_u32(nsg_rd, a + 0x710, &look) || !(look & (f ? 8u : 2u)) || !nsg_matrix(a, m)) {
                if (g_net_shots_log) LOG("[net] runes: our %s let go\n", f ? "Stasis" : "Magnesis");
                *objs[f] = 0;
                continue;
            }
            NetRuneObj *o = f ? &r.stop : &r.mag;
            o->hashid = f ? NSG.stop_hid : NSG.mag_hid, o->pos[0] = m[3], o->pos[1] = m[7], o->pos[2] = m[11];
            nshot_q_from_m(m, o->q);
            r.flags |= f ? NRF_STOP : NRF_MAG;
        }
        r.nice = (uint8_t)NSG.nice;
        for (int i = 0; i < NSG.nice; i++) {
            memcpy(r.ice[i].pos, (float[3]){NSG.ice[i][3], NSG.ice[i][7], NSG.ice[i][11]}, sizeof r.ice[i].pos);
            nshot_q_from_m(NSG.ice[i], r.ice[i].q);
        }
        net_runes_local(&r, now);
    }
}

// (for the loot helper) this actor of ours is followed as a shot and has not lain still NSHOT_STILL_HOLD_MS: an arrow or thrown
// weapon about to fly or in flight, a Remote Bomb out -> 1 (not a drop yet: the friends draw its flight); 0 = not one of our
// shots, or landed (a landed thrown weapon is hidden in the friends' games from then on: a drop to share). netshots 0: always 0.
static int netshots_local_flying(uint64_t actor) {
    if (!g_net_shots || !actor) return 0;
    uint64_t now = GetTickCount64();
    for (int i = 0; i < NSHOT_MAX; i++) {
        const NsTrack *t = &NSG.L.tr[i];
        if (t->actor != actor || t->boomed) continue;
        return !(t->still_since && now >= t->still_since && now - t->still_since >= NSHOT_STILL_HOLD_MS);
    }
    return 0;
}

// ---- drawing: a shot model (data/gear/<name>.bin), one draw actor per (name, instance) ----
static bool nsg_weapon_name(const char *n) { return !strncmp(n, "Weapon_", 7); }
static NsgModel *nsg_model(const char *name, int inst, int owner, uint64_t now) {
    if (!NSG.tried) NSG.tried = true, NSG.shadow = (void (*)(int, float, float))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_actor_shadow");
    NsgModel *e = NULL;
    for (int i = 0; i < NSG.nm && !e; i++)
        if (NSG.m[i].inst == inst && !strcmp(NSG.m[i].name, name)) e = &NSG.m[i];
    // (the exe tells actors apart by path: "/./" dots, at most 6; a weapon's start past netgear's slot copies 1..NET_MAX_PEERS)
    int dots = nsg_weapon_name(name) ? NET_MAX_PEERS + 1 + inst : inst;
    if (!e) {
        if (NSG.nm >= NSG_POOL || dots > 6) {
            if (!NSG.pool_full_said) NSG.pool_full_said = true, LOG("[net] shots: %s #%d not drawn: %d shot models known this session (or no path left for it)\n", name, inst + 1, NSG.nm);
            return NULL;
        }
        e = &NSG.m[NSG.nm++];
        memset(e, 0, sizeof *e);
        snprintf(e->name, sizeof e->name, "%s", name);
        e->inst = inst;
    }
    if (e->id) return e->id > 0 ? e : NULL;
    if (e->retry_at && now < e->retry_at) return NULL;
    char mp[300], pp[300], dm[600], dp[600];
    snprintf(mp, sizeof mp, "%s%s.bin", sora_path(NGD_DIR), name);
    snprintf(pp, sizeof pp, "%s", sora_path(NGD_POSE));
    if (GetFileAttributesA(mp) == INVALID_FILE_ATTRIBUTES || GetFileAttributesA(pp) == INVALID_FILE_ATTRIBUTES) {
        e->retry_at = now + NGD_RETRY_MS;
        if (!e->said) e->said = true, LOG("[net] shots: %s or %s missing (the install bakes them: shots_bake.py): %s not drawn (looked for again every %d s)\n", mp, pp, name, NGD_RETRY_MS / 1000);
        return NULL;
    }
    int cap = AV.cap ? AV.cap() : 0, used = av_used(), res = g_of_reserve > AV_RESERVE ? g_of_reserve : AV_RESERVE;
    if (!cap || NSG.total >= g_net_shots_max || cap - used <= res) {
        e->retry_at = now + NGD_RETRY_MS;
        if (!e->said) e->said = true, LOG("[net] shots: %s #%d not drawn: %d shot model(s) loaded (netshotsmax %d), %d of %d draw actors in use (%d must stay free)\n", name, inst + 1,
                                         NSG.total, g_net_shots_max, used, cap, res);
        return NULL;
    }
    av_dot_pathn(mp, dm, sizeof dm, dots), av_dot_pathn(pp, dp, sizeof dp, dots);
    int a = AV.load(dm, dp);
    if (a <= 0) {
        e->retry_at = now + NGD_RETRY_MS;
        if (++e->fails >= 3) e->id = -1;
        LOG("[net] shots: %s: load failed (%s)%s\n", name, dm, e->id < 0 ? ", given up for this session" : "");
        return NULL;
    }
    NSG.total++;
    if (AV.bones && AV.bones(a) != 1) {
        if (AV.off) AV.off(a);
        e->id = -1;
        LOG("[net] shots: %s has %d bones, not the one-bone model shots_bake writes: given up for this session\n", dm, AV.bones(a));
        return NULL;
    }
    e->id = a, e->owner = owner;
    if (NSG.shadow) NSG.shadow(a, 0, 0);  // (no blob disc)
    LOG("[net] shots: %s #%d loaded: actor %d (%d shot model(s), %d of %d draw actors in use)\n", name, inst + 1, a, NSG.total, av_used(), cap);
    return e;
}

static void nsg_boom(int k, const NetBoom *b, const float shift[2]) {
    if (!g_net_shots_boom) return;
    if (NSG.fx_boom == -2) NSG.fx_boom = sora_fx_id("fire_hit_last"), NSG.se_boom = sora_sfx_id("fire_hit");
    float p[3] = {b->pos[0] + shift[0], b->pos[1], b->pos[2] + shift[1]};
    if (NSG.fx_boom >= 0) {
        net_rfx_room();  // (his copies hold at most NET_RFX_MAX fx slots, as his other fx)
        sora_fx_spawn(NSG.fx_boom, p, 0, 0.01f * g_k * (float)g_net_shots_boom / 100.0f);
    }
    if (NSG.se_boom >= 0 && AV.sfx_at) AV.sfx_at(NSG.se_boom, 1.0f, p);  // (the exe's own: not sent back out)
    NSG.st.booms_played++;
    if (g_net_shots_log) LOG("[net] shots: peer %d's %s went off here at %.1f %.1f %.1f (boom #%u)\n", k + 1, net_shots_kind_txt(b->kind), p[0], p[1], p[2], b->seq);
}

static void nsg_off(int k) {
    for (int i = 0; i < NSG.nm; i++)
        if (NSG.m[i].shown && NSG.m[i].owner == k && NSG.m[i].id > 0) {
            if (AV.off) AV.off(NSG.m[i].id);
            NSG.m[i].shown = false;
        }
    if (k >= 0 && k < NET_MAX_PEERS && NSG.thr[k]) {
        if (NGD.s[k].shown != NSG.thr[k] && AV.off) AV.off(NSG.thr[k]);  // (netgear shows it in his hand again: his)
        NSG.thr[k] = 0;
    }
}

// ---- end of avatar_tick: the friends' shots drawn (netshots 1), and their Cryonis pillars (netrunes 1) ----
#define NSG_ICE_NAME "Obj_IceMakerBlock"
static void nsg_draw(uint64_t now) {
    bool shots = g_net_shots, ice = g_net_runes;
    if ((!shots && !ice) || !AV.world || !AV.off || !AV.load) {
        for (int k = 0; k < NET_MAX_PEERS; k++) nsg_off(k);
        return;
    }
    uint64_t at = now > NET_INTERP_MS ? now - NET_INTERP_MS : 0;
    struct { int k, kind; bool pooled, still; const char *name; float p[3], q[4], d2; } vis[NET_MAX_PEERS * (NST_TRACKS + NET_RUNES_ICE)];
    int nv = 0;
    bool thr_used[NET_MAX_PEERS] = {0};
    for (int k = 0; k < NET_MAX_PEERS; k++) {
        if (!net_shots_rec_ok(k)) continue;
        NsPeer *P = &NETS.peer[k].p;
        float sh[2] = {g_av_shift[k][0], g_av_shift[k][1]};
        if (shots) {
            net_shots_reap(k, now, at);  // (s290: nsp_reap; with netshotsstuck also the cap)
            NetBoom b;
            while (nsp_boom_due(P, at, &b))
                if (AVS[k].shown) nsg_boom(k, &b, sh);  // (his Sora not drawn here: on another map / far: no boom either)
        }
        if (!AVS[k].shown) continue;
        for (int i = 0; shots && i < NST_TRACKS; i++) {
            const NstTrack *t = &P->tr[i];
            if (!t->used || (t->kind == NSK_THROWN && (t->flags & NSF_STILL))) continue;  // (a weapon that landed: a dropped item, loot's)
            if (!nsp_sample(t, at, vis[nv].p, vis[nv].q)) continue;
            vis[nv].still = g_net_shots_stuck && nshot_kind_arrow(t->kind) && (t->flags & NSF_STILL);  // (s290: when models run short, a flying one first)
            vis[nv].k = k, vis[nv].kind = t->kind, vis[nv].name = t->name, nv++;
        }
        const NetRunes *r = ice ? net_runes_of(k, now) : NULL;
        for (int i = 0; r && i < r->nice && i < NET_RUNES_ICE; i++) {  // (his pillars: they stand still, his newest places as they are)
            memcpy(vis[nv].p, r->ice[i].pos, sizeof vis[nv].p), memcpy(vis[nv].q, r->ice[i].q, sizeof vis[nv].q);
            vis[nv].still = false, vis[nv].k = k, vis[nv].kind = NSK_N, vis[nv].name = NSG_ICE_NAME, nv++;
        }
        for (int i = nv; i-- > 0 && vis[i].k == k;) {
            vis[i].p[0] += sh[0], vis[i].p[2] += sh[1], vis[i].pooled = false;
            float dx = vis[i].p[0] - g_body[0], dy = vis[i].p[1] - g_body[1], dz = vis[i].p[2] - g_body[2];
            vis[i].d2 = dx * dx + dy * dy + dz * dz;
        }
    }
    for (int i = 1; i < nv; i++)  // (nearest first: when models run short, the far ones go)
        for (int j = i; j > 0 && (vis[j].still < vis[j - 1].still || (vis[j].still == vis[j - 1].still && vis[j].d2 < vis[j - 1].d2)); j--) {
            __typeof__(vis[0]) tmp = vis[j];
            vis[j] = vis[j - 1], vis[j - 1] = tmp;
        }
    for (int i = 0; i < NSG.nm; i++) NSG.m[i].used = false;
    for (int i = 0; i < nv; i++) {
        int k = vis[i].k;
        float rows[12];
        nshot_rows(vis[i].q, rows);
        if (vis[i].kind == NSK_THROWN) {  // (his weapon: netgear's model of it when netgear is on and draws that one; his hand showing it
                                          // wins. netgear 0: netgear's list and budget are not touched, our own copy below)
            const WpnDef *def = g_net_gear ? wpn_find(vis[i].name) : NULL;
            NgdAct *g = def && !def->keyblade && !thr_used[k] ? ngd_get(k, vis[i].name, now) : NULL;
            if (g && g->id > 0) {
                if (NGD.s[k].shown == g->id) continue;
                if (NSG.thr[k] && NSG.thr[k] != g->id && NGD.s[k].shown != NSG.thr[k]) AV.off(NSG.thr[k]);
                AV.world(g->id, rows, vis[i].p, 0, 0.01f);
                NSG.thr[k] = g->id, thr_used[k] = true, NSG.st.drawn++;
                continue;
            }
        }
        int inst = 0;  // (the how-manyth of this model drawn from our pool this frame: each needs its own draw actor)
        for (int j = 0; j < i; j++)
            if (vis[j].pooled && !strcmp(vis[j].name, vis[i].name)) inst++;
        NsgModel *e = nsg_model(vis[i].name, inst, k, now);
        if (!e) { NSG.st.nomodel++; continue; }
        AV.world(e->id, rows, vis[i].p, 0, 0.01f);
        e->used = true, e->shown = true, e->owner = k, vis[i].pooled = true, NSG.st.drawn++;
    }
    for (int i = 0; i < NSG.nm; i++)
        if (NSG.m[i].shown && !NSG.m[i].used && NSG.m[i].id > 0) AV.off(NSG.m[i].id), NSG.m[i].shown = false;
    for (int k = 0; k < NET_MAX_PEERS; k++)
        if (NSG.thr[k] && !thr_used[k]) {
            if (NGD.s[k].shown != NSG.thr[k]) AV.off(NSG.thr[k]);
            NSG.thr[k] = 0;
        }
}

static void nsg_diag(void) {
    const NsLocal *L = &NSG.L;
    int followed = 0;
    for (int i = 0; i < NSHOT_MAX; i++) followed += L->tr[i].actor != 0;
    LOG("[net] shots (ours): followed now %d; taken %u (on our bow %u), arrows flew %u, not his %u, full %u, evicted %u, gone %u, lay still %u, never moved %u, "
        "on an unused bow %u, booms %u, bombs put away %u, weapons ours again %u; walks %u (up to %u actors, %.0f us average, %u us most, %.0f us lately: "
        "walking %s)\n",
        followed, L->st.tracked, L->st.on_bow, L->st.flew, L->st.not_his, L->st.full, L->st.evicted, L->st.gone, L->st.still, L->st.unmoved, L->st.idle,
        L->st.booms, L->st.put_away, L->st.his_again, NSG.st.walks, NSG.st.walked_max, NSG.st.walks ? NSG.st.walk_us / NSG.st.walks : 0.0, NSG.st.walk_us_max,
        NSG.walk_avg, NSG.slow == 2 ? "once a second" : NSG.slow ? "every 6th frame" : "every frame while bowing");
    if (g_net_shots_stuck)  // (s290)
        LOG("[net] shots (ours, netshotsstuck %d s): on another proc unit %u, lost %u (found again by the walk %u), lay still until the game removed them %u, "
            "still past the cap %u\n", g_net_shots_stuck, L->st.rewatched, L->st.lost, L->st.found, L->st.stuck_gone, L->st.capped);
    LOG("[net] shots (theirs): drawn %u (draw frames), no model %u, booms played %u; %d model(s) loaded (netshotsmax %d), netshotsboom %d%%\n", NSG.st.drawn, NSG.st.nomodel,
        NSG.st.booms_played, NSG.total, g_net_shots_max, g_net_shots_boom);
    for (int i = 0; i < NSG.nm; i++)
        LOG("[net]   %s #%d: %s\n", NSG.m[i].name, NSG.m[i].inst + 1,
            NSG.m[i].id > 0 ? (NSG.m[i].shown ? "shown" : "loaded") : NSG.m[i].id < 0 ? "given up" : "not loaded (missing bake / no free actor: tried again)");
}

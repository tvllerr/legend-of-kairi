// sora_net_movers.h (s289 objs part 3): netmovers, the level's mechanisms (lifts, elevators, doors, shutters, gears, bridges,
// turning / sliding floors, propellers, gates, a Divine Beast's parts...) move the same in every world of a Melty room.
// After Savage Wind PR #294 "Online mode" (byluki, fixes by mstampfli; MIT) movers_update, re-written for our lanes and keys.
// Included by sora_net_objs.h (NOB, the reads, the guest calls); the pure parts: sora_net_objs_core.h nm_* (host-tested).
//   ours  the actors are looked through every 500 ms (the objects' walk, no_actors_scan): a mechanism (nm_kind) with a map
//         HashId within 80 m is watched (24 at most; PR 48). Sampled at 20 Hz: one that moved (1 cm / 0.002) or changed its
//         AS (slot 0) by itself is ours to send while it moves and 1.5 s after it stops (the nearest 6: kind-2 pose records
//         with its AS name).
//   his   a record of one we watch: his, unless ours moves now and our player id is lower (PR). His moves are shown 120 ms
//         behind (as his Sora): woken (then watched on its new proc unit, PR alive_rewatch), taken there through its body
//         (objs_body_to) and put there (objs_set_mtx), his AS played (objs_play_as, when it differs, at most every 250 ms);
//         held at his last place 1.5 s after his last record, then ours again 0.7 s later.
//   our Link at it = within 6 m in x z of its place (or of where his record takes it), from 2 m under to 6 m over it (GUESS
//         sizes: a mechanism's place is its actor's origin, a big one's platform may reach farther); a Divine Beast and its
//         parts (Remains*: Link walks inside them) within 220 m x z and 150 m under / over:
//         netmoverride 0 (default): his moves of it are NOT shown while our Link is there (netmoverslog: one line); it goes
//           his way once Link has left. The case that can hurt (a lift under / over Link put somewhere else at once) stays out.
//         netmoverride 1 (the lab's first test, LAB_STEPS E1): it goes his way through its body only, at most 5 cm and 0.03 rad
//           a frame (3 m/s), never put there; his AS played.
//   our Link near it (within 30 m of its place or his, not at it: a long bridge / a wide floor Link may still stand on): his
//         moves of it never jump, they step at most 0.2 m and 0.05 rad a frame (12 m/s; a lift on its way moves a few cm a
//         frame: only a first catch-up or a gap in his records is slowed). Farther: at his place at once (PR).
// Needs the exe rows objs_body_to / objs_set_mtx / objs_wake (his AS: objs_as_in_slot / objs_play_as too; an exe relink with
// the s289 sora_sites.h; 1.9.0 only). Key 0 = nothing read, sent or moved.
//   his stream over (s290 labfix, sora_net_movers_core.h): it goes back to the place OUR game had it at when his stream began
//         (our game's own logic left it there: a shrine's entrance elevator at the bottom) and its body is stopped there, then
//         ours again (lab s290 E3: before, our elevator rose by itself once the friend's leave had been shown); a shrine's
//         entrance elevator / shutter (each player's own enter / leave scene) is not watched at all (lab key netmoverelev 1: it is).
#pragma once
#include "sora_net_movers_core.h"

#define NM_RESCAN_MS 250   // one of his we do not watch yet: the actors looked through again this soon (PR: at once)
#define NM_AS_TRY_MS 250   // his AS tried on ours at most this often

typedef struct {
    uint32_t h; uint64_t actor, unit; uint32_t id;
    char name[40];
    float last[12]; char as[NET_OBJS_AS]; bool sampled;  // our last 20 Hz sample
    NmOwn o;
    NmRing ring; char his_as[NET_OBJS_AS]; uint64_t his_at, as_try; uint8_t his_player;  // his records
    bool said_ours, said_his, said_link, said_as, said_near;
    NmBack back; char own_as[NET_OBJS_AS];  // (s290 labfix) our place + AS of it before his stream: where it goes back to
    // (s292 coopfix, netmoverstrace) after a let-go: where it goes by itself, NM_TRACE_MS (reads only)
    uint64_t tr_start, tr_next, tr_prev_at; float tr_from[3], tr_prev[3], tr_vmin, tr_vmax; const char *tr_why; bool tr_woke_said;
} NmTrk;
static int g_net_mover_elev;  // (s290 labfix) live "set netmoverelev 1" (lab): a shrine's entrance elevator / shutter watched again
static int g_net_mover_trace;  // (s292 coopfix) live "set netmoverstrace 1" (lab): a mover let go is traced 10 s (s291a (d): the elevator sank)
#define NM_TRACE_MS 10000      // (s292) traced this long after a let-go
#define NM_TRACE_EVERY_MS 250  // ... one line this often
static struct {
    NmTrk t[NM_TRACK];
    uint64_t rescan_at;
    struct { uint32_t tx_rec, rx_rec, rx_ours, rx_unknown, driven, link_skip, ride_steps, near_steps, near_held, as_played, as_failed, full, rewatched,
             back, back_done, back_stop, back_wait, back_own, scene_skip; } st;  // (s290 labfix: the way back, the scene-driven ones left out)
} NMV;

static NmTrk *nm_trk_of(uint32_t h) {
    for (int i = 0; h && i < NM_TRACK; i++)
        if (NMV.t[i].h == h) return &NMV.t[i];
    return NULL;
}
// alive, or the same actor gone to another proc unit (woken: by us, or by itself as a player comes near - a shrine's elevator;
// PR alive_rewatch): its id the same, not being deleted, its new unit holding it; then watched there
static bool nm_alive(NmTrk *t) {
    if (no_alive(t->actor, t->unit, t->id)) return true;
    if (!t->actor || no_rd32(t->actor + 0x60) != t->id || no_rd8(t->actor + 0x64) == 3) return false;
    uint64_t u = no_rd64(t->actor + 0x68);
    if (!NO_VALID(u) || no_rd64(u + 0x48) != t->actor) return false;
    t->unit = u, NMV.st.rewatched++;
    return true;
}

// the actors' walk (no_actors_scan) found a mechanism near us: watched from now on (its state now = its "last", quiet: PR)
static void nm_scan_see(uint64_t a, const char *name, const float L[3]) {
    uint32_t h = no_hid(a);
    float m[12];
    if (!h || !no_mtx(a, m)) return;
    float p[3] = {m[3], m[7], m[11]};
    if (!(no_d3(L, p) < NM_NEAR)) return;
    NmTrk *t = nm_trk_of(h);
    if (nm_scene_driven(name) && !g_net_mover_elev) {  // (s290 labfix: each player's own enter / leave scene; one his / on its way back ends first)
        if (t && !t->his_at && !t->back.on) memset(t, 0, sizeof *t);
        if (!t) NMV.st.scene_skip++;
        return;
    }
    if (t) { t->actor = a, t->unit = no_rd64(a + 0x68), t->id = no_rd32(a + 0x60); return; }  // (the game may make it again)
    for (int i = 0; i < NM_TRACK && !t; i++)
        if (!NMV.t[i].h) t = &NMV.t[i];
    if (!t) { NMV.st.full++; return; }
    memset(t, 0, sizeof *t);
    t->h = h, t->actor = a, t->unit = no_rd64(a + 0x68), t->id = no_rd32(a + 0x60);
    snprintf(t->name, sizeof t->name, "%s", name);
    memcpy(t->last, m, sizeof t->last), t->sampled = true;
    no_playing(a, t->as);
    t->o.quiet = NM_QUIET;
    if (g_net_movers_log > 1) LOG("[net] movers: watching %s %#x at %.1f %.1f %.1f (AS %s)\n", name, h, p[0], p[1], p[2], t->as[0] ? t->as : "-");
}
// after a walk: the ones gone (deleted by the game) or out of reach (and not his now) are forgotten
static void nm_scan_end(const float L[3]) {
    for (int i = 0; i < NM_TRACK; i++) {
        NmTrk *t = &NMV.t[i];
        if (!t->h) continue;
        float m[12];
        bool alive = nm_alive(t), away = alive && no_mtx(t->actor, m) && !(no_d3(L, (const float[3]){m[3], m[7], m[11]}) < NM_NEAR + 20);
        if (!alive || (away && !t->his_at && !t->back.on)) {  // (not `far`: a Windows macro)
            if (g_net_movers_log > 1) LOG("[net] movers: %s %#x %s: no longer watched\n", t->name, t->h, alive ? "out of reach" : "gone");
            memset(t, 0, sizeof *t);
        }
    }
}

// 20 Hz: ours that moved / changed its AS by itself lately go out (the nearest NM_SEND) as kind-2 pose records, when anyone
// is in the room to get them (the samples go on without anyone: whose it is stays right)
static void nm_sample(NetObjRec *r, int *n, int *pk, const float L[3], bool anyone, uint64_t now) {
    struct { int i; float d; } send[NM_TRACK];
    int ns = 0;
    for (int i = 0; i < NM_TRACK; i++) {
        NmTrk *t = &NMV.t[i];
        float m[12];
        if (!t->h || !nm_alive(t) || !no_mtx(t->actor, m)) continue;
        char as[NET_OBJS_AS];
        no_playing(t->actor, as);
        bool changed = !t->sampled || nm_moved(m, t->last) || strcmp(as, t->as);
        memcpy(t->last, m, sizeof t->last), memcpy(t->as, as, sizeof t->as), t->sampled = true;
        bool ours = nm_ours(&t->o, now, changed);
        if (g_net_movers_log && ours != t->said_ours) {
            if (ours) LOG("[net] movers: %s %#x moves here (AS %s) at %.1f %.1f %.1f: ours, sent\n", t->name, t->h, as[0] ? as : "-", m[3], m[7], m[11]);
            else if (!t->his_at) LOG("[net] movers: %s %#x still here 1.5 s (AS %s): ours no more\n", t->name, t->h, as[0] ? as : "-");
        }
        t->said_ours = ours;
        if (!ours || !anyone) continue;
        float p[3] = {m[3], m[7], m[11]}, d = no_d3(L, p);
        int at = ns++;
        while (at > 0 && send[at - 1].d > d) send[at] = send[at - 1], at--;
        send[at].i = i, send[at].d = d;
    }
    for (int j = 0; j < ns && j < NM_SEND; j++) {
        const NmTrk *t = &NMV.t[send[j].i];
        NetObjRec *o = &r[*n];
        memset(o, 0, sizeof *o);
        o->kind = NOR_MOVER, o->h = t->h;
        memcpy(o->m, t->last, sizeof o->m), memcpy(o->as, t->as, sizeof o->as);
        NMV.st.tx_rec++;
        if (++*n == NET_OBJS_N) no_flush(r, n, pk);
    }
}

// a kind-2 record of his (net_objs_recv)
static void nm_recv(int k, const NetObjRec *r, uint64_t now) {
    NMV.st.rx_rec++;
    NmTrk *t = nm_trk_of(r->h);
    if (!t) {  // (not watched here: not loaded / farther than 80 m / new; the walk soon, PR)
        NMV.st.rx_unknown++;
        if (now >= NMV.rescan_at) NMV.rescan_at = now + NM_RESCAN_MS, NOB.act_at = now;
        return;
    }
    uint8_t his = NET.slot[k].p.player;
    if (!nm_take(&t->o, now, NET.me, his)) { NMV.st.rx_ours++; return; }  // (ours moves now and our id is lower: ours)
    if (t->his_player != his) t->ring.n = 0;  // (another friend's: his own stream)
    if (!t->his_at) {  // (s290 labfix: his stream begins: our place of it kept, it goes back there when his stream is over)
        float cur[12] = {0};
        bool ok = nm_alive(t) && no_mtx(t->actor, cur);
        if (t->back.on && g_net_movers_log) LOG("[net] movers: %s %#x: %s's again on its way back: his shown, our place kept\n", t->name, t->h, no_who(his));
        bool fresh = !t->back.have;
        nm_back_his(&t->back, cur, ok);
        if (fresh && ok) no_playing(t->actor, t->own_as);
        else if (fresh) t->own_as[0] = 0;
        t->tr_woke_said = false;  // (s292 netmoverstrace: our wake said once per stream of his)
        if (g_net_mover_trace)
            LOG("[net] movers trace: %s %#x: %s's stream begins; ours at %.2f %.2f %.2f, state %u (1 awake), AS %s%s\n", t->name, t->h, no_who(his),
                cur[3], cur[7], cur[11], ok ? (unsigned)no_rd8(t->actor + 0x64) : 0u, t->own_as[0] ? t->own_as : "-", fresh ? "" : " (on its way back: our place kept)");
    }
    t->his_player = his, t->his_at = now ? now : 1;
    nm_ring_push(&t->ring, now, r->m);
    memcpy(t->his_as, r->as, sizeof t->his_as);
    if (g_net_movers_log && !t->said_his)
        LOG("[net] movers: %s moves %s %#x (AS %s): ours shows his\n", no_who(his), t->name, t->h, r->as[0] ? r->as : "-");
    t->said_his = true;
}

// ---- (s292 coopfix) netmoverstrace (lab, reads only): what a mover does by itself after we let it go. s291a (d): the shrine's
// entrance elevator, back at our place and stopped there ("ours again"), sank at once and went on (8.4 -> -24.5 m); s290 E3 saw
// it RISE after a friend's leave was shown. Which one it is decides the fix (GUESSES, none proven): a velocity left in a
// keyframed body (objs_body_to sets velocities, 0x71012d4e18: about the same speed all the way), the body turned dynamic by our
// wake (gravity: faster and faster, then stopped by a floor), or the elevator's own logic, woken, running a move of its own (a
// steady speed that stops at its other end, its AS changing). One line per NM_TRACE_EVERY_MS: place, metres from the let-go,
// speed and up / down speed since the line before, its state byte (+0x64: 1 awake; 3 being deleted), AS, ours (moving by
// itself) / his, our Link's distance; one line at the end (fastest / slowest).
static void nm_trace_start(NmTrk *t, const char *why, const float L[3], uint64_t now) {
    float m[12];
    if (!g_net_mover_trace || !no_mtx(t->actor, m)) return;
    t->tr_start = now ? now : 1, t->tr_next = now + NM_TRACE_EVERY_MS, t->tr_prev_at = now, t->tr_why = why;
    t->tr_from[0] = t->tr_prev[0] = m[3], t->tr_from[1] = t->tr_prev[1] = m[7], t->tr_from[2] = t->tr_prev[2] = m[11];
    t->tr_vmin = 1e30f, t->tr_vmax = 0;
    char as[NET_OBJS_AS];
    LOG("[net] movers trace: %s %#x let go (%s) at %.2f %.2f %.2f, state %u, AS %s, our place of it %.2f %.2f %.2f, our Link %.1f m: traced %d s\n",
        t->name, t->h, why, m[3], m[7], m[11], (unsigned)no_rd8(t->actor + 0x64), no_playing(t->actor, as)[0] ? as : "-", t->back.own[3], t->back.own[7],
        t->back.own[11], L ? no_d3(L, (const float[3]){m[3], m[7], m[11]}) : -1.0f, NM_TRACE_MS / 1000);
}
static void nm_trace_frame(NmTrk *t, const float L[3], uint64_t now) {
    if (!t->tr_start) return;
    if (!g_net_mover_trace) { t->tr_start = 0; return; }
    float m[12];
    bool end = now - t->tr_start >= NM_TRACE_MS;
    if (now < t->tr_next && !end) return;
    t->tr_next = now + NM_TRACE_EVERY_MS;
    if (!nm_alive(t) || !no_mtx(t->actor, m)) {
        LOG("[net] movers trace: %s %#x +%.2f s: gone / no matrix: trace over\n", t->name, t->h, (double)(now - t->tr_start) / 1000.0);
        t->tr_start = 0;
        return;
    }
    float p[3] = {m[3], m[7], m[11]}, dt = now > t->tr_prev_at ? (float)(now - t->tr_prev_at) / 1000.0f : 0;
    float v = dt > 0 ? no_d3(p, t->tr_prev) / dt : 0, vy = dt > 0 ? (p[1] - t->tr_prev[1]) / dt : 0;
    if (dt > 0 && v < t->tr_vmin) t->tr_vmin = v;
    if (v > t->tr_vmax) t->tr_vmax = v;
    char as[NET_OBJS_AS];
    LOG("[net] movers trace: %s %#x +%.2f s: at %.2f %.2f %.2f, %.2f m from the let-go (dy %+.2f), %.2f m/s (up %+.2f m/s), state %u, AS %s, %s, our Link %.1f m\n",
        t->name, t->h, (double)(now - t->tr_start) / 1000.0, p[0], p[1], p[2], no_d3(p, t->tr_from), p[1] - t->tr_from[1], v, vy,
        (unsigned)no_rd8(t->actor + 0x64), no_playing(t->actor, as)[0] ? as : "-",
        t->his_at ? "his: shown" : t->back.on ? "on its way back" : t->said_ours ? "ours: moves by itself" : "still", L ? no_d3(L, p) : -1.0f);
    memcpy(t->tr_prev, p, sizeof t->tr_prev), t->tr_prev_at = now;
    if (!end) return;
    LOG("[net] movers trace: %s %#x over after %d s (%s): %.2f m from the let-go (dy %+.2f), fastest %.2f m/s, slowest %.2f m/s\n", t->name, t->h,
        NM_TRACE_MS / 1000, t->tr_why ? t->tr_why : "-", no_d3(p, t->tr_from), p[1] - t->tr_from[1], t->tr_vmax, t->tr_vmin < 1e29f ? t->tr_vmin : 0.0f);
    t->tr_start = 0;
}
// (s292) netmoverstrace: our wake of one (asleep before: its state byte), once per stream of his
static void nm_trace_wake(NmTrk *t, unsigned was) {
    if (!g_net_mover_trace || t->tr_woke_said) return;
    t->tr_woke_said = true;
    LOG("[net] movers trace: %s %#x was asleep (state %u): woken by us (objs_wake), state now %u\n", t->name, t->h, was, (unsigned)no_rd8(t->actor + 0x64));
}

// (s290 labfix) its body stopped where it is: objs_body_to to the place it is at = velocity 0 (the one his moves left)
static void nm_stop_body(NmTrk *t, const float cur[12]) {
    if (no_has_body(t->actor)) no_call(NOB.body_to, no_rd64(t->actor + 0x190), cur);
}
// (s290 labfix) one frame of the way back to our own place of it (sora_net_movers_core.h nm_back_step), the calls his moves use
static void nm_back_frame(NmTrk *t, const float L[3], uint64_t now) {
    float cur[12], out[12];
    if (!nm_alive(t)) { memset(&t->back, 0, sizeof t->back); return; }  // (gone: nothing to put back)
    if (!no_mtx(t->actor, cur)) {
        if (now - t->back.since > NM_BACK_MS) memset(&t->back, 0, sizeof t->back);
        return;
    }
    t->o.theirs_ms = now;  // (its way back is not ours to send: a friend would show it)
    bool big = nm_big(t->name), at_cur = L && nm_link_at(L, cur, big), at_own = L && nm_link_at(L, t->back.own, big);
    bool nearby = !L || nm_link_near(L, cur, t->back.own);
    bool waited = t->back.waiting;
    int w = nm_back_step(&t->back, cur, at_cur, at_own, nearby, now, out);
    if (w == NMB_NONE) return;
    if (waited && !t->back.waiting && g_net_movers_log)  // (s292 coopfix: the end of a wait said too, s291a (d) had none)
        LOG("[net] movers: %s %#x: our Link left where it goes back to: on its way back again from %.1f %.1f %.1f (%.1f m off)\n", t->name, t->h,
            cur[3], cur[7], cur[11], nm_back_off(cur, t->back.own));
    if (w == NMB_OWN) {  // (its own logic moves it away from our place: let go, nothing written)
        NMV.st.back_own++;
        if (g_net_movers_log) LOG("[net] movers: %s %#x: its own logic moves it away from our place (%.1f m off at %.1f %.1f %.1f): let go, ours again\n",
                                  t->name, t->h, nm_back_off(cur, t->back.own), cur[3], cur[7], cur[11]);
        nm_trace_start(t, "its own logic moved it away", L, now);
        return;
    }
    if (w == NMB_DONE) {
        NMV.st.back_done++;
        if (g_net_movers_log) LOG("[net] movers: %s %#x back at our place of it (%.1f %.1f %.1f), stopped there: ours again (our game's own logic)\n",
                                  t->name, t->h, cur[3], cur[7], cur[11]);
        nm_trace_start(t, "back at our place, stopped", L, now);
        return;
    }
    if (no_rd8(t->actor + 0x64) != 1) {  // (asleep: its body takes nothing; as his moves)
        unsigned was = no_rd8(t->actor + 0x64);
        no_call(NOB.wake, t->actor, NULL);
        t->unit = no_rd64(t->actor + 0x68);
        nm_trace_wake(t, was);
    }
    if (w == NMB_HOLD || w == NMB_STOP) {
        nm_stop_body(t, out);
        if (w == NMB_HOLD) NMV.st.back_wait++;
        else NMV.st.back_stop++;
        if (g_net_movers_log)
            LOG(w == NMB_HOLD ? "[net] movers: %s %#x: our Link stands where it goes back to: it waits, stopped at %.1f %.1f %.1f\n"
                              : "[net] movers: %s %#x not back at our place after 10 s: stopped at %.1f %.1f %.1f, ours again\n",
                t->name, t->h, cur[3], cur[7], cur[11]);
        if (w == NMB_STOP) nm_trace_start(t, "not back after 10 s, stopped where it was", L, now);
        return;
    }
    uint64_t body = no_has_body(t->actor) ? no_rd64(t->actor + 0x190) : 0;
    if (body) no_call(NOB.body_to, body, out);
    if (w == NMB_BOTH || !body) no_call(NOB.set_mtx, t->actor, out);
}

// every frame: his where he has them (120 ms behind), through their bodies; our Link at one = netmoverride's rule
static void nm_drive(const float L[3], uint64_t now) {
    for (int i = 0; i < NM_TRACK; i++) {
        NmTrk *t = &NMV.t[i];
        if (!t->h) continue;
        nm_trace_frame(t, L, now);  // (s292 coopfix: netmoverstrace 1 only, after a let-go)
        if (t->back.on) { nm_back_frame(t, L, now); continue; }  // (s290 labfix: on its way back to our place)
        if (!t->his_at) continue;
        bool alive = nm_alive(t);
        if (!alive || now - t->his_at > NM_HOLD_MS) {  // (his are over: back to our place, then ours; s290 labfix)
            bool said = t->said_his;
            t->his_at = 0, t->ring.n = 0, t->said_his = t->said_link = t->said_near = false;
            float cur[12];
            if (!alive) {
                memset(&t->back, 0, sizeof t->back);
                if (g_net_movers_log && said) LOG("[net] movers: %s %#x: no record of %s's for 1.5 s: ours again\n", t->name, t->h, no_who(t->his_player));
                continue;
            }
            bool ok = no_mtx(t->actor, cur);
            if (nm_back_start(&t->back, now)) {
                NMV.st.back++;
                if (g_net_movers_log && said)
                    LOG("[net] movers: %s %#x: no record of %s's for 1.5 s: it goes back to our place of it (%.1f %.1f %.1f, %.1f m from here; AS %s)\n",
                        t->name, t->h, no_who(t->his_player), t->back.own[3], t->back.own[7], t->back.own[11],
                        ok ? no_d3((const float[3]){cur[3], cur[7], cur[11]}, (const float[3]){t->back.own[3], t->back.own[7], t->back.own[11]}) : -1.0f,
                        t->own_as[0] ? t->own_as : "-");
                char as[NET_OBJS_AS];
                if (t->own_as[0] && NOB.as_ok && strcmp(no_playing(t->actor, as), t->own_as)) {  // (ours, as before his)
                    bool played = no_play_as(t->actor, t->own_as, false);
                    if (g_net_movers_log) LOG("[net] movers: %s %#x: our AS %s %s (was his %s)\n", t->name, t->h, t->own_as, played ? "played" : "NOT played", as[0] ? as : "-");
                }
                nm_back_frame(t, L, now);
            } else {  // (no place of ours kept: stopped where it is)
                if (ok) nm_stop_body(t, cur);
                memset(&t->back, 0, sizeof t->back), NMV.st.back_stop++;
                if (g_net_movers_log && said) LOG("[net] movers: %s %#x: no record of %s's for 1.5 s: ours again, stopped where it is (no place of ours kept)\n", t->name, t->h, no_who(t->his_player));
                nm_trace_start(t, "his over, no place of ours kept: stopped where it was", L, now);
            }
            continue;
        }
        float cur[12], want[12];
        if (!no_mtx(t->actor, cur) || !nm_ring_at(&t->ring, now - NET_INTERP_MS, want)) continue;
        bool big = nm_big(t->name), link_at = L && (nm_link_at(L, cur, big) || nm_link_at(L, want, big));
        t->o.theirs_ms = now;  // (while his are shown, its moves here are his)
        if (link_at && !g_net_mover_ride) {
            NMV.st.link_skip++;
            if (g_net_movers_log && !t->said_link)
                LOG("[net] movers: our Link is at %s %#x: %s's moves of it are not shown while he is (netmoverride 0)\n", t->name, t->h, no_who(t->his_player));
            t->said_link = true;
            continue;
        }
        if (g_net_movers_log && t->said_link && !link_at) LOG("[net] movers: our Link left %s %#x: it goes his way again\n", t->name, t->h);
        t->said_link = false;
        if (no_rd8(t->actor + 0x64) != 1) {  // (asleep: its body takes nothing; woken, it goes to another proc unit: watched there, PR)
            unsigned was = no_rd8(t->actor + 0x64);
            no_call(NOB.wake, t->actor, NULL);
            t->unit = no_rd64(t->actor + 0x68);
            nm_trace_wake(t, was);
        }
        uint64_t body = no_has_body(t->actor) ? no_rd64(t->actor + 0x190) : 0;
        if (link_at) {  // (netmoverride 1: slowly, through its body only: it carries our Link, never jumps under him)
            float step[12];
            nm_step(cur, want, NM_RIDE_STEP, NM_RIDE_RAD, step);
            if (body) no_call(NOB.body_to, body, step);
            else no_call(NOB.set_mtx, t->actor, step);  // (no body: nothing to stand on, only drawn)
            NMV.st.ride_steps++;
        } else if (L && nm_link_near(L, cur, want)) {  // (our Link within 30 m: it never jumps, it steps there; GUESS: both calls as below)
            float step[12];
            bool held = nm_step(cur, want, NM_NEAR_STEP, NM_NEAR_RAD, step);
            if (body) no_call(NOB.body_to, body, step);
            no_call(NOB.set_mtx, t->actor, step);
            NMV.st.near_steps++, NMV.st.near_held += held;
            if (g_net_movers_log && held && !t->said_near)
                LOG("[net] movers: %s %#x: %s's place of it is %.1f m off, our Link within 30 m: it goes there in steps (0.2 m / 2.9 deg a frame), not at once\n",
                    t->name, t->h, no_who(t->his_player), no_d3((const float[3]){cur[3], cur[7], cur[11]}, (const float[3]){want[3], want[7], want[11]}));
            if (held) t->said_near = true;  // (one line while his are shown: the counts in netobjsdiag)
        } else {
            if (body) no_call(NOB.body_to, body, want);
            no_call(NOB.set_mtx, t->actor, want);  // (PR: both, every frame)
            NMV.st.driven++;
        }
        if (t->his_as[0] && now >= t->as_try && NOB.as_ok) {
            t->as_try = now + NM_AS_TRY_MS;
            char as[NET_OBJS_AS];
            if (strcmp(no_playing(t->actor, as), t->his_as)) {
                bool ok = no_play_as(t->actor, t->his_as, false);
                if (ok) NMV.st.as_played++;
                else NMV.st.as_failed++;
                if (g_net_movers_log && (ok || !t->said_as)) LOG("[net] movers: %s %#x: his AS %s %s (was %s)\n", t->name, t->h, t->his_as, ok ? "played" : "NOT played", as[0] ? as : "-");
                if (!ok) t->said_as = true;
            }
        }
    }
}

// (s290 labfix) one his / on its way back: its body stopped where it is first (the velocity his moves left: lab s290 E3); the
// call needs the frame (no_call refuses outside it: then as before s290)
static void nm_forget(void) {
    for (int i = 0; i < NM_TRACK; i++) {
        NmTrk *t = &NMV.t[i];
        float cur[12];
        // (s291 review 1) never during a load: the actor may be half deleted (cleared only, as before s290)
        if (!NET.loading && t->h && (t->his_at || t->back.on) && nm_alive(t) && no_mtx(t->actor, cur)) nm_stop_body(t, cur);
    }
    memset(NMV.t, 0, sizeof NMV.t);
}

// (s290 labfix) live "set netmoverelev 0|1" (lab; sora_net.h net_live_set, before net_objs_live_set)
static bool nm_live_set(const char *key, int v) {
    if (NO_KEYIS(key, "netmoverstrace")) {  // (s292 coopfix, lab: reads + lines only)
        if (g_net_mover_trace == (v != 0)) return true;
        g_net_mover_trace = v != 0;
        LOG("[net] movers: netmoverstrace %d%s\n", g_net_mover_trace,
            g_net_mover_trace ? ": a mover let go (back at our place / its own logic / given up) is traced 10 s, a line each 0.25 s; his stream's start and our wake said" : "");
        return true;
    }
    if (!NO_KEYIS(key, "netmoverelev")) return false;
    if (g_net_mover_elev == (v != 0)) return true;
    g_net_mover_elev = v != 0;
    LOG("[net] movers: netmoverelev %d: %s\n", g_net_mover_elev, g_net_mover_elev
        ? "a shrine's entrance elevator / shutter is watched again (lab: each player's own enter / leave scene shows in the other's world)"
        : "a shrine's entrance elevator / shutter is left to each game (default: each player's own enter / leave scene)");
    return true;
}

static void nm_diag(const float L[3]) {
    int n = 0, his = 0, ours = 0;
    for (int i = 0; i < NM_TRACK; i++) n += NMV.t[i].h != 0, his += NMV.t[i].his_at != 0, ours += NMV.t[i].said_ours;
    LOG("[net] movers: %s, ride %d; watched %d (%d his, %d ours; %u not watched: full; %u re-watched after a wake); sent %u records; "
        "received %u (%u ours kept, %u not watched here); shown %u frames at once, %u held back (our Link at it), %u slow steps (netmoverride 1), "
        "%u near steps (%u slowed: would have jumped); AS played %u, not %u%s; his over: %u went back to our place (%u there, %u stopped where "
        "they were, %u let go: their own logic moved them, %u waited for our Link), %u sightings of a shrine entrance elevator / shutter not watched (netmoverelev %d)\n",
        g_net_movers ? (NOB.movers_ok ? "on" : "on but OFF here (see the next line)") : "OFF", g_net_mover_ride, n, his, ours, NMV.st.full,
        NMV.st.rewatched, NMV.st.tx_rec, NMV.st.rx_rec, NMV.st.rx_ours, NMV.st.rx_unknown, NMV.st.driven, NMV.st.link_skip, NMV.st.ride_steps,
        NMV.st.near_steps, NMV.st.near_held, NMV.st.as_played, NMV.st.as_failed, NOB.movers_ok && !NOB.as_ok ? " (no AS rows: his AS not played)" : "",
        NMV.st.back, NMV.st.back_done, NMV.st.back_stop, NMV.st.back_own, NMV.st.back_wait, NMV.st.scene_skip, g_net_mover_elev);
    if (g_net_movers && !NOB.movers_ok) LOG("[net] movers: %s\n", no_rows_txt("movers"));
    bool said[NM_TRACK] = {0};  // (the lab: the 8 nearest, to compare both games' lines by HashId)
    for (int k = 0; k < 8 && L; k++) {
        int best = -1;
        float bd = 1e30f;
        for (int i = 0; i < NM_TRACK; i++) {
            float m[12];
            if (said[i] || !NMV.t[i].h || !no_mtx(NMV.t[i].actor, m)) continue;
            float d = no_d3(L, (const float[3]){m[3], m[7], m[11]});
            if (d < bd) bd = d, best = i;
        }
        if (best < 0) break;
        said[best] = true;
        const NmTrk *t = &NMV.t[best];
        float m[12];
        no_mtx(t->actor, m);
        LOG("[net] movers:   %s %#x at %.2f %.2f %.2f, %.1f m, AS %s (%s)\n", t->name, t->h, m[3], m[7], m[11], bd, t->as[0] ? t->as : "-",
            t->his_at ? (t->said_link ? "his, held back: our Link at it" : "his: shown") : t->back.on ? "on its way back to our place"
            : t->said_ours ? "ours: sending" : "still");
    }
}

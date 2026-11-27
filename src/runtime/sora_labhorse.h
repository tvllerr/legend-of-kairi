// sora_labhorse.h (s291 labhorse): a LAB-ONLY live line that puts a rideable horse next to Link, for nethorse's lab tests
// F2-F7 (sb/s289/objs/LAB_STEPS.md "F": "our Link rides"). Neither lab save owns a horse (s290 / s291 LAB_RESULTS (f)).
// ONE-SHOT lines of the live file (take them out again after they fired, as `give` / `warpto`):
//   labhorse [GameRomHorseNN]  ask the game for that horse BY NAME (default GameRomHorse01) with the game's own actor request
//                              (the loot_request row: netloot's "made here", VERIFIED in game s289-s290 for items) and put it at
//                              Link's side with BotW's own warp (Actor::setMtx, no_warp: VERIFIED s289 for barrels). One lab
//                              horse at a time. Names nethorse counts only (GameRomHorse*, not Saddle / Reins; GameROMMotorcycle).
//   labhorse here              our lab horse (else the nearest loaded horse) back to Link's side (after a fast travel, a fall)
//   labhorse near [m]          the nearest LOADED horse within m (default 150) to Link's side: a wild one of the field
//   labhorse off               our lab horse deleted (refused while Link rides; a request still loading is deleted once made)
//   labhorse stam 0|1          1 = Link's stamina held full while he rides (a wild horse bucks while his stamina lasts: GUESS
//                              that a full wheel always outlasts it); 0 (default) = as BotW
//   labhorse diag              one line: our horse, the ride (state word + PlayerInfo's horse link), the horses near
// GUESS (not seen in game yet): that a horse made this way (no map object, no stable) has its AI and can be mounted (a wild
// horse: A from its side, then it bucks until calm; soothe = L); that the riding bit / PlayerInfo +0x78 mHorseLink (zeldaret
// actPlayerInfo.h, 1.5.0) name the ridden horse; that +X of his yaw is a side he can mount from (try the other with `here`).
// Never in a release: the live file exists only with SAVAGEWIND_SORA_LIVE (dev / lab launchers; the release ones set it
// empty), and nothing here runs before a labhorse line. Idle = one compare a frame.
// Needs (sora.c): g_loading, TITLE, gs_actor / STAM_CUR / STAM_MAX, LOG, and sora_net_objs.h's NOB / no_* (no_bind binds).
#pragma once
#include "sora_labhorse_core.h"

// the request's handle + its name: calloc'd once, NEVER freed (the game keeps their addresses while a request loads: a hot
// reload with one in flight leaves the game a valid handle, as loot's LtHandles)
typedef struct { uint64_t unit, flag; char name[LH_NAME_MAX]; } LhMem;

static struct {
    LhCmd cmd;                // a command waiting for a game-thread frame
    char want[LH_NAME_MAX];   // (LH_SPAWN)
    float arg;                // (LH_NEAR metres, LH_STAM 0 / 1)
    uint64_t cmd_at;
    LhMem *mem;
    bool pending, orphan, said_slow;  // (a request loading; orphan = `off` came first: deleted once made)
    uint64_t asked_at;
    uint64_t horse, unit;     // our lab horse (made by us, or the one `near` / `here` moved: ours = we made it)
    uint32_t id;
    bool ours;
    char name[LH_NAME_MAX];
    bool stam, rides, held_told;
    uint32_t sw;
} LH;

static uint64_t lh_x0(uint64_t fn, uint64_t x0) {  // fn(x0) -> x0 (game thread only)
    if (!fn || !NOB.api || !NOB.api->in_frame()) return 0;
    SwArgs a;
    memset(&a, 0, sizeof a);
    a.x[0] = x0, a.nx = 6;
    SwRet r = {0};
    return NOB.api->call(NULL, fn, &a, &r) ? r.x0 : 0;
}
static bool lh_rows(void) {
    return NOB.api && SW_HAS(NOB.api, call) && SW_HAS(NOB.api, vcall) && SW_HAS(NOB.api, in_frame) && SW_HAS(NOB.api, actor_name) &&
           NOB.req && NOB.take && NOB.release && NOB.del && NOB.creator_p && NOB.heap_p && NOB.set_mtx && NOB.wake;
}
static bool lh_alive(void) { return LH.horse && no_alive(LH.horse, LH.unit, LH.id); }
static const char *lh_nm(uint64_t a) {
    const char *n = a && NOB.api ? NOB.api->actor_name(a) : NULL;
    return n ? n : "?";
}
// the actor PlayerInfo's mHorseLink (+0x78: a BaseProcLink, unit + id as loot's lt_linked reads the weapon links) names (GUESS)
static uint64_t lh_pi_horse(void) {
    uint64_t pi = NOB.ss ? no_rd64(NOB.ss->player_info) : 0, u = pi ? no_rd64(pi + 0x78) : 0;
    uint32_t id = pi ? no_rd32(pi + 0x80) : 0;
    if (!NO_VALID(u) || no_rd32(u + 0x40) != id) return 0;
    uint64_t a = no_rd64(u + 0x48);
    return NO_VALID(a) && no_rd8(a + 0x64) != 3 ? a : 0;
}
static bool lh_riding(uint32_t *sw) {
    *sw = NOB.state_word ? NOB.state_word() : 0;
    return (*sw & NB_SW_RIDING) != 0;
}
// put a at Link's side (BotW's warp; the PR's set_mtx when the warp call is refused). -> true = moved
static bool lh_put(uint64_t a, const char *why) {
    uint64_t la = no_link_actor();
    float lm[12], m[12], hm[12];
    if (!la || !no_mtx(la, lm)) { LOG("[labhorse] %s: no Link matrix, not moved\n", why); return false; }
    lh_side_mtx(lm, m);
    if (no_rd8(a + 0x64) != 1) no_call(NOB.wake, a, NULL);  // (asleep / far: woken first, as netobjs' moves)
    bool own = false, ok = no_warp(a, m, &own);
    if (!ok) ok = no_call(NOB.set_mtx, a, m);
    float d = no_mtx(a, hm) ? no_d3((const float[3]){hm[3], hm[7], hm[11]}, (const float[3]){lm[3], lm[7], lm[11]}) : -1;
    LOG("[labhorse] %s: %s (actor %#llx) put at %.2f %.2f %.2f by %s%s; now %.2f m from Link (at %.2f %.2f %.2f)\n", why, lh_nm(a),
        (unsigned long long)a, m[3], m[7], m[11], ok ? "BotW's warp" : "nothing (both calls refused)", own ? " (its class's own setMtx)" : "", d,
        lm[3], lm[7], lm[11]);
    return ok;
}
// the horses loaded now: their actors, names, places (Link and actors being deleted skipped). -> how many
#define LH_MAXH 64
static int lh_horses(uint64_t *ha, const char **hn, float (*hp)[3], float me[3]) {
    uint64_t la = no_link_actor();
    float lm[12];
    if (!la || !no_mtx(la, lm)) return 0;
    me[0] = lm[3], me[1] = lm[7], me[2] = lm[11];
    int n = no_all_actors(no_rd64(la + 0x68), g_no_act, NO_MAX_ACTORS), k = 0;
    for (int i = 0; i < n && k < LH_MAXH; i++) {
        uint64_t a = g_no_act[i];
        float m[12];
        const char *nm = a == la || no_rd8(a + 0x64) == 3 ? NULL : NOB.api->actor_name(a);
        if (!nm || !nh_horse(nm) || !no_mtx(a, m)) continue;
        ha[k] = a, hn[k] = nm, hp[k][0] = m[3], hp[k][1] = m[7], hp[k][2] = m[11], k++;
    }
    return k;
}
static void lh_adopt(uint64_t a, bool ours) {
    LH.horse = a, LH.unit = no_rd64(a + 0x68), LH.id = no_rd32(a + 0x60), LH.ours = ours;
    snprintf(LH.name, sizeof LH.name, "%s", lh_nm(a));
}

static void lh_spawn(uint64_t now) {
    if (lh_alive()) { LOG("[labhorse] refused: our lab horse %s is still here (labhorse here = to Link, labhorse off = gone)\n", LH.name); return; }
    if (LH.pending) { LOG("[labhorse] refused: a request for a %s is still loading\n", LH.mem->name); return; }
    if (!no_link_actor()) { LOG("[labhorse] refused: no Link\n"); return; }
    if (!LH.mem) LH.mem = (LhMem *)calloc(1, sizeof(LhMem));
    uint64_t cr = no_rd64(NOB.creator_p), ho = no_rd64(NOB.heap_p);
    cr = cr ? no_rd64(cr) : 0, ho = ho ? no_rd64(ho) : 0;
    uint64_t heap = ho ? no_rd64(ho + 0x28) : 0;
    if (!LH.mem || !cr || !heap) { LOG("[labhorse] refused: no actor creator / heap yet (creator %#llx heap %#llx)\n", (unsigned long long)cr, (unsigned long long)heap); return; }
    memset(LH.mem, 0, sizeof *LH.mem);
    snprintf(LH.mem->name, sizeof LH.mem->name, "%s", LH.want);
    // (nh_request's / lt_request's call: x0 creator, x1 the name, x2 the heap, x3 the handle, x4 params 0, x5 0, w6 1, s0 0)
    SwArgs a;
    memset(&a, 0, sizeof a);
    a.x[0] = cr, a.x[1] = (uint64_t)(uintptr_t)LH.mem->name, a.x[2] = heap, a.x[3] = (uint64_t)(uintptr_t)LH.mem, a.x[6] = 1, a.nx = 7;
    a.s[0] = 0, a.nv = 1;
    SwRet r = {0};
    if (!NOB.api->call(NULL, NOB.req, &a, &r) || !(r.x0 & 1)) { LOG("[labhorse] the game refused the request for a %s (x0 %#llx)\n", LH.want, (unsigned long long)r.x0); return; }
    LH.pending = true, LH.orphan = false, LH.said_slow = false, LH.asked_at = now;
    LOG("[labhorse] asked the game for a %s (by name, the loot request; lab only)\n", LH.want);
}
static void lh_pending(uint64_t now) {
    uint64_t u = *(volatile uint64_t *)&LH.mem->unit;
    int st = u ? (int)no_rd32(u) : -1;
    LhWait w = lh_wait(st, now - LH.asked_at, LH.said_slow);
    if (w == LHW_SLOW) LH.said_slow = true, LOG("[labhorse] the %s is still loading after %d s (unit state %d)\n", LH.mem->name, LH_SLOW_MS / 1000, st);
    if (w == LHW_FAILED) {
        lh_x0(NOB.release, (uint64_t)(uintptr_t)LH.mem), LH.pending = false;
        LOG("[labhorse] the game could not make a %s (unit state %d): released\n", LH.mem->name, st);
    }
    if (w != LHW_TAKE) return;
    uint64_t h = lh_x0(NOB.take, (uint64_t)(uintptr_t)LH.mem);
    LH.pending = false;
    if (!NO_VALID(h)) { LOG("[labhorse] the %s was ready but the take gave no actor (%#llx)\n", LH.mem->name, (unsigned long long)h); return; }
    if (LH.orphan) { lh_x0(NOB.del, h); LOG("[labhorse] the %s came after labhorse off: deleted\n", LH.mem->name); return; }
    lh_adopt(h, true);
    LOG("[labhorse] made %s (actor %#llx, id %u) after %.1f s\n", LH.name, (unsigned long long)h, LH.id, (now - LH.asked_at) / 1000.0);
    lh_put(h, "made");
}
static void lh_diag(void) {
    uint64_t ha[LH_MAXH];
    const char *hn[LH_MAXH];
    float hp[LH_MAXH][3], me[3] = {0};
    int n = lh_horses(ha, hn, hp, me);
    uint32_t sw;
    bool ride = lh_riding(&sw);
    uint64_t pih = lh_pi_horse();
    char hl[400] = "";
    int o = 0;
    for (int i = 0; i < n && i < 6 && o < (int)sizeof hl - 80; i++) {
        float dx = hp[i][0] - me[0], dy = hp[i][1] - me[1], dz = hp[i][2] - me[2];
        o += snprintf(hl + o, sizeof hl - o, " %s %#llx %.1f m;", hn[i], (unsigned long long)ha[i], sqrtf(dx * dx + dy * dy + dz * dz));
    }
    LOG("[labhorse] diag: our horse %s%s%s, request %s; Link %s (state %#x), PlayerInfo +0x78 horse link -> %s %#llx%s; stam hold %s; "
        "%d horse(s) loaded:%s\n", LH.horse ? LH.name : "none", LH.horse ? (lh_alive() ? " (alive" : " (GONE") : "", LH.horse ? (LH.ours ? ", made by us)" : ", moved by us)") : "",
        LH.pending ? "loading" : "none", ride ? "RIDES" : "does not ride", sw, pih ? lh_nm(pih) : "nothing", (unsigned long long)pih,
        pih && pih == LH.horse ? " (= ours)" : "", LH.stam ? "on" : "off", n, n ? hl : " none");
}
static void lh_run(uint64_t now) {
    switch (LH.cmd) {
    case LH_SPAWN: lh_spawn(now); break;
    case LH_OFF: {
        uint32_t sw;
        if (lh_alive() && lh_riding(&sw)) { LOG("[labhorse] off refused: Link rides (get off first)\n"); break; }
        if (LH.pending) LH.orphan = true, LOG("[labhorse] off: the %s still loading is deleted once made\n", LH.mem->name);
        if (lh_alive() && LH.ours) lh_x0(NOB.del, LH.horse), LOG("[labhorse] off: our lab horse %s deleted\n", LH.name);
        else if (lh_alive()) LOG("[labhorse] off: %s is the field's (moved, not made by us): left as it is\n", LH.name);
        else if (!LH.pending) LOG("[labhorse] off: no lab horse\n");
        LH.horse = 0;
        break;
    }
    case LH_HERE:
    case LH_NEAR: {
        uint32_t sw;
        if (lh_riding(&sw)) { LOG("[labhorse] %s refused: Link rides\n", LH_CMD_TXT[LH.cmd]); break; }
        if (LH.cmd == LH_HERE && lh_alive()) { lh_put(LH.horse, "here"); break; }
        uint64_t ha[LH_MAXH];
        const char *hn[LH_MAXH];
        float hp[LH_MAXH][3], me[3], d = 0;
        int n = lh_horses(ha, hn, hp, me), i = lh_nearest(me, (const float (*)[3])hp, hn, NULL, n, LH.cmd == LH_NEAR ? LH.arg : LH_NEAR_M, &d);
        if (i < 0) { LOG("[labhorse] %s: no loaded horse within %.0f m (%d loaded): try labhorse (made) or the fields\n", LH_CMD_TXT[LH.cmd], LH.cmd == LH_NEAR ? LH.arg : LH_NEAR_M, n); break; }
        LOG("[labhorse] %s: %s %.1f m away (of %d loaded)\n", LH_CMD_TXT[LH.cmd], hn[i], d, n);
        lh_adopt(ha[i], false);
        lh_put(ha[i], LH_CMD_TXT[LH.cmd]);
        break;
    }
    case LH_STAM:
        LH.stam = LH.arg != 0, LH.held_told = false;
        LOG("[labhorse] stam %d: %s\n", LH.stam, LH.stam ? "Link's stamina held full while he rides (lab only)" : "stamina as BotW");
        break;
    case LH_DIAG: lh_diag(); break;
    default: break;
    }
}

// the live line (sora.c's live file loop): true = it was a labhorse line (done on the next game-thread frame)
static bool lh_live_line(const char *ln) {
    char name[LH_NAME_MAX];
    float arg;
    LhCmd c = lh_parse(ln, name, &arg);
    if (c == LH_NONE) return false;
    if (c == LH_BAD) {
        LOG("[labhorse] live: \"%s\" not understood: labhorse [GameRomHorseNN|GameROMMotorcycle] | here | near [m] | off | stam 0|1 | diag\n", ln);
        return true;
    }
    if (LH.cmd != LH_NONE) LOG("[labhorse] live: %s dropped for %s (one at a time)\n", LH_CMD_TXT[LH.cmd], LH_CMD_TXT[c]);
    LH.cmd = c, LH.arg = arg, LH.cmd_at = GetTickCount64();
    snprintf(LH.want, sizeof LH.want, "%s", name);
    LOG("[labhorse] live: %s%s%s (lab only)\n", LH_CMD_TXT[c], name[0] ? " " : "", name);
    return true;
}

// every frame, after warp_tick (the game thread, inside the mod frame). Idle (no labhorse line yet) = nothing
static void lh_tick(uint64_t now) {
    if (LH.cmd == LH_NONE && !LH.pending && !LH.stam && !LH.horse) return;
    no_bind();
    if (!lh_rows()) {
        if (LH.cmd != LH_NONE) LOG("[labhorse] refused: %s\n", no_rows_txt("labhorse"));
        LH.cmd = LH_NONE, LH.pending = false, LH.stam = false, LH.horse = 0;
        return;
    }
    uint32_t ps = NOB.ss && NOB.ss->pause_state ? no_rd32(NOB.ss->pause_state) : 0;  // (loot's test: 0 playing, 2 the pause menu / map)
    bool frame = NOB.api->in_frame() && !g_loading && !TITLE.on && ps == 0;
    if (LH.cmd != LH_NONE && !frame && now - LH.cmd_at > LH_FRAME_MS)
        LOG("[labhorse] %s refused: no game-thread world frame for %d s (loading / title / paused?)\n", LH_CMD_TXT[LH.cmd], LH_FRAME_MS / 1000), LH.cmd = LH_NONE;
    if (!frame) return;
    if (LH.cmd != LH_NONE) {
        LhCmd c = LH.cmd;
        lh_run(now);
        if (LH.cmd == c) LH.cmd = LH_NONE;
    }
    if (LH.pending) lh_pending(now);
    if (LH.horse && !lh_alive()) LOG("[labhorse] our lab horse %s is gone (the game removed it: a load / too far?)\n", LH.name), LH.horse = 0;
    uint32_t sw;
    bool ride = lh_riding(&sw);
    if (ride != LH.rides) {
        uint64_t pih = lh_pi_horse();
        LOG("[labhorse] Link %s (state %#x); PlayerInfo +0x78 horse link -> %s%s\n", ride ? "RIDES" : "rides no more", sw, pih ? lh_nm(pih) : "nothing",
            pih && pih == LH.horse ? " (= our lab horse)" : "");
        LH.rides = ride, LH.held_told = false;
    }
    // (the riding bit is a GUESS while a wild horse bucks: PlayerInfo's horse link on our lab horse counts too; it may stay set
    // after he gets off = the hold goes on there too: harmless in the lab)
    bool hold = ride || (LH.stam && LH.horse && lh_pi_horse() == LH.horse);
    if (LH.stam && hold) {  // (sora.c glidestam_set's write: PlayerInfo +0xa4 = the actor's max)
        uint64_t pi, a = gs_actor(&pi);
        if (a) {
            float max = GV(float, a, STAM_MAX);
            if (GV(float, pi, STAM_CUR) < max) *(float *)(uintptr_t)(pi + STAM_CUR) = max;
            if (!LH.held_told) LH.held_told = true, LOG("[labhorse] stam: held at %.0f while Link rides\n", max);
        }
    }
}

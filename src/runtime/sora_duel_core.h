// (s272 duel, co-op item 6) DUELS between two players: the pure parts, no game state, host-tested by runtime/test/duel_test.c.
// The game side is runtime/sora_duel.h (sora.c), the net side runtime/sora_net_duel.h (sora_net.h), the wire NET_DUEL in
// runtime/sora_net_proto.h. Design = the user's answers (RESUME "CO-OP PLAN" 6, s270) + recon/s269/S269_HELPER_DUEL.md:
//   ask      a Triangle "Duel" near a friend (out of combat) -> his Triangle "Accept Duel" for 10 s (no answer = decline)
//   ring     10 m, centred between the two at the accept; leaving it (0.5 s outside) = forfeit
//   count    3-2-1 then "Go!"; the fight lasts 3 min at most, then the higher HP % wins (equal = a draw)
//   damage   KH2's formula as it is (FUN_1403c12f0, recon/S109_HELPER_KH2DMG.md, VERIFIED decompile): the attacker's game
//            decides the hit (his blade / spell met the puppet he sees), the victim's game the damage (his own Defense,
//            his difficulty) and writes it; the loser drops to 1 HP (KO), never 0: no death, no Game Over
//   allowed  combos, magic, Guard, dodge; no Drive Forms / Limits / summons / items / party, no Drive gauge from duel hits
//   end      KO, leaving the ring, an enemy coming near (no winner), the time limit, a load / map change / the other game gone;
//            both healed + MP full after. World damage can't take you below 1 during a duel (it does not end it).
//   others   a third player sees the ring (NDO_RING), can't hit / be hit (no path exists: duel hits only go to the opponent)
// FRIENDLY FIRE stays OFF everywhere outside a duel: every hit path here needs a duel in its FIGHT phase.
// Numbers: GUESS unless marked (the user's s270 answers are marked "user").
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DU_RING_M 20.0f        // ring radius, m (= BotW units) (s270 FYI default 10; user s274: "double the size")
#define DU_RING_FX_REF_M 10.0f // duelringpct's fx size is measured at this radius (the s270 10 m ring)
#define DU_RING_DY 8.0f        // a body this far above / below the centre is out of the ring too (GUESS: a fall off a cliff)
#define DU_NEAR_M 6.0f         // the asker's "Duel" prompt shows with a friend this close (GUESS, recon)
#define DU_ACCEPT_MS 10000     // the friend's "Accept Duel" window (user: 10 s, no answer = decline)
#define DU_ASK_SLACK_MS 2500   // the asker waits 10 s + this for the answer (latency; an older build never answers)
#define DU_COUNT_MS 3000       // 3-2-1 (user: a 3-2-1 countdown)
#define DU_GO_MS 800           // "Go!" on screen after the count (GUESS)
#define DU_LIMIT_MS 180000     // time limit (user: 3 min, then the higher HP % wins)
#define DU_OUT_MS 500          // outside the ring this long in a row = forfeit (GUESS: a knockback across the line does not end it)
#define DU_ENEMY_M 30.0f       // "out of combat" = no live Enemy_* (animals excluded) this close (user FYI default s270)
#define DU_CHASE_M 60.0f       // ... and no Enemy_* within this whose target state says it hunts Link (GUESS, recon S269 sec 1)
#define DU_HIT_HOLD_MS 10000   // ... and no hit taken in this long (user FYI default s270; the party's PC_BATTLE_HIT_HOLD)
#define DU_PEER_GRACE_MS 2500  // after the accept: the other game's SNAP needs this long to show NF_DUEL (GUESS)
#define DU_PEER_MS 3000        // no NF_DUEL from the opponent this long = his game left the duel (crash, hot reload, lost END)
#define DU_TIME_WAIT_MS 3000   // at the time limit: wait this long for his TIME report, then his last SNAP's HP % decides
#define DU_OVER_MS 3500        // the result banner, then idle
#define DU_COOL_MS 5000        // no new "Duel" prompt / ask this long after a duel / a refusal (GUESS)
#define DU_RING_TX_MS 2000     // NDO_RING to everyone this often while a duel runs (a third player / a late joiner)
#define DU_RING_STALE_MS 5500  // a third player drops a ring not refreshed this long
#define DU_FRONT_MS 1000       // the victim's flinch faces the attacker for this long after a duel hit
#define DU_RESEND_MS 90        // UDP: ANSWER / HIT / END / TIME go again this much later (Melty room events are reliable: once)
#define DU_RESENDS 2           // ... this many extra times (the receiver drops the copies)

// ---- phases / roles / results / end reasons / answers ----
enum { DU_IDLE = 0, DU_ASKING, DU_ASKED, DU_COUNT, DU_FIGHT, DU_TIMEUP, DU_OVER, DU_NPH };
static const char *const DU_PH_NAME[DU_NPH] = {"idle", "asking", "asked", "countdown", "fight", "time up", "over"};
enum { DU_ROLE_NONE = 0, DU_ROLE_ASKER, DU_ROLE_FRIEND };
enum { DUR_NONE = 0, DUR_WIN, DUR_LOSE, DUR_DRAW, DUR_CANCEL, DUR_N };
static const char *const DUR_NAME[DUR_N] = {"none", "win", "lose", "draw", "cancel"};
// (s289: DUE_GAVEUP = the menu's Forfeit / the live "duel end" line in a fight; DUE_FORFEIT stays the ring-out. New reasons go at the
//  END: the wire sends the number, and an older build turns a number >= its DUE_N into DUE_CANCEL, sora_duel.h s273 H1)
enum { DUE_NONE = 0, DUE_KO, DUE_FORFEIT, DUE_LEFT, DUE_ENEMY, DUE_TIME, DUE_GONE, DUE_CANCEL, DUE_GAVEUP, DUE_N };
static const char *const DUE_NAME[DUE_N] = {"none", "KO (dropped to 1 HP)", "left the ring", "left (a load / another map / Sora mode off)",
                                            "an enemy came near", "time limit", "the other game left the duel", "cancelled", "forfeit (gave up)"};
enum { DUA_ACCEPT = 1, DUA_DECLINE, DUA_BUSY };  // (DUA_DECLINE = his 10 s ran out or he walked off; BUSY + a DUG_* why)

// ---- the gate: may this game ask / be asked / accept now ----
enum { DUG_OK = 0, DUG_OFF, DUG_NOT_DRIVEN, DUG_BUSY, DUG_COOL, DUG_MENU, DUG_FORM, DUG_SUMMON, DUG_BOARD, DUG_AIR, DUG_ENEMY,
       DUG_CHASE, DUG_HIT, DUG_FAR, DUG_N };
static const char *const DUG_WHY[DUG_N] = {"ok", "duels are off there (set duel 0)", "not in the world (a load / the title / dead / Link has the pad)",
                                           "already in a duel", "a duel just ended (5 s)", "a menu is open", "in a Drive Form / a guest body",
                                           "a summon is out", "on the skateboard", "not standing on the ground",
                                           "an enemy is near (30 m)", "an enemy is after you", "hit in the last 10 s",
                                           "too far / another map"};
typedef struct {
    bool on;          // live duel 1
    bool driven;      // Sora drives Link in the world (not loading / dead / the title / Link's own state)
    bool busy;        // a duel / a prompt of ours already runs
    bool menu;        // the command menu's sub page / a camp / picker screen
    bool form;        // a Drive Form or a guest body (Mickey)
    bool summon;      // a summon out
    bool board;       // riding the skateboard
    bool ground;      // standing on the ground (Link's state "ground", Sora not in the air)
    uint64_t now, cool_until, last_hit_ms;  // last_hit_ms 0 = never
    float enemy_m;    // nearest live Enemy_* (horizontal, m); < 0 = none
    float chase_m;    // nearest Enemy_* hunting Link; < 0 = none
} DuGateIn;
// strict: the moment of a press (asker's "Duel", friend's "Accept"): + menu / board / air. Not strict: when an ASK comes in
// (he may be gliding: the prompt waits for him to land).
static inline int du_gate(const DuGateIn *g, bool strict) {
    if (!g || !g->on) return DUG_OFF;
    if (!g->driven) return DUG_NOT_DRIVEN;
    if (g->busy) return DUG_BUSY;
    if (g->now < g->cool_until) return DUG_COOL;
    if (strict && g->menu) return DUG_MENU;
    if (g->form) return DUG_FORM;
    if (g->summon) return DUG_SUMMON;
    if (strict && g->board) return DUG_BOARD;
    if (strict && !g->ground) return DUG_AIR;
    if (g->enemy_m >= 0 && g->enemy_m <= DU_ENEMY_M) return DUG_ENEMY;
    if (g->chase_m >= 0 && g->chase_m <= DU_CHASE_M) return DUG_CHASE;
    if (g->last_hit_ms && g->now < g->last_hit_ms + DU_HIT_HOLD_MS) return DUG_HIT;
    return DUG_OK;
}
// an enemy name that counts for "combat": Enemy_* only (BotW's animals, Animal_*, are in the same list: they do not count)
static inline bool du_enemy_name(const char *nm) { return nm && !strncmp(nm, "Enemy_", 6); }
// (s290 duelhorse, user: "make the duel prompt not take over X near horses") a horse one can mount: wild Animal_Horse_* (and
// the other Animal_Horse kinds), a registered GameRomHorse* (not its saddle / reins), the Master Cycle
#define DU_HORSE_M 5.0f        // no "Duel" prompt while a horse is this close (GUESS: BotW's Mount prompt is ~2 m; 5 m covers a walk up)
static inline bool du_horse_name(const char *nm) {
    if (nm && !strncmp(nm, "OnlineHorse_", 12)) nm += 12;  // (s291 duelride: a friend's horse puppet, nethorse's, by the horse it stands for)
    return nm && (!strncmp(nm, "Animal_Horse", 12) || (!strncmp(nm, "GameRomHorse", 12) && !strstr(nm, "Saddle") && !strstr(nm, "Reins")) ||
                  !strcmp(nm, "GameROMMotorcycle"));
}
// (s291 duelride, s291 lab: copy2 got "Duel" toward pc riding 0.9 m away; the stall horses were the only horses duelhorse saw)
// why the "Duel" prompt is not offered now: DURB_NONE it may be; DURB_WE_RIDE our Link rides; DURB_PUPPET a friend's horse puppet
// (nethorse) within the duelhorse distance (puppet_m < 0 = none shown; horse_m <= 0 = duelhorse 0: puppets do not count).
// duelride 0 = as before (DURB_NONE). A friend who rides is skipped by du_ride_skip (his record fresh = nh_rides).
enum { DURB_NONE, DURB_WE_RIDE, DURB_PUPPET };
static inline int du_ride_block(int duelride, bool we_ride, float puppet_m, float horse_m) {
    if (!duelride) return DURB_NONE;
    if (we_ride) return DURB_WE_RIDE;
    if (horse_m > 0 && puppet_m >= 0 && puppet_m <= horse_m) return DURB_PUPPET;
    return DURB_NONE;
}
static inline bool du_ride_skip(int duelride, bool he_rides) { return duelride && he_rides; }
// BotW's enemy target record state (enemy+0xcc4, recon S55: 0 none, 2 seen, 1/4 lost with the timer running, 3/5 special):
// 2 and 4 = it is after Link (the same two states linkpos.c's redirect works on). GUESS: valid for every Enemy_*.
static inline bool du_chasing_state(int32_t st) { return st == 2 || st == 4; }

// ---- a friend's hit blocked? (du_take_hit) ----
// The way KH2 blocks a hit on Sora here (sora_guard.h guard_frame's "want"): KH2's Guard window facing the attacker, Reflect's
// barrier, the invulnerable frames (post-block G.inv, g_rc_inv = the Reaction Dodge's, Quick Run's and, with rollinv, the Dodge
// Roll's), the Aerial Dodge block (adblock), a party Limit (refused in a duel anyway). (s284) a paused player takes the hit.
// (s288 port, user ruling 2026-11-25: Dodge Roll / Aerial Dodge / Quick Run also block a friend's duel hits, keys still OFF)
enum { DUB_NONE, DUB_GUARD, DUB_REFLECT, DUB_INV, DUB_ADODGE, DUB_LIMIT };
static inline int du_block_why(bool paused, bool guard_win, bool reflect, bool inv, bool adodge, bool limit) {
    if (paused) return DUB_NONE;
    return guard_win ? DUB_GUARD : reflect ? DUB_REFLECT : inv ? DUB_INV : adodge ? DUB_ADODGE : limit ? DUB_LIMIT : DUB_NONE;
}
static inline const char *du_block_name(int why) {
    return why == DUB_GUARD ? "Guard" : why == DUB_REFLECT ? "Reflect" : why == DUB_INV ? "invulnerable frames"
         : why == DUB_ADODGE ? "invulnerable frames (Aerial Dodge block)" : why == DUB_LIMIT ? "Limit" : "none";
}

// ---- damage (KH2 FUN_1403c12f0 as sora_levels.h sl_kh_apply / sl_def_tick use it, VERIFIED S109) ----
// V = S - Def, V = clamp(V, [max(1, Min), Cap]); dmg = ceil(Power x V / 100); then the boost (Combo / element Boost: ab_boost),
// then the victim's difficulty "taken" factor (0 x0.5, 1 x1, 2 x2, 3 x2 = SL_DIFF_TAKEN, VERIFIED). A player victim has no enmp
// row: Min 1, no Cap (GUESS). S = the attacker's Str + Keyblade Str (+2 Berserk Charge) for the blade, Mag + Keyblade Mag for a
// spell (his game sends it); Def = the victim's own lvup Defense. KH2 dmg = quarter hearts here (the mod's HUD HP = Link's
// life in quarter hearts: "1 KH2 HP per quarter heart", the user's pick s270 "KH2's formula as is").
static const int DU_DIFF_TAKEN[4] = {50, 100, 200, 200};
static inline int du_dmg(int S, int def, int power, int boost_pct, int diff, int *v_out) {
    // (s273 review L4: the wire carries u16s; clamp so power x V cannot overflow an int)
    if (S > 1000) S = 1000;
    if (power > 5000) power = 5000;
    if (boost_pct > 500) boost_pct = 500;
    int v = S - def;
    if (v < 1) v = 1;
    if (v_out) *v_out = v;
    if (power <= 0) return 0;
    int d = (power * v + 99) / 100;
    if (d < 1) d = 1;
    if (boost_pct > 0) d += (boost_pct * d + 99) / 100;
    if (diff < 0) diff = 0;
    if (diff > 3) diff = 3;
    d = (DU_DIFF_TAKEN[diff] * d + 99) / 100;
    return d < 1 ? 1 : d;
}
// (s286, the user: "make damage weaker in duels so they last a little longer") the victim's duel damage x pct / 100,
// rounded to the nearest, at least 1 (GUESS: KH2 has no PvP; 50 = a 3 becomes 2, a 2 or 1 stays 1). pct 100 = unchanged.
static inline int du_dmg_scale(int d, int pct) {
    if (d <= 0) return d;
    if (pct < 1) pct = 1;
    if (pct > 1000) pct = 1000;
    int r = (d * pct + 50) / 100;
    return r < 1 ? 1 : r;
}
// the victim's life after a duel hit: never below 1 (KH2's "1 HP" loser rule); *ko = this hit took him to the floor
static inline int du_life_after(int life, int dmg, bool *ko) {
    if (ko) *ko = false;
    if (life <= 0 || dmg <= 0) return life;
    int v = life - dmg;
    if (v <= 1) {
        if (ko) *ko = true;
        return 1;
    }
    return v;
}
// HP in permille of max (the time-limit judge; max 0 = 0)
static inline uint16_t du_pm(int life, int max) {
    if (max <= 0 || life <= 0) return 0;
    if (life >= max) return 1000;
    return (uint16_t)((1000LL * life) / max);
}
static inline int du_time_result(uint16_t mine, uint16_t his) { return mine > his ? DUR_WIN : mine < his ? DUR_LOSE : DUR_DRAW; }
// the other game's END (his reason, his own result) seen from here
static inline int du_result_from_peer(int reason, int his) {
    (void)reason;
    return his == DUR_LOSE ? DUR_WIN : his == DUR_WIN ? DUR_LOSE : his == DUR_DRAW ? DUR_DRAW : DUR_CANCEL;
}

// ---- the ring ----
static inline void du_mid(const float a[3], const float b[3], float c[3]) {
    for (int i = 0; i < 3; i++) c[i] = (a[i] + b[i]) * 0.5f;
}
static inline float du_hdist(const float a[3], const float b[3]) { return hypotf(a[0] - b[0], a[2] - b[2]); }
static inline bool du_inside(const float c[3], float r, const float p[3]) {
    if (!isfinite(p[0]) || !isfinite(p[1]) || !isfinite(p[2])) return false;
    return du_hdist(c, p) <= r && fabsf(p[1] - c[1]) <= DU_RING_DY;
}
typedef struct { uint64_t since; } DuOut;
// one frame of "outside the ring?": true once he has been outside DU_OUT_MS in a row
static inline bool du_out_step(DuOut *o, bool outside, uint64_t now) {
    if (!outside) { o->since = 0; return false; }
    if (!o->since) o->since = now ? now : 1;
    return now >= o->since + DU_OUT_MS;
}

// ---- the countdown / the clock on screen ----
// t0 = the count's start; -> 3, 2, 1 while counting, 0 = "Go!" (DU_GO_MS), -1 = nothing
static inline int du_count_digit(uint64_t t0, uint64_t now) {
    if (!t0 || now < t0) return -1;
    uint64_t e = now - t0;
    if (e < DU_COUNT_MS) return 3 - (int)(e / 1000);
    if (e < DU_COUNT_MS + DU_GO_MS) return 0;
    return -1;
}
static inline void du_clock_txt(uint64_t ms_left, char *b, int n) {
    unsigned s = (unsigned)((ms_left + 999) / 1000);
    snprintf(b, (size_t)n, "%u:%02u", s / 60, s % 60);
}

// ---- the duel's own state machine (one per game) ----
typedef struct {
    int phase, role;
    uint32_t id;          // the asker's nonce (never 0)
    uint64_t t0;          // this phase's start
    uint64_t count0;      // the countdown's start (the fight starts DU_COUNT_MS later)
    uint32_t limit_ms;    // 0 = DU_LIMIT_MS (live dueltime)
    int result, reason;   // (DU_OVER)
    bool my_time, his_time;
    uint16_t my_pm, his_pm;
} DuSm;
enum { DUS_NONE = 0, DUS_ASK_TIMEOUT, DUS_PROMPT_TIMEOUT, DUS_FIGHT, DUS_TIMEUP, DUS_TIME_DECIDE, DUS_IDLE };
static inline uint32_t du_limit(const DuSm *m) { return m->limit_ms ? m->limit_ms : DU_LIMIT_MS; }
static inline void du_sm_reset(DuSm *m) { uint32_t l = m->limit_ms; memset(m, 0, sizeof *m), m->limit_ms = l; }
static inline bool du_sm_busy(const DuSm *m) { return m->phase != DU_IDLE; }
static inline bool du_sm_live(const DuSm *m) { return m->phase == DU_COUNT || m->phase == DU_FIGHT || m->phase == DU_TIMEUP; }
static inline void du_sm_go(DuSm *m, int ph, uint64_t now) { m->phase = ph, m->t0 = now ? now : 1; }
// asker: our ASK went out
static inline bool du_sm_ask(DuSm *m, uint32_t id, uint64_t now) {
    if (m->phase != DU_IDLE || !id) return false;
    du_sm_reset(m), m->role = DU_ROLE_ASKER, m->id = id, du_sm_go(m, DU_ASKING, now);
    return true;
}
// friend: an ASK came in (false = we are busy: the caller answers BUSY)
static inline bool du_sm_asked(DuSm *m, uint32_t id, uint64_t now) {
    if (m->phase != DU_IDLE || !id) return false;
    du_sm_reset(m), m->role = DU_ROLE_FRIEND, m->id = id, du_sm_go(m, DU_ASKED, now);
    return true;
}
// both: the accept (the friend's press / the asker's ACCEPT answer) -> the countdown
static inline bool du_sm_count(DuSm *m, uint64_t now) {
    if (m->phase != DU_ASKING && m->phase != DU_ASKED) return false;
    du_sm_go(m, DU_COUNT, now), m->count0 = m->t0;
    return true;
}
static inline void du_sm_end(DuSm *m, int result, int reason, uint64_t now) {
    m->result = result, m->reason = reason;
    du_sm_go(m, DU_OVER, now);
}
// the fight's time left (ms), 0 outside the fight
static inline uint64_t du_sm_left(const DuSm *m, uint64_t now) {
    if (m->phase != DU_FIGHT) return 0;
    uint64_t end = m->count0 + DU_COUNT_MS + du_limit(m);
    return now >= end ? 0 : end - now;
}
// my TIME report / his: the judge once both are in (or DUS_TIME_DECIDE after the wait, with his SNAP's HP % from the caller)
static inline void du_sm_time_mine(DuSm *m, uint16_t pm) { m->my_time = true, m->my_pm = pm; }
static inline void du_sm_time_his(DuSm *m, uint16_t pm) { m->his_time = true, m->his_pm = pm; }
static inline bool du_sm_time_ready(const DuSm *m) { return m->phase == DU_TIMEUP && m->my_time && m->his_time; }
// one frame: the timed transitions (the caller acts on the event)
static inline int du_sm_step(DuSm *m, uint64_t now) {
    switch (m->phase) {
    case DU_ASKING:
        if (now >= m->t0 + DU_ACCEPT_MS + DU_ASK_SLACK_MS) return DUS_ASK_TIMEOUT;
        break;
    case DU_ASKED:
        if (now >= m->t0 + DU_ACCEPT_MS) return DUS_PROMPT_TIMEOUT;
        break;
    case DU_COUNT:
        if (now >= m->count0 + DU_COUNT_MS) { du_sm_go(m, DU_FIGHT, now); return DUS_FIGHT; }
        break;
    case DU_FIGHT:
        if (now >= m->count0 + DU_COUNT_MS + du_limit(m)) { du_sm_go(m, DU_TIMEUP, now); return DUS_TIMEUP; }
        break;
    case DU_TIMEUP:
        if (now >= m->t0 + DU_TIME_WAIT_MS) return DUS_TIME_DECIDE;
        break;
    case DU_OVER:
        if (now >= m->t0 + DU_OVER_MS) { du_sm_reset(m); return DUS_IDLE; }
        break;
    }
    return DUS_NONE;
}
// ---- (s289, the user's pick on recon/s289/S289_HELPER_COOPCHECK.md 3c) the Debug Menu's God Mode in a duel ----
// God Mode refills the life every frame (sora_dbgmenu.h dbm_god_tick_), so a duel hit (du_take_hit writes the life down) was put
// back at once and a God player only lost to one hit bigger than his whole life. Live "set duelgod 1" (default 1 since s290; 0 = as before):
// while this game's duel is live (du_sm_live: the countdown, the fight, and the time-up judge, whose HP % must be the real one)
// the refill waits; it is back by itself once the duel is over (forfeit, time, END, a load, Sora mode off, duel 0: at once; a KO:
// after the KO hold, sora_duel.h DU_KO_HOLD_MS, the loser's 2 s at 1 HP that ends with the duel's own full heal, so a God player's
// KO looks like anyone's). The God Mode row stays as the player set it (nothing changes in it, nothing is saved differently). The
// exe's hit drop stays on: duel hits are written into the life (du_life_set), not dealt through BotW's addDamage, so they count.
// key = duelgod, live = du_sm_live (sora_net_duel.h DUS.live), ko_hold = this game's KO hold runs (DUS.ko_hold). -> the refill waits
static inline bool du_god_waits(bool key, bool live, bool ko_hold) { return key && (live || ko_hold); }
// the pause's log edge, once a God Mode frame: *paused (false at start) follows wait. -> +1 = it pauses now, -1 = it is back now,
// 0 = no change
static inline int du_god_edge(bool *paused, bool wait) {
    if (wait == *paused) return 0;
    *paused = wait;
    return wait ? 1 : -1;
}

// his game's NF_DUEL: true = he is gone (no flag for DU_PEER_MS, after the grace from the countdown's start)
static inline bool du_peer_gone(uint64_t count0, uint64_t last_flag_ms, uint64_t now) {
    if (!count0 || now < count0 + DU_PEER_GRACE_MS) return false;
    if (last_flag_ms <= count0) return true;  // (never seen since the accept, the grace is over)
    return now >= last_flag_ms + DU_PEER_MS;
}

// ---- (s289) a HIT's src (the wire's u8, sora_net_proto.h): 0 blade, 1 Fire, 2 Blizzard, 3 Thunder, 4 Keyblade throw, 6 Magnet,
// 7 Reflect. Src numbers are reserved: 4 = Keyblade throw, 5 = BotW weapon throw, 6 = Magnet, 7 = Reflect (recon/s289/S289_HELPER_DUELPLAN.md).
// Any src is damage (net_unpack_duel does not check it); a game without its name logs "another attack" (an older build: "Thunder").
enum { DU_SRC_BLADE = 0, DU_SRC_FIRE, DU_SRC_BLIZZARD, DU_SRC_THUNDER, DU_SRC_KBTHROW, DU_SRC_MAGNET = 6, DU_SRC_REFLECT = 7 };
static inline const char *du_src_name(int src) {
    switch (src) {
    case DU_SRC_BLADE: return "blade";
    case DU_SRC_FIRE: return "Fire";
    case DU_SRC_BLIZZARD: return "Blizzard";
    case DU_SRC_THUNDER: return "Thunder";
    case DU_SRC_KBTHROW: return "Keyblade throw";
    case DU_SRC_MAGNET: return "Magnet";    // (s289 duelmagnet: the pull's one hit, sora_duel_magnet.h)
    case DU_SRC_REFLECT: return "Reflect";  // (s289 duelreflect: Reflect's burst, sora_duel_reflect.h)
    default: return "another attack";
    }
}
// the attacker's log word: "blade" / "spell" (Fire, Blizzard, Thunder) as before s289, the name for every other src
static inline const char *du_src_kind(int src) { return src >= DU_SRC_FIRE && src <= DU_SRC_THUNDER ? "spell" : du_src_name(src); }

// ---- the victim's hit de-dupe (UDP copies, a resend) ----
typedef struct { uint16_t s[32]; int n, i; } DuSeen;
static inline bool du_seen_add(DuSeen *d, uint16_t hseq) {
    for (int k = 0; k < d->n; k++) if (d->s[k] == hseq) return false;
    d->s[d->i] = hseq, d->i = (d->i + 1) % 32;
    if (d->n < 32) d->n++;
    return true;
}

// ---- the one-game pretend opponent (live duelfake 1): a still target, its own HP in quarter hearts ----
typedef struct { bool on; float feet[3]; int life, max; uint64_t next_hit; uint16_t hseq; } DuFake;
#define DU_FAKE_LIFE 48     // 12 hearts (GUESS: a mid-game save)
#define DU_FAKE_DIST 3.0f   // m in front of Sora when it starts

// ---- hit volume of a player body: KH2's Sora target part, a sphere 70 cm above the feet, radius 55 (re:0x3D7520, VERIFIED KH2,
// sora.c KH_SORA_PART_H / _R) as the cylinder the blade test takes: 55 cm round, 15..125 cm above the feet ----
#define DU_BODY_R_CM 55.0f
#define DU_BODY_Y0_CM 15.0f
#define DU_BODY_H_CM 110.0f
// a point (a spell's centre) against that body: horizontal reach r (BotW units), vertical band [lo, hi] above the feet (units)
static inline bool du_body_near(const float feet[3], const float p[3], float r, float lo, float hi) {
    float dx = p[0] - feet[0], dz = p[2] - feet[2], dy = p[1] - feet[1];
    return dx * dx + dz * dz <= r * r && dy >= lo && dy <= hi;
}
// ---- (s289 kbthrow) the thrown Keyblade (sora_kbthrow.h's base-form throw, flown by sora_limit.h lm_frame) against the opponent:
// lm_frame's enemy test as it is: his drawn feet within a flat r of the blade (the 160 cm the Raid uses on enemies; projectiles_kh2
// attack_set_radius is a GUESS source, see sora_limit.h; live duelthrowr) and the blade -50..220 cm above his feet. One hit on the
// way out and one on the way back; a hit on the way out turns the blade back, as at an enemy target (lm_frame does the turn).
// BotW units; to_u = 0.01 x g_k (KH cm -> units).
#define DU_THROW_R_CM 160
#define DU_THROW_DY_LO_CM (-50.0f)
#define DU_THROW_DY_HI_CM 220.0f
static inline bool du_throw_near(const float feet[3], const float blade[3], float r_cm, float to_u) {
    return du_body_near(feet, blade, r_cm * to_u, DU_THROW_DY_LO_CM * to_u, DU_THROW_DY_HI_CM * to_u);
}
enum { DUT_NONE = 0, DUT_OUT, DUT_BACK };  // du_throw_step: no hit / a hit on the way out (the blade turns back) / on the way back
// one frame of the flight (blade = where it is now; back = it flies home). *legs: bit 1 = the way out had its hit, bit 2 = the way back
static inline int du_throw_step(uint8_t *legs, bool back, const float feet[3], const float blade[3], float r_cm, float to_u) {
    uint8_t b = back ? 2 : 1;
    if (!legs || (*legs & b) || !du_throw_near(feet, blade, r_cm, to_u)) return DUT_NONE;
    *legs |= b;
    return back ? DUT_BACK : DUT_OUT;
}
// the HIT's Power: KH2's 200 (atkp 1866, Strike Raid's "Raid") x kbthrowdmg % (50 -> 100), rounded as lm_hit does, at least 1
static inline int du_throw_power(int power, int pct) {
    int p = (power * pct + 50) / 100;
    return p < 1 ? 1 : p;
}
// the asker's "Duel" prompt: Sora (at me, facing fw = {sin, cos}) looks at the friend within DU_FACE_DEG (GUESS), after standing
// still DU_IDLE_MS (GUESS): Triangle is BotW's A too (talk / pick up), so walking past a friend does not eat it
#define DU_FACE_DEG 60.0f
#define DU_IDLE_MS 750
static inline bool du_facing(const float me[3], const float fw[2], const float f[3]) {
    float dx = f[0] - me[0], dz = f[2] - me[2], d = hypotf(dx, dz);
    if (d < 0.5f) return true;  // (standing on him: any facing)
    return (dx * fw[0] + dz * fw[1]) / d >= cosf(DU_FACE_DEG * 3.14159265f / 180.0f);
}
// the front half plane test of a hit at "from" on a body at p facing yaw (0 = +z): KH2's front / back flinch
static inline bool du_from_front(const float p[3], float yaw, const float from[3]) {
    return (from[0] - p[0]) * sinf(yaw) + (from[2] - p[2]) * cosf(yaw) >= 0;
}

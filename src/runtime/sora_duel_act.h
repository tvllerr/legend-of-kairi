// (s289 lab hooks, duel plan step 0: recon/s289/S289_HELPER_DUELPLAN.md) live "duel act <verb>": this copy does an EXISTING move
// with no pad and no window focus, started the way the pad starts it, so an unfocused copy can act in the two-copy lab.
// TEST-ONLY: nothing happens unless the line is sent, and only with duel 1. The pure part (the verbs, the line, the pending
// act's clock) is host-tested by runtime/test/duel_test.c; the game side is sora_duel.h du_act_* (the three injection points:
// du_cast_gate after menu_tick, du_input before rc_tick, du_act_stick where Melty's pilot stick goes in sora.c).
//
// The verbs (DU_ACT_VERBS) and how each one drives the game:
//   reflect / magnet  DUAK_CAST   the spell as the command menu hands it over (du_cast_gate): the menu's rules first (Roxas /
//                                 Mickey's pane have no Magic; greyed = MP Charge / 0 MP / a form with no Magic / not owned,
//                                 with the menu's error sound; the menu's short lock after a command, M.dead, is waited out),
//                                 then sora.c's own cast start (combo gate, air, take-off) and the MP paid at the cast frame.
//   throw             DUAK_HOLD   Triangle + R1 held (kbthrow's start, sora_kbthrow.h) until the aim starts, then R1 let go =
//                                 the throw at the aim's end (MP paid there). No Triangle PRESS edge: no board / pick-up /
//                                 Reaction Command from it.
//   guard             DUAK_PRESS  one Square press, the stick neutral for that frame (sora_guard.h guard_press).
//   roll              DUAK_PRESS  one Square press, the stick pushed along Sora's facing for that frame (sora.c's Dodge Roll; a
//                                 Quick Run where the game gives one instead).
// One "[duel] act <verb>: ..." line per act: "started ..." or "refused: <why>". A press / hold waits for Sora free on the ground
// (DU_ACT_WAIT_MS); a newer act replaces a waiting one (that one logs "refused: replaced by ..."); an act nothing took in
// DU_ACT_EXPIRE_MS (no driven frame: a load, the title, a menu, BotW's own state) is refused.
// To add a verb (later helpers): a DUACT_* before DUACT_N + its row in DU_ACT_VERBS (name, kind), then its cases in sora_duel.h:
// a cast = du_act_spell (+ du_act_grey_why); a press = du_act_press_why + du_act_check_press (and the stick in du_act_input /
// du_act_stick); a hold = the buttons + the started test in du_act_input's DUAK_HOLD branch (+ du_act_hold_why).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum { DUACT_NONE = 0, DUACT_REFLECT, DUACT_MAGNET, DUACT_THROW, DUACT_GUARD, DUACT_ROLL, DUACT_N };
enum { DUAK_NONE = 0, DUAK_CAST, DUAK_PRESS, DUAK_HOLD };
typedef struct {
    const char *name;
    int kind;
} DuActVerb;
static const DuActVerb DU_ACT_VERBS[DUACT_N] = {
    [DUACT_NONE] = {"", DUAK_NONE},
    [DUACT_REFLECT] = {"reflect", DUAK_CAST},
    [DUACT_MAGNET] = {"magnet", DUAK_CAST},
    [DUACT_THROW] = {"throw", DUAK_HOLD},
    [DUACT_GUARD] = {"guard", DUAK_PRESS},
    [DUACT_ROLL] = {"roll", DUAK_PRESS},
};
#define DU_ACT_USAGE "reflect | magnet | throw | guard | roll"
#define DU_ACT_WAIT_MS 3000     // a press / hold waits this long for Sora free on the ground (then: refused, why)
#define DU_ACT_EXPIRE_MS 10000  // an act nothing took (no driven frame) is refused after this long

static inline const char *du_act_name(int verb) { return verb > DUACT_NONE && verb < DUACT_N ? DU_ACT_VERBS[verb].name : "?"; }
static inline int du_act_kind(int verb) { return verb > DUACT_NONE && verb < DUACT_N ? DU_ACT_VERBS[verb].kind : DUAK_NONE; }
// a verb's number from its name (n chars), DUACT_NONE = no such verb (exact, lower case: as the other live lines)
static inline int du_act_find(const char *s, size_t n) {
    for (int v = DUACT_NONE + 1; v < DUACT_N; v++)
        if (strlen(DU_ACT_VERBS[v].name) == n && !strncmp(DU_ACT_VERBS[v].name, s, n)) return v;
    return DUACT_NONE;
}
static inline bool du_act_sp(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
// the text after "duel ": "act <verb>" (one verb, spaces around it are fine) -> DUAL_*; *verb set for DUAL_OK
enum { DUAL_NO = 0, DUAL_OK, DUAL_EMPTY, DUAL_BAD };
static inline int du_act_line(const char *a, int *verb) {
    *verb = DUACT_NONE;
    if (!a || strncmp(a, "act", 3) || (a[3] && !du_act_sp(a[3]))) return DUAL_NO;  // ("actor": not ours)
    const char *p = a + 3;
    while (du_act_sp(*p)) p++;
    if (!*p) return DUAL_EMPTY;
    const char *w = p;
    while (*p && !du_act_sp(*p)) p++;
    size_t n = (size_t)(p - w);
    while (du_act_sp(*p)) p++;
    if (*p) return DUAL_BAD;  // (a second word: no verb takes one yet)
    *verb = du_act_find(w, n);
    return *verb ? DUAL_OK : DUAL_BAD;
}

// the pending act
enum { DUAP_IDLE = 0, DUAP_WAIT, DUAP_CHECK, DUAP_HOLD };  // waiting for a frame to act on | acted: judged next driven frame | holding
typedef struct {
    int verb, ph;
    uint64_t t0;      // the live line's time (ms)
    uint64_t act_ms;  // when it acted / began holding (ms)
    int spell;        // a cast: the spell
    int stick;        // the press frame's stick: 0 none, 1 neutral (guard), 2 along the facing (roll); used once by du_act_stick
    char why[128];    // DUAP_CHECK: what may stop it, judged when it acted (the line if it did not start)
} DuAct;
// a new act: the old waiting one (if any) is returned for its "replaced" line
static inline int du_act_queue(DuAct *a, int verb, uint64_t now) {
    int old = a->ph ? a->verb : DUACT_NONE;
    memset(a, 0, sizeof *a);
    a->verb = verb, a->ph = DUAP_WAIT, a->t0 = now ? now : 1, a->spell = -1;
    return old;
}
static inline bool du_act_late(const DuAct *a, uint64_t now, uint64_t since, uint64_t ms) { return a->ph && now > since + ms; }

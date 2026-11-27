// (s289 duelmenu) The command menu's "Duel" list: the pure part (no game state), host-tested by runtime/test/duelmenu_test.c.
// The game side is runtime/sora_duelmenu.h (page 2's slot 2 = KH2's Party slot, the list page, the pick). Plan: recon/s289/
// S289_HELPER_DUELPLAN.md 2.5 (the user's route (a)). The rows follow this game's duel phase (sora_duel_core.h DU_*):
//   idle / over     "Ask <name>" per friend in the room. Greyed with HIS reason as "<name>: too far" (the Triangle prompt's own rule,
//                   sora_duel.h du_input: drawn here within the near distance flat and less than DUM_DY above / below; no pose = another
//                   map / faded out), "<name>: in a duel" (his NF_DUEL), "<name>: away" (NF_DEAD / NF_LOADING). With OUR gate's reason
//                   (du_gate strict: an enemy near, in the air, 5 s after a duel, ...) every Ask row is greyed and one more greyed row
//                   at the end says why ("Enemy near"). Over = "Wait 5 s" (du_ask refuses while the duel is not idle).
//   asking          "Cancel" (we asked: the same END cancel as the live "duel end")
//   asked           "Accept" (greyed + the reason row when our strict gate says no: du_accept would refuse) / "Decline"
//   countdown / fight / time up   "Forfeit" (du_end lose, "left the ring", as the live "duel end")
// A greyed row's pick = the error SE, nothing is called. The reason row's act is DUM_NONE.
// GUESS (KH2 has no duel; all of it is ours): the texts, the order (net slot order, the pretend friend last), DUM_NAME_MAX.
#pragma once
#include "sora_duel_core.h"

#define DUM_MAX 8         // rows at most (the exe's sub page draws 8)
#define DUM_TEXT 40       // a row's text buffer
#define DUM_NAME_MAX 10   // a friend's name in a row is cut to this many characters (GUESS; s289 measure of camp_seq.bin's sys font at 0.8:
                          //  "Ask copy2" 57.8 px, "Ask Wwwwwwwwww" 120.4, "copy2: too far" 86.4; a live Slate row, "Remote Bomb Cube", 110.2)
#define DUM_DY 3.0f       // the Triangle prompt's height band (sora_duel.h du_input: |his feet - ours| < 3 m)
#define DUM_SETTLE_MS 400 // (game side) a press this soon after the list's phase changed under it is not a pick (GUESS)

enum { DUM_NONE = 0, DUM_ASK, DUM_ACCEPT, DUM_DECLINE, DUM_CANCEL, DUM_FORFEIT, DUM_NACT };
static const char *const DUM_ACT_NAME[DUM_NACT] = {"none", "Ask", "Accept", "Decline", "Cancel", "Forfeit"};
// a row's why: DUG_OK, a DUG_* of our gate, or one of these two about him
enum { DUMW_DUEL = DUG_N, DUMW_AWAY, DUMW_N };

typedef struct {
    int k;               // his net slot (or the pretend friend's DU_FAKE_K): the Ask row's argument
    const char *name;    // NULL / "" = "friend"
    bool pose;           // drawn here (a valid pose; false = another map / faded out)
    bool in_duel, away;  // his NF_DUEL / NF_DEAD or NF_LOADING
    float dist, dy;      // flat distance to us (m), his feet's height minus ours (m)
} DuMenuFriend;
typedef struct {
    char text[DUM_TEXT];
    int act;             // DUM_*: the call a pick makes (DUM_NONE = the reason row)
    int k;               // the Ask row's friend (-1 on the other rows)
    int why;             // DUG_OK, or why it is greyed (DUG_* / DUMW_*)
    bool grey;
} DuMenuRow;

// the reason row's text (our gate) / the words after "<name>: " (his side: too far, in a duel, away)
static inline const char *dum_why_short(int why) {
    switch (why) {
    case DUG_OK: return "";
    case DUG_OFF: return "Duels are off";
    case DUG_NOT_DRIVEN: return "Not in the world";
    case DUG_BUSY: return "Wait 5 s";  // (over: du_ask refuses until idle; the 5 s cool runs from the end)
    case DUG_COOL: return "Wait 5 s";
    case DUG_MENU: return "A menu is open";
    case DUG_FORM: return "In a Drive Form";
    case DUG_SUMMON: return "A summon is out";
    case DUG_BOARD: return "On the board";
    case DUG_AIR: return "In the air";
    case DUG_ENEMY: return "Enemy near";
    case DUG_CHASE: return "Enemy after you";
    case DUG_HIT: return "Hit just now";
    case DUG_FAR: return "too far";
    case DUMW_DUEL: return "in a duel";
    case DUMW_AWAY: return "away";
    default: return "not now";
    }
}
// the log's words for a why
static inline const char *dum_why_long(int why) {
    if (why >= 0 && why < DUG_N) return DUG_WHY[why];
    return why == DUMW_DUEL ? "he is in a duel" : why == DUMW_AWAY ? "he is away (dead / loading)" : "?";
}

// the Triangle prompt's distance rule, as du_input has it: d <= near (flat), |dy| < DUM_DY, a pose drawn here
static inline bool dum_friend_near(const DuMenuFriend *f, float near_m) {
    return f->pose && isfinite(f->dist) && isfinite(f->dy) && f->dist >= 0 && f->dist <= near_m && fabsf(f->dy) < DUM_DY;
}
// his side's reason, first one wins: away, in a duel, too far (the prompt skips all three)
static inline int dum_friend_why(const DuMenuFriend *f, float near_m) {
    if (f->away) return DUMW_AWAY;
    if (f->in_duel) return DUMW_DUEL;
    if (!dum_friend_near(f, near_m)) return DUG_FAR;
    return DUG_OK;
}
static inline void dum_name(char *o, int cap, const char *name) {
    const char *s = name && name[0] ? name : "friend";
    int n = 0;
    for (; s[n] && n < DUM_NAME_MAX && n < cap - 1; n++) o[n] = (unsigned char)s[n] < 0x20 ? '?' : s[n];
    o[n] = 0;
}
static inline int dum_row_put(DuMenuRow *out, int n, int cap, int act, int k, int why, const char *txt) {
    if (n >= cap) return n;
    DuMenuRow *r = &out[n];
    memset(r, 0, sizeof *r);
    snprintf(r->text, sizeof r->text, "%s", txt ? txt : "");
    r->act = act, r->k = k, r->why = why, r->grey = why != DUG_OK;
    return n + 1;
}

// does page 2 show "Duel" in slot 2: the key on, duels on, and a friend in the room (or our duel past idle: its Cancel / Accept /
// Forfeit stay reachable when he drops out of the room list)
static inline bool dum_show(bool key, bool duel_on, int phase, int nfriends) {
    if (!key || !duel_on) return false;
    return nfriends > 0 || phase == DU_ASKING || phase == DU_ASKED || phase == DU_COUNT || phase == DU_FIGHT || phase == DU_TIMEUP;
}

// the list's rows. phase = DU.sm.phase; gate = our du_gate (strict, with the list itself treated as closed: it closes before the
// call); f / nf = the friends in the room; near_m = the prompt's distance (live duelnear, default DU_NEAR_M). -> the row count
static inline int du_menu_rows(int phase, int gate, const DuMenuFriend *f, int nf, float near_m, DuMenuRow *out, int cap) {
    if (!out || cap <= 0) return 0;
    if (cap > DUM_MAX) cap = DUM_MAX;
    if (!f || nf < 0) nf = 0;
    int n = 0;
    switch (phase) {
    case DU_ASKING: return dum_row_put(out, n, cap, DUM_CANCEL, -1, DUG_OK, "Cancel");
    case DU_ASKED:
        n = dum_row_put(out, n, cap, DUM_ACCEPT, -1, gate, "Accept");
        n = dum_row_put(out, n, cap, DUM_DECLINE, -1, DUG_OK, "Decline");
        if (gate != DUG_OK) n = dum_row_put(out, n, cap, DUM_NONE, -1, gate, dum_why_short(gate));
        return n;
    case DU_COUNT:
    case DU_FIGHT:
    case DU_TIMEUP: return dum_row_put(out, n, cap, DUM_FORFEIT, -1, DUG_OK, "Forfeit");
    case DU_IDLE:
    case DU_OVER: break;
    default: return 0;
    }
    if (nf == 0) return 0;
    if (phase == DU_OVER && gate == DUG_OK) gate = DUG_BUSY;  // (du_ask refuses while the duel is not idle)
    int room = gate != DUG_OK && cap > 1 ? cap - 1 : cap;    // (the reason row keeps its place)
    for (int i = 0; i < nf && n < room; i++) {
        int his = dum_friend_why(&f[i], near_m);
        char nm[DUM_NAME_MAX + 1], t[DUM_TEXT];
        dum_name(nm, (int)sizeof nm, f[i].name);
        if (his != DUG_OK) snprintf(t, sizeof t, "%s: %s", nm, dum_why_short(his));
        else snprintf(t, sizeof t, "Ask %s", nm);
        n = dum_row_put(out, n, cap, DUM_ASK, f[i].k, his != DUG_OK ? his : gate, t);
    }
    if (gate != DUG_OK) n = dum_row_put(out, n, cap, DUM_NONE, -1, gate, dum_why_short(gate));
    return n;
}
// the Ask row of friend k in rows (-1 = none): the pick's check again on the next frame
static inline int dum_find_ask(const DuMenuRow *r, int n, int k) {
    for (int i = 0; i < n; i++)
        if (r[i].act == DUM_ASK && r[i].k == k) return i;
    return -1;
}
// the list's own guard: a press within DUM_SETTLE_MS of its phase changing under it is not a pick (changed_at 0 = never)
static inline bool dum_settled(uint64_t changed_at, uint64_t now) { return !changed_at || now >= changed_at + DUM_SETTLE_MS; }

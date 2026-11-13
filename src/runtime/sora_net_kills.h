// s196 killsync: "kills stay dead after a reload" (live `set killsync 0|1`, compiled default 1 since s197: user verified; it was 0
//   in s196). DLL only, no exe change. Proof it is on with no live key: one "[kills] kill sync on (the compiled default ...)" line
//   every time net turns on (net_set_on -> net_kills_on_log), right after the "[net] on:" line.
//   Two games kill a group of placed enemies; one then loads a save (or its Sora dies and it Continues) and had them all back
//   while the other did not. Now every game keeps a KILL LIST: the placed enemies (map HashId, unique per map object and the same
//   in every copy: recon/s194/S194_SPEC43_FACTS.md 1) it saw die since its last Blood Moon, each with the local TIME it died:
//     - its own kills (the exe's EDMG ring, drained by net_edmg_pump: entries with kill + hid; also while alone, when they are
//       not sent), the kills other games sent (NET_EDMG with NEDF_KILL; only with `set edmg 1`, as their apply), and the
//       kills that came in a catch-up (below, with the age the answerer sent: kill time = now - age).
//     - (s197 review S2) own and EDMG kills only while THIS game is in the open world (sora_world_open() == 1; not known = not
//       kept), an EDMG kill not from a peer on another map (sora_net_area.h hidden), and never a Stal (`_Bone`) enemy: ks_name_ok
//       (BotW brings those back every night / every Trial of the Sword run / every DLC fight, not only at a Blood Moon).
//   CATCH-UP: a game that loads a save (sora_levels.h: the exe pending map is reset, then this list is cleared, then it asks)
//     or sees a new peer slot (a join, or our own fresh start after a hot reload / net 0 -> 1: the slots come back) ASKS each
//     peer for his list (NET_KILLS, sora_net_proto.h). Both sides of a join ask each other, so both end with the union.
//     (s197 review M1) Over plain UDP every game has its OWN Blood Moons. So the ask says how long ago MY last Blood Moon was
//     and the answer carries every kill's age: he sends only kills younger than my moon (ks_cut; one of "not known" age only to
//     an asker who saw no moon), and I drop again on receive what is not younger than my moon (an answer he started before it).
//     A kill handed on keeps its true age (game C hears B's catch-up kills from A with A's age + the transit, never "now").
//     My "last moon" is only known after a Blood Moon in this DLL session: a fresh start, a join and a DLL hot reload (statics
//     wiped: dev only, accepted) ask with "none seen" = they get everything their peers have.
//     A Melty room shares one Blood Moon (the room clock), but a guest's can be "owed" (in a shrine: sora_clock.h): its count only
//     goes up when it plays here, and that is when the list is cleared and the moon time set (net_kills_tick, the same place), so
//     the same age rule covers the window in both directions.
//   APPLY ("die as they load in", the user's pick): the exe's pending map holds only 256 entries (it evicts the oldest kill), so
//     the list is NOT dumped into it. Every KS_SCAN_MS on a driven frame in the open world (s197 S2) the DLL reads which enemies
//     are loaded here and alive (sora.c sora_kills_loaded: the exe's linkpos_enemies list + each actor's HashId at actor+0x7c8 ->
//     +0x58, life > 0, ks_name_ok) and hands each one on the list to linkpos_edmg_apply(hid, 0, 0, 0, 0, 1) (the shared-HP export:
//     a kill rides that enemy's next damage call; the game's own death runs). At most once per KS_REAPPLY_MS per enemy.
//   CLEARED by: a Blood Moon in THIS game (sora_world_bloodmoon_count + 1, the count the room clock uses; s197 S1: the exe's pending
//     map of remote hits / kills is reset with it through its linkpos_edmg_reset export, as at a save load), my own save load
//     (then asked again), `set killsclear 1` (also the exe reset), `set killsync 0`. Nothing else forgets a kill (the list is
//     append-only between clears: an answer's parts are slices of it).
//   (s197 review S3 / S4) a peer gets at most one NEW answer snapshot per KS_ANS_NEW_MS (his re-ask for missing parts of the
//     current one is always served) and the "asks for our kill list" line is rate limited; an ask takes at most
//     KS_ASK_PARTS_MAX parts, KS_ASK_RESTARTS_MAX snapshot restarts and KS_ASK_NEW_MAX new kills; the table probe is capped
//     (KS_PROBE_MAX). Full / capped = new kills refused, the enemies stay alive (the safe side).
//   live: `set killsync 0|1` (default 1 since s197; 0 = nothing recorded, asked, answered or applied; nothing sent: the wire is as before),
//         `set killsclear 1` (ONE-SHOT, every time the line runs: forget the list and every catch-up in flight; remove the line
//         again), `set killslog 0|1` (a line per recorded / applied kill and per part); one "[kills]" line in `net diag`.
//   linkpos_edmg_apply -1 (the exe has SAVAGEWIND_LINK_EDMG off) = logged once, then idle: nothing applied, nothing asked (the
//   list is still kept and handed to peers that ask).
// Two parts: CORE (pure: the list, the asker / answerer state; host-tested by runtime/test/kills_test.c) and GLUE
// (#ifdef NET_KILLS_GLUE, included by sora_net.h after the edmg glue: NET, NETED, net_send_to, LOG; tested by net_glue_test.c /
// net_melty_test.c). Numbers marked GUESS were not measured in game.
#pragma once
#include "sora_net_proto.h"

// ===================== CORE =====================
#define KS_CAP 6784             // (s197: 64 parts x 106; it was 8192 with 160-hid parts) kills on the list. GUESS it fits: the s166
                                // table has 6300 placed enemy / animal rows (MainField + shrines, DLC not counted) and the list only
                                // holds kills since the last Blood Moon. Full = a NEW hid is refused (counted, logged once per clear);
                                // the ones on the list stay. An answer is <= 64 parts.
#define KS_HASH 16384           // open addressing table (2^14, load <= 0.42)
#define KS_PROBE_MAX 64         // (s197 S4) GUESS: a hid more than this many slots from its home slot is refused (crafted collisions
                                // cannot make an add / find walk the table; at load 0.42 an honest run that long is practically never)
#define KS_SCAN_MS 100          // GUESS: how often the loaded enemies are checked against the list (driven frames only)
#define KS_REAPPLY_MS 3000      // GUESS: an enemy on the list seen alive again this long after its last apply gets it again
                                // (the exe's ride window is 120 game frames, ~2 s: then the kill goes back to its pending map)
#define KS_REASK_MS 1500        // GUESS: UDP: an ask with parts still missing is sent again this often (with the parts we have)
#define KS_REASK_MELTY_MS 5000  // GUESS: Melty room (reliable): only a late safety (an ask that reached him before his slot for us)
#define KS_ASK_TIMEOUT_MS 20000 // GUESS: a peer that has not answered every part by then (an old build: never) is given up (logged)
#define KS_TX_PER_TICK 4        // GUESS: answer parts sent per net tick, all peers together (~650 B each)
#define KS_MOON_MS 500          // the Blood Moon count is read this often
#define KS_MOON_SLACK_MS 2000   // (s197 M1) GUESS: a kill must be this much younger than the moon to cross it (covers the transit
                                // of an answer and the ms lost to whole-second ages; the safe side: a kill right after a moon is lost)
#define KS_ANS_NEW_MS 2000      // (s197 S3) GUESS: at most one NEW answer snapshot per peer this often (his 1.5 s / 5 s re-ask
                                // gets a refused one later)
#define KS_ASK_RESTARTS_MAX 1   // (s197 S4) GUESS: an ask follows at most this many snapshot changes of the answerer (a clear there)
#define KS_ASK_PARTS_MAX (2 * NET_KILLS_CHUNKS_MAX)  // (s197 S4) GUESS: parts one ask takes, all snapshots together
#define KS_ASK_NEW_MAX (KS_CAP / 2)                  // (s197 S4) GUESS: new kills one ask may add (one answer never fills the list)
#define NET_KILLS_DEFAULT true  // s197: killsync's compiled default (g_net_kills, GLUE), user verified; a DLL hot reload starts here again
#define KS_KT_OLD INT64_MIN     // a kill time: not known / 18.2 h or older (sent as NET_KILLS_AGE_OLD)
#define KS_CUT_ALL INT64_MIN    // a cut: every kill passes, the KS_KT_OLD ones too (the asker saw no Blood Moon)
_Static_assert(KS_CAP <= NET_KILLS_CHUNK * NET_KILLS_CHUNKS_MAX, "a full list fits one answer");
_Static_assert(KS_CAP < 65536 && KS_HASH >= 2 * KS_CAP && (KS_HASH & (KS_HASH - 1)) == 0, "index / table sizes");
_Static_assert(NET_KILLS_PKT <= 657 && NET_KILLS_PKT <= NET_PKT_MAX, "a full part fits the s196 budget, a packet and the Melty connector's 1024");

enum { KSRC_OWN = 0, KSRC_EDMG = 1, KSRC_CATCH = 2, KSRC_N = 3 };
static const char *const KSRC_TXT[KSRC_N] = {"own kill", "EDMG kill", "catch-up"};
typedef struct {
    uint32_t tab[KS_HASH];    // hid, 0 = empty
    uint16_t at[KS_HASH];     // its index in hid[]
    uint32_t hid[KS_CAP];     // in the order they came (an answer's parts are slices of it)
    uint64_t applied[KS_CAP]; // last apply here (ms; own kills: when seen), 0 = never = WAITING (its enemy not seen alive here yet)
    int64_t kt[KS_CAP];       // (s197 M1) when it died, local ms (KS_KT_OLD = not known / very old); a duplicate keeps the newest
    uint8_t src[KS_CAP];
    int n, nwait;             // nwait = entries with applied 0
    uint32_t epoch;           // +1 at every clear (an answer in flight from an older epoch is dropped)
    uint32_t nsrc[KSRC_N], full, crowd;  // (crowd: refused by KS_PROBE_MAX)
} KsList;
enum { KSA_BAD = -2, KSA_FULL = -1, KSA_DUP = 0, KSA_NEW = 1 };
// The ONE name rule (own kills, sora.c sora_kills_loaded = what the apply scan offers): only placed ENEMIES are kept dead.
//   Animal_* (deer, boars, wolves, bears...) also go through the exe's enemy hooks, but BotW brings them back on its own (GUESS: area
//   reloads, not Blood Moons), so a kept kill would kill every new one (and drop its meat again).
//   (s197 S2) Stal enemies ("_Bone": Enemy_Bokoblin_Bone_Junior, Enemy_Giant_Bone...) come back every night: never kept / applied.
static inline bool ks_name_ok(const char *name) { return name && !strncmp(name, "Enemy_", 6) && !strstr(name, "_Bone"); }

// ---- (s197 M1) kill times <-> wire ages ----
// a kill time -> its age now in whole seconds (rounded down), NET_KILLS_AGE_OLD when not known or >= 65535 s
static inline uint16_t ks_age_s(int64_t kt, uint64_t now) {
    if (kt == KS_KT_OLD) return NET_KILLS_AGE_OLD;
    int64_t d = (int64_t)now - kt;
    if (d <= 0) return 0;
    return d >= (int64_t)NET_KILLS_AGE_OLD * 1000 ? NET_KILLS_AGE_OLD : (uint16_t)(d / 1000);
}
// a received age -> a local kill time, rounded to the OLDER end (+999 ms: the sender rounded down), so a kill never gets younger
// on its way except by the transit (KS_MOON_SLACK_MS covers that)
static inline int64_t ks_kt_from_age(uint16_t age, uint64_t now) {
    return age == NET_KILLS_AGE_OLD ? KS_KT_OLD : (int64_t)now - (int64_t)age * 1000 - 999;
}
// what an ask says: seconds since my last Blood Moon (rounded down = the safe side), saturating at 0xFFFE; none seen = NONE
static inline uint16_t ks_moon_s(bool seen, uint64_t moon_t, uint64_t now) {
    if (!seen) return NET_KILLS_MOON_NONE;
    uint64_t s = (now > moon_t ? now - moon_t : 0) / 1000;
    return s > NET_KILLS_MOON_NONE - 1 ? NET_KILLS_MOON_NONE - 1 : (uint16_t)s;
}
// the answerer: an ask's moon -> the kill time a kill must be newer than (KS_CUT_ALL for NONE). The moon of the ask is a little
// older than it says by the time it is answered (transit, rounding down): the cut only moves to the safe side.
static inline int64_t ks_cut(uint16_t moon_s, uint64_t now) {
    return moon_s == NET_KILLS_MOON_NONE ? KS_CUT_ALL : (int64_t)now - (int64_t)moon_s * 1000 + KS_MOON_SLACK_MS;
}
// the asker's own cut for what arrives
static inline int64_t ks_my_cut(bool seen, uint64_t moon_t) { return seen ? (int64_t)moon_t + KS_MOON_SLACK_MS : KS_CUT_ALL; }
// a kill made after the cut (a KS_KT_OLD one only passes KS_CUT_ALL)
static inline bool ks_kt_after(int64_t kt, int64_t cut) { return cut == KS_CUT_ALL || (kt != KS_KT_OLD && kt > cut); }

static inline uint32_t ks_slot0(uint32_t hid) { return (hid * 2654435761u) >> 18; }  // (top 14 bits)
static inline int ks_find(const KsList *L, uint32_t hid) {
    if (!hid) return -1;
    for (uint32_t s = ks_slot0(hid), c = 0; c < KS_PROBE_MAX; c++, s = (s + 1) & (KS_HASH - 1)) {
        if (!L->tab[s]) return -1;
        if (L->tab[s] == hid) return L->at[s];
    }
    return -1;
}
static inline void ks_mark(KsList *L, int i, uint64_t now) {
    if (i < 0 || i >= L->n) return;
    if (!L->applied[i]) L->nwait--;
    L->applied[i] = now ? now : 1;
}
// applied: 0 = waiting; else the ms it was applied / seen dead here. kt = when it died (KS_KT_OLD = not known). A duplicate keeps
// its first source and the NEWEST kill time; a waiting one takes `applied`.
static inline int ks_add(KsList *L, uint32_t hid, int src, uint64_t applied, int64_t kt) {
    if (!hid || src < 0 || src >= KSRC_N) return KSA_BAD;
    uint32_t s = ks_slot0(hid), c = 0;
    for (; c < KS_PROBE_MAX && L->tab[s] && L->tab[s] != hid; c++) s = (s + 1) & (KS_HASH - 1);
    if (c == KS_PROBE_MAX) { L->crowd++; return KSA_FULL; }  // (s197 S4: only crafted / very unlucky hids get here)
    if (L->tab[s]) {
        int i = L->at[s];
        if (applied && !L->applied[i]) ks_mark(L, i, applied);
        if (kt > L->kt[i]) L->kt[i] = kt;
        return KSA_DUP;
    }
    if (L->n >= KS_CAP) { L->full++; return KSA_FULL; }
    int i = L->n++;
    L->tab[s] = hid, L->at[s] = (uint16_t)i;
    L->hid[i] = hid, L->src[i] = (uint8_t)src, L->applied[i] = applied, L->kt[i] = kt;
    L->nwait += !applied, L->nsrc[src]++;
    return KSA_NEW;
}
static inline void ks_clear(KsList *L) {
    uint32_t e = L->epoch + 1;
    memset(L, 0, sizeof *L);
    L->epoch = e;
}
// which of the loaded-and-alive hids here get the kill now (their list index into out[]): on the list, not applied within KS_REAPPLY_MS
static inline int ks_pick(const KsList *L, const uint32_t *loaded, int nl, uint64_t now, int *out, int max) {
    int k = 0;
    for (int j = 0; j < nl && k < max; j++) {
        int i = ks_find(L, loaded[j]);
        if (i < 0) continue;
        bool dup = false;
        for (int q = 0; q < k; q++) dup = dup || out[q] == i;  // (the same hid twice in one enum)
        if (dup) continue;
        if (!L->applied[i] || now < L->applied[i] || now - L->applied[i] >= KS_REAPPLY_MS) out[k++] = i;
    }
    return k;
}

// ---- the asker (one per peer slot) ----
typedef struct {
    bool active, done;        // asking now / every part of the answer is in
    bool capped, cap_said;    // (s197 S4) a cap was hit: the ask is over (the parts / kills taken stay)
    uint32_t nonce;           // this ask's (0 = never asked); parts with another nonce are stale
    uint16_t tag; uint8_t n;  // the answer snapshot we collect (0 = none yet) and its part count
    uint8_t restarts;         // (s197 S4) snapshot changes followed
    uint16_t parts_in;        // (s197 S4) parts taken, all snapshots
    uint64_t got;             // parts in
    uint64_t t0, next;        // started / next (re-)ask due
    uint32_t tries, hids, fresh, old, refused;  // (old: older than my Blood Moon, dropped; refused: past KS_ASK_NEW_MAX)
    uint32_t newk;            // new kills this ask added (KS_ASK_NEW_MAX)
} KsAsk;
static inline void ks_ask_start(KsAsk *a, uint32_t nonce, uint64_t now) {
    memset(a, 0, sizeof *a);
    a->active = true, a->nonce = nonce ? nonce : 1, a->t0 = now, a->next = now;
}
enum { KSD_NONE = 0, KSD_SEND, KSD_TIMEOUT };
// once a tick: SEND = an ask is due now (counted, the next one scheduled); TIMEOUT = given up now (once)
static inline int ks_ask_step(KsAsk *a, uint64_t now, bool melty) {
    if (!a->active || a->done) return KSD_NONE;
    if (now >= a->t0 && now - a->t0 >= KS_ASK_TIMEOUT_MS) { a->active = false; return KSD_TIMEOUT; }
    if (now < a->next) return KSD_NONE;
    a->tries++, a->next = now + (melty ? KS_REASK_MELTY_MS : KS_REASK_MS);
    return KSD_SEND;
}
static inline uint64_t ks_ask_have(const KsAsk *a) { return a->tag ? a->got : 0; }
enum { KSP_TAKE = 0, KSP_DONE, KSP_DUP, KSP_STALE, KSP_BAD, KSP_CAP };
// a PART for this asker: TAKE / DONE (its hids are new to this ask: take them; DONE = it was the last one missing), DUP (that part
// is already here), STALE (another / an abandoned ask: before a load, after killsclear, a timed-out one), BAD (numbers that do not fit),
// CAP (s197 S4: more snapshot restarts / parts than one honest answer needs: the ask ends here, what came stays)
static inline int ks_ask_part(KsAsk *a, uint32_t nonce, uint16_t tag, int i, int n) {
    if (!a->nonce || nonce != a->nonce || (!a->active && !a->done)) return KSP_STALE;
    if (!tag || n < 1 || n > NET_KILLS_CHUNKS_MAX || i < 0 || i >= n) return KSP_BAD;
    if (tag != a->tag) {
        if (a->done) return KSP_STALE;   // (done: a later snapshot of his adds only kills EDMG already brought)
        if (a->tag && a->restarts >= KS_ASK_RESTARTS_MAX) { a->active = false, a->capped = true; return KSP_CAP; }
        a->restarts += a->tag != 0;
        a->tag = tag, a->n = (uint8_t)n, a->got = 0;  // (his list changed under the answer (a clear there): start over on the new one)
    } else if (n != a->n) return KSP_BAD;
    uint64_t bit = 1ull << i;
    if (a->got & bit) return KSP_DUP;
    if (a->parts_in >= KS_ASK_PARTS_MAX) { a->active = false, a->capped = true; return KSP_CAP; }
    a->got |= bit, a->parts_in++;
    if (__builtin_popcountll(a->got) == n) { a->done = true, a->active = false; return KSP_DONE; }
    return KSP_TAKE;
}
// the kills of a TAKE / DONE part onto the list: those not newer than my cut (ks_my_cut: my last Blood Moon) are dropped (*old),
// a new one past KS_ASK_NEW_MAX for this ask is refused (a->capped), a full list refuses (*full). -> how many were new to the list.
static inline int ks_take(KsList *L, KsAsk *a, const uint32_t *hid, const uint16_t *age, int k, uint64_t now, int64_t cut, int *old, int *full) {
    int fresh = 0, o = 0, f = 0;
    for (int j = 0; j < k; j++) {
        int64_t kt = ks_kt_from_age(age[j], now);
        if (!ks_kt_after(kt, cut)) { o++; continue; }
        if (a->newk >= KS_ASK_NEW_MAX && ks_find(L, hid[j]) < 0) { a->capped = true, a->refused++; continue; }
        int r = ks_add(L, hid[j], KSRC_CATCH, 0, kt);
        if (r == KSA_NEW) fresh++, a->newk++;
        else if (r == KSA_FULL) f++;
    }
    a->old += (uint32_t)o;
    if (old) *old = o;
    if (full) *full = f;
    return fresh;
}

// ---- the answerer (one per peer slot) ----
typedef struct {
    bool active;              // parts still to send
    uint32_t nonce, epoch;    // the ask and our list epoch the snapshot is of
    uint16_t tag; int count; uint8_t n;  // the snapshot: its tag (never 0), the first `count` hids of our list, in n parts
    uint64_t todo;            // parts still to send
    int64_t cut;              // (s197 M1) only kills newer than this go (ks_cut of his moon; KS_CUT_ALL = all)
    int sendable;             // (log) kills of the snapshot past the cut when it was taken
    uint64_t t_new;           // (s197 S3) when the last new snapshot was taken (0 = never)
} KsAns;
static inline int ks_parts(int count) { return count <= 0 ? 1 : (count + NET_KILLS_CHUNK - 1) / NET_KILLS_CHUNK; }
static inline uint64_t ks_all(int n) { return n >= 64 ? ~0ull : (1ull << n) - 1; }
enum { KSAN_SAME = 0, KSAN_NEW = 1, KSAN_LATER = 2 };
// an ASK from this peer: a new ask (or our list was cleared since) = a new snapshot, every part (NEW; LATER = refused: one was taken
// less than KS_ANS_NEW_MS ago, nothing changes, he asks again); the same ask again = the parts he says he lacks (SAME; its cut moves
// to the newer of the two, never back). moon = his ask's (seconds since his last Blood Moon). *tagc = our snapshot counter (never 0).
static inline int ks_ans_ask(KsAns *s, uint32_t nonce, uint16_t have_tag, uint64_t have, uint16_t moon, const KsList *L, uint16_t *tagc, uint64_t now) {
    bool fresh = !s->tag || nonce != s->nonce || L->epoch != s->epoch;
    int64_t cut = ks_cut(moon, now);
    if (fresh) {
        if (s->t_new && now >= s->t_new && now - s->t_new < KS_ANS_NEW_MS) return KSAN_LATER;
        if (!++*tagc) ++*tagc;
        s->nonce = nonce, s->epoch = L->epoch, s->count = L->n, s->n = (uint8_t)ks_parts(L->n), s->tag = *tagc, s->cut = cut;
        s->t_new = now ? now : 1, s->sendable = 0;
        for (int i = 0; i < L->n; i++) s->sendable += ks_kt_after(L->kt[i], cut);
    } else if (cut != KS_CUT_ALL && (s->cut == KS_CUT_ALL || cut > s->cut)) s->cut = cut;  // (a later cut only: never back to older kills)
    s->todo = ks_all(s->n) & ~(have_tag == s->tag ? have : 0);
    s->active = s->todo != 0;
    return fresh ? KSAN_NEW : KSAN_SAME;
}
// the next part to send: 1 = *i / hid[0..*k) with age[], the kills of slice *i newer than the cut (0..NET_KILLS_CHUNK of them: a
// part can be empty, it still goes so the bitmap fills), 0 = nothing (done, or our list was cleared under it: dropped, *aborted = 1)
static inline int ks_ans_next(KsAns *s, const KsList *L, uint64_t now, int *i, uint32_t *hid, uint16_t *age, int *k, int *aborted) {
    if (aborted) *aborted = 0;
    if (!s->active) return 0;
    if (L->epoch != s->epoch || s->count > L->n) {
        s->active = false, s->todo = 0;
        if (aborted) *aborted = 1;
        return 0;
    }
    if (!s->todo) { s->active = false; return 0; }
    int c = __builtin_ctzll(s->todo);
    s->todo &= s->todo - 1;
    int a = c * NET_KILLS_CHUNK, b = a + NET_KILLS_CHUNK < s->count ? a + NET_KILLS_CHUNK : s->count, m = 0;
    for (int j = a; j < b; j++)
        if (ks_kt_after(L->kt[j], s->cut)) hid[m] = L->hid[j], age[m++] = ks_age_s(L->kt[j], now);
    *k = m, *i = c;
    if (!s->todo) s->active = false;
    return 1;
}
static inline uint32_t ks_nonce(uint64_t now, uint32_t ctr) {
    uint32_t x = (uint32_t)(now * 2654435761u) ^ (ctr * 0x85EBCA6Bu) ^ (uint32_t)(now >> 32);
    x ^= x >> 16, x *= 0x7feb352du, x ^= x >> 15;
    return x ? x : 1;
}

#ifdef NET_KILLS_GLUE
// ===================== GLUE (needs NET, NETED (net_edmg_bind / apply / reset), net_send_to, LOG from sora_net.h) =====================
// live "set killsync 0|1" (0: nothing recorded, asked, answered or applied; the wire as before). Starting ON needs nothing more than
// what `set killsync 1` sets up: NETK.idle false and joined[] zero are NETK's zero state (a fresh / hot-reloaded DLL),
// and net_kills_reset (every net 0 -> 1) zeroes joined[] again.
static bool g_net_kills = NET_KILLS_DEFAULT;
static bool g_net_kills_log;  // live "set killslog 1"

static struct {
    KsList L;
    KsAsk ask[NET_MAX_PEERS];
    KsAns ans[NET_MAX_PEERS];
    uint64_t joined[NET_MAX_PEERS];  // the slots' joined_ms + 1 as last seen (0 = no slot): a change = a new peer = ask him
    uint16_t seq, tagc;
    uint32_t nctr;
    bool moons_known, want_all, idle, bound, full_said, reset_said;
    bool moon_seen;                  // (s197 M1) a Blood Moon played in this game in this DLL session: moon_t = when we saw it
    uint64_t moon_t;
    int moons, rr;
    uint64_t next_moon, next_scan, last_now;  // (last_now: the last tick's clock, for `net diag`)
    NetLogRate ask_log;              // (s197 S3) the "asks for our kill list" lines
    int (*loaded)(uint32_t *hids, int max);  // the placed enemies loaded here and alive (sora.c sora_kills_loaded); NULL = no apply
    int (*moons_fn)(void);                   // sora_world_bloodmoon_count (-1 = not known yet); NULL = no Blood Moon clear
    int (*open_fn)(void);                    // (s197 S2) sora_world_open (1 = open world, 0 = not, -1 = not known); NULL = never open
    struct { uint32_t own, edmg, catchup, full, ask_tx, ask_done, ask_timeout, ask_rx, ans_new, ans_later, part_tx, ans_abort, part_rx, part_dup, part_stale,
             part_bad, part_cap, rx_bad, rx_off, hid_rx, hid_new, hid_old, applied, ap_queued, ap_pending, ap_ignored, scans, clr_moon, clr_load, clr_key,
             not_open, exe_reset; } st;
} NETK;

static void net_kills_bind(void) {
    if (NETK.bound) return;
    NETK.bound = true;
#ifdef SORA_KILLS_HOOKS  // (sora.c: defined after sora_airhold.h, which reads the actor HashId)
    NETK.loaded = sora_kills_loaded;
#endif
#ifdef SORA_WORLD_HOOKS  // (sora.c: sora_world.h)
    NETK.moons_fn = sora_world_bloodmoon_count;
    NETK.open_fn = sora_world_open;
#endif
}
// (s197 S2) this game is in the open world now (the overworld: world::Manager stage type 1); not known = no
static bool net_kills_open(void) { return NETK.open_fn && NETK.open_fn() == 1; }

static void net_kills_stop_asks(void) {
    for (int k = 0; k < NET_MAX_PEERS; k++) NETK.ask[k].active = false, NETK.ans[k].active = false;
}
// forget the list (and every catch-up in flight: their late parts are stale)
static void net_kills_forget(const char *why) {
    if (NETK.L.n || NETK.L.full) LOG("[kills] %s: forgot %d kill(s) (%d waiting for their enemy to load here)\n", why, NETK.L.n, NETK.L.nwait);
    else LOG("[kills] %s: the list was empty\n", why);
    ks_clear(&NETK.L);
    NETK.full_said = false;
    net_kills_stop_asks();
}
// (s197 review S1) the exe's pending map (remote hits / kills waiting for their enemy, no time limit for kills) belongs to the old
// world too: its own export, the one sora_levels.h calls at a save load (that is why the exe's line says "save loaded")
static void net_kills_exe_reset(const char *why) {
    net_edmg_bind();
    if (!NETED.reset) {
        if (!NETK.reset_said) NETK.reset_said = true, LOG("[kills] %s: this exe has no linkpos_edmg_reset: remote kills parked in it stay (old exe)\n", why);
        return;
    }
    int kills = 0, n = NETED.reset(&kills);
    NETK.st.exe_reset++;
    LOG("[kills] %s: the exe's waiting remote hits were dropped too (%d, %d of them kills; its \"[linkpos] edmg reset (save loaded)\" line is this)\n", why, n, kills);
}

// net on (net_set_on): no ask / answer of an earlier session, every slot is new; the list itself stays (it is this world's)
static void net_kills_reset(uint64_t now) {
    net_kills_stop_asks();
    memset(NETK.ask, 0, sizeof NETK.ask), memset(NETK.ans, 0, sizeof NETK.ans), memset(NETK.joined, 0, sizeof NETK.joined);
    NETK.seq = (uint16_t)(now * 2246822519u >> 11), NETK.rr = 0;
}
// (s197) net on (net_set_on, after its "[net] on:" line): ONE line saying whether kill sync runs, so a log shows the compiled
// default took with no live key (a `set killsync 1` line is a no-op now and logs nothing)
static void net_kills_on_log(void) {
    if (g_net_kills == NET_KILLS_DEFAULT)
        LOG("[kills] kill sync %s (the compiled default since s197, no live key needed; set killsync %d to change it)\n", g_net_kills ? "on" : "off", !g_net_kills);
    else LOG("[kills] kill sync %s (set killsync %d: not the compiled default)\n", g_net_kills ? "on" : "OFF", (int)g_net_kills);
}

// a kill this game saw: its own (the EDMG ring) or an EDMG kill from a peer, at kill time kt. applied 0 = waiting for its enemy here.
static int net_kills_note(uint32_t hid, int src, uint64_t applied, int64_t kt) {
    if (!g_net_kills || !hid) return KSA_BAD;
    int r = ks_add(&NETK.L, hid, src, applied, kt);
    if (r == KSA_NEW) {
        if (src == KSRC_OWN) NETK.st.own++;
        else if (src == KSRC_EDMG) NETK.st.edmg++;
        else NETK.st.catchup++;
        if (g_net_kills_log && src != KSRC_CATCH) LOG("[kills] + hid=%08x (%s): %d on the list\n", hid, KSRC_TXT[src], NETK.L.n);
    } else if (r == KSA_FULL) {
        NETK.st.full++;
        if (!NETK.full_said) NETK.full_said = true, LOG("[kills] the list is full (%d): hid=%08x and later new kills are NOT kept (the old ones stay)\n", KS_CAP, hid);
    }
    return r;
}

// net_edmg_pump: one of OUR kills (also while alone: not sent, still kept); seen dead here = not waiting
static void net_kills_own(uint32_t hid, const char *name, uint64_t now) {
    // (s209 step 2) Melty's BEAT note "kill": every enemy this game killed (detail: its actor name), kept on the list or not
    if (NET.melty && hid) sora_melty_addon_beat("kill", 0, name && *name ? name : "enemy");
    if (!g_net_kills || !hid) return;
    if (!ks_name_ok(name)) {
        if (g_net_kills_log) LOG("[kills] hid=%08x %s: not a kept enemy (Enemy_*, no Stal _Bone): not kept\n", hid, name ? name : "?");
        return;
    }
    if (!net_kills_open()) {  // (s197 S2: a shrine, the Trial of the Sword, a DLC fight, the title screen: BotW brings those back)
        NETK.st.not_open++;
        if (g_net_kills_log) LOG("[kills] hid=%08x %s: not in the open world here: not kept\n", hid, name ? name : "?");
        return;
    }
    net_kills_note(hid, KSRC_OWN, now, (int64_t)now);
}
// net_edmg_recv: slot k's EDMG kill, after its apply here: queued (1) / ignored = dead here already (0) = done here; pending (2) /
// the exe without the export or with shared HP off (-1) = waiting for its enemy to show up alive here. (s197 S2) only while we are
// in the open world and he is not on another map (no name on the wire: a Stal is never offered by the apply scan, ks_name_ok)
static void net_kills_edmg(int k, uint32_t hid, int apply_ret, uint64_t now) {
    if (!g_net_kills || !hid) return;
    if (!net_kills_open() || (k >= 0 && k < NET_MAX_PEERS && NET.slot[k].hidden)) { NETK.st.not_open++; return; }
    net_kills_note(hid, KSRC_EDMG, apply_ret == 1 || apply_ret == 0 ? now : 0, (int64_t)now);
}

// sora_levels.h: BotW read a game_data.sav (Continue / Load). The exe's pending map was just reset; our list goes too, then we ask.
static void net_kills_on_load(int slot) {
    if (!g_net_kills) return;
    NETK.st.clr_load++;
    char why[64];
    snprintf(why, sizeof why, "save loaded (slot %d)", slot);
    net_kills_forget(why);
    NETK.want_all = true;  // (asked at the next net tick, every peer slot; a peer that comes later asks / is asked on its join)
}

static void net_kills_ask_start(int k, uint64_t now, const char *why) {
    ks_ask_start(&NETK.ask[k], ks_nonce(now, ++NETK.nctr), now);
    LOG("[kills] asking peer %d \"%s\" for his kill list (%s, nonce %08x)\n", k + 1, NET.slot[k].p.name, why, NETK.ask[k].nonce);
}

// a slot that appeared (or was taken by someone else) since we last looked = a new peer: his old ask / answer state goes, he is asked.
// Run from the tick AND before a packet of his is handled (his ask may come in the very receive that made his slot: it must not be
// wiped by the tick's join check after it).
static void net_kills_join_check(int k, uint64_t now) {
    uint64_t j = NET.slot[k].used ? NET.slot[k].joined_ms + 1 : 0;  // (+1: a slot joined at ms 0 is still a slot)
    if (j == NETK.joined[k]) return;
    NETK.joined[k] = j;
    memset(&NETK.ask[k], 0, sizeof NETK.ask[k]), memset(&NETK.ans[k], 0, sizeof NETK.ans[k]);
    if (j && !NETK.idle && !g_net_loop) net_kills_ask_start(k, now, "join");
}

static void net_kills_scan(uint64_t now) {
    uint32_t h[64];
    int n = NETK.loaded(h, 64), pick[64];
    NETK.st.scans++;
    int c = ks_pick(&NETK.L, h, n < 0 ? 0 : n > 64 ? 64 : n, now, pick, 64);
    for (int j = 0; j < c; j++) {
        int i = pick[j];
        uint32_t hid = NETK.L.hid[i];
        int r = NETED.apply(hid, 0, 0, 0, 0, 1);
        if (r < 0) {
            NETK.idle = true;
            LOG("[kills] linkpos_edmg_apply says the exe has shared HP off (SAVAGEWIND_LINK_EDMG): kill sync is idle (nothing applied or asked; "
                "the list is still kept and given to peers that ask)\n");
            return;
        }
        bool wait = !NETK.L.applied[i];
        ks_mark(&NETK.L, i, now);
        NETK.st.applied++;
        if (r == 1) NETK.st.ap_queued++;
        else if (r == 2) NETK.st.ap_pending++;
        else NETK.st.ap_ignored++;
        if (g_net_kills_log || wait)
            LOG("[kills] hid=%08x (%s) is loaded here and alive: kill applied -> %s%s\n", hid, KSRC_TXT[NETK.L.src[i]],
                r == 1 ? "queued (it dies at its next damage call)" : r == 2 ? "pending (not in the exe's list any more)" : "ignored (dead / deleting)",
                wait ? "" : " (again: it was still alive)");
    }
}

// every net_tick (after the receive): the Blood Moon check, new peers, asks + answers out, the apply scan (link_pos NULL = not driven)
static void net_kills_tick(const float *link_pos, uint64_t now) {
    if (!g_net_kills) return;
    net_kills_bind();
    net_edmg_bind();
    NETK.last_now = now;
    if (NETK.moons_fn && now >= NETK.next_moon) {
        NETK.next_moon = now + KS_MOON_MS;
        int m = NETK.moons_fn();
        if (m >= 0) {
            if (NETK.moons_known && m > NETK.moons) {
                // (s197 M1) THE place a Blood Moon reaches the kill list: the list goes and "my last moon" = now, together (the moon
                // time can only be later than the real one, by up to KS_MOON_MS + the count's own delay = the safe side)
                NETK.st.clr_moon++;
                char why[64];
                snprintf(why, sizeof why, "Blood Moon in this game (count %d -> %d)", NETK.moons, m);
                net_kills_forget(why);
                NETK.moon_seen = true, NETK.moon_t = now;
                net_kills_exe_reset(why);  // (s197 S1)
            }
            NETK.moons = m, NETK.moons_known = true;  // (the first count seen is adopted: a game start, a hot reload)
        }
    }
    if (g_net_loop) return;  // (loopback: slot 0 is ourselves, nobody to ask)
    for (int k = 0; k < NET_MAX_PEERS; k++) net_kills_join_check(k, now);
    if (NETK.want_all) {
        NETK.want_all = false;
        for (int k = 0; k < NET_MAX_PEERS; k++) if (NET.slot[k].used && !NETK.idle) net_kills_ask_start(k, now, "our save was loaded");
    }
    if (!(NET.melty && !sora_melty_connected())) {  // (a room event while not connected would be lost: wait)
        uint16_t moon = ks_moon_s(NETK.moon_seen, NETK.moon_t, now);
        for (int k = 0; k < NET_MAX_PEERS; k++) {
            NetPeerSlot *sl = &NET.slot[k];
            if (!sl->used) continue;
            KsAsk *a = &NETK.ask[k];
            int d = ks_ask_step(a, now, NET.melty);
            if (d == KSD_TIMEOUT) {
                NETK.st.ask_timeout++;
                LOG("[kills] peer %d \"%s\" did not answer our ask in %d s (%d of %d part(s), %u ask(s)): an old build? Kept what came (%u kills)\n", k + 1,
                    sl->p.name, KS_ASK_TIMEOUT_MS / 1000, __builtin_popcountll(a->got), a->n, a->tries, a->hids);
            } else if (d == KSD_SEND) {
                uint8_t b[NET_KILLS_ASK_LEN];
                int len = net_pack_kills_ask(NET.me, ++NETK.seq, a->nonce, a->tag, ks_ask_have(a), moon, b, (int)sizeof b);
                if (len > 0) net_send_to(sl->ip, sl->port, b, len), NETK.st.ask_tx++;
            }
        }
        for (int sent = 0, tries = 0; sent < KS_TX_PER_TICK && tries < NET_MAX_PEERS; tries++) {
            int k = NETK.rr;
            NETK.rr = (NETK.rr + 1) % NET_MAX_PEERS;
            NetPeerSlot *sl = &NET.slot[k];
            if (!sl->used) { NETK.ans[k].active = false; continue; }
            int i, cnt, ab;
            static uint32_t h[NET_KILLS_CHUNK];
            static uint16_t ag[NET_KILLS_CHUNK];
            while (sent < KS_TX_PER_TICK && ks_ans_next(&NETK.ans[k], &NETK.L, now, &i, h, ag, &cnt, &ab)) {
                uint8_t b[NET_KILLS_PKT];
                int len = net_pack_kills_part(NET.me, ++NETK.seq, NETK.ans[k].nonce, NETK.ans[k].tag, i, NETK.ans[k].n, h, ag, cnt, b, (int)sizeof b);
                if (len <= 0) { NET.st.err++; continue; }
                net_send_to(sl->ip, sl->port, b, len), NETK.st.part_tx++, sent++;
                if (g_net_kills_log) LOG("[kills] -> peer %d part %d/%d (%d kills)\n", k + 1, i + 1, NETK.ans[k].n, cnt);
            }
            if (ab) NETK.st.ans_abort++;
            if (NETK.ans[k].active) NETK.rr = k;  // (budget used up: this peer goes on first next tick)
        }
    }
    if (link_pos && !NETK.idle && NETK.L.n && NETK.loaded && NETED.apply && now >= NETK.next_scan && net_kills_open()) {  // (s197 S2)
        NETK.next_scan = now + KS_SCAN_MS;
        net_kills_scan(now);
    }
}

// a NET_KILLS packet from slot k (header already checked by net_unpack_ex)
static void net_kills_recv(int k, const uint8_t *buf, int n, uint64_t now) {
    static NetKillsMsg m;  // (1.1 KB: not on the receive loop's stack)
    uint8_t pl = 0;
    uint16_t sq = 0;
    if (!net_unpack_kills(buf, n, &pl, &sq, &m)) { NETK.st.rx_bad++; return; }
    if (!g_net_kills) { NETK.st.rx_off++; return; }  // (killsync 0: no answer, nothing taken)
    if (!g_net_loop) net_kills_join_check(k, now);
    NetPeerSlot *sl = &NET.slot[k];
    if (m.op == NKO_ASK) {
        NETK.st.ask_rx++;
        if (g_net_loop) return;
        int r = ks_ans_ask(&NETK.ans[k], m.nonce, m.tag, m.have, m.moon, &NETK.L, &NETK.tagc, now);
        if (r == KSAN_LATER) { NETK.st.ans_later++; return; }  // (s197 S3: no line; `net diag` counts them)
        if (r == KSAN_NEW) {
            NETK.st.ans_new++;
            bool first_quiet = false;
            const KsAns *s = &NETK.ans[k];
            char mo[48];
            if (m.moon == NET_KILLS_MOON_NONE) snprintf(mo, sizeof mo, "he saw no Blood Moon");
            else snprintf(mo, sizeof mo, "his last Blood Moon %u s ago", (unsigned)m.moon);
            if (net_lograte_ok(&NETK.ask_log, now, &first_quiet))
                LOG("[kills] peer %d \"%s\" asks for our kill list (%s): %d of our %d kill(s) are after it, in %d part(s)\n", k + 1, sl->p.name, mo,
                    s->sendable, s->count, s->n);
            else if (first_quiet) LOG("[kills] (more kill-list asks in the next %d s are not logged: `net diag` counts them)\n", NET_LOGRATE_MS / 1000);
        }
        return;
    }
    NETK.st.part_rx++;
    KsAsk *a = &NETK.ask[k];
    int r = ks_ask_part(a, m.nonce, m.tag, m.i, m.n);
    if (r == KSP_STALE) {
        NETK.st.part_stale++;
        if (g_net_kills_log) LOG("[kills] <- peer %d part %d/%d: stale (an ask from before a load / clear / timeout), dropped\n", k + 1, m.i + 1, m.n);
        return;
    }
    if (r == KSP_DUP) { NETK.st.part_dup++; return; }
    if (r == KSP_BAD) { NETK.st.part_bad++; return; }
    if (r == KSP_CAP) {
        NETK.st.part_cap++;
        if (!a->cap_said) a->cap_said = true, LOG("[kills] peer %d \"%s\": his answer changed / grew past what one answer can be (%u parts, %d restart(s)): "
                                                    "ask ended, kept the %u kill(s) that came\n", k + 1, sl->p.name, (unsigned)a->parts_in, a->restarts, a->hids);
        return;
    }
    int old = 0, full = 0, fresh = ks_take(&NETK.L, a, m.hid, m.age, m.k, now, ks_my_cut(NETK.moon_seen, NETK.moon_t), &old, &full);
    NETK.st.catchup += (uint32_t)fresh, NETK.st.full += (uint32_t)full, NETK.st.hid_old += (uint32_t)old;
    if (full && !NETK.full_said) NETK.full_said = true, LOG("[kills] the list is full (%d): later new kills are NOT kept (the old ones stay)\n", KS_CAP);
    if (a->capped && !a->cap_said) a->cap_said = true, LOG("[kills] peer %d \"%s\": more than %d new kills in one answer: the rest is not taken\n", k + 1, sl->p.name, KS_ASK_NEW_MAX);
    a->hids += m.k, a->fresh += (uint32_t)fresh, NETK.st.hid_rx += m.k, NETK.st.hid_new += (uint32_t)fresh;
    if (g_net_kills_log) LOG("[kills] <- peer %d part %d/%d: %d kills, %d new, %d from before our last Blood Moon\n", k + 1, m.i + 1, m.n, m.k, fresh, old);
    if (r == KSP_DONE) {
        NETK.st.ask_done++;
        LOG("[kills] catch-up from peer %d \"%s\": %u kill(s) in %d part(s), %u new to us, %u from before our last Blood Moon (dropped), %llu ms (%u ask(s)); "
            "list %d, %d waiting for their enemy here\n", k + 1, sl->p.name, a->hids, a->n, a->fresh, a->old, (unsigned long long)(now >= a->t0 ? now - a->t0 : 0),
            a->tries, NETK.L.n, NETK.L.nwait);
    }
}

// live keys (net_live_set): true = handled
static bool net_kills_live_set(const char *key, int v) {
    if (!strcmp(key, "killsync")) {
        if (g_net_kills == (v != 0)) return true;
        g_net_kills = v != 0;
        if (g_net_kills) {
            // (s197 M1) the Blood Moon count stays known: one that played while it was off is seen at the next read (a clear of
            // the empty list + "my last moon" = then: late = the safe side); a count never read yet is adopted as before
            NETK.idle = false;                                    // (the exe asked again)
            memset(NETK.joined, 0, sizeof NETK.joined);           // (every slot there now counts as new: asked at the next tick)
            LOG("[kills] kill sync on: placed enemies killed since the last Blood Moon die as they load in after a reload / for a joining game\n");
        } else {
            net_kills_forget("kill sync off");
            LOG("[kills] kill sync off (set killsync 1): nothing recorded, asked, answered or applied\n");
        }
    } else if (!strcmp(key, "killsclear")) {  // (ONE-SHOT every time the line runs: remove it from the live file again)
        if (!v) return true;
        NETK.st.clr_key++;
        net_kills_forget("set killsclear");
        net_kills_exe_reset("set killsclear");  // (s197 S1)
    } else if (!strcmp(key, "killslog")) {
        g_net_kills_log = v != 0;
        LOG("[kills] log %s\n", g_net_kills_log ? "on" : "off");
    } else return false;
    return true;
}

static void net_kills_diag(void) {
    int asking = 0, ans = 0;
    for (int k = 0; k < NET_MAX_PEERS; k++) asking += NETK.ask[k].active, ans += NETK.ans[k].active;
    char mo[40];
    if (NETK.moon_seen) snprintf(mo, sizeof mo, "last Blood Moon %u s ago", (unsigned)ks_moon_s(true, NETK.moon_t, NETK.last_now));
    else snprintf(mo, sizeof mo, "no Blood Moon seen");
    LOG("[kills] (%s%s): list %d (own %u, EDMG %u, catch-up %u; full refused %u, crowded %u), waiting %d, %s, not in the open world %u; asks sent %u "
        "(done %u, timed out %u, asking now %d), parts in %u (new kills %u of %u, before our Blood Moon %u, dup %u, stale %u, bad %u, capped %u); asks from "
        "peers %u (new %u, too soon %u (log quiet %u), answering now %d), parts sent %u (aborted by a clear %u); applied %u (queued %u, pending %u, "
        "ignored %u; %u scans); clears: Blood Moon %u, load %u, key %u (exe reset %u); bad packets %u, off %u\n",
        g_net_kills ? "on" : "OFF (set killsync 1)", NETK.idle ? ", IDLE: exe shared HP off" : NETED.apply || !NETED.bound ? "" : ", NO EXE APPLY",
        NETK.L.n, NETK.L.nsrc[KSRC_OWN], NETK.L.nsrc[KSRC_EDMG], NETK.L.nsrc[KSRC_CATCH], NETK.L.full, NETK.L.crowd, NETK.L.nwait, mo, NETK.st.not_open,
        NETK.st.ask_tx, NETK.st.ask_done, NETK.st.ask_timeout, asking, NETK.st.part_rx, NETK.st.hid_new, NETK.st.hid_rx, NETK.st.hid_old, NETK.st.part_dup,
        NETK.st.part_stale, NETK.st.part_bad, NETK.st.part_cap, NETK.st.ask_rx, NETK.st.ans_new, NETK.st.ans_later, NETK.ask_log.quiet, ans, NETK.st.part_tx,
        NETK.st.ans_abort, NETK.st.applied, NETK.st.ap_queued, NETK.st.ap_pending, NETK.st.ap_ignored, NETK.st.scans, NETK.st.clr_moon, NETK.st.clr_load,
        NETK.st.clr_key, NETK.st.exe_reset, NETK.st.rx_bad, NETK.st.rx_off);
}
#endif

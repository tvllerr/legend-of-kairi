// host tests for runtime/sora_dbgsave.h (dbgsave #60, s288 port): the sidecar's "dbgmenu" line, written and read back
// build: clang -O1 -g -Wall -Iruntime runtime/test/dbgsave_test.c -o dbgsave_test.exe
#include "sora_dbgsave.h"
#include "sora_pchar.h"  // (s289 fix3b: the Party rows' character ids, PCH_HOME / PCH_NONE)

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

// a sidecar as sl_sidecar_write lays it out, with the line in its place (between "summon" and "dream")
static void side_(char *b, size_t cap, const char *line) {
    snprintf(b, cap, "{\n  \"version\": 1,\n  \"exp\": 1234,\n  \"drive\": 300,\n  \"summon\": 0,\n%s  \"dream\":\"sword\",\n  \"saved\": \"x\"\n}\n", line);
}

static void test_round_trip(void) {
    int v[DBS_NROWS], w[DBS_NROWS], bad = -1;
    bool have[DBS_NROWS];
    for (int r = 0; r < DBS_NROWS; r++) v[r] = r * 3 - 7;  // negatives too (Party None = -1)
    char line[512], b[2048];
    int n = dbs_write(line, sizeof line, v);
    CHECK(n > 0 && n == (int)strlen(line));
    CHECK(!strncmp(line, "  \"dbgmenu\": \"lv=-7,kb=-4,", 26) && !strcmp(line + n - 3, "\",\n"));
    side_(b, sizeof b, line);
    CHECK(dbs_parse(b, w, have, &bad) == DBS_NROWS && bad == 0);
    for (int r = 0; r < DBS_NROWS; r++) CHECK(have[r] && w[r] == v[r]);
    // the other flat keys of the sidecar still read their own value (the line has no quoted name inside)
    CHECK(strstr(b, "\"drive\": 300") != NULL && strstr(line, "\"drive\"") == NULL);
}

static void test_too_small(void) {
    int v[DBS_NROWS] = {0};
    char line[40];
    memset(line, 'x', sizeof line);
    CHECK(dbs_write(line, sizeof line, v) == 0 && line[0] == 0);  // never half a line
    int big[DBS_NROWS];
    for (int r = 0; r < DBS_NROWS; r++) big[r] = -2147483647 - 1;
    char l2[512];
    int n = dbs_write(l2, sizeof l2, big);
    CHECK(n > 0 && n <= 362);  // the worst case fits the sidecar's 512 buffer
    printf("worst-case line: %d chars\n", n);
}

static void test_missing_and_broken(void) {
    int w[DBS_NROWS], bad;
    bool have[DBS_NROWS];
    char b[2048];
    side_(b, sizeof b, "");  // an older save: no key
    CHECK(dbs_parse(b, w, have, &bad) == -1);
    CHECK(dbs_parse(NULL, w, have, &bad) == -1);
    side_(b, sizeof b, "  \"dbgmenu\": 5,\n");  // not a string
    CHECK(dbs_parse(b, w, have, &bad) == -2);
    side_(b, sizeof b, "  \"dbgmenu\": \"lv=3,kb=1\n");  // no closing quote on its line (a cut / hand-edited file)
    CHECK(dbs_parse(b, w, have, &bad) == -2);
    for (int r = 0; r < DBS_NROWS; r++) CHECK(!have[r]);
    strcpy(b, "{ \"dbgmenu\": \"lv=3");  // the file ends inside the value
    CHECK(dbs_parse(b, w, have, &bad) == -2);
    side_(b, sizeof b, "  \"dbgmenu\": \"\",\n");  // empty: nothing read, nothing bad
    CHECK(dbs_parse(b, w, have, &bad) == 0 && bad == 0);
}

static void test_old_format_and_junk(void) {
    int w[DBS_NROWS], bad;
    bool have[DBS_NROWS];
    char b[2048];
    side_(b, sizeof b, "  \"dbgmenu\": \"0,1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,1,0\",\n");  // PR #60's positional format: nothing used
    CHECK(dbs_parse(b, w, have, &bad) == 0 && bad == 20);
    side_(b, sizeof b, "  \"dbgmenu\": \"lv=12,newrow=4,god=1,kb=,clip=1x,gyro=0,lv=13\",\n");  // a later DLL's row, junk, a repeat
    CHECK(dbs_parse(b, w, have, &bad) == 3 && bad == 3);
    CHECK(have[0] && w[0] == 13 && have[5] && w[5] == 1 && have[18] && w[18] == 0 && !have[1] && !have[6]);
    side_(b, sizeof b, "  \"dbgmenu\" :  \"mickey=2\",\n");  // spaces around ':' are fine (the glue clamps Mickey to 1)
    CHECK(dbs_parse(b, w, have, &bad) == 1 && have[14] && w[14] == 2);
}

// ---- (s289 fix3b) the Party rows under the co-op room's one-member rule (recon/s289/S289_HELPER_COOPCHECK.md 3b) ----
// A Party row's value as sora_dbgmenu.h dbm_save_val_ builds it: the row's value before s289 (dbm_state = PCH_NONE for a None slot,
// else the character in it, mapped by dbm_save_party_: PCH_NONE -> -1), then dbs_party_val with the rule's mark (g_coop_auto).
static int party_old_(int under, bool off) { int st = off ? PCH_NONE : under; return st == PCH_NONE ? -1 : st; }
static int party_val_(int under, bool off, bool rule) { return dbs_party_val(party_old_(under, off), rule, under); }
// the line's values for a Debug Menu state: every row Off (Gyro Aim On, its normal state) except the two Party rows
static void rows_(int v[DBS_NROWS], int p1, int p2) {
    for (int r = 0; r < DBS_NROWS; r++) v[r] = r == 18 ? 1 : 0;
    v[10] = p1, v[11] = p2;
}
// one place's state: the character the slot holds (g_pchar), None (g_pslot_off), the rule made that None (g_coop_auto)
typedef struct { int under; bool off, rule; } Place;
static const Place FRESH[2] = {{PCH_DONALD, false, false}, {PCH_GOOFY, false, false}};  // a new start: Donald + Goofy, no None
// a single-player Continue on the line, then the next dbm_coop_tick_: dbm_slot_load (out of a room the Party rows are not skipped)
// hands a row to dbm_set when the saved value differs from dbm_save_val_'s (cur; old_cur = as before s289); dbm_set's value < 0 =
// the player's OWN None (mark cleared: the room's end never gives it back), a character = the slot filled + him (mark cleared). A
// row not handed over keeps its state, and dbm_coop_tick_ (out of the room) gives a rule's None its member back.
// -> who stands in each place after both (PCH_NONE = None); refusals (a Drive Form, missing data) are not modelled
static void solo_continue_(const char *line, const Place st[2], bool old_cur, int out[2]) {
    char b[2048];
    int w[DBS_NROWS], bad;
    bool have[DBS_NROWS];
    side_(b, sizeof b, line);
    int n = dbs_parse(b, w, have, &bad);
    for (int s = 0; s < 2; s++) {
        const Place *p = &st[s];
        int cur = old_cur ? party_old_(p->under, p->off) : party_val_(p->under, p->off, p->rule);
        if (n > 0 && have[10 + s] && w[10 + s] != cur) out[s] = w[10 + s] < 0 ? PCH_NONE : w[10 + s];
        else out[s] = p->off && !p->rule ? PCH_NONE : p->under;
    }
}

static void test_party_coop(void) {
    const int D = PCH_HOME[0], G = PCH_HOME[1];
    int v[DBS_NROWS], o[DBS_NROWS], got[2];
    char line[512], old[512];
    CHECK(D == PCH_DONALD && G == PCH_GOOFY);
    // 1. no rule mark = the row's value, whatever it is (single player never sets the mark: its line is byte for byte the old one)
    for (int row = -1; row <= PCH_NONE; row++)
        for (int c = 0; c < PCH_N; c++) CHECK(dbs_party_val(row, false, c) == row);
    // ... and every single-player state of both rows (each character or None in each place) writes the old line exactly
    for (int a = 0; a <= PCH_NONE; a++)
        for (int g = 0; g <= PCH_NONE; g++) {
            int u0 = a == PCH_NONE ? D : a, u1 = g == PCH_NONE ? G : g;  // (a None slot keeps its own member under it)
            rows_(v, party_val_(u0, a == PCH_NONE, false), party_val_(u1, g == PCH_NONE, false));
            rows_(o, party_old_(u0, a == PCH_NONE), party_old_(u1, g == PCH_NONE));
            CHECK(dbs_write(line, sizeof line, v) > 0 && dbs_write(old, sizeof old, o) > 0 && !strcmp(line, old));
        }
    // 2. the player's OWN None (no mark) stays -1, in a room too
    CHECK(party_val_(G, true, false) == -1 && party_val_(D, true, false) == -1);
    // 3. the room's rule made Goofy's place None (keep 0, the start: pchar_coop_drop empties slot 1); Goofy waits under it
    rows_(v, party_val_(D, false, false), party_val_(G, true, true));
    CHECK(v[10] == D && v[11] == G);
    CHECK(dbs_write(line, sizeof line, v) > 0 && strstr(line, ",party1=0,party2=1,") != NULL);
    solo_continue_(line, FRESH, false, got);
    CHECK(got[0] == D && got[1] == G);  // single player has both
    // ... the line before the fix said party2=-1: the solo Continue ran it as his own None and Goofy was gone (the bug)
    rows_(o, party_old_(D, false), party_old_(G, true));
    CHECK(dbs_write(old, sizeof old, o) > 0 && strstr(old, ",party1=0,party2=-1,") != NULL);
    solo_continue_(old, FRESH, true, got);
    CHECK(got[0] == D && got[1] == PCH_NONE);
    // 4. Riku picked in Donald's place in the room, the rule's None in Goofy's: the room's end leaves Riku + Goofy, so does the line
    rows_(v, party_val_(PCH_RIKU, false, false), party_val_(G, true, true));
    CHECK(dbs_write(line, sizeof line, v) > 0 && strstr(line, ",party1=2,party2=1,") != NULL);
    solo_continue_(line, FRESH, false, got);
    CHECK(got[0] == PCH_RIKU && got[1] == G);
    // 5. Auron picked in Goofy's place: the rule's None goes to Donald's place (keep 1); Donald waits under it
    rows_(v, party_val_(D, true, true), party_val_(PCH_AURON, false, false));
    CHECK(dbs_write(line, sizeof line, v) > 0 && strstr(line, ",party1=0,party2=3,") != NULL);
    solo_continue_(line, FRESH, false, got);
    CHECK(got[0] == D && got[1] == PCH_AURON);
    // 6. both None in a room: the rule's in Donald's place, the player's own in Goofy's -> Donald back, Goofy's own None kept
    rows_(v, party_val_(D, true, true), party_val_(G, true, false));
    CHECK(dbs_write(line, sizeof line, v) > 0 && strstr(line, ",party1=0,party2=-1,") != NULL);
    solo_continue_(line, FRESH, false, got);
    CHECK(got[0] == D && got[1] == PCH_NONE);
    // ... the same session after the room (its end gave Donald back): the same result
    const Place after6[2] = {{D, false, false}, {G, true, false}};
    solo_continue_(line, after6, false, got);
    CHECK(got[0] == D && got[1] == PCH_NONE);
    // 7. the mark seen while the slot is filled again (a stale mark: never expected) still writes whoever stands there
    for (int c = 0; c < PCH_N; c++) CHECK(party_val_(c, false, true) == c);
    // 8. a Continue in single player while the mark is still set (the room just left, dbm_coop_tick_ not run yet; or the net not
    // up yet after a hot reload): the line just written loads as no change and the tick gives Goofy back ...
    const Place pend[2] = {{D, false, false}, {G, true, true}};
    rows_(v, party_val_(D, false, false), party_val_(G, true, true));
    CHECK(dbs_write(line, sizeof line, v) > 0);
    solo_continue_(line, pend, false, got);
    CHECK(got[0] == D && got[1] == G);
    // ... and an older line of this slot with the player's own None in Goofy's place is that None again (before s289 the -1 equalled
    // the rule's None as "no change", and the tick then gave Goofy back: the saved None was lost)
    rows_(o, party_val_(D, false, false), party_val_(G, true, false));
    CHECK(dbs_write(old, sizeof old, o) > 0 && strstr(old, ",party2=-1,") != NULL);
    solo_continue_(old, pend, false, got);
    CHECK(got[0] == D && got[1] == PCH_NONE);
    solo_continue_(old, pend, true, got);
    CHECK(got[0] == D && got[1] == G);
    // 9. a save while parked (Sora mode off in a room: dbm_coop_tick_ keeps the mark) or right after a hot reload (party_pick_restore
    // reads the mark back from PPICK_ENV " auto=", party_test.c s269 F3) has the same state, so the same value as case 3
    CHECK(party_val_(G, true, true) == G && party_val_(D, true, true) == D);
}

int main(void) {
    test_round_trip();
    test_too_small();
    test_missing_and_broken();
    test_old_format_and_junk();
    test_party_coop();
    printf("%s dbgsave_test (%d checks, %d failed)\n", fails ? "FAIL" : "PASS", runs, fails);
    return fails != 0;
}

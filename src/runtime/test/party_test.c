// party_test.c: (s217 riku; s220 members) host tests of the party swap's pure parts: sora_pchar.h (who may stand where, what needs
// whom, the Debug Menu rows' offer rules: no character twice, a missing bake is not offered) and sora_party.h's CORE
// (SORA_PARTY_CORE_ONLY): the stand-ins' KH2 rows (Riku, Auron, Mulan, Aladdin, Jack Sparrow), the per-character state swap in
// both slots (a stand-in in Donald's or Goofy's place and back = Donald / Goofy byte-identical), and the battle AI with the REAL
// bakes (read only): a stand-in only ever plays his A-row attacks (no spell of Donald's, no Goofy ability), his windows hit, and
// Donald + Goofy after all the swaps fight exactly as before.
// usage: party_test.exe [riku bake dir] [donald data dir] [members bake root] [cloud bake dir]
//   (defaults D:/SoraBotW/sb/s217/riku/bake2, D:/SoraBotW/data/party, D:/SoraBotW/sb/s220/members/bake: <root>/<key>/<key>_*.bin,
//   (s223) D:/SoraBotW/sb/s223/cloud/sbuild/out/party: cloud_*.bin)
// build (worktree root, UCRT64 clang): clang -O1 -g -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers
//        -Iruntime runtime/test/party_test.c -o <out>/party_test.exe
#include <stdarg.h>
#include <stdio.h>
static int g_verbose;
static void plog(const char *fmt, ...) {
    if (!g_verbose) return;
    va_list a;
    va_start(a, fmt);
    vprintf(fmt, a);
    va_end(a);
}
#define PARTY_LOG plog
#define SORA_PARTY_CORE_ONLY
#include "sora_party.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

// ---- a flat world (y 0), one enemy, the hooks
static bool ray(const float a[3], const float b[3], float h[3], float n[3]) {
    if (a[1] < 0 || b[1] > 0) return false;
    h[0] = a[0], h[1] = 0, h[2] = a[2], n[0] = n[2] = 0, n[1] = 1;
    return true;
}
static PEnemy g_en[4];
static int g_nen, g_hits[PARTY_N], g_spells[PARTY_N], g_sounds[PARTY_N];
static int dmg_cb(int member, uint64_t actor, int32_t life, int power, int magic) {
    (void)member, (void)actor, (void)life, (void)magic;
    int d = (int)lroundf(4.0f * (float)power / 100.0f);
    return d < 1 ? 1 : d;
}
static void hit_cb(int member, uint64_t actor, int dmg, int f54, int f40, const float at[3], const char *fx, int atkp) {
    (void)actor, (void)dmg, (void)f54, (void)f40, (void)at, (void)fx, (void)atkp;
    g_hits[member]++;  // (the enemy keeps its life: the fight goes on for the whole run)
}
static int heal_cb(int member) { (void)member; return 12; }
static int fx_cb(const char *name, const float pos[3], float yaw) { (void)name, (void)pos, (void)yaw; return 1; }
static void fx_move_cb(int h, const float pos[3], float yaw) { (void)h, (void)pos, (void)yaw; }
static void fx_end_cb(int h, float fade) { (void)h, (void)fade; }
static void weapon_cb(int member, bool shown) { (void)member, (void)shown; }
static void sound_cb(int member, int ev, int arg, const float at[3]) { (void)ev, (void)arg, (void)at; g_sounds[member]++; }
static const PHooks HK = {dmg_cb, hit_cb, heal_cb, fx_cb, fx_move_cb, fx_end_cb, weapon_cb, sound_cb};

// one scripted fight: Sora stands at the origin facing +z, an enemy 3 m ahead; n presents of 2 KH frames. Records per slot which
// action entries started (by index) and every spell it released.
typedef struct { int attacks, spells, hops, starts[16], hits, spell_frames, sounds; float pos[3]; float a1_clock; } Run;  // (s218: + attack1's longest clock)
static uint32_t g_run_rnd;
static void fight(Run r[PARTY_N], int n) {
    memset(r, 0, sizeof(Run) * PARTY_N);
    memset(g_hits, 0, sizeof g_hits), memset(g_spells, 0, sizeof g_spells), memset(g_sounds, 0, sizeof g_sounds);
    g_nen = 1;
    g_en[0] = (PEnemy){0xE1, {0, 0, 3.0f}, 100};
    for (int p = 1; p <= n; p++) {
        PIn in = {{0, 0, 0}, 0, true, 2.0f, 0.01f, (uint64_t)p, ray, g_en, g_nen, 0.5f, 1.6f, 12, 12, &HK};
        party_core_tick(&in);
        for (int i = 0; i < PARTY_N; i++) {
            const PMember *m = &P.m[i];
            Run *q = &r[i];
            if (m->act && m->act != PACT_LAND && m->act_clock == 0.0f) {  // (an action started this present: its clock is still 0)
                if (m->act == PACT_ATTACK) q->attacks++;
                if (m->act == PACT_CAST) q->spells++;
                if (m->act == PACT_HOP) q->hops++;
                if (m->act_entry >= 0 && m->act_entry < 16 && m->act != PACT_HOP) q->starts[m->act_entry]++;
            }
            if (m->act && m->act_spell != PS_NONE) q->spell_frames++;  // (a playing action that is a spell / ability)
            if (m->act == PACT_ATTACK && m->act_entry == 2 && m->act_clock > q->a1_clock) q->a1_clock = m->act_clock;
        }
    }
    for (int i = 0; i < PARTY_N; i++) r[i].hits = g_hits[i], r[i].sounds = g_sounds[i], memcpy(r[i].pos, P.m[i].pos, sizeof r[i].pos);
    g_run_rnd = P.rnd;
}
static void fresh_fight_state(void) { P.battle = false, P.trail.n = 0, P.ticks = 0, P.last_present = 0, P.rnd = 12345, memset(PSP, 0, sizeof PSP); }
static bool same_run(const Run *a, const Run *b) {
    return a->attacks == b->attacks && a->spells == b->spells && a->hops == b->hops && a->hits == b->hits && !memcmp(a->pos, b->pos, sizeof a->pos) &&
           !memcmp(a->starts, b->starts, sizeof a->starts);
}

// the files a stand-in needs (the glue's party_char_data: model + pose + motion) under dir/<key>/
static bool member_files(const char *root, int c) {
    static const char *const W[3] = {"model", "pose", "motion"};
    for (int k = 0; k < 3; k++) {
        char p[512];
        snprintf(p, sizeof p, "%s/%s/%s_%s.bin", root, PCH_TAB[c].key, PCH_TAB[c].key, W[k]);
        FILE *f = fopen(p, "rb");
        if (!f) return false;
        fclose(f);
    }
    return true;
}
// the glue's first load of a stand-in into a slot (party_swap's order: begin, his bakes, take); returns his hit slot count
static int load_member(int slot, int c, const char *dir, int *nmot, int *got) {
    char p[512];
    party_char_begin(slot, c);
    snprintf(p, sizeof p, "%s/%s_motion.bin", dir, PCH_TAB[c].key);
    *got = party_mot_load(slot, p);
    *nmot = PMOT_N[slot];
    snprintf(p, sizeof p, "%s/%s_hits.bin", dir, PCH_TAB[c].key);
    int nh = party_hits_load(slot, p);
    party_char_take(slot, c);
    return nh;
}

// (s220) the four new members' KH2 rows (VERIFIED from the files: sb/s220/members/py/aihdr.py, prty.py, plrp.py, weapons.py,
// gen/members.txt) and their bakes (sb/s220/members/bake/<key>/sanity.txt)
typedef struct {
    int c;
    const PartyDef *d;
    const PEntry *pent;
    float turn, move_min, move_max, hop_chance;
    int nhop;
    float hop0_angle, hop0_up;
    int arow0;           // attack1's A row (the next three follow)
    float a1_dmax, a2_dmax;
    uint8_t lv1_mag, lv99_str, lv99_mag;
    int nmot, nhit;      // the bake: motion slots, hit slots
    uint16_t a300_frames;
    int a300_win;        // A300's hit windows (power 0 windows included: Sparrow's)
} MemberRef;
static const MemberRef MEM[4] = {
    {PCH_AURON, &PARTY_AURON, PENT_AURON, 0.261799f, 30, 180, 0.2f, 2, 180, 150, 8, 300, 500, 0, 58, 0, 41, 7, 86, 1},
    {PCH_MULAN, &PARTY_MULAN, PENT_MULAN, 0.261799f, 300, 600, 0.4f, 3, 45, 25, 16, 300, 450, 7, 58, 61, 41, 6, 88, 1},
    {PCH_ALADDIN, &PARTY_ALADDIN, PENT_ALADDIN, 0.261799f, 120, 300, 0.6f, 3, 60, 40, 20, 250, 300, 0, 58, 0, 44, 8, 56, 1},
    {PCH_SPARROW, &PARTY_SPARROW, PENT_SPARROW, 0.19634955f, 300, 600, 0.6f, 1, 180, 60, 24, 450, 150, 0, 58, 0, 42, 11, 164, 5},
};

int main(int argc, char **argv) {
    const char *rdir = argc > 1 ? argv[1] : "D:/SoraBotW/sb/s217/riku/bake2";  // (s218: the 60 fps fix)
    const char *ddir = argc > 2 ? argv[2] : "D:/SoraBotW/data/party";
    const char *mroot = argc > 3 ? argv[3] : "D:/SoraBotW/sb/s220/members/bake";
    const char *cdir = argc > 4 ? argv[4] : "D:/SoraBotW/sb/s223/cloud/sbuild/out/party";  // (s223)
    g_verbose = getenv("PARTY_TEST_V") != NULL;
    char p[512];

    // ---- sora_pchar.h: who may stand where, what needs whom
    int home[2] = {PCH_DONALD, PCH_GOOFY}, riku[2] = {PCH_RIKU, PCH_GOOFY}, g_out[2] = {PCH_DONALD, PCH_AURON}, both[2] = {PCH_MULAN, PCH_SPARROW};
    CHECK(pchar_can(PCH_DONALD, 0) && !pchar_can(PCH_DONALD, 1) && !pchar_can(PCH_GOOFY, 0) && pchar_can(PCH_GOOFY, 1));
    for (int c = PCH_RIKU; c < PCH_N; c++) CHECK(pchar_can(c, 0) && pchar_can(c, 1) && !pchar_can(c, 2) && !pchar_can(c, -1) && PCH_TAB[c].home == -1);
    CHECK(!pchar_can(-1, 0) && !pchar_can(PCH_N, 0) && pchar_home(home, 0) && pchar_home(home, 1) && !pchar_home(riku, 0) && pchar_home(riku, 1));
    for (int k = 0; k < PNEED_N; k++) CHECK(pchar_need_ok(home, k));
    CHECK(!pchar_need_ok(riku, PNEED_COMET) && !pchar_need_ok(riku, PNEED_DUCKFLARE) && pchar_need_ok(riku, PNEED_WHIRLI) && pchar_need_ok(riku, PNEED_KNOCK));
    CHECK(!pchar_need_ok(riku, PNEED_TRINITY) && !pchar_need_ok(riku, PNEED_SUMMON) && !pchar_need_ok(riku, PNEED_ANTI) && !pchar_need_ok(home, PNEED_N));
    CHECK(!pchar_limit_ok(riku, 0) && pchar_limit_ok(riku, 1) && pchar_limit_ok(home, 0));
    // (s220) Goofy out of the party: no Whirli-Goof / Knocksmash (their Limits are not listed), Summons greyed, no Anti; Donald's stay
    CHECK(!pchar_need_ok(g_out, PNEED_WHIRLI) && !pchar_need_ok(g_out, PNEED_KNOCK) && !pchar_need_ok(g_out, PNEED_SUMMON) && !pchar_need_ok(g_out, PNEED_ANTI));
    CHECK(!pchar_need_ok(g_out, PNEED_TRINITY) && pchar_need_ok(g_out, PNEED_COMET) && pchar_need_ok(g_out, PNEED_DUCKFLARE));
    CHECK(pchar_limit_ok(g_out, 0) && !pchar_limit_ok(g_out, 1) && !strcmp(pchar_missing_text(g_out), "Goofy is"));
    for (int k = 0; k < PNEED_N; k++) CHECK(!pchar_need_ok(both, k));
    CHECK(!strcmp(pchar_missing_text(both), "Donald and Goofy are") && !strcmp(pchar_missing_text(riku), "Donald is"));
    CHECK(pchar_fallback(PCH_RIKU, 0, false) == PCH_DONALD && pchar_fallback(PCH_RIKU, 0, true) == PCH_RIKU && pchar_fallback(PCH_RIKU, 1, true) == PCH_RIKU);
    CHECK(pchar_fallback(99, 0, true) == PCH_DONALD && pchar_fallback(PCH_DONALD, 0, true) == PCH_DONALD && pchar_fallback(PCH_DONALD, 1, true) == PCH_GOOFY);
    {   // (s223 rikufx) sora_pfx.h: the stand-ins' own weapon effects; Donald / Goofy / Cloud have no row (= as before)
        char b[48];
        CHECK(PFX_N == 5 && pfx_row(PCH_DONALD) < 0 && pfx_row(PCH_GOOFY) < 0 && pfx_row(PCH_CLOUD) < 0 && pfx_row(-1) < 0 && pfx_row(PCH_N) < 0);
        for (int c = PCH_RIKU; c <= PCH_SPARROW; c++) CHECK(pfx_row(c) >= 0 && !strcmp(PFX_TAB[pfx_row(c)].key, PCH_TAB[c].key));
        CHECK(pfx_hit_name(PCH_RIKU, 1, b, sizeof b) && !strcmp(b, "pfx_riku_hit1") && pfx_hit_name(PCH_RIKU, 2, b, sizeof b) && !strcmp(b, "pfx_riku_hit2"));
        CHECK(pfx_trail_name(PCH_RIKU, b, sizeof b) && !strcmp(b, "pfx_riku_trail") && pfx_trail_name(PCH_SPARROW, b, sizeof b) && !strcmp(b, "pfx_sparrow_trail"));
        CHECK(!pfx_hit_name(PCH_RIKU, 0, b, sizeof b) && !pfx_hit_name(PCH_RIKU, 4, b, sizeof b) && !pfx_hit_name(PCH_DONALD, 1, b, sizeof b) &&
              !pfx_hit_name(PCH_GOOFY, 1, b, sizeof b) && !pfx_trail_name(PCH_CLOUD, b, sizeof b) && !pfx_hit_name(PCH_RIKU, 1, b, 4));
        // = tools/sorabuild/fx/partyfx.py: 200000 + 1000 * PCH + slot (Riku 2, Auron 3, Mulan 4, Aladdin 5, Sparrow 6: partyfx.py ROWS)
        CHECK(PCH_RIKU == 2 && PCH_AURON == 3 && PCH_MULAN == 4 && PCH_ALADDIN == 5 && PCH_SPARROW == 6);
        CHECK(pfx_trail_key(PCH_RIKU, 604) == 202604 && pfx_trail_key(PCH_SPARROW, 728) == 206728 && pfx_trail_key(PCH_DONALD, 604) < 0 &&
              pfx_trail_key(PCH_RIKU, 1000) < 0 && pfx_trail_key(PCH_RIKU, -1) < 0);
        // never one of Sora's keys (raw slot, forms 10000 * (form + 1) + slot <= 80000 + 999)
        CHECK(pfx_trail_key(PCH_RIKU, 0) > 80999);
        // ribbons: the exe's top two of 8 (slot 0 -> 7, slot 1 -> 6); none on an exe with fewer than 4 / no export
        CHECK(pfx_ribbon_slot(0, 8) == 7 && pfx_ribbon_slot(1, 8) == 6 && pfx_ribbon_slot(0, 4) == 3 && pfx_ribbon_slot(1, 4) == 2);
        CHECK(pfx_ribbon_slot(1, 3) < 0 && pfx_ribbon_slot(0, 2) < 0 && pfx_ribbon_slot(0, 0) < 0 && pfx_ribbon_slot(2, 8) < 0 && pfx_ribbon_slot(-1, 8) < 0);
    }
    CHECK(pchar_of_pick(PPICK_RIKU) == PCH_RIKU && pchar_of_pick(PPICK_DONALD) == PCH_DONALD && pchar_of_pick(-3) == PCH_DONALD && pchar_of_pick(7) == PCH_DONALD);
    for (int c = 0; c < PCH_N; c++) CHECK(strlen(PCH_TAB[c].name) <= 9 && PCH_TAB[c].key && *PCH_TAB[c].key);
    CHECK(PCH_TAB[PCH_RIKU].lvup == 13 && PCH_TAB[PCH_RIKU].sfx && PCH_TAB[PCH_DONALD].sfx && PCH_RIKU == 2);  // (s220 riku2: his sound rows; 2: an older DLL's hot-reload env)
    CHECK(PCH_TAB[PCH_AURON].lvup == 5 && PCH_TAB[PCH_MULAN].lvup == 6 && PCH_TAB[PCH_ALADDIN].lvup == 7 && PCH_TAB[PCH_SPARROW].lvup == 8);
    for (int c = PCH_AURON; c <= PCH_SPARROW; c++) CHECK(PCH_TAB[c].sfx && PCH_TAB[c].wfield && PCH_TAB[c].rt_turn);  // (s223 voices: their sound rows)
    CHECK(!PCH_TAB[PCH_CLOUD].sfx && PCH_TAB[PCH_CLOUD].wfield && PCH_TAB[PCH_CLOUD].rt_turn);  // (s223 merge: Cloud stays silent)
    CHECK(!PCH_TAB[PCH_RIKU].wfield && PCH_TAB[PCH_DONALD].wfield && PCH_TAB[PCH_GOOFY].wfield);

    // ---- (s220) the Debug Menu rows' offer rules (pure): own member first, installed stand-ins in table order, never one twice
    {
        uint32_t all = 0;
        for (int c = PCH_RIKU; c < PCH_N; c++) all |= 1u << c;
        int o[PCH_N], n;
        n = pchar_offer(0, home, all, o);
        CHECK(n == 7 && o[0] == PCH_DONALD && o[1] == PCH_RIKU && o[2] == PCH_AURON && o[5] == PCH_SPARROW && o[6] == PCH_CLOUD);  // (s223: + Cloud)
        n = pchar_offer(1, home, all, o);
        CHECK(n == 7 && o[0] == PCH_GOOFY && o[1] == PCH_RIKU && o[5] == PCH_SPARROW && o[6] == PCH_CLOUD);
        for (int k = 0; k < n; k++) CHECK(o[k] != PCH_DONALD);  // (Donald never in Goofy's row)
        // no character twice: Auron in Goofy's place -> not on Party 1; Mulan in Donald's -> not on Party 2
        int ga[2] = {PCH_DONALD, PCH_AURON}, md[2] = {PCH_MULAN, PCH_GOOFY}, ma[2] = {PCH_MULAN, PCH_AURON};
        CHECK(!pchar_offered(0, ga, all, PCH_AURON) && pchar_offered(1, ga, all, PCH_AURON) && pchar_offered(0, ga, all, PCH_MULAN));
        CHECK(!pchar_offered(1, md, all, PCH_MULAN) && pchar_offered(0, md, all, PCH_MULAN) && pchar_offered(1, md, all, PCH_AURON));
        CHECK(pchar_offer(0, ma, all, o) == 6 && pchar_offer(1, ma, all, o) == 6);
        for (int s = 0; s < 2; s++)
            for (int k = 0, nn = pchar_offer(s, ma, all, o); k < nn; k++) CHECK(!pchar_elsewhere(ma, s, o[k]));
        CHECK(pchar_elsewhere(ma, 0, PCH_AURON) && !pchar_elsewhere(ma, 0, PCH_MULAN) && pchar_elsewhere(ma, 1, PCH_MULAN));
        // a missing bake = not offered (and nothing at all installed = the own member only)
        uint32_t nomulan = all & ~(1u << PCH_MULAN);
        CHECK(!pchar_offered(0, home, nomulan, PCH_MULAN) && !pchar_offered(1, home, nomulan, PCH_MULAN) && pchar_offered(0, home, nomulan, PCH_ALADDIN));
        CHECK(pchar_offer(0, home, 0, o) == 1 && o[0] == PCH_DONALD && pchar_offer(1, home, 0, o) == 1 && o[0] == PCH_GOOFY);
        CHECK(pchar_offer(0, home, 1u << PCH_DONALD | 1u << PCH_GOOFY, o) == 1);  // (the home bits never add a stand-in)
        CHECK(pchar_offer(2, home, all, o) == 0 && pchar_offer(-1, home, all, o) == 0);
        // stepping: Right / Left clamp, A wraps; a row with nobody else refuses (-1)
        CHECK(pchar_step(0, home, all, 1) == PCH_RIKU && pchar_step(0, home, all, -1) == -1 && pchar_step(0, home, all, 0) == PCH_RIKU);
        int sp[2] = {PCH_SPARROW, PCH_GOOFY}, cl[2] = {PCH_CLOUD, PCH_GOOFY}, gc[2] = {PCH_DONALD, PCH_CLOUD};
        CHECK(pchar_step(0, sp, all, 1) == PCH_CLOUD && pchar_step(0, sp, all, -1) == PCH_ALADDIN && pchar_step(0, sp, all, 0) == PCH_CLOUD);
        // (s223) Cloud = the last one: Right beeps there, A wraps to the slot's own member; in Goofy's place he is not on Party 1
        CHECK(pchar_step(0, cl, all, 1) == -1 && pchar_step(0, cl, all, -1) == PCH_SPARROW && pchar_step(0, cl, all, 0) == PCH_DONALD);
        CHECK(!pchar_offered(0, gc, all, PCH_CLOUD) && pchar_offered(1, gc, all, PCH_CLOUD) && pchar_step(0, sp, all & ~(1u << PCH_CLOUD), 1) == -1);
        CHECK(pchar_step(0, home, 0, 1) == -1 && pchar_step(0, home, 0, 0) == -1 && pchar_step(1, home, 0, -1) == -1);
        int rg[2] = {PCH_DONALD, PCH_RIKU};
        CHECK(pchar_step(0, rg, 1u << PCH_RIKU, 1) == -1 && pchar_step(0, rg, 1u << PCH_RIKU, 0) == -1);  // (the one stand-in is in Party 2)
        CHECK(pchar_step(1, ga, all, 1) == PCH_MULAN && pchar_step(1, ga, all, -1) == PCH_RIKU);
        CHECK(pchar_step(0, ga, all, 1) == PCH_RIKU && pchar_step(0, (int[2]){PCH_RIKU, PCH_AURON}, all, 1) == PCH_MULAN);  // (Auron skipped)
        // the installed mask from the real files (the glue's party_char_data): the four new bakes + Riku's, and a folder without them
        uint32_t have = 0;
        for (int c = PCH_AURON; c < PCH_N; c++)
            if (member_files(mroot, c)) have |= 1u << c;
        CHECK(have == (1u << PCH_AURON | 1u << PCH_MULAN | 1u << PCH_ALADDIN | 1u << PCH_SPARROW));  // (Cloud's bake is not in that root)
        uint32_t none = 0;
        for (int c = PCH_RIKU; c < PCH_N; c++)
            if (member_files("D:/SoraBotW/sb/s220/members/no_such_dir", c)) none |= 1u << c;
        CHECK(none == 0 && pchar_offer(0, home, none, o) == 1 && pchar_step(0, home, none, 1) == -1);
    }

    // ---- Riku's rows = KH2's (prty 35, plrp 12, friend block re:0x720730, Way to the Dawn stat row, lvup character 13)
    const PartyDef *R = &PARTY_RIKU;
    CHECK(!strcmp(R->name, "riku") && R->side == 1 && R->walk == 2 && R->run == 8 && fabsf(R->turn - 0.261799f) < 1e-6f);
    CHECK(R->slot[0] == 2 && R->slot[1] == 7 && R->slot[2] == 11 && R->bslot[0] == 0 && R->bslot[1] == 4 && R->bslot[2] == 8);
    CHECK(R->kd[0].slot == 66 && R->kd[1].slot == 70 && R->kd[2].slot == 74 && R->kd[3].slot == 78);
    CHECK(R->search == 800 && R->keep == 1200 && R->leash == 1200 && R->pause_min == 6 && R->pause_max == 60 && R->move_min == 60 && R->move_max == 120);
    CHECK(fabsf(R->hop_chance - 0.2f) < 1e-6f && R->mp_gate_scale == 1 && R->nhop == 1);  // (s220: KH2's 0.2, was 0.3)
    CHECK(R->hop[0].angle == 180 && R->hop[0].up == 80 && R->hop[0].speed == 12 && fabsf(R->hop[0].brake - 0.98f) < 1e-6f && R->hop[0].frames == 18);
    CHECK(R->wstr == 3 && R->wmag == 0 && R->mp_max == 100 && R->mp_charge == 1200);
    CHECK(PARTY_LV_RIKU[0].exp == 0 && PARTY_LV_RIKU[0].str == 4 && PARTY_LV_RIKU[0].mag == 7 && PARTY_LV_RIKU[1].exp == 40);
    CHECK(PARTY_LV_RIKU[98].exp == 2818098 && PARTY_LV_RIKU[98].str == 58 && PARTY_LV_RIKU[98].mag == 61);
    for (int l = 1; l < 99; l++) CHECK(PARTY_LV_RIKU[l].exp > PARTY_LV_RIKU[l - 1].exp && PARTY_LV_RIKU[l].str >= PARTY_LV_RIKU[l - 1].str);
    int nr = PCHAR_PENT_N[PCH_RIKU];
    CHECK(PCHAR_PENT[PCH_RIKU] == PENT_RIKU && nr == 6);
    for (int k = 0; k < nr; k++) CHECK(PENT_RIKU[k].spell == PS_NONE && PENT_RIKU[k].h != PH_SPELL && PENT_RIKU[k].h != PH_CURE && PENT_RIKU[k].cost == 0);
    CHECK(PENT_RIKU[2].row == 44 && PENT_RIKU[2].motion == 151 && PENT_RIKU[2].dmax == 160 && PENT_RIKU[2].hmin == -160 && PENT_RIKU[2].hmax == 160);
    CHECK(PENT_RIKU[3].row == 45 && PENT_RIKU[3].motion == 152 && PENT_RIKU[4].jump && PENT_RIKU[5].jump && PENT_RIKU[4].hmin == 160 && PENT_RIKU[5].hmax == 360);
    CHECK(PCHAR_DEF[PCH_DONALD] == &PARTY[0] && PCHAR_DEF[PCH_GOOFY] == &PARTY[1] && PCHAR_DEF[PCH_RIKU] == &PARTY_RIKU);

    // ---- (s220) Auron / Mulan / Aladdin / Sparrow rows = KH2's
    for (int k = 0; k < 4; k++) {
        const MemberRef *M = &MEM[k];
        const PartyDef *D = M->d;
        CHECK(PCHAR_DEF[M->c] == D && PCHAR_PENT[M->c] == M->pent && PCHAR_PENT_N[M->c] == 6 && PCHAR_LV[M->c] == PARTY_LV_MEMBERS[k]);
        CHECK(!strcmp(D->name, PCH_TAB[M->c].key) && D->walk == 2 && D->run == 8 && fabsf(D->turn - M->turn) < 1e-6f);
        CHECK(D->slot[0] == 2 && D->slot[1] == 7 && D->slot[2] == 11 && D->bslot[0] == 0 && D->bslot[1] == 4 && D->bslot[2] == 8);
        CHECK(D->kd[0].slot == 66 && D->kd[1].slot == 70 && D->kd[2].slot == 74 && D->kd[3].slot == 78);
        CHECK(D->search == 800 && D->keep == 1200 && D->leash == 1200 && D->pause_min == 6 && D->pause_max == 60);
        CHECK(D->move_min == M->move_min && D->move_max == M->move_max && fabsf(D->hop_chance - M->hop_chance) < 1e-6f && D->mp_gate_scale == 1);
        CHECK(D->nhop == M->nhop && D->hop[0].angle == M->hop0_angle && D->hop[0].up == M->hop0_up);
        for (int h = 0; h < D->nhop; h++) CHECK(D->hop[h].frames > 0 && D->hop[h].brake > 0.5f && D->hop[h].speed > 0);
        CHECK(D->wstr == 3 && D->wmag == 0 && D->mp_max == 100 && D->mp_charge == 1200);
        const PEntry *E = M->pent;
        for (int j = 0; j < 6; j++) CHECK(E[j].spell == PS_NONE && E[j].h != PH_SPELL && E[j].h != PH_CURE && E[j].cost == 0);
        CHECK(E[0].h == PH_PAUSE && E[1].h == PH_MOVE && !strcmp(E[2].name, "attack1") && !strcmp(E[5].name, "air2"));
        CHECK(E[2].row == M->arow0 && E[3].row == M->arow0 + 1 && E[4].row == M->arow0 + 2 && E[5].row == M->arow0 + 3);
        CHECK(E[2].motion == 151 && E[3].motion == 152 && E[4].motion == 181 && E[5].motion == 182 && !E[2].jump && !E[3].jump && E[4].jump && E[5].jump);
        CHECK(E[2].dmax == M->a1_dmax && E[3].dmax == M->a2_dmax && E[2].hmin == -100);
        const PLv *L = PARTY_LV_MEMBERS[k];
        CHECK(L[0].exp == 0 && L[0].str == 4 && L[0].mag == M->lv1_mag && L[1].exp == 40 && L[98].exp == 2818098 && L[98].str == M->lv99_str && L[98].mag == M->lv99_mag);
        for (int l = 1; l < 99; l++) CHECK(L[l].exp > L[l - 1].exp && L[l].str >= L[l - 1].str && L[l].mag >= L[l - 1].mag);
    }

    // ---- Donald + Goofy loaded into their slots as party_load does (their real motion / hit bakes, read only)
    for (int i = 0; i < PARTY_N; i++) {
        snprintf(p, sizeof p, "%s/%s_motion.bin", ddir, PARTY[i].name);
        CHECK(party_mot_load(i, p) > 0);
        snprintf(p, sizeof p, "%s/%s_hits.bin", ddir, PARTY[i].name);
        CHECK(party_hits_load(i, p) > 0);
    }
    static __typeof__(P) P0;  // the party state as a fresh DLL has it (the run's start)
    P0 = P;
    PartyDef d0 = PARTY[0], g0 = PARTY[1];
    static PMotX mot0[PMOT_MAX], gmot0[PMOT_MAX];
    static PHSlot hit0[PHIT_MAX], ghit0[PHIT_MAX];
    memcpy(mot0, PMOT[0], sizeof mot0), memcpy(hit0, PHIT[0], sizeof hit0);
    memcpy(gmot0, PMOT[1], sizeof gmot0), memcpy(ghit0, PHIT[1], sizeof ghit0);
    int nm0 = PMOT_N[0], nh0 = PHIT_N[0], gnm0 = PMOT_N[1], gnh0 = PHIT_N[1];
    CHECK(party_lv_tab(0) == PARTY_LV[0] && party_lv_tab(1) == PARTY_LV[1] && PENT[0] == PENT_DONALD && PENT[1] == PENT_GOOFY);

    // Donald + Goofy's fight before any swap (the reference)
    P = P0, memset(PSP, 0, sizeof PSP), P.rnd = 12345;
    Run before[2];
    fight(before, 1500);
    uint32_t rnd_before = g_run_rnd;
    CHECK(before[0].attacks > 0 && before[0].hits > 0 && before[1].attacks > 0);
    printf("  Donald before: %d attacks, %d casts, %d hops, %d hits; Goofy %d attacks, %d casts, %d hits; rnd %u\n", before[0].attacks,
           before[0].spells, before[0].hops, before[0].hits, before[1].attacks, before[1].spells, before[1].hits, rnd_before);

    // ---- a stand-in nobody loaded yet can't be put in (the glue's fallback: Donald stays, nothing changes)
    CHECK(!party_char_put(0, PCH_RIKU) && g_pchar[0] == PCH_DONALD && !memcmp(&PARTY[0], &d0, sizeof d0) && PENT[0] == PENT_DONALD);
    CHECK(!party_char_put(0, -1) && !party_char_put(2, PCH_RIKU) && !party_char_put(0, PCH_N));

    // ---- Riku's first load into slot 0 (the glue's order: begin, his bakes, take)
    party_char_begin(0, PCH_RIKU);
    CHECK(PCS[PCH_DONALD].ok && !strcmp(PARTY[0].name, "riku") && PMOT_N[0] == 0 && PHIT_N[0] == 0 && PARTY[0].side == 1);
    snprintf(p, sizeof p, "%s/riku_motion.bin", rdir);
    int got = party_mot_load(0, p);
    snprintf(p, sizeof p, "%s/riku_hits.bin", rdir);
    int nh = party_hits_load(0, p);
    printf("  Riku's bakes: %d of 7 loco / blow slots, %d motion slots, %d hit slots\n", got, PMOT_N[0], nh);
    CHECK(got == 7 && PMOT_N[0] == 39 && nh == 4);
    CHECK(PARTY[0].frames[0] == 140 && PARTY[0].frames[1] == 64 && PARTY[0].frames[2] == 44 && PARTY[0].bframes[0] == 240 && PARTY[0].bframes[1] == 76);
    CHECK(pmot_find(0, 604) && pmot_find(0, 604)->frames == 68 && pmot_find(0, 724)->frames == 264 &&  // (s218: A300 / A330 are 60 fps motions)
           pmot_find(0, 608)->frames == 52 && pmot_find(0, 14) && pmot_find(0, 22));
    CHECK(phit_find(0, 604) && phit_find(0, 604)->nwin == 3 && phit_find(0, 608)->nwin == 2 && phit_find(0, 604)->win[1].power == 100);
    party_char_take(0, PCH_RIKU);
    CHECK(g_pchar[0] == PCH_RIKU && PCS[PCH_RIKU].ok && PENT[0] == PENT_RIKU && PENT_N[0] == 6 && party_lv_tab(0) == PARTY_LV_RIKU);
    CHECK(pc_find_entry(0, "fire") < 0 && pc_find_entry(0, "cure") < 0 && pc_find_entry(0, "thunder") < 0 && pc_find_entry(0, "attack1") == 2);
    CHECK(!memcmp(&PARTY[1], &g0, sizeof g0) && PENT[1] == PENT_GOOFY && g_pchar[1] == PCH_GOOFY);  // (Goofy's slot untouched)
    CHECK(!P.m[0].placed && P.m[0].act == 0 && P.m[0].kd == KD_NONE && P.m[0].hp == 100 && P.m[0].mp == 100 && P.m[0].cur == -1);
    // the remote party (a friend's game) keeps Donald's own def / motions whoever stands here
    CHECK(party_home_def(0) == &PCS[PCH_DONALD].def && !strcmp(party_home_def(0)->name, "donald") && party_home_def(1) == &PARTY[1]);
    CHECK(party_home_mot(0, 604) && party_home_mot(0, 604)->frames == 64 && party_home_mot(0, 764) && !pmot_find(0, 764));
    // (s220) no character twice: Riku can't also go into Goofy's place
    CHECK(!party_char_put(1, PCH_RIKU) && g_pchar[1] == PCH_GOOFY && !memcmp(&PARTY[1], &g0, sizeof g0));

    // ---- Riku fights: his A-row attacks only, never one of Donald's spells, his windows hit
    fresh_fight_state();
    Run rk[2];
    fight(rk, 1500);
    printf("  Riku: %d attacks (attack1 %d, attack2 %d), %d casts, %d hops, %d hits, %d spell frames, %d script sounds\n", rk[0].attacks,
           rk[0].starts[2], rk[0].starts[3], rk[0].spells, rk[0].hops, rk[0].hits, rk[0].spell_frames, rk[0].sounds);
    CHECK(rk[0].attacks > 0 && rk[0].starts[2] > 0 && rk[0].starts[3] > 0 && rk[0].spells == 0 && rk[0].spell_frames == 0 && rk[0].hits > 0);
    CHECK(rk[0].starts[4] == 0 && rk[0].starts[5] == 0);  // (the jump rows stay off: P.air_rows, as Donald's)
    for (int s = 0; s < PSP_N; s++) CHECK(!(PSP[s].on && PSP[s].member == 0));
    // (s218) attack1 = A300's real 68 frames (the old bake held its last pose 68 more: the "freeze")
    printf("  Riku attack1: longest clock %.1f of 68 frames\n", rk[0].a1_clock);
    CHECK(rk[0].a1_clock > 60 && rk[0].a1_clock < 68);
    // (s218) his attacks turn on their own range trigger 22 frames (A300 0-10, 20-33, 44-48); A301 0-6, 18-27
    const PMotX *a300 = pmot_find(0, 604), *a301 = pmot_find(0, 608);
    CHECK(a300->bits && pc_rt_turn(0, a300) && pc_rt_turn(0, a301) && !pc_rt_turn(0, pmot_find(0, 7)));
    CHECK(pc_rt_bit(a300, 0, 22) && pc_rt_bit(a300, 10.9f, 22) && !pc_rt_bit(a300, 11, 22) && !pc_rt_bit(a300, 15, 22) && pc_rt_bit(a300, 25, 22));
    CHECK(pc_rt_bit(a300, 46, 22) && !pc_rt_bit(a300, 50, 22) && !pc_rt_bit(a300, 67.9f, 22) && pc_rt_bit(a301, 20, 22) && !pc_rt_bit(a301, 10, 22));
    // (s218) Way to the Dawn: hidden on his field idle / walk / run slots (2 / 7 / 11) and while a blend out of one runs
    CHECK(party_weapon_field_hidden(0, 7, false, -1) && party_weapon_field_hidden(0, 2, false, -1) && party_weapon_field_hidden(0, 11, true, 7));
    CHECK(!party_weapon_field_hidden(0, 4, false, 7) && party_weapon_field_hidden(0, 4, true, 7) && !party_weapon_field_hidden(0, 604, false, -1));
    CHECK(!party_weapon_field_hidden(1, 7, false, -1) && !party_weapon_field_hidden(1, 2, true, 7));  // (Goofy: always shown)
    // (s218) the lunge: A300's root moves him 400 cm (to_u 0.01: 4 u) with nobody in the way; an enemy 190 cm ahead stops him at its
    // body (pc_push, KH2's pair push stand-in): the lunge is applied, the push takes it back
    for (int withen = 0; withen < 2; withen++) {
        PMember *m = &P.m[0];
        memset(PSP, 0, sizeof PSP);
        float z0 = 0;
        m->pos[0] = 0, m->pos[1] = 0, m->pos[2] = z0, m->face = 0, m->act = PACT_ATTACK, m->act_entry = 2, m->act_slot = 604, m->act_clock = 0;
        m->act_frames = a300->frames, m->act_target = withen ? 0xE1 : 0, m->act_spell = PS_NONE, m->kd = KD_NONE, m->blend = -1;
        g_nen = withen, g_en[0] = (PEnemy){0xE1, {0, 0, 1.9f}, 100};
        float zmax = 0;
        int n = 0;
        while (m->act == PACT_ATTACK && n++ < 100) {
            PIn in = {{0, 0, -3.0f}, 0, true, 2.0f, 0.01f, (uint64_t)(5000 + n), ray, g_en, g_nen, 0.5f, 1.6f, 12, 12, &HK};
            pc_act_tick(0, &in);
            if (m->act) pc_push(0, &in);
            if (m->pos[2] > zmax) zmax = m->pos[2];
        }
        printf("  Riku A300 lunge %s: %d presents, moved %.2f u (max %.2f), ends %.2f u from the enemy centre\n", withen ? "at an enemy 1.9 u ahead" : "alone",
               n, m->pos[2] - z0, zmax, 1.9f - m->pos[2]);
        if (!withen) CHECK(fabsf(m->pos[2] - 4.0f) < 0.05f && n <= 35);
        else CHECK(m->pos[2] < 1.9f && m->pos[2] > 0.3f);
        m->act = PACT_NONE;
    }
    g_nen = 1;

    // ---- back to Donald: byte-identical, and he fights exactly as before (same rolls, same moves, same hits)
    CHECK(party_char_put(0, PCH_DONALD) && g_pchar[0] == PCH_DONALD);
    CHECK(!memcmp(&PARTY[0], &d0, sizeof d0) && PMOT_N[0] == nm0 && PHIT_N[0] == nh0 && PENT[0] == PENT_DONALD && PENT_N[0] == PCHAR_PENT_N[PCH_DONALD]);
    CHECK(!memcmp(PMOT[0], mot0, sizeof mot0) && !memcmp(PHIT[0], hit0, sizeof hit0) && party_lv_tab(0) == PARTY_LV[0]);
    CHECK(party_home_def(0) == &PARTY[0] && party_home_mot(0, 604) == pmot_find(0, 604));
    CHECK(!pc_rt_turn(0, pmot_find(0, 604)) && !party_weapon_field_hidden(0, 7, false, -1));  // (s218: Donald's s55 rules, weapon always shown)
    P = P0, memset(PSP, 0, sizeof PSP), P.rnd = 12345;
    Run after[2];
    fight(after, 1500);
    printf("  Donald after Riku: %d attacks, %d casts, %d hops, %d hits, rnd %u\n", after[0].attacks, after[0].spells, after[0].hops, after[0].hits, g_run_rnd);
    CHECK(same_run(&after[0], &before[0]) && same_run(&after[1], &before[1]) && g_run_rnd == rnd_before);
    // a second Riku pick comes from PCS (no reload): the same tables
    CHECK(party_char_put(0, PCH_RIKU) && PMOT_N[0] == 39 && PHIT_N[0] == 4 && !strcmp(PARTY[0].name, "riku") && party_char_put(0, PCH_DONALD));
    CHECK(!memcmp(&PARTY[0], &d0, sizeof d0));
    // (s220) Riku in GOOFY's place: Goofy's side (-1), Goofy saved and given back byte-identical
    CHECK(party_char_put(1, PCH_RIKU) && g_pchar[1] == PCH_RIKU && PARTY[1].side == -1 && PCS[PCH_GOOFY].ok && PENT[1] == PENT_RIKU);
    CHECK(!pchar_need_ok(g_pchar, PNEED_WHIRLI) && pchar_need_ok(g_pchar, PNEED_COMET) && party_home_def(1) == &PCS[PCH_GOOFY].def);
    CHECK(!party_char_put(0, PCH_RIKU) && g_pchar[0] == PCH_DONALD);  // (no character twice, the other way round)
    CHECK(party_char_put(1, PCH_GOOFY) && !memcmp(&PARTY[1], &g0, sizeof g0) && PMOT_N[1] == gnm0 && PHIT_N[1] == gnh0 && PENT[1] == PENT_GOOFY);
    CHECK(!memcmp(PMOT[1], gmot0, sizeof gmot0) && !memcmp(PHIT[1], ghit0, sizeof ghit0) && party_lv_tab(1) == PARTY_LV[1]);

    // ---- (s220) each new member: his real bakes, first in Donald's place, then (from PCS) in Goofy's; he fights with his A rows only
    for (int k = 0; k < 4; k++) {
        const MemberRef *M = &MEM[k];
        const char *nm = PCH_TAB[M->c].name;
        char dir[512];
        snprintf(dir, sizeof dir, "%s/%s", mroot, PCH_TAB[M->c].key);
        int nmot, gotk, nhk = load_member(0, M->c, dir, &nmot, &gotk);
        printf("  %s's bakes: %d of 7 loco / blow slots, %d motion slots, %d hit slots\n", nm, gotk, nmot, nhk);
        CHECK(gotk == 7 && nmot == M->nmot && nhk == M->nhit && g_pchar[0] == M->c && PCS[M->c].ok && PARTY[0].side == 1);
        CHECK(PENT[0] == M->pent && PENT_N[0] == 6 && party_lv_tab(0) == PARTY_LV_MEMBERS[k] && !strcmp(PARTY[0].name, PCH_TAB[M->c].key));
        const PMotX *mo = pmot_find(0, 604);
        const PHSlot *hs = phit_find(0, 604);
        CHECK(mo && mo->frames == M->a300_frames && hs && hs->nwin == M->a300_win && pmot_find(0, 608) && phit_find(0, 608));
        CHECK(PARTY[0].frames[0] == M->d->frames[0] && PARTY[0].frames[1] == M->d->frames[1] && PARTY[0].frames[2] == M->d->frames[2]);  // (the defaults = his bake)
        CHECK(PARTY[0].bframes[0] == M->d->bframes[0] && PARTY[0].kd[0].frames == M->d->kd[0].frames && PARTY[0].kd[1].frames == M->d->kd[1].frames);
        CHECK(!party_weapon_field_hidden(0, 7, false, -1) && !party_weapon_field_hidden(0, 2, true, 11));  // (his weapon shown on the field)
        CHECK(!pc_rt_turn(0, mo) && pc_rt_turn(0, pmot_find(0, 724)));  // (ground attacks carry no turn frames; the air rows do)
        CHECK(!memcmp(&PARTY[1], &g0, sizeof g0) && g_pchar[1] == PCH_GOOFY);
        fresh_fight_state();
        Run r[2];
        fight(r, 1500);
        printf("  %s in Donald's place: %d attacks (attack1 %d, attack2 %d), %d casts, %d hops, %d hits, %d spell frames, %d sounds\n", nm,
               r[0].attacks, r[0].starts[2], r[0].starts[3], r[0].spells, r[0].hops, r[0].hits, r[0].spell_frames, r[0].sounds);
        CHECK(r[0].attacks > 0 && r[0].starts[2] + r[0].starts[3] == r[0].attacks && r[0].spells == 0 && r[0].spell_frames == 0 && r[0].hits > 0);
        CHECK(r[0].starts[4] == 0 && r[0].starts[5] == 0 && r[0].sounds == 0);
        for (int s = 0; s < PSP_N; s++) CHECK(!(PSP[s].on && PSP[s].member == 0));
        CHECK(party_char_put(0, PCH_DONALD) && !memcmp(&PARTY[0], &d0, sizeof d0) && PENT[0] == PENT_DONALD);
        // in Goofy's place (no reload: his PCS state), Goofy's side; he fights there too
        CHECK(party_char_put(1, M->c) && g_pchar[1] == M->c && PARTY[1].side == -1 && PMOT_N[1] == M->nmot && PHIT_N[1] == M->nhit && PENT[1] == M->pent);
        CHECK(!pchar_need_ok(g_pchar, PNEED_KNOCK) && !pchar_need_ok(g_pchar, PNEED_SUMMON) && !pchar_need_ok(g_pchar, PNEED_ANTI) && pchar_need_ok(g_pchar, PNEED_DUCKFLARE));
        fresh_fight_state();
        fight(r, 1500);
        printf("  %s in Goofy's place: %d attacks, %d casts, %d hits (Donald beside him: %d attacks, %d casts)\n", nm, r[1].attacks, r[1].spells, r[1].hits,
               r[0].attacks, r[0].spells);
        CHECK(r[1].attacks > 0 && r[1].spells == 0 && r[1].spell_frames == 0 && r[1].hits > 0 && r[1].starts[4] == 0 && r[1].starts[5] == 0);
        for (int s = 0; s < PSP_N; s++) CHECK(!(PSP[s].on && PSP[s].member == 1));
        CHECK(party_char_put(1, PCH_GOOFY) && !memcmp(&PARTY[1], &g0, sizeof g0) && PENT[1] == PENT_GOOFY);
    }
    // two stand-ins at once (Mulan in Donald's place, Sparrow in Goofy's): nothing that needs Donald or Goofy; both fight
    CHECK(party_char_put(0, PCH_MULAN) && party_char_put(1, PCH_SPARROW) && PARTY[0].side == 1 && PARTY[1].side == -1);
    CHECK(!party_char_put(0, PCH_SPARROW) && !party_char_put(1, PCH_MULAN) && g_pchar[0] == PCH_MULAN && g_pchar[1] == PCH_SPARROW);
    for (int k = 0; k < PNEED_N; k++) CHECK(!pchar_need_ok(g_pchar, k));
    {
        fresh_fight_state();
        Run r[2];
        fight(r, 1500);
        printf("  Mulan + Sparrow: %d / %d attacks, %d / %d hits, %d casts\n", r[0].attacks, r[1].attacks, r[0].hits, r[1].hits, r[0].spells + r[1].spells);
        CHECK(r[0].attacks > 0 && r[1].attacks > 0 && r[0].hits > 0 && r[1].hits > 0 && r[0].spells + r[1].spells == 0);
    }

    // ---- (s223) Cloud (N_HB550_BTL, the 1000 Heartless helper): no KH2 party rows of his (recon/s223/S223_HELPER_CLOUD.md): walk =
    // his town walk's own root (168 cm / 72 frames), never blown (no blow motion), no hop; his real bake (sorabuild party_cloud)
    {
        const PartyDef *D = &PARTY_CLOUD;
        CHECK(PCH_CLOUD == 7 && PCH_N == 8 && !strcmp(PCH_TAB[PCH_CLOUD].key, "cloud") && !strcmp(PCH_TAB[PCH_CLOUD].name, "Cloud"));
        CHECK(PCH_TAB[PCH_CLOUD].lvup == 0 && !PCH_TAB[PCH_CLOUD].sfx && PCH_TAB[PCH_CLOUD].wfield && PCH_TAB[PCH_CLOUD].home == -1);
        CHECK(PCHAR_DEF[PCH_CLOUD] == D && PCHAR_PENT[PCH_CLOUD] == PENT_CLOUD && PCHAR_PENT_N[PCH_CLOUD] == 4 && PCHAR_LV[PCH_CLOUD] == PARTY_LV_MEMBERS[0]);
        CHECK(!strcmp(D->name, "cloud") && fabsf(D->walk - 168.0f / 72.0f) < 1e-5f && D->run == 8 && fabsf(D->turn - 0.261799f) < 1e-6f);
        CHECK(D->no_blow && D->nhop == 0 && D->hop_chance == 0 && D->wstr == 3 && D->wmag == 0 && D->mp_max == 100);
        CHECK(D->slot[0] == 2 && D->slot[1] == 7 && D->slot[2] == 11 && D->bslot[0] == 0 && D->bslot[1] == 4 && D->bslot[2] == 8);
        for (int c = 0; c < PCH_CLOUD; c++) CHECK(!PCHAR_DEF[c]->no_blow);  // (everybody else is blown as before)
        CHECK(PENT_CLOUD[0].h == PH_PAUSE && PENT_CLOUD[1].h == PH_MOVE && !strcmp(PENT_CLOUD[2].name, "attack1") && !strcmp(PENT_CLOUD[3].name, "attack2"));
        for (int j = 0; j < 4; j++) CHECK(PENT_CLOUD[j].spell == PS_NONE && PENT_CLOUD[j].cost == 0 && !PENT_CLOUD[j].jump && PENT_CLOUD[j].h != PH_SPELL);
        CHECK(PENT_CLOUD[2].motion == 151 && PENT_CLOUD[3].motion == 152 && PENT_CLOUD[2].dmax == 250 && PENT_CLOUD[3].dmin == 200 && PENT_CLOUD[3].dmax == 600);
        uint32_t chave = 0;
        {
            static const char *const W[4] = {"model", "pose", "motion", "hits"};
            int k = 0;
            for (; k < 4; k++) {
                snprintf(p, sizeof p, "%s/cloud_%s.bin", cdir, W[k]);
                FILE *f = fopen(p, "rb");
                if (!f) break;
                fclose(f);
            }
            if (k == 4) chave = 1u << PCH_CLOUD;
        }
        CHECK(chave == 1u << PCH_CLOUD);
        // his first load into Donald's place
        CHECK(party_char_put(0, PCH_DONALD) && party_char_put(1, PCH_GOOFY));
        int nmot, gotc, nhc = load_member(0, PCH_CLOUD, cdir, &nmot, &gotc);
        printf("  Cloud's bakes: %d of 7 loco / blow slots (he has no blow), %d motion slots, %d hit slots\n", gotc, nmot, nhc);
        CHECK(gotc == 3 && nmot == 8 && nhc == 2 && g_pchar[0] == PCH_CLOUD && PCS[PCH_CLOUD].ok && PARTY[0].side == 1 && PARTY[0].no_blow);
        CHECK(PENT[0] == PENT_CLOUD && PENT_N[0] == 4 && party_lv_tab(0) == PARTY_LV_MEMBERS[0] && !strcmp(PARTY[0].name, "cloud"));
        CHECK(PARTY[0].frames[0] == 180 && PARTY[0].frames[1] == 72 && PARTY[0].frames[2] == 72 && PARTY[0].bframes[0] == 120 && PARTY[0].bframes[1] == 72);
        const PMotX *c1 = pmot_find(0, 604), *c2 = pmot_find(0, 608);
        const PHSlot *h1 = phit_find(0, 604), *h2 = phit_find(0, 608);
        CHECK(c1 && c1->frames == 138 && c2 && c2->frames == 72 && h1 && h1->nwin == 3 && h2 && h2->nwin == 1);
        CHECK(h1->win[0].atkp == 1651 && h1->win[2].atkp == 1653 && h1->win[0].power == 100 && h2->win[0].atkp == 1654 && h2->win[0].power == 75);
        CHECK(h1->win[0].start == 20 && h1->win[0].end == 28 && h2->win[0].start == 26 && h2->win[0].end == 39 && h1->win[0].group == 5);
        CHECK(!pmot_find(0, 12) && !pmot_find(0, 20) && !pmot_find(0, 66) && !pmot_find(0, 724));  // (no jump / land / blow / air motion)
        CHECK(!party_weapon_field_hidden(0, 7, false, -1) && !pc_rt_turn(0, c1) && !pc_rt_turn(0, c2));
        CHECK(!memcmp(&PARTY[1], &g0, sizeof g0) && g_pchar[1] == PCH_GOOFY);
        fresh_fight_state();
        Run r[2];
        fight(r, 1500);
        printf("  Cloud in Donald's place: %d attacks (attack1 %d, attack2 %d), %d casts, %d hops, %d hits, %d spell frames, %d sounds\n",
               r[0].attacks, r[0].starts[2], r[0].starts[3], r[0].spells, r[0].hops, r[0].hits, r[0].spell_frames, r[0].sounds);
        CHECK(r[0].attacks > 0 && r[0].starts[2] > 0 && r[0].starts[2] + r[0].starts[3] == r[0].attacks && r[0].spells == 0 && r[0].spell_frames == 0);
        CHECK(r[0].hits > 0 && r[0].hops == 0 && r[0].sounds == 0);
        for (int s = 0; s < PSP_N; s++) CHECK(!(PSP[s].on && PSP[s].member == 0));
        // never knocked down (a blast right beside him), while Riku in the same place still is
        CHECK(P.m[0].placed);
        party_knockdown_core(0, (float[3]){P.m[0].pos[0], 0, P.m[0].pos[2] + 0.5f}, 1, 0.01f);
        CHECK(P.m[0].kd == KD_NONE && party_blast_core(P.m[0].pos, 600, 0.01f) >= 1 && P.m[0].kd == KD_NONE);
        // 51A's dash: its root moves him 662 cm (to_u 0.01: 6.62 u) with nobody in the way
        {
            PMember *m = &P.m[0];
            memset(PSP, 0, sizeof PSP);
            m->pos[0] = 0, m->pos[1] = 0, m->pos[2] = 0, m->face = 0, m->act = PACT_ATTACK, m->act_entry = 3, m->act_slot = 608, m->act_clock = 0;
            m->act_frames = c2->frames, m->act_target = 0, m->act_spell = PS_NONE, m->kd = KD_NONE, m->blend = -1;
            g_nen = 0;
            int n = 0;
            while (m->act == PACT_ATTACK && n++ < 100) {
                PIn in = {{0, 0, -3.0f}, 0, true, 2.0f, 0.01f, (uint64_t)(7000 + n), ray, g_en, g_nen, 0.5f, 1.6f, 12, 12, &HK};
                pc_act_tick(0, &in);
            }
            printf("  Cloud 51A dash alone: %d presents, moved %.2f u\n", n, m->pos[2]);
            CHECK(fabsf(m->pos[2] - 6.62f) < 0.05f && n <= 40);
            m->act = PACT_NONE, g_nen = 1;
        }
        CHECK(party_char_put(0, PCH_RIKU) && !PARTY[0].no_blow);
        P.m[0].placed = true, P.m[0].pos[0] = P.m[0].pos[1] = P.m[0].pos[2] = 0, P.m[0].face = 0;
        party_knockdown_core(0, (float[3]){0, 0, 0.5f}, 1, 0.01f);
        CHECK(P.m[0].kd == KD_AIR);
        CHECK(party_char_put(0, PCH_DONALD) && !memcmp(&PARTY[0], &d0, sizeof d0) && PENT[0] == PENT_DONALD && P.m[0].kd == KD_NONE);
        // in Goofy's place (from PCS, no reload): Goofy's side, he fights there; then with Riku in Donald's place (two stand-ins)
        CHECK(party_char_put(1, PCH_CLOUD) && g_pchar[1] == PCH_CLOUD && PARTY[1].side == -1 && PMOT_N[1] == 8 && PHIT_N[1] == 2 && PENT[1] == PENT_CLOUD);
        CHECK(!pchar_need_ok(g_pchar, PNEED_WHIRLI) && !pchar_need_ok(g_pchar, PNEED_SUMMON) && pchar_need_ok(g_pchar, PNEED_COMET));
        fresh_fight_state();
        fight(r, 1500);
        printf("  Cloud in Goofy's place: %d attacks, %d casts, %d hits (Donald beside him: %d attacks, %d casts)\n", r[1].attacks, r[1].spells, r[1].hits,
               r[0].attacks, r[0].spells);
        CHECK(r[1].attacks > 0 && r[1].spells == 0 && r[1].spell_frames == 0 && r[1].hits > 0 && r[1].hops == 0);
        CHECK(party_char_put(0, PCH_RIKU) && !party_char_put(0, PCH_CLOUD) && g_pchar[0] == PCH_RIKU && g_pchar[1] == PCH_CLOUD);
        fresh_fight_state();
        fight(r, 1500);
        printf("  Riku + Cloud: %d / %d attacks, %d / %d hits, %d casts\n", r[0].attacks, r[1].attacks, r[0].hits, r[1].hits, r[0].spells + r[1].spells);
        CHECK(r[0].attacks > 0 && r[1].attacks > 0 && r[0].hits > 0 && r[1].hits > 0 && r[0].spells + r[1].spells == 0);
    }

    // ---- everybody home: Donald + Goofy byte-identical and their fight bit for bit the reference one
    CHECK(party_char_put(0, PCH_DONALD) && party_char_put(1, PCH_GOOFY));
    CHECK(!memcmp(&PARTY[0], &d0, sizeof d0) && !memcmp(&PARTY[1], &g0, sizeof g0) && !memcmp(PMOT[0], mot0, sizeof mot0) && !memcmp(PMOT[1], gmot0, sizeof gmot0));
    CHECK(!memcmp(PHIT[0], hit0, sizeof hit0) && !memcmp(PHIT[1], ghit0, sizeof ghit0) && PENT[0] == PENT_DONALD && PENT[1] == PENT_GOOFY);
    P = P0, memset(PSP, 0, sizeof PSP), P.rnd = 12345;
    Run last[2];
    fight(last, 1500);
    printf("  Donald + Goofy after all the swaps: %d / %d attacks, %d / %d casts, %d / %d hits, rnd %u\n", last[0].attacks, last[1].attacks,
           last[0].spells, last[1].spells, last[0].hits, last[1].hits, g_run_rnd);
    CHECK(same_run(&last[0], &before[0]) && same_run(&last[1], &before[1]) && g_run_rnd == rnd_before);

    printf("%s: %d/%d checks passed\n", fails ? "FAILED" : "OK", runs - fails, runs);
    return fails ? 1 : 0;
}

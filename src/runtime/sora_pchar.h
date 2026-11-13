// sora_pchar.h: (s217 riku, party swap STEP 1; s220 members: slice 3) WHO stands in a party slot: the character table and the
// rules KH2 ties to a member. Pure (no game, no sora.c): included by sora_party.h and sora_camp_dbg.h, host-tested by
// runtime/test/party_test.c (+ camp_test.c for the Debug Menu rows).
//
// A party SLOT (0 = Donald's place, 1 = Goofy's) holds a CHARACTER. Slot 0 = Donald and slot 1 = Goofy is the game as it
// always was; the Debug Menu rows "Party 1" (slot 0) and "Party 2" (slot 1) can put a world member in either slot for this
// session. A row offers its slot's own member + every stand-in whose bake is installed, never a character that already stands in
// the other slot (pchar_offer). More members = one more row here + one in sora_party.h's PCHAR_* tables + their bake
// (data/party/<key>_{model,pose,motion,hits}.bin, sorabuild step party_<key>).
//
// What KH2 ties to a member (user s214: "what needs the missing member is not offered"):
//   Comet + Duck Flare need Donald, Whirli-Goof + Knocksmash need Goofy (their Limits: hidden from the Limit list),
//   Trinity needs both (not in the mod yet: the rule is here for when it is), Summons + Anti need both (greyed / no roll).
// Drive Form absorption stays by SLOT (Wisdom takes slot 0, Valor slot 1, Master / Final both): whoever stands there.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// (s220: ids 0..2 unchanged: the hot-reload env SORA_PARTY_PICK of an older DLL names Riku 2)
enum { PCH_DONALD, PCH_GOOFY, PCH_RIKU, PCH_AURON, PCH_MULAN, PCH_ALADDIN, PCH_SPARROW, PCH_CLOUD, PCH_N };  // (s223: + Cloud 7)
#define PCH_SLOTS 2

typedef struct {
    const char *key;   // file prefix + log name: <party dir>/<key>_model.bin, _pose.bin, _motion.bin, _hits.bin
    const char *name;  // the name the Debug Menu shows (<= 9 chars: the tag column)
    int home;          // the slot this is KH2's own member of (0 Donald, 1 Goofy), -1 = a world member standing in
    uint8_t slots;     // bit k: may stand in slot k
    bool sfx;          // he has KH2 sound rows (Donald / Goofy: the s56 pack; a stand-in (s220): sora_party.h PSFX_* rows keyed by
                       // his PCH_*, played only when sfx.bin has his marker <key>_sfx_v1, else silent); false = always silent
    int lvup;          // 00battle.bin lvup character (Donald 2, Goofy 3, Auron 5, Mulan 6, Aladdin 7, Sparrow 8, Riku 13; Cloud 0 = none)
    bool wfield;       // (s218) his weapon is drawn on the FIELD idle / walk / run slots (false: hidden there, shown in battle stance)
    bool rt_turn;      // (s218) an attack turns toward its target on its motion's range trigger 22 frames (false: the s55 first-frames rule)
} PChar;
static const PChar PCH_TAB[PCH_N] = {
    {"donald", "Donald", 0, 1u << 0, true, 2, true, false},
    {"goofy", "Goofy", 1, 1u << 1, true, 3, true, false},
    // s218: Way to the Dawn hidden on his field slots (KH2's own field weapon motions on bone 53 cut through his legs in every frame,
    // battle stance / attacks never: recon/s218/S218_HELPER_RIKU_FIX.md 1), his attacks turn on their range trigger 22 frames
    // (A300: 0-10, 20-33, 44-48). s220: both slots (step 1 had Donald's only).
    // s220 riku2: his sounds (voice eh0_riku, se593 swings / hits, footsteps) = fx/sfxparty2.py's rows
    {"riku", "Riku", -1, 3u, true, 13, false, true},
    // (s220 members) weapon SHOWN on the field: sb/s220/members/py/wcut.py finds no weapon through the legs on their field idle /
    // walk / run (Auron: a few blade tris touch his arm / collar at 1.2-1.4 m on walk / run, as his battle stance rests the sword on the
    // shoulder; Sparrow: 2 tris at 0.93 m = the blade against his coat on the field idle); renders sb/s220/members/render/*/sheet.jpg.
    // rt_turn: only their air rows (A330 / A331, off: P.air_rows) carry turn frames; their ground attacks fall back to the s55 rule.
    // (s223 voices) sfx: their KH2 sounds (voice he0_auron / mu0_mulan / al0_aladdin / ca0_sparrow, swings, hits, steps) = fx/sfxparty2.py's rows
    {"auron", "Auron", -1, 3u, true, 5, true, true},
    {"mulan", "Mulan", -1, 3u, true, 6, true, true},
    {"aladdin", "Aladdin", -1, 3u, true, 7, true, true},
    {"sparrow", "Sparrow", -1, 3u, true, 8, true, true},  // (KH2's "Jack Sparrow" is 12 chars: the tag column takes 9)
    // (s223 cloud, recon/s223/S223_HELPER_CLOUD.md) Cloud = N_HB550_BTL, the 1000 Heartless helper: NOT a KH2 party member, so no lvup
    // character (0: the HUD plate keeps the slot's own face; KH2 has no plate face of his) and silent. wfield true: KH2's own town
    // motions (his field idle / walk) scale the sword bone to 0.01 themselves (his battle idle and attacks hold it), so no hiding here.
    {"cloud", "Cloud", -1, 3u, false, 0, true, true},
};
static const int PCH_HOME[PCH_SLOTS] = {PCH_DONALD, PCH_GOOFY};

static inline bool pchar_valid(int c) { return c >= 0 && c < PCH_N; }
static inline bool pchar_can(int c, int slot) { return pchar_valid(c) && slot >= 0 && slot < PCH_SLOTS && (PCH_TAB[c].slots >> slot & 1); }
// slot holds its own KH2 member (Donald in 0, Goofy in 1)
static inline bool pchar_home(const int pchar[PCH_SLOTS], int slot) { return slot >= 0 && slot < PCH_SLOTS && pchar[slot] == PCH_HOME[slot]; }
// (s220) character c stands in a slot other than `slot` (the no-character-twice rule)
static inline bool pchar_elsewhere(const int pchar[PCH_SLOTS], int slot, int c) {
    for (int s = 0; s < PCH_SLOTS; s++)
        if (s != slot && pchar[s] == c) return true;
    return false;
}

// the party features that need a particular member
enum { PNEED_COMET, PNEED_DUCKFLARE, PNEED_WHIRLI, PNEED_KNOCK, PNEED_TRINITY, PNEED_SUMMON, PNEED_ANTI, PNEED_N };
static const char *const PNEED_NAME[PNEED_N] = {"Comet", "Duck Flare", "Whirli-Goof", "Knocksmash", "Trinity", "Summons", "Anti Form"};
static const uint8_t PNEED_SLOTS[PNEED_N] = {1u << 0, 1u << 0, 1u << 1, 1u << 1, 3u, 3u, 3u};  // which slots must hold their own member
static inline bool pchar_need_ok(const int pchar[PCH_SLOTS], int need) {
    if (need < 0 || need >= PNEED_N) return false;
    for (int s = 0; s < PCH_SLOTS; s++)
        if ((PNEED_SLOTS[need] >> s & 1) && !pchar_home(pchar, s)) return false;
    return true;
}
// a party Limit whose member is party slot `member` (sora_plimit.h PlDef.member): offered only with that slot's own member
static inline bool pchar_limit_ok(const int pchar[PCH_SLOTS], int member) { return pchar_home(pchar, member); }
// (s220) the log / gate text of who is missing: "Donald" / "Goofy" / "Donald and Goofy" for a need's slots that are not home
static inline const char *pchar_missing_text(const int pchar[PCH_SLOTS]) {
    bool d = !pchar_home(pchar, 0), g = !pchar_home(pchar, 1);
    return d && g ? "Donald and Goofy are" : d ? "Donald is" : g ? "Goofy is" : "nobody is";
}

// ---- (s220) the Debug Menu rows "Party 1" (slot 0) / "Party 2" (slot 1): the row's value = the character id in the slot.
// The characters a row offers, in order: the slot's own member, then every stand-in that may stand there, whose bake is
// installed (bit c of `have`: party_have_mask, sora_party.h) and who does not stand in the other slot. Returns the count.
static inline int pchar_offer(int slot, const int pchar[PCH_SLOTS], uint32_t have, int out[PCH_N]) {
    if (slot < 0 || slot >= PCH_SLOTS) return 0;
    int n = 0;
    out[n++] = PCH_HOME[slot];
    for (int c = 0; c < PCH_N; c++)
        if (PCH_TAB[c].home < 0 && pchar_can(c, slot) && (have >> c & 1) && !pchar_elsewhere(pchar, slot, c)) out[n++] = c;
    return n;
}
static inline bool pchar_offered(int slot, const int pchar[PCH_SLOTS], uint32_t have, int c) {
    int o[PCH_N], n = pchar_offer(slot, pchar, have, o);
    for (int k = 0; k < n; k++)
        if (o[k] == c) return true;
    return false;
}
// Left / Right (dir -1 / +1) on the row: the offered character before / after the slot's one, -1 at an end (clamped, as the Outfit
// row); dir 0 = A: the next one, wrapping (-1 when the row offers nobody else)
static inline int pchar_step(int slot, const int pchar[PCH_SLOTS], uint32_t have, int dir) {
    int o[PCH_N], n = pchar_offer(slot, pchar, have, o), k = 0;
    if (n <= 0) return -1;
    while (k < n && o[k] != pchar[slot]) k++;
    if (k == n) k = 0;  // (the one in force is not offered: as if on the home member)
    if (dir == 0) return n > 1 ? o[(k + 1) % n] : -1;
    k += dir > 0 ? 1 : -1;
    return k >= 0 && k < n ? o[k] : -1;
}

// ---- the old live keys "ppick 0|1" / "dbmparty 0|1" (s217): 0 = Donald, 1 = Riku in Donald's place
enum { PPICK_DONALD, PPICK_RIKU };
static inline int pchar_of_pick(int v) { return v == PPICK_RIKU ? PCH_RIKU : PCH_DONALD; }
// what slot `slot` gets for a wanted character: the wanted one when it may stand there and its data loads, else the slot's own
static inline int pchar_fallback(int want, int slot, bool data_ok) {
    if (!pchar_can(want, slot) || !data_ok) return PCH_HOME[slot < 0 || slot >= PCH_SLOTS ? 0 : slot];
    return want;
}
// (s220 riku2) the name the pause menu's member list shows for slot `slot`: NULL = the slot's own member (KH2's own name text, msg
// key 31 / 32 as before), else the stand-in's name (a literal text, like Roxas' s111)
static inline const char *pchar_menu_name(const int pchar[PCH_SLOTS], int slot) {
    if (slot < 0 || slot >= PCH_SLOTS || pchar_home(pchar, slot) || !pchar_valid(pchar[slot])) return NULL;
    return PCH_TAB[pchar[slot]].name;
}

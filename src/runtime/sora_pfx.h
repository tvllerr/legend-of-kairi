// sora_pfx.h: (s223 rikufx) the party stand-ins' OWN KH2 weapon effects: the hit spark KH2 picks by the attack row's EffectOnHit and
// the swing ribbon of their weapon (recon/s223/S223_HELPER_RIKUFX.md). Pure (no game, no sora.c): included by sora_party.h (its glue
// draws them), host-tested by runtime/test/party_test.c.
//
// The pack rows come from tools/sorabuild/fx/partyfx.py (appended to fx.bin from our fxrec recording of the player's own
// W_EH000 / W_HE000 / W_MU000 / W_AL000 / W_CA000_HUMAN .a.us):
//   pfx_<key>_hit<k>  the element KH2's spark selector picks for EffectOnHit k (re:0x3D1B40 -> re:0x2C6730: the first PAX record of
//                     the attack owner's set, here his weapon's, whose category holds (1 << 32 << (k - 1)) | 1); missing = KH2 has none
//   pfx_<key>_trail   his weapon set's element 10 (the ribbon look: texture + blend + colour, as Sora's kb<xx>_trail)
//   trail windows     fx.bin's trail table at pfx_trail_key(character, mset slot) = his weapon mset's range trigger 12
// Donald and Goofy have no row: they keep the s54 sparks ("hit" / "hit_finish" = the Kingdom Key's) and no ribbon, as before.
// An fx.bin without the rows (older pack) = every stand-in as before s223 (base sparks, no ribbon); one log line per character.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define PFX_TRAIL_BASE 200000  // = partyfx.py TRAIL_BASE (Sora's forms use slot + 10000 * (form + 1) <= 80000 + slot)
#define PFX_NEOH 3             // EffectOnHit 1..3

typedef struct {
    int pch;          // sora_pchar.h PCH_* (partyfx.py ROWS carry the same ids)
    const char *key;  // pack name part: pfx_<key>_hit<k> / pfx_<key>_trail
} PfxRow;
static const PfxRow PFX_TAB[] = {
    {PCH_RIKU, "riku"},        // W_EH000 (Way to the Dawn): EffectOnHit 1 -> element 1, 2 -> element 2
    {PCH_AURON, "auron"},      // W_HE000: 1 -> element 1 (no EffectOnHit-2 element: KH2 draws none there; none of his rows has 2)
    {PCH_MULAN, "mulan"},      // W_MU000: as Auron
    {PCH_ALADDIN, "aladdin"},  // W_AL000: as Auron
    {PCH_SPARROW, "sparrow"},  // W_CA000_HUMAN: 1 -> element 1, 2 -> element 2 (GUESS: the _HUMAN set; its PAX = W_CA000's)
    // Cloud (N_HB550_BTL, sword inside the body model): not here. His attacks are his BODY mset's, so KH2 picks from
    // N_HB550_BTL.a.us (EffectOnHit 1 -> element 1, 2 -> element 4); his body mset has NO range-12 trail window: his swing effects
    // are elements his motions START by frame trigger 1 (50A @20 -> element 0 = a bone-239 loop, 51A @28 -> 2 + 3, 52A -> 5 / 8 ...),
    // like Final Form's sora_ffx.h: separate work (recon/s223/S223_HELPER_RIKUFX.md).
};
#define PFX_N ((int)(sizeof PFX_TAB / sizeof PFX_TAB[0]))

static inline int pfx_row(int c) {
    for (int r = 0; r < PFX_N; r++)
        if (PFX_TAB[r].pch == c) return r;
    return -1;
}
// the pack name of character c's spark for EffectOnHit eoh (1..3); false = no row (Donald, Goofy, Cloud, bad input)
static inline bool pfx_hit_name(int c, int eoh, char *b, size_t n) {
    int r = pfx_row(c);
    if (r < 0 || eoh < 1 || eoh > PFX_NEOH || !b || n < 2) return false;
    return snprintf(b, n, "pfx_%s_hit%d", PFX_TAB[r].key, eoh) < (int)n;
}
static inline bool pfx_trail_name(int c, char *b, size_t n) {
    int r = pfx_row(c);
    if (r < 0 || !b || n < 2) return false;
    return snprintf(b, n, "pfx_%s_trail", PFX_TAB[r].key) < (int)n;
}
// the fx.bin trail-table key of character c's mset slot (sora_fx_trail_window); -1 = no row / bad slot
static inline int pfx_trail_key(int c, int slot) { return pfx_row(c) >= 0 && slot >= 0 && slot < 1000 ? PFX_TRAIL_BASE + 1000 * c + slot : -1; }
// The exe ribbon party slot i draws on: the TOP two of the exe's nslots (sora_fx_trail_slots; 0 / 1 = Sora's blades, 2 + 2k / 3 + 2k
// = remote player k's): slot 0 -> nslots - 1, slot 1 -> nslots - 2. -1 = none (an exe with 2 / no ribbon slots for them).
// sora_net_avatar.h av_ribbon_busy() says when a remote player uses that one (4 players): he wins, the stand-in draws no ribbon.
static inline int pfx_ribbon_slot(int i, int nslots) {
    int r = nslots - 1 - i;
    return i >= 0 && i < 2 && r >= 2 ? r : -1;
}

// (s289 duelreflect, duel plan 2.1: recon/s289/S289_HELPER_DUELPLAN.md) Reflect's burst against the duel opponent: the pure
// parts, host-tested by runtime/test/duel_test.c. The game side is du_reflect_burst (sora_duel.h), called by sora_magic2.h's
// mg_reflect_burst_hits after its enemies, with the same cylinder and the same burst clock (MgReflect.bt, KH frames).
//
// KH2 data (VERIFIED, 00battle.bin atkp rows; sb/s289/duelplan/rows.txt; the numbers live in sora_magic2.h MG_REF_*):
//   burst A = rows 574 / 576 / 578 (burst tier 1 / 2 / 3): Power 50, EnemyReaction 3 / 4 / 4, Interval 10 / 8 / 6 KH frames at
//             SubId 0 / 512 / 1024 (= Reflect / Reflera / Reflega, the spell tier);
//   burst B = rows 667 / 668 / 669: Power 550, EnemyReaction 6 / 6 / 7, Interval 0 (each target once).
// The meaning of the s133 script fields (A lives 28 KH frames, then B 6; the tier from the block count) is from the s133 notes in
// sora_magic2.h: GUESS until checked in the KH2 exe.
//
// The opponent, as an enemy (mg_reflect_burst_hits + mg_rehit_due re:0x3CF600): his drawn feet inside the cylinder (horizontal R,
// -H .. 2H around the caster's feet; GUESS vertical window, Fire's ring convention) -> A hits at the first contact and again each
// time the burst clock passes the Interval since his last A hit; B hits him once at the end. B is the duel's finisher (NDH_FINISH:
// duelrag knocks him down; GUESS: KH2's reaction 6 / 7 is a blow-away). Wire: a HIT with src 7 "Reflect" (NDH_SPELL), no new op,
// NET_VERSION 12; an older game takes it as damage (its log calls it "Thunder").
// The numbers stay in ONE place, sora_magic2.h's MG_REF_* tables: the game passes them in; the host test uses the rows' values.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sora_net_proto.h"
#include "sora_duel_core.h"

typedef struct {
    bool a_on;     // he took an A hit in this burst: a_next = when the next one is due (burst clock, KH frames)
    float a_next;
    bool b_done;   // he took the B hit of this burst
    int na, nb;    // A / B hits in this burst (the log line)
} DuRefl;
enum { DU_REFL_NONE = 0, DU_REFL_A = 1, DU_REFL_B = 2 };

// one mg_reflect_burst_hits call for the opponent. bt = the burst clock (MgReflect.bt), second = the B call, iv = the A row's
// Interval for the spell tier, inside = his drawn feet are in the cylinder this call (and a duel fight runs). -> DU_REFL_*.
// A NEW burst: an A call at bt 0 (mg_reflect_tick sets bt = 0 when the barrier ends and makes the first call; every later call
// comes after bt += kh, and kh > 0: KH_PHYS_STEP 2 x the frame ratio, at least 0.1). The reset happens even when he is outside.
static inline int du_refl_step(DuRefl *s, float bt, bool second, float iv, bool inside) {
    if (!second && bt <= 0.0f) memset(s, 0, sizeof *s);
    if (!inside) return DU_REFL_NONE;
    if (second) {
        if (s->b_done) return DU_REFL_NONE;  // (mg_reflect_tick makes one B call per burst: belt and braces)
        s->b_done = true, s->nb++;
        return DU_REFL_B;
    }
    if (s->a_on && bt < s->a_next) return DU_REFL_NONE;  // (mg_rehit_due: "bt < next" = not yet; then next = bt + iv)
    s->a_on = true, s->a_next = bt + (iv > 0 ? iv : 0), s->na++;
    return DU_REFL_A;
}
// (the cylinder: sora_duel_core.h du_body_near(caster's feet, his feet, R, -H, 2H), the same test as mg_reflect_burst_hits' enemies;
//  a NaN pose fails every comparison there = outside)
// the HIT's body for one burst hit (du_hit_out fills op / id / hseq / from / at): Power and the row from sora_magic2.h's tables,
// S = the caster's Mag + Keyblade Mag (a spell, as du_spell), no boost (the mod's ability table has Fire / Blizzard / Thunder
// Boost only; GUESS: KH2 has none for Reflect, not checked in its data), B = the finisher
static inline void du_refl_hit(NetDuelMsg *m, bool b, int power, int atkp, int react, int stat) {
    m->power = (uint16_t)(power < 0 ? 0 : power > 65535 ? 65535 : power);
    m->src = DU_SRC_REFLECT;
    m->flags = (uint8_t)(NDH_SPELL | (b ? NDH_FINISH : 0));
    m->react = (uint8_t)(react < 0 ? 0 : react > 255 ? 255 : react);
    m->atkp = (uint16_t)(atkp < 0 ? 0 : atkp > 65535 ? 65535 : atkp);
    m->stat = (uint16_t)(stat < 0 ? 0 : stat > 65535 ? 65535 : stat);
    m->boost = 0;
}

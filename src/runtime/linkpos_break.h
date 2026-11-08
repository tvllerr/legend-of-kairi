// s55 breakables (recon/S55_HELPER_BREAK.md): BotW actors whose DamageParam says Breakable (wooden boxes, barrels,
// pots, ore, boards, cracked walls, ...). linkpos.c keeps the list from the opacity hook; sora.c finds the two
// functions with GetProcAddress (so the hot-reload DLL still loads on an exe linked before s55).
#pragma once
#include <stdint.h>

enum { BRK_SWORD, BRK_LARGESWORD, BRK_SPEAR, BRK_ARROW, BRK_BOMB, BRK_BODY, BRK_ANCIENT, BRK_SHOCKWAVE, BRK_IMPULSE,
       BRK_GANONBEAM, BRK_NRATE };  // (DamageParam DamageRate order = res::DamageParam::DamageSource)

typedef struct {
    uint64_t actor;
    float pos[3];           // feet (mMtx translation), BotW units
    int32_t life, life_max; // now / the most seen (its GParam Life when first seen undamaged)
    float rate[BRK_NRATE];  // DamageRate by source
    uint8_t lightning;      // DamageParam LightningAffect
    char name[48];
} LinkposBreakable;

typedef int (*LinkposBreakablesFn)(LinkposBreakable *out, int max);  // "linkpos_breakables": copies, any thread
typedef int (*LinkposBreakHitFn)(uint64_t actor, int n);              // "linkpos_break_hit": life after, -1 refused

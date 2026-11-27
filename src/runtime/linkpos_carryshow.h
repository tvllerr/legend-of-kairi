// (s291 popin) linkpos.c carry_obj_shown keeps a liftable (barrel / pot / rock / orb ...) visible while Link is hidden: a carried
// object takes its holder's opacity, and hidden Link's is 0 (s217). Its old rule = EVERY liftable within CARRY_SHOW_DIST of Link
// whose opacity is being lowered, held or not, so BotW's own fades of the pots / barrels standing near him were forced to 1 every
// frame: they popped instead of fading (s226 review R3, GLITCHES s226).
// Held-only rule (the DLL's live key carryheld 1, asked every frame through the exe export linkpos_carry_show_held): only the object
// in Link +0x88 (VERIFIED: BotW's grab actions read the carried object there, s217 decompile; live 0.6.0 lab log s290 "[carry] holds
// ... (Link +0x88 ...)") is kept visible, plus the last held one for CARRYSHOW_GRACE frames after it left his hands (Link +0x88 = 0
// at once on a throw / put down, same log: GUESS its fade back to BotW's 1 may still be running then). Any other liftable near
// hidden Link keeps BotW's own opacity. Key off (or an old DLL) = the old rule, unchanged.
// Pure (no guest memory): host test runtime/test/carryshow_test.c. Any thread: the two fields are written racily (cosmetic only;
// a torn pair = at worst one frame of BotW's own value, or of the forced 1).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define CARRYSHOW_GRACE 60  // frames the last held object stays forced visible after Link +0x88 let go of it (GUESS, ~1 s)

typedef struct { volatile uint64_t last, at; } CarryShowMem;

// a = the actor at the opacity apply (already a liftable near hidden Link being lowered), held = Link +0x88 now (0 = nothing,
// or the object is being deleted), frame = the exe's frame counter. true = force its opacity to 1.
static inline bool carryshow_keep(CarryShowMem *m, uint64_t a, uint64_t held, uint64_t frame) {
    if (held) m->last = held, m->at = frame;
    if (!a) return false;
    if (a == held) return true;
    uint64_t last = m->last, at = m->at;
    return a == last && frame >= at && frame - at <= CARRYSHOW_GRACE;
}

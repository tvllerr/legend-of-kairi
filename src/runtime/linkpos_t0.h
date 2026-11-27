// linkpos_t0.h (s291 ridestart, exe side, pure C): when "Link is in the world" (linkpos.c g_t0, linkpos_world_frame())
// is set for the first time. The whole Sora mode (sora.c sora_mod_frame_body), the live file, the Melty room's "in the
// world" and the KH2 HUD wait for it.
// Bug (s291e lab, sb/s291/LAB_RESULTS.md "## Room start"): g_t0 needed Link's ground bit, so a save made while riding
// (state word 0x100000, no ground bit) never started the mod until he got off: 6+ min of plain Link, "paused/no world".
// The ground rule dates from the Savage Wind port (4d9c84a5); its only job is "not on the title / the load": BotW's
// title has state word 0 (61 boots, sora_title.h), the save load 0x1 (linkpos.c state notes), neither has bit 0x100000.
// Rule (ride on): the ground bit as before, OR the riding bit 0x100000 (sora.c LINK_SW_RIDING: the mount through the
// ride, left via 0x600002 / 0x100010; also on a Lynel's back - GUESS from the epictest logs, lab s291 "[labhorse] Link
// RIDES (state 0x100000)"). Ride off = exactly the old rule. Other non-ground states (0x1 load, 0x2 / 0x90 climbing,
// swimming, gliding) stay out: 0x1 IS the load, and the others are not seen at a save load (GUESS: BotW saves there?).
// Host test: runtime/test/t0ride_test.c.
#pragma once
#include <stdint.h>

#define T0_SW_GROUND 0x08000000u  // (= linkpos.c STATE_GROUND)
#define T0_SW_RIDING 0x100000u    // (= sora.c LINK_SW_RIDING)

enum { T0_NO = 0, T0_GROUND = 1, T0_RIDING = 2 };

// have_actor: Link's actor pointer is known (linkpos state_of(0) = LINK_NONE); sw = his state word (actor +0xcec);
// ride = linkpos_set_t0_ride's value (0 off). The caller also needs the servo body (g_vel_head), as before.
static inline int t0_start_kind(int have_actor, uint32_t sw, int ride) {
    if (!have_actor) return T0_NO;
    if (sw & T0_SW_GROUND) return T0_GROUND;
    return ride && (sw & T0_SW_RIDING) ? T0_RIDING : T0_NO;
}


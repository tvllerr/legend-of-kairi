// sora_net_flip.h (s290 flip): the room keys whose compiled default went ON in s290 (the user: "Flip the passes ON", after the
// two-copy lab sb/s290/LAB_RESULTS.md: netobjs, roomnoauto, netgear, netshots, netrunes here; netloot, revive, mgelemclaim and the
// duel keys in their own files). A live "set <key> 1" logs the key's line with its exe check (sites ok / NEEDS EXE RELINK); a
// compiled default never ran that step, so this says it ONCE per DLL copy, at the first frame a Melty room is up (alone in it too):
// netobjs (no_bind: breaks, moves on / OFF + why: an old exe without the s289 rows breaks nothing new and moves nothing),
// roomnoauto (na_bind: sites ok / none = nothing will be written). netloot says its own at its first tick (NETL.said, "[loot]
// netloot 1: ..."); revive its missing exports at its first world frame (rv_bind); netgear / netshots / netrunes need no exe row
// (display only). A key set to 0 before that frame shows as 0. No room (single player, the solo build, a UDP session): no bind,
// no line. Included by sora_net.h right after sora_net_objs.h; net_flip_frame from net_frame. Reads only: nothing is written.
#pragma once

static bool g_net_flip_said;  // the line went out (once per DLL copy: a hot reload says it again in the next room frame)

static void net_flip_frame(void) {
    if (g_net_flip_said || !NET.on || !NET.melty || !sora_melty_connected()) return;
    g_net_flip_said = true;
    char objs[200];
    if (!g_net_objs) snprintf(objs, sizeof objs, "0");
    else {
        no_bind();
        snprintf(objs, sizeof objs, "1 (breaks; %s)", no_moves_txt());
    }
    char na[220];
    if (!g_net_noauto) snprintf(na, sizeof na, "0");
    else {
        na_bind();
        snprintf(na, sizeof na, "1 (%s)", na_sites_txt());
    }
    LOG("[net] room keys (default ON since s290; set <key> 0 = off): netobjs %s, roomnoauto %s, netgear %d, netshots %d, netrunes %d\n",
        objs, na, g_net_gear ? 1 : 0, g_net_shots ? 1 : 0, g_net_runes ? 1 : 0);
}

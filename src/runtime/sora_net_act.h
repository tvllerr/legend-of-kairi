// (s209 step 2, ours) Which action a KH2 motion slot is, for Melty's ACT / PLAYED notes: the sender says ACT where its
// attack / jump starts (sora.c); a receiver says PLAYED when a remote Sora's SNAP slot turns into one (sora_net_avatar.h).
// Both sides read the same tables, so they agree:
//   attack  the slot's motion id (slot / 4: mset slot = id * 4 + variant) is the motion of an entry of that body's ptya
//           table (sora_params*.h, baked from KH2's 00battle.bin: the attacks sora.c plays at its attack start, from the
//           same table). Not attacks here: magic, Limits, summons, reactions, finishers outside ptya (no ACT for them either).
//   jump    motion id 3 (sora.c MOT_JUMP: the one jump sora.c starts, set_motion(MOT_JUMP)).
// Pure (no game state): host-tested by runtime/test/net_note_test.c.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include "sora_params.h"  // (KhPtya)

enum { NACT_NONE = 0, NACT_ATTACK = 1, NACT_JUMP = 2 };
#define NACT_MOT_JUMP 3  // = sora.c MOT_JUMP

// the action of motion slot `slot` for a body whose attacks are t[0..n): NACT_*; *motion = its motion id (slot / 4)
static inline int nact_kind(int slot, const KhPtya *t, int n, int *motion) {
    if (motion) *motion = slot >= 0 ? slot / 4 : -1;
    if (slot < 0) return NACT_NONE;
    int id = slot / 4;
    if (id == NACT_MOT_JUMP) return NACT_JUMP;
    for (int i = 0; t && i < n; i++)
        if (t[i].motion == id) return NACT_ATTACK;
    return NACT_NONE;
}
static inline const char *nact_name(int kind) { return kind == NACT_ATTACK ? "attack" : kind == NACT_JUMP ? "jump" : ""; }
// the detail both sides write: "m<motion id>" (the receiver from the slot, the sender from its motion id: the same text)
static inline void nact_detail(char *out, int cap, int motion) { snprintf(out, (size_t)cap, "m%d", motion); }

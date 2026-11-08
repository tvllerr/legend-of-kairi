// s59 fire world (recon/S59_HELPER_FIRE.md): Sora's / Donald's Fire sets BotW things alight through BotW's own
// chemical system - campfires, cooking pots, torches, lamps, burnable ivy (their chemical set to burning, the way the
// game's own "ignite" message does it), enemies (the same call: BotW's burn reaction + burn damage), flint sparks
// (the Flint action's spark element) and, opt-in, a lightning-strike fire burst at a point (grass fires).
// linkpos.c keeps the list of fire objects from the opacity hook; sora.c finds these by GetProcAddress, so the
// hot-reload DLL still loads on an exe linked before s59 (then: "fire world: not in this exe").
#pragma once
#include <stdint.h>

typedef struct {
    uint64_t actor;
    float pos[3];  // feet (mMtx translation), BotW units
    char name[48];
} LinkposFireObj;

typedef int (*LinkposFireObjsFn)(LinkposFireObj *out, int max);           // "linkpos_fire_objects": copies, any thread
typedef int (*LinkposFireIgniteFn)(uint64_t actor);                         // "linkpos_fire_ignite": 1 lit, 2 asked, 0 no, -1 refused
typedef int (*LinkposFireSparkFn)(const float p[3], float radius, float life);  // "linkpos_fire_spark": 1 sent, -1 refused
typedef int (*LinkposFireBurstFn)(const float p[3]);                         // "linkpos_fire_burst": 1 sent, -1 refused

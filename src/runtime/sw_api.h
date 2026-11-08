// s175 "idea 1": the exe's primitives for the hot-reloaded Sora DLL (runtime/sora*.c), so NEW feature logic lives in the
// DLL (hot reload) instead of linkpos.c (relink + restart). ONE versioned table, one GetProcAddress / dlsym:
//     const SwApi *api = ((const SwApi *(*)(uint32_t))GetProcAddress(GetModuleHandleA(NULL), SW_API_EXPORT))(1);
// NULL (an exe before s175, or a wanted version above SW_API_VERSION) = the DLL keeps its old path.
// Fields are only ever ADDED at the end (version stays 1 while that holds); a field at offset >= api->size is absent.
// Plan + rules: D:/SoraBotW/docs/specs/2026-11-07-idea1-exe-primitives-plan.md
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cpu.h"

#define SW_API_VERSION 1
#define SW_API_EXPORT "sw_api"
#define SW_HAS(api, field) ((api) && offsetof(SwApi, field) + sizeof((api)->field) <= (api)->size && (api)->field)

// ---- guest calls ----
// The exe saves the whole Cpu, carves `scratch` zeroed bytes (16-aligned, <= SW_SCRATCH_MAX) below the live guest sp, copies
// `init` to the scratch start, sets sp below the scratch, loads x0..x(nx-1) / s0..s(nv-1), calls, copies the first `out_len`
// scratch bytes to `out`, fills SwRet, and restores the Cpu. x_scratch bit i: x[i] is an OFFSET into the scratch and becomes
// that address (e.g. x1 = a zeroed InlineParamPack, a Matrix34f, a SafeString built in `init`).
#define SW_SCRATCH_MAX 0x1000
typedef struct {
    uint64_t x[8];
    uint32_t s[4];           // float bits for s0..s3 (v regs: low 32 bits, rest 0)
    uint8_t nx, nv;          // how many x / s to load (the others keep the caller's values)
    uint8_t x_scratch;       // bit i: x[i] = scratch address + x[i]
    uint8_t pad_;
    uint16_t scratch;        // bytes (0 = none)
    uint16_t init_len, out_len;
    const void *init;
    void *out;
} SwArgs;
typedef struct {
    uint64_t x0, x1;
    uint32_t s0;             // float bits
} SwRet;

// ---- AI tree ----
typedef struct {
    uint64_t node, vt;       // the node, its vtable
    uint16_t idx;            // AIProgram index (+0x18), 0 = none
    uint8_t depth;           // 0 = RootAi
    uint8_t leaf;            // 1 = getType != 0 (an Action / Behavior)
} SwAiNode;

// ---- hooks (hot-reload safe) ----
// A vtable slot is pointed at one of SW_HOOKS exe-owned HLE stubs (registered at boot; HLE pcs never enter the per-thread
// dispatch cache, so this works at run time, like the s38 animal damage stubs). The stub calls fn, or the original when fn is
// NULL. fn REPLACES the slot: call api->hook_orig(c, id) inside it to run the original (with c as it is then).
// Runs on ANY guest thread (RootAi::calc etc. run on workers): fn must be thread-safe and must not call api->call with c == NULL.
typedef void (*SwHookFn)(Cpu *c, int id, void *user);
#define SW_HOOKS 32

// ---- taps: fixed points in exe code that call the DLL while set (NULL = the exe's own tested path) ----
// SW_TAP_PUPPET_ATTACK: linkpos_esync_puppet, after setMtx and before the AS loop, IN PLACE OF esx_atk_frame (s174 R2).
//   int fn(Cpu *c, uint32_t hid, uint64_t actor, const void *target /* EsState / LinkposEsState */, int alive)
//   returns 1 while the DLL's local attack runs (the exe then marks slot 0 applied without restarting the owner's AS).
//   When the tap is set while the exe runner has an attack running, the exe ends that one first (leave_).
typedef int (*SwTapPuppetAttackFn)(Cpu *c, uint32_t hid, uint64_t actor, const void *target, int alive);
#define SW_TAP_PUPPET_ATTACK 0
// SW_TAP_PUPPET_AI (s181): esx_rootai_hook (RootAi::calc, the game's own AI phase, ANY worker thread), for an actor whose brain esync
//   froze, right where the exe skips the brain. void fn(Cpu *c, uint64_t actor, uint64_t root_ai). c = that worker's Cpu (guest calls
//   with c are fine; the exe restores nothing after fn: use api->vcall / call, which save + restore it). Several workers may call fn
//   at once for DIFFERENT actors: fn must lock per actor. After tap_set(.., NULL) a worker may still be inside fn (the old DLL copy is
//   never unloaded, so its code stays valid). Only exes with bit SW_TAP_PUPPET_AI in api->taps_called call it.
typedef void (*SwTapPuppetAiFn)(Cpu *c, uint64_t actor, uint64_t root_ai);
#define SW_TAP_PUPPET_AI 1
#define SW_TAPS 8

typedef struct SwApi {
    uint32_t version, size;  // SW_API_VERSION, sizeof(SwApi) in the exe

    // memory: any thread, never faults (readable() = linkpos.c's, with its commit-chunk cache)
    bool (*in_guest)(uint64_t a, size_t n);
    size_t (*readable)(uint64_t a, size_t n);                 // how many of the n bytes can be read
    bool (*read)(uint64_t a, void *out, size_t n);            // all n readable -> copied
    bool (*write)(uint64_t a, const void *in, size_t n);      // committed READWRITE guest pages only (no VirtualProtect)

    // game state
    bool (*in_frame)(void);               // inside linkpos_frame (sora_mod_frame runs there): c == NULL calls allowed
    uint64_t (*frame_id)(void);           // linkpos frame counter (g_vel_frame_id)
    uint64_t (*site)(const char *name);   // a game_sites entry for this game version by name (e.g. "atkact_attack_vt"), 0 = none
    uint64_t (*fn_at)(uint64_t guest);    // the recompiled function containing a guest address, 0 = none
    const char *(*actor_name)(uint64_t actor);  // linkpos actor_name ("" when unreadable)

    // guest calls. c == NULL: kernel_current_cpu(), only when in_frame(). false = refused (no Cpu, no function there, bad args)
    bool (*call)(Cpu *c, uint64_t fn, const SwArgs *a, SwRet *r);
    bool (*vcall)(Cpu *c, uint64_t obj, int slot, const SwArgs *a, SwRet *r);  // fn = (*obj)[slot] (readable-checked), x0 = obj

    // AI tree (getType is a guest call -> a Cpu as above). Only nodes whose node+0x8 (ACTION_ACTOR) == actor, like esync.
    int (*ai_walk)(Cpu *c, uint64_t actor, SwAiNode *out, int max);  // every DISTINCT node under RootAi (shared nodes once,
                                                                     // <= 4000 visited, 2048 stack); returns how many stored
    bool (*ai_leaf)(Cpu *c, uint64_t actor, SwAiNode *out);          // the running leaf: child-index chain from RootAi, <= 32 deep

    // hooks
    int (*vt_hook)(uint64_t vt, int slot, SwHookFn fn, void *user, const char *name);  // id >= 0; -1 = all used / bad slot;
                                                                     // the same vt+slot again = same id, fn / user replaced
    void (*vt_unhook)(int id);                                       // the original goes back into the slot
    void (*hook_orig)(Cpu *c, int id);

    // taps
    bool (*tap_set)(int tap, void *fn);                              // false = unknown tap

    // esync (multiplayer enemies)
    bool (*esync_actor)(uint32_t hid, uint64_t *actor, int *puppet);  // the loaded actor of a hid; puppet = its brain frozen by us

    // s181: bit t set = this exe really calls tap t (tap_set accepts any t < SW_TAPS, also ones an older exe never calls)
    uint32_t taps_called;
} SwApi;

#ifdef __cplusplus
extern "C" {
#endif
// The exe's one export for this (runtime/linkpos_api_block.inc). NULL when want > SW_API_VERSION.
const SwApi *sw_api(uint32_t want);
// exe only: sora_host.c calls it when it switches to a new DLL copy (before the new copy's first frame) - every vt hook is
// unhooked (original back in its slot) and every tap cleared, so nothing points at the old copy's logic. Also RT_ON_RESET
// (game reboot: the guest memory is wiped, the table is forgotten without writing the slots).
void sw_api_dll_switch(void);
#ifdef __cplusplus
}
#endif

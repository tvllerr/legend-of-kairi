// Sora-in-BotW: KH2's own visual effects for what Sora does (runtime/sora_fx.c, exe side; the mod DLL calls these
// through the exe's import library like linkpos_* / sora_sfx_*). Recon: D:/SoraBotW/recon/S35_HELPER_X_FX.md
//
// Data: D:/SoraBotW/data/fx/fx.bin (tools/fxbake.py): KH2's effect elements as Project Heart Hunters recorded them
// from KH2's own effect runtime (per 60 Hz frame: textured triangle batches, world point + camera-space offset per
// vertex, KH2's blend state) with KH2's HD effect textures. Loaded on the first call; a no-op without it.
//   SAVAGEWIND_SORA_FX=<path>|0     fx.bin (default D:/SoraBotW/data/fx/fx.bin; 0 = no effects)
//   SAVAGEWIND_SORA_FX_LOG=1        log every spawn / end
//   SAVAGEWIND_SORA_FX_FLIPZ=1      mirror the recorded z of every element (chirality test, see the report)
//   SAVAGEWIND_SORA_FX_BIAS=<u>     scene-depth bias for the effects (default 0.05)
//   SAVAGEWIND_SORA_FX_TRAIL=0      no Keyblade swing ribbon
#ifndef SORA_FX_H
#define SORA_FX_H
#include <stdbool.h>
#include <stdint.h>

// ---- game thread (sora.c): spawn / drive. Positions in BotW world units (the space of Link's matrix), yaw = Sora's
// facing in rad (direction (sin, cos) in BotW x, z, as sora_draw_pose's <face>), scale = u per KH cm (0.01 * g_k).
// An element by pack name: "hit" "hit_finish" "hit_guard" "keyblade_appear" "body_hit" "body_hit_finish",
// "fire_cast" "fire_shot" "fire_ring" "fire_hit" "fire_hit_last", "blizzard_cast" "blizzard_shot" "blizzard_shard"
// "blizzard_hit" "blizzard_hit2", "thunder_cast" "thunder_shot" "thunder_bolt" "thunder_bolt_air" "thunder_hit",
// "cure_cast" "cure_shot" "cure_ring" "cure_flower" "cure_sparkle" (tools/fxbake.py ELEMENTS). -1 = unknown.
int sora_fx_id(const char *name);

// Starts element `id` at `pos` facing `yaw`. Bound elements (the cast glows, the Fire ring, Cure's flower: KH2 binds
// them to Sora / his Keyblade tip) follow sora_fx_move; the others stay where they were spawned (a particle born
// on a frame keeps the owner pose of that frame, as in KH2). Returns a handle (>= 0) or -1. 32 slots; when all are
// busy the oldest one-shot gives way.
int sora_fx_spawn(int id, const float pos[3], float yaw, float scale);
int sora_fx_event(const char *name, const float pos[3], float yaw, float scale);  // = spawn(sora_fx_id(name), ...)
void sora_fx_move(int h, const float pos[3], float yaw);  // the owner's place this frame (bound elements, shards)
void sora_fx_kill(int h);                                  // gone at once (KH2's effect_kill)
void sora_fx_fadeout(int h, float frames);                 // KH2's effect_fadeout: alpha to 0 over `frames`, then gone
bool sora_fx_alive(int h);
void sora_fx_tick(float kh);                               // once per frame, BEFORE sora_draw_pose / _world of the
                                                           // same frame: advances every element by `kh` KH frames
                                                           // and hands the frame's triangles to the render thread
void sora_fx_clear(void);                                  // everything off (Sora mode off, pause)

// The Keyblade swing ribbon (an approximation: KH2 draws its blade trail in the engine inside the motion's
// weapon_trail windows, there is no PAX element for it). Call once per frame with the blade's grip and tip in world
// units while a window is open (sora_fx_trail_window), `on` false otherwise (the ribbon fades out over its length).
void sora_fx_trail(const float grip[3], const float tip[3], bool on);
bool sora_fx_trail_window(int slot, float clock);  // KH2's weapon_trail range of mset slot `slot` covers `clock`
// (s89 kbfx) ribbon r (0 = the main blade, = sora_fx_trail; 1 = Valor's left blade), drawn in the look of element
// `style` (a Keyblade's "kb<xx>_trail" from sora_fx_id: KH2's W_EX010_<xx>.a.us element 10 texture + blend + colour;
// -1 = the s35 white gradient). flags bit 0: the texture's v 0 at the grip (default: at the tip, GUESS). The DLL
// resolves it with GetProcAddress (an exe before s89 has only sora_fx_trail).
void sora_fx_trail_ex(int r, const float grip[3], const float tip[3], bool on, int style, int flags);
// (mp2-perf) the number of ribbons sora_fx_trail_ex takes (4: 2 / 3 = the remote player's blades; a missing export = an exe with 2)
int sora_fx_trail_slots(void);

// (s97 b11-trail) ribbon smoothing: n 0 = straight quads between the per-frame samples (old), 1 = adaptive
// Catmull-Rom subdivision (default), 2..8 = that many sub-steps per segment. Env SAVAGEWIND_SORA_FX_TRAIL_SMOOTH sets it at load.
void sora_fx_trail_smooth(int n);

bool sora_fx_ready(void);
int sora_fx_count(void);  // live elements (for logs)

// ---- render thread: sora_draw.c sora_draw_present, inside its dynamic rendering after Sora's body, before
// vkCmdEndRendering: pc = sora_draw's 32-float push constants of this frame (mvp, k, vp, duv, z), sdepth = the
// scene depth view it bound (VK_NULL_HANDLE: no occlusion), nearest = its nearest sampler.
#ifdef VK_VERSION_1_0
// (s57) fmt: the colour format of that rendering (the window image's or, with the ui order on, the game image's).
void sora_fx_draw(VkCommandBuffer cb, const float pc[32], VkImageView sdepth, VkSampler nearest, VkFormat fmt);
#endif

#endif

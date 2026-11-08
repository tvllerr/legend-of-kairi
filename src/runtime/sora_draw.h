// Sora-in-BotW: Sora's own draw pass (runtime/sora_draw.c, in the exe). Recon: D:/SoraBotW/recon/SAVAGE_WIND_RECON.md
// sessions 10-12 (the bake, the pass, scene-depth occlusion), S35_HELPER_S_SHADOW.md (the blob shadow),
// S35_HELPER_P_PARTY.md section 2 + S54_HELPER_PARTY_DRAW.md (s54, party slice 1: the draw actors).
// sora.c (the hot-reload DLL) declares these inline today; it may include this header instead. New names reach the DLL
// only after the exe's next link (build/savagewind.dll.a learns them then).
#pragma once
#include <stdbool.h>

// ---- game thread (sora.c, end of its frame): what to draw this frame
// Pose of mset slot <slot> at <clock> KH frames (between baked frames: blended), entity at Link's feet facing <face>
// (rad, direction (sin, cos) in BotW x, z), KH cm -> BotW units * <scale>. Unknown slot: the last pose stays.
void sora_draw_pose(int slot, float clock, float face, float scale);
// Nothing drawn until the next pose (paused, no Sora, Link's state OTHER, ...). (s54) ALL actors: the party hides with
// Sora at every old call site (load screens, Game Over, pause); sora_draw_actor_off(0) hides Sora alone.
void sora_draw_off(void);

// The pose in the mod's hands (retargets): bones in the bake, a slot's baked model-space matrices, a draw with edited
// ones (sora_draw.c "the pose in the mod's hands" has the spaces and layouts).
int sora_draw_bone_count(void);
bool sora_draw_slot_pose(int slot, float clock, float *out);
void sora_draw_world(const float *bones, float face, float scale);

// The blob shadow: a soft dark disc flat on the floor under Sora (KH2 style; KH2 itself projects a low-poly shadow
// mesh, there is no shadow texture to borrow). Env SAVAGEWIND_SORA_SHADOW=0 turns it off.
// sora_draw_shadow: the floor's world y under him this frame (BotW units, the space of Link's matrix; a value above
// his feet is clamped to his feet) and the disc's centre alpha on the ground (0..1; <= 0 = none). Call every frame
// BEFORE that frame's sora_draw_pose / sora_draw_world; the values stay until the next call. Never called: the floor
// = his feet (the disc rides with him in the air). The height feet - floor shrinks and fades it:
// sora_draw_shadow_shape: radius on the ground (u), fade_h = the height (u) where it is min_scale of the radius and
// min_alpha of the strength (linear in between, clamped above); <= 0 keeps a value. Defaults: 0.40, 4.0, 0.35, 0.30.
void sora_draw_shadow(float floor_y, float strength);
void sora_draw_shadow_shape(float radius, float fade_h, float min_scale, float min_alpha);
// The body's alpha where the scene depth hides it (0..1, 0 = off; s48: Sora under the water while swimming). Only the
// nearest hidden surface draws (a depth prepass), so it reads as a silhouette, not an X-ray.
void sora_draw_ghost(float alpha);
// A glow over the whole body: rgb 0..1 and amount 0..1 (0 = off; s54: Link's blue in the map warp). Rides with the
// next skinned frame; stays until the next call.
void sora_draw_tint(float r, float g, float b, float amount);

// ---- (s54, party slice 1) draw actors: one baked character each (S35_HELPER_P_PARTY.md 2.2). Sora = actor 0 (the
// env bake, loaded on first use); every function above = its actor-0 call below (sora_draw_off: all actors). Game
// thread, like the rest. An unknown id does nothing (bone count 0, slot pose false).
#define SORA_DRAW_ACTORS 48  // (s146 outfit: 24 -> 48, each Drive-Form body in an outfit is one more actor, sora_outfit.h; the DLL also works with 24 by leaving OF_RESERVE free) (s136: 14 -> 24 for the summons: Chicken Little + Sora call, later Stitch/Genie/Peter Pan) (s72: Sora, Donald, Goofy, Valor, Wisdom + 1 free; s89 trial: + Limit, Master, Final, Anti = 9; s109 Roxas = 10; s110 Mickey = 11; s110 skate: + board, Sora rider, Roxas rider = 14, sora_skate.h)
// Loads a bake pair (the 'SKRG' model + 'SKRP' pose formats of sorabake / partybake, bone counts checked) and returns
// its actor id (1..SORA_DRAW_ACTORS-1), or -1 (no Sora draw: SAVAGEWIND_SORA_DRAW=0 / no Sora bake; a missing or bad
// file; the table full). The same pair again (the DLL's hot reload runs its init again) returns the same id. Its
// textures reach the GPU at the render thread's first present that sees it posed; it draws from the next one.
int sora_draw_actor_load(const char *model_path, const char *pose_path);
// (s110 skate) SORA_DRAW_ACTORS of the exe the DLL runs with (the export is missing in an older exe: GetProcAddress NULL).
int sora_draw_actor_capacity(void);
// (s68 drive) The actor Sora's own calls above drive (0 = base Sora; a loaded form actor while in a Drive Form); the
// switch carries shadow / ghost / tint / weapon settings over and hides the old body. sora_draw_body = the current one.
void sora_draw_set_body(int id);
int sora_draw_body(void);
// Pose of slot <slot> at <clock> for actor <id> with its FEET at world <feet> (BotW units; NULL = Link's matrix, the
// rule of sora_draw_pose) facing <face>, KH cm -> u * <scale>; unknown slot: its last pose stays (per actor).
void sora_draw_actor_pose(int id, int slot, float clock, const float feet[3], float face, float scale);
// The edited-bones draw (sora_draw_world), same placement rule: sora_draw_actor_bone_count(id) x 12 f32, model space.
void sora_draw_actor_world(int id, const float *bones, const float feet[3], float face, float scale);
int sora_draw_actor_bone_count(int id);
bool sora_draw_actor_slot_pose(int id, int slot, float clock, float *out);
void sora_draw_actor_off(int id);  // hidden until its next pose
// Its blob shadow / shape / ghost / tint (as sora_draw_shadow / _shadow_shape / _ghost / _tint, per actor; defaults as
// Sora's).
void sora_draw_actor_shadow(int id, float floor_y, float strength);
void sora_draw_actor_shadow_shape(int id, float radius, float fade_h, float min_scale, float min_alpha);
void sora_draw_actor_ghost(int id, float alpha);
void sora_draw_actor_tint(int id, float r, float g, float b, float amount);
// The weapon groups (KH2 group attr bit 0x100: Donald's staff, Goofy's shield, Sora's Keyblade) shown or skipped: KH2
// hides the party's weapon in the no-weapon motion variants (S35_HELPER_P_PARTY.md 1.2). Rides with the next pose.
void sora_draw_actor_weapon(int id, bool shown);
// (s85) A body baked with alt Keyblades (sorabake SORABAKE_KB_ALT: group attr bits 24..31 = sora_kb.h alt id) draws
// only blade <alt> on chain <chain> (0 = the hand blade on 178, 1 = Valor's left); alt 0 = the bake's default (1 Kingdom
// Key / 2 Star Seeker). Groups without alt bits (old bakes) always draw. Rides with the next pose; sora_draw_set_body
// carries it over.
void sora_draw_actor_blade(int id, int chain, int alt);
// (s58, Goofy Bash's flying shield) The weapon groups drawn DETACHED from the hand: the bake's weapon bones (the last
// ones, hung on the hand: Goofy / Donald 4, Sora's Keyblade 12; found from the ATTR_WEAPON groups) skinned with
// <bones> (<n> = that count x 12 f32, the bake's row layout, in the weapon object's own space, KH cm: e.g. W_EX030
// motion 201 = KH2's Bash flight spin, partybake "wpose") or, NULL, with their current pose made relative to the first
// weapon bone (rigid, no motion of its own), placed at world <pos> (BotW units) turned by <yaw> (rad, as face), KH cm
// -> u * <scale>. Drawn even while sora_draw_actor_weapon(id, false) hides the hand one. pos NULL = back on the hand;
// a wrong <n> = back on the hand (logged once). Rides with the next pose / world draw, stays until changed.
void sora_draw_actor_weapon_at(int id, const float *bones, int n, const float pos[3], float yaw, float scale);

// (s59, the KH2 Game Over: recon/S59_HELPER_GAMEOVER.md) Screen mode: from the next pose / world draw the actors use
// THIS camera (view = 3 x 4 world -> view rows like linkpos_camera_view, proj = near, far, fovy (rad), aspect (<= 0:
// the game image's)) instead of BotW's, so the mod places them in a space of its own (sora_draw_actor_world with feet).
// They are drawn over the window image at present whatever the ui order (over BotW's 2D UI: its Game Over screen),
// without scene depth, blob shadows or KH2 effects, after clear (x0 y0 x1 y1, fractions of the game image; NULL or
// x1 <= x0 = none) is filled black. on = false: BotW's camera again, and no actor draws until its next pose. Game
// thread. The mod DLL looks it up by name (GetProcAddress): exes linked before s59 lack it.
void sora_draw_screen(bool on, const float view[12], const float proj[4], const float clear[4]);
void sora_draw_screen_mask(float x0, float y0, float x1, float y1);  // (s105) screen mode: a hole Sora is not drawn in (fractions of the game image)

// ---- render thread: device.c present_locked, inside the window image's rendering, right after the game image
void sora_draw_present(float vx, float vy, float vw, float vh);
// (s57) The ui order (SAVAGEWIND_SORA_UI_ORDER, default 1; sora_draw.c "where the pass goes"): 1 = the actors into
// the game's final image at BotW's scene composite (under its 2D UI), 0 = over the window image here (the old order).
int sora_draw_ui_order(void);
void sora_draw_set_ui_order(int on);
// (mp2-perf) remote avatar perf cuts, DLL "set drawperf N" (looked up by name): bits 1 C1 bones blended once per publish,
// 2 C2 static index buffer, 4 C3 one vertex copy fewer per side, 8 C4 cached descriptor sets (default 15 = all), 16 = verify
// (one-shot: logs the old vs new memcmp). A cleared bit = the old path. sora_draw_perf: the cut bits now.
void sora_draw_set_perf(int bits);
int sora_draw_perf(void);
// exec.c (the command-list translation, under gpu_lock): a scene pass is wanted this frame / the pass itself, right
// after BotW's scene composite into its final image (a raw UNORM view of it, layout GENERAL, w x h; vp = the
// composite's viewport in it, px), with no rendering open.
bool sora_draw_scene_want(void);
#ifdef VK_VERSION_1_0
void sora_draw_scene_pass(VkImageView view, VkFormat fmt, uint32_t w, uint32_t h, const float vp[4]);
#endif
// A world point (BotW units) -> window px with the camera sora_draw_present drew with this frame (sora_hud_present,
// same thread, after it); false behind the camera or when no Sora frame was drawn.
bool sora_draw_project(const float w[3], float *sx, float *sy);

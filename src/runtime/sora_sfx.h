// Sora-in-BotW: KH2's sound effects for what Sora does (runtime/sora_sfx.c, exe side; the mod DLL calls these through
// the exe's import library like linkpos_* / sora_hud_*). Recon: D:/SoraBotW/recon/S35_HELPER_A_SFX.md
//
// Data: D:/SoraBotW/data/sfx/sfx.bin (tools/sfxpack.py: KH2's SEs as PCM16 + Sora's mset frame triggers + the voice
// table + the system / spell names + the atkp-row hit table). Loaded on the first call; everything is a no-op without it.
//   SAVAGEWIND_SORA_SFX=<path>|0    sfx.bin (default D:/SoraBotW/data/sfx/sfx.bin; 0 = silent)
//   SAVAGEWIND_SORA_SFX_GAIN=<g>    extra gain (default 1); the menu's master volume (g_settings.volume) and
//                                  SAVAGEWIND_AUDIO_GAIN apply too, as they do to the game's own audio
//   SAVAGEWIND_SORA_SFX_LOG=1       log every play
#ifndef SORA_SFX_H
#define SORA_SFX_H
#include <stdbool.h>
#include <stdint.h>

// A sound by name: "cursor" "decide" "submenu" "cancel" "error" "danger" "page" "lock_on" "lock_off" "game_over",
// "fire_cast" "blizzard_cast" "thunder_cast" "cure_cast" (+ every .mag step: "fire_1.3", "thunder_2.2", ...),
// "jump" "land" "swing1".."swing3" "finisher" "air_swing1" "hit" "glide_start" "roll" ...  (tools/sfxpack.py NAMES),
// or a json key ("se/se000.win32.scd#1"). -1 = unknown (logged once per name).
int sora_sfx_id(const char *name);

// Play sound `id` once at `gain` (1 = KH2's own level). Returns a voice handle (>= 0) or -1. 16 voices; the oldest
// one-shot gives way when all are busy.
int sora_sfx_play(int id, float gain);
int sora_sfx_play_pan(int id, float gain, float pan, bool loop);  // pan -1 left .. 0 centre .. 1 right; loop until stop
int sora_sfx_play_at(int id, float gain, const float pos[3]);     // panned / attenuated from Link's position and the camera
int sora_sfx_event(const char *name, float gain);                 // = sora_sfx_play(sora_sfx_id(name), gain)
void sora_sfx_stop(int voice);                                    // stops a handle (a loop); harmless if it ended
bool sora_sfx_playing(int voice);

// A loop by name kept in step with a state: on -> starts it if it is not playing, off -> stops it. One slot per name
// ("danger" = the low-HP alarm, KH2 SE 9, which loops in the data).
void sora_sfx_loop(const char *name, bool on);

// Sora's mset frame triggers (KH2's play_sound / footstep_sound / vsb_voice2 rows of P_EX100.mset, mined per bake
// slot): fires every trigger of `slot` with prev < frame <= clock (motion start: prev = -1). Footstep variants cycle,
// vsb_voice2 rows go through the voice table (priority: a bark only replaces a quieter-priority one still playing).
// Call once per frame from the motion advance with the slot being played. Returns how many sounds started.
int sora_sfx_motion(int slot, float prev_clock, float clock, const float pos[3]);

// A voice bark by vtbl row (4 = attack grunt, 5 = heavy attack / finisher, 6 = ..., 29 / 30 = hurt, 1 = blown away,
// 7 = recover, 3 = item, 8 = form change / summon; see the report). Returns the voice handle or -1 (silent choice).
int sora_sfx_voice(int vtbl);

// The Keyblade hit SE for an attack's atkp row (HitWin.atkp of sora_hits.bin -> KH2's hit table): plays it at pos.
int sora_sfx_hit(int atkp_row, float gain, const float pos[3]);

void sora_sfx_stop_all(void);  // e.g. Sora mode off / game paused
bool sora_sfx_ready(void);     // the pack is loaded and the device opened

#endif

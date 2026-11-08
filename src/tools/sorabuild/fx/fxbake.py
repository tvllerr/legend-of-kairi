#!/usr/bin/env python3
"""Sora-in-BotW: pack KH2's own visual effects for the runtime effect player (runtime/sora_fx.c).

Source (nothing downloaded, nothing decoded from KH2's files here): Project Heart Hunters' effect recording
  <cache>/kh2_fx.bin + kh2_fx.json   'PHF2' v4 (PHH tools/kh2fx_format.py): every KH2 effect element BL2 plays,
                                     recorded from KH2's own effect runtime per 60 Hz frame as textured triangle
                                     batches (world point + camera-space offset per vertex, KH2's blend state) with
                                     the HD textures as KH2 ships them (.dds DXT5 / 32-bit BGRA)
  <cache>/mined/kh2_sounds.kh.json   Sora's weapon-trail windows per mset slot (actors.0.slots[*].trails)
  <cache> = the PHH cache dir (env SB_FXCACHE, old name SB_PHH)

Output: fx.bin ('SKFX' v1, little-endian), the elements Sora needs, frames inflated, textures
decoded to BGRA8 (the format sora_draw.c uploads), vertices slimmed:
  header   u32 'SKFX', u32 version 1, u32 n_tex, u32 n_eff, u32 n_frames, u32 n_batches, u32 n_verts, u32 n_trail,
           u32 tex_off, u32 eff_off, u32 frame_off, u32 batch_off, u32 vert_off, u32 trail_off, u32 pix_off, u32 pix_bytes
  Tex      [n_tex]     u32 w, h, off (bytes into pix), pad                                   (16)
  Eff      [n_eff]     char name[32]; u32 first_frame, frames, loops (0/1), pax_flags; i32 bone; u32 fade;
                       u32 max_verts (biggest frame); u32 pad                                (64)
  Frame    [n_frames]  u32 first_batch, n_batch                                             (8)
  Batch    [n_batches] u16 tex (0xFFFF none); u8 flags (PHF2: 1 offsets, 2 sorted, 4 upright, 8 ribbon); u8 blend
                       (0 additive src-alpha/one, 1 alpha src-alpha/inv-src-alpha, 2 src-alpha/zero);
                       u8 clamp (0 wrap, 1 clamp); u8 pad[3]; u32 first_vert, n_vert; f32 upright[9]   (52)
  Vert     [n_verts]   f32 pos[3] (KH2 effect space: x, y DOWN, z, cm); f32 off[3] (camera space: x right, y down,
                       z forward, cm); f32 uv[2]; u16 age; u16 pad; u8 rgba[4] (KH2's colour x 1.992188, clamped) (40)
  Trail    [n_trail]   u32 slot, start, end                                                  (12)
  pix      BGRA8 texels, each texture at its off

  python -I tools/fxbake.py [--cache <dir>] --out <fx.bin> [--list]
    --list only prints the chosen elements and their sizes.

Which elements, and why (recon/S35_HELPER_X_FX.md has the evidence):
  w_ex010.1 / .2 / .3  Kingdom Key hit sparks: KH2's spark selector (re:0x3D1B40) picks the first element of the
                       held weapon's effect set whose category holds bit (32 + EffectOnHit - 1) | 1: Sora's combo
                       rows have EffectOnHit 1 (-> .1), the finishers 2 (-> .2); .3 (kind 2) for guard-type hits
  w_ex010.0            the Keyblade's appear flash (element 0 of a weapon set, re:0x3EFA60)
  sora.14 / sora.42    Sora's own body-attack sparks (EffectOnHit 1 / 2 of P_EX100.a.us), PHH's HIT_SPARK table
  <spell>_1.0~<f>      the Lv1 cast glow at the Keyblade tip (element 0, bind 0x4003, a loop) with KH2's own
                       loop-end tail at the shot frame f = the cast motion's magic_cast trigger (Fire 22, Blizzard 16,
                       Thunder 18, Cure 18: mined motion_triggers slots 224 / 236 / 248 / 260 type 17)
  <spell>_1.1          the shot burst at the Keyblade tip (element 1, bound)
  fire_1.2             the Fire ring on Sora (PHH spells_kh2 ELEMENTS: fire -> 2); fire_1.3 / .4 its hit sparks
  blizzard_1.2         the shard (trimmed to 64 frames: 30 of flight + 12 of fade); blizzard_1.3 / .4 its hits
  thunder_1.2          the bolt from the sky at the target (ground), thunder_1.3 (air target); thunder_1.4 its hit
  cure_1.2 / .3 / .4   the Cure ring at the feet, the green flower / sparkles on Sora (bind 0x4009 = his centre)
"""
import argparse
import hashlib  # (s89 review dedupe)
import json
import os
import struct
import sys
import zlib
import sys as _sys, os as _os; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__))); import sndsrc  # (s156) sound inputs: recipe tree or PHH cache

CACHE = os.environ["SB_FXCACHE"] if "SB_FXCACHE" in os.environ else os.environ["SB_PHH"]  # fx cache dir (SB_FXCACHE; SB_PHH = old name), passed in by sorabuild (ctx.fxcache)
FXCACHE = os.environ.get("SB_FXDIR") or CACHE  # (s161) dir with kh2_fx.bin / kh2_fx.json: our fxrec recording ("fx": "fxrec") or the PHH cache
TRIGDIR = os.environ["SB_TRIGDIR"]  # dir with trig_W_EX010_RX.json / trig_W_EX200.json  # trig_W_EX010_RX.json from `sorabake trigjson W_EX010_RX`
SORA_ACTOR = "0"

# (pack name, PHF2 element name, frame cap or 0 = all)
ELEMENTS = [
    ("hit", "w_ex010.1", 0), ("hit_finish", "w_ex010.2", 0), ("hit_guard", "w_ex010.3", 0),
    ("keyblade_appear", "w_ex010.0", 0),
    ("body_hit", "sora.14", 0), ("body_hit_finish", "sora.42", 0),
    ("fire_cast", "fire_1.0~22", 0), ("fire_shot", "fire_1.1", 0), ("fire_ring", "fire_1.2", 0),
    ("fire_hit", "fire_1.3", 0), ("fire_hit_last", "fire_1.4", 0),
    ("blizzard_cast", "blizzard_1.0~16", 0), ("blizzard_shot", "blizzard_1.1", 0), ("blizzard_shard", "blizzard_1.2", 64),
    ("blizzard_hit", "blizzard_1.3", 0), ("blizzard_hit2", "blizzard_1.4", 0),
    ("thunder_cast", "thunder_1.0~18", 0), ("thunder_shot", "thunder_1.1", 0), ("thunder_bolt", "thunder_1.2", 0),
    ("thunder_bolt_air", "thunder_1.3", 0), ("thunder_hit", "thunder_1.4", 0),
    ("cure_cast", "cure_1.0~18", 0), ("cure_shot", "cure_1.1", 0), ("cure_ring", "cure_1.2", 0),
    ("cure_flower", "cure_1.3", 0), ("cure_sparkle", "cure_1.4", 0),
    # s55 (LV slice 4), appended so every older id / texture index stays the same (recon/S55_HELPER_BAKE.md):
    # 00effect.bar "system" element 15 = the level-up effect: re:0x3ED190 / re:0x3ECF30 (Sora / party level up) and
    # re:0x3EDF90 (Get Bonus, via re:0x3ED350) call re:0x2C6B30(system set re:0x3F6590, 15, ...); bind 0x4009 (body
    # centre), PAX flags 0xE45 (Bind + BindOnlyPos ...), 161 frames, its own SE = element +0x18 = 25 (se000 #25)
    ("level_up", "system.15", 0),
    # element 16 = the same kind of effect for a Drive Form level up (re:0x3E8380, fmlv) and a summon level up
    # (re:0x40D530), both via re:0x3ED390; 141 frames, SE 26 (se000 #26)
    ("form_level_up", "system.16", 0),
    # s70 (Drive change, S64_HELPER_DRIVE_RECON.md 3), appended: CHANGEFORM slot 210 frame 0 = sora.1 (the Drive sphere,
    # root) + sora.2..13 (limb glows, 600 f loops on bones); Valor entry slot 215 = valor.0..12 + valor.100..102; the
    # revert = sora.15..27 on base Sora. The DLL binds each to its bone (sora_form.h FX_*).
    ("drive_sphere", "sora.1", 0),
    ("drive_glow_2", "sora.2", 0),
    ("drive_glow_3", "sora.3", 0),
    ("drive_glow_4", "sora.4", 0),
    ("drive_glow_5", "sora.5", 0),
    ("drive_glow_6", "sora.6", 0),
    ("drive_glow_7", "sora.7", 0),
    ("drive_glow_8", "sora.8", 0),
    ("drive_glow_9", "sora.9", 0),
    ("drive_glow_10", "sora.10", 0),
    ("drive_glow_11", "sora.11", 0),
    ("drive_glow_12", "sora.12", 0),
    ("drive_glow_13", "sora.13", 0),
    ("valor_enter_0", "valor.0", 0),
    ("valor_enter_1", "valor.1", 0),
    ("valor_enter_2", "valor.2", 0),
    ("valor_enter_3", "valor.3", 0),
    ("valor_enter_4", "valor.4", 0),
    ("valor_enter_5", "valor.5", 0),
    ("valor_enter_6", "valor.6", 0),
    ("valor_enter_7", "valor.7", 0),
    ("valor_enter_8", "valor.8", 0),
    ("valor_enter_9", "valor.9", 0),
    ("valor_enter_10", "valor.10", 0),
    ("valor_enter_11", "valor.11", 0),
    ("valor_enter_12", "valor.12", 0),
    ("valor_enter_100", "valor.100", 0),
    ("valor_enter_101", "valor.101", 0),
    ("valor_enter_102", "valor.102", 0),
    ("drive_revert_15", "sora.15", 0),
    ("drive_revert_16", "sora.16", 0),
    ("drive_revert_17", "sora.17", 0),
    ("drive_revert_18", "sora.18", 0),
    ("drive_revert_19", "sora.19", 0),
    ("drive_revert_20", "sora.20", 0),
    ("drive_revert_21", "sora.21", 0),
    ("drive_revert_22", "sora.22", 0),
    ("drive_revert_23", "sora.23", 0),
    ("drive_revert_24", "sora.24", 0),
    ("drive_revert_25", "sora.25", 0),
    ("drive_revert_26", "sora.26", 0),
    ("drive_revert_27", "sora.27", 0),
    # s73 Wisdom (PHH projectiles_kh2 'wisd' BDX): wisdom.20 = the magic bullet (a 600 f loop; flies 24 f + fades 8),
    # .21..24 its end / hit bursts; .25..43 the shot flash at the Keyblade tip (one per Shoot frame trigger, element =
    # the trigger param); entry slot 215 = wisdom.0..12 + 100..102 (as valor.*)
    ("wisdom_bullet", "wisdom.20", 48),
    ("wisdom_end_21", "wisdom.21", 0), ("wisdom_end_22", "wisdom.22", 0), ("wisdom_end_23", "wisdom.23", 0),
    ("wisdom_end_24", "wisdom.24", 0),
] + [("wisdom_shot_%d" % e, "wisdom.%d" % e, 0) for e in range(25, 44)] + [
    ("wisdom_enter_%d" % e, "wisdom.%d" % e, 240 if e in (11, 12, 100) else 0) for e in list(range(0, 13)) + [100, 101, 102]
]
# s88 (spell tiers): KH2 has a PAX block per spell level (magic/FIRE_2.mag = Fira, FIRE_3.mag = Firaga, ...) with the same
# element numbers as Lv1 (spells_kh2.py ELEMENTS: ring = 2, hits 3 / 4, ...). Packed as "<name>_2" / "<name>_3" (appended
# after everything so every older id stays put). Only the ring / shard / bolt / hit / flower elements (the cast glow + shot burst stay Lv1's, to spare RAM).
S88_TIERS = [
    ("fire_ring", "fire_%d.2"), ("fire_hit", "fire_%d.3"),
    ("fire_hit_last", "fire_%d.4"), 
    ("blizzard_shard", "blizzard_%d.2"),
    ("blizzard_hit", "blizzard_%d.3"), ("blizzard_hit2", "blizzard_%d.4"),
    ("thunder_bolt", "thunder_%d.2"),
    ("thunder_bolt_air", "thunder_%d.3"), ("thunder_hit", "thunder_%d.4"),
    ("cure_ring", "cure_%d.2"),
    ("cure_flower", "cure_%d.3"), ("cure_sparkle", "cure_%d.4"),
]
if os.environ.get("FXBAKE_S88", "1") != "0":
    ELEMENTS += [("%s_%d" % (pk, lv), nm % lv, 64 if pk == "blizzard_shard" else 0) for lv in (2, 3) for pk, nm in S88_TIERS]
# s89 kbfx (recon/S89_HELPER_KBFX.md): every Keyblade's own KH2 effect set obj/W_EX010_<xx>.a.us (03system.bin "went" ->
# 00objentry -> the model name; PHH mined/keyblades.kh.json). Elements 1 / 2 / 3 = the combo / finisher / guard sparks
# (as w_ex010.1 / .2 / .3 for the Kingdom Key), element 10 = the weapon's swing ribbon ("kiseki": a 64-segment ribbon
# bound to the blade, alpha 0 while no weapon_trail window is open; its texture + blend + vertex colour are the
# Keyblade's own trail look). Element 10 is packed with ONE frame (cap -1 = its frame 1 only; frame 0 is empty): the runtime only reads its first batch (texture,
# blend) and first vertex (colour); KH2 builds the ribbon's shape from the blade's motion. Appended after everything so
# every older id / texture index stays put. Pack names: kb<xx>_hit / _hit_finish / _hit_guard / _trail (the Kingdom
# Key keeps "hit" / "hit_finish" / "hit_guard" and only adds kb00_trail).
KBFX_SETS = ["10", "20", "30", "40", "50", "60", "70", "80", "90", "a0", "b0", "c0", "d0", "e0", "f0", "g0", "h0", "j0",
             "k0", "m0", "n0", "p0", "r0", "u0", "v0", "w0", "y0", "z0"]
if os.environ.get("FXBAKE_S89_KBFX", "1") != "0":
    ELEMENTS += [("kb00_trail", "w_ex010.10", -1)]
    for _s in KBFX_SETS:
        ELEMENTS += [("kb%s_hit" % _s, "w_ex010_%s.1" % _s, 0), ("kb%s_hit_finish" % _s, "w_ex010_%s.2" % _s, 0),
                     ("kb%s_hit_guard" % _s, "w_ex010_%s.3" % _s, 0), ("kb%s_trail" % _s, "w_ex010_%s.10" % _s, -1)]
# s89 b4-master (tools_s89/master copy): Master Form's P_EX100_TRIF.a.us elements (PHH recording names master.*): the entry
# (slot 215 A121 frame triggers 1..12 + 100..102, as valor.* / wisdom.*; the 600 f loops 5 / 6 / 100 / 101 capped 240) and
# the attack elements 20..32 (D334 slot 736 frame triggers 20 / 21, D333 slot 732 25..32; all root-bound). Appended last.
if os.environ.get("FXBAKE_S89_MASTER", "1") != "0":
    ELEMENTS += [("master_enter_%d" % e, "master.%d" % e, 240 if e in (5, 6, 100, 101) else 0) for e in list(range(0, 13)) + [100, 101, 102]]
    # (s89 review) FXBAKE_S89_MASTER_SPIN=0 leaves out master.20 (D334's 241-frame spin: 825k verts = 33 MB of the pack)
    ELEMENTS += [("master_fx_%d" % e, "master.%d" % e, 0) for e in [20, 21] + list(range(25, 33))
                 if e != 20 or os.environ.get("FXBAKE_S89_MASTER_SPIN", "1") != "0"]
# s91 b5-limfx (tools_s91/limfx copy): Limit Form's own effects (PHH recording names limit.* = obj/P_EX100_KH1F.a.us). The
# ability motions' frame trigger id 1 param N spawns element N (slots 844 K400 @0 -> 40; 848 K401 -> 26 27 28; 852 K402 -> 29 30
# 31; 884 K410 -> 41; 924 K420 -> 42; 936 K423 -> 33; 964 K430 -> 43; 988 K436 -> 21; 992 K437 -> 22; finishers 616 -> 38 39,
# 620 Zantetsuken -> 47..58, 732 -> 44 45) = "limit_fx_<N>"; the code-driven ones: limit_charge = .20 (Infinity charge, K435 trigger
# 9 @28), limit_shot = .23 (a shot, capped 180 f), limit_raid_trail = .32 (capped 80 f), limit_catch = .34. Appended last.
if os.environ.get("FXBAKE_S91_LIMFX", "1") != "0":
    ELEMENTS += [("limit_fx_%d" % e, "limit.%d" % e, 0) for e in [21, 22, 26, 27, 28, 29, 30, 31, 33, 38, 39, 40, 41, 42, 43, 44, 45] + list(range(47, 59))]
    ELEMENTS += [("limit_charge", "limit.20", 0), ("limit_shot", "limit.23", 180), ("limit_raid_trail", "limit.32", 80), ("limit_catch", "limit.34", 0)]
# s92 b6-formfx (tools_s92/formfx copy): the ENTRY effects of Limit (P_EX100_KH1F.a.us limit.*), Final (ULTF final.*) and Anti
# (HTLF anti.*): every form's slot 215 (A121) carries the same 15 frame-trigger-1 rows at frame 0 (motion_triggers.kh.json,
# actors 12 / 14 / 15, same as Valor / Wisdom / Master) = elements 0..12 + 100..102, as valor_enter_* / wisdom_enter_* /
# master_enter_*. Packed as limit_enter_N / final_enter_N / anti_enter_N, appended LAST. The 600-frame loops (final.1..12, .100)
# are capped at 240 like Master's. Switch FXBAKE_S92=0 = the s91 pack.
if os.environ.get("FXBAKE_S92", "1") != "0":
    for _f, _loops in (("limit", ()), ("final", tuple(range(1, 13)) + (100,)), ("anti", (1, 5, 6))):
        ELEMENTS += [("%s_enter_%d" % (_f, e), "%s.%d" % (_f, e), 240 if e in _loops else 0) for e in list(range(0, 13)) + [100, 101, 102]]
# s93 b7-ffx: Final Form's swing effects (P_EX100_ULTF.a.us, PHH recording names final.*): Final's motions carry KH2 frame trigger
# id 1 with the element number as param (final_motion.bin: 604 -> 52..57, 612 -> 20..24, 616 -> 25..40, 724 -> 58..61, 728 -> 62 63,
# 732 -> 42..51, 160 -> 64): final.20..64, packed as final_fx_<N> (none loops: <= 121 frames). Appended LAST. FXBAKE_S93=0 = the
# s92 pack. Also below (main): each form's own weapon_trail windows at slot + 10000 * (actor - 9).
if os.environ.get("FXBAKE_S93", "1") != "0":
    ELEMENTS += [("final_fx_%d" % e, "final.%d" % e, 0) for e in range(20, 65)]
# s99 b14-potfx: the item-use effects of 00effect.bar system set (PHH ItemTask, S98_HELPER_POTION.md): element 0 = the user effect
# (first run of the task, 60 f), element 1 = Potion's own element (timer end, 101 f, bind 0x4009). Appended LAST. FXBAKE_S99=0 = s93 pack.
if os.environ.get("FXBAKE_S99", "1") != "0":
    ELEMENTS += [("item_user", "system.0", 0), ("item_potion", "system.1", 0)]
# s107 b19-abilfx: the effect elements of the combo-ability motions (P_EX100.a.us "sora.N", frame trigger id 1 param N in the base
# sora_motion.bin: slot 660 A314 Guard Break -> 30..32, 664 A315 Explosion -> 44..48, 668 A316 Finishing Leap -> 33 34, 676 A318 Flash
# Step -> 53..56, 680 A319 Vicinity Break -> 49 50, 684 A317 Counterguard -> 40 41, 688 A344 Retaliating Slash -> 39, 776 A343 Aerial
# Finish -> 35..38, 788 A346 Magnet Burst -> 51 52). Packed as abil_fx_<N>. Appended LAST. FXBAKE_S107=0 = the s99 pack.
if os.environ.get("FXBAKE_S107", "1") != "0":
    ELEMENTS += [("abil_fx_%d" % e, "sora.%d" % e, 0) for e in list(range(30, 42)) + list(range(44, 57))]
# s110 roxsfx: KH2 Roxas (P_EX110.a.us == P_EX110_BTLF.a.us, byte-identical). PHH recorded one element of it, roxas.0 (37 f, root bound,
# the same look as sora.0), played by his only effect_caster trigger: frame trigger id 1 param 0 at @0 of mset slot 162 (R212, the
# guard-like motion). Packed as roxas_fx_0, appended LAST (older ids unchanged). Switch FXBAKE_S110=0 = the s107 pack.
if os.environ.get("FXBAKE_S110", "1") != "0":
    ELEMENTS += [("roxas_fx_0", "roxas.0", 0)]

# s110m mickeyfx: KH2 Mickey (P_EX200.a.us = PHH "ex200.N", W_EX200.a.us = "w_ex200.N"), appended LAST. The motion -> element table is
# `sorabake trigjson P_EX200` (frame trigger id 1 effect_caster, p1 = element): A330/A331 724/728 swing: 728 @24 -> 23; 732 @108 -> 22 (the
# finisher body flash); 768 A121 Healing Light: 5 @0 (start glow, tip), 6 @138 (the heal burst, tip); 772 A122 D-Charge: 8 + 9 @50 (the
# charge aura, root, 600 f loops, capped to 150 f here: the DLL fades them when the button is let go); 776 A370 Pearl: 0 @0 (cast glow at
# the tip, a loop whose tail ~40 plays at the shot frame, as Sora's "~f" casts) and 1 @40 (the shot burst). Weapon sparks w_ex200.1/2/3
# (EffectOnHit 1 / 2 / guard), selected by the DLL (sora_kbfx.h mk_kbfx_name). Not packed: ex200.10..21 (600 f bone loops, flags 2625, no mset trigger
# calls them: GUESS = the Drive-full limb glows), ex200.2/3/4 (no bone/root flags, no trigger), ex200.7 (181 f root, no trigger).
if os.environ.get("FXBAKE_S110M", "1") != "0":
    ELEMENTS += [("mk_pearl_cast", "ex200.0~40", 0), ("mk_pearl_burst", "ex200.1", 0),
                 ("mk_heal_a", "ex200.5", 0), ("mk_heal_b", "ex200.6", 0),
                 ("mk_dcharge_a", "ex200.8", 150), ("mk_dcharge_b", "ex200.9", 150),
                 ("mk_swing_a", "ex200.23", 0), ("mk_swing_b", "ex200.22", 0),
                 ("mk_hit", "w_ex200.1", 0), ("mk_hit_finish", "w_ex200.2", 0), ("mk_hit_guard", "w_ex200.3", 0)]

# s117 pearl: KH2 Mickey's Pearl projectile, the world-space elements s110m left out (no bone / root flags, no mset trigger = spawned by
# code). Measured / rendered (recon/S117_HELPER_PEARL.md): ex200.2 = a 600 f loop, camera-facing glowing orb + sparkles (the BALL; packed
# as its first 60 f, forced to loop); ex200.3 = 60 f burst of sparks / flare ~4 m wide (the HIT); ex200.4 = 60 f translucent bubble with an
# upward flare (the final pop, GUESS role). mk_pearl_cast2 = the cast glow the s110m "ex200.0~40" lost: that PHF2 variant holds ONLY the
# loop-end tail (frames 0..39 EMPTY), so the glow was invisible while Mickey wound up; here ex200.0 frames 0..39 (the loop) + the
# tail's frames 40..68. Spec "a[x:y]+b[x:y]" = slices of PHF2 elements joined. Appended LAST. FXBAKE_S117=0 = the s110m pack.
LOOP_FORCE = {"mk_pearl_ball"}
if os.environ.get("FXBAKE_S117", "1") != "0":
    ELEMENTS += [("mk_pearl_ball", "ex200.2", 60), ("mk_pearl_hit", "ex200.3", 0), ("mk_pearl_pop", "ex200.4", 0),
                 ("mk_pearl_cast2", "ex200.0[:40]+ex200.0~40[40:]", 0)]
DEDUPE = os.environ.get("FXBAKE_S89_DEDUPE", "1") != "0"  # (s89 review)
TRAIL_TEX = "trail"  # a procedural ribbon texture appended last (KH2 draws the Keyblade ribbon in the engine, no PAX)

TEXREC = struct.Struct("<II")
EFFREC = struct.Struct("<32sIIIIIhHI")
BATCH = struct.Struct("<HBB4B4B4Bf4f4f3f3f3f9fII")
VERTEX = struct.Struct("<3f3f2fHH4H")
OUT_TEX = struct.Struct("<IIII")
OUT_EFF = struct.Struct("<32sIIIIiIII")
OUT_FRAME = struct.Struct("<II")
OUT_BATCH = struct.Struct("<HBBB3BII9f")
OUT_VERT = struct.Struct("<3f3f2fHH4B")
OUT_TRAIL = struct.Struct("<III")
OUT_HDR = struct.Struct("<4s15I")
assert OUT_BATCH.size == 52 and OUT_VERT.size == 40 and OUT_EFF.size == 64 and OUT_HDR.size == 64


class Phf2:
    def __init__(self, path):
        self.f = open(path, "rb")
        magic, ver, nt, ne, tt, et = struct.unpack("<4sIIIII", self.f.read(24))
        if magic != b"PHF2" or ver != 4:
            raise SystemExit(f"{path}: not a PHF2 v4 file")
        self.f.seek(tt)
        self.textures = [TEXREC.unpack(self.f.read(8)) for _ in range(nt)]
        self.f.seek(et)
        self.effects = {}
        for _ in range(ne):
            r = self.f.read(EFFREC.size)
            nm, off, ps, us, nf, fl, bone, pf, _r = EFFREC.unpack(r)
            self.effects[nm.split(b"\0", 1)[0].decode()] = (off, ps, us, nf, fl, bone, pf)

    def texture(self, i):
        off, size = self.textures[i]
        self.f.seek(off)
        return self.f.read(size)

    def frames(self, name, cap=0):
        if name not in self.effects:
            raise SystemExit(f"fxbake: recording {name!r} is not in {self.f.name} (fx_keep 'used' records only the effects the fx tools' "
                             "tables name; add it to the tool's table, or set \"fx_keep\": \"all\")")
        off, ps, us, nf, fl, bone, pf = self.effects[name]
        self.f.seek(off)
        table = struct.unpack(f"<{nf + 1}I", self.f.read(4 * (nf + 1)))
        base = off + 4 * (nf + 1)
        n = nf if not cap else min(cap, nf)
        out = []
        for i in range(n):
            self.f.seek(base + table[i])
            d = zlib.decompress(self.f.read(table[i + 1] - table[i]))
            (nb,) = struct.unpack_from("<I", d, 0)
            q, fr = 4, []
            for _ in range(nb):
                v = BATCH.unpack_from(d, q)
                q += BATCH.size
                nv = v[43]
                verts = [VERTEX.unpack_from(d, q + k * VERTEX.size) for k in range(nv)]
                q += VERTEX.size * nv
                fr.append((v, verts))
            out.append(fr)
        return out


# ---- DDS -> BGRA8 (DXT5 / BC3 and uncompressed 32-bit; pure python, the textures are small)
def _c565(c):
    return ((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31


def decode_dxt5(data, w, h):
    out = bytearray(w * h * 4)
    q = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            a0, a1 = data[q], data[q + 1]
            abits = int.from_bytes(data[q + 2:q + 8], "little")
            c0, c1 = struct.unpack_from("<HH", data, q + 8)
            cbits = struct.unpack_from("<I", data, q + 12)[0]
            q += 16
            p0, p1 = _c565(c0), _c565(c1)
            if c0 > c1:
                pal = [p0, p1, tuple((2 * p0[i] + p1[i]) // 3 for i in range(3)), tuple((p0[i] + 2 * p1[i]) // 3 for i in range(3))]
            else:
                pal = [p0, p1, tuple((p0[i] + p1[i]) // 2 for i in range(3)), (0, 0, 0)]
            if a0 > a1:
                apal = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
            else:
                apal = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
            for py in range(4):
                for px in range(4):
                    x, y = bx + px, by + py
                    if x >= w or y >= h:
                        continue
                    i = py * 4 + px
                    c = pal[(cbits >> (2 * i)) & 3]
                    o = (y * w + x) * 4
                    out[o], out[o + 1], out[o + 2], out[o + 3] = c[2], c[1], c[0], apal[(abits >> (3 * i)) & 7]  # BGRA
    return bytes(out)


def dds_bgra(blob):
    if blob[:4] != b"DDS ":
        raise ValueError("not a DDS")
    hgt, wid = struct.unpack_from("<II", blob, 12)
    fourcc = blob[84:88]
    bpp = struct.unpack_from("<I", blob, 88)[0]
    body = blob[128:]
    if fourcc == b"DXT5":
        return wid, hgt, decode_dxt5(body, wid, hgt)
    if fourcc == b"\0\0\0\0" and bpp == 32:
        rm, gm, bm, am = struct.unpack_from("<IIII", blob, 92)
        out = bytearray(wid * hgt * 4)

        def shift(m):
            return (m & -m).bit_length() - 1 if m else 0

        sr, sg, sb, sa = shift(rm), shift(gm), shift(bm), shift(am)
        for i in range(wid * hgt):
            px = struct.unpack_from("<I", body, i * 4)[0]
            out[i * 4] = (px & bm) >> sb & 255
            out[i * 4 + 1] = (px & gm) >> sg & 255
            out[i * 4 + 2] = (px & rm) >> sr & 255
            out[i * 4 + 3] = (px & am) >> sa & 255 if am else 255
        return wid, hgt, bytes(out)
    raise ValueError(f"unsupported DDS {fourcc!r} {bpp} bpp")


def trail_texture(w=64, h=16):
    """A soft white ribbon: alpha fades along u (1 at the blade's newest segment, 0 at the oldest) and at the v edges."""
    out = bytearray(w * h * 4)
    for y in range(h):
        ey = 1.0 - abs((y + 0.5) / h * 2 - 1) ** 2
        for x in range(w):
            ax = (1.0 - (x + 0.5) / w) ** 1.5
            a = int(255 * ey * ax)
            o = (y * w + x) * 4
            out[o], out[o + 1], out[o + 2], out[o + 3] = 255, 240, 220, a  # BGRA: a warm white (KH2's trail is white-blue)
    return w, h, bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", default=FXCACHE)
    ap.add_argument("--out", required=True)
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    src = Phf2(os.path.join(a.cache, "kh2_fx.bin"))
    meta = json.load(open(os.path.join(a.cache, "kh2_fx.json"), encoding="utf-8"))["effects"]
    sounds = json.load(open(sndsrc.sounds_json(), encoding="utf-8"))
    slots = sounds["actors"][SORA_ACTOR]["slots"]
    trails = []
    for k, v in slots.items():
        for win in v.get("trails") or []:
            trails.append((int(k), int(win[0]), int(win[1])))
    if os.environ.get("FXBAKE_S93", "1") != "0":  # s93 b7-ffx: the Drive Forms' own windows (Valor 10000+ .. Anti 60000+); the DLL reads Final's + Limit's
        seen = set()
        for aid in ("10", "11", "12", "13", "14", "15"):
            for k, v in sounds["actors"][aid]["slots"].items():
                for win in v.get("trails") or []:
                    t = (10000 * (int(aid) - 9) + int(k), int(win[0]), int(win[1]) & 0xFFFFFFFF)  # end -1 = open (0xFFFFFFFF, as the runtime reads it)
                    if t not in seen:
                        seen.add(t)
                        trails.append(t)
    if os.environ.get("FXBAKE_S110", "1") != "0":  # s110 roxsfx: Roxas' own weapon_trail windows at slot + 70000 (the DLL: sora.c ffx_trail_win)
        # = W_EX010_RX.mset range trigger id 12 (weapon_trail; checked against Sora: W_EX010.mset slot 604 range 12 [13,23] == kh2_sounds actors.0 trails)
        wr = json.load(open(os.path.join(TRIGDIR, "trig_W_EX010_RX.json")))
        for k, v in wr.items():
            for r in v["range"]:
                if r[0] == 12:
                    trails.append((70000 + int(k), r[1], r[2] & 0xFFFFFFFF))
    if os.environ.get("FXBAKE_S110M", "1") != "0":  # s110m: Mickey's own weapon_trail windows at slot + 80000 (FORM_MICKEY 7 + 1; W_EX200.mset range trigger 12)
        wr = json.load(open(os.path.join(TRIGDIR, "trig_W_EX200.json")))
        for k, v in wr.items():
            for r in v["range"]:
                if r[0] == 12:
                    trails.append((80000 + int(k), r[1], r[2] & 0xFFFFFFFF))
    trails.sort()

    tex_map, textures = {}, []  # PHF2 texture index -> pack index
    SEEN, DUPS = {}, []  # (s89 review dedupe)
    effs, frames, batches, verts = [], [], [], []
    for pack_name, name, cap in ELEMENTS:
        parts = [(q.split("[")[0], q.split("[")[1].rstrip("]") if "[" in q else None) for q in name.split("+")]  # (s117: slices)
        if any(b not in src.effects for b, _ in parts):
            print(f"  {pack_name}: {name} NOT in the recording, skipped")
            continue
        off, ps, us, nf, fl, bone, pf = src.effects[parts[0][0]]
        m = meta.get(parts[0][0], {})
        if len(parts) > 1 or parts[0][1] is not None:
            frs = []
            for b, sl in parts:
                lo, hi = (sl.split(":") if sl else ("", ""))
                frs += src.frames(b)[int(lo or 0):(int(hi) if hi else None)]
        else:
            frs = src.frames(name, cap) if cap >= 0 else src.frames(name, 2)[1:]  # (s89 kbfx: -1 = frame 1 alone)
        b0, v0 = len(batches), len(verts)  # (s89 review: the dedupe below)
        first_frame, nverts, maxv = len(frames), 0, 0
        for fr in frs:
            first_batch, fv = len(batches), 0
            for v, vs in fr:
                tex = v[0]
                if tex == 0xFFFE:
                    tex = 0xFFFF  # (previous frame's texture: not used by the chosen elements; drawn untextured)
                if tex != 0xFFFF:
                    if tex not in tex_map:
                        tex_map[tex] = len(textures)
                        textures.append(tex)
                    tex = tex_map[tex]
                blend_en, op, srcb, dstb = v[3:7]
                blend = 0 if dstb == 2 else (1 if dstb == 6 else 2)  # D3D: 2 ONE, 6 INVSRCALPHA, 1 ZERO
                if not blend_en:
                    blend = 2
                clamp = 1 if v[12] or v[13] else 0
                first_vert = len(verts)
                for p in vs:
                    c = [min(255, int(round(x / 32767.5 * 1.992188 * 255))) for x in p[10:14]]
                    verts.append(OUT_VERT.pack(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], 0, *c))
                batches.append(OUT_BATCH.pack(tex, v[2], blend, clamp, 0, 0, 0, first_vert, len(vs), *v[33:42]))
                fv += len(vs)
            frames.append(OUT_FRAME.pack(first_batch, len(fr)))
            nverts += fv
            maxv = max(maxv, fv)
        # (s89 review) FXBAKE_S89_DEDUPE (default 1): an s89 element (kb* / master_*) whose frames are byte-identical to an
        # earlier element's (same textures, batches, vertices) points at that one's frames instead of a second copy.
        # Older elements are never changed (their ids, frames and texture indices stay as before s89); they are only
        # remembered as possible sources.
        if DEDUPE:
            h = hashlib.sha1(b"".join(verts[v0:]))
            h.update(repr(tuple(OUT_BATCH.unpack(x)[:7] + OUT_BATCH.unpack(x)[8:] + (OUT_BATCH.unpack(x)[7] - v0,) for x in batches[b0:])).encode())
            h.update(repr(tuple((OUT_FRAME.unpack(x)[0] - b0, OUT_FRAME.unpack(x)[1]) for x in frames[first_frame:])).encode())
            key = h.digest()
            if key in SEEN and (pack_name.startswith("kb") or pack_name.startswith("master_") or pack_name.startswith("limit_")):
                del frames[first_frame:], batches[b0:], verts[v0:]
                first_frame = SEEN[key]
                DUPS.append(pack_name)
            elif key not in SEEN:
                SEEN[key] = first_frame
        loops = 1 if ((m.get("loops") and not cap and "~" not in name) or pack_name in LOOP_FORCE) else 0
        effs.append((pack_name, first_frame, len(frs), loops, pf, bone, int(m.get("fade", 0)), maxv))
        print(f"  {pack_name:16s} = {name:18s} {len(frs):3d} frames{' (loop)' if loops else ''}, {nverts:6d} verts, "
              f"bone {bone:#06x} pax flags {pf:#05x}, textures {sorted(set(tex_map[v[0]] for fr in frs for v, _ in fr if v[0] < 0xFFFE))}")
    if DUPS: print(f"  (s89 dedupe) {len(DUPS)} elements share an earlier one's frames: {' '.join(DUPS)}")
    print(f"{len(effs)} elements, {len(frames)} frames, {len(batches)} batches, {len(verts)} verts "
          f"({len(verts) * OUT_VERT.size / 1e6:.1f} MB), {len(textures)} textures")
    if a.list:
        return
    pix, texrecs = bytearray(), []
    for ti in textures:
        w, h, bgra = dds_bgra(src.texture(ti))
        texrecs.append(OUT_TEX.pack(w, h, len(pix), 0))
        pix += bgra
        print(f"  texture {len(texrecs) - 1} = PHF2 #{ti}: {w}x{h}")
    w, h, bgra = trail_texture()
    texrecs.append(OUT_TEX.pack(w, h, len(pix), 0))
    pix += bgra
    effrecs = [OUT_EFF.pack(n.encode()[:31], ff, nf, lp, pf, bone, fade, maxv, 0) for n, ff, nf, lp, pf, bone, fade, maxv in effs]
    trailrecs = [OUT_TRAIL.pack(*t) for t in trails]
    body = [b"".join(texrecs), b"".join(effrecs), b"".join(frames), b"".join(batches), b"".join(verts), b"".join(trailrecs), bytes(pix)]
    offs, q = [], OUT_HDR.size
    for b in body:
        offs.append(q)
        q += len(b)
        q += -q % 16
    hdr = OUT_HDR.pack(b"SKFX", 1, len(texrecs), len(effrecs), len(frames), len(batches), len(verts), len(trailrecs),
                       offs[0], offs[1], offs[2], offs[3], offs[4], offs[5], offs[6], len(pix))
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "wb") as f:
        f.write(hdr)
        for b, o in zip(body, offs):
            f.write(bytes(o - f.tell()))
            f.write(b)
    print(f"{a.out}: {os.path.getsize(a.out) / 1e6:.1f} MB, {len(trails)} trail windows")


if __name__ == "__main__":
    main()

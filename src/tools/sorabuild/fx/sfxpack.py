#!/usr/bin/env python3
"""Sora-in-BotW: pack KH2's sound effects for the runtime SE player (runtime/sora_sfx.c).

Sources (already decoded by Project Heart Hunters' miner, nothing is downloaded or decoded here):
  <cache>/mined/kh2_sounds.kh.json   SE ids -> files, Sora's per-slot frame triggers (play_sound / footstep_sound /
                                     vsb_voice2), the voice table (vtbl rows -> hb0_sora.scd subsongs), the system SEs
                                     (cursor 1, submenu 3, ...), the magic .mag effect SEs, the atkp-row hit SEs
  <cache>/kh2_sfx/<file>.wav         plain RIFF PCM16, 1 or 2 channels, ~48 kHz (47974..48003)
  <cache> = the PHH cache dir (env SB_FXCACHE, old name SB_PHH)

Output: sfx.bin ('SKSF' v1, little-endian), everything Sora can play in one file:
  header   u32 magic 'SKSF', u32 version 1, u32 n_sounds, u32 n_trig, u32 n_trig_sounds, u32 n_vtbl, u32 n_vtbl_choices,
           u32 n_names, u32 n_hits, u32 data_off, u32 data_bytes, u32 pad
  Sound    [n_sounds]      char key[48]; u32 rate; u32 channels; f32 volume; u32 loop; u32 offset (bytes into data);
                           u32 frames                                       (sizeof 72)
  Trig     [n_trig]        u32 slot; u32 frame; u32 kind (0 sound, 1 voice vtbl); u32 vtbl; u32 cycle; u32 count;
                           u32 first                                        (sizeof 28; sorted by slot, frame)
  u32      [n_trig_sounds] sound indexes (Trig.first .. +count)
  Vtbl     [n_vtbl]        u32 vtbl; u32 priority; u32 count; u32 first   (sizeof 16)
  Choice   [n_vtbl_choices] i32 sound (-1 = silence); u32 weight (0 = equal share)  (sizeof 8)
  Name     [n_names]       char name[32]; u32 sound                        (sizeof 36; "cursor", "fire_cast", ...)
  Hit      [n_hits]        u32 atkp_row; u32 sound                         (sizeof 8)
  data     PCM16 interleaved, each sound at its offset

  python tools/sfxpack.py [--cache <dir>] --out <sfx.bin> [--all]
    --all packs every sound of the json (~700), else only what Sora's events reference (~200).
"""
import argparse, json, os, struct, sys, wave
import sys as _sys, os as _os; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__))); import sndsrc  # (s156) sound inputs: recipe tree or PHH cache

CACHE = os.environ["SB_FXCACHE"] if "SB_FXCACHE" in os.environ else os.environ["SB_PHH"]  # fx cache dir (SB_FXCACHE; SB_PHH = old name), passed in by sorabuild (ctx.fxcache)
SORA_ACTOR = "0"

# Named events the runtime asks for by string (sora_sfx_event / sora_sfx_id): system SEs from the json's "system"
# table, spell SEs from the "effects" table (<spell>_<level>.<step>: .0 = the cast / spawn, the others the effect's
# later steps), plus a few handy aliases. VERIFIED entries come straight from the json; GUESS ones are noted.
NAMES = {
    # KH2 SYSTEM_SE (json "system", re:0x3B11E0): VERIFIED
    "cursor": ("system", "cursor"), "decide": ("system", "decide"), "submenu": ("system", "submenu"),
    "cancel": ("system", "cancel"), "error": ("system", "error"), "lock_on": ("system", "lock_on"),
    "lock_off": ("system", "lock_off"), "danger": ("system", "danger"), "page": ("system", "page"),
    "drive_bar": ("system", "drive_bar"), "drive_full": ("system", "drive_full"), "game_over": ("system", "game_over"),
    # magic (json "effects" from the .mag files): VERIFIED ids; which step is "the cast" vs "the hit" is by the
    # .mag's own order (step 0 = the first entry, played at spawn)
    "fire_cast": ("effects", "fire_1.0"), "fire_loop": ("effects", "fire_1.2"), "fire_hit": ("effects", "fire_1.3"),
    "fire_end": ("effects", "fire_1.11"),
    "fira_cast": ("effects", "fire_2.0"), "firaga_cast": ("effects", "fire_3.0"),
    "blizzard_cast": ("effects", "blizzard_1.0"), "blizzard_shot": ("effects", "blizzard_1.1"),
    "blizzard_hit": ("effects", "blizzard_1.3"), "blizzard_end": ("effects", "blizzard_1.6"),
    "thunder_cast": ("effects", "thunder_1.0"), "thunder_bolt": ("effects", "thunder_1.2"),
    "thunder_end": ("effects", "thunder_1.4"),
    "cure_cast": ("effects", "cure_1.0"), "cure_heal": ("effects", "cure_1.3"), "cure_end": ("effects", "cure_1.4"),
    "magnet_cast": ("effects", "magnet_1.0"), "reflect_cast": ("effects", "reflect_1.0"),
    # motion-code SEs Sora's mset slots play (json actor 0 slot triggers): VERIFIED ids, named for direct use
    "jump": ("sound", "se/se000.win32.scd#303"),        # A003 JUMP frame 3 (also B003 EX050 frame 4)
    "jump_step": ("sound", "se/se705.win32.scd#25"),    # A003 JUMP frame 2 footstep (push-off)
    "land": ("sound", "se/se705.win32.scd#26"),         # A005 LAND frame 2 footstep
    "hang": ("sound", "se/se000.win32.scd#308"),        # A100 HANG frame 4 (ledge grab)
    "hang_up": ("sound", "se/se000.win32.scd#304"),     # A101 HANG_UP frame 2
    "item": ("sound", "se/se000.win32.scd#305"),        # A110 ITEM frame 10
    "walk_cloth": ("sound", "se/se000.win32.scd#300"),  # A001 WALK mid-cycle (cloth rustle)
    "run_cloth": ("sound", "se/se000.win32.scd#301"),   # A002 RUN frame 2
    "swing1": ("sound", "se/se500.win32.scd#35"),       # A300 EX000 frame 1 (combo 1 whoosh)
    "swing2": ("sound", "se/se500.win32.scd#36"),       # A301 EX001 frame 1
    "swing3": ("sound", "se/se500.win32.scd#37"),       # A302 EX002 frame 1
    "finisher": ("sound", "se/se500.win32.scd#38"),     # A303 EX003 frame 10 (ground finisher)
    "finisher2": ("sound", "se/se500.win32.scd#39"),    # A304 EX004 frame 1 (the other ground finisher)
    "air_swing1": ("sound", "se/se500.win32.scd#42"),   # A310 EX010 frame 1
    "air_swing2": ("sound", "se/se500.win32.scd#49"),   # A311 EX011 frame 6
    "air_finisher": ("sound", "se/se500.win32.scd#50"), # A313 EX013 frame 18
    "hit": ("hit", "126"),                              # atkp row 126 -> se500#0: the base Keyblade hit
    "glide_start": ("sound", "se/se999.win32.scd#19"),  # GUESS: A160 EX051 frame 1 (a 12-frame air motion, se999#19)
    "roll": ("sound", "se/se705.win32.scd#25"),         # GUESS: no SE trigger in the roll slot; the push-off step
    "wall_hit": ("sound", "se/se500.win32.scd#10"),     # se 500010: weapon attack hits BG (re:0x3D1960), EXTRA below
}

# s55 (LV slice 4): appended AFTER every older sound and name, so the older sound indexes / name rows stay identical.
# Evidence (recon/S55_HELPER_BAKE.md): the level up (re:0x3ED190 / re:0x3ECF30) and Get Bonus (re:0x3EDF90 ->
# re:0x3ED350) start 00effect.bar system element 15, whose +0x18 SE = 25; the effect player plays it through
# re:0x1DE150(se) = bank se / 1000, index se % 1000 -> se000 #25. Form (re:0x3E8380) / summon (re:0x40D530) level
# ups start element 16 (re:0x3ED390), SE 26 -> se000 #26. KH2 has no separate "ability learned" SE: the learned
# ability is only a line on the level-up / bonus panel (re:0x3ED7D0 / re:0x3EDF90 play no SE of their own).
LATE_NAMES = {
    "level_up": "se/se000.win32.scd#25",        # VERIFIED: system element 15's SE (Sora / party level up)
    "bonus_up": "se/se000.win32.scd#25",        # VERIFIED: Get Bonus starts the same element 15
    "form_level_up": "se/se000.win32.scd#26",   # VERIFIED: system element 16's SE (Drive Form level up)
    "summon_level_up": "se/se000.win32.scd#26", # VERIFIED: summon level up starts the same element 16
    "drive_sphere": "se/se000.win32.scd#114",   # VERIFIED (s87c): sora.1's SE (the Drive sphere, CHANGEFORM)
    "drive_revert": "se/se000.win32.scd#115",   # VERIFIED (s87c): sora.15's SE (Revert)
}

# Sounds the PHH miner never extracted (no json entry): key -> json-style record, wav under EXTRA_DIR/<file>.
# Made by hand: vgmstream-cli -i -s <subsong> -o <EXTRA_DIR>/<file> se500.win32.scd (recon/WALL_REPEL_RECON.md s11).
EXTRA_DIR = os.environ["SB_EXTRA"]  # dir holding se/se500/010.wav (baked by sorabuild step sfx_extra_wav)
EXTRA = {
    "se/se500.win32.scd#10": {"file": "se/se500/010.wav", "rate": 48008, "channels": 2, "volume": 1.0},  # subsong 11
}


# s56: Donald (json actor 1, P_EX020) and Goofy (actor 2, P_EX030). Everything below is read from the same json
# (recon/S56_HELPER_PARTYSFX.md has the evidence per row); runtime/sora_party.h plays them by these names.
#  <who>_<set>.<k>   KH2 vtbl row's choices in order (the -3 "partner's name" entry is the <who>_name_<char> rows);
#                    <who>_<set> = choice 0 (for a direct sora_sfx_event). Voices: voice/us/battle/hb0_<who>.win32.scd
#  <who>_name_<c>    the actor's `names` table (vtbl -3 entries: the partner he talks about, by KH2 character id)
#  <who>_step<f>.<k> footstep kind f (mset trigger 2, its param; 12soundinfo.bar): 0 run / 4 walk cycle 4 variants
#  <who>_fx<e>       the SE of element e of his own effect set (obj/P_EX0x0.a.us, element +0x18 = bank*1000+index)
#  + the weapon mset's play_sound rows and a few aliases. Hit rows: the actor's atkp rows (00battle.bin -> se) go into
#  the Hit table, after Sora's (disjoint row numbers), so sora_sfx_hit(atkp) plays the party's own hit SEs.
# s75 Wisdom Form (json "effects" of obj/P_EX100_MAGF.a.us, element +0x18 SE): appended after the party rows. The shot
# flash elements wisdom.25..43 all carry se999 #11 (= "wisdom_shot"); the end / hit bursts .21..24 = #12 / #13 / #13 / #14.
WISDOM_NAMES = {"wisdom_shot": "wisdom.25", "wisdom_enter": "wisdom.0", "wisdom_end_21": "wisdom.21",
                "wisdom_end_22": "wisdom.22", "wisdom_end_23": "wisdom.23", "wisdom_end_24": "wisdom.24"}


PARTY_ACTORS = (("donald", "1"), ("goofy", "2"))
PARTY_VOICE = {  # set name -> vtbl id (00battle.bin vtbl rows, re:0x3CDB60)
    "donald": {"attack": 4, "spell": 6, "cure_self": 21, "item": 3, "hurt": 29, "hurt_low": 30, "blown": 1, "ko": 2,
               "stun_end": 10},
    "goofy": {"attack": 4, "item": 3, "hurt": 29, "hurt_low": 30, "blown": 1, "ko": 2, "stun_end": 10},
}
PARTY_CHAR = {"1": "sora", "2": "donald", "3": "goofy"}
PARTY_WEAPON_SE = {  # the weapon mset's (W_EX020 / W_EX030) play_sound rows, by sound
    "donald": {"se/se535.win32.scd#34": "donald_swing", "se/se535.win32.scd#35": "donald_air_swing"},
    "goofy": {"se/se560.win32.scd#34": "goofy_swing1", "se/se560.win32.scd#35": "goofy_swing2",
              "se/se560.win32.scd#38": "goofy_tornado"},
}
PARTY_ALIAS = {  # alias -> an existing party name (packed as its own row, same sound)
    "donald_fire_cast": "donald_fx0", "donald_blizzard_cast": "donald_fx3", "donald_thunder_cast": "donald_fx7",
    "donald_cure_cast": "donald_fx11", "goofy_tornado_start": "goofy_fx0", "goofy_bash_throw": "goofy_fx2",
    "goofy_turbo": "goofy_fx3",
}
PARTY_HIT_ALIAS = {"donald_hit": ("1", "151"), "donald_hit_big": ("1", "153"), "goofy_hit": ("2", "156"),
                   "goofy_hit_big": ("2", "158"), "goofy_tornado_hit": ("2", "1160"), "goofy_turbo_hit": ("2", "1162")}


def party_names(js):
    out = []
    for who, aid in PARTY_ACTORS:
        act = js["actors"][aid]
        for set_name, vt in PARTY_VOICE[who].items():
            ks = [c["sound"] for c in act["vtbl"][str(vt)]["choices"] if c.get("sound")]
            for k, key in enumerate(ks):
                out.append((f"{who}_{set_name}.{k}", key))
            if ks:
                out.append((f"{who}_{set_name}", ks[0]))
        for c, v in sorted(act["names"].items(), key=lambda x: int(x[0])):
            out.append((f"{who}_name_{PARTY_CHAR.get(c, c)}", v["sound"]))
        steps = {}
        for sl in act["slots"].values():
            for t in sl.get("triggers", []):
                if t["type"] == "footstep_sound" and t.get("sounds"):
                    steps.setdefault(int(t["footstep"]), t["sounds"])
        for f in sorted(steps):
            for k, key in enumerate(steps[f]):
                out.append((f"{who}_step{f}.{k}", key))
        for k, e in sorted(js["effects"].items(), key=lambda x: (x[0].split(".")[0], int(x[0].split(".")[1]) if x[0].split(".")[1].isdigit() else 0)):
            if k.startswith(who + ".") and e.get("sounds"):
                out.append((f"{who}_fx{k.split('.')[1]}", e["sounds"][0]))
        for key, name in PARTY_WEAPON_SE[who].items():
            out.append((name, key))
    have = dict(out)
    for alias, name in PARTY_ALIAS.items():
        out.append((alias, have[name]))
    for alias, (aid, row) in PARTY_HIT_ALIAS.items():
        out.append((alias, js["actors"][aid]["hits"][row]["sounds"][0]))
    return out


def wav_pcm16(path):
    with wave.open(path, "rb") as w:
        if w.getsampwidth() != 2:
            raise ValueError(f"{path}: {w.getsampwidth() * 8}-bit, want PCM16")
        return w.getframerate(), w.getnchannels(), w.getnframes(), w.readframes(w.getnframes())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", default=CACHE)
    ap.add_argument("--out", required=True)
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args()
    js = json.load(open(sndsrc.sounds_json()))
    sounds, actor = js["sounds"], js["actors"][SORA_ACTOR]
    for k, s in EXTRA.items():
        sounds.setdefault(k, dict(s, dir=EXTRA_DIR))

    keys, index = [], {}

    def want(key):
        if key not in sounds:
            return -1
        if key not in index:
            index[key] = len(keys)
            keys.append(key)
        return index[key]

    # 1. the per-slot triggers (sorted by slot, frame so the runtime can scan a slot's range)
    trigs, trig_sounds = [], []
    for slot, sl in sorted(((int(s), v) for s, v in actor["slots"].items())):
        for t in sorted(sl.get("triggers", []), key=lambda t: t["frame"]):
            if t["type"] == "vsb_voice2":
                trigs.append((slot, t["frame"], 1, int(t["vtbl"]), 0, 0, 0))
            elif t["type"] in ("play_sound", "footstep_sound"):
                ids = [want(k) for k in t.get("sounds", [])]
                ids = [i for i in ids if i >= 0]
                if not ids:
                    continue
                trigs.append((slot, t["frame"], 0, 0, 1 if t.get("cycle") else 0, len(ids), len(trig_sounds)))
                trig_sounds += ids
    # 2. the voice table
    vtbls, choices = [], []
    for v in sorted(actor["vtbl"].values(), key=lambda r: r["vtbl"]):
        first = len(choices)
        for c in v["choices"]:
            choices.append((want(c["sound"]) if c.get("sound") else -1, int(c.get("weight", 0))))
        vtbls.append((v["vtbl"], v["priority"], len(choices) - first, first))
    # 3. the atkp-row hits
    hits = []
    for row, h in sorted(((int(r), v) for r, v in actor["hits"].items())):
        i = want(h["sounds"][0]) if h.get("sounds") else -1
        if i >= 0:
            hits.append((row, i))
    # 4. the named events
    names, late = [], []
    for name, (table, k) in NAMES.items():
        if table == "system":
            key = js["system"][k]["sound"]
        elif table == "effects":
            key = js["effects"][k]["sounds"][0]
        elif table == "hit":
            key = actor["hits"][k]["sounds"][0]
        else:
            key = k
        if key in EXTRA and key not in index:
            late.append((name, key))  # packed last, so the older sounds keep their indexes
            continue
        i = want(key)
        if i < 0:
            print(f"  (no file for {name}: {key})", file=sys.stderr)
            continue
        names.append((name, i))
    # also every spell effect step, by its json key (fire_1.3 ...), so the DLL can ask for any of them
    for k, e in js["effects"].items():
        if e.get("sounds") and k.split("_")[0] in ("fire", "blizzard", "thunder", "cure", "magnet", "reflect"):
            i = want(e["sounds"][0])
            if i >= 0:
                names.append((k, i))
    for name, key in late:
        names.append((name, want(key)))
    for name, key in LATE_NAMES.items():
        i = want(key)
        if i < 0:
            print(f"  (no file for {name}: {key})", file=sys.stderr)
            continue
        names.append((name, i))
    # s56: Donald + Goofy, appended after everything above (older sound indexes / name rows / hit rows unchanged)
    for name, key in party_names(js):
        i = want(key)
        if i < 0:
            print(f"  (no file for {name}: {key})", file=sys.stderr)
            continue
        names.append((name, i))
    for name, k in WISDOM_NAMES.items():  # (s75)
        i = want(js["effects"][k]["sounds"][0])
        if i < 0:
            print(f"  (no file for {name}: {k})", file=sys.stderr)
            continue
        names.append((name, i))
    sora_rows = {r for r, _ in hits}
    for who, aid in PARTY_ACTORS:
        for row, h in sorted(((int(r), v) for r, v in js["actors"][aid]["hits"].items())):
            if row in sora_rows:  # (never: Sora's atkp rows and the party's are disjoint; keep Sora's sound if so)
                print(f"  (party hit row {row} is also Sora's: kept Sora's)", file=sys.stderr)
                continue
            i = want(h["sounds"][0]) if h.get("sounds") else -1
            if i >= 0:
                hits.append((row, i))
    # s91 b5-limfx: Limit Form (json actor 12, P_EX100_KH1F.mset). Appended after everything above (older sound / name /
    # hit indexes unchanged). (1) the K4xx motions' triggers: mset slots >= 844 exist only in the KH1F mset (Sora's own top out
    # at 838; Limit's slots below that are the same numbers as Sora's, whose triggers are already packed), so they never
    # collide and the list stays sorted by slot. (2) the SE of each Limit effect element (json "effects" limit.N, element +0x18
    # SE: se999 #45.. #56) as the name limit_fx_<N> (+ limit_charge = .20, limit_raid_trail = .32). (3) the atkp rows 1857..1895
    # that only Limit's moves use (actor 12 "hits", se500), so its melee / shot hits have their own SEs, not the base hit.
    lim = js["actors"]["12"]
    for slot, sl in sorted(((int(s_), v) for s_, v in lim["slots"].items())):
        if slot < 844:
            continue
        for t in sorted(sl.get("triggers", []), key=lambda t: t["frame"]):
            if t["type"] == "vsb_voice2":
                trigs.append((slot, t["frame"], 1, int(t["vtbl"]), 0, 0, 0))
            elif t["type"] in ("play_sound", "footstep_sound"):
                ids = [i for i in (want(k) for k in t.get("sounds", [])) if i >= 0]
                if ids:
                    trigs.append((slot, t["frame"], 0, 0, 1 if t.get("cycle") else 0, len(ids), len(trig_sounds)))
                    trig_sounds += ids
    for k, e in sorted(js["effects"].items(), key=lambda x: (x[0].split(".")[0], int(x[0].split(".")[1]) if x[0].split(".")[1].isdigit() else 0)):
        if k.startswith("limit.") and e.get("sounds"):
            i = want(e["sounds"][0])
            if i >= 0:
                names.append(("limit_fx_" + k.split(".")[1], i))
                if k == "limit.20": names.append(("limit_charge", i))
                if k == "limit.32": names.append(("limit_raid_trail", i))
    have_rows = {r for r, _ in hits}
    for row, h in sorted(((int(r), v) for r, v in lim["hits"].items())):
        if row in have_rows or not h.get("sounds"):
            continue
        i = want(h["sounds"][0])
        if i >= 0:
            hits.append((row, i))
    # s92 b6-formfx: every Drive Form's OWN mset triggers (json actors 10 Valor, 11 Wisdom, 12 Limit, 13 Master, 14 Final, 15 Anti;
    # footsteps / swing whooshes / voice rows from P_EX100_<form>.mset + the form's weapon msets). Until s91 a form played the
    # BASE Sora actor's triggers for its slot number (sora_sfx_motion(slot)); 66 of the form slots with triggers carry a different
    # list in their own mset (or none in the base's: Final / Anti footsteps, Master 236..244 / 740, Wisdom 228 ...). Packed at
    # slot + 10000 * (actor - 9) (Valor 10000+, Wisdom 20000+, Limit 30000+, Master 40000+, Final 50000+, Anti 60000+): all above
    # the base slots, so the list stays sorted and older rows / indexes are unchanged; the runtime adds the offset while a form's
    # motions are on (sora.c sfx_slot()). Limit's K4xx slots >= 844 stay at their raw slot (s91). Marker name "formse_v1" =
    # the pack has these rows (the DLL falls back to the raw slot when an older sfx.bin lacks it).
    NFORMSE = 0
    for aid in ("10", "11", "12", "13", "14", "15"):
        base = 10000 * (int(aid) - 9)
        for slot, sl in sorted(((int(s_), v) for s_, v in js["actors"][aid]["slots"].items())):
            if aid == "12" and slot >= 844:
                continue
            for t in sorted(sl.get("triggers", []), key=lambda t: t["frame"]):
                if t["type"] == "vsb_voice2":
                    trigs.append((base + slot, t["frame"], 1, int(t["vtbl"]), 0, 0, 0)); NFORMSE += 1
                elif t["type"] in ("play_sound", "footstep_sound"):
                    ids = [i for i in (want(k) for k in t.get("sounds", [])) if i >= 0]
                    if ids:
                        trigs.append((base + slot, t["frame"], 0, 0, 1 if t.get("cycle") else 0, len(ids), len(trig_sounds)))
                        trig_sounds += ids; NFORMSE += 1
    print(f"  form-own triggers packed: {NFORMSE}")
    names.append(("formse_v1", want(js["effects"]["wisdom.0"]["sounds"][0])))
    for fn, k in (("anti_enter_100", "anti.100"),):  # (anti.100's SE: se000 #132, the Anti entry's second sound)
        i = want(js["effects"][k]["sounds"][0])
        if i >= 0: names.append((fn, i))
    # s93 b7-ffx: the SE of each Final swing effect element (json "effects" final.20..64, element +0x18 SE: se999 #25.. #36, #3, #8, #9, #1) as
    # final_fx_<N>, played with the fx (sora_ffx.h). Elements without a sound in KH2 stay silent. Appended last.
    for k, e in sorted(js["effects"].items(), key=lambda x: (x[0].split(".")[0], int(x[0].split(".")[1]) if x[0].split(".")[1].isdigit() else 0)):
        if k.startswith("final.") and k.split(".")[1].isdigit() and 20 <= int(k.split(".")[1]) <= 64 and e.get("sounds"):
            i = want(e["sounds"][0])
            if i >= 0: names.append(("final_fx_" + k.split(".")[1], i))
    if a.all:
        for k in sounds:
            want(k)

    # the PCM
    data, recs, missing = bytearray(), [], []
    for key in keys:
        s = sounds[key]
        path = sndsrc.wavfile(s)
        if not os.path.exists(path):
            missing.append(key)
            recs.append((key, s["rate"], s["channels"], float(s.get("volume", 1.0)), 1 if s.get("loop") else 0, 0, 0))
            continue
        rate, ch, n, pcm = wav_pcm16(path)
        while len(data) % 4:
            data.append(0)
        recs.append((key, rate, ch, float(s.get("volume", 1.0)), 1 if s.get("loop") else 0, len(data), n))
        data += pcm

    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    hdr_size = 48
    body = bytearray()
    for key, rate, ch, vol, loop, off, n in recs:
        body += struct.pack("<48sIIfIII", key.encode()[:47], rate, ch, vol, loop, off, n)
    for t in trigs:
        body += struct.pack("<7I", *t)
    for i in trig_sounds:
        body += struct.pack("<I", i)
    for v in vtbls:
        body += struct.pack("<4I", *v)
    for snd, w in choices:
        body += struct.pack("<iI", snd, w)
    for name, i in names:
        body += struct.pack("<32sI", name.encode()[:31], i)
    for row, i in hits:
        body += struct.pack("<II", row, i)
    while (hdr_size + len(body)) % 16:
        body.append(0)
    data_off = hdr_size + len(body)
    hdr = struct.pack("<4s11I", b"SKSF", 1, len(recs), len(trigs), len(trig_sounds), len(vtbls), len(choices),
                      len(names), len(hits), data_off, len(data), 0)
    with open(a.out, "wb") as f:
        f.write(hdr + body + data)
    print(f"{a.out}: {len(recs)} sounds ({len(data) / 1e6:.1f} MB PCM16), {len(trigs)} slot triggers, {len(vtbls)} vtbl rows, "
          f"{len(names)} names, {len(hits)} hit rows; missing files: {missing or 'none'}")


if __name__ == "__main__":
    main()

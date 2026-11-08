#!/usr/bin/env python3
"""sorabuild step sounds_mine (s182): the whole sound recipe from the player's own KH2 files -> work/sounds_recipe.json.

  python -E -P mine_recipe.py --kh2 <KH2 extract> --exe "<KINGDOM HEARTS II FINAL MIX.exe>" --trigdir <dir of trig_*.json>
         --sumdata <dir of <peterpan|tink|genie>/<key>[call]_motion.bin> --work <scratch dir> --out <sounds_recipe.json>

Replaces the old frozen tools/sorabuild/sounds_recipe.json (generated once from PHH's mined tables by the retired
sounds/dev/gen_recipe.py).  The sections and who mines them:
  actors, motions        mine_actors.py   (msets, 00battle atkp/vtbl, weapon Seb banks, 12soundinfo, voice SCD cues)
  effects                mine_effects.py  (the PAX elements of fxrec/files82.txt)
  system, hud            mine_misc.py     (our 12 system names -> se000 ids; fontinfo.bar 'sys'; exe icon widths, sha1 pinned)
  sounds, scd_sha256     mine_sounds.py   (which sounds = what our sfx tools ask for, traced on silent wavs; fields from the SCDs)
Section order and JSON layout = the old file's (version, note, sounds, actors, effects, system, motions, hud, scd_sha256;
indent 0, no spaces).  Every section equals the old file's values (sounds/mine/cmp_part.py against
sounds/dev/recipe_reference.json) except one sound the old table lacked: se000 #306 (Roxas' run, vanilla KH2, added s182 by
the user's call; mine_sounds.KEEP_OUT); the "sounds" entry order differs too (the old one was PHH's table order; no consumer
reads the dict order: build_sounds.py looks sounds up by key).  Standard Python only (+ the sfx tools the trace runs).
"""
import argparse
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kh2snd  # noqa: E402
import mine_actors  # noqa: E402
import mine_effects  # noqa: E402
import mine_misc  # noqa: E402
import mine_sounds  # noqa: E402

NOTE = ("Sora in BotW sound recipe: sound ids, KH2 SCD file + audio stream, codec, volumes, trigger frames. Numbers and names "
        "only, no game data. Mined at install from the player's own KH2 files by sounds/mine/mine_recipe.py.")


def mine(kh2, exe, trigdir, sumdata, work, keep=False, log=print):
    t0 = time.time()
    cache = kh2snd.ScdCache(kh2)
    tables = mine_actors.mine(kh2)
    log("actors %d, motions %d (%.1f s)" % (len(tables["actors"]), len(tables["motions"]), time.time() - t0))
    effects, skipped = mine_effects.mine(kh2, cache)
    log("effects %d (%d elements without a playable sound skipped)" % (len(effects), len(skipped)))
    system = mine_misc.mine_system(cache)
    hud = {"font_widths": mine_misc.font_widths(kh2), "icon_widths": mine_misc.icon_widths(exe)}
    log("system %d, hud font %d / icon %d" % (len(system), len(hud["font_widths"]), len(hud["icon_widths"])))
    sounds, sha = mine_sounds.mine(kh2, tables, effects, system, trigdir, sumdata, work, keep, cache, log=log)
    log("sounds %d from %d SCDs (%.0f s)" % (len(sounds), len(sha), time.time() - t0))
    return {"version": 1, "note": NOTE, "sounds": sounds, "actors": tables["actors"], "effects": effects, "system": system,
            "motions": tables["motions"], "hud": hud, "scd_sha256": sha}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kh2", required=True)
    ap.add_argument("--exe", required=True, help="KINGDOM HEARTS II FINAL MIX.exe (Steam, sha1 pinned) for hud.icon_widths")
    ap.add_argument("--trigdir", required=True, help="dir with trig_*.json (sorabuild fx_trigjson)")
    ap.add_argument("--sumdata", required=True, help="dir with the baked summon motions (<key>/<key>_motion.bin, <key>call_motion.bin)")
    ap.add_argument("--work", required=True, help="scratch dir (wiped)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--keep", action="store_true", help="keep the scratch tree")
    a = ap.parse_args()
    recipe = mine(a.kh2, a.exe, a.trigdir, a.sumdata, a.work, a.keep, log=lambda m: print(m, flush=True))
    tmp = a.out + ".tmp"
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        json.dump(recipe, f, indent=0, separators=(",", ":"))
        f.write("\n")
    os.replace(tmp, a.out)
    print("recipe -> %s (%d bytes)" % (a.out, os.path.getsize(a.out)))


if __name__ == "__main__":
    main()

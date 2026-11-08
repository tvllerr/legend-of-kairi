#!/usr/bin/env python3
"""Mine the recipe's "effects" section from the player's own KH2 effect files (s182; replaces the PHH-mined table).

  python -E -P mine_effects.py --kh2 <KH2 extract> --out <effects.json>

Source: the 82 effect files of fxrec/files82.txt (our fxrec list: KH2 file + our recording prefix, e.g. magic/FIRE_1.mag
fire_1).  Each file's first PAX (BAR entry type 0x12) holds 0x50-byte elements:
  +0x02 u16 element id   -> effect name "<prefix>.<id>" (the first element with that id)
  +0x10 u32 flags        bit 0 -> "wait" gets +0.1 (OUR constant, see WAIT_PAD)
  +0x14 f32              -> "wait" = this + pad
  +0x18 i32 SE           KH2 sound id (bank * 1000 + index, -1 = none) -> "se", "sounds": [the recipe key]
An element enters only if its SE is a playable sound of the player's SCD (a sound entry with a track; KH2 keeps empty
slots of type 16, e.g. se000 #0 / #174, se250 #30, se267 #19 / #20, se283 #25).
Output: {"effects": {name: {"sounds": [key], "se": id, "wait": w}}} in file order (EFFECT_ORDER), then element order.
"""
import argparse
import json
import os
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kh2snd  # noqa: E402

TOOLS = os.path.normpath(os.path.join(HERE, "..", ".."))
FILES82 = os.path.join(TOOLS, "fxrec", "files82.txt")

# OUR constants -------------------------------------------------------------------------------------------------------
# wait = element f32 +0x14 + 0.1 when flags bit 0 is set (bound / spawned effects), else + 0.0. The 0.1 s pad is a
# hand-picked delay of the old table (no KH2 field holds it); the rule reproduces all 529 rows of the frozen recipe.
WAIT_PAD = 0.1
# dict order only (no consumer depends on it: sfxpack sorts the names itself): the party files come before Sora's.
EFFECT_ORDER_FIRST = ["donald", "goofy"]


def file_list():
    rows = []
    for ln in open(FILES82, encoding="utf-8"):
        ln = ln.strip()
        if ln and not ln.startswith("#"):
            rel, pre = ln.split()
            rows.append((rel, pre))
    first = [r for p in EFFECT_ORDER_FIRST for r in rows if r[1] == p]
    out = []
    for r in rows:
        if r[1] == "sora":
            out += first
        if r not in first:
            out.append(r)
    return out


def mine(kh2, cache=None):
    cache = cache or kh2snd.ScdCache(kh2)
    eff, skipped = {}, []
    for rel, pre in file_list():
        p = kh2snd.kh2_file(kh2, rel)
        if not p:
            raise SystemExit("effect file %s not found under %s" % (rel, kh2))
        for _, ident, e in kh2snd.pax_elements(p):
            name = "%s.%d" % (pre, ident)
            if name in eff:
                continue
            se = struct.unpack_from("<i", e, 0x18)[0]
            if se < 0:
                continue
            key = kh2snd.se_key(se)
            if not kh2snd.sound_record(cache, key):
                skipped.append((name, se))
                continue
            flags = struct.unpack_from("<I", e, 0x10)[0]
            wait = round(struct.unpack_from("<f", e, 0x14)[0] + (WAIT_PAD if flags & 1 else 0.0), 6)
            eff[name] = {"sounds": [key], "se": se, "wait": wait}
    return eff, skipped


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kh2", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    t0 = time.time()
    eff, skipped = mine(a.kh2)
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        json.dump({"effects": eff}, f, indent=0, separators=(",", ":"))
        f.write("\n")
    print("effects: %d (%d elements skipped: SE without a playable sound, %d with se 0) -> %s (%.1f s)" % (
        len(eff), len(skipped), sum(1 for _, s in skipped if s == 0), a.out, time.time() - t0))


if __name__ == "__main__":
    main()

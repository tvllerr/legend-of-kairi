#!/usr/bin/env python3
"""Mine the recipe's "system" and "hud" sections from the player's own KH2 files (s182; replaces PHH's tables).

  python -E -P mine_misc.py --kh2 <KH2 extract> --exe "<KINGDOM HEARTS II FINAL MIX.exe>" --out <misc.json>

system  the 12 menu / HUD sounds the runtime asks for by name (sfxpack NAMES "system" rows).  The names and which KH2 id
        each one is are OUR table (SYSTEM below): KH2 plays these se000 ids through its system-SE call
        0x1DD170(id, 0) -> 0x1DF790(2, id, ...) (Ghidra headless, KH2 FM exe sha1 2548F119...: call sites pass 1, 2, 3,
        4, 5, 7, 8, 19, 20, 32, 33 among others; danger 9 is started elsewhere).  The sound key is checked against the
        player's se000.win32.scd (a playable sound entry).
hud     font_widths = msg/us/fontinfo.bar entry 'sys' (280 bytes, one advance per glyph from code 32; hudbake reads the
        same entry); icon_widths = the icon-font advance table of the KH2 FM exe, u16[256] at RVA 0x5C8680 (.rdata),
        read from the exe file (sha1 pinned).  Without --exe the hud section has no icon_widths (and the run says so).
"""
import argparse
import hashlib
import json
import os
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kh2snd  # noqa: E402

# OUR constants -------------------------------------------------------------------------------------------------------
# runtime name -> KH2 se000 sound id (the names are the sfx.bin event names, see fx/sfxpack.py NAMES)
SYSTEM = [("cursor", 1), ("decide", 2), ("submenu", 3), ("cancel", 4), ("error", 5), ("lock_on", 7), ("lock_off", 8),
          ("danger", 9), ("page", 20), ("drive_bar", 32), ("drive_full", 33), ("game_over", 19)]
EXE_SHA1 = "2548f1191fe6587a34947952763ce6bb4f3b5c1a"  # Steam KH2 FM exe (the one fxrec runs)
ICON_RVA, ICON_N = 0x5C8680, 256
FONTINFO = "msg/us/fontinfo.bar"


def mine_system(cache):
    out = {}
    for name, se in SYSTEM:
        key = kh2snd.se_key(se)
        if not kh2snd.sound_record(cache, key):
            raise SystemExit("system sound %s: %s is not a playable sound in the player's SCD" % (name, key))
        out[name] = {"sound": key, "se": se}
    return out


def font_widths(kh2):
    p = os.path.join(kh2, "kh2_first", "original", FONTINFO)
    if not os.path.isfile(p):
        p = kh2snd.kh2_file(kh2, FONTINFO)
    if not p:
        raise SystemExit("%s not found under %s" % (FONTINFO, kh2))
    d = open(p, "rb").read()
    for t, nm, off, sz in kh2snd.bar_entries(d) or []:
        if nm == "sys":
            return list(d[off:off + sz])
    raise SystemExit("%s: no 'sys' entry" % p)


def pe_read(d, rva, n):
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    ns, so = struct.unpack_from("<H", d, pe + 6)[0], struct.unpack_from("<H", d, pe + 20)[0]
    for i in range(ns):
        o = pe + 24 + so + 40 * i
        vs, va, rs, ra = struct.unpack_from("<IIII", d, o + 8)
        if va <= rva and rva + n <= va + min(vs, rs):
            return d[ra + rva - va:ra + rva - va + n]
    raise SystemExit("RVA %#x not in a file-backed section" % rva)


def icon_widths(exe):
    d = open(exe, "rb").read()
    sha = hashlib.sha1(d).hexdigest()
    if sha != EXE_SHA1:
        raise SystemExit("%s: sha1 %s is not the KH2 FM exe this table was found in (%s)" % (exe, sha, EXE_SHA1))
    return list(struct.unpack("<%dH" % ICON_N, pe_read(d, ICON_RVA, 2 * ICON_N)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kh2", required=True)
    ap.add_argument("--exe", help="KINGDOM HEARTS II FINAL MIX.exe (Steam) for hud.icon_widths")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    t0 = time.time()
    cache = kh2snd.ScdCache(a.kh2)
    hud = {"font_widths": font_widths(a.kh2)}
    if a.exe:
        hud["icon_widths"] = icon_widths(a.exe)
    else:
        print("hud: no --exe, icon_widths left out")
    frag = {"system": mine_system(cache), "hud": hud}
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(frag, f, indent=0, separators=(",", ":"))
        f.write("\n")
    print("system %d, hud font %d / icon %d -> %s (%.1f s)" % (len(frag["system"]), len(hud["font_widths"]),
                                                             len(hud.get("icon_widths", [])), a.out, time.time() - t0))


if __name__ == "__main__":
    main()

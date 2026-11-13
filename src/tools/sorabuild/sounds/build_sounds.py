#!/usr/bin/env python3
"""sorabuild "sounds": recipe -> the sound inputs of the sfx tools, from the player's OWN KH2 SCD files.

  python -E -P build_sounds.py --recipe <work/sounds_recipe.json> --kh2 <KH2 extract> --out <work/sounds>
         [--jobs N] [--vgmstream <vgmstream-cli.exe>] [--only-adpcm]

Writes the tree that fx/sndsrc.py reads (same layout as the old PHH cache, only what we use):
  mined/kh2_sounds.kh.json   the recipe's sounds/actors/effects/system (numbers and names only)
  bdx/motions.kh.json        the two mset trigger tables chickfx / stitchfx read
  kh2_hud.json               font + icon advance widths for hudbake
  kh2_sfx/<se|voice>/...wav  every sound of the recipe, decoded from <kh2>/<recipe sound .scd>

Codecs: "adpcm" (Microsoft ADPCM, SCD codec 12) is decoded here with fx/mkextra.py (bit-exact to vgmstream).
"vorbis" (SCD codec 6, the Sora/Donald/Goofy battle voices) needs vgmstream-cli (--vgmstream, or on PATH, or
<tools>/sounds/bin/vgmstream-cli[.exe] from fetch_vgmstream.py; exe sha256 pinned in VGM_KNOWN).  If it is missing the run says
"vorbis voices: vgmstream-cli missing" and exits with an error (the old --vorbis-fallback <PHH cache> option was removed in s163).
"""
import argparse
import hashlib
import json
import multiprocessing
import os
import shutil
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fx"))
sys.path.insert(0, os.path.join(HERE, ".."))
import mkextra  # noqa: E402  (our own ADPCM decoder)
import pesig    # noqa: E402  (s201: the pin also matches the exe with an Authenticode signature added, e.g. by Melty)


def scd_audio_streams(d):
    """[(stream index, codec)] of the playable audio streams (SCD codec 12 = MS ADPCM, 6 = Ogg Vorbis) in table order =
    vgmstream's subsong numbering (subsong N = the N-th such stream; empty slots and other codecs are skipped; proven by the
    bit-exact compare in recon/S156_HELPER_SOUNDS.md)."""
    hs = struct.unpack_from("<H", d, 0x0E)[0]
    nsnd, ntrk, nattr, _, otrk, osnd = struct.unpack_from("<HHHHII", d, hs)
    offs = struct.unpack_from("<%dI" % nsnd, d, osnd)
    out = []
    for i, o in enumerate(offs):
        if o:
            codec = struct.unpack_from("<I", d, o + 12)[0]
            if codec in (6, 12):
                out.append((i, codec))
    return out


# s157: only this build is proven to give the PHH-identical voices (133/133 byte-identical); fetch_vgmstream.py pins its zip
VGM_KNOWN = {"29df08c557ada8269c92a6abfe3886ad2f65849b0b26ba78a0819b363bbc5b85": "r2117 win64 vgmstream-cli.exe"}


def find_vgmstream(arg, any_build=False):
    """--vgmstream path, else sounds/bin/ (fetch_vgmstream.py). No PATH search: an unpinned build is refused unless
    --vgmstream-any (dev, e.g. a Linux build on the Deck until its hash is pinned). (s201) The pinned exe with an Authenticode
    signature added (Melty signs a mod's native files at install) is the pinned build too: pesig rebuilds the unsigned bytes."""
    for c in (arg, os.path.join(HERE, "bin", "vgmstream-cli.exe"), os.path.join(HERE, "bin", "vgmstream-cli")):
        if c and os.path.isfile(c):
            sha = hashlib.sha256(open(c, "rb").read()).hexdigest()
            hit = next(((k, how) for k in VGM_KNOWN for how in [pesig.matches(c, k)] if how), None)
            if hit:
                print("vgmstream-cli: %s (%s, pinned build%s)" % (c, VGM_KNOWN[hit[0]], ", signed" if hit[1] == "signed" else ""),
                      flush=True)
            elif any_build:
                print("vgmstream-cli: %s UNPINNED build (sha256 %s, --vgmstream-any)" % (c, sha), flush=True)
            else:
                raise SystemExit("vgmstream-cli %s is not the pinned build (sha256 %s); run sounds/fetch_vgmstream.py "
                                 "or pass --vgmstream-any" % (c, sha))
            return c
    return None


def decode_vorbis(vgm, scd, subsong, out):
    """vgmstream-cli -s <subsong> -o <wav> <scd>   (subsong is 1-based)"""
    os.makedirs(os.path.dirname(out), exist_ok=True)
    r = subprocess.run([vgm, "-s", str(subsong), "-o", out, scd], capture_output=True)
    if r.returncode or not os.path.exists(out):
        raise SystemExit("vgmstream-cli failed on %s subsong %d: %s" % (scd, subsong, r.stderr[-300:]))


def _job(task):
    """one SCD: [(stream, out)] all ADPCM -> wavs (runs in a worker process)"""
    scd, items = task
    d = open(scd, "rb").read()
    for stream, out in items:
        ch, rate, ba, coefs, data = mkextra.scd_stream(scd, stream, d)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        mkextra.write_wav(out, ch, rate, mkextra.decode_msadpcm(ch, ba, coefs, data))
    return len(items)


def write_json(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        json.dump(obj, f, separators=(",", ":"))  # key order is part of the data (pack order)
    os.replace(tmp, path)


def check_scds(kh2, want):
    """The recipe's stream numbers are only right for the same SCD files as the dev PC: compare their sha256 first."""
    if not want:
        raise SystemExit("recipe has no scd_sha256 table (mine it with sounds/mine/mine_recipe.py, step sounds_mine)")
    bad = []
    for rel, sha in sorted(want.items()):
        p = os.path.join(kh2, rel)
        if not os.path.exists(p):
            bad.append("%s: missing" % rel)
        elif hashlib.sha256(open(p, "rb").read()).hexdigest() != sha:
            bad.append("%s: different file (not the KH2 build this recipe was made from)" % rel)
    if bad:
        raise SystemExit("KH2 sound files do not match the recipe:" + "".join("\n  " + x for x in bad))
    print("source SCDs: %d match the recipe (sha256)" % len(want), flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--recipe", required=True)
    ap.add_argument("--kh2", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--jobs", type=int, default=0)
    ap.add_argument("--vgmstream")
    ap.add_argument("--vgmstream-any", action="store_true", help="accept an unpinned vgmstream-cli build (dev only)")
    ap.add_argument("--vorbis-fallback", help=argparse.SUPPRESS)  # removed (s163): refused with a message below
    ap.add_argument("--only-adpcm", action="store_true", help="decode only the ADPCM wavs (proof runs)")
    ap.add_argument("--no-scd-check", action="store_true", help="skip the sha256 check of the source SCDs (dev only)")
    a = ap.parse_args()
    if a.vorbis_fallback:
        raise SystemExit("--vorbis-fallback was removed: install vgmstream-cli r2117 (sounds/fetch_vgmstream.py) or pass --vgmstream")
    t0 = time.time()
    rc = json.load(open(a.recipe, encoding="utf-8"))
    assert rc["version"] == 1
    if not a.no_scd_check:
        check_scds(a.kh2, rc.get("scd_sha256") or {})
    out = a.out
    # 1. the tables (exactly the slice of kh2_sounds.kh.json the tools read)
    snd = {k: {f: v[f] for f in ("file", "rate", "channels", "volume", "loop") if f in v} for k, v in rc["sounds"].items()}
    write_json(os.path.join(out, "mined", "kh2_sounds.kh.json"),
               {"sounds": snd, "actors": rc["actors"], "effects": rc["effects"], "system": rc["system"]})
    write_json(os.path.join(out, "bdx", "motions.kh.json"), {"files": rc["motions"]})
    write_json(os.path.join(out, "kh2_hud.json"), {"font": {"widths": rc["hud"]["font_widths"]},
                                                   "icon_widths": {"widths": rc["hud"]["icon_widths"]}})
    # 2. the wavs
    by_scd, vorbis = {}, []
    for key, v in rc["sounds"].items():
        dst = os.path.join(out, "kh2_sfx", v["file"])
        if v["codec"] == "adpcm":
            by_scd.setdefault(v["scd"], []).append((v["stream"], dst))
        elif v["codec"] == "vorbis":
            vorbis.append((v["scd"], v["subsong"], dst, v["file"]))
        else:
            raise SystemExit("unknown codec %r for %s" % (v["codec"], key))
    tasks = []
    for scd, items in sorted(by_scd.items()):
        p = os.path.join(a.kh2, scd)
        if not os.path.exists(p):
            raise SystemExit("missing KH2 file %s (recipe sound source)" % p)
        tasks.append((p, items))
    n = 0
    jobs = a.jobs or max(1, min(len(tasks), (os.cpu_count() or 2) - 1))
    if jobs > 1:
        with multiprocessing.Pool(jobs) as pool:
            for k in pool.imap_unordered(_job, tasks):
                n += k
    else:
        for t in tasks:
            n += _job(t)
    print("adpcm wavs decoded: %d (%.0f s)" % (n, time.time() - t0), flush=True)
    if a.only_adpcm:  # proof runs only: the tree is INCOMPLETE, so never report success
        print("--only-adpcm: %d vorbis voices skipped (incomplete tree, exit 3)" % len(vorbis))
        sys.exit(3)
    vgm = find_vgmstream(a.vgmstream, a.vgmstream_any)
    if vorbis and not vgm:
        print("vorbis voices: vgmstream-cli missing (%d wavs)" % len(vorbis), flush=True)
        raise SystemExit("vorbis voices: vgmstream-cli missing. Run sounds/fetch_vgmstream.py or pass --vgmstream <exe>")
    else:
        for scd, sub, dst, rel in vorbis:
            decode_vorbis(vgm, os.path.join(a.kh2, scd), sub, dst)
        print("vorbis voices: %d wavs decoded with %s" % (len(vorbis), vgm))
    print("sounds tree ready: %s (%.0f s)" % (out, time.time() - t0))


if __name__ == "__main__":
    multiprocessing.freeze_support()
    main()

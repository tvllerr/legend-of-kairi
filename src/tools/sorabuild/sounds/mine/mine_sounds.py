#!/usr/bin/env python3
"""Mine the recipe's "sounds" and "scd_sha256" sections from the player's own KH2 SCD files (s182; replaces PHH's table).

  python -E -P mine_sounds.py --kh2 <KH2 extract> --tables <json with "actors" + "motions"> --sbwork <sorabuild work dir>
         (or --trigdir <dir of trig_*.json> --sumdata <dir of the baked summon motions> instead of --sbwork)
         --work <scratch dir> --out <sounds.json> [--effects effects.json] [--misc misc.json] [--keep]

What a sound record is (all from the SCD, see kh2snd.py): key "<bank path>#<sound index>" -> file "<bank>/<NNN>.wav",
rate / channels / codec of the sound's first track's audio stream, volume = the sound entry's f32 volume rounded to 2
decimals, scd = where the bank is in the extract, stream = that audio index, subsong = its vgmstream subsong number.

WHICH sounds (the selection) = exactly what OUR sfx tools ask for, the same way gen_recipe.py's --trace did it, but with
KH2's full sound list instead of PHH's table:
  1. every playable sound of every KH2 se/ + voice/us/battle/ bank (~25.5k) goes into a scratch sounds tree laid out like
     work/sounds (mined/kh2_sounds.kh.json with --tables' actors, our effects + system, bdx/motions.kh.json, a 16-frame
     silent wav per sound);
  2. the sfx.bin chain of steps/fx.py runs on it with SB_WAVTRACE (sfxpack, sfxadd_s98, sfxadd_s107, sfxroxas, sfxmickey,
     chickfx --no-fx, stitchfx --no-fx, sumsfx, ksse, dfse; sfxskate reads no recipe sound and is skipped); it needs the
     trig_*.json (fx_trigjson) and the baked summon motions (fxsumdata) of a sorabuild work dir (--sbwork, read only);
  3. selection = the traced wavs + LVUP_WAV (sfx_lvup_wav reads it directly), minus the sounds our tools bring themselves
     (sfxpack EXTRA: se500 #10 comes from step sfx_extra_se500) and KEEP_OUT (gaps of the old table kept for identity; empty since s182).
--tables: the "actors" / "motions" sections (mine_actors.py's fragment, or sounds/dev/recipe_reference.json).
"loop": only on system sounds whose SCD sound has the loop attribute (bit 0): OUR rule, see LOOP_ONLY_SYSTEM.
Order of "sounds": the order the tools first ask for them (dict order is not read by any consumer).
"""
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kh2snd  # noqa: E402
import mine_effects  # noqa: E402
import mine_misc  # noqa: E402

TOOLS = os.path.normpath(os.path.join(HERE, "..", ".."))
FX = os.path.join(TOOLS, "fx")
BANK_DIRS = ["se", "voice/us/battle"]

# OUR constants -------------------------------------------------------------------------------------------------------
LVUP_WAV = "se/se000/025.wav"            # steps/fx.py sfx_lvup_wav reads it directly (not through sndsrc): always in
TOOL_EXTRA = {"se/se500.win32.scd#10"}   # fx/sfxpack.py EXTRA: our own decode (step sfx_extra_se500), never a recipe sound
# Sounds a tool asks for that the frozen recipe never had (the old PHH table lacked them, so the tool skipped the trigger).
# A line here leaves the sound out (old-table identity); without it the vanilla sound plays. (s182, user's call: match vanilla
# KH2) se000 #306 was dropped from here: Roxas' run (P_EX110 slot 8 R002, frame 28 play_sound 0/306, sfxroxas.py) now plays it,
# as in vanilla (Sora's run plays se000 #301 there).
KEEP_OUT = set()
# KH2 flags 6 of the selected sounds as looping (se000 #9 / #135 / #153 / #169, se999 #45 / #50), but the runtime only
# stops a looping sound for the system "danger" beep; the other five play once (the frozen recipe: loop on se000 #9 only).
LOOP_ONLY_SYSTEM = True


def all_banks(kh2):
    rels = set()
    for pk in os.listdir(kh2):
        for sub in ("original", "remastered"):
            for bd in BANK_DIRS:
                p = os.path.join(kh2, pk, sub, bd)
                if os.path.isdir(p):
                    rels.update(bd + "/" + f for f in os.listdir(p) if f.endswith(".win32.scd"))
    return sorted(rels)


def full_table(kh2, cache):
    snd = {}
    for rel in all_banks(kh2):
        _, scd = cache.get(rel)
        for i in range(len(scd.snd_offs)):
            r = kh2snd.sound_record(cache, "%s#%d" % (rel, i))
            if r:
                snd["%s#%d" % (rel, i)] = r
    return snd


def silent_wav(ch, rate, frames=16):
    data = b"\0" * (2 * ch * frames)
    return (b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " +
            struct.pack("<IHHIIHH", 16, 1, ch, rate, rate * 2 * ch, 2 * ch, 16) + b"data" + struct.pack("<I", len(data)) + data)


def write_json(p, o):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        json.dump(o, f, separators=(",", ":"))


def trace(work, full, tables, effects, system, trigdir, sumdata, kh2):
    """run the sfx chain on the full table; returns the traced wav paths in first-request order"""
    if os.path.isdir(work):
        shutil.rmtree(work)
    root = os.path.join(work, "sounds")
    t0 = time.time()
    write_json(os.path.join(root, "mined", "kh2_sounds.kh.json"),
               {"sounds": {k: {f: v[f] for f in ("file", "rate", "channels", "volume")} for k, v in full.items()},
                "actors": tables["actors"], "effects": effects, "system": system})
    write_json(os.path.join(root, "bdx", "motions.kh.json"), {"files": tables["motions"]})
    blobs = {}
    for v in full.values():
        p = os.path.join(root, "kh2_sfx", v["file"])
        os.makedirs(os.path.dirname(p), exist_ok=True)
        b = blobs.get((v["channels"], v["rate"])) or blobs.setdefault((v["channels"], v["rate"]), silent_wav(v["channels"], v["rate"]))
        with open(p, "wb") as f:
            f.write(b)
    print("trace tree: %d silent wavs (%.0f s)" % (len(full), time.time() - t0), flush=True)
    # inputs the chain needs besides the sounds (stand-ins where the sfx side never looks at them)
    fxdir = os.path.join(work, "fxdir")
    write_json(os.path.join(fxdir, "kh2_fx.json"), {"effects": {"hb040.%d" % e: {"bone": 0} for e in (1, 4, 6, 7, 9, 13)}})
    extra = os.path.join(work, "extra")
    os.makedirs(os.path.join(extra, "se", "se500"))
    with open(os.path.join(extra, "se", "se500", "010.wav"), "wb") as f:
        f.write(silent_wav(2, 48008))
    for sub, src in (("fxtrig", trigdir), ("fxsumdata", sumdata)):
        if not os.path.isdir(src):
            raise SystemExit("%s: no such dir (the %s input: sorabuild fx_trigjson / the summon motion bakes)" % (src, sub))
        shutil.copytree(src, os.path.join(work, sub))  # a copy: the tools never touch the inputs
    tr = os.path.join(work, "wavtrace.txt")
    env = dict(os.environ, SB_SNDROOT=root, SB_WAVTRACE=tr, SB_FXCACHE=fxdir, SB_FXDIR=fxdir, SB_KH2=kh2, SB_EXTRA=extra,
               SB_TRIGDIR=os.path.join(work, "fxtrig"), SB_SUMDATA=os.path.join(work, "fxsumdata"))
    open(tr, "w").close()
    ch = os.path.join(work, "chain")
    os.makedirs(ch)
    cur = os.path.join(ch, "sfx.bin")
    n = [0]

    def run(script, *args, out_dir=None):
        t = time.time()
        r = subprocess.run([sys.executable, "-E", "-P", os.path.join(FX, script)] + list(args), env=env,
                           capture_output=True, text=True)
        if r.returncode:
            raise SystemExit("%s failed:\n%s\n%s" % (script, r.stdout[-2000:], r.stderr[-2000:]))
        lines = sum(1 for _ in open(tr, encoding="utf-8"))
        print("  %-14s ok (%.1f s), wav requests %d -> %d" % (script, time.time() - t, n[0], lines), flush=True)
        n[0] = lines

    def tmp(name):
        d = os.path.join(work, "tmp", name)
        os.makedirs(d, exist_ok=True)
        return d

    run("sfxpack.py", "--cache", fxdir, "--out", cur)
    for script, extra_args in (("sfxadd_s98.py", []), ("sfxadd_s107.py", []),
                               ("sfxroxas.py", [env["SB_TRIGDIR"]]), ("sfxmickey.py", [env["SB_TRIGDIR"]])):
        run(script, cur, cur + ".new", *extra_args)
        os.replace(cur + ".new", cur)
    d = tmp("chick")
    run("chickfx.py", "--no-fx", "--sfx-in", cur, "--fx-in", cur, "--out", d, "--header", os.path.join(d, "h.h"))
    os.replace(os.path.join(d, "sfx.bin"), cur)
    d = tmp("stitch")
    run("stitchfx.py", "--no-fx", "--sfx-in", cur, "--fx-in", cur, "--out", d, "--fx", "6,7,9", "--header", os.path.join(d, "h.h"))
    os.replace(os.path.join(d, "sfx.bin"), cur)
    for script, name in (("sumsfx.py", "sum"), ("ksse.py", "ksse"), ("dfse.py", "dfse")):
        d = tmp(name)
        run(script, "build", "--sfx-in", cur, "--out", d)
        os.replace(os.path.join(d, "sfx.bin"), cur)
    seen, order = set(), []
    for ln in open(tr, encoding="utf-8"):
        ln = ln.strip()
        if ln and ln not in seen:
            seen.add(ln)
            order.append(ln)
    return order


def mine(kh2, tables, effects, system, trigdir, sumdata, work, keep=False, cache=None, log=print):
    """-> (sounds, scd_sha256): the two recipe sections. tables = {"actors", "motions"}; trigdir = dir with trig_*.json
    (fx_trigjson); sumdata = dir with <peterpan|tink|genie>/<key>[call]_motion.bin (the summon bakes); work = scratch (wiped)."""
    t0 = time.time()
    cache = cache or kh2snd.ScdCache(kh2)
    full = full_table(kh2, cache)
    log("KH2 sounds: %d playable in %d banks (%.0f s)" % (len(full), len(cache.c), time.time() - t0))
    order = trace(work, full, tables, effects, system, trigdir, sumdata, kh2)
    by_file = {v["file"]: k for k, v in full.items()}
    if LVUP_WAV not in order:
        order.append(LVUP_WAV)
    unknown = [f for f in order if f not in by_file]
    if unknown:
        raise SystemExit("traced wavs that are no KH2 sound: %s" % unknown[:10])
    sys_keys = {v["sound"] for v in system.values()}
    sounds = {}
    for f in order:
        k = by_file[f]
        if k in TOOL_EXTRA or k in KEEP_OUT:
            continue
        r = dict(full[k])
        loop = r.pop("_loop_flag") and (k in sys_keys or not LOOP_ONLY_SYSTEM)
        e = {x: r[x] for x in ("file", "rate", "channels", "volume")}
        if loop:
            e["loop"] = True
        e.update({x: r[x] for x in ("scd", "stream", "subsong", "codec")})
        sounds[k] = e
    sha = {}
    for rel in sorted({e["scd"] for e in sounds.values()}):
        with open(os.path.join(kh2, rel), "rb") as f:
            sha[rel] = hashlib.sha256(f.read()).hexdigest()
    if not keep:
        shutil.rmtree(work, ignore_errors=True)
    return sounds, sha


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kh2", required=True)
    ap.add_argument("--tables", required=True, help='json with "actors" and "motions" (helper A fragment or the recipe)')
    ap.add_argument("--sbwork", help="sorabuild work dir with fxtrig/ and fxsumdata/ (read only)")
    ap.add_argument("--trigdir", help="dir with trig_*.json (else <sbwork>/fxtrig)")
    ap.add_argument("--sumdata", help="dir with the baked summon motions (else <sbwork>/fxsumdata)")
    ap.add_argument("--work", required=True, help="scratch dir (wiped)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--effects", help="mine_effects.py output (else mined here)")
    ap.add_argument("--misc", help="mine_misc.py output (system; else mined here)")
    ap.add_argument("--keep", action="store_true", help="keep the scratch tree")
    a = ap.parse_args()
    trigdir = a.trigdir or (a.sbwork and os.path.join(a.sbwork, "fxtrig"))
    sumdata = a.sumdata or (a.sbwork and os.path.join(a.sbwork, "fxsumdata"))
    if not trigdir or not sumdata:
        ap.error("give --sbwork or both --trigdir and --sumdata")
    t0 = time.time()
    cache = kh2snd.ScdCache(a.kh2)
    tables = json.load(open(a.tables, encoding="utf-8"))
    effects = json.load(open(a.effects, encoding="utf-8"))["effects"] if a.effects else mine_effects.mine(a.kh2, cache)[0]
    system = json.load(open(a.misc, encoding="utf-8"))["system"] if a.misc else mine_misc.mine_system(cache)
    sounds, sha = mine(a.kh2, tables, effects, system, trigdir, sumdata, a.work, a.keep, cache,
                       log=lambda m: print(m, flush=True))
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        json.dump({"sounds": sounds, "scd_sha256": sha}, f, indent=0, separators=(",", ":"))
        f.write("\n")
    nc = {"adpcm": 0, "vorbis": 0}
    for v in sounds.values():
        nc[v["codec"]] += 1
    print("sounds: %d (%s), %d SCDs -> %s (%.0f s)" % (len(sounds), nc, len(sha), a.out, time.time() - t0))


if __name__ == "__main__":
    main()

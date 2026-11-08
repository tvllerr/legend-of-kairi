#!/usr/bin/env python3
"""(s137 sumdata) APPENDS Peter Pan's (pp_fx_*) and Genie's (gn_fx_*) effect elements to the LIVE fx.bin (Tinker Bell has no recorded effect: none).
Reuses chickfx.py's encoder (build_fx: copies the old pack byte for byte, appends elements/frames/batches/verts/textures, verifies the old
rows are identical). Source of the frames = the ProjectHeartHunters effect recording cache (DATA: recordings of KH2's own P_EX330 / P_AL010 .a.us effects).

  python -I sumfx.py plan  [--fx-in LIVE]                 sizes only (decompresses the source frames, writes nothing): projected fx.bin size
  python -I sumfx.py build [--fx-in LIVE] --out OUTDIR    writes OUTDIR/fx.bin (= live + new elements; never touches the live file)
Idempotent: a summon whose prefix is already in the pack is skipped (a partial set is an error). The uint32 offset cap (4 GiB) is checked.
Loop-end tail variants (al010.N~M, 720 near-copies of the loops) are NOT packed, like the cl_fx tails.  --cap-loops N keeps only the first N frames of 600 frame loops."""
import argparse, importlib.util, os, struct, sys, zlib
HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("chickfx", os.path.join(HERE, "chickfx.py"))
cf = importlib.util.module_from_spec(spec); spec.loader.exec_module(cf)
fxb = cf.fxb
CAP32 = (1 << 32) - 1
SETS = [("pp_fx_", "ex330.%d", list(range(0, 29))), ("gn_fx_", "al010.%d", list(range(0, 82)))]


def pack_names(path):
    f = open(path, "rb"); h = fxb.OUT_HDR.unpack(f.read(fxb.OUT_HDR.size))
    assert h[0] == b"SKFX" and h[1] == 1
    f.seek(h[9]); raw = f.read(64 * h[3])
    return h, {raw[i * 64:i * 64 + 32].split(b"\0")[0].decode() for i in range(h[3])}


def plan(src, caploops):
    psrc = fxb.Phf2(os.path.join(cf.FXCACHE, "kh2_fx.bin"))
    tot = dict(fr=0, ba=0, ve=0, el=0)
    for pre, fmt, els in SETS:
        s = dict(fr=0, ba=0, ve=0)
        for n in els:
            name = fmt % n
            m = psrc.effects[name]
            cap = caploops if (caploops and m[3] > caploops) else 0
            fr = ba = ve = 0
            for frame in cf.frames_iter(psrc, name, cap):
                fr += 1; ba += len(frame); ve += sum(len(vs) for _, vs in frame)
            by = ve * 40 + ba * 52 + fr * 8 + 64
            print("  %-10s = %-9s %4d frames %6d batches %9d verts %8.2f MB" % (pre + str(n), name, fr, ba, ve, by / 1e6)); sys.stdout.flush()
            s["fr"] += fr; s["ba"] += ba; s["ve"] += ve
        by = s["ve"] * 40 + s["ba"] * 52 + s["fr"] * 8 + 64 * len(els)
        print("%s total: %d frames %d batches %d verts = %.1f MB (+ new textures)" % (pre, s["fr"], s["ba"], s["ve"], by / 1e6))
        for k in s: tot[k] += s[k]
        tot["el"] += len(els)
    by = tot["ve"] * 40 + tot["ba"] * 52 + tot["fr"] * 8 + 64 * tot["el"]
    live = os.path.getsize(src)
    print("ALL: +%.1f MB; live %d B -> projected %d B (%.2f GiB; uint32 cap 4294967295 B = %.1f %% used)" % (by / 1e6, live, live + by, (live + by) / 2**30, 100.0 * (live + by) / CAP32))
    return live + by


def build(src, outdir):
    os.makedirs(outdir, exist_ok=True)
    cur = src; tmps = []
    for k, (pre, fmt, els) in enumerate(SETS):
        h, names = pack_names(cur)
        have = [n for n in names if n.startswith(pre)]
        if len(have) == len(els):
            print(pre, "already in the pack: skipped"); continue
        assert not have, "%s partial set (%d of %d): restore the .pre-s137d backup first" % (pre, len(have), len(els))
        cf.FX_PREFIX = pre; cf.FX_SRC_FMT = fmt; cf.FX_ELEMENTS = els
        dst = os.path.join(outdir, "fx_%d.tmp" % k)
        cf.build_fx(cur, dst, 0)
        if cur != src: os.remove(cur)
        cur = dst; tmps.append(dst)
    final = os.path.join(outdir, "fx.bin")
    if cur == src:
        print("nothing to do"); return None
    assert os.path.getsize(cur) <= CAP32, "fx.bin would pass the uint32 offset cap"
    os.replace(cur, final)
    print("wrote", final, os.path.getsize(final), "B")
    return final


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["plan", "build"])
    ap.add_argument("--fx-in", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--cap-loops", type=int, default=0)
    a = ap.parse_args()
    plan(a.fx_in, a.cap_loops) if a.cmd == "plan" else build(a.fx_in, a.out)

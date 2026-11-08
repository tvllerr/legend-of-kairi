#!/usr/bin/env python3
"""(s166 fxcensus) Which of the recorded KH2 effects does the mod actually use?  READ-ONLY: it only reads the fx tools' tables,
the DLL sources, the trig_*.json files and (optionally) a finished kh2_fx.bin / kh2_fx.json.  It records and builds nothing.

  python -E -P fxcensus.py [--fxdir <dir with kh2_fx.bin + kh2_fx.json>] [--runtime <runtime dir>] [--trig <dir with trig_*.json>]
                           [--times <s161times.txt>] [--live <names.txt>] [--keep keep.json] [--report report.txt]

How the mod reaches an effect: runtime/*.c|h only ever ask fx.bin by NAME (sora_fx_id("fire_ring"), snprintf'd names such as
"kb%s_hit", cue tables of the summons); the network sends the fx.bin INDEX.  fx.bin is built by the fx chain (steps/fx.py) from a
FIXED list of source elements ("<file prefix>.<element>" or "<prefix>.<element>~<tail frame>" in kh2_fx.json).  So the kept
recordings = the source element of every fx.bin entry.  The DLL cannot reach a kh2_fx element that no chain tool packs.

keep list (--keep) = {"elements": {"fire_1": [0,1,..]}, "tails": {"fire_1.0": [22]}, "sources": [[pack, source spec, tool],..]}
"""
import argparse
import collections
import importlib.util
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
for k in ("SB_FXCACHE", "SB_FXDIR", "SB_TRIGDIR", "SB_KH2", "SB_SNDROOT"):
    os.environ.setdefault(k, os.path.join(os.environ.get("TEMP", "."), "fxcensus_dummy"))


def load(name, path=None):
    spec = importlib.util.spec_from_file_location(name, path or os.path.join(HERE, name + ".py"))
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


def split_spec(spec):
    """'a[:40]+b[40:]' -> ['a', 'b'] (slices of recordings joined, fxbake.py); 'x.0~22' stays one name."""
    return [re.sub(r"\[[^\]]*\]$", "", p) for p in spec.split("+")]


def sources():
    """[(pack name, source recording name, tool)] from every fx chain tool (modules imported, tables read, nothing run)."""
    out = []
    fxb = load("fxbake")
    for pk, el, _cap in fxb.ELEMENTS:
        for s in split_spec(el):
            out.append((pk, s, "fxbake"))
    prz = load("prizefx")
    out.append(("prize_*", "prize.0", "prizefx"))
    mg = load("magicfx")
    for pk, el, _cap, _f in mg.ELEMENTS:
        out.append((pk, el, "magicfx"))
    lf = load("limitfx")
    for pk, el in lf.ELEMENTS:
        out.append((pk, el, "limitfx"))
    cf = load("chickfx")
    for n in cf.FX_ELEMENTS:
        out.append((cf.FX_PREFIX + str(n), cf.FX_SRC_FMT % n, "chickfx"))
    src = open(os.path.join(HERE, "fpsfx.py"), encoding="utf-8").read()  # fpsfx: module-level script, so read its constants
    m = re.search(r'FX_SRC_FMT = "([^"]+)"; cf\.FX_ELEMENTS = list\(range\((\d+), (\d+)\)\)', src)
    for n in range(int(m.group(2)), int(m.group(3))):
        out.append(("clf_fx_%d" % n, m.group(1) % n, "fpsfx"))
    sf = load("sumfx")
    for pre, fmt, els in sf.SETS:
        for n in els:
            out.append((pre + str(n), fmt % n, "sumfx"))
    st = open(os.path.join(TOOLS, "steps", "fx.py"), encoding="utf-8").read()  # stitchfx is run twice with --fx lists (steps/fx.py)
    for lst in re.findall(r'"--fx", "([0-9,]+)"', st):
        for n in lst.split(","):
            out.append(("st_fx_" + n, "hb040." + n, "stitchfx"))
    return out


def split_name(s):
    m = re.match(r"^(.+)\.(\d+)(?:~(\d+))?$", s)
    return m.group(1), int(m.group(2)), (int(m.group(3)) if m.group(3) else None)


def phf2_index(path):
    """name -> (offset, compressed bytes) of the effect frames, and the total file size (header + index only)."""
    f = open(path, "rb")
    magic, ver, nt, ne, tt, et = struct.unpack("<4sIIIII", f.read(24))
    f.seek(et)
    recs = []
    for _ in range(ne):
        r = f.read(struct.calcsize("<32sIIIIIhHI"))
        nm, off = struct.unpack_from("<32sI", r, 0)
        recs.append((nm.split(b"\0", 1)[0].decode(), off))
    size = os.path.getsize(path)
    offs = sorted(o for _, o in recs) + [size]
    nxt = {o: offs[i + 1] for i, o in enumerate(offs[:-1])}
    return {n: (o, nxt[o] - o) for n, o in recs}, size, tt, et


def keep_list():
    """(s166 fx_keep "used") the recordings the fx chain reads, from OUR tool tables only (no game bytes, no kh2_fx.*):
    {"elements": {prefix: [ids]}, "tails": {"prefix.id": [tail frames]}, "sources": [[pack, source, tool], ..]}.
    A tail source x.N~at also keeps its base element x.N (the same process decides alive / loop end)."""
    src = sources()
    elements = collections.defaultdict(set)
    tails = collections.defaultdict(set)
    for _pk, s, _t in src:
        pre, n, at = split_name(s)
        elements[pre].add(n)
        if at is not None:
            tails[pre + "." + str(n)].add(at)
    return dict(elements={k: sorted(v) for k, v in sorted(elements.items())}, tails={k: sorted(v) for k, v in sorted(tails.items())},
                sources=[list(x) for x in src])


def dll_names(runtime):
    """string literals of runtime/sora*.c|h (+ other runtime files that call sora_fx_*)"""
    lits = set()
    for fn in sorted(os.listdir(runtime)):
        if not fn.endswith((".c", ".h")) or not fn.startswith(("sora", "hle")):
            continue
        t = open(os.path.join(runtime, fn), encoding="utf-8", errors="replace").read()
        lits.update(re.findall(r'"([A-Za-z0-9_%.]{3,48})"', t))
    return lits


def dll_ref(packname, lits, pats):
    if packname in lits:
        return "literal"
    for p, lit in pats:
        if p.fullmatch(packname):
            return "pattern " + lit
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fxdir", default="D:/SoraBotW/sb/fxrecwork/fxrec/out")
    ap.add_argument("--runtime", default=os.path.join(TOOLS, "..", "..", "runtime"))
    ap.add_argument("--trig", default="D:/SoraBotW/sb/work/fxtrig")
    ap.add_argument("--times", default="D:/SoraBotW/recon/s161times.txt")
    ap.add_argument("--live", default="D:/SoraBotW/sb/s166census/live_names.txt")
    ap.add_argument("--keep")
    ap.add_argument("--check", help="(s166) dir with kh2_fx.bin + kh2_fx.json: every recording the fx chain tools read must be in it, else exit 1")
    ap.add_argument("--emit-keep", help="(s166) write ONLY the keep list (keep_list()) to this json and exit: needs no kh2_fx.*")
    ap.add_argument("--report")
    a = ap.parse_args()
    if a.check:
        meta = json.load(open(os.path.join(a.check, "kh2_fx.json"), encoding="utf-8"))["effects"]
        idx = phf2_index(os.path.join(a.check, "kh2_fx.bin"))[0]
        miss = sorted({s for _pk, s, _t in sources() if s not in meta or s not in idx})
        if miss:
            raise SystemExit("fxcensus: %d recording(s) the fx chain tools read are missing from %s: %s\n  (fx_keep 'used' records only the "
                             "keep list = these tables; a tool that reads another element needs it added to its table, or set \"fx_keep\": \"all\")" % (
                                 len(miss), a.check, ", ".join(miss[:40])))
        print("fxcensus: all %d recordings the fx chain reads are in %s" % (len({s for _pk, s, _t in sources()}), a.check))
        return
    if a.emit_keep:
        k = keep_list()
        with open(a.emit_keep, "w", encoding="utf-8", newline="\n") as f:
            json.dump(k, f, indent=1, sort_keys=True)
        print("fxcensus: keep list -> %s (%d files, %d elements, %d tails)" % (
            a.emit_keep, len(k["elements"]), sum(len(v) for v in k["elements"].values()), sum(len(v) for v in k["tails"].values())))
        return
    P = print
    lines = []

    def P(*x):
        s = " ".join(str(i) for i in x)
        lines.append(s)
        print(s)

    meta = json.load(open(os.path.join(a.fxdir, "kh2_fx.json"), encoding="utf-8"))["effects"]
    idx, binsize, tt, et = phf2_index(os.path.join(a.fxdir, "kh2_fx.bin"))
    src = sources()
    P("== (b) fx chain tools: %d (pack entry -> source recording) rows, %d distinct recordings" % (len(src), len({s for _, s, _ in src})))
    byt = collections.Counter(t for _, _, t in src)
    P("   rows per tool:", dict(byt))
    missing = sorted({s for _, s, _ in src if s not in meta})
    P("   sources missing in kh2_fx.json:", missing or "none")

    keep = sorted({s for _, s, _ in src})
    # a tail "x.N~at" needs the base element x.N recorded (same process) + its loop end at `at`
    elements = collections.defaultdict(set)
    tails = collections.defaultdict(set)
    for s in keep:
        pre, n, at = split_name(s)
        elements[pre].add(n)
        if at is not None:
            tails[pre + "." + str(n)].add(at)
    # (c) trigger tables
    P("== (c) effect_caster triggers (mset frame trigger id 1, p1 = element) in the trig_*.json the chain uses")
    trigmap = {"P_EX200": "ex200", "P_EX110": "roxas", "W_EX200": "w_ex200", "W_EX010_RX": "w_ex010"}
    for tn, pre in trigmap.items():
        p = os.path.join(a.trig, "trig_%s.json" % tn)
        if not os.path.exists(p):
            P("   %s: file not found" % tn)
            continue
        j = json.load(open(p))
        els = sorted({f[3] for v in j.values() for f in v["frame"] if f[0] == 1})
        notk = [e for e in els if e not in elements.get(pre, set())]
        P("   %-10s elements called by triggers %s ; not kept: %s" % (tn, els, notk or "none"))
    P("   (Sora's / the forms' / the summons' motion triggers: their elements are the ones named in fxbake.py / chain comments;")
    P("    the baked *_motion.bin are not read here. The pack list above already holds them.)")

    # (a) DLL
    lits = dll_names(a.runtime)
    pats = [(re.compile(re.sub(r"%[ds]", r"[A-Za-z0-9_]+", re.escape(l).replace("\\%", "%")) if False else
             re.sub(r"%[ds]", "[A-Za-z0-9_]+", l)), l) for l in lits if "%" in l]
    packnames = []
    for pk, s, t in src:
        if pk == "prize_*":
            packnames += ["prize_hp", "prize_mp", "prize_drive", "prize_munny"]
        else:
            packnames.append(pk)
    unref = []
    kinds = collections.Counter()
    for pk in packnames:
        r = dll_ref(pk, lits, pats)
        kinds[(r or "none").split(" ")[0]] += 1
        if not r:
            unref.append(pk)
    P("   hit kinds (exact literal / only a printf pattern like '%s_fx%d', which is loose / none):", dict(kinds))
    fam = ("cl_fx_", "clf_fx_", "st_fx_", "pp_fx_", "gn_fx_")
    un2 = [u for u in unref if not u.startswith(fam)]
    P("== (a) DLL (%s): %d literals; pack names with a literal or a printf pattern hit: %d of %d" % (
        os.path.normpath(a.runtime), len(lits), len(packnames) - len(unref), len(packnames)))
    P("   summon families (cl_fx_ clf_fx_ st_fx_ pp_fx_ gn_fx_) come from generated cue headers (sora_summon_fx*.h, not in git): %d names" % (len(unref) - len(un2)))
    P("   other pack names with NO literal / pattern hit (%d, GUESS: reached through a table / header / computed name; kept anyway):" % len(un2))
    P("   ", ", ".join(un2[:80]) + (" ..." if len(un2) > 80 else ""))
    comp = sorted(l for l in lits if "%" in l and re.search(r"(fx|hit|trail|enter|burst|bolt|ring|shard|flower|sparkle|start|sphere|end|barrier|flash)", l))
    P("   computed-name patterns found (names built at run time; every possible result is inside fx.bin, so no extra recording):")
    P("   ", comp)
    P("   Reach: the DLL asks fx.bin by NAME (sora_fx_id) and the net code sends fx.bin INDEXES. Nothing indexes kh2_fx.bin")
    P("   directly, so no computed index can reach an unrecorded effect: fx.bin is fixed at build time. The only limit on the cut")
    P("   is layer 1 (everything in fx.bin, safe). Dropping fx.bin entries nobody plays (layer 2) shifts indexes: not advised.")

    if os.path.exists(a.live):
        live = {l.split()[1] for l in open(a.live)}
        P("== check vs the live fx.bin (%d names): packed by the tools but not in live: %s ; in live but not from the tools: %s" % (
            len(live), sorted(set(packnames) - live)[:10] or "none", sorted(live - set(packnames))[:10] or "none"))

    # numbers
    total = len(meta)
    allfiles = collections.Counter(split_name(k)[0] for k in meta)
    kept_rec = set()
    for pre, ns in elements.items():
        for n in ns:
            kept_rec.add("%s.%d" % (pre, n))
    for k in keep:
        kept_rec.add(k)
    # with --pre style recording the tail variants are cut from the base element's run: count tails separately
    used_el = {k for k in kept_rec if "~" not in k}
    used_tail = {k for k in kept_rec if "~" in k}
    base_for_tail = {k.split("~")[0] for k in used_tail}
    P("== numbers")
    n_el_all = sum(1 for k in meta if "~" not in k)
    n_tail_all = total - n_el_all
    P("   recordings in kh2_fx.json: %d  (= %d elements + %d loop-end tails)" % (total, n_el_all, n_tail_all))
    P("   used: %d elements + %d tails = %d of %d (%.1f %%); bases of the tails: %d (already counted as elements: %d)" % (
        len(used_el), len(used_tail), len(kept_rec), total, 100.0 * len(kept_rec) / total, len(base_for_tail), len(base_for_tail & used_el)))
    P("   files with something used: %d of %d" % (len({split_name(k)[0] for k in kept_rec}), len(allfiles)))
    # sizes
    def csz(k):
        return idx[k][1] if k in idx else 0
    tot_eff = sum(csz(k) for k in meta)
    kept_eff = sum(csz(k) for k in kept_rec)
    P("   kh2_fx.bin %d B: effect frames %d B (%.1f %%), index/textures/other %d B" % (binsize, tot_eff, 100.0 * tot_eff / binsize, binsize - tot_eff))
    P("   kept effect frames %d B = %.1f %% of the frames. Textures: the used effects use only some of the 382 textures (GUESS: count them)" % (
        kept_eff, 100.0 * kept_eff / tot_eff))
    P("   estimated kh2_fx.bin with only the kept recordings: ~%.0f MB + textures (kept textures only, GUESS <= %.0f MB) of today's %.0f MB" % (
        kept_eff / 1e6, (binsize - tot_eff) / 1e6, binsize / 1e6))
    # per-file table + time
    times = {}
    for l in open(a.times):
        m = re.match(r"(\S+) (\d+) s\) (\d+)", l)
        if m:
            times[m.group(1)] = (int(m.group(2)), int(m.group(3)))
    P("== per file: kept elements / tails, total recordings, time of the full file (s161times: recordings N, seconds M)")
    P("   %-12s %5s %5s %6s | %8s %8s | %6s %6s | pre(run, not stored)" % ("file", "elem", "tail", "recs", "M s", "N", "est opt", "est pes"))
    sum_m = sum_opt = sum_pes = 0.0
    fx_frames = lambda k: meta[k]["frames"]
    per = {}
    for pre in sorted(allfiles):
        ks = [k for k in kept_rec if split_name(k)[0] == pre]
        if not ks:
            nrec, m_ = times.get(pre, (allfiles[pre], 0))
            sum_m += m_
            continue
        recs = [k for k in meta if split_name(k)[0] == pre]
        nrec, m_ = times.get(pre, (len(recs), 0))
        filebytes = sum(csz(k) for k in recs) or 1
        share = sum(csz(k) for k in ks) / filebytes
        ne = len({split_name(k)[1] for k in ks if "~" not in k})
        nt = sum(1 for k in ks if "~" in k)
        maxid = max(split_name(k)[1] for k in ks)
        keptids = {split_name(k)[1] for k in ks}
        preids = sorted({split_name(k)[1] for k in recs if "~" not in k and split_name(k)[1] < maxid} - keptids)
        pre_frames = sum(fx_frames("%s.%d" % (pre, i)) for i in preids)
        tot_frames = sum(fx_frames(k) for k in recs if "~" not in k) or 1
        fixed = 5.0
        opt = fixed + m_ * share
        pes = opt + 0.3 * m_ * (pre_frames / tot_frames)   # GUESS: 30 % of a file's time is emulation, paid again for the pre elements
        sum_m += m_
        sum_opt += opt
        sum_pes += pes
        per[pre] = dict(elements=sorted(keptids), tails={k: sorted(v) for k, v in tails.items() if k.startswith(pre + ".")}, pre=preids)
        P("   %-12s %5d %5d %6d | %8d %8d | %6.0f %6.0f | %s" % (pre, ne, nt, len(recs), m_, nrec, opt, pes, preids[:12] if len(preids) < 13 else "%d ids" % len(preids)))
    unused = sorted(f for f in allfiles if f not in per)
    P("   files with NOTHING used (not recorded at all): %d: %s (time %d s)" % (len(unused), ", ".join(unused), sum(times.get(f, (0, 0))[1] for f in unused)))
    P("   total of s161times: %d s = %.0f min of recorder time (summed over files; wall = this / ~--jobs overlap)" % (sum_m, sum_m / 60))
    P("   GUESS estimate with the cut: optimistic %.0f s (%.1f %%), pessimistic %.0f s (%.1f %%) -> saved %.0f..%.0f min of recorder time" % (
        sum_opt, 100 * sum_opt / sum_m, sum_pes, 100 * sum_pes / sum_m, (sum_m - sum_pes) / 60, (sum_m - sum_opt) / 60))
    P("   (optimistic = time follows the compressed bytes kept + 5 s start; pessimistic = + 30 percent of the file time for running the skipped earlier elements)")

    # (idea 2) loops
    P("== loops (idea 2): a looping element is recorded for 600 frames; json 'period' = [period, warmup] (loop_of: smallest period < 200)")
    loops = {k: v for k, v in meta.items() if v.get("loops") and "~" not in k}
    withp = {k: v for k, v in loops.items() if v.get("period")}
    P("   looping elements: %d ; with a found period: %d ; tail variants (name~at): %d" % (len(loops), len(withp), n_tail_all))
    sf = sb = 0
    for k, v in withp.items():
        p, w = v["period"]
        keepf = min(v["frames"], p + w)
        sf += v["frames"] - keepf
        sb += csz(k) * (1.0 - keepf / v["frames"])
    allf = sum(v["frames"] for v in meta.values())
    P("   frames of all recordings %d ; storing period+warmup for those %d elements saves %d frames (%.1f %%), ~%.0f MB of effect frames (GUESS: bytes ~ frames)" % (
        allf, len(withp), sf, 100.0 * sf / allf, sb / 1e6))
    usedl = [k for k in withp if k in kept_rec]
    sf2 = sum(withp[k]["frames"] - min(withp[k]["frames"], sum(withp[k]["period"])) for k in usedl)
    sb2 = sum(csz(k) * (1.0 - min(1.0, sum(withp[k]["period"]) / withp[k]["frames"])) for k in usedl)
    P("   among the USED recordings: %d loop with a period; saves %d frames, ~%.0f MB (GUESS). Others used loops without a period: %s" % (
        len(usedl), sf2, sb2 / 1e6, sorted(k for k in loops if k in kept_rec and k not in withp)[:20]))
    P("   Most of the 120-tail weight (%d tails recorded for the 93 'tails' elements) disappears with the cut, because only ~%d tails are packed." % (n_tail_all, len(used_tail)))

    if a.keep:
        json.dump(dict(elements={k: sorted(v) for k, v in elements.items()}, tails={k: sorted(v) for k, v in tails.items()},
                       per_file=per, sources=[list(s) for s in src]), open(a.keep, "w"), indent=1)
        P("keep list ->", a.keep)
    if a.report:
        open(a.report, "w", encoding="utf-8").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()

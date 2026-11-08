"""Compare two fxv1 element dumps (ours vs oracle). Own code.
usage: python -E -P fxv_cmp.py <ours.json> <oracle.json> [--tol 1e-3] [--frames N] [--detail K]"""
import sys, json, math, os, struct

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fxvbin import load_any  # noqa: E402  (s164: ours may be a binary .fxv)

F32 = struct.Struct("<f")
BITS = [0, 0]           # s162: p/o/uv components bit-identical after float32 rounding (the bin stores float32), total


def f32(x):
    return F32.unpack(F32.pack(x))[0]


def vd32(a, b):
    """s162: worst |f32(ours) - f32(oracle)| beyond one float32 step at that size (vertices near 2.8e6 of an
    ill-conditioned solve have 0.25-unit float32 steps: a 0.09 float64 'diff' there is no diff in the bin)"""
    if len(a) != len(b):
        return float("inf")
    w = 0.0
    for x, y in zip(a, b):
        x, y = f32(x), f32(y)
        BITS[1] += 1
        if x == y:
            BITS[0] += 1
            continue
        d = abs(x - y)
        step = math.ldexp(1.0, math.frexp(max(abs(x), abs(y)))[1] - 24)
        w = max(w, 0.0 if d <= step else d)
    return w

EXACT = ["program", "flags", "blend", "depth", "sampler", "vertexCount"]
SCAL = ["alphaRef", "anchorAge"]
VECS = ["upright", "anchor", "anchorOff", "fog", "tfog", "fogColour"]


def vd(a, b):
    if len(a) != len(b):
        return float("inf")             # s158 review: different vector lengths = mismatch (zip truncated before)
    return max((abs(x - y) for x, y in zip(a, b)), default=0.0)


def cmp_batch(o, r):
    """returns (fails dict field->(ours,oracle,worst), worst dict field->(diff,vertex))"""
    fails, diffs = {}, {}
    for k in EXACT:
        if o.get(k) != r.get(k):
            fails[k] = (o.get(k), r.get(k))
    for k in SCAL:
        diffs[k] = (abs(o[k] - r[k]), None)
    for k in VECS:
        diffs[k] = (vd(o[k], r[k]), None)
    n = min(len(o["p"]), len(r["p"]))
    for b, who in ((o, "ours"), (r, "oracle")):     # s158 review: every per-vertex list must hold vertexCount entries
        if any(len(b[k]) != b["vertexCount"] for k in ("p", "o", "uv", "c", "age")):
            fails["lengths-" + who] = (b["vertexCount"], [len(b[k]) for k in ("p", "o", "uv", "c", "age")])
    for k, key in (("p", "p"), ("o", "o"), ("uv", "uv")):
        w, wi = 0.0, None
        for i in range(n):
            d = vd32(o[key][i], r[key][i])
            if d > w: w, wi = d, i
        diffs[k] = (w, wi)
    w, wi = 0.0, None
    for i in range(n):
        d = vd(o["c"][i], r["c"][i]) / 32767.5
        if d > w: w, wi = d, i
    diffs["c"] = (w, wi)
    bad = [i for i in range(n) if o["age"][i] != r["age"][i]]
    diffs["age"] = (len(bad), bad[0] if bad else None)
    return fails, diffs, n


def main(a):
    if len(a) < 2:
        print(__doc__); return 2
    tol = float(a[a.index("--tol") + 1]) if "--tol" in a else 1e-3
    maxf = int(a[a.index("--frames") + 1]) if "--frames" in a else None
    detail = int(a[a.index("--detail") + 1]) if "--detail" in a else 10
    O, R = load_any(a[0]), load_any(a[1])
    notes = []
    if O["frames"] != R["frames"]:
        notes.append("FRAME COUNT differs: ours %d oracle %d (comparing common prefix)" % (O["frames"], R["frames"]))
    nf = min(len(O["perFrame"]), len(R["perFrame"]))
    if maxf is not None: nf = min(nf, maxf)
    texmap, texbad = {}, set()
    worst = {}  # field -> (diff, frame, batch, vertex)
    fok = bok = btot = vtot = 0
    fbad = []
    first_mis = None
    for fi in range(nf):
        of, rf = O["perFrame"][fi]["batches"], R["perFrame"][fi]["batches"]
        msgs = []
        if len(of) != len(rf):
            msgs.append("batch count ours %d oracle %d" % (len(of), len(rf)))
        fine = not msgs
        for bi in range(min(len(of), len(rf))):
            o, r = of[bi], rf[bi]
            btot += 1
            fails, diffs, n = cmp_batch(o, r)
            vtot += n
            ok = not fails
            t = texmap.setdefault(o["tex"], r["tex"])
            special = {0xFFFE, 0xFFFF}
            if (o["tex"] in special or r["tex"] in special) and o["tex"] != r["tex"]:
                fails["tex-special"] = (o["tex"], r["tex"]); ok = False
            if t != r["tex"]:
                fails["tex-map"] = (o["tex"], "%d (earlier->%d)" % (r["tex"], t)); ok = False
                texbad.add(o["tex"])
            for k, (d, vi) in diffs.items():
                if d > worst.get(k, (-1,))[0]: worst[k] = (d, fi, bi, vi)
                if (d > 0 if k in ("age", "c") else d > tol):    # s158 review: colour (u16) and age exact
                    ok = False
                    fails[k] = (None, None, d, vi)
            if ok: bok += 1
            else:
                fine = False
                for k, v in fails.items():
                    msgs.append("b%d %s: %s" % (bi, k, v if len(v) == 2 else "worst %.6g at vertex %s" % (v[2], v[3])))
                    if len(v) == 2 or True: pass
                if first_mis is None: first_mis = (fi, bi, o, r)
        if fine: fok += 1
        else: fbad.append((fi, msgs))
    print("%s vs %s: frames ok %d/%d, batches ok %d/%d, vertices compared %d" % (O["name"], R["name"], fok, nf, bok, btot, vtot))
    for n in notes: print("NOTE", n)
    print("texture map ours->oracle:", dict(sorted(texmap.items())), ("INCONSISTENT for ours tex %s" % sorted(texbad)) if texbad else "(consistent)")
    n0 = len(notes)
    if len(set(texmap.values())) != len(texmap):
        notes.append("texture map not 1:1 (two ours ids -> same oracle id)")
    # s158 review a1: element-level fields. Our solver does not fill bone / paxFlags yet (writer stage, slice 4):
    # reported as ELEMENT lines; --strict-element makes them fail the result.
    elem_bad = ["%s ours %s oracle %s" % (k, O.get(k), R.get(k)) for k in ("flags", "bone", "paxFlags") if O.get(k) != R.get(k)]
    for e in elem_bad: print("ELEMENT", e)
    if elem_bad and "--strict-element" in a: notes.append("element fields differ")
    for n_ in notes[n0:]: print("NOTE", n_)
    print("worst per field (diff @frame/batch/vertex):")
    for k in ["p", "o", "uv", "c", "age"] + SCAL + VECS:
        if k in worst:
            d, fi, bi, vi = worst[k]
            print("  %-9s %.6g @ f%d b%d v%s%s" % (k, d, fi, bi, vi, " (count)" if k == "age" else ""))
    for fi, msgs in fbad[:detail]:
        print("frame %d MISMATCH: %s" % (fi, "; ".join(msgs[:8])))
    if len(fbad) > detail: print("... %d more mismatching frames" % (len(fbad) - detail))
    if first_mis:
        fi, bi, o, r = first_mis
        print("first mismatching batch f%d b%d, first 3 vertices (ours | oracle):" % (fi, bi))
        for i in range(min(3, len(o["p"]), len(r["p"]))):
            for tag, b in (("ours  ", o), ("oracle", r)):
                print("  v%d %s p=%s o=%s uv=%s age=%s c=%s" % (i, tag, b["p"][i], b["o"][i], b["uv"][i], b["age"][i], b["c"][i]))
    print("float32 bits: %d of %d p/o/uv components identical (%.4f%%)" % (BITS[0], BITS[1], 100.0 * BITS[0] / max(1, BITS[1])))
    allok = not fbad and not notes
    print("RESULT:", "ALL MATCH (tol %g)" % tol if allok else "MISMATCH")
    return 0 if allok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

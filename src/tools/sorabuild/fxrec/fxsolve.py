"""fxsolve.py (s158) - fxrec slice 3: turn the 5 camera runs of one element into world-space vertices (fxv1 json).
Own code. Model (S155_HELPER_FXHARNESS.md "camera trick", behaviour only):
  KH2 folds the camera into its matrices, so each run gives camera-space points q_X. Runs A (identity view), B, C (turned
  views R_B, R_C, row vectors) solve per vertex  q_X = c R_X + o W_X  for the world point c and the offset o:
    W_X = I            plain billboards / camera-aligned particles (o = camera-space offset)
    W_X = upright      KH2's upright basis of run X (effect camera block +0x180, 3x3 of float4 rows)
    W_X = R_X J        ribbons (J: (x, y, z) -> (-y, x, 0))
  Residual > TOL in all three = draw left out (counted). Runs X / Z move the owner 1 unit per frame along x / z:
  a vertex moved by k units uses the effect matrix of frame k, age = frame - k.
Positions: program 0x17 = (x, y, z, 1) x CB3[24..27]; 0x2B = the float4 KH2 wrote; then x CB4 x CB2[5..8], / w.
Colour: 0x17 sat(rgba/255 x CB3[3] + CB3[1]); 0x2B float4 at +32. UV x 1/4096. Stored colour u16 = round(c x 32767.5).
  python -E -P fxsolve.py <dir> <tag> --out <json | fxv>  (reads <dir>/<tag>_{A,B,C,X,Z}.fxd from runcams.sh)
  s164: --out *.fxv = binary fxv2 (fxvbin.py: exact float bits incl. the NaN sign, written frame by frame); else fxv1 json.
  s168: the runs are STREAMED (FxdRun: one frame of each run in memory, ~0.1 GB for hb040.6 instead of 8.5-10 GB). A big recording
  (run A .fxd > 1 GB) is solved by 4 processes (--part i/N each, then joined; byte-identical); env FXSOLVE_PARTS=n sets n (1 = off).
"""
import ctypes
import hashlib
import json
import math
import shutil
import struct
import sys

import os

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # -P: fxvbin.py sits next to this file
import fxvbin  # noqa: E402

# s162 bit-exact = PHH: every product / pinv below is plain IEEE float64 in a fixed order (no BLAS / LAPACK, whose
# kernels differ per CPU), and the CRT's sin / cos run the SSE2 path (ucrtbase picks an FMA3 path on FMA3 CPUs that
# rounds differently in the last bit; the camera matrices come from math.sin / math.cos).
if sys.platform == "win32":
    try:
        ctypes.CDLL("ucrtbase")._set_FMA3_enable(0)
    except (OSError, AttributeError):
        pass

TOL = 1e-2
STEP = 1.0
CAMS = {"B": (37.0, 23.0), "C": (-61.0, -14.0)}
SAMPLER_ADDR = (0, 1, 1, 0)
J = np.array([[0.0, 1.0, 0.0], [-1.0, 0.0, 0.0], [0.0, 0.0, 0.0]])
STATE0 = 0x60


def mm(a, b):
    """a @ b summed k = 0, 1, ... in order (separate IEEE mul / add: same bits on every CPU, no BLAS)"""
    a, b = np.asarray(a, np.float64), np.asarray(b, np.float64)
    out = a[..., :, 0:1] * b[0]
    for j in range(1, b.shape[0]):
        out = out + a[..., :, j:j + 1] * b[j]
    return out


EPS = 2.220446049250313e-16


def jacobi_pinv(rows, rcond):
    """Moore-Penrose inverse of an M x N list matrix (M >= N): one-sided Jacobi SVD (Hestenes), sweeps over the column
    pairs (p < q) in order until no pair rotates; singular values <= rcond x the largest dropped (numpy's rule).
    Returns N x M. Plain Python floats: the same bits on every machine.
    The textbook algorithm (Hestenes 1958; Demmel-Veselic 1992), written for the same operation order as PHH's fpdet.pinv
    because the bake must match its bits: any other order = other last bits (s162, user OK)."""
    m, n = len(rows), len(rows[0])
    u = [list(r) for r in rows]
    v = [[1.0 if i == j else 0.0 for j in range(n)] for i in range(n)]
    for _ in range(100):
        turned = False
        for p in range(n - 1):
            for q in range(p + 1, n):
                aa = bb = ab = 0.0
                for i in range(m):
                    x, y = u[i][p], u[i][q]
                    aa += x * x
                    bb += y * y
                    ab += x * y
                if ab == 0.0 or abs(ab) <= EPS * math.sqrt(aa * bb):
                    continue
                turned = True
                z = (bb - aa) / (2.0 * ab)
                t = 0.5 / z if abs(z) > 1e150 else (1.0 if z >= 0.0 else -1.0) / (abs(z) + math.sqrt(1.0 + z * z))
                c = 1.0 / math.sqrt(1.0 + t * t)
                s = c * t
                for i in range(m):
                    x, y = u[i][p], u[i][q]
                    u[i][p] = c * x - s * y
                    u[i][q] = s * x + c * y
                for i in range(n):
                    x, y = v[i][p], v[i][q]
                    v[i][p] = c * x - s * y
                    v[i][q] = s * x + c * y
        if not turned:
            break
    sig = []
    for k in range(n):
        acc = 0.0
        for i in range(m):
            acc += u[i][k] * u[i][k]
        sig.append(math.sqrt(acc))
    cut = rcond * max(sig) if sig else 0.0
    keep = [k for k in range(n) if sig[k] > cut]
    out = [[0.0] * m for _ in range(n)]
    for j in range(n):
        for i in range(m):
            acc = 0.0
            for k in keep:
                acc += v[j][k] * (u[i][k] / sig[k]) / sig[k]
            out[j][i] = acc
    return out


_PINV = {}


def pinv(a, rcond=1e-15):
    """numpy.linalg.pinv's answer, machine-independent (see jacobi_pinv); M < N via pinv(A^T)^T"""
    a = np.ascontiguousarray(a, np.float64)
    key = a.shape + (a.tobytes(),)
    if len(_PINV) > 50000:      # s168: bounded (same answers, only recomputed)
        _PINV.clear()
    if key not in _PINV:
        rows = a.tolist()
        if a.shape[0] >= a.shape[1]:
            _PINV[key] = np.array(jacobi_pinv(rows, rcond), np.float64)
        else:
            _PINV[key] = np.array(jacobi_pinv([list(c) for c in zip(*rows)], rcond), np.float64).T.copy()
    return _PINV[key].copy()


def view(yaw, pitch):
    """the view fxrec_s2.c writes (double math, stored as float32)"""
    y, p = math.radians(yaw), math.radians(pitch)
    cy, sy, cp, sp = math.cos(y), math.sin(y), math.cos(p), math.sin(p)
    ry = np.array([[cy, 0, -sy], [0, 1, 0], [sy, 0, cy]])
    rx = np.array([[1, 0, 0], [0, cp, sp], [0, -sp, cp]])
    return mm(ry, rx)          # s159: kept in float64 (the solve's matrix; PHH's split of rank-5 upright solves depends on it)


ROT = {"A": np.eye(3), **{k: view(*v) for k, v in CAMS.items()}}


DRW_FIXED = (("state", 0x250), ("cb2", 0x90), ("cb3", 0x200), ("cb4", 0x40), ("cb6", 0x50), ("cam", 0x280))
DRW_TAIL = sum(n for _, n in DRW_FIXED)


class FxdRun:
    """s168: one camera run, streamed. __init__ walks the tags once with seek (no draw bytes kept) and runs every check the
    old whole-file read_fxd made (torn / failed run): FRM numbers and draw counts, END frame count, no draws after the last
    FRM. frames(a, b) then reads frames a..b-1 from the file one at a time (a generator of draw lists)."""

    def __init__(self, path):
        self.path = path
        size = os.path.getsize(path)
        offs, cur, ended, nframes = [4], 0, 0, None
        with open(path, "rb", buffering=1 << 20) as f:
            assert f.read(4) == b"FXD1", path
            p = 4
            while p < size:
                tag = f.read(4); p += 4
                if tag == b"SRT\0":
                    f.seek(16, 1); p += 16
                elif tag == b"DRW\0":
                    vb = struct.unpack("<I", f.read(32)[28:32])[0]
                    p += 32 + vb + DRW_TAIL
                    assert p <= size, "%s: torn run (draw past the end of the file)" % path
                    f.seek(p)
                    cur += 1
                elif tag == b"FRM\0":
                    fr, n = struct.unpack("<II", f.read(8)); p += 8
                    assert fr == len(offs) - 1 and n == cur, (path, fr, n, cur)
                    offs.append(p); cur = 0
                elif tag == b"END\0":
                    nframes, ended = struct.unpack("<II", f.read(8)); p += 8
                else:
                    raise ValueError("bad tag %r at %d in %s" % (tag, p - 4, path))
        self.nframes_found = len(offs) - 1
        assert nframes and nframes == self.nframes_found and not cur, "%s: torn / failed run (END %d, frames %d)" % (path, nframes or 0, self.nframes_found)
        self.offs, self.ended, self.nframes = offs, ended, nframes

    def frames(self, a=0, b=None):
        b = self.nframes if b is None else b
        with open(self.path, "rb", buffering=1 << 20) as f:
            f.seek(self.offs[a])
            srt = None
            for fi in range(a, b):
                cur = []
                while True:
                    tag = f.read(4)
                    if tag == b"SRT\0":                         # s159: the next DRW is a depth-sorted draw (kind 1 key, 2 anchor)
                        kind, key, ax, ay = struct.unpack("<Iiff", f.read(16))
                        srt = (kind, key, float(ax), float(ay))
                    elif tag == b"DRW\0":
                        fr, cnt, prim, fl, stride, prog, tex, vb = struct.unpack("<6IiI", f.read(32))
                        d = dict(frame=fr, count=cnt, prim=prim, flags=fl, stride=stride, program=prog, tex=tex, v=f.read(vb), sort=srt)
                        srt = None
                        for k, n in DRW_FIXED:
                            d[k] = f.read(n)
                        cur.append(d)
                    elif tag == b"FRM\0":
                        fr, n = struct.unpack("<II", f.read(8))
                        assert fr == fi and n == len(cur), (self.path, fr, n, len(cur))
                        break
                    else:
                        raise ValueError("bad tag %r in %s" % (tag, self.path))
                yield cur


def used_bytes(d):
    """s162 review: the vertex bytes KH2 reads. The last u32 of each vertex (GS-style flag word, in no program's input
    layout) is unwritten stack data, read only by a strip (prim 6) drawn with flag bit 1 from its third vertex on:
    every other copy is taken as 0 (else a loop's frames never repeat byte for byte = wrong period)."""
    n, s, v = d["count"], d["stride"], d["v"]
    if n < 1 or s < 4:
        return v
    b = bytearray(v)
    for i in range(min(2, n) if d["prim"] == 6 and d["flags"] & 2 else n):
        b[i * s + s - 4:i * s + s] = bytes(4)
    return bytes(b)


def f32(buf, off, n):
    return np.frombuffer(buf, np.float32, n, off).astype(np.float64)


def decode(d):
    """positions (N x 3, after / w), colours (N x 4), uvs (N x 2)"""
    n, s = d["count"], d["stride"]
    a = np.frombuffer(d["v"], np.uint8).reshape(n, s)
    if d["program"] == 0x17:
        xyz = a[:, 0:12].copy().view(np.float32).astype(np.float64)
        rgba = a[:, 12:16].astype(np.float64) / 255.0
        uv = a[:, 16:24].copy().view(np.float32).astype(np.float64)
        m = f32(d["cb3"], 24 * 16, 16).reshape(4, 4)
        p = mm(np.hstack([xyz, np.ones((n, 1))]), m)
        col = np.clip(rgba * f32(d["cb3"], 3 * 16, 4) + f32(d["cb3"], 1 * 16, 4), 0.0, 1.0)
    elif d["program"] == 0x2B:
        f = a[:, 0:56].copy().view(np.float32).astype(np.float64)
        p, col, uv = f[:, 0:4], f[:, 8:12], f[:, 12:14]
    else:
        raise ValueError(d["program"])
    p = mm(mm(p, f32(d["cb4"], 0, 16).reshape(4, 4)), f32(d["cb2"], 5 * 16, 16).reshape(4, 4))
    w = np.where(np.abs(p[:, 3:4]) > 1e-12, p[:, 3:4], 1.0)
    return p[:, :3] / w, col, uv / 4096.0


def mean_rows(a):
    """column means: correctly rounded sum (math.fsum, order free) / N"""
    return [math.fsum(a[:, j].tolist()) / len(a) for j in range(a.shape[1])]


def anchor_point(d, sort=None):
    """the sorted sprite's local anchor (x, y, 0, 1) through the draw's CB3[24..27] (0x4AF4C0 role), / w.
    s162 review: the anchor pair comes from run A's sort record for all three runs (= PHH), each run's own CB3."""
    sort = sort or d["sort"]
    q = mm(np.array([[sort[2], sort[3], 0.0, 1.0]]), f32(d["cb3"], 24 * 16, 16).reshape(4, 4))[0]
    return q[:3] / (q[3] if abs(q[3]) > 1e-12 else 1.0)


def basis(d, off):
    return f32(d["cam"], off, 12).reshape(3, 4)[:, :3]


def solve_plain(q):
    """camera-aligned offsets: q_X = c R_X + o, c = q_A - o -> o from q_B - q_A R_B = o (I - R_B) (same for C)"""
    M = np.vstack([(np.eye(3) - ROT[k]).T for k in "BC"])                 # 6 x 3
    rhs = np.hstack([q[k] - mm(q["A"], ROT[k]) for k in "BC"])            # N x 6
    o = mm(rhs, pinv(M).T) if len(rhs) else np.zeros((0, 3))
    res = float(np.abs(mm(o, M.T) - rhs).max()) if len(o) else 0.0
    o[np.abs(o) < 1e-5] = 0.0
    return q["A"] - o, o, res


def solve(q, W):
    """q: {A,B,C: N x 3}, W: {A,B,C: 3 x 3} -> c, o, worst residual of q_X = c R_X + o W_X"""
    M = np.vstack([np.hstack([ROT[k].T, W[k].T]) for k in "ABC"])          # 9 x 6
    rhs = np.hstack([q[k] for k in "ABC"])                                  # N x 9
    sol = mm(rhs, pinv(M).T) if len(rhs) else np.zeros((0, 6))
    res = float(np.abs(mm(sol, M.T) - rhs).max()) if len(sol) else 0.0
    c, o = sol[:, :3].copy(), sol[:, 3:].copy()
    o[np.abs(o) < 1e-5] = 0.0
    return c, o, res


def triangles(prim, n):
    """KH2's primitive rules (0x142F40) as triangle-list indices; strip winding (odd triangles swap the first two)"""
    if prim == 5:
        return [(i, i + 1, i + 2) for i in range(0, n - 2, 3)]
    if prim == 7 and n > 3:
        return [(0, i + 1, i + 2) for i in range(n - 2)]
    if prim == 8:                                   # quad a b c d drawn as the strip a b d c
        out = []
        for q in range(0, n - 3, 4):
            out += [(q, q + 1, q + 3), (q + 3, q + 1, q + 2)]
        return out
    if prim in (6, 7):                              # strip (7 with three vertices = one triangle)
        return [((k + 1, k, k + 2) if k & 1 else (k, k + 1, k + 2)) for k in range(n - 2)]
    raise ValueError("unknown primitive %d" % prim)


def state(d):
    st, cb3, cb6 = d["state"], d["cb3"], d["cb6"]
    u = lambda off: struct.unpack_from("<I", st, off - STATE0)[0] & 0xFF
    blend = [u(0x180), u(0x184), u(0x188), u(0x18C)]
    depth = [u(0x84), u(0x260), u(0x264), u(0x270)]
    c6 = np.frombuffer(cb6, np.float32)
    sampler = [struct.unpack_from("<I", cb6, 0x24)[0] & 0xFF, SAMPLER_ADDR[struct.unpack_from("<I", st, 0x1F0 - STATE0)[0] & 3],
               SAMPLER_ADDR[struct.unpack_from("<I", st, 0x204 - STATE0)[0] & 3], 1]
    fog = [float(x) for x in np.frombuffer(cb3, np.float32, 4, 4 * 16)]
    tfog = [float(x) for x in np.frombuffer(cb3, np.float32, 4, 2 * 16)] if d["program"] == 0x17 else [0.0, 1.0, 1.0, 1.0]
    return dict(blend=blend, depth=depth, sampler=sampler, alphaRef=float(c6[8]), fog=fog, tfog=tfog,
                fogColour=[float(x) for x in c6[12:15]])


def lineup(*frs):
    if any(f is None for f in frs) or len({len(f) for f in frs}) != 1:
        return False
    key = lambda d: (d["program"], d["prim"], d["flags"], d["count"], d["stride"])
    return all(len({key(x) for x in ds}) == 1 for ds in zip(*frs))


BIG_FXD = 1 << 30      # s168: an A run .fxd bigger than this is solved in PARTS processes (env FXSOLVE_PARTS overrides; 1 = never)
PARTS = 4


def part_ranges(offs, n):
    """n contiguous frame ranges of about the same run-A bytes (offs = FxdRun.offs); every process computes the same ones"""
    nf = len(offs) - 1
    cuts, total = [0], offs[-1] - offs[0]
    for i in range(1, n):
        want = offs[0] + total * i // n
        k = cuts[-1]
        while k < nf and offs[k] < want:
            k += 1
        cuts.append(k)
    cuts.append(nf)
    return list(zip(cuts[:-1], cuts[1:]))


def main(a):
    if len(a) < 2:
        print(__doc__); return 2
    dr, tag = a[0], a[1]
    out = a[a.index("--out") + 1] if "--out" in a else None
    part = None
    if "--part" in a:                                  # s168: --part i/N = solve frames range i of N into <out>.part<i>
        i, n = a[a.index("--part") + 1].split("/")
        part = (int(i), int(n))
    runs = {k: FxdRun("%s/%s_%s.fxd" % (dr, tag, k)) for k in "ABCXZ"}
    nf, ended = runs["A"].nframes, runs["A"].ended
    st = {}
    isfxv = bool(out and out.endswith(".fxv"))
    if part and not isfxv:
        raise SystemExit("--part needs --out *.fxv")
    parts = part[1] if part else int(os.environ.get("FXSOLVE_PARTS", "0")) or (PARTS if os.path.getsize("%s/%s_A.fxd" % (dr, tag)) > BIG_FXD else 1)
    if not part and not os.environ.get("FXSOLVE_PARTS") and parts > 1:   # (s168 review) runfile already runs SOLVEP solves at once
        parts = min(parts, max(2, (os.cpu_count() or 4) // max(1, int(os.environ.get("SOLVEP", "1") or 1))))
    if isfxv and not part and parts > 1 and nf >= parts:
        return _solve_parts(dr, tag, out, parts, runs)
    lo, hi = part_ranges(runs["A"].offs, part[1])[part[0]] if part else (0, nf)
    flags = 0 if ended else 1
    if part:
        fxv = fxvbin.Writer("%s.part%d" % (out, part[0]), hi - lo, flags, header=False)
    else:
        fxv = fxvbin.Writer(out, nf, flags) if isfxv else None   # s164: binary
    try:
        moved, anyA, per = _solve(runs, lo, hi, st, fxv)
        if part:
            fxv.finish_part()
            with open("%s.part%d.st" % (out, part[0]), "w") as f:
                json.dump(dict(st=st, moved=moved, anyA=int(anyA)), f)
            return 0
        return _finish(tag, out, st, fxv, nf, ended, moved, anyA, per)
    except BaseException:
        if fxv:
            fxv.abort()
        raise


def _solve_parts(dr, tag, out, n, runs):
    """s168: N processes solve one frame range each (frames are independent; stats are plain sums), then the parts are
    joined: header + parts in order + the END record = the same bytes as a one-process solve."""
    import subprocess
    nf, ended = runs["A"].nframes, runs["A"].ended
    names = ["%s.part%d" % (out, i) for i in range(n)]
    for p in names:
        for x in (p, p + ".tmp", p + ".st"):
            if os.path.exists(x):
                os.remove(x)
    ps = [subprocess.Popen([sys.executable, "-E", "-P", os.path.abspath(__file__), dr, tag, "--out", out, "--part", "%d/%d" % (i, n)])
          for i in range(n)]
    rc = [p.wait() for p in ps]
    try:
        if any(rc):
            raise SystemExit("fxsolve: part process failed (exit codes %s)" % rc)
        st, moved, anyA = {}, False, 0
        for p in names:
            r = json.load(open(p + ".st"))
            for k, v in r["st"].items():
                st[k] = st.get(k, 0) + v
            moved, anyA = moved or r["moved"], anyA + r["anyA"]
        tmp = out + ".tmp"
        with open(tmp, "wb") as o:
            o.write(fxvbin.MAGIC + struct.pack("<II", nf, 0 if ended else 1))
            for p in names:
                with open(p, "rb") as f:
                    shutil.copyfileobj(f, o, 1 << 24)
            o.write(fxvbin.end_record(dict(_head(tag, nf, ended), schema="fxv2", stats=st)))
        os.replace(tmp, out)
    finally:
        for p in names:
            for x in (p, p + ".tmp", p + ".st"):
                if os.path.exists(x):
                    os.remove(x)
    return _report(tag, nf, ended, st, moved, anyA)


def _head(tag, nf, ended):
    return dict(schema="fxv1", name=tag, frames=nf, flags=0 if ended else 1, bone=0, paxFlags=0)


def _report(tag, nf, ended, st, moved, anyA):
    if not moved and anyA:
        print("note: no vertex moved in the X owner run (the element ignores the manager translation)")
    print("%s: %d frames (ended %d)" % (tag, nf, ended))
    for k, v in sorted(st.items()):
        print("  %s: %s" % (k, v))
    return 0


def _finish(tag, out, st, fxv, nf, ended, moved, anyA, per):
    head = _head(tag, nf, ended)
    if fxv:
        fxv.close(dict(head, schema="fxv2", stats=st))
    elif out:
        open(out, "w", newline="\n").write(json.dumps(dict(head, perFrame=per, stats=st)))
    return _report(tag, nf, ended, st, moved, anyA)


VCOLS = ("p", "o", "uv", "age", "c")


def _solve(runs, lo, hi, st, fxv):
    """frames lo..hi-1 of the 5 runs, streamed in lockstep (one frame of each in memory); returns `moved`"""
    bump = lambda k, n=1: st.__setitem__(k, st.get(k, 0) + n)
    per = []
    moved = anyA = False
    its = {k: runs[k].frames(lo, min(hi, runs[k].nframes)) if lo < runs[k].nframes else iter(()) for k in "ABCXZ"}
    for fi in range(lo, hi):
        fa = next(its["A"])
        g = {k: next(its[k], None) for k in "BCXZ"}
        anyA = anyA or bool(fa)
        if not moved and g["X"] is not None:
            moved = any(da["v"] != dx["v"] or da["cb3"] != dx["cb3"] or da["cb4"] != dx["cb4"] for da, dx in zip(fa, g["X"]))
        cam_ok, own_ok = lineup(fa, g["B"], g["C"]), lineup(fa, g["X"], g["Z"])
        if not cam_ok: bump("frames without a camera line-up")
        if not own_ok: bump("frames whose draws depend on the owner path (age 0)")
        batches, texuse = [], []
        lk = hashlib.blake2b(digest_size=16)          # s162: PHH loop_period key = every RAW draw of run A
        for d in fa:
            lk.update(struct.pack("<4i", d["program"], d["prim"], d["count"], d["tex"]))
            for f in ("v", "state", "cb3"):
                lk.update(struct.pack("<I", len(d[f])) + (used_bytes(d) if f == "v" else d[f]))
        for di, d in enumerate(fa):
            if d["program"] not in (0x17, 0x2B):
                bump("skipped draws (program %#x)" % d["program"]); continue
            pa, col, uv = decode(d)
            upright = [0.0] * 9
            if cam_ok:
                ds = {"A": d, "B": g["B"][di], "C": g["C"][di]}
                q = {k: decode(ds[k])[0] for k in "ABC"}
                c, o, res = solve_plain(q)
                kind = "plain"
                if res > TOL:
                    c2, o2, r2 = solve(q, {k: basis(ds[k], 0x180) for k in "ABC"})
                    if r2 <= TOL:
                        c, o, res, kind = c2, o2, r2, "upright"
                        upright = [float(x) for x in basis(d, 0).ravel()]
                    else:
                        c3, o3, r3 = solve(q, {k: (J if k == "A" else mm(ROT[k], J)) for k in "ABC"})   # A: J itself (= PHH)
                        if r3 <= TOL:
                            c, o, res, kind = c3, o3, r3, "ribbon"
                if res > TOL:
                    bump("draws off the camera model (left out)"); continue
                bump("%s draws" % kind)
            else:
                c, o, kind = pa, np.zeros_like(pa), "plain"
            if d["tex"] > 0:      # s162: PHH numbers a texture here (also for a draw that ends with 0 triangles)
                texuse.append(d["tex"])
            ages = np.zeros(len(pa), np.int64)
            if own_ok:   # PHH: ages whenever the owner runs line up (s160: no "moved" gate; that read raw bytes only)
                px, pz = decode(g["X"][di])[0], decode(g["Z"][di])[0]
                dx, dz = px - pa, pz - pa
                kx, kz = dx[:, 0] / STEP, dz[:, 2] / STEP
                k = np.round(kx)
                other = np.maximum(np.abs(dx[:, 1:]).max(1), np.abs(dz[:, :2]).max(1)) if len(pa) else np.zeros(0)
                ok = (np.abs(kx - k) < 1e-2) & (np.abs(kz - k) < 1e-2) & (other < TOL) & (k >= 0) & (k <= fi)
                ages = np.where(ok, fi - k, 0).astype(np.int64)
                bump("vertices off the owner model (age 0)", int((~ok).sum()))
            anchor, aoff = [0.0] * 3, [0.0] * 3
            if d["sort"]:
                if d["sort"][0] == 2 and d["program"] == 0x17:
                    if cam_ok:
                        ca, oa, _ = solve_plain({k: anchor_point(ds[k], d["sort"])[None] for k in "ABC"})
                        anchor, aoff = [float(x) for x in ca[0]], [float(x) for x in oa[0]]
                    else:
                        anchor = [float(x) for x in anchor_point(d)]
                else:
                    if len(c): anchor, aoff = mean_rows(c), mean_rows(o)
            has_o = bool(np.any(o))
            flags = (2 if d["sort"] else 0) | (1 if has_o else 0) | (4 if kind == "upright" and has_o else 0) | (8 if kind == "ribbon" and has_o else 0)
            idx = np.array(triangles(d["prim"], d["count"]), np.int64).reshape(-1)
            if not len(idx):
                continue
            c16 = np.clip(np.round(col * 32767.5), 0, 65535).astype(np.int64)
            b = dict(tex=d["tex"] if d["tex"] > 0 else (0xFFFE if d["tex"] == -2 else 0xFFFF), program=d["program"],
                     flags=flags, **state(d), anchor=anchor, anchorOff=aoff,
                     upright=upright if flags & 4 else [0.0] * 9, anchorAge=int(ages.min()) if len(ages) else 0)
            # PHH merge key (kh2fx_format Batch.key): no anchor / anchorOff / anchorAge (the first batch keeps its own)
            key = json.dumps({k: v for k, v in b.items() if k not in ("anchor", "anchorOff", "anchorAge")}, sort_keys=True)
            # s164: vertex columns stay numpy arrays (a list of pieces per merged batch) until the frame is written
            b.update(vertexCount=len(idx), p=[c[idx]], o=[o[idx]], uv=[uv[idx]], age=[np.minimum(ages[idx], 0xFFFF)],
                     c=[c16[idx]])
            if batches and not (flags & 2) and not (batches[-1][1]["flags"] & 2) and batches[-1][0] == key:   # sorted: never merged
                m = batches[-1][1]
                for f in ("p", "o", "uv", "age", "c"):
                    m[f] += b[f]
                m["vertexCount"] += b["vertexCount"]
            else:
                batches.append((key, b))
        bl = [b for _, b in batches]
        for b in bl:
            for f in VCOLS:
                b[f] = np.concatenate(b[f])
        if fxv:
            for b in bl:
                b["vb"] = fxvbin.vertex_bytes(*(b.pop(f) for f in VCOLS))
            fxv.frame(fi, lk.hexdigest(), texuse, bl)
        else:
            for b in bl:
                for f in VCOLS:
                    b[f] = b[f].tolist()
            per.append(dict(frame=fi, batches=bl, texuse=texuse, loopkey=lk.hexdigest()))
    return moved, anyA, per


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

"""fxvbin.py (s164) - fxv2: the binary form of fxsolve's fxv1 json (one recording = one element's frames). Own code.

Why: json keeps no NaN sign (s163: 4896 x86 default NaNs ffc00000 came back as 7fc00000), is ~3x bigger and slow to parse.
fxv2 keeps every value's exact bits: batch fields as float64 / int, vertices as the 44-byte PHF2 vertex the writer packs
(3f p, 3f o, 2f uv, u16 age, u16 0, 4 u16 colour), cast float64 -> float32 once here exactly like struct.pack('<f')
(round to nearest; a finite value that turns inf is an error, as in struct).

  file   = "FXV2" u32 frames u32 flags (1 = cut / not ended)
           frames x [ "FRM\\0" u32 frame, 16 B loopkey, u32 ntex, ntex x i32 texuse, u32 nbatch, nbatch x (HDR + n x 44 B) ]
           "END\\0" u32 len, json meta (schema, name, frames, flags, bone, paxFlags, stats)
  python -E -P fxvbin.py <rec.fxv> [--json <out.json>]     (summary / fxv1 json for the compare tools)
"""
import json
import os
import struct
import sys

import numpy as np

MAGIC = b"FXV2"
HDR = struct.Struct("<IiI4i4i4id4d4d3d3d3d9diI")
VDT = np.dtype([("p", "<f4", (3,)), ("o", "<f4", (3,)), ("uv", "<f4", (2,)), ("age", "<u2"), ("pad", "<u2"), ("c", "<u2", (4,))])
assert VDT.itemsize == 44
FIELDS = ("tex", "program", "flags", "blend", "depth", "sampler", "alphaRef", "fog", "tfog", "fogColour", "anchor",
          "anchorOff", "upright", "anchorAge", "vertexCount")
WIDTH = {"blend": 4, "depth": 4, "sampler": 4, "fog": 4, "tfog": 4, "fogColour": 3, "anchor": 3, "anchorOff": 3, "upright": 9}


def to_f32(a, what):
    """float64 -> float32 as struct.pack('<f') does it (one C cast; NaN keeps its sign)"""
    a = np.asarray(a, np.float64)
    with np.errstate(over="ignore", invalid="ignore"):
        b = a.astype(np.float32)
    if np.any(np.isinf(b) & np.isfinite(a)):
        raise OverflowError("fxvbin: %s value too large for float32" % what)
    return b


def vertex_bytes(p, o, uv, age, c):
    """N x 3, N x 3, N x 2 float64, N ages, N x 4 colour u16 -> the PHF2 vertex bytes"""
    n = len(p)
    v = np.zeros(n, VDT)
    v["p"], v["o"], v["uv"] = to_f32(p, "p").reshape(n, 3), to_f32(o, "o").reshape(n, 3), to_f32(uv, "uv").reshape(n, 2)
    age, c = np.asarray(age, np.int64), np.asarray(c, np.int64).reshape(n, 4)
    if n and (age.min() < 0 or age.max() > 0xFFFF or c.min() < 0 or c.max() > 0xFFFF):
        raise OverflowError("fxvbin: age / colour out of u16")
    v["age"], v["c"] = age, c
    return v.tobytes()


def end_record(meta):
    m = json.dumps(meta).encode()
    return b"END\0" + struct.pack("<I", len(m)) + m


class Writer:
    """fxv2 out file, written frame by frame to <path>.tmp, renamed on close (a killed solve leaves no torn .fxv)"""

    def __init__(self, path, frames, flags, header=True):
        """header=False (s168): a PART file = frames only, no FXV2 header, no END (fxsolve --part; joined by fxsolve)"""
        self.path, self.tmp = path, path + ".tmp"
        self.f = open(self.tmp, "wb", buffering=1 << 22)
        if header:
            self.f.write(MAGIC + struct.pack("<II", frames, flags))
        self.n = 0

    def frame(self, fi, loopkey, texuse, batches):
        """batches: dicts with the FIELDS (vertexCount included) + "vb" = vertex bytes"""
        f = self.f
        f.write(b"FRM\0" + struct.pack("<I", fi) + bytes.fromhex(loopkey) + struct.pack("<I", len(texuse)))
        f.write(struct.pack("<%di" % len(texuse), *texuse))
        f.write(struct.pack("<I", len(batches)))
        for b in batches:
            if len(b["vb"]) != 44 * b["vertexCount"]:
                raise ValueError("fxvbin: vertex bytes / count mismatch")
            vals = []
            for k in FIELDS:
                vals += list(b[k]) if k in WIDTH else [b[k]]
            f.write(HDR.pack(*vals))
            f.write(b["vb"])
        self.n += 1

    def close(self, meta):
        self.f.write(end_record(meta))
        self.f.close()
        os.replace(self.tmp, self.path)

    def finish_part(self):
        self.f.close()
        os.replace(self.tmp, self.path)

    def abort(self):
        self.f.close()
        if os.path.exists(self.tmp):
            os.remove(self.tmp)


def _hdr(vals):
    b, i = {}, 0
    for k in FIELDS:
        w = WIDTH.get(k)
        if w:
            b[k] = list(vals[i:i + w]); i += w
        else:
            b[k] = vals[i]; i += 1
    return b


def read(path):
    """-> (head dict(frames, flags), frame generator). Each frame: dict(frame, loopkey hex, texuse, batches); each batch =
    the FIELDS + "vb" (vertex bytes). The file is read once; frames are cut out as they are asked for."""
    buf = open(path, "rb").read()
    if buf[:4] != MAGIC:
        raise ValueError("%s: not an fxv2 file" % path)
    frames, flags = struct.unpack_from("<II", buf, 4)

    def gen():
        p = 12
        for want in range(frames):
            if buf[p:p + 4] != b"FRM\0":
                raise ValueError("%s: torn file at frame %d" % (path, want))
            fi, = struct.unpack_from("<I", buf, p + 4)
            key = buf[p + 8:p + 24].hex()
            nt, = struct.unpack_from("<I", buf, p + 24); p += 28
            tex = list(struct.unpack_from("<%di" % nt, buf, p)); p += 4 * nt
            nb, = struct.unpack_from("<I", buf, p); p += 4
            bs = []
            for _ in range(nb):
                b = _hdr(HDR.unpack_from(buf, p)); p += HDR.size
                n = 44 * b["vertexCount"]
                b["vb"] = buf[p:p + n]; p += n
                bs.append(b)
            yield dict(frame=fi, loopkey=key, texuse=tex, batches=bs)
        if buf[p:p + 4] != b"END\0":
            raise ValueError("%s: no END after %d frames" % (path, frames))
    return dict(frames=frames, flags=flags), gen()


def meta(path):
    buf = open(path, "rb").read()
    at = buf.rfind(b"END\0")
    n, = struct.unpack_from("<I", buf, at + 4)
    return json.loads(buf[at + 8:at + 8 + n])


def to_fxv1(path):
    """the fxv1 json dict of an fxv2 file (vertex floats = the stored float32 values), for fxv_cmp.py / phf2proof.py"""
    head, frames = read(path)
    per = []
    for fr in frames:
        bs = []
        for b in fr["batches"]:
            v = np.frombuffer(b.pop("vb"), VDT)
            b.update(p=v["p"].astype(np.float64).tolist(), o=v["o"].astype(np.float64).tolist(),
                     uv=v["uv"].astype(np.float64).tolist(), age=v["age"].tolist(), c=v["c"].tolist())
            bs.append(b)
        per.append(dict(frame=fr["frame"], batches=bs, texuse=fr["texuse"], loopkey=fr["loopkey"]))
    m = meta(path)
    m.update(schema="fxv1", perFrame=per)
    return m


def load_any(path):
    """fxv1 json or fxv2 binary -> fxv1 dict"""
    with open(path, "rb") as f:
        if f.read(4) == MAGIC:
            return to_fxv1(path)
    return json.load(open(path, encoding="utf-8"))


def main(a):
    if not a:
        print(__doc__); return 2
    if "--json" in a:
        open(a[a.index("--json") + 1], "w", newline="\n").write(json.dumps(to_fxv1(a[0])))
        return 0
    head, frames = read(a[0])
    nb = nv = 0
    for fr in frames:
        nb += len(fr["batches"]); nv += sum(b["vertexCount"] for b in fr["batches"])
    print("%s: %d frames, flags %d, %d batches, %d vertices; meta %s" % (a[0], head["frames"], head["flags"], nb, nv,
                                                                         json.dumps(meta(a[0])["stats"])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

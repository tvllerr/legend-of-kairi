"""Dump FULL per-vertex data of one PHH kh2_fx.bin element as schema fxv1. Own code.
usage: python -E -P phf2v.py <kh2_fx.bin> <name> --out <json> [--frames N]"""
import struct, sys, json, zlib, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from phf2dump import Phf2, BATCH

VS = struct.Struct("<3f3f2fHH4H")
assert VS.size == 44


def read_frame(ph, e, i, tab, base):
    ph.f.seek(base + tab[i])
    d = zlib.decompress(ph.f.read(tab[i + 1] - tab[i]))
    cnt = struct.unpack_from("<I", d, 0)[0]
    p = 4
    out = []
    for _ in range(cnt):
        v = BATCH.unpack_from(d, p)
        p += BATCH.size
        vc = v[43]
        P, O, UV, A, C = [], [], [], [], []
        for x in VS.iter_unpack(d[p:p + vc * 44]):
            P.append(list(x[0:3])); O.append(list(x[3:6])); UV.append(list(x[6:8]))
            A.append(x[8]); C.append(list(x[10:14]))
        p += vc * 44
        out.append(dict(tex=v[0], program=v[1], flags=v[2], blend=list(v[3:7]), depth=list(v[7:11]),
                        sampler=list(v[11:15]), alphaRef=v[15], fog=list(v[16:20]), tfog=list(v[20:24]),
                        fogColour=list(v[24:27]), anchor=list(v[27:30]), anchorOff=list(v[30:33]),
                        upright=list(v[33:42]), anchorAge=v[42], vertexCount=vc,
                        p=P, o=O, uv=UV, age=A, c=C))
    assert p == len(d)
    return out


def main(a):
    if len(a) < 2 or "--out" not in a:
        print(__doc__); return 2
    path, name = a[0], a[1]
    out = a[a.index("--out") + 1]
    nfr = int(a[a.index("--frames") + 1]) if "--frames" in a else None
    ph = Phf2(path)
    e = ph.eff[name]
    tab, base = ph.frame_chunks(e)
    n = e["frames"] if nfr is None else min(nfr, e["frames"])
    pf = [dict(frame=i, batches=read_frame(ph, e, i, tab, base)) for i in range(n)]
    for f in pf:
        for b in f["batches"]:
            assert b["vertexCount"] == len(b["p"]) == len(b["c"])
    res = dict(schema="fxv1", name=name, frames=n, flags=e["flags"], bone=e["bone"], paxFlags=e["paxFlags"], perFrame=pf)
    with open(out, "w", newline="\n") as fh:
        json.dump(res, fh)
    print(name, "frames", n, "batches", sum(len(f["batches"]) for f in pf),
          "vertices", sum(b["vertexCount"] for f in pf for b in f["batches"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

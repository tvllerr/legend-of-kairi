"""Pure-python reader/dumper for PHH kh2_fx.bin (PHF2 v4). Own code.
usage: python -E -P phf2dump.py <kh2_fx.bin> <name> [...] --out <json>
       python -E -P phf2dump.py <kh2_fx.bin> --list <substring>"""
import struct, sys, json, zlib

BATCH = struct.Struct("<HBB 4B 4B 4B f 4f 4f 3f 3f 3f 9f I I")
assert BATCH.size == 132
VERT = 44
EREC = struct.Struct("<32sIIIIIhHI")


class Phf2:
    def __init__(self, path):
        self.f = open(path, "rb")
        h = self.f.read(24)
        magic, ver, self.ntex, self.neff, self.ttab, self.etab = struct.unpack("<4sIIIII", h)
        assert magic == b"PHF2" and ver == 4, (magic, ver)
        self.f.seek(self.etab)
        raw = self.f.read(60 * self.neff)
        self.eff = {}
        for i in range(self.neff):
            n, off, pk, rw, fr, fl, bone, pax, _ = EREC.unpack_from(raw, i * 60)
            self.eff[n.split(b"\0")[0].decode()] = dict(offset=off, packed=pk, raw=rw, frames=fr, flags=fl, bone=bone, paxFlags=pax)

    def frame_chunks(self, e):
        """chunk table offsets are relative to the end of the (frames+1)-entry table."""
        self.f.seek(e["offset"])
        n = e["frames"]
        tab = struct.unpack("<%dI" % (n + 1), self.f.read(4 * (n + 1)))
        base = e["offset"] + 4 * (n + 1)
        return tab, base

    def frame(self, e, i, tab=None, base=None):
        if tab is None:
            tab, base = self.frame_chunks(e)
        self.f.seek(base + tab[i])
        d = zlib.decompress(self.f.read(tab[i + 1] - tab[i]))
        cnt = struct.unpack_from("<I", d, 0)[0]
        p = 4
        out = []
        for _ in range(cnt):
            v = BATCH.unpack_from(d, p)
            p += 132
            vc = v[-1]
            out.append(dict(tex=v[0], program=v[1], flags=v[2], blend=list(v[3:7]), vertexCount=vc))
            p += vc * VERT
        assert p == len(d), "batch sizes do not add up: %d != %d" % (p, len(d))
        return out


def dump(ph, name):
    e = ph.eff[name]
    tab, base = ph.frame_chunks(e)
    assert tab[0] == 0, "chunk[0]=%d" % tab[0]
    assert base + tab[-1] == e["offset"] + e["packed"], "chunk end %d vs packed %d" % (base + tab[-1], e["offset"] + e["packed"] - base)
    frames = []
    for i in range(e["frames"]):
        b = ph.frame(e, i, tab, base)
        frames.append(dict(frame=i, batches=b, totalBatches=len(b), totalVertices=sum(x["vertexCount"] for x in b)))
    return dict(name=name, frames=e["frames"], flags=e["flags"], bone=e["bone"], paxFlags=e["paxFlags"], perFrame=frames)


def main(a):
    if len(a) < 2:
        print(__doc__); return 2
    ph = Phf2(a[0])
    if a[1] == "--list":
        s = a[2] if len(a) > 2 else ""
        for n in sorted(ph.eff):
            if s in n:
                print(n, ph.eff[n]["frames"])
        return 0
    out = None
    names = []
    i = 1
    while i < len(a):
        if a[i] == "--out":
            out = a[i + 1]; i += 2
        else:
            names.append(a[i]); i += 1
    res = [dump(ph, n) for n in names]
    js = json.dumps(res[0] if len(res) == 1 else res)
    if out:
        open(out, "w", newline="\n").write(js)
    for r in res:
        print(r["name"], "frames", r["frames"], "ok")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

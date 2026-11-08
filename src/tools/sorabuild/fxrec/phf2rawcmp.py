"""phf2rawcmp.py (s164) - exact byte proof of OUR kh2_fx.bin elements vs the PHH oracle bin. Own code.
  python -E -P phf2rawcmp.py <our kh2_fx.bin> [<oracle kh2_fx.bin>]

Per element of ours (by name): the effect record (raw size, frames, cut, bone, flags) and every frame's DECOMPRESSED bytes
must equal the oracle's, except the batch texture slot (u16 at batch +0), which goes through one global ours -> oracle map
(a subset run numbers textures from 0). Floats are compared as bytes: NaN bits count. Then each mapped texture blob must be
byte-identical. Exit 0 = PASS.
"""
import struct
import sys
import zlib

HEAD = struct.Struct("<4sIIIII")
TEXENT = struct.Struct("<II")
EFFENT = struct.Struct("<32sIIIIIhHI")
BATCH = 132
VERT = 44
ORACLE = "%LOCALAPPDATA%/ProjectHeartHunters/cache/kh2_fx.bin"


class Bin:
    def __init__(self, path):
        self.f = open(path, "rb")
        magic, ver, nt, ne, toff, eoff = HEAD.unpack(self.f.read(HEAD.size))
        assert magic == b"PHF2" and ver == 4, path
        self.f.seek(toff)
        self.tex = [TEXENT.unpack(self.f.read(8)) for _ in range(nt)]
        self.f.seek(eoff)
        self.eff = {}
        for _ in range(ne):
            r = EFFENT.unpack(self.f.read(EFFENT.size))
            self.eff[r[0].split(b"\0")[0].decode()] = r[1:]

    def blob(self, off, size):
        self.f.seek(off)
        return self.f.read(size)

    def frames(self, name):
        off, size, raw, nfr = self.eff[name][:4]
        body = self.blob(off, size)
        offs = struct.unpack_from("<%dI" % (nfr + 1), body, 0)
        base = 4 * (nfr + 1)
        for i in range(nfr):
            yield zlib.decompress(body[base + offs[i]:base + offs[i + 1]])


def tex_slots(raw):
    """(byte offset, slot) of every batch texture field in one raw frame"""
    n, = struct.unpack_from("<I", raw, 0)
    p, out = 4, []
    for _ in range(n):
        out.append((p, struct.unpack_from("<H", raw, p)[0]))
        vc, = struct.unpack_from("<I", raw, p + BATCH - 4)
        p += BATCH + VERT * vc
    if p != len(raw):
        raise ValueError("frame layout: %d != %d" % (p, len(raw)))
    return out


def main(a):
    if not a:
        print(__doc__); return 2
    ours, orc = Bin(a[0]), Bin(a[1] if len(a) > 1 else ORACLE)
    tmap, bad, nfr, nel = {}, [], 0, 0
    for name, e in ours.eff.items():
        nel += 1
        if name not in orc.eff:
            bad.append((name, "not in oracle")); continue
        o = orc.eff[name]
        if e[2:] != o[2:]:          # raw size, frames, cut, bone, flags, pad
            bad.append((name, "record", e[2:], o[2:])); continue
        for i, (x, y) in enumerate(zip(ours.frames(name), orc.frames(name))):
            nfr += 1
            if len(x) != len(y):
                bad.append((name, i, "frame size %d vs %d" % (len(x), len(y)))); break
            sx, sy = tex_slots(x), tex_slots(y)
            xb, ok = bytearray(x), True
            for (px, tx), (py, ty) in zip(sx, sy):
                if px != py:
                    ok = False; break
                if tx < 0xFFFE:
                    if tmap.setdefault(tx, ty) != ty:
                        ok = False; break
                    struct.pack_into("<H", xb, px, ty)
            if not ok or bytes(xb) != y:
                k = next((j for j in range(len(y)) if xb[j] != y[j]), -1)
                bad.append((name, i, "bytes differ first at +%d" % k)); break
    texbad = [t for t, u in tmap.items() if ours.blob(*ours.tex[t]) != orc.blob(*orc.tex[u])]
    print("elements %d, frames %d byte-compared, textures mapped %d (%d differ)" % (nel, nfr, len(tmap), len(texbad)))
    for b in bad[:10]:
        print("  DIFF", b)
    ok = not bad and not texbad
    print("RAW PASS" if ok else "RAW FAIL (%d elements)" % len(bad))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

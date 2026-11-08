"""phf2subset.py (s161) - a PHF2 v4 cache dir holding only the named elements of another one (bodies + textures copied
byte for byte, textures re-indexed by first use is NOT done: all textures kept), for fxbake A/B runs. Own code.
  python -E -P phf2subset.py <src kh2_fx.bin> <names from this kh2_fx.bin> <out dir>
"""
import json
import os
import struct
import sys

HEADER = struct.Struct("<4sIIIII")
EFFREC = struct.Struct("<32sIIIIIhHI")


def safe_replace(tmp, final, *protected):
    """rename onto final (new inode); refuse when final is one of the protected files (e.g. the PHH oracle)"""
    for q in protected:
        if os.path.exists(final) and os.path.exists(q) and os.path.samefile(final, q):
            raise SystemExit(f"phf2subset: refusing to replace {final} (same file as {q})")
    os.replace(tmp, final)


def main(a):
    src, ref, out = a[0], a[1], a[2]
    os.makedirs(out, exist_ok=True)
    names = []
    with open(ref, "rb") as f:
        _, _, nt, ne, tt, et = HEADER.unpack(f.read(24))
        f.seek(et)
        names = [EFFREC.unpack(f.read(60))[0].split(b"\0")[0].decode() for _ in range(ne)]
    with open(src, "rb") as f:
        _, _, nt, ne, tt, et = HEADER.unpack(f.read(24))
        f.seek(tt)
        texs = [struct.unpack("<II", f.read(8)) for _ in range(nt)]
        f.seek(et)
        recs = {}
        for _ in range(ne):
            r = EFFREC.unpack(f.read(60))
            recs[r[0].split(b"\0")[0].decode()] = r
        keep = [n for n in names if n in recs]
        head = bytearray(24 + 8 * nt + 60 * len(keep))
        cur = len(head)
        trec = []
        for o, s in texs:
            trec.append((cur, s))
            cur += s + (-(cur + s) % 16)
        erec = []
        for n in keep:
            r = recs[n]
            erec.append((n, cur, r))
            cur += r[2] + (-(cur + r[2]) % 16)
        HEADER.pack_into(head, 0, b"PHF2", 4, nt, len(keep), 24, 24 + 8 * nt)
        for i, t in enumerate(trec):
            struct.pack_into("<II", head, 24 + 8 * i, *t)
        for i, (n, at, r) in enumerate(erec):
            EFFREC.pack_into(head, 24 + 8 * nt + 60 * i, r[0], at, *r[2:])
        tmpb = os.path.join(out, "kh2_fx.bin.new")
        with open(tmpb, "wb") as g:
            g.write(head)
            for (o, s) in texs:
                f.seek(o)
                g.write(f.read(s))
                g.write(bytes(-g.tell() % 16))
            for n, at, r in erec:
                assert g.tell() == at
                f.seek(r[1])
                g.write(f.read(r[2]))
                g.write(bytes(-g.tell() % 16))
    safe_replace(tmpb, os.path.join(out, "kh2_fx.bin"), src, src[:-4] + ".json")
    js = json.load(open(src[:-4] + ".json", encoding="utf-8"))
    js["effects"] = {n: js["effects"][n] for n in keep}
    tmpj = os.path.join(out, "kh2_fx.json.new")
    with open(tmpj, "w", encoding="utf-8", newline="\n") as g:
        json.dump(js, g, indent=1)
    safe_replace(tmpj, os.path.join(out, "kh2_fx.json"), src, src[:-4] + ".json")
    print(len(keep), "elements ->", out)


if __name__ == "__main__":
    main(sys.argv[1:])

"""Carve one participant's motions out of a Limit bar (limit/fm/<file>.bar entry 'LIMI' = the Limit's motion set) into a
plain mset BAR (slot = base + 4 * (n - 252)) that tools_s133l/limitbake (partybake + env overrides) reads like P_EX0x0.mset.
  python -E -P mkmset.py <file> <base> <body name> <weapon name> <out body.mset> <out weapon.mset>
e.g. goofy 1100 0010 0011 out_s133/limit/goofy_sora.mset out_s133/limit/goofy_sora_w.mset"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # (run with -E -P: no script dir on sys.path)
from barls import entries

def bar(slots, count):
    """slots: {index: bytes}; every other index is an empty DUMM entry (like KH2's msets)."""
    head = 16 + 16 * count
    out = bytearray(b"BAR\x01" + struct.pack("<I", count) + b"\0" * 8)
    data = bytearray()
    for i in range(count):
        if i in slots:
            while (head + len(data)) % 16: data.append(0)
            off = head + len(data)
            data += slots[i]
            out += struct.pack("<I4sII", 17, ("A%03d" % (i % 1000)).encode(), off, len(slots[i]))
        else:
            out += struct.pack("<I4sII", 17, b"DUMM", head, 0)
    return bytes(out + data)

def carve(file, name, first=252, last=270):
    b = open(os.path.join(os.environ["SORABAKE_KH2"], "kh2_fifth/original/limit/fm/%s.bar" % file), "rb").read()
    ls = [e for e in entries(b) if e[2] == "LIMI"][0]
    m = b[ls[3]:ls[3] + ls[4]]
    got = {}
    for e in entries(m):
        if e[4] == 0 or not (first <= e[0] <= last): continue
        s = m[e[3]:e[3] + e[4]]
        for y in entries(s):
            if y[2] == name and y[4]:
                got[e[0]] = s[y[3]:y[3] + y[4]]
    return got

if __name__ == "__main__":
    f, base, nb, nw, ob, ow = sys.argv[1:7]
    base = int(base)
    idle = sys.argv[7] if len(sys.argv) > 7 else None   # the real weapon mset: its slot 0 (the grip at rest) is the pose of a slot without a weapon motion
    for name, out in ((nb, ob), (nw, ow)):
        got = carve(f, name)
        slots = {base + 4 * (k - 252): v for k, v in got.items()}   # id*4: Sora's mot_of(id) finds var 0 only
        if idle and out == ow:
            ib = open(idle, "rb").read()
            e0 = entries(ib)[0]
            slots[0] = ib[e0[3]:e0[3] + e0[4]]
        open(out, "wb").write(bar(slots, base + 4 * 20))
        print(out, sorted(slots), [len(v) for v in slots.values()])

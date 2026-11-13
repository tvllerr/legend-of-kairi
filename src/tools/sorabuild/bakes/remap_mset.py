"""(s223 cloud) Copy chosen motions of one or more msets into a new plain mset BAR at the slot numbers the party driver plays (the
player's layout: slot = motion id x 4 + variant), so partybake bakes an object whose mset has another layout (Cloud's N_HB550_BTL:
an N_ object's 209-entry mset, idle 0, attacks 201 / 202; his walk is in his town mset N_HB550_RTN, slot 1) like a P_ member's.
Each motion is copied byte for byte (its anb keeps its own name), every other slot is an empty DUMM entry (like KH2's msets).
  python -E -P remap_mset.py <out.mset> <dst=[k:]src,...> <src0.mset> [<src1.mset> ...]
k = which source file (default 0). e.g. out.mset 0=0,2=1:0,4=1:1,604=201 N_HB550_BTL.mset N_HB550_RTN.mset"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # (run with -E -P: no script dir on sys.path)
from barls import entries


def remap(srcs, pairs):
    """srcs: [mset bytes]; pairs: [(dst, file index, src slot)] -> the new BAR bytes"""
    ents = [entries(s) for s in srcs]
    count = max(d for d, _, _ in pairs) + 1
    want = {d: (k, s) for d, k, s in pairs}
    if len(want) != len(pairs):
        raise SystemExit("remap_mset: a destination slot is named twice")
    head = 16 + 16 * count
    out = bytearray(b"BAR\x01" + struct.pack("<I", count) + b"\0" * 8)
    data = bytearray()
    for i in range(count):
        if i in want:
            k, s = want[i]
            if not 0 <= k < len(srcs) or not 0 <= s < len(ents[k]):
                raise SystemExit("remap_mset: source %d:%d is not there" % (k, s))
            _, t, nm, off, sz = ents[k][s]
            if t != 17 or sz == 0:
                raise SystemExit("remap_mset: source %d:%d (%s) is empty / not a motion (type %d)" % (k, s, nm, t))
            while (head + len(data)) % 16:
                data.append(0)
            o = head + len(data)
            data += srcs[k][off:off + sz]
            out += struct.pack("<I4sII", 17, nm.encode("latin1")[:4].ljust(4, b"\0"), o, sz)
        else:
            out += struct.pack("<I4sII", 17, b"DUMM", head, 0)
    return bytes(out + data)


def parse(spec):
    pairs = []
    for it in spec.split(","):
        d, _, s = it.partition("=")
        k, _, s2 = s.rpartition(":")
        if not d.strip().isdigit() or not s2.strip().isdigit() or (k and not k.isdigit()):
            raise SystemExit("remap_mset: %r is not <dst>=[<file>:]<src>" % it)
        pairs.append((int(d), int(k or 0), int(s2)))
    return pairs


if __name__ == "__main__":
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    srcs = [open(p, "rb").read() for p in sys.argv[3:]]
    out = remap(srcs, parse(sys.argv[2]))
    with open(sys.argv[1], "wb") as f:
        f.write(out)
    print("%s: %d slots, %d bytes (%s)" % (sys.argv[1], struct.unpack_from("<I", out, 4)[0], len(out), sys.argv[2]))

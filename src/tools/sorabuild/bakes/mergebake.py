"""Append the slots of an 'add' bake file to a live bake file (SKRM motion.bin or SKRP pose.bin); idempotent (slots the live
file already has are skipped), same-size bone count required for poses. Writes <out>; never touches the inputs.
  python -I mergebake.py motion <live> <add> <out>
  python -I mergebake.py pose   <live> <add> <out>
SKRM: header 16 {magic, ver, n, 0}, dir n x 24 {u16 slot, frames, loop, flags, u32 off_root, off_bits, off_ftrig, u16 nft, start}, data (absolute offsets).
SKRP: header 16 {magic, ver, n, bones}, dir n x 8 {u16 slot, poses, u32 off}, data (absolute offsets)."""
import struct, sys

def read(path):
    return open(path, "rb").read()

def merge_motion(a, b):
    na, nb = struct.unpack_from("<II", a, 8)[0], struct.unpack_from("<I", b, 8)[0]
    have = {struct.unpack_from("<H", a, 16 + 24 * i)[0] for i in range(na)}
    rows, blobs = [], []
    data = bytearray()
    ent = lambda f, n, i: bytearray(f[16 + 24 * i:16 + 24 * i + 24])
    # existing: keep their data block as is (offsets are absolute: shift by the dir growth)
    add = [i for i in range(nb) if struct.unpack_from("<H", b, 16 + 24 * i)[0] not in have]
    grow = 24 * len(add)
    out_dir = bytearray()
    for i in range(na):
        e = ent(a, na, i)
        for o in (8, 12, 16):
            v = struct.unpack_from("<I", e, o)[0]
            if v: struct.pack_into("<I", e, o, v + grow)
        out_dir += e
    base = len(a) + grow          # new data goes after the old file
    tail = bytearray()
    for i in add:
        e = ent(b, nb, i)
        d0 = min(v for o in (8, 12, 16) for v in [struct.unpack_from("<I", e, o)[0]] if v)
        for o in (8, 12, 16):
            v = struct.unpack_from("<I", e, o)[0]
            if v: struct.pack_into("<I", e, o, base + len(tail) + (v - d0))
        # copy the slot's bytes: from its first offset to the end of its ftrig block (nft x 8)
        nft = struct.unpack_from("<H", e, 20)[0]
        end = struct.unpack_from("<I", b, 16 + 24 * i + 16)[0] + 8 * nft
        out_dir += e
        tail += b[d0:end]
    hdr = bytearray(a[:16]); struct.pack_into("<I", hdr, 8, na + len(add))
    return bytes(hdr + out_dir + a[16 + 24 * na:] + tail), [struct.unpack_from("<H", b, 16 + 24 * i)[0] for i in add]

def merge_pose(a, b):
    na, bones = struct.unpack_from("<II", a, 8)
    nb, bones_b = struct.unpack_from("<II", b, 8)
    if bones != bones_b: raise SystemExit("bone count differs: %d vs %d" % (bones, bones_b))
    have = {struct.unpack_from("<H", a, 16 + 8 * i)[0] for i in range(na)}
    add = [i for i in range(nb) if struct.unpack_from("<H", b, 16 + 8 * i)[0] not in have]
    grow = 8 * len(add)
    out_dir = bytearray()
    for i in range(na):
        s, n, off = struct.unpack_from("<HHI", a, 16 + 8 * i)
        out_dir += struct.pack("<HHI", s, n, off + grow)
    tail = bytearray()
    base = len(a) + grow
    for i in add:
        s, n, off = struct.unpack_from("<HHI", b, 16 + 8 * i)
        out_dir += struct.pack("<HHI", s, n, base + len(tail))
        tail += b[off:off + n * bones * 48]
    hdr = bytearray(a[:16]); struct.pack_into("<I", hdr, 8, na + len(add))
    return bytes(hdr + out_dir + a[16 + 8 * na:] + tail), [struct.unpack_from("<H", b, 16 + 8 * i)[0] for i in add]

if __name__ == "__main__":
    kind, live, add, out = sys.argv[1:5]
    a, b = read(live), read(add)
    assert a[:4] == b[:4] == (b"SKRM" if kind == "motion" else b"SKRP")
    res, added = (merge_motion if kind == "motion" else merge_pose)(a, b)
    open(out, "wb").write(res)
    print("%s: +%d slots %s -> %s (%d bytes)" % (kind, len(added), added, out, len(res)))

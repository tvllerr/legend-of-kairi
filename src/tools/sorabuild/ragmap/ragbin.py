"""ragbin.py <ragmap header from `sorabake ragmap`> <out.bin>: the KH2 skeleton numbers of one character as a small binary file
(data/ragmap/<char>.bin) that the mod reads at startup. Layout (little endian):
  u32 'SRAG' (0x47415253), u32 version 1, u32 nbones, u32 nmap,
  int16 parent[nbones] (padded with zeros to a multiple of 4 bytes),
  float32 bind_pos[nbones][3]   (rest pose world position of every bone, cm; the Keyblade's in its own weapon space),
  float32 map[nmap][4]          (per joint-map entry: unit direction x y z + length cm, the bind pose of sora -> sora_to).
The header is only a scratch file in the build's work dir (it is KH2-derived; nothing of it is shipped)."""
import re
import struct
import sys

MAGIC, VERSION = 0x47415253, 1
FLT = r"-?\d+\.\d+f"


def parse(text):
    m = re.search(r"static const int16_t \w+_rag_parent\[\w+\] = \{(.*?)\};", text, re.S)
    parent = [int(x) for x in re.findall(r"-?\d+", m.group(1))]
    m = re.search(r"static const float \w+_rag_bind_pos\[\w+\]\[3\] = \{(.*?)\n\};", text, re.S)
    pos = [tuple(float(x[:-1]) for x in t) for t in re.findall(r"\{(%s), (%s), (%s)\}" % (FLT, FLT, FLT), m.group(1))]
    m = re.search(r"static const SoraRagBone \w+\[\] = \{(.*?)\n\};", text, re.S)
    rows = []
    for ln in m.group(1).splitlines():
        t = re.search(r"\{(%s), (%s), (%s)\}, (%s), \"" % (FLT, FLT, FLT, FLT), ln)
        if t:
            rows.append(tuple(float(x[:-1]) for x in t.groups()))
    if len(pos) != len(parent):
        raise SystemExit("ragbin: %d parents but %d bind positions" % (len(parent), len(pos)))
    return parent, pos, rows


def pack(parent, pos, rows):
    out = bytearray(struct.pack("<4I", MAGIC, VERSION, len(parent), len(rows)))
    out += struct.pack("<%dh" % len(parent), *parent)
    out += b"\0" * (-len(out) % 4)
    for p in pos:
        out += struct.pack("<3f", *p)
    for r in rows:
        out += struct.pack("<4f", *r)
    return bytes(out)


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        parent, pos, rows = parse(f.read())
    with open(sys.argv[2], "wb") as f:
        f.write(pack(parent, pos, rows))
    print("%s: %d bones, %d map entries" % (sys.argv[2], len(parent), len(rows)))


if __name__ == "__main__":
    main()

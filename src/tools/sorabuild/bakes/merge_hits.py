# Merge: keep every slot of the live sora_hits.bin byte-for-byte, add slots 664/668/684/788 from a FBODY hitbake.
import struct, sys
old_p, new_p, out_p = sys.argv[1:4]
ADD = [664, 668, 684, 788]
def parse(p):
    d = open(p, 'rb').read(); assert d[:4] == b'SKRH'
    n = struct.unpack_from('<I', d, 8)[0]; r = {}
    for i in range(n):
        s, nw, ns, _, ow, os_ = struct.unpack_from('<4HII', d, 16 + 16 * i)
        r[s] = (d[ow:ow + 16 * nw], d[os_:os_ + 24 * ns], nw, ns)
    return r
a, b = parse(old_p), parse(new_p)
for s in ADD:
    assert s not in a and s in b
    a[s] = b[s]
slots = sorted(a); head = 16 + 16 * len(slots); body = bytearray(); dr = []
for s in slots:
    w, g, nw, ns = a[s]; ow = head + len(body); body += w; os_ = head + len(body); body += g
    dr.append(struct.pack('<4HII', s, nw, ns, 0, ow, os_))
open(out_p, 'wb').write(b'SKRH' + struct.pack('<III', 1, len(slots), 0) + b''.join(dr) + bytes(body))
print(out_p, len(slots), 'slots', head + len(body), 'bytes')

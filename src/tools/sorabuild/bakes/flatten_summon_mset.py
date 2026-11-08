# (s134) Summon mset slots 252/253 (Sora's call/finish motion) are entry type 20 = a BAR of 12 BARs (only one non-empty).
# Writes a copy of the mset with those entries replaced by the inner motion BAR (type 17), so UNCHANGED sorabake can bake them.
# usage: python -I flatten_summon_mset.py in.mset out.mset [slot ...]   (default 252 253)
import struct, sys
src, dst = sys.argv[1], sys.argv[2]
# (s137) an argument may be "slot" (the single non-empty inner motion) or "slot:idx" (inner entry idx of the 12-entry BAR)
fix = {}
for a in (sys.argv[3:] or ['252', '253']):
    sl, _, ix = a.partition(':')
    fix[int(sl)] = int(ix) if ix else None
d = open(src, 'rb').read()
cnt = struct.unpack_from('<i', d, 4)[0]
ents = []
for i in range(cnt):
    t, dup, nm, off, sz = struct.unpack_from('<HH4sII', d, 16 + 16 * i)
    blob = d[off:off + sz]
    if i in fix and t == 20:
        c = struct.unpack_from('<i', blob, 4)[0]
        inner = [blob[o:o + s] for (_, _, _, o, s) in (struct.unpack_from('<HH4sII', blob, 16 + 16 * j) for j in range(c)) if s > 16]
        if fix[i] is not None:
            _, _, _, o, s = struct.unpack_from('<HH4sII', blob, 16 + 16 * fix[i]); inner = [blob[o:o + s]]
        if len(inner) > 1 and fix[i] is None:
            print('slot %d: %d non-empty inner motions, using the first (idx with the triggers); pass slot:idx to choose' % (i, len(inner)), file=sys.stderr); inner = inner[:1]
        assert len(inner) == 1, (i, len(inner))
        blob, t = inner[0], 17
    ents.append((t, dup, nm, blob))
pos = 16 + 16 * cnt
body = bytearray(); hdr = bytearray()
for t, dup, nm, blob in ents:
    body += b'\0' * ((-(pos + len(body))) % 16)
    hdr += struct.pack('<HH4sII', t, dup, nm, pos + len(body), len(blob))
    body += blob
open(dst, 'wb').write(d[:16] + bytes(hdr) + bytes(body))

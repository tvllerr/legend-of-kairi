#!/usr/bin/env python3
"""(s137 sumdata) Genie's four Drive-Form versions live in ONE mesh: 12 variant groups, 3 per version, told apart by the KH2 group attribute bits 6..8
(attr >> 6 & 7 = 2 Valor, 3 Wisdom, 4 Master, 5 Final = the part ids in the exe's version table at rva 727E80, see the report). The runtime's draw pass can already
show ONE of several alternative groups (the s85 Keyblade alt-blade mechanism: group attr has ATTR_WEAPON 0x100 and an alt id in bits 24..31; the actor draws the
group only if alt == blade[chain], set by sora_draw_actor_blade(actor, 0, alt); chain 0 and default alt 1 for body groups).
So this rewrites the 12 groups: attr |= 0x100 | (version << 24), version 1 Valor, 2 Wisdom, 3 Master, 4 Final.
Then   sora_draw_actor_blade(genie_actor, 0, version)   picks the version with NO exe change (default = 1 Valor). Do not call sora_draw_actor_weapon(.., false)
(hide_weapon would hide all four).   usage: python -I genie_alt.py in_model.bin out_model.bin"""
import struct, sys
src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, "rb").read())
magic, ver, nb, ng, nt, nv, ni, nx = struct.unpack_from("<8I", d, 0)
assert magic == 0x47524B53 and ver == 1
go = 32 + 16 * nt
ver_of = {0x80: 1, 0xC0: 2, 0x100: 3, 0x140: 4}
cnt = {1: 0, 2: 0, 3: 0, 4: 0}
for g in range(ng):
    tex, attr, fi, n = struct.unpack_from("<4I", d, go + 16 * g)
    if attr & 0xFF000000 == 0 and (attr & 0x1C0) in (0x80, 0xC0, 0x100, 0x140) and (attr & ~0x1C7) == 0:
        v = ver_of[attr & 0x1C0]
        struct.pack_into("<I", d, go + 16 * g + 4, attr | 0x100 | (v << 24))
        cnt[v] += 1
        print("group %d tex %d attr %#x -> %#x (version %d)" % (g, tex, attr, attr | 0x100 | (v << 24), v))
assert cnt == {1: 3, 2: 3, 3: 3, 4: 3}, cnt
open(dst, "wb").write(d)
print("wrote", dst)

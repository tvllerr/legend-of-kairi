# s139 sumplate: bake the KH2 summon portraits (the 'face' Imgd of each summon's .a.us, PC HD 256x256) into sumface.bin
# 'SKSF' v1: u32 magic, ver, n, tile px, atlas w, atlas h, cols; then n x {i32 l, t, r, b} (the face part's rect in PS2 px
# around the plate's anchor, from the face Seqd's anim group 3: recon/S139_RESEARCH_SUMPLATE.md), then RGBA8 atlas (tiles in
# a grid, 'cols' per row, order below).  Read-only on D:/KH2Extract.  Usage: python -I tools_s139/sumface_bake.py [out]
import struct, sys
import os
KH2 = os.environ['SORABUILD_KH2'].rstrip('/')  # (sorabuild) the KH2 extract root, was D:/KH2Extract
from PIL import Image  # (s154 review) run with python -E -P (user site visible); the old by-path fallback is gone
R = KH2 + '/'
TILE = 256
FACES = [  # (name, HD file, rect l t r b)
    ('Chicken Little P_EX350', R + 'kh2_sixth/remastered/obj/P_EX350.a.us/-15.png', (-30, -36, 32, 28)),
    ('Stitch N_HB040_BTL',     R + 'kh2_fifth/remastered/obj/N_HB040_BTL.a.us/-18.png', (-30, -32, 32, 32)),
    ('Genie P_AL010',          R + 'kh2_sixth/remastered/obj/P_AL010.a.us/-21.dds', (-30, -36, 32, 28)),
    ('Peter Pan P_EX330',      R + 'kh2_sixth/remastered/obj/P_EX330.a.us/-10.dds', (-30, -32, 32, 32)),
    # spare (not drawn by the plate: KH2 shows Peter's). Her face is a 256x128 image; rect = GUESS (Peter's)
    ('Tinker Bell P_EX340',    R + 'kh2_sixth/remastered/obj/P_EX340.a.us/-0.dds', (-30, -32, 32, 32)),
]
out = sys.argv[1] if len(sys.argv) > 1 else 'out_s139sumplate/sumface.bin'
cols = 3
rows = (len(FACES) + cols - 1) // cols
atlas = Image.new('RGBA', (cols * TILE, rows * TILE), (0, 0, 0, 0))
for i, (nm, p, _) in enumerate(FACES):
    im = Image.open(p).convert('RGBA')
    print(nm, im.size)
    if im.width > TILE or im.height > TILE:
        im.thumbnail((TILE, TILE), Image.LANCZOS)  # (Tinker Bell's is 256x128: pasted at the tile's top-left, not stretched)
    atlas.paste(im, ((i % cols) * TILE, (i // cols) * TILE))
b = b'SKSF' + struct.pack('<6I', 1, len(FACES), TILE, atlas.width, atlas.height, cols)
for _, _, r in FACES:
    b += struct.pack('<4i', *r)
b += atlas.tobytes()
open(out, 'wb').write(b)
print(out, len(b), 'bytes, atlas', atlas.size)

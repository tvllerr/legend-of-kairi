# itempic_bake.py: bake KH2's Keyblade item pictures into out_s120/itempic.bin ('SKIP' v1) + the contact sheet.
#   python3 tools_s120/itempic_bake.py            (reads D:/KH2Extract/kh2_fifth/remastered/itempic/item-NNN.imd/-0.dds, 512x512 RGBA)
# KB_TAB index -> picture index = the item entry's +20 u16 in 03system.bin (VERIFIED, see recon/S120_HELPER_ITEMPIC.md).
# File: u32 magic 'SKIP', ver 1, w, h, n, pix_off | n x {u16 x, y, w, h, lw, lh} | RGBA8 w*h at pix_off (straight alpha).
# Cells = a fixed grid, COLS per row, CELL x CELL; cell i = KB_TAB[i] (the DLL's sora_camp_pic.h uses the same grid).
import struct, re, sys
import os
KH2 = os.environ['SORABUILD_KH2'].rstrip('/')  # (sorabuild) the KH2 extract root, was D:/KH2Extract
from PIL import Image, ImageDraw
PICS = [43, 46, 44, 45] + list(range(47, 65)) + [300, 301]   # KB_TAB order (runtime/sora_kb.h)
# (s214 kb) our own two Keyblades (rows 25 / 26): our own renders, tools/sorabuild/ours/kb/<name>_pic.png (512x512 RGBA, straight alpha)
PICS += ['sealing_blade_pic.png', 'guardian_core_pic.png']
OURS = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'ours', 'kb')
COLS, CELL, LOGICAL = 6, 256, 128    # PS2 IMGD is 128x128 (HD = 512x512); KH2 draws it ~128 virtual px
R = KH2 + '/kh2_fifth/remastered/itempic/'
_here = os.path.dirname(os.path.abspath(__file__))
_kb = os.path.join(_here, 'sora_kb.h')   # (sorabuild) copy of runtime/sora_kb.h shipped next to this script (a release has no runtime/)
_rt = os.path.join(_here, '..', '..', '..', '..', 'runtime', 'sora_kb.h')   # repo checkout (a release has no runtime/)
if not os.path.isfile(_kb): _kb = _rt
elif os.path.isfile(_rt) and open(_rt, 'rb').read().split() != open(_kb, 'rb').read().split():   # (whitespace-insensitive: CRLF/LF)
    # (s178) dev repo: the shipped copy must not silently win over runtime/sora_kb.h (a new Keyblade would be missing from the bake)
    raise SystemExit('itempic_bake: %s differs from %s - copy runtime/sora_kb.h over tools/sorabuild/hud/py/sora_kb.h and run again' % (_kb, _rt))
src = open(_kb).read()
tab = src[src.index('KB_TAB[] ='):src.index('#define KB_N')]
names = re.findall(r'\{(?:KB_KINGDOM_KEY|KB_STAR_SEEKER|0x[0-9a-f]+), "([^"]+)"', tab)
assert len(names) == len(PICS)
n = len(PICS); rows = (n + COLS - 1) // COLS
W, H = COLS * CELL, rows * CELL
atlas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
sheet = Image.new('RGBA', (COLS * 200, rows * 215), (60, 60, 80, 255)); d = ImageDraw.Draw(sheet)
ents = b''
for i, p in enumerate(PICS):
    im = Image.open(os.path.join(OURS, p) if isinstance(p, str) else R + 'item-%03d.imd/-0.dds' % p).convert('RGBA')
    assert im.size == (512, 512)
    a = im.getchannel('A'); pm = Image.merge('RGBA', [Image.composite(c, Image.new('L', im.size, 0), a) for c in im.split()[:3]] + [a])
    pm = pm.resize((CELL, CELL), Image.LANCZOS)
    px = pm.load()
    for y in range(CELL):
        for x in range(CELL):
            r, g, b, al = px[x, y]
            if al: px[x, y] = (min(255, r * 255 // al), min(255, g * 255 // al), min(255, b * 255 // al), al)
    x0, y0 = (i % COLS) * CELL, (i // COLS) * CELL
    atlas.paste(pm, (x0, y0))
    ents += struct.pack('<6H', x0, y0, CELL, CELL, LOGICAL, LOGICAL)
    sheet.alpha_composite(im.resize((200, 200), Image.LANCZOS), ((i % COLS) * 200, (i // COLS) * 215 + 15))
    d.text(((i % COLS) * 200 + 2, (i // COLS) * 215 + 1), '%d %s %s' % (i, names[i], p if isinstance(p, str) else 'item-%03d' % p), fill='white')
off = 24 + len(ents)
open(sys.argv[1], 'wb').write(  # (sorabuild) out path = argv[1]
    struct.pack('<6I', 0x50494B53, 1, W, H, n, off) + ents + atlas.tobytes())
if len(sys.argv) > 2: sheet.convert('RGB').save(sys.argv[2])
print('itempic.bin', W, H, n, off + W * H * 4)

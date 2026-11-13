# Sora-in-BotW (s55 HUD helper): the text of KH2's level-up panel (dk::LEVEL_UP), vanilla lines only, from the KH2 PC
# message files -> lvup_labels.txt for hudbake `lvup` (one label per line: "<index> <key> <font> <codes hex>").
#   font 1 = KH2's system font kind 1 (drop shadow): the character's name and the level number (re:0x17D9E0 / 0x17D870)
#   font 0 = kind 0 (no shadow): the panel's lines (re:0x16AE30 / 0x17D920)
# KH2 rule (PHH levelup_kh2.py / hud_kh2.py, re:0x3ED7D0): per level, "Strength Increased!" (sys.bar 0xade),
# "Magic Increased!" (0xadf), "Defense Increased!" (0xae0), "Maximum AP Increased!" (0xae1) for each stat that grew, then
# the level's Dream Weapon ability as 0xae8 = "{0d}!" with {0d} -> [09 <the item's icon>] + the item's name; the first
# two lines go on the panel. Name: sys.bar 0x51f "Sora". Level: "%2d" (re:0x5B2660; digit d -> code 0x21 + d (re:0x3ADCC0, the HUD numerals), space 01).
#   python -I tools/kh2_lvup_text.py [--kh2 D:/KH2Extract/kh2_first/original] [--out D:/SoraBotW/data/levels]
# Formats: sys.bar / 03system.bin as tools/kh2_dream_text.py and tools/kh2_levels.py read them (item rows: 24 bytes
# {u16 id, u8 type, f0, f1, rank, u16 stat, u16 name, u16 desc, u16 buy, sell, cmd, slot, pic, u8 icon1, u8 icon2}); the text icon = icon2 (row +23: 3 = ability, 8 = magic; == PHH kh2_hud.json items[].icon).
import importlib.util, json, os, struct, sys

def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path); m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m

dt = load(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'kh2_dream_text.py'), 'dt')  # (sorabuild) was an absolute path
KH2, OUT, LEVELS = None, None, None  # (sorabuild) --kh2 <extract>/kh2_first/original --out <dir> --levels <kh2_sora_levels.json>, all required
a = sys.argv[1:]
while a:
    k = a.pop(0)
    if k == '--kh2': KH2 = a.pop(0)
    elif k == '--out': OUT = a.pop(0)
    elif k == '--levels': LEVELS = a.pop(0)
    else: sys.exit(__doc__)
if not (KH2 and OUT and LEVELS): sys.exit('kh2_lvup_text.py --kh2 <dir> --out <dir> --levels <kh2_sora_levels.json>')

M = {}
for typ, name, blob in dt.bar_entries(open(f'{KH2}/msg/us/sys.bar', 'rb').read()):
    if typ == 2:
        for mid, raw in dt.msgs(blob): M[mid] = raw

def raw(mid):  # the message bytes up to the terminator
    b = M[mid]; return b[:b.index(0)]

sysb = open(f'{KH2}/03system.bin', 'rb').read()
ent = {n: (off, size) for typ, n, blob in [] for off, size in []}
n = struct.unpack_from('<I', sysb, 4)[0]
item_off = None
for k in range(n):
    typ, dup, nm, off, size = struct.unpack_from('<HH4sII', sysb, 16 + 16 * k)
    if nm.rstrip(b'\0') == b'item': item_off = off
ver, cnt = struct.unpack_from('<II', sysb, item_off)
items = {}
for i in range(cnt):
    o = item_off + 8 + 24 * i
    iid, typ, f0, f1, rank, stat, name, desc, buy, sell, cmd, slot, pic, icon1, icon2 = struct.unpack_from('<HBBBBHHHHHHHHBB', sysb, o)
    items[iid] = (name, icon1, icon2, o)

lv = json.load(open(LEVELS))
ab_ids = sorted({int(x) for x in json.load(open(LEVELS))['items_used'].keys()}
                if 'items_used' in lv else [])
# (s76 form level-up) the Drive Forms' level abilities, 00battle.bin fmlv @0x3446C (row = u8 form<<4|level, u8 growth
# level, u16 ability, i32 EXP): Valor 385 Auto Valor / 162 Combo Plus + growth High Jump LV1-3 (94-96), Wisdom 386 Auto
# Wisdom / 412 / 413 + growth Quick Run LV1-3 (98-100). Names = the first word of sys.bar 485 "Valor Form" / 486 "Wisdom Form" (keys fn1/fn2;
# the whole name overran the "LV n" in the name slot, s76 lvuptest).
ab_ids = sorted(set(ab_ids) | {94, 95, 96, 98, 99, 100, 162, 385, 386, 412, 413})
# (s89 b4-master copy) Master: fmlv form 4 = 387 Auto Master / 163 Air Combo Plus + growth Aerial Dodge LV1-3 (102-104),
# name = sys.bar 487 "Master Form" first word (key fn4 = sora_hud's SORA_LVUP_NAME_FORM(4))
ab_ids = sorted(set(ab_ids) | {102, 103, 104, 163, 387})
# (s89 review) Limit: fmlv form 3 = 568 Auto Limit / 405 Draw / 407 Lucky Lucky + growth Dodge Roll LV1-3 (564-566), name =
# sys.bar 20095 "Limit Form" first word (fn3); Final: form 5 = 388 Auto Final / 398 Form Boost + growth Glide LV1-3
# (106-108), name = sys.bar 488 "Final Form" first word (fn5). Item names checked in 03system / sys.bar (s89 review).
ab_ids = sorted(set(ab_ids) | {564, 565, 566, 568, 405, 407, 106, 107, 108, 388, 398})
# (s92 b6-formfx) the 24 Sora Keyblades (sora_kb.h KB_TAB item ids, 03system.bin item type 2) as ab<id> labels "<icon>Name!" for the
# "Keyblade obtained" panel (the level-up panel's ability line, SORA_LVUP_ABILITY(id)); set LVUP_KB=0 for the s89 label set
if os.environ.get('LVUP_KB', '1') != '0':
    ab_ids = sorted(set(ab_ids) | {0x29, 0x1e0, 0x2a, 0x2b, 0x1e1, 0x1e4, 0x1e5, 0x1e6, 0x1e7, 0x1e8, 0x1e9, 0x1ea, 0x1eb, 0x1ec, 0x1ed,
                                   0x1ee, 0x1ef, 0x1f0, 0x1f1, 0x1f2, 0x1f3, 0x1f4, 0x21f, 0x220})
FORM_NAMES = ((1, 485), (2, 486), (3, 20095), (4, 487), (5, 488))
labels, info = [], {}
def add(key, font, codes, src):
    labels.append((key, font, bytes(codes))); info[key] = {'font': font, 'text': dt.decode(bytes(codes)), 'src': src}

add('name', 1, raw(0x51f), 'sys.bar 0x51f')
for f, mid in FORM_NAMES: add(f'fn{f}', 1, raw(mid).split(b'')[0], f'sys.bar {mid} first word (the name slot fits "Sora"-sized text)')
for L in range(1, 100):
    s = f'{L:2d}'; add(f'lv{L:02d}', 1, [0x01 if c == " " else 0x21 + int(c) for c in s], "re:0x5B2660 \"%2d\" -> re:0x3ADCC0 (digit d = code 0x21 + d)")
for key, mid in (('str', 0xade), ('mag', 0xadf), ('def', 0xae0), ('ap', 0xae1)):
    add(key, 0, raw(mid), f'sys.bar {mid:#x}')
tpl = raw(0xae8)
assert tpl[0] == 0x0d, tpl
for iid in ab_ids:
    name, icon1, icon2, o = items[iid]
    nm = None
    for k in (name, name & 0x7fff, name | 0x8000):
        if k in M: nm = raw(k); break
    codes = bytearray()
    for c in tpl:
        codes += bytes([0x09, icon2]) + nm if c == 0x0d else bytes([c])
    add(f'ab{iid}', 0, codes, f'sys.bar 0xae8 + 03system item {iid} @{o:#x} (name {name:#x}, icon {icon2})')
# (s214 kb) our own two Keyblades (runtime/sora_kb.h rows 25 / 26, not KH2 items): the same "<icon><name>!" line with the
# Kingdom Key's item icon and OUR name, spelled in the sys font codes (kh2_dream_text.table: A-Z 0x2e.., a-z 0x9a.., space 0x01)
OURS = ((0x7e01, 'Sealing Blade'), (0x7e02, 'Guardian Core'))
if os.environ.get('LVUP_KB', '1') != '0':
    inv = {chr(65 + i): 0x2e + i for i in range(26)}
    inv.update({chr(97 + i): 0x9a + i for i in range(26)})
    inv[' '] = 0x01
    assert all(dt.T[inv[ch]] == ch for ch in inv if ch != ' ')
    kk_icon = items[0x29][2]
    for iid, nm in OURS:
        assert iid not in items, iid
        codes = bytearray()
        for c in tpl:
            codes += bytes([0x09, kk_icon]) + bytes(inv[ch] for ch in nm) if c == 0x0d else bytes([c])
        add(f'ab{iid}', 0, codes, f'ours (s214): sys.bar 0xae8 + the Kingdom Key icon {kk_icon} + "{nm}"')

os.makedirs(OUT, exist_ok=True)
with open(f'{OUT}/lvup_labels.txt', 'w') as f:
    for i, (k, fo, c) in enumerate(labels): f.write(f'{i} {k} {fo} {c.hex()}\n')
json.dump(info, open(f'{OUT}/lvup_text.json', 'w'), indent=1)
for k in ('fn1', 'fn2', 'fn3', 'fn4', 'fn5') + tuple(f'ab{i}' for i in (94, 98, 102, 103, 104, 106, 107, 108, 162, 163, 385, 386, 387, 388, 398, 405, 407, 412, 413, 564, 565, 566, 568)): print(' ', k, ascii(info[k]['text']), info[k]['src'])
print(f'{len(labels)} labels -> {OUT}/lvup_labels.txt')
for k in ('name', 'lv02', 'lv10', 'str', 'mag', 'def', 'ap') + tuple(f'ab{i}' for i in ab_ids[:3]): print(' ', k, ascii(info[k]['text']), info[k]['src'])

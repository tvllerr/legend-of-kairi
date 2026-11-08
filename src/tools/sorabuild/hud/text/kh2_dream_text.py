# Sora-in-BotW (s54 dream): KH2's own Station of Awakening lines (the Dream Weapon choice) from the KH2 PC message
# files -> data/dream/dream_text.json (the lines with file / entry / message id, decoded text and the raw glyph bytes)
# and data/dream/dream_labels.txt (one label per line: "<id> <key> <glyph bytes in hex>", read by tools/hudbake
# `dream`, which renders them in KH2's sys font into data/hud/dream.bin). Vanilla text only: every label is a line,
# or a word cut out of a line, of the game's own messages; nothing is typed in here except the ids to look for.
#
#   python -I tools/kh2_dream_text.py [--extract D:/KH2Extract] [--out D:/SoraBotW/data/dream]
#   python -I tools/kh2_dream_text.py --grep "power|seek"      (search every US msg entry, print matches)
#
# Formats (OpenKH, D:/KH1Rewrite/references/OpenKh): BAR = "BAR\x01", u32 count, u32 x2, then count x {u16 type,
# u16 dup, char name[4], u32 off, u32 size}; a msg entry (BAR type 2) = u32 1, u32 n, n x {u32 id, u32 off}, the
# strings 0-terminated in the international system encoding (OpenKh.Kh2/Messages/Internals/InternationalSystemDecode.cs);
# the control codes' argument lengths follow OpenKh.Kh2/Msg.cs GetMsgData. Glyph byte b = font cell b - 0x20
# (hudbake's LabelCodes); 0x01 = space (no glyph), 0x02 = new line.
import argparse, glob, json, os, re, struct, sys

ARGS1 = {0x04, 0x06, 0x09, 0x0a, 0x0b, 0x0c, 0x0e, 0x16, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f}
ARGS = {**{c: 1 for c in ARGS1}, 0x12: 2, 0x14: 2, 0x15: 2, 0x18: 2, 0x17: 2, 0x08: 3, 0x07: 4, 0x11: 4, 0x13: 4,
        0x0f: 5, 0x05: 6}

def table():
    t = {0x01: ' ', 0x02: '\n'}
    for i in range(10): t[0x21 + i] = chr(0xFF10 + i); t[0x90 + i] = str(i)
    for i in range(26): t[0x2e + i] = chr(65 + i); t[0x9a + i] = chr(97 + i)
    for k, v in zip(range(0x48, 0x58), "!?%/※、。.,;:…-–〜'"): t[k] = v
    for k, v in zip(range(0x5a, 0x6e), "()「」『』“”[]<>-–⤷♩⇾⇽◯✕"): t[k] = v
    for k, v in zip(range(0x74, 0x7f), ["I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XIII"]): t[k] = v
    for k, v in zip(range(0x7f, 0x8f), ["α", "β", "γ", "⭑", "⭒", "XI", "XII", "&", "#", "®", "▴", "▾", "▸", "◂", "°", "♪"]): t[k] = v
    s = "ÆæßàáâäèéêëìíîïñòóôöùúûüºX»«ÀÁÂÄÈÉÊËÌÍÎÏÑÒÓÔÖÙÚÛÜ¡¿Çç‛’`´\"'★☆■□▲△●○♪♫→←↑↓・❤"
    for i, ch in enumerate(s): t[0xb4 + i] = ch
    t[0xcd] = '—'
    t[0x20], t[0x2b], t[0x2c], t[0x2d], t[0x73] = '⬛', '+', '−', 'ₓ', '\t'
    return t
T = table()

def tokens(b):
    """(kind, bytes) per item: 'g' a glyph / space / new line byte, 'c' a control code with its arguments."""
    out, i = [], 0
    while i < len(b):
        c = b[i]; i += 1
        if c == 0: break
        if c in T: out.append(('g', bytes([c]))); continue
        n = ARGS.get(c, 0)
        out.append(('c', b[i - 1:i + n])); i += n
    return out

def decode(b):
    return ''.join(T[t[0]] if k == 'g' else '{%s}' % ':'.join('%02x' % x for x in t) for k, t in tokens(b))

def plain(s):  # the text without control codes (the line as shown)
    return re.sub(r'\{[0-9a-f:]+\}', '', s)

def bar_entries(data):
    if data[:4] != b'BAR\x01': return []
    n = struct.unpack_from('<I', data, 4)[0]
    out = []
    for k in range(n):
        typ, dup, name, off, size = struct.unpack_from('<HH4sII', data, 16 + 16 * k)
        out.append((typ, name.rstrip(b'\0').decode('latin1'), data[off:off + size]))
    return out

def msgs(blob):
    if len(blob) < 8 or struct.unpack_from('<I', blob, 0)[0] != 1: return []
    n = struct.unpack_from('<I', blob, 4)[0]
    return [(mid, blob[off:]) for mid, off in (struct.unpack_from('<II', blob, 8 + 8 * k) for k in range(n))]

def all_msgs(root):
    for f in sorted(glob.glob(os.path.join(root, 'kh2_*', '*', 'msg', 'us', '*.bar'))):
        if not os.path.isfile(f): continue  # (remastered/*.bar are texture folders)
        for typ, name, blob in bar_entries(open(f, 'rb').read()):
            if typ != 2: continue
            for mid, raw in msgs(blob):
                yield os.path.relpath(f, root).replace('\\', '/'), name, mid, raw

def glyph_lines(raw):
    """The message's display lines as glyph bytes (control codes dropped, split at 0x02, trailing spaces cut)."""
    lines, cur = [], bytearray()
    for k, t in tokens(raw):
        if k == 'c': continue
        if t[0] == 0x02: lines.append(bytes(cur)); cur = bytearray()
        else: cur += t
    lines.append(bytes(cur))
    return [l.rstrip(b'\x01') for l in lines]

def text_of(g): return ''.join(T[c] for c in g)

# The Station of Awakening (KH2's Dive to the Heart, Roxas; Twilight Town messages kh2_first/original/msg/us/tt.bar):
# examine line + question per weapon, the two answers, the "Received the power of ..." line. KH2 pairs: the blade =
# strength = warrior, the shield = protection = guardian, the staff = wisdom = mystic (the question's {15:xx:00} answer
# arg 09 / 0a / 0b = the stat lines 0x45f6 Strength / 0x45f7 Defense / 0x45f8 Magic, in that order).
# Row names: KH2 has no standalone "Sword" / "Shield" / "Staff" message, so the word is cut from a KH2 item name
# (sys.bar: 0x05de "Sword of the Ancestor", 0x05d4 "Knight's Shield", 0x05ce "Mage's Staff") - same glyphs, same case.
TT, SYS = 'kh2_first/original/msg/us/tt.bar', 'kh2_first/original/msg/us/sys.bar'
LABELS = [  # (key, file, msg id, line index or None = all lines, word to cut out or None)
    ('row_sword', SYS, 0x05de, 0, 'Sword'), ('row_shield', SYS, 0x05d4, 0, 'Shield'), ('row_staff', SYS, 0x05ce, 0, 'Staff'),
    ('desc_sword_1', TT, 0x43c0, 0, None), ('desc_sword_2', TT, 0x43c0, 1, None),
    ('desc_shield_1', TT, 0x43c4, 0, None), ('desc_shield_2', TT, 0x43c4, 1, None),
    ('desc_staff_1', TT, 0x43c2, 0, None), ('desc_staff_2', TT, 0x43c2, 1, None),
    ('ask_sword', TT, 0x43c1, 0, None), ('ask_shield', TT, 0x43c5, 0, None), ('ask_staff', TT, 0x43c3, 0, None),
    ('ans_no', TT, 0x43c1, 1, None), ('ans_yes', TT, 0x43c1, 2, None),
    ('got_sword', TT, 0x45fc, 0, None), ('got_shield', TT, 0x45fd, 0, None), ('got_staff', TT, 0x45fe, 0, None),
]
# also recorded (not drawn by the picker): the stat lines after the choice, and the dive's voice lines before it
EXTRA = [('stat_sword', TT, 0x45f6), ('stat_shield', TT, 0x45f7), ('stat_staff', TT, 0x45f8), ('ap_1', TT, 0x45fa),
         ('ap_2', TT, 0x45fb), ('dive_1', TT, 0x45f2), ('dive_2', TT, 0x45f3), ('dive_3', TT, 0x45f4), ('dive_4', TT, 0x45f5)]
# searched and NOT in KH2's US messages (all of kh2_first + kh2_fifth msg/us): "Is this the power you seek?", "The
# power of the warrior / guardian / mystic", any give-up question - those are KH1's Dive to the Heart lines.
NOT_FOUND = [r'power you seek', r'^the power of the (warrior|guardian|mystic)', r'give up the power', r'you.ve chosen',
             r'invincible courage', r'kindness to aid', r'inner strength', r'terrible destruction', r'wonder and ruin']

def main():
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    ap = argparse.ArgumentParser()
    ap.add_argument('--extract', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--grep')
    a = ap.parse_args()
    rows = list(all_msgs(a.extract))
    if a.grep:
        rx = re.compile(a.grep, re.I)
        for f, name, mid, raw in rows:
            s = decode(raw)
            if rx.search(plain(s)): print('%s %s 0x%04x(%d): %r' % (f, name, mid, mid, s))
        return
    by = {(f, mid): raw for f, name, mid, raw in rows}
    labels, out_lines = [], []
    for i, (key, f, mid, li, word) in enumerate(LABELS):
        raw = by[(f, mid)]
        g = glyph_lines(raw)[li]
        if word:
            w = bytes(next(c for c, ch in T.items() if ch == x and c >= 0x20) for x in word)
            assert w in g, (key, text_of(g))
            g = w
        labels.append({'id': i, 'key': key, 'file': f, 'msg_id': '0x%04x' % mid, 'line': li, 'cut': word is not None,
                       'message': decode(raw), 'text': text_of(g), 'glyphs': g.hex()})
        out_lines.append('%d %s %s' % (i, key, g.hex()))
    extra = [{'key': key, 'file': f, 'msg_id': '0x%04x' % mid, 'message': decode(by[(f, mid)])} for key, f, mid in EXTRA]
    missing = {}
    for pat in NOT_FOUND:
        rx = re.compile(pat, re.I | re.M)
        missing[pat] = ['%s 0x%04x' % (f, mid) for f, name, mid, raw in rows if rx.search(plain(decode(raw)))][:5]
    os.makedirs(a.out, exist_ok=True)
    doc = {'source': a.extract + ' (KH2 PC, msg/us, international encoding)', 'tool': 'tools/kh2_dream_text.py (s54 dream)',
           'labels': labels, 'extra': extra, 'searched_not_found': missing,
           'note': 'labels[].id = the label index in data/hud/dream.bin (tools/hudbake dream) = SORA_DREAM_L_* in '
                   'recon/wip_s54_dream/sora_dream.h; glyphs = the message bytes (font cell = byte - 0x20, 01 = space)'}
    json.dump(doc, open(os.path.join(a.out, 'dream_text.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    open(os.path.join(a.out, 'dream_labels.txt'), 'w', newline='\n').write('\n'.join(out_lines) + '\n')
    for l in labels: print('%2d %-14s %s 0x%s  %r' % (l['id'], l['key'], l['file'].split('/')[-1], l['msg_id'][2:], l['text']))
    print('not found in KH2:', {k: v for k, v in missing.items()})

if __name__ == '__main__':
    main()

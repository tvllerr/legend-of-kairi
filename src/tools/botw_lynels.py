# botw_lynels.py: list actors from BotW 1.9.0 map units (Yaz0 + BYML, no deps). Usage: python -I botw_lynels.py <Map/MainField dir> [substr=Lynel]
import os, struct, sys

def yaz0(d):
    assert d[:4] == b'Yaz0'
    n = struct.unpack('>I', d[4:8])[0]
    out = bytearray(); i = 16
    while len(out) < n:
        h = d[i]; i += 1
        for b in range(8):
            if len(out) >= n: break
            if h & (0x80 >> b):
                out.append(d[i]); i += 1
            else:
                b1, b2 = d[i], d[i+1]; i += 2
                dist = ((b1 & 15) << 8 | b2) + 1
                cnt = b1 >> 4
                if cnt == 0: cnt = d[i] + 0x12; i += 1
                else: cnt += 2
                for _ in range(cnt): out.append(out[-dist])
    return bytes(out)

class Byml:
    def __init__(s, d):
        s.d = d
        s.e = '<' if d[:2] == b'YB' else '>'
        _, _, no, so, ro = struct.unpack(s.e + '2sHIII', d[:16])
        s.names = s.strtab(no); s.strs = s.strtab(so); s.root = ro
    def u32(s, o): return struct.unpack_from(s.e + 'I', s.d, o)[0]
    def strtab(s, o):
        if not o: return []
        n = int.from_bytes(s.d[o+1:o+4], 'big' if s.e == '>' else 'little')
        r = []
        for i in range(n):
            a = o + s.u32(o + 4 + 4*i)
            r.append(s.d[a:s.d.index(b'\0', a)].decode('utf8', 'replace'))
        return r
    def val(s, t, v, o_is_inline=True):
        if t == 0xa0: return s.strs[v]
        if t in (0xc0, 0xc1): return s.node(v)
        if t == 0xd0: return bool(v)
        if t == 0xd1: return struct.unpack(s.e + 'i', struct.pack(s.e + 'I', v))[0]
        if t == 0xd2: return struct.unpack(s.e + 'f', struct.pack(s.e + 'I', v))[0]
        if t == 0xd3: return v
        if t == 0xd4: return struct.unpack_from(s.e + 'q', s.d, v)[0]
        if t == 0xd5: return struct.unpack_from(s.e + 'Q', s.d, v)[0]
        if t == 0xd6: return struct.unpack_from(s.e + 'd', s.d, v)[0]
        return None
    def node(s, o):
        t = s.d[o]; n = int.from_bytes(s.d[o+1:o+4], 'big' if s.e == '>' else 'little')
        if t == 0xc0:
            ts = o + 4; vs = ts + ((n + 3) & ~3)
            return [s.val(s.d[ts+i], s.u32(vs + 4*i)) for i in range(n)]
        if t == 0xc1:
            r = {}
            for i in range(n):
                e = o + 4 + 8*i
                raw = s.d[e:e+3]
                ni = int.from_bytes(raw, 'big' if s.e == '>' else 'little')
                r[s.names[ni]] = s.val(s.d[e+3], s.u32(e+4))
            return r
        raise ValueError(hex(t))
    def parse(s): return s.node(s.root)

def main():
    root, pat = sys.argv[1], (sys.argv[2] if len(sys.argv) > 2 else 'Lynel')
    for unit in sorted(os.listdir(root)):
        for kind in ('Dynamic', 'Static'):
            f = os.path.join(root, unit, f'{unit}_{kind}.smubin')
            if not os.path.exists(f): continue
            b = Byml(yaz0(open(f, 'rb').read())).parse()
            for o in b.get('Objs', []):
                nm = o.get('UnitConfigName', '')
                if pat in nm:
                    t = o.get('Translate')
                    print(f"{unit} {kind} {nm} {t[0]:.2f} {t[1]:.2f} {t[2]:.2f} hash={o.get('HashId')} lvl={o.get('!Parameters',{}).get('EnemyLevel') if isinstance(o.get('!Parameters'),dict) else None}")
main()

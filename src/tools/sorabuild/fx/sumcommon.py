# (s137 sumdata) shared readers: baked motion.bin (SKRM v1, see tools/sorabake/Program.cs header), atkp rows of 00battle.bin, 03system cmd rows.
import struct, sys, os
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from kh2bar import bar

KH2 = os.path.join(os.environ["SB_KH2"], "kh2_first/original")

def read_motion(path):
    d = open(path, "rb").read()
    magic, ver, n, _ = struct.unpack_from("<4sIII", d, 0)
    assert magic == b"SKRM", magic
    out = {}
    for i in range(n):
        slot, frames, loop, flags, off_root, off_bits, off_ft, nft, start = struct.unpack_from("<HHHHIIIHH", d, 16 + 24 * i)
        ft = [struct.unpack_from("<HBBhh", d, off_ft + 8 * k) for k in range(nft)]
        root = None
        if flags & 2:
            root = [struct.unpack_from("<4f", d, off_root + 16 * k) for k in range(frames + 1)]
        bits = [struct.unpack_from("<I", d, off_bits + 4 * k)[0] for k in range(frames)]
        out[slot] = dict(frames=frames, loop=loop, flags=flags, start=start, ftrig=ft, root=root, bits=bits)
    return out

_atkp = None
def atkp(i):
    """atkp row id i (00battle.bin 'atkp': u32 version, u32 count, 48 byte rows; id at +2; layout verified on id 1444: Power 25, EnemyReaction 8, EffectOnHit 1)."""
    global _atkp
    if _atkp is None:
        d, e = bar(KH2 + "/00battle.bin")
        x = [x for x in e if x[2] == "atkp"][0]
        base = x[3] + 8
        n = struct.unpack_from("<I", d, x[3] + 4)[0]
        _atkp = {}
        for k in range(n):
            r = d[base + 48 * k: base + 48 * k + 48]
            _atkp.setdefault(struct.unpack_from("<H", r, 2)[0], r)
    r = _atkp.get(i)
    if r is None:
        return None
    return dict(id=i, type=r[4], crit=r[5], power=r[6], team=r[7], element=r[8], reaction=r[10], effect=r[11], kb1=r[12], raw=r.hex())

_cmd = None
def cmd(i):
    """03system.bin cmd row (48 bytes: id u16, execute u8 @2, msg id u16 @8 & 0x7FFF, flags u32 @12, Cost u16 @28, Time u16 @32 [VERIFIED on 447 / 468 / 542 / 548 vs S132])."""
    global _cmd
    if _cmd is None:
        d, e = bar(KH2 + "/03system.bin")
        c = [x for x in e if x[2] == "cmd"][0]
        n = struct.unpack_from("<I", d, c[3] + 4)[0]
        base = c[3] + 8
        _cmd = {}
        for k in range(n):
            r = d[base + 48 * k: base + 48 * k + 48]
            _cmd[struct.unpack_from("<H", r, 0)[0]] = r
    r = _cmd.get(i)
    if r is None:
        return None
    return dict(id=i, execute=r[2], msg=struct.unpack_from("<H", r, 8)[0] & 0x7FFF, flags=struct.unpack_from("<I", r, 12)[0],
                cost=struct.unpack_from("<H", r, 28)[0], time=struct.unpack_from("<H", r, 32)[0])

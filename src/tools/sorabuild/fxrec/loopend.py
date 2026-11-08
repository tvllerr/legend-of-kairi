"""loopend.py - does the script that drives a KH2 effect file ever call effect_loop_end / effect_loop_end_kill?

  python -E -P loopend.py magic/FIRE_1.mag obj/P_EX100.a.us limit/fm/donald.bar
  python -E -P loopend.py --all-oracle      (compare with the PHH kh2_fx.json cache, read only)

Own code. The instruction table (opcode masks, word sizes, which ones jump/call/stop) follows the public format
description in OpenKH's Bdxio (BdxInstructionDescs.cs, Copyright OpenKH Team and Contributors, Apache License 2.0,
http://www.apache.org/licenses/LICENSE-2.0). Trap numbers (group 0: 44 effect_loop_end, 85 effect_loop_end_kill) are
from the KH2 exe notes (re:0x41E190 / re:0x41E1C0).

Format used:
  BAR  : "BAR\\1", u32 count @4, 0x10-byte entries from 0x10: u16 type, u16 dup, char[4] name, u32 off, u32 size.
         type 3 = BDX script.
  BDX  : name[16], i32 work, stack, temp @16/20/24, then (i32 key, i32 addr) pairs from 28 until (0,0).
         Code is 16-bit words; word n lives at byte 16 + 2n. Trigger addrs are word numbers.
  Code : word w. Size by w & mask (see OPS). syscall = (w & 15) == 10, 2 words: trap group = (w >> 6) & 0xFFF,
         second word = trap index (so a call of trap (0,44) is the word pair  [0x0A | 0<<6, 44]).
  Walk : from each trigger addr, linear; b/beqz/bnez/jal/jal32 follow the relative target
         (target = at + size + signed offset); stop on b, exit, ret, unknown opcode.
  Reveal: like the reference (OpenKH `decode -r`), words below the text pool that no trigger path reached are also
         tried as code starts, lowest first. The text pool = the trailing run of words that are all printable ASCII
         (or 0) bytes. Reveal matters for subroutines only reached through pushed addresses (push.bd / push).
"""
import json
import os
import struct
import sys

KH2 = os.environ.get("FXREC_KH2", "D:/KH2Extract")
PKGS = ["kh2_first", "kh2_second", "kh2_third", "kh2_fourth", "kh2_fifth", "kh2_sixth"]
BAR_BDX = 3
LOOP_END_TRAPS = {(0, 44): "effect_loop_end", (0, 85): "effect_loop_end_kill"}
ORACLE = os.path.join(os.environ.get("LOCALAPPDATA", ""), "ProjectHeartHunters", "cache", "kh2_fx.json")  # (dev-only oracle; s182: no user path in the release)

# (value, mask, words, kind)  kind: None / 'jmp' / 'cond' / 'call' / 'stop' / 'sys'
# FindByCode semantics: first match in this order.
OPS = [
    (0, 63, 3, None), (16, 63, 3, None),
    (32, 0xFFFF, 2, None), (96, 0xFFFF, 2, None), (160, 0xFFFF, 2, None), (224, 0xFFFF, 2, None),
    (48, 0xFFFF, 2, None), (112, 0xFFFF, 2, None), (176, 0xFFFF, 2, None), (240, 0xFFFF, 2, None),
    (1, 0xFFCF, 2, None), (65, 0xFFCF, 2, None), (129, 0xFFCF, 2, None), (193, 0xFFCF, 2, None),
    (2, 0xFFCF, 3, None), (66, 0xFFCF, 3, None), (130, 0xFFCF, 3, None), (194, 0xFFCF, 3, None),
    (3, 15, 2, None), (4, 15, 1, None),
]
# one-word ALU ops with full-word match
for _v in (5, 133, 197, 261, 325, 389, 453, 517, 581, 645, 709, 85, 149, 341, 405, 469, 533, 597, 661, 725,
           6, 70, 134, 198, 262, 326, 390, 454, 518, 582, 646, 710, 22, 86, 150, 214, 278):
    OPS.append((_v, 0xFFFF, 1, None))
OPS += [
    (7, 0xFFCF, 2, "jmp"), (71, 0xFFCF, 2, "cond"), (135, 0xFFCF, 2, "cond"),
    (8, 15, 2, "call"),
    (9, 0xFFCF, 1, None), (73, 0xFFCF, 1, "stop"), (137, 0xFFCF, 1, "stop"),
    (201, 0xFFCF, 1, None), (329, 0xFFCF, 1, None), (393, 0xFFCF, 1, None), (457, 0xFFCF, 1, None),
    (521, 0xFFCF, 1, None), (585, 0xFFCF, 1, None),
    (10, 15, 2, "sys"), (11, 15, 3, "call32"),
]


def find_op(w):
    for v, m, n, k in OPS:
        if w & m == v:
            return n, k
    return None


def s16(x):
    return x - 0x10000 if x & 0x8000 else x


def bdx_calls(blob, reveal=True):
    """Return the set of loop-end trap names found at proven instruction starts."""
    if len(blob) < 32:
        return set()
    nw = (len(blob) - 16) // 2
    words = struct.unpack_from("<%dH" % nw, blob, 16)

    def w(i):
        return words[i] if 0 <= i < nw else None

    # triggers
    trig = []
    pos = 28
    while pos + 8 <= len(blob):
        k, a = struct.unpack_from("<ii", blob, pos)
        pos += 8
        if k == 0 and a == 0:
            break
        trig.append(a)
    first = (12 + 8 * (len(trig) + 1)) // 2
    start = {}  # word index -> (size, kind) for proven instruction starts
    covered = set()  # word indices that are the operand part of a proven instruction
    found = set()

    def walk(entry):
        work = [entry]
        while work:
            e = work.pop()
            while True:
                if e in start or e in covered or e < 0 or e >= nw:
                    break
                op = find_op(words[e])
                if op is None:
                    break
                n, kind = op
                if e + n > nw:
                    break
                if any((e + k) in start or (e + k) in covered for k in range(1, n)):
                    break
                start[e] = (n, kind)
                for k in range(1, n):
                    covered.add(e + k)
                if kind in ("jmp", "cond", "call"):
                    work.append((e + n + s16(words[e + 1])) & 0xFFFF)
                elif kind == "call32":
                    work.append((e + n + struct.unpack_from("<i", blob, 16 + 2 * (e + 1))[0]) & 0xFFFF)
                elif kind == "sys":
                    key = ((words[e] >> 6) & 0xFFF, words[e + 1])
                    if key in LOOP_END_TRAPS:
                        found.add(LOOP_END_TRAPS[key])
                if kind in ("jmp", "stop"):
                    break
                e += n

    for a in trig:
        walk(a)
    if reveal:
        # text pool: trailing run of words whose two bytes are 0 or printable
        def ok(b):
            return b == 0 or 32 <= b <= 126
        pool = nw
        for i in range(nw - 1, first - 1, -1):
            if ok(words[i] & 255) and ok(words[i] >> 8):
                pool = i
            else:
                break
        for i in range(first, min(pool, nw - 8)):
            if i not in start and i not in covered:
                walk(i)
    return found


def locate(name):
    hit = None
    for p in PKGS:
        f = "%s/%s/original/%s" % (KH2, p, name)
        if os.path.isfile(f):
            hit = f
    return hit


def bar_entries(data):
    if data[:3] != b"BAR" or len(data) < 16:
        return []
    cnt = struct.unpack_from("<I", data, 4)[0]
    out = []
    for i in range(cnt):
        o = 0x10 + 0x10 * i
        if o + 16 > len(data):
            break
        t, _d = struct.unpack_from("<HH", data, o)
        nm = data[o + 4:o + 8].split(b"\0")[0].decode("latin1")
        off, sz = struct.unpack_from("<II", data, o + 8)
        out.append((t, nm, data[off:off + sz]))
    return out


def loop_end(name, reveal=True):
    """-> (bool, [entry names that call it])"""
    src = name[:-5] + ".mdlx" if name.endswith(".a.us") else name
    f = locate(src)
    if not f:
        return False, []
    with open(f, "rb") as fh:
        data = fh.read()
    hits = []
    for t, nm, blob in bar_entries(data):
        if t == BAR_BDX and bdx_calls(blob, reveal):
            hits.append(nm)
    return bool(hits), hits


def oracle():
    with open(ORACLE, "r", encoding="utf-8") as fh:
        eff = json.load(fh)["effects"]
    files = {}
    for k, v in eff.items():
        d = files.setdefault(v["file"], {"tails": 0, "max600": False, "n": 0})
        d["n"] += 1
        if "~" in k:
            d["tails"] += 1
        if v.get("frames") == 600:
            d["max600"] = True
    return files


def all_oracle():
    files = oracle()
    match = 0
    for name in sorted(files):
        d = files[name]
        got, hits = loop_end(name)
        got0, _ = loop_end(name, reveal=False)
        ref = d["tails"] > 0
        if got == ref:
            tag, match = "ok", match + 1
        elif got and not ref and d["max600"]:
            tag, match = "ok-no-tails(600-frame element present but no tail recorded)", match + 1
        elif got and not ref:
            tag = "EXPLAINED? script calls it, no element reached 600 frames"
            match += 1
        else:
            tag = "MISMATCH"
        print("%-32s ref=%d ours=%d (noreveal=%d) %s %s" % (name, ref, got, got0, ",".join(hits), tag))
    print("MATCH %d/%d" % (match, len(files)))


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    if argv[0] == "--all-oracle":
        all_oracle()
        return 0
    for n in argv:
        got, hits = loop_end(n)
        print("%s loop_end=%d (%s)" % (n, int(got), ",".join(hits)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

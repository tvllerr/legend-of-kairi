"""(s223 rikufx) host check of the stand-ins' own weapon effects: the DLL's table (runtime/sora_pfx.h + sora_pchar.h) against the pack
partyfx.py built and the stand-ins' hit bakes. Read only.

  python -I party_pfx_check.py <worktree> <old fx.bin> <new fx.bin> <party data dir: <key>_hits.bin>

Checks: partyfx.py ROWS use sora_pchar.h's PCH ids and sora_pfx.h's keys; the old pack has no pfx_* name and every old name keeps its
index in the new one; every pfx_* name the DLL asks for a stand-in's bake (each EffectOnHit his windows carry + the ribbon) is in the
new pack; every trail-table key the new pack adds is 200000 + 1000 * PCH + slot of a row; Riku's hit windows all lie in his trail
windows (A300 / A301 / A330 / A331)."""
import os
import re
import struct
import sys

W, OLD, NEW, PDIR = sys.argv[1:5]
ok = bad = 0


def check(c, what):
    global ok, bad
    if c:
        ok += 1
    else:
        bad += 1
        print("  FAIL", what)


def pack(path):
    with open(path, "rb") as f:
        h = struct.unpack("<4s15I", f.read(64))
        f.seek(h[9])
        eff = f.read(64 * h[3])
        f.seek(h[13])
        tr = f.read(12 * h[7])
    names = [eff[64 * i:64 * i + 32].split(b"\0")[0].decode("latin1") for i in range(h[3])]
    return names, [struct.unpack_from("<III", tr, 12 * i) for i in range(h[7])]


def hits(path):
    b = open(path, "rb").read()
    n = struct.unpack_from("<I", b, 8)[0]
    out = {}
    for i in range(n):
        s, nw, ns, _, ow, os_ = struct.unpack_from("<4H2I", b, 16 + 16 * i)
        out[s] = [struct.unpack_from("<6H2B2x", b, ow + 16 * k) for k in range(nw)]
    return out


src = open(os.path.join(W, "runtime/sora_pchar.h"), encoding="utf-8").read()
enum = re.search(r"enum \{ (PCH_DONALD[^}]*)\}", src).group(1)
pch = {n.strip(): i for i, n in enumerate(x for x in enum.split(",") if x.strip() and x.strip() != "PCH_N")}
pfx = open(os.path.join(W, "runtime/sora_pfx.h"), encoding="utf-8").read()
dll_rows = re.findall(r"\{(PCH_\w+), \"(\w+)\"\}", pfx)
sys.path.insert(0, os.path.join(W, "tools/sorabuild/fx"))
os.environ.setdefault("SB_FXCACHE", ".")
os.environ.setdefault("SB_TRIGDIR", ".")
import partyfx  # noqa: E402
tool_rows = [(k, p) for k, p, *_ in partyfx.ROWS]
check(len(dll_rows) == len(tool_rows) == 5, "5 rows in sora_pfx.h and partyfx.py")
for (pn, key), (tkey, tp) in zip(dll_rows, tool_rows):
    check(key == tkey and pch[pn] == tp, "row %s: DLL %s=%d, tool %s=%d" % (key, pn, pch[pn], tkey, tp))
check(partyfx.TRAIL_BASE == int(re.search(r"#define PFX_TRAIL_BASE (\d+)", pfx).group(1)), "trail base DLL = tool")

on, otr = pack(OLD)
nn, ntr = pack(NEW)
check(not any(n.startswith("pfx_") for n in on), "the old pack has no pfx_* rows")
check(nn[:len(on)] == on, "every old name keeps its index (%d)" % len(on))
check(ntr[:len(otr)] == otr, "every old trail window kept (%d)" % len(otr))
added = set(nn[len(on):])
check(all(n.startswith("pfx_") for n in added), "only pfx_* rows added")
keys = {(partyfx.TRAIL_BASE + 1000 * p) // 1000: k for k, p in tool_rows}
for slot, s, e in ntr[len(otr):]:
    check(slot // 1000 in keys and slot % 1000 < 1000, "new trail key %d belongs to a row" % slot)
for key, p in tool_rows:
    hb = hits(os.path.join(PDIR, key + "_hits.bin"))
    eohs = sorted({w[7] for ws in hb.values() for w in ws if w[4] > 0})
    for k in eohs:
        check("pfx_%s_hit%d" % (key, k) in added, "%s: EffectOnHit %d of his windows has pfx_%s_hit%d" % (key, k, key, k))
    check("pfx_%s_trail" % key in added, "%s: ribbon pfx_%s_trail" % (key, key))
    tws = [t for t in ntr[len(otr):] if t[0] // 1000 == (partyfx.TRAIL_BASE + 1000 * p) // 1000]
    atk = [s for s in hb if any(w[4] > 0 for w in hb[s])]
    check(all(any(t[0] % 1000 == s for t in tws) for s in atk), "%s: every attack slot %s has trail windows" % (key, atk))
    print("  %-8s EffectOnHit %s, %d trail windows, attack slots %s" % (key, eohs, len(tws), atk))
    if key == "riku":
        for s, ws in hb.items():
            for w in ws:
                inside = any(t[0] % 1000 == s and t[1] <= w[0] and (t[2] == 0xFFFFFFFF or w[1] <= t[2]) for t in tws)
                check(inside, "riku slot %d hit window %d..%d inside a trail window" % (s, w[0], w[1]))
print("%s: %d / %d checks passed" % ("OK" if not bad else "FAILED", ok, ok + bad))
sys.exit(1 if bad else 0)

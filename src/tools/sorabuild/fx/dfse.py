#!/usr/bin/env python3
"""(s150 Duck Flare) APPENDS the Duck Flare sounds to the LIVE sfx.bin (SKSF v1; same rules as ksse.py: old rows / PCM stay byte-identical,
new sound rows + names are appended; no trigger rows).
NAMES (exact; the code helper plays them with sora_sfx_id(name)):
  df_se999_125 .. df_se999_130   se/se999.win32.scd#125..130   (the PAX element SEs of donald2.bar e0..e5; new sounds)
  donald_megaduck, donald_megaduck.0   voice/us/battle/hb0_donald.win32.scd#18 (vtbl 16, Megaduck) - only when no existing name already holds that key
  dfse_v1                        marker (-> df_se999_125's sound)
hb0_donald #10 / #11 (vtbl 4, Rocket Flare) are ALREADY held by donald_attack.0 / .1 (checked, not added again).
  python -I dfse.py plan  [--sfx-in LIVE]
  python -I dfse.py build [--sfx-in LIVE] --out OUTDIR      writes OUTDIR/sfx.bin (never touches the live file); no output + exit 0 when dfse_v1 exists"""
import argparse, json, os, struct, sys, wave
import sys as _sys, os as _os; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__))); import sndsrc  # (s156) sound inputs: recipe tree or PHH cache
CACHE = os.environ["SB_FXCACHE"] if "SB_FXCACHE" in os.environ else os.environ["SB_PHH"]  # fx cache dir (SB_FXCACHE; SB_PHH = old name), passed in by sorabuild (ctx.fxcache)
SK = struct.Struct("<4s11I")
MARK = "dfse_v1"
DONALD18 = "voice/us/battle/hb0_donald.win32.scd#18"
WANT = [("df_se999_%d" % n, "se/se999.win32.scd#%d" % n) for n in range(125, 131)]


def build(src, dst, plan_only=False):
    d = open(src, "rb").read()
    magic, ver, ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes, _ = SK.unpack_from(d, 0)
    assert magic == b"SKSF" and ver == 1
    S = json.load(open(sndsrc.sounds_json(), encoding="utf-8"))["sounds"]
    o = 48
    sec = {}
    for nm, cnt, sz in (("sounds", ns, 72), ("trig", nt, 28), ("ts", nts, 4), ("vt", nv, 16), ("ch", nc, 8), ("names", nn, 36), ("hits", nh, 8)):
        sec[nm] = d[o:o + cnt * sz]
        o += cnt * sz
    pcm = bytearray(d[data_off:data_off + data_bytes])
    keys = [sec["sounds"][i * 72:i * 72 + 48].split(b"\x00")[0].decode() for i in range(ns)]
    kidx = {k: i for i, k in enumerate(keys)}
    names = [(sec["names"][i * 36:i * 36 + 32].split(b"\x00")[0].decode(), struct.unpack_from("<I", sec["names"], i * 36 + 32)[0]) for i in range(nn)]
    nameset = {n for n, _ in names}
    if MARK in nameset:
        print("already installed (marker %s): nothing to do" % MARK)
        return None
    new_s = bytearray()
    new_keys = []
    new_names = []

    def want(key):
        if key in kidx:
            return kidx[key], False
        s = S.get(key)
        assert s, "no such cue in the mined table: " + key
        path = sndsrc.wavfile(s)
        assert os.path.exists(path), path
        with wave.open(path, "rb") as w:
            assert w.getsampwidth() == 2
            rate, ch, n, fr = w.getframerate(), w.getnchannels(), w.getnframes(), w.readframes(w.getnframes())
        while len(pcm) % 4:
            pcm.append(0)
        new_s.extend(struct.pack("<48sIIfIII", key.encode()[:47], rate, ch, float(s.get("volume", 1.0)), 1 if s.get("loop") else 0, len(pcm), n))
        pcm.extend(fr)
        kidx[key] = ns + len(new_keys)
        new_keys.append(key)
        print("  + sound %-44s %d Hz %d ch %6d frames (%.2f s) vol %.2f" % (key, rate, ch, n, n / rate, s.get("volume", 1.0)))
        return kidx[key], True

    for nm, key in WANT:
        assert nm not in nameset, "name %s exists without the marker" % nm
        i, new = want(key)
        new_names.append((nm, i))
        if not new:
            print("  = %-14s reuses existing sound %d (%s), held by: %s" % (nm, i, key, ",".join(n for n, s in names if s == i) or "(no name)"))
    holders = [n for n, s in names if s == kidx.get(DONALD18, -1)]
    if holders:
        print("  donald_megaduck NOT added: hb0_donald#18 is already held by", holders)
    else:
        i, _ = want(DONALD18)
        for nm in ("donald_megaduck", "donald_megaduck.0"):
            assert nm not in nameset, nm
            new_names.append((nm, i))
    new_names.append((MARK, kidx[WANT[0][1]]))
    print("sounds +%d (%.2f MB PCM16), names +%d" % (len(new_keys), (len(pcm) - data_bytes) / 1e6, len(new_names)))
    if plan_only:
        return None
    body = bytearray(sec["sounds"] + bytes(new_s) + sec["trig"] + sec["ts"] + sec["vt"] + sec["ch"] + sec["names"] +
                     b"".join(struct.pack("<32sI", n.encode(), s) for n, s in new_names) + sec["hits"])
    while (48 + len(body)) % 16:
        body.append(0)
    hdr = SK.pack(b"SKSF", 1, ns + len(new_keys), nt, nts, nv, nc, nn + len(new_names), nh, 48 + len(body), len(pcm), 0)
    tmp = dst + ".tmp"
    open(tmp, "wb").write(hdr + body + pcm)
    os.replace(tmp, dst)
    n = open(dst, "rb").read()
    m2, v2, ns_, nt_, nts_, nv_, nc_, nn_, nh_, do2, db2, _ = SK.unpack_from(n, 0)
    assert (ns_, nt_, nts_, nv_, nc_, nn_, nh_) == (ns + len(new_keys), nt, nts, nv, nc, nn + len(new_names), nh)
    assert n[48:48 + 72 * ns] == sec["sounds"] and n[do2:do2 + data_bytes] == d[data_off:data_off + data_bytes]
    base = 48 + 72 * ns_
    assert n[base:base + 28 * nt + 4 * nts + 16 * nv + 8 * nc + 36 * nn] == sec["trig"] + sec["ts"] + sec["vt"] + sec["ch"] + sec["names"], "old tables changed"
    for k, (nm, s) in enumerate(new_names):  # the new names resolve (sora_sfx_id order: names first)
        r = base + 28 * nt + 4 * nts + 16 * nv + 8 * nc + 36 * (nn + k)
        assert n[r:r + 32].split(b"\0")[0].decode() == nm and struct.unpack_from("<I", n, r + 32)[0] == s < ns_
    print("VERIFIED: old sound rows, PCM, trigger/vtbl/name tables identical; %d new names resolve; %d bytes (was %d)" % (len(new_names), len(n), len(d)))
    return dst


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["plan", "build"])
    ap.add_argument("--sfx-in", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    build(a.sfx_in, os.path.join(a.out, "sfx.bin"), a.cmd == "plan")

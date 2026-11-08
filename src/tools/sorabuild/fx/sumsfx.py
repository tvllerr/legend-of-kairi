#!/usr/bin/env python3
"""(s137 sumdata) APPENDS Peter Pan (pp_*), Tinker Bell (tb_* triggers only) and Genie (gn_*) sounds, voice lines and mset sound triggers to the LIVE sfx.bin.
Format and rules = chickfx.py build_sfx (SKSF v1): old sounds / triggers / names / PCM stay byte-identical, new rows are appended, the trigger
table is re-sorted by slot (stable) because several summons append their own slot ranges.

  python -I sumsfx.py plan  [--sfx-in LIVE]
  python -I sumsfx.py build [--sfx-in LIVE] --out OUTDIR      writes OUTDIR/sfx.bin (never touches the live file)

SOUNDS     pp_se<N>  = se/se595.win32.scd#N (the whole bank: Peter's own, N = 32..47)       pp_v<N> = voice/us/battle/peterpan.win32.scd#N (all 9)
           gn_se<N>  = se/se597.win32.scd#N (the whole bank, 31)                            gn_v<N> = voice/us/battle/genie.win32.scd#N (all 6)
           Tinker Bell has no sound bank of her own (VERIFIED: no tinker voice or bank in the mined sound table); her motion triggers play se595 #46 / #47.
           Footsteps (id 2) point at se705 / existing keys; Sora's own voice lines (id 14) at existing hb0_sora keys (GUESS: p1 = line index, see the report).
TRIGGERS   sfx slot = BASE + mset slot, BASE: Peter 95000, Tinker Bell 96000, Genie 97000 (Chicken Little 90000; the Stitch helper picks its own, keep it < 95000)
           every frame trigger of the baked motions (body + Sora's call/limit motions that live in the summon's mset): id 8 play_sound (p1 bank, p2 index),
           id 13 voice of the summon, id 14 voice of Sora (GUESS), id 2 footstep.   Marker names: ppse_v1, tbse_v1, gnse_v1 (idempotence)."""
import argparse, json, os, struct, sys, wave
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sumcommon as SC
import sys as _sys, os as _os; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__))); import sndsrc  # (s156) sound inputs: recipe tree or PHH cache
CACHE = os.environ["SB_FXCACHE"] if "SB_FXCACHE" in os.environ else os.environ["SB_PHH"]  # fx cache dir (SB_FXCACHE; SB_PHH = old name), passed in by sorabuild (ctx.fxcache)
SK = struct.Struct("<4s11I")
OUT = os.environ["SB_SUMDATA"]  # dir with <key>/<key>_motion.bin + <key>/<key>call_motion.bin (baked summon motions, staged by sorabuild)
CALLSLOTS = {"peterpan": [252, 253, 255, 256], "tink": [252], "genie": [252, 253, 255, 257, 259, 261, 263, 265, 267]}
SUMMONS = [
    dict(key="peterpan", pre="pp", base=95000, marker="ppse_v1", bank=595, voice="peterpan"),
    dict(key="tink", pre="tb", base=96000, marker="tbse_v1", bank=None, voice=None),
    dict(key="genie", pre="gn", base=97000, marker="gnse_v1", bank=597, voice="genie"),
]


def triggers(key):
    """[(mset slot, frame, id, p1, p2, is_call)] from the baked motion bins (out_s137sumdata/<key>/)."""
    out = []
    body = SC.read_motion(os.path.join(OUT, key, key + "_motion.bin"))
    call = SC.read_motion(os.path.join(OUT, key, key + "call_motion.bin"))
    for slot, m in body.items():
        if slot in CALLSLOTS[key]:
            continue  # the body bake of a Sora-call slot number is garbage on the wrong skeleton
        for fr, tid, npar, p1, p2 in m["ftrig"]:
            out.append((slot, fr, tid, p1, p2, False))
    for slot in CALLSLOTS[key]:
        for fr, tid, npar, p1, p2 in call[slot]["ftrig"]:
            out.append((slot, fr, tid, p1, p2, True))
    out.sort(key=lambda t: (t[5], t[0], t[1]))
    return out


def build(src, dst, plan_only=False):
    d = open(src, "rb").read()
    magic, ver, ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes, _ = SK.unpack_from(d, 0)
    assert magic == b"SKSF" and ver == 1
    sj = json.load(open(sndsrc.sounds_json(), encoding="utf-8"))
    S = sj["sounds"]
    o = 48
    sec = {}
    for nm, cnt, sz in (("sounds", ns, 72), ("trig", nt, 28), ("ts", nts, 4), ("vt", nv, 16), ("ch", nc, 8), ("names", nn, 36), ("hits", nh, 8)):
        sec[nm] = d[o:o + cnt * sz]
        o += cnt * sz
    pcm = bytearray(d[data_off:data_off + data_bytes])
    keys = [sec["sounds"][i * 72:i * 72 + 48].split(b"\x00")[0].decode() for i in range(ns)]
    kidx = {k: i for i, k in enumerate(keys)}
    names = {sec["names"][i * 36:i * 36 + 32].split(b"\x00")[0].decode() for i in range(nn)}
    fkind = {}
    for a in sj["actors"].values():
        for sl in a["slots"].values():
            for t in sl.get("triggers", []):
                if t["type"] == "footstep_sound" and t.get("sounds"):
                    fkind.setdefault(int(t["footstep"]), t["sounds"])
    new_s = bytearray()
    new_keys = []
    new_names = []
    maxold = max(struct.unpack_from("<I", sec["trig"], i * 28)[0] for i in range(nt))
    miss = set()
    rows = []
    tsnd = []
    info = []

    def want(key):
        if key in kidx:
            return kidx[key]
        s = S.get(key)
        if not s:
            return -1
        path = sndsrc.wavfile(s)
        if not os.path.exists(path):
            return -1
        with wave.open(path, "rb") as w:
            assert w.getsampwidth() == 2
            rate, ch, n, fr = w.getframerate(), w.getnchannels(), w.getnframes(), w.readframes(w.getnframes())
        while len(pcm) % 4:
            pcm.append(0)
        new_s.extend(struct.pack("<48sIIfIII", key.encode()[:47], rate, ch, float(s.get("volume", 1.0)), 1 if s.get("loop") else 0, len(pcm), n))
        pcm.extend(fr)
        kidx[key] = ns + len(new_keys)
        new_keys.append(key)
        return kidx[key]

    done = []
    for sm in SUMMONS:
        if sm["marker"] in names:
            print(sm["key"], "already installed (marker %s): skipped" % sm["marker"])
            continue
        pre = sm["pre"]
        first = len(new_names)
        if sm["bank"]:
            for n in range(0, 64):
                k = "se/se%d.win32.scd#%d" % (sm["bank"], n)
                if k in S:
                    i = want(k)
                    if i >= 0:
                        new_names.append(("%s_se%d" % (pre, n), i))
            for n in range(0, 64):
                k = "voice/us/battle/%s.win32.scd#%d" % (sm["voice"], n)
                if k in S:
                    i = want(k)
                    if i >= 0:
                        new_names.append(("%s_v%d" % (pre, n), i))
        for slot, frame, tid, p1, p2, iscall in triggers(sm["key"]):
            sl = sm["base"] + slot

            def emit(ids, kind_flag, nm):
                rows.append((sl, frame, 0, 0, kind_flag, len(ids), len(tsnd) + nts))
                tsnd.extend(ids)
                info.append((sm["key"], slot, frame, tid, nm))
            if tid == 8:
                k = "se/se%03d.win32.scd#%d" % (p1, p2)
                i = want(k)
                if i < 0:
                    miss.add(k)
                    continue
                emit([i], 0, k)
            elif tid == 13:
                k = "voice/us/battle/%s.win32.scd#%d" % (sm["voice"], p1)
                i = want(k) if sm["voice"] else -1
                if i < 0:
                    miss.add(k)
                    continue
                emit([i], 0, k)
            elif tid == 14:   # Sora's own voice line (GUESS: p1 = line index of his voice bank)
                k = "voice/us/battle/hb0_sora.win32.scd#%d" % p1
                i = want(k)
                if i < 0:
                    miss.add(k + " (Sora voice, id 14)")
                    continue
                emit([i], 0, k + " (GUESS)")
            elif tid == 2:
                ids = [i for i in (want(k) for k in fkind.get(p1, [])) if i >= 0]
                if not ids:
                    miss.add("footstep %d" % p1)
                    continue
                emit(ids, 1, "footstep %d" % p1)
        done.append(sm["key"])
        new_names.append((sm["marker"], new_names[first][1] if len(new_names) > first else 0))
    assert not rows or min(r[0] for r in rows) > maxold, "a new slot range overlaps an old one (max old slot %d)" % maxold
    print("sounds +%d (%.1f MB PCM16), triggers +%d, trigger sounds +%d, names +%d; missing: %s" %
          (len(new_keys), (len(pcm) - data_bytes) / 1e6, len(rows), len(tsnd), len(new_names), sorted(miss) or "none"))
    if not done:
        print("nothing to do")
        return info, None
    trig_old = [sec["trig"][i * 28:(i + 1) * 28] for i in range(nt)]
    allr = trig_old + [struct.pack("<7I", *r) for r in rows]
    allr.sort(key=lambda b: struct.unpack_from("<I", b, 0)[0])    # stable
    body = bytearray(sec["sounds"] + bytes(new_s) + b"".join(allr) + sec["ts"] + b"".join(struct.pack("<I", i) for i in tsnd) + sec["vt"] + sec["ch"] +
                     sec["names"] + b"".join(struct.pack("<32sI", n.encode(), s) for n, s in new_names) + sec["hits"])
    while (48 + len(body)) % 16:
        body.append(0)
    hdr = SK.pack(b"SKSF", 1, ns + len(new_keys), nt + len(rows), nts + len(tsnd), nv, nc, nn + len(new_names), nh, 48 + len(body), len(pcm), 0)
    if plan_only:
        return info, None
    tmp = dst + ".tmp"
    open(tmp, "wb").write(hdr + body + pcm)
    os.replace(tmp, dst)
    n = open(dst, "rb").read()
    m2, v2, ns_, nt_, nts_, nv_, nc_, nn_, nh_, do2, db2, _ = SK.unpack_from(n, 0)
    assert n[48:48 + 72 * ns] == sec["sounds"] and n[do2:do2 + data_bytes] == d[data_off:data_off + data_bytes]
    o2 = 48 + 72 * ns_
    got = [n[o2 + 28 * i:o2 + 28 * i + 28] for i in range(nt_)]
    olds = set(trig_old)
    assert [g for g in got if g in olds] == sorted(trig_old, key=lambda b: struct.unpack_from("<I", b, 0)[0]), "old trigger rows changed"
    assert all(struct.unpack_from("<I", got[i], 0)[0] <= struct.unpack_from("<I", got[i + 1], 0)[0] for i in range(nt_ - 1))
    print("VERIFIED: old sound rows + PCM identical, old trigger rows present, triggers sorted; %d bytes (was %d)" % (len(n), len(d)))
    return info, dst


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["plan", "build"])
    ap.add_argument("--sfx-in", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    info, dst = build(a.sfx_in, os.path.join(a.out, "sfx.bin"), a.cmd == "plan")
    if a.cmd == "plan":
        for r in info:
            print("  ", r)

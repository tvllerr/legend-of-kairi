#!/usr/bin/env python3
"""s110m mickeyfx: append KH2 Mickey's (P_EX200.mset + W_EX200.mset) sound triggers to an EXISTING sfx.bin (SKSF v1) without
rebuilding it: every older sound index / trigger / vtbl / choice / name / hit row stays byte-identical (checked at the end).

Convention (as tools_s110/roxsfx/sfxroxas.py): triggers at slot + 10000 * (form + 1); Mickey = FORM_MICKEY 7 -> slot + 80000 (sora.c sfx_slot()).
His voice rows get the vtbl id 8000 + KH2 vtbl (Roxas 7000+, Sora raw). Marker name `mkse_v1`.

Inputs
  trig_P_EX110.json / trig_W_EX010_RX.json   from `sorabake trigjson` (frame triggers: id, frame, nparam, p1, p2):
      id 2 footstep (p1 = 12soundinfo footstep kind), id 8 play_sound (p1 bank, p2 index -> se<bank>#<index>),
      id 14 vsb_voice2 (p1 = KH2 vtbl).  Same decode as PHH's kh2_sounds.kh.json for Sora (checked on slots 6 / 604 / 608 / 724 / 14 / 42).
  PHH kh2_sounds.kh.json   sounds (key -> wav), footstep kind -> sound lists (from any actor), actor 20 (Roxas) vtbl table
  PHH kh2_sfx/<wav>        the PCM16 files
Usage: python -I sfxroxas.py <in sfx.bin> <out sfx.bin> [trigdir]
"""
import json, os, struct, sys, wave
import sys as _sys, os as _os; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__))); import sndsrc  # (s156) sound inputs: recipe tree or PHH cache

CACHE = os.environ["SB_FXCACHE"] if "SB_FXCACHE" in os.environ else os.environ["SB_PHH"]  # fx cache dir (SB_FXCACHE; SB_PHH = old name), passed in by sorabuild (ctx.fxcache)
FORM_BASE = 80000
VTBL_BASE = 8000


def load(path):
    d = open(path, "rb").read()
    magic, ver, ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes, _ = struct.unpack_from("<4s11I", d, 0)
    assert magic == b"SKSF" and ver == 1
    return d, (ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    tdir = sys.argv[3]
    if os.path.abspath(src) == os.path.abspath(dst):
        sys.exit("refusing to overwrite the input")
    d, (ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes) = load(src)
    sj = json.load(open(sndsrc.sounds_json(), encoding="utf-8"))
    sounds_js = sj["sounds"]
    P = json.load(open(os.path.join(tdir, "trig_P_EX200.json")))
    W = json.load(open(os.path.join(tdir, "trig_W_EX200.json")))

    o = 48
    sec_sounds = d[o:o + 72 * ns]; o += 72 * ns
    sec_trig = d[o:o + 28 * nt]; o += 28 * nt
    sec_ts = d[o:o + 4 * nts]; o += 4 * nts
    sec_vt = d[o:o + 16 * nv]; o += 16 * nv
    sec_ch = d[o:o + 8 * nc]; o += 8 * nc
    sec_names = d[o:o + 36 * nn]; o += 36 * nn
    sec_hits = d[o:o + 8 * nh]
    pcm = bytearray(d[data_off:data_off + data_bytes])
    keys = [sec_sounds[i * 72:i * 72 + 48].split(b"\0")[0].decode() for i in range(ns)]
    kidx = {k: i for i, k in enumerate(keys)}
    names = {sec_names[i * 36:i * 36 + 32].split(b"\0")[0].decode() for i in range(nn)}
    last_slot = max(struct.unpack_from("<I", sec_trig, i * 28)[0] for i in range(nt))
    assert last_slot < FORM_BASE, last_slot  # (Roxas 70000+ is fine; nothing at 80000+ yet)
    assert "mkse_v1" not in names, "already has the Mickey rows"

    # footstep kind -> sound keys (12soundinfo.bar; any actor's footstep triggers)
    fkind = {}
    for a in sj["actors"].values():
        for sl in a["slots"].values():
            for t in sl.get("triggers", []):
                if t["type"] == "footstep_sound" and t.get("sounds"):
                    fkind.setdefault(int(t["footstep"]), t["sounds"])

    new_s = bytearray()
    new_keys = []

    def want(key):
        if key in kidx:
            return kidx[key]
        s = sounds_js.get(key)
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

    trigs, tsnd = [], []
    miss = set()
    used_vt = set()
    counts = {"footstep": 0, "sound": 0, "voice": 0}
    per_slot = {}
    for D in (P, W):
        for slot_s, v in D.items():
            for f in v["frame"]:
                per_slot.setdefault(int(slot_s), []).append(f)
    for slot in sorted(per_slot):
        for tid, frame, npar, p1, p2 in sorted(per_slot[slot], key=lambda f: f[1]):
            if tid == 14:
                used_vt.add(p1)
                trigs.append((FORM_BASE + slot, frame, 1, VTBL_BASE + p1, 0, 0, 0))
                counts["voice"] += 1
            elif tid == 2:
                ids = [want(k) for k in fkind.get(p1, [])]
                ids = [i for i in ids if i >= 0]
                if not ids:
                    miss.add(f"footstep kind {p1} (slot {slot})")
                    continue
                trigs.append((FORM_BASE + slot, frame, 0, 0, 1, len(ids), len(tsnd) + nts))
                tsnd += ids
                counts["footstep"] += 1
            elif tid == 8:
                key = f"se/se{p1:03d}.win32.scd#{p2}"
                i = want(key)
                if i < 0:
                    miss.add(f"{key} (slot {slot})")
                    continue
                trigs.append((FORM_BASE + slot, frame, 0, 0, 0, 1, len(tsnd) + nts))
                tsnd.append(i)
                counts["sound"] += 1
    # voice table: Mickey = json actor 22 (mickey_coat) vtbl (00battle.bin rows, character 4 -> micky.win32.scd subsongs)
    act = sj["actors"]["22"]["vtbl"]
    vts, chs = [], []
    for vid in sorted(used_vt):
        row = act.get(str(vid))
        if not row:
            miss.add(f"vtbl {vid} not in actor 22")
            continue
        first = len(chs) + nc
        for c in row["choices"]:
            chs.append((want(c["sound"]) if c.get("sound") else -1, int(c.get("weight", 0))))
        vts.append((VTBL_BASE + vid, row["priority"], len(row["choices"]), first))
    # Mickey's atkp rows (mickey.kh.json atkp, SubId 0 ReflectHitSound; VERIFIED in the json): 1088 1089 1090 1091 1121 1190 -> se594 #0,
    # 1199 -> #1, 1122 -> #3, 1092 (finisher) -> #4. 1151 (Pearl?) has none (-1). Same atkp-row hit table the rest of the pack uses.
    new_hits = []
    for row, idx in ((1088, 0), (1089, 0), (1090, 0), (1091, 0), (1121, 0), (1190, 0), (1199, 1), (1122, 3), (1092, 4)):
        i = want("se/se594.win32.scd#%d" % idx)
        if i >= 0:
            new_hits.append((row, i))
    marker = want("voice/us/battle/micky.win32.scd#8")
    new_names = struct.pack("<32sI", b"mkse_v1", max(marker, 0))
    ns2 = ns + len(new_keys)
    body = (sec_sounds + bytes(new_s) + sec_trig + b"".join(struct.pack("<7I", *t) for t in trigs) + sec_ts +
            b"".join(struct.pack("<I", i) for i in tsnd) + sec_vt + b"".join(struct.pack("<4I", *v) for v in vts) + sec_ch +
            b"".join(struct.pack("<iI", s, w) for s, w in chs) + sec_names + new_names + sec_hits + b"".join(struct.pack("<II", r, i) for r, i in new_hits))
    body = bytearray(body)
    while (48 + len(body)) % 16:
        body.append(0)
    hdr = struct.pack("<4s11I", b"SKSF", 1, ns2, nt + len(trigs), nts + len(tsnd), nv + len(vts), nc + len(chs), nn + 1, nh + len(new_hits),
                      48 + len(body), len(pcm), 0)
    open(dst, "wb").write(hdr + body + pcm)
    print(f"{dst}: +{len(new_keys)} sounds, +{len(trigs)} triggers {counts}, +{len(tsnd)} trig sounds, +{len(vts)} vtbl rows (ids "
          f"{[v[0] for v in vts]}), +{len(chs)} choices, +1 name (mkse_v1); {len(pcm) / 1e6:.1f} MB PCM16")
    print("new sounds:", new_keys)
    print("missing:", sorted(miss) or "none")

    # old rows byte-identical?
    n, (ns_, nt_, nts_, nv_, nc_, nn_, nh_, do2, db2) = load(dst)
    o2 = 48
    assert n[o2:o2 + 72 * ns] == sec_sounds
    o2 += 72 * ns2
    assert n[o2:o2 + 28 * nt] == sec_trig
    o2 += 28 * nt_
    assert n[o2:o2 + 4 * nts] == sec_ts
    o2 += 4 * nts_
    assert n[o2:o2 + 16 * nv] == sec_vt
    o2 += 16 * nv_
    assert n[o2:o2 + 8 * nc] == sec_ch
    o2 += 8 * nc_
    assert n[o2:o2 + 36 * nn] == sec_names
    o2 += 36 * nn_
    assert n[o2:o2 + 8 * nh] == sec_hits and nh_ == nh + len(new_hits)
    assert n[do2:do2 + data_bytes] == d[data_off:data_off + data_bytes]
    print("VERIFIED: old sounds / trigger / trig-sound / vtbl / choice / name / hit rows and the old PCM are byte-identical")


if __name__ == "__main__":
    main()

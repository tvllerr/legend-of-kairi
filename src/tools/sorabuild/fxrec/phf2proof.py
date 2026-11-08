"""phf2proof.py (s161) - proof for phf2write.py. Own code.
  python -E -P phf2proof.py <our dir (kh2_fx.bin + kh2_fx.json)> <out dirs as given to phf2write...> [--oracle <kh2_fx.bin>] [--subset]

(a) round trip: recon/s156_work/phf2v.py read_frame on OUR bin == our fxv1 json, every recording (exact)
(b) per recording: EFFREC (frames, flags, bone, paxFlags, raw size) and json fields (frames loops cut bone flags fade file t
    tails period) == the oracle's; frames == the oracle's by recon/s156_work/fxv_cmp.py's rules (tol 1e-3 on p/o/uv/fog.., age and
    colour exact, tex through one global ours->oracle map); also counts frames that are bit-identical; order of elements (index)
(c) texture bytes: our blob for every used texture == the oracle blob its map points to
"""
import hashlib
import json
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fxvbin  # noqa: E402
from phf2dump import Phf2       # noqa: E402  (PHH-format reader, own code)
from phf2v import read_frame    # noqa: E402
from fxv_cmp import cmp_batch, EXACT   # noqa: E402

PKGS = ["kh2_first", "kh2_second", "kh2_third", "kh2_fourth", "kh2_fifth", "kh2_sixth"]
PNG = bytes([0x89, 0x50, 0x4E, 0x47])


def indep_asset(d, pre, rec, local_id):
    """the asset file bytes a recording's local tex id names, found without the writer's code: the run txt from
    <pre>.texsrc (default <pre>_A.txt), its TEXID line, the LAST package whose remastered tree holds the file.
    None for a PNG asset (converted by the writer; covered by step (c) only)."""
    txt = pre + "_A.txt"
    sp = os.path.join(d, pre + ".texsrc")
    if os.path.exists(sp):
        for ln in open(sp):
            f = ln.split()
            if f and f[0] == rec:
                txt = f[1]
    fxname = open(os.path.join(d, pre + ".fxname")).read().strip()
    for ln in open(os.path.join(d, txt), encoding="utf-8", errors="replace"):
        f = ln.split()
        if len(f) >= 5 and f[0] == "TEXID" and int(f[1]) == local_id:
            if f[2] != "hd":
                return b""
            rel = "effect/texcommon.dpd" if f[3] == "texcommon" else fxname
            for pk in reversed(PKGS):
                path = f"D:/KH2Extract/{pk}/remastered/{rel}/{f[4]}"
                if os.path.isfile(path):
                    b = open(path, "rb").read()
                    return None if b.startswith(PNG) else b
    raise SystemExit(f"{pre}: TEXID {local_id} / asset not found for {rec}")

F32 = struct.Struct("<f")
FL = ("p", "o", "uv", "fog", "tfog", "fogColour", "anchor", "anchorOff", "upright")


def f32(x):
    return F32.unpack(F32.pack(x))[0]


def quant(batch):
    """the json batch as the bin stores it: floats rounded to float32"""
    b = dict(batch)
    for k in FL:
        v = b[k]
        b[k] = [[f32(x) for x in r] for r in v] if v and isinstance(v[0], list) else [f32(x) for x in v]
    b["alphaRef"] = f32(b["alphaRef"])
    return b

ORACLE = "%LOCALAPPDATA%/ProjectHeartHunters/cache/kh2_fx.bin"
BATCH = struct.Struct("<HBB4B4B4Bf4f4f3f3f3f9fII")
FIELDS = ["frames", "loops", "cut", "bone", "flags", "fade", "file", "t", "tails", "period"]


def raw_frame(ph, e, i, tab, base):
    ph.f.seek(base + tab[i])
    return zlib.decompress(ph.f.read(tab[i + 1] - tab[i])), tab[i + 1] - tab[i]


def packed_body(ph, e):
    ph.f.seek(e["offset"])
    return ph.f.read(e["packed"])


def tex_walk(a, b, tmap, who, bad):
    """a, b = decompressed frames; batch-by-batch compare with tex patched; fills tmap ours->oracle"""
    if len(a) != len(b):
        return False
    pa = pb = 4
    if a[:4] != b[:4]:
        return False
    for _ in range(struct.unpack_from("<I", a, 0)[0]):
        ha, hb = BATCH.unpack_from(a, pa), BATCH.unpack_from(b, pb)
        ta, tb = ha[0], hb[0]
        if (ta >= 0xFFFE) != (tb >= 0xFFFE) or (ta >= 0xFFFE and ta != tb):
            return False
        if ta < 0xFFFE:
            if tmap.setdefault(ta, tb) != tb:
                bad.append((who, ta, tmap[ta], tb))
        n = ha[43]
        if a[pa + 2:pa + 132 + 44 * n] != b[pb + 2:pb + 132 + 44 * n]:
            return False
        pa += 132 + 44 * n
        pb += 132 + 44 * n
    return True


def main(av):
    d = av[0]
    orc = av[av.index("--oracle") + 1] if "--oracle" in av else ORACLE
    ours, oracle = Phf2(os.path.join(d, "kh2_fx.bin")), Phf2(orc)
    mj = json.load(open(os.path.join(d, "kh2_fx.json")))["effects"]
    oj = json.load(open(orc[:-4] + ".json"))["effects"]
    # our textures
    def tex_blobs(ph):
        ph.f.seek(ph.ttab)
        recs = [struct.unpack("<II", ph.f.read(8)) for _ in range(ph.ntex)]
        return recs
    otex, rtex = tex_blobs(oracle), tex_blobs(ours)
    # json prefix -> out dir (for fxv1 files): every dir given on the command line
    dirs = {}
    for x in av[1:]:
        if x in ("--oracle", "--subset") or (av[av.index(x) - 1] == "--oracle"):
            continue
        p, _, pre = x.partition("=")
        dirs[pre or os.path.basename(p.rstrip("/\\"))] = p
    stat = dict(n=0, rt_ok=0, hdr_ok=0, frames_ok=0, frames_bit_identical=0, frames_total=0, missing_oracle=0)
    subset = "--subset" in av
    diffs = {}
    tmap, tbad = {}, []
    first_bad = []
    for name in ours.eff:
        stat["n"] += 1
        e = ours.eff[name]
        pre = name.split(".")[0]
        # (a) round trip: json (tex -> our global index through the FileSet's TEXID table, floats -> f32) == read back
        jp = os.path.join(dirs[pre], name + ".fxv")       # s164: binary fxv2 first, else the fxv1 json
        j = fxvbin.load_any(jp if os.path.exists(jp) else jp[:-4] + ".json")
        tab, base = ours.frame_chunks(e)
        rt = [dict(frame=i, batches=read_frame(ours, e, i, tab, base)) for i in range(e["frames"])]
        loc = {}
        okrt = rt and True
        for fr, rf in zip(j["perFrame"], rt):
            if len(fr["batches"]) != len(rf["batches"]):
                okrt = False
                break
            for jb, rb in zip(fr["batches"], rf["batches"]):
                if jb["tex"] < 0xFFFE:
                    if loc.setdefault(jb["tex"], rb["tex"]) != rb["tex"]:
                        okrt = False
                    jb = dict(jb, tex=rb["tex"])
                if quant(jb) != rb:
                    okrt = False
        okrt = okrt if e["frames"] else True
        okrt = okrt and e["frames"] == j["frames"] == len(rt)
        # the texture behind each local id must be the asset file the TEXID table names (independent lookup; PNG skipped)
        for lt, gt in loc.items():
            want = indep_asset(dirs[pre], pre, name.split(".", 1)[1], lt)
            if want is None:
                stat["png_skipped"] = stat.get("png_skipped", 0) + 1
                continue
            ours.f.seek(rtex[gt][0])
            if ours.f.read(rtex[gt][1]) != want:
                okrt = False
        if okrt:
            stat["rt_ok"] += 1
        else:
            first_bad.append(("roundtrip", name))
        if name not in oracle.eff:
            stat["missing_oracle"] += 1
            continue
        o = oracle.eff[name]
        # (b) header + json
        bad = [k for k in ("frames", "flags", "bone", "paxFlags", "raw") if e[k] != o[k]]
        mine, theirs = mj[name], oj[name]
        bad += ["json." + k for k in FIELDS if mine.get(k) != theirs.get(k)]
        for k in bad:
            diffs.setdefault(k, []).append(name)
        if not bad:
            stat["hdr_ok"] += 1
        # frames vs the oracle (fxv_cmp rules)
        otab, obase = oracle.frame_chunks(o)
        okf = True
        for i in range(e["frames"]):
            ob = read_frame(oracle, o, i, otab, obase)
            rb = rt[i]["batches"]
            stat["frames_total"] += 1
            same_bits = ob == rb or all({**x, "tex": 0} == {**y, "tex": 0} for x, y in zip(ob, rb))
            stat["frames_bit_identical"] += len(ob) == len(rb) and same_bits
            if len(ob) != len(rb):
                okf = False
                first_bad.append(("batch count", name, i))
                break
            for bi, (x, y) in enumerate(zip(rb, ob)):
                fails, dd, _n = cmp_batch(x, y)
                bad_f = bool(fails) or any((d > 0 if k in ("age", "c") else d > 1e-3) for k, (d, _v) in dd.items())
                if x["tex"] < 0xFFFE or y["tex"] < 0xFFFE:
                    if tmap.setdefault(x["tex"], y["tex"]) != y["tex"]:
                        tbad.append((name, x["tex"], tmap[x["tex"]], y["tex"]))
                        bad_f = True
                elif x["tex"] != y["tex"]:
                    bad_f = True
                if bad_f:
                    okf = False
                    first_bad.append(("frame", name, i, bi, fails))
                    break
            if not okf:
                break
        stat["frames_ok"] += okf
    # (c) textures
    used = sorted(tmap)
    same = 0
    for t in used:
        ours.f.seek(rtex[t][0]); a = ours.f.read(rtex[t][1])
        oracle.f.seek(otex[tmap[t]][0]); b = oracle.f.read(otex[tmap[t]][1])
        same += a == b
    # element order / index: within every file the oracle's index must increase in our order and be contiguous
    mono = contiguous = True
    for pre in dirs:
        oidx = [oj[n]["index"] for n in ours.eff if n.split(".")[0] == pre and n in oj]
        mono &= all(x < y for x, y in zip(oidx, oidx[1:]))
        contiguous &= all(y == x + 1 for x, y in zip(oidx, oidx[1:]))
    def blob(o, s_):
        ours.f.seek(o)
        return ours.f.read(s_)
    # element name sets per prefix against the oracle: any extra element fails; a missing one fails unless --subset
    extra, missing = [], []
    for pre in dirs:
        mine = {n for n in ours.eff if n.split(".")[0] == pre}
        theirs = {n for n in oracle.eff if n.split(".")[0] == pre}
        extra += sorted(mine - theirs)
        missing += sorted(theirs - mine)
    print(f"element names vs oracle: {len(extra)} extra {extra[:5]}, {len(missing)} missing {missing[:5]}"
          + (" (missing allowed: --subset)" if subset else ""))
    ndistinct = len({hashlib.sha1(blob(o, s_)).digest() for o, s_ in rtex})
    print("RESULT", stat)
    print("header/json field diffs:", {k: (len(v), v[:3]) for k, v in diffs.items()} or "none")
    print(f"textures: {len(used)} used, {same} byte-identical to the oracle blob; map conflicts {len(tbad)}; "
          f"our blobs {len(rtex)}, distinct {ndistinct}")
    print(f"element order vs oracle index (within each file): increasing {mono}, contiguous {contiguous}")
    ks = sorted(k for k in tmap if k < 0xFFFE)
    print("texture order: our first-use order maps to increasing oracle indices:", all(tmap[x] < tmap[y] for x, y in zip(ks, ks[1:])))
    print("tex map ours->oracle:", sorted(tmap.items()))
    print("first problems:", first_bad[:5])
    ok = (stat["rt_ok"] == stat["n"] and stat["frames_ok"] == stat["n"] - stat["missing_oracle"] and not diffs
          and same == len(used) and not tbad and not extra and (subset or not missing))
    print("PROOF PASS" if ok else "PROOF FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

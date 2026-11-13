#!/usr/bin/env python3
"""(s223 rikufx) The party stand-ins' own KH2 weapon effects: APPENDED rows of fx.bin ('SKFX' v1). Nothing old is touched (checked at the
end, byte for byte, like kbownfx.py).

  python -E -P partyfx.py --fx-in <fx.bin> --rec <dir: kh2_fx.bin + kh2_fx.json> --trig <dir: trig_<mset>.json> --kh2 <KH2 extract>
                          --out <dir> [--list]

Inputs (all made at install from the player's own KH2 files; no KH2 byte is in this file):
  --rec   our fxrec recording of the stand-ins' weapon effect sets (fxrec/files_party.txt: W_EH000 / W_HE000 / W_MU000 / W_AL000 /
          W_CA000_HUMAN .a.us, recorded by recordall.py like the 82 main files, written by phf2write.py)
  --trig  the weapon msets' triggers (fx/trigjson: trig_W_EH000.json ...): range trigger 12 = KH2's weapon_trail windows
  --kh2   the KH2 extract: each weapon set's PAX element table (the hit-spark categories, below)

KH2 FACTS (recon/s223/S223_HELPER_RIKUFX.md; the KH2 FM exe via Ghidra headless, recon/ghidra/out/s223_rikufx_*.c):
  * The hit spark: re:0x3D1730 (an attack lands) -> re:0x3D1B40: atkp row = re:0x4AD270(attack +0x20), EffectOnHit = row +0xB;
    the effect set = the one of the attack's OWNER object (*attack +0x28); element = re:0x3F6410 -> re:0x2C6730(set, mask) = the
    FIRST PAX record (file order) whose u64 category (+8) holds every bit of mask = (1 << 32 << (EffectOnHit - 1)) | p5.
    p5 = 1 on the path that also sets the attack's 0x1000 flag, else 4 (re:0x3D1730). No record of these sets (nor W_EX010's) has
    bit 2 (4), so the drawn spark is the p5 = 1 case: INFERENCE (not traced which path a plain hit takes).
  * A stand-in's attack windows are range trigger 10 of his WEAPON's mset (partybake HitBake reads W_<xx>.mset), so the owner of
    his attacks = the weapon object = its W_<xx>.a.us set (as Sora's Keyblade = W_EX010_<xx>.a.us).
  * Element 10 of a weapon set = its swing ribbon (s89 kbfx inference for W_EX010_<xx>, recon/S89_HELPER_KBFX.md: 600-frame loop
    bound to bone 2, ribbon topology, alpha 0 in the recording); every stand-in set has an element 10 bound to bone 2 with flag
    bit 0x10000 set, like W_EX010's: GUESS that it is the same ribbon. Packed as kbfx does: frame 1 alone (the runtime reads its
    first batch's texture + blend + first vertex colour).
  * Ribbon windows: the weapon mset's range trigger 12 (weapon_trail; s110 roxsfx checked it equals Sora's W_EX010.mset windows).

PACK ROWS (per stand-in <key>):
  pfx_<key>_hit<k>   k = 1..3: the element KH2's selector picks for EffectOnHit k (a copy of that element: all frames); no row when
                     no record matches (e.g. Auron / Mulan / Aladdin have no EffectOnHit-2 element: KH2 draws none there)
  pfx_<key>_trail    element 10, frame 1 alone
  trail windows      slot key 200000 + 1000 * <PCH id> + <mset slot> (sora_pfx.h PFX_TRAIL_BASE; runtime/sora_pchar.h PCH_* ids)
  New textures go BEFORE the pack's last texture (the ribbon gradient sora_fx.c uses); one identical to an existing texture is
  reused. Old ids / frames / batches / verts / textures / trail windows keep their indices and bytes.
"""
import argparse
import hashlib
import importlib.util
import json
import mmap
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
os.environ.setdefault("SB_FXCACHE", ".")  # (fxbake.py reads these at import; this tool passes every path itself)
os.environ.setdefault("SB_TRIGDIR", ".")

# (key, runtime/sora_pchar.h PCH_* id, recording prefix, KH2 weapon effect file, weapon mset of his motions)
ROWS = [
    ("riku", 2, "w_eh000", "obj/W_EH000.a.us", "W_EH000"),
    ("auron", 3, "w_he000", "obj/W_HE000.a.us", "W_HE000"),
    ("mulan", 4, "w_mu000", "obj/W_MU000.a.us", "W_MU000"),
    ("aladdin", 5, "w_al000", "obj/W_AL000.a.us", "W_AL000"),
    # (partybake: Sparrow's body + weapon models are the _HUMAN ones, his msets P_CA000 / W_CA000; GUESS: the _HUMAN effect set, whose
    # PAX table is the same as W_CA000.a.us's)
    ("sparrow", 6, "w_ca000_human", "obj/W_CA000_HUMAN.a.us", "W_CA000"),
]
TRAIL_BASE = 200000
TRAIL_EL = 10
NEOH = 3
PKGS = ["kh2_first", "kh2_second", "kh2_third", "kh2_fourth", "kh2_fifth", "kh2_sixth"]
OUT_TEX = struct.Struct("<IIII")
OUT_EFF = struct.Struct("<32sIIIIiIII")
OUT_FRAME = struct.Struct("<II")
OUT_BATCH = struct.Struct("<HBBB3BII9f")
OUT_VERT = struct.Struct("<3f3f2fHH4B")
OUT_TRAIL = struct.Struct("<III")
OUT_HDR = struct.Struct("<4s15I")
assert OUT_BATCH.size == 52 and OUT_VERT.size == 40 and OUT_EFF.size == 64 and OUT_HDR.size == 64


def die(msg):
    raise SystemExit("[partyfx] " + msg)


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def new_names():
    out = []
    for key, *_ in ROWS:
        out += ["pfx_%s_hit%d" % (key, k) for k in range(1, NEOH + 1)] + ["pfx_%s_trail" % key]
    return out


# ------------------------------------------------------------------------------------------------ KH2: PAX categories, triggers
def kh2_file(kh2, name):
    hit = None
    for p in PKGS:
        c = os.path.join(kh2, p, "original", name)
        if os.path.isfile(c):
            hit = c
    return hit or die("%s not found under %s" % (name, kh2))


def pax_records(path):
    """[(element id, u64 category)] in file order (the order re:0x2C6730 walks)"""
    raw = open(path, "rb").read()
    base = 0
    if raw[:4] == b"BAR\x01":
        n = struct.unpack_from("<I", raw, 4)[0]
        found = [struct.unpack_from("<I", raw, 0x18 + 0x10 * i)[0] for i in range(n) if struct.unpack_from("<H", raw, 0x10 + 0x10 * i)[0] == 0x12]
        if not found:
            die("%s: no PAX entry" % path)
        base = found[0]
    if raw[base:base + 4] != b"PAX_":
        die("%s: not a PAX" % path)
    return [(struct.unpack_from("<H", raw, base + 0x12 + 0x50 * i)[0], struct.unpack_from("<Q", raw, base + 0x18 + 0x50 * i)[0])
            for i in range(struct.unpack_from("<I", raw, base + 8)[0])]


def spark_element(recs, eoh, p5=1):
    """re:0x2C6730 with re:0x3D1B40's mask: the first record whose category holds every mask bit; None = no spark"""
    mask = (1 << 32 << (eoh - 1)) | p5
    for ident, cat in recs:
        if cat & mask == mask:
            return ident
    return None


def trail_windows(trig_path, pch):
    d = json.load(open(trig_path, encoding="utf-8"))
    out = []
    for slot, v in d.items():
        for r in v.get("range") or []:
            if r[0] == 12:
                out.append((TRAIL_BASE + 1000 * pch + int(slot), int(r[1]), int(r[2]) & 0xFFFFFFFF))
    return sorted(out)


# ------------------------------------------------------------------------------------------------ the pack
class Pack:
    def __init__(self, path):
        if not os.path.isfile(path):
            die("no fx pack at %s" % path)
        self.f = open(path, "rb")
        self.mm = mmap.mmap(self.f.fileno(), 0, access=mmap.ACCESS_READ)
        if len(self.mm) < 64:
            die("%s is too small for an SKFX pack" % path)
        h = OUT_HDR.unpack_from(self.mm, 0)
        if h[0] != b"SKFX" or h[1] != 1:
            die("%s is not an SKFX v1 pack" % path)
        self.ntex, self.neff, self.nfr, self.nba, self.nve, self.ntr = h[2:8]
        self.o_tex, self.o_eff, self.o_fr, self.o_ba, self.o_ve, self.o_tr, self.o_pix, self.pixb = h[8:16]
        if self.o_pix + self.pixb > len(self.mm):
            die("%s is cut short" % path)
        self.tex = [OUT_TEX.unpack_from(self.mm, self.o_tex + i * 16) for i in range(self.ntex)]
        self.eff = {}
        for i in range(self.neff):
            r = OUT_EFF.unpack_from(self.mm, self.o_eff + i * 64)
            self.eff[r[0].split(b"\0")[0].decode("latin1")] = r[1:]

    def trails(self):
        return [OUT_TRAIL.unpack_from(self.mm, self.o_tr + 12 * i) for i in range(self.ntr)]

    def close(self):
        self.mm.close()
        self.f.close()


def convert(fxb, src, meta, en, frame1_only):
    """one PHF2 recording -> [(texture PHF2 index | None, flags, blend, clamp, upright[9], vert bytes, n)] per frame (as fxbake.main)"""
    frs = src.frames(en, 2)[1:] if frame1_only else src.frames(en)
    out = []
    for fr in frs:
        f = []
        for v, vs in fr:
            tex = v[0]
            if tex == 0xFFFE:
                tex = 0xFFFF
            blend_en, op, srcb, dstb = v[3:7]
            blend = 0 if dstb == 2 else (1 if dstb == 6 else 2)
            if not blend_en:
                blend = 2
            clamp = 1 if v[12] or v[13] else 0
            vb = bytearray()
            for p in vs:
                c = [min(255, int(round(x / 32767.5 * 1.992188 * 255))) for x in p[10:14]]
                vb += OUT_VERT.pack(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], 0, *c)
            f.append((None if tex == 0xFFFF else tex, v[2], blend, clamp, v[33:42], bytes(vb), len(vs)))
        out.append(f)
    return out


def build(a, log):
    fxb = load(os.path.join(HERE, "fxbake.py"), "fxb")
    binp, jsp = os.path.join(a.rec, "kh2_fx.bin"), os.path.join(a.rec, "kh2_fx.json")
    if not (os.path.isfile(binp) and os.path.isfile(jsp)):
        die("no recording kh2_fx.bin / kh2_fx.json in %s (sorabuild step fxrec_party)" % a.rec)
    src = fxb.Phf2(binp)
    meta = json.load(open(jsp, encoding="utf-8"))["effects"]
    pk = Pack(a.fx_in)
    for n in new_names():
        if n in pk.eff:
            die("the pack already has %s (run on the pack BEFORE this step)" % n)
    old_last = pk.ntex - 1
    last = pk.tex[old_last]
    if last[2] != max(t[2] for t in pk.tex) or last[2] + last[0] * last[1] * 4 != pk.pixb:
        die("the pack's last texture is not the ribbon gradient at the end of pix")
    old_keys = {t[0] for t in pk.trails()}

    elements, trails = [], []  # (name, frames, loops, pax flags, bone, fade)
    for key, pch, pre, wfile, wmset in ROWS:
        recs = pax_records(kh2_file(a.kh2, wfile))
        picks = []
        for k in range(1, NEOH + 1):
            el = spark_element(recs, k)
            if el is None:
                picks.append("EffectOnHit %d: none" % k)
                continue
            en = "%s.%d" % (pre, el)
            if en not in src.effects:
                die("recording %s is not in %s" % (en, binp))
            off, ps, us, nf, fl, bone, pf = src.effects[en]
            elements.append(("pfx_%s_hit%d" % (key, k), convert(fxb, src, meta, en, False), 0, pf, bone, int(meta.get(en, {}).get("fade", 0))))
            picks.append("EffectOnHit %d: element %d" % (k, el))
        en = "%s.%d" % (pre, TRAIL_EL)
        if en not in src.effects:
            die("recording %s is not in %s" % (en, binp))
        off, ps, us, nf, fl, bone, pf = src.effects[en]
        elements.append(("pfx_%s_trail" % key, convert(fxb, src, meta, en, True), 0, pf, bone, int(meta.get(en, {}).get("fade", 0))))
        tw = trail_windows(os.path.join(a.trig, "trig_%s.json" % wmset), pch)
        clash = [t for t in tw if t[0] in old_keys]
        if clash:
            die("trail key %d of %s is already in the pack" % (clash[0][0], key))
        trails += tw
        log("  %-8s %s (PAX %s): %s; ribbon = element %d; %d trail windows over %d slots" %
            (key, wfile, " ".join("%d:%#x" % r for r in recs), ", ".join(picks), TRAIL_EL, len(tw), len({t[0] for t in tw})))

    # ---- textures: one per PHF2 index used; identical to an old one = reused; else inserted before the gradient
    used = []
    for _, frs, *_r in elements:
        for fr in frs:
            for b in fr:
                if b[0] is not None and b[0] not in used:
                    used.append(b[0])
    sizes = {}
    for i in range(old_last):
        w, h, o, _ = pk.tex[i]
        sizes.setdefault((w, h), []).append(i)
    remap, new_tex = {}, []
    for ti in used:
        w, h, bgra = fxb.dds_bgra(src.texture(ti))
        same = None
        for i in sizes.get((w, h), []):
            o = pk.o_pix + pk.tex[i][2]
            if pk.mm[o:o + w * h * 4] == bgra:
                same = i
                break
        for k, (w2, h2, b2) in enumerate(new_tex):
            if same is None and (w2, h2) == (w, h) and b2 == bgra:
                same = old_last + k
        if same is None:
            same = old_last + len(new_tex)
            new_tex.append((w, h, bgra))
        remap[ti] = same
    t_last = old_last + len(new_tex)  # the gradient's new index

    # ---- the new rows
    newb, newv, newf, newe = bytearray(), bytearray(), bytearray(), bytearray()
    nv_tot = 0
    for name, frs, loops, pax, bone, fade in elements:
        first_frame, maxv = pk.nfr + len(newf) // 8, 0
        for fr in frs:
            first_batch, fv = pk.nba + len(newb) // 52, 0
            for tex, fl, bl, cl, up, vb, n in fr:
                t = 0xFFFF if tex is None else remap[tex]
                newb += OUT_BATCH.pack(t, fl, bl, cl, 0, 0, 0, pk.nve + nv_tot, n, *up)
                newv += vb
                nv_tot += n
                fv += n
            newf += OUT_FRAME.pack(first_batch, len(fr))
            maxv = max(maxv, fv)
        newe += OUT_EFF.pack(name.encode(), first_frame, len(frs), loops, pax, bone, fade, maxv, 0)
        log("  + %-20s %3d frames, max %5d verts/frame, pax %#x bone %#x" % (name, len(frs), maxv, pax, bone))
    log("%d new elements, %d frames, %d batches, %d verts, %d new textures (%d reused), %d trail windows" %
        (len(elements), len(newf) // 8, len(newb) // 52, nv_tot, len(new_tex), len(used) - len(new_tex), len(trails)))
    if a.list:
        pk.close()
        return None

    # ---- write (as kbownfx.build_fx: new textures before the gradient; trail rows appended after the old ones)
    mm = pk.mm
    last_off = last[2]
    npix = bytearray()
    new_texrecs = []
    for (w, h, b) in new_tex:
        new_texrecs.append(OUT_TEX.pack(w, h, last_off + len(npix), 0))
        npix += b
    tex_tbl = mm[pk.o_tex:pk.o_tex + 16 * old_last] + b"".join(new_texrecs) + OUT_TEX.pack(last[0], last[1], last_off + len(npix), 0)
    ntex2, pix2 = pk.ntex + len(new_tex), pk.pixb + len(npix)
    nfr2, nba2, nve2, neff2, ntr2 = pk.nfr + len(newf) // 8, pk.nba + len(newb) // 52, pk.nve + nv_tot, pk.neff + len(newe) // 64, pk.ntr + len(trails)
    newt = b"".join(OUT_TRAIL.pack(*t) for t in trails)
    sizes_ = [ntex2 * 16, neff2 * 64, nfr2 * 8, nba2 * 52, nve2 * 40, ntr2 * 12, pix2]
    offs, q = [], OUT_HDR.size
    for s in sizes_:
        offs.append(q)
        q += s
        q += -q % 16
    if q > (1 << 32) - 1:
        die("the new pack passes the uint32 cap")
    hdr = OUT_HDR.pack(b"SKFX", 1, ntex2, neff2, nfr2, nba2, nve2, ntr2, *offs, pix2)

    def copy_range(fo, x, y):
        while x < y:
            n = min(y - x, 32 << 20)
            fo.write(mm[x:x + n])
            x += n
    os.makedirs(a.out, exist_ok=True)
    dst = os.path.join(a.out, "fx.bin")
    tmp = dst + ".tmp"
    with open(tmp, "wb") as fo:
        fo.write(hdr)
        parts = [lambda: fo.write(tex_tbl),
                 lambda: (copy_range(fo, pk.o_eff, pk.o_eff + 64 * pk.neff), fo.write(newe)),
                 lambda: (copy_range(fo, pk.o_fr, pk.o_fr + 8 * pk.nfr), fo.write(newf)),
                 lambda: (copy_range(fo, pk.o_ba, pk.o_ba + 52 * pk.nba), fo.write(newb)),
                 lambda: (copy_range(fo, pk.o_ve, pk.o_ve + 40 * pk.nve), fo.write(newv)),
                 lambda: (copy_range(fo, pk.o_tr, pk.o_tr + 12 * pk.ntr), fo.write(newt)),
                 lambda: (copy_range(fo, pk.o_pix, pk.o_pix + last_off), fo.write(npix), copy_range(fo, pk.o_pix + last_off, pk.o_pix + pk.pixb))]
        for off_, fn in zip(offs, parts):
            fo.write(bytes(off_ - fo.tell()))
            fn()
    os.replace(tmp, dst)
    log("%s: %.1f MB (was %.1f MB): +%d elements, +%d trail windows, textures %d -> %d (gradient now #%d)" %
        (dst, os.path.getsize(dst) / 1e6, len(mm) / 1e6, len(newe) // 64, len(trails), pk.ntex, ntex2, t_last))
    pk.close()
    verify(a.fx_in, dst, trails, log)
    return dst


def verify(old, new, trails, log):
    po, pn = Pack(old), Pack(new)
    mo, mn = po.mm, pn.mm
    SZ = (16, 64, 8, 52, 40, 12)
    cnt_o = (po.ntex - 1, po.neff, po.nfr, po.nba, po.nve, po.ntr)
    oo = (po.o_tex, po.o_eff, po.o_fr, po.o_ba, po.o_ve, po.o_tr)
    on = (pn.o_tex, pn.o_eff, pn.o_fr, pn.o_ba, pn.o_ve, pn.o_tr)
    names = ("tex", "eff", "frame", "batch", "vert", "trail")
    for i in range(6):
        n, x = cnt_o[i] * SZ[i], 0
        while x < n:
            step = min(n - x, 64 << 20)
            if mo[oo[i] + x:oo[i] + x + step] != mn[on[i] + x:on[i] + x + step]:
                die("VERIFY FAILED: old %s rows differ" % names[i])
            x += step
    ol, nl = po.tex[-1], pn.tex[-1]
    if ol[:2] != nl[:2]:
        die("VERIFY FAILED: the gradient is not the last texture")
    x, n = 0, ol[2]
    while x < n:
        step = min(n - x, 64 << 20)
        if mo[po.o_pix + x:po.o_pix + x + step] != mn[pn.o_pix + x:pn.o_pix + x + step]:
            die("VERIFY FAILED: old pixels differ")
        x += step
    if mo[po.o_pix + ol[2]:po.o_pix + po.pixb] != mn[pn.o_pix + nl[2]:pn.o_pix + pn.pixb]:
        die("VERIFY FAILED: gradient pixels differ")
    if pn.trails()[po.ntr:] != [tuple(t) for t in trails]:
        die("VERIFY FAILED: the new trail windows are not the ones built")
    for b in range(po.nba, pn.nba):
        t, fl, bl, cl, _, _, _, fv, nv = struct.unpack_from("<HBBB3BII", mn, pn.o_ba + b * 52)
        if not (t == 0xFFFF or t < pn.ntex - 1) or fv + nv > pn.nve or bl > 2:
            die("VERIFY FAILED: new batch %d out of range" % b)
    for fi in range(po.nfr, pn.nfr):
        fb, n_ = struct.unpack_from("<II", mn, pn.o_fr + fi * 8)
        if fb + n_ > pn.nba:
            die("VERIFY FAILED: new frame %d out of range" % fi)
    added = set(pn.eff) - set(po.eff)
    if not added <= set(new_names()) or not any(n.endswith("_trail") for n in added):
        die("VERIFY FAILED: unexpected new names %s" % sorted(added - set(new_names())))
    for nm in added:
        ff, nf = pn.eff[nm][0], pn.eff[nm][1]
        if nf < 1 or ff + nf > pn.nfr:
            die("VERIFY FAILED: %s frames out of range" % nm)
    log("VERIFIED fx: old tex (%d) / eff (%d) / frame (%d) / batch (%d) / vert (%d) / trail (%d) rows and all old pixels (%d B) "
        "byte-identical; the gradient is still the last texture; %d new elements, %d new trail windows, every new row in range" %
        (po.ntex - 1, po.neff, po.nfr, po.nba, po.nve, po.ntr, ol[2], len(added), len(trails)))
    po.close()
    pn.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fx-in", required=True)
    ap.add_argument("--rec", required=True)
    ap.add_argument("--trig", required=True)
    ap.add_argument("--kh2", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    build(a, lambda m: print(m, flush=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())

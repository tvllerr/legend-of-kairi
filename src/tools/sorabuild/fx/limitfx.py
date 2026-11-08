#!/usr/bin/env python3
"""(s133 limits) Appends the party Limits' REAL effect elements to an existing fx.bin ('SKFX' v1) by name:
    lim_donald_0 .. lim_donald_6    PHH recording of limit/fm/donald.bar's PAX (Comet): 0 the 600-frame aura on Donald (loop), 1 the
                                    cast flash at his staff, 2..5 the comets / blasts, 6 the Comet Rain's big effect
    lim_goofy_0 .. lim_goofy_5      limit/fm/goofy.bar's PAX (Whirli-Goof): 0 / 1 hit sparks, 2 Goofy's appear flash, 3 / 4 / 5 the
                                    effects of Sora's three motions (A930 / A931 / A932 frame triggers 1)
Source: PHH's recording (kh2_fx.bin / .json 'lim_donald.N' / 'lim_goofy.N'; the '~k' variants of lim_donald.0 are the same
element cut to k frames and are NOT taken: the DLL kills the loop itself). Same conversion as fxbake.py.
New textures go BEFORE the pack's last texture (sora_fx.c uses the last one as the ribbon gradient), as prizefx.py does;
a texture the pack already has (same size + pixels) is reused.

  python -E -P limitfx.py --in <fx.bin> --out <new fx.bin> [--only PREFIX,...] [--list]
Re-running on a pack that already has lim_donald_0 / lim_goofy_0 does nothing (exit 0, no output written).
Works in either order with the magic helper's install (magicfx.py): both rebuild from the LIVE pack and skip names that exist."""
import argparse, hashlib, importlib.util, os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.environ["SB_FXCACHE"] if "SB_FXCACHE" in os.environ else os.environ["SB_PHH"]  # fx cache dir (SB_FXCACHE; SB_PHH = old name), passed in by sorabuild (ctx.fxcache)
FXCACHE = os.environ.get("SB_FXDIR") or CACHE  # (s161) dir with kh2_fx.bin / kh2_fx.json: our fxrec recording ("fx": "fxrec") or the PHH cache


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


fxb = load(os.path.join(HERE, "fxbake.py"), "fxb")
prz = load(os.path.join(HERE, "prizefx.py"), "prz")
OUT_TEX, OUT_EFF, OUT_FRAME, OUT_BATCH, OUT_VERT = fxb.OUT_TEX, fxb.OUT_EFF, fxb.OUT_FRAME, fxb.OUT_BATCH, fxb.OUT_VERT

# (pack name, PHF2 element, loops)
ELEMENTS = [("lim_donald_%d" % i, "lim_donald.%d" % i) for i in range(7)] + \
           [("lim_goofy_%d" % i, "lim_goofy.%d" % i) for i in range(6)] +            [("lim_goofy2_%d" % i, "lim_goofy2.%d" % i) for i in range(17)] +            [("lim_donald2_%d" % i, "lim_donald2.%d" % i) for i in range(6)]   # (s149) goofy2.bar PAX e0..e16; (s150) donald2.bar PAX e0..e5 (Duck Flare); full uncut, the '~k' variants are not taken

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="inp", required=True)
    ap.add_argument("--out")
    ap.add_argument("--cache", default=FXCACHE)
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--only", default="", help="comma list of pack-name prefixes (e.g. lim_donald_,lim_goofy_): only those ELEMENTS are appended, so each install step of the history can be replayed in order")
    a = ap.parse_args()
    import json
    src = fxb.Phf2(os.path.join(a.cache, "kh2_fx.bin"))
    meta = json.load(open(os.path.join(a.cache, "kh2_fx.json"), encoding="utf-8"))["effects"]
    P = prz.read_pack(a.inp)
    names = [P["eff"][i * 64:i * 64 + 32].split(b"\0")[0].decode() for i in range(P["neff"])]
    only = tuple(x for x in a.only.split(",") if x)
    todo = [(pn, en) for pn, en in ELEMENTS if pn not in names and en in src.effects and (not only or pn.startswith(only))]
    for pn, en in ELEMENTS:
        if en not in src.effects and (not only or pn.startswith(only)):
            print("  %s: %s NOT in the recording, skipped" % (pn, en))
    if not todo:
        print("the pack already has every lim_* element: nothing to do")
        return 0
    # ---- convert (as fxbake.main)
    tex_map, textures = {}, []
    effs, frames, batches, verts = [], [], [], []
    base_fr, base_ba, base_ve = P["n"][2], P["n"][3], P["n"][4]
    for pn, en in todo:
        frs = src.frames(en)
        m = meta.get(en, {})
        off, ps, us, nf, fl, bone, pf = src.effects[en]
        first_frame, nverts, maxv = base_fr + len(frames), 0, 0
        for fr in frs:
            first_batch, fv = base_ba + len(batches), 0
            for v, vs in fr:
                tex = v[0]
                if tex == 0xFFFE:
                    tex = 0xFFFF
                if tex != 0xFFFF:
                    if tex not in tex_map:
                        tex_map[tex] = len(textures)
                        textures.append(tex)
                    tex = tex_map[tex]  # NEW-texture index (remapped below)
                blend_en, op, srcb, dstb = v[3:7]
                blend = 0 if dstb == 2 else (1 if dstb == 6 else 2)
                if not blend_en:
                    blend = 2
                clamp = 1 if v[12] or v[13] else 0
                first_vert = base_ve + len(verts)
                for p in vs:
                    c = [min(255, int(round(x / 32767.5 * 1.992188 * 255))) for x in p[10:14]]
                    verts.append(OUT_VERT.pack(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], 0, *c))
                batches.append(OUT_BATCH.pack(tex, v[2], blend, clamp, 0, 0, 0, first_vert, len(vs), *v[33:42]))
                fv += len(vs)
            frames.append(OUT_FRAME.pack(first_batch, len(fr)))
            nverts += fv
            maxv = max(maxv, fv)
        loops = 1 if m.get("loops") else 0
        effs.append((pn, first_frame, len(frs), loops, pf, bone, int(m.get("fade", 0)), maxv))
        print("  %-14s = %-15s %4d frames%s, %6d verts, bone %#06x pax flags %#05x" % (pn, en, len(frs), " (loop)" if loops else "", nverts, bone, pf))
    print("%d new elements, %d frames, %d batches, %d verts, %d textures" % (len(effs), len(frames), len(batches), len(verts), len(textures)))
    if a.list or not a.out:
        return 0
    # ---- textures: reuse an identical one, else insert before the old last texture
    ntex = P["ntex"]
    old_last = ntex - 1
    texrecs = [P["tex"][i * 16:i * 16 + 16] for i in range(ntex)]
    pix = bytearray(P["pix"])
    offs_all = [struct.unpack_from("<IIII", t)[2] for t in texrecs]
    assert offs_all[old_last] == max(offs_all), "last texture is not last in pix"
    last_off = offs_all[old_last]
    last_pix = bytes(pix[last_off:])
    del pix[last_off:]
    sizes = {}
    for i in range(old_last):
        w, h, o, _ = struct.unpack_from("<IIII", texrecs[i])
        sizes.setdefault((w, h), []).append(i)
    new_tex = texrecs[:old_last]
    remap = {}
    for k, ti in enumerate(textures):
        w, h, bgra = fxb.dds_bgra(src.texture(ti))
        same = None
        for i in sizes.get((w, h), []):
            o = struct.unpack_from("<IIII", texrecs[i])[2]
            if hashlib.sha1(bytes(pix[o:o + w * h * 4])).digest() == hashlib.sha1(bgra).digest():
                same = i
                break
        if same is not None:
            remap[k] = same
        else:
            remap[k] = len(new_tex)
            new_tex.append(OUT_TEX.pack(w, h, len(pix), 0))
            pix += bgra
            print("  texture %d = PHF2 #%d: %dx%d" % (remap[k], ti, w, h))
    t_last = len(new_tex)
    new_tex.append(OUT_TEX.pack(*struct.unpack_from("<III", texrecs[old_last])[:2], len(pix), 0))
    pix += last_pix
    batch = bytearray(P["batch"])
    for i in range(len(batch) // 52):  # old batches that named the old last texture
        t = struct.unpack_from("<H", batch, i * 52)[0]
        if t == old_last:
            struct.pack_into("<H", batch, i * 52, t_last)
    nb = bytearray()
    for x in batches:  # new batches: local index -> final
        t = struct.unpack_from("<H", x, 0)[0]
        if t != 0xFFFF:
            x = struct.pack("<H", remap[t]) + x[2:]
        nb += x
    P["tex"] = b"".join(new_tex)
    P["eff"] = P["eff"] + b"".join(OUT_EFF.pack(n.encode()[:31], ff, nf, lp, pf, bone, fade, maxv, 0) for n, ff, nf, lp, pf, bone, fade, maxv in effs)
    P["frame"] = P["frame"] + b"".join(frames)
    P["batch"] = bytes(batch) + bytes(nb)
    P["vert"] = P["vert"] + b"".join(verts)
    P["pix"] = bytes(pix)
    P["n"] = (len(new_tex), P["neff"] + len(effs), P["n"][2] + len(frames), P["n"][3] + len(batches), P["n"][4] + len(verts), P["n"][5])
    P["ntex"] = len(new_tex)
    P["neff"] = P["neff"] + len(effs)
    prz.write_pack(P, a.out)
    print("%s: %.1f MB" % (a.out, os.path.getsize(a.out) / 1e6))
    return 0


if __name__ == "__main__":
    sys.exit(main())

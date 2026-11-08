#!/usr/bin/env python3
"""(s133 magic) Appends KH2's REAL Magnet and Reflect effects (PHH's recordings in kh2_fx.bin, magic/MAGNET_<n>.mag and
magic/REFLECT_<n>.mag) to an existing fx.bin ('SKFX' v1, fxbake.py), same pattern as prizefx.py:
the live pack is read, the new elements/textures/frames are APPENDED (old element ids, frames and texture indices stay), a pack
that already has magnet_start is left alone (exit 0, no output written).

Which recordings and why (the scripts: recon/S133_HELPER_MAGIC.md; MAGNET/REFLECT MAGIC_SHOT thread, disassembled with PHH bdx_vm):
  magnet_cast         magnet_1.0~34   the cast glow (element 0, bound to the Keyblade tip) with KH2's loop-end tail at the cast frame
                                      34 (motion A365 slot 272 magic_cast trigger @34), the same recipe as fire_cast = fire_1.0~22
  magnet_shot         magnet_1.1      the shot burst at the tip (element 1, 65 frames)
  magnet_start[_2/_3] magnet_<n>.2    element 2: appears at the sphere centre at the shot (32 frames)
  magnet_sphere[..]   magnet_<n>.3    element 3: THE SPHERE (a 600 frame loop in the recording; packed 220 frames = longer than the
                                      longest life, grow + hold + shrink = 167 / 190 / 213 frames after its 16 frame start, and forced
                                      to loop; GUESS: the recording is a steady emitter, so the cap hides nothing). The script scales it
                                      with effect_set_scale; the DLL respawns it at stepped scales until the exe has a set-scale call
  magnet_end[..]      magnet_<n>.4    element 4: the end burst when the pull attack is freed (40 frames)
  reflect_barrier[..] reflect_<n>.0   element 0: the barrier on Sora (55 frames = its life, bound 0x4000, PAX fade 8)
  reflect_flash[..]   reflect_<n>.1   element 1: the flash when a hit is blocked (17 frames)
  reflect_burst1[..]  reflect_<n>.4   the burst for < 5 blocked hits (61 frames);  reflect_burst2 = .5 (5..9);  reflect_burst3 = .6 (>= 10)
Tiers: no suffix = Lv1 (Magnet / Reflect), _2 = Lv2 (Magnera / Reflera), _3 = Lv3 (Magnega / Reflega). Not packed: the air variants
(reflect elements 2 / 3 / 14-16 have the same frame counts and vertex counts as 0 / 1 / 4-6: GUESS the same look, the DLL uses the ground ones
in the air too) and the Wisdom / Master / Final variants (12 / 13, magnet 5 / 6).
Blend modes, clamp and colours come straight from the recordings (the rule of fxbake.py). New textures are inserted BEFORE the pack's
last texture (the procedural ribbon gradient sora_fx.c emit_trail uses) and a texture the pack already holds (same size + pixels) is reused.

  python -E -P magicfx.py --in <fx.bin> --out <new fx.bin>
"""
import argparse, hashlib, importlib.util, os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.environ["SB_FXCACHE"] if "SB_FXCACHE" in os.environ else os.environ["SB_PHH"]  # fx cache dir (SB_FXCACHE; SB_PHH = old name), passed in by sorabuild (ctx.fxcache)
FXCACHE = os.environ.get("SB_FXDIR") or CACHE  # (s161) dir with kh2_fx.bin / kh2_fx.json: our fxrec recording ("fx": "fxrec") or the PHH cache


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


prize = load(os.path.join(HERE, "prizefx.py"), "prizefx")  # read_pack / write_pack + the fxbake import
fxb = prize.fxb
OUT_TEX, OUT_EFF, OUT_FRAME, OUT_BATCH, OUT_VERT = fxb.OUT_TEX, fxb.OUT_EFF, fxb.OUT_FRAME, fxb.OUT_BATCH, fxb.OUT_VERT

# (pack name, PHF2 element, frame cap (0 = all), force loop)
ELEMENTS = [("magnet_cast", "magnet_1.0~34", 0, False), ("magnet_shot", "magnet_1.1", 0, False)]
for lv in (1, 2, 3):
    sfx = "" if lv == 1 else "_%d" % lv
    ELEMENTS += [("magnet_start" + sfx, "magnet_%d.2" % lv, 0, False),
                 ("magnet_sphere" + sfx, "magnet_%d.3" % lv, 220, True),
                 ("magnet_end" + sfx, "magnet_%d.4" % lv, 0, False),
                 ("reflect_barrier" + sfx, "reflect_%d.0" % lv, 0, False),
                 ("reflect_flash" + sfx, "reflect_%d.1" % lv, 0, False),
                 ("reflect_burst1" + sfx, "reflect_%d.4" % lv, 0, False),
                 ("reflect_burst2" + sfx, "reflect_%d.5" % lv, 0, False),
                 ("reflect_burst3" + sfx, "reflect_%d.6" % lv, 0, False)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="inp", required=True)
    ap.add_argument("--out")
    ap.add_argument("--cache", default=FXCACHE)
    ap.add_argument("--dry", action="store_true", help="only list the elements and their sizes")
    a = ap.parse_args()
    if not a.out and not a.dry:
        ap.error("--out is required (or --dry)")
    P = prize.read_pack(a.inp)
    names = [P["eff"][i * 64:i * 64 + 32].split(b"\0")[0].decode() for i in range(P["neff"])]
    if "magnet_start" in names:
        print("the pack already has the Magnet / Reflect elements: nothing to do")
        return 0
    src = fxb.Phf2(os.path.join(a.cache, "kh2_fx.bin"))
    import json
    meta = json.load(open(os.path.join(a.cache, "kh2_fx.json"), encoding="utf-8"))["effects"]
    missing = [n for _, n, _, _ in ELEMENTS if n not in src.effects]
    if missing:
        print("NOT in the recording:", missing)
        return 3
    ntex = P["ntex"]
    old_last = ntex - 1
    texrecs = [P["tex"][i * 16:i * 16 + 16] for i in range(ntex)]
    pix = bytearray(P["pix"])
    last_off = struct.unpack_from("<IIII", texrecs[old_last])[2]
    assert last_off == max(struct.unpack_from("<IIII", t)[2] for t in texrecs), "the last texture is not last in pix"
    # texture dedupe: (w, h, sha1 of the BGRA bytes) of every existing texture but the procedural gradient
    have = {}
    for i in range(old_last):
        w, h, off, _ = struct.unpack_from("<IIII", texrecs[i])
        have.setdefault((w, h, hashlib.sha1(bytes(pix[off:off + w * h * 4])).digest()), i)
    last_pix = bytes(pix[last_off:])
    del pix[last_off:]
    new_tex, tex_map, n_new, n_reuse = [], {}, 0, 0  # PHF2 texture index -> pack index

    def pack_tex(ti):
        nonlocal n_new, n_reuse
        if ti in tex_map:
            return tex_map[ti]
        w, h, bgra = fxb.dds_bgra(src.texture(ti))
        key = (w, h, hashlib.sha1(bgra).digest())
        if key in have:
            tex_map[ti] = have[key]
            n_reuse += 1
        else:
            new_tex.append((w, h, bgra))
            tex_map[ti] = old_last + len(new_tex) - 1
            have[key] = tex_map[ti]
            n_new += 1
        return tex_map[ti]

    frames, effs, newb, newv = bytearray(P["frame"]), bytearray(P["eff"]), bytearray(), bytearray()
    nfr, nba, nve = P["n"][2], P["n"][3], P["n"][4]
    total_v = 0
    for pack_name, el, cap, force_loop in ELEMENTS:
        _off, _ps, _us, _nf, _fl, bone, pf = src.effects[el]
        m = meta.get(el, {})
        frs = src.frames(el, cap)
        first_frame, maxv = len(frames) // 8, 0
        for fr in frs:
            first_batch, fv = nba + len(newb) // 52, 0
            for v, vs in fr:
                tex = v[0]
                if tex == 0xFFFE:
                    tex = 0xFFFF
                if tex != 0xFFFF:
                    tex = pack_tex(tex)
                blend_en, op, srcb, dstb = v[3:7]
                blend = 0 if dstb == 2 else (1 if dstb == 6 else 2)
                if not blend_en:
                    blend = 2
                clamp = 1 if v[12] or v[13] else 0
                first_vert = nve + len(newv) // 40
                for p in vs:
                    c = [min(255, int(round(x / 32767.5 * 1.992188 * 255))) for x in p[10:14]]
                    newv += OUT_VERT.pack(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], 0, *c)
                newb += OUT_BATCH.pack(tex, v[2], blend, clamp, 0, 0, 0, first_vert, len(vs), *v[33:42])
                fv += len(vs)
            frames += OUT_FRAME.pack(first_batch, len(fr))
            maxv = max(maxv, fv)
            total_v += fv
        loops = 1 if force_loop or (m.get("loops") and not cap and "~" not in el) else 0
        effs += OUT_EFF.pack(pack_name.encode()[:31], first_frame, len(frs), loops, pf, bone, int(m.get("fade", 0)), maxv, 0)
        print(f"  {pack_name:20s} = {el:16s} {len(frs):3d} frames{' (loop)' if loops else ''}, max {maxv:5d} verts/frame, bone {bone:#06x} pax flags {pf:#05x} fade {int(m.get('fade', 0))}")
    print(f"{len(ELEMENTS)} elements, {total_v} vertices ({total_v * 40 / 1e6:.1f} MB), textures: {n_new} new ({sum(w * h * 4 for w, h, _ in new_tex) / 1e6:.1f} MB), {n_reuse} already in the pack")
    if a.dry:
        return 0
    # textures: the new ones go before the old last (the gradient), which moves to the end
    t_last = old_last + len(new_tex)
    out_tex = list(texrecs[:old_last])
    for w, h, bgra in new_tex:
        out_tex.append(OUT_TEX.pack(w, h, len(pix), 0))
        pix += bgra
    out_tex.append(OUT_TEX.pack(*struct.unpack_from("<III", texrecs[old_last])[:2], len(pix), 0))
    pix += last_pix
    batch = bytearray(P["batch"])
    nrenum = 0
    for i in range(len(batch) // 52):  # old batches that named the old last texture (none expected)
        t = struct.unpack_from("<H", batch, i * 52)[0]
        if t == old_last:
            struct.pack_into("<H", batch, i * 52, t_last)
            nrenum += 1
    nb2 = bytearray(newb)  # (new batches that mapped to old_last cannot exist: the gradient is never deduped)
    if nrenum:
        print(f"renumbered {nrenum} old batches that used the last texture")
    P2 = dict(P)
    P2["tex"] = b"".join(out_tex)
    P2["eff"] = bytes(effs)
    P2["frame"] = bytes(frames)
    P2["batch"] = bytes(batch) + bytes(nb2)
    P2["vert"] = P["vert"] + bytes(newv)
    P2["pix"] = bytes(pix)
    P2["n"] = (len(out_tex), len(effs) // 64, len(frames) // 8, len(P2["batch"]) // 52, len(P2["vert"]) // 40, P["n"][5])
    prize.write_pack(P2, a.out)
    print(f"{a.out}: {os.path.getsize(a.out) / 1e6:.1f} MB; textures {ntex} -> {len(out_tex)}, elements {P['neff']} -> {P2['n'][1]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

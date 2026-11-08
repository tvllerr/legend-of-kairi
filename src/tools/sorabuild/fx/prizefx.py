#!/usr/bin/env python3
"""(s131 prize) Appends KH2's REAL prize orbs (obj/PRIZE.mdlx = a PAX effect, element "prize.0") to an existing fx.bin
('SKFX' v1, tools/fxbake.py), as four looping elements the DLL spawns with sora_fx_spawn (runtime/sora_orbs.h):
    prize_hp (green)  prize_mp (blue)  prize_drive (yellow)  prize_munny (gold gem)

Source: PHH's recording of prize.0 (kh2_fx.bin, 60 frames, two billboards per frame: the orb core sprite + a rotating glow
swirl, both texture PRIZE -0.dds; core alpha, swirl ADDITIVE per the recording - s132 review) and the two HD textures KH2 ships in obj/PRIZE.mdlx (-0.dds = the sprite atlas,
-1.dds = a glass bubble). The recording is the YELLOW orb (core = the atlas' yellow ball, swirl orange) = Drive (VERIFIED by the
atlas: yellow ball top-left, green ball bottom-left).
  prize_drive  the recording as is (+ the glass bubble on top, GUESS: it is the PRIZE file's 2nd texture; the recording does not draw it)
  prize_hp     core = the atlas' green ball (same cell shifted down one row), swirl tinted green         (GUESS: cell + tint)
  prize_mp     core = the atlas' white soft disc tinted blue, swirl tinted blue                           (GUESS: no blue sprite exists)
  prize_munny  core = the atlas' gem (yellow / green / purple facets), swirl tinted gold, no bubble        (GUESS: size / swirl)
Every vertex gets age 0 so the whole orb follows sora_fx_move (the recording is unbound with its owner at the origin).
The new textures are inserted BEFORE the pack's last texture (sora_fx.c emit_trail uses the last one as the ribbon gradient);
any batch that named the old last texture is renumbered. Old element ids / frames / batches / verts keep their indices.

  python -E -P prizefx.py --in <fx.bin> --out <new fx.bin> [--preview out.png]
Re-running on a pack that already has prize_* does nothing (exit 0, no output written).
"""
import argparse, importlib.util, os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
FXB = os.path.join(HERE, "fxbake.py")
CACHE = os.environ["SB_FXCACHE"] if "SB_FXCACHE" in os.environ else os.environ["SB_PHH"]  # fx cache dir (SB_FXCACHE; SB_PHH = old name), passed in by sorabuild (ctx.fxcache)
FXCACHE = os.environ.get("SB_FXDIR") or CACHE  # (s161) dir with kh2_fx.bin / kh2_fx.json: our fxrec recording ("fx": "fxrec") or the PHH cache
PRIZE = os.path.join(os.environ["SB_KH2"], "kh2_first/remastered/obj/PRIZE.mdlx") + "/"
spec = importlib.util.spec_from_file_location("fxb", FXB)
fxb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fxb)
OUT_TEX, OUT_EFF, OUT_FRAME, OUT_BATCH, OUT_VERT, OUT_HDR = fxb.OUT_TEX, fxb.OUT_EFF, fxb.OUT_FRAME, fxb.OUT_BATCH, fxb.OUT_VERT, fxb.OUT_HDR

# atlas cells in uv (256 x 128): the recording's body quad is the yellow ball's cell
CELL_A = (0.0234375, 0.046875, 0.2265625, 0.453125)  # u0 v0 u1 v1 (yellow ball) = the recording's body uv
ROW = 64.0 / 128.0                                    # the green ball / white disc sit one row (64 px) below
COL = 63.0 / 256.0                                    # the white disc is one column (63 px) right of the ball
# the gem: pixels ~ x 135..250, y 5..122 of the atlas
CELL_GEM = (135 / 256.0, 5 / 128.0, 250 / 256.0, 122 / 128.0)
# the glass bubble texture (-1.dds, 256 x 128): the sphere is ~ x 9..120, y 9..119 -> use the whole left half
CELL_BUB = (0.0, 0.0, 0.5, 1.0)

KINDS = [  # name, core cell (u0 v0 u1 v1), core tint rgb (x/255), swirl tint, bubble?, core half size scale, swirl scale
    ("prize_hp", (CELL_A[0], CELL_A[1] + ROW, CELL_A[2], CELL_A[3] + ROW), (1, 1, 1), (0.45, 1.0, 0.45), True, 1.0, 1.0),
    ("prize_mp", (CELL_A[0] + COL, CELL_A[1] + ROW, CELL_A[2] + COL, CELL_A[3] + ROW), (0.8, 1.3, 1.7), (0.7, 1.2, 2.0), True, 1.0, 1.0),
    ("prize_drive", CELL_A, (1, 1, 1), (1, 1, 1), True, 1.0, 1.0),
    ("prize_munny", CELL_GEM, (1.0, 0.95, 0.75), (1.0, 0.8, 0.35), False, 1.25, 0.8),
]


def col8(x):  # PHF2 u16 colour -> the pack's byte (as fxbake.py)
    return min(255, int(round(x / 32767.5 * 1.992188 * 255)))


def read_pack(path):
    b = open(path, "rb").read()
    h = struct.unpack_from("<4s15I", b, 0)
    assert h[0] == b"SKFX" and h[1] == 1, "not an SKFX v1 pack"
    ntex, neff, nfr, nba, nve, ntr = h[2:8]
    o = h[8:15]
    pixb = h[15]
    P = {"ntex": ntex, "neff": neff}
    P["tex"] = b[o[0]:o[0] + 16 * ntex]
    P["eff"] = b[o[1]:o[1] + 64 * neff]
    P["frame"] = b[o[2]:o[2] + 8 * nfr]
    P["batch"] = b[o[3]:o[3] + 52 * nba]
    P["vert"] = b[o[4]:o[4] + 40 * nve]
    P["trail"] = b[o[5]:o[5] + 12 * ntr]
    P["pix"] = b[o[6]:o[6] + pixb]
    P["n"] = (ntex, neff, nfr, nba, nve, ntr)
    return P


def write_pack(P, path):
    ntex, neff, nfr, nba, nve, ntr = P["n"]
    body = [P["tex"], P["eff"], P["frame"], P["batch"], P["vert"], P["trail"], P["pix"]]
    offs, q = [], OUT_HDR.size
    for x in body:
        offs.append(q)
        q += len(x)
        q += -q % 16
    hdr = OUT_HDR.pack(b"SKFX", 1, ntex, neff, nfr, nba, nve, ntr, *offs, len(P["pix"]))
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(hdr)
        for x, o in zip(body, offs):
            f.write(bytes(o - f.tell()))
            f.write(x)
    os.replace(tmp, path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="inp", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--cache", default=FXCACHE)
    ap.add_argument("--preview")
    a = ap.parse_args()
    P = read_pack(a.inp)
    names = [P["eff"][i * 64:i * 64 + 32].split(b"\0")[0].decode() for i in range(P["neff"])]
    if any(n.startswith("prize_") for n in names):
        print("the pack already has prize_* elements: nothing to do")
        return 0
    src = fxb.Phf2(os.path.join(a.cache, "kh2_fx.bin"))
    frs = src.frames("prize.0")
    assert len(frs) == 60 and all(len(f) == 2 for f in frs), "prize.0 is not the 2-batch, 60-frame recording this tool expects"
    meta0 = frs[0][0][0]
    # textures: [atlas, bubble] go before the old last texture (the ribbon gradient)
    ntex = P["ntex"]
    atlas = fxb.dds_bgra(open(PRIZE + "-0.dds", "rb").read())
    bubble = fxb.dds_bgra(open(PRIZE + "-1.dds", "rb").read())
    old_last = ntex - 1
    texrecs = [P["tex"][i * 16:i * 16 + 16] for i in range(ntex)]
    pix = bytearray(P["pix"])
    last_off = struct.unpack_from("<IIII", texrecs[old_last])[2]
    last_pix = bytes(pix[last_off:])
    del pix[last_off:]  # the gradient is the last in pix order too (asserted below)
    assert struct.unpack_from("<IIII", texrecs[old_last])[2] == max(struct.unpack_from("<IIII", t)[2] for t in texrecs), "last texture is not last in pix"
    t_atlas, t_bub, t_last = old_last, old_last + 1, old_last + 2
    new_tex = texrecs[:old_last]
    for (w, h, bgra) in (atlas, bubble):
        new_tex.append(OUT_TEX.pack(w, h, len(pix), 0))
        pix += bgra
    new_tex.append(OUT_TEX.pack(*struct.unpack_from("<III", texrecs[old_last])[:2], len(pix), 0))
    pix += last_pix
    # renumber batches that named the old last texture (none expected)
    batch = bytearray(P["batch"])
    nrenum = 0
    for i in range(len(batch) // 52):
        t = struct.unpack_from("<H", batch, i * 52)[0]
        if t == old_last:
            struct.pack_into("<H", batch, i * 52, t_last)
            nrenum += 1
    if nrenum:
        print(f"renumbered {nrenum} batches that used the last texture")
    frames, effs, newb, newv = bytearray(P["frame"]), bytearray(P["eff"]), bytearray(), bytearray()
    nfr, nba, nve = P["n"][2], P["n"][3], P["n"][4]
    flags0 = meta0[2]
    up = meta0[33:42]

    def quad(vs, uvmap, scale, tint, alpha_mul=1.0):
        # vs: the recording's 6 verts of one billboard; uvmap(u, v) -> new uv; scale: off scale; tint rgb 0..1
        out = []
        for p in vs:
            c = [col8(x) for x in p[10:14]]
            r, g, b = (min(255, int(round(c[i] * tint[i]))) for i in range(3))
            u, v = uvmap(p[6], p[7])
            out.append(OUT_VERT.pack(p[0], p[1], p[2], p[3] * scale, p[4] * scale, p[5] * scale, u, v, 0, 0, r, g, b, min(255, int(round(c[3] * alpha_mul)))))
        return out

    def remap(cell_from, cell_to):
        def f(u, v):
            nu = (u - cell_from[0]) / (cell_from[2] - cell_from[0])
            nv = (v - cell_from[1]) / (cell_from[3] - cell_from[1])
            return cell_to[0] + nu * (cell_to[2] - cell_to[0]), cell_to[1] + nv * (cell_to[3] - cell_to[1])
        return f

    def bmode(v):  # same rule as fxbake.py: D3D dst 2 ONE -> 0 additive, 6 INVSRCALPHA -> 1 alpha, else 2; clamp from v[12]/v[13]
        blend_en, op, srcb, dstb = v[3:7]
        blend = 0 if dstb == 2 else (1 if dstb == 6 else 2)
        return (blend if blend_en else 2), (1 if v[12] or v[13] else 0)

    for name, cell, ctint, stint, bub, cscale, sscale in KINDS:
        first_frame = len(frames) // 8
        maxv = 0
        for fr in frs:
            body, swirl = fr[0][1], fr[1][1]
            fb = nba + len(newb) // 52
            bm_body, bm_swirl = bmode(fr[0][0]), bmode(fr[1][0])  # (s132 review) the recording's own blend + clamp per batch (swirl = additive)
            groups = [(t_atlas, quad(body, remap(CELL_A, cell), cscale, ctint), bm_body),
                      (t_atlas, quad(swirl, lambda u, v: (u, v), sscale, stint), bm_swirl)]
            if bub:  # the glass bubble over the core: body's quad corners, 1.28x the half size (the sphere fills 110 of 128 px)
                groups.append((t_bub, quad(body, remap(CELL_A, CELL_BUB), 1.28, (1, 1, 1), 0.7), (1, 1)))  # GUESS: alpha, clamped
            nv = 0
            for t, vs, (blend, clamp) in groups:
                newb += OUT_BATCH.pack(t, flags0, blend, clamp, 0, 0, 0, nve + len(newv) // 40, len(vs), *up)
                for x in vs:
                    newv += x
                nv += len(vs)
            frames += OUT_FRAME.pack(fb, len(groups))
            maxv = max(maxv, nv)
        effs += OUT_EFF.pack(name.encode(), first_frame, 60, 1, 0, 0, 0, maxv, 0)  # loops, pax flags 0 (unbound: the whole orb takes sora_fx_move)
        print(f"  {name}: first frame {first_frame}, 60 frames (loop), {maxv} verts per frame")
    P2 = dict(P)
    P2["tex"] = b"".join(new_tex)
    P2["eff"] = bytes(effs)
    P2["frame"] = bytes(frames)
    P2["batch"] = bytes(batch) + bytes(newb)
    P2["vert"] = P["vert"] + bytes(newv)
    P2["pix"] = bytes(pix)
    P2["n"] = (len(new_tex), len(effs) // 64, len(frames) // 8, len(P2["batch"]) // 52, len(P2["vert"]) // 40, P["n"][5])
    write_pack(P2, a.out)
    print(f"{a.out}: {os.path.getsize(a.out) / 1e6:.1f} MB; textures {P['ntex']} -> {len(new_tex)} (atlas {t_atlas}, bubble {t_bub}, ribbon gradient {t_last})")
    if a.preview:
        preview(P2, a.preview)
    return 0


def preview(P, path):
    """Software billboard render of frames 0 / 15 / 30 / 45 of the four orbs (alpha blend, no depth) for a look."""
    import numpy as np
    from PIL import Image
    ntex = P["n"][0]
    tex = []
    for i in range(ntex):
        w, h, off, _ = struct.unpack_from("<IIII", P["tex"], i * 16)
        arr = np.frombuffer(P["pix"], np.uint8, w * h * 4, off).reshape(h, w, 4)[:, :, [2, 1, 0, 3]].astype(np.float32) / 255
        tex.append(arr)
    names = [P["eff"][i * 64:i * 64 + 32].split(b"\0")[0].decode() for i in range(P["n"][1])]
    S, N = 160, 4
    sheet = Image.new("RGB", (S * N * 2, S * 2), (40, 60, 40))
    for ki, nm in enumerate([n for n in names if n.startswith("prize_")]):
        ei = names.index(nm)
        ff, nf = struct.unpack_from("<II", P["eff"], ei * 64 + 32)[:2]
        for fi, fr in enumerate((0, 15, 30, 45)):
            fb, nb = struct.unpack_from("<II", P["frame"], (ff + fr) * 8)
            img = np.zeros((S, S, 3), np.float32)
            img[:] = np.array([40, 60, 40], np.float32) / 255 * (1 if fi % 2 == 0 else 3)
            for b in range(fb, fb + nb):
                rec = struct.unpack_from("<HBBBBBBII", P["batch"], b * 52)
                t, first, n = rec[0], rec[7], rec[8]
                vs = [struct.unpack_from("<3f3f2fHH4B", P["vert"], (first + k) * 40) for k in range(n)]
                T = tex[t]
                th, tw = T.shape[:2]
                for tri in range(0, n, 3):
                    pts = [(S / 2 + v[3] * 3.0, S / 2 + v[4] * 3.0) for v in vs[tri:tri + 3]]
                    uvs = [(v[6], v[7]) for v in vs[tri:tri + 3]]
                    cols = [v[10:14] for v in vs[tri:tri + 3]]
                    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
                    for y in range(max(0, int(min(ys))), min(S, int(max(ys)) + 1)):
                        for x in range(max(0, int(min(xs))), min(S, int(max(xs)) + 1)):
                            (x0, y0), (x1, y1), (x2, y2) = pts
                            d = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
                            if abs(d) < 1e-6:
                                continue
                            l0 = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / d
                            l1 = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / d
                            l2 = 1 - l0 - l1
                            if min(l0, l1, l2) < -1e-3:
                                continue
                            u = l0 * uvs[0][0] + l1 * uvs[1][0] + l2 * uvs[2][0]
                            v = l0 * uvs[0][1] + l1 * uvs[1][1] + l2 * uvs[2][1]
                            tx = T[min(th - 1, max(0, int(v * th))), min(tw - 1, max(0, int(u * tw)))]
                            c = [(l0 * cols[0][k] + l1 * cols[1][k] + l2 * cols[2][k]) / 255 for k in range(4)]
                            al = tx[3] * min(1, c[3])
                            img[y, x] = img[y, x] * (1 - al) + tx[:3] * np.array(c[:3]).clip(0, 1) * al
            sheet.paste(Image.fromarray((img.clip(0, 1) * 255).astype(np.uint8)), ((ki * 2 + fi % 2) * S, (fi // 2) * S))
    sheet.save(path)
    print("preview", path)


if __name__ == "__main__":
    sys.exit(main())

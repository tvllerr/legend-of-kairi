"""KAIRI logos for the friend install: builds data/logo/Logo3DGameTitle_00.bin, data/logo/Logo2D_00.bin and
data/overlay/Pack/Title.pack from the player's OWN BotW romfs + our art (s182; lineage tools_s110/logo build_logo.py /
build_title.py / make_title_overlay.py and tools_s135 build_loadlogo_mask.py, ported to numpy only).

  python -E -P logo_build.py <romfs dir> <out data dir> <kbexport dir>

What is whose:
  kairi_layers.npz   OUR art: the K / I / R / I letters (chipped, anti-aliased), in the cutscene-logo grid (412x306) and in the
                     title-screen grid (382x300). No BotW and no KH2 pixels in it (regenerate: dev/README.md).
  kbpaint.py         the Kingdom Key painting, rendered at install from the player's KH2 W_EX010 model (<kbexport dir> = output of
                     `sorabake kbexport W_EX010 <dir>`: mesh.obj + t0_256x128.rgba).
  the constants      our fitted maps (cutscene -> title -> loading mask) and layout numbers.
  BotW textures      read from the player's Pack/Bootup.pack (Layout/Common.sblarc: Logo3DGameTitle_00^l, the source of the
                     kept words THE LEGEND OF / BREATH OF THE WILD, the A, the flower and the TM) and Pack/Title.pack
                     (Layout/Title.sblarc: Logo_W_00^l); they are cut, shifted and mixed at install time, never shipped.
Deterministic: plain numpy float math, one SEEDED rng (kbpaint, pinned by the kit's numpy - re-verify on a kit numpy/Pillow change), Yaz0 re-encode of a fixed window.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import numpy as np  # noqa: E402

import bntx  # noqa: E402
import kbpaint  # noqa: E402
import ndi_lite as ndi  # noqa: E402
import sarc  # noqa: E402
import swz  # noqa: E402
import yaz0  # noqa: E402
import yaz0splice  # noqa: E402

CUT_W, CUT_H = 412, 306
TITLE_W, TITLE_H = 382, 300
L2D_W, L2D_H = 360, 257
BLEED_RULE = 1  # (s182 review: fixed, no env knob - the bytes must not depend on the environment)
# our fitted maps (float64 exactly as fitted)
TITLE_MAP = (1.0226840042265612, 1.0256478992705353, 10.749885978094676, 4.1046018708600025)   # cut = title * s + o
L2D_MAP = (1.4225563624301676, 1.422987276490638, -49.84039093250348, -11.811889327185547)     # cut = mask * s + o
CREAM = np.array([255, 253, 228], np.float32)
CREAM_T = np.array([250, 247, 220], np.float32)   # the title texture's own cream


def find_tex(pack, sblarc, name, w, h, fmt):
    """-> (decoded sblarc bytes, yaz0 stream, entry (name, offset, size) of the bntx, texture dict, pack entries, po, ps)"""
    ents = sarc.parse(pack)[0]
    nm, po, ps = [x for x in ents if x[0] == sblarc][0]
    yz = pack[po:po + ps]
    raw = yaz0.decompress(yz) if yz[:4] == b'Yaz0' else yz
    bn = [x for x in sarc.parse(raw)[0] if x[0] == 'timg/__Combined.bntx'][0]
    t = {x['name']: x for x in bntx.textures(raw[bn[1]:bn[1] + bn[2]])}[name]
    if (t['w'], t['h'], t['fmt']) != (w, h, fmt):
        raise SystemExit('logo_build: %s in %s is %dx%d fmt %x, not the BotW 1.9.0 texture' % (name, sblarc, t['w'], t['h'], t['fmt']))
    return raw, yz, bn, t, ents, po, ps


def decode_rgba(raw, bn, t):
    o = bn[1] + t['offs'][0]
    d = raw[o:o + t['imgsz']]
    return swz.deswizzle(d, t['w'], t['h'], 4, t['blk']).reshape(t['h'], t['w'], 4)


def over_factory(st):
    def over(layer_rgb, layer_a):
        na = layer_a + st['alpha'] * (1 - layer_a)
        st['rgb'] = (layer_rgb * layer_a[..., None] + st['rgb'] * (st['alpha'] * (1 - layer_a))[..., None]) / np.maximum(na[..., None], 1e-4)
        st['alpha'] = na
    return over


def shift(img, dx, dy):
    return np.roll(np.roll(img, dy, 0), dx, 1)


def bleed(rgb, alpha, cream):
    """colour of the transparent texels: the nearest opaque texel within 3 px, else the plain cream (compresses better)"""
    op = alpha > 0.02
    near, by, bx = ndi.near_opaque(op, 3, BLEED_RULE)
    return np.where(op[..., None], rgb, np.where(near[..., None], rgb[by, bx], cream))


def piece_masks(orig):
    """the A, the TM and the flower of the BotW logo (cutscene grid), as build_logo.py cut them"""
    H, W = orig.shape[:2]
    yy, xx = np.mgrid[0:H, 0:W]
    a0 = orig[..., 3]
    lab, _ = ndi.label(a0 > 128)
    Amask = ndi.dilate4(lab == 18)
    tm_ids = [i for i in np.unique(lab[119:129, 360:375]) if i > 0 and i not in (1, 18)]
    tmmask = np.isin(lab, tm_ids) & (xx > 355) & (yy < 135)
    tmmask = ndi.dilate4(tmmask) & (xx > 355) & (yy < 135) & ~Amask
    t = (a0 > 40) & (xx >= 91) & (xx <= 142) & (yy >= 155) & (yy <= 226)
    fl, nfl = ndi.label(t, eight=True)
    sizes = [int(((fl == i) & t).sum()) for i in range(1, nfl + 1)]
    keep = [1 + int(np.argmax(sizes))]
    flmask = ndi.dilate4(np.isin(fl, keep)) & (a0 > 0) & (xx >= 91) & (xx <= 142) & (yy >= 155) & (yy <= 226)
    return Amask, tmmask, flmask


def build_cut(orig, ours, kbdir):
    """the KAIRI cutscene logo (412x306 RGBA uint8)"""
    H, W = orig.shape[:2]
    a0 = orig[..., 3]
    Amask, tmmask, flmask = piece_masks(orig)
    base = orig.astype(np.float32).copy()
    clear = np.zeros((H, W), bool)
    clear[:, :146] = True
    clear[112:225, 146:] = True
    base[clear] = 0
    st = {'alpha': base[..., 3] / 255., 'rgb': base[..., :3].copy()}
    over = over_factory(st)
    AX = 138 - 307
    FDX, FDY = 14, -21
    TDX, TDY = 397 - 361, 0
    Acol = orig[..., :3].astype(np.float32)
    over(np.roll(Acol, AX, 1), np.roll(np.where(Amask, a0 / 255., 0), AX, 1))
    over(np.broadcast_to(CREAM, (H, W, 3)), ours['cut_La'])
    over(shift(Acol, FDX, FDY), shift(np.where(flmask, a0 / 255., 0), FDX, FDY))
    over(shift(Acol, TDX, TDY), shift(np.where(tmmask, a0 / 255., 0), TDX, TDY))
    fc, fa = kbpaint.build(kbdir)
    over(fc, fa * (1 - ours['cut_Fr']))
    rgb = bleed(st['rgb'], st['alpha'], CREAM)
    final = np.dstack([rgb, st['alpha'] * 255]).clip(0, 255).round().astype(np.uint8)
    return final, (Amask, tmmask, flmask)


def build_title(T8, masks, ours, kbdir):
    """the KAIRI title-screen logo (382x300 RGBA uint8) from the title texture T8 and the cutscene piece masks"""
    T = T8.astype(np.float32)
    H, W = T.shape[:2]
    sx, sy, ox, oy = (np.float64(v) for v in TITLE_MAP)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    CY, CX = yy * sy + oy, xx * sx + ox
    La = ours['title_La']
    clr = (CX < 146) | ((CY >= 112) & (CY < 225))
    ta = T[..., 3]
    st = {'alpha': np.where(clr, 0, ta / 255.), 'rgb': T[..., :3].copy()}
    over = over_factory(st)

    def own_piece(cutmask, box):
        x0, x1, y0, y1 = box
        inb = (xx >= x0) & (xx <= x1) & (yy >= y0) & (yy <= y1)
        return (ndi.bilinear0(cutmask.astype(np.float32), CY, CX) > 0.3) & (ta > 0) & inb

    tx = lambda x: (x - ox) / sx  # noqa: E731
    ty = lambda y: (y - oy) / sy  # noqa: E731
    Amask, tmmask, flmask = masks
    Am = own_piece(Amask, (tx(290), tx(410), ty(100), ty(240)))
    Fm = own_piece(flmask, (tx(91), tx(142), ty(155), ty(226)))
    Tm = own_piece(tmmask, (tx(355), W, 0, ty(135)))
    AXt = int(round((138 - 307) / sx))
    FDXt = int(round(14 / sx))
    FDYt = int(round(-21 / sy))
    Tc = T[..., :3]
    over(shift(Tc, AXt, 0), shift(np.where(Am, ta / 255., 0), AXt, 0))
    over(np.broadcast_to(CREAM_T, (H, W, 3)), La)
    over(shift(Tc, FDXt, FDYt), shift(np.where(Fm, ta / 255., 0), FDXt, FDYt))
    ys_, xs_ = np.nonzero(Tm)
    cap_top = int(np.floor(ty(119.9)))
    last_col = int(np.nonzero(La.max(0) > 0.5)[0].max())
    TDXt = min(W - 2, last_col) - xs_.max()
    TDYt = (cap_top - 2) - ys_.max()
    over(shift(Tc, TDXt, TDYt), shift(np.where(Tm, ta / 255., 0), TDXt, TDYt))
    band = ta[20:80, 40:100] > 128      # the Master Sword's column centre in the title texture = where the Keyblade goes
    cx = 40 + (band * np.arange(60)[None, :]).sum() / band.sum()
    fc, fa = kbpaint.build(kbdir, top=0, tip=299, cx=cx, W=W, H=H)
    over(fc, fa * (1 - ours['title_Fr']))
    rgb = bleed(st['rgb'], st['alpha'], CREAM_T)
    return np.dstack([rgb, st['alpha'] * 255]).clip(0, 255).round().astype(np.uint8)


def build_l2d(cut):
    """the loading-screen mask Logo2D_00^d (360x257 L): the KAIRI cutscene alpha warped by our fitted map, the Keyblade weighted by luminance"""
    sx, sy, tx, ty = L2D_MAP
    yy, xx = np.mgrid[0:L2D_H, 0:L2D_W].astype(float)
    im = cut.astype(float) / 255
    a = im[..., 3]
    lum = im[..., 0] * .3 + im[..., 1] * .59 + im[..., 2] * .11
    a = a * np.clip(lum / 0.95, 0, 1) ** 0.6
    a = ndi.gaussian(a, 0.45 * sx)
    k = ndi.bilinear0(a, sy * yy + ty, sx * xx + tx)
    return np.clip(np.round(k * 255), 0, 255).astype(np.uint8)


def splice_title_pack(pack, ents, po, ps, yz, raw, bn, t, img8):
    a = bn[1] + t['offs'][0]
    b = a + t['imgsz']
    assert swz.swizzle(swz.deswizzle(raw[a:b], t['w'], t['h'], 4, t['blk']), t['w'], t['h'], 4, t['blk']) == raw[a:b]
    sw = swz.swizzle(img8.reshape(t['h'], t['w'] * 4), t['w'], t['h'], 4, t['blk'])
    assert len(sw) == t['imgsz']
    newraw = raw[:a] + sw + raw[b:]
    y2, _ = yaz0splice.splice(yz, newraw, a, b)
    assert yaz0.decompress(y2) == newraw
    if len(y2) > len(yz):
        raise SystemExit('logo_build: re-encoded Title.sblarc is bigger than the original (%d > %d)' % (len(y2), len(yz)))
    y2 = y2 + b'\0' * (len(yz) - len(y2))
    newpack = pack[:po] + y2 + pack[po + ps:]
    assert len(newpack) == len(pack) and sarc.parse(newpack)[0] == ents
    return newpack


def build_all(romfs, kbdir):
    ours = dict(np.load(os.path.join(HERE, 'kairi_layers.npz')))
    boot = open(os.path.join(romfs, 'Pack', 'Bootup.pack'), 'rb').read()
    raw, yz, bn, t, ents, po, ps = find_tex(boot, 'Layout/Common.sblarc', 'Logo3DGameTitle_00^l', CUT_W, CUT_H, 0xb06)
    orig = decode_rgba(raw, bn, t)
    cut, masks = build_cut(orig, ours, kbdir)
    out = {}
    out['logo/Logo3DGameTitle_00.bin'] = swz.swizzle(cut.reshape(CUT_H, CUT_W * 4), CUT_W, CUT_H, 4, t['blk'])
    assert len(out['logo/Logo3DGameTitle_00.bin']) == t['imgsz'] == 638976
    t2 = find_tex(boot, 'Layout/Common.sblarc', 'Logo2D_00^d', L2D_W, L2D_H, 0x201)[3]
    mask = build_l2d(cut)
    out['logo/Logo2D_00.bin'] = swz.swizzle(mask, L2D_W, L2D_H, 1, t2['blk'])
    assert len(out['logo/Logo2D_00.bin']) == t2['imgsz'] == 147456
    tp = open(os.path.join(romfs, 'Pack', 'Title.pack'), 'rb').read()
    raw3, yz3, bn3, t3, ents3, po3, ps3 = find_tex(tp, 'Layout/Title.sblarc', 'Logo_W_00^l', TITLE_W, TITLE_H, 0xb06)
    title = build_title(decode_rgba(raw3, bn3, t3), masks, ours, kbdir)
    out['overlay/Pack/Title.pack'] = splice_title_pack(tp, ents3, po3, ps3, yz3, raw3, bn3, t3, title)
    return out, dict(cut=cut, mask=mask, title=title, orig=orig)


def main():
    romfs, outdir, kbdir = sys.argv[1:4]
    out, _ = build_all(romfs, kbdir)
    for rel, data in out.items():
        p = os.path.join(outdir, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p + '.tmp', 'wb').write(data)
        os.replace(p + '.tmp', p)
        print('wrote', rel, len(data))


if __name__ == '__main__':
    main()

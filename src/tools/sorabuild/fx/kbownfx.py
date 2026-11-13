#!/usr/bin/env python3
"""(s216 kbfx) The mod's own two Keyblades get their OWN swing ribbon, hit sparks, hit sound and (Sealing Blade) beam: APPENDED rows
of fx.bin / sfx.bin. Nothing old is touched (checked at the end, byte for byte, like chickfx.py).

  python -E -P kbownfx.py --fx-in <fx.bin> --sfx-in <sfx.bin> --out <dir> --ours <tools/sorabuild/ours/kb> [--no-fx] [--no-sfx]

Inputs: the running packs of the fx chain (fx.bin already holds KH2's per-Keyblade sets kb<xx>_* of tools/sorabuild/fx/fxbake.py,
made at install from the player's own KH2 data) + our synthesised WAVs (ours/kb/kbsynth.py). No KH2 / BotW byte is in this file
or in the WAVs.

EFFECTS (fx.bin, SKFX v1). Set names: Sealing Blade "s1", Guardian Core "g1" (KH2's own sets all end in '0').
  kbs1_hit / _hit_finish / _hit_guard   = Fenrir's (kbm0_*) sparks, same frames / motion / size / timing (the placeholder the user
                                          liked), RECOLOURED: every texture they use becomes a grey copy (its brightness: max of
                                          r g b, alpha kept) and the hue moves to the vertex colours: core = blue-white
                                          178 235 255, deep = indigo 92 100 230, accent = gold 255 206 96 (Fenrir's warm parts:
                                          its orange embers / gold stars / finisher ring -> gold sparkle).
  kbg1_hit / _hit_finish / _hit_guard   = Photon Debugger's (kb90_*) sparks recoloured the same way: core = cyan 76 235 255,
                                          deep = 24 150 190, accent = orange 255 122 15 (the particle-atlas embers + the
                                          finisher's ring).
  kbs1_trail / kbg1_trail               = kbm0_trail / kb90_trail (the runtime reads only the first batch's texture + blend and the
                                          first vertex's colour) with a NEW ribbon texture made from the source ribbon's
                                          brightness: Sealing = indigo -> blue-white -> white + gold sparkle dots; Guardian =
                                          dark teal -> cyan -> white-cyan + an orange line along the v 0 edge (the tip, GUESS
                                          default of sora_fx.c). Vertex colour white (the texture carries the colour).
  kbs1_beam                             = PROCEDURAL (ours): a flying blue-white crescent, 150 cm wide, a 32-frame LOOP,
                                          unbound (pax 0, bone 0) like mk_pearl_ball: draw it with sora_fx_spawn at the beam's
                                          point + sora_fx_move every frame, yaw = its travel direction (the crescent's convex
                                          side faces +z = forward, its ends trail 35 cm back and droop 43 cm); ghost copies at
                                          vertex age 2 / 4 / 6 and gold sparkles at age 0..10 lag along the flight path.
  kbs1_beam_hit                         = kbs1_hit_finish's frames scaled x1.5 + a procedural expanding ground ring (blue glow
                                          + a later thin gold ring), one-shot, 50 frames.
  kbs1_beam_r000 .. kbs1_beam_r165      = (s217) PROCEDURAL: the crescent lying in the swing's plane (flat C, convex side + sharp
                                          core forward, glow trailing; a second band across the plane so it never vanishes
                                          edge-on), pre-rolled about the travel axis in 15 deg steps (sora_fx.c is exe side and
                                          places unbound elements with a yaw only). Same loop / ghosts / sparkles / colours as
                                          kbs1_beam, which stays as it was (sora_kbbeam.h: "kbbeamroll 0").
  New textures (grey copies, the two ribbons, our band + sparkle) are inserted BEFORE the pack's last texture (the ribbon gradient
  sora_fx.c emit_trail uses); one identical to an existing texture is reused. Old ids / frames / batches / verts / textures keep
  their indices.
SOUNDS (sfx.bin, SKSF v1): + kbs1_hit, kbg1_hit, kbs1_beam, kbs1_beam_hit (keys "ours/kb/<name>.wav", names = the same) and the
  marker name "kbse_v1". No trigger / voice / hit-table row changes.
"""
import argparse
import hashlib
import mmap
import os
import struct
import sys
import traceback
import wave

import numpy as np

OUT_TEX = struct.Struct("<IIII")
OUT_EFF = struct.Struct("<32sIIIIiIII")
OUT_FRAME = struct.Struct("<II")
OUT_BATCH = struct.Struct("<HBBB3BII9f")
OUT_HDR = struct.Struct("<4s15I")
VERT = np.dtype([("p", "<f4", 3), ("off", "<f4", 3), ("uv", "<f4", 2), ("age", "<u2"), ("pad", "<u2"), ("rgba", "u1", 4)])
assert VERT.itemsize == 40 and OUT_BATCH.size == 52 and OUT_EFF.size == 64
SK = struct.Struct("<4s11I")

PAL = {  # role -> rgb (GUESS colours, from the approved looks recon/s212/S212_HELPER_KEYBLADES.md)
    "s1": {"core": (178, 235, 255), "deep": (92, 100, 230), "accent": (255, 206, 96)},
    "g1": {"core": (76, 235, 255), "deep": (24, 150, 190), "accent": (255, 122, 15)},
}
SRC = {"s1": "m0", "g1": "90"}  # the placeholder set each one is shaped from (the feel the user approved)
KINDS = ["hit", "hit_finish", "hit_guard"]
# Which colour each (texture, blend) group of the source sparks takes (read from the live pack, s216: mean / max vertex colours per
# group + the textures as pictures). blend None = any. A texture of another size than TEX_WH (a different pack) -> "auto".
# auto = the hue rule: warm and saturated (Fenrir's gold-tinted finisher slashes) -> accent, else core.
TEX_WH = {0: (256, 512), 1: (128, 128), 2: (128, 64), 3: (128, 64), 57: (128, 64), 58: (128, 64), 59: (128, 64), 90: (128, 64),
          91: (128, 64), 92: (128, 128), 93: (128, 128)}
ROLE_TAB = {
    # Fenrir (kbm0_*): 0 = the particle atlas (orange embers), 2 = the finisher's ring arc, 92 add = gold stars -> gold; 3 = the big
    # translucent wave behind the burst -> indigo haze; 1 / 90 / 92 alpha / 93 = flash, streaks, pale stars, the finisher's sparkle
    # cloud -> blue-white
    "s1": {(0, None): "accent", (2, None): "accent", (92, 0): "accent", (93, None): "core", (3, None): "deep",
           (1, None): "core", (90, None): "core", (92, 1): "core", (91, None): "auto"},
    # Photon Debugger (kb90_*): 0 = the particle atlas (embers) + 2 = the finisher's ring -> orange; 58 alpha (blended stars) -> deep
    # cyan; 1 / 57 / 58 add / 59 (flash, streaks, stars, the circuit lines) -> cyan
    "g1": {(0, None): "accent", (2, None): "accent", (58, 1): "deep", (1, None): "core", (57, None): "core", (58, 0): "core",
           (59, None): "core"},
}
# The shapes that made the placeholder read as Fenrir's / Photon Debugger's (their stars) are redrawn as ours: Fenrir's 5-point stars
# (92, and the square cells of the 91 and 0 atlases: the 0 atlas's cells are the gold embers) -> four-point glints; Photon
# Debugger's star outlines (58 atlas) -> hexagons. (Guardian keeps the 0 atlas as it is: its round flare is the orange burst.)
RESHAPE = {"s1": {0: "glint", 91: "glint", 92: "glint"}, "g1": {58: "hex"}}
# (s217 beam) the crescent that lies in the swing's plane, pre-rolled: sora_fx.c (exe side) places an unbound element with a yaw
# only, so each roll about the travel axis is its own element (APPENDED after kbs1_beam_hit; every s216 row stays as it was).
# kbs1_beam_r<deg> = the flat crescent rolled by <deg> (0 = flat, 90 = upright; 180 = 0 again, the shape is symmetric).
BEAM_ROLL_STEP = 15
BEAM_ROLLS = list(range(0, 180, BEAM_ROLL_STEP))
BEAM_ROLL_FX = ["kbs1_beam_r%03d" % r for r in BEAM_ROLLS]
NEW_FX = (["kbs1_" + k for k in KINDS] + ["kbs1_trail"] + ["kbg1_" + k for k in KINDS] + ["kbg1_trail", "kbs1_beam", "kbs1_beam_hit"]
          + BEAM_ROLL_FX)
SOUNDS = ["kbs1_hit", "kbg1_hit", "kbs1_beam", "kbs1_beam_hit"]
SFX_MARKER = "kbse_v1"


def die(msg):
    raise SystemExit("[kbownfx] " + msg)


# (s217 review F2) Every float this tool GENERATES (procedural positions / offsets / UVs, and the alphas / colours it computes before
# they become bytes) goes through an integer first: the release pins fx.bin by sha256, and sin / cos / exp / ** may differ in the last
# bit on another PC's C runtime / CPU. Near-cancellations (x cos r - y sin r that should be 0 came out as 1e-15 garbage: 26,031 such
# position floats in the s217 pack) then become exact 0, everything else an exact multiple of 1/Q (exact in float32 for |v| < 16384).
# Values copied from KH2's own elements are NOT touched (they stay byte-identical). The 0..255 values use a fine grid: it only has
# to catch values a hair from an int() / round() edge (which a last bit could push across); a coarse one would itself move values.
Q_POS = 1024      # positions + offsets, KH cm: half a quantum = 0.0005 cm
Q_UV = 1 << 16    # UVs (0..1)
Q_VAL = 1 << 30   # alphas / colours (0..255) before int() / round()


def qz(a, q):
    """a (any shape) -> float64 multiples of 1/q (from int64, so -0.0 and garbage near 0 become +0.0)"""
    return np.rint(np.asarray(a, np.float64) * q).astype(np.int64) / q


def rnd(a):
    """a generated 0..255 value -> its whole number: on the 2^-30 grid first, then half UP (a value a hair above .5 that the s217
    pack rounded up stays up: the pack the user saw keeps its pixels)"""
    return np.floor(qz(a, Q_VAL) + 0.5)


def qverts(v):
    """a generated VERT array, ready to write: p / off / uv on their grids (once, last: after any roll)"""
    v["p"] = qz(v["p"], Q_POS)
    v["off"] = qz(v["off"], Q_POS)
    v["uv"] = qz(v["uv"], Q_UV)
    return v


# ================================================================================================================ fx: reading
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
        self.h = h
        self.ntex, self.neff, self.nfr, self.nba, self.nve, self.ntr = h[2:8]
        self.o_tex, self.o_eff, self.o_fr, self.o_ba, self.o_ve, self.o_tr, self.o_pix, self.pixb = h[8:16]
        if self.o_pix + self.pixb > len(self.mm):
            die("%s is cut short" % path)
        self.tex = [OUT_TEX.unpack_from(self.mm, self.o_tex + i * 16) for i in range(self.ntex)]
        self.eff = {}
        for i in range(self.neff):
            r = OUT_EFF.unpack_from(self.mm, self.o_eff + i * 64)
            self.eff[r[0].split(b"\0")[0].decode("latin1")] = r[1:]

    def pixels(self, t):
        w, h, off, _ = self.tex[t]
        return np.frombuffer(self.mm[self.o_pix + off:self.o_pix + off + w * h * 4], np.uint8).reshape(h, w, 4)  # BGRA

    def element(self, name):
        """[(batch tuple, verts ndarray copy)] per frame"""
        ff, nf = self.eff[name][0], self.eff[name][1]
        out = []
        for k in range(nf):
            fb, n = OUT_FRAME.unpack_from(self.mm, self.o_fr + (ff + k) * 8)
            fr = []
            for b in range(fb, fb + n):
                bt = OUT_BATCH.unpack_from(self.mm, self.o_ba + b * 52)
                v = np.frombuffer(self.mm[self.o_ve + bt[7] * 40:self.o_ve + (bt[7] + bt[8]) * 40], VERT).copy()
                fr.append((bt, v))
            out.append(fr)
        return out

    def close(self):
        self.mm.close()
        self.f.close()


# ================================================================================================================ fx: textures
def grey(px):
    """brightness copy: rgb = max(r, g, b), alpha kept (the hue moves to the vertex colours)"""
    v = px[:, :, :3].max(axis=2)
    out = px.copy()
    out[:, :, 0] = out[:, :, 1] = out[:, :, 2] = v
    return out


def ramp(g, stops):
    """g 0..1 (h, w) -> rgb (h, w, 3) through the (pos, (r, g, b)) stops"""
    pos = np.array([s[0] for s in stops])
    out = np.zeros(g.shape + (3,))
    for c in range(3):
        out[..., c] = np.interp(g, pos, [s[1][c] for s in stops])
    return out


def trail_tex(px, which):
    """our ribbon texture from a source ribbon's brightness (BGRA in, BGRA out)"""
    h, w = px.shape[:2]
    g = px[:, :, :3].max(axis=2).astype(np.float64) / 255
    a = px[:, :, 3].astype(np.float64)
    if which == "s1":
        rgb = ramp(g, [(0.0, (70, 80, 210)), (0.45, (178, 235, 255)), (1.0, (240, 250, 255))])
        rng = np.random.default_rng(2165)
        for _ in range(max(12, w * h // 200)):  # gold sparkle dots, older (larger u) ones fewer and fainter
            u = float(qz(rng.random() ** 1.6 * 0.8, Q_VAL))
            x, y = int(u * w), int((0.12 + 0.76 * rng.random()) * h)
            fade = max(0.0, 1 - u / 0.8)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < w and 0 <= yy < h:
                        k = 1.0 if dx == dy == 0 else 0.45
                        rgb[yy, xx] = rgb[yy, xx] * (1 - k) + np.array((255, 214, 110)) * k
                        a[yy, xx] = max(a[yy, xx], 235 * fade * k)
    else:
        rgb = ramp(g, [(0.0, (10, 70, 90)), (0.45, (76, 235, 255)), (1.0, (220, 255, 255))])
        r0, r1 = max(1, int(h * 0.04)), max(2, int(h * 0.13))  # the orange line along the v 0 (tip) edge
        for y in range(r0, r1):
            k = 0.9 * (1 - abs((y - (r0 + r1) / 2) / ((r1 - r0) / 2 + 0.5)))
            rgb[y] = rgb[y] * (1 - k) + np.array((255, 122, 15)) * k
    out = np.zeros_like(px)
    out[:, :, 0], out[:, :, 1], out[:, :, 2] = rgb[..., 2], rgb[..., 1], rgb[..., 0]
    out[:, :, 3] = np.clip(rnd(a), 0, 255)
    return out


def components(mask):
    """4-connected components of a bool mask -> [(ys, xs)] (small textures: a plain flood fill)"""
    h, w = mask.shape
    seen = np.zeros_like(mask)
    out = []
    for y0 in range(h):
        for x0 in range(w):
            if not mask[y0, x0] or seen[y0, x0]:
                continue
            stack, ys, xs = [(y0, x0)], [], []
            seen[y0, x0] = True
            while stack:
                y, x = stack.pop()
                ys.append(y), xs.append(x)
                for yy, xx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                    if 0 <= yy < h and 0 <= xx < w and mask[yy, xx] and not seen[yy, xx]:
                        seen[yy, xx] = True
                        stack.append((yy, xx))
            out.append((np.array(ys), np.array(xs)))
    return out


def shape_alpha(kind, x, y, outline):
    """our shapes on -1..1 coords: "glint" = a four-point glint (long thin rays, the vertical one longest, a round core: the
    Sealing Blade's holy sparkle), "hex" = a hexagon (Guardian tech: an outline ring + a centre dot, or a filled plate with a
    bright rim)"""
    if kind == "glint":
        core = np.exp(-(x * x + y * y) / 0.04)
        rays = np.exp(-np.abs(x) / 0.06) * np.clip(1 - np.abs(y), 0, 1) ** 1.5 +             0.75 * np.exp(-np.abs(y) / 0.06) * np.clip(1 - np.abs(x) / 0.8, 0, 1) ** 1.5
        diag = 0.3 * np.exp(-np.abs(x - y) / 0.05) * np.exp(-np.abs(x + y) / 0.35) +             0.3 * np.exp(-np.abs(x + y) / 0.05) * np.exp(-np.abs(x - y) / 0.35)
        return np.clip(core + rays + diag, 0, 1)
    d = np.maximum(np.abs(x), np.abs(x) * 0.5 + np.abs(y) * 0.866)  # 1 = the hexagon's edge
    rim = np.clip(1 - np.abs(d - 0.86) / 0.1, 0, 1)
    if outline:
        return np.clip(rim + np.clip(1 - d / 0.18, 0, 1), 0, 1)
    return np.clip(np.where(d < 0.86, 0.5, 0) + rim, 0, 1)


def reshape(px, kind):
    """keep the texture's layout (where its shapes sit: KH2 UVs point there), draw OUR shape in each roughly square shape's box"""
    out = px.copy()
    a = px[:, :, 3]
    n = 0
    for ys, xs in components(a > 24):
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        bw, bh = x1 - x0, y1 - y0
        if min(bw, bh) < 6 or max(bw, bh) > 1.6 * min(bw, bh):
            continue  # streaks / tiny dots stay as they are
        fill = len(ys) / float(bw * bh)
        amax = float(a[ys, xs].max())
        out[ys, xs, 3] = 0
        gy, gx = np.mgrid[y0:y1, x0:x1]
        sx, sy = (gx + 0.5 - (x0 + x1) / 2) / (bw / 2), (gy + 0.5 - (y0 + y1) / 2) / (bh / 2)
        sa = shape_alpha(kind, sx, sy, fill < 0.3) * amax
        reg = out[y0:y1, x0:x1]
        reg[:, :, 3] = np.maximum(reg[:, :, 3], rnd(sa).astype(np.uint8))
        reg[:, :, :3] = np.where((sa > 0)[..., None], 255, reg[:, :, :3])
        n += 1
    return out, n


def band_tex(w=64, h=32):
    """ours: a soft band, bright in the middle across v, a faint streak grain along u (white; the colour is the vertex's)"""
    rng = np.random.default_rng(2166)
    v = (np.arange(h) + 0.5) / h
    across = np.sin(np.pi * v) ** 1.5
    grain = 0.82 + 0.18 * np.interp(np.arange(w), np.linspace(0, w, 9), rng.random(9))
    a = np.outer(across, grain) * 255
    out = np.full((h, w, 4), 255, np.uint8)
    out[:, :, 3] = np.clip(rnd(a), 0, 255)
    return out


def sparkle_tex(n=32):
    """ours: a four-point star with a round core (white)"""
    c = (np.arange(n) + 0.5) / n * 2 - 1
    x, y = np.meshgrid(c, c)
    core = np.exp(-(x * x + y * y) / 0.03)
    arms = np.exp(-np.abs(x) / 0.05) * np.exp(-np.abs(y) / 0.45) + np.exp(-np.abs(y) / 0.05) * np.exp(-np.abs(x) / 0.45)
    a = np.clip(core + 0.8 * arms, 0, 1) * np.clip(1 - np.sqrt(x * x + y * y), 0, 1) ** 0.5
    out = np.full((n, n, 4), 255, np.uint8)
    out[:, :, 3] = np.clip(rnd(a * 255), 0, 255)
    return out


class Textures:
    """new textures (deduped against the old pack and each other); refs are ("old", i) or ("new", k)"""

    def __init__(self, pk):
        self.pk = pk
        self.by_hash = {}
        for i, (w, h, off, _) in enumerate(pk.tex[:-1]):
            self.by_hash.setdefault((w, h, hashlib.md5(pk.mm[pk.o_pix + off:pk.o_pix + off + w * h * 4]).digest()), ("old", i))
        self.new = []  # (w, h, bytes)
        self.grey_of = {}

    def add(self, px):
        h, w = px.shape[:2]
        b = np.ascontiguousarray(px, np.uint8).tobytes()
        key = (w, h, hashlib.md5(b).digest())
        if key not in self.by_hash:
            self.by_hash[key] = ("new", len(self.new))
            self.new.append((w, h, b))
        return self.by_hash[key]

    def grey(self, t, kind=None):
        if (t, kind) not in self.grey_of:
            px = grey(self.pk.pixels(t))
            if kind:
                px, n = reshape(px, kind)
                print("    texture %d: %d shapes redrawn as our %s" % (t, n, kind), flush=True)
            self.grey_of[(t, kind)] = self.add(px)
        return self.grey_of[(t, kind)]


# ================================================================================================================ fx: recolour
def recolour_element(pk, frames, which, log):
    """frames of a source spark element -> the same frames, grey textures + our palette in the vertex colours.
    Each (texture, blend) group of the element gets ONE role (from its alpha-weighted mean vertex colour over all frames, so a
    group never changes colour mid-effect); every vertex keeps its own brightness (max of r g b) and, partly, its whiteness."""
    pal = {k: np.array(v, np.float64) for k, v in PAL[which].items()}
    acc = {}
    for fr in frames:
        for bt, v in fr:
            c = v["rgba"][:, :3].astype(np.float64)
            wgt = v["rgba"][:, 3].astype(np.float64) + 1e-3
            s = acc.setdefault((bt[0], bt[2]), [np.zeros(3), 0.0])
            s[0] += (c * wgt[:, None]).sum(0)
            s[1] += wgt.sum()
    roles = {}
    for (t, blend), (cs, ws) in acc.items():
        em = cs / max(ws, 1e-6)
        tab = ROLE_TAB[which]
        role = tab.get((t, blend), tab.get((t, None), "auto"))
        if role != "auto" and t != 0xFFFF and tuple(pk.tex[t][:2]) != TEX_WH.get(t):
            log("    NOTE: texture %d is %dx%d, not the expected %s: hue rule" % ((t,) + tuple(pk.tex[t][:2]) + (TEX_WH.get(t),)))
            role = "auto"
        if role == "auto":
            V, S = em.max(), (em.max() - em.min()) / max(em.max(), 1e-6)
            role = "accent" if (em[0] > em[2] + 15 and S > 0.3 and V >= 90) else "core"
        roles[(t, blend)] = role
    out = []
    for fr in frames:
        nf = []
        for bt, v in fr:
            t, blend = bt[0], bt[2]
            role = roles[(t, blend)]
            e = v["rgba"][:, :3].astype(np.float64)
            Vv = e.max(1)
            Sv = (Vv - e.min(1)) / np.maximum(Vv, 1e-6)
            sat = {"core": 0.55 + 0.45 * Sv, "deep": 0.85 + 0.15 * Sv, "accent": 0.8 + 0.2 * Sv}[role]
            P = pal[role] / pal[role].max()
            rgb = Vv[:, None] * ((1 - sat)[:, None] + sat[:, None] * P[None, :])
            nv = v.copy()
            nv["rgba"][:, :3] = np.clip(np.round(rgb), 0, 255).astype(np.uint8)
            rk = RESHAPE[which].get(t) if t != 0xFFFF and tuple(pk.tex[t][:2]) == TEX_WH.get(t) else None
            nf.append((bt, ("grey", t, rk) if t != 0xFFFF else None, nv))
        out.append(nf)
    log("    roles (tex/blend) -> %s" % ", ".join("%s/%d %s" % (t if t != 0xFFFF else "-", b, r) for (t, b), r in sorted(roles.items())))
    return out


def resolve_frames(tx, frames):
    """("grey", t) texture refs -> Textures refs"""
    out = []
    for fr in frames:
        out.append([(bt, None if r is None else tx.grey(r[1], r[2]) if r[0] == "grey" else r, v) for bt, r, v in fr])
    return out


# ================================================================================================================ fx: procedural
def mkverts(pts, uvs, rgba, off=None, age=0):
    """(only the procedural generators make verts here; their callers put them on the grid with qverts, review F2)"""
    n = len(pts)
    v = np.zeros(n, VERT)
    v["p"] = pts
    if off is not None:
        v["off"] = off
    v["uv"] = uvs
    v["age"] = age
    v["rgba"] = rgba
    return v


def strip(cent, nrm, width, rgba_a, color, age=0):
    """a band along the centre points (N+1, 3) with in-plane normals (N+1, 3) and widths (N+1,): triangle list"""
    n = len(cent) - 1
    o, i_ = cent + nrm * (width[:, None] / 2), cent - nrm * (width[:, None] / 2)
    pts, uvs, cols = [], [], []
    for k in range(n):
        u0, u1 = k / n, (k + 1) / n
        quad = [(o[k], (u0, 0.0), rgba_a[k]), (i_[k], (u0, 1.0), rgba_a[k]), (o[k + 1], (u1, 0.0), rgba_a[k + 1]),
                (i_[k + 1], (u1, 1.0), rgba_a[k + 1])]
        for q in (0, 1, 2, 1, 3, 2):
            pts.append(quad[q][0]), uvs.append(quad[q][1]), cols.append(tuple(color) + (int(qz(quad[q][2], Q_VAL)),))
    return mkverts(np.array(pts), np.array(uvs), np.array(cols), age=age)


def crescent(nseg=24, span=150.0, theta_m=np.radians(60), back=35.0):
    """the beam's centre line in KH effect space (x, y DOWN, z forward), cm: an arch facing +z"""
    th = np.linspace(-theta_m, theta_m, nseg + 1)
    R = span / 2 / np.sin(theta_m)
    up = R * np.cos(th) - R + 20.0   # top 20 cm above the point, the ends ~23 cm below it
    x = R * np.sin(th)
    z = 10.0 - back * (1 - np.cos(th)) / (1 - np.cos(theta_m))
    cent = np.stack([x, -up, z], 1)
    nrm = np.stack([np.sin(th), -np.cos(th), np.zeros_like(th)], 1)  # radial, in the x-y plane (y down)
    taper = np.cos(th / theta_m * np.pi / 2) ** 0.8
    return th, cent, nrm, taper


def quads_camera(pts, size, rgba, age):
    """camera-facing quads (flags 1: the offsets are camera space) at pts (n, 3), sizes (n,), rgba (n, 4), ages (n,)"""
    P, O, U, C, A = [], [], [], [], []
    corners = [(-1, -1, 0, 0), (1, -1, 1, 0), (-1, 1, 0, 1), (1, 1, 1, 1)]
    for p, s, c, ag in zip(pts, size, rgba, age):
        q = [(p, (cx * s, cy * s, 0.0), (u, w)) for cx, cy, u, w in corners]
        for k in (0, 1, 2, 1, 3, 2):
            P.append(q[k][0]), O.append(q[k][1]), U.append(q[k][2]), C.append(c), A.append(ag)
    v = mkverts(np.array(P), np.array(U), np.array(C), off=np.array(O))
    v["age"] = np.array(A)
    return v


def beam_frames(band, spark, nf=32):
    th, cent, nrm, taper = crescent()
    rng = np.random.default_rng(2167)
    ns = 14
    sp_t = rng.uniform(-0.9, 0.9, ns)          # where along the arc (fraction of theta_m)
    sp_r = rng.uniform(-0.5, 0.5, ns)          # across the band (fraction of the glow width)
    sp_age = rng.integers(0, 11, ns)           # lag along the flight path (frames)
    sp_n = rng.integers(1, 3, ns)              # twinkles per loop
    sp_ph = rng.random(ns)
    sp_sz = rng.uniform(4.0, 8.0, ns)
    frames = []
    for f in range(nf):
        ph = 2 * np.pi * f / nf
        fr = []
        wg = 34.0 * taper * (1 + 0.08 * np.sin(2 * ph))
        fr.append((band, 0, strip(cent, nrm, wg, 150 * taper ** 0.5, (120, 170, 255))))
        wc = 12.0 * taper
        ac = 255 * (0.9 + 0.1 * np.sin(3 * ph)) * taper ** 0.3
        fr.append((band, 0, strip(cent, nrm, wc, ac, (230, 246, 255))))
        for ag, k in ((2, 0.55), (4, 0.3), (6, 0.15)):  # ghosts: drawn at the owner's pose `ag` frames ago = along its path
            fr.append((band, 0, strip(cent, nrm, wc * (1 + 0.15 * ag / 2), ac * k, (150, 190, 255), age=ag)))
        th_k = sp_t * th[-1]
        idx = np.interp(th_k, th, np.arange(len(th)))
        pc = np.stack([np.interp(idx, np.arange(len(th)), cent[:, c]) for c in range(3)], 1)
        nk = np.stack([np.interp(idx, np.arange(len(th)), nrm[:, c]) for c in range(3)], 1)
        wk = np.interp(idx, np.arange(len(th)), wg)
        pts = pc + nk * (sp_r * wk)[:, None]
        tw = 0.5 + 0.5 * np.sin(2 * np.pi * (sp_n * f / nf + sp_ph))
        cols = np.array([(255, 212, 110, int(qz(255 * x, Q_VAL))) for x in tw])
        fr.append((spark, 1, quads_camera(pts, sp_sz * (0.7 + 0.3 * tw), cols, sp_age)))
        frames.append([(t, fl, qverts(v)) for t, fl, v in fr])
    return frames


def crescent_flat(nseg=24, span=150.0, theta_m=np.radians(60), back=35.0):
    """(s217) the swing-plane crescent's centre line, FLAT (roll 0): in the x-z plane of KH effect space (x right, y down,
    z forward), cm, its middle 10 cm ahead of the point, its ends 35 cm further back (convex side = +z = the travel direction,
    the sharp edge leads). -> th, cent, in-plane outward normal, taper"""
    th = np.linspace(-theta_m, theta_m, nseg + 1)
    R = span / 2 / np.sin(theta_m)
    k = back / (1 - np.cos(theta_m))
    cent = np.stack([R * np.sin(th), np.zeros_like(th), 10.0 - k * (1 - np.cos(th))], 1)
    tg = np.stack([R * np.cos(th), np.zeros_like(th), -k * np.sin(th)], 1)
    tg /= np.linalg.norm(tg, axis=1)[:, None]
    nrm = np.stack([-tg[:, 2], np.zeros_like(th), tg[:, 0]], 1)  # in the plane, outward (forward at the middle)
    taper = np.cos(th / theta_m * np.pi / 2) ** 0.8
    return th, cent, nrm, taper


def roll_verts(v, deg):
    """turn every point about the z (travel) axis by deg: the crescent's width goes from right (KH +x) toward up (KH -y).
    In sora_fx.c's world: width = cos(deg) x Sora's right + sin(deg) x up (right = (-cos yaw, 0, sin yaw) in BotW x, z).
    Camera-space offsets (the sparkles' billboard corners) are not turned."""
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    x, y = v["p"][:, 0].copy(), v["p"][:, 1].copy()
    v["p"][:, 0] = x * c + y * s
    v["p"][:, 1] = -x * s + y * c
    return v


def beam_flat_frames(band, spark, roll, nf=32):
    """(s217) kbs1_beam_r<roll>: the crescent in the swing plane. Two bands crossed at right angles (one in the crescent's plane,
    one across it) so it never vanishes edge-on: seen along the travel direction a flat one is a thin level line of light.
    The glow trails behind the sharp core (the leading edge); ghosts at age 2 / 4 / 6 (both bands) and 14 gold sparkles at
    age 0..10 trail along the flight path (sora_fx.c's pose ring), as kbs1_beam. Same colours as kbs1_beam. Loop, nf frames."""
    th, cent, nrm, taper = crescent_flat()
    up = np.tile(np.array([0.0, 1.0, 0.0]), (len(th), 1))  # across the plane (KH y, down; the band is symmetric)
    rng = np.random.default_rng(2167)
    ns = 14
    sp_t = rng.uniform(-0.9, 0.9, ns)          # where along the arc (fraction of theta_m)
    sp_r = rng.uniform(-0.5, 0.3, ns)          # across the band (fraction of the glow width; mostly on the trailing side)
    sp_y = rng.uniform(-0.5, 0.5, ns)          # across the plane (fraction of the cross glow width)
    sp_age = rng.integers(0, 11, ns)           # lag along the flight path (frames)
    sp_n = rng.integers(1, 3, ns)              # twinkles per loop
    sp_ph = rng.random(ns)
    sp_sz = rng.uniform(4.0, 8.0, ns)
    frames = []
    for f in range(nf):
        ph = 2 * np.pi * f / nf
        fr = []
        wg = 34.0 * taper * (1 + 0.08 * np.sin(2 * ph))            # glow in the plane: mostly BEHIND the core
        fr.append((band, 0, strip(cent - nrm * (0.3 * wg)[:, None], nrm, wg, 150 * taper ** 0.5, (120, 170, 255))))
        wx = 20.0 * taper * (1 + 0.08 * np.sin(2 * ph))            # glow across the plane
        fr.append((band, 0, strip(cent, up, wx, 120 * taper ** 0.5, (120, 170, 255))))
        wc = 12.0 * taper
        ac = 255 * (0.9 + 0.1 * np.sin(3 * ph)) * taper ** 0.3
        fr.append((band, 0, strip(cent, nrm, wc, ac, (230, 246, 255))))       # the sharp core
        wcx = 7.0 * taper
        fr.append((band, 0, strip(cent, up, wcx, ac, (230, 246, 255))))       # its cross
        for ag, k in ((2, 0.55), (4, 0.3), (6, 0.15)):  # ghosts: drawn at the owner's pose `ag` frames ago = along its path
            fr.append((band, 0, strip(cent, nrm, wc * (1 + 0.15 * ag / 2), ac * k, (150, 190, 255), age=ag)))
            fr.append((band, 0, strip(cent, up, wcx * (1 + 0.15 * ag / 2), ac * k, (150, 190, 255), age=ag)))
        th_k = sp_t * th[-1]
        idx = np.interp(th_k, th, np.arange(len(th)))
        pc = np.stack([np.interp(idx, np.arange(len(th)), cent[:, c]) for c in range(3)], 1)
        nk = np.stack([np.interp(idx, np.arange(len(th)), nrm[:, c]) for c in range(3)], 1)
        wk = np.interp(idx, np.arange(len(th)), wg)
        wxk = np.interp(idx, np.arange(len(th)), wx)
        pts = pc + nk * (sp_r * wk)[:, None] + np.array([0.0, 1.0, 0.0]) * (sp_y * wxk)[:, None]
        tw = 0.5 + 0.5 * np.sin(2 * np.pi * (sp_n * f / nf + sp_ph))
        cols = np.array([(255, 212, 110, int(qz(255 * x, Q_VAL))) for x in tw])
        fr.append((spark, 1, quads_camera(pts, sp_sz * (0.7 + 0.3 * tw), cols, sp_age)))
        frames.append([(t, fl, qverts(roll_verts(v, roll))) for t, fl, v in fr])   # (review F2: after the roll, whose
        # near-cancellations x cos r - y sin r become exact 0)
    return frames


def ring_batch(f, n, band, r0, r1, w0, col, a0, delay=0):
    """a flat expanding ring on the ground plane (x-z) at frame f of n"""
    k = (f - delay) / max(1, n - 1 - delay)
    if k < 0 or k > 1:
        return None
    r = r0 + (r1 - r0) * k ** 0.6
    seg = 40
    phi = np.linspace(0, 2 * np.pi, seg + 1)
    cent = np.stack([r * np.cos(phi), np.zeros_like(phi), r * np.sin(phi)], 1)
    nrm = np.stack([np.cos(phi), np.zeros_like(phi), np.sin(phi)], 1)
    w = np.full(seg + 1, w0 * (1 - 0.5 * k))
    a = np.full(seg + 1, a0 * (1 - k) ** 1.3)
    return (band, 0, qverts(strip(cent, nrm, w, a, col)))


# ================================================================================================================ fx: writing
def batch_rec(tex, flags, blend, clamp, first, n, upright=(0.0,) * 9):
    return [tex, flags, blend, clamp, first, n, upright]


def build_fx(src_path, dst, log):
    pk = Pack(src_path)
    for n in NEW_FX:
        if n in pk.eff:
            die("the pack already has %s (run on the pack BEFORE this step)" % n)
    need = ["kb%s_%s" % (SRC[w], k) for w in SRC for k in KINDS + ["trail"]]
    miss = [n for n in need if n not in pk.eff]
    if miss:
        die("the pack has no %s (the s89 per-Keyblade sets of fxbake.py): cannot shape our sets" % ", ".join(miss))
    old_last = pk.ntex - 1
    last = pk.tex[old_last]
    if last[2] != max(t[2] for t in pk.tex) or last[2] + last[0] * last[1] * 4 != pk.pixb:
        die("the pack's last texture is not the ribbon gradient at the end of pix")
    bt_tex = np.frombuffer(pk.mm[pk.o_ba:pk.o_ba + 52 * pk.nba], np.uint8).reshape(pk.nba, 52)[:, :2].copy().view("<u2").ravel()
    if (bt_tex == old_last).any():
        die("an old batch uses the last texture (the gradient): it would need renumbering")

    tx = Textures(pk)
    elements = []  # (name, frames [(texref|None, flags, blend, clamp, upright, verts)], loops, pax, bone, fade)

    def from_src(nm):
        return [[(r, bt[1], bt[2], bt[3], bt[9:18], v) for bt, r, v in fr] for fr in nm]

    for which in ("s1", "g1"):
        for k in KINDS:
            sname = "kb%s_%s" % (SRC[which], k)
            log("  kb%s_%s <- %s" % (which, k, sname))
            frs = resolve_frames(tx, recolour_element(pk, pk.element(sname), which, log))
            e = pk.eff[sname]
            elements.append(("kb%s_%s" % (which, k), from_src(frs), 0, e[3], e[4], e[5]))
        sname = "kb%s_trail" % SRC[which]
        src = pk.element(sname)
        bt, v = src[0][0]
        if bt[0] == 0xFFFF:
            die("%s has no texture" % sname)
        tref = tx.add(trail_tex(pk.pixels(bt[0]), which))
        frs = []
        for fr in src:
            nf = []
            for b, vv in fr:
                vv = vv.copy()
                vv["rgba"][:, :3] = 255
                nf.append((tref, b[1], b[2], b[3], b[9:18], vv))
            frs.append(nf)
        e = pk.eff[sname]
        elements.append(("kb%s_trail" % which, frs, 0, e[3], e[4], e[5]))
        log("  kb%s_trail <- %s (texture %d -> our ribbon)" % (which, sname, bt[0]))
    band, spark = tx.add(band_tex()), tx.add(sparkle_tex())
    bf = beam_frames(band, spark)
    elements.append(("kbs1_beam", [[(t, fl, 0, 1, (0.0,) * 9, v) for t, fl, v in fr] for fr in bf], 1, 0, 0, 0))
    log("  kbs1_beam: procedural crescent, %d frames (loop)" % len(bf))
    fin = [e for e in elements if e[0] == "kbs1_hit_finish"][0]
    hf = []
    nfin = len(fin[1])
    for f, fr in enumerate(fin[1]):
        nf = []
        for t, fl, bl, cl, up, v in fr:
            v = v.copy()
            v["p"] *= 1.5
            v["off"] *= 1.5
            nf.append((t, fl, bl, cl, up, v))
        for rb in (ring_batch(f, 24, band, 20, 160, 22, (120, 170, 255), 220),
                   ring_batch(f, 30, band, 10, 120, 8, (255, 206, 96), 230, delay=3)):
            if rb:
                nf.append((rb[0], 0, rb[1], 1, (0.0,) * 9, rb[2]))
        hf.append(nf)
    elements.append(("kbs1_beam_hit", hf, 0, fin[3], fin[4], fin[5]))
    log("  kbs1_beam_hit: kbs1_hit_finish x1.5 + 2 rings, %d frames" % nfin)
    for r, nm in zip(BEAM_ROLLS, BEAM_ROLL_FX):  # (s217: after every s216 row, so those keep their indices and bytes)
        bf = beam_flat_frames(band, spark, r)
        elements.append((nm, [[(t, fl, 0, 1, (0.0,) * 9, v) for t, fl, v in fr] for fr in bf], 1, 0, 0, 0))
    log("  kbs1_beam_r000..r%03d: the swing-plane crescent, %d rolls x %d frames (loop)" % (BEAM_ROLLS[-1], len(BEAM_ROLLS), len(bf)))

    # ---- the new rows
    k_new = len(tx.new)
    newb, newv, newf, newe = bytearray(), bytearray(), bytearray(), bytearray()
    nv_tot = 0
    for name, frs, loops, pax, bone, fade in elements:
        first_frame = pk.nfr + len(newf) // 8
        maxv = 0
        for fr in frs:
            first_batch = pk.nba + len(newb) // 52
            fv = 0
            for tref, fl, bl, cl, up, v in fr:
                t = 0xFFFF if tref is None else (tref[1] if tref[0] == "old" else old_last + tref[1])
                first_vert = pk.nve + nv_tot
                newb += OUT_BATCH.pack(t, fl, bl, cl, 0, 0, 0, first_vert, len(v), *up)
                newv += np.ascontiguousarray(v).tobytes()
                nv_tot += len(v)
                fv += len(v)
            newf += OUT_FRAME.pack(first_batch, len(fr))
            maxv = max(maxv, fv)
        newe += OUT_EFF.pack(name.encode(), first_frame, len(frs), loops, pax, bone, fade, maxv, 0)
        log("  + %-15s %3d frames%s, max %5d verts/frame, pax %#x bone %#x" % (name, len(frs), " (loop)" if loops else "", maxv, pax, bone))
    # ---- write (as chickfx.build_fx: new textures before the gradient)
    mm = pk.mm
    last_off = last[2]
    npix = bytearray()
    new_texrecs = []
    for (w, h, b) in tx.new:
        new_texrecs.append(OUT_TEX.pack(w, h, last_off + len(npix), 0))
        npix += b
    tex_tbl = mm[pk.o_tex:pk.o_tex + 16 * old_last] + b"".join(new_texrecs) + OUT_TEX.pack(last[0], last[1], last_off + len(npix), 0)
    ntex2, pix2 = pk.ntex + k_new, pk.pixb + len(npix)
    nfr2, nba2, nve2, neff2 = pk.nfr + len(newf) // 8, pk.nba + len(newb) // 52, pk.nve + len(newv) // 40, pk.neff + len(newe) // 64
    sizes = [ntex2 * 16, neff2 * 64, nfr2 * 8, nba2 * 52, nve2 * 40, pk.ntr * 12, pix2]
    offs, q = [], OUT_HDR.size
    for s in sizes:
        offs.append(q)
        q += s
        q += -q % 16
    if q > (1 << 32) - 1:
        die("the new pack passes the uint32 cap")
    hdr = OUT_HDR.pack(b"SKFX", 1, ntex2, neff2, nfr2, nba2, nve2, pk.ntr, *offs, pix2)

    def copy_range(fo, a, b):
        while a < b:
            n = min(b - a, 32 << 20)
            fo.write(mm[a:a + n])
            a += n
    tmp = dst + ".tmp"
    with open(tmp, "wb") as fo:
        fo.write(hdr)
        parts = [lambda: fo.write(tex_tbl),
                 lambda: (copy_range(fo, pk.o_eff, pk.o_eff + 64 * pk.neff), fo.write(newe)),
                 lambda: (copy_range(fo, pk.o_fr, pk.o_fr + 8 * pk.nfr), fo.write(newf)),
                 lambda: (copy_range(fo, pk.o_ba, pk.o_ba + 52 * pk.nba), fo.write(newb)),
                 lambda: (copy_range(fo, pk.o_ve, pk.o_ve + 40 * pk.nve), fo.write(newv)),
                 lambda: copy_range(fo, pk.o_tr, pk.o_tr + 12 * pk.ntr),
                 lambda: (copy_range(fo, pk.o_pix, pk.o_pix + last_off), fo.write(npix), copy_range(fo, pk.o_pix + last_off, pk.o_pix + pk.pixb))]
        for off_, fn in zip(offs, parts):
            fo.write(bytes(off_ - fo.tell()))
            fn()
    os.replace(tmp, dst)
    log("%s: %.1f MB (was %.1f MB): +%d elements, +%d frames, +%d batches, +%d verts, textures %d -> %d (%d new, gradient now #%d)" %
        (dst, os.path.getsize(dst) / 1e6, len(mm) / 1e6, len(newe) // 64, len(newf) // 8, len(newb) // 52, len(newv) // 40,
         pk.ntex, ntex2, k_new, ntex2 - 1))
    pk.close()
    verify_fx(src_path, dst, log)


def verify_fx(old, new, log):
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
    if pn.ntr != po.ntr:
        die("VERIFY FAILED: trail window count changed")
    for b in range(po.nba, pn.nba):
        t, fl, bl, cl, _, _, _, fv, nv = struct.unpack_from("<HBBB3BII", mn, pn.o_ba + b * 52)
        if not (t == 0xFFFF or t < pn.ntex - 1) or fv + nv > pn.nve or bl > 2:
            die("VERIFY FAILED: new batch %d out of range" % b)
    for fi in range(po.nfr, pn.nfr):
        fb, n_ = struct.unpack_from("<II", mn, pn.o_fr + fi * 8)
        if fb + n_ > pn.nba:
            die("VERIFY FAILED: new frame %d out of range" % fi)
    for nm in NEW_FX:
        if nm not in pn.eff:
            die("VERIFY FAILED: %s missing" % nm)
        ff, nf = pn.eff[nm][0], pn.eff[nm][1]
        if nf < 1 or ff + nf > pn.nfr:
            die("VERIFY FAILED: %s frames out of range" % nm)
    if set(pn.eff) - set(po.eff) != set(NEW_FX):
        die("VERIFY FAILED: unexpected new names %s" % sorted(set(pn.eff) - set(po.eff) - set(NEW_FX)))
    log("VERIFIED fx: old tex (%d) / eff (%d) / frame (%d) / batch (%d) / vert (%d) / trail (%d) rows and all old pixels (%d B) "
        "byte-identical; the gradient is still the last texture; %d new elements by name, every new row in range" %
        (po.ntex - 1, po.neff, po.nfr, po.nba, po.nve, po.ntr, ol[2], len(NEW_FX)))
    po.close()
    pn.close()


# ================================================================================================================ sfx
def read_wav(path):
    if not os.path.isfile(path):
        die("missing our sound %s (tools/sorabuild/ours/kb/kbsynth.py makes it)" % path)
    try:
        with wave.open(path, "rb") as w:
            sw, rate, ch, n = w.getsampwidth(), w.getframerate(), w.getnchannels(), w.getnframes()
            fr = w.readframes(n)
    except (wave.Error, EOFError) as e:
        die("%s is not a readable WAV (%s)" % (path, e))
    if sw != 2 or ch not in (1, 2) or not 8000 <= rate <= 96000 or not 2 <= n <= rate * 10 or len(fr) != n * ch * 2:
        die("%s: want PCM16, 1-2 channels, 8-96 kHz, under 10 s (got %d B/sample, %d ch, %d Hz, %d frames)" % (path, sw, ch, rate, n))
    return rate, ch, n, fr


def build_sfx(src, dst, ours, log):
    if not os.path.isfile(src):
        die("no sfx pack at %s" % src)
    d = open(src, "rb").read()
    if len(d) < 48:
        die("%s is too small for an SKSF pack" % src)
    magic, ver, ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes, _ = SK.unpack_from(d, 0)
    if magic != b"SKSF" or ver != 1:
        die("%s is not an SKSF v1 pack" % src)
    o, sec = 48, {}
    for nm, cnt, sz in (("sounds", ns, 72), ("trig", nt, 28), ("ts", nts, 4), ("vt", nv, 16), ("ch", nc, 8), ("names", nn, 36), ("hits", nh, 8)):
        sec[nm] = d[o:o + cnt * sz]
        o += cnt * sz
    if o > data_off or data_off + data_bytes > len(d):
        die("%s: sections overlap / cut short" % src)
    names = {sec["names"][i * 36:i * 36 + 32].split(b"\0")[0].decode("latin1") for i in range(nn)}
    if SFX_MARKER in names or any(s in names for s in SOUNDS):
        die("the sfx pack already has our Keyblade sounds (run on the pack BEFORE this step)")
    pcm = bytearray(d[data_off:data_off + data_bytes])
    new_s, new_names = bytearray(), []
    for i, nm in enumerate(SOUNDS):
        rate, ch, n, fr = read_wav(os.path.join(ours, nm + ".wav"))
        while len(pcm) % 4:
            pcm.append(0)
        new_s += struct.pack("<48sIIfIII", ("ours/kb/%s.wav" % nm).encode(), rate, ch, 1.0, 0, len(pcm), n)
        pcm += fr
        new_names.append((nm, ns + i))
        log("  + sound %-14s #%d: %.3f s, %d Hz, %d ch" % (nm, ns + i, n / rate, rate, ch))
    names_b = b"".join(struct.pack("<32sI", n.encode(), s) for n, s in new_names) + struct.pack("<32sI", SFX_MARKER.encode(), ns)
    body = bytearray(sec["sounds"] + bytes(new_s) + sec["trig"] + sec["ts"] + sec["vt"] + sec["ch"] + sec["names"] + names_b + sec["hits"])
    while (48 + len(body)) % 16:
        body.append(0)
    hdr = SK.pack(b"SKSF", 1, ns + len(SOUNDS), nt, nts, nv, nc, nn + len(new_names) + 1, nh, 48 + len(body), len(pcm), 0)
    tmp = dst + ".tmp"
    with open(tmp, "wb") as f:
        f.write(hdr + body + pcm)
    os.replace(tmp, dst)
    # verify: old rows + old PCM byte-identical, the new names resolve
    n_ = open(dst, "rb").read()
    h2 = SK.unpack_from(n_, 0)
    o2 = 48
    for nm, cnt_old, cnt_new, sz in (("sounds", ns, h2[2], 72), ("trig", nt, h2[3], 28), ("ts", nts, h2[4], 4), ("vt", nv, h2[5], 16),
                                     ("ch", nc, h2[6], 8), ("names", nn, h2[7], 36), ("hits", nh, h2[8], 8)):
        if n_[o2:o2 + cnt_old * sz] != sec[nm]:
            die("VERIFY FAILED: old sfx %s rows differ" % nm)
        o2 += cnt_new * sz
    if h2[3:7] != (nt, nts, nv, nc) or h2[8] != nh:
        die("VERIFY FAILED: trigger / voice / choice / hit counts changed")
    if n_[h2[9]:h2[9] + data_bytes] != d[data_off:data_off + data_bytes]:
        die("VERIFY FAILED: old PCM differs")
    nmap = {}
    base = 48 + 72 * h2[2] + 28 * h2[3] + 4 * h2[4] + 16 * h2[5] + 8 * h2[6]
    for i in range(h2[7]):
        a, s = struct.unpack_from("<32sI", n_, base + 36 * i)
        nmap.setdefault(a.split(b"\0")[0].decode("latin1"), s)
    for k, (nm, s) in enumerate(new_names):
        if nmap.get(nm) != s:
            die("VERIFY FAILED: name %s does not resolve to sound %d" % (nm, s))
    log("%s: %.1f MB (was %.1f MB), +%d sounds, +%d names" % (dst, len(n_) / 1e6, len(d) / 1e6, len(SOUNDS), len(new_names) + 1))
    log("VERIFIED sfx: old sound (%d) / trigger (%d) / trig-sound (%d) / vtbl (%d) / choice (%d) / name (%d) / hit (%d) rows and the "
        "old PCM (%d B) byte-identical; %s resolve by name" % (ns, nt, nts, nv, nc, nn, nh, data_bytes, ", ".join(SOUNDS)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fx-in", required=True)
    ap.add_argument("--sfx-in", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--ours", required=True, help="tools/sorabuild/ours/kb (our synthesised WAVs)")
    ap.add_argument("--no-fx", action="store_true")
    ap.add_argument("--no-sfx", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)

    def log(m):
        print(m, flush=True)
    if not a.no_sfx:  # (sfx first: cheap, and it checks our WAVs before the big fx write)
        build_sfx(a.sfx_in, os.path.join(a.out, "sfx.bin"), a.ours, log)
    if not a.no_fx:
        build_fx(a.fx_in, os.path.join(a.out, "fx.bin"), log)


if __name__ == "__main__":
    try:
        main()
    except SystemExit:
        raise
    except Exception:  # never an unhandled crash: say what failed, exit 1
        traceback.print_exc()
        raise SystemExit("[kbownfx] failed (see above)")

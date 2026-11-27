"""netgear (s289): the BotW melee weapon a friend holds, drawn on his Sora puppet in a room. This bakes each weapon's model
from the player's OWN BotW romfs into the mod's draw format, at install time (nothing from BotW is in the repo or a package):
  romfs/Actor/Pack/<actor>.sbactorpack   its ModelList (.bmodellist): which bfres folder + which model (FMDL) the actor draws
  romfs/Model/<folder>.sbfres            the FMDL: LOD 0 triangles, positions, uvs, the material's albedo name
  romfs/Model/<folder>.Tex.sbfres        the BNTX: the albedo texture (BC1..BC7 / RGBA8, block-linear)

  python -E -P gear_bake.py <romfs dir> <out dir> [--tex N] [--only A,B,...]

Writes, for every melee weapon actor of the romfs (Actor/Pack/Weapon_Sword_* / Weapon_Lsword_* / Weapon_Spear_*):
  <out>/gear/<actor>.bin   'SKRG' v1 (the layout of tools/sorabuild/cs/partybake Program.cs "sora_model.bin"): ONE bone = the
                           BotW model's own frame (x = the guard, y = the flat side, z = along the blade), positions in cm
                           (metres x 100), one group per shape, BGRA8 textures shrunk to <= N px (default 256)
  <out>/gear/gear_pose.bin 'SKRP' v1: one slot (0), one pose, the identity bone (sora_draw's loader needs a pose file whose
                           bone count matches; the DLL draws the weapon with sora_draw_actor_world, never this pose)
  <out>/gear/gear_list.txt one line per actor: baked (verts, tris, textures) or skipped (why)
Display only: the runtime loads these as draw actors (no BotW actor, nothing pickable, no physics, no save).
Deterministic: integer box shrink (Image.reduce), no float resampling; the kit's numpy 2.5.3 / Pillow 12.3.0 / oead 1.3.0.
"""
import os
import struct
import sys

import numpy as np
import oead
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "logo"))
import bntx  # noqa: E402  (the logo builder's BNTX reader: BRTI fields + mip offsets)
import swz  # noqa: E402   (its block-linear deswizzle)

PREFIXES = ("Weapon_Sword_", "Weapon_Lsword_", "Weapon_Spear_")
MAX_VERTS = 200000  # (sanity: the biggest melee LOD 0 is a few thousand)
MAX_TEX = 8         # (sora_draw MAX_TEX 80, MAX_GROUPS 160: far below)


# ---------------------------------------------------------------- Switch BFRES (v0.5, BotW 1.6+ / Savage Wind romfs)
class Bfres:
    def __init__(self, d):
        if d[:8] != b"FRES    ":
            raise ValueError("not a Switch BFRES")
        self.d = d
        bi = self.q(0x90)
        self.buf_size, self.buf_off = self.u(bi + 4), self.q(bi + 8)

    def q(self, o): return struct.unpack_from("<Q", self.d, o)[0]
    def u(self, o): return struct.unpack_from("<I", self.d, o)[0]
    def h(self, o): return struct.unpack_from("<H", self.d, o)[0]
    def f(self, o, n): return struct.unpack_from("<%df" % n, self.d, o)

    def s(self, o):
        if not o:
            return ""
        n = self.h(o)
        return self.d[o + 2:o + 2 + n].decode("ascii", "replace")

    def dict_keys(self, o):  # a _DIC: u32 magic, s32 count, (count + 1) nodes of 16 B (node 0 = the root)
        if not o or self.d[o:o + 4] != b"_DIC":
            return []
        n = struct.unpack_from("<i", self.d, o + 4)[0]
        return [self.s(self.q(o + 8 + 16 * (i + 1) + 8)) for i in range(n)]

    def models(self):
        arr, n = self.q(0x28), self.h(0xbc)
        out = []
        for i in range(n):
            m = arr + 0x78 * i
            if self.d[m:m + 4] != b"FMDL":
                raise ValueError("FMDL %d: bad magic" % i)
            out.append((self.s(self.q(m + 0x10)), m))
        return out


# (layout byte -> (components, bits)); type byte: 1 unorm, 2 snorm, 3 uint, 4 sint, 5 float
_LAYOUT = {0x02: (1, 8), 0x09: (2, 8), 0x0b: (4, 8), 0x0a: (1, 16), 0x12: (2, 16), 0x15: (4, 16),
           0x14: (1, 32), 0x17: (2, 32), 0x18: (3, 32), 0x19: (4, 32)}


def decode_attr(raw, fmt, count, stride, off):
    """raw = the vertex buffer's bytes; -> float32 (count, comps)"""
    lay, typ = fmt >> 8, fmt & 0xff
    if lay not in _LAYOUT or typ not in (1, 2, 3, 4, 5):
        raise ValueError("vertex format %#06x not read" % fmt)
    comps, bits = _LAYOUT[lay]
    nb = bits // 8
    dt = {(1, 1): np.uint8, (1, 2): np.int8, (1, 3): np.uint8, (1, 4): np.int8, (2, 1): np.uint16, (2, 2): np.int16,
          (2, 3): np.uint16, (2, 4): np.int16, (2, 5): np.float16, (4, 1): np.uint32, (4, 2): np.int32, (4, 3): np.uint32,
          (4, 4): np.int32, (4, 5): np.float32}.get((nb, typ))
    if dt is None:
        raise ValueError("vertex format %#06x not read" % fmt)
    a = np.frombuffer(raw, np.uint8, count=count * stride).reshape(count, stride)[:, off:off + comps * nb]
    v = np.ascontiguousarray(a).view(dt).reshape(count, comps).astype(np.float64)
    if typ == 1:
        v /= float((1 << bits) - 1)
    elif typ == 2:
        v = np.maximum(v / float((1 << (bits - 1)) - 1), -1.0)
    return v.astype(np.float32)


def euler_xyz(r):
    cx, sx, cy, sy, cz, sz = np.cos(r[0]), np.sin(r[0]), np.cos(r[1]), np.sin(r[1]), np.cos(r[2]), np.sin(r[2])
    rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return rz @ ry @ rx


def quat(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def bones_world(bf, skl):
    """-> list of 4x4 bind matrices (column vectors, model space), the matrix -> bone list"""
    n, arr = bf.h(skl + 0x3c), bf.q(skl + 0x18)
    nsm, nrg = bf.h(skl + 0x3e), bf.h(skl + 0x40)
    loc, par = [], []
    for i in range(n):
        b = arr + 0x50 * i
        flags = bf.u(b + 0x24)
        s, r, t = bf.f(b + 0x28, 3), bf.f(b + 0x34, 4), bf.f(b + 0x44, 3)
        m = np.eye(4)
        m[:3, :3] = (euler_xyz(r[:3]) if flags & 0x1000 else quat(r)) @ np.diag(s)
        m[:3, 3] = t
        loc.append(m)
        p = bf.h(b + 0x1a)
        par.append(-1 if p == 0xffff else p)
    world = [None] * n
    for i in range(n):  # (each bone's chain up to a resolved parent; a parent index past the list / a cycle ends the chain)
        chain, j = [], i
        while j >= 0 and world[j] is None and len(chain) <= n:
            chain.append(j)
            j = par[j] if par[j] < n else -1
        base = world[j] if j >= 0 and world[j] is not None else np.eye(4)
        for k in reversed(chain):
            base = base @ loc[k]
            world[k] = base
    m2b = list(struct.unpack_from("<%dH" % (nsm + nrg), bf.d, bf.q(skl + 0x20))) if nsm + nrg else []
    return world, m2b


def render_info(bf, mat):
    arr, n = bf.q(mat + 0x18), bf.h(mat + 0xa6)
    out = {}
    for i in range(n):
        e = arr + 0x18 * i
        name, data, cnt, typ = bf.s(bf.q(e)), bf.q(e + 8), bf.h(e + 0x10), bf.d[e + 0x12]
        if typ == 2 and cnt:
            out[name] = bf.s(bf.q(data))
    return out


def material(bf, mat):
    ntex = bf.d[mat + 0xa8]
    names = [bf.s(bf.q(bf.q(mat + 0x38) + 8 * i)) for i in range(ntex)]
    samp = bf.dict_keys(bf.q(mat + 0x50))
    alb = None
    if "_a0" in samp and samp.index("_a0") < len(names):
        alb = names[samp.index("_a0")]
    if alb is None:
        alb = next((t for t in names if t.endswith("_Alb")), names[0] if names else None)
    ri = render_info(bf, mat)
    mode = ri.get("gsys_render_state_mode", "opaque")
    atest = ri.get("gsys_alpha_test_enable", "false") == "true"
    return {"name": bf.s(bf.q(mat + 0x10)), "albedo": alb, "mode": mode, "atest": atest, "textures": names, "samplers": samp,
            "color0": shader_param(bf, mat, "const_color0")}


def shader_param(bf, mat, want):
    """a float shader param of the material (array of 0x20: u64 name at +8, u8 size at +0x11, u16 offset at +0x12 into the
    material's param source at +0x68); None = not there"""
    arr, n, src = bf.q(mat + 0x58), bf.h(mat + 0xaa), bf.q(mat + 0x68)
    for i in range(n):
        e = arr + 0x20 * i
        if bf.s(bf.q(e + 8)) == want:
            sz, off = bf.d[e + 0x11], bf.h(e + 0x12)
            return bf.f(src + off, sz // 4) if sz and sz % 4 == 0 else None
    return None


def model_mesh(bf, m):
    """FMDL at m -> list of shapes: dict(name, mat, pos (n,3) model metres, uv (n,2), idx (k,) into pos)"""
    skl, vbs, shp, mats = bf.q(m + 0x20), bf.q(m + 0x28), bf.q(m + 0x30), bf.q(m + 0x40)
    nsh, nmat = bf.h(m + 0x6a), bf.h(m + 0x6c)
    world, m2b = bones_world(bf, skl)
    mlist = [material(bf, mats + 0xb8 * i) for i in range(nmat)]
    out = []
    for si in range(nsh):
        o = shp + 0x70 * si
        name, vb = bf.s(bf.q(o + 0x10)), bf.q(o + 0x18)
        mi, bone, skin, nmesh = bf.h(o + 0x5e), bf.h(o + 0x60), bf.d[o + 0x66], bf.d[o + 0x67]
        if not nmesh:
            continue
        na, nbuf, vbase, vc = bf.d[vb + 0x54], bf.d[vb + 0x55], bf.u(vb + 0x50), bf.u(vb + 0x58)
        if vc > MAX_VERTS:
            raise ValueError("shape %s: %d verts" % (name, vc))
        sz, st = bf.q(vb + 0x38), bf.q(vb + 0x40)
        starts, a = [], bf.buf_off + vbase
        for b in range(nbuf):
            starts.append(a)
            a = (a + bf.u(sz + 0x10 * b) + 7) & ~7
        attrs = {}
        for ai in range(na):
            e = bf.q(vb + 0x10) + 0x10 * ai
            attrs[bf.s(bf.q(e))] = (bf.h(e + 8), bf.h(e + 0xc), bf.h(e + 0xe))
        if "_p0" not in attrs:
            continue

        def get(nm):
            fmt, off, b = attrs[nm]
            stride = bf.u(st + 0x10 * b)
            return decode_attr(bf.d[starts[b]:starts[b] + stride * vc], fmt, vc, stride, off)
        pos = get("_p0")[:, :3].astype(np.float64)
        uv = get("_u0")[:, :2] if "_u0" in attrs else np.zeros((vc, 2), np.float32)
        # skin 0: rigid on the shape's bone; 1: each vertex on its own bone (_i0 -> the matrix list); 2+: model space already
        if skin == 0 and bone < len(world):
            w = world[bone]
            pos = pos @ w[:3, :3].T + w[:3, 3]
        elif skin == 1 and "_i0" in attrs and m2b:
            ix = get("_i0")[:, 0].astype(np.int64)
            for v in range(vc):
                bi = m2b[ix[v]] if 0 <= ix[v] < len(m2b) else 0
                w = world[bi] if bi < len(world) else np.eye(4)
                pos[v] = w[:3, :3] @ pos[v] + w[:3, 3]
        me = bf.q(o + 0x20)  # mesh 0 = LOD 0
        fo, prim, ifmt, icnt, first = bf.u(me + 0x20), bf.u(me + 0x24), bf.u(me + 0x28), bf.u(me + 0x2c), bf.u(me + 0x30)
        if prim != 3:
            raise ValueError("shape %s: primitive %d is not a triangle list" % (name, prim))
        it = {0: np.uint8, 1: np.uint16, 2: np.uint32}.get(ifmt)
        if it is None or icnt % 3:
            raise ValueError("shape %s: index format %d, count %d" % (name, ifmt, icnt))
        idx = np.frombuffer(bf.d, it, count=icnt, offset=bf.buf_off + fo).astype(np.int64) + first
        if not icnt:
            continue
        if idx.min() < 0 or idx.max() >= vc:
            raise ValueError("shape %s: index past its %d verts" % (name, vc))
        used, idx = np.unique(idx, return_inverse=True)  # (only LOD 0's vertices: the lower LODs share the buffer)
        out.append({"name": name, "mat": mlist[mi] if mi < len(mlist) else None, "pos": pos[used], "uv": uv[used],
                    "idx": idx.reshape(-1), "skin": skin, "fmts": {k: v[0] for k, v in attrs.items()}, "nmesh": nmesh})
    return out


# ---------------------------------------------------------------- BNTX textures
_BCN = {0x1a: (1, 8), 0x1b: (2, 16), 0x1c: (3, 16), 0x1d: (4, 8), 0x1e: (5, 16), 0x20: (7, 16)}


def decode_texture(bn, t):
    """-> RGBA uint8 (h, w, 4), mip 0"""
    hi, w, h, blk = t["fmt"] >> 8, t["w"], t["h"], t["blk"] & 7
    raw = bn[t["offs"][0]:]
    if hi in _BCN:
        n, bpb = _BCN[hi]
        wb, hb = (w + 3) // 4, (h + 3) // 4
        lin = swz.deswizzle(raw, wb, hb, bpb, blk)
        mode = "L" if n == 4 else "RGB" if n == 5 else "RGBA"
        img = Image.frombytes(mode, (wb * 4, hb * 4), lin.tobytes(), "bcn", n).crop((0, 0, w, h))
        return np.asarray(img.convert("RGBA"))  # (BC5: r, g, 0, 255 - the caller reads it as an energy-blade mask)
    if hi == 0x0b:  # R8G8B8A8
        return swz.deswizzle(raw, w, h, 4, blk).reshape(h, w, 4).copy()
    raise ValueError("texture %s: format %#06x not read" % (t["name"], t["fmt"]))


def energy(rgba, c0):
    """a BC5 energy-blade mask -> its colour (the material's const_color0, an HDR colour: clipped, a third of the way to
    white so it reads as a glow under the draw's light) where r says blade (alpha = r: the opaque pass cuts at 0.5)"""
    c = np.clip(np.array(c0[:3] if c0 and len(c0) >= 3 else (0.1, 0.6, 1.0), np.float64), 0, 1)
    c = c + (1 - c) * 0.35
    g = rgba[..., 1:2].astype(np.float64) / 255.0
    rgb = np.clip(c * (0.75 + 0.25 * g) * 255.0 + 0.5, 0, 255).astype(np.uint8)
    return np.concatenate([rgb, rgba[..., 0:1]], 2)


def shrink(rgba, side):
    h, w = rgba.shape[:2]
    f = 1
    while max(w, h) // f > side and (w // f) % 2 == 0 and (h // f) % 2 == 0:
        f *= 2
    if f == 1:
        return rgba
    return np.asarray(Image.fromarray(rgba, "RGBA").reduce(f))


# ---------------------------------------------------------------- the mod's draw format
def skrg(shapes, texs):
    """shapes: list of (tex index, attr, pos cm (n,3), uv (n,2), idx); texs: list of BGRA (h, w, 4) -> bytes"""
    nv = sum(len(s[2]) for s in shapes)
    ni = sum(len(s[4]) for s in shapes)
    hdr = struct.pack("<4s7I", b"SKRG", 1, 1, len(shapes), len(texs), nv, nv, ni)
    body = 32 + 16 * len(texs) + 16 * len(shapes) + 16 * nv + 20 * nv + 4 * ni
    tex_tab, tex_dat, o = b"", b"", (body + 15) & ~15
    for t in texs:
        h, w = t.shape[:2]
        tex_tab += struct.pack("<4I", w, h, o, 0)
        tex_dat += t.tobytes() + b"\0" * ((-t.nbytes) & 15)
        o += t.nbytes + ((-t.nbytes) & 15)
    grp, vert, infl, idx = [], [], [], []
    vb, ib = 0, 0
    for ti, attr, pos, uv, ix in shapes:
        grp.append(struct.pack("<4I", ti, attr, ib, len(ix)))
        n = len(pos)
        vv = np.zeros(n, dtype=[("fi", "<u4"), ("ni", "<u4"), ("u", "<f4"), ("v", "<f4")])
        vv["fi"], vv["ni"], vv["u"], vv["v"] = np.arange(vb, vb + n), 1, uv[:, 0], uv[:, 1]
        vert.append(vv.tobytes())
        ii = np.zeros(n, dtype=[("b", "<u4"), ("p", "<f4", 4)])
        ii["p"][:, :3], ii["p"][:, 3] = pos, 1.0
        infl.append(ii.tobytes())
        idx.append((ix + vb).astype("<u4").tobytes())
        vb, ib = vb + n, ib + len(ix)
    out = hdr + tex_tab + b"".join(grp) + b"".join(vert) + b"".join(infl) + b"".join(idx)
    out += b"\0" * (((body + 15) & ~15) - len(out))
    return out + tex_dat


def skrp_identity():
    return struct.pack("<4s3I", b"SKRP", 1, 1, 1) + struct.pack("<HHI", 0, 1, 24) + struct.pack("<12f", 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0)


# ---------------------------------------------------------------- one actor
def model_of(romfs, actor):
    """-> (bfres folder, FMDL name) from the actor pack's ModelList (an AAMP: ModelData_0 / Base.Folder, Unit / Unit_0.UnitName);
    fallback: the actor's own name for both (every BotW 1.9.0 melee weapon is named so)"""
    p = os.path.join(romfs, "Actor", "Pack", actor + ".sbactorpack")
    try:
        b = open(p, "rb").read()
        sa = oead.Sarc(oead.yaz0.decompress(b) if b[:4] == b"Yaz0" else b)
        for f in sa.get_files():
            if f.name.endswith(".bmodellist"):
                ml = oead.aamp.ParameterIO.from_binary(f.data).lists["ModelData"].lists["ModelData_0"]
                folder = str(ml.objects["Base"].params["Folder"].v)
                unit = str(ml.lists["Unit"].objects["Unit_0"].params["UnitName"].v)
                if folder and unit:
                    return folder, unit
    except Exception:  # (oead's InvalidDataError is no OSError / ValueError: any problem = the name fallback)
        pass
    return actor, actor


def unyaz(b):
    return bytes(oead.yaz0.decompress(b)) if b[:4] == b"Yaz0" else b


def bntx_in(d):
    """the BNTX of a Switch BFRES (its external files: u64 offset, u32 size each); b"" = none"""
    if d[:8] != b"FRES    ":
        return d if d[:4] == b"BNTX" else b""
    n, arr = struct.unpack_from("<H", d, 0xc8)[0], struct.unpack_from("<Q", d, 0x98)[0]
    for i in range(n):
        o, sz = struct.unpack_from("<QI", d, arr + 16 * i)
        if d[o:o + 4] == b"BNTX" and o + sz <= len(d):
            return d[o:o + sz]
    return b""


def bake_actor(romfs, actor, side):
    folder, unit = model_of(romfs, actor)
    mp = os.path.join(romfs, "Model", folder + ".sbfres")
    tp = os.path.join(romfs, "Model", folder + ".Tex.sbfres")
    md = unyaz(open(mp, "rb").read())
    bf = Bfres(md)
    mods = dict(bf.models())
    if unit not in mods:
        raise ValueError("model %s not in %s.sbfres (%s)" % (unit, folder, ", ".join(mods)))
    shapes = model_mesh(bf, mods[unit])
    bn = bntx_in(unyaz(open(tp, "rb").read())) if os.path.exists(tp) else b""
    bn = bn or bntx_in(md)  # (a model file with its textures inside)
    tdict = {t["name"]: t for t in bntx.textures(bn)} if bn else {}
    texs, tix, out = [], {}, []
    for s in shapes:
        m = s["mat"] or {}
        alb = m.get("albedo")
        if alb not in tix:
            t = tdict.get(alb) if len(texs) < MAX_TEX else None
            rgba = shrink(decode_texture(bn, t), side).copy() if t else np.full((4, 4, 4), 200, np.uint8)  # (none: grey)
            keep_a = m.get("mode") in ("mask", "translucent") or m.get("atest")
            if t and t["fmt"] >> 8 == 0x1e:  # (BC5 "_Fx": Guardian / Ancient energy blades: r = the blade's shape, g = a glow)
                rgba = energy(rgba, m.get("color0"))
            elif not keep_a:
                rgba[..., 3] = 255
            tix[alb] = len(texs)
            texs.append(np.ascontiguousarray(rgba[..., [2, 1, 0, 3]]))
        attr = 1 if m.get("mode") == "translucent" else 0
        out.append((tix[alb], attr, (s["pos"] * 100.0).astype(np.float32), s["uv"].astype(np.float32), s["idx"]))
    if not out:
        raise ValueError("no drawable shape")
    return skrg(out, texs), {"folder": folder, "unit": unit, "shapes": shapes, "texs": texs}


def actors(romfs):
    d = os.path.join(romfs, "Actor", "Pack")
    return sorted(f[:-len(".sbactorpack")] for f in os.listdir(d) if f.endswith(".sbactorpack") and f.startswith(PREFIXES))


def write_atomic(path, data):
    """(s289 review2 nit 5) a bake killed half way leaves the old file or none, never a short one: a temp file, then a rename"""
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)


def actor_name_ok(a):
    """an actor name is letters, digits and _ only (the runtime's wpn_find table names): no dot, slash or colon, so a
    --only name can never write outside <out>/gear"""
    return 0 < len(a) <= 31 and all(c.isascii() and (c.isalnum() or c == "_") for c in a)


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    opt = {a.split("=")[0]: (a.split("=", 1)[1] if "=" in a else "") for a in argv if a.startswith("--")}
    if len(args) != 2:
        raise SystemExit(__doc__)
    romfs, out = args
    side = int(opt.get("--tex") or 256)
    only = [x for x in (opt.get("--only") or "").split(",") if x]
    bad = [x for x in only if not actor_name_ok(x)]
    if bad:  # (s289 review2 nit 5: --only=../x would have written <out>/gear/../x.bin)
        raise SystemExit("gear_bake: --only takes actor names (letters, digits, _; at most 31): not %s" % ", ".join(bad))
    gd = os.path.join(out, "gear")
    os.makedirs(gd, exist_ok=True)
    lines, nb = [], 0
    for a in only or actors(romfs):
        if not actor_name_ok(a):  # (a romfs file name the runtime could never ask for)
            lines.append("%s skipped: not an actor name the runtime draws" % a)
            continue
        try:
            blob, info = bake_actor(romfs, a, side)
        except Exception as e:  # (an install step: one odd weapon is skipped and listed, never the whole bake)
            lines.append("%s skipped: %s: %s" % (a, type(e).__name__, e))
            continue
        write_atomic(os.path.join(gd, a + ".bin"), blob)
        nv = sum(len(s["pos"]) for s in info["shapes"])
        nt = sum(len(s["idx"]) for s in info["shapes"]) // 3
        lines.append("%s baked: %s/%s, %d shapes, %d verts, %d tris, textures %s, %d bytes" % (
            a, info["folder"], info["unit"], len(info["shapes"]), nv, nt,
            " ".join("%dx%d" % (t.shape[1], t.shape[0]) for t in info["texs"]), len(blob)))
        nb += 1
    write_atomic(os.path.join(gd, "gear_pose.bin"), skrp_identity())
    write_atomic(os.path.join(gd, "gear_list.txt"), ("\n".join(lines) + "\n").encode("utf-8"))
    print("gear_bake: %d of %d weapons baked into %s" % (nb, len(lines), gd))


if __name__ == "__main__":
    main(sys.argv[1:])

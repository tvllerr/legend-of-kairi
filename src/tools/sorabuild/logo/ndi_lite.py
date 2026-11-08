"""The few scipy.ndimage operations the logo build used, in plain numpy (the friend kit has no scipy).
Each one reproduces what scipy gave on the logo masks (tie-breaks included); tools/sorabuild/logo/logo_build.py checks the
final bytes against the sha256 manifest, so a deviation is caught at install time."""
import numpy as np


def label(mask, eight=False):
    """connected components, labels 1..n numbered in raster order of each component's first pixel (= scipy.ndimage.label)"""
    m = np.asarray(mask, bool)
    H, W = m.shape
    lab = np.zeros((H, W), np.int64)
    parent = [0]

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    nxt = 1
    nb = [(-1, 0), (0, -1)] + ([(-1, -1), (-1, 1)] if eight else [])
    for y in range(H):
        row = m[y]
        for x in np.nonzero(row)[0]:
            near = []
            for dy, dx in nb:
                yy, xx = y + dy, x + dx
                if 0 <= yy < H and 0 <= xx < W and lab[yy, xx]:
                    near.append(find(lab[yy, xx]))
            if not near:
                parent.append(nxt)
                lab[y, x] = nxt
                nxt += 1
            else:
                r = min(near)
                lab[y, x] = r
                for o in near:
                    if o != r:
                        parent[o] = r
    # renumber roots in raster order of first appearance
    roots = np.array([find(i) for i in range(nxt)])
    flat = roots[lab.ravel()]
    out = np.zeros(H * W, np.int64)
    seen = {}
    for i in np.nonzero(flat)[0]:
        r = int(flat[i])
        if r not in seen:
            seen[r] = len(seen) + 1
        out[i] = seen[r]
    return out.reshape(H, W), len(seen)


def dilate4(m):
    """binary_dilation, default cross structure, 1 iteration"""
    m = np.asarray(m, bool)
    o = m.copy()
    o[1:] |= m[:-1]
    o[:-1] |= m[1:]
    o[:, 1:] |= m[:, :-1]
    o[:, :-1] |= m[:, 1:]
    return o


def bilinear0(img, cy, cx):
    """map_coordinates(img, [cy, cx], order=1, mode='constant', cval=0): zero outside [0, n-1]"""
    img = np.asarray(img, np.float64)
    H, W = img.shape
    cy = np.asarray(cy, np.float64)
    cx = np.asarray(cx, np.float64)
    ok = (cy >= 0) & (cy <= H - 1) & (cx >= 0) & (cx <= W - 1)
    y = np.where(ok, cy, 0.0)
    x = np.where(ok, cx, 0.0)
    y0 = np.minimum(np.floor(y).astype(np.int64), H - 2 if H > 1 else 0)
    x0 = np.minimum(np.floor(x).astype(np.int64), W - 2 if W > 1 else 0)
    fy, fx = y - y0, x - x0
    y1, x1 = np.minimum(y0 + 1, H - 1), np.minimum(x0 + 1, W - 1)
    top = img[y0, x0] * (1 - fx) + img[y0, x1] * fx
    bot = img[y1, x0] * (1 - fx) + img[y1, x1] * fx
    return np.where(ok, top * (1 - fy) + bot * fy, 0.0)


def gaussian(a, sigma):
    """gaussian_filter(a, sigma), truncate 4, mode 'reflect', axis 0 then 1"""
    r = int(4.0 * sigma + 0.5)
    x = np.arange(-r, r + 1, dtype=np.float64)
    w = np.exp(-0.5 / (sigma * sigma) * x * x)
    w /= w.sum()
    a = np.asarray(a, np.float64)
    for ax in (0, 1):
        n = a.shape[ax]
        idx = np.arange(-r, n + r)
        # reflect: d c b a | a b c d | d c b a
        idx = np.where(idx < 0, -idx - 1, idx)
        idx = np.where(idx >= n, 2 * n - 1 - idx, idx)
        p = np.take(a, idx, axis=ax)
        o = np.zeros_like(a)
        sl = [slice(None), slice(None)]
        for k in range(2 * r + 1):
            sl[ax] = slice(k, k + n)
            o += w[k] * p[tuple(sl)]
        a = o
    return a


def near_opaque(op, rad=3, rule=0):
    """for every texel: (dist<=rad flag, index of the nearest opaque texel). Brute force over the small disc."""
    H, W = op.shape
    offs = sorted(((dy * dy + dx * dx, dy, dx) for dy in range(-rad, rad + 1) for dx in range(-rad, rad + 1)
                   if dy * dy + dx * dx <= rad * rad and (dy or dx)), key=lambda t: _key(t, rule))
    yy, xx = np.mgrid[0:H, 0:W]
    best = np.full((H, W), -1, np.int64)
    by = np.zeros((H, W), np.int64)
    bx = np.zeros((H, W), np.int64)
    for d2, dy, dx in reversed(offs):      # later (nearer-first order) writes win
        sy, sx = yy + dy, xx + dx
        ok = (sy >= 0) & (sy < H) & (sx >= 0) & (sx < W)
        hit = np.zeros((H, W), bool)
        hit[ok] = op[sy[ok], sx[ok]]
        best[hit] = d2
        by[hit] = sy[hit]
        bx[hit] = sx[hit]
    return best >= 0, by, bx


def _key(t, rule):
    d2, dy, dx = t
    return [(d2, dy, dx), (d2, dx, dy), (d2, -dy, dx), (d2, dy, -dx), (d2, -dy, -dx), (d2, -dx, dy), (d2, dx, -dy), (d2, -dx, -dy)][rule]


def edt_small(zero):
    """distance_transform_edt(~zero) clipped for the logo's outline: 0 on `zero`, 1 on 4-neighbours of it, 9 elsewhere
    (the outline only reads clip(1.4 - d, 0, 1), so d >= sqrt(2) all give 0)"""
    zero = np.asarray(zero, bool)
    return np.where(zero, 0.0, np.where(dilate4(zero), 1.0, 9.0))


def erode4(m, iterations=1):
    """binary_erosion, default cross structure, border value 0"""
    m = np.asarray(m, bool)
    for _ in range(iterations):
        p = np.pad(m, 1, constant_values=False)
        m = p[1:-1, 1:-1] & p[:-2, 1:-1] & p[2:, 1:-1] & p[1:-1, :-2] & p[1:-1, 2:]
    return m


def zoom1(a, z):
    """scipy.ndimage.zoom(a, z, order=1) (grid_mode False: output i reads input i*(n-1)/(m-1))"""
    a = np.asarray(a, np.float64)
    for ax in (0, 1):
        n = a.shape[ax]
        m = int(round(n * z))
        c = np.arange(m) * ((n - 1) / (m - 1)) if m > 1 else np.zeros(m)
        i0 = np.minimum(np.floor(c).astype(np.int64), n - 2)
        f = c - i0
        lo, hi = np.take(a, i0, axis=ax), np.take(a, i0 + 1, axis=ax)
        sh = [1, 1]
        sh[ax] = m
        f = f.reshape(sh)
        a = lo * (1 - f) + hi * f
    return a

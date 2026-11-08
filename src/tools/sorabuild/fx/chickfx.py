#!/usr/bin/env python3
"""(s136 chickfx) Chicken Little's (KH2 summon, P_EX350) own sounds, voice lines and effects as APPENDED rows of the live sfx.bin / fx.bin,
plus the cue table runtime/sora_summon_fx.h. Nothing old is touched (checked at the end, byte for byte).

  python -I tools_s136/chickfx.py --sfx-in <sfx.bin> --fx-in <fx.bin> --out <dir> --header <h>
                                  [--cap9 N] [--no-fx] [--no-sfx] [--header runtime/sora_summon_fx.h]

SOUNDS (sfx.bin, SKSF v1, like tools_s110m/mkfx/sfxmickey.py)
  +9 sounds   se596 #32..#40 (Chicken Little's bank, all the sounds KH2's motions call + the rest of the bank), key "se/se596.win32.scd#N"
  +16 sounds  every line of voice/us/battle/chicken_little.win32.scd (#0..#29, 16 of them), key "voice/us/battle/chicken_little.win32.scd#N"
  names       cl_se<N> (N = 32..40), cl_v<N> (N = the voice index), marker "clse_v1"
  triggers    slot 90000 + the P_EX350.mset slot (form group 8 = FORM 9; groups 0..8 are used by Sora / forms / Roxas / Mickey), every
              frame trigger of the motion that plays a sound: id 13 (vsb_voice, p1 = voice index), id 8 (play_sound, p1 bank, p2 index),
              id 2 (footstep, p1 = footstep kind, cycled). Voice lines are plain sound triggers (kind 0), NOT bark/vtbl rows, so they never
              cut Sora's own barks. Sora's call motions 252 / 253 are in the SUMMON's mset, so their rows are 90252 / 90253.
EFFECTS (fx.bin, SKFX v1, like tools_s131p/prizefx.py)
  +11 elements  cl_fx_<N> = PHH recording ex350.<N> (N = 0..10), all of them; the loop-end tail variants ex350.0~N / 2~N / 4~N (360 of them,
                one per cut frame, 200 MB of near-copies) are NOT packed. ex350.9 (the 351 frame call-in) is 5.7 M verts = 229 MB;
                --cap9 N keeps only its first N frames.
  new textures are inserted BEFORE the pack's last texture (the ribbon gradient sora_fx.c emit_trail uses); a new texture identical to an
  existing one is reused. Old element ids / frames / batches / verts / textures keep their indices.
"""
import argparse, hashlib, importlib.util, json, mmap, os, struct, sys, zlib
import sys as _sys, os as _os; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__))); import sndsrc  # (s156) sound inputs: recipe tree or PHH cache

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("fxb", os.path.join(HERE, "fxbake.py"))
fxb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fxb)
OUT_TEX, OUT_EFF, OUT_FRAME, OUT_BATCH, OUT_VERT, OUT_HDR = fxb.OUT_TEX, fxb.OUT_EFF, fxb.OUT_FRAME, fxb.OUT_BATCH, fxb.OUT_VERT, fxb.OUT_HDR
CACHE = os.environ["SB_FXCACHE"] if "SB_FXCACHE" in os.environ else os.environ["SB_PHH"]  # fx cache dir (SB_FXCACHE; SB_PHH = old name), passed in by sorabuild (ctx.fxcache)
FXCACHE = os.environ.get("SB_FXDIR") or CACHE  # (s161) dir with kh2_fx.bin / kh2_fx.json: our fxrec recording ("fx": "fxrec") or the PHH cache
SLOT_BASE = 90000          # sfx trigger slot = SLOT_BASE + mset slot
FX_PREFIX = "cl_fx_"
FX_SRC_FMT = "ex350.%d"  # the PHH recording name of element N
FX_ELEMENTS = list(range(0, 11))  # ex350.0 .. ex350.10 (the PHF2 recording has 11 plain + 360 "~" tail variants)
SUMMON_SLOTS = {0: "idle", 1: "walk", 2: "run", 3: "jump", 4: "fall", 5: "land", 9: "item", 201: "attack 1", 202: "attack 2", 203: "attack 3",
                231: "entry (80A)", 232: "exit (81A)", 241: "90A", 242: "91A", 252: "Sora call (A830)", 253: "Sora call end (A831)"}
SK = struct.Struct("<4s11I")


# --------------------------------------------------------------------------------------------------------------- triggers (shared)
def read_triggers():
    """(slot, frame, id, p1, p2) of P_EX350.mset from PHH's motions.kh.json (VERIFIED equal to the baked chicken_motion.bin ftrigs)."""
    m = json.load(open(sndsrc.motions_json(), encoding="utf-8"))["files"]["obj_P_EX350_mset"]["slots"]
    out = []
    for slot_s, sub in m.items():
        for name, v in sub.items():
            for tid, frame, par in v["frames"]:
                out.append((int(slot_s), int(frame), int(tid), par))
    out.sort(key=lambda t: (t[0], t[1]))
    return out


# --------------------------------------------------------------------------------------------------------------- sfx
def build_sfx(src, dst, trigs):
    import wave
    d = open(src, "rb").read()
    magic, ver, ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes, _ = SK.unpack_from(d, 0)
    assert magic == b"SKSF" and ver == 1
    sj = json.load(open(sndsrc.sounds_json(), encoding="utf-8"))
    sounds_js = sj["sounds"]
    o = 48
    sec = {}
    for nm, cnt, sz in (("sounds", ns, 72), ("trig", nt, 28), ("ts", nts, 4), ("vt", nv, 16), ("ch", nc, 8), ("names", nn, 36), ("hits", nh, 8)):
        sec[nm] = d[o:o + cnt * sz]
        o += cnt * sz
    pcm = bytearray(d[data_off:data_off + data_bytes])
    keys = [sec["sounds"][i * 72:i * 72 + 48].split(b"\0")[0].decode() for i in range(ns)]
    kidx = {k: i for i, k in enumerate(keys)}
    names = {sec["names"][i * 36:i * 36 + 32].split(b"\0")[0].decode() for i in range(nn)}
    assert "clse_v1" not in names, "already has the Chicken Little rows"
    last_slot = max(struct.unpack_from("<I", sec["trig"], i * 28)[0] for i in range(nt))
    assert last_slot < SLOT_BASE, "slot range %d.. already used (max %d)" % (SLOT_BASE, last_slot)
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

    SE = "se/se596.win32.scd#%d"
    VO = "voice/us/battle/chicken_little.win32.scd#%d"
    new_names = []
    for i in range(0, 64):
        if SE % i in sounds_js:
            w = want(SE % i)
            if w >= 0:
                new_names.append(("cl_se%d" % i, w))
    for i in range(0, 64):
        if VO % i in sounds_js:
            w = want(VO % i)
            if w >= 0:
                new_names.append(("cl_v%d" % i, w))
    assert len(new_names) == 25, len(new_names)
    miss = set()
    rows, tsnd = [], []
    rowinfo = []  # for the report
    for slot, frame, tid, par in trigs:
        if tid == 13:
            key = VO % par[0]
            i = want(key)
            if i < 0:
                miss.add(key)
                continue
            rows.append((SLOT_BASE + slot, frame, 0, 0, 0, 1, len(tsnd) + nts)); tsnd.append(i)
            rowinfo.append((slot, frame, "voice", "cl_v%d" % par[0]))
        elif tid == 8:
            key = "se/se%03d.win32.scd#%d" % (par[0], par[1])
            i = want(key)
            if i < 0:
                miss.add(key)
                continue
            rows.append((SLOT_BASE + slot, frame, 0, 0, 0, 1, len(tsnd) + nts)); tsnd.append(i)
            nm = "cl_se%d" % par[1] if par[0] == 596 else key
            rowinfo.append((slot, frame, "sfx", nm))
        elif tid == 2:
            ids = [i for i in (want(k) for k in fkind.get(par[0], [])) if i >= 0]
            if not ids:
                miss.add("footstep %d" % par[0])
                continue
            rows.append((SLOT_BASE + slot, frame, 0, 0, 1, len(ids), len(tsnd) + nts)); tsnd += ids
            assert len(fkind[par[0]]) == 1, "footstep kind with several sounds: the table needs one row per sound"
            rowinfo.append((slot, frame, "sfx", fkind[par[0]][0]))  # a json key: sora_sfx_id accepts it
    rows.sort(key=lambda r: (r[0], r[1]))
    marker = new_names[0][1]
    names_b = b"".join(struct.pack("<32sI", n.encode(), s) for n, s in new_names) + struct.pack("<32sI", b"clse_v1", marker)
    body = bytearray(sec["sounds"] + bytes(new_s) + sec["trig"] + b"".join(struct.pack("<7I", *t) for t in rows) + sec["ts"] +
                     b"".join(struct.pack("<I", i) for i in tsnd) + sec["vt"] + sec["ch"] + sec["names"] + names_b + sec["hits"])
    while (48 + len(body)) % 16:
        body.append(0)
    ns2 = ns + len(new_keys)
    hdr = SK.pack(b"SKSF", 1, ns2, nt + len(rows), nts + len(tsnd), nv, nc, nn + len(new_names) + 1, nh, 48 + len(body), len(pcm), 0)
    tmp = dst + ".tmp"
    open(tmp, "wb").write(hdr + body + pcm)
    os.replace(tmp, dst)
    print("%s: +%d sounds (%.1f MB PCM16), +%d triggers (slots %d..), +%d trig sounds, +%d names (25 + clse_v1)" %
          (dst, len(new_keys), (len(pcm) - data_bytes) / 1e6, len(rows), SLOT_BASE, len(tsnd), len(new_names) + 1))
    print("missing:", sorted(miss) or "none")
    # old rows byte-identical?
    n = open(dst, "rb").read()
    m2, v2, ns_, nt_, nts_, nv_, nc_, nn_, nh_, do2, db2, _ = SK.unpack_from(n, 0)
    o2 = 48
    assert n[o2:o2 + 72 * ns] == sec["sounds"]; o2 += 72 * ns_
    assert n[o2:o2 + 28 * nt] == sec["trig"]; o2 += 28 * nt_
    assert n[o2:o2 + 4 * nts] == sec["ts"]; o2 += 4 * nts_
    assert n[o2:o2 + 16 * nv] == sec["vt"]; o2 += 16 * nv_
    assert n[o2:o2 + 8 * nc] == sec["ch"]; o2 += 8 * nc_
    assert n[o2:o2 + 36 * nn] == sec["names"]; o2 += 36 * nn_
    assert n[o2:o2 + 8 * nh] == sec["hits"] and nh_ == nh
    assert n[do2:do2 + data_bytes] == d[data_off:data_off + data_bytes]
    # triggers still sorted by slot, new ones all after the old ones
    tr = [struct.unpack_from("<II", n, 48 + 72 * ns_ + 28 * i) for i in range(nt_)]
    assert all(tr[i][0] <= tr[i + 1][0] for i in range(len(tr) - 1)), "triggers not sorted by slot"
    print("VERIFIED: old sounds / triggers / trig-sounds / vtbl / choice / name / hit rows and the old PCM are byte-identical; triggers sorted by slot")
    return rowinfo, {n_: s for n_, s in new_names}


# --------------------------------------------------------------------------------------------------------------- fx
def frames_iter(src, name, cap=0):
    """Phf2.frames, one frame at a time (ex350.9 is 5.7 M verts: all tuples at once would take GBs)."""
    off, ps, us, nf, fl, bone, pf = src.effects[name]
    f = src.f
    f.seek(off)
    table = struct.unpack(f"<{nf + 1}I", f.read(4 * (nf + 1)))
    base = off + 4 * (nf + 1)
    for i in range(nf if not cap else min(cap, nf)):
        f.seek(base + table[i])
        d = zlib.decompress(f.read(table[i + 1] - table[i]))
        (nb,) = struct.unpack_from("<I", d, 0)
        q, fr = 4, []
        for _ in range(nb):
            v = fxb.BATCH.unpack_from(d, q)
            q += fxb.BATCH.size
            nv = v[43]
            fr.append((v, [fxb.VERTEX.unpack_from(d, q + k * fxb.VERTEX.size) for k in range(nv)]))
            q += fxb.VERTEX.size * nv
        yield fr


def copy_range(fout, mm, a, b):
    while a < b:
        n = min(b - a, 32 << 20)
        fout.write(mm[a:a + n])
        a += n


def build_fx(src_path, dst, cap9):
    f = open(src_path, "rb")
    mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    h = OUT_HDR.unpack_from(mm, 0)
    assert h[0] == b"SKFX" and h[1] == 1
    ntex, neff, nfr, nba, nve, ntr = h[2:8]
    o_tex, o_eff, o_frame, o_batch, o_vert, o_trail, o_pix, pixb = h[8:16]
    names = [mm[o_eff + i * 64:o_eff + i * 64 + 32].split(b"\0")[0].decode() for i in range(neff)]
    assert not any(n.startswith(FX_PREFIX) for n in names), "the pack already has cl_fx_* elements"
    texrec = [struct.unpack_from("<IIII", mm, o_tex + i * 16) for i in range(ntex)]
    old_last = ntex - 1
    assert texrec[old_last][2] == max(t[2] for t in texrec), "last texture is not last in pix"
    last_off = texrec[old_last][2]
    last_bytes = texrec[old_last][0] * texrec[old_last][1] * 4
    assert last_off + last_bytes == pixb, "ribbon gradient is not the end of pix"
    # no old batch may use the last texture (it would need renumbering)
    for i in range(nba):
        assert struct.unpack_from("<H", mm, o_batch + i * 52)[0] != old_last, "an old batch uses the last texture: renumber needed"
    # md5 of existing textures by size, for reuse
    by_hash = {}
    for i, (w, hh, off, _) in enumerate(texrec[:old_last]):
        by_hash.setdefault((w, hh, hashlib.md5(mm[o_pix + off:o_pix + off + w * hh * 4]).digest()), ("old", i))

    psrc = fxb.Phf2(os.path.join(FXCACHE, "kh2_fx.bin"))
    meta = json.load(open(os.path.join(FXCACHE, "kh2_fx.json"), encoding="utf-8"))["effects"]
    tex_map = {}               # PHF2 texture index -> final pack index (old index, or new_base + k)
    new_tex_list = []          # (w, h, bgra) new ones, in order
    tex_final_pending = {}     # PHF2 idx -> ("old", i) or ("new", k)
    newb, newv, newf, newe = bytearray(), bytearray(), bytearray(), bytearray()
    report = []
    pend = []   # (new batch index, ("old"|"new", texture index)) patched once the number of new textures is known
    for n in FX_ELEMENTS:
        name = FX_SRC_FMT % n
        cap = cap9 if n == 9 else 0
        off, ps, us, nf, fl, bone, pf = psrc.effects[name]
        m = meta[name]
        first_frame = nfr + len(newf) // 8
        nvert_tot = maxv = 0
        nfrm = 0
        for fr in frames_iter(psrc, name, cap):
            first_batch = nba + len(newb) // 52
            fv = 0
            for v, vs in fr:
                tex = v[0]
                if tex == 0xFFFE:
                    tex = 0xFFFF
                if tex != 0xFFFF:
                    if tex not in tex_final_pending:
                        w, hh, bgra = fxb.dds_bgra(psrc.texture(tex))
                        key = (w, hh, hashlib.md5(bgra).digest())
                        if key in by_hash:
                            tex_final_pending[tex] = by_hash[key]
                        else:
                            tex_final_pending[tex] = ("new", len(new_tex_list))
                            by_hash[key] = ("new", len(new_tex_list))
                            new_tex_list.append((w, hh, bgra))
                    tex = tex_final_pending[tex]
                blend_en, op, srcb, dstb = v[3:7]
                blend = 0 if dstb == 2 else (1 if dstb == 6 else 2)
                if not blend_en:
                    blend = 2
                clamp = 1 if v[12] or v[13] else 0
                first_vert = nve + len(newv) // 40
                for p in vs:
                    c = [min(255, int(round(x / 32767.5 * 1.992188 * 255))) for x in p[10:14]]
                    newv += OUT_VERT.pack(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], 0, *c)
                # texture index is patched after the new-texture base is known: store a placeholder (tuple -> temp record)
                newb += OUT_BATCH.pack(0xFFFF if tex == 0xFFFF else 0, v[2], blend, clamp, 0, 0, 0, first_vert, len(vs), *v[33:42])
                if tex != 0xFFFF:
                    pend.append((len(newb) // 52 - 1, tex))
                fv += len(vs)
            newf += OUT_FRAME.pack(first_batch, len(fr))
            nvert_tot += fv
            maxv = max(maxv, fv)
            nfrm += 1
        loops = 1 if (m.get("loops") and not cap) else 0
        newe += OUT_EFF.pack((FX_PREFIX + str(n)).encode(), first_frame, nfrm, loops, pf, bone, int(m.get("fade", 0)), maxv, 0)
        report.append((FX_PREFIX + str(n), name, nfrm, loops, bone, pf, int(m.get("fade", 0)), maxv, nvert_tot))
        print("  %-9s = %-9s %3d frames%s, %7d verts (max %5d/frame), bone %#06x pax %#05x fade %d" %
              (FX_PREFIX + str(n), name, nfrm, " (loop)" if loops else "", nvert_tot, maxv, bone, pf, int(m.get("fade", 0))))
    new_base = old_last     # new textures take indices old_last .. old_last + k - 1, the gradient moves to old_last + k
    k_new = len(new_tex_list)
    for bi, (kind, idx) in pend:
        t = idx if kind == "old" else new_base + idx
        struct.pack_into("<H", newb, bi * 52, t)
    # --- write
    npix = bytearray()
    new_texrecs = []
    for (w, hh, bgra) in new_tex_list:
        new_texrecs.append(OUT_TEX.pack(w, hh, last_off + len(npix), 0))
        npix += bgra
    last_new_off = last_off + len(npix)
    tex_tbl = mm[o_tex:o_tex + 16 * old_last] + b"".join(new_texrecs) + OUT_TEX.pack(texrec[old_last][0], texrec[old_last][1], last_new_off, 0)
    ntex2 = ntex + k_new
    pix2 = pixb + len(npix)
    sizes = [ntex2 * 16, (neff + len(newe) // 64) * 64, (nfr + len(newf) // 8) * 8, (nba + len(newb) // 52) * 52, (nve + len(newv) // 40) * 40, ntr * 12, pix2]
    offs, q = [], OUT_HDR.size
    for s in sizes:
        offs.append(q)
        q += s
        q += -q % 16
    nfr2, nba2, nve2, neff2 = nfr + len(newf) // 8, nba + len(newb) // 52, nve + len(newv) // 40, neff + len(newe) // 64
    hdr = OUT_HDR.pack(b"SKFX", 1, ntex2, neff2, nfr2, nba2, nve2, ntr, *offs, pix2)
    tmp = dst + ".tmp"
    with open(tmp, "wb") as fo:
        fo.write(hdr)
        parts = [lambda: fo.write(tex_tbl),
                 lambda: (copy_range(fo, mm, o_eff, o_eff + 64 * neff), fo.write(newe)),
                 lambda: (copy_range(fo, mm, o_frame, o_frame + 8 * nfr), fo.write(newf)),
                 lambda: (copy_range(fo, mm, o_batch, o_batch + 52 * nba), fo.write(newb)),
                 lambda: (copy_range(fo, mm, o_vert, o_vert + 40 * nve), fo.write(newv)),
                 lambda: copy_range(fo, mm, o_trail, o_trail + 12 * ntr),
                 lambda: (copy_range(fo, mm, o_pix, o_pix + last_off), fo.write(npix), copy_range(fo, mm, o_pix + last_off, o_pix + pixb))]
        for off_, fn in zip(offs, parts):
            fo.write(bytes(off_ - fo.tell()))
            fn()
    os.replace(tmp, dst)
    print("%s: %.1f MB (was %.1f MB): +%d elements, +%d frames, +%d batches, +%d verts, textures %d -> %d (%d reused from the old pack, %d new, gradient now #%d)" %
          (dst, os.path.getsize(dst) / 1e6, len(mm) / 1e6, len(newe) // 64, len(newf) // 8, len(newb) // 52, len(newv) // 40, ntex, ntex2,
           len(set(i for k, i in tex_final_pending.values() if k == "old")), k_new, ntex2 - 1))
    mm.close(); f.close()
    verify_fx(src_path, dst)
    return report


def verify_fx(old, new):
    fo, fn = open(old, "rb"), open(new, "rb")
    mo, mn = mmap.mmap(fo.fileno(), 0, access=mmap.ACCESS_READ), mmap.mmap(fn.fileno(), 0, access=mmap.ACCESS_READ)
    ho, hn = OUT_HDR.unpack_from(mo, 0), OUT_HDR.unpack_from(mn, 0)
    SZ = (16, 64, 8, 52, 40, 12)
    nm = ("tex", "eff", "frame", "batch", "vert", "trail")
    for i in range(6):
        cnt = ho[2 + i]
        if i == 0:
            cnt -= 1
        a0, b0 = ho[8 + i], hn[8 + i]
        n = cnt * SZ[i]
        x = 0
        while x < n:
            step = min(n - x, 64 << 20)
            assert mo[a0 + x:a0 + x + step] == mn[b0 + x:b0 + x + step], nm[i]
            x += step
        print("  %s: old %d rows prefix-identical (new %d)" % (nm[i], ho[2 + i], hn[2 + i]))
    ol = struct.unpack_from("<III", mo, ho[8] + (ho[2] - 1) * 16)
    nl = struct.unpack_from("<III", mn, hn[8] + (hn[2] - 1) * 16)
    assert ol[0] == nl[0] and ol[1] == nl[1]
    x, n = 0, ol[2]
    while x < n:
        step = min(n - x, 64 << 20)
        assert mo[ho[14] + x:ho[14] + x + step] == mn[hn[14] + x:hn[14] + x + step], "old pixels"
        x += step
    assert mo[ho[14] + ol[2]:ho[14] + ho[15]] == mn[hn[14] + nl[2]:hn[14] + hn[15]], "gradient pixels"
    # every new batch: texture in range, verts in range
    for b in range(ho[5], hn[5]):
        t, fl, bl, cl, _, _, _, fv, nv = struct.unpack_from("<HBBBBBBII", mn, hn[11] + b * 52)
        assert t == 0xFFFF or t < hn[2] - 1, ("batch texture", b, t)
        assert fv + nv <= hn[6], "batch verts"
    for fidx in range(ho[4], hn[4]):
        fb, n_ = struct.unpack_from("<II", mn, hn[10] + fidx * 8)
        assert fb + n_ <= hn[5]
    for e in range(ho[3], hn[3]):
        r = OUT_EFF.unpack_from(mn, hn[9] + e * 64)
        assert r[1] + r[2] <= hn[4]
    print("VERIFIED: old tex/eff/frame/batch/vert/trail rows and all old pixels byte-identical; ribbon gradient is still the last texture; new rows in range")
    mo.close(); mn.close(); fo.close(); fn.close()


# --------------------------------------------------------------------------------------------------------------- header
def write_header(path, cues, fxrep):
    """cues: (slot, frame, who, kind, name, bone, guess) rows."""
    kinds = {"sfx": "SMC_SFX", "voice": "SMC_VOICE", "fx": "SMC_FX"}
    L = []
    L.append("""// Sora-in-BotW: Chicken Little's (KH2 summon, P_EX350) sound / voice / effect cues. DATA ONLY (s136 chickfx, generated by chickfx.py).
//
// One row = "when the summon's motion <slot> passes <frame>, fire <name>". Frames are the baked motion's own clock (60 Hz GFR, the same unit as
// out_s134/chicken/chicken_motion.bin: slot 231 has 350 frames). A row fires when prev_clock < frame <= clock (a frame-0 row needs prev = -1).
//   who  SMC_SUMMON = the cue belongs to the chicken's own body motion; SMC_SORA = it belongs to Sora's call motion 252 / 253 (these live in the
//        SUMMON's mset and play on SORA'S skeleton, out_s134/chicken_call/chickencall_motion.bin).
//   kind SMC_SFX   name = sfx.bin name (sora_sfx_id(name) -> sora_sfx_play_at), a sound effect
//        SMC_VOICE name = sfx.bin name, a voice line (play like an sfx; they are plain sounds, not bark rows)
//        SMC_FX    name = fx.bin name (sora_fx_id(name) -> sora_fx_spawn); bone = the effect's bind: SM_BONE_ROOT (0x4000) = the summon's feet / root,
//                  a small number N = the summon body's skeleton bone N (the pack's own value, tools/fxbake.py: bone 0x4000|3 is the Keyblade tip
//                  for Sora's forms; for this body it is the P_EX350 bone index), SM_BONE_NONE (0) = unbound (stays where spawned)
//   guess  1 = inferred (the data has no row for it), 0 = taken straight from the mset frame triggers.
// The same sounds are ALSO in sfx.bin as triggers at slot SM_SFX_BASE + slot (sora_sfx_motion(SM_SFX_BASE + slot, prev, clock, pos) plays the
// SFX + VOICE rows of this table by itself). Use that OR this table for sound, never both.
// Without the new packs sora_sfx_id / sora_fx_id return -1 and every cue is silent.
#ifndef SORA_SUMMON_FX_H
#define SORA_SUMMON_FX_H
#include <stdint.h>

#define SM_SFX_BASE 90000
#define SM_BONE_NONE 0
#define SM_BONE_ROOT 0x4000
enum { SMC_SFX = 0, SMC_VOICE = 1, SMC_FX = 2 };
enum { SMC_SUMMON = 0, SMC_SORA = 1 };
typedef struct { int16_t slot; uint16_t frame; uint8_t who, kind, guess, pad; int32_t bone; const char *name; } SmCue;

// sorted by (who, slot, frame)
static const SmCue SM_CUES[] = {""")
    for slot, frame, who, kind, name, bone, guess, note in cues:
        L.append('    {%d, %d, %s, %s, %d, 0, %s, "%s"},  // %s' % (slot, frame, "SMC_SORA" if who else "SMC_SUMMON", kinds[kind], guess,
                                                                 ("SM_BONE_ROOT" if bone == 0x4000 else str(bone)), name, note))
    L.append("};")
    L.append("#define SM_NCUES ((int)(sizeof(SM_CUES) / sizeof(SM_CUES[0])))")
    L.append("""
// Packed in fx.bin but with NO mset trigger (code-driven in KH2's object script obj_P_EX350, which is not decoded): cl_fx_0 / cl_fx_2 (600 f loops,
// unbound, 12 verts: a small sprite pair), cl_fx_1 (36 f burst), cl_fx_3 (34 f), cl_fx_4 (600 f loop on bone 54, 432 verts/frame: an aura or
// trail), cl_fx_6 (71 f on bone 54, 2814 verts), cl_fx_7 (50 f). GUESS: hit/attack bursts + an aura. No cue is emitted for them.
#endif""")
    open(path, "w", encoding="utf-8", newline="\n").write("\n".join(L) + "\n")
    print("wrote", path, len(cues), "cues")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sfx-in", required=True)
    ap.add_argument("--fx-in", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--header", required=True)  # cue table (generated code, not used by the build)
    ap.add_argument("--cap9", type=int, default=0)
    ap.add_argument("--no-fx", action="store_true")
    ap.add_argument("--no-sfx", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    trigs = read_triggers()
    # call-motion triggers on Sora's side: slots 252 / 253 (the mset slot numbers)
    sfxinfo = []
    if not a.no_sfx:
        sfxinfo, _ = build_sfx(a.sfx_in, os.path.join(a.out, "sfx.bin"), trigs)
    fxrep = []
    if not a.no_fx:
        fxrep = build_fx(a.fx_in, os.path.join(a.out, "fx.bin"), a.cap9)
    # ---- cue table
    pf = {r[0]: r[4] for r in fxrep}
    if not fxrep:
        pf = {"cl_fx_5": 92, "cl_fx_8": 92, "cl_fx_9": 0x4000, "cl_fx_10": 0x4000}
    cues = []
    for slot, frame, tid, par in trigs:
        who = 1 if slot in (252, 253) else 0
        if tid == 1:
            nm = "cl_fx_%d" % par[0]
            cues.append((slot, frame, who, "fx", nm, pf[nm], 0, "effect_caster ex350.%d, bind %#x" % (par[0], pf[nm])))
    for slot, frame, kind, nm in sfxinfo:
        who = 1 if slot in (252, 253) else 0
        cues.append((slot, frame, who, kind, nm, 0, 0, SUMMON_SLOTS.get(slot, "")))
    cues.sort(key=lambda c: (c[2], c[0], c[1], c[3] != "fx"))
    write_header(a.header, cues, fxrep)


if __name__ == "__main__":
    main()

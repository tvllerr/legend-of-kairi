#!/usr/bin/env python3
"""(s137 stitch) Stitch's (KH2 summon, N_HB040_BTL) sounds, voice lines and effects as APPENDED rows of the live sfx.bin / fx.bin, plus his cue table
runtime/sora_summon_fx_st.h. Derived from chickfx.py (same encoders, same checks). Nothing old is touched (checked at the end, byte for byte).
Idempotent: a sound key / name / fx element that is already in the pack is skipped, so a second run (or a run for more elements later, e.g. Ohana!'s
st_fx_11..13) only appends what is missing. A prebuilt whole bank is NOT shipped (another helper appends Peter Pan / Genie): the build always starts from the
LIVE files at install time.

  python -I stitchfx.py --sfx-in <sfx.bin> --fx-in <fx.bin> --out <dir> --header <h>
                                         [--fx 6,7,9] [--no-fx] [--no-sfx] [--header runtime/sora_summon_fx_st.h]

SOUNDS (sfx.bin)  +se598 #32..#51 (Stitch's bank, 19 of them), +6 voice lines voice/us/battle/stitch.win32.scd #4 6 7 13 18 21; names st_se<N> / st_v<N>,
  marker "stse_v1". NO trigger rows (the cue table plays them): sfx.bin gets sounds + names only.
EFFECTS (fx.bin)  st_fx_<N> = PHH recording hb040.<N> for the N in --fx (default 6 7 9: the entry's two effects and the leave puff); new textures go
  before the pack's last texture (the ribbon gradient), identical ones are reused.
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
FX_PREFIX = "st_fx_"
FX_SRC_FMT = "hb040.%d"  # the PHH recording name of element N
FX_ELEMENTS = [6, 7, 9]
SUMMON_SLOTS = {0: "idle", 161: "10A", 201: "attack 1 (50A)", 202: "attack 2 (50B)", 203: "52A", 204: "53A", 205: "54A", 206: "attack 3 (50C)",
                231: "entry (80A)", 232: "leave", 241: "90A (Ohana! intro)", 242: "Blast (91A)", 252: "Sora call (A820)", 253: "Sora Ohana! intro (A821)",
                254: "Sora Ohana! idle (A825)", 255: "Sora Ukulele (A823)", 256: "Sora Blast (A824)", 257: "Sora Shoot (A822)"}
SORA_SLOTS = (252, 253, 254, 255, 256, 257)   # (s138 ohana) type-20 slots: played on Sora's skeleton
# (s138 ohana) effects that the KH2 exe code starts (not mset triggers), VERIFIED in the decomps (recon/S138_HELPER_OHANA.md): FUN_1401c9c10(self, <hb040 index>, <obj>, <bind>)
#   Ukulele (re:0x1C8700): index 4 on Stitch (bind 0x13) and on Sora (bind 0x41), Blast (re:0x1C8440): index 13 on Sora (bind 0x41); the shots' muzzle (re:0x1C9920): index 1, bone 0x6C (arm 0/2) / 0x36 (arm 1/3).
#   (slot, frame, who, element, bone) - frame 1 = the start of the command's motion (GUESS: the exe starts them with the motion)
EXTRA_FX = [(205, 1, 0, 4, 0x13), (255, 1, 1, 4, 0x41), (256, 1, 1, 13, 0x41)]
MUZZLE_BONE = {0: 0x6C, 2: 0x6C, 1: 0x36, 3: 0x36}
SK = struct.Struct("<4s11I")


# --------------------------------------------------------------------------------------------------------------- triggers (shared)
def read_triggers():
    """(slot, frame, id, p1, p2) of N_HB040_BTL.mset from PHH's motions.kh.json (VERIFIED equal to the baked stitch_motion.bin ftrigs)."""
    m = json.load(open(sndsrc.motions_json(), encoding="utf-8"))["files"]["obj_N_HB040_BTL_mset"]["slots"]
    out = []
    for slot_s, sub in m.items():
        for name, v in sub.items():
            for tid, frame, par in v["frames"]:
                out.append((int(slot_s), int(frame), int(tid), par))
    out.sort(key=lambda t: (t[0], t[1]))
    return out


# --------------------------------------------------------------------------------------------------------------- sfx
def build_sfx(src, dst, trigs):
    """Appends Stitch's sounds + names (no trigger rows). Returns (rowinfo for the cue table, number of new sounds)."""
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
    have_names = {sec["names"][i * 36:i * 36 + 32].split(b"\0")[0].decode() for i in range(nn)}
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

    SE = "se/se598.win32.scd#%d"
    VO = "voice/us/battle/stitch.win32.scd#%d"
    new_names = []
    nse = nvo = 0
    for i in range(0, 64):
        if SE % i in sounds_js:
            w = want(SE % i)
            nse += w >= 0
            if w >= 0 and "st_se%d" % i not in have_names:
                new_names.append(("st_se%d" % i, w))
    for i in range(0, 64):
        if VO % i in sounds_js:
            w = want(VO % i)
            nvo += w >= 0
            if w >= 0 and "st_v%d" % i not in have_names:
                new_names.append(("st_v%d" % i, w))
    assert nse == 19 and nvo == 6, (nse, nvo)
    miss = set()
    rowinfo = []  # for the cue table
    for slot, frame, tid, par in trigs:
        if tid == 13:
            if want(VO % par[0]) < 0:
                miss.add(VO % par[0]); continue
            rowinfo.append((slot, frame, "voice", "st_v%d" % par[0]))
        elif tid == 8:
            if par[0] != 598:
                miss.add("bank %d" % par[0]); continue
            if want(SE % par[1]) < 0:
                miss.add(SE % par[1]); continue
            rowinfo.append((slot, frame, "sfx", "st_se%d" % par[1]))
    if not new_keys and not new_names:
        print("sfx.bin: Stitch's sounds are already in the pack (nothing to append)")
        return rowinfo, 0
    names_b = b"".join(struct.pack("<32sI", n.encode(), s) for n, s in new_names)
    if "stse_v1" not in have_names:
        names_b += struct.pack("<32sI", b"stse_v1", new_names[0][1])
    n_new_names = len(names_b) // 36
    body = bytearray(sec["sounds"] + bytes(new_s) + sec["trig"] + sec["ts"] + sec["vt"] + sec["ch"] + sec["names"] + names_b + sec["hits"])
    while (48 + len(body)) % 16:
        body.append(0)
    hdr = SK.pack(b"SKSF", 1, ns + len(new_keys), nt, nts, nv, nc, nn + n_new_names, nh, 48 + len(body), len(pcm), 0)
    tmp = dst + ".tmp"
    open(tmp, "wb").write(hdr + body + pcm)
    os.replace(tmp, dst)
    print("%s: +%d sounds (%.1f MB PCM16), +%d names" % (dst, len(new_keys), (len(pcm) - data_bytes) / 1e6, n_new_names))
    print("missing:", sorted(miss) or "none")
    n = open(dst, "rb").read()
    m2, v2, ns_, nt_, nts_, nv_, nc_, nn_, nh_, do2, db2, _ = SK.unpack_from(n, 0)
    o2 = 48
    assert n[o2:o2 + 72 * ns] == sec["sounds"]; o2 += 72 * ns_
    assert n[o2:o2 + 28 * nt] == sec["trig"] and nt_ == nt; o2 += 28 * nt_
    assert n[o2:o2 + 4 * nts] == sec["ts"]; o2 += 4 * nts_
    assert n[o2:o2 + 16 * nv] == sec["vt"]; o2 += 16 * nv_
    assert n[o2:o2 + 8 * nc] == sec["ch"]; o2 += 8 * nc_
    assert n[o2:o2 + 36 * nn] == sec["names"]; o2 += 36 * nn_
    assert n[o2:o2 + 8 * nh] == sec["hits"] and nh_ == nh
    assert n[do2:do2 + data_bytes] == d[data_off:data_off + data_bytes]
    print("VERIFIED: old sounds / triggers / trig-sounds / vtbl / choice / name / hit rows and the old PCM are byte-identical")
    return rowinfo, len(new_keys)


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


def build_fx(src_path, dst, caps):
    f = open(src_path, "rb")
    mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    h = OUT_HDR.unpack_from(mm, 0)
    assert h[0] == b"SKFX" and h[1] == 1
    ntex, neff, nfr, nba, nve, ntr = h[2:8]
    o_tex, o_eff, o_frame, o_batch, o_vert, o_trail, o_pix, pixb = h[8:16]
    names = [mm[o_eff + i * 64:o_eff + i * 64 + 32].split(b"\0")[0].decode() for i in range(neff)]
    todo = [n for n in FX_ELEMENTS if FX_PREFIX + str(n) not in names]
    if not todo:
        print("fx.bin: st_fx_%s are already in the pack (nothing to append)" % ",".join(map(str, FX_ELEMENTS)))
        mm.close(); f.close()
        return []
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
    for n in todo:
        name = FX_SRC_FMT % n
        cap = caps.get(n, 0)
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
def write_header(path, cues):
    """cues: (slot, frame, who, kind, name, bone, guess, note) rows."""
    kinds = {"sfx": "SMC_SFX", "voice": "SMC_VOICE", "fx": "SMC_FX"}
    L = []
    L.append("""// Sora-in-BotW: Stitch's (KH2 summon, N_HB040_BTL) sound / voice / effect cues. DATA ONLY (s137 stitch, generated by stitchfx.py).
// Same format as sora_summon_fx.h (SmCue / SMC_*, included first): a row fires when prev_clock < frame <= clock of the mset slot's baked clock (60 Hz).
// who SMC_SORA rows belong to Sora's motions 252 / 253 (call, Ohana! intro) and 254..257 (Ohana! idle / Ukulele / Blast / Shoot), played on his skeleton from the SUMMON's mset. Slot 232 is NOT an mset slot (Stitch has no
// exit motion): it is the leave cue of his object script (VERIFIED, bdx: stitch_effect_start 9 bound to 0x4000, then sysobj_fadeout 30 / obj_leave).
// guess 1 = inferred; 0 = taken from the mset frame triggers / the bdx script. Without the installed packs sora_sfx_id /
// sora_fx_id return -1 and every cue is silent.
#ifndef SORA_SUMMON_FX_ST_H
#define SORA_SUMMON_FX_ST_H
#include "sora_summon_fx.h"

// sorted by (who, slot, frame)
static const SmCue SM_CUES_ST[] = {""")
    for slot, frame, who, kind, name, bone, guess, note in cues:
        L.append('    {%d, %d, %s, %s, %d, 0, %s, "%s"},  // %s' % (slot, frame, "SMC_SORA" if who else "SMC_SUMMON", kinds[kind], guess,
                                                                 ("SM_BONE_ROOT" if bone == 0x4000 else str(bone)), name, note))
    L.append("};")
    L.append("#define SM_NCUES_ST ((int)(sizeof(SM_CUES_ST) / sizeof(SM_CUES_ST[0])))")
    L.append("""
// Packed: st_fx_1 (shot muzzle), 4 (Ukulele), 13 (Blast) since s138. Not packed / not cued: st_fx_0 (his on-screen Limit aura, 12 MB loop), 2, 3, 5, 8, 10, 11, 12 (script-driven; roles not decoded).
#endif""")
    open(path, "w", encoding="utf-8", newline="\n").write("\n".join(L) + "\n")
    print("wrote", path, len(cues), "cues")


def main():
    global FX_ELEMENTS
    ap = argparse.ArgumentParser()
    ap.add_argument("--sfx-in", required=True)
    ap.add_argument("--fx-in", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--header", required=True)
    ap.add_argument("--fx", default="6,7,9,1,4,13")
    ap.add_argument("--cap6", type=int, default=90, help="keep only the first N frames of hb040.6 (600 frames x 45k verts = 1.1 GB uncapped)")
    ap.add_argument("--no-fx", action="store_true")
    ap.add_argument("--no-sfx", action="store_true")
    ap.add_argument("--header-only", action="store_true", help="write the cue table without touching any bank (cue names are fixed)")
    a = ap.parse_args()
    FX_ELEMENTS = [int(x) for x in a.fx.split(",") if x]
    os.makedirs(a.out, exist_ok=True)
    trigs = [t for t in read_triggers() if t[0] in (0, 161, 201, 202, 203, 204, 205, 206, 231, 241, 242, 252, 253, 254, 255, 256, 257)]
    sfxinfo, nsnd = [], 0
    if a.header_only or a.no_sfx:
        # the cue rows are a pure function of the triggers: rebuild them without opening a bank
        for slot, frame, tid, par in trigs:
            if tid == 13:
                sfxinfo.append((slot, frame, "voice", "st_v%d" % par[0]))
            elif tid == 8 and par[0] == 598:
                sfxinfo.append((slot, frame, "sfx", "st_se%d" % par[1]))
    else:
        sfxinfo, nsnd = build_sfx(a.sfx_in, os.path.join(a.out, "sfx.bin"), trigs)
    fxrep = []
    if not (a.no_fx or a.header_only):
        fxrep = build_fx(a.fx_in, os.path.join(a.out, "fx.bin"), {6: a.cap6})
    meta = json.load(open(os.path.join(FXCACHE, "kh2_fx.json"), encoding="utf-8"))["effects"]
    cues = []
    for slot, frame, tid, par in trigs:
        who = 1 if slot in SORA_SLOTS else 0
        if tid == 7 and par[0] == 1000 and 1 in FX_ELEMENTS:   # (s138 ohana) the shot's muzzle flash, bone by arm (re:0x1C9920)
            cues.append((slot, frame, who, "fx", FX_PREFIX + "1", MUZZLE_BONE[par[1]], 0, "shot muzzle hb040.1, arm %d" % par[1]))
        if tid == 1 and par[0] in (6, 7, 9):
            nm = FX_PREFIX + str(par[0])
            bone = int(meta[FX_SRC_FMT % par[0]].get("bone", 0))
            cues.append((slot, frame, who, "fx", nm, bone, 0, "effect_caster hb040.%d, bind %#x" % (par[0], bone)))
    for slot, frame, who, el, bone in EXTRA_FX:
        if el in FX_ELEMENTS:
            cues.append((slot, frame, who, "fx", FX_PREFIX + str(el), bone, 1, "exe code starts hb040.%d with the command (GUESS: at the motion start)" % el))
    for slot, frame, kind, nm in sfxinfo:
        who = 1 if slot in SORA_SLOTS else 0
        cues.append((slot, frame, who, kind, nm, 0, 0, SUMMON_SLOTS.get(slot, "")))
    # the leave puff (VERIFIED in the bdx script, not an mset trigger): hb040.9 bound to 0x4000 when the summon leaves
    cues.append((232, 0, 0, "fx", FX_PREFIX + "9", 0x4000, 0, "leave: stitch_effect_start 9 bind 0x4000 (bdx 3468), then fadeout 30"))
    cues.sort(key=lambda c: (c[2], c[0], c[1], c[3] != "fx"))
    write_header(a.header, cues)
    if a.no_sfx or a.header_only: return
    print("pack files in", a.out)


if __name__ == "__main__":
    main()

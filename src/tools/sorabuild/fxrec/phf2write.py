"""phf2write.py (s161, rewritten fresh after review) - fxrec whole-file out dirs -> a PHF2 v4 kh2_fx.bin + kh2_fx.json.
Own code; only format / behaviour facts from recon/S161_HELPER_WRITER.md are used.

  python -E -P phf2write.py --out <dir> <whole-file out dir>[=<prefix>] [...]      (files are combined in the given order)

An out dir is what runfile.sh makes: <tag>.list, <tag>.fxname, <tag>.texsrc ("<rec> <run txt>": the run whose log holds that
recording's ELEMENT/TAIL line and TEXID table; missing file = <tag>_A.txt for all), <tag>.texhd/.fxhd (pkg lines),
<tag>.<rec>.fxv (s164 binary fxv2, preferred) or <tag>.<rec>.json (fxv1). Writes <dir>/kh2_fx.bin and <dir>/kh2_fx.json (via temp files + rename; refuses the PHH oracle).
"""
import hashlib
import itertools
import json
import os
import re
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # -P: fxvbin.py sits next to this file
import fxvbin  # noqa: E402

HEAD = struct.Struct("<4sIIIII")
TEXENT = struct.Struct("<II")
EFFENT = struct.Struct("<32sIIIIIhHI")
BATCH = struct.Struct("<HBB4B4B4Bf4f4f3f3f3f9fII")
VERT = struct.Struct("<3f3f2fHH4H")
assert BATCH.size == 132 and VERT.size == 44 and EFFENT.size == 60
KH2X = os.environ.get("FXREC_KH2", "D:/KH2Extract")
PKG_ORDER = ["kh2_first", "kh2_second", "kh2_third", "kh2_fourth", "kh2_fifth", "kh2_sixth"]
ORACLE_BIN = os.path.join(os.environ.get("LOCALAPPDATA", ""), "ProjectHeartHunters", "cache", "kh2_fx.bin")  # (s182: no user path in the release)
NO_TEX, PREV_TEX = 0xFFFF, 0xFFFE
ZLEVEL = 6
MAX_BYTES = 0xFFFFFFFF


def die(msg):
    raise SystemExit("phf2write: " + msg)


def protect(path):
    """never write onto the PHH oracle (or anything hard-linked to it)"""
    for o in (ORACLE_BIN, ORACLE_BIN[:-4] + ".json"):
        if os.path.exists(path) and os.path.exists(o) and os.path.samefile(path, o):
            die(f"{path} is the PHH oracle file {o}")


def replace_file(tmp, final):
    protect(final)
    os.replace(tmp, final)          # a new inode: any hard link keeps its old content


# ---------------------------------------------------------------------------- the KH2 effect file (PAX)
def kh2_path(name):
    hit = None
    for p in PKG_ORDER:
        c = f"{KH2X}/{p}/original/{name}"
        if os.path.isfile(c):
            hit = c
    return hit or die(f"{name} not found under {KH2X}")


def pax_table(path):
    """element id -> (bone, flags, fade, (tx, ty, tz)) from the first element carrying that id"""
    raw = open(path, "rb").read()
    base = 0
    if raw[:4] == b"BAR\x01":
        found = [struct.unpack_from("<I", raw, 0x18 + 0x10 * i)[0] for i in range(struct.unpack_from("<I", raw, 4)[0])
                 if struct.unpack_from("<H", raw, 0x10 + 0x10 * i)[0] == 0x12]
        if not found:
            die(f"{path}: no PAX entry")
        base = found[0]
    if raw[base:base + 4] != b"PAX_":
        die(f"{path}: not a PAX")
    table = {}
    for i in range(struct.unpack_from("<I", raw, base + 8)[0]):
        at = base + 0x10 + 0x50 * i
        ident = struct.unpack_from("<H", raw, at + 2)[0]
        if ident not in table:
            table[ident] = (struct.unpack_from("<h", raw, at + 6)[0], struct.unpack_from("<I", raw, at + 0x10)[0],
                            raw[at + 5], struct.unpack_from("<3f", raw, at + 0x1C))
    return table


# ---------------------------------------------------------------------------- textures
def dds32(w, h, bgra):
    """a plain 32-bit BGRA DDS: 124-byte header (one mip) + the texels"""
    words = [124, 0x100F, h, w, w * 4, 0, 1] + [0] * 11 + [32, 0x41, 0, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000] + [0x1000, 0, 0, 0, 0]
    return struct.pack("<4s31I", b"DDS ", *words) + bytes(bgra)


def as_dds(blob):
    """KH2 ships a few HD pages as PNG: decode to the same texels in a 32-bit DDS; DDS stays as it is"""
    if not blob.startswith(b"\x89PNG"):
        return blob
    import io
    from PIL import Image
    img = Image.open(io.BytesIO(blob)).convert("RGBA")
    rgba = img.tobytes()
    out = bytearray(len(rgba))
    out[0::4], out[1::4], out[2::4], out[3::4] = rgba[2::4], rgba[1::4], rgba[0::4], rgba[3::4]
    return dds32(img.width, img.height, out)


class TexStore:
    """textures in first-use order, one per (file, asset); PS2-data pages: one EMPTY texture per (file, None) (= PHH)"""

    def __init__(self):
        self.blobs, self.slot = [], {}
        self.total = 0

    def get(self, fileset, ref):
        key = (ref[0], ref[2])
        if key not in self.slot:
            self.slot[key] = len(self.blobs)
            self.blobs.append(fileset.read_asset(*ref) if ref[2] is not None else b"")
            self.total += len(self.blobs[-1]) + 16
        return self.slot[key]


class FileSet:
    def __init__(self, d, prefix):
        self.dir, self.prefix = d, prefix
        at = lambda ext: os.path.join(d, prefix + ext)
        self.fxname = open(at(".fxname")).read().strip()
        self.recs = open(at(".list")).read().split()
        self.src = {r: prefix + "_A.txt" for r in self.recs}
        if os.path.exists(at(".texsrc")):
            self.src = {}
            for ln in open(at(".texsrc")):
                f = ln.split()
                if f:
                    self.src[f[0]] = f[1]
        for r in self.recs:
            if r not in self.src:
                die(f"{prefix}: recording {r} has no run txt in {prefix}.texsrc")
        self.pkg = {}
        for tag, ext in (("texcommon", ".texhd"), ("fx", ".fxhd")):
            if os.path.exists(at(ext)):
                self.pkg[tag] = open(at(ext)).readline().split()[1]
        self._logs = {}

    def log(self, txt):
        """(states, texture refs) of one run log: states[rec] = killed?, refs[n] = (file, kind, asset)"""
        if txt not in self._logs:
            text = open(os.path.join(self.dir, txt), encoding="utf-8", errors="replace").read()
            states = {m.group(1): m.group(2) == "killed"
                      for m in re.finditer(r"^(?:ELEMENT|TAIL) (\S+) (ended|killed) after", text, re.M)}
            refs = {}
            for m in re.finditer(r"^TEXID (\d+) (\S+) (.*)$", text, re.M):
                if m.group(2) == "ps2file":         # s162: a PS2-data page of a loaded file (recorder fxrec_s2.c)
                    kind = m.group(3).split()[0]
                    refs[int(m.group(1))] = ("effect/texcommon.dpd" if kind == "texcommon" else self.fxname, kind, None)
                    continue
                if m.group(2) != "hd":
                    die(f"{txt}: TEXID {m.group(1)} is '{m.group(2)} {m.group(3)}' (not an HD texture)")
                kind, asset = m.group(3).split()[:2]
                refs[int(m.group(1))] = ("effect/texcommon.dpd" if kind == "texcommon" else self.fxname, kind, asset)
            self._logs[txt] = (states, refs)
        return self._logs[txt]

    def killed(self, rec):
        states = self.log(self.src[rec])[0]
        if rec not in states:
            die(f"{self.prefix}: no ELEMENT/TAIL line for {rec} in {self.src[rec]}")
        return states[rec]

    def read_asset(self, rel, kind, asset):
        first = [self.pkg[kind]] if kind in self.pkg else []
        for p in first + [q for q in reversed(PKG_ORDER) if q not in first]:
            f = f"{KH2X}/{p}/remastered/{rel}/{asset}"
            if os.path.isfile(f):
                return as_dds(open(f, "rb").read())
        die(f"HD asset missing: {rel}/{asset}")

    def order(self):
        """ids ascending; each id's tails (by start frame) before the id itself"""
        tails, plain = {}, set()
        for r in self.recs:
            if "~" in r:
                a, b = r.split("~")
                tails.setdefault(int(a), []).append(int(b))
            else:
                plain.add(int(r))
        out = []
        for i in sorted(plain | set(tails)):
            out += [f"{i}~{t}" for t in sorted(tails.get(i, ()))]
            if i in plain:
                out.append(str(i))
        return out


# ---------------------------------------------------------------------------- one recording, streamed
# s163: fxsolve's NaNs come from x86 float math (e.g. inf - inf) = the x86 "default NaN" fff8000000000000 (f32 ffc00000),
# the same bits PHH writes. JSON keeps no NaN sign ("NaN" reads back as +7ff8.. -> f32 7fc00000): w_ex010_j0.2 f16-49 had 4896
# such floats = the only bytes of the whole kh2_fx.bin that differed from PHH. So "NaN" reads back as the x86 default NaN.
X86_NAN = struct.unpack("<d", bytes.fromhex("000000000000f8ff"))[0]


def _const(s):
    return X86_NAN if s == "NaN" else float(s)


def frames_of(path):
    """yield the frame objects of one recording one by one: <name>.fxv (s164 binary fxv2: batches carry "vb" = ready
    vertex bytes, exact bits) or else <name>.json (fxv1; the file text is held, the parsed objects are not)"""
    if os.path.exists(path[:-5] + ".fxv"):
        yield from fxvbin.read(path[:-5] + ".fxv")[1]
        return
    text = open(path, encoding="utf-8").read()
    head = text.find('"perFrame": [')
    if head < 0:
        die(f"{path}: no perFrame")
    dec = json.JSONDecoder(parse_constant=_const)
    i = head + len('"perFrame": [')
    while True:
        while text[i] in " ,\n":
            i += 1
        if text[i] == "]":
            return
        obj, i = dec.raw_decode(text, i)
        yield obj


MERGE_SKIP = ("anchor", "anchorOff", "anchorAge", "vertexCount", "p", "o", "uv", "age", "c", "tex", "vb")
VFIELDS = ("p", "o", "uv", "age", "c")


def merged(batches, tex_slot):
    """s162 review: PHH merges neighbouring unsorted batches by the BIN texture slot (fxsolve could only use the
    recorder's id): two recorder ids of one (file, asset) must become one batch, as in PHH. The first keeps its anchors."""
    out = []
    for b in batches:
        t = b["tex"] if b["tex"] in (NO_TEX, PREV_TEX) else tex_slot(b["tex"])
        key = (t, json.dumps({k: v for k, v in b.items() if k not in MERGE_SKIP}, sort_keys=True))
        if out and not (b["flags"] & 2) and not (out[-1][2]["flags"] & 2) and out[-1][0] == key:
            m = out[-1][2]
            if "vb" in b:
                m["vb"].append(b["vb"])
            else:
                for f in VFIELDS:
                    m[f] = m[f] + b[f]
            m["vertexCount"] += b["vertexCount"]
        else:
            out.append((key, t, dict(b, vb=[b["vb"]]) if "vb" in b else dict(b)))
    return [(t, b) for _, t, b in out]


def frame_bytes(batches, tex_slot):
    batches = merged(batches, tex_slot)
    out = [struct.pack("<I", len(batches))]
    for t, b in batches:
        out.append(BATCH.pack(t, b["program"], b["flags"], *b["blend"], *b["depth"], *b["sampler"], b["alphaRef"], *b["fog"],
                              *b["tfog"], *b["fogColour"], *b["anchor"], *b["anchorOff"], *b["upright"], b["anchorAge"],
                              b["vertexCount"]))
        if "vb" in b:                  # s164 fxv2: the vertices are already the packed 44-byte records
            out += b["vb"]             # (merged() keeps them as a list of pieces)
            continue
        out += [VERT.pack(*b["p"][k], *b["o"][k], *b["uv"][k], b["age"][k], 0, *b["c"][k]) for k in range(b["vertexCount"])]
    return b"".join(out)


def frame_digest(batches):
    """what 'the same frame' means for the loop test: every batch field but the anchor age"""
    h = hashlib.blake2b(digest_size=16)
    for b in batches:
        h.update(json.dumps({k: v for k, v in b.items() if k != "anchorAge"}, sort_keys=True).encode())
    return h.digest()


def loop_of(digests):
    """[period, warmup] of the smallest period (< 200) from which the frames repeat exactly, leaving 60 frames to spare; else None"""
    n = len(digests)
    for p in range(1, 200):
        last_diff = max((f for f in range(n - p) if digests[f] != digests[f + p]), default=-1)
        if last_diff + 1 < n - p - 60:
            return [p, last_diff + 1]
    return None


def pack_body(chunks):
    """frame-count + 1 offsets (relative to the end of the table), then the chunks"""
    offs = [0] + list(itertools.accumulate(len(c) for c in chunks))
    return struct.pack(f"<{len(offs)}I", *offs) + b"".join(chunks)


# ---------------------------------------------------------------------------- the whole bin
def build(out_dir, sets):
    os.makedirs(out_dir, exist_ok=True)
    binp, jsonp = os.path.join(out_dir, "kh2_fx.bin"), os.path.join(out_dir, "kh2_fx.json")
    protect(binp)
    protect(jsonp)
    tex = TexStore()
    rows = []                   # per recording: what goes to the effect table + json
    names = set()
    tmpbody = binp + ".bodies.tmp"
    tmpbin = binp + ".new"
    pos = 0
    try:
        with open(tmpbody, "wb") as bodies:
            for fs in sets:
                pax = pax_table(kh2_path(fs.fxname))
                for rec in fs.order():
                    name = f"{fs.prefix}.{rec}"
                    if name in names:
                        die(f"duplicate element name {name}")
                    names.add(name)
                    if len(name.encode()) > 31:
                        print(f"warning: name {name} is longer than 31 bytes and is cut in the table", file=sys.stderr)
                    ident = int(rec.split("~")[0])
                    if ident not in pax:
                        die(f"{name}: element id {ident} is not in the PAX of {fs.fxname}")
                    bone, flags, fade, trans = pax[ident]
                    cut = fs.killed(rec)
                    refs = fs.log(fs.src[rec])[1]
                    chunks, rawlen, digests = [], 0, []
                    want_loop = cut and "~" not in rec

                    def slot(n, name=name, refs=refs, fs=fs, rec=rec):
                        if n not in refs:
                            die(f"{name}: tex id {n} not in the TEXID table of {fs.src[rec]}")
                        return tex.get(fs, refs[n])
                    for fr in frames_of(os.path.join(fs.dir, f"{name}.json")):
                        for n in fr.get("texuse", ()):   # s162: PHH's numbering = every textured draw, in draw order
                            slot(n)
                        if want_loop:                     # s162: PHH's period key = the raw run-A draws (fxsolve loopkey)
                            digests.append(fr["loopkey"] if "loopkey" in fr else frame_digest(fr["batches"]))
                        raw = frame_bytes(fr["batches"], slot)
                        rawlen += len(raw)
                        chunks.append(zlib.compress(raw, ZLEVEL))
                    body = pack_body(chunks)
                    padded = len(body) + (-len(body) % 16)
                    if 24 + 8 * len(tex.blobs) + 60 * (len(rows) + 1) + tex.total + pos + padded > MAX_BYTES:
                        die("the bin would pass 4 GiB (u32 offsets)")
                    bodies.write(body + bytes(padded - len(body)))
                    rows.append(dict(name=name, off=pos, size=len(body), raw=rawlen, frames=len(chunks), cut=cut, bone=bone,
                                     flags=flags, fade=fade, trans=trans, file=fs.fxname, rec=rec, ident=ident,
                                     period=loop_of(digests) if want_loop else None))
                    pos += padded
        nt, ne = len(tex.blobs), len(rows)
        head = bytearray(24 + 8 * nt + 60 * ne)
        at = len(head)
        for i, b in enumerate(tex.blobs):
            TEXENT.pack_into(head, 24 + 8 * i, at, len(b))
            at += len(b) + (-(at + len(b)) % 16)
        HEAD.pack_into(head, 0, b"PHF2", 4, nt, ne, 24, 24 + 8 * nt)
        for i, r in enumerate(rows):
            EFFENT.pack_into(head, 24 + 8 * nt + 60 * i, r["name"].encode()[:31], at + r["off"], r["size"], r["raw"], r["frames"],
                             int(r["cut"]), r["bone"], r["flags"] & 0xFFFF, 0)
        with open(tmpbin, "wb") as f:
            f.write(head)
            for b in tex.blobs:
                f.write(b)
                f.write(bytes(-f.tell() % 16))
            if f.tell() != at:
                die("internal: layout mismatch")
            with open(tmpbody, "rb") as src:
                while True:
                    c = src.read(1 << 24)
                    if not c:
                        break
                    f.write(c)
            size = f.tell()
        replace_file(tmpbin, binp)
    finally:
        for t in (tmpbody, tmpbin):
            if os.path.exists(t):
                os.remove(t)
    # json: the tail count of a base element is known only after the pass
    ntails = {}
    for r in rows:
        if "~" in r["rec"]:
            key = (r["name"].split(".")[0], r["ident"])
            ntails[key] = max(ntails.get(key, 0), int(r["rec"].split("~")[1]) + 1)
    eff = {}
    for i, r in enumerate(rows):
        tail = "~" in r["rec"]
        e = {"index": i, "frames": r["frames"], "loops": 0 if tail else int(r["cut"]), "cut": r["cut"], "bone": r["bone"],
             "flags": r["flags"], "fade": int(r["fade"]), "file": r["file"], "t": [round(float(x), 4) for x in r["trans"]]}
        n = ntails.get((r["name"].split(".")[0], r["ident"]))
        if n is not None and not tail:
            e["tails"] = n
        if r["cut"] and not tail:
            e["period"] = r["period"]
        eff[r["name"]] = e
    doc = {"_meta": {"source": "tools/sorabuild/fxrec/phf2write.py", "kh_exe_sha1": "2548F1191FE6587A34947952763CE6BB4F3B5C1A",
                     "format": "PHF2 v4"}, "effects": eff, "report": {"elements": ne, "stats": {}}}
    tmpj = jsonp + ".new"
    try:
        with open(tmpj, "w", encoding="utf-8", newline="\n") as f:
            json.dump(doc, f, indent=1)
        replace_file(tmpj, jsonp)
    finally:                    # s162 review: a failed json write leaves no temp behind (recordall would refuse the dir)
        if os.path.exists(tmpj):
            os.remove(tmpj)
    return size, nt, ne


def main(a):
    if "--out" not in a or len(a) < 3:
        print(__doc__)
        return 2
    out = a[a.index("--out") + 1]
    dirs = [x for i, x in enumerate(a) if x != "--out" and (i == 0 or a[i - 1] != "--out")]
    sets = []
    for d in dirs:
        d, _, pre = d.partition("=")
        sets.append(FileSet(d, pre or os.path.basename(d.rstrip("/\\"))))
    size, nt, ne = build(out, sets)
    print(f"-> {out}/kh2_fx.bin {size / 1e6:.1f} MB, {nt} textures, {ne} elements")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

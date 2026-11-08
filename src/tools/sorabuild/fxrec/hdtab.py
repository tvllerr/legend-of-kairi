"""s160: HD texture table of one KH2 file = what hd_lookup 0x12AA20 answers (own code, format from OpenKH hed.md).

  python -E -P hdtab.py <KH2Extract file path | name> [--image DIR] [--out FILE]

hed entry 0x20: md5(name) [16], offset i64, data length i32, actual length i32. pkg asset at offset: 16-byte header
{decompressed length, remastered count, compressed length, date}, then count x 0x30 remastered headers
(name[0x20], i32 offset +0x20, i32 original-file offset +0x24, decompressed +0x28, compressed +0x2C).
KH2 masks the original offset with 0x1FFFFFFF (0x12AA20). Packages are read in the game's patch order kh2_first ..
kh2_sixth and the LAST one holding the name wins. Vanilla only: a package with a backup/ copy (mod installs) is read
from backup/. Output lines: "pkg <pkg> size <actual length>" then "<offset hex> <asset name>" per HD asset.
"""
import argparse, hashlib, os, struct, sys

PKGS = ["kh2_first", "kh2_second", "kh2_third", "kh2_fourth", "kh2_fifth", "kh2_sixth"]
IMAGE = (os.path.join(os.path.dirname(os.environ["FXREC_GAMEEXE"]), "Image", "dt") if os.environ.get("FXREC_GAMEEXE")
         else "D:/SteamLibrary/steamapps/common/KINGDOM HEARTS -HD 1.5+2.5 ReMIX-/Image/dt")  # sorabuild sets FXREC_GAMEEXE


def pkg_path(image, pkg, ext):
    b = os.path.join(image, "backup", pkg + ext)
    return b if os.path.exists(b) else os.path.join(image, pkg + ext)


def find(image, name):
    want = hashlib.md5(name.encode("ascii")).digest()
    hit = None
    for pkg in PKGS:
        h = open(pkg_path(image, pkg, ".hed"), "rb").read()
        for i in range(0, len(h), 0x20):
            if h[i:i + 16] == want:
                off, dlen, alen = struct.unpack_from("<qii", h, i + 16)
                hit = (pkg, off, alen)
    return hit


def table(image, name):
    hit = find(image, name)
    if hit is None:
        raise SystemExit(f"hdtab: {name} not in any package")
    pkg, off, alen = hit
    with open(pkg_path(image, pkg, ".pkg"), "rb") as f:
        f.seek(off)
        dec, count, comp, date = struct.unpack("<iiii", f.read(16))
        if dec != alen:
            raise SystemExit(f"hdtab: {name} header length {dec} != hed {alen}")
        rows = []
        for _ in range(count):
            hd = f.read(0x30)
            nm = hd[:0x20].split(b"\0", 1)[0].decode("ascii")
            rows.append((struct.unpack_from("<i", hd, 0x24)[0] & 0x1FFFFFFF, nm))
    return pkg, alen, rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    ap.add_argument("--image", default=IMAGE)
    ap.add_argument("--out")
    a = ap.parse_args()
    p = a.file.replace("\\", "/")
    name = p.split("/original/", 1)[1] if "/original/" in p else p
    pkg, alen, rows = table(a.image, name)
    if os.path.exists(a.file) and os.path.getsize(a.file) != alen:
        raise SystemExit(f"hdtab: {a.file} is {os.path.getsize(a.file)} bytes, package {pkg} has {alen}")
    if "/original/" in p and f"/{pkg}/" not in p:
        print(f"hdtab: note: {name} wins from {pkg} (path says otherwise)", file=sys.stderr)
    out = open(a.out, "w", newline="\n") if a.out else sys.stdout
    out.write(f"pkg {pkg} size {alen}\n")
    last = {}
    for o, nm in rows:                 # several headers on one offset: the LAST one answers (a dict, as PHH builds it)
        if o in last:
            print(f"hdtab: note: offset 0x{o:X} in {name}: {last[o]} replaced by {nm}", file=sys.stderr)
        last[o] = nm
    for o, nm in rows:
        if last.get(o) == nm:
            out.write(f"{o:X} {nm}\n")
            del last[o]
    return 0


if __name__ == "__main__":
    sys.exit(main())

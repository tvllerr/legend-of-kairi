"""mklinuxkit (s280, Deck release 2 step 1): make the LINUX friend kit, on Linux (Steam Deck), from a Windows kit + pinned downloads.

  python3 kit/mklinuxkit.py --base <Windows kit dir (made by mkkit.py)> --dl <dir with the pinned downloads (pins.txt)>
      --out <new dir> [--proton "<Steam>/steamapps/common/Proton 11.0"]

What it takes (sorabuild config "kit": <out> works the same as the Windows kit, sb.EXE = "" on Linux):
  tools/<tool>/   the SAME framework-dependent .NET DLLs as the Windows kit (no rebuild: same bytes, only the platform differs);
                  the Windows apphost <tool>.exe is replaced by an sh wrapper <tool> = `dotnet <tool>.dll` (DOTNET_ROOT from sorabuild)
  tools/fxrec_boot/fxrec_boot.exe   the Windows recorder as it is (it maps the Windows KH2 exe): runs under wine/wine.sh
  dotnet/         .NET 8.0.32 runtime linux-x64 (Microsoft tarball, sha512 pinned)
  python/         python-build-standalone 3.12.10 install_only + numpy 2.5.3 / Pillow 12.3.0 / oead 1.3.0 manylinux wheels
                  (the same versions as the Windows kit)
  ffmpeg/ffmpeg   BtbN static linux64 gpl n9.0.2 (the Windows kit has gyan 9.0.2)
  vgmstream/vgmstream-cli   official r2117 vgmstream-linux.zip (static)
  wine/wine.sh    (with --proton) starts a Windows exe with that Proton's wine, prefix <kit>/wine/pfx (made on first use)
  openkh/, LICENSES/  from the Windows kit (+ the Linux inputs' license texts)
Every download is checked against pins.txt (name alg hash url) before it is used. No game bytes.
"""
import argparse
import hashlib
import json
import os
import shutil
import stat
import subprocess
import sys
import tarfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
SBDIR = os.path.dirname(HERE)
sys.path.insert(0, SBDIR)
sys.path.insert(0, HERE)
import sb  # noqa: E402
from mkkit import write_kit_json  # noqa: E402

DOTNET_TOOLS = ["sorabake", "partybake", "gobake", "skatebake", "hudbake", "trigjson", "kh2pick"]
WRAP = """#!/bin/sh
# (s280) Linux kit: the framework-dependent {t}.dll on the kit's .NET runtime (sorabuild sets DOTNET_ROOT)
exec "${{DOTNET_ROOT:?DOTNET_ROOT not set (sorabuild sets it)}}/dotnet" "$(dirname "$0")/{t}.dll" "$@"
"""
WINE = """#!/bin/sh
# (s280) Linux kit: start a Windows exe (fxrec_boot.exe) with Proton's wine; own prefix next to this script, no Steam needed
D="$(cd "$(dirname "$0")" && pwd)"
export WINEPREFIX="${{KAIRI_WINEPREFIX:-$D/pfx}}" WINEDEBUG="${{WINEDEBUG:--all}}" WINEDLLOVERRIDES="${{WINEDLLOVERRIDES:-mscoree,mshtml=}}"
exec "{wine}" "$@"
"""


def sha(p, alg):
    h = hashlib.new(alg)
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest()


def pinned(dl, prefix):
    """the one pins.txt entry whose name starts with prefix, checked"""
    hits = []
    for ln in open(os.path.join(dl, "pins.txt"), encoding="utf-8"):
        p = ln.split()
        if len(p) >= 3 and p[0].startswith(prefix):
            hits.append(p)
    if len(hits) != 1:
        raise SystemExit("mklinuxkit: %d pins for %s" % (len(hits), prefix))
    name, alg, want = hits[0][:3]
    path = os.path.join(dl, name)
    if sha(path, alg) != want:
        raise SystemExit("mklinuxkit: %s is not the pinned file (%s)" % (path, alg))
    print("pinned OK  %s" % name, flush=True)
    return path, hits[0]


def chmodx(p):
    os.chmod(p, os.stat(p).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def safe_extract(tf, dst):
    for m in tf.getmembers():
        if m.name.startswith("/") or ".." in m.name.split("/"):
            raise SystemExit("mklinuxkit: unsafe path in archive: %s" % m.name)
    tf.extractall(dst, filter="tar") if hasattr(tarfile, "data_filter") else tf.extractall(dst)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--dl", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--proton", default="")
    a = ap.parse_args()
    if sys.platform == "win32":
        raise SystemExit("mklinuxkit: run it on Linux (the archives hold symlinks and exec bits)")
    out = os.path.abspath(a.out)
    if os.path.exists(out) and os.listdir(out):
        raise SystemExit("mklinuxkit: %s is not empty" % out)
    os.makedirs(out, exist_ok=True)
    base = os.path.abspath(a.base)
    bk = json.load(open(os.path.join(base, "kit.json"), encoding="utf-8"))
    stale = sb.kit_stale(bk)
    if stale:
        raise SystemExit("mklinuxkit: the base kit's %s were built from other sources than this checkout" % ", ".join(stale))
    bfiles = bk["files"]

    def base_copy(rel_dir, dst, skip=()):
        n = 0
        for d, _, fs in os.walk(os.path.join(base, rel_dir)):
            for f in fs:
                p = os.path.join(d, f)
                rel = os.path.relpath(p, base).replace("\\", "/")
                if f in skip:
                    continue
                if bfiles.get(rel) != sha(p, "sha256"):
                    raise SystemExit("mklinuxkit: base kit file changed since its kit.json: %s" % rel)
                q = os.path.join(dst, os.path.relpath(p, os.path.join(base, rel_dir)))
                os.makedirs(os.path.dirname(q), exist_ok=True)
                shutil.copyfile(p, q)
                n += 1
        return n

    # .NET tools: same DLLs, sh wrapper instead of the apphost
    for t in DOTNET_TOOLS:
        td = os.path.join(out, "tools", t)
        n = base_copy("tools/" + t, td, skip=(t + ".exe",))
        w = os.path.join(td, t)
        open(w, "w", newline="\n").write(WRAP.format(t=t))
        chmodx(w)
        print("tools/%s: %d files + wrapper" % (t, n))
    base_copy("tools/fxrec_boot", os.path.join(out, "tools", "fxrec_boot"))
    base_copy("openkh", os.path.join(out, "openkh"))
    base_copy("LICENSES", os.path.join(out, "LICENSES"))
    pins = []

    p, pin = pinned(a.dl, "dotnet-runtime-8.0.32-linux-x64")
    pins.append(pin)
    with tarfile.open(p) as tf:
        safe_extract(tf, os.path.join(out, "dotnet"))
    if not os.path.isdir(os.path.join(out, "dotnet", "shared", "Microsoft.NETCore.App", "8.0.32")):
        raise SystemExit("mklinuxkit: no Microsoft.NETCore.App 8.0.32 in the runtime tarball")

    p, pin = pinned(a.dl, "cpython-3.12.10+")
    pins.append(pin)
    with tarfile.open(p) as tf:
        safe_extract(tf, out)          # top dir "python/"
    site = os.path.join(out, "python", "lib", "python3.12", "site-packages")
    for pre in ("numpy-2.5.3-cp312", "pillow-12.3.0-cp312", "oead-1.3.0-cp312"):
        p, pin = pinned(a.dl, pre)
        pins.append(pin)
        with zipfile.ZipFile(p) as z:
            for n in z.namelist():
                if n.startswith("/") or ".." in n.split("/"):
                    raise SystemExit("mklinuxkit: unsafe path in %s: %s" % (p, n))
            z.extractall(site)
    for d in os.listdir(site):                          # <pkg>.data/platlib/* -> site-packages (what pip does)
        plat = os.path.join(site, d, "platlib")
        if d.endswith(".data") and os.path.isdir(plat):
            for n in os.listdir(plat):
                shutil.move(os.path.join(plat, n), os.path.join(site, n))
    for d, _, fs in os.walk(site):                     # zip extract drops exec bits: shared objects only need read
        for f in fs:
            if ".so" in f:
                chmodx(os.path.join(d, f))
    py = os.path.join(out, "python", "bin", "python3")
    chk = subprocess.run([py, "-E", "-P", "-c", "import numpy, PIL.Image, oead, sys; print(sys.version.split()[0], numpy.__version__)"],
                         env={"PATH": "/usr/bin:/bin"}, capture_output=True, text=True)
    if chk.returncode:
        raise SystemExit("mklinuxkit: the kit python cannot import numpy / Pillow / oead:\n" + chk.stderr)
    print("python OK", chk.stdout.strip())

    p, pin = pinned(a.dl, "ffmpeg-n9.0.2")
    pins.append(pin)
    os.makedirs(os.path.join(out, "ffmpeg"))
    with tarfile.open(p) as tf:
        for m in tf.getmembers():
            b = os.path.basename(m.name)
            if m.isfile() and (m.name.endswith("/bin/ffmpeg") or b == "LICENSE.txt"):
                dst = os.path.join(out, "ffmpeg", "ffmpeg" if b == "ffmpeg" else "LICENSE.txt")
                with tf.extractfile(m) as s, open(dst, "wb") as d:
                    shutil.copyfileobj(s, d)
    chmodx(os.path.join(out, "ffmpeg", "ffmpeg"))
    shutil.copyfile(os.path.join(out, "ffmpeg", "LICENSE.txt"), os.path.join(out, "LICENSES", "ffmpeg-linux-LICENSE.txt"))

    p, pin = pinned(a.dl, "vgmstream-linux.zip")
    pins.append(pin)
    os.makedirs(os.path.join(out, "vgmstream"))
    with zipfile.ZipFile(p) as z:
        open(os.path.join(out, "vgmstream", "vgmstream-cli"), "wb").write(z.read("vgmstream-cli"))
    chmodx(os.path.join(out, "vgmstream", "vgmstream-cli"))
    shutil.copyfile(os.path.join(out, "python", "LICENSE.txt") if os.path.isfile(os.path.join(out, "python", "LICENSE.txt"))
                    else os.path.join(out, "python", "lib", "python3.12", "LICENSE.txt"),
                    os.path.join(out, "LICENSES", "Python-linux-LICENSE.txt"))

    if a.proton:
        wine = os.path.join(a.proton, "files", "bin", "wine")
        if not os.path.isfile(wine):
            raise SystemExit("mklinuxkit: no wine at %s" % wine)
        os.makedirs(os.path.join(out, "wine"))
        w = os.path.join(out, "wine", "wine.sh")
        open(w, "w", newline="\n").write(WINE.format(wine=wine))
        chmodx(w)

    for t in ("ffmpeg/ffmpeg", "vgmstream/vgmstream-cli", "dotnet/dotnet"):
        r = subprocess.run([os.path.join(out, t), "-version" if "ffmpeg" in t else "--info" if "dotnet" in t else "-V"],
                           capture_output=True, text=True)
        print("%s: exit %d  %s" % (t, r.returncode, (r.stdout or r.stderr).strip().splitlines()[:1]))
    write_kit_json(out, {t: sb.src_hash(t) for t in sb.KIT_SRC},
                   {"platform": "linux-x86_64", "base": {"kit": os.path.basename(base), "made": bk.get("made")},
                    "pins": [" ".join(p) for p in pins]})


if __name__ == "__main__":
    main()

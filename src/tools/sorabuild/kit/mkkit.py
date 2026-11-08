"""mkkit: make the friend kit (s168, installer item 3+4) - dev PC only, friends never run this.

  python -E -P kit/mkkit.py --out D:/SoraBotW/sb/kit --dotnet D:/Tools/dotnet8/dotnet.exe --openkh D:/KH2BusterSword/conv/out
      --openkh-apps D:/Tools/OpenKH/openkh/Apps --busybox <busybox-w64-...exe> --python <embeddable python dir with site-packages>
      --ffmpeg <ffmpeg.exe of gyan 9.0.2 full_build> [--vgmstream <dir with vgmstream-cli.exe + dlls>] [--clang C:/msys64/ucrt64/bin/clang.exe]

Kit layout (sorabuild config "kit": <dir>):
  kit.json            {"src": {tool: sb.src_hash(tool)}, "files": {rel: sha256}, ...}: sorabuild refuses a tool whose sources changed
  tools/<tool>/       sorabake partybake gobake skatebake hudbake trigjson (dotnet build -c Release, the SAME command as the steps)
                      fxrec_boot (clang, sb.fxrec_cmd) kh2pick (only-the-needed-files KH2 extractor)
  dotnet/             .NET 8 runtime only (dotnet.exe + host/fxr + shared/Microsoft.NETCore.App), from the SDK: DOTNET_ROOT for the tools
  openkh/             OpenKH release DLLs (only the sorabuild input probe; each tool dir has its own copies)
  busybox/sh.exe      busybox-w32 (runfile.sh: jobs, wait, xargs -P) instead of Git bash / MSYS2
  python/             python.org 3.12 embeddable + numpy / Pillow / oead (runs sorabuild itself)
  ffmpeg/ffmpeg.exe   sfx_skate resample (gyan 9.0.2 full_build; output matched to it)
  vgmstream/          vgmstream-cli r2117 (sha256 pinned in sounds/build_sounds.py)
  LICENSES/           (s176) every license text + README.txt index (kit/licenses + the texts shipped with the inputs)
Only our code + redistributable tools; no game bytes.
"""
import argparse
import glob
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
SBDIR = os.path.dirname(HERE)
sys.path.insert(0, SBDIR)
import sb  # noqa: E402

CS = {  # tool -> (source dir, msbuild property for the OpenKH dir)
    "sorabake": ("cs/sorabake", "OpenKhDir"), "partybake": ("cs/partybake", "OpenKhDir"), "gobake": ("cs/gobake", "OpenKhDir"),
    "skatebake": ("cs/skatebake", "OpenKhDir"), "trigjson": ("fx/trigjson", "OpenKhDir"), "hudbake": ("hud/hudbake", "OpenKh"),
}


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest()


def run(args, cwd=None, env=None):
    e = dict(os.environ)
    e.update(env or {})
    print("$ " + " ".join(args), flush=True)
    r = subprocess.run(args, cwd=cwd, env=e)
    if r.returncode:
        raise SystemExit("mkkit: failed (%d): %s" % (r.returncode, " ".join(args)))


WV2_VER = "1.0.4258.31"
WV2_SHA = "56f7f4b8bf9aee4b8efefbbdd4f67d5f74ebd1b100ed0806da71bf76af481aa9"


def make_licenses(out, webview2):
    """(s176) LICENSES/: the static texts in kit/licenses (GPL / LGPL / Apache / vgmstream / Savage Wind + README index) and the
    texts that come WITH the shipped inputs (python, numpy / pillow / oead dist-info, .NET, WebView2 nupkg, Fira Sans OFL).
    Every file the README names must exist, or mkkit stops."""
    import zipfile
    d = os.path.join(out, "LICENSES")
    shutil.copytree(os.path.join(HERE, "licenses"), d)
    site = os.path.join(out, "python", "Lib", "site-packages")
    take = {"Python-LICENSE.txt": os.path.join(out, "python", "LICENSE.txt"),
            "dotnet-LICENSE.txt": os.path.join(out, "dotnet", "LICENSE.txt"),
            "dotnet-ThirdPartyNotices.txt": os.path.join(out, "dotnet", "ThirdPartyNotices.txt"),
            "FiraSans-OFL.txt": os.path.join(SBDIR, "..", "installer", "ui", "fonts", "FiraSans-OFL.txt")}
    for pkg, name in (("numpy", "LICENSE.txt"), ("pillow", "LICENSE"), ("oead", "LICENSE.md")):
        hit = glob.glob(os.path.join(site, "%s-*.dist-info" % pkg, "licenses", name))
        if len(hit) != 1:
            raise SystemExit("mkkit: %s license: %d matches in %s" % (pkg, len(hit), site))
        take["%s-%s%s" % (pkg, name, "" if os.path.splitext(name)[1] else ".txt")] = hit[0]
    for dst, src in take.items():
        if not os.path.isfile(src):
            raise SystemExit("mkkit: license text missing: %s" % src)
        shutil.copy2(src, os.path.join(d, dst))
    if webview2:
        with zipfile.ZipFile(os.path.join(webview2, "microsoft.web.webview2.%s.nupkg" % WV2_VER)) as z:
            for n in ("LICENSE.txt", "NOTICE.txt"):
                with open(os.path.join(d, "WebView2-" + n), "wb") as f:
                    f.write(z.read(n))
    readme = open(os.path.join(d, "README.txt"), encoding="utf-8").read()
    named = {w.strip(",") for w in readme.split() if w.strip(",").endswith((".txt", ".md")) and w.strip(",") != "README.txt"}
    if not webview2:
        named -= {"WebView2-LICENSE.txt", "WebView2-NOTICE.txt"}
    missing = sorted(n for n in named if not os.path.isfile(os.path.join(d, n)))
    if missing:
        raise SystemExit("mkkit: LICENSES/README.txt names files that are not there: %s" % ", ".join(missing))
    print("mkkit: LICENSES/ %d files" % len(os.listdir(d)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--dotnet", required=True)
    ap.add_argument("--openkh", required=True)
    ap.add_argument("--openkh-apps", required=True)
    ap.add_argument("--busybox", required=True)
    ap.add_argument("--python", required=True)
    ap.add_argument("--ffmpeg", required=True)
    ap.add_argument("--vgmstream", default=os.path.join(SBDIR, "sounds", "bin"))
    ap.add_argument("--clang", default="C:/msys64/ucrt64/bin/clang.exe")
    ap.add_argument("--webview2", help="(s168) dir with microsoft.web.webview2.%s.nupkg: builds the installer window "
                    "(tools/installer_host) and ships Microsoft.WindowsDesktop.App" % WV2_VER)
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    if os.path.exists(out) and os.listdir(out):
        raise SystemExit("mkkit: %s is not empty (use a new dir)" % out)
    os.makedirs(out, exist_ok=True)
    tmp = os.path.join(out, "_build")
    sdk = os.path.dirname(a.dotnet)
    env = {"DOTNET_ROOT": sdk, "DOTNET_CLI_TELEMETRY_OPTOUT": "1", "DOTNET_NOLOGO": "1", "DOTNET_CLI_HOME": os.path.join(tmp, "home")}

    # C# tools: copy the sources out (no obj/ in git), same build command as the steps
    for t, (rel, prop) in CS.items():
        src = os.path.join(tmp, t)
        shutil.copytree(os.path.join(SBDIR, rel), src)
        proj = glob.glob(os.path.join(src, "*.csproj"))[0]
        run([a.dotnet, "build", proj, "-c", "Release", "-o", os.path.join(out, "tools", t),
             "-p:%s=%s" % (prop, a.openkh.replace("\\", "/")), "--nologo", "-v", "q"], cwd=src, env=env)
    src = os.path.join(tmp, "kh2pick")
    shutil.copytree(os.path.join(SBDIR, "cs", "kh2pick"), src)
    run([a.dotnet, "build", os.path.join(src, "kh2pick.csproj"), "-c", "Release", "-o", os.path.join(out, "tools", "kh2pick"),
         "-p:OpenKhApps=" + a.openkh_apps.replace("\\", "/"), "--nologo", "-v", "q"], cwd=src, env=env)
    # every tool must need only Microsoft.NETCore.App (the kit ships nothing else)
    for rc in glob.glob(os.path.join(out, "tools", "*", "*.runtimeconfig.json")):   # (built before installer_host)
        fw = json.load(open(rc))["runtimeOptions"].get("framework", {}).get("name")
        if fw != "Microsoft.NETCore.App":
            raise SystemExit("mkkit: %s needs framework %s (kit ships only Microsoft.NETCore.App)" % (rc, fw))

    # fxrec_boot.exe (clang; the PATH holds the clang dir so its runtime DLLs are found at link time)
    fxd = os.path.join(out, "tools", "fxrec_boot")
    os.makedirs(fxd)
    run(sb.fxrec_cmd(a.clang, os.path.join(fxd, "fxrec_boot.exe")),
        env={"PATH": os.path.dirname(a.clang) + os.pathsep + os.environ.get("PATH", "")})
    # (s168) the installer window: WebView2 in a WinForms form (needs Microsoft.WindowsDesktop.App, shipped below)
    if a.webview2:
        nup = os.path.join(a.webview2, "microsoft.web.webview2.%s.nupkg" % WV2_VER)
        if sha256(nup) != WV2_SHA:
            raise SystemExit("mkkit: %s is not the pinned WebView2 SDK (sha256)" % nup)
        src = os.path.join(tmp, "installer_host")
        shutil.copytree(os.path.join(SBDIR, "..", "installer", "host"), src)
        hd = os.path.join(out, "tools", "installer_host")
        run([a.dotnet, "build", os.path.join(src, "LegendOfKairiSetup.csproj"), "-c", "Release", "-o", hd,
             "-p:RestoreSources=" + a.webview2.replace("\\", "/"), "--nologo", "-v", "q"], cwd=src, env=env)
        for pat in ("*.xml", "*.pdb", "Microsoft.Web.WebView2.Wpf.dll"):
            for f in glob.glob(os.path.join(hd, pat)):
                os.remove(f)
        for arch in ("win-arm64", "win-x86"):
            shutil.rmtree(os.path.join(hd, "runtimes", arch), ignore_errors=True)
    shutil.rmtree(tmp)

    # .NET runtime only (from the SDK): dotnet.exe + host/fxr/<v> + shared/Microsoft.NETCore.App/<v>
    dn = os.path.join(out, "dotnet")
    os.makedirs(dn)
    for f in ("dotnet.exe", "LICENSE.txt", "ThirdPartyNotices.txt"):
        shutil.copy2(os.path.join(sdk, f), os.path.join(dn, f))
    shutil.copytree(os.path.join(sdk, "host"), os.path.join(dn, "host"))
    shutil.copytree(os.path.join(sdk, "shared", "Microsoft.NETCore.App"), os.path.join(dn, "shared", "Microsoft.NETCore.App"))
    if a.webview2:   # (s168) WinForms for the installer window (90 MB, the whole shared framework)
        shutil.copytree(os.path.join(sdk, "shared", "Microsoft.WindowsDesktop.App"), os.path.join(dn, "shared", "Microsoft.WindowsDesktop.App"))

    shutil.copytree(a.openkh, os.path.join(out, "openkh"), ignore=shutil.ignore_patterns("conv*"))
    os.makedirs(os.path.join(out, "busybox"))
    shutil.copy2(a.busybox, os.path.join(out, "busybox", "sh.exe"))
    shutil.copytree(a.python, os.path.join(out, "python"), ignore=shutil.ignore_patterns("__pycache__"))
    # (s168) unpacked wheels: <pkg>.data/platlib/* belongs in site-packages (what pip does); oead's msvcp140-<hash>.dll lives there and
    # without it `import oead` fails on a PC whose PATH has no msvcp140 (found by the bare-PATH kit run)
    site = os.path.join(out, "python", "Lib", "site-packages")
    for plat in glob.glob(os.path.join(site, "*.data", "platlib")):
        for n in os.listdir(plat):
            dst = os.path.join(site, n)
            if os.path.exists(dst):
                raise SystemExit("mkkit: %s from %s already exists" % (n, plat))
            shutil.move(os.path.join(plat, n), dst)
    chk = subprocess.run([os.path.join(out, "python", "python.exe"), "-E", "-P", "-c", "import numpy, PIL.Image, oead"],
                         env={"SYSTEMROOT": os.environ.get("SYSTEMROOT", "C:\\Windows"), "PATH": os.environ.get("SYSTEMROOT", "C:\\Windows")})
    if chk.returncode:
        raise SystemExit("mkkit: the kit python cannot import numpy / Pillow / oead with a bare PATH")
    os.makedirs(os.path.join(out, "ffmpeg"))
    shutil.copy2(a.ffmpeg, os.path.join(out, "ffmpeg", "ffmpeg.exe"))
    shutil.copytree(a.vgmstream, os.path.join(out, "vgmstream"))
    make_licenses(out, a.webview2)

    files = {}
    for d, _, fs in os.walk(out):
        for f in fs:
            p = os.path.join(d, f)
            files[os.path.relpath(p, out).replace("\\", "/")] = sha256(p)
    kj = {"made": time.strftime("%Y-%m-%d %H:%M:%S"), "src": {t: sb.src_hash(t) for t in sb.KIT_SRC},
          "files": dict(sorted(files.items()))}
    json.dump(kj, open(os.path.join(out, "kit.json"), "w", newline="\n"), indent=1)
    mb = sum(os.path.getsize(os.path.join(out, r)) for r in files) / 1e6
    print("mkkit: %s  %d files, %.0f MB" % (out, len(files), mb))


if __name__ == "__main__":
    main()

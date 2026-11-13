"""mkzip: make the friend release folder + zip of "Legend of Kairi" (s177) - dev PC only, friends never run this.

  python -E -P tools/installer/mkzip.py --kit <mkkit output> --out <empty release dir> [--zip <file.zip>]
      [--exe build/savagewind.exe] [--dll build/sora_mod.dll] --src-commit "exe=42a9182 dll=96ec63d"
      [--runtime-dlls <Savage Wind folder with SDL3.dll, avcodec-63.dll ...>] [--no-runtime-dlls] [--version 0.1.0-alpha]
      --track github|melty      (s223, required) github = the public GitHub release: single player (SAVAGEWIND_SORA_SOLO=1 in
                                bin/kairi_env.txt), the DLL built with NO_MELTY=1, and meltycheck refuses anything of Melty's;
                                melty = the Melty package's zip (as before)

Release folder (= what the friend unpacks; the installer's --root defaults to this folder):
  Install Legend of Kairi.cmd     starts the installer with the kit's own python
  kit/                            mkkit output (python, .NET runtime, tools, installer window, LICENSES)
  tools/sorabuild, tools/installer  the git-tracked files of this checkout (no dev config, no mkzip)
  LICENSES/                       copy of kit/LICENSES (the credits page opens this one)
  bin/                            savagewind.exe + sora_mod.dll (+ the runtime DLLs the exe imports, + (s212) the fonts and the
                                  pipeline seed it looks for beside itself, + kairi_env.txt: SAVAGEWIND_SORA=1)
  manifest.json                   release, data build id, bin file sha256s, the source commit of exe + DLL
Nothing from BotW / KH2 / saves / logs goes in: the installer builds the mod data on the friend's PC.
"""
import argparse
import fnmatch
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import meltycheck  # noqa: E402  (s223: nothing of Melty's in the GitHub release)

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
NAME = "Legend of Kairi"
DEFAULT_EXE = "D:/SoraBotW/src/sw-a41/build/savagewind.exe"
DEFAULT_DLL = "D:/SoraBotW/src/sw-a41/build/sora_mod.dll"
# DLLs the exe imports that are not Windows system DLLs (objdump -p, s177). The mod DLL imports only Windows DLLs + savagewind.exe.
# (s187) + what THOSE DLLs import: SDL3.dll imports libiconv-2.dll, which 0.1.0-0.1.4 did not ship (it only worked because the
# launcher's working folder is the stock Savage Wind folder, which has it). main() now follows the imports of every shipped DLL.
RUNTIME_DLLS = ["SDL3.dll", "avcodec-63.dll", "avformat-63.dll", "avutil-61.dll", "swscale-10.dll", "libshaderc_shared.dll",
                "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll", "libiconv-2.dll"]
SYSTEM_DLLS = {"kernel32.dll", "user32.dll", "shell32.dll", "winmm.dll", "ws2_32.dll", "pdh.dll", "vulkan-1.dll",
               "advapi32.dll"}  # (s212: Savage Wind 0.5.3's exe imports ADVAPI32 too, as stock 0.5.3-alpha.5's does)
# (s187) Windows DLLs the RUNTIME DLLs import (never shipped). Checked with objdump -p on the stock 0.4.1-alpha.4 DLLs (s212: stock
# 0.5.3-alpha.5 ships the very same DLL files, sha256 equal).
SYSTEM_DLLS_RT = {"advapi32.dll", "gdi32.dll", "imm32.dll", "ole32.dll", "oleaut32.dll", "setupapi.dll", "version.dll", "bcrypt.dll",
                  "secur32.dll", "crypt32.dll", "ncrypt.dll", "msvcrt.dll", "ucrtbase.dll", "ntdll.dll", "iphlpapi.dll", "dbghelp.dll",
                  "dwmapi.dll", "uxtheme.dll", "shlwapi.dll", "comctl32.dll", "hid.dll", "cfgmgr32.dll", "rpcrt4.dll", "psapi.dll",
                  "userenv.dll", "avrt.dll", "mfplat.dll", "mf.dll", "mfreadwrite.dll", "evr.dll", "propsys.dll", "d3d11.dll",
                  "d3d12.dll", "dxgi.dll", "dxva2.dll", "d3d9.dll", "winhttp.dll", "wldap32.dll", "comdlg32.dll", "powrprof.dll"}
# (s187) LICENSES/bin-runtime-DLLs.txt: what bin/ ships besides our own exe + DLL
BIN_LICENSE_NOTE = """Legend of Kairi - the runtime DLLs in bin/
==========================================

bin/savagewind.exe and bin/sora_mod.dll are built from Savage Wind (MIT, SavageWind-MIT.txt) plus this mod. The other DLLs in
bin/ are the unchanged runtime libraries from the Savage Wind 0.5.3-alpha.5 Windows release:

File(s)                                              Library                      License
---------------------------------------------------  ---------------------------  -------------------------------------------
SDL3.dll                                             SDL 3                        zlib license (https://libsdl.org)
libiconv-2.dll                                       GNU libiconv (used by SDL3)  LGPL-2.1-or-later (LGPL-2.1.txt)
avcodec-63.dll avformat-63.dll avutil-61.dll         FFmpeg libraries             LGPL-2.1-or-later (LGPL-2.1.txt)
  swscale-10.dll
libshaderc_shared.dll                                Shaderc (Google)             Apache-2.0
libgcc_s_seh-1.dll libstdc++-6.dll                   GCC runtime libraries        GPL-3.0 with the GCC Runtime Library Exception
libwinpthread-1.dll                                  mingw-w64 winpthreads        MIT / BSD-3-Clause

Also from that release, unchanged, because savagewind.exe looks for them beside itself:
FiraSans-Medium.ttf FiraSans-SemiBold.ttf            Fira Sans (menus, labels)    SIL Open Font License 1.1 (FiraSans-OFL.txt)
NotoSansThai-Medium.ttf                              Noto Sans Thai               SIL Open Font License 1.1 (NotoSansThai-OFL.txt)
NotoSansHebrew-Medium.ttf                            Noto Sans Hebrew             SIL Open Font License 1.1 (NotoSansHebrew-OFL.txt)
vk_pipelines.seed                                    Savage Wind's pipeline seed  MIT (SavageWind-MIT.txt; hashes and render state,
                                                                                  nothing of the game)
The OFL texts are next to the fonts in bin/ and in LICENSES/. bin/kairi_env.txt is ours (SAVAGEWIND_SORA=1: the mod on, Savage
Wind's update check and installs off, also when bin/savagewind.exe is started by hand).

Source code: https://www.gnu.org/software/libiconv/ , https://ffmpeg.org/download.html , https://github.com/libsdl-org/SDL ,
https://github.com/google/shaderc , https://gcc.gnu.org , https://www.mingw-w64.org . You may replace any of these DLLs with
your own build of the same library version.
"""
# (s212) what savagewind.exe looks for beside itself (runtime/paths.c rt_resource: the exe's folder first) and stock ships there:
# the fonts (menus, button labels, Thai / Hebrew menus) with their licenses, and the pipeline seed (precompile.c seed_find).
# From the --runtime-dlls folder (the stock release), unchanged.
STOCK_RESOURCES = ["FiraSans-Medium.ttf", "FiraSans-SemiBold.ttf", "FiraSans-OFL.txt", "NotoSansThai-Medium.ttf", "NotoSansThai-OFL.txt",
                   "NotoSansHebrew-Medium.ttf", "NotoSansHebrew-OFL.txt", "vk_pipelines.seed"]
# (s212) license texts of stock files that the kit's LICENSES/ lacks (FidelityFX: the FSR 1 code built into savagewind.exe)
STOCK_LICENSES = ["NotoSansThai-OFL.txt", "NotoSansHebrew-OFL.txt", "FidelityFX-LICENSE.txt"]
# (s212, review R1) bin/kairi_env.txt (runtime/kairi_env.h, read first in main): a plain start of bin/savagewind.exe is the mod with
# Savage Wind's updater off (post_update.c update_off_for_mod). The real environment wins, so the launchers can still say otherwise.
KAIRI_ENV = ("# Legend of Kairi: read by savagewind.exe at start (runtime/kairi_env.h). Unset variables only; the environment wins.\n"
             "SAVAGEWIND_SORA=1\n")
# (s223) the GitHub release plays on your own: a plain start of bin/savagewind.exe never turns the mod's co-op on either
KAIRI_ENV_GITHUB = KAIRI_ENV + "SAVAGEWIND_SORA_SOLO=1\n"
# (s212, review R1) Savage Wind's updater files: with sw-manifest.txt beside the exe the stock update check, the Configuration detour
# and SW Update (which would put the STOCK savagewind.exe over ours) come back. Never in a release, anywhere.
FORBIDDEN = {"sw-manifest.txt", "sw update.exe", "savage wind configuration.exe"}
SKIP_TOOLS = {"tools/installer/mkzip.py", "tools/sorabuild/config.thispc.json", "tools/sorabuild/config.fxrec.json",
              # host tests and the dev-only PHH-oracle proof scripts (they name C:/Users/... paths; the build steps never call them)
              "tools/installer/test_gameside.py", "tools/installer/test_recover.py", "tools/installer/test_lock.py",
              "tools/installer/test_update.py", "tools/installer/test_precheck.py", "tools/installer/test_selfupdate.py", "tools/installer/test_uisfx.py",
              "tools/installer/test_shortcut.py", "tools/installer/test_play.py", "tools/installer/test_server.py", "tools/installer/signrel.py",
              "tools/installer/test_swsetup.py", "tools/installer/test_swinstall.py", "tools/installer/meltycheck.py",
              "tools/installer/test_meltycheck.py", "tools/installer/build_github_dll.sh", "tools/sorabuild/kit/rebuild_tools.py",   # (s223)
              "tools/sorabuild/test_pesig.py",   # (s201) host test of pesig.py (reads the dev kit + Melty's signed copy)
              "tools/sorabuild/logo/dev/gen_layers.py", "tools/sorabuild/logo/dev/README.md",
              "tools/sorabuild/ragmap/dev/dump_old.c", "tools/sorabuild/ragmap/dev/dump_new.c", "tools/sorabuild/ragmap/dev/proof.sh",
              # (s182) the old frozen sound recipe (made from PHH's mined tables; step sounds_mine mines it at install now), its retired
              # generator and the dev compare tool
              "tools/sorabuild/sounds/dev/recipe_reference.json", "tools/sorabuild/sounds/dev/gen_recipe.py",
              "tools/sorabuild/sounds/mine/cmp_part.py",
              "tools/sorabuild/fxrec/cmpfile.sh", "tools/sorabuild/fxrec/phf2proof.py", "tools/sorabuild/fxrec/phf2rawcmp.py"}
# (s182) loopend.py + phf2write.py were skipped in s179 but recordall.py RUNS them (fxrec_build crashed at a friend install): they ship
# (s178 S9) must never be in a release. Dirs/files by name anywhere, dirs only at the top level (the kit's python has its own data/ dirs)
BAD_ANY = {"__pycache__", "save", "saves", "extracted", "romfs", ".git", ".installer-ui", ".env"}
BAD_TOP = {"work", "data", "logs", "play", "recon"}
BAD_GLOB = ["*.log", "*.sav", "*.pyc", "*.key", "*.pem", "config.*.json", "live_test.txt", "sora_mod_live*", "Play Legend of Kairi*.bat",
            "*.lnk"]    # (s185) shortcuts are made on the player's PC (they hold absolute paths of the PC they were made on)
KEEP_PEM = ("cacert.pem",)       # the kit's python (certifi) ships its CA list
TEXT_EXT = {".py", ".json", ".txt", ".md", ".cmd", ".bat", ".cs", ".c", ".h", ".js", ".html", ".css", ".csproj", ".ps1", ".sh", ".ini",
            ".cfg", ".xml", ".toml"}
USER_PATH = ("c:/users/", "c:" + chr(92) + "users" + chr(92))


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest()


def tracked(rel):
    out = subprocess.run(["git", "-C", REPO, "ls-files", "-z", rel], capture_output=True, check=True).stdout
    return [p for p in out.decode("utf-8").split("\0") if p]


def imports(exe):
    """the DLL names a PE file imports (objdump -p, from the MSYS2 ucrt64 binutils on PATH)"""
    out = subprocess.run(["objdump", "-p", exe], capture_output=True, text=True, check=True).stdout
    return sorted({ln.split(":", 1)[1].strip() for ln in out.splitlines() if ln.strip().startswith("DLL Name:")})


def forbidden_files(out):
    """(s212, review R1) Savage Wind's updater files anywhere in the release folder (by name, any case) -> their paths"""
    return [os.path.relpath(os.path.join(d, n), out) + "  (Savage Wind's updater: never in a release)"
            for d, dirs, fs in os.walk(out) for n in dirs + fs if n.lower() in FORBIDDEN]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kit", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--zip")
    ap.add_argument("--exe", default=DEFAULT_EXE)
    ap.add_argument("--dll", default=DEFAULT_DLL)
    ap.add_argument("--src-commit", required=True, help='e.g. "exe=42a9182 dll=96ec63d" (recorded in manifest.json)')
    ap.add_argument("--runtime-dlls", default="D:/recompiled games/SavageWind-0.5.3-alpha.5-windows/Savage Wind",
                    help="the stock Savage Wind folder the runtime DLLs, fonts and pipeline seed come from (s212: 0.5.3-alpha.5)")
    ap.add_argument("--no-runtime-dlls", action="store_true", help="do not ship the exe's runtime DLLs (the friend's Savage Wind folder has them)")
    ap.add_argument("--version", default="0.1.0-alpha", help="THE release version: written into manifest.json, which the installer window shows")
    ap.add_argument("--allow-dirty", action="store_true", help="ship tools/ with uncommitted edits (the release is then not reproducible)")
    ap.add_argument("--track", required=True, choices=("github", "melty"),
                    help="(s223) github = the public single-player release (meltycheck must pass); melty = the Melty package's zip")
    a = ap.parse_args()
    # (s178 S7) the shipped tools/ are the working-tree content of tracked files: refuse uncommitted edits, record the commit
    dirty = subprocess.run(["git", "-C", REPO, "status", "--porcelain", "--", "tools/sorabuild", "tools/installer"],
                           capture_output=True, text=True, check=True).stdout.strip()
    if dirty and not a.allow_dirty:
        raise SystemExit("mkzip: tools/sorabuild + tools/installer have uncommitted changes (commit them, or --allow-dirty):\n" + dirty[:600])
    tools_commit = subprocess.run(["git", "-C", REPO, "rev-parse", "HEAD"], capture_output=True, text=True, check=True).stdout.strip()
    out = os.path.abspath(a.out)
    if os.path.exists(out) and os.listdir(out):
        raise SystemExit("mkzip: %s is not empty (use a new dir)" % out)
    for p in (a.exe, a.dll, os.path.join(a.kit, "kit.json")):
        if not os.path.isfile(p):
            raise SystemExit("mkzip: missing %s" % p)
    os.makedirs(out, exist_ok=True)

    # launcher + kit + tools + licenses
    shutil.copy2(os.path.join(HERE, "templates", "Install %s.cmd" % NAME), os.path.join(out, "Install %s.cmd" % NAME))
    # (s184) Update.bat: checks GitHub for a newer release, checks its sha256, runs that release's installer Update on this folder
    shutil.copy2(os.path.join(HERE, "templates", "Update %s.bat" % NAME), os.path.join(out, "Update %s.bat" % NAME))
    shutil.copytree(a.kit, os.path.join(out, "kit"), ignore=shutil.ignore_patterns("__pycache__"))
    n_tools = 0
    for rel in tracked("tools/sorabuild") + tracked("tools/installer"):
        if rel in SKIP_TOOLS:
            continue
        dst = os.path.join(out, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(os.path.join(REPO, rel), dst)
        n_tools += 1
    lic = os.path.join(a.kit, "LICENSES")
    if not os.path.isdir(lic):
        raise SystemExit("mkzip: %s has no LICENSES/ (make the kit with the current mkkit)" % a.kit)
    shutil.copytree(lic, os.path.join(out, "LICENSES"))
    for name in STOCK_LICENSES:  # (s212)
        src = os.path.join(a.runtime_dlls, name)
        if not os.path.isfile(src):
            raise SystemExit("mkzip: %s not found in %s (the stock Savage Wind folder)" % (name, a.runtime_dlls))
        if not os.path.exists(os.path.join(out, "LICENSES", name)):
            shutil.copy2(src, os.path.join(out, "LICENSES", name))
    with open(os.path.join(out, "LICENSES", "bin-runtime-DLLs.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(BIN_LICENSE_NOTE)

    # bin/: exe + mod DLL + the runtime DLLs the exe imports
    bd = os.path.join(out, "bin")
    os.makedirs(bd)
    shutil.copy2(a.exe, os.path.join(bd, "savagewind.exe"))
    shutil.copy2(a.dll, os.path.join(bd, "sora_mod.dll"))
    imp = [d for d in imports(a.exe) if not d.lower().startswith("api-ms-win-") and d.lower() not in SYSTEM_DLLS]
    unknown = sorted(set(imp) - set(RUNTIME_DLLS))
    if unknown:
        raise SystemExit("mkzip: the exe imports DLLs this builder does not know: %s (add them to RUNTIME_DLLS)" % ", ".join(unknown))
    shipped = []
    if not a.no_runtime_dlls:
        # (s187) ship the exe's runtime DLLs AND everything those import in turn (SDL3.dll -> libiconv-2.dll was missing before)
        known = {d.lower(): d for d in RUNTIME_DLLS}
        todo, seen = list(imp), set()
        while todo:
            d = todo.pop(0)
            if d.lower() in seen:
                continue
            seen.add(d.lower())
            src = os.path.join(a.runtime_dlls, d)
            if not os.path.isfile(src):
                raise SystemExit("mkzip: runtime DLL %s not found in %s" % (d, a.runtime_dlls))
            shutil.copy2(src, os.path.join(bd, d))
            shipped.append(d)
            for sub in imports(src):
                low = sub.lower()
                if low.startswith("api-ms-win-") or low in SYSTEM_DLLS or low in SYSTEM_DLLS_RT or low in seen:
                    continue
                if low not in known:
                    raise SystemExit("mkzip: %s imports %s, which this builder does not know (a Windows DLL -> SYSTEM_DLLS_RT, "
                                     "one to ship -> RUNTIME_DLLS)" % (d, sub))
                todo.append(known[low])
        shipped.sort()
    for name in STOCK_RESOURCES:  # (s212) fonts + licenses + pipeline seed beside the exe, as stock has them
        src = os.path.join(a.runtime_dlls, name)
        if not os.path.isfile(src):
            raise SystemExit("mkzip: %s not found in %s (the stock Savage Wind folder)" % (name, a.runtime_dlls))
        shutil.copy2(src, os.path.join(bd, name))
    with open(os.path.join(bd, "kairi_env.txt"), "w", encoding="utf-8", newline="\n") as f:  # (s212, R1)
        f.write(KAIRI_ENV_GITHUB if a.track == "github" else KAIRI_ENV)
    # (the mod DLL's live copies and anything else must never get in)
    binfiles = {f: {"size": os.path.getsize(os.path.join(bd, f)), "sha256": sha256(os.path.join(bd, f))} for f in sorted(os.listdir(bd))}

    dm = json.load(open(os.path.join(REPO, "tools", "sorabuild", "manifest.thispc.json"), encoding="utf-8"))
    man = {"release": NAME, "version": a.version, "track": a.track, "made": time.strftime("%Y-%m-%d %H:%M:%S"),
           "source_commit": a.src_commit, "tools_commit": tools_commit, "tools_dirty": bool(dirty), "net_version_note": "players need the very same release (exe + DLL)",
           "data": {"build_id": dm["build_id"], "files": len(dm["files"]),
                    "manifest": "tools/sorabuild/manifest.thispc.json",
                    "manifest_sha256": sha256(os.path.join(REPO, "tools", "sorabuild", "manifest.thispc.json"))},
           "bin": binfiles, "bin_runtime_dlls_shipped": shipped, "bin_imports_needing_runtime_dlls": imp,
           # (s183) what an Update copies over an older install (installer update.py); written last, so it lists itself too
           "top": sorted(os.listdir(out)) + ["manifest.json"]}
    with open(os.path.join(out, "manifest.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(man, f, indent=1)

    # leftovers check: nothing that must not ship (S9)
    bad = forbidden_files(out)  # (s212, R1) Savage Wind's updater files
    for d, dirs, fs in os.walk(out):
        top = os.path.normcase(d) == os.path.normcase(out)
        for n in dirs + fs:
            low = n.lower()
            hit = low in BAD_ANY or (top and low in BAD_TOP) or any(
                fnmatch.fnmatch(low, g.lower()) for g in BAD_GLOB if not (g == "*.pem" and low in KEEP_PEM))
            if hit:
                bad.append(os.path.relpath(os.path.join(d, n), out))
    # a text file of ours that names a dev-PC user path (kit/ is third-party software and is not scanned)
    for base in [out] + [os.path.join(out, x) for x in ("tools", "LICENSES")]:
        for d, dirs, fs in os.walk(base):
            if base == out:
                dirs[:] = []
            for n in fs:
                p = os.path.join(d, n)
                if os.path.splitext(n)[1].lower() in TEXT_EXT and os.path.getsize(p) < 8 << 20:
                    t = open(p, "rb").read().decode("utf-8", "replace").lower()
                    if any(u in t for u in USER_PATH):
                        bad.append(os.path.relpath(p, out) + "  (names a C:/Users path)")
    if a.track == "github":   # (s223) Melty's connector / setup is under Melty's licence: never in the public release
        bad += ["%s  (Melty's, not for the GitHub release: build the DLL with NO_MELTY=1)" % x for x in meltycheck.scan(out)]
    if bad:
        raise SystemExit("mkzip: files that must not ship: %s" % ", ".join(bad[:10]))

    nfiles, size = 0, 0
    for d, _, fs in os.walk(out):
        for n in fs:
            nfiles += 1
            size += os.path.getsize(os.path.join(d, n))
    print("mkzip: %s  %d files, %.1f MB  (tools %d files, bin %d files)" % (out, nfiles, size / 1e6, n_tools, len(binfiles)))
    if a.zip:
        zp = os.path.abspath(a.zip)
        top = "%s %s" % (NAME, a.version)
        with zipfile.ZipFile(zp, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for d, dirs, fs in os.walk(out):
                dirs.sort()
                for n in sorted(fs):
                    p = os.path.join(d, n)
                    z.write(p, top + "/" + os.path.relpath(p, out).replace("\\", "/"))
        print("mkzip: %s  %.1f MB" % (zp, os.path.getsize(zp) / 1e6))


if __name__ == "__main__":
    sys.exit(main())

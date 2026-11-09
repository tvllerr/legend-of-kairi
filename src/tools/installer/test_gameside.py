"""Host test for the launchers (s178 M1): run both .bat files under real cmd.exe from install roots / Savage Wind folders whose names hold
cmd specials, with a fake game exe (the text "savagewind.exe" is swapped for "fakegame.exe" in the TEST copy only, so a real running
Savage Wind can neither be started nor blocks the already-running check).

  python tools/installer/test_gameside.py [--keep]     (needs clang on PATH for the fake exe; MSYS2 ucrt64 bin)
"""
import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gameside  # noqa: E402

C_SRC = r'''#include <stdio.h>
#include <windows.h>
int main(int c,char**v){char cwd[1024];GetCurrentDirectoryA(1024,cwd);
FILE*f=fopen("fake_marker.txt","w");if(f){fprintf(f,"arg1=%s cwd=%s\n",c>1?v[1]:"",cwd);fclose(f);}return 0;}
'''
NAMES = ["plain", "with space", "A&B (1)", "x ^ y", "p(a)r", "bang!", "pct 50% done %nosuchvar%", "all & ( ) ! ^ % ' , ; = x", "a&echo pwned&b"]


def build_fake(d):
    src = os.path.join(d, "fake.c")
    with open(src, "w") as f:
        f.write(C_SRC)
    exe = os.path.join(d, "fakegame.exe")
    subprocess.run(["clang", "-O1", src, "-o", exe], check=True)
    return exe


def make_data(root, skip=None):
    """the key body files under <root>/data (all but <skip>)"""
    for k in gameside.KEY_FILES:
        if k == skip:
            continue
        p = os.path.join(root, "data", *k.split("/"))
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "wb") as f:
            f.write(b"x")


def run_one(base, name, multi, fake, missing=None, manifest_id=None, build_id=None, expect="start"):
    """expect: "start" (the fake exe ran) | "incomplete" | "otherversion" (message shown, exe NOT started)"""
    root = os.path.join(base, "root " + name)
    sw = os.path.join(base, "sw " + name)
    os.makedirs(os.path.join(root, "bin"))
    os.makedirs(sw)
    open(os.path.join(sw, "savagewind.ini"), "w").close()
    shutil.copy2(fake, os.path.join(root, "bin", "fakegame.exe"))
    make_data(root, missing)
    if manifest_id is not None:     # "" = a manifest that has no build id at all
        bl = '  "build_id": "%s",\n' % manifest_id if manifest_id else ""
        with open(os.path.join(root, "manifest.json"), "w") as f:
            f.write('{\n "release": "Legend of Kairi",\n "made": "2026-11-08 14:52:22",\n "data": {\n%s  "files": 143\n }\n}\n' % bl)
    if build_id is not None:
        with open(os.path.join(root, "data", "BUILD_ID"), "wb") as f:
            f.write(build_id.encode() + b"\r\n")
    text = gameside.launcher_text(multi, sw, "1.2.3.4:7790").replace("savagewind.exe", "fakegame.exe")
    bat = os.path.join(root, "t.bat")
    with open(bat, "wb") as f:
        f.write(text.encode("utf-8"))
    p = subprocess.run('cmd /c ""%s""' % bat, shell=False, capture_output=True, timeout=30, stdin=subprocess.DEVNULL)
    marker = os.path.join(sw, "fake_marker.txt")
    for _ in range(100):
        if os.path.isfile(marker):
            break
        time.sleep(0.1)
    out = p.stdout.decode("mbcs", "replace") + p.stderr.decode("mbcs", "replace")
    if expect != "start":
        want = {"incomplete": ["files are incomplete", "(%s is missing)" % (missing or "").replace("/", "\\"), "Install Legend of Kairi.cmd again"],
                "otherversion": ["files are from another version", "choose Update"]}[expect]
        return (not os.path.isfile(marker)) and p.returncode == 1 and all(w in out for w in want), out
    ok = os.path.isfile(marker) and "arg1=extracted" in open(marker).read() and os.path.normcase(sw) in os.path.normcase(open(marker).read())
    return ok and "incomplete" not in out and "another version" not in out, out


def test_peers():
    """(S3) the friend's PEERS survives a rerun however the line was edited; the .bat is replaced via a temp file"""
    d = tempfile.mkdtemp(prefix="loktest_", dir=os.environ.get("LOK_TEST_TMP"))
    try:
        sw = os.path.join(d, "sw")
        os.makedirs(sw)
        cases = [('set "PEERS=100.1.2.3:7790"', "100.1.2.3:7790"), ("set PEERS=100.1.2.3:7790", "100.1.2.3:7790"),
                 ("  SET   PEERS = 100.1.2.3:7790  ", "100.1.2.3:7790"), ("set PEERS=100.1.2.3:7790 & rem bob", "100.1.2.3:7790"),
                 ('set "PEERS=1.1.1.1:7790,2.2.2.2:7790"', "1.1.1.1:7790,2.2.2.2:7790"), ('set "PEERS="', gameside.PLACEHOLDER)]
        for line, want in cases:
            p = os.path.join(d, "m.bat")
            t = gameside.launcher_text(True, sw, "OLD:1").replace('set "PEERS=OLD:1"', line)
            assert line in t
            with open(p, "wb") as f:
                f.write(t.encode())
            got = gameside._kept_peers(p)
            assert got == want, (line, got)
            gameside._write_bat(p, gameside.launcher_text(True, sw, got))
            assert gameside._kept_peers(p) == want and not os.path.exists(p + ".tmp")
        assert gameside._kept_peers(os.path.join(d, "nofile.bat")) == gameside.PLACEHOLDER
        print("OK   peers kept (%d forms)" % len(cases))
    finally:
        shutil.rmtree(d, ignore_errors=True)


def run_bugreport(base, name, short=False):
    """(s183) 'Make bug report.bat' under real cmd.exe from an install root / Savage Wind folder whose names hold cmd specials:
    one zip with the logs, install logs, manifest, launchers, savagewind.ini, data_listing and a systeminfo.txt; no saves, no game files."""
    import zipfile
    root = os.path.join(base, "root " + name)
    sw = os.path.join(base, "sw " + name)
    dest = os.path.join(base, "dest")
    for d in (os.path.join(root, "logs"), os.path.join(root, "work", "logs"), os.path.join(root, "data"), os.path.join(sw, "save", "save_0"),
              os.path.join(sw, "extracted"), dest):
        os.makedirs(d)
    files = {os.path.join(root, "logs", "last.log"): "LAST LOG", os.path.join(root, "logs", "prev.log"): "PREV LOG",
             os.path.join(root, "work", "logs", "install.log"): "INSTALL LOG", os.path.join(root, "manifest.json"): '{"version": "9"}',
             os.path.join(sw, "savagewind.ini"): "[x]\nfoo=1\n", os.path.join(sw, "save", "save_0", "caption.sav"): "SAVE",
             os.path.join(sw, "extracted", "game.bin"): "GAMEFILE", os.path.join(root, "data", "sora_model.bin"): "DATA"}
    for p, t in files.items():
        with open(p, "w") as f:
            f.write(t)
    with open(os.path.join(root, gameside.SOLO), "wb") as f:
        f.write(gameside.launcher_text(False, sw).encode("utf-8"))
    bat = os.path.join(root, gameside.BUGREPORT)
    gameside._write_bat(bat, gameside.bugreport_text(sw))
    gameside._write_bat(os.path.join(root, gameside.BUGREPORT_PS1), gameside.bugreport_ps_text())
    gameside._write_bat(os.path.join(root, gameside.MULTI), gameside.launcher_text(True, sw, "100.64.1.2:7790,100.64.3.4:7790"))
    if short:  # run it through the 8.3 short path (%~dp0 is then short): data_listing must still cut the right names
        q = subprocess.run('cmd /c for %I in ("' + bat + '") do @echo %~sI', capture_output=True, timeout=30, stdin=subprocess.DEVNULL)
        sb = q.stdout.decode("mbcs", "replace").strip()
        if not sb or "~" not in sb or not os.path.isfile(sb):
            return True, "no 8.3 short names on this volume: skipped"
        bat = sb
    env = dict(os.environ, KAIRI_BUGREPORT_DIR=dest)
    p = subprocess.run('cmd /c ""%s""' % bat, shell=False, capture_output=True, timeout=180, stdin=subprocess.DEVNULL, env=env)
    out = p.stdout.decode("mbcs", "replace") + p.stderr.decode("mbcs", "replace")
    zs = [f for f in os.listdir(dest) if f.startswith("Legend of Kairi bug report ") and f.endswith(".zip")]
    if len(zs) != 1:
        return False, "no single zip in dest: %s\n%s" % (zs, out)
    z = zipfile.ZipFile(os.path.join(dest, zs[0]))
    names = [n.replace("\\", "/") for n in z.namelist()]
    blob = {n.replace("\\", "/"): z.read(n) for n in z.namelist() if not n.endswith("/")}
    need = ["logs/last.log", "logs/prev.log", "work_logs/install.log", "install_folder/manifest.json", "launchers/" + gameside.SOLO,
            "launchers/" + gameside.BUGREPORT, "savagewind_folder/savagewind.ini", "systeminfo.txt", "data_listing.txt"]
    missing = [n for n in need if n not in names]
    bad = [n for n in names if "caption.sav" in n or "game.bin" in n or "save_0" in n]
    si = blob.get("systeminfo.txt", b"").decode("utf-8", "replace")
    ok = not missing and not bad and "GPU:" in si and "RAM:" in si and "Windows:" in si and blob.get("logs/last.log", b"").strip() == b"LAST LOG"
    ok = ok and "sora_model.bin" in blob.get("data_listing.txt", b"").decode("utf-8", "replace") and "Done." in out
    # the data listing names are relative to the install root ("data\\sora_model.bin"), also from a short path
    ok = ok and any(l.startswith("data\\sora_model.bin") for l in blob.get("data_listing.txt", b"").decode("utf-8-sig", "replace").splitlines())
    # the zip's copy of the multiplayer launcher has no IPs; the real one in the install root keeps them
    mp = blob.get("launchers/" + gameside.MULTI, b"").decode("utf-8", "replace")
    ok = ok and "<removed>" in mp and "100.64" not in mp.replace("100.101.102.103", "")
    ok = ok and "100.64.1.2" in open(os.path.join(root, gameside.MULTI), encoding="utf-8").read()
    ok = ok and "launchers/" + gameside.BUGREPORT_PS1 in names
    return ok, "missing=%s bad=%s\n%s\n--- systeminfo ---\n%s" % (missing, bad, out, si)


def test_bugreport_text():
    """(s183) the .bat is CRLF, ascii for an ascii path, utf-8 + chcp for another, % doubled, the script marker is a PowerShell comment"""
    t = gameside.bugreport_text(r"C:\Games\a%b&c\Savage Wind")
    assert "\r\n" in t and "\n" not in t.replace("\r\n", ""), "CRLF"
    assert r'set "GAME=C:\Games\a%%b&c\Savage Wind"' in t, [l for l in t.split("\r\n") if "GAME" in l]
    assert "chcp" not in t and "Invoke-Expression" not in t and "-Command" not in t and "ReadAllText" not in t
    assert '-File "%~dp0make_bug_report.ps1"' in t and "exit /b 0" in t and "pause" in t
    ps = gameside.bugreport_ps_text()
    assert ps.isascii() and "\r\n" in ps and "Invoke-Expression" not in ps and "$env:SORA_ROOT" in ps and "<removed>" in ps
    u = gameside.bugreport_text("C:\\Spiele\\M\u00fcnchen")
    assert "chcp 65001" in u
    print("OK   bug report text")


def main():
    test_peers()
    test_bugreport_text()
    base = tempfile.mkdtemp(prefix="loktest_", dir=os.environ.get("LOK_TEST_TMP"))
    fail = 0
    try:
        fake = build_fake(base)
        for name in NAMES:
            for multi in (False, True):
                ok, out = run_one(os.path.join(base, "n%d%d" % (NAMES.index(name), multi)), name, multi, fake)
                print("%-4s %-30s %s" % ("OK" if ok else "FAIL", name, "multi" if multi else "solo"))
                if not ok:
                    fail += 1
                    print(out)
        pre = [("data complete, no manifest (check skipped)", dict(), "start"),
               ("data complete, BUILD_ID matches", dict(manifest_id="f9088e2d0399c69c", build_id="f9088e2d0399c69c"), "start"),
               ("BUILD_ID differs", dict(manifest_id="f9088e2d0399c69c", build_id="0123456789abcdef"), "otherversion"),
               ("BUILD_ID file missing", dict(manifest_id="f9088e2d0399c69c"), "otherversion"),
               ("manifest without build_id (check skipped)", dict(manifest_id=""), "start")]
        pre += [("missing " + k, dict(missing=k), "incomplete") for k in gameside.KEY_FILES]
        for name in ("all & ( ) ! ^ % ' , ; = x", "with space"):
            for multi in (False, True):
                for i, (label, kw, exp) in enumerate(pre):
                    ok, out = run_one(os.path.join(base, "q%d%d%d" % (NAMES.index(name) if name in NAMES else 99, multi, i)), name, multi, fake,
                                      expect=exp, **kw)
                    print("%-4s %-30s %-6s %s" % ("OK" if ok else "FAIL", name[:30], "multi" if multi else "solo", label))
                    if not ok:
                        fail += 1
                        print(out)
        for name in NAMES:
            for short in (False, True):
                ok, out = run_bugreport(os.path.join(base, "b%d%d" % (NAMES.index(name), short)), name, short)
                print("%-4s %-30s %s" % ("OK" if ok else "FAIL", name, "bug report" + (" (8.3 path)" if short else "")))
                if not ok:
                    fail += 1
                    print(out)
    finally:
        if "--keep" not in sys.argv:
            shutil.rmtree(base, ignore_errors=True)
    print("failures:", fail)
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())

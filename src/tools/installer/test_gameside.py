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
const char*e1=getenv("SAVAGEWIND_SORA_SOLO"),*e2=getenv("SAVAGEWIND_NO_UPDATE_CHECK"),*e3=getenv("SAVAGEWIND_VK_VDYN");
FILE*f=fopen("fake_marker.txt","w");if(f){fprintf(f,"arg1=%s arg2=%s solo=%s noupd=%s vdyn=%s cwd=%s\n",c>1?v[1]:"",c>2?v[2]:"",
e1?e1:"-",e2?e2:"-",e3?e3:"-",cwd);fclose(f);}return 0;}
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


def run_one(base, name, multi, fake, missing=None, manifest_id=None, build_id=None, expect="start", swv=gameside.SW_WANT,
            gid=None, have_gid=None):
    """expect: "start" (the fake exe ran) | "incomplete" | "otherversion" | "swversion" | "gameid" (message shown, exe NOT started).
    (s223) swv: the version line of the Savage Wind folder's sw-manifest.txt (None = no file); gid: data/GAME_ID (None = no file);
    have_gid: <sw>/extracted/swgame.dll.id (LF, as stock writes it; None = no file)"""
    root = os.path.join(base, "root " + name)
    sw = os.path.join(base, "sw " + name)
    os.makedirs(os.path.join(root, "bin"))
    os.makedirs(sw)
    open(os.path.join(sw, "savagewind.ini"), "w").close()
    if swv is not None:
        with open(os.path.join(sw, "sw-manifest.txt"), "wb") as f:
            f.write(b"savagewind-update-manifest 1\nversion " + swv.encode() + b"\nf 00 1 savagewind.exe\n")
    if have_gid is not None:
        os.makedirs(os.path.join(sw, "extracted"))
        with open(os.path.join(sw, "extracted", "swgame.dll.id"), "wb") as f:
            f.write(have_gid.encode() + b"\n")
    shutil.copy2(fake, os.path.join(root, "bin", "fakegame.exe"))
    make_data(root, missing)
    if gid is not None:
        with open(os.path.join(root, "data", "GAME_ID"), "wb") as f:
            f.write(gid.encode() + b"\r\n")
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
                "otherversion": ["files are from another version", "choose Update"],
                "swversion": ["has Savage Wind %s" % (swv or "an unknown version"), "made for Savage Wind %s" % gameside.SW_WANT],
                "gameid": ["game code in your Savage Wind folder is not the one", "Install Legend of Kairi.cmd again"]}[expect]
        return (not os.path.isfile(marker)) and p.returncode == 1 and all(w in out for w in want), out
    m = open(marker).read() if os.path.isfile(marker) else ""
    ok = "arg1=extracted arg2=--no-launch-flags " in m and os.path.normcase(sw) in os.path.normcase(m)
    ok = ok and "noupd=1 vdyn=0" in m and (("solo=1" in m) if not multi else ("solo=-" in m))   # (s223)
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
    assert "System32" in u and "chcp.com" in u and " 65001 " in u
    print("OK   bug report text")


def test_scrub():
    """(s185 sec) the bug report holds no IPs, Windows user names, PC name or player names; versions / timestamps / loopback stay; a file that
    is not plain text is left out (scrub_notes.txt says so)"""
    import zipfile
    base = tempfile.mkdtemp(prefix="loktest_", dir=os.environ.get("LOK_TEST_TMP"))
    try:
        root, sw, dest = (os.path.join(base, n) for n in ("root", "sw", "dest"))
        for d in (os.path.join(root, "logs"), os.path.join(root, "work", "logs"), sw, dest):
            os.makedirs(d, exist_ok=True)
        log = ("12:34:56 Legend of Kairi 0.4.1 build 1.9.0 data 10.0.26200.1.5\n"
               "[net] peer pinned to 100.64.12.200:7790\n[net] peer 2 joined: Zorbax Friend 100.101.102.103:7790\n"
               "[net] peer 3 joined: Bob\nlocal 127.0.0.1:7790 any 0.0.0.0 bad 999.1.2.3\n"
               "v6 fd7a:115c:a1e0::1234:5678 and 2001:db8:0:0:0:0:0:1 and fe80::1%12 ts 2026-11-09 01:02:03:456 Foo::bar\n"
               "path C:\\Users\\Zorbax\\AppData\\Local\\x and C:/Users/Other Person/y and D:\\Users\\Jane Doe\\z; host ZORBOX-PC ok; ab zorbax. zorbaxx\n")
        texts = {os.path.join(root, "logs", "last.log"): log,
                 os.path.join(root, "work", "logs", "install.log"): "C:\\Users\\Zorbax\\kit\n",
                 os.path.join(root, "manifest.json"): '{"version": "0.4.1"}', os.path.join(sw, "savagewind.ini"): "[x]\n"}
        for p, t in texts.items():
            with open(p, "w", newline="") as f:
                f.write(t)
        with open(os.path.join(root, "logs", "bin.log"), "wb") as f:      # UTF-16 (BOM + NULs): not plain text, left out
            f.write("peer 100.1.2.3 Zorbax\r\n".encode("utf-16"))
        with open(os.path.join(root, "logs", "prev.log"), "wb") as f:     # (s185 review) stray NULs (a crash mid-write): kept, scrubbed
            f.write(b"12:34:56 crash at 0x00007FF6A1B2C3D4 peer 100.9.8.7:7790 C:\\Users\\Zorbax\\x\n" + b"\x00" * 512 + b"tail line\n")
        ps1 = os.path.join(root, gameside.BUGREPORT_PS1)
        gameside._write_bat(ps1, gameside.bugreport_ps_text())
        env = dict(os.environ, KAIRI_BUGREPORT_DIR=dest, SORA_ROOT=root + "\\", GAME=sw, USERNAME="Zorbax", COMPUTERNAME="ZORBOX-PC")
        pse = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "WindowsPowerShell", "v1.0", "powershell.exe")
        p = subprocess.run([pse, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ps1],
                           capture_output=True, timeout=180, stdin=subprocess.DEVNULL, env=env)
        out = p.stdout.decode("mbcs", "replace") + p.stderr.decode("mbcs", "replace")
        zs = [f for f in os.listdir(dest) if f.endswith(".zip")]
        assert len(zs) == 1, out
        z = zipfile.ZipFile(os.path.join(dest, zs[0]))
        blob = {n.replace("\\", "/"): z.read(n).decode("utf-8", "replace") for n in z.namelist() if not n.endswith("/")}
        allt = "\n".join(blob.values()).lower()
        for bad in ("100.64.12.200", "100.101.102.103", "zorbax ", "zorbax\\", "zorbax.", "zorbox", "other person", "jane","fd7a:115c", "2001:db8", "fe80::1", "zorbax friend", "joined: bob"):
            assert bad not in allt, (bad, blob)
        lg = blob["logs/last.log"]
        for keep in ("0.4.1", "build 1.9.0", "10.0.26200.1.5", "12:34:56", "127.0.0.1", "0.0.0.0", "999.1.2.3", "2026-11-09 01:02:03:456", "Foo::bar", "zorbaxx"):
            assert keep in lg, (keep, lg)
        assert "peer pinned to <ip>:7790" in lg and "joined: <player> <ip>:7790" in lg and "joined: <player>\n" in lg, lg
        assert "\\Users\\<user>\\AppData" in lg and "/Users/<user>/y" in lg and "host <pc> ok" in lg and "ab <user>." in lg, lg
        assert "logs/bin.log" not in blob and "bin.log" in blob.get("scrub_notes.txt", ""), list(blob)
        pv = blob.get("logs/prev.log", "")
        assert pv == "12:34:56 crash at 0x00007FF6A1B2C3D4 peer <ip>:7790 C:\\Users\\<user>\\x\ntail line\n", repr(pv)
        assert "NUL bytes removed: logs\\prev.log" in blob["scrub_notes.txt"], blob["scrub_notes.txt"]
        assert "0.4.1" in blob["install_folder/manifest.json"]
        assert "Users\\<user>\\kit" in blob["work_logs/install.log"]
        print("OK   bug report scrub")
    finally:
        shutil.rmtree(base, ignore_errors=True)


def test_no_bare_tools():
    """(s185 sec) the generated .bat files start cmd / powershell / chcp / tasklist by full System32 path only"""
    import re as _re
    sw = r"C:\Games\Savage Wind"
    pat = _re.compile(r"(?i)(^|[\s\"=(])(cmd|powershell|chcp|tasklist|findstr|systeminfo|ipconfig|where|timeout|reg|icacls|robocopy|tar)(\.exe|\.com)?(\s|$|\")")
    for t in (gameside.launcher_text(False, sw), gameside.launcher_text(True, sw), gameside.bugreport_text(sw), gameside.bugreport_text("C:\\M\u00fcnchen")):
        for l in t.split("\r\n"):
            if l.lstrip().lower().startswith(("rem ", "echo ")):
                continue
            assert not pat.search(l), l
    assert '/min "%SystemRoot%\\System32\\cmd.exe" /c' in gameside.launcher_text(False, sw)
    for f in os.listdir(os.path.join(HERE, "templates")):
        t = open(os.path.join(HERE, "templates", f), encoding="utf-8").read()
        assert not _re.search(r"(?im)^\s*(cmd|powershell|chcp|tasklist|findstr|timeout)\b", t), f
    print("OK   no bare tool names in the launchers")


def main():
    test_peers()
    test_bugreport_text()
    test_no_bare_tools()
    test_scrub()
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
               ("manifest without build_id (check skipped)", dict(manifest_id=""), "start"),
               # (s223) Savage Wind's version + its game code id
               ("Savage Wind moved on to 0.5.4", dict(swv="0.5.4-alpha.1"), "swversion"),
               ("Savage Wind 0.4.1 (no sw-manifest.txt)", dict(swv=None), "swversion"),
               ("game id matches", dict(gid="7689ce31c1953be6", have_gid="7689ce31c1953be6"), "start"),
               ("game id differs", dict(gid="7689ce31c1953be6", have_gid="1111111111111111"), "gameid"),
               ("game id file gone from Savage Wind", dict(gid="7689ce31c1953be6"), "gameid"),
               ("no data/GAME_ID (check skipped)", dict(have_gid="1111111111111111"), "start")]
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

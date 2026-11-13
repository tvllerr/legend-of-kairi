"""Tests for the shortcuts (s185): the two .lnk files next to the launchers and the done screen's /api/shortcut (desktop shortcut).
Run: python tools/installer/test_shortcut.py      (Windows; nothing is written to the real Desktop, the game is never started)
"""
import http.client
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import types

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gameside  # noqa: E402
import installer  # noqa: E402

PS = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "WindowsPowerShell", "v1.0", "powershell.exe")
NAMES = ["with space & amp", "all & ( ) ! ^ % ' , ; = x", "br[ack]et"]
DUMMY_BAT = '@echo off\r\necho ran> "%~dp0lnk_marker.txt"\r\n'     # stands in for a launcher: only writes a marker next to itself


def read_lnk(p):
    """target, working dir, icon, window style of a .lnk, read back through WScript.Shell (path via the environment)"""
    r = subprocess.run([PS, "-NoProfile", "-NonInteractive", "-Command",
                        "$s = (New-Object -ComObject WScript.Shell).CreateShortcut($env:LNK); "
                        "[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false); "
                        "@($s.TargetPath, $s.WorkingDirectory, $s.IconLocation, [string]$s.WindowStyle) -join \"`n\""],
                       env=dict(os.environ, LNK=p), capture_output=True, timeout=60, stdin=subprocess.DEVNULL)
    parts = r.stdout.decode("utf-8", "replace").strip().split("\n")
    assert r.returncode == 0 and len(parts) == 4, (p, r.stdout, r.stderr)
    return [x.strip() for x in parts]


def fake_install(root):
    os.makedirs(os.path.join(root, *gameside.ICON_REL[:-1]), exist_ok=True)
    shutil.copy2(os.path.join(HERE, "host", "kairi.ico"), os.path.join(root, *gameside.ICON_REL))
    for bat in (gameside.SOLO, gameside.MULTI):
        with open(os.path.join(root, bat), "w", newline="") as f:
            f.write(DUMMY_BAT)


def same(a, b):
    return os.path.normcase(os.path.normpath(a)) == os.path.normcase(os.path.normpath(b))


def test_install_folder(base):
    """both shortcuts: right target / working dir / our icon / normal window, for folder names with spaces and cmd specials;
    the solo shortcut really starts its .bat"""
    for name in NAMES:
        root = os.path.join(base, "root " + name)
        fake_install(root)
        logs = []
        made = gameside.install_shortcuts(root, logs.append)
        assert len(made) == len(gameside.SHORTCUTS), (name, logs)
        for (lnk, bat, _), p in zip(gameside.SHORTCUTS, made):
            assert same(p, os.path.join(root, lnk)) and os.path.isfile(p), (p, lnk)
            tgt, wd, icon, style = read_lnk(p)
            assert same(tgt, os.path.join(root, bat)), (tgt, bat)
            assert same(wd, root), wd
            assert icon.rsplit(",", 1)[1].strip() == "0" and same(icon.rsplit(",", 1)[0], os.path.join(root, *gameside.ICON_REL)), icon
            assert style == "1", style
        os.startfile(made[0])  # noqa: S606 - the dummy .bat above (writes a marker), never the game
        mk = os.path.join(root, "lnk_marker.txt")
        for _ in range(100):
            if os.path.isfile(mk):
                break
            time.sleep(0.1)
        assert os.path.isfile(mk), "the shortcut did not start its .bat (%s)" % name
        # made again (an update / a second install): replaced in place
        assert len(gameside.install_shortcuts(root, logs.append)) == len(gameside.SHORTCUTS)
        print("OK   install-folder shortcuts: %s" % name)


def fake_release(base):
    rel = os.path.join(base, "release & co")
    os.makedirs(os.path.join(rel, "bin"))
    for n in gameside.NEEDED:
        with open(os.path.join(rel, "bin", n), "wb") as f:
            f.write(b"x")
    os.makedirs(os.path.join(rel, *gameside.ICON_REL[:-1]))
    shutil.copy2(os.path.join(HERE, "host", "kairi.ico"), os.path.join(rel, *gameside.ICON_REL))
    sw = os.path.join(base, "sw")
    os.makedirs(sw)
    return rel, sw


def test_failure_never_fails_install(base):
    """install_game_side: shortcuts made normally; a PowerShell that is missing or a script that fails only logs one line"""
    rel, sw = fake_release(base)
    logs = []
    r = gameside.install_game_side(rel, sw, logs.append, release=rel)
    assert len(r["shortcuts"]) == len(gameside.SHORTCUTS) and all(os.path.isfile(p) for p in r["shortcuts"]), logs
    for lnk, _, _ in gameside.SHORTCUTS:
        os.remove(os.path.join(rel, lnk))
    for attr, bad in (("POWERSHELL", os.path.join(base, "no", "powershell.exe")), ("SHORTCUT_PS1", os.path.join(base, "no", "x.ps1"))):
        old = getattr(gameside, attr)
        setattr(gameside, attr, bad)
        try:
            logs = []
            r = gameside.install_game_side(rel, sw, logs.append, release=rel)
        finally:
            setattr(gameside, attr, old)
        assert r["shortcuts"] == [] and os.path.isfile(r["solo"]) and "multi" not in r, (attr, r)
        miss = [l for l in logs if "could not make the shortcut" in l]
        assert len(miss) == len(gameside.SHORTCUTS) and all(len(l.splitlines()) == 1 for l in miss), (attr, logs)
        assert not any(os.path.exists(os.path.join(rel, lnk)) for lnk, _, _ in gameside.SHORTCUTS)
    print("OK   a failing shortcut step does not fail install_game_side")


def post(port, path, token, body=b"{}"):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=60)
    c.request("POST", path, body, {"X-Token": token, "Content-Type": "application/json", "Content-Length": str(len(body))})
    r = c.getresponse()
    return r.status, json.loads(r.read() or b"{}")


def real_desktop_lnk():
    r = subprocess.run([PS, "-NoProfile", "-NonInteractive", "-Command", "[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false); "
                        "[Environment]::GetFolderPath('Desktop')"], capture_output=True, timeout=60, stdin=subprocess.DEVNULL)
    d = r.stdout.decode("utf-8", "replace").strip()
    p = os.path.join(d, gameside.DESKTOP_LNK) if d else ""
    return p, (os.path.getmtime(p) if p and os.path.exists(p) else None)


def test_api(base):
    """/api/shortcut: token checked, only when done, the request body changes nothing, the .lnk lands in the (test) desktop folder"""
    dest = os.path.join(base, "inst & x")
    fake_install(dest)
    desk = os.path.join(base, "desk top")
    os.makedirs(desk)
    log = []
    st = types.SimpleNamespace(status="running", root=dest, kit=dest, sfx=False, desktop_dir=desk, add_log=log.append)
    st.dest = lambda: dest
    real, real_m = real_desktop_lnk()
    tok = "tok-185"
    srv = installer.Server(("127.0.0.1", 0), installer.make_handler(st, tok))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    port = srv.server_address[1]
    try:
        assert post(port, "/api/shortcut", "wrong")[0] == 403
        code, r = post(port, "/api/shortcut", tok)
        assert code == 200 and "not finished" in r.get("error", ""), r
        assert not os.listdir(desk)
        st.status = "done"
        other = os.path.join(base, "elsewhere")
        os.makedirs(other)
        body = json.dumps({"path": other, "folder": other, "desktop": other, "target": "C:/Windows/notepad.exe", "name": "x.lnk"}).encode()
        code, r = post(port, "/api/shortcut", tok, body)
        assert code == 200 and r == {"ok": True}, (code, r)
        p = os.path.join(desk, gameside.DESKTOP_LNK)
        assert os.listdir(desk) == [gameside.DESKTOP_LNK] and not os.listdir(other), (os.listdir(desk), os.listdir(other))
        tgt, wd, icon, style = read_lnk(p)
        assert same(tgt, os.path.join(dest, gameside.SOLO)) and same(wd, dest) and "kairi.ico" in icon and style == "1", (tgt, wd, icon)
        code, r = post(port, "/api/shortcut", tok)       # pressed again (a new page): replaced, still ok
        assert r == {"ok": True}, r
        os.remove(os.path.join(dest, gameside.SOLO))      # no launcher: plain-words error
        code, r = post(port, "/api/shortcut", tok)
        assert "was not found" in r.get("error", ""), r
        st.desktop_dir = os.path.join(base, "no such desktop")   # a desktop folder that is gone: plain-words error, nothing raised
        open(os.path.join(dest, gameside.SOLO), "w").close()
        code, r = post(port, "/api/shortcut", tok)
        assert code == 200 and "Could not add the shortcut to your desktop" in r.get("error", ""), r
    finally:
        srv.shutdown()
    assert real_desktop_lnk() == (real, real_m), "the real Desktop was touched"
    print("OK   /api/shortcut (test desktop only; real Desktop untouched)")


def main():
    if sys.platform != "win32":
        print("test_shortcut: Windows only, skipped")
        return 0
    base = tempfile.mkdtemp(prefix="loktest_lnk_", dir=os.environ.get("LOK_TEST_TMP"))
    try:
        test_install_folder(base)
        test_failure_never_fails_install(base)
        test_api(base)
    finally:
        if "--keep" not in sys.argv:
            shutil.rmtree(base, ignore_errors=True)
    print("test_shortcut: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())

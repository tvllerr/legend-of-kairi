"""Tests for the installer's /api/play route (s185). Run: python tools/installer/test_play.py
Uses a fake state object and a dummy launcher (a .bat that only writes a marker file): the real game is never started."""
import http.client
import json
import os
import sys
import tempfile
import threading
import time
import types

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gameside  # noqa: E402
import installer  # noqa: E402


def fake_state(dest, status):
    st = types.SimpleNamespace(status=status, root=dest, kit=dest, sfx=False)
    st.dest = lambda: dest
    return st


def post(port, path, token, body=b"{}"):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    c.request("POST", path, body, {"X-Token": token, "Content-Type": "application/json", "Content-Length": str(len(body))})
    r = c.getresponse()
    return r.status, json.loads(r.read() or b"{}")


def main():
    with tempfile.TemporaryDirectory() as d:
        # 1) pure function: not done / no launcher / the Popen call is exactly what the spec says
        assert "not finished" in installer.play_solo(fake_state(d, "running"))["error"]
        e = installer.play_solo(fake_state(d, "done"))["error"]
        assert gameside.SOLO in e and d in e, e
        bat = os.path.join(d, gameside.SOLO)
        open(bat, "w").write("@echo off\r\necho hi> \"%~dp0marker.txt\"\r\n")
        seen = {}

        def fake_popen(args, **kw):
            seen["args"], seen["kw"] = args, kw
        assert installer.play_solo(fake_state(d, "done"), popen=fake_popen) == {}
        assert seen["args"][1:] == ["/c", ".\\" + gameside.SOLO] and seen["args"][0].lower().endswith("cmd.exe"), seen
        if sys.platform == "win32":     # (s185 review) really runs from a folder with cmd specials in its name (& ^ ( ) % !)
            for nm in ("a & b", "caret^ x", "Games (SSD) x", "pct 100%done", "bang! x"):
                dd = os.path.join(d, nm)
                os.makedirs(dd)
                open(os.path.join(dd, gameside.SOLO), "w").write("@echo off\r\necho hi> \"%~dp0marker.txt\"\r\n")
                assert installer.play_solo(fake_state(dd, "done")) == {}
                for _ in range(50):
                    if os.path.isfile(os.path.join(dd, "marker.txt")):
                        break
                    time.sleep(0.1)
                assert os.path.isfile(os.path.join(dd, "marker.txt")), "the dummy launcher did not run from: " + nm
        assert seen["kw"]["cwd"] == d and seen["kw"]["close_fds"] is True
        if sys.platform == "win32":
            import subprocess
            assert seen["kw"]["creationflags"] == subprocess.CREATE_NEW_CONSOLE | subprocess.CREATE_NEW_PROCESS_GROUP
        # 2) over HTTP: token needed, the request body cannot change what runs, the dummy launcher really runs and survives
        tok = "tok123"
        srv = installer.Server(("127.0.0.1", 0), installer.make_handler(fake_state(d, "done"), tok))
        threading.Thread(target=srv.serve_forever, daemon=True).start()
        port = srv.server_address[1]
        try:
            assert post(port, "/api/play", "wrong")[0] == 403
            if sys.platform == "win32":
                code, r = post(port, "/api/play", tok, json.dumps({"path": "C:/Windows/notepad.exe", "bat": "x", "what": "y"}).encode())
                assert code == 200 and r == {}, (code, r)
                for _ in range(50):
                    if os.path.isfile(os.path.join(d, "marker.txt")):
                        break
                    time.sleep(0.1)
                assert os.path.isfile(os.path.join(d, "marker.txt")), "the dummy launcher did not run"
            srv.RequestHandlerClass  # noqa: B018
        finally:
            srv.shutdown()
        srv2 = installer.Server(("127.0.0.1", 0), installer.make_handler(fake_state(d, "running"), tok))
        threading.Thread(target=srv2.serve_forever, daemon=True).start()
        try:
            code, r = post(srv2.server_address[1], "/api/play", tok)
            assert code == 200 and "error" in r, (code, r)
        finally:
            srv2.shutdown()
    print("test_play: OK")


if __name__ == "__main__":
    main()

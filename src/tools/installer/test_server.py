"""Host test for the installer's local server guards (s185 sec): Host header + token. python tools/installer/test_server.py"""
import http.client
import os
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import installer  # noqa: E402


def get(port, path, host, token=None):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    c.putrequest("GET", path, skip_host=True)
    if host is not None:
        c.putheader("Host", host)
    if token is not None:
        c.putheader("X-Token", token)
    c.endheaders()
    r = c.getresponse()
    r.read()
    c.close()
    return r.status


def main():
    srv = installer.Server(("127.0.0.1", 0), installer.make_handler(None, "sekret"))
    port = srv.server_address[1]
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    try:
        assert get(port, "/", "127.0.0.1:%d" % port) == 200
        assert get(port, "/", "localhost:%d" % port) == 200
        for bad in ("evil.example:%d" % port, "127.0.0.1", "127.0.0.1:1", "localhost", "127.0.0.1.evil.com:%d" % port, "", None):
            assert get(port, "/", bad) == 403, bad
            assert get(port, "/index.html", bad) == 403, bad
            assert get(port, "/api/state", bad, "sekret") == 403, bad
        h = "127.0.0.1:%d" % port
        assert get(port, "/api/state", h) == 403 and get(port, "/api/state", h, "wrong") == 403 and get(port, "/api/state", h, "sekreT") == 403
        assert get(port, "/sfx/x", h, "wrong") == 403
        print("ok")
    finally:
        srv.shutdown()


main()

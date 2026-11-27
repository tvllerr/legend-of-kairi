"""(s227) host test: the GitHub installer's install_game_side copies the manifest's "colour-luts/<file>" entries into <root>/bin
(savagewind.exe runs from <root>/bin and rt_resource("colour-luts") looks beside it), also over an older install.
Run: python tools/installer/test_luts.py      (fakes only; no game, no real install)"""
import hashlib
import json
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gameside  # noqa: E402


def put(p, b):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "wb") as f:
        f.write(b)


def release(base, tag):
    rel = os.path.join(base, "rel")
    files = {n: b"x" + tag for n in gameside.NEEDED}
    files.update({"colour-luts/Savage_Wind.cube": b"lut" + tag, "colour-luts/README.md": b"readme"})
    for n, b in files.items():
        put(os.path.join(rel, "bin", n), b)
    man = {"bin": {n: {"size": len(b), "sha256": hashlib.sha256(b).hexdigest()} for n, b in files.items()}}
    put(os.path.join(rel, "manifest.json"), json.dumps(man).encode())
    return rel


def main():
    with tempfile.TemporaryDirectory() as d:
        sw = os.path.join(d, "sw")
        os.makedirs(sw)
        root = os.path.join(d, "install")
        os.makedirs(root)
        for tag in (b"A", b"B"):   # fresh install, then an update over it
            rel = release(d, tag)
            gameside.install_shortcuts = lambda *a, **k: []
            gameside.write_game_id = lambda *a, **k: None
            gameside.write_build_id = lambda *a, **k: None
            gameside.install_game_side(root, sw, lambda m: None, release=rel)
            got = open(os.path.join(root, "bin", "colour-luts", "Savage_Wind.cube"), "rb").read()
            assert got == b"lut" + tag, got
            assert os.path.isfile(os.path.join(root, "bin", "colour-luts", "README.md"))
            assert not [n for _, _, fs in os.walk(root) for n in fs if n.endswith(".new")]
            print("OK   install_game_side copies colour-luts/ into bin/ (%s)" % tag.decode())


main()

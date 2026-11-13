"""(s223) host test: meltycheck finds Melty's files / licence lines / connector strings; mkzip needs --track and the GitHub
track's kairi_env.txt is single player; mksrc stops when Melty's content got past its EXCLUDE list.
python tools/installer/test_meltycheck.py"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import meltycheck  # noqa: E402
import mkzip  # noqa: E402

TMP = os.environ.get("LOK_TEST_TMP")
M = "Mel" + "ty"


def w(p, b):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "wb") as f:
        f.write(b)


def t_scan(d):
    clean = os.path.join(d, "clean")
    w(os.path.join(clean, "tools", "installer", "a.py"), b"# mentions melty and MELTY_NET_DIR in our own words\n")
    w(os.path.join(clean, "runtime", "sora_melty_types.h"), b"/* ours, MIT */")
    w(os.path.join(clean, "bin", "sora_mod.dll"), b"MZ\x00ours\x00")
    assert meltycheck.scan(clean) == [], meltycheck.scan(clean)
    cases = [("runtime/melty/x.txt", b"x", "Melty's folder"), ("src/melty-setup/build.py", b"x", "Melty's folder"),
             ("a/sora_melty.cpp", b"x", "Melty's file"), ("a/Melty_Net.h", b"x", "Melty's file"),
             ("a/copy.h", b"// SPDX-License-Identifier: LicenseRef-" + M.encode() + b"-Mashups-Only\n", "has"),
             ("a/note.txt", b"these are licensed for " + M.encode() + b" mashups", "has"),
             ("a/x.cpp", b"using namespace " + M.lower().encode() + b"::net;", "has"),
             ("bin/sora_mod.dll", b"MZ\x00" + M.upper().encode() + b"_NET_IMPLEMENTATION\x00", "has")]
    cases += [("bin/x%d.dll" % i, b"MZ\x00" + m + b"\x00", "has") for i, m in enumerate(meltycheck.BIN_MARKERS)]
    for i, (rel, body, frag) in enumerate(cases):
        t = os.path.join(d, "c%d" % i)
        w(os.path.join(t, "ok.txt"), b"fine")
        w(os.path.join(t, *rel.split("/")), body)
        got = meltycheck.scan(t)
        assert len(got) == 1 and frag in got[0], (rel, got)
    assert meltycheck.scan(HERE) == [], "this folder (meltycheck.py itself included) is clean: %s" % meltycheck.scan(HERE)
    assert meltycheck.main([clean]) == 0 and meltycheck.main([os.path.join(d, "c0")]) == 1
    # one file (build_github_dll.sh checks the DLL alone), and a path that is not there
    assert meltycheck.scan(os.path.join(d, "c%d" % (len(cases) - 1), "bin", "x%d.dll" % (len(meltycheck.BIN_MARKERS) - 1)))
    assert meltycheck.scan(os.path.join(clean, "bin", "sora_mod.dll")) == []
    assert "not found" in meltycheck.scan(os.path.join(d, "nothing"))[0]


def t_mkzip():
    assert "SAVAGEWIND_SORA_SOLO=1\n" in mkzip.KAIRI_ENV_GITHUB and "SOLO" not in mkzip.KAIRI_ENV
    assert mkzip.KAIRI_ENV_GITHUB.startswith(mkzip.KAIRI_ENV)
    r = subprocess.run([sys.executable, os.path.join(HERE, "mkzip.py"), "--kit", "x", "--out", "y", "--src-commit", "z"],
                       capture_output=True, text=True)
    assert r.returncode == 2 and "--track" in r.stderr, (r.returncode, r.stderr[-300:])
    r = subprocess.run([sys.executable, os.path.join(HERE, "mkzip.py"), "--kit", "x", "--out", "y", "--src-commit", "z", "--track", "steam"],
                       capture_output=True, text=True)
    assert r.returncode == 2 and "invalid choice" in r.stderr
    for n in ("tools/installer/meltycheck.py", "tools/installer/test_meltycheck.py", "tools/installer/test_swsetup.py", "tools/installer/build_github_dll.sh",
              "tools/installer/test_swinstall.py"):
        assert n in mkzip.SKIP_TOOLS, n


def t_mksrc(d):
    """a Melty file outside EXCLUDE's folders stops the snapshot"""
    repo = os.path.join(d, "repo")
    w(os.path.join(repo, "LICENSE"), b"MIT\nCopyright (c) 2026 Peiikai (LAS Recomp, on which this project is built)\n")
    w(os.path.join(repo, "README.md"), b"readme")
    w(os.path.join(repo, "runtime", "sora.c"), b"int x;")
    w(os.path.join(repo, "runtime", "melty", "sora_melty.cpp"), b"excluded by its folder")
    git = ["git", "-C", repo, "-c", "user.name=t", "-c", "user.email=t@example.invalid"]
    subprocess.run(["git", "init", "-q", repo], check=True)
    subprocess.run(git + ["add", "-A"], check=True)
    subprocess.run(git + ["commit", "-q", "-m", "a"], check=True)
    mksrc = os.path.join(HERE, "..", "mksrc.py")
    r = subprocess.run([sys.executable, mksrc, "HEAD", os.path.join(d, "out1"), "--repo", repo], capture_output=True, text=True)
    assert r.returncode == 0 and "meltycheck ok" in r.stdout, (r.stdout, r.stderr)
    assert not os.path.exists(os.path.join(d, "out1", "runtime", "melty"))
    w(os.path.join(repo, "third_party", "melty_net.h"), b"// a copy that slipped in")
    subprocess.run(git + ["add", "-A"], check=True)
    subprocess.run(git + ["commit", "-q", "-m", "b"], check=True)
    r = subprocess.run([sys.executable, mksrc, "HEAD", os.path.join(d, "out2"), "--repo", repo], capture_output=True, text=True)
    assert r.returncode == 1 and "Melty's content in the snapshot" in r.stderr and "melty_net.h" in r.stderr, (r.returncode, r.stderr)


def main():
    with tempfile.TemporaryDirectory(dir=TMP) as d:
        t_scan(os.path.join(d, "scan"))
        t_mkzip()
        t_mksrc(os.path.join(d, "src"))
    print("ok (3 groups)")


main()

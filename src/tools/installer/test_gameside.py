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


def run_one(base, name, multi, fake):
    root = os.path.join(base, "root " + name)
    sw = os.path.join(base, "sw " + name)
    os.makedirs(os.path.join(root, "bin"))
    os.makedirs(sw)
    open(os.path.join(sw, "savagewind.ini"), "w").close()
    shutil.copy2(fake, os.path.join(root, "bin", "fakegame.exe"))
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
    ok = os.path.isfile(marker) and "arg1=extracted" in open(marker).read() and os.path.normcase(sw) in os.path.normcase(open(marker).read())
    return ok, p.stdout.decode("mbcs", "replace") + p.stderr.decode("mbcs", "replace")


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


def main():
    test_peers()
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
    finally:
        if "--keep" not in sys.argv:
            shutil.rmtree(base, ignore_errors=True)
    print("failures:", fail)
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())

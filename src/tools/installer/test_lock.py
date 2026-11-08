"""Host test for the installer lock (S2) + the non-ASCII refusals (S6). python tools/installer/test_lock.py"""
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import installer  # noqa: E402

CHILD = "import sys; sys.path.insert(0, %r); import installer; print(installer.acquire_lock(%r))"


def child(root):
    return subprocess.run([sys.executable, "-c", CHILD % (os.path.dirname(os.path.abspath(__file__)), root)],
                          capture_output=True, text=True).stdout.strip()


def main():
    with tempfile.TemporaryDirectory(dir=os.environ.get("LOK_TEST_TMP")) as r:
        assert installer.acquire_lock(r), "first lock"
        assert child(r) == "False", "second installer must be refused while the first lives"
        lp = os.path.join(r, ".installer-ui", "lock")
        installer.release_lock(lp)
        assert not os.path.exists(lp), "released"
        # stale lock: pid of a process that has exited
        p = subprocess.Popen([sys.executable, "-c", "pass"])
        p.wait()
        open(lp, "w").write(str(p.pid))
        assert not installer.pid_alive(p.pid)
        assert child(r) == "True", "stale lock must not block"
        open(lp, "w").write("garbage")
        assert child(r) == "True", "garbage lock must not block"
    ok, msg, _ = installer.check_sw("C:/Users/J\u00f6rg/Savage Wind")
    assert not ok and "plain letters" in msg, msg
    shown = []
    installer.message_box = shown.append
    sys.argv = ["installer.py", "--root", "C:/Users/J\u00f6rg/Kairi"]
    assert installer.main() == 1 and shown and "plain letters" in shown[0]
    print("ok")


main()

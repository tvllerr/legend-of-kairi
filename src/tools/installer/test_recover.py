"""Host test for installer.recover_after_bad_verify (s178 M2). python tools/installer/test_recover.py"""
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import installer  # noqa: E402


def main():
    with tempfile.TemporaryDirectory(dir=os.environ.get("LOK_TEST_TMP")) as r:
        os.makedirs(os.path.join(r, "data", "fx"))
        os.makedirs(os.path.join(r, "work", "done"))
        for f in ("data/fx/a.bin", "data/fx/b.bin"):
            open(os.path.join(r, f), "w").close()
        open(os.path.join(r, "work", "done", "fx_a"), "w").close()
        bad = ["fx       fx/a.bin       DIFF bytes", "fx       ../../evil  MISSING"]
        msg, files = installer.recover_after_bad_verify(r, bad)
        assert not os.path.exists(os.path.join(r, "data", "fx", "a.bin")), "bad file not removed"
        assert os.path.exists(os.path.join(r, "data", "fx", "b.bin")), "good file removed"
        assert os.path.exists(os.path.join(r, "work", "done", "fx_a")), "marker kept (the step reruns because its output is gone)"
        assert "press Install again" in msg, msg
        msg2, _ = installer.recover_after_bad_verify(r, bad, files)
        assert "send the files" in msg2, msg2
        msg3, none = installer.recover_after_bad_verify(r, ["garbage line"])
        assert not os.path.exists(os.path.join(r, "work", "done", "fx_a")), "unclear -> markers cleared"
    print("ok")


main()

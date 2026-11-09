"""Host test for the (s183) pre-checks: installer.check_sw game version, gameside.write_build_id / clear_build_id.
python tools/installer/test_precheck.py"""
import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gameside  # noqa: E402
import installer  # noqa: E402


def make_sw(d, version):
    """a fake Savage Wind folder; version None = no Version.txt"""
    with open(os.path.join(d, "savagewind.exe"), "wb") as f:
        f.write(b"x")
    with open(os.path.join(d, "README.md"), "w") as f:
        f.write("SavageWind-%s" % installer.SW_VERSION)
    pack = os.path.join(d, "extracted", "romfs", "Pack")
    os.makedirs(pack)
    open(os.path.join(pack, "Bootup.pack"), "wb").close()
    if version is not None:
        os.makedirs(os.path.join(d, "extracted", "romfs", "System"))
        with open(os.path.join(d, "extracted", "romfs", "System", "Version.txt"), "wb") as f:
            f.write(version)


def main():
    os.environ.pop("SAVAGEWIND_DATA_ROOT", None)
    cases = [(b"1.9.0", True, ""), (b"1.9.0\r\n", True, ""), (b"\xef\xbb\xbf1.9.0", True, "Could not read"),
             (b"1.8.2", False, "has 1.8.2"), (b"1.6.0\n", False, "needs Breath of the Wild 1.9.0"),
             (b"", True, "Could not read"), (b"garbage", True, "Could not read"), (None, True, "Could not read")]
    for raw, want_ok, frag in cases:
        with tempfile.TemporaryDirectory(dir=os.environ.get("LOK_TEST_TMP")) as d:
            make_sw(d, raw)
            ok, msg, info = installer.check_sw(d)
            assert ok == want_ok and frag in msg, (raw, ok, msg)
            if not ok:
                assert msg == "Legend of Kairi needs Breath of the Wild 1.9.0; your Savage Wind has %s." % raw.decode().strip(), msg
    # build id: written from the manifest, cleared at the start of a run
    with tempfile.TemporaryDirectory(dir=os.environ.get("LOK_TEST_TMP")) as r:
        logs = []
        assert gameside.write_build_id(r, r, logs.append) == "" and not os.path.exists(os.path.join(r, "data", "BUILD_ID")), "no manifest"
        with open(os.path.join(r, "manifest.json"), "w") as f:
            json.dump({"data": {"build_id": "f9088e2d0399c69c"}}, f)
        assert gameside.write_build_id(r, r, logs.append) == "f9088e2d0399c69c"
        assert open(os.path.join(r, "data", "BUILD_ID"), "rb").read() == b"f9088e2d0399c69c\r\n"
        gameside.clear_build_id(r)
        assert not os.path.exists(os.path.join(r, "data", "BUILD_ID"))
        gameside.clear_build_id(r)      # already gone: no error
        with open(os.path.join(r, "manifest.json"), "w") as f:
            json.dump({"data": {"build_id": 'bad" & x'}}, f)
        assert gameside.write_build_id(r, r, logs.append) == "", "odd characters are refused"
    print("ok")


main()

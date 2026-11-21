"""Host test for the KH2 folder check (installer.check_kh2: Steam Image/dt or Epic Image/en, picked by the exe's sha1) and
the Epic folder search (installer.epic_kh2: the Epic launcher's install manifests). Synthetic folders only.
python tools/installer/test_kh2folder.py"""
import hashlib
import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import installer  # noqa: E402

STEAM_EXE, EPIC_EXE, OTHER_EXE = b"fake steam kh2 exe", b"fake epic kh2 exe", b"some other kh2 exe"


def sha1u(b):
    return hashlib.sha1(b).hexdigest().upper()


def make_kh2(d, exe, subs):
    """a fake KH 1.5+2.5 folder: the KH2 FM exe with these bytes + Image/<sub>/kh2_first.hed for each sub"""
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, installer.KH2_EXE), "wb") as f:
        f.write(exe)
    for s in subs:
        os.makedirs(os.path.join(d, "Image", s))
        open(os.path.join(d, "Image", s, "kh2_first.hed"), "wb").close()
    return d


def main():
    # the real table has exactly the two stores
    assert sorted(v[0] for v in installer.KH2_BUILDS.values()) == ["Epic", "Steam"], installer.KH2_BUILDS
    installer.KH2_BUILDS = {sha1u(STEAM_EXE): ("Steam", "dt"), sha1u(EPIC_EXE): ("Epic", "en")}
    with tempfile.TemporaryDirectory(dir=os.environ.get("LOK_TEST_TMP")) as t:
        steam = make_kh2(os.path.join(t, "steam"), STEAM_EXE, ["dt"])
        epic = make_kh2(os.path.join(t, "epic"), EPIC_EXE, ["en"])
        for pick in (steam, os.path.join(steam, "Image"), os.path.join(steam, "Image", "dt")):
            ok, msg, info = installer.check_kh2(pick)
            assert ok and msg.endswith("(Steam).") and info["build"] == "Steam", (pick, msg)
            assert info["img"] == os.path.join(steam, "Image", "dt").replace("\\", "/"), info
        for pick in (epic, os.path.join(epic, "Image"), os.path.join(epic, "Image", "en")):
            ok, msg, info = installer.check_kh2(pick)
            assert ok and msg.endswith("(Epic).") and info["build"] == "Epic", (pick, msg)
            assert info["img"] == os.path.join(epic, "Image", "en").replace("\\", "/"), info
            assert info["exe"] == os.path.join(epic, installer.KH2_EXE).replace("\\", "/"), info
        # another exe version: refused, whatever its folder looks like
        ok, msg, _ = installer.check_kh2(make_kh2(os.path.join(t, "other"), OTHER_EXE, ["dt", "en"]))
        assert not ok and "not the Steam or Epic release" in msg, msg
        # the Epic exe with only a Steam-style Image/dt (and the other way round): its own image folder is missing
        ok, msg, _ = installer.check_kh2(make_kh2(os.path.join(t, "mixed1"), EPIC_EXE, ["dt"]))
        assert not ok and "no Image/en" in msg, msg
        ok, msg, _ = installer.check_kh2(make_kh2(os.path.join(t, "mixed2"), STEAM_EXE, ["en"]))
        assert not ok and "no Image/dt" in msg, msg
        # not a KH folder at all / nothing picked
        ok, msg, _ = installer.check_kh2(t)
        assert not ok and "not the KINGDOM HEARTS" in msg, msg
        ok, msg, _ = installer.check_kh2("")
        assert not ok and "Steam or Epic" in msg, msg

        # Epic launcher manifests: the first one whose InstallLocation holds the KH2 exe + Image/en
        if sys.platform == "win32":
            pd = os.path.join(t, "ProgramData")
            man = os.path.join(pd, "Epic", "EpicGamesLauncher", "Data", "Manifests")
            os.makedirs(man)
            old = os.environ.get("PROGRAMDATA")
            os.environ["PROGRAMDATA"] = pd
            try:
                assert installer.epic_kh2() == "", "no manifests -> nothing"
                with open(os.path.join(man, "0A.item"), "w", encoding="utf-8") as f:
                    json.dump({"DisplayName": "Another game", "InstallLocation": os.path.join(t, "nothing")}, f)
                with open(os.path.join(man, "0B.item"), "w", encoding="utf-8") as f:
                    f.write("{ not json")
                with open(os.path.join(man, "0C.item"), "w", encoding="utf-8") as f:
                    json.dump({"DisplayName": "Steam-shaped KH", "InstallLocation": steam}, f)    # no Image/en
                assert installer.epic_kh2() == "", installer.epic_kh2()
                with open(os.path.join(man, "0D.item"), "w", encoding="utf-8") as f:
                    json.dump({"DisplayName": "KINGDOM HEARTS HD 1.5+2.5 ReMIX", "InstallLocation": epic}, f)
                got = installer.epic_kh2()
                assert got == os.path.normpath(epic).replace("\\", "/"), got
            finally:
                if old is None:
                    os.environ.pop("PROGRAMDATA", None)
                else:
                    os.environ["PROGRAMDATA"] = old
    print("PASS")


if __name__ == "__main__":
    main()

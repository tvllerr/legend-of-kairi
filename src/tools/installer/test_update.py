"""Host test for the in-place update (s183, update.py): old-folder detection, copy/swap + its crash recovery, the changed-data
invalidation and the cleanup.  python tools/installer/test_update.py"""
import json
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import update  # noqa: E402


def w(p, t=""):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write(t)


def r(p):
    with open(p, encoding="utf-8") as f:
        return f.read()


def man(version, data_files, build_id):
    return {"release": "Legend of Kairi", "version": version, "data": {"build_id": build_id}}, \
        {"build_id": build_id, "files": {k: {"size": 1, "sha256": v} for k, v in data_files.items()}}


def make_release(d, version, data_files, build_id, top=True):
    m, dm = man(version, data_files, build_id)
    if top:
        m["top"] = ["Install Legend of Kairi.cmd", "LICENSES", "bin", "kit", "tools", "manifest.json"]
    w(os.path.join(d, "manifest.json"), json.dumps(m))
    w(os.path.join(d, "tools", "sorabuild", "manifest.thispc.json"), json.dumps(dm))
    w(os.path.join(d, "tools", "sorabuild", "sorabuild.py"), "# " + version)
    w(os.path.join(d, "kit", "kit.json"), version)
    w(os.path.join(d, "bin", "savagewind.exe"), "exe " + version)
    w(os.path.join(d, "LICENSES", "README.txt"), version)
    w(os.path.join(d, "Install Legend of Kairi.cmd"), "@echo " + version)
    return d


def make_old_install(d, version, data_files, build_id):
    make_release(d, version, data_files, build_id, top=False)
    w(os.path.join(d, "kit", "only_in_old.txt"), "gone after the update")       # removed files must disappear
    w(os.path.join(d, "tools", "sorabuild", "old_step.py"), "old")
    for f in data_files:
        w(os.path.join(d, "data", f), "data " + f)
    for s in ("step_a", "step_b", "step_c", "_kh2pick"):
        w(os.path.join(d, "work", "done", s), "done")
    w(os.path.join(d, "work", "kh2", "x.bin"), "kh2")
    w(os.path.join(d, "logs", "last.log"), "log")
    w(os.path.join(d, "Play Legend of Kairi Multiplayer.bat"), 'set "PEERS=100.1.2.3:7790"')
    w(os.path.join(d, ".installer-ui", "choices.json"), json.dumps({"sw": "D:/SW", "kh2": "D:/KH2"}))
    return d


LIST = ("step_a                   bakes    done  deps=- out=a.bin\n"
        "step_b                   fx       done  deps=step_a out=fx/b.bin fx/c.bin\n"
        "step_c                   hud      done  deps=- out=hud\n"
        "step_n                   other    -     deps=- out=-\n")


def test_detect(base):
    rel = make_release(os.path.join(base, "new"), "0.1.3-alpha", {"a.bin": "1"}, "B")
    old = make_old_install(os.path.join(base, "old"), "0.1.1-alpha", {"a.bin": "1"}, "A")
    ok, msg, info = update.detect(old, rel, "0.1.3-alpha")
    assert ok and info["version"] == "0.1.1-alpha" and "0.1.3-alpha" in msg, msg
    assert info["sw"] == "D:/SW" and info["kh2"] == "D:/KH2" and not info["same"]
    ok, _, info2 = update.detect(os.path.join(old, "bin"), rel, "0.1.3-alpha")          # picked bin/ inside it
    assert ok and os.path.normcase(info2["root"]) == os.path.normcase(old.replace("\\", "/")), info2
    ok, msg, _ = update.detect(rel, rel, "0.1.3-alpha")
    assert not ok and "new download itself" in msg, msg
    sw = os.path.join(base, "Savage Wind")
    w(os.path.join(sw, "savagewind.exe"), "x")
    ok, msg, _ = update.detect(sw, rel, "0.1.3-alpha")
    assert not ok and "Savage Wind folder" in msg, msg
    rnd = os.path.join(base, "random")
    w(os.path.join(rnd, "manifest.json"), json.dumps({"release": "Something else", "version": "9"}))
    ok, msg, _ = update.detect(rnd, rel, "0.1.3-alpha")
    assert not ok and "not a Legend of Kairi folder" in msg, msg
    ok, msg, _ = update.detect(os.path.join(base, "nope"), rel, "0.1.3-alpha")
    assert not ok and "does not exist" in msg, msg
    fresh = make_release(os.path.join(base, "fresh"), "0.1.0-alpha", {}, "Z")             # unzipped, never installed
    ok, msg, _ = update.detect(fresh, rel, "0.1.3-alpha")
    assert not ok and "never installed" in msg, msg
    ok, msg, _ = update.detect(old, rel, "0.1.0-alpha")
    assert not ok and "newer than this download" in msg, msg
    ok, msg, info = update.detect(old, rel, "0.1.1-alpha")
    assert ok and info["same"] and "same version" in msg, msg
    inner = os.path.join(rel, "oldcopy")
    make_old_install(inner, "0.1.1-alpha", {}, "A")
    ok, msg, _ = update.detect(inner, rel, "0.1.3-alpha")
    assert not ok and "inside the other" in msg, msg
    # (s184) Update.bat unpacks the new release into <old>/.kairi-download/<zip>/<top>/: that one may update its parent
    dl = os.path.join(old, ".kairi-download", "Legend-of-Kairi-0.1.3-alpha", "Legend of Kairi 0.1.3-alpha")
    make_release(dl, "0.1.3-alpha", {}, "B")
    ok, msg, info = update.detect(old, dl, "0.1.3-alpha")
    assert ok and info["root"] == old.replace(chr(92), "/"), msg
    other = os.path.join(old, "elsewhere", "rel")              # but not from any other folder inside it
    make_release(other, "0.1.3-alpha", {}, "B")
    assert not update.detect(old, other, "0.1.3-alpha")[0]
    assert ".kairi-download" not in update.release_items(dl)
    shutil.rmtree(os.path.join(old, ".kairi-download"))
    shutil.rmtree(os.path.join(old, "elsewhere"))
    # choices.json missing: the folders come from work/install_config.json
    os.remove(os.path.join(old, ".installer-ui", "choices.json"))
    w(os.path.join(old, "work", "install_config.json"), json.dumps({"romfs": "E:/G/Savage Wind/extracted/romfs",
                                                                    "kh2exe": "E:/S/KH/KINGDOM HEARTS II FINAL MIX.exe"}))
    assert update.old_choices(old) == ("E:/G/Savage Wind", "E:/S/KH")
    assert update.version_tuple("0.1.10-alpha") > update.version_tuple("0.1.9-alpha")


def test_swap_and_invalidate(base):
    new_files = {"a.bin": "1", "fx/b.bin": "2-new", "fx/c.bin": "3", "hud/h.bin": "4", "new.bin": "5"}
    rel = make_release(os.path.join(base, "new"), "0.1.3-alpha", {k: v for k, v in new_files.items() if k != "new.bin"}, "B")
    old = make_old_install(os.path.join(base, "old"), "0.1.1-alpha", {"a.bin": "1", "fx/b.bin": "2", "fx/c.bin": "3",
                                                                      "hud/h.bin": "4", "gone.bin": "6"}, "A")
    logs = []
    update.prepare(rel, old, logs.append)
    assert r(os.path.join(old, "bin", "savagewind.exe")) == "exe 0.1.3-alpha"
    assert json.load(open(os.path.join(old, "manifest.json")))["version"] == "0.1.3-alpha"
    assert r(os.path.join(old, "Install Legend of Kairi.cmd")) == "@echo 0.1.3-alpha"
    assert not os.path.exists(os.path.join(old, "kit", "only_in_old.txt")), "kit/ replaced as a whole"
    assert not os.path.exists(os.path.join(old, "tools", "sorabuild", "old_step.py")), "tools/ replaced as a whole"
    for keep in ("data/a.bin", "work/kh2/x.bin", "work/done/_kh2pick", "logs/last.log", "Play Legend of Kairi Multiplayer.bat",
                 ".installer-ui/choices.json"):
        assert os.path.exists(os.path.join(old, keep)), keep + " must stay"
    bak = os.path.join(old, update.UPD, "bak")
    assert r(os.path.join(bak, "bin", "savagewind.exe")) == "exe 0.1.1-alpha", "old copy kept until the update finished"
    assert os.path.exists(os.path.join(bak, "kit", "only_in_old.txt"))
    assert not os.path.exists(os.path.join(old, update.UPD, "stage"))
    j = update.read_journal(old)
    assert j["from_version"] == "0.1.1-alpha" and j["phase"] == "swapped"
    # data: fx/b.bin changed (step_b owns it), gone.bin is only in the old manifest (deleted, no step needed)
    hit = update.invalidate(old, LIST, logs.append)
    assert hit == ["step_b"], hit
    assert not os.path.exists(os.path.join(old, "work", "done", "step_b"))
    assert os.path.exists(os.path.join(old, "work", "done", "step_a")) and os.path.exists(os.path.join(old, "work", "done", "step_c"))
    assert not os.path.exists(os.path.join(old, "data", "fx", "b.bin")) and not os.path.exists(os.path.join(old, "data", "gone.bin"))
    assert os.path.exists(os.path.join(old, "data", "fx", "c.bin")), "unchanged files stay"
    # a second Update of the same release (e.g. after a failed build): copies again, does NOT invalidate again
    w(os.path.join(old, "work", "done", "step_b"), "done")     # the build re-ran it meanwhile
    update.prepare(rel, old, logs.append)
    assert r(os.path.join(bak, "bin", "savagewind.exe")) == "exe 0.1.1-alpha", "bak is never overwritten by a newer copy"
    assert update.invalidate(old, LIST, logs.append) == ["step_b"]
    assert os.path.exists(os.path.join(old, "work", "done", "step_b")), "an earlier attempt's invalidation is not repeated"
    assert update.detect(old, rel, "0.1.3-alpha")[2]["version"] == "0.1.1-alpha", "unfinished update shows the version it came from"
    update.finish(old, logs.append)
    assert not os.path.exists(os.path.join(old, update.UPD))
    assert update.detect(old, rel, "0.1.3-alpha")[2]["same"]


def test_unowned_rebuilds_all(base):
    rel = make_release(os.path.join(base, "new"), "0.1.3-alpha", {"a.bin": "1", "z/new.bin": "9"}, "B")
    old = make_old_install(os.path.join(base, "old"), "0.1.1-alpha", {"a.bin": "1"}, "A")
    update.prepare(rel, old, print)
    assert update.invalidate(old, LIST, print) == ["*"]
    done = os.listdir(os.path.join(old, "work", "done"))
    assert done == ["_kh2pick"], done      # every step marker gone, the KH2 extract marker kept
    # no old manifest at all: same
    old2 = make_old_install(os.path.join(base, "old2"), "0.1.0-alpha", {"a.bin": "1"}, "A")
    os.remove(os.path.join(old2, "tools", "sorabuild", "manifest.thispc.json"))
    update.prepare(rel, old2, print)
    assert update.invalidate(old2, LIST, print) == ["*"]


def test_crash_recovery(base):
    rel = make_release(os.path.join(base, "new"), "0.1.3-alpha", {"a.bin": "1"}, "B")
    old = make_old_install(os.path.join(base, "old"), "0.1.1-alpha", {"a.bin": "1"}, "A")
    # 1) a crash while copying: a half stage is left behind; the old folder is untouched and the next run starts the stage over
    w(os.path.join(old, update.UPD, "stage", "kit", "half.bin"), "half")
    update.write_journal(old, {"from_version": "0.1.1-alpha", "phase": "staging"})
    shutil.copy2(os.path.join(old, "tools", "sorabuild", "manifest.thispc.json"), os.path.join(old, update.UPD, "old_data_manifest.json"))
    # 2) a failing rename in the middle of the swap (a file in use): this run's items go back, the error is plain words
    real = os.rename
    calls = {"n": 0}

    def flaky(a, b):
        calls["n"] += 1
        if os.path.basename(a) == "tools" and update.UPD + os.sep + "stage" in a:
            raise PermissionError(5, "in use", a)
        return real(a, b)
    update.os.rename = flaky
    try:
        update.prepare(rel, old, print)
        raise AssertionError("must fail")
    except RuntimeError as e:
        assert "Close every window" in str(e), e
    finally:
        update.os.rename = real
    assert r(os.path.join(old, "bin", "savagewind.exe")) == "exe 0.1.1-alpha", "swapped items of the failed run were put back"
    assert json.load(open(os.path.join(old, "manifest.json")))["version"] == "0.1.1-alpha"
    assert os.path.exists(os.path.join(old, "kit", "only_in_old.txt"))
    assert os.path.exists(os.path.join(old, "tools", "sorabuild", "old_step.py")), "tools/ was moved to bak, then back"
    # 3) a hard crash (power loss) right in the middle: bin swapped, kit moved to bak but the new one not in yet
    os.makedirs(os.path.join(old, update.UPD, "bak"), exist_ok=True)
    real(os.path.join(old, "kit"), os.path.join(old, update.UPD, "bak", "kit"))
    real(os.path.join(old, "bin"), os.path.join(old, update.UPD, "bak", "bin"))
    shutil.copytree(os.path.join(rel, "bin"), os.path.join(old, "bin"))
    update.write_journal(old, dict(update.read_journal(old), phase="swapping"))
    assert not os.path.exists(os.path.join(old, "kit"))
    update.prepare(rel, old, print)                # Update again repairs it
    for n in ("bin/savagewind.exe", "kit/kit.json", "tools/sorabuild/sorabuild.py", "manifest.json"):
        assert "0.1.3-alpha" in r(os.path.join(old, n)), n
    assert r(os.path.join(old, update.UPD, "bak", "kit", "kit.json")) == "0.1.1-alpha", "the original stays the bak"
    assert update.read_journal(old)["from_version"] == "0.1.1-alpha"


def test_release_items(base):
    rel = make_release(os.path.join(base, "new"), "0.1.3-alpha", {}, "B", top=False)       # an older manifest: no "top"
    w(os.path.join(rel, "data", "x"), "a user's build in the download folder")
    w(os.path.join(rel, "Play Legend of Kairi.bat"), "x")
    items = update.release_items(rel)
    assert items[-1] == "manifest.json" and "data" not in items and "Play Legend of Kairi.bat" not in items, items
    m = json.load(open(os.path.join(rel, "manifest.json")))
    m["top"] = ["bin", "data", "work", "../evil", "Play Legend of Kairi.bat", "manifest.json", "missing_dir"]
    w(os.path.join(rel, "manifest.json"), json.dumps(m))
    assert update.release_items(rel) == ["bin", "manifest.json"], update.release_items(rel)


def test_busy(base):
    old = make_old_install(os.path.join(base, "old"), "0.1.1-alpha", {}, "A")
    os.remove(os.path.join(old, "bin", "savagewind.exe"))
    assert update.busy_message(old) == "", update.busy_message(old)
    if sys.platform != "win32":
        return
    import subprocess
    # a stand-in game: Windows' ping.exe copied to <old>/bin/savagewind.exe (killed by its PID only)
    shutil.copy2(os.path.join(os.environ.get("SystemRoot", "C:/Windows"), "System32", "PING.EXE"), os.path.join(old, "bin", "savagewind.exe"))
    p = subprocess.Popen([os.path.join(old, "bin", "savagewind.exe"), "-n", "30", "127.0.0.1"], stdout=subprocess.DEVNULL)
    try:
        import time
        time.sleep(0.5)
        assert any(x.lower().endswith("savagewind.exe") for x in update.running_from(old)), update.running_from(old)
        assert "is running" in update.busy_message(old), update.busy_message(old)
        assert update.busy_message(os.path.join(base, "other")) == ""
    finally:
        p.kill()
        p.wait()
    assert update.busy_message(old) == ""
    # (s184) a program running from <old>/.kairi-download/ (the new release's installer window) does not count
    dlbin = os.path.join(old, ".kairi-download", "x", "kit")
    os.makedirs(dlbin)
    shutil.copy2(os.path.join(os.environ.get("SystemRoot", "C:/Windows"), "System32", "PING.EXE"), os.path.join(dlbin, "LegendOfKairiSetup.exe"))
    p = subprocess.Popen([os.path.join(dlbin, "LegendOfKairiSetup.exe"), "-n", "30", "127.0.0.1"], stdout=subprocess.DEVNULL)
    try:
        time.sleep(0.5)
        assert update.running_from(old) == [] and update.busy_message(old) == "", update.running_from(old)
    finally:
        p.kill()
        p.wait()


def test_junction_escape(base):
    """(s185 sec) a crafted OLD install whose data/fx is a junction to a folder outside, and whose old manifest names fx/b.bin: the update
    must not delete the outside file (and the same for the done markers / installer's recover)"""
    import subprocess
    new_files = {"a.bin": "1", "fx/b.bin": "2-new", "fx/c.bin": "3", "hud/h.bin": "4"}
    rel = make_release(os.path.join(base, "new"), "0.1.3-alpha", new_files, "B")
    old = make_old_install(os.path.join(base, "old"), "0.1.1-alpha", {"a.bin": "1", "fx/b.bin": "2", "fx/c.bin": "3", "hud/h.bin": "4"}, "A")
    outside = os.path.join(base, "outside")
    w(os.path.join(outside, "b.bin"), "PRECIOUS")
    w(os.path.join(outside, "c.bin"), "PRECIOUS2")
    shutil.rmtree(os.path.join(old, "data", "fx"))
    rc = subprocess.run(["cmd", "/c", "mklink", "/J", os.path.join(old, "data", "fx"), outside], capture_output=True)
    if rc.returncode != 0 or not os.path.isdir(os.path.join(old, "data", "fx")):
        print("skip junction test (mklink /J failed)")
        return
    assert update.is_reparse(os.path.join(old, "data", "fx")) and not os.path.islink(os.path.join(old, "data", "fx")) or os.name != "nt"
    assert update.safe_member(os.path.join(old, "data"), "fx/b.bin") is None
    assert update.safe_member(os.path.join(old, "data"), "a.bin") == os.path.normpath(os.path.join(old, "data", "a.bin"))
    assert update.safe_member(os.path.join(old, "data"), "../logs/last.log") is None
    logs = []
    update.prepare(rel, old, logs.append)
    hit = update.invalidate(old, LIST, logs.append)
    assert r(os.path.join(outside, "b.bin")) == "PRECIOUS" and r(os.path.join(outside, "c.bin")) == "PRECIOUS2", logs
    assert hit == ["step_b"], hit
    assert any("skipped fx/b.bin" in l for l in logs), logs
    # the installer's own recover step (verify said fx/c.bin is wrong) has the same guard
    import installer
    installer.recover_after_bad_verify(old, ["fx fx/c.bin DIFF"])
    assert r(os.path.join(outside, "c.bin")) == "PRECIOUS2"
    # a junction as the done folder: the marker files behind it stay
    shutil.rmtree(os.path.join(old, "work", "done"))
    od = os.path.join(base, "outdone")
    w(os.path.join(od, "step_a"), "x")
    subprocess.run(["cmd", "/c", "mklink", "/J", os.path.join(old, "work", "done"), od], capture_output=True, check=True)
    installer.update_after_bad_verify(old, "x")
    assert r(os.path.join(od, "step_a")) == "x"
    print("ok junction")


def test_relocated_folders(base):
    """(s185 review) a NORMAL update of an install whose data/ and work/ are junctions the player made (the big folders on another
    drive), reached through a junction above the install root too: the changed data file and its done marker are still removed"""
    import subprocess
    rel = make_release(os.path.join(base, "new"), "0.1.3-alpha", {"a.bin": "1", "fx/b.bin": "2-new", "fx/c.bin": "3", "hud/h.bin": "4"}, "B")
    real = make_old_install(os.path.join(base, "real", "old"), "0.1.1-alpha", {"a.bin": "1", "fx/b.bin": "2", "fx/c.bin": "3", "hud/h.bin": "4"}, "A")
    for n in ("data", "work"):
        shutil.move(os.path.join(real, n), os.path.join(base, "big_" + n))
        rc = subprocess.run(["cmd", "/c", "mklink", "/J", os.path.join(real, n), os.path.join(base, "big_" + n)], capture_output=True)
        if rc.returncode != 0:
            print("skip relocated-folders test (mklink /J failed)")
            return
    subprocess.run(["cmd", "/c", "mklink", "/J", os.path.join(base, "via"), os.path.join(base, "real")], capture_output=True, check=True)
    old = os.path.join(base, "via", "old")
    logs = []
    update.prepare(rel, old, logs.append)
    assert update.invalidate(old, LIST, logs.append) == ["step_b"]
    assert not os.path.exists(os.path.join(base, "big_data", "fx", "b.bin")), "the changed data file is deleted through data/"
    assert os.path.exists(os.path.join(base, "big_data", "fx", "c.bin"))
    assert not os.path.exists(os.path.join(base, "big_work", "done", "step_b")), "the marker is removed through work/"
    assert os.path.exists(os.path.join(base, "big_work", "done", "step_a"))
    assert not any("skipped" in l for l in logs), logs
    # every step rebuilt (no old manifest): the markers go, _kh2pick stays
    os.remove(os.path.join(old, update.UPD, "old_data_manifest.json"))
    j = update.read_journal(old)
    j.pop("invalidated", None)
    update.write_journal(old, j)
    assert update.invalidate(old, LIST, logs.append) == ["*"]
    assert sorted(os.listdir(os.path.join(base, "big_work", "done"))) == ["_kh2pick"], os.listdir(os.path.join(base, "big_work", "done"))
    import installer
    w(os.path.join(old, "data", "fx", "c.bin"), "wrong")
    installer.recover_after_bad_verify(old, ["fx fx/c.bin DIFF"])
    assert not os.path.exists(os.path.join(base, "big_data", "fx", "c.bin")), "recover deletes the wrong file through data/"
    print("ok relocated folders")


def main():
    for t in (test_detect, test_swap_and_invalidate, test_unowned_rebuilds_all, test_crash_recovery, test_release_items, test_busy,
              test_junction_escape, test_relocated_folders):
        with tempfile.TemporaryDirectory(dir=os.environ.get("LOK_TEST_TMP")) as base:
            t(base)
    print("ok")


main()

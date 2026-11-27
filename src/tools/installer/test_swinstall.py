"""(s223, s254: 0.6.0) host test for the installer's Savage Wind wiring: check_sw's folder kinds, the "sw" phase first (a failed
upgrade changes nothing: the old install's launcher keeps its BUILD_ID), the upgrade's disk need, and the work/done tag
(the s221 Melty fix: markers of another release's cut-off build never carry over).  python tools/installer/test_swinstall.py"""
import collections
import json
import os
import sys
import tempfile
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import installer  # noqa: E402
import swsetup  # noqa: E402

TMP = os.environ.get("LOK_TEST_TMP")


def sw_folder(d, kind):
    os.makedirs(os.path.join(d, "extracted", "romfs", "Pack"))
    open(os.path.join(d, "extracted", "romfs", "Pack", "Bootup.pack"), "wb").close()
    os.makedirs(os.path.join(d, "extracted", "romfs", "System"))
    open(os.path.join(d, "extracted", "romfs", "System", "Version.txt"), "w").write("1.9.0")
    open(os.path.join(d, "savagewind.exe"), "wb").write(b"x")
    if kind == "0.4.1":
        open(os.path.join(d, "README.md"), "w").write("SavageWind-0.4.1-alpha.4-windows.zip")
    elif kind == "upgrading":
        open(os.path.join(d, "README.md"), "w").write("half")
        open(os.path.join(d, swsetup.JOURNAL), "w").write("{}")
    else:
        open(os.path.join(d, "sw-manifest.txt"), "w").write("savagewind-update-manifest 1\nversion %s\n" % kind)


def t_check_sw(d):
    for kind, ok, upg, frag in (("0.6.0-alpha.6", True, False, "Savage Wind 0.6.0-alpha.6 with your game files."),
                                ("0.5.3-alpha.5", True, True, "Savage Wind 0.5.3-alpha.5 with your game files. The installer brings it "
                                                              "up to Savage Wind 0.6.0-alpha.6 first"),
                                ("0.4.1", True, True, "Savage Wind 0.4.1-alpha.4 with your game files. The installer brings it up to "
                                                      "Savage Wind 0.6.0-alpha.6 first"),
                                ("upgrading", True, True, "upgrade to 0.6.0-alpha.6 started"),
                                ("0.6.1-alpha.1", False, None, "This folder has Savage Wind 0.6.1-alpha.1"),
                                ("0.5.1-alpha.2", False, None, "Nothing was changed")):
        sw = os.path.join(d, kind)
        sw_folder(sw, kind)
        got_ok, msg, info = installer.check_sw(sw)
        assert got_ok == ok and frag in msg, (kind, got_ok, msg)
        if ok:
            assert info["upgrade"] == upg, (kind, info)
            # (s254) what stops working there: 0.1.4's co-op (0.4.1 / a cut-off upgrade), an older install not updated (0.5.3)
            assert ("co-op" in msg) == (upg and kind != "0.5.3-alpha.5"), msg
            assert ("Update an existing install" in msg) == (kind == "0.5.3-alpha.5"), msg
            assert ("(126 MB)" in msg) == upg, msg
            assert info["kind"] == {"0.6.0-alpha.6": "0.6.0", "0.5.3-alpha.5": "0.5.3"}.get(kind, kind), info
    assert installer.SW_VERSION == "0.6.0-alpha.6" == swsetup.SW_WANT
    # (s255, review finding 4) a cut-off upgrade that started from 0.5.3 gets the 0.5.3 sentence, not the 0.1.4 co-op one;
    # and the zip is "unpacked into this folder" (it is downloaded into the Legend of Kairi folder)
    sw = os.path.join(d, "upgrading")
    with open(os.path.join(sw, swsetup.JOURNAL), "w") as f:
        json.dump({"from": swsetup.SW_PREV, "to": swsetup.SW_WANT, "old_files": []}, f)
    got_ok, msg, info = installer.check_sw(sw)
    assert got_ok and info["kind"] == "upgrading" and "Update an existing install" in msg and "co-op" not in msg, msg
    assert "unpacks it into this folder" in msg, msg


class A:
    root = kit = kh2img = None
    stock_zip = None


def fake_state(d, mode="install"):
    st = installer.State.__new__(installer.State)
    st.lock = threading.Lock()
    st.log, st.seq = collections.deque(maxlen=600), 0
    st.mode, st.gen, st.status = mode, 1, "running"
    st.phase_state = {k: "wait" for k, _ in installer.PHASES}
    st.sw = {"path": os.path.join(d, "sw"), "ok": True, "msg": "", "info": {"upgrade": True}}
    st.upd = {"path": "", "ok": False, "msg": "", "info": {}}
    st.root, st.kit, st.stock_zip, st.job, st.proc = d, d, None, None, None
    st.error, st.t1, st.t0, st.steps = "", None, 0, {}
    return st


def t_phase_first(d):
    st = fake_state(d)
    assert st.phases()[0][0] == "sw" and [k for k, _ in st.phases()].count("sw") == 1
    st.mode = "update"
    assert [k for k, _ in st.phases()][:2] == ["sw", "copy"]
    # the upgrade fails (a download that did not check out): the run fails with its words, before anything is written
    calls = []
    keep = (swsetup.ensure_ready, installer.gameside.clear_build_id, installer.update.prepare)
    swsetup.ensure_ready = lambda *a, **k: (_ for _ in ()).throw(swsetup.Fail("The Savage Wind download did not check out."))
    installer.gameside.clear_build_id = lambda root: calls.append("clear")
    installer.update.prepare = lambda *a: calls.append("prepare")
    try:
        st = fake_state(d, "install")
        st._run2(1)
        assert st.status == "failed" and "did not check out" in st.error and calls == [], (st.status, st.error, calls)
        assert st.phase_state["sw"] == "failed"
        # stopped while it ran: nothing else runs, no failure shown
        swsetup.ensure_ready = lambda *a, **k: "stopped"
        st = fake_state(d, "install")
        st._run2(1)
        assert calls == [] and st.status == "running" and not st.error
        # ready: the run goes on to the next steps (clear_build_id first)
        swsetup.ensure_ready = lambda *a, **k: "ready"
        installer.State.set_sw = lambda self, p: None
        st = fake_state(d, "install")
        st.config = lambda: (_ for _ in ()).throw(RuntimeError("stop here"))
        st._run2(1)
        assert calls == ["clear"] and st.phase_state["sw"] == "done" and st.error == "stop here", (calls, st.error)
    finally:
        swsetup.ensure_ready, installer.gameside.clear_build_id, installer.update.prepare = keep


def t_disk(d):
    st = fake_state(d)
    st.upd = {"path": "", "ok": False}
    g = installer.free_gb(d)
    st.sw["info"] = {"upgrade": False}
    assert st.disk()["need"] == installer.NEED_DISK_GB
    st.sw["info"] = {"upgrade": True}
    assert st.disk()["need"] == installer.NEED_DISK_GB + installer.NEED_SW_GB and g is not None
    snap_w = dict(installer.PHASE_W)
    assert snap_w["sw"] < installer.SW_UPGRADE_W


def t_work_tag(d):
    rel = os.path.join(d, "rel")
    os.makedirs(os.path.join(rel, "kit"))
    json.dump({"version": "0.1.8-alpha", "data": {"build_id": "aaaa"}}, open(os.path.join(rel, "manifest.json"), "w"))
    open(os.path.join(rel, "kit", "kit.json"), "w").write('{"tools": 1}')
    tag = installer.work_tag(rel, os.path.join(rel, "kit"))
    assert tag.startswith("aaaa kit ") and len(tag) == len("aaaa kit ") + 16
    open(os.path.join(rel, "kit", "kit.json"), "w").write('{"tools": 2}')
    assert installer.work_tag(rel, os.path.join(rel, "kit")) != tag, "a kit-only fix has another tag"
    assert installer.work_tag(os.path.join(d, "none"), os.path.join(d, "none")) == "none kit none"

    def work(name, made_for=None, marks=("cs_build", "bake_sora", "_kh2")):
        w = os.path.join(d, name)
        os.makedirs(os.path.join(w, "done"))
        for m in marks:
            open(os.path.join(w, "done", m), "w").write("x")
        if made_for is not None:
            open(os.path.join(w, installer.WORK_FOR), "w").write(made_for)
        return w

    def left(w):
        return sorted(os.listdir(os.path.join(w, "done")))
    T = "b2 kit k2"
    cases = [  # (made-for.txt, update mode, markers left)
        (T + "\nrunning\n", False, ["_kh2", "bake_sora", "cs_build"]),     # the same release tried again carries on
        (T + "\ndone\n", True, ["_kh2", "bake_sora", "cs_build"]),
        ("b1 kit k1\nrunning\n", False, ["_kh2"]),                         # another release's build was cut off
        ("b1 kit k1\nrunning\n", True, ["_kh2"]),
        ("b1 kit k2\nrunning\n", True, ["_kh2"]),
        ("b1 kit k1\ndone\n", False, ["_kh2"]),
        ("b1 kit k2\ndone\n", True, ["_kh2", "bake_sora", "cs_build"]),    # finished install, same kit, updated: invalidate decides
        ("b1 kit k1\ndone\n", True, ["_kh2"]),                             # (s224) another kit: its tools are stale -> all again
        (None, False, ["_kh2"]),                                           # markers from before made-for.txt existed
        (None, True, ["_kh2"]),                                            # (s224) a finished 0.1.4 install (kit unknown) updated
    ]
    for i, (mf, upd, want) in enumerate(cases):
        w = work("w%d" % i, mf)
        installer.drop_other_release_work(w, T, upd, log=lambda s: None)
        assert left(w) == want, (mf, upd, left(w))
        assert installer.read_work_for(w) == (T, "running")
    installer.write_work_for(w, T, "done")
    assert installer.read_work_for(w) == (T, "done")
    w = os.path.join(d, "fresh")
    installer.drop_other_release_work(w, "T", False, log=lambda s: None)    # no work/ at all yet
    assert installer.read_work_for(w) == ("T", "running")


def t_gameside(d):
    """(s223) 0.1.4's Multiplayer launcher + shortcut go (a .txt copy kept), data/GAME_ID only for an id our exe carries"""
    gs = installer.gameside
    assert gs.SW_WANT == swsetup.SW_WANT and [s[0] for s in gs.SHORTCUTS] == ["Legend of Kairi.lnk"]
    root = os.path.join(d, "root")
    os.makedirs(root)
    open(os.path.join(root, gs.MULTI), "w").write('set "PEERS=100.1.2.3:7790"\n')
    open(os.path.join(root, gs.OLD_MULTI_LNK), "wb").write(b"lnk")
    assert gs.remove_old_multiplayer(root, log=lambda s: None) == [gs.MULTI, gs.OLD_MULTI_LNK]
    assert not os.path.exists(os.path.join(root, gs.MULTI)) and not os.path.exists(os.path.join(root, gs.OLD_MULTI_LNK))
    assert "100.1.2.3" in open(os.path.join(root, "work", "removed", gs.MULTI + ".txt")).read()
    assert gs.remove_old_multiplayer(root, log=lambda s: None) == []
    sw = os.path.join(d, "sw")
    os.makedirs(os.path.join(sw, "extracted"))
    os.makedirs(os.path.join(root, "bin"))
    open(os.path.join(root, "bin", "savagewind.exe"), "wb").write(b"MZ 7689ce31c1953be6\x00")
    assert gs.write_game_id(root, root, sw, log=lambda s: None) == "" and not os.path.exists(os.path.join(root, "data", "GAME_ID"))
    open(os.path.join(sw, "extracted", "swgame.dll.id"), "w").write("7689ce31c1953be6\n")
    assert gs.write_game_id(root, root, sw, log=lambda s: None) == "7689ce31c1953be6"
    assert open(os.path.join(root, "data", "GAME_ID"), "rb").read() == b"7689ce31c1953be6\r\n"
    open(os.path.join(sw, "extracted", "swgame.dll.id"), "w").write("1111111111111111\n")
    try:
        gs.write_game_id(root, root, sw, log=lambda s: None)
        raise AssertionError("an id our exe lacks must stop the install")
    except RuntimeError as e:
        assert "not the one this Legend of Kairi was made for" in str(e)
    t = gs.launcher_text(False, sw)
    for want in ("set SAVAGEWIND_SORA_SOLO=1", "set SAVAGEWIND_SORA_NET=0", "set SAVAGEWIND_LINK_ESYNC=0", "set SAVAGEWIND_VK_VDYN=0",
                 "set SAVAGEWIND_NO_UPDATE_CHECK=1", "extracted --no-launch-flags", ":swversion", ":gameid"):
        assert want in t, want


def t_mkzip_guard(d):
    """(s254, the s252 review's finding 4) mkzip packs nothing unless installer, launcher, stock folder and exe are for ONE
    Savage Wind version"""
    import mkzip
    stock = os.path.join(d, "stock")
    os.makedirs(stock)
    open(os.path.join(stock, "sw-manifest.txt"), "w").write("savagewind-update-manifest 1\nversion %s\n" % swsetup.SW_WANT)
    exe = os.path.join(d, "savagewind.exe")
    open(exe, "wb").write(b"MZ " + b"\x00".join(g.encode() for g in swsetup.GAME_IDS) + b"\x00 end")
    assert mkzip.version_mismatch(exe, stock) == [], mkzip.version_mismatch(exe, stock)
    # the exe of another Savage Wind version (0.5.3's ids)
    old = os.path.join(d, "old.exe")
    open(old, "wb").write(b"MZ 7689ce31c1953be6\x00 end")
    bad = mkzip.version_mismatch(old, stock)
    assert len(bad) == 1 and all(g in bad[0] for g in swsetup.GAME_IDS), bad
    # the runtime files from another stock folder (s250: 0.6.0 files, a 0.5.3 installer), or from a folder with no manifest
    open(os.path.join(stock, "sw-manifest.txt"), "w").write("savagewind-update-manifest 1\nversion 0.5.3-alpha.5\n")
    bad = mkzip.version_mismatch(exe, stock)
    assert len(bad) == 1 and "has Savage Wind 0.5.3-alpha.5" in bad[0] and swsetup.SW_WANT in bad[0], bad
    os.remove(os.path.join(stock, "sw-manifest.txt"))
    assert "(no sw-manifest.txt)" in mkzip.version_mismatch(exe, stock)[0]
    open(os.path.join(stock, "sw-manifest.txt"), "w").write("savagewind-update-manifest 1\nversion %s\n" % swsetup.SW_WANT)
    # the launcher's version, the pinned zip, the licence note
    keep = (mkzip.gameside.SW_WANT, dict(swsetup.STOCK), mkzip.BIN_LICENSE_NOTE)
    try:
        mkzip.gameside.SW_WANT = "0.5.3-alpha.5"
        assert "gameside.SW_WANT is 0.5.3-alpha.5" in mkzip.version_mismatch(exe, stock)[0]
        mkzip.gameside.SW_WANT = keep[0]
        swsetup.STOCK = dict(keep[1], name="SavageWind-0.5.3-alpha.5-windows.zip")
        assert "swsetup.STOCK" in mkzip.version_mismatch(exe, stock)[0]
        swsetup.STOCK = dict(keep[1])
        mkzip.BIN_LICENSE_NOTE = keep[2].replace(swsetup.SW_WANT, "0.5.3-alpha.5")
        assert "BIN_LICENSE_NOTE" in mkzip.version_mismatch(exe, stock)[0]
    finally:
        mkzip.gameside.SW_WANT, swsetup.STOCK, mkzip.BIN_LICENSE_NOTE = keep[0], dict(keep[1]), keep[2]
    assert mkzip.version_mismatch(exe, stock) == []
    # (s255, the s254 review's finding 6) the DLL fits the exe: every name it takes from savagewind.exe is exported by it
    # (objdump -p's own lines, tabs and all)
    dll_txt = ("\tDLL Name: KERNEL32.dll\n\tvma:     Ordinal  Hint  Member-Name  Bound-To\n\t00142000  <none>  011b  CloseHandle\n\n"
               "\tDLL Name: savagewind.exe\n\tvma:     Ordinal  Hint  Member-Name  Bound-To\n\t001424d8  <none>  0000  dispatch_lookup\n"
               "\t001424e0  <none>  0000  guest_call\n\n\tDLL Name: USER32.dll\n\tvma:     Ordinal  Hint  Member-Name  Bound-To\n"
               "\t00143000  <none>  0001  MessageBoxA\n\n")
    exe_txt = ("Export Address Table -- Ordinal Base 1\n\t          Ordinal  Address  Type\n\t[   0] +base[   1] 0066d12c Export RVA\n\n"
               "[Ordinal/Name Pointer] Table -- Ordinal Base 1\n\t          Ordinal   Hint Name\n\t[   0] +base[   1]  0000 dispatch_lookup\n"
               "\t[   1] +base[   2]  0001 guest_call\n\t[   2] +base[   3]  0002 other\n\nThe Function Table\n")
    assert mkzip.dll_misses("e", "d", dump=lambda p: exe_txt if p == "e" else dll_txt) == []
    assert mkzip.dll_misses("e", "d", dump=lambda p: exe_txt.replace("guest_call", "guest_call2") if p == "e" else dll_txt) == ["guest_call"]
    assert "no import from savagewind.exe" in mkzip.dll_misses("e", "d", dump=lambda p: exe_txt)[0]


def t_mkzip_kit(d):
    """(s291, the s291 review's finding 1) mkzip packs nothing with a kit whose tools were built from other sources: the SAME
    sb.kit_stale rule sb.kit_tool applies at cs_build on the player's PC (one rule, not a second hash)"""
    import subprocess
    import mkzip
    sb = mkzip.sb
    kit = os.path.join(d, "kit")
    os.makedirs(kit)

    def put(kj):
        with open(os.path.join(kit, "kit.json"), "w", encoding="utf-8") as f:
            f.write(kj if isinstance(kj, str) else json.dumps(kj))
    good = {"src": {t: sb.src_hash(t) for t in sb.KIT_SRC}, "files": {}}
    put(good)
    assert mkzip.kit_mismatch(kit) == [], mkzip.kit_mismatch(kit)
    # one tool built from old sources (the 0.2.3 kit: sorabake before PR #64's Program.cs), one tool the kit never had (kh2mot)
    old = dict(good, src=dict(good["src"], sorabake="0" * 64))
    del old["src"]["kh2mot"]
    put(old)
    bad = mkzip.kit_mismatch(kit)
    assert len(bad) == 2 and "sorabake" in bad[0] and "000000000000" in bad[0] and "kh2mot" in bad[1] and "none" in bad[1], bad
    assert bad == ["kit tool %s was built from other sources (kit %s, tree %s)" % (t, (old["src"].get(t) or "none")[:12],
                                                                                    sb.src_hash(t)[:12]) for t in ("sorabake", "kh2mot")]
    # the player's side says the same tools are stale (kit_tool would stop at cs_build / kh2mot)
    assert sb.kit_stale(old) == ["sorabake", "kh2mot"] and sb.kit_stale(good) == []
    # no src at all, a src that is not a dict, broken JSON, no kit.json
    put({"files": {}})
    assert len(mkzip.kit_mismatch(kit)) == len(sb.KIT_SRC)
    put({"src": ["x"]})
    assert len(mkzip.kit_mismatch(kit)) == len(sb.KIT_SRC)
    put("{not json")
    assert "cannot be read" in mkzip.kit_mismatch(kit)[0]
    os.remove(os.path.join(kit, "kit.json"))
    assert "cannot be read" in mkzip.kit_mismatch(kit)[0]
    # end to end: mkzip itself refuses before it writes anything (out stays absent)
    put(old)
    exe, dll = os.path.join(d, "e.exe"), os.path.join(d, "d.dll")
    for p in (exe, dll):
        open(p, "wb").write(b"MZ")
    out = os.path.join(d, "out")
    r = subprocess.run([sys.executable, "-E", "-P", os.path.join(os.path.dirname(os.path.abspath(__file__)), "mkzip.py"),
                        "--kit", kit, "--out", out, "--exe", exe, "--dll", dll, "--src-commit", "t", "--track", "github",
                        "--allow-dirty"], capture_output=True, text=True)
    assert r.returncode != 0 and "does not fit this tree, nothing was packed" in r.stderr and "sorabake" in r.stderr, r.stderr
    assert not os.path.exists(out), "mkzip wrote %s" % out
    put(good)   # a matching kit gets past the guard (then stops at the next check: the fake exe)
    r = subprocess.run([sys.executable, "-E", "-P", os.path.join(os.path.dirname(os.path.abspath(__file__)), "mkzip.py"),
                        "--kit", kit, "--out", out, "--exe", exe, "--dll", dll, "--src-commit", "t", "--track", "github",
                        "--allow-dirty", "--runtime-dlls", d], capture_output=True, text=True)
    assert r.returncode != 0 and "does not fit this tree" not in r.stderr and "version mismatch" in r.stderr, r.stderr


def main():
    with tempfile.TemporaryDirectory(dir=TMP) as d:
        for t in (t_check_sw, t_phase_first, t_disk, t_work_tag, t_gameside, t_mkzip_guard, t_mkzip_kit):
            sub = os.path.join(d, t.__name__)
            os.makedirs(sub)
            t(sub)
    print("ok (7 groups)")


main()

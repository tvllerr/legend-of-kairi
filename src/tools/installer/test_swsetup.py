"""(s223) host test for swsetup.py: the Savage Wind folder kinds, the pinned stock zip, the safe unpack (manifest last), the
upgrade journal (a cut-off upgrade carries on), backup once, the busy + other-version refusals (nothing changed) and the
stock setup's result lines.  python tools/installer/test_swsetup.py"""
import hashlib
import io
import json
import os
import shutil
import sys
import tempfile
import urllib.request
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import swsetup  # noqa: E402

GID = "7689ce31c1953be6"
TMP = os.environ.get("LOK_TEST_TMP")


def tree_hash(d):
    h = hashlib.sha256()
    for r, ds, fs in sorted(os.walk(d)):
        ds.sort()
        for n in sorted(fs):
            p = os.path.join(r, n)
            h.update(os.path.relpath(p, d).encode() + b"\0" + open(p, "rb").read())
    return h.hexdigest()


def fake_zip(path, extra=None, version=swsetup.SW_WANT):
    """a tiny stock-like zip: savagewind.exe (carries GID), README.md, swgen/swgen.exe, sw-manifest.txt (with every f line)"""
    files = {"savagewind.exe": b"MZ stock 0.5.3 " + GID.encode() + b"\x00 end", "README.md": b"# Savage Wind\nno zip name here\n",
             "swgen/swgen.exe": b"swgen", "gamelib-src/a.c": b"int a;"}
    files.update(extra or {})
    man = "savagewind-update-manifest 1\nversion %s\n" % version + "".join(
        "f %s %d %s\n" % (hashlib.sha256(b).hexdigest(), len(b), n) for n, b in sorted(files.items()))
    with zipfile.ZipFile(path, "w") as z:
        z.writestr("Savage Wind/", "")
        for n, b in files.items():
            z.writestr("Savage Wind/" + n, b)
        z.writestr("Savage Wind/" + swsetup.MANIFEST, man)
    return path


def pin(path):
    swsetup.STOCK = dict(swsetup.STOCK, size=os.path.getsize(path), sha256=swsetup.sha256_file(path))


def make_041(sw):
    os.makedirs(os.path.join(sw, "extracted", "exefs"))
    os.makedirs(os.path.join(sw, "extracted", "romfs", "Pack"))
    os.makedirs(os.path.join(sw, "save", "save_0", "0"))
    for rel, b in {"savagewind.exe": b"MZ old 0.4.1 767a39f088e81e1a\x00", "README.md": b"get SavageWind-0.4.1-alpha.4-windows.zip",
                   "savagewind.ini": b"volume = 0.30\n", "save/save_0/0/game_data.sav": b"SAVE41",
                   "extracted/exefs/main": b"main", "extracted/romfs/Pack/Bootup.pack": b"boot",
                   "extracted/swgame.dll": b"old lib", "extracted/swgame.dll.id": b"767a39f088e81e1a\n"}.items():
        with open(os.path.join(sw, *rel.split("/")), "wb") as f:
            f.write(b)


def our_exe(d):
    p = os.path.join(d, "bin", "savagewind.exe")
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "wb") as f:
        f.write(b"MZ kairi " + GID.encode() + b"\x00")
    return p


def setup_ok(sw, out, log, on_proc, alive):
    """stands in for stock's setup: builds the 0.5.3 game code + the version mark"""
    with open(os.path.join(sw, "extracted", "swgame.dll"), "wb") as f:
        f.write(b"new lib")
    with open(os.path.join(sw, "extracted", "swgame.dll.id"), "w") as f:
        f.write(GID + "\n")
    os.makedirs(os.path.join(sw, "cache"), exist_ok=True)
    with open(os.path.join(sw, "cache", "last-version.txt"), "w") as f:
        f.write(swsetup.SW_WANT + "\n")
    return "ready", ""


def no_busy(sw):
    return ""


def t_kinds(d):
    sw = os.path.join(d, "k")
    os.makedirs(sw)
    assert swsetup.sw_kind(sw) == "unknown"
    open(os.path.join(sw, "README.md"), "w").write("SavageWind-0.4.1-alpha.4-windows.zip")
    assert swsetup.sw_kind(sw) == "0.4.1"
    open(os.path.join(sw, "README.md"), "w").write("SavageWind-0.3.0-alpha.2-windows.zip")
    assert swsetup.sw_kind(sw) == "other:0.3.0-alpha.2", swsetup.sw_kind(sw)
    open(os.path.join(sw, swsetup.MANIFEST), "w").write("savagewind-update-manifest 1\nversion 0.5.3-alpha.5\n")
    assert swsetup.sw_kind(sw) == "0.5.3"
    open(os.path.join(sw, swsetup.MANIFEST), "w").write("savagewind-update-manifest 1\nversion 0.5.4-alpha.1\n")
    assert swsetup.sw_kind(sw) == "other:0.5.4-alpha.1"
    assert "Savage Wind 0.5.4-alpha.1" in swsetup.kind_message("other:0.5.4-alpha.1")
    assert "0.1.4" in swsetup.kind_message("unknown") and "Nothing was changed" in swsetup.kind_message("unknown")
    # (s226) 0.5.0 - 0.5.2: Savage Wind's own way up, in the refusal
    for k in ("other:0.5.1-alpha.1", "other:0.5.2-alpha.3"):
        m = swsetup.kind_message(k)
        assert "press Update" in m and "Savage Wind Configuration" in m and "Nothing was changed" in m and "0.1.4" in m, m
    m = swsetup.kind_message("other:0.5.0-alpha.2")
    assert "extract its zip over" in m and "press Update" not in m and "Nothing was changed" in m and "0.1.4" in m, m
    for k in ("other:0.5.4-alpha.1", "other:0.3.0-alpha.2", "other:0.5.10-alpha.1", "unknown"):
        m = swsetup.kind_message(k)
        assert "press Update" not in m and "extract its zip" not in m and "choose another folder" in m, m
    open(os.path.join(sw, swsetup.JOURNAL), "w").write("{}")
    # (s226 review G1) a stale journal in a folder of another version: refused, never "upgrading"
    assert swsetup.sw_kind(sw) == "other:0.5.4-alpha.1", swsetup.sw_kind(sw)
    open(os.path.join(sw, swsetup.MANIFEST), "w").write("savagewind-update-manifest 1\nversion 0.5.3-alpha.5\n")
    assert swsetup.sw_kind(sw) == "upgrading"   # unpacked to the end, stock's setup still to run
    os.remove(os.path.join(sw, swsetup.MANIFEST))
    assert swsetup.sw_kind(sw) == "upgrading"   # cut off mid-unpack (the manifest comes last)
    for k in ("0.5.3", "0.4.1", "upgrading"):
        assert swsetup.kind_message(k) == ""


def t_names():
    ok = ["Savage Wind/a.dll", "Savage Wind/zig/lib/x.h", "Savage Wind/SW Update.exe"]
    bad = ["Savage Wind/../x", "Other/x", "Savage Wind/a\\b", "Savage Wind/C:x", "Savage Wind/", "Savage Wind//x",
           "Savage Wind/./x", "Savage Wind/x.kairi-new", "Savage Wind/x. ", "Savage Wind/dir./x", "/Savage Wind/x"]
    for n in ok:
        assert swsetup.safe_name(n), n
    for n in bad:
        assert swsetup.safe_name(n) is None, n


def t_fetch(d):
    z = fake_zip(os.path.join(d, "stock.zip"))
    pin(z)
    assert swsetup.fetch_stock(os.path.join(d, "dl0"), z, log=lambda s: None) == z
    assert not os.path.exists(os.path.join(d, "dl0")), "a local zip is never copied"
    swsetup.STOCK["sha256"] = "0" * 64
    try:
        swsetup.fetch_stock(os.path.join(d, "dl0"), z, log=lambda s: None)
        raise AssertionError("a wrong sha must be refused")
    except swsetup.Fail as e:
        assert "not the official one" in str(e)
    pin(z)
    data = open(z, "rb").read()

    class Op:
        def __init__(self, body):
            self.body = body

        def open(self, req, timeout=0):
            assert req.full_url.startswith("https://")
            return io.BytesIO(self.body)
    dl = os.path.join(d, "dl")
    p = swsetup.fetch_stock(dl, None, log=lambda s: None, opener=Op(data))
    assert p == os.path.join(dl, swsetup.STOCK["name"]) and open(p, "rb").read() == data and not os.path.exists(p + ".part")
    assert swsetup.fetch_stock(dl, None, log=lambda s: None, opener=Op(b"")) == p, "a complete earlier download is reused"
    os.remove(p)
    for body, frag in ((data[:-5], "did not check out"), (data + b"xx", "bigger than")):
        try:
            swsetup.fetch_stock(dl, None, log=lambda s: None, opener=Op(body))
            raise AssertionError(frag)
        except swsetup.Fail as e:
            assert frag in str(e), e
        assert not os.path.exists(p)
    h = swsetup._HttpsOnly()
    try:
        h.redirect_request(urllib.request.Request("https://x/a"), None, 302, "", {}, "http://evil/a")
        raise AssertionError("an http redirect must be refused")
    except swsetup.Fail:
        pass


def t_unpack(d):
    z = fake_zip(os.path.join(d, "u.zip"))
    sw = os.path.join(d, "u")
    make_041(sw)
    order = []
    real = os.replace

    def rec(a, b):
        order.append(os.path.basename(b))
        real(a, b)
    swsetup.os.replace = rec
    try:
        n = swsetup.unpack_stock(z, sw, log=lambda s: None)
    finally:
        swsetup.os.replace = real
    assert n == 5 and order[-1] == swsetup.MANIFEST, order
    assert swsetup.program_files_ok(sw) == "" and swsetup.sw_kind(sw) == "0.5.3"
    assert open(os.path.join(sw, "save", "save_0", "0", "game_data.sav"), "rb").read() == b"SAVE41"
    assert open(os.path.join(sw, "savagewind.ini"), "rb").read() == b"volume = 0.30\n"
    assert not any(f.endswith(swsetup.NEW) for _, _, fs in os.walk(sw) for f in fs)
    with open(os.path.join(sw, "swgen", "swgen.exe"), "ab") as f:
        f.write(b"x")
    assert swsetup.program_files_ok(sw) == "swgen/swgen.exe"
    # an odd name anywhere in the zip: nothing at all is written
    bad = os.path.join(d, "bad.zip")
    with zipfile.ZipFile(bad, "w") as zz:
        zz.writestr("Savage Wind/README.md", "new")
        zz.writestr("Savage Wind/../evil.txt", "x")
        zz.writestr("Savage Wind/" + swsetup.MANIFEST, "version x\n")
    sw2 = os.path.join(d, "u2")
    make_041(sw2)
    before = tree_hash(sw2)
    try:
        swsetup.unpack_stock(bad, sw2, log=lambda s: None)
        raise AssertionError("odd name")
    except swsetup.Fail:
        pass
    assert tree_hash(sw2) == before and not os.path.exists(os.path.join(d, "evil.txt"))


def t_upgrade(d):
    z = fake_zip(os.path.join(d, "up.zip"))
    pin(z)
    sw, lok = os.path.join(d, "sw"), os.path.join(d, "lok")
    make_041(sw)
    exe = our_exe(lok)
    work = os.path.join(lok, "work")
    # 1. stock's setup fails part-way: the journal stays, the folder says "upgrading", the backup is the 0.4.1 one
    try:
        swsetup.ensure_ready(sw, exe, work, log=lambda s: None, local_zip=z, busy_check=no_busy,
                             run_setup=lambda *a: ("failed", "cut off"))
        raise AssertionError("setup failure must raise")
    except swsetup.Fail as e:
        assert "did not finish" in str(e)
    assert swsetup.sw_kind(sw) == "upgrading"
    bk = os.path.join(work, "sw-upgrade", "backup-" + swsetup.SW_OLD)
    assert open(os.path.join(bk, "save", "save_0", "0", "game_data.sav"), "rb").read() == b"SAVE41"
    # the player plays on meanwhile (or the save changes): a retry never overwrites the first copy
    with open(os.path.join(sw, "save", "save_0", "0", "game_data.sav"), "wb") as f:
        f.write(b"LATER")
    # 2. cut off in the middle of the unpack: half the files are new, the manifest is not there yet
    os.remove(os.path.join(sw, swsetup.MANIFEST))
    with open(os.path.join(sw, "README.md"), "w") as f:
        f.write("SavageWind-0.4.1-alpha.4-windows.zip")
    assert swsetup.sw_kind(sw) == "upgrading", "the journal, not the files, tells a cut-off upgrade"
    # 3. again: carries on, sets up, done
    r = swsetup.ensure_ready(sw, exe, work, log=lambda s: None, local_zip=z, busy_check=no_busy, run_setup=setup_ok)
    assert r == "upgraded", r
    assert swsetup.sw_kind(sw) == "0.5.3" and not os.path.exists(os.path.join(sw, swsetup.JOURNAL))
    assert open(os.path.join(bk, "save", "save_0", "0", "game_data.sav"), "rb").read() == b"SAVE41"
    assert open(os.path.join(sw, "save", "save_0", "0", "game_data.sav"), "rb").read() == b"LATER", "saves are never replaced"
    assert swsetup.game_files_state(sw, exe) == ""
    # 4. once more: nothing to do
    assert swsetup.ensure_ready(sw, exe, work, log=lambda s: None, local_zip=z, busy_check=no_busy,
                                run_setup=lambda *a: (_ for _ in ()).throw(AssertionError("no setup"))) == "ready"
    # 5. Savage Wind moved its game code on (its own updater): the state says so, a setup runs again
    with open(os.path.join(sw, "extracted", "swgame.dll.id"), "w") as f:
        f.write("1111111111111111\n")
    assert "another Savage Wind version" in swsetup.game_files_state(sw, exe)
    assert swsetup.ensure_ready(sw, exe, work, log=lambda s: None, local_zip=z, busy_check=no_busy, run_setup=setup_ok) == "set up"
    # 6. last-version mark missing = its setup never finished
    os.remove(os.path.join(sw, "cache", "last-version.txt"))
    assert "has not finished" in swsetup.game_files_state(sw, exe)


def t_refusals(d):
    z = fake_zip(os.path.join(d, "r.zip"))
    pin(z)
    lok = os.path.join(d, "rlok")
    exe = our_exe(lok)
    # busy
    sw = os.path.join(d, "busy")
    make_041(sw)
    before = tree_hash(sw)
    try:
        swsetup.ensure_ready(sw, exe, os.path.join(lok, "work"), log=lambda s: None, local_zip=z,
                             busy_check=lambda s: "Savage Wind is running from that folder (savagewind.exe).", run_setup=setup_ok)
        raise AssertionError("busy")
    except swsetup.Fail as e:
        assert "running" in str(e)
    assert tree_hash(sw) == before and not os.path.exists(os.path.join(lok, "work", "sw-upgrade"))
    # another version
    sw = os.path.join(d, "other")
    make_041(sw)
    with open(os.path.join(sw, swsetup.MANIFEST), "w") as f:
        f.write("savagewind-update-manifest 1\nversion 0.5.1-alpha.1\n")
    before = tree_hash(sw)
    try:
        swsetup.ensure_ready(sw, exe, os.path.join(lok, "work"), log=lambda s: None, local_zip=z, busy_check=no_busy, run_setup=setup_ok)
        raise AssertionError("other")
    except swsetup.Fail as e:
        assert "Savage Wind 0.5.1-alpha.1" in str(e) and "Nothing was changed" in str(e), e
    assert tree_hash(sw) == before
    # a download that fails: nothing changed
    sw = os.path.join(d, "nodl")
    make_041(sw)
    before = tree_hash(sw)
    swsetup.STOCK["sha256"] = "1" * 64
    try:
        swsetup.ensure_ready(sw, exe, os.path.join(lok, "work"), log=lambda s: None, local_zip=z, busy_check=no_busy, run_setup=setup_ok)
        raise AssertionError("bad zip")
    except swsetup.Fail:
        pass
    assert tree_hash(sw) == before and swsetup.sw_kind(sw) == "0.4.1"
    pin(z)
    # a fresh 0.5.3 whose own setup never ran: plain message
    sw = os.path.join(d, "fresh")
    os.makedirs(sw)
    swsetup.unpack_stock(z, sw, log=lambda s: None)
    try:
        swsetup.ensure_ready(sw, exe, os.path.join(lok, "work"), log=lambda s: None, busy_check=no_busy, run_setup=setup_ok)
        raise AssertionError("not set up")
    except swsetup.Fail as e:
        assert "not set up yet" in str(e)
    # stock turned the copy down
    sw = os.path.join(d, "refused")
    make_041(sw)
    try:
        swsetup.ensure_ready(sw, exe, os.path.join(lok, "work2"), log=lambda s: None, local_zip=z, busy_check=no_busy,
                             run_setup=lambda *a: ("refused", "no romfs"))
        raise AssertionError("refused")
    except swsetup.Fail as e:
        assert "turned down" in str(e) and "saves" in str(e)


def t_stock_lines(d):
    """run_stock_setup reads stock's own result lines (a .cmd stands in for savagewind.exe)"""
    if os.name != "nt":
        return
    cases = [("[launcher] setup: copied already -> READY", "ready"),
             ("[launcher] setup: no game files -> NOT READY", "refused"),
             ("[gamebuild] building\r\n[launcher] FAILED: zig crashed | x\r\n[launcher] setup: code -> NOT READY", "failed"),
             ("nothing", "failed")]
    for i, (lines, want) in enumerate(cases):
        sw = os.path.join(d, "st%d" % i)
        os.makedirs(sw)
        cmd = os.path.join(sw, "fake.cmd")
        with open(cmd, "w", newline="\r\n") as f:
            f.write("@echo off\n")
            for ln in lines.split("\r\n"):
                f.write("echo %s\n" % ln.replace("|", "^|").replace(">", "^>"))
            f.write("echo root=%SAVAGEWIND_DATA_ROOT% upd=%SAVAGEWIND_NO_UPDATE_CHECK% hid=%SAVAGEWIND_HIDDEN% arg=%1\n")
        got = []
        res, why = swsetup.run_stock_setup(sw, os.path.join(sw, "o.log"), log=got.append, exe=cmd)
        assert res == want, (lines, res, why)
        txt = open(os.path.join(sw, "o.log"), encoding="utf-8", errors="replace").read()
        assert "root=%s upd=1 hid=1 arg=--no-launch-flags" % sw in txt, txt
    assert why == "it ended early (exit 0)", why
    # an outside SAVAGEWIND_* setting never reaches stock's setup
    os.environ["SAVAGEWIND_GAME_LIB"] = "x"
    sw = os.path.join(d, "env")
    os.makedirs(sw)
    cmd = os.path.join(sw, "fake.cmd")
    with open(cmd, "w", newline="\r\n") as f:
        f.write("@echo off\necho lib=[%SAVAGEWIND_GAME_LIB%]\n")
    swsetup.run_stock_setup(sw, os.path.join(sw, "o.log"), log=lambda s: None, exe=cmd)
    del os.environ["SAVAGEWIND_GAME_LIB"]
    assert "lib=[]" in open(os.path.join(sw, "o.log")).read()


def main():
    keep = dict(swsetup.STOCK)
    n = 0
    with tempfile.TemporaryDirectory(dir=TMP) as d:
        for t in (t_kinds, t_names, t_fetch, t_unpack, t_upgrade, t_refusals, t_stock_lines):
            sub = os.path.join(d, t.__name__)
            os.makedirs(sub)
            t(sub) if t is not t_names else t()
            swsetup.STOCK = dict(keep)
            n += 1
    assert swsetup.STOCK["sha256"] == "667c4909a8ed72c002e5aa40a00d38720627eebf48b860beb5cd8805421f1285"
    assert swsetup.STOCK["size"] == 93924792
    print("ok (%d groups)" % n)


main()

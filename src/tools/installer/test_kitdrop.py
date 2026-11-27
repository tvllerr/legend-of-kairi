"""(s291) host test for the quicker update (kitdrop.py + installer.drop_other_release_work with kit= / list_steps=): an update
with ANOTHER kit drops only the markers of the steps whose kit tools changed (+ the steps after them); the runtime changed, no
old work/kit_used.json, a kit_used.json of another build -> every marker as before. Also: sb.KIT_STEPS names every kit tool and
every step that calls take_kit_tool / kit_tool.  python tools/installer/test_kitdrop.py"""
import ast
import glob
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SBDIR = os.path.normpath(os.path.join(HERE, "..", "sorabuild"))
sys.path.insert(0, HERE)
import installer  # noqa: E402
import kitdrop  # noqa: E402
import sb  # noqa: E402  (installer put tools/sorabuild on the path)
import update  # noqa: E402

TMP = os.environ.get("LOK_TEST_TMP")


def w(p, t=""):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write(t)


def real_list(base):
    """the real sorabuild --list of this checkout (no kit / KH2 needed): the real step names + deps"""
    cfg = os.path.join(base, "lcfg.json")
    w(cfg, json.dumps({"out": os.path.join(base, "lout"), "work": os.path.join(base, "lwork"), "fx": "fxrec", "sounds": "recipe"}))
    r = subprocess.run([sys.executable, "-E", "-P", os.path.join(SBDIR, "sorabuild.py"), "--config", cfg, "--list"],
                       capture_output=True, text=True, cwd=SBDIR)
    assert r.returncode == 0, r.stderr
    return r.stdout


def kit_json(tools=None, other=None, made="2026-11-01"):
    """a kit.json like mkkit's: src hash per tool + sha256 per file"""
    tools = tools or {}
    src = {t: v[0] for t, v in tools.items()}
    files = {}
    for t, (_, fs) in tools.items():
        for n, h in fs.items():
            files["tools/%s/%s" % (t, n)] = h
    files.update(other or {})
    return json.dumps({"made": made, "src": src, "files": dict(sorted(files.items()))}, indent=1).encode()


BASE_TOOLS = {t: ("src-" + t, {t + ".exe": "e-" + t, t + ".dll": "d-" + t}) for t in sb.KIT_SRC}
BASE_OTHER = {"dotnet/dotnet.exe": "dn1", "python/python.exe": "py1", "openkh/OpenKh.Kh2.dll": "ok1", "busybox/sh.exe": "bb1",
              "ffmpeg/ffmpeg.exe": "ff1", "vgmstream/vgmstream-cli.exe": "vg1", "LICENSES/README.txt": "lic1",
              "tools/installer_host/LegendOfKairiSetup.exe": "host1"}


def variant(**chg):
    """BASE kit with changes: tool=('src', {file: sha}) or other={rel: sha}"""
    tools = {t: (s, dict(fs)) for t, (s, fs) in BASE_TOOLS.items()}
    other = dict(BASE_OTHER)
    for k, v in chg.items():
        if k == "other":
            other.update(v)
        elif k == "gone":
            for t in v:
                del tools[t]
        else:
            tools[k] = v
    return kit_json(tools, other, made=str(len(chg)))


class Install:
    """work/ of a finished install: every real step done, made-for.txt = (old build, old kit) done, kit_used.json = the old kit"""
    def __init__(self, base, name, lst, old_kit, used=True, used_raw=None, state="done"):
        self.root = os.path.join(base, name)
        self.work = os.path.join(self.root, "work")
        self.done = os.path.join(self.work, "done")
        self.steps = [ln.split()[0] for ln in lst.splitlines() if ln.strip()]
        for s in self.steps + ["_kh2pick"]:
            w(os.path.join(self.done, s), "x")
        w(os.path.join(self.work, "cs", "sorabake", "sorabake.exe"), "OLD sorabake")      # the s224 tool copy
        installer.write_work_for(self.work, "B1 kit " + kitdrop.kit_sha(old_kit), state)
        if used:
            with open(os.path.join(self.work, kitdrop.KIT_USED), "wb") as f:
                f.write(used_raw if used_raw is not None else old_kit)
        self.kit = os.path.join(self.root, "newkit")
        os.makedirs(self.kit)

    def update(self, new_kit, lst, upd=True, list_steps=None, old_kit_json=None):
        with open(os.path.join(self.kit, "kit.json"), "wb") as f:
            f.write(new_kit)
        tag = "B2 kit " + kitdrop.kit_sha(new_kit)
        self.calls = 0

        def ls():
            self.calls += 1
            return lst
        self.logs = []
        n = installer.drop_other_release_work(self.work, tag, upd, self.logs.append, kit=self.kit, list_steps=list_steps or ls,
                                                 old_kit_json=old_kit_json)
        assert installer.read_work_for(self.work) == (tag, "running")
        with open(os.path.join(self.work, kitdrop.KIT_USED), "rb") as f:
            assert f.read() == new_kit, "kit_used.json = a byte copy of the new kit.json"
        return n

    def left(self):
        return set(os.listdir(self.done))

    def gone(self):
        return set(self.steps) - self.left()


def downstream_of(lst, seeds):
    return set(kitdrop.downstream(lst, seeds))


def test_one_tool(base, lst):
    """sorabake changed (0.2.1 -> 0.2.2 kind): cs_build + every step after it (77 of 90 today: the summons feed sounds_mine);
    fxrec_build / fxrec_record / fxrec_party, fx_trigjson, hud_build, hud_text keep theirs"""
    old = variant()
    new = variant(sorabake=("src-sorabake-2", {"sorabake.exe": "e2", "sorabake.dll": "d2"}))
    i = Install(base, "one", lst, old)
    n = i.update(new, lst)
    want = downstream_of(lst, {"cs_build"})
    assert "form_final" in want and "sora_model" in want and "fxrec_record" not in want
    assert i.gone() == want and n == len(want), (sorted(i.gone() ^ want), n)
    for s in ("fxrec_record", "fxrec_build", "fxrec_party", "hud_build", "hud_text", "fx_trigjson", "_kh2pick"):
        assert s in i.left(), s + " must keep its marker (its tool did not change)"
    assert i.calls == 1 and any("sorabake" in l and "dropped" in l for l in i.logs), i.logs
    # + new python byte-code caches (kit185 -> kit280w had 26): still only the tool's steps; a changed .py is another matter
    i = Install(base, "pyc", lst, old)
    i.update(variant(sorabake=("src-sorabake-2", {"sorabake.exe": "e2"}),
                     other={"python/Lib/site-packages/PIL/__pycache__/ImageOps.cpython-312.pyc": "new"}), lst)
    assert i.gone() == want
    i = Install(base, "py", lst, old)
    i.update(variant(other={"python/Lib/site-packages/PIL/ImageOps.py": "new"}), lst)
    assert i.left() == {"_kh2pick"}
    # a rebuild of the same sources (another OpenKH: same src hash, other dll) counts as a change too
    i = Install(base, "one_rebuild", lst, old)
    i.update(variant(partybake=("src-partybake", {"partybake.exe": "e-partybake", "partybake.dll": "OTHER"})), lst)
    assert i.gone() == want
    # hudbake only -> hud_build + the hud bins after it; cs_build stays
    i = Install(base, "hud", lst, old)
    i.update(variant(hudbake=("src-hudbake-2", {"hudbake.exe": "e2", "hudbake.dll": "d2"})), lst)
    want = downstream_of(lst, {"hud_build"})
    assert i.gone() == want and "hud_hud" in want and "cs_build" in i.left()
    # fxrec_boot: fxrec_build -> fxrec_record (+ the fx chain after it)
    i = Install(base, "fxrec", lst, old)
    i.update(variant(fxrec_boot=("src-fxrec-2", {"fxrec_boot.exe": "e2"})), lst)
    assert i.gone() == downstream_of(lst, {"fxrec_build"}) and "fxrec_record" in i.gone() and "cs_build" in i.left()
    # kh2pick: its files are sha256-checked -> nothing to drop (and "_kh2pick" stays, as in the old drop-all)
    i = Install(base, "kh2pick", lst, old)
    assert i.update(variant(kh2pick=("src-kh2pick-2", {"kh2pick.exe": "e2"})), lst) == 0
    assert not i.gone() and "_kh2pick" in i.left() and i.calls == 0
    # only "made" differs (no tool, no file): nothing dropped, the step list is not even asked
    i = Install(base, "made", lst, old)
    assert i.update(kit_json(BASE_TOOLS, BASE_OTHER, made="later"), lst) == 0 and not i.gone() and i.calls == 0
    print("ok one tool changed")


def test_runtime(base, lst):
    """the .NET runtime (or python, openkh, ffmpeg, LICENSES, the installer window...) changed: every marker, as before s291"""
    old = variant()
    for rel in ("dotnet/dotnet.exe", "python/python.exe", "openkh/OpenKh.Kh2.dll", "ffmpeg/ffmpeg.exe",
                "dotnet/shared/Microsoft.NETCore.App/8.0.0/coreclr.dll", "vgmstream/new.dll", "licenses/README.txt",
                "tools/installer_host2/x.exe"):
        i = Install(base, "rt_" + rel.replace("/", "_"), lst, old)
        new = variant(sorabake=("src-sorabake-2", {"sorabake.exe": "e2", "sorabake.dll": "d2"}), other={rel: "CHANGED"})
        i.update(new, lst)
        assert i.left() == {"_kh2pick"}, (rel, sorted(i.left())[:5])
        assert any("every step runs again" in l and "other kit files" in l for l in i.logs), i.logs
    # a tool that is gone from the new kit, or a new tool with no sb.KIT_STEPS line (kh2mot before its line is added)
    i = Install(base, "gone", lst, old)
    i.update(variant(gone=["skatebake"]), lst)
    assert i.left() == {"_kh2pick"}
    newtool = variant(kh2mot=("src-kh2mot", {"kh2mot.exe": "m1"})) if "kh2mot" not in sb.KIT_STEPS else \
        variant(zz_unknown=("src-zz", {"zz.exe": "z1"}))
    i = Install(base, "newtool", lst, old)
    i.update(newtool, lst)
    assert i.left() == {"_kh2pick"} and any("sb.KIT_STEPS" in l for l in i.logs), i.logs
    # the step list fails -> every marker
    i = Install(base, "lsfail", lst, old)

    def boom():
        raise RuntimeError("sorabuild did not start")
    i.update(variant(sorabake=("x", {"sorabake.exe": "y"})), lst, list_steps=boom)
    assert i.left() == {"_kh2pick"} and any("step list failed" in l for l in i.logs)
    print("ok runtime changed")


def test_no_kit_used(base, lst):
    """no old kit_used.json (every install made before s291) -> every marker; a kit_used.json that is not made-for.txt's kit
    (written, then the build of ANOTHER kit was cut off before made-for.txt) -> every marker; state running / install -> every"""
    old = variant()
    new = variant(sorabake=("src-sorabake-2", {"sorabake.exe": "e2"}))
    i = Install(base, "none", lst, old, used=False)
    i.update(new, lst)
    assert i.left() == {"_kh2pick"} and i.calls == 0 and any("no kit_used.json" in l for l in i.logs), i.logs
    i = Install(base, "stale", lst, old, used_raw=variant(other={"x": "y"}))
    i.update(new, lst)
    assert i.left() == {"_kh2pick"} and i.calls == 0 and any("not the kit of made-for" in l for l in i.logs), i.logs
    i = Install(base, "garbage", lst, b"not json", used_raw=b"not json")
    i.update(new, lst)
    assert i.left() == {"_kh2pick"}
    i = Install(base, "running", lst, old, state="running")
    i.update(new, lst)
    assert i.left() == {"_kh2pick"} and i.calls == 0
    i = Install(base, "install", lst, old)
    i.update(new, lst, upd=False)
    assert i.left() == {"_kh2pick"} and i.calls == 0
    # same kit (update) -> kept as before; update.invalidate decides
    i = Install(base, "samekit", lst, old)
    assert i.update(old, lst) == 0 and not i.gone() and i.calls == 0
    # without kit= (old callers): unchanged rule, and a kit_used.json left behind is removed
    i = Install(base, "nokit", lst, old)
    installer.drop_other_release_work(i.work, "B2 kit " + kitdrop.kit_sha(new), True, lambda s: None)
    assert i.left() == {"_kh2pick"} and not os.path.exists(os.path.join(i.work, kitdrop.KIT_USED))
    print("ok no old kit_used.json")


def test_s224_sorabake(base, lst):
    """s224 run B: a finished install updated to a release whose kit has a new sorabake.exe. The old copy in work/cs/sorabake must
    not be used: cs_build (the step that copies it) and form_final (which runs it) lose their marker, so sorabuild re-copies the
    tool and re-bakes - even when NO data file's reference changed (update.invalidate alone would keep everything). Then the
    whole update order: prepare -> drop -> invalidate -> kit_used.json for the next update."""
    old = variant()
    new = variant(sorabake=("src-sorabake-224", {"sorabake.exe": "e224", "sorabake.dll": "d224"}))
    rel = os.path.join(base, "rel")
    w(os.path.join(rel, "manifest.json"), json.dumps({"release": "Legend of Kairi", "version": "0.2.4-alpha",
                                                      "data": {"build_id": "B2"}}))
    dm = {"build_id": "B2", "files": {"forms/final_model.bin": {"size": 1, "sha256": "same"}}}
    w(os.path.join(rel, "tools", "sorabuild", "manifest.thispc.json"), json.dumps(dm))
    os.makedirs(os.path.join(rel, "kit"))
    with open(os.path.join(rel, "kit", "kit.json"), "wb") as f:
        f.write(new)
    for n in ("bin/savagewind.exe", "LICENSES/README.txt", "Install Legend of Kairi.cmd"):
        w(os.path.join(rel, n), "new")
    i = Install(base, "s224", lst, old)
    root = i.root
    w(os.path.join(root, "manifest.json"), json.dumps({"release": "Legend of Kairi", "version": "0.2.3-alpha",
                                                       "data": {"build_id": "B1"}}))
    w(os.path.join(root, "tools", "sorabuild", "manifest.thispc.json"), json.dumps(dict(dm, build_id="B1")))
    w(os.path.join(root, "data", "forms", "final_model.bin"), "baked by the old sorabake")
    os.makedirs(os.path.join(root, "kit"))
    with open(os.path.join(root, "kit", "kit.json"), "wb") as f:     # the old install's own kit (prepare swaps it)
        f.write(old)
    logs = []
    update.prepare(rel, root, logs.append)
    tag = installer.work_tag(rel, os.path.join(rel, "kit"))
    assert tag == "B2 kit " + kitdrop.kit_sha(new)
    installer.drop_other_release_work(i.work, tag, True, logs.append, kit=os.path.join(rel, "kit"), list_steps=lambda: lst)
    assert update.invalidate(root, lst, logs.append) == [], "no data reference changed: invalidate alone re-runs nothing"
    for s in ("cs_build", "form_final", "sora_model"):
        assert s not in i.left(), s + " must run again with the new sorabake (s224)"
    assert "fxrec_record" in i.left() and "_kh2pick" in i.left()
    # the next update (same kit as this one) sees this kit in kit_used.json
    with open(os.path.join(i.work, kitdrop.KIT_USED), "rb") as f:
        assert kitdrop.kit_sha(f.read()) == installer._kit_of(tag)
    print("ok s224 sorabake")


def test_option_backup(base, lst):
    """(s291 option 1) no kit_used.json (every install made before s291): the old kit.json in .kairi-update/bak/kit/ stands in for
    it when its sha is made-for.txt's kit. Matches -> per-tool drop; sha mismatch / no backup / unreadable -> every marker; a
    kit_used.json that IS there but is stale is never replaced by the backup"""
    old = variant()
    new = variant(sorabake=("src-sorabake-2", {"sorabake.exe": "e2"}))
    want = downstream_of(lst, {"cs_build"})

    def bak(i, raw):
        p = os.path.join(i.root, update.UPD, "bak", "kit", "kit.json")
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "wb") as f:
            f.write(raw)
        return p
    i = Install(base, "bak_ok", lst, old, used=False)
    i.update(new, lst, old_kit_json=bak(i, old))
    assert i.gone() == want and "fxrec_record" in i.left() and i.calls == 1, (sorted(i.gone() ^ want)[:5], i.logs)
    i = Install(base, "bak_bad", lst, old, used=False)
    i.update(new, lst, old_kit_json=bak(i, variant(other={"dotnet/dotnet.exe": "other"})))
    assert i.left() == {"_kh2pick"} and i.calls == 0 and any("backup is not the kit" in l for l in i.logs), i.logs
    i = Install(base, "bak_none", lst, old, used=False)
    i.update(new, lst, old_kit_json=os.path.join(i.root, update.UPD, "bak", "kit", "kit.json"))
    assert i.left() == {"_kh2pick"} and i.calls == 0 and any("no old kit.json in the update backup" in l for l in i.logs), i.logs
    i = Install(base, "bak_garbage", lst, b"{bad", used=False)       # sha matches made-for, but it is no kit.json
    i.update(new, lst, old_kit_json=bak(i, b"{bad"))
    assert i.left() == {"_kh2pick"} and i.calls == 0
    i = Install(base, "bak_stale_used", lst, old, used_raw=variant(other={"x": "y"}))
    i.update(new, lst, old_kit_json=bak(i, old))
    assert i.left() == {"_kh2pick"} and any("is not the kit of made-for" in l for l in i.logs), i.logs
    # end to end: a 0.2.3-style install (no kit_used.json) -> update.prepare moves its kit into the backup -> the drop uses it
    rel = os.path.join(base, "rel")
    w(os.path.join(rel, "manifest.json"), json.dumps({"release": "Legend of Kairi", "version": "0.2.4-alpha",
                                                      "data": {"build_id": "B2"}}))
    w(os.path.join(rel, "tools", "sorabuild", "manifest.thispc.json"), json.dumps({"build_id": "B2", "files": {}}))
    os.makedirs(os.path.join(rel, "kit"))
    with open(os.path.join(rel, "kit", "kit.json"), "wb") as f:
        f.write(new)
    for n in ("bin/savagewind.exe", "LICENSES/README.txt", "Install Legend of Kairi.cmd"):
        w(os.path.join(rel, n), "new")
    i = Install(base, "bak_e2e", lst, old, used=False)
    w(os.path.join(i.root, "manifest.json"), json.dumps({"release": "Legend of Kairi", "version": "0.2.3-alpha",
                                                         "data": {"build_id": "B1"}}))
    os.makedirs(os.path.join(i.root, "kit"))
    with open(os.path.join(i.root, "kit", "kit.json"), "wb") as f:
        f.write(old)
    update.prepare(rel, i.root, lambda s: None)
    tag = installer.work_tag(rel, os.path.join(rel, "kit"))
    logs = []
    installer.drop_other_release_work(i.work, tag, True, logs.append, kit=os.path.join(rel, "kit"), list_steps=lambda: lst,
                                      old_kit_json=os.path.join(i.root, update.UPD, "bak", "kit", "kit.json"))
    assert i.gone() == want, (sorted(i.gone() ^ want)[:5], logs)
    print("ok option 1: the old kit.json from the update backup")


def test_option_no_build_effect(base, lst):
    """(s291 option 2) the installer window (tools/installer_host/, dotnet/shared/Microsoft.WindowsDesktop.App/) and LICENSES/
    never count; anything else outside the tools still drops every marker (test_runtime)"""
    old = variant(other={"dotnet/shared/Microsoft.WindowsDesktop.App/8.0.0/System.Windows.Forms.dll": "wf1"})
    host = {"tools/installer_host/LegendOfKairiSetup.exe": "host2", "tools/installer_host/new.dll": "n",
            "LICENSES/README.txt": "lic2", "LICENSES/NEW-LICENSE.txt": "x",
            "dotnet/shared/Microsoft.WindowsDesktop.App/8.0.0/System.Windows.Forms.dll": "wf2"}
    i = Install(base, "host_only", lst, old)
    assert i.update(variant(other=host), lst) == 0 and not i.gone() and i.calls == 0, i.logs
    i = Install(base, "host_tool", lst, old)
    i.update(variant(sorabake=("src-sorabake-2", {"sorabake.exe": "e2"}), other=host), lst)
    assert i.gone() == downstream_of(lst, {"cs_build"}), i.logs
    i = Install(base, "host_gone", lst, old)       # the window's files removed from the kit: no build effect either
    nk = json.loads(variant().decode())
    nk["files"] = {k: v for k, v in nk["files"].items() if not k.startswith("tools/installer_host/")}
    assert i.update(json.dumps(nk).encode(), lst) == 0 and not i.gone()
    assert kitdrop.NO_BUILD_EFFECT == ("tools/installer_host/", "LICENSES/", "dotnet/shared/Microsoft.WindowsDesktop.App/")
    print("ok option 2: installer window + LICENSES have no build effect")


def _tools_in_call(call, fn, mod):
    """the kit tool names a take_kit_tool / kit_tool call can pass: a string, or a loop variable over a module-level list"""
    a = call.args[1] if len(call.args) > 1 else None
    if isinstance(a, ast.Constant) and isinstance(a.value, str):
        return [a.value]
    if isinstance(a, ast.Name):
        for node in ast.walk(fn):
            if isinstance(node, ast.For) and isinstance(node.target, ast.Name) and node.target.id == a.id:
                it = node.iter
                if isinstance(it, ast.Name):
                    for top in mod.body:
                        if isinstance(top, ast.Assign) and any(isinstance(t, ast.Name) and t.id == it.id for t in top.targets):
                            return [e.value for e in top.value.elts]
                if isinstance(it, (ast.List, ast.Tuple)):
                    return [e.value for e in it.elts]
    return None


def test_kit_steps_complete(base, lst):
    """sb.KIT_STEPS: one line per sb.KIT_SRC tool, every step in it exists, and every step function that calls take_kit_tool /
    kit_tool with a tool is listed for that tool (a new kit tool such as kh2mot cannot be forgotten)"""
    assert set(sb.KIT_STEPS) == set(sb.KIT_SRC), "sb.KIT_STEPS needs a line for: %s" % sorted(set(sb.KIT_SRC) ^ set(sb.KIT_STEPS))
    names = set(kitdrop.parse_deps(lst))
    for t, ss in sb.KIT_STEPS.items():
        for s in ss:
            assert s in names or s.startswith("_"), "sb.KIT_STEPS[%s]: no step %s" % (t, s)
    found = []
    for f in sorted(glob.glob(os.path.join(SBDIR, "steps", "*.py"))):
        mod = ast.parse(open(f, encoding="utf-8").read())
        fn_step = {}      # function name -> step names (Step("name", fn, ...))
        for node in ast.walk(mod):
            if isinstance(node, ast.Call) and getattr(node.func, "id", getattr(node.func, "attr", "")) == "Step" and \
                    len(node.args) > 1 and isinstance(node.args[0], ast.Constant) and isinstance(node.args[1], ast.Name):
                fn_step.setdefault(node.args[1].id, []).append(node.args[0].value)
        for fn in [n for n in ast.walk(mod) if isinstance(n, ast.FunctionDef)]:
            for c in ast.walk(fn):
                if isinstance(c, ast.Call) and getattr(c.func, "attr", getattr(c.func, "id", "")) in ("take_kit_tool", "kit_tool"):
                    tools = _tools_in_call(c, fn, mod)
                    where = "%s:%s" % (os.path.basename(f), fn.name)
                    assert tools, "%s: cannot tell which kit tool it takes: list its step in sb.KIT_STEPS and teach this test" % where
                    steps = fn_step.get(fn.name)
                    assert steps, "%s takes a kit tool but no Step(\"...\", %s) names it: list its step in sb.KIT_STEPS" % (where, fn.name)
                    for t in tools:
                        for s in steps:
                            assert s in sb.KIT_STEPS.get(t, []), "sb.KIT_STEPS[%r] must list step %s (%s)" % (t, s, where)
                            found.append((t, s))
    assert ("sorabake", "cs_build") in found and ("hudbake", "hud_build") in found and ("trigjson", "fx_trigjson") in found \
        and ("fxrec_boot", "fxrec_build") in found, found
    print("ok sb.KIT_STEPS complete (%d tool uses)" % len(found))


def test_real_kits(base, lst):
    """(when they exist on this PC) real kit.json pairs: which tools differ, and the answer is never a crash"""
    kits = [p for p in (os.environ.get("LOK_TEST_KITS") or "").split(";") if p and os.path.isfile(p)]
    for a, b in zip(kits, kits[1:]):
        ra, rb = open(a, "rb").read(), open(b, "rb").read()
        tools, other = kitdrop.changed_tools(kitdrop.load_kit(ra), kitdrop.load_kit(rb))
        m, why = kitdrop.plan(kitdrop.kit_sha(ra), ra, rb, sb.KIT_STEPS, lambda: lst)
        print("   %s -> %s: tools %s, other files %d, drop %s (%s)" % (os.path.basename(os.path.dirname(a)),
              os.path.basename(os.path.dirname(b)), tools, len(other), "ALL" if m is None else len(m), why))
    print("ok real kits (%d)" % len(kits))


def main():
    with tempfile.TemporaryDirectory(dir=TMP) as d:
        lst = real_list(d)
        for t in (test_one_tool, test_runtime, test_no_kit_used, test_s224_sorabake, test_option_backup, test_option_no_build_effect,
                  test_kit_steps_complete, test_real_kits):
            sub = os.path.join(d, t.__name__)
            os.makedirs(sub)
            t(sub, lst)
    print("ok")


main()

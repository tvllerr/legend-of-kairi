"""Host test for the sorabuild step "nethorse" (s292, steps/nethorse.py): the player build makes the horse puppets.

  python -E -B tools/sorabuild/nethorse/test_nethorse_step.py [--romfs <BotW romfs> --tmp <scratch dir> [--lab <data dir>]]
Always (no game bytes): nethorse/targets.txt = the step's outputs (53, all inside overlay/); OFF (default config): no step,
not in --list, no file in targets.txt or either manifest (their build ids = verify.py's formula); ON (config nethorse true):
the step, the other steps unchanged, the ResourceSizeTable / ActorInfo have one writer (or later writers depend on nethorse),
no other step writes a puppet, and an update to a turned-on release re-runs only this step (installer/update.py owners).
If targets.txt lists them (turned on): every one there and in both manifests.
With --romfs (read only) + --tmp (a scratch folder, emptied): the step on a scratch install: its files = the bake manifest's
(and the shipped one's when turned on), "Turning it on" step 3 (verify.py --merge) on manifest copies: old entries kept, 53
added, new build id, verify --expect ALL SAME, a bad reference refused; a second run gives the same bytes, a stale OnlineHorse_* pack goes, a foreign ActorInfo / RSTB in the
overlay and a lab install record (nethorse.s289.json) are refused with nothing written, a romfs that bakes another set is
refused. With --lab: every file of the lab's install record (nethorse.s289.json "added") byte-identical to the step's.
"""
import argparse
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SB = os.path.dirname(HERE)
sys.path.insert(0, SB)
sys.path.insert(0, os.path.join(SB, "..", "installer"))
import sb  # noqa: E402
import update  # noqa: E402

FAILS = []


def check(cond, what):
    print("%s  %s" % ("ok  " if cond else "FAIL", what))
    if not cond:
        FAILS.append(what)


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


NH = load("sbsteps_nethorse_t", os.path.join(SB, "steps", "nethorse.py"))
SBMAIN = load("sorabuild_t", os.path.join(SB, "sorabuild.py"))


def sha(p):
    with open(p, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def build_id(files):
    lines = "".join("%s %d %s\n" % (r, files[r]["size"], files[r]["sha256"]) for r in sorted(files))
    return hashlib.sha256(lines.encode()).hexdigest()[:16]


def list_text(tmp, on):
    """sorabuild --list with a config that has "nethorse": on"""
    cfg = os.path.join(tmp, "list_cfg_%d.json" % on)
    os.makedirs(tmp, exist_ok=True)
    c = {"out": os.path.join(tmp, "list_out"), "work": os.path.join(tmp, "list_work")}
    if on:
        c["nethorse"] = True
    with open(cfg, "w", encoding="utf-8") as f:
        json.dump(c, f)
    r = subprocess.run([sys.executable, "-E", "-P", "-B", os.path.join(SB, "sorabuild.py"), "--config", cfg, "--list"],
                       capture_output=True, text=True)
    check(r.returncode == 0, "sorabuild --list runs (nethorse %s)" % ("on" if on else "off"))
    return r.stdout


def static_checks(tmp):
    outs = NH.outputs()
    check(len(outs) == 53, "nethorse/targets.txt: 53 files (%d)" % len(outs))
    check(all(o.startswith("overlay/") for o in outs), "every nethorse target is inside overlay/")
    check(sum(o.startswith("overlay/Actor/Pack/OnlineHorse_") for o in outs) == 50, "50 puppet packs")
    check(set(NH.TABLES) | {"overlay/horse.txt"} <= set(outs), "ActorInfo, ResourceSizeTable and horse.txt are targets")
    # OFF (the default): no step, no target, no manifest entry
    off = SBMAIN.load_steps({})
    check("nethorse" not in off, "default config: no step nethorse")
    check("nethorse" not in SBMAIN.load_steps({"nethorse": False}), "config nethorse false: no step nethorse")
    lt_off = list_text(tmp, False)
    check(not any(ln.startswith("nethorse ") for ln in lt_off.splitlines()), "  not in sorabuild --list")
    main_t = [rel for rel, grp in targets(os.path.join(SB, "targets.txt"))]
    turned_on = any(r in main_t for r in outs)
    mans = {m: json.load(open(os.path.join(SB, m), encoding="utf-8")) for m in ("manifest.thispc.json", "manifest.deck.json")}
    for m, man in mans.items():
        check(build_id(man["files"]) == man["build_id"], "%s build id = verify.py's formula (%s)" % (m, man["build_id"]))
    if not turned_on:
        check(not any(o in main_t for o in outs), "targets.txt: no nethorse file (off)")
        for m, man in mans.items():
            check(not any(k in outs or k.startswith("overlay/Actor/Pack/OnlineHorse_") for k in man["files"]),
                  "%s: no nethorse file (off)" % m)
    else:   # (turned on later: all 3 parts of "Turning it on" together)
        check(all(o in main_t for o in outs), "turned on: targets.txt lists every nethorse target")
        for m, man in mans.items():
            check(all(o in man["files"] for o in outs), "turned on: %s lists every nethorse target" % m)
    # ON: the step, its outputs, the one-writer rule, the update owner
    steps = SBMAIN.load_steps({"nethorse": True})
    check("nethorse" in steps, "config nethorse true: step nethorse")
    if "nethorse" not in steps:
        return
    check(steps["nethorse"].outputs == outs, "  its outputs = nethorse/targets.txt")
    check(steps["nethorse"].deps == [], "  it needs no other step (romfs only)")
    check(set(steps) - {"nethorse"} == set(off), "  every other step the same as with it off")
    users = {}
    for n, s in steps.items():
        for t in NH.TABLES:
            if t in s.outputs:
                users.setdefault(t, []).append(n)

    def after(n, target, seen=None):
        seen = seen or set()
        for d in steps[n].deps:
            if d == target or (d not in seen and (seen.add(d) or after(d, target, seen))):
                return True
        return False
    for t in NH.TABLES:
        w = users.get(t, [])
        check("nethorse" in w, "%s: written by step nethorse" % t)
        check(all(n == "nethorse" or after(n, "nethorse") for n in w),
              "%s: every other writer depends on nethorse (%s)" % (t, ", ".join(w)))
    clash = [n for n, s in steps.items() if n != "nethorse" and any(
        o in outs and o not in NH.TABLES or o.startswith("overlay/Actor/Pack/OnlineHorse_") or o == "overlay/horse.txt"
        for o in s.outputs)]
    check(not clash, "no other step writes a puppet / horse.txt (%s)" % (", ".join(clash) or "none"))
    # an update from a release without the puppets to one with them on: the 53 new files are owned by nethorse alone
    lt_on = list_text(tmp, True)
    check(lt_on.splitlines() and set(lt_on.splitlines()) - set(lt_off.splitlines()) ==
          {ln for ln in lt_on.splitlines() if ln.startswith("nethorse ")}, "  --list on = off + the nethorse line")
    own = update.owners(lt_on)
    old = mans["manifest.thispc.json"]
    new = {"files": dict(old["files"], **{o: {"size": 1, "sha256": "0" * 64} for o in outs})}
    ch = update.changed_files(old, new)
    hit = {own.get(f) or next((s for o, s in own.items() if f.startswith(o.rstrip("/") + "/")), None) for f in ch}
    check(sorted(ch) == sorted(outs) and hit == {"nethorse"}, "update to a release with it on: only step nethorse re-runs (%s)" % hit)
    check(update.work_inputs(lt_on, {"nethorse"}) == [], "  and no work-only step before it")


def targets(path):
    out = []
    with open(path, encoding="utf-8") as f:
        for ln in f:
            ln = ln.split("#", 1)[0].strip()
            if ln:
                out.append(tuple(ln.split()))
    return out


def merge_checks(tmp, data):
    """'Turning it on' step 3 on COPIES of both manifests: verify.py --merge with the step's files as the reference"""
    for m in ("manifest.thispc.json", "manifest.deck.json"):
        cp = os.path.join(tmp, m)
        shutil.copyfile(os.path.join(SB, m), cp)
        r = subprocess.run([sys.executable, "-E", "-P", "-B", os.path.join(SB, "verify.py"), "--out", data, "--ref", data,
                            "--targets", NH.TARGETS, "--group", "nethorse", "--manifest", cp, "--merge"],
                           capture_output=True, text=True)
        old = json.load(open(os.path.join(SB, m), encoding="utf-8"))
        new = json.load(open(cp, encoding="utf-8"))
        outs = NH.outputs()
        check(r.returncode == 0 and "manifest merged" in r.stdout, "verify.py --merge into a copy of %s" % m)
        check(all(new["files"].get(k) == v for k, v in old["files"].items()) and set(new["files"]) == set(old["files"]) | set(outs),
              "  every old entry kept, the 53 added")
        check(build_id(new["files"]) == new["build_id"] != old["build_id"], "  a new build id by the formula (%s)" % new["build_id"])
        # the player's check of a turned-on build: verify --expect over the nethorse targets
        r2 = subprocess.run([sys.executable, "-E", "-P", "-B", os.path.join(SB, "verify.py"), "--out", data, "--expect", cp,
                             "--targets", NH.TARGETS], capture_output=True, text=True)
        check(r2.returncode == 0 and "ALL SAME" in r2.stdout, "  verify --expect of the step's files against it: ALL SAME")
        bad = subprocess.run([sys.executable, "-E", "-P", "-B", os.path.join(SB, "verify.py"), "--out", data, "--ref",
                              os.path.join(tmp, "nowhere"), "--targets", NH.TARGETS, "--group", "nethorse", "--manifest", cp,
                              "--merge"], capture_output=True, text=True)
        check(bad.returncode != 0 and json.load(open(cp, encoding="utf-8")) == new, "  --merge against a reference without them: refused, manifest unchanged")


class Ctx(sb.Ctx):
    def __init__(self, romfs, out, work):
        super().__init__({"romfs": romfs, "out": out, "work": work})
        self.step = "nethorse"


def run_step(romfs, out, work, outs=None):
    """-> None (ok) or the SystemExit text"""
    try:
        NH._run(Ctx(romfs, out, work), outs)
        return None
    except SystemExit as e:
        return str(e)


def tree(d):
    got = {}
    for dp, dn, fn in os.walk(d):
        for n in fn:
            p = os.path.join(dp, n)
            got[os.path.relpath(p, d).replace("\\", "/")] = sha(p)
    return got


def step_checks(romfs, tmp, lab):
    outs = NH.outputs()
    root = os.path.join(tmp, "inst")
    shutil.rmtree(root, ignore_errors=True)
    out, work = os.path.join(root, "data"), os.path.join(root, "work")
    os.makedirs(os.path.join(out, "overlay", "Pack"))
    with open(os.path.join(out, "overlay", "Pack", "Title.pack"), "wb") as f:
        f.write(b"logo step stand-in")   # (another step's overlay file: must stay)
    err = run_step(romfs, out, work)
    check(err is None, "step runs on a scratch install (%s)" % err)
    t1 = tree(out)
    bm = json.load(open(os.path.join(work, "nethorse", "bake", "horse_manifest.json"), encoding="utf-8"))["files"]
    man = {"overlay/" + r: {"sha256": v} for r, v in bm.items()}
    check(all(o in t1 and t1[o] == man[o]["sha256"] for o in outs), "every file = the bake manifest's sha256")
    ship = json.load(open(os.path.join(SB, "manifest.thispc.json"), encoding="utf-8"))["files"]
    if any(o in ship for o in outs):   # (turned on: the shipped manifest's bytes)
        check(all(ship.get(o, {}).get("sha256") == t1.get(o) for o in outs), "every file = manifest.thispc.json's sha256")
    check(set(t1) == set(outs) | {"overlay/Pack/Title.pack"}, "nothing else written into data/ (%d files)" % len(t1))
    check(t1["overlay/Pack/Title.pack"] == hashlib.sha256(b"logo step stand-in").hexdigest(), "the logo's Title.pack untouched")
    if lab:
        rec = json.load(open(os.path.join(lab, "nethorse.s289.json"), encoding="utf-8"))["added"]
        same = [r for r, s in rec.items() if t1.get(r) == s and os.path.isfile(os.path.join(lab, *r.split("/"))) and
                sha(os.path.join(lab, *r.split("/"))) == s]
        check(len(same) == len(rec) == len(outs) and set(rec) == set(outs),
              "lab data folder: %d of %d files of its install record byte-identical" % (len(same), len(rec)))
    merge_checks(tmp, out)
    # idempotent + stale puppet
    stale = os.path.join(out, "overlay", "Actor", "Pack", "OnlineHorse_Old.sbactorpack")
    with open(stale, "wb") as f:
        f.write(b"old")
    other = os.path.join(out, "overlay", "Actor", "Pack", "SomethingElse.sbactorpack")
    with open(other, "wb") as f:
        f.write(b"not ours")
    err = run_step(romfs, out, work)
    t2 = tree(out)
    check(err is None and all(t2[o] == t1[o] for o in outs), "a second run: the same bytes")
    check(not os.path.exists(stale), "  an OnlineHorse_* pack of an older bake is removed")
    check(os.path.exists(other), "  another pack in that folder is left alone")
    os.remove(other)
    # foreign table: refused, nothing written
    rp = os.path.join(out, *NH.TABLES[1].split("/"))
    with open(rp, "wb") as f:
        f.write(b"someone else's RSTB")
    os.remove(os.path.join(out, "overlay", "horse.txt"))
    err = run_step(romfs, out, work)
    check(err is not None and "not written by this step" in err, "a foreign ResourceSizeTable in the overlay: refused")
    check(not os.path.exists(os.path.join(out, "overlay", "horse.txt")) and open(rp, "rb").read() == b"someone else's RSTB",
          "  nothing written, the foreign table kept")
    os.remove(rp)
    # our own older table (in written.json) is replaced
    with open(rp, "wb") as f:
        f.write(b"our old RSTB")
    w = json.load(open(os.path.join(work, NH.WRITTEN), encoding="utf-8"))
    w[NH.TABLES[1]] = hashlib.sha256(b"our old RSTB").hexdigest()
    with open(os.path.join(work, NH.WRITTEN), "w", encoding="utf-8") as f:
        json.dump(w, f)
    err = run_step(romfs, out, work)
    check(err is None and sha(rp) == man[NH.TABLES[1]]["sha256"], "a table this step wrote before is replaced")
    # lab record
    with open(os.path.join(out, NH.LAB_RECORD), "w") as f:
        f.write("{}")
    os.remove(os.path.join(out, "overlay", "horse.txt"))
    err = run_step(romfs, out, work)
    check(err is not None and "horse_install.py --undo" in err and not os.path.exists(os.path.join(out, "overlay", "horse.txt")),
          "a lab install record in data/: refused, nothing written")
    os.remove(os.path.join(out, NH.LAB_RECORD))
    # a romfs that bakes another set
    err = run_step(romfs, out, work, outs=outs[:-1])
    check(err is not None and "other puppets" in err and not os.path.exists(os.path.join(out, "overlay", "horse.txt")),
          "another set of puppets than targets.txt: refused, nothing written")
    err = run_step(romfs, out, work)
    check(err is None and tree(out) == t1, "and the next good run puts everything back")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--romfs")
    ap.add_argument("--tmp")
    ap.add_argument("--lab")
    a = ap.parse_args()
    tmp = a.tmp or os.path.join(os.environ.get("TMP", "."), "test_nethorse_step")
    static_checks(tmp)
    if a.romfs:
        if not a.tmp:
            raise SystemExit("--romfs needs --tmp (a scratch folder)")
        step_checks(a.romfs, a.tmp, a.lab)
    print("\n%d FAILED" % len(FAILS) if FAILS else "\nall ok")
    sys.exit(1 if FAILS else 0)


if __name__ == "__main__":
    main()

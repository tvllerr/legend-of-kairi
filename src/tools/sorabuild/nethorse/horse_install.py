"""nethorse (s289 objs part 4): put horse_bake.py's puppets into a mod data folder's RomFS overlay, or take them out again.
For the main session's dev / lab data folders (D:/SoraBotW/data, the lab's managed game data); NOT a step of the Kairi
installer (the proposed hook is in the s289 objs report). After Savage Wind PR #294 "Online mode" by byluki, MIT.

  python -E -P -B horse_install.py <bake dir> <data dir> <romfs dir>          install into <data dir>/overlay
  python -E -P -B horse_install.py --check <bake dir> <data dir> <romfs dir>  every check, then what it would do; no change
  python -E -P -B horse_install.py --undo <data dir>                          take out what the install put there

Install refuses (and changes nothing) when:
  - savagewind.exe runs (any copy: the game reads the overlay at start and while it loads), or that cannot be checked;
  - <data dir>/overlay is missing; an install record <data dir>/nethorse.s289.json is there already (--undo first);
  - a baked file's sha256 is not its horse_manifest.json one;
  - the romfs' Actor/ActorInfo.product.sbyml / RSTB are not the ones the bake was made from (the overlay's copies replace
    the game's own: they must come from the romfs the game reads; bake again from that romfs);
  - any file it would put there exists already (the expected live state is "absent": today's overlay holds
    Pack/Title.pack only). So no live file is ever replaced and no <file>.pre-s289 backup is needed; the record is the undo.
It writes the record first (every target + its sha256, the folders it makes), then each file (a temp file, its sha256
checked, then renamed into place), then marks the record "installed".
Undo refuses while savagewind.exe runs and when a file it put there was changed since (sha256): then nothing is removed.
Else it removes those files, the folders it made when empty, and the record. A half-done install is undone the same way.
"""
import hashlib
import json
import os
import subprocess
import sys

MANIFEST = "horse_manifest.json"
RECORD = "nethorse.s289.json"
INFO = "Actor/ActorInfo.product.sbyml"
RSTB = "System/Resource/ResourceSizeTable.product.srsizetable"
SKIP = ("horse_list.txt", MANIFEST)  # (the bake's own notes: not overlay files)


def sha_file(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def game_running():
    """-> (running, why); a check that cannot run counts as running (refused)"""
    try:
        if os.name == "nt":
            r = subprocess.run(["tasklist", "/FI", "IMAGENAME eq savagewind.exe", "/NH", "/FO", "CSV"],
                               capture_output=True, text=True, timeout=30)
            if r.returncode != 0:
                return True, "tasklist failed (%d): cannot tell whether the game runs" % r.returncode
            return "savagewind.exe" in r.stdout.lower(), "tasklist"
        for pid in os.listdir("/proc"):
            if pid.isdigit():
                try:
                    with open("/proc/%s/cmdline" % pid, "rb") as f:
                        if b"savagewind" in f.read().lower():
                            return True, "/proc"
                except OSError:
                    pass
        return False, "/proc"
    except Exception as e:  # (no tasklist / no /proc: refuse)
        return True, "cannot tell whether the game runs (%s: %s)" % (type(e).__name__, e)


def safe_rel(rel):
    parts = rel.split("/")
    return rel and not rel.startswith("/") and ":" not in rel and "\\" not in rel and all(p not in ("", ".", "..") for p in parts)


def refuse(msg):
    raise SystemExit("horse_install: REFUSED: %s\n(nothing changed)" % msg)


def check_game():
    run, why = game_running()
    if run:
        refuse("savagewind.exe runs (%s): close every copy of the game first" % why)


def plan(bake, data, romfs):
    """every install check; -> (targets {rel to data: (src, sha)}, dirs to make (rel to data, top first), manifest sha)"""
    ov = os.path.join(data, "overlay")
    if not os.path.isdir(ov):
        refuse("%s is missing (the logo step makes data/overlay/Pack/Title.pack; the launchers turn the overlay on only "
               "with it)" % ov)
    if os.path.exists(os.path.join(data, RECORD)):
        refuse("%s is there: installed already (run --undo first, then install again)" % os.path.join(data, RECORD))
    mp = os.path.join(bake, MANIFEST)
    if not os.path.isfile(mp):
        refuse("%s: no %s (not a horse_bake.py folder)" % (bake, MANIFEST))
    with open(mp, "rb") as f:
        mraw = f.read()
    man = json.loads(mraw.decode("utf-8"))
    files, inputs = man.get("files") or {}, man.get("romfs_inputs") or {}
    if set(inputs) != {INFO, RSTB} or "horse.txt" not in files or INFO not in files or RSTB not in files:
        refuse("%s does not list the romfs inputs / horse.txt / ActorInfo / RSTB" % mp)
    bad = []
    for rel, want in sorted(inputs.items()):
        p = os.path.join(romfs, *rel.split("/"))
        got = sha_file(p) if os.path.isfile(p) else "missing"
        if got != want:
            bad.append("%s: %s, the bake's %s" % (rel, got[:16], want[:16]))
    if bad:
        refuse("the romfs %s is not the one the bake was made from (bake again from the romfs the game reads):\n  %s" % (
            romfs, "\n  ".join(bad)))
    targets, bad, there = {}, [], []
    for rel, want in sorted(files.items()):
        if rel in SKIP:
            continue
        if not safe_rel(rel):
            refuse("%s lists %r" % (mp, rel))
        src = os.path.join(bake, *rel.split("/"))
        got = sha_file(src) if os.path.isfile(src) else "missing"
        if got != want:
            bad.append("%s: %s, the manifest's %s" % (rel, got[:16], want[:16]))
        dst_rel = "overlay/" + rel
        if os.path.lexists(os.path.join(data, *dst_rel.split("/"))):
            dst = os.path.join(data, *dst_rel.split("/"))
            same = os.path.isfile(dst) and sha_file(dst) == want
            there.append("%s (%s)" % (dst_rel, "the same bytes, but not by an install record of ours" if same else
                                      "another file: sha256 %s" % (sha_file(dst)[:16] if os.path.isfile(dst) else "a folder")))
        targets[dst_rel] = (src, want)
    if bad:
        refuse("baked files not as the manifest says (bake again):\n  " + "\n  ".join(bad))
    if there:
        refuse("files it would add are there already (expected: absent; nothing is replaced):\n  " + "\n  ".join(there))
    dirs = []
    for rel in targets:
        parts = rel.split("/")[:-1]
        for i in range(1, len(parts) + 1):
            d = "/".join(parts[:i])
            if d not in dirs and not os.path.isdir(os.path.join(data, *parts[:i])):
                dirs.append(d)
    return targets, dirs, hashlib.sha256(mraw).hexdigest()


def write_record(data, rec):
    p = os.path.join(data, RECORD)
    tmp = p + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        json.dump(rec, f, indent=1, sort_keys=True)
        f.write("\n")
    os.replace(tmp, p)


def install(bake, data, romfs, dry):
    check_game()
    targets, dirs, msha = plan(bake, data, romfs)
    ov = os.path.join(data, "overlay")
    if not os.path.isfile(os.path.join(ov, "Pack", "Title.pack")):
        print("horse_install: NOTE: no overlay/Pack/Title.pack: the Kairi / Melty launchers turn the overlay on only with it"
              " (the dev launcher always does): without the overlay the DLL says so and makes no puppets")
    if dry:
        print("horse_install: --check: all checks pass; it would add %d files under %s (folders made: %s); nothing changed" % (
            len(targets), ov, ", ".join(dirs) or "none"))
        return
    rec = {"what": "s289 objs nethorse puppets (tools/sorabuild/nethorse/horse_install.py); --undo takes them out",
           "state": "installing", "bake": os.path.abspath(bake), "bake_manifest_sha256": msha,
           "romfs": os.path.abspath(romfs), "added": {rel: sha for rel, (src, sha) in sorted(targets.items())}, "dirs_made": dirs}
    write_record(data, rec)
    for d in dirs:
        os.makedirs(os.path.join(data, *d.split("/")), exist_ok=True)
    for rel, (src, sha) in sorted(targets.items()):
        dst = os.path.join(data, *rel.split("/"))
        tmp = dst + ".s289tmp"
        with open(src, "rb") as f, open(tmp, "wb") as g:
            g.write(f.read())
        if sha_file(tmp) != sha:
            os.remove(tmp)
            raise SystemExit("horse_install: FAILED: %s copied wrong (sha256): stopped half way; run --undo" % rel)
        os.replace(tmp, dst)
    rec["state"] = "installed"
    write_record(data, rec)
    print("horse_install: added %d files under %s (record %s); undo: horse_install.py --undo %s" % (
        len(targets), ov, os.path.join(data, RECORD), data))


def undo(data, dry):
    check_game()
    p = os.path.join(data, RECORD)
    if not os.path.isfile(p):
        refuse("no %s: nothing was installed here by horse_install.py" % p)
    with open(p, "r", encoding="utf-8") as f:
        rec = json.load(f)
    added, dirs = rec.get("added") or {}, rec.get("dirs_made") or []
    changed, gone, keep = [], [], []
    for rel, sha in sorted(added.items()):
        if not safe_rel(rel) or not rel.startswith("overlay/"):
            refuse("%s lists %r" % (p, rel))
        dst = os.path.join(data, *rel.split("/"))
        if not os.path.lexists(dst):
            gone.append(rel)
        elif not os.path.isfile(dst) or sha_file(dst) != sha:
            changed.append(rel)
        else:
            keep.append(rel)
        if os.path.exists(dst + ".s289tmp"):
            keep.append(rel + ".s289tmp")
    if changed:
        refuse("changed since the install (sha256), left alone:\n  " + "\n  ".join(changed))
    if dry:
        print("horse_install: --check --undo: it would remove %d files (%d already gone) and the folders %s when empty" % (
            len(keep), len(gone), ", ".join(dirs) or "none"))
        return
    for rel in keep:
        os.remove(os.path.join(data, *rel.split("/")))
    left = []
    for d in sorted(dirs, key=lambda x: -x.count("/")):
        q = os.path.join(data, *d.split("/"))
        if os.path.isdir(q):
            if os.listdir(q):
                left.append(d)
            else:
                os.rmdir(q)
    os.remove(p)
    print("horse_install: undone: %d files removed (%d were gone already)%s; record removed" % (
        len([k for k in keep if not k.endswith(".s289tmp")]), len(gone),
        "; folders kept (not empty): " + ", ".join(left) if left else ""))


def main(argv):
    flags = {a for a in argv if a.startswith("--")}
    args = [a for a in argv if not a.startswith("--")]
    if flags - {"--undo", "--check"}:
        raise SystemExit(__doc__)
    if "--undo" in flags:
        if len(args) != 1:
            raise SystemExit(__doc__)
        undo(os.path.abspath(args[0]), "--check" in flags)
        return
    if len(args) != 3:
        raise SystemExit(__doc__)
    install(*(os.path.abspath(a) for a in args), dry="--check" in flags)


if __name__ == "__main__":
    main(sys.argv[1:])

"""nethorse puppets (s292): the files the DLL's nethorse needs (runtime/sora_net_horse.h: a friend's horse under his Sora in a
Melty room), built into the install's RomFS overlay folder data/overlay from the player's OWN BotW romfs. Until s292 only the
lab data folder had them (tools/sorabuild/nethorse/horse_install.py + the s291 horsefix / coat installs on top, by hand).

Step "nethorse": tools/sorabuild/nethorse/horse_bake.py (the CURRENT bake: sorted AS lists + the Coat_* AS of s291) into
work/nethorse/bake, then every overlay file of the bake into data/overlay/<same path>:
  overlay/Actor/Pack/OnlineHorse_<actor>.sbactorpack    50 puppets (31 horses, 19 saddles / reins) of the 1.9.0 romfs
  overlay/Actor/ActorInfo.product.sbyml                 the romfs' own + one entry per puppet
  overlay/System/Resource/ResourceSizeTable.product.srsizetable   the romfs' own + the puppets' rows
  overlay/horse.txt                                     the DLL's actor -> puppet map
The list of files = nethorse/targets.txt (the step's outputs; update.invalidate finds their owner through them); a romfs that
bakes any other set fails the step. Pure python (the kit python's oead 1.3.0): no kit tool, so no KIT_STEPS line. ~2 s, 1.6 MB.

OPT-IN (s292, main session: GitHub releases are single player today, so the puppets would do nothing there; the default PC /
Deck targets + manifests stay byte-identical, build ids da4052a4c2b91274 / 0f7a640e91b04ca9). Without config "nethorse": true
the step does not exist (not in --list, not run by `all`). Turning it on for the multiplayer build, in ONE commit:
  1. the build config gets "nethorse": true: tools/installer/installer.py config() (Windows) and tools/installer/linux/install.sh
     (the config.json it writes, Deck), and any other config that builds a package's data (the Melty build);
  2. the lines of nethorse/targets.txt are appended to targets.txt (the player's verify then checks them);
  3. both manifests take the 53 files (a build of this step from the 1.9.0 romfs, checked against a reference data folder):
       python -E -P verify.py --out <built data> --ref <reference data> --targets nethorse/targets.txt --group nethorse \
           --manifest manifest.thispc.json --merge          (and the same with manifest.deck.json, after a Deck build check)
  test_nethorse_step.py checks both states (off: nothing anywhere; on: everything consistent).

The two tables (ActorInfo, ResourceSizeTable) REPLACE the game's own when the overlay is on (runtime/services/fs.c; the exe's
rstb_patch then grows the voice / HUD / model rows on top of the overlay's copy, fs.c from_overlay). This step is their ONLY
writer and always starts from the ROMFS copies (byte-identical output). A later step that adds rows of its own must list
deps=["nethorse"] and merge onto data/overlay's copy (a re-run of this step drops its marker, so it merges again);
test_nethorse_step.py fails when a second writer has no such dep. A copy there that this step did not write (not its new
bytes, not in work/nethorse/written.json) is refused, never overwritten: another tool's table.

The lab's install record (data/nethorse.s289.json of horse_install.py) is refused too: horse_install.py --undo first.
"""
import hashlib
import json
import os
import shutil

from sb import Step

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # tools/sorabuild
GROUP = "nethorse"
OV = "overlay/"
TABLES = ("overlay/Actor/ActorInfo.product.sbyml", "overlay/System/Resource/ResourceSizeTable.product.srsizetable")
PUPPET_DIR = "overlay/Actor/Pack"
PUPPET_PREFIX = "OnlineHorse_"
LAB_RECORD = "nethorse.s289.json"      # (horse_install.py RECORD, in the data dir)
WRITTEN = "nethorse/written.json"     # (in work/: what this step put into data/overlay last time, rel -> sha256)
BAKE_SKIP = ("horse_list.txt", "horse_manifest.json")   # (the bake's notes: horse_install.py SKIP; kept in work/)


TARGETS = os.path.join(HERE, "nethorse", "targets.txt")


def outputs(targets=None):
    """the overlay files of group nethorse in nethorse/targets.txt (relative to data/)"""
    out = []
    with open(targets or TARGETS, encoding="utf-8") as f:
        for ln in f:
            ln = ln.split("#", 1)[0].strip()
            if ln:
                rel, grp = ln.split()
                if grp == GROUP:
                    out.append(rel)
    return out


def _sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def _read_written(ctx):
    try:
        with open(os.path.join(ctx.work, WRITTEN), encoding="utf-8") as f:
            w = json.load(f)
        return w if isinstance(w, dict) else {}
    except (OSError, ValueError):
        return {}


def _run(ctx, outs=None):
    outs = outs or outputs()
    if not outs:
        raise SystemExit("[sorabuild] nethorse: nethorse/targets.txt lists no file of group nethorse")
    for rel in outs:
        if not rel.startswith(OV) or ".." in rel.split("/"):
            raise SystemExit("[sorabuild] nethorse: target %s is not inside overlay/" % rel)
    ctx.need(os.path.join(ctx.romfs, "Actor", "ActorInfo.product.sbyml"),
             os.path.join(ctx.romfs, "System", "Resource", "ResourceSizeTable.product.srsizetable"))
    lab = os.path.join(ctx.out, LAB_RECORD)
    if os.path.exists(lab):
        raise SystemExit("[sorabuild] nethorse: %s is there (the lab's horse_install.py put the puppets in by hand): run "
                         "tools/sorabuild/nethorse/horse_install.py --undo %s first" % (lab, ctx.out))
    bake = os.path.join(ctx.work, "nethorse", "bake")
    shutil.rmtree(bake, ignore_errors=True)   # (ours: a killed bake must not leave a folder horse_bake.py refuses)
    if os.path.exists(bake):
        raise SystemExit("[sorabuild] nethorse: could not empty %s (a file in use?)" % bake)
    ctx.py(os.path.join(ctx.tools, "nethorse", "horse_bake.py"), ctx.romfs, bake)
    with open(os.path.join(bake, "horse_manifest.json"), encoding="utf-8") as f:
        man = json.load(f)
    got = sorted(OV + r for r in man.get("files", {}) if r not in BAKE_SKIP)
    if got != sorted(outs):
        extra, miss = sorted(set(got) - set(outs)), sorted(set(outs) - set(got))
        raise SystemExit("[sorabuild] nethorse: your BotW romfs bakes other puppets than this release expects (not the 1.9.0 "
                         "romfs Savage Wind made?): extra %s, missing %s (see %s)" % (
                             ", ".join(extra[:4]) or "none", ", ".join(miss[:4]) or "none",
                             os.path.join(bake, "horse_list.txt")))
    want = {OV + r: s for r, s in man["files"].items() if r not in BAKE_SKIP}
    for rel in outs:   # (the bake's own sha256: a short / damaged file never reaches data/)
        src = os.path.join(bake, *rel[len(OV):].split("/"))
        if _sha(src) != want[rel]:
            raise SystemExit("[sorabuild] nethorse: %s is not as the bake's manifest says" % src)
    old = _read_written(ctx)
    for rel in TABLES:   # (the merge rule: never overwrite another tool's ActorInfo / ResourceSizeTable)
        p = os.path.join(ctx.out, *rel.split("/"))
        if os.path.isfile(p):
            s = _sha(p)
            if s != want[rel] and s != old.get(rel):
                raise SystemExit("[sorabuild] nethorse: %s is there and was not written by this step (sha256 %s): another tool's "
                                 "table, refused (nothing replaced). Move it away (or merge its rows in a step with "
                                 "deps=[\"nethorse\"]), then build again" % (p, s[:16]))
    # (stale puppets of an older bake: only OUR prefix, in OUR folder)
    pdir = os.path.join(ctx.out, *PUPPET_DIR.split("/"))
    keep = {os.path.basename(r) for r in outs if r.startswith(PUPPET_DIR + "/")}
    gone = []
    if os.path.isdir(pdir):
        for n in sorted(os.listdir(pdir)):
            if n.startswith(PUPPET_PREFIX) and n.endswith(".sbactorpack") and n not in keep:
                os.remove(os.path.join(pdir, n))
                gone.append(n)
    if gone:
        ctx.log("removed %d puppets of an older bake: %s" % (len(gone), ", ".join(gone[:4])))
    wp = os.path.join(ctx.work, WRITTEN)
    os.makedirs(os.path.dirname(wp), exist_ok=True)
    rec = dict(old)
    rec.update({rel: want[rel] for rel in outs})   # (written first: a stop half way still knows these bytes as ours)
    with open(wp + ".tmp", "w", encoding="utf-8", newline="\n") as f:
        json.dump(rec, f, indent=1, sort_keys=True)
    os.replace(wp + ".tmp", wp)
    for rel in sorted(outs):
        src = os.path.join(bake, *rel[len(OV):].split("/"))
        dst = ctx.outp(rel)
        tmp = dst + ".tmp"
        shutil.copyfile(src, tmp)
        if _sha(tmp) != want[rel]:
            os.remove(tmp)
            raise SystemExit("[sorabuild] nethorse: %s copied wrong (sha256)" % dst)
        ctx.commit(tmp, dst)
    with open(wp + ".tmp", "w", encoding="utf-8", newline="\n") as f:
        json.dump({rel: want[rel] for rel in outs}, f, indent=1, sort_keys=True)
    os.replace(wp + ".tmp", wp)
    ctx.log("%d files into %s (%d puppets)" % (len(outs), os.path.join(ctx.out, "overlay"), len(keep)))


STEPS = [Step("nethorse", _run, outputs=outputs(), opt="nethorse")]

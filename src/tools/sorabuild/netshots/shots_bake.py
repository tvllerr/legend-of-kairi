"""netshots (s289): a friend's arrows, Remote Bombs (and, for a later step, his Cryonis pillars) drawn in your world in a room
(`set netshots 1`). This bakes each one's model from the player's OWN BotW romfs into the mod's draw format, at install time
(nothing from BotW is in the repo or a package), with netgear's baker (tools/sorabuild/netgear/gear_bake.py: the same 'SKRG'
one-bone format, the same identity pose file).
After Savage Wind PR #294 "Online mode" by byluki (fixes by mstampfli), MIT: its list of the shots it shows.

  python -E -P -B shots_bake.py <romfs dir> <out dir> [--tex N]

  <romfs>/Pack/TitleBG.pack     a SARC: Actor/Pack/<actor>.sbactorpack + Model/<folder>(.Tex).sbfres of the arrows and bombs
                                (read only; the few files needed are copied to <out>/gear/.shots_romfs and that is removed after)
  <romfs>/Actor/Pack, /Model    looked in when TitleBG.pack lacks one (a romfs laid out loose)
Writes:
  <out>/gear/<actor>.bin        'SKRG' v1, ONE bone = the BotW model's own frame (the actor's matrix places it), positions in cm
  <out>/gear/gear_pose.bin      'SKRP' v1 identity pose, only when it is not there yet (gear_bake.py writes the same bytes)
  <out>/gear/shots_list.txt     one line per actor: baked (verts, tris, textures) or skipped (why)
Never touches gear_list.txt (netgear's) or any other file. Display only: the runtime loads these as draw actors (no BotW actor,
nothing pickable, no physics, no save).
"""
import os
import shutil
import sys

import oead

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "netgear"))
import gear_bake  # noqa: E402

# (runtime/sora_net_shots_core.h NSK_NAME, in its order; then the later step's Cryonis pillar)
SHOTS = ["NormalArrow", "FireArrow", "IceArrow", "ElectricArrow", "BombArrow_A", "AncientArrow", "BrightArrow",
         "RemoteBomb", "RemoteBombCube", "RemoteBomb2", "RemoteBombCube2", "Obj_IceMakerBlock"]


def folder_only(romfs, actor):
    """a ModelList with a Folder but no Unit list (Obj_IceMakerBlock: Folder Obj_IcemakerBlock_A): -> the folder, "" = none"""
    p = os.path.join(romfs, "Actor", "Pack", actor + ".sbactorpack")
    try:
        sa = oead.Sarc(gear_bake.unyaz(open(p, "rb").read()))
        for f in sa.get_files():
            if f.name.endswith(".bmodellist"):
                ml = oead.aamp.ParameterIO.from_binary(f.data).lists["ModelData"].lists["ModelData_0"]
                return str(ml.objects["Base"].params["Folder"].v)
    except Exception:  # (oead's errors are no OSError / ValueError: any problem = none)
        pass
    return ""


_model_of = gear_bake.model_of


def model_of(romfs, actor):
    folder, unit = _model_of(romfs, actor)
    if os.path.exists(os.path.join(romfs, "Model", folder + ".sbfres")):
        return folder, unit
    f = folder_only(romfs, actor)
    if not f:
        return folder, unit
    mp = os.path.join(romfs, "Model", f + ".sbfres")
    if os.path.exists(mp):  # (no Unit: the folder's only FMDL, Obj_IcemakerBlock_A -> Obj_IcemakerBlock_A_01)
        try:
            names = [n for n, _ in gear_bake.Bfres(gear_bake.unyaz(open(mp, "rb").read())).models()]
            if f not in names and len(names) == 1:
                return f, names[0]
        except Exception:  # (bake_actor says what is wrong with it)
            pass
    return f, f


def gather(romfs, tmp, actors):
    """copy the actor packs and their model files (TitleBG.pack first, then the loose romfs) into tmp's romfs layout"""
    os.makedirs(os.path.join(tmp, "Actor", "Pack"), exist_ok=True)
    os.makedirs(os.path.join(tmp, "Model"), exist_ok=True)
    tb = os.path.join(romfs, "Pack", "TitleBG.pack")
    sa = oead.Sarc(gear_bake.unyaz(open(tb, "rb").read())) if os.path.exists(tb) else None
    files = {f.name: f for f in sa.get_files()} if sa else {}

    def put(rel):
        dst = os.path.join(tmp, *rel.split("/"))
        if os.path.exists(dst):
            return True
        if rel in files:
            with open(dst, "wb") as o:
                o.write(bytes(files[rel].data))
            return True
        src = os.path.join(romfs, *rel.split("/"))
        if os.path.exists(src):
            shutil.copyfile(src, dst)
            return True
        return False

    for a in actors:
        if not put("Actor/Pack/%s.sbactorpack" % a):
            continue
        folder, _ = model_of(tmp, a)
        if not gear_bake.actor_name_ok(folder):  # (a name from the romfs: never a path outside tmp)
            continue
        put("Model/%s.sbfres" % folder)
        put("Model/%s.Tex.sbfres" % folder)


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    opt = {a.split("=")[0]: (a.split("=", 1)[1] if "=" in a else "") for a in argv if a.startswith("--")}
    if len(args) != 2:
        raise SystemExit(__doc__)
    romfs, out = args
    side = int(opt.get("--tex") or 128)
    gd = os.path.join(out, "gear")
    tmp = os.path.join(gd, ".shots_romfs")
    os.makedirs(gd, exist_ok=True)
    shutil.rmtree(tmp, ignore_errors=True)
    gear_bake.model_of = model_of  # (bake_actor looks its model up through this name: our Folder-only fallback, this run only)
    lines, nb = [], 0
    try:
        gather(romfs, tmp, SHOTS)
        for a in SHOTS:
            try:
                blob, info = gear_bake.bake_actor(tmp, a, side)
            except Exception as e:  # (an install step: one odd model is skipped and listed, never the whole bake)
                lines.append("%s skipped: %s: %s" % (a, type(e).__name__, e))
                continue
            gear_bake.write_atomic(os.path.join(gd, a + ".bin"), blob)
            nv = sum(len(s["pos"]) for s in info["shapes"])
            nt = sum(len(s["idx"]) for s in info["shapes"]) // 3
            lines.append("%s baked: %s/%s, %d shapes, %d verts, %d tris, textures %s, %d bytes" % (
                a, info["folder"], info["unit"], len(info["shapes"]), nv, nt,
                " ".join("%dx%d" % (t.shape[1], t.shape[0]) for t in info["texs"]), len(blob)))
            nb += 1
    finally:
        gear_bake.model_of = _model_of
        shutil.rmtree(tmp, ignore_errors=True)
    pose = os.path.join(gd, "gear_pose.bin")
    if not os.path.exists(pose):
        gear_bake.write_atomic(pose, gear_bake.skrp_identity())
    gear_bake.write_atomic(os.path.join(gd, "shots_list.txt"), ("\n".join(lines) + "\n").encode("utf-8"))
    print("shots_bake: %d of %d shot models baked into %s" % (nb, len(SHOTS), gd))


if __name__ == "__main__":
    main(sys.argv[1:])

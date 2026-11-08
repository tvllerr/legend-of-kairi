"""Group sounds: work/sounds_recipe.json + work/sounds/ = the sound inputs of the sfx tools, switch config "sounds".

  "sounds": "recipe"  (default since s163)
      sounds_mine   (s182) work/sounds_recipe.json = our numbers and names (sound ids, SCD file + stream, volumes, trigger frames,
                    effect / system / hud tables) MINED from the player's own KH2 files by sounds/mine/mine_recipe.py (it replaced
                    the frozen tools/sorabuild/sounds_recipe.json; the old file is kept as sounds/dev/recipe_reference.json, dev only).
                    Inputs: the KH2 extract (kh2pick list), config "kh2exe" (hud icon widths, sha1 pinned), the trig_*.json of
                    fx_trigjson and the baked summon motions (summon_peterpan / _tink / _genie): it runs our sfx tools on silent
                    wavs to see which sounds they ask for, so it must never read the recipe it is writing (it does not).
      sounds_build  recipe + the player's own KH2 SCD files -> work/sounds/{mined,bdx,kh2_sfx,kh2_hud.json}.  Config extras:
                    "vgmstream" = path of vgmstream-cli (Ogg Vorbis voices; the old "vorbis_fallback": "phh" option was removed).
  "sounds": "phh"     explicit opt-in, deprecated: nothing to build, the tools read the old PHH 0.4.8 cache (config fxcache) as before.
Switching the mode: re-run `sounds_mine` by name (its dependents then re-run).
"""
import hashlib
import os
import shutil
import sys
from sb import Step

RECIPE = "sounds_recipe.json"   # in the work dir (written by sounds_mine)
SUMMON_MOTIONS = ["peterpan", "tink", "genie"]   # = steps/fx.py SUMMON_MOTIONS (sumsfx reads them)
# what the miner's answer depends on besides the KH2 files: the miners and the sfx tools its trace runs
MINE_SRC = ["sounds/mine/" + n for n in ("mine_recipe.py", "mine_actors.py", "mine_effects.py", "mine_misc.py", "mine_sounds.py",
                                         "kh2snd.py", "kh2bin.py")] + \
    ["sounds/build_sounds.py", "fxrec/files82.txt", "fx/sndsrc.py"] + \
    ["fx/" + n for n in ("sfxpack.py", "sfxadd_s98.py", "sfxadd_s107.py", "sfxroxas.py", "sfxmickey.py", "chickfx.py", "stitchfx.py",
                         "sumsfx.py", "ksse.py", "dfse.py")]


def recipe_path(ctx):
    return os.path.join(ctx.work, RECIPE)


def mine_tag(ctx):
    """what sounds_mine_mode.txt must say: "phh", or "mine code=<hash of MINE_SRC>"."""
    if ctx.snd_mode == "phh":
        return "phh"
    h = hashlib.sha256()
    for n in MINE_SRC:
        h.update(n.encode() + b"\0" + open(os.path.join(ctx.tools, n), "rb").read().replace(b"\r\n", b"\n") + b"\0")
    return "mine code=" + h.hexdigest()[:16]


def mine_stale(ctx):
    """a mode switch, changed miner / sfx tool code or a lost recipe re-runs sounds_mine (and so every step after it)"""
    if ctx.snd_mode == "recipe" and not os.path.exists(recipe_path(ctx)):
        return True
    try:
        return open(ctx.workp("sounds_mine_mode.txt")).read().strip() != mine_tag(ctx)
    except OSError:
        return True


def mine(ctx):
    marker = ctx.workp("sounds_mine_mode.txt")
    if ctx.snd_mode == "phh":
        ctx.log("sounds: phh (tools read the PHH cache %s); no recipe to mine" % ctx.fxcache)
        open(marker, "w").write("phh\n")
        return
    exe = ctx.cfg.get("kh2exe")
    if not exe or not os.path.isfile(exe):
        raise SystemExit("[sorabuild] sounds_mine needs config kh2exe = the Steam KH2 FM exe (hud icon widths), not %r" % (exe,))
    trig = ctx.workp("fxtrig/x")[:-2]
    sumdata = ctx.workp("sounds_mine/sumdata/x")[:-2]
    shutil.rmtree(sumdata, ignore_errors=True)
    for k in SUMMON_MOTIONS:  # the same copies fxsfx_sumdata makes, in our own dir (fxsumdata belongs to the fx chain)
        for f in (k + "_motion.bin", k + "call_motion.bin"):
            src = os.path.join(ctx.out, "forms", f)
            ctx.need(src)
            ctx.copy(src, os.path.join(sumdata, k, f))
    out = recipe_path(ctx)
    if os.path.exists(out):
        os.remove(out)
    ctx.py(os.path.join(ctx.tools, "sounds", "mine", "mine_recipe.py"), "--kh2", ctx.kh2, "--exe", exe, "--trigdir", trig,
           "--sumdata", sumdata, "--work", ctx.workp("sounds_mine/trace/x")[:-2], "--out", out)
    ctx.need(out)
    ctx.log("recipe %s sha256 %s" % (out, hashlib.sha256(open(out, "rb").read()).hexdigest()))
    open(marker, "w").write(mine_tag(ctx) + "\n")


def mode_tag(ctx):
    """what sounds_mode.txt must say for the current config: "phh", or "recipe <sha256 of work/sounds_recipe.json>"."""
    if ctx.snd_mode == "phh":
        return "phh"
    recipe = recipe_path(ctx)
    sha = hashlib.sha256(open(recipe, "rb").read()).hexdigest() if os.path.exists(recipe) else "missing"
    return "recipe " + sha


def stale(ctx):
    """s157 review S1: a mode switch or a changed recipe re-runs sounds_build (and so every step after it)."""
    try:
        return open(ctx.workp("sounds_mode.txt")).read().strip() != mode_tag(ctx)
    except OSError:
        return True


def build(ctx):
    mode = ctx.snd_mode
    marker = ctx.workp("sounds_mode.txt")
    if mode == "phh":
        ctx.log("sounds: phh (tools read the PHH cache %s); nothing to build" % ctx.fxcache)
        open(marker, "w").write("phh\n")
        return
    recipe = recipe_path(ctx)
    ctx.need(recipe)
    if ctx.cfg.get("only_adpcm"):  # s157 review S2: would mark a build done without the 133 voices
        raise SystemExit("sounds: config only_adpcm is for build_sounds.py proof runs only, never a sorabuild build")
    args = ["--recipe", recipe, "--kh2", ctx.kh2, "--out", ctx.sndroot()]
    if ctx.cfg.get("vgmstream"):
        args += ["--vgmstream", ctx.cfg["vgmstream"]]
    ctx.py(os.path.join(ctx.tools, "sounds", "build_sounds.py"), *args)
    open(marker, "w").write(mode_tag(ctx) + "\n")


STEPS = [Step("sounds_mine", mine, work_outputs=["sounds_mine_mode.txt"],
              deps=["fx_trigjson"] + ["summon_" + k for k in SUMMON_MOTIONS]),
         Step("sounds_build", build, work_outputs=["sounds_mode.txt"], deps=["sounds_mine"])]
STEPS[0].stale = mine_stale
STEPS[1].stale = stale

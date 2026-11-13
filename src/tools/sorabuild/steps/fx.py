"""Group fx: data/fx/fx.bin, data/sfx/sfx.bin, data/sfx/extra/se/se000/025.wav.

Both packs are ONE full bake plus a chain of appends; the ORDER of the chain is the element / sound order of the live
files (multiplayer sends pack indexes), so the dependencies below are the exact install order of the mod's history
(recon/S153_FI_HELPER_FX.md sections 1 and 2).  The running pack of each chain lives in work/fxchain/fx.bin and
work/fxchain/sfx.bin; every step reads it, writes <name>.new and renames it over (atomic, no backups, no size guards).

Inputs: the PHH 0.4.8 cache (ctx.fxcache), KH2 extract (ctx.kh2, for PRIZE.mdlx, the F_TT010 skateboard SCD, se500.win32.scd and
the four sorabake trig JSONs) and the baked summon motions forms/{peterpan,tink,genie}[call]_motion.bin that the bakes
group puts into ctx.out (needed by fxsfx_sumdata only).  Tools: tools/sorabuild/fx/*.py (paths come in as arguments or SB_* env).
"""
import os
import shutil
import sys
import sb
from sb import Step

FXD = "fx"  # tools dir below ctx.tools
CHAIN_FX = "fxchain/fx.bin"
CHAIN_SFX = "fxchain/sfx.bin"
TRIG = ["W_EX010_RX", "W_EX200", "P_EX110", "P_EX200"]
PARTY_TRIG = ["W_EH000", "W_HE000", "W_MU000", "W_AL000", "W_CA000"]  # (s223 rikufx: the stand-ins' weapon msets, fx_party's trail windows)
TRIG = TRIG + PARTY_TRIG
SUMMON_MOTIONS = ["peterpan", "tink", "genie"]
CAP32 = (1 << 32) - 1


def T(ctx, name):
    return os.path.join(ctx.tools, FXD, name)


def envs(ctx):
    """The paths every tool of the group needs (the tools read these SB_* variables)."""
    return {
        "SB_FXCACHE": ctx.fxcache,
        "SB_FXDIR": ctx.fxdir(),  # (s161) kh2_fx.bin / kh2_fx.json: work/fxrec/out ("fx": "fxrec") or the PHH cache
        "SB_KH2": ctx.kh2,
        "SB_TRIGDIR": ctx.workp("fxtrig/x")[:-2],
        "SB_EXTRA": ctx.workp("fxextra/x")[:-2],
        "SB_SUMDATA": ctx.workp("fxsumdata/x")[:-2],
        **ctx.snd_env(),  # (s156) "sounds": "recipe" -> SB_SNDROOT = work/sounds (sndsrc.py)
    }


def disk(ctx):
    free = shutil.disk_usage(ctx.work).free / 2**30
    ctx.log("free disk on the work drive: %.1f GiB" % free)


def cur_fx(ctx):
    return ctx.workp(CHAIN_FX)


def cur_sfx(ctx):
    return ctx.workp(CHAIN_SFX)


def swap(ctx, new, cur):
    """new pack -> running pack (atomic); the size check of the 4 GiB uint32 offset cap."""
    ctx.need(new)
    if os.path.getsize(new) > CAP32:
        raise SystemExit("[sorabuild] %s: %s passes the uint32 cap" % (ctx.step, new))
    os.replace(new, cur)
    ctx.log("%s now %d B" % (os.path.basename(cur), os.path.getsize(cur)))
    disk(ctx)


def tmpdir(ctx, name):
    d = ctx.workp("fxtmp/%s/x" % name)[:-1]
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d, exist_ok=True)
    return d


# ------------------------------------------------------------------------------------------------- bake inputs
def trigjson(ctx):
    """The four trig_*.json (mset frame / range triggers): built by a small C# tool (= sorabake `trigjson`)."""
    src = ctx.workp("fxtrigjson_src/x")[:-1]
    build = ctx.workp("fxtrigjson_build/x")[:-1]
    if not sb.take_kit_tool(ctx, "trigjson", build):  # (s168) friend kit: prebuilt, else build it here
        shutil.rmtree(src, ignore_errors=True)
        os.makedirs(src)
        for f in ("trigjson.csproj", "Program.cs"):
            ctx.copy(os.path.join(ctx.tools, FXD, "trigjson", f), os.path.join(src, f))
        ctx.run([ctx.dotnet, "build", "-c", "Release", "-o", build, "-p:OpenKhDir=" + ctx.openkh,
                 os.path.join(src, "trigjson.csproj")], cwd=src)
    exe = os.path.join(build, "trigjson.exe")
    for n in TRIG:
        ctx.run([exe, ctx.kh2, n, ctx.workp("fxtrig/trig_%s.json" % n)])
    shutil.rmtree(src, ignore_errors=True)


def extra_se500(ctx):
    """data/sfx/extra/se/se500/010.wav (wall_hit = se500 #10): decoded from the KH2 SCD, bit-exact to vgmstream -s 11."""
    scd = os.path.join(ctx.kh2, "kh2_sixth/original/se/se500.win32.scd")
    ctx.need(scd)
    ctx.py(T(ctx, "mkextra.py"), "scd", "--scd", scd, "--index", "10", "--out", ctx.workp("fxextra/se/se500/010.wav"))


def lvup_wav(ctx):
    """data/sfx/extra/se/se000/025.wav (runtime level-up chime) = PHH's se000 #25 wav at the json volume 0.9."""
    src = os.path.join(ctx.sndroot(), "kh2_sfx/se/se000/025.wav")
    ctx.need(src)
    ctx.py(T(ctx, "mkextra.py"), "vol", "--in", src, "--volume", "0.9", "--out", ctx.outp("sfx/extra/se/se000/025.wav"))


# ------------------------------------------------------------------------------------------------- fx chain
def fx_base(ctx):
    new = cur_fx(ctx) + ".new"
    ctx.py(T(ctx, "fxbake.py"), "--cache", ctx.fxdir(), "--out", new, env=envs(ctx))
    swap(ctx, new, cur_fx(ctx))


def _fx_tool(script, *extra):
    def run(ctx):
        new = cur_fx(ctx) + ".new"
        if os.path.exists(new):
            os.remove(new)
        ctx.py(T(ctx, script), "--in", cur_fx(ctx), "--out", new, "--cache", ctx.fxdir(), *extra, env=envs(ctx))
        swap(ctx, new, cur_fx(ctx))
    return run


def _hdr(ctx, name):
    return ctx.workp("fxhdr/" + name)


def fxsfx_chick(ctx):
    d = tmpdir(ctx, "chick")
    ctx.py(T(ctx, "chickfx.py"), "--sfx-in", cur_sfx(ctx), "--fx-in", cur_fx(ctx), "--out", d,
           "--header", _hdr(ctx, "sora_summon_fx.h"), env=envs(ctx))
    swap(ctx, os.path.join(d, "fx.bin"), cur_fx(ctx))
    swap(ctx, os.path.join(d, "sfx.bin"), cur_sfx(ctx))
    shutil.rmtree(d, ignore_errors=True)


def fx_fps(ctx):
    d = tmpdir(ctx, "fps")
    ctx.py(T(ctx, "fpsfx.py"), "--fx-in", cur_fx(ctx), "--out", d, env=envs(ctx))
    swap(ctx, os.path.join(d, "fx.bin"), cur_fx(ctx))
    shutil.rmtree(d, ignore_errors=True)


def fxsfx_stitch(ctx):
    d = tmpdir(ctx, "stitch")
    ctx.py(T(ctx, "stitchfx.py"), "--sfx-in", cur_sfx(ctx), "--fx-in", cur_fx(ctx), "--out", d, "--fx", "6,7,9",
           "--header", _hdr(ctx, "sora_summon_fx_st_stitch.h"), env=envs(ctx))
    swap(ctx, os.path.join(d, "fx.bin"), cur_fx(ctx))
    swap(ctx, os.path.join(d, "sfx.bin"), cur_sfx(ctx))
    shutil.rmtree(d, ignore_errors=True)


def fx_ohana(ctx):
    d = tmpdir(ctx, "ohana")
    ctx.py(T(ctx, "stitchfx.py"), "--no-sfx", "--fx-in", cur_fx(ctx), "--sfx-in", cur_sfx(ctx), "--out", d, "--fx", "1,4,13",
           "--header", _hdr(ctx, "sora_summon_fx_st.h"), env=envs(ctx))
    swap(ctx, os.path.join(d, "fx.bin"), cur_fx(ctx))
    shutil.rmtree(d, ignore_errors=True)


def fxsfx_sumdata(ctx):
    # the baked summon motions (bakes group) -> <SB_SUMDATA>/<key>/<key>_motion.bin, <key>call_motion.bin
    for k in SUMMON_MOTIONS:
        for f in (k + "_motion.bin", k + "call_motion.bin"):
            s = os.path.join(ctx.out, "forms", f)
            if not os.path.exists(s):
                raise SystemExit("[sorabuild] %s: needs the baked %s in the out tree (bakes group) first" % (ctx.step, s))
            ctx.copy(s, ctx.workp("fxsumdata/%s/%s" % (k, f)))
    d = tmpdir(ctx, "sumsfx")
    ctx.py(T(ctx, "sumsfx.py"), "build", "--sfx-in", cur_sfx(ctx), "--out", d, env=envs(ctx))
    swap(ctx, os.path.join(d, "sfx.bin"), cur_sfx(ctx))
    ctx.py(T(ctx, "sumfx.py"), "build", "--fx-in", cur_fx(ctx), "--out", d, env=envs(ctx))
    swap(ctx, os.path.join(d, "fx.bin"), cur_fx(ctx))
    shutil.rmtree(d, ignore_errors=True)


def fxsfx_knock(ctx):
    new = cur_fx(ctx) + ".new"
    if os.path.exists(new):
        os.remove(new)  # (s154 review FYI) never swap in a stale .new
    ctx.py(T(ctx, "limitfx.py"), "--in", cur_fx(ctx), "--out", new, "--cache", ctx.fxdir(), "--only", "lim_goofy2_", env=envs(ctx))
    swap(ctx, new, cur_fx(ctx))
    d = tmpdir(ctx, "ksse")
    ctx.py(T(ctx, "ksse.py"), "build", "--sfx-in", cur_sfx(ctx), "--out", d, env=envs(ctx))
    swap(ctx, os.path.join(d, "sfx.bin"), cur_sfx(ctx))
    shutil.rmtree(d, ignore_errors=True)


def fxsfx_duck(ctx):
    new = cur_fx(ctx) + ".new"
    if os.path.exists(new):
        os.remove(new)  # (s154 review FYI) never swap in a stale .new
    ctx.py(T(ctx, "limitfx.py"), "--in", cur_fx(ctx), "--out", new, "--cache", ctx.fxdir(), "--only", "lim_donald2_", env=envs(ctx))
    swap(ctx, new, cur_fx(ctx))
    d = tmpdir(ctx, "dfse")
    ctx.py(T(ctx, "dfse.py"), "build", "--sfx-in", cur_sfx(ctx), "--out", d, env=envs(ctx))
    swap(ctx, os.path.join(d, "sfx.bin"), cur_sfx(ctx))
    shutil.rmtree(d, ignore_errors=True)


def fxsfx_kbown(ctx):
    """(s216 kbfx) the mod's own two Keyblades: their sparks / ribbons / beam (shaped at install from the per-Keyblade KH2 sets this
    chain already packed: Fenrir's + Photon Debugger's, recoloured and redrawn; our procedural textures) + our synthesised hit / beam
    sounds (tools/sorabuild/ours/kb/*.wav, made by kbsynth.py). Appended LAST to both packs (old ids unchanged), before fx_final's
    dfblend (it never touches lim_donald2_*)."""
    d = tmpdir(ctx, "kbown")
    ctx.py(T(ctx, "kbownfx.py"), "--fx-in", cur_fx(ctx), "--sfx-in", cur_sfx(ctx), "--out", d,
           "--ours", os.path.join(ctx.tools, "ours", "kb"), env=envs(ctx))
    swap(ctx, os.path.join(d, "fx.bin"), cur_fx(ctx))
    swap(ctx, os.path.join(d, "sfx.bin"), cur_sfx(ctx))
    shutil.rmtree(d, ignore_errors=True)


def fx_party(ctx):
    """(s223 rikufx) the party stand-ins' own KH2 weapon effects (fx/partyfx.py): their hit sparks by EffectOnHit, their weapon's swing
    ribbon (element 10) and its weapon_trail windows (range trigger 12 of the weapon msets), from step fxrec_party's recording of the
    player's own KH2 files. Appended LAST (old ids, frames, textures and trail windows unchanged; checked by the tool), before fx_final's
    dfblend (it never touches lim_donald2_*). No recording ("fx": "phh") = nothing appended: the stand-ins draw the base sparks."""
    rec = ctx.workp("fxrec_party/out/x")[:-2]
    if not os.path.isfile(os.path.join(rec, "kh2_fx.bin")):
        if ctx.fx_mode != "phh":  # (s226 review F2) a recording build without the party recording must not pass as built
            raise SystemExit("[sorabuild] %s: no party recording at %s (step fxrec_party left no kh2_fx.bin)" % (ctx.step, rec))
        ctx.log("no party recording at %s (config fx = phh): fx.bin gets no stand-in rows (base sparks, no ribbon)" % rec)
        return
    d = tmpdir(ctx, "party")
    ctx.py(T(ctx, "partyfx.py"), "--fx-in", cur_fx(ctx), "--rec", rec, "--trig", envs(ctx)["SB_TRIGDIR"], "--kh2", ctx.kh2, "--out", d,
           env=envs(ctx))
    swap(ctx, os.path.join(d, "fx.bin"), cur_fx(ctx))
    shutil.rmtree(d, ignore_errors=True)


def fx_final(ctx):
    """dfblend (last, re-run after anything that touches lim_donald2_*) on a copy, then the data file."""
    tmp = ctx.outp("fx/fx.bin") + ".tmp"
    shutil.copyfile(cur_fx(ctx), tmp)
    ctx.py(T(ctx, "dfblend.py"), tmp, env=envs(ctx))
    ctx.commit(tmp, ctx.outp("fx/fx.bin"))
    os.remove(cur_fx(ctx))  # the running pack is spent (up to 1.9 GB)
    disk(ctx)


# ------------------------------------------------------------------------------------------------- sfx chain
def sfx_base(ctx):
    new = cur_sfx(ctx) + ".new"
    ctx.py(T(ctx, "sfxpack.py"), "--cache", ctx.fxcache, "--out", new, env=envs(ctx))
    swap(ctx, new, cur_sfx(ctx))


def _sfx_tool(script, *extra):
    def run(ctx):
        new = cur_sfx(ctx) + ".new"
        if os.path.exists(new):
            os.remove(new)
        ctx.py(T(ctx, script), cur_sfx(ctx), new, *extra, env=envs(ctx))
        swap(ctx, new, cur_sfx(ctx))
    return run


def sfx_skate(ctx):
    ff = ctx.cfg.get("ffmpeg") or shutil.which("ffmpeg")
    if not ff:
        raise SystemExit("[sorabuild] sfx_skate needs ffmpeg (config key 'ffmpeg' or on PATH): it decodes the skateboard SCD")
    new = cur_sfx(ctx) + ".new"
    ctx.py(T(ctx, "sfxskate.py"), cur_sfx(ctx), new, env=dict(envs(ctx), SB_FFMPEG=ff))
    swap(ctx, new, cur_sfx(ctx))


def sfx_roxas(ctx):
    new = cur_sfx(ctx) + ".new"
    ctx.py(T(ctx, "sfxroxas.py"), cur_sfx(ctx), new, envs(ctx)["SB_TRIGDIR"], env=envs(ctx))
    swap(ctx, new, cur_sfx(ctx))


def sfx_mickey(ctx):
    new = cur_sfx(ctx) + ".new"
    ctx.py(T(ctx, "sfxmickey.py"), cur_sfx(ctx), new, envs(ctx)["SB_TRIGDIR"], env=envs(ctx))
    swap(ctx, new, cur_sfx(ctx))


def sfx_final(ctx):
    tmp = ctx.outp("sfx/sfx.bin") + ".tmp"
    shutil.copyfile(cur_sfx(ctx), tmp)
    ctx.commit(tmp, ctx.outp("sfx/sfx.bin"))
    os.remove(cur_sfx(ctx))


STEPS = [
    # bake inputs nobody scripted before
    Step("fx_trigjson", trigjson, work_outputs=["fxtrig/trig_%s.json" % n for n in TRIG]),
    Step("sfx_extra_se500", extra_se500, work_outputs=["fxextra/se/se500/010.wav"]),
    Step("sfx_lvup_wav", lvup_wav, outputs=["sfx/extra/se/se000/025.wav"], deps=["sounds_build"]),
    # fx.bin chain
    Step("fx_base", fx_base, deps=["fx_trigjson", "sounds_build", "fxrec_record"]),
    Step("fx_prize", _fx_tool("prizefx.py"), deps=["fx_base"]),
    Step("fx_magic", _fx_tool("magicfx.py"), deps=["fx_prize"]),
    Step("fx_limits", _fx_tool("limitfx.py", "--only", "lim_donald_,lim_goofy_"), deps=["fx_magic"]),
    # sfx.bin chain
    Step("sfx_base", sfx_base, deps=["sfx_extra_se500", "sounds_build"]),
    Step("sfx_potion", _sfx_tool("sfxadd_s98.py"), deps=["sfx_base"]),
    Step("sfx_abil", _sfx_tool("sfxadd_s107.py"), deps=["sfx_potion"]),
    Step("sfx_roxas", sfx_roxas, deps=["sfx_abil", "fx_trigjson"]),
    Step("sfx_mickey", sfx_mickey, deps=["sfx_roxas"]),
    Step("sfx_skate", sfx_skate, deps=["sfx_mickey"]),
    # the summons: each tool touches both packs (fx order 5..11, sfx order 7..11)
    Step("fxsfx_chick", fxsfx_chick, deps=["fx_limits", "sfx_skate"]),
    Step("fx_fps", fx_fps, deps=["fxsfx_chick"]),
    Step("fxsfx_stitch", fxsfx_stitch, deps=["fx_fps"]),
    Step("fx_ohana", fx_ohana, deps=["fxsfx_stitch"]),
    Step("fxsfx_sumdata", fxsfx_sumdata, deps=["fx_ohana"] + ["summon_" + k for k in SUMMON_MOTIONS]),  # (s154 merge) needs the bakes group's summon motions
    Step("fxsfx_knock", fxsfx_knock, deps=["fxsfx_sumdata"]),
    Step("fxsfx_duck", fxsfx_duck, deps=["fxsfx_knock"]),
    Step("fxsfx_kbown", fxsfx_kbown, deps=["fxsfx_duck"]),  # (s216 kbfx: our Keyblades' own effects + sounds, both packs)
    Step("sfx_party2", _sfx_tool("sfxparty2.py"), deps=["fxsfx_kbown"]),  # (s220 riku2: the party stand-ins' sounds, appended last)
    Step("fx_party", fx_party, deps=["fxsfx_kbown", "fxrec_party", "fx_trigjson"]),  # (s223 rikufx: the stand-ins' own sparks + ribbons)
    Step("fx_final", fx_final, deps=["fx_party"], outputs=["fx/fx.bin"]),
    Step("sfx_final", sfx_final, deps=["sfx_party2"], outputs=["sfx/sfx.bin"]),
]

# (s154 review M1/M2) the running packs make these steps ONE chain: if any of them has to run, sorabuild re-runs all of them
# from fx_base / sfx_base (2 min). A half-done step or a lone re-run can then never leave a pack with missing or doubled elements.
CHAIN = {"fx_base", "fx_prize", "fx_magic", "fx_limits", "sfx_base", "sfx_potion", "sfx_abil", "sfx_roxas", "sfx_mickey",
         "sfx_skate", "fxsfx_chick", "fx_fps", "fxsfx_stitch", "fx_ohana", "fxsfx_sumdata", "fxsfx_knock", "fxsfx_duck",
         "fxsfx_kbown", "sfx_party2", "fx_party", "fx_final", "sfx_final"}
for _s in STEPS:
    if _s.name in CHAIN:
        _s.chain = "fxpacks"

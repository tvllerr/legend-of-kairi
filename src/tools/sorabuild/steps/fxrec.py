"""Group fxrec: work/fxrec/out/{kh2_fx.bin,kh2_fx.json} = the effect recordings the fx tools read, switch config "fx".

  "fx": "phh"    (default) nothing to build: the fx tools read kh2_fx.bin / kh2_fx.json from the PHH cache as before.
  "fx": "fxrec"  our own recorder (tools/sorabuild/fxrec/): fxrec_build compiles fxrec_boot.exe (MSYS2 UCRT64 clang), fxrec_record
                 runs it over the 82 files of fxrec/files82.txt (loopend.py decides the loop-end tails from each file's own script)
                 and phf2write.py writes the PHF2 v4 bin + json.  Config: "kh2exe" = the Steam KH2 FM exe (sha1 2548F119..5A, others
                 refused), "kh2" = KH2 extract; optional "fxrec_jobs" (files recorded at once, default 2; each file = 5 recorder
                 processes of up to 768 MB; the heavy files run alone anyway, s164), "clang" (clang.exe, default MSYS2 UCRT64), "bash" (Git bash), and for tests only
                 "fxrec_only" = regex over the file names (a subset bin; the fx chain refuses to use it).
  The mod's other PHH-cache inputs (mined/kh2_sounds.kh.json etc.) come from the "sounds" switch, not from here.
  The kh2_fx.json "sounds" keys are NOT needed: sfxpack.py reads effects[..].sounds from mined/kh2_sounds.kh.json (the sounds tree).
Config "fx_keep" (s166): "used" (default) records only the effects the fx chain tools read (keep list = fx/fxcensus.py --emit-keep, derived from the
tools' own tables, no game bytes: 644 of 11,861 recordings, 14 of 82 files not run at all); "all" = every recording (the s164 full run).
Switching fx_keep re-runs fxrec_record and the whole fx chain after it (the tag holds the mode and a hash of the tool tables).
Switching the mode / a changed list, exe, filter or fxrec source re-runs both steps and everything after them.
Full run: python -E -P sorabuild.py --config <cfg with "fx": "fxrec"> fxrec_record     (~1-2 h; resumable: finished files are kept)
"""
import hashlib
import os
import shutil
import subprocess
import sys
import sb
from sb import Step

D = "fxrec"
SOURCES = ["fxrec_boot.c", "fxrec_s2.c", "fxrec_ldr.c", "fxrec_ldr.h"]
CLANG_DEFAULT = "C:/msys64/ucrt64/bin/clang.exe"


def _fx(ctx, n):
    return os.path.join(ctx.tools, D, n)


def _sha(paths):
    h = hashlib.sha256()
    for p in paths:
        h.update(open(p, "rb").read().replace(b"\r\n", b"\n"))
    return h.hexdigest()[:16]


def _code(ctx):
    return _sha([_fx(ctx, n) for n in SOURCES + ["runfile.sh", "loopend.py", "fxsolve.py", "fxvbin.py", "hdtab.py", "phf2write.py",
                                                    "recordall.py", "files82.txt"]])


KEEP_SRC = ["fx/fxcensus.py", "fx/fxbake.py", "fx/prizefx.py", "fx/magicfx.py", "fx/limitfx.py", "fx/chickfx.py", "fx/fpsfx.py",
            "fx/sumfx.py", "fx/stitchfx.py", "steps/fx.py"]


def _keep_code(ctx):
    """the keep list follows the tool tables: hash their sources (the list itself is derived at record time)"""
    return _sha([os.path.join(ctx.tools, n) for n in KEEP_SRC])


def mode_tag(ctx, what):
    if ctx.fx_mode == "phh":
        return "phh"
    keep = ctx.fx_keep
    return "fxrec %s code=%s only=%s exe=%s keep=%s%s" % (what, _code(ctx), ctx.cfg.get("fxrec_only", ""), ctx.cfg.get("kh2exe", ""), keep,
                                                          (" keepcode=" + _keep_code(ctx)) if keep == "used" else "")


def _stale(what, marker):
    def stale(ctx):
        try:
            return open(ctx.workp(marker)).read().strip() != mode_tag(ctx, what)
        except OSError:
            return True
    return stale


def build(ctx):
    marker = ctx.workp("fxrec_build_mode.txt")
    if ctx.fx_mode == "phh":
        ctx.log("fx: phh (the fx tools read kh2_fx.* from the PHH cache %s); nothing to build" % ctx.fxcache)
        open(marker, "w").write("phh\n")
        return
    exe = ctx.workp("fxrec/build/fxrec_boot.exe")
    if sb.take_kit_tool(ctx, "fxrec_boot", os.path.dirname(exe)):  # (s168) friend kit: prebuilt, no clang
        ctx.need(exe)
        open(marker, "w").write(mode_tag(ctx, "build") + "\n")
        return
    clang = ctx.cfg.get("clang") or shutil.which("clang") or CLANG_DEFAULT
    if not os.path.isfile(clang) and not shutil.which(clang):
        raise SystemExit("[sorabuild] fxrec_build needs clang (MSYS2 UCRT64: pacman -S mingw-w64-ucrt-x86_64-clang) - config 'clang'")
    env = {"PATH": os.path.dirname(clang) + os.pathsep + os.environ.get("PATH", "")}
    # same command as fxrec/build.sh; our exe must not sit at 0x140000000 (KH2 is mapped there): fixed low base, no ASLR
    ctx.run(sb.fxrec_cmd(clang, exe), env=env)
    ctx.need(exe)
    open(marker, "w").write(mode_tag(ctx, "build") + "\n")


def record(ctx):
    marker = ctx.workp("fxrec_record_mode.txt")
    if ctx.fx_mode == "phh":
        ctx.log("fx: phh; nothing to record")
        open(marker, "w").write("phh\n")
        return
    exe = ctx.cfg.get("kh2exe", "")
    args = ["--list", _fx(ctx, "files82.txt"), "--kh2", ctx.kh2, "--exe", exe, "--out", ctx.workp("fxrec/x")[:-2],
            "--jobs", str(ctx.cfg.get("fxrec_jobs", 2)), "--build", ctx.workp("fxrec/build/fxrec_boot.exe")]
    if ctx.cfg.get("fxrec_only"):
        args += ["--only", ctx.cfg["fxrec_only"]]
        ctx.log("TEST FILTER fxrec_only=%r: subset recording, not usable by the fx chain" % ctx.cfg["fxrec_only"])
    if ctx.cfg.get("bash"):
        args += ["--sh", ctx.cfg["bash"]]
    keepf = ctx.workp("fxrec/keep.json")
    if ctx.fx_keep == "used":
        # keep list from OUR tool tables (no game bytes, no kh2_fx.*): only these elements / tails are recorded
        ctx.py(os.path.join(ctx.tools, "fx", "fxcensus.py"), "--emit-keep", keepf)
        args += ["--keep", keepf]
    ctx.py(_fx(ctx, "recordall.py"), *args)
    if not ctx.cfg.get("fxrec_only"):
        # loud at build time: every recording the chain tools read must be in the written kh2_fx.json/.bin
        ctx.py(os.path.join(ctx.tools, "fx", "fxcensus.py"), "--check", ctx.workp("fxrec/out/x")[:-2])
    open(marker, "w").write(mode_tag(ctx, "record") + "\n")


STEPS = [
    Step("fxrec_build", build, work_outputs=["fxrec_build_mode.txt"]),
    Step("fxrec_record", record, deps=["fxrec_build"], work_outputs=["fxrec_record_mode.txt"]),
]
STEPS[0].stale = _stale("build", "fxrec_build_mode.txt")
STEPS[1].stale = _stale("record", "fxrec_record_mode.txt")

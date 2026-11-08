"""HUD bakes (KH2 -> data/hud) + the KH2 text tables they need (work/levels, work/dream).

hudbake = the s119 lineage + camp/campseq from the Oct 30 tools/hudbake lineage (tools/sorabuild/hud/hudbake).
Text: tools/hudbake's kh2_levels.py / kh2_dream_text.py and tools_s92/formfx/kh2_lvup_text.py (the version that made
the live lvup_labels.txt, 208 labels incl. the s92 Keyblade panel rows). Python bakes: tools_s111/s116/s139/s120 copies.
All verified byte-identical to the live files on this PC (s154).
"""
import hashlib
import os
import sb
from sb import Step

_SRC = ("hudbake.csproj", "Program.cs")


def _env(ctx):
    # (s182) font widths = fontinfo.bar "sys"; icon widths = work/sounds/kh2_hud.json (mined from the KH2 exe by sounds_mine/sounds_build)
    return {"SORABUILD_KH2": ctx.kh2, "SORABUILD_HUDJSON": os.path.join(ctx.sndroot(), "kh2_hud.json")}


def _hudbake(ctx):
    return os.path.join(ctx.work, "cs", "hudbake", "hudbake.exe")


def _src_tag(ctx):
    """(s163 review MUST-FIX) sha256 of hudbake's sources: an old work dir keeps an old hudbake.exe (one that still wants
    SORABUILD_PHH) unless hud_build re-runs when Program.cs / the csproj change."""
    h = hashlib.sha256()
    for f in _SRC:
        h.update(open(os.path.join(ctx.tools, "hud", "hudbake", f), "rb").read().replace(b"\r\n", b"\n"))
    return h.hexdigest()


def _stale(ctx):
    try:
        return open(ctx.workp("hudbake_src.txt")).read().strip() != _src_tag(ctx)
    except OSError:
        return True


def _build(ctx):
    if sb.take_kit_tool(ctx, "hudbake", os.path.join(ctx.work, "cs", "hudbake")):  # (s168) friend kit: prebuilt
        open(ctx.workp("hudbake_src.txt"), "w", newline="\n").write(_src_tag(ctx) + "\n")
        return
    ctx.need(ctx.dotnet, os.path.join(ctx.openkh, "OpenKh.Kh2.dll"))
    src = os.path.join(ctx.work, "cs-src", "hudbake")  # (s154 review S7) build a copy in work, never inside the repo
    for f in _SRC:
        ctx.copy(os.path.join(ctx.tools, "hud", "hudbake", f), os.path.join(src, f))
    proj = os.path.join(src, "hudbake.csproj")
    ctx.run([ctx.dotnet, "build", proj, "-c", "Release", "-o", os.path.join(ctx.work, "cs", "hudbake"),
             "-p:OpenKh=" + ctx.openkh, "--nologo", "-v", "q"])
    open(ctx.workp("hudbake_src.txt"), "w", newline="\n").write(_src_tag(ctx) + "\n")


def _text(ctx):
    t = os.path.join(ctx.tools, "hud", "text")
    orig = os.path.join(ctx.kh2, "kh2_first", "original")
    lv, dr = ctx.workp("levels/x"), ctx.workp("dream/x")
    lv, dr = os.path.dirname(lv), os.path.dirname(dr)
    ctx.py(os.path.join(t, "kh2_levels.py"), "--kh2", orig, "--out", lv)
    ctx.py(os.path.join(t, "kh2_dream_text.py"), "--extract", ctx.kh2, "--out", dr)
    ctx.py(os.path.join(t, "kh2_lvup_text.py"), "--kh2", orig, "--out", lv, "--levels", os.path.join(lv, "kh2_sora_levels.json"))


def _hb(cmd, out, *extra):
    def run(ctx):
        tmp = ctx.outp(out) + ".tmp"
        args = [_hudbake(ctx), cmd, tmp] + [e(ctx) if callable(e) else e for e in extra]
        ctx.run(args, env=_env(ctx))
        ctx.commit(tmp, ctx.outp(out))
    return run


def _pyb(script, out):
    def run(ctx):
        tmp = ctx.outp(out) + ".tmp"
        ctx.py(os.path.join(ctx.tools, "hud", "py", script), tmp, env=_env(ctx))
        ctx.commit(tmp, ctx.outp(out))
    return run


B = ["hud_build", "sounds_build"]
STEPS = [
    Step("hud_build", _build, work_outputs=["cs/hudbake/hudbake.exe", "hudbake_src.txt"]),
    Step("hud_text", _text, work_outputs=["levels/kh2_sora_levels.json", "levels/lvup_labels.txt", "dream/dream_labels.txt"]),
    Step("hud_hud", _hb("hud", "hud/hud.bin"), outputs=["hud/hud.bin"], deps=B),
    Step("hud_cmd", _hb("cmd", "hud/cmd.bin"), outputs=["hud/cmd.bin"], deps=B),
    Step("hud_lock", _hb("lock", "hud/lock.bin"), outputs=["hud/lock.bin"], deps=B),
    Step("hud_party", _hb("party", "hud/party.bin"), outputs=["hud/party.bin"], deps=B),
    Step("hud_drive", _hb("drive", "hud/drive.bin"), outputs=["hud/drive.bin"], deps=B),
    Step("hud_camp", _hb("camp", "hud/camp.bin", "0"), outputs=["hud/camp.bin"], deps=B),
    Step("hud_campseq", _hb("campseq", "hud/camp_seq.bin", lambda c: os.path.join(c.out, "hud", "camp_text.txt")),
         outputs=["hud/camp_seq.bin"], deps=B + ["camp_text"]),
    Step("hud_dream", _hb("dream", "hud/dream.bin", lambda c: os.path.join(c.work, "dream", "dream_labels.txt")),
         outputs=["hud/dream.bin"], deps=B + ["hud_text"]),
    Step("hud_lvup", _hb("lvup", "hud/lvup.bin", lambda c: os.path.join(c.work, "levels", "lvup_labels.txt")),
         outputs=["hud/lvup.bin"], deps=B + ["hud_text"]),
    Step("hud_roxface", _pyb("roxface_bake.py", "hud/roxface.bin"), outputs=["hud/roxface.bin"]),
    Step("hud_mkface", _pyb("mkface_bake.py", "hud/mkface.bin"), outputs=["hud/mkface.bin"]),
    Step("hud_sumface", _pyb("sumface_bake.py", "hud/sumface.bin"), outputs=["hud/sumface.bin"]),
    Step("hud_itempic", _pyb("itempic_bake.py", "hud/itempic.bin"), outputs=["hud/itempic.bin"]),
]
STEPS[0].stale = _stale

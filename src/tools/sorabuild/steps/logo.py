"""KAIRI logos (s182): data/logo/Logo3DGameTitle_00.bin (cutscene title), data/logo/Logo2D_00.bin (loading screen) and
data/overlay/Pack/Title.pack (title screen). Built from the player's OWN files: BotW romfs (Pack/Bootup.pack, Pack/Title.pack: the
kept words, the A, the flower, the TM and the Yaz0 pack around the title texture), their KH2 W_EX010 model (the Keyblade, via
`sorabake kbexport`, so cs_build must have run; W_EX010.mdlx is in kit/kh2_files.json) + OUR letters in
tools/sorabuild/logo/kairi_layers.npz. Pure numpy, no scipy, ~25 s. Byte-identical to the s110/s115/s135 dev files (logo/logo_build.py)."""
import os
import shutil

from sb import Step

OUTS = ["logo/Logo3DGameTitle_00.bin", "logo/Logo2D_00.bin", "overlay/Pack/Title.pack"]


def _logo(ctx):
    sorabake = os.path.join(ctx.work, "cs", "sorabake", "sorabake.exe")   # built / copied from the kit by step cs_build
    ctx.need(os.path.join(ctx.romfs, "Pack", "Bootup.pack"), os.path.join(ctx.romfs, "Pack", "Title.pack"),
             os.path.join(ctx.kh2, "kh2_first", "original", "obj", "W_EX010.mdlx"), sorabake)
    kb = os.path.join(ctx.work, "logo_kb")
    shutil.rmtree(kb, ignore_errors=True)
    ctx.run([sorabake, "kbexport", "W_EX010", kb], env={"SORABAKE_KH2": ctx.kh2})
    ctx.need(os.path.join(kb, "mesh.obj"), os.path.join(kb, "t0_256x128.rgba"))
    tmp = os.path.join(ctx.work, "logo_tmp")
    shutil.rmtree(tmp, ignore_errors=True)
    ctx.py(os.path.join(ctx.tools, "logo", "logo_build.py"), ctx.romfs, tmp, kb)
    for rel in OUTS:
        ctx.commit(os.path.join(tmp, rel), ctx.outp(rel))


STEPS = [Step("logo_build", _logo, outputs=OUTS, deps=["cs_build"])]

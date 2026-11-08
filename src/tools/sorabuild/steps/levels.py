"""BotW-derived level data (built from the player's own romfs)."""
import os
from sb import Step


def _gdt(ctx):
    ctx.need(os.path.join(ctx.romfs, "Pack", "Bootup.pack"))
    tmp = ctx.outp("levels/gamedata_flag_names.txt") + ".tmp"
    ctx.py(os.path.join(ctx.tools, "levels", "gdt_names.py"), ctx.romfs, tmp)
    ctx.commit(tmp, ctx.outp("levels/gamedata_flag_names.txt"))


STEPS = [Step("gdt_names", _gdt, outputs=["levels/gamedata_flag_names.txt"])]

"""Our own hand-made data files (no game data): copied from tools/sorabuild/ours/."""
import os
from sb import Step


def _camp_text(ctx):
    ctx.copy(os.path.join(ctx.tools, "ours", "hud", "camp_text.txt"), ctx.outp("hud/camp_text.txt"))


STEPS = [Step("camp_text", _camp_text, outputs=["hud/camp_text.txt"])]

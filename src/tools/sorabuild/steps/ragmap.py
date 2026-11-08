"""Skeleton data for the ragdoll / puppet retarget (s182): data/ragmap/{sora,roxas,mickey}.bin = the bone parents and bind-pose
positions of the player's OWN KH2 models (P_EX100 + W_EX010, P_EX110 + W_EX010, P_EX200 + W_EX200), so no KH2 model numbers sit in the
source. `sorabake ragmap` (the prebuilt kit tool, unchanged) writes the old C header into the work dir, ragmap/ragbin.py packs
its numbers into the small binary file the mod reads (runtime/sora_ragmap.h). Byte-identical to the former typed-in arrays."""
import os
import shutil

import sb
from sb import Step

OUTS = ["ragmap/sora.bin", "ragmap/roxas.bin", "ragmap/mickey.bin"]
# the same model envs as steps/bakes.py ROXAS / MICKEY (Mickey's idle is slot 0 of his mset, Sora's 2)
ENVS = {"sora": {},
        "roxas": {"SORABAKE_MODEL": "P_EX110", "SORABAKE_KB_MDLX": "W_EX010", "SORABAKE_KB_MSET": "W_EX010_RX", "SORABAKE_KB_JOINT": "172",
                  "SORABAKE_FORM": "roxas", "SORABAKE_PRTY": "1", "SORABAKE_PTYA": "9"},
        "mickey": {"SORABAKE_MODEL": "P_EX200", "SORABAKE_KB_MDLX": "W_EX200", "SORABAKE_KB_MSET": "W_EX200", "SORABAKE_KB_JOINT": "169",
                   "SORABAKE_FORM": "mickey", "SORABAKE_PRTY": "28", "SORABAKE_PTYA": "28", "SORABAKE_IDLE_SLOT": "0"}}


def exe(ctx, t):
    return os.path.join(ctx.work, "cs", t, t + ".exe")


def _ragmap(ctx):
    ctx.need(exe(ctx, "sorabake"))
    tmp = ctx.workp("ragmap_tmp/x")[:-2]
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    for who, env in ENVS.items():
        h = os.path.join(tmp, who + ".h")
        e = {"SORABAKE_KH2": ctx.kh2}
        e.update(env)
        ctx.run([exe(ctx, "sorabake"), "ragmap", h], cwd=tmp, env=e)
        ctx.need(h)
        ctx.py(os.path.join(sb.HERE, "ragmap", "ragbin.py"), h, os.path.join(tmp, who + ".bin"))
    for who in ENVS:
        ctx.commit(os.path.join(tmp, who + ".bin"), ctx.outp("ragmap/%s.bin" % who))
    shutil.rmtree(tmp, ignore_errors=True)


STEPS = [Step("ragmap_build", _ragmap, outputs=OUTS, deps=["cs_build"])]

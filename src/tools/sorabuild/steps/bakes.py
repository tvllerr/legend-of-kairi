"""Group 'bakes': the C# model / pose / motion / hit bakers (sorabake, partybake, gobake, skatebake) + the python slot merges.

Everything is rebuilt from the player's KH2 extract (ctx.kh2). Tools live in tools/sorabuild/cs/<tool>/ (built by step cs_build into
work/cs/<tool>/), python helpers in tools/sorabuild/bakes/, our own Keyblade alt lists in tools/sorabuild/ours/kb/.
Merge order (strict, each merge appends slots to the previous result): base bake -> s133 limits -> s149 knock -> s150 duck.
"""
import os
import shutil
import sys
import time

import sb
from sb import Step

CS_TOOLS = ["sorabake", "partybake", "gobake", "skatebake"]
BK = os.path.join(sb.HERE, "bakes")


# ---------------------------------------------------------------- tools
def _cs_build(ctx):
    if ctx.kit:  # (s168) friend kit: prebuilt tools, no .NET SDK
        for t in CS_TOOLS:
            sb.take_kit_tool(ctx, t, ctx.workp("cs/%s/x" % t)[:-2])
        return
    ctx.need(os.path.join(ctx.dotnet), os.path.join(ctx.openkh, "OpenKh.Kh2.dll"))
    env = {"DOTNET_ROOT": os.path.dirname(ctx.dotnet), "DOTNET_CLI_TELEMETRY_OPTOUT": "1", "DOTNET_NOLOGO": "1",
           "DOTNET_CLI_HOME": ctx.workp("cs-home/x")[:-2]}
    for t in CS_TOOLS:
        src = ctx.workp("cs-src/%s/x" % t)[:-2]
        if os.path.isdir(src):
            shutil.rmtree(src)
        shutil.copytree(os.path.join(sb.HERE, "cs", t), src)   # build outside the repo: no obj/ in git
        outd = ctx.workp("cs/%s/x" % t)[:-2]
        ctx.run([ctx.dotnet, "build", "-c", "Release", "-o", outd, "-p:OpenKhDir=" + ctx.openkh.replace("\\", "/")], cwd=src, env=env)
        ctx.need(os.path.join(outd, t + ".exe"))


def exe(ctx, t):
    return os.path.join(ctx.work, "cs", t, t + ".exe")


def kenv(ctx, env=None):
    e = {"SORABAKE_KH2": ctx.kh2}
    e.update(env or {})
    return e


def run_tool(ctx, tool, cmd, outs, env=None, extra=(), cwd=None):
    """tool <cmd> <tmp of every out> <extra...>; outs = final paths; each is committed (atomic rename) after a clean exit."""
    tmps = [o + ".tmp" for o in outs]
    for o in outs:
        os.makedirs(os.path.dirname(o), exist_ok=True)
    ctx.run([exe(ctx, tool)] + list(cmd if isinstance(cmd, (list, tuple)) else [cmd]) + tmps + list(extra),
            cwd=cwd or os.path.dirname(outs[0]), env=kenv(ctx, env))
    for t, o in zip(tmps, outs):
        ctx.need(t)
        ctx.commit(t, o)


def alts(ctx):
    r = {}
    for k, f in (("A1", "alt_main.txt"), ("A2", "alt_valor_left.txt")):
        with open(os.path.join(sb.HERE, "ours", "kb", f), encoding="utf-8") as fh:
            r[k] = fh.read().strip()
    return r


def wd(ctx, name):
    d = os.path.join(ctx.work, "bakes", name)
    os.makedirs(d, exist_ok=True)
    return d


def pymerge(ctx, kind, live, add, out):
    ctx.py(os.path.join(BK, "mergebake.py"), kind, live, add, out + ".tmp")
    ctx.commit(out + ".tmp", out)


# ---------------------------------------------------------------- Sora (actor 0)
SORA_ALT = {"SORABAKE_KB_ID": "1", "SORABAKE_MERGE": "1"}


def _sora_model(ctx):
    env = dict(SORA_ALT, SORABAKE_KB_ALT=alts(ctx)["A1"])
    run_tool(ctx, "sorabake", "model", [ctx.outp("sora_model.bin")], env)


def _sora_base(ctx):
    d = wd(ctx, "sora_base")
    run_tool(ctx, "sorabake", "bake", [os.path.join(d, "motion.bin"), os.path.join(d, "params.h")])
    run_tool(ctx, "sorabake", "pose", [os.path.join(d, "pose.bin")])


def _sora_hits(ctx):
    d = wd(ctx, "sora_hits")
    base, fb = os.path.join(d, "base.bin"), os.path.join(d, "fbody.bin")
    run_tool(ctx, "sorabake", "hitbake", [base])
    run_tool(ctx, "sorabake", "hitbake", [fb], {"SORABAKE_HIT_FBODY": "1"})
    out = ctx.outp("sora_hits.bin")
    # s106: every slot of the base bake + slots 664 668 684 788 from the FBODY bake
    ctx.py(os.path.join(BK, "merge_hits.py"), base, fb, out + ".tmp")
    ctx.commit(out + ".tmp", out)


def limit_set(ctx, d, bar, base, who):
    """Carve a Limit bar's motions of one participant into plain msets (mkmset.py) and bake them with partybake.
    who: list of (name, bodyname, weaponname, body model, weapon model, weapon joint, prty, slots...)"""
    for (n, body, wbar, model, weapon, wj, prty) in who:
        O = os.path.join(ctx.kh2, "kh2_sixth", "original", "obj")
        ms, mw = os.path.join(d, "%s_%s.mset" % (bar, n)), os.path.join(d, "%s_%s_w.mset" % (bar, n))
        ctx.py(os.path.join(BK, "mkmset.py"), bar, str(base), body, wbar, ms, mw, os.path.join(O, weapon + ".mset"),
               env={"SORABAKE_KH2": ctx.kh2})
        env = {"PARTYBAKE_MODEL": model, "PARTYBAKE_WEAPON": weapon, "PARTYBAKE_WJOINT": wj, "PARTYBAKE_PRTY": prty,
               "PARTYBAKE_MSET": ms, "PARTYBAKE_KBMSET": mw}
        run_tool(ctx, "partybake", "bake", [os.path.join(d, "%s_motion.bin" % n), os.path.join(d, "%s_params.h" % n)], env)
        run_tool(ctx, "partybake", "pose", [os.path.join(d, "%s_pose.bin" % n)], env)


def _lim133(ctx):    # s133: Comet (Donald) + Whirli-Goof (Sora, Goofy)
    d = wd(ctx, "s133")
    limit_set(ctx, d, "goofy", 1200, [("sora", "0010", "0011", "P_EX100", "W_EX010", "178", "1"),
                                      ("goofy", "0030", "0031", "P_EX030", "W_EX030", "101", "20")])
    limit_set(ctx, d, "donald", 1300, [("donald", "0020", "0021", "P_EX020", "W_EX020", "131", "17")])


def _knock149(ctx):  # s149: Knocksmash (goofy2.bar) slots 1400.. (Sora, Goofy)
    d = wd(ctx, "s149")
    limit_set(ctx, d, "goofy2", 1400, [("sora", "0010", "0011", "P_EX100", "W_EX010", "178", "1"),
                                       ("goofy", "0030", "0031", "P_EX030", "W_EX030", "101", "20")])


def _duck150(ctx):   # s150: Duck Flare (donald2.bar) slots 1500.. (Donald)
    d = wd(ctx, "s150")
    limit_set(ctx, d, "donald2", 1500, [("donald", "0020", "0021", "P_EX020", "W_EX020", "131", "17")])


def merge_chain(ctx, kind, base, adds, out):
    """base + each add in order -> out (intermediate results in work)"""
    cur = base
    for i, a in enumerate(adds):
        dst = out if i == len(adds) - 1 else ctx.workp("bakes/merge_tmp/%s/%s_%d_%s" % (ctx.step, os.path.basename(out), i, kind))
        pymerge(ctx, kind, cur, a, dst)
        cur = dst


def _sora_mp(ctx):
    for kind, nm in (("motion", "sora_motion.bin"), ("pose", "sora_pose.bin")):
        merge_chain(ctx, kind, os.path.join(wd(ctx, "sora_base"), nm.split("_")[1]),
                    [os.path.join(wd(ctx, "s133"), "sora_%s.bin" % kind), os.path.join(wd(ctx, "s149"), "sora_%s.bin" % kind)],
                    ctx.outp(nm))
    # the temp merge files are 80 MB each: drop them
    shutil.rmtree(os.path.join(ctx.work, "bakes", "merge_tmp", ctx.step), ignore_errors=True)   # (s168) per step: steps run in parallel


# ---------------------------------------------------------------- Donald / Goofy (party)
PARTY = {"donald": ("P_EX020", "W_EX020", "131", "17"), "goofy": ("P_EX030", "W_EX030", "101", "20")}


def party_env(who):
    m, w, j, p = PARTY[who]
    return {"PARTYBAKE_MODEL": m, "PARTYBAKE_WEAPON": w, "PARTYBAKE_WJOINT": j, "PARTYBAKE_PRTY": p}


def _party(who, adds):
    def run(ctx):
        d = wd(ctx, "party_" + who)
        env = party_env(who)
        run_tool(ctx, "partybake", "model", [ctx.outp("party/%s_model.bin" % who)], env)
        run_tool(ctx, "partybake", "hitbake", [ctx.outp("party/%s_hits.bin" % who)], env)
        run_tool(ctx, "partybake", "bake", [os.path.join(d, "motion.bin"), os.path.join(d, "params.h")], env)   # PARTYBAKE_SEEK default 1
        run_tool(ctx, "partybake", "pose", [os.path.join(d, "pose.bin")], env)
        for kind in ("motion", "pose"):
            merge_chain(ctx, kind, os.path.join(d, kind + ".bin"),
                        [os.path.join(wd(ctx, s), "%s_%s.bin" % (who, kind)) for s in adds], ctx.outp("party/%s_%s.bin" % (who, kind)))
        shutil.rmtree(os.path.join(ctx.work, "bakes", "merge_tmp", ctx.step), ignore_errors=True)   # (s168) per step: steps run in parallel
        if who == "goofy":   # the thrown shield: W_EX030 mset slot 804
            run_tool(ctx, "partybake", "wpose", [ctx.outp("party/goofy_weapon_flight.bin")], env, extra=["804"])
    return run


# ---------------------------------------------------------------- Drive Forms (valor wisdom limit master final anti)
# one dict per form: body mdlx/mset name, main weapon mset (mdlx W_EX010 on joint 178), optional left weapon mset (W_EX010_30 on joint 86), prty/ptya row, plrp
FORMS = {
    "valor":  dict(model="P_EX100_BTLF", kb="W_EX010_BTLF_R", kb2="W_EX010_BTLF_L", prty=2),
    "wisdom": dict(model="P_EX100_MAGF", kb="W_EX010_MAGF", kb2=None, prty=3),
    "limit":  dict(model="P_EX100_KH1F", kb="W_EX010_KH1F", kb2=None, prty=66, plrp="0x83"),
    "master": dict(model="P_EX100_TRIF", kb="W_EX010_TRIF_R", kb2="W_EX010_TRIF_L", prty=4, plrp="0x84"),
    "final":  dict(model="P_EX100_ULTF", kb="W_EX010_ULTF_R", kb2="W_EX010_ULTF_L", prty=5, plrp="0x85"),
    "anti":   dict(model="P_EX100_HTLF", kb="W_EX010", kb2=None, prty=6, plrp="0x86", nomesh=True),
}


def form_env(f, left=False, model=False, ctx=None):
    d = FORMS[f]
    e = {"SORABAKE_MODEL": d["model"], "SORABAKE_PRTY": d["prty"], "SORABAKE_PTYA": d["prty"], "SORABAKE_FORM": f}
    if d.get("plrp"):
        e["SORABAKE_PLRP"] = d["plrp"]
    if d.get("nomesh"):
        e["SORABAKE_KB_NOMESH"] = "1"
    else:
        if left:
            e.update(SORABAKE_KB_MDLX="W_EX010_30", SORABAKE_KB_MSET=d["kb2"], SORABAKE_KB_JOINT="86")
        else:
            e.update(SORABAKE_KB_MDLX="W_EX010", SORABAKE_KB_MSET=d["kb"], SORABAKE_KB_JOINT="178")
        if d["kb2"] and not left:
            e.update(SORABAKE_KB2_MDLX="W_EX010_30", SORABAKE_KB2_MSET=d["kb2"], SORABAKE_KB2_JOINT="86")
    if model and not d.get("nomesh"):
        a = alts(ctx)
        e.update(SORABAKE_KB_ID="1", SORABAKE_MERGE="1", SORABAKE_KB_ALT=a["A1"])
        if d["kb2"]:
            e.update(SORABAKE_KB2_ID="2", SORABAKE_KB2_ALT=a["A2"])
    return e


def _form(f, hits):
    """hits: None | 'single' (hitbake) | 'lr' (hitbake right + left blade) | 'body' (bodyhits)"""
    def run(ctx):
        d = wd(ctx, "form_" + f)
        o = lambda k: ctx.outp("forms/%s_%s.bin" % (f, k))
        run_tool(ctx, "sorabake", "bake", [o("motion"), os.path.join(d, "params.h")], form_env(f))
        run_tool(ctx, "sorabake", "model", [o("model")], form_env(f, model=True, ctx=ctx))
        run_tool(ctx, "sorabake", "pose", [o("pose")], form_env(f))
        if hits == "single":
            # limit: SORABAKE_HIT_BODY=1 also takes the body mset's own damaging windows (Zantetsuken K304, s89)
            run_tool(ctx, "sorabake", "hitbake", [o("hits")], dict(form_env(f), SORABAKE_HIT_BODY="1"))
        elif hits == "lr":
            run_tool(ctx, "sorabake", "hitbake", [o("hits_r")], form_env(f))
            run_tool(ctx, "sorabake", "hitbake", [o("hits_l")], form_env(f, left=True))
        elif hits == "body":
            run_tool(ctx, "sorabake", "bodyhits", [o("hits_r"), o("hits_l")], form_env(f))
    return run


def form_outputs(f, hits):
    r = ["forms/%s_%s.bin" % (f, k) for k in ("model", "pose", "motion")]
    if hits == "single":
        r.append("forms/%s_hits.bin" % f)
    elif hits:
        r += ["forms/%s_hits_%s.bin" % (f, k) for k in "lr"]
    return r


# ---------------------------------------------------------------- Roxas, Mickey, game over, skate
def _generic(name, tool_env, kind_hits=True, pose_slots=(), motion=True):
    def run(ctx):
        d = wd(ctx, name)
        o = lambda k: ctx.outp("forms/%s_%s.bin" % (name, k))
        run_tool(ctx, "sorabake", "bake", [o("motion"), os.path.join(d, "params.h")], tool_env)
        run_tool(ctx, "sorabake", "model", [o("model")], tool_env)
        run_tool(ctx, "sorabake", "hitbake", [o("hits")], tool_env)
        run_tool(ctx, "sorabake", "pose", [o("pose")], tool_env, extra=list(pose_slots))
    return run


ROXAS = {"SORABAKE_MODEL": "P_EX110", "SORABAKE_KB_MDLX": "W_EX010", "SORABAKE_KB_MSET": "W_EX010_RX", "SORABAKE_KB_JOINT": "172",
         "SORABAKE_FORM": "roxas", "SORABAKE_PRTY": "1", "SORABAKE_PTYA": "9"}
MICKEY = {"SORABAKE_MODEL": "P_EX200", "SORABAKE_KB_MDLX": "W_EX200", "SORABAKE_KB_MSET": "W_EX200", "SORABAKE_KB_JOINT": "169",
          "SORABAKE_FORM": "mickey", "SORABAKE_PRTY": "28", "SORABAKE_PTYA": "28"}


def _gameover(ctx):
    run_tool(ctx, "gobake", "pose", [ctx.outp("gameover_pose.bin")])


def _skate(ctx):
    d = wd(ctx, "skate")
    K1 = os.path.join(ctx.kh2, "kh2_first", "original", "obj")
    K6 = os.path.join(ctx.kh2, "kh2_sixth", "original", "obj")
    f = lambda k: ctx.outp("forms/skate_%s.bin" % k)
    hdr = os.path.join(d, "params.h")
    run_tool(ctx, "skatebake", "board", [f("board_model"), f("board_pose"), hdr + ".b"], {})
    run_tool(ctx, "skatebake", ["rider", os.path.join(K6, "P_EX100.mdlx"), os.path.join(K1, "F_TT010_SORA.mset")],
             [f("sora_model"), f("sora_pose"), hdr + ".s"], {}, extra=["SKB"])
    run_tool(ctx, "skatebake", ["rider", os.path.join(K1, "P_EX110.mdlx"), os.path.join(K1, "F_TT010.mset")],
             [f("rox_model"), f("rox_pose"), hdr + ".r"], {})


# ---------------------------------------------------------------- summons: chicken stitch peterpan tink genie
# model: obj name (kh2_sixth/kh2_first) or ('fifth', name); kbj: the dead weapon chain's parent bone; call: slots of the summon mset that
# become Sora-side call motions (flattened first, baked on Sora's own skeleton); body: pose slots of the summon body (None = all)
SUMMONS = {
    "chicken":  dict(model="P_EX350", kbj=None, call="252 253", body=None),
    "stitch":   dict(model=("fifth", "N_HB040_BTL"), kbj="0", call="252 253 254 255 256 257", body="0 201 202 205 206 231 241 242", flat2=True),
    "peterpan": dict(model="P_EX330", kbj=None, call="252 253 255 256", body=None),
    "tink":     dict(model="P_EX340", kbj="1", call="252", body="231 232"),
    "genie":    dict(model="P_AL010", kbj="1", call="252 253 255 257 259 261 263 265 267", body=None, genie=True),
}


def _summon(name):
    def run(ctx):
        c = SUMMONS[name]
        d = wd(ctx, "summon_" + name)
        o = lambda k: ctx.outp("forms/%s_%s.bin" % (name, k))
        if isinstance(c["model"], tuple):
            base = os.path.join(ctx.kh2, "kh2_fifth", "original", "obj", c["model"][1])
            mdl, mset = base + ".mdlx", base + ".mset"
        else:
            mdl, mset = c["model"], os.path.join(ctx.kh2, "kh2_sixth", "original", "obj", c["model"] + ".mset")
        env = {"SORABAKE_MODEL": mdl, "SORABAKE_KB_NOMESH": "1", "SORABAKE_FORM": name}
        if c["kbj"]:
            env["SORABAKE_KB_JOINT"] = c["kbj"]
        if isinstance(c["model"], tuple):
            env["SORABAKE_MSET"] = mset
        pose_slots = c["body"].split() if c["body"] else []
        model_out = o("model") if not c.get("genie") else os.path.join(d, "model_raw.bin")
        run_tool(ctx, "sorabake", "bake", [o("motion"), os.path.join(d, "params.h")], env)
        run_tool(ctx, "sorabake", "model", [model_out], env)
        run_tool(ctx, "sorabake", "hitbake", [o("hits")], env)
        run_tool(ctx, "sorabake", "pose", [o("pose")], env, extra=pose_slots)
        if c.get("genie"):
            out = o("model")
            ctx.py(os.path.join(BK, "genie_alt.py"), model_out, out + ".tmp")
            ctx.commit(out + ".tmp", out)
        flat = os.path.join(d, "flat.mset")
        ctx.py(os.path.join(BK, "flatten_summon_mset2.py" if c.get("flat2") else "flatten_summon_mset.py"), mset, flat, *c["call"].split())
        cenv = {"SORABAKE_MSET": flat, "SORABAKE_FORM": name + "call"}
        cf = lambda k: ctx.outp("forms/%scall_%s.bin" % (name, k))
        run_tool(ctx, "sorabake", "bake", [cf("motion"), os.path.join(d, "params_call.h")], cenv)
        run_tool(ctx, "sorabake", "pose", [cf("pose")], cenv, extra=c["call"].split())
    return run


def summon_outputs(n):
    return ["forms/%s_%s.bin" % (n, k) for k in ("model", "pose", "motion", "hits")] + ["forms/%scall_%s.bin" % (n, k) for k in ("pose", "motion")]


# ---------------------------------------------------------------- outfits: <outfit>_<body>_model.bin = the body's own model env with ONLY the body mdlx swapped
OUTFITS = ["nm", "xm", "tr", "wi"]      # Halloween Town, Christmas Town, Space Paranoids, Timeless River
BODIES = {"sora": "", "valor": "_BTLF", "wisdom": "_MAGF", "limit": "_KH1F", "master": "_TRIF", "final": "_ULTF", "anti": "_HTLF"}


def _outfits(ctx):
    for o in OUTFITS:
        for b, sfx in BODIES.items():
            if b == "sora":
                env = dict(SORA_ALT, SORABAKE_KB_ALT=alts(ctx)["A1"])
            else:
                env = form_env(b, model=True, ctx=ctx)
            env["SORABAKE_MODEL"] = "P_EX100_%s%s" % (o.upper(), sfx)
            env["SORABAKE_MSET"] = "P_EX100" + sfx      # outfit mdlx have no mset: the form's own (skeletons are equal)
            run_tool(ctx, "sorabake", "model", [ctx.outp("outfits/%s_%s_model.bin" % (o, b))], env)


def _final(part):
    """Final Form exists in three generations that are ALL live: the old files (fps x2 bake, SORABAKE_FPSFIX=0), *_s91 (fps fix, the
    default of the merged tool) and the *_s92 hit files (INTERVAL + RADIUS + FBODY knobs); runtime/sora.c ffps_path() picks them.
    (s168) one step per generation (form_final = model + old, form_final_s91, form_final_s92): they run in parallel."""
    def run(ctx):
        d = wd(ctx, "form_final")
        o = lambda k: ctx.outp("forms/final_%s.bin" % k)
        if part == "old":
            old = dict(form_env("final"), SORABAKE_FPSFIX="0")
            run_tool(ctx, "sorabake", "model", [o("model")], form_env("final", model=True, ctx=ctx))
            run_tool(ctx, "sorabake", "bake", [o("motion"), os.path.join(d, "params_old.h")], old)
            run_tool(ctx, "sorabake", "pose", [o("pose")], old)
            run_tool(ctx, "sorabake", "hitbake", [o("hits_r")], old)
            run_tool(ctx, "sorabake", "hitbake", [o("hits_l")], dict(form_env("final", left=True), SORABAKE_FPSFIX="0"))
        elif part == "s91":
            new = form_env("final")
            run_tool(ctx, "sorabake", "bake", [o("motion_s91"), os.path.join(d, "params_s91.h")], new)
            run_tool(ctx, "sorabake", "pose", [o("pose_s91")], new)
            run_tool(ctx, "sorabake", "hitbake", [o("hits_r_s91")], new)
            run_tool(ctx, "sorabake", "hitbake", [o("hits_l_s91")], form_env("final", left=True))
        else:
            new = form_env("final")
            k92 = {"SORABAKE_HIT_INTERVAL": "1", "SORABAKE_HIT_RADIUS": "1", "SORABAKE_HIT_FBODY": "1"}
            run_tool(ctx, "sorabake", "hitbake", [o("hits_r_s92")], dict(new, **k92))
            k92l = {"SORABAKE_HIT_INTERVAL": "1", "SORABAKE_HIT_RADIUS": "1"}   # the left blade file has NO FBODY (the body windows live in the right file only)
            run_tool(ctx, "sorabake", "hitbake", [o("hits_l_s92")], dict(form_env("final", left=True), **k92l))
    return run


STEPS = [
    Step("cs_build", _cs_build, work_outputs=["cs/sorabake/sorabake.exe", "cs/partybake/partybake.exe",
                                             "cs/gobake/gobake.exe", "cs/skatebake/skatebake.exe"]),
    Step("sora_model", _sora_model, outputs=["sora_model.bin"], deps=["cs_build"]),
    Step("sora_base", _sora_base, work_outputs=["bakes/sora_base/motion.bin", "bakes/sora_base/pose.bin"], deps=["cs_build"]),
    Step("sora_hits", _sora_hits, outputs=["sora_hits.bin"], deps=["cs_build"]),
    Step("lim133", _lim133, work_outputs=["bakes/s133/%s_%s.bin" % (n, k) for n in ("sora", "goofy", "donald") for k in ("motion", "pose")],
         deps=["cs_build"]),
    Step("knock149", _knock149, work_outputs=["bakes/s149/%s_%s.bin" % (n, k) for n in ("sora", "goofy") for k in ("motion", "pose")],
         deps=["cs_build", "lim133"]),
    Step("duck150", _duck150, work_outputs=["bakes/s150/donald_motion.bin", "bakes/s150/donald_pose.bin"], deps=["cs_build", "knock149"]),
    Step("sora_mp", _sora_mp, outputs=["sora_motion.bin", "sora_pose.bin"], deps=["sora_base", "lim133", "knock149"]),
    Step("party_donald", _party("donald", ["s133", "s150"]),
         outputs=["party/donald_%s.bin" % k for k in ("model", "hits", "motion", "pose")], deps=["cs_build", "lim133", "duck150"]),
    Step("party_goofy", _party("goofy", ["s133", "s149"]),
         outputs=["party/goofy_%s.bin" % k for k in ("model", "hits", "motion", "pose", "weapon_flight")], deps=["cs_build", "lim133", "knock149"]),
    Step("form_valor", _form("valor", "lr"), outputs=form_outputs("valor", "lr"), deps=["cs_build"]),
    Step("form_wisdom", _form("wisdom", None), outputs=form_outputs("wisdom", None), deps=["cs_build"]),
    Step("form_limit", _form("limit", "single"), outputs=form_outputs("limit", "single"), deps=["cs_build"]),
    Step("form_master", _form("master", "lr"), outputs=form_outputs("master", "lr"), deps=["cs_build"]),
    Step("form_anti", _form("anti", "body"), outputs=form_outputs("anti", "body"), deps=["cs_build"]),
    Step("form_roxas", _generic("roxas", ROXAS), outputs=["forms/roxas_%s.bin" % k for k in ("model", "pose", "motion", "hits")], deps=["cs_build"]),
    Step("form_mickey", _generic("mickey", MICKEY), outputs=["forms/mickey_%s.bin" % k for k in ("model", "pose", "motion", "hits")], deps=["cs_build"]),
    Step("gameover", _gameover, outputs=["gameover_pose.bin"], deps=["cs_build"]),
    Step("skate", _skate, outputs=["forms/skate_%s.bin" % k for k in ("board_model", "board_pose", "sora_model", "sora_pose", "rox_model", "rox_pose")],
         deps=["cs_build"]),
] + [Step("summon_" + n, _summon(n), outputs=summon_outputs(n), deps=["cs_build"]) for n in SUMMONS] + [
    Step("outfits", _outfits, outputs=["outfits/%s_%s_model.bin" % (o, b) for o in OUTFITS for b in BODIES], deps=["cs_build"]),
    Step("form_final", _final("old"), outputs=["forms/final_%s.bin" % k for k in ("model", "motion", "pose", "hits_r", "hits_l")],
         deps=["cs_build"]),
    Step("form_final_s91", _final("s91"), outputs=["forms/final_%s.bin" % k for k in ("motion_s91", "pose_s91", "hits_r_s91", "hits_l_s91")],
         deps=["cs_build"]),
    Step("form_final_s92", _final("s92"), outputs=["forms/final_%s.bin" % k for k in ("hits_r_s92", "hits_l_s92")], deps=["cs_build"]),
]

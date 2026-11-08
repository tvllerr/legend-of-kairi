"""sorabuild common code: config, context, steps.

A step module lives in steps/<group>.py and defines STEPS = [Step(...), ...].
  Step(name, run=fn(ctx), outputs=[paths relative to OUT], deps=[step names], work_outputs=[paths relative to WORK])
- run(ctx) writes its outputs under ctx.out (the data tree) or ctx.work (scratch: bake inputs, intermediate packs).
- Never hard-code a D:/ or C:/ path in a step: every input comes from ctx (ctx.kh2, ctx.fxcache, ctx.romfs, ctx.repo ...).
- A step is "done" when its marker exists and all its outputs exist; `all` skips done steps, a named step always re-runs.
- Write outputs atomically: build into a temp name, then ctx.commit(tmp, final).
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))  # src/sw-a41

CFG_KEYS = {
    "kh2": "KH2 extract root (OpenKH IdxImg layout: kh2_first..kh2_sixth/{original,remastered})",
    "fxcache": "dir with kh2_fx.bin + kh2_fx.json for config fx = phh (an old Project Heart Hunters 0.4.8 cache; with fx = fxrec the fx tools read work/fxrec/out and this is not needed). Old name: phh",
    "romfs": "BotW 1.9.0 romfs dir (Pack/Bootup.pack, Actor/, Map/ ...)",
    "out": "output data tree (what the game reads as <SORA_ROOT>/data)",
    "work": "scratch dir (intermediate files, logs, done markers)",
    "kh2exe": "KINGDOM HEARTS II FINAL MIX.exe of the Steam KH2 FM install (needed for config fx = fxrec and sounds = recipe (step sounds_mine: hud icon widths); sha1 2548F1191FE6587A34947952763CE6BB4F3B5C1A)",
    "dotnet": "dotnet.exe of a .NET 8 SDK (with a kit: the kit's .NET 8 runtime, only DOTNET_ROOT for the prebuilt tools)",
    "openkh": "dir with the OpenKH release DLLs (OpenKh.Kh2.dll, OpenKh.Common.dll, ...)",
    "kh2img": "(s168) instead of 'kh2': the Steam KH2 'Image/dt' dir (kh2_*.hed + .pkg); only the needed files are extracted to work/kh2",
    "kit": "(s168) friend kit dir made by kit/mkkit.py: prebuilt C# tools + fxrec_boot.exe + .NET runtime + busybox + ffmpeg + vgmstream; "
           "with it no .NET SDK / clang / MSYS2 / Git bash is needed (dotnet, openkh, bash, ffmpeg, vgmstream default to the kit's)",
}

# (s168) what each prebuilt kit tool is built from: kit.json holds src_hash() of these; a kit built from other sources is refused
KIT_SRC = {
    "sorabake": "cs/sorabake", "partybake": "cs/partybake", "gobake": "cs/gobake", "skatebake": "cs/skatebake",
    "hudbake": "hud/hudbake", "trigjson": "fx/trigjson", "fxrec_boot": "fxrec", "kh2pick": "cs/kh2pick",
}
FXREC_SRC = ["fxrec_boot.c", "fxrec_s2.c", "fxrec_ldr.c", "fxrec_ldr.h"]
FXREC_FLAGS = ["-O1", "-g", "-Wall", "-Wno-unused-function", "-Wl,--image-base,0x10000000", "-Wl,--disable-dynamicbase",
               "-Wl,--disable-high-entropy-va", "-Wl,--stack,0x4000000", "-lbcrypt"]


def fxrec_cmd(clang, exe):
    """the one clang command for fxrec_boot.exe (step fxrec_build and kit/mkkit.py)"""
    return [clang] + FXREC_FLAGS[:4] + ["-o", exe] + [os.path.join(HERE, "fxrec", n) for n in FXREC_SRC if n.endswith(".c")] + \
        FXREC_FLAGS[4:]


def src_files(tool):
    d = os.path.join(HERE, KIT_SRC[tool])
    if tool == "fxrec_boot":
        return [os.path.join(d, n) for n in FXREC_SRC]
    return sorted(os.path.join(d, n) for n in os.listdir(d) if os.path.isfile(os.path.join(d, n)))


def src_hash(tool):
    """sha256 over (name, CRLF->LF content) of a tool's sources (+ the clang flags for fxrec_boot)"""
    h = hashlib.sha256()
    for p in src_files(tool):
        h.update(os.path.basename(p).encode() + b"\0" + open(p, "rb").read().replace(b"\r\n", b"\n") + b"\0")
    if tool == "fxrec_boot":
        h.update(" ".join(FXREC_FLAGS).encode())
    return h.hexdigest()


def kit_tool(ctx, tool):
    """prebuilt dir of a tool in the kit (checked against the repo sources), or None without a kit"""
    if not ctx.kit:
        return None
    try:
        kj = json.load(open(os.path.join(ctx.kit, "kit.json"), encoding="utf-8"))
    except OSError:
        raise SystemExit("[sorabuild] kit %s has no kit.json (make it with kit/mkkit.py)" % ctx.kit)
    want, got = src_hash(tool), kj.get("src", {}).get(tool)
    if got != want:
        raise SystemExit("[sorabuild] kit tool %s was built from other sources (kit %s, repo %s): rebuild the kit with kit/mkkit.py"
                         % (tool, (got or "none")[:12], want[:12]))
    d = os.path.join(ctx.kit, "tools", tool)
    exe = os.path.join(d, tool + ".exe")
    if not os.path.isfile(exe):
        raise SystemExit("[sorabuild] kit is missing %s" % exe)
    return d


KH2_LIST = os.path.join(HERE, "kit", "kh2_files.json")


def pick_kh2(cfg):
    """(s168) config "kh2img" = the Steam KH2 'Image/dt' dir (kh2_*.hed/.pkg): extract ONLY the files of kit/kh2_files.json into
    work/kh2 (kit tool kh2pick, every sha256 checked) and use that as "kh2". Re-runs when the list changes."""
    img = cfg.get("kh2img")
    if not img or cfg.get("kh2"):
        return
    if not cfg.get("kit"):
        raise SystemExit("[sorabuild] config kh2img needs a kit (tools/kh2pick)")
    dst = os.path.join(cfg["work"], "kh2")
    cfg["kh2"] = dst
    tag = sha256(KH2_LIST)
    mark = os.path.join(cfg["work"], "done", "_kh2pick")
    try:
        if open(mark).read().strip() == tag:
            return
    except OSError:
        pass
    exe = os.path.join(cfg["kit"], "tools", "kh2pick", "kh2pick.exe")
    e = dict(os.environ, DOTNET_ROOT=os.path.dirname(cfg["dotnet"]))
    os.makedirs(os.path.join(cfg["work"], "logs"), exist_ok=True)
    os.makedirs(os.path.dirname(mark), exist_ok=True)
    t = time.time()
    print("[sorabuild] kh2pick: extracting the needed KH2 files from %s" % img, flush=True)
    with open(os.path.join(cfg["work"], "logs", "_kh2pick.log"), "w", encoding="utf-8") as f:
        r = subprocess.run([exe, img, KH2_LIST, dst], env=e, stdout=f, stderr=subprocess.STDOUT)
    logf = os.path.join(cfg["work"], "logs", "_kh2pick.log")
    if r.returncode == 1:  # kh2pick: 1 = a listed file is missing or its sha256 differs
        raise SystemExit("[sorabuild] kh2pick: your KH2 files differ from the Steam KH2 FM release (modded or another version? "
                         "a KH2 mod installed into the .pkg files changes them) - see %s" % logf)
    if r.returncode:
        raise SystemExit("[sorabuild] kh2pick crashed (%d) - see %s" % (r.returncode, logf))
    open(mark, "w").write(tag + "\n")
    print("[sorabuild] kh2pick: done %.1f s" % (time.time() - t), flush=True)


def take_kit_tool(ctx, tool, dst):
    """with a kit: copy its prebuilt <tool> dir to dst (same place the build would write) and return True; else False"""
    d = kit_tool(ctx, tool)
    if d is None:
        return False
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    shutil.copytree(d, dst)
    ctx.log("prebuilt %s from the kit (sources %s)" % (tool, src_hash(tool)[:12]))
    return True


LEAK_PREFIXES = ("SORABAKE_", "PARTYBAKE_", "SORA_", "FXBAKE_", "LVUP_", "CMDTEST_", "HUDBAKE_", "SORABUILD_", "SB_")

# (s154 review S4) one probe file per input, checked before any step runs
PROBES = {"kh2": "kh2_first/original/00battle.bin", "fxcache": "kh2_fx.json", "romfs": "Pack/Bootup.pack",
          "openkh": "OpenKh.Kh2.dll", "dotnet": ""}


class Step:
    def __init__(self, name, run, outputs=(), deps=(), work_outputs=(), group="", chain=None):
        self.name, self.run, self.deps, self.chain = name, run, list(deps), chain
        self.outputs, self.work_outputs, self.group = list(outputs), list(work_outputs), group


class Ctx:
    def __init__(self, cfg):
        self.cfg = cfg
        for k in CFG_KEYS:
            setattr(self, k, cfg.get(k, ""))
        self.repo = REPO
        self.tools = HERE
        self.python = sys.executable
        self.step = None
        for d in (self.out, self.work, os.path.join(self.work, "logs"), os.path.join(self.work, "done")):
            os.makedirs(d, exist_ok=True)

    # sounds switch (s156, default flipped s163): config "sounds": "recipe" (default) = work/sounds_recipe.json (step sounds_mine, s182) + the player's own KH2 SCD files (no PHH cache for
    # sounds / mset triggers / hud widths); "phh" (opt-in, deprecated) = the PHH 0.4.8 cache as before
    @property
    def snd_mode(self):
        m = self.cfg.get("sounds", "recipe")
        if m not in ("phh", "recipe"):
            raise SystemExit("[sorabuild] config 'sounds' must be 'phh' or 'recipe', not %r" % (m,))
        return m

    # fx switch (s161): config "fx": "fxrec" = kh2_fx.bin / kh2_fx.json recorded by OUR fxrec from the player's own KH2 exe + extract
    # (step fxrec_record -> work/fxrec/out); "phh" (default) = the PHH cache as before.  Only kh2_fx.* move; sounds have their own switch.
    @property
    def fx_mode(self):
        m = self.cfg.get("fx", "phh")
        if m not in ("phh", "fxrec"):
            raise SystemExit("[sorabuild] config 'fx' must be 'phh' or 'fxrec', not %r" % (m,))
        return m

    # fx_keep (s166): "used" (default) = the fxrec recorder records only the KH2 effects the fx chain tools read (keep list derived from our
    # own tool tables by fx/fxcensus.py, no game bytes); "all" = every recording of the 82 files (the s164 full run, ~1 h)
    @property
    def fx_keep(self):
        m = self.cfg.get("fx_keep", "used")
        if m not in ("used", "all"):
            raise SystemExit("[sorabuild] config 'fx_keep' must be 'used' or 'all', not %r" % (m,))
        return m

    def fxdir(self):
        """dir with kh2_fx.bin + kh2_fx.json for the fx tools"""
        if self.fx_mode == "phh":
            return self.fxcache
        if self.cfg.get("fxrec_only"):
            raise SystemExit("[sorabuild] config 'fxrec_only' is a test filter: the subset kh2_fx.bin cannot feed the full fx chain "
                             "(run the single tools by hand against %s)" % os.path.join(self.work, "fxrec", "out"))
        return os.path.join(self.work, "fxrec", "out")

    def sndroot(self):
        """dir with mined/ bdx/ kh2_sfx/ kh2_hud.json: work/sounds (recipe) or the PHH cache"""
        return os.path.join(self.work, "sounds") if self.snd_mode == "recipe" else self.fxcache

    def snd_env(self):
        e = {"SB_SNDROOT": self.sndroot()} if self.snd_mode == "recipe" else {}
        if self.cfg.get("wavtrace"):  # dev only: log every wav the tools open (sounds_mine traces its own run without this)
            e["SB_WAVTRACE"] = self.cfg["wavtrace"]
        return e

    # paths
    def outp(self, rel):
        p = os.path.join(self.out, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        return p

    def workp(self, rel):
        p = os.path.join(self.work, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        return p

    def need(self, *paths):
        for p in paths:
            if not os.path.exists(p):
                raise SystemExit("[sorabuild] %s: missing input %s" % (self.step, p))

    def commit(self, tmp, final):
        os.makedirs(os.path.dirname(final), exist_ok=True)
        os.replace(tmp, final)

    # logging + running
    def _logf(self):
        return os.path.join(self.work, "logs", "%s.log" % self.step)

    def log(self, msg):
        line = "[%s] %s" % (self.step, msg)
        print(line, flush=True)
        with open(self._logf(), "a", encoding="utf-8") as f:
            f.write(line + "\n")

    def run(self, args, cwd=None, env=None):
        """Run a command; its output goes to work/logs/<step>.log. Raises on a non-zero exit."""
        args = [str(a) for a in args]
        # (s154 review S2) old bake knobs left in a dev shell must not change the bytes: only the step env sets them
        e = {k: v for k, v in os.environ.items() if not k.upper().startswith(LEAK_PREFIXES)}
        # (s166) the baked .NET apps (sorabake.exe ...) find the runtime only through DOTNET_ROOT when .NET is not
        # installed system-wide (friend PC, Deck/Proton): always point it at the config's SDK
        if self.dotnet:
            e["DOTNET_ROOT"] = os.path.dirname(self.dotnet)
        if env:
            e.update({k: str(v) for k, v in env.items()})
        envs = ("  (env " + " ".join("%s=%s" % kv for kv in env.items()) + ")") if env else ""
        self.log("$ " + " ".join(args) + envs)
        t = time.time()
        with open(self._logf(), "a", encoding="utf-8") as f:
            r = subprocess.run(args, cwd=cwd, env=e, stdout=f, stderr=subprocess.STDOUT)
        if r.returncode:
            raise SystemExit("[sorabuild] %s: command failed (%d): %s  -> see %s" % (
                self.step, r.returncode, " ".join(args), self._logf()))
        self.log("  ok %.1f s" % (time.time() - t))

    def py(self, script, *args, cwd=None, env=None):
        """Run a repo python script: -E -P = no PYTHON* env, no script dir / cwd on sys.path (like -I), but the
        user site-packages stay visible (numpy / Pillow live there on this PC; -I hides them)."""
        self.run([self.python, "-E", "-P", script] + list(args), cwd=cwd, env=env)

    def copy(self, src, dst):
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)


def free_mb():
    """(s168) available physical RAM in MB: Windows GlobalMemoryStatusEx (also under Wine/Proton), Linux MemAvailable; None if unknown"""
    try:
        if sys.platform == "win32":
            import ctypes

            class MS(ctypes.Structure):
                _fields_ = [("len", ctypes.c_uint32), ("load", ctypes.c_uint32)] + [(n, ctypes.c_uint64) for n in
                            ("total", "avail", "ptotal", "pavail", "vtotal", "vavail", "xavail")]
            m = MS()
            m.len = ctypes.sizeof(MS)
            return m.avail // (1 << 20) if ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(m)) else None
        with open("/proc/meminfo") as f:
            for ln in f:
                if ln.startswith("MemAvailable:"):
                    return int(ln.split()[1]) // 1024
    except Exception:
        return None
    return None


def sha256(path, bs=1 << 22):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            b = f.read(bs)
            if not b:
                return h.hexdigest()
            h.update(b)


def load_cfg(path, overrides):
    cfg = {}
    if path:
        with open(path, encoding="utf-8") as f:
            cfg = json.load(f)
    for k, v in overrides.items():
        if v:
            cfg[k] = v
    # (s163) config key "phh" was renamed "fxcache"; the old name still works (and so does --phh / SB_PHH)
    if cfg.get("phh") and not cfg.get("fxcache"):
        cfg["fxcache"] = cfg["phh"]
    if cfg.get("vorbis_fallback") == "phh":
        raise SystemExit("[sorabuild] config vorbis_fallback \"phh\" was removed: install vgmstream-cli r2117 (python -E -P sounds/fetch_vgmstream.py, or config \"vgmstream\") for the Ogg Vorbis voices")
    if cfg.get("kit"):  # (s168) kit defaults; an explicit config value still wins
        kd = cfg["kit"]
        for k, rel in (("dotnet", "dotnet/dotnet.exe"), ("openkh", "openkh"), ("bash", "busybox/sh.exe"),
                       ("ffmpeg", "ffmpeg/ffmpeg.exe"), ("vgmstream", "vgmstream/vgmstream-cli.exe")):
            cfg.setdefault(k, os.path.join(kd, rel).replace("\\", "/"))
    for k in ("out", "work"):
        if not cfg.get(k):
            raise SystemExit("[sorabuild] config needs '%s' (%s)" % (k, CFG_KEYS[k]))
    return cfg


def check_inputs(cfg):
    bad = []
    # (s161) the PHH cache is optional once fx = fxrec and sounds = recipe (without the vorbis fallback copy)
    no_phh = cfg.get("fx") == "fxrec" and cfg.get("sounds", "recipe") == "recipe"
    for k, probe in PROBES.items():
        if k == "fxcache" and no_phh:
            continue
        v = cfg.get(k, "")
        p = os.path.join(v, probe) if probe else v
        if not v or not os.path.exists(p):
            bad.append("  %-7s %s  (needs %s)" % (k, v or "<not set>", p if v else CFG_KEYS[k]))
    if (cfg.get("fx") == "fxrec" or cfg.get("sounds", "recipe") == "recipe") and not (cfg.get("kh2exe") and os.path.isfile(cfg["kh2exe"])):
        bad.append("  kh2exe  %s  (needs the Steam KH2 FM exe for config fx = fxrec / sounds = recipe)" % (cfg.get("kh2exe") or "<not set>"))
    if bad:
        raise SystemExit("[sorabuild] inputs missing:" + chr(10) + chr(10).join(bad))

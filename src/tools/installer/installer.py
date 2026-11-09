"""Legend of Kairi installer (Sora in BotW, s168): a local web page shown in an Edge app window, driven by the kit's own Python.

  <kit>/python/pythonw.exe -E -P tools/installer/installer.py [--root <install dir>] [--kit <kit dir>]
        [--kh2img <Image/dt override, dev>] [--port N] [--browser]

- Serves tools/installer/ui on 127.0.0.1 (random port, a random token guards the API) and opens it with
  msedge --app (own profile in <root>/.installer-ui, so the window is its own process: closing it ends the installer
  and stops a running build; the build resumes from its done markers next time).
- The player picks two folders: Savage Wind 0.4.1-alpha.4 (with its game files set up: extracted/romfs) and the Steam
  KINGDOM HEARTS HD 1.5+2.5 ReMIX folder (found through Steam by itself when it can). Nothing else is asked.
- Install = tools/sorabuild/sorabuild.py all (kit mode, kh2img) into <root>/data with scratch <root>/work, then
  verify.py --expect manifest.json (every file = the reference build). Game files are only read, never written.
- (s183) Update: the player can pick an OLDER Legend of Kairi folder instead; this release's program files replace the old ones there
  and only the data files that changed are rebuilt (update.py), then the same build / verify / game side run in that folder.
Paths: <root> defaults to the folder above tools/ (the unpacked friend zip); the kit to <root>/kit.
"""
import argparse
import collections
import hashlib
import http.server
import json
import mimetypes
import os
import re
import secrets
import shutil
import socketserver
import subprocess
import sys
import threading
import time
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
UI = os.path.join(HERE, "ui")
SBDIR = os.path.normpath(os.path.join(HERE, "..", "sorabuild"))
DEFAULT_VERSION = "0.1.0-alpha"   # same default as mkzip --version; the release's manifest.json wins
VERSION = DEFAULT_VERSION + "-dev"
KH2_DIRNAME = "KINGDOM HEARTS -HD 1.5+2.5 ReMIX-"
KH2_EXE = "KINGDOM HEARTS II FINAL MIX.exe"
KH2_EXE_SHA1 = "2548F1191FE6587A34947952763CE6BB4F3B5C1A"
SW_VERSION = "0.4.1-alpha.4"
GAME_VERSION = "1.9.0"      # the BotW version the mod is built for
NEED_UPDATE_GB = 5         # (s183) stage + old copies of the program files (~1.1 GB) + the steps that re-run (GUESS: fxrec needs more)
RELEASE = os.path.normpath(os.path.join(HERE, "..", ".."))   # this download's folder (bin/ kit/ tools/ manifest.json)
NEED_DISK_GB = 15          # s168kit4 (exact KH2 list): work 4.8 GB + data 2.8 GB at the end; + room for the fx recordings while they run (GUESS)
WINDOW_TITLE = "Legend of Kairi"

sys.path.insert(0, SBDIR)
sys.path.insert(0, HERE)
import gameside   # noqa: E402  (the last step: bin/ + the two launchers)
import update     # noqa: E402  (s183: update an older install in place)
try:
    import sorabuild as _sbmain   # EST (seconds per step) for the progress weights
    EST = dict(_sbmain.EST)
except Exception:
    EST = {}

PHASES = [("kh2", "Unpack the KH2 files the mod needs"), ("bakes", "Bake Sora, the Drive Forms, the party and the summons"),
          ("fxrec", "Record the KH2 effects"), ("fx", "Build the effect and sound packs"),
          ("other", "Build the HUD, sounds, text and logos"), ("verify", "Check every file"),
          ("game", "Set up the game program and the launchers")]
GROUP_PHASE = {"bakes": "bakes", "fxrec": "fxrec", "fx": "fx", "hud": "other", "levels": "other", "logo": "other", "ragmap": "other", "ours": "other", "sounds": "other"}
PHASE_W = {"kh2": 5, "verify": 25, "game": 2, "copy": 3}
COPY_PHASE = ("copy", "Bring the program files up to date")     # (s183) update only, before the others
PAR = 2.8    # (s168) serial step seconds / wall seconds of the parallel build (s168kit3: 36.5 min of steps in 12.9 min, jobs 3)


# ------------------------------------------------------------------------------------------------ checks
def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest().upper()


def check_sw(path):
    """-> (ok, message, info). Savage Wind folder: savagewind.exe + extracted/romfs (its own 'set up game files' done)."""
    if not path:
        return False, "Choose your Savage Wind folder.", {}
    if not path.isascii():
        return False, ("This folder's path has letters Windows programs here cannot handle. Move your Savage Wind folder to a path "
                       "with plain letters only, for example C:/Games/Savage Wind, then choose it again."), {}
    if not os.path.isfile(os.path.join(path, "savagewind.exe")):
        return False, "No savagewind.exe in this folder. Choose the folder you unpacked Savage Wind %s into." % SW_VERSION, {}
    readme = os.path.join(path, "README.md")
    try:
        txt = open(readme, encoding="utf-8", errors="replace").read()
    except OSError:
        txt = ""
    if "SavageWind-%s" % SW_VERSION not in txt:
        return False, "This is not Savage Wind %s. Legend of Kairi works with that version only." % SW_VERSION, {}
    data = os.environ.get("SAVAGEWIND_DATA_ROOT") or path
    romfs = os.path.join(data, "extracted", "romfs")
    if not os.path.isfile(os.path.join(romfs, "Pack", "Bootup.pack")):
        return False, ("Savage Wind %s is here, but its game files are not set up yet. Start Savage Wind once, choose your "
                       "game files folder and let it finish, then come back." % SW_VERSION), {}
    ver = game_version(romfs)
    if ver and ver != GAME_VERSION:
        return False, "Legend of Kairi needs Breath of the Wild %s; your Savage Wind has %s." % (GAME_VERSION, ver), {}
    msg = "Savage Wind %s with your game files." % SW_VERSION
    if not ver:
        msg += " (Could not read the game version: Legend of Kairi needs Breath of the Wild %s.)" % GAME_VERSION
    return True, msg, {"romfs": romfs.replace("\\", "/"), "game_version": ver}


def game_version(romfs):
    """the BotW version of the extracted game files: romfs/System/Version.txt (the file Savage Wind itself reads), '' when unreadable"""
    try:
        with open(os.path.join(romfs, "System", "Version.txt"), "rb") as f:
            raw = f.read(64)
    except OSError:
        return ""
    m = re.fullmatch(r"(\d+(?:\.\d+){1,3})", raw.decode("ascii", "replace").strip())
    return m.group(1) if m else ""


def check_kh2(path):
    """-> (ok, message, info). The Steam KINGDOM HEARTS HD 1.5+2.5 ReMIX folder (Image/dt + the KH2 FM exe)."""
    if not path:
        return False, "Choose your KINGDOM HEARTS HD 1.5+2.5 ReMIX folder (Steam).", {}
    p = path
    if os.path.basename(p.rstrip("/\\")).lower() == "dt":           # picked Image/dt itself
        p = os.path.dirname(os.path.dirname(p.rstrip("/\\")))
    elif os.path.basename(p.rstrip("/\\")).lower() == "image":
        p = os.path.dirname(p.rstrip("/\\"))
    exe, dt = os.path.join(p, KH2_EXE), os.path.join(p, "Image", "dt")
    if not os.path.isfile(exe) or not os.path.isfile(os.path.join(dt, "kh2_first.hed")):
        return False, "This is not the KINGDOM HEARTS HD 1.5+2.5 ReMIX folder (no %s / Image/dt)." % KH2_EXE, {}
    if sha1(exe) != KH2_EXE_SHA1:
        return False, ("Your %s is not the Steam release this mod is made for (Epic and other versions are not supported "
                       "yet)." % KH2_EXE), {}
    return True, "KINGDOM HEARTS HD 1.5+2.5 ReMIX (Steam).", {"exe": exe.replace("\\", "/"), "img": dt.replace("\\", "/"),
                                                              "dir": p.replace("\\", "/")}


def steam_kh2():
    """the Steam KH2 folder, from the registry + libraryfolders.vdf, or ''"""
    if sys.platform != "win32":
        return ""
    try:
        import winreg
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as k:
            steam = winreg.QueryValueEx(k, "SteamPath")[0]
    except OSError:
        return ""
    libs = [steam]
    try:
        vdf = open(os.path.join(steam, "steamapps", "libraryfolders.vdf"), encoding="utf-8", errors="replace").read()
        libs += [m.replace("\\\\", "\\") for m in re.findall(r'"path"\s+"([^"]+)"', vdf)]
    except OSError:
        pass
    for lib in libs:
        d = os.path.join(lib, "steamapps", "common", KH2_DIRNAME)
        if os.path.isfile(os.path.join(d, KH2_EXE)):
            return os.path.normpath(d).replace("\\", "/")
    return ""


def free_gb(path):
    p = path
    while p and not os.path.exists(p):
        p = os.path.dirname(p)
    try:
        return shutil.disk_usage(p or ".").free / 2**30
    except OSError:
        return None


# ------------------------------------------------------------------------------------------------ folder dialog (Windows)
def pick_folder(title, start=""):
    """the Windows folder picker (IFileOpenDialog, FOS_PICKFOLDERS) owned by our window; '' when cancelled"""
    import ctypes
    from ctypes import POINTER, byref, c_long, c_uint32, c_void_p, c_wchar_p, cast
    ole32, shell32, user32 = ctypes.windll.ole32, ctypes.windll.shell32, ctypes.windll.user32
    ole32.CoInitializeEx(None, 2)

    class GUID(ctypes.Structure):
        _fields_ = [("a", c_uint32), ("b", ctypes.c_uint16), ("c", ctypes.c_uint16), ("d", ctypes.c_ubyte * 8)]

    def guid(s):
        g = GUID()
        ole32.CLSIDFromString(c_wchar_p(s), byref(g))
        return g

    def call(obj, idx, *args, argtypes=()):
        vt = cast(obj, POINTER(POINTER(c_void_p)))[0]
        fn = ctypes.WINFUNCTYPE(c_long, c_void_p, *argtypes)(vt[idx])
        return fn(obj, *args) & 0xFFFFFFFF

    dlg = c_void_p()
    if ole32.CoCreateInstance(byref(guid("{DC1C5A9C-E88A-4dde-A5A1-60F82A20AEF7}")), None, 1,
                              byref(guid("{d57c7288-d4ad-4768-be02-9d969532d960}")), byref(dlg)) & 0xFFFFFFFF:
        raise OSError("folder dialog not available")
    try:
        opts = c_uint32()
        call(dlg, 10, byref(opts), argtypes=(POINTER(c_uint32),))
        call(dlg, 9, opts.value | 0x20 | 0x40 | 0x800, argtypes=(c_uint32,))     # PICKFOLDERS FORCEFILESYSTEM PATHMUSTEXIST
        call(dlg, 17, title, argtypes=(c_wchar_p,))
        if start and os.path.isdir(start):
            item = c_void_p()
            if not shell32.SHCreateItemFromParsingName(c_wchar_p(os.path.normpath(start)), None,
                                                       byref(guid("{43826d1e-e718-42ee-bc55-a1e261c37bfe}")), byref(item)):
                call(dlg, 12, item, argtypes=(c_void_p,))
                call(item, 2)
        owner = user32.FindWindowW(None, c_wchar_p(WINDOW_TITLE))
        hr = call(dlg, 3, owner or None, argtypes=(c_void_p,))
        if hr == 0x800704C7:        # cancelled
            return ""
        if hr:
            raise OSError("folder dialog failed (%08x)" % hr)
        res = c_void_p()
        hr = call(dlg, 20, byref(res), argtypes=(POINTER(c_void_p),))
        if hr or not res.value:
            raise OSError("folder dialog gave no result (%08x)" % hr)
        name = c_wchar_p()
        call(res, 5, 0x80058000, byref(name), argtypes=(c_uint32, POINTER(c_wchar_p)))     # SIGDN_FILESYSPATH
        out = name.value or ""
        ole32.CoTaskMemFree(name)
        call(res, 2)
        return out.replace("\\", "/")
    finally:
        call(dlg, 2)


# ------------------------------------------------------------------------------------------------ job object (Windows)
def make_job():
    """(s168 review) a job with KILL_ON_JOB_CLOSE: every build process (and the tools it starts) dies with the installer,
    even when the installer is killed or crashes, so a later Install never races an orphaned build"""
    if sys.platform != "win32":
        return None
    import ctypes
    from ctypes import wintypes
    k32 = ctypes.windll.kernel32
    k32.CreateJobObjectW.restype = wintypes.HANDLE

    class BASIC(ctypes.Structure):
        _fields_ = [("a", ctypes.c_int64), ("b", ctypes.c_int64), ("LimitFlags", wintypes.DWORD), ("c", ctypes.c_size_t),
                    ("d", ctypes.c_size_t), ("e", wintypes.DWORD), ("f", ctypes.c_size_t), ("g", wintypes.DWORD), ("h", wintypes.DWORD)]

    class EXT(ctypes.Structure):
        _fields_ = [("basic", BASIC), ("io", ctypes.c_uint64 * 6), ("p1", ctypes.c_size_t), ("p2", ctypes.c_size_t),
                    ("p3", ctypes.c_size_t), ("p4", ctypes.c_size_t)]
    job = k32.CreateJobObjectW(None, None)
    if not job:
        return None
    info = EXT()
    info.basic.LimitFlags = 0x2000          # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
    if not k32.SetInformationJobObject(wintypes.HANDLE(job), 9, ctypes.byref(info), ctypes.sizeof(info)):   # extended limits
        return None
    return job


def job_add(job, proc):
    if job and sys.platform == "win32":
        import ctypes
        from ctypes import wintypes
        ctypes.windll.kernel32.AssignProcessToJobObject(wintypes.HANDLE(job), wintypes.HANDLE(int(proc._handle)))


def recover_after_bad_verify(root, bad_lines, prev_bad=None):
    """(s178 M2) verify.py printed DIFF / MISSING / NOT IN lines ("<group> <file> <status>"). The done markers would make every later
    Install skip all steps and fail the same way, so: delete the wrong data files (a step whose output is gone counts as not done and
    re-runs, with the steps that depend on it). Nothing identifiable -> clear every marker. -> (plain message, set of the bad files).
    The same files wrong again after a rebuild -> this PC builds them differently (not a broken file): say so."""
    root = root.replace("\\", "/")
    files = set()
    for l in bad_lines:
        m = re.match(r"\S+\s+(\S+)\s+(DIFF|MISSING|NOT IN)", l)
        if m:
            files.add(m.group(1))
    removed = 0
    for rel in sorted(files):
        p = os.path.normpath(os.path.join(root, "data", rel))
        if os.path.commonpath([p, os.path.normpath(os.path.join(root, "data"))]) != os.path.normpath(os.path.join(root, "data")):
            continue
        try:
            os.remove(p)
            removed += 1
        except OSError:
            pass
    if not files:
        done = os.path.join(root, "work", "done")
        if os.path.isdir(done):
            for n in os.listdir(done):
                try:
                    os.remove(os.path.join(done, n))
                except OSError:
                    pass
    if prev_bad and files and files <= prev_bad:
        return ("Some files still did not come out right after rebuilding them. This PC seems to build them differently. "
                "Please send the files in the work\\logs folder to the mod author (tvllerr). (%d file(s): %s)" % (len(files), ", ".join(sorted(files)[:3])), files)
    return ("Some files did not come out right; press Install again to rebuild them. (%d file(s): %s)"
            % (len(files), ", ".join(sorted(files)[:3])), files)


def update_after_bad_verify(root, msg):
    """(s183) verify failed after an update: old work/ files may not fit the new steps, so the next Update rebuilds every step in that
    folder (the KH2 extract stays). -> the message for the player"""
    done = os.path.join(root, "work", "done")
    if os.path.isdir(done):
        for n in os.listdir(done):
            if not n.startswith("_"):
                try:
                    os.remove(os.path.join(done, n))
                except OSError:
                    pass
    if "build them differently" in msg:
        return msg
    tail = (" (" + msg.split("(", 1)[1]) if "(" in msg else ""
    return "Some files did not come out right after the update. Press Update again: this time it rebuilds every file (about 15 minutes)." + tail


# ------------------------------------------------------------------------------------------------ state + build
class State:
    def __init__(self, a):
        self.lock = threading.Lock()
        self.root = os.path.abspath(a.root).replace("\\", "/")
        global VERSION
        try:   # the release's manifest.json (written by mkzip --version) is the one source of the version
            VERSION = json.load(open(os.path.join(self.root, "manifest.json"), encoding="utf-8"))["version"]
        except (OSError, ValueError, KeyError):
            pass
        self.kit = os.path.abspath(a.kit or os.path.join(self.root, "kit")).replace("\\", "/")
        self.kh2img_dev = a.kh2img
        self.status = "setup"          # setup | running | verifying | done | failed | cancelled
        self.sw = {"path": "", "ok": False, "msg": "", "info": {}}
        self.kh2 = {"path": "", "ok": False, "msg": "", "info": {}, "auto": False}
        self.log, self.seq = collections.deque(maxlen=600), 0
        self.proc, self.t0, self.t1 = None, None, None
        self.gen, self.thread, self.job = 0, None, make_job()     # gen: a Stop + new Install never mixes two runs
        self.pick_lock = threading.Lock()
        self.steps = {}                # name -> {"group", "phase", "w", "state": todo|run|done|skip}
        self.phase_state = {k: "wait" for k, _ in PHASES}
        self.error, self.build_id = "", ""
        self.launchers = {}
        self.mode = "install"          # install | update (s183: the build runs in self.upd's folder)
        self.upd = {"path": "", "ok": False, "msg": "", "info": {}}
        self.settings_file = os.path.join(self.root, ".installer-ui", "choices.json")
        try:
            prev = json.load(open(self.settings_file, encoding="utf-8"))
        except (OSError, ValueError):
            prev = {}
        self.set_sw(prev.get("sw", ""))
        kh = prev.get("kh2") or steam_kh2()
        self.set_kh2(kh, auto=bool(kh and not prev.get("kh2")))
        if os.path.isdir(os.path.join(self.root, "work", "done")) and any(
                n for n in os.listdir(os.path.join(self.root, "work", "done")) if not n.startswith("_")):
            self.status = "setup"
            self.resumable = True
        else:
            self.resumable = False

    def save_choices(self, path=None):
        path = path or self.settings_file
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            json.dump({"sw": self.sw["path"], "kh2": self.kh2["path"]}, f)

    # (s183) update an older install
    def set_upd(self, p):
        ok, msg, info = update.detect(p, RELEASE, VERSION) if p else (False, "", {})
        self.upd = {"path": info.get("root", p) if ok else p, "ok": ok, "msg": msg, "info": info}
        if ok:      # the old install's game folders, when they still check out (else the player picks them here)
            if info.get("sw") and check_sw(info["sw"])[0]:
                self.set_sw(info["sw"])
            if info.get("kh2") and check_kh2(info["kh2"])[0]:
                self.set_kh2(info["kh2"])

    def updating(self):
        return self.upd["ok"] and bool(self.upd["path"])

    def dest(self):
        """the folder the build writes into: this one, or the old install being updated"""
        if self.mode == "update" or (self.status not in ("running", "verifying") and self.updating()):
            return self.upd["path"]
        return self.root

    def phases(self):
        return ([COPY_PHASE] if self.mode == "update" else []) + PHASES

    @property
    def sbdir(self):
        """the sorabuild that builds: this release's own, or (update) the copy just put into the old folder"""
        return os.path.join(self.dest(), "tools", "sorabuild").replace(chr(92), "/") if self.mode == "update" else SBDIR

    def set_sw(self, p):
        ok, msg, info = check_sw(p) if p else (False, "", {})
        self.sw = {"path": p, "ok": ok, "msg": msg, "info": info}

    def set_kh2(self, p, auto=False):
        ok, msg, info = check_kh2(p) if p else (False, "", {})
        self.kh2 = {"path": p, "ok": ok, "msg": msg, "info": info, "auto": auto}

    def add_log(self, line):
        with self.lock:
            self.seq += 1
            self.log.append((self.seq, line))

    def disk(self):
        g = free_gb(self.dest())
        need = NEED_UPDATE_GB if self.updating() else NEED_DISK_GB
        return {"free": None if g is None else round(g, 1), "need": need, "ok": g is None or g >= need}

    def snapshot(self, since):
        with self.lock:
            lines = [l for s, l in self.log if s > since]
            phases = []
            for k, label in self.phases():
                st = [s for s in self.steps.values() if s["phase"] == k and s["state"] != "skip"]
                w = sum(s["w"] for s in st) or (PHASE_W.get(k, 0) if k in ("copy", "kh2", "verify", "game") else 0)
                dw = sum(s["w"] for s in st if s["state"] == "done")
                ps = self.phase_state.get(k, "wait")
                frac = 1.0 if ps == "done" else (dw / w if w else 0.0)
                phases.append({"key": k, "label": label, "state": ps, "frac": round(frac, 3),
                               "running": [n for n, s in self.steps.items() if s["phase"] == k and s["state"] == "run"]})
            fixed = [k for k, _ in self.phases() if k in PHASE_W]
            tot = sum(s["w"] for s in self.steps.values() if s["state"] != "skip") + sum(PHASE_W[k] for k in fixed)
            done = sum(s["w"] for s in self.steps.values() if s["state"] == "done") + \
                sum(PHASE_W[k] for k in fixed if self.phase_state.get(k) == "done")
            frac = done / tot if tot else 0.0
            el = (time.time() - self.t0) if self.t0 and self.status in ("running", "verifying") else \
                ((self.t1 - self.t0) if self.t0 and self.t1 else 0)
            eta = self._eta(el) if self.status in ("running", "verifying") else None
            return {"version": VERSION, "status": self.status, "root": self.dest(), "sw": self.sw, "kh2": self.kh2,
                    "mode": self.mode, "upd": self.upd, "updating": self.updating(),
                    "disk": self.disk(), "resumable": self.resumable, "phases": phases, "frac": round(frac, 4),
                    "elapsed": round(el), "eta": None if eta is None else round(eta), "error": self.error,
                    "build_id": self.build_id, "launchers": self.launchers, "seq": self.seq, "lines": lines, "sw_version": SW_VERSION}

    def _eta(self, el):
        """seconds left: the rest of the serial work / PAR, never less than the longest step still to finish, scaled by
        this PC's speed (EST vs the real seconds of the finished steps). None in the first minute."""
        if el < 60:
            return None
        now = time.time()
        est = [(s["w"], s["secs"]) for s in self.steps.values() if s["state"] == "done" and s.get("secs")]
        speed = min(3.0, max(0.3, sum(e for e, _ in est) / sum(a for _, a in est))) if len(est) >= 3 else 1.0
        left, longest = 0.0, 0.0
        for s in self.steps.values():
            if s["state"] in ("todo", "run"):
                r = s["w"] - ((now - s["t"]) * speed if s["state"] == "run" and s.get("t") else 0)
                r = max(r, 0.1 * s["w"])
                left += r
                longest = max(longest, r)
        if self.phase_state["kh2"] != "done":
            left += PHASE_W["kh2"]
        rest = max(left / PAR, longest) / speed
        return rest + (PHASE_W["verify"] if self.phase_state["verify"] != "done" else 0) + PHASE_W["game"]

    # -------------------------------------------------------------------------------------------- build
    def config(self):
        root = self.dest()
        kit = (root + "/kit") if self.mode == "update" else self.kit
        cfg = {"kit": kit, "kh2img": self.kh2img_dev or self.kh2["info"]["img"], "romfs": self.sw["info"]["romfs"],
               "out": root + "/data", "work": root + "/work", "fx": "fxrec", "sounds": "recipe", "fx_keep": "used",
               "kh2exe": self.kh2["info"]["exe"]}
        p = os.path.join(root, "work", "install_config.json")
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            json.dump(cfg, f, indent=1)
        return p

    def _popen(self, args):
        """a hidden console of its own: the console tools sorabuild starts share it (no window pops up per tool)"""
        sysroot = os.environ.get("SYSTEMROOT", r"C:\Windows")
        env = {k: v for k, v in os.environ.items() if not k.upper().startswith("PYTHON")}
        env["PATH"] = os.pathsep.join([os.path.join(sysroot, "System32"), sysroot,
                                       os.path.join(sysroot, "System32", "WindowsPowerShell", "v1.0")])
        env["PYTHONUTF8"] = "1"
        kw = {}
        if sys.platform == "win32":
            si = subprocess.STARTUPINFO()
            si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            si.wShowWindow = 0
            kw = {"startupinfo": si, "creationflags": subprocess.CREATE_NEW_CONSOLE}
        return subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                                env=env, cwd=self.sbdir, **kw)

    def kit_python(self):
        return os.path.join(self.kit, "python", "python.exe")     # (update: this release's kit python runs the copy in the old folder)

    def start(self):
        with self.lock:
            if self.status in ("running", "verifying") or (self.thread and self.thread.is_alive()) or self.proc:
                return "the last install is still stopping - try again in a moment"
            if not (self.sw["ok"] and self.kh2["ok"]):
                return "choose both folders first"
            if not self.disk()["ok"]:
                return "not enough free disk space"
            self.mode = "update" if self.updating() else "install"
            if self.mode == "update":
                d = self.upd["path"]
                ok, msg, _ = update.detect(d, RELEASE, VERSION)      # again: the folder may have changed since it was picked
                busy = msg if not ok else update.busy_message(d)
                if not busy and not acquire_lock(d):
                    busy = "Another installer is working on that folder. Close it first."
                if busy:
                    self.mode = "install"
                    return busy
            self.status, self.error, self.build_id = "running", "", ""
            self.t0, self.t1 = time.time(), None
            self.phase_state = {k: "wait" for k, _ in self.phases()}
            self.steps = {}
            self.gen += 1
            self.thread = threading.Thread(target=self._run, args=(self.gen,), daemon=True)
        self.save_choices()
        self.thread.start()
        return ""

    def _live(self, gen):
        return self.gen == gen and self.status != "cancelled"

    def _spawn(self, gen, args):
        """start a build process for run <gen>, unless that run was stopped meanwhile (then None)"""
        with self.lock:
            if not self._live(gen):
                return None
            p = self._popen(args)
            job_add(self.job, p)
            self.proc = p
            return p

    def _drain(self, p, sink):
        for raw in p.stdout:
            sink(raw.decode("utf-8", "replace").rstrip())
        rc = p.wait()
        with self.lock:
            if self.proc is p:
                self.proc = None
        return rc

    def _list_steps(self, cfgp, raw=False):
        r = subprocess.run([self.kit_python(), "-E", "-P", os.path.join(self.sbdir, "sorabuild.py"), "--config", cfgp, "--list"],
                           capture_output=True, text=True, cwd=self.sbdir,
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        if r.returncode:
            self.add_log("[installer] sorabuild --list failed (%d): %s" % (r.returncode, (r.stderr or r.stdout).strip()[-400:]))
            if raw:
                raise RuntimeError("The build tools did not start (see the details).")
        if raw:
            return r.stdout
        steps = {}
        for ln in r.stdout.splitlines():
            m = re.match(r"(\S+)\s+(\S+)\s+(done|-)\s", ln)
            if m:
                n, g, d = m.groups()
                steps[n] = {"group": g, "phase": GROUP_PHASE.get(g, "other"), "w": max(1.0, EST.get(n, 3)),
                            "state": "skip" if d == "done" else "todo"}
        return steps

    def _run(self, gen):
        try:
            self._run2(gen)
        finally:
            if self.mode == "update":
                release_lock(os.path.join(self.dest(), ".installer-ui", "lock"))

    def _run2(self, gen):
        try:
            upd = self.mode == "update"
            gameside.clear_build_id(self.dest())   # (s183) until verify passes again the launchers refuse to start (stale data)
            if upd:   # (s183) the old folder gets this release's program files, then only what changed is rebuilt
                with self.lock:
                    self.phase_state["copy"] = "run"
                self.add_log("[update] %s: %s -> %s" % (self.dest(), self.upd["info"].get("version", "?"), VERSION))
                update.prepare(RELEASE, self.dest(), self.add_log)
                self.save_choices(os.path.join(self.dest(), ".installer-ui", "choices.json"))
            cfgp = self.config()
            self.add_log("[installer] config %s" % cfgp)
            if upd:
                update.invalidate(self.dest(), self._list_steps(cfgp, raw=True), self.add_log)
                with self.lock:
                    self.phase_state["copy"] = "done"
            steps = self._list_steps(cfgp)
            with self.lock:
                if not self._live(gen):
                    return
                self.steps = steps
                self.phase_state["kh2"] = "run"
            p = self._spawn(gen, [self.kit_python(), "-E", "-P", os.path.join(self.sbdir, "sorabuild.py"), "--config", cfgp, "all"])
            if p is None:
                return

            def build_line(line):
                self.add_log(line)
                self._parse(line)
            rc = self._drain(p, build_line)
            if not self._live(gen):
                return
            if rc:
                raise RuntimeError(self._last_error() or "the build stopped (exit %d)" % rc)
            with self.lock:
                for k in ("kh2", "bakes", "fxrec", "fx", "other"):
                    self.phase_state[k] = "done"
                self.status, self.phase_state["verify"] = "verifying", "run"
            man = os.path.join(self.sbdir, "manifest.thispc.json")
            v = self._spawn(gen, [self.kit_python(), "-E", "-P", os.path.join(self.sbdir, "verify.py"), "--out", self.dest() + "/data",
                                  "--expect", man])
            if v is None:
                return
            out = []

            def verify_line(line):
                out.append(line)
                if not re.match(r"\S+\s+\S+\s+SAME$", line):
                    self.add_log(line)
            rc = self._drain(v, verify_line)
            if not self._live(gen):
                return
            if rc:
                bad = [l for l in out if re.search(r"(DIFF|MISSING|NOT IN)", l)]
                raise RuntimeError(self._after_bad_verify(bad))
            m = [re.match(r"build id (\w+)", l) for l in out]
            with self.lock:
                if not self._live(gen):
                    return
                self.build_id = next((x.group(1) for x in m if x), "")
                self.phase_state["verify"] = "done"
                self.phase_state["game"] = "run"
            # (s177) the game side: bin/ into the install root + the two launchers (safe to repeat; never touches saves)
            if upd:   # bin/ is already this release's (prepare): the game side only checks it + rewrites the launchers
                self.launchers = gameside.install_game_side(self.dest(), self.sw["path"], self.add_log, release=self.dest())
                update.finish(self.dest(), self.add_log)
                with self.lock:
                    self.set_upd(self.dest())     # it is this version now
            else:
                self.launchers = gameside.install_game_side(self.root, self.sw["path"], self.add_log)
            with self.lock:
                if not self._live(gen):
                    return
                self.phase_state["game"] = "done"
                self.status, self.t1 = "done", time.time()
            self.add_log("[installer] %s in %.1f min, build id %s" % ("updated to %s" % VERSION if upd else "done",
                                                                     (self.t1 - self.t0) / 60, self.build_id))
        except Exception as e:  # noqa: BLE001 - shown to the player
            with self.lock:
                if not self._live(gen):
                    return
                self.error = str(e)
                self.status, self.t1 = "failed", time.time()
                for k, v in self.phase_state.items():
                    if v == "run":
                        self.phase_state[k] = "failed"
            self.add_log("[installer] FAILED: %s" % e)

    def _after_bad_verify(self, bad):
        """(s178 M2) verify failed: make the next Install rebuild the wrong files instead of skipping every step again. -> the message"""
        msg, self.last_bad = recover_after_bad_verify(self.dest(), bad, getattr(self, "last_bad", None))
        if self.mode == "update":
            msg = update_after_bad_verify(self.dest(), msg)
        self.add_log("[installer] " + msg)
        return msg

    def _last_error(self):
        with self.lock:
            for _, l in reversed(self.log):
                if "FAILED" in l or "[sorabuild]" in l and ("missing" in l or "refuse" in l or "differ" in l or "crash" in l):
                    return l.replace("[sorabuild] ", "")
        return ""

    def _parse(self, line):
        with self.lock:
            if "kh2pick: done" in line or re.match(r"\[sorabuild\] \d+ steps, up to", line):
                self.phase_state["kh2"] = "done"
            m = re.match(r"\[(\w+)\] start$", line)
            if m and m.group(1) in self.steps:
                s = self.steps[m.group(1)]
                s["state"], s["t"] = "run", time.time()
                self.phase_state["kh2"] = "done"
                if self.phase_state[s["phase"]] == "wait":
                    self.phase_state[s["phase"]] = "run"
            m = re.match(r"\[progress\] \d+/\d+ (\w+) ([\d.]+) s", line)
            if m and m.group(1) in self.steps:
                s = self.steps[m.group(1)]
                s["state"], s["secs"] = "done", float(m.group(2))
                ph = s["phase"]
                if all(x["state"] in ("done", "skip") for x in self.steps.values() if x["phase"] == ph):
                    self.phase_state[ph] = "done"
            m = re.match(r"\[sorabuild\] (\w+): done, skipped", line)
            if m and m.group(1) in self.steps:
                self.steps[m.group(1)]["state"] = "skip"

    def cancel(self):
        with self.lock:
            p = self.proc
            if self.status not in ("running", "verifying"):
                return
            self.status, self.t1 = "cancelled", time.time()
            self.resumable = True
        if p:
            if sys.platform == "win32":
                subprocess.run(["taskkill", "/T", "/F", "/PID", str(p.pid)], capture_output=True,
                               creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            else:
                p.kill()
        self.add_log("[installer] stopped. Install again to continue where it stopped.")


# ------------------------------------------------------------------------------------------------ http
def make_handler(st, token):
    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def _send(self, code, body, ctype="application/json"):
            b = body if isinstance(body, bytes) else json.dumps(body).encode()
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(b)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(b)

        def do_GET(self):
            u = urllib.parse.urlparse(self.path)
            if u.path.startswith("/api/"):
                if self.headers.get("X-Token") != token:
                    return self._send(403, {"error": "token"})
                if u.path == "/api/state":
                    q = urllib.parse.parse_qs(u.query)
                    return self._send(200, st.snapshot(int(q.get("since", ["0"])[0])))
                return self._send(404, {"error": "no such api"})
            rel = "index.html" if u.path in ("/", "") else u.path.lstrip("/")
            p = os.path.normpath(os.path.join(UI, rel))
            if os.path.commonpath([p, UI]) != UI or not os.path.isfile(p):
                return self._send(404, b"not found", "text/plain")
            ctype = mimetypes.guess_type(p)[0] or "application/octet-stream"
            if p.endswith(".js"):
                ctype = "text/javascript"
            self._send(200, open(p, "rb").read(), ctype)

        def do_POST(self):
            if self.headers.get("X-Token") != token:
                return self._send(403, {"error": "token"})
            n = int(self.headers.get("Content-Length") or 0)
            try:
                body = json.loads(self.rfile.read(n) or b"{}")
            except ValueError:
                body = {}
            u = urllib.parse.urlparse(self.path).path
            if u == "/api/pick":
                what = body.get("what")
                cur = {"sw": st.sw, "kh2": st.kh2, "upd": st.upd}.get(what, st.kh2)["path"]
                if what == "upd" and not cur:
                    cur = os.path.dirname(st.root)       # an older release usually sits next to this one
                if not st.pick_lock.acquire(blocking=False):
                    return self._send(200, {"picked": ""})     # a picker is already open
                try:
                    p = pick_folder("Choose your Savage Wind %s folder" % SW_VERSION if what == "sw" else
                                    "Choose the folder of your older Legend of Kairi" if what == "upd" else
                                    "Choose your KINGDOM HEARTS HD 1.5+2.5 ReMIX folder", cur)
                except OSError as e:
                    return self._send(200, {"error": str(e)})
                finally:
                    st.pick_lock.release()
                if p:
                    with st.lock:
                        if st.status in ("running", "verifying"):
                            return self._send(200, {"error": "the install is running"})
                        if what == "sw":
                            st.set_sw(p)
                        elif what == "upd":
                            st.set_upd(p)
                        else:
                            st.set_kh2(p)
                    st.save_choices()
                return self._send(200, {"picked": p})
            if u == "/api/update":      # (s183) set (path) or clear ("") the old install to update without the picker (also: tests)
                with st.lock:
                    if st.status in ("running", "verifying"):
                        return self._send(200, {"error": "the install is running"})
                    st.set_upd(body.get("path") or "")
                    st.mode = "install"
                return self._send(200, {"ok": st.upd["ok"], "msg": st.upd["msg"]})
            if u == "/api/install":
                return self._send(200, {"error": st.start()})
            if u == "/api/cancel":
                st.cancel()
                return self._send(200, {})
            if u == "/api/open":
                what = body.get("what")
                p = {"log": os.path.join(st.dest(), "work", "logs"), "root": st.dest(),
                     "licenses": os.path.join(st.root, "LICENSES")}.get(what)
                if p and os.path.exists(p) and sys.platform == "win32":
                    os.startfile(p)  # noqa: S606 - opens one of our own folders in Explorer
                return self._send(200, {})
            return self._send(404, {"error": "no such api"})
    return H


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


def find_edge():
    for base in (os.environ.get("ProgramFiles(x86)"), os.environ.get("ProgramFiles"), os.environ.get("LOCALAPPDATA")):
        if base:
            p = os.path.join(base, "Microsoft", "Edge", "Application", "msedge.exe")
            if os.path.isfile(p):
                return p
    return ""


def pid_alive(pid):
    if pid <= 0:
        return False
    if sys.platform != "win32":
        try:
            os.kill(pid, 0)
            return True
        except OSError:
            return False
    import ctypes
    k = ctypes.WinDLL("kernel32", use_last_error=True)   # (s183) the real last error, not one Python's own calls left behind
    k.OpenProcess.restype = ctypes.c_void_p
    h = k.OpenProcess(0x1000, False, pid)         # PROCESS_QUERY_LIMITED_INFORMATION
    if not h:
        return ctypes.get_last_error() == 5       # access denied = it exists
    code = ctypes.c_ulong()
    ok = k.GetExitCodeProcess(ctypes.c_void_p(h), ctypes.byref(code))
    k.CloseHandle(ctypes.c_void_p(h))
    return bool(ok) and code.value == 259         # STILL_ACTIVE


def _not_python(pid):
    """(s183) True when pid is a running program that is NOT a python (the lock's installer is gone and Windows gave its number to
    another program); False when it is a python or cannot be told"""
    if sys.platform != "win32":
        return False
    import ctypes
    from ctypes import wintypes
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.OpenProcess.restype = wintypes.HANDLE
    h = k.OpenProcess(0x1000, False, pid)
    if not h:
        return False
    try:
        buf, n = ctypes.create_unicode_buffer(1024), wintypes.DWORD(1024)
        if not k.QueryFullProcessImageNameW(wintypes.HANDLE(h), 0, buf, ctypes.byref(n)):
            return False
        return not os.path.basename(buf.value).lower().startswith("python")
    finally:
        k.CloseHandle(wintypes.HANDLE(h))


def acquire_lock(root):
    """(s178 S2) one installer per folder: <root>/.installer-ui/lock holds our pid (created exclusively). A lock whose process is gone
    (crash, power loss) is replaced. -> True if we hold it now, False if another live installer does."""
    d = os.path.join(root, ".installer-ui")
    os.makedirs(d, exist_ok=True)
    lp = os.path.join(d, "lock")
    for _ in range(3):
        try:
            fd = os.open(lp, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
        except FileExistsError:
            try:
                pid = int(open(lp).read().strip() or 0)
            except (OSError, ValueError):
                pid = 0
            if pid and pid != os.getpid() and pid_alive(pid) and not _not_python(pid):
                return False
            try:
                os.remove(lp)
            except OSError:
                pass
            continue
        os.write(fd, str(os.getpid()).encode())
        os.close(fd)
        import atexit
        atexit.register(release_lock, lp)
        return True
    return False


def release_lock(lp):
    try:
        if int(open(lp).read().strip() or 0) == os.getpid():
            os.remove(lp)
    except (OSError, ValueError):
        pass


def front_window():
    if sys.platform == "win32":
        import ctypes
        h = ctypes.windll.user32.FindWindowW(None, WINDOW_TITLE)
        if h:
            ctypes.windll.user32.ShowWindow(h, 9)
            ctypes.windll.user32.SetForegroundWindow(h)


def message_box(text):
    print(text, flush=True)
    if sys.platform == "win32":
        import ctypes
        ctypes.windll.user32.MessageBoxW(None, text, WINDOW_TITLE, 0x10)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.normpath(os.path.join(HERE, "..", "..")))
    ap.add_argument("--kit")
    ap.add_argument("--kh2img", help="dev: an Image/dt to read instead of the chosen KH2 folder's (e.g. unmodded hard links)")
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--browser", action="store_true", help="dev: open in the default browser, serve until Ctrl+C")
    ap.add_argument("--serve", action="store_true", help="dev: only serve (print the URL), until Ctrl+C")
    a = ap.parse_args()
    root = os.path.abspath(a.root)
    if not root.isascii():   # (s178 S6) the mod and the build tools open files by their ANSI name
        message_box("The folder this is in has letters in its name that Windows programs here cannot handle:\n\n%s\n\n"
                    "Move the whole Legend of Kairi folder to a path with plain letters only, for example C:\\Games\\Legend of Kairi, "
                    "and start the installer from there." % root)
        return 1
    if not acquire_lock(root):
        front_window()
        print("installer: another installer is already running on this folder", flush=True)
        return 0
    st = State(a)
    token = secrets.token_urlsafe(18)
    srv = Server(("127.0.0.1", a.port), make_handler(st, token))
    url = "http://127.0.0.1:%d/?t=%s" % (srv.server_address[1], token)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    print("installer: %s (root %s)" % (url, st.root), flush=True)
    shown = False
    if not (a.browser or a.serve):
        shown = show_window(st, url)
    if not shown:
        if not a.serve:
            import webbrowser
            webbrowser.open(url)
        try:
            while True:
                time.sleep(3600)
        except KeyboardInterrupt:
            pass
    st.cancel()
    srv.shutdown()


def show_window(st, url):
    """our own window (kit tools/installer_host = WebView2, no browser / account / extensions); on a PC without the WebView2
    runtime: a locked-down Edge app window (InPrivate = no sign-in or sync, extensions off). Returns after the window closed;
    False if neither could be shown."""
    host = os.path.join(st.kit, "tools", "installer_host", "LegendOfKairiSetup.exe")
    if os.path.isfile(host):
        env = dict(os.environ, DOTNET_ROOT=os.path.join(st.kit, "dotnet"))
        t = time.time()
        rc = subprocess.run([host, "--url", url, "--data", os.path.join(st.root, ".installer-ui", "webview"), "--title", WINDOW_TITLE],
                            env=env).returncode
        if rc != 3:   # 3 = no WebView2 runtime
            if rc and time.time() - t < 5:
                st.add_log("[installer] window program failed (%d), trying Edge" % rc)
            else:
                return True
    edge = find_edge()
    if not edge:
        return False
    prof = os.path.join(st.root, ".installer-ui", "edge")
    subprocess.run([edge, "--app=" + url, "--user-data-dir=" + prof, "--inprivate", "--disable-extensions", "--disable-sync",
                    "--no-first-run", "--no-default-browser-check", "--window-size=1280,760", "--disable-features=Translate",
                    "--hide-crash-restore-bubble"])
    return True


if __name__ == "__main__":
    sys.exit(main())

"""recordall.py (s161) - record every file of a list with fxrec and write kh2_fx.bin + kh2_fx.json. The sorabuild step
fxrec_record (steps/fxrec.py) calls this; it can also be run by hand:

  python -E -P recordall.py --list files82.txt --kh2 <KH2Extract> --exe <KH2 FM exe> --out <work dir> [--jobs 2] [--only REGEX] [--sh bash]

list lines: "<kh2 file> <prefix> [heavy]" (other columns are ignored: the old oracle tail counts must not be used - loopend.py decides).
Per file (a "job", --jobs of them at once; each = 5 recorder processes, up to 768 MB arena each):
  loopend.py <file>  -> --tails 120 if the file's script calls effect_loop_end
  PRIZE.mdlx         -> --pool 0 ; P_EX200 / P_EX220 -> --isotails 1   (same rules as allfiles.sh)
  runfile.sh <file> <out>/rec/<prefix> <prefix> "<args>"   (log: <out>/rec/<prefix>.run.txt)
A file is skipped when <out>/rec/<prefix>.ok holds the same signature (exe sha1, file line, args, code hash); the .fxd dumps are
deleted after each file. s164: HEAVY files (HEAVY prefixes, a "heavy" list column, --heavy REGEX) run ALONE: one starts only when no
file is running and nothing else starts while it runs (two heavy files at once got processes killed on 16 GB). Light files overlap.
Then phf2write.py --out <out>/out <rec dirs in list order>.  Exit 1 if any file failed (nothing written).
"""
import argparse
import concurrent.futures as cf
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
EXE_SHA1 = "2548F1191FE6587A34947952763CE6BB4F3B5C1A"
PKGS = ["kh2_first", "kh2_second", "kh2_third", "kh2_fourth", "kh2_fifth", "kh2_sixth"]
# per-file signature = what makes a recording (s162 review: NOT phf2write.py - a writer fix must not force a re-record)
CODE = ["runfile.sh", "loopend.py", "fxsolve.py", "fxvbin.py", "hdtab.py", "fxrec_boot.c", "fxrec_s2.c", "fxrec_ldr.c", "fxrec_ldr.h"]


def sha(path, algo):
    h = hashlib.new(algo)
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest().upper()


def code_hash():
    h = hashlib.sha256()
    for n in CODE:
        h.update(open(os.path.join(HERE, n), "rb").read().replace(b"\r\n", b"\n"))
    return h.hexdigest()[:16]


# s164: files whose recording needs so much RAM / time that nothing else may run beside them (s161 times: D:/SoraBotW/recon/s161times.txt)
HEAVY = {"ex200", "ex220", "hb040", "m_ex920", "al010"}


class Gate:
    """s161/s163/s164 start gate. One file at a time waits to start (it holds `gate` while it waits); it starts when the last started
    file has FINISHED or had `grow_s` to grow, free RAM >= `min_free` MB, and: a light file needs no heavy one running, a heavy
    file needs nothing running at all. A waiting heavy file holds `gate`, so newer files cannot overtake it (no starvation).
    `slock` guards the state and is never held while sleeping (finish() must not wait on gate)."""

    def __init__(self, free_mb, grow_s=60.0, min_free=4000, poll=(0.5, 2.0), log=None, idle_wait=120.0, idle_wait_after=5.0):
        # s224: `idle_wait` = how long a file waits for free RAM while NOTHING of ours runs. Then the RAM is held by something
        # else (a game, a browser, or a PC with 8 GB in all) and waiting cannot free it: the file starts anyway (one file alone,
        # like the old jobs 1), instead of the setup hanging forever on "waits for 4 GB free RAM" (s224 run A: 2.3 GB free with
        # a game open = stuck 17+ min with nothing running).
        # s226 (review G7): the long wait is paid ONCE. After the first forced start the RAM is known to be held by something
        # else, so later files wait only `idle_wait_after` (else 87 files x 120 s = about 3 hours of waiting on an 8 GB PC).
        self.free_mb, self.grow_s, self.min_free, self.poll, self.idle_wait = free_mb, grow_s, min_free, poll, idle_wait
        self.idle_wait_after = idle_wait_after
        self.log = log or (lambda m: print(m, flush=True))
        self.gate = threading.Lock()
        self.slock = threading.Lock()
        self.last = {"t": 0.0, "n": 0, "done": True}   # the most recently started file: start time, number, finished yet?
        self.running = 0
        self.heavy_running = False

    def start(self, name, heavy=False):
        """blocks until `name` may start; returns its number for finish()"""
        with self.gate:
            told = told_h = False
            idle_since = None
            while True:
                with self.slock:
                    left = 0 if self.last["done"] else self.grow_s - (time.time() - self.last["t"])
                    busy = self.running > 0 if heavy else self.heavy_running
                    idle = self.running == 0
                fm = self.free_mb()
                if left <= 0 and not busy and (fm is None or fm >= self.min_free):
                    break
                if idle and left <= 0:
                    idle_since = idle_since or time.time()
                    if time.time() - idle_since >= self.idle_wait:
                        self.log("recordall: %s starts with %s MB free RAM (nothing else of ours runs; waited %d s)" %
                                 (name, fm, self.idle_wait))
                        self.idle_wait = min(self.idle_wait, self.idle_wait_after)
                        break
                else:
                    idle_since = None
                if busy and not told_h:
                    self.log("recordall: %s is %s, waits for %s" % (name, "HEAVY" if heavy else "light",
                             "all running files to finish" if heavy else "the running heavy file to finish"))
                    told_h = True
                elif not busy and not told and left <= 0:
                    self.log("recordall: %s waits for 4 GB free RAM (%s MB now)" % (name, fm))
                    told = True
                time.sleep(max(self.poll[0], min(left, self.poll[1])))
            with self.slock:
                self.last["n"] += 1
                self.last.update(t=time.time(), done=False)
                self.running += 1
                self.heavy_running = heavy
                if heavy:
                    self.log("recordall: %s (HEAVY) starts alone" % name)
                return self.last["n"]

    def finish(self, n, heavy=False):
        with self.slock:
            self.running -= 1
            if heavy:
                self.heavy_running = False
            if self.last["n"] == n:
                self.last["done"] = True


def read_list(path):
    out = []
    for ln in open(path, encoding="utf-8").read().splitlines():
        ln = ln.strip()
        if not ln or ln.startswith("#"):
            continue
        p = ln.split()
        if len(p) < 2:
            raise SystemExit("recordall: bad list line %r" % ln)
        out.append((p[0], p[1], p[1] in HEAVY or "heavy" in p[2:]))
    return out


def find_sh(want):
    for c in (want, shutil.which("bash"), "C:/Program Files/Git/bin/bash.exe", "C:/Program Files/Git/usr/bin/bash.exe"):
        if c and os.path.isfile(c):
            return c
        if c and shutil.which(c):
            return shutil.which(c)
    raise SystemExit("recordall: no bash found (Git for Windows); pass --sh")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", required=True)
    ap.add_argument("--kh2", required=True)
    ap.add_argument("--exe", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--only", default="")
    ap.add_argument("--heavy", default="", help="REGEX over file name / prefix: matching files also run alone (s164)")
    ap.add_argument("--keep", default="", help="(s166 fx_keep used) keep list json (fxcensus.py --emit-keep): record only the kept elements / tails; "
                    "files without any kept element are not run and not written")
    ap.add_argument("--keep-fxd", action="store_true", help="dev: do not delete the .fxd dumps after the solve (to re-run fxsolve on them)")
    ap.add_argument("--sh", default="")
    ap.add_argument("--build", default="", help="path of the fxrec_boot.exe (default <here>/build/fxrec_boot.exe)")
    a = ap.parse_args()

    if not os.path.isfile(a.exe):
        raise SystemExit("recordall: KH2 exe not found: " + a.exe)
    got = sha(a.exe, "sha1")
    if got != EXE_SHA1:
        raise SystemExit("recordall: unsupported KH2 exe (sha1 %s, need %s: the Steam KH2 FM exe)" % (got, EXE_SHA1))
    boot = a.build or os.path.join(HERE, "build", "fxrec_boot.exe")
    if not os.path.isfile(boot):
        raise SystemExit("recordall: %s missing (sh build.sh / the sorabuild step fxrec_build)" % boot)
    sh = find_sh(a.sh)
    files = read_list(a.list)
    if a.only:
        files = [f for f in files if re.search(a.only, f[0])]
        if not files:
            raise SystemExit("recordall: --only %r matches no file of the list" % a.only)
    if a.heavy:
        files = [(n, p, h or bool(re.search(a.heavy, n) or re.search(a.heavy, p))) for n, p, h in files]
    keep = None
    if a.keep:
        keep = json.load(open(a.keep, encoding="utf-8"))
        if not a.only:
            unknown = sorted(set(keep["elements"]) - {p for _, p, _h in files})
            if unknown:
                raise SystemExit("recordall: the keep list names files that are not in the list: %s" % unknown)
        files = [f for f in files if f[1] in keep["elements"]]
        print("recordall: fx_keep used: %d files with kept elements" % len(files), flush=True)
    rec = os.path.join(a.out, "rec")
    outd = os.path.join(a.out, "out")
    os.makedirs(rec, exist_ok=True)

    env = dict(os.environ)
    shdirs = [os.path.dirname(sh)]
    root = os.path.dirname(os.path.dirname(sh))
    for d in ("usr/bin", "bin", "mingw64/bin"):
        if os.path.isdir(os.path.join(root, d)):
            shdirs.append(os.path.join(root, d))
    env["PATH"] = os.pathsep.join([os.path.dirname(sys.executable)] + shdirs + [env.get("PATH", "")])
    env.update({"FXREC_KH2": a.kh2, "FXREC_PYTHON": sys.executable.replace("\\", "/"), "FXREC_GAMEEXE": a.exe.replace("\\", "/"),
                "FXREC_BOOT": boot.replace("\\", "/")})
    chash = code_hash()

    def locate(name):
        f = None
        for p in PKGS:  # last package wins (KH2's patch order)
            c = "%s/%s/original/%s" % (a.kh2, p, name)
            if os.path.isfile(c):
                f = c
        return f

    env.setdefault("SOLVEP", "4")      # s161/s162: solver processes per file (16 GB PC: 2 jobs x 4)

    def free_mb():
        """free physical RAM in MB (Windows GlobalMemoryStatusEx; None elsewhere)"""
        if sys.platform != "win32":
            return None
        import ctypes

        class MS(ctypes.Structure):
            _fields_ = [("len", ctypes.c_uint32), ("load", ctypes.c_uint32)] + [(n, ctypes.c_uint64) for n in
                        ("total", "avail", "ptotal", "pavail", "vtotal", "vavail", "xavail")]
        m = MS(); m.len = ctypes.sizeof(MS)
        return m.avail // (1 << 20) if ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(m)) else None

    gate = Gate(free_mb)

    def one(item):
        name, pre, heavy = item
        t0 = time.time()
        f = locate(name)
        if not f:
            return name, pre, "NOT IN KH2 EXTRACT", 0
        le = subprocess.run([sys.executable, "-E", "-P", os.path.join(HERE, "loopend.py"), name], env=env, capture_output=True, text=True)
        m = re.search(r" loop_end=([01])", le.stdout)
        if not m:
            return name, pre, "loopend.py failed: " + (le.stderr or le.stdout)[-200:], 0
        x = []
        if m.group(1) == "1":
            x += ["--tails", "120"]
        if keep:
            ids = keep["elements"][pre]
            ta = ["%s:%d" % (k.split(".", 1)[1], at) for k, ats in sorted(keep["tails"].items()) if k.rsplit(".", 1)[0] == pre for at in ats]
            if ta and m.group(1) != "1":
                return name, pre, "keep list wants tails but the file's script has no loop end", 0
            if not ta:
                x = []             # no kept tail in this file: no tail recording at all
            x += ["--only", ",".join(map(str, ids))]
            if ta:
                x += ["--tailat", ",".join(ta)]
        if name == "obj/PRIZE.mdlx":
            x += ["--pool", "0"]
        if name in ("obj/P_EX200.a.us", "obj/P_EX220.a.us"):
            x += ["--isotails", "1"]
        args = " ".join(x)
        sig = "exe=%s file=%s %s args=%s code=%s" % (EXE_SHA1, name, pre, args, chash)
        d = os.path.join(rec, pre)
        okf = os.path.join(rec, pre + ".ok")
        if os.path.isfile(okf) and open(okf).read().strip() == sig and os.path.isfile(os.path.join(d, pre + ".list")):
            return name, pre, "skipped (done)", 0
        if os.path.isfile(okf):
            os.remove(okf)
        n = gate.start(name, heavy)
        t0 = time.time()
        try:
            shutil.rmtree(d, ignore_errors=True)
            with open(os.path.join(rec, pre + ".run.txt"), "w", encoding="utf-8") as lg:
                r = subprocess.run([sh, os.path.join(HERE, "runfile.sh").replace("\\", "/"), f, d.replace("\\", "/"), pre, args],
                                   env=env, stdout=lg, stderr=subprocess.STDOUT)
        finally:
            gate.finish(n, heavy)
        if r.returncode:
            return name, pre, "RUN FAILED (see %s.run.txt)" % os.path.join(rec, pre), time.time() - t0
        for fx in ([] if a.keep_fxd else os.listdir(d)):
            if fx.endswith(".fxd"):
                os.remove(os.path.join(d, fx))
        open(okf, "w").write(sig + "\n")
        return name, pre, "ok", time.time() - t0

    print("recordall: %d files (%d heavy, run alone), %d at a time, code %s" % (len(files), sum(f[2] for f in files), a.jobs, chash), flush=True)
    bad = 0
    with cf.ThreadPoolExecutor(max_workers=max(1, a.jobs)) as ex:
        for name, pre, st, secs in ex.map(one, files):
            print("FILE %-28s %-14s %s (%d s)" % (name, pre, st, secs), flush=True)
            if st not in ("ok", "skipped (done)"):
                bad += 1
    if bad:
        raise SystemExit("recordall: %d file(s) failed, kh2_fx.bin not written" % bad)
    # safety: phf2write opens kh2_fx.bin "wb". Never let that happen in place on a file that may be hard-linked to a PHH cache:
    # the out dir must hold nothing but our own earlier output, and that is unlinked (not truncated) first.
    os.makedirs(outd, exist_ok=True)
    for n in os.listdir(outd):         # s162 review: a killed writer leaves its temp files: ours too, unlinked
        if n not in ("kh2_fx.bin", "kh2_fx.json", "kh2_fx.bin.new", "kh2_fx.bin.bodies.tmp", "kh2_fx.json.new"):
            raise SystemExit("recordall: %s is not an empty fxrec out dir (found %s)" % (outd, n))
        os.remove(os.path.join(outd, n))
    cmd = [sys.executable, "-E", "-P", os.path.join(HERE, "phf2write.py"), "--out", outd] + \
          ["%s=%s" % (os.path.join(rec, pre), pre) for _, pre, _h in files]
    r = subprocess.run(cmd, env=env)
    if r.returncode:
        raise SystemExit("recordall: phf2write.py failed")
    print("recordall: wrote %s/kh2_fx.bin + kh2_fx.json" % outd)


if __name__ == "__main__":
    main()

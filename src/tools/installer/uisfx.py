"""Installer menu sounds from the player's OWN KH2 install (s184). The mod ships no KH2 data: at runtime this pulls ONE file, the
system sound bank se/se000.win32.scd (17 MB, found through the .hed by the kit's kh2pick, ~1.5 s), decodes 4 small sounds with the kit's
vgmstream-cli, trims them and writes <root>/.installer-ui/sfx/{move,ok,back,done}.wav. Cached by a json stamp. Every failure = no sounds
(returns False), never an exception and never a blocked install.

Which KH2 sound is which: sorabuild/sounds/mine/mine_misc.py SYSTEM table (our names for se000 ids; KH2 plays them through its
system-SE call): 1 cursor, 2 decide, 4 cancel. "done" = se000 #33 (our "drive_full", a rising chime): a GUESS of what fits best.
"""
import array
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
SCD_REL = "kh2_first/original/se/se000.win32.scd"
SOUNDS = {"move": 1, "ok": 2, "back": 4, "done": 33}      # name -> se000 sound index
NAMES = tuple(SOUNDS)
FILES_JSON = os.path.join(HERE, "..", "sorabuild", "kit", "kh2_files.json")
STAMP_V = 1
CODECS = (6, 12)


def sfx_dir(root):
    return os.path.join(root, ".installer-ui", "sfx")


def _stat(p):
    try:
        s = os.stat(p)
        return [s.st_size, int(s.st_mtime)]
    except OSError:
        return None


def stamp_for(img):
    """what the cache depends on: the KH2 dt folder + sizes/mtimes of its kh2_first .hed/.pkg + the sound table"""
    return {"v": STAMP_V, "img": img.replace("\\", "/"), "hed": _stat(os.path.join(img, "kh2_first.hed")),
            "pkg": _stat(os.path.join(img, "kh2_first.pkg")), "sounds": SOUNDS}


def cached(root, img):
    d = sfx_dir(root)
    try:
        if json.load(open(os.path.join(d, "stamp.json"), encoding="utf-8")) != stamp_for(img):
            return False
        return all(os.path.getsize(os.path.join(d, n + ".wav")) > 44 for n in NAMES)
    except (OSError, ValueError):
        return False


def scd_subsongs(d):
    """se000 sound index -> vgmstream subsong (1-based) for the playable ones (same numbering as sorabuild kh2snd.Scd)"""
    if d[:8] != b"SEDBSSCF":
        raise ValueError("not an SCD")
    ver = struct.unpack_from("<I", d, 8)[0]
    hs = struct.unpack_from("<H", d, 0x0E)[0]
    ns, nt, na, _, otrk, oaud = struct.unpack_from("<HHHHII", d, hs)
    snd = struct.unpack_from("<%dI" % ns, d, hs + 0x20)
    aud = struct.unpack_from("<%dI" % na, d, oaud)
    sub, k = {}, 0
    for a, o in enumerate(aud):
        if o and struct.unpack_from("<I", d, o + 12)[0] in CODECS:
            k += 1
            sub[a] = k
    out = {}
    for i, o in enumerate(snd):
        if not o:
            continue
        n, typ = d[o], d[o + 3]
        core = o + 0x10 + (0x30 if ver >= 4 else 0)
        if n and typ not in (12, 13):
            a = struct.unpack_from("<HH", d, core)[1]
            if a in sub:
                out[i] = sub[a]
    return out


def trim_wav(src, dst, thresh=150, tail=0.06, fade=0.04):
    """cut the trailing silence (the SCD sounds carry up to a second of it), short fade-out, write 16-bit PCM"""
    with wave.open(src, "rb") as w:
        ch, rate, sw, n = w.getnchannels(), w.getframerate(), w.getsampwidth(), w.getnframes()
        raw = w.readframes(n)
    if sw != 2:
        raise ValueError("not 16-bit")
    a = array.array("h")
    a.frombytes(raw)
    if sys.byteorder == "big":
        a.byteswap()
    last = -1
    for i in range(len(a) - 1, -1, -1):
        if abs(a[i]) > thresh:
            last = i
            break
    if last < 0:
        raise ValueError("silent")
    end = min(len(a), (last // ch + 1 + int(tail * rate)) * ch)
    if end <= ch * rate // 50:
        raise ValueError("silent")
    a = a[:end]
    nf = min(len(a) // ch, int(fade * rate))
    for f in range(nf):
        g = f / float(nf)
        for c in range(ch):
            j = len(a) - 1 - f * ch - (ch - 1 - c)
            a[j] = int(a[j] * g)
    with wave.open(dst, "wb") as o:
        o.setnchannels(ch)
        o.setsampwidth(2)
        o.setframerate(rate)
        o.writeframes(a.tobytes() if sys.byteorder == "little" else (lambda b: (b.byteswap(), b.tobytes())[1])(array.array("h", a)))


JOB = None     # (s184 review) the installer's kill-on-close job object: our tools die with the window


def _run(args, env=None, timeout=60):
    kw = {}
    if os.name == "nt":
        kw["creationflags"] = 0x08000000 | 0x00004000       # CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS
    p = subprocess.Popen(args, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kw)
    if JOB and os.name == "nt":
        try:
            import ctypes
            from ctypes import wintypes
            ctypes.windll.kernel32.AssignProcessToJobObject(wintypes.HANDLE(JOB), wintypes.HANDLE(int(p._handle)))
        except Exception:
            pass
    try:
        out, _ = p.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        p.kill()
        out, _ = p.communicate()
        raise
    return subprocess.CompletedProcess(args, p.returncode, out, None)


def build(root, img, kit, log=lambda s: None):
    """extract + decode into <root>/.installer-ui/sfx; True when all 4 files are there. Never raises."""
    try:
        if cached(root, img):
            return True
        t0 = time.time()
        d = sfx_dir(root)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d, exist_ok=True)
        tmp = tempfile.mkdtemp(prefix="w", dir=d)
        try:
            doc = json.load(open(FILES_JSON, encoding="utf-8"))
            doc["files"] = [f for f in doc["files"] if f[0] == SCD_REL]
            if len(doc["files"]) != 1:
                return False
            doc["bytes"] = doc["files"][0][1]
            doc["packages"] = ["kh2_first"]
            lst = os.path.join(tmp, "list.json")
            json.dump(doc, open(lst, "w", encoding="utf-8"))
            env = dict(os.environ, DOTNET_ROOT=os.path.join(kit, "dotnet"))
            r = _run([os.path.join(kit, "tools", "kh2pick", "kh2pick.exe"), img, lst, os.path.join(tmp, "kh2")], env)
            scd = os.path.join(tmp, "kh2", *SCD_REL.split("/"))
            if r.returncode or not os.path.isfile(scd):
                log("uisfx: kh2pick rc %s" % r.returncode)
                return False
            sub = scd_subsongs(open(scd, "rb").read())
            vgm = os.path.join(kit, "vgmstream", "vgmstream-cli.exe")
            for name, idx in SOUNDS.items():
                raw = os.path.join(tmp, name + ".wav")
                r = _run([vgm, "-s", str(sub[idx]), "-o", raw, scd])
                if r.returncode or not os.path.isfile(raw):
                    log("uisfx: vgmstream failed on %s" % name)
                    return False
                trim_wav(raw, os.path.join(d, name + ".wav"))
            json.dump(stamp_for(img), open(os.path.join(d, "stamp.json"), "w", encoding="utf-8"))
            log("uisfx: %d sounds in %.1f s" % (len(SOUNDS), time.time() - t0))
            return True
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    except Exception as e:      # any failure = a silent installer
        try:
            log("uisfx: %s: %s" % (type(e).__name__, e))
        except Exception:
            pass
        return False


def ready(root):
    d = sfx_dir(root)
    return all(os.path.isfile(os.path.join(d, n + ".wav")) for n in NAMES) and os.path.isfile(os.path.join(d, "stamp.json"))


if __name__ == "__main__":      # uisfx.py <KH2 Image/dt> <root> <kit>
    t = time.time()
    ok = build(sys.argv[2], sys.argv[1], sys.argv[3], print)
    print("ok" if ok else "FAILED", "%.2f s" % (time.time() - t))
    sys.exit(0 if ok else 1)

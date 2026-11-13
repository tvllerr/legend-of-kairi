"""(s223) Savage Wind's own files for Legend of Kairi's GitHub release: the version check, the 0.4.1 -> 0.5.3 upgrade in
place ("U1": the same folder, the same game files copy, saves and settings kept) and stock Savage Wind's no-click setup.

MIT licence, like the rest of tools/installer. Python stdlib only (the kit's python runs it; melty-setup/build.py may use it).

Folder kinds (sw_kind):
  "0.5.3"      sw-manifest.txt says version 0.5.3-alpha.5 (the version Legend of Kairi is built for)
  "0.4.1"      README.md names SavageWind-0.4.1-alpha.4 and there is no sw-manifest.txt (0.1.4's version: upgraded in place)
  "upgrading"  our journal .kairi-sw-upgrade.json is there: an upgrade that was cut off (it carries on)
  "other:<v>"  another Savage Wind version: refused, nothing is changed
  "unknown"    not a Savage Wind folder we can name: refused, nothing is changed

The upgrade: the stock 0.5.3 zip (pinned size + sha256) -> backup savagewind.ini + save/ once -> journal -> every file of the
zip next to its target as <name>.kairi-new, then os.replace (sw-manifest.txt last: the folder says 0.5.3 only once all of
0.5.3's files are in) -> stock's own setup keeps the game files copy and builds the 0.5.3 game code + shaders -> the game
files state check -> journal removed.
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.parse
import urllib.request
import zipfile

SW_WANT = "0.5.3-alpha.5"
SW_OLD = "0.4.1-alpha.4"
STOCK = {"name": "SavageWind-0.5.3-alpha.5-windows.zip", "size": 93924792,
         "sha256": "667c4909a8ed72c002e5aa40a00d38720627eebf48b860beb5cd8805421f1285",
         "url": "https://github.com/Revivedx/SavageWindRecomp/releases/download/v0.5.3-alpha.5/SavageWind-0.5.3-alpha.5-windows.zip"}
TOP = "Savage Wind/"            # the zip's one top folder
GAMELIB = "swgame.dll"          # the game code stock's setup builds into extracted/
JOURNAL = ".kairi-sw-upgrade.json"
MANIFEST = "sw-manifest.txt"
NEW = ".kairi-new"
SETUP_MINUTES = 90              # stock's setup: code ~10 min, shaders more on a slow PC (melty-setup/build.py SETUP_MINUTES)
POLL_S = 0.5


class Fail(Exception):
    """a plain message for the player"""


def read_text(path, limit=4096):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read(limit)
    except OSError:
        return None


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest()


# ------------------------------------------------------------------------------------------------ what folder is it?
def manifest_version(sw):
    """'0.5.3-alpha.5' from sw-manifest.txt line 2 ('version 0.5.3-alpha.5'); '' when there is none"""
    for line in (read_text(os.path.join(sw, MANIFEST), 1024) or "").splitlines()[:4]:
        if line.startswith("version "):
            return line[8:].strip()
    return ""


def readme_version(sw):
    """0.4.x releases name their zip in README.md ('SavageWind-0.4.1-alpha.4-windows.zip'); '' when it does not"""
    m = re.search(r"SavageWind-(\d+\.\d+\.\d+(?:-[0-9A-Za-z.]+?)?)-(?:windows|linux|macos)", read_text(os.path.join(sw, "README.md"), 1 << 20) or "")
    return m.group(1) if m else ""


def sw_kind(sw):
    v = manifest_version(sw)
    # (s226 review G1) our journal counts only while the folder is still ours to finish: no sw-manifest.txt yet (it is
    # unpacked last) or 0.5.3's. A journal left behind in a folder that Savage Wind has since updated past 0.5.3 must not
    # get the 0.5.3 zip unpacked over it: that folder is "other" (refused, nothing changed).
    if os.path.isfile(os.path.join(sw, JOURNAL)) and (not v or v == SW_WANT):
        return "upgrading"
    if v:
        return "0.5.3" if v == SW_WANT else "other:" + v
    v = readme_version(sw)
    if v == SW_OLD:
        return "0.4.1"
    return "other:" + v if v else "unknown"


def program_files_ok(sw):
    """-> '' when every 'f <sha256> <size> <name>' line of sw-manifest.txt matches the file, else the first that doesn't"""
    txt = read_text(os.path.join(sw, MANIFEST), 1 << 20)
    if not txt:
        return MANIFEST
    n = 0
    for line in txt.splitlines():
        if not line.startswith("f "):
            continue
        parts = line.split(" ", 3)
        if len(parts) != 4:
            return line[:80]
        _, sha, size, name = parts
        p = os.path.join(sw, *name.split("/"))
        try:
            if os.path.getsize(p) != int(size) or sha256_file(p) != sha:
                return name
        except (OSError, ValueError):
            return name
        n += 1
    return "" if n else MANIFEST


# ------------------------------------------------------------------------------------------------ the stock zip
class _HttpsOnly(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if urllib.parse.urlparse(newurl).scheme != "https":
            raise Fail("The Savage Wind download was sent to a non-https address. Try again later.")
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def zip_ok(path):
    try:
        return os.path.getsize(path) == STOCK["size"] and sha256_file(path) == STOCK["sha256"]
    except OSError:
        return False


def fetch_stock(dest_dir, local=None, log=print, opener=None):
    """-> the path of the checked stock 0.5.3 zip. local = a copy on this PC (tests / dev; checked the same way, never copied).
    Else <dest_dir>/<name> when an earlier run left it complete, else a download (https only, size cap, sha256 pin) into
    <name>.part, renamed when it checks out."""
    if local:
        if not zip_ok(local):
            raise Fail("The Savage Wind %s zip at %s is not the official one (size or checksum differ)." % (SW_WANT, local))
        log("[sw] using %s" % local)
        return local
    os.makedirs(dest_dir, exist_ok=True)
    final = os.path.join(dest_dir, STOCK["name"])
    if zip_ok(final):
        log("[sw] %s already downloaded" % STOCK["name"])
        return final
    part = final + ".part"
    log("[sw] downloading %s (%.0f MB)" % (STOCK["url"], STOCK["size"] / 1e6))
    op = opener or urllib.request.build_opener(_HttpsOnly)
    h = hashlib.sha256()
    got = 0
    try:
        with op.open(urllib.request.Request(STOCK["url"], headers={"User-Agent": "LegendOfKairi-installer"}), timeout=60) as r, \
                open(part, "wb") as f:
            while True:
                b = r.read(1 << 20)
                if not b:
                    break
                got += len(b)
                if got > STOCK["size"]:
                    raise Fail("The Savage Wind download is bigger than the official zip. Try again later.")
                h.update(b)
                f.write(b)
    except Fail:
        raise
    except OSError as e:
        raise Fail("Could not download Savage Wind %s (%s). Check your internet connection and press the button again." % (SW_WANT, e))
    if got != STOCK["size"] or h.hexdigest() != STOCK["sha256"]:
        try:
            os.remove(part)
        except OSError:
            pass
        raise Fail("The Savage Wind download did not check out (it was cut off or changed). Press the button again.")
    os.replace(part, final)
    return final


def safe_name(name):
    """a zip member under 'Savage Wind/' -> its relative path parts, or None (dirs, '..', absolute, drive letters, odd chars)"""
    if not name.startswith(TOP) or name.endswith("/") or "\\" in name or ":" in name:
        return None
    parts = name[len(TOP):].split("/")
    if not parts or any(p in ("", ".", "..") or p != p.strip() or p.endswith(".") for p in parts):
        return None
    if any(p.lower().endswith(NEW) for p in parts):
        return None
    return parts


def unpack_stock(zpath, sw, log=print):
    """every file of the zip into sw, each through <file>.kairi-new + os.replace; sw-manifest.txt last.
    Files of the folder that the zip doesn't have (saves, settings, extracted/, cache/) are never touched."""
    with zipfile.ZipFile(zpath) as z:
        items = []
        for zi in z.infolist():
            if zi.is_dir():
                continue
            parts = safe_name(zi.filename)
            if parts is None:
                raise Fail("The Savage Wind zip has an odd file name (%s): nothing more was unpacked." % zi.filename)
            items.append((parts, zi))
        if not any(p == [MANIFEST] for p, _ in items):
            raise Fail("The Savage Wind zip has no %s." % MANIFEST)
        items.sort(key=lambda it: it[0] == [MANIFEST])      # the manifest last
        base = os.path.realpath(sw)
        for parts, zi in items:
            dst = os.path.join(sw, *parts)
            if not os.path.realpath(dst).startswith(base + os.sep):
                raise Fail("The Savage Wind zip points outside its folder (%s)." % zi.filename)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            tmp = dst + NEW
            with z.open(zi) as src, open(tmp, "wb") as f:
                shutil.copyfileobj(src, f, 1 << 20)
            try:
                os.replace(tmp, dst)
            except PermissionError:
                try:
                    os.remove(tmp)
                except OSError:
                    pass
                raise Fail("Windows would not let the installer replace %s (a program from that folder is running?). Close "
                           "Savage Wind and press the button again." % os.path.join(sw, *parts))
    log("[sw] unpacked %d files of Savage Wind %s" % (len(items), SW_WANT))
    return len(items)


# ------------------------------------------------------------------------------------------------ the old version's settings + saves
def backup_once(sw, dest, log=print):
    """savagewind.ini + save/ -> dest (once: a retry never overwrites the first, pre-upgrade copy). -> dest"""
    if os.path.isfile(os.path.join(dest, "done.txt")):
        return dest
    tmp = dest + ".tmp"
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    if os.path.isfile(os.path.join(sw, "savagewind.ini")):
        shutil.copy2(os.path.join(sw, "savagewind.ini"), tmp)
    if os.path.isdir(os.path.join(sw, "save")):
        shutil.copytree(os.path.join(sw, "save"), os.path.join(tmp, "save"))
    with open(os.path.join(tmp, "done.txt"), "w", encoding="utf-8") as f:
        f.write("Savage Wind's settings and saves before the upgrade to %s (%s)\n" % (SW_WANT, time.strftime("%Y-%m-%d %H:%M")))
    shutil.rmtree(dest, ignore_errors=True)
    os.replace(tmp, dest)
    log("[sw] saved a copy of the settings and saves in %s" % dest)
    return dest


# ------------------------------------------------------------------------------------------------ stock's setup
def exe_has_id(exe, gid):
    """does this program carry that game library id (its GAMELIB_ID / GAMELIB_ID_COMPAT string, runtime/gamelib.c)?"""
    with open(exe, "rb") as f:
        return re.search(rb"(?<![0-9a-f])" + gid.encode("ascii") + rb"\x00", f.read()) is not None


def game_id(sw):
    """the id of the game code in sw/extracted ('' when there is none)"""
    gid = (read_text(os.path.join(sw, "extracted", GAMELIB + ".id"), 128) or "").strip()
    return gid if re.fullmatch(r"[0-9a-f]{16}", gid) else ""


def game_files_state(sw, our_exe):
    """-> '' when the game files in sw are set up for our_exe (Legend of Kairi's savagewind.exe) and stock 0.5.3, else what
    is missing (plain words). As melty-setup/build.py game_files_state with the data folder = the Savage Wind folder."""
    ext = os.path.join(sw, "extracted")
    if not os.path.isfile(os.path.join(ext, "exefs", "main")):
        return "not set up yet"
    if not os.path.isfile(os.path.join(ext, "romfs", "Pack", "Bootup.pack")):
        return "the copy of the game files is incomplete"
    gid = game_id(sw)
    if not os.path.isfile(os.path.join(ext, GAMELIB)) or not gid:
        return "the game code is not built"
    if not os.path.isfile(our_exe):
        raise Fail("Legend of Kairi's download is incomplete (bin\\savagewind.exe is missing). Download it again.")
    if not exe_has_id(our_exe, gid) or not exe_has_id(os.path.join(sw, "savagewind.exe"), gid):
        return "the game code is another Savage Wind version's (%s)" % gid
    ver = manifest_version(sw)
    if ver and (read_text(os.path.join(sw, "cache", "last-version.txt"), 128) or "").strip() != ver:
        return "Savage Wind %s's setup has not finished" % ver
    return ""


def run_stock_setup(sw, out, log=print, on_proc=None, alive=None, minutes=SETUP_MINUTES, exe=None):
    """ONE run of stock savagewind.exe's no-click setup that keeps the game files copy in sw/extracted
    (SAVAGEWIND_LAUNCHER_EXTRACT="": extract_thread's "copied already" branch builds the game code + shaders + the version
    mark). Its log goes to out. on_proc(p): the caller ties it to its job. alive(): False = stop now.
    -> ("ready" | "refused" | "failed" | "stopped", why). refused = NOT READY before building any game code."""
    exe = exe or os.path.join(sw, "savagewind.exe")
    env = {k: v for k, v in os.environ.items() if not k.upper().startswith(("SAVAGEWIND_", "SORA_"))}
    env.update(SAVAGEWIND_DATA_ROOT=sw, SAVAGEWIND_LAUNCHER_EXTRACT="", SAVAGEWIND_NO_UPDATE_CHECK="1", SAVAGEWIND_HIDDEN="1")
    kw = {}
    if sys.platform == "win32":
        kw["creationflags"] = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "wb") as f:
        p = subprocess.Popen([exe, "--no-launch-flags"], cwd=sw, env=env, stdout=f, stderr=subprocess.STDOUT,
                             stdin=subprocess.DEVNULL, **kw)
    if on_proc:
        on_proc(p)
    t0, seen, text, stopped = time.time(), 0, "", False
    try:
        while True:
            ended = p.poll() is not None
            text = read_text(out, 64 << 20) or ""
            lines = text.split("\n")
            if not ended:
                lines = lines[:-1]
            for line in lines[seen:]:
                line = line.rstrip("\r")
                if line.startswith(("[launcher]", "[gamebuild]", "[update]")) or (
                        line.startswith("[precompile]") and "translation failed" not in line):
                    log("[sw] " + line)
            seen = max(seen, len(lines))
            if ended:
                break
            if alive is not None and not alive():
                stopped = True
                break
            if time.time() - t0 > minutes * 60:
                raise Fail("Savage Wind's setup took more than %d minutes. Press the button again to go on with it. Its log: %s"
                           % (minutes, out))
            time.sleep(POLL_S)
    finally:
        if p.poll() is None:
            p.terminate()
            try:
                p.wait(15)
            except subprocess.TimeoutExpired:
                p.kill()
    if stopped:
        return "stopped", ""
    res = re.findall(r"^\[launcher\] setup: (.*) -> (READY|NOT READY)\r?$", text, re.M)
    failed = [m.split(" | ")[0] for m in re.findall(r"^\[launcher\] FAILED: (.*?)\r?$", text, re.M)]
    if res and res[-1][1] == "READY":
        log("[sw] Savage Wind's setup finished in %.0f s" % (time.time() - t0))
        return "ready", ""
    why = failed[-1] if failed else (res[-1][0] if res else "it ended early (exit %s)" % p.returncode)
    if res and "[gamebuild]" not in text and "disk space" not in why.lower():
        return "refused", why
    return "failed", why


# ------------------------------------------------------------------------------------------------ the whole step
def kind_message(kind):
    """the plain refusal for a folder Legend of Kairi can't use ('' for the kinds it can)"""
    if kind in ("0.5.3", "0.4.1", "upgrading"):
        return ""
    found = "Savage Wind %s" % kind[6:] if kind.startswith("other:") else "a Savage Wind version it can't tell"
    coop = "(Playing co-op? Stay on Legend of Kairi 0.1.4 with Savage Wind %s.)" % SW_OLD
    v = kind[6:] if kind.startswith("other:") else ""
    # (s226) 0.5.0 - 0.5.2: Savage Wind's own way up to 0.5.3 (upstream's release notes: 0.5.1 added its one-click update;
    # a 0.5.0 folder takes the new zip over it). We don't upgrade these ourselves (only 0.4.1, the 0.1.4 players' folder).
    if v.startswith(("0.5.1-", "0.5.2-")) or v in ("0.5.1", "0.5.2"):
        return ("This folder has %s. This Legend of Kairi needs Savage Wind %s. Open Savage Wind Configuration in that folder and "
                "press Update, start Savage Wind once and let it finish its setup, then run this installer again. Nothing was "
                "changed. %s" % (found, SW_WANT, coop))
    if v.startswith("0.5.0-") or v == "0.5.0":
        return ("This folder has %s. This Legend of Kairi needs Savage Wind %s. Download Savage Wind %s and extract its zip over "
                "that folder (your game files, saves and settings stay), start Savage Wind once and let it finish its setup, "
                "then run this installer again. Nothing was changed. %s" % (found, SW_WANT, SW_WANT, coop))
    return ("This folder has %s. This Legend of Kairi works with Savage Wind %s (it can also bring a %s folder up to %s for "
            "you). Download Savage Wind %s, or choose another folder. Nothing was changed. %s"
            % (found, SW_WANT, SW_OLD, SW_WANT, SW_WANT, coop))


def busy(sw):
    """'' unless a program from the Savage Wind folder runs (the stock game, its configuration tool, its updater)"""
    try:
        import update
        r = update.running_from(sw)
    except Exception:
        r = []
    return ("Savage Wind is running from that folder (%s). Close it first, then press the button again." % os.path.basename(r[0])) if r else ""


def ensure_ready(sw, our_exe, work, log=print, local_zip=None, on_proc=None, alive=None, busy_check=busy, run_setup=run_stock_setup):
    """the Savage Wind folder ready for Legend of Kairi: an upgrade from 0.4.1 when needed, stock's setup when its game code
    isn't 0.5.3's. work = <Legend of Kairi>/work (download + backup + setup log under work/sw-upgrade). Raises Fail.
    -> "ready" (nothing to do) | "upgraded" | "set up" | "stopped" """
    kind = sw_kind(sw)
    msg = kind_message(kind)
    if msg:
        raise Fail(msg)
    up = os.path.join(work, "sw-upgrade")
    did = "ready"
    if kind in ("0.4.1", "upgrading"):
        b = busy_check(sw)
        if b:
            raise Fail(b)
        z = fetch_stock(up, local_zip, log)          # before the first write into the folder: a failed download changes nothing
        if alive is not None and not alive():
            return "stopped"
        jp = os.path.join(sw, JOURNAL)
        j = {}
        if os.path.isfile(jp):
            try:
                j = json.load(open(jp, encoding="utf-8"))
            except (OSError, ValueError):
                j = {}
        backup_once(sw, os.path.join(up, "backup-" + j.get("from", SW_OLD)), log)
        if not j:
            j = {"from": SW_OLD, "to": SW_WANT, "started": time.strftime("%Y-%m-%d %H:%M:%S")}
            with open(jp + ".tmp", "w", encoding="utf-8") as f:
                json.dump(j, f)
            os.replace(jp + ".tmp", jp)
            log("[sw] bringing Savage Wind %s up to %s in place (your saves and settings stay)" % (SW_OLD, SW_WANT))
        else:
            log("[sw] carrying on with the upgrade to %s" % SW_WANT)
        unpack_stock(z, sw, log)
        bad = program_files_ok(sw)
        if bad:
            raise Fail("Savage Wind %s's files did not come out right (%s). Press the button again." % (SW_WANT, bad))
        did = "upgraded"
    elif not os.path.isfile(os.path.join(sw, "extracted", "exefs", "main")):
        raise Fail("Savage Wind %s is here, but its game files are not set up yet. Start Savage Wind once, choose your game "
                   "files folder and let it finish, then come back." % SW_WANT)
    why = game_files_state(sw, our_exe)
    if why:
        b = busy_check(sw)
        if b:
            raise Fail(b)
        out = os.path.join(up, "stock-setup.log")
        log("[sw] game files: %s; Savage Wind's own setup builds the %s game code (keeps the game files copy; about 10-20 "
            "minutes)" % (why, SW_WANT))
        res, reason = run_setup(sw, out, log, on_proc, alive)
        if res == "stopped":
            return "stopped"
        if res == "refused":
            raise Fail("Savage Wind %s turned down the game files in that folder (%s). Start savagewind.exe in your Savage Wind "
                       "folder once, choose your game files folder and let it finish, then press the button again. Your saves "
                       "and settings are kept." % (SW_WANT, reason))
        if res != "ready":
            raise Fail("Savage Wind's setup did not finish (%s). Press the button again to go on with it. Its log: %s" % (reason, out))
        why = game_files_state(sw, our_exe)
        if why:
            raise Fail("Savage Wind's setup ended, but %s: it doesn't fit this Legend of Kairi release. Its log: %s" % (why, out))
        did = "set up" if did == "ready" else did
    try:
        os.remove(os.path.join(sw, JOURNAL))
    except FileNotFoundError:
        pass
    log("[sw] Savage Wind %s is ready" % SW_WANT)
    return did

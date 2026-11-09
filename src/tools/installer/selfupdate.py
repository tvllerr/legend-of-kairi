"""Legend of Kairi self-updater (stdlib only; runs with the install's own kit python).
Looks on GitHub for a newer release, downloads + checks + unpacks it into <install>/.kairi-download/,
then starts the NEW release's installer in --update mode and exits at once (so nothing from <install>/kit keeps running)."""
import argparse, hashlib, http.client, json, os, re, shutil, subprocess, sys, time, traceback, urllib.error, urllib.request, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import update  # version_tuple, NAME, _read_json
import sigcheck  # (s186) Ed25519 via the kit python's own OpenSSL

API = "https://api.github.com/repos/tvllerr/legend-of-kairi/releases"
DL = ".kairi-download"
BS = chr(92)   # a backslash, for the Windows paths in messages
UA = "LegendOfKairi-Updater"
DL_PREFIX = "https://github.com/tvllerr/legend-of-kairi/releases/download/"   # (s185 sec) the only place a release zip may come from
VER_RE = re.compile(r"\d+(\.\d+){0,3}(-[A-Za-z0-9.]+)?")                         # a release version, as it may appear in a file name
MAX_API = 5 << 20       # the releases JSON is read at most this big
# (s186) signed updates: a release is only taken when its "<zip name>.sig" asset is a valid Ed25519 signature (by one of
# these public keys) of sigcheck.message(zip name, size, sha256). The private key never leaves the author's PC, so somebody who
# gets into the GitHub account still cannot push an update to players. More than one key = key change-over.
PUBKEYS = ["20cbed9b257173dce6201213cf1246b6dda4d497b86dc6e90e1f57938ce8151d"]
MAX_SIG = 1024          # a .sig asset is 128 hex characters
DEV = False             # True only when --api was given on the command line (tests / dev): file:// URLs are allowed then


class Fail(Exception):
    pass


def pid_alive(pid):
    try:
        import ctypes
        k = ctypes.windll.kernel32
        k.OpenProcess.restype = ctypes.c_void_p
        k.GetExitCodeProcess.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        k.CloseHandle.argtypes = [ctypes.c_void_p]
        h = k.OpenProcess(0x1000, False, int(pid))  # PROCESS_QUERY_LIMITED_INFORMATION
        if not h:
            return False
        code = ctypes.c_ulong()
        ok = k.GetExitCodeProcess(h, ctypes.byref(code))
        k.CloseHandle(h)
        return bool(ok) and code.value == 259  # STILL_ACTIVE
    except Exception:
        return False


def _runs_from(pid, d):
    """(s184 review) the lock's pid is a program inside d (a reused pid of some other program does not block the update)"""
    try:
        import ctypes
        from ctypes import wintypes
        k = ctypes.windll.kernel32
        k.OpenProcess.restype = wintypes.HANDLE
        h = k.OpenProcess(0x1000, False, int(pid))
        if not h:
            return True          # cannot look (access denied): be careful, count it as ours
        try:
            buf, n = ctypes.create_unicode_buffer(1024), wintypes.DWORD(1024)
            if not k.QueryFullProcessImageNameW(wintypes.HANDLE(h), 0, buf, ctypes.byref(n)):
                return True
            return os.path.normcase(os.path.realpath(buf.value)).startswith(os.path.normcase(os.path.realpath(d)) + os.sep)
        finally:
            k.CloseHandle(wintypes.HANDLE(h))
    except Exception:
        return True


def find_live_lock(d, depth=4):
    """a .installer-ui/lock with a live pid somewhere under d (the new installer's own folder)"""
    for cur, dirs, files in os.walk(d):
        if cur[len(d):].count(os.sep) >= depth:
            dirs[:] = []
        if os.path.basename(cur) == ".installer-ui" and "lock" in files:
            try:
                with open(os.path.join(cur, "lock")) as f:
                    pid = int(f.read().strip() or 0)
            except (OSError, ValueError):
                pid = 0
            if pid and pid != os.getpid() and pid_alive(pid) and _runs_from(pid, d):
                return True
    return False


def cleanup_old(root):
    d = os.path.join(root, DL)
    if not os.path.isdir(d):
        return
    if find_live_lock(d):
        print("The installer window of the last update is still open. Finish the update there (or close that window), then run this again.")
        sys.exit(2)
    for n in os.listdir(d):
        p = os.path.join(d, n)
        try:
            shutil.rmtree(p) if os.path.isdir(p) else os.remove(p)
        except OSError as e:
            raise Fail("Could not clean up the old download folder (%s): %s" % (p, e))


class _HttpsOnly(urllib.request.HTTPRedirectHandler):
    """(s185 sec) a redirect (GitHub sends release downloads to another https host) may never go to plain http or any other scheme"""
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if not DEV and not newurl.lower().startswith("https://"):
            raise Fail("GitHub redirected the download to an address that is not https, so it was refused.")
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def http_open(url, accept=None):
    h = {"User-Agent": UA}
    if accept:
        h["Accept"] = accept
    if not DEV and not url.lower().startswith("https://"):
        raise Fail("The update address is not https, so it was refused.")
    try:
        return urllib.request.build_opener(_HttpsOnly).open(urllib.request.Request(url, headers=h), timeout=30)
    except urllib.error.HTTPError as e:
        if e.code in (403, 429):
            raise Fail("GitHub is limiting requests from your connection right now. Try again in a little while.")
        raise Fail("GitHub answered with an error (%s). Try again later." % e.code)
    except (urllib.error.URLError, OSError) as e:
        raise Fail("Could not reach GitHub. Check your internet connection and try again. (%s)" % getattr(e, "reason", e))


def find_newest(api):
    """-> (tag, version_string, asset) of the newest usable release, or None; refusals are printed"""
    with http_open(api, "application/vnd.github+json") as r:
        try:
            raw = r.read(MAX_API + 1)
            if len(raw) > MAX_API:
                raise Fail("GitHub sent a far bigger answer than expected, so it was refused.")
            rels = json.loads(raw.decode("utf-8"))
        except (ValueError, UnicodeDecodeError):
            raise Fail("GitHub sent something unreadable. Try again later.")
    best = None
    for rel in rels if isinstance(rels, list) else []:
        if rel.get("draft") or rel.get("prerelease"):
            continue
        tag = str(rel.get("tag_name") or "")
        ver = tag[1:] if tag[:1] in ("v", "V") else tag
        asset = next((a for a in rel.get("assets") or [] if str(a.get("name", "")) == "Legend-of-Kairi-%s.zip" % ver), None)
        if not asset or not VER_RE.fullmatch(ver):
            continue
        if not re.fullmatch(r"sha256:[0-9a-fA-F]{64}", str(asset.get("digest") or "")):
            print("Skipping %s: GitHub gave no checksum for its download, so it cannot be checked." % tag)
            continue
        sig = next((a for a in rel.get("assets") or [] if str(a.get("name", "")) == asset["name"] + ".sig"), None)
        if not sig:
            print("Skipping %s: it is not signed." % tag)
            continue
        if best is None or update.version_tuple(ver) > update.version_tuple(best[1]):
            best = (tag, ver, asset, sig)
    return best


def check_signature(asset, sig, keys):
    """(s186) fetch the small .sig asset and check it against what GitHub announced for the zip (name, size, sha256);
    the download itself is then held to that same size + sha256. Raises Fail when anything is off."""
    url = str(sig.get("browser_download_url") or "")
    if not DEV and not url.startswith(DL_PREFIX):
        raise Fail("The signature address is not one of this project's GitHub release addresses, so it was refused.")
    with http_open(url) as r:
        try:
            raw = r.read(MAX_SIG + 1)
        except (OSError, http.client.HTTPException) as e:
            raise Fail("Could not fetch the update's signature. Check your internet connection and try again. (%s)" % e)
    bad = Fail("This update is not signed by the Legend of Kairi author, so it was refused. Nothing was changed.")
    try:
        text = raw.decode("ascii").strip()
    except UnicodeDecodeError:
        raise bad
    if len(raw) > MAX_SIG or not re.fullmatch(r"[0-9a-fA-F]{128}", text):
        raise bad
    msg = sigcheck.message(asset["name"], int(asset["size"]), asset["digest"].split(":", 1)[1])
    try:
        ok = any(sigcheck.verify(bytes.fromhex(k), bytes.fromhex(text), msg) for k in keys)
    except (sigcheck.SigError, ValueError) as e:
        raise Fail("Could not check the update's signature (%s). Nothing was changed." % e)
    if not ok:
        raise bad


def _rm(p):
    try:
        os.remove(p)
    except OSError:
        pass


def download(url, dest, size, digest):
    d = os.path.dirname(dest)
    os.makedirs(d, exist_ok=True)
    free = shutil.disk_usage(d).free
    if free < size * 4:
        raise Fail("Not enough free disk space: need about %d MB, only %d MB free." % (size * 4 >> 20, free >> 20))
    part = dest + ".part"
    h = hashlib.sha256()
    done = 0
    last = 0
    try:
        with http_open(url) as r, open(part, "wb") as f:
            cl = r.headers.get("Content-Length") if getattr(r, "headers", None) else None
            if cl and cl.isdigit() and int(cl) > size:
                raise Fail("The download is bigger than GitHub announced (%s bytes, expected %d), so it was stopped." % (cl, size))
            while True:
                b = r.read(1 << 20)
                if not b:
                    break
                f.write(b)
                h.update(b)
                done += len(b)
                if done > size:         # (s185 sec) stop at once, do not wait for the end of a stream that never ends
                    raise Fail("The download is bigger than GitHub announced (more than %d bytes), so it was stopped." % size)
                if time.time() - last >= 1:
                    last = time.time()
                    print("\r  %d / %d MB" % (done >> 20, size >> 20), end="", flush=True)
        print("\r  %d / %d MB" % (done >> 20, size >> 20))
    except Fail:
        _rm(part)
        raise
    except (OSError, http.client.HTTPException) as e:
        _rm(part)
        if getattr(e, "errno", 0) == 28:
            raise Fail("The disk is full. Free some space and try again.")
        raise Fail("The download was interrupted. Check your internet connection and try again. (%s)" % e)
    if done != size:
        _rm(part)
        raise Fail("The download is the wrong size (%d bytes, expected %d). Try again." % (done, size))
    if h.hexdigest().lower() != digest.lower():
        _rm(part)
        raise Fail("The download did not match its checksum, so it was thrown away. Try again.")
    os.replace(part, dest)


def safe_extract(zpath, out):
    try:
        z = zipfile.ZipFile(zpath)
    except zipfile.BadZipFile:
        raise Fail("The downloaded file is not a valid zip.")
    with z:
        infos = z.infolist()
        tops = set()
        for i in infos:
            n = i.filename.replace("\\", "/")
            parts = [p for p in n.split("/") if p]
            if n.startswith("/") or re.match(r"^[A-Za-z]:", n) or ".." in parts or not parts:
                raise Fail("The download contains an unsafe file name (%s) and was refused." % i.filename)
            tops.add(parts[0])
            if len(parts) == 1 and not n.endswith("/"):
                raise Fail("The download is not laid out as one release folder.")
        if len(tops) != 1:
            raise Fail("The download is not laid out as one release folder.")
        top = tops.pop()
        # (s184 review) the zip's top folder is dropped and out is short: Windows without long paths stops at 259 characters
        longest = max(len(i.filename) - len(top) for i in infos)
        if len(os.path.abspath(out)) + longest > 259 and not long_paths_on():
            raise Fail(("The path of this Legend of Kairi folder is too long for Windows to unpack the update into it:\n  %s\n"
                        "Move the whole folder to a shorter path (for example C:" + BS + "Games" + BS + "Legend of Kairi), "
                        "then run this again.") % os.path.dirname(os.path.dirname(os.path.abspath(out))))
        os.makedirs(out, exist_ok=True)
        base = os.path.realpath(out)
        for i in infos:
            rel = i.filename.replace("\\", "/").split("/", 1)[1] if "/" in i.filename.replace("\\", "/") else ""
            if not rel:
                continue
            t = os.path.realpath(os.path.join(out, rel))
            if not t.startswith(base + os.sep):
                raise Fail("The download contains an unsafe file name (%s) and was refused." % i.filename)
            if rel.endswith("/"):
                os.makedirs(t, exist_ok=True)
                continue
            os.makedirs(os.path.dirname(t), exist_ok=True)
            with z.open(i) as src, open(t, "wb") as dst:
                shutil.copyfileobj(src, dst, 1 << 20)
    return out


def long_paths_on():
    try:
        import winreg
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Control\FileSystem") as k:
            return winreg.QueryValueEx(k, "LongPathsEnabled")[0] == 1
    except OSError:
        return False


def run(a):
    # realpath: %~dp0 can be an 8.3 short path; the installer stores this folder for the launchers
    root = os.path.realpath(os.path.abspath(a.root or os.path.join(HERE, "..", "..")))
    m = update._read_json(os.path.join(root, "manifest.json")) or {}
    if m.get("release") != update.NAME:
        raise Fail("%s is not a Legend of Kairi install folder (no valid manifest.json)." % root)
    cur = str(m.get("version", "0"))
    if not (os.path.isdir(os.path.join(root, "data")) and os.path.isdir(os.path.join(root, "work"))):
        raise Fail("This Legend of Kairi was never installed. Run Install Legend of Kairi first.")
    if not a.check:
        cleanup_old(root)
    best = find_newest(a.api)
    if best is None:
        raise Fail("Could not find any downloadable release on GitHub.")
    tag, ver, asset, sig = best
    newer = update.version_tuple(ver) > update.version_tuple(cur)
    unfinished = os.path.isfile(update.journal_path(root))    # an earlier update stopped part way: get it again + finish it
    if unfinished and not newer and not a.check:
        print("An earlier update of this folder did not finish. Getting %s again to finish it." % ver)
    if not newer and not a.force and not (unfinished and not a.check):
        print("Legend of Kairi %s is up to date (newest on GitHub: %s)." % (cur, ver))
        return 2
    size = int(asset["size"])
    print("Legend of Kairi %s is available (you have %s), download size %d MB." % (ver, cur, size >> 20))
    if a.check:
        return 0
    if not a.yes:
        try:
            ans = input("Download and update? [Y/n] ").strip().lower()
        except EOFError:
            ans = "n"
        if ans not in ("", "y", "yes"):
            print("Cancelled.")
            return 2
    dl = os.path.join(root, DL)
    name = asset["name"]
    zpath = os.path.join(dl, name)
    url = str(asset.get("browser_download_url") or "")
    if not DEV and not url.startswith(DL_PREFIX):
        raise Fail("The download address is not one of this project's GitHub release addresses, so it was refused.")
    check_signature(asset, sig, a.keys)
    print("Signature OK.")
    print("Downloading %s ..." % name)
    download(asset["browser_download_url"], zpath, size, asset["digest"].split(":", 1)[1])
    print("Unpacking ...")
    out = os.path.join(dl, "n")      # short on purpose (path length)
    try:
        new = safe_extract(zpath, out)
    except Fail:
        shutil.rmtree(out, ignore_errors=True)
        raise
    except OSError as e:
        shutil.rmtree(out, ignore_errors=True)
        if getattr(e, "errno", 0) == 28:
            raise Fail("The disk is full. Free some space and try again.")
        raise Fail(("Could not unpack the download (%s). If the path of this folder is long, move it to a shorter one "
                    "(for example C:" + BS + "Games" + BS + "Legend of Kairi) and run this again.") % e)
    nm = update._read_json(os.path.join(new, "manifest.json")) or {}
    need = ["tools/installer/installer.py", "kit/python/pythonw.exe"]
    if nm.get("release") != update.NAME or nm.get("version") != ver or any(not os.path.isfile(os.path.join(new, p)) for p in need):
        shutil.rmtree(out, ignore_errors=True)
        raise Fail("The downloaded release looks incomplete or is not the version GitHub announced. Nothing was changed.")
    _rm(zpath)
    if a.no_launch:
        print(new)
        return 0
    subprocess.Popen([os.path.join(new, "kit", "python", "pythonw.exe"), "-E", "-P",
                      os.path.join(new, "tools", "installer", "installer.py"), "--update", root],
                     cwd=new, close_fds=True, creationflags=0x00000008 | 0x00000200)  # DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP
    print("The installer window opens: press Update Legend of Kairi.")
    return 0


def main():
    ap = argparse.ArgumentParser(description="Update Legend of Kairi to the newest release", allow_abbrev=False)
    ap.add_argument("--root", help="the install folder (default: two folders above this file)")
    ap.add_argument("--api", default=None, help="releases list URL (tests / dev only: also allows file:// downloads)")
    ap.add_argument("--check", action="store_true", help="only say whether a newer release exists")
    ap.add_argument("--yes", action="store_true", help="do not ask")
    ap.add_argument("--force", action="store_true", help="take the newest release even if it is the same version")
    ap.add_argument("--no-launch", action="store_true", help="stop after unpacking")
    ap.add_argument("--pubkey", action="append", help="tests only (needs --api): check signatures against this key instead")
    a = ap.parse_args()
    global DEV
    DEV = a.api is not None
    a.keys = a.pubkey if (DEV and a.pubkey) else PUBKEYS      # outside tests the built-in keys are the only ones
    a.api = a.api or API
    try:
        return run(a)
    except Fail as e:
        print("\n" + str(e))
        return 1
    except Exception:
        traceback.print_exc()
        print("\nSomething unexpected went wrong. Nothing was installed.")
        return 1


if __name__ == "__main__":
    sys.exit(main())

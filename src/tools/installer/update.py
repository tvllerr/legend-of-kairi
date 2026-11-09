"""Legend of Kairi installer: update an OLDER install in place (s183), instead of a new 15-minute install.

The player picks the folder of an older Legend of Kairi (it has manifest.json "release": "Legend of Kairi" + data/ + work/). Then:
  1. prepare(): this release's program files (bin/ kit/ tools/ LICENSES/ the Install .cmd, manifest.json last) replace the old ones.
     Each one is first copied into <old>/.kairi-update/stage/, then swapped in with two renames; the old copy is kept in
     <old>/.kairi-update/bak/ until the update finished (verify ALL SAME + launchers written). data/, work/ (the KH2 extract and the
     done markers), logs/, .installer-ui/ and the launchers stay. Saves live in the Savage Wind folder and are never touched.
  2. invalidate(): the data files whose reference sha256 changed between the OLD release's manifest and this one (or are new / gone):
     their file is deleted and the done marker of the step that writes it is removed, so sorabuild re-runs exactly those steps (and the
     steps after them). New steps have no marker and run anyway; kh2pick re-picks by itself when kit/kh2_files.json changed.
  3. the normal build + verify + game side run in the old folder (installer.py).
  4. finish(): .kairi-update/ (journal + bak) is deleted.
A crash or power loss at any point leaves a folder that the next Update (or a full Install there) repairs: the journal remembers the
original version + manifest, a half-copied stage is thrown away, an item whose bak exists is never backed up twice.
"""
import fnmatch
import json
import os
import re
import shutil
import sys

NAME = "Legend of Kairi"
UPD = ".kairi-update"
# what a release folder ships at its top level (mkzip writes the same list into manifest.json "top"; older manifests have none)
DEFAULT_TOP = ["bin", "kit", "tools", "LICENSES", "Install Legend of Kairi.cmd", "manifest.json"]
# never copied over / never moved away, whatever a manifest says: the player's build, logs, choices, launchers
NEVER = {"data", "work", "logs", ".installer-ui", UPD, "save", "saves", "extracted"}
NEVER_GLOB = ["Play Legend of Kairi*.bat"]
SUBDIRS = {"bin", "data", "work", "tools", "kit", "logs", "licenses"}


def version_tuple(v):
    """'0.1.2-alpha' -> (0, 1, 2); unknown parts -> 0"""
    m = re.match(r"\s*(\d+)(?:\.(\d+))?(?:\.(\d+))?", v or "")
    return tuple(int(x or 0) for x in m.groups()) if m else (0, 0, 0)


def _read_json(p):
    try:
        with open(p, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def journal_path(root):
    return os.path.join(root, UPD, "state.json")


def read_journal(root):
    return _read_json(journal_path(root)) or {}


def write_journal(root, j):
    p = journal_path(root)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    tmp = p + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        json.dump(j, f, indent=1)
    os.replace(tmp, p)


def old_choices(root):
    """the Savage Wind + KH2 folders the old install used: .installer-ui/choices.json, else work/install_config.json"""
    c = _read_json(os.path.join(root, ".installer-ui", "choices.json")) or {}
    sw, kh2 = c.get("sw") or "", c.get("kh2") or ""
    cfg = _read_json(os.path.join(root, "work", "install_config.json")) or {}
    if not sw and cfg.get("romfs"):
        r = cfg["romfs"].replace("\\", "/").rstrip("/")
        if r.lower().endswith("/extracted/romfs"):
            sw = r[: -len("/extracted/romfs")]
    if not kh2 and cfg.get("kh2exe"):
        kh2 = os.path.dirname(cfg["kh2exe"].replace("\\", "/"))
    return sw, kh2


def detect(path, release, new_version):
    """-> (ok, message, info). info: root, version (the version it was before any update started), sw, kh2, same (bool)."""
    if not path:
        return False, "Choose the folder of your older Legend of Kairi.", {}
    p = os.path.normpath(path)
    if not os.path.isdir(p):
        return False, "This folder does not exist.", {}
    if os.path.basename(p).lower() in SUBDIRS and os.path.isfile(os.path.join(os.path.dirname(p), "manifest.json")):
        p = os.path.dirname(p)        # picked bin/ or data/ inside it
    if not p.isascii():
        return False, "This folder's path has letters Windows programs here cannot handle. Move it to a path with plain letters only.", {}
    if os.path.normcase(os.path.realpath(p)) == os.path.normcase(os.path.realpath(release)):
        return False, ("This is the folder of the new download itself. Choose the folder of your OLDER Legend of Kairi "
                       "(the one with Play Legend of Kairi.bat in it)."), {}
    man = _read_json(os.path.join(p, "manifest.json"))
    if not man or man.get("release") != NAME:
        if os.path.isfile(os.path.join(p, "savagewind.exe")):
            return False, ("This is the Savage Wind folder. Choose the Legend of Kairi folder instead (the one with "
                           "Play Legend of Kairi.bat in it)."), {}
        return False, "This is not a Legend of Kairi folder (it has no Legend of Kairi manifest.json).", {}
    rp, rr = os.path.normcase(os.path.realpath(p)), os.path.normcase(os.path.realpath(release))
    if rr.startswith(rp + os.sep) or rp.startswith(rr + os.sep):
        return False, "The old install and the new download must be two separate folders (one is inside the other).", {}
    if not (os.path.isdir(os.path.join(p, "data")) and os.path.isdir(os.path.join(p, "work"))):
        return False, ("This Legend of Kairi was never installed (no data and work folders). Use Install Legend of Kairi "
                       "instead."), {}
    j = read_journal(p)
    old = j.get("from_version") or man.get("version") or "?"
    cur = man.get("version") or "?"
    if version_tuple(cur) > version_tuple(new_version):
        return False, ("This install is Legend of Kairi %s, newer than this download (%s). Download the newest release to "
                       "update it." % (cur, new_version)), {}
    sw, kh2 = old_choices(p)
    info = {"root": p.replace("\\", "/"), "version": old, "sw": sw, "kh2": kh2, "same": old == new_version,
            "resume": bool(j)}
    if j:
        msg = "Legend of Kairi %s, its update to %s did not finish. Update again to finish it." % (old, new_version)
    elif old == new_version:
        msg = "Legend of Kairi %s, the same version. Update checks every file and repairs what is wrong." % old
    else:
        msg = "Legend of Kairi %s. Update brings it to %s." % (old, new_version)
    return True, msg, info


def release_items(release):
    man = _read_json(os.path.join(release, "manifest.json")) or {}
    top = man.get("top") or DEFAULT_TOP
    out = []
    for n in top:
        if n in NEVER or any(fnmatch.fnmatch(n, g) for g in NEVER_GLOB) or "/" in n or "\\" in n or n in (".", ".."):
            continue
        if os.path.exists(os.path.join(release, n)):
            out.append(n)
    # manifest.json last: while it still says the old version, the swap is not complete
    out.sort(key=lambda n: (n == "manifest.json", n.lower()))
    if "manifest.json" not in out:
        raise RuntimeError("This download has no manifest.json. Download Legend of Kairi again.")
    return out


def _rm(p):
    if os.path.isdir(p) and not os.path.islink(p):
        shutil.rmtree(p)
    elif os.path.lexists(p):
        os.remove(p)


def _copy(src, dst):
    if os.path.isdir(src):
        shutil.copytree(src, dst, ignore=shutil.ignore_patterns("__pycache__"))
    else:
        shutil.copy2(src, dst)


def prepare(release, root, log=print):
    """steps 1 (copy + swap). Safe to repeat. Raises RuntimeError (plain words); on a failed swap the items of this run go back."""
    release, root = os.path.abspath(release), os.path.abspath(root)
    items = release_items(release)
    u = os.path.join(root, UPD)
    stage, bak = os.path.join(u, "stage"), os.path.join(u, "bak")
    j = read_journal(root)
    if not j:
        man = _read_json(os.path.join(root, "manifest.json")) or {}
        j = {"from_version": man.get("version", "?"), "phase": "staging"}
        # the OLD reference manifest of the data (what is in data/ now): needed to see which files change
        om = os.path.join(root, "tools", "sorabuild", "manifest.thispc.json")
        os.makedirs(u, exist_ok=True)
        if os.path.isfile(om):
            shutil.copy2(om, os.path.join(u, "old_data_manifest.json"))
        write_journal(root, j)
    to_v = (_read_json(os.path.join(release, "manifest.json")) or {}).get("version", "?")
    j.update({"to_version": to_v, "items": items, "phase": "staging"})
    write_journal(root, j)
    # stage: a fresh copy every time (a half copy from a crash is thrown away)
    if os.path.exists(stage):
        shutil.rmtree(stage)
    os.makedirs(stage)
    os.makedirs(bak, exist_ok=True)
    for n in items:
        try:
            _copy(os.path.join(release, n), os.path.join(stage, n))
        except OSError as e:
            shutil.rmtree(stage, ignore_errors=True)       # the old folder is untouched so far
            raise RuntimeError("Could not copy %s into the old folder (%s). Is the disk full?" % (n, e))
    log("[update] copied %d program items next to the old ones" % len(items))
    j["phase"] = "swapping"
    write_journal(root, j)
    done = []      # (name, had_bak_before) of this run, to undo on failure
    try:
        for n in items:
            cur, b, s = os.path.join(root, n), os.path.join(bak, n), os.path.join(stage, n)
            had = os.path.lexists(b)
            done.append((n, had))           # before the first rename: a failure in the middle of this item is undone too
            if os.path.lexists(cur):
                if had:
                    _rm(cur)            # a copy from an earlier update attempt; bak already holds the original
                else:
                    os.rename(cur, b)
            os.rename(s, cur)
    except OSError as e:
        for n, had in reversed(done):          # put this run's items back the way they were
            cur, b = os.path.join(root, n), os.path.join(bak, n)
            try:
                if not had and os.path.lexists(b):
                    if os.path.lexists(cur):
                        _rm(cur)
                    os.rename(b, cur)
            except OSError:
                pass
        raise RuntimeError("Could not replace the old program files (%s). Close every window and program that uses the old Legend "
                           "of Kairi folder (Explorer windows inside it, the game, an editor) and press Update again." % e)
    shutil.rmtree(stage, ignore_errors=True)
    j["phase"] = "swapped"
    write_journal(root, j)
    log("[update] program files replaced: %s" % ", ".join(items))
    return j


def owners(steps_list_text):
    """sorabuild --list text -> {data file or dir: step}"""
    out = {}
    for ln in steps_list_text.splitlines():
        m = re.match(r"(\S+)\s+(\S+)\s+(done|-)\s.*\bout=(.*)$", ln.rstrip())
        if m and m.group(4).strip() != "-":
            for o in m.group(4).split():
                out[o] = m.group(1)
    return out


def changed_files(old_man, new_man):
    """data files whose reference differs (new, changed or gone) between two manifest.thispc.json dicts"""
    a, b = (old_man or {}).get("files", {}), (new_man or {}).get("files", {})
    return sorted(f for f in set(a) | set(b) if a.get(f) != b.get(f))


def invalidate(root, steps_list_text, log=print):
    """step 2. -> list of the steps whose marker was removed (or ["*"] = all markers, when the old manifest is unknown)."""
    root = os.path.abspath(root)
    j = read_journal(root)
    data, done = os.path.join(root, "data"), os.path.join(root, "work", "done")
    old_man = _read_json(os.path.join(root, UPD, "old_data_manifest.json"))
    new_man = _read_json(os.path.join(root, "tools", "sorabuild", "manifest.thispc.json"))
    if not new_man:
        raise RuntimeError("The new tools have no data manifest. Download Legend of Kairi again.")
    if "invalidated" in j and j.get("invalidated_for") == new_man.get("build_id"):
        return j["invalidated"]       # done by an earlier attempt of this same update: the build carries on from its markers
    own = owners(steps_list_text)
    hit = set()
    files = changed_files(old_man, new_man) if old_man else None
    unowned = []
    for f in files or ():
        st = own.get(f) or next((s for o, s in own.items() if f.startswith(o.rstrip("/") + "/")), None)
        if st:
            hit.add(st)
        elif f in new_man.get("files", {}):
            unowned.append(f)
    if files is None or unowned:
        # unknown old data (no manifest) or a file no step claims: rebuild every step (the KH2 extract stays)
        hit = {"*"}
        log("[update] %s: every step is rebuilt" % ("the old data manifest is missing" if files is None else
                                                  "no step owns %s" % ", ".join(unowned[:3])))
        if os.path.isdir(done):
            for n in os.listdir(done):
                if not n.startswith("_"):
                    _rm(os.path.join(done, n))
    else:
        for f in files:
            p = os.path.normpath(os.path.join(data, f))
            if os.path.commonpath([p, os.path.normpath(data)]) == os.path.normpath(data) and os.path.isfile(p):
                os.remove(p)
        for s in hit:
            m = os.path.join(done, s)
            if os.path.exists(m):
                os.remove(m)
        log("[update] %d data file(s) changed since %s: re-running %s" % (len(files), j.get("from_version", "?"),
                                                                       ", ".join(sorted(hit)) or "no build step"))
    j["phase"], j["invalidated"], j["invalidated_for"] = "invalidated", sorted(hit), new_man.get("build_id")
    write_journal(root, j)
    return sorted(hit)


def finish(root, log=print):
    """step 4: the update is complete (verify passed, launchers written): drop the journal + the old copies"""
    u = os.path.join(os.path.abspath(root), UPD)
    shutil.rmtree(u, ignore_errors=True)
    if os.path.exists(u):
        log("[update] could not delete %s (delete it by hand; it only holds the old program files)" % u)


# ------------------------------------------------------------------------------------------------ is the game of that folder running?
def running_from(root):
    """the paths of running programs whose exe sits inside root (savagewind.exe in bin/, the kit's python...). Windows only, else []."""
    if sys.platform != "win32":
        return []
    import ctypes
    from ctypes import wintypes
    psapi, k32 = ctypes.windll.psapi, ctypes.windll.kernel32
    k32.OpenProcess.restype = wintypes.HANDLE
    arr = (wintypes.DWORD * 8192)()
    got = wintypes.DWORD()
    if not psapi.EnumProcesses(arr, ctypes.sizeof(arr), ctypes.byref(got)):
        return []
    base = os.path.normcase(os.path.realpath(root)) + os.sep
    me = os.getpid()
    out = []
    for pid in arr[: got.value // ctypes.sizeof(wintypes.DWORD)]:
        if not pid or pid == me:
            continue
        h = k32.OpenProcess(0x1000, False, pid)         # PROCESS_QUERY_LIMITED_INFORMATION
        if not h:
            continue
        try:
            buf = ctypes.create_unicode_buffer(1024)
            n = wintypes.DWORD(1024)
            if k32.QueryFullProcessImageNameW(wintypes.HANDLE(h), 0, buf, ctypes.byref(n)):
                p = os.path.normcase(os.path.realpath(buf.value))
                if p.startswith(base):
                    out.append(buf.value)
        finally:
            k32.CloseHandle(wintypes.HANDLE(h))
    return out


def locked_files(root):
    """files of root/bin that cannot be opened for writing (a running game holds them), when the process list cannot tell"""
    out = []
    for n in ("savagewind.exe", "sora_mod.dll"):
        p = os.path.join(root, "bin", n)
        if os.path.isfile(p):
            try:
                with open(p, "r+b"):
                    pass
            except PermissionError:
                out.append(p)
            except OSError:
                pass
    return out


def busy_message(root):
    """'' when nothing of root is running, else a plain message"""
    r = running_from(root)
    if any(os.path.basename(p).lower() == "savagewind.exe" for p in r) or locked_files(root):
        return "Legend of Kairi from that folder is running. Close the game window first, then press Update again."
    if r:
        return ("A program from that folder is running (%s). Close it (an installer window of the old version?), then press Update "
                "again." % os.path.basename(r[0]))
    return ""

"""Legend of Kairi installer, last step (s177): the game side. Runs after `verify --expect` passed.

  install_game_side(root, sw_dir, log)
    1. bin/ (savagewind.exe + sora_mod.dll + the runtime DLLs of the release) is checked against manifest.json (size + sha256)
       and copied into <root>/bin (when the release folder IS the install root, that is the check only).
    2. two launchers are written into <root> (CRLF .bat):
         Play Legend of Kairi.bat               solo, net off
         Play Legend of Kairi Multiplayer.bat   net on; the friend's Tailscale IP goes on a marked line (kept when this step runs again)
    Re-running overwrites bin/ and the launchers; saves (the Savage Wind folder's save\\), data/ and logs/ are never touched.

The mod finds its files through SORA_ROOT (runtime/sora_path.h maps every "D:/SoraBotW/..." default to <SORA_ROOT>/...), the mod DLL
sits next to the exe (runtime/sora_host.c: <exe dir>/sora_mod.dll). The game runs with the Savage Wind folder as its working
directory (its savagewind.ini, extracted\\ game files and save\\), so the player's saves are the ones Savage Wind already uses.
"""
import hashlib
import json
import os
import re
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
RELEASE = os.path.normpath(os.path.join(HERE, "..", ".."))      # the unpacked release folder (bin/, manifest.json)
SOLO = "Play Legend of Kairi.bat"
MULTI = "Play Legend of Kairi Multiplayer.bat"
PLACEHOLDER = "FILL-IN-FRIENDS-TAILSCALE-IP:7790"
NEEDED = ("savagewind.exe", "sora_mod.dll")


def _sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest()


def _bat_path(p):
    """a path for a .bat: backslashes, % doubled"""
    return os.path.normpath(p).replace("%", "%%")


def launcher_text(multi, sw_dir, peers=PLACEHOLDER):
    """the .bat text (LF here; written as CRLF). The install root is the .bat's own folder (%~dp0)."""
    sw = _bat_path(sw_dir)
    title = "Multiplayer" if multi else "Solo"
    L = ["@echo off"]
    if not sw.isascii():
        L += ["chcp 65001 >nul"]
    L += ["rem Legend of Kairi - %s. Written by the installer; running the installer again rewrites this file." % title,
          "rem (it keeps the friend's address below). Saves are the ones Savage Wind already uses (its save folder)."]
    if multi:
        L += ["",
              "rem ===== FILL THIS IN ONCE: your friend's Tailscale IP, then :7790 (more friends: separate them with commas) =====",
              "rem ===== example:  set \"PEERS=100.101.102.103:7790\"   (Tailscale shows the IP next to your friend's name) =====",
              'set "PEERS=%s"' % peers,
              "rem ===== nothing else in this file needs changing =====",
              "",
              "rem Everybody has to run the very same Legend of Kairi release. Windows may ask once to allow savagewind.exe",
              "rem through the firewall: say yes (UDP port 7790 must be open between the players)."]
    L += ["setlocal",
          'set "SORA_ROOT=%~dp0"',
          'set "GAME=' + sw + '"',
          'set "EXE=%SORA_ROOT%bin\\savagewind.exe"',
          'set "LOGS=%SORA_ROOT%logs"',
          ""]
    if multi:
        L += ['if /i "%PEERS:~0,7%"=="FILL-IN" (',
              "    echo This is the Multiplayer launcher, and it still needs your friend's address.",
              "    echo Right-click this file, choose Edit, and replace FILL-IN-FRIENDS-TAILSCALE-IP:7790 on the line",
              "    echo that starts with set PEERS with your friend's Tailscale IP and :7790, for example 100.101.102.103:7790.",
              "    echo Save it and start it again. To play alone use Play Legend of Kairi.bat.",
              "    pause",
              "    exit /b 1",
              ")"]
    # "running" = ANY savagewind.exe process (the friend runs ONE game). No pipe and no find/findstr from PATH: tasklist is called by
    # its full System32 path and its CSV output is read by for /f, so a PATH with GNU find (bash-started cmd) cannot break it.
    L += ['set "SYS=%SystemRoot%\\System32"',
          'if not exist "%SYS%\\tasklist.exe" set "SYS=C:\\Windows\\System32"',
          'for /f "tokens=1 delims=," %%a in (\'""%SYS%\\tasklist.exe" /fi "imagename eq savagewind.exe" /nh /fo csv"\') do if /i "%%~a"=="savagewind.exe" goto :running',
          'if not exist "%EXE%" goto :noexe',
          'if not exist "%GAME%\\savagewind.ini" if not exist "%GAME%\\extracted" goto :nogame',
          "",
          'if not exist "%LOGS%" mkdir "%LOGS%"',
          'if exist "%LOGS%\\last.log" move /y "%LOGS%\\last.log" "%LOGS%\\prev.log" >nul',
          "",
          "set SAVAGEWIND_SORA=1",
          "set SAVAGEWIND_SORA_RELOAD=0",
          'set "SAVAGEWIND_SORA_LIVE="',
          "rem the mod's own files (built by the installer) are under %SORA_ROOT%data"]
    if multi:
        L += ["rem multiplayer: UDP port 7790; shared enemy HP, enemies act the same, enemy attacks synced",
              "set SAVAGEWIND_SORA_NET=1",
              'set "SAVAGEWIND_SORA_NET_NAME=%USERNAME%"',
              'set "SAVAGEWIND_SORA_NET_PEERS=%PEERS%"',
              "set SAVAGEWIND_LINK_EDMG=1",
              "set SAVAGEWIND_LINK_ESYNC=1",
              "rem enemy attacks synced: 3 = the copy's attack runs in the game's AI phase (s182: 0.05 ms per step, 0 = off)",
              "set SAVAGEWIND_LINK_ESYNC_ATK=3"]
    else:
        L += ["rem solo: multiplayer off",
              "set SAVAGEWIND_SORA_NET=0",
              'set "SAVAGEWIND_SORA_NET_PEERS="',
              'set "SAVAGEWIND_LINK_EDMG="',
              'set "SAVAGEWIND_LINK_ESYNC="',
              'set "SAVAGEWIND_LINK_ESYNC_ATK="']
    L += ["rem title logos, when this release has them (remove = BotW's own logos)",
          'if exist "%SORA_ROOT%data\\logo\\Logo3DGameTitle_00.bin" set "SAVAGEWIND_LOGO_TEX=%SORA_ROOT%data\\logo\\Logo3DGameTitle_00.bin"',
          'if exist "%SORA_ROOT%data\\logo\\Logo2D_00.bin" set "SAVAGEWIND_LOGO2D_TEX=%SORA_ROOT%data\\logo\\Logo2D_00.bin"',
          'if exist "%SORA_ROOT%data\\overlay\\Pack\\Title.pack" set "SAVAGEWIND_ROMFS_OVERLAY=%SORA_ROOT%data\\overlay"',
          "",
          'cd /d "%GAME%" || (pause & exit /b 1)',
          "rem the child cmd reads EXE and LOGS from its environment: the path never sits on the start line, so & ^ ( ) in it cannot break it",
          'start "Legend of Kairi - %s" /min cmd /c ""%%%%EXE%%%%" extracted > "%%%%LOGS%%%%\\last.log" 2>&1"' % title]
    L += ["exit /b 0",
          "",
          ":running",
          "echo Legend of Kairi (Savage Wind) is already running. Close that game window first, then start this again.",
          "pause",
          "exit /b 1",
          ":noexe",
          'echo The game program is missing: "%EXE%"',
          "echo Run Install Legend of Kairi again.",
          "pause",
          "exit /b 1",
          ":nogame",
          'echo The Savage Wind folder was not found: "%GAME%"',
          "echo Run Install Legend of Kairi again and pick the Savage Wind folder.",
          "pause",
          "exit /b 1"]
    return "\r\n".join(L) + "\r\n"


def _write_bat(path, text):
    enc = "utf-8" if not text.isascii() else "ascii"
    tmp = path + ".tmp"       # (s178 S3) never leave a half-written launcher behind
    with open(tmp, "wb") as f:
        f.write(text.encode(enc))
    os.replace(tmp, path)


def _kept_peers(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            t = f.read()
    except OSError:
        return PLACEHOLDER
    # (s178 S3) tolerant: set PEERS=a:7790 / set "PEERS=a:7790" / extra spaces / a trailing "& rem ..." (rem lines never match)
    m = re.search(r'^[ \t]*set[ \t]+"?[ \t]*PEERS[ \t]*=([^"\r\n&]*)', t, re.M | re.I)
    v = m.group(1).strip() if m else ""
    return v if v else PLACEHOLDER


def install_game_side(root, sw_dir, log=print, release=RELEASE):
    """-> {"solo": path, "multi": path, "bin": path}. Raises RuntimeError with a plain-words message."""
    root = os.path.abspath(root)
    src, dst = os.path.join(release, "bin"), os.path.join(root, "bin")
    man = os.path.join(release, "manifest.json")
    want = None
    if os.path.isfile(man):
        with open(man, encoding="utf-8") as f:
            want = json.load(f).get("bin")
    if not os.path.isdir(src):
        raise RuntimeError("The game program folder (bin) is missing from this download. Download Legend of Kairi again.")
    names = sorted(want) if want else sorted(n for n in os.listdir(src) if os.path.isfile(os.path.join(src, n)))
    for n in NEEDED:
        if n not in names or not os.path.isfile(os.path.join(src, n)):
            raise RuntimeError("%s is missing from this download. Download Legend of Kairi again." % n)
    for n in names:
        p = os.path.join(src, n)
        if not os.path.isfile(p):
            raise RuntimeError("%s is missing from this download. Download Legend of Kairi again." % n)
        if want and (os.path.getsize(p) != want[n]["size"] or _sha256(p) != want[n]["sha256"]):
            raise RuntimeError("%s is damaged (it does not match the release). Download Legend of Kairi again." % n)
    log("[game] %d program files checked" % len(names))
    if os.path.normcase(os.path.realpath(src)) != os.path.normcase(os.path.realpath(dst)):
        os.makedirs(dst, exist_ok=True)
        for n in names:
            tmp = os.path.join(dst, n + ".new")
            try:
                shutil.copy2(os.path.join(src, n), tmp)
                os.replace(tmp, os.path.join(dst, n))
            except PermissionError:
                raise RuntimeError("Could not replace %s: the game is still running. Close Savage Wind and run the installer again." % n)
        log("[game] copied to %s" % dst)
    else:
        log("[game] program files are already in %s" % dst)
    if not os.path.isdir(sw_dir):
        raise RuntimeError("The Savage Wind folder %s is gone. Choose it again." % sw_dir)
    solo, multi = os.path.join(root, SOLO), os.path.join(root, MULTI)
    peers = _kept_peers(multi)
    _write_bat(solo, launcher_text(False, sw_dir))
    _write_bat(multi, launcher_text(True, sw_dir, peers))
    log("[game] launchers: %s, %s" % (SOLO, MULTI))
    return {"solo": solo, "multi": multi, "bin": dst, "peers_set": peers != PLACEHOLDER}

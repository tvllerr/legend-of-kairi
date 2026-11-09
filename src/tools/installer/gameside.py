"""Legend of Kairi installer, last step (s177): the game side. Runs after `verify --expect` passed.

  install_game_side(root, sw_dir, log)
    1. bin/ (savagewind.exe + sora_mod.dll + the runtime DLLs of the release) is checked against manifest.json (size + sha256)
       and copied into <root>/bin (when the release folder IS the install root, that is the check only).
    2. two launchers are written into <root> (CRLF .bat):
         Play Legend of Kairi.bat               solo, net off
         Play Legend of Kairi Multiplayer.bat   net on; the friend's Tailscale IP goes on a marked line (kept when this step runs again)
       and (s183) a third one for bug reports:
         Make bug report.bat                    zips the logs + manifest + launchers + savagewind.ini + a systeminfo.txt (Windows, GPU,
                                                driver, RAM) into "Legend of Kairi bug report <date-time>.zip" on the Desktop (no saves, no game files)
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
BUGREPORT = "Make bug report.bat"
BUGREPORT_PS1 = "make_bug_report.ps1"   # (revfix) the PowerShell half: a plain script next to the .bat (no -Command / Invoke-Expression: antivirus flags that)
PLACEHOLDER = "FILL-IN-FRIENDS-TAILSCALE-IP:7790"
NEEDED = ("savagewind.exe", "sora_mod.dll")
# (s183) the body files the launcher insists on (under data/): without any of them the HUD can work while the KH2 body is missing
KEY_FILES = ("sora_model.bin", "sora_pose.bin", "sora_motion.bin", "forms/roxas_model.bin", "ragmap/sora.bin", "hud/camp_seq.bin")
BUILD_ID_FILE = "BUILD_ID"


def _sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest()


def _bat_path(p):
    """a path for a .bat: backslashes, % doubled"""
    return os.path.normpath(p).replace("/", "\\").replace("%", "%%")   # (a MSYS2 python's normpath keeps "/")


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
          "rem (s183) pre-checks: the body files must be there, and data\\BUILD_ID (written by the installer once verify passed) must match",
          "rem manifest.json. A manifest that cannot be read skips the second check (never blocks on a parse problem)."]
    for k in KEY_FILES:
        k = k.replace("/", "\\")
        L += ['if not exist "%%SORA_ROOT%%data\\%s" set "MISSING=%s" & goto :incomplete' % (k, k)]
    L += ['set "WANT="',
          'set "HAVE="',
          'if exist "%SORA_ROOT%manifest.json" for /f "usebackq tokens=1,2 delims=:, " %%a in ("%SORA_ROOT%manifest.json") do if "%%~a"=="build_id" if not defined WANT set "WANT=%%~b"',
          "if not defined WANT goto :idok",
          'if not exist "%SORA_ROOT%data\\BUILD_ID" goto :otherversion',
          'set /p HAVE=<"%SORA_ROOT%data\\BUILD_ID"',
          'if /i not "%HAVE%"=="%WANT%" goto :otherversion',
          ":idok",
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
          ":incomplete",
          "echo Legend of Kairi's files are incomplete (%MISSING% is missing).",
          "echo Run Install Legend of Kairi.cmd again (choose Update if you had an older version).",
          "pause",
          "exit /b 1",
          ":otherversion",
          "echo Legend of Kairi's files are from another version.",
          "echo Run Install Legend of Kairi.cmd and choose Update.",
          "pause",
          "exit /b 1",
          ":nogame",
          'echo The Savage Wind folder was not found: "%GAME%"',
          "echo Run Install Legend of Kairi again and pick the Savage Wind folder.",
          "pause",
          "exit /b 1"]
    return "\r\n".join(L) + "\r\n"


# The PowerShell half of "Make bug report.bat": make_bug_report.ps1 next to it, run with -File (no path ever sits on a command line:
# SORA_ROOT / GAME travel in the environment, where & ^ % ! ( ) are plain).
BUGREPORT_PS = r'''$ErrorActionPreference = 'Continue'
# the .bat's %~dp0 may be an 8.3 short path (C:\PROGRA~1\...): resolve every part to its long name, so names cut from it are right
function Get-LongPath([string]$p) {
    $p = [IO.Path]::GetFullPath($p).TrimEnd('\')
    $r = [IO.Path]::GetPathRoot($p).TrimEnd('\')
    $cur = $r
    foreach ($c in $p.Substring($r.Length).Split('\')) {
        if (-not $c) { continue }
        $next = $null
        try { $m = @([IO.Directory]::GetFileSystemEntries($cur + '\', $c)); if ($m.Count -ge 1) { $next = $m[0] } } catch { }
        if ($next) { $cur = $next } else { $cur = $cur + '\' + $c }
    }
    return $cur
}
$root = Get-LongPath $env:SORA_ROOT
$game = $env:GAME
$stamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$desk = [Environment]::GetFolderPath('Desktop')
if (-not $desk -or -not (Test-Path -LiteralPath $desk)) { $desk = Join-Path $env:USERPROFILE 'Desktop' }
if (-not (Test-Path -LiteralPath $desk)) { $desk = $root }
if ($env:KAIRI_BUGREPORT_DIR -and (Test-Path -LiteralPath $env:KAIRI_BUGREPORT_DIR)) { $desk = $env:KAIRI_BUGREPORT_DIR }   # (tests)
$zip = Join-Path $desk ('Legend of Kairi bug report ' + $stamp + '.zip')
$stage = Join-Path ([IO.Path]::GetTempPath()) ('kairi_bugreport_' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage -Force | Out-Null
$notes = New-Object System.Collections.ArrayList

# copy a file even while the game still has it open for writing; a file over 40 MB keeps only its last 40 MB
function Copy-Shared([string]$src, [string]$dst) {
    try {
        $share = [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete
        $in = New-Object IO.FileStream($src, [IO.FileMode]::Open, [IO.FileAccess]::Read, $share)
        try {
            $max = 40MB
            if ($in.Length -gt $max) { $in.Seek(-$max, [IO.SeekOrigin]::End) | Out-Null }
            $out = [IO.File]::Create($dst)
            try { $in.CopyTo($out) } finally { $out.Dispose() }
        } finally { $in.Dispose() }
        return $true
    } catch { [void]$notes.Add('could not copy ' + $src + ': ' + $_.Exception.Message); return $false }
}
function Put-Files([string]$dir, [string]$filter, [string]$sub) {
    if (-not (Test-Path -LiteralPath $dir)) { [void]$notes.Add('missing folder: ' + $dir); return }
    $items = @(Get-ChildItem -LiteralPath $dir -Filter $filter -File -ErrorAction SilentlyContinue)
    if ($items.Count -eq 0) { [void]$notes.Add('no ' + $filter + ' in ' + $dir); return }
    $d = Join-Path $stage $sub
    New-Item -ItemType Directory -Path $d -Force | Out-Null
    foreach ($f in $items) { [void](Copy-Shared $f.FullName (Join-Path $d $f.Name)) }
}

Put-Files (Join-Path $root 'logs') '*.log' 'logs'
Put-Files (Join-Path $root 'work\logs') '*.log' 'work_logs'
Put-Files $root 'manifest.json' 'install_folder'
Put-Files $root '*.bat' 'launchers'
Put-Files $root '*.ps1' 'launchers'
# the zip copy of the multiplayer launcher does not carry the friends' IP list (it is a personal address)
try {
    $ld = Join-Path $stage 'launchers'
    if (Test-Path -LiteralPath $ld) {
        foreach ($f in @(Get-ChildItem -LiteralPath $ld -Filter '*.bat' -File)) {
            $t = [IO.File]::ReadAllText($f.FullName)
            $t2 = [regex]::Replace($t, '(?im)^([ \t]*set[ \t]+"?[ \t]*PEERS[ \t]*=)[^"\r\n&]*', '${1}<removed>')
            if ($t2 -ne $t) { [IO.File]::WriteAllText($f.FullName, $t2, (New-Object Text.UTF8Encoding($false))) }
        }
    }
} catch { [void]$notes.Add('could not blank the PEERS value: ' + $_.Exception.Message) }
if ($game) { Put-Files $game 'savagewind.ini' 'savagewind_folder' }

# what is installed, by name and size only (no file content): shows a missing or empty data file at a glance
try {
    $dd = Join-Path $root 'data'
    if (Test-Path -LiteralPath $dd) {
        Get-ChildItem -LiteralPath $dd -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 3000 |
            ForEach-Object { $_.FullName.Substring($root.Length + 1) + '  ' + $_.Length } |
            Set-Content -LiteralPath (Join-Path $stage 'data_listing.txt') -Encoding UTF8
    } else { [void]$notes.Add('no data folder in ' + $root) }
} catch { [void]$notes.Add('data listing failed: ' + $_.Exception.Message) }

# systeminfo.txt
$si = New-Object System.Collections.ArrayList
function Add-Line([string]$t) { [void]$si.Add($t) }
Add-Line ('Legend of Kairi bug report ' + $stamp)
Add-Line ('install folder: ' + $root)
Add-Line ('Savage Wind folder: ' + $game)
try {
    $os = Get-CimInstance Win32_OperatingSystem
    Add-Line ('Windows: ' + $os.Caption + ' ' + $os.Version + ' (build ' + $os.BuildNumber + ', ' + $os.OSArchitecture + ')')
} catch { Add-Line ('Windows: ' + [Environment]::OSVersion.VersionString + ' (details failed: ' + $_.Exception.Message + ')') }
try {
    foreach ($c in @(Get-CimInstance Win32_Processor)) { Add-Line ('CPU: ' + $c.Name.Trim() + ', ' + $c.NumberOfCores + ' cores') }
} catch { Add-Line 'CPU: unknown' }
try {
    $cs = Get-CimInstance Win32_ComputerSystem
    Add-Line ('RAM: ' + [math]::Round($cs.TotalPhysicalMemory / 1GB, 1) + ' GB')
} catch { Add-Line 'RAM: unknown' }
$gpuOk = $false
try {
    foreach ($g in @(Get-CimInstance Win32_VideoController)) {
        $gpuOk = $true
        Add-Line ('GPU: ' + $g.Name + ' | driver ' + $g.DriverVersion + ' (' + $g.DriverDate + ') | status ' + $g.Status)
    }
} catch { Add-Line ('GPU (CIM) failed: ' + $_.Exception.Message) }
if (-not $gpuOk) {
    try {
        $w = & (Join-Path $env:SystemRoot 'System32\wbem\wmic.exe') path win32_VideoController get Name,DriverVersion /format:list 2>$null
        foreach ($l in $w) { if ($l -and $l.Trim()) { Add-Line ('GPU (wmic): ' + $l.Trim()) } }
    } catch { Add-Line 'GPU: unknown' }
}
try {
    $drv = (Get-Item -LiteralPath $root).PSDrive
    if ($drv) { Add-Line ('free disk space on ' + $drv.Name + ': ' + [math]::Round($drv.Free / 1GB, 1) + ' GB') }
} catch { }
foreach ($n in $notes) { Add-Line ('note: ' + $n) }
$si | Set-Content -LiteralPath (Join-Path $stage 'systeminfo.txt') -Encoding UTF8

$code = 0
try {
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force -ErrorAction Stop
    Write-Host ''
    Write-Host 'Done. The bug report is here:'
    Write-Host ('  ' + $zip)
    Write-Host 'Send that one file to Tyler (tvllerr) and say what you saw. It holds no save files and no game files.'
} catch {
    Write-Host ''
    Write-Host ('Could not make the zip: ' + $_.Exception.Message)
    Write-Host ('The collected files are in: ' + $stage)
    $code = 1
    $stage = $null
}
if ($stage) { Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue }
exit $code
'''


def bugreport_text(sw_dir):
    """"Make bug report.bat" (LF here; written as CRLF): the cmd half sets SORA_ROOT / GAME and runs make_bug_report.ps1 (next to it,
    bugreport_ps_text()) with -File. Nothing is ever put on a command line but the script's own path."""
    sw = _bat_path(sw_dir)
    L = ["@echo off"]
    if not sw.isascii():
        L += ["chcp 65001 >nul"]
    L += ["rem Legend of Kairi - Make bug report. Written by the installer; running the installer again rewrites this file.",
          "rem Double-click it after something went wrong: it puts ONE zip on your Desktop (game logs, install logs, the launchers,",
          "rem savagewind.ini and a systeminfo.txt with Windows / graphics card / driver / RAM). No saves, no game files.",
          "setlocal",
          'set "SORA_ROOT=%~dp0"',
          'set "GAME=' + sw + '"',
          r'set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"',
          'if not exist "%PS%" set "PS=powershell.exe"',
          'if not exist "%~dp0' + BUGREPORT_PS1 + '" (',
          "    echo " + BUGREPORT_PS1 + " is missing next to this file. Run the Legend of Kairi installer again.",
          "    pause",
          "    exit /b 1",
          ")",
          "echo Making the Legend of Kairi bug report, one moment...",
          '"%PS%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0' + BUGREPORT_PS1 + '"',
          "echo.",
          "pause",
          "exit /b 0"]
    return "\r\n".join(L) + "\r\n"


def bugreport_ps_text():
    """make_bug_report.ps1 (ASCII, CRLF)"""
    return "\r\n".join(BUGREPORT_PS.rstrip("\n").split("\n")) + "\r\n"


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


def clear_build_id(root):
    """(s183) an install / update is starting: the data is not known good any more, so the launchers must refuse until it finished"""
    try:
        os.remove(os.path.join(root, "data", BUILD_ID_FILE))
    except OSError:
        pass


def write_build_id(root, release, log=print):
    """(s183) verify passed: data/BUILD_ID = the manifest's data.build_id (what the launchers compare with <root>/manifest.json)"""
    bid = ""
    for m in (os.path.join(root, "manifest.json"), os.path.join(release, "manifest.json")):
        try:
            with open(m, encoding="utf-8") as f:
                bid = str(json.load(f).get("data", {}).get("build_id") or "")
        except (OSError, ValueError, AttributeError):
            bid = ""
        if bid:
            break
    if not re.fullmatch(r"[0-9A-Za-z_.-]+", bid):
        log("[game] no usable data build id in manifest.json: the launchers skip the version check")
        return ""
    os.makedirs(os.path.join(root, "data"), exist_ok=True)
    with open(os.path.join(root, "data", BUILD_ID_FILE), "w", newline="") as f:
        f.write(bid + "\r\n")
    log("[game] data build id %s" % bid)
    return bid


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
    bug = os.path.join(root, BUGREPORT)
    _write_bat(bug, bugreport_text(sw_dir))
    _write_bat(os.path.join(root, BUGREPORT_PS1), bugreport_ps_text())
    log("[game] launchers: %s, %s, %s (+ %s)" % (SOLO, MULTI, BUGREPORT, BUGREPORT_PS1))
    write_build_id(root, release, log)
    return {"solo": solo, "multi": multi, "bugreport": bug, "bin": dst, "peers_set": peers != PLACEHOLDER}

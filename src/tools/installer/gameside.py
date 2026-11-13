"""Legend of Kairi installer, last step (s177): the game side. Runs after `verify --expect` passed.

  install_game_side(root, sw_dir, log)
    1. bin/ (savagewind.exe + sora_mod.dll + the runtime DLLs of the release) is checked against manifest.json (size + sha256;
       (s201) a file with an Authenticode signature added, e.g. by Melty, matches when its unsigned bytes do: sorabuild/pesig.py)
       and copied into <root>/bin (when the release folder IS the install root, that is the check only).
    2. two launchers are written into <root> (CRLF .bat):
         Play Legend of Kairi.bat               solo, net off
         Play Legend of Kairi Multiplayer.bat   net on; the friend's Tailscale IP goes on a marked line (kept when this step runs again)
       and (s183) a third one for bug reports:
         Make bug report.bat                    zips the logs + manifest + launchers + savagewind.ini + a systeminfo.txt (Windows, GPU,
                                                driver, RAM) into "Legend of Kairi bug report <date-time>.zip" on the Desktop (no saves, no game files)
       and (s185) two shortcuts with the Legend of Kairi icon that start the first two: "Legend of Kairi.lnk", "Legend of Kairi
       Multiplayer.lnk" (shortcut.ps1; a failure is logged and never stops the install)
    Re-running overwrites bin/ and the launchers; saves (the Savage Wind folder's save\\), data/ and logs/ are never touched.

The mod finds its files through SORA_ROOT (runtime/sora_path.h maps every "D:/SoraBotW/..." default to <SORA_ROOT>/...), the mod DLL
sits next to the exe (runtime/sora_host.c: <exe dir>/sora_mod.dll). The game runs with the Savage Wind folder as its working
directory (its savagewind.ini, extracted\\ game files and save\\), so the player's saves are the ones Savage Wind already uses.
"""
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "sorabuild"))
import pesig  # noqa: E402  (s201: bin/ files with an Authenticode signature added still match the manifest)
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
GAME_ID_FILE = "GAME_ID"    # (s223) data/GAME_ID: the Savage Wind game code id this release's savagewind.exe carries (the launcher compares)
SW_WANT = "0.5.3-alpha.5"   # (s223) the Savage Wind version of this release (swsetup.SW_WANT; test_swinstall checks they agree)
REMOVED_DIR = ("work", "removed")   # (s223) a 0.1.4 Multiplayer launcher is kept here as .txt (its friend's address), never run
CHCP = r'"%SystemRoot%\System32\chcp.com" 65001 >nul'    # (s185 sec) full path: never a chcp from the current directory


def _bat_path(p):
    """a path for a .bat: backslashes, % doubled"""
    return os.path.normpath(p).replace("/", "\\").replace("%", "%%")   # (a MSYS2 python's normpath keeps "/")


def launcher_text(multi, sw_dir, peers=PLACEHOLDER):
    """the .bat text (LF here; written as CRLF). The install root is the .bat's own folder (%~dp0)."""
    sw = _bat_path(sw_dir)
    title = "Multiplayer" if multi else "Solo"
    L = ["@echo off"]
    if not sw.isascii():
        L += [CHCP]
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
          "rem (s223) Savage Wind must still be %s (its own updater may have moved it on) and its game code the one this" % SW_WANT,
          "rem release's program was made for (data\\GAME_ID, written by the installer; no file = no check)",
          'set "SWV="',
          'if exist "%GAME%\\sw-manifest.txt" for /f "usebackq tokens=1,2" %%a in ("%GAME%\\sw-manifest.txt") do if "%%a"=="version" if not defined SWV set "SWV=%%b"',
          'if not "%%SWV%%"=="%s" goto :swversion' % SW_WANT,
          'if not exist "%SORA_ROOT%data\\GAME_ID" goto :gameidok',
          'set "GID="',
          'set "HAVEGID="',
          'set /p GID=<"%SORA_ROOT%data\\GAME_ID"',
          'if exist "%GAME%\\extracted\\swgame.dll.id" set /p HAVEGID=<"%GAME%\\extracted\\swgame.dll.id"',
          'if /i not "%GID%"=="%HAVEGID%" goto :gameid',
          ":gameidok",
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
        L += ["rem solo: multiplayer off (s223: this release plays on your own; SORA_SOLO also keeps the mod's co-op code off)",
              "set SAVAGEWIND_SORA_SOLO=1",
              "set SAVAGEWIND_SORA_NET=0",
              'set "SAVAGEWIND_SORA_NET_PEERS="',
              "set SAVAGEWIND_LINK_EDMG=0",
              "set SAVAGEWIND_LINK_ESYNC=0",
              "set SAVAGEWIND_LINK_ESYNC_ATK=0"]
    L += ["rem (s223) Savage Wind's own update check off (it would offer the stock program over the mod's), and 0.5.3's",
          "rem vertex-copy feature off (s219: about 14 fps less after a death / load with the mod)",
          "set SAVAGEWIND_NO_UPDATE_CHECK=1",
          "set SAVAGEWIND_VK_VDYN=0"]
    L += ["rem title logos, when this release has them (remove = BotW's own logos)",
          'if exist "%SORA_ROOT%data\\logo\\Logo3DGameTitle_00.bin" set "SAVAGEWIND_LOGO_TEX=%SORA_ROOT%data\\logo\\Logo3DGameTitle_00.bin"',
          'if exist "%SORA_ROOT%data\\logo\\Logo2D_00.bin" set "SAVAGEWIND_LOGO2D_TEX=%SORA_ROOT%data\\logo\\Logo2D_00.bin"',
          'if exist "%SORA_ROOT%data\\overlay\\Pack\\Title.pack" set "SAVAGEWIND_ROMFS_OVERLAY=%SORA_ROOT%data\\overlay"',
          "",
          'cd /d "%GAME%" || (pause & exit /b 1)',
          "rem the child cmd reads EXE and LOGS from its environment: the path never sits on the start line, so & ^ ( ) in it cannot break it",
          'start "Legend of Kairi - %s" /min "%%SystemRoot%%\\System32\\cmd.exe" /c ""%%%%EXE%%%%" extracted --no-launch-flags > "%%%%LOGS%%%%\\last.log" 2>&1"' % title]
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
          ":swversion",
          "if not defined SWV set \"SWV=an unknown version\"",
          "echo Your Savage Wind folder has Savage Wind %SWV%, but this Legend of Kairi is made for Savage Wind " + SW_WANT + ".",
          "echo Did Savage Wind update itself? Wait for a Legend of Kairi release made for that version, or unpack",
          "echo Savage Wind " + SW_WANT + " into that folder again and run Install Legend of Kairi.cmd. Folder: \"%GAME%\"",
          "pause",
          "exit /b 1",
          ":gameid",
          "echo Savage Wind's game code in your Savage Wind folder is not the one this Legend of Kairi was made for.",
          "echo (Savage Wind set it up again, or it is another version's.) Run Install Legend of Kairi.cmd again: it fixes it.",
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

# (s185 sec) privacy scrub: every file in the report folder is read as bytes (Latin-1 keeps the bytes as they are, so UTF-8 text round-trips),
# friends' IPs, Windows user names and this PC's name are replaced, and a file that cannot be scrubbed is LEFT OUT of the zip (never shipped as it is).
$scrubExt = @('.log', '.txt', '.bat', '.cmd', '.ps1', '.json', '.ini', '.md', '.csv', '.xml', '.cfg', '.conf', '.yaml', '.yml')
$L1 = [Text.Encoding]::GetEncoding(28591)
$rxOpt = [Text.RegularExpressions.RegexOptions]::IgnoreCase
$rxIp4 = New-Object Text.RegularExpressions.Regex('(?<!\d)(?<!\d\.)(?:(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)\.){3}(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(?!\d)(?!\.\d)')
$rxIp6a = New-Object Text.RegularExpressions.Regex('(?<![0-9A-Fa-f:])(?:[0-9A-Fa-f]{1,4}:){7}[0-9A-Fa-f]{1,4}(?![0-9A-Fa-f:])')
$rxIp6b = New-Object Text.RegularExpressions.Regex('(?<![0-9A-Fa-f:])(?:[0-9A-Fa-f]{1,4}:){2,7}:(?:[0-9A-Fa-f]{1,4}(?::[0-9A-Fa-f]{1,4}){0,5})?(?![0-9A-Fa-f:])')
$rxIp6c = New-Object Text.RegularExpressions.Regex('(?<![0-9A-Fa-f:])fe80::(?:[0-9A-Fa-f]{1,4}(?::[0-9A-Fa-f]{1,4}){0,6})?(?:%\w+)?', $rxOpt)
$rxUsers = New-Object Text.RegularExpressions.Regex('([\\/]Users[\\/]+)[^\\/\r\n"<>|:*?]+', $rxOpt)
$rxJoin1 = New-Object Text.RegularExpressions.Regex('(\bjoined:[ \t]*)(?!<player>)([^\r\n]*?)(?=[ \t]+(?:(?:\d{1,3}\.){3}\d{1,3}|\[[0-9A-Fa-f:]+\])(?::\d+)?[ \t]*\r?$)', ([Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [Text.RegularExpressions.RegexOptions]::Multiline))
$rxJoin2 = New-Object Text.RegularExpressions.Regex('(\bjoined:[ \t]*)(?!<player>)[^\s]+', $rxOpt)
$ip4Eval = [Text.RegularExpressions.MatchEvaluator]{ param($m) if ($m.Value -eq '127.0.0.1' -or $m.Value -eq '0.0.0.0') { $m.Value } else { '<ip>' } }
function Latin1-Of([string]$s) { return $L1.GetString([Text.Encoding]::UTF8.GetBytes($s)) }
$wholeWords = New-Object System.Collections.ArrayList
foreach ($pair in @(@($env:USERNAME, '<user>'), @($env:COMPUTERNAME, '<pc>'))) {
    $n = [string]$pair[0]
    if ($n -and $n.Trim().Length -ge 3) {
        $n = $n.Trim()
        foreach ($variant in @($n, (Latin1-Of $n))) {
            $rx = New-Object Text.RegularExpressions.Regex(('(?<![A-Za-z0-9_])' + [regex]::Escape($variant) + '(?![A-Za-z0-9_])'), $rxOpt)
            [void]$wholeWords.Add(@($rx, $pair[1]))
        }
    }
}
function Scrub-Text([string]$t) {
    $t = $rxJoin1.Replace($t, '${1}<player>')    # the player name before the address
    $t = $rxJoin2.Replace($t, '${1}<player>')
    $t = $rxUsers.Replace($t, '${1}<user>')
    $t = $rxIp4.Replace($t, $ip4Eval)
    $t = $rxIp6a.Replace($t, '<ip>')
    $t = $rxIp6b.Replace($t, '<ip>')
    $t = $rxIp6c.Replace($t, '<ip>')
    foreach ($w in $wholeWords) { $t = $w[0].Replace($t, $w[1]) }
    return $t
}
$scrubNotes = New-Object System.Collections.ArrayList
$scrubFailed = $false
$nScrubbed = 0
foreach ($f in @(Get-ChildItem -LiteralPath $stage -Recurse -File -ErrorAction SilentlyContinue)) {
    $rel = $f.FullName.Substring($stage.Length + 1)
    $ok = $false
    try {
        if ($scrubExt -contains $f.Extension.ToLowerInvariant()) {
            $bytes = [IO.File]::ReadAllBytes($f.FullName)
            $hasNul = [Array]::IndexOf($bytes, [byte]0) -ge 0
            $u16 = $bytes.Length -ge 2 -and (($bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) -or ($bytes[0] -eq 0xFE -and $bytes[1] -eq 0xFF))
            if (-not ($hasNul -and $u16)) {
                $t = $L1.GetString($bytes)
                # (s185 review) a log with stray NUL bytes (a crash or power cut in the middle of a write) is the one the report is
                # for: the NULs are dropped and the rest is scrubbed like any text, instead of leaving the whole log out
                if ($hasNul) { $t = $t.Replace([string][char]0, ''); [void]$scrubNotes.Add('NUL bytes removed: ' + $rel) }
                $t2 = Scrub-Text $t
                if ($hasNul -or ($t2 -cne $t)) { [IO.File]::WriteAllBytes($f.FullName, $L1.GetBytes($t2)) }
                $ok = $true
                $nScrubbed++
            } else { [void]$scrubNotes.Add('left out (not plain text): ' + $rel) }
        } else { [void]$scrubNotes.Add('left out (not a text file type): ' + $rel) }
    } catch { [void]$scrubNotes.Add('left out (could not be scrubbed: ' + $_.Exception.Message + '): ' + $rel) }
    if (-not $ok) {
        try { Remove-Item -LiteralPath $f.FullName -Force -ErrorAction Stop } catch { $scrubFailed = $true }
    }
}
[void]$scrubNotes.Insert(0, 'privacy scrub: ' + $nScrubbed + ' text files scrubbed (IP addresses -> <ip>, Windows user / PC name and player names -> <user> <pc> <player>)')
$scrubNotes | Set-Content -LiteralPath (Join-Path $stage 'scrub_notes.txt') -Encoding UTF8

$code = 0
try {
    if ($scrubFailed) { throw 'a file could not be scrubbed or removed, so no zip was made' }
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
        L += [CHCP]
    L += ["rem Legend of Kairi - Make bug report. Written by the installer; running the installer again rewrites this file.",
          "rem Double-click it after something went wrong: it puts ONE zip on your Desktop (game logs, install logs, the launchers,",
          "rem savagewind.ini and a systeminfo.txt with Windows / graphics card / driver / RAM). No saves, no game files.",
          "setlocal",
          'set "SORA_ROOT=%~dp0"',
          'set "GAME=' + sw + '"',
          r'set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"',
          r'if not exist "%PS%" set "PS=C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe"',
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


# (s185) Windows shortcuts with the Legend of Kairi icon next to the launchers (and, only when the player asks, on the Desktop).
# The .lnk files start the .bat files above (the friend's address still lives in the Multiplayer .bat). Window style: NORMAL, not
# minimized: a launcher that stops on a pre-check ("files are incomplete", "put your friend's address here", "already running") shows
# its message and waits for a key; when the checks pass it hands over with `start /min` and closes at once, so the console only flashes.
SHORTCUTS = (("Legend of Kairi.lnk", SOLO, "Play Legend of Kairi on your own"),)   # (s223) single player: no Multiplayer shortcut
OLD_MULTI_LNK = "Legend of Kairi Multiplayer.lnk"     # (s223) 0.1.4's: removed by an update
DESKTOP_LNK = SHORTCUTS[0][0]
ICON_REL = ("tools", "installer", "host", "kairi.ico")     # our own heart-in-stained-glass icon (also the window program's); ships in tools/
SHORTCUT_PS1 = os.path.join(HERE, "shortcut.ps1")
POWERSHELL = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "WindowsPowerShell", "v1.0", "powershell.exe")


def shortcut_icon(root):
    p = os.path.join(root, *ICON_REL)
    return p if os.path.isfile(p) else ""


def make_shortcut(folder, name, target, workdir, icon="", desc=""):
    """one .lnk via shortcut.ps1 (WScript.Shell). folder "" = the player's real Desktop (resolved by the shell). Every path travels in
    the environment, nothing is pasted into a command. -> the written path; raises RuntimeError (one line) when it could not."""
    if sys.platform != "win32":
        raise RuntimeError("shortcuts are made on Windows only")
    sysroot = os.environ.get("SystemRoot", r"C:\Windows")
    env = {k: v for k, v in os.environ.items() if not k.upper().startswith(("PYTHON", "KAIRI_LNK_"))}
    env["PATH"] = os.pathsep.join([os.path.join(sysroot, "System32"), sysroot, os.path.join(sysroot, "System32", "WindowsPowerShell", "v1.0")])
    env.update({"KAIRI_LNK_DIR": os.path.normpath(folder) if folder else "", "KAIRI_LNK_NAME": name, "KAIRI_LNK_TARGET": os.path.normpath(target),
                "KAIRI_LNK_WD": os.path.normpath(workdir), "KAIRI_LNK_ICON": os.path.normpath(icon) if icon else "", "KAIRI_LNK_DESC": desc})
    try:
        r = subprocess.run([POWERSHELL, "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", SHORTCUT_PS1],
                           env=env, cwd=sysroot, stdin=subprocess.DEVNULL, capture_output=True, timeout=60,
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    except (OSError, subprocess.SubprocessError) as e:
        raise RuntimeError("PowerShell did not run (%s)" % e)
    out = r.stdout.decode("utf-8", "replace").strip()
    if r.returncode or not out:
        err = r.stderr.decode("mbcs" if sys.platform == "win32" else "utf-8", "replace").strip().splitlines()
        raise RuntimeError((err[-1] if err else "exit %d" % r.returncode)[:300])
    return out


def install_shortcuts(root, log=print):
    """the shortcut in the install folder; a failure is logged (one line) and never stops the install. -> the paths made"""
    made = []
    icon = shortcut_icon(root)
    for name, bat, desc in SHORTCUTS:
        try:
            made.append(make_shortcut(root, name, os.path.join(root, bat), root, icon, desc))
        except RuntimeError as e:
            log("[game] could not make the shortcut %s (%s): start %s instead" % (name, e, bat))
    if made:
        log("[game] shortcuts: %s" % ", ".join(os.path.basename(p) for p in made))
    return made


def desktop_shortcut(root, desktop=""):
    """(s185) the done screen's "Add a desktop shortcut": "Legend of Kairi.lnk" on the Desktop -> the solo launcher of <root>.
    desktop "" = the real one (tests pass a folder). -> the path; raises RuntimeError in plain words."""
    bat = os.path.join(root, SOLO)
    if not os.path.isfile(bat):
        raise RuntimeError("%s was not found in:\n%s" % (SOLO, root))
    try:
        return make_shortcut(desktop, DESKTOP_LNK, bat, root, shortcut_icon(root), SHORTCUTS[0][2])
    except RuntimeError as e:
        raise RuntimeError("Could not add the shortcut to your desktop (%s). You can start the game from the folder instead." % e)


def write_game_id(root, release, sw_dir, log=print):
    """(s223) data/GAME_ID = the id of the game code in the Savage Wind folder, when this release's savagewind.exe carries it
    (else the install stops: the game would not load that code). -> the id ('' = none to check: no game code id file)"""
    import swsetup
    gid = swsetup.game_id(sw_dir)
    p = os.path.join(root, "data", GAME_ID_FILE)
    if not gid:
        log("[game] no game code id in %s: the launcher skips that check" % sw_dir)
        try:
            os.remove(p)
        except OSError:
            pass
        return ""
    if not swsetup.exe_has_id(os.path.join(release, "bin", "savagewind.exe"), gid):
        raise RuntimeError("Savage Wind's game code in %s (%s) is not the one this Legend of Kairi was made for. Run the installer "
                           "again." % (sw_dir, gid))
    os.makedirs(os.path.join(root, "data"), exist_ok=True)
    with open(p, "w", newline="") as f:
        f.write(gid + "\r\n")
    log("[game] game code id %s" % gid)
    return gid


def remove_old_multiplayer(root, log=print):
    """(s223) single player release: 0.1.4's "Play Legend of Kairi Multiplayer.bat" + its .lnk go. The .bat is kept as text under
    work/removed (it holds the friend's address, for going back to 0.1.4), never as a .bat. -> the removed names"""
    gone = []
    bat = os.path.join(root, MULTI)
    if os.path.isfile(bat):
        keep = os.path.join(root, *REMOVED_DIR)
        os.makedirs(keep, exist_ok=True)
        shutil.copy2(bat, os.path.join(keep, MULTI + ".txt"))
        os.remove(bat)
        gone.append(MULTI)
    lnk = os.path.join(root, OLD_MULTI_LNK)
    if os.path.isfile(lnk):
        os.remove(lnk)
        gone.append(OLD_MULTI_LNK)
    if gone:
        log("[game] removed %s (co-op is not in this release; a copy of the old launcher is in %s)" % (", ".join(gone), os.path.join(*REMOVED_DIR)))
    return gone


def install_game_side(root, sw_dir, log=print, release=RELEASE):
    """-> {"solo": path, "bin": path, ...}. Raises RuntimeError with a plain-words message. (s223) single player: the
    Multiplayer launcher is neither written nor kept."""
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
    nsigned = 0
    for n in names:
        p = os.path.join(src, n)
        if not os.path.isfile(p):
            raise RuntimeError("%s is missing from this download. Download Legend of Kairi again." % n)
        how = pesig.matches(p, want[n]["sha256"], want[n]["size"]) if want else "unsigned"
        if not how:
            raise RuntimeError("%s is damaged (it does not match the release). Download Legend of Kairi again." % n)
        if how == "signed":
            nsigned += 1
    log("[game] %d program files checked%s" % (len(names), " (%d with a signature added)" % nsigned if nsigned else ""))
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
    solo = os.path.join(root, SOLO)
    write_game_id(root, release, sw_dir, log)
    _write_bat(solo, launcher_text(False, sw_dir))
    removed = remove_old_multiplayer(root, log)
    bug = os.path.join(root, BUGREPORT)
    _write_bat(bug, bugreport_text(sw_dir))
    _write_bat(os.path.join(root, BUGREPORT_PS1), bugreport_ps_text())
    log("[game] launchers: %s, %s (+ %s)" % (SOLO, BUGREPORT, BUGREPORT_PS1))
    try:
        lnks = install_shortcuts(root, log)
    except Exception as e:  # noqa: BLE001 - (s185) a shortcut is a nicety: it never fails the install
        log("[game] could not make the shortcuts (%s): start the .bat files instead" % e)
        lnks = []
    write_build_id(root, release, log)
    return {"solo": solo, "bugreport": bug, "bin": dst, "shortcuts": lnks, "removed": removed}

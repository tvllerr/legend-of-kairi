# Legend of Kairi installer (s185): writes one Windows shortcut (.lnk). Started by gameside.py with
#   powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File shortcut.ps1
# Every path comes in through the environment (never on a command line, so & ' % ( ) in a folder name are plain text):
#   KAIRI_LNK_DIR     the folder the shortcut goes into; empty = this user's real Desktop (it may be redirected, e.g. to OneDrive)
#   KAIRI_LNK_NAME    the shortcut's file name (ends in .lnk)
#   KAIRI_LNK_TARGET  the .bat it starts      KAIRI_LNK_WD  its working folder
#   KAIRI_LNK_ICON    an .ico file (optional) KAIRI_LNK_DESC  the tooltip
# Prints the full path of the shortcut it wrote; exit code 1 + one line on stderr when it could not.
$ErrorActionPreference = 'Stop'
try { [Console]::OutputEncoding = New-Object Text.UTF8Encoding($false) } catch { }    # the printed path is read as UTF-8
try {
    $dir = $env:KAIRI_LNK_DIR
    if (-not $dir) {
        $dir = [Environment]::GetFolderPath('Desktop')
        if (-not $dir -or -not [IO.Directory]::Exists($dir)) { $dir = Join-Path $env:USERPROFILE 'Desktop' }
    }
    if (-not $dir -or -not [IO.Directory]::Exists($dir)) { throw ('the folder does not exist: ' + $dir) }
    $name = $env:KAIRI_LNK_NAME
    if (-not $name -or -not $name.EndsWith('.lnk') -or $name.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0) { throw 'bad shortcut name' }
    if (-not [IO.File]::Exists($env:KAIRI_LNK_TARGET)) { throw ('the launcher does not exist: ' + $env:KAIRI_LNK_TARGET) }
    $path = [IO.Path]::Combine($dir, $name)
    $sh = New-Object -ComObject WScript.Shell
    $lnk = $sh.CreateShortcut($path)
    $lnk.TargetPath = $env:KAIRI_LNK_TARGET
    $lnk.WorkingDirectory = $env:KAIRI_LNK_WD
    if ($env:KAIRI_LNK_ICON -and [IO.File]::Exists($env:KAIRI_LNK_ICON)) { $lnk.IconLocation = $env:KAIRI_LNK_ICON + ',0' }
    if ($env:KAIRI_LNK_DESC) { $lnk.Description = $env:KAIRI_LNK_DESC }
    $lnk.WindowStyle = 1    # normal: the launcher's checks must stay readable (it closes by itself once the game starts)
    $lnk.Save()
    if (-not [IO.File]::Exists($path)) { throw 'the shortcut was not written' }
    [Console]::Out.Write($path)
    exit 0
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}

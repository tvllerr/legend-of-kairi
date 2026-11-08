@echo off
rem Play Sora in BotW 4.1 - 2nd copy ("COPY2") for the multiplayer test on this PC (slice 4, spec 3.3).
rem Needs the folder made by:  sh D:/SoraBotW/src/<tree>/tools/mp/copy2.sh   (D:\SoraBotW\play2\game: own save\, cache\, ini).
rem Same exe as the main launcher, but its own game folder, live file and log, port 7791, name PC2, pinned to the main game
rem (127.0.0.1:7790). The main launcher's "already running" check is NOT here: the main copy may be running.
rem Only a 2nd COPY2 is refused (window title check). Live commands: D:\SoraBotW\play2\live_test.txt
rem Log of the last run: D:\SoraBotW\play2\logs\last41.log (the one before it: prev41.log).
rem RAM: 2 x ~5.8 GB on 16 GB is tight; this is a test only. Start the MAIN game first (it owns port 7790).
setlocal
set "GAME=D:\SoraBotW\play2\game"
set "EXE=D:\SoraBotW\src\sw-a41\build\savagewind.exe"
set "PLAY=D:\SoraBotW\play2"
set "TITLE=Sora in BotW 4.1 COPY2"

tasklist /v /fi "imagename eq cmd.exe" /fi "windowtitle eq %TITLE%" | find /i "cmd.exe" >nul
if not errorlevel 1 (
    echo COPY2 is already running. Close it first, then run this again.
    pause
    exit /b 1
)
if not exist "%GAME%\extracted\swgame.dll" (
    echo Can't find %GAME% - run copy2.sh first.
    pause
    exit /b 1
)
if not exist "%EXE%" (
    echo Can't find the Sora build: %EXE%
    pause
    exit /b 1
)

if not exist "%PLAY%\logs" mkdir "%PLAY%\logs"
if exist "%PLAY%\logs\last41.log" move /y "%PLAY%\logs\last41.log" "%PLAY%\logs\prev41.log" >nul

set SAVAGEWIND_SORA=1
set "SAVAGEWIND_SORA_LIVE=%PLAY%\live_test.txt"
rem multiplayer: name PC2, own port 7791, pinned to the main game on this PC
set SAVAGEWIND_SORA_NET=1
set SAVAGEWIND_SORA_NET_NAME=PC2
set SAVAGEWIND_SORA_NET_PORT=7791
set SAVAGEWIND_SORA_NET_PEERS=127.0.0.1:7790
rem (s110) "The Legend of KAIRI" title logo in the opening cutscene (remove this line = BotW's own logo)
set "SAVAGEWIND_LOGO_TEX=D:\SoraBotW\data\logo\Logo3DGameTitle_00.bin"
rem (s135) KAIRI loading-screen logo (remove this line = BotW logo)
set "SAVAGEWIND_LOGO2D_TEX=D:\SoraBotW\data\logo\Logo2D_00.bin"
rem (s115) "The Legend of KAIRI" logo on the title screen too: RomFS overlay with Pack/Title.pack (Logo_W_00). NEVER put
rem Bootup.pack in this folder (it would skip the HUD button letters). Remove this line = BotW title logo.
set "SAVAGEWIND_ROMFS_OVERLAY=D:\SoraBotW\data\overlay"

cd /d "%GAME%" || (pause & exit /b 1)
start "%TITLE%" /min cmd /c ""%EXE%" extracted > "%PLAY%\logs\last41.log" 2>&1"

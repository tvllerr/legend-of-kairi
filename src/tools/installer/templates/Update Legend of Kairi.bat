@echo off
rem Legend of Kairi: look on GitHub for a newer release, download it, and open its installer to update this folder.
rem Needs the kit python that "Install Legend of Kairi" set up.
if not exist "%~dp0kit\python\python.exe" (
  echo Run Install Legend of Kairi first.
  pause
  exit /b 1
)
"%~dp0kit\python\python.exe" -E -P "%~dp0tools\installer\selfupdate.py" %*
if errorlevel 1 (
  pause
  exit /b 1
)
exit /b 0

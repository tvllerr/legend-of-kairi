@echo off
rem Legend of Kairi installer (Sora in BotW): the kit's own Python shows the installer window (nothing else needs to be installed)
start "" "%~dp0kit\python\pythonw.exe" -E -P "%~dp0tools\installer\installer.py"

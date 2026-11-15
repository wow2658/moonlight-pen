@echo off
cd /d "%~dp0"
cscript.exe //nologo "%~dp0install-shortcuts.vbs"
if errorlevel 1 (
    echo Installation failed. Keep this message and report it.
)
pause

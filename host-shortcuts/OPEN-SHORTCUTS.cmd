@echo off
cd /d "%~dp0"
cscript.exe //nologo "%~dp0open-shortcuts.vbs"
if errorlevel 1 pause

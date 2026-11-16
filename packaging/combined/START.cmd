@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0support\launch-combined.ps1"
if errorlevel 1 pause

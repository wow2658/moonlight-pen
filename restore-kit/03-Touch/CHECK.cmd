@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -CheckOnly
set "restoreExit=%errorlevel%"
pause
exit /b %restoreExit%

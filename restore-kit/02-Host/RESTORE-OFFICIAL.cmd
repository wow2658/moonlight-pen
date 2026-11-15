@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0restore-official.ps1"
set "restoreExit=%errorlevel%"
pause
exit /b %restoreExit%

@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0start-client.ps1"
set "restoreExit=%errorlevel%"
if not "%restoreExit%"=="0" pause
exit /b %restoreExit%

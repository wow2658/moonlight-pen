@echo off
setlocal
set "packageRoot=%~dp0"
cd /d "%TEMP%"
start "Moonlight remove" "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%packageRoot%support\uninstall-client.ps1" -Root "%packageRoot%."
exit /b

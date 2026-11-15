@echo off
setlocal
cd /d "%~dp0app"
if not exist "Moonlight.exe" (
    echo Moonlight.exe is missing. Extract the entire 01-Laptop ZIP first.
    echo Keep this launcher and the app folder together.
    pause
    exit /b 1
)
start "Moonlight Pen" /wait "Moonlight.exe"
if errorlevel 1 (
    echo Moonlight could not start or exited with an error.
    echo Check Windows Security and the files in the app folder.
    pause
    exit /b 1
)

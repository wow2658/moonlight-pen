@echo off
setlocal
cd /d "%~dp0.."
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist build-public\host-helper mkdir build-public\host-helper
cd build-public\host-helper
cl /nologo /EHsc /std:c++17 /utf-8 /O2 /MT /DUNICODE /D_UNICODE ..\..\host-helper\main.cpp /Fe:Moonlight-Display-Helper.exe /link user32.lib shell32.lib ole32.lib /subsystem:windows
if errorlevel 1 exit /b 1
start /wait "" Moonlight-Display-Helper.exe --self-test
exit /b %errorlevel%

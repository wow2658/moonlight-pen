@echo off
setlocal
cd /d "%~dp0.."
where cl >nul 2>nul
if errorlevel 1 exit /b 1
where qmake >nul 2>nul
if errorlevel 1 exit /b 1
if not exist libs\windows exit /b 1
if not exist build-public mkdir build-public
cd build-public
cl /nologo /EHsc /std:c++17 /utf-8 ..\tests\penpressure-test.cpp /Fe:penpressure-test.exe
if errorlevel 1 exit /b 1
penpressure-test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 ..\tests\hostdisplay-test.cpp /Fe:hostdisplay-test.exe
if errorlevel 1 exit /b 1
hostdisplay-test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE ..\tests\hostdisplay-paint-test.cpp ..\app\streaming\input\hostdisplaycontrols.cpp /Fe:hostdisplay-paint-test.exe /link user32.lib gdi32.lib comctl32.lib gdiplus.lib
if errorlevel 1 exit /b 1
hostdisplay-paint-test.exe
if errorlevel 1 exit /b 1
qmake ..\moonlight-qt.pro "CONFIG+=release" "CONFIG-=debug" "QMAKE_CXXFLAGS+=/utf-8"
if errorlevel 1 exit /b 1
where jom >nul 2>nul
if errorlevel 1 (
    nmake release
) else (
    jom -j 3 release
)
exit /b %errorlevel%

@echo off
setlocal
if "%~1"=="" (
  echo Usage: build-touch-release.cmd OUTPUT_DIRECTORY
  echo Use an x64 MSVC developer prompt and an output directory outside the repository.
  exit /b 1
)
if not exist "%~1" mkdir "%~1"
set "SOURCES=%~dp0..\touch-toggle"
cl /nologo /EHsc /std:c++17 /utf-8 /W4 /WX /O2 /MT /guard:cf /D_WIN32_WINNT=0x0A00 /DMOONLIGHT_PUBLIC_TOUCH "%SOURCES%\TouchToggle.cpp" cfgmgr32.lib advapi32.lib user32.lib gdi32.lib shell32.lib /Fo:"%~1\TouchToggle.obj" /Fe:"%~1\TouchToggle.exe" /link /SUBSYSTEM:WINDOWS /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /DEPENDENTLOADFLAG:0x800 /MANIFEST:EMBED /MANIFESTINPUT:"%SOURCES%\TouchToggle.manifest" /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
if errorlevel 1 exit /b 1
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0write-touch-release-pin.ps1" -OutputDirectory "%~1"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /W4 /WX /O2 /MT /guard:cf /D_WIN32_WINNT=0x0A00 /DMOONLIGHT_PUBLIC_TOUCH /I"%~1" "%SOURCES%\TouchDispatcher.cpp" cfgmgr32.lib advapi32.lib bcrypt.lib user32.lib gdi32.lib shell32.lib /Fo:"%~1\TouchDispatcher.obj" /Fe:"%~1\TouchDispatcher.exe" /link /SUBSYSTEM:WINDOWS /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /DEPENDENTLOADFLAG:0x800 /MANIFEST:EMBED /MANIFESTINPUT:"%SOURCES%\TouchToggle.manifest" /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /W4 /WX /O2 /MT "%~dp0..\tests\touch-device-selection-test.cpp" /Fo:"%~1\touch-device-selection-test.obj" /Fe:"%~1\touch-device-selection-test.exe"
if errorlevel 1 exit /b 1
"%~1\touch-device-selection-test.exe"
exit /b %errorlevel%

@echo off
setlocal
if "%~2"=="" (
  echo Usage: build-touch-tools.cmd PRIVATE_PROFILE_DIRECTORY OUTPUT_DIRECTORY
  echo Run in an x64 MSVC developer command prompt. Keep both directories outside this repository.
  exit /b 1
)
if not exist "%~1\device-profile.local.h" exit /b 1
if not exist "%~2" mkdir "%~2"
cl /nologo /EHsc /std:c++17 /utf-8 /W4 /WX /O2 /MT /guard:cf /D_WIN32_WINNT=0x0A00 /I"%~1" "%~dp0..\touch-toggle\TouchToggle.cpp" cfgmgr32.lib user32.lib gdi32.lib shell32.lib /Fo:"%~2\TouchToggle.obj" /Fe:"%~2\TouchToggle.exe" /link /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT /MANIFEST:EMBED /MANIFESTINPUT:"%~dp0..\touch-toggle\TouchToggle.manifest" /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0pin-touch-backend.ps1" -ProfileDirectory "%~1" -Backend "%~2\TouchToggle.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /W4 /WX /O2 /MT /guard:cf /D_WIN32_WINNT=0x0A00 /I"%~1" "%~dp0..\touch-toggle\TouchDispatcher.cpp" cfgmgr32.lib bcrypt.lib user32.lib gdi32.lib shell32.lib /Fo:"%~2\TouchDispatcher.obj" /Fe:"%~2\TouchDispatcher.exe" /link /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT /MANIFEST:EMBED /MANIFESTINPUT:"%~dp0..\touch-toggle\TouchToggle.manifest" /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
exit /b %errorlevel%

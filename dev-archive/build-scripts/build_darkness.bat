@echo off
rem Full build of our Darkness port. Updated 2026-10-15: the source moved from E: to D: (2026-10-11), and since the
rem SDK's imgui-public patch (02) was dropped the game needs the SDK's third-party include list, the same as
rem Condemned 2's build_game.bat. Needs lld-link.exe (a copy of ld.lld.exe) in the LLVM-MinGW bin\ (MACHINES.md).
setlocal
if "%LOGDIR%"=="" set LOGDIR=%TEMP%
set GAME=D:\the-darkness\src\DarknessRecomp
set SDK=D:\condemned-2-vr\src\rexglue-sdk
set "TP=%SDK%\thirdparty"
set "INCS=-I%TP%/imgui -I%TP%/fmt/include -I%TP%/simde -I%TP%/simde/simde -I%TP%/spdlog/include -I%TP%/renderdoc -I%TP%/sdl3/include -I%TP%/dxc/include -I%TP%/tomlplusplus/include"

call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat" > "%LOGDIR%\vcv4.log" 2>&1
set "PATH=C:\Program Files\CMake\bin;C:\Users\Tefa\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"

cd /d "%GAME%"
if exist out\build\win-amd64-release rmdir /s /q out\build\win-amd64-release

echo ### CONFIGURE (same recipe as Condemned 2)
cmake --preset win-amd64-release -DREXSDK_DIR=%SDK% -DCMAKE_C_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_C_FLAGS="-march=x86-64-v2 %INCS%" -DCMAKE_CXX_FLAGS="-march=x86-64-v2 %INCS%" > "%LOGDIR%\dcfg.txt" 2>&1
if errorlevel 1 (
  echo CONFIGURE FAILED
  powershell -NoProfile -Command "Select-String -Path '%LOGDIR%\dcfg.txt' -Pattern 'rror' | Select-Object -First 12 | ForEach-Object { $_.Line }"
  exit /b 1
)
echo CONFIGURE OK

echo.
echo ### BUILD (full log -^> dbuild.txt)
cmake --build out\build\win-amd64-release > "%LOGDIR%\dbuild.txt" 2>&1
echo build errorlevel: %errorlevel%
echo --- first failures, if any ---
powershell -NoProfile -Command "Select-String -Path '%LOGDIR%\dbuild.txt' -Pattern 'error:|error [A-Z]+[0-9]+|FAILED' | Select-Object -First 10 | ForEach-Object { $_.Line }"
powershell -NoProfile -Command "Get-Content '%LOGDIR%\dbuild.txt' -Tail 3"

echo.
echo ### PRODUCED:
dir /b "%GAME%\out\build\win-amd64-release\*.exe" 2>nul
echo ### FINISHED

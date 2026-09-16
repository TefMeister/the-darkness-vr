@echo off
setlocal
set LOGDIR=C:\Users\Tefa\AppData\Local\Temp\claude\D--Program-Files--x86--Steam-steamapps-common\433c5e9a-8719-4657-add8-0d486cd69aab\scratchpad
set GAME=E:\the-darkness\src\DarknessRecomp
set SDK=E:\condemned-2-vr\src\rexglue-sdk

call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat" > "%LOGDIR%\vcv4.log" 2>&1
set "PATH=C:\Program Files\CMake\bin;C:\Users\Tefa\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"

cd /d "%GAME%"
if exist out\build\win-amd64-release rmdir /s /q out\build\win-amd64-release

echo ### CONFIGURE (same recipe that worked for Condemned 2)
cmake --preset win-amd64-release -DREXSDK_DIR=%SDK% -DCMAKE_C_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_C_FLAGS="-march=x86-64-v2" -DCMAKE_CXX_FLAGS="-march=x86-64-v2" > "%LOGDIR%\dcfg.txt" 2>&1
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

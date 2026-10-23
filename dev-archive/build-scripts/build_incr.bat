@echo off
rem Incremental build of our Darkness port on the dev PC: no wipe, no fresh configure (CMake re-runs
rem itself when CMakeLists.txt changed). Updated 2026-10-23: source moved E: -> D: (2026-10-11), and the
rem log folder comes from %LOGDIR% (default %TEMP%) instead of an old session's scratchpad.
setlocal
if "%LOGDIR%"=="" set LOGDIR=%TEMP%
call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat" > "%LOGDIR%\vcv5.log" 2>&1
set "PATH=C:\Program Files\CMake\bin;%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"
cd /d D:\the-darkness\src\DarknessRecomp
echo ### INCREMENTAL BUILD (no reconfigure, no wipe)
cmake --build out\build\win-amd64-release > "%LOGDIR%\incr.txt" 2>&1
echo build errorlevel: %errorlevel%
powershell -NoProfile -Command "Select-String -Path '%LOGDIR%\incr.txt' -Pattern 'error:|error [A-Z]+[0-9]+|FAILED' | Select-Object -First 10 | ForEach-Object { $_.Line }"
powershell -NoProfile -Command "Get-Content '%LOGDIR%\incr.txt' -Tail 3"
echo ### FINISHED

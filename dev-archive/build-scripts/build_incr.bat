@echo off
setlocal
set LOGDIR=C:\Users\Tefa\AppData\Local\Temp\claude\D--Program-Files--x86--Steam-steamapps-common\433c5e9a-8719-4657-add8-0d486cd69aab\scratchpad
call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat" > "%LOGDIR%\vcv5.log" 2>&1
set "PATH=C:\Program Files\CMake\bin;C:\Users\Tefa\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"
cd /d E:\the-darkness\src\DarknessRecomp
echo ### INCREMENTAL BUILD (no reconfigure, no wipe)
cmake --build out\build\win-amd64-release > "%LOGDIR%\incr.txt" 2>&1
echo build errorlevel: %errorlevel%
powershell -NoProfile -Command "Select-String -Path '%LOGDIR%\incr.txt' -Pattern 'error:|error [A-Z]+[0-9]+|FAILED' | Select-Object -First 10 | ForEach-Object { $_.Line }"
powershell -NoProfile -Command "Get-Content '%LOGDIR%\incr.txt' -Tail 3"
echo ### FINISHED

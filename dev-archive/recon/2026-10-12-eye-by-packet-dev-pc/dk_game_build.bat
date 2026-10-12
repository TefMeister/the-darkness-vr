@echo off
setlocal
rem Dev-PC build of The Darkness port after the 2026-10-11 move from E: to D: (fresh configure, same recipe).
set LOGDIR=C:\Users\Tefa\AppData\Local\Temp\claude\D--Program-Files--x86--Steam-steamapps-common\2679f761-c3e6-49f9-a555-ea18de416130\scratchpad
set LLVM=C:\Users\Tefa\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260616-ucrt-x86_64\bin
set GAME=D:\the-darkness\src\DarknessRecomp
set SDK=D:\condemned-2-vr\src\rexglue-sdk
set "TP=%SDK%\thirdparty"
set "INCS=-I%TP%/imgui -I%TP%/fmt/include -I%TP%/simde -I%TP%/simde/simde -I%TP%/spdlog/include -I%TP%/renderdoc -I%TP%/sdl3/include -I%TP%/dxc/include -I%TP%/tomlplusplus/include"
call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat" > "%LOGDIR%\vcv2.log" 2>&1
set "PATH=C:\Program Files\CMake\bin;C:\Users\Tefa\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"
cd /d "%GAME%"
if exist out\build\win-amd64-release\build.ninja goto configured
cmake --preset win-amd64-release -DREXSDK_DIR=%SDK% -DCMAKE_C_COMPILER=%LLVM%\clang.exe -DCMAKE_CXX_COMPILER=%LLVM%\clang++.exe -DCMAKE_LINKER=%LLVM%\lld-link.exe -DCMAKE_C_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_C_FLAGS="-march=x86-64-v2 %INCS%" -DCMAKE_CXX_FLAGS="-march=x86-64-v2 %INCS%" > "%LOGDIR%\game-cfg.txt" 2>&1
if errorlevel 1 ( echo GAME CONFIGURE FAILED & powershell -NoProfile -Command "Get-Content '%LOGDIR%\game-cfg.txt' -Tail 15" & exit /b 1 )
:configured
cmake --build out\build\win-amd64-release > "%LOGDIR%\game-build.txt" 2>&1
echo GAME build errorlevel: %errorlevel%
powershell -NoProfile -Command "Select-String -Path '%LOGDIR%\game-build.txt' -Pattern 'error:|FAILED' | Select-Object -First 10 | ForEach-Object { $_.Line }"
powershell -NoProfile -Command "Get-Content '%LOGDIR%\game-build.txt' -Tail 3"
dir /b out\build\win-amd64-release\*.exe out\build\win-amd64-release\*.dll 2>nul

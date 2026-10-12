@echo off
setlocal
rem Dev-PC SDK build after the 2026-10-11 move from E: to D: (fresh configure, same settings as before).
set LOGDIR=C:\Users\Tefa\AppData\Local\Temp\claude\D--Program-Files--x86--Steam-steamapps-common\2679f761-c3e6-49f9-a555-ea18de416130\scratchpad
set LLVM=C:\Users\Tefa\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260616-ucrt-x86_64\bin
call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat" > "%LOGDIR%\vcv.log" 2>&1
set "PATH=C:\Program Files\CMake\bin;C:\Users\Tefa\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"
cd /d D:\condemned-2-vr\src\rexglue-sdk
if exist out\build\win-amd64\build.ninja goto configured
cmake --preset win-amd64 -DCMAKE_C_COMPILER=%LLVM%\clang.exe -DCMAKE_CXX_COMPILER=%LLVM%\clang++.exe -DCMAKE_LINKER=%LLVM%\lld-link.exe -DCMAKE_C_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 > "%LOGDIR%\sdk-cfg.txt" 2>&1
if errorlevel 1 ( echo CONFIGURE FAILED & powershell -NoProfile -Command "Get-Content '%LOGDIR%\sdk-cfg.txt' -Tail 15" & exit /b 1 )
:configured
cmake --build out\build\win-amd64 --config Release > "%LOGDIR%\sdk-build.txt" 2>&1
echo SDK build errorlevel: %errorlevel%
powershell -NoProfile -Command "Select-String -Path '%LOGDIR%\sdk-build.txt' -Pattern 'error:|FAILED' | Select-Object -First 10 | ForEach-Object { $_.Line }"
powershell -NoProfile -Command "Get-Content '%LOGDIR%\sdk-build.txt' -Tail 3"

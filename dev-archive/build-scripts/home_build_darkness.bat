@echo off
setlocal
rem Home-PC build of the Darkness port (2026-09-27). Project made with 'rexglue init' in D:\the-darkness-build\src\DarknessRecomp,
rem then this repo's port-project files copied over it; game-files is a junction to the game copy. Configure must
rem run AFTER codegen has once produced generated\default\sources.cmake, or the game code is not compiled in.
rem It also passes the SDK third-party include folders that condemned-2-vr's build_game.bat passes (the SDK
rem patch 02 that made them automatic no longer configures on SDK v0.10.0 release).
set LOGDIR=D:\the-darkness-build\logs
set GAME=D:\the-darkness-build\src\DarknessRecomp
set SDK=D:\the-darkness-build\rexglue-sdk
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > "%LOGDIR%\vcvars-game.log" 2>&1
if errorlevel 1 ( echo VCVARS FAILED & exit /b 1 )
set "PATH=C:\Program Files\LLVM\bin;C:\Program Files\CMake\bin;%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"
cd /d "%GAME%"
set "TP=%SDK%\thirdparty"
set "INCS=-I%TP%/imgui -I%TP%/fmt/include -I%TP%/simde -I%TP%/simde/simde -I%TP%/spdlog/include -I%TP%/renderdoc -I%TP%/sdl3/include -I%TP%/dxc/include -I%TP%/tomlplusplus/include"
if exist out\build\win-amd64-release\build.ninja goto built_cfg
echo ### CONFIGURE
cmake --preset win-amd64-release -DREXSDK_DIR=%SDK% -DCMAKE_C_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_C_FLAGS="-march=x86-64-v2 %INCS%" -DCMAKE_CXX_FLAGS="-march=x86-64-v2 %INCS%" > "%LOGDIR%\game-cfg.txt" 2>&1
if errorlevel 1 ( echo CONFIGURE FAILED & exit /b 1 )
echo CONFIGURE OK
:built_cfg
echo ### CODEGEN
cmake --build out\build\win-amd64-release --target darknessrecomp_codegen > "%LOGDIR%\game-codegen.txt" 2>&1
echo codegen errorlevel: %errorlevel%
echo ### BUILD
cmake --build out\build\win-amd64-release > "%LOGDIR%\game-build.txt" 2>&1
echo build errorlevel: %errorlevel%
dir /b out\build\win-amd64-release\*.exe 2>nul
echo ### FINISHED

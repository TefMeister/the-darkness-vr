@echo off
setlocal
rem Home-PC build of the ReXGlue SDK (2026-09-27). Checkout: rexglue-sdk v0.10.0 release (c94f5eb) with
rem submodules; then fixlinks (symlinks arrive as text on Windows), then these patches, line endings
rem LF: condemned-2-vr 01, and this repo's sdk-patches 04, 05, 06. Condemned-2-vr 02 is NOT applied:
rem on the v0.10.0 release it breaks configure (imgui "not in any export set"); the game build passes the
rem third-party include folders instead.
set ROOT=D:\the-darkness-build
set LOGDIR=%ROOT%\logs
if not exist "%LOGDIR%" mkdir "%LOGDIR%"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > "%LOGDIR%\vcvars.log" 2>&1
if errorlevel 1 ( echo VCVARS FAILED & exit /b 1 )
rem LLVM's own clang first: an llvm-mingw clang may also be on PATH and targets the wrong ABI.
set "PATH=C:\Program Files\LLVM\bin;C:\Program Files\CMake\bin;%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"
cd /d %ROOT%\rexglue-sdk
if exist out\build\win-amd64\build.ninja goto configured
echo ### CONFIGURE
cmake --preset win-amd64 -DCMAKE_C_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc > "%LOGDIR%\sdk-cfg.txt" 2>&1
if errorlevel 1 ( echo CONFIGURE FAILED & exit /b 1 )
:configured
echo ### BUILD Release
cmake --build out\build\win-amd64 --config Release > "%LOGDIR%\sdk-build.txt" 2>&1
echo build errorlevel: %errorlevel%
echo ### FINISHED

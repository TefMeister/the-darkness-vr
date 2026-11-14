@echo off
rem Builds and runs the Quest-to-pad mapping tests (job 1, 2026-11-14). No game, no SDK: the mapping is plain C++.
setlocal
set "HERE=%~dp0"
set "PP=%HERE%..\port-project"
set "OUT=%TEMP%\dk_test_quest_controls"
if not exist "%OUT%" mkdir "%OUT%"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > "%OUT%\vcvars.log" 2>&1
if errorlevel 1 ( echo VCVARS FAILED & exit /b 1 )
set "PATH=C:\Program Files\LLVM\bin;%PATH%"
clang-cl /nologo /std:c++latest /EHsc /W3 /Fe"%OUT%\test_quest_controls.exe" /Fo"%OUT%\\" "%PP%\tests\test_quest_controls.cpp" "%PP%\src\vr_quest_controls.cpp"
if errorlevel 1 ( echo BUILD FAILED & exit /b 1 )
"%OUT%\test_quest_controls.exe"
exit /b %errorlevel%

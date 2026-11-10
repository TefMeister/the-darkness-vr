@echo off
rem Clears a build tree's precompiled headers when the compiler has changed since they were made (2026-11-10).
rem Why: clang refuses a precompiled header made by another clang build ("PCH file built from a different branch"),
rem and ninja does not know the compiler changed, so it never remakes them: every file then fails. This happened
rem to the port's tree and to the SDK's own tree after the compiler update of 2026-11-03.
rem How: the compiler's own version text is kept in <tree>\compiler-stamp.txt; when it differs from the compiler
rem on PATH (or there is no stamp yet), every *.pch under the tree is deleted (ninja remakes them) and the stamp
rem is rewritten. Usage, after PATH is set:  call "%~dp0home_stale_pch.bat" <build tree>
setlocal
set "TREE=%~1"
if not exist "%TREE%\build.ninja" exit /b 0
set "NOW=%TEMP%\dk-compiler-now-%RANDOM%.txt"
clang --version > "%NOW%" 2>&1
if errorlevel 1 ( echo STALE-PCH CHECK: clang not found on PATH, nothing cleared & del "%NOW%" 2>nul & exit /b 0 )
if not exist "%TREE%\compiler-stamp.txt" goto clear
fc /b "%NOW%" "%TREE%\compiler-stamp.txt" > nul 2>&1
if not errorlevel 1 ( del "%NOW%" & exit /b 0 )
:clear
echo ### COMPILER CHANGED since this tree's precompiled headers were made: clearing them (they are rebuilt now)
del /s /q "%TREE%\*.pch" > nul 2>&1
copy /y "%NOW%" "%TREE%\compiler-stamp.txt" > nul
del "%NOW%"
exit /b 0

@echo off
rem ---------------------------------------------------------------------------
rem vs-env.bat <command...>
rem
rem Runs a command inside the Visual Studio x64 developer environment
rem (INCLUDE / LIB / WindowsSdkDir set, VS cmake + ninja on PATH). Required for
rem cmake --build on this repo -- see CLAUDE.md "Windows gotcha".
rem
rem Usage examples:
rem   vs-env.bat cmake -S . -B build
rem   vs-env.bat cmake --build build --config Debug
rem   vs-env.bat ctest --test-dir build -C Debug
rem ---------------------------------------------------------------------------
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 (
    echo vs-env.bat: failed to initialize VS dev environment 1>&2
    exit /b 1
)
%*

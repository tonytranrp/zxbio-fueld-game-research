@echo off
rem Double-click this to build Fuel Farm (the fast "dev" preset). Assumes
rem scripts\setup.bat has already been run on this machine at least once.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
if errorlevel 1 (
    echo.
    pause
)

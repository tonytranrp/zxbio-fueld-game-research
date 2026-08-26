@echo off
rem Double-click this to set up a new machine for building Fuel Farm and then
rem build it. Installs Visual Studio C++ Build Tools, CMake, Ninja, Rust, Git,
rem and sccache; asks for administrator rights (a Windows permission prompt)
rem since Visual Studio's installer requires them.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1" %*

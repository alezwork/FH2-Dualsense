@echo off
rem FH2-DualSense: takes d3d12.dll away again (and puts back a d3d12.dll that was there before)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Uninstall %*

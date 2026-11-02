@echo off
rem FH2-DualSense: puts d3d12.dll next to azimuth.exe (finds Azimuth, or asks where it is)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*

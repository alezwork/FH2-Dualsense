@echo off
rem Builds DualSense.dll, the PlayStation 5 controller for Forza Horizon 2 on Azimuth 0.9.10, a mod of the FH2 Mod
rem Platform, into out\fh2-mods\ (the PlayStation pictures inside it: tools\make_pack.py).
rem DualSense.dll writes its DualSense\DualSense.ini (next to azimuth.exe) with the defaults when there is none.
rem build.cmd install   also copies it into <FH2_GAME>\Azimuth\fh2-mods\ (made when missing).
rem FH2_GAME: the folder with Azimuth\ and "Forza Horizon 2"\ (by default the one above this one), e.g.
rem   set FH2_GAME=D:\Games\Forza Horizon 2
rem Compiler: Visual Studio Build Tools (C++) when there, else zig (python -m pip install ziglang).
rem The PlayStation pictures need Python with numpy and Pillow and the game's files; without them the DLL has no
rem pictures of its own (it then takes a DualSense\DualSense.icons when there is one).
cd /d "%~dp0"
if not defined FH2_GAME set "FH2_GAME=%~dp0.."
set B=%TEMP%\fh2dualsense_build
set OUT=%~dp0out\fh2-mods
if not exist "%B%" mkdir "%B%"
if not exist "%OUT%" mkdir "%OUT%"
python -I tools\ini2h.py DualSense\DualSense.ini DualSense\ini_default.h || exit /b 1

rem the PlayStation pictures, drawn in the sizes of the game's own (needs the game and tools\xg\kernelx.dll)
if exist "%B%\DualSense.icons" del "%B%\DualSense.icons"
if not exist tools\xg\kernelx.dll call tools\xg\build.cmd
cd /d "%~dp0"
python -I tools\make_pack.py "%B%\DualSense.icons" "%FH2_GAME%\Forza Horizon 2\Mount" "%FH2_GAME%\Forza Horizon 2" || echo no PlayStation pictures made: the DLL gets none of its own
python -I tools\bin2h.py "%B%\DualSense.icons" DualSense\icons_pack.h || exit /b 1

set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if exist "%VCVARS%" call "%VCVARS%" >nul 2>nul
where cl >nul 2>nul && goto msvc
python -m ziglang version >nul 2>nul && goto zig
echo no C compiler: python -m pip install ziglang (or Visual Studio Build Tools with C++)
exit /b 1

:msvc
set CL_FLAGS=/nologo /O1 /Gy /W3 /MT /GS- /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS
cl %CL_FLAGS% /LD DualSense\DualSense.c /Fo"%B%\\" /link /OPT:REF /OPT:ICF /OUT:"%B%\DualSense.dll" /IMPLIB:"%B%\DualSense.lib" kernel32.lib user32.lib hid.lib setupapi.lib ole32.lib || exit /b 1
goto built

:zig
set ZCC=python -m ziglang cc -target x86_64-windows-gnu -Os -s -shared -DUNICODE -D_UNICODE -Wall -Wno-unused-function -Wno-missing-braces -ffunction-sections -fdata-sections -Wl,--gc-sections
%ZCC% DualSense\DualSense.c -o "%B%\DualSense.dll" -lhid -lsetupapi -lole32 -luser32 || exit /b 1

:built
copy /y "%B%\DualSense.dll" "%OUT%\" >nul || exit /b 1
for %%F in ("%OUT%\DualSense.dll") do echo built "%OUT%\DualSense.dll" (%%~zF bytes)
if /i not "%~1"=="install" exit /b 0

if not exist "%FH2_GAME%\Azimuth\azimuth.exe" (
  echo no "%FH2_GAME%\Azimuth\azimuth.exe": not installed
  exit /b 1
)
if not exist "%FH2_GAME%\Azimuth\fh2-mods" mkdir "%FH2_GAME%\Azimuth\fh2-mods"
copy /y "%OUT%\DualSense.dll" "%FH2_GAME%\Azimuth\fh2-mods\" >nul || exit /b 1
echo installed into "%FH2_GAME%\Azimuth\fh2-mods"
if not exist "%FH2_GAME%\Azimuth\d3dcompiler_47.dll" echo   the FH2 Mod Platform is not there: put its d3dcompiler_47.dll next to azimuth.exe (see the README)
if exist "%FH2_GAME%\Azimuth\mods\DualSense.dll" echo   an old mods\DualSense.dll is there (Azimuth 0.9.9): it can go

@echo off
rem Builds d3d12.dll, the PlayStation 5 controller for Forza Horizon 2 on Azimuth 0.9.10, into out\ (the PlayStation
rem pictures inside it: tools\make_pack.py). It goes next to azimuth.exe, beside Forza Horizon 2 Extra Options'
rem winmm.dll, which it needs; it writes its DualSense\DualSense.ini there with the defaults when there is none.
rem build.cmd install   also copies it into <FH2_GAME>\Azimuth\.
rem FH2_GAME: the folder with Azimuth\ and "Forza Horizon 2"\ (by default the one above this one), e.g.
rem   set FH2_GAME=D:\Games\Forza Horizon 2
rem Compiler: zig (python -m pip install ziglang): its clang builds the Direct3D 12 pass-through (proxy.h).
rem The PlayStation pictures need Python with numpy and Pillow and the game's files; without them the DLL has no
rem pictures of its own (it then takes a DualSense\DualSense.icons when there is one).
cd /d "%~dp0"
if not defined FH2_GAME set "FH2_GAME=%~dp0.."
set B=%TEMP%\fh2dualsense_build
set OUT=%~dp0out
if not exist "%B%" mkdir "%B%"
if not exist "%OUT%" mkdir "%OUT%"
python -I tools\ini2h.py DualSense\DualSense.ini DualSense\ini_default.h || exit /b 1

rem the PlayStation pictures, drawn in the sizes of the game's own (needs the game and tools\xg\kernelx.dll)
if exist "%B%\DualSense.icons" del "%B%\DualSense.icons"
if not exist tools\xg\kernelx.dll call tools\xg\build.cmd
cd /d "%~dp0"
python -I tools\make_pack.py "%B%\DualSense.icons" "%FH2_GAME%\Forza Horizon 2\Mount" "%FH2_GAME%\Forza Horizon 2" || echo no PlayStation pictures made: the DLL gets none of its own
python -I tools\bin2h.py "%B%\DualSense.icons" DualSense\icons_pack.h || exit /b 1

python -m ziglang version >nul 2>nul || (
  echo no zig: python -m pip install ziglang
  exit /b 1
)
set MH=third_party\minhook\src
python -m ziglang cc -target x86_64-windows-gnu -Os -s -shared -DUNICODE -D_UNICODE -Wall -Wno-unused-function -Wno-missing-braces -ffunction-sections -fdata-sections -Wl,--gc-sections DualSense\DualSense.c %MH%\hook.c %MH%\buffer.c %MH%\trampoline.c %MH%\hde\hde64.c DualSense\d3d12.def -o "%B%\d3d12.dll" -lhid -lsetupapi -lole32 -luser32 || exit /b 1

copy /y "%B%\d3d12.dll" "%OUT%\" >nul || exit /b 1
for %%F in ("%OUT%\d3d12.dll") do echo built "%OUT%\d3d12.dll" (%%~zF bytes)
if /i not "%~1"=="install" exit /b 0

if not exist "%FH2_GAME%\Azimuth\azimuth.exe" (
  echo no "%FH2_GAME%\Azimuth\azimuth.exe": not installed
  exit /b 1
)
copy /y "%OUT%\d3d12.dll" "%FH2_GAME%\Azimuth\" >nul || exit /b 1
echo installed into "%FH2_GAME%\Azimuth"
if not exist "%FH2_GAME%\Azimuth\winmm.dll" echo   no winmm.dll next to azimuth.exe: install Forza Horizon 2 Extra Options, the mod is off without it
if exist "%FH2_GAME%\Azimuth\DualSense\cache\UI.zip" echo   the copies of 2.2 in DualSense\cache\ are no longer used: the folder can go
if exist "%FH2_GAME%\Azimuth\fh2-mods\DualSense.dll" echo   an old fh2-mods\DualSense.dll (2.0, 2.1) is there: take it away, or the DualSense is driven twice
if exist "%FH2_GAME%\Azimuth\mods\DualSense.dll" echo   an old mods\DualSense.dll (1.x) is there: it can go

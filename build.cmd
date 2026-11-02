@echo off
rem Builds d3d12.dll, the PlayStation 5 controller and the keyboard for Forza Horizon 2 on Azimuth, into out\ (the
rem PlayStation pictures inside it: tools\make_pack.py), and the release in out\release\ and out\FH2-DualSense-<ver>.zip
rem (d3d12.dll, Install.cmd, Uninstall.cmd, install.ps1, README.txt). It goes next to azimuth.exe, with Forza Horizon 2
rem Extra Options or without it; it writes its DualSense\DualSense.ini there with the defaults when there is none.
rem build.cmd install   also installs it into the Azimuth folder (install\install.ps1 finds it, or AZIMUTH= says it).
rem FH2_GAME: the folder with "Forza Horizon 2"\Mount (by default the one above this one), e.g.
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
python -m ziglang cc -target x86_64-windows-gnu -Os -s -shared -DUNICODE -D_UNICODE -Wall -Wno-unused-function -Wno-missing-braces -ffunction-sections -fdata-sections -Wl,--gc-sections DualSense\DualSense.c %MH%\hook.c %MH%\buffer.c %MH%\trampoline.c %MH%\hde\hde64.c DualSense\d3d12.def -o "%B%\d3d12.dll" -lhid -lsetupapi -lole32 -luser32 -lgdi32 || exit /b 1

copy /y "%B%\d3d12.dll" "%OUT%\" >nul || exit /b 1
for %%F in ("%OUT%\d3d12.dll") do echo built "%OUT%\d3d12.dll" (%%~zF bytes)

rem the release: what a player downloads
for /f "tokens=3 delims= " %%V in ('findstr /c:"#define DS_VERSION" DualSense\base.h') do set VER=%%~V
set R=%OUT%\release
if exist "%R%" rmdir /s /q "%R%"
mkdir "%R%" || exit /b 1
copy /y "%OUT%\d3d12.dll" "%R%\" >nul
copy /y install\Install.cmd "%R%\" >nul
copy /y install\Uninstall.cmd "%R%\" >nul
copy /y install\install.ps1 "%R%\" >nul
copy /y install\README.txt "%R%\" >nul
if exist "%OUT%\FH2-DualSense-%VER%.zip" del "%OUT%\FH2-DualSense-%VER%.zip"
powershell -NoProfile -Command "Compress-Archive -Path '%R%\*' -DestinationPath '%OUT%\FH2-DualSense-%VER%.zip'" || exit /b 1
echo release "%OUT%\FH2-DualSense-%VER%.zip"
if /i not "%~1"=="install" exit /b 0

if defined AZIMUTH (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%R%\install.ps1" -Quiet -Azimuth "%AZIMUTH%"
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%R%\install.ps1" -Quiet
)

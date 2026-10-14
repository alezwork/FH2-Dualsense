@echo off
rem Builds the controller mod for Forza Horizon 2 on Azimuth into Source\out\mods\ -- one file for Azimuth\mods\:
rem   DualSense.dll   the DualSense, the Xbox controller's impulse triggers, the keyboard, the button icons
rem                   (the PlayStation pictures inside it: DualSense.icons from tools\make_pack.py)
rem It writes its DualSense.ini with the defaults when there is none. Nothing else to install.
rem build.cmd install   also copies it into ..\Azimuth\mods\ and ..\Azimuth FF\mods\.
rem Compiler: Visual Studio Build Tools (C++) when there, else zig (python -m pip install ziglang).
rem The PlayStation pictures need Python with numpy and Pillow and the game's files (..\Forza Horizon 2); without
rem them the DLL has no pictures of its own (it then takes a mods\DualSense.icons when there is one).
cd /d "%~dp0"
set B=%TEMP%\fh2input_build
set OUT=%~dp0out\mods
if not exist "%B%" mkdir "%B%"
if not exist "%OUT%" mkdir "%OUT%"
python -I tools\ini2h.py DualSense\DualSense.ini DualSense\ini_default.h || exit /b 1

rem the PlayStation pictures, drawn in the sizes of the game's own (needs the game and tools\xg\kernelx.dll)
if exist "%B%\DualSense.icons" del "%B%\DualSense.icons"
if not exist tools\xg\kernelx.dll call tools\xg\build.cmd
cd /d "%~dp0"
python -I tools\make_pack.py "%B%\DualSense.icons" "%~dp0..\Forza Horizon 2\Mount" "%~dp0..\Forza Horizon 2" || echo no PlayStation pictures made: the DLL gets none of its own
python -I tools\bin2h.py "%B%\DualSense.icons" DualSense\icons_pack.h || exit /b 1

set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if exist "%VCVARS%" call "%VCVARS%" >nul 2>nul
where cl >nul 2>nul && goto msvc
python -m ziglang version >nul 2>nul && goto zig
echo no C compiler: python -m pip install ziglang (or Visual Studio Build Tools with C++)
exit /b 1

:msvc
set CL_FLAGS=/nologo /O1 /Gy /W3 /MT /GS- /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS
cl %CL_FLAGS% /LD DualSense\DualSense.c /Fo"%B%\\" /link /OPT:REF /OPT:ICF /OUT:"%B%\DualSense.dll" /IMPLIB:"%B%\DualSense.lib" kernel32.lib user32.lib gdi32.lib hid.lib setupapi.lib ole32.lib || exit /b 1
goto built

:zig
set ZCC=python -m ziglang cc -target x86_64-windows-gnu -Os -s -shared -DUNICODE -D_UNICODE -Wall -Wno-unused-function -Wno-missing-braces -ffunction-sections -fdata-sections -Wl,--gc-sections
%ZCC% DualSense\DualSense.c -o "%B%\DualSense.dll" -lhid -lsetupapi -lole32 -luser32 -lgdi32 || exit /b 1

:built
copy /y "%B%\DualSense.dll" "%OUT%\" >nul || exit /b 1
for %%F in ("%OUT%\DualSense.dll") do echo built "%OUT%\DualSense.dll" (%%~zF bytes)
if /i not "%~1"=="install" exit /b 0

for %%E in ("%~dp0..\Azimuth" "%~dp0..\Azimuth FF") do (
  if exist "%%~E\mods" (
    copy /y "%OUT%\DualSense.dll" "%%~E\mods\" >nul || exit /b 1
    echo installed into "%%~E\mods"
    if exist "%%~E\mods\Keyboard.dll" echo   an old Keyboard.dll is there: the keyboard is in DualSense.dll now, it can go
    if exist "%%~E\mods\DualSense.icons" echo   a DualSense.icons is there: it is used instead of the pictures inside the DLL
  )
)

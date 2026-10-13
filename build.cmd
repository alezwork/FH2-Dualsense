@echo off
rem Builds the input mods for Forza Horizon 2 on Azimuth into Source\out\mods\ -- what goes into Azimuth\mods\:
rem   DualSense.dll     the PlayStation 5 controller, the button icons in three looks (DualSense\DualSense.c)
rem   DualSense.icons   the PlayStation icons (a zip of PNGs, tools\make_pack.py; the Xbox look is the game's own,
rem                     the keyboard look DualSense.dll draws from Keyboard.ini)
rem   Keyboard.dll      the keyboard (Keyboard\Keyboard.c)
rem Each DLL writes its ini with the defaults when there is none. Nothing else to install.
rem build.cmd install   also copies them into ..\Azimuth\mods\ and ..\Azimuth FF\mods\.
rem Compiler: Visual Studio Build Tools (C++) when there, else zig (python -m pip install ziglang).
cd /d "%~dp0"
set B=%TEMP%\fh2input_build
set OUT=%~dp0out\mods
if not exist "%B%" mkdir "%B%"
if not exist "%OUT%" mkdir "%OUT%"
python -I tools\ini2h.py DualSense\DualSense.ini DualSense\ini_default.h || exit /b 1
python -I tools\ini2h.py Keyboard\Keyboard.ini Keyboard\ini_default.h || exit /b 1
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if exist "%VCVARS%" call "%VCVARS%" >nul 2>nul
where cl >nul 2>nul && goto msvc
python -m ziglang version >nul 2>nul && goto zig
echo no C compiler: python -m pip install ziglang (or Visual Studio Build Tools with C++)
exit /b 1

:msvc
set CL_FLAGS=/nologo /O2 /W3 /MT /GS- /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS
cl %CL_FLAGS% /LD DualSense\DualSense.c /Fo"%B%\\" /link /OUT:"%B%\DualSense.dll" /IMPLIB:"%B%\DualSense.lib" kernel32.lib user32.lib gdi32.lib hid.lib setupapi.lib ole32.lib || exit /b 1
cl %CL_FLAGS% /LD Keyboard\Keyboard.c /Fo"%B%\\" /link /OUT:"%B%\Keyboard.dll" /IMPLIB:"%B%\Keyboard.lib" kernel32.lib user32.lib || exit /b 1
goto built

:zig
set ZCC=python -m ziglang cc -target x86_64-windows-gnu -O2 -shared -DUNICODE -D_UNICODE -Wall -Wno-unused-function -Wno-missing-braces
%ZCC% DualSense\DualSense.c -o "%B%\DualSense.dll" -lhid -lsetupapi -lole32 -luser32 -lgdi32 || exit /b 1
%ZCC% Keyboard\Keyboard.c -o "%B%\Keyboard.dll" -luser32 || exit /b 1

:built
copy /y "%B%\DualSense.dll" "%OUT%\" >nul || exit /b 1
copy /y "%B%\Keyboard.dll" "%OUT%\" >nul || exit /b 1
rem the PlayStation icons: drawn in the sizes of the game's own (needs the game and tools\xg\kernelx.dll)
if not exist tools\xg\kernelx.dll call tools\xg\build.cmd || exit /b 1
python -I tools\make_pack.py "%OUT%\DualSense.icons" "%~dp0..\Forza Horizon 2\Mount" "%~dp0..\Forza Horizon 2" || exit /b 1
echo built "%OUT%"
if /i not "%~1"=="install" exit /b 0

for %%E in ("%~dp0..\Azimuth" "%~dp0..\Azimuth FF") do (
  if exist "%%~E\mods" (
    copy /y "%OUT%\DualSense.dll" "%%~E\mods\" >nul || exit /b 1
    copy /y "%OUT%\DualSense.icons" "%%~E\mods\" >nul || exit /b 1
    copy /y "%OUT%\Keyboard.dll" "%%~E\mods\" >nul || exit /b 1
    echo installed into "%%~E\mods"
  )
)

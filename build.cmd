@echo off
rem Builds the controller mod for Forza Horizon 2 on Azimuth into out\ :
rem   out\mods\DualSense.dll        for <Azimuth>\mods\: the DualSense, the Xbox controller's impulse triggers, the
rem                                 keyboard, the button icons (the PlayStation pictures inside it: tools\make_pack.py)
rem   out\loader\azimuth-tls.dll    the mod loader, for <Azimuth>\data\bin\ (the helper that was there renamed
rem                                 azimuth-tls.dll.original: it is loaded first -- Azimuth's own or ExtraOptions)
rem DualSense.dll writes its DualSense.ini with the defaults when there is none.
rem build.cmd install   also copies DualSense.dll into <FH2_GAME>\Azimuth\mods\ (made when missing).
rem FH2_GAME: the folder with Azimuth\ and "Forza Horizon 2"\ (by default the one above this one), e.g.
rem   set FH2_GAME=D:\Forza Horizon 2 XboxOne
rem Compiler: Visual Studio Build Tools (C++) when there, else zig (python -m pip install ziglang).
rem The PlayStation pictures need Python with numpy and Pillow and the game's files; without them the DLL has no
rem pictures of its own (it then takes a mods\DualSense.icons when there is one).
cd /d "%~dp0"
if not defined FH2_GAME set "FH2_GAME=%~dp0.."
set B=%TEMP%\fh2input_build
set OUT=%~dp0out\mods
set OUTL=%~dp0out\loader
if not exist "%B%" mkdir "%B%"
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUTL%" mkdir "%OUTL%"
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
cl %CL_FLAGS% /LD DualSense\DualSense.c /Fo"%B%\\" /link /OPT:REF /OPT:ICF /OUT:"%B%\DualSense.dll" /IMPLIB:"%B%\DualSense.lib" kernel32.lib user32.lib gdi32.lib hid.lib setupapi.lib ole32.lib || exit /b 1
cl /nologo /O1 /W3 /GS- /DUNICODE /D_UNICODE /LD loader\modloader.c /Fo"%B%\\" /link /NODEFAULTLIB /ENTRY:DllMain /OUT:"%B%\azimuth-tls.dll" /IMPLIB:"%B%\azimuth-tls.lib" kernel32.lib || exit /b 1
goto built

:zig
set ZCC=python -m ziglang cc -target x86_64-windows-gnu -Os -s -shared -DUNICODE -D_UNICODE -Wall -Wno-unused-function -Wno-missing-braces -ffunction-sections -fdata-sections -Wl,--gc-sections
%ZCC% DualSense\DualSense.c -o "%B%\DualSense.dll" -lhid -lsetupapi -lole32 -luser32 -lgdi32 || exit /b 1
rem the loader: no C runtime, entry DllMain, its TLS directory kept
python -m ziglang cc -target x86_64-windows-gnu -O2 -fno-stack-protector -Wall -DUNICODE -D_UNICODE -c loader\modloader.c -o "%B%\modloader.o" || exit /b 1
python -m ziglang cc -target x86_64-windows-gnu -shared -nostdlib -s "%B%\modloader.o" -o "%B%\azimuth-tls.dll" -Wl,-e,DllMain -Wl,-u,_tls_used -lkernel32 || exit /b 1

:built
copy /y "%B%\DualSense.dll" "%OUT%\" >nul || exit /b 1
copy /y "%B%\azimuth-tls.dll" "%OUTL%\" >nul || exit /b 1
for %%F in ("%OUT%\DualSense.dll") do echo built "%OUT%\DualSense.dll" (%%~zF bytes)
for %%F in ("%OUTL%\azimuth-tls.dll") do echo built "%OUTL%\azimuth-tls.dll" (%%~zF bytes)
if /i not "%~1"=="install" exit /b 0

for %%E in ("%FH2_GAME%\Azimuth" "%FH2_GAME%\Azimuth FF") do (
  if exist "%%~E\azimuth.exe" (
    if not exist "%%~E\mods" mkdir "%%~E\mods"
    copy /y "%OUT%\DualSense.dll" "%%~E\mods\" >nul || exit /b 1
    echo installed into "%%~E\mods"
    if exist "%%~E\mods\Keyboard.dll" echo   an old Keyboard.dll is there: the keyboard is in DualSense.dll now, it can go
    if exist "%%~E\mods\DualSense.icons" echo   a DualSense.icons is there: it is used instead of the pictures inside the DLL
  )
)

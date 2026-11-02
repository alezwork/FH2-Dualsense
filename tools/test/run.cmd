@echo off
rem The mod without the game: out\d3d12.dll in a stand-in for azimuth.exe (fakeaz.c), with a data\azimuth.ini that
rem points at the game's folder. Checks that the game's files are given from DualSense\cache\, and that the icons
rem switch in a copy kept in memory. Then tools\test\check.py looks at the copies themselves, and isgame.c that the
rem game's exe is known as Forza Horizon 2 (and Horizon 3's is not).
rem   set FH2_GAME=D:\Games\Forza Horizon 2      (the folder with "Forza Horizon 2"\Mount)
rem   tools\test\run.cmd
setlocal
cd /d "%~dp0"
if not defined FH2_GAME set "FH2_GAME=%~dp0..\..\.."
set "MEDIA=%FH2_GAME%\Forza Horizon 2\Mount\media"
if not exist "%MEDIA%\zipmanifest.xml" (
  echo no "%MEDIA%\zipmanifest.xml": set FH2_GAME
  exit /b 1
)
set T=%TEMP%\fh2dualsense_test
set MH=..\..\third_party\minhook\src
if exist "%T%" rmdir /s /q "%T%"
mkdir "%T%\data" || exit /b 1
copy /y "%~dp0..\..\out\d3d12.dll" "%T%\" >nul || exit /b 1
(
  echo [title-1a96545b]
  echo folder=%FH2_GAME%\Forza Horizon 2
) > "%T%\data\azimuth.ini"
python -m ziglang cc -target x86_64-windows-gnu -O1 -municode fakeaz.c -o "%T%\azimuth.exe" -ld3d12 || exit /b 1
python -m ziglang cc -target x86_64-windows-gnu -O1 -municode isgame.c %MH%\hook.c %MH%\buffer.c %MH%\trampoline.c %MH%\hde\hde64.c -o "%T%\isgame.exe" 2>nul || exit /b 1
echo ---- the game's exe known (1 = Forza Horizon 2)
"%T%\isgame.exe" "%MEDIA%\..\forza_durango_release_final.exe"
set FH2_DS_TEST=1
"%T%\azimuth.exe" "%MEDIA%"
set R=%ERRORLEVEL%
echo ---- DualSense.log
type "%T%\DualSense\DualSense.log"
python -I "%~dp0check.py" "%T%\DualSense\cache" "%MEDIA%" || set R=1
exit /b %R%

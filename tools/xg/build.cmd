@echo off
rem kernelx.dll: the stub the game's xg_x.dll needs to run on a PC (tools\make_ps_icons.py loads both).
rem Compiler: Visual Studio Build Tools (C++) when there, else zig (python -m pip install ziglang).
cd /d "%~dp0"
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if exist "%VCVARS%" call "%VCVARS%" >nul 2>nul
where cl >nul 2>nul && goto msvc
python -m ziglang version >nul 2>nul && goto zig
echo no C compiler: python -m pip install ziglang (or Visual Studio Build Tools with C++)
exit /b 1
:msvc
cl /nologo /O2 /W3 /GS- /LD kernelx.c /link /DEF:kernelx.def /NODEFAULTLIB /ENTRY:DllMain /OUT:kernelx.dll kernel32.lib >build.log 2>&1 || (type build.log & exit /b 1)
del kernelx.obj kernelx.lib kernelx.exp build.log 2>nul
goto done
:zig
python -m ziglang cc -target x86_64-windows-gnu -O2 -shared kernelx.c kernelx.def -o "%TEMP%\kernelx.dll" || exit /b 1
copy /y "%TEMP%\kernelx.dll" kernelx.dll >nul || exit /b 1
:done
echo built kernelx.dll

@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

if not exist "%~dp0build\obs-lib" mkdir "%~dp0build\obs-lib"
cd /d "%~dp0build\obs-lib"

dumpbin /exports "C:\Program Files\obs-studio\bin\64bit\w32-pthreads.dll" > pthreads_exports_raw.txt

echo LIBRARY w32-pthreads > w32-pthreads.def
echo EXPORTS >> w32-pthreads.def

for /f "tokens=1,2,3,4" %%A in ('findstr /r "^[ ][ ]*[0-9]" pthreads_exports_raw.txt') do (
    if not "%%D"=="" echo     %%D >> w32-pthreads.def
)

lib /def:w32-pthreads.def /out:w32-pthreads.lib /machine:x64

if exist w32-pthreads.lib (
    echo SUCCESS: w32-pthreads.lib created
) else (
    echo FAILED
    exit /b 1
)

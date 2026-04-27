@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

echo Generating import library from obs.dll...

if not exist "%~dp0build\obs-lib" mkdir "%~dp0build\obs-lib"
cd /d "%~dp0build\obs-lib"

dumpbin /exports "C:\Program Files\obs-studio\bin\64bit\obs.dll" > obs_exports_raw.txt

echo LIBRARY obs > obs.def
echo EXPORTS >> obs.def

setlocal enabledelayedexpansion
set "IN_EXPORTS=0"
for /f "tokens=1,2,3,4*" %%A in (obs_exports_raw.txt) do (
    if "%%A"=="ordinal" (
        set "IN_EXPORTS=1"
    ) else if !IN_EXPORTS!==1 (
        echo %%A | findstr /r "^[0-9]" >nul 2>&1
        if !errorlevel!==0 (
            if not "%%D"=="" (
                echo     %%D >> obs.def
            )
        )
    )
)

lib /def:obs.def /out:obs.lib /machine:x64

if exist obs.lib (
    echo SUCCESS: obs.lib created
) else (
    echo FAILED: obs.lib not created
)

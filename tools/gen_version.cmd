@echo off
setlocal
for /f "delims=" %%v in ('git describe --tags --always --dirty') do set GITVER=%%v

rem --long always yields TAG-N-gHASH (dot/hyphen-delimited), so the numeric FILEVERSION/
rem PRODUCTVERSION parts for the .rc resource can be pulled out positionally.
set VMAJOR=0
set VMINOR=0
set VPATCH=0
set VBUILD=0
for /f "tokens=1-4 delims=.-" %%a in ('git describe --tags --always --long') do (
    set VMAJOR=%%a
    set VMINOR=%%b
    set VPATCH=%%c
    set VBUILD=%%d
)

set OUT=%~dp0..\src\Version.h
set TMP=%OUT%.tmp

> "%TMP%" (
    echo #pragma once
    echo.
    echo #define MINIMD_VERSION "%GITVER%"
    echo #define MINIMD_VERSION_MAJOR %VMAJOR%
    echo #define MINIMD_VERSION_MINOR %VMINOR%
    echo #define MINIMD_VERSION_PATCH %VPATCH%
    echo #define MINIMD_VERSION_BUILD %VBUILD%
)

fc /b "%TMP%" "%OUT%" >nul 2>&1
if errorlevel 1 (
    move /y "%TMP%" "%OUT%" >nul
) else (
    del "%TMP%"
)

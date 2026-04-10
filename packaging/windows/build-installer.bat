@echo off
REM ═══════════════════════════════════════════════════════════════════
REM  Build the Inno Setup installer for cool-retro-term
REM ═══════════════════════════════════════════════════════════════════

setlocal EnableDelayedExpansion

set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
set "SCRIPTDIR=%~dp0"
set "DEPLOYDIR=%SCRIPTDIR%..\..\deploy\cool-retro-term"
set "DISTDIR=%SCRIPTDIR%..\..\dist"

REM ── Verify deploy directory exists ─────────────────────────────────
if not exist "!DEPLOYDIR!\cool-retro-term.exe" (
    echo ERROR: Deploy directory not found or missing executable.
    echo Please run build.bat first.
    exit /b 1
)

REM ── Verify Inno Setup ──────────────────────────────────────────────
if not exist "!ISCC!" (
    echo ERROR: Inno Setup 6 not found at !ISCC!
    echo Please install Inno Setup 6 from https://jrsoftware.org/isinfo.php
    exit /b 1
)

REM ── Create dist directory ──────────────────────────────────────────
if not exist "!DISTDIR!" mkdir "!DISTDIR!"

REM ── Build installer ────────────────────────────────────────────────
echo Building installer...
"!ISCC!" "!SCRIPTDIR!installer.iss"
if errorlevel 1 (
    echo ERROR: Installer build failed.
    exit /b 1
)

echo.
echo ===================================================================
echo  Installer created successfully!
echo  Output: !DISTDIR!\cool-retro-term-1.0.0-win64-setup.exe
echo ===================================================================

exit /b 0

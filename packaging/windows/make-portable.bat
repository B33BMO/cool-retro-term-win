@echo off
REM ═══════════════════════════════════════════════════════════════════
REM  Create a portable ZIP distribution of cool-retro-term
REM ═══════════════════════════════════════════════════════════════════

setlocal

set "SCRIPTDIR=%~dp0"
set "DEPLOYDIR=%SCRIPTDIR%..\..\deploy\cool-retro-term"
set "DISTDIR=%SCRIPTDIR%..\..\dist"
set "ZIPNAME=cool-retro-term-1.0.0-win64-portable.zip"

REM ── Verify deploy directory exists ─────────────────────────────────
if not exist "%DEPLOYDIR%\cool-retro-term.exe" (
    echo ERROR: Deploy directory not found or missing executable.
    echo Please run build.bat first.
    exit /b 1
)

REM ── Create dist directory ──────────────────────────────────────────
if not exist "%DISTDIR%" mkdir "%DISTDIR%"

REM ── Create ZIP using PowerShell ────────────────────────────────────
echo Creating portable ZIP...

REM Remove old zip if exists
if exist "%DISTDIR%\%ZIPNAME%" del "%DISTDIR%\%ZIPNAME%"

powershell -Command "Compress-Archive -Path '%DEPLOYDIR%' -DestinationPath '%DISTDIR%\%ZIPNAME%' -Force"
if errorlevel 1 (
    echo ERROR: Failed to create ZIP archive.
    exit /b 1
)

echo.
echo ═══════════════════════════════════════════════════════════════════
echo  Portable ZIP created successfully!
echo  Output: %DISTDIR%\%ZIPNAME%
echo ═══════════════════════════════════════════════════════════════════

exit /b 0

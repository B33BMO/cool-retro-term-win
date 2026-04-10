@echo off
REM ═══════════════════════════════════════════════════════════════════
REM  cool-retro-term: Build Everything
REM  Compiles, deploys, creates installer AND portable zip
REM ═══════════════════════════════════════════════════════════════════

setlocal
set "SCRIPTDIR=%~dp0"

echo.
echo  ██████╗ ██████╗  ██████╗ ██╗          ██████╗ ███████╗████████╗██████╗  ██████╗
echo ██╔════╝██╔═══██╗██╔═══██╗██║          ██╔══██╗██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗
echo ██║     ██║   ██║██║   ██║██║   █████╗ ██████╔╝█████╗     ██║   ██████╔╝██║   ██║
echo ╚██████╗╚██████╔╝╚██████╔╝███████╗     ██╔══██╗██╔══╝     ██║   ██╔══██╗╚██████╔╝
echo  ╚═════╝ ╚═════╝  ╚═════╝ ╚══════╝     ╚═╝  ╚═╝╚══════╝     ╚═╝   ╚═╝  ╚═╝ ╚═════╝
echo                         Windows Build Pipeline
echo.

REM ── Step 1: Build and Deploy ───────────────────────────────────────
echo *** Step 1: Building and deploying...
call "%SCRIPTDIR%build.bat" --clean
if errorlevel 1 (
    echo.
    echo FAILED at Step 1 ^(Build^). See errors above.
    exit /b 1
)

REM ── Step 2: Create Installer ───────────────────────────────────────
echo.
echo *** Step 2: Creating installer...
call "%SCRIPTDIR%build-installer.bat"
if errorlevel 1 (
    echo.
    echo FAILED at Step 2 ^(Installer^). See errors above.
    exit /b 1
)

REM ── Step 3: Create Portable ZIP ────────────────────────────────────
echo.
echo *** Step 3: Creating portable ZIP...
call "%SCRIPTDIR%make-portable.bat"
if errorlevel 1 (
    echo.
    echo FAILED at Step 3 ^(Portable ZIP^). See errors above.
    exit /b 1
)

echo.
echo ═══════════════════════════════════════════════════════════════════
echo  ALL DONE! Distribution files:
echo.
echo  Installer:    dist\cool-retro-term-1.0.0-win64-setup.exe
echo  Portable ZIP: dist\cool-retro-term-1.0.0-win64-portable.zip
echo ═══════════════════════════════════════════════════════════════════

exit /b 0

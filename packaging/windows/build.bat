@echo off
REM ═══════════════════════════════════════════════════════════════════
REM  cool-retro-term Windows Build Script
REM  Builds the project with MSVC 2022 + Qt 6.11 and runs windeployqt
REM ═══════════════════════════════════════════════════════════════════

setlocal enabledelayedexpansion

REM ── Configuration ──────────────────────────────────────────────────
set "QTDIR=C:\Qt\6.11.0\msvc2022_64"
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "JOM=C:\Qt\Tools\QtCreator\bin\jom\jom.exe"
set "SRCDIR=%~dp0..\.."
set "BUILDDIR=%SRCDIR%\build-win64"
set "DEPLOYDIR=%SRCDIR%\deploy\cool-retro-term"
set "ARCH=amd64"

REM ── Parse Arguments ────────────────────────────────────────────────
set "CLEAN="
set "DEPLOY_ONLY="
:parse_args
if "%1"=="--clean" (set "CLEAN=1" & shift & goto parse_args)
if "%1"=="--deploy-only" (set "DEPLOY_ONLY=1" & shift & goto parse_args)
if "%1"=="--help" goto :usage
if "%1"=="-h" goto :usage
goto :start

:usage
echo Usage: build.bat [--clean] [--deploy-only]
echo   --clean        Clean build directory before building
echo   --deploy-only  Skip build, only run windeployqt on existing build
exit /b 0

:start

REM ── Verify Prerequisites ──────────────────────────────────────────
if not exist "%QTDIR%\bin\qmake.exe" (
    echo ERROR: Qt not found at %QTDIR%
    echo Please install Qt 6.11.0 MSVC 2022 64-bit or update QTDIR in this script.
    exit /b 1
)
if not exist "%VCVARS%" (
    echo ERROR: Visual Studio 2022 not found.
    echo Please install VS 2022 or update VCVARS in this script.
    exit /b 1
)

REM ── Set up MSVC environment ────────────────────────────────────────
echo [1/4] Setting up MSVC environment...
call "%VCVARS%" %ARCH%
if errorlevel 1 (
    echo ERROR: Failed to set up MSVC environment.
    exit /b 1
)

REM Add Qt to PATH
set "PATH=%QTDIR%\bin;%PATH%"

if defined DEPLOY_ONLY goto :deploy

REM ── Clean (optional) ──────────────────────────────────────────────
if defined CLEAN (
    echo [1/4] Cleaning build directory...
    if exist "%BUILDDIR%" rmdir /s /q "%BUILDDIR%"
)

REM ── Pre-compile shaders ────────────────────────────────────────────
REM (must run before qmake/rcc so the QRC can find them)
echo [1b/4] Pre-compiling shaders with qsb...
call :build_shaders
if errorlevel 1 exit /b 1

REM ── Create build directory ─────────────────────────────────────────
if not exist "%BUILDDIR%" mkdir "%BUILDDIR%"
pushd "%BUILDDIR%"

REM ── Run qmake ──────────────────────────────────────────────────────
echo [2/4] Running qmake...
"%QTDIR%\bin\qmake.exe" "%SRCDIR%\cool-retro-term.pro" -spec win32-msvc CONFIG+=release
if errorlevel 1 (
    echo ERROR: qmake failed.
    popd
    exit /b 1
)

REM ── Build ──────────────────────────────────────────────────────────
REM Use nmake (single-threaded) to avoid jom issues with extension-less Qt headers
echo [3/4] Building with nmake...
nmake
if errorlevel 1 (
    echo ERROR: Build failed.
    popd
    exit /b 1
)

popd

:deploy
REM ── Deploy ─────────────────────────────────────────────────────────
echo [4/4] Running windeployqt...

REM Clean and recreate deploy directory
if exist "%DEPLOYDIR%" rmdir /s /q "%DEPLOYDIR%"
mkdir "%DEPLOYDIR%"

REM Copy the main executable
copy "%BUILDDIR%\cool-retro-term.exe" "%DEPLOYDIR%\" >nul

REM Copy the qmltermwidget QML plugin
mkdir "%DEPLOYDIR%\qmltermwidget\QMLTermWidget"
copy "%BUILDDIR%\qmltermwidget\QMLTermWidget\qmltermwidget.dll" "%DEPLOYDIR%\qmltermwidget\QMLTermWidget\" >nul
copy "%SRCDIR%\qmltermwidget\src\qmldir" "%DEPLOYDIR%\qmltermwidget\QMLTermWidget\" >nul
copy "%SRCDIR%\qmltermwidget\src\QMLTermScrollbar.qml" "%DEPLOYDIR%\qmltermwidget\QMLTermWidget\" >nul

REM Copy color schemes, keyboard layouts, and shaders via PowerShell
REM (avoiding xcopy to keep EDR logs clean)
powershell -NoProfile -Command ^
    "Copy-Item -Path '%SRCDIR%\qmltermwidget\lib\color-schemes' -Destination '%DEPLOYDIR%\qmltermwidget\QMLTermWidget\color-schemes' -Recurse -Force; " ^
    "Copy-Item -Path '%SRCDIR%\qmltermwidget\lib\kb-layouts' -Destination '%DEPLOYDIR%\qmltermwidget\QMLTermWidget\kb-layouts' -Recurse -Force; " ^
    "if (Test-Path '%SRCDIR%\app\shaders\*.qsb') { New-Item -ItemType Directory -Force -Path '%DEPLOYDIR%\shaders' | Out-Null; Copy-Item -Path '%SRCDIR%\app\shaders\*.qsb' -Destination '%DEPLOYDIR%\shaders\' -Force }"

REM Run windeployqt to collect Qt DLLs, QML modules, and platform plugins
"%QTDIR%\bin\windeployqt.exe" --release --qmldir "%SRCDIR%\app\qml" "%DEPLOYDIR%\cool-retro-term.exe"
if errorlevel 1 (
    echo WARNING: windeployqt reported issues, but deployment may still work.
)

echo.
echo ═══════════════════════════════════════════════════════════════════
echo  Build complete!
echo  Output: %DEPLOYDIR%
echo ═══════════════════════════════════════════════════════════════════
echo.
echo To create installer: Run packaging\windows\build-installer.bat
echo To create portable zip: Run packaging\windows\make-portable.bat

exit /b 0

REM ═══════════════════════════════════════════════════════════════════
REM  Subroutine: build_shaders
REM  Pre-compiles all shader variants using Qt Shader Baker (qsb)
REM ═══════════════════════════════════════════════════════════════════
:build_shaders
set "QSB=%QTDIR%\bin\qsb.exe"
set "SHADERSDIR=%SRCDIR%\app\shaders"

REM Simple (non-variant) shaders
for %%F in (burn_in.frag burn_in.vert terminal_frame.frag terminal_frame.vert terminal_dynamic.vert terminal_static.vert) do (
    if not exist "%SHADERSDIR%\%%F.qsb" (
        "%QSB%" --glsl "100 es,120,150" --hlsl 50 --msl 12 --qt6 -o "%SHADERSDIR%\%%F.qsb" "%SHADERSDIR%\%%F"
        if errorlevel 1 exit /b 1
    )
)

REM Dynamic variants (5 raster modes x 2 burn-in x 2 frame x 2 chroma = 40)
for %%R in (0 1 2 3 4) do (
    for %%B in (0 1) do (
        for %%D in (0 1) do (
            for %%C in (0 1) do (
                set "OUT=%SHADERSDIR%\terminal_dynamic_raster%%R_burn%%B_frame%%D_chroma%%C.frag.qsb"
                if not exist "!OUT!" (
                    "%QSB%" --glsl "100 es,120,150" --hlsl 50 --msl 12 --qt6 ^
                        -DCRT_RASTER_MODE=%%R -DCRT_BURN_IN=%%B -DCRT_DISPLAY_FRAME=%%D -DCRT_CHROMA=%%C ^
                        -o "!OUT!" "%SHADERSDIR%\terminal_dynamic.frag"
                    if errorlevel 1 exit /b 1
                )
            )
        )
    )
)

REM Static variants (2 rgb x 2 bloom x 2 curve x 2 shine = 16)
for %%R in (0 1) do (
    for %%B in (0 1) do (
        for %%C in (0 1) do (
            for %%S in (0 1) do (
                set "OUT=%SHADERSDIR%\terminal_static_rgb%%R_bloom%%B_curve%%C_shine%%S.frag.qsb"
                if not exist "!OUT!" (
                    "%QSB%" --glsl "100 es,120,150" --hlsl 50 --msl 12 --qt6 ^
                        -DCRT_RGB_SHIFT=%%R -DCRT_BLOOM=%%B -DCRT_CURVATURE=%%C -DCRT_FRAME_SHININESS=%%S ^
                        -o "!OUT!" "%SHADERSDIR%\terminal_static.frag"
                    if errorlevel 1 exit /b 1
                )
            )
        )
    )
)

exit /b 0

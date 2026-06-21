@echo off
REM Week 0 build script for simple-daw.
REM Initializes the Visual Studio 2022 dev environment, then runs CMake configure + build.
REM Mirrors the pattern from Markdown Viewer / build-release.bat.

setlocal

set "VSDEVCMD=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
    echo [error] VsDevCmd.bat not found at "%VSDEVCMD%".
    echo         Open Visual Studio Installer and confirm "Desktop development with C++" is installed.
    exit /b 1
)

set "CMAKEROOT=C:\Program Files\CMake"
if not exist "%CMAKEROOT%\bin\cmake.exe" (
    echo [error] CMake not found at "%CMAKEROOT%".
    echo         Install with: winget install Kitware.CMake
    exit /b 1
)

set "PATH=%CMAKEROOT%\bin;%PATH%"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"

set "BUILDDIR=build"
set "BINDIR=%BUILDDIR%\%CONFIG%"

call "%VSDEVCMD%" -arch=amd64 -host_arch=amd64
if errorlevel 1 exit /b 1

if not exist "%BUILDDIR%" (
    echo [configure] Generating %BUILDDIR%...
    cmake -S . -B %BUILDDIR% -G "Visual Studio 17 2022" -A x64
    if errorlevel 1 exit /b 1
)

echo [build] %CONFIG% into %BINDIR%...
cmake --build %BUILDDIR% --config %CONFIG% --parallel
if errorlevel 1 exit /b 1

set "EXE=%BUILDDIR%\SimpleDaw_artefacts\%CONFIG%\Simple DAW.exe"
if exist "%EXE%" (
    echo.
    echo [ok] Built "%EXE%"
    echo [run]  Run:  "%EXE%"
) else (
    echo [error] Simple DAW.exe not found at "%EXE%".
    exit /b 1
)

endlocal

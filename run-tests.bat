@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [run-tests] Failed to initialize vcvars64.
    exit /b 1
)
cmake --build build --config Debug --target SimpleDawTests
if errorlevel 1 exit /b 1
"build\SimpleDawTests_artefacts\Debug\SimpleDawTests.exe"
exit /b %ERRORLEVEL%

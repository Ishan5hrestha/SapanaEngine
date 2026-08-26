@echo off
setlocal enabledelayedexpansion

rem Load the MSVC build environment (adjust path/edition if different)
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo Failed to load MSVC environment. Check the vcvars64.bat path below.
    exit /b 1
)

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD=%ROOT%\build\Windows"
set "OUT=%BUILD%\game\sandbox_cube"

cmake --build "%BUILD%" -j %NUMBER_OF_PROCESSORS% --target SapanaSandbox
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

if not exist "%OUT%" mkdir "%OUT%"
robocopy "%ROOT%\game\sandbox_cube\assets" "%OUT%" /E /NFL /NDL /NJH /NJS
if errorlevel 8 (
    echo Asset sync failed.
    exit /b 1
)

echo Assets synced -^> %OUT%
endlocal

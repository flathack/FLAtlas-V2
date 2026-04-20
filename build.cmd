@echo off
setlocal
cd /d "%~dp0"

set "QT_ROOT=C:\Qt\6.8.3\mingw_64"
set "QT_BIN=%QT_ROOT%\bin"
set "MINGW_BIN=C:\Qt\Tools\mingw1310_64\bin"
set "BUILD_DIR=build"

set "PATH=%QT_BIN%;%MINGW_BIN%;%PATH%"

where cmake >nul 2>nul
if errorlevel 1 (
    echo cmake wurde nicht gefunden.
    pause
    exit /b 1
)

if not exist "%QT_ROOT%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo Qt6Config.cmake nicht gefunden:
    echo   %QT_ROOT%\lib\cmake\Qt6\Qt6Config.cmake
    pause
    exit /b 1
)

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo Konfiguriere CMake...
    cmake -S . -B "%BUILD_DIR%" -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="%QT_ROOT%"
    if errorlevel 1 (
        echo CMake-Konfiguration fehlgeschlagen.
        pause
        exit /b 1
    )
)

echo Baue FLAtlas...
cmake --build "%BUILD_DIR%" --target FLAtlas
if errorlevel 1 (
    echo Build fehlgeschlagen.
    pause
    exit /b 1
)

echo Build erfolgreich:
echo   %CD%\%BUILD_DIR%\src\FLAtlas.exe
endlocal

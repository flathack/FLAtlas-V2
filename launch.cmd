@echo off
setlocal
cd /d "%~dp0"

set "QT_DIR=C:\Qt\6.8.3\mingw_64\bin"
set "MINGW_DIR=C:\Qt\Tools\mingw1310_64\bin"
set "PATH=%QT_DIR%;%MINGW_DIR%;%PATH%"

if not exist "build\src\FLAtlas.exe" (
    echo FLAtlas.exe nicht gefunden. Bitte zuerst bauen:
    echo   cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="%QT_DIR%\.."
    echo   cmake --build build
    pause
    exit /b 1
)

start "" "build\src\FLAtlas.exe"
endlocal

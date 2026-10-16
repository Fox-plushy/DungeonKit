@echo off
setlocal
title DungeonKit quick build

set PATH=C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.12.0\mingw_64\bin;C:\Program Files\CMake\bin;%PATH%
set QTDIR=C:/Qt/6.12.0/mingw_64

rem ---- always work in the folder this file is in, same as Package-DungeonKit.bat ----
cd /d "%~dp0"
if errorlevel 1 goto fail
if not exist CMakeLists.txt if exist "%~dp0DungeonKit\CMakeLists.txt" cd /d "%~dp0DungeonKit"
if not exist CMakeLists.txt if exist "C:\Users\Admin\Downloads\DungeonKit\CMakeLists.txt" cd /d "C:\Users\Admin\Downloads\DungeonKit"
if not exist CMakeLists.txt (
    echo  *** Could not find CMakeLists.txt. Put this file inside the DungeonKit folder, next to CMakeLists.txt. ***
    goto fail
)
echo  Using folder: %CD%

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QTDIR%"
if errorlevel 1 goto fail

cmake --build build
if errorlevel 1 goto fail

windeployqt build\DungeonKit.exe
if errorlevel 1 goto fail

echo.
echo  BUILD OK - starting DungeonKit...
start "" "build\DungeonKit.exe"
exit /b 0

:fail
echo.
echo  *** Something above failed. Copy the error lines and give them to Plushy, or try to figure it out yourself ***
echo.
pause
exit /b 1

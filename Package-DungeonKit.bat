@echo off
setlocal
title DungeonKit package builder

rem ---- Qt / MinGW / CMake locations (same as Build-DungeonKit.bat) ----
set PATH=C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.12.0\mingw_64\bin;C:\Program Files\CMake\bin;%PATH%
set QTDIR=C:/Qt/6.12.0/mingw_64

rem ---- always work in the folder this file is in ----
cd /d "%~dp0"
if errorlevel 1 goto fail
if not exist CMakeLists.txt if exist "%~dp0DungeonKit\CMakeLists.txt" cd /d "%~dp0DungeonKit"
if not exist CMakeLists.txt if exist "C:\Users\Admin\Downloads\DungeonKit\CMakeLists.txt" cd /d "C:\Users\Admin\Downloads\DungeonKit"
if not exist CMakeLists.txt (
    echo  *** Could not find CMakeLists.txt. Put this file inside the DungeonKit folder, next to CMakeLists.txt. ***
    goto fail
)
echo  Using folder: %CD%

echo.
echo  [1/5] Building DungeonKit and the console DLLs...
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QTDIR%"
if errorlevel 1 goto fail
cmake --build build
if errorlevel 1 goto fail

rem ---- the version number comes from src\Version.h ----
set VER=
for /f "tokens=3" %%a in ('findstr /c:"DK_APP_VERSION" src\Version.h') do set VER=%%~a
if "%VER%"=="" set VER=dev

set OUT=dist\DungeonKit-%VER%
set ZIP=dist\DungeonKit-%VER%.zip

echo.
echo  [2/5] Making a clean folder: %OUT%
if exist "%OUT%" rmdir /s /q "%OUT%"
if exist "%ZIP%" del /q "%ZIP%"
mkdir "%OUT%"
if errorlevel 1 goto fail

echo.
echo  [3/5] Copying DungeonKit.exe and the Qt files it needs...
copy /y "build\DungeonKit.exe" "%OUT%\" >nul
if errorlevel 1 goto fail
windeployqt --release "%OUT%\DungeonKit.exe"
if errorlevel 1 goto fail

echo.
echo  [4/5] Copying the console DLLs (Probe folder)...
mkdir "%OUT%\Probe"
if not exist "build\Probe\DungeonKitConsole.dll" (
    echo  *** build\Probe\DungeonKitConsole.dll is missing. The Minecraft Dungeons console DLL did not build. ***
    goto fail
)
if not exist "build\Probe\DungeonKitConsole2.dll" (
    echo  *** build\Probe\DungeonKitConsole2.dll is missing. The Minecraft Dungeons II console DLL did not build. ***
    goto fail
)
copy /y "build\Probe\*.dll" "%OUT%\Probe\" >nul
if errorlevel 1 goto fail

rem anything else that sits next to the exe in the build folder (for example a logo.png override) comes along too
if exist "build\logo.png" copy /y "build\logo.png" "%OUT%\" >nul

echo.
echo  [5/5] Zipping it so you can send it...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%OUT%\*' -DestinationPath '%ZIP%' -Force"
if errorlevel 1 goto fail

echo.
echo  ============================================================
echo   PACKAGE OK
echo   Folder: %CD%\%OUT%
echo   Zip   : %CD%\%ZIP%
echo.
echo   Contents:
dir /b "%OUT%\Probe"
echo   (the .dll files above are inside the Probe folder)
echo.
echo   Send the zip. Tell people to unzip it first and run
echo   DungeonKit.exe from the unzipped folder, with the Probe
echo   folder left next to it.
echo  ============================================================
echo.
start "" explorer "%CD%\dist"
pause
exit /b 0

:fail
echo.
echo  *** Something above failed. Copy the error lines and paste them to Claude. ***
echo.
pause
exit /b 1

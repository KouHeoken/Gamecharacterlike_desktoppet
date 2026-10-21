@echo off
rem Build and run Gamecharacterlike_desktoppet (Qt 6.11.0 MinGW 64-bit)
setlocal

set "QT_ROOT=D:\Programing\Qt"
set "QT_DIR=%QT_ROOT%\6.11.0\mingw_64"
set "MINGW_BIN=%QT_ROOT%\Tools\mingw1310_64\bin"
set "CMAKE=%QT_ROOT%\Tools\CMake_64\bin\cmake.exe"

set "SRC=%~dp0"
set "BUILD=%SRC%build"

set "PATH=%MINGW_BIN%;%QT_DIR%\bin;%PATH%"

if /I "%~1"=="run" goto :run
if /I "%~1"=="clean" goto :clean

:build
"%CMAKE%" -S "%SRC%." -B "%BUILD%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
    -DCMAKE_MAKE_PROGRAM="%MINGW_BIN%\mingw32-make.exe" ^
    -DCMAKE_CXX_COMPILER="%MINGW_BIN%\g++.exe" || exit /b 1
"%CMAKE%" --build "%BUILD%" -j 8 || exit /b 1
goto :eof

:run
call "%~f0" build || exit /b 1
start "" "%BUILD%\Gamecharacterlike_desktoppet.exe"
goto :eof

:clean
if exist "%BUILD%" rmdir /s /q "%BUILD%"
echo Cleaned %BUILD%
goto :eof

@echo off
echo FF8 Script Loader - Build Script
echo =================================
echo.

REM Check if vcpkg toolchain is available
if "%VCPKG_ROOT%"=="" (
    echo WARNING: VCPKG_ROOT environment variable not set!
    echo Please set VCPKG_ROOT to your vcpkg installation directory.
    echo Example: set VCPKG_ROOT=C:\vcpkg
    echo.
    set /p VCPKG_ROOT="Enter vcpkg root path (or press Enter to continue without): "
)

REM Set build configuration
set BUILD_TYPE=Release
if "%1"=="debug" set BUILD_TYPE=Debug
if "%1"=="Debug" set BUILD_TYPE=Debug

echo Build Configuration: %BUILD_TYPE%
echo.

REM Create build directory
if not exist build mkdir build
cd build

REM Configure CMake for 32-bit
echo Configuring CMake for 32-bit (Win32)...
if not "%VCPKG_ROOT%"=="" (
    cmake -A Win32 ^
          -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
          -DVCPKG_TARGET_TRIPLET=x32-windows ^
          -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
          ..
) else (
    echo Building without vcpkg toolchain...
    cmake -A Win32 -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ..
)

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed!
    pause
    exit /b 1
)

REM Build the project
echo.
echo Building project...
cmake --build . --config %BUILD_TYPE% --parallel

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)

REM Copy configuration file to output directory
echo.
echo Copying configuration files...
copy "..\config\memory_config.toml" "bin\%BUILD_TYPE%\" >nul 2>&1

echo.
echo ================================
echo Build completed successfully!
echo ================================
echo.
echo Output files:
echo   Injector: build\bin\%BUILD_TYPE%\ff8_injector.exe
echo   Hook DLL: build\bin\%BUILD_TYPE%\ff8_hook.dll
echo   Config:   build\bin\%BUILD_TYPE%\memory_config.toml
echo.
echo To run:
echo   1. Copy ff8_hook.dll and memory_config.toml to your FF8 directory
echo   2. Run ff8_injector.exe from the FF8 directory
echo.
pause 
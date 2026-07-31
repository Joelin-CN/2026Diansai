@echo off
REM 操场路径循迹功能编译测试脚本（Windows版本）
REM 用途：验证新添加的track_path模块能否正确编译

echo ==========================================
echo   Track Path Module Compilation Test
echo ==========================================
echo.

REM 检查构建目录
if not exist "build" (
    echo [INFO] Creating build directory...
    mkdir build
)

cd build

REM 配置CMake
echo [Step 1/3] Configuring CMake...
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed!
    exit /b 1
)

echo.
echo [Step 2/3] Compiling track path module...

REM 只编译新添加的文件以快速验证
mingw32-make Core/Src/app/track_path.c.obj
if %errorlevel% neq 0 (
    echo [ERROR] track_path.c compilation failed!
    exit /b 1
)
echo   √ track_path.c compiled successfully

mingw32-make Core/Src/app/track_control_app.c.obj
if %errorlevel% neq 0 (
    echo [ERROR] track_control_app.c compilation failed!
    exit /b 1
)
echo   √ track_control_app.c compiled successfully

mingw32-make Core/Src/app/track_demo.c.obj
if %errorlevel% neq 0 (
    echo [ERROR] track_demo.c compilation failed!
    exit /b 1
)
echo   √ track_demo.c compiled successfully

echo.
echo [Step 3/3] Building full firmware...
mingw32-make -j4

if %errorlevel% equ 0 (
    echo.
    echo ==========================================
    echo   √ Build Successful!
    echo ==========================================
    echo.
    echo Generated files:
    dir /b v1.0_freeRTOS.elf v1.0_freeRTOS.bin v1.0_freeRTOS.hex 2>nul
    echo.
    echo Next steps:
    echo   1. Flash firmware to STM32F407
    echo   2. See docs\TRACK_PATH_USAGE.md for usage instructions
    echo   3. Place vehicle at track start position (A point^)
    echo   4. Power on and test
    echo.
) else (
    echo.
    echo [ERROR] Full build failed!
    echo Check compilation errors above.
    exit /b 1
)

cd ..

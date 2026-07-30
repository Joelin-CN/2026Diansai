#!/bin/bash
# 操场路径循迹功能编译测试脚本
# 用途：验证新添加的track_path模块能否正确编译

echo "=========================================="
echo "  Track Path Module Compilation Test"
echo "=========================================="
echo ""

# 检查构建目录
if [ ! -d "build" ]; then
    echo "[INFO] Creating build directory..."
    mkdir -p build
fi

cd build

# 配置CMake
echo "[Step 1/3] Configuring CMake..."
cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug

if [ $? -ne 0 ]; then
    echo "[ERROR] CMake configuration failed!"
    exit 1
fi

echo ""
echo "[Step 2/3] Compiling track path module..."

# 只编译新添加的文件以快速验证
make Core/Src/app/track_path.c.obj
if [ $? -ne 0 ]; then
    echo "[ERROR] track_path.c compilation failed!"
    exit 1
fi
echo "  ✓ track_path.c compiled successfully"

make Core/Src/app/track_control_app.c.obj
if [ $? -ne 0 ]; then
    echo "[ERROR] track_control_app.c compilation failed!"
    exit 1
fi
echo "  ✓ track_control_app.c compiled successfully"

make Core/Src/app/track_demo.c.obj
if [ $? -ne 0 ]; then
    echo "[ERROR] track_demo.c compilation failed!"
    exit 1
fi
echo "  ✓ track_demo.c compiled successfully"

echo ""
echo "[Step 3/3] Building full firmware..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo ""
    echo "=========================================="
    echo "  ✓ Build Successful!"
    echo "=========================================="
    echo ""
    echo "Generated files:"
    ls -lh v1.0_freeRTOS.elf v1.0_freeRTOS.bin v1.0_freeRTOS.hex 2>/dev/null || echo "  (firmware files)"
    echo ""
    echo "Next steps:"
    echo "  1. Flash firmware to STM32F407"
    echo "  2. See docs/TRACK_PATH_USAGE.md for usage instructions"
    echo "  3. Place vehicle at track start position (A point)"
    echo "  4. Power on and test"
    echo ""
else
    echo ""
    echo "[ERROR] Full build failed!"
    echo "Check compilation errors above."
    exit 1
fi

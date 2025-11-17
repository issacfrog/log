#!/bin/bash
# 生成 DEB 安装包的脚本
# 基于当前的 CMakeLists.txt，不修改任何源文件

set -e  # 遇到错误立即退出

echo "=========================================="
echo "开始生成 spdlog DEB 安装包"
echo "=========================================="

# 进入 build 目录（如果存在）
if [ -d "build" ]; then
    cd build
    echo "已进入 build 目录"
else
    echo "错误: build 目录不存在，请先编译项目"
    exit 1
fi

# 检查是否已编译
if [ ! -f "libspdlog.so" ] && [ ! -f "libspdlog.so.1.16" ]; then
    echo "警告: 未找到编译的库文件，开始编译..."
    cmake --build . -j$(nproc)
fi

echo ""
echo "配置 CPack 生成 DEB 包..."
echo ""

# 重新配置 CMake，设置 CPack 参数用于生成 DEB 包
cmake .. \
    -DCPACK_GENERATOR=DEB \
    -DCPACK_DEBIAN_PACKAGE_MAINTAINER="Your Name <your.email@example.com>" \
    -DCPACK_DEBIAN_PACKAGE_DEPENDS="libc6 (>= 2.17), libstdc++6 (>= 4.8)" \
    -DCPACK_DEBIAN_PACKAGE_ARCHITECTURE=$(dpkg --print-architecture) \
    -DCPACK_DEBIAN_PACKAGE_SECTION="libs" \
    -DCPACK_DEBIAN_PACKAGE_PRIORITY="optional" \
    -DCPACK_DEBIAN_PACKAGE_NAME="libspdlog1" \
    -DCPACK_DEBIAN_PACKAGE_SHLIBDEPS=ON

echo ""
echo "开始生成 DEB 包..."
echo ""

# 生成 DEB 包
cpack -G DEB

echo ""
echo "=========================================="
echo "DEB 包生成完成！"
echo "=========================================="
echo ""
echo "生成的 DEB 包文件："
ls -lh *.deb 2>/dev/null || echo "未找到 .deb 文件"
echo ""
echo "安装命令："
echo "  sudo dpkg -i *.deb"
echo ""
echo "如果遇到依赖问题，运行："
echo "  sudo apt-get install -f"
echo ""


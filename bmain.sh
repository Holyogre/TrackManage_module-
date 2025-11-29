#!/bin/bash

# 针对WSL与G+=不兼容问题的补丁:
sudo sysctl vm.mmap_rnd_bits=28

# 清理旧构建
rm -rf build
mkdir -p build
# make clean

# 进入构建目录
cd build || exit

# 生成构建配置（Debug模式）
# cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake .. -DCMAKE_BUILD_TYPE=Release 

# 编译主程序和测试
make -j$(nproc)

# 运行测试
# ctest --output-on-failure
ctest -V > report.txt
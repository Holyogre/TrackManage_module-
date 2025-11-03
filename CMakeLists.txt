cmake_minimum_required(VERSION 3.15)
project(Trackermanager LANGUAGES CXX)

# cmake启动测试的指令，如果不用的话请注释
enable_testing()  

# 启动日志宏
add_definitions(-DENABLE_SPDLOG)  # 关键！向编译器传递宏定义

# 基础配置
set(CMAKE_CXX_STANDARD 17)

# 可执行文件配置
file(GLOB SOURCES "src/*.cpp")
file(GLOB MYUTILS "utils/*.cpp")
add_executable(main
    main.cpp
    ${SOURCES}
    ${MYUTILS}
)

# 包含目录
target_include_directories(main PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/utils
)

# 连接日志库
find_package(spdlog REQUIRED)
target_link_libraries(main PRIVATE spdlog::spdlog)

add_subdirectory(tests)
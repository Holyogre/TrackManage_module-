cmake_minimum_required(VERSION 3.16)
project(TrackManager LANGUAGES CXX)

set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
# ==================== 基本配置 ====================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  

# ==================== 配置选项 ====================
# cmake启动测试的指令，如果不用的话请注释
enable_testing()  

# 启动日志宏
add_definitions(-DENABLE_SPDLOG)  

# 指定日志目录（编译期），如果没有该路径则配置阶段直接失败
set(TRACKMANAGER_DEFAULT_LOG_DIR "${PROJECT_SOURCE_DIR}/log")
if(NOT EXISTS ${TRACKMANAGER_DEFAULT_LOG_DIR})
    message(FATAL_ERROR "TRACKMANAGER_DEFAULT_LOG_DIR (${TRACKMANAGER_DEFAULT_LOG_DIR}) 不存在，需要修改")
endif()

# 将默认日志目录注入编译宏，供运行时回退使用
add_compile_definitions(TRACKMANAGER_DEFAULT_LOG_DIR=\"${TRACKMANAGER_DEFAULT_LOG_DIR}\")

# ==================== 查找依赖包 ====================

# 添加线程支持
find_package(Threads REQUIRED)

# 查找 spdlog
find_package(spdlog REQUIRED)

# 查找 OpenCV
find_package(OpenCV REQUIRED)

# ==================== 可执行文件配置 ====================

file(GLOB SOURCES "src/*.cpp")
file(GLOB MYUTILS "utils/*.cpp")
add_executable(main
    main.cpp
    ${SOURCES}
    ${MYUTILS}
)

# 包含目录
target_include_directories(main PUBLIC 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/utils
    ${OpenCV_INCLUDE_DIRS}  # 添加 OpenCV 包含目录
)

# 链接库
target_link_libraries(main PUBLIC 
    spdlog::spdlog 
    Threads::Threads
    "${OpenCV_LIBS}"  # 添加 OpenCV 库，推荐加引号防止变量为空时报错
)




# ==================== 添加编译警告选项 ====================
# if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion")
#     message(STATUS "启用基础警告: -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion")
# endif()
# # ==================== 等级1：更严格警告 ====================
# if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow -Wunused -Wnull-dereference")
#     message(STATUS "启用等级1警告: 变量遮蔽, 未使用, 空指针检测")
# endif()
# # 继续添加等级1的剩余警告：
# if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdouble-promotion -Wold-style-cast -Wcast-align -Woverloaded-virtual")
#     message(STATUS "启用等级1剩余警告: 类型转换安全")
# endif()
# # ==================== 等级2：UB特别检测 ====================
# if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wundef -Wuninitialized -Winit-self -Wstrict-overflow=5")
#     message(STATUS "启用等级2警告: UB检测, 未初始化变量")
# endif()
# # ==================== 等级3：代码质量警告 ====================
# if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast")
#     message(STATUS "启用等级3警告: 代码质量, 重复代码检测")
# endif()


add_subdirectory(tests)


cmake_minimum_required(VERSION 3.15)
project(Trackermanager LANGUAGES CXX)

# 基础配置
set(CMAKE_CXX_STANDARD 17)

# cmake启动测试的指令，如果不用的话请注释
enable_testing()  

# 启动日志宏
add_definitions(-DENABLE_SPDLOG)  # 关键！向编译器传递宏定义


# 指定日志目录（编译期），如果没有该路径则配置阶段直接失败
set(TRACKMANAGER_DEFAULT_LOG_DIR "/home/holyogre/TrackManager/log")
if(NOT EXISTS ${TRACKMANAGER_DEFAULT_LOG_DIR})
    message(FATAL_ERROR "TRACKMANAGER_DEFAULT_LOG_DIR (${TRACKMANAGER_DEFAULT_LOG_DIR}) 不存在，需要修改")
endif()

# 将默认日志目录注入编译宏，供运行时回退使用（优先使用环境变量）
add_compile_definitions(TRACKMANAGER_DEFAULT_LOG_DIR=\"${TRACKMANAGER_DEFAULT_LOG_DIR}\")


# # 添加编译警告选项
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

# 添加线程支持
find_package(Threads REQUIRED)

# 连接日志库
find_package(spdlog REQUIRED)
target_link_libraries(main PRIVATE 
    spdlog::spdlog 
    Threads::Threads
)

add_subdirectory(tests)
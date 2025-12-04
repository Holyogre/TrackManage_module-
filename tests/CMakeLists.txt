find_package(Catch2 REQUIRED CONFIG HINTS "/usr/local/lib/cmake/Catch2") 

# 定义一个函数来简化测试目标的创建
function(add_trackermanager_test TEST_NAME SOURCE_FILE)
    # 捕获额外参数作为编译文件名
    set(IMPL_FILES ${ARGN})

    add_executable(${TEST_NAME}
        ${SOURCE_FILE}
        ${IMPL_FILES}
        ../utils/Logger.cpp  # 添加 Logger 实现
    )

    target_include_directories(${TEST_NAME} PRIVATE 
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/utils  # 添加 utils 包含路径
        ${CMAKE_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}  # 添加 OpenCV 包含目录
    )

    target_link_libraries(${TEST_NAME} PRIVATE 
        Catch2::Catch2WithMain
        spdlog::spdlog  # 链接 spdlog
        "${OpenCV_LIBS}"  # 添加 OpenCV 库，推荐加引号防止变量为空时报错
    )

    # 关键：定义宏
    target_compile_definitions(${TEST_NAME} PRIVATE
        ENABLE_SPDLOG
    )

    # 启用 ASan 
    target_compile_options(${TEST_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${TEST_NAME} PRIVATE -fsanitize=address)

    set_target_properties(${TEST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
endfunction()


# 使用函数添加测试
# 测试太麻烦了，先删除了
# add_trackermanager_test(test_TrackConfig 
#                 test_trackConfig.cpp  
#                 ../include/TrackConfig.hpp)
add_trackermanager_test(test_latestBuffer 
                test_latestBuffer.cpp 
                ../src/LatestKBuffer.hpp)
add_trackermanager_test(test_trackermanager
                test_trackerManager.cpp
                ../src/TrackerManager.cpp)
add_trackermanager_test(test_trackVisualizer
                test_trackVisualizer.cpp            
                ../src/TrackerManager.cpp
                ../src/TrackerVisualizer.cpp
                ../src/TrackerVisualizer.hpp) 

# 注册数据层滚动
# add_test(NAME latestKbuffer功能测试 COMMAND test_latestBuffer "[LatestKBuffer][]")
# add_test(NAME trackermanager测试 COMMAND test_trackermanager "[TrackerManager][]")
# add_test(NAME trackVisualizer基本绘制功能测试 COMMAND test_trackVisualizer "[TrackerVisualizer][draw]")
# add_test(NAME TrackConfig配置加载测试 COMMAND test_TrackConfig "[TrackConfig][thread]")  #测试太麻烦了，先删了
/*****************************************************************************
 * @file test_trackmanager_test.cpp
 * @brief TrackManagerTest 使用示例
 *
 * 演示如何使用 TrackManagerTest 类定期生成虚假航迹并批量发送
 *
 * @version 1.0
 * @date 2025-12-06
 *
 * @copyright Copyright (c) 2025
 *****************************************************************************/

#include "include/TrackManager_TEST.hpp"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <csignal>

std::atomic<bool> running(true);

// 信号处理函数
void signal_handler(int signal)
{
    std::cout << "\n收到信号 " << signal << "，正在停止测试..." << std::endl;
    running = false;
}

int main()
{
    // 注册信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try
    {
        // 创建 ManagementService 实例
        auto service = std::make_shared<track_project::ManagementService>(100, 50);
        std::cout << "ManagementService 创建成功" << std::endl;

        // 等待服务初始化
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 创建测试工具
        auto tester = std::make_unique<track_project::TrackManagerTest>(service);
        std::cout << "\nTrackManagerTest 创建成功" << std::endl;

        // 配置测试参数
        track_project::TrackManagerTest::TestConfig config;
        config.num_tracks = 10;
        config.update_times = 2000; // 更新间隔 2000ms
        config.lon_min = 119.9;
        config.lon_max = 120.1;
        config.lat_min = 29.9;
        config.lat_max = 30.1;
        config.min_speed = 10.0;  // 10 m/s
        config.max_speed = 100.0; // 100 m/s
        config.min_course = 0.0;
        config.max_course = 360.0;
        config.max_accel_sog = 10.0;
        config.max_accel_cog = 1.0;
        config.draw_points = false;

        // 启动测试
        tester->start(config);

        std::cout << "\n测试已启动，正在生成航迹数据..." << std::endl;
        std::cout << "等待测试运行..." << std::endl;

        // 主循环，等待信号
        while (running)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // 每5秒输出一次状态
            static int counter = 0;
            counter++;

            if (counter % 5 == 0)
            {
                std::cout << "状态: 测试运行中..." << std::endl;
            }
        }

        // 停止测试
        std::cout << "\n正在停止测试..." << std::endl;
        tester->stop();

        // 等待一段时间确保资源清理
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::cout << "\n==========================================" << std::endl;
        std::cout << "测试完成" << std::endl;
        std::cout << "==========================================" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

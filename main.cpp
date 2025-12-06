#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>
#include <string_view>
#include "./include/ManagementService.hpp"

static constexpr std::string_view TRACK_PACKET_ID = "TRACK_PACKET";

// 生成测试用的TrackPoint
track_project::TrackPoint create_test_track_point(double base_lon, double base_lat, int index)
{
    track_project::TrackPoint point;
    point.longitude = base_lon + index * 0.001;
    point.latitude = base_lat + index * 0.001;
    point.angle = 45.0 + index * 5.0;
    point.distance = 10.0 + index * 0.5;
    point.sog = 20.0 + index * 1.0;
    point.cog = 90.0 + index * 10.0;
    point.is_associated = true;
    point.time = track_project::Timestamp::now();
    return point;
}

// 测试优先级处理
void test_priority_processing()
{

    std::cout << "=== 测试优先级处理 (MERGE -> CREATE -> ADD) ===" << std::endl;

    // 创建ManagementService实例
    auto service = std::make_unique<track_project::ManagementService>(100, 50);
    std::cout << "ManagementService创建成功" << std::endl;

    // 等待工作线程启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 同时发送多个不同类型的指令（顺序：ADD, CREATE, MERGE）
    std::cout << "\n同时发送指令（故意按ADD、CREATE、MERGE顺序发送）:" << std::endl;

    // 1. 先发送ADD指令
    {
        std::vector<std::pair<track_project::TrackerHeader, track_project::TrackPoint>> add_data;
        track_project::TrackerHeader header;
        header.track_id = 999; // 不存在的ID，用于测试
        header.state = 0;
        track_project::TrackPoint point = create_test_track_point(120.0, 30.0, 1);
        add_data.push_back(std::make_pair(header, point));

        service->add_track_command(add_data);
        std::cout << "已发送ADD指令" << std::endl;
    }

    // 短暂延迟
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 2. 发送CREATE指令
    {
        std::vector<std::array<track_project::TrackPoint, 4>> create_data;
        std::array<track_project::TrackPoint, 4> track_array;
        for (int j = 0; j < 4; ++j)
        {
            track_array[j] = create_test_track_point(120.0, 30.0, j);
        }
        create_data.push_back(track_array);

        service->create_track_command(create_data);
        std::cout << "已发送CREATE指令" << std::endl;
    }

    // 短暂延迟
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 3. 发送MERGE指令
    {
        service->merge_command(100, 200); // 不存在的ID，用于测试
        std::cout << "已发送MERGE指令" << std::endl;
    }

    std::cout << "\n指令发送顺序: ADD -> CREATE -> MERGE" << std::endl;
    std::cout << "期望处理顺序: MERGE -> CREATE -> ADD (按照优先级)" << std::endl;

    // 等待所有指令处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // 发送清空指令
    service->clear_all_command();

    // 等待清理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "\n=== 优先级测试完成 ===" << std::endl;
}

// 测试点迹绘制功能
void test_point_cloud_drawing()
{
    std::cout << "\n=== 测试点迹绘制功能 ===" << std::endl;

    // 创建ManagementService实例
    auto service = std::make_unique<track_project::ManagementService>(100, 50);
    std::cout << "ManagementService创建成功" << std::endl;

    // 等待工作线程启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 创建测试点迹数据
    std::vector<track_project::TrackPoint> point_cloud;

    // 创建一些测试点迹
    for (int i = 0; i < 20; ++i)
    {
        track_project::TrackPoint point;
        // 在可视范围内生成点迹 (119.0-121.0, 29.0-31.0)
        point.longitude = 119.5 + (i % 10) * 0.15; // 119.5-121.0
        point.latitude = 29.5 + (i / 10) * 0.15;   // 29.5-30.5
        point.angle = 45.0 + i * 5.0;
        point.distance = 10.0 + i * 0.5;
        point.sog = 5.0 + i * 1.0;          // 速度大于0.1，会绘制方向线
        point.cog = 30.0 + i * 15.0;        // 航向
        point.is_associated = (i % 3 == 0); // 每3个点有一个是已关联的
        point.time = track_project::Timestamp::now();

        point_cloud.push_back(point);

        std::cout << "创建点迹 " << i << ": 位置("
                  << std::fixed << std::setprecision(4) << point.longitude << ", " << point.latitude
                  << "), 关联状态: " << (point.is_associated ? "已关联" : "未关联")
                  << ", 速度: " << point.sog << " m/s" << std::endl;
    }

    std::cout << "\n总共创建 " << point_cloud.size() << " 个测试点迹" << std::endl;
    std::cout << "其中 " << std::count_if(point_cloud.begin(), point_cloud.end(), [](const auto &p)
                                          { return p.is_associated; })
              << " 个点迹已关联（应显示为蓝色）" << std::endl;
    std::cout << "其中 " << std::count_if(point_cloud.begin(), point_cloud.end(), [](const auto &p)
                                          { return !p.is_associated; })
              << " 个点迹未关联（应显示为红色）" << std::endl;

    // 调用点迹绘制函数
    std::cout << "\n调用 draw_point_command 绘制点迹..." << std::endl;
    service->draw_point_command(point_cloud);

    // 等待绘制完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 创建一些航迹来测试点迹背景与航迹的叠加
    std::cout << "\n创建测试航迹以验证点迹背景..." << std::endl;
    {
        std::vector<std::array<track_project::TrackPoint, 4>> create_data;
        std::array<track_project::TrackPoint, 4> track_array;
        for (int j = 0; j < 4; ++j)
        {
            track_array[j] = create_test_track_point(120.0, 30.0, j);
        }
        create_data.push_back(track_array);

        service->create_track_command(create_data);
        std::cout << "已发送CREATE指令创建测试航迹" << std::endl;
    }

    // 等待航迹绘制（draw_track会显示点迹背景）
    std::cout << "\n等待航迹绘制（将显示点迹背景）..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // 清空
    service->clear_all_command();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "\n=== 点迹绘制测试完成 ===" << std::endl;
    std::cout << "注意：如果OpenCV窗口打开，您应该能看到：" << std::endl;
    std::cout << "1. 蓝色点：已关联的点迹" << std::endl;
    std::cout << "2. 红色点：未关联的点迹" << std::endl;
    std::cout << "3. 方向线：速度大于0.1m/s的点迹会有方向指示" << std::endl;
    std::cout << "4. 航迹线：黑色的航迹线叠加在点迹背景上" << std::endl;
}

// 测试多线程并发
void test_concurrent_commands()
{
    std::cout << "\n=== 测试多线程并发指令 ===" << std::endl;

    auto service = std::make_unique<track_project::ManagementService>(200, 100);
    std::cout << "ManagementService创建成功" << std::endl;

    // 等待工作线程启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 创建多个线程同时发送指令
    const int num_threads = 5;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([i, &service]()
                             {
            // 每个线程发送混合类型的指令
            for (int j = 0; j < 3; ++j)
            {
                // 随机选择指令类型
                int cmd_type = rand() % 3;
                
                switch (cmd_type)
                {
                case 0: // MERGE
                    service->merge_command(i * 10 + j, i * 10 + j + 1);
                    break;
                    
                case 1: // CREATE
                {
                    std::vector<std::array<track_project::TrackPoint, 4>> create_data;
                    std::array<track_project::TrackPoint, 4> track_array;
                    for (int k = 0; k < 4; ++k)
                    {
                        track_project::TrackPoint point;
                        point.longitude = 120.0 + i * 0.1;
                        point.latitude = 30.0 + i * 0.1;
                        point.angle = 45.0 + k * 5.0;
                        point.distance = 10.0 + k * 0.5;
                        point.sog = 20.0 + k * 1.0;
                        point.cog = 90.0 + k * 10.0;
                        point.is_associated = true;
                        point.time = track_project::Timestamp::now();
                        track_array[k] = point;
                    }
                    create_data.push_back(track_array);
                    service->create_track_command(create_data);
                    break;
                }
                    
                case 2: // ADD
                {
                    std::vector<std::pair<track_project::TrackerHeader, track_project::TrackPoint>> add_data;
                    track_project::TrackerHeader header;
                    header.track_id = i * 10 + j;
                    header.state = 0;
                    track_project::TrackPoint point = create_test_track_point(120.0, 30.0, j);
                    add_data.push_back(std::make_pair(header, point));
                    service->add_track_command(add_data);
                    break;
                }
                }
                
                // 短暂延迟
                std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 50));
            } });
    }

    // 等待所有线程完成
    for (auto &thread : threads)
    {
        thread.join();
    }

    std::cout << "所有并发指令已发送" << std::endl;

    // 等待指令处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // 获取统计信息
    std::cout << "\n--- 并发测试统计信息 ---" << std::endl;
    const auto &manager = service->get_tracker_manager();
    std::cout << "总容量: " << manager.get_total_capacity() << std::endl;
    std::cout << "已使用: " << manager.get_used_count() << std::endl;
    std::cout << "下一个航迹ID: " << manager.get_next_track_id() << std::endl;

    // 清空
    service->clear_all_command();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "\n=== 并发测试完成 ===" << std::endl;
}

int main()
{
    srand(static_cast<unsigned>(time(nullptr))); // 设置随机种子
    std::cout << "Tracker Management Service 高级测试 " << TRACK_PACKET_ID << std::endl;

    try
    {
        // 测试点迹绘制功能
        test_point_cloud_drawing();

        // 测试优先级处理
        // test_priority_processing();

        // 测试并发处理
        // test_concurrent_commands();

        std::cout << "\n所有测试完成！" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n测试程序执行完毕，等待2秒确保所有资源清理..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    return 0;
}

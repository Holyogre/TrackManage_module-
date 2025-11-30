#include <catch2/catch_all.hpp>
#include "../src/TrackerManager.hpp"
#include "TrackerVisualizer.hpp"
#include <cstring>
#include <iostream>
#include <chrono>
#include "../utils/Logger.hpp"
#include <numeric>
#include <thread>

using namespace track_project::trackermanager;
using namespace track_project::commondata;
using namespace track_project::communicate;

TEST_CASE("TrackerVisualizer 基本绘制功能", "[TrackerVisualizer][draw]")
{
    // 设置经纬度范围（北京大致范围）
    double lon_min = 116.0;
    double lon_max = 117.0;
    double lat_min = 39.0;
    double lat_max = 40.0;

    // 创建可视化器和航迹管理器
    track_project::trackermanager::TrackerVisualizer visualizer(lon_min, lon_max, lat_min, lat_max);
    track_project::trackermanager::TrackerManager manager(10, 100); // 10条航迹，每航迹100点容量

    SECTION("创建单条航迹并绘制")
    {
        // 创建一条航迹
        std::uint32_t track_id = manager.createTrack();
        REQUIRE(manager.isValidTrack(track_id) == true);

        // 添加几个测试点
        for (int i = 0; i < 100; i++)
        {
            TrackPoint point;
            point.longitude = 116.1 + i * 0.005; // 向东移动
            point.latitude = 39.1 + i * 0.005;  // 向北移动
            point.is_associated = true;
            point.time.now();

            bool result = manager.push_track_point(track_id, point);
            REQUIRE(result == true);
        }

        // 测试绘制功能
        REQUIRE_NOTHROW(visualizer.draw_track(manager));

        // 测试状态打印功能
        REQUIRE_NOTHROW(visualizer.printFullState(manager));

        LOG_INFO << "单条航迹绘制测试完成，窗口将显示5秒，按任意键继续...";

        // 等待用户按键或超时
        cv::waitKey(5000); // 显示5秒
    }
}

TEST_CASE("TrackerVisualizer 边界情况测试", "[TrackerVisualizer][edge]")
{
    // 使用较小的经纬度范围测试边界
    track_project::trackermanager::TrackerVisualizer visualizer(116.3, 116.4, 39.8, 39.9);
    track_project::trackermanager::TrackerManager manager(5, 50);

    SECTION("边界点测试")
    {
        std::uint32_t track_id = manager.createTrack();

        // 测试边界点
        TrackPoint edge_point;
        edge_point.longitude = 116.3; // 最小经度
        edge_point.latitude = 39.8;   // 最小纬度
        edge_point.is_associated = true;
        edge_point.time.now();
        REQUIRE(manager.push_track_point(track_id, edge_point) == true);

        edge_point.longitude = 116.4; // 最大经度
        edge_point.latitude = 39.9;   // 最大纬度
        REQUIRE(manager.push_track_point(track_id, edge_point) == true);

        // 应该能正常绘制边界点
        REQUIRE_NOTHROW(visualizer.draw_track(manager));

        LOG_INFO << "边界点绘制测试完成";
        // 等待用户按键或超时
        cv::waitKey(5000); // 显示5秒
    }
}

TEST_CASE("TrackerManager 可视化绘制性能测试", "[TrackerVisualizer][benchmark]")
{
    constexpr std::uint32_t TRACK_COUNT = 2000;
    constexpr std::uint32_t POINTS_PER_TRACK = 2000;

    // 设置经纬度范围
    double lon_min = 116.0;
    double lon_max = 117.0;
    double lat_min = 39.0;
    double lat_max = 40.0;

    // 统一的测试数据模板
    TrackPoint base_point;
    base_point.longitude = 116.3974;
    base_point.latitude = 39.9093;
    base_point.sog = 10.5;
    base_point.cog = 45.0;
    base_point.angle = 30.0;
    base_point.distance = 1000.0;
    base_point.is_associated = true;
    base_point.time = Timestamp::now();

    SECTION("GUI刷新性能测试")
    {
        std::vector<std::tuple<int, int, std::string>> test_cases = {
            {500, 100, "5万点"},
            {500, 500, "25万点"},
            {500, 1000, "50万点"},
            {1000, 100, "10万点"},
            {1000, 500, "50万点"},
            {1000, 1000, "100万点"},
            {1500, 100, "15万点"},
            {1500, 500, "75万点"},
            {1500, 1000, "150万点"},
        };

        for (const auto &[tracks, points, description] : test_cases)
        {
            LOG_INFO << "\n"
                     << std::string(50, '=') << std::endl;
            LOG_INFO << "测试案例: " << description << std::endl;
            LOG_INFO << "规模: " << tracks << " 航迹 × " << points << " 点/航迹" << std::endl;
            LOG_INFO << "总点数: " << tracks * points << std::endl;
            LOG_INFO << std::string(50, '=') << std::endl;

            // 预先准备数据
            TrackerManager scale_manager(tracks, points);
            std::vector<std::uint32_t> scale_track_ids;

            // 填充数据...
            for (int i = 0; i < tracks; ++i)
            {
                std::uint32_t id = scale_manager.createTrack();
                if (id != 0)
                {
                    scale_track_ids.push_back(id);
                    TrackPoint track_point = base_point;
                    track_point.longitude = 116.1 + (id % 50) * 0.015;
                    track_point.latitude = 39.1 + (id % 40) * 0.02;

                    for (int j = 0; j < points; ++j)
                    {
                        track_point.longitude += 0.0002;
                        track_point.latitude += 0.0001;
                        scale_manager.push_track_point(id, track_point);
                    }
                }
            }

            // 性能测试参数
            constexpr int NUM_RUNS = 20;
            std::vector<double> run_times;
            run_times.reserve(NUM_RUNS);

            LOG_INFO << "开始性能测试 (" << NUM_RUNS << " 次运行)..." << std::endl;

            // 预热运行（创建第一个visualizer并预热）
            {
                TrackerVisualizer warmup_visualizer(lon_min, lon_max, lat_min, lat_max);
                warmup_visualizer.draw_track(scale_manager);
                cv::waitKey(10);
                // warmup_visualizer 在这里析构，窗口关闭
            }

            // 正式测试运行
            for (int i = 0; i < NUM_RUNS; ++i)
            {
                double time_ms = 0.0;

                // 在计时开始前创建visualizer
                TrackerVisualizer visualizer(lon_min, lon_max, lat_min, lat_max);

                auto start = std::chrono::high_resolution_clock::now();

                visualizer.draw_track(scale_manager);

                auto end = std::chrono::high_resolution_clock::now();

                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                time_ms = duration.count() / 1000.0;

                run_times.push_back(time_ms);

                // 短暂延迟，避免系统过载
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // 计算统计信息
            double sum = std::accumulate(run_times.begin(), run_times.end(), 0.0);
            double mean = sum / NUM_RUNS;

            double variance = 0.0;
            for (double time : run_times)
            {
                variance += (time - mean) * (time - mean);
            }
            variance /= NUM_RUNS;
            double std_dev = std::sqrt(variance);

            // 找到最小和最大值
            double min_time = *std::min_element(run_times.begin(), run_times.end());
            double max_time = *std::max_element(run_times.begin(), run_times.end());

            // 输出统计结果
            LOG_INFO << "\n*** 性能统计结果 ***" << std::endl;
            LOG_INFO << "平均时间: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
            LOG_INFO << "标准差:   " << std::fixed << std::setprecision(3) << std_dev << " ms" << std::endl;
            LOG_INFO << "最小值:   " << std::fixed << std::setprecision(3) << min_time << " ms" << std::endl;
            LOG_INFO << "最大值:   " << std::fixed << std::setprecision(3) << max_time << " ms" << std::endl;
            LOG_INFO << "刷新频率: " << std::fixed << std::setprecision(1) << (1000.0 / mean) << " FPS" << std::endl;

            // 验证数据完整性
            REQUIRE(scale_manager.getUsedCount() == scale_track_ids.size());
        }
    }
}
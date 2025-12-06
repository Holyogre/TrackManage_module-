/*****************************************************************************
 * @file TrackManager_TEST.hpp
 * @brief TrackManager 测试工具 - 定期生成虚假航迹并批量发送
 *
 * 主要功能：
 * 1. start时候生成航迹
 * 2. 定期依据航迹运行状态更新航迹
 * 3. 依据数据特性发送给ManagementService
 * 4. 停机后删除内部航迹
 *
 * @version 1.0
 * @date 2025-12-06
 *
 * @copyright Copyright (c) 2025
 *****************************************************************************/

#ifndef _TRACKMANAGER_TEST_HPP_
#define _TRACKMANAGER_TEST_HPP_

#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <random>
#include <chrono>
#include <functional>
#include <iostream>
#include <iomanip>

#include "ManagementService.hpp"

namespace track_project
{

    /**
     * @class TrackManagerTest
     * @brief TrackManager 测试工具类
     *
     * 用于定期生成虚假航迹数据并通过 ManagementService 批量发送，
     * 支持配置航迹数量、更新频率、速度范围等参数。
     */
    class TrackManagerTest
    {
    public:
        /*****************************************************************************
         * @brief 测试配置参数结构体
         *****************************************************************************/
        struct TestConfig
        {
            // 航迹参数
            std::uint32_t num_tracks = 30; // 航迹数量

            // 更新时间
            std::uint32_t update_times = 1000; // 更新时间m/s

            // 位置范围（经纬度）
            double lon_min = 119.9; // 经度最小值
            double lon_max = 120.1; // 经度最大值
            double lat_min = 29.9;  // 纬度最小值
            double lat_max = 30.1;  // 纬度最大值

            // 速度范围
            double min_speed = 5.0;  // 最小速度（m/s）
            double max_speed = 50.0; // 最大速度（m/s）

            // 航向范围（北偏东角度，0-360度）
            double min_course = 0.0;   // 最小航向
            double max_course = 360.0; // 最大航向

            // 加速度，
            double max_accel_sog = 10;
            double max_accel_cog = 1;

            // 输出控制
            bool draw_points = true; // 是否绘制额外点迹
        };

        /*****************************************************************************
         * @brief 构造函数
         *
         * @param service ManagementService 实例引用
         * @param config 测试配置参数
         *****************************************************************************/
        TrackManagerTest(std::shared_ptr<ManagementService> service);

        /*****************************************************************************
         * @brief 析构函数，停止测试线程
         *****************************************************************************/
        ~TrackManagerTest();

        // 禁止拷贝和移动
        TrackManagerTest(const TrackManagerTest &) = delete;
        TrackManagerTest &operator=(const TrackManagerTest &) = delete;
        TrackManagerTest(TrackManagerTest &&) = delete;
        TrackManagerTest &operator=(TrackManagerTest &&) = delete;

        /*****************************************************************************
         * @brief 启动测试
         * TODO 创建航迹，存放到std::vector<std::pair<TrackerHeader, TrackPoint>> _point;
         * TODO 向Service发送创建航迹指令
         * TODO 进入工作循环
         * @param update_times 更新间隔
         *****************************************************************************/
        void start(const TestConfig &config);

        /*****************************************************************************
         * @brief 停止测试
         * TODO 向Service发送clear指令
         * TODO 清空内部所有航迹
         *****************************************************************************/
        void stop();

    private:
        /*****************************************************************************
         * @brief 循环线程
         * TODO update_tracks，更新点迹
         * TOOD 依据CONFIG，添加随机点迹（未关联），发送DRAWCOMMAND
         * TODO 打包成关联航迹格式，发送ADDCOMMAND
         *****************************************************************************/
        void test_thread_func(std::uint32_t duration_ms);

        /*****************************************************************************
         * @brief 创建航迹
         * TODO 更新共四个点
         * TODO 向Service发送创建航迹指令
         *****************************************************************************/
        void generate_tracks();

        /*****************************************************************************
         * @brief 更新现有航迹状态
         * TODO 依据现有航迹航向航速经纬度，给航向航速增加一个轻微波动（按加速度添加）后，外推一个点
         *****************************************************************************/
        void udpate_tracks();

        /*****************************************************************************
         * @brief 在范围内生成随机 double 值
         *
         * @param min 最小值
         * @param max 最大值
         * @return double 随机值
         *****************************************************************************/
        double random_double(double min, double max);

        /*****************************************************************************
         * @brief 在范围内生成随机整数
         *
         * @param min 最小值
         * @param max 最大值
         * @return std::uint32_t 随机整数
         *****************************************************************************/
        std::uint32_t random_int(std::uint32_t min, std::uint32_t max);

    private:
        // ManagementService 实例
        std::shared_ptr<ManagementService> service_;

        // 测试配置
        TestConfig config_;

        // 线程控制
        std::thread test_thread_;

        // 记录运行批次数
        std::atomic<std::uint32_t> batch_count_;

        // 随机数生成器
        std::random_device rd_;
        std::mt19937 gen_;
        std::uniform_real_distribution<> real_dist_;

        // 点迹
        std::vector<std::pair<TrackerHeader, TrackPoint>> _point;
    };

} // namespace track_project

#endif // _TRACKMANAGER_TEST_HPP_

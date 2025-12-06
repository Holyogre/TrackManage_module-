/*****************************************************************************
 * @file TrackManager_TEST.cpp
 * @brief TrackManager 测试工具 - 实现文件
 *
 * @version 1.0
 * @date 2025-12-06
 *
 * @copyright Copyright (c) 2025
 *****************************************************************************/

#include "../include/TrackManager_TEST.hpp"
#include <cmath>
#include <algorithm>

namespace track_project
{

    /*****************************************************************************
     * @brief 构造函数
     *
     * @param service ManagementService 实例引用
     *****************************************************************************/
    TrackManagerTest::TrackManagerTest(std::shared_ptr<ManagementService> service)
        : service_(service),
          batch_count_(0),
          gen_(rd_()),
          real_dist_(0.0, 1.0)
    {
        std::cout << "TrackManagerTest: 初始化完成" << std::endl;
    }

    /*****************************************************************************
     * @brief 析构函数，停止测试线程
     *****************************************************************************/
    TrackManagerTest::~TrackManagerTest()
    {
        stop();
        std::cout << "TrackManagerTest: 析构完成" << std::endl;
    }

    /*****************************************************************************
     * @brief 启动测试
     *
     * @param config 测试配置参数
     *****************************************************************************/
    void TrackManagerTest::start(const TestConfig &config)
    {
        config_ = config;

        std::cout << "TrackManagerTest: 启动测试" << std::endl;
        std::cout << "  航迹数量: " << config_.num_tracks << std::endl;
        std::cout << "  更新间隔: " << config_.update_times << " ms" << std::endl;
        std::cout << "  位置范围: 经度[" << config_.lon_min << ", " << config_.lon_max
                  << "], 纬度[" << config_.lat_min << ", " << config_.lat_max << "]" << std::endl;

        // TODO 创建航迹，存放到std::vector<std::pair<TrackerHeader, TrackPoint>> _point;
        generate_tracks();

        // TODO 向Service发送创建航迹指令
        // 创建航迹指令需要 std::vector<std::array<TrackPoint, 4>> 格式
        // 我们需要基于每个航迹的起点外推4个点
        std::vector<std::array<TrackPoint, 4>> create_data;
        create_data.reserve(config_.num_tracks);

        // 外推四次，直线外推即可
        for (std::uint32_t i = 0; i < config_.num_tracks; ++i)
        {
            std::array<TrackPoint, 4> track_array;
            
            // 获取航迹起点
            const auto &start_pair = _point[i];
            const TrackerHeader &header = start_pair.first;
            const TrackPoint &start_point = start_pair.second;
            
            // 第一个点就是起点
            track_array[0] = start_point;
            
            // 外推后续3个点
            for (std::uint32_t j = 1; j < 4; ++j)
            {
                TrackPoint extrapolated_point = track_array[j - 1];
                
                // 直线外推：基于前一个点的速度和航向
                double time_interval = 1.0; // 假设1秒间隔
                double distance = extrapolated_point.sog * time_interval / 111000.0; // 转换为度（粗略）
                double rad_course = extrapolated_point.cog * M_PI / 180.0;
                
                extrapolated_point.longitude += distance * std::sin(rad_course);
                extrapolated_point.latitude += distance * std::cos(rad_course);
                
                // 保持其他参数一致
                extrapolated_point.time = Timestamp::now();
                
                track_array[j] = extrapolated_point;
            }
            
            create_data.push_back(track_array);
        }

        if (!create_data.empty())
        {
            service_->create_track_command(create_data);
            std::cout << "TrackManagerTest: 已发送 " << create_data.size() << " 条航迹创建指令" << std::endl;
        }

        // TODO 进入工作循环
        test_thread_ = std::thread(&TrackManagerTest::test_thread_func, this, config_.update_times);

        std::cout << "TrackManagerTest: 测试已启动，工作线程运行中" << std::endl;
    }

    /*****************************************************************************
     * @brief 停止测试
     *****************************************************************************/
    void TrackManagerTest::stop()
    {
        // TODO 向Service发送clear指令
        service_->clear_all_command();
        std::cout << "TrackManagerTest: 已发送清空指令" << std::endl;

        // TODO 清空内部所有航迹
        _point.clear();
        std::cout << "TrackManagerTest: 已清空内部航迹数据" << std::endl;

        // 等待线程结束
        if (test_thread_.joinable())
        {
            test_thread_.join();
            std::cout << "TrackManagerTest: 工作线程已停止" << std::endl;
        }

        std::cout << "TrackManagerTest: 测试已停止，总共运行 " << batch_count_ << " 批次" << std::endl;
    }

    /*****************************************************************************
     * @brief 循环线程
     *
     * @param update_interval_ms 更新间隔（毫秒）
     *****************************************************************************/
    void TrackManagerTest::test_thread_func(std::uint32_t update_interval_ms)
    {
        std::cout << "TrackManagerTest: 工作线程开始运行" << std::endl;

        while (true)
        {
            // update_tracks，更新点迹
            udpate_tracks();

            // 依据CONFIG，添加随机点迹（未关联），发送DRAWCOMMAND
            if (config_.draw_points)
            {
                std::vector<TrackPoint> random_points;
                std::uint32_t num_random_points = random_int(5, 15);

                for (std::uint32_t i = 0; i < num_random_points; ++i)
                {
                    TrackPoint point;
                    point.longitude = random_double(config_.lon_min, config_.lon_max);
                    point.latitude = random_double(config_.lat_min, config_.lat_max);
                    point.sog = random_double(config_.min_speed, config_.max_speed);
                    point.cog = random_double(config_.min_course, config_.max_course);
                    point.is_associated = false; // 未关联的点迹
                    point.time = Timestamp::now();

                    random_points.push_back(point);
                }

                if (!random_points.empty())
                {
                    service_->draw_point_command(random_points);
                }
            }

            // 打包成关联航迹格式，发送ADDCOMMAND
            if (!_point.empty())
            {
                service_->add_track_command(_point);
            }

            // 更新批次计数
            batch_count_++;

            // 每10批次输出一次状态
            if (batch_count_ % 10 == 0)
            {
                std::cout << "TrackManagerTest: 已运行 " << batch_count_ << " 批次，当前点迹数: "
                          << _point.size() << std::endl;
            }

            // 等待下一个更新周期
            std::this_thread::sleep_for(std::chrono::milliseconds(update_interval_ms));
        }

        std::cout << "TrackManagerTest: 工作线程结束运行" << std::endl;
    }

    /*****************************************************************************
     * @brief 创建航迹
     *****************************************************************************/
    void TrackManagerTest::generate_tracks()
    {
        _point.clear();
        _point.reserve(config_.num_tracks); // 每条航迹4个点

        for (std::uint32_t track_id = 1; track_id <= config_.num_tracks; ++track_id)
        {
            // 创建航迹头
            TrackerHeader header;
            header.track_id = track_id; // 其他几个用随机值即可

            // 生成航迹起点
            TrackPoint point;
            point.longitude = random_double(config_.lon_min, config_.lon_max);
            point.latitude = random_double(config_.lat_min, config_.lat_max);
            point.sog = random_double(config_.min_speed, config_.max_speed);
            point.cog = random_double(config_.min_course, config_.max_course);

            _point.push_back({header, point});
        }
    }

    /*****************************************************************************
     * @brief 更新现有航迹状态
     *****************************************************************************/
    void TrackManagerTest::udpate_tracks()
    {

        for (auto &pair : _point)
        {
            TrackerHeader &header = pair.first;
            TrackPoint &point = pair.second;

            // 增加航速波动（按加速度）
            double sog_accel = random_double(-config_.max_accel_sog, config_.max_accel_sog);
            point.sog += sog_accel;
            point.sog = std::max(config_.min_speed, std::min(config_.max_speed, point.sog));

            // 增加航向波动（按加速度）
            double cog_accel = random_double(-config_.max_accel_cog, config_.max_accel_cog);
            point.cog += cog_accel;
            if (point.cog < 0)
                point.cog += 360.0;
            if (point.cog >= 360.0)
                point.cog -= 360.0;

            // 外推位置（假设1秒时间间隔）
            double time_interval = 1.0;
            double distance = point.sog * time_interval / 111000.0;
            double rad_course = point.cog * M_PI / 180.0;

            point.longitude += distance * std::sin(rad_course);
            point.latitude += distance * std::cos(rad_course);

            // 确保位置在范围内
            point.longitude = std::max(config_.lon_min, std::min(config_.lon_max, point.longitude));
            point.latitude = std::max(config_.lat_min, std::min(config_.lat_max, point.latitude));

            // 更新点编号和时间戳
            header.point_num++;
            point.time = Timestamp::now();
        }
    }

    /*****************************************************************************
     * @brief 在范围内生成随机 double 值
     *
     * @param min 最小值
     * @param max 最大值
     * @return double 随机值
     *****************************************************************************/
    double TrackManagerTest::random_double(double min, double max)
    {
        return min + (max - min) * real_dist_(gen_);
    }

    /*****************************************************************************
     * @brief 在范围内生成随机整数
     *
     * @param min 最小值
     * @param max 最大值
     * @return std::uint32_t 随机整数
     *****************************************************************************/
    std::uint32_t TrackManagerTest::random_int(std::uint32_t min, std::uint32_t max)
    {
        std::uniform_int_distribution<std::uint32_t> dist(min, max);
        return dist(gen_);
    }

} // namespace track_project

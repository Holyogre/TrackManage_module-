/*****************************************************************************
 * @file defstruct.h
 * @author xjl (xjl20011009@126.com)
 * @brief 该库用于管理对外输出的数据的结构，外界通过唯一函数接口修改里面的参数
 * @version 0.1
 * @date 2025-11-03
 *
 * @copyright Copyright (c) 2025
 *****************************************************************************/
#ifndef DEFSTRUCT_H
#define DEFSTRUCT_H

// 基础结构
#include <vector>
#include <string>
// 输出格式控制
#include <iostream>
#include <iomanip>
// 结构体控制
#include <type_traits>
// 时间戳
#include <ctime>
#include <chrono>

namespace commonData
{
    // 时间戳结构（毫秒精度）
    struct Timestamp
    {
        int64_t milliseconds; // 从1970-01-01开始的毫秒数

        Timestamp()
        {
            auto now = std::chrono::system_clock::now();
            milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now.time_since_epoch())
                               .count();
        }

        // 静态函数：获取当前时间戳
        static Timestamp now()
        {
            return Timestamp(); // 直接调用构造函数创建当前时间戳
        }

        // 重载 << 操作符用于调试输出
        friend std::ostream &operator<<(std::ostream &os, const Timestamp &ts)
        {
            time_t seconds = ts.milliseconds / 1000;
            int ms = ts.milliseconds % 1000;

            std::tm tm_info = *std::localtime(&seconds);
            os << std::put_time(&tm_info, "%Y-%m-%d %H:%M:%S")
               << "." << std::setfill('0') << std::setw(3) << ms;
            return os;
        }
    };

    /*****************************************************************************
     * @brief 输入结构部分（通讯层负责数据转换）
     *****************************************************************************/
    // TODO检测点迹头预留接口
    struct DetectedPointHeader
    {
    };

    // TODO预留给检测点迹的
    struct DetectedPoint
    {
        int todo;
    };

    /*****************************************************************************
     * @brief 中间结构部分
     *****************************************************************************/
    // TODO预留给关联点迹的，我的想法是关联点往后放，没关联的点往前放
    struct AssociatedPoint
    {
        int id;
        // 其他参数
    };

    //  TODO 预留给新航迹
    struct NewTrack
    {
        bool isAis;
        AssociatedPoint associated_point[4];
    };

    // 卡尔曼滤波点迹
    struct PredictedPoint
    {
    };

    // 现有航迹结构体
    struct ExistTrack
    {
    };

    // 缓冲区定义
    struct TrackingBuffer
    {
        // 未确定部分
        DetectedPointHeader detected_head;
        DetectedPoint *detected_point; // 发过来的数据包直接存储在缓冲区里面

        // 中间结构，不确定
        std::vector<AssociatedPoint> associated_point;
        std::vector<NewTrack> new_track;

        // 确定部分
        std::vector<PredictedPoint> predicted_point; // 预测结构
        std::vector<ExistTrack> existed_point;
    };

    /*****************************************************************************
     * @brief 输出结构部分
     *****************************************************************************/
    // 航迹头，请确保数据是完全自包含的数据块
    struct TrackerHeader
    {
        int id = 0;
        int extrapolation_count = 0;
        int point_size = 0;
        int state = 3; // 0表示航迹正常，1表示航迹外推，2表示航迹终结

        void start(int track_id)
        {
            id = track_id;
            extrapolation_count = 0;
            point_size = 0;
            int state = 3;
        };

        void clear()
        {
            id = 0;
            extrapolation_count = 0;
            point_size = 0;
            int state = 3;
        };

        friend std::ostream &operator<<(std::ostream &os, const TrackerHeader &header)
        {
            os << "TrackerHeader [";
            os << "id=" << header.id << ", ";
            os << "extrapolation_count=" << header.extrapolation_count;
            os << "]";

            return os;
        }
    };

    // 航迹点标准结构
    struct TrackPoint
    {
        double longitude;
        double latitude;
        double sog;      // 对地速度
        double cog;      // 对地航向
        double angle;    // 雷达观测角度
        double distance; // 雷达观测距离

        bool is_associated; // 是否关联
        Timestamp time;

        // 重载 << 操作符用于调试输出
        friend std::ostream &operator<<(std::ostream &os, const TrackPoint &point)
        {
            os << "TrackPoint{"
               << "lon:" << std::fixed << std::setprecision(6) << point.longitude
               << ", lat:" << point.latitude
               << ", sog:" << std::setprecision(1) << point.sog
               << ", cog:" << point.cog
               << ", time:" << point.time
               << "}";
            return os;
        }
    };

    // TODO约束,让DEEPSEEK生成下得了
    static_assert(std::is_trivially_copyable_v<TrackPoint>,
                  "TrackPoint 不是平凡的");
    static_assert(std::is_standard_layout_v<TrackPoint>,
                  "TrackPoint 不是内存连续的");
    static_assert(std::is_trivially_copyable_v<TrackerHeader>,
                  "TrackerHeader 不是平凡的");
    static_assert(std::is_standard_layout_v<TrackerHeader>,
                  "TrackerHeader 不是内存连续的");

} // namespace CommonData

#endif
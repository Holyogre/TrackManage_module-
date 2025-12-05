/*****************************************************************************
 * @file defstruct.h
 * @author xjl (xjl20011009@126.com)
 *
 * @brief 包含三个部分
 * 1. 通用结构体
 * 2，流水线架构专用结构体
 * 3.通信模块结构体
 *
 * @version 0.1
 * @date 2025-11-03
 *
 * @copyright Copyright (c) 2025
 *****************************************************************************/
#ifndef _DEF_TRACK_STRUCT_H_
#define _DEF_TRACK_STRUCT_H_

// 基础结构
#include <vector>
#include <string>
#include <cstdint> //精确位数整数
// 文件控制
#include <fstream>
// 输出格式控制
#include <iostream>
#include <iomanip>
// 时间戳
#include <ctime>
#include <chrono>
// 结构体控制
#include <type_traits>
// 断言
#include <assert.h>

namespace track_project
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
            int64_t ms = ts.milliseconds % 1000;

            std::tm tm_info = *std::localtime(&seconds);
            os << std::put_time(&tm_info, "%Y-%m-%d %H:%M:%S")
               << "." << std::setfill('0') << std::setw(3) << ms;
            return os;
        }
    };

    // 通用联合体，用于不同类型转换
    union IntFloatUnion
    {
        int ri;
        float rf;
    };

    /*****************************************************************************
     * @brief 流水线架构：TrackingBuffer，计划为每个服务设计一个环形缓冲区结构体
     * 特性：
     * 1. 下级流水线只准用到上级流水线(和 **预处理线程** )的数据
     * 2. 表示航迹，点迹大小情况统一使用std::uint32_t确保数据长度一致性和非负性
     * 3. 运动类参数统一使用double
     * 4. is类型参数统一使用bool
     *****************************************************************************/
    namespace pipeline
    {

        /*****************************************************************************
         * @brief 预处理线程，预处理段：
         * 输入条件：无
         * 输出条件：环形缓冲区存在空闲段（此时航迹管理刚刚处理完）
         * 通讯层允许输入数据堆积，提供适量弹性时序，处理数据为下图结构，存放到环形缓冲区中
         * 至少占用一个进程，不占用线程
         *****************************************************************************/
        // 检测点迹结构
        struct DetectedPoint
        {
            // 运动信息
            double longitude;
            double latitude;

            // 分区依据
            double angle;
            double distance;
            double doppler;

            // 卡尔曼滤波预处理，不考虑曲率
            double x;
            double y;
        };

        /*****************************************************************************
         * @brief 点航关联结构&卡尔曼滤波，第一段流水线：
         * 输入条件：环形缓冲区存在空闲段（此时航迹管理刚刚处理完）
         * 输出条件：无
         * 至少占用一个线程，是否占用GPU待定
         *****************************************************************************/
        // 剩余点迹 
        struct AssociatedPoint
        {
            bool is_associated; // 关联情况
        };

        // 航迹结构
        struct TrackPoint
        {
            double longitude;
            double latitude;
            double sog;      // 对地速度,m/s
            double cog;      // 对地航向,北偏东角度，度
            double angle;    // 雷达观测角度，雷达法线顺时针方向，度
            double distance; // 雷达观测距离，距离雷达站距离，km

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

        /*****************************************************************************
         * @brief 航迹起批流水线结构&卡尔曼滤波流水线结构，组成并行的第二段流水线：
         * 输入条件：环形缓冲区存在空闲段（此时航迹关联刚刚处理完）
         * 输出条件：无
         * 至少占用三个线程,起批需使用GPU
         *****************************************************************************/
        // 航迹点标准结构，固定八字节存储



    } // namespace pipeline

    /*****************************************************************************
     * @brief 通信结构，内存有序排列，方便定长数据读取
     *****************************************************************************/
    namespace communicate
    {
        // 航迹头，请确保数据是完全自包含的数据块，固定4位存一个数字
        struct TrackerHeader
        {
            std::uint32_t track_id = 0;
            std::uint32_t extrapolation_count = 0;
            std::uint32_t point_num = 0;
            int state = 3; // 0表示航迹正常，1表示航迹外推，2表示航迹终结

            void start(std::uint32_t id)
            {
                track_id = id;
                extrapolation_count = 0;
                point_num = 0;
                state = 0;
            };

            void clear()
            {
                track_id = 0;
                extrapolation_count = 0;
                point_num = 0;
                state = -1;
            };
        };

    } // namespace communicate

} // track_project

#endif
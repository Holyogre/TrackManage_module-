/*****************************************************************************
 * @file trackManager.hpp
 * @author xjl (xjl20011009@126.com)
 * @brief 航迹管理器后端服务
 * 1、调用latestKBuffer，设计内存池
 * 2.提供航迹创建，航迹删除，航迹合并（同传感器合批），航迹输出服务
 * 3.提供航迹更新功能，且自动实现单群识别，航迹判断
 * 4.支持一键初始化所有航迹
 *
 * @version 0.3
 * @date 2025-11-03
 *
 * @copyright Copyright (c) 2025
 *
 *****************************************************************************/
#ifndef _TRACK_MANAGER_HPP_
#define _TRACK_MANAGER_HPP_

// 标准库文件
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstring>

// 全局头文件
#include "../include/defstruct.h"

// 数据结构
#include "latestKBuffer.hpp"

namespace trackerManager
{
/*************************************** 航迹PACK包航迹头和航迹点定义***************************************/
// 航迹头，请确保数据是完全自包含的数据块，固定4位存一个数字
#pragma pack(push, 4)
    struct TrackerHeader
    {
        uint32_t track_id = 0;
        uint32_t extrapolation_count = 0;
        uint32_t point_num = 0;
        int state = 3; // 0表示航迹正常，1表示航迹外推，2表示航迹终结

        void start(uint32_t id)
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

        friend std::ostream &operator<<(std::ostream &os, const TrackerHeader &header)
        {
            os << "TrackerHeader [";
            os << "track_id=" << header.track_id << ", ";
            os << "extrapolation_count=" << header.extrapolation_count;
            os << "]";

            return os;
        }
    };
#pragma pack(pop)

// 航迹点标准结构，固定八字节存储
#pragma pack(push, 8)
    struct TrackPoint
    {
        double longitude;
        double latitude;
        double sog;      // 对地速度,m/s
        double cog;      // 对地航向,北偏东角度，度
        double angle;    // 雷达观测角度，雷达法线顺时针方向，度
        double distance; // 雷达观测距离，距离雷达站距离，km

        bool is_associated; // 是否关联
        commonData::Timestamp time;

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
#pragma pack(pop)

    // 约束
    static_assert(std::is_trivially_copyable_v<TrackPoint>, "TrackPoint 不是平凡的");
    static_assert(std::is_standard_layout_v<TrackPoint>, "TrackPoint 不是内存连续的");
    static_assert(std::is_trivially_copyable_v<TrackerHeader>, "TrackerHeader 不是平凡的");
    static_assert(std::is_standard_layout_v<TrackerHeader>, "TrackerHeader 不是内存连续的");

    /***************************************航迹管理类***************************************/
    class TrackerManager
    {
        // 航迹基础结构
        struct TrackerContainer
        {

            TrackerHeader header; // 内存连续且平凡

            LatestKBuffer<TrackPoint> data;

            TrackerContainer(uint32_t point_size) : data(point_size) {}

            void clear()
            {
                header.clear();
                data.clear();
            }

            friend std::ostream &operator<<(std::ostream &os, const TrackerContainer &troj)
            {
                os << "TrackerContainer= [" << troj.header << troj.data << "]";
                return os;
            }
        };

    public:
        /*****************************************************************************
         * @brief 构造新的 Tracker Manager 对象
         *
         * @param track_size 航迹容量上限
         * @param point_size 点迹容量上限
         *****************************************************************************/
        TrackerManager(uint32_t track_size = 2000, uint32_t point_size = 2000);

        /*****************************************************************************
         * @brief 创建新航迹
         *
         * @return 航迹ID，如果内存池已满返回0
         *****************************************************************************/
        uint32_t createTrack();

        /*****************************************************************************
         * @brief 删除航迹
         *
         * @param track_id 要删除的航迹ID
         * @return 是否成功删除
         *****************************************************************************/
        bool deleteTrack(uint32_t track_id);

        /*****************************************************************************
         * @brief 往对应的航迹中存入一个点迹，返回为FALSE的时候需要把对应的航迹给删除了
         *
         * @param track_id
         * @return true 存放航迹点成功
         * @return false 返回FALSE的时候，表明该容器内已经不再有该ID对应的航迹了，所有流水线需要逐级删除这个航迹
         *****************************************************************************/
        bool push_track_point(uint32_t track_id, TrackPoint point);

        /*****************************************************************************
         * @brief 将两条航迹合并（将源航迹数据追加到目标航迹），然后以源航迹的ID号存活下去
         *
         * @param source_track_id 源航迹ID（将被删除），新航迹
         * @param target_track_id 目标航迹ID（保留并接收数据），原本将消亡的航迹
         * @return bool 合并是否成功
         *
         * @note 适用于人工判定两条中断航迹实为同一目标的情况
         *****************************************************************************/
        bool merge_tracks(uint32_t source_track_id, uint32_t target_track_id);

        /*****************************************************************************
         * @brief 拉取航迹数据，塞到发送区
         *
         * @warning 需要自行确保缓冲区容量足够！
         *
         * @param dest 数据提取的目标区域
         * @param track_id 航迹ID
         * @return size_t 偏移Bit数量
         *****************************************************************************/
        size_t pack_track(char *buffer, uint32_t track_id);

        /*****************************************************************************
         * @brief 清空所有航迹
         *****************************************************************************/
        void clearAll();

        // 唯一存在，禁止拷贝
        TrackerManager(const TrackerManager &) = delete;
        TrackerManager &operator=(const TrackerManager &) = delete;

        // 仅允许移动
        TrackerManager(TrackerManager &&) = default;
        TrackerManager &operator=(TrackerManager &&) = default;

        ~TrackerManager() = default;

        // DEBUG类，获取相关信息，实际能开辟的空间应当以size_t来计算
        size_t getTotalCapacity() const { return buffer_pool_.size(); }
        size_t getUsedCount() const { return track_id_to_pool_index_.size(); }
        size_t getFreeCount() const { return free_slots_.size(); }
        bool isValidTrack(uint32_t track_id) const
        {
            return track_id_to_pool_index_.find(track_id) != track_id_to_pool_index_.end();
        }

        // 测试类专用友元
        friend class TrackerManagerDebugger;

    private:
        // 内存池
        std::vector<TrackerContainer> buffer_pool_;

        // 管理数据结构
        std::unordered_map<uint32_t, uint32_t> track_id_to_pool_index_; // 航迹ID -> 池索引
        std::vector<uint32_t> free_slots_;                              // 空闲槽位索引

        uint32_t next_track_id_; // 内部ID自增性，保证唯一性
        const uint32_t max_point_size;
    };

} // namespace trackerManager

#endif
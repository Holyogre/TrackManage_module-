/*****************************************************************************
 * @file ManagementService.hpp
 * @brief 管理服务实现类 - 对外暴露接口不变，内部使用线程循环处理指令
 *
 * 实现要求：
 * 1. 对外暴露接口不变（继承自TrackManagementAPI）
 * 2. 内部是一个循环，每次先处理merge，然后处理create，然后处理add指令
 * 3. 初始化的时候生成线程，一直循环
 * 4. 析构的时候释放线程
 *
 * @version 0.1
 * @date 2025-12-05
 *
 * @copyright Copyright (c) 2025
 *****************************************************************************/

#ifndef _MANAGEMENT_SERVICE_HPP_
#define _MANAGEMENT_SERVICE_HPP_

#include <memory>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

#include "../include/TrackManagementAPI.hpp"
#include "../include/defstruct.h"
#include "TrackerManager.hpp"
#include "TrackerVisualizer.hpp"

namespace track_project
{

    /**
     * @class ManagementService
     * @brief 管理服务实现类，继承自TrackManagementAPI
     *
     * 使用工作线程循环处理指令，指令处理顺序：merge -> create -> add
     */
    class ManagementService : public TrackManagementAPI
    {
    public:
        /*****************************************************************************
         * @brief 构造函数
         *
         * @param track_size 航迹容量上限
         * @param point_size 点迹容量上限
         *****************************************************************************/
        ManagementService(std::uint32_t track_size = 2000, std::uint32_t point_size = 2000);

        /*****************************************************************************
         * @brief 析构函数，停止工作线程并清理资源
         *****************************************************************************/
        virtual ~ManagementService();

        // 禁止拷贝和移动
        ManagementService(const ManagementService &) = delete;
        ManagementService &operator=(const ManagementService &) = delete;
        ManagementService(ManagementService &&) = delete;
        ManagementService &operator=(ManagementService &&) = delete;

        /*****************************************************************************
         * @brief 航迹生成请求
         *
         * @param new_track 新航迹结构
         *****************************************************************************/
        virtual void create_track_command(std::vector<std::array<TrackPoint, 4>> &new_track) override;

        /*****************************************************************************
         * @brief 航迹添加请求
         *
         * @param updated_track 卡尔曼滤波结果
         *****************************************************************************/
        virtual void add_track_command(std::vector<std::pair<TrackerHeader, TrackPoint>> &updated_track) override;

        /*****************************************************************************
         * @brief 航迹融合请求
         *
         * @param source_track_id
         * @param target_track_id
         *****************************************************************************/
        virtual void merge_command(std::uint32_t source_track_id, std::uint32_t target_track_id) override;

        /*****************************************************************************
         * @brief 清空数据区
         *****************************************************************************/
        virtual void clear_all_command() override;

        /*****************************************************************************
         * @brief 获取TrackerManager引用（只读）
         *****************************************************************************/
        const trackmanager::TrackerManager &get_tracker_manager() const { return tracker_manager_; }

    private:
        // 指令类型枚举
        enum class CommandType
        {
            MERGE,
            CREATE,
            ADD,
            CLEAR_ALL
        };

        // 指令结构体
        struct Command
        {
            CommandType type;

            // 根据不同指令类型存储不同数据
            union
            {
                struct
                {
                    std::uint32_t source_track_id;
                    std::uint32_t target_track_id;
                } merge_data;

                struct
                {
                    std::vector<std::array<TrackPoint, 4>> *new_track;
                } create_data;

                struct
                {
                    std::vector<std::pair<TrackerHeader, TrackPoint>> *updated_track;
                } add_data;
            };

            // 构造函数
            Command(CommandType t) : type(t) {}
        };

        /*****************************************************************************
         * @brief 工作线程函数，循环处理指令
         *****************************************************************************/
        void worker_thread();

        /*****************************************************************************
         * @brief 处理单个指令
         *
         * @param cmd 要处理的指令
         *****************************************************************************/
        void process_command(const Command &cmd);

        /*****************************************************************************
         * @brief 处理merge指令
         *
         * @param source_track_id 源航迹ID
         * @param target_track_id 目标航迹ID
         *****************************************************************************/
        void process_merge(std::uint32_t source_track_id, std::uint32_t target_track_id);

        /*****************************************************************************
         * @brief 处理create指令
         *
         * @param new_track 新航迹数据
         *****************************************************************************/
        void process_create(std::vector<std::array<TrackPoint, 4>> &new_track);

        /*****************************************************************************
         * @brief 处理add指令
         *
         * @param updated_track 更新的航迹数据
         *****************************************************************************/
        void process_add(std::vector<std::pair<TrackerHeader, TrackPoint>> &updated_track);

        /*****************************************************************************
         * @brief 处理clear_all指令
         *****************************************************************************/
        void process_clear_all();

        /*****************************************************************************
         * @brief 处理指定类型的所有指令
         *
         * @param type 要处理的指令类型
         * @return bool 是否处理了任何指令
         *****************************************************************************/
        bool process_commands_by_type(CommandType type);

    private:
        // Tracker管理器
        trackmanager::TrackerManager tracker_manager_;
        trackmanager::TrackerVisualizer track_visualizer_;

        // 线程控制
        std::thread worker_thread_;
        std::atomic<bool> stop_flag_;

        // 指令队列
        std::queue<Command> command_queue_;
        std::mutex queue_mutex_;
        std::condition_variable queue_cv_;

        // 数据存储（用于避免数据在队列中失效）
        std::vector<std::array<TrackPoint, 4>> create_buffer_;
        std::vector<std::pair<TrackerHeader, TrackPoint>> add_buffer_;
        std::mutex buffer_mutex_;
    };

} // namespace track_project

#endif // _MANAGEMENT_SERVICE_HPP_

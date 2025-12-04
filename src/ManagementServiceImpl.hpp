/*****************************************************************************
 * @file ManagementServiceImpl.hpp
 * @author xjl (xjl20011009@126.com)
 * @brief ManagementService 具体实现类
 *
 * 实现功能：
 * 1. 在一个单独的线程中进行事务循环
 * 2. 接收BUFFER数据，进行航迹管理、显示
 * 3. 定期（1min）读取配置文件，重置接收接口
 * 4. 组合调用可视化插件、日志插件和航迹管理层
 *
 * @version 0.1
 * @date 2025-12-04
 *
 * @copyright Copyright (c) 2025
 *****************************************************************************/
#ifndef _MANAGEMENT_SERVICE_IMPL_HPP_
#define _MANAGEMENT_SERVICE_IMPL_HPP_

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "../include/ManagementService.h"
#include "TrackerManager.hpp"
#include "TrackerVisualizer.hpp"
#include "../utils/Logger.hpp"

namespace track_project
{

    class ManagementServiceImpl : public ManagementService
    {
    public:
        /*****************************************************************************
         * @brief 构造函数
         *
         * @param config_path 配置文件路径
         *****************************************************************************/
        explicit ManagementServiceImpl(const std::string &config_path = "");

        /*****************************************************************************
         * @brief 析构函数
         *****************************************************************************/
        ~ManagementServiceImpl() override;

        // 禁止拷贝和移动
        ManagementServiceImpl(const ManagementServiceImpl &) = delete;
        ManagementServiceImpl &operator=(const ManagementServiceImpl &) = delete;
        ManagementServiceImpl(ManagementServiceImpl &&) = delete;
        ManagementServiceImpl &operator=(ManagementServiceImpl &&) = delete;

        /*****************************************************************************
         * @brief 接收上一级流水线完成指令
         *
         * 实现应处理完成的数据，并触发下一阶段处理。
         * 内部的任何失败都写入日志，然后继续运行。
         *
         * @param buffer 流水线缓冲区，包含各级流水线的处理结果
         *****************************************************************************/
        void onPipelineComplete(const pipeline::TrackingBuffer &buffer) override;

        /*****************************************************************************
         * @brief 接收航迹融合指令
         *
         * 当需要融合两条航迹时调用此接口（如人工判定两条中断航迹实为同一目标）。
         * 实现应调用航迹管理层的合并功能，并更新相关状态。
         *
         * @param source_track_id 源航迹ID（新航迹，将被删除）
         * @param target_track_id 目标航迹ID（旧航迹，保留并接收数据）
         *****************************************************************************/
        void onTrackFusion(std::uint32_t source_track_id, std::uint32_t target_track_id) override;

        /*****************************************************************************
         * @brief reload
         *
         * 所有组件恢复到initialize的状态
         *****************************************************************************/
        void reload() override;

        /*****************************************************************************
         * @brief 启动服务线程
         *
         * @return bool 启动是否成功
         *****************************************************************************/
        bool start();

        /*****************************************************************************
         * @brief 停止服务线程
         *****************************************************************************/
        void stop();

        /*****************************************************************************
         * @brief 获取服务运行状态
         *
         * @return bool 服务是否正在运行
         *****************************************************************************/
        bool isRunning() const { return running_; }

        /*****************************************************************************
         * @brief 获取活跃航迹数量
         *
         * @return size_t 活跃航迹数量
         *****************************************************************************/
        size_t getActiveTrackCount() const;

        /*****************************************************************************
         * @brief 重新加载配置文件
         *
         * 定期（1min）调用此函数读取配置文件，重置接收接口
         *****************************************************************************/
        void reloadConfig();

    private:
        /*****************************************************************************
         * @brief 服务线程主函数
         *****************************************************************************/
        void serviceThread();

        /*****************************************************************************
         * @brief 处理流水线缓冲区数据
         *
         * @param buffer 流水线缓冲区
         *****************************************************************************/
        void processPipelineBuffer(const pipeline::TrackingBuffer &buffer);

        /*****************************************************************************
         * @brief 初始化组件
         *
         * @return bool 初始化是否成功
         *****************************************************************************/
        bool initializeComponents();

        /*****************************************************************************
         * @brief 清理组件资源
         *****************************************************************************/
        void cleanupComponents();

    private:
        // 配置
        std::string config_path_;

        // 服务线程控制
        std::atomic<bool> running_{false};
        std::thread service_thread_;
        std::mutex buffer_mutex_;
        std::condition_variable buffer_cv_;

        // 组件
        std::unique_ptr<trackmanager::TrackerManager> tracker_manager_;
        std::unique_ptr<trackmanager::TrackerVisualizer> racker_visualizer_;

        // 当前占有数据区
        std::atomic<pipeline::TrackingBuffer> buffer_queue_;

        // 最后一次配置重载时间
        std::chrono::steady_clock::time_point last_config_reload_;
    };

} // namespace track_project

#endif // _MANAGEMENT_SERVICE_IMPL_HPP_

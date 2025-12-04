/*****************************************************************************
 * @file ManagementService.h
 * @author xjl (xjl20011009@126.com)
 * @brief 航机管理器API接口，抽象基类
 *
 * 提供如下主要指令接口：
 * 1. 接收上一级流水线完成指令，执行处理
 * 2. 接收上一级流水线reload指令，实现当前流水线重载
 * 3. 接收航迹融合指令
 *
 * @version 0.3
 * @date 2025-12-04
 *
 * @copyright Copyright (c) 2025
 *****************************************************************************/

#ifndef _MANAGEMENT_SERVICE_H_
#define _MANAGEMENT_SERVICE_H_

#include <cstdint>
#include <vector>
#include <memory>
#include <string>

// 包含必要的头文件
#include "defstruct.h"

namespace track_project
{

    /**
     * @class TrackManagementAPI
     * @brief 管理服务层抽象基类 - 对外暴露的统一接口
     *
     * 该接口用于与流水线架构对接，提供标准化的控制指令接口。
     * 具体实现应组合调用可视化插件、日志插件和航迹管理层。
     */
    class TrackManagementAPI
    {
    public:
        virtual ~TrackManagementAPI() = default;

        /*****************************************************************************
         * @brief 接收上一级流水线完成指令
         *
         * 当上一级流水线（如卡尔曼滤波、航迹关联等）完成处理后调用此接口。
         * 实现应处理完成的数据，并触发下一阶段处理。
         * 内部的任何失败都写入日志，然后继续运行，不用反馈正确与否，也没有总控来统一管理
         *
         * @param buffer 流水线缓冲区，包含各级流水线的处理结果
         *****************************************************************************/
        virtual void onPipelineComplete() = 0;

        /*****************************************************************************
         * @brief 接收航迹融合指令
         *
         * 当需要融合两条航迹时调用此接口（如人工判定两条中断航迹实为同一目标）。
         * 实现应调用航迹管理层的合并功能，并更新相关状态。
         *
         * @param source_track_id 源航迹ID（新航迹，将被删除）
         * @param target_track_id 目标航迹ID（旧航迹，保留并接收数据）
         *****************************************************************************/
        virtual void onTrackFusion(std::uint32_t source_track_id, std::uint32_t target_track_id) = 0;

        /*****************************************************************************
         * @brief reload
         *
         * 所有组件恢复到initialize的状态
         *****************************************************************************/
        virtual void reload() = 0;

    protected:
        // 保护构造函数，确保只能通过派生类实例化
        TrackManagementAPI() = default;

        // 禁止拷贝和移动
        TrackManagementAPI(const TrackManagementAPI &) = delete;
        TrackManagementAPI &operator=(const TrackManagementAPI &) = delete;
        TrackManagementAPI(TrackManagementAPI &&) = delete;
        TrackManagementAPI &operator=(TrackManagementAPI &&) = delete;
    };

} // namespace track_project

#endif // _MANAGEMENT_SERVICE_H_

/*****************************************************************************
 * @file ManagementService.h
 * @author xjl (xjl20011009@126.com)
 * @brief 管理服务层 - 对外暴露的虚函数接口
 *
 * 提供两个主要指令接口：
 * 1. 接收上一级流水线完成指令
 * 2. 接收航迹融合指令
 * 3. 定期（1min）读取配置文件，重置接收接口
 *
 * 实现功能：
 * 在一个单独的线程中进行事务循环，外部通过接口进行请求，内部接口收到请求后进行对应处理
 * 接收BUFFER数据，进行航迹管理，显示；
 *
 * @version 0.2
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
     * @class ManagementService
     * @brief 管理服务层抽象基类 - 对外暴露的统一接口
     *
     * 该接口用于与流水线架构对接，提供标准化的控制指令接口。
     * 具体实现应组合调用可视化插件、日志插件和航迹管理层。
     */
    class ManagementService
    {
    public:
        virtual ~ManagementService() = default;

        /*****************************************************************************
         * @brief 接收上一级流水线完成指令
         *
         * 当上一级流水线（如卡尔曼滤波、航迹关联等）完成处理后调用此接口。
         * 实现应处理完成的数据，并触发下一阶段处理。
         * 内部的任何失败都写入日志，然后继续运行，不用反馈正确与否，也没有总控来统一管理
         *
         * @param buffer 流水线缓冲区，包含各级流水线的处理结果
         *****************************************************************************/
        virtual void onPipelineComplete(const pipeline::TrackingBuffer &buffer) = 0;

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
        virtual void shutdown() = 0;

    protected:
        // 保护构造函数，确保只能通过派生类实例化
        ManagementService() = default;

        // 禁止拷贝和移动
        ManagementService(const ManagementService &) = delete;
        ManagementService &operator=(const ManagementService &) = delete;
        ManagementService(ManagementService &&) = delete;
        ManagementService &operator=(ManagementService &&) = delete;
    };

} // namespace track_project

#endif // _MANAGEMENT_SERVICE_H_

/*****************************************************************************
 * @file ManagementService.h
 * @author xjl (xjl20011009@126.com)
 * @brief 管理服务层 - 对外暴露的虚函数接口
 * 
 * 提供两个主要指令接口：
 * 1. 接收上一级流水线完成指令
 * 2. 接收航迹融合指令
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

namespace track_project {

/**
 * @class ManagementService
 * @brief 管理服务层抽象基类 - 对外暴露的统一接口
 * 
 * 该接口用于与流水线架构对接，提供标准化的控制指令接口。
 * 具体实现应组合调用可视化插件、日志插件和航迹管理层。
 */
class ManagementService {
public:
    virtual ~ManagementService() = default;

    /*****************************************************************************
     * @brief 接收上一级流水线完成指令
     * 
     * 当上一级流水线（如卡尔曼滤波、航迹关联等）完成处理后调用此接口。
     * 实现应处理完成的数据，并触发下一阶段处理（如航迹管理、可视化等）。
     * 
     * @param buffer 流水线缓冲区，包含各级流水线的处理结果
     * @param pipeline_stage 流水线阶段标识
     *                  0: 预处理完成
     *                  1: 航迹关联完成  
     *                  2: 航迹起批/卡尔曼滤波完成
     *                  3: 航迹管理完成
     * @return bool 处理是否成功
     *****************************************************************************/
    virtual bool onPipelineComplete(
        const pipeline::TrackingBuffer& buffer,
        std::uint32_t pipeline_stage) = 0;

    /*****************************************************************************
     * @brief 接收航迹融合指令
     * 
     * 当需要融合两条航迹时调用此接口（如人工判定两条中断航迹实为同一目标）。
     * 实现应调用航迹管理层的合并功能，并更新相关状态。
     * 
     * @param source_track_id 源航迹ID（新航迹，将被删除）
     * @param target_track_id 目标航迹ID（旧航迹，保留并接收数据）
     * @param fusion_reason 融合原因描述（用于日志记录）
     * @return bool 融合是否成功
     *****************************************************************************/
    virtual bool onTrackFusion(
        std::uint32_t source_track_id,
        std::uint32_t target_track_id,
        const std::string& fusion_reason = "manual_fusion") = 0;

    /*****************************************************************************
     * @brief 获取服务状态
     * 
     * @return std::string 服务状态描述（运行中、停止、错误等）
     *****************************************************************************/
    virtual std::string getServiceStatus() const = 0;

    /*****************************************************************************
     * @brief 初始化服务
     * 
     * @param config_path 配置文件路径（可选）
     * @return bool 初始化是否成功
     *****************************************************************************/
    virtual bool initialize(const std::string& config_path = "") = 0;

    /*****************************************************************************
     * @brief 停止服务
     * 
     * 清理资源，停止所有相关组件
     *****************************************************************************/
    virtual void shutdown() = 0;

    /*****************************************************************************
     * @brief 获取活跃航迹列表
     * 
     * @return std::vector<std::uint32_t> 当前活跃航迹ID列表
     *****************************************************************************/
    virtual std::vector<std::uint32_t> getActiveTracks() const = 0;

    /*****************************************************************************
     * @brief 获取航迹数据
     * 
     * @param track_id 航迹ID
     * @return std::vector<track_project::communicate::TrackPoint> 航迹点数据
     *****************************************************************************/
    virtual std::vector<track_project::communicate::TrackPoint> getTrackData(std::uint32_t track_id) const = 0;

protected:
    // 保护构造函数，确保只能通过派生类实例化
    ManagementService() = default;
    
    // 禁止拷贝和移动
    ManagementService(const ManagementService&) = delete;
    ManagementService& operator=(const ManagementService&) = delete;
    ManagementService(ManagementService&&) = delete;
    ManagementService& operator=(ManagementService&&) = delete;
};

} // namespace track_project

#endif // _MANAGEMENT_SERVICE_H_

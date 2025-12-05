/*****************************************************************************
 * @file ManagementService.h
 * @author xjl (xjl20011009@126.com)
 * @brief 航机管理器API接口，抽象基类
 *
 * 提供如下主要指令接口：
 * 1. 新增航迹
 * 2. 删除航迹
 * 3. 融合航迹
 * 4. 开启可视化
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
         * @brief 航迹生成请求
         *
         * @param new_track 新航迹结构
         *****************************************************************************/
        virtual void create_track_command(std::vector<std::array<TrackPoint, 4>> &new_track) = 0;

        /*****************************************************************************
         * @brief 航迹添加请求
         *
         * @param updated_track 卡尔曼滤波结果
         *****************************************************************************/
        virtual void add_track_command(std::vector<std::pair<TrackerHeader, TrackPoint>> &updated_track) = 0;

        /*****************************************************************************
         * @brief 航迹融合请求
         *
         * @param source_track_id
         * @param target_track_id
         *****************************************************************************/
        virtual void merge_command(std::uint32_t source_track_id, std::uint32_t target_track_id) = 0;

        /*****************************************************************************
         * @brief 清空数据区
         *****************************************************************************/
        virtual void clear_all_command() = 0;

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

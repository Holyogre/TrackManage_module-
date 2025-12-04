/*****************************************************************************
 * @file management_service_example.cpp
 * @author xjl (xjl20011009@126.com)
 * @brief ManagementService 使用示例
 * 
 * 演示如何：
 * 1. 创建 ManagementServiceImpl 实例
 * 2. 启动服务线程
 * 3. 发送流水线完成指令
 * 4. 发送航迹融合指令
 * 5. 关闭服务
 *
 * @version 0.1
 * @date 2025-12-04
 *
 * @copyright Copyright (c) 2025
 *****************************************************************************/

#include <iostream>
#include <thread>
#include <chrono>

#include "../include/defstruct.h"
#include "../src/ManagementServiceImpl.hpp"

using namespace track_project;

int main() {
    std::cout << "=== ManagementService 使用示例 ===" << std::endl;
    
    try {
        // 1. 创建 ManagementServiceImpl 实例
        std::cout << "1. 创建 ManagementServiceImpl 实例..." << std::endl;
        auto service = std::make_unique<ManagementServiceImpl>("config/config.ini");
        
        // 2. 启动服务线程
        std::cout << "2. 启动服务线程..." << std::endl;
        if (!service->start()) {
            std::cerr << "启动服务失败" << std::endl;
            return 1;
        }
        
        std::cout << "服务已启动，运行状态: " << (service->isRunning() ? "运行中" : "已停止") << std::endl;
        std::cout << "活跃航迹数量: " << service->getActiveTrackCount() << std::endl;
        
        // 3. 模拟发送流水线完成指令
        std::cout << "\n3. 模拟发送流水线完成指令..." << std::endl;
        {
            pipeline::TrackingBuffer buffer;
            
            // 添加一些测试数据
            pipeline::DetectedPoint detected_point;
            detected_point.longitude = 116.3975;  // 北京经度
            detected_point.latitude = 39.9087;    // 北京纬度
            buffer.detected_point.push_back(detected_point);
            
            pipeline::AssociatedPoint associated_point;
            associated_point.track_id = 1;
            associated_point.point_id = 1;
            buffer.associated_point.push_back(associated_point);
            
            // 发送流水线完成指令
            service->onPipelineComplete(buffer);
            std::cout << "流水线完成指令已发送" << std::endl;
        }
        
        // 等待处理
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 4. 模拟发送航迹融合指令
        std::cout << "\n4. 模拟发送航迹融合指令..." << std::endl;
        {
            // 假设航迹1和航迹2需要融合
            std::uint32_t source_track_id = 1;
            std::uint32_t target_track_id = 2;
            
            service->onTrackFusion(source_track_id, target_track_id);
            std::cout << "航迹融合指令已发送: " << source_track_id << " -> " << target_track_id << std::endl;
        }
        
        // 5. 等待一段时间，模拟服务运行
        std::cout << "\n5. 服务运行中（等待5秒）..." << std::endl;
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "已运行 " << (i + 1) << " 秒，活跃航迹: " << service->getActiveTrackCount() << std::endl;
        }
        
        // 6. 关闭服务
        std::cout << "\n6. 关闭服务..." << std::endl;
        service->shutdown();
        
        std::cout << "\n=== 示例程序完成 ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "程序异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

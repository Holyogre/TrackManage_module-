/*****************************************************************************
 * @file ManagementServiceImpl.cpp
 * @author xjl (xjl20011009@126.com)
 * @brief ManagementService 具体实现
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

#include "ManagementServiceImpl.hpp"
#include <chrono>
#include <iostream>

// 如果TrackerVisualizer需要OpenCV，我们可以条件编译
// 暂时先注释掉可视化部分，专注于核心功能
// #define ENABLE_VISUALIZATION

using namespace track_project;

namespace {
    // 默认配置
    constexpr size_t DEFAULT_TRACK_CAPACITY = 2000;
    constexpr size_t DEFAULT_TRACK_LENGTH = 2000;
    constexpr std::chrono::minutes CONFIG_RELOAD_INTERVAL(1); // 1分钟
}

ManagementServiceImpl::ManagementServiceImpl(const std::string& config_path)
    : config_path_(config_path)
    , last_config_reload_(std::chrono::steady_clock::now()) {
    
    LOG_INFO("ManagementServiceImpl 构造函数，配置文件: {}", 
             config_path_.empty() ? "默认" : config_path_);
    
    if (!initializeComponents()) {
        LOG_ERROR("ManagementServiceImpl 组件初始化失败");
        throw std::runtime_error("组件初始化失败");
    }
}

ManagementServiceImpl::~ManagementServiceImpl() {
    stop();
    shutdown();
    LOG_INFO("ManagementServiceImpl 析构函数完成");
}

bool ManagementServiceImpl::initializeComponents() {
    try {
        // 初始化航迹管理器
        tracker_manager_ = std::make_unique<trackmanager::TrackerManager>(
            DEFAULT_TRACK_CAPACITY, DEFAULT_TRACK_LENGTH);
        
        LOG_INFO("航迹管理器初始化成功，容量: {} 条航迹，每条 {} 个点迹",
                 DEFAULT_TRACK_CAPACITY, DEFAULT_TRACK_LENGTH);
        
        // 初始化可视化组件（条件编译）
#ifdef ENABLE_VISUALIZATION
        tracker_visualizer_ = std::make_unique<TrackerVisualizer>();
        if (tracker_visualizer_) {
            LOG_INFO("可视化组件初始化成功");
        }
#else
        LOG_INFO("可视化组件已禁用（编译时未启用 ENABLE_VISUALIZATION）");
#endif
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("组件初始化异常: {}", e.what());
        return false;
    }
}

void ManagementServiceImpl::cleanupComponents() {
    tracker_visualizer_.reset();
    tracker_manager_.reset();
    LOG_INFO("所有组件资源已清理");
}

bool ManagementServiceImpl::start() {
    if (running_) {
        LOG_WARN("服务已经在运行中");
        return true;
    }
    
    running_ = true;
    service_thread_ = std::thread(&ManagementServiceImpl::serviceThread, this);
    
    LOG_INFO("管理服务线程已启动");
    return true;
}

void ManagementServiceImpl::stop() {
    if (!running_) {
        return;
    }
    
    LOG_INFO("正在停止管理服务线程...");
    running_ = false;
    buffer_cv_.notify_all();
    
    if (service_thread_.joinable()) {
        service_thread_.join();
        LOG_INFO("管理服务线程已停止");
    }
}

void ManagementServiceImpl::serviceThread() {
    LOG_INFO("管理服务线程开始运行");
    
    while (running_) {
        try {
            // 检查是否需要重载配置
            auto now = std::chrono::steady_clock::now();
            if (now - last_config_reload_ >= CONFIG_RELOAD_INTERVAL) {
                reloadConfig();
                last_config_reload_ = now;
            }
            
            // 处理缓冲区队列
            std::vector<pipeline::TrackingBuffer> buffers_to_process;
            {
                std::unique_lock<std::mutex> lock(buffer_mutex_);
                if (buffer_queue_.empty()) {
                    // 等待数据或超时
                    buffer_cv_.wait_for(lock, std::chrono::milliseconds(100));
                    continue;
                }
                
                buffers_to_process.swap(buffer_queue_);
            }
            
            // 处理所有缓冲区
            for (const auto& buffer : buffers_to_process) {
                processPipelineBuffer(buffer);
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("服务线程异常: {}", e.what());
            // 继续运行，不退出线程
        }
    }
    
    LOG_INFO("管理服务线程结束运行");
}

void ManagementServiceImpl::onPipelineComplete(const pipeline::TrackingBuffer& buffer) {
    try {
        LOG_DEBUG("收到流水线完成指令，缓冲区大小 - 检测点: {}, 关联点: {}, 预测点: {}",
                  buffer.detected_point.size(),
                  buffer.associated_point.size(),
                  buffer.predicted_point.size());
        
        // 将缓冲区添加到队列
        {
            std::unique_lock<std::mutex> lock(buffer_mutex_);
            buffer_queue_.push_back(buffer);
        }
        
        // 通知服务线程有新数据
        buffer_cv_.notify_one();
        
        LOG_DEBUG("流水线缓冲区已添加到处理队列，当前队列大小: {}", buffer_queue_.size());
        
    } catch (const std::exception& e) {
        LOG_ERROR("处理流水线完成指令异常: {}", e.what());
        // 继续运行，不抛出异常
    }
}

void ManagementServiceImpl::onTrackFusion(std::uint32_t source_track_id, std::uint32_t target_track_id) {
    try {
        LOG_INFO("收到航迹融合指令: 源航迹 {} -> 目标航迹 {}", source_track_id, target_track_id);
        
        if (!tracker_manager_) {
            LOG_ERROR("航迹管理器未初始化，无法执行融合");
            return;
        }
        
        // 检查航迹是否存在
        if (!tracker_manager_->isValidTrack(source_track_id)) {
            LOG_ERROR("源航迹 {} 不存在", source_track_id);
            return;
        }
        
        if (!tracker_manager_->isValidTrack(target_track_id)) {
            LOG_ERROR("目标航迹 {} 不存在", target_track_id);
            return;
        }
        
        // 执行航迹融合
        bool success = tracker_manager_->merge_tracks(source_track_id, target_track_id);
        
        if (success) {
            LOG_INFO("航迹融合成功: {} -> {}", source_track_id, target_track_id);
            
            // 更新可视化（如果启用）
#ifdef ENABLE_VISUALIZATION
            if (tracker_visualizer_) {
                // 这里可以调用可视化更新接口
                LOG_DEBUG("已通知可视化组件更新融合后的航迹");
            }
#endif
        } else {
            LOG_ERROR("航迹融合失败: {} -> {}", source_track_id, target_track_id);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("处理航迹融合指令异常: {}", e.what());
        // 继续运行，不抛出异常
    }
}

void ManagementServiceImpl::shutdown() {
    LOG_INFO("正在关闭管理服务...");
    
    // 停止服务线程
    stop();
    
    // 清理组件资源
    cleanupComponents();
    
    LOG_INFO("管理服务已关闭");
}

void ManagementServiceImpl::processPipelineBuffer(const pipeline::TrackingBuffer& buffer) {
    try {
        LOG_DEBUG("开始处理流水线缓冲区，包含 {} 个检测点，{} 个关联点，{} 个预测点",
                  buffer.detected_point.size(),
                  buffer.associated_point.size(),
                  buffer.predicted_point.size());
        
        // 处理检测点迹 - 创建新航迹或更新现有航迹
        for (const auto& point : buffer.detected_point) {
            // 这里可以实现检测点迹的处理逻辑
            // 例如：关联到现有航迹或创建新航迹
            LOG_TRACE("处理检测点迹: lon={}, lat={}", point.longitude, point.latitude);
        }
        
        // 处理关联点迹 - 更新航迹状态
        for (const auto& point : buffer.associated_point) {
            LOG_TRACE("处理关联点迹: 航迹 {}, 点迹 {}", point.track_id, point.point_id);
            
            // 这里可以调用 tracker_manager_->push_track_point() 等接口
        }
        
        // 处理预测点迹 - 更新卡尔曼滤波结果
        for (const auto& point : buffer.predicted_point) {
            if (point.is_updated) {
                LOG_TRACE("处理更新后的预测点迹: 航迹 {}, 位置 ({}, {})", 
                         point.track_id, point.x, point.y);
            }
        }
        
        // 处理现有航迹
        for (const auto& track : buffer.existed_point) {
            LOG_TRACE("处理现有航迹: ID {}, 状态 {}, 点数 {}", 
                     track.track_id, track.state, track.point_num);
        }
        
        // 更新可视化（如果启用）
#ifdef ENABLE_VISUALIZATION
        if (tracker_visualizer_ && tracker_manager_) {
            // 获取所有活跃航迹并更新可视化
            auto active_tracks = tracker_manager_->getActiveTrackIds();
            if (!active_tracks.empty()) {
                LOG_DEBUG("更新可视化，活跃航迹数: {}", active_tracks.size());
            }
        }
#endif
        
        LOG_DEBUG("流水线缓冲区处理完成");
        
    } catch (const std::exception& e) {
        LOG_ERROR("处理流水线缓冲区异常: {}", e.what());
        // 继续处理下一个缓冲区
    }
}

void ManagementServiceImpl::reloadConfig() {
    try {
        LOG_INFO("重新加载配置文件: {}", config_path_);
        
        // 这里实现配置文件的读取和解析
        // 例如：读取新的端口配置、阈值参数等
        
        if (config_path_.empty()) {
            LOG_DEBUG("使用默认配置，无需重载");
            return;
        }
        
        // 模拟配置重载
        LOG_INFO("配置文件已重载（待实现具体解析逻辑）");
        
        // 可以在这里重置接收接口、更新组件配置等
        
    } catch (const std::exception& e) {
        LOG_ERROR("重载配置文件异常: {}", e.what());
    }
}

size_t ManagementServiceImpl::getActiveTrackCount() const {
    if (!tracker_manager_) {
        LOG_WARN("航迹管理器未初始化");
        return 0;
    }
    
    return tracker_manager_->getUsedCount();
}

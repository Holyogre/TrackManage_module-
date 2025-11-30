/*****************************************************************************
 * @file TrackerComm.hpp
 * @author xjl (xjl20011009@126.com)
 * @brief 航迹管理器通信模块
 * 1. 禁止热重载
 * 1. 由commondata::Config控制，读取哪个端口的控制命令，执行航迹融合、日志记录
 * 2. 由commondata::Config控制，将数据发送出去
 * @version 0.1
 * @date 2025-11-29
 *
 * @copyright Copyright (c) 2025
 *
 *****************************************************************************/
#ifndef _TRACKER_COMM_HPP_
#define _TRACKER_COMM_HPP_

// 内存操作
#include <cstring>
// 结构体定义
#include "../include/defstruct.h"
// 网络定义
#include <arpa/inet.h>
// unix操作系统API
#include <unistd.h>
// 线程控制
#include <thread>
#include <Logger.hpp>

namespace track_project
{
    namespace trackmanager
    {
        class TrackerComm
        {
        public:
            TrackerComm(commondata::Config configx)
                : track_dst_ip_(configx.track_dst_ip),
                  track_dst_port_(configx.track_dst_port),
                  sockfd_(-1)
            {
                // 初始化目标地址
                memset(&dest_addr_, 0, sizeof(dest_addr_));
                dest_addr_.sin_family = AF_INET; // IPV4协议
                dest_addr_.sin_port = htons(track_dst_port_);
                inet_pton(AF_INET, track_dst_ip_.c_str(), &dest_addr_.sin_addr);

                // 记录初始化信息
                LOG_DEBUG << "TrackerComm初始化: " << track_dst_ip_ << ":" << track_dst_port_ << std::endl;

                // 启动时直接创建socket，避免首次发送延迟
                create_socket();
            }

            ~TrackerComm()
            {
                // 只在析构时关闭
                if (sockfd_ >= 0)
                {
                    close(sockfd_);
                    LOG_DEBUG << "TrackerComm清理完成" << std::endl;
                }
            }

            // 发送跟踪数据
            bool SendTrackData(const float *data, size_t data_count)
            {
                if (data_count == 0)
                {
                    LOG_INFO << "空跟踪数据，跳过发送" << std::endl;
                    return true;
                }

                // 发送
                ssize_t sent = sendto(sockfd_, data, data_count * sizeof(float), 0,
                                      (struct sockaddr *)&dest_addr_, sizeof(dest_addr_));
                if (sent == static_cast<ssize_t>(data_count * sizeof(float)))
                {
                    LOG_DEBUG << "跟踪数据发送成功: " << sent << "字节" << std::endl;
                    return true;
                }

                LOG_ERROR << "跟踪数据发送失败(尝试" << strerror(errno) << std::endl;
                return false;
            }

            // 获取连接状态
            bool IsConnected() const
            {
                return sockfd_ >= 0;
            }

            // 重新配置目标地址
            void Reconfigure(const std::string &new_ip, uint16_t new_port)
            {
                track_dst_ip_ = new_ip;
                track_dst_port_ = new_port;

                memset(&dest_addr_, 0, sizeof(dest_addr_)); // 清空结构体
                dest_addr_.sin_family = AF_INET;
                dest_addr_.sin_port = htons(track_dst_port_);                    // 设置端口号
                inet_pton(AF_INET, track_dst_ip_.c_str(), &dest_addr_.sin_addr); // 将字符串IP地址转换为二进制格式并存入结构体

                LOG_DEBUG << "TrackerComm重新配置: " << track_dst_ip_ << ":" << track_dst_port_ << std::endl; // TODO改成日志库
            }

        private:
            // 创造连接
            bool create_socket()
            {
                sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
                if (sockfd_ < 0)
                {
                    LOG_ERROR << "创建socket失败: " << strerror(errno) << std::endl;
                    return false;
                }

                LOG_DEBUG << "Socket创建成功" << std::endl; // TODO改成日志库
                return true;
            }

        private:
            // 配置参数
            std::string track_dst_ip_;
            uint16_t track_dst_port_;

            // UDP通信
            int sockfd_;
            struct sockaddr_in dest_addr_;
        };
    } // namespace trackmanager
} // namespace track_project
#endif
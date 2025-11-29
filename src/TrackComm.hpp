#ifndef _TRACK_COMM_HPP_
#define _TRACK_COMM_HPP_

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

namespace track_project
{
    namespace trackmanager
    {
        class TrackComm
        {
        public:
            TrackComm(commondata::Config configx)
                : track_dst_ip_(configx.track_dst_ip),
                  track_dst_port_(configx.track_dst_port),
                  sockfd_(-1)
            {
                // 初始化目标地址
                memset(&dest_addr_, 0, sizeof(dest_addr_));
                dest_addr_.sin_family = AF_INET; // IPV4协议
                dest_addr_.sin_port = htons(track_dst_port_);
                inet_pton(AF_INET, track_dst_ip_.c_str(), &dest_addr_.sin_addr);

                // DEBUG信息 TODO，改成日志库输出
                std::cout << "TrackComm初始化: " << track_dst_ip_ << ":" << track_dst_port_ << std::endl;

                // 启动时直接创建socket，避免首次发送延迟
                create_socket();
            }

            ~TrackComm()
            {
                // 只在析构时关闭
                if (sockfd_ >= 0)
                {
                    close(sockfd_);
                    std::cout << "TrackComm清理完成" << std::endl;
                }
            }

            // 发送跟踪数据
            bool SendTrackData(const float *data, size_t data_count)
            {
                if (data_count == 0)
                {
                    std::cout << "空跟踪数据，跳过发送" << std::endl;
                    return true;
                }

                // 发送
                ssize_t sent = sendto(sockfd_, data, data_count * sizeof(float), 0,
                                      (struct sockaddr *)&dest_addr_, sizeof(dest_addr_));
                if (sent == static_cast<ssize_t>(data_count * sizeof(float)))
                {
                    std::cout << "跟踪数据发送成功: " << sent << "字节" << std::endl;
                    return true;
                }

                std::cerr << "跟踪数据发送失败(尝试" << strerror(errno) << std::endl;
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

                std::cout << "TrackComm重新配置: " << track_dst_ip_ << ":" << track_dst_port_ << std::endl; // TODO改成日志库
            }

        private:
            // 创造连接
            bool create_socket()
            {
                sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
                if (sockfd_ < 0)
                {
                    std::cerr << "创建socket失败: " << strerror(errno) << std::endl;
                    return false;
                }

                std::cout << "Socket创建成功" << std::endl; // TODO改成日志库
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
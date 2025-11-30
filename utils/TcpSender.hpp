/*****************************************************************************
 * @file TcpSender.hpp
 * @author xjl (xjl20011009@126.com)
 *
 * @brief TCP数据发送基类
 * 简洁的TCP数据发送功能
 *
 * @version 0.1
 * @date 2025-11-07
 *
 * @copyright Copyright (c) 2025
 *****************************************************************************/
#ifndef _TCP_SENDER_HPP_
#define _TCP_SENDER_HPP_

// 基础结构
#include <string>
#include <cstdint>
#include <vector>
#include <system_error>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace track_project
{

/*****************************************************************************
 * @brief TCP数据发送器
 *****************************************************************************/
class TcpSender
{
private:
    int sockfd_ = -1;
    std::string dest_ip_;
    std::uint16_t dest_port_;
    bool is_connected_ = false;
    static constexpr size_t MAX_PACKET_SIZE = 65535;

public:
    /*****************************************************************************
     * @brief 构造函数
     * @param ip 目标IP地址
     * @param port 目标端口
     *****************************************************************************/
    TcpSender(const std::string &ip, std::uint16_t port) 
        : dest_ip_(ip), dest_port_(port) {}

    /*****************************************************************************
     * @brief 析构函数
     *****************************************************************************/
    ~TcpSender()
    {
        disconnect();
    }

    /*****************************************************************************
     * @brief 连接到目标服务器
     * @return true 成功, false 失败
     *****************************************************************************/
    bool connect()
    {
        if (is_connected_)
        {
            disconnect();
        }

        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0)
        {
            return false;
        }

        sockaddr_in dest_addr{};
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(dest_port_);
        
        if (inet_pton(AF_INET, dest_ip_.c_str(), &dest_addr.sin_addr) <= 0)
        {
            close(sockfd_);
            sockfd_ = -1;
            return false;
        }

        if (::connect(sockfd_, (sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
        {
            close(sockfd_);
            sockfd_ = -1;
            return false;
        }

        is_connected_ = true;
        return true;
    }

    /*****************************************************************************
     * @brief 断开连接
     *****************************************************************************/
    void disconnect()
    {
        if (sockfd_ >= 0)
        {
            close(sockfd_);
            sockfd_ = -1;
            is_connected_ = false;
        }
    }

    /*****************************************************************************
     * @brief 发送数据
     * @param data 数据指针
     * @param length 数据长度
     * @return true 成功, false 失败
     *****************************************************************************/
    bool send(const void *data, size_t length)
    {
        if (!is_connected_ || sockfd_ < 0)
        {
            return false;
        }

        if (data == nullptr || length == 0)
        {
            return false;
        }

        const char *byte_data = static_cast<const char *>(data);
        size_t sent_total = 0;

        while (sent_total < length)
        {
            size_t remaining = length - sent_total;
            size_t chunk_size = (remaining > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : remaining;

            ssize_t sent = ::send(sockfd_, byte_data + sent_total, chunk_size, MSG_NOSIGNAL);
            
            if (sent < 0)
            {
                return false;
            }

            sent_total += sent;
        }

        return true;
    }

    /*****************************************************************************
     * @brief 发送vector数据
     * @param data vector数据
     * @return true 成功, false 失败
     *****************************************************************************/
    template <typename T>
    bool sendVector(const std::vector<T> &data)
    {
        if (data.empty())
        {
            return true;
        }
        return send(data.data(), data.size() * sizeof(T));
    }

    /*****************************************************************************
     * @brief 检查是否已连接
     *****************************************************************************/
    bool isConnected() const { return is_connected_; }

    /*****************************************************************************
     * @brief 获取目标IP
     *****************************************************************************/
    std::string getDestIp() const { return dest_ip_; }

    /*****************************************************************************
     * @brief 获取目标端口
     *****************************************************************************/
    std::uint16_t getDestPort() const { return dest_port_; }

    // 禁用拷贝
    TcpSender(const TcpSender &) = delete;
    TcpSender &operator=(const TcpSender &) = delete;
};

} // namespace track_project

#endif // _TCP_SENDER_HPP_
/*****************************************************************************
 * @file TrackerComm.hpp
 * @author xjl (xjl20011009@126.com)
 * @brief 发送接收不是成对的！
 * @version 0.2
 * @date 2025-12-02
 *
 * @copyright Copyright (c) 2025
 *
 *****************************************************************************/
#ifndef _TRACKER_COMM_HPP_
#define _TRACKER_COMM_HPP_

#include <cstdint>
#include <string>
#include <mutex>
#include <chrono>
#include <atomic>
#include <thread>
#include <vector>
#include <memory>
#include <condition_variable>
#include "../utils/UdpBasic.hpp"
#include "../include/TrackConfig.hpp"
#include <defstruct.h>

namespace track_project::trackmanager
{
    // 线程安全的通信类
    class TrackerComm
    {
    public:
#pragma pack(push, 1)
        struct PacketHeader
        {
            char packet_id[128];           // 数据包ID (128字节)
            std::uint32_t total_fragments; // 总片段数
            std::uint32_t fragment_index;  // 当前片段索引
            std::uint32_t total_size;      // 总数据大小（所有分片的总大小）
            std::uint32_t fragment_size;   // 当前分片大小
            std::uint32_t checksum;        // 校验和，32位异或
        };
#pragma pack(pop)

        /*****************************************************************************
         * @brief 构造新的 Tracker Comm 对象
         * @param filepath 配置文件路径
         *****************************************************************************/
        explicit TrackerComm(const std::string &filepath);

        /*****************************************************************************
         * @brief 析构函数，停止接收数据
         *****************************************************************************/
        ~TrackerComm();

        /*****************************************************************************
         * @brief 发送数据接口（自动分包）
         * @param data 要发送的数据向量
         * @return true 所有分片发送成功
         * @return false 发送失败，原因见日志
         *****************************************************************************/
        bool sendData(const std::vector<IntFloatUnion> &data);

        /*****************************************************************************
         * @brief 读取接收到的数据，然后清空接收区
         * @return std::vector<std::uint32_t> 接收数据的向量
         *****************************************************************************/
        std::vector<std::uint32_t> readReceivedData();

        /*****************************************************************************
         * @brief 重新加载配置
         * @param filepath 新的配置文件路径
         *****************************************************************************/
        void reload(const std::string &filepath);

        /*****************************************************************************
         * @brief 获取当前接收状态
         * @return true 正在接收数据
         * @return false 已停止接收
         *****************************************************************************/
        bool isReceiving() const { return is_receiving_.load(); }

        // 禁止拷贝和赋值
        TrackerComm(const TrackerComm &) = delete;
        TrackerComm &operator=(const TrackerComm &) = delete;
        TrackerComm(TrackerComm &&) = delete;
        TrackerComm &operator=(TrackerComm &&) = delete;

    private:
        // 常量定义
        static constexpr size_t HEADER_SIZE = sizeof(PacketHeader);
        static constexpr size_t MAX_SEND_SIZE = 4096;       // 最大单个发送包大小
        static constexpr size_t MAX_RECV_SIZE = 4096;       // 最大数据大小
        static constexpr size_t RECV_BUFFER_CAPACITY = 100; // 接收缓冲区容量

        // 接收端过滤器，多余位置填0
        static constexpr char TRACK_COMMAND_ID[] = "TRACK_MERGE_COMMAND";

        // 发送端包头名称
        static constexpr char TRACK_PACKET_ID[] = "TRACK_PACKET";

        /*****************************************************************************
         * @brief 接收数据循环（在线程中运行）
         *****************************************************************************/
        void receiveLoop();

        /*****************************************************************************
         * @brief 停止循环接收数据，清空接收区
         * @param wait_complete 是否等待线程完全停止
         *****************************************************************************/
        void stopReceiveData(bool wait_complete = true);

        /*****************************************************************************
         * @brief 开始循环接收数据
         * @return true 启动成功
         * @return false 启动失败
         *****************************************************************************/
        bool startReceiveData();

        /*****************************************************************************
         * @brief 计算32位异或校验和（优化版本）
         * @param data 指向数据的指针
         * @param size 数据大小（字节）
         * @return std::uint32_t 计算得到的校验和
         *****************************************************************************/
        static std::uint32_t calculateChecksum(const void *data, size_t size);

        /*****************************************************************************
         * @brief 验证接收到的数据包
         * @param header 包头指针
         * @param payload 数据载荷指针
         * @param data_size 数据大小
         * @return true 验证通过
         * @return false 验证失败
         *****************************************************************************/
        bool validatePacket(const PacketHeader *header, const uint8_t *payload, size_t data_size);

        /*****************************************************************************
         * @brief 等待接收线程完全停止
         * @param timeout_ms 超时时间（毫秒）
         * @return true 线程已停止
         * @return false 超时
         *****************************************************************************/
        bool waitForThreadStop(int timeout_ms = 1000);

    private:
        std::string filepath_; // 配置文件路径
        TrackConfig config_;   // 配置对象

        std::unique_ptr<UdpBase> send_socket_; // 发送套接字
        std::unique_ptr<UdpBase> recv_socket_; // 接收套接字

        // 接收缓冲区，存储uint32_t数据
        std::vector<std::uint32_t> recv_buffer_;
        mutable std::mutex recv_mutex_;

        // 接收线程相关
        std::thread recv_thread_;
        std::atomic<bool> is_receiving_;
        std::atomic<bool> should_stop_;
        std::condition_variable thread_stop_cv_;
        std::mutex thread_stop_mutex_;
        bool thread_stopped_;
    };

} // namespace track_project::trackmanager

#endif // _TRACKER_COMM_HPP_
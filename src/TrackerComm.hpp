/*****************************************************************************
 * @file TrackerComm.hpp
 * @author xjl (xjl20011009@126.com)
 * @brief 航迹通信管理器
 * 特性：
 * 1. 发送大数据自动分片（UDP）
 * 2. 接收指令过滤存储
 * 3. 支持配置热重载
 *
 * @version 0.3
 * @date 2025-12-03
 *****************************************************************************/
#ifndef _TRACKER_COMM_HPP_
#define _TRACKER_COMM_HPP_

#include "TrackConfig.hpp"
#include <defstruct.h>

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <memory>
#include <chrono>

namespace track_project::trackmanager
{
    class UdpTrackerComm
    {
    private:
        /*****************************************************************************
         * @brief 包头部结构（56字节）
         *****************************************************************************/
#pragma pack(push, 1)
        struct PacketHeader
        {
            char packet_id[32];       // 包标识/指令ID
            uint32_t total_fragments; // 总分片数
            uint32_t fragment_index;  // 当前分片索引
            uint32_t total_size;      // 总数据大小（字节）
            uint32_t fragment_size;   // 当前分片大小（字节）
            uint32_t checksum;        // 异或校验和
            uint32_t reserved;        // 保留字段（对齐）
        };
#pragma pack(pop)

    public:
        /*****************************************************************************
         * @brief 构造UDP通信器，创建线程，启动时间循环
         * @param config_path 配置文件路径
         *****************************************************************************/
        explicit UdpTrackerComm(const std::string &config_path);

        /*****************************************************************************
         * @brief 析构函数，程序关闭等系统释放资源
         *****************************************************************************/
        ~UdpTrackerComm();

        // 实现接口方法
        bool requestSendTrackData(const std::vector<IntFloatUnion> &data);
        std::vector<std::uint32_t> readCommands();
        bool requestReload(const std::string &config_path);

        // 禁用拷贝和移动
        UdpTrackerComm(const UdpTrackerComm &) = delete;
        UdpTrackerComm &operator=(const UdpTrackerComm &) = delete;
        UdpTrackerComm(UdpTrackerComm &&) = delete;
        UdpTrackerComm &operator=(UdpTrackerComm &&) = delete;

    private:
        // 常量定义
        static constexpr size_t HEADER_SIZE = sizeof(PacketHeader);
        static constexpr size_t MAX_UDP_PAYLOAD = 65507;          // UDP最大包长
        static constexpr size_t MAX_FRAGMENT_SIZE = 4096;         // 单个分片最大载荷
        static constexpr size_t COMMAND_BUFFER_CAPACITY = 10000;  // 指令缓冲区容量（约10000条指令）
        static constexpr size_t COMMAND_WARNING_THRESHOLD = 8000; // 缓冲区警告阈值

        // 包标识常量
        static constexpr char TRACK_DATA_PREFIX[] = "TRACK_DATA"; // 航迹数据标识

        /*****************************************************************************
         * @brief 事务循环
         *****************************************************************************/
        void udpTransactionLoop();

        /*****************************************************************************
         * @brief 创建和绑定socket
         * @return true 创建成功
         *****************************************************************************/
        bool createAndBindSockets();

        /*****************************************************************************
         * @brief 关闭所有socket
         *****************************************************************************/
        void closeSockets();

        /*****************************************************************************
         * @brief 执行配置重载（同步）
         * @param config_path 配置文件路径
         * @return true 重载成功
         *****************************************************************************/
        bool performReload(const std::string &config_path);

        /*****************************************************************************
         * @brief 计算32位异或校验和
         * @param data 数据指针
         * @param size 数据大小
         * @return uint32_t 校验和
         *****************************************************************************/
        static uint32_t calculateXorChecksum(const void *data, size_t size);

        /*****************************************************************************
         * @brief 验证接收到的数据包
         * @param header 包头指针
         * @param payload 数据指针
         * @param payload_size 数据大小
         * @return true 验证通过
         *****************************************************************************/
        bool validatePacket(const PacketHeader *header, const uint8_t *payload, size_t payload_size);

        /*****************************************************************************
         * @brief 判断是否为有效指令（根据过滤器）
         * @param packet_id 包标识
         * @return true 是指令包
         *****************************************************************************/
        bool isCommandPacket(const std::string &packet_id) const;

    private:
        // Socket文件描述符
        int send_socket_fd_ = -1;
        int recv_socket_fd_ = -1;

        // 配置
        std::string config_path_;
        TrackConfig config_;

        // 接收缓冲区（存储原始uint32_t数据）
        std::vector<std::uint32_t> command_buffer_;
        // 发送缓冲区
        std::vector<std::vector<IntFloatUnion>> send_buffer_;
        // 重载配置文件
        bool should_reload_config_;
    };

} // namespace track_project::trackmanager

#endif // _TRACKER_COMM_HPP_
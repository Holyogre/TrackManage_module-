/*****************************************************************************
 * @file TrackerComm.cpp
 * @author xjl (xjl20011009@126.com)
 * @brief TrackerComm 实现文件
 * @version 0.2
 * @date 2025-12-02
 *
 * @copyright Copyright (c) 2025
 *
 *****************************************************************************/
#include "TrackerComm.hpp"
#include <algorithm>
#include <cstring>
#include <string_view>
#include <sstream>
#include <iomanip>
#include "../utils/Logger.hpp"

namespace track_project::trackmanager
{
    // 构造函数
    TrackerComm::TrackerComm(const std::string &filepath)
        : filepath_(filepath), // 这里改为值传递，而不是引用
          config_(filepath),
          is_receiving_(false),
          should_stop_(false),
          thread_stopped_(true)
    {
        // 创建发送socket
        send_socket_ = std::make_unique<UdpBase>(false, true, false);

        // 创建接收socket
        recv_socket_ = std::make_unique<UdpBase>(false, true, false);

        // 如果接收socket有效，启动接收线程
        if (recv_socket_ && recv_socket_->isValid())
        {
            startReceiveData();
        }
        else
        {
            LOG_ERROR << "接收socket创建失败";
        }
    }

    // 析构函数
    TrackerComm::~TrackerComm()
    {
        stopReceiveData(true); // 等待线程完全停止
    }

    // 发送数据接口
    bool TrackerComm::sendData(const std::vector<IntFloatUnion> &data)
    {
        // 参数检查
        if (data.empty())
        {
            LOG_ERROR << "发送数据为空";
            return false;
        }

        if (!send_socket_ || !send_socket_->isValid())
        {
            LOG_ERROR << "发送socket无效";
            return false;
        }

        size_t total_data_size = data.size() * sizeof(IntFloatUnion);

        // 检查总数据大小是否合理
        if (total_data_size > 1024 * 1024 * 1024) // 1GB限制
        {
            LOG_ERROR << "发送数据大小 " << total_data_size << " 过大";
            return false;
        }

        const uint8_t *raw_data = reinterpret_cast<const uint8_t *>(data.data());

        // 计算需要多少分片
        size_t max_payload = MAX_SEND_SIZE - HEADER_SIZE; // 扣除包头大小
        size_t total_fragments = (total_data_size + max_payload - 1) / max_payload;

        LOG_DEBUG << "发送数据大小: " << total_data_size
                  << " 字节, 需要分片: " << total_fragments
                  << ", 每片最大载荷: " << max_payload << " 字节";

        // 循环发送每个分片
        bool all_fragments_sent = true;
        for (size_t fragment_index = 0; fragment_index < total_fragments; ++fragment_index)
        {
            // 计算当前分片的偏移和大小
            size_t offset = fragment_index * max_payload;
            size_t fragment_size = std::min(max_payload, total_data_size - offset);

            // 创建发送缓冲区
            std::vector<uint8_t> send_buffer(HEADER_SIZE + fragment_size);

            // 准备包头
            PacketHeader *header = reinterpret_cast<PacketHeader *>(send_buffer.data());

            // 填充包ID
            memset(header->packet_id, 0, sizeof(header->packet_id));
            std::string packet_id = std::string(TRACK_PACKET_ID) + "_" +
                                    std::to_string(fragment_index) + "_" +
                                    std::to_string(total_fragments);
            size_t copy_len = std::min(packet_id.length(), sizeof(header->packet_id) - 1);
            strncpy(header->packet_id, packet_id.c_str(), copy_len);

            header->total_fragments = static_cast<uint32_t>(total_fragments);
            header->fragment_index = static_cast<uint32_t>(fragment_index);
            header->total_size = static_cast<uint32_t>(total_data_size);  // 总数据大小
            header->fragment_size = static_cast<uint32_t>(fragment_size); // 当前分片大小
            header->checksum = calculateChecksum(raw_data + offset, fragment_size);

            // 拷贝当前分片的数据
            memcpy(send_buffer.data() + HEADER_SIZE, raw_data + offset, fragment_size);

            // 发送当前分片
            bool fragment_success = send_socket_->sendTo(
                send_buffer.data(),
                send_buffer.size(),
                reinterpret_cast<const sockaddr *>(&config_.trackmanager_dst_sockaddr),
                sizeof(config_.trackmanager_dst_sockaddr));

            if (!fragment_success)
            {
                LOG_ERROR << "分片 " << fragment_index << "/" << total_fragments
                          << " 发送失败, 大小: " << fragment_size << " 字节";
                all_fragments_sent = false;
                break; // 一个分片失败就停止发送
            }

            LOG_DEBUG << "成功发送分片 " << fragment_index << "/" << total_fragments
                      << ", 大小: " << fragment_size << " 字节";

            // 小延迟避免网络拥塞（仅当有多个分片时）
            if (total_fragments > 1 && fragment_index < total_fragments - 1)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }

        if (all_fragments_sent)
        {
            LOG_INFO << "成功发送 " << total_fragments << " 个分片, 总大小: "
                     << total_data_size << " 字节";
        }
        else
        {
            LOG_ERROR << "数据发送失败，可能部分分片已发送";
        }

        return all_fragments_sent;
    }

    // 读取接收到的数据
    std::vector<std::uint32_t> TrackerComm::readReceivedData()
    {
        std::lock_guard<std::mutex> lock(recv_mutex_);

        std::vector<std::uint32_t> result;
        result.swap(recv_buffer_);

        // 不清留容量，直接重新分配（更简单）
        // recv_buffer_ 会在下次插入时自动分配

        return result;
    }

    // 重新加载配置
    void TrackerComm::reload(const std::string &filepath)
    {
        LOG_INFO << "重新加载配置: " << filepath;

        // 停止接收数据，等待线程完全停止
        stopReceiveData(true);

        // 检查线程是否完全停止
        if (!waitForThreadStop(2000)) // 等待2秒
        {
            LOG_ERROR << "接收线程停止超时";
            return;
        }

        // 更新配置
        filepath_ = filepath;
        if (!config_.reload(filepath))
        {
            LOG_ERROR << "配置重载失败，保持旧配置";
        }

        // 清空接收缓冲区
        {
            std::lock_guard<std::mutex> lock(recv_mutex_);
            recv_buffer_.clear();
            recv_buffer_.reserve(RECV_BUFFER_CAPACITY);
        }

        // 重新创建接收socket（使用新配置）
        recv_socket_ = std::make_unique<UdpBase>(false, true, false);

        // 重新启动接收
        startReceiveData();

        LOG_INFO << "配置重载完成";
    }

    // 等待接收线程完全停止
    bool TrackerComm::waitForThreadStop(int timeout_ms)
    {
        std::unique_lock<std::mutex> lock(thread_stop_mutex_);
        return thread_stop_cv_.wait_for(lock,
                                        std::chrono::milliseconds(timeout_ms),
                                        [this]
                                        { return thread_stopped_; });
    }

    // 接收数据循环
    void TrackerComm::receiveLoop()
    {
        LOG_INFO << "接收线程启动";

        std::vector<uint8_t> recv_buffer(HEADER_SIZE + MAX_RECV_SIZE);

        while (!should_stop_.load())
        {
            if (!recv_socket_ || !recv_socket_->isValid())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // 非阻塞接收
            ssize_t received = recv_socket_->receiveFrom(recv_buffer.data(), recv_buffer.size());

            if (received <= 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            // 检查包大小
            if (static_cast<size_t>(received) < HEADER_SIZE)
            {
                LOG_ERROR << "收到包太小: " << received << " 字节";
                continue;
            }

            // 解析包头
            const PacketHeader *header = reinterpret_cast<const PacketHeader *>(recv_buffer.data());
            size_t data_size = static_cast<size_t>(received) - HEADER_SIZE;
            const uint8_t *payload = recv_buffer.data() + HEADER_SIZE;

            // 验证数据包
            if (!validatePacket(header, payload, data_size))
            {
                continue;
            }

            // 处理数据：将数据转换为uint32_t数组存储
            size_t uint32_count = data_size / sizeof(std::uint32_t);
            if (data_size % sizeof(std::uint32_t) != 0)
            {
                LOG_ERROR << "数据大小不是4字节的整数倍: " << data_size;
                continue;
            }

            std::lock_guard<std::mutex> lock(recv_mutex_);

            // 检查缓冲区容量
            if (recv_buffer_.size() + uint32_count > RECV_BUFFER_CAPACITY)
            {
                LOG_ERROR << "接收缓冲区将满，丢弃数据。当前: "
                          << recv_buffer_.size() << "/" << RECV_BUFFER_CAPACITY;
                continue;
            }

            // 将数据转换为uint32_t并存储
            const std::uint32_t *uint32_data = reinterpret_cast<const std::uint32_t *>(payload);
            recv_buffer_.insert(recv_buffer_.end(), uint32_data, uint32_data + uint32_count);

            LOG_DEBUG << "成功接收 " << data_size << " 字节数据, 转换为 "
                      << uint32_count << " 个uint32_t";
        }

        // 线程即将退出，更新状态
        {
            std::lock_guard<std::mutex> lock(thread_stop_mutex_);
            thread_stopped_ = true;
            is_receiving_.store(false);
            thread_stop_cv_.notify_all();
        }

        LOG_INFO << "接收线程退出";
    }

    // 停止接收数据
    void TrackerComm::stopReceiveData(bool wait_complete)
    {
        LOG_INFO << "停止接收数据" << (wait_complete ? "（等待完成）" : "（不等待）");

        // 设置停止标志
        should_stop_.store(true);

        // 更新线程状态
        {
            std::lock_guard<std::mutex> lock(thread_stop_mutex_);
            thread_stopped_ = false;
        }

        // 清空接收缓冲区
        {
            std::lock_guard<std::mutex> lock(recv_mutex_);
            recv_buffer_.clear();
        }

        if (wait_complete && recv_thread_.joinable())
        {
            // 等待线程结束
            recv_thread_.join();
        }
        else if (recv_thread_.joinable())
        {
            // 不等待，分离线程让它自己结束
            recv_thread_.detach();
        }

        is_receiving_.store(false);
    }

    // 开始接收数据
    bool TrackerComm::startReceiveData()
    {
        if (is_receiving_.load())
        {
            LOG_ERROR << "已经在接收数据";
            return true;
        }

        if (!recv_socket_ || !recv_socket_->isValid())
        {
            LOG_ERROR << "接收socket无效";
            return false;
        }

        // 重置停止标志
        should_stop_.store(false);

        // 更新线程状态
        {
            std::lock_guard<std::mutex> lock(thread_stop_mutex_);
            thread_stopped_ = false;
        }

        // 启动接收线程
        try
        {
            recv_thread_ = std::thread(&TrackerComm::receiveLoop, this);
            is_receiving_.store(true);
            LOG_INFO << "启动接收数据";
            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "启动接收线程失败: " << e.what();
            is_receiving_.store(false);
            return false;
        }
    }

    // 计算校验和（优化版本）
    std::uint32_t TrackerComm::calculateChecksum(const void *data, size_t size)
    {
        if (!data || size == 0)
        {
            return 0;
        }

        const uint8_t *bytes = static_cast<const uint8_t *>(data);
        uint32_t checksum = 0;

        // 对齐到4字节边界
        const uint32_t *words = reinterpret_cast<const uint32_t *>(data);
        size_t word_count = size / 4;

        // 按4字节处理
        for (size_t i = 0; i < word_count; ++i)
        {
            checksum ^= words[i];
        }

        // 处理剩余字节（0-3个字节）
        size_t remaining = size % 4;
        if (remaining > 0)
        {
            uint32_t last_word = 0;
            const uint8_t *tail = bytes + (word_count * 4);

            // 按照小端序处理剩余字节
            switch (remaining)
            {
            case 3:
                last_word |= static_cast<uint32_t>(tail[2]) << 16;
                // fallthrough
            case 2:
                last_word |= static_cast<uint32_t>(tail[1]) << 8;
                // fallthrough
            case 1:
                last_word |= static_cast<uint32_t>(tail[0]);
                break;
            }

            checksum ^= last_word;
        }

        return checksum;
    }

    // 验证数据包
    bool TrackerComm::validatePacket(const PacketHeader *header, const uint8_t *payload, size_t data_size)
    {
        // 验证数据大小
        if (data_size > MAX_RECV_SIZE)
        {
            LOG_ERROR << "接收数据大小 " << data_size << " 超过最大值";
            return false;
        }

        // 验证包ID过滤器
        std::string packet_id(header->packet_id, sizeof(header->packet_id));

        // 查找第一个非空字符位置
        size_t first_non_null = packet_id.find_first_not_of('\0');
        if (first_non_null == std::string::npos)
        {
            LOG_ERROR << "包ID为空";
            return false;
        }

        // 截取实际包ID（去除尾部空字符）
        packet_id = packet_id.substr(0, packet_id.find('\0', first_non_null));

        // 检查是否包含TRACK_COMMAND_ID - 查找第一个字符
        if (packet_id.empty() || packet_id.find(TRACK_COMMAND_ID) == std::string::npos)
        {
            LOG_DEBUG << "包ID不包含命令标识，丢弃: "
                      << (packet_id.length() > 32 ? packet_id.substr(0, 32) + "..." : packet_id);
            return false;
        }

        // 验证校验和
        uint32_t calculated_checksum = calculateChecksum(payload, data_size);
        if (header->checksum != calculated_checksum)
        {
            LOG_ERROR << "校验和错误: 包头=" << header->checksum
                      << ", 计算=" << calculated_checksum;
            return false;
        }

        // 验证分片大小与数据大小匹配
        if (header->fragment_size != static_cast<uint32_t>(data_size))
        {
            LOG_ERROR << "分片大小不匹配: 包头=" << header->fragment_size
                      << ", 实际=" << data_size;
            return false;
        }

        // 验证分片索引
        if (header->fragment_index >= header->total_fragments)
        {
            LOG_ERROR << "分片索引越界: index=" << header->fragment_index
                      << ", total=" << header->total_fragments;
            return false;
        }

        LOG_DEBUG << "验证通过: " << packet_id
                  << ", 分片 " << header->fragment_index << "/" << header->total_fragments
                  << ", 大小 " << data_size << " 字节";

        return true;
    }

} // namespace track_project::trackmanager
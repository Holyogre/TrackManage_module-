/*****************************************************************************
 * @file UdpBasic.hpp
 * @author xjl (xjl20011009@126.com，DeepSeek写的)
 * @brief 通用UDP基类，仅用于LINUX系统
 * 1. 不包含广播功能（IPv6不兼容，需时通过继承添加）
 * 2. 组播使用通用接口（自动区分IPv4/IPv6）
 * 3. 支持IPv4和IPv6双协议栈
 * 4. 支持移动语义，禁用拷贝语义
 *
 * @version 0.3
 * @date 2025-12-02
 *****************************************************************************/
#ifndef _UDP_BASE_HPP_
#define _UDP_BASE_HPP_

// 标准库
#include <string>
#include <cstdlib>
#include <cstdint>
#include <cstring>
// 系统库
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <net/if.h>

class UdpBase
{
    // 地址类型枚举
    enum class AddressType
    {
        IPv4,       ///< 使用IPv4协议（AF_INET）
        IPv6,       ///< 使用IPv6协议（AF_INET6）
        Unspecified ///< 未指定（初始状态或错误状态）
    };

public:
    /*****************************************************************************
     * @brief 构造函数
     *
     * @param use_ipv6 是否使用IPv6协议
     *        - true: 创建IPv6 socket (AF_INET6)
     *        - false: 创建IPv4 socket (AF_INET) [默认]
     *
     * @param reuse_addr 是否启用地址重用（SO_REUSEADDR）
     *        - true: 允许立即重用地址（快速重启）[默认]
     *        - false: 遵循标准TIME_WAIT规则
     *
     * @param non_blocking 是否设为非阻塞模式
     *        - true: 非阻塞，sendto()/recvfrom()立即返回
     *        - false: 阻塞模式，等待操作完成 [默认]
     *        ⚠️注意：非阻塞模式下需检查EAGAIN/EWOULDBLOCK错误
     *
     * @异常：构造函数不会抛出异常，但可通过isValid()检查是否创建成功
     *****************************************************************************/
    explicit UdpBase(bool use_ipv6 = false,
                     bool reuse_addr = true,
                     bool non_blocking = false)
    {
        // 选择协议族：IPv4或IPv6
        int domain = use_ipv6 ? AF_INET6 : AF_INET;
        address_type_ = use_ipv6 ? AddressType::IPv6 : AddressType::IPv4;

        // 创建UDP socket
        sockfd_ = ::socket(domain, SOCK_DGRAM, 0);
        if (sockfd_ < 0)
        {
            // socket创建失败，保持sockfd_ = -1
            // 调用者应检查isValid()或处理错误
            return;
        }

        // 设置地址重用选项（快速重启）
        if (reuse_addr)
        {
            int opt = 1;
            // IPv4地址重用
            ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            // IPv6需要额外设置（仅IPv6模式有效）
            if (use_ipv6)
            {
                // IPV6_V6ONLY：设为1时只处理IPv6，0时可双栈
                ::setsockopt(sockfd_, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
            }
        }

        // 设置非阻塞模式
        if (non_blocking)
        {
            int flags = ::fcntl(sockfd_, F_GETFL, 0);
            if (flags >= 0)
            {
                ::fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK);
            }
        }
    }

    /*****************************************************************************
     * @brief 析构函数 - 自动清理资源
     *
     * @注意：遵循RAII原则，对象销毁时自动关闭socket
     *****************************************************************************/
    virtual ~UdpBase()
    {
        close();
    }

    /*****************************************************************************
     * @brief 设置发送缓冲区大小，发送速度不足时可调大
     *
     * @param size 期望的缓冲区大小（字节）
     * @return true 设置成功（实际大小可能被系统调整）
     * @return false 设置失败（socket无效或参数错误）
     *****************************************************************************/
    bool setSendBufferSize(int size)
    {
        if (sockfd_ < 0 || size <= 0)
            return false;
        return ::setsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == 0;
    }

    /*****************************************************************************
     * @brief 设置接收缓冲区大小，接收包丢失时可调大
     *
     * @param size 期望的缓冲区大小（字节）
     * @return true 设置成功（实际大小可能被系统调整）
     * @return false 设置失败（socket无效或参数错误）
     *****************************************************************************/
    bool setReceiveBufferSize(int size)
    {
        if (sockfd_ < 0 || size <= 0)
            return false;
        return ::setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF,
                            &size, sizeof(size)) == 0;
    }

    /*****************************************************************************
     * @brief 通用数据发送方法
     *
     * @param data 要发送的数据指针
     * @param size 数据大小（字节），必须 <= 65507（UDP最大包长）
     * @param addr 目标地址结构指针（sockaddr_in或sockaddr_in6）
     * @param addr_len 地址结构长度（sizeof(sockaddr_in)等）
     * @return true 发送成功（所有数据已被内核接受）
     * @return false 发送失败（socket无效、参数错误或网络错误）
     *****************************************************************************/
    bool sendTo(const void *data, size_t size,
                const sockaddr *addr, socklen_t addr_len)
    {
        if (sockfd_ < 0 || data == nullptr || size == 0 || addr == nullptr)
        {
            return false;
        }

        // 发送数据，MSG_NOSIGNAL避免SIGPIPE信号（Linux特有）
        ssize_t sent = ::sendto(sockfd_, data, size, MSG_NOSIGNAL, addr, addr_len);
        return sent == static_cast<ssize_t>(size);
    }

    /*****************************************************************************
     * @brief 通用数据接收方法
     * 1. 阻塞模式下会等待直到数据到达或出错
     * 2. 非阻塞模式下立即返回，无数据时返回-1且errno=EAGAIN/EWOULDBLOCK
     * @param buffer 接收缓冲区指针
     * @param buffer_size 缓冲区大小（字节），建议为 65507（UDP最大包长），否则可能截断 UDP 包
     * @return ssize_t 实际接收的字节数
     *         >=0: 成功接收的数据大小
     *         -1: 接收失败（检查errno）
     *****************************************************************************/
    ssize_t receiveFrom(void *buffer, size_t buffer_size)
    {
        if (sockfd_ < 0 || buffer == nullptr || buffer_size == 0)
        {
            return -1;
        }

        // 使用sockaddr_storage可以接收任意类型的地址
        sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);

        ssize_t received = ::recvfrom(sockfd_, buffer, buffer_size, 0,
                                      reinterpret_cast<sockaddr *>(&addr), &addr_len);
        return received;
    }

    /*****************************************************************************
     * @brief 加入组播组（通用接口）
     *
     * @param group_ip 组播地址
     *        - IPv4: 224.0.0.0 ~ 239.255.255.255（建议用239.x.x.x本地范围）
     *        - IPv6: FF00::/8（建议用FF02::1等链路本地地址）
     *
     * @param interface_ip 本地网络接口标识
     *        - IPv4: 接口的IP地址（如"192.168.1.100"）或空字符串（默认接口）
     *        - IPv6: 接口名称（如"eth0"）或空字符串（默认接口）
     *        ⚠️注意：IPv4和IPv6的参数类型不同，这是协议设计差异
     *
     * @return true 加入成功
     * @return false 加入失败（地址无效、接口不存在或无权限）
     *****************************************************************************/
    bool joinMulticastGroup(const std::string &group_ip,
                            const std::string &interface_ip = "")
    {
        if (sockfd_ < 0)
            return false;

        // 根据socket类型调用对应的实现
        if (address_type_ == AddressType::IPv4)
        {
            return joinIPv4MulticastGroup(group_ip, interface_ip);
        }
        else if (address_type_ == AddressType::IPv6)
        {
            return joinIPv6MulticastGroup(group_ip, interface_ip);
        }

        return false; // 地址类型未知
    }

    /*****************************************************************************
     * @brief 离开组播组
     *
     * @param group_ip 要离开的组播地址
     * @return true 离开成功
     * @return false 离开失败（未加入或参数错误）
     *
     * @注意：通常不是必须调用，socket关闭时会自动离开所有组播组
     *****************************************************************************/
    bool leaveMulticastGroup(const std::string &group_ip)
    {
        if (sockfd_ < 0)
            return false;

        if (address_type_ == AddressType::IPv4)
        {
            return leaveIPv4MulticastGroup(group_ip);
        }
        else if (address_type_ == AddressType::IPv6)
        {
            return leaveIPv6MulticastGroup(group_ip);
        }

        return false;
    }

    /*****************************************************************************
     * @brief 关闭socket并释放资源
     *
     * @注意：
     * 1. 可多次调用，重复调用无害
     * 2. 关闭后socket不再可用，需重新创建
     * 3. 析构函数会自动调用此方法
     *****************************************************************************/
    void close()
    {
        if (sockfd_ >= 0)
        {
            ::close(sockfd_);
            sockfd_ = -1;
            address_type_ = AddressType::Unspecified;
        }
    }

    /*****************************************************************************
     * @brief 检查socket是否有效
     *
     * @return true socket已创建且有效
     * @return false socket未创建或已关闭
     *
     * @建议：构造函数后立即检查，避免后续操作失败
     *****************************************************************************/
    [[nodiscard]] bool isValid() const { return sockfd_ >= 0; }

    // ==================== 禁止拷贝，支持移动 ====================
    UdpBase(const UdpBase &) = delete;
    UdpBase &operator=(const UdpBase &) = delete;

    UdpBase(UdpBase &&other) noexcept
        : sockfd_(other.sockfd_),
          address_type_(other.address_type_)
    {
        other.sockfd_ = -1;
        other.address_type_ = AddressType::Unspecified;
    }

    UdpBase &operator=(UdpBase &&other) noexcept
    {
        if (this != &other)
        {
            close(); // 关闭当前socket
            sockfd_ = other.sockfd_;
            address_type_ = other.address_type_;
            other.sockfd_ = -1;
            other.address_type_ = AddressType::Unspecified;
        }
        return *this;
    }

protected:
    /*****************************************************************************
     * @brief IPv4组播加入实现
     * @return true 加入成功
     * @return false 加入失败（典型原因：组播地址无效、接口地址无效、权限不足等）
     *****************************************************************************/
    bool joinIPv4MulticastGroup(const std::string &group_ip,
                                const std::string &interface_ip)
    {
        ip_mreq mreq{};
        ::memset(&mreq, 0, sizeof(mreq));

        // 设置组播组地址
        if (::inet_pton(AF_INET, group_ip.c_str(), &mreq.imr_multiaddr) <= 0)
        {
            return false; // 组播地址无效
        }

        // 设置网络接口
        if (!interface_ip.empty())
        {
            // 指定具体接口的IP地址
            if (::inet_pton(AF_INET, interface_ip.c_str(), &mreq.imr_interface) <= 0)
            {
                return false; // 接口地址无效
            }
        }
        else
        {
            // 使用默认接口（INADDR_ANY）
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        }

        return ::setsockopt(sockfd_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                            &mreq, sizeof(mreq)) == 0;
    }

    /*****************************************************************************
     * @brief IPv6组播加入实现
     * @return true 加入成功
     * @return false 加入失败（典型原因：组播地址无效、接口无效、权限不足等）
     *****************************************************************************/
    bool joinIPv6MulticastGroup(const std::string &group_ip,
                                const std::string &interface_ip)
    {
        ipv6_mreq mreq{};
        ::memset(&mreq, 0, sizeof(mreq));

        // 设置组播组地址
        if (::inet_pton(AF_INET6, group_ip.c_str(), &mreq.ipv6mr_multiaddr) <= 0)
        {
            return false; // 组播地址无效
        }

        // 设置网络接口（IPv6使用接口索引，不是IP地址）
        if (!interface_ip.empty())
        {
            // 将接口名称转换为索引（如"eth0" -> 2）
            unsigned int ifindex = ::if_nametoindex(interface_ip.c_str());
            if (ifindex == 0)
            {
                // 接口不存在，尝试解析为数字索引
                char *endptr = nullptr;
                ifindex = std::strtoul(interface_ip.c_str(), &endptr, 10);
                if (endptr == interface_ip.c_str() || ifindex == 0)
                {
                    return false; // 接口无效
                }
            }
            mreq.ipv6mr_interface = ifindex;
        }
        else
        {
            // 使用默认接口（索引0）
            mreq.ipv6mr_interface = 0;
        }

        return ::setsockopt(sockfd_, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                            &mreq, sizeof(mreq)) == 0;
    }

    /*****************************************************************************
     * @brief 离开IPv4组播组实现
     *****************************************************************************/
    bool leaveIPv4MulticastGroup(const std::string &group_ip)
    {
        ip_mreq mreq{};
        ::memset(&mreq, 0, sizeof(mreq));

        if (::inet_pton(AF_INET, group_ip.c_str(), &mreq.imr_multiaddr) <= 0)
        {
            return false;
        }

        // 离开组播组时通常使用默认接口
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        return ::setsockopt(sockfd_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                            &mreq, sizeof(mreq)) == 0;
    }

    /*****************************************************************************
     * @brief 离开IPv6组播组实现
     *****************************************************************************/
    bool leaveIPv6MulticastGroup(const std::string &group_ip)
    {
        ipv6_mreq mreq{};
        ::memset(&mreq, 0, sizeof(mreq));

        if (::inet_pton(AF_INET6, group_ip.c_str(), &mreq.ipv6mr_multiaddr) <= 0)
        {
            return false;
        }

        // 使用默认接口离开
        mreq.ipv6mr_interface = 0;

        return ::setsockopt(sockfd_, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
                            &mreq, sizeof(mreq)) == 0;
    }

private:
    int sockfd_ = -1;                                     ///< Socket文件描述符，-1表示无效
    AddressType address_type_ = AddressType::Unspecified; ///< 当前使用的地址类型
}; // class UdpBase

#endif // _UDP_BASE_HPP_
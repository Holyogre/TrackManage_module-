/*****************************************************************************
 * @file config.hpp
 * @author xjl (xjl20011009@126.com)
 *
 * @brief 配置文件读取工具类
 * 原子化读取航迹管理器配置项
 * 记录日志
 *
 * @version 0.3
 * @date 2025-11-07
 *
 * @copyright Copyright (c) 2025
 *****************************************************************************/
#ifndef _TRACK_CONFIG_HPP_
#define _TRACK_CONFIG_HPP_

// 基础结构
#include <string>
#include <cstdint> //精确位数整数
// 文件控制
#include <fstream>
// 算法函数
#include <algorithm>
// 字符处理
#include <cctype>
// 网络通信
#include <netinet/in.h>
#include <arpa/inet.h>
// 日志库
#include "../utils/Logger.hpp"

namespace track_project
{

    class TrackConfig
    {
    public:
        // === 对外暴露的可读配置项 ===
        std::string track_dst_ip = "127.0.0.1";
        std::uint16_t trackmanager_dst_port = 5555;
        std::uint16_t trackmanager_recv_port = 5556;
        sockaddr_in dst_sockaddr{}; // 预解析网络结构

    public:
        //======== 构造函数 ========
        explicit TrackConfig(const std::string &filepath)
        {
            reload(filepath);
        }

        //======== 重载配置 ========
        bool reload(const std::string &filepath)
        {
            std::ifstream file(filepath);
            if (!file.is_open())
            {
                LOG_ERROR << "无法打开配置文件: " << filepath << "，使用默认配置";
                return false;
            }

            // 旧配置用于回滚
            auto old = *this;

            bool parse_error = false;
            std::string line;

            while (std::getline(file, line))
            {
                trim(line);
                if (line.empty() || line[0] == '#')
                    continue;

                size_t pos = line.find('=');
                if (pos == std::string::npos)
                    continue;

                std::string key = trim_copy(line.substr(0, pos));
                std::string value = trim_copy(line.substr(pos + 1));

                if (!applyKeyValue(key, value))
                {
                    parse_error = true;
                }
            }

            // 基本合法性验证
            if (!isValid())
            {
                LOG_ERROR << "配置验证失败，回滚旧配置";
                *this = old;
                return false;
            }

            // 预解析 IP → sockaddr_in
            if (!updateSockaddr())
            {
                LOG_ERROR << "IP 地址格式错误，回滚旧配置";
                *this = old;
                return false;
            }

            LOG_INFO << "配置文件重载成功: " << filepath;
            return !parse_error; // 有解析错误但最终可用，也算成功
        }

    private:

        //=== 去除首尾空白 ===
        static void trim(std::string &s)
        {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch)
                                            { return !std::isspace(ch); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch)
                                 { return !std::isspace(ch); })
                        .base(),
                    s.end());
        }

        static std::string trim_copy(std::string s)
        {
            trim(s);
            return s;
        }

        //=== 解析并应用 key=value ===
        bool applyKeyValue(const std::string &key, const std::string &value)
        {
            try
            {
                if (key == "TRACK_DST_IP")
                {
                    track_dst_ip = value;
                }
                else if (key == "TRACK_DST_PORT")
                {
                    trackmanager_dst_port = static_cast<std::uint16_t>(std::stoi(value));
                }
                else if (key == "TRACK_RECV_PORT")
                {
                    trackmanager_recv_port = static_cast<std::uint16_t>(std::stoi(value));
                }
                else
                {
                    LOG_INFO << "未知配置项: " << key << " = " << value;
                }
            }
            catch (const std::exception &e)
            {
                LOG_ERROR << "配置项解析失败 [key=" << key
                          << ", value=" << value
                          << "]: " << e.what();
                return false;
            }
            return true;
        }

        //=== 配置合法性检查 ===
        bool isValid() const
        {
            if (track_dst_ip.empty())
            {
                LOG_ERROR << "目标 IP 为空";
                return false;
            }
            if (trackmanager_dst_port == 0 || trackmanager_recv_port == 0)
            {
                LOG_ERROR << "端口不能为 0";
                return false;
            }
            if (trackmanager_dst_port == trackmanager_recv_port)
            {
                LOG_ERROR << "发送端口和接收端口不能相同";
                return false;
            }
            return true;
        }

        //=== 预构造 sockaddr_in ===
        bool updateSockaddr()
        {
            dst_sockaddr = {}; // 清零
            dst_sockaddr.sin_family = AF_INET;
            dst_sockaddr.sin_port = htons(trackmanager_dst_port);

            int res = inet_pton(AF_INET, track_dst_ip.c_str(), &dst_sockaddr.sin_addr);
            if (res != 1)
            {
                LOG_ERROR << "IP 地址格式非法：" << track_dst_ip;
                return false;
            }
            return true;
        }
    };

} // namespace track_project

#endif // _TRACK_CONFIG_HPP_
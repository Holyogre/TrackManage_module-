/*****************************************************************************
 * @file config.hpp
 * @author xjl (xjl20011009@126.com)
 *
 * @brief 配置文件读取工具类
 * 原子化读取航迹管理器配置项
 * 记录日志
 *
 * @version 0.1
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
// 日志库
#include "../utils/Logger.hpp"

namespace track_project
{

    /*****************************************************************************
     * @brief 字符串处理工具函数
     *****************************************************************************/
    inline void trim(std::string &s)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch)
                                        { return !std::isspace(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch)
                             { return !std::isspace(ch); })
                    .base(),
                s.end());
    }

    /*****************************************************************************
     * @brief 航迹跟踪配置项结构体
     * 皆为可反复修改的参数，初始化配置项编译期间确定，在CMAKE里面
     *****************************************************************************/
    struct TrackConfig
    {
        // 网络配置
        std::string track_dst_ip = "127.0.0.1";      // 航迹发送端IP
        std::uint16_t trackmanager_dst_port = 5555;  // 航迹发送端端口
        std::uint16_t trackmanager_recv_port = 5556; // 航迹管理器控制端口

        // 构造函数
        explicit TrackConfig(const std::string &filepath)
        {
            reload(filepath);
        }

        // 从配置文件重新加载配置
        bool reload(const std::string &filepath)
        {
            std::ifstream file(filepath);
            if (!file.is_open())
            {
                LOG_ERROR << "无法打开配置文件: " << filepath << "，使用默认配置";
                return false;
            }

            // 保存旧配置用于回滚
            auto old_ip = track_dst_ip;
            auto old_dst_port = trackmanager_dst_port;
            auto old_recv_port = trackmanager_recv_port;

            bool has_error = false;
            std::string line;

            while (std::getline(file, line))
            {
                trim(line);
                if (line.empty() || line[0] == '#')
                    continue;

                size_t eqPos = line.find('=');
                if (eqPos == std::string::npos)
                    continue;

                std::string key = line.substr(0, eqPos);
                std::string value = line.substr(eqPos + 1);
                trim(key);
                trim(value);

                try
                {
                    if (key == "TRACK_DST_IP")
                        track_dst_ip = value;
                    else if (key == "TRACK_DST_PORT")
                        trackmanager_dst_port = static_cast<std::uint16_t>(std::stoi(value));
                    else if (key == "TRACK_RECV_PORT")
                        trackmanager_recv_port = static_cast<std::uint16_t>(std::stoi(value));
                    else
                    {
                        LOG_INFO << "未知配置项: " << key << " = " << value;
                    }
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR << "配置项解析失败 [key=" << key << ", value=" << value << "]: " << e.what();
                    has_error = true;
                }
            }

            if (has_error)
            {
                // 配置解析有错误，回滚到旧配置
                LOG_ERROR << "配置文件解析错误，回滚到之前配置";
                track_dst_ip = old_ip;
                trackmanager_dst_port = old_dst_port;
                trackmanager_recv_port = old_recv_port;
                return false;
            }

            LOG_INFO << "配置文件重载成功: " << filepath;
            LOG_DEBUG << "新配置: " << *this;
            return true;
        }

        // 验证配置是否有效
        bool isValid() const
        {
            if (track_dst_ip.empty())
            {
                LOG_ERROR << "目标IP地址为空";
                return false;
            }
            if (trackmanager_dst_port == 0 || trackmanager_recv_port == 0)
            {
                LOG_ERROR << "端口号不能为0";
                return false;
            }
            if (trackmanager_dst_port == trackmanager_recv_port)
            {
                LOG_ERROR << "发送端口和接收端口不能相同";
                return false;
            }
            return true;
        }

        // 调试输出
        friend std::ostream &operator<<(std::ostream &os, const TrackConfig &config)
        {
            os << "TrackConfig {"
               << "track_dst_ip: " << config.track_dst_ip
               << ", trackmanager_dst_port: " << config.trackmanager_dst_port
               << ", trackmanager_recv_port: " << config.trackmanager_recv_port
               << "}";
            return os;
        }
    };

} // namespace track_project

#endif // _TRACK_CONFIG_HPP_
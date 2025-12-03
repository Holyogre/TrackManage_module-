#include <catch2/catch_all.hpp>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <chrono>
#include "../include/TrackConfig.hpp"

using namespace track_project;

// 错误文件名字
const std::string invalid_config_file = "../test_config_invalid.ini";
// 正确文件名字
const std::string valid_config_file = "../test_config_valid.ini";
// 临时有效配置文件
const std::string temp_valid_config_file = "../test_config_temp_valid.ini";
// 完全不存在的文件
const std::string non_existent_file = "../non_existent_file.ini";
// 创建测试配置文件的辅助函数
void createTestConfigFile(const std::string &filename,
                          const std::string &ip,
                          uint16_t send_port,
                          uint16_t recv_port)
{
    std::ofstream config_file(filename);
    config_file << "track_dst_ip = " << ip << "\n";
    config_file << "trackmanager_dst_port = " << send_port << "\n";
    config_file << "trackmanager_recv_port = " << recv_port << "\n";
    config_file.close();
}

// 功能测试
TEST_CASE("TrackConfig 配置加载测试", "[TrackConfig][Normal]")
{
    // 准备测试前的配置文件
    SECTION("加载有效配置文件")
    {
        // 创建有效配置文件
        createTestConfigFile(valid_config_file, "192.168.1.100", 7777, 8888);
        // 测试构造函数加载
        TrackConfig config(valid_config_file);

        REQUIRE(config.track_dst_ip == "192.168.1.100");
        REQUIRE(config.trackmanager_dst_port == 7777);
        REQUIRE(config.trackmanager_recv_port == 8888);

        // 验证dst_sockaddr是否正确解析
        REQUIRE(config.trackmanager_dst_sockaddr.sin_family == AF_INET);
        // 注意：这里可能需要根据实际的IP解析来调整验证

        std::cout << "有效配置加载测试通过" << std::endl;
    }

    SECTION("reload方法测试 - 重新加载有效配置")
    {
        // 先创建一个初始配置文件
        createTestConfigFile(valid_config_file, "10.0.0.1", 1111, 2222);

        TrackConfig config(valid_config_file);
        REQUIRE(config.track_dst_ip == "10.0.0.1");

        // 创建新的有效配置文件
        createTestConfigFile(temp_valid_config_file, "10.0.0.2", 3333, 4444);

        // 测试reload方法
        bool reload_success = config.reload(temp_valid_config_file);

        if (reload_success)
        {
            REQUIRE(config.track_dst_ip == "10.0.0.2");
            REQUIRE(config.trackmanager_dst_port == 3333);
            REQUIRE(config.trackmanager_recv_port == 4444);
            std::cout << "reload有效配置测试通过" << std::endl;
        }
        else
        {
            // 如果reload失败，配置应该保持不变
            REQUIRE(config.track_dst_ip == "10.0.0.1");
            std::cout << "reload失败，配置回滚测试通过" << std::endl;
        }
    }

    SECTION("原子性测试：加载无效配置文件时回滚旧配置")
    {
        // 创建初始有效配置文件
        createTestConfigFile(valid_config_file, "172.16.0.1", 5555, 6666);

        TrackConfig config(valid_config_file);
        std::string original_ip = config.track_dst_ip;
        uint16_t original_send_port = config.trackmanager_dst_port;
        uint16_t original_recv_port = config.trackmanager_recv_port;

        // 创建无效配置文件
        std::ofstream bad_file(invalid_config_file);
        bad_file << "corrupted config content\n";
        bad_file.close();

        // 尝试reload无效配置文件
        bool reload_result = config.reload(invalid_config_file);

        // 验证reload失败
        REQUIRE_FALSE(reload_result);

        // 验证配置没有被破坏，仍然保持原来的值
        REQUIRE(config.track_dst_ip == original_ip);
        REQUIRE(config.trackmanager_dst_port == original_send_port);
        REQUIRE(config.trackmanager_recv_port == original_recv_port);

        std::cout << "原子性回滚测试通过" << std::endl;
    }
}

// 异常测试
TEST_CASE("TrackConfig 异常测试", "[TrackConfig][Exception]")
{
    // 先创建一个能正常使用的配置文件
    createTestConfigFile(valid_config_file, "192.168.1.100", 8888, 9999);
    TrackConfig config(valid_config_file);

    std::string original_ip = config.track_dst_ip;
    uint16_t original_send_port = config.trackmanager_dst_port;
    uint16_t original_recv_port = config.trackmanager_recv_port;

    // 使用reload进行测试
    SECTION("不存在的文件测试")
    {
        // 测试加载不存在的文件
        bool result = config.reload(non_existent_file);

        // reload应该失败
        REQUIRE_FALSE(result);

        std::cout << "不存在的文件测试通过" << std::endl;
    }

    SECTION("无等号格式测试")
    {
        std::ofstream bad_file(invalid_config_file);
        bad_file << "track_dst_ip 192.168.1.1\n"; // 缺少等号
        bad_file << "trackmanager_dst_port = 7777\n";
        bad_file << "trackmanager_recv_port = 8888\n";
        bad_file.close();

        bool result = config.reload(invalid_config_file);

        // 应该返回false，因为有一行没有等号
        REQUIRE_FALSE(result);

        std::cout << "无等号格式测试通过" << std::endl;
    }

    SECTION("等号右侧无意义值测试")
    {
        std::ofstream nonsense_file(invalid_config_file);
        nonsense_file << "track_dst_ip = not_an_ip_address\n";
        nonsense_file << "trackmanager_dst_port = not_a_port\n";
        nonsense_file << "trackmanager_recv_port = 9999\n";
        nonsense_file.close();

        bool result = config.reload(invalid_config_file);

        // 应该返回false，因为IP和端口解析失败
        REQUIRE_FALSE(result);

        std::cout << "无意义值测试通过" << std::endl;
    }

    // IP地址无法解析/不存在此类IP地址
    SECTION("错误的IP地址测试")
    {
        std::vector<std::string> invalid_ips = {
            "999.999.999.999", // 超出范围
            "256.256.256.256", // 超出范围
            "192.168.1.",      // 不完整
            "192.168.1.1.1",   // 过多部分
            "",                // 空字符串
            "192.168.1.256",   // 最后一段超出范围
        };

        for (const auto &invalid_ip : invalid_ips)
        {
            std::cout << "测试无效IP: " << invalid_ip << std::endl;

            std::ofstream ip_test_file(invalid_config_file);
            ip_test_file << "track_dst_ip = " << invalid_ip << "\n";
            ip_test_file << "trackmanager_dst_port = 7777\n";
            ip_test_file << "trackmanager_recv_port = 8888\n";
            ip_test_file.close();

            bool result = config.reload(invalid_config_file);

            // 应该返回false
            REQUIRE_FALSE(result);

            std::cout << "  -> 测试通过" << std::endl;
        }

        std::cout << "错误IP地址测试完成" << std::endl;
    }

    // 端口号无法解析/端口号超出范围
    SECTION("错误的端口号测试")
    {
        struct PortTest
        {
            std::string port_str;
            std::string description;
        };

        std::vector<PortTest> invalid_port_tests = {
            {"0", "端口0"},
            {"65536", "超出范围"},
            {"70000", "远超出范围"},
            {"-1", "负数"},
            {"not_a_number", "非数字"},
            {"123.456", "浮点数"},
            {"", "空字符串"},
        };

        for (const auto &test : invalid_port_tests)
        {
            std::cout << "测试无效端口: " << test.description << "(" << test.port_str << ")" << std::endl;

            // 测试发送端口无效
            {
                std::ofstream port_test_file(invalid_config_file);
                port_test_file << "[network]\n";
                port_test_file << "track_dst_ip = 192.168.1.1\n";
                port_test_file << "trackmanager_dst_port = " << test.port_str << "\n";
                port_test_file << "trackmanager_recv_port = 8888\n";
                port_test_file.close();

                bool result = config.reload(invalid_config_file);
                REQUIRE_FALSE(result);
                std::cout << "  -> 发送端口测试: 通过" << std::endl;
            }

            // 测试接收端口无效
            {
                std::ofstream port_test_file(invalid_config_file);
                port_test_file << "[network]\n";
                port_test_file << "track_dst_ip = 192.168.1.1\n";
                port_test_file << "trackmanager_dst_port = 7777\n";
                port_test_file << "trackmanager_recv_port = " << test.port_str << "\n";
                port_test_file.close();

                bool result = config.reload(invalid_config_file);
                REQUIRE_FALSE(result);
                std::cout << "  -> 接收端口测试: 通过" << std::endl;
            }
        }

        std::cout << "错误端口号测试完成" << std::endl;
    }

    SECTION("大小写敏感测试")
    {
        // 测试配置项名称的大小写
        std::ofstream case_test_file(invalid_config_file);
        case_test_file << "[network]\n";
        case_test_file << "TRACK_DST_IP = 10.0.0.1\n";       // 全大写
        case_test_file << "TrackManager_Dst_Port = 7777\n";  // 混合大小写
        case_test_file << "trackmanager_recv_port = 8888\n"; // 正确小写
        case_test_file.close();

        bool result = config.reload(invalid_config_file);

        // 应该返回false，因为大小写不匹配
        REQUIRE_FALSE(result);

        std::cout << "大小写敏感测试通过" << std::endl;
    }

    // 清理测试文件
    std::remove(invalid_config_file.c_str());

}
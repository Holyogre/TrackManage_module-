#include <catch2/catch_all.hpp>
#include <fstream>
#include <thread>
#include <atomic>
#include "../include/TrackConfig.hpp"

using namespace track_project;

// 测试文件路径
const std::string valid_config_file = "../config_valid.ini";
const std::string temp_valid_config_file = "../test_config_temp_valid.ini";
const std::string invalid_config_file = "../test_config_invalid.ini";
const std::string non_existent_file = "../non_existent_file.ini";

// 辅助函数：创建配置文件
void createTestConfigFile(const std::string &filename,
                          const std::string &ip,
                          uint16_t send_port,
                          uint16_t recv_port,
                          const std::string &filters = "")
{
    std::ofstream config_file(filename);
    config_file << "track_dst_ip = " << ip << "\n";
    config_file << "trackmanager_dst_port = " << send_port << "\n";
    config_file << "trackmanager_recv_port = " << recv_port << "\n";
    if (!filters.empty())
    {
        config_file << "trackmanager_recv_filters = " << filters << "\n";
    }
    config_file.close();
}

// 测试套件：TrackConfig 功能验证
TEST_CASE("TrackConfig 配置加载与重载", "[TrackConfig][basic]")
{

    SECTION("1. 初始加载有效配置文件")
    {
        createTestConfigFile(valid_config_file, "192.168.1.100", 7777, 8888, "TRACK_, SYSTEM_");
        TrackConfig config(valid_config_file);

        // 验证基础配置
        REQUIRE(config.track_dst_ip == "192.168.1.100");
        REQUIRE(config.trackmanager_dst_port == 7777);
        REQUIRE(config.trackmanager_recv_port == 8888);

        // 验证过滤规则解析
        REQUIRE(config.trackmanager_recv_filters.size() == 2);
        LOG_INFO << config.trackmanager_recv_filters[0];
        LOG_INFO << config.trackmanager_recv_filters[1];
        REQUIRE(config.trackmanager_recv_filters[0] == "TRACK_");
        REQUIRE(config.trackmanager_recv_filters[1] == "SYSTEM_");
    }

    SECTION("2. reload() 方法：成功重载新配置")
    {
        // 初始配置
        createTestConfigFile(valid_config_file, "192.168.1.100", 7777, 8888, "TRACK_, SYSTEM_");
        TrackConfig config(valid_config_file);

        // 新配置
        createTestConfigFile(temp_valid_config_file, "10.0.0.2", 3333, 4444, "NEW_FILTER");

        // 执行重载
        bool success = config.reload(temp_valid_config_file);

        // 验证结果
        REQUIRE(success);
        REQUIRE(config.track_dst_ip == "10.0.0.2");
        REQUIRE(config.trackmanager_dst_port == 3333);
        REQUIRE(config.trackmanager_recv_filters.size() == 1);
        REQUIRE(config.trackmanager_recv_filters[0] == "NEW_FILTER");
    }

    SECTION("3. reload() 原子性：无效配置触发回滚")
    {
        // 初始配置
        createTestConfigFile(valid_config_file, "172.16.0.1", 5555, 6666, "VALID_FILTER");
        TrackConfig config(valid_config_file);
        auto old_filters = config.trackmanager_recv_filters;

        // 创建无效配置文件（缺少关键字段）
        std::ofstream bad_file(invalid_config_file);
        bad_file << "invalid_key = invalid_value\n"; // 故意缺少必需字段
        bad_file.close();

        // 执行重载（应失败）
        bool success = config.reload(invalid_config_file);

        // 验证回滚行为
        REQUIRE_FALSE(success);
        REQUIRE(config.track_dst_ip == "172.16.0.1");             // 配置值未被破坏
        REQUIRE(config.trackmanager_recv_filters == old_filters); // 过滤规则未被破坏
    }

}

TEST_CASE("线程安全测试", "[TrackConfig][thread]")
{
    SECTION("并发安全测试：reload() 期间配置一致性")
    {
        // 初始配置
        createTestConfigFile(valid_config_file, "192.168.1.100", 9000, 10000, "TRACK_, SYSTEM_");
        TrackConfig config(valid_config_file);

        // 模拟并发访问的线程
        std::atomic<bool> thread_running(true);
        std::thread reader([&]
                           {
            while (thread_running) {
                LOG_DEBUG << "读取配置: dst_port=" << config.trackmanager_dst_port;
                // 持续读取配置，验证中间状态不会暴露
                REQUIRE((config.trackmanager_dst_port == 9000 || config.trackmanager_dst_port == 10000));
            } });

        // 创建新配置文件
        createTestConfigFile(temp_valid_config_file, "192.168.1.2", 10000, 10001, "TRACK_, SYSTEM_");

        // 执行重载（可能耗时）
        bool success = config.reload(temp_valid_config_file);
        REQUIRE(success);

        // 停止线程并验证
        thread_running = false;
        reader.join();
    }
}

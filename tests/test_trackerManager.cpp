#include <catch2/catch_all.hpp>
#include "../src/TrackerManager.hpp"
#include "TrackerManagerDebugger.hpp"
#include <cstring>
#include <iostream>
#include "../utils/Logger.hpp"

using namespace track_project::trackmanager;
using namespace track_project;

// 定义最大外推次数,当>MAX_EXTRAPOLATION_TIMES时，删除对应航迹
constexpr size_t MAX_EXTRAPOLATION_TIMES = 3;

// gpt生成的对应的测试代码，改动数据结构这些测试应该还是能通过的
TEST_CASE("TrackerManager 构造/析构函数测试", "[TrackerManager][Construction]")
{
    SECTION("标准构造")
    {
        TrackerManager manager;
        REQUIRE(manager.get_total_capacity() == 2000);
        REQUIRE(manager.get_used_count() == 0);
    }

    SECTION("定制化构造")
    {
        TrackerManager manager(100, 50);
        REQUIRE(manager.get_total_capacity() == 100);
        REQUIRE(manager.get_used_count() == 0);
    }

    SECTION("析构")
    {
        {
            TrackerManager manager(100, 50);
            for (int i = 0; i < 50; ++i)
            {
                manager.create_track();
            }
        }
        // 如果没崩溃就认为正常运行
        SUCCEED("在ASAN下若通过，则证明可以正常析构");
    }
}

TEST_CASE("TrackerManager 申请新航迹容器", "[TrackerManager][create]")
{
    TrackerManager manager(10, 5);

    SECTION("申请多条新航迹")
    {
        std::uint32_t id1 = manager.create_track();
        std::uint32_t id2 = manager.create_track();
        std::uint32_t id3 = manager.create_track();

        REQUIRE(id1 == 1);
        REQUIRE(id2 == 2);
        REQUIRE(id3 == 3);
        REQUIRE(manager.get_used_count() == 3);

        REQUIRE(manager.is_valid_track(id1));
        REQUIRE(manager.is_valid_track(id2));
        REQUIRE(manager.is_valid_track(id3));
    }

    SECTION("满空间时申请新航迹")
    {
        // Fill the pool
        for (std::uint32_t i = 0; i < 10; ++i)
        {
            manager.create_track();
        }

        REQUIRE(manager.get_used_count() == 10);

        // Try to create one more
        size_t overflow_id = manager.create_track();
        REQUIRE(overflow_id == 0);
        REQUIRE(manager.get_used_count() == 10);
    }

    SECTION("ID自增性测试")
    {
        manager.create_track();
        for (std::uint32_t i = 1; i < 10; i++)
        {
            manager.delete_track(i);

            size_t id = manager.create_track();
            REQUIRE(id == i + 1);
            REQUIRE(manager.get_used_count() == 1); // 确保是同一个内存池
        }
    }
}

TEST_CASE("TrackerManager 放入元素", "[TrackerManager][push]")
{
    TrackerManager manager(10, 5);
    std::uint32_t track_id = manager.create_track();

    // 创建测试点
    TrackPoint test_point;
    test_point.longitude = 116.3974;
    test_point.latitude = 39.9093;
    test_point.sog = 10.5;
    test_point.cog = 45.0;
    test_point.is_associated = true;

    SECTION("放入不存在的航迹")
    {
        std::uint32_t invalid_track_id = 9999;
        size_t initial_used = manager.get_used_count();
        bool result = manager.push_track_point(invalid_track_id, test_point);

        REQUIRE(result == false);
        REQUIRE(manager.get_used_count() == initial_used); // 航迹数量不变
    }

    SECTION("放置超出容量的航迹点")
    {
        // 填充到容量上限
        for (int i = 0; i < 5; i++)
        {
            TrackPoint point = test_point;
            point.longitude += i * 0.001; // 稍微改变位置以区分不同点
            point.time = Timestamp::now();
            bool result = manager.push_track_point(track_id, point);
            REQUIRE(result == true);
        }

        // 验证航迹仍然有效
        REQUIRE(manager.is_valid_track(track_id) == true);

        // 继续添加应该仍然成功（LatestKBuffer 会自动处理）
        TrackPoint extra_point = test_point;
        extra_point.longitude = 120.0;
        bool final_result = manager.push_track_point(track_id, extra_point);
        REQUIRE(final_result == true);
    }

    SECTION("关联点重置外推计数")
    {
        // 先添加几个非关联点增加外推计数
        TrackPoint non_associated_point = test_point;
        non_associated_point.is_associated = false;

        for (std::uint32_t i = 0; i < 2; i++)
        {
            manager.push_track_point(track_id, non_associated_point);
        }

        // 添加关联点应该减少外推计数
        bool result = manager.push_track_point(track_id, test_point);
        REQUIRE(result == true);
        REQUIRE(manager.is_valid_track(track_id) == true);
    }

    SECTION("超过最大外推次数触发删除")
    {
        TrackPoint non_associated_point = test_point;
        non_associated_point.is_associated = false;

        // 添加 MAX_EXTRAPOLATION_TIMES 个非关联点
        for (std::uint32_t i = 0; i <= MAX_EXTRAPOLATION_TIMES; i++)
        {
            bool result = manager.push_track_point(track_id, non_associated_point);
            REQUIRE(result == true);
            // TrackerManagerDebugger::printFullState(manager);
        }

        // 再添加一个非关联点应该触发删除
        size_t initial_used = manager.get_used_count();
        bool final_result = manager.push_track_point(track_id, non_associated_point);

        REQUIRE(final_result == false); // 返回false表示航迹被删除
        REQUIRE(manager.get_used_count() == initial_used - 1);
        REQUIRE(manager.is_valid_track(track_id) == false);
    }
}

TEST_CASE("TrackerManager 删除航迹容器", "[TrackerManager][delete]")
{

    TrackerManager manager(10, 5);
    std::uint32_t track_id = manager.create_track();

    SECTION("删除存在的航迹")
    {
        REQUIRE(manager.delete_track(track_id) == true);
        REQUIRE(manager.get_used_count() == 0);
        REQUIRE(manager.is_valid_track(track_id) == false);
    }

    SECTION("删除不存在的航迹")
    {
        REQUIRE(manager.delete_track(track_id + 1) == false);
        REQUIRE(manager.get_used_count() == 1);
    }

    SECTION("全航迹清理测试")
    {
        // 多造点航迹
        manager.create_track();
        manager.create_track();
        manager.create_track();
        REQUIRE(manager.get_used_count() == 4);

        manager.clear_all();

        REQUIRE(manager.get_used_count() == 0);

        // ID重置确认
        size_t new_id = manager.create_track();
        REQUIRE(new_id == 1); // ID重置
        REQUIRE(manager.get_used_count() == 1);
    }
}

TEST_CASE("TrackerManager NUMA性能测试", "[TrackerManager][test_bench_chrono]")
{
    constexpr std::uint32_t TRACK_COUNT = 2000;
    constexpr std::uint32_t POINTS_PER_TRACK = 2000;
    constexpr size_t BUFFER_SIZE = 100 * 1024 * 1024; // 100MB缓冲区

    // 统一的测试数据
    TrackPoint point;
    point.longitude = 116.3974;
    point.latitude = 39.9093;
    point.sog = 10.5;
    point.cog = 45.0;
    point.is_associated = true;
    point.time = Timestamp::now();

    auto run_benchmark = [](const std::string &name, auto &&func)
    {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "[" << name << "] " << duration.count() << " us" << std::endl;
        return duration;
    };

    SECTION("核心场景1: 航迹创建性能")
    {
        TrackerManager manager(TRACK_COUNT, POINTS_PER_TRACK); // 创建类不算时间
        auto duration = run_benchmark("创建2000个空航迹", [&]()
                                      {
            for (std::uint32_t i = 0; i < TRACK_COUNT; ++i) {
                manager.create_track();
            } });

        double avg_time_per_track = (double)duration.count() / TRACK_COUNT;
        std::cout << "平均每个航迹创建时间: " << avg_time_per_track << " us" << std::endl;
        // TrackerManagerDebugger::printStatistics(manager);
    }

    SECTION("核心场景2: 点迹写入性能")
    {
        TrackerManager manager(TRACK_COUNT, POINTS_PER_TRACK);

        // 预先创建所有航迹
        std::vector<std::uint32_t> track_ids;
        for (std::uint32_t i = 0; i < TRACK_COUNT; ++i)
        {
            std::uint32_t id = manager.create_track();
            if (id != 0)
                track_ids.push_back(id);
        }

        auto duration = run_benchmark("写入400万个点迹", [&]()
                                      {
            for (std::uint32_t track_id : track_ids) {
                for (std::uint32_t j = 0; j < POINTS_PER_TRACK; ++j) {
                    manager.push_track_point(track_id, point);
                }
            } });

        double avg_time_per_point = (double)duration.count() / (TRACK_COUNT * POINTS_PER_TRACK);
        std::cout << "平均每个点迹写入时间: " << avg_time_per_point << " us" << std::endl;
        // TrackerManagerDebugger::printFullState(manager);
    }

    SECTION("核心场景3: 航迹删除性能")
    {
        TrackerManager manager(TRACK_COUNT, POINTS_PER_TRACK);

        // 创建并填充所有航迹
        std::vector<std::uint32_t> track_ids;
        for (std::uint32_t i = 0; i < TRACK_COUNT; ++i)
        {
            std::uint32_t id = manager.create_track();
            if (id != 0)
            {
                track_ids.push_back(id);
                for (std::uint32_t j = 0; j < POINTS_PER_TRACK; ++j) // 虽说理论不应该有影响，实际上还是有一点的
                {
                    manager.push_track_point(id, point);
                }
            }
        }

        auto duration = run_benchmark("删除2000个满载航迹", [&]()
                                      {
            for (std::uint32_t track_id : track_ids) {
                manager.delete_track(track_id);
            } });

        double avg_time_per_track = (double)duration.count() / TRACK_COUNT;
        std::cout << "平均每个航迹删除时间: " << avg_time_per_track << " us" << std::endl;
        // TrackerManagerDebugger::printStatistics(manager);
    }
}

TEST_CASE("TrackerManager 核心性能测试", "[TrackerManager][benchmark]")
{
    constexpr std::uint32_t TRACK_COUNT = 2000;
    constexpr std::uint32_t POINTS_PER_TRACK = 2000;
    constexpr size_t BUFFER_SIZE = 100 * 1024 * 1024; // 100MB缓冲区

    // 统一的测试数据
    TrackPoint point;
    point.longitude = 116.3974;
    point.latitude = 39.9093;
    point.sog = 10.5;
    point.cog = 45.0;
    point.is_associated = true;
    point.time = Timestamp::now();

    SECTION("核心场景1: 航迹创建性能")
    {
        TrackerManager manager(TRACK_COUNT, POINTS_PER_TRACK);

        BENCHMARK("创建2000个空航迹")
        {
            for (std::uint32_t i = 0; i < TRACK_COUNT; ++i)
            {
                manager.create_track();
            }
        };
    }

    SECTION("核心场景2: 点迹写入性能")
    {
        TrackerManager manager(TRACK_COUNT, POINTS_PER_TRACK);

        // 预先创建所有航迹
        std::vector<std::uint32_t> track_ids;
        for (std::uint32_t i = 0; i < TRACK_COUNT; ++i)
        {
            std::uint32_t id = manager.create_track();
            if (id != 0)
                track_ids.push_back(id);
        }

        BENCHMARK("写入400万个点迹")
        {
            for (std::uint32_t track_id : track_ids)
            {
                for (std::uint32_t j = 0; j < POINTS_PER_TRACK; ++j)
                {
                    manager.push_track_point(track_id, point);
                }
            }
        };
    }

    SECTION("核心场景3: 航迹删除性能")
    {
        TrackerManager manager(TRACK_COUNT, POINTS_PER_TRACK);

        // 创建并填充所有航迹
        std::vector<std::uint32_t> track_ids;
        for (std::uint32_t i = 0; i < TRACK_COUNT; ++i)
        {
            std::uint32_t id = manager.create_track();
            if (id != 0)
            {
                track_ids.push_back(id);
                for (std::uint32_t j = 0; j < POINTS_PER_TRACK; ++j)
                {
                    manager.push_track_point(id, point);
                }
            }
        }

        BENCHMARK("删除2000个满载航迹")
        {
            for (std::uint32_t track_id : track_ids)
            {
                manager.delete_track(track_id);
            }
        };
    }
}

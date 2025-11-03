#include <catch2/catch_all.hpp>
#include "../src/trackManager.hpp"
#include "TrackerManagerDebugger.hpp"
#include <cstring>
#include <iostream>
#include "../utils/logger.hpp"

using namespace trackerManager;

// 定义最大外推次数,当>MAX_EXTRAPOLATION_TIMES时，删除对应航迹
constexpr size_t MAX_EXTRAPOLATION_TIMES = 3;

// gpt生成的对应的测试代码，改动数据结构这些测试应该还是能通过的
TEST_CASE("TrackerManager 构造/析构函数测试", "[TrackerManager][Construction]")
{
    SECTION("标准构造")
    {
        TrackerManager manager;
        REQUIRE(manager.getTotalCapacity() == 2000);
        REQUIRE(manager.getUsedCount() == 0);
        REQUIRE(manager.getFreeCount() == 2000);
    }

    SECTION("定制化构造")
    {
        TrackerManager manager(100, 50);
        REQUIRE(manager.getTotalCapacity() == 100);
        REQUIRE(manager.getUsedCount() == 0);
        REQUIRE(manager.getFreeCount() == 100);
    }

    SECTION("析构")
    {
        {
            TrackerManager manager(100, 50);
            for (int i = 0; i < 50; ++i)
            {
                manager.createTrack();
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
        size_t id1 = manager.createTrack();
        size_t id2 = manager.createTrack();
        size_t id3 = manager.createTrack();

        REQUIRE(id1 == 1);
        REQUIRE(id2 == 2);
        REQUIRE(id3 == 3);
        REQUIRE(manager.getUsedCount() == 3);
        REQUIRE(manager.getFreeCount() == 7);

        REQUIRE(manager.isValidTrack(id1));
        REQUIRE(manager.isValidTrack(id2));
        REQUIRE(manager.isValidTrack(id3));
    }

    SECTION("满空间时申请新航迹")
    {
        // Fill the pool
        for (int i = 0; i < 10; ++i)
        {
            manager.createTrack();
        }

        REQUIRE(manager.getUsedCount() == 10);
        REQUIRE(manager.getFreeCount() == 0);

        // Try to create one more
        size_t overflow_id = manager.createTrack();
        REQUIRE(overflow_id == 0);
        REQUIRE(manager.getUsedCount() == 10);
    }

    SECTION("ID自增性测试")
    {
        manager.createTrack();
        for (int i = 1; i < 10; i++)
        {
            manager.deleteTrack(i);

            size_t id = manager.createTrack();
            REQUIRE(id == i + 1);
            REQUIRE(manager.getUsedCount() == 1); // 确保是同一个内存池
        }
    }
}

TEST_CASE("TrackerManager 放入元素", "[TrackerManager][push]")
{
    TrackerManager manager(10, 5);
    size_t track_id = manager.createTrack();

    // 创建测试点
    commonData::track_point test_point;
    test_point.longitude = 116.3974;
    test_point.latitude = 39.9093;
    test_point.sog = 10.5;
    test_point.cog = 45.0;
    test_point.angle = 30.0;
    test_point.distance = 1000.0;
    test_point.is_associated = true;

    SECTION("放入不存在的航迹")
    {
        size_t invalid_track_id = 9999;
        size_t initial_used = manager.getUsedCount();
        bool result = manager.push_track_point(invalid_track_id, test_point);

        REQUIRE(result == false);
        REQUIRE(manager.getUsedCount() == initial_used); // 航迹数量不变
    }

    SECTION("放置超出容量的航迹点")
    {
        // 填充到容量上限
        for (int i = 0; i < 5; i++)
        {
            commonData::track_point point = test_point;
            point.longitude += i * 0.001; // 稍微改变位置以区分不同点
            point.time = commonData::Timestamp::now();
            bool result = manager.push_track_point(track_id, point);
            REQUIRE(result == true);
        }

        // 验证航迹仍然有效
        REQUIRE(manager.isValidTrack(track_id) == true);

        // 继续添加应该仍然成功（LatestKBuffer 会自动处理）
        commonData::track_point extra_point = test_point;
        extra_point.longitude = 120.0;
        bool final_result = manager.push_track_point(track_id, extra_point);
        REQUIRE(final_result == true);
    }

    SECTION("关联点重置外推计数")
    {
        // 先添加几个非关联点增加外推计数
        commonData::track_point non_associated_point = test_point;
        non_associated_point.is_associated = false;

        for (int i = 0; i < 2; i++)
        {
            manager.push_track_point(track_id, non_associated_point);
        }

        // 添加关联点应该减少外推计数
        bool result = manager.push_track_point(track_id, test_point);
        REQUIRE(result == true);
        REQUIRE(manager.isValidTrack(track_id) == true);
    }

    SECTION("超过最大外推次数触发删除")
    {
        commonData::track_point non_associated_point = test_point;
        non_associated_point.is_associated = false;

        // 添加 MAX_EXTRAPOLATION_TIMES 个非关联点
        for (int i = 0; i <= MAX_EXTRAPOLATION_TIMES; i++)
        {
            bool result = manager.push_track_point(track_id, non_associated_point);
            REQUIRE(result == true);
            TrackerManagerDebugger::printFullState(manager);
        }

        // 再添加一个非关联点应该触发删除
        size_t initial_used = manager.getUsedCount();
        bool final_result = manager.push_track_point(track_id, non_associated_point);

        REQUIRE(final_result == false); // 返回false表示航迹被删除
        REQUIRE(manager.getUsedCount() == initial_used - 1);
        REQUIRE(manager.isValidTrack(track_id) == false);
    }
}

TEST_CASE("TrackerManager 删除航迹容器", "[TrackerManager][delete]")
{

    TrackerManager manager(10, 5);
    size_t track_id = manager.createTrack();

    SECTION("删除存在的航迹")
    {
        REQUIRE(manager.deleteTrack(track_id) == true);
        REQUIRE(manager.getUsedCount() == 0);
        REQUIRE(manager.getFreeCount() == 10);
        REQUIRE(manager.isValidTrack(track_id) == false);
    }

    SECTION("删除不存在的航迹")
    {
        REQUIRE(manager.deleteTrack(track_id + 1) == false);
        REQUIRE(manager.getUsedCount() == 0);
        REQUIRE(manager.getFreeCount() == 10);
    }

    SECTION("全航迹清理测试")
    {
        // 多造点航迹
        manager.createTrack();
        manager.createTrack();
        manager.createTrack();
        REQUIRE(manager.getUsedCount() == 3);
        REQUIRE(manager.getFreeCount() == 7);

        manager.clearAll();

        REQUIRE(manager.getUsedCount() == 0);
        REQUIRE(manager.getFreeCount() == 10);

        // ID重置确认
        size_t new_id = manager.createTrack();
        REQUIRE(new_id == 1); // ID重置
        REQUIRE(manager.getUsedCount() == 1);
    }
}

TEST_CASE("TrackerManager压力测试", "[TrackerManager][testbench]")
{
    TrackerManager manager(1000, 1000);

    BENCHMARK("航迹快速申请删除测")
    {
        size_t id = 0;
        while (id < 1000)
        {
            id = manager.createTrack();
            manager.deleteTrack(id);
        }
    };
}

#include <catch2/catch_all.hpp>
#include "../src/LatestKBuffer.hpp"
#include <string>
#include <vector>
#include <iostream>

using namespace track_project::trackmanager;

TEST_CASE("LatestKBuffer 基础功能测试 - double类型", "[LatestKBuffer][double]")
{
    SECTION("构造和基础信息")
    {
        LatestKBuffer<double> buffer(5);
        REQUIRE(buffer.capacity() == 5);
        REQUIRE(buffer.size() == 0);
        REQUIRE(buffer.empty() == true);
        REQUIRE(buffer.full() == false);
    }

    SECTION("追加数据")
    {
        LatestKBuffer<double> buffer(3);

        buffer.push(1.1);
        REQUIRE(buffer.size() == 1);
        REQUIRE(buffer[0] == 1.1);

        buffer.push(2.2);
        REQUIRE(buffer.size() == 2);
        REQUIRE(buffer[0] == 1.1);
        REQUIRE(buffer[1] == 2.2);

        buffer.push(3.3);
        REQUIRE(buffer.size() == 3);
        REQUIRE(buffer.full() == true);
    }

    SECTION("滚动更新机制")
    {
        LatestKBuffer<double> buffer(3);

        buffer.push(1.0);
        buffer.push(2.0);
        buffer.push(3.0); // 满了

        buffer.push(4.0); // 应该淘汰1.0
        REQUIRE(buffer.size() == 3);
        REQUIRE(buffer[0] == 2.0); // 最老的数据变成2.0
        REQUIRE(buffer[1] == 3.0);
        REQUIRE(buffer[2] == 4.0); // 最新的数据

        buffer.push(5.0); // 淘汰2.0
        REQUIRE(buffer[0] == 3.0);
        REQUIRE(buffer[1] == 4.0);
        REQUIRE(buffer[2] == 5.0);
    }

    SECTION("定点修改数据")
    {
        LatestKBuffer<double> buffer(3);
        buffer.push(1.0);
        buffer.push(2.0);

        buffer[0] = 10.0; // 修改最老的数据
        buffer[1] = 20.0; // 修改中间的数据

        REQUIRE(buffer[0] == 10.0);
        REQUIRE(buffer[1] == 20.0);
    }

    SECTION("移动语义和原地构造")
    {
        LatestKBuffer<double> buffer(3);

        // 移动语义
        double value = 42.0;
        buffer.push(std::move(value));
        REQUIRE(buffer[0] == 42.0);

        // 原地构造
        buffer.emplace(99.9);
        REQUIRE(buffer[1] == 99.9);
    }
}

TEST_CASE("LatestKBuffer 字符串测试 - std::string类型", "[LatestKBuffer][string]")
{
    SECTION("字符串基础操作")
    {
        LatestKBuffer<std::string> buffer(3);

        buffer.push("hello");
        buffer.push("world");
        buffer.emplace("test");

        REQUIRE(buffer.size() == 3);
        REQUIRE(buffer[0] == "hello");
        REQUIRE(buffer[1] == "world");
        REQUIRE(buffer[2] == "test");
    }

    SECTION("字符串滚动更新")
    {
        LatestKBuffer<std::string> buffer(2);

        buffer.push("first");
        buffer.push("second");
        buffer.push("third"); // 应该淘汰"first"

        REQUIRE(buffer.size() == 2);
        REQUIRE(buffer[0] == "second");
        REQUIRE(buffer[1] == "third");
    }

    SECTION("字符串修改")
    {
        LatestKBuffer<std::string> buffer(3);
        buffer.push("apple");
        buffer.push("banana");

        buffer[0] += " pie";
        buffer[1] = "orange";

        REQUIRE(buffer[0] == "apple pie");
        REQUIRE(buffer[1] == "orange");
    }

    SECTION("长字符串测试")
    {
        LatestKBuffer<std::string> buffer(2);
        std::string long_str(1000, 'A'); // 1000个'A'
        std::string another_long_str(1000, 'B');

        buffer.push(long_str);
        buffer.push(another_long_str);

        REQUIRE(buffer[0] == long_str);
        REQUIRE(buffer[1] == another_long_str);
    }

    SECTION("清空测试")
    {
        LatestKBuffer<std::string> buffer(3);
        buffer.push("apple");
        buffer.push("banana");

        REQUIRE(buffer.size() == 2);
        REQUIRE(buffer.empty() == false);

        buffer.clear();

        REQUIRE(buffer.size() == 0);
        REQUIRE(buffer.empty() == true);
        REQUIRE(buffer.full() == false);
    }
}

TEST_CASE("LatestKBuffer 边界测试", "[LatestKBuffer][edge]")
{
    SECTION("容量为1的边界情况")
    {
        LatestKBuffer<int> buffer(1);

        REQUIRE(buffer.empty() == true);

        buffer.push(42);
        REQUIRE(buffer.full() == true);
        REQUIRE(buffer[0] == 42);

        buffer.push(99); // 替换唯一的数据
        REQUIRE(buffer[0] == 99);
    }

    SECTION("空缓冲区访问")
    {
        LatestKBuffer<int> buffer(3);

        // 空缓冲区应该返回空
        REQUIRE(buffer.empty() == true);
        // 访问空缓冲区应该断言失败（在Debug模式下）
    }
}


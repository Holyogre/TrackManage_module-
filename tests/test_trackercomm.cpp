#include <catch2/catch_all.hpp>
#include "../src/TrackerComm.hpp"
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <chrono>

using namespace track_project::trackmanager;
using namespace track_project;

// 生成测试数据
std::vector<IntFloatUnion> generateTestData(size_t size)
{
    std::vector<IntFloatUnion> data;
    data.reserve(size);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    for (size_t i = 0; i < size; ++i)
    {
        IntFloatUnion item;
        item.ri = dist(gen);
        data.push_back(item);
    }

    return data;
}

// 多线程调用安全性测试
TEST_CASE("TrackerComm 多线程安全性测试", "[TrackerComm][thread-safety]")
{
    // 使用测试配置文件
    TrackerComm tracker_comm("../../config/config.ini");

    // 创建一个线程循环给config.trackmanager_recv_port端口发送数据，把发送数据打印出来
    // 注意，数据组成格式为：[数据头，数据内容]，数据头格式是：
    //         struct PacketHeader
    //         {
    //             char packet_id[128];           // 数据包ID (128字节)
    //             std::uint32_t total_fragments; // 总片段数
    //             std::uint32_t fragment_index;  // 当前片段索引
    //             std::uint32_t total_size;      // 总数据大小（所有分片的总大小）
    //             std::uint32_t fragment_size;   // 当前分片大小
    //             std::uint32_t checksum;        // 校验和，32位异或
    //         };
    // 然后数据内容是uint32_t数组

    // 旨在避免偶发程序多开稳定性问题
    SECTION("多线程发送数据")
    {
        constexpr int NUM_THREADS = 5;
        constexpr int NUM_ITERATIONS = 10;
        constexpr size_t DATA_SIZE = 1000;

        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};
        std::atomic<int> total_attempts{0};

        // 创建发送线程
        for (int i = 0; i < NUM_THREADS; ++i)
        {
            threads.emplace_back([&, thread_id = i]()
                                 {
                for (int j = 0; j < NUM_ITERATIONS; ++j)
                {
                    try
                    {
                        auto test_data = generateTestData(DATA_SIZE + thread_id * 10 + j);
                        bool result = tracker_comm.sendData(test_data);
                        
                        total_attempts.fetch_add(1, std::memory_order_relaxed);
                        if (result)
                        {
                            success_count.fetch_add(1, std::memory_order_relaxed);
                        }
                        
                        // 模拟不同线程发送间隔
                        std::this_thread::sleep_for(std::chrono::milliseconds(10 * thread_id));
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "Thread " << thread_id << " exception: " << e.what() << std::endl;
                    }
                } });
        }

        // 等待所有线程完成
        for (auto &t : threads)
        {
            t.join();
        }

        REQUIRE(total_attempts == NUM_THREADS * NUM_ITERATIONS);
        LOG_DEBUG << "线程数据已发送，用网络分析仪查看正确性!" << success_count << " / " << total_attempts << " 成功发送" << std::endl;
    }

    SECTION("异步读取数据")
    {

        // 启动该类，令其循环读取数据并打印，
    }

    SECTION("发送数据同时重载接口测试")
    {
        constexpr int NUM_THREADS = 4;
        constexpr int NUM_OPERATIONS = 15;

        std::vector<std::thread> threads;
        std::atomic<int> reload_count{0};
        std::atomic<int> send_count{0};
        std::atomic<bool> stop_all{false};

        // 创建混合操作线程
        for (int i = 0; i < NUM_THREADS; ++i)
        {
            threads.emplace_back([&, thread_id = i]()
                                 {
                for (int j = 0; j < NUM_OPERATIONS; ++j)
                {
                    try
                    {
                        if (thread_id % 2 == 0)
                        {
                            // 发送数据线程
                            auto test_data = generateTestData(100 + thread_id * 5);
                            bool result = tracker_comm.sendData(test_data);
                            if (result)
                            {
                                send_count.fetch_add(1, std::memory_order_relaxed);
                            }
                        }
                        else
                        {
                            // 重载配置线程
                            tracker_comm.reload("../../config/track_config.ini");
                            reload_count.fetch_add(1, std::memory_order_relaxed);
                        }
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(30));
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "Mixed operation thread " << thread_id 
                                  << " exception: " << e.what() << std::endl;
                    }
                    
                    if (stop_all.load(std::memory_order_relaxed))
                    {
                        break;
                    }
                } });
        }

        // 同时创建一些读取线程
        std::vector<std::thread> read_threads;
        constexpr int NUM_READERS = 2;
        std::atomic<int> read_operations{0};

        for (int i = 0; i < NUM_READERS; ++i)
        {
            read_threads.emplace_back([&, thread_id = i]()
                                      {
                while (!stop_all.load(std::memory_order_relaxed))
                {
                    try
                    {
                        auto data = tracker_comm.readReceivedData();
                        read_operations.fetch_add(1, std::memory_order_relaxed);
                        std::this_thread::sleep_for(std::chrono::milliseconds(25));
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "Reader during mixed test " << thread_id 
                                  << " exception: " << e.what() << std::endl;
                    }
                } });
        }

        // 等待操作线程完成
        for (auto &t : threads)
        {
            t.join();
        }

        // 停止所有线程
        stop_all.store(true, std::memory_order_relaxed);
        for (auto &t : read_threads)
        {
            t.join();
        }

        std::cout << "混合操作测试完成: "
                  << send_count << " sends, "
                  << reload_count << " reloads, "
                  << read_operations << " reads" << std::endl;

        REQUIRE(send_count + reload_count == (NUM_THREADS / 2 + NUM_THREADS % 2) * NUM_OPERATIONS);
    }

    SECTION("多线程构造和析构测试")
    {
        constexpr int NUM_INSTANCES = 3;
        constexpr int NUM_THREADS = 3;

        std::vector<std::thread> threads;
        std::atomic<int> instance_count{0};
        std::vector<std::unique_ptr<TrackerComm>> instances;

        // 多线程创建实例
        for (int i = 0; i < NUM_THREADS; ++i)
        {
            threads.emplace_back([&, thread_id = i]()
                                 {
                try
                {
                    // 创建多个实例
                    for (int j = 0; j < NUM_INSTANCES; ++j)
                    {
                        auto comm = std::make_unique<TrackerComm>("../../config/track_config.ini");
                        
                        // 进行一些操作
                        auto test_data = generateTestData(50);
                        comm->sendData(test_data);
                        auto received = comm->readReceivedData();
                        
                        std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex*>(&instances));
                        instances.push_back(std::move(comm));
                        instance_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Instance creation thread " << thread_id 
                              << " exception: " << e.what() << std::endl;
                } });
        }

        // 等待所有线程完成
        for (auto &t : threads)
        {
            t.join();
        }

        REQUIRE(instance_count == NUM_THREADS * NUM_INSTANCES);
        REQUIRE(instances.size() == static_cast<size_t>(NUM_THREADS * NUM_INSTANCES));

        std::cout << "多线程构造析构测试完成: 创建了 "
                  << instance_count << " 个实例" << std::endl;

        // 清空实例（自动析构）
        instances.clear();
    }

    SECTION("并发访问接收状态测试")
    {
        constexpr int NUM_THREADS = 5;
        constexpr int NUM_QUERIES = 100;

        std::vector<std::thread> threads;
        std::atomic<int> is_receiving_count{0};
        std::atomic<int> total_queries{0};

        // 创建查询线程
        for (int i = 0; i < NUM_THREADS; ++i)
        {
            threads.emplace_back([&, thread_id = i]()
                                 {
                for (int j = 0; j < NUM_QUERIES; ++j)
                {
                    try
                    {
                        bool is_receiving = tracker_comm.isReceiving();
                        total_queries.fetch_add(1, std::memory_order_relaxed);
                        
                        if (is_receiving)
                        {
                            is_receiving_count.fetch_add(1, std::memory_order_relaxed);
                        }
                        
                        // 随机间隔查询，模拟真实并发场景
                        std::this_thread::sleep_for(std::chrono::microseconds(100 + (thread_id * 20)));
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "Query thread " << thread_id 
                                  << " exception: " << e.what() << std::endl;
                    }
                } });
        }

        // 同时创建一个发送数据的线程来改变状态
        std::thread sender_thread([&]()
                                  {
            for (int i = 0; i < 10; ++i)
            {
                auto test_data = generateTestData(200);
                tracker_comm.sendData(test_data);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } });

        // 等待所有线程完成
        for (auto &t : threads)
        {
            t.join();
        }
        sender_thread.join();

        REQUIRE(total_queries == NUM_THREADS * NUM_QUERIES);
        std::cout << "并发访问测试完成: "
                  << total_queries << " 次查询，"
                  << is_receiving_count << " 次返回 true" << std::endl;
    }
}

TEST_CASE("TrackerComm 功能测试", "[TrackerComm][communication]")
{
    // TODO
}
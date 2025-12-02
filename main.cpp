#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>
// #include "./src/TrackerVisualizer.hpp"
#include <string_view>

static constexpr std::string_view TRACK_PACKET_ID = "TRACK_PACKET";

// 4. 主函数示例
int main()
{
    srand(static_cast<unsigned>(time(nullptr))); // 设置随机种子
    std::cout << "Tracker Communication Module Test " << TRACK_PACKET_ID << std::endl;

    return 0;
}
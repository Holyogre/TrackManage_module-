/*****************************************************************************
 * @file TrackerManagerDebugger.hpp
 * @author xjl (xjl20011009@126.com)
 * @brief 航迹管理器调试辅助模块
 * @version 0.1
 * @date 2025-11-29
 * 
 * @copyright Copyright (c) 2025
 * 
 *****************************************************************************/
#include "../src/TrackerManager.hpp"
#include "../utils/Logger.hpp"
#include <iomanip>
#include <chrono>

namespace track_project
{
    namespace trackmanager
    {
        class TrackerManagerDebugger
        {
        public:
            // 打印完整状态
            static void printFullState(const TrackerManager &manager)
            {
                std::stringstream ss;
                ss << "\n"
                   << std::string(60, '=') << std::endl;
                ss << "              TRACKER MANAGER 完整状态" << std::endl;
                ss << std::string(60, '=') << std::endl;

                printStatistics(manager, ss);
                ss << std::endl;
                printTrackMapping(manager, ss);
                ss << std::endl;
                printMemoryPool(manager, ss);
                ss << std::endl;

                // 一致性检查
                ss << "\n一致性检查: ";
                if (validateConsistency(manager, ss))
                {
                    ss << "✓ 通过" << std::endl;
                }
                else
                {
                    ss << "✗ 失败" << std::endl;
                }

                ss << std::string(60, '=') << "\n"
                   << std::endl;

                // 输出到日志
                LOG_DEBUG << ss.str();
            }

            // 1. 打印统计信息
            static void printStatistics(const TrackerManager &manager)
            {
                std::stringstream ss;
                printStatistics(manager, ss);
                LOG_DEBUG << ss.str();
            }

            static void printStatistics(const TrackerManager &manager, std::stringstream &ss)
            {
                ss << "系统统计:" << std::endl;
                ss << std::string(50, '-') << std::endl;
                ss << "  总容量: " << manager.getTotalCapacity() << " 个航迹" << std::endl;
                ss << "  使用中: " << manager.getUsedCount() << " 个航迹" << std::endl;
                ss << "  下个ID: " << manager.next_track_id_ << std::endl;
                ss << "  点容量: " << manager.track_length << " 点/航迹" << std::endl;
            }

            // 2. 打印内存池详情
            static void printMemoryPool(const TrackerManager &manager)
            {
                std::stringstream ss;
                printMemoryPool(manager, ss);
                LOG_DEBUG<<ss.str();
            }

            static void printMemoryPool(const TrackerManager &manager, std::stringstream &ss)
            {
                ss << "内存池详情 (" << manager.buffer_pool_.size() << "个槽位):" << std::endl;
                ss << std::string(50, '-') << std::endl;

                size_t active_count = 0;
                for (size_t i = 0; i < manager.buffer_pool_.size(); i++)
                {
                    const auto &container = manager.buffer_pool_[i];
                    if (container.header.track_id != 0)
                    {
                        active_count++;
                        ss << "  [" << std::setw(3) << i << "] 航迹" << std::setw(4) << container.header.track_id
                           << " [状态:" << std::setw(4) << stateToString(container.header.state)
                           << ", 外推:" << std::setw(1) << container.header.extrapolation_count
                           << ", 点数:" << std::setw(3) << container.data.size() << "]";

                        // 显示最近点的时间（如果有）
                        if (!container.data.empty())
                        {
                            const auto &latest_point = container.data[0]; // 假设最新点在索引0
                            ss << " 最新时间:" << latest_point.time;
                        }
                        ss << std::endl;
                    }
                }

                if (active_count == 0)
                {
                    ss << "  [无活跃航迹]" << std::endl;
                }
            }

            // 3. 打印航迹映射
            static void printTrackMapping(const TrackerManager &manager)
            {
                std::stringstream ss;
                printTrackMapping(manager, ss);
                LOG_DEBUG<<ss.str();
            }

            static void printTrackMapping(const TrackerManager &manager, std::stringstream &ss)
            {
                ss << "航迹映射表 (" << manager.track_id_to_pool_index_.size() << "个活跃航迹):" << std::endl;
                ss << std::string(50, '-') << std::endl;

                if (manager.track_id_to_pool_index_.empty())
                {
                    ss << "  [无映射关系]" << std::endl;
                    return;
                }

                for (const auto &[track_id, pool_index] : manager.track_id_to_pool_index_)
                {
                    ss << "  航迹ID " << std::setw(4) << track_id << " → 池索引 " << std::setw(3) << pool_index;

                    if (pool_index < manager.buffer_pool_.size())
                    {
                        const auto &container = manager.buffer_pool_[pool_index];
                        ss << " [状态:" << stateToString(container.header.state)
                           << ", 点数:" << container.data.size() << "]";

                        // 验证一致性
                        if (container.header.track_id != track_id)
                        {
                            ss << " ✗ ID不匹配!";
                        }
                    }
                    else
                    {
                        ss << " ✗ 索引越界!";
                    }
                    ss << std::endl;
                }
            }

        private:
            // 状态转换辅助函数
            static const char *stateToString(int state)
            {
                switch (state)
                {
                case 0:
                    return "正常";
                case 1:
                    return "外推";
                case 2:
                    return "终结";
                default:
                    return "未知";
                }
            }

            static const char *boolToString(bool value)
            {
                return value ? "是" : "否";
            }

            // 数据完整性检查
            static bool validateConsistency(const TrackerManager &manager)
            {
                std::stringstream ss;
                return validateConsistency(manager, ss);
            }

            static bool validateConsistency(const TrackerManager &manager, std::stringstream &ss)
            {
                bool consistent = true;

                // 检查映射关系一致性
                for (const auto &[track_id, pool_index] : manager.track_id_to_pool_index_)
                {
                    if (pool_index >= manager.buffer_pool_.size())
                    {
                        ss << "✗ 航迹 " << track_id << " 的池索引 " << pool_index << " 越界" << std::endl;
                        consistent = false;
                    }

                    const auto &container = manager.buffer_pool_[pool_index];
                    if (container.header.track_id != track_id)
                    {
                        ss << "✗ 映射不一致: 航迹 " << track_id << " 指向的容器ID是 " << container.header.track_id << std::endl;
                        consistent = false;
                    }

                    if (container.header.point_num != container.data.size())
                    {
                        ss << "✗ 映射不一致: 航迹 " << track_id << " header的size是 " << container.header.point_num
                           << "实际size是" << container.data.size() << std::endl;
                        consistent = false;
                    }
                }

                // 检查空闲槽位有效性
                for (size_t free_slot : manager.free_slots_)
                {
                    if (free_slot >= manager.buffer_pool_.size())
                    {
                        ss << "✗ 空闲槽位 " << free_slot << " 越界" << std::endl;
                        consistent = false;
                    }
                }

                return consistent;
            }

            // 验证单个航迹
            static bool validateTrack(const TrackerManager &manager, std::uint32_t track_id)
            {
                return validateConsistency(manager) && manager.isValidTrack(track_id);
            }
        };

    } // namespace trackermanager
}
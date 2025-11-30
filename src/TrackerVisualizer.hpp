/*****************************************************************************
 * @file TrackerVisualizer.hpp
 * @author xjl (xjl20011009@126.com)
 * @brief 航迹管理器可视化模块
 * 1. 通过接口获取trackmanager的航迹信息，从而实时显示TrackerManager的航迹信息
 * 2. 使用OpenCV进行图像绘制，仅用于绘制航迹信息，不存在其他任何信息
 * @version 0.1
 * @date 2025-11-29
 *
 * @copyright Copyright (c) 2025
 *
 *****************************************************************************/
#ifndef TRACKER_VISUALIZER_HPP
#define TRACKER_VISUALIZER_HPP

#include <opencv2/opencv.hpp>
#include <vector>
#include "TrackerManager.hpp"
#include "Logger.hpp"

namespace track_project
{
    namespace trackermanager
    {
        class TrackerVisualizer
        {
        public:
            /*****************************************************************************
             * @brief 构造可视化对象
             * @param lon_min 经度最小值
             * @param lon_max 经度最大值
             * @param lat_min 纬度最小值
             * @param lat_max 纬度最大值
             * @param track_length 航迹点长度
             *****************************************************************************/
            TrackerVisualizer(double lon_min, double lon_max, double lat_min, double lat_max, int track_size = 2000, int track_length = 2000)
                : img(1440, 2560, CV_8UC3, cv::Scalar(255, 255, 255)), // 彩色RGB画布，背景是白色
                  lon_min(lon_min), lon_max(lon_max), lat_min(lat_min), lat_max(lat_max)
            {
                height = img.rows;
                width = img.cols;
                active_track_ids.reserve(track_size);
                track_points.reserve(track_length);
            };

            ~TrackerVisualizer() = default;

            /*****************************************************************************
             * @brief 读取航迹管理器并绘制航迹
             * 航迹用渐变黑色线条绘制，新航迹点透明度较高，历史航迹点透明度较低，标注航迹号
             * 当航迹号错误，如内存超过范围、航迹点为0、航迹点超出边界，跳过该航迹，并报相关错误
             *
             * @param manager 航迹管理器对象
             *****************************************************************************/
            void draw_track(const TrackerManager &manager)
            {
                img.setTo(cv::Scalar(255, 255, 255)); // 清空图像

                // 使用公开只读接口获取活跃航迹ID并获取快照，无需访问私有成员
                std::vector<std::uint32_t> active_ids = manager.getActiveTrackIds();

                for (auto track_id : active_ids)
                {
                    using TrackPoint = track_project::communicate::TrackPoint;
                    using TrackerHeader = track_project::communicate::TrackerHeader;

                    // 使用零拷贝只读引用获取头部和数据
                    const TrackerHeader *header_ptr = manager.getHeaderRef(track_id);
                    const LatestKBuffer<TrackPoint> *data_ptr = manager.getDataRef(track_id);
                    if (!header_ptr || !data_ptr)
                    {
                        LOG_ERROR << "TrackerVisualizer: 无法获取航迹" << track_id << "的只读引用，跳过";
                        continue;
                    }

                    if (data_ptr->size() == 0)
                    {
                        LOG_ERROR << "TrackerVisualizer: 航迹ID" << track_id << "的航迹点为空，跳过该航迹绘制";
                        continue;
                    }

                    //************************航迹绘制
                    track_points.clear();
                    // 航迹计算，超界点跳过
                    for (size_t i = 0; i < data_ptr->size(); ++i)
                    {
                        const auto &point = (*data_ptr)[i];
                        cv::Point img_point;
                        img_point.x = static_cast<int>(((point.longitude - lon_min) / (lon_max - lon_min)) * width);
                        img_point.y = static_cast<int>(((lat_max - point.latitude) / (lat_max - lat_min)) * height);

                        if (img_point.x < 0 || img_point.x >= width || img_point.y < 0 || img_point.y >= height)
                        {
                            LOG_ERROR << "航迹ID" << track_id << "点" << i << "坐标超出图像范围，跳过该点";
                            continue;
                        }

                        track_points.push_back(img_point);
                    }

                    // 绘制航迹线条，使用颜色渐变表示时间
                    for (int i = 1; i < track_points.size(); ++i)
                    {
                        double age_ratio = static_cast<double>(i) / track_points.size();
                        int gray_value = static_cast<int>(255 * (1.0 - age_ratio));
                        cv::Scalar color(gray_value, gray_value, gray_value);
                        cv::line(img, track_points[i - 1], track_points[i], color, 2);
                    }

                    // **********************标注航迹号
                    if (!track_points.empty())
                    {
                        const cv::Point &latest_point = track_points.back();
                        std::string track_text = std::to_string(track_id);

                        // 设置文字参数
                        const int font_face = cv::FONT_HERSHEY_SIMPLEX;
                        const double font_scale = 0.6;
                        const int thickness = 2;
                        const cv::Scalar text_color(0, 0, 0);

                        // 计算文字位置（确保在图像范围内）
                        cv::Point text_org(latest_point.x + 5, latest_point.y - 5);

                        // 边界检查：确保文字不会超出图像边界
                        int text_width = track_text.length() * 12; // 估算文字宽度
                        int text_height = 20;                      // 估算文字高度

                        if (text_org.x + text_width >= width) // 横坐标判断
                        {
                            text_org.x = width - text_width - 5;
                        }
                        else if (text_org.x < 0)
                        {
                            text_org.x = 5;
                        }
                        if (text_org.y - text_height < 0) // 纵坐标判断
                        {
                            text_org.y = text_height + 5;
                        }
                        else if (text_org.y >= height)
                        {
                            text_org.y = height - 5;
                        }

                        // 直接绘制文字
                        cv::putText(img, track_text, text_org, font_face, font_scale,
                                    text_color, thickness, cv::LINE_AA);
                    }
                }

                cv::imshow("Track Visualizer", img);
                cv::waitKey(10);
            }

            /*****************************************************************************
             * @brief 打印航迹管理器完整状态到日志，包括统计信息（INFO）和内存池详情（DEBUG）
             *
             * @param manager
             * @version 0.1
             * @author xjl (xjl20011009@126.com)
             * @date 2025-11-29
             * @copyright Copyright (c) 2025
             *****************************************************************************/
            void printFullState(const TrackerManager &manager)
            {
                std::stringstream ss;
                ss << "\n"
                   << std::string(60, '=') << std::endl;
                ss << "              TRACKER MANAGER 完整状态" << std::endl;
                ss << std::string(60, '=') << std::endl;

                // 打印航迹管理类统计信息
                printStatistics(manager, ss);
                ss << std::endl;
                LOG_INFO << ss.str();

                // 打印内存池详情
                ss.clear();
                ss.str("");
                printMemoryPool(manager, ss);
                ss << std::endl;

                ss << std::string(60, '=') << "\n"
                   << std::endl;

                // 输出到日志
                LOG_DEBUG << ss.str();
            }

        private:
            // 1. 打印统计信息
            void printStatistics(const TrackerManager &manager, std::stringstream &ss)
            {
                ss << "系统统计:" << std::endl;
                ss << std::string(50, '-') << std::endl;
                ss << "  总容量: " << manager.getTotalCapacity() << " 个航迹" << std::endl;
                ss << "  使用中: " << manager.getUsedCount() << " 个航迹" << std::endl;
                ss << "  下个ID: " << manager.getNextTrackId() << std::endl;
            }

            // 2. 打印内存池详情
            void printMemoryPool(const TrackerManager &manager, std::stringstream &ss)
            {
                ss << "内存池详情 (" << manager.getTotalCapacity() << "个槽位):" << std::endl;
                ss << std::string(50, '-') << std::endl;

                size_t active_count = 0;
                std::vector<std::uint32_t> active_ids = manager.getActiveTrackIds();

                for (auto track_id : active_ids)
                {
                    const track_project::communicate::TrackerHeader *header_ptr = manager.getHeaderRef(track_id);
                    const LatestKBuffer<track_project::communicate::TrackPoint> *data_ptr = manager.getDataRef(track_id);
                    if (!header_ptr || !data_ptr)
                        continue;

                    active_count++;
                    ss << "  航迹" << std::setw(4) << header_ptr->track_id
                       << " [状态:" << std::setw(4) << stateToString(header_ptr->state)
                       << ", 外推:" << std::setw(1) << header_ptr->extrapolation_count
                       << ", 点数:" << std::setw(3) << data_ptr->size() << "]";
                    // 显示最近点的时间（如果有）
                    if (data_ptr->size() > 0)
                    {
                        const auto &latest_point = (*data_ptr)[data_ptr->size() - 1];
                        ss << " 最新时间:" << latest_point.time;
                    }
                    ss << std::endl;
                    ss << std::endl;
                }

                if (active_count == 0)
                {
                    ss << "  [无活跃航迹]" << std::endl;
                }
            }

            // 状态转换辅助函数
            const char *stateToString(int state)
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

        private:
            // 画布参数
            cv::Mat img;                               // 画布
            double lon_min, lon_max, lat_min, lat_max; // 经纬度范围
            int height, width;                         // 画布高度和宽度

            // 航迹点存放空间,为提高速度采用预分配方式，仅被draw_track使用
            std::vector<std::uint32_t> active_track_ids;
            std::vector<cv::Point> track_points;
        };

    } // namespace trackermanager
} // namespace track_project
#endif // TRACKER_VISUALIZER_HPP
#include <iostream>
#include "tgtcore.h"
#include "allsignals.h"
#include "defsystem.h"
#include "defstruct.h"
//通信
#include <boost/bind.hpp>
#include "dds_mpi/dds_mpi.h"
#include "dds_mpi/hrdac_codec.h"
#include "fms.h"

extern allsignals objsignal;
extern dds_mpi* ptr_mpi;

#define EARTH_R 6371.004

Tgtcore::Tgtcore()
{

    //************************************参数初始化
    // 通用参数初始化
    plane_num = 1;
    ship_num = 10;

    // 数据生成格式
    int angle_steps = 21;    // -50,-45,-40,...,0,...,+45,+50 (共21个角度)
    int distance_steps = 17; // 50,75,100,...,450 (共17个距离)

    // 开辟空间
    point_plane = (POINT *)malloc(sizeof(POINT) * plane_num);
    point_ship = (POINT *)malloc(sizeof(POINT) * ship_num);

    // ADS数据点
    for (int i = 0; i < plane_num; ++i)
    {
        int base_idx = i / 2;   // 基础位置索引 (0-356)
        int speed_type = i % 2; // 速度类型: 0=最小速度, 1=最大速度

        int angle_idx = base_idx / distance_steps;    // 角度索引 (0-20)
        int distance_idx = base_idx % distance_steps; // 距离索引 (0-16)

        // 角度: -50 ~ +50, 间隔5度
        float angle = -50.0f + angle_idx * 5.0f;
        // 距离: 50 ~ 450km, 间隔25km (50,75,100,...,450)
        float distance = 50.0f + distance_idx * 25.0f;

        // COG与ANGLE保持一致
        float cog = angle;
        // 速度：最小40或最大400
        float sog = (speed_type == 0) ? 40.0f : 400.0f;

        point_plane[i].x = 1000 * distance * qCos(qDegreesToRadians(-angle + BASEMENT_DBETAR));
        point_plane[i].y = 1000 * distance * qSin(qDegreesToRadians(-angle + BASEMENT_DBETAR));

        point_plane[i].vx = sog * qCos(qDegreesToRadians(-cog + BASEMENT_DBETAR));
        point_plane[i].vy = sog * qSin(qDegreesToRadians(-cog + BASEMENT_DBETAR));
    }

    // AIS数据点
    for (int i = 0; i < ship_num; ++i)
    {
        int base_idx = i / 2;   // 基础位置索引 (0-356)
        int speed_type = i % 2; // 速度类型: 0=最小速度, 1=最大速度

        int angle_idx = base_idx / distance_steps;    // 角度索引 (0-20)
        int distance_idx = base_idx % distance_steps; // 距离索引 (0-16)

        // 角度: -50 ~ +50, 间隔5度
        float angle = -50.0f + angle_idx * 5.0f;
        // 距离: 50 ~ 450km, 间隔25km
        float distance = 50.0f + distance_idx * 25.0f;

        // COG与ANGLE保持一致
        float cog = angle;
        // 速度：最小3或最大30
        float sog = (speed_type == 0) ? 3.0f : 30.0f;

        point_ship[i].x = 1000 * distance * qCos(qDegreesToRadians(angle));
        point_ship[i].y = 1000 * distance * qSin(qDegreesToRadians(angle));

        point_ship[i].vx = sog * qCos(qDegreesToRadians(cog));
        point_ship[i].vy = sog * qSin(qDegreesToRadians(cog));
    }

    // 初始化定时器
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Tgtcore::tgtsim_next);
}
Tgtcore::~Tgtcore()
{
    // 释放开辟的点迹空间s
    free(point_plane);
    free(point_ship);
}

// 给ADSX数据库包个时间戳
template <typename T>
void Tgtcore::getSystemTime(T &it)
{
    // 使用C++11 chrono库获取微秒级时间（跨平台）
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

    // 转换为秒和微秒部分
    time_t seconds = micros / 1000000;
    int microseconds = micros % 1000000;
    int milliseconds = microseconds / 1000;

    // 转换为本地时间的年月日时分秒
    struct tm *local_time = localtime(&seconds);

    // 填充到int数组
    it.tm_year = local_time->tm_year + 1900; // 年
    it.tm_month = local_time->tm_mon + 1;    // 月（0-11 → 1-12）
    it.tm_day = local_time->tm_mday;         // 日
    it.tm_hour = local_time->tm_hour;        // 时
    it.tm_min = local_time->tm_min;          // 分
    it.tm_sec = local_time->tm_sec;          // 秒
    it.tm_msec = milliseconds;               // 毫秒
    it.tm_wsec = microseconds % 1000;        // 微秒（剩余部分）
    it.tm_nsec = 0;
}

void Tgtcore::tgtsim_next()
{

    //*****************************执行飞机
    // 点迹区域处理
    for (int i = 0; i < plane_num; ++i)
    {
        POINT &p = point_plane[i];
        struct adsx temp_adsx;
        int true_id=i+1;
        memcpy(&temp_adsx.icao[0], &true_id, 4);

        // 加入时间戳
        getSystemTime(temp_adsx);

        // 经纬度计算 haversine算法
        float distance = hypotf(p.x, p.y) / 1000.0;
        float bearing = atan2f(p.y, p.x);
        float temp_lat = asinf(sinf(BASEMENT_DLAR_RAD) * cosf(distance / EARTH_R) +
                               cosf(BASEMENT_DLAR_RAD) * sinf(distance / EARTH_R) * cosf(bearing));
        float temp_lon = BASEMENT_DLOR_RAD + atan2(sinf(bearing) * sinf(distance / EARTH_R) * cosf(BASEMENT_DLAR_RAD),
                                                   cosf(distance / EARTH_R) - sinf(BASEMENT_DLAR_RAD) * sinf(temp_lat));
        temp_adsx.Latitude = temp_lat * 180 / 3.1415926;
        temp_adsx.Longitude = temp_lon * 180 / 3.1415926;

        // 航向航速计算，速度差值收曲率影响小，可以忽略
        temp_adsx.speed = hypotf(p.vx, p.vy);
        temp_adsx.angle = (90 - atan2f(p.vy, p.vx) * 180 / 3.1415926);
        if (temp_adsx.angle < 0)
            temp_adsx.angle += 360.0f;

        // 发送数据
        objsignal.ads_sim(temp_adsx);
        memcpy(send_buffer,&temp_adsx,128);
        HtoN((int*)send_buffer,32);
        send_buffer[4]=0x01;
        ptr_mpi->send_data_adsb_message(24, send_buffer, 128);
        // 数据刷新
        tgtsim_update(p, update_time);
    }

    //*****************************执行船只
    // 点迹区域处理
    for (int i = 0; i < ship_num; ++i)
    {
        POINT &p = point_ship[i];
        struct aisx temp_aisx;
        memcpy(&temp_aisx.mmsi[0], &i, 4);
        temp_aisx.id=1;//TODO

        // 加入时间戳
        getSystemTime(temp_aisx);

        // 经纬度计算 haversine算法
        float distance = hypotf(p.x, p.y) / 1000.0;
        float bearing = atan2f(p.y, p.x);
        float temp_lat = asinf(sinf(BASEMENT_DLAR_RAD) * cosf(distance / EARTH_R) +
                               cosf(BASEMENT_DLAR_RAD) * sinf(distance / EARTH_R) * cosf(bearing));
        float temp_lon = BASEMENT_DLOR_RAD + atan2(sinf(bearing) * sinf(distance / EARTH_R) * cosf(BASEMENT_DLAR_RAD),
                                                   cosf(distance / EARTH_R) - sinf(BASEMENT_DLAR_RAD) * sinf(temp_lat));
        temp_aisx.Latitude = temp_lat * 180 / 3.1415926;
        temp_aisx.Longitude = temp_lon * 180 / 3.1415926;

        // 航向航速计算，速度差值收曲率影响小，可以忽略
        temp_aisx.cog = hypotf(p.vx, p.vy) * 1.94384;   // 单位，节
        temp_aisx.sog = (90 - atan2f(p.vy, p.vx)) * 10; // 北偏东，单位0.1度
        if (temp_aisx.sog < 0)
            temp_aisx.sog += 360.0f;

        // 发送数据
        objsignal.ais_sim(temp_aisx);
        memcpy(send_buffer,&temp_aisx,128);
        HtoN((int*)send_buffer,32);
        ptr_mpi->send_data_ais_message(24, send_buffer, 128);

        // 数据刷新
        tgtsim_update(p, update_time);
    }
}

void Tgtcore::tgtsim_update(POINT &p, float tacc)
{
    p.x = p.x + tacc * p.vx;
    p.y = p.y + tacc * p.vy;
    if (p.x < 1e-4)
        p.x = 1e-4;
    if (p.vx < 1e-4)
        p.vx = 1e-4;
}

void Tgtcore::tgtsim_start(const int time_ms)
{
    update_time = (float)(time_ms) / 1000.0f;
    timer->start(time_ms);
}

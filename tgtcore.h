#ifndef TGTCORE_H
#define TGTCORE_H

/* 该类仅用于内部构造AIS/ADSB数据，可以被直接删除 */

#include <QObject>
#include <QTimer>
#include <QRandomGenerator>
#include <QtMath>

class Tgtcore : public QObject {
    Q_OBJECT
public:
    explicit Tgtcore();
    ~Tgtcore();

    //态势模拟启动
    void tgtsim_start(const int time_ms);  // 启动模拟，仅允许使用一次

private:
    void HtoN(int *buf, int len)
    {
        for (int nn = 0; nn < len; nn++)
        {
            buf[nn] = (buf[nn] & 0x000000FF) << 24 | (buf[nn] & 0x0000FF00) << 8 | (buf[nn] & 0x00FF0000) >> 8 | (buf[nn] & 0xFF000000) >> 24;
        }
    }

    //类内点迹定义
    struct POINT {
        float x;
        float y;
        float vx; // 航向角（相对于y轴顺时针度数）
        float vy; // 航速
    };

    QTimer* timer;       // 定时器
    int plane_num;  // 飞机边界内ADS点的数量
    int ship_num;  // 船只边界内AIS点的数量
    float update_time;    //存储更新时间

    char send_buffer[128];

    //点迹指针
    POINT* point_plane;
    POINT* point_ship;

    template <typename T>
    void getSystemTime(T& it);

    void tgtsim_next();  // 仿真数据发包
    void tgtsim_update(POINT& p,float tacc); // 点更新

};

#endif // TGTCORE_H

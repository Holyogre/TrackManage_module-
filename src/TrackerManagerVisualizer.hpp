#ifndef TRACKER_MANAGER_VISUALIZER_HPP
#define TRACKER_MANAGER_VISUALIZER_HPP

#include <opencv2/opencv.hpp>
#include <vector>

namespace trackviz {

// Draw a filled or stroked point (circle) on img.
// - img: destination image (CV_8UC3 recommended)
// - pt: point coordinates
// - color: BGR color (default red)
// - radius: circle radius in pixels
// - thickness: -1 for filled, >0 for outline
inline void drawPoint(cv::Mat& img,
                      const cv::Point& pt,
                      const cv::Scalar& color = cv::Scalar(0, 0, 255),
                      int radius = 5,
                      int thickness = -1) {
    if (img.empty()) return;
    cv::circle(img, pt, radius, color, thickness, cv::LINE_AA);
}

// Draw a polyline connecting pts.
// - closed: whether to close the polyline
inline void drawPolyline(cv::Mat& img,
                         const std::vector<cv::Point>& pts,
                         const cv::Scalar& color = cv::Scalar(0, 255, 0),
                         int thickness = 2,
                         bool closed = false) {
    if (img.empty() || pts.empty()) return;
    std::vector<cv::Point> tmp = pts; // polylines takes a non-const vector
    cv::polylines(img, tmp, closed, color, thickness, cv::LINE_AA);
}

// Example that creates a canvas, draws a point and a polyline, then shows the result.
// Call trackviz::showExample(); from a main() to run.
inline void showExample(int width = 640, int height = 480) {
    cv::Mat canvas(height, width, CV_8UC3, cv::Scalar(30, 30, 30));

    // Example single point
    cv::Point pt(width / 4, height / 2);
    drawPoint(canvas, pt, cv::Scalar(0, 0, 255), 8, -1);

    // Example polyline (you can replace these points)
    std::vector<cv::Point> poly{
        {width / 2 - 100, height / 2 + 50},
        {width / 2 - 50,  height / 2 - 40},
        {width / 2 + 20,  height / 2 + 30},
        {width / 2 + 100, height / 2 - 80}
    };
    drawPolyline(canvas, poly, cv::Scalar(0, 255, 0), 3, false);

    // Mark polyline vertices
    for (const auto& v : poly) drawPoint(canvas, v, cv::Scalar(255, 200, 0), 4, -1);

    const char* win = "TrackerManager Visualizer - example";
    cv::imshow(win, canvas);
    cv::waitKey(0);
    cv::destroyWindow(win);
}

} // namespace trackviz
#endif // TRACKER_MANAGER_VISUALIZER_HPP
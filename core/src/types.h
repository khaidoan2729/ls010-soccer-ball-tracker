#pragma once
// Data contracts passed between stages. Stages only talk to each other through these types.
// All times are seconds since program start (now_s()); all coordinates are frame pixels.
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace ballcore {

inline double now_s() {
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// Capture -> everyone. `image` is shared between consumers: treat as read-only.
struct Frame {
    cv::Mat image;  // BGR
    double t = 0;
    uint64_t seq = 0;
};

// Detector -> Tracker.
struct Detection {
    cv::Rect2f box;          // top-left x, y, width, height
    float confidence = 0;    // 0..1
};

// Tracker -> PanController, Analytics, UI.
struct TrackPoint {
    double t = 0;
    float x = 0, y = 0;      // ball centre
};
struct TrackState {
    double t = 0;
    bool visible = false;
    Detection ball;                  // valid when visible
    std::vector<TrackPoint> trail;   // last 1 s, oldest first
};

// Analytics -> UI / recording.
struct Analytics {
    double speed_mps = 0;
    double height_m = 0;
    std::string zone;                            // "defence" | "middle" | "attack"
    std::map<std::string, double> zone_time_s;   // per zone
};

// Everything the UI needs; serialised as /api/state.
struct PipelineState {
    double t = 0;
    bool camera_ok = false;
    int frame_width = 0, frame_height = 0;
    double capture_fps = 0, processing_fps = 0;
    TrackState track;
    Analytics analytics;
    double pan_deg = 0;
    bool pan_manual = false;
    bool recording = false;
};

}  // namespace ballcore

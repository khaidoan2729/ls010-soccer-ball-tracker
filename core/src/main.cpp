// ballcore walking skeleton: every stage exists behind an interface and is wired together.
//
//   capture thread:   ICamera -> Latest<Frame>
//   pipeline thread:  Frame -> IDetector -> ITracker -> IPanController -> IServo
//                                              \-> IAnalytics -> PipelineState -> IRecorder
//   server threads:   /stream (Latest<Frame>), /api/state (Latest<PipelineState>), /api/pan, /api/record
//
// Usage: ballcore [camera_device=/dev/video0] [port=8080]

#include <atomic>
#include <csignal>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>

#include "analytics/analytics.h"
#include "capture/camera.h"
#include "control/pan.h"
#include "detect/detector.h"
#include "latest.h"
#include "record/recorder.h"
#include "server/http_server.h"
#include "track/tracker.h"
#include "types.h"

using namespace ballcore;

static std::atomic<bool> g_run{true};

// Events per second, updated about once a second.
struct Rate {
    double fps = 0, start = -1;
    int n = 0;
    void tick(double t) {
        if (start < 0) start = t;
        ++n;
        if (t - start >= 1.0) { fps = n / (t - start); n = 0; start = t; }
    }
};

int main(int argc, char** argv) {
    now_s();  // t = 0
    const std::string device = argc > 1 ? argv[1] : "/dev/video0";
    const int port = argc > 2 ? std::stoi(argv[2]) : 8080;
    std::signal(SIGINT, [](int) { g_run = false; });
    std::signal(SIGTERM, [](int) { g_run = false; });
    std::signal(SIGPIPE, SIG_IGN);

    // ---- stages (swap stubs for real implementations here) ----
    OpenCvCamera camera(device, 1280, 720, 30);
    StubDetector detector;
    StubTracker tracker;
    StubPanController pan;
    NullServo servo;
    StubAnalytics analytics;
    StubRecorder recorder;

    Latest<Frame> frames;
    Latest<PipelineState> state;
    std::atomic<bool> camera_ok{false};
    std::atomic<double> capture_fps{0};
    std::mutex pan_m;
    std::optional<double> manual_pan;  // set via /api/pan

    // ---- capture thread: keeps retrying if the camera is missing or lost (FR-SP-04) ----
    std::thread capture_thread([&] {
        Rate rate;
        uint64_t seq = 0;
        while (g_run) {
            if (!camera.open()) {
                std::cerr << "camera: cannot open " << device << ", retrying\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            cv::Mat img;  // fresh buffer each frame; consumers may still hold older frames
            while (g_run && camera.read(img)) {
                camera_ok = true;
                const double t = now_s();
                rate.tick(t);
                capture_fps = rate.fps;
                frames.publish(Frame{img, t, ++seq});
                img = cv::Mat();
            }
            camera_ok = false;
            capture_fps = 0;
            camera.close();
            if (g_run) {
                std::cerr << "camera: lost, reconnecting\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    });

    // ---- server ----
    ServerHooks hooks;
    hooks.state = [&] { return state.get(); };
    hooks.set_pan = [&](std::optional<double> a) { std::lock_guard<std::mutex> lk(pan_m); manual_pan = a; };
    hooks.set_recording = [&](bool on) {
        if (on) recorder.start(); else recorder.stop();
        return recorder.recording();
    };
    HttpServer server(BALLCORE_WEB_ROOT, frames, hooks);
    if (!server.start("0.0.0.0", port)) { g_run = false; capture_thread.join(); return 1; }
    std::cerr << "ballcore: http://0.0.0.0:" << port << "/\n";

    // ---- pipeline (this thread) ----
    Rate rate;
    uint64_t seq = 0;
    Frame f;
    PipelineState s;
    while (g_run) {
        const bool fresh = frames.wait_newer(seq, f, 0.5);
        if (fresh) {
            rate.tick(f.t);
            s.t = f.t;
            s.frame_width = f.image.cols;
            s.frame_height = f.image.rows;
            s.track = tracker.update(f.t, detector.detect(f));
            {
                std::lock_guard<std::mutex> lk(pan_m);
                s.pan_manual = manual_pan.has_value();
                s.pan_deg = s.pan_manual ? *manual_pan : pan.update(s.track, f.image.cols);
            }
            servo.set_angle(s.pan_deg);
            s.analytics = analytics.update(s.track);
        }
        s.camera_ok = camera_ok;
        s.capture_fps = capture_fps;
        s.processing_fps = s.camera_ok ? rate.fps : 0;
        s.recording = recorder.recording();
        if (fresh && s.recording) recorder.write(f, s);
        state.publish(s);
    }

    server.stop();
    capture_thread.join();
    return 0;
}

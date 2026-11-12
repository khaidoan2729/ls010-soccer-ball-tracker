#pragma once
#include <string>

#include <opencv2/videoio.hpp>

namespace ballcore {

// Frame source. Later: OAK-D, video-file playback.
class ICamera {
public:
    virtual ~ICamera() = default;
    virtual bool open() = 0;
    virtual bool read(cv::Mat& out) = 0;  // blocks for the next frame; false on failure
    virtual void close() = 0;
};

// USB webcam. Must use V4L2 + MJPG, otherwise the webcam falls back to YUYV at ~5 fps.
class OpenCvCamera : public ICamera {
public:
    OpenCvCamera(std::string device, int width, int height, int fps)
        : device_(std::move(device)), w_(width), h_(height), fps_(fps) {}

    bool open() override {
        if (!cap_.open(device_, cv::CAP_V4L2)) return false;
        cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
        cap_.set(cv::CAP_PROP_FRAME_WIDTH, w_);
        cap_.set(cv::CAP_PROP_FRAME_HEIGHT, h_);
        cap_.set(cv::CAP_PROP_FPS, fps_);
        return true;
    }
    bool read(cv::Mat& out) override { return cap_.read(out) && !out.empty(); }
    void close() override { cap_.release(); }

private:
    std::string device_;
    int w_, h_, fps_;
    cv::VideoCapture cap_;
};

}  // namespace ballcore

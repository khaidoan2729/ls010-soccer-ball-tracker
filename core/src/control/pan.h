#pragma once
#include <algorithm>

#include "types.h"

namespace ballcore {

// Servo backend. Later: SysfsPwmServo (hardware PWM, GPIO 18) or Pca9685Servo.
class IServo {
public:
    virtual ~IServo() = default;
    virtual void set_angle(double deg) = 0;  // 0 = centre
};

// Track -> desired pan angle (degrees, 0 = centre).
class IPanController {
public:
    virtual ~IPanController() = default;
    virtual double update(const TrackState& track, int frame_width) = 0;
};

// STUB: no hardware, just remembers the angle.
class NullServo : public IServo {
public:
    void set_angle(double deg) override { angle_ = deg; }
private:
    double angle_ = 0;
};

// STUB: nudges the angle toward the ball; holds when not visible. Limited to +/-limit_deg.
class StubPanController : public IPanController {
public:
    explicit StubPanController(double limit_deg = 30) : limit_(limit_deg) {}
    double update(const TrackState& tr, int frame_width) override {
        if (tr.visible && frame_width > 0) {
            const cv::Rect2f& b = tr.ball.box;
            double err = (b.x + b.width / 2 - frame_width / 2.0) / (frame_width / 2.0);  // -1..1
            angle_ = std::clamp(angle_ + 0.5 * err, -limit_, limit_);
        }
        return angle_;
    }
private:
    double limit_, angle_ = 0;
};

}  // namespace ballcore

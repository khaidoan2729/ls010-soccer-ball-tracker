#pragma once
#include <cmath>
#include <vector>

#include "types.h"

namespace ballcore {

// Frame -> candidate ball boxes. Later: OnnxDetector / NcnnDetector.
class IDetector {
public:
    virtual ~IDetector() = default;
    virtual std::vector<Detection> detect(const Frame& frame) = 0;
};

// STUB: returns a fake ball sweeping across the frame so downstream stages have data.
class StubDetector : public IDetector {
public:
    std::vector<Detection> detect(const Frame& f) override {
        const float w = static_cast<float>(f.image.cols), h = static_cast<float>(f.image.rows);
        const float s = h / 20;
        const float cx = w * (0.5f + 0.4f * static_cast<float>(std::sin(f.t)));
        const float conf = 0.6f + 0.4f * static_cast<float>(std::sin(f.t * 0.5));
        return {Detection{cv::Rect2f(cx - s / 2, h * 0.6f - s / 2, s, s), conf}};
    }
};

}  // namespace ballcore

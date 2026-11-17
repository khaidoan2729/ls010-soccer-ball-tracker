#pragma once
#include <algorithm>
#include <vector>

#include "types.h"

namespace ballcore {

// Detections over time -> one ball track with a 1 s trail.
class ITracker {
public:
    virtual ~ITracker() = default;
    virtual TrackState update(double t, const std::vector<Detection>& detections) = 0;
};

// STUB: takes the most confident detection; keeps the last 1 s of positions.
class StubTracker : public ITracker {
public:
    TrackState update(double t, const std::vector<Detection>& dets) override {
        state_.t = t;
        state_.visible = !dets.empty();
        if (state_.visible) {
            state_.ball = *std::max_element(dets.begin(), dets.end(),
                [](const Detection& a, const Detection& b) { return a.confidence < b.confidence; });
            const cv::Rect2f& b = state_.ball.box;
            state_.trail.push_back({t, b.x + b.width / 2, b.y + b.height / 2});
        }
        auto& tr = state_.trail;
        tr.erase(tr.begin(), std::find_if(tr.begin(), tr.end(),
                                          [t](const TrackPoint& p) { return t - p.t <= 1.0; }));
        return state_;
    }

private:
    TrackState state_;
};

}  // namespace ballcore

#pragma once
#include "types.h"

namespace ballcore {

// Track -> speed, height, zone time.
class IAnalytics {
public:
    virtual ~IAnalytics() = default;
    virtual Analytics update(const TrackState& track) = 0;
};

// STUB: placeholder values only.
class StubAnalytics : public IAnalytics {
public:
    Analytics update(const TrackState&) override {
        Analytics a;
        a.zone = "middle";
        a.zone_time_s = {{"defence", 0}, {"middle", 0}, {"attack", 0}};
        return a;
    }
};

}  // namespace ballcore

#pragma once
#include <atomic>

#include "types.h"

namespace ballcore {

// Session recording (video.mp4 + tracking.csv + summary.json).
// start()/stop() are called from the HTTP thread, write() from the pipeline thread.
class IRecorder {
public:
    virtual ~IRecorder() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool recording() const = 0;
    virtual void write(const Frame& frame, const PipelineState& state) = 0;
};

// STUB: tracks the on/off flag, writes nothing.
class StubRecorder : public IRecorder {
public:
    bool start() override { on_ = true; return true; }
    void stop() override { on_ = false; }
    bool recording() const override { return on_; }
    void write(const Frame&, const PipelineState&) override {}
private:
    std::atomic<bool> on_{false};
};

}  // namespace ballcore

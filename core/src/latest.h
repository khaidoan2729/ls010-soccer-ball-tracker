#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace ballcore {

// Single-slot mailbox between threads: the producer overwrites, consumers take the newest value
// and skip what they missed, so a slow consumer never slows the producer.
template <class T>
class Latest {
public:
    void publish(T v) {
        {
            std::lock_guard<std::mutex> lk(m_);
            value_ = std::move(v);
            ++seq_;
        }
        cv_.notify_all();
    }

    T get() const {
        std::lock_guard<std::mutex> lk(m_);
        return value_;
    }

    // Wait for a value newer than `seq`; on success copies it and updates `seq`.
    bool wait_newer(uint64_t& seq, T& out, double timeout_s) {
        std::unique_lock<std::mutex> lk(m_);
        if (!cv_.wait_for(lk, std::chrono::duration<double>(timeout_s), [&] { return seq_ > seq; }))
            return false;
        out = value_;
        seq = seq_;
        return true;
    }

private:
    mutable std::mutex m_;
    std::condition_variable cv_;
    T value_{};
    uint64_t seq_ = 0;
};

}  // namespace ballcore

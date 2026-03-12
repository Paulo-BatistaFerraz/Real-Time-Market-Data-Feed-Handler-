#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include "qf/common/types.hpp"

namespace qf::data {

// ReplayClock — virtual clock for historical data replay.
// Provides virtual time that can run faster/slower than real time.
//   speed 1.0 = real-time, 10.0 = 10× faster, 0.0 = as-fast-as-possible.
class ReplayClock {
public:
    ReplayClock();

    // Set the playback speed multiplier.
    //   1.0 = real-time, 10.0 = 10×, 0.0 = as-fast-as-possible (no sleeping).
    void set_speed(double multiplier);

    // Current speed multiplier.
    double speed() const { return speed_.load(std::memory_order_relaxed); }

    // Start the clock at the given virtual origin timestamp.
    void start(Timestamp origin);

    // Return current virtual timestamp (ns since midnight).
    Timestamp now() const;

    // Block until the virtual clock reaches the given timestamp.
    // Returns immediately if already past that time, or if speed == 0.
    void wait_until(Timestamp target);

    // Advance the virtual clock to the given timestamp (for as-fast-as-possible mode).
    void advance_to(Timestamp ts);

    // Whether the clock has been started.
    bool running() const { return running_.load(std::memory_order_acquire); }

private:
    using SteadyClock = std::chrono::steady_clock;
    using TimePoint   = SteadyClock::time_point;

    std::atomic<double>   speed_{1.0};
    std::atomic<bool>     running_{false};

    // Wall-clock time when start() was called.
    TimePoint wall_origin_;

    // Virtual origin timestamp (ns since midnight).
    Timestamp virtual_origin_{0};

    // For as-fast-as-possible mode: manually advanced virtual time.
    std::atomic<Timestamp> manual_now_{0};

    // For wait_until signaling.
    mutable std::mutex              mtx_;
    mutable std::condition_variable cv_;
};

}  // namespace qf::data

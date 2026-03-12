#include "qf/data/replay/replay_clock.hpp"

#include <algorithm>
#include <thread>

namespace qf::data {

ReplayClock::ReplayClock() = default;

void ReplayClock::set_speed(double multiplier) {
    if (multiplier < 0.0) multiplier = 0.0;

    // If running, we need to re-anchor: save current virtual time, reset wall origin.
    if (running_.load(std::memory_order_acquire)) {
        Timestamp cur = now();
        {
            std::lock_guard<std::mutex> lk(mtx_);
            virtual_origin_ = cur;
            wall_origin_    = SteadyClock::now();
            manual_now_.store(cur, std::memory_order_relaxed);
        }
    }

    speed_.store(multiplier, std::memory_order_relaxed);
    cv_.notify_all();
}

void ReplayClock::start(Timestamp origin) {
    std::lock_guard<std::mutex> lk(mtx_);
    virtual_origin_ = origin;
    manual_now_.store(origin, std::memory_order_relaxed);
    wall_origin_ = SteadyClock::now();
    running_.store(true, std::memory_order_release);
    cv_.notify_all();
}

Timestamp ReplayClock::now() const {
    if (!running_.load(std::memory_order_acquire)) {
        return manual_now_.load(std::memory_order_relaxed);
    }

    double spd = speed_.load(std::memory_order_relaxed);

    // As-fast-as-possible: virtual time is advanced manually.
    if (spd == 0.0) {
        return manual_now_.load(std::memory_order_relaxed);
    }

    auto elapsed_wall = SteadyClock::now() - wall_origin_;
    auto wall_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed_wall).count();
    auto virtual_elapsed = static_cast<uint64_t>(static_cast<double>(wall_ns) * spd);
    return virtual_origin_ + virtual_elapsed;
}

void ReplayClock::wait_until(Timestamp target) {
    double spd = speed_.load(std::memory_order_relaxed);

    // As-fast-as-possible: no sleeping, just advance.
    if (spd == 0.0) {
        return;
    }

    // Already past the target?
    if (now() >= target) {
        return;
    }

    // Calculate how long to sleep in wall time.
    Timestamp cur_virtual = now();
    if (cur_virtual >= target) return;

    uint64_t virtual_delta = target - cur_virtual;
    auto wall_ns = static_cast<uint64_t>(static_cast<double>(virtual_delta) / spd);

    // Use a condition variable with timeout so speed changes wake us up.
    auto deadline = SteadyClock::now() + std::chrono::nanoseconds(wall_ns);

    std::unique_lock<std::mutex> lk(mtx_);
    cv_.wait_until(lk, deadline, [&] {
        return now() >= target;
    });
}

void ReplayClock::advance_to(Timestamp ts) {
    manual_now_.store(ts, std::memory_order_relaxed);
    cv_.notify_all();
}

}  // namespace qf::data

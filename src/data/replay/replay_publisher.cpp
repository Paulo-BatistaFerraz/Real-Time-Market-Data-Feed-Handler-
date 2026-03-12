#include "qf/data/replay/replay_publisher.hpp"

#include <cstring>

namespace qf::data {

ReplayPublisher::ReplayPublisher(MultiStreamMerger& merger, ReplayClock& clock,
                                 PublishCallback callback)
    : merger_(merger), clock_(clock), callback_(std::move(callback)) {}

void ReplayPublisher::start() {
    running_.store(true, std::memory_order_release);
    paused_.store(false, std::memory_order_release);
    ticks_published_.store(0, std::memory_order_relaxed);
    bytes_published_.store(0, std::memory_order_relaxed);
    seek_target_.store(0, std::memory_order_relaxed);

    bool clock_started = false;

    merger_.merge([&](const TickRecord& tick) {
        if (!running_.load(std::memory_order_acquire)) return;

        // Skip ticks before the seek target.
        Timestamp seek_ts = seek_target_.load(std::memory_order_acquire);
        if (seek_ts > 0 && tick.timestamp < seek_ts) return;

        // Handle pause/resume.
        if (!wait_if_paused()) return;  // stop() was called while paused

        // Start the replay clock at the first non-skipped tick.
        if (!clock_started) {
            clock_.start(tick.timestamp);
            clock_started = true;
        }

        // Wait until virtual time reaches this tick's timestamp.
        clock_.wait_until(tick.timestamp);
        if (!running_.load(std::memory_order_acquire)) return;

        // In as-fast-as-possible mode, advance the clock manually.
        if (clock_.speed() == 0.0) {
            clock_.advance_to(tick.timestamp);
        }

        // Publish the raw payload through the pipeline callback.
        callback_(tick.payload, tick.payload_length, tick.timestamp);

        ticks_published_.fetch_add(1, std::memory_order_relaxed);
        bytes_published_.fetch_add(tick.payload_length, std::memory_order_relaxed);
    });

    running_.store(false, std::memory_order_release);
}

void ReplayPublisher::stop() {
    running_.store(false, std::memory_order_release);
    merger_.stop();
    // Wake any thread blocked in wait_if_paused.
    {
        std::lock_guard<std::mutex> lock(pause_mtx_);
        paused_.store(false, std::memory_order_release);
    }
    pause_cv_.notify_all();
}

void ReplayPublisher::pause() {
    paused_.store(true, std::memory_order_release);
}

void ReplayPublisher::resume() {
    {
        std::lock_guard<std::mutex> lock(pause_mtx_);
        paused_.store(false, std::memory_order_release);
    }
    pause_cv_.notify_all();
}

void ReplayPublisher::seek(Timestamp target) {
    seek_target_.store(target, std::memory_order_release);
}

bool ReplayPublisher::wait_if_paused() {
    std::unique_lock<std::mutex> lock(pause_mtx_);
    pause_cv_.wait(lock, [this] {
        return !paused_.load(std::memory_order_acquire) ||
               !running_.load(std::memory_order_acquire);
    });
    return running_.load(std::memory_order_acquire);
}

}  // namespace qf::data

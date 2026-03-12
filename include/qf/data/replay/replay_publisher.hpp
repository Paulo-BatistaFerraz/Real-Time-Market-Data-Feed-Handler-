#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include "qf/common/types.hpp"
#include "qf/data/data_types.hpp"
#include "qf/data/replay/multi_stream_merger.hpp"
#include "qf/data/replay/replay_clock.hpp"

namespace qf::data {

// Callback matching the pipeline's Q1 producer interface:
// (raw_data, length, receive_timestamp_ns)
using PublishCallback = std::function<void(const uint8_t*, size_t, uint64_t)>;

// ReplayPublisher — publishes merged multi-symbol events to the pipeline's Q1.
// Same interface as MulticastReceiver so it can be a drop-in replacement.
// Supports pause/resume/seek operations for interactive replay.
class ReplayPublisher {
public:
    ReplayPublisher(MultiStreamMerger& merger, ReplayClock& clock,
                    PublishCallback callback);

    // Start publishing merged events through the pipeline.
    // Blocks the calling thread until all events are published or stop() is called.
    void start();

    // Stop publishing.
    void stop();

    // Pause replay — events stop being emitted until resume() is called.
    void pause();

    // Resume after a pause.
    void resume();

    // Seek to a specific timestamp. Only valid while paused.
    // Ticks before this timestamp will be skipped on resume.
    void seek(Timestamp target);

    // Whether the publisher is currently running (may be paused).
    bool running() const { return running_.load(std::memory_order_acquire); }

    // Whether the publisher is currently paused.
    bool paused() const { return paused_.load(std::memory_order_acquire); }

    // Current seek position (0 = from start, or last seek target).
    Timestamp seek_position() const { return seek_target_.load(std::memory_order_relaxed); }

    // Stats.
    uint64_t ticks_published() const { return ticks_published_.load(std::memory_order_relaxed); }
    uint64_t bytes_published() const { return bytes_published_.load(std::memory_order_relaxed); }

private:
    // Block while paused, returning false if stop() was called.
    bool wait_if_paused();

    MultiStreamMerger& merger_;
    ReplayClock&       clock_;
    PublishCallback    callback_;

    std::atomic<bool>      running_{false};
    std::atomic<bool>      paused_{false};
    std::atomic<Timestamp> seek_target_{0};

    std::mutex              pause_mtx_;
    std::condition_variable pause_cv_;

    std::atomic<uint64_t> ticks_published_{0};
    std::atomic<uint64_t> bytes_published_{0};
};

}  // namespace qf::data

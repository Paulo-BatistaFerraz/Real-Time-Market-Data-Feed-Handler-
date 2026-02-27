#pragma once

#include <chrono>
#include <cstdint> 

namespace mdfh {

struct Clock {
    using HiResClock = std::chrono::high_resolution_clock;

    static uint64_t now_ns() {
        auto now = HiResClock::now();
        auto since_epoch = now.time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch).count()
        );
    }

    // Nanoseconds since midnight (for protocol timestamps)
    static uint64_t nanos_since_midnight() {
        auto now = std::chrono::system_clock::now();
        auto today = std::chrono::floor<std::chrono::days>(now);
        auto since_midnight = now - today;
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(since_midnight).count()
        );
    }
};

}  // namespace mdfh

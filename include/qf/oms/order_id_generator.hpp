#pragma once

#include <atomic>
#include <cstdint>

namespace qf::oms {

// Thread-safe monotonically increasing order ID generator.
// Uses atomic increment — safe to call from any thread.
class OrderIdGenerator {
public:
    explicit OrderIdGenerator(uint64_t start = 1) : next_id_(start) {}

    // Generate the next unique ID. Thread-safe.
    uint64_t next() { return next_id_.fetch_add(1, std::memory_order_relaxed); }

    // Current value (next ID that will be issued). For diagnostics only.
    uint64_t peek() const { return next_id_.load(std::memory_order_relaxed); }

    // Total number of IDs issued so far.
    uint64_t count() const { return next_id_.load(std::memory_order_relaxed) - start_; }

private:
    std::atomic<uint64_t> next_id_;
    uint64_t start_{1};
};

}  // namespace qf::oms

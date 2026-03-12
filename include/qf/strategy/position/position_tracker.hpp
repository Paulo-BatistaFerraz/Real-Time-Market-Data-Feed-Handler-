#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include "qf/common/types.hpp"

namespace qf::strategy {

// PositionTracker — tracks net position per symbol.
//
// Positive values indicate a long position, negative values indicate short.
// Thread-safety: not thread-safe; callers must synchronise externally.
class PositionTracker {
public:
    // Adjust position for |symbol| by |quantity| units on the given |side|.
    // Buy increases position; Sell decreases it.
    void update(const Symbol& symbol, Side side, Quantity quantity);

    // Return net position for |symbol| (0 if no position recorded).
    int64_t get(const Symbol& symbol) const;

    // Return a snapshot of all positions (keyed by symbol's raw bytes).
    const std::unordered_map<uint64_t, int64_t>& get_all() const;

    // Reset all positions to zero.
    void reset();

private:
    // Keyed by Symbol::as_key() for O(1) hashing.
    std::unordered_map<uint64_t, int64_t> positions_;
};

}  // namespace qf::strategy

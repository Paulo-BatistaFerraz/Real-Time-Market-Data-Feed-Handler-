#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include "qf/common/types.hpp"

namespace qf::strategy {

// A single fill record.
struct Fill {
    Timestamp timestamp;
    Symbol    symbol;
    Side      side;
    Price     price;
    Quantity  quantity;
};

// FillTracker — records every fill and provides analytics per symbol.
//
// Thread-safety: not thread-safe; callers must synchronise externally.
class FillTracker {
public:
    // Record a new fill.
    void record(Timestamp ts, const Symbol& symbol, Side side,
                Price price, Quantity quantity);

    // Return volume-weighted average fill price for |symbol| (0 if none).
    double average_price(const Symbol& symbol) const;

    // Return all fills for |symbol|.
    const std::vector<Fill>& fills(const Symbol& symbol) const;

    // Return total fill count across all symbols.
    size_t total_fills() const;

    // Reset all recorded fills.
    void reset();

private:
    // Keyed by Symbol::as_key().
    std::unordered_map<uint64_t, std::vector<Fill>> fills_;
    static const std::vector<Fill> empty_;
};

}  // namespace qf::strategy

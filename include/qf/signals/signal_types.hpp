#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "qf/common/types.hpp"

namespace qf::signals {

// Signal direction: the recommended trade action
enum class SignalDirection : uint8_t {
    Buy     = 0,
    Sell    = 1,
    Neutral = 2
};

// Signal strength classification
enum class SignalStrength : uint8_t {
    Strong   = 0,
    Moderate = 1,
    Weak     = 2,
    None     = 3
};

// Signal type identifies the source/category of the signal
enum class SignalType : uint8_t {
    Momentum       = 0,
    MeanReversion  = 1,
    OrderFlow      = 2,
    Volatility     = 3,
    Composite      = 4,
    ML             = 5
};

// A trading signal produced by the signal engine.
// strength is clamped to [-1, +1]: negative = bearish, positive = bullish.
struct Signal {
    Symbol         symbol;
    SignalType     type;
    double         strength;    // [-1.0, +1.0]
    SignalDirection direction;
    uint64_t       timestamp;   // nanoseconds since midnight

    // Classify strength into discrete buckets
    SignalStrength classify() const {
        double abs_str = strength < 0.0 ? -strength : strength;
        if (abs_str >= 0.7) return SignalStrength::Strong;
        if (abs_str >= 0.4) return SignalStrength::Moderate;
        if (abs_str > 0.0)  return SignalStrength::Weak;
        return SignalStrength::None;
    }
};

// A vector of named feature values, produced by feature extraction.
struct FeatureVector {
    std::unordered_map<std::string, double> values;  // feature_name -> value
    uint64_t timestamp;  // nanoseconds since midnight

    void set(const std::string& name, double value) {
        values[name] = value;
    }

    double get(const std::string& name, double default_val = 0.0) const {
        auto it = values.find(name);
        return (it != values.end()) ? it->second : default_val;
    }

    bool has(const std::string& name) const {
        return values.find(name) != values.end();
    }

    size_t size() const { return values.size(); }
};

// A single trade record used in trade history
struct TradeRecord {
    Symbol   symbol;
    Price    price;
    Quantity quantity;
    Side     aggressor_side;  // who initiated the trade
    uint64_t timestamp;
};

// Rolling trade history for a symbol, used by features that need recent trade data
class TradeHistory {
public:
    explicit TradeHistory(size_t max_size = 10000)
        : max_size_(max_size) {}

    void add(const TradeRecord& trade) {
        if (trades_.size() >= max_size_) {
            trades_.erase(trades_.begin());
        }
        trades_.push_back(trade);
    }

    const std::vector<TradeRecord>& trades() const { return trades_; }
    size_t size() const { return trades_.size(); }
    bool empty() const { return trades_.empty(); }

    const TradeRecord& latest() const { return trades_.back(); }
    const TradeRecord& oldest() const { return trades_.front(); }

    void clear() { trades_.clear(); }

private:
    std::vector<TradeRecord> trades_;
    size_t max_size_;
};

}  // namespace qf::signals

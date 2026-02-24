#pragma once

#include <cstdint>
#include <cstring>

namespace mdfh {

enum class Side : uint8_t {
    Buy  = 0x01,
    Sell = 0x02
};

// Fixed-point price: actual_price = raw_price / 10000.0
// e.g., 1850500 = $185.0500
using Price = uint32_t;
using Quantity = uint32_t;
using OrderId = uint64_t;
using Timestamp = uint64_t;  // nanoseconds since midnight

constexpr size_t SYMBOL_LENGTH = 8;

struct Symbol {
    char data[SYMBOL_LENGTH];

    Symbol() { std::memset(data, 0, SYMBOL_LENGTH); }

    explicit Symbol(const char* str) {
        std::memset(data, 0, SYMBOL_LENGTH);
        std::strncpy(data, str, SYMBOL_LENGTH);
    }

    // Reinterpret as uint64 for O(1) hashing/comparison
    uint64_t as_key() const {
        uint64_t key;
        std::memcpy(&key, data, sizeof(key));
        return key;
    }

    bool operator==(const Symbol& other) const {
        return as_key() == other.as_key();
    }
};

struct SymbolHash {
    size_t operator()(const Symbol& s) const {
        return std::hash<uint64_t>{}(s.as_key());
    }
};

// Convert fixed-point price to display string
inline double price_to_double(Price p) {
    return static_cast<double>(p) / 10000.0;
}

inline Price double_to_price(double d) {
    return static_cast<Price>(d * 10000.0);
}

}  // namespace mdfh

#pragma once

#include <cstdint>
#include <cstring>
#include <array>
#include "qf/common/types.hpp"

namespace qf::data {

// Maximum raw payload size for a single tick record.
static constexpr size_t MAX_PAYLOAD_SIZE = 128;

// A single market data tick as received from the feed.
#pragma pack(push, 1)
struct TickRecord {
    Timestamp timestamp;          // nanoseconds since midnight
    Symbol    symbol;             // 8-byte fixed symbol
    char      message_type;       // 'A','X','E','R','T' (mirrors protocol MessageType)
    uint16_t  payload_length;     // actual bytes stored in payload
    uint8_t   payload[MAX_PAYLOAD_SIZE]; // raw wire bytes or parsed fields

    TickRecord() : timestamp{0}, message_type{0}, payload_length{0} {
        std::memset(payload, 0, MAX_PAYLOAD_SIZE);
    }
};
#pragma pack(pop)

// An OHLCV bar aggregated from ticks.
struct Bar {
    Symbol    symbol;
    Price     open;
    Price     high;
    Price     low;
    Price     close;
    uint64_t  volume;       // total quantity
    uint32_t  tick_count;   // number of ticks in bar
    Timestamp start_time;   // bar open time
    Timestamp end_time;     // bar close time

    Bar() : open{0}, high{0}, low{0}, close{0},
            volume{0}, tick_count{0}, start_time{0}, end_time{0} {}
};

// A snapshot of top-of-book market state at a point in time.
struct MarketSnapshot {
    Symbol    symbol;
    Price     best_bid;
    Price     best_ask;
    Price     last_trade_price;
    Timestamp timestamp;

    MarketSnapshot() : best_bid{0}, best_ask{0},
                       last_trade_price{0}, timestamp{0} {}
};

}  // namespace qf::data

#pragma once

#include <cstdint>
#include <cstddef>
#include "mdfh/common/types.hpp"
#include "mdfh/common/constants.hpp"
#include "mdfh/protocol/messages.hpp"

namespace mdfh::core {

// Q1: Network → Parser
struct RawPacket {
    uint8_t  data[RAW_PACKET_BUF];
    size_t   len;
    uint64_t recv_timestamp;  // Clock::now_ns() at receive time
};

// Q2: Parser → Book Engine
struct ParsedEvent {
    ParsedMessage msg;
    uint64_t wire_timestamp;  // timestamp from message header
    uint64_t recv_timestamp;  // carried from RawPacket
};

// What happened to the book
enum class EventType : uint8_t {
    OrderAdded,
    OrderCancelled,
    OrderExecuted,
    OrderReplaced,
    Trade
};

// Q3: Book Engine → Consumer
struct BookUpdate {
    Symbol    symbol;
    EventType event;
    Price     best_bid;
    Quantity  bid_qty;
    Price     best_ask;
    Quantity  ask_qty;
    uint64_t  latency_ns;  // recv_timestamp → now (end-to-end pipeline latency)
};

}  // namespace mdfh::core

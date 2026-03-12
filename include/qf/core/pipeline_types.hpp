#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include "qf/common/types.hpp"
#include "qf/core/price_level.hpp"
#include "qf/protocol/messages.hpp"

namespace qf::core {

// Q1: Network → Parser
struct RawPacket {
    uint8_t  data[1500];
    size_t   length;
    uint64_t receive_timestamp;
};

// Q2: Parser → Book Engine
struct ParsedEvent {
    ParsedMessage message;
    uint64_t receive_timestamp;
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
    Symbol                    symbol;
    std::optional<PriceLevel> best_bid;
    std::optional<PriceLevel> best_ask;
    uint64_t                  receive_timestamp;
    uint64_t                  book_timestamp;
};

}  // namespace qf::core

#pragma once

#include <cstdint>
#include <string>
#include "qf/common/types.hpp"

namespace qf::oms {

// Order type classification
enum class OrderType : uint8_t {
    Market = 0,
    Limit  = 1
};

// Order lifecycle states
enum class OrderState : uint8_t {
    New         = 0,
    Sent        = 1,
    Acked       = 2,
    PartialFill = 3,
    Filled      = 4,
    Cancelled   = 5,
    Rejected    = 6
};

// An order managed by the OMS through its full lifecycle.
struct OmsOrder {
    OrderId   order_id{0};
    Symbol    symbol;
    Side      side{Side::Buy};
    OrderType type{OrderType::Market};
    Price     price{0};          // limit price (0 for market orders)
    Quantity  quantity{0};       // original requested quantity
    Quantity  filled_quantity{0};
    OrderState state{OrderState::New};

    // Lifecycle timestamps (nanoseconds since epoch)
    Timestamp created{0};
    Timestamp sent{0};
    Timestamp acked{0};
    Timestamp last_fill{0};

    // Convenience helpers
    Quantity remaining() const { return quantity - filled_quantity; }
    bool is_terminal() const {
        return state == OrderState::Filled ||
               state == OrderState::Cancelled ||
               state == OrderState::Rejected;
    }
};

// Report of a single fill (partial or final) received from the venue.
struct FillReport {
    OrderId   order_id{0};
    Price     fill_price{0};
    Quantity  fill_quantity{0};
    Timestamp timestamp{0};
    bool      is_final{false};
};

// Records a state transition in the order lifecycle.
struct OrderEvent {
    OrderId    order_id{0};
    OrderState from_state{OrderState::New};
    OrderState to_state{OrderState::New};
    Timestamp  timestamp{0};
    std::string reason;
};

}  // namespace qf::oms

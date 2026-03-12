#pragma once

#include "qf/common/types.hpp"

namespace qf::core {

struct Order {
    OrderId   order_id;
    Symbol    symbol;
    Side      side;
    Price     price;
    Quantity  remaining_quantity;
    uint64_t  timestamp;

    Order()
        : order_id(0), side(Side::Buy), price(0),
          remaining_quantity(0), timestamp(0) {}

    Order(OrderId order_id, Symbol symbol, Side side, Price price,
          Quantity remaining_quantity, uint64_t ts)
        : order_id(order_id), symbol(symbol), side(side), price(price),
          remaining_quantity(remaining_quantity), timestamp(ts) {}
};

}  // namespace qf::core

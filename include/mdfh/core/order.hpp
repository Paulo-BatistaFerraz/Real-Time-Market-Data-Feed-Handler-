#pragma once

#include "mdfh/common/types.hpp"

namespace mdfh::core {

struct Order {
    OrderId   id;
    Side      side;
    Symbol    symbol;
    Price     price;
    Quantity  quantity;
    uint64_t  timestamp;

    Order() : id(0), side(Side::Buy), price(0), quantity(0), timestamp(0) {}

    Order(OrderId id, Side side, Symbol symbol, Price price, Quantity quantity, uint64_t ts)
        : id(id), side(side), symbol(symbol), price(price), quantity(quantity), timestamp(ts) {}
};

}  // namespace mdfh::core

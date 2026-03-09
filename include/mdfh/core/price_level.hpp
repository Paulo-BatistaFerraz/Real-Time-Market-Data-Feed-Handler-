#pragma once

#include "mdfh/common/types.hpp"

namespace mdfh::core {

struct PriceLevel {
    Price    price;
    Quantity total_qty;
    uint32_t order_count;

    PriceLevel() : price(0), total_qty(0), order_count(0) {}
    explicit PriceLevel(Price p) : price(p), total_qty(0), order_count(0) {}

    void add_quantity(Quantity qty) {
        total_qty += qty;
        ++order_count;
    }

    void remove_quantity(Quantity qty) {
        total_qty = (qty >= total_qty) ? 0 : total_qty - qty;
        if (order_count > 0) --order_count;
    }

    bool is_empty() const { return order_count == 0 || total_qty == 0; }
};

}  // namespace mdfh::core

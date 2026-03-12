#pragma once

#include "qf/common/types.hpp"

namespace qf::core {

struct PriceLevel {
    Price    price;
    Quantity total_quantity;
    uint32_t order_count;

    PriceLevel() : price(0), total_quantity(0), order_count(0) {}
    explicit PriceLevel(Price p) : price(p), total_quantity(0), order_count(0) {}

    void add(Quantity qty) {
        total_quantity += qty;
        ++order_count;
    }

    bool remove(Quantity qty) {
        total_quantity = (qty >= total_quantity) ? 0 : total_quantity - qty;
        if (order_count > 0) --order_count;
        return order_count == 0;
    }

    void modify(Quantity old_qty, Quantity new_qty) {
        total_quantity = (old_qty >= total_quantity) ? 0 : total_quantity - old_qty;
        total_quantity += new_qty;
    }

    bool is_empty() const { return order_count == 0; }
};

}  // namespace qf::core

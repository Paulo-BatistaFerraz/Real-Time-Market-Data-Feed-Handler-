#include "qf/oms/order_validator.hpp"

namespace qf::oms {

void OrderValidator::add_known_symbol(const Symbol& symbol) {
    known_symbols_.insert(symbol.as_key());
}

ValidationResult OrderValidator::validate(const strategy::Decision& decision) const {
    // 1. Quantity must be > 0
    if (decision.quantity == 0) {
        return {false, "Quantity must be greater than zero"};
    }

    // 2. Limit orders must have price > 0
    if (decision.order_type == strategy::OrderType::Limit && decision.limit_price == 0) {
        return {false, "Limit order must have price greater than zero"};
    }

    // 3. Symbol must be known/registered
    if (known_symbols_.find(decision.symbol.as_key()) == known_symbols_.end()) {
        return {false, "Unknown symbol"};
    }

    return {true, {}};
}

}  // namespace qf::oms

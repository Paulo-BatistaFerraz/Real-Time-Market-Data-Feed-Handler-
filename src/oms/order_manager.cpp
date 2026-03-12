#include "qf/oms/order_manager.hpp"
#include "qf/common/clock.hpp"

namespace qf::oms {

OrderManager::OrderManager() {
    // Wire StateTransitions callback to journal all events automatically.
    transitions_.set_event_callback([this](const OrderEvent& event) {
        journal_.append(event);
    });
}

OMResult OrderManager::submit(const strategy::Decision& decision) {
    OMResult result;

    // Validate basic fields
    if (decision.quantity == 0) {
        result.error = "Decision has zero quantity";
        return result;
    }

    // Assign a unique order ID
    OrderId id = next_id_.fetch_add(1);

    // Build the OmsOrder in New state
    OmsOrder order;
    order.order_id = id;
    order.symbol   = decision.symbol;
    order.side     = decision.side;
    order.type     = static_cast<OrderType>(static_cast<uint8_t>(decision.order_type));
    order.price    = decision.limit_price;
    order.quantity = decision.quantity;
    order.state    = OrderState::New;
    order.created  = Clock::now_ns();

    // Insert into the store
    orders_[id] = order;

    // Transition New -> Sent
    auto tr = apply_transition(orders_[id], OrderState::Sent, "submit");
    if (!tr.success) {
        orders_.erase(id);
        return tr;
    }

    orders_[id].sent = Clock::now_ns();

    result.success  = true;
    result.order_id = id;
    return result;
}

OMResult OrderManager::on_ack(OrderId order_id) {
    auto* order = find_order(order_id);
    if (!order) {
        return {false, order_id, "Order not found"};
    }

    auto result = apply_transition(*order, OrderState::Acked, "ack");
    if (result.success) {
        order->acked = Clock::now_ns();
    }
    return result;
}

OMResult OrderManager::on_fill(const FillReport& report) {
    auto* order = find_order(report.order_id);
    if (!order) {
        return {false, report.order_id, "Order not found"};
    }

    // Update filled quantity
    order->filled_quantity += report.fill_quantity;
    order->last_fill = report.timestamp != 0 ? report.timestamp : Clock::now_ns();

    // Determine target state
    OrderState target = (order->filled_quantity >= order->quantity)
                            ? OrderState::Filled
                            : OrderState::PartialFill;

    return apply_transition(*order, target, "fill");
}

OMResult OrderManager::cancel(OrderId order_id) {
    auto* order = find_order(order_id);
    if (!order) {
        return {false, order_id, "Order not found"};
    }

    return apply_transition(*order, OrderState::Cancelled, "cancel");
}

OMResult OrderManager::on_reject(OrderId order_id, const std::string& reason) {
    auto* order = find_order(order_id);
    if (!order) {
        return {false, order_id, "Order not found"};
    }

    return apply_transition(*order, OrderState::Rejected, reason);
}

std::optional<OmsOrder> OrderManager::get_order(OrderId order_id) const {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return std::nullopt;
    return it->second;
}

OmsOrder* OrderManager::find_order(OrderId id) {
    auto it = orders_.find(id);
    return (it != orders_.end()) ? &it->second : nullptr;
}

OMResult OrderManager::apply_transition(OmsOrder& order, OrderState target,
                                        const std::string& reason) {
    auto tr = transitions_.transition(order, target, reason);
    OMResult result;
    result.order_id = order.order_id;
    if (tr.success) {
        result.success = true;
    } else {
        result.error = tr.error;
    }
    return result;
}

}  // namespace qf::oms

#pragma once

#include "qf/oms/oms_types.hpp"
#include "qf/oms/state_machine/state_transitions.hpp"
#include "qf/oms/state_machine/order_journal.hpp"
#include "qf/strategy/strategy_types.hpp"

#include <atomic>
#include <optional>
#include <unordered_map>

namespace qf::oms {

// Result of an OrderManager operation.
struct OMResult {
    bool        success{false};
    OrderId     order_id{0};
    std::string error;
};

// Centralized order lifecycle manager.
// All order creation, fills, cancels, and rejects flow through this class.
// Every state change is validated by StateTransitions and recorded in the OrderJournal.
class OrderManager {
public:
    OrderManager();

    // Submit a new order from a strategy Decision.
    // Creates an OmsOrder in New state, assigns an ID, validates, and transitions to Sent.
    OMResult submit(const strategy::Decision& decision);

    // Venue acknowledged the order.
    OMResult on_ack(OrderId order_id);

    // Venue reported a fill (partial or final).
    OMResult on_fill(const FillReport& report);

    // Request cancellation of an active order.
    OMResult cancel(OrderId order_id);

    // Venue rejected the order.
    OMResult on_reject(OrderId order_id, const std::string& reason);

    // Read-only access to an order. Returns nullopt if not found.
    std::optional<OmsOrder> get_order(OrderId order_id) const;

    // Number of orders currently tracked.
    size_t order_count() const { return orders_.size(); }

    // Read-only access to the journal.
    const OrderJournal& journal() const { return journal_; }

private:
    std::unordered_map<OrderId, OmsOrder> orders_;
    StateTransitions transitions_;
    OrderJournal journal_;
    std::atomic<OrderId> next_id_{1};

    // Lookup helper — returns nullptr if order not found.
    OmsOrder* find_order(OrderId id);

    // Apply a transition, journal the event, and build the result.
    OMResult apply_transition(OmsOrder& order, OrderState target, const std::string& reason);
};

}  // namespace qf::oms

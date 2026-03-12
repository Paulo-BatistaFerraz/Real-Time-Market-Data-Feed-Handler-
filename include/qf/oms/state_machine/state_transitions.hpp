#pragma once

#include "qf/oms/oms_types.hpp"
#include "qf/oms/state_machine/order_states.hpp"
#include <array>
#include <functional>
#include <stdexcept>
#include <string>

namespace qf::oms {

// Result of a state transition attempt.
struct TransitionResult {
    bool        success{false};
    OrderEvent  event;
    std::string error;
};

namespace detail {

constexpr size_t NUM_ORDER_STATES = 7;

constexpr std::array<std::array<bool, NUM_ORDER_STATES>, NUM_ORDER_STATES> build_transition_table() {
    std::array<std::array<bool, NUM_ORDER_STATES>, NUM_ORDER_STATES> t{};
    // New(0) -> Sent(1)
    t[0][1] = true;
    // Sent(1) -> Acked(2), Rejected(6)
    t[1][2] = true;
    t[1][6] = true;
    // Acked(2) -> PartialFill(3), Filled(4), Cancelled(5)
    t[2][3] = true;
    t[2][4] = true;
    t[2][5] = true;
    // PartialFill(3) -> PartialFill(3), Filled(4), Cancelled(5)
    t[3][3] = true;
    t[3][4] = true;
    t[3][5] = true;
    return t;
}

constexpr auto TRANSITION_TABLE = build_transition_table();

}  // namespace detail

// Validated state machine for order lifecycle transitions.
// Guards against invalid state changes and generates OrderEvents.
class StateTransitions {
public:
    using EventCallback = std::function<void(const OrderEvent&)>;

    // Set a callback invoked on every successful transition.
    void set_event_callback(EventCallback cb) { callback_ = std::move(cb); }

    // Returns true only for the legal transitions defined by the OMS spec.
    static bool is_valid(OrderState from, OrderState to);

    // Apply a state transition to an order.
    // On success: updates order.state, returns TransitionResult with success=true and the OrderEvent.
    // On failure: returns TransitionResult with success=false and an error message.
    TransitionResult transition(OmsOrder& order, OrderState new_state, const std::string& reason);

    // Throwing variant — throws std::runtime_error on invalid transition.
    OrderEvent transition_or_throw(OmsOrder& order, OrderState new_state, const std::string& reason);

private:
    EventCallback callback_;
};

}  // namespace qf::oms

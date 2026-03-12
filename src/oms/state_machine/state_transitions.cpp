#include "qf/oms/state_machine/state_transitions.hpp"
#include "qf/common/clock.hpp"

namespace qf::oms {

bool StateTransitions::is_valid(OrderState from, OrderState to) {
    auto f = static_cast<size_t>(from);
    auto t = static_cast<size_t>(to);
    if (f >= detail::NUM_ORDER_STATES || t >= detail::NUM_ORDER_STATES) return false;
    return detail::TRANSITION_TABLE[f][t];
}

TransitionResult StateTransitions::transition(OmsOrder& order, OrderState new_state,
                                              const std::string& reason) {
    TransitionResult result;
    OrderState old_state = order.state;

    if (!is_valid(old_state, new_state)) {
        result.success = false;
        result.error = std::string("Invalid transition: ") +
                       state_to_string(old_state) + " -> " +
                       state_to_string(new_state);
        return result;
    }

    // Apply the transition
    order.state = new_state;

    // Build the event
    OrderEvent event;
    event.order_id   = order.order_id;
    event.from_state = old_state;
    event.to_state   = new_state;
    event.timestamp  = Clock::now_ns();
    event.reason     = reason;

    // Notify callback
    if (callback_) {
        callback_(event);
    }

    result.success = true;
    result.event   = event;
    return result;
}

OrderEvent StateTransitions::transition_or_throw(OmsOrder& order, OrderState new_state,
                                                 const std::string& reason) {
    auto result = transition(order, new_state, reason);
    if (!result.success) {
        throw std::runtime_error(result.error);
    }
    return result.event;
}

}  // namespace qf::oms

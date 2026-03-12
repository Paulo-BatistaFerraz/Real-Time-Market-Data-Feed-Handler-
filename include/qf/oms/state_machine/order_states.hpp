#pragma once

#include "qf/oms/oms_types.hpp"
#include <array>

namespace qf::oms {

// Compile-time validation of legal state transitions.
// Returns true if moving from `from` to `to` is a valid transition.
inline bool is_valid_transition(OrderState from, OrderState to) {
    // Transition table: rows = from-state, columns = to-state
    // New -> Sent, Cancelled, Rejected
    // Sent -> Acked, Cancelled, Rejected
    // Acked -> PartialFill, Filled, Cancelled
    // PartialFill -> PartialFill, Filled, Cancelled
    // Filled -> (terminal)
    // Cancelled -> (terminal)
    // Rejected -> (terminal)

    constexpr size_t N = 7;
    // table[from][to] = allowed
    constexpr std::array<std::array<bool, N>, N> table = {{
        //              New    Sent   Acked  PFill  Filled Cancel Reject
        /* New     */ {{false, true,  false, false, false, true,  true }},
        /* Sent    */ {{false, false, true,  false, false, true,  true }},
        /* Acked   */ {{false, false, false, true,  true,  true,  false}},
        /* PFill   */ {{false, false, false, true,  true,  true,  false}},
        /* Filled  */ {{false, false, false, false, false, false, false}},
        /* Cancel  */ {{false, false, false, false, false, false, false}},
        /* Reject  */ {{false, false, false, false, false, false, false}},
    }};

    auto f = static_cast<size_t>(from);
    auto t = static_cast<size_t>(to);
    if (f >= N || t >= N) return false;
    return table[f][t];
}

// Convert OrderState to a human-readable string.
inline const char* state_to_string(OrderState s) {
    switch (s) {
        case OrderState::New:         return "New";
        case OrderState::Sent:        return "Sent";
        case OrderState::Acked:       return "Acked";
        case OrderState::PartialFill: return "PartialFill";
        case OrderState::Filled:      return "Filled";
        case OrderState::Cancelled:   return "Cancelled";
        case OrderState::Rejected:    return "Rejected";
    }
    return "Unknown";
}

}  // namespace qf::oms

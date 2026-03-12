#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "qf/common/types.hpp"
#include "qf/core/event_bus.hpp"

namespace qf::risk {

// Event fired on EventBus when the circuit breaker trips.
struct CircuitBreakerTripped {
    Timestamp   timestamp;
    std::string reason;
    double      drawdown;          // Current drawdown at time of trip
    double      drawdown_limit;    // Configured limit that was breached
    bool        manual;            // True if triggered by manual kill switch
};

// CircuitBreaker — emergency kill switch for all trading.
//
// Triggers when portfolio drawdown exceeds a configurable threshold (default 2%)
// or when manually tripped via trip(). On trigger it:
//   1. Cancels all open orders (via registered cancel callback)
//   2. Disables all strategies (via registered disable callback)
//   3. Fires a CircuitBreakerTripped event on the EventBus
//
// Cannot be auto-reset — requires an explicit call to reset().
class CircuitBreaker {
public:
    // Callback to cancel all open orders.
    using CancelAllCallback  = std::function<void()>;
    // Callback to disable all strategies.
    using DisableAllCallback = std::function<void()>;
    // Callback for alert notifications.
    using AlertCallback      = std::function<void(const std::string&)>;

    // Construct with drawdown threshold (fraction, e.g. 0.02 = 2%).
    explicit CircuitBreaker(double drawdown_threshold = 0.02,
                            core::EventBus* event_bus = nullptr);

    // Set the callback invoked to cancel all open orders on trip.
    void set_cancel_callback(CancelAllCallback cb);

    // Set the callback invoked to disable all strategies on trip.
    void set_disable_callback(DisableAllCallback cb);

    // Set an optional alert callback fired on trip.
    void set_alert_callback(AlertCallback cb);

    // Update current drawdown. If drawdown exceeds threshold and breaker
    // is not already tripped, triggers the breaker automatically.
    // drawdown is expressed as a fraction (e.g. 0.025 = 2.5% drawdown).
    void update_drawdown(double drawdown);

    // Manual kill switch — trips the breaker immediately regardless of drawdown.
    void trip(const std::string& reason = "manual kill switch");

    // Re-enable trading after a trip. Requires explicit human call.
    void reset();

    // Returns true if the circuit breaker is currently tripped.
    bool is_tripped() const;

    // Accessors
    double drawdown_threshold() const { return drawdown_threshold_; }
    double current_drawdown() const { return current_drawdown_; }

private:
    double drawdown_threshold_;
    double current_drawdown_{0.0};
    std::atomic<bool> tripped_{false};

    core::EventBus* event_bus_;

    CancelAllCallback  cancel_callback_;
    DisableAllCallback disable_callback_;
    AlertCallback      alert_callback_;

    // Internal: execute all trip actions.
    void execute_trip(const std::string& reason, bool manual);
};

}  // namespace qf::risk

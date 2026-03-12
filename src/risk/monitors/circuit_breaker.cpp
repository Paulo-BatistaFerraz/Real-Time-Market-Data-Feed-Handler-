#include "qf/risk/monitors/circuit_breaker.hpp"
#include "qf/common/logging.hpp"

#include <sstream>

namespace qf::risk {

CircuitBreaker::CircuitBreaker(double drawdown_threshold, core::EventBus* event_bus)
    : drawdown_threshold_(drawdown_threshold)
    , event_bus_(event_bus) {}

void CircuitBreaker::set_cancel_callback(CancelAllCallback cb) {
    cancel_callback_ = std::move(cb);
}

void CircuitBreaker::set_disable_callback(DisableAllCallback cb) {
    disable_callback_ = std::move(cb);
}

void CircuitBreaker::set_alert_callback(AlertCallback cb) {
    alert_callback_ = std::move(cb);
}

void CircuitBreaker::update_drawdown(double drawdown) {
    current_drawdown_ = drawdown;

    if (!tripped_.load(std::memory_order_acquire) && drawdown > drawdown_threshold_) {
        std::ostringstream msg;
        msg << "drawdown " << (drawdown * 100.0)
            << "% exceeds limit " << (drawdown_threshold_ * 100.0) << "%";
        execute_trip(msg.str(), false);
    }
}

void CircuitBreaker::trip(const std::string& reason) {
    if (tripped_.load(std::memory_order_acquire)) {
        QF_LOG_WARN_T("RISK", "circuit breaker already tripped, ignoring trip(\"{}\")", reason);
        return;
    }
    execute_trip(reason, true);
}

void CircuitBreaker::reset() {
    if (!tripped_.load(std::memory_order_acquire)) {
        QF_LOG_WARN_T("RISK", "circuit breaker reset called but not tripped");
        return;
    }
    tripped_.store(false, std::memory_order_release);
    QF_LOG_INFO_T("RISK", "circuit breaker RESET — trading re-enabled");
}

bool CircuitBreaker::is_tripped() const {
    return tripped_.load(std::memory_order_acquire);
}

void CircuitBreaker::execute_trip(const std::string& reason, bool manual) {
    tripped_.store(true, std::memory_order_release);

    QF_LOG_ERROR_T("RISK", "CIRCUIT BREAKER TRIPPED: {}", reason);

    // 1. Cancel all open orders.
    if (cancel_callback_) {
        cancel_callback_();
    }

    // 2. Disable all strategies.
    if (disable_callback_) {
        disable_callback_();
    }

    // 3. Fire alert.
    if (alert_callback_) {
        alert_callback_(reason);
    }

    // 4. Publish event on EventBus.
    if (event_bus_) {
        CircuitBreakerTripped event{};
        event.reason = reason;
        event.drawdown = current_drawdown_;
        event.drawdown_limit = drawdown_threshold_;
        event.manual = manual;
        event_bus_->publish(event);
    }
}

}  // namespace qf::risk

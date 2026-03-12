#include <gtest/gtest.h>
#include "qf/risk/monitors/circuit_breaker.hpp"
#include "qf/core/event_bus.hpp"

using namespace qf::risk;

// --- Basic state tests ---

TEST(CircuitBreakerTest, InitiallyNotTripped) {
    CircuitBreaker cb;
    EXPECT_FALSE(cb.is_tripped());
}

TEST(CircuitBreakerTest, DefaultDrawdownThreshold) {
    CircuitBreaker cb;
    EXPECT_DOUBLE_EQ(cb.drawdown_threshold(), 0.02);
}

TEST(CircuitBreakerTest, CustomThreshold) {
    CircuitBreaker cb(0.05);
    EXPECT_DOUBLE_EQ(cb.drawdown_threshold(), 0.05);
}

// --- Manual trip/reset ---

TEST(CircuitBreakerTest, ManualTrip) {
    CircuitBreaker cb;
    cb.trip("emergency");
    EXPECT_TRUE(cb.is_tripped());
}

TEST(CircuitBreakerTest, ManualReset) {
    CircuitBreaker cb;
    cb.trip("emergency");
    EXPECT_TRUE(cb.is_tripped());
    cb.reset();
    EXPECT_FALSE(cb.is_tripped());
}

TEST(CircuitBreakerTest, TripWithoutResetStaysTripped) {
    CircuitBreaker cb;
    cb.trip();
    // Calling update_drawdown with a safe value should NOT reset.
    cb.update_drawdown(0.0);
    EXPECT_TRUE(cb.is_tripped());
}

TEST(CircuitBreakerTest, CannotAutoReset) {
    CircuitBreaker cb(0.02);
    cb.update_drawdown(0.03);  // triggers
    EXPECT_TRUE(cb.is_tripped());

    // Drawdown going back below threshold does NOT reset.
    cb.update_drawdown(0.01);
    EXPECT_TRUE(cb.is_tripped());

    // Only explicit reset re-enables.
    cb.reset();
    EXPECT_FALSE(cb.is_tripped());
}

// --- Drawdown-triggered trip ---

TEST(CircuitBreakerTest, DrawdownBelowThresholdNoTrip) {
    CircuitBreaker cb(0.02);
    cb.update_drawdown(0.01);
    EXPECT_FALSE(cb.is_tripped());
}

TEST(CircuitBreakerTest, DrawdownAtThresholdNoTrip) {
    CircuitBreaker cb(0.02);
    cb.update_drawdown(0.02);
    EXPECT_FALSE(cb.is_tripped());
}

TEST(CircuitBreakerTest, DrawdownAboveThresholdTrips) {
    CircuitBreaker cb(0.02);
    cb.update_drawdown(0.025);
    EXPECT_TRUE(cb.is_tripped());
}

TEST(CircuitBreakerTest, DrawdownTracked) {
    CircuitBreaker cb(0.02);
    cb.update_drawdown(0.015);
    EXPECT_DOUBLE_EQ(cb.current_drawdown(), 0.015);
}

// --- Callback invocation ---

TEST(CircuitBreakerTest, CancelCallbackFired) {
    CircuitBreaker cb;
    bool cancel_called = false;
    cb.set_cancel_callback([&]() { cancel_called = true; });
    cb.trip("test");
    EXPECT_TRUE(cancel_called);
}

TEST(CircuitBreakerTest, DisableCallbackFired) {
    CircuitBreaker cb;
    bool disable_called = false;
    cb.set_disable_callback([&]() { disable_called = true; });
    cb.trip("test");
    EXPECT_TRUE(disable_called);
}

TEST(CircuitBreakerTest, AlertCallbackFired) {
    CircuitBreaker cb;
    std::string alert_msg;
    cb.set_alert_callback([&](const std::string& msg) { alert_msg = msg; });
    cb.trip("critical failure");
    EXPECT_EQ(alert_msg, "critical failure");
}

TEST(CircuitBreakerTest, AllCallbacksFiredOnDrawdownTrip) {
    CircuitBreaker cb(0.02);
    bool cancel_called = false;
    bool disable_called = false;
    std::string alert_msg;

    cb.set_cancel_callback([&]() { cancel_called = true; });
    cb.set_disable_callback([&]() { disable_called = true; });
    cb.set_alert_callback([&](const std::string& msg) { alert_msg = msg; });

    cb.update_drawdown(0.03);

    EXPECT_TRUE(cancel_called);
    EXPECT_TRUE(disable_called);
    EXPECT_FALSE(alert_msg.empty());
}

TEST(CircuitBreakerTest, CallbacksNotFiredOnSecondTrip) {
    CircuitBreaker cb;
    int cancel_count = 0;
    cb.set_cancel_callback([&]() { cancel_count++; });

    cb.trip("first");
    EXPECT_EQ(cancel_count, 1);

    // Second trip while already tripped should be ignored.
    cb.trip("second");
    EXPECT_EQ(cancel_count, 1);
}

TEST(CircuitBreakerTest, DrawdownTripDoesNotRetrigger) {
    CircuitBreaker cb(0.02);
    int cancel_count = 0;
    cb.set_cancel_callback([&]() { cancel_count++; });

    cb.update_drawdown(0.03);
    EXPECT_EQ(cancel_count, 1);

    // Further high drawdowns should not re-trigger.
    cb.update_drawdown(0.05);
    EXPECT_EQ(cancel_count, 1);
}

// --- EventBus integration ---

TEST(CircuitBreakerTest, EventBusPublishOnTrip) {
    qf::core::EventBus bus;
    CircuitBreaker cb(0.02, &bus);

    CircuitBreakerTripped received{};
    bool event_fired = false;
    bus.subscribe<CircuitBreakerTripped>(
        [&](const CircuitBreakerTripped& e) {
            received = e;
            event_fired = true;
        });

    cb.trip("manual kill");

    EXPECT_TRUE(event_fired);
    EXPECT_EQ(received.reason, "manual kill");
    EXPECT_TRUE(received.manual);
    EXPECT_DOUBLE_EQ(received.drawdown_limit, 0.02);
}

TEST(CircuitBreakerTest, EventBusPublishOnDrawdownTrip) {
    qf::core::EventBus bus;
    CircuitBreaker cb(0.02, &bus);

    CircuitBreakerTripped received{};
    bool event_fired = false;
    bus.subscribe<CircuitBreakerTripped>(
        [&](const CircuitBreakerTripped& e) {
            received = e;
            event_fired = true;
        });

    cb.update_drawdown(0.025);

    EXPECT_TRUE(event_fired);
    EXPECT_FALSE(received.manual);
    EXPECT_DOUBLE_EQ(received.drawdown, 0.025);
    EXPECT_DOUBLE_EQ(received.drawdown_limit, 0.02);
}

TEST(CircuitBreakerTest, NoEventBusOK) {
    // Should not crash when event_bus is nullptr.
    CircuitBreaker cb(0.02, nullptr);
    cb.trip("no bus");
    EXPECT_TRUE(cb.is_tripped());
}

// --- Reset + retrip cycle ---

TEST(CircuitBreakerTest, ResetAndRetripWorks) {
    CircuitBreaker cb(0.02);
    int cancel_count = 0;
    cb.set_cancel_callback([&]() { cancel_count++; });

    cb.trip("first");
    EXPECT_TRUE(cb.is_tripped());
    EXPECT_EQ(cancel_count, 1);

    cb.reset();
    EXPECT_FALSE(cb.is_tripped());

    cb.trip("second");
    EXPECT_TRUE(cb.is_tripped());
    EXPECT_EQ(cancel_count, 2);
}

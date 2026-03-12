#include <gtest/gtest.h>
#include "qf/oms/state_machine/state_transitions.hpp"
#include "qf/oms/state_machine/order_states.hpp"

using namespace qf::oms;

// --- Valid transition table tests ---

TEST(StateMachine, NewToSentIsValid) {
    EXPECT_TRUE(StateTransitions::is_valid(OrderState::New, OrderState::Sent));
}

TEST(StateMachine, SentToAckedIsValid) {
    EXPECT_TRUE(StateTransitions::is_valid(OrderState::Sent, OrderState::Acked));
}

TEST(StateMachine, AckedToPartialFillIsValid) {
    EXPECT_TRUE(StateTransitions::is_valid(OrderState::Acked, OrderState::PartialFill));
}

TEST(StateMachine, AckedToFilledIsValid) {
    EXPECT_TRUE(StateTransitions::is_valid(OrderState::Acked, OrderState::Filled));
}

TEST(StateMachine, PartialFillToFilledIsValid) {
    EXPECT_TRUE(StateTransitions::is_valid(OrderState::PartialFill, OrderState::Filled));
}

TEST(StateMachine, PartialFillToPartialFillIsValid) {
    EXPECT_TRUE(StateTransitions::is_valid(OrderState::PartialFill, OrderState::PartialFill));
}

TEST(StateMachine, AckedToCancelledIsValid) {
    EXPECT_TRUE(StateTransitions::is_valid(OrderState::Acked, OrderState::Cancelled));
}

TEST(StateMachine, SentToRejectedIsValid) {
    EXPECT_TRUE(StateTransitions::is_valid(OrderState::Sent, OrderState::Rejected));
}

// --- Invalid transitions ---

TEST(StateMachine, NewToFilledIsInvalid) {
    EXPECT_FALSE(StateTransitions::is_valid(OrderState::New, OrderState::Filled));
}

TEST(StateMachine, NewToAckedIsInvalid) {
    EXPECT_FALSE(StateTransitions::is_valid(OrderState::New, OrderState::Acked));
}

TEST(StateMachine, SentToFilledIsInvalid) {
    EXPECT_FALSE(StateTransitions::is_valid(OrderState::Sent, OrderState::Filled));
}

TEST(StateMachine, FilledIsTerminal) {
    EXPECT_FALSE(StateTransitions::is_valid(OrderState::Filled, OrderState::New));
    EXPECT_FALSE(StateTransitions::is_valid(OrderState::Filled, OrderState::Sent));
    EXPECT_FALSE(StateTransitions::is_valid(OrderState::Filled, OrderState::Acked));
    EXPECT_FALSE(StateTransitions::is_valid(OrderState::Filled, OrderState::Cancelled));
}

TEST(StateMachine, CancelledIsTerminal) {
    EXPECT_FALSE(StateTransitions::is_valid(OrderState::Cancelled, OrderState::Filled));
    EXPECT_FALSE(StateTransitions::is_valid(OrderState::Cancelled, OrderState::New));
}

TEST(StateMachine, RejectedIsTerminal) {
    EXPECT_FALSE(StateTransitions::is_valid(OrderState::Rejected, OrderState::Sent));
    EXPECT_FALSE(StateTransitions::is_valid(OrderState::Rejected, OrderState::New));
}

// --- Transition execution tests ---

TEST(StateMachine, TransitionUpdatesState) {
    StateTransitions sm;
    OmsOrder order;
    order.order_id = 1;
    order.state = OrderState::New;

    auto result = sm.transition(order, OrderState::Sent, "submit");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(order.state, OrderState::Sent);
    EXPECT_EQ(result.event.from_state, OrderState::New);
    EXPECT_EQ(result.event.to_state, OrderState::Sent);
    EXPECT_EQ(result.event.order_id, 1u);
}

TEST(StateMachine, InvalidTransitionRejectedWithError) {
    StateTransitions sm;
    OmsOrder order;
    order.order_id = 42;
    order.state = OrderState::New;

    auto result = sm.transition(order, OrderState::Filled, "bad");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(order.state, OrderState::New);  // unchanged
    EXPECT_FALSE(result.error.empty());
}

TEST(StateMachine, TransitionOrThrowOnInvalid) {
    StateTransitions sm;
    OmsOrder order;
    order.state = OrderState::New;

    EXPECT_THROW(sm.transition_or_throw(order, OrderState::Filled, "bad"), std::runtime_error);
    EXPECT_EQ(order.state, OrderState::New);  // unchanged
}

TEST(StateMachine, CallbackFiredOnSuccess) {
    StateTransitions sm;
    OrderEvent captured;
    sm.set_event_callback([&](const OrderEvent& e) { captured = e; });

    OmsOrder order;
    order.order_id = 7;
    order.state = OrderState::New;

    sm.transition(order, OrderState::Sent, "test");
    EXPECT_EQ(captured.order_id, 7u);
    EXPECT_EQ(captured.from_state, OrderState::New);
    EXPECT_EQ(captured.to_state, OrderState::Sent);
}

TEST(StateMachine, CallbackNotFiredOnFailure) {
    StateTransitions sm;
    bool called = false;
    sm.set_event_callback([&](const OrderEvent&) { called = true; });

    OmsOrder order;
    order.state = OrderState::New;

    sm.transition(order, OrderState::Filled, "bad");
    EXPECT_FALSE(called);
}

// --- state_to_string ---

TEST(StateMachine, StateToStringCoversAll) {
    EXPECT_STREQ(state_to_string(OrderState::New), "New");
    EXPECT_STREQ(state_to_string(OrderState::Sent), "Sent");
    EXPECT_STREQ(state_to_string(OrderState::Acked), "Acked");
    EXPECT_STREQ(state_to_string(OrderState::PartialFill), "PartialFill");
    EXPECT_STREQ(state_to_string(OrderState::Filled), "Filled");
    EXPECT_STREQ(state_to_string(OrderState::Cancelled), "Cancelled");
    EXPECT_STREQ(state_to_string(OrderState::Rejected), "Rejected");
}

/**
 * @file test_state_machine.cpp
 * @brief Unit tests for SeaDrop state machines
 */

#include <gtest/gtest.h>
#include <seadrop/state_machine.h>

using namespace seadrop;

// ============================================================================
// Transfer State Machine Tests
// ============================================================================

TEST(TransferStateMachineTest, InitialState) {
  TransferStateMachine sm;
  EXPECT_EQ(sm.current(), TransferState::Pending);
}

TEST(TransferStateMachineTest, CustomInitialState) {
  TransferStateMachine sm(TransferState::InProgress);
  EXPECT_EQ(sm.current(), TransferState::InProgress);
}

TEST(TransferStateMachineTest, ValidTransitionPendingToAwaitingAccept) {
  TransferStateMachine sm;

  auto result = sm.transition(TransferState::AwaitingAccept);
  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(sm.current(), TransferState::AwaitingAccept);
}

TEST(TransferStateMachineTest, ValidTransitionFullFlow) {
  TransferStateMachine sm;

  EXPECT_TRUE(sm.transition(TransferState::AwaitingAccept).is_ok());
  EXPECT_TRUE(sm.transition(TransferState::Preparing).is_ok());
  EXPECT_TRUE(sm.transition(TransferState::InProgress).is_ok());
  EXPECT_TRUE(sm.transition(TransferState::Completed).is_ok());

  EXPECT_EQ(sm.current(), TransferState::Completed);
  EXPECT_TRUE(sm.is_terminal());
}

TEST(TransferStateMachineTest, InvalidTransitionPendingToCompleted) {
  TransferStateMachine sm;

  auto result = sm.transition(TransferState::Completed);
  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(result.error().code(), ErrorCode::InvalidState);
  EXPECT_EQ(sm.current(), TransferState::Pending);
}

TEST(TransferStateMachineTest, InvalidTransitionSkipAwaitingAccept) {
  TransferStateMachine sm;

  // Can't go directly from Pending to Preparing
  auto result = sm.transition(TransferState::Preparing);
  EXPECT_TRUE(result.is_error());
}

TEST(TransferStateMachineTest, CanTransition) {
  TransferStateMachine sm;

  EXPECT_TRUE(sm.can_transition(TransferState::AwaitingAccept));
  EXPECT_TRUE(sm.can_transition(TransferState::Cancelled));
  EXPECT_TRUE(sm.can_transition(TransferState::Failed));

  EXPECT_FALSE(sm.can_transition(TransferState::Preparing));
  EXPECT_FALSE(sm.can_transition(TransferState::InProgress));
  EXPECT_FALSE(sm.can_transition(TransferState::Completed));
}

TEST(TransferStateMachineTest, ValidTransitions) {
  TransferStateMachine sm;
  auto valid = sm.valid_transitions();

  EXPECT_EQ(valid.size(), 3u);
  EXPECT_TRUE(valid.count(TransferState::AwaitingAccept));
  EXPECT_TRUE(valid.count(TransferState::Cancelled));
  EXPECT_TRUE(valid.count(TransferState::Failed));
}

TEST(TransferStateMachineTest, TerminalStateNoTransitions) {
  TransferStateMachine sm;

  sm.transition(TransferState::AwaitingAccept);
  sm.transition(TransferState::Rejected);

  EXPECT_TRUE(sm.is_terminal());
  EXPECT_TRUE(sm.valid_transitions().empty());

  // Can't transition out of terminal state
  auto result = sm.transition(TransferState::Pending);
  EXPECT_TRUE(result.is_error());
}

TEST(TransferStateMachineTest, PauseResume) {
  TransferStateMachine sm;

  sm.transition(TransferState::AwaitingAccept);
  sm.transition(TransferState::Preparing);
  sm.transition(TransferState::InProgress);

  // Pause
  EXPECT_TRUE(sm.transition(TransferState::Paused).is_ok());
  EXPECT_FALSE(sm.is_terminal());
  EXPECT_TRUE(sm.is_active());

  // Resume
  EXPECT_TRUE(sm.transition(TransferState::InProgress).is_ok());
  EXPECT_TRUE(sm.is_active());
}

TEST(TransferStateMachineTest, CancelFromAnyNonTerminal) {
  // From Pending
  TransferStateMachine sm1;
  EXPECT_TRUE(sm1.transition(TransferState::Cancelled).is_ok());

  // From AwaitingAccept
  TransferStateMachine sm2;
  sm2.transition(TransferState::AwaitingAccept);
  EXPECT_TRUE(sm2.transition(TransferState::Cancelled).is_ok());

  // From InProgress
  TransferStateMachine sm3;
  sm3.transition(TransferState::AwaitingAccept);
  sm3.transition(TransferState::Preparing);
  sm3.transition(TransferState::InProgress);
  EXPECT_TRUE(sm3.transition(TransferState::Cancelled).is_ok());

  // From Paused
  TransferStateMachine sm4;
  sm4.transition(TransferState::AwaitingAccept);
  sm4.transition(TransferState::Preparing);
  sm4.transition(TransferState::InProgress);
  sm4.transition(TransferState::Paused);
  EXPECT_TRUE(sm4.transition(TransferState::Cancelled).is_ok());
}

TEST(TransferStateMachineTest, StateChangedCallback) {
  TransferStateMachine sm;

  TransferState recorded_from = TransferState::Pending;
  TransferState recorded_to = TransferState::Pending;
  int callback_count = 0;

  sm.on_state_changed([&](TransferState from, TransferState to) {
    recorded_from = from;
    recorded_to = to;
    callback_count++;
  });

  sm.transition(TransferState::AwaitingAccept);

  EXPECT_EQ(recorded_from, TransferState::Pending);
  EXPECT_EQ(recorded_to, TransferState::AwaitingAccept);
  EXPECT_EQ(callback_count, 1);

  sm.transition(TransferState::Preparing);
  EXPECT_EQ(callback_count, 2);
}

TEST(TransferStateMachineTest, TerminalCallback) {
  TransferStateMachine sm;

  TransferState terminal_state = TransferState::Pending;
  bool terminal_called = false;

  sm.on_terminal([&](TransferState state) {
    terminal_state = state;
    terminal_called = true;
  });

  // Non-terminal transitions don't trigger
  sm.transition(TransferState::AwaitingAccept);
  EXPECT_FALSE(terminal_called);

  // Terminal transition triggers
  sm.transition(TransferState::Rejected);
  EXPECT_TRUE(terminal_called);
  EXPECT_EQ(terminal_state, TransferState::Rejected);
}

TEST(TransferStateMachineTest, ForceTransition) {
  TransferStateMachine sm;

  // Force an invalid transition
  sm.force_transition(TransferState::Completed);
  EXPECT_EQ(sm.current(), TransferState::Completed);
}

TEST(TransferStateMachineTest, Reset) {
  TransferStateMachine sm;

  sm.transition(TransferState::AwaitingAccept);
  sm.transition(TransferState::Preparing);

  sm.reset();
  EXPECT_EQ(sm.current(), TransferState::Pending);
}

TEST(TransferStateMachineTest, IsActive) {
  TransferStateMachine sm;

  EXPECT_FALSE(sm.is_active());

  sm.transition(TransferState::AwaitingAccept);
  EXPECT_FALSE(sm.is_active());

  sm.transition(TransferState::Preparing);
  EXPECT_FALSE(sm.is_active());

  sm.transition(TransferState::InProgress);
  EXPECT_TRUE(sm.is_active());

  sm.transition(TransferState::Paused);
  EXPECT_TRUE(sm.is_active());

  sm.transition(TransferState::Cancelled);
  EXPECT_FALSE(sm.is_active());
}

// ============================================================================
// Connection State Machine Tests
// ============================================================================

TEST(ConnectionStateMachineTest, InitialState) {
  ConnectionStateMachine sm;
  EXPECT_EQ(sm.current(), ConnectionState::Disconnected);
}

TEST(ConnectionStateMachineTest, ValidConnectionFlow) {
  ConnectionStateMachine sm;

  EXPECT_TRUE(sm.transition(ConnectionState::Connecting).is_ok());
  EXPECT_TRUE(sm.transition(ConnectionState::Establishing).is_ok());
  EXPECT_TRUE(sm.transition(ConnectionState::Handshaking).is_ok());
  EXPECT_TRUE(sm.transition(ConnectionState::Connected).is_ok());

  EXPECT_TRUE(sm.is_connected());
}

TEST(ConnectionStateMachineTest, Disconnect) {
  ConnectionStateMachine sm;

  sm.transition(ConnectionState::Connecting);
  sm.transition(ConnectionState::Establishing);
  sm.transition(ConnectionState::Handshaking);
  sm.transition(ConnectionState::Connected);
  sm.transition(ConnectionState::Disconnecting);
  sm.transition(ConnectionState::Disconnected);

  EXPECT_FALSE(sm.is_connected());
}

TEST(ConnectionStateMachineTest, ErrorRecovery) {
  ConnectionStateMachine sm;

  sm.transition(ConnectionState::Connecting);
  sm.transition(ConnectionState::Error);

  EXPECT_FALSE(sm.is_connected());
  EXPECT_TRUE(sm.transition(ConnectionState::Disconnected).is_ok());
}

// ============================================================================
// Discovery State Machine Tests
// ============================================================================

TEST(DiscoveryStateMachineTest, InitialState) {
  DiscoveryStateMachine sm;
  EXPECT_EQ(sm.current(), DiscoveryState::Idle);
  EXPECT_FALSE(sm.is_active());
}

TEST(DiscoveryStateMachineTest, IdleToActive) {
  DiscoveryStateMachine sm;

  EXPECT_TRUE(sm.transition(DiscoveryState::Active).is_ok());
  EXPECT_TRUE(sm.is_active());
}

TEST(DiscoveryStateMachineTest, StopFromActive) {
  DiscoveryStateMachine sm;

  sm.transition(DiscoveryState::Active);
  sm.transition(DiscoveryState::Idle);

  EXPECT_FALSE(sm.is_active());
}

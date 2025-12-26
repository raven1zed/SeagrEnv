/**
 * @file state_machine.cpp
 * @brief Transfer state machine implementation
 */

#include "seadrop/transfer.h"
#include "seadrop/state_machine.h"

namespace seadrop {

// ============================================================================
// Transfer State Machine
// ============================================================================

// Define valid state transitions
const std::map<TransferState, std::set<TransferState>>
    TransferStateMachine::valid_transitions_ = {
        // Pending -> AwaitingAccept, Cancelled, Failed
        {TransferState::Pending,
         {TransferState::AwaitingAccept, TransferState::Cancelled,
          TransferState::Failed}},

        // AwaitingAccept -> Preparing, Rejected, Cancelled, Failed
        {TransferState::AwaitingAccept,
         {TransferState::Preparing, TransferState::Rejected,
          TransferState::Cancelled, TransferState::Failed}},

        // Preparing -> InProgress, Failed, Cancelled
        {TransferState::Preparing,
         {TransferState::InProgress, TransferState::Failed,
          TransferState::Cancelled}},

        // InProgress -> Paused, Completed, Cancelled, Failed
        {TransferState::InProgress,
         {TransferState::Paused, TransferState::Completed,
          TransferState::Cancelled, TransferState::Failed}},

        // Paused -> InProgress, Cancelled, Failed
        {TransferState::Paused,
         {TransferState::InProgress, TransferState::Cancelled,
          TransferState::Failed}},

        // Terminal states have no valid transitions
        {TransferState::Completed, {}},
        {TransferState::Cancelled, {}},
        {TransferState::Rejected, {}},
        {TransferState::Failed, {}}};

TransferStateMachine::TransferStateMachine() : state_(TransferState::Pending) {}

TransferStateMachine::TransferStateMachine(TransferState initial)
    : state_(initial) {}

TransferState TransferStateMachine::current() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_;
}

Result<void> TransferStateMachine::transition(TransferState to) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Check if transition is valid
  auto it = valid_transitions_.find(state_);
  if (it == valid_transitions_.end()) {
    return Error(ErrorCode::InvalidState,
                 std::string("Unknown state: ") + transfer_state_name(state_));
  }

  if (it->second.find(to) == it->second.end()) {
    return Error(ErrorCode::InvalidState, std::string("Invalid transition: ") +
                                              transfer_state_name(state_) +
                                              " -> " + transfer_state_name(to));
  }

  // Perform transition
  TransferState from = state_;
  state_ = to;

  // Notify callback (outside lock would be better, but keep simple for now)
  if (state_changed_cb_) {
    state_changed_cb_(from, to);
  }

  // Check for terminal state
  if (is_terminal() && terminal_cb_) {
    terminal_cb_(to);
  }

  return Result<void>::ok();
}

void TransferStateMachine::force_transition(TransferState to) {
  std::lock_guard<std::mutex> lock(mutex_);
  TransferState from = state_;
  state_ = to;

  if (state_changed_cb_) {
    state_changed_cb_(from, to);
  }

  if (is_terminal() && terminal_cb_) {
    terminal_cb_(to);
  }
}

bool TransferStateMachine::can_transition(TransferState to) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = valid_transitions_.find(state_);
  if (it == valid_transitions_.end()) {
    return false;
  }
  return it->second.find(to) != it->second.end();
}

std::set<TransferState> TransferStateMachine::valid_transitions() const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = valid_transitions_.find(state_);
  if (it == valid_transitions_.end()) {
    return {};
  }
  return it->second;
}

bool TransferStateMachine::is_terminal() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_ == TransferState::Completed ||
         state_ == TransferState::Cancelled ||
         state_ == TransferState::Rejected || state_ == TransferState::Failed;
}

bool TransferStateMachine::is_active() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_ == TransferState::InProgress || state_ == TransferState::Paused;
}

void TransferStateMachine::reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  TransferState from = state_;
  state_ = TransferState::Pending;

  if (state_changed_cb_ && from != TransferState::Pending) {
    state_changed_cb_(from, TransferState::Pending);
  }
}

void TransferStateMachine::on_state_changed(StateChangedCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_changed_cb_ = std::move(callback);
}

void TransferStateMachine::on_terminal(
    std::function<void(TransferState)> callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  terminal_cb_ = std::move(callback);
}

// ============================================================================
// Connection State Machine
// ============================================================================

const std::map<ConnectionState, std::set<ConnectionState>>
    ConnectionStateMachine::valid_transitions_ = {
        // Disconnected -> Connecting
        {ConnectionState::Disconnected, {ConnectionState::Connecting}},

        // Connecting -> Establishing, Disconnected, Error
        {ConnectionState::Connecting,
         {ConnectionState::Establishing, ConnectionState::Disconnected,
          ConnectionState::Error}},

        // Establishing -> Handshaking, Disconnected, Error
        {ConnectionState::Establishing,
         {ConnectionState::Handshaking, ConnectionState::Disconnected,
          ConnectionState::Error}},

        // Handshaking -> Connected, Disconnected, Error
        {ConnectionState::Handshaking,
         {ConnectionState::Connected, ConnectionState::Disconnected,
          ConnectionState::Error}},

        // Connected -> Disconnecting, Lost
        {ConnectionState::Connected,
         {ConnectionState::Disconnecting, ConnectionState::Lost}},

        // Disconnecting -> Disconnected
        {ConnectionState::Disconnecting, {ConnectionState::Disconnected}},

        // Lost -> Connecting, Disconnected
        {ConnectionState::Lost,
         {ConnectionState::Connecting, ConnectionState::Disconnected}},

        // Error -> Disconnected
        {ConnectionState::Error, {ConnectionState::Disconnected}}};

ConnectionStateMachine::ConnectionStateMachine()
    : state_(ConnectionState::Disconnected) {}

ConnectionState ConnectionStateMachine::current() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_;
}

Result<void> ConnectionStateMachine::transition(ConnectionState to) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = valid_transitions_.find(state_);
  if (it == valid_transitions_.end() ||
      it->second.find(to) == it->second.end()) {
    return Error(ErrorCode::InvalidState,
                 std::string("Invalid connection transition: ") +
                     connection_state_name(state_) + " -> " +
                     connection_state_name(to));
  }

  ConnectionState from = state_;
  state_ = to;

  if (state_changed_cb_) {
    state_changed_cb_(from, to);
  }

  return Result<void>::ok();
}

bool ConnectionStateMachine::can_transition(ConnectionState to) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = valid_transitions_.find(state_);
  if (it == valid_transitions_.end()) {
    return false;
  }
  return it->second.find(to) != it->second.end();
}

std::set<ConnectionState> ConnectionStateMachine::valid_transitions() const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = valid_transitions_.find(state_);
  if (it == valid_transitions_.end()) {
    return {};
  }
  return it->second;
}

bool ConnectionStateMachine::is_connected() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_ == ConnectionState::Connected;
}

void ConnectionStateMachine::reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  ConnectionState from = state_;
  state_ = ConnectionState::Disconnected;

  if (state_changed_cb_ && from != ConnectionState::Disconnected) {
    state_changed_cb_(from, ConnectionState::Disconnected);
  }
}

void ConnectionStateMachine::on_state_changed(StateChangedCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_changed_cb_ = std::move(callback);
}

// ============================================================================
// Discovery State Machine
// ============================================================================

const std::map<DiscoveryState, std::set<DiscoveryState>>
    DiscoveryStateMachine::valid_transitions_ = {
        // Uninitialized -> Idle
        {DiscoveryState::Uninitialized, {DiscoveryState::Idle}},

        // Idle -> Advertising, Scanning, Active, Error
        {DiscoveryState::Idle,
         {DiscoveryState::Advertising, DiscoveryState::Scanning,
          DiscoveryState::Active, DiscoveryState::Error}},

        // Advertising -> Active, Idle, Scanning
        {DiscoveryState::Advertising,
         {DiscoveryState::Active, DiscoveryState::Idle,
          DiscoveryState::Scanning}},

        // Scanning -> Active, Idle, Advertising
        {DiscoveryState::Scanning,
         {DiscoveryState::Active, DiscoveryState::Idle,
          DiscoveryState::Advertising}},

        // Active -> Advertising, Scanning, Idle
        {DiscoveryState::Active,
         {DiscoveryState::Advertising, DiscoveryState::Scanning,
          DiscoveryState::Idle}},

        // Error -> Idle, Uninitialized
        {DiscoveryState::Error,
         {DiscoveryState::Idle, DiscoveryState::Uninitialized}}};

DiscoveryStateMachine::DiscoveryStateMachine() : state_(DiscoveryState::Idle) {}

DiscoveryState DiscoveryStateMachine::current() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_;
}

Result<void> DiscoveryStateMachine::transition(DiscoveryState to) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = valid_transitions_.find(state_);
  if (it == valid_transitions_.end() ||
      it->second.find(to) == it->second.end()) {
    return Error(ErrorCode::InvalidState,
                 std::string("Invalid discovery transition: ") +
                     discovery_state_name(state_) + " -> " +
                     discovery_state_name(to));
  }

  DiscoveryState from = state_;
  state_ = to;

  if (state_changed_cb_) {
    state_changed_cb_(from, to);
  }

  return Result<void>::ok();
}

bool DiscoveryStateMachine::can_transition(DiscoveryState to) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = valid_transitions_.find(state_);
  if (it == valid_transitions_.end()) {
    return false;
  }
  return it->second.find(to) != it->second.end();
}

bool DiscoveryStateMachine::is_active() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_ == DiscoveryState::Advertising ||
         state_ == DiscoveryState::Scanning || state_ == DiscoveryState::Active;
}

void DiscoveryStateMachine::reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  DiscoveryState from = state_;
  state_ = DiscoveryState::Idle;

  if (state_changed_cb_ && from != DiscoveryState::Idle) {
    state_changed_cb_(from, DiscoveryState::Idle);
  }
}

void DiscoveryStateMachine::on_state_changed(StateChangedCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_changed_cb_ = std::move(callback);
}

} // namespace seadrop

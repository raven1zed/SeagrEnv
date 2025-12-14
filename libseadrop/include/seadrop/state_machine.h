/**
 * @file state_machine.h
 * @brief Transfer state machine for SeaDrop
 *
 * Manages transfer states with validated transitions and callbacks.
 */

#ifndef SEADROP_STATE_MACHINE_H
#define SEADROP_STATE_MACHINE_H

#include "seadrop/connection.h"
#include "seadrop/discovery.h"
#include "seadrop/error.h"
#include "seadrop/types.h"
#include <functional>
#include <map>
#include <mutex>
#include <set>

namespace seadrop {

// ============================================================================
// Transfer State Machine
// ============================================================================

/**
 * @brief Manages state transitions for file transfers
 *
 * Enforces valid state transitions and emits callbacks on changes.
 * Thread-safe.
 *
 * @code
 *   TransferStateMachine sm;
 *
 *   sm.on_state_changed([](TransferState from, TransferState to) {
 *       std::cout << "State: " << transfer_state_name(from)
 *                 << " -> " << transfer_state_name(to) << std::endl;
 *   });
 *
 *   sm.transition(TransferState::AwaitingAccept);
 *   sm.transition(TransferState::Preparing);
 *   sm.transition(TransferState::InProgress);
 * @endcode
 */
class SEADROP_API TransferStateMachine {
public:
  TransferStateMachine();
  explicit TransferStateMachine(TransferState initial);

  /**
   * @brief Get current state
   */
  TransferState current() const;

  /**
   * @brief Attempt to transition to a new state
   * @param to Target state
   * @return Success or error if transition is invalid
   */
  Result<void> transition(TransferState to);

  /**
   * @brief Force transition to a state (bypasses validation)
   * @param to Target state
   *
   * Use with caution. Primarily for error recovery.
   */
  void force_transition(TransferState to);

  /**
   * @brief Check if transition to state is valid
   * @param to Target state
   * @return true if transition is allowed
   */
  bool can_transition(TransferState to) const;

  /**
   * @brief Get all valid next states from current state
   */
  std::set<TransferState> valid_transitions() const;

  /**
   * @brief Check if state machine is in a terminal state
   * @return true if Completed, Cancelled, Rejected, or Failed
   */
  bool is_terminal() const;

  /**
   * @brief Check if transfer is active (in progress or paused)
   */
  bool is_active() const;

  /**
   * @brief Reset to initial state
   */
  void reset();

  // ========================================================================
  // Callbacks
  // ========================================================================

  using StateChangedCallback =
      std::function<void(TransferState from, TransferState to)>;

  /**
   * @brief Register callback for state changes
   * @param callback Called after each successful transition
   */
  void on_state_changed(StateChangedCallback callback);

  /**
   * @brief Register callback for terminal states
   * @param callback Called when entering Completed/Cancelled/Rejected/Failed
   */
  void on_terminal(std::function<void(TransferState)> callback);

private:
  mutable std::mutex mutex_;
  TransferState state_;
  StateChangedCallback state_changed_cb_;
  std::function<void(TransferState)> terminal_cb_;

  static const std::map<TransferState, std::set<TransferState>>
      valid_transitions_;
};

// ============================================================================
// Connection State Machine
// ============================================================================

/**
 * @brief Manages state transitions for connections
 */
class SEADROP_API ConnectionStateMachine {
public:
  ConnectionStateMachine();

  ConnectionState current() const;
  Result<void> transition(ConnectionState to);
  bool can_transition(ConnectionState to) const;
  std::set<ConnectionState> valid_transitions() const;
  bool is_connected() const;
  void reset();

  using StateChangedCallback =
      std::function<void(ConnectionState from, ConnectionState to)>;
  void on_state_changed(StateChangedCallback callback);

private:
  mutable std::mutex mutex_;
  ConnectionState state_ = ConnectionState::Disconnected;
  StateChangedCallback state_changed_cb_;

  static const std::map<ConnectionState, std::set<ConnectionState>>
      valid_transitions_;
};

// ============================================================================
// Discovery State Machine
// ============================================================================

/**
 * @brief Manages state transitions for device discovery
 */
class SEADROP_API DiscoveryStateMachine {
public:
  DiscoveryStateMachine();

  DiscoveryState current() const;
  Result<void> transition(DiscoveryState to);
  bool can_transition(DiscoveryState to) const;
  bool is_active() const;
  void reset();

  using StateChangedCallback =
      std::function<void(DiscoveryState from, DiscoveryState to)>;
  void on_state_changed(StateChangedCallback callback);

private:
  mutable std::mutex mutex_;
  DiscoveryState state_ = DiscoveryState::Idle;
  StateChangedCallback state_changed_cb_;

  static const std::map<DiscoveryState, std::set<DiscoveryState>>
      valid_transitions_;
};

} // namespace seadrop

#endif // SEADROP_STATE_MACHINE_H

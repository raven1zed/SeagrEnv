/**
 * @file device.h
 * @brief Device discovery, pairing, and trust management for SeaDrop
 *
 * This module handles:
 * - Discovered device information
 * - Device pairing with 6-digit PIN
 * - Trust store (persistent list of paired devices)
 * - Trust levels and permissions
 */

#ifndef SEADROP_DEVICE_H
#define SEADROP_DEVICE_H

#include "distance.h"
#include "error.h"
#include "platform.h"
#include "types.h"
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace seadrop {

// ============================================================================
// Trust Level
// ============================================================================

/**
 * @brief Trust level for a device
 *
 * This determines what operations are allowed with the device.
 */
enum class TrustLevel : uint8_t {
  /// Unknown device, never seen before
  Unknown = 0,

  /// Device has been seen but not paired
  Discovered = 1,

  /// Pending pairing confirmation
  PairingPending = 2,

  /// Fully trusted, paired device
  Trusted = 3,

  /// Blocked device (user explicitly blocked)
  Blocked = 255
};

/**
 * @brief Get human-readable name for trust level
 */
SEADROP_API const char *trust_level_name(TrustLevel level);

// ============================================================================
// Device Information
// ============================================================================

/**
 * @brief Complete information about a discovered or known device
 */
struct Device {
  // Identity
  DeviceId id;      // Unique device ID (from public key)
  std::string name; // User-visible device name

  // Platform info
  DevicePlatform platform = DevicePlatform::Unknown;
  DeviceType type = DeviceType::Unknown;
  std::string seadrop_version; // Protocol version

  // Trust
  TrustLevel trust_level = TrustLevel::Unknown;

  // Capabilities
  bool supports_wifi_direct = false;
  bool supports_bluetooth = false;
  bool supports_clipboard = false;

  // Current connection state
  bool is_connected = false;
  ConnectionType connection_type = ConnectionType::None;

  // Distance (if connected)
  DistanceInfo distance;

  // Timestamps
  std::chrono::system_clock::time_point first_seen;
  std::chrono::system_clock::time_point last_seen;
  std::chrono::system_clock::time_point paired_at; // When trust was established

  // User notes
  std::string user_alias; // User-set nickname (optional)
  std::string notes;      // User notes (optional)

  // ========================================================================
  // Helper Methods
  // ========================================================================

  /// Check if device is currently trusted
  bool is_trusted() const { return trust_level == TrustLevel::Trusted; }

  /// Check if device is blocked
  bool is_blocked() const { return trust_level == TrustLevel::Blocked; }

  /// Get display name (user alias if set, otherwise device name)
  std::string display_name() const {
    return user_alias.empty() ? name : user_alias;
  }

  /// Check if device can auto-accept files based on zone and trust
  bool can_auto_accept_files(TrustZone zone) const;

  /// Check if device can auto-share clipboard based on zone and trust
  bool can_auto_clipboard(TrustZone zone) const;
};

// ============================================================================
// Pairing Request
// ============================================================================

/**
 * @brief Information about an incoming pairing request
 */
struct PairingRequest {
  Device device;                                    // Device requesting to pair
  std::string pin_code;                             // 6-digit PIN to display
  std::chrono::steady_clock::time_point expires_at; // When request expires
  bool is_incoming; // True if we received the request

  /// Check if request has expired
  bool is_expired() const {
    return std::chrono::steady_clock::now() > expires_at;
  }

  /// Get remaining time in seconds
  int remaining_seconds() const;
};

// ============================================================================
// Device Store (Trust Database)
// ============================================================================

/**
 * @brief Persistent storage for trusted devices
 *
 * The device store manages the database of known devices, their trust
 * levels, and cryptographic keys for secure communication.
 */
class SEADROP_API DeviceStore {
public:
  DeviceStore();
  ~DeviceStore();

  // Non-copyable
  DeviceStore(const DeviceStore &) = delete;
  DeviceStore &operator=(const DeviceStore &) = delete;

  // ========================================================================
  // Initialization
  // ========================================================================

  /**
   * @brief Initialize the device store with a database path
   * @param db_path Path to SQLite database file
   * @return Success or error
   */
  Result<void> init(const std::string &db_path);

  /**
   * @brief Close the device store
   */
  void close();

  // ========================================================================
  // Device Queries
  // ========================================================================

  /**
   * @brief Get a device by ID
   * @param id Device ID to look up
   * @return Device info or error if not found
   */
  Result<Device> get_device(const DeviceId &id) const;

  /**
   * @brief Get all trusted devices
   * @return List of trusted devices
   */
  std::vector<Device> get_trusted_devices() const;

  /**
   * @brief Get all blocked devices
   * @return List of blocked devices
   */
  std::vector<Device> get_blocked_devices() const;

  /**
   * @brief Get all known devices (trusted + blocked + discovered)
   * @return List of all devices
   */
  std::vector<Device> get_all_devices() const;

  /**
   * @brief Check if a device is trusted
   */
  bool is_trusted(const DeviceId &id) const;

  /**
   * @brief Check if a device is blocked
   */
  bool is_blocked(const DeviceId &id) const;

  // ========================================================================
  // Device Modification
  // ========================================================================

  /**
   * @brief Add or update a device
   * @param device Device info to store
   * @return Success or error
   */
  Result<void> save_device(const Device &device);

  /**
   * @brief Trust a device (after successful pairing)
   * @param id Device ID to trust
   * @param shared_key Shared encryption key from pairing
   * @return Success or error
   */
  Result<void> trust_device(const DeviceId &id, const Bytes &shared_key);

  /**
   * @brief Block a device
   * @param id Device ID to block
   * @return Success or error
   */
  Result<void> block_device(const DeviceId &id);

  /**
   * @brief Remove trust from a device (unpair)
   * @param id Device ID to untrust
   * @return Success or error
   */
  Result<void> untrust_device(const DeviceId &id);

  /**
   * @brief Unblock a device
   * @param id Device ID to unblock
   * @return Success or error
   */
  Result<void> unblock_device(const DeviceId &id);

  /**
   * @brief Delete a device from the store entirely
   * @param id Device ID to delete
   * @return Success or error
   */
  Result<void> delete_device(const DeviceId &id);

  /**
   * @brief Set user alias for a device
   */
  Result<void> set_device_alias(const DeviceId &id, const std::string &alias);

  // ========================================================================
  // Cryptographic Keys
  // ========================================================================

  /**
   * @brief Get shared encryption key for a trusted device
   * @param id Device ID
   * @return Shared key or error if not trusted
   */
  Result<Bytes> get_shared_key(const DeviceId &id) const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Pairing Manager
// ============================================================================

/**
 * @brief Handles the device pairing process
 *
 * Pairing uses a 6-digit PIN displayed on both devices. The user
 * verifies the PINs match, creating a trusted relationship with
 * a shared encryption key.
 */
class SEADROP_API PairingManager {
public:
  PairingManager();
  ~PairingManager();

  // ========================================================================
  // Lifecycle
  // ========================================================================

  /**
   * @brief Initialize with device store reference
   */
  Result<void> init(DeviceStore *store);

  // ========================================================================
  // Pairing Operations
  // ========================================================================

  /**
   * @brief Initiate pairing with a device
   * @param device Device to pair with
   * @return Pairing request with PIN code to display, or error
   */
  Result<PairingRequest> initiate_pairing(const Device &device);

  /**
   * @brief Accept an incoming pairing request
   * @param request The pairing request to accept
   * @return Success or error
   *
   * This should be called after the user verifies the PIN matches.
   */
  Result<void> accept_pairing(const PairingRequest &request);

  /**
   * @brief Reject an incoming pairing request
   * @param request The pairing request to reject
   */
  void reject_pairing(const PairingRequest &request);

  /**
   * @brief Cancel an outgoing pairing request
   */
  void cancel_pairing();

  /**
   * @brief Check if pairing is in progress
   */
  bool is_pairing() const;

  /**
   * @brief Get current pairing request (if any)
   */
  std::optional<PairingRequest> get_current_pairing() const;

  // ========================================================================
  // Callbacks
  // ========================================================================

  using PairingRequestCallback = std::function<void(const PairingRequest &)>;
  using PairingCompleteCallback =
      std::function<void(const Device &, bool success)>;

  /**
   * @brief Set callback for incoming pairing requests
   */
  void on_pairing_request(PairingRequestCallback callback);

  /**
   * @brief Set callback for pairing completion (success or failure)
   */
  void on_pairing_complete(PairingCompleteCallback callback);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Device Callbacks
// ============================================================================

/// Called when a new device is discovered
using DeviceDiscoveredCallback = std::function<void(const Device &device)>;

/// Called when a device is no longer visible
using DeviceLostCallback = std::function<void(const DeviceId &id)>;

/// Called when device information is updated (RSSI, name, etc.)
using DeviceUpdatedCallback = std::function<void(const Device &device)>;

} // namespace seadrop

#endif // SEADROP_DEVICE_H

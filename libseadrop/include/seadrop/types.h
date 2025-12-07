/**
 * @file types.h
 * @brief Core type definitions for SeaDrop
 */

#ifndef SEADROP_TYPES_H
#define SEADROP_TYPES_H

#include "platform.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace seadrop {

// ============================================================================
// Basic Types
// ============================================================================

using Byte = uint8_t;
using ByteVec = std::vector<Byte>;
using Bytes = std::vector<Byte>;
using ByteSpan = std::pair<const Byte *, size_t>;

// ============================================================================
// Identifiers
// ============================================================================

/// Device unique identifier (32 bytes, derived from public key)
struct DeviceId {
  static constexpr size_t SIZE = 32;
  std::array<Byte, SIZE> data;

  bool operator==(const DeviceId &other) const { return data == other.data; }
  bool operator!=(const DeviceId &other) const { return data != other.data; }
  bool operator<(const DeviceId &other) const { return data < other.data; }

  std::string to_hex() const;
  static std::optional<DeviceId> from_hex(const std::string &hex);

  bool is_zero() const;
};

/// Transfer session identifier
struct TransferId {
  static constexpr size_t SIZE = 16;
  std::array<Byte, SIZE> data;

  bool operator==(const TransferId &other) const { return data == other.data; }
  bool operator!=(const TransferId &other) const { return data != other.data; }

  std::string to_hex() const;
  static TransferId generate();
};

// ============================================================================
// Device Information
// ============================================================================

/// Device platform type
enum class DevicePlatform : uint8_t {
  Unknown = 0,
  Linux = 1,
  Windows = 2,
  MacOS = 3,
  Android = 4,
  iOS = 5
};

/// Device type (form factor)
enum class DeviceType : uint8_t {
  Unknown = 0,
  Desktop = 1,
  Laptop = 2,
  Tablet = 3,
  Phone = 4,
  TV = 5,
  Watch = 6
};

/// Connection type used for transfer
enum class ConnectionType : uint8_t {
  None = 0,
  WifiDirect = 1,
  Bluetooth = 2,
  LocalNet = 3, // Same network, direct TCP
  Internet = 4  // Relay server
};

/// Information about a discovered device
struct DeviceInfo {
  DeviceId id;
  std::string name;
  DevicePlatform platform = DevicePlatform::Unknown;
  DeviceType device_type = DeviceType::Unknown;
  std::string version; // SeaDrop version

  // Discovery metadata
  ConnectionType preferred_connection = ConnectionType::None;
  int signal_strength = 0; // -100 to 0 dBm
  bool supports_wifi_direct = false;
  bool supports_bluetooth = false;

  // Timestamps
  std::chrono::system_clock::time_point first_seen;
  std::chrono::system_clock::time_point last_seen;

  // User preferences
  bool is_trusted = false;
  bool is_blocked = false;
};

// ============================================================================
// Transfer Types
// ============================================================================

// Note: TransferState moved back here from transfer.h
enum class TransferState : uint8_t {
  Pending = 0,        // Waiting for user approval
  AwaitingAccept = 1, // Waiting for receiver to accept
  Preparing = 2,      // Preparing files
  Connecting = 3,     // Establishing connection
  InProgress = 4,     // Actively transferring
  Paused = 5,         // User paused
  Completed = 6,      // Successfully completed
  Cancelled = 7,      // User cancelled
  Failed = 8,         // Error occurred
  Rejected = 9        // Receiver rejected
};

/// Transfer direction
enum class TransferDirection : uint8_t { Send = 0, Receive = 1 };

/// Single file in a transfer
struct TransferFile {
  std::string path;      // Relative path within transfer
  std::string name;      // File name
  uint64_t size = 0;     // File size in bytes
  std::string mime_type; // MIME type

  // Progress
  uint64_t bytes_transferred = 0;
  bool is_complete = false;
  bool has_error = false;
};

/// Information about a file transfer
struct TransferInfo {
  TransferId id;
  TransferDirection direction;
  TransferState state = TransferState::Pending;
  ConnectionType connection_type = ConnectionType::None;

  // Peer info
  DeviceId peer_id;
  std::string peer_name;

  // Files
  std::vector<TransferFile> files;
  uint64_t total_size = 0;  // Total bytes
  uint64_t transferred = 0; // Bytes transferred
  uint32_t file_count = 0;

  // Timing
  std::chrono::system_clock::time_point started;
  std::chrono::system_clock::time_point completed;
  std::chrono::milliseconds elapsed{0};

  // Statistics
  double progress = 0.0;       // 0.0 to 1.0
  double speed_bps = 0.0;      // Current bytes per second
  std::chrono::seconds eta{0}; // Estimated time remaining

  // Error info
  std::string error_message;
  int error_code = 0;
};

// ============================================================================
// Callbacks
// ============================================================================

/// Callback for device discovery events
// CONFLICT FIX: These are redefined in device.h with 'Device' instead of
// 'DeviceInfo' using DeviceDiscoveredCallback = std::function<void(const
// DeviceInfo &device)>;
using DeviceLostCallback = std::function<void(const DeviceId &id)>;
// using DeviceUpdatedCallback = std::function<void(const DeviceInfo &device)>;

/// Callback for transfer events
using TransferRequestCallback =
    std::function<void(const TransferInfo &transfer)>;
using TransferProgressCallback =
    std::function<void(const TransferInfo &transfer)>;
using TransferCompleteCallback =
    std::function<void(const TransferInfo &transfer)>;
using TransferErrorCallback =
    std::function<void(const TransferInfo &transfer, const std::string &error)>;

// ============================================================================
// Configuration
// ============================================================================

/// Discovery visibility mode
enum class VisibilityMode : uint8_t {
  Hidden = 0,       // Not discoverable
  ContactsOnly = 1, // Only trusted devices can see us
  Everyone = 2      // All nearby devices can see us
};

/// Reception mode (how to handle incoming transfers)
enum class ReceptionMode : uint8_t {
  AlwaysAsk = 0,    // Always prompt user
  TrustedAuto = 1,  // Auto-accept from trusted devices
  AlwaysAccept = 2, // Accept all (dangerous)
  AlwaysReject = 3  // Reject all
};

/// User configuration
struct Config {
  // Identity
  std::string device_name;

  // Visibility
  VisibilityMode visibility = VisibilityMode::Everyone;
  ReceptionMode reception = ReceptionMode::AlwaysAsk;

  // Paths
  std::string download_path;
  bool use_subfolders = true; // Create subdirs per sender

  // Limits
  uint64_t max_file_size = 0; // 0 = unlimited
  uint32_t max_files_per_transfer = 1000;

  // Network
  bool enable_wifi_direct = true;
  bool enable_bluetooth = true;
  bool enable_local_network = true;
  uint16_t tcp_port = 17530; // Default SeaDrop port

  // Security
  bool require_encryption = true;
  bool verify_checksums = true;
};

} // namespace seadrop

#endif // SEADROP_TYPES_H

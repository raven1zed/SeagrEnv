/**
 * @file seadrop.h
 * @brief Main SeaDrop API Header
 *
 * SeaDrop - Cross-Platform P2P File Sharing
 * "SeamlessDrop" - An Open-Source AirDrop for Linux + Android
 *
 * This is the main header file for the SeaDrop library. It provides
 * a unified API for:
 * - Device discovery (via BLE)
 * - P2P connection (via WiFi Direct)
 * - File transfer (encrypted, chunked)
 * - Clipboard sharing
 * - Distance-based trust zones
 *
 * Quick Start:
 * @code
 *   #include <seadrop/seadrop.h>
 *
 *   seadrop::SeaDrop app;
 *   app.init(config);
 *
 *   app.on_device_discovered([](const Device& d) {
 *       std::cout << "Found: " << d.name << std::endl;
 *   });
 *
 *   app.start_discovery();
 * @endcode
 *
 * @author SeaDrop Team
 * @license GPL-3.0-or-later
 */

#ifndef SEADROP_SEADROP_H
#define SEADROP_SEADROP_H

// Core headers (in dependency order)
#include "error.h"
#include "platform.h"
#include "types.h"

// Feature modules (in dependency order)
#include "clipboard.h"
#include "config.h"
#include "connection.h"
#include "database.h"
#include "device.h"
#include "discovery.h"
#include "distance.h"
#include "security.h"
#include "transfer.h"

#include <filesystem>
#include <functional>
#include <memory>

namespace seadrop {

// ============================================================================
// Version Information
// ============================================================================

/// SeaDrop major version
constexpr int VERSION_MAJOR = 1;

/// SeaDrop minor version
constexpr int VERSION_MINOR = 0;

/// SeaDrop patch version
constexpr int VERSION_PATCH = 0;

/// SeaDrop version string
constexpr const char *VERSION_STRING = "1.0.0";

/// SeaDrop protocol version (for compatibility checks)
constexpr int PROTOCOL_VERSION = 1;

/**
 * @brief Get version information
 */
struct VersionInfo {
  int major = VERSION_MAJOR;
  int minor = VERSION_MINOR;
  int patch = VERSION_PATCH;
  const char *version_string = VERSION_STRING;
  int protocol_version = PROTOCOL_VERSION;
  const char *build_date = __DATE__;
  const char *build_time = __TIME__;
};

SEADROP_API VersionInfo get_version();

// ============================================================================
// SeaDrop State
// ============================================================================

/**
 * @brief Overall state of the SeaDrop instance
 */
enum class SeaDropState : uint8_t {
  /// Not initialized
  Uninitialized = 0,

  /// Initialized, but not active
  Idle = 1,

  /// Discovering nearby devices
  Discovering = 2,

  /// Connected to a peer, ready to transfer
  Connected = 3,

  /// Active file transfer in progress
  Transferring = 4,

  /// Error state
  Error = 255
};

/**
 * @brief Get human-readable name for state
 */
SEADROP_API const char *seadrop_state_name(SeaDropState state);

// ============================================================================
// Main SeaDrop Class
// ============================================================================

/**
 * @brief Main SeaDrop Application Class
 *
 * This is the primary interface for using SeaDrop. It coordinates all
 * the subsystems (discovery, connection, transfer, clipboard) and
 * provides a simple API for common operations.
 *
 * Typical usage flow:
 * 1. Create SeaDrop instance
 * 2. Configure with init()
 * 3. Set up callbacks (on_device_discovered, etc.)
 * 4. Start discovery with start_discovery()
 * 5. Connect to a device with connect()
 * 6. Send files with send_file() or send_files()
 * 7. Handle incoming transfers via callbacks
 *
 * Example:
 * @code
 *   seadrop::SeaDrop app;
 *
 *   SeaDropConfig config;
 *   config.device_name = "My Laptop";
 *   config.download_path = "/home/user/Downloads/SeaDrop";
 *
 *   app.init(config);
 *
 *   // Discovery callback
 *   app.on_device_discovered([&](const Device& device) {
 *       if (device.is_trusted()) {
 *           app.connect(device);
 *       }
 *   });
 *
 *   // Transfer callback
 *   app.on_transfer_request([&](const TransferRequest& req) {
 *       if (req.auto_accepted) {
 *           show_toast("Receiving from " + req.sender.name);
 *       } else {
 *           show_accept_dialog(req);
 *       }
 *   });
 *
 *   app.start_discovery();
 * @endcode
 */
class SEADROP_API SeaDrop {
public:
  SeaDrop();
  ~SeaDrop();

  // Non-copyable
  SeaDrop(const SeaDrop &) = delete;
  SeaDrop &operator=(const SeaDrop &) = delete;

  // ========================================================================
  // Lifecycle
  // ========================================================================

  /**
   * @brief Initialize SeaDrop
   * @param config Configuration options
   * @return Success or error
   */
  Result<void> init(const SeaDropConfig &config);

  /**
   * @brief Shutdown SeaDrop and release all resources
   */
  void shutdown();

  /**
   * @brief Check if initialized
   */
  bool is_initialized() const;

  /**
   * @brief Get current state
   */
  SeaDropState get_state() const;

  // ========================================================================
  // Discovery
  // ========================================================================

  /**
   * @brief Start discovering nearby devices
   * @return Success or error
   */
  Result<void> start_discovery();

  /**
   * @brief Stop discovery
   */
  void stop_discovery();

  /**
   * @brief Check if discovery is active
   */
  bool is_discovering() const;

  /**
   * @brief Get list of nearby devices
   */
  std::vector<Device> get_nearby_devices() const;

  // ========================================================================
  // Connection
  // ========================================================================

  /**
   * @brief Connect to a device via WiFi Direct
   * @param device Device to connect to
   * @return Success or error
   */
  Result<void> connect(const Device &device);

  /**
   * @brief Disconnect from current peer
   */
  void disconnect();

  /**
   * @brief Check if connected to a peer
   */
  bool is_connected() const;

  /**
   * @brief Get connected peer info
   * @return Device info or nullopt if not connected
   */
  std::optional<Device> get_connected_peer() const;

  // ========================================================================
  // Pairing
  // ========================================================================

  /**
   * @brief Initiate pairing with a device
   * @param device Device to pair with
   * @return Pairing request with PIN, or error
   */
  Result<PairingRequest> pair(const Device &device);

  /**
   * @brief Accept incoming pairing request
   * @param request Pairing request to accept
   * @return Success or error
   */
  Result<void> accept_pairing(const PairingRequest &request);

  /**
   * @brief Reject incoming pairing request
   */
  void reject_pairing(const PairingRequest &request);

  /**
   * @brief Check if device is trusted (paired)
   */
  bool is_trusted(const DeviceId &id) const;

  /**
   * @brief Get list of trusted devices
   */
  std::vector<Device> get_trusted_devices() const;

  /**
   * @brief Remove trust from a device (unpair)
   */
  Result<void> untrust_device(const DeviceId &id);

  /**
   * @brief Block a device
   */
  Result<void> block_device(const DeviceId &id);

  // ========================================================================
  // File Transfer
  // ========================================================================

  /**
   * @brief Send a file to connected peer
   * @param path Path to file
   * @return Transfer ID or error
   */
  Result<TransferId> send_file(const std::filesystem::path &path);

  /**
   * @brief Send multiple files to connected peer
   * @param paths Paths to files
   * @return Transfer ID or error
   */
  Result<TransferId>
  send_files(const std::vector<std::filesystem::path> &paths);

  /**
   * @brief Send a directory (recursively) to connected peer
   * @param path Path to directory
   * @return Transfer ID or error
   */
  Result<TransferId> send_directory(const std::filesystem::path &path);

  /**
   * @brief Send text content to connected peer
   * @param text Text to send
   * @return Transfer ID or error
   */
  Result<TransferId> send_text(const std::string &text);

  /**
   * @brief Accept an incoming transfer
   * @param transfer_id Transfer to accept
   * @return Success or error
   */
  Result<void> accept_transfer(const TransferId &transfer_id);

  /**
   * @brief Reject an incoming transfer
   * @param transfer_id Transfer to reject
   */
  void reject_transfer(const TransferId &transfer_id);

  /**
   * @brief Cancel an ongoing transfer
   * @param transfer_id Transfer to cancel
   */
  void cancel_transfer(const TransferId &transfer_id);

  /**
   * @brief Get transfer progress
   */
  std::optional<TransferProgress>
  get_transfer_progress(const TransferId &id) const;

  // ========================================================================
  // Clipboard
  // ========================================================================

  /**
   * @brief Push current clipboard to connected peer
   * @return Success or error
   */
  Result<void> push_clipboard();

  /**
   * @brief Enable/disable auto-clipboard in Zone 1
   * @param enabled Enable auto-share
   */
  void set_auto_clipboard(bool enabled);

  /**
   * @brief Check if auto-clipboard is enabled
   */
  bool is_auto_clipboard_enabled() const;

  // ========================================================================
  // Distance Monitoring
  // ========================================================================

  /**
   * @brief Get current distance info for connected peer
   * @return Distance info or nullopt if not connected
   */
  std::optional<DistanceInfo> get_peer_distance() const;

  /**
   * @brief Get current trust zone for connected peer
   */
  TrustZone get_peer_zone() const;

  /**
   * @brief Set zone thresholds
   */
  Result<void> set_zone_thresholds(const ZoneThresholds &thresholds);

  // ========================================================================
  // Configuration
  // ========================================================================

  /**
   * @brief Get current configuration
   */
  const SeaDropConfig &get_config() const;

  /**
   * @brief Update configuration
   */
  Result<void> set_config(const SeaDropConfig &config);

  /**
   * @brief Get config manager for direct access
   */
  ConfigManager &get_config_manager();

  // ========================================================================
  // Local Device Info
  // ========================================================================

  /**
   * @brief Get our device info
   */
  Device get_local_device() const;

  /**
   * @brief Get our device ID
   */
  DeviceId get_local_id() const;

  /**
   * @brief Set device name
   */
  Result<void> set_device_name(const std::string &name);

  // ========================================================================
  // Callbacks - Discovery
  // ========================================================================

  /**
   * @brief Set callback for device discovered
   */
  void on_device_discovered(std::function<void(const Device &)> callback);

  /**
   * @brief Set callback for device lost (no longer visible)
   */
  void on_device_lost(std::function<void(const DeviceId &)> callback);

  /**
   * @brief Set callback for device updated (RSSI, etc.)
   */
  void on_device_updated(std::function<void(const Device &)> callback);

  // ========================================================================
  // Callbacks - Connection
  // ========================================================================

  /**
   * @brief Set callback for connection request (incoming)
   */
  void on_connection_request(std::function<void(const Device &)> callback);

  /**
   * @brief Set callback for successful connection
   */
  void on_connected(std::function<void(const Device &)> callback);

  /**
   * @brief Set callback for disconnection
   */
  void on_disconnected(
      std::function<void(const DeviceId &, const std::string &)> callback);

  // ========================================================================
  // Callbacks - Pairing
  // ========================================================================

  /**
   * @brief Set callback for incoming pairing request
   */
  void on_pairing_request(std::function<void(const PairingRequest &)> callback);

  /**
   * @brief Set callback for pairing complete
   */
  void on_pairing_complete(
      std::function<void(const Device &, bool success)> callback);

  // ========================================================================
  // Callbacks - Transfer
  // ========================================================================

  /**
   * @brief Set callback for incoming transfer request
   */
  void
  on_transfer_request(std::function<void(const TransferRequest &)> callback);

  /**
   * @brief Set callback for transfer progress
   */
  void
  on_transfer_progress(std::function<void(const TransferProgress &)> callback);

  /**
   * @brief Set callback for transfer complete
   */
  void
  on_transfer_complete(std::function<void(const TransferResult &)> callback);

  /**
   * @brief Set callback for file received (called per file)
   */
  void on_file_received(std::function<void(const FileInfo &)> callback);

  // ========================================================================
  // Callbacks - Clipboard
  // ========================================================================

  /**
   * @brief Set callback for clipboard received
   */
  void on_clipboard_received(
      std::function<void(const ReceivedClipboard &)> callback);

  // ========================================================================
  // Callbacks - Distance/Security
  // ========================================================================

  /**
   * @brief Set callback for zone changes
   */
  void on_zone_changed(std::function<void(const ZoneChangeEvent &)> callback);

  /**
   * @brief Set callback for security alerts
   */
  void on_security_alert(
      std::function<void(const DeviceId &, const std::string &)> callback);

  // ========================================================================
  // Callbacks - General
  // ========================================================================

  /**
   * @brief Set callback for state changes
   */
  void on_state_changed(std::function<void(SeaDropState)> callback);

  /**
   * @brief Set callback for errors
   */
  void on_error(std::function<void(const Error &)> callback);

  // ========================================================================
  // Component Access (Advanced)
  // ========================================================================

  /**
   * @brief Get discovery manager
   */
  DiscoveryManager &get_discovery_manager();

  /**
   * @brief Get connection manager
   */
  ConnectionManager &get_connection_manager();

  /**
   * @brief Get transfer manager
   */
  TransferManager &get_transfer_manager();

  /**
   * @brief Get clipboard manager
   */
  ClipboardManager &get_clipboard_manager();

  /**
   * @brief Get distance monitor
   */
  DistanceMonitor &get_distance_monitor();

  /**
   * @brief Get device store
   */
  DeviceStore &get_device_store();

  /**
   * @brief Get database
   */
  Database &get_database();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Global Helpers
// ============================================================================

/**
 * @brief Get platform name
 */
SEADROP_API const char *get_platform_name();

/**
 * @brief Check if WiFi Direct is supported on this platform
 */
SEADROP_API bool is_wifi_direct_supported();

/**
 * @brief Check if Bluetooth is supported on this platform
 */
SEADROP_API bool is_bluetooth_supported();

/**
 * @brief Get default device name based on hostname/device model
 */
SEADROP_API std::string get_default_device_name();

/**
 * @brief Open file with default application
 */
SEADROP_API Result<void> open_file(const std::filesystem::path &path);

/**
 * @brief Reveal file in file manager
 */
SEADROP_API Result<void>
reveal_in_file_manager(const std::filesystem::path &path);

} // namespace seadrop

#endif // SEADROP_SEADROP_H

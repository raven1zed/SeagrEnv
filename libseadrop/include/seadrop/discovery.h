/**
 * @file discovery.h
 * @brief BLE-based device discovery for SeaDrop
 *
 * SeaDrop uses Bluetooth Low Energy (BLE) for discovering nearby
 * devices. This provides low-power, always-on discovery while
 * WiFi Direct handles the actual data transfer.
 *
 * Discovery Flow:
 *   1. Advertise our presence via BLE
 *   2. Scan for other SeaDrop devices
 *   3. Exchange device info (name, capabilities)
 *   4. User selects device for connection
 *   5. Hand off to WiFi Direct for data transfer
 */

#ifndef SEADROP_DISCOVERY_H
#define SEADROP_DISCOVERY_H

#include "device.h"
#include "error.h"
#include "platform.h"
#include "types.h"
#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace seadrop {

// ============================================================================
// Discovery State
// ============================================================================

/**
 * @brief Current state of the discovery subsystem
 */
enum class DiscoveryState : uint8_t {
  /// Not initialized
  Uninitialized = 0,

  /// Initialized but not scanning/advertising
  Idle = 1,

  /// Advertising our presence only
  Advertising = 2,

  /// Scanning for devices only
  Scanning = 3,

  /// Both advertising and scanning (normal mode)
  Active = 4,

  /// Error state (Bluetooth off, permission denied, etc.)
  Error = 255
};

/**
 * @brief Get human-readable name for discovery state
 */
SEADROP_API const char *discovery_state_name(DiscoveryState state);

// ============================================================================
// BLE Advertisement Data
// ============================================================================

/**
 * @brief SeaDrop-specific BLE advertisement format
 *
 * The advertisement packet contains just enough information for
 * discovery. Full device info is exchanged after initial contact.
 */
struct AdvertiseData {
  // SeaDrop service UUID (16 bytes)
  static constexpr uint8_t SERVICE_UUID[16] = {
      0x53, 0x65, 0x61, 0x44, 0x72, 0x6f, 0x70, 0x21, // "SeaDrop!"
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

  // Device ID (first 6 bytes for BLE, full 32 in scan response)
  std::array<Byte, 6> device_id_short;

  // Flags (1 byte)
  struct Flags {
    bool supports_wifi_direct : 1;
    bool supports_bluetooth_transfer : 1;
    bool supports_clipboard : 1;
    bool is_receiving : 1; // Currently open for transfers
    bool reserved : 4;
  } flags;

  // Protocol version (1 byte)
  uint8_t protocol_version = 1;

  // Device type (1 byte)
  DeviceType device_type = DeviceType::Unknown;

  // Serialize to bytes for BLE advertisement
  Bytes serialize() const;

  // Deserialize from BLE advertisement
  static std::optional<AdvertiseData> deserialize(const Bytes &data);
};

/**
 * @brief Full device info exchanged in scan response
 */
struct ScanResponseData {
  DeviceId device_id;          // Full 32-byte device ID
  std::string device_name;     // Device name (UTF-8, max 29 bytes)
  DevicePlatform platform;     // OS platform
  std::string seadrop_version; // Version string

  Bytes serialize() const;
  static std::optional<ScanResponseData> deserialize(const Bytes &data);
};

// ============================================================================
// Discovered Device
// ============================================================================

/**
 * @brief Information about a device discovered via BLE
 */
struct DiscoveredDevice {
  Device device; // Full device info

  // Discovery metadata
  int rssi_dbm = -100; // Signal strength
  std::chrono::steady_clock::time_point discovered_at;
  std::chrono::steady_clock::time_point last_seen;
  int seen_count = 0; // How many times we've seen this device

  // Platform-specific handle (for connection)
  std::string ble_address; // BLE MAC address (platform-specific format)

  /// Check if device was seen recently (within timeout)
  bool is_recent(std::chrono::seconds timeout = std::chrono::seconds(30)) const;
};

// ============================================================================
// Discovery Configuration
// ============================================================================

/**
 * @brief Configuration options for device discovery
 */
struct DiscoveryConfig {
  /// How long to scan before pausing (saves battery)
  std::chrono::seconds scan_duration{10};

  /// How long to pause between scan cycles
  std::chrono::seconds scan_pause{5};

  /// Continuous scanning (no pause, more battery)
  bool continuous_scan = false;

  /// Remove devices not seen for this long
  std::chrono::seconds device_timeout{60};

  /// Advertising interval (lower = faster discovery, more battery)
  std::chrono::milliseconds advertise_interval{100};

  /// Scan mode (low power vs low latency)
  enum class ScanMode {
    LowPower,  // Less frequent, saves battery
    Balanced,  // Default
    LowLatency // More frequent, faster discovery
  } scan_mode = ScanMode::Balanced;

  /// Only discover devices advertising SeaDrop service
  bool filter_seadrop_only = true;
};

// ============================================================================
// Discovery Manager
// ============================================================================

/**
 * @brief Manages BLE device discovery for SeaDrop
 *
 * The DiscoveryManager handles:
 * - BLE advertising (making ourselves visible)
 * - BLE scanning (finding other devices)
 * - Maintaining list of nearby devices
 * - RSSI updates for distance monitoring
 *
 * Example usage:
 * @code
 *   DiscoveryManager discovery;
 *   discovery.init(my_device_info);
 *
 *   discovery.on_device_discovered([](const DiscoveredDevice& device) {
 *       std::cout << "Found: " << device.device.name << std::endl;
 *   });
 *
 *   discovery.start();
 * @endcode
 */
class SEADROP_API DiscoveryManager {
public:
  DiscoveryManager();
  ~DiscoveryManager();

  // Non-copyable
  DiscoveryManager(const DiscoveryManager &) = delete;
  DiscoveryManager &operator=(const DiscoveryManager &) = delete;

  // ========================================================================
  // Initialization
  // ========================================================================

  /**
   * @brief Initialize the discovery manager
   * @param local_device Our device information to advertise
   * @param config Discovery configuration
   * @return Success or error (e.g., Bluetooth not available)
   */
  Result<void> init(const Device &local_device,
                    const DiscoveryConfig &config = {});

  /**
   * @brief Shutdown discovery and release resources
   */
  void shutdown();

  /**
   * @brief Check if discovery is initialized
   */
  bool is_initialized() const;

  // ========================================================================
  // Discovery Control
  // ========================================================================

  /**
   * @brief Start discovery (advertising + scanning)
   * @return Success or error
   */
  Result<void> start();

  /**
   * @brief Stop discovery
   */
  void stop();

  /**
   * @brief Start advertising only (don't scan)
   */
  Result<void> start_advertising();

  /**
   * @brief Stop advertising
   */
  void stop_advertising();

  /**
   * @brief Start scanning only (don't advertise)
   */
  Result<void> start_scanning();

  /**
   * @brief Stop scanning
   */
  void stop_scanning();

  /**
   * @brief Get current discovery state
   */
  DiscoveryState get_state() const;

  // ========================================================================
  // Device List
  // ========================================================================

  /**
   * @brief Get list of all discovered devices
   * @return Vector of discovered devices (may include stale entries)
   */
  std::vector<DiscoveredDevice> get_discovered_devices() const;

  /**
   * @brief Get list of recently-seen devices only
   * @param timeout Maximum age of devices to include
   * @return Vector of recently-seen devices
   */
  std::vector<DiscoveredDevice> get_nearby_devices(
      std::chrono::seconds timeout = std::chrono::seconds(30)) const;

  /**
   * @brief Get a specific device by ID
   * @return Device info or nullopt if not found/stale
   */
  std::optional<DiscoveredDevice> get_device(const DeviceId &id) const;

  /**
   * @brief Clear list of discovered devices
   */
  void clear_discovered_devices();

  // ========================================================================
  // Configuration
  // ========================================================================

  /**
   * @brief Update discovery configuration
   * @param config New configuration
   * @return Success or error
   */
  Result<void> set_config(const DiscoveryConfig &config);

  /**
   * @brief Get current configuration
   */
  DiscoveryConfig get_config() const;

  /**
   * @brief Update our advertised device info
   * @param device Updated device information
   */
  void set_local_device(const Device &device);

  /**
   * @brief Set whether we're currently receiving (shown in advertisement)
   */
  void set_receiving(bool is_receiving);

  // ========================================================================
  // Callbacks
  // ========================================================================

  /**
   * @brief Set callback for when a new device is discovered
   */
  void
  on_device_discovered(std::function<void(const DiscoveredDevice &)> callback);

  /**
   * @brief Set callback for when a device is no longer visible
   */
  void on_device_lost(std::function<void(const DeviceId &)> callback);

  /**
   * @brief Set callback for device updates (RSSI change, etc.)
   */
  void
  on_device_updated(std::function<void(const DiscoveredDevice &)> callback);

  /**
   * @brief Set callback for discovery state changes
   */
  void on_state_changed(std::function<void(DiscoveryState)> callback);

  /**
   * @brief Set callback for errors
   */
  void on_error(std::function<void(const Error &)> callback);

  // Allow platform implementations to see the opaque type
  class Impl;

private:
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Platform Helpers
// ============================================================================

/**
 * @brief Check if Bluetooth is available on this device
 */
SEADROP_API bool is_bluetooth_available();

/**
 * @brief Check if Bluetooth is currently enabled
 */
SEADROP_API bool is_bluetooth_enabled();

/**
 * @brief Request user to enable Bluetooth
 * @return True if request was shown, false if not possible
 */
SEADROP_API bool request_enable_bluetooth();

/**
 * @brief Check if we have necessary Bluetooth permissions
 */
SEADROP_API bool has_bluetooth_permission();

} // namespace seadrop

#endif // SEADROP_DISCOVERY_H

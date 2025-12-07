/**
 * @file connection.h
 * @brief WiFi Direct P2P connection management for SeaDrop
 *
 * SeaDrop uses WiFi Direct for high-speed peer-to-peer data transfer.
 * This module handles:
 * - WiFi Direct group formation
 * - P2P connection establishment
 * - TCP socket management over WiFi Direct
 * - Connection lifecycle and cleanup
 *
 * Connection Flow:
 *   1. Device A initiates connection (becomes Group Owner or Client)
 *   2. WiFi Direct group is formed
 *   3. Devices get IP addresses on the P2P network
 *   4. TCP connection established for data transfer
 *   5. Encryption handshake (using libsodium)
 *   6. Ready for file transfer
 */

#ifndef SEADROP_CONNECTION_H
#define SEADROP_CONNECTION_H

#include "device.h"
#include "error.h"
#include "platform.h"
#include "types.h"
#include <chrono>
#include <functional>
#include <memory>


namespace seadrop {

// ============================================================================
// Connection State
// ============================================================================

/**
 * @brief Current state of a P2P connection
 */
enum class ConnectionState : uint8_t {
  /// No connection
  Disconnected = 0,

  /// Starting WiFi Direct group formation
  Connecting = 1,

  /// WiFi Direct connected, establishing TCP
  Establishing = 2,

  /// TCP connected, performing encryption handshake
  Handshaking = 3,

  /// Fully connected and ready for transfer
  Connected = 4,

  /// Connection is being closed
  Disconnecting = 5,

  /// Connection lost unexpectedly
  Lost = 6,

  /// Error during connection
  Error = 255
};

/**
 * @brief Get human-readable name for connection state
 */
SEADROP_API const char *connection_state_name(ConnectionState state);

// ============================================================================
// WiFi Direct Role
// ============================================================================

/**
 * @brief Role in WiFi Direct group
 */
enum class P2pRole : uint8_t {
  /// Not in a group
  None = 0,

  /// Group Owner (acts as access point)
  GroupOwner = 1,

  /// Client (connects to group owner)
  Client = 2
};

// ============================================================================
// Connection Info
// ============================================================================

/**
 * @brief Information about the current connection
 */
struct ConnectionInfo {
  // Connection state
  ConnectionState state = ConnectionState::Disconnected;

  // Peer information
  DeviceId peer_id;
  std::string peer_name;

  // WiFi Direct info
  P2pRole role = P2pRole::None;
  std::string group_name; // WiFi Direct group SSID
  std::string local_ip;   // Our IP on P2P interface
  std::string peer_ip;    // Peer's IP on P2P interface
  uint16_t port = 0;      // TCP port for data

  // Signal quality
  int rssi_dbm = -100;
  int link_speed_mbps = 0;

  // Timing
  std::chrono::steady_clock::time_point connected_at;
  std::chrono::milliseconds connection_duration() const;

  // Statistics
  uint64_t bytes_sent = 0;
  uint64_t bytes_received = 0;

  // Error info (if state == Error)
  Error last_error;
};

// ============================================================================
// Connection Configuration
// ============================================================================

/**
 * @brief Configuration for P2P connections
 */
struct ConnectionConfig {
  /// Timeout for WiFi Direct group formation
  std::chrono::seconds formation_timeout{30};

  /// Timeout for TCP connection establishment
  std::chrono::seconds tcp_timeout{10};

  /// Timeout for encryption handshake
  std::chrono::seconds handshake_timeout{5};

  /// TCP port for data transfer (0 = auto-select)
  uint16_t tcp_port = 17530;

  /// Prefer to be Group Owner (faster connection for initiator)
  bool prefer_group_owner = true;

  /// Group Owner intent (0-15, higher = more likely to be GO)
  int go_intent = 7;

  /// Enable persistent group (faster reconnection to same device)
  bool persistent_group = true;

  /// Keep-alive interval (0 = disabled)
  std::chrono::seconds keepalive_interval{30};
};

// ============================================================================
// Connection Manager
// ============================================================================

/**
 * @brief Manages WiFi Direct connections for SeaDrop
 *
 * The ConnectionManager handles the entire P2P connection lifecycle,
 * from WiFi Direct group formation to encrypted TCP channel setup.
 *
 * Example usage:
 * @code
 *   ConnectionManager connection;
 *   connection.init(config);
 *
 *   connection.on_connected([](const ConnectionInfo& info) {
 *       std::cout << "Connected to: " << info.peer_name << std::endl;
 *   });
 *
 *   connection.connect(target_device);
 * @endcode
 */
class SEADROP_API ConnectionManager {
public:
  ConnectionManager();
  ~ConnectionManager();

  // Non-copyable
  ConnectionManager(const ConnectionManager &) = delete;
  ConnectionManager &operator=(const ConnectionManager &) = delete;

  // ========================================================================
  // Initialization
  // ========================================================================

  /**
   * @brief Initialize the connection manager
   * @param local_device Our device info (for handshake)
   * @param device_store Device store for trust lookups
   * @param config Connection configuration
   * @return Success or error
   */
  Result<void> init(const Device &local_device, DeviceStore *device_store,
                    const ConnectionConfig &config = {});

  /**
   * @brief Shutdown and release resources
   */
  void shutdown();

  /**
   * @brief Check if initialized
   */
  bool is_initialized() const;

  // ========================================================================
  // Connection Operations
  // ========================================================================

  /**
   * @brief Connect to a device via WiFi Direct
   * @param device Target device to connect to
   * @return Success (connection started) or error
   *
   * This starts the connection process asynchronously.
   * Use on_connected() callback to know when connection is ready.
   */
  Result<void> connect(const Device &device);

  /**
   * @brief Accept an incoming connection
   * @param device Device requesting connection
   * @return Success or error
   */
  Result<void> accept_connection(const Device &device);

  /**
   * @brief Reject an incoming connection
   * @param device Device requesting connection
   */
  void reject_connection(const Device &device);

  /**
   * @brief Disconnect from current peer
   */
  void disconnect();

  /**
   * @brief Cancel ongoing connection attempt
   */
  void cancel_connection();

  // ========================================================================
  // Connection State
  // ========================================================================

  /**
   * @brief Get current connection state
   */
  ConnectionState get_state() const;

  /**
   * @brief Get full connection info
   */
  ConnectionInfo get_connection_info() const;

  /**
   * @brief Check if currently connected
   */
  bool is_connected() const;

  /**
   * @brief Get connected peer's device ID
   * @return Peer ID or empty if not connected
   */
  std::optional<DeviceId> get_peer_id() const;

  // ========================================================================
  // Data Channel Access
  // ========================================================================

  /**
   * @brief Get the underlying socket for data transfer
   * @return Socket file descriptor or -1 if not connected
   *
   * The socket is already set up with encryption. Use the
   * TransferManager for actual file transfers rather than
   * raw socket operations.
   */
  int get_socket() const;

  /**
   * @brief Get current RSSI reading from WiFi Direct connection
   * @return RSSI in dBm
   */
  int get_rssi() const;

  // ========================================================================
  // Configuration
  // ========================================================================

  /**
   * @brief Update connection configuration
   */
  Result<void> set_config(const ConnectionConfig &config);

  /**
   * @brief Get current configuration
   */
  ConnectionConfig get_config() const;

  // ========================================================================
  // Callbacks
  // ========================================================================

  /**
   * @brief Set callback for state changes
   */
  void on_state_changed(std::function<void(ConnectionState)> callback);

  /**
   * @brief Set callback for successful connection
   */
  void on_connected(std::function<void(const ConnectionInfo &)> callback);

  /**
   * @brief Set callback for disconnection
   */
  void on_disconnected(
      std::function<void(const DeviceId &, const std::string &reason)>
          callback);

  /**
   * @brief Set callback for incoming connection requests
   */
  void on_connection_request(std::function<void(const Device &)> callback);

  /**
   * @brief Set callback for connection errors
   */
  void on_error(std::function<void(const Error &)> callback);

  /**
   * @brief Set callback for RSSI updates
   */
  void on_rssi_updated(std::function<void(int rssi_dbm)> callback);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Platform Helpers
// ============================================================================

/**
 * @brief Check if WiFi Direct is available on this device
 */
SEADROP_API bool is_wifi_direct_available();

/**
 * @brief Check if WiFi is currently enabled
 */
SEADROP_API bool is_wifi_enabled();

/**
 * @brief Request user to enable WiFi
 */
SEADROP_API bool request_enable_wifi();

/**
 * @brief Check if we have necessary WiFi Direct permissions
 */
SEADROP_API bool has_wifi_direct_permission();

/**
 * @brief Get the P2P interface name (e.g., "p2p0", "p2p-wlan0-0")
 */
SEADROP_API std::string get_p2p_interface();

} // namespace seadrop

#endif // SEADROP_CONNECTION_H

/**
 * @file wpa_supplicant.h
 * @brief wpa_supplicant WiFi Direct types and internal declarations
 *
 * Internal types for WiFi Direct P2P via wpa_supplicant.
 */

#ifndef SEADROP_PLATFORM_LINUX_WPA_SUPPLICANT_H
#define SEADROP_PLATFORM_LINUX_WPA_SUPPLICANT_H

#include "dbus_helpers.h"
#include <atomic>
#include <string>
#include <thread>

namespace seadrop {
namespace platform {

// wpa_supplicant D-Bus constants
constexpr const char *WPA_SERVICE = "fi.w1.wpa_supplicant1";
constexpr const char *WPA_PATH = "/fi/w1/wpa_supplicant1";
constexpr const char *WPA_IFACE = "fi.w1.wpa_supplicant1";
constexpr const char *WPA_IFACE_IFACE = "fi.w1.wpa_supplicant1.Interface";
constexpr const char *WPA_P2P_IFACE =
    "fi.w1.wpa_supplicant1.Interface.P2PDevice";
constexpr const char *WPA_GROUP_IFACE = "fi.w1.wpa_supplicant1.Group";

/**
 * @brief P2P Group role
 */
enum class P2PGroupRole {
  Unknown,
  GroupOwner, // GO - acts as access point
  Client      // P2P client
};

/**
 * @brief P2P connection state
 */
enum class P2PState {
  Idle,
  Discovering,
  Connecting,
  Connected,
  GroupFormed,
  Error
};

/**
 * @brief WiFi Direct peer information
 */
struct P2PPeer {
  std::string object_path;    // wpa_supplicant peer object path
  std::string device_address; // P2P device address
  std::string device_name;    // Device name
  std::string device_type;    // Device type (e.g., "smartphone")
  int go_intent = 0;          // Group owner intent (0-15)
};

/**
 * @brief WiFi Direct group information
 */
struct P2PGroup {
  std::string object_path;    // Group interface path
  std::string interface_name; // Network interface (e.g., "p2p-wlan0-0")
  P2PGroupRole role = P2PGroupRole::Unknown;
  std::string ssid;       // Group SSID
  std::string passphrase; // Group passphrase (if GO)
  std::string local_ip;   // Our IP address in the group
  std::string peer_ip;    // Peer's IP address
};

/**
 * @brief wpa_supplicant platform context
 *
 * Holds all WiFi Direct related state.
 */
struct WpaSupplicantContext {
  DBusConnectionWrapper conn; // System D-Bus connection
  std::string interface_path; // Primary WiFi interface object path
  std::string interface_name; // Interface name (e.g., "wlan0")
  P2PState state = P2PState::Idle;
  P2PGroup current_group; // Current P2P group
  std::atomic<bool> stop_requested{false};
  std::thread event_thread; // D-Bus event processing thread
};

/**
 * @brief Find the primary WiFi interface
 */
Result<std::string> find_wifi_interface(DBusConnection *conn);

/**
 * @brief Get interface's P2P device address
 */
Result<std::string> get_p2p_device_address(DBusConnection *conn,
                                           const std::string &iface_path);

/**
 * @brief Start P2P device (required before P2P operations)
 */
Result<void> p2p_start(DBusConnection *conn, const std::string &iface_path);

/**
 * @brief Stop P2P device
 */
Result<void> p2p_stop(DBusConnection *conn, const std::string &iface_path);

/**
 * @brief Start P2P discovery (find peers)
 */
Result<void> p2p_find(DBusConnection *conn, const std::string &iface_path,
                      int timeout_seconds = 30);

/**
 * @brief Stop P2P discovery
 */
Result<void> p2p_stop_find(DBusConnection *conn, const std::string &iface_path);

/**
 * @brief Connect to a P2P peer
 */
Result<void> p2p_connect(DBusConnection *conn, const std::string &iface_path,
                         const std::string &peer_address, int go_intent = 7);

/**
 * @brief Disconnect from P2P group
 */
Result<void> p2p_disconnect(DBusConnection *conn,
                            const std::string &iface_path);

/**
 * @brief Get assigned IP address after group formation
 */
Result<std::string> get_p2p_ip_address(const std::string &interface_name);

} // namespace platform
} // namespace seadrop

#endif // SEADROP_PLATFORM_LINUX_WPA_SUPPLICANT_H

/**
 * @file wpa_supplicant.cpp
 * @brief wpa_supplicant WiFi Direct implementation
 *
 * Implements WiFi Direct P2P connections via wpa_supplicant D-Bus interface.
 */

#include "wpa_supplicant.h"
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <ifaddrs.h>
#include <net/if.h>
#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>

namespace seadrop {
namespace platform {

// ============================================================================
// Interface Discovery
// ============================================================================

Result<std::string> find_wifi_interface(DBusConnection *conn) {
  // Call GetInterface to get the WiFi interface
  // First, try to find wlan0 or similar

  // Get list of interfaces from wpa_supplicant
  auto reply =
      call_method(conn, WPA_SERVICE, WPA_PATH, WPA_IFACE, "GetInterface");

  // If direct call fails, try to create interface
  // For now, use a common approach: assume wlan0 exists

  // Try to get interface by name
  DBusMessageWrapper msg(dbus_message_new_method_call(
      WPA_SERVICE, WPA_PATH, WPA_IFACE, "GetInterface"));

  if (!msg) {
    return Error(ErrorCode::PlatformError, "Failed to create D-Bus message");
  }

  const char *ifname = "wlan0";
  dbus_message_append_args(msg.get(), DBUS_TYPE_STRING, &ifname,
                           DBUS_TYPE_INVALID);

  DBusErrorWrapper error;
  DBusMessage *reply_msg = dbus_connection_send_with_reply_and_block(
      conn, msg.get(), 5000, error.get());

  if (!reply_msg || error.is_set()) {
    // Try other common interface names
    const char *alt_names[] = {"wlp2s0", "wlp3s0", "wlan1", nullptr};

    for (int i = 0; alt_names[i] != nullptr; i++) {
      DBusMessageWrapper alt_msg(dbus_message_new_method_call(
          WPA_SERVICE, WPA_PATH, WPA_IFACE, "GetInterface"));

      if (!alt_msg)
        continue;

      dbus_message_append_args(alt_msg.get(), DBUS_TYPE_STRING, &alt_names[i],
                               DBUS_TYPE_INVALID);

      DBusErrorWrapper alt_error;
      DBusMessage *alt_reply = dbus_connection_send_with_reply_and_block(
          conn, alt_msg.get(), 5000, alt_error.get());

      if (alt_reply && !alt_error.is_set()) {
        // Parse object path from reply
        const char *path = nullptr;
        if (dbus_message_get_args(alt_reply, nullptr, DBUS_TYPE_OBJECT_PATH,
                                  &path, DBUS_TYPE_INVALID)) {
          std::string result(path);
          dbus_message_unref(alt_reply);
          return result;
        }
        dbus_message_unref(alt_reply);
      }
    }

    return Error(ErrorCode::NotFound, "No WiFi interface found");
  }

  // Parse object path from reply
  const char *path = nullptr;
  if (!dbus_message_get_args(reply_msg, nullptr, DBUS_TYPE_OBJECT_PATH, &path,
                             DBUS_TYPE_INVALID)) {
    dbus_message_unref(reply_msg);
    return Error(ErrorCode::PlatformError, "Failed to parse interface path");
  }

  std::string result(path);
  dbus_message_unref(reply_msg);
  return result;
}

Result<std::string> get_p2p_device_address(DBusConnection *conn,
                                           const std::string &iface_path) {
  return get_string_property(conn, WPA_SERVICE, iface_path.c_str(),
                             WPA_P2P_IFACE, "P2PDeviceAddress");
}

// ============================================================================
// P2P Operations
// ============================================================================

Result<void> p2p_start(DBusConnection *conn, const std::string &iface_path) {
  SEADROP_UNUSED(conn);
  SEADROP_UNUSED(iface_path);

  // P2P is typically auto-started when the interface is created
  // Some systems may require explicit initialization

  return Result<void>::ok();
}

Result<void> p2p_stop(DBusConnection *conn, const std::string &iface_path) {
  // Ensure we're not in a group
  p2p_disconnect(conn, iface_path);

  // Stop any ongoing find
  p2p_stop_find(conn, iface_path);

  return Result<void>::ok();
}

Result<void> p2p_find(DBusConnection *conn, const std::string &iface_path,
                      int timeout_seconds) {
  DBusMessageWrapper msg(dbus_message_new_method_call(
      WPA_SERVICE, iface_path.c_str(), WPA_P2P_IFACE, "Find"));

  if (!msg) {
    return Error(ErrorCode::PlatformError, "Failed to create D-Bus message");
  }

  // Add arguments dict
  DBusMessageIter iter, dict_iter;
  dbus_message_iter_init_append(msg.get(), &iter);
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);

  // Add Timeout entry
  if (timeout_seconds > 0) {
    DBusMessageIter entry_iter, variant_iter;
    dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                     &entry_iter);

    const char *key = "Timeout";
    dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);

    dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "i",
                                     &variant_iter);
    dbus_int32_t timeout = timeout_seconds;
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_INT32, &timeout);
    dbus_message_iter_close_container(&entry_iter, &variant_iter);

    dbus_message_iter_close_container(&dict_iter, &entry_iter);
  }

  dbus_message_iter_close_container(&iter, &dict_iter);

  DBusErrorWrapper error;
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(
      conn, msg.get(), 5000, error.get());

  if (error.is_set()) {
    return error.to_error();
  }

  if (reply) {
    dbus_message_unref(reply);
  }

  return Result<void>::ok();
}

Result<void> p2p_stop_find(DBusConnection *conn,
                           const std::string &iface_path) {
  auto result = call_method(conn, WPA_SERVICE, iface_path.c_str(),
                            WPA_P2P_IFACE, "StopFind");

  if (result.is_error()) {
    // Not finding is not an error
    return Result<void>::ok();
  }

  return Result<void>::ok();
}

Result<void> p2p_connect(DBusConnection *conn, const std::string &iface_path,
                         const std::string &peer_address, int go_intent) {
  DBusMessageWrapper msg(dbus_message_new_method_call(
      WPA_SERVICE, iface_path.c_str(), WPA_P2P_IFACE, "Connect"));

  if (!msg) {
    return Error(ErrorCode::PlatformError, "Failed to create D-Bus message");
  }

  // Build args dict
  DBusMessageIter iter, dict_iter;
  dbus_message_iter_init_append(msg.get(), &iter);
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);

  // peer - the peer to connect to
  {
    DBusMessageIter entry_iter, variant_iter;
    dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                     &entry_iter);
    const char *key = "peer";
    dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s",
                                     &variant_iter);
    const char *peer = peer_address.c_str();
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &peer);
    dbus_message_iter_close_container(&entry_iter, &variant_iter);
    dbus_message_iter_close_container(&dict_iter, &entry_iter);
  }

  // wps_method - use PBC (push button config)
  {
    DBusMessageIter entry_iter, variant_iter;
    dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                     &entry_iter);
    const char *key = "wps_method";
    dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s",
                                     &variant_iter);
    const char *method = "pbc";
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &method);
    dbus_message_iter_close_container(&entry_iter, &variant_iter);
    dbus_message_iter_close_container(&dict_iter, &entry_iter);
  }

  // go_intent
  {
    DBusMessageIter entry_iter, variant_iter;
    dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                     &entry_iter);
    const char *key = "go_intent";
    dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "i",
                                     &variant_iter);
    dbus_int32_t intent = go_intent;
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_INT32, &intent);
    dbus_message_iter_close_container(&entry_iter, &variant_iter);
    dbus_message_iter_close_container(&dict_iter, &entry_iter);
  }

  dbus_message_iter_close_container(&iter, &dict_iter);

  DBusErrorWrapper error;
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(
      conn, msg.get(), 30000, error.get()); // 30s timeout for connection

  if (error.is_set()) {
    return error.to_error();
  }

  if (reply) {
    dbus_message_unref(reply);
  }

  return Result<void>::ok();
}

Result<void> p2p_disconnect(DBusConnection *conn,
                            const std::string &iface_path) {
  auto result = call_method(conn, WPA_SERVICE, iface_path.c_str(),
                            WPA_P2P_IFACE, "Disconnect");

  if (result.is_error()) {
    // Not connected is not an error
    return Result<void>::ok();
  }

  return Result<void>::ok();
}

// ============================================================================
// IP Address Retrieval
// ============================================================================

Result<std::string> get_p2p_ip_address(const std::string &interface_name) {
  struct ifaddrs *ifaddr = nullptr;

  if (getifaddrs(&ifaddr) == -1) {
    return Error(ErrorCode::PlatformError, "Failed to get interface addresses");
  }

  std::string result;

  for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) {
      continue;
    }

    // Check if this is our P2P interface
    if (std::strncmp(ifa->ifa_name, interface_name.c_str(),
                     interface_name.length()) != 0) {
      continue;
    }

    // Check for IPv4
    if (ifa->ifa_addr->sa_family == AF_INET) {
      char addr_buf[INET_ADDRSTRLEN];
      struct sockaddr_in *sin =
          reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);

      if (inet_ntop(AF_INET, &sin->sin_addr, addr_buf, sizeof(addr_buf))) {
        result = addr_buf;
        break;
      }
    }
  }

  freeifaddrs(ifaddr);

  if (result.empty()) {
    return Error(ErrorCode::NotFound, "No IP address found for interface");
  }

  return result;
}

} // namespace platform
} // namespace seadrop

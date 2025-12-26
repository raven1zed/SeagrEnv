/**
 * @file bluez_ble.cpp
 * @brief BlueZ BLE discovery implementation
 *
 * Implements BLE device discovery using BlueZ via D-Bus.
 * This implements the platform hooks declared in discovery_pimpl.h.
 */

#include "bluez_ble.h"
#include "../../discovery_pimpl.h"
#include <algorithm>
#include <cstring>

namespace seadrop {

// ============================================================================
// Platform Hook Implementations
// ============================================================================

Result<void> platform_discovery_start(DiscoveryManager::Impl *impl) {
  using namespace platform;

  // Initialize BlueZ context if needed
  // For now, just set state to active
  impl->set_state(DiscoveryState::Active);

  // Start both advertising and scanning
  auto adv_result = platform_discovery_start_advertising(impl);
  if (adv_result.is_error()) {
    // Log but don't fail - scanning can work without advertising
  }

  auto scan_result = platform_discovery_start_scanning(impl);
  if (scan_result.is_error()) {
    return scan_result;
  }

  return Result<void>::ok();
}

void platform_discovery_stop(DiscoveryManager::Impl *impl) {
  platform_discovery_stop_scanning(impl);
  platform_discovery_stop_advertising(impl);
}

Result<void>
platform_discovery_start_advertising(DiscoveryManager::Impl *impl) {
  SEADROP_UNUSED(impl);

  // Get system bus connection
  auto conn_result = platform::get_system_bus();
  if (conn_result.is_error()) {
    return conn_result.error();
  }

  // Find adapter
  auto adapter_result = platform::find_adapter(conn_result.value().get());
  if (adapter_result.is_error()) {
    return adapter_result.error();
  }

  const auto &adapter = adapter_result.value();

  // Ensure adapter is powered
  auto power_result = platform::set_adapter_powered(conn_result.value().get(),
                                                    adapter.object_path, true);
  if (power_result.is_error()) {
    return power_result.error();
  }

  // TODO: Register BLE advertisement with SeaDrop service data
  // For now, we'll use legacy discoverable mode as a placeholder

  return Result<void>::ok();
}

void platform_discovery_stop_advertising(DiscoveryManager::Impl *impl) {
  SEADROP_UNUSED(impl);
  // TODO: Unregister BLE advertisement
}

Result<void> platform_discovery_start_scanning(DiscoveryManager::Impl *impl) {
  SEADROP_UNUSED(impl);

  auto conn_result = platform::get_system_bus();
  if (conn_result.is_error()) {
    return conn_result.error();
  }

  auto adapter_result = platform::find_adapter(conn_result.value().get());
  if (adapter_result.is_error()) {
    return adapter_result.error();
  }

  auto scan_result = platform::start_discovery(
      conn_result.value().get(), adapter_result.value().object_path);
  if (scan_result.is_error()) {
    return scan_result.error();
  }

  impl->set_state(DiscoveryState::Scanning);
  return Result<void>::ok();
}

void platform_discovery_stop_scanning(DiscoveryManager::Impl *impl) {
  auto conn_result = platform::get_system_bus();
  if (conn_result.is_error()) {
    return;
  }

  auto adapter_result = platform::find_adapter(conn_result.value().get());
  if (adapter_result.is_error()) {
    return;
  }

  platform::stop_discovery(conn_result.value().get(),
                           adapter_result.value().object_path);

  if (impl->state == DiscoveryState::Scanning) {
    impl->set_state(DiscoveryState::Idle);
  }
}

namespace platform {

// ============================================================================
// BlueZ Adapter Discovery
// ============================================================================

Result<BlueZAdapter> find_adapter(DBusConnection *conn) {
  // Call org.freedesktop.DBus.ObjectManager.GetManagedObjects on BlueZ
  auto reply =
      call_method(conn, BLUEZ_SERVICE, "/",
                  "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");

  if (reply.is_error()) {
    return reply.error();
  }

  DBusMessageIter iter, dict_iter;
  if (!dbus_message_iter_init(reply.value().get(), &iter)) {
    return Error(ErrorCode::PlatformError, "Empty reply from BlueZ");
  }

  // Iterate through managed objects to find adapter
  if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
    return Error(ErrorCode::PlatformError, "Unexpected reply format");
  }

  dbus_message_iter_recurse(&iter, &dict_iter);

  while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY) {
    DBusMessageIter entry_iter, iface_dict_iter;
    dbus_message_iter_recurse(&dict_iter, &entry_iter);

    // Get object path
    const char *path = nullptr;
    dbus_message_iter_get_basic(&entry_iter, &path);
    dbus_message_iter_next(&entry_iter);

    if (!path) {
      dbus_message_iter_next(&dict_iter);
      continue;
    }

    // Check interfaces for Adapter1
    if (dbus_message_iter_get_arg_type(&entry_iter) == DBUS_TYPE_ARRAY) {
      dbus_message_iter_recurse(&entry_iter, &iface_dict_iter);

      while (dbus_message_iter_get_arg_type(&iface_dict_iter) ==
             DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter iface_entry;
        dbus_message_iter_recurse(&iface_dict_iter, &iface_entry);

        const char *iface = nullptr;
        dbus_message_iter_get_basic(&iface_entry, &iface);

        if (iface && std::strcmp(iface, BLUEZ_ADAPTER_IFACE) == 0) {
          // Found an adapter!
          BlueZAdapter adapter;
          adapter.object_path = path;

          // Get adapter properties
          auto addr = get_string_property(conn, BLUEZ_SERVICE, path,
                                          BLUEZ_ADAPTER_IFACE, "Address");
          if (addr.is_ok()) {
            adapter.address = addr.value();
          }

          auto name = get_string_property(conn, BLUEZ_SERVICE, path,
                                          BLUEZ_ADAPTER_IFACE, "Name");
          if (name.is_ok()) {
            adapter.name = name.value();
          }

          return adapter;
        }

        dbus_message_iter_next(&iface_dict_iter);
      }
    }

    dbus_message_iter_next(&dict_iter);
  }

  return Error(ErrorCode::NotFound, "No Bluetooth adapter found");
}

// ============================================================================
// Adapter Control
// ============================================================================

Result<void> set_adapter_powered(DBusConnection *conn,
                                 const std::string &adapter_path,
                                 bool powered) {
  dbus_bool_t value = powered ? TRUE : FALSE;
  return set_property(conn, BLUEZ_SERVICE, adapter_path.c_str(),
                      BLUEZ_ADAPTER_IFACE, "Powered", DBUS_TYPE_BOOLEAN,
                      &value);
}

Result<void> start_discovery(DBusConnection *conn,
                             const std::string &adapter_path) {
  // Set discovery filter for BLE devices
  // TODO: Set filter for SeaDrop service UUID

  // Call StartDiscovery
  auto result = call_method(conn, BLUEZ_SERVICE, adapter_path.c_str(),
                            BLUEZ_ADAPTER_IFACE, "StartDiscovery");

  if (result.is_error()) {
    // Check if already discovering (not an error)
    if (result.error().message.find("Already") != std::string::npos ||
        result.error().message.find("InProgress") != std::string::npos) {
      return Result<void>::ok();
    }
    return result.error();
  }

  return Result<void>::ok();
}

Result<void> stop_discovery(DBusConnection *conn,
                            const std::string &adapter_path) {
  auto result = call_method(conn, BLUEZ_SERVICE, adapter_path.c_str(),
                            BLUEZ_ADAPTER_IFACE, "StopDiscovery");

  if (result.is_error()) {
    // Not discovering is not an error
    if (result.error().message.find("Not") != std::string::npos) {
      return Result<void>::ok();
    }
    return result.error();
  }

  return Result<void>::ok();
}

// ============================================================================
// BLE Advertisement (Placeholder)
// ============================================================================

Result<std::string> register_advertisement(DBusConnection *conn,
                                           const std::string &adapter_path,
                                           const Bytes &service_data) {
  SEADROP_UNUSED(conn);
  SEADROP_UNUSED(adapter_path);
  SEADROP_UNUSED(service_data);

  // TODO: Implement proper LEAdvertisement1 registration
  // This requires:
  // 1. Export an object implementing LEAdvertisement1 interface
  // 2. Call RegisterAdvertisement on the adapter's LEAdvertisingManager1

  return Error(ErrorCode::NotSupported, "BLE advertising not yet implemented");
}

Result<void> unregister_advertisement(DBusConnection *conn,
                                      const std::string &adapter_path,
                                      const std::string &adv_path) {
  SEADROP_UNUSED(conn);
  SEADROP_UNUSED(adapter_path);
  SEADROP_UNUSED(adv_path);

  // TODO: Call UnregisterAdvertisement
  return Result<void>::ok();
}

} // namespace platform
} // namespace seadrop

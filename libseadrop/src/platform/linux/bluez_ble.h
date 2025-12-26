/**
 * @file bluez_ble.h
 * @brief BlueZ BLE types and internal declarations
 *
 * Internal types for the BlueZ BLE implementation.
 */

#ifndef SEADROP_PLATFORM_LINUX_BLUEZ_BLE_H
#define SEADROP_PLATFORM_LINUX_BLUEZ_BLE_H

#include "dbus_helpers.h"
#include <atomic>
#include <string>
#include <thread>

namespace seadrop {
namespace platform {

// SeaDrop BLE Service UUID (128-bit custom UUID)
// Format: XXXXXXXX-0000-1000-8000-00805F9B34FB (Base UUID)
// SeaDrop uses: 5EA0D0D0-0001-1000-8000-00805F9B34FB
constexpr const char *SEADROP_SERVICE_UUID =
    "5ea0d0d0-0001-1000-8000-00805f9b34fb";

// BlueZ D-Bus constants
constexpr const char *BLUEZ_SERVICE = "org.bluez";
constexpr const char *BLUEZ_ADAPTER_IFACE = "org.bluez.Adapter1";
constexpr const char *BLUEZ_DEVICE_IFACE = "org.bluez.Device1";
constexpr const char *BLUEZ_LE_ADV_MANAGER_IFACE =
    "org.bluez.LEAdvertisingManager1";
constexpr const char *BLUEZ_LE_ADV_IFACE = "org.bluez.LEAdvertisement1";
constexpr const char *BLUEZ_GATT_MANAGER_IFACE = "org.bluez.GattManager1";

/**
 * @brief BlueZ adapter state
 */
struct BlueZAdapter {
  std::string object_path;   // e.g., "/org/bluez/hci0"
  std::string address;       // MAC address
  std::string name;          // Adapter name
  bool powered = false;      // Is adapter powered on
  bool discovering = false;  // Is scanning
  bool discoverable = false; // Is advertising (legacy)
};

/**
 * @brief BlueZ platform context
 *
 * Holds all BlueZ-related state for the Linux platform.
 */
struct BlueZContext {
  DBusConnectionWrapper conn;              // System D-Bus connection
  BlueZAdapter adapter;                    // Primary Bluetooth adapter
  std::string advertisement_path;          // Our registered advertisement
  std::atomic<bool> scanning{false};       // Are we currently scanning
  std::atomic<bool> advertising{false};    // Are we currently advertising
  std::thread scan_thread;                 // Background scanning thread
  std::atomic<bool> stop_requested{false}; // Signal to stop threads
};

/**
 * @brief Find the first available BlueZ adapter
 */
Result<BlueZAdapter> find_adapter(DBusConnection *conn);

/**
 * @brief Power on/off the adapter
 */
Result<void> set_adapter_powered(DBusConnection *conn,
                                 const std::string &adapter_path, bool powered);

/**
 * @brief Start BLE discovery (scanning)
 */
Result<void> start_discovery(DBusConnection *conn,
                             const std::string &adapter_path);

/**
 * @brief Stop BLE discovery
 */
Result<void> stop_discovery(DBusConnection *conn,
                            const std::string &adapter_path);

/**
 * @brief Register a BLE advertisement
 */
Result<std::string> register_advertisement(DBusConnection *conn,
                                           const std::string &adapter_path,
                                           const Bytes &service_data);

/**
 * @brief Unregister a BLE advertisement
 */
Result<void> unregister_advertisement(DBusConnection *conn,
                                      const std::string &adapter_path,
                                      const std::string &adv_path);

} // namespace platform
} // namespace seadrop

#endif // SEADROP_PLATFORM_LINUX_BLUEZ_BLE_H

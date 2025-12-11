/**
 * @file bluetooth_win.cpp
 * @brief Windows Bluetooth implementation (Simulation/Mock for now)
 */

#include "seadrop/discovery.h"
#include "seadrop/platform.h"
#include "../discovery_pimpl.h"

#ifdef SEADROP_PLATFORM_WINDOWS

#include <thread>
#include <mutex>

namespace seadrop {

// ============================================================================
// Platform Helper Implementations
// ============================================================================

bool is_bluetooth_available() {
  // TODO: Check if Bluetooth adapter is present on Windows
  return true;
}

bool is_bluetooth_enabled() {
  // TODO: Check if Bluetooth is enabled on Windows
  return true;
}

bool request_enable_bluetooth() {
  // TODO: Show Windows Bluetooth settings
  return false;
}

bool has_bluetooth_permission() { return true; }

// ============================================================================
// DiscoveryManager Platform Hooks
// ============================================================================

// Simulation Thread for UI Testing
// TODO: Replace with real
// Windows.Devices.Bluetooth.Advertisement.BluetoothLEAdvertisementWatcher

void platform_discovery_start(DiscoveryManager::Impl *impl) {
  if (!impl)
    return;
  impl->set_state(DiscoveryState::Active);

  // Mock Device Discovery for UI Testing
  std::thread([impl]() {
    // Simulate a delay
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Mock Device 1: Android Phone
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (impl->state != DiscoveryState::Active)
        return;

      DiscoveredDevice dd;
      dd.device.name = "Pixel 6 Pro";
      // Random ID
      dd.device.id.data.fill(0xAA);
      dd.device.platform = DevicePlatform::Android;
      dd.device.type = DeviceType::Phone;
      dd.rssi_dbm = -45;
      dd.last_seen = std::chrono::steady_clock::now();
      dd.discovered_at = std::chrono::steady_clock::now();

      // Add to list
      std::string key = dd.device.id.to_hex();
      bool is_new = impl->devices.find(key) == impl->devices.end();
      impl->devices[key] = dd;

      // Notify
      if (is_new && impl->discovered_cb) {
        // Must call callback outside lock if possible, but for mock is okay?
        // Better to be safe, but here Impl is locked.
        // In real impl, we should use a dispatcher or careful locking.
        impl->discovered_cb(dd);
      } else if (!is_new && impl->updated_cb) {
        impl->updated_cb(dd);
      }
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Mock Device 2: Linux Laptop
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (impl->state != DiscoveryState::Active)
        return;

      DiscoveredDevice dd;
      dd.device.name = "Ubuntu Desktop";
      dd.device.id.data.fill(0xBB);
      dd.device.platform = DevicePlatform::Linux;
      dd.device.type = DeviceType::Desktop;
      dd.rssi_dbm = -60;
      dd.last_seen = std::chrono::steady_clock::now();
      dd.discovered_at = std::chrono::steady_clock::now();

      std::string key = dd.device.id.to_hex();
      bool is_new = impl->devices.find(key) == impl->devices.end();
      impl->devices[key] = dd;

      if (is_new && impl->discovered_cb) {
        impl->discovered_cb(dd);
      }
    }
  }).detach();
}

void platform_discovery_stop(DiscoveryManager::Impl *impl) {
  if (!impl)
    return;
  impl->set_state(DiscoveryState::Idle);
  // In real impl, stop the watcher here
}

Result<void>
platform_discovery_start_advertising(DiscoveryManager::Impl *impl) {
  if (!impl)
    return Error(ErrorCode::InvalidState, "No impl");
  impl->set_state(DiscoveryState::Advertising);
  return Result<void>::ok();
}

void platform_discovery_stop_advertising(DiscoveryManager::Impl *impl) {
  if (!impl)
    return;
  impl->set_state(DiscoveryState::Idle);
}

Result<void> platform_discovery_start_scanning(DiscoveryManager::Impl *impl) {
  // Re-use mock start
  platform_discovery_start(impl);
  return Result<void>::ok();
}

void platform_discovery_stop_scanning(DiscoveryManager::Impl *impl) {
  platform_discovery_stop(impl);
}

} // namespace seadrop

#endif // SEADROP_PLATFORM_WINDOWS

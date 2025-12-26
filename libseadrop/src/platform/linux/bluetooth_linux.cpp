// Stub implementation for Linux BLE/Bluetooth discovery
#include "../../discovery_pimpl.h"

namespace seadrop {

Result<void> platform_discovery_start(DiscoveryManager::Impl *impl) {
  // Stub: platform not fully implemented yet
  return Result<void>();
}

void platform_discovery_stop(DiscoveryManager::Impl *impl) {
  // Stub: platform not fully implemented yet
}

Result<void> platform_discovery_start_advertising(DiscoveryManager::Impl *impl) {
  // Stub: platform not fully implemented yet
  return Result<void>();
}

void platform_discovery_stop_advertising(DiscoveryManager::Impl *impl) {
  // Stub: platform not fully implemented yet
}

Result<void> platform_discovery_start_scanning(DiscoveryManager::Impl *impl) {
  // Stub: platform not fully implemented yet
  return Result<void>();
}

void platform_discovery_stop_scanning(DiscoveryManager::Impl *impl) {
  // Stub: platform not fully implemented yet
}

} // namespace seadrop

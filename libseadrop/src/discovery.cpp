/**
 * @file discovery.cpp
 * @brief BLE device discovery implementation
 */

#include "seadrop/discovery.h"
#include "discovery_pimpl.h"
#include <chrono>

namespace seadrop {

// ============================================================================
// Discovery State Names
// ============================================================================

const char *discovery_state_name(DiscoveryState state) {
  switch (state) {
  case DiscoveryState::Uninitialized:
    return "Uninitialized";
  case DiscoveryState::Idle:
    return "Idle";
  case DiscoveryState::Advertising:
    return "Advertising";
  case DiscoveryState::Scanning:
    return "Scanning";
  case DiscoveryState::Active:
    return "Active";
  case DiscoveryState::Error:
    return "Error";
  default:
    return "Unknown";
  }
}

// ============================================================================
// AdvertiseData
// ============================================================================

Bytes AdvertiseData::serialize() const {
  // TODO: Implement BLE advertisement packet serialization
  return {};
}

std::optional<AdvertiseData> AdvertiseData::deserialize(const Bytes &data) {
  // TODO: Implement BLE advertisement packet deserialization
  SEADROP_UNUSED(data);
  return std::nullopt;
}

Bytes ScanResponseData::serialize() const {
  // TODO: Implement scan response serialization
  return {};
}

std::optional<ScanResponseData>
ScanResponseData::deserialize(const Bytes &data) {
  // TODO: Implement scan response deserialization
  SEADROP_UNUSED(data);
  return std::nullopt;
}

// ============================================================================
// DiscoveredDevice
// ============================================================================

bool DiscoveredDevice::is_recent(std::chrono::seconds timeout) const {
  auto now = std::chrono::steady_clock::now();
  return (now - last_seen) <= timeout;
}

// ============================================================================
// DiscoveryManager Implementation
// ============================================================================

DiscoveryManager::DiscoveryManager() : impl_(std::make_unique<Impl>()) {}
DiscoveryManager::~DiscoveryManager() { shutdown(); }

Result<void> DiscoveryManager::init(const Device &local_device,
                                    const DiscoveryConfig &config) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  impl_->local_device = local_device;
  impl_->config = config;
  impl_->set_state(DiscoveryState::Idle);

  return Result<void>::ok();
}

void DiscoveryManager::shutdown() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  stop();
  impl_->devices.clear();
  impl_->set_state(DiscoveryState::Uninitialized);
}

bool DiscoveryManager::is_initialized() const {
  return impl_->state != DiscoveryState::Uninitialized;
}

Result<void> DiscoveryManager::start() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return platform_discovery_start(impl_.get());
}

void DiscoveryManager::stop() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  platform_discovery_stop(impl_.get());

  if (impl_->state != DiscoveryState::Uninitialized) {
    impl_->set_state(DiscoveryState::Idle);
  }
}

Result<void> DiscoveryManager::start_advertising() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return platform_discovery_start_advertising(impl_.get());
}

void DiscoveryManager::stop_advertising() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  platform_discovery_stop_advertising(impl_.get());
}

Result<void> DiscoveryManager::start_scanning() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return platform_discovery_start_scanning(impl_.get());
}

void DiscoveryManager::stop_scanning() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  platform_discovery_stop_scanning(impl_.get());
}

DiscoveryState DiscoveryManager::get_state() const { return impl_->state; }

std::vector<DiscoveredDevice> DiscoveryManager::get_discovered_devices() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  std::vector<DiscoveredDevice> result;
  result.reserve(impl_->devices.size());
  for (const auto &[key, device] : impl_->devices) {
    result.push_back(device);
  }
  return result;
}

std::vector<DiscoveredDevice>
DiscoveryManager::get_nearby_devices(std::chrono::seconds timeout) const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  std::vector<DiscoveredDevice> result;
  for (const auto &[key, device] : impl_->devices) {
    if (device.is_recent(timeout)) {
      result.push_back(device);
    }
  }
  return result;
}

std::optional<DiscoveredDevice>
DiscoveryManager::get_device(const DeviceId &id) const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->device_key(id);
  auto it = impl_->devices.find(key);
  if (it != impl_->devices.end() && it->second.is_recent()) {
    return it->second;
  }
  return std::nullopt;
}

void DiscoveryManager::clear_discovered_devices() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->devices.clear();
}

Result<void> DiscoveryManager::set_config(const DiscoveryConfig &config) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config = config;
  return Result<void>::ok();
}

DiscoveryConfig DiscoveryManager::get_config() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->config;
}

void DiscoveryManager::set_local_device(const Device &device) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->local_device = device;
  // TODO: Update advertising if active
}

void DiscoveryManager::set_receiving(bool is_receiving) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->is_receiving = is_receiving;
  // TODO: Update advertising if active
}

void DiscoveryManager::on_device_discovered(
    std::function<void(const DiscoveredDevice &)> callback) {
  impl_->discovered_cb = std::move(callback);
}

void DiscoveryManager::on_device_lost(
    std::function<void(const DeviceId &)> callback) {
  impl_->lost_cb = std::move(callback);
}

void DiscoveryManager::on_device_updated(
    std::function<void(const DiscoveredDevice &)> callback) {
  impl_->updated_cb = std::move(callback);
}

void DiscoveryManager::on_state_changed(
    std::function<void(DiscoveryState)> callback) {
  impl_->state_changed_cb = std::move(callback);
}

void DiscoveryManager::on_error(std::function<void(const Error &)> callback) {
  impl_->error_cb = std::move(callback);
}

} // namespace seadrop

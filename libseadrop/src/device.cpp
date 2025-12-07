/**
 * @file device.cpp
 * @brief Device management and trust store implementation
 */

#include "seadrop/device.h"
#include "seadrop/security.h"
#include <map>
#include <mutex>

namespace seadrop {

// ============================================================================
// Trust Level Names
// ============================================================================

const char *trust_level_name(TrustLevel level) {
  switch (level) {
  case TrustLevel::Unknown:
    return "Unknown";
  case TrustLevel::Discovered:
    return "Discovered";
  case TrustLevel::PairingPending:
    return "Pairing Pending";
  case TrustLevel::Trusted:
    return "Trusted";
  case TrustLevel::Blocked:
    return "Blocked";
  default:
    return "Invalid";
  }
}

// ============================================================================
// Device Helper Methods
// ============================================================================

bool Device::can_auto_accept_files(TrustZone zone) const {
  if (trust_level != TrustLevel::Trusted) {
    return false;
  }

  // Based on distance zone
  switch (zone) {
  case TrustZone::Intimate:
  case TrustZone::Close:
    return true; // Full auto-accept
  case TrustZone::Nearby:
    return false; // Large files need confirmation
  case TrustZone::Far:
  case TrustZone::Unknown:
    return false; // Always confirm
  default:
    return false;
  }
}

bool Device::can_auto_clipboard(TrustZone zone) const {
  if (trust_level != TrustLevel::Trusted) {
    return false;
  }

  // Only auto-clipboard in Zone 1 (Intimate)
  return zone == TrustZone::Intimate;
}

// ============================================================================
// PairingRequest Methods
// ============================================================================

int PairingRequest::remaining_seconds() const {
  auto now = std::chrono::steady_clock::now();
  if (now >= expires_at)
    return 0;

  auto remaining =
      std::chrono::duration_cast<std::chrono::seconds>(expires_at - now);
  return static_cast<int>(remaining.count());
}

// ============================================================================
// DeviceStore Implementation
// ============================================================================

class DeviceStore::Impl {
public:
  std::string db_path;
  mutable std::mutex mutex;
  bool initialized = false;

  // In-memory cache (actual persistence handled by Database class)
  std::map<std::string, Device> devices;
  std::map<std::string, Bytes> shared_keys;

  std::string device_key(const DeviceId &id) const { return id.to_hex(); }
};

DeviceStore::DeviceStore() : impl_(std::make_unique<Impl>()) {}
DeviceStore::~DeviceStore() { close(); }

Result<void> DeviceStore::init(const std::string &db_path) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (impl_->initialized) {
    return Error(ErrorCode::AlreadyInitialized,
                 "DeviceStore already initialized");
  }

  impl_->db_path = db_path;
  impl_->initialized = true;

  // TODO: Load from SQLite database

  return Result<void>::ok();
}

void DeviceStore::close() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->devices.clear();
  impl_->shared_keys.clear();
  impl_->initialized = false;
}

Result<Device> DeviceStore::get_device(const DeviceId &id) const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->device_key(id);
  auto it = impl_->devices.find(key);
  if (it == impl_->devices.end()) {
    return Error(ErrorCode::RecordNotFound, "Device not found");
  }

  return it->second;
}

std::vector<Device> DeviceStore::get_trusted_devices() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  std::vector<Device> result;
  for (const auto &[key, device] : impl_->devices) {
    if (device.trust_level == TrustLevel::Trusted) {
      result.push_back(device);
    }
  }
  return result;
}

std::vector<Device> DeviceStore::get_blocked_devices() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  std::vector<Device> result;
  for (const auto &[key, device] : impl_->devices) {
    if (device.trust_level == TrustLevel::Blocked) {
      result.push_back(device);
    }
  }
  return result;
}

std::vector<Device> DeviceStore::get_all_devices() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  std::vector<Device> result;
  for (const auto &[key, device] : impl_->devices) {
    result.push_back(device);
  }
  return result;
}

bool DeviceStore::is_trusted(const DeviceId &id) const {
  auto result = get_device(id);
  return result.is_ok() && result.value().trust_level == TrustLevel::Trusted;
}

bool DeviceStore::is_blocked(const DeviceId &id) const {
  auto result = get_device(id);
  return result.is_ok() && result.value().trust_level == TrustLevel::Blocked;
}

Result<void> DeviceStore::save_device(const Device &device) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->device_key(device.id);
  impl_->devices[key] = device;

  // TODO: Persist to SQLite

  return Result<void>::ok();
}

Result<void> DeviceStore::trust_device(const DeviceId &id,
                                       const Bytes &shared_key) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->device_key(id);
  auto it = impl_->devices.find(key);
  if (it == impl_->devices.end()) {
    return Error(ErrorCode::RecordNotFound, "Device not found");
  }

  it->second.trust_level = TrustLevel::Trusted;
  it->second.paired_at = std::chrono::system_clock::now();
  impl_->shared_keys[key] = shared_key;

  // TODO: Persist to SQLite

  return Result<void>::ok();
}

Result<void> DeviceStore::block_device(const DeviceId &id) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->device_key(id);
  auto it = impl_->devices.find(key);
  if (it == impl_->devices.end()) {
    return Error(ErrorCode::RecordNotFound, "Device not found");
  }

  it->second.trust_level = TrustLevel::Blocked;
  impl_->shared_keys.erase(key); // Remove encryption key

  return Result<void>::ok();
}

Result<void> DeviceStore::untrust_device(const DeviceId &id) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->device_key(id);
  auto it = impl_->devices.find(key);
  if (it == impl_->devices.end()) {
    return Error(ErrorCode::RecordNotFound, "Device not found");
  }

  it->second.trust_level = TrustLevel::Discovered;
  impl_->shared_keys.erase(key);

  return Result<void>::ok();
}

Result<void> DeviceStore::unblock_device(const DeviceId &id) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->device_key(id);
  auto it = impl_->devices.find(key);
  if (it == impl_->devices.end()) {
    return Error(ErrorCode::RecordNotFound, "Device not found");
  }

  it->second.trust_level = TrustLevel::Discovered;

  return Result<void>::ok();
}

Result<void> DeviceStore::delete_device(const DeviceId &id) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->device_key(id);
  impl_->devices.erase(key);
  impl_->shared_keys.erase(key);

  return Result<void>::ok();
}

Result<void> DeviceStore::set_device_alias(const DeviceId &id,
                                           const std::string &alias) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->device_key(id);
  auto it = impl_->devices.find(key);
  if (it == impl_->devices.end()) {
    return Error(ErrorCode::RecordNotFound, "Device not found");
  }

  it->second.user_alias = alias;

  return Result<void>::ok();
}

Result<Bytes> DeviceStore::get_shared_key(const DeviceId &id) const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->device_key(id);
  auto it = impl_->shared_keys.find(key);
  if (it == impl_->shared_keys.end()) {
    return Error(ErrorCode::DeviceNotTrusted, "No shared key for device");
  }

  return it->second;
}

// ============================================================================
// PairingManager Implementation
// ============================================================================

class PairingManager::Impl {
public:
  DeviceStore *store = nullptr;
  std::optional<PairingRequest> current_pairing;
  std::mutex mutex;

  // Our ephemeral key pair for this pairing session
  std::optional<KeyPair> ephemeral_keys;

  // Callbacks
  PairingRequestCallback request_cb;
  PairingCompleteCallback complete_cb;
};

PairingManager::PairingManager() : impl_(std::make_unique<Impl>()) {}
PairingManager::~PairingManager() = default;

Result<void> PairingManager::init(DeviceStore *store) {
  if (!store) {
    return Error(ErrorCode::InvalidArgument, "DeviceStore cannot be null");
  }
  impl_->store = store;
  return Result<void>::ok();
}

Result<PairingRequest> PairingManager::initiate_pairing(const Device &device) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (impl_->current_pairing.has_value()) {
    return Error(ErrorCode::InvalidState, "Pairing already in progress");
  }

  // Generate ephemeral key pair
  auto keys_result = KeyPair::generate();
  if (keys_result.is_error()) {
    return keys_result.error();
  }
  impl_->ephemeral_keys = keys_result.value();

  // Generate PIN for user verification
  std::string pin = generate_pairing_pin();

  PairingRequest request;
  request.device = device;
  request.pin_code = pin;
  request.is_incoming = false;
  request.expires_at =
      std::chrono::steady_clock::now() + std::chrono::seconds(60);

  impl_->current_pairing = request;

  return request;
}

Result<void> PairingManager::accept_pairing(const PairingRequest &request) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (!impl_->current_pairing.has_value()) {
    return Error(ErrorCode::InvalidState, "No pairing in progress");
  }

  // In a real implementation, we would:
  // 1. Exchange public keys with peer
  // 2. Compute shared secret
  // 3. Derive encryption key
  // 4. Verify PIN matches on both sides

  // For now, generate a test shared key
  auto shared_key = random_bytes(32);

  // Trust the device
  auto result = impl_->store->trust_device(request.device.id, shared_key);
  if (result.is_error()) {
    if (impl_->complete_cb) {
      impl_->complete_cb(request.device, false);
    }
    return result;
  }

  impl_->current_pairing = std::nullopt;
  impl_->ephemeral_keys = std::nullopt;

  if (impl_->complete_cb) {
    impl_->complete_cb(request.device, true);
  }

  return Result<void>::ok();
}

void PairingManager::reject_pairing(const PairingRequest &request) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  impl_->current_pairing = std::nullopt;
  impl_->ephemeral_keys = std::nullopt;

  if (impl_->complete_cb) {
    impl_->complete_cb(request.device, false);
  }
}

void PairingManager::cancel_pairing() {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (impl_->current_pairing.has_value() && impl_->complete_cb) {
    impl_->complete_cb(impl_->current_pairing->device, false);
  }

  impl_->current_pairing = std::nullopt;
  impl_->ephemeral_keys = std::nullopt;
}

bool PairingManager::is_pairing() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->current_pairing.has_value();
}

std::optional<PairingRequest> PairingManager::get_current_pairing() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->current_pairing;
}

void PairingManager::on_pairing_request(PairingRequestCallback callback) {
  impl_->request_cb = std::move(callback);
}

void PairingManager::on_pairing_complete(PairingCompleteCallback callback) {
  impl_->complete_cb = std::move(callback);
}

} // namespace seadrop

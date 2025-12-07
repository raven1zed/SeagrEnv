/**
 * @file seadrop.cpp
 * @brief Main SeaDrop class implementation
 */

#include "seadrop/seadrop.h"
#include <mutex>

namespace seadrop {

// ============================================================================
// Version Information
// ============================================================================

VersionInfo get_version() { return VersionInfo{}; }

// ============================================================================
// State Names
// ============================================================================

const char *seadrop_state_name(SeaDropState state) {
  switch (state) {
  case SeaDropState::Uninitialized:
    return "Uninitialized";
  case SeaDropState::Idle:
    return "Idle";
  case SeaDropState::Discovering:
    return "Discovering";
  case SeaDropState::Connected:
    return "Connected";
  case SeaDropState::Transferring:
    return "Transferring";
  case SeaDropState::Error:
    return "Error";
  default:
    return "Unknown";
  }
}

// ============================================================================
// Platform Helpers
// ============================================================================

const char *get_platform_name() {
#if defined(SEADROP_PLATFORM_LINUX)
  return "Linux";
#elif defined(SEADROP_PLATFORM_ANDROID)
  return "Android";
#elif defined(SEADROP_PLATFORM_WINDOWS)
  return "Windows";
#elif defined(SEADROP_PLATFORM_MACOS)
  return "macOS";
#elif defined(SEADROP_PLATFORM_IOS)
  return "iOS";
#else
  return "Unknown";
#endif
}

bool is_wifi_direct_supported() {
#if defined(SEADROP_PLATFORM_LINUX) || defined(SEADROP_PLATFORM_ANDROID)
  return true;
#elif defined(SEADROP_PLATFORM_WINDOWS)
  // Windows support is limited
  return false;
#else
  return false;
#endif
}

bool is_bluetooth_supported() {
#if defined(SEADROP_PLATFORM_LINUX) || defined(SEADROP_PLATFORM_ANDROID) ||    \
    defined(SEADROP_PLATFORM_WINDOWS) || defined(SEADROP_PLATFORM_APPLE)
  return true;
#else
  return false;
#endif
}

std::string get_default_device_name() {
  // TODO: Get actual hostname/device model
  return "SeaDrop Device";
}

Result<void> open_file(const std::filesystem::path &path) {
  // TODO: Platform-specific implementation
  SEADROP_UNUSED(path);
  return Error(ErrorCode::NotSupported, "Not yet implemented");
}

Result<void> reveal_in_file_manager(const std::filesystem::path &path) {
  // TODO: Platform-specific implementation
  SEADROP_UNUSED(path);
  return Error(ErrorCode::NotSupported, "Not yet implemented");
}

// ============================================================================
// SeaDrop Implementation
// ============================================================================

class SeaDrop::Impl {
public:
  SeaDropState state = SeaDropState::Uninitialized;
  SeaDropConfig config;
  std::mutex mutex;

  // Subsystems
  DiscoveryManager discovery;
  ConnectionManager connection;
  TransferManager transfer;
  ClipboardManager clipboard;
  DistanceMonitor distance;
  DeviceStore device_store;
  PairingManager pairing;
  ConfigManager config_manager;
  Database database;

  // Our identity
  Device local_device;
  std::optional<KeyPair> identity_keys;

  // Callbacks
  std::function<void(const Device &)> device_discovered_cb;
  std::function<void(const DeviceId &)> device_lost_cb;
  std::function<void(const Device &)> device_updated_cb;
  std::function<void(const Device &)> connection_request_cb;
  std::function<void(const Device &)> connected_cb;
  std::function<void(const DeviceId &, const std::string &)> disconnected_cb;
  std::function<void(const PairingRequest &)> pairing_request_cb;
  std::function<void(const Device &, bool)> pairing_complete_cb;
  std::function<void(const TransferRequest &)> transfer_request_cb;
  std::function<void(const TransferProgress &)> transfer_progress_cb;
  std::function<void(const TransferResult &)> transfer_complete_cb;
  std::function<void(const FileInfo &)> file_received_cb;
  std::function<void(const ReceivedClipboard &)> clipboard_received_cb;
  std::function<void(const ZoneChangeEvent &)> zone_changed_cb;
  std::function<void(const DeviceId &, const std::string &)> security_alert_cb;
  std::function<void(SeaDropState)> state_changed_cb;
  std::function<void(const Error &)> error_cb;

  void set_state(SeaDropState new_state) {
    if (state != new_state) {
      state = new_state;
      if (state_changed_cb) {
        state_changed_cb(state);
      }
    }
  }
};

SeaDrop::SeaDrop() : impl_(std::make_unique<Impl>()) {}
SeaDrop::~SeaDrop() { shutdown(); }

Result<void> SeaDrop::init(const SeaDropConfig &config) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (impl_->state != SeaDropState::Uninitialized) {
    return Error(ErrorCode::AlreadyInitialized, "SeaDrop already initialized");
  }

  // Validate config
  auto validation = config.validate();
  if (validation.is_error()) {
    return validation;
  }

  impl_->config = config;

  // Initialize security first
  auto sec_result = security_init();
  if (sec_result.is_error()) {
    return sec_result;
  }

  // Generate or load identity
  auto key_result = KeyPair::generate();
  if (key_result.is_error()) {
    return key_result.error();
  }
  impl_->identity_keys = key_result.value();

  // Set up local device info
  impl_->local_device.name = config.device_name.empty()
                                 ? get_default_device_name()
                                 : config.device_name;

#if defined(SEADROP_PLATFORM_LINUX)
  impl_->local_device.platform = DevicePlatform::Linux;
  impl_->local_device.type = DeviceType::Desktop; // TODO: Detect properly
#elif defined(SEADROP_PLATFORM_ANDROID)
  impl_->local_device.platform = DevicePlatform::Android;
  impl_->local_device.type = DeviceType::Phone;
#elif defined(SEADROP_PLATFORM_WINDOWS)
  impl_->local_device.platform = DevicePlatform::Windows;
  impl_->local_device.type = DeviceType::Desktop;
#endif

  impl_->local_device.seadrop_version = VERSION_STRING;
  impl_->local_device.supports_wifi_direct = is_wifi_direct_supported();
  impl_->local_device.supports_bluetooth = is_bluetooth_supported();
  impl_->local_device.supports_clipboard = true;

  // Derive device ID from public key
  auto id_hash = hash(Bytes(impl_->identity_keys->public_key.begin(),
                            impl_->identity_keys->public_key.end()));
  if (id_hash.is_ok()) {
    std::memcpy(impl_->local_device.id.data.data(), id_hash.value().data(),
                DeviceId::SIZE);
  }

  // Initialize database
  if (!config.database_path.empty()) {
    auto db_result = impl_->database.open(config.database_path);
    if (db_result.is_error()) {
      // Log warning but continue - we can work without persistent storage
    }
  }

  // Initialize device store
  auto store_result = impl_->device_store.init(config.database_path.string());
  if (store_result.is_error()) {
    // Non-fatal
  }

  // Initialize pairing manager
  impl_->pairing.init(&impl_->device_store);

  // Initialize transfer manager
  TransferOptions transfer_opts;
  transfer_opts.save_directory = config.download_path;
  transfer_opts.on_conflict = config.conflict_resolution;
  transfer_opts.verify_checksum = config.verify_checksums;
  impl_->transfer.init(transfer_opts);

  // Initialize clipboard manager
  impl_->clipboard.init(config.clipboard);

  // Initialize distance monitor with thresholds
  impl_->distance.set_zone_thresholds(config.zone_thresholds);

  // Set up internal callbacks
  impl_->distance.on_zone_changed([this](const ZoneChangeEvent &e) {
    if (impl_->zone_changed_cb) {
      impl_->zone_changed_cb(e);
    }
  });

  impl_->distance.on_security_alert(
      [this](const DeviceId &id, const std::string &msg) {
        if (impl_->security_alert_cb) {
          impl_->security_alert_cb(id, msg);
        }
      });

  impl_->set_state(SeaDropState::Idle);
  return Result<void>::ok();
}

void SeaDrop::shutdown() {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (impl_->state == SeaDropState::Uninitialized) {
    return;
  }

  // Stop all subsystems
  impl_->discovery.shutdown();
  impl_->connection.shutdown();
  impl_->transfer.shutdown();
  impl_->clipboard.shutdown();
  impl_->distance.stop();
  impl_->device_store.close();
  impl_->database.close();

  impl_->set_state(SeaDropState::Uninitialized);
}

bool SeaDrop::is_initialized() const {
  return impl_->state != SeaDropState::Uninitialized;
}

SeaDropState SeaDrop::get_state() const { return impl_->state; }

// ============================================================================
// Discovery
// ============================================================================

Result<void> SeaDrop::start_discovery() {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (impl_->state == SeaDropState::Uninitialized) {
    return Error(ErrorCode::NotInitialized, "SeaDrop not initialized");
  }

  auto result = impl_->discovery.start();
  if (result.is_ok()) {
    if (impl_->state == SeaDropState::Idle) {
      impl_->set_state(SeaDropState::Discovering);
    }
  }
  return result;
}

void SeaDrop::stop_discovery() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->discovery.stop();

  if (impl_->state == SeaDropState::Discovering) {
    impl_->set_state(SeaDropState::Idle);
  }
}

bool SeaDrop::is_discovering() const {
  return impl_->discovery.get_state() == DiscoveryState::Active;
}

std::vector<Device> SeaDrop::get_nearby_devices() const {
  auto discovered = impl_->discovery.get_nearby_devices();

  std::vector<Device> devices;
  devices.reserve(discovered.size());
  for (const auto &d : discovered) {
    devices.push_back(d.device);
  }
  return devices;
}

// ============================================================================
// Connection
// ============================================================================

Result<void> SeaDrop::connect(const Device &device) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  return impl_->connection.connect(device);
}

void SeaDrop::disconnect() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->connection.disconnect();
}

bool SeaDrop::is_connected() const { return impl_->connection.is_connected(); }

std::optional<Device> SeaDrop::get_connected_peer() const {
  auto peer_id = impl_->connection.get_peer_id();
  if (!peer_id) {
    return std::nullopt;
  }

  // TODO: Get full device info
  Device device;
  device.id = *peer_id;
  return device;
}

// ============================================================================
// Pairing
// ============================================================================

Result<PairingRequest> SeaDrop::pair(const Device &device) {
  return impl_->pairing.initiate_pairing(device);
}

Result<void> SeaDrop::accept_pairing(const PairingRequest &request) {
  return impl_->pairing.accept_pairing(request);
}

void SeaDrop::reject_pairing(const PairingRequest &request) {
  impl_->pairing.reject_pairing(request);
}

bool SeaDrop::is_trusted(const DeviceId &id) const {
  return impl_->device_store.is_trusted(id);
}

std::vector<Device> SeaDrop::get_trusted_devices() const {
  return impl_->device_store.get_trusted_devices();
}

Result<void> SeaDrop::untrust_device(const DeviceId &id) {
  return impl_->device_store.untrust_device(id);
}

Result<void> SeaDrop::block_device(const DeviceId &id) {
  return impl_->device_store.block_device(id);
}

// ============================================================================
// Transfer
// ============================================================================

Result<TransferId> SeaDrop::send_file(const std::filesystem::path &path) {
  return impl_->transfer.send_file(path);
}

Result<TransferId>
SeaDrop::send_files(const std::vector<std::filesystem::path> &paths) {
  return impl_->transfer.send_files(paths);
}

Result<TransferId> SeaDrop::send_directory(const std::filesystem::path &path) {
  return impl_->transfer.send_directory(path);
}

Result<TransferId> SeaDrop::send_text(const std::string &text) {
  return impl_->transfer.send_text(text);
}

Result<void> SeaDrop::accept_transfer(const TransferId &transfer_id) {
  return impl_->transfer.accept_transfer(transfer_id);
}

void SeaDrop::reject_transfer(const TransferId &transfer_id) {
  impl_->transfer.reject_transfer(transfer_id);
}

void SeaDrop::cancel_transfer(const TransferId &transfer_id) {
  impl_->transfer.cancel_transfer(transfer_id);
}

std::optional<TransferProgress>
SeaDrop::get_transfer_progress(const TransferId &id) const {
  auto result = impl_->transfer.get_progress(id);
  return result.is_ok() ? std::optional(result.value()) : std::nullopt;
}

// ============================================================================
// Clipboard
// ============================================================================

Result<void> SeaDrop::push_clipboard() {
  auto peer = get_connected_peer();
  if (!peer) {
    return Error(ErrorCode::NotConnected, "Not connected to any device");
  }
  return impl_->clipboard.push_to_device(*peer);
}

void SeaDrop::set_auto_clipboard(bool enabled) {
  impl_->clipboard.set_auto_share(enabled);
}

bool SeaDrop::is_auto_clipboard_enabled() const {
  return impl_->clipboard.is_auto_share_enabled();
}

// ============================================================================
// Distance
// ============================================================================

std::optional<DistanceInfo> SeaDrop::get_peer_distance() const {
  auto peer_id = impl_->connection.get_peer_id();
  if (!peer_id) {
    return std::nullopt;
  }

  auto result = impl_->distance.get_distance(*peer_id);
  return result.is_ok() ? std::optional(result.value()) : std::nullopt;
}

TrustZone SeaDrop::get_peer_zone() const {
  auto peer_id = impl_->connection.get_peer_id();
  if (!peer_id) {
    return TrustZone::Unknown;
  }
  return impl_->distance.get_zone(*peer_id);
}

Result<void> SeaDrop::set_zone_thresholds(const ZoneThresholds &thresholds) {
  return impl_->distance.set_zone_thresholds(thresholds);
}

// ============================================================================
// Configuration
// ============================================================================

const SeaDropConfig &SeaDrop::get_config() const { return impl_->config; }

Result<void> SeaDrop::set_config(const SeaDropConfig &config) {
  auto validation = config.validate();
  if (validation.is_error()) {
    return validation;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config = config;

  // Update subsystems with new config
  impl_->distance.set_zone_thresholds(config.zone_thresholds);
  impl_->clipboard.set_config(config.clipboard);

  return Result<void>::ok();
}

ConfigManager &SeaDrop::get_config_manager() { return impl_->config_manager; }

// ============================================================================
// Local Device
// ============================================================================

Device SeaDrop::get_local_device() const { return impl_->local_device; }

DeviceId SeaDrop::get_local_id() const { return impl_->local_device.id; }

Result<void> SeaDrop::set_device_name(const std::string &name) {
  if (name.empty() || name.length() > 64) {
    return Error(ErrorCode::InvalidArgument, "Invalid device name");
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->local_device.name = name;
  impl_->config.device_name = name;

  // Update discovery advertisement
  impl_->discovery.set_local_device(impl_->local_device);

  return Result<void>::ok();
}

// ============================================================================
// Callbacks
// ============================================================================

void SeaDrop::on_device_discovered(
    std::function<void(const Device &)> callback) {
  impl_->device_discovered_cb = std::move(callback);
}

void SeaDrop::on_device_lost(std::function<void(const DeviceId &)> callback) {
  impl_->device_lost_cb = std::move(callback);
}

void SeaDrop::on_device_updated(std::function<void(const Device &)> callback) {
  impl_->device_updated_cb = std::move(callback);
}

void SeaDrop::on_connection_request(
    std::function<void(const Device &)> callback) {
  impl_->connection_request_cb = std::move(callback);
}

void SeaDrop::on_connected(std::function<void(const Device &)> callback) {
  impl_->connected_cb = std::move(callback);
}

void SeaDrop::on_disconnected(
    std::function<void(const DeviceId &, const std::string &)> callback) {
  impl_->disconnected_cb = std::move(callback);
}

void SeaDrop::on_pairing_request(
    std::function<void(const PairingRequest &)> callback) {
  impl_->pairing_request_cb = std::move(callback);
}

void SeaDrop::on_pairing_complete(
    std::function<void(const Device &, bool)> callback) {
  impl_->pairing_complete_cb = std::move(callback);
}

void SeaDrop::on_transfer_request(
    std::function<void(const TransferRequest &)> callback) {
  impl_->transfer_request_cb = std::move(callback);
}

void SeaDrop::on_transfer_progress(
    std::function<void(const TransferProgress &)> callback) {
  impl_->transfer_progress_cb = std::move(callback);
}

void SeaDrop::on_transfer_complete(
    std::function<void(const TransferResult &)> callback) {
  impl_->transfer_complete_cb = std::move(callback);
}

void SeaDrop::on_file_received(std::function<void(const FileInfo &)> callback) {
  impl_->file_received_cb = std::move(callback);
}

void SeaDrop::on_clipboard_received(
    std::function<void(const ReceivedClipboard &)> callback) {
  impl_->clipboard_received_cb = std::move(callback);
}

void SeaDrop::on_zone_changed(
    std::function<void(const ZoneChangeEvent &)> callback) {
  impl_->zone_changed_cb = std::move(callback);
}

void SeaDrop::on_security_alert(
    std::function<void(const DeviceId &, const std::string &)> callback) {
  impl_->security_alert_cb = std::move(callback);
}

void SeaDrop::on_state_changed(std::function<void(SeaDropState)> callback) {
  impl_->state_changed_cb = std::move(callback);
}

void SeaDrop::on_error(std::function<void(const Error &)> callback) {
  impl_->error_cb = std::move(callback);
}

// ============================================================================
// Component Access
// ============================================================================

DiscoveryManager &SeaDrop::get_discovery_manager() { return impl_->discovery; }
ConnectionManager &SeaDrop::get_connection_manager() {
  return impl_->connection;
}
TransferManager &SeaDrop::get_transfer_manager() { return impl_->transfer; }
ClipboardManager &SeaDrop::get_clipboard_manager() { return impl_->clipboard; }
DistanceMonitor &SeaDrop::get_distance_monitor() { return impl_->distance; }
DeviceStore &SeaDrop::get_device_store() { return impl_->device_store; }
Database &SeaDrop::get_database() { return impl_->database; }

} // namespace seadrop

/**
 * @file connection.cpp
 * @brief WiFi Direct connection manager implementation (stub)
 *
 * This is a stub implementation. Platform-specific code will be
 * implemented in platform/linux/ and platform/android/ directories.
 */

#include "seadrop/connection.h"
#include <mutex>

namespace seadrop {

// ============================================================================
// Connection State Names
// ============================================================================

const char *connection_state_name(ConnectionState state) {
  switch (state) {
  case ConnectionState::Disconnected:
    return "Disconnected";
  case ConnectionState::Connecting:
    return "Connecting";
  case ConnectionState::Establishing:
    return "Establishing";
  case ConnectionState::Handshaking:
    return "Handshaking";
  case ConnectionState::Connected:
    return "Connected";
  case ConnectionState::Disconnecting:
    return "Disconnecting";
  case ConnectionState::Lost:
    return "Lost";
  case ConnectionState::Error:
    return "Error";
  default:
    return "Unknown";
  }
}

// ============================================================================
// ConnectionInfo
// ============================================================================

std::chrono::milliseconds ConnectionInfo::connection_duration() const {
  if (state != ConnectionState::Connected) {
    return std::chrono::milliseconds(0);
  }

  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                               connected_at);
}

// ============================================================================
// ConnectionManager Implementation
// ============================================================================

class ConnectionManager::Impl {
public:
  ConnectionState state = ConnectionState::Disconnected;
  ConnectionConfig config;
  ConnectionInfo current_info;
  Device local_device;
  DeviceStore *device_store = nullptr;

  std::mutex mutex;
  int socket_fd = -1;

  // Callbacks
  std::function<void(ConnectionState)> state_changed_cb;
  std::function<void(const ConnectionInfo &)> connected_cb;
  std::function<void(const DeviceId &, const std::string &)> disconnected_cb;
  std::function<void(const Device &)> connection_request_cb;
  std::function<void(const Error &)> error_cb;
  std::function<void(int)> rssi_updated_cb;

  void set_state(ConnectionState new_state) {
    if (state != new_state) {
      state = new_state;
      current_info.state = new_state;
      if (state_changed_cb) {
        state_changed_cb(state);
      }
    }
  }
};

ConnectionManager::ConnectionManager() : impl_(std::make_unique<Impl>()) {}
ConnectionManager::~ConnectionManager() { shutdown(); }

Result<void> ConnectionManager::init(const Device &local_device,
                                     DeviceStore *device_store,
                                     const ConnectionConfig &config) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  impl_->local_device = local_device;
  impl_->device_store = device_store;
  impl_->config = config;
  impl_->state = ConnectionState::Disconnected;

  return Result<void>::ok();
}

void ConnectionManager::shutdown() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  disconnect();
}

bool ConnectionManager::is_initialized() const {
  return true; // Always initialized after construction
}

Result<void> ConnectionManager::connect(const Device &device) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (impl_->state != ConnectionState::Disconnected) {
    return Error(ErrorCode::AlreadyConnected,
                 "Already connecting or connected");
  }

  impl_->set_state(ConnectionState::Connecting);
  impl_->current_info.peer_id = device.id;
  impl_->current_info.peer_name = device.name;

  // TODO: Initiate WiFi Direct connection
  // This will be implemented in platform-specific code

  return Result<void>::ok();
}

Result<void> ConnectionManager::accept_connection(const Device &device) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  // TODO: Accept incoming WiFi Direct connection
  SEADROP_UNUSED(device);

  return Result<void>::ok();
}

void ConnectionManager::reject_connection(const Device &device) {
  // TODO: Reject incoming WiFi Direct connection
  SEADROP_UNUSED(device);
}

void ConnectionManager::disconnect() {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (impl_->state == ConnectionState::Disconnected) {
    return;
  }

  DeviceId peer_id = impl_->current_info.peer_id;

  impl_->set_state(ConnectionState::Disconnecting);

  // TODO: Close WiFi Direct connection

  if (impl_->socket_fd >= 0) {
    // TODO: Close socket
    impl_->socket_fd = -1;
  }

  impl_->set_state(ConnectionState::Disconnected);
  impl_->current_info = ConnectionInfo{};

  if (impl_->disconnected_cb) {
    impl_->disconnected_cb(peer_id, "User disconnected");
  }
}

void ConnectionManager::cancel_connection() {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (impl_->state == ConnectionState::Connecting ||
      impl_->state == ConnectionState::Establishing ||
      impl_->state == ConnectionState::Handshaking) {

    // TODO: Cancel connection attempt
    impl_->set_state(ConnectionState::Disconnected);
  }
}

ConnectionState ConnectionManager::get_state() const { return impl_->state; }

ConnectionInfo ConnectionManager::get_connection_info() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->current_info;
}

bool ConnectionManager::is_connected() const {
  return impl_->state == ConnectionState::Connected;
}

std::optional<DeviceId> ConnectionManager::get_peer_id() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (impl_->state != ConnectionState::Connected) {
    return std::nullopt;
  }

  return impl_->current_info.peer_id;
}

int ConnectionManager::get_socket() const { return impl_->socket_fd; }

int ConnectionManager::get_rssi() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->current_info.rssi_dbm;
}

Result<void> ConnectionManager::set_config(const ConnectionConfig &config) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config = config;
  return Result<void>::ok();
}

ConnectionConfig ConnectionManager::get_config() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->config;
}

void ConnectionManager::on_state_changed(
    std::function<void(ConnectionState)> callback) {
  impl_->state_changed_cb = std::move(callback);
}

void ConnectionManager::on_connected(
    std::function<void(const ConnectionInfo &)> callback) {
  impl_->connected_cb = std::move(callback);
}

void ConnectionManager::on_disconnected(
    std::function<void(const DeviceId &, const std::string &)> callback) {
  impl_->disconnected_cb = std::move(callback);
}

void ConnectionManager::on_connection_request(
    std::function<void(const Device &)> callback) {
  impl_->connection_request_cb = std::move(callback);
}

void ConnectionManager::on_error(std::function<void(const Error &)> callback) {
  impl_->error_cb = std::move(callback);
}

void ConnectionManager::on_rssi_updated(std::function<void(int)> callback) {
  impl_->rssi_updated_cb = std::move(callback);
}

} // namespace seadrop

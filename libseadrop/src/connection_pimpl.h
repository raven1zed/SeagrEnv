#ifndef SEADROP_CONNECTION_PIMPL_H
#define SEADROP_CONNECTION_PIMPL_H

#include "seadrop/connection.h"
#include <mutex>

namespace seadrop {

class ConnectionManager::Impl {
public:
  ConnectionState state = ConnectionState::Disconnected;
  ConnectionConfig config;
  ConnectionInfo current_info;
  Device local_device;
  DeviceStore *device_store = nullptr;

  mutable std::mutex mutex;
  int socket_fd = -1;

  // Platform-specific context (opaque pointer)
  void *platform_ctx = nullptr;

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

// Platform hooks
Result<void> platform_connection_init(ConnectionManager::Impl *impl);
void platform_connection_shutdown(ConnectionManager::Impl *impl);
Result<void> platform_connection_connect(ConnectionManager::Impl *impl,
                                         const Device &device);
Result<void> platform_connection_accept(ConnectionManager::Impl *impl,
                                        const Device &device);
void platform_connection_disconnect(ConnectionManager::Impl *impl);
void platform_connection_cancel(ConnectionManager::Impl *impl);

} // namespace seadrop

#endif // SEADROP_CONNECTION_PIMPL_H

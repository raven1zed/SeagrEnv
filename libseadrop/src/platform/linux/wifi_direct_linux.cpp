/**
 * @file wifi_direct_linux.cpp
 * @brief WiFi Direct platform hooks for Linux
 *
 * Implements the platform hooks declared in connection_pimpl.h using
 * wpa_supplicant.
 */

#include "../../connection_pimpl.h"
#include "wpa_supplicant.h"

namespace seadrop {

// ============================================================================
// Platform Hook Implementations
// ============================================================================

Result<void> platform_connection_init(ConnectionManager::Impl *impl) {
  using namespace platform;

  // Get system bus connection
  auto conn_result = get_system_bus();
  if (conn_result.is_error()) {
    return conn_result.error();
  }

  // Find WiFi interface
  auto iface_result = find_wifi_interface(conn_result.value().get());
  if (iface_result.is_error()) {
    return iface_result.error();
  }

  // Create platform context
  auto *ctx = new WpaSupplicantContext();
  ctx->conn = std::move(conn_result.value());
  ctx->interface_path = iface_result.value();

  impl->platform_ctx = ctx;

  // Start P2P device
  auto start_result = p2p_start(ctx->conn.get(), ctx->interface_path);
  if (start_result.is_error()) {
    // Not fatal - P2P might already be started
  }

  return Result<void>::ok();
}

void platform_connection_shutdown(ConnectionManager::Impl *impl) {
  if (!impl->platform_ctx) {
    return;
  }

  auto *ctx = static_cast<platform::WpaSupplicantContext *>(impl->platform_ctx);

  // Stop P2P
  platform::p2p_stop(ctx->conn.get(), ctx->interface_path);

  // Cleanup
  ctx->stop_requested = true;
  if (ctx->event_thread.joinable()) {
    ctx->event_thread.join();
  }

  delete ctx;
  impl->platform_ctx = nullptr;
}

Result<void> platform_connection_connect(ConnectionManager::Impl *impl,
                                         const Device &device) {
  using namespace platform;

  if (!impl->platform_ctx) {
    return Error(ErrorCode::InvalidState, "Platform not initialized");
  }

  auto *ctx = static_cast<WpaSupplicantContext *>(impl->platform_ctx);

  // Use device's WiFi Direct address (stored in id)
  std::string peer_address = device.id.to_hex();

  // Initiate P2P connection
  auto result = p2p_connect(ctx->conn.get(), ctx->interface_path, peer_address,
                            impl->config.go_intent);

  if (result.is_error()) {
    return result.error();
  }

  ctx->state = P2PState::Connecting;
  return Result<void>::ok();
}

Result<void> platform_connection_accept(ConnectionManager::Impl *impl,
                                        const Device &device) {
  // For P2P, accepting is similar to connecting
  // The GO negotiation determines roles
  return platform_connection_connect(impl, device);
}

void platform_connection_disconnect(ConnectionManager::Impl *impl) {
  using namespace platform;

  if (!impl->platform_ctx) {
    return;
  }

  auto *ctx = static_cast<WpaSupplicantContext *>(impl->platform_ctx);
  p2p_disconnect(ctx->conn.get(), ctx->interface_path);
  ctx->state = P2PState::Idle;
  ctx->current_group = P2PGroup{};
}

void platform_connection_cancel(ConnectionManager::Impl *impl) {
  // Same as disconnect for P2P
  platform_connection_disconnect(impl);
}

} // namespace seadrop

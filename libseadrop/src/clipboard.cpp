/**
 * @file clipboard.cpp
 * @brief Clipboard sharing implementation
 */

#include "seadrop/clipboard.h"
#include <mutex>

namespace seadrop {

// ============================================================================
// Clipboard Type Names
// ============================================================================

const char *clipboard_type_name(ClipboardType type) {
  switch (type) {
  case ClipboardType::Empty:
    return "Empty";
  case ClipboardType::Text:
    return "Text";
  case ClipboardType::Url:
    return "URL";
  case ClipboardType::RichText:
    return "Rich Text";
  case ClipboardType::Image:
    return "Image";
  case ClipboardType::Files:
    return "Files";
  case ClipboardType::Unknown:
    return "Unknown";
  default:
    return "Invalid";
  }
}

// ============================================================================
// ClipboardData Methods
// ============================================================================

std::string ClipboardData::get_text() const {
  if (type != ClipboardType::Text && type != ClipboardType::Url &&
      type != ClipboardType::RichText) {
    return "";
  }

  return std::string(data.begin(), data.end());
}

ClipboardData ClipboardData::from_text(const std::string &text) {
  ClipboardData data;
  data.type = ClipboardType::Text;
  data.data = Bytes(text.begin(), text.end());
  data.preview = text.substr(0, 100);
  data.mime_type = "text/plain";
  data.captured_at = std::chrono::system_clock::now();
  return data;
}

ClipboardData ClipboardData::from_url(const std::string &url) {
  ClipboardData data;
  data.type = ClipboardType::Url;
  data.data = Bytes(url.begin(), url.end());
  data.preview = url.substr(0, 100);
  data.mime_type = "text/uri-list";
  data.captured_at = std::chrono::system_clock::now();
  return data;
}

ClipboardData ClipboardData::from_image(const Bytes &png_data, int width,
                                        int height) {
  ClipboardData data;
  data.type = ClipboardType::Image;
  data.data = png_data;
  data.preview =
      "[Image " + std::to_string(width) + "x" + std::to_string(height) + "]";
  data.mime_type = "image/png";
  data.image_info.width = width;
  data.image_info.height = height;
  data.image_info.channels = 4; // RGBA
  data.captured_at = std::chrono::system_clock::now();
  return data;
}

// ============================================================================
// ClipboardManager Implementation
// ============================================================================

class ClipboardManager::Impl {
public:
  ClipboardConfig config;
  std::mutex mutex;
  bool initialized = false;

  // Receive history
  std::vector<ReceivedClipboard> history;
  static constexpr size_t MAX_HISTORY = 50;

  // Callbacks
  std::function<void(const ReceivedClipboard &)> received_cb;
  std::function<void(const Device &)> sent_cb;
  std::function<void(const Error &)> error_cb;
};

ClipboardManager::ClipboardManager() : impl_(std::make_unique<Impl>()) {}
ClipboardManager::~ClipboardManager() { shutdown(); }

Result<void> ClipboardManager::init(const ClipboardConfig &config) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  impl_->config = config;
  impl_->initialized = true;

  return Result<void>::ok();
}

void ClipboardManager::shutdown() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->history.clear();
  impl_->initialized = false;
}

Result<ClipboardData> ClipboardManager::get_local_clipboard() const {
  // TODO: Platform-specific clipboard reading
  return ClipboardData();
}

Result<void> ClipboardManager::set_local_clipboard(const ClipboardData &data) {
  // TODO: Platform-specific clipboard writing
  SEADROP_UNUSED(data);
  return Result<void>::ok();
}

Result<void> ClipboardManager::push_to_device(const Device &device) {
  auto clipboard_result = get_local_clipboard();
  if (clipboard_result.is_error()) {
    return clipboard_result.error();
  }

  const auto &data = clipboard_result.value();
  if (data.is_empty()) {
    return Error(ErrorCode::InvalidState, "Clipboard is empty");
  }

  return send_clipboard(device, data);
}

Result<int> ClipboardManager::push_to_all_trusted() {
  // TODO: Implement sending to all connected trusted devices
  return 0;
}

Result<void> ClipboardManager::send_clipboard(const Device &device,
                                              const ClipboardData &data) {
  // TODO: Send clipboard data over connection
  SEADROP_UNUSED(device);
  SEADROP_UNUSED(data);

  if (impl_->sent_cb) {
    impl_->sent_cb(device);
  }

  return Result<void>::ok();
}

void ClipboardManager::set_auto_share(bool enabled) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config.auto_share_enabled = enabled;
}

bool ClipboardManager::is_auto_share_enabled() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->config.auto_share_enabled;
}

void ClipboardManager::trigger_auto_share_check() {
  if (!impl_->config.auto_share_enabled) {
    return;
  }

  // TODO: Check if in Zone 1 and push clipboard
}

std::vector<ReceivedClipboard>
ClipboardManager::get_receive_history(size_t limit) const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (limit == 0 || limit >= impl_->history.size()) {
    return impl_->history;
  }

  return std::vector<ReceivedClipboard>(impl_->history.end() - limit,
                                        impl_->history.end());
}

Result<void> ClipboardManager::apply_received(size_t index) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (index >= impl_->history.size()) {
    return Error(ErrorCode::InvalidArgument, "Invalid history index");
  }

  auto result = set_local_clipboard(impl_->history[index].data);
  if (result.is_ok()) {
    impl_->history[index].applied = true;
  }

  return result;
}

void ClipboardManager::clear_history() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->history.clear();
}

void ClipboardManager::set_config(const ClipboardConfig &config) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config = config;
}

ClipboardConfig ClipboardManager::get_config() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->config;
}

void ClipboardManager::on_received(
    std::function<void(const ReceivedClipboard &)> callback) {
  impl_->received_cb = std::move(callback);
}

void ClipboardManager::on_sent(std::function<void(const Device &)> callback) {
  impl_->sent_cb = std::move(callback);
}

void ClipboardManager::on_error(std::function<void(const Error &)> callback) {
  impl_->error_cb = std::move(callback);
}

// ============================================================================
// Platform Helpers (Stubs)
// ============================================================================

bool is_clipboard_available() { return true; }

Result<void> register_clipboard_hotkey(const std::string &hotkey,
                                       std::function<void()> callback) {
  // TODO: Platform-specific implementation
  SEADROP_UNUSED(hotkey);
  SEADROP_UNUSED(callback);
  return Error(ErrorCode::NotSupported, "Not yet implemented");
}

void unregister_clipboard_hotkey() {
  // TODO: Platform-specific implementation
}

} // namespace seadrop

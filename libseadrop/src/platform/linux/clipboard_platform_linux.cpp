/**
 * @file clipboard_platform_linux.cpp
 * @brief Clipboard platform hooks for Linux
 *
 * Implements the platform hooks declared in clipboard_pimpl.h using
 * X11/Wayland clipboard access.
 */

#include "../../clipboard_pimpl.h"
#include "clipboard_linux.h"

namespace seadrop {

// ============================================================================
// Platform Hook Implementations
// ============================================================================

Result<ClipboardData> platform_get_clipboard() {
  using namespace platform;

  // Try to read text first (most common)
  auto text_result = read_clipboard_text();
  if (text_result.is_ok() && !text_result.value().empty()) {
    const auto &text = text_result.value();

    // Check if it's a URL
    if (text.find("http://") == 0 || text.find("https://") == 0 ||
        text.find("ftp://") == 0) {
      return ClipboardData::from_url(text);
    }

    return ClipboardData::from_text(text);
  }

  // Try to read image
  auto image_result = read_clipboard_image();
  if (image_result.is_ok() && !image_result.value().empty()) {
    // TODO: Parse PNG header to get dimensions
    return ClipboardData::from_image(image_result.value(), 0, 0);
  }

  // Empty clipboard
  return ClipboardData();
}

Result<void> platform_set_clipboard(const ClipboardData &data) {
  using namespace platform;

  if (data.is_empty()) {
    return Result<void>::ok();
  }

  switch (data.type) {
  case ClipboardType::Text:
  case ClipboardType::Url:
  case ClipboardType::RichText:
    return write_clipboard_text(data.get_text());

  case ClipboardType::Image:
    return write_clipboard_image(data.data);

  case ClipboardType::Files:
    // Convert file list to text (newline-separated paths)
    return write_clipboard_text(data.get_text());

  default:
    return Error(ErrorCode::NotSupported, "Unsupported clipboard type");
  }
}

Result<void> platform_register_hotkey(const std::string &hotkey,
                                      std::function<void()> callback) {
  return platform::register_hotkey(hotkey, std::move(callback));
}

void platform_unregister_hotkey() { platform::unregister_hotkey(); }

} // namespace seadrop

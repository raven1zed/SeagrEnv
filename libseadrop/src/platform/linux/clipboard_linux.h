/**
 * @file clipboard_linux.h
 * @brief Linux clipboard types and internal declarations
 *
 * Internal types for X11/Wayland clipboard access.
 */

#ifndef SEADROP_PLATFORM_LINUX_CLIPBOARD_LINUX_H
#define SEADROP_PLATFORM_LINUX_CLIPBOARD_LINUX_H

#include "seadrop/clipboard.h"
#include "seadrop/error.h"
#include <string>

namespace seadrop {
namespace platform {

/**
 * @brief Clipboard backend type
 */
enum class ClipboardBackend {
  Unknown,
  X11,
  Wayland,
  Fallback // wl-clipboard / xclip command-line tools
};

/**
 * @brief Detect the current display server
 */
ClipboardBackend detect_display_server();

/**
 * @brief Read text from the clipboard
 */
Result<std::string> read_clipboard_text();

/**
 * @brief Write text to the clipboard
 */
Result<void> write_clipboard_text(const std::string &text);

/**
 * @brief Read image from clipboard (PNG format)
 */
Result<Bytes> read_clipboard_image();

/**
 * @brief Write image to clipboard (PNG format)
 */
Result<void> write_clipboard_image(const Bytes &png_data);

/**
 * @brief Register a global hotkey
 * @param key_string Key combination (e.g., "Ctrl+Shift+V")
 * @param callback Function to call when hotkey is pressed
 */
Result<void> register_hotkey(const std::string &key_string,
                             std::function<void()> callback);

/**
 * @brief Unregister the global hotkey
 */
void unregister_hotkey();

} // namespace platform
} // namespace seadrop

#endif // SEADROP_PLATFORM_LINUX_CLIPBOARD_LINUX_H

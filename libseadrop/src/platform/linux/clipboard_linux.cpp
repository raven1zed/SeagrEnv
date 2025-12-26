/**
 * @file clipboard_linux.cpp
 * @brief Linux clipboard implementation
 *
 * Implements clipboard access using command-line tools (xclip, xsel, wl-copy)
 * for maximum compatibility across different Linux environments.
 */

#include "clipboard_linux.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace seadrop {
namespace platform {

// ============================================================================
// Display Server Detection
// ============================================================================

ClipboardBackend detect_display_server() {
  // Check for Wayland
  const char *wayland = std::getenv("WAYLAND_DISPLAY");
  if (wayland && wayland[0] != '\0') {
    return ClipboardBackend::Wayland;
  }

  // Check for X11
  const char *display = std::getenv("DISPLAY");
  if (display && display[0] != '\0') {
    return ClipboardBackend::X11;
  }

  // Fallback (might be headless or unknown)
  return ClipboardBackend::Fallback;
}

// ============================================================================
// Command Execution Helper
// ============================================================================

namespace {

/**
 * @brief Execute a command and capture its output
 */
Result<std::string> execute_command(const std::string &cmd) {
  std::array<char, 4096> buffer;
  std::string result;

  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                pclose);
  if (!pipe) {
    return Error(ErrorCode::PlatformError, "Failed to execute command: " + cmd);
  }

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }

  return result;
}

/**
 * @brief Execute a command with input data
 */
Result<void> execute_command_with_input(const std::string &cmd,
                                        const std::string &input) {
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "w"),
                                                pclose);
  if (!pipe) {
    return Error(ErrorCode::PlatformError, "Failed to execute command: " + cmd);
  }

  if (fwrite(input.data(), 1, input.size(), pipe.get()) != input.size()) {
    return Error(ErrorCode::PlatformError, "Failed to write to pipe");
  }

  return Result<void>::ok();
}

/**
 * @brief Check if a command exists
 */
bool command_exists(const char *cmd) {
  std::string check = "command -v ";
  check += cmd;
  check += " >/dev/null 2>&1";
  return system(check.c_str()) == 0;
}

} // namespace

// ============================================================================
// Clipboard Operations
// ============================================================================

Result<std::string> read_clipboard_text() {
  auto backend = detect_display_server();

  std::string cmd;

  if (backend == ClipboardBackend::Wayland) {
    // Try wl-paste for Wayland
    if (command_exists("wl-paste")) {
      cmd = "wl-paste --no-newline 2>/dev/null";
    } else {
      return Error(ErrorCode::NotSupported,
                   "wl-paste not found. Install wl-clipboard package.");
    }
  } else if (backend == ClipboardBackend::X11) {
    // Try xclip or xsel for X11
    if (command_exists("xclip")) {
      cmd = "xclip -selection clipboard -o 2>/dev/null";
    } else if (command_exists("xsel")) {
      cmd = "xsel --clipboard --output 2>/dev/null";
    } else {
      return Error(ErrorCode::NotSupported,
                   "xclip or xsel not found. Install one of them.");
    }
  } else {
    return Error(ErrorCode::NotSupported,
                 "No display server detected (headless mode?)");
  }

  return execute_command(cmd);
}

Result<void> write_clipboard_text(const std::string &text) {
  auto backend = detect_display_server();

  std::string cmd;

  if (backend == ClipboardBackend::Wayland) {
    if (command_exists("wl-copy")) {
      cmd = "wl-copy 2>/dev/null";
    } else {
      return Error(ErrorCode::NotSupported,
                   "wl-copy not found. Install wl-clipboard package.");
    }
  } else if (backend == ClipboardBackend::X11) {
    if (command_exists("xclip")) {
      cmd = "xclip -selection clipboard 2>/dev/null";
    } else if (command_exists("xsel")) {
      cmd = "xsel --clipboard --input 2>/dev/null";
    } else {
      return Error(ErrorCode::NotSupported,
                   "xclip or xsel not found. Install one of them.");
    }
  } else {
    return Error(ErrorCode::NotSupported,
                 "No display server detected (headless mode?)");
  }

  return execute_command_with_input(cmd, text);
}

Result<Bytes> read_clipboard_image() {
  auto backend = detect_display_server();

  std::string cmd;

  if (backend == ClipboardBackend::Wayland) {
    if (command_exists("wl-paste")) {
      cmd = "wl-paste --type image/png 2>/dev/null";
    } else {
      return Error(ErrorCode::NotSupported, "wl-paste not found");
    }
  } else if (backend == ClipboardBackend::X11) {
    if (command_exists("xclip")) {
      cmd = "xclip -selection clipboard -t image/png -o 2>/dev/null";
    } else {
      return Error(ErrorCode::NotSupported, "xclip not found");
    }
  } else {
    return Error(ErrorCode::NotSupported, "No display server detected");
  }

  auto result = execute_command(cmd);
  if (result.is_error()) {
    return result.error();
  }

  const auto &str = result.value();
  return Bytes(str.begin(), str.end());
}

Result<void> write_clipboard_image(const Bytes &png_data) {
  auto backend = detect_display_server();

  std::string cmd;

  if (backend == ClipboardBackend::Wayland) {
    if (command_exists("wl-copy")) {
      cmd = "wl-copy --type image/png 2>/dev/null";
    } else {
      return Error(ErrorCode::NotSupported, "wl-copy not found");
    }
  } else if (backend == ClipboardBackend::X11) {
    if (command_exists("xclip")) {
      cmd = "xclip -selection clipboard -t image/png 2>/dev/null";
    } else {
      return Error(ErrorCode::NotSupported, "xclip not found");
    }
  } else {
    return Error(ErrorCode::NotSupported, "No display server detected");
  }

  std::string data(png_data.begin(), png_data.end());
  return execute_command_with_input(cmd, data);
}

// ============================================================================
// Hotkey Registration (Stub)
// ============================================================================

Result<void> register_hotkey(const std::string &key_string,
                             std::function<void()> callback) {
  SEADROP_UNUSED(key_string);
  SEADROP_UNUSED(callback);

  // Hotkey registration requires either:
  // 1. X11: XGrabKey API
  // 2. Wayland: Compositor-specific protocols
  // 3. Desktop environment APIs (GNOME, KDE, etc.)
  //
  // For now, we return NotSupported. Users can configure hotkeys
  // through their desktop environment settings.

  return Error(
      ErrorCode::NotSupported,
      "Global hotkey registration requires desktop environment support. "
      "Configure Ctrl+Shift+V in your system keyboard shortcuts.");
}

void unregister_hotkey() {
  // Nothing to do for stub implementation
}

} // namespace platform
} // namespace seadrop

/**
 * @file clipboard.h
 * @brief P2P Clipboard sharing for SeaDrop
 *
 * SeaDrop provides completely offline, peer-to-peer clipboard sharing.
 * Key features:
 * - Explicit push (tap button or hotkey)
 * - Optional auto-share in Zone 1 (Intimate distance)
 * - Support for text, URLs, and images
 * - User-initiated only (no automatic sync)
 *
 * This is NOT clipboard sync. The user explicitly decides when to share.
 */

#ifndef SEADROP_CLIPBOARD_H
#define SEADROP_CLIPBOARD_H

#include "device.h"
#include "distance.h"
#include "error.h"
#include "platform.h"
#include "types.h"
#include <chrono>
#include <functional>
#include <memory>
#include <vector>


namespace seadrop {

// ============================================================================
// Clipboard Content Types
// ============================================================================

/**
 * @brief Type of clipboard content
 */
enum class ClipboardType : uint8_t {
  /// Empty clipboard
  Empty = 0,

  /// Plain text
  Text = 1,

  /// URL (treated specially for preview)
  Url = 2,

  /// Rich text (HTML)
  RichText = 3,

  /// Image (PNG format)
  Image = 4,

  /// File paths (list of files)
  Files = 5,

  /// Unknown/unsupported format
  Unknown = 255
};

/**
 * @brief Get human-readable name for clipboard type
 */
SEADROP_API const char *clipboard_type_name(ClipboardType type);

// ============================================================================
// Clipboard Data
// ============================================================================

/**
 * @brief Clipboard content ready for transfer
 */
struct ClipboardData {
  /// Content type
  ClipboardType type = ClipboardType::Empty;

  /// Raw data bytes
  Bytes data;

  /// Preview text (first 100 chars for notification)
  std::string preview;

  /// MIME type (for images)
  std::string mime_type;

  /// Image dimensions (if type == Image)
  struct ImageInfo {
    int width = 0;
    int height = 0;
    int channels = 0; // 3 = RGB, 4 = RGBA
  } image_info;

  /// File paths (if type == Files)
  std::vector<std::string> file_paths;

  /// Timestamp when captured
  std::chrono::system_clock::time_point captured_at;

  // ========================================================================
  // Helper Methods
  // ========================================================================

  /// Check if clipboard is empty
  bool is_empty() const { return type == ClipboardType::Empty || data.empty(); }

  /// Get size in bytes
  size_t size() const { return data.size(); }

  /// Get text content (for Text/Url/RichText types)
  std::string get_text() const;

  /// Create from plain text
  static ClipboardData from_text(const std::string &text);

  /// Create from URL
  static ClipboardData from_url(const std::string &url);

  /// Create from image data
  static ClipboardData from_image(const Bytes &png_data, int width, int height);
};

// ============================================================================
// Received Clipboard
// ============================================================================

/**
 * @brief Information about received clipboard content
 */
struct ReceivedClipboard {
  /// The clipboard data
  ClipboardData data;

  /// Sender device info
  Device sender;

  /// When received
  std::chrono::system_clock::time_point received_at;

  /// Whether this was auto-received (zone 1 auto-share)
  bool auto_received = false;

  /// Has user applied this to local clipboard?
  bool applied = false;
};

// ============================================================================
// Clipboard Configuration
// ============================================================================

/**
 * @brief Configuration for clipboard sharing
 */
struct ClipboardConfig {
  /// Enable auto-share when in Zone 1 (Intimate)
  /// This is opt-in, disabled by default
  bool auto_share_enabled = false;

  /// Share text content
  bool share_text = true;

  /// Share URL content
  bool share_urls = true;

  /// Share image content
  bool share_images = true;

  /// Maximum image size to share (bytes, 0 = unlimited)
  size_t max_image_size = 10 * 1024 * 1024; // 10 MB

  /// Auto-apply received clipboard to local clipboard
  bool auto_apply_received = false;

  /// Show notification when clipboard received
  bool notify_on_receive = true;

  /// Hotkey for pushing clipboard (platform-specific)
  std::string push_hotkey = "Ctrl+Shift+V";
};

// ============================================================================
// Clipboard Manager
// ============================================================================

/**
 * @brief Manages P2P clipboard sharing for SeaDrop
 *
 * The ClipboardManager handles:
 * - Reading local clipboard content
 * - Pushing clipboard to connected/trusted devices
 * - Receiving clipboard from other devices
 * - Auto-share in Zone 1 (when enabled)
 *
 * Example usage:
 * @code
 *   ClipboardManager clipboard;
 *   clipboard.init(config);
 *
 *   clipboard.on_received([](const ReceivedClipboard& data) {
 *       show_notification("Clipboard from " + data.sender.name);
 *   });
 *
 *   // Explicit push
 *   clipboard.push_to_device(target_device);
 * @endcode
 */
class SEADROP_API ClipboardManager {
public:
  ClipboardManager();
  ~ClipboardManager();

  // Non-copyable
  ClipboardManager(const ClipboardManager &) = delete;
  ClipboardManager &operator=(const ClipboardManager &) = delete;

  // ========================================================================
  // Initialization
  // ========================================================================

  /**
   * @brief Initialize the clipboard manager
   * @param config Clipboard configuration
   * @return Success or error
   */
  Result<void> init(const ClipboardConfig &config = {});

  /**
   * @brief Shutdown
   */
  void shutdown();

  // ========================================================================
  // Local Clipboard
  // ========================================================================

  /**
   * @brief Get current local clipboard content
   * @return Clipboard data or error
   */
  Result<ClipboardData> get_local_clipboard() const;

  /**
   * @brief Set local clipboard content (from received data)
   * @param data Data to set
   * @return Success or error
   */
  Result<void> set_local_clipboard(const ClipboardData &data);

  // ========================================================================
  // Sending Clipboard
  // ========================================================================

  /**
   * @brief Push current clipboard to a specific device
   * @param device Target device (must be connected)
   * @return Success or error
   *
   * This is the explicit "Share Clipboard" action.
   */
  Result<void> push_to_device(const Device &device);

  /**
   * @brief Push current clipboard to all connected trusted devices
   * @return Number of devices sent to, or error
   *
   * Only sends to devices that are currently connected.
   */
  Result<int> push_to_all_trusted();

  /**
   * @brief Send specific clipboard data to a device
   * @param device Target device
   * @param data Clipboard data to send
   * @return Success or error
   */
  Result<void> send_clipboard(const Device &device, const ClipboardData &data);

  // ========================================================================
  // Auto-Share (Zone 1 Only)
  // ========================================================================

  /**
   * @brief Enable or disable auto-share
   * @param enabled Enable auto-share when in Zone 1
   *
   * When enabled, clipboard changes are automatically pushed to
   * connected trusted devices that are in the Intimate zone.
   */
  void set_auto_share(bool enabled);

  /**
   * @brief Check if auto-share is enabled
   */
  bool is_auto_share_enabled() const;

  /**
   * @brief Trigger auto-share check (called when zone changes)
   *
   * This is typically called by the DistanceMonitor when a device
   * enters Zone 1. It will push clipboard if auto-share is enabled.
   */
  void trigger_auto_share_check();

  // ========================================================================
  // Receive History
  // ========================================================================

  /**
   * @brief Get list of recently received clipboards
   * @param limit Maximum number to return (0 = all)
   */
  std::vector<ReceivedClipboard> get_receive_history(size_t limit = 10) const;

  /**
   * @brief Apply received clipboard to local clipboard
   * @param index Index in receive history
   * @return Success or error
   */
  Result<void> apply_received(size_t index);

  /**
   * @brief Clear receive history
   */
  void clear_history();

  // ========================================================================
  // Configuration
  // ========================================================================

  /**
   * @brief Update configuration
   */
  void set_config(const ClipboardConfig &config);

  /**
   * @brief Get current configuration
   */
  ClipboardConfig get_config() const;

  // ========================================================================
  // Callbacks
  // ========================================================================

  /**
   * @brief Set callback for received clipboard
   */
  void on_received(std::function<void(const ReceivedClipboard &)> callback);

  /**
   * @brief Set callback for clipboard send success
   */
  void on_sent(std::function<void(const Device &)> callback);

  /**
   * @brief Set callback for errors
   */
  void on_error(std::function<void(const Error &)> callback);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Platform Helpers
// ============================================================================

/**
 * @brief Check if clipboard access is available
 */
SEADROP_API bool is_clipboard_available();

/**
 * @brief Register global hotkey for clipboard push
 * @param hotkey Hotkey string (e.g., "Ctrl+Shift+V")
 * @param callback Function to call when hotkey pressed
 * @return Success or error
 */
SEADROP_API Result<void>
register_clipboard_hotkey(const std::string &hotkey,
                          std::function<void()> callback);

/**
 * @brief Unregister clipboard hotkey
 */
SEADROP_API void unregister_clipboard_hotkey();

} // namespace seadrop

#endif // SEADROP_CLIPBOARD_H

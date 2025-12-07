/**
 * @file config.h
 * @brief User configuration and settings for SeaDrop
 */

#ifndef SEADROP_CONFIG_H
#define SEADROP_CONFIG_H

#include "clipboard.h"
#include "distance.h"
#include "error.h"
#include "platform.h"
#include "transfer.h"
#include "types.h"
#include <filesystem>
#include <memory>
#include <string>


namespace seadrop {

// ============================================================================
// User Configuration
// ============================================================================

/**
 * @brief Complete user configuration for SeaDrop
 */
struct SeaDropConfig {
  // ========================================================================
  // Identity
  // ========================================================================

  /// Device display name (shown to other devices)
  std::string device_name;

  // ========================================================================
  // Visibility
  // ========================================================================

  /// Visibility mode
  VisibilityMode visibility = VisibilityMode::Everyone;

  /// How to handle incoming transfers
  ReceptionMode reception = ReceptionMode::AlwaysAsk;

  // ========================================================================
  // File Transfer
  // ========================================================================

  /// Where to save received files
  std::filesystem::path download_path;

  /// Create subdirectory per sender
  bool use_sender_subdir = true;

  /// How to handle filename conflicts
  ConflictResolution conflict_resolution = ConflictResolution::AutoRename;

  /// Verify file checksums after transfer
  bool verify_checksums = true;

  /// Maximum file size to accept (0 = unlimited)
  uint64_t max_file_size = 0;

  /// Maximum number of files per transfer
  uint32_t max_files_per_transfer = 1000;

  // ========================================================================
  // Distance Zones
  // ========================================================================

  /// Zone distance thresholds
  ZoneThresholds zone_thresholds;

  /// Enable distance-based trust zones
  bool enable_distance_zones = true;

  /// Show security alerts for zone changes
  bool show_zone_alerts = true;

  // ========================================================================
  // Clipboard
  // ========================================================================

  /// Clipboard sharing configuration
  ClipboardConfig clipboard;

  // ========================================================================
  // Connectivity
  // ========================================================================

  /// Enable WiFi Direct (primary transfer method)
  bool enable_wifi_direct = true;

  /// Enable Bluetooth (for discovery)
  bool enable_bluetooth = true;

  /// TCP port for data transfer
  uint16_t tcp_port = 17530;

  // ========================================================================
  // Security
  // ========================================================================

  /// Require encryption for all transfers
  bool require_encryption = true;

  /// Pairing timeout in seconds
  int pairing_timeout_seconds = 60;

  // ========================================================================
  // Notifications
  // ========================================================================

  /// Show toast notifications for received files
  bool show_notifications = true;

  /// Toast auto-dismiss time in seconds (0 = don't auto-dismiss)
  int toast_duration_seconds = 3;

  /// Play sound on file received
  bool play_sound = true;

  // ========================================================================
  // Appearance (Desktop)
  // ========================================================================

  /// Dark mode
  bool dark_mode = true;

  /// Start minimized to tray
  bool start_minimized = false;

  /// Close to tray instead of exiting
  bool close_to_tray = true;

  // ========================================================================
  // Data Paths
  // ========================================================================

  /// Path to configuration file
  std::filesystem::path config_file_path;

  /// Path to trust database
  std::filesystem::path database_path;

  /// Path to log files
  std::filesystem::path log_path;

  // ========================================================================
  // Methods
  // ========================================================================

  /// Load defaults based on platform
  void load_defaults();

  /// Validate configuration
  Result<void> validate() const;

  /// Get default download path for platform
  static std::filesystem::path get_default_download_path();

  /// Get default config directory for platform
  static std::filesystem::path get_default_config_dir();
};

// ============================================================================
// Configuration Manager
// ============================================================================

/**
 * @brief Manages loading, saving, and validating configuration
 */
class SEADROP_API ConfigManager {
public:
  ConfigManager();
  ~ConfigManager();

  // Non-copyable
  ConfigManager(const ConfigManager &) = delete;
  ConfigManager &operator=(const ConfigManager &) = delete;

  // ========================================================================
  // Initialization
  // ========================================================================

  /**
   * @brief Initialize with config file path
   * @param config_path Path to config file (created if doesn't exist)
   * @return Success or error
   */
  Result<void> init(const std::filesystem::path &config_path = {});

  // ========================================================================
  // Configuration Access
  // ========================================================================

  /**
   * @brief Get current configuration
   */
  const SeaDropConfig &get() const;

  /**
   * @brief Get mutable configuration reference
   *
   * After modifying, call save() to persist changes.
   */
  SeaDropConfig &get_mutable();

  /**
   * @brief Set entire configuration
   */
  Result<void> set(const SeaDropConfig &config);

  // ========================================================================
  // Persistence
  // ========================================================================

  /**
   * @brief Load configuration from file
   */
  Result<void> load();

  /**
   * @brief Save configuration to file
   */
  Result<void> save();

  /**
   * @brief Reset to defaults
   */
  void reset_defaults();

  // ========================================================================
  // Individual Settings
  // ========================================================================

  /// Set device name
  Result<void> set_device_name(const std::string &name);

  /// Set visibility mode
  Result<void> set_visibility(VisibilityMode mode);

  /// Set reception mode
  Result<void> set_reception(ReceptionMode mode);

  /// Set download path
  Result<void> set_download_path(const std::filesystem::path &path);

  /// Set zone thresholds
  Result<void> set_zone_thresholds(const ZoneThresholds &thresholds);

  /// Set auto-clipboard
  Result<void> set_auto_clipboard(bool enabled);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace seadrop

#endif // SEADROP_CONFIG_H

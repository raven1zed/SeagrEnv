/**
 * @file config.cpp
 * @brief Configuration management implementation
 */

// Standard library includes FIRST, before any project headers
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

// Platform includes
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <shlobj.h>
#include <windows.h>

#else
#include <pwd.h>
#include <unistd.h>
#endif

// Project includes LAST
#include "seadrop/config.h"

// Namespace alias to avoid pollution issues
namespace fs = ::std::filesystem;

namespace seadrop {

// GCC 15 workaround removed

// ============================================================================
// SeaDropConfig Methods
// ============================================================================

void SeaDropConfig::load_defaults() {
  device_name = ""; // Will use get_default_device_name()
  visibility = VisibilityMode::Everyone;
  reception = ReceptionMode::AlwaysAsk;

  download_path = get_default_download_path();
  use_sender_subdir = true;
  conflict_resolution = ConflictResolution::AutoRename;
  verify_checksums = true;
  max_file_size = 0; // Unlimited
  max_files_per_transfer = 1000;

  zone_thresholds.reset_defaults();
  enable_distance_zones = true;
  show_zone_alerts = true;

  clipboard.auto_share_enabled = false;
  clipboard.share_text = true;
  clipboard.share_urls = true;
  clipboard.share_images = true;

  enable_wifi_direct = true;
  enable_bluetooth = true;
  tcp_port = 17530;

  require_encryption = true;
  pairing_timeout_seconds = 60;

  show_notifications = true;
  toast_duration_seconds = 3;
  play_sound = true;

  dark_mode = true;
  start_minimized = false;
  close_to_tray = true;

  auto config_dir = get_default_config_dir();
#ifdef _WIN32
  config_file_path = config_dir / L"config.json";
  database_path = config_dir / L"seadrop.db";
  log_path = config_dir / L"logs";
#else
  config_file_path = config_dir / "config.json";
  database_path = config_dir / "seadrop.db";
  log_path = config_dir / "logs";
#endif
}

Result<void> SeaDropConfig::validate() const {
  if (device_name.length() > 64) {
    return Error(ErrorCode::InvalidArgument,
                 "Device name too long (max 64 chars)");
  }

  if (!zone_thresholds.is_valid()) {
    return Error(ErrorCode::InvalidArgument, "Invalid zone thresholds");
  }

  if (tcp_port > 0 && tcp_port < 1024) {
    return Error(ErrorCode::InvalidArgument,
                 "TCP port must be >= 1024 or 0 for auto");
  }

  return Result<void>::ok();
}

fs::path SeaDropConfig::get_default_download_path() {
#ifdef _WIN32
  wchar_t pathBuf[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, pathBuf))) {
    fs::path result(pathBuf);
    result = result / L"Downloads" / L"SeaDrop";
    return result;
  }
  return fs::path(L"C:\\Users\\Downloads\\SeaDrop");
#else
  const char *home = std::getenv("HOME");
  if (!home) {
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
      home = pw->pw_dir;
    }
  }
  if (home) {
    fs::path result(home);
    result = result / "Downloads" / "SeaDrop";
    return result;
  }
  return fs::path("/tmp/SeaDrop");
#endif
}

fs::path SeaDropConfig::get_default_config_dir() {
#ifdef _WIN32
  wchar_t pathBuf[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, pathBuf))) {
    fs::path result(pathBuf);
    result = result / L"SeaDrop";
    return result;
  }
  return fs::path(L"C:\\ProgramData\\SeaDrop");
#elif defined(__APPLE__)
  const char *home = std::getenv("HOME");
  if (home) {
    fs::path result(home);
    result = result / "Library" / "Application Support" / "SeaDrop";
    return result;
  }
  return fs::path("/tmp/SeaDrop");
#else
  // Linux: Use XDG_CONFIG_HOME or ~/.config
  const char *xdg_config = std::getenv("XDG_CONFIG_HOME");
  if (xdg_config) {
    fs::path result(xdg_config);
    result = result / "seadrop";
    return result;
  }

  const char *home = std::getenv("HOME");
  if (home) {
    fs::path result(home);
    result = result / make_path(".config") / make_path("seadrop");
    return result;
  }

  return fs::path("/tmp/seadrop");
#endif
}

// ============================================================================
// ConfigManager Implementation
// ============================================================================

class ConfigManager::Impl {
public:
  SeaDropConfig config;
  fs::path config_path;
  std::mutex mutex;
  bool initialized = false;
};

ConfigManager::ConfigManager() : impl_(std::make_unique<Impl>()) {
  impl_->config.load_defaults();
}

ConfigManager::~ConfigManager() = default;

Result<void> ConfigManager::init(const fs::path &config_path) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (config_path.empty()) {
    impl_->config_path = SeaDropConfig::get_default_config_dir();
#ifdef _WIN32
    impl_->config_path = impl_->config_path / L"config.json";
#else
    impl_->config_path = impl_->config_path / "config.json";
#endif
  } else {
    impl_->config_path = config_path;
  }

  // Create config directory if needed
  auto dir = impl_->config_path.parent_path();
  if (!dir.empty()) {
    ::std::error_code ec;
    if (!fs::exists(dir, ec)) {
      fs::create_directories(dir, ec);
      // Ignore errors - non-fatal
    }
  }

  // Try to load existing config
  {
    ::std::error_code ec;
    if (fs::exists(impl_->config_path, ec)) {
      auto result = load();
      if (result.is_error()) {
        // Use defaults, log warning
      }
    }
  }

  impl_->initialized = true;
  return Result<void>::ok();
}

const SeaDropConfig &ConfigManager::get() const { return impl_->config; }

SeaDropConfig &ConfigManager::get_mutable() { return impl_->config; }

Result<void> ConfigManager::set(const SeaDropConfig &config) {
  auto validation = config.validate();
  if (validation.is_error()) {
    return validation;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config = config;
  return Result<void>::ok();
}

Result<void> ConfigManager::load() {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  // TODO: Parse JSON config file
  // For now, just use defaults
  impl_->config.load_defaults();

  return Result<void>::ok();
}

Result<void> ConfigManager::save() {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  // TODO: Write JSON config file

  return Result<void>::ok();
}

void ConfigManager::reset_defaults() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config.load_defaults();
}

Result<void> ConfigManager::set_device_name(const std::string &name) {
  if (name.empty() || name.length() > 64) {
    return Error(ErrorCode::InvalidArgument, "Invalid device name");
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config.device_name = name;
  return save();
}

Result<void> ConfigManager::set_visibility(VisibilityMode mode) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config.visibility = mode;
  return save();
}

Result<void> ConfigManager::set_reception(ReceptionMode mode) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config.reception = mode;
  return save();
}

Result<void> ConfigManager::set_download_path(const fs::path &path) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config.download_path = path;
  return save();
}

Result<void>
ConfigManager::set_zone_thresholds(const ZoneThresholds &thresholds) {
  if (!thresholds.is_valid()) {
    return Error(ErrorCode::InvalidArgument, "Invalid zone thresholds");
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config.zone_thresholds = thresholds;
  return save();
}

Result<void> ConfigManager::set_auto_clipboard(bool enabled) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config.clipboard.auto_share_enabled = enabled;
  return save();
}

} // namespace seadrop

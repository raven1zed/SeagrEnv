/**
 * @file distance.h
 * @brief Distance-based trust zone monitoring for SeaDrop
 *
 * This module provides RSSI-based distance estimation and trust zone
 * management. It's a key security feature that adjusts permissions
 * based on physical proximity between devices.
 *
 * Trust Zones:
 *   INTIMATE (0-3m):   Full trust, all auto-accepted, clipboard auto-share
 *   CLOSE (3-10m):     Auto-accept with toast notification
 *   NEARBY (10-30m):   Small files auto, large files need confirmation
 *   FAR (30m+):        Always confirm, security alert shown
 */

#ifndef SEADROP_DISTANCE_H
#define SEADROP_DISTANCE_H

#include "error.h"
#include "platform.h"
#include "types.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>


namespace seadrop {

// ============================================================================
// Trust Zones
// ============================================================================

/**
 * @brief Trust zone based on physical distance
 *
 * Each zone has different permission levels for file transfers and
 * clipboard sharing. Zone boundaries are user-configurable.
 */
enum class TrustZone : uint8_t {
  /// 0-3 meters: Maximum trust, all operations allowed without prompts
  Intimate = 0,

  /// 3-10 meters: High trust, auto-accept with subtle notification
  Close = 1,

  /// 10-30 meters: Medium trust, prompts for large files
  Nearby = 2,

  /// 30+ meters: Low trust, all transfers require confirmation
  Far = 3,

  /// Unable to determine distance (connection lost, etc.)
  Unknown = 255
};

/**
 * @brief Get human-readable name for trust zone
 */
SEADROP_API const char *trust_zone_name(TrustZone zone);

// ============================================================================
// Distance Information
// ============================================================================

/**
 * @brief Raw RSSI reading with timestamp
 */
struct RssiReading {
  int rssi_dbm; // Raw RSSI in dBm (-100 to 0)
  std::chrono::steady_clock::time_point timestamp; // When reading was taken
  bool is_bluetooth;                               // BLE vs WiFi Direct RSSI
};

/**
 * @brief Processed distance information
 */
struct DistanceInfo {
  // RSSI data
  int rssi_dbm = -100;      // Raw RSSI value in dBm
  int rssi_smoothed = -100; // Smoothed RSSI (moving average)

  // Distance estimation
  float distance_meters = 999.0f; // Estimated distance in meters
  TrustZone zone = TrustZone::Unknown;

  // Signal quality for UI
  int signal_bars = 0; // 1-4 bars for display

  // Confidence & stability
  float confidence = 0.0f; // 0.0-1.0, how reliable the estimate is
  bool is_stable = false;  // True if zone hasn't changed recently

  // Timing
  std::chrono::steady_clock::time_point last_update;
  std::chrono::milliseconds age() const;
};

// ============================================================================
// Zone Thresholds Configuration
// ============================================================================

/**
 * @brief User-configurable zone boundary thresholds
 */
struct ZoneThresholds {
  // Distance thresholds in meters (default values)
  float intimate_max = 3.0f; // Zone 1: 0 to intimate_max
  float close_max = 10.0f;   // Zone 2: intimate_max to close_max
  float nearby_max = 30.0f;  // Zone 3: close_max to nearby_max
                             // Zone 4: beyond nearby_max

  // RSSI thresholds in dBm (calculated from distance, but can be overridden)
  int intimate_rssi = -55; // Stronger than this = Intimate
  int close_rssi = -70;    // Stronger than this = Close
  int nearby_rssi = -85;   // Stronger than this = Nearby
                           // Weaker than nearby_rssi = Far

  // Validation
  bool is_valid() const {
    return intimate_max > 0 && close_max > intimate_max &&
           nearby_max > close_max;
  }

  // Reset to defaults
  void reset_defaults();

  // Calculate RSSI thresholds from distance thresholds
  void calculate_rssi_from_distance();
};

// ============================================================================
// Zone Change Events
// ============================================================================

/**
 * @brief Information about a zone change event
 */
struct ZoneChangeEvent {
  DeviceId device_id;
  TrustZone previous_zone;
  TrustZone current_zone;
  DistanceInfo distance_info;
  bool is_moving_closer;        // True if device is approaching
  bool requires_security_alert; // True if unexpected zone change
  std::chrono::steady_clock::time_point timestamp;
};

/// Callback for zone changes
using ZoneChangedCallback = std::function<void(const ZoneChangeEvent &event)>;

/// Callback for distance updates (called frequently)
using DistanceUpdatedCallback =
    std::function<void(const DeviceId &device, const DistanceInfo &info)>;

/// Callback for security alerts
using SecurityAlertCallback =
    std::function<void(const DeviceId &device, const std::string &message)>;

// ============================================================================
// Distance Monitor Class
// ============================================================================

/**
 * @brief Monitors RSSI and calculates trust zones for connected devices
 *
 * The DistanceMonitor continuously reads RSSI from BLE/WiFi Direct
 * connections, applies smoothing algorithms, and maps the signal
 * strength to trust zones.
 *
 * Example usage:
 * @code
 *   DistanceMonitor monitor;
 *   monitor.set_zone_thresholds(thresholds);
 *   monitor.on_zone_changed([](const ZoneChangeEvent& e) {
 *       if (e.current_zone == TrustZone::Far) {
 *           show_security_alert();
 *       }
 *   });
 *   monitor.start();
 * @endcode
 */
class SEADROP_API DistanceMonitor {
public:
  DistanceMonitor();
  ~DistanceMonitor();

  // Non-copyable, movable
  DistanceMonitor(const DistanceMonitor &) = delete;
  DistanceMonitor &operator=(const DistanceMonitor &) = delete;
  DistanceMonitor(DistanceMonitor &&) noexcept;
  DistanceMonitor &operator=(DistanceMonitor &&) noexcept;

  // ========================================================================
  // Lifecycle
  // ========================================================================

  /**
   * @brief Start monitoring RSSI for all connected devices
   * @return Success or error
   */
  Result<void> start();

  /**
   * @brief Stop monitoring
   */
  void stop();

  /**
   * @brief Check if monitoring is active
   */
  bool is_running() const;

  // ========================================================================
  // Configuration
  // ========================================================================

  /**
   * @brief Set zone boundary thresholds
   * @param thresholds New threshold configuration
   * @return Success or error if invalid
   */
  Result<void> set_zone_thresholds(const ZoneThresholds &thresholds);

  /**
   * @brief Get current zone thresholds
   */
  ZoneThresholds get_zone_thresholds() const;

  /**
   * @brief Set RSSI smoothing window size
   * @param samples Number of samples to average (default: 5)
   */
  void set_smoothing_window(int samples);

  /**
   * @brief Set minimum time between zone change notifications
   * @param duration Hysteresis duration (default: 2 seconds)
   */
  void set_zone_change_hysteresis(std::chrono::milliseconds duration);

  // ========================================================================
  // Distance Queries
  // ========================================================================

  /**
   * @brief Get current distance info for a device
   * @param device_id Device to query
   * @return Distance info or error if device not found
   */
  Result<DistanceInfo> get_distance(const DeviceId &device_id) const;

  /**
   * @brief Get current trust zone for a device
   * @param device_id Device to query
   * @return Trust zone or Unknown if device not found
   */
  TrustZone get_zone(const DeviceId &device_id) const;

  /**
   * @brief Check if a device is within a specific zone or closer
   * @param device_id Device to query
   * @param zone Maximum zone to check
   * @return True if device is in this zone or closer
   */
  bool is_within_zone(const DeviceId &device_id, TrustZone zone) const;

  // ========================================================================
  // RSSI Input (Called by Connection Layer)
  // ========================================================================

  /**
   * @brief Feed a new RSSI reading into the monitor
   * @param device_id Device the reading is from
   * @param reading RSSI reading with timestamp
   *
   * This is called by the connection layer whenever a new RSSI
   * value is obtained from BLE or WiFi Direct.
   */
  void feed_rssi(const DeviceId &device_id, const RssiReading &reading);

  /**
   * @brief Remove a device from monitoring (when disconnected)
   */
  void remove_device(const DeviceId &device_id);

  // ========================================================================
  // Callbacks
  // ========================================================================

  /**
   * @brief Set callback for zone changes
   */
  void on_zone_changed(ZoneChangedCallback callback);

  /**
   * @brief Set callback for distance updates
   */
  void on_distance_updated(DistanceUpdatedCallback callback);

  /**
   * @brief Set callback for security alerts
   */
  void on_security_alert(SecurityAlertCallback callback);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Convert RSSI to estimated distance in meters
 *
 * Uses the log-distance path loss model:
 *   distance = 10 ^ ((TxPower - RSSI) / (10 * n))
 *
 * @param rssi_dbm RSSI value in dBm
 * @param tx_power Measured RSSI at 1 meter (default: -59 dBm)
 * @param path_loss_exp Path loss exponent (default: 2.0 for free space)
 * @return Estimated distance in meters
 */
SEADROP_API float rssi_to_distance(int rssi_dbm, int tx_power = -59,
                                   float path_loss_exp = 2.0f);

/**
 * @brief Convert distance to expected RSSI
 *
 * Inverse of rssi_to_distance().
 *
 * @param distance_m Distance in meters
 * @param tx_power Measured RSSI at 1 meter (default: -59 dBm)
 * @param path_loss_exp Path loss exponent (default: 2.0 for free space)
 * @return Expected RSSI in dBm
 */
SEADROP_API int distance_to_rssi(float distance_m, int tx_power = -59,
                                 float path_loss_exp = 2.0f);

/**
 * @brief Map RSSI to signal bars (1-4)
 *
 * @param rssi_dbm RSSI value in dBm
 * @return Signal bars: 4 (excellent), 3 (good), 2 (fair), 1 (weak)
 */
SEADROP_API int rssi_to_signal_bars(int rssi_dbm);

/**
 * @brief Map RSSI to trust zone using given thresholds
 */
SEADROP_API TrustZone rssi_to_zone(int rssi_dbm,
                                   const ZoneThresholds &thresholds);

} // namespace seadrop

#endif // SEADROP_DISTANCE_H

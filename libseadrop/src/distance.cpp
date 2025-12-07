/**
 * @file distance.cpp
 * @brief Distance-based trust zone monitoring implementation
 */

#include "seadrop/distance.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>


namespace seadrop {

// ============================================================================
// Trust Zone Names
// ============================================================================

const char *trust_zone_name(TrustZone zone) {
  switch (zone) {
  case TrustZone::Intimate:
    return "Intimate";
  case TrustZone::Close:
    return "Close";
  case TrustZone::Nearby:
    return "Nearby";
  case TrustZone::Far:
    return "Far";
  case TrustZone::Unknown:
    return "Unknown";
  default:
    return "Invalid";
  }
}

// ============================================================================
// DistanceInfo
// ============================================================================

std::chrono::milliseconds DistanceInfo::age() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - last_update);
}

// ============================================================================
// ZoneThresholds
// ============================================================================

void ZoneThresholds::reset_defaults() {
  intimate_max = 3.0f;
  close_max = 10.0f;
  nearby_max = 30.0f;

  calculate_rssi_from_distance();
}

void ZoneThresholds::calculate_rssi_from_distance() {
  // Use the inverse of rssi_to_distance formula
  // RSSI = TxPower - 10 * n * log10(distance)
  constexpr int TX_POWER = -59; // Measured RSSI at 1 meter
  constexpr float PATH_LOSS = 2.0f;

  intimate_rssi =
      static_cast<int>(TX_POWER - 10.0f * PATH_LOSS * std::log10(intimate_max));
  close_rssi =
      static_cast<int>(TX_POWER - 10.0f * PATH_LOSS * std::log10(close_max));
  nearby_rssi =
      static_cast<int>(TX_POWER - 10.0f * PATH_LOSS * std::log10(nearby_max));
}

// ============================================================================
// Utility Functions
// ============================================================================

float rssi_to_distance(int rssi_dbm, int tx_power, float path_loss_exp) {
  // Log-distance path loss model:
  // distance = 10 ^ ((TxPower - RSSI) / (10 * n))

  if (rssi_dbm >= tx_power) {
    // Signal stronger than reference, very close
    return 0.1f;
  }

  float exponent =
      (static_cast<float>(tx_power) - static_cast<float>(rssi_dbm)) /
      (10.0f * path_loss_exp);

  return std::pow(10.0f, exponent);
}

int distance_to_rssi(float distance_m, int tx_power, float path_loss_exp) {
  // Inverse of rssi_to_distance
  if (distance_m < 0.1f) {
    distance_m = 0.1f;
  }

  return static_cast<int>(tx_power -
                          10.0f * path_loss_exp * std::log10(distance_m));
}

int rssi_to_signal_bars(int rssi_dbm) {
  // Map RSSI to 1-4 signal bars
  if (rssi_dbm >= -55)
    return 4; // Excellent
  if (rssi_dbm >= -70)
    return 3; // Good
  if (rssi_dbm >= -85)
    return 2; // Fair
  return 1;   // Weak
}

TrustZone rssi_to_zone(int rssi_dbm, const ZoneThresholds &thresholds) {
  if (rssi_dbm >= thresholds.intimate_rssi) {
    return TrustZone::Intimate;
  } else if (rssi_dbm >= thresholds.close_rssi) {
    return TrustZone::Close;
  } else if (rssi_dbm >= thresholds.nearby_rssi) {
    return TrustZone::Nearby;
  } else {
    return TrustZone::Far;
  }
}

// ============================================================================
// DistanceMonitor Implementation
// ============================================================================

class DistanceMonitor::Impl {
public:
  // RSSI history for smoothing
  struct DeviceData {
    std::deque<RssiReading> readings;
    DistanceInfo current_info;
    TrustZone last_zone = TrustZone::Unknown;
    std::chrono::steady_clock::time_point last_zone_change;
  };

  std::unordered_map<std::string, DeviceData> devices; // Keyed by device ID hex

  ZoneThresholds thresholds;
  int smoothing_window = 5;
  std::chrono::milliseconds zone_hysteresis{2000};

  std::atomic<bool> running{false};
  mutable std::mutex mutex;

  // Callbacks
  ZoneChangedCallback zone_changed_cb;
  DistanceUpdatedCallback distance_updated_cb;
  SecurityAlertCallback security_alert_cb;

  std::string device_id_to_key(const DeviceId &id) const {
    // Simple hex conversion for map key
    std::string key;
    key.reserve(id.data.size() * 2);
    for (uint8_t byte : id.data) {
      char buf[3];
      snprintf(buf, sizeof(buf), "%02x", byte);
      key += buf;
    }
    return key;
  }

  int calculate_smoothed_rssi(const std::deque<RssiReading> &readings) {
    if (readings.empty())
      return -100;

    // Simple moving average
    int sum = 0;
    for (const auto &r : readings) {
      sum += r.rssi_dbm;
    }
    return sum / static_cast<int>(readings.size());
  }

  void process_reading(const std::string &key, DeviceData &data,
                       const DeviceId &device_id, const RssiReading &reading) {
    // Add to history
    data.readings.push_back(reading);
    while (data.readings.size() > static_cast<size_t>(smoothing_window)) {
      data.readings.pop_front();
    }

    // Calculate smoothed values
    int smoothed = calculate_smoothed_rssi(data.readings);
    float distance = rssi_to_distance(smoothed);
    TrustZone zone = rssi_to_zone(smoothed, thresholds);

    // Update distance info
    data.current_info.rssi_dbm = reading.rssi_dbm;
    data.current_info.rssi_smoothed = smoothed;
    data.current_info.distance_meters = distance;
    data.current_info.zone = zone;
    data.current_info.signal_bars = rssi_to_signal_bars(smoothed);
    data.current_info.last_update = reading.timestamp;

    // Calculate confidence based on reading stability
    if (data.readings.size() >= static_cast<size_t>(smoothing_window)) {
      int min_rssi = data.readings.front().rssi_dbm;
      int max_rssi = min_rssi;
      for (const auto &r : data.readings) {
        min_rssi = std::min(min_rssi, r.rssi_dbm);
        max_rssi = std::max(max_rssi, r.rssi_dbm);
      }
      int variance = max_rssi - min_rssi;
      // Lower variance = higher confidence
      data.current_info.confidence = std::max(0.0f, 1.0f - (variance / 30.0f));
      data.current_info.is_stable = variance < 10;
    }

    // Call distance update callback
    if (distance_updated_cb) {
      distance_updated_cb(device_id, data.current_info);
    }

    // Check for zone change
    auto now = std::chrono::steady_clock::now();
    if (zone != data.last_zone) {
      // Apply hysteresis
      if (now - data.last_zone_change >= zone_hysteresis) {
        TrustZone previous = data.last_zone;
        data.last_zone = zone;
        data.last_zone_change = now;

        // Create zone change event
        ZoneChangeEvent event;
        event.device_id = device_id;
        event.previous_zone = previous;
        event.current_zone = zone;
        event.distance_info = data.current_info;
        event.is_moving_closer =
            static_cast<int>(zone) < static_cast<int>(previous);
        event.timestamp = now;

        // Security alert if moving to Far zone unexpectedly
        event.requires_security_alert =
            (previous != TrustZone::Unknown && zone == TrustZone::Far &&
             previous != TrustZone::Far);

        if (zone_changed_cb) {
          zone_changed_cb(event);
        }

        if (event.requires_security_alert && security_alert_cb) {
          security_alert_cb(device_id, "Device moved to far zone unexpectedly. "
                                       "Verify before accepting transfers.");
        }
      }
    }
  }
};

DistanceMonitor::DistanceMonitor() : impl_(std::make_unique<Impl>()) {
  impl_->thresholds.reset_defaults();
}

DistanceMonitor::~DistanceMonitor() { stop(); }

DistanceMonitor::DistanceMonitor(DistanceMonitor &&) noexcept = default;
DistanceMonitor &
DistanceMonitor::operator=(DistanceMonitor &&) noexcept = default;

Result<void> DistanceMonitor::start() {
  impl_->running.store(true);
  return Result<void>::ok();
}

void DistanceMonitor::stop() { impl_->running.store(false); }

bool DistanceMonitor::is_running() const { return impl_->running.load(); }

Result<void>
DistanceMonitor::set_zone_thresholds(const ZoneThresholds &thresholds) {
  if (!thresholds.is_valid()) {
    return Error(ErrorCode::InvalidArgument, "Invalid zone thresholds");
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->thresholds = thresholds;
  return Result<void>::ok();
}

ZoneThresholds DistanceMonitor::get_zone_thresholds() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->thresholds;
}

void DistanceMonitor::set_smoothing_window(int samples) {
  impl_->smoothing_window = std::max(1, std::min(20, samples));
}

void DistanceMonitor::set_zone_change_hysteresis(
    std::chrono::milliseconds duration) {
  impl_->zone_hysteresis = duration;
}

Result<DistanceInfo>
DistanceMonitor::get_distance(const DeviceId &device_id) const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->device_id_to_key(device_id);
  auto it = impl_->devices.find(key);
  if (it == impl_->devices.end()) {
    return Error(ErrorCode::PeerNotFound, "Device not found");
  }

  return it->second.current_info;
}

TrustZone DistanceMonitor::get_zone(const DeviceId &device_id) const {
  auto result = get_distance(device_id);
  return result.is_ok() ? result.value().zone : TrustZone::Unknown;
}

bool DistanceMonitor::is_within_zone(const DeviceId &device_id,
                                     TrustZone zone) const {
  TrustZone current = get_zone(device_id);
  if (current == TrustZone::Unknown)
    return false;
  return static_cast<int>(current) <= static_cast<int>(zone);
}

void DistanceMonitor::feed_rssi(const DeviceId &device_id,
                                const RssiReading &reading) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->device_id_to_key(device_id);
  auto &data = impl_->devices[key];

  impl_->process_reading(key, data, device_id, reading);
}

void DistanceMonitor::remove_device(const DeviceId &device_id) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->devices.erase(impl_->device_id_to_key(device_id));
}

void DistanceMonitor::on_zone_changed(ZoneChangedCallback callback) {
  impl_->zone_changed_cb = std::move(callback);
}

void DistanceMonitor::on_distance_updated(DistanceUpdatedCallback callback) {
  impl_->distance_updated_cb = std::move(callback);
}

void DistanceMonitor::on_security_alert(SecurityAlertCallback callback) {
  impl_->security_alert_cb = std::move(callback);
}

} // namespace seadrop

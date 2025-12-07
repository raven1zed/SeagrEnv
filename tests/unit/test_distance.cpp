/**
 * @file test_distance.cpp
 * @brief Unit tests for distance-based trust zones
 */

#include <gtest/gtest.h>
#include <seadrop/seadrop.h>

using namespace seadrop;

class DistanceTest : public ::testing::Test {
protected:
  DistanceMonitor monitor;
  DeviceId test_device_id;

  void SetUp() override {
    // Create a test device ID
    for (size_t i = 0; i < DeviceId::SIZE; ++i) {
      test_device_id.data[i] = static_cast<Byte>(i);
    }

    monitor.start();
  }

  void TearDown() override { monitor.stop(); }

  void feed_rssi(int rssi) {
    RssiReading reading;
    reading.rssi_dbm = rssi;
    reading.timestamp = std::chrono::steady_clock::now();
    reading.is_bluetooth = true;

    monitor.feed_rssi(test_device_id, reading);
  }
};

// ============================================================================
// RSSI to Distance Conversion
// ============================================================================

TEST_F(DistanceTest, RssiToDistanceBasic) {
  // At 1 meter, RSSI should be around tx_power (-59 dBm by default)
  float d1 = rssi_to_distance(-59); // 1 meter
  EXPECT_NEAR(d1, 1.0f, 0.1f);

  // Weaker signal = farther
  float d2 = rssi_to_distance(-69); // About 3m
  EXPECT_GT(d2, d1);

  float d3 = rssi_to_distance(-79); // About 10m
  EXPECT_GT(d3, d2);
}

TEST_F(DistanceTest, DistanceToRssi) {
  // Inverse of rssi_to_distance
  int rssi1 = distance_to_rssi(1.0f); // 1 meter
  EXPECT_NEAR(rssi1, -59, 2);

  int rssi2 = distance_to_rssi(10.0f); // 10 meters
  EXPECT_LT(rssi2, rssi1);             // Weaker signal
}

// ============================================================================
// Signal Bars
// ============================================================================

TEST_F(DistanceTest, RssiToSignalBars) {
  EXPECT_EQ(rssi_to_signal_bars(-40), 4); // Excellent
  EXPECT_EQ(rssi_to_signal_bars(-60), 3); // Good
  EXPECT_EQ(rssi_to_signal_bars(-75), 2); // Fair
  EXPECT_EQ(rssi_to_signal_bars(-90), 1); // Weak
}

// ============================================================================
// Zone Detection
// ============================================================================

TEST_F(DistanceTest, RssiToZone) {
  ZoneThresholds thresholds;
  thresholds.reset_defaults();

  // Zone 1: Intimate (-30 to -55 dBm)
  EXPECT_EQ(rssi_to_zone(-40, thresholds), TrustZone::Intimate);
  EXPECT_EQ(rssi_to_zone(-55, thresholds), TrustZone::Intimate);

  // Zone 2: Close (-55 to -70 dBm)
  EXPECT_EQ(rssi_to_zone(-60, thresholds), TrustZone::Close);
  EXPECT_EQ(rssi_to_zone(-70, thresholds), TrustZone::Close);

  // Zone 3: Nearby (-70 to -85 dBm)
  EXPECT_EQ(rssi_to_zone(-75, thresholds), TrustZone::Nearby);
  EXPECT_EQ(rssi_to_zone(-85, thresholds), TrustZone::Nearby);

  // Zone 4: Far (< -85 dBm)
  EXPECT_EQ(rssi_to_zone(-90, thresholds), TrustZone::Far);
  EXPECT_EQ(rssi_to_zone(-100, thresholds), TrustZone::Far);
}

// ============================================================================
// Zone Thresholds
// ============================================================================

TEST_F(DistanceTest, ZoneThresholdsDefault) {
  ZoneThresholds thresholds;
  thresholds.reset_defaults();

  EXPECT_FLOAT_EQ(thresholds.intimate_max, 3.0f);
  EXPECT_FLOAT_EQ(thresholds.close_max, 10.0f);
  EXPECT_FLOAT_EQ(thresholds.nearby_max, 30.0f);

  EXPECT_TRUE(thresholds.is_valid());
}

TEST_F(DistanceTest, ZoneThresholdsValidation) {
  ZoneThresholds invalid;
  invalid.intimate_max = 0.0f; // Invalid: must be > 0
  EXPECT_FALSE(invalid.is_valid());

  ZoneThresholds invalid2;
  invalid2.intimate_max = 10.0f;
  invalid2.close_max = 5.0f; // Invalid: must be > intimate_max
  EXPECT_FALSE(invalid2.is_valid());
}

TEST_F(DistanceTest, CustomZoneThresholds) {
  ZoneThresholds custom;
  custom.intimate_max = 2.0f;
  custom.close_max = 5.0f;
  custom.nearby_max = 15.0f;
  custom.calculate_rssi_from_distance();

  EXPECT_TRUE(custom.is_valid());

  auto result = monitor.set_zone_thresholds(custom);
  EXPECT_TRUE(result.is_ok());

  auto retrieved = monitor.get_zone_thresholds();
  EXPECT_FLOAT_EQ(retrieved.intimate_max, 2.0f);
}

// ============================================================================
// Distance Monitor
// ============================================================================

TEST_F(DistanceTest, MonitorStartStop) {
  EXPECT_TRUE(monitor.is_running());

  monitor.stop();
  EXPECT_FALSE(monitor.is_running());

  auto result = monitor.start();
  EXPECT_TRUE(result.is_ok());
  EXPECT_TRUE(monitor.is_running());
}

TEST_F(DistanceTest, MonitorFeedRssi) {
  // Unknown device initially
  auto zone = monitor.get_zone(test_device_id);
  EXPECT_EQ(zone, TrustZone::Unknown);

  // Feed a reading
  feed_rssi(-50); // Strong signal, Zone 1

  // Now should be known
  zone = monitor.get_zone(test_device_id);
  EXPECT_EQ(zone, TrustZone::Intimate);

  auto result = monitor.get_distance(test_device_id);
  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(result.value().zone, TrustZone::Intimate);
}

TEST_F(DistanceTest, MonitorSmoothing) {
  // Feed several readings
  feed_rssi(-60);
  feed_rssi(-62);
  feed_rssi(-58);
  feed_rssi(-61);
  feed_rssi(-59);

  auto result = monitor.get_distance(test_device_id);
  EXPECT_TRUE(result.is_ok());

  // Smoothed RSSI should be around -60
  EXPECT_NEAR(result.value().rssi_smoothed, -60, 3);
}

TEST_F(DistanceTest, IsWithinZone) {
  feed_rssi(-50); // Zone 1 (Intimate)

  EXPECT_TRUE(monitor.is_within_zone(test_device_id, TrustZone::Intimate));
  EXPECT_TRUE(monitor.is_within_zone(test_device_id, TrustZone::Close));
  EXPECT_TRUE(monitor.is_within_zone(test_device_id, TrustZone::Nearby));
  EXPECT_TRUE(monitor.is_within_zone(test_device_id, TrustZone::Far));
}

TEST_F(DistanceTest, RemoveDevice) {
  feed_rssi(-60);

  EXPECT_NE(monitor.get_zone(test_device_id), TrustZone::Unknown);

  monitor.remove_device(test_device_id);

  EXPECT_EQ(monitor.get_zone(test_device_id), TrustZone::Unknown);
}

// ============================================================================
// Zone Change Callback
// ============================================================================

TEST_F(DistanceTest, ZoneChangeCallback) {
  bool callback_called = false;
  ZoneChangeEvent received_event;

  monitor.on_zone_changed([&](const ZoneChangeEvent &event) {
    callback_called = true;
    received_event = event;
  });

  // Set hysteresis to 0 for immediate callback
  monitor.set_zone_change_hysteresis(std::chrono::milliseconds(0));

  // Initial reading
  feed_rssi(-50); // Zone 1

  // Move to Zone 4 (big change)
  feed_rssi(-90); // Zone 4

  // Callback should have been triggered
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(received_event.current_zone, TrustZone::Far);
}

// ============================================================================
// Trust Zone Names
// ============================================================================

TEST_F(DistanceTest, TrustZoneNames) {
  EXPECT_STREQ(trust_zone_name(TrustZone::Intimate), "Intimate");
  EXPECT_STREQ(trust_zone_name(TrustZone::Close), "Close");
  EXPECT_STREQ(trust_zone_name(TrustZone::Nearby), "Nearby");
  EXPECT_STREQ(trust_zone_name(TrustZone::Far), "Far");
  EXPECT_STREQ(trust_zone_name(TrustZone::Unknown), "Unknown");
}

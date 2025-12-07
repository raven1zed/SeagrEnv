/**
 * @file test_full_transfer.cpp
 * @brief Integration test for complete file transfer flow
 */

#include <chrono>
#include <gtest/gtest.h>
#include <seadrop/seadrop.h>
#include <thread>


using namespace seadrop;

class IntegrationTest : public ::testing::Test {
protected:
  void SetUp() override { security_init(); }
};

// ============================================================================
// SeaDrop Initialization
// ============================================================================

TEST_F(IntegrationTest, InitializeSeaDrop) {
  SeaDrop app;

  SeaDropConfig config;
  config.device_name = "Test Device";
  config.download_path =
      std::filesystem::temp_directory_path() / "seadrop_test";

  auto result = app.init(config);
  EXPECT_TRUE(result.is_ok());
  EXPECT_TRUE(app.is_initialized());
  EXPECT_EQ(app.get_state(), SeaDropState::Idle);

  app.shutdown();
  EXPECT_FALSE(app.is_initialized());
}

TEST_F(IntegrationTest, DoubleInitialize) {
  SeaDrop app;

  SeaDropConfig config;
  config.device_name = "Test";

  auto result1 = app.init(config);
  EXPECT_TRUE(result1.is_ok());

  auto result2 = app.init(config);
  EXPECT_TRUE(result2.is_error());
  EXPECT_EQ(result2.error().code, ErrorCode::AlreadyInitialized);

  app.shutdown();
}

TEST_F(IntegrationTest, LocalDeviceInfo) {
  SeaDrop app;

  SeaDropConfig config;
  config.device_name = "My Laptop";

  app.init(config);

  auto device = app.get_local_device();
  EXPECT_EQ(device.name, "My Laptop");
  EXPECT_FALSE(device.id.is_zero());
  EXPECT_EQ(device.seadrop_version, VERSION_STRING);

  app.shutdown();
}

TEST_F(IntegrationTest, ChangeDeviceName) {
  SeaDrop app;

  SeaDropConfig config;
  config.device_name = "Original";

  app.init(config);

  auto result = app.set_device_name("Updated");
  EXPECT_TRUE(result.is_ok());

  EXPECT_EQ(app.get_local_device().name, "Updated");

  app.shutdown();
}

// ============================================================================
// Discovery
// ============================================================================

TEST_F(IntegrationTest, StartStopDiscovery) {
  SeaDrop app;

  SeaDropConfig config;
  config.device_name = "Test";

  app.init(config);

  auto result = app.start_discovery();
  EXPECT_TRUE(result.is_ok());
  EXPECT_TRUE(app.is_discovering());
  EXPECT_EQ(app.get_state(), SeaDropState::Discovering);

  app.stop_discovery();
  EXPECT_FALSE(app.is_discovering());
  EXPECT_EQ(app.get_state(), SeaDropState::Idle);

  app.shutdown();
}

// ============================================================================
// Configuration
// ============================================================================

TEST_F(IntegrationTest, ConfigValidation) {
  SeaDropConfig config;
  config.load_defaults();

  auto result = config.validate();
  EXPECT_TRUE(result.is_ok());

  // Invalid device name
  config.device_name = std::string(100, 'a'); // Too long
  result = config.validate();
  EXPECT_TRUE(result.is_error());
}

TEST_F(IntegrationTest, ZoneThresholdsConfig) {
  SeaDrop app;

  SeaDropConfig config;
  config.device_name = "Test";
  config.zone_thresholds.intimate_max = 2.0f;
  config.zone_thresholds.close_max = 8.0f;
  config.zone_thresholds.nearby_max = 20.0f;

  app.init(config);

  ZoneThresholds new_thresholds;
  new_thresholds.intimate_max = 5.0f;
  new_thresholds.close_max = 15.0f;
  new_thresholds.nearby_max = 40.0f;

  auto result = app.set_zone_thresholds(new_thresholds);
  EXPECT_TRUE(result.is_ok());

  app.shutdown();
}

// ============================================================================
// Callbacks
// ============================================================================

TEST_F(IntegrationTest, StateChangeCallback) {
  SeaDrop app;

  std::vector<SeaDropState> states;

  SeaDropConfig config;
  config.device_name = "Test";

  app.init(config);

  app.on_state_changed(
      [&states](SeaDropState state) { states.push_back(state); });

  app.start_discovery();
  app.stop_discovery();

  // Should have recorded state changes
  EXPECT_FALSE(states.empty());

  app.shutdown();
}

// ============================================================================
// Platform Info
// ============================================================================

TEST_F(IntegrationTest, PlatformInfo) {
  auto platform = get_platform_name();
  EXPECT_NE(platform, "Unknown");

  auto version = get_version();
  EXPECT_EQ(version.major, VERSION_MAJOR);
  EXPECT_EQ(version.minor, VERSION_MINOR);
  EXPECT_EQ(version.protocol_version, PROTOCOL_VERSION);
}

TEST_F(IntegrationTest, DefaultDeviceName) {
  auto name = get_default_device_name();
  EXPECT_FALSE(name.empty());
}

// ============================================================================
// Trust System
// ============================================================================

TEST_F(IntegrationTest, TrustOperations) {
  SeaDrop app;

  SeaDropConfig config;
  config.device_name = "Test";

  app.init(config);

  // Initially no trusted devices
  auto trusted = app.get_trusted_devices();
  EXPECT_TRUE(trusted.empty());

  app.shutdown();
}

// ============================================================================
// Auto-Clipboard Setting
// ============================================================================

TEST_F(IntegrationTest, AutoClipboard) {
  SeaDrop app;

  SeaDropConfig config;
  config.device_name = "Test";
  config.clipboard.auto_share_enabled = false;

  app.init(config);

  EXPECT_FALSE(app.is_auto_clipboard_enabled());

  app.set_auto_clipboard(true);
  EXPECT_TRUE(app.is_auto_clipboard_enabled());

  app.set_auto_clipboard(false);
  EXPECT_FALSE(app.is_auto_clipboard_enabled());

  app.shutdown();
}

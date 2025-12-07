/**
 * @file test_device.cpp
 * @brief Unit tests for device management and trust store
 */

#include <gtest/gtest.h>
#include <seadrop/seadrop.h>

using namespace seadrop;

class DeviceTest : public ::testing::Test {
protected:
  DeviceStore store;
  DeviceId test_id;

  void SetUp() override {
    // Create test device ID
    for (size_t i = 0; i < DeviceId::SIZE; ++i) {
      test_id.data[i] = static_cast<Byte>(i);
    }

    store.init(":memory:"); // In-memory for tests
  }

  void TearDown() override { store.close(); }

  Device create_test_device(const std::string &name) {
    Device device;
    device.id = test_id;
    device.name = name;
    device.platform = DevicePlatform::Linux;
    device.type = DeviceType::Desktop;
    device.trust_level = TrustLevel::Discovered;
    return device;
  }
};

// ============================================================================
// DeviceId
// ============================================================================

TEST_F(DeviceTest, DeviceIdToHex) {
  DeviceId id;
  for (size_t i = 0; i < DeviceId::SIZE; ++i) {
    id.data[i] = static_cast<Byte>(i);
  }

  auto hex = id.to_hex();
  EXPECT_EQ(hex.length(), DeviceId::SIZE * 2);
  EXPECT_EQ(hex.substr(0, 6), "000102");
}

TEST_F(DeviceTest, DeviceIdFromHex) {
  std::string hex = "000102030405060708090a0b0c0d0e0f"
                    "101112131415161718191a1b1c1d1e1f";

  auto result = DeviceId::from_hex(hex);
  ASSERT_TRUE(result.has_value());

  for (size_t i = 0; i < DeviceId::SIZE; ++i) {
    EXPECT_EQ(result->data[i], static_cast<Byte>(i));
  }
}

TEST_F(DeviceTest, DeviceIdFromHexInvalid) {
  // Too short
  auto result1 = DeviceId::from_hex("0001020304");
  EXPECT_FALSE(result1.has_value());

  // Not hex
  auto result2 = DeviceId::from_hex(std::string(64, 'g'));
  EXPECT_FALSE(result2.has_value());
}

TEST_F(DeviceTest, DeviceIdIsZero) {
  DeviceId zero{};
  EXPECT_TRUE(zero.is_zero());

  DeviceId nonzero{};
  nonzero.data[0] = 1;
  EXPECT_FALSE(nonzero.is_zero());
}

// ============================================================================
// Trust Levels
// ============================================================================

TEST_F(DeviceTest, TrustLevelNames) {
  EXPECT_STREQ(trust_level_name(TrustLevel::Unknown), "Unknown");
  EXPECT_STREQ(trust_level_name(TrustLevel::Discovered), "Discovered");
  EXPECT_STREQ(trust_level_name(TrustLevel::Trusted), "Trusted");
  EXPECT_STREQ(trust_level_name(TrustLevel::Blocked), "Blocked");
}

// ============================================================================
// Device Helper Methods
// ============================================================================

TEST_F(DeviceTest, DeviceIsTrusted) {
  Device device;
  device.trust_level = TrustLevel::Discovered;
  EXPECT_FALSE(device.is_trusted());

  device.trust_level = TrustLevel::Trusted;
  EXPECT_TRUE(device.is_trusted());
}

TEST_F(DeviceTest, DeviceIsBlocked) {
  Device device;
  device.trust_level = TrustLevel::Discovered;
  EXPECT_FALSE(device.is_blocked());

  device.trust_level = TrustLevel::Blocked;
  EXPECT_TRUE(device.is_blocked());
}

TEST_F(DeviceTest, DeviceDisplayName) {
  Device device;
  device.name = "My Phone";
  device.user_alias = "";

  EXPECT_EQ(device.display_name(), "My Phone");

  device.user_alias = "Work Device";
  EXPECT_EQ(device.display_name(), "Work Device");
}

TEST_F(DeviceTest, CanAutoAcceptFiles) {
  Device device;
  device.trust_level = TrustLevel::Trusted;

  EXPECT_TRUE(device.can_auto_accept_files(TrustZone::Intimate));
  EXPECT_TRUE(device.can_auto_accept_files(TrustZone::Close));
  EXPECT_FALSE(device.can_auto_accept_files(TrustZone::Nearby));
  EXPECT_FALSE(device.can_auto_accept_files(TrustZone::Far));

  // Untrusted device should never auto-accept
  device.trust_level = TrustLevel::Discovered;
  EXPECT_FALSE(device.can_auto_accept_files(TrustZone::Intimate));
}

TEST_F(DeviceTest, CanAutoClipboard) {
  Device device;
  device.trust_level = TrustLevel::Trusted;

  // Only Zone 1 should allow auto-clipboard
  EXPECT_TRUE(device.can_auto_clipboard(TrustZone::Intimate));
  EXPECT_FALSE(device.can_auto_clipboard(TrustZone::Close));
  EXPECT_FALSE(device.can_auto_clipboard(TrustZone::Nearby));
  EXPECT_FALSE(device.can_auto_clipboard(TrustZone::Far));
}

// ============================================================================
// DeviceStore
// ============================================================================

TEST_F(DeviceTest, SaveAndGetDevice) {
  auto device = create_test_device("Test Device");

  auto save_result = store.save_device(device);
  EXPECT_TRUE(save_result.is_ok());

  auto get_result = store.get_device(test_id);
  ASSERT_TRUE(get_result.is_ok());
  EXPECT_EQ(get_result.value().name, "Test Device");
}

TEST_F(DeviceTest, GetNonexistentDevice) {
  DeviceId unknown_id;
  unknown_id.data[0] = 0xFF;

  auto result = store.get_device(unknown_id);
  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(result.error().code, ErrorCode::RecordNotFound);
}

TEST_F(DeviceTest, TrustDevice) {
  auto device = create_test_device("Phone");
  store.save_device(device);

  Bytes shared_key = {1, 2, 3, 4, 5};
  auto result = store.trust_device(test_id, shared_key);
  EXPECT_TRUE(result.is_ok());

  EXPECT_TRUE(store.is_trusted(test_id));

  auto key_result = store.get_shared_key(test_id);
  EXPECT_TRUE(key_result.is_ok());
  EXPECT_EQ(key_result.value(), shared_key);
}

TEST_F(DeviceTest, BlockDevice) {
  auto device = create_test_device("Spammer");
  store.save_device(device);

  auto result = store.block_device(test_id);
  EXPECT_TRUE(result.is_ok());

  EXPECT_TRUE(store.is_blocked(test_id));

  // Blocked list should contain the device
  auto blocked = store.get_blocked_devices();
  EXPECT_EQ(blocked.size(), 1u);
}

TEST_F(DeviceTest, UntrustDevice) {
  auto device = create_test_device("Phone");
  store.save_device(device);
  store.trust_device(test_id, {1, 2, 3});

  EXPECT_TRUE(store.is_trusted(test_id));

  auto result = store.untrust_device(test_id);
  EXPECT_TRUE(result.is_ok());

  EXPECT_FALSE(store.is_trusted(test_id));

  // Shared key should be removed
  auto key_result = store.get_shared_key(test_id);
  EXPECT_TRUE(key_result.is_error());
}

TEST_F(DeviceTest, SetDeviceAlias) {
  auto device = create_test_device("Phone");
  store.save_device(device);

  auto result = store.set_device_alias(test_id, "Work Phone");
  EXPECT_TRUE(result.is_ok());

  auto get_result = store.get_device(test_id);
  EXPECT_EQ(get_result.value().user_alias, "Work Phone");
}

TEST_F(DeviceTest, DeleteDevice) {
  auto device = create_test_device("Temporary");
  store.save_device(device);

  auto result = store.delete_device(test_id);
  EXPECT_TRUE(result.is_ok());

  auto get_result = store.get_device(test_id);
  EXPECT_TRUE(get_result.is_error());
}

TEST_F(DeviceTest, GetTrustedDevices) {
  // Save multiple devices
  auto device1 = create_test_device("Phone 1");
  device1.id.data[0] = 1;
  store.save_device(device1);
  store.trust_device(device1.id, {1});

  auto device2 = create_test_device("Phone 2");
  device2.id.data[0] = 2;
  store.save_device(device2);
  store.trust_device(device2.id, {2});

  auto device3 = create_test_device("Untrusted");
  device3.id.data[0] = 3;
  store.save_device(device3);

  auto trusted = store.get_trusted_devices();
  EXPECT_EQ(trusted.size(), 2u);
}

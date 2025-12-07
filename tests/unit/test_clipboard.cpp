/**
 * @file test_clipboard.cpp
 * @brief Unit tests for clipboard sharing
 */

#include <gtest/gtest.h>
#include <seadrop/seadrop.h>

using namespace seadrop;

class ClipboardTest : public ::testing::Test {
protected:
  ClipboardManager manager;

  void SetUp() override {
    ClipboardConfig config;
    manager.init(config);
  }

  void TearDown() override { manager.shutdown(); }
};

// ============================================================================
// Clipboard Type Names
// ============================================================================

TEST_F(ClipboardTest, ClipboardTypeNames) {
  EXPECT_STREQ(clipboard_type_name(ClipboardType::Empty), "Empty");
  EXPECT_STREQ(clipboard_type_name(ClipboardType::Text), "Text");
  EXPECT_STREQ(clipboard_type_name(ClipboardType::Url), "URL");
  EXPECT_STREQ(clipboard_type_name(ClipboardType::Image), "Image");
}

// ============================================================================
// ClipboardData Creation
// ============================================================================

TEST_F(ClipboardTest, FromText) {
  auto data = ClipboardData::from_text("Hello, world!");

  EXPECT_EQ(data.type, ClipboardType::Text);
  EXPECT_EQ(data.size(), 13u);
  EXPECT_EQ(data.mime_type, "text/plain");
  EXPECT_EQ(data.get_text(), "Hello, world!");
}

TEST_F(ClipboardTest, FromUrl) {
  auto data = ClipboardData::from_url("https://example.com/page");

  EXPECT_EQ(data.type, ClipboardType::Url);
  EXPECT_EQ(data.mime_type, "text/uri-list");
  EXPECT_EQ(data.get_text(), "https://example.com/page");
}

TEST_F(ClipboardTest, FromImage) {
  Bytes png_data = {0x89, 0x50, 0x4E, 0x47}; // PNG magic bytes
  auto data = ClipboardData::from_image(png_data, 100, 200);

  EXPECT_EQ(data.type, ClipboardType::Image);
  EXPECT_EQ(data.mime_type, "image/png");
  EXPECT_EQ(data.image_info.width, 100);
  EXPECT_EQ(data.image_info.height, 200);
}

TEST_F(ClipboardTest, Preview) {
  std::string long_text(200, 'a');
  auto data = ClipboardData::from_text(long_text);

  // Preview should be truncated to 100 chars
  EXPECT_EQ(data.preview.length(), 100u);
}

TEST_F(ClipboardTest, GetTextFromNonText) {
  Bytes png_data = {0x89, 0x50};
  auto data = ClipboardData::from_image(png_data, 10, 10);

  // Should return empty string for non-text types
  EXPECT_EQ(data.get_text(), "");
}

TEST_F(ClipboardTest, IsEmpty) {
  ClipboardData empty;
  EXPECT_TRUE(empty.is_empty());

  auto text = ClipboardData::from_text("Hello");
  EXPECT_FALSE(text.is_empty());
}

// ============================================================================
// Clipboard Manager
// ============================================================================

TEST_F(ClipboardTest, AutoShareDefault) {
  // Default should be disabled
  EXPECT_FALSE(manager.is_auto_share_enabled());
}

TEST_F(ClipboardTest, SetAutoShare) {
  manager.set_auto_share(true);
  EXPECT_TRUE(manager.is_auto_share_enabled());

  manager.set_auto_share(false);
  EXPECT_FALSE(manager.is_auto_share_enabled());
}

TEST_F(ClipboardTest, Config) {
  ClipboardConfig config;
  config.share_text = false;
  config.max_image_size = 5 * 1024 * 1024;

  manager.set_config(config);

  auto retrieved = manager.get_config();
  EXPECT_FALSE(retrieved.share_text);
  EXPECT_EQ(retrieved.max_image_size, 5u * 1024 * 1024);
}

// ============================================================================
// History
// ============================================================================

TEST_F(ClipboardTest, HistoryEmpty) {
  auto history = manager.get_receive_history();
  EXPECT_TRUE(history.empty());
}

TEST_F(ClipboardTest, ClearHistory) {
  // Even if we somehow had entries, clear should work
  manager.clear_history();
  auto history = manager.get_receive_history();
  EXPECT_TRUE(history.empty());
}

TEST_F(ClipboardTest, ApplyInvalidIndex) {
  auto result = manager.apply_received(999);
  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
}

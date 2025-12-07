/**
 * @file test_transfer.cpp
 * @brief Unit tests for file transfer protocol
 */

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <seadrop/seadrop.h>


using namespace seadrop;
namespace fs = std::filesystem;

class TransferTest : public ::testing::Test {
protected:
  TransferManager manager;
  fs::path test_dir;

  void SetUp() override {
    // Create temp directory for tests
    test_dir = fs::temp_directory_path() / "seadrop_test";
    fs::create_directories(test_dir);

    TransferOptions opts;
    opts.save_directory = test_dir;
    manager.init(opts);
  }

  void TearDown() override {
    manager.shutdown();
    fs::remove_all(test_dir);
  }

  fs::path create_test_file(const std::string &name, size_t size) {
    fs::path path = test_dir / name;
    std::ofstream file(path, std::ios::binary);
    for (size_t i = 0; i < size; ++i) {
      file.put(static_cast<char>(i % 256));
    }
    return path;
  }
};

// ============================================================================
// Transfer ID
// ============================================================================

TEST_F(TransferTest, TransferIdGeneration) {
  auto id1 = TransferId::generate();
  auto id2 = TransferId::generate();

  EXPECT_NE(id1, id2);
}

TEST_F(TransferTest, TransferIdHex) {
  auto id = TransferId::generate();
  auto hex = id.to_hex();

  EXPECT_EQ(hex.length(), TransferId::SIZE * 2);
}

// ============================================================================
// Utility Functions
// ============================================================================

TEST_F(TransferTest, FormatBytes) {
  EXPECT_EQ(format_bytes(0), "0 B");
  EXPECT_EQ(format_bytes(512), "512 B");
  EXPECT_EQ(format_bytes(1024), "1.0 KB");
  EXPECT_EQ(format_bytes(1536), "1.5 KB");
  EXPECT_EQ(format_bytes(1024 * 1024), "1.0 MB");
  EXPECT_EQ(format_bytes(1024 * 1024 * 1024), "1.0 GB");
}

TEST_F(TransferTest, FormatSpeed) {
  EXPECT_EQ(format_speed(1024), "1.0 KB/s");
  EXPECT_EQ(format_speed(10 * 1024 * 1024), "10.0 MB/s");
}

TEST_F(TransferTest, FormatDuration) {
  EXPECT_EQ(format_duration(std::chrono::milliseconds(5000)), "5s");
  EXPECT_EQ(format_duration(std::chrono::milliseconds(65000)), "1m 5s");
  EXPECT_EQ(format_duration(std::chrono::milliseconds(3665000)), "1h 1m");
}

TEST_F(TransferTest, DetectMimeType) {
  EXPECT_EQ(detect_mime_type("photo.jpg"), "image/jpeg");
  EXPECT_EQ(detect_mime_type("photo.JPEG"), "image/jpeg");
  EXPECT_EQ(detect_mime_type("document.pdf"), "application/pdf");
  EXPECT_EQ(detect_mime_type("script.py"), "text/x-python");
  EXPECT_EQ(detect_mime_type("unknown.xyz"), "application/octet-stream");
}

TEST_F(TransferTest, GenerateUniqueFilename) {
  // Create a file
  auto path = create_test_file("test.txt", 100);

  std::vector<fs::path> existing = {path};

  auto unique = generate_unique_filename(path, existing);
  EXPECT_NE(unique, path);
  EXPECT_EQ(unique.filename().string(), "test (1).txt");

  // If that exists too
  existing.push_back(unique);
  auto unique2 = generate_unique_filename(path, existing);
  EXPECT_EQ(unique2.filename().string(), "test (2).txt");
}

// ============================================================================
// Transfer State
// ============================================================================

TEST_F(TransferTest, TransferStateNames) {
  EXPECT_STREQ(transfer_state_name(TransferState::Pending), "Pending");
  EXPECT_STREQ(transfer_state_name(TransferState::InProgress), "In Progress");
  EXPECT_STREQ(transfer_state_name(TransferState::Completed), "Completed");
  EXPECT_STREQ(transfer_state_name(TransferState::Failed), "Failed");
}

// ============================================================================
// Transfer Manager
// ============================================================================

TEST_F(TransferTest, SendFileNotFound) {
  auto result = manager.send_file(test_dir / "nonexistent.txt");
  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(result.error().code, ErrorCode::FileNotFound);
}

TEST_F(TransferTest, SendEmptyFileList) {
  std::vector<fs::path> empty;
  auto result = manager.send_files(empty);
  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
}

TEST_F(TransferTest, SendText) {
  auto result = manager.send_text("Hello, world!");
  EXPECT_TRUE(result.is_ok());

  auto progress = manager.get_progress(result.value());
  EXPECT_TRUE(progress.is_ok());
  EXPECT_EQ(progress.value().total_bytes, 13u); // "Hello, world!"
}

TEST_F(TransferTest, CancelTransfer) {
  auto result = manager.send_text("Test data");
  ASSERT_TRUE(result.is_ok());

  auto id = result.value();
  manager.cancel_transfer(id);

  auto completed = manager.get_result(id);
  EXPECT_TRUE(completed.is_ok());
  EXPECT_EQ(completed.value().state, TransferState::Cancelled);
}

TEST_F(TransferTest, GetActiveTransfers) {
  manager.send_text("Test 1");
  manager.send_text("Test 2");

  auto active = manager.get_active_transfers();
  EXPECT_EQ(active.size(), 2u);
}

// ============================================================================
// File Checksum
// ============================================================================

TEST_F(TransferTest, CalculateFileChecksum) {
  auto path = create_test_file("checksum_test.bin", 1024);

  auto result = calculate_file_checksum(path);
  ASSERT_TRUE(result.is_ok());
  EXPECT_EQ(result.value().size(), 32u);

  // Same file should give same checksum
  auto result2 = calculate_file_checksum(path);
  EXPECT_EQ(result.value(), result2.value());
}

TEST_F(TransferTest, ChecksumNonexistentFile) {
  auto result = calculate_file_checksum(test_dir / "nonexistent.bin");
  EXPECT_TRUE(result.is_error());
}

// ============================================================================
// Transfer Options
// ============================================================================

TEST_F(TransferTest, DefaultOptions) {
  auto opts = manager.get_default_options();
  EXPECT_EQ(opts.on_conflict, ConflictResolution::AutoRename);
  EXPECT_TRUE(opts.verify_checksum);
}

TEST_F(TransferTest, SetDefaultOptions) {
  TransferOptions opts;
  opts.on_conflict = ConflictResolution::Overwrite;
  opts.chunk_size = 32 * 1024;

  manager.set_default_options(opts);

  auto retrieved = manager.get_default_options();
  EXPECT_EQ(retrieved.on_conflict, ConflictResolution::Overwrite);
  EXPECT_EQ(retrieved.chunk_size, 32u * 1024);
}

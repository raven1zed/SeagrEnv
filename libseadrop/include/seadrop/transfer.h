/**
 * @file transfer.h
 * @brief File transfer protocol for SeaDrop
 *
 * SeaDrop uses a chunked transfer protocol with:
 * - End-to-end encryption (XChaCha20-Poly1305)
 * - Progress tracking with speed/ETA
 * - Auto-rename on filename conflicts
 * - Resume support for interrupted transfers
 * - Checksum verification (BLAKE2b)
 *
 * Protocol Overview:
 *   1. Sender sends TransferRequest (file list, sizes, etc.)
 *   2. Receiver accepts/rejects
 *   3. For each file:
 *      a. Send FileHeader (name, size, checksum)
 *      b. Send file data in 64KB chunks
 *      c. Receiver verifies checksum
 *   4. Transfer complete
 */

#ifndef SEADROP_TRANSFER_H
#define SEADROP_TRANSFER_H

#include "device.h"
#include "distance.h"
#include "error.h"
#include "platform.h"
#include "types.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace seadrop {

// ============================================================================
// Transfer Constants
// ============================================================================

/// Default chunk size for file transfers (64 KB)
constexpr size_t DEFAULT_CHUNK_SIZE = 64 * 1024;

/// Maximum filename length (UTF-8 bytes)
constexpr size_t MAX_FILENAME_LENGTH = 255;

/// Maximum path length (UTF-8 bytes)
constexpr size_t MAX_PATH_LENGTH = 4096;

/// Maximum file size (theoretically unlimited, but UI shows warning above this)
constexpr uint64_t WARN_FILE_SIZE = 10ULL * 1024 * 1024 * 1024; // 10 GB

// ============================================================================
// Conflict Resolution
// ============================================================================

/**
 * @brief How to handle filename conflicts
 */
enum class ConflictResolution : uint8_t {
  /// Auto-rename: photo.jpg → photo (1).jpg [DEFAULT]
  AutoRename = 0,

  /// Overwrite existing file
  Overwrite = 1,

  /// Skip the file
  Skip = 2,

  /// Ask user for each conflict
  Ask = 3
};

// ============================================================================
// Transfer Options
// ============================================================================

/**
 * @brief Configuration options for a transfer
 */
struct TransferOptions {
  /// How to handle filename conflicts
  ConflictResolution on_conflict = ConflictResolution::AutoRename;

  /// Directory to save received files
  std::filesystem::path save_directory;

  /// Create subdirectory per sender (e.g., "Downloads/Phone/")
  bool use_sender_subdir = true;

  /// Verify file checksums after transfer
  bool verify_checksum = true;

  /// Chunk size for transfer (adjust for network conditions)
  size_t chunk_size = DEFAULT_CHUNK_SIZE;

  /// Enable compression (zstd) - good for text, bad for already-compressed
  /// files
  bool compress = false;

  /// Preserve file timestamps
  bool preserve_timestamps = true;

  /// Maximum concurrent file transfers (within one session)
  int max_concurrent_files = 1;
};

// ============================================================================
// Transfer State
// ============================================================================

// TransferState is defined in types.h

/**
 * @brief Get human-readable name for transfer state
 */
SEADROP_API const char *transfer_state_name(TransferState state);

// ============================================================================
// File Information
// ============================================================================

/**
 * @brief Information about a single file in a transfer
 */
struct FileInfo {
  /// Relative path within transfer (e.g., "photos/vacation/beach.jpg")
  std::filesystem::path relative_path;

  /// Original filename
  std::string name;

  /// File size in bytes
  uint64_t size = 0;

  /// MIME type (e.g., "image/jpeg")
  std::string mime_type;

  /// BLAKE2b checksum (32 bytes)
  std::array<Byte, 32> checksum;

  /// File modification time
  std::chrono::system_clock::time_point modified_time;

  /// Is this a directory entry?
  bool is_directory = false;

  // Transfer progress (for UI)
  uint64_t bytes_transferred = 0;
  bool is_complete = false;
  bool has_error = false;
  std::string error_message;

  /// Get progress as percentage (0.0 - 1.0)
  double progress() const {
    return size > 0 ? static_cast<double>(bytes_transferred) / size : 0.0;
  }

  /// Get final saved path (after potential rename)
  std::filesystem::path saved_path;
};

// ============================================================================
// Transfer Request
// ============================================================================

/**
 * @brief Request to initiate a file transfer
 */
struct TransferRequest {
  /// Unique transfer ID
  TransferId id;

  /// Sender device info
  Device sender;

  /// List of files to transfer
  std::vector<FileInfo> files;

  /// Total size of all files
  uint64_t total_size = 0;

  /// Total number of files
  uint32_t file_count = 0;

  /// Optional message from sender
  std::string message;

  /// Transfer options (from sender, receiver may adjust)
  TransferOptions options;

  /// When request was created
  std::chrono::system_clock::time_point created_at;

  /// Request expiration time
  std::chrono::system_clock::time_point expires_at;

  /// Is this an auto-accepted transfer? (trusted device, appropriate zone)
  bool auto_accepted = false;

  /// Check if request has expired
  bool is_expired() const {
    return std::chrono::system_clock::now() > expires_at;
  }
};

// ============================================================================
// Transfer Progress
// ============================================================================

/**
 * @brief Real-time progress information for a transfer
 */
struct TransferProgress {
  /// Transfer ID
  TransferId id;

  /// Current state
  TransferState state = TransferState::Pending;

  /// Overall progress (0.0 - 1.0)
  double progress = 0.0;

  /// Bytes transferred so far
  uint64_t bytes_transferred = 0;

  /// Total bytes to transfer
  uint64_t total_bytes = 0;

  /// Current transfer speed (bytes per second)
  double speed_bps = 0.0;

  /// Average transfer speed
  double avg_speed_bps = 0.0;

  /// Estimated time remaining
  std::chrono::seconds eta;

  /// Time elapsed since start
  std::chrono::milliseconds elapsed;

  /// Currently transferring file index
  int current_file_index = 0;

  /// Current file progress
  std::optional<FileInfo> current_file;

  /// Number of completed files
  int completed_files = 0;

  /// Total number of files
  int total_files = 0;

  // ========================================================================
  // Helper Methods
  // ========================================================================

  /// Get human-readable speed (e.g., "2.5 MB/s")
  std::string speed_string() const;

  /// Get human-readable ETA (e.g., "2m 30s")
  std::string eta_string() const;

  /// Get human-readable progress (e.g., "45.2 MB / 100 MB")
  std::string progress_string() const;
};

// ============================================================================
// Transfer Result
// ============================================================================

/**
 * @brief Final result of a completed transfer
 */
struct TransferResult {
  /// Transfer ID
  TransferId id;

  /// Final state (Completed, Cancelled, Rejected, Failed)
  TransferState state;

  /// Total bytes transferred
  uint64_t bytes_transferred = 0;

  /// Total duration
  std::chrono::milliseconds duration;

  /// Average speed
  double avg_speed_bps = 0.0;

  /// Successfully transferred files
  std::vector<FileInfo> successful_files;

  /// Failed files (with error messages)
  std::vector<FileInfo> failed_files;

  /// Skipped files (conflicts with Skip resolution)
  std::vector<FileInfo> skipped_files;

  /// Error message (if state is Failed)
  std::string error_message;

  /// Check if transfer was fully successful
  bool is_success() const {
    return state == TransferState::Completed && failed_files.empty();
  }
};

// ============================================================================
// Transfer Manager
// ============================================================================

/**
 * @brief Manages file transfers for SeaDrop
 *
 * The TransferManager handles sending and receiving files over
 * an established WiFi Direct connection. It manages:
 * - Transfer protocol (chunked, encrypted)
 * - Progress tracking
 * - Conflict resolution
 * - Checksum verification
 * - Queue management for multiple transfers
 *
 * Example usage:
 * @code
 *   TransferManager transfer;
 *   transfer.init(&connection, &distance_monitor, config);
 *
 *   transfer.on_progress([](const TransferProgress& p) {
 *       update_ui(p.progress, p.speed_string());
 *   });
 *
 *   TransferId id = transfer.send_file("/path/to/file.jpg");
 * @endcode
 */
class SEADROP_API TransferManager {
public:
  TransferManager();
  ~TransferManager();

  // Non-copyable
  TransferManager(const TransferManager &) = delete;
  TransferManager &operator=(const TransferManager &) = delete;

  // ========================================================================
  // Initialization
  // ========================================================================

  /**
   * @brief Initialize the transfer manager
   * @param options Default transfer options
   * @return Success or error
   */
  Result<void> init(const TransferOptions &options = {});

  /**
   * @brief Shutdown and cancel all pending transfers
   */
  void shutdown();

  // ========================================================================
  // Sending Files
  // ========================================================================

  /**
   * @brief Send a single file
   * @param path Path to file
   * @param options Optional per-transfer options
   * @return Transfer ID or error
   */
  Result<TransferId> send_file(const std::filesystem::path &path,
                               const TransferOptions &options = {});

  /**
   * @brief Send multiple files
   * @param paths Paths to files
   * @param options Optional per-transfer options
   * @return Transfer ID or error
   */
  Result<TransferId> send_files(const std::vector<std::filesystem::path> &paths,
                                const TransferOptions &options = {});

  /**
   * @brief Send a directory (recursively)
   * @param path Path to directory
   * @param options Optional per-transfer options
   * @return Transfer ID or error
   */
  Result<TransferId> send_directory(const std::filesystem::path &path,
                                    const TransferOptions &options = {});

  /**
   * @brief Send text content (for text sharing)
   * @param text Text to send
   * @param filename Optional filename (default: "text.txt")
   * @return Transfer ID or error
   */
  Result<TransferId> send_text(const std::string &text,
                               const std::string &filename = "text.txt");

  /**
   * @brief Send raw data (for clipboard, etc.)
   * @param data Data to send
   * @param filename Filename for the data
   * @param mime_type MIME type
   * @return Transfer ID or error
   */
  Result<TransferId> send_data(const Bytes &data, const std::string &filename,
                               const std::string &mime_type);

  // ========================================================================
  // Receiving Files
  // ========================================================================

  /**
   * @brief Accept a transfer request
   * @param request_id Transfer ID to accept
   * @param options Optional options (e.g., save directory)
   * @return Success or error
   */
  Result<void> accept_transfer(const TransferId &request_id,
                               const TransferOptions &options = {});

  /**
   * @brief Reject a transfer request
   * @param request_id Transfer ID to reject
   * @param reason Optional rejection reason
   */
  void reject_transfer(const TransferId &request_id,
                       const std::string &reason = "");

  // ========================================================================
  // Transfer Control
  // ========================================================================

  /**
   * @brief Pause a transfer
   * @param transfer_id Transfer to pause
   * @return Success or error
   */
  Result<void> pause_transfer(const TransferId &transfer_id);

  /**
   * @brief Resume a paused transfer
   * @param transfer_id Transfer to resume
   * @return Success or error
   */
  Result<void> resume_transfer(const TransferId &transfer_id);

  /**
   * @brief Cancel a transfer
   * @param transfer_id Transfer to cancel
   */
  void cancel_transfer(const TransferId &transfer_id);

  // ========================================================================
  // Transfer Queries
  // ========================================================================

  /**
   * @brief Get progress for a transfer
   * @param transfer_id Transfer ID
   * @return Progress info or error if not found
   */
  Result<TransferProgress> get_progress(const TransferId &transfer_id) const;

  /**
   * @brief Get result for a completed transfer
   * @param transfer_id Transfer ID
   * @return Result or error if not found/not complete
   */
  Result<TransferResult> get_result(const TransferId &transfer_id) const;

  /**
   * @brief Get all active transfers
   */
  std::vector<TransferProgress> get_active_transfers() const;

  /**
   * @brief Get pending transfer requests (waiting for accept/reject)
   */
  std::vector<TransferRequest> get_pending_requests() const;

  // ========================================================================
  // Configuration
  // ========================================================================

  /**
   * @brief Set default transfer options
   */
  void set_default_options(const TransferOptions &options);

  /**
   * @brief Get default transfer options
   */
  TransferOptions get_default_options() const;

  // ========================================================================
  // Callbacks
  // ========================================================================

  /**
   * @brief Set callback for incoming transfer requests
   */
  void
  on_transfer_request(std::function<void(const TransferRequest &)> callback);

  /**
   * @brief Set callback for progress updates
   */
  void on_progress(std::function<void(const TransferProgress &)> callback);

  /**
   * @brief Set callback for transfer completion
   */
  void on_complete(std::function<void(const TransferResult &)> callback);

  /**
   * @brief Set callback for file received (called per file)
   */
  void on_file_received(std::function<void(const FileInfo &)> callback);

  /**
   * @brief Set callback for errors
   */
  void
  on_error(std::function<void(const TransferId &, const Error &)> callback);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Generate unique filename for conflict resolution
 * @param path Original path
 * @param existing_files Set of files that already exist
 * @return New path with number suffix (e.g., "file (1).txt")
 */
SEADROP_API std::filesystem::path generate_unique_filename(
    const std::filesystem::path &path,
    const std::vector<std::filesystem::path> &existing_files);

/**
 * @brief Calculate BLAKE2b checksum of a file
 * @param path Path to file
 * @return 32-byte checksum or error
 */
SEADROP_API Result<std::array<Byte, 32>>
calculate_file_checksum(const std::filesystem::path &path);

/**
 * @brief Detect MIME type from file extension and/or content
 */
SEADROP_API std::string detect_mime_type(const std::filesystem::path &path);

/**
 * @brief Format bytes as human-readable string (e.g., "2.5 MB")
 */
SEADROP_API std::string format_bytes(uint64_t bytes);

/**
 * @brief Format speed as human-readable string (e.g., "10.5 MB/s")
 */
SEADROP_API std::string format_speed(double bytes_per_second);

/**
 * @brief Format duration as human-readable string (e.g., "2m 30s")
 */
SEADROP_API std::string format_duration(std::chrono::milliseconds duration);

} // namespace seadrop

#endif // SEADROP_TRANSFER_H

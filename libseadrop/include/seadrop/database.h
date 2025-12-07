/**
 * @file database.h
 * @brief SQLite database wrapper for SeaDrop
 *
 * Handles persistent storage for:
 * - Trusted devices and their encryption keys
 * - Transfer history
 * - User settings (optional, can use separate config file)
 */

#ifndef SEADROP_DATABASE_H
#define SEADROP_DATABASE_H

#include "device.h"
#include "error.h"
#include "platform.h"
#include "transfer.h"
#include "types.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>


namespace seadrop {

// ============================================================================
// Transfer History Entry
// ============================================================================

/**
 * @brief A record in transfer history
 */
struct TransferHistoryEntry {
  /// Unique record ID
  int64_t id = 0;

  /// Transfer ID
  TransferId transfer_id;

  /// Peer device ID
  DeviceId peer_id;

  /// Peer name (at time of transfer)
  std::string peer_name;

  /// Direction (send or receive)
  TransferDirection direction;

  /// Final state
  TransferState state;

  /// Files in the transfer
  std::vector<std::string> filenames;

  /// Total bytes transferred
  uint64_t total_bytes = 0;

  /// Number of files
  int file_count = 0;

  /// Duration (milliseconds)
  int64_t duration_ms = 0;

  /// Timestamp
  std::chrono::system_clock::time_point timestamp;

  /// Error message (if failed)
  std::string error_message;
};

// ============================================================================
// Database Interface
// ============================================================================

/**
 * @brief SQLite database manager for SeaDrop
 *
 * The database stores:
 * - Trusted devices with their public keys and shared secrets
 * - Transfer history for the activity log
 * - Blocked devices
 */
class SEADROP_API Database {
public:
  Database();
  ~Database();

  // Non-copyable
  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;

  // ========================================================================
  // Initialization
  // ========================================================================

  /**
   * @brief Open or create database
   * @param path Path to SQLite database file
   * @return Success or error
   */
  Result<void> open(const std::filesystem::path &path);

  /**
   * @brief Close database
   */
  void close();

  /**
   * @brief Check if database is open
   */
  bool is_open() const;

  // ========================================================================
  // Devices
  // ========================================================================

  /**
   * @brief Save a device
   * @param device Device info
   * @param shared_key Shared encryption key (for trusted devices)
   * @return Success or error
   */
  Result<void> save_device(const Device &device, const Bytes &shared_key = {});

  /**
   * @brief Get a device by ID
   * @param id Device ID
   * @return Device info or error if not found
   */
  Result<Device> get_device(const DeviceId &id);

  /**
   * @brief Get shared key for a device
   * @param id Device ID
   * @return Shared key or error if not found/not trusted
   */
  Result<Bytes> get_shared_key(const DeviceId &id);

  /**
   * @brief Get all trusted devices
   */
  std::vector<Device> get_trusted_devices();

  /**
   * @brief Get all blocked devices
   */
  std::vector<Device> get_blocked_devices();

  /**
   * @brief Delete a device
   * @param id Device ID
   * @return Success or error
   */
  Result<void> delete_device(const DeviceId &id);

  /**
   * @brief Update device trust level
   */
  Result<void> update_trust_level(const DeviceId &id, TrustLevel level);

  // ========================================================================
  // Transfer History
  // ========================================================================

  /**
   * @brief Add a transfer to history
   * @param entry History entry
   * @return Record ID or error
   */
  Result<int64_t> add_history(const TransferHistoryEntry &entry);

  /**
   * @brief Get transfer history
   * @param limit Maximum entries to return (0 = all)
   * @param offset Offset for pagination
   * @return List of history entries
   */
  std::vector<TransferHistoryEntry> get_history(size_t limit = 100,
                                                size_t offset = 0);

  /**
   * @brief Get transfer history for a specific device
   * @param device_id Device to filter by
   * @param limit Maximum entries
   * @return List of history entries
   */
  std::vector<TransferHistoryEntry>
  get_device_history(const DeviceId &device_id, size_t limit = 50);

  /**
   * @brief Get total transfer statistics
   */
  struct TransferStats {
    int64_t total_transfers = 0;
    int64_t total_bytes_sent = 0;
    int64_t total_bytes_received = 0;
    int64_t total_files_sent = 0;
    int64_t total_files_received = 0;
    int64_t failed_transfers = 0;
  };
  TransferStats get_transfer_stats();

  /**
   * @brief Delete transfer history entry
   */
  Result<void> delete_history_entry(int64_t id);

  /**
   * @brief Clear all transfer history
   */
  Result<void> clear_history();

  /**
   * @brief Clear history older than given time
   */
  Result<void>
  clear_history_before(std::chrono::system_clock::time_point before);

  // ========================================================================
  // Maintenance
  // ========================================================================

  /**
   * @brief Vacuum database (reclaim space)
   */
  Result<void> vacuum();

  /**
   * @brief Get database size in bytes
   */
  int64_t get_size() const;

  /**
   * @brief Run integrity check
   * @return True if database is OK
   */
  bool integrity_check();

  /**
   * @brief Backup database to file
   */
  Result<void> backup(const std::filesystem::path &backup_path);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace seadrop

#endif // SEADROP_DATABASE_H

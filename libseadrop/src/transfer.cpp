/**
 * @file transfer.cpp
 * @brief File transfer protocol implementation
 */

// Standard library includes FIRST
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <unordered_map>

// Project includes LAST
#include "seadrop/config.h"
#include "seadrop/security.h"
#include "seadrop/transfer.h"

namespace seadrop {

// ============================================================================
// Transfer State Names
// ============================================================================

const char *transfer_state_name(TransferState state) {
  switch (state) {
  case TransferState::Pending:
    return "Pending";
  case TransferState::AwaitingAccept:
    return "Awaiting Accept";
  case TransferState::Preparing:
    return "Preparing";
  case TransferState::InProgress:
    return "In Progress";
  case TransferState::Paused:
    return "Paused";
  case TransferState::Completed:
    return "Completed";
  case TransferState::Cancelled:
    return "Cancelled";
  case TransferState::Rejected:
    return "Rejected";
  case TransferState::Failed:
    return "Failed";
  default:
    return "Unknown";
  }
}

// ============================================================================
// TransferProgress Helper Methods
// ============================================================================

std::string TransferProgress::speed_string() const {
  return format_speed(speed_bps);
}

std::string TransferProgress::eta_string() const {
  return format_duration(
      std::chrono::duration_cast<std::chrono::milliseconds>(eta));
}

std::string TransferProgress::progress_string() const {
  return format_bytes(bytes_transferred) + " / " + format_bytes(total_bytes);
}

// ============================================================================
// Utility Functions
// ============================================================================

std::filesystem::path generate_unique_filename(
    const std::filesystem::path &path,
    const std::vector<std::filesystem::path> &existing_files) {

  // Check if file already exists in list
  bool exists = false;
  for (const auto &existing : existing_files) {
    if (existing.filename() == path.filename()) {
      exists = true;
      break;
    }
  }

  if (!exists && !std::filesystem::exists(path)) {
    return path;
  }

  // Generate unique name: file.txt -> file (1).txt -> file (2).txt
  std::string stem = path.stem().string();
  std::string ext = path.extension().string();
  std::filesystem::path parent = path.parent_path();

  for (int i = 1; i < 10000; ++i) {
    std::ostringstream oss;
    oss << stem << " (" << i << ")" << ext;

    std::filesystem::path candidate = parent / oss.str();

    // Check against existing list
    bool found = false;
    for (const auto &existing : existing_files) {
      if (existing.filename() == candidate.filename()) {
        found = true;
        break;
      }
    }

    if (!found && !std::filesystem::exists(candidate)) {
      return candidate;
    }
  }

  // Fallback: add timestamp
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::ostringstream oss;
  oss << stem << "_" << time << ext;
  return parent / oss.str();
}

Result<std::array<Byte, 32>>
calculate_file_checksum(const std::filesystem::path &path) {
  auto result = hash_file(path.string());
  if (result.is_error()) {
    return result.error();
  }
  return result.value();
}

std::string detect_mime_type(const std::filesystem::path &path) {
  std::string ext = path.extension().string();

  // Convert to lowercase
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  // Common MIME types
  static const std::unordered_map<std::string, const char *> mime_map = {
      // Images
      {".jpg", "image/jpeg"},
      {".jpeg", "image/jpeg"},
      {".png", "image/png"},
      {".gif", "image/gif"},
      {".webp", "image/webp"},
      {".svg", "image/svg+xml"},
      {".ico", "image/x-icon"},
      {".bmp", "image/bmp"},

      // Documents
      {".pdf", "application/pdf"},
      {".doc", "application/msword"},
      {".docx", "application/"
                "vnd.openxmlformats-officedocument.wordprocessingml.document"},
      {".xls", "application/vnd.ms-excel"},
      {".xlsx",
       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
      {".ppt", "application/vnd.ms-powerpoint"},
      {".pptx",
       "application/"
       "vnd.openxmlformats-officedocument.presentationml.presentation"},
      {".odt", "application/vnd.oasis.opendocument.text"},

      // Text
      {".txt", "text/plain"},
      {".html", "text/html"},
      {".htm", "text/html"},
      {".css", "text/css"},
      {".js", "text/javascript"},
      {".json", "application/json"},
      {".xml", "application/xml"},
      {".csv", "text/csv"},
      {".md", "text/markdown"},

      // Audio
      {".mp3", "audio/mpeg"},
      {".wav", "audio/wav"},
      {".ogg", "audio/ogg"},
      {".flac", "audio/flac"},
      {".m4a", "audio/mp4"},

      // Video
      {".mp4", "video/mp4"},
      {".webm", "video/webm"},
      {".mkv", "video/x-matroska"},
      {".avi", "video/x-msvideo"},
      {".mov", "video/quicktime"},

      // Archives
      {".zip", "application/zip"},
      {".tar", "application/x-tar"},
      {".gz", "application/gzip"},
      {".7z", "application/x-7z-compressed"},
      {".rar", "application/vnd.rar"},

      // Code
      {".c", "text/x-c"},
      {".cpp", "text/x-c++"},
      {".h", "text/x-c"},
      {".hpp", "text/x-c++"},
      {".py", "text/x-python"},
      {".java", "text/x-java"},
      {".rs", "text/x-rust"},
      {".go", "text/x-go"},
      {".sh", "application/x-sh"},
  };

  auto it = mime_map.find(ext);
  if (it != mime_map.end()) {
    return it->second;
  }

  return "application/octet-stream";
}

std::string format_bytes(uint64_t bytes) {
  constexpr uint64_t KB = 1024;
  constexpr uint64_t MB = KB * 1024;
  constexpr uint64_t GB = MB * 1024;
  constexpr uint64_t TB = GB * 1024;

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1);

  if (bytes >= TB) {
    oss << (static_cast<double>(bytes) / TB) << " TB";
  } else if (bytes >= GB) {
    oss << (static_cast<double>(bytes) / GB) << " GB";
  } else if (bytes >= MB) {
    oss << (static_cast<double>(bytes) / MB) << " MB";
  } else if (bytes >= KB) {
    oss << (static_cast<double>(bytes) / KB) << " KB";
  } else {
    oss << bytes << " B";
  }

  return oss.str();
}

std::string format_speed(double bytes_per_second) {
  return format_bytes(static_cast<uint64_t>(bytes_per_second)) + "/s";
}

std::string format_duration(std::chrono::milliseconds duration) {
  auto total_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(duration).count();

  if (total_seconds < 0) {
    return "calculating...";
  }

  int hours = static_cast<int>(total_seconds / 3600);
  int minutes = static_cast<int>((total_seconds % 3600) / 60);
  int seconds = static_cast<int>(total_seconds % 60);

  std::ostringstream oss;

  if (hours > 0) {
    oss << hours << "h " << minutes << "m";
  } else if (minutes > 0) {
    oss << minutes << "m " << seconds << "s";
  } else {
    oss << seconds << "s";
  }

  return oss.str();
}

// ============================================================================
// TransferManager Implementation
// ============================================================================

class TransferManager::Impl {
public:
  TransferOptions default_options;
  bool initialized = false;
  std::mutex mutex;

  // Active transfers
  std::map<std::string, TransferProgress> active_transfers;
  std::map<std::string, TransferRequest> pending_requests;
  std::map<std::string, TransferResult> completed_transfers;

  // Callbacks
  std::function<void(const TransferRequest &)> request_cb;
  std::function<void(const TransferProgress &)> progress_cb;
  std::function<void(const TransferResult &)> complete_cb;
  std::function<void(const FileInfo &)> file_received_cb;
  std::function<void(const TransferId &, const Error &)> error_cb;

  std::string transfer_key(const TransferId &id) const { return id.to_hex(); }
};

TransferManager::TransferManager() : impl_(std::make_unique<Impl>()) {}
TransferManager::~TransferManager() { shutdown(); }

Result<void> TransferManager::init(const TransferOptions &options) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (impl_->initialized) {
    return Error(ErrorCode::AlreadyInitialized,
                 "TransferManager already initialized");
  }

  impl_->default_options = options;

  // Set default save directory if not specified
  if (impl_->default_options.save_directory.empty()) {
    impl_->default_options.save_directory =
        SeaDropConfig::get_default_download_path();
  }

  impl_->initialized = true;
  return Result<void>::ok();
}

void TransferManager::shutdown() {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  // Cancel all active transfers
  for (auto &[key, progress] : impl_->active_transfers) {
    progress.state = TransferState::Cancelled;
  }

  impl_->active_transfers.clear();
  impl_->pending_requests.clear();
  impl_->initialized = false;
}

Result<TransferId> TransferManager::send_file(const std::filesystem::path &path,
                                              const TransferOptions &options) {
  std::vector<std::filesystem::path> paths = {path};
  return send_files(paths, options);
}

Result<TransferId>
TransferManager::send_files(const std::vector<std::filesystem::path> &paths,
                            const TransferOptions &options) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (!impl_->initialized) {
    return Error(ErrorCode::NotInitialized, "TransferManager not initialized");
  }

  if (paths.empty()) {
    return Error(ErrorCode::InvalidArgument, "No files to send");
  }

  // Validate all paths exist
  uint64_t total_size = 0;
  std::vector<FileInfo> files;

  for (const auto &path : paths) {
    if (!std::filesystem::exists(path)) {
      return Error(ErrorCode::FileNotFound, "File not found: " + path.string());
    }

    FileInfo file;
    file.relative_path = path.filename();
    file.name = path.filename().string();
    file.size = std::filesystem::file_size(path);
    file.mime_type = detect_mime_type(path);

    // Calculate checksum
    auto checksum_result = calculate_file_checksum(path);
    if (checksum_result.is_ok()) {
      file.checksum = checksum_result.value();
    }

    total_size += file.size;
    files.push_back(std::move(file));
  }

  // Create transfer
  TransferId id = TransferId::generate();

  TransferProgress progress;
  progress.id = id;
  progress.state = TransferState::Pending;
  progress.progress = 0.0;
  progress.bytes_transferred = 0;
  progress.total_bytes = total_size;
  progress.total_files = static_cast<int>(files.size());
  progress.completed_files = 0;

  impl_->active_transfers[impl_->transfer_key(id)] = progress;

  // TODO: Actually initiate the transfer over the connection

  return id;
}

Result<TransferId>
TransferManager::send_directory(const std::filesystem::path &path,
                                const TransferOptions &options) {
  if (!std::filesystem::is_directory(path)) {
    return Error(ErrorCode::InvalidArgument, "Path is not a directory");
  }

  // Collect all files recursively
  std::vector<std::filesystem::path> files;
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(path)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path());
    }
  }

  return send_files(files, options);
}

Result<TransferId> TransferManager::send_text(const std::string &text,
                                              const std::string &filename) {
  Bytes data(text.begin(), text.end());
  return send_data(data, filename, "text/plain");
}

Result<TransferId> TransferManager::send_data(const Bytes &data,
                                              const std::string &filename,
                                              const std::string &mime_type) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  if (!impl_->initialized) {
    return Error(ErrorCode::NotInitialized, "TransferManager not initialized");
  }

  TransferId id = TransferId::generate();

  FileInfo file;
  file.relative_path = filename;
  file.name = filename;
  file.size = data.size();
  file.mime_type = mime_type;

  // Calculate checksum
  auto hash_result = hash(data);
  if (hash_result.is_ok()) {
    file.checksum = hash_result.value();
  }

  TransferProgress progress;
  progress.id = id;
  progress.state = TransferState::Pending;
  progress.total_bytes = data.size();
  progress.total_files = 1;

  impl_->active_transfers[impl_->transfer_key(id)] = progress;

  return id;
}

Result<void> TransferManager::accept_transfer(const TransferId &request_id,
                                              const TransferOptions &options) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->transfer_key(request_id);
  auto it = impl_->pending_requests.find(key);
  if (it == impl_->pending_requests.end()) {
    return Error(ErrorCode::RecordNotFound, "Transfer request not found");
  }

  // Move from pending to active
  TransferProgress progress;
  progress.id = request_id;
  progress.state = TransferState::InProgress;
  progress.total_bytes = it->second.total_size;
  progress.total_files = static_cast<int>(it->second.file_count);

  impl_->active_transfers[key] = progress;
  impl_->pending_requests.erase(it);

  // TODO: Signal acceptance to sender

  return Result<void>::ok();
}

void TransferManager::reject_transfer(const TransferId &request_id,
                                      const std::string &reason) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->transfer_key(request_id);
  impl_->pending_requests.erase(key);

  // TODO: Signal rejection to sender
}

Result<void> TransferManager::pause_transfer(const TransferId &transfer_id) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->transfer_key(transfer_id);
  auto it = impl_->active_transfers.find(key);
  if (it == impl_->active_transfers.end()) {
    return Error(ErrorCode::RecordNotFound, "Transfer not found");
  }

  if (it->second.state != TransferState::InProgress) {
    return Error(ErrorCode::InvalidState, "Transfer not in progress");
  }

  it->second.state = TransferState::Paused;
  return Result<void>::ok();
}

Result<void> TransferManager::resume_transfer(const TransferId &transfer_id) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->transfer_key(transfer_id);
  auto it = impl_->active_transfers.find(key);
  if (it == impl_->active_transfers.end()) {
    return Error(ErrorCode::RecordNotFound, "Transfer not found");
  }

  if (it->second.state != TransferState::Paused) {
    return Error(ErrorCode::InvalidState, "Transfer not paused");
  }

  it->second.state = TransferState::InProgress;
  return Result<void>::ok();
}

void TransferManager::cancel_transfer(const TransferId &transfer_id) {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->transfer_key(transfer_id);
  auto it = impl_->active_transfers.find(key);
  if (it != impl_->active_transfers.end()) {
    it->second.state = TransferState::Cancelled;

    // Create result
    TransferResult result;
    result.id = transfer_id;
    result.state = TransferState::Cancelled;
    result.bytes_transferred = it->second.bytes_transferred;

    impl_->completed_transfers[key] = result;
    impl_->active_transfers.erase(it);

    if (impl_->complete_cb) {
      impl_->complete_cb(result);
    }
  }
}

Result<TransferProgress>
TransferManager::get_progress(const TransferId &transfer_id) const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->transfer_key(transfer_id);
  auto it = impl_->active_transfers.find(key);
  if (it == impl_->active_transfers.end()) {
    return Error(ErrorCode::RecordNotFound, "Transfer not found");
  }

  return it->second;
}

Result<TransferResult>
TransferManager::get_result(const TransferId &transfer_id) const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  auto key = impl_->transfer_key(transfer_id);
  auto it = impl_->completed_transfers.find(key);
  if (it == impl_->completed_transfers.end()) {
    return Error(ErrorCode::RecordNotFound, "Transfer result not found");
  }

  return it->second;
}

std::vector<TransferProgress> TransferManager::get_active_transfers() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  std::vector<TransferProgress> result;
  for (const auto &[key, progress] : impl_->active_transfers) {
    result.push_back(progress);
  }
  return result;
}

std::vector<TransferRequest> TransferManager::get_pending_requests() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);

  std::vector<TransferRequest> result;
  for (const auto &[key, request] : impl_->pending_requests) {
    result.push_back(request);
  }
  return result;
}

void TransferManager::set_default_options(const TransferOptions &options) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->default_options = options;
}

TransferOptions TransferManager::get_default_options() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->default_options;
}

void TransferManager::on_transfer_request(
    std::function<void(const TransferRequest &)> callback) {
  impl_->request_cb = std::move(callback);
}

void TransferManager::on_progress(
    std::function<void(const TransferProgress &)> callback) {
  impl_->progress_cb = std::move(callback);
}

void TransferManager::on_complete(
    std::function<void(const TransferResult &)> callback) {
  impl_->complete_cb = std::move(callback);
}

void TransferManager::on_file_received(
    std::function<void(const FileInfo &)> callback) {
  impl_->file_received_cb = std::move(callback);
}

void TransferManager::on_error(
    std::function<void(const TransferId &, const Error &)> callback) {
  impl_->error_cb = std::move(callback);
}

} // namespace seadrop

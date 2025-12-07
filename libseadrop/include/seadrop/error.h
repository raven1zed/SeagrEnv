/**
 * @file error.h
 * @brief Error codes and result types for SeaDrop
 *
 * SeaDrop uses a Result type pattern for error handling,
 * avoiding exceptions for better cross-platform compatibility.
 */

#ifndef SEADROP_ERROR_H
#define SEADROP_ERROR_H

#include "platform.h"
#include <optional>
#include <string>
#include <variant>


namespace seadrop {

// ============================================================================
// Error Codes
// ============================================================================

enum class ErrorCode : int {
  // Success (0)
  Success = 0,

  // General errors (1-99)
  Unknown = 1,
  InvalidArgument = 2,
  InvalidState = 3,
  NotInitialized = 4,
  AlreadyInitialized = 5,
  NotSupported = 6,
  Timeout = 7,
  Cancelled = 8,

  // Discovery errors (100-199)
  DiscoveryFailed = 100,
  DiscoveryNotAvailable = 101,
  BluetoothOff = 102,
  BluetoothNotSupported = 103,
  BleAdvertiseFailed = 104,
  BleScanFailed = 105,

  // Connection errors (200-299)
  ConnectionFailed = 200,
  ConnectionLost = 201,
  ConnectionRefused = 202,
  ConnectionTimeout = 203,
  WifiDirectNotAvailable = 204,
  WifiDirectFailed = 205,
  GroupFormationFailed = 206,
  PeerNotFound = 207,
  AlreadyConnected = 208,
  NotConnected = 209,

  // Transfer errors (300-399)
  TransferFailed = 300,
  TransferCancelled = 301,
  TransferRejected = 302,
  FileNotFound = 303,
  FileReadError = 304,
  FileWriteError = 305,
  DiskFull = 306,
  FileTooLarge = 307,
  InvalidFileType = 308,
  ChecksumMismatch = 309,

  // Security errors (400-499)
  SecurityError = 400,
  EncryptionFailed = 401,
  DecryptionFailed = 402,
  AuthenticationFailed = 403,
  KeyExchangeFailed = 404,
  InvalidSignature = 405,
  TrustDenied = 406,
  DeviceNotTrusted = 407,
  PairingFailed = 408,
  PairingRejected = 409,

  // Platform errors (500-599)
  PlatformError = 500,
  PermissionDenied = 501,
  ServiceUnavailable = 502,
  HardwareNotAvailable = 503,
  DriverError = 504,

  // Database errors (600-699)
  DatabaseError = 600,
  DatabaseCorrupted = 601,
  DatabaseLocked = 602,
  RecordNotFound = 603
};

// ============================================================================
// Error Information
// ============================================================================

/**
 * @brief Detailed error information
 */
struct Error {
  ErrorCode code = ErrorCode::Success;
  std::string message;
  std::string details;  // Additional context
  std::string location; // Function/file where error occurred

  Error() = default;

  explicit Error(ErrorCode c, std::string msg = "", std::string det = "")
      : code(c), message(std::move(msg)), details(std::move(det)) {}

  /// Check if this represents an error
  bool is_error() const { return code != ErrorCode::Success; }

  /// Check if this represents success
  bool is_ok() const { return code == ErrorCode::Success; }

  /// Get human-readable error string
  std::string to_string() const;

  /// Create success result
  static Error ok() { return Error(ErrorCode::Success); }
};

// ============================================================================
// Result Type
// ============================================================================

/**
 * @brief Result type that holds either a value or an error
 *
 * Usage:
 *   Result<int> result = some_function();
 *   if (result) {
 *       int value = result.value();
 *   } else {
 *       Error err = result.error();
 *   }
 */
template <typename T> class Result {
public:
  /// Construct with success value
  Result(T value) : data_(std::move(value)) {}

  /// Construct with error
  Result(Error error) : data_(std::move(error)) {}

  /// Construct with error code
  Result(ErrorCode code, std::string message = "")
      : data_(Error(code, std::move(message))) {}

  /// Check if result is success
  bool is_ok() const { return std::holds_alternative<T>(data_); }

  /// Check if result is error
  bool is_error() const { return std::holds_alternative<Error>(data_); }

  /// Boolean conversion (true = success)
  explicit operator bool() const { return is_ok(); }

  /// Get the value (undefined behavior if error)
  T &value() & { return std::get<T>(data_); }
  const T &value() const & { return std::get<T>(data_); }
  T &&value() && { return std::get<T>(std::move(data_)); }

  /// Get the error (undefined behavior if success)
  Error &error() & { return std::get<Error>(data_); }
  const Error &error() const & { return std::get<Error>(data_); }

  /// Get value or default
  T value_or(T default_value) const {
    return is_ok() ? std::get<T>(data_) : std::move(default_value);
  }

  /// Get optional value
  std::optional<T> to_optional() const {
    return is_ok() ? std::optional<T>(std::get<T>(data_)) : std::nullopt;
  }

private:
  std::variant<T, Error> data_;
};

/**
 * @brief Specialization for void result (success or error, no value)
 */
template <> class Result<void> {
public:
  /// Construct success
  Result() : error_(std::nullopt) {}

  /// Construct with error
  Result(Error error) : error_(std::move(error)) {}

  /// Construct with error code
  Result(ErrorCode code, std::string message = "")
      : error_(Error(code, std::move(message))) {}

  /// Check if result is success
  bool is_ok() const { return !error_.has_value(); }

  /// Check if result is error
  bool is_error() const { return error_.has_value(); }

  /// Boolean conversion (true = success)
  explicit operator bool() const { return is_ok(); }

  /// Get the error
  Error &error() { return error_.value(); }
  const Error &error() const { return error_.value(); }

  /// Create success result
  static Result ok() { return Result(); }

private:
  std::optional<Error> error_;
};

// ============================================================================
// Convenience Macros
// ============================================================================

/// Return early if result is error
#define SEADROP_TRY(result)                                                    \
  do {                                                                         \
    auto &&_result = (result);                                                 \
    if (_result.is_error()) {                                                  \
      return _result.error();                                                  \
    }                                                                          \
  } while (0)

/// Return early with error if condition is false
#define SEADROP_REQUIRE(condition, error_code, message)                        \
  do {                                                                         \
    if (!(condition)) {                                                        \
      return ::seadrop::Error(error_code, message);                            \
    }                                                                          \
  } while (0)

// ============================================================================
// Error Code Helpers
// ============================================================================

/// Get human-readable name for error code
SEADROP_API const char *error_code_name(ErrorCode code);

/// Get description for error code
SEADROP_API const char *error_code_description(ErrorCode code);

/// Check if error code is recoverable
SEADROP_API bool is_recoverable(ErrorCode code);

} // namespace seadrop

#endif // SEADROP_ERROR_H

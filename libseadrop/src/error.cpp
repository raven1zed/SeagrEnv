/**
 * @file error.cpp
 * @brief Error handling implementation
 */

#include "seadrop/error.h"
#include <sstream>

namespace seadrop {

// ============================================================================
// Error Code Names
// ============================================================================

const char *error_code_name(ErrorCode code) {
  switch (code) {
  case ErrorCode::Success:
    return "Success";
  case ErrorCode::Unknown:
    return "Unknown";
  case ErrorCode::InvalidArgument:
    return "InvalidArgument";
  case ErrorCode::InvalidState:
    return "InvalidState";
  case ErrorCode::NotInitialized:
    return "NotInitialized";
  case ErrorCode::AlreadyInitialized:
    return "AlreadyInitialized";
  case ErrorCode::NotSupported:
    return "NotSupported";
  case ErrorCode::Timeout:
    return "Timeout";
  case ErrorCode::Cancelled:
    return "Cancelled";

  case ErrorCode::DiscoveryFailed:
    return "DiscoveryFailed";
  case ErrorCode::DiscoveryNotAvailable:
    return "DiscoveryNotAvailable";
  case ErrorCode::BluetoothOff:
    return "BluetoothOff";
  case ErrorCode::BluetoothNotSupported:
    return "BluetoothNotSupported";
  case ErrorCode::BleAdvertiseFailed:
    return "BleAdvertiseFailed";
  case ErrorCode::BleScanFailed:
    return "BleScanFailed";

  case ErrorCode::ConnectionFailed:
    return "ConnectionFailed";
  case ErrorCode::ConnectionLost:
    return "ConnectionLost";
  case ErrorCode::ConnectionRefused:
    return "ConnectionRefused";
  case ErrorCode::ConnectionTimeout:
    return "ConnectionTimeout";
  case ErrorCode::WifiDirectNotAvailable:
    return "WifiDirectNotAvailable";
  case ErrorCode::WifiDirectFailed:
    return "WifiDirectFailed";
  case ErrorCode::GroupFormationFailed:
    return "GroupFormationFailed";
  case ErrorCode::PeerNotFound:
    return "PeerNotFound";
  case ErrorCode::AlreadyConnected:
    return "AlreadyConnected";
  case ErrorCode::NotConnected:
    return "NotConnected";

  case ErrorCode::TransferFailed:
    return "TransferFailed";
  case ErrorCode::TransferCancelled:
    return "TransferCancelled";
  case ErrorCode::TransferRejected:
    return "TransferRejected";
  case ErrorCode::FileNotFound:
    return "FileNotFound";
  case ErrorCode::FileReadError:
    return "FileReadError";
  case ErrorCode::FileWriteError:
    return "FileWriteError";
  case ErrorCode::DiskFull:
    return "DiskFull";
  case ErrorCode::FileTooLarge:
    return "FileTooLarge";
  case ErrorCode::InvalidFileType:
    return "InvalidFileType";
  case ErrorCode::ChecksumMismatch:
    return "ChecksumMismatch";

  case ErrorCode::SecurityError:
    return "SecurityError";
  case ErrorCode::EncryptionFailed:
    return "EncryptionFailed";
  case ErrorCode::DecryptionFailed:
    return "DecryptionFailed";
  case ErrorCode::AuthenticationFailed:
    return "AuthenticationFailed";
  case ErrorCode::KeyExchangeFailed:
    return "KeyExchangeFailed";
  case ErrorCode::InvalidSignature:
    return "InvalidSignature";
  case ErrorCode::TrustDenied:
    return "TrustDenied";
  case ErrorCode::DeviceNotTrusted:
    return "DeviceNotTrusted";
  case ErrorCode::PairingFailed:
    return "PairingFailed";
  case ErrorCode::PairingRejected:
    return "PairingRejected";

  case ErrorCode::PlatformError:
    return "PlatformError";
  case ErrorCode::PermissionDenied:
    return "PermissionDenied";
  case ErrorCode::ServiceUnavailable:
    return "ServiceUnavailable";
  case ErrorCode::HardwareNotAvailable:
    return "HardwareNotAvailable";
  case ErrorCode::DriverError:
    return "DriverError";

  case ErrorCode::DatabaseError:
    return "DatabaseError";
  case ErrorCode::DatabaseCorrupted:
    return "DatabaseCorrupted";
  case ErrorCode::DatabaseLocked:
    return "DatabaseLocked";
  case ErrorCode::RecordNotFound:
    return "RecordNotFound";

  default:
    return "UnknownError";
  }
}

// ============================================================================
// Error Code Descriptions
// ============================================================================

const char *error_code_description(ErrorCode code) {
  switch (code) {
  case ErrorCode::Success:
    return "Operation completed successfully";
  case ErrorCode::Unknown:
    return "An unknown error occurred";
  case ErrorCode::InvalidArgument:
    return "Invalid argument provided";
  case ErrorCode::InvalidState:
    return "Operation not valid in current state";
  case ErrorCode::NotInitialized:
    return "Component not initialized";
  case ErrorCode::AlreadyInitialized:
    return "Component already initialized";
  case ErrorCode::NotSupported:
    return "Operation not supported";
  case ErrorCode::Timeout:
    return "Operation timed out";
  case ErrorCode::Cancelled:
    return "Operation was cancelled";

  case ErrorCode::DiscoveryFailed:
    return "Device discovery failed";
  case ErrorCode::DiscoveryNotAvailable:
    return "Discovery service not available";
  case ErrorCode::BluetoothOff:
    return "Bluetooth is disabled";
  case ErrorCode::BluetoothNotSupported:
    return "Bluetooth not supported on this device";
  case ErrorCode::BleAdvertiseFailed:
    return "BLE advertising failed";
  case ErrorCode::BleScanFailed:
    return "BLE scanning failed";

  case ErrorCode::ConnectionFailed:
    return "Failed to establish connection";
  case ErrorCode::ConnectionLost:
    return "Connection was lost unexpectedly";
  case ErrorCode::ConnectionRefused:
    return "Connection was refused by peer";
  case ErrorCode::ConnectionTimeout:
    return "Connection attempt timed out";
  case ErrorCode::WifiDirectNotAvailable:
    return "WiFi Direct not available";
  case ErrorCode::WifiDirectFailed:
    return "WiFi Direct operation failed";
  case ErrorCode::GroupFormationFailed:
    return "Failed to form WiFi Direct group";
  case ErrorCode::PeerNotFound:
    return "Peer device not found";
  case ErrorCode::AlreadyConnected:
    return "Already connected to a device";
  case ErrorCode::NotConnected:
    return "Not connected to any device";

  case ErrorCode::TransferFailed:
    return "File transfer failed";
  case ErrorCode::TransferCancelled:
    return "File transfer was cancelled";
  case ErrorCode::TransferRejected:
    return "File transfer was rejected";
  case ErrorCode::FileNotFound:
    return "File not found";
  case ErrorCode::FileReadError:
    return "Error reading file";
  case ErrorCode::FileWriteError:
    return "Error writing file";
  case ErrorCode::DiskFull:
    return "Disk is full";
  case ErrorCode::FileTooLarge:
    return "File is too large";
  case ErrorCode::InvalidFileType:
    return "Invalid file type";
  case ErrorCode::ChecksumMismatch:
    return "File checksum verification failed";

  case ErrorCode::SecurityError:
    return "Security error occurred";
  case ErrorCode::EncryptionFailed:
    return "Encryption failed";
  case ErrorCode::DecryptionFailed:
    return "Decryption failed";
  case ErrorCode::AuthenticationFailed:
    return "Authentication failed";
  case ErrorCode::KeyExchangeFailed:
    return "Key exchange failed";
  case ErrorCode::InvalidSignature:
    return "Invalid digital signature";
  case ErrorCode::TrustDenied:
    return "Trust relationship denied";
  case ErrorCode::DeviceNotTrusted:
    return "Device is not trusted";
  case ErrorCode::PairingFailed:
    return "Device pairing failed";
  case ErrorCode::PairingRejected:
    return "Pairing was rejected";

  case ErrorCode::PlatformError:
    return "Platform-specific error occurred";
  case ErrorCode::PermissionDenied:
    return "Permission denied";
  case ErrorCode::ServiceUnavailable:
    return "Required service unavailable";
  case ErrorCode::HardwareNotAvailable:
    return "Required hardware not available";
  case ErrorCode::DriverError:
    return "Driver error occurred";

  case ErrorCode::DatabaseError:
    return "Database error occurred";
  case ErrorCode::DatabaseCorrupted:
    return "Database is corrupted";
  case ErrorCode::DatabaseLocked:
    return "Database is locked";
  case ErrorCode::RecordNotFound:
    return "Record not found in database";

  default:
    return "Unknown error occurred";
  }
}

// ============================================================================
// Recoverability
// ============================================================================

bool is_recoverable(ErrorCode code) {
  switch (code) {
  // Non-recoverable errors
  case ErrorCode::NotSupported:
  case ErrorCode::BluetoothNotSupported:
  case ErrorCode::HardwareNotAvailable:
  case ErrorCode::DatabaseCorrupted:
    return false;

  // All others are potentially recoverable
  default:
    return true;
  }
}

// ============================================================================
// Error::to_string
// ============================================================================

std::string Error::to_string() const {
  std::ostringstream oss;

  oss << error_code_name(code);

  if (!message.empty()) {
    oss << ": " << message;
  }

  if (!details.empty()) {
    oss << " (" << details << ")";
  }

  if (!location.empty()) {
    oss << " [" << location << "]";
  }

  return oss.str();
}

} // namespace seadrop

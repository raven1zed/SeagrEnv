/**
 * @file types.cpp
 * @brief Core type implementations
 */

#include "seadrop/types.h"
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>

namespace seadrop {

// ============================================================================
// DeviceId
// ============================================================================

std::string DeviceId::to_hex() const {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (size_t i = 0; i < SIZE; ++i) {
    oss << std::setw(2) << static_cast<int>(data[i]);
  }
  return oss.str();
}

std::optional<DeviceId> DeviceId::from_hex(const std::string &hex) {
  if (hex.length() != SIZE * 2) {
    return std::nullopt;
  }

  DeviceId id;
  for (size_t i = 0; i < SIZE; ++i) {
    std::string byte_str = hex.substr(i * 2, 2);
    char *end;
    unsigned long val = std::strtoul(byte_str.c_str(), &end, 16);
    if (end != byte_str.c_str() + 2 || val > 255) {
      return std::nullopt;
    }
    id.data[i] = static_cast<Byte>(val);
  }

  return id;
}

bool DeviceId::is_zero() const {
  for (size_t i = 0; i < SIZE; ++i) {
    if (data[i] != 0)
      return false;
  }
  return true;
}

// ============================================================================
// TransferId
// ============================================================================

std::string TransferId::to_hex() const {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (size_t i = 0; i < SIZE; ++i) {
    oss << std::setw(2) << static_cast<int>(data[i]);
  }
  return oss.str();
}

TransferId TransferId::generate() {
  TransferId id;

  // Use C++ standard library random for ID generation
  // Note: For production, consider using libsodium's random_bytes
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint16_t> dis(0, 255);

  for (size_t i = 0; i < SIZE; ++i) {
    id.data[i] = static_cast<Byte>(dis(gen));
  }

  return id;
}

} // namespace seadrop

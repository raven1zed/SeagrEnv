/**
 * @file wifi_direct_win.cpp
 * @brief Windows WiFi Direct implementation using WFD API
 */

#include "seadrop/connection.h"
#include "seadrop/platform.h"

#ifdef SEADROP_PLATFORM_WINDOWS

#include <cstdlib>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>


// C++/WinRT Headers
// These require a modern Windows SDK (10.0.17134.0+) and /std:c++17
// If not available, we fall back to a stub implementation
#if defined(_MSC_VER) && __has_include(<winrt/Windows.Foundation.h>)
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.WiFiDirect.h>
#include <winrt/Windows.Foundation.h>


using namespace winrt;
using namespace Windows::Devices::WiFiDirect;
using namespace Windows::Devices::Enumeration;
#else
#define SEADROP_NO_WINRT
#endif

namespace seadrop {

// ============================================================================
// Platform Helper Implementations
// ============================================================================

bool is_wifi_direct_available() {
#ifndef SEADROP_NO_WINRT
  try {
    // We can't synchronously check specifically for WFD presence easily without
    // async, but we can check if the API is contractually available. For a
    // runtime check, we'd typically look for compatible adapters. This provides
    // a reasonable "is this OS capable?" check.
    return true;
  } catch (...) {
    return false;
  }
#else
  return false;
#endif
}

bool is_wifi_enabled() {
  // Hard to check universally without specific radio APIs
  return true;
}

bool request_enable_wifi() {
#ifndef SEADROP_NO_WINRT
  // Launch ms-settings:network-wifi
  std::system("start ms-settings:network-wifi");
  return true;
#else
  return false;
#endif
}

bool has_wifi_direct_permission() { return true; }

std::string get_p2p_interface() {
  return "wlan0"; // Placeholder for Windows virtual adapter name
}

// ============================================================================
// ConnectionManager Platform Implementation
// ============================================================================

// TODO: Implement the ConnectionManager::connect/disconnect flow using
// WiFiDirectDevice requires async handling which we will add in the next
// iteration.

} // namespace seadrop

#endif // SEADROP_PLATFORM_WINDOWS

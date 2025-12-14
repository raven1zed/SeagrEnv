/**
 * @file platform.h
 * @brief Platform detection and abstraction macros for SeaDrop
 *
 * This header provides compile-time platform detection and defines
 * the appropriate macros for cross-platform development.
 */

#ifndef SEADROP_PLATFORM_H
#define SEADROP_PLATFORM_H

// ============================================================================
// Platform Detection
// ============================================================================

#if defined(__ANDROID__)
#define SEADROP_PLATFORM_ANDROID 1
#define SEADROP_PLATFORM_LINUX 1
#define SEADROP_PLATFORM_NAME "Android"
#elif defined(__linux__)
#define SEADROP_PLATFORM_LINUX 1
#define SEADROP_PLATFORM_NAME "Linux"
#else
#error "Unsupported platform. SeaDrop only supports Linux and Android."
#endif

// ============================================================================
// Architecture Detection
// ============================================================================

#if defined(__x86_64__) || defined(_M_X64)
#define SEADROP_ARCH_X64 1
#define SEADROP_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#define SEADROP_ARCH_X86 1
#define SEADROP_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define SEADROP_ARCH_ARM64 1
#define SEADROP_ARCH_NAME "arm64"
#elif defined(__arm__) || defined(_M_ARM)
#define SEADROP_ARCH_ARM 1
#define SEADROP_ARCH_NAME "arm"
#else
#define SEADROP_ARCH_UNKNOWN 1
#define SEADROP_ARCH_NAME "unknown"
#endif

// ============================================================================
// Compiler Detection
// ============================================================================

#if defined(__clang__)
#define SEADROP_COMPILER_CLANG 1
#define SEADROP_COMPILER_NAME "Clang"
#define SEADROP_COMPILER_VERSION __clang_major__
#elif defined(__GNUC__)
#define SEADROP_COMPILER_GCC 1
#define SEADROP_COMPILER_NAME "GCC"
#define SEADROP_COMPILER_VERSION __GNUC__
#elif defined(_MSC_VER)
#define SEADROP_COMPILER_MSVC 1
#define SEADROP_COMPILER_NAME "MSVC"
#define SEADROP_COMPILER_VERSION _MSC_VER
#else
#define SEADROP_COMPILER_UNKNOWN 1
#define SEADROP_COMPILER_NAME "Unknown"
#define SEADROP_COMPILER_VERSION 0
#endif

// ============================================================================
// Export/Import Macros
// ============================================================================

#ifdef SEADROP_BUILDING_SHARED
#ifdef SEADROP_PLATFORM_WINDOWS
#define SEADROP_API __declspec(dllexport)
#else
#define SEADROP_API __attribute__((visibility("default")))
#endif
#else
#ifdef SEADROP_PLATFORM_WINDOWS
#define SEADROP_API __declspec(dllimport)
#else
#define SEADROP_API
#endif
#endif

#define SEADROP_LOCAL __attribute__((visibility("hidden")))

// ============================================================================
// Utility Macros
// ============================================================================

#define SEADROP_UNUSED(x) (void)(x)

#ifdef SEADROP_COMPILER_MSVC
#define SEADROP_FORCE_INLINE __forceinline
#define SEADROP_NO_INLINE __declspec(noinline)
#else
#define SEADROP_FORCE_INLINE inline __attribute__((always_inline))
#define SEADROP_NO_INLINE __attribute__((noinline))
#endif

// Deprecation warnings
#ifdef SEADROP_COMPILER_MSVC
#define SEADROP_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#define SEADROP_DEPRECATED(msg) __attribute__((deprecated(msg)))
#endif

// Branch prediction hints
#if defined(SEADROP_COMPILER_GCC) || defined(SEADROP_COMPILER_CLANG)
#define SEADROP_LIKELY(x) __builtin_expect(!!(x), 1)
#define SEADROP_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define SEADROP_LIKELY(x) (x)
#define SEADROP_UNLIKELY(x) (x)
#endif

// ============================================================================
// Debug/Release Detection
// ============================================================================

#if defined(DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
#define SEADROP_DEBUG 1
#else
#define SEADROP_RELEASE 1
#endif

// ============================================================================
// Feature Detection
// ============================================================================

// WiFi Direct support
#if defined(SEADROP_PLATFORM_LINUX) || defined(SEADROP_PLATFORM_ANDROID)
#define SEADROP_HAS_WIFI_DIRECT 1
#define SEADROP_WIFI_DIRECT_WPA_SUPPLICANT 1
#elif defined(SEADROP_PLATFORM_WINDOWS)
#define SEADROP_HAS_WIFI_DIRECT 1
#define SEADROP_WIFI_DIRECT_WFD_API 1
#elif defined(SEADROP_PLATFORM_MACOS) || defined(SEADROP_PLATFORM_IOS)
#define SEADROP_HAS_WIFI_DIRECT 1
#define SEADROP_WIFI_DIRECT_MULTIPEER 1
#endif

// Bluetooth support
#if defined(SEADROP_PLATFORM_LINUX)
#ifdef HAS_BLUEZ
#define SEADROP_HAS_BLUETOOTH 1
#define SEADROP_BLUETOOTH_BLUEZ 1
#endif
#elif defined(SEADROP_PLATFORM_WINDOWS)
#define SEADROP_HAS_BLUETOOTH 1
#define SEADROP_BLUETOOTH_WIN32 1
#elif defined(SEADROP_PLATFORM_APPLE)
#define SEADROP_HAS_BLUETOOTH 1
#define SEADROP_BLUETOOTH_COREBLUETOOTH 1
#elif defined(SEADROP_PLATFORM_ANDROID)
#define SEADROP_HAS_BLUETOOTH 1
#define SEADROP_BLUETOOTH_ANDROID 1
#endif

#endif // SEADROP_PLATFORM_H

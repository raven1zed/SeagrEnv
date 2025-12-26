/**
 * @file platform.h
 * @brief Platform detection and abstraction macros for SeaDrop
 *
 * This header provides compile-time platform detection and defines
 * the appropriate macros for cross-platform development.
 * 
 * SeaDrop only supports Linux and Android platforms.
 */

#ifndef SEADROP_PLATFORM_H
#define SEADROP_PLATFORM_H

// ============================================================================
// Platform Detection (Linux and Android only)
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

#if defined(__x86_64__)
#define SEADROP_ARCH_X64 1
#define SEADROP_ARCH_NAME "x86_64"
#elif defined(__i386__)
#define SEADROP_ARCH_X86 1
#define SEADROP_ARCH_NAME "x86"
#elif defined(__aarch64__)
#define SEADROP_ARCH_ARM64 1
#define SEADROP_ARCH_NAME "arm64"
#elif defined(__arm__)
#define SEADROP_ARCH_ARM 1
#define SEADROP_ARCH_NAME "arm"
#else
#define SEADROP_ARCH_UNKNOWN 1
#define SEADROP_ARCH_NAME "unknown"
#endif

// ============================================================================
// Compiler Detection (GCC and Clang only)
// ============================================================================

#if defined(__clang__)
#define SEADROP_COMPILER_CLANG 1
#define SEADROP_COMPILER_NAME "Clang"
#define SEADROP_COMPILER_VERSION __clang_major__
#elif defined(__GNUC__)
#define SEADROP_COMPILER_GCC 1
#define SEADROP_COMPILER_NAME "GCC"
#define SEADROP_COMPILER_VERSION __GNUC__
#else
#define SEADROP_COMPILER_UNKNOWN 1
#define SEADROP_COMPILER_NAME "Unknown"
#define SEADROP_COMPILER_VERSION 0
#endif

// ============================================================================
// Export/Import Macros
// ============================================================================

#ifdef SEADROP_BUILDING_SHARED
#define SEADROP_API __attribute__((visibility("default")))
#else
#define SEADROP_API
#endif

#define SEADROP_LOCAL __attribute__((visibility("hidden")))

// ============================================================================
// Utility Macros
// ============================================================================

#define SEADROP_UNUSED(x) (void)(x)

#define SEADROP_FORCE_INLINE inline __attribute__((always_inline))
#define SEADROP_NO_INLINE __attribute__((noinline))

// Deprecation warnings
#define SEADROP_DEPRECATED(msg) __attribute__((deprecated(msg)))

// Branch prediction hints
#define SEADROP_LIKELY(x) __builtin_expect(!!(x), 1)
#define SEADROP_UNLIKELY(x) __builtin_expect(!!(x), 0)

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

// WiFi Direct support (Linux and Android use wpa_supplicant)
#define SEADROP_HAS_WIFI_DIRECT 1
#define SEADROP_WIFI_DIRECT_WPA_SUPPLICANT 1

// Bluetooth support
#if defined(SEADROP_PLATFORM_ANDROID)
#define SEADROP_HAS_BLUETOOTH 1
#define SEADROP_BLUETOOTH_ANDROID 1
#elif defined(SEADROP_PLATFORM_LINUX)
#ifdef HAS_BLUEZ
#define SEADROP_HAS_BLUETOOTH 1
#define SEADROP_BLUETOOTH_BLUEZ 1
#endif
#endif

#endif // SEADROP_PLATFORM_H

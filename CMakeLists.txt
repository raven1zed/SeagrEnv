# ============================================================================
# SeaDrop - Cross-Platform P2P File Sharing
# ============================================================================
# "SeamlessDrop" - An Open-Source AirDrop for Linux + Android
# https://github.com/seadrop/seadrop
# License: GPL-3.0-or-later
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(SeaDrop
    VERSION 1.0.0
    DESCRIPTION "Cross-platform P2P file sharing via WiFi Direct"
    HOMEPAGE_URL "https://seadrop.org"
    LANGUAGES CXX
)

# ============================================================================
# Build Options
# ============================================================================

option(BUILD_DESKTOP    "Build Qt desktop application"     ON)
option(BUILD_TESTS      "Build unit and integration tests" ON)
option(BUILD_SHARED     "Build shared library"             ON)
option(ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)

# ============================================================================
# C++ Standard and Compiler Settings
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Werror=return-type
    )
    
    if(ENABLE_SANITIZERS)
        add_compile_options(-fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# ============================================================================
# Platform Check (Linux and Android only)
# ============================================================================

if(NOT UNIX OR APPLE)
    message(FATAL_ERROR "SeaDrop only supports Linux and Android platforms.")
endif()

# ============================================================================
# Dependencies
# ============================================================================

# Find required packages
find_package(PkgConfig REQUIRED)

# libsodium for encryption
pkg_check_modules(SODIUM REQUIRED libsodium)

# SQLite for local database
pkg_check_modules(SQLITE REQUIRED sqlite3)

# BlueZ for Bluetooth (Linux only)
pkg_check_modules(BLUEZ bluez)
pkg_check_modules(DBUS dbus-1)

# Threading
find_package(Threads REQUIRED)

# ============================================================================
# Core Library (libseadrop)
# ============================================================================

add_subdirectory(libseadrop)

# ============================================================================
# Desktop Application (Qt)
# ============================================================================

if(BUILD_DESKTOP)
    # Find Qt6
    find_package(Qt6 COMPONENTS Core Widgets Network QUIET)
    
    if(Qt6_FOUND)
        message(STATUS "Qt6 found, building desktop application")
        add_subdirectory(desktop)
    else()
        message(WARNING "Qt6 not found, skipping desktop application")
    endif()
endif()

# ============================================================================
# Tests
# ============================================================================

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)

# Install headers
install(DIRECTORY libseadrop/include/seadrop
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# ============================================================================
# Package Configuration
# ============================================================================

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SeaDropConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SeaDropConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SeaDrop
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/SeaDropConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "============================================================")
message(STATUS "SeaDrop v${PROJECT_VERSION} Configuration Summary")
message(STATUS "============================================================")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Components:")
message(STATUS "  libseadrop:    ON")
message(STATUS "  Desktop app:   ${BUILD_DESKTOP}")
message(STATUS "  Tests:         ${BUILD_TESTS}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  libsodium:     ${SODIUM_VERSION}")
message(STATUS "  SQLite:        ${SQLITE_VERSION}")
if(BLUEZ_FOUND)
    message(STATUS "  BlueZ:         ${BLUEZ_VERSION}")
endif()
if(Qt6_FOUND)
    message(STATUS "  Qt6:           ${Qt6_VERSION}")
endif()
message(STATUS "============================================================")
message(STATUS "")

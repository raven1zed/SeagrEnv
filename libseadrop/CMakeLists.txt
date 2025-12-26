# ============================================================================
# libseadrop - Core Library
# ============================================================================
# SeaDrop only supports Linux and Android platforms.

# Core source files
set(SEADROP_SOURCES
    src/seadrop.cpp
    src/error.cpp
    src/types.cpp
    src/device.cpp
    src/discovery.cpp
    src/connection.cpp
    src/transfer.cpp
    src/clipboard.cpp
    src/security.cpp
    src/distance.cpp
    src/config.cpp
    src/database.cpp
    src/protocol.cpp
    src/state_machine.cpp
)

# Header files (for IDE visibility)
set(SEADROP_HEADERS
    include/seadrop/seadrop.h
    include/seadrop/platform.h
    include/seadrop/types.h
    include/seadrop/error.h
    include/seadrop/device.h
    include/seadrop/discovery.h
    include/seadrop/connection.h
    include/seadrop/transfer.h
    include/seadrop/clipboard.h
    include/seadrop/security.h
    include/seadrop/distance.h
    include/seadrop/config.h
    include/seadrop/database.h
    include/seadrop/protocol.h
    include/seadrop/state_machine.h
)

# Platform-specific sources (Linux/Android)
if(UNIX)
    list(APPEND SEADROP_SOURCES
        src/platform/linux/bluez_ble.cpp
        src/platform/linux/wpa_supplicant.cpp
        src/platform/linux/wifi_direct_linux.cpp
        src/platform/linux/clipboard_linux.cpp
        src/platform/linux/clipboard_platform_linux.cpp
    )
    
    list(APPEND SEADROP_HEADERS
        src/platform/linux/dbus_helpers.h
        src/platform/linux/bluez_ble.h
        src/platform/linux/wpa_supplicant.h
        src/platform/linux/clipboard_linux.h
        src/connection_pimpl.h
        src/clipboard_pimpl.h
    )
endif()

# Create library
if(BUILD_SHARED)
    add_library(seadrop SHARED ${SEADROP_SOURCES} ${SEADROP_HEADERS})
    target_compile_definitions(seadrop PRIVATE SEADROP_BUILDING_SHARED)
else()
    add_library(seadrop STATIC ${SEADROP_SOURCES} ${SEADROP_HEADERS})
endif()

# Alias for modern CMake usage
add_library(SeaDrop::seadrop ALIAS seadrop)

# C++17 requirement
target_compile_features(seadrop PUBLIC cxx_std_17)

# Include directories
target_include_directories(seadrop
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(seadrop
    PUBLIC
        Threads::Threads
    PRIVATE
        ${SODIUM_LIBRARIES}
        ${SQLITE_LIBRARIES}
)

target_include_directories(seadrop PRIVATE
    ${SODIUM_INCLUDE_DIRS}
    ${SQLITE_INCLUDE_DIRS}
)

# Linux/Android-specific dependencies
if(UNIX)
    # D-Bus is required for BlueZ and wpa_supplicant
    if(DBUS_FOUND)
        target_link_libraries(seadrop PRIVATE ${DBUS_LIBRARIES})
        target_include_directories(seadrop PRIVATE ${DBUS_INCLUDE_DIRS})
        target_compile_definitions(seadrop PRIVATE HAS_DBUS)
    else()
        message(WARNING "D-Bus not found. BLE discovery and WiFi Direct will not work.")
    endif()
    
    # BlueZ is required for BLE
    if(BLUEZ_FOUND)
        target_link_libraries(seadrop PRIVATE ${BLUEZ_LIBRARIES})
        target_include_directories(seadrop PRIVATE ${BLUEZ_INCLUDE_DIRS})
        target_compile_definitions(seadrop PRIVATE HAS_BLUEZ)
    else()
        message(WARNING "BlueZ not found. BLE discovery will not work.")
    endif()
endif()

# Set library properties
set_target_properties(seadrop PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME seadrop
    POSITION_INDEPENDENT_CODE ON
)

# Export target
install(TARGETS seadrop
    EXPORT SeaDropTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(DIRECTORY include/seadrop/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/seadrop
    FILES_MATCHING PATTERN "*.h"
)

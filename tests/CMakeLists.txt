# ============================================================================
# SeaDrop Tests
# ============================================================================

enable_testing()

# Find or fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Common test utilities
add_library(test_utils INTERFACE)
target_include_directories(test_utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# ============================================================================
# Unit Tests
# ============================================================================

# Security tests (encryption, hashing, etc.)
add_executable(test_security
    unit/test_security.cpp
)
target_link_libraries(test_security PRIVATE
    seadrop
    GTest::gtest_main
    test_utils
)
add_test(NAME SecurityTests COMMAND test_security)

# Distance/Zone tests
add_executable(test_distance
    unit/test_distance.cpp
)
target_link_libraries(test_distance PRIVATE
    seadrop
    GTest::gtest_main
    test_utils
)
add_test(NAME DistanceTests COMMAND test_distance)

# Transfer protocol tests
add_executable(test_transfer
    unit/test_transfer.cpp
)
target_link_libraries(test_transfer PRIVATE
    seadrop
    GTest::gtest_main
    test_utils
)
add_test(NAME TransferTests COMMAND test_transfer)

# Clipboard tests
add_executable(test_clipboard
    unit/test_clipboard.cpp
)
target_link_libraries(test_clipboard PRIVATE
    seadrop
    GTest::gtest_main
    test_utils
)
add_test(NAME ClipboardTests COMMAND test_clipboard)

# Device/Trust tests
add_executable(test_device
    unit/test_device.cpp
)
target_link_libraries(test_device PRIVATE
    seadrop
    GTest::gtest_main
    test_utils
)
add_test(NAME DeviceTests COMMAND test_device)

# ============================================================================
# Integration Tests
# ============================================================================

add_executable(test_integration
    integration/test_full_transfer.cpp
)
target_link_libraries(test_integration PRIVATE
    seadrop
    GTest::gtest_main
    test_utils
)
add_test(NAME IntegrationTests COMMAND test_integration)

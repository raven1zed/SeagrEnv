# ============================================================================
# SeaDrop Qt GUI Application
# ============================================================================
# 
# This is the Qt-based graphical interface for SeaDrop.
# It links against libseadrop for all core functionality.
#
# Requirements:
#   - Qt 6.2+ (Core, Widgets, Network)
#   - libseadrop (built from parent directory)
#
# Build:
#   mkdir build && cd build
#   cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt6
#   make
#
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(seadrop-qt VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# C++ Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ============================================================================
# Find Qt6
# ============================================================================
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# ============================================================================
# Find libseadrop
# ============================================================================
# Option 1: If building as part of the main project
if(TARGET SeaDrop::seadrop)
    set(SEADROP_LIB SeaDrop::seadrop)
else()
    # Option 2: Find installed libseadrop
    find_package(SeaDrop QUIET)
    if(SeaDrop_FOUND)
        set(SEADROP_LIB SeaDrop::seadrop)
    else()
        # Option 3: Use parent build directory
        set(SEADROP_LIB seadrop)
        link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../build)
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libseadrop/include)
    endif()
endif()

# ============================================================================
# Sources
# ============================================================================
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/seadropbridge.cpp
    src/devicemodel.cpp
    src/transferdialog.cpp
    src/settingsdialog.cpp
)

set(HEADERS
    src/mainwindow.h
    src/seadropbridge.h
    src/devicemodel.h
    src/transferdialog.h
    src/settingsdialog.h
)

set(UI_FILES
    ui/mainwindow.ui
    ui/transferdialog.ui
    ui/settingsdialog.ui
)

set(RESOURCES
    resources/resources.qrc
)

# ============================================================================
# Executable
# ============================================================================
add_executable(seadrop-qt
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
    ${RESOURCES}
)

target_link_libraries(seadrop-qt PRIVATE
    Qt6::Core
    Qt6::Widgets
    ${SEADROP_LIB}
)

target_include_directories(seadrop-qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}  # For generated UI headers
)

# ============================================================================
# Install
# ============================================================================
install(TARGETS seadrop-qt
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ============================================================================
# Desktop Entry (Linux)
# ============================================================================
if(UNIX AND NOT APPLE)
    install(FILES resources/seadrop.desktop
        DESTINATION share/applications
    )
    install(FILES resources/icons/seadrop.png
        DESTINATION share/icons/hicolor/256x256/apps
    )
endif()

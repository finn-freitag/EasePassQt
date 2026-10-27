# Qt version floor (T0, decided 2026-10-15; details in docs/BUILD.md):
#   nixpkgs nixos-unstable / nixos-26.05: Qt 6.11.2   nixos-25.11: Qt 6.10.2
#   Flatpak org.kde.Platform: branches 6.11 and 6.12 exist
#   Windows CI: aqtinstall (install-qt-action), any version
# 6.12 LTS is not yet in nixpkgs, so the floor is Qt 6.11. Do not use APIs newer than 6.11.
# (nixos-25.11 is not supported; use the flake, which pins nixos-unstable.)
cmake_minimum_required(VERSION 3.21)
project(EasePass VERSION 1.4.0 LANGUAGES CXX)

set(EASEPASS_QT_MIN_VERSION 6.11)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(EASEPASS_BUILD_TESTS "Build the test executables" ON)
option(EASEPASS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(Qt6 ${EASEPASS_QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Widgets Network Svg)
find_package(OpenSSL REQUIRED)

# --- argon2 / zxing-cpp ------------------------------------------------------
# Prefer CMake package configs (vcpkg on Windows, zxing-cpp's own config on Linux),
# fall back to pkg-config (Nix, most Linux distributions).
find_package(unofficial-argon2 CONFIG QUIET)
find_package(ZXing CONFIG QUIET)
if(NOT unofficial-argon2_FOUND OR NOT ZXing_FOUND)
    find_package(PkgConfig REQUIRED)
endif()

add_library(easepass_argon2 INTERFACE)
if(unofficial-argon2_FOUND)
    target_link_libraries(easepass_argon2 INTERFACE unofficial::argon2::libargon2)
else()
    pkg_check_modules(ARGON2 REQUIRED IMPORTED_TARGET libargon2)
    target_link_libraries(easepass_argon2 INTERFACE PkgConfig::ARGON2)
endif()

add_library(easepass_zxing INTERFACE)
if(ZXing_FOUND)
    target_link_libraries(easepass_zxing INTERFACE ZXing::ZXing)
else()
    pkg_check_modules(ZXING REQUIRED IMPORTED_TARGET zxing)
    target_link_libraries(easepass_zxing INTERFACE PkgConfig::ZXING)
endif()

# --- Compiler warnings -------------------------------------------------------
add_library(easepass_warnings INTERFACE)
if(MSVC)
    target_compile_options(easepass_warnings INTERFACE /W4 /permissive- /utf-8)
    if(EASEPASS_WARNINGS_AS_ERRORS)
        target_compile_options(easepass_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(easepass_warnings INTERFACE -Wall -Wextra)
    if(EASEPASS_WARNINGS_AS_ERRORS)
        target_compile_options(easepass_warnings INTERFACE -Werror)
    endif()
endif()

# --- Core library (Qt Core/Gui/Network only; no Widgets/Quick) ----------------
set(CORE_SOURCES
    src/core/CryptoHelper.cpp
    src/core/PasswordItem.cpp
    src/core/DatabaseFile.cpp
    src/core/DatabaseManager.cpp
    src/core/TotpHelper.cpp
    src/core/PasswordGenerator.cpp
    src/core/WebsiteIconManager.cpp
    src/core/QrCodeScanner.cpp
    src/core/INetworkClient.cpp
    src/core/ISettingsStore.cpp
    src/core/AppSettings.cpp
    src/core/SecureBuffer.cpp
    src/core/SecretString.cpp
    src/core/ICoreDumpControl.cpp
    src/core/LocalFileDatabaseSource.cpp
    src/core/DeferredSaveManager.cpp
    src/core/DatabaseService.cpp
    src/core/CommonPasswordSequence.cpp
    src/core/PasswordStrength.cpp
    src/core/SortingHelper.cpp
    src/core/TagSearchManager.cpp
    src/core/SearchPasswordsManager.cpp
    src/core/EmailAddressIndexer.cpp
    src/core/PasswordItemTagHelper.cpp
)

set(CORE_HEADERS
    src/core/CryptoHelper.h
    src/core/PasswordItem.h
    src/core/DatabaseFile.h
    src/core/DatabaseManager.h
    src/core/TotpHelper.h
    src/core/PasswordGenerator.h
    src/core/WebsiteIconManager.h
    src/core/QrCodeScanner.h
    src/core/IClock.h
    src/core/INetworkClient.h
    src/core/ISettingsStore.h
    src/core/AppSettings.h
    src/core/SecureBuffer.h
    src/core/SecretString.h
    src/core/ICoreDumpControl.h
    src/core/IDatabaseSource.h
    src/core/LocalFileDatabaseSource.h
    src/core/DeferredSaveManager.h
    src/core/DatabaseService.h
    src/core/CommonPasswordSequence.h
    src/core/PasswordStrength.h
    src/core/SortingHelper.h
    src/core/TagSearchManager.h
    src/core/SearchPasswordsManager.h
    src/core/EmailAddressIndexer.h
    src/core/PasswordItemTagHelper.h
)

add_library(easepass_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(easepass_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)

target_link_libraries(easepass_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Network
    PRIVATE
        OpenSSL::Crypto
        easepass_argon2
        easepass_zxing
        easepass_warnings
)

# --- Widgets application (legacy UI, replaced by the QML app in T24) ----------
set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/LoginWidget.cpp
    src/ui/PasswordsWidget.cpp
    src/ui/PasswordViewerWidget.cpp
    src/ui/EditItemDialog.cpp
    src/ui/Manage2FADialog.cpp
    src/ui/PasswordGeneratorDialog.cpp
    src/ui/CreateDatabaseDialog.cpp
    src/ui/AboutDialog.cpp
    src/ui/ScreenCaptureDialog.cpp
)

set(UI_HEADERS
    src/ui/MainWindow.h
    src/ui/LoginWidget.h
    src/ui/PasswordsWidget.h
    src/ui/PasswordViewerWidget.h
    src/ui/EditItemDialog.h
    src/ui/Manage2FADialog.h
    src/ui/PasswordGeneratorDialog.h
    src/ui/CreateDatabaseDialog.h
    src/ui/AboutDialog.h
    src/ui/ScreenCaptureDialog.h
)

set(RESOURCES
    resources.qrc
)

add_executable(easepass
    src/main.cpp
    ${UI_SOURCES}
    ${UI_HEADERS}
    ${RESOURCES}
)

if(WIN32)
    set_target_properties(easepass PROPERTIES WIN32_EXECUTABLE ON)
endif()

target_include_directories(easepass PRIVATE
    src
    src/ui
)

target_link_libraries(easepass PRIVATE
    easepass_core
    Qt6::Widgets
    Qt6::Svg
    easepass_warnings
)

# --- Tests -------------------------------------------------------------------
if(EASEPASS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation targets
include(GNUInstallDirs)
install(TARGETS easepass RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(UNIX AND NOT APPLE)
    install(FILES easepass.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/appicon.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME easepass.svg)
    install(FILES assets/appicon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME easepass.png)
endif()

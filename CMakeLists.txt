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

find_package(Qt6 ${EASEPASS_QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Widgets Network Svg PrintSupport
             Qml Quick QuickControls2)
find_package(OpenSSL REQUIRED)

# Linux: xdg-desktop-portal screen capture via QtDBus (D4).
if(UNIX AND NOT APPLE)
    find_package(Qt6 ${EASEPASS_QT_MIN_VERSION} REQUIRED COMPONENTS DBus)
    set(EASEPASS_HAVE_PORTAL ON)
endif()

# Webcam QR scanning (T9) needs Qt Multimedia; optional so minimal builds still work.
option(EASEPASS_WITH_MULTIMEDIA "Build webcam QR scanning (Qt Multimedia)" ON)
if(EASEPASS_WITH_MULTIMEDIA)
    find_package(Qt6 ${EASEPASS_QT_MIN_VERSION} QUIET COMPONENTS Multimedia)
    if(NOT Qt6Multimedia_FOUND)
        message(WARNING "Qt Multimedia not found: webcam scanning disabled")
        set(EASEPASS_WITH_MULTIMEDIA OFF)
    endif()
endif()

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
    src/core/SecondFactor.cpp
    src/core/DatabaseOperationResult.cpp
    src/core/DatabaseOperations.cpp
    src/core/IClipboard.cpp
    src/core/ClipboardManager.cpp
    src/core/InactivityManager.cpp
    src/core/LeakCheckService.cpp
    src/core/ScreenGeometry.cpp
    src/core/IScreenCapture.cpp
    src/core/PortalScreenCapture.cpp
    src/core/ScreenQrScanner.cpp
    src/core/IWebcamScanner.cpp
    src/core/PrintDocumentBuilder.cpp
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
    src/core/SecondFactor.h
    src/core/DatabaseOperationResult.h
    src/core/DatabaseOperations.h
    src/core/IClipboard.h
    src/core/ClipboardManager.h
    src/core/InactivityManager.h
    src/core/LeakCheckService.h
    src/core/ScreenGeometry.h
    src/core/IScreenCapture.h
    src/core/PortalScreenCapture.h
    src/core/ScreenQrScanner.h
    src/core/IWebcamScanner.h
    src/core/PrintDocumentBuilder.h
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

if(EASEPASS_HAVE_PORTAL)
    target_link_libraries(easepass_core PRIVATE Qt6::DBus)
    target_compile_definitions(easepass_core PUBLIC EASEPASS_HAVE_PORTAL)
endif()

# --- App services (QML-facing later; Widgets dialogs for now) -----------------
add_library(easepass_app STATIC
    src/app/PrintService.cpp
    src/app/PrintService.h
    src/app/AppController.cpp
    src/app/AppController.h
    src/app/DatabaseController.cpp
    src/app/DatabaseController.h
    src/app/PasswordListModel.cpp
    src/app/PasswordListModel.h
    src/app/PasswordFilterModel.cpp
    src/app/PasswordFilterModel.h
    src/app/TotpTimerService.cpp
    src/app/TotpTimerService.h
    src/app/ItemViewModel.cpp
    src/app/ItemViewModel.h
    src/app/SettingsViewModel.cpp
    src/app/SettingsViewModel.h
    src/app/DialogService.cpp
    src/app/DialogService.h
    src/app/ClipboardService.cpp
    src/app/ClipboardService.h
    src/app/PrintViewModel.cpp
    src/app/PrintViewModel.h
    src/app/ScreenScanViewModel.cpp
    src/app/ScreenScanViewModel.h
    src/app/AppContext.cpp
    src/app/AppContext.h
)
target_include_directories(easepass_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/app)
target_link_libraries(easepass_app
    PUBLIC easepass_core Qt6::Widgets Qt6::PrintSupport Qt6::Qml
    PRIVATE easepass_warnings
)

# --- Multimedia (webcam) -----------------------------------------------------
if(EASEPASS_WITH_MULTIMEDIA)
    add_library(easepass_multimedia STATIC
        src/multimedia/QtWebcamScanner.cpp
        src/multimedia/QtWebcamScanner.h
    )
    target_include_directories(easepass_multimedia PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/multimedia)
    target_link_libraries(easepass_multimedia
        PUBLIC easepass_core Qt6::Multimedia
        PRIVATE easepass_warnings
    )
    target_compile_definitions(easepass_multimedia PUBLIC EASEPASS_HAVE_MULTIMEDIA)
endif()

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
    easepass_app
    Qt6::Widgets
    Qt6::Svg
    easepass_warnings
)
if(EASEPASS_WITH_MULTIMEDIA)
    target_sources(easepass PRIVATE src/ui/WebcamScanDialog.cpp src/ui/WebcamScanDialog.h)
    target_link_libraries(easepass PRIVATE easepass_multimedia)
endif()

# --- QML application (T11, D14: second target; becomes `easepass` in T24) ------
qt_policy(SET QTP0001 NEW)   # QML modules under qrc:/qt/qml/
add_subdirectory(qml/EasePass)

add_executable(easepass_qml
    src/qmlapp/main.cpp
    src/qmlapp/QuickStyle.cpp
    src/qmlapp/QuickStyle.h
    ${RESOURCES}
)
target_include_directories(easepass_qml PRIVATE src/qmlapp)
if(WIN32)
    set_target_properties(easepass_qml PROPERTIES WIN32_EXECUTABLE ON)
endif()
target_link_libraries(easepass_qml PRIVATE
    easepass_core
    easepass_app
    easepass_qml_module
    easepass_qml_moduleplugin
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickControls2
    easepass_warnings
)

# --- Tests -------------------------------------------------------------------
if(EASEPASS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation targets
include(GNUInstallDirs)
install(TARGETS easepass easepass_qml RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(UNIX AND NOT APPLE)
    install(FILES easepass.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/appicon.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME easepass.svg)
    install(FILES assets/appicon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME easepass.png)
endif()

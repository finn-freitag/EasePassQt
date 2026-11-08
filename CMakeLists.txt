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

# --- Windows: no min/max macros from <windows.h> (they break std::min/std::max) ---
if(WIN32)
    add_compile_definitions(NOMINMAX)
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
    src/core/TotpHelper.cpp
    src/core/PasswordGenerator.cpp
    src/core/WebsiteIconManager.cpp
    src/core/QrCodeScanner.cpp
    src/core/QrCodeGenerator.cpp
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
    src/core/TotpHelper.h
    src/core/PasswordGenerator.h
    src/core/WebsiteIconManager.h
    src/core/QrCodeScanner.h
    src/core/QrCodeGenerator.h
    src/core/BuildInfo.cpp
    src/core/BuildInfo.h
    src/core/FileLogger.cpp
    src/core/FileLogger.h
    src/core/FilePermissions.h
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

# App version for BuildInfo / About page / QCoreApplication::applicationVersion (T18)
target_compile_definitions(easepass_core PUBLIC EASEPASS_VERSION="${PROJECT_VERSION}")

if(EASEPASS_HAVE_PORTAL)
    target_link_libraries(easepass_core PRIVATE Qt6::DBus)
    target_compile_definitions(easepass_core PUBLIC EASEPASS_HAVE_PORTAL)
endif()

# --- App services (view models and services exposed to QML; T12) -------------
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
    src/app/DailyTipService.cpp
    src/app/DailyTipService.h
    src/app/PasswordActions.cpp
    src/app/PasswordActions.h
    src/app/WebsiteIconService.cpp
    src/app/WebsiteIconService.h
    src/app/PasswordGeneratorViewModel.cpp
    src/app/PasswordGeneratorViewModel.h
    src/app/QrImportService.cpp
    src/app/QrImportService.h
    src/app/WebcamScanViewModel.cpp
    src/app/WebcamScanViewModel.h
    src/app/DatabaseManagementViewModel.cpp
    src/app/DatabaseManagementViewModel.h
    src/app/AboutViewModel.cpp
    src/app/AboutViewModel.h
    src/app/AutoLogoutService.cpp
    src/app/AutoLogoutService.h
    src/app/ActivityEventFilter.cpp
    src/app/ActivityEventFilter.h
    src/app/WindowStateController.cpp
    src/app/WindowStateController.h
    src/app/SingleInstance.cpp
    src/app/SingleInstance.h
    src/app/CommandLine.cpp
    src/app/CommandLine.h
    src/app/LanguageManager.cpp
    src/app/LanguageManager.h
    src/app/MainWindowLoader.cpp
    src/app/MainWindowLoader.h
)
# Changelog for the About page (C#: Assets/changelog.txt; T18)
qt_add_resources(easepass_app "changelog"
    PREFIX "/changelog"
    BASE "resources/changelog"
    FILES resources/changelog/changelog.txt
)
qt_add_resources(easepass_app "abouticon"
    PREFIX "/about"
    BASE "assets"
    FILES assets/appicon.svg
)
# Daily tips (C#: resources/DailyTips_*.txt, bundled instead of downloaded; T13)
qt_add_resources(easepass_app "dailytips"
    PREFIX "/dailytips"
    BASE "resources/dailytips"
    FILES resources/dailytips/DailyTips_en-US.txt resources/dailytips/DailyTips_de-DE.txt
)
target_include_directories(easepass_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/app)
target_link_libraries(easepass_app
    PUBLIC easepass_core Qt6::Widgets Qt6::PrintSupport Qt6::Qml Qt6::Quick
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

# --- Application (Qt Quick; T11, the Widgets UI was removed in T24) ------------
qt_policy(SET QTP0001 NEW)   # QML modules under qrc:/qt/qml/
add_subdirectory(qml/EasePass)

add_executable(easepass
    src/main/main.cpp
    src/main/QuickStyle.cpp
    src/main/QuickStyle.h
    resources.qrc
)
target_include_directories(easepass PRIVATE src/main)
if(WIN32)
    set_target_properties(easepass PROPERTIES WIN32_EXECUTABLE ON)
endif()
target_link_libraries(easepass PRIVATE
    easepass_core
    easepass_app
    easepass_qml_module
    easepass_qml_moduleplugin
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
    easepass_warnings
)
if(EASEPASS_WITH_MULTIMEDIA)
    target_link_libraries(easepass PRIVATE easepass_multimedia)
endif()

# --- Translations (T20) --------------------------------------------------------
# i18n/easepass_<lang>.ts are generated by `cmake --build <dir> --target update_translations`
# (lupdate over the C++ and QML sources), filled from the C# .resw files by
# tools/resw2ts.py and compiled to .qm files embedded in easepass_app under :/i18n/.
find_package(Qt6 ${EASEPASS_QT_MIN_VERSION} REQUIRED COMPONENTS LinguistTools)
set(EASEPASS_TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/i18n/easepass_de.ts
    ${CMAKE_CURRENT_SOURCE_DIR}/i18n/easepass_pt_BR.ts
)
# English needs only the plural forms ("%n password(s)").
set(EASEPASS_PLURALS_TS ${CMAKE_CURRENT_SOURCE_DIR}/i18n/easepass_en.ts)
qt_add_lupdate(
    SOURCE_TARGETS easepass_core easepass_app easepass_qml_module easepass
    TS_FILES ${EASEPASS_TS_FILES}
    PLURALS_TS_FILE ${EASEPASS_PLURALS_TS}
    OPTIONS -no-obsolete -locations relative
)
qt_add_lrelease(
    TS_FILES ${EASEPASS_TS_FILES} ${EASEPASS_PLURALS_TS}
    QM_FILES_OUTPUT_VARIABLE EASEPASS_QM_FILES
)
qt_add_resources(easepass_app "translations"
    PREFIX "/i18n"
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${EASEPASS_QM_FILES}
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
    install(FILES dist/linux/easepass.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES dist/linux/easepass-mime.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages RENAME easepass.xml)
    install(FILES assets/appicon.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME easepass.svg)
    install(FILES assets/appicon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME easepass.png)
endif()

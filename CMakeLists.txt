cmake_minimum_required(VERSION 3.16)
project(EasePass LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Svg Test)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(ARGON2 REQUIRED IMPORTED_TARGET libargon2)
pkg_check_modules(ZXING REQUIRED IMPORTED_TARGET zxing)

set(CORE_SOURCES
    src/core/CryptoHelper.cpp
    src/core/PasswordItem.cpp
    src/core/DatabaseFile.cpp
    src/core/DatabaseManager.cpp
    src/core/TotpHelper.cpp
    src/core/PasswordGenerator.cpp
    src/core/WebsiteIconManager.cpp
    src/core/QrCodeScanner.cpp
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
)

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
    ${CORE_SOURCES}
    ${CORE_HEADERS}
    ${UI_SOURCES}
    ${UI_HEADERS}
    ${RESOURCES}
)

target_include_directories(easepass PRIVATE
    src
    src/core
    src/ui
)

target_link_libraries(easepass PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    OpenSSL::Crypto
    PkgConfig::ARGON2
    PkgConfig::ZXING
)

# Test executable
add_executable(test_easepass
    tests/test_easepass.cpp
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

target_include_directories(test_easepass PRIVATE
    src
    src/core
)

target_link_libraries(test_easepass PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
    OpenSSL::Crypto
    PkgConfig::ARGON2
    PkgConfig::ZXING
)

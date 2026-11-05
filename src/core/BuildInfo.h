#pragma once

#include <QList>
#include <QString>

namespace EasePass::Core {

/**
 * @brief Version of the app and of the libraries it was built with (About page, T18).
 * The app version comes from the CMake project version (EASEPASS_VERSION).
 */
namespace BuildInfo {

struct Dependency {
    QString name;
    QString version; ///< runtime/build version, empty if the library does not report one
    QString license; ///< SPDX identifier
    QString url;
};

/// CMake project version, e.g. "1.4.0".
QString appVersion();
/// Qt, OpenSSL, Argon2, zxing-cpp (Wasmtime joins with the plugin host, T25+).
QList<Dependency> dependencies();

} // namespace BuildInfo

} // namespace EasePass::Core

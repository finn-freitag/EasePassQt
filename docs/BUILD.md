# Building Ease Pass Qt

## Qt version floor

**Minimum: Qt 6.11** (`EASEPASS_QT_MIN_VERSION` in `CMakeLists.txt`). Do not use Qt APIs
newer than 6.11.

Decision basis (T0, 2026-10-15):

| Target | Qt available |
|--------|--------------|
| nixpkgs `nixos-unstable` (pinned in `flake.lock`) | 6.11.2 |
| nixpkgs `nixos-26.05` | 6.11.2 |
| nixpkgs `nixos-25.11` | 6.10.2 (not supported; use the flake) |
| Flatpak `org.kde.Platform` | branches 6.11 and 6.12 |
| Windows CI (`install-qt-action` / aqtinstall) | any; pinned to `6.11.*` |

Qt 6.12 LTS (the long-term target, D1) is not in nixpkgs yet, so the floor is 6.11.
Raise it when every target ships 6.12.

## Layout

| Target | What |
|--------|------|
| `easepass_core` | static library, `src/core`; links only Qt Core/Gui/Network, OpenSSL, argon2, zxing-cpp |
| `easepass` | Widgets application (legacy UI, replaced by the QML app in T24) |
| `easepass_qml_module` (+`plugin`) | static QML module `EasePass` in `qml/EasePass`: `Theme` singleton, control library, icons |
| `easepass_qml` | Qt Quick application (D14); shows the component gallery until the pages exist (T12+) |
| `easepass_app` | static library `src/app`: view models + services (T12), `AppContext` composition root |
| `test_app` | QtTest executable for the view models (fakes only), one CTest test per function |
| `test_qml` | QtQuickTest runner; one CTest test per `tests/qml/tst_*.qml`, QML warnings fail the test |
| `test_core` | QtTest executable for core module, registered with CTest (alias: `test_easepass`) |

Dependencies are found via their CMake package config first (vcpkg on Windows,
`ZXingConfig.cmake` on Linux) and fall back to pkg-config (`libargon2`, `zxing`).

## Linux (Nix)

```bash
nix develop            # or: nix-shell
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug          # all tests
ctest --preset linux-debug-unit     # fast tests only (no full-cost Argon2)
./build/linux-debug/easepass
```

`linux-release` works the same way. Build directories are `build/<preset>`.
`nix build .#easepass` builds the package and runs the `unit` tests.

## Linux (other distributions)

Install Qt >= 6.11 (Core, Gui, Widgets, Network, Svg, Test), OpenSSL, libargon2,
zxing-cpp (>= 2.0), CMake >= 3.25, Ninja and a C++20 compiler, then use the presets above.

## Windows (MSVC + vcpkg)

Requirements: Visual Studio 2022 (C++ workload), Qt 6.11 for MSVC 2022 64-bit, vcpkg.
From a "x64 Native Tools" developer prompt:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.11.2\msvc2022_64"
cmake --preset windows-msvc         # vcpkg installs openssl, argon2, zxing-cpp (vcpkg.json)
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

Qt's `bin` directory must be on `PATH` to run the tests/app (or use `windeployqt`).

## Tests

Rules: `steps.txt` section 3.3. Each QtTest function is registered as its own CTest
test (`<executable>.<function>`) in `tests/CMakeLists.txt` with a label:

- `unit`: fast tests
- `slow`: tests that run Argon2 with production parameters (250 MB x 10 iterations)
- `integration`: reserved

A guard test (`test_core.all_functions_registered`) fails if a test function exists in
the executable but is not registered with CTest. Tests run with `QT_QPA_PLATFORM=offscreen`.
Fixtures live in `tests/fixtures/` (see its README).

## Warnings and formatting

Own targets compile with `-Wall -Wextra` (GCC/Clang) or `/W4` (MSVC).
`-DEASEPASS_WARNINGS_AS_ERRORS=ON` turns them into errors.
`.clang-format` matches the existing style; format only the code you touch:
`clang-format -i <files>`.

## CI

`.github/workflows/ci.yml`: Linux job (Nix dev shell, presets, ctest, `nix build`) and Windows
job (MSVC, vcpkg manifest, `install-qt-action`, presets, ctest).

## QML module (T11)

- Style: `FluentWinUI3` (ships with Qt >= 6.8 on every platform) if found in Qt's QML dirs,
  else `Basic`; `QT_QUICK_CONTROLS_STYLE` overrides it (`src/qmlapp/QuickStyle.cpp`).
- `Theme` (singleton): follows the system light/dark scheme; `Theme.themeMode` forces
  light/dark. All component colours, radii, spacing and font sizes come from it.
- Icons: own monochrome SVGs in `qml/EasePass/icons/` (no Segoe glyphs), tinted at use.
- Components never touch the clipboard; copy controls emit `copyRequested(text)`, which the
  view models (T12) route to `ClipboardManager`.
- On Nix the desktop session can export QML/plugin paths of another Qt version. `test_qml`
  pins `QML_IMPORT_PATH`/`QT_PLUGIN_PATH` to the Qt it was built against; to run
  `easepass_qml` from a dev shell, use the same approach or the wrapped Nix package.

## View models (T12)

`App::AppContext` builds the whole object graph from `AppContext::Deps` (settings store,
clipboard, network client, screen capture, clock, KDF params; defaults are the real
implementations) and `registerQml()` exposes these singletons in the QML module
`EasePass.App` 1.0:

| QML name | C++ class | Purpose |
|----------|-----------|---------|
| `AppController` | `AppController` | pages Login/Register/Passwords/Database/Settings/About/Plugins, back stack |
| `DatabaseController` | `DatabaseController` | open/lock/create/save, busy + error state |
| `PasswordList` | `PasswordFilterModel` over `PasswordListModel` | search (`/tag` = tag search), sort, suggestions |
| `ItemViewModel` | `ItemViewModel` | edit buffer, validation, strength + leak checks, TOTP |
| `SettingsViewModel` | `SettingsViewModel` | `settings` (AppSettings), limits, resets |
| `DialogService` | `DialogService` | C++ -> QML dialog requests (`respond(id, result)`), InfoBar messages |
| `ClipboardService` | `ClipboardService` | copy with auto-clear (ClipboardManager) |
| `PrintViewModel` | `PrintViewModel` | print / preview / PDF export |
| `ScreenScanViewModel` | `ScreenScanViewModel` | QR scan from the screen |
| `TotpTimer` | `TotpTimerService` | the one shared 1 s TOTP timer |
| `DailyTip` | `DailyTipService` | tip of the day for the login page |

Secrets (passwords, 2FA secrets) are never list-model roles; only `ItemViewModel` exposes them.

## Login / Register (T13)

- `easepass_qml` starts on the Register (OOBE) page when no known database file exists,
  else on Login. Register creates `<AppData>/easepass.epdb` and opens it.
- Pages get their C++ objects as properties (`controller`, ...) instead of using the
  singletons directly, so QtQuickTests drive them with `tests/qml/FakeDatabaseController.qml`.
  `Main.qml` receives the real objects as initial properties from `src/qmlapp/main.cpp`.
- Daily tips: `resources/dailytips/DailyTips_<lang>.txt` (copied from the C# repository's
  `resources/`, MIT) are bundled in qrc; the C# app downloads them from GitHub at runtime,
  the Qt app makes no network request for them. To update, copy the files from upstream.
- One-time (OTP) second factor databases get a new token after every login; it is shown once
  in `OneTimeTokenDialog`.

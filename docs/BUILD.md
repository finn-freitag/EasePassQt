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
| `easepass_qml_module` (+`plugin`) | static QML module `EasePass` in `qml/EasePass`: `Theme` singleton, control library, icons |
| `easepass` | the application (Qt Quick, `src/main`); the Widgets UI was removed in T24 |
| `easepass_app` | static library `src/app`: view models + services (T12), `AppContext` composition root, `loadMainWindow` |
| `test_app` | QtTest executable for the view models (fakes only), one CTest test per function |
| `test_qml` | QtQuickTest runner; one CTest test per `tests/qml/tst_*.qml`, QML warnings fail the test |
| `test_core` | QtTest executable for core module, registered with CTest |
| `test_smoke` | application smoke test (T24): real `Main.qml` + `AppContext`, unlocks a copy of `test.epdb` |

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

Install Qt >= 6.11 (Core, Gui, Widgets, Network, Svg, Test, LinguistTools), OpenSSL, libargon2,
zxing-cpp (>= 2.0), CMake >= 3.25, Ninja and a C++20 compiler, then use the presets above.

## Windows (MSVC + vcpkg)

Requirements: Visual Studio 2022 (C++ workload), Qt 6.11 for MSVC 2022 64-bit, vcpkg.
From a "x64 Native Tools" developer prompt:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.11.2\msvc2022_64"
cmake --preset windows-msvc         # vcpkg installs openssl, argon2, nu-book-zxing-cpp (vcpkg.json)
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

Qt's `bin` directory must be on `PATH` to run the tests/app (or use `windeployqt`).

- The vcpkg port of zxing-cpp is `nu-book-zxing-cpp`; it builds only the old writer backend
  (`ZXING_WRITERS=OLD`, no zint). `QrCodeGenerator` therefore picks the writer by
  `ZXING_USE_ZINT` (from `<ZXing/Version.h>`), not by the major version. Nix builds both.
- CI installs Qt with `jurplel/install-qt-action`, using an aqtinstall commit that knows the
  per-compiler repository layout of Qt >= 6.11 (aqtinstall 3.3.0 does not; see `ci.yml`).

## Tests

Rules: `steps.txt` section 3.3. Each QtTest function is registered as its own CTest
test (`<executable>.<function>`) in `tests/CMakeLists.txt` with a label:

- `unit`: fast tests
- `slow`: tests that run Argon2 with production parameters (250 MB x 10 iterations)
- `integration`: reserved
- `smoke`: the application smoke tests (T24): `smoke.unlock_fixture_lists_items` (real main
  window offscreen: type the password of a copy of `tests/fixtures/test.epdb`, click Login, the
  four items are listed, log out; label `slow` too) and `smoke.launch_app` (the real `easepass`
  binary: `--version`, then a 5 s offscreen run with `--db <copy>` and a private XDG profile;
  fails on QML errors/warnings in its output or when no log file is written;
  `tests/smoke/RunApp.cmake`)

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
  else `Basic`; `QT_QUICK_CONTROLS_STYLE` overrides it (`src/main/QuickStyle.cpp`).
- `Theme` (singleton): follows the system light/dark scheme; `Theme.themeMode` forces
  light/dark. All component colours, radii, spacing and font sizes come from it.
- Icons: own monochrome SVGs in `qml/EasePass/icons/` (no Segoe glyphs), tinted at use.
- Components never touch the clipboard; copy controls emit `copyRequested(text)`, which the
  view models (T12) route to `ClipboardManager`.
- On Nix the desktop session can export QML/plugin paths of another Qt version. `test_qml`
  pins `QML_IMPORT_PATH`/`QT_PLUGIN_PATH` to the Qt it was built against; to run
  `easepass` from a dev shell, use the same approach or the wrapped Nix package.

## View models (T12)

`App::AppContext` builds the whole object graph from `AppContext::Deps` (settings store,
clipboard, network client, screen capture, webcam scanner, clock, KDF params; defaults are
the real implementations, the webcam scanner defaults to none) and `registerQml()` exposes these singletons in the QML module
`EasePass.App` 1.0:

| QML name | C++ class | Purpose |
|----------|-----------|---------|
| `AppController` | `AppController` | pages Login/Register/Passwords/Database/Settings/About/Plugins, back stack |
| `DatabaseController` | `DatabaseController` | open/lock/create/save, busy + error state |
| `PasswordList` | `PasswordFilterModel` over `PasswordListModel` | search (`/tag` = tag search), sort, suggestions |
| `ItemViewModel` | `ItemViewModel` | edit buffer, validation, strength + leak checks, TOTP |
| `SettingsViewModel` | `SettingsViewModel` | `settings` (AppSettings), limits, languages, C# text-field rules, resets, reset popularity |
| `DialogService` | `DialogService` | C++ -> QML dialog requests (`respond(id, result)`), InfoBar messages |
| `ClipboardService` | `ClipboardService` | copy with auto-clear (ClipboardManager) |
| `PrintViewModel` | `PrintViewModel` | print / preview / PDF export |
| `ScreenScanViewModel` | `ScreenScanViewModel` | QR scan from the screen |
| `TotpTimer` | `TotpTimerService` | the one shared 1 s TOTP timer |
| `DailyTip` | `DailyTipService` | tip of the day for the login page |
| `PasswordActions` | `PasswordActions` | copy/delete/export by row, click tracking |
| `WebsiteIcons` | `WebsiteIconService` | favicon URLs (`image://favicon/...`) |
| `EditorViewModel` | `ItemViewModel` (second instance) | edit buffer of the add/edit/2FA dialogs |
| `PasswordGenerator` | `PasswordGeneratorViewModel` | generator dialog / generate button (stored settings, leak check) |
| `QrImport` | `QrImportService` | 2FA QR from file / clipboard, validates otpauth URIs |
| `WebcamScan` | `WebcamScanViewModel` | webcam QR scan, preview via `image://webcam/<rev>` |
| `AutoLogout` | `AutoLogoutService` | inactivity logout: warning countdown, preventLogout(), locks when idle |
| `About` | `AboutViewModel` | About page: version, developers, links (safe opener), dependencies, changelog |
| `DatabaseManagement` | `DatabaseManagementViewModel` | Database page: known databases, rename/delete, password, second factor, import into, export, backup |

Secrets (passwords, 2FA secrets) are never list-model roles; only `ItemViewModel` exposes them.

## Login / Register (T13)

- `easepass` starts on the Register (OOBE) page when no known database file exists,
  else on Login. Register creates `<AppData>/easepass.epdb` and opens it.
- Pages get their C++ objects as properties (`controller`, ...) instead of using the
  singletons directly, so QtQuickTests drive them with `tests/qml/FakeDatabaseController.qml`.
  `Main.qml` receives the real objects as initial properties from `App::loadMainWindow` (`src/app/MainWindowLoader.cpp`, shared by
  `src/main/main.cpp` and the smoke test).
- Daily tips: `resources/dailytips/DailyTips_<lang>.txt` (copied from the C# repository's
  `resources/`, MIT) are bundled in qrc; the C# app downloads them from GitHub at runtime,
  the Qt app makes no network request for them. To update, copy the files from upstream.
- One-time (OTP) second factor databases get a new token after every login; it is shown once
  in `OneTimeTokenDialog`.

## Passwords page (T14)

- `PasswordsPage.qml`: list (search, sort menu, context menu) | details (copy controls, TOTP
  bar, strength chart, leak warning), splitter width stored in `AppSettings::splitterWidth`.
  Shortcuts as in C#: Ctrl+F, Ctrl+N, Ctrl+E/F2, Ctrl+Up/Down, Ctrl+L, F1, Esc.
- Page tests (`tests/qml/tst_passwords.qml`) use the real App layer through `testContext`
  (`tests/qml/QmlTestContext.*`: AppContext with fakes, fake clock).
- Theme follows what the Controls style draws (probe control), because some platforms report
  the colour scheme as Unknown while FluentWinUI3 still renders dark.

## Item dialogs (T15)

- `ItemEditDialog.qml` (add + edit, C# AddItemPage), `Add2FADialog.qml`, `TwoFactorFields.qml`,
  `PasswordGeneratorDialog.qml`, `WebcamScanDialog.qml`; opened from `Main.qml` on the
  Passwords page's add/edit/2FA/generate requests.
- The dialogs edit `EditorViewModel`, a second `ItemViewModel`: Cancel discards the buffer and
  the detail pane keeps its item; after a save the detail pane reloads from the database
  (`ItemViewModel` follows `DatabaseService::itemsChanged` while it has no unsaved edits).
- Generator: `PasswordGenerator::generateFromCharset` follows C# `PasswordHelper` (stored
  length + characters, every available character class, no common sequences); with "verify
  not leaked" on, leaked candidates are drawn again (at most 20, C# loops forever).
- Export 2FA renders the otpauth URI as QR code (`Core::QrCodeGenerator`, zxing-cpp writer;
  2.x and 3.x APIs supported) and shows it as a PNG data URL, so no image cache keeps it.
- Webcam scanning needs `EASEPASS_WITH_MULTIMEDIA` (`easepass` then passes a
  `QtWebcamScanner`); without it the webcam button is disabled.
- Tests: `tests/qml/tst_itemdialogs.qml` (fake clipboard, screen capture and webcam through
  `testContext`), `tests/app/ItemDialogsTest.cpp`.
- Headless runs (`QT_QPA_PLATFORM=offscreen`) use Qt's software renderer, which does not clip
  a `Shape` (the strength chart) inside a scrolled `ScrollView`; GPU backends do. For
  screenshots set `QT_QUICK_BACKEND=rhi`.

## Database page (T16)

- `DatabasePage.qml` (C# ManageDatabasePage): cards for every known database with its state
  (loaded, temporary, file not found) and the actions for the selected one. Dialogs:
  `ChangePasswordDialog`, `ManageSecondFactorDialog` (with the enable confirmation),
  `DeleteDatabaseDialog` (the database name must be typed), `RenameDatabaseDialog`,
  `SelectItemsDialog` (export selection; import with Add / Override + confirmation),
  `PasswordPromptDialog`, `PrintDatabaseDialog` (print / preview / PDF, passwords only after the
  plaintext warning), plus the existing `CreateDatabaseDialog` and `ImportDatabaseFlow`.
- Operations that need the decrypted database work on the open database. "Load this database"
  selects the file on the login page and locks the open one (`DatabaseController::switchTo`).
- "Load temp database" (Passwords page and Database page) turns an open temporary database into a
  normal, writable one and adds it to the known list (`DatabaseController::makePermanent`).
- Reachable from the Passwords page OOBE link and from Settings > Database > Manage.
- Tests: `tests/qml/tst_database.qml` works on real `.epdb` files in a temporary directory
  (`testContext.createSampleFile`, cheap KDF); file dialogs are bypassed with
  `DatabasePage.fileOverride`. `tests/app/DatabaseManagementTest.cpp` covers the view model.

## Settings page (T17)

- `SettingsPage.qml` (C# SettingsPage): language (applies immediately, T20),
  database (Manage, auto-save delay), security (inactivity logout 1..1000 min, clipboard clear
  1..100000 s), password generator (characters + reset, length, verify not leaked),
  miscellaneous (quick copy, show icons, download website icons, reset popularity). Plugin
  sections come with T26.
- Controls write AppSettings directly through `SettingsViewModel.settings`; spin boxes re-bind
  after user edits so resets show up. The two generator text fields follow the C# rules
  (`applyPasswordLengthText`, `applyPasswordCharsText`) and are not bound, so clearing a field
  stores the default without refilling the field while typing.
- Tests: `tests/qml/tst_settings.qml`, `AppServicesTest::testSettings*`.

## About page (T18)

- `AboutPage.qml` (C# AboutPage) with the new `Expander.qml`: version, developers, contributor,
  website / GitHub (this port) / original Windows version, changelog, report bug / request
  feature, dependencies with licenses, privacy policy, disclaimer.
- Version: the CMake project version, passed as `EASEPASS_VERSION` (compile definition on
  `easepass_core`, `Core::BuildInfo::appVersion()`); both executables use it for
  `QCoreApplication::applicationVersion`. Bump `project(EasePass VERSION ...)` only.
- Changelog: `resources/changelog/changelog.txt` (copied from the C# `Assets/changelog.txt`, UTF-8
  without BOM), bundled in qrc and parsed by `AboutViewModel::parseChangelog` ('#' headlines).
- Links open through `AboutViewModel::openUrl` (http, https, mailto only; QDesktopServices by
  default). Tests replace the opener (`testContext.openedUrls`), so no browser starts.
- Dependency versions come from the libraries at runtime/build time (`Core::BuildInfo`).

## App behaviour (T19)

- **Inactivity logout**: `AutoLogoutService` enables `Core::InactivityManager` while a database is
  open; `ActivityEventFilter` (installed on the application) reports every spontaneous key,
  mouse, wheel, touch and tablet event. 30 s before the logout `AutoLogoutDialog.qml` shows a
  countdown ("Stay logged in", Escape or any input keep the session; "Log out now" locks).
  `ManageSecondFactorDialog` suspends the logout while it is open (C#: PreventAutologout).
  Locking closes the item dialogs.
- **Window geometry/state**: `WindowStateController` restores size, position and maximized state
  from AppSettings (C# state values: 0 maximized, 1 minimized, 2 restored), keeps only the
  restored geometry (like C#), moves windows that are off every screen back onto the primary
  screen, and saves on close/quit. `Main.qml` starts hidden; `main.cpp` shows it afterwards.
- **Single instance**: `SingleInstance` (QLockFile in the runtime dir + QLocalServer, per user and
  settings profile). A second `easepass` sends its arguments to the first one, which raises
  its window, and exits.
- **Command line**: `easepass [--db <path>] [<path>]`, `--help`, `--version` (both handled before
  the GUI starts, so they work without a display). A known database
  is preselected on the login page; another file is offered as temporary database
  ("(Temp)", read-only until "Load temp database"; C# file association behaviour).
- **Log file**: `Core::FileLogger` writes all Qt messages to
  `<AppData>/logs/easepass_<date>_<time>.log` (owner-only permissions) with a system header,
  continues in `..._2.log` after 1 MiB, deletes files older than 28 days and keeps the newest 10.
  Messages pass `FileLogger::redact()` (otpauth URIs, `password=`/`secret=`/`token=` values) as a
  safeguard; code must still never log secrets.
- **Desktop integration**: `easepass.desktop` (`%f`, MIME type), `dist/linux/easepass.metainfo.xml`
  (AppStream), `dist/linux/easepass-mime.xml` (`application/x-easepass-database`, `*.epdb`),
  installed to `share/{applications,metainfo,mime/packages}`. The AppStream id becomes reverse-DNS
  with the Flatpak work (T30).

## Translations (T20)

- Languages: English (source), German, Portuguese (Brazil). Files: `i18n/easepass_de.ts`,
  `i18n/easepass_pt_BR.ts`, and `i18n/easepass_en.ts` (plural forms only). They are compiled to
  `.qm` at build time and embedded in `easepass_app` under `:/i18n/`.
- `App::LanguageManager` loads the file for `AppSettings::language`, sets the default `QLocale`
  and retranslates the QML engine: switching the language in the settings applies immediately.
  Missing translations show the English text; unknown language tags use English.
- Workflow after changing UI texts:
  ```bash
  cmake --build build/<preset> --target update_translations   # lupdate, keeps translations
  tools/resw2ts.py --resw-dir <C# repo>/EasePass/Strings i18n/easepass_de.ts i18n/easepass_pt_BR.ts
  ```
  then translate the remaining entries (Qt Linguist or by editing the `.ts`).
  `tools/resw2ts.py` copies the de-DE/pt-BR text of the C# `.resw` entry whose en-US value equals
  our source text (ignoring case and trailing punctuation) and marks it `resw: <Key>` (translator
  comment), so upstream changes can be pulled in again; manual translations are never touched and
  running it twice changes nothing.
- CTest `i18n.translations_up_to_date` runs lupdate on copies and fails when a `qsTr`/`tr` text is
  missing from the `.ts` files, a stale one is left, or a German/Portuguese entry is unfinished.
  `i18n.resw2ts` unit-tests the conversion script. Both need Python 3.
- C++ strings outside `Q_OBJECT` classes must use `QCoreApplication::translate("Context", ...)`
  literally (a custom `tr()` helper hides them from lupdate).
- Daily tips: `resources/dailytips/DailyTips_<lang>.txt` (en-US, de-DE; other languages use English).

## Security review (T23)

- Findings, fixes and the remaining limits are in [SECURITY.md](SECURITY.md): secret lifetimes,
  clipboard per platform, crash dumps, mlock/swap, logs, files on disk, network privacy.
- New files that hold database contents (databases, `.bak`, exports, PDF exports) and log files
  are created owner-only (`src/core/FilePermissions.h`); existing files keep their permissions.
- Locking clears a copied secret from the clipboard and the generator's last password.
- `tests/app/SecurityTest.cpp`: one regression test per finding (Unix permission checks are
  skipped on Windows).

## Switch-over (T24)

- `easepass_qml` became `easepass`; its sources moved from `src/qmlapp` to `src/main`.
- Removed: the Widgets UI (`src/ui`, old `src/main.cpp`), the `DatabaseManager` singleton (its
  only users were the Widgets UI; `DatabaseService` + `AppSettings` replace it) with its tests,
  `WebsiteIconManager::instance()`, and the `test_easepass` alias target.
- Qt Widgets is still linked: `QApplication` is needed for `QPrintDialog` (D7).
- `--help`/`--version` no longer create the `QApplication` (they aborted without a display,
  found by the Nix sandbox build).

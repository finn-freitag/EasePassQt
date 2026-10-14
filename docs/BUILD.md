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
| `test_easepass` | QtTest executable, registered with CTest |

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

A guard test (`test_easepass.all_functions_registered`) fails if a test function exists in
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

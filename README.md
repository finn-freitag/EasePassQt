<p align="center">
  <img src="./assets/appicon.svg" height="150px" width="auto" alt="alternate text">
</p>
<h1 align="center">Ease Pass (Qt 6 Edition for Linux)</h1>

A native C++ / Qt 6 rewrite of [**Ease Pass**](https://github.com/FrozenAssassine/EasePass/) for Linux (NixOS, KDE Plasma 6, GNOME, etc.), maintaining **100% byte-level file format compatibility** with the latest `.epdb` format from the original C# Windows version.

---

## ✨ Features

- **Exact File Format Compatibility**:
  - Full support for the latest `.epdb` file format (EpdbV2DbVersion / Version 1.4).
  - Uses **Argon2id** key derivation (250 MB memory, 10 iterations, 10 lanes) and **AES-256-CBC** with PKCS7 padding.
  - Seamlessly opens database files created with the original Windows Ease Pass and saves databases readable by both versions.
- **Password Management**:
  - List entries with website favicons or auto-generated high-contrast circular letter avatars.
  - View, Add, Edit, and Delete password entries.
  - Real-time search across Name, Username, Email, Website, Notes, and Tags.
  - Sorting by Name (A-Z, Z-A), Username, Website, and Usage frequency.
- **Two-Factor Authentication (2FA / TOTP)**:
  - Supports RFC 6238 TOTP tokens (SHA1, SHA256, SHA512, custom digits and intervals).
  - **Countdown Validity Progress Bar**: Live visual bar and second countdown showing how long the current code remains valid (a requested feature missing in the original app).
  - **QR Code Scanning**:
    - Scan QR code from an image file.
    - Scan QR code from system clipboard (image or `otpauth://` URI).
    - Interactive screen capture to scan QR codes directly from any window on screen.
- **Password Generator**:
  - Cryptographically secure generator with configurable length (4 to 64 chars).
  - Custom rules for uppercase, lowercase, numbers, and special symbols.
- **Website Icons**:
  - Automatically fetches domain favicons asynchronously with local caching in `~/.cache/EasePass/icons`.
  - Fallback to Google Favicon service and color-hashed initials avatars.
- **Desktop Integration**:
  - Fully native Qt 6 application adhering to KDE Plasma 6 themes, light/dark palettes, and system shortcuts (Ctrl+N, Ctrl+F, Ctrl+E, Ctrl+L, F1).

---

## 🛠️ Building & Running

### Using NixOS / `nix-shell` (Recommended)

A `shell.nix` is included with all required dependencies (`qt6`, `openssl`, `libargon2`, `zxing-cpp`, `cmake`, `gcc`, `pkg-config`).

1. Enter the nix shell and build:
   ```bash
   nix-shell --run "mkdir -p build && cd build && cmake .. && make -j\$(nproc)"
   ```

2. Run Ease Pass:
   ```bash
   nix-shell --run "./build/easepass"
   ```

3. Run the automated test suite:
   ```bash
   nix-shell --run "./build/test_easepass"
   ```

### Building on Other Linux Distributions

Ensure you have the following installed:
- Qt 6 (Core, Gui, Widgets, Network, Svg, Test)
- OpenSSL (libcrypto)
- `libargon2`
- `zxing-cpp` (>= 2.0)
- CMake (>= 3.16) and a C++20 compiler

Then build:
```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
./easepass
```

{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    cmake
    ninja
    pkg-config
    qt6.wrapQtAppsHook
    clang-tools # clang-format
  ];

  buildInputs = with pkgs; [
    gcc
    qt6.qtbase
    qt6.qtsvg
    qt6.qtmultimedia
    qt6.qtdeclarative
    qt6.qttools # Linguist tools (lupdate/lrelease, T20)
    openssl
    libargon2
    zxing-cpp
    wasmtime # C API for the plugin host (T26)
    zlib # .epplugin packages are ZIP files (T27)
  ];
}

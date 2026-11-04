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
    openssl
    libargon2
    zxing-cpp
  ];
}

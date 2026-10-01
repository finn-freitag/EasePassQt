{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    cmake
    pkg-config
    qt6.wrapQtAppsHook
  ];

  buildInputs = with pkgs; [
    gcc
    qt6.qtbase
    qt6.qtsvg
    openssl
    libargon2
    zxing-cpp
  ];
}

{
  description = "Ease Pass - Secure password manager rewritten in Qt 6 for Linux";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f (import nixpkgs { inherit system; }));
    in {
      packages = forAllSystems (pkgs: rec {
        default = easepass;
        easepass = pkgs.stdenv.mkDerivation {
          pname = "easepass";
          version = "1.4.0";

          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
            qt6.wrapQtAppsHook
          ];

          buildInputs = with pkgs; [
            qt6.qtbase
            qt6.qtsvg
            openssl
            libargon2
            zxing-cpp
          ];

          meta = with pkgs.lib; {
            description = "Secure password manager rewritten in Qt 6 for Linux";
            homepage = "https://github.com/finn-freitag/EasePassQt";
            license = licenses.mit;
            platforms = platforms.linux;
            mainProgram = "easepass";
          };
        };
      });

      apps = forAllSystems (pkgs: {
        default = {
          type = "app";
          program = "${self.packages.${pkgs.system}.default}/bin/easepass";
        };
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
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
        };
      });

      overlays.default = final: prev: {
        easepass = self.packages.${prev.system}.default;
      };

      nixosModules.default = { config, lib, pkgs, ... }:
        let
          cfg = config.programs.easepass;
        in {
          options.programs.easepass = {
            enable = lib.mkEnableOption "EasePass password manager";
          };

          config = lib.mkIf cfg.enable {
            environment.systemPackages = [
              self.packages.${pkgs.system}.default
            ];
          };
        };
    };
}

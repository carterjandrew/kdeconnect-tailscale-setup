{
  description = "KDE Connect Tailscale setup helper";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        nativeBuildInputs = with pkgs; [
          cmake
          ninja
          pkg-config
          qt6.wrapQtAppsHook
        ];
        buildInputs = with pkgs; [
          qt6.qtbase
        ];
      in {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "kdeconnect-tailscale";
          version = "0.1.0";
          src = pkgs.lib.cleanSourceWith {
            src = self;
            filter = path: type:
              let name = baseNameOf path;
              in !(type == "directory" && builtins.elem name [ "build" ".git" ])
                 && name != "result";
          };
          inherit nativeBuildInputs buildInputs;
          cmakeFlags = [ "-DBUILD_TESTING=ON" ];
          doCheck = true;
          checkPhase = "ctest --output-on-failure";
        };

        apps.default = flake-utils.lib.mkApp {
          drv = self.packages.${system}.default;
        } // {
          meta.description = "Configure KDE Connect discovery over Tailscale";
        };

        devShells.default = pkgs.mkShell {
          inherit nativeBuildInputs buildInputs;
        };

        formatter = pkgs.nixfmt;
      });
}

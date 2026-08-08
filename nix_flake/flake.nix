{
  description = "Dev shell for PurdueElectricRacing/daqapp2";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
    rust-overlay.url = "github:oxalica/rust-overlay";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, rust-overlay, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        overlays = [ (import rust-overlay) ];
        pkgs = import nixpkgs {
          inherit system overlays;
        };
      in {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            pkg-config
            systemd
            udev
            wayland
            wayland-protocols
            libxkbcommon
            mesa
            libGL
            dbus

            (rust-bin.stable.latest.default.override {
              extensions = [ "rust-src" "clippy" "rustfmt" "rust-analyzer" ];
            })
          ];

          shellHook = ''
            export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath [
              pkgs.wayland
              pkgs.libxkbcommon
              pkgs.mesa
              pkgs.libGL
              pkgs.dbus
            ]}:$LD_LIBRARY_PATH
          '';
        };
      });
}
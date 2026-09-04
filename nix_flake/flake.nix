{
  description = "Dev shell for PurdueElectricRacing/monorepo";

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

        python = pkgs.python312.withPackages (ps: with ps; [
          cantools
          jinja2
          pydantic
        ]);
      in {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            gnumake
            ninja

            gcc
            clang
            libclang
            clang-tools
            lcov
            gtest
            cppcheck

            gcc-arm-embedded
            openocd
            stlink-tool

            (rust-bin.stable.latest.default.override {
              extensions = [
                "rust-src"
                "clippy"
                "rustfmt"
                "rust-analyzer"
              ];
            })

            python
            mypy

            pkg-config
            systemd
            udev
            wayland
            wayland-protocols
            libxkbcommon
            mesa
            libGL
            dbus

            direnv
            nix-direnv
          ];

          shellHook = ''
            export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath [
              pkgs.wayland
              pkgs.libxkbcommon
              pkgs.mesa
              pkgs.libGL
              pkgs.dbus
            ]}:$LD_LIBRARY_PATH

            export CC="${pkgs.gcc}/bin/gcc"
            export CXX="${pkgs.gcc}/bin/g++"

            echo "Purdue Electric Racing development environment"
            echo "  CMake:      $(cmake --version | head -n1)"
            echo "  Ninja:      $(ninja --version)"
            echo "  GCC:        $($CC --version | head -n1)"
            echo "  G++:        $($CXX --version | head -n1)"
            echo "  ARM GCC:    $(arm-none-eabi-gcc --version | head -n1)"
            echo "  OpenOCD:    $(openocd --version 2>&1 | head -n1)"
            echo "  Python:     $(python --version)"
            echo "  Rust:       $(rustc --version)"
          '';
        };
      });
}

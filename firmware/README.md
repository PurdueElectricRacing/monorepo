# PER Firmware

This directory contains the embedded software for PER vehicle controllers

## Directory layout

| Path | Purpose |
| --- | --- |
| `source/` | One source directory per firmware target. See its [target guide](source/README.md). |
| `common/` | Reusable vehicle firmware components, HAL wrappers, and utilities. |
| `can_library/` | CAN configuration, code generation, and shared CAN definitions. |
| `cmake/` | Toolchain, dependency discovery, and common CMake helpers. |
| `external/` | Third-party libraries and STM32 vendor dependencies. |
| `support/` | Linker scripts, SVD files, and OpenOCD configuration. |
| `firmware_build.py` | Firmware-specific CMake/Ninja build and packaging helper. |

## Targets

Vehicle targets include `main_module`, `dashboard`, `torque_vector`, `pdu`,
`a_box`, `daq`, and `driveline`. `driveline` produces separate front and rear
binaries. `f4_testing` and `g4_testing` are bench-testing targets.

## Building

Install the ARM toolchain, CMake, Ninja, and Python dependencies using the
repository [setup guide](../docs/setup.md).

From the repository root:

```bash
python3 per_build.py firmware
```

The firmware helper creates these local build products:

- `build/` — CMake/Ninja build tree and `compile_commands.json`.
- `output/` — target `.elf` and `.hex` files.
- `output/firmware_<git-ref>.tar.gz` — packaged firmware, when `--package` is
  requested.
- `can_library/generated/` and `can_library/dbc/` - generated CAN artifacts
   - Header files for CAN message packing, unpacking, stale, and more
   - DBC files for use in the DAQ app and other tools

> [!WARNING]
> Every build preforms a clean build!

## Static Analysis

Run cppcheck over firmware (`source/` and `common/`):

```bash
python3 per_build.py firmware --check
```

This configures the build to generate `compile_commands.json`, then runs cppcheck.
It runs automatically in CI on every push(after the firmware build passes) and fails on any finding. 
Requires `cppcheck` to be installed (see the [setup guide](../docs/setup.md)).

## Debugging and editor support

Open the repository root in VS Code. The supplied tasks build firmware through
the root build script, and `.vscode/launch.json` points Cortex-Debug at the
target output files.

Clangd reads `build/compile_commands.json` through the repository's `.clangd` configuration.
Run a firmware build to update the compile commands before using code navigation or diagnostics.

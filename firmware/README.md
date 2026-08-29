# PER Firmware

This directory contains the embedded software for PER vehicle controllers.

## Directory layout

| Path | Purpose |
| --- | --- |
| `source/` | One source directory per firmware target. See its [target guide](source/README.md). |
| `common/` | Reusable vehicle firmware components, HAL wrappers, and utilities. |
| `can_library/` | CAN configuration, code generation, and shared CAN definitions. |
| `cmake/` | Toolchain, dependency discovery, and common CMake helpers. |
| `external/` | Third-party libraries and STM32 vendor dependencies. |
| `support/` | Linker scripts, SVD files, and OpenOCD configuration. |
| `build_firmware.py` | Fixed workflow that builds and packages all firmware. |
| `check_firmware.py` | Fixed workflow that runs static analysis on all firmware. |

## Targets

Vehicle targets include `main_module`, `dashboard`, `torque_vector`, `pdu`,
`a_box`, `daq`, and `driveline`. `driveline` produces separate front and rear
binaries. `f4_testing` and `g4_testing` are bench-testing targets.

## Building

Install the ARM toolchain, CMake, Ninja, and Python dependencies using the
repository [setup guide](../docs/setup.md).

From the repository root:

```bash
python3 firmware/build_firmware.py
```

The firmware helper creates these local build products:

- `build/` — CMake/Ninja build tree and `compile_commands.json`.
- `output/` — target `.elf` and `.hex` files.
- `output/firmware_<git-ref>.tar.gz` — packaged firmware.
- `can_library/generated/` and `can_library/dbc/` — generated CAN artifacts:
  - Header files for CAN message packing, unpacking, stale, and more
  - DBC files for use in the DAQ app and other tools

> [!WARNING]
> Every invocation performs a clean build.

## Static Analysis

Run cppcheck over `source/`, `common/`, and `can_library/` with:

```bash
python3 firmware/check_firmware.py
```

The check runs automatically in CI on every push and fails on any finding.
`cppcheck` must be installed; see the [setup guide](../docs/setup.md).

## Debugging and editor support

Open the repository root in VS Code. The supplied tasks run the fixed firmware
workflow, and `.vscode/launch.json` points Cortex-Debug at the target output
files.

Clangd reads `build/compile_commands.json` through the repository's `.clangd`
configuration. Run a firmware build to update the compile commands before using
code navigation or diagnostics.

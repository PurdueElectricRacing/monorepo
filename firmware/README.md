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

## G4 clocks

STM32G4 applications and bench tests run SYSCLK, AHB, APB1, and APB2 at
170 MHz using the RCC PLL/Range 1 boost preset. Resident bootloaders remain
at 16 MHz directly from HSE, leaving the PLL unused for application startup.
Vehicle boards retain their 16 MHz external clock input; bench tests use
HSI16. Peripheral timing must use the RCC clock getters rather than assume
a 16 MHz bus clock.

## G4 bootloader

The [bootloader guide](source/bootloader/README.md) documents architecture,
wire protocol, flash layout, and recovery. Use the command in [Building](#building)
to build all of: standalone firmware, resident bootloaders, and bootloadable applications.
The script then packages only bootloadable application binaries, CRCs, `output/manifest.json`, and
`firmware_<git-ref>.tar.gz`. Package copies under `output/images/` pad a final
partial word with erased `0xFF` before recording its size and CRC. Flash the
matching `bootloader_<node>` image first; DaqApp and the target share the
manifest format, CRC algorithm, address, board names, and CAN IDs.

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
- `can_library/generated/` — generated headers for CAN message packing, unpacking, stale detection, and more.
- `../superdbc/` — generated SuperDBC definitions and schema for use in the DAQ app and other tools.

> [!WARNING]
> Every invocation performs a clean build.


### Build Outputs

For non-bootloader targets, the build produces a single standalone image. For
bootloader targets, the build produces three image variants: standalone, resident bootloader, and bootloadable application. 

Each image has `.elf`, `.hex`, `.bin`, and `.elf.map` artifacts.

| Variant | Example ELF |
| --- | --- |
| Standalone | `output/a_box/a_box.elf` |
| Resident bootloader | `output/bootloader_a_box/bootloader_a_box.elf` |
| Bootloadable application | `output/bootloadable_a_box/bootloadable_a_box.elf` |

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

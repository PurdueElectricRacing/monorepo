# PER Software ⚡️

![Firmware](https://github.com/PurdueElectricRacing/monorepo/actions/workflows/build_firmware.yml/badge.svg?branch=master)
![DAQ Workspace](https://github.com/PurdueElectricRacing/monorepo/actions/workflows/build_daq.yml/badge.svg?branch=master)
![Documentation](https://github.com/PurdueElectricRacing/monorepo/actions/workflows/deploy_doxygen.yml/badge.svg?branch=master)
![GitHub commit activity](https://img.shields.io/github/commit-activity/m/PurdueElectricRacing/monorepo?style=flat-square)

<a href="https://www.vultr.com/"><img src="docs/assets/vultr.png" alt="Vultr" height="32"></a>

A monorepo of all firmware projects, shared libraries, code generation, and off-car tooling for PER FSAE EV.


## Directory Structure
- `daq/`
	- Desktop app (`daqapp`)
	- CLI (`daqcli`)
	- Shared-library (`daqcore`)
	- Data backend (`daqcloud`)
- `docs/` - Shared documentation
- `firmware/` - Embedded firmware, CAN library, and shared C code
- `generators/` - CANpiler + Fault Generator
- `plot_factory/` - Automated plotting pipeline
- `tests/` - Host side and cross-project integration tests

## Doxygen
Most recent doxygen deployment (master branch): https://purdueelectricracing.github.io/monorepo/


## Getting Started

To compile software for the PER vehicle, make sure your system is set up by following the steps in [setup.md](docs/setup.md) if you haven’t already.

> [!NOTE]
> [setup.md](docs/setup.md) is here!


## Building

`build_all.py` is the repository-level build entry point. It runs the complete
firmware, FIL build and PER integration tests, DAQ workspace, and host-test
workflows in order.

From the repository root, build all projects and run tests with:
```bash
python3 build_all.py
```

Each component also has a fixed build command. To build and package all firmware
boards:
```bash
python3 firmware/build_firmware.py
```

To run static analysis over all firmware boards:
```bash
python3 firmware/check_firmware.py
```

To build every DAQ workspace member and target:
```bash
cargo build --manifest-path daq/Cargo.toml --workspace --all-targets --locked
```

Running DAQ binaries is supported only from their own package directories:
```bash
# DaqApp
cd daq/daqapp && cargo run
# DaqCLI
cd daq/daqcli && cargo run
# DaqCloud
cd daq/daqcloud && cargo run
```

To run tests and generate a coverage report:
```bash
python3 tests/run_tests.py
```

## Hardware Debugging 

In VS Code, go to **View → Run and Debug**, select the appropriate MCU target from the dropdown, then press the green ▶️ arrow to flash and live-debug the firmware.

Once everything is [set up](docs/setup.md), open the repository with `code .`.
You can then build all projects by pressing:

```
Ctrl + Shift + B on Windows/Linux
Cmd + Shift + B on macOS
```

This triggers the default build task configured in `.vscode/tasks.json`,
which runs the monorepo build process automatically.

## Emulator Debugging

Every vehicle board also runs in FIL, PER's firmware-in-the-loop emulator, with
no hardware attached. Build the firmware once, then in
**View → Run and Debug** pick an `Emulator: <board>` configuration and press the
green ▶️ arrow:

```bash
python3 firmware/build_firmware.py
```

The configuration builds FIL from `tests/external/fil`, starts
`fil debug tests/fil/boards/<board>.json --gdb-port 3333 --once`, and attaches
`arm-none-eabi-gdb` to it. Breakpoints, data watchpoints, stepping, flash
`load`, reset/restart, and the SVD peripheral view work as they do on hardware.
Semihosting is enabled in every emulator configuration, so `printf`-style debug
output appears in the Debug Console. Live Watch also works:
FIL serves the extension's second read-only GDB session on the same port and
answers variable reads between execution slices, so watched RAM symbols refresh
while the firmware runs without halting it. Peripheral (MMIO) reads and writes
are refused while the target runs.

The VS Code tasks also include `fil build` (build only the emulator binary),
`fil network run` (one simulated second of the six-board vehicle network), and
`fil monitor (telnet)` (attach to the live monitor: `mdw`/`mdb`/`regs` while the
firmware keeps running). See [tests/fil/README.md](tests/fil/README.md) for the
emulator scenarios and test runner.

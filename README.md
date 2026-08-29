# PER Software ⚡️

![Firmware](https://github.com/PurdueElectricRacing/monorepo/actions/workflows/build_firmware.yml/badge.svg?branch=master)
![DaqApp](https://github.com/PurdueElectricRacing/monorepo/actions/workflows/build_daqapp.yml/badge.svg?branch=master)
![Documentation](https://github.com/PurdueElectricRacing/monorepo/actions/workflows/deploy_doxygen.yml/badge.svg?branch=master)
![GitHub commit activity](https://img.shields.io/github/commit-activity/m/PurdueElectricRacing/monorepo?style=flat-square)

A monorepo of all firmware projects, shared libraries, code generation, and off-car tooling for PER FSAE EV.


## Directory Structure
- `firmware/` - Embedded firmware, including its CAN library and shared C code
- `daqapp/` - Desktop DAQ application
- `docs/` - Shared documentation

## Doxygen
Most recent doxygen deployment (master branch): https://purdueelectricracing.github.io/monorepo/


## Getting Started

To compile software for the PER vehicle, make sure your system is set up by following the steps in [setup.md](docs/setup.md) if you haven’t already.

> [!NOTE]
> [setup.md](docs/setup.md) is here!


## Building

`build_all.py` is the repository-level build entry point. It runs the complete
firmware, DAQ app, and host-test workflows in order.

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

To build DaqApp:
```bash
cargo build --manifest-path daqapp/Cargo.toml
```

To run host tests and generate an HTML coverage report:
```bash
python3 tests/build_tests.py
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

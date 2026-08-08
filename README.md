# PER Software ⚡️

![Firmware](https://github.com/PurdueElectricRacing/monorepo/actions/workflows/build_firmware.yml/badge.svg?branch=master)
![DaqApp](https://github.com/PurdueElectricRacing/monorepo/actions/workflows/build_daqapp.yml/badge.svg?branch=master)
![Documentation](https://github.com/PurdueElectricRacing/monorepo/actions/workflows/doxygen.yml/badge.svg?branch=master)
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

`per_build.py` is the repository-level build entry point. It invokes each
project's own build system from the appropriate directory.

From the repository root, build all projects with:
```bash
python3 per_build.py
```

To build only firmware:
```bash
python3 per_build.py firmware --package
```

To build DaqApp only only:
```bash
python3 per_build.py daqapp
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

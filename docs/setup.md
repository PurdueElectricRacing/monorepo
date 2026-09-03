# PER Monorepo Setup

This guide sets up the complete PER software development environment.


## Table of Contents

- [1. Clone the Repository](#1-clone-the-repository)
  - [1.1 Install Git](#11-install-git)
  - [1.2 Clone the Repository](#12-clone-the-repository)
- [2. Install Visual Studio Code](#2-install-visual-studio-code)
  - [2.1 Install Recommended Extensions](#21-install-recommended-extensions)
- [3. Install Platform + Unit Testing Tools](#3-install-platform--unit-testing-tools)
  - [3A. macOS](#3a-macos)
	- [3A.1 Install Homebrew](#3a1-install-homebrew)
	- [3A.2 Install Apple's Command Line Tools](#3a2-install-apples-command-line-tools)
	- [3A.3 Install Firmware and Test Tools](#3a3-install-firmware-and-test-tools)
  - [3B. Linux](#3b-linux)
	- [3B.1 Update the System](#3b1-update-the-system)
	- [3B.2 Install Development Tools](#3b2-install-development-tools)
  - [3C. Windows - WSL](#3c-windows---wsl)
	- [3C.1 Install WSL](#3c1-install-wsl)
	- [3C.2 Update Ubuntu](#3c2-update-ubuntu)
	- [3C.3 Use VS Code inside of WSL](#3c3-use-vs-code-inside-of-wsl)
	- [3C.4 USB / ST-LINK Access](#3c4-usb--st-link-access)
- [4. Install Python Dependencies](#4-install-python-dependencies)
- [5. Install Rust/DAQ Tools](#5-install-rustdaq-tools)
  - [5.1 Rust](#51-rust)
  - [5.2 Linux / WSL DAQ Dependencies](#52-linux--wsl-daq-dependencies)
- [6. Build the Repository](#6-build-the-repository)
- [7. Individual Build Commands](#7-individual-build-commands)
  - [7.1 Build All Firmware](#71-build-all-firmware)
  - [7.2 Run Firmware Static Analysis](#72-run-firmware-static-analysis)
  - [7.3 Build the DAQ App](#73-build-the-daq-app)
  - [7.4 Build and Run Host Tests](#74-build-and-run-host-tests)
- [8. Build from VS Code](#8-build-from-vs-code)
- [9. Hardware Debugging](#9-hardware-debugging)

### Platforms supported

It is possible to build the PER software on macOS, Linux, and Windows.
However, if you are using Windows, you should use WSL 2 with Ubuntu
or dual boot into Linux.

## 1. Clone the Repository

### 1.1 Install Git

Git is required on every platform.

Check that it is installed:

```bash
git --version
```

If you don't have `git`, see: [Section 3](#3-install-platform-tools).

### 1.2 Clone the Repository

Open a terminal and move to the directory where you want to keep the PER code.
(Recommended: your home directory.)

Then clone the repository with its submodules:

```bash
git clone --recurse-submodules https://github.com/PurdueElectricRacing/monorepo.git
```

If you already cloned the repository without `--recurse-submodules`, initialize the submodules manually:

```bash
git submodule update --init --recursive
```

Check that it worked and submodules were also cloned:

```bash
ls monorepo
ls monorepo/firmware/external/cmsis-device-g4/
```

You should see something like:
```text
build	      daqapp	  firmware   output	       tests
build_all.py  Dockerfile  LICENSE    README.md
can_library   docs	  nix_flake  requirements.txt
```
```text
CODE_OF_CONDUCT.md  _htmresc  LICENSE.md  Release_Notes.html  Source
CONTRIBUTING.md     Include   README.md   SECURITY.md
```

## 2. Install Visual Studio Code

VS Code is the recommended editor for PER development.

Download and install VS Code:

* **macOS:** [VS Code for macOS](https://code.visualstudio.com/docs/setup/mac)
* **Windows:** [VS Code for Windows](https://code.visualstudio.com/docs/setup/windows)
* **Linux:** [VS Code for Linux](https://code.visualstudio.com/docs/setup/linux)

After installing VS Code, open the repository:

```bash
cd monorepo # note: depends on where you cloned the repository
code .
```

You can also open it from the VS Code GUI: File -> Open Folder ->
then navigate to where you cloned `monorepo`

### 2.1 Install Recommended Extensions

When VS Code prompts you to install the recommended extensions, install all of them.

You can also open the Extensions panel:

* **Windows/Linux:** `Ctrl + Shift + X`
* **macOS:** `Cmd + Shift + X`
* Or click the Extensions icon on the left sidebar

and select Install All under the recommended extensions.

## 3. Install Platform + Unit Testing Tools

Install the tools for your operating system.

### 3A. macOS

#### 3A.1 Install Homebrew

Install [Homebrew](https://brew.sh/) if it is not already installed.

Check:

```bash
brew --version
```

#### 3A.2 Install Apple's Command Line Tools

Run:

```bash
xcode-select --install
```

These provide the host compiler and other standard development tools used by the project.

#### 3A.3 Install Firmware and Test Tools

Run:

```bash
brew install \
	git \
	cmake \
	ninja \
	openocd \
	stlink \
	python3 \
	lcov \
	googletest \
	cppcheck
```

Install the ARM embedded compiler:

```bash
brew install --cask gcc-arm-embedded
```

Check:

```bash
git --version
cmake --version
ninja --version
python3 --version
arm-none-eabi-gcc --version
openocd --version
```

### 3B. Linux

If you are using linux, you probably already know what you're doing.
However, here is an overview of the setup for Debian/`apt`-based distributions (Ubuntu, Debian, Pop!_OS, etc.).


#### 3B.1 Update the System

```bash
sudo apt update
sudo apt upgrade
```

#### 3B.2 Install Development Tools

```bash
sudo apt install \
	git \
	cmake \
	ninja-build \
	gcc \
	g++ \
	python3 \
	python3-pip \
	python3-venv \
	gcc-arm-none-eabi \
	openocd \
	stlink-tools \
	cppcheck \
	lcov \
	libgtest-dev \
	pkg-config \
	libudev-dev \
	usbutils
```

Check:

```bash
git --version
cmake --version
ninja --version
python3 --version
arm-none-eabi-gcc --version
openocd --version
```

### 3C. Windows - WSL

It is highly advised to either use WSL 2 with Ubuntu or dual boot into Linux for PER development.

#### 3C.1 Install WSL

Follow Microsoft's WSL installation guide: https://learn.microsoft.com/en-us/windows/wsl/install

After installation, install the most recent Ubuntu distribution.
(As of the time of writing, Ubuntu 26.04 is the latest version.)

Open a WSL terminal and check:

```bash
uname -a
```

#### 3C.2 Update Ubuntu

Inside WSL:

```bash
sudo apt update
sudo apt upgrade
```

Then follow the [Linux instructions in Section 3B](#3b-linux) inside of WSL.

#### 3C.3 Use VS Code inside of WSL

Install the [normal Windows version of VS Code](#2-install-visual-studio-code).

Then install the WSL extension.

From the WSL terminal, enter the repository and run:

```bash
code .
```

VS Code should open the repository using the WSL environment.

You can also open the repository inside of WSL + VS Code by `Ctrl + Shift + P` -> "WSL: Reopen Folder in WSL".


#### 3C.4 USB / ST-LINK Access

Building inside WSL does not require USB access.

However. You will need additional USB configuration to connect an ST-LINK debugger directly to WSL.

This can be configured like so:
* Microsoft's WSL USB documentation:
https://learn.microsoft.com/en-us/windows/wsl/connect-usb

## 4. Install Python Dependencies

PER's build and code-generation tooling uses Python.

The repository currently pins its Python dependencies in `requirements.txt`.

For best integration with the VS Code tasks/launch configurations, install the dependencies to
your system Python:
```bash
python3 -m pip install -r requirements.txt
```
> [!NOTE]
> If you encounter an “externally managed environment” error, you can run:
> ```bash
> python3 -m pip install -r requirements.txt --break-system-packages
> ```

## 5. Install Rust/DAQ Tools

### 5.1 Rust

Install Rust using `rustup`.

Run:

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

Then restart your terminal.

Use the stable Rust toolchain:

```bash
rustup default stable
```

Check:

```bash
rustc --version
cargo --version
```

### 5.2 Linux / WSL DAQ Dependencies

Linux needs additional packages for serial-device enumeration:

```bash
sudo apt install libudev-dev pkg-config
```

These were already installed in the Linux setup section.

## 6. Build the Repository

The repository's main build entry point is:

```bash
python build_all.py
```

This builds the firmware, DAQ App, and host tests.

Run it from the repository root:

```bash
python build_all.py
```

If this completes successfully, your development environment is set up correctly.


## 7. Individual Build Commands

You can also build individual parts of the repository.

### 7.1 Build All Firmware

```bash
python firmware/build_firmware.py
```

### 7.2 Run Firmware Static Analysis

```bash
python firmware/check_firmware.py
```

### 7.3 Build the DAQ App

```bash
cargo build --manifest-path daqapp/Cargo.toml
```

### 7.4 Build and Run Host Tests

```bash
python tests/build_tests.py
```

This also generates the host-test coverage report.

## 8. Build from VS Code

Once the repository is open in VS Code, use:

* **Windows/Linux:** `Ctrl + Shift + B`
* **macOS:** `Cmd + Shift + B`

This will run the default build task, which builds the entire repository.

You can also use:

* `Ctrl/Cmd + Shift + B` -> "Tasks: Run Task"

Then select the individual build task you want to run.

## 9. Hardware Debugging

Once your software build works, you can connect an ST-LINK debugger and flash firmware.

In VS Code:

1. Open Run and Debug
2. Select the appropriate MCU target
3. Connect the ST-LINK to the target board
4. Press the green ▶ button

The repository contains VS Code launch configurations for the supported targets.

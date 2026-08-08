# PER Monorepo Setup Instructions

# 1. Tools

The PER firmware development environment relies on several tools -- such as `cmake`, `ninja`, and `stlink` -- to build, debug, flash, and deploy code to the vehicle. To streamline setup, we (highly) recommend using a package manager compatible with your operating system. Package managers help manage dependencies and ensure tools are correctly installed and updated. Below are supported package manager setups for each OS (macOS, Windows, Linux):


## MacOS Tools Setup
1. [Homebrew](https://brew.sh/): macOS package manager. Open a terminal (e.g., iTerm) and paste and run the installation command provided on the homebrew install page.
	- After installing Homebrew, make sure to add /opt/homebrew/bin to your system’s PATH environment variable. The exact command should be printed at the end of the homebrew install.
	- After installation, ensure brew is installed by running:
	`brew --version`

2. Run the following commands in your terminal to install the other tools (you can copy and paste them all at once).
```bash
brew install git cmake ninja openocd stlink python3
brew install --cask gcc-arm-embedded
```

3. Download VSCode from the website: https://code.visualstudio.com/docs/setup/mac

## Windows Tools Setup
You should use WSL if possible, the choco toolchain for Windows is too old. Install [here](https://learn.microsoft.com/en-us/windows/wsl/install).

1. If you are using Ubuntu version, use the newest version as Ubuntu is a bit weird with package versions as well. You may need to update it. 
2. Follow the Linux steps after this
3. VSCode should have good integration with WSL, you can use the windows version of VSCode with WSL extension.
4. You will have to setup USB on WSL (it is a bit weird). You can do this when you are done with onboarding, and you can build the repository. [Instructions](https://learn.microsoft.com/en-us/windows/wsl/connect-usb)

Native Windows is still possible:
1. [Chocolatey](https://chocolatey.org/install#install-step2): Windows package manager Paste and run the installation command provided on the Chocolatey install page.
	- Open the Start Menu, scroll to W, and locate Windows PowerShell. Right-click on PowerShell and select "Run as Administrator".
	- After installation, confirm choco is installed by running:
	`choco --version`
2. In your administrator powershell, paste the following commands (right click to paste in powershell terminal).

```bash
choco install git cmake ninja python3 openocd 
```
- Enter (`A`) on the first prompt to select 'Yes to All'

> [!NOTE]
> Make sure to run these in PowerShell as Administrator!

3. STLink drivers need to be manually installed from [here](https://www.st.com/en/development-tools/stsw-link009.html).

4. gcc-arm-embedded needs to be manually installed from [here](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (Download and install arm-gnu-toolchain-15.2.rel1-mingw-w64-x86_64-arm-none-eabi.msi)

5. Download VSCode from the website: https://code.visualstudio.com/download

## Linux Tools Setup
1. You probably already know what you're doing, so here are the commands for `apt` (Ubuntu, Debian, Pop!, etc.):
```bash
sudo apt update && sudo apt upgrade
sudo apt install git cmake python3 python3-pip ninja-build gcc-arm-none-eabi openocd stlink-tools
```
2. Visual Studio Code requires some special attention, install from [here](https://code.visualstudio.com/docs/setup/linux).

## DAQ App Tools Setup

The DAQ app is written in Rust. Install the stable Rust toolchain with
[rustup](https://rustup.rs/); this also installs `cargo`, Rust's build tool and
package manager.

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source "$HOME/.cargo/env"
rustup default stable
cargo --version
```

On Linux, install the development files used to enumerate serial devices:

```bash
sudo apt install libudev-dev pkg-config
```

On Windows, follow these steps inside WSL. macOS needs no additional DAQ app
system packages.


# 2. VSCode Setup (All OS)

VS Code is the recommended editor for firmware development.

## Install Extensions
Open the Extensions view (Ctrl+Shift+X or Cmd+Shift+X) and install the [PER extension pack](https://marketplace.visualstudio.com/items?itemName=irvingywang.per-pack) by searching the extension marketplace for "PER-Pack".
> [!WARNING]
> PER-Pack is required! Dont skip this step.

## Turn on Autosave
Go to `File -> Autosave` and click check to turn on autosave.


# 3. Repository Setup (All OS)

Follow these steps to download the PER codebase and get started on development:


## Open a new terminal
Open a new terminal. Just use your regular user account. 

For Windows, open your terminal or WSL.

You should land in your home directory:
```bash
/home/{USER}
```

For MacOS:
```bash
/Users/{username}
```

You can confirm with:
```bash
pwd
```

## Clone PER repo
1. Once you're in your home directory, clone the PER monorepo with the following command:
```bash
git clone https://github.com/PurdueElectricRacing/monorepo.git
```

2. Enter the repository:
```bash
cd monorepo
```

## Setup & Build
1. To build firmware, install the required Python packages.

You can install the dependencies in one of two ways:

**Option A** — Use a virtual environment to isolate dependencies
```bash
python3 -m venv .venv
source .venv/bin/activate  # On Windows: .venv\Scripts\activate
pip3 install -r requirements.txt
```
Before running any build commands, make sure to activate the virtual environment each time by running:
```bash
source .venv/bin/activate  # On Windows: .venv\Scripts\activate
```

**Option B (Simpler)** — Install directly to your system Python
```bash
pip3 install -r requirements.txt
```
> [!NOTE]
> If you encounter an “externally managed environment” error when using Option B, run:
> ```bash
> pip3 install -r requirements.txt --break-system-packages
> ```


2. Build one project or both projects from the repository root:

```bash
python3 per_build.py firmware  # embedded firmware only
python3 per_build.py daqapp    # desktop DAQ app only
python3 per_build.py           # both projects
```

3. Launch Visual Studio Code:
```bash
code .
```
4. Try running a build by doing `CTRL/CMD + Shift + B` in your VSCode window

```
Ctrl + Shift + B on Windows/Linux
Cmd + Shift + B on macOS
```

All done!

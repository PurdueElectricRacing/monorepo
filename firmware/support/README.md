# Support
Vendor and host-side files used by the build system and debugger. Not compiled into firmware.

## `linker/`
Per-target GNU linker scripts. [`cmake/common_component.cmake`](../cmake/common_component.cmake) selects one based on the `LINKER_SCRIPT` argument plus a suffix (`.ld`, `_APP.ld`, or `_BL.ld`).
- `STM32F407VGTx_FLASH.ld` Standalone-app layout for the F4 targets (DAQ, f4_testing).
- `STM32F407VGTx_FLASH_APP.ld` F4 app layout when paired with the bootloader.
- `STM32F407VGTx_FLASH_BL.ld` F4 bootloader layout.
- `STM32G474RETX_FLASH.ld` Standalone-app layout for the G4 targets.
- `STM32G474RETX_FLASH_APP.ld` G4 app layout when paired with the bootloader.
- `STM32G474RETX_FLASH_BL.ld` G4 bootloader layout.

## `svd/`
CMSIS-SVD register descriptions used by Cortex-Debug to render peripheral state in VS Code.
- `STM32F407.svd` Used by the retained F407 `f4_testing` launch config.
- `STM32F423.svd` Used by the DAQ launch config for STM32F423RHT6.
- `STM32G474.svd` Used by G4 launch configs (Main Module, Dashboard, Driveline, Torque Vectoring, A_Box, PDU, g4_testing).

## `openocd/`
OpenOCD configs for debugging and board provisioning, plus ad-hoc helpers.
- `openocd-gdb.cfg` Auto-`resume` on `gdb-detach`. Loaded by the DAQ debug config.
- `cannector-option-bytes.cfg` Configures CANnector to boot from main flash independently of the BOOT0 pin. Run once per board using the VS Code option-byte launch entry or `openocd -f firmware/support/openocd/cannector-option-bytes.cfg` from the repository root. Reloading the persistent option bytes resets the MCU.
- `stlink.cfg` Local ST-Link interface override (not referenced from `.vscode/`).
- `mem_helper.tcl` Memory-access helper script (not referenced from `.vscode/`).
- `swj-dp.tcl` SWJ/SWD helper script (not referenced from `.vscode/`).

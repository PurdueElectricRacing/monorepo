# Support
Vendor and host-side files used by the build system and debugger. Not compiled into firmware.

## `linker/`
Per-target GNU linker scripts. [`cmake/common_component.cmake`](../cmake/common_component.cmake) selects one based on the `LINKER_SCRIPT` argument plus a suffix (`.ld`, `_APP.ld`, or `_BL.ld`).
- `STM32F423RHTx_FLASH.ld` Standalone layout for DAQ and f4_testing: 1536 KiB flash,
  320 KiB ordinary SRAM. Legacy `.ccmram` input sections are initialized with
  `.data` in SRAM; no separate CCM or alias region is allocated. No F4 bootloader
  or bootloadable-app layout is currently supported.
- `STM32G474RETX_FLASH.ld` Standalone-app layout for the G4 targets.
- `STM32G474RETX_FLASH_APP.ld` G4 app layout when paired with the bootloader.
- `STM32G474RETX_FLASH_BL.ld` G4 bootloader layout.

## `svd/`
CMSIS-SVD register descriptions used by Cortex-Debug to render peripheral state in VS Code.
- `STM32F423.svd` Used by DAQ and f4_testing launch configs for STM32F423RHT6.
- `STM32G474.svd` Used by G4 launch configs (Main Module, Dashboard, Driveline, Torque Vectoring, A_Box, PDU, g4_testing).

### F423 SVD provenance and corrections

The register description comes from ST's STM32F423 SVD 1.2 in the official
[Keil STM32F4xx_DFP v3.1.1 source](https://github.com/Open-CMSIS-Pack/STM32F4xx_DFP/blob/v3.1.1/CMSIS/SVD/STM32F423.svd).
Repository corrections cover interrupt metadata, the FSMC register address,
removal of nonexistent SPI6, CAN2/3 filter layouts and RCC fields. These were
checked against RM0430 and the F423 CMSIS header; other vendor register metadata
has not been exhaustively qualified. The ST copyright notice is preserved and
the license is in [`svd/LICENSE-Apache-2.0.txt`](svd/LICENSE-Apache-2.0.txt).

## `openocd/`
OpenOCD configs for debugging and board provisioning, plus ad-hoc helpers.
- `openocd-gdb.cfg` Auto-`resume` on `gdb-detach`. Loaded by the DAQ debug config.
- `cannector-option-bytes.cfg` Configures CANnector to boot from main flash independently of the BOOT0 pin. Run once per board using the VS Code option-byte launch entry or `openocd -f firmware/support/openocd/cannector-option-bytes.cfg` from the repository root. Reloading the persistent option bytes resets the MCU.
- `stlink.cfg` Local ST-Link interface override (not referenced from `.vscode/`).
- `mem_helper.tcl` Memory-access helper script (not referenced from `.vscode/`).
- `swj-dp.tcl` SWJ/SWD helper script (not referenced from `.vscode/`).

Both F4 targets select F423 CMSIS/startup/PHAL/RTOS libraries. This selects the
silicon and memory map; it does not complete the driver/clock audit or DAQ board
integration in [the migration plan](../../docs/f423_migration_checklist.md).
The DAQ schematic pinout remains assigned to a separate stacked PR.

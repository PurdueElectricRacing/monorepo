# Deployment and Debugging

Irving Wang (irvingw@purdue.edu)

## About
This document provides step by step instructions on how to flash firmware to PER devices and how to use the debugging tools set up for this project.


## Flashing Firmware
1. Open the run and debug panel by clicking the play icon on the left sidebar or pressing `Ctrl/Cmd + Shift + D`.
2. Select the appropriate debug configuration (based on node name) from the dropdown at the top of the panel.
3. Connect the device to your computer via an ST-Link.
4. Click the green play button at the top of the panel to start the flashing process.
5. If successful, you should see the debug program jump to the `main()` in the source code.

Additional Notes:
- The VSCode debug configurations are stored in the `.vscode/launch.json` file.
    - Helpful parameters to tweak include:
        - `preLaunchTask`: Comment it out to prevent automatic building before flashing.
        - `runToEntryPoint`: Comment it out to prevent automatic pausing at `main()`.

## STM32G474RE Nucleo bootloader smoke test

This uses the existing `main_module` bootloader identity so DaqApp requires no
special target support.

1. Build the test package and resident images:
   `python3 firmware/build_firmware.py --package-g4-testing`
2. Flash `firmware/output/bootloader_MAIN_MODULE/bootloader_MAIN_MODULE.hex`
   to the Nucleo over its initial ST-Link/SWD connection. Reset the board after
   programming.
3. Connect a 3.3 V CAN transceiver to the Nucleo: `PB13 (FDCAN2_TX) -> TXD`,
   `PB12 (FDCAN2_RX) <- RXD`, and connect 3V3 and GND. Connect transceiver
   `CANH`, `CANL`, and GND to the CANable. Provide normal 120 ohm termination
   at the two ends of the CAN bus.
4. In DaqApp, select the generated `firmware/can_library/dbc/VCAN_*.dbc`, set
   **CAN Bus** to **VCAN** and **CAN Speed** to **500k**, select the CANable
   serial port, and connect it.
5. Add the **Bootloader** widget, select
   `firmware/output/firmware_g4_testing_<hash>.tar.gz`, and wait for the
   `main_module` row to show **Available**. Select only that row and upload it.
   The Nucleo LED on PA5 should blink after the update completes. A subsequent
   upload exercises the application-to-bootloader reset handshake as well.

## Using the Debugging Tools
**VSCode debugger GUI**:
- Play/Pause: Start or pause execution.
- Click on the line numbers in the source code to set breakpoints. The execution will pause when it hits a breakpoint.
    - Breakpoints can be disabled/re-enabled without being removed by toggling the checkbox
- Reset: Reset the device (goes back to `main()`).
- Step Over: Execute the next line of code, skipping over function calls.
- Step Into: Step into function calls to debug inside them.
- Step Out: Step out of the current function.

**Livewatch**:
- Add variables (global ones only) to monitor their values in real-time during execution.
    - These values are updated around 5 times per second.
    - Enum members will show their name instead of numeric values

**Watch Expressions**:
- The debugger must be paused to add watch expressions.
- Used to view local and global variables.

**Peripheral View**:
- The debugger must be paused to view peripheral registers.
- The the current state of various MCU peripherals like GPIO, USART, TIMERS, etc can be viewed here.
- Extremely useful for debugging HAL-related issues.

**Call Stack**:
- View the current function call stack to understand the execution flow.

> [!NOTE]
> If you put a breakpoint in `HardFault_Handler()`, you can check the call stack to see where the fault originated from.

**GDB**:
- Open the debug console in VSCode to enter GDB commands directly.
- All standard GDB commands are supported:
    - `step`, `next`, `continue`, `break`, `print`, etc.
    - TUI mode (e.g `layout src/asm`) is not available.

> [!NOTE]
> The `disassemble {function_name}` command is used to view the assembly instructions of a function.
> This can be useful when your debugger seems to "jump around" unexpectedly in the VSCode GUI due to compiler optimizations.







# Dashboard
This node bears the primary responsibility of managing the pedalbox and thus is a torque-path critical board.

Additionally, it hosts the main driver interface for the vehicle, displaying telemetry on a Nextion LCD and handling user input through buttons.

## Notable Files
- [`main.c`](main.c) / [`main.h`](main.h): Initialization and application entry point for the dashboard node.
- `CMakeLists.txt`: Build configuration for the dashboard target.
- `pedals.c` / `pedals.h`: Pedalbox sampling, plausibility checks, and message TX.
- `driver_interface.c` / `driver_interface.h`: Periodic loop, button EXTI callback, and high-level page navigation actions.
- `lcd.c` / `lcd.h`: Page registry and dispatch (per-page `update`, `move_up`, `move_down`, `select`, `telemetry` callbacks).
- `menu_system.c` / `menu_system.h`: Generic Nextion menu framework. Defines `menu_element_t` / `menu_page_t` and the navigation/redraw helpers used by every page.
- `colors.h`: Named color constants used by the UI.
- `Dashboard26.HMI`: Nextion Editor design file for the Dashboard26 LCD. Open in Nextion Editor to modify the on-screen layout, then re-flash the LCD.
- `pages/`: Per-page implementations (`race`, `faults`, `calibration`, `amk`). Each page exposes the callback set required by `lcd.c`.
- [`telemetry.c`](telemetry/telemetry.c) / [`telemetry.h`](telemetry/telemetry.h)

## Hardware Block Diagram
![dashboard](dashboard.drawio.png)

## Design Files
![colors](design/PER26_colors.png)

![race](design/RACE.png)

![calibration](design/CALIBRATION.png)

![fault](design/FAULT.png)

![amk](design/AMK.png)

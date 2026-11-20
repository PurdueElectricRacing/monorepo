# Source
Firmware source code for PER's distributed vehicle software. One directory per node, each is compiled into a separate binary. See each node's own README for module-level details.

## Vehicle control nodes
- `main_module` Master control unit. Owns the vehicle state machine, SDC monitoring, inverter interface, and the final torque request.
- `dashboard` Pedalbox sampling/plausibility plus the driver UI rendered on the Nextion LCD.
- `torque_vector` State estimation and torque-vectoring control loops (does not directly drive the inverters).

## Power and sensing nodes
- `pdu` Low-voltage power distribution: rail switching, ADC telemetry, nFAULT scanning, and cooling control.
- `a_box` Accumulator (battery) box: BMS master, charging management, and battery telemetry.
- `driveline` Front/rear driveline sensor interface and CAN telemetry. The same source is compiled into two binaries via CMake preprocessor flags.

## Data acquisition
- `daq` CAN bus logging to SD card and real-time UDP/Ethernet streaming, fed by the in-house lockless SPMC queue.

## Bench / dev nodes
- `g4_testing` STM32G4-based bench testing target. Not deployed on the vehicle.

## Torque Path
![torque path](./torque_path.drawio.png)

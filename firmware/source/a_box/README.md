# A_BOX
A_BOX (short for "accumulator box") is the BMS master. It owns battery telemetry, the BMS shutdown rail (`BMS_SDC_CTRL`), and the charger control state machine. It is a safety-critical board.

## Notable Files
- [`main.c`](main.c) / [`main.h`](main.h): Peripheral init (FDCAN1/2, SPI1, ADC1+DMA, GPIO), task startup order, and the `bms_task` periodic that runs ADBMS updates and pack/thermal fault checks.
- `CMakeLists.txt`: Build configuration for the A_BOX target.
- [`adbms/README.md`](adbms/README.md): Canonical ADBMS6380 driver docs (high-level state machine, balancing, command/PEC flow). `g_bms` is shared with `bms_task` and the charger FSM.
- [`charging_fsm.c`](charging_fsm/charging_fsm.c) / [`charging_fsm.h`](charging_fsm/charging_fsm.h): Charger control FSM (`CHARGING_FSM_PERIOD_MS = 1000`) that clamps charge requests, toggles BMS balancing via `ADBMS_set_balancing_enabled`, and publishes `charging_fsm_internals`.
- [`telemetry.c`](telemetry/telemetry.c) / [`telemetry.h`](telemetry/telemetry.h): Multi-rate CAN telemetry publishers (on both VCAN and CCAN with `_ccan` suffix), using physical module numbering (`0..4`):
  - 100 Hz: `pack_analog`, rotating `cell_telemetry` (`5` modules x `29` cells = `145` cells, `1.45 s` full sweep)
  - 8 Hz: rotating `thermistor_telemetry` (`5` modules x `20` thermistors = `100` thermistors, `12.5 s` full sweep)
  - 0.2 Hz: `abox_version` with `FIRMWARE_GIT_HASH`
- `bms_task` in [`main.c`](main.c) (5 Hz): `pack_bms`, `bms_pecs`
- [`charging_fsm.c`](charging_fsm/charging_fsm.c) (1 Hz on CCAN): `charging_fsm_internals`
- [`thermistor.c`](thermistor/thermistor.c) / [`thermistor.h`](thermistor/thermistor.h): Datasheet-derived lookup table and `thermistor_R_to_T()` helper used by the ADBMS driver.
- Fault handling in `bms_task` (in [`main.c`](main.c)): raises `IMD`, `BMS_DISCONNECTED`, pack full/empty, cell under/overvoltage, and pack warm/cold/overtemp faults; `BMS_SDC_CTRL` is driven by `BMS_DISCONNECTED`.

## Pack-Level Voltage Threshold Audit (145S Topology)

With `5` modules of `29S` cells (`145` series cells total, nominal range `2.7 V..4.2 V` per cell -> `391.5 V..609.0 V` theoretical sum), the following pack-level thresholds were audited against the previous `112S` configuration (`112 * 2.7 V = 302.4 V`, `112 * 4.2 V = 470.4 V`) and left unchanged pending approved hardware/charger limits before hardware acceptance:
- `PACK_FULL` (`max: 470` V) and `PACK_EMPTY` (`min: 302` V) fault definitions in `generators/configs/nodes/A_BOX.json` (and the corresponding `< 470` V check in `firmware/source/main_module/torque_controller/torque_controller.c`).
- `MAX_PACK_CHARGING_VOLTS` (`540` V) in `firmware/source/a_box/charging_fsm/charging_fsm.c`.

## Hardware Block Diagram
![abox](abox.drawio.png)

## Charging
![charging fsm](charging_fsm/charging_FSM.drawio.png)
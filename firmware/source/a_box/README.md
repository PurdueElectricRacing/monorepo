# A_BOX
A_BOX (short for "accumulator box") is the BMS master. It owns battery telemetry, the BMS shutdown rail (`BMS_SDC_CTRL`), and the charger control state machine. It is a safety-critical board.

## Notable Files
- [`main.c`](main.c) / [`main.h`](main.h): Peripheral init (FDCAN1/2, SPI1, ADC1+DMA, GPIO), task startup order, and the `bms_task` periodic that runs ADBMS updates and pack/thermal fault checks.
- `CMakeLists.txt`: Build configuration for the A_BOX target.
- [`adbms/README.md`](adbms/README.md): Canonical ADBMS6380 driver docs (high-level state machine, balancing, command/PEC flow). `g_bms` is shared with `bms_task` and the charger FSM.
- [`charging_fsm.c`](charging_fsm/charging_fsm.c) / [`charging_fsm.h`](charging_fsm/charging_fsm.h): Charger control FSM (`CHARGING_FSM_PERIOD_MS = 1000`) that clamps charge requests, toggles `g_bms.is_balancing_enabled`, and publishes `charging_fsm_internals`.
- [`telemetry.c`](telemetry/telemetry.c) / [`telemetry.h`](telemetry/telemetry.h): Multi-rate CAN telemetry publishers (on both VCAN and CCAN with `_ccan` suffix):
  - 100 Hz: `pack_analog`, rotating `cell_telemetry`
  - 8 Hz: rotating `thermistor_telemetry`
  - 0.2 Hz: `abox_version` with `FIRMWARE_GIT_HASH`
- `bms_task` in [`main.c`](main.c) (5 Hz): `pack_bms`, `bms_pecs`
- [`charging_fsm.c`](charging_fsm/charging_fsm.c) (1 Hz on CCAN): `charging_fsm_internals`
- [`thermistor.c`](thermistor/thermistor.c) / [`thermistor.h`](thermistor/thermistor.h): Datasheet-derived lookup table and `thermistor_R_to_T()` helper used by the ADBMS driver.
- Fault handling in `bms_task` (in [`main.c`](main.c)): raises `IMD`, `BMS_DISCONNECTED`, pack full/empty, cell under/overvoltage, and pack warm/cold/overtemp faults; `BMS_DISCONNECTED` also drives `BMS_SDC_CTRL`.

## Hardware Block Diagram
![abox](abox.drawio.png)

## Charging
![charging fsm](charging_fsm/charging_FSM.drawio.png)
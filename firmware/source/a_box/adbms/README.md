# ADBMS(6380)

Provides a high-level interface for managing `ADBMS6380` battery management chips
in a single-direction isoSPI daisy chain configuration (`ADBMS6830B` in Analog Devices
datasheets; the `ADBMS6380` identifier spelling is retained across firmware for API stability).
It handles communication, configuration, measurement collection, and balancing
for multi-module battery packs, supporting cell voltage, thermistor, and GPIO readings
as well as error detection via PEC checks.

## Pack & Daisy-Chain Topology (5 Modules / 29S / 10 Chips)

The pack consists of **5 physical battery modules**, each containing **2 ADBMS chips**
(`Chip A` and `Chip B`), for **10 chips** total in the single isoSPI daisy chain.

| Quantity | Value | Notes |
| --- | --- | --- |
| Physical modules (`ADBMS_MODULE_COUNT`) | `5` (`0..4`) | Public module numbering used by getters and CAN telemetry |
| Chips per module (`ADBMS_CHIPS_PER_MODULE`) | `2` (`Chip A`, `Chip B`) | Both chips sit on the same isoSPI daisy chain |
| Total chips (`ADBMS_CHIP_COUNT`) | `10` (`0..9`) | Raw protocol/storage sizing (`dev.chips[10]`) |
| Cells per module (`ADBMS_CELLS_PER_MODULE`) | `29` (`0..28`) | `0..15` on `Chip A` (`C1..C16`), `16..28` on `Chip B` (`C1..C13`) |
| Unused cell channels on `Chip B` | `3` (zero-based `13..15`, 1-based `C14..C16`) | Shorted on hardware; excluded from stats, validity checks, telemetry, and balancing (`DCC14..DCC16` forced clear) |
| Total pack series cells (`ADBMS_PACK_CELL_COUNT`) | `145` | `5 * 29` |
| Thermistors per module (`ADBMS_THERMISTORS_PER_MODULE`) | `20` (`0..19`) | `0..9` on `Chip A` (`GPIO1..GPIO10`), `10..19` on `Chip B` (`GPIO1..GPIO10`); all 20 assumed populated |
| Total pack thermistors (`ADBMS_PACK_THERMISTOR_COUNT`) | `100` | `5 * 20` |

### Physical-to-Chain Mapping (`ADBMS_MODULE_CHIP_INDICES`)

- Raw protocol and register storage in `adbms6380_driver_t` is indexed by daisy-chain chip index (`dev.chips[0..9]`, each with `16` cell channels and `10` GPIO channels).
- Public APIs (`ADBMS_get_cell_voltage`, `ADBMS_get_therm_temp`, `ADBMS_is_cell_discharging`) and CAN telemetry (`cell_telemetry`, `thermistor_telemetry`) use **physical module and channel indices** (`module_idx` in `0..4`, `cell_idx` in `0..28`, `therm_idx` in `0..19`).
- `ADBMS_MODULE_CHIP_INDICES` in `adbms_config.h` is the single source of truth mapping `(module_idx, ADBMS_CHIP_ROLE_A / B)` to daisy-chain chip index `0..9` (defaulting to contiguous `[2 * m, 2 * m + 1]` ordering).
- **Hardware bring-up requirement**: Verify the physical isoSPI harness and board-to-board chain order on the accumulator before hardware acceptance; if the physical wiring differs from `[2 * m, 2 * m + 1]`, update only `ADBMS_MODULE_CHIP_INDICES` in `adbms_config.h`.

## Usage

### Initialization

1. Define a global `adbms_bms_t` struct:
   ```c
   adbms_bms_t g_bms = {0};
   ```

2. Configure your SPI peripheral and GPIOs.
Note that the driver expects manual control of the CS line
(`nss_sw` must be false in the SPI config).

3. Initialize the driver in `main` (after GPIO setup):
   ```c
   ADBMS_init(&g_bms, &bms_spi_config);
   ```

### Periodic Operation

Call the periodic function regularly:
```c
void g_bms_periodic(void) {
    ADBMS_periodic(&g_bms, MIN_V_FOR_BALANCE, MIN_DELTA_FOR_BALANCE);
}
```

This updates cell voltage, thermistor readings, and error flags
(and all the aggregate statistics) in the `adbms_bms_t g_bms` struct.

#### For example

```c
RTOS_DEFINE_TASK(g_bms_periodic, 200, TASK_PRIORITY_HIGH, 2048);

int main(void) {
    // ... GPIO and SPI initialization ...

    ADBMS_init(&g_bms, &bms_spi_config);

    // Create periodic thread
    RTOS_START_TASK(g_bms_periodic);

    // ... rest of main ...
    vTaskStartScheduler();
}
```

BMS task should have high priority to ensure timely updates of measurements and
so that SPI communication is not delayed/interrupted.


### Accessing Measurements

After each periodic call, measurements, statistics, and status can be accessed via thez `ADBMS_` getters:
- `ADBMS_get_cell_voltage(&g_bms, module_idx, cell_idx)`: Per-module cell voltage in volts
- `ADBMS_is_cell_discharging(&g_bms, module_idx, cell_idx)`: Whether a cell is actively discharging (balancing)
- `ADBMS_get_therm_temp(&g_bms, module_idx, therm_idx)`: Per-module thermistor temperature in Celsius
- `ADBMS_get_min/max/sum_voltage(&g_bms)`: Pack-level cell voltage statistics
- `ADBMS_get_min/max/avg_therm_temp(&g_bms)`: Pack-level thermistor temperature statistics
- `ADBMS_get_state(&g_bms)`: Current `adbms_state_t` (connecting, read loop, recovering, fatal)
- Error flags:
	- `ADBMS_get_cell_voltage_pec_err(&g_bms, cmd_idx)` and `ADBMS_get_gpio_voltage_pec_err(&g_bms, cmd_idx)`: PEC error flags for cell and GPIO voltage read commands. In the case of a PEC error during reading cell or GPIO voltages (which require multiple read commands), only the specific command that failed will have its PEC error flag set and the rest of the voltages for successful commands will still be updated/valid and the pack-level statistics will be updated based on the new valid readings and the old readings for the failed commands.
	- Internal register mismatch and PEC flags also also saved in `g_bms.dev`

### Controlling the Balancing

- To enable or disable BMS balancing, call `ADBMS_set_balancing_enabled` before the next periodic call:
```c
ADBMS_set_balancing_enabled(&g_bms, true); // or false to disable balancing
```
The driver will handle the rest (setting the appropriate bits in the REGB register of each module based on the cell voltages and the configured thresholds).

#### Balancing algorithm

The balancing algorithm is a simple threshold-based approach:
- If the minimum cell voltage in the pack exceeds `MIN_V_FOR_BALANCE` (defined in ABOX)
- And the difference between a cell voltage and the minimum cell voltage is greater than `MIN_DELTA_FOR_BALANCE` (also defined in ABOX)
- Then the discharge (balancing) for that cell will be enabled by setting the appropriate bit in the REGB register of the chip that contains that cell.
- Otherwise, discharge for that cell will be disabled.
- Unused `Chip B` channels (`13..15` zero-based, `DCC14..DCC16` one-based) are never evaluated for balancing and are explicitly cleared before every `ADBMS6380_write_regb` call (in both `ADBMS_STATE_CONNECTING` and `ADBMS_STATE_READ_LOOP`).

## File Structure

- `adbms_config.h`: Shared pack topology configuration (`ADBMS_MODULE_COUNT`, `ADBMS_CHIP_COUNT`, `ADBMS_MODULE_CHIP_INDICES`, channel mapping helpers)
- `adbms.c` / `adbms.h`: High-level flow driver logic, pack statistics, balancing, state management
- `adbms6380.c` / `adbms6380.h`: Low-level chip-specific protocol, SPI framing, register packing, multi-command readout, thermistor conversion
- `commands.h`: `static constexpr` command definitions for ADBMS6380.
- `pec.c` / `pec.h`: PEC (CRC15/CRC10) calculations

## State Machine

![ADBMS FSM](./ADBMS_FSM.drawio.png)

## Notes

- The driver is designed for FreeRTOS-based systems with a 1000Hz tick rate (1ms tick period).
  - Its primary reliance on FreeRTOS comes from the use of `RTOS_delay_ms(ADBMS6380_WAKE_DELAY_MS)` in the wake-up sequence.
- All communication is performed via SPI, with manual CS control for timing.
	- The SPI transfer is a blocking DMA operation and should not fail (returns void)
- Error handling: flags for PEC checksum failures and register read-back mismatches. During `ADBMS_STATE_READ_LOOP`, if a single cell or GPIO read command group fails PEC validation, only that command group's PEC error flag is set and its channels retain their previous sample while successful command groups update normally.
- Thermistor readout (`ADBMS6380_read_therms`) assumes all `20` thermistors per module (`10` per chip, `100` total) are populated and starts an `ADAX` conversion immediately followed by `RDAUXA..D` reads without conversion-completion polling. If hardware bring-up reveals conversion-latency artifacts or unpopulated GPIO channels, handle conversion polling / open-circuit rejection in a dedicated follow-up.
- Sampling vs. CAN telemetry sweep rates: `bms_task` runs `ADBMS_periodic` at `5 Hz` (`200 ms`), updating all `145` cell voltages and `100` thermistor temperatures in `g_bms` every `200 ms`. CAN telemetry rotates one slot per frame: `cell_telemetry` at `100 Hz` sweeps all `145` cells in `1.45 s` (`145 / 100 Hz`), and `thermistor_telemetry` at `8 Hz` sweeps all `100` thermistors in `12.5 s` (`100 / 8 Hz`).
- Make sure `ADBMS_MODULE_COUNT`, `ADBMS_CHIP_COUNT`, and `ADBMS_MODULE_CHIP_INDICES` in `adbms_config.h` match the physical accumulator daisy-chain configuration.
- There is a hidden reliance on a global variable defined in `../thermistor/thermistor.c` (and `extern`ed in the header): `thermistor_lut`

## Authors

- Primary: Millan Kumar
- Supporting:
	- Irving Wang
	- Ronak Jain
	- Shriya Balu
	- Sebastian Arthur

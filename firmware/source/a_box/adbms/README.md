# ADBMS(6380)

Provides a high-level interface for managing `ADBMS6380` battery management modules
in a single-direction daisy chain configuration.
It handles communication, configuration, measurement collection, and balancing
for multi-module battery packs, supporting cell voltage, thermistor, and GPIO readings
as well as error detection via PEC checks.

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

After each periodic call, measurements, statistics, and status can be queried via the `ADBMS_` getters:
- `ADBMS_get_cell_voltage(&g_bms, module_idx, cell_idx)`: Per-module cell voltage in volts (`ADBMS6380_CELL_COUNT` (16) per module)
- `ADBMS_is_cell_discharging(&g_bms, module_idx, cell_idx)`: Whether a cell is actively discharging (balancing)
- `ADBMS_get_therm_temp(&g_bms, module_idx, therm_idx)`: Per-module thermistor temperature in Celsius (`ADBMS6380_GPIO_COUNT` (10) per module)
	- Temps calculated with the following flow:
		1. GPIO voltage (`g_bms.dev.modules[i].gpio_voltages`)
		2. R2 in the thermistor divider
			- R1 = `ADBMS6380_GPIO_R1_OHM` = 10kOhms
			- Vin = VREG2 = `ADBMS6380_GPIO_VIN_V` = 3.0v
		3. Temperature via lerp of datasheet values
- `ADBMS_get_min_voltage(&g_bms)` / `ADBMS_get_max_voltage(&g_bms)` / `ADBMS_get_sum_voltage(&g_bms)`: Pack-level cell voltage statistics
- `ADBMS_get_min_therm_temp(&g_bms)` / `ADBMS_get_max_therm_temp(&g_bms)` / `ADBMS_get_avg_therm_temp(&g_bms)`: Pack-level thermistor temperature statistics
- `ADBMS_get_state(&g_bms)`: Current `adbms_state_t` (`ADBMS_STATE_CONNECTING`, `ADBMS_STATE_READ_LOOP`, `ADBMS_STATE_RECOVERING`, `ADBMS_STATE_FATAL`)
- Error flags:
	- `ADBMS_get_cell_voltage_pec_err(&g_bms, cmd_idx)` and `ADBMS_get_gpio_voltage_pec_err(&g_bms, cmd_idx)`: PEC error flags for cell and GPIO voltage read commands. In the case of a PEC error during reading cell or GPIO voltages (which require multiple read commands), only the specific command that failed will have its PEC error flag set and the rest of the voltages for successful commands will still be updated/valid and the pack-level statistics will be updated based on the new valid readings and the old readings for the failed commands.
	- Internal register mismatch and PEC flags on `g_bms.dev`: `g_bms.dev.modules[i].err_rega_mismatch`, `g_bms.dev.modules[i].err_regb_mismatch`, `ADBMS6380_has_rega_mismatch(&g_bms.dev)`, `ADBMS6380_has_regb_mismatch(&g_bms.dev)`, `g_bms.dev.err_rega_pec`, and `g_bms.dev.err_regb_pec`.

### Controlling the Balancing
- To enable or disable BMS balancing, call `ADBMS_set_balancing_enabled` before the next periodic call (and query with `ADBMS_is_balancing_enabled`):
```c
ADBMS_set_balancing_enabled(&g_bms, true); // or false to disable balancing
```
The driver will handle the rest (setting the appropriate bits in the REGB register of each module based on the cell voltages and the configured thresholds).

#### Balancing algorithm

The balancing algorithm is a simple threshold-based approach:
- If the minimum cell voltage in the pack exceeds `MIN_V_FOR_BALANCE` (defined in ABOX)
- And the difference between a cell voltage and the minimum cell voltage is greater than `MIN_DELTA_FOR_BALANCE` (also defined in ABOX)
- Then the discharge (balancing) for that cell will be enabled by setting the appropriate bit in the REGB register of the module that contains that cell.
- Otherwise, discharge for that cell will be disabled.

## File Structure

- `adbms_config.h`: Shared pack topology configuration (`ADBMS_MODULE_COUNT`).
- `adbms.c` / `adbms.h`: High-level flow driver logic, pack statistics, balancing, and state management (`ADBMS_` prefix).
- `adbms6380.c` / `adbms6380.h`: Low-level chip-specific protocol, SPI framing, register packing, multi-command readout, and thermistor conversion (`ADBMS6380_` prefix).
- `commands.h`: `static constexpr` command definitions for ADBMS6380.
- `pec.c` / `pec.h`: PEC (CRC15/CRC10) calculation utilities (`PEC_` prefix).

## State Machine

![ADBMS FSM](./ADBMS_FSM.drawio.png)

## Notes

- The driver is designed for FreeRTOS-based systems with a 1000Hz tick rate (1ms tick period).
  - Its primary reliance on FreeRTOS comes from the use of `RTOS_delay_ms(ADBMS6380_WAKE_DELAY_MS)` in the wake-up sequence.
- All communication is performed via SPI, with manual CS control for timing.
	- The SPI transfer is a blocking DMA operation and should not fail (returns void)
- Error handling: flags for PEC checksum failures and register read-back mismatches
- Make sure the `ADBMS_MODULE_COUNT` constant in `adbms_config.h` matches the actual number of modules in your daisy chain configuration!!
- There is a hidden reliance on a global variable defined in `../thermistor/thermistor.c` (and `extern`ed in the header): `thermistor_lut`

## Authors

- Primary: Millan Kumar
- Supporting:
	- Irving Wang
	- Ronak Jain
	- Shriya Balu
	- Sebastian Arthur

# ADBMS(6380)

Provides a high-level interface for managing `ADBMS6380` battery management modules
in a single-direction daisy chain configuration.
It handles communication, configuration, and measurement collection, and balancing
for multi-module battery packs, supporting cell voltage, thermistor, GPIO readings
as well as error detection via PEC checks.

## Usage

### Initialization

1. Define a global `adbms_bms_t` struct and a TX buffer:
   ```c
   adbms_bms_t g_bms = {0};
   uint8_t g_bms_tx_buf[ADBMS_SPI_TX_BUFFER_SIZE] = {0};
   ```

2. Configure your SPI peripheral and GPIOs.
Note that the driver expects manual control of the CS line
(`nss_sw` must be false in the SPI config).

3. Initialize the driver in main (after GPIO and SPI setup):
   ```c
   adbms_init(&g_bms, &bms_spi_config, g_bms_tx_buf);
   ```

### Periodic Operation

Call the periodic function regularly:
```c
void g_bms_periodic() {
	adbms_periodic(&g_bms, MIN_V_FOR_BALANCE, MIN_DELTA_FOR_BALANCE);
}
```

This updates cell voltage, thermistor readings, and error flags
(and all the aggregate statistics) in the `adbms_bms_t g_bms` struct.

#### For example

```c
RTOS_DEFINE_TASK(g_bms_periodic, 200, TASK_PRIORITY_HIGH, 2048);

int main() {
	// ... GPIO and SPI initialization ...

	adbms_init(&g_bms, &bms_spi_config, g_bms_tx_buf);

	// Create periodic thread
    RTOS_START_TASK(g_bms_periodic);

	// ... rest of main ...
    vTaskStartScheduler();
}
```

BMS task should have high priority to ensure timely updates of measurements and
so that SPI communication is not delayed/interrupted.


### Accessing Measurements

After each periodic call, values can be read from the `adbms_bms_t` struct:
- `g_bms.modules[i].cell_voltages`: Per-module cell voltages
	- `ADBMS6380_CELL_COUNT` (16) per module
- `g_bms.modules[i].therms_temps`: Per-module thermistor temperatures in Celsius
	- `ADBMS6380_GPIO_COUNT` (10) per module
	- Temps calculated with the following flow:
		1. GPIO voltage
		2. R2 in the thermistor divider
			- R1 = `ADBMS_GPIO_R1` = 10kOhms
			- Vin = VREG2 = `ADBMS_GPIO_VIN` = 3.0v
		3. Temperature via lerp of datasheet values
- `g_bms.min_voltage`/`g_bms.max_voltage`/`g_bms.avg_voltage`/`g_bms.sum_voltage`: Pack-level cell voltage statistics
- `g_bms.min_temp`/`g_bms.max_temp`/`g_bms.avg_temp`:  Pack-level thermistor temperature statistics
- Error flags:
	- `g_bms.err_spi`: SPI HAL error
	- `g_bms.err_rega_mismatch`: REGA register mismatch error
		- The configuration we tried to write did not match the reading. Indicates a potential communication issue or misconfiguration.
		- Same for REGB.
		- There is also a flag in each module struct for per-module REGA and REGB mismatch errors (`g_bms.modules[i].err_rega_mismatch` and `g_bms.modules[i].err_regb_mismatch`)
			- Indicate if there was a mismatch for that specific module (as opposed to the pack-level flags which indicate if there was a mismatch for any module in the pack).
	- `g_bms.err_*_pec`: PEC error flags for various operations (register reads, cell voltage reads, GPIO voltage reads). Indicates potential communication issues or data corruption. In the case of a PEC error during reading cell or GPIO voltages (which require multiple read commands), only the specific command that failed will have its PEC error flag set and the rest of the voltages for successful commands will still be updated/valid and the pack-level statistics will be updated based on the new valid readings and the old readings for the failed commands.

### Controlling the Balancing
- To enable or disable BMS balancing, simply set the `is_discharge_enabled` field in the `adbms_bms_t` struct before the next periodic call:
```c
g_bms.is_discharge_enabled = true; // or false to disable balancing
```
The driver will handle the rest (setting the appropriate bits in the REGB register of each module based on the cell voltages and the configured thresholds).

#### Balancing algorithim

The balancing algorithm is a simple threshold-based approach:
- If the minimum cell voltage in the pack exceeds `MIN_V_FOR_BALANCE` (defined in ABOX)
- And the difference between a cell voltage and the minimum cell voltage is greater than `MIN_DELTA_FOR_BALANCE` (also defined in ABOX)
- Then the discharge (balancing) for that cell will be enabled by setting the appropriate bit in the REGB register of the module that contains that cell.
- Otherwise, discharge for that cell will be disabled.

## File Structure

- `adbms.c` / `adbms.h`: High-level driver logic and state management.
- `adbms6380.c` / `adbms6380.h`: Low-level chip-specific functions and command encoding.
- `commands.c` / `commands.h`: Command definitions for ADBMS6380.
- `pec.c` / `pec.h`: PEC (CRC) calculation utilities.

## Notes

- The driver is designed for FreeRTOS-based systems with a 1000Hz tick rate (1ms tick period).
  - Its primary reliance on FreeRTOS comes from the use of `RTOS_delay_ms(ADBMS6380_WAKE_DELAY_MS)` in the wake-up sequence.
- All communication is performed via SPI, with manual CS control for timing.
- Error handling is robust, with flags for SPI, PEC, and register mismatches.
- Make sure the `ADBMS_MODULE_COUNT` constant matches the actual number of modules in your daisy chain configuration!!
- There is a hidden reliance on a global variable defined in `../thermistor/thermistor.c` (and `extern`ed in the header): `thermistor_lut`

## Authors

- Primary: Millan Kumar
- Supporting:
	- Irving Wang
	- Ronak Jain
	- Shriya Balu
	- Sebastian Arthur

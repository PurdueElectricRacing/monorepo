# abox isoSPI filtering bench pair

`abox_isospi_target` runs a local copy of the production abox ADBMS driver:
seven modules, SPI mode 0, nominal 500 kHz (the existing PHAL clock divider),
200 ms task period, and the original wake pulses, configuration, measurement,
and retry/recovery sequences. Balancing stays disabled. Only the BMS task and
a PB5 heartbeat toggling every 100 ms run. PB9 lights on a hard fault.

`abox_isospi_slave` emulates the entire seven-module chain on a second abox.
It handles configuration A/B writes and readback, six cell groups, four auxiliary
groups, and the ADCV/ADAX commands used by the copied driver. Configuration writes
are reversed into read order and committed only after a complete valid chain
write. Responses contain six data bytes and a valid PEC10 per module, with a
fixed zero command counter. Command PEC15 and write PEC10 are checked.

Cell readings start at 3.70005 V, increasing by 3 mV per module and 0.45 mV per
cell. Auxiliary readings start at 1.5 V with the same increments (roughly room
temperature through the copied thermistor conversion). Unused group slots are
zero. This is a traffic emulator for these commands, not a full chip model.

## Hardware

Use an isoSPI bridge configured to **receive isoSPI and drive SPI clock/CS** on
the slave board. The bridge part/strapping is not described in this repository;
verify that setup and the bridge's signal directions before connecting the pair.
A stock master-configured bridge cannot be turned into a slave by MCU firmware.

| MCU pin | Master board | Slave board |
| --- | --- | --- |
| PA5 / SPI1 SCK | Clock output | Clock input from bridge |
| PA7 / SPI1 MOSI | Command output | Command input from bridge |
| PA6 / SPI1 MISO | Response input | Response output to bridge |
| PB0 | Active-low CS output | Active-low CS input from bridge |
| PB5 | Heartbeat LED | Heartbeat LED |
| PB9 | Hard-fault LED | Hard-fault LED |

Keep bridge SPI mode at mode 0, 8-bit, MSB first. Wire according to signal
**direction**, since bridge pin names may be relative to its master mode.
Connect the two bridges through the isoSPI path/filter under test. The slave
uses PB0 EXTI and software NSS; no extra hardware-NSS jumper is required.
Power the slave first, with CS idle high, then start the master. Direct logic-level
SPI testing uses the same signals and a common ground, with bridges disconnected
or otherwise prevented from driving those lines.

The slave preloads four command dummy bytes while deselected, selects SPI in
the CS interrupt, and fills the response FIFO after command PEC validation.
Both transport interrupts run above the RTOS syscall priority and make no RTOS
calls. Only the slave target is compiled with `-O2` to bound interrupt latency.
The master's command-to-read gap is unchanged. Confirm CS-to-clock timing and
first-response-byte alignment on hardware; host tests cannot validate them.

## Build and flash

From the repository root, with the normal firmware toolchain/dependencies:

```sh
cmake -S firmware -B /tmp/abox-isospi-build -G Ninja
cmake --build /tmp/abox-isospi-build --target abox_isospi_target.elf abox_isospi_slave.elf a_box.elf -j 4
```

Artifacts are in `firmware/output/abox_isospi_target/` and
`firmware/output/abox_isospi_slave/` (`.elf`, `.hex`, `.bin`, and map files).
These targets always use standalone flash layout at `0x08000000`, including
when `BOOTLOADER_BUILD` is enabled. Flashing them replaces any resident bootloader.
With only the intended board's ST-Link connected, flash its image:

```sh
openocd -f interface/stlink.cfg -f target/stm32g4x.cfg -c "program firmware/output/abox_isospi_target/abox_isospi_target.elf verify reset exit"
# Switch to the slave board's ST-Link:
openocd -f interface/stlink.cfg -f target/stm32g4x.cfg -c "program firmware/output/abox_isospi_slave/abox_isospi_slave.elf verify reset exit"
```

## Checks

Run the portable protocol tests (no board required):

```sh
gcc -std=c23 -Wall -Wextra -Werror -Wconversion \
  -I firmware/source/abox_isospi_slave/protocol \
  firmware/source/abox_isospi_slave/tests/test_emulator.c \
  firmware/source/abox_isospi_slave/protocol/emulator.c \
  firmware/source/abox_isospi_slave/protocol/commands.c \
  firmware/source/abox_isospi_slave/protocol/pec.c \
  -o /tmp/test-isospi-emulator
/tmp/test-isospi-emulator
```

Tests cover every read group, full 56-byte replies, independent bitwise PEC
references, configuration ordering, invalid PECs/commands, wake pulses, truncated
headers/reads/writes, conversion commands, and subsequent recovery.

On the bench:

1. Confirm both heartbeat LEDs toggle. Inspect master `g_bms`: it should remain
   `ADBMS_STATE_CONNECTED`, with the expected cell/thermistor data and clear PEC
   and configuration-mismatch flags.
2. Inspect slave `g_emulator.stats`: `commands` should grow; `invalid_pec`,
   `unsupported`, `incomplete`, and `peripheral_errors` should remain zero.
   Clockless wake pulses do not count as incomplete frames. CS deassertion
   resets protocol state and peripheral FIFOs after incomplete/error frames.
3. Capture SPI and isoSPI. Check the four-byte command followed immediately by
   56 response bytes in the same CS assertion, correct first-byte alignment,
   all seven modules, wake pulses, and the 200 ms steady-state polling period.
4. Repeat identical captures with each filtering configuration. Use debugger
   error flags/counters alongside waveforms; avoid halting a live transaction,
   which itself changes timing and may create errors.

Hardware response timing and filter performance must be verified on the bench.

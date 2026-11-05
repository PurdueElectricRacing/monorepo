# Bench-testing the 170 MHz clock on G4 boards

A step-by-step guide for people who have never flashed a board or done
hardware testing before. It covers:

0. Flashing a G4 board and reading results from it (do this first)
1. One 170 MHz board talking to **itself** over SPI
2. A 170 MHz board talking to a **16 MHz** board over SPI
3. The same two boards talking over **UART** (USART)

Each test reports its result in variables that you read live from VS Code
while the board runs. Nothing needs a scope or a logic analyzer.

---

## What you need

| Item | How many | Notes |
|---|---|---|
| G4 board: NUCLEO-G474RE (recommended) or the PER G4 dev board | 2 (1 for test 1) | A Nucleo has an ST-LINK debugger built in. A dev board needs a separate ST-LINK. |
| USB cable for each board | 2 | Must be a *data* cable, not a charge-only one. |
| Female-to-female jumper wires | ~6 | The Nucleo has male header pins. |
| A spare USB power source (phone charger or power bank) | optional | Makes working with two boards easier (see "Two boards, one computer"). |

You also need a working build. If `python3 build_all.py` (or "build all" in
VS Code) passes, you're set.

> [!IMPORTANT]
> Every wire in these tests carries 3.3 V logic. Never connect a pin to 5 V,
> and never connect two output pins together (for example TX to TX).

---

## Part 0: Flash a board and read results from it

### 0.1 Words you'll see

- **Flashing**: copying the compiled program (an `.elf` file) into the chip's
  flash memory. The program stays there after power-off.
- **ST-LINK**: the USB debugger that flashes the chip and lets you watch its
  memory while it runs. On a Nucleo it's the top part of the board, the end
  the USB cable plugs into.
- **Debug session**: VS Code connected to the chip through the ST-LINK. You
  can pause the program, step through it, and watch variables.
- **Live Watch**: a VS Code panel that re-reads chosen global variables about
  5 times a second *while the program keeps running*. This is how you read
  the test results.

### 0.2 Start VS Code so the build tools are found

The flash button first rebuilds the firmware, so VS Code needs the build
tools on its `PATH`. Open a terminal in the repo root and run:

```sh
source .venv/bin/activate
export ARM_TOOLCHAIN="$HOME/Downloads/arm-gnu-toolchain-15.3.rel1-x86_64-arm-none-eabi"
export PATH="$ARM_TOOLCHAIN/bin:$PATH"
code .
```

(Adjust the toolchain path to wherever yours is installed.) Install the
recommended extensions if VS Code asks, especially **Cortex-Debug**.

### 0.3 Pick which test gets built

`g4_testing` builds **one** test at a time, chosen by a single line in
[g4_testing.h](g4_testing.h):

```c
#define G4_TESTING_CHOSEN TEST_BLINKY
```

Change `TEST_BLINKY` to the test you want (`TEST_RCC`, `TEST_SPI`,
`TEST_PEER`, ...) and save. **Don't commit this change.** Set it back to
`TEST_BLINKY` when you're done.

### 0.4 Flash

1. Plug **one** board into your computer by USB. On a Nucleo, the red/green
   COM LED near the USB connector lights up.
2. In VS Code, open **Run and Debug** (`Ctrl+Shift+D`).
3. In the dropdown at the top, choose **`g4_testing`**.
4. Press the green ▶ button.
5. Wait. The firmware rebuilds first (this takes a minute or two), then the
   flash is written. When it's done, the editor jumps to `main()` and stops
   there with a yellow highlight. The program is now flashed and paused.
6. Press ▶ (Continue, `F5`) in the small debug toolbar to let it run.

If it fails, see [Troubleshooting](#troubleshooting).

### 0.5 Read results with Live Watch

1. In the Run and Debug sidebar, find the **LIVE WATCH** section (Cortex-Debug
   adds it).
2. Click **+** and type a global variable's name, for example `peer_results`.
   Press Enter.
3. Click the arrow next to it to expand its fields. The values update while
   the board runs.

Watch expressions (the normal **WATCH** panel) work too, but only refresh
while the program is paused.

### 0.6 Stop

Press the red ■ (Stop). The program stays in flash, so it runs again on its
own every time the board is powered or you press the black **RESET** button.

### 0.7 First flash: check the 170 MHz clock itself (`TEST_RCC`)

Do this once on **each** board before the other tests. It proves the clock
setup works on that board.

1. Set `G4_TESTING_CHOSEN` to `TEST_RCC`. No wires are needed.
2. Flash and run as in 0.4.
3. In Live Watch, add `rcc_failed_step`.

| What you see | Meaning |
|---|---|
| `rcc_failed_step = 4294967295` (that's `0xFFFFFFFF`) | **Pass.** The test switched 16 → 170 → 170 → 16 → 170 MHz and every register check passed. |
| Any other number | **Fail** at that step (0-4). Also add `rcc_failed_check`, `rcc_failed_expected` and `rcc_failed_actual`. `rcc_failed_check` names the check that failed (see `rcc_check_id_t` in [rcc_test.c](rcc_test.c)). |
| Values never change and the program seems stuck | Pause (⏸) and look at the call stack. If it's inside an RCC function, the clock setup is hanging. Report which function. |

On the PER dev board the green LED means pass and the red LED means fail. A
Nucleo has no LEDs on those pins, so use Live Watch.

---

## Pin cheat sheet (NUCLEO-G474RE)

The tests use these chip pins. Where a pin is on the Arduino-style headers,
its Arduino label is shown too. Pins marked *morpho* are only on the two long
double-row headers. Always double-check against the pinout card printed in
the Nucleo box, or the NUCLEO-G474RE user manual (UM2505), before wiring.

| Chip pin | Arduino label | Used for |
|---|---|---|
| PA5 | D13 | SPI1 SCK (clock) |
| PA6 | D12 | SPI1 MISO |
| PA7 | D11 | SPI1 MOSI |
| PA4 | A2 | SPI1 chip select |
| PB13 | *morpho* | SPI2 SCK |
| PB14 | *morpho* | SPI2 MISO |
| PB15 | *morpho* | SPI2 MOSI |
| PB12 | *morpho* | SPI2 chip select (NSS) |
| PA9 | D8 | USART1 TX (transmit) |
| PA10 | D2 | USART1 RX (receive) |
| GND | GND | Ground. There are several; any of them works. |

> [!NOTE]
> On the Nucleo, PA5 also drives the green user LED (LD2). It will flicker
> during SPI tests. That's normal.

---

## Test 1: One 170 MHz board talking to itself over SPI (`TEST_SPI`)

### What it proves

On one board running at 170 MHz, SPI1 acts as **master** and drives the clock,
and SPI2 acts as **slave** and listens. Each round they exchange 16 bytes in
both directions and check them. The test runs 1000 rounds, 100 ms apart
(about 100 seconds). This checks that SPI, its DMA transfers and its timing
all work at 170 MHz.

Because both ends are on the same chip, this can't detect a wrong *bit rate*.
Tests 2 and 3 cover that.

### Wiring (one board, 4 wires)

| From (SPI1, master) | To (SPI2, slave) | Signal |
|---|---|---|
| PA5 (D13) | PB13 | Clock: master → slave |
| PA7 (D11) | PB15 | MOSI: master out → slave in |
| PA6 (D12) | PB14 | MISO: master in ← slave out |
| PA4 (A2) | PB12 | Chip select: master → slave |

No ground wire is needed, because both ends share the same chip.

### Run it

1. Set `G4_TESTING_CHOSEN` to `TEST_SPI`.
2. Flash and run (0.4).
3. In Live Watch, add `pass_count`, `fail_count` and `iteration`.

### Pass/fail

| What you see | Meaning |
|---|---|
| `iteration` climbs to 1000, `pass_count` = 1000, `fail_count` = 0 | **Pass.** |
| `fail_count` climbing | Data is getting corrupted. Re-check that MOSI goes to MOSI and MISO to MISO. They are *not* crossed for SPI. |
| `iteration` stuck at a small number | A transfer never finished. The clock or chip-select wire is probably missing or on the wrong pin. Pause and check the call stack. |

---

## Two boards, one computer

Tests 2 and 3 need two boards running at the same time, but VS Code debugs one
at a time. The simplest way:

1. **Flash board A**: plug in only A, flash it (0.4), then Stop (0.6).
2. **Unplug A from the computer and power it from a phone charger or power
   bank.** It keeps running the program you flashed.
3. **Plug in board B**, flash it, and leave its debug session running. Use
   Live Watch to read B's results.
4. To read A's results, swap roles: power B from the charger and debug A.
   Debugging A **resets** A, so start a fresh debug session on A and wait a few
   seconds for new numbers.

Each board has its own USB power. That's fine, *as long as the GND wire
between the boards is connected*.

> [!TIP]
> Advanced: you can keep both plugged into the computer by telling VS Code
> which ST-LINK to use. Copy the `g4_testing` entry in `.vscode/launch.json`,
> give the copy a new `name`, and add `"serialNumber": "<ST-LINK serial>"` to
> each entry. You can get the serial numbers with `st-info --probe`.

---

## The peer test (`TEST_PEER`), used by tests 2 and 3

Both boards run the same file, [peer_test.c](peer_test.c), built twice with
different settings. The settings are at the top of the file:

```c
// ---- Per-board configuration: change these for each Nucleo ----
static constexpr uint32_t PEER_NODE_ID            = 1U;
static constexpr PHAL_RCC_Mode_t PEER_RCC_MODE    = PHAL_RCC_HSI_170MHZ;
static constexpr uint32_t PEER_SPI_MASTER_NODE_ID = 1U;
static constexpr PHAL_FDCAN_BaudRate_t PEER_CAN_BAUD = FDCAN_BAUD_500K;
static constexpr bool PEER_UART_ENABLED = true;
static constexpr bool PEER_SPI_ENABLED  = true;
static constexpr bool PEER_CAN_ENABLED  = false;
```

| Setting | Board A (the 16 MHz reference) | Board B (170 MHz, under test) |
|---|---|---|
| `PEER_NODE_ID` | `0U` | `1U` |
| `PEER_RCC_MODE` | `PHAL_RCC_HSI_16MHZ` | `PHAL_RCC_HSI_170MHZ` |
| `PEER_SPI_MASTER_NODE_ID` | **same on both**: which board drives the SPI clock | same |
| `PEER_*_ENABLED` | **same on both**: which links to test | same |

So the procedure is: set A's values, flash A, set B's values, flash B. Set
`G4_TESTING_CHOSEN` to `TEST_PEER` first. **Don't commit these edits.**

Every 100 ms each board sends a numbered message on each enabled link and
checks the messages it receives from the other board. The results are in one
variable, **`peer_results`**. Add it to Live Watch and expand it.

---

## Test 2: 170 MHz ↔ 16 MHz over SPI

### What it proves

Board B, at 170 MHz, exchanges SPI data with board A, at 16 MHz, in both
directions: once with B as master and once with A as master. This proves each
clock speed works as SPI master and as SPI slave against a board running the
*other* speed.

SPI is *synchronous*: the master sends its own clock on SCK and the slave
follows it, so a wrong SPI speed does **not** corrupt the data. This test
proves the boards interoperate; it does not measure the SCK frequency. Only a
scope, a logic analyzer, or the real ADBMS6380 on a_box can measure that.

### Settings (both boards)

```c
static constexpr bool PEER_UART_ENABLED = false;
static constexpr bool PEER_SPI_ENABLED  = true;
static constexpr bool PEER_CAN_ENABLED  = false;
```

Run 2a with `PEER_SPI_MASTER_NODE_ID = 1U` (B, at 170 MHz, is master). Then
run 2b with `PEER_SPI_MASTER_NODE_ID = 0U` (A, at 16 MHz, is master).
Reflash **both** boards between 2a and 2b.

### Wiring (two boards, 5 wires, straight through)

| Board A | Board B |
|---|---|
| PA5 (D13) | PA5 (D13) |
| PA6 (D12) | PA6 (D12) |
| PA7 (D11) | PA7 (D11) |
| PA4 (A2) | PA4 (A2) |
| GND | GND |

### Order matters

The slave has to be listening before the master starts talking. Power up or
reset the **slave first**, then the master. If you get it backwards, press
RESET on the master afterwards. The test ignores errors that happen before
the first good exchange.

### Pass/fail (check both boards)

| `peer_results` field | Pass |
|---|---|
| `spi_good` | Climbs by about 10 per second |
| `spi_bad` | Stays `0` |

If `spi_good` stays at 0: check the 5 wires, especially GND and chip select
(PA4), and make sure only one board is configured as master. If `spi_bad`
climbs, look for a loose wire, or swap MOSI and MISO back if you crossed them.
They go straight through.

---

## Test 3: 170 MHz ↔ 16 MHz over UART

### What it proves

This is the **strongest** of the three tests. UART has no clock wire: each
side times every bit using *its own* clock. If board B's 170 MHz setup is even
a few percent off, the bytes it sends are corrupted on arrival at A, and vice
versa.

The test also measures the time between A's messages using B's FreeRTOS
clock, and the other way round. A sends one message every 100 ms by *its*
clock, so if both clocks are right, each board measures about 100 ms. This
catches clock errors down to about 1%.

### Settings (both boards)

```c
static constexpr bool PEER_UART_ENABLED = true;
static constexpr bool PEER_SPI_ENABLED  = false;
static constexpr bool PEER_CAN_ENABLED  = false;
```

(`PEER_SPI_MASTER_NODE_ID` doesn't matter here.)

### Wiring (two boards, 3 wires, TX and RX **crossed**)

| Board A | Board B |
|---|---|
| PA9 (D8) TX | PA10 (D2) RX |
| PA10 (D2) RX | PA9 (D8) TX |
| GND | GND |

Each board's TX (transmit) goes to the other board's RX (receive). Never
connect TX to TX.

Start order doesn't matter for UART.

### Pass/fail (check both boards)

| `peer_results` field | Pass |
|---|---|
| `uart.good` | Climbs by about 10 per second |
| `uart.bad` | Stays `0` |
| `uart_rx_interval_avg_ms` | `99`-`101` after ~10 seconds |

| Symptom | Likely cause |
|---|---|
| `uart.good` stuck at 0 on one board | That board's RX wire is missing or connected to TX. Check the crossing. |
| `uart.bad` climbing | Bytes are arriving corrupted. With good wiring and GND, this means the two boards disagree about baud rate, which is a **real clock bug**. Record it. |
| `uart_rx_interval_avg_ms` around 106, or 94 | One board's clock is about 6% off. |
| `uart_rx_interval_avg_ms` around 1000+ or 10 | One board's FreeRTOS tick is badly wrong (for example, a stale clock-rate value). Record which board. |

---

## Optional: CAN

`PEER_CAN_ENABLED = true` also tests CAN between the two boards, at
`PEER_CAN_BAUD`. Each board needs a 3.3 V CAN transceiver breakout wired to
PB13 (TX) and PB12 (RX). Connect the two transceivers' CANH to CANH and CANL
to CANL, with a 120 Ω resistor between CANH and CANL at each end. Pass:
`can.good` climbs about 10 per second, and `can.bad`, `can_tx_dropped`,
`can_max_tec` and `can_max_rec` all stay 0. Without transceivers, leave CAN
off: it would only report errors.

---

## Troubleshooting

| Problem | Try |
|---|---|
| The ▶ button fails before flashing, with build errors | Run `python3 firmware/build_firmware.py` in the terminal from 0.2 to see the real error. Usually VS Code wasn't started from that terminal, so it can't find the tools. |
| `Error: open failed` / `unable to find a matching CMSIS-DAP device` / no ST-LINK found | Use a data USB cable, try another port, and close any other debug session. On Linux or WSL the ST-LINK may need USB permissions (see `docs/setup.md` 3C.4). |
| `target not halted` / `couldn't connect` | Hold the board's RESET button, press ▶, and release RESET when "Connecting" appears. |
| Live Watch shows nothing | Live Watch only shows **global** variables, and only while a debug session is running. Check the spelling of the name. |
| The program is stuck in `HardFault_Handler` (pause to see) | A peripheral failed to start. Look at the call stack to see which `..._init` call led there. |
| The numbers stopped changing | The board may have reset or crashed. Pause and check where it is. |
| Things worked, then failed after reflashing | You may have forgotten to change the per-board settings (node ID, clock) for that board. |

When you're done, set `G4_TESTING_CHOSEN` back to `TEST_BLINKY` and discard the
`peer_test.c` setting changes (`git checkout firmware/source/g4_testing/`).

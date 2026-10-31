# PHAL F4 audit for STM32F423RHT6

Reviewed on 2026-10-23 at repository revision `6e4001ae7`. This supplements the
[migration checklist](f423_migration_checklist.md). Every `.c` and `.h` under
`firmware/common/phal_F4/` was inspected against the local
[RM0430 Rev 9](datasheets/STM32F423_RM0430.pdf) and checked-in F423 CMSIS header.
Package/AF claims use [DS11580 Rev 8](https://www.st.com/resource/en/datasheet/stm32f423ch.pdf).
Errata references use [ES0372 Rev 8](https://www.st.com/resource/en/errata_sheet/es0372-stm32f413xgxh-and-stm32f423xh-device-errata-stmicroelectronics.pdf).
No firmware implementation was changed for this audit. The later user-supplied
board image is reflected in checklist D/F; every finding below is mapped to
a concrete fix and verification step in checklist K/L. Baseline statements about
the current two-bus DAQ source remain historical audit observations; the supplied
board requires the CAN3 and protocol integration described in the plan.

“Compatible” means the register interface used by the driver exists on F423.
It does not mean the driver is correct, complete, electrically validated, or
exercised on hardware. Existing defects are separated from F423 changes so
that a port does not carry them forward as verified code.

## Implementation follow-up (2026-10-30)

The audit below describes its original baseline. Project-owned device selection
has since moved to F423; both F4 targets and all eleven PHAL modules compile/link
in the full build. The compile blockers, flash geometry and obsolete support
artifacts were addressed in the checklist's cleanup step; runtime findings remain
open unless explicitly checked off there.

Full-build validation also exposed a missed SDIO difference: bit 9 is reserved in
F423 SDIO STA/ICR/MASK (RM0430 §31.8.11–13); there is no STBITERR event. The driver
now omits that interrupt/status/clear handling. Existing CRC, timeout, underrun
and overrun handling remains. This compile fix does not qualify SDIO timing,
flow control or errata behavior.

## Compile evidence (historical baseline)

ARM GCC 14.3.1, `-std=gnu23 -mcpu=cortex-m4 -mfpu=fpv4-sp-d16
-mfloat-abi=hard -fsyntax-only`, with the repository CMSIS Core/device includes:

| Probe | Result |
| --- | --- |
| Current PHAL with `STM32F407xx` | All 11 translation units pass. |
| Temporary copy with F407 guards/device-header names changed to F423 | 9/11 pass; ADC and GPIO fail as described below. |
| Current `system_stm32f4xx.c` with `STM32F423xx` | Passes without source changes. |

The temporary copy was under `/tmp/f423-phal-audit`; repository firmware was
unchanged. Only `STM32F407xx` and `stm32f407xx.h` were substituted. The flash
map and RCC limits deliberately remained F407 values: passing syntax checks
**does not validate those settings**. The probe isolates extra problems after
device-selection work. Unexpanded configuration macros and linker/IRQ collisions
need additional checks.

## RCC — incompatible policy and existing sequencing defects

Files: `rcc/rcc.h`, `rcc/rcc.c`. Evidence: RM0430 §3.4.1/Table 6, §5.4.1,
§6.2/§6.3, especially `RCC_PLLCFGR`, `RCC_CFGR`, `RCC_DCKCFGR2`.

- **F423 change:** the F407-only SYSCLK cap is 168 MHz. F423 allows 100 MHz
  HCLK, 50 MHz PCLK1, 100 MHz PCLK2. DAQ requests 168 MHz from a 336 MHz VCO.
  The old plan's missing 170 MHz CMSIS path is unrelated to the actual source.
- **F423 change:** VOS is two bits: scale 1 (`11`) up to 100 MHz, scale 2
  (`10`, reset) up to 84 MHz, scale 3 (`01`) up to 64 MHz. PHAL never selects
  VOS or waits for regulator readiness. Use the documented PLL off/on sequence
  and regulator-ready check before running above the selected scale's limit.
- **F423 change:** F407-style flash thresholds are wrong for F423. At 2.7–3.6 V,
  F423 permits 25/50/75/100 MHz with 0/1/2/3 wait states. Current code chooses
  2 WS at 80 MHz, where 3 are required. Use actual HCLK and supply range,
  verify latency readback, and order changes safely.
- **Reusable:** PLLM 2–63, PLLN 50–432, VCO input 1–2 MHz, VCO output
  100–432 MHz, PLLP 2/4/6/8 and PLLQ 2–15 remain valid. SYSCLK still uses
  PLLP; PLLR is an I2S/DFSDM output, not a replacement for main SYSCLK decoding.
- **Existing:** clock setup switches to the fast PLL before configuring bus
  dividers, potentially overclocking APB1 during transition. It proceeds after
  intermediate errors. Set safe dividers before increasing SYSCLK.
- **Existing:** the HSI switch ORs a zero-valued SW selection and tests a
  zero-valued SWS mask, so it neither selects nor waits for HSI correctly when
  changing sources. HSE selection also ORs SW without clearing it. Both lower
  flash latency before establishing the slower clock. Use masked SW writes,
  full SWS comparisons, and safe latency ordering.
- **Existing:** the PLL search uses `uint8_t` for a multiplier permitted up to
  432 and does not reset it for each input divisor. An unsuccessful search can
  wrap indefinitely. Use a wide counter, reset it per divisor, and require exact
  integer ratios instead of truncation.
- **Existing:** PLLQ uses floor division by 48 MHz. For example, a 200 MHz VCO
  selects Q=4, yielding 50 MHz, exceeding the SDIO/RNG PLLQ limit in the register
  description. Select a valid kernel clock/divider explicitly; USB requires
  48 MHz. F423 also has SDIO/CK48 selectors in `RCC_DCKCFGR2`.
- **Existing:** AHB reconfiguration does not call `SystemCoreClockUpdate()`;
  changing HPRE leaves the core-rate cache/RTOS tick wrong. Prescaler helpers
  need zero/range/exact-divisibility checks and timeouts.
- **Board-dependent:** PHAL and CMake assume 16 MHz HSE; PHAL always enables
  bypass. A crystal and an externally driven clock need different HSEBYP
  settings. Confirm the new board's source rather than assuming either.

## GPIO — confirmed compile blockers and package constraints

Files: `gpio/gpio.h`, `gpio/gpio.c`. Evidence: RM0430 §7, particularly
§7.4.1 (`GPIOx_MODER`, ports A–H); DS11580 Figure 13, Tables 10/12.

- **F423 compile blockers:** `GPIOI` / `RCC_AHB1ENR_GPIOIEN` do not exist.
  The used `GPIO_MODER_MODE0_Msk` / `GPIO_MODER_MODE1_Pos` aliases are also
  absent; use `GPIO_MODER_MODER0_Msk` / `GPIO_MODER_MODER1_Pos`.
- **Reusable:** MODER, OTYPER, OSPEEDR, PUPDR, IDR, BSRR and AFR layouts used
  here agree with F423. Family-wide E/F/G definitions in a header do not make
  those pins available on the RH LQFP64 package.
- **F423 port:** extend F407-guarded pin macros, validating each against the
  package. CAN1 PD0/PD1 and USART2 PD5/PD6 cannot be used on RHT6. The checklist
  enumerates the current DAQ CAN/SPI1/SDIO/GPIO pin assignments.
- **Existing:** `GPIO_INIT_SPI2_MISO_PB4` is misnamed: PB4 AF5 is **SPI1_MISO**
  on F423. Numeric speed comments, including “180 MHz,” are not electrical
  guarantees. Use datasheet load/voltage specifications. Pin numbers and
  per-package availability are not validated by this driver.

## ADC — compile blocker despite being unused by DAQ

Files: `adc/adc.h`, `adc/adc.c`. Evidence: RM0430 §13, especially §13.12.16
`ADC_CCR`; F423 header `ADC1_COMMON` / `RCC_APB2ENR_ADC1EN`.

- **F423 compile blocker:** `ADC123_COMMON` is absent; use `ADC1_COMMON`.
- **F423 change:** only ADC1 exists. Current `adc_number` arithmetic can select
  reserved RCC bits for ADC2/3. Reject unsupported instances/numbers and validate
  channel, acquisition-time, ADC-clock and conversion-rate limits.
- **Reusable:** regular-sequence, resolution, sample-time, CR1/CR2 and data
  interfaces exist. ADC1 DMA2 stream0/channel0 remains valid (Table 31).
- **Existing:** check nonzero sequence length and channel/rank bounds. Reinit
  does not consistently clear prior scan/DDS state. Internal sensor conversions
  need the documented stabilization time after TSVREFE is enabled.

## SPI — same DFF interface, current initialization defects

Files: `spi/spi.h`, `spi/spi.c`; DAQ `main.c` and
`ethernet/w5500_callbacks.c`. Evidence: RM0430 §29, especially §29.7.1
(`DFF`), §29.7.2 (`CR2`), §9.3.3/Table 31.

- **Reusable:** F423 uses **8/16-bit `SPI_CR1.DFF`**. It does not require a
  G4-style DS/FRXTH rewrite. Extend the F407 guard so frame-size setup still runs.
- **Existing:** init sets SPE before writing DFF; RM0430 requires SPE=0 for
  DFF changes. Configure before enabling the peripheral.
- **Existing DAQ blocker:** `eth_spi_config.data_rate` is omitted, defaulting
  to zero; `PHAL_SPI_init()` divides by it. Provide a valid nonzero speed and
  check the new PCLK2 divider/limit. Current floor/log arithmetic can also select
  a speed faster than requested.
- **Reusable:** SPI1 RX = DMA2 stream2/channel3; SPI1 TX = DMA2 stream3/channel3.
  SPI2 stream3 RX / stream4 TX channel0 routes remain valid too.
- **Existing limits:** only SPI1/2 are accepted. DMA completion enables/implements
  only the SPI1 TX IRQ, assumes non-null RX DMA configuration, and uses `DMEIF4`
  for stream7 instead of `DMEIF7`. Resolve before claiming generic SPI DMA support.
- **Actual DAQ path:** DMA configs are initialized, but W5500 registers custom
  **polling** callbacks accessing DR/SR and controlling CS. Verify these as well
  as PHAL init. The custom path offsets `rx_data` by `txlen` even for writes
  passing NULL; remove that undefined pointer arithmetic.

## DMA — compatible engine, device-specific routes

Files: `dma/dma.h`, `dma/dma.c`. Evidence: RM0430 §9, Tables 30/31,
`DMA_SxCR`, `DMA_SxNDTR`, `DMA_LIFCR/HIFCR`.

- **Reusable:** stream engine, CR/NDTR/PAR/M0AR, flag positions and RCC interface
  used here exist. F423 CHSEL is **four bits**; CMSIS handles this, and
  `uint8_t dma_chan_request` can represent channel 8/9.
- **Existing:** `while (init->stream->CR &= DMA_SxCR_EN)` modifies CR while
  polling. Use a read-only EN check and wait for disable acknowledgement before
  changing address/count/config. `PHAL_DMA_stopTxfer()` also does not wait before
  callers may reconfigure streams.
- **Existing:** validate controller/stream/index consistency, request number,
  alignment and transfer count (up to 65535). `mem_to_mem` is not encoded in DIR,
  and DIR>1 is rejected, so advertised memory-to-memory mode is unimplemented.
  The `0x2D` flag-clear mask omits HTIF (`0x10`); define intended flags explicitly
  if half-transfer operation is supported.

## USART — reusable registers, changed UART5 TX route

Files: `usart/usart.h`, `usart/usart.c`. Evidence: RM0430 §28 and §9/Table 30.

- **F423 port:** extend the F407 branch for clock enables, DR/SR accesses,
  interrupts, constants and config macros. F423 retains SR/DR/BRR/CR1/CR2/CR3.
  Current fallback macros use RDR/TDR, which are not the F423 interface.
- **F423 route change:** `USART5_TXDMA_CONT_CONFIG` selects DMA1 stream7,
  **channel4**. F423 UART5_TX uses **channel8**. Other implemented USART1/2/3,
  UART4, UART5_RX and USART6 DMA tuples match Tables 30/31.
- **Scope:** `TOTAL_NUM_UART=8` does not imply eight working peripherals: switches
  implement USART1/2/3/6 and UART4/5; UART7/8 are partly declared. F423 has ten
  instances; adding UART7–10 is optional for DAQ.
- **Existing:** USART6 DMA2 stream6 and USART6 IRQ wrappers are commented out.
  SDIO owns DMA2 stream6; enabling that TX route needs resource checks. Several
  USART DMA handlers also conflict with SPI2 assignments.
- **Existing:** stream7 error handling uses `DMEIF4`, not `DMEIF7`; baud and
  active-handle inputs need bounds checks. Test DMA RX startup/IDLE handling
  under real traffic rather than inferring correctness from register compatibility.
- **Existing:** `SB_TWO=01` and `SB_HALF=10` are reversed. F423 CR2.STOP encodes
  `01` as 0.5 stop bit and `10` as two stop bits. Hardware flow-control options
  are declared but `PHAL_initUSART()` does not program CTSE/RTSE.

## Flash — extend capacity and fix fallback

Files: `flash/flash.h`, `flash/flash.c`. Evidence: RM0430 §3.3/Table 5,
§3.5, `FLASH_CR` / `FLASH_SR`.

- **F423 change:** 16 sectors, `16K×4 + 64K + 128K×11`, ending at
  `0x0817FFFF`. Sectors 0–11 match F407; add 12–15 and MAX=15. CMSIS `FLASH_END`
  is correct already; PHAL's `USER_FLASH_END_ADDRESS` and lookup encode F407.
- **F423 port:** non-F407 `flashGetSector()` unconditionally assigns sector7
  after its comparisons; this is not a final `else`. Merely enabling a new header
  branch leaves an incorrect erase lookup.
- **Reusable:** unlock keys, PG/SER/SNB/STRT and PSIZE mechanisms match F423.
  Word programming with PSIZE=32 requires the appropriate supply range; current
  fixed width is not valid for every supported MCU voltage.
- **Existing:** validate write bounds/alignment/count overflow, wait/check status
  per operation, handle/clear error flags, preserve cache consistency, and clean
  up PG/SER/LOCK on every exit. Buffered writes wait only at the end and return
  no programming error status; several failure returns leave flash unlocked or
  PG set. U64 writing is two word operations, not atomic 64-bit programming.

## RTC — compatible registers, wrong prescaler setup

Files: `rtc/rtc.h`, `rtc/rtc.c`. Evidence: RM0430 §25, initialization/shadow
register rules and §25.6.5 `RTC_PRER` (pp. 763–764).

- **F423 port:** both F407-only DBP branches can use F423 `PWR_CR_DBP`.
  RCC backup-domain, WPR, ISR, PRER, TR and DR interfaces used here exist.
- **Existing:** header says LSE but code selects **LSI**. `PREDIV_A` is only
  seven bits; `RTC_ASYNC_PRESCAL=255` cannot be programmed as stated. PRER
  writes OR without clearing fields: from reset `0x007F00FF`, sync 124 leaves 255
  in place. For nominal 32 kHz, one valid pair is A=127, S=249; LSI still needs
  tolerance/calibration handling.
- **Existing:** `PHAL_getTimeRTC()` rereads TR/DR for individual fields instead
  of taking a consistent calendar snapshot. It returns decimal values through
  fields named `_bcd`; DAQ filename formatting currently uses them as decimal.
  Document that contract rather than silently changing it.
- **Existing:** changing retained RTCSEL requires backup-domain reset; code
  comments out reset and does not resolve a retained different source. Check
  synchronization, WPR relocking, retained state and GPS forced-time updates.

## PWM — compatible timers, wrong clock assumptions

Files: `pwm/pwm.h`, `pwm/pwm.c`. Evidence: RM0430 timer chapters and §6.2,
`RCC_DCKCFGR.TIMPRE`.

- **Reusable:** referenced TIM1–5 and TIM8–14 exist. Excluding TIM6/7 from PWM
  is correct. Package AF/channel availability still varies.
- **Existing:** PSC uses PCLK directly. With TIMPRE=0 and an APB divider greater
  than 1, timer clocks are **2×PCLK**, making PWM twice the requested frequency.
  F423's TIMPRE alternative matters if enabled.
- **Existing:** accepts four channels on every timer and writes BDTR for all.
  Respect channel counts and use BDTR/MOE only where implemented. Check frequency/
  PSC bounds and generate an update event to load preload state deterministically.

## CAN — compatible CAN1/2 design, clock-dependent timing

Files: `can/can.h`, `can/can.c`. Evidence: RM0430 §32/Table 219 and BTR/filter,
TX mailbox, RX FIFO and IRQ definitions.

- **Reusable:** CAN1 master/CAN2 slave share 28 filter banks; enabling CAN1 for
  CAN2 and using CAN1 filter registers is correct. The 14/14 split, 32-bit ID
  list/mask layout, mailbox transport and FIFO0 IRQs apply.
- **Clock dependency:** timing lookup accepts only listed PCLK1/baud pairs.
  24 MHz at 500 kbit/s is supported; 48/50 MHz are not. Recalculate timing or
  retain a supported PCLK1 after lowering SYSCLK.
- **Feature limit:** PHAL rejects CAN3. F423 CAN3 has independent memory and 14
  filters, not CAN1/2's shared scheme. Supporting it needs more than accepting
  another pointer. Current DAQ needs only CAN1/2.
- **Existing:** init/synchronization loops have no timeout. Test transceiver,
  error and sustained-load behavior on the real bus.

## CRC — register interface and algorithm remain usable

Files: `crc/crc.h`, `crc/crc.c`. Evidence: RM0430 §4.

RCC enable, CRC reset/data registers, fixed polynomial `0x04C11DB7`, all-ones
seed and 32-bit word feed match F423. Software fallback uses the same polynomial/
seed. Verify data ordering against consumers if formats change; no F423-specific
register rewrite was identified for current use.

## Shared SDIO and watchdog checks

These are outside PHAL F4 but DAQ depends on them.

- `common/sdio/sdio.c`: SDIO interface and DMA2 stream6/channel4 remain valid
  (RM0430 §31/Table 31). CLKDIV 118 assumes 48 MHz to obtain 400 kHz; transfer
  CLKDIV 0 gives 24 MHz. Check `SDIOSEL`/`CK48MSEL`, PCLK2/SDIO relationships,
  unique IRQ ownership and the flow-control-disabled errata condition.
- `common/watchdog/watchdog.c`: F423 uses `DBGMCU->APB1FZ` and
  `DBGMCU_APB1_FZ_DBG_IWDG_STOP`, as in the F407 branch. Extend architecture
  selection and check PR/RLR update completion (RM0430 §§22,34). With `/32`,
  reload units approximate milliseconds only at nominal 32 kHz LSI; tolerate
  oscillator variation.

## Errata review and remaining validation

Relevant ES0372 items extend the register review:

- §2.2.7: RCC enable delay/readback/barrier before peripheral accesses.
- §§2.2.9,2.10.1,2.10.3: DBP delay, RTC snapshot and repeated INIT handling.
- §2.5.2: evaluate SDIO underrun timing at actual clocks, width and load.
- §2.14: validate SPI disable/master receive timing; current TXE/BSY checks
  cover part of the disable workaround, but electrical timing needs measurement.
- §2.2.11: SPI1+SDIO are APB2 peripherals; their concurrency alone does not
  establish the specific AHB-peripheral DMA2 corruption scenario. Reassess if
  QUADSPI/FSMC/GPIO DMA is added.
- Review other peripheral/core limitations when enabling additional features.

Syntax probes/static review cannot establish runtime timing, oscillator/supply
correctness, wiring, flash reliability or throughput. Build/map and hardware
acceptance steps remain open in the checklist.

# F407 → F423 Migration Checklist

Fact-checked on 2026-10-23 against repository revision `6e4001ae7`, the local
**RM0430 Rev 9**, **RM0090 Rev 21**, ST's **DS11580 Rev 8**, and **ES0372 Rev 8**.
This plan tracks an in-progress migration; firmware has not been tested on F423 hardware.
The [PHAL audit](f423_phal_audit.md) records the register review, compile probes,
and existing driver defects separately. Follow-up planning incorporates the
user-supplied STM32F423RHT6 pinout image; pin changes will be implemented in a
**separate stacked PR**, as requested. Section K assigns a fix and validation
step to every audit finding; section L maps audit topics to those work items.

Target: **STM32F423RHT6**, LQFP64, **1536 KiB flash, 320 KiB SRAM, 100 MHz maximum**.
The original plan's 512 KiB flash, 128 KiB RAM, two-CAN, and absent-USB assumptions
were incorrect. Peripheral/register presence and package pin availability are
separate constraints.

## Sources

- [RM0430 Rev 9](datasheets/STM32F423_RM0430.pdf): F413/F423 memory and registers,
  especially §§2.3, 3.3–3.5, 5.4, 6, 7, 9, 13, 25, 28, 29, 31–34.
- [RM0090 Rev 21](https://www.st.com/resource/en/reference_manual/dm00031020.pdf): F407 comparison. It does **not**
  cover F423 despite the broad `F42xxx` wording in its introduction.
- [DS11580 Rev 8](https://www.st.com/resource/en/datasheet/stm32f423ch.pdf): F423xH
  device characteristics; §4.3/Figure 13 for LQFP64, Tables 10 and 12 for pins/AFs.
- [ES0372 Rev 8](https://www.st.com/resource/en/errata_sheet/es0372-stm32f413xgxh-and-stm32f423xh-device-errata-stmicroelectronics.pdf):
  F413/F423 silicon and documentation errata. Read with RM0430.
- Checked-in `firmware/external/cmsis-device-f4/Include/stm32f423xx.h`, submodule
  commit `0fa0e489e053fa1ca7790bb40b4d76458f64c55d` (`v2.6.11`).
- User-supplied schematic image in this conversation: B1, STM32F423RHT6,
  SCAN/VCAN/MCAN, W5500, SDIO, LEDs, and control/debug connections. The image
  is a planning reference, not an archived netlist or proof of signal polarity.

## Corrections to the original plan

| Original assertion | Fact check and consequence |
| --- | --- |
| F423RH has 512 KiB flash / eight sectors | **False:** 1536 KiB / 16 sectors, numbered 0–15. Extend the flash map; do not shrink it. RM0430 §3.3/Table 5. |
| F423RH has 128 KiB RAM | **False:** SRAM1 = 256 KiB at `0x20000000`; SRAM2 = 64 KiB at `0x20040000`. RM0430 §2.3.1. |
| No CCM RAM | **Correct**, but SRAM2 also has an alias at `0x10000000`. It is ordinary SRAM, not F407 CCM or an extra 64 KiB. Do not allocate both aliases independently. |
| No CAN3; losing a DAQ bus | **False:** F423 has CAN1/2/3. F407 already has only CAN1/2. Current DAQ uses two buses, so this migration does not inherently remove a bus. |
| No USB OTG or FSMC | **False:** F423 has USB OTG FS and FSMC. Pin availability limits external interfaces on LQFP64. Ethernet MAC and DCMI are absent. |
| F423 is a proper subset of F407 | **False:** F423 also adds CAN3, SPI4/5, QUADSPI, DFSDM, and more UARTs. It has SPI1–5, not SPI6; F407 has SPI1–3. |
| RH means PD0–PD3 are available | **False:** LQFP64 exposes **PD2 only**, plus PA/PB/PC and PH0/PH1; no PE/PF/PG pins. |
| Only PE3, PE15, PD4 need relocation | **Incomplete:** DAQ's **PD13/PD14/PD15 LEDs** are also unavailable. Six existing signals require relocation. |
| RM0090 covers F423 | **False:** use the already-added RM0430 PDF. |
| CMSIS system source lacks the F423 HSE path | **False:** `SystemCoreClockUpdate()` uses common F4 PLL decoding. The cited conditional blocks configure optional external SRAM/SDRAM, not the internal clock tree. |
| F423 needs a 170 MHz HSE path | **False:** its limit is 100 MHz, and current DAQ requests 168 MHz. Change application and PHAL clock setup. |
| CAN/SPI/DMA/IWDG/RTC are all clean | **Overstated:** several register interfaces remain usable, but clocks, guards, DMA routing, existing defects, and errata need work. See the PHAL audit. |
| F4 BL linker script has a 1 MiB flash region | **False:** BL actually allocates 32 KiB; APP allocates 992 KiB and standalone 1024 KiB. All three declare F407 RAM/CCM. |
| Every F407 library must be renamed | **Scope decided:** migrate all F4 targets to F423 and remove project-owned F407 support. CMSIS defines/startup select silicon, not library names. |
| Host Rust has no flash-size constants; tests have no board references | **False as blanket claims:** the G474 updater has slot/address constants, and FIL tests name G4 boards. They do not currently describe a DAQ/F423 updater. |

## Migration scope (updated 2026-10-30)

All F4 firmware targets, including `daq` and `f4_testing`, will move to F423.
No F407 build, debugger, linker or driver compatibility is required after the
migration. Keep the vendor CMSIS submodule intact; remove project-owned F407
selection/support rather than deleting files inside the vendor dependency.
For source targets without a supplied board map, temporary pin reassignment is
authorized: choose available pins valid for the target package/peripheral AF,
avoid collisions and SWD pins, and document the temporary mapping. These choices
support compilation/bring-up and do not establish physical board compatibility.
DAQ's supplied schematic remains the pinout contract for its separate stacked PR.

**Implementation scope (user clarification, 2026-10-30):** make the minimum clean
changes needed for existing F4 targets to run on F423. Preserve existing APIs,
valid-input behavior, timeout/error policy and supported features unless the
F423 hardware or the supplied board requires a difference. The audit remains a
complete record of findings and proposed fixes, not authorization to repair every
inherited F407 driver bug. The scope table in K determines migration requirements;
other audit work is deferred unless bring-up demonstrates a concrete blocker.

The broad RCC-03 transition rewrite was reverted before commit. The RCC-02 PLL
search refactor from `7bfb33c11` is also reverted in the working tree: the selected
16 MHz / M=8 / N=96 path already works with the original search. Its helper files
and changes to clamping/invalid-request behavior are removed. Required F423 clock
limits, startup divider ordering, voltage/latency and kernel clock selection still
need the focused migration work described below.


## A. Device selection, CMSIS, and library targets

- [x] Add/select `STM32F423xx` and `gcc/startup_stm32f423xx.s` through
  `firmware/cmake/dependencies.cmake`. All F4 consumers now use `CMSIS_F423`;
  F423 startup/system sources compile in the full build. The startup header's
  F413 comment is cosmetic.
- [x] **Remove project-owned F407 support.** Implemented section I and replacements
  in A–C. Move all F4 CMake/library consumers, device guards/direct includes,
  generator selections and launch entries to F423; provide/select the F423
  linker layout before deleting the F407 scripts. Remove the obsolete F407 SVD,
  unused APP/BL scripts and project-owned F407 configuration/files. Update
  READMEs, support-file inventories, examples and current setup instructions
  so none advertise F407 support. Use documented valid temporary F423 pins for
  examples without a schematic; keep DAQ's actual pinout in its stacked PR.
  Leave upstream vendor files intact and label any retained audit/manual
  comparisons as historical references rather than supported targets.
  **Verify:** search tracked project-owned filenames and contents for `F407`
  case-insensitively, review every remaining hit, confirm no references point
  to removed files, configure/build the affected F423 targets and check existing
  G4 targets. Record any remaining migration blockers explicitly; removing old
  support alone does not establish runtime readiness.
- [x] Migrate `f4_testing` to F423, including its CMSIS/library dependencies,
  startup, linker and debugger selection. Replace Discovery-board PD12–PD15
  LED assignments with valid temporary F423 package pins and document the map;
  audit every example/direct GPIO access for unsupported pins and AF conflicts.
- [x] Add F423 equivalents of `CMSIS_F407`, `PHAL_F407`, `FREERTOS_LIB_F407`,
  and `RTOS_F407`, or repurpose them intentionally. Update DAQ and dependencies
  in `common/sdio`, `external/w5500`, and `external/fatfs`, which link F407
  libraries. Move all F4 consumers to the F423 variants, then remove the F407
  targets; do not maintain parallel F407 libraries.
- [x] Extend device selection/guards in the following paths, relative to `firmware/`:
  - `common/phal_F4/phal_F4.h`, `gpio/gpio.h`, `usart/usart.h`, `usart/usart.c`,
    `spi/spi.c`, `flash/flash.h`, `flash/flash.c`, `rcc/rcc.h`, `rtc/rtc.c`;
  - `common/watchdog/watchdog.c`, `common/heartbeat/heartbeat.h`,
    `common/rtos/FreeRTOSConfig.h`;
  - `can_library/can_common.h`, `can_library/source/can_init.c`, `can_tx.c`,
    `can_rx.c`, including the exclusive-target checks;
  - `source/daq/spmc/spmc.c`, which now includes `stm32f423xx.h` (the baseline directly included F407).
  The baseline had nine F407 guards in `usart.c`; selection now uses F423.
- [x] Fix the additional **GPIO and ADC compile blockers** in the PHAL audit.
  CMake globs all PHAL `.c` files, so unused ADC code still has to compile.
- [x] Keep `HSE_VALUE` and PHAL's HSE frequency consistent with the actual board.
  The board owner confirmed 16 MHz for all PER boards, including G4, on
  2026-10-30. `PER_HSE_HZ` in `dependencies.cmake` exports the shared CMSIS
  `HSE_VALUE`; PHAL F4 derives `HSE_CLOCK_RATE_HZ` from it and rejects missing
  configuration. All firmware targets build; compile-command checks confirm
  16 MHz for all F423/G4 units, and compile probes verify the PHAL alias follows
  CMSIS. No CMSIS vendor-source or CPU/FreeRTOS-port changes were needed.
  DAQ now requests 96 MHz / 192 MHz VCO and PHAL
  caps SYSCLK at 100 MHz; these choices still need the RCC fixes and hardware
  measurements below. No F423-specific CMSIS system-source patch is needed
  for the present internal-memory configuration. CPU hard-float flags and the
  Cortex-M4F FreeRTOS port remain appropriate.

## B. Linker scripts and RAM layout

- [x] Add an F423RHT6 standalone script with FLASH at `0x08000000`, length
  **1536K**, and ordinary SRAM spanning `0x20000000`–`0x2004FFFF` (**320K**).
  Separate SRAM1/SRAM2 regions are also valid if allocated correctly.
- [x] Remove F407 CCM assumptions from the F423 script. Move any `.ccmram`
  contents into real SRAM and verify startup initialization. The SRAM2 alias
  at `0x10000000` cannot be counted in addition to the 320K.
- [x] Select the F423 script in both `source/daq/CMakeLists.txt` and
  `source/f4_testing/CMakeLists.txt`. Check stack top, `.data`, `.bss`, C
  library heap, main/IRQ stack, and DMA buffer placement in the resulting map.
- [x] Remove the project-owned F407 standalone, APP and BL linker scripts
  after moving their references to F423. No current target uses the F407 APP/BL
  scripts; this migration does not add an F423 bootloader.

## C. SVD and debugger

- [x] Supply an F423 SVD and validate register/IRQ metadata offline.
  Added `firmware/support/svd/STM32F423.svd` from ST's SVD 1.2 in the official
  Keil STM32F4xx_DFP v3.1.1 source. Corrected vendor IRQ metadata, nonexistent
  SPI6, FSMC base, CAN2/3 filter layouts and RCC fields against RM0430/CMSIS.
  Source, license, corrections and validation limits are recorded in the
  [support README](../firmware/support/README.md#f423-svd-provenance-and-corrections).
- [x] Update **Daq** in `.vscode/launch.json` to `STM32F423RHT6` and
  `STM32F423.svd`. Both F4 entries now select F423 and retain the family config
  `target/stm32f4x.cfg`. JSON and referenced support paths checked.
- [x] Update `firmware/support/README.md` to describe retained/new debug artifacts,
  SVD provenance/corrections and the pending firmware/hardware prerequisites.

- [x] Complete the all-F4 scope change for debug support: migrate `f4_testing`
  to `STM32F423RHT6`/`STM32F423.svd` and remove the unused F407 SVD. The checked
  items above record the earlier DAQ-only work; this follow-up is now complete.

Hardware debugger acceptance remains pending under VAL-01: after building an
F423 ELF, confirm OpenOCD identifies F423 and reports **1536 KiB** flash, then
verify halt, register inspection, programming and resume. Checking off the three
artifact/configuration tasks above does not claim hardware validation.

## D. New board pinout — separate stacked PR

The supplied image replaces the old instruction to find replacement pins from
scratch. The table below transcribes its named connections. Physical pin numbers
are included for review against the final netlist; GPIO/AF capability is checked
against DS11580 Tables 10/12. The screenshot itself is not stored in this repo.

| Signal in supplied image | F423 pin (package pin) | Configuration / migration consequence |
| --- | --- | --- |
| SCAN RX / TX | PA11 (44) / PA12 (45) | CAN1, AF9; replaces CAN1's current VCAN role. |
| VCAN RX / TX | PB12 (33) / PB13 (34) | CAN2, AF9; changes pins and current CAN2/MCAN role. |
| MCAN RX / TX | PA8 (41) / PA15 (50) | CAN3, AF11; requires CAN3 driver/IRQ/protocol work beyond GPIO changes. |
| W5500 nSS / SCK / MOSI / MISO | PA4 (20) / PA5 (21) / PA7 (23) / PB4 (56) | SPI1, AF5 for data/clock, GPIO CS; MISO moves from PA6 to PB4. |
| W5500 RST / nINT | PC3 (11) / PC2 (10) | Reset GPIO and interrupt input; confirm polarity/pulls and whether nINT is used or explicitly left masked. |
| SDIO CMD / CLK | PA6 (22) / PB15 (36) | AF12; replace PD2/PC12 macros. |
| SDIO D0 / D1 / D2 / D3 | PC8 (39) / PC9 (40) / PA9 (42) / PC11 (52) | AF12, four-bit mode; D2 moves from PC10 to PA9. |
| SD_Detect | PC4 (24) | Card-detect input; verify active level/pull from socket wiring. |
| SD Detect LED / SD ACTIVITY LED | PC5 (25) / PB0 (26) | GPIO LED outputs; polarity must match circuit. |
| USE_SD_LED | PC1 (9) | Named LED signal; confirm role. It does not establish a replacement input for current `LOG_ENABLE`. |
| HB_LED / ERR_LED / SCAN_LED | PB5 (57) / PB6 (58) / PB7 (59) | GPIO status LEDs; decide which supplies the existing connection indicator. |
| VCAN RX LED / VCAN TX LED | PB8 (61) / PB9 (62) | Additional GPIO activity indicators; drive from the corresponding bus events. |
| SD_Enable | PB1 (27) | Confirm direction/polarity and whether this controls power or requests logging before replacing any old input. |
| SPF_ALERT | PB2 (28) | Candidate replacement power-loss alert; verify meaning, polarity, pull, and hold-up timing. If confirmed, use EXTI2/PB2, not EXTI15/PE15. |
| HSE_IN | PH0 (5); PH1 (6) marked unused | Board owner confirmed 16 MHz. Consistent with a driven HSE input; confirm drive/source type before qualifying bypass mode. |
| SWDIO / SWCLK / SWO / NRST | PA13 (46) / PA14 (49) / PB3 (55) / NRST (7) | Preserve SWD/debug. PB4 and PA15 are repurposed JTAG pins; use SWD. |

The old map's six unbonded signals (PD13/14/15 LEDs, PE3 reset, PE15 alert,
PD4 card detect) remain invalid, but **other bonded pins also change function**:
PA6 becomes SDIO CMD, PA8 becomes MCAN RX, PA9 becomes SDIO D2, and PB5/PB6
become LEDs. Do not patch only the six absent pins. Old SD_ERROR_LED and
LOG_ENABLE roles have no explicit one-to-one assignment in the image.

- [ ] **BOARD-01 — finalize the board contract (stacked pinout PR).** Check this
  transcription against the final netlist, capture named GPIO/AF assignments and
  active levels in board configuration, and resolve `USE_SD_LED`, `SD_Enable`,
  `SPF_ALERT`, the old log-enable/error-LED roles, and W5500 nINT policy. Confirm
  3V3 operating range, HSE drive/source type (16 MHz confirmed), and backup supply: VBAT is marked
  unconnected in the image, so RTC supply/retention must be checked against the
  complete schematic. **Verify:** review every used pin and its direction/pull;
  no unresolved control signal may be enabled with an assumed polarity.
- [ ] **BOARD-02 — apply the supplied map (stacked pinout PR).** Update `main.h`,
  GPIO/SDIO/CAN AF macros, status LEDs, W5500 reset/interrupt/CS, SD detect and
  SD control, heartbeat/fault/shutdown paths, and reserved debug pins together.
  Migrate the F4 examples too, using documented temporary valid pin assignments
  where no board schematic exists; keep those examples distinct from the DAQ map. **Verify:** initialization and every direct GPIO access agree
  with the table/netlist; no old PA6 MISO, PA8/9 LED, PB5/6 CAN, PE/PD LED, or
  PD2 CMD assignment survives in the new-board target.
- [ ] **BOARD-03 — complete power-loss and SD control behavior (core/integration
  work; pin/IRQ routing in the stacked pinout PR, after BOARD-01).** If
  SPF_ALERT is the power-loss input, clear-and-set
  SYSCFG EXTICR1 for PB2, choose the documented edge, use EXTI2's mask/pending
  flag and handler, and notify the SD task from the ISR. Implement flush/close
  and high-load shutdown in task context; the old handler has these calls
  commented out. Define safe reset/output levels and SD-enable sequencing.
  **Verify:** inject alert/card-insertion/enable transitions and show flush
  completion within measured hold-up time without blocking in the ISR.

The pinout PR depends on the core fixes in K and CAN3/data-format work in F.
A compilable core port may land first; the new board is not ready for use until
its stacked pinout PR and the end-to-end three-bus changes are integrated.

## E. PHAL F4 and clocks — compatibility and audit notes

See [the complete PHAL audit](f423_phal_audit.md) for all eleven driver modules.
The scope table in K separates required compatibility changes from inherited bugs
in the following summary; these combined audit bullets are not all release gates.

- [ ] Replace DAQ's 168 MHz request and PHAL's 168 MHz cap. F423 limits are
  SYSCLK/HCLK **100 MHz**, PCLK1 **50 MHz**, PCLK2 **100 MHz**. Select VOS and
  flash latency using RM0430; set safe bus dividers before increasing SYSCLK.
- [ ] One **candidate**, using the confirmed 16 MHz HSE and assuming suitable supply, is
  PLLM=8, PLLN=96, PLLP=2, PLLQ=4: VCO=192 MHz, SYSCLK/HCLK=96 MHz,
  PCLK1=24 MHz (`/4`), PCLK2=48 MHz (`/2`), PLLQ=48 MHz. Scale 1 and 3 flash
  wait states apply at 2.7–3.6 V. This preserves existing 24 MHz CAN entries
  and a 48 MHz SDIO source; it is not measured or implemented.
- [ ] Fix GPIO's nonexistent F423 `GPIOI` case and missing `MODE` aliases;
  use actual F423 `MODER` field names. Validate pin bonding separately.
- [ ] Adapt ADC to `ADC1_COMMON`, accept ADC1 only, and validate channels and
  clock limits. DAQ does not call it, but the PHAL library compiles it.
- [ ] Extend flash sector definitions/lookup through sector 15, use end address
  **`0x0817FFFF`**, and reject invalid/unaligned operations. Sectors 0–11 keep
  their F407 addresses; add 12–15 at `0x08100000`, `0x08120000`, `0x08140000`,
  `0x08160000`. The non-F407 `flashGetSector()` branch unconditionally assigns
  7 after the comparisons; it is not a working fallback.
- [ ] Preserve the F4 SPI `CR1.DFF` implementation for F423; configure it with
  SPE=0. Set a valid nonzero `eth_spi_config.data_rate`: it currently defaults
  to zero, which PHAL divides by during init.
- [ ] Retain valid SPI1 DMA routes; fix **UART5 TX to DMA1 stream7/channel 8**
  if USART support is retained (currently channel 4). Review IRQ ownership and
  other existing defects in the audit.
- [ ] Fix RTC prescalers/calendar access; validate IWDG setup/debug-freeze
  guards. Register compatibility does not prove correct current behavior.

## F. CAN generator, topology, and the new third bus

The old board initializes CAN1/VCAN and CAN2/MCAN. The supplied board routes
**CAN1=SCAN, CAN2=VCAN, CAN3=MCAN**. This is a new hardware requirement,
not a silicon-driven loss of a bus. Full use of this board needs CAN3 support;
a two-bus intermediate build must be labeled incomplete for this board.

- [ ] **TOPO-01 — separate controller identity from logical bus ID.** Define one
  board-owned mapping for CAN1/SCAN, CAN2/VCAN, CAN3/MCAN; use it for init,
  callbacks, filters, LEDs, diagnostics, and subscriptions. Keep existing
  logical network IDs from `generators/configs/system/bus_configs.json`
  (VCAN=0, MCAN=1, SCAN=3); do not renumber the five network buses. Update
  `generators/configs/external_nodes/DAQ.json` or an explicit DAQ board config
  to describe the three physical connections; its current VCAN subscription
  alone is not a physical topology. **Verify:** inject the same CAN identifier
  on each interface and prove correct bus attribution in logs and UDP.
  Move GPS-time subscription/queue routing to logical VCAN (now CAN2);
  `can_callback.c` currently checks CAN1 explicitly. Verify GPS sync receives
  VCAN messages and ignores the same identifier arriving on SCAN/MCAN.
- [ ] **TOPO-02 — extend generator and common CAN integration.** Support F423
  bxCAN architecture guards in `codegen/contexts.py` and remove project-owned
  F407 selections. Add `CAN3` to `core/declarations.py` and all applicable peripheral
  views/templates, generated enqueue/filter declarations, shared init/TX/RX
  routing, queue/IRQ definitions, and exclusive-target checks. `hardware.py`'s
  CAN/FDCAN distinction is reusable, but filter capacity must respect CAN3's
  independent 14 banks. **Verify:** regenerate from source; compile examples
  for F407 CAN1/2, F423 CAN1/2/3 and G474 FDCAN without hand-editing generated files.
- [ ] **TOPO-03 — version the DAQ frame format before enabling three buses.**
  `common/timestamped_frame/timestamped_frame.h` currently stores a one-bit bus
  ID; `source/daq/can_callback/can_callback.c` maps all non-CAN1 frames to 1.
  Update UDP encode/decode and Rust live/log consumers, including
  `daq/daqcore/src/log_parse/{parse,consts}.rs`, `can/driver.rs`, `can/logger.rs`
  and `can_thread/run.rs`. The current UDP parser strips the bus flag when
  constructing `CanFrame`; preserve bus metadata through decoding, dispatch,
  logging and UI selection instead of assigning every received frame to the
  selected decoder's bus. Remove the current bus>1 legacy-logging rejection
  only for the newly versioned format, keeping old-format restrictions explicit.
  Define a versioned bus encoding
  capable of IDs 0, 1, 3, update masks/validation and all consumers, and document
  compatibility with old captures/hosts. Reusing reserved identity bit 29 along
  with bit 31 is one candidate that preserves 29-bit CAN IDs and bit 30 IDE;
  approve/document the layout and version negotiation/identification before
  implementation. Do not silently reinterpret old data or collide with CAN IDs.
  **Verify:** old-format fixtures remain readable, new SCAN/VCAN/MCAN frames
  round-trip live and from disk, and unsupported versions/bus IDs fail clearly.
  Recalculate memory/packet sizing if the current 16-byte record changes.
- [ ] Complete **CAN-01–04** in K before three-bus acceptance. CAN3 requires
  independent filters/clock/IRQs, and all three SPMC producer IRQs must share
  the same preemption priority. Extending GPIO macros alone cannot enable it.

## G. Memory budget and performance

- [ ] Verify the linked memory budget against **320 KiB**, then measure stack
  high-water marks and sustained three-bus CAN logging/UDP throughput at the lower
  CPU speed. More SRAM does not compensate automatically for less CPU.
- [ ] Include the SPMC buffer: `512 × 12 × 16 = 98,304` bytes of frame storage
  alone, plus metadata. This is much larger than the task stacks.
- [ ] DAQ explicitly declares two 4096-byte stacks, a 512-byte RTC-sync stack,
  a 512-byte heartbeat stack, and a 256-byte watchdog stack: **9472 bytes**, plus
  TCBs, idle/timer stacks, queues, filesystem state, and other globals.
  `RTOS_DEFINE_TASK` sizes are **bytes**, converted to `StackType_t` elements.
  Current DAQ `main.c` does not declare a diagnostics task.
- [ ] FreeRTOS dynamic allocation is disabled and its heap size is zero. This
  does not remove C-library/main/interrupt-stack budgeting from the linker map.

## H. Documentation and datasheets

- [x] Link RM0430 as the F423 manual in `docs/library.md`. The RM0430 file is
  already present and is Rev 9.
- [x] Make F423/RM0430 the supported F4 documentation throughout the repository.
  Keep RM0090 only as a clearly labeled historical audit reference if needed;
  it does not imply ongoing F407 support.
- [ ] Archive/link the F423xH **DS11580** datasheet and **ES0372** errata for
  electrical/package/silicon constraints. Neither was present locally at audit
  time. The F413 DS11581 is not the exact F423 datasheet.
- [x] Update `firmware/support/README.md` after selecting linker/debug support.
  `firmware/README.md` describes generic targets and needs no part-number edit.

## I. Remove obsolete F407 support

- [x] Remove project-owned F407 standalone/APP/BL linker scripts and F407 SVD
  once all references are migrated; leave upstream CMSIS vendor files intact.
- [x] Remove F407 CMake targets, launch selections, generator architecture
  choices and project driver branches after all F4 consumers use F423. Update
  `f4_testing` launch device/SVD too. Search project-owned code for residual
  F407 assumptions; historical audit comparisons may remain explicitly labeled.
- [x] Replace the direct F407 include in DAQ SPMC with device-neutral selection;
  update stale comments in RTC, PWM, and peripheral documentation as appropriate.

### Cleanup verification (2026-10-30)

- Configured with CMake 4.1.6 / ARM GCC 15.2.1 and built **all** firmware targets
  in `/tmp/f423-cleanup-build`, including F423 DAQ/bench, G4 applications and
  G4 bootloaders. Used the existing dependency-equipped Python environment;
  `pydantic` is no longer a configuration blocker in that environment.
- Built the FreeRTOS and PWM bench selections separately; the default onboarding
  selection also links. LED placeholders are PC0–PC3 (onboarding uses PC0), with
  USART2 PA2/PA3. The bench README records external wiring and PWM SWO usage.
- CAN compiler tests passed (18 tests), and `git diff --check` passed.
- Fixed required F423 compilation differences: `ADC1_COMMON`, unavailable GPIOI,
  GPIO MODER field names, and SDIO's reserved bit 9. Removed obsolete SDIO
  start-bit-error masks/status/clear handling rather than defining reserved bits.
- Extended flash lookup/constants through sector 15, end `0x0817FFFF`; exercised
  the first/last byte of every sector with the actual lookup function. Full flash
  validation, failure cleanup and destructive on-device tests remain FLASH-01–04.
- Verified `.ccmram` probe data lands inside initialized `.data` at `0x20000000`,
  with a flash load address, and `_estack = 0x20050000`. No separate CCM alias is
  allocated. Final runtime stack/heap/DMA budget checks remain VAL-02.
- Reviewed remaining project-owned `F407` hits: historical audit/plan text and an
  unrelated CRC table value / dependency checksums. Vendor CMSIS/FreeRTOS content
  is unchanged. Removed the local RM0090 PDF; historical comparisons link ST's
  online manual. Current support inventories and launch paths use F423.

This completes support removal and compile/link migration only. DAQ's legacy
pin map still needs BOARD-01/02 in its separate stacked PR; CAN3/protocol work,
clock sequencing/latency and applicable F423 compatibility/errata work are pending.
Other inherited driver findings are follow-ups under K's scope table.

## J. Scope, errata, and acceptance checks

- **CMSIS:** header/startup/internal-clock reporting are usable. The original
  plan contradicted itself by declaring the whole submodule unaffected while
  asserting a clock blocker. Neither blanket statement describes the port.
- **Host Rust:** no F423-driven wire-format change is required for the current
  two-bus DAQ protocol, but the supplied board now requires the three-bus
  format/host changes in TOPO-03. `daq/daqcore/src/firmware/protocol.rs` contains
  G474 updater address/slot constants; DAQ is not in that six-board updater.
  Keep those G474 updater constants scoped to their existing boards.
- **Build/CI/container/Nix:** targeted searches found no F407 selector to change
  outside the firmware/debug/generator paths listed above. Build scripts still
  need validation with the eventual F423 target. `build_firmware.py` packages
  only G4 bootloadable nodes, although it also builds other targets.
  `check_firmware.py` suppresses cppcheck findings under `common/phal_F4/*`;
  passing that workflow alone cannot establish PHAL F423 compatibility.
- **DBC/tests:** unchanged CAN messages do not inherently need a silicon-driven
  DBC change, but tests reference board configurations; existing G4 FIL tests
  do not validate F423 DAQ hardware.
- [ ] Apply relevant ES0372 workarounds, especially RCC enable delays, RTC
  calendar access, SPI shutdown/receive timing, and SDIO clock/bandwidth limits.
- [ ] Build/link all migrated F4 targets and existing G4 targets; inspect map,
  vectors, and generated CAN guards. Audit syntax probes are not a successful
  migrated firmware build.
- [ ] On hardware, verify measured clocks, the complete supplied pin map, all
  three CAN buses and their logical IDs, W5500 traffic, SD reads/writes under
  concurrent CAN load, RTC rollover/GPS sync, IWDG reset/debug behavior, and
  power-loss flush.
  Test first/last flash-sector operations only in a reserved area.

## Top blockers, ranked

1. Correct the 168 MHz request, VOS/flash latency, and clock-switch order.
2. Integrate the separate stacked pinout PR using the supplied board map.
3. Add coherent F423 device/build selection and fix GPIO/ADC compile blockers.
4. Replace the linker layout and extend flash support to 1536 KiB/16 sectors.
5. Add CAN3 and versioned three-bus host/log support; validate timing, SPI,
   RTC, SDIO, errata, and hardware behavior against K/M before sign-off.

## K. Detailed implementation work from the PHAL audit

Keep every audit note and its proposed resolution below. Only the portions in
this table are migration implementation requirements; broader validation,
recovery, API changes and driver refactors are follow-up work. Preserve existing
behavior for unchanged hardware interfaces. Paths are under
`firmware/common/phal_F4/` unless otherwise stated.

| Area / audit IDs | Minimum migration work |
| --- | --- |
| RCC-01/03/04/05/06 | Keep 16 MHz HSE, choose a legal F423 clock tree, set required VOS/flash latency and APB dividers before increasing clocks, and supply correct clocks to enabled consumers. Retain existing APIs/error policy. General PLL search, dynamic reconfiguration, timeout/fallback framework and arbitrary-request validation are deferred. |
| GPIO-01/02/03; BOARD-01–03 | Fix device/register selection and provide bonded pins/AFs and required electrical settings. DAQ mapping stays in its stacked PR; preserve existing SD/power-loss behavior. New pin validation and unrelated GPIO cleanup are deferred. |
| ADC-01 | Use the F423 common block and single ADC instance; keep existing conversion behavior. ADC-02/03 validation/reinitialization improvements are deferred unless an existing F4 target needs them to run. |
| SPI-01–05; DMA-01–04 | Preserve compatible register/DMA interfaces and existing transfer modes. Supply a valid W5500 clock request and correct only demonstrated migration blockers or applicable F423 errata on enabled paths. General DMA/SPI lifecycle, input-validation and polling rewrites are deferred. |
| UART-01/02 | Select the existing SR/DR path, correct F423-specific DMA routing (UART5 TX channel 8), and ensure enabled consumers have unique IRQ owners. Framing/flow-control/receive-policy redesign in UART-03/04 is deferred. |
| FLASH-01 | Keep F423 geometry/address/sector lookup correct. A new table abstraction and generalized validation/status/cache refactors in FLASH-02–04 are deferred unless required by an enabled migration consumer. |
| RTC-01–03; PWM-01–03; CRC-01; WDG-01/02 | Preserve compatible operation and public contracts; update device guards and changed clock dependencies where necessary. Inherited calendar, timer, watchdog and validation bugs remain follow-up unless demonstrated to block a migrated target. |
| CAN-01/02/04; TOPO-01–03 | Add the board-required CAN3/filter/IRQ path, retain correct timing at the selected PCLK1, and carry three bus identities end to end. Change host/log format only where the current format cannot represent them. General CAN timeout/recovery work in CAN-03 is deferred. |
| SDIO-01/02; ERR-01/02 | Remove reserved F423 bits, use the required pin/kernel-clock/divider settings and resolve applicable F423 errata for enabled transfers. Do not rewrite all peripheral enable sites or unused modes without a demonstrated requirement. |
| VAL-01–03; G | Build all existing F4 targets and G4 regression targets, check F423 memory/vectors, then verify migrated board functions on hardware. Broader audit defects are recorded, not silently marked fixed. |

### Audit follow-ups

Unchecked items below retain the detailed proposed fix and verification plan for
future work. An unchecked follow-up is not a migration blocker by itself; apply
the table above to each subtask before implementing it.

### K.1. RCC and flash latency

- [ ] **RCC-01 — select the F423 clock tree.** For migration, configure and
  verify the selected target rates without changing the clock API or request
  policy. General planner/validation proposal (deferred): in `rcc.h/.c`, introduce
  per-device SYSCLK/HCLK/APB/VOS limits and remove silent target clamping.
  Precompute an exact clock tree, including PLLM/N/P/Q and kernel selectors,
  before touching hardware. Use the 96/24/48 MHz candidate in E only after
  BOARD-01 confirms the HSE drive/source type and supply (16 MHz is confirmed).
  Validate unchanged PLL ranges from the
  audit, nonzero requests and supported ratios; leave PLLR at a documented valid
  setting unless I2S/DFSDM is enabled. **Verify:** arithmetic cases cover boundary,
  zero, non-divisible and unattainable requests; report actual achieved rates.
- [ ] **RCC-02 — deferred: correct the general PLL search.** The previous
  helper/refactor was reverted to preserve existing behavior. The migration's
  16 MHz input and 192 MHz VCO use M=8/N=96, found by the existing search.
  For a separate general driver fix, use counters wide enough for N=432, restart
  N for each M and use exact wide arithmetic. Verify N>255, later-M solutions,
  fractional input/M and impossible targets. Do not change clamping or error
  policy as part of this migration without a concrete target requirement.
- [x] **RCC-03 — startup APB1 divider ordering (2026-10-30).** Before selecting
  PLL as SYSCLK, `PHAL_configurePLLSystemClock()` changes an undivided APB1 to
  `/2`, preserving an existing `/2` or slower divider. This keeps APB1 at or below
  50 MHz for F423's maximum 100 MHz SYSCLK; APB2 and HPRE are unchanged. The
  existing final bus configuration still sets DAQ's requested `/4` (24 MHz).
  **Verified:** temporary ASan/UBSan register checks of the actual functions
  cover all eight PPRE1 encodings at 96/100 MHz, the intermediate APB1 limit,
  preservation of slower dividers/APB2/HPRE, and final `/4` application. All
  configured F423/G4 targets and all three F4 bench variants build. Hardware
  verification and RCC-04's VOS/latency configuration remain open.
  **Deferred follow-up:** the remaining general transition proposal is not
  implemented by this checkbox: enable/wait for HSI; select
  SW with clear-and-set writes and compare the full SWS field. Keep a usable
  source enabled during transition, switch off the current PLL only after moving
  SYSCLK away from it, configure safe AHB/APB dividers before raising SYSCLK,
  and stop on the first failure. Use bounded ready/switch polls that work before
  the scheduler/tick exists; return an error with the system on a known safe
  source where possible. **Verify:** reset→PLL, PLL→HSI, HSE/PLL reconfiguration,
  and simulated missing-ready conditions; inspect intermediate APB rates too.
- [x] **RCC-04 — F423 startup VOS/flash latency (2026-10-30).** For a PLL
  SYSCLK above 84 MHz, enable/read back PWR's clock and select Scale 1 while
  PLL is off. After enabling PLL, wait for PLLRDY and VOSRDY before selecting
  PLL as SYSCLK (RM0430 §5.1.3/§5.4.1). Lower-rate startup retains the existing
  voltage-scale setting. Replace the F407 flash thresholds with RM0430 table 6's
  2.7–3.6 V column: 0/1/2/3 WS through 25/50/75/100 MHz. SYSCLK is used as a
  conservative upper bound on HCLK, so the final AHB divider cannot require more
  latency. Raise/read back latency before switching; retain any higher existing
  latency. The selected 96 MHz startup uses Scale 1 and 3 WS.
  **Verified:** 22 temporary ASan/UBSan register cases cover table boundaries,
  80/96 MHz, VOS writes with PLL off, ready-before-switch ordering, unchanged
  unrelated PWR/FLASH bits and preservation of higher latency. All configured
  F423/G4 firmware targets and all three F4 bench variants build.
  **Board qualification remains open:** this implements the supplied schematic's
  nominal 3V3 supply, assuming VDD stays within 2.7–3.6 V. Confirm its guaranteed
  range and measured clocks at bring-up; another range needs its table column.
  General live clock/VOS changes and latency reduction remain deferred, with
  the existing API, PLL search and wait/error policy preserved.
- [x] **RCC-05 — select the 48 MHz SDIO kernel clock (2026-10-30).** Keep the
  existing PLLQ calculation and reject VCO rates not exactly divisible by 48 MHz
  before register writes. Explicitly clear `CK48MSEL` and `SDIOSEL` to select
  main PLLQ → CK48 → SDIO (RM0430 §6.3.29), preserving unrelated mux fields.
  DAQ's selected 192 MHz VCO / Q=4 supplies exactly 48 MHz; the old 200 MHz /
  Q=4 result (50 MHz) is rejected. No clock API or other PLL search changes.
  **Verified:** temporary ASan/UBSan register checks cover all four prior mux
  combinations, Q=4/48 MHz, preservation of unrelated mux bits, and rejection of
  200/201/202 MHz VCO requests before hardware writes. All configured F423/G4
  targets and all three F4 bench variants build. Measure the resulting SDIO
  clock on hardware with the stacked pinout PR; USB remains unused.
- [x] **RCC-06 — synchronize startup clock reporting (2026-10-30).** Keep the
  existing `SystemCoreClockUpdate()` calls after source switches; add the missing
  call after HPRE changes. Set `AHBClockRateHz` from CMSIS HCLK and APB globals
  from the programmed PPRE fields using CMSIS `APBPrescTable`. The selected DAQ
  rates remain 96/24/24 MHz; bench examples retain their 16 MHz settings. Update
  the APB maximum-rate comments to F423's 50/100 MHz limits.
  **Verified:** 450 temporary ASan/UBSan arithmetic/register cases cover both
  16/96 MHz sources, all supported AHB divisors and APB divisor combinations.
  CMSIS HCLK and AHB/APB globals match the programmed rates. All configured
  F423/G4 firmware targets and all three F4 bench variants build.
  **Still open:** BOARD-01 must confirm HSE drive type before changing the
  existing bypass policy. Measure the RTOS 1 ms tick and clocks on hardware
  without stealing MCAN RX on PA8. General request validation and live clock
  reconfiguration are deferred; API and error behavior are unchanged.

### K.2. GPIO and board-independent pin macros

- [ ] **GPIO-01 — repair F423 compilation.** Guard GPIOI support by actual
  device availability and use the common `MODER` field names present in both
  headers (or explicit compatibility definitions). Extend appropriate F4 guards.
  **Verify:** compile every PHAL source under the F423 device macro;
  selecting F423 must not depend on defining F407 simultaneously.
- [ ] **GPIO-02 — make macros match the supported board.** Correct the PB4
  SPI1 MISO name and introduce the AF12 SDIO PA6/PB15/PA9 alternatives used by
  BOARD-02. Keep unsupported-package PD0/PD1/PD5/PD6 choices out of the F423RH
  board table; migrate bench/example macros to valid temporary F423 pins and
  document their assignments. Validate pin<16,
  AF<16 and bank membership before shifts/register writes. **Verify:** review
  every table entry against DS11580 and compile board init independently.
- [ ] **GPIO-03 — document electrical settings and reset behavior.** Replace
  absolute MHz comments with speed-grade encodings and datasheet-dependent
  limits. Specify pull, output type, LED polarity, safe initial level and drive
  speed in the board contract; set output latches before enabling outputs when
  needed to avoid reset/CS/enable glitches. **Verify:** scope CS/reset/SD-enable
  at reset and initialization, with SWD still connected.

### K.3. ADC

- [ ] **ADC-01 — select the real peripheral.** Use the device-appropriate ADC
  common-block name and explicitly accept only ADC1 for F423; remove arithmetic
  enabling of nonexistent ADC2/3 RCC bits. **Verify:** all PHAL sources compile
  and invalid instances/numbers fail before any register write.
- [ ] **ADC-02 — validate and reset configuration.** Enforce sequence length
  1–16, legal channel/rank values and uniqueness where required, prescaler and
  datasheet ADC clock/acquisition limits. Clear/rewrite SCAN, DISCEN, CONT,
  DMA and DDS fields for every mode change so old state cannot leak.
  **Verify:** continuous→discontinuous and circular-DMA→off transitions, invalid
  inputs and sequence bounds with focused register tests or a bench harness.
- [ ] **ADC-03 — handle internal sensors and reinit.** Wait the specified
  TSVREFE stabilization interval before conversions; resolve sensor/VBAT shared
  channel selection using the exact device documentation. Update sequences only
  while conversion is stopped or follow the ES0372 restart procedure.
  **Verify:** sensor/reference settling and known-voltage samples if ADC is used.
  DAQ may leave ADC disabled, but it must compile; record hardware testing as
  not exercised for this target rather than claiming complete ADC qualification.

### K.4. SPI and the actual W5500 path

- [ ] **SPI-01 — fix init and rate selection.** Require 8/16-bit DFF and a
  nonzero requested speed; configure DFF/BR/CPOL/CPHA/NSS with SPE=0. Pick the
  smallest legal power-of-two divider whose actual SCK does not exceed the
  request or board/device limit; reject unattainable inputs. Set a board-owned
  nonzero W5500 rate in DAQ. **Verify:** divider bounds and rates around each
  boundary, then measure SCK and read W5500's version register on PB4 MISO.
- [ ] **SPI-02 — define supported instances/modes.** Keep SPI1/2 as the supported
  set for this migration and explicitly reject SPI3–5; retain DFF rather than
  adding unrelated DS/FRXTH logic. Validate transfer configuration before
  asserting CS or marking a transfer busy. **Verify:** invalid instance/frame
  size/configuration leaves CS inactive and the driver idle.
- [ ] **SPI-03 — fix DMA completion/ownership.** Validate optional RX state
  before dereference; support TX-only DMA or reject it clearly before starting.
  Replace stream7's DMEIF4 with its actual flag and dispatch/enable the IRQ
  corresponding to the configured TX stream, with a single owner per vector.
  Preserve valid SPI1 stream2 RX/stream3 TX channel3 and SPI2 routes.
  **Verify:** successful, transfer/direct/FIFO-error and abort paths release CS,
  stop streams with DMA-01, clear flags, and leave usable error status.
- [ ] **SPI-04 — repair and exercise polling callbacks.** Remove NULL-pointer
  arithmetic from `ethernet/w5500_callbacks.c`; index buffers only when present
  and bound accesses by their supplied lengths. Keep CS asserted across the
  full W5500 header/payload and use documented TXE/BSY/DR/SR sequences, including
  overrun clearing. **Verify:** single-byte and burst read/write, empty payload,
  and back-to-back traffic with a logic analyzer; these callbacks, not the
  configured DMA pair, are the present W5500 data path.
- [ ] **SPI-05 — apply relevant shutdown/receive errata.** Keep master TX
  disable after TXE=1/BSY=0, bound waits, and handle slave/receive-only modes
  only if implemented. Check SCK feedback timing against ES0372 at the chosen
  speed and load; reduce SCK or apply the documented timing workaround if needed.
  **Verify:** long Ethernet transfers under CAN/SD load and timeout/reset recovery;
  explicitly exclude unused slave/I2S/CRC modes from the supported configuration.

### K.5. DMA

- [ ] **DMA-01 — stop streams safely.** Replace the destructive EN polling
  expression with a read-only test; clear EN and wait for acknowledgement before
  changing CR/PAR/M0AR/NDTR/FCR. Make stop/reconfigure/re-enable use the same
  bounded completion helper and propagate timeout failure to callers.
  **Verify:** mock delayed EN clearing plus repeated SD/SPI/USART stop/restart;
  no configuration register changes while the stream is enabled.
- [ ] **DMA-02 — validate transfer descriptors.** Check controller/stream/index
  consistency, device request range (including F423 channels 8/9), direction,
  priority, widths, alignment and count 1–65535 before writing registers.
  Explicitly define zero-length behavior. **Verify:** boundary/error inputs and
  expanded config macros, including UART5 channel 8 and SDIO/SPI stream tuples.
- [ ] **DMA-03 — resolve unimplemented modes explicitly.** Keep migration API
  scope to peripheral-to-memory and memory-to-peripheral transfers and reject `mem_to_mem=true` with a clear
  unsupported result; fix the API comment instead of pretending DIR supports it.
  A future memory-to-memory implementation would need DMA2 and its real DIR mode.
  **Verify:** unsupported modes fail without altering hardware or stream state.
- [ ] **DMA-04 — clear correct flags and assign IRQs once.** Use named per-stream
  IFCR flags, including HTIF when clearing an entire stream, instead of the
  incomplete 0x2D mask. Clear flags with direct writes and avoid interrupt flag
  read-modify-write. Define a resource table for SDIO/SPI/USART streams and IRQs.
  **Verify:** all streams' masks, stale-flag restart, and an actual link of the
  selected consumers with no duplicate/missing vector owner.

### K.6. USART

- [ ] **UART-01 — retain the correct F4 interface.** Extend device selection for
  SR/DR, clock enables, BRR, constants, IRQs and DMA macros; avoid the RDR/TDR
  fallback for F423. Define the supported set as USART1/2/3/6 and UART4/5;
  explicitly reject UART7–10 until implemented and document array/index limits.
  **Verify:** instantiate each supported configuration macro under F423,
  reject unsupported peripherals/indices and compile both header branches.
- [ ] **UART-02 — fix routing and vector conflicts.** Change only F423 UART5_TX
  to DMA1 stream7/channel 8, retain valid routes for other instances, and fix
  stream7's DMEIF flag. USART6 TX cannot own SDIO's DMA2 stream6 concurrently;
  provide a device-valid alternative with its handler or reject that combination.
  Resolve UART4/SPI2 DMA1 stream4 conflicts similarly. **Verify:** route table
  review, link-time vector ownership, and loopback on each enabled route.
- [ ] **UART-03 — correct framing/flow control.** Encode two stop bits as `10`
  and half a stop bit as `01`; reject fractional stops on UARTs that do not support
  them. Program CTSE/RTSE only for supported instances/modes, or reject requested
  hardware flow control until implemented. **Verify:** inspect CR2/CR3 values and
  scope/loop back each advertised framing/flow-control mode.
- [ ] **UART-04 — validate baud and receive lifecycle.** Check baud/fck/oversample
  divisors and handle/pointer/index bounds before registering an active transfer.
  Review RXNE→DMA startup, error clearing, IDLE detection and stop/rearm ordering
  using SR/DR rules and DMA-01; add bounded error recovery. **Verify:** short,
  long, back-to-back and errored receptions at the selected clocks, including
  the ES0372 IDLE-detection limitation; use an explicit length/timeout framing
  fallback where an IDLE-only completion guarantee cannot be made.

### K.7. Flash

- [ ] **FLASH-01 — describe geometry once.** Add a per-device sector table with
  F423 sectors 0–15/end `0x0817FFFF`; remove F407-specific maps and drive lookup,
  maximum sector and erase-range validation from it. Eliminate the unconditional
  sector 7 fallback. **Verify:** every sector start/end and out-of-range address,
  especially the 1 MiB boundary and sectors 12–15; cross-check linker regions.
- [ ] **FLASH-02 — preflight programming/erase requests.** Check alignment,
  range, erased state where required, count/size arithmetic overflow and empty
  request semantics before unlocking. Choose/document PSIZE from the board's
  guaranteed voltage; reject unsafe width/voltage combinations.
  **Verify:** malformed requests cause no programming/erase action; word/U64/
  buffered boundaries stay inside flash, and U64 is documented as two writes.
- [ ] **FLASH-03 — implement operation cleanup/status.** Clear prior status
  using documented writes, wait/check BSY and error flags for each programming
  operation, and propagate errors instead of unconditional buffered success.
  Funnel all exits through cleanup that clears PG/SER as appropriate and locks
  flash, including timeout and non-erased-address returns. **Verify:** simulated
  write/erase failures and timeouts, cleanup state and successful readback.
- [ ] **FLASH-04 — maintain cache and execution safety.** Apply RM0430's cache
  maintenance sequence after modification; account for flash-stall latency in
  task/watchdog/interrupt behavior. Use a reserved test sector outside code and
  retained data, with recovery access via SWD. **Verify:** program/read/erase
  readback including cached reads on hardware; first/last-sector coverage needs
  a dedicated test image/layout, never an erase of a running application's code.

### K.8. RTC

- [ ] **RTC-01 — specify the timebase and valid calendar.** Keep LSI unless the
  final board provides another valid source; PH1 and PC14/15 are unused in the
  supplied image. Replace the incorrect LSE comment, mask-and-set PRER fields
  in the required two-write INIT sequence, and use representable prescalers
  (e.g. 127/249 for nominal 32 kHz). Validate dates/weekdays and define a legal
  fallback rather than initializing month/day/weekdays to zero. Quantify LSI
  drift/calibration and GPS correction policy. **Verify:** PRER from reset and
  retained values, 1-second ticks, month/year/leap-day rollover and GPS updates.
- [ ] **RTC-02 — handle retained domain and protection.** Enable/read back PWR,
  set/wait DBP as required, inspect RTCSEL/INITS and retained calendar before
  changing anything. If source changes, explicitly reset and reinitialize the
  backup domain according to the retention policy, rather than ORing a new
  RTCSEL. Unlock/relock WPR and restore DBP on every exit; bound waits and wait
  for synchronization before use or repeated INIT as ES0372 requires.
  **Verify:** cold start, software reset, repeated forced sync, absent LSI-ready,
  differing retained source and actual board VBAT/retention behavior.
- [ ] **RTC-03 — return a consistent timestamp.** Implement the documented
  shadow/snapshot sequence plus the applicable ES0372 consistency workaround,
  rather than rereading TR/DR independently per field. Keep the current decimal
  output contract explicit through names/docs or a separate conversion API;
  inputs remain BCD where expected. Update callers atomically if API changes.
  **Verify:** midnight/month rollover snapshots and filename/GPS conversions
  cannot mix dates/times or reinterpret decimal as BCD.

### K.9. PWM

- [ ] **PWM-01 — derive the timer kernel rate.** Compute TIMxCLK from actual
  PCLK/APB divider and TIMPRE using the device rules, rather than PCLK alone;
  derive PSC/ARR with overflow, zero-frequency and achievable-rate checks.
  **Verify:** APB=/1,/2,/4 and both TIMPRE states where supported, then measure
  a known PWM rate on a permitted test pin without taking a live CAN/SDIO pin.
- [ ] **PWM-02 — respect timer capabilities.** Add an instance capability table
  for channel count, counter width and BDTR/MOE. Reject TIM6/7 and nonexistent
  channels; avoid reserved BDTR writes on general-purpose timers. Validate the
  board's timer AF choice independently. **Verify:** each supported timer/channel
  limit and rejection of a four-channel request on a one/two-channel timer.
- [ ] **PWM-03 — load preloads deterministically.** Disable the counter/output
  as needed, write PSC/ARR/CCRs, generate the required update, clear unintended
  pending status and enable in a controlled order. **Verify:** first pulse,
  duty 0/100%, frequency changes and repeated initialization. DAQ does not enable
  PWM currently; record disabled/not hardware-exercised until a test target uses it.

### K.10. CAN

- [ ] **CAN-01 — implement F423 CAN3 independently.** Add CAN3 instance/clock
  support and TX/RX IRQs; operate its own 14 filter banks through CAN3 registers,
  not CAN1's shared block. Keep CAN1/2's 28 banks and 14/14 split with the CAN1
  clock dependency for CAN2. Respect standard/extended ID filter packing,
  empty-list accept-all and per-controller capacity. **Verify:** mailbox/FIFO/
  filter/error paths on all three, with disjoint filters proving no cross-bank
  interference; F407 must reject unavailable CAN3 before touching hardware.
- [ ] **CAN-02 — tie baud tables to actual PCLK1.** Retain a supported 24 MHz
  PCLK1 or calculate validated timing entries for another selected rate. Verify
  bit time, sample point and SJW for each required bus baud rather than reusing
  constants at 48/50 MHz. **Verify:** loopback then real transceiver traffic with
  measured rates; unsupported PCLK/baud combinations fail before init.
- [ ] **CAN-03 — bound initialization and bus recovery.** Add timeouts/error
  propagation for initialization/synchronization and verify transceiver/reset
  sequencing. Define behavior for an absent bus, bus-off and full mailboxes/FIFO.
  **Verify:** unplugged/no-ACK/reconnect and saturated-bus cases return/report
  controlled errors instead of hanging the firmware.
- [ ] **CAN-04 — integrate three producers correctly.** Complete TOPO-01–03,
  add CAN3 RX0/TX enable/priority/callback routing as used by DAQ, and give all
  SPMC producer IRQs the same preemption priority to retain its single-producer
  assumption. Trace bus→record→queue→SD/UDP→host and attach activity LEDs to
  logical buses. **Verify:** simultaneous SCAN/VCAN/MCAN input at target load,
  correct IDs/timestamps and measured overflow/follower-drop counters.

### K.11. CRC, SDIO, watchdog and errata

- [ ] **CRC-01 — preserve the validated interface/contract.** Keep F423's
  fixed polynomial `0x04C11DB7`, all-ones seed and 32-bit feed/reset behavior. Add
  a known-word-sequence comparison between hardware and the existing software
  fallback if CRC is exercised by a migration consumer; document byte order.
  **Verify:** empty/multiword vectors and consumer agreement. No unrelated G474
  updater or format change is needed merely because this peripheral is retained.
- [ ] **SDIO-01 — bind kernel source, pins and dividers together.** With RCC-05,
  retain 48 MHz input and explicitly set CLKDIV 118 for 400 kHz identification and
  CLKDIV 0 for 24 MHz transfer, four-bit mode and BOARD-02's new AF12 map. Keep
  DMA2 stream6/channel 4 and an exclusive handler. **Verify:** measured CMD/CLK,
  initialization, single/multiblock read/write and removal/reinsert recovery.
- [ ] **SDIO-02 — resolve underrun/CRC errata at real clocks.** Evaluate ES0372
  §2.5.2's inequality using the selected PCLK2/SDIOCLK/SDIO_CK and bus width,
  not a generic assumption that flow-control-off is safe. Raise PCLK2 or reduce
  SD clock/bandwidth if needed and document the resulting margin. Handle the
  no-CRC response status per §2.5.1 for any command path that needs it.
  **Verify:** long readback/checksummed writes concurrent with CAN and W5500;
  retain timeout/error counters and exercise card/error/abort recovery.
- [ ] **WDG-01 — initialize and freeze correctly.** Extend F423 header/debug
  selection, enable LSI and wait for readiness as needed, unlock/configure PR/RLR,
  wait for update completion, reload/start and preserve reset-cause reporting.
  Keep the valid APB1FZ debug-freeze mechanism. **Verify:** normal petting,
  intentionally starved reset and debugger halt/resume on actual F423.
- [ ] **WDG-02 — define real timeout bounds.** Calculate reload from selected
  prescaler/LSI bounds and desired timeout; do not equate raw reload with exact
  milliseconds. Account for flash stalls and flush duration. Apply the relevant
  IWDG errata if low-APB/Stop behavior is used. **Verify:** minimum/maximum
  expected reset interval and watchdog behavior during stress/power-loss tests.
- [ ] **ERR-01 — centralize RCC enable settling.** Add an appropriate readback,
  barrier or documented delay after enabling each used peripheral before its
  first register access; cover GPIO, DMA, SPI, ADC, CAN, PWR/RTC, CRC and SDIO.
  **Verify:** review every enable site against ES0372 §2.2.7 and instrument
  initial accesses in focused tests/bring-up; incidental instruction delay is
  not the documented implementation contract.
- [ ] **ERR-02 — close the errata applicability record.** Record actual silicon
  revision, enabled features, relevant ES0372 sections, implemented workaround
  IDs and evidence. RTC DBP/calendar/INIT links to RTC-02/03; SPI to SPI-05;
  SDIO to SDIO-02. SPI1+SDIO are both APB2 and do not alone establish §2.2.11's
  AHB-peripheral DMA2 scenario. Mark it not triggered for that configuration
  and reassess if QUADSPI/FSMC/GPIO DMA is introduced. Review core limitations
  and newly enabled modes instead of marking all peripheral errata cleared.
  **Verify:** no enabled-feature limitation lacks a resolution or explicit gate.

## L. Audit coverage and resolution policy

This table is the review checklist for keeping the audit and implementation plan
in sync. Each row includes compatibility notes as well as defects; a valid
interface is a constraint to preserve. The links include deferred follow-ups;
only the minimum compatibility work in K is required for this migration.

| Audit section / notes | Resolution plan / follow-up IDs |
| --- | --- |
| Compile evidence; F407-only guards; unused code still compiled | A, GPIO-01, ADC-01, UART-01, VAL-01 |
| RCC limits, PLL ranges/PLLR, search, transition order/errors, zero SW/SWS, VOS/latency, PLLQ/selectors, HPRE caches, HSE mode/frequency | RCC-01–06; BOARD-01; SDIO-01 |
| GPIO missing bank/aliases, reusable registers, package/macros, PB4 naming, electrical comments/bounds | GPIO-01–03; BOARD-01–03 |
| ADC common block/single instance, sequence/mode/clock validation, DMA route, stabilization | ADC-01–03; DMA-02; ERR-02 |
| SPI DFF/enable order, rate/zero request, instance limits, IRQ/null RX/flags, polling/CS/NULL arithmetic, valid DMA routes | SPI-01–05; DMA-01/04; BOARD-02 |
| DMA engine/four-bit CHSEL, destructive EN poll/stop, descriptor/count bounds, mem-to-mem mismatch, missing HTIF | DMA-01–04 |
| USART SR/DR/fallback, UART5 channel 8, supported set/array bounds, USART6/SPI2 IRQ conflicts, stream7 flags, baud/RX/IDLE, stop bits and CTSE/RTSE | UART-01–04; DMA-02/04 |
| Flash geometry/fallback, keys/PSIZE, alignment/range/overflow, busy/errors/cleanup, buffered status/cache, non-atomic U64 | FLASH-01–04; B |
| RTC LSI/LSE, seven-bit prescaler/OR reset state, snapshot/decimal-vs-BCD, retained source/INIT/DBP/WPR/GPS | RTC-01–03; BOARD-01/03 |
| PWM instances/timer clocks/TIMPRE, channels/BDTR, bounds/update event | PWM-01–03 |
| CAN1/2 shared-filter compatibility, PCLK timings, CAN3 limitation, absent timeouts | CAN-01–04; TOPO-01–03 |
| CRC polynomial/seed/reset/word ordering and fallback | CRC-01 |
| SDIO route/dividers/kernel clocks/IRQ ownership/flow-control errata | SDIO-01/02; RCC-05; DMA-04; BOARD-02 |
| Watchdog APB1FZ compatibility, PR/RLR completion, LSI-derived timing | WDG-01/02 |
| RCC delay, RTC errata, SDIO underrun, SPI timing, conditional DMA2 corruption, other core/peripheral limits | ERR-01/02 plus linked RTC/SPI/SDIO items |
| Syntax checks are not runtime sign-off; source/binary/header and board evidence | VAL-01–03; M |

For modes unused by migrated targets, preserve existing behavior and record
hardware testing as not exercised. Do not add rejection paths or remove features
to close audit notes. Inherited defects remain documented follow-ups; implement
only a demonstrated compatibility/runtime blocker with the smallest coherent fix.
All F4 paths select F423; retaining F407 compatibility is outside migration scope.

## M. PR sequence and final verification

The user has assigned the board-pin implementation to a separate stacked PR.
The migration plan therefore distinguishes changes that can be reviewed before
pinout integration from final new-board acceptance:

1. **Core migration work:** start with the next-step F407 removal item in A
   (section I and its A–C replacements), then complete the remaining
   compatibility work in K's scope table,
   for every F4 target, including `f4_testing`. Keep temporary example pin maps
   out of any claim that
   the binary is usable on the supplied board.
2. **Three-bus integration work:** the required portions of F and CAN-01/02/04,
   including host/log compatibility only where the third bus requires a change.
   It may be a dedicated PR in the stack, but must be ready before
   enabling all three physical interfaces on the new board.
3. **Separate stacked pinout PR:** BOARD-01/02, the pin/IRQ routing portion of
   BOARD-03, and the table in D, plus board configuration/initialization and
   final resolved controls. Preserve existing power-loss/SD behavior while adapting its concrete pin mapping;
   add behavior only when required by the new hardware. Rebase on the core
   changes and the required CAN3/protocol interfaces. Document temporary F423
   example maps separately from the actual DAQ schematic.
4. **Integration evidence:** final target build/map/debug review and real-board
   measurements after the stack is combined. Documentation tasks H/I accompany
   the changes they describe; cleanup does not substitute for acceptance.

- [ ] **VAL-01 — build and inspect supported configurations.** Compile/link all
  eleven PHAL modules with real F423 selection, instantiate DMA/UART/SPI macros,
  regenerate CAN source, and build the DAQ stack, F423 `f4_testing` and existing
  G474 targets. Verify exactly one device macro, correct startup/vector entries,
  exclusive IRQ owners, SVD/device selection and OpenOCD flash identification.
  Run focused checks for the compatibility changes selected by K's scope table;
  existing cppcheck suppression and G4 FIL tests cannot stand in for these.
  **Verify:** attach build/check commands, results, generated-source changes,
  map/vector inspection and debugger identification for the actual target set.
- [ ] **VAL-02 — account for memory and CPU headroom.** Inspect the final map
  for 1536 KiB flash/320 KiB unique SRAM, no double-counted SRAM2 alias, correctly
  initialized data and aligned DMA buffers. Include SPMC frame storage 98,304 bytes,
  metadata, explicitly declared task stacks 9472 bytes, kernel stacks/TCBs/queues,
  filesystem and C-library/main/IRQ-stack headroom. Recompute if frame format
  changes. Enable an appropriate stack high-water/overflow observation method;
  measure at the chosen CPU rate with simultaneous three-bus load and slow SD.
  **Verify:** record remaining flash/SRAM, per-task minimum free stack, overflow
  counters and measured throughput; set and justify acceptance margins before
  calling the selected memory/clock configuration sufficient.
- [ ] **VAL-03 — record reproducible acceptance results.** Run the pin/control,
  clock, CAN, SPI/W5500, SDIO, RTC/GPS, power-loss and watchdog checks attached to
  each work item. Include malformed input, timeout, bus/card disconnect, error
  and restart cases, and hardware/software CRC or reserved-area flash checks
  where exercised. Save configuration, silicon revision, instruments/captures,
  observed rates, drops, memory/stack margins and remaining unsupported modes.
  Close a work item only with its stated evidence; no unchecked board-dependent
  behavior or two-bus-only result establishes full new-board migration success.
  **Verify:** provide an acceptance matrix linking every enabled path/work item
  to a passing result, a documented unsupported-mode policy, or an unresolved
  issue that blocks the corresponding completion claim.

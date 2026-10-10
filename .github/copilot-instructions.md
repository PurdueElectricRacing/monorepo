# PER Monorepo Review Instructions

This repository contains the embedded and off-car software for PER's FSAE electric vehicle.

These review checks capture the essence of consistent, safe, and maintainable code. They are not comprehensive. Apply the underlying principles to the changed code and its consumers, including cases not listed here. Explicit coding prohibitions still apply.

## Repository Map

- `firmware/source/`: STM32 vehicle nodes
- `firmware/common/`: shared C components, drivers, and HALs
- `firmware/can_library/`: CAN runtime, codecs, and generated CAN/fault code
- `firmware/external/`: third-party and vendor dependencies
- `daq/`: Rust Cargo workspace with:
  - shared CAN/processing logic in `daqcore`
  - desktop UI in `daqapp`
  - CLI presentation in `daqcli`
- `generators/`: Python CANpiler, fault, and DearUnits generators, their configurations, and templates
- `plot_factory/`: Python telemetry analysis and plotting pipeline
- `docs/`: shared documentation, including `docs/code_style.md`
- `tests/`: C/C++ host-test harness and cross-project integration support. Python tests also live alongside generator and DearUnits code

## Build System Context

- Firmware uses CMake and `arm-none-eabi-gcc` for STM32 Cortex-M (G4, F4) targets.
- Firmware is C23: `bool`, `nullptr`, `constexpr`, and `static_assert` are valid.
- DAQ is a Rust Cargo workspace. Firmware allocation and C naming restrictions do not apply to Rust or Python host tools.
- CAN, fault, unit, and DBC artifacts are generated from `generators/` inputs. Check fixes in configurations, generator logic, or templates rather than hand-edited generated outputs.

## Code Standards

@docs/code_style.md

- Review against `docs/code_style.md`, applying the appropriate language rules.
- Dynamic memory allocation is forbidden in embedded firmware, including FreeRTOS objects. Check stack and buffer bounds.
- Rust `.unwrap()` and `.unwrap_err()` are forbidden, including in tests/examples. Production `.expect()`/`.expect_err()` must be exceptional and require a code-established invariant, nearby justification, and an explanatory message. Tests may use descriptive `expect`/`expect_err` calls for setup and assertions. Only safe Rust is allowed; no `unsafe` blocks.
- Check that types enforce units, valid states, ownership, and API contracts where possible, rather than relying only on runtime checks or comments.
- Check that changed behavior has relevant tests and updated in-repo documentation.
- Ground findings in changed code and affected consumers; explain the concrete failure or regression rather than reporting speculative issues.

## Documentation and Instruction Drift

- For repository restructuring, moved or renamed modules, or changes to subsystem boundaries, toolchains, and generated outputs, check that affected documentation (including AI/review instructions) are updated in the same PR.
- Verify directory maps, documented paths, links, include references, and examples against the resulting tree and declared generated outputs. Flag stale guidance even when the code itself still builds.
- Check this file, `docs/code_style.md`, `.gitar/review/monorepo.md`, and affected component guides. When conventions change, keep repeated rules consistent across these documents.
- Use the current code, manifests, and build configuration to establish repository structure and tooling; do not assume the directory map here is still accurate.

## Firmware: Real-Time Constraints

- Code must be deterministic
- Avoid blocking calls unless explicitly intended
- Minimize ISR execution time

## Firmware: Concurrency Rules

- Make sure to evaluate interrupt priority inconsistencies that could lead to priority inversion
- Make sure to evaluate for deadlock potential when using multiple locks or critical sections
- Make sure to evaluate for race conditions when accessing shared resources
- Assume preemption between:
  - Main loop
  - FreeRTOS tasks
  - Interrupt handlers
  - DMA callbacks
- Shared variables between ISR and main context must:
  - Use `volatile` where needed for hardware or asynchronously updated values
  - Be protected using appropriate atomic operations or critical sections where needed. `volatile` alone does not provide atomicity or synchronization

## Rust: Concurrency Rules

- Check worker/UI state ownership and channel disconnection, cancellation, and shutdown.
- Keep blocking I/O and expensive processing outside UI updates. Check lock ordering and avoid holding locks across blocking work.

## Data and Cross-Project Compatibility

- Check CAN IDs, bus mappings, payload sizes, signedness, byte order, units, and scale/offset across generators, firmware, and DAQ consumers.
- Check bootloader compatibility across resident firmware, applications, package manifests, and the DAQ updater: board names, addresses, image size/padding, CRC, CAN IDs, and protocol handshakes.
- Check telemetry time alignment, derived signals, and missing/non-finite data in decoding and plotting. Check numeric narrowing and lossy conversions.
- Check runtime file paths against the DAQ package working directories, including settings, logs, themes, HIL presets, and generated DBC files.

## Errors

- Do not silently ignore faults or errors. Check pointer contracts and safe behavior when sensors, communications, or peripherals fail.
- Handle user input, files, devices, and channel failures recoverably. Do not replace error handling with panics or unjustified `expect` calls.
- Defaults, `.ok()`, and ignored results must not conceal failures. Meaningful fallbacks and documented best-effort cleanup are allowed.

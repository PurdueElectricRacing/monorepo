# BMS implementation plan: five 29-series modules

## Objective

Support five physical battery modules, each containing two ADBMS chips and 29 series cells. Keep the implementation simple, readable, and maintainable by separating physical module numbering from chip communication order and retaining one copy of each measurement.

This document is an implementation plan; it does not change firmware behavior.

## 1. Hardware model and assumptions

| Quantity | Per physical module | Whole pack |
| --- | ---: | ---: |
| Physical modules | 1 | 5 |
| ADBMS chips | 2 | 10 |
| Connected series cells | 29 | 145 |
| Raw cell measurement channels | 32 | 160 |
| Unused cell channels | 3 | 15 |
| Populated thermistors, assumed | 20 | 100 |

Use the following interpretation of the supplied schematic:

| Chip role | Physical cells, one-based | Chip channels, one-based | Unused channels |
| --- | --- | --- | --- |
| A | 1–16 | 1–16 | None |
| B | 17–29 | 1–13 | 14–16 |

The lower chip is drawn upside down. Its numbering still begins at channel 1, which corresponds to physical cell 17. Its upper three cell channels and corresponding S connections are shown shorted. These are absent cells, not additional zero-volt cells in the pack.

Assume all ten GPIO inputs on each chip are populated with thermistors, as requested. Define module thermistors 1–10 as chip A GPIO1–10 and module thermistors 11–20 as chip B GPIO1–10. This is a logical numbering convention; it does not claim that thermistor locations correspond one-to-one with cell locations.

Assume one daisy chain contains all ten chips. Confirm the complete isoSPI route, physical module order, and A/B receive order against the full schematic and hardware. The supplied crop shows the between-chip link but does not establish the complete pack chain order.

Before hardware acceptance, also confirm the actual part number and the 13-cell input/filter implementation against the local [ADBMS6830B datasheet](datasheets/ADBMS6830B_public.pdf). The current source spells the driver `6380`; retain existing identifiers during this functional change to avoid mixing a broad mechanical rename into it. Record the discrepancy in the driver documentation.

## 2. Current behavior and affected areas

| Area | Current implementation | Required change |
| --- | --- | --- |
| Topology | `ADBMS_MODULE_COUNT = 7` in `adbms_config.h` | Distinguish five modules from ten chips |
| Device storage | `dev.modules[]` holds one entry per chip | Rename to `dev.chips[]` and clarify the type/comments |
| Transport | Buffer lengths, wake loops, PEC checks, and configuration loops use module count | Use chip count throughout |
| Cell acquisition | Six commands read 16 channels per chip | Preserve the raw protocol and channel count |
| Cell statistics | Every raw channel contributes | Include only 145 connected cells |
| Voltage validity | Pack extrema outside 1–5 V trigger recovery | Exclude absent channels; preserve checks on connected cells |
| Balancing | All 16 channels per chip are candidates | Clear all flags, then select only connected cells |
| Thermistors | Ten GPIO conversions per chip | Keep conversion; aggregate all 100 thermistors |
| Public getters | Module index currently selects a chip | Make module index select a physical module |
| CAN telemetry | Rotates through chip-sized groups | Publish five groups of 29 cells and 20 thermistors |
| DAQ battery widgets | Seven groups of 16 cells and ten thermistors | Display five groups of 29 cells and 20 thermistors |
| Pack thresholds | Full/empty thresholds and charging limit are absolute pack voltages | Review against the new pack specification |

Relevant source files:

- `firmware/source/a_box/adbms/adbms_config.h`
- `firmware/source/a_box/adbms/adbms6380.h` and `.c`
- `firmware/source/a_box/adbms/adbms.h` and `.c`
- `firmware/source/a_box/telemetry/telemetry.c`
- `firmware/source/a_box/main.c`
- `firmware/source/a_box/charging_fsm/charging_fsm.c`
- `generators/configs/nodes/A_BOX.json`
- `daq/daqapp/src/ui/battery/common.rs`
- `daq/daqapp/src/ui/battery/battery_voltage.rs`
- `daq/daqapp/src/ui/battery/battery_temps.rs`

## 3. Design decisions

### 3.1 Give each count one meaning

Keep topology definitions in `adbms_config.h`, using typed `static constexpr` values in the existing C23 style:

| Proposed constant | Value or derivation |
| --- | --- |
| `ADBMS_MODULE_COUNT` | 5 physical modules |
| `ADBMS_CHIPS_PER_MODULE` | 2 |
| `ADBMS_CHIP_COUNT` | Module count × chips per module = 10 |
| `ADBMS_CHIP_A_CELL_COUNT` | 16 |
| `ADBMS_CHIP_B_CELL_COUNT` | 13 |
| `ADBMS_CELLS_PER_MODULE` | A count + B count = 29 |
| `ADBMS_PACK_CELL_COUNT` | Module count × cells per module = 145 |
| `ADBMS_THERMISTORS_PER_CHIP` | 10 |
| `ADBMS_THERMISTORS_PER_MODULE` | Chips per module × thermistors per chip = 20 |
| `ADBMS_PACK_THERMISTOR_COUNT` | Module count × thermistors per module = 100 |

Retain `ADBMS6380_CELL_COUNT = 16` and `ADBMS6380_GPIO_COUNT = 10` as hardware limits in the low-level header. Do not change the hardware channel count to 13 or 29. Assert that the topology's active counts fit the hardware limits and that the assumed thermistor count matches the GPIO count. Put assertions requiring both headers in the high-level header or implementation to avoid circular includes.

Derive totals instead of repeating numeric literals. Replace existing uses of `ADBMS_MODULE_COUNT` according to their actual meaning; a global textual replacement would be incorrect.

### 3.2 Keep chip storage as the only measurement storage

Rename the low-level per-device type from `adbms6380_module_t` to `adbms6380_chip_t`, and its array from `modules[]` to `chips[]`. Each entry retains 16 cell voltages, ten GPIO voltages, ten temperatures, discharge flags, cached configuration, and mismatch flags.

Do not introduce another physical-module array containing copies of measurements. Public getters translate physical module/channel indices into the existing chip arrays. This avoids synchronization code and additional RAM.

Use `chip_idx` for transport/storage indices and `module_idx` for physical modules. Apply this distinction to comments, local names, buffer descriptions, and read/write ordering explanations.

### 3.3 Centralize physical-to-chain mapping

Use one small constant table in `adbms_config.h`, indexed by physical module and chip role A/B, whose entries are raw receive-order chip indices. Name it clearly, for example `ADBMS_MODULE_CHIP_INDICES`. Use named A/B roles rather than unexplained column numbers.

An initial table can contain `{0, 1}, {2, 3}, {4, 5}, {6, 7}, {8, 9}`, explicitly marked as an assumption until verified. If hardware order differs, update this table without changing public numbering, protocol code, or telemetry loops. Each chip index must occur exactly once; verify this invariant in a focused test.

Add only the small inline mapping helpers needed by the getters and aggregation code. Keep these helpers independent of SPI/RTOS headers so mapping can be tested directly. Avoid a runtime topology registry, dynamic allocation, function-pointer callbacks, or a generic battery configuration framework.

For zero-based public indices:

| Public input | Chip role | Raw channel index |
| --- | --- | --- |
| Cell `0..15` | A | `cell_idx` |
| Cell `16..28` | B | `cell_idx - ADBMS_CHIP_A_CELL_COUNT` |
| Thermistor `0..9` | A | `therm_idx` |
| Thermistor `10..19` | B | `therm_idx - ADBMS_THERMISTORS_PER_CHIP` |

All public module indices are `0..4`. Document these bounds as caller preconditions, consistent with the current getter interface. Ensure every in-repository caller enforces them through named loop limits. Do not return a plausible zero reading for an invalid index.

## 4. Implementation sequence

### Step 1: Introduce topology and clarify device terminology

1. Add the constants, chip-role names, mapping table, and pure mapping helpers described above.
2. Rename per-chip storage/types and local transport variables to use chip terminology.
3. Audit every `ADBMS_MODULE_COUNT` use, including documentation, and assign it either physical-module or chip-count semantics.
4. Update any direct accesses to `g_bms.dev` or low-level types. Search the entire repository, not only the driver directory.

Deliverable: the distinction between physical modules and communication chips is explicit, and no transport operation uses physical module count.

### Step 2: Make transport cover ten chips

In `adbms6380.h` and `.c`, use `ADBMS_CHIP_COUNT` for:

- The chip-state array and TX/RX buffer sizes.
- Wake pulse count.
- Response length and per-chip PEC checking.
- Configuration write/read-back loops and mismatch checks.
- Cell/GPIO readout decoding and thermistor conversion loops.

Preserve the existing reverse configuration-write ordering relative to receive order. Do not reverse the raw array to match the physical module numbering; the mapping table handles that distinction.

With the current framing, a complete configuration write requires `4 + 10 × 8 = 84` TX bytes, and a register response requires `10 × 8 = 80` RX bytes. Check these sizes against the static buffers, `strbuf` capacity, and SPI length type. Cell and auxiliary command-group counts remain six and four respectively. PEC remains associated with every chip packet, including packets containing unused cell channels.

Deliverable: all ten devices participate in existing acquisition, configuration verification, and recovery behavior without changing the wire protocol.

### Step 3: Update public getters and pack statistics

1. Keep the public getter names and signatures, but define their module argument as a physical module index.
2. Map cell voltage and discharge-status getters through the same cell mapping helper.
3. Map thermistor getters through the thermistor mapping helper.
4. In `read_cells()`, loop over physical modules, reduce 16 values from that module's A chip and 13 from its B chip, and combine the results. Reuse the existing small reduction helper rather than adding a configurable iterator framework.
5. Initialize extrema from the first connected cell. Divide the pack sum by `ADBMS_PACK_CELL_COUNT` for the average.
6. In `read_therms()`, include all ten temperatures on each of ten chips, and divide by `ADBMS_PACK_THERMISTOR_COUNT`. No thermistor population mask is needed under the all-populated assumption.

Keep all raw channels available internally for diagnosis. Their presence in the raw arrays must not make unused channels contribute to physical-cell statistics, voltage validity checks, or telemetry.

Keep the current connected-cell plausibility checks, PEC flags, retry policy, and recovery/fatal state machine. In particular, do not weaken the minimum-voltage check to accommodate shorted unused inputs. Existing failed-read behavior retains previous command-group samples; document this behavior rather than claiming this topology change adds freshness guarantees.

Deliverable: shorted B channels cannot force a false recovery, and every connected cell and populated thermistor contributes exactly once.

### Step 4: Restrict balancing to connected cells

Use a straightforward clear-then-select algorithm:

1. Clear discharge requests for all 160 raw channels at the start of each balance calculation.
2. Return immediately if balancing is disabled or the existing minimum-voltage condition is not met.
3. Iterate the 145 physical cells and set flags through the shared cell mapping when their voltage exceeds the existing balance threshold.

This guarantees the unused channels stay false even if old state previously set them. Keep the 16-bit register packing format unchanged.

Before each high-level REGB write, ensure unused discharge requests are cleared using the same topology mapping. A short private helper shared by the connecting and read-loop paths can enforce this invariant without another stored channel mask. Initialization already clears all requests; preserve that behavior. The low-level register encoder should remain a chip-protocol routine.

Verify emitted configuration bytes, not just the in-memory flags: on every B chip, DCC14–DCC16 must be zero. Connected channels A16 and B13 must still be independently selectable. Preserve existing balancing thresholds and enable conditions.

Deliverable: absent cells can never be selected for balancing through the high-level BMS API.

### Step 5: Publish physical module telemetry

In `telemetry.c`:

- Cycle cell telemetry through module `0..4`, cell `0..28`.
- Cycle thermistor telemetry through module `0..4`, thermistor `0..19`.
- Use the updated public getters; do not duplicate chip mapping arithmetic in telemetry.
- Preserve the current message rates, scale factors, balancing field, and publication on both vehicle and charging buses.

The corresponding fields in `generators/configs/nodes/A_BOX.json` are already `uint8_t`, so the new indices fit without changing payload widths or CAN identifiers. Update descriptions for both normal and `_ccan` messages to state that module numbering is physical and identify the valid channel ranges. Regenerate artifacts through `generators/generate.py`; do not hand-edit generated CAN headers, DBCs, or SuperDBC output.

At existing rates, each bus receives a complete cell sweep in approximately `145 / 100 = 1.45 s` and a thermistor sweep in `100 / 8 = 12.5 s`. These are individual-channel publication intervals, not the BMS acquisition period. The BMS task still updates aggregate fault inputs at its existing 200 ms period, subject to measured execution time.

Document the semantic change: old logs and old firmware use chip-sized module numbering; the updated consumer expects physical modules. Deploy matching firmware and DAQ changes together. Historical topology autodetection is outside this change.

Deliverable: exactly 145 cell slots and 100 thermistor slots are published per sweep, with no unused cell slots.

### Step 6: Update DAQ consumers

In `daq/daqapp/src/ui/battery/common.rs`, set the existing constants to five modules, 29 cells per module, and 20 thermistors per module. Keep the small Rust constant group with a comment linking to the firmware topology and this plan. A cross-language configuration generator is unnecessary for this fixed configuration.

Audit the voltage and temperature widgets for array sizes, index checks, completion counts, labels, and layout assumptions. Their current reconstruction routines already use the shared constants; update only remaining assumptions that actually need changes. Check that 29 cell tiles and 20 temperature tiles remain usable at supported window sizes.

Verify that retained/queryable frame history covers a complete 12.5 s thermistor sweep with margin. A short capture may legitimately show unfilled slots; avoid filling them with zero values. Keep the existing distinction between the age of the latest message and the completeness of a multiplexed scan: the shared one-second stale timeout currently describes latest-message age. Do not claim that a live stream proves every channel is fresh, and do not simply raise this shared timeout to 12.5 s. Add per-channel freshness only if explicitly taken up as a separate requirement.

Deliverable: the app reconstructs and displays all new physical slots and rejects out-of-range module/channel indices.

### Step 7: Review pack-level limits and document integration

Audit these existing values against the approved pack specification:

- `PACK_FULL.max = 470 V` and `PACK_EMPTY.min = 302 V` in `generators/configs/nodes/A_BOX.json`.
- `MAX_PACK_CHARGING_VOLTS = 540 V` in `charging_fsm.c`.
- Any DAQ charging defaults, request limits, and pack-voltage range assumptions found by repository search.

Determine whether each limit is a chemistry-derived pack threshold or an independent charger/hardware limit. Use the new 145-series count when deriving pack thresholds from approved per-cell limits. Do not scale hardware limits automatically or infer new thresholds from topology alone. Record the chosen values and rationale before hardware acceptance; if the approved values are unavailable, leave that item explicitly unresolved.

Update `adbms/README.md` with the topology table, physical public indices, raw chip storage, mapping-table location, thermistor assumption, telemetry sweep times, and hardware order verification. Update `a_box/README.md` and affected DAQ documentation where their descriptions change.

## 5. Validation plan

### 5.1 Focused host tests

Add meaningful behavioral tests under `firmware/source/a_box/adbms/tests/`, registered through the existing `add_firmware_unit_test` helper and `tests/CMakeLists.txt`. Compile production C as C23; use the repository's C shim pattern where C++20 cannot include C23-only headers. Keep SPI/RTOS stubs small and local to the tests; do not add a general mocking framework or expose private implementation functions solely for testing.

| Behavior | Test and expected result |
| --- | --- |
| Mapping boundaries | Check physical cells 0, 15, 16, and 28 and thermistors 0, 9, 10, and 19 in first and last modules against independently specified expected chip/channel pairs |
| Mapping coverage | Every valid physical cell maps uniquely; every raw chip belongs to one physical module; no valid cell maps to B channels 14–16 |
| Pack statistics | Fill active channels with 3.7 V and unused channels with zero: min/max/average 3.7 V, sum approximately 536.5 V |
| Unused-channel exclusion | Put extreme values in unused channels; physical-cell statistics and balance choices remain unchanged |
| Last connected channel | Change the last module's B13 value and verify that min/max/sum, getter, and balance selection reflect it |
| Connected-cell fault | A genuine invalid connected-cell voltage still invokes the existing recovery behavior |
| Thermistor coverage | Give all 100 thermistors distinct temperatures; verify extrema and mean include the final B GPIO10 |
| Balancing | Seed all discharge flags true, recalculate, and inspect encoded REGB: unused bits are clear, while eligible A16/B13 can be set |
| Balancing disabled | All raw discharge requests are cleared when disabled or below the existing minimum balance voltage |
| Transport ordering | Use distinct per-chip responses/configurations to verify all ten packets and the existing reverse write order |
| PEC handling | A corrupt packet from the tenth chip fails the existing command read/retry path |
| Telemetry sweep | Capture both buses over complete sweeps: all 145/100 slots occur once before wrap, all indices are in range, and boundary values match the raw chip data |

Prefer testing through existing public entry points with stubbed acquisition and CAN capture. If reaching a behavior requires substantial unrelated hardware emulation, extract only the small production helper with a clear responsibility rather than reproducing implementation logic inside the test.

In Rust, add focused reconstruction tests using decoded telemetry for module 4/cell 28 and module 4/thermistor 19, complete sweeps, and invalid indices. Verify missing slots remain unknown. Reuse existing test fixtures where available.

### 5.2 Build and repository checks

Run the repository workflows after implementation, from the repository root:

```sh
python3 tests/run_tests.py
python3 firmware/build_firmware.py
python3 firmware/check_firmware.py
cargo test --manifest-path daq/daqapp/Cargo.toml --locked
cargo build --manifest-path daq/daqapp/Cargo.toml --locked
```

The test runner already runs generation and the host test suite. The firmware builder performs a clean build; avoid redundant repeats once checks pass. Run the existing FIL scenarios against rebuilt firmware as an integration regression when the FIL toolchain is available; FIL is not a substitute for validating the actual ten-chip daisy chain.

Review A_BOX RAM/flash usage and task stack headroom. Measure the BMS cycle, including ten-chip wake delays and normal/worst retry traffic, against its 200 ms task period. Change timing only if measurements show a problem, and document the reason.

### 5.3 Hardware acceptance

1. Verify full chain order and update the module-to-chip table. Identify chips with distinct known channel inputs or existing device diagnostics; repeated equal voltages cannot establish ordering.
2. Confirm each module's cell 16/17 boundary and cell 29, plus every module boundary, against an independent measurement.
3. Confirm unused B channels are shorted as intended and cannot affect pack statistics or produce a false software undervoltage/recovery.
4. Confirm all 100 thermistor channels are populated, use the expected divider/conversion, and appear under the intended logical module/channel numbers.
5. Exercise selected balancing channels A16 and B13 and verify unused discharge bits remain zero in the written registers.
6. Compare the 145-cell software voltage sum with an independent pack voltage measurement within the expected measurement accuracy.
7. Verify real connected-cell faults, configuration/communication failures, and resulting BMS fault/shutdown behavior retain their intended operation.
8. Capture both CAN buses for longer than a thermistor sweep; verify physical indices and complete DAQ reconstruction.
9. Confirm the reviewed pack/charger limits before charging acceptance.

## 6. Scope boundaries

Keep this implementation focused on the new topology. Preserve the existing state machine, retry policy, thresholds unless reviewed above, and basic thermistor conversion. Do not add runtime topology selection, old-pack compatibility modes, duplicate module measurement caches, a generic iterator framework, or a broad chip-name migration.

The current temperature path starts ADAX and immediately reads auxiliary registers; the source already notes missing conversion-completion polling. Treat that as an existing limitation, record it during acceptance, and handle any required correction separately rather than silently claiming this plan resolves it. Likewise, this change does not redesign sample freshness, concurrency, or fault latching. A discovered issue that prevents correct operation must be resolved or explicitly tracked before acceptance.

## 7. Suggested review units and completion criteria

Implement in three reviewable units, keeping each unit buildable:

1. **Firmware topology and behavior:** constants, mapping, ten-chip transport, physical getters, statistics, balancing, telemetry, and focused firmware tests. Make the count transition atomically within this unit so no intermediate version interprets five physical modules as five chips.
2. **Consumers and configuration:** DAQ dimensions/reconstruction tests, CAN descriptions and regenerated outputs, and pack-limit changes supported by the approved specification. Coordinate deployment with the firmware unit.
3. **Documentation and acceptance evidence:** final chain mapping, hardware verification results, timing/resource measurements, and updated driver/A_BOX/DAQ documentation. Documentation can accompany earlier units where helpful.

The work is complete when:

- Ten chips communicate successfully through the confirmed chain mapping.
- Five physical modules expose exactly 29 cells and 20 thermistors each.
- All physical cells and thermistors contribute exactly once to statistics and telemetry.
- No unused B cell channel contributes to cell validity, statistics, telemetry, or a discharge command.
- Existing fault behavior still responds to genuine connected-cell and communication failures.
- Matching DAQ software displays all 145 cells and 100 thermistors.
- Pack limits are reviewed, required tests/builds pass, and hardware/timing acceptance evidence is recorded.

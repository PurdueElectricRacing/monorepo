# PER firmware-in-the-loop tests

FIL is pinned at `tests/external/fil` as a Git submodule. Initialize it with:

```sh
git submodule update --init --recursive tests/external/fil
```

These tests execute the six real vehicle boards without attached hardware
(`g4_testing` is deliberately excluded). Board and network configs use paths
relative to their JSON files, so they work regardless of the current directory.
The MCU model comes from FIL; standalone ELF files come from `firmware/output`
and start at `0x08000000`.

## Scenarios and generated stimuli

`tests/fil/scenarios/*.json` holds the maintained test behavior. Each CAN
entry has a logical bus, a CAN message, and signals with physical values or
enum choice names:

```json
{"type": "can", "bus": "MCAN", "message": "INVA_INFO", "at_ms": 500,
 "repeat": {"every_ms": 500, "count": 13},
 "values": {"AMK_Status_bSystemReady": true, "AMK_Status_bQuitDcOn": true}}
```

`raw: [bytes]` supplies the payload bytes instead of named signal values.
For both CAN events and expectations, signals omitted from `values` leave their
wire bits at zero.

The `canpiler` FIL pass resolves each scenario into
`tests/fil/stimuli/{stem}_stimuli.json` in the raw FIL stimulus format (buses as
ids, plus ids and packed payloads). Message ids come from canpiler,
and values are packed with the same scale/offset, enum, sign, and byte-order
rules as the generated firmware macros.

`tests/fil/network.json`, `tests/fil/ready_to_drive.json`,
`tests/fil/bootloader_update.json`, and
`tests/fil/bootloader_update_incomplete.json` are
hand-maintained FIL networks. The scenario-driven networks reference generated
stimuli, which are not committed, so run the canpiler generator once on a fresh
checkout before `run_tests.py`:

```sh
python3 generators/generate.py
```

## Build and run

Requirements: built PER firmware, Python 3.11+, Make, CMake, a C++20 Clang
compiler, and matching `llvm-profdata`. FIL's upstream tests also need
GoogleTest (CMake can fetch it). Release builds use IPO where supported and
host CPU tuning; the resulting executable targets the build machine.

```sh
python3 firmware/build_firmware.py
python3 tests/fil/run_tests.py --build
```

The build invokes FIL's full `make pgo` workflow: instrumented build, one second
of six-board network training with strict MMIO and full ADC fidelity, profile
merge, optimized build, and FIL's own CTest suite. Outputs stay in FIL's ignored
`build-pgo-generate/` and `build-pgo/` directories, as required by its cleanup
safety guard. Set `PGO_CXX` and
`LLVM_PROFDATA` in the environment to select tools from the same LLVM release.
`CMAKE_BUILD_PARALLEL_LEVEL` controls build concurrency.

Run just the PER scenarios using the existing PGO executable:

```sh
python3 tests/fil/run_tests.py
# Or use another FIL build:
python3 tests/fil/run_tests.py --fil /path/to/fil
```

The runner fails on simulation errors, early stops, or unmet stimulus
expectations. It discovers every network config in this directory (a JSON file
with `boards` and `buses`), derives a run duration from the scenario's stimuli,
and runs each one. It checks:

- All six boards start independently and reach 10 ms with strict MMIO.
- Each network reaches the time budget with a successful exit. Only
  `bootloader_update.json` may return FIL runtime error 4 for a dashboard reset:
  it must report no other board stops or stderr diagnostics besides the
  expected network board-failure summary. FIL does not restart that board.
- `network.json` (no expectations) reaches 20 ms and carries
  firmware-originated CAN TX/RX.
- `ready_to_drive.json` satisfies the ready-to-drive stimulus expectations
  (initial zero torque commands, later drive commands, and vehicle state).
- `bootloader_update.json` reprograms the resident bootloader region from a
  running application through its dedicated update command set.
- `bootloader_update_incomplete.json` rejects a premature CRC even when the
  unsent trailing words are erased-state bytes matching the CRC, refuses JUMP,
  and locks the aborted transfer.

The update scenarios use small synthetic images (a stack pointer, a reset
vector, and a one-instruction loop) rather than the packaged firmware, so they
exercise the protocol and flash path without depending on application contents.
Their expectations cover the `ACK(size)` and `ACK(CRC)` responses; FIL reports a
reset request after the application-side `JUMP`.

The `run_tests.yml` GitHub Actions workflow also runs the full PGO build and PER
scenarios on pull requests targeting `master` and pushes to `master`. Its FIL
job uses the existing firmware container, installs the PGO tools, and builds
vehicle firmware before running the tests, independently of host unit tests.

`python3 build_all.py` builds firmware, builds/tests FIL with PGO, then builds
DAQ and runs the existing host-test workflow. The standalone host-test runner
remains independent and does not require FIL or prebuilt vehicle firmware.

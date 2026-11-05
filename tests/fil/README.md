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

`tests/fil/network.json` and `tests/fil/ready_to_drive.json` are
hand-maintained FIL networks. The latter references its generated stimulus,
which is not committed, so run the canpiler generator once on a fresh checkout
before `run_tests.py`:

```sh
python3 generators/generate.py
```

## Build and run

Requirements: built PER firmware, Python 3.11+, Make, CMake, a C++20
compiler, and GoogleTest (CMake can fetch it). FIL's default build is tuned
for fast iteration (Ninja and ccache when available, precompiled headers, no
IPO) and targets the build machine.

```sh
python3 firmware/build_firmware.py
python3 tests/fil/run_tests.py --build
```

The build invokes FIL's default `make` target, which produces
`tests/external/fil/build/fil`. Outputs stay in FIL's ignored `build/`
directory. `CMAKE_BUILD_PARALLEL_LEVEL` controls build concurrency.

Run just the PER scenarios using the existing FIL executable:

```sh
python3 tests/fil/run_tests.py
# Or use another FIL build:
python3 tests/fil/run_tests.py --fil /path/to/fil
```

The runner fails on simulation errors, early stops, or unmet stimulus
expectations. It checks:

- All six boards start independently and reach 10 ms with strict MMIO.
- The shared network reaches 20 ms and carries firmware-originated CAN TX/RX.
- The vehicle reaches 6 seconds with the ready-to-drive stimulus expectations
  satisfied (initial zero torque commands, later drive commands, and vehicle state).

## VS Code: debug the emulator with cortex-debug

`.vscode/launch.json` has `Emulator: <board>` configurations for the six
vehicle boards, and `.vscode/tasks.json` has the matching `fil debug (<board>)`
background tasks. Both use the board configs in `tests/fil/boards`, which
resolve their `firmware/output` ELFs relative to the repository.

## CI/CD

The `run_tests.yml` GitHub Actions workflow also builds FIL and runs the PER
scenarios on pull requests targeting `master` and pushes to `master`. Its FIL
job uses the existing firmware container and builds vehicle firmware before
running the tests, independently of host unit tests.

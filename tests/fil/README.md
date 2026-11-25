# PER Firmware-in-the-Loop (FIL) Tests

FIL is pinned to a release at `tests/external/fil` as a git submodule.

## Directory layout

| Path | Purpose |
| --- | --- |
| `boards/` | Per-board FIL configs pointing at standalone ELFs in `artifacts/firmware/` |
| `scenarios/` | Maintained test scenarios using logical CAN bus, message, and signal names |
| `stimuli/` | Generated raw FIL stimuli |
| `network.json`, `ready_to_drive.json` | Multi-board network configs referencing `boards/` and `stimuli/` |
| `run_tests.py` | Builds FIL and runs the actual FIL tests |

## Running

Build firmware (which also runs `generators/generate.py`), build FIL, and run the tests:

```bash
python3 firmware/build_firmware.py
python3 tests/fil/run_tests.py --build
```

Once FIL is built, run just the tests:

```bash
python3 tests/fil/run_tests.py
python3 tests/fil/run_tests.py --fil /path/to/fil
```

## Scenarios

Add or edit JSON files in `scenarios/`. CAN events and expectations take a logical bus, message name, and physical/enum signal values (or `raw: [bytes]`). Omitted signals default to zero:

```json
{"type": "can", "bus": "MCAN", "message": "INVA_INFO", "at_ms": 500,
 "repeat": {"every_ms": 500, "count": 13},
 "values": {"AMK_Status_bSystemReady": true, "AMK_Status_bQuitDcOn": true}}
```

Run `python3 generators/generate.py` to regenerate `stimuli/{stem}_stimuli.json`.

## Updating FIL

CI (`.github/workflows/run_tests.yml`) runs against a pinned prebuilt FIL release binary instead of compiling the submodule.
When bumping `tests/external/fil`, update the release URL + SHA-256 checksum in `.github/workflows/run_tests.yml` to match.

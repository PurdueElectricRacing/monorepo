# CANpiler-FIL Integration - Master Plan

## Goal

Make the FIL (firmware-in-the-loop) configs under `tests/fil/` DERIVED artifacts of the CAN declarations in `generators/configs/`, produced by a new `filgen` generator alongside canpiler/faultgen/dearunits. The only hand-maintained test data is a small SCENARIO file containing irreducible test behavior: timing windows, GPIO/ADC device stimuli, and CAN traffic referenced by message name with signal values (not raw ids/bytes).

## Motivation

- `tests/fil/` board/network/stimulus JSON duplicates data already declared in `generators/configs/` (CAN ids via `id_override` and link-stage allocation, buses via `system/bus_configs.json`, board-to-bus wiring via node `peripheral` fields, byte payloads via signal/scale/enum definitions).
- Hand-maintained raw CAN ids and byte payloads break silently when messages are re-linked or re-laid-out. Verified examples: 0x231-0x234 (INVERTER A-D info, `id_override`), 0x111-0x114 (MAIN_MODULE inverter-set echoes, `id_override`), 0x46 (dynamically linked `main_hb`, data [5] = `car_state_t.ready2drive`).

## Scope boundaries

- NO changes to the `tests/external/fil` submodule, `firmware/`, or `daq/`.
- Python code follows the existing generator conventions (`faultgen`/`dearunits`): plain classes, pydantic models for file formats, Jinja or direct dict serialization, `Artifact` outputs, pytest unit tests picked up by the root `pytest.ini` (`testpaths` includes `generators/`).
- Shared code-style principles from `docs/code_style.md` apply: descriptive names, no magic numbers, deliberate error handling, docs kept in sync, follow existing style, no unnecessary abstraction.

## Architecture

Pipeline change in `generators/generate.py` (post-link, mirrors faultgen.generate pattern):

1. `filgen` loads `tests/fil/scenarios/ready_to_drive.json` (new hand-maintained scenario file, pydantic-validated, strict/extra=forbid like the other configs).
2. A RESOLVER turns scenario entries into concrete FIL JSON: message names -> final ids from `LinkedCan.tx_messages`/`subscriptions`, logical bus names -> FIL bus id strings from `LinkedCan.bus_configs`, signal values -> packed bytes using the same packing rules canpiler's compile stage uses (big-endian byte order per message `byte_order`, signal `bit_offset`/`bit_length`/`scale`/`offset`, enum choice names).
3. Emitted artifacts (collection "fil", root `tests/fil/`):
   - `boards/*.json` (6 boards) from node peripheral mappings + bus_configs.
   - `network.json` and `ready_to_drive.json` (network-level) from bus_configs + board list.
   - `stimuli/ready_to_drive_stimuli.json` resolved from the scenario (raw FIL stimulus schema, so the submodule is untouched).
4. `run_tests.py` keeps its file-name contract (`network.json`, `ready_to_drive.json`, `boards/{board}.json`) so CI and PGO training args keep working unchanged.

Scenario file schema (hand-maintained, the only new hand-written data):
- top level: `schema_version`, `name`, `events` (list), `expect` (list).
- CAN events/expects reference `message` (linked message name) + optional `values` (signal_name -> value, enum choices by name, scaled units); `bus` is the logical bus name (e.g. "MCAN"); timing fields (`at_ms`, `window_ms`, `repeat`) stay here.
- GPIO/ADC events stay here as-is (they are physical, not CAN, data).
- Raw escape hatch: a CAN event may alternatively specify `id` + `data` bytes directly for frames with no declaration.

## Work packages

- [ ] WP0: Baseline verification. Run `python3 generators/generate.py` on a scratch copy of `tests/fil/`; record the current resolved ids (INVA-D info 0x231-0x234, INVA-D set 0x111-0x114, main_hb 0x46) and the exact current JSON content of every file filgen will emit, as the golden reference.
- [ ] WP1: `filgen` skeleton. `generators/filgen/{__init__?no, api.py, models.py}`, add `FIL_*` path constants to `core/config.py`, wire `FilGenerator.generate(linked_can, scenarios_dir, version)` into `generate.py` with a "fil" output root. Scenario pydantic models with strict validation.
- [ ] WP2: Board + network emission. Emit the 6 board files, `network.json`, `ready_to_drive.json`. Golden diff: emitted bytes must equal the current checked-in files exactly.
- [ ] WP3: Scenario + stimulus resolver. Write `tests/fil/scenarios/ready_to_drive.json` expressing the current ready_to_drive stimuli by name; implement id/bus/payload resolution (reuse canpiler packing; if the compile-stage signal layout is the right source, use `LinkedCan` compiled messages). Golden diff: resolved stimulus JSON must equal the current checked-in `ready_to_drive_stimuli.json` exactly (same ids, same bytes, same order).
- [ ] WP4: Tests. pytest unit tests under `generators/filgen/tests/` covering: scenario validation failures (unknown message, unknown bus, bad enum choice, duplicate keys), id resolution (override + dynamic), byte packing (including a scaled signal and an enum-by-name value), and board/network emission against the golden reference.
- [ ] WP5: Docs + hygiene. Update `tests/fil/README.md` and `generators/configs/README.md` to describe the new flow (generated files vs the hand-maintained scenario). Ensure `python3 generators/generate.py` is idempotent (re-run produces identical files). Verify `pytest` from repo root passes.

## Verification (definition of done)

1. `python3 generators/generate.py` runs clean in the nix dev shell / CI container deps (pydantic, jinja2, pytest).
2. `pytest` (root pytest.ini) passes, including new filgen tests.
3. Filgen output for boards/network/ready_to_drive/ready_to_drive_stimuli is byte-identical to the pre-change checked-in files (golden diff).
4. `python3 tests/fil/run_tests.py` is unmodified and, when firmware ELFs exist, still passes (CI parity; locally at minimum: generated files parse identically to what run_tests.py loads).
5. No changes outside: `generators/`, `tests/fil/` (README, scenario, no longer-hand-maintained JSONs become generated), `docs/plans/`.
6. All commits on `millan/canpiler_fil`, not pushed.

## Check-off log

(as work proceeds, each WP gets a dated line under its checkbox)

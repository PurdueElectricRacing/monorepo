# daqcore SuperDBC decoder design

Status: implemented; workspace validation complete.

## Intention and verified baseline

Replace production `can_decode` / `can-dbc` dependencies with a transport-neutral
SuperDBC v1 database in daqcore. Load metadata once, decode without allocations,
and store owned values and database-local handles rather than strings and maps.

The current generated artifact is `dbc/superdbc_4d93f8e.json`: five buses,
145 messages, 510 signals, 14 signals with limits, 192 with choices, nine extended
messages, and a sample maximum of 16 signals / 32 bits. All 145 messages' decoding
metadata matches the DBC files generated from the same revision. The schema and
compiler additionally permit 64-bit integers and 64 one-bit signals in a frame.

Measured baseline `can_decode` allocation/reallocation calls for sampled messages
with 1, 4, and 16 signals: 6, 18, and 58. These are allocation events, not retained
memory measurements. At 10 Hz for 30 seconds, a cache holds **300** frames per
stream; total memory also depends on the number and rates of message streams.

## Database ownership, identity, and metadata

- `SuperDbc` contains a private `Arc<DbInner>`. `load_file` / `from_str` return
  `Result<SuperDbc, DbError>`; cloning shares all immutable metadata in O(1).
  No reference count operation is needed for decoding through a borrowed database.
  Frames do not carry `Arc`s or metadata references.
- Keep owned strings in database definitions for v1; a separate string table is
  unnecessary to eliminate per-frame allocations. Interning can follow measured
  need. Metadata access returns borrowed definitions / strings. UI search stores
  message IDs, cloning metadata only on an explicit selection where needed.
- Each successfully loaded database gets a process-local `DbGeneration`. Clones
  retain it; an independently loaded document gets a different generation even
  if its content hash matches. The generator hash is provenance, not chronology.
- `BusId` is the checked schema slot (0..7), never a vector index. Bus lookup maps
  the ID to the database's internal index. Message identity is `(BusId, MessageId)`;
  database-local message and signal indices are also generation-bound.
- `MessageId` has checked constructors for standard (11-bit) and extended (29-bit)
  IDs. Standard and extended IDs with the same raw number remain distinct. The
  legacy parser flag representation uses bit 31 only at explicit boundaries.
- `DbError` implements Display / Error and carries owned diagnostic paths/reasons.
  Load failures do not require static or leaked message/signal names.

## Validation and bit packing

Use strict serde input types matching schema v1, rejecting unknown fields and
missing required nullable fields and duplicate object keys. Validate schema
version, hash syntax, bus IDs,
positive baud rates / periods, priorities, ID ranges, names, lengths, duplicate
IDs/names, signal types, limits, choices, and transmitter/receiver references.

For every signal, precompute its actual bit positions and footprint:

- Little endian: increment the bit position.
- Big endian: decrement within a byte; after bit 0, advance to bit 7 of the next
  byte (`next = bit + 15`). The start bit is the most significant bit.
- Check every occupied bit against payload length and a per-message occupancy
  mask. Never use `start_bit + bit_length` for a Motorola signal.
- `float32` requires exactly 32 bits and no integer choices. Integer signals
  support 1..64 bits. Choice keys must be canonical decimal integers and fit
  the signal's signedness/width; support the entire u64 range.
- Scale and offset must be finite; zero scale is rejected because the database
  supports encoding. Limits must be finite and ordered (equal bounds are allowed).

Retain the old decoder only as a dev dependency for differential tests and
benchmarks. Check independent byte vectors as well as round trips so encode and
decode cannot conceal a shared packing mistake.

## Owned frames and the public API

`daqcore::can::CanFrame` is a checked classic CAN value containing `MessageId`,
up to eight payload bytes, optional transport-supplied `BusId`, and data/remote
kind. Payload length is private; constructors reject invalid lengths. SLCAN
adapters and bitrate conversions live only in daqapp's driver.

`DecodedFrame` is an owned, Copy value containing generation, bus, message ID,
message index, raw bytes/length, 64 positional signal slots, and a u64 presence
mask. `n_signals` is the definition's signal count, not the present-signal count.
Each signal slot stores its physical f64 and exact u64 raw bits (16 bytes).
Raw signed/unsigned/f32 interpretation and enum labels come from the definition.
This supports the complete classic-CAN capacity without per-frame heap allocation.
The fixed frame is approximately 1.04 KiB, not the earlier 400-byte 16-slot model;
measure its actual size and cache costs in the implementation's tests/benchmark.

- `Decoder::new(&SuperDbc, BusId)` validates the bus binding. The decoder borrows
  the database only for its use; it is not stored alongside the owner in a
  self-referential State. Decode accepts checked frames or checked payload slices.
- Unknown IDs / remote requests return no decoded frame. Oversized payload slices
  return a typed error. A short data payload returns a partial frame: preserve
  positional indices and mark each signal absent unless all its bits are present.
- Resolving a frame against metadata first checks its generation, bus, and ID.
  Borrowed message/signal views expose labels and values without allocation.
  Named signal access is a convenience for existing consumers; indexed access is
  available for resolved subscriptions. Consumers never unwrap absent values.
- `MessageDef::encode(values, policy)` takes physical values in definition order
  and returns `Result<CanFrame, EncodeError>`, carrying the definition's DLC.
  Require exactly one value per signal and reject non-finite inputs. Round integer
  raw values to nearest (ties away from zero). Explicit `Reject` / `Clamp` policy
  applies to raw representability; encoding does not silently enforce application
  limits. UI controls constrain their own physical ranges and use Clamp.
- Provide `encode_raw(&[RawValue])` for exact 64-bit integers and float bit values.
  It validates types/widths; physical f64 encoding cannot promise exact integers
  beyond the f64 precision range.
  Metadata clones used by selection controls remain editable; encoding checks
  their layout against the cached bit positions and rejects invalid edits.

## Loading, replacement, UI, and HIL

Load a selected JSON file once and send a cheap database clone plus the selected
bus to the CAN thread. The CAN thread installs the database/bus, stops scheduled
sends and active HIL expectations, and acknowledges activation with the generation
and bus. UI commits the pending database only on the matching acknowledgment;
queued frames from other generations are discarded. Widget selections, frozen
views, and history are invalidated on activation so old indices and physical data
cannot be reused under new definitions. Invalid loads leave the active DB intact.

Persist the database path and selected bus; accept the old `dbc_path` settings key
as an alias. Offer a JSON file picker and explicit bus selection. Auto-discovery
applies only without a configured JSON path: select the newest modification time,
then filename as deterministic tie-break; never sort Git hashes chronologically.
An old `.dbc` configuration migrates to discovery of a SuperDBC file. Preserve
manual selection and report a failed explicit JSON path rather than silently
switching databases.

Send controls use explicit limits when available. Without limits, derive integer
physical endpoints from signedness/width and scale/offset and sort them (including
negative scales). Float inputs use a finite editable fallback range; bool/enums
retain raw-value labels. Display enum labels as `label (raw)`. Hex/binary formatting
uses exact raw bits, including signed two's complement and floats.

HIL test ranges continue to come from the test configuration. Resolve expected
message/signal names against the selected bus when starting a run. Unknown names,
missing values, and non-finite physical values cannot pass an expectation. Changes
of database or selected bus stop the run instead of reusing resolved handles.

## Transport and log compatibility

- Driver read/write APIs use neutral CanFrame. Serial and loopback frames without
  a bus tag use the explicit selected bus; UDP frames preserve the record's tag.
  Simulated traffic uses the selected SuperDBC bus and reloads on activation.
- UDP / legacy `.log` identity uses bit 31 for bus 0/1, bit 30 for extended ID,
  and low 29 bits for arbitration ID. Decode these explicitly into BusId /
  MessageId; never infer extended status from numerical ID magnitude. Reject the
  reserved bit and invalid standard IDs. Preserve the legacy 16-byte layout.
- Legacy records have eight payload bytes and no DLC/RTR field. This migration
  cannot recover missing length/kind; retain that limitation without inventing
  a new log format. Only bus slots 0 and 1 can be recorded in the legacy format;
  do not silently relabel traffic from other buses as bus 0.
- Live remote frames remain unparsed; CAN FD remains unsupported and is dropped
  with a warning at the driver boundary. Bootloader sends/responses retain their
  standard-ID filtering and correct payload lengths.
- Log parsing loads a database once per job, binds bus 0 and 1 explicitly (defaults
  VCAN / MCAN), and resolves headers from metadata. CSV columns use positional
  handles rather than per-frame string tuple construction. Keep the existing
  per-bus database overrides as an explicit compatibility option.
- daqcli accepts a SuperDBC path and log input/output arguments and exposes this
  functionality without depending on daqapp.

## Implementation and acceptance

1. Implement strict loading, checked CAN types, decoder, encoder, and metadata
   views in daqcore. Add fixture-based differential tests for all messages,
   independent Motorola vectors, float/scaled/signed values, and full-width raw
   round trips. Measure allocations and throughput against the old decoder.
2. Migrate the CAN thread, activation messages, settings, driver boundary, raw
   logging, bootloader frame conversion, and simulation. Test bus/EID routing,
   including extended IDs 1/2/3 and equal IDs on different buses.
3. Migrate every widget and formatter, including GPS, dynamics, and battery views;
   clear generation-bound state on activation. Test short/unknown frames and HIL
   missing/NaN handling. Keep each integration checkpoint compiling.
4. Migrate log_parse and the log-parser UI; add CLI smoke coverage and golden CSV
   checks. Verify existing log layout remains unchanged.
5. Remove production can_decode/can-dbc dependencies and obsolete conversion
   helpers. Retain can_decode only for differential tests/benchmarks. Verify
   SLCAN references are confined to the driver (and dependency manifests).

Run cargo fmt, workspace tests, all-target compilation, and representative decode
benchmarks. No unrelated firmware/submodule changes. Benchmark claims are measured
results, not promised speedups. Engine/RamCache documents referenced by the original
proposal are absent in this checkout; this implementation does not invent those
subsystems or treat their existence as a dependency.

## Completed validation and measured results

The implementation is in `daq/daqcore/src/superdbc/`; the GUI, drivers, HIL,
log parser, and CLI now consume it. Production dependency-tree inspection confirms
that can_decode / can-dbc are absent; can_decode remains a core dev dependency.
SLCAN references in production source are confined to the driver.

Software checks completed on 2026-10-03:

- `cargo fmt --all --manifest-path daq/Cargo.toml -- --check`
- `cargo test --workspace --locked --offline --manifest-path daq/Cargo.toml`:
  **31 passed**, including existing bootloader coverage.
- `cargo check --workspace --all-targets --locked --offline --manifest-path daq/Cargo.toml`
- `git diff --check`

The Linux GUI checks used the locally installed Nix libudev/libxkbcommon
development packages through PKG_CONFIG_PATH. Existing dead-code warnings remain.

Frozen fixtures cover all 145 messages against can_decode, with 16 payloads per
message and exact raw re-encoding. Additional coverage includes independent
Motorola vectors, 64-bit signed/unsigned extrema, 64 boolean slots, partial
payloads, enum keys, strict schema errors, database generations, selected bus IDs,
settings migration, stale sends, standard/extended ID collisions, activation
cancellation, HIL missing/NaN values, scope history reset, SLCAN DLC/remote kind,
legacy UDP/log flags, and CLI/CSV output. The CSV check binds transport slot 1 to
CCAN (database bus ID 2), including its extended arbitration ID 1.

The allocation-counting regression test records **zero allocation/reallocation
events** across repeated decoding, borrowed metadata/enum access, and encoding.
DecodedFrame measures **1,064 bytes** on x86_64. A 300-frame stream therefore uses
319,200 bytes for frame storage alone, excluding timestamps/container capacity.

Final release benchmark (`cargo bench --locked --offline --manifest-path daq/Cargo.toml -p daqcore --bench decode`),
100,000 iterations per sample on Intel Core Ultra 7 256V, rustc 1.95.0:

| Message | Signals | SuperDBC ns/frame | can_decode ns/frame | Ratio |
| --- | ---: | ---: | ---: | ---: |
| abox_init | 1 | 138 | 267 | 1.94x |
| vcu_torque_request | 4 | 184 | 1,004 | 5.45x |
| main_module_fault_sync | 16 | 104 | 3,182 | 30.62x |

These are machine-specific microbenchmarks without confidence intervals, not
end-to-end GUI or hardware throughput claims. Physical CAN hardware and an
interactive GUI smoke test were not exercised in this environment. The legacy
log format still lacks DLC/RTR and remains limited to two bus slots.

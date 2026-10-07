# superdbc — Public API Design

Design specification for a new `daqcore::superdbc` module replacing `can_decode` and `can-dbc`. This document defines the API and behavior to implement next; it does not implement the module or migrate consumers.

The application selects one active SuperDBC document. That document contains all buses, so every message lookup and codec operation has an explicit bus. Definitions are compiled once at load, and decoded results own their data so cached frames can survive database replacement or drop.

## 1. Design decisions

- Reuse [`CanIdentity`](daq/daqcore/src/frame.rs) for validated standard/extended CAN identity. Superdbc never accepts a flag-packed `u32` message ID.
- Key messages by `(bus_id, CanIdentity)`. Numeric bus IDs are the runtime interface; bus names resolve through metadata lookup.
- Use ordinary owned strings, vectors, and `IndexMap`. No `Arc`, `Cow`, leaked strings, lifetime-bearing decoded frames, or custom signal hash table is required by the module.
- Provide physical-value encoding and exact raw-value encoding. Require every defined signal and report input errors instead of silently supplying defaults or clamping.
- Give loading, parsing, decoding, and encoding separate error types. Decode errors cannot represent JSON or I/O failures. Callers that only need successful decoded frames can use `.ok()`.
- Do not check hash contents, syntax, or digests. Preserve hash strings as supplied metadata; add no `sha2` dependency.
- Support SuperDBC schema version `1.1` and classic CAN payloads of zero through eight bytes. CAN FD decoding is outside this version.

### Why the original proposal changes

The global ID index was ambiguous across buses. In the current artifact, standard ID `0x111` names `INVA_SET` on MCAN and `pump_and_fan_current` on VCAN. The bus is part of the lookup key even when only one database is active.

A decoded `&'static str` cannot borrow a name from an owned database. Holding a database elsewhere does not prove such a lifetime, and leaking names would accumulate memory across reloads. Owned decoded strings avoid both problems.

The proposed display-format enum omitted decimal formats that occur in the artifact. Its `i64` raw representation also excluded unsigned 64-bit values, and its Float32 path incorrectly omitted scaling. The fixed 16-slot signal table had no capacity margin for a current 16-signal message.

The performance claims were estimates rather than measurements. The current `dbc/superdbc_b708877.json` contains five buses, 145 messages, and 510 signals, with at most 16 signals per message. These are fixture facts, not API capacity limits.

## 2. Module and ownership layout

```text
daqcore/src/
  lib.rs              pub mod superdbc;
  superdbc/
    mod.rs            Public type re-exports; implementation modules private
    model.rs          Private serde models for the SuperDBC 1.1 document
    error.rs          LoadError, ParseError, DecodeError, EncodeError
    database.rs       Database, Bus, Node; loading, validation, lookup indexes
    message.rs        Message, MessageKey, SignalDefinition, metadata types
    extract.rs        Private compiled extraction and insertion operations
    decode.rs         Owned DecodedMessage and DecodedSignalValue
    encode.rs         Physical and raw encoding
```

Keep the database and operation-specific error types under `daqcore::superdbc`; no crate-root re-exports are necessary. Public re-exports include `Database`, `Bus`, `Node`, `Message`, `MessageKey`, `SignalDefinition`, `RawType`, `RawValue`, `ByteOrder`, `SignalLimits`, `DisplayFormat`, `DecodedMessage`, `DecodedSignalValue`, `LoadError`, `ParseError`, `DecodeError`, and `EncodeError`. The private parsing models remain handwritten for now; schema-driven type generation is deferred.

`Database` owns a `Vec<Bus>`, and each `Bus` owns a `Vec<Message>`. Each message owns its signal definitions. Preserve document iteration order using `IndexMap` for JSON bus names. Private indexes map bus names and IDs to bus positions, `MessageKey` to `(bus_index, message_index)`, and signal names to definition positions. Never duplicate message definitions to produce a flattened slice.

Metadata access borrows from the database. Decoded results are independent owned values and derive `Debug` and `Clone`, as required by `ParsedFrame` and its cache. The database is immutable after construction and can be `Send + Sync` without locks or shared-ownership wrappers. How application threads obtain access to the active document is a later integration concern, not part of the codec's public ownership contract.

## 3. Public API

The signatures below specify the intended interface; method bodies and private indexes are omitted.

### Identity and values

```rust
use crate::frame::CanIdentity;
use std::path::{Path, PathBuf};

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct MessageKey {
    pub bus_id: u8,
    pub identity: CanIdentity,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RawType {
    Unsigned,
    Signed,
    Float32,
}

#[derive(Clone, Copy, Debug)]
pub enum RawValue {
    Integer(i128), // exact for both signed and unsigned 64-bit signals
    Float32(f32),  // unscaled IEEE-754 value; to_bits() exposes its wire bits
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ByteOrder {
    LittleEndian,
    BigEndian,
}

#[derive(Clone, Copy, Debug)]
pub struct SignalLimits {
    pub min: f64,
    pub max: f64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DisplayFormat {
    Hex,
    Binary,
    Integer,
    Decimal(u8),
}
```

`CanIdentity::new(raw_id, is_extended)` validates IDs at input boundaries. Standard and extended frames with the same numeric ID remain different identities, including extended IDs below `0x800`. Use `identity.raw_id()` for display and `identity.is_extended()` for transport conversion. Superdbc has no `dbc_id()`, flag-setting helper, or parallel stored `is_extended` field.

`MessageKey` identifies a definition within the active database; it is not a stable identifier across different database versions. UI state resolves keys again after database replacement.

### Database and bus

```rust
pub struct Database {
    path: Option<PathBuf>,
    content_hash: String,
    version_hash: String,
    schema_version: String,
    buses: Vec<Bus>,
    // Private lookup indexes.
}

impl Database {
    pub fn load(path: &Path) -> Result<Self, LoadError>;
    pub fn from_json(json: &str) -> Result<Self, ParseError>;
    pub fn path(&self) -> Option<&Path>;
    pub fn content_hash(&self) -> &str;
    pub fn version_hash(&self) -> &str;
    pub fn schema_version(&self) -> &str;

    pub fn buses(&self) -> &[Bus];
    pub fn bus(&self, name: &str) -> Option<&Bus>;
    pub fn bus_by_id(&self, bus_id: u8) -> Option<&Bus>;
    pub fn messages(&self) -> impl Iterator<Item = &Message>;
    pub fn message(&self, bus_id: u8, identity: CanIdentity) -> Option<&Message>;

    pub fn decode(
        &self, bus_id: u8, identity: CanIdentity, data: &[u8],
    ) -> Result<DecodedMessage, DecodeError>;
    pub fn encode(
        &self, bus_id: u8, identity: CanIdentity, values: &[(&str, f64)],
    ) -> Result<Vec<u8>, EncodeError>;
    pub fn encode_raw(
        &self, bus_id: u8, identity: CanIdentity, values: &[(&str, RawValue)],
    ) -> Result<Vec<u8>, EncodeError>;
}

pub struct Bus {
    name: String,
    bus_id: u8,
    baud_rate: u64,
    nodes: Vec<Node>,
    messages: Vec<Message>,
    // Private message index keyed by CanIdentity.
}

#[derive(Debug, Clone)]
pub struct Node {
    pub name: String,
    pub is_external: bool,
}

impl Bus {
    pub fn name(&self) -> &str;
    pub fn bus_id(&self) -> u8;
    pub fn baud_rate(&self) -> u64;
    pub fn nodes(&self) -> &[Node];
    pub fn messages(&self) -> &[Message];
    pub fn message(&self, identity: CanIdentity) -> Option<&Message>;
    pub fn decode(&self, identity: CanIdentity, data: &[u8]) -> Result<DecodedMessage, DecodeError>;
    pub fn encode(&self, identity: CanIdentity, values: &[(&str, f64)]) -> Result<Vec<u8>, EncodeError>;
    pub fn encode_raw(&self, identity: CanIdentity, values: &[(&str, RawValue)]) -> Result<Vec<u8>, EncodeError>;
}
```

`load` reads a file and delegates to the same parsing and compilation used by `from_json`, then records its path. A database constructed from JSON has no path. Database-wide message iteration flattens bus order followed by message order; bus-scoped message access remains a slice for pickers and simulated traffic.

`content_hash` is the supplied definitions digest. `version_hash` comes from `versions.hash` and is the value used in the artifact filename, such as `b708877`. They are not interchangeable and neither is verified.

### Definitions

```rust
pub struct Message { /* owned metadata, signals, and compiled indexes */ }

impl Message {
    pub fn key(&self) -> MessageKey;
    pub fn identity(&self) -> CanIdentity;
    pub fn name(&self) -> &str;
    pub fn length_bytes(&self) -> u8;
    pub fn transmitter(&self) -> &str;
    pub fn receivers(&self) -> &[String];
    pub fn nominal_period_ms(&self) -> Option<u32>;
    pub fn priority(&self) -> u8;
    pub fn description(&self) -> &str;
    pub fn signals(&self) -> &[SignalDefinition];
    pub fn signal(&self, name: &str) -> Option<&SignalDefinition>;
    pub fn decode(&self, data: &[u8]) -> Result<DecodedMessage, DecodeError>;
    pub fn encode(&self, values: &[(&str, f64)]) -> Result<Vec<u8>, EncodeError>;
    pub fn encode_raw(&self, values: &[(&str, RawValue)]) -> Result<Vec<u8>, EncodeError>;
}

pub struct SignalDefinition { /* owned metadata and private compiled codec */ }

impl SignalDefinition {
    pub fn name(&self) -> &str;
    pub fn description(&self) -> &str;
    pub fn data_type(&self) -> &str;
    pub fn raw_type(&self) -> RawType;
    pub fn start_bit(&self) -> u8;
    pub fn bit_length(&self) -> u8;
    pub fn byte_order(&self) -> ByteOrder;
    pub fn scale(&self) -> f64;
    pub fn offset(&self) -> f64;
    pub fn limits(&self) -> Option<SignalLimits>;
    pub fn unit(&self) -> &str;
    pub fn display_format(&self) -> Option<DisplayFormat>;
    pub fn choices(&self) -> impl Iterator<Item = (i128, &str)>;
    pub fn choice_label(&self, raw: i128) -> Option<&str>;
}
```

Store integer choices in an ordinary `BTreeMap<i128, String>` for sorted iteration and label lookup. Missing or empty choice maps produce an empty iterator. Preserve raw integer keys rather than interpreting them as physical values. Metadata strings stay literal; presentation placeholders belong in UIs.

### Owned decoded results

```rust
#[derive(Debug, Clone)]
pub struct DecodedMessage {
    key: MessageKey,
    name: String,
    tx_node: String,
    signals: indexmap::IndexMap<String, DecodedSignalValue>,
}

impl DecodedMessage {
    pub fn key(&self) -> MessageKey;
    pub fn name(&self) -> &str;
    pub fn tx_node(&self) -> &str;
    pub fn signal(&self, name: &str) -> Option<&DecodedSignalValue>;
    pub fn iter(&self) -> impl Iterator<Item = (&str, &DecodedSignalValue)>;
    pub fn len(&self) -> usize;
    pub fn is_empty(&self) -> bool;
}

#[derive(Debug, Clone)]
pub struct DecodedSignalValue {
    physical: f64,
    raw: RawValue,
    enum_label: Option<String>,
}

impl DecodedSignalValue {
    pub fn physical(&self) -> f64;
    pub fn raw(&self) -> RawValue;
    pub fn enum_label(&self) -> Option<&str>;
    pub fn int_rounded(&self) -> i128;
}
```

Insert decoded signals in definition order and reserve the map capacity before decoding. Signal names occur once as map keys; values do not duplicate names or units. Consumers obtain units and other formatting metadata from definitions.

For integer-backed signals, `int_rounded()` returns the exact raw integer. For Float32 signals, it returns `physical.round() as i128`, preserving the existing saturating conversion behavior, including NaN becoming zero. This method does not mean rounded physical value for integer-backed signals; callers wanting that use `physical().round()` explicitly.

### Operation-specific errors

Use standard `Result<T, E>` with a distinct error enum for each operation family. Do not introduce a catch-all `superdbc::Error`, boxed errors, or a custom result wrapper. An application that combines operations can define its own enclosing error type or convert errors to text at its UI/command boundary.

```rust
#[derive(Debug)]
pub enum LoadError {
    Io(std::io::Error),
    Parse(ParseError),
}

#[derive(Debug)]
pub enum ParseError {
    InvalidJson(serde_json::Error),
    UnsupportedSchemaVersion { found: String },
    InvalidDefinition { context: String, reason: String },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DecodeError {
    UnknownBus { bus_id: u8 },
    UnknownMessage { key: MessageKey },
    InvalidPayloadLength {
        key: MessageKey,
        expected_min: u8,
        actual: usize,
    },
}

#[derive(Debug)]
pub enum EncodeError {
    UnknownBus { bus_id: u8 },
    UnknownMessage { key: MessageKey },
    MissingSignal { key: MessageKey, signal: String },
    UnknownSignal { key: MessageKey, signal: String },
    DuplicateSignal { key: MessageKey, signal: String },
    TypeMismatch { key: MessageKey, signal: String, expected: RawType },
    NonFiniteValue { key: MessageKey, signal: String },
    Float32Overflow { key: MessageKey, signal: String },
    PhysicalOutOfRange { key: MessageKey, signal: String, physical: f64 },
    RawOutOfRange {
        key: MessageKey,
        signal: String,
        value: i128,
        min: i128,
        max: i128,
    },
}
```

The return types establish the failure boundary:

| Operation | Error type | Possible failures |
|---|---|---|
| `Database::load` | `LoadError` | File reading or a wrapped `ParseError` |
| `Database::from_json` | `ParseError` | JSON/model deserialization, unsupported version, or invalid definitions |
| Database/bus/message `decode` | `DecodeError` | Lookup or payload length; never JSON, I/O, or invalid-definition failures |
| Database/bus/message `encode` / `encode_raw` | `EncodeError` | Lookup, signal input, or value representability; never JSON or I/O failures |
| Metadata lookup such as `bus`, `message`, or `signal` | `Option<&T>` | Absence, with no diagnostic error required |

Error types are shared within an operation family rather than multiplying types for every scope. A resolved `Bus` cannot produce `UnknownBus`; a resolved `Message` cannot produce either lookup error. `Message::decode` therefore only produces `InvalidPayloadLength`. Bad signal layouts and invalid identities are rejected during construction; they are not runtime decode error variants.

All `DecodeError` fields are copyable and allocation-free. Decoding arbitrary bytes never returns an encoding range error: unknown enum values, out-of-limit physical values, and Float32 NaN/infinity remain valid decoded telemetry. Unknown buses/messages still return typed errors rather than being conflated with successful empty messages.

`EncodeError` uses structured variants so the send UI can distinguish incomplete input, wrong raw types, non-finite values, Float32 overflow, and range failures without parsing error text. `PhysicalOutOfRange` carries the requested physical value when inverse scaling or integer quantization exceeds the wire range; exact raw integer range failures carry `i128` bounds, which cover both signed and unsigned 64-bit values. `NonFiniteValue` covers non-finite input or inverse-scaled values; `Float32Overflow` covers a finite inverse-scaled value that becomes infinite on narrowing.

Implement manual `Display` and `std::error::Error` for each enum, following daqcore's existing style. `LoadError::source()` returns its I/O or parse error, and `ParseError::source()` returns the wrapped JSON error when applicable; other variants have no source. Provide `From<std::io::Error>` and `From<ParseError>` for `LoadError`, and `From<serde_json::Error>` for `ParseError`. Do not convert load/parse errors into decode/encode errors.

## 4. Loading, decoding, and encoding behavior

### Loading and validation

Read JSON, inspect `versions.schema_version`, reject unsupported versions, then deserialize private version-1.1 models and validate semantic constraints before compiling indexes. JSON syntax and model-deserialization failures return `ParseError::InvalidJson`; semantic failures return `ParseError::InvalidDefinition`. File reads are the only source of `LoadError::Io`. Reject unknown fields in those models. Follow the current generator's required/nullable fields and its optional `display_format` field.

Validate bus IDs in `0..=7`, positive baud rates, unique bus IDs and names, valid CAN ID widths, message lengths in `0..=8`, priority in `0..=5`, and positive nominal periods when present. Within each bus, reject duplicate message names or duplicate complete CAN identities. Identical identities on different buses are valid, as are standard and extended identities with the same numeric ID.

Validate unique signal names, nonempty schema-required names, bit lengths in `1..=64`, and every signal's actual occupied bits against the declared payload. Reject overlapping signals using occupied wire bits rather than assuming Motorola signals form a monotonically numbered bit range. Float32 requires 32 bits and no choices. Choice keys must match the schema's integer-key syntax and fit the signal's signed or unsigned range.

Require finite scale and offset, nonzero scale, and finite ordered limits. Negative scale is valid. Require supported raw types, byte orders, and display formats. Parse `hex`, `binary`, `integer`, and `0f` through `7f` strictly; decimal precision outside that range is invalid.

Require hash fields to be strings because they are metadata fields, but do not validate their format, length, contents, or agreement with the document. Do not recompute or canonicalize hashes.

### Extraction and decoding

Compile masks, shifts, signed ranges, and byte extraction/insertion operations at load. Use a stack `[u8; 8]` or `u64` payload window. Handle the 64-bit case without shifting by 64.

Little-endian start bits identify the least significant signal bit. Motorola start bits identify the most significant bit with sawtooth traversal: the next bit is `bit - 1`, except bit zero of a byte advances to bit seven of the next byte (`bit + 15`). Compile all byte segments needed for extraction and insertion. Respect each signal's byte order, including little-endian sub-byte fields inside otherwise big-endian messages.

Accept `message.length_bytes() <= data.len() <= 8`. Only the declared payload bytes participate in decoding; ignore trailing padding. This supports current fixed-eight-byte log records. A zero-length message must have no signals and can decode an empty or padded payload. Remote and FD frame kinds are filtered by callers before invoking this classic-data codec.

Preserve integer raw values exactly in `i128`. Decode Float32 from its extracted bits before converting it to `f64`. For both kinds, compute:

```text
physical = numeric_raw * scale + offset
```

Resolve enum labels using the raw integer, leaving unmatched values numeric. Decoding preserves out-of-limit values and IEEE NaN/infinity instead of rejecting telemetry; declared physical limits are metadata, not decode filters.

### Encoding

Both encoders require each defined signal exactly once. Input order is irrelevant. Reject unknown names, duplicates, missing names, and invalid values. Initialize unused payload bits to zero and return exactly the declared number of bytes. A message with no signals accepts an empty input list.

Physical encoding accepts `f64` values and applies:

```text
numeric_raw = (physical - offset) / scale
```

For integer signals, round to nearest with ties away from zero. Check representability before casting; saturating Rust casts must not turn an out-of-range input into an accepted value. In particular, the upper exclusive bounds `2^64` and `2^63` must be rejected for unsigned and signed 64-bit encoding respectively. For Float32, narrow the inverse-scaled value to `f32` and reject overflow to infinity. Reject non-finite input or inverse-scaled values. Do not clamp.

Raw encoding bypasses scaling and accepts `RawValue::Integer(i128)` for integer signals and `RawValue::Float32(f32)` for Float32 signals. Validate integer bit-range bounds and reject mismatched variants or non-finite Float32 inputs. Finite Float32 bit patterns, including negative zero, are preserved. The physical encoder is convenient but cannot express every 64-bit integer exactly; the raw encoder provides that exactness, including in messages mixing integer and Float32 signals.

Declared physical limits guide UI ranges; the codec enforces wire representability rather than those limits.

## 5. Usage examples

Examples are intended call patterns once the module exists; they are not patches to current consumers.

### Load and resolve a bus

```rust
use daqcore::superdbc::{Database, LoadError};
use std::path::Path;

fn inspect(path: &Path) -> Result<(), LoadError> {
    let db = Database::load(path)?;
    if let Some(bus) = db.bus("MCAN") {
        println!("{}: id {}, {} baud", bus.name(), bus.bus_id(), bus.baud_rate());
        for message in bus.messages() {
            println!("{}: {}", message.name(), message.identity());
        }
    }
    Ok(())
}
```

### Decode using the incoming bus ID

```rust
let decoded = db.decode(bus_id, frame.identity, &frame.data)?;
if let Some(value) = decoded.signal("Z_axis") {
    let physical = value.physical();
    let label = value.enum_label();
    // Use physical and label in the caller's projection.
}
```

A `FrameDecoder` can keep its existing optional decoded field by invoking the decoder only for `FrameKind::Data` and storing `db.decode(bus_id, frame.identity, &frame.data).ok()`. Diagnostics may match `DecodeError::UnknownBus`, `DecodeError::UnknownMessage`, and `DecodeError::InvalidPayloadLength` before discarding it; no JSON/I/O match arms are necessary. Unknown traffic need not produce a log entry for every frame.

### Encode physical or exact raw values

```rust
use daqcore::superdbc::RawValue;

// `message` is resolved from the active database using bus ID and identity.
let payload = message.encode(&[("speed", 2500.0), ("enabled", 1.0)])?;
let key = message.key();
// The sender receives key.bus_id, key.identity, and payload together.

// This example assumes a message defining a 64-bit unsigned `counter` signal.
let exact_payload = counter_message.encode_raw(&[
    ("counter", RawValue::Integer(u64::MAX as i128)),
])?;
```

For existing UI-owned names, build borrowed pairs with `signal.name.as_str()`; neither encoder requires a `HashMap` or `Cow` input. A resolved message already has the complete identity, so sending never reconstructs it from raw ID and format fields.

### Pick messages and resolve metadata

```rust
let candidates: Vec<_> = db.messages()
    .filter(|message| message.name().contains(search_text))
    .map(|message| message.key())
    .collect();

if let Some(key) = selected_key {
    if let Some(message) = db.message(key.bus_id, key.identity) {
        for definition in message.signals() {
            // Read description, unit, limits, choices, and display format.
        }
    }
}
```

Keep display formatting in the existing formatter. Explicit user formatter rules take precedence, then a superdbc display hint, then the existing default (enum label when available, otherwise numeric formatting). Integer and decimal hints format physical values; integer hex/binary formats use raw values. Preserve the existing float hex/binary numeric fallback through `int_rounded()`. Units remain available from signal definitions.

## 6. Later consumer migration

The following work is guidance for integration after the module exists. It is outside this documentation-only change.

| Consumer | Intended migration |
|---|---|
| App database selection | Select one active SuperDBC document. Preserve current database/state on failed replacement; clear metadata selections and prepared send state after successful replacement. |
| CAN thread / frame decoding | Replace parser access with explicit bus ID plus `CanIdentity`; retain `Option<DecodedMessage>` in `ParsedFrame` using typed decode results. |
| Frame transport, cache, and send scheduling | Preserve bus alongside identity. Use the complete bus/identity key wherever equal IDs on different buses could otherwise collide. |
| Message picker, send UI, jitter, scope | Store `MessageKey` rather than cloned definitions or bare IDs; resolve metadata from the active database. Send all signal values and surface encoding errors. |
| Viewer and formatter | Use owned decoded name/transmitter and ordered signal iteration; obtain units, raw type, bit width, and display hints from definitions. |
| HIL, plots, battery views, signal series | Replace `.signals.get(name)?.value` with `.signal(name)?`; retain physical-value and enum behavior. |
| Simulated driver | Choose messages from the intended bus's slice and construct traffic from message identity, bus ID, and declared length. |
| Log parsing / table export | Decode each record with its bus ID using the same active document; obtain headers from bus-scoped definitions and support padded eight-byte payloads. |
| daqcli | Currently a stub; future commands use the same database and codec interfaces. |

Current `CanFrame` and `ParsedFrame` do not retain bus ID even though UDP/log record identities contain it. Later integration must preserve that information through decoding, cached frames, selections, and transmission. Sources that lack a bus identifier need an explicit connection-to-bus mapping. Do not infer the bus from CAN ID bits.

Transport flag layouts are boundary-specific: existing log records place their extended flag differently from the DBC representation. Unpack transport flags into a bus ID and validated `CanIdentity`; never pass a packed transport identity to superdbc or treat all layouts as equivalent.

The existing log parser assumes two recorded buses, while SuperDBC bus IDs occupy `0..=7`. Preserve the current record interpretation until its transport format changes. Dropping the dual-file log-parser controls is a separate consumer migration; the codec does not retain a two-database API.

Cached decoded results remain valid after reload because they own their values. Clear existing session/cache state when a successful application replacement would otherwise pair old decoded values with new formatting metadata. Do not store borrowed definitions in persistent UI state.

Once consumers are migrated, remove the four DBC conversion helpers and `can_decode`/`can-dbc` manifest entries. Retain transport-specific masks only where transports still need them. No new hashing dependency is needed.

## 7. Acceptance tests and performance criteria

### Functional acceptance

- Check operation-specific return types: file loading wraps parse failures, in-memory parsing exposes no I/O variant, and decode/encode expose no JSON or load variants. Verify error source chains and typed distinctions between unknown bus, unknown message, invalid payload length, and signal/value encoding failures.

- Load the current artifact and enumerate all five buses, 145 messages, and 510 signals. Accept every current decimal format, null display hints, and an omitted optional display hint.
- Reject unsupported schema versions, unknown fields, duplicate bus IDs, duplicate identities within a bus, duplicate message/signal names, invalid widths, bad limits, zero scale, overlapping bits, and signals outside the declared payload.
- Accept repeated CAN identities across buses and distinguish standard/extended identities with the same numeric ID. Decode the current MCAN/VCAN `0x111` collision using the requested bus.
- Verify known bytes for unaligned little-endian fields, Motorola fields spanning multiple bytes, mixed-order messages, signed extrema, 64-bit widths, positive and negative scaling, offsets, Float32 scaling, and enum lookup/fallback. Use known vectors in addition to encode/decode round trips so shared codec mistakes cannot hide.
- Encode `u64::MAX` and `i64::MIN` exactly through raw inputs, including a mixed integer/Float32 message. Reject physical values at the exclusive 64-bit upper bounds before casts, wrong raw variants, and non-finite encode inputs.
- Reject missing, unknown, and duplicate input signals without clamping. Verify rounding ties, finite Float32 narrowing, raw negative-zero preservation, and zeroed unused bits.
- Accept declared-length and padded-eight-byte payloads, ignore padding, reject short or over-eight-byte payloads, and support zero-length messages. Filter remote/FD kinds at the caller seam.
- Preserve out-of-limit telemetry and non-finite Float32 decode values. Check that integer `int_rounded()` returns raw rather than scaled physical values.
- Keep decoded values readable and cloneable after database drop or reload. Verify lookup and iteration order without a fixed signal-count cap.
- Preserve supplied content/version hash strings, including arbitrary string contents, without integrity or hash-format checks.
- Verify formatter precedence, units, enum defaults, decimal hints, and raw integer hex/binary formatting during consumer integration.

### Performance evaluation

Compare the implementation with the locally used `can_decode` version in release mode using the same message definitions, bytes, and frame mix. Measure load time, decode/encode throughput, allocation counts, and decoded-frame cloning as exercised by the RAM cache. Include low-signal and high-signal messages, enums, Float32, and multi-byte Motorola extraction.

Database dispatch and name lookup use average O(1) hash-table access, excluding the cost of hashing a name. Full message encoding/decoding remains O(number of signals); choice lookup is O(log number of choices). Decoding allocates the owned names, labels, and ordered map, and cloning copies that owned data.

Precomputation removes repeated codec setup, and decoded values avoid duplicate name/unit strings. These are implementation properties, not a measured speed guarantee. Report benchmark results before claiming faster loading, lower memory use, or improved frame throughput. Any later ownership or storage optimization requires evidence and must preserve this public contract.
